/******************************************************************************
 * Name         : error.c
 * Author       : James McCarthy
 * Created      : 28/07/2004
 *
 * Copyright    : 2000-2006 by Imagination Technologies Limited.
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
 * $Log: error.c $
 *****************************************************************************/

#include "error.h"
#include "../parser/debug.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define MESSAGE_STYLE_3D_LABS	1
#undef  MESSAGE_STYLE_EMACS

#define ERROR_SCRATCH_BUFFER_SIZE 400

static IMG_VOID LogErrorMessage(ErrorLog       *psErrorLog, 
								ErrorType       eErrorType, 
								const Token    *psToken, 
								const IMG_CHAR *pszFormat, 
								va_list         vaList)
								IMG_FORMAT_PRINTF(4, 0);


//ErrorLog *gpsErrorLog = IMG_NULL;

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
IMG_INTERNAL IMG_VOID SetErrorLog(ErrorLog *psErrorLog, IMG_BOOL bDisplayMessages)
{
	psErrorLog->uTotalNumErrorMessages     = 0;
	psErrorLog->uNumInternalErrorMessages  = 0;
	psErrorLog->uNumProgramErrorMessages   = 0;
	psErrorLog->uNumProgramWarningMessages = 0;
	psErrorLog->bDisplayMessages           = bDisplayMessages;
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
IMG_INTERNAL IMG_VOID FreeErrorLogMessages(ErrorLog *psErrorLog)
{
	if (psErrorLog)
	{
		IMG_UINT32 i;

		for (i =0; i < psErrorLog->uTotalNumErrorMessages; i++)
		{
			DebugMemFree(psErrorLog->sErrorMessages[i].pszErrorMessageString);
		}
	}
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
IMG_INTERNAL IMG_VOID DisplayErrorMessages(ErrorLog *psErrorLog, ErrorType eErrorType)
{
	IMG_UINT32 i;

	for (i = 0; i < psErrorLog->uTotalNumErrorMessages; i++)
	{
		ErrorMessage *psErrorMessage = &(psErrorLog->sErrorMessages[i]);

		if (psErrorMessage->eErrorType & eErrorType)
		{
			DEBUG_MESSAGE(("%s", psErrorMessage->pszErrorMessageString));
		}
	}
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
static IMG_VOID LogErrorMessageBuffer(ErrorLog		*psErrorLog,
										ErrorType		eErrorType,
										const Token		*psToken,
										const IMG_CHAR	*pszString)
{
	IMG_UINT32 uLineNumber;
	IMG_UINT32 uStringNumber;

	ErrorMessage *psErrorMessage = IMG_NULL;

	const IMG_CHAR *pszErrorType;

	switch (eErrorType)
	{
		case ERRORTYPE_INTERNAL:
			pszErrorType = "INTERNAL";
			break;
		case ERRORTYPE_PROGRAM_ERROR:
			pszErrorType = "ERROR";
			break;
		case ERRORTYPE_PROGRAM_WARNING:
			pszErrorType = "WARNING";
			break;
		default:
			pszErrorType = "UNKNOWN";
			break;
	}

	if (psErrorLog)
	{
		IMG_UINT32 uStringSize;

		/* Check for error limit */
		if (psErrorLog->uTotalNumErrorMessages >= MAX_ERROR_MESSAGES)
		{
			return;
		}

		psErrorMessage = &psErrorLog->sErrorMessages[psErrorLog->uTotalNumErrorMessages];

		/* Calculate size of string to allocate */
		/**
		 * OGL64 Review.
		 * Use size_t for strlen?
		 */
		uStringSize = (IMG_UINT32)strlen(pszString) + 40;

		if (psToken)
		{
			/* Allocate space for under line ptr */
			uStringSize += 512;
		}
		
		psErrorMessage->pszErrorMessageString = DebugMemAlloc(uStringSize);

		if (!psErrorMessage->pszErrorMessageString)
		{
			DEBUG_MESSAGE(("LogErrorMessageBuffer: Failed to alloc mem for error string\n"));
			return;
		}

		psErrorMessage->eErrorType = eErrorType;

		if (eErrorType == ERRORTYPE_INTERNAL)
		{
			psErrorLog->uNumInternalErrorMessages++;
		}
		else if (eErrorType == ERRORTYPE_PROGRAM_ERROR)
		{
			psErrorLog->uNumProgramErrorMessages++;
		}
		else if (eErrorType == ERRORTYPE_PROGRAM_WARNING)
		{
			psErrorLog->uNumProgramWarningMessages++;
		}

		/* Increase total number of messages */
		psErrorLog->uTotalNumErrorMessages++;
	}

	if (psToken)
	{
		uLineNumber   = psToken->uLineNumber;
		uStringNumber = psToken->uStringNumber;

		psErrorMessage->uCharNumber    = psToken->uCharNumber;
		psErrorMessage->pszStartOfLine = psToken->pszStartOfLine;


#ifdef MESSAGE_STYLE_3D_LABS
		if (psErrorMessage)
		{
			/* */
			sprintf(psErrorMessage->pszErrorMessageString, "%s: %u:%u: %s", pszErrorType, uStringNumber, uLineNumber, pszString);
		
			if (psErrorLog->bDisplayMessages)
			{
				DEBUG_MESSAGE(("%s", psErrorMessage->pszErrorMessageString));
			}
		}
		else
		{
			DEBUG_MESSAGE(("%s: %u:%u: %s", pszErrorType, uStringNumber, uLineNumber, pszString));
		}
#else
	#ifdef MESSAGE_STYLE_EMACS
			if (psErrorMessage)
			{
				sprintf(psErrorMessage->pszErrorMessageString, "%s(%u) : %s",".\\test2.vs", uLineNumber, pszString);

				if (psErrorLog->bDisplayMessages)
				{
					DEBUG_MESSAGE(("%s", psErrorMessage->pszErrorMessageString));
				}

			}
			else
			{
				DEBUG_MESSAGE(("%s(%u) : %s",".\\test2.vs", uLineNumber, pszString));
			}

	#else
			if (psErrorMessage)
			{
				IMG_UINT32 i = 0;
				IMG_CHAR *pszNewString;

				sprintf(psErrorMessage->pszErrorMessageString, "%s: %u:%u: %s", pszErrorType, uStringNumber, uLineNumber, pszString);


				pszNewString = &(psErrorMessage->pszErrorMessageString[strlen(psErrorMessage->pszErrorMessageString)]);

				while (i < 255 &&
					   psErrorMessage->pszStartOfLine[i] != '\n' &&
					   psErrorMessage->pszStartOfLine[i] != '\r' &&
					   psErrorMessage->pszStartOfLine[i] != '\0')
				{
					pszNewString[i] = psErrorMessage->pszStartOfLine[i];
					i++;
				}

				pszNewString[i++] = '\n';
				pszNewString = &(pszNewString[i]);

				for (i = 0; i < psErrorMessage->uCharNumber; i++)
				{
					if (psErrorMessage->pszStartOfLine[i] == '\t')
					{
						pszNewString[i] = '\t';
					}
					else
					{
						pszNewString[i] = ' ';
					}
				}

				sprintf(&pszNewString[i], "^\n");

				if (psErrorLog->bDisplayMessages)
				{
					DEBUG_MESSAGE(("%s", psErrorMessage->pszErrorMessageString));
				}
			}
			else
			{
				DebugMessageWithLineInfo(psToken, "%s: %u:%u: %s", pszErrorType, uStringNumber, uLineNumber, pszString);
			}
	#endif
#endif
	}
	else
	{
		psErrorMessage->uCharNumber    = 0;
		psErrorMessage->pszStartOfLine = IMG_NULL;

		if (psErrorMessage)
		{
			sprintf(psErrorMessage->pszErrorMessageString, "%s: %s", pszErrorType, pszString);
		}
		else
		{
			DEBUG_MESSAGE(("%s: %s", pszErrorType, pszString));
		}
	}
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
static IMG_VOID LogErrorMessage(ErrorLog       *psErrorLog, 
								ErrorType       eErrorType, 
								const Token    *psToken, 
								const IMG_CHAR *pszFormat, 
								va_list         vaList)
{
	IMG_CHAR acErrorScratchBuffer[ERROR_SCRATCH_BUFFER_SIZE];
   	/* Put message into buffer */
	/* Unsafe - Add safe version of this function for your OS if you know it */ 
	vsprintf (acErrorScratchBuffer, pszFormat, vaList);

	/* Reset the argument pointer */
	va_end(vaList);

	LogErrorMessageBuffer(psErrorLog, eErrorType, psToken, acErrorScratchBuffer);
}

#define FindNearestToken(a) (a)

#ifdef DEBUG

static IMG_VOID PrintInternalErrorMessage(Token *psToken, const IMG_CHAR *pszFormat, va_list vaList)  IMG_FORMAT_PRINTF(2, 0);

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
static IMG_VOID PrintInternalErrorMessage(Token *psToken, 
											const IMG_CHAR *pszFormat, 
											va_list         vaList)
#ifdef STANDALONE
{
	fprintf(stderr, "INTERNAL - ");

	if(psToken)
	{
		fprintf(stderr, "%u:%u:%u '%.*s' - ", 
			psToken->uStringNumber, psToken->uLineNumber, psToken->uCharNumber,
			(IMG_INT)psToken->uSizeOfDataInBytes, (IMG_CHAR*)psToken->pvData);
	}

	vfprintf(stderr, pszFormat, vaList);
	va_end(vaList);
}
#else /* !STANDALONE */
{
	IMG_CHAR pszBuffer[ERROR_SCRATCH_BUFFER_SIZE];
	sprintf(pszBuffer, "INTERNAL - ");

	if(psToken)
	{
		sprintf(&pszBuffer[strlen(pszBuffer)], "%u:%u:%u '%.*s' - ", 
			psToken->uStringNumber, psToken->uLineNumber, psToken->uCharNumber,
			(IMG_INT)psToken->uSizeOfDataInBytes, (IMG_CHAR*)psToken->pvData);
	}

	vsprintf(&pszBuffer[strlen(pszBuffer)], pszFormat, vaList);
	va_end(vaList);

	PVR_TRACE(("%s", pszBuffer));
}
#endif /* ifndef STANDALONE */

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
IMG_INTERNAL IMG_VOID PrintInternalErrorFunc(const IMG_CHAR *pszFormat, ...)
{
	va_list vaList;

	va_start (vaList, pszFormat);

	PrintInternalErrorMessage(IMG_NULL, pszFormat, vaList);
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
IMG_INTERNAL IMG_VOID PrintInternalTokenErrorFunc(Token *psToken, const IMG_CHAR *pszFormat, ...)
{
	va_list vaList;

	va_start (vaList, pszFormat);

	PrintInternalErrorMessage(psToken, pszFormat, vaList);
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
IMG_INTERNAL IMG_VOID PrintInternalNodeErrorFunc(GLSLNode *psNode, const IMG_CHAR *pszFormat, ...)
{
	va_list vaList;

	va_start (vaList, pszFormat);

	PrintInternalErrorMessage(FindNearestToken(psNode->psParseTreeEntry), pszFormat, vaList);
}

#endif /* defined DEBUG */



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
IMG_INTERNAL IMG_VOID LogProgramError(ErrorLog *psErrorLog, const IMG_CHAR *pszFormat, ...)
{
	va_list vaList;

	va_start (vaList, pszFormat);

	LogErrorMessage(psErrorLog, ERRORTYPE_PROGRAM_ERROR, IMG_NULL, pszFormat, vaList);
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
IMG_INTERNAL IMG_VOID LogProgramWarning(ErrorLog *psErrorLog, const IMG_CHAR *pszFormat, ...)
{
	va_list vaList;

	va_start (vaList, pszFormat);

	LogErrorMessage(psErrorLog, ERRORTYPE_PROGRAM_WARNING, IMG_NULL, pszFormat, vaList);
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
IMG_INTERNAL IMG_VOID LogProgramNodeError(ErrorLog *psErrorLog, GLSLNode *psNode, const IMG_CHAR *pszFormat, ...)
{
	va_list vaList;

	va_start (vaList, pszFormat);

	LogErrorMessage(psErrorLog, ERRORTYPE_PROGRAM_ERROR, FindNearestToken(psNode->psParseTreeEntry), pszFormat, vaList);
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
IMG_INTERNAL IMG_VOID LogProgramNodeWarning(ErrorLog *psErrorLog, GLSLNode *psNode, const IMG_CHAR *pszFormat, ...)
{
	va_list vaList;

	va_start (vaList, pszFormat);

	LogErrorMessage(psErrorLog, ERRORTYPE_PROGRAM_WARNING, FindNearestToken(psNode->psParseTreeEntry), pszFormat, vaList);
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
IMG_INTERNAL IMG_VOID LogProgramTokenError(ErrorLog *psErrorLog, const Token *psToken, const IMG_CHAR *pszFormat, ...)
{
	va_list vaList;

	va_start (vaList, pszFormat);

	LogErrorMessage(psErrorLog, ERRORTYPE_PROGRAM_ERROR, psToken, pszFormat, vaList);
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
IMG_INTERNAL IMG_VOID LogProgramTokenWarning(ErrorLog *psErrorLog, const Token *psToken, const IMG_CHAR *pszFormat, ...)
{
	va_list vaList;

	va_start (vaList, pszFormat);

	LogErrorMessage(psErrorLog, ERRORTYPE_PROGRAM_WARNING, psToken, pszFormat, vaList);
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
IMG_INTERNAL IMG_VOID LogProgramParserError(ErrorLog *psErrorLog, const Token *psToken, const IMG_CHAR *pszFormat, ...)
{
	va_list vaList;

	va_start (vaList, pszFormat);

	LogErrorMessage(psErrorLog, ERRORTYPE_PROGRAM_ERROR, psToken, pszFormat, vaList);
}



/******************************************************************************
 End of file (error.c)
******************************************************************************/

