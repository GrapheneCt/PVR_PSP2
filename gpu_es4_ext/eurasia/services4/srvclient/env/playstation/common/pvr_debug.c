#include <kernel.h>
#include <libdbg.h>

#include "eurasia/include4/services.h"
#include "eurasia/include4/pvr_debug.h"

static IMG_UINT32 gPVRDebugLevel = DBGPRIV_CALLTRACE;

static IMG_VOID PVRSRVInheritEnvironmentDebugLevel(IMG_VOID)
{
	gPVRDebugLevel = DBGPRIV_FATAL | DBGPRIV_ERROR | DBGPRIV_WARNING | DBGPRIV_MESSAGE | DBGPRIV_VERBOSE | DBGPRIV_CALLTRACE;
}

/*----------------------------------------------------------------------------
<function>
	FUNCTION   : PVRDebugPrintf
	PURPOSE    : To output a debug message to the user
	PARAMETERS : In : uDebugLevel - The current debug level
				 In : pszFile - The source file generating the message
				 In : uLine - The line of the source file
				 In : pszFormat - The message format string
				 In : ... - Zero or more arguments for use by the format string
	RETURNS    : None
</function>
------------------------------------------------------------------------------*/
IMG_EXPORT IMG_VOID PVRSRVDebugPrintf(IMG_UINT32 ui32DebugLevel,
	const IMG_CHAR *pszFileName,
	IMG_UINT32 ui32Line,
	const IMG_CHAR *pszFormat,
	...)
{
	IMG_CHAR szBuffer[PVR_MAX_DEBUG_MESSAGE_LEN];
	IMG_CHAR *szBufferEnd = szBuffer;
	IMG_CHAR *szBufferLimit = szBuffer + sizeof(szBuffer) - 1;
	IMG_CHAR *pszLeafName;
	IMG_BOOL bTrace;
	va_list vaArgs;

	/* The Limit - End pointer arithmetic we're doing in snprintf
	   ensures that our buffer remains null terminated from this */
	*szBufferLimit = '\0';

	PVRSRVInheritEnvironmentDebugLevel();

	pszLeafName = (IMG_CHAR *)sceClibStrrchr(pszFileName, '/');

	if (pszLeafName)
	{
		pszFileName = pszLeafName;
	}

	bTrace = (IMG_BOOL)(ui32DebugLevel & DBGPRIV_CALLTRACE) ? IMG_TRUE : IMG_FALSE;

	if (!(gPVRDebugLevel & ui32DebugLevel))
	{
		return;
	}

	/* Add in the level of warning */
	sceClibSnprintf(szBufferEnd, szBufferLimit - szBufferEnd, "PVR:");
	szBufferEnd += sceClibStrnlen(szBufferEnd, PVR_MAX_DEBUG_MESSAGE_LEN);
	if (bTrace == IMG_FALSE)
	{
		switch (ui32DebugLevel)
		{
		case DBGPRIV_FATAL:
		{
			sceClibSnprintf(szBufferEnd, szBufferLimit - szBufferEnd, "(Fatal):");
			break;
		}

		case DBGPRIV_ERROR:
		{
			sceClibSnprintf(szBufferEnd, szBufferLimit - szBufferEnd, "(Error):");
			break;
		}

		case DBGPRIV_WARNING:
		{
			sceClibSnprintf(szBufferEnd, szBufferLimit - szBufferEnd, "(Warning):");
			break;
		}

		case DBGPRIV_MESSAGE:
		{
			sceClibSnprintf(szBufferEnd, szBufferLimit - szBufferEnd, "(Message):");
			break;
		}

		case DBGPRIV_VERBOSE:
		{
			sceClibSnprintf(szBufferEnd, szBufferLimit - szBufferEnd, "(Verbose):");
			break;
		}

		default:
		{
			sceClibSnprintf(szBufferEnd, szBufferLimit - szBufferEnd, "(Unknown message level):");
			break;
		}
		}
		szBufferEnd += sceClibStrnlen(szBufferEnd, PVR_MAX_DEBUG_MESSAGE_LEN);
	}
	sceClibSnprintf(szBufferEnd, szBufferLimit - szBufferEnd, " ");
	szBufferEnd += sceClibStrnlen(szBufferEnd, PVR_MAX_DEBUG_MESSAGE_LEN);

	va_start(vaArgs, pszFormat);
	sceClibVsnprintf(szBufferEnd, szBufferLimit - szBufferEnd, pszFormat, vaArgs);
	va_end(vaArgs);
	szBufferEnd += sceClibStrnlen(szBufferEnd, PVR_MAX_DEBUG_MESSAGE_LEN);

	/*
	 * Metrics and Traces don't need a location
	 */
	if (bTrace == IMG_FALSE)
	{
		sceClibSnprintf(szBufferEnd, szBufferLimit - szBufferEnd,
			" [%d, %s]", (IMG_INT)ui32Line, pszFileName);
		szBufferEnd += sceClibStrnlen(szBufferEnd, PVR_MAX_DEBUG_MESSAGE_LEN);
	}

	sceClibPrintf("%s\n", szBuffer);
}

/*----------------------------------------------------------------------------
<function>
	FUNCTION   : PVRDebugAssertFail
	PURPOSE    : To indicate to the user that a debug assertion has failed and
				 to prevent the program from continuing.
	PARAMETERS : In : pszFile - The name of the source file where the assertion failed
				 In : uLine - The line number of the failed assertion
	RETURNS    : NEVER!
</function>
------------------------------------------------------------------------------*/
IMG_EXPORT IMG_VOID PVRSRVDebugAssertFail(const IMG_CHAR *pszFile, IMG_UINT32 uLine)
{
	PVRSRVDebugPrintf(DBGPRIV_FATAL, pszFile, uLine, "Debug assertion failed!");
	sceKernelExitProcess(-1);
}

#if defined(PVRSRV_NEED_PVR_TRACE)

/*----------------------------------------------------------------------------
<function>
	FUNCTION   : PVRTrace
	PURPOSE    : To output a debug message to the user
	PARAMETERS : In : pszFormat - The message format string
				 In : ... - Zero or more arguments for use by the format string
	RETURNS    : None
</function>
------------------------------------------------------------------------------*/
IMG_EXPORT IMG_VOID PVRSRVTrace(const IMG_CHAR *pszFormat, ...)
{
	IMG_CHAR szMessage[PVR_MAX_DEBUG_MESSAGE_LEN];
	IMG_CHAR *szMessageEnd = szMessage;
	IMG_CHAR *szMessageLimit = szMessage + sizeof(szMessage) - 1;
	va_list ArgList;

	/* The Limit - End pointer arithmetic we're doing in snprintf
	   ensures that our buffer remains null terminated from this */
	*szMessageLimit = '\0';

	sceClibSnprintf(szMessageEnd, szMessageLimit - szMessageEnd, "PVR: ");
	szMessageEnd += sceClibStrnlen(szMessageEnd, PVR_MAX_DEBUG_MESSAGE_LEN + 1);

	va_start(ArgList, pszFormat);
	sceClibVsnprintf(szMessageEnd, szMessageLimit - szMessageEnd, pszFormat, ArgList);
	va_end(ArgList);
	szMessageEnd += sceClibStrnlen(szMessageEnd, PVR_MAX_DEBUG_MESSAGE_LEN + 1);

	sceClibSnprintf(szMessageEnd, szMessageLimit - szMessageEnd, "\n");
	szMessageEnd += sceClibStrnlen(szMessageEnd, PVR_MAX_DEBUG_MESSAGE_LEN + 1);

	sceClibPrintf(szMessage);
}

#endif /* defined(PVRSRV_NEED_PVR_TRACE) */