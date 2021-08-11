/******************************************************************************
 * Name         : prepro.c
 * Author       : James McCarthy
 * Created      : 25/11/2004
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
 * Platform     : ANSI C
 *
 * Modifications:-
 * $Log: prepro.c $
 *****************************************************************************/

#include "glsl.h"
#include "prepro.h"
#include "error.h"
#include "string.h"
#include <stdio.h>

#include "../parser/debug.h"
#include "../parser/metrics.h"

#if defined(GLSL_ES)

IMG_INTERNAL const IMG_UINT32 guSupportedGLSLVersions[GLSL_NUM_VERSIONS_SUPPORTED] = {100};

#else

IMG_INTERNAL const IMG_UINT32 guSupportedGLSLVersions[GLSL_NUM_VERSIONS_SUPPORTED] = {100,110,120};

#endif


#define MAX_NESTED_IF_LEVELS 255

typedef struct GLSLPreProcessorContextTAG
{
	GLSLExtension eEnabledExtensions;
	IMG_UINT32    uSupportedVersion;  
}GLSLPreProcessorContext;

typedef struct ExpressionNodeTAG
{
	Token                    *psToken;
	IMG_UINT32                uTokenPrecedence;
	struct ExpressionNodeTAG *psParentNode;
	struct ExpressionNodeTAG *psChildNode[2];
	IMG_UINT32                uNumChildren;
} ExpressionNode;

typedef struct TokenLLTAG
{
	Token                   sToken;
	struct TokenLLTAG      *psNext;
	struct TokenLLTAG      *psPrev;
	IMG_BOOL                bRemove;
} TokenLL;

typedef enum PPSymbolTypeTAG
{
	PPST_DEFINEMACRO,
} PPSymbolType;

typedef struct DefineMacroTAG
{
	PPSymbolType     ePPSymbolType;
	IMG_BOOL         bTakesArgs;
	IMG_UINT32       uNumArgs;
	IMG_UINT32       uNumArgUsages;
	IMG_UINT32      *puArgumentToUse;
	IMG_UINT32      *puArgumentPositions;
	Token           *psTokenList;
	IMG_UINT32       uNumTokens;
} DefineMacro;



typedef enum IfBlockStateTAG
{
	IFSTATE_BLOCK_IF_ENABLED,
	IFSTATE_BLOCK_ELSE_ENABLED,
	IFSTATE_BLOCK_ELIF_ENABLED,
	IFSTATE_BLOCK_IF_DISABLED,
	IFSTATE_BLOCK_ELSE_DISABLED,
	IFSTATE_BLOCK_ELIF_DISABLED,
	IFSTATE_BLOCK_IGNORE_ALL_FURTHER_IFS,
} IfBlockState;

#define BLOCK_ENABLED(a) ((a == IFSTATE_BLOCK_IF_ENABLED) || (a == IFSTATE_BLOCK_ELSE_ENABLED) || (a == IFSTATE_BLOCK_ELIF_ENABLED))

typedef struct IfStatusTAG
{
	IMG_UINT32   uCurrentActiveLevel;
	IMG_UINT32   uNestedIfLevel;
	IfBlockState aeIfBlockStates[MAX_NESTED_IF_LEVELS];
} IfStatus;

#define POSSIBLE_DEFINE_NAME(a) \
(((a)->eTokenName == TOK_IDENTIFIER) || ((a)->pvData && ((((IMG_CHAR *)((a)->pvData))[0] >= 'a' &&  ((IMG_CHAR *)((a)->pvData))[0] <= 'z') || (((IMG_CHAR *)((a)->pvData))[0] >= 'A' &&  ((IMG_CHAR *)((a)->pvData))[0] <= 'Z'))))

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static IMG_VOID PPFreeDefineMacro(IMG_VOID *pvDefineMacro)
{
	DefineMacro *psDefineMacro = (DefineMacro *)pvDefineMacro;

	if (psDefineMacro->psTokenList)
	{
		IMG_UINT32 i;

		for (i = 0; i <  psDefineMacro->uNumTokens; i++)
		{
			if (psDefineMacro->psTokenList[i].pvData)
			{
				DebugMemFree(psDefineMacro->psTokenList[i].pvData);
			}
		}

		DebugMemFree(psDefineMacro->psTokenList);
	}

	if (psDefineMacro->puArgumentToUse)
	{
		DebugMemFree(psDefineMacro->puArgumentToUse);
	}

	if (psDefineMacro->puArgumentPositions)
	{
		DebugMemFree(psDefineMacro->puArgumentPositions);
	}


	DebugMemFree(psDefineMacro);
}

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static TokenLL *PPGetNextDirectiveTokenEntry(TokenLL *psTokenEntry,
											TokenLL *psLastEntry)
{
	Token *psToken;

	do
	{
		/* Check if we've run out of tokens */
		if (!psTokenEntry || (psTokenEntry == psLastEntry) || !psTokenEntry->psNext)
		{
			return IMG_NULL;
		}

		psTokenEntry = psTokenEntry->psNext;

		psToken = &psTokenEntry->sToken;

		/* Ignore certain tokens */
	} while ((psToken->eTokenName == TOK_BACK_SLASH)     ||
			 (psToken->eTokenName == TOK_NEWLINE)        ||
			 (psToken->eTokenName == TOK_CARRIGE_RETURN) ||
			 (psToken->eTokenName == TOK_TAB)            ||
			 (psToken->eTokenName == TOK_VTAB)           ||
			 (psToken->eTokenName == TOK_COMMENT)        ||
			 (psToken->eTokenName == TOK_INCOMMENT)      ||
			 (psToken->eTokenName == TOK_ENDOFSTRING)    ||
			 (psToken->eTokenName == TOK_TERMINATEPARSING));

	return psTokenEntry;
}

/******************************************************************************
 * Function Name: PPCreateTokenList
 *
 * Inputs       : psTokenLL     - First token to copy
 *                psLastEntry   - Last token to copy
 *
 * Outputs      : puNumTokens   - Number of tokens copied
 * Returns      : An array with the tokens that are in a linked list between psTokenLL and psLastEntry
 * Globals Used : -
 *
 * Description  : Copies a linked list of tokens and returns an array with the same contents.
 *                Note: it skips all tokens marked with bRemove.
 *****************************************************************************/
static Token *PPCreateTokenList(TokenLL *psTokenLL, TokenLL *psLastEntry, IMG_UINT32 *puNumTokens)
{
	IMG_UINT32 uNumTokens = 0;

	TokenLL *psCurrentEntry = psTokenLL;

	Token *psTokenList;

	/* Count number of tokens */
	while (psCurrentEntry && (psCurrentEntry != psLastEntry))
	{
		if (!psCurrentEntry->bRemove)
		{
			uNumTokens++;
		}

		psCurrentEntry = psCurrentEntry->psNext;
	}

	/* Alloc memory for token list */
	psTokenList = DebugMemAlloc(sizeof(Token) * uNumTokens);

	if(uNumTokens && !psTokenList)
	{
		return IMG_NULL;
	}

	uNumTokens = 0;

	psCurrentEntry = psTokenLL;

	/* Copy the tokens to the list */
	while (psCurrentEntry && (psCurrentEntry != psLastEntry))
	{
		if (!psCurrentEntry->bRemove)
		{
			psTokenList[uNumTokens] = psCurrentEntry->sToken;

			/* Copy the data */
			if (psCurrentEntry->sToken.uSizeOfDataInBytes)
			{
				psTokenList[uNumTokens].pvData = DebugMemAlloc(psCurrentEntry->sToken.uSizeOfDataInBytes);
				if(psCurrentEntry->sToken.uSizeOfDataInBytes && !psTokenList[uNumTokens].pvData)
				{
					return IMG_NULL;
				}
				memcpy(psTokenList[uNumTokens].pvData, psCurrentEntry->sToken.pvData, psCurrentEntry->sToken.uSizeOfDataInBytes);
			}

			uNumTokens++;
		}

		psCurrentEntry = psCurrentEntry->psNext;
	}

	*puNumTokens = uNumTokens;

	return psTokenList;
}


/******************************************************************************
 * Function Name: PPCreateTokenLinkedList
 *
 * Inputs       : psTokMemHeap       - Memory heap used to allocate the tokens in the linked list
 *                psTokenList        - Array of tokens to copy
 *                uNumTokens         - Length of the array of tokens to copy
 *                psInheritedTokenEntry - NFI
 *
 * Outputs      : TokenLL            - A linked list of tokens with the same contents as psTokenList
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Copies an array of tokens and returns the generated linked list of tokens.
 *****************************************************************************/
static TokenLL *PPCreateTokenLinkedList(MemHeap     *psTokMemHeap,
										Token       *psTokenList,
										IMG_UINT32   uNumTokens,
										TokenLL     *psInheritedTokenEntry)
{
	IMG_UINT32 i;

	TokenLL *psPrev = IMG_NULL, *psFirstEntry = IMG_NULL;

	/* Copy all the tokens into a linked list structure */
	for (i = 0; i < uNumTokens; i++)
	{
		TokenLL *psNewEntry = DebugAllocHeapItem(psTokMemHeap);

		/* Copy the token */
		psNewEntry->sToken = psTokenList[i];

		/* Copy the data */
		if (psTokenList[i].uSizeOfDataInBytes)
		{
			psNewEntry->sToken.pvData = DebugMemAlloc(psTokenList[i].uSizeOfDataInBytes);
			if(psTokenList[i].uSizeOfDataInBytes && !psNewEntry->sToken.pvData)
			{
				return IMG_NULL;
			}
			memcpy(psNewEntry->sToken.pvData, psTokenList[i].pvData, psTokenList[i].uSizeOfDataInBytes);
		}

		if (psInheritedTokenEntry)
		{
			psNewEntry->sToken.pszStartOfLine = psInheritedTokenEntry->sToken.pszStartOfLine;
			psNewEntry->sToken.uStringNumber  = psInheritedTokenEntry->sToken.uStringNumber;	
			psNewEntry->sToken.uLineNumber    = psInheritedTokenEntry->sToken.uLineNumber;
			psNewEntry->sToken.uCharNumber    = psInheritedTokenEntry->sToken.uCharNumber;
			psNewEntry->bRemove               = psInheritedTokenEntry->bRemove;
		}
		else
		{
			psNewEntry->bRemove = IMG_FALSE;
		}

		psNewEntry->psPrev = psPrev;

		psNewEntry->psNext = IMG_NULL;

		if (psPrev)
		{
			psPrev->psNext = psNewEntry;
		}

		psPrev = psNewEntry;

		if (i == 0)
		{
			psFirstEntry = psNewEntry;
		}
	}
	return psFirstEntry;
}

/******************************************************************************
 * Function Name: PPRemoveTokenLLEntry
 *
 * Inputs       : psTokMemHeap - Memory heap where the token was allocated from
 *                psLLEntry    - Token to remove from a linked list.
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Remove a token from a linked list.
 *****************************************************************************/
static TokenLL *PPRemoveTokenLLEntry(MemHeap	*psTokMemHeap, TokenLL	*psLLEntry)
{
	TokenLL *psNextEntry = psLLEntry->psNext;
	TokenLL *psPrevEntry = psLLEntry->psPrev;

	if (psNextEntry)
	{
		psNextEntry->psPrev = psPrevEntry;
	}

	if (psPrevEntry)
	{
		psPrevEntry->psNext = psNextEntry;
	}

#ifdef DUMP_LOGFILES
	DumpLogMessage(LOGFILE_PREPROCESSOR, 0,"Removing '%s'\n", psLLEntry->sToken.pvData);
#endif


	if (psLLEntry->sToken.pvData)
	{
		DebugMemFree(psLLEntry->sToken.pvData);
	}
	
	DebugFreeHeapItem(psTokMemHeap, psLLEntry);

	if (psPrevEntry)
	{
#ifdef DUMP_LOGFILES
		//DumpLogMessage(LOGFILE_PREPROCESSOR, 0," returning '%s'\n", psPrevEntry->sToken.pvData);
#endif
		return psPrevEntry;
	}
	else
	{
#ifdef DUMP_LOGFILES
		//DumpLogMessage(LOGFILE_PREPROCESSOR, 0," returning '%s'\n", psNextEntry->sToken.pvData);
#endif

		return psNextEntry;
	}
}

/******************************************************************************
 * Function Name: PPInsertList
 *
 * Inputs       : psLLEntry         - Place in the destination list where the list of tokens must be inserted
 *                psFirstListEntry  - First element to insert in the destination list
 *                psLastListEntry   - Last element to insert in the destination list
 * Outputs      : -
 * Returns      : psLastListentry
 * Globals Used : -
 *
 * Description  : Inserts a list of tokens into another list of tokens.
 *****************************************************************************/
static TokenLL *PPInsertList(TokenLL  *psLLEntry,
							TokenLL  *psFirstListEntry,
							TokenLL  *psLastListEntry)
{
	TokenLL *psOldNext = psLLEntry->psNext;

	/* Insert this list into the existing one */
	psLLEntry->psNext        = psFirstListEntry;
	psFirstListEntry->psPrev = psLLEntry;
	psOldNext->psPrev        = psLastListEntry;
	psLastListEntry->psNext  = psOldNext;

	return psLastListEntry;
}


/******************************************************************************
 * Function Name: PPInsertTokensIntoLinkedList
 *
 * Inputs       : psTokMemHeap           - Memory heap where the tokens will be allocated from
 *                psLLEntry              - Place where the tokens will be inserted
 *                psTokenList            - Array of tokens to be inserted
 *                uNumTokens             - Number of tokens to be inserted
 *                psInheritedTokenEntry  - NFI
 * Outputs      : -
 * Returns      : The last element that has been inserted
 * Globals Used : -
 *
 * Description  : Inserts the tokens in the given array into a linked list.
 *****************************************************************************/
static TokenLL *PPInsertTokensIntoLinkedList(MemHeap     *psTokMemHeap,
											TokenLL     *psLLEntry,
											Token       *psTokenList,
											IMG_UINT32   uNumTokens,
											TokenLL     *psInheritedTokenEntry)
{
	if (uNumTokens)
	{
		TokenLL *psFirstEntry, *psLastEntry;

		/* Create a new linked list containing these tokens */
		psFirstEntry = PPCreateTokenLinkedList(psTokMemHeap, psTokenList, uNumTokens, psInheritedTokenEntry);

		psLastEntry = psFirstEntry;

		/* Find last entry of new list */
		while (psLastEntry->psNext)
		{
			psLastEntry = psLastEntry->psNext;
		}


#ifdef DUMP_LOGFILES
		{
			TokenLL *psNewList = psFirstEntry;
			while (psNewList)
			{
				DumpLogMessage(LOGFILE_PREPROCESSOR,
							   0,
							   "Inserting token '%s'\n",
							   psNewList->sToken.pvData ? psNewList->sToken.pvData : "no token data");
				psNewList = psNewList->psNext;
			}
		}
#endif

		/* Insert this list into the existing one */
		return PPInsertList(psLLEntry, psFirstEntry, psLastEntry);
	}
	else
	{
		return psLLEntry;
	}
}

/******************************************************************************
 * Function Name: PPReplicateAndInsertIntoLinkedList
 *
 * Inputs       : psTokMemHeap      - Memory heap where the tokens will be tokens will be allocated from.
 *                psInsertionPoint  - Place where the tokens will be inserted.
 *                psFirstListEntry  - First token to insert
 *                psLastListEntry   - Last token to insert
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Duplicates and inserts a list of tokens into another list.
 *****************************************************************************/
static TokenLL *PPReplicateAndInsertIntoLinkedList(MemHeap     *psTokMemHeap,
												TokenLL     *psInsertionPoint,
												TokenLL     *psFirstListEntry,
												TokenLL     *psLastListEntry)
{
	TokenLL *psCurrentEntry = psFirstListEntry; 
	TokenLL *psNewList = IMG_NULL, *psFirstNewListEntry = IMG_NULL;

	do
	{
		TokenLL *psLastNewEntry = psNewList;

		/* Alloc memory for the copy */
		psNewList = DebugAllocHeapItem(psTokMemHeap);

		/* Copy the entry */
		*psNewList = *psCurrentEntry;

		/* Copy the data */
		psNewList->sToken.pvData = DebugMemAlloc(psCurrentEntry->sToken.uSizeOfDataInBytes);
		if(psCurrentEntry->sToken.uSizeOfDataInBytes && !psNewList->sToken.pvData)
		{
			return IMG_NULL;
		}
		memcpy(psNewList->sToken.pvData, psCurrentEntry->sToken.pvData, psCurrentEntry->sToken.uSizeOfDataInBytes);

		/* link the new list */
		psNewList->psPrev = psLastNewEntry;

		if (psLastNewEntry)
		{
			psLastNewEntry->psNext = psNewList;
		}
		else
		{
			psFirstNewListEntry = psNewList;
		}

		/* Get the next entry */
		psCurrentEntry = PPGetNextDirectiveTokenEntry(psCurrentEntry, psLastListEntry);
	} while (psCurrentEntry);

	return PPInsertList(psInsertionPoint, psFirstNewListEntry, psNewList);
}


/******************************************************************************
 * Function Name: PPCheckForMacroReplacement
 *
 * Inputs       : psTokMemHeap     - Memory heap where any tokens will be allocated from
 *                psFirstEntry     - Beginning of the list to check for macro replacement
 *                psLastEntry      - End of the list to check for macro replacement
 *                psSymbolTable    - Macro symbol table.
 *
 * Outputs      : ppsReturnEntry   - List of tokens after aplying macro preprocessing? NFI.
 * Returns      : NFI.
 * Globals Used : -
 *
 * Description  : Scans a list of tokens and if any of them matches a macro identifier, it is replaced accordingly.
 *****************************************************************************/
static IMG_BOOL PPCheckForMacroReplacement(GLSLCompilerPrivateData *psCPD,
										   MemHeap     *psTokMemHeap,
										TokenLL     *psFirstEntry, 
										TokenLL     *psLastEntry,
										SymTable    *psSymbolTable,
										TokenLL    **ppsReturnEntry, 
										TokenLL		**psNewLastEntry)
{
	/* psLastEntry may not be the real last entry for this a macro replacement if this replacement happens 
	   in multiple lines.

	   In this case, such macro must have parameters, and we need to look over psLastEntry to try to match the brackets 
	   in order to find the real end of the replace statement.  Which means that psLastEntry will change too.
	 */

	IMG_UINT32 uSymbolID, i = 0;

	TokenLL  *psCurrentEntry  = psFirstEntry, *psFirstEntryOfMacro;

	TokenLL **ppsParameterEntries;

	IMG_CHAR *pszDefineMacroName;

	IMG_BOOL bSearchOverLastEntry = IMG_TRUE;

	if (psNewLastEntry != IMG_NULL)
	{
		(*psNewLastEntry) = IMG_NULL;
	}

	while (psCurrentEntry)
	{
		Token     *psToken        = &(psCurrentEntry->sToken);

		IMG_UINT32 uNumParameters   = 0;

		psFirstEntryOfMacro = IMG_NULL;

		ppsParameterEntries = IMG_NULL;

		if (POSSIBLE_DEFINE_NAME(psToken))
		{
			if (FindSymbol(psSymbolTable,
						   psToken->pvData,
						   &uSymbolID,
						   IMG_TRUE))
			{
				/* Get the data about this define */
				DefineMacro *psDefineMacro = (DefineMacro *)GetSymbolData(psSymbolTable, uSymbolID);
			
				if (!psDefineMacro || psDefineMacro->ePPSymbolType != PPST_DEFINEMACRO)
				{
					LOG_INTERNAL_TOKEN_ERROR((psToken,"PPCheckForMacroReplacement: Could not fetch data / data was of wrong type\n"));
					if (ppsReturnEntry) *ppsReturnEntry = psCurrentEntry;
					return IMG_FALSE;
				}

				/* Store the first entry of the macro */
				psFirstEntryOfMacro = psCurrentEntry;

				pszDefineMacroName = psToken->pvData;

#ifdef DUMP_LOGFILES
				DumpLogMessage(LOGFILE_PREPROCESSOR, 0, "Found usage of macro '%s'\n",  pszDefineMacroName);
#endif
				
				/* Does this macro take arguments? */
				if (psDefineMacro->bTakesArgs)
				{
					/* Get next entry */
					psCurrentEntry = PPGetNextDirectiveTokenEntry(psCurrentEntry, psLastEntry);

					/* Check for left bracket */
					if (psCurrentEntry && (psCurrentEntry->sToken.eTokenName == TOK_LEFT_PAREN))
					{
						/* Check for right bracket token which marks the end of the macro */
						IMG_UINT32 uNumLeftBrackets = 1;
						TokenLL * psRightBracket = psCurrentEntry->psNext;
						if (psRightBracket == IMG_NULL)
						{
							LogProgramTokenError(psCPD->psErrorLog, psToken, "'%s' : Expected macro parameter\n", pszDefineMacroName);
							if (ppsReturnEntry) 
								*ppsReturnEntry = psCurrentEntry;
							return IMG_FALSE;
						}

						while(psRightBracket)
						{
							if (psRightBracket->sToken.eTokenName == TOK_RIGHT_PAREN)
							{
								uNumLeftBrackets -= 1;
								if (uNumLeftBrackets == 0)
								{
									/* we have found a matched right bracket for this parameterised macro.
									   
									 */
									break;
								}
							}
							else if (psRightBracket->sToken.eTokenName == TOK_LEFT_PAREN)
							{
								uNumLeftBrackets += 1;
							}

							/* If we are at the end of this line and a matched right paren is still not found, we need to make sure
							   psLastEntry marks the real statement end.  This is done by searching over psLastEntry line by line.  
							   If the line containing ";", the search must be stopped, as ";" indicates the end of a statement.  
							 */
							if (psRightBracket == psLastEntry)
							{
								/* Alreay the end of the string, definitly forget right bracket for macro replacement */
								if ((psLastEntry->psNext == IMG_NULL) || 
									(psLastEntry->psNext->sToken.eTokenName == TOK_ENDOFSTRING) || 
									(psLastEntry->psNext->sToken.eTokenName == TOK_TERMINATEPARSING))
								{
									LogProgramTokenError(psCPD->psErrorLog, psToken, "'%s' : Missing right bracket\n", pszDefineMacroName);
									if (ppsReturnEntry) 
										*ppsReturnEntry = psCurrentEntry;
									return IMG_FALSE;
								}
								
								/* This line by line increatment of the psLastEntry should be stopped after encouter a line contain ";" */
								if (bSearchOverLastEntry)
								{
									TokenLL * psSavedLastEntry = psLastEntry;

									while((psLastEntry = psLastEntry->psNext) != IMG_NULL)
									{
										if (psLastEntry->sToken.eTokenName == TOK_SEMICOLON)
										{
											bSearchOverLastEntry = IMG_FALSE;
										}

										if ((psLastEntry->sToken.eTokenName == TOK_NEWLINE) || 
											(psLastEntry->sToken.eTokenName == TOK_CARRIGE_RETURN))
										{
											break; 
										}
									}

									if (psLastEntry != psSavedLastEntry)
									{
										/* we have proper search over a new line, the old new line token should be removed from the list */
										psRightBracket = psRightBracket->psNext;
										if ((psSavedLastEntry->sToken.eTokenName == TOK_NEWLINE) || 
											(psSavedLastEntry->sToken.eTokenName == TOK_CARRIGE_RETURN) || 
											(psSavedLastEntry->sToken.eTokenName == TOK_BACK_SLASH))
										{
											PPRemoveTokenLLEntry(psTokMemHeap, psSavedLastEntry);
										}

										if (psNewLastEntry != IMG_NULL)
										{
											(*psNewLastEntry) = psLastEntry;
										}
										continue;
									}
								}
							}
							
							psRightBracket = psRightBracket->psNext;
						}
						
						if (psRightBracket == IMG_NULL)
						{
							LogProgramTokenError(psCPD->psErrorLog, psToken, "'%s' : Missing right bracket\n", pszDefineMacroName);
							if (ppsReturnEntry) 
								*ppsReturnEntry = psCurrentEntry;
							return IMG_FALSE;
						}

						/* Allocate space start and end point of parameter lists */
						ppsParameterEntries  = DebugMemAlloc((psDefineMacro->uNumArgs + 1) * sizeof(TokenLL*) * 2);
						if(!ppsParameterEntries)
						{
							return IMG_FALSE;
						}
				
						while (psCurrentEntry)
						{
							IMG_UINT32 uBracketCount = 0;
							IMG_UINT32 uNumberOfParameterTokens = 0;

							psCurrentEntry = PPGetNextDirectiveTokenEntry(psCurrentEntry, psLastEntry);

							/* Check next token */
							if (!psCurrentEntry)
							{
								LogProgramTokenError(psCPD->psErrorLog, psToken, "'%s' : Expected macro parameter\n", pszDefineMacroName);
								DebugMemFree(ppsParameterEntries);
								if (ppsReturnEntry) *ppsReturnEntry = psCurrentEntry;
								return IMG_FALSE;
							}

							psToken = &(psCurrentEntry->sToken);

							/* Initialise the first entry to null in case an empty parameter was supplied to the macro */
							ppsParameterEntries[(uNumParameters * 2) + 0] = IMG_NULL;

							/* Search for last token of this parameter */
							while (psCurrentEntry)
							{
								if (psCurrentEntry->sToken.eTokenName == TOK_RIGHT_PAREN)
								{
									if (!uBracketCount)
									{
										break;
									}
									else
									{
										uBracketCount--;
									}
								}
								else if (psCurrentEntry->sToken.eTokenName == TOK_LEFT_PAREN)
								{
									uBracketCount++;
								}
								else if ((psCurrentEntry->sToken.eTokenName == TOK_COMMA) && !uBracketCount)
								{
									break;
								}

								/* Store this as current last token of parameter */
								ppsParameterEntries[(uNumParameters * 2) + 1] = psCurrentEntry;
								if(!uNumberOfParameterTokens)
								{
									/* This is also the first token of the parameter */
									ppsParameterEntries[(uNumParameters * 2) + 0] = psCurrentEntry;
								}

								uNumberOfParameterTokens++;

								psCurrentEntry = PPGetNextDirectiveTokenEntry(psCurrentEntry, psLastEntry);
							}

#ifdef DUMP_LOGFILES
							if(ppsParameterEntries[(uNumParameters * 2) + 0])
							{
								DumpLogMessage(LOGFILE_PREPROCESSOR,
										   0,
										   "Parameter %d is %d tokens '%s' to '%s'\n",
										   uNumParameters,
										   uNumberOfParameterTokens,
										   ppsParameterEntries[(uNumParameters * 2) + 0]->sToken.pvData, 
										   ppsParameterEntries[(uNumParameters * 2) + 1]->sToken.pvData);
							}
							else
							{
								DumpLogMessage(LOGFILE_PREPROCESSOR,
										   0,
										   "Parameter %d is 0 tokens\n",
										   uNumParameters);
							}
#endif

							/* Increase number of parameters */
							uNumParameters++;
							
							/* Check for too many parameters */
							if (uNumParameters >  psDefineMacro->uNumArgs)
							{
								LogProgramTokenError(psCPD->psErrorLog, psToken, "'%s' : Too many args for macro\n", pszDefineMacroName);
								DebugMemFree(ppsParameterEntries);
								if (ppsReturnEntry) *ppsReturnEntry = psCurrentEntry;
								return IMG_FALSE;
							}

							/* Check next token */
							if (!psCurrentEntry)
							{
								LogProgramTokenError(psCPD->psErrorLog, psToken, "'%s' : Expected comma or bracket\n", pszDefineMacroName);
								DebugMemFree(ppsParameterEntries);
								if (ppsReturnEntry) *ppsReturnEntry = psCurrentEntry;
								return IMG_FALSE;
							}

							psToken = &(psCurrentEntry->sToken);
				
							/* Check for comma or bracket */
							if (psToken->eTokenName == TOK_RIGHT_PAREN)
							{
								break;
							}
							else if (psToken->eTokenName != TOK_COMMA)
							{
								LogProgramTokenError(psCPD->psErrorLog, psToken, "'%s' : Expected comma or bracket\n", pszDefineMacroName);
								DebugMemFree(ppsParameterEntries);
								if (ppsReturnEntry) *ppsReturnEntry = psCurrentEntry;
								return IMG_FALSE;
							}
						}
					}
					else
					{
						LogProgramTokenError(psCPD->psErrorLog, psToken, "'%s' : Expected macro parameter\n", pszDefineMacroName);
						DebugMemFree(ppsParameterEntries);
						if (ppsReturnEntry) *ppsReturnEntry = psCurrentEntry;
						return IMG_FALSE;
					}

				} /* if (psDefineMacro->bTakesArgs) */

				/* Check number of parameters matched number of arguments */
				if (uNumParameters < psDefineMacro->uNumArgs)
				{
					LogProgramTokenError(psCPD->psErrorLog, psToken, "'%s' : Too few args for macro\n", pszDefineMacroName);

					if (ppsParameterEntries)
					{
						DebugMemFree(ppsParameterEntries);
					}
					if (ppsReturnEntry) *ppsReturnEntry = psCurrentEntry;
					return IMG_FALSE;
				}
			
				if (!psDefineMacro->uNumArgUsages)
				{
					/* Replace the current token with the defined list */
					PPInsertTokensIntoLinkedList(psTokMemHeap,
											   psCurrentEntry,
											   psDefineMacro->psTokenList,
											   psDefineMacro->uNumTokens,
											   psFirstEntryOfMacro);
				}
				else
				{
					IMG_UINT32 uStartOfTokenList = 0;
					IMG_UINT32 uNumArgsUsed      = 0;
					TokenLL   *psInsertionPoint  = psCurrentEntry;

					for (i = 0; (i < psDefineMacro->uNumTokens) && (uNumArgsUsed < psDefineMacro->uNumArgUsages); i++)
					{
						if (psDefineMacro->puArgumentPositions[uNumArgsUsed] == i)
						{
							TokenLL *psFirstParamterEntry = ppsParameterEntries[(psDefineMacro->puArgumentToUse[uNumArgsUsed] * 2) + 0];

							/* Copy all tokens up until this point in */
							psInsertionPoint = PPInsertTokensIntoLinkedList(psTokMemHeap,
																		  psInsertionPoint, 
																		  &psDefineMacro->psTokenList[uStartOfTokenList], 
																		  i - uStartOfTokenList,
																		  psFirstEntryOfMacro);

							if(psFirstParamterEntry)/* Check for empty parameter */
							{
								/* Copy the parameter tokens in */
								TokenLL *psLastParamterEntry = ppsParameterEntries[(psDefineMacro->puArgumentToUse[uNumArgsUsed] * 2) + 1];							
								psInsertionPoint = PPReplicateAndInsertIntoLinkedList(psTokMemHeap,
																		 psInsertionPoint, 
																		 psFirstParamterEntry, 
																		 psLastParamterEntry);
							}

							uNumArgsUsed++;

							uStartOfTokenList = i+1;
						}
					}

					/* Copy any remaining tokens in */
					psInsertionPoint = PPInsertTokensIntoLinkedList(psTokMemHeap,
																  psInsertionPoint,
																  &psDefineMacro->psTokenList[uStartOfTokenList],
																  psDefineMacro->uNumTokens - uStartOfTokenList,
																  psFirstEntryOfMacro);

					if (uNumArgsUsed != psDefineMacro->uNumArgUsages)
					{
						LOG_INTERNAL_TOKEN_ERROR((psToken,"'%s' : Argument counts don't match\n", pszDefineMacroName));
					}
				}

#ifdef DUMP_LOGFILES
				DumpLogMessage(LOGFILE_PREPROCESSOR,
							   5,
							   "Replacing '%s' (%d:%d:%d) with '",
							   psToken->pvData,
							   psToken->uStringNumber,
							   psToken->uLineNumber,
							   psToken->uCharNumber);

				for (i = 0; i <  psDefineMacro->uNumTokens; i++)
				{
					DumpLogMessage(LOGFILE_PREPROCESSOR, 0, "%s", psDefineMacro->psTokenList[i].pvData);

					if (i < psDefineMacro->uNumTokens - 1)
					{
						DumpLogMessage(LOGFILE_PREPROCESSOR, 0, " ");
					}
				}

				DumpLogMessage(LOGFILE_PREPROCESSOR, 0, "'\n");
#endif

				if (ppsParameterEntries)
				{
					DebugMemFree(ppsParameterEntries);
					ppsParameterEntries = IMG_NULL;
				}


				if (psCurrentEntry == psFirstEntry)
				{
					/* This is now the first entry */
					psFirstEntry = psCurrentEntry->psNext;
				}
			}
			/* Handle defined situations */
			else if (strcmp(psToken->pvData, "defined") == 0)
			{
				IMG_BOOL bBracketed = IMG_FALSE;

				IMG_CHAR *pszDefineName;

				psFirstEntryOfMacro = psCurrentEntry;

				psCurrentEntry = PPGetNextDirectiveTokenEntry(psCurrentEntry, psLastEntry);

				/* Check for left bracket */
				if (psCurrentEntry && psCurrentEntry->sToken.eTokenName == TOK_LEFT_PAREN)
				{
					bBracketed = IMG_TRUE;
					
					psCurrentEntry = PPGetNextDirectiveTokenEntry(psCurrentEntry, psLastEntry);
				}

				/* Check for name of define we're checking */
				if (!psCurrentEntry || !POSSIBLE_DEFINE_NAME(&(psCurrentEntry->sToken)))
				{
					LogProgramTokenError(psCPD->psErrorLog, psToken, "'defined' : expected define name\n");
					if (ppsReturnEntry) *ppsReturnEntry = psCurrentEntry;
					return IMG_FALSE;
				}

				pszDefineName = psCurrentEntry->sToken.pvData;

				/* If there was a left bracket the check for a right one */
				if (bBracketed)
				{
					psCurrentEntry = PPGetNextDirectiveTokenEntry(psCurrentEntry, psLastEntry);

					if (!psCurrentEntry || psCurrentEntry->sToken.eTokenName != TOK_RIGHT_PAREN)
					{
						LogProgramTokenError(psCPD->psErrorLog, psToken, "'defined' : expected right bracket\n");
						if (ppsReturnEntry) *ppsReturnEntry = psCurrentEntry;
						return IMG_FALSE;
					}
				}

				/*
				   Don't need to replace these tokens, simply change the identifer/right bracket token to be
				   an int constant with a value of 1 if the define exists or 0 if it doesn't
				*/
				psCurrentEntry->sToken.eTokenName = TOK_INTCONSTANT;


#ifdef DUMP_LOGFILES
				DumpLogMessage(LOGFILE_PREPROCESSOR, 0, "Found usage of 'defined'\n");

				DumpLogMessage(LOGFILE_PREPROCESSOR,
							   5,
							   "Replacing '%s' (%d:%d:%d) with '",
							   psCurrentEntry->sToken.pvData,
							   psCurrentEntry->sToken.uStringNumber,
							   psCurrentEntry->sToken.uLineNumber,
							   psCurrentEntry->sToken.uCharNumber);
#endif


				if (FindSymbol(psSymbolTable,
							   pszDefineName,
							   &uSymbolID,
							   IMG_TRUE))
				{
					strcpy(psCurrentEntry->sToken.pvData, "1");
				}
				else
				{
					strcpy(psCurrentEntry->sToken.pvData, "0");
				}

#ifdef DUMP_LOGFILES
				DumpLogMessage(LOGFILE_PREPROCESSOR, 0, "%s'\n",  psCurrentEntry->sToken.pvData);
#endif
				/* Wind the pointer back one since we don't want to remove the entry we've replaced */
				psCurrentEntry = psCurrentEntry->psPrev;

			}
		}
		

		psCurrentEntry = PPGetNextDirectiveTokenEntry(psCurrentEntry, psLastEntry);


		/* Remove all tokens that made up the macro */
		if (psCurrentEntry == IMG_NULL)
		{
			if (psFirstEntryOfMacro != IMG_NULL)
			{
				PPRemoveTokenLLEntry(psTokMemHeap, psFirstEntryOfMacro);
			}
		}
		else
		{
			while (psFirstEntryOfMacro && (psFirstEntryOfMacro != psCurrentEntry))
			{
				TokenLL *psNextEntry = psFirstEntryOfMacro->psNext;

				PPRemoveTokenLLEntry(psTokMemHeap, psFirstEntryOfMacro);

				psFirstEntryOfMacro = psNextEntry;
			}
		}
	}

#ifdef DUMP_LOGFILES

	//DumpLogMessage(LOGFILE_PREPROCESSOR, 0, "Returning with a pointer to '%s'\n", psFirstEntry->sToken.pvData);
#endif

	if (ppsReturnEntry) *ppsReturnEntry = psFirstEntry;
	return IMG_TRUE;
}



/******************************************************************************
 * Function Name: PPDestroyTokenLinkedList
 *
 * Inputs       : psTokMemHeap   - Memory heap where the tokens were allocated from.
 *                psEntry        - First element of the linked list.
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Destroys a linked list of tokens.
 *****************************************************************************/
static IMG_BOOL PPDestroyTokenLinkedList(MemHeap     *psTokMemHeap, TokenLL *psEntry)
{
	TokenLL *psCurrentEntry = psEntry;

	/* Go to start of list */
	while (psCurrentEntry->psPrev)
	{
		psCurrentEntry = psCurrentEntry->psPrev;
	}
	
	/* Go through all of list deleting members */
	while (psCurrentEntry)
	{
		TokenLL *psNext = psCurrentEntry->psNext;

		if (psCurrentEntry->sToken.pvData)
		{
			DebugMemFree(psCurrentEntry->sToken.pvData);
		}

		DebugFreeHeapItem(psTokMemHeap, psCurrentEntry);

		psCurrentEntry = psNext;
	}

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: PPGetTokenPrecedence
 *
 * Inputs       : psToken             - Token whose precedence we are interested in.
 *                nPrecedenceAdjuster - Value added to the token precedence.
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Returns the precedence of a token added to nPrecedenceAdjuster.
 *****************************************************************************/
static IMG_UINT32 PPGetTokenPrecedence(Token *psToken, IMG_INT32 nPrecedenceAdjuster)
{
	switch (psToken->eTokenName)
	{
		case TOK_AND_OP:
		case TOK_OR_OP:
			return 1 + nPrecedenceAdjuster;
		case TOK_AMPERSAND:
		case TOK_CARET:
		case TOK_VERTICAL_BAR:
			return 2 + nPrecedenceAdjuster;
		case TOK_EQ_OP:
		case TOK_NE_OP:
			return 3 + nPrecedenceAdjuster;
		case TOK_LEFT_ANGLE:
		case TOK_RIGHT_ANGLE:
		case TOK_LE_OP:
		case TOK_GE_OP:
			return 4 + nPrecedenceAdjuster;
		case TOK_LEFT_OP:
		case TOK_RIGHT_OP:
			return 5 + nPrecedenceAdjuster;
		case TOK_PLUS:
		case TOK_DASH:
			return 6 + nPrecedenceAdjuster;
		case TOK_STAR:
		case TOK_SLASH:
		case TOK_PERCENT:
			return 7 + nPrecedenceAdjuster;
		case TOK_BANG:
		case TOK_TILDE:
			return 9 + nPrecedenceAdjuster;
		case TOK_INTCONSTANT:
			return 11 + nPrecedenceAdjuster;
		default:
			DEBUG_MESSAGE(("PPGetTokenPrecedence: Unrecognised token (0x%04X, '%s')\n", psToken->eTokenName, (IMG_CHAR*)psToken->pvData));
			return 0;
	}
}

/******************************************************************************
 * Function Name: PPAddChildNode
 *
 * Inputs       : psParentNode      - Node where the child will be added to
 *                psToken           - Child node to add to the parent node
 *                uTokenPrecedence  - Precedence of the child node.
 * Outputs      : -
 * Returns      : The parent node, taking into consideration the precendence of the newly inserted token.
 * Globals Used : -
 *
 * Description  : NFI.
 *****************************************************************************/
static ExpressionNode *PPAddChildNode(GLSLCompilerPrivateData *psCPD, ExpressionNode *psParentNode, Token *psToken, IMG_UINT32 uTokenPrecedence)
{
	ExpressionNode *psChildNode;
	IMG_UINT32 i;

	if (psParentNode)
	{
		if (psParentNode->uNumChildren >= 2)
		{
			LOG_INTERNAL_TOKEN_ERROR((psToken,"PPAddChildNode: No more room for children\n"));
			return IMG_NULL;
		}
	}

	psChildNode = DebugMemAlloc(sizeof(ExpressionNode));

	if(!psChildNode)
	{
		return IMG_NULL;
	}

	psChildNode->psToken          = psToken;
	psChildNode->uTokenPrecedence = uTokenPrecedence;
	psChildNode->psParentNode     = psParentNode;
	psChildNode->uNumChildren     = 0;

	for (i = 0; i < 2; i++)
	{
		psChildNode->psChildNode[i] = IMG_NULL;
	}

	if (psParentNode)
	{
		psParentNode->psChildNode[psParentNode->uNumChildren] = psChildNode;
		psParentNode->uNumChildren++;
	}

	return psChildNode;
}

/******************************************************************************
 * Function Name: PPAddParentNode
 *
 * Inputs       : psChildNode       -
 *                psToken           -
 *                uTokenPrecedence  -
 *                bInsert           - 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : NFI.
 *****************************************************************************/
static ExpressionNode *PPAddParentNode(GLSLCompilerPrivateData *psCPD, ExpressionNode *psChildNode, Token *psToken, IMG_UINT32 uTokenPrecedence, IMG_BOOL bInsert)
{
	ExpressionNode *psParentNode;
	ExpressionNode *psOldParentNode = IMG_NULL;

	if (!psChildNode)
	{
		LOG_INTERNAL_TOKEN_ERROR((psToken,"PPAddParentNode: No child node to add parent to\n"));
		return IMG_NULL;
	
	}

	if (bInsert)
	{
		psOldParentNode = psChildNode->psParentNode;

		if (!psOldParentNode)
		{
			LOG_INTERNAL_TOKEN_ERROR((psToken,"PPAddParentNode: Cannot insert, no parent node for child\n"));
			return IMG_NULL;
		}
	}
	else if (psChildNode->psParentNode)
	{
		LOG_INTERNAL_TOKEN_ERROR((psToken,"PPAddParentNode: Child already has parent\n"));
		return IMG_NULL;
	}

	psParentNode = DebugMemAlloc(sizeof(ExpressionNode));

	if(!psParentNode)
	{
		return IMG_NULL;
	}

	psParentNode->psToken          = psToken;
	psParentNode->uTokenPrecedence = uTokenPrecedence;
	psParentNode->psChildNode[0]   = psChildNode;
	psParentNode->uNumChildren     = 1;
	psParentNode->psParentNode     = psOldParentNode;

	/* Replace the pointer to the child node with a pointer to the newly created node */
	if (bInsert)
	{
		IMG_UINT32 i;
		IMG_BOOL bInserted = IMG_FALSE;

		for (i = 0; i < psOldParentNode->uNumChildren; i++)
		{
			if (psOldParentNode->psChildNode[i] == psChildNode)
			{
				psOldParentNode->psChildNode[i] = psParentNode;
				bInserted = IMG_TRUE;
			}
		}

		if (!bInserted)
		{
			LOG_INTERNAL_TOKEN_ERROR((psToken,"PPAddParentNode: Cannot insert, failed to find matching child nodea\n"));
			return IMG_NULL;

		}
	}

	return psParentNode;
}


/******************************************************************************
 * Function Name: PPCleanUpBranches
 *
 * Inputs       : psExpressionTree - Expression tree to destroy.
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Frees recursively an expression tree. See PPFreeExpressionTree.
 *****************************************************************************/
static IMG_VOID PPCleanUpBranches(ExpressionNode *psExpressionTree)
{
	IMG_UINT32 i;

	for (i = 0; i < psExpressionTree->uNumChildren; i++)
	{
		PPCleanUpBranches(psExpressionTree->psChildNode[i]);
	}

	DebugMemFree(psExpressionTree);
}

/******************************************************************************
 * Function Name: PPFreeExpressionTree
 *
 * Inputs       : psExpressionTree - A node in the tree to be freed.
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Frees a whole expression tree given any of its nodes.
 *****************************************************************************/
static IMG_VOID PPFreeExpressionTree(ExpressionNode *psExpressionTree)
{
	if (psExpressionTree)
	{
		/* Traverse back to top of tree */
		while (psExpressionTree->psParentNode)
		{
			psExpressionTree = psExpressionTree->psParentNode;
		}

		PPCleanUpBranches(psExpressionTree);
	}
}


/* 
1. If it's precedence is lower than or equal to the current node and lower than or equal to the parent node then make the parent node the current node.
2. If it's precedence is lower than or equal to the current node but higher than the parent node then insert it between the two nodes.
3. If it's precedence is higher than the current node than make it a child of the current node.
*/


/******************************************************************************
 * Function Name: PPCreateExpressionTree
 *
 * Inputs       : psTokenEntry     - First token in the expression
 *                psLastTokenEntry - Last token in the expression
 * Outputs      : -
 * Returns      : An expression tree formed by the token list.
 * Globals Used : -
 *
 * Description  : Given a list of tokens it builds an expression tree with them.
 *****************************************************************************/
static ExpressionNode *PPCreateExpressionTree(GLSLCompilerPrivateData *psCPD, TokenLL  *psTokenEntry,
										 TokenLL  *psLastTokenEntry)
{
	Token *psLastToken = IMG_NULL;
	ExpressionNode *psCurrentNode = IMG_NULL;
	IMG_INT32 nPrecedenceAdjuster = 0;

	/* Get 1st token */
	psTokenEntry = PPGetNextDirectiveTokenEntry(psTokenEntry, psLastTokenEntry);

	if (!psTokenEntry)
	{
		LOG_INTERNAL_ERROR(("PPCreateExpressionTree: No tokens!\n"));
		return IMG_NULL;
	}

	while (psTokenEntry)
	{
		IMG_UINT32 uTokenPrecedence;

		Token *psToken = &psTokenEntry->sToken;

		/* Brackets adjust tokens precedence */
		if (psToken->eTokenName == TOK_LEFT_PAREN)
		{
			nPrecedenceAdjuster += 20;
		}
		else if (psToken->eTokenName == TOK_RIGHT_PAREN)
		{
			nPrecedenceAdjuster -= 20;

			if (nPrecedenceAdjuster < 0)
			{
				LogProgramTokenError(psCPD->psErrorLog, psToken, "Syntax error, unexpected ')'\n");
				PPFreeExpressionTree(psCurrentNode);
				return IMG_NULL;
			}
		}
		else
		{
			/* Get tokens precedence value */ 
			uTokenPrecedence = PPGetTokenPrecedence(psToken, nPrecedenceAdjuster);

			/* Was the token not recognised */
			if (!uTokenPrecedence)
			{
				LogProgramTokenError(psCPD->psErrorLog, psToken, "Syntax error, unexpected '%s'\n", psToken->pvData);

				/* This expression will evaluate to 0 then */
				PPFreeExpressionTree(psCurrentNode);
				return IMG_NULL;
			}

			/* Do we have an node yet? */
			if (psCurrentNode)
			{
				IMG_BOOL bNodeCreated = IMG_FALSE;

				while (!bNodeCreated)
				{
					/* Does this token have a higher precedence that the current node? */
					if (uTokenPrecedence > psCurrentNode->uTokenPrecedence)
					{
						/* Add this node as a child */
						psCurrentNode = PPAddChildNode(psCPD, psCurrentNode, psToken, uTokenPrecedence);

						bNodeCreated = IMG_TRUE;
					}
					/* Token has a lower or equal precedence */
					else 
					{
						/* Does it have a parent node? */
						if (psCurrentNode->psParentNode)
						{
							/* Does it have a lower precedence than the parent node? */
							if (uTokenPrecedence <= psCurrentNode->psParentNode->uTokenPrecedence)
							{
								/* Yes make the current node the parent node and keep moving back up the tree */
								psCurrentNode = psCurrentNode->psParentNode;
							}
							else
							{
								/* No, insert this node between the two */
								psCurrentNode = PPAddParentNode(psCPD, psCurrentNode, psToken, uTokenPrecedence, IMG_TRUE);

								bNodeCreated = IMG_TRUE;
							}
						}
						/* No parent node so make this one top node */
						else
						{
							psCurrentNode = PPAddParentNode(psCPD, psCurrentNode, psToken, uTokenPrecedence, IMG_FALSE);

							bNodeCreated = IMG_TRUE;
						}
					}
				}
			}
			else
			{
				/* First node, add it to top of tree */
				psCurrentNode = PPAddChildNode(psCPD, psCurrentNode, psToken, uTokenPrecedence);
			}
			
			/* Check node was succesfully created */
			if (!psCurrentNode)
			{
				/* Failure to add node was probably caused by incorrect token in stream */
				LogProgramTokenError(psCPD->psErrorLog, psToken, "Syntax error (adding node failed)\n");
				PPFreeExpressionTree(psCurrentNode);
				return IMG_NULL;
			}
		}
		
		psLastToken = psToken;

		/* Get the next token */
		psTokenEntry = PPGetNextDirectiveTokenEntry(psTokenEntry, psLastTokenEntry);
	}

	/* Were all brackets closed? */
	if (nPrecedenceAdjuster)
	{
		LogProgramTokenError(psCPD->psErrorLog, psLastToken, "Syntax error, expected ')'\n");
		PPFreeExpressionTree(psCurrentNode);
		return IMG_NULL;
	}


	/* Traverse back to top of tree */
	if (psCurrentNode)
	{
		while (psCurrentNode->psParentNode)
		{
			psCurrentNode = psCurrentNode->psParentNode;
		}
	}

	return psCurrentNode;

}
/******************************************************************************
 * Function Name: PPCalculateResultOfExpressionTree
 *
 * Inputs       : psExpressionTree  - Expression tree to evaluate.
 *                psSymbolTable     - Symbol table of macros.
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Evaluates an expression tree given a symbol table with the values of all defined macros.
 *****************************************************************************/
static IMG_INT32 PPCalculateResultOfExpressionTree(GLSLCompilerPrivateData *psCPD, ExpressionNode *psExpressionTree, SymTable *psSymbolTable)
{
	ExpressionNode *psCurrentNode = psExpressionTree;

	Token *psToken = psCurrentNode->psToken;

	IMG_INT32 nSrcs[2];

	IMG_UINT32 uNumExpectedChildren, i;

	switch (psToken->eTokenName)
	{
		case TOK_AND_OP: /* Intentional fall-through */
		case TOK_OR_OP:
		case TOK_AMPERSAND:
		case TOK_CARET:
		case TOK_VERTICAL_BAR:
		case TOK_EQ_OP:
		case TOK_NE_OP:
		case TOK_LEFT_ANGLE:
		case TOK_RIGHT_ANGLE:
		case TOK_LE_OP:
		case TOK_GE_OP:
		case TOK_LEFT_OP:
		case TOK_RIGHT_OP:
		case TOK_STAR:
		case TOK_SLASH:
		case TOK_PERCENT:
			uNumExpectedChildren = 2;
			break;

		case TOK_PLUS: /* Intentional fall-through */
		case TOK_DASH:
			if (psCurrentNode->uNumChildren == 1 || psCurrentNode->uNumChildren == 2)
			{
				uNumExpectedChildren = psCurrentNode->uNumChildren;
			}
			else
			{
				uNumExpectedChildren = 2;
			}
			break;

		case TOK_BANG: /* Intentional fall-through */
		case TOK_TILDE:
			uNumExpectedChildren = 1;
			break;

		case TOK_INTCONSTANT:
			uNumExpectedChildren = 0;
			break;

		default:
			LogProgramTokenError(psCPD->psErrorLog, psToken, "Syntax error (2)\n");
			LOG_INTERNAL_ERROR(("PPCalculateResultOfExpressionTree: Unrecognised token\n"));
			return 0;
	}


	/* Check number of children was correct */
	if (psCurrentNode->uNumChildren != uNumExpectedChildren)
	{
		LOG_INTERNAL_TOKEN_ERROR((psToken,"PPCalculateResultOfExpressionTree: Unexpected number of children for '%s' (%d vs %d)\n",
							  psToken->pvData,
							  psCurrentNode->uNumChildren,
							  uNumExpectedChildren));

		if(psCurrentNode->uNumChildren)
		{
			printf("Child was '%s'\n",(IMG_CHAR *)psCurrentNode->psChildNode[0]->psToken->pvData);
		}

		LogProgramTokenError(psCPD->psErrorLog, psToken, "Syntax error (3)\n");
		return 0;
	}


	/* recursively pick up values of children */
	for (i = 0; i < psCurrentNode->uNumChildren; i++)
	{
		nSrcs[i] = PPCalculateResultOfExpressionTree(psCPD, psCurrentNode->psChildNode[i], psSymbolTable);
	}

	switch (psToken->eTokenName)
	{
		case TOK_AND_OP:
			return nSrcs[0] && nSrcs[1];
		case TOK_OR_OP:
			return nSrcs[0] || nSrcs[1];
		case TOK_AMPERSAND:
			return nSrcs[0] & nSrcs[1];
		case TOK_CARET:
			return nSrcs[0] ^ nSrcs[1];
		case TOK_VERTICAL_BAR:
			return nSrcs[0] | nSrcs[1];
		case TOK_EQ_OP:
			return nSrcs[0] == nSrcs[1];
		case TOK_NE_OP:
			return nSrcs[0] != nSrcs[1];
		case TOK_LEFT_ANGLE:
			return nSrcs[0] < nSrcs[1];
		case TOK_RIGHT_ANGLE:
			return nSrcs[0] > nSrcs[1];
		case TOK_LE_OP:
			return nSrcs[0] <= nSrcs[1];
		case TOK_GE_OP:
			return nSrcs[0] >= nSrcs[1];
		case TOK_LEFT_OP:
			return nSrcs[0] << nSrcs[1];
		case TOK_RIGHT_OP:
			return nSrcs[0] >> nSrcs[1];
		case TOK_PLUS:
			if (psCurrentNode->uNumChildren == 1)
			{
				return nSrcs[0];
			}
			else
			{
				return nSrcs[0] + nSrcs[1];
			}
		case TOK_DASH:
			if (psCurrentNode->uNumChildren == 1)
			{
				return -nSrcs[0];
			}
			else
			{
				return nSrcs[0] - nSrcs[1];
			}
		case TOK_STAR:
			return nSrcs[0] * nSrcs[1];
		case TOK_SLASH:
			return nSrcs[0] / nSrcs[1];
		case TOK_PERCENT:
			return nSrcs[0] % nSrcs[1];
		case TOK_BANG:
			return !(nSrcs[0]);
		case TOK_TILDE:
			return ~(nSrcs[0]);
		case TOK_INTCONSTANT:
			return strtol(psToken->pvData,0,0);
		default:
			LogProgramTokenError(psCPD->psErrorLog, psToken, "Syntax error\n");
			LOG_INTERNAL_TOKEN_ERROR((psToken,"PPCalculateResultOfExpressionTree: Unrecognised token\n"));
			return 0;
	}
}

/******************************************************************************
 * Function Name: PPProcessIf
 *
 * Inputs       : psTokMemHeap      - Memory heap where the tokens will be allocated from.
 *                psTokenEntry      - First token to process
 *                psLastTokenEntry  - Last token to process
 *                psSymbolTable     - Symbol table with macro definitions.
 *                psIfStatus        - NFI.
 *
 * Outputs      : NFI.
 * Returns      : 
 * Globals Used : -
 *
 * Description  : NFI.
 *****************************************************************************/
static IMG_BOOL PPProcessIf(GLSLCompilerPrivateData *psCPD,
							MemHeap     *psTokMemHeap,
							TokenLL     *psTokenEntry,
							TokenLL     *psLastTokenEntry,
							SymTable    *psSymbolTable,
							IfStatus    *psIfStatus)
{
	/* Only evaluate the expression if we're inside an enabled block */
	if (BLOCK_ENABLED(psIfStatus->aeIfBlockStates[psIfStatus->uCurrentActiveLevel]))
	{
		ExpressionNode *psExpressionTree; 

		IMG_BOOL bIfResult = IMG_FALSE;

		/* Check for enough tokens */
		if (!PPGetNextDirectiveTokenEntry(psTokenEntry, psLastTokenEntry))
		{
			LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "invalid integer constant expression\n");
			return IMG_FALSE;
		}

		/* Check for macro usage within */
		if (!PPCheckForMacroReplacement(psCPD, psTokMemHeap, psTokenEntry, psLastTokenEntry, psSymbolTable, &psTokenEntry, IMG_NULL))
		{
			return IMG_FALSE;
		}

		/* Create expression tree */
		psExpressionTree = PPCreateExpressionTree(psCPD, psTokenEntry, psLastTokenEntry);

		if (psExpressionTree)
		{
			/* Calculate result of expression */
			bIfResult = PPCalculateResultOfExpressionTree(psCPD, psExpressionTree, psSymbolTable) > 0 ? IMG_TRUE : IMG_FALSE;

			/* Free the expression tree */
			PPFreeExpressionTree(psExpressionTree);
		}


#ifdef DUMP_LOGFILES
	DumpLogMessage(LOGFILE_PREPROCESSOR,
				   0,
				   "#if  %s (%d:%d:%d)\n",
				   bIfResult ? "TRUE" : "FALSE",
				   psTokenEntry->sToken.uStringNumber,
				   psTokenEntry->sToken.uLineNumber,
				   psTokenEntry->sToken.uCharNumber);
#endif

		psIfStatus->uNestedIfLevel++;

		/* Check we haven't exceeded maximum nesting level */
		if (psIfStatus->uNestedIfLevel >= MAX_NESTED_IF_LEVELS)
		{
			LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "Max number of nested #if's is 255\n");
			return IMG_FALSE;
		}

		if (bIfResult)
		{
			psIfStatus->aeIfBlockStates[psIfStatus->uNestedIfLevel] = IFSTATE_BLOCK_IF_ENABLED;
		}
		else
		{
			psIfStatus->aeIfBlockStates[psIfStatus->uNestedIfLevel] = IFSTATE_BLOCK_IF_DISABLED;
		}

		/* Only update the current active #if status if we're inside an enabled block */
		if (BLOCK_ENABLED(psIfStatus->aeIfBlockStates[psIfStatus->uCurrentActiveLevel]))
		{
			psIfStatus->uCurrentActiveLevel = psIfStatus->uNestedIfLevel;
		}
	}
	else
	{

#ifdef DUMP_LOGFILES
	DumpLogMessage(LOGFILE_PREPROCESSOR,
				   0,
				   "#if  not processed (inside disabled block) (%d:%d:%d)\n",
				   psTokenEntry->sToken.uStringNumber,
				   psTokenEntry->sToken.uLineNumber,
				   psTokenEntry->sToken.uCharNumber);
#endif

		/* Just keep a count so we can match the endifs */
		psIfStatus->uNestedIfLevel++;
	}

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: PPProcessElif
 *
 * Inputs       : psTokMemHeap      - Memory heap where the tokens will be allocated from.
 *                psTokenEntry      - First token to process
 *                psLastTokenEntry  - Last token to process
 *                psSymbolTable     - Symbol table with macro definitions.
 *                psIfStatus        - NFI.
 *
 * Outputs      : NFI.
 * Returns      : 
 * Globals Used : -
 *
 * Description  : NFI.
 *****************************************************************************/
static IMG_BOOL PPProcessElif(GLSLCompilerPrivateData *psCPD, MemHeap     *psTokMemHeap,
							TokenLL     *psTokenEntry,
							TokenLL     *psLastTokenEntry,
							SymTable    *psSymbolTable,
							IfStatus    *psIfStatus)
{
	/* Only evaluate the expression if we're inside an enabled block */
	if (BLOCK_ENABLED(psIfStatus->aeIfBlockStates[psIfStatus->uCurrentActiveLevel]) || 
		(psIfStatus->uCurrentActiveLevel == psIfStatus->uNestedIfLevel))
	{
		ExpressionNode *psExpressionTree; 

		IfBlockState *peIfBlockState = &(psIfStatus->aeIfBlockStates[psIfStatus->uNestedIfLevel]);

		IMG_BOOL bElifResult = IMG_FALSE;

		/* Check we're not at the base level */
		if (!psIfStatus->uNestedIfLevel)
		{
			LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "'' : syntax error #elif mismatch\n");
			return IMG_FALSE;
		}

		/* If we have already found an enabled #if block and are now ignoring 
		   all further #ifs until the matching #endif, skip processing this 
		   #elif - we know it is disabled  */
		if(*peIfBlockState == IFSTATE_BLOCK_IGNORE_ALL_FURTHER_IFS)
		{
			return IMG_TRUE;
		}

		/* Was the current block enabled? */
		if (BLOCK_ENABLED(*peIfBlockState))
		{
			/* Yes - then this and all further #else and #elif's should be ignored until we reach an #endif */
			*peIfBlockState = IFSTATE_BLOCK_IGNORE_ALL_FURTHER_IFS;

			return IMG_TRUE; 
		}


		/* Check for enough tokens */
		if (!PPGetNextDirectiveTokenEntry(psTokenEntry, psLastTokenEntry))
		{
			LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "invalid integer constant expression\n");
			return IMG_FALSE;
		}

		/* Check for macro usage within */
		if (!PPCheckForMacroReplacement(psCPD, psTokMemHeap, psTokenEntry, psLastTokenEntry, psSymbolTable, &psTokenEntry, IMG_NULL))
		{
			return IMG_FALSE;
		}

		/* Create expression tree */
		psExpressionTree = PPCreateExpressionTree(psCPD, psTokenEntry, psLastTokenEntry);

		if (psExpressionTree)
		{
			/* Calculate result of expression */
			bElifResult = PPCalculateResultOfExpressionTree(psCPD, psExpressionTree, psSymbolTable) > 0 ? IMG_TRUE : IMG_FALSE;

			/* Free the expression tree */
			PPFreeExpressionTree(psExpressionTree);
		}

#ifdef DUMP_LOGFILES
	DumpLogMessage(LOGFILE_PREPROCESSOR,
				   0,
				   "#elif  %s (%d:%d:%d)\n",
				   bElifResult ? "TRUE" : "FALSE",
				   psTokenEntry->sToken.uStringNumber,
				   psTokenEntry->sToken.uLineNumber,
				   psTokenEntry->sToken.uCharNumber);
#endif

		/* Update the current status */
		if (bElifResult)
		{
			/* Mark this block as enabled */
			*peIfBlockState  = IFSTATE_BLOCK_ELIF_ENABLED;
		}	
		else
		{
			/* Mark this block as disabled */
			*peIfBlockState  = IFSTATE_BLOCK_ELIF_DISABLED;
		}
	}
	else
	{
#ifdef DUMP_LOGFILES
		DumpLogMessage(LOGFILE_PREPROCESSOR,
					   0,
					   "#elif  not processed (inside disabled block) (%d:%d:%d)\n",
					   psTokenEntry->sToken.uStringNumber,
					   psTokenEntry->sToken.uLineNumber,
					   psTokenEntry->sToken.uCharNumber);
#endif
	}

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: PPProcessElse
 *
 * Inputs       : psTokenEntry      - First token to process
 *                psLastTokenEntry  - Last token to process
 *                psIfStatus        - NFI.
 *
 * Outputs      : NFI.
 * Returns      : 
 * Globals Used : -
 *
 * Description  : NFI.
 *****************************************************************************/
static IMG_BOOL PPProcessElse(GLSLCompilerPrivateData *psCPD, TokenLL  *psTokenEntry,
							TokenLL  *psLastTokenEntry,
							IfStatus *psIfStatus)
{
	/* Only evaluate the expression if we're inside an enabled block */
	if (BLOCK_ENABLED(psIfStatus->aeIfBlockStates[psIfStatus->uCurrentActiveLevel]) || 
		(psIfStatus->uCurrentActiveLevel == psIfStatus->uNestedIfLevel))
	{

		IfBlockState *peIfBlockState = &(psIfStatus->aeIfBlockStates[psIfStatus->uNestedIfLevel]);

		PVR_UNREFERENCED_PARAMETER(psLastTokenEntry);

		/* Check we're not at the base level */
		if (!psIfStatus->uNestedIfLevel)
		{
			LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "'' : syntax error #else mismatch\n");
			return IMG_FALSE;
		}

		/* Check for a previous #else */
		if (*peIfBlockState == IFSTATE_BLOCK_ELSE_ENABLED ||
			*peIfBlockState == IFSTATE_BLOCK_ELSE_DISABLED)
		{
			LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "'' : syntax error #else after a #else\n");
			return IMG_FALSE;
		}

		if (*peIfBlockState != IFSTATE_BLOCK_IGNORE_ALL_FURTHER_IFS)
		{
			/* Invert the block enables */
			if (BLOCK_ENABLED(*peIfBlockState))
			{
				*peIfBlockState = IFSTATE_BLOCK_ELSE_DISABLED;

#ifdef DUMP_LOGFILES
	DumpLogMessage(LOGFILE_PREPROCESSOR,
				   0,
				   "#else  TRUE (%d:%d:%d)\n",
				   psTokenEntry->sToken.uStringNumber,
				   psTokenEntry->sToken.uLineNumber,
				   psTokenEntry->sToken.uCharNumber);
#endif
			}
			else
			{
				*peIfBlockState = IFSTATE_BLOCK_ELSE_ENABLED;

#ifdef DUMP_LOGFILES
	DumpLogMessage(LOGFILE_PREPROCESSOR,
				   0,
				   "#else  FALSE (%d:%d:%d)\n",
				   psTokenEntry->sToken.uStringNumber,
				   psTokenEntry->sToken.uLineNumber,
				   psTokenEntry->sToken.uCharNumber);
#endif
			}
		}
	}

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: PPProcessIfdef
 *
 * Inputs       : psTokenEntry      - First token to process
 *                psLastTokenEntry  - Last token to process
 *                psSymbolTable     - Symbol table with macro definitions.
 *                psIfStatus        - NFI.
 *                bInvertResult     - NFI.
 *
 * Outputs      : NFI.
 * Returns      : 
 * Globals Used : -
 *
 * Description  : NFI.
 *****************************************************************************/
static IMG_BOOL PPProcessIfdef(GLSLCompilerPrivateData *psCPD, TokenLL  *psTokenEntry,
							TokenLL  *psLastTokenEntry,
							SymTable *psSymbolTable,
							IfStatus *psIfStatus,
							IMG_BOOL  bInvertResult)
{
	IMG_BOOL bIfResult = IMG_FALSE;

	/* Only evaluate the expression if we're inside an enabled block */
	if (BLOCK_ENABLED(psIfStatus->aeIfBlockStates[psIfStatus->uCurrentActiveLevel]))
	{
		psTokenEntry = PPGetNextDirectiveTokenEntry(psTokenEntry, psLastTokenEntry);

		/* Check for enough tokens */
		if (!psTokenEntry)
		{
			LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "Syntax error, #if[n]def expected an identifier\n");
			return IMG_FALSE;
		}

		if (!POSSIBLE_DEFINE_NAME(&psTokenEntry->sToken))
		{
			LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "Syntax error, #if[n]def expected an identifier\n");
			return IMG_FALSE;
		}

		/* After the identifier, we must have either a newline or tab followed by newline - tab token is used for concatenated whitespace. */
		if(!(psTokenEntry->psNext && (psTokenEntry->psNext->sToken.eTokenName == TOK_NEWLINE || 
			(psTokenEntry->psNext->psNext && 
			psTokenEntry->psNext->sToken.eTokenName == TOK_TAB && 
			psTokenEntry->psNext->psNext->sToken.eTokenName == TOK_NEWLINE))))
		{
			LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "Syntax error, #if[n]def expected newline after identifier\n");
			return IMG_FALSE;
		}

		/* Check it has been defined */
		bIfResult = FindSymbol(psSymbolTable,
							   psTokenEntry->sToken.pvData,
							   IMG_NULL,
							   IMG_TRUE);

		if (bInvertResult)
		{
			bIfResult = (IMG_BOOL)(!bIfResult);
		}

		psIfStatus->uNestedIfLevel++;

#ifdef DUMP_LOGFILES
	DumpLogMessage(LOGFILE_PREPROCESSOR,
				   0,
				   "#if[n]def  %s (%d:%d:%d)\n",
				   bIfResult ? "TRUE" : "FALSE",
				   psTokenEntry->sToken.uStringNumber,
				   psTokenEntry->sToken.uLineNumber,
				   psTokenEntry->sToken.uCharNumber);
#endif


		/* Check we haven't exceeded maximum nesting level */
		if (psIfStatus->uNestedIfLevel >= MAX_NESTED_IF_LEVELS)
		{
			LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "Max number of nested #if's is 255\n");
			return IMG_FALSE;
		}

		if (bIfResult)
		{
			psIfStatus->aeIfBlockStates[psIfStatus->uNestedIfLevel] = IFSTATE_BLOCK_IF_ENABLED;
		}
		else
		{
			psIfStatus->aeIfBlockStates[psIfStatus->uNestedIfLevel] = IFSTATE_BLOCK_IF_DISABLED;
		}

		/* Only update the current active #if status if we're inside an enabled block */
		if (BLOCK_ENABLED(psIfStatus->aeIfBlockStates[psIfStatus->uCurrentActiveLevel]))
		{
			psIfStatus->uCurrentActiveLevel = psIfStatus->uNestedIfLevel;
		}
	}
	else
	{
		/* Just keep a count so we can match the endifs */
		psIfStatus->uNestedIfLevel++;

#ifdef DUMP_LOGFILES
	DumpLogMessage(LOGFILE_PREPROCESSOR,
				   0,
				   "#if[n]def  not processed (inside disabled block) (%d:%d:%d)\n",
				   psTokenEntry->sToken.uStringNumber,
				   psTokenEntry->sToken.uLineNumber,
				   psTokenEntry->sToken.uCharNumber);
#endif

	}

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: PPProcessEndif
 *
 * Inputs       : psTokenEntry      - First token to process
 *                psLastTokenEntry  - Last token to process
 *                psIfStatus        - NFI.
 *
 * Outputs      : NFI.
 * Returns      : 
 * Globals Used : -
 *
 * Description  : NFI.
 *****************************************************************************/
static IMG_BOOL PPProcessEndif(GLSLCompilerPrivateData *psCPD, TokenLL  *psTokenEntry,
							TokenLL  *psLastTokenEntry,
							IfStatus *psIfStatus)
{
	PVR_UNREFERENCED_PARAMETER(psLastTokenEntry);

#ifdef DUMP_LOGFILES
	DumpLogMessage(LOGFILE_PREPROCESSOR,
				   0,
				   "#endif (%d:%d:%d)\n",
				   psTokenEntry->sToken.uStringNumber,
				   psTokenEntry->sToken.uLineNumber,
				   psTokenEntry->sToken.uCharNumber);
#endif


	if (!psIfStatus->uNestedIfLevel)
	{
		LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "Syntax error, #endif mismatch\n");
		return IMG_FALSE;
	}

	/* Reduce the currently active level if it's the same as the current level */
	if (psIfStatus->uNestedIfLevel == psIfStatus->uCurrentActiveLevel)
	{
		psIfStatus->uCurrentActiveLevel--;
	}

	/* Reduce nested status */
	psIfStatus->uNestedIfLevel--;

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: PPProcessError
 *
 * Inputs       : psTokenEntry     - 
 *                psLastTokenEntry - 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : NFI.
 *****************************************************************************/
static IMG_BOOL PPProcessError(GLSLCompilerPrivateData *psCPD, TokenLL *psTokenEntry,
							TokenLL *psLastTokenEntry)
{
	IMG_CHAR *pszErrorMessage = IMG_NULL;

	TokenLL *psCurrentEntry = PPGetNextDirectiveTokenEntry(psTokenEntry, psLastTokenEntry);

	/* Get a pointer to the first char of the message */
	if (psCurrentEntry)
	{
		Token *psFirstToken = &(psCurrentEntry->sToken);
		Token *psLastToken  = &(psLastTokenEntry->sToken);

		IMG_CHAR *pszStartOfErrorMessage = &(psFirstToken->pszStartOfLine[psFirstToken->uCharNumber]);
		IMG_CHAR *pszEndOfErrorMessage   = &(psLastToken->pszStartOfLine[psLastToken->uCharNumber]);

		/**
		 * OGL64 Review.
		 * Use size_t for strlen?
		 */
		IMG_UINT32 uErrorMessageSize = (IMG_UINT32)(pszEndOfErrorMessage - pszStartOfErrorMessage);

		pszErrorMessage = DebugMemAlloc(sizeof(IMG_CHAR) * (uErrorMessageSize + 1));
		if(!pszErrorMessage)
		{
			return IMG_FALSE;
		}

		memcpy(pszErrorMessage, pszStartOfErrorMessage, uErrorMessageSize);
		pszErrorMessage[uErrorMessageSize] = '\0';

		LogProgramTokenError(psCPD->psErrorLog, &(psCurrentEntry->sToken), "%s\n", pszErrorMessage);

		DebugMemFree(pszErrorMessage);
	}
	else
	{
		LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "\n");
	}
	
	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: PPProcessLine
 *
 * Inputs       : psTokMemHeap              - Memory heap where any tokens will be allocated from.
 *                psTokenEntry              - First token in the line
 *                psLastTokenEntry          - Last token in the line
 *                psSymbolTable             - Symbol table of macros
 *                pnLineNumberAdjustment    - Offset to apply to the line number
 *                pnStringNumberAdjustment  - Offset to apply to the string number
 *
 * Outputs      : -
 * Returns      : Success.
 * Globals Used : -
 *
 * Description  : Processes a #line directive.
 *****************************************************************************/
static IMG_BOOL PPProcessLine(GLSLCompilerPrivateData *psCPD, 
							  MemHeap     *psTokMemHeap,
							TokenLL     *psTokenEntry,
							TokenLL     *psLastTokenEntry,
							SymTable    *psSymbolTable,
							IMG_INT32   *pnLineNumberAdjustment,
							IMG_INT32   *pnStringNumberAdjustment)
{
	TokenLL  *psCurrentEntry = PPGetNextDirectiveTokenEntry(psTokenEntry, psLastTokenEntry);

	/* Check for enough tokens */
	if (!psCurrentEntry)
	{
		LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "'' : syntax error (0) #line\n");
		return IMG_FALSE;
	}

	/* Check for macro usage within */
	if (!PPCheckForMacroReplacement(psCPD, psTokMemHeap, psCurrentEntry, psLastTokenEntry, psSymbolTable, &psCurrentEntry, IMG_NULL))
	{
		return IMG_FALSE;
	}

	if (psCurrentEntry->sToken.eTokenName != TOK_INTCONSTANT)
	{
		LogProgramTokenError(psCPD->psErrorLog, &(psCurrentEntry->sToken), "'' : syntax error (1) #line\n");
		return IMG_FALSE;
	}
	else
	{
		*pnLineNumberAdjustment = (strtol(psCurrentEntry->sToken.pvData,0,0) - (psCurrentEntry->sToken.uLineNumber - *pnLineNumberAdjustment)) - 1;
	}
	
	psCurrentEntry = PPGetNextDirectiveTokenEntry(psCurrentEntry, psLastTokenEntry);
	
	if (psCurrentEntry)
	{
		if (psCurrentEntry->sToken.eTokenName != TOK_INTCONSTANT)
		{
			LogProgramTokenError(psCPD->psErrorLog, &(psCurrentEntry->sToken), "'' : syntax error (2) #line\n");
			return IMG_FALSE;
		}
		else
		{
			*pnStringNumberAdjustment = (strtol(psCurrentEntry->sToken.pvData,0,0) - (psCurrentEntry->sToken.uStringNumber - *pnStringNumberAdjustment));
		}
	}

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: PPProcessPragma
 *
 * Inputs       : psContext
 *                psTokenEntry
 *                psLastTokenEntry
 * Outputs      : -
 * Returns      : Success
 * Globals Used : -
 *
 * Description  : Processes a #pragma statement. NFI.
 *****************************************************************************/
static IMG_BOOL PPProcessPragma(GLSLCompilerPrivateData *psCPD, 
								GLSLPreProcessorContext *psContext,
								TokenLL *psTokenEntry,
								TokenLL *psLastTokenEntry)
{
	IMG_CHAR *pszPragmaName;

	TokenLL *psCurrentEntry = PPGetNextDirectiveTokenEntry(psTokenEntry, psLastTokenEntry);

	PVR_UNREFERENCED_PARAMETER(psContext);

	/* Check for enough tokens */
	if (!psCurrentEntry || !POSSIBLE_DEFINE_NAME(&psCurrentEntry->sToken))
	{
		LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "'' : syntax error #pragma (1)\n");
		return IMG_FALSE;
	}

	/* Store the name of the pragma */
	pszPragmaName = psCurrentEntry->sToken.pvData;

	psCurrentEntry = PPGetNextDirectiveTokenEntry(psCurrentEntry, psLastTokenEntry);

	if ((strcmp(pszPragmaName, "optimize") == 0) ||
		(strcmp(pszPragmaName, "debug") == 0))
	{
		IMG_CHAR *pszOnOffStatus;

		if (!psCurrentEntry || psCurrentEntry->sToken.eTokenName != TOK_LEFT_PAREN)
		{
			LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "'' : syntax error #pragma (2)\n");
			return IMG_FALSE;
		}

		psCurrentEntry = PPGetNextDirectiveTokenEntry(psCurrentEntry, psLastTokenEntry);

		if (!psCurrentEntry || !POSSIBLE_DEFINE_NAME(&psCurrentEntry->sToken))
		{
			LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "'' : syntax error #pragma (3)\n");
			return IMG_FALSE;
		}

		pszOnOffStatus = psCurrentEntry->sToken.pvData;
		
		if (strcmp(pszOnOffStatus, "on") == 0)
		{
			/* XXX: do something */
		}
		else if (strcmp(pszOnOffStatus, "off") == 0)
		{
			/* XXX: do something */
		}
		else
		{
			LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "'' : syntax error #pragma (4)\n");
			return IMG_FALSE;
		}

		psCurrentEntry = PPGetNextDirectiveTokenEntry(psCurrentEntry, psLastTokenEntry);

		if (!psCurrentEntry || psCurrentEntry->sToken.eTokenName != TOK_RIGHT_PAREN)
		{
			LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "'' : syntax error #pragma (5)\n");
			return IMG_FALSE;
		}
	}
#if defined(GLSL_ES)
	else if(strcmp(pszPragmaName, "STDGL") == 0)
	{
		IMG_CHAR *pszInvariantStatus;

		if (!psCurrentEntry || !psCurrentEntry->sToken.pvData ||
		    strcmp((IMG_CHAR*)psCurrentEntry->sToken.pvData, "invariant"))
		{
			LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "'' : syntax error #pragma (3)\n");
			return IMG_FALSE;
		}

		psCurrentEntry = PPGetNextDirectiveTokenEntry(psCurrentEntry, psLastTokenEntry);

		if (!psCurrentEntry || psCurrentEntry->sToken.eTokenName != TOK_LEFT_PAREN)
		{
			LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "'' : syntax error #pragma (2)\n");
			return IMG_FALSE;
		}
	
		psCurrentEntry = PPGetNextDirectiveTokenEntry(psCurrentEntry, psLastTokenEntry);

		if (!psCurrentEntry || !POSSIBLE_DEFINE_NAME(&(psCurrentEntry->sToken)))
		{
			LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "'' : syntax error #pragma (3)\n");
			return IMG_FALSE;
		}
		
		pszInvariantStatus = psCurrentEntry->sToken.pvData;

		if (strcmp(pszInvariantStatus, "all") == 0)
		{
			psContext->eEnabledExtensions |= GLSLEXT_OES_INVARIANTALL;
		}
		else
		{
			LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "'' : syntax error #pragma (4)\n");
			return IMG_FALSE;
		}
	}
#endif


	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: CountTokens
 *
 * Inputs       : psFirstEntry     - First token in the list
 *                psLastTokenEntry - Last token in the list
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : NFI.
 *****************************************************************************/
static IMG_UINT32 CountTokens(TokenLL  *psFirstEntry,
							TokenLL  *psLastTokenEntry)
{
	IMG_UINT32 uNumTokens = 0;

	/* Get first token of directive */
	TokenLL *psTokenEntry = psFirstEntry;

	/* Copy all tokens */
	while (psTokenEntry)
	{
		uNumTokens++;

		psTokenEntry = PPGetNextDirectiveTokenEntry(psTokenEntry, psLastTokenEntry);
	}

	return uNumTokens;
}

/******************************************************************************
 * Function Name: ProcessDefineMacro
 *
 * Inputs       : psTokenEntry     - First token in the list
 *                psLastTokenEntry - Last token in the list
 *                psSymbolTable    - Symbol table for macros.
 *                bIsPredefined    - NFI.
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Processes a #define statement. NFI.
 *****************************************************************************/
static IMG_BOOL ProcessDefineMacro(GLSLCompilerPrivateData *psCPD, 
								   TokenLL  *psTokenEntry,
									TokenLL  *psLastTokenEntry,
									SymTable *psSymbolTable,
									IMG_BOOL bIsPredefined)
{
	IMG_UINT32    uNumArgs      = 0;
	IMG_UINT32    uNumArgUsages = 0;
	IMG_UINT32    i;
	IMG_UINT32    uTokenNum     = 0;
	IMG_UINT32    uNumTokens    = 0;
	IMG_UINT32   *puArgumentsToUse, *puArgumentPositions;
	TokenLL      *psCurrentEntry = psTokenEntry;
	Token        *psToken = &(psCurrentEntry->sToken);
	Token        *psTokenList;
	DefineMacro  *psDefineMacro;
	IMG_CHAR    **ppszArgumentNames = IMG_NULL; 
	IMG_CHAR     *pszDefineName;
	IMG_BOOL      bTakesArgs = IMG_FALSE;

	PVR_UNREFERENCED_PARAMETER(psSymbolTable);

	/* Get first entry */
	psCurrentEntry = PPGetNextDirectiveTokenEntry(psTokenEntry, psLastTokenEntry);

	/* Check for enough tokens */
	if (!psCurrentEntry)
	{
		LogProgramTokenError(psCPD->psErrorLog, &(psCurrentEntry->sToken), "Syntax error, no parameter specified for #define\n");
		return IMG_FALSE;
	}

	/* Check for valid token */
	if (!POSSIBLE_DEFINE_NAME(&(psCurrentEntry->sToken)))
	{
		LogProgramTokenError(psCPD->psErrorLog, &(psCurrentEntry->sToken), "Syntax error, invalid name for #define\n");
		return IMG_FALSE;
	}

	/* Store name of define */
	pszDefineName = (IMG_CHAR *)psCurrentEntry->sToken.pvData;

	if(!bIsPredefined)
	{
		if(strncmp(pszDefineName, "GL_", 3) == 0)
		{
			LogProgramTokenError(psCPD->psErrorLog, &(psCurrentEntry->sToken), "Syntax error, cannot #define %s, as GL_ is reserved for predefined macros\n",pszDefineName);
			return IMG_FALSE;
		}
		else if(strstr(pszDefineName, "__") != 0)
		{
			LogProgramTokenError(psCPD->psErrorLog, &(psCurrentEntry->sToken), "Syntax error, cannot #define %s, as __ is reserved for predefined macros\n",pszDefineName);
			return IMG_FALSE;
		}
	}


	/* Check it hasn't been defined already */
	if (FindSymbol(psSymbolTable,
				   pszDefineName,
				   IMG_NULL,
				   IMG_TRUE))
	{
		LogProgramTokenError(psCPD->psErrorLog, &(psCurrentEntry->sToken), "Syntax error, '%s' macro redefinition\n", pszDefineName);
		return IMG_FALSE;
	}

	/* get first entry */
	psCurrentEntry = PPGetNextDirectiveTokenEntry(psCurrentEntry, psLastTokenEntry);

	/* Is is a macro with arguments? */
	if (psCurrentEntry && (psCurrentEntry->sToken.eTokenName == TOK_LEFT_PAREN))
	{
		/* 
		   This is a bit messy:
		   If there is a space betwen the name of the define and the left parenthesis then this is a simple substitution
		   (i.e. one that doesn't take arguments). 
		   
		   #define helloworld1(a) a  
		   #define helloworld2 (a) a

		   helloworld1(1); // this would produce the code: "1;"
		   helloworld2(1); // this is an error, macro does not take any arguments.
		   helloworld2;    // this would produce the code: "(a) a;" 

		   However, all spaces are lost at the token generation stage (they are ignored at every other stage) so the only
		   way we can detect the space is by looking at the character positions of the tokens - like I said, a bit messy!
		*/

		/* Check for a gap between the left paren and identifier */
		if (psCurrentEntry->sToken.uCharNumber - (psCurrentEntry->psPrev->sToken.uCharNumber + (psCurrentEntry->psPrev->sToken.uSizeOfDataInBytes - 1)) == 0)
		{

#ifdef DUMP_LOGFILES
			DumpLogMessage(LOGFILE_PREPROCESSOR, 0, "%s is a macro with arguments\n", pszDefineName);
#endif
			bTakesArgs = IMG_TRUE;

			/* Get next entry */
			psCurrentEntry = PPGetNextDirectiveTokenEntry(psCurrentEntry, psLastTokenEntry);

			/* Check for first argument */
			if (!psCurrentEntry)
			{
				LogProgramTokenError(psCPD->psErrorLog, psToken, "'%s' : Expected macro argument\n", pszDefineName);
				return IMG_FALSE;	
			}

			/* If it's not an empty argument list then process the arguments */
			if (psCurrentEntry->sToken.eTokenName != TOK_RIGHT_PAREN)
			{
				/* Do a rough calc to see how much space to allocate for the argument names */
				uNumTokens = CountTokens(psCurrentEntry, psLastTokenEntry);

				/* Allocate space for the arguments */
				ppszArgumentNames = DebugMemAlloc(uNumTokens * sizeof (IMG_CHAR*)); 

				if(uNumTokens && !ppszArgumentNames)
				{
					return IMG_FALSE;
				}

				/* Search through and extract all the arguments */
				while (psCurrentEntry)
				{
					/* Check next token */
					if (!POSSIBLE_DEFINE_NAME(&psCurrentEntry->sToken))
					{
						LogProgramTokenError(psCPD->psErrorLog, psToken, "'%s' : Expected macro argument\n", pszDefineName);
						DebugMemFree(ppszArgumentNames);
						return IMG_FALSE;
					}

					psToken = &psCurrentEntry->sToken;

					/* Copy the argument name */
					ppszArgumentNames[uNumArgs] = psToken->pvData;

					uNumArgs++;

					/* Get next entry */
					psCurrentEntry = PPGetNextDirectiveTokenEntry(psCurrentEntry, psLastTokenEntry);

					/* Check next token */
					if (!psCurrentEntry)
					{
						LogProgramTokenError(psCPD->psErrorLog, psToken, "'%s' : Expected comma or bracket\n", pszDefineName);
						DebugMemFree(ppszArgumentNames);
						return IMG_FALSE;
					}

					psToken = &psCurrentEntry->sToken;

					/* Check for comma or bracket */
					if (psToken->eTokenName == TOK_RIGHT_PAREN)
					{
						break;
					}
					else if (psToken->eTokenName == TOK_COMMA)
					{
						/* Get next entry */
						psCurrentEntry = PPGetNextDirectiveTokenEntry(psCurrentEntry, psLastTokenEntry);
					}
					else
					{
						LogProgramTokenError(psCPD->psErrorLog, psToken, "'%s' : Expected comma or bracket\n", pszDefineName);
						DebugMemFree(ppszArgumentNames);
						return IMG_FALSE;
					}
				}

				/* Check for correct termination */
				if (psToken->eTokenName != TOK_RIGHT_PAREN)
				{
					LogProgramTokenError(psCPD->psErrorLog, psToken, "'%s' : Expected macro argument\n", pszDefineName);
					DebugMemFree(ppszArgumentNames);
					return IMG_FALSE;
				}

			}

			/* Get first token of the macro */
			psCurrentEntry = PPGetNextDirectiveTokenEntry(psCurrentEntry, psLastTokenEntry);
		}
	}

	/* Count the rest of the tokens left */
	uNumTokens = CountTokens(psCurrentEntry, psLastTokenEntry);

	/* Allocate memory for the aguments to use index (allocates max possible size - a little bit wasteful) */ 
	puArgumentsToUse    = DebugMemAlloc(uNumTokens * sizeof(IMG_UINT32));

	if(uNumTokens && !puArgumentsToUse)
	{
		return IMG_FALSE;
	}

	/* Allocate memory for the arguments positions (allocates max possible size - a little bit wasteful) */
	puArgumentPositions = DebugMemAlloc(uNumTokens * sizeof(IMG_UINT32));

	if(uNumTokens && !puArgumentPositions)
	{
		DebugMemFree(puArgumentsToUse);
		return IMG_FALSE;
	}

	/* Allocate memory for the tokens */
	psTokenList         = DebugMemAlloc(uNumTokens * sizeof(Token));

	if(uNumTokens && !psTokenList)
	{
		DebugMemFree(puArgumentsToUse);
		DebugMemFree(puArgumentPositions);
		return IMG_FALSE;
	}


#ifdef DUMP_LOGFILES
	DumpLogMessage(LOGFILE_PREPROCESSOR,
				   0,
				   "Storing %d token(s) for #define '%s' (%d:%d:%d)\n",
				   uNumTokens,
				   pszDefineName,
				   psToken->uStringNumber,
				   psToken->uLineNumber,
				   psToken->uCharNumber);
#endif

	/* Scan the rest of the tokens looking for usages of these arguments */
	while (psCurrentEntry)
	{
		Token *psNewToken = &psTokenList[uTokenNum];
	
		psToken = &psCurrentEntry->sToken;

		/* Is this an identifier */
		if (POSSIBLE_DEFINE_NAME(psToken))
		{
			/* Does it match one of the specified arguments */
			for (i = 0; i < uNumArgs; i++)
			{
				if (strcmp(psToken->pvData, ppszArgumentNames[i]) == 0)
				{
					/* Store this usage */
					puArgumentsToUse[uNumArgUsages] = i;

					puArgumentPositions[uNumArgUsages] = uTokenNum;

					/* Increase the count of the number of times an argument has been used */
					uNumArgUsages++;

					/* Get outta here */
					break;
				}
			}
		}

		/* Copy the token */
		*psNewToken = *psToken;

		/* Copy the data */
		psNewToken->pvData = DebugMemAlloc(psToken->uSizeOfDataInBytes);

		if(psToken->uSizeOfDataInBytes && !psNewToken->pvData)
		{
			return IMG_FALSE;
		}
		memcpy(psNewToken->pvData, psToken->pvData, psToken->uSizeOfDataInBytes);

#ifdef DUMP_LOGFILES
		DumpLogMessage(LOGFILE_PREPROCESSOR, 5, "Storing %d - '%s'\n", uTokenNum, psNewToken->pvData);
#endif

		/* Get the next token */
		psCurrentEntry = PPGetNextDirectiveTokenEntry(psCurrentEntry, psLastTokenEntry);

		uTokenNum++;
	}

#ifdef DUMP_LOGFILES
	for (i = 0; i < uNumArgUsages; i++)
	{
		DumpLogMessage(LOGFILE_PREPROCESSOR,
					   10,
					   "Argument %d ('%s') is used at position %d/%d\n",
					   puArgumentsToUse[i],
					   ppszArgumentNames[puArgumentsToUse[i]],
					   puArgumentPositions[i],
					   uTokenNum-1);
	}
#endif

	/* Allocate memory for the macro structure */
	psDefineMacro                      = DebugMemAlloc(sizeof(DefineMacro));

	if(!psDefineMacro)
	{
		return IMG_FALSE;
	}

	psDefineMacro->ePPSymbolType       = PPST_DEFINEMACRO;
	psDefineMacro->bTakesArgs          = bTakesArgs;
	psDefineMacro->uNumArgs            = uNumArgs;
	psDefineMacro->uNumArgUsages       = uNumArgUsages;
	psDefineMacro->puArgumentPositions = puArgumentPositions;
	psDefineMacro->puArgumentToUse     = puArgumentsToUse;
	psDefineMacro->psTokenList         = psTokenList;
	psDefineMacro->uNumTokens          = uTokenNum;


	/* Free mem */
	DebugMemFree(ppszArgumentNames);

	/* Add this to the symbol table */
	if (!AddSymbol(psSymbolTable,
				   pszDefineName,
				   psDefineMacro,
				   sizeof(DefineMacro),
				   IMG_FALSE,
				   IMG_NULL,
				   PPFreeDefineMacro))
	{
		LOG_INTERNAL_TOKEN_ERROR((&(psCurrentEntry->sToken),"ProcessDefine: Failed to add '%s' to symbol table", pszDefineName));
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ProcessUndef
 *
 * Inputs       : psTokenEntry     - First token in the list
 *                psLastTokenEntry - Last token in the list.
 *                psSymbolTable    - Symbol table for macros
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static IMG_BOOL ProcessUndef(GLSLCompilerPrivateData *psCPD, 
							 TokenLL  *psTokenEntry,
							TokenLL  *psLastTokenEntry,
							SymTable *psSymbolTable)
{
	IMG_UINT32 uSymbolID;
	IMG_CHAR *pszDefineName;

	psTokenEntry = PPGetNextDirectiveTokenEntry(psTokenEntry, psLastTokenEntry);

	/* Check for enough tokens */
	if (!psTokenEntry)
	{
		LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "Syntax error, no parameter specified for #undef\n");
		return IMG_FALSE;
	}

	if (!POSSIBLE_DEFINE_NAME(&psTokenEntry->sToken))
	{
		LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "Syntax error, invalid name for #undef\n");
		return IMG_FALSE;
	}

	/* Store name of define */
	pszDefineName = (IMG_CHAR *)psTokenEntry->sToken.pvData;

	if(strncmp(pszDefineName, "GL_", 3) == 0)
	{
		LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "Syntax error, cannot #undef %s, as GL_ is reserved for predefined macros\n",pszDefineName);
		return IMG_FALSE;
	}
	else if(strstr(pszDefineName, "__") != 0)
	{
		LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "Syntax error, cannot #undef %s, as __ is reserved for predefined macros\n",pszDefineName);
		return IMG_FALSE;
	}


	/* Check it has been defined already */
	if (!FindSymbol(psSymbolTable,
					psTokenEntry->sToken.pvData,
					&uSymbolID,
					IMG_TRUE))
	{
		/* GLSL allows a shader to #undef an undefined identifier, but in this 
		   case there is nothing to remove from the symbol table */
		return IMG_TRUE;
	}

#ifdef DUMP_LOGFILES
	DumpLogMessage(LOGFILE_PREPROCESSOR, 0, "Removing #define of '%s'\n", psTokenEntry->sToken.pvData);
#endif

	/* Remove the symbol */
	if (!RemoveSymbol(psSymbolTable, uSymbolID))
	{
		LOG_INTERNAL_TOKEN_ERROR((&psTokenEntry->sToken,"ProcessUndef: Failed to removed symbol '%s'\n", psTokenEntry->sToken.pvData));
		return IMG_FALSE;
	}

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: PPProcessVersion
 *
 * Inputs       : psContext
 *                psTokMemHeap
 *                psTokenEntry
 *                psLastTokenEntry
 * Outputs      : -
 * Returns      : Success.
 * Globals Used : -
 *
 * Description  : Change the language version of the source code.
 *****************************************************************************/
static IMG_BOOL PPProcessVersion(GLSLCompilerPrivateData *psCPD, 
								 GLSLPreProcessorContext *psContext,
								MemHeap                 *psTokMemHeap,
								TokenLL                 *psTokenEntry,
								TokenLL                 *psLastTokenEntry)
{
	IMG_UINT32 uVersion, i;
	IMG_BOOL   bMatchedVersion = IMG_FALSE;

	/* Back track beyond '#version' */
	TokenLL *psFirstEntry   = psTokenEntry;

	TokenLL *psCurrentEntry = psTokenEntry->psPrev->psPrev;

	/* Check this is the first thing to appear */
	while (psCurrentEntry)
	{
		if ((psCurrentEntry->sToken.eTokenName != TOK_NEWLINE) &&
			(psCurrentEntry->sToken.eTokenName != TOK_CARRIGE_RETURN) &&
			(psCurrentEntry->sToken.eTokenName != TOK_COMMENT) &&
			(psCurrentEntry->sToken.eTokenName != TOK_TAB) &&
			(psCurrentEntry->sToken.eTokenName != TOK_VTAB) &&
			(psCurrentEntry->sToken.eTokenName != TOK_INCOMMENT) &&
			(psCurrentEntry->sToken.eTokenName != TOK_OUTCOMMENT) &&
			(psCurrentEntry->sToken.eTokenName != TOK_ENDOFSTRING) &&
			(psCurrentEntry->sToken.eTokenName != TOK_TERMINATEPARSING))
		{
			LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "#version must occur before any other statement in the program\n");
			return IMG_FALSE;
		}

		psCurrentEntry = psCurrentEntry->psPrev;
	}

	psTokenEntry = PPGetNextDirectiveTokenEntry(psTokenEntry, psLastTokenEntry);

	/* Check for enough tokens */
	if (!psTokenEntry)
	{
		LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "Syntax error, no parameter specified for #version\n");
		return IMG_FALSE;
	}

	if (psTokenEntry->sToken.eTokenName != TOK_INTCONSTANT)
	{
		LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "Syntax error, invalid parameter supplied for #version\n");
		return IMG_FALSE;
	}

	uVersion = strtol(psTokenEntry->sToken.pvData,0,0);

	for (i = 0; i < GLSL_NUM_VERSIONS_SUPPORTED; i++)
	{
		if (guSupportedGLSLVersions[i] == uVersion)
		{
			bMatchedVersion = IMG_TRUE;
			psContext->uSupportedVersion = uVersion;
			break;
		}
	}

	/* Insert tokens to indicate a language version change has taken place */
	if (bMatchedVersion)
	{
		Token     asToken;

		/* Inherit most of the info */
		asToken = psFirstEntry->sToken;
		asToken.eTokenName         = TOK_LANGUAGE_VERSION;
		asToken.pvData             = IMG_NULL;
		asToken.uSizeOfDataInBytes = 0;

		/* Hack: piggyback on pszStartOfLine to store the language version */
		/**
		 * OGL64 Review.
		 * ...?
		 */
		asToken.pszStartOfLine = (IMG_CHAR*)(IMG_SIZE_T)(uVersion);

		/* Insert a version change token into the list */
		PPInsertTokensIntoLinkedList(psTokMemHeap, psTokenEntry, &asToken, 1, IMG_NULL);

#ifdef DUMP_LOGFILES
		DumpLogMessage(LOGFILE_PREPROCESSOR, 0, "Inserting language version change tokens (new version = %d)\n", uVersion);
#endif

	}
	else
	{
		LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "Syntax error, GLSL Version %u not supported\n", uVersion);
	}

	return bMatchedVersion;
}

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : FIXME: This should be split with blocks of #ifdef ENABLE_EXT_BLAH kind of blocks.
 *****************************************************************************/
static GLSLExtension PPIsExtensionSupported(IMG_CHAR *pszExtensionName)
{
#if defined(GLSL_ES)

	/* Not currently supporting 3D textures */
	if(strcmp(pszExtensionName, "GL_OES_texture_3D") == 0)
		return GLSLEXT_NONE;
	
	/* Not currently supporting noise */
	if(strcmp(pszExtensionName, "GL_OES_standard_noise") == 0)
		return GLSLEXT_NONE;

	if(strcmp(pszExtensionName, "GL_OES_standard_derivatives") == 0)
		return GLSLEXT_OES_STANDARD_DERIVATIVES;

	if(strcmp(pszExtensionName, "GL_IMG_texture_stream2") == 0)
		return GLSLEXT_IMG_TEXTURE_STREAM;

	if(strcmp(pszExtensionName, "GL_EXT_shader_texture_lod") == 0)
		return GLSLEXT_EXT_TEXTURE_LOD;

	if(strcmp(pszExtensionName, "GL_OES_EGL_image_external") == 0)
		return GLSLEXT_OES_TEXTURE_EXTERNAL;

#else /* !defined GLSL_ES */
	if(strcmp(pszExtensionName, "GL_IMG_precision") == 0)
		return GLSLEXT_IMG_PRECISION;

	if(strcmp(pszExtensionName, "GL_IMG_texture_stream") == 0)
		return GLSLEXT_IMG_TEXTURE_STREAM;

#if defined(SUPPORT_GL_TEXTURE_RECTANGLE)
	if(strcmp(pszExtensionName, "GL_ARB_texture_rectangle") == 0)
		return GLSLEXT_ARB_TEXTURE_RECTANGLE;
#endif

#endif /* defined GLSL_ES */

	return GLSLEXT_NONE;
}

/******************************************************************************
 * Function Name: PPIsExtensionCombinationSupported
 *
 * Inputs       : eCurrentEnables, eNewExtension
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Allows combinations of extensions to be rejected
 *****************************************************************************/

static IMG_BOOL PPIsExtensionCombinationSupported(GLSLExtension eCurrentEnables, GLSLExtension eNewExtension, const IMG_CHAR **ppszBadCombo)
{
#if defined(GLSL_ES)
	if(eNewExtension == GLSLEXT_IMG_TEXTURE_STREAM)
	{
		if(eCurrentEnables & GLSLEXT_OES_TEXTURE_EXTERNAL)
		{
			const IMG_CHAR *pszEGLImageExternal = "GL_OES_EGL_image_external";
				
			*ppszBadCombo = pszEGLImageExternal;
			return IMG_FALSE;
		}
			
	}

	if(eNewExtension == GLSLEXT_OES_TEXTURE_EXTERNAL)
	{
		if(eCurrentEnables & GLSLEXT_IMG_TEXTURE_STREAM)
		{
			const IMG_CHAR *pszIMGStream = "GL_IMG_texture_stream2";
			*ppszBadCombo = pszIMGStream;
			return IMG_FALSE;
		}
	}
#endif /* defined GLSL_ES */

	return IMG_TRUE;

}

/******************************************************************************
 * Function Name: PPProcessExtension
 *
 * Inputs       : psContext
 *                psTokMemHeap
 *                psTokenEntry
 *                psLastTokenEntry
 * Outputs      : -
 * Returns      : Success
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static IMG_BOOL PPProcessExtension(GLSLCompilerPrivateData *psCPD, 
								   GLSLPreProcessorContext *psContext,
								MemHeap                 *psTokMemHeap,
								TokenLL *psTokenEntry,
								TokenLL *psLastTokenEntry)
{
	Token     acTokens[2];
	IMG_CHAR *pszExtensionName, *pszBehaviourName;

	psTokenEntry = PPGetNextDirectiveTokenEntry(psTokenEntry, psLastTokenEntry);

	/* Check for enough tokens */
	if (!psTokenEntry || !psTokenEntry->sToken.pvData)
	{
		LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "'' :  extension name not specified\n");
		return IMG_FALSE;
	}

	pszExtensionName = psTokenEntry->sToken.pvData;

	psTokenEntry = PPGetNextDirectiveTokenEntry(psTokenEntry, psLastTokenEntry);

	/* Check for enough tokens */
	if (!psTokenEntry || psTokenEntry->sToken.eTokenName != TOK_COLON)
	{
		LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "'' :  ':' missing after extension name\n");
		return IMG_FALSE;
	}

	psTokenEntry = PPGetNextDirectiveTokenEntry(psTokenEntry, psLastTokenEntry);

	if (!psTokenEntry || !psTokenEntry->sToken.pvData)
	{
		LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "'' :  behaviour for extension not specified\n");
		return IMG_FALSE;
	}

	pszBehaviourName = psTokenEntry->sToken.pvData;

	if (strcmp(pszBehaviourName, "require") == 0)
	{
		GLSLExtension eExtensionEnable;

		if (strcmp(pszExtensionName, "all") == 0)
		{
ExtensionAllError:
			LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "'' : extension 'all' cannot have 'require' or 'enable' behaviour\n");
			return IMG_FALSE;
		}

		eExtensionEnable = PPIsExtensionSupported(pszExtensionName);

		if (eExtensionEnable == GLSLEXT_NONE)
		{
			LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "Extension %s not supported\n", pszExtensionName);
			return IMG_FALSE;
		}
		else
		{
			const IMG_CHAR *pszBadCombo = IMG_NULL;

			if(PPIsExtensionCombinationSupported(psContext->eEnabledExtensions, eExtensionEnable, &pszBadCombo))
			{
				psContext->eEnabledExtensions |= eExtensionEnable;
			}
			else
			{
				LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "Extension %s not supported in combination with %s\n", 
					pszExtensionName, pszBadCombo);
			}
		}
	}
	else if (strcmp(pszBehaviourName, "enable") == 0)
	{
		GLSLExtension eExtensionEnable;

		if (strcmp(pszExtensionName, "all") == 0)
		{
			goto ExtensionAllError;
		}

		eExtensionEnable = PPIsExtensionSupported(pszExtensionName);

		if(eExtensionEnable == GLSLEXT_NONE)
		{
ExtensionNotSupportedWarning:
			LogProgramTokenWarning(psCPD->psErrorLog, &psTokenEntry->sToken, "Extension %s not supported\n", pszExtensionName);
			return IMG_TRUE;
		}
		else
		{
			const IMG_CHAR *pszBadCombo = IMG_NULL;

			if(PPIsExtensionCombinationSupported(psContext->eEnabledExtensions, eExtensionEnable, &pszBadCombo))
			{
				psContext->eEnabledExtensions |= eExtensionEnable;
			}
			else
			{
				LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "Extension %s not supported in combination with %s\n", 
					pszExtensionName, pszBadCombo);
			}
		}
	}
	else if (strcmp(pszBehaviourName, "warn") == 0)
	{
		if (strcmp(pszExtensionName, "all") == 0)
		{
			/* ??? do something? */
		}
		else if (!PPIsExtensionSupported(pszExtensionName))
		{
			goto ExtensionNotSupportedWarning;
		}
	}
	else if (strcmp(pszBehaviourName, "disable") == 0)
	{
		if (strcmp(pszExtensionName, "all") == 0)
		{
			/* ??? do something? */
		}
		else
		{
			GLSLExtension eExtensionEnable = PPIsExtensionSupported(pszExtensionName);

			if(eExtensionEnable == GLSLEXT_NONE)
			{
				goto ExtensionNotSupportedWarning;
			}
			else
			{
				psContext->eEnabledExtensions &= ~eExtensionEnable;
			}
		}
	}
	else
	{
		LogProgramTokenError(psCPD->psErrorLog, &psTokenEntry->sToken, "'' :  behaviour '%s' is not supported\n", pszBehaviourName);
		return IMG_FALSE;
	}

	/* !!! Inserting tokens? Questionable design decision */
	/* Insert tokens to indicate that an extension change has taken place */
	memset(acTokens, 0, 2*sizeof(Token));
	acTokens[0].eTokenName = TOK_EXTENSION_CHANGE;
	acTokens[1].eTokenName = TOK_INTCONSTANT;
	/*
	   Hack: piggyback on pszStartOfLine to store a bitmask.
	*/
	/**
	 * OGL64 Review.
	 * ...what is going on here?
	 */
	acTokens[1].pszStartOfLine = (IMG_CHAR*)(IMG_SIZE_T)psContext->eEnabledExtensions;

	/* Insert some version change tokens into the list */
	PPInsertTokensIntoLinkedList(psTokMemHeap, psTokenEntry, acTokens, 2, IMG_NULL);

#ifdef DUMP_LOGFILES
	DumpLogMessage(LOGFILE_PREPROCESSOR, 0, "Inserting extension change tokens (new extensions = 0x%X)\n", psContext->eEnabledExtensions);
#endif
	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: PPProcessDirective
 *
 * Inputs       : psContext
 *                psTokMemHeap
 *                psFirstTokenEntry
 *                psLastTokenEntry
 *                psSymbolTable
 *                psIfStatus
 *                pnLineNumberAdjustment
 *                pnStringNumberAdjustment
 * Outputs      : -
 * Returns      : Success
 * Globals Used : -
 *
 * Description  : ???
 *****************************************************************************/
static IMG_BOOL PPProcessDirective(GLSLCompilerPrivateData *psCPD, 
								   GLSLPreProcessorContext *psContext,
								MemHeap     *psTokMemHeap,
								TokenLL     *psFirstTokenEntry,
								TokenLL     *psLastTokenEntry,
								SymTable    *psSymbolTable,
								IfStatus    *psIfStatus,
								IMG_INT32   *pnLineNumberAdjustment,
								IMG_INT32   *pnStringNumberAdjustment)
{
	IMG_BOOL bSuccess = IMG_TRUE;

	Token   *psToken;

	/* Get the next token from the directive */
	TokenLL *psTokenEntry = PPGetNextDirectiveTokenEntry(psFirstTokenEntry, psLastTokenEntry);

	/* Check for no preprocessor commands */
	if (!psTokenEntry)
	{
		return IMG_TRUE;
	}

	/* Get the token */
	psToken = &psTokenEntry->sToken;

	switch (psToken->eTokenName)
	{
		case TOK_IDENTIFIER:
		{
			if(!strcmp(psToken->pvData, "if"))
			{
				bSuccess = PPProcessIf(psCPD, psTokMemHeap, psTokenEntry, psLastTokenEntry, psSymbolTable, psIfStatus);
			}
			else if(!strcmp(psToken->pvData, "else"))
			{
				bSuccess = PPProcessElse(psCPD, psTokenEntry, psLastTokenEntry, psIfStatus);
			}
			else if (strcmp(psToken->pvData, "ifdef") == 0)
			{
				bSuccess = PPProcessIfdef(psCPD, psTokenEntry, psLastTokenEntry, psSymbolTable, psIfStatus, IMG_FALSE);
			}
			else if (strcmp(psToken->pvData, "ifndef") == 0)
			{
				bSuccess = PPProcessIfdef(psCPD, psTokenEntry, psLastTokenEntry, psSymbolTable, psIfStatus, IMG_TRUE);
			}
			else if (strcmp(psToken->pvData, "elif") == 0)
			{
				bSuccess = PPProcessElif(psCPD, psTokMemHeap, psTokenEntry, psLastTokenEntry, psSymbolTable, psIfStatus);
			}
			else if (strcmp(psToken->pvData, "endif") == 0)
			{
				bSuccess = PPProcessEndif(psCPD, psTokenEntry, psLastTokenEntry, psIfStatus);
			}
			else if (BLOCK_ENABLED(psIfStatus->aeIfBlockStates[psIfStatus->uCurrentActiveLevel]))
			{
				if (strcmp(psToken->pvData, "define") == 0)
				{
					bSuccess = ProcessDefineMacro(psCPD, psTokenEntry, psLastTokenEntry, psSymbolTable, IMG_FALSE);
				}
				else if (strcmp(psToken->pvData, "undef") == 0)
				{
					bSuccess = ProcessUndef(psCPD, psTokenEntry, psLastTokenEntry, psSymbolTable);
				}
				else if (strcmp(psToken->pvData, "error") == 0)
				{
					bSuccess = PPProcessError(psCPD, psTokenEntry, psLastTokenEntry);
				}
				else if (strcmp(psToken->pvData, "line") == 0)
				{
					bSuccess = PPProcessLine(psCPD, psTokMemHeap, psTokenEntry, psLastTokenEntry, psSymbolTable, pnLineNumberAdjustment, pnStringNumberAdjustment);
				}
				else if (strcmp(psToken->pvData, "pragma") == 0)
				{
					bSuccess = PPProcessPragma(psCPD, psContext, psTokenEntry, psLastTokenEntry);
				}
				else if (strcmp(psToken->pvData, "extension") == 0)
				{
					bSuccess = PPProcessExtension(psCPD, psContext, psTokMemHeap, psTokenEntry, psLastTokenEntry);
				}
				else if (strcmp(psToken->pvData, "version") == 0)
				{
					bSuccess = PPProcessVersion(psCPD, psContext, psTokMemHeap, psTokenEntry, psLastTokenEntry);
				}
				else
				{
					LogProgramTokenError(psCPD->psErrorLog, psToken, "'' :   Invalid directive %s\n", psToken->pvData);
					return IMG_FALSE;
				}
			}

			break;
		}
		default:
		{
			if (psToken->pvData)
			{
				LogProgramTokenError(psCPD->psErrorLog, psToken, "'' :   Invalid directive %s\n", psToken->pvData);
			}
			else
			{
				LogProgramTokenError(psCPD->psErrorLog, psToken, "'' :   Invalid directive\n");
			}
			return IMG_FALSE;
		}
	}

	return bSuccess;
}


#if defined(GLSL_ES)
/******************************************************************************
 * Function Name: PPSetDefaultDefine
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Used to set some default defines
 *****************************************************************************/
static IMG_BOOL PPSetDefaultDefine(GLSLCompilerPrivateData *psCPD,
								   IMG_CHAR *pszDefineName,
											IMG_CHAR *pszDefineValue,
											SymTable *psSymbolTable)
{
	IMG_UINT32 uLineLength = strlen(pszDefineName) + strlen(pszDefineValue) + 20;

	IMG_CHAR *pszLine = DebugMemAlloc(uLineLength);

	IMG_BOOL bSuccess;

	TokenLL asTokenLL[3] = {{{0}}};
	Token   *psToken;

	/* Construct a line */
	if(!pszLine)
	{
		return IMG_FALSE;
	}
	sprintf(pszLine, "#define %s %s\n", pszDefineName, pszDefineValue);

	asTokenLL[0].psPrev  = IMG_NULL; 
	asTokenLL[0].psNext  = &asTokenLL[1];
	asTokenLL[0].bRemove = IMG_TRUE;

	asTokenLL[1].psPrev  = &asTokenLL[0];
	asTokenLL[1].psNext  = &asTokenLL[2];
	asTokenLL[1].bRemove = IMG_TRUE;

	asTokenLL[2].psPrev  = &asTokenLL[0];
	asTokenLL[2].psNext  = IMG_NULL;
	asTokenLL[2].bRemove = IMG_TRUE;

	/* Build the token list */
	psToken = &asTokenLL[1].sToken;

	psToken->eTokenName         = TOK_IDENTIFIER;
	psToken->pszStartOfLine     = pszLine;
	psToken->uStringNumber      = 0;
	psToken->uLineNumber        = 0;
	psToken->uCharNumber        = 8;
	psToken->uSizeOfDataInBytes = strlen(pszDefineName);
	psToken->pvData             = (IMG_VOID *)pszDefineName;

	psToken = &(asTokenLL[2].sToken);

	psToken->eTokenName         = TOK_INTCONSTANT;
	psToken->pszStartOfLine     = pszLine;
	psToken->uStringNumber      = 0;
	psToken->uLineNumber        = 0;
	psToken->uCharNumber        = 14;
	psToken->uSizeOfDataInBytes = strlen(pszDefineValue);
	psToken->pvData             = (IMG_VOID *)pszDefineValue;

	/* Add the define */
	bSuccess = ProcessDefineMacro(psCPD, &asTokenLL[0], &asTokenLL[2], psSymbolTable, IMG_TRUE);

	DebugMemFree(pszLine);

	return bSuccess;
}
#endif

/******************************************************************************
 * Function Name: PPPreProcessTokenList
 *
 * Inputs       : pvData          - PreProcessor data (see GLSLPreProcessorData)
 *                psTokenList     - Array of tokens to preprocess
 *                uNumTokens      - Length of the array of tokens
 *                
 * Outputs      : ppsNewTokenList - Array of tokens after preprocessing
 *                puNewNumTokens  - Length of the array of tokens after preprocessing
 *
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise
 * Globals Used : -
 *
 * Description  : Aplies the GLSL preprocessor to an array of tokens. Generates
 *                an array of preprocessed tokens.
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL PPPreProcessTokenList(GLSLCompilerPrivateData *psCPD,
											IMG_VOID			   *pvData,
										Token                 *psTokenList,
										IMG_UINT32             uNumTokens,
										Token                **ppsNewTokenList,
										IMG_UINT32            *puNewNumTokens)

{
	GLSLPreProcessorData **ppsData = (GLSLPreProcessorData **)pvData;
	IfStatus            sIfStatus;

	IMG_BOOL            bProcessingDirective    = IMG_FALSE;
	IMG_INT32           nLineNumberAdjustment   = 0;
	IMG_INT32           nStringNumberAdjustment = 0;
	SymbolTableContext *psSymbolTableContext;
	SymTable           *psSymbolTable;  
	GLSLPreProcessorContext sContext = {0};

	TokenLL            *psTokenLL;
	TokenLL            *psCurrentEntry;
	TokenLL            *psFirstTokenOfDirective = IMG_NULL, *psLastBackSlashEntry = IMG_NULL, *psFirstCodeBlockEntry = IMG_NULL;
	MemHeap            *psTokMemHeap;

	IMG_BOOL            bSuccess = IMG_TRUE;

#ifdef DUMP_LOGFILES
	DumpLogMessage(LOGFILE_PREPROCESSOR, 0, "Preprocessor logfile\n");
#endif

	/* Setup the context */
	sContext.uSupportedVersion  = GLSL_DEFAULT_VERSION_SUPPORT;
	sContext.eEnabledExtensions = GLSLEXT_DEFAULT_EXTENSIONS; 
	/* Reset the if status */
	sIfStatus.uNestedIfLevel      = 0;
	sIfStatus.uCurrentActiveLevel = 0;
	sIfStatus.aeIfBlockStates[0]  = IFSTATE_BLOCK_IF_ENABLED;

	/* Create symbol table for managing preprocesser definitions */
	psSymbolTableContext = InitSymbolTableManager();
	psSymbolTable        = CreateSymTable(psSymbolTableContext,
										  "Preprocessor state",
										  100, 
										  GLSL_NUM_BITS_FOR_SYMBOL_IDS, 
										  IMG_NULL);
	if(!psSymbolTable)
		return IMG_FALSE;

	/* 1.2 seems to be a fairly consistant ratio */
	/* Using 1.25 to avoid floating point multiply */
	psTokMemHeap = DebugCreateHeap(sizeof(TokenLL), uNumTokens + (uNumTokens >> 2));

#if defined(GLSL_ES)
	// FIXME: this should be organized with #ifdef ENABLE_EXT_BLAH blocks
	/* Set default preprocessor defines */
	PPSetDefaultDefine(psCPD, "GL_ES", "1", psSymbolTable);
	PPSetDefaultDefine(psCPD, "GL_FRAGMENT_PRECISION_HIGH", "1", psSymbolTable);
	//PPSetDefaultDefine(psCPD, "GL_OES_standard_noise", "1", psSymbolTable);
	PPSetDefaultDefine(psCPD, "GL_OES_standard_derivatives", "1", psSymbolTable);
	PPSetDefaultDefine(psCPD, "GL_EXT_shader_texture_lod", "1", psSymbolTable);
	PPSetDefaultDefine(psCPD, "GL_IMG_texture_stream2", "1", psSymbolTable);

	PPSetDefaultDefine(psCPD, "GL_OES_EGL_image_external", "1", psSymbolTable);
#endif

	/* Create a token linked list from the input array */
	psTokenLL = PPCreateTokenLinkedList(psTokMemHeap, psTokenList, uNumTokens, IMG_NULL);

	/* Get the first entry */
	psCurrentEntry = psTokenLL;

	psFirstCodeBlockEntry = psCurrentEntry;

	/* Search through all tokens */
	while (psCurrentEntry)
	{
		IMG_BOOL bRemoveCurrentToken = IMG_FALSE;

		Token *psToken = &psCurrentEntry->sToken;

		if (bProcessingDirective)
		{
			bRemoveCurrentToken = IMG_TRUE;
		}

		/* Adjust the line number */
		// Commented temporarily: psToken->uLineNumber += nLineNumberAdjustment;
		
		/* Check for string changes before we carry out any line, string number adjustments */
		if (psCurrentEntry->psNext)
		{
			/* If the string has changed then we need to reset any line number adjustments */
			if (psToken->uStringNumber != psCurrentEntry->psNext->sToken.uStringNumber)
			{
				nLineNumberAdjustment = 0;
			}
		}

		/* Adjust the string number */
		psToken->uStringNumber += nStringNumberAdjustment;

		switch (psToken->eTokenName)
		{
			case TOK_HASH:
			{
				psFirstTokenOfDirective = psCurrentEntry;

				bProcessingDirective = IMG_TRUE;

				break;
			}
			case TOK_BACK_SLASH:
			{
				psLastBackSlashEntry = psCurrentEntry;
				
				break;
			}
			case TOK_NEWLINE:
			case TOK_CARRIGE_RETURN:
			case TOK_ENDOFSTRING:
			case TOK_TERMINATEPARSING:
			{
				IMG_BOOL bMultiLineStatement = IMG_FALSE;

				/* If the last token was a back slash then we're dealing with a multi line statement (probably) 

				   However, this is not the only case, during the macro replacement, the call of macro may still happen in multiple lines.  
				   This case cannot be detected by this simple token link list traverse.  This means bMultiLineStatement may give wrong 
				   information for macro replacement.  This error should be corrected during the PPCheckForMacroReplacement.
				 */
				if (psLastBackSlashEntry && (psLastBackSlashEntry == psCurrentEntry->psPrev))
				{
					bMultiLineStatement = IMG_TRUE;
				}

				if (psToken->eTokenName == TOK_ENDOFSTRING || psToken->eTokenName == TOK_TERMINATEPARSING)
				{
					bRemoveCurrentToken = IMG_FALSE;
				}
				
				if (bProcessingDirective)
				{
					if (!bMultiLineStatement)
					{
						/* Process this directive */
						if (!PPProcessDirective(psCPD, &sContext,
											  psTokMemHeap,
											  psFirstTokenOfDirective,
											  psCurrentEntry,
											  psSymbolTable,
											  &sIfStatus,
											  &nLineNumberAdjustment,
											  &nStringNumberAdjustment))
						{
							bSuccess = IMG_FALSE;
						}
						
						bProcessingDirective = IMG_FALSE;

						/* Start a new code block */
						psFirstCodeBlockEntry = psCurrentEntry;
					}
				}
				else 
				{
					if (!bMultiLineStatement)
					{
						/* Check for any usage of defines on this line 

						   This multiline statement information is may wrong, PPCheckForMacroReplacement will correct this wrong judgement 
						   and return the adjustment of psCurrentEntry.
						 */
						TokenLL *psNewLastEntry;
						if (!PPCheckForMacroReplacement(psCPD, psTokMemHeap, psFirstCodeBlockEntry, psCurrentEntry, psSymbolTable, IMG_NULL, &psNewLastEntry))
						{
							bSuccess = IMG_FALSE;
						}
						
						if (psNewLastEntry != IMG_NULL)
						{
							psCurrentEntry = psNewLastEntry;
						}

						/* Start a new code block */
						psFirstCodeBlockEntry = psCurrentEntry;
					}
				}
				
				break;
			}
			case TOK_IDENTIFIER:
			{
				/* Could make this faster by introducing tokens for each of these */
				if (strcmp(psToken->pvData, "__LINE__") == 0)
				{
					/* Don't need to free and reallocate since there should already be enough space */
					sprintf(psToken->pvData, "%u", psToken->uLineNumber);

					psToken->eTokenName = TOK_INTCONSTANT;
				}
				else if (strcmp(psToken->pvData, "__FILE__") == 0)
				{
					/* Don't need to free and reallocate since there should already be enough space */
					sprintf(psToken->pvData, "%u", psToken->uStringNumber);

					psToken->eTokenName = TOK_INTCONSTANT;
				}
				else if (strcmp(psToken->pvData, "__VERSION__") == 0)
				{
					/* Don't need to free and reallocate since there should already be enough space */
					sprintf(psToken->pvData, "%u", sContext.uSupportedVersion);

					psToken->eTokenName = TOK_INTCONSTANT;
				}
			}
			default:
				break;

		}

		if (bProcessingDirective || !BLOCK_ENABLED(sIfStatus.aeIfBlockStates[sIfStatus.uCurrentActiveLevel]))
		{
			bRemoveCurrentToken = IMG_TRUE;
		}

		/* Indicate if token should be removed or not */
		psCurrentEntry->bRemove = bRemoveCurrentToken;

		/* Are there any more tokens to process? */
		if (!psCurrentEntry->psNext)
		{
			if (bProcessingDirective)
			{
				/* Process this directive */
				if (!PPProcessDirective(psCPD, &sContext,
									  psTokMemHeap,
									  psFirstTokenOfDirective,
									  psCurrentEntry,
									  psSymbolTable,
									  &sIfStatus,
									  &nLineNumberAdjustment,
									  &nStringNumberAdjustment))
				{
					bSuccess = IMG_FALSE;
				}
			}
			else
			{
				/* Check for macro replacements withany of the remaining tokens */
				TokenLL *psNewLastEntry;
				if (!PPCheckForMacroReplacement(psCPD, psTokMemHeap, psFirstCodeBlockEntry, psCurrentEntry, psSymbolTable, IMG_NULL, &psNewLastEntry))
				{
					bSuccess = IMG_FALSE;
				}

				if (psNewLastEntry != IMG_NULL)
				{
					psCurrentEntry = psNewLastEntry;
				}
			}
		}

		/* Get next entry */
		psCurrentEntry = psCurrentEntry->psNext;

	}

	if (sIfStatus.uNestedIfLevel)
	{
		LogProgramError(psCPD->psErrorLog, "Premature end of source; preprocessor expected #endif\n");
		bSuccess = IMG_FALSE;
	}

	/* Create the new token list */
	if(bSuccess)
	{
		*ppsNewTokenList = PPCreateTokenList(psTokenLL, IMG_NULL, puNewNumTokens); 
		if(!*ppsNewTokenList) 
			return IMG_FALSE;
	}

	/* Destroy the linked list */
	PPDestroyTokenLinkedList(psTokMemHeap, psTokenLL);

	/* Destroy the heap */
	DebugDestroyHeap(psTokMemHeap);

	RemoveSymbolTableFromManager(psSymbolTableContext, psSymbolTable);
	DestroySymTable(psSymbolTable);

	DestroySymbolTableManager(psSymbolTableContext);

	if(bSuccess)
	{
		GLSLPreProcessorData *psData = DebugMemAlloc(sizeof(GLSLPreProcessorData));

		if(psData)
		{
			psData->eEnabledExtensions = sContext.eEnabledExtensions;
			*ppsData = psData;
		}
		else
		{
			*ppsData = IMG_NULL;
			bSuccess = IMG_FALSE;
		}
	}

	return bSuccess;
}

/******************************************************************************
 * Function Name: PPDestroyPreProcessorData
 *
 * Inputs       : pvData
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL PPDestroyPreProcessorData(IMG_VOID *pvData)
{
	GLSLPreProcessorData *psData = (GLSLPreProcessorData *)pvData;

	if(psData)
	{
		DebugMemFree(psData);
	}
	return IMG_TRUE;
}

/******************************************************************************
 End of file (prepro.c)
******************************************************************************/

