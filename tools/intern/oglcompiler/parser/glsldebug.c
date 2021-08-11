/******************************************************************************
 * Name         : debug.c
 * Author       : James McCarthy
 * Created      : 22/01/2004
 *
 * Copyright    : 2000-2007 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited, Home Park
 *              : Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * Modifications:-
 * $Log: glsldebug.c $
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#if defined(LINUX)
#include <sys/stat.h> /* For directory creation/manipulation functions */
#include <unistd.h>
#endif
#if !defined(LINUX) && !defined(__psp2__)
#include <direct.h> /* For directory creation/manipulation functions */
#endif
#include "debug.h"


#if !defined(STANDALONE)
#include "pvr_debug.h"
#endif


#define DEBUG_SCRATCH_BUFFER_SIZE 500

#if defined(DUMP_LOGFILES)

static FILE *gLogFiles[LOGFILE_LAST_LOG_FILE]={ 0 };


/******************************************************************************
 * Function Name: GetLogFile
 *
 * Inputs       : eLogFile
 * Outputs      : -
 * Returns      : Pointer to logfile
 * Globals Used : -
 *
 * Description  : Get's a handle to a logfile
 *****************************************************************************/
#define GetLogFile(a) ((a >=  LOGFILE_LAST_LOG_FILE) ? IMG_NULL : gLogFiles[eLogFile])


	#ifdef LINUX 

	static IMG_INT32 MakeDir(IMG_CHAR *pszFilename)
	{
		return mkdir(pszFilename, S_IRWXU);
	}

	static IMG_INT32 ChangeDir(IMG_CHAR *pszPath)
	{
		return chdir(pszPath);
	}

	#else


			static IMG_INT32 MakeDir(IMG_CHAR *pszFilename)
			{
				return _mkdir(pszFilename);
			}

			static IMG_INT32 ChangeDir(IMG_CHAR *pszPath)
			{
				return _chdir(pszPath);
			}
	#endif

/******************************************************************************
 * Function Name: CreateLogFile
 *
 * Inputs       : eLogFile
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL LogFileActive(LogFile eLogFile)
{
	if (!GetLogFile(eLogFile))
	{
		return IMG_FALSE;
	}
	else
	{
		return IMG_TRUE;
	}
}

/******************************************************************************
 * Function Name: CreateLogFile
 *
 * Inputs       : eLogFile
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL CreateLogFile(LogFile eLogFile, IMG_CHAR *pszFileName)
{
	FILE     *LogFile = NULL;
	IMG_BOOL  bChangedToLogFileDir = IMG_FALSE;

	if (eLogFile >= LOGFILE_LAST_LOG_FILE)
	{
		return IMG_FALSE;
	}

	if(ChangeDir("logfiles"))
	{
		if ( MakeDir("logfiles") == 0)
		{
			ChangeDir("logfiles");
			bChangedToLogFileDir = IMG_TRUE;
		}
		else
		{
			bChangedToLogFileDir = IMG_FALSE;
		}
	}
	else
	{
		bChangedToLogFileDir = IMG_TRUE;
	}

	if (pszFileName)
	{
		LogFile = fopen(pszFileName, "wc");
	}

	if (bChangedToLogFileDir)
	{
		ChangeDir("..");
	}

	gLogFiles[eLogFile] = LogFile;

	if (!LogFile)
	{
		DEBUG_MESSAGE(("CreateLogFile: Failed to create logfile \n"));
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: CloseLogFile
 *
 * Inputs       : -
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_VOID CloseLogFile(LogFile eLogFile)
{
	FILE *LogFile = GetLogFile(eLogFile);

	if (LogFile)
	{
		fclose(LogFile);
		LogFile = NULL;
	}
}

/******************************************************************************
 * Function Name: FlushLogFiles
 *
 * Inputs       : -
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_VOID FlushLogFiles(void)
{
	int i;
	for(i = 0; i < LOGFILE_LAST_LOG_FILE; i++)
	{
		if(gLogFiles[i])
		{
			fflush(gLogFiles[i]);
		}
	}
}

/******************************************************************************
 * Function Name: DumpLogMessage
 *
 * Inputs       : eLogFile, uTabLength, pszFormat
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : Writes a message to a log file.
 *****************************************************************************/
IMG_INTERNAL IMG_VOID DumpLogMessage(LogFile eLogFile, IMG_UINT32 uTabLength, const IMG_CHAR *pszFormat, ...)
{
	va_list vaArgs;
	IMG_UINT32 i;
	static IMG_BOOL bTabLengthErrorReported = IMG_FALSE;

	FILE *LogFile = GetLogFile(eLogFile);

	if (LogFile)
	{
		IMG_CHAR acScratchBuffer[DEBUG_SCRATCH_BUFFER_SIZE];

		if ((uTabLength > (DEBUG_SCRATCH_BUFFER_SIZE / 2)) && !bTabLengthErrorReported)
		{
			DEBUG_MESSAGE(("DumpLogMessage: Tab length too long (%d)(LOGFILE = %08X)!\n(Will not report any further errors of this type)\n",
						 uTabLength,
						 eLogFile));
			uTabLength = DEBUG_SCRATCH_BUFFER_SIZE / 2;
			bTabLengthErrorReported = IMG_TRUE;
		}

		va_start (vaArgs, pszFormat);

		vsnprintf (acScratchBuffer, DEBUG_SCRATCH_BUFFER_SIZE, pszFormat, vaArgs);
		
		for (i = 0; i < uTabLength; i++)
		{
			fprintf(LogFile, " ");
		}
		
		fprintf(LogFile, "%s", acScratchBuffer);
		
		va_end (vaArgs);
	}
}

#endif


#if (defined(DEBUG) && defined(STANDALONE)) || defined(METRICS)
/******************************************************************************
 * Function Name: DebugMessageFn
 *
 * Inputs       : pszFormat
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : Prints out a debug message.
 *****************************************************************************/
IMG_INTERNAL IMG_VOID DebugMessageFn(const IMG_CHAR *pszFormat, ...)
{
	IMG_CHAR acScratchBuffer[DEBUG_SCRATCH_BUFFER_SIZE];


	va_list vaArgs;

	va_start (vaArgs, pszFormat);

	vsprintf (acScratchBuffer, pszFormat, vaArgs);

	va_end (vaArgs);

	/* Windows (and possibly Linux) only */
	printf("%s", acScratchBuffer);
}

/******************************************************************************
 * Function Name: DebugMessageWithLineInfo 
 *
 * Inputs       : psToken, pszFormat
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : Prints out a debug message with line information.
 *****************************************************************************/
IMG_INTERNAL IMG_VOID DebugMessageWithLineInfo(Token *psToken, const IMG_CHAR *pszFormat, ...)
{
	va_list vaArgs;
	IMG_UINT32 i = 0;
	IMG_CHAR acScratchBuffer[DEBUG_SCRATCH_BUFFER_SIZE];

	if (!psToken)
	{
		DEBUG_MESSAGE(("DebugMessageWithLineInfo: Cannot dump token info, pointer is NULL\n"));
		return;
	}

	va_start (vaArgs, pszFormat);

	sprintf(acScratchBuffer, "%u:%u: ", 0, psToken->uLineNumber+1);

	vsprintf (&acScratchBuffer[strlen(acScratchBuffer)], pszFormat, vaArgs);

	va_end (vaArgs);

	DEBUG_MESSAGE((acScratchBuffer));

	while (i < (DEBUG_SCRATCH_BUFFER_SIZE - 10) && 
		   psToken->pszStartOfLine[i] != '\n' && 
		   psToken->pszStartOfLine[i] != '\r' && 
		   psToken->pszStartOfLine[i] != '\0')
	{
		acScratchBuffer[i] = psToken->pszStartOfLine[i];
		i++;
	}

	acScratchBuffer[i] = '\0';

	DEBUG_MESSAGE(("%s\n", acScratchBuffer));

	for (i = 0; i < psToken->uCharNumber; i++)
	{
		if (psToken->pszStartOfLine[i] == '\t')
		{
			DEBUG_MESSAGE(("\t"));
		}
		else
		{
			DEBUG_MESSAGE((" "));
		}
	}

	DEBUG_MESSAGE(("^\n"));

}
#endif /* defined(DEBUG) && defined(STANDALONE) */

/******************************************************************************
 End of file (debug.c)
******************************************************************************/
