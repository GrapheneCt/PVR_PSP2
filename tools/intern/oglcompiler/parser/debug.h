/**************************************************************************
 * Name         : debug.h
 * Author       : James McCarthy
 * Created      : 22/01/2004
 *
 * Copyright    : 2000-2004 by Imagination Technologies Limited. All rights reserved.
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
 * $Log: debug.h $
 **************************************************************************/

#ifndef __gl_debug_h_
#define __gl_debug_h_


#include <stdlib.h>
#include "tokens.h"
#include "img_types.h"
#include "memmgr.h"

#if (defined(DEBUG) || defined(METRICS)) && !defined(STANDALONE)
#include "pvr_debug.h"
#endif



/* Assertions */
#ifdef DEBUG 
#define DebugAssert(condition) \
	if(!(condition)) {LOG_INTERNAL_ERROR(("%s Line %d: Assertion '%s' fails\n", __FILE__, __LINE__, #condition));}
#else
#define DebugAssert(condition)
#endif



typedef IMG_VOID (IMG_CALLCONV *DebugDisplayMessageFunction)(const IMG_CHAR *pszString);

#ifdef DUMP_LOGFILES

typedef enum LogFileTAG
{
	LOGFILE_PARSETREE           = 0,
	LOGFILE_OPTIMISED_PARSETREE = 1,
	LOGFILE_TOKENS_GENERATED    = 2,
	LOGFILE_TOKENISING_LOG      = 3,
	LOGFILE_PARSING_LOG         = 4,
	LOGFILE_SYNTAX_GRAMMAR      = 5,
	LOGFILE_LEXICAL_RULES       = 6,
	LOGFILE_SYNTAXSYMBOLS       = 7,
	LOGFILE_SYMBOLTABLE         = 8,
	LOGFILE_COMPILER            = 9,
	LOGFILE_ASTREE              = 10,
	LOGFILE_ICODE               = 11,
	LOGFILE_SYMTABLE_DATA       = 12,
	LOGFILE_BINDINGSYMBOLS      = 13,
	LOGFILE_PREPROCESSOR        = 14,
	LOGFILE_MEMORY_STATS        = 15,
	LOGFILE_LAST_LOG_FILE       = 16, /* IF you add another log file always bump this one to the end */
} LogFile;


IMG_BOOL CreateLogFile(LogFile eLogFile, IMG_CHAR *pszFileName);

IMG_VOID CloseLogFile(LogFile eLogFile);

IMG_VOID FlushLogFiles(void);

IMG_BOOL LogFileActive(LogFile eLogFile);

IMG_VOID IMG_CALLCONV DumpLogMessage(LogFile eLogFile, IMG_UINT32 uTabLength, const IMG_CHAR *pszFormat, ...) IMG_FORMAT_PRINTF(3, 4);

#endif

/* Always use the macro DEBUG_MESSAGE rather than calling DebugMessageFn directly */

#if (defined(DEBUG) && defined(STANDALONE)) || defined(METRICS)
IMG_VOID IMG_CALLCONV DebugMessageWithLineInfo(Token *psToken, const IMG_CHAR *pszFormat, ...) IMG_FORMAT_PRINTF(2, 3);
IMG_VOID IMG_CALLCONV DebugMessageFn(const IMG_CHAR *pszFormat, ...) IMG_FORMAT_PRINTF(1, 2);
#	ifdef STANDALONE
#		define DEBUG_MESSAGE(args) DebugMessageFn args
#	else
#		define DEBUG_MESSAGE(args) PVR_TRACE(args)
#	endif
#else  /* (defined(DEBUG) && defined(STANDALONE)) || defined(METRICS) */
#	define DEBUG_MESSAGE(args)
#endif /* (defined(DEBUG) && defined(STANDALONE)) || defined(METRICS) */

#endif // __gl_debug_h_
