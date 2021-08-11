/**************************************************************************
 * Name         : tokens.h
 * Author       : James McCarthy
 * Created      : 25/11/2003
 *
 * Copyright    : 2000-2003 by Imagination Technologies Limited. All rights reserved.
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
 * $Log: tokens.h $
 **************************************************************************/

#ifndef __gl_tokens_h_
#define __gl_tokens_h_

#include "img_types.h"

#define TokenName enum yytokentype
#include "glsl_parser.tab.h"

#ifdef SRC_DEBUG
	#define COMP_UNDEFINED_SOURCE_LINE	(IMG_UINT32)(-1)
#endif /* SRC_DEBUG */

typedef struct TokenTAG
{
	TokenName		eTokenName;
	IMG_CHAR		*pszStartOfLine;
	IMG_UINT32      uStringNumber;
	IMG_UINT32      uLineNumber;
	IMG_UINT32      uCharNumber;
	IMG_UINT32      uSizeOfDataInBytes;
	/* Changed from void* to char* so debuggers let you see what the string value is. */
	IMG_CHAR		*pvData;
} Token;

#endif /* __gl_tokens_h_ */
