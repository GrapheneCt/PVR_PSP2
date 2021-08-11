/******************************************************************************
 * Name         : lex.c
 * Author       : James McCarthy
 * Created      : 22/01/2004
 *
 * Copyright    : 2000-2008 by Imagination Technologies Limited.
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
 * $Log: lex.c $
 *****************************************************************************/

#include "lex.h"
#include "debug.h"
#include "../glsl/error.h"
#include "../glsl/prepro.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define LEX_LINE_FEED       ((IMG_CHAR)0xA)
#define LEX_CARRIAGE_RETURN ((IMG_CHAR)0xD)


typedef struct LexContextTAG
{
	IMG_UINT32             uCurrentStringNumber;
	IMG_UINT32             uCurrentLineNumber;
	IMG_CHAR            *pszSource;
	IMG_CHAR            *pszCurrentSource;
	IMG_CHAR            *pszStartOfLine;
	IMG_UINT32             uNumTokens;
	Token               *psTokenList;
} LexContext;

#ifdef DUMP_LOGFILES

/******************************************************************************
 * Function Name: DecodeToken
 *
 * Inputs       : LexContext, Token,
 * Outputs      : pDst
 * Returns      : -
 * Globals Used : -
 *
 * Description  : Prints info about the current token
 *****************************************************************************/
static IMG_VOID LexDecodeToken(LexContext *psLexContext, Token *psToken, IMG_CHAR *pDst, IMG_UINT32 uDstSize)
{
	IMG_CHAR acTmp[500] = {'\0'};

	PVR_UNREFERENCED_PARAMETER(psLexContext);

	if (psToken->pvData)
	{
		IMG_CHAR *data = (IMG_CHAR *)(psToken->pvData);

		/* Filter out the newlines (char num == 10) */
		if (*data != 10)
		{
			snprintf(acTmp,(sizeof(acTmp) / sizeof(IMG_CHAR)) - 1,"'%.490s'",data);

		}
		else
		{
			*acTmp = '\0';
		}
	}

		snprintf(pDst,uDstSize,
			 "Token (%3u,%3u) =  %d %s",
			 psToken->uLineNumber,
			 psToken->uCharNumber,
			 psToken->eTokenName,
			 acTmp);

}


/******************************************************************************
 * Function Name: LexDecodeTokenList
 *
 * Inputs       : Lexcontext
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : Decodes the list of tokens and dumps the results to a log file
 *****************************************************************************/
static IMG_VOID LexDecodeTokenList(LexContext *psLexContext)
{
	IMG_UINT32 i;

	/* Quick check to see if it's worth doing the rest */
	if (!LogFileActive(LOGFILE_TOKENS_GENERATED))
	{
		return;
	}

	for (i = 0; i < psLexContext->uNumTokens; i++)
	{
		IMG_CHAR DecodeString[200];

		LexDecodeToken(psLexContext, &(psLexContext->psTokenList[i]), DecodeString, (sizeof(DecodeString) / sizeof(IMG_CHAR)) - 1);

		DumpLogMessage(LOGFILE_TOKENS_GENERATED, 0, "%-4d %s (0x%03X)\n",i,DecodeString,psLexContext->psTokenList[i].eTokenName);
	}
}
#endif

/******************************************************************************
 * Function Name: LexDestroyTokenList 
 *
 * Inputs       : Lexcontext
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : Frees mem associated with the token list
 *****************************************************************************/
IMG_INTERNAL IMG_VOID LexDestroyTokenList(Token *psTokenList, IMG_UINT32 uNumTokens)
{
	IMG_UINT32 i; 

	for (i = 0; i < uNumTokens; i++)
	{
		if (psTokenList[i].pvData)
		{
			DebugMemFree(psTokenList[i].pvData);
		}
	}

	DebugMemFree(psTokenList);
}

/******************************************************************************
 * Function Name: LexCallocToken 
 *
 * Input-output : ppsTokenBase, pu32AllocatedTokens, pu32UsedTokens
 * Returns      : A pointer
 *
 * Description  : Returns a pointer to the first available position in a dinamically-resized array.
 *****************************************************************************/
static Token * LexCallocToken(Token **ppsTokenBase, IMG_UINT32 *pu32AllocatedTokens, IMG_UINT32 *pu32UsedTokens)
{
	Token      *psToken = *ppsTokenBase;
	IMG_UINT32 u32AllocatedTokens = *pu32AllocatedTokens;
	IMG_UINT32 u32UsedTokens = *pu32UsedTokens;

	if(u32UsedTokens >= u32AllocatedTokens)
	{
		/* Increase the allocated space by 25% at a time to keep the insertion time amortized O(1) */
		u32AllocatedTokens += u32AllocatedTokens? (u32AllocatedTokens >> 2) : 32;
		// DebugAssert(u32AllocatedTokens > u32UsedTokens);
		psToken = DebugMemRealloc(*ppsTokenBase, sizeof(Token)*u32AllocatedTokens);

		if(!psToken)
		{
			return IMG_NULL;
		}

		*pu32AllocatedTokens = u32AllocatedTokens;
		*ppsTokenBase = psToken;

		/* Zero all the newly allocated tokens in a single go */
		memset(&psToken[u32UsedTokens], 0, sizeof(Token)*(u32AllocatedTokens - u32UsedTokens));
	}
	
	*pu32UsedTokens = u32UsedTokens + 1;

	return &psToken[u32UsedTokens];
}


/******************************************************************************
 * Function Name: LexEatNewlines 
 *
 * Input-output : ppszLine, pui32CurrentChar, pui32LineNumber
 * Returns      : Success
 *
 * Description  : Finds newlines at the given position of the given string and
 *                updates the position and line number as they are found.
 *****************************************************************************/
#ifdef INLINE_IS_PRAGMA
#pragma inline(LexEatNewlines)
#endif
static INLINE IMG_BOOL LexEatNewlines(const IMG_CHAR **ppszLine, IMG_UINT32 *pui32CurrentChar, IMG_UINT32 *pui32LineNumber)
{
	IMG_BOOL bFoundNewLines = IMG_FALSE;
	const IMG_CHAR *pszLine = *ppszLine;
	IMG_UINT32 i = *pui32CurrentChar;
	IMG_UINT32 ui32LineNumber = *pui32LineNumber;

	while(pszLine[i] == LEX_LINE_FEED || pszLine[i] == LEX_CARRIAGE_RETURN)
	{
		if(pszLine[i] == LEX_LINE_FEED)
		{
			++ui32LineNumber;
			pszLine = &pszLine[i+1];
			i = 0;
			bFoundNewLines = IMG_TRUE;
		}
		else
		{
			++i;
		}
	}

	*ppszLine = pszLine;
	*pui32CurrentChar = i;
	*pui32LineNumber = ui32LineNumber;

	return bFoundNewLines;
}


/******************************************************************************
 * Function Name: LexGeneratePreprocessorTokens 
 *
 * Inputs       : ppszSources, uNumSources
 * Outputs      : ppsFirstToken, pu32NumTokens
 * Returns      : Success
 * Globals Used : -
 *
 * Description  : Performs lexical analysis on the given list of source strings
 *                and returns a list of preprocessor tokens. That is, it does _not_
 *                identify keywords (e.g. 'for', 'while', 'vec3'): they are classified
 *                simply as "identifiers".
 *****************************************************************************/
static IMG_BOOL LexGeneratePreprocessorTokens(GLSLCompilerPrivateData *psCPD,
											  const IMG_CHAR *ppszSources[], IMG_UINT32 uNumSources,
											  Token **ppsFirstToken, IMG_UINT32 *pu32NumTokens)
{
	Token *psTokenBase = IMG_NULL, *psCurrentToken = IMG_NULL;
	IMG_UINT32 u32AllocatedTokens = 0, u32UsedTokens = 0;
	IMG_UINT32 ui32Source, ui32LineNumber = 1, i;
	IMG_CHAR   *pcToken;
	const IMG_CHAR *pszLine;

	/* The scanner (lexer, tokenizer) is a hardwired finite state machine */
	enum
	{
		LEX_INITIAL                               = 0x1, /* Starting state of the scanner */

		LEX_AFTER_SLASH                           = 0x2, /* Intermediate when, from the initial state, we read a slash */
		LEX_IN_MULTI_LINE_COMMENT                 = 0x3, /* Inside a C-style multi-line comment, */
		                                                 /* before we have found an asterisk that could mark the end of the comment*/

		LEX_IN_MULTI_LINE_COMMENT_AFTER_ASTERISK  = 0x4, /* Inside a C-style multi-line comment, after we have scanned an asterisk */
		                                                 /* that could mark the end of the comment */

		LEX_IN_SINGLE_LINE_COMMENT                = 0x5, /* Inside a C++ single-line comment */

	} eState = LEX_INITIAL;

	DebugAssert(ppszSources);
	DebugAssert(ppsFirstToken);
	DebugAssert(pu32NumTokens);

	for(ui32Source = 0; ui32Source < uNumSources; ++ui32Source)
	{
		/* We currently handle comments that are split between several strings
		   but we don't support properly other split data.
		   Hopefully no-one relies on building a single token out of the end of one string and the beginning of the next.
		*/
		pszLine = ppszSources[ui32Source];
		i = 0;

		if(!pszLine)
		{
			continue;
		}

		while(pszLine[i])
		{
			/* If there's no current token, alloc one */
			if(!psCurrentToken)
			{
				psCurrentToken = LexCallocToken(&psTokenBase, &u32AllocatedTokens, &u32UsedTokens);

				if(!psCurrentToken)
				{
ReportOOMErrorAndReturnFalse:
					for(psCurrentToken = psTokenBase; psCurrentToken != &psTokenBase[u32UsedTokens]; ++psCurrentToken)
					{
						DebugMemFree(psCurrentToken->pvData);
					}

					LOG_INTERNAL_ERROR(("LexGeneratePreprocessorTokens: OOM for tokens\n"));

					DebugMemFree(psTokenBase);
					return IMG_FALSE;
				}

				psCurrentToken->eTokenName     = TOK_INVALID_TOKEN;
				psCurrentToken->uStringNumber  = ui32Source;
			}

			/* Determine what type of token is it */
			if(eState == LEX_INITIAL)
			{
				/* Eat any leading whitespace and report it as a single token
				   since whitespace is significant for the preprocessor.
				*/
				if(pszLine[i] == ' ' || pszLine[i] == '\t' || pszLine[i] == '\v')
				{
					do
					{
						++i;
					}
					while(pszLine[i] == ' ' || pszLine[i] == '\t' || pszLine[i] == '\v');

					psCurrentToken->eTokenName = TOK_TAB;
					psCurrentToken->pvData = IMG_NULL;
					psCurrentToken = IMG_NULL;
					continue;
				}

				/* Look for comments */
				if(pszLine[i] == '/')
				{
					eState = LEX_AFTER_SLASH;

					if(pszLine[i+1] == '*')
					{
						/* Eat C-style (multiline) comment */
						eState = LEX_IN_MULTI_LINE_COMMENT;
						i += 2;

EatRemainderOfMultiLineComment:
						while(pszLine[i] && eState != LEX_INITIAL)
						{
							if(pszLine[i] == '*')
							{
								eState = LEX_IN_MULTI_LINE_COMMENT_AFTER_ASTERISK;
								++i;
							}
							else if(pszLine[i] == '/' && eState == LEX_IN_MULTI_LINE_COMMENT_AFTER_ASTERISK)
							{
								/* End of the comment found */
								eState = LEX_INITIAL;
								++i;
							}
							else
							{
								eState = LEX_IN_MULTI_LINE_COMMENT;

								/* Count newlines if there are any or continue reading the input */
								if(!LexEatNewlines(&pszLine, &i, &ui32LineNumber))
								{
									++i;
								}
							}
						}
						continue;
					}
					else if(pszLine[i+1] == '/')
					{
						/* Eat C++-style single-line comment */
						eState = LEX_IN_SINGLE_LINE_COMMENT;
						i += 2;

EatRemainderOfSingleLineComment:
						while(pszLine[i] && eState != LEX_INITIAL)
						{
							/* Read LF or CR as newline */
							if(pszLine[i] == LEX_LINE_FEED 
								|| pszLine[i] == LEX_CARRIAGE_RETURN)
							{
								eState = LEX_INITIAL;
								++ui32LineNumber;
								pszLine = &pszLine[i+1];
								i = 0;

								psCurrentToken->eTokenName = TOK_NEWLINE;
								psCurrentToken->pvData = IMG_NULL;
								psCurrentToken = IMG_NULL;
							}
							else
							{
								++i;
							}
						}
						continue;
					}
					/* Else the slash did not start a comment. It will be tokenized normally */
					eState = LEX_INITIAL;
				}
				
				psCurrentToken->uStringNumber  = ui32Source;
				psCurrentToken->pszStartOfLine = (IMG_CHAR*)pszLine;
				psCurrentToken->uLineNumber    = ui32LineNumber;
				psCurrentToken->uCharNumber    = i;
				psCurrentToken->pvData         = (IMG_VOID*)&pszLine[i];

				if((pszLine[i] >= 'a' && pszLine[i] <= 'z') || (pszLine[i] >= 'A' && pszLine[i] <= 'Z') || (pszLine[i] == '_'))
				{
					/* Preprocessing identifier */
					++i;
					while((pszLine[i] >= 'a' && pszLine[i] <= 'z') || (pszLine[i] >= 'A' && pszLine[i] <= 'Z') ||
					       (pszLine[i] >= '0' && pszLine[i] <= '9') || (pszLine[i] == '_'))
					{
						++i;
					}
					psCurrentToken->eTokenName = TOK_IDENTIFIER;
				}
				else if((pszLine[i] >= '0' && pszLine[i] <= '9') ||
				        (pszLine[i] == '.' && (pszLine[i+1] >= '0' && pszLine[i+1] <= '9')))
				{
					/* Preprocessing number */
					const IMG_CHAR *pszStartPtr = &pszLine[i], *pszEndPtrFloat = pszStartPtr, *pszEndPtrInt = pszStartPtr;
					IMG_BOOL bValid = IMG_TRUE;

					/* Int or float? If int, is it octal, decimal or hex? */
					if(pszLine[i] == '0')
					{
						/* Cannot be decimal integer. */
						if(pszLine[i+1] == 'x' || pszLine[i+1] == 'X')
						{
							/* It must be hexadecimal int */
							strtol(pszStartPtr, (IMG_CHAR**)&pszEndPtrInt, 16);
						}
						else
						{
							/* See if it's a valid octal. If not, it must be a float */
							strtol(pszStartPtr, (IMG_CHAR**)&pszEndPtrInt, 8);
							
							if(*pszEndPtrInt >= '8' && *pszEndPtrInt <= '9')
							{
								bValid = IMG_FALSE;
							}
							else
							{
								strtod(pszStartPtr, (IMG_CHAR**)&pszEndPtrFloat);
								goto CheckForFloatingPointSuffix;
							}
						}
					}
					else
					{
						/* May be decimal int or float */
						strtol(pszStartPtr, (IMG_CHAR**)&pszEndPtrInt, 10);
						strtod(pszStartPtr, (IMG_CHAR**)&pszEndPtrFloat);					
CheckForFloatingPointSuffix:
						/*	Check for a floating point suffix - only allowed if the number contains a dot. */
						if(pszEndPtrFloat[0] == 'f' || pszEndPtrFloat[0] == 'F')
						{
							const IMG_CHAR* dot;
							for(dot = pszStartPtr; dot < pszEndPtrFloat; dot++)
							{
								if(*dot == '.')
								{
									pszEndPtrFloat++;
									break;
								}
							}
						}
					}

					/* 
					   NOTE: we use strod intentionally instead of strtof because the former already exists in ISO C90
							  whereas the latter was introduced in ISO C99.
					*/
					
					if(pszEndPtrInt >= pszEndPtrFloat)
					{
						{
							if(!bValid || (pszEndPtrInt == pszStartPtr) || 
							   (*pszEndPtrInt >= '0' && *pszEndPtrInt <= '9') ||
							   (*pszEndPtrInt >= 'a' && *pszEndPtrInt <= 'z') ||
							   (*pszEndPtrInt >= 'A' && *pszEndPtrInt <= 'Z') ||
							   (*pszEndPtrInt == '_'))
							{
								LogProgramError(psCPD->psErrorLog, "%d:%d:%d invalid number literal '%.*s'.\n",
									psCurrentToken->uStringNumber, psCurrentToken->uLineNumber, psCurrentToken->uCharNumber,
									pszEndPtrInt - pszStartPtr + 1, pszStartPtr);

								/**
								 * OGL64 Review.
								 * Use size_t for strlen?
								 */
								i = (IMG_UINT32)(pszEndPtrInt - pszLine + 1);
								continue;
							}
							/**
							 * OGL64 Review.
							 * Use size_t for strlen?
							 */
							i = (IMG_UINT32)(pszEndPtrInt - &pszLine[0]);
							psCurrentToken->eTokenName = TOK_INTCONSTANT;
						}
					}
					else
					{
						/**
						 * OGL64 Review.
						 * Use size_t for strlen?
						 */
						i = (IMG_UINT32)(pszEndPtrFloat - &pszLine[0]);
						psCurrentToken->eTokenName = TOK_FLOATCONSTANT;
					}
				}
				else if(pszLine[i] == LEX_LINE_FEED || pszLine[i] == LEX_CARRIAGE_RETURN)
				{
					LexEatNewlines(&pszLine, &i, &ui32LineNumber);
					psCurrentToken->eTokenName = TOK_NEWLINE;
					psCurrentToken->pvData = IMG_NULL;
					psCurrentToken = IMG_NULL;
					continue;
				}
				else
				{
					/* Punctuation mark, ordered by their ASCII representation */
					TokenName eName = TOK_INVALID_TOKEN;

					switch(pszLine[i])
					{
					case '!': eName = TOK_BANG; break;
					case '#': eName = TOK_HASH; break;
					case '%': eName = TOK_PERCENT; break;
					case '&': eName = TOK_AMPERSAND; break;
					case '(': eName = TOK_LEFT_PAREN; break;
					case ')': eName = TOK_RIGHT_PAREN; break;
					case '*': eName = TOK_STAR; break;
					case '+': eName = TOK_PLUS; break;
					case ',': eName = TOK_COMMA; break;
					case '-': eName = TOK_DASH; break;
					case '.': eName = TOK_DOT; break;
					case '/': eName = TOK_SLASH; break;
					case ':': eName = TOK_COLON; break;
					case ';': eName = TOK_SEMICOLON; break;
					case '<': eName = TOK_LEFT_ANGLE; break;
					case '=': eName = TOK_EQUAL; break;
					case '>': eName = TOK_RIGHT_ANGLE; break;
					case '?': eName = TOK_QUESTION; break;
					case '[': eName = TOK_LEFT_BRACKET; break;
					case '\\': // Technically the backslash is not part of the language but shader developers expect it to work
					          eName = TOK_BACK_SLASH; break; 
					case ']': eName = TOK_RIGHT_BRACKET; break;
					case '^': eName = TOK_CARET; break;
					case '_': eName = TOK_UNDERSCORE; break;
					case '{': eName = TOK_LEFT_BRACE; break;
					case '|': eName = TOK_VERTICAL_BAR; break;
					case '}': eName = TOK_RIGHT_BRACE; break;
					case '~': eName = TOK_TILDE; break;
					default:
						LogProgramError(psCPD->psErrorLog, "%d:%d%d '%c' is an invalid character in GLSL.\n",
							psCurrentToken->uStringNumber, psCurrentToken->uLineNumber,
							psCurrentToken->uCharNumber, pszLine[i]);
						++i;
						continue;
					}

					++i;

					/* Join punctuation marks that appear together */
					if(psCurrentToken != psTokenBase)
					{
						Token *psPrevToken = psCurrentToken - 1;
						TokenName ePrevName = psPrevToken->eTokenName;

						switch(eName)
						{
						case TOK_EQUAL:
							/* Detect +=, -=, *=, /=, etc. and update the previous token accordingly */
							switch(ePrevName)
							{
							case TOK_BANG:          ePrevName = TOK_NE_OP; break;
							case TOK_PERCENT:       ePrevName = TOK_MOD_ASSIGN; break;
							case TOK_AMPERSAND:     ePrevName = TOK_AND_ASSIGN; break;
							case TOK_STAR:          ePrevName = TOK_MUL_ASSIGN; break;
							case TOK_PLUS:          ePrevName = TOK_ADD_ASSIGN; break;
							case TOK_DASH:          ePrevName = TOK_SUB_ASSIGN; break;
							case TOK_SLASH:         ePrevName = TOK_DIV_ASSIGN; break;
							case TOK_LEFT_ANGLE:    ePrevName = TOK_LE_OP; break;
							case TOK_EQUAL:         ePrevName = TOK_EQ_OP; break;
							case TOK_RIGHT_ANGLE:   ePrevName = TOK_GE_OP; break;
							case TOK_CARET:         ePrevName = TOK_XOR_ASSIGN; break;
							case TOK_VERTICAL_BAR:  ePrevName = TOK_OR_ASSIGN; break;
							case TOK_LEFT_OP:		ePrevName = TOK_LEFT_ASSIGN; break;
							case TOK_RIGHT_OP:		ePrevName = TOK_RIGHT_ASSIGN; break;
							default: /* do nothing */ break;
							}
							break;

						case TOK_AMPERSAND:
							if(ePrevName == TOK_AMPERSAND)
							{
								ePrevName = TOK_AND_OP;
							}
							break;

						case TOK_DASH:
							if(ePrevName == TOK_DASH)
							{
								ePrevName = TOK_DEC_OP;
							}
							break;

						case TOK_PLUS:
							if(ePrevName == TOK_PLUS)
							{
								ePrevName = TOK_INC_OP;
							}
							break;

						case TOK_VERTICAL_BAR:
							if(ePrevName == TOK_VERTICAL_BAR)
							{
								ePrevName = TOK_OR_OP;
							}
							break;

						case TOK_CARET:
							if(ePrevName == TOK_CARET)
							{
								ePrevName = TOK_XOR_OP;
							}
							break;

						case TOK_LEFT_ANGLE:
							if(ePrevName == TOK_LEFT_ANGLE)
							{
								ePrevName = TOK_LEFT_OP;
							}
							break;

						case TOK_RIGHT_ANGLE:
							if(ePrevName == TOK_RIGHT_ANGLE)
							{
								ePrevName = TOK_RIGHT_OP;
							}
							break;

						default:
							/* do nothing */
							break;
						}

						if(ePrevName != psPrevToken->eTokenName)
						{
							/* If the current punctuation mark has changed the type of the previous token
							   resume scanning instead of yielding a new token. The pvData of the previous token
							   must be updated, though.
							*/
							DebugAssert(i >= 3);
							DebugMemFree(psPrevToken->pvData);
							psPrevToken->uSizeOfDataInBytes = 3;
							pcToken = DebugMemAlloc(psPrevToken->uSizeOfDataInBytes);
							if(!pcToken)
							{
								goto ReportOOMErrorAndReturnFalse;
							}
							memcpy(pcToken, &pszLine[i-2], psPrevToken->uSizeOfDataInBytes);
							pcToken[2] = '\0';
							psPrevToken->pvData = pcToken;
							psPrevToken->eTokenName = ePrevName;
							continue;
						}
					}

					psCurrentToken->eTokenName = eName;
				}

				/* Copy the characters forming the token into a null-terminated string */
				/**
				 * OGL64 Review.
				 * Use size_t for strlen?
				 */
				psCurrentToken->uSizeOfDataInBytes = (IMG_UINT32)(&pszLine[i] -(IMG_CHAR*)psCurrentToken->pvData + 1);
				pcToken = DebugMemAlloc(psCurrentToken->uSizeOfDataInBytes);
				if(!pcToken)
				{
					psCurrentToken->pvData = IMG_NULL;
					goto ReportOOMErrorAndReturnFalse;
				}
				memcpy(pcToken, psCurrentToken->pvData, psCurrentToken->uSizeOfDataInBytes-1);
				pcToken[psCurrentToken->uSizeOfDataInBytes-1] = '\0';
				psCurrentToken->pvData = pcToken;

				psCurrentToken = IMG_NULL;
			}
			else if(eState == LEX_IN_MULTI_LINE_COMMENT || eState == LEX_IN_MULTI_LINE_COMMENT_AFTER_ASTERISK)
			{
				DebugAssert(i == 0);
				goto EatRemainderOfMultiLineComment;
			}
			else
			{
				DebugAssert(eState == LEX_IN_SINGLE_LINE_COMMENT);
				goto EatRemainderOfSingleLineComment;
			}
		}

		/* There's no need to insert an end-of-string marker here.
		   That token is an artifact that shouldn't exist at all.
		   The spec states clearly that tokenization should behave as if the strings were simply concatenated.
		*/
	}

	if(eState != LEX_INITIAL)
	{
		LogProgramError(psCPD->psErrorLog, "Reached end of GLSL source inside a comment!\n");
	}

	/* Insert token to indicate termination of parsing.
	   Ideally this token shouldn't exist either but it's used in some places like ASTBIBuildIdentifierList
	*/
	if(!psCurrentToken)
	{
		psCurrentToken = LexCallocToken(&psTokenBase, &u32AllocatedTokens, &u32UsedTokens);

		if(!psCurrentToken)
		{
			goto ReportOOMErrorAndReturnFalse;
		}
	}
	psCurrentToken->eTokenName = TOK_TERMINATEPARSING;
	
	*ppsFirstToken = psTokenBase;
	*pu32NumTokens = u32UsedTokens;

	return IMG_TRUE;
}


/* *** START OF AUTOGENERATED HASH FUNCTIONS AND DATA *** */

/* Generated by $(EURASIAROOT)/tools/intern/oglcompiler/parser/perfect_hash.py */
/* DO NOT MODIFY THIS AUTOGENERATED CODE: CHANGE THE SCRIPT ABOVE */

#ifdef GLSL_ES

//
// GLSL-ES 1.00 ACTIVE KEYWORDS HASH
//

/* Table used by the hash function LexKeywordHash */
static const IMG_UINT8 au8KwMagic[64] = 
{
	/*  0 */ 0, 0, 0, 36, 0, 0, 0, 0, 0, 33, 4, 5, 0, 34, 0, 19, 23, 0, 0, 
	/* 19 */ 0, 42, 24, 35, 0, 1, 45, 8, 49, 3, 0, 15, 42, 19, 0, 29, 32, 
	/* 36 */ 30, 5, 26, 28, 18, 10, 51, 43, 0, 54, 19, 7, 42, 57, 5, 50, 40, 
	/* 53 */ 8, 0, 46, 0, 0, 11, 63, 9, 61, 4, 9
};

/* List of keywords */
static IMG_CHAR* const apszKeyword[44] = 
{
	/*  0 */ "attribute", "const", "uniform", "varying", "break", "continue", 
	/*  6 */ "do", "for", "while", "if", "else", "in", "out", "inout", "float", 
	/* 15 */ "int", "void", "bool", "true", "false", "lowp", "mediump", "highp", 
	/* 23 */ "precision", "invariant", "discard", "return", "mat2", "mat3", 
	/* 29 */ "mat4", "vec2", "vec3", "vec4", "ivec2", "ivec3", "ivec4", "bvec2", 
	/* 37 */ "bvec3", "bvec4", "sampler2D", "samplerCube", "samplerStreamIMG", 
	/* 42 */ "samplerExternalOES", "struct"
};

/* Mapping from hashes to token names */
static const TokenName aeKeywordName[44] = 
{
	/*  0 */ TOK_ATTRIBUTE, TOK_CONST, TOK_UNIFORM, TOK_VARYING, TOK_BREAK, 
	/*  5 */ TOK_CONTINUE, TOK_DO, TOK_FOR, TOK_WHILE, TOK_IF, TOK_ELSE, TOK_IN, 
	/* 12 */ TOK_OUT, TOK_INOUT, TOK_FLOAT, TOK_INT, TOK_VOID, TOK_BOOL, TOK_BOOLCONSTANT, 
	/* 19 */ TOK_BOOLCONSTANT, TOK_LOWP, TOK_MEDIUMP, TOK_HIGHP, TOK_PRECISION, 
	/* 24 */ TOK_INVARIANT, TOK_DISCARD, TOK_RETURN, TOK_MAT2X2, TOK_MAT3X3, 
	/* 29 */ TOK_MAT4X4, TOK_VEC2, TOK_VEC3, TOK_VEC4, TOK_IVEC2, TOK_IVEC3, 
	/* 35 */ TOK_IVEC4, TOK_BVEC2, TOK_BVEC3, TOK_BVEC4, TOK_SAMPLER2D, TOK_SAMPLERCUBE, 
	/* 41 */ TOK_SAMPLERSTREAMIMG, TOK_SAMPLEREXTERNALOES, TOK_STRUCT
};

/* Returns the hash value of an identifier. 
 * It's used to know which single keyword we have to compare it to since
 * NO TWO KEYWORDS HAVE THE SAME HASH VALUE.
 * The hash function has been carefully chosen to ensure this property.
 */
static IMG_UINT32 LexKeywordHash(const IMG_CHAR* pszIdent, IMG_UINT32 ui32Length)
{
	IMG_UINT32 i, ui32Hash[2] = {0x161EAC33, 0x5B0C2C52};
	
	/* Identifiers must have at least one non-null character */
	/* GLES_ASSERT(pszIdent && ui32Length > 1); */
	/* GLES_ASSERT(ui32Length == strlen(pszIdent) + 1); */
	
	/* The two hashes can be computed in parallel in a superscalar processor. Sweet :) */
	for(i=0; i < ui32Length-1; ++i)
	{
		ui32Hash[0] += pszIdent[i];
		ui32Hash[1] ^= pszIdent[i];
		ui32Hash[0] ^= ui32Hash[0] << 3;
		ui32Hash[1] += ui32Hash[1] >> 7;
	}
	
	return (au8KwMagic[ui32Hash[0] & 0x3F] + au8KwMagic[ui32Hash[1] & 0x3F]) & 0x3F;
}


//
// GLSL-ES 1.00 RESERVED KEYWORDS HASH
//

/* Table used by the hash function LexReservedHash */
static const IMG_UINT8 au8RsvMagic[64] = 
{
	/*  0 */ 0, 0, 30, 0, 48, 0, 54, 0, 63, 0, 42, 41, 14, 0, 0, 0, 32, 35, 
	/* 18 */ 29, 62, 21, 43, 0, 0, 55, 47, 43, 11, 41, 0, 59, 28, 4, 1, 7, 
	/* 35 */ 25, 3, 25, 31, 16, 0, 32, 15, 42, 59, 57, 35, 37, 39, 5, 63, 
	/* 51 */ 29, 0, 59, 46, 0, 3, 34, 43, 61, 53, 0, 5, 44
};

/* List of keywords */
static IMG_CHAR* const apszReserved[49] = 
{
	/*  0 */ "asm", "class", "union", "enum", "typedef", "template", "this", 
	/*  7 */ "packed", "goto", "switch", "default", "inline", "noinline", 
	/* 13 */ "volatile", "public", "static", "extern", "external", "interface", 
	/* 19 */ "flat", "long", "short", "double", "half", "fixed", "unsigned", 
	/* 26 */ "superp", "input", "output", "hvec2", "hvec3", "hvec4", "dvec2", 
	/* 33 */ "dvec3", "dvec4", "fvec2", "fvec3", "fvec4", "sampler1D", "sampler3D", 
	/* 40 */ "sampler1DShadow", "sampler2DShadow", "sampler2DRect", "sampler3DRect", 
	/* 44 */ "sampler2DRectShadow", "sizeof", "cast", "namespace", "using"

};

/* Returns the hash value of an identifier. 
 * It's used to know which single keyword we have to compare it to since
 * NO TWO KEYWORDS HAVE THE SAME HASH VALUE.
 * The hash function has been carefully chosen to ensure this property.
 */
static IMG_UINT32 LexReservedHash(const IMG_CHAR* pszIdent, IMG_UINT32 ui32Length)
{
	IMG_UINT32 i, ui32Hash[2] = {0xA8924D9B, 0x127E9D8E};
	
	/* Identifiers must have at least one non-null character */
	/* GLES_ASSERT(pszIdent && ui32Length > 1); */
	/* GLES_ASSERT(ui32Length == strlen(pszIdent) + 1); */
	
	/* The two hashes can be computed in parallel in a superscalar processor. Sweet :) */
	for(i=0; i < ui32Length-1; ++i)
	{
		ui32Hash[0] += pszIdent[i];
		ui32Hash[1] ^= pszIdent[i];
		ui32Hash[0] ^= ui32Hash[0] << 3;
		ui32Hash[1] += ui32Hash[1] >> 7;
	}
	
	return (au8RsvMagic[ui32Hash[0] & 0x3F] + au8RsvMagic[ui32Hash[1] & 0x3F]) & 0x3F;
}

#else /* !defined(GLSL_ES) */


#ifdef OGL_TEXTURE_STREAM

/* Output of perfect_hash_glsl120.py with OGL_TEXTURE_STREAM True */

//
// DESKTOP GLSL 1.20 ACTIVE KEYWORDS HASH
//

/* Table used by the hash function LexKeywordHash */
static const IMG_UINT8 au8KwMagic[64] = 
{
	/*  0 */ 0, 2, 40, 3, 0, 0, 0, 16, 0, 23, 54, 6, 49, 43, 11, 8, 50, 0, 
	/* 18 */ 42, 28, 12, 39, 0, 46, 34, 0, 10, 0, 50, 0, 17, 26, 52, 29, 61, 
	/* 35 */ 63, 5, 26, 26, 57, 58, 20, 48, 48, 0, 29, 53, 49, 7, 13, 59, 
	/* 51 */ 39, 0, 2, 27, 19, 55, 31, 49, 28, 29, 15, 4, 45
};

/* List of keywords */
static IMG_CHAR* const apszKeyword[53] = 
{
	/*  0 */ "attribute", "const", "uniform", "varying", "centroid", "break", 
	/*  6 */ "continue", "do", "for", "while", "if", "else", "in", "out", 
	/* 14 */ "inout", "float", "int", "void", "bool", "true", "false", "invariant", 
	/* 22 */ "discard", "return", "mat2", "mat3", "mat4", "mat2x2", "mat2x3", 
	/* 29 */ "mat2x4", "mat3x2", "mat3x3", "mat3x4", "mat4x2", "mat4x3", "mat4x4", 
	/* 36 */ "vec2", "vec3", "vec4", "ivec2", "ivec3", "ivec4", "bvec2", "bvec3", 
	/* 44 */ "bvec4", "sampler1D", "sampler2D", "sampler3D", "samplerCube", 
	/* 49 */ "sampler1DShadow", "sampler2DShadow", "struct", "samplerStreamIMG"

};

/* Mapping from hashes to token names */
static const TokenName aeKeywordName[53] = 
{
	/*  0 */ TOK_ATTRIBUTE, TOK_CONST, TOK_UNIFORM, TOK_VARYING, TOK_CENTROID, 
	/*  5 */ TOK_BREAK, TOK_CONTINUE, TOK_DO, TOK_FOR, TOK_WHILE, TOK_IF, 
	/* 11 */ TOK_ELSE, TOK_IN, TOK_OUT, TOK_INOUT, TOK_FLOAT, TOK_INT, TOK_VOID, 
	/* 18 */ TOK_BOOL, TOK_BOOLCONSTANT, TOK_BOOLCONSTANT, TOK_INVARIANT, 
	/* 22 */ TOK_DISCARD, TOK_RETURN, TOK_MAT2X2, TOK_MAT3X3, TOK_MAT4X4, 
	/* 27 */ TOK_MAT2X2, TOK_MAT2X3, TOK_MAT2X4, TOK_MAT3X2, TOK_MAT3X3, TOK_MAT3X4, 
	/* 33 */ TOK_MAT4X2, TOK_MAT4X3, TOK_MAT4X4, TOK_VEC2, TOK_VEC3, TOK_VEC4, 
	/* 39 */ TOK_IVEC2, TOK_IVEC3, TOK_IVEC4, TOK_BVEC2, TOK_BVEC3, TOK_BVEC4, 
	/* 45 */ TOK_SAMPLER1D, TOK_SAMPLER2D, TOK_SAMPLER3D, TOK_SAMPLERCUBE, 
	/* 49 */ TOK_SAMPLER1DSHADOW, TOK_SAMPLER2DSHADOW, TOK_STRUCT, TOK_SAMPLERSTREAMIMG

};

/* Returns the hash value of an identifier. 
 * It's used to know which single keyword we have to compare it to since
 * NO TWO KEYWORDS HAVE THE SAME HASH VALUE.
 * The hash function has been carefully chosen to ensure this property.
 */
static IMG_UINT32 LexKeywordHash(const IMG_CHAR* pszIdent, IMG_UINT32 ui32Length)
{
	IMG_UINT32 i, ui32Hash[2] = {0x75A5D0B6, 0x77EF4FE0};
	
	/* Identifiers must have at least one non-null character */
	/* GLES_ASSERT(pszIdent && ui32Length > 1); */
	/* GLES_ASSERT(ui32Length == strlen(pszIdent) + 1); */
	
	/* The two hashes can be computed in parallel in a superscalar processor. Sweet :) */
	for(i=0; i < ui32Length-1; ++i)
	{
		ui32Hash[0] += pszIdent[i];
		ui32Hash[1] ^= pszIdent[i];
		ui32Hash[0] ^= ui32Hash[0] << 3;
		ui32Hash[1] += ui32Hash[1] >> 7;
	}
	
	return (au8KwMagic[ui32Hash[0] & 0x3F] + au8KwMagic[ui32Hash[1] & 0x3F]) & 0x3F;
}


//
// DESKTOP GLSL 1.20 RESERVED KEYWORDS HASH
//

/* Table used by the hash function LexReservedHash */
static const IMG_UINT8 au8RsvMagic[64] = 
{
	/*  0 */ 0, 27, 0, 0, 48, 43, 32, 50, 30, 5, 0, 24, 56, 28, 34, 1, 0, 
	/* 17 */ 0, 0, 0, 0, 55, 13, 5, 30, 42, 51, 50, 0, 47, 25, 0, 38, 1, 1, 
	/* 35 */ 0, 41, 36, 60, 34, 6, 56, 38, 50, 0, 37, 40, 0, 21, 0, 3, 15, 
	/* 52 */ 47, 43, 50, 14, 0, 6, 24, 15, 0, 39, 0, 59
};

/* List of keywords */
static IMG_CHAR* const apszReserved[47] = 
{
	/*  0 */ "asm", "class", "union", "enum", "typedef", "template", "this", 
	/*  7 */ "packed", "goto", "switch", "default", "inline", "noinline", 
	/* 13 */ "volatile", "public", "static", "extern", "external", "interface", 
	/* 19 */ "long", "short", "double", "half", "fixed", "unsigned", "lowp", 
	/* 26 */ "mediump", "highp", "precision", "input", "output", "hvec2", 
	/* 32 */ "hvec3", "hvec4", "dvec2", "dvec3", "dvec4", "fvec2", "fvec3", 
	/* 39 */ "fvec4", "sampler2DRect", "sampler3DRect", "sampler2DRectShadow", 
	/* 43 */ "sizeof", "cast", "namespace", "using"
};

/* Returns the hash value of an identifier. 
 * It's used to know which single keyword we have to compare it to since
 * NO TWO KEYWORDS HAVE THE SAME HASH VALUE.
 * The hash function has been carefully chosen to ensure this property.
 */
static IMG_UINT32 LexReservedHash(const IMG_CHAR* pszIdent, IMG_UINT32 ui32Length)
{
	IMG_UINT32 i, ui32Hash[2] = {0x16483355, 0x2C7E0A19};
	
	/* Identifiers must have at least one non-null character */
	/* GLES_ASSERT(pszIdent && ui32Length > 1); */
	/* GLES_ASSERT(ui32Length == strlen(pszIdent) + 1); */
	
	/* The two hashes can be computed in parallel in a superscalar processor. Sweet :) */
	for(i=0; i < ui32Length-1; ++i)
	{
		ui32Hash[0] += pszIdent[i];
		ui32Hash[1] ^= pszIdent[i];
		ui32Hash[0] ^= ui32Hash[0] << 3;
		ui32Hash[1] += ui32Hash[1] >> 7;
	}
	
	return (au8RsvMagic[ui32Hash[0] & 0x3F] + au8RsvMagic[ui32Hash[1] & 0x3F]) & 0x3F;
}

#else

//
// DESKTOP GLSL 1.20 ACTIVE KEYWORDS HASH
//

/* Table used by the hash function LexKeywordHash */
static const IMG_UINT8 au8KwMagic[64] = 
{
	/*  0 */ 0, 0, 0, 0, 9, 33, 37, 5, 13, 60, 20, 43, 3, 4, 46, 22, 43, 0, 
	/* 18 */ 18, 40, 8, 32, 50, 39, 21, 56, 9, 55, 0, 0, 40, 28, 13, 56, 13, 
	/* 35 */ 49, 11, 0, 9, 55, 9, 2, 33, 40, 46, 0, 8, 52, 17, 28, 0, 44, 
	/* 52 */ 0, 61, 47, 48, 3, 16, 15, 0, 12, 34, 13, 54
};

/* List of keywords */
static IMG_CHAR* const apszKeyword[52] = 
{
	/*  0 */ "attribute", "const", "uniform", "varying", "centroid", "break", 
	/*  6 */ "continue", "do", "for", "while", "if", "else", "in", "out", 
	/* 14 */ "inout", "float", "int", "void", "bool", "true", "false", "invariant", 
	/* 22 */ "discard", "return", "mat2", "mat3", "mat4", "mat2x2", "mat2x3", 
	/* 29 */ "mat2x4", "mat3x2", "mat3x3", "mat3x4", "mat4x2", "mat4x3", "mat4x4", 
	/* 36 */ "vec2", "vec3", "vec4", "ivec2", "ivec3", "ivec4", "bvec2", "bvec3", 
	/* 44 */ "bvec4", "sampler1D", "sampler2D", "sampler3D", "samplerCube", 
	/* 49 */ "sampler1DShadow", "sampler2DShadow", "struct"
};

/* Mapping from hashes to token names */
static const TokenName aeKeywordName[52] = 
{
	/*  0 */ TOK_ATTRIBUTE, TOK_CONST, TOK_UNIFORM, TOK_VARYING, TOK_CENTROID, TOK_BREAK,
	/*  6 */ TOK_CONTINUE, TOK_DO, TOK_FOR, TOK_WHILE, TOK_IF, TOK_ELSE, TOK_IN, TOK_OUT,
	/* 14 */ TOK_INOUT, TOK_FLOAT, TOK_INT, TOK_VOID, TOK_BOOL, TOK_BOOLCONSTANT, TOK_BOOLCONSTANT, TOK_INVARIANT,
	/* 22 */ TOK_DISCARD, TOK_RETURN, TOK_MAT2X2, TOK_MAT3X3, TOK_MAT4X4, TOK_MAT2X2, TOK_MAT2X3,
	/* 29 */ TOK_MAT2X4, TOK_MAT3X2, TOK_MAT3X3, TOK_MAT3X4, TOK_MAT4X2, TOK_MAT4X3, TOK_MAT4X4,
	/* 36 */ TOK_VEC2, TOK_VEC3, TOK_VEC4, TOK_IVEC2, TOK_IVEC3, TOK_IVEC4, TOK_BVEC2, TOK_BVEC3,
	/* 44 */ TOK_BVEC4, TOK_SAMPLER1D, TOK_SAMPLER2D, TOK_SAMPLER3D, TOK_SAMPLERCUBE,
	/* 49 */ TOK_SAMPLER1DSHADOW, TOK_SAMPLER2DSHADOW, TOK_STRUCT
};

/* Returns the hash value of an identifier. 
 * It's used to know which single keyword we have to compare it to since
 * NO TWO KEYWORDS HAVE THE SAME HASH VALUE.
 * The hash function has been carefully chosen to ensure this property.
 */
static IMG_UINT32 LexKeywordHash(const IMG_CHAR* pszIdent, IMG_UINT32 ui32Length)
{
	IMG_UINT32 i, ui32Hash[2] = {0x33B1D2A0, 0x38256657};
	
	/* Identifiers must have at least one non-null character */
	//DebugAssert(pszIdent && ui32Length > 1);
	//DebugAssert(ui32Length == strlen(pszIdent) + 1);
	
	/* The two hashes can be computed in parallel in a superscalar processor. Sweet :) */
	for(i=0; i < ui32Length-1; ++i)
	{
		ui32Hash[0] += pszIdent[i];
		ui32Hash[1] ^= pszIdent[i];
		ui32Hash[0] ^= ui32Hash[0] << 3;
		ui32Hash[1] += ui32Hash[1] >> 7;
	}
	
	return (au8KwMagic[ui32Hash[0] & 0x3F] + au8KwMagic[ui32Hash[1] & 0x3F]) & 0x3F;
}


//
// DESKTOP GLSL 1.20 RESERVED KEYWORDS HASH
//

/* Table used by the hash function LexReservedHash */
static const IMG_UINT8 au8RsvMagic[64] = 
{
	/*  0 */ 0, 0, 0, 0, 0, 0, 2, 23, 0, 40, 0, 0, 45, 0, 39, 24, 52, 40, 
	/* 18 */ 7, 0, 31, 11, 0, 6, 63, 42, 0, 53, 10, 37, 39, 37, 4, 23, 54, 
	/* 35 */ 28, 27, 36, 6, 5, 36, 43, 58, 0, 0, 33, 12, 8, 0, 15, 37, 53, 
	/* 52 */ 21, 30, 13, 30, 35, 38, 15, 0, 5, 49, 0, 0
};

/* List of keywords */
static IMG_CHAR* const apszReserved[47] = 
{
	/*  0 */ "asm", "class", "union", "enum", "typedef", "template", "this", 
	/*  7 */ "packed", "goto", "switch", "default", "inline", "noinline", 
	/* 13 */ "volatile", "public", "static", "extern", "external", "interface", 
	/* 19 */ "long", "short", "double", "half", "fixed", "unsigned", "lowp", 
	/* 26 */ "mediump", "highp", "precision", "input", "output", "hvec2", 
	/* 32 */ "hvec3", "hvec4", "dvec2", "dvec3", "dvec4", "fvec2", "fvec3", 
	/* 39 */ "fvec4", "sampler2DRect", "sampler3DRect", "sampler2DRectShadow", 
	/* 43 */ "sizeof", "cast", "namespace", "using"
};

/* Returns the hash value of an identifier. 
 * It's used to know which single keyword we have to compare it to since
 * NO TWO KEYWORDS HAVE THE SAME HASH VALUE.
 * The hash function has been carefully chosen to ensure this property.
 */
static IMG_UINT32 LexReservedHash(const IMG_CHAR* pszIdent, IMG_UINT32 ui32Length)
{
	IMG_UINT32 i, ui32Hash[2] = {0xFD57DBA, 0x66CE4937};
	
	/* Identifiers must have at least one non-null character */
	//DebugAssert(pszIdent && ui32Length > 1);
	//DebugAssert(ui32Length == strlen(pszIdent) + 1);
	
	/* The two hashes can be computed in parallel in a superscalar processor. Sweet :) */
	for(i=0; i < ui32Length-1; ++i)
	{
		ui32Hash[0] += pszIdent[i];
		ui32Hash[1] ^= pszIdent[i];
		ui32Hash[0] ^= ui32Hash[0] << 3;
		ui32Hash[1] += ui32Hash[1] >> 7;
	}
	
	return (au8RsvMagic[ui32Hash[0] & 0x3F] + au8RsvMagic[ui32Hash[1] & 0x3F]) & 0x3F;
}
#endif /* #ifdef OGL_TEXTURE_STREAM */

#endif /* !defined(GLSL_ES) */

/* *** END OF AUTOGENERATED HASH FUNCTIONS AND DATA *** */


/******************************************************************************
 * Function Name: LexMatchKeywords 
 *
 * Input-output : psLexContext
 * Returns      : Success
 *
 * Description  : Iterates over the array of preprocessor tokens to find GLSL keywords
 *                (reserved or otherwise). The array is modified in-place, generating 
 *                syntactical tokens.
 *****************************************************************************/
static IMG_BOOL LexMatchKeywords(GLSLCompilerPrivateData *psCPD, LexContext *psLexContext)
{
	IMG_BOOL   bSuccess = IMG_TRUE;
	IMG_UINT32 i, ui32Hash;
	Token      *psTokenList = psLexContext->psTokenList;
	const IMG_CHAR* pszIdent;

	for(i=0; i < psLexContext->uNumTokens; ++i)
	{
		if(psTokenList[i].eTokenName == TOK_IDENTIFIER)
		{
			pszIdent = (IMG_CHAR*)psTokenList[i].pvData;

			/* Thanks to the perfect hash function we only have to compare the identifier
			   with a single keyword at most. See http://en.wikipedia.org/wiki/Perfect_hash_function
			*/
			ui32Hash = LexKeywordHash(pszIdent, psTokenList[i].uSizeOfDataInBytes);

			if(ui32Hash < sizeof(apszKeyword)/sizeof(IMG_CHAR*) &&
			   !strcmp(pszIdent, apszKeyword[ui32Hash]))
			{
				psTokenList[i].eTokenName = aeKeywordName[ui32Hash];
			}
			else
			{
				/* It didn't match an active keyword. Is it a reserved keyword? */
				ui32Hash = LexReservedHash(pszIdent, psTokenList[i].uSizeOfDataInBytes);

				if(ui32Hash < sizeof(apszReserved)/sizeof(IMG_CHAR*) &&
				   !strcmp(pszIdent, apszReserved[ui32Hash]))
				{
					LogProgramError(psCPD->psErrorLog, "%d:%d:%d reserved keyword '%s'.\n",
						psTokenList[i].uStringNumber, psTokenList[i].uLineNumber,
						psTokenList[i].uCharNumber, psTokenList[i].pvData);
					bSuccess = IMG_FALSE;
				}
			}
		}
	}

	return bSuccess;
}


/******************************************************************************
 * Function Name: LexCreateTokenList
 *
 * Inputs       : LexruleDatabase, Source, RemoveList, RemoveListSize
 * Outputs      : puNumTokens
 * Returns      : Tokenlist
 * Globals Used : -
 *
 * Description  : Generates a list of syntactical tokens and attaches it to the parse context 
 *****************************************************************************/
IMG_INTERNAL Token *LexCreateTokenList(IMG_VOID *pvCompilerPrivateData,
									   const IMG_CHAR            *ppszSources[],
										IMG_UINT32              uNumSources,
										IMG_UINT32             *puNumTokens,
										IMG_VOID				**ppvPreProcessorData)
{
	GLSLCompilerPrivateData *psCPD = (GLSLCompilerPrivateData*)pvCompilerPrivateData;
	Token *psNewToken;
	IMG_UINT32 ui32NumNewTokens = 0;
	IMG_UINT32 i;
	LexContext sLexContext;

	/* Reset context pointers */
	sLexContext.uNumTokens            = 0;
	sLexContext.psTokenList           = IMG_NULL;
	sLexContext.uCurrentStringNumber  = 0;

	if(!LexGeneratePreprocessorTokens(psCPD, ppszSources, uNumSources, &sLexContext.psTokenList, &sLexContext.uNumTokens))
	{
		DEBUG_MESSAGE(("LexCreateTokenList: error generating preprocessor tokens\n"));
		return IMG_NULL;
	}

	/* Run the preprocessor */
	{
		Token      *psPreProcessedTokenList;
		IMG_UINT32  uNewNumTokens;
		IMG_BOOL    bSuccess;

		MetricStart(pvCompilerPrivateData, METRICS_PREPROCESSOR);

		bSuccess = PPPreProcessTokenList(psCPD, ppvPreProcessorData, sLexContext.psTokenList, sLexContext.uNumTokens, &psPreProcessedTokenList, &uNewNumTokens);
		
		/* Destroy the old token list */
		LexDestroyTokenList(sLexContext.psTokenList, sLexContext.uNumTokens);
		
		MetricFinish(pvCompilerPrivateData, METRICS_PREPROCESSOR);

		if (!bSuccess)
		{
			return IMG_NULL;
		}

		sLexContext.psTokenList = psPreProcessedTokenList;
		sLexContext.uNumTokens = uNewNumTokens;

		/* Dump some log file info */
#ifdef DUMP_LOGFILES
		DumpLogMessage(LOGFILE_TOKENS_GENERATED, 0, "\nAfter preprocessor %d tokens:\n", sLexContext.uNumTokens);
		LexDecodeTokenList(&sLexContext);
#endif
	}

	/* Remove all whitespace and detect calls to the pseudo-function of arrays '.length()' */
	psNewToken = DebugMemAlloc(sizeof(Token)*sLexContext.uNumTokens);

	if(!psNewToken)
	{
		DEBUG_MESSAGE(("LexGeneratePreprocessorTokens: OOM for tokens\n"));
		LexDestroyTokenList(sLexContext.psTokenList, sLexContext.uNumTokens);
		return IMG_NULL;
	}

	for(i=0; i < sLexContext.uNumTokens; ++i)
	{
		if((sLexContext.psTokenList[i].eTokenName == TOK_TAB) ||
		   (sLexContext.psTokenList[i].eTokenName == TOK_NEWLINE))
		{
			DebugMemFree(sLexContext.psTokenList[i].pvData);
		}
		else
		{
#if !defined(GLSL_ES)
			/* XXX: FIXME: calls to array.length() should be detected during syntactic analysis, not here */
			if(sLexContext.psTokenList[i].eTokenName == TOK_RIGHT_PAREN && (ui32NumNewTokens >= 3))
			{
				Token *psDot = &psNewToken[ui32NumNewTokens-3];
				Token *psLength = &psNewToken[ui32NumNewTokens-2];
				Token *psLeftParen = &psNewToken[ui32NumNewTokens-1];

				if((psDot->eTokenName == TOK_DOT) &&
				   (psLength->eTokenName == TOK_IDENTIFIER && psLength->pvData && !strcmp(psLength->pvData, "length")) &&
				   (psLeftParen->eTokenName == TOK_LEFT_PAREN))
				{
					sLexContext.psTokenList[i].eTokenName = TOK_ARRAY_LENGTH;
					DebugMemFree(psNewToken[ui32NumNewTokens-3].pvData);
					DebugMemFree(psNewToken[ui32NumNewTokens-2].pvData);
					DebugMemFree(psNewToken[ui32NumNewTokens-1].pvData);
					ui32NumNewTokens = ui32NumNewTokens - 3;
				}
			}
#endif

			psNewToken[ui32NumNewTokens++] = sLexContext.psTokenList[i];
		}
	}
	DebugMemFree(sLexContext.psTokenList);
	sLexContext.psTokenList = psNewToken;
	sLexContext.uNumTokens = ui32NumNewTokens;

	/* Find identifiers that match keywords (reserved or otherwise)*/
	if(!LexMatchKeywords(psCPD, &sLexContext))
	{
		LexDestroyTokenList(sLexContext.psTokenList, sLexContext.uNumTokens);
		return IMG_NULL;
	}

#ifdef DUMP_LOGFILES
	/* Dump some log file info */
	DumpLogMessage(LOGFILE_TOKENS_GENERATED, 0, "\nAfter token removal %d tokens:\n", sLexContext.uNumTokens);
	LexDecodeTokenList(&sLexContext);
#endif

	*puNumTokens = sLexContext.uNumTokens;

	return sLexContext.psTokenList;
}

/******************************************************************************
 End of file (lex.c)
******************************************************************************/

