/**************************************************************************
 * Name         : errorlog.h
 * Author       : James McCarthy
 * Created      : 17/02/2006
 *
 * Copyright    : 2000-2006 by Imagination Technologies Limited. All rights reserved.
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
 * $Log: errorlog.h $
 **************************************************************************/

#ifndef __gl_errorlog_h_
#define __gl_errorlog_h_

#include "img_types.h"

#define MAX_ERROR_MESSAGES 100

typedef enum ErrorTypeTAG {
	ERRORTYPE_INTERNAL        = 0x1,
	ERRORTYPE_PROGRAM_ERROR   = 0x2,
	ERRORTYPE_PROGRAM_WARNING = 0x4,
	ERRORTYPE_ALL             = 0x7,
} ErrorType;
	
typedef struct ErrorMessageTAG
{
	ErrorType   eErrorType;
	IMG_CHAR   *pszErrorMessageString;
	IMG_CHAR   *pszStartOfLine;
	IMG_UINT32  uCharNumber;
} ErrorMessage;

typedef struct ErrorLogTAG
{
	ErrorMessage  sErrorMessages[MAX_ERROR_MESSAGES];
	IMG_UINT32    uNumProgramErrorMessages;
	IMG_UINT32    uNumProgramWarningMessages;
	IMG_UINT32    uNumInternalErrorMessages;
	IMG_UINT32    uTotalNumErrorMessages;
	IMG_BOOL      bDisplayMessages;
} ErrorLog;

#endif // _gl_errorlog_h_
