/**************************************************************************
 * Name         : error.h
 * Author       : James McCarthy
 * Created      : 28/07/2004
 *
 * Copyright    : 2000-2004 by Imagination Technologies Limited. All rights reserved.
 *              : No part of this software, either material or conceptual 
 *              : may be copied or distributed, transmitted, transcribed,A
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
 * $Log: error.h $
 **************************************************************************/

#ifndef __gl_error_h_
#define __gl_error_h_

#include "errorlog.h"
#include "glsltree.h"


IMG_VOID SetErrorLog(ErrorLog *psErrorLog, IMG_BOOL bDisplayMessages);

IMG_VOID FreeErrorLogMessages(ErrorLog *psErrorLog);

IMG_VOID DisplayErrorMessages(ErrorLog *psErrorLog, ErrorType eErrorType);

IMG_VOID LogProgramError(ErrorLog *psErrorLog, const IMG_CHAR *pszFormat, ...) IMG_FORMAT_PRINTF(2, 3);

IMG_VOID LogProgramWarning(ErrorLog *psErrorLog, const IMG_CHAR *pszFormat, ...) IMG_FORMAT_PRINTF(2, 3);

IMG_VOID LogProgramNodeError(ErrorLog *psErrorLog, GLSLNode *psNode, const IMG_CHAR *pszFormat, ...) IMG_FORMAT_PRINTF(3, 4);

IMG_VOID LogProgramNodeWarning(ErrorLog *psErrorLog, GLSLNode *psNode, const IMG_CHAR *pszFormat, ...) IMG_FORMAT_PRINTF(3, 4);

#define LogProgramParseTreeError LogProgramTokenError
#define PrintInternalParseTreeErrorFunc PrintInternalTokenErrorFunc

IMG_VOID LogProgramTokenError(ErrorLog *psErrorLog, const Token *psToken, const IMG_CHAR *pszFormat, ...) IMG_FORMAT_PRINTF(3, 4);

IMG_VOID LogProgramTokenWarning(ErrorLog *psErrorLog, const Token *psToken, const IMG_CHAR *pszFormat, ...) IMG_FORMAT_PRINTF(3, 4);

IMG_VOID LogProgramParserError(ErrorLog *psErrorLog, const Token *psToken, const IMG_CHAR *pszFormat, ...) IMG_FORMAT_PRINTF(3, 4);


#define INCREMENT_INTERNAL_ERROR_COUNT() psCPD->psErrorLog->uNumInternalErrorMessages++

#ifdef DEBUG
	/* These functions just print the error message to stderr. They DON'T store it in the error log */
	IMG_VOID PrintInternalErrorFunc(const IMG_CHAR *pszFormat, ...) IMG_FORMAT_PRINTF(1, 2);
	IMG_VOID PrintInternalNodeErrorFunc(GLSLNode *psNode, const IMG_CHAR *pszFormat, ...) IMG_FORMAT_PRINTF(2, 3);	
	IMG_VOID PrintInternalTokenErrorFunc(Token *psToken, const IMG_CHAR *pszFormat, ...) IMG_FORMAT_PRINTF(2, 3);

	/* ISO C90 does not include variadic (multiple-argument) macros, so we have to do this... */
#   define LOG_INTERNAL_ERROR(args)             {PrintInternalErrorFunc args; \
												INCREMENT_INTERNAL_ERROR_COUNT();}

#   define LOG_INTERNAL_NODE_ERROR(args)        {PrintInternalNodeErrorFunc args; \
												INCREMENT_INTERNAL_ERROR_COUNT();}

#   define LOG_INTERNAL_PARSE_TREE_ERROR(args)  {PrintInternalParseTreeErrorFunc args; \
												INCREMENT_INTERNAL_ERROR_COUNT();}

#   define LOG_INTERNAL_TOKEN_ERROR(args)       {PrintInternalTokenErrorFunc args; \
												INCREMENT_INTERNAL_ERROR_COUNT();}


#else /* !defined DEBUG */

	/* ISO C90 does not include variadic (multiple-argument) macros, so we have to do this... */
#   define LOG_INTERNAL_ERROR(args)             INCREMENT_INTERNAL_ERROR_COUNT()

#   define LOG_INTERNAL_NODE_ERROR(args)        INCREMENT_INTERNAL_ERROR_COUNT()

#   define LOG_INTERNAL_PARSE_TREE_ERROR(args)  INCREMENT_INTERNAL_ERROR_COUNT()

#   define LOG_INTERNAL_TOKEN_ERROR(args)       INCREMENT_INTERNAL_ERROR_COUNT()

#endif /* !defined DEBUG */

#endif //__gl_error_h_
