/**************************************************************************
 * Name         : parser.h
 * Author       : James McCarthy
 * Created      : 26/11/2003
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
 * $Log: parser.h $
 **************************************************************************/
#ifndef __gl_parser_h_
#define __gl_parser_h_


#include "lex.h"
#include "symtab.h"
#include "img_types.h"
#include "debug.h"
#include "../glsl/glsl.h"


typedef IMG_VOID (IMG_CALLCONV *PFN_INTERNAL_ERROR)(Token*,  const IMG_CHAR *pszFormat, ...);
typedef IMG_VOID (IMG_CALLCONV *PFN_PROGRAM_ERROR)(Token*,  const IMG_CHAR *pszFormat, ...);

#ifdef COMPACT_MEMORY_MODEL

#define PTE_SYNTAX_TABLE_POS_UNINIT 0x000003FF

//IMG_BOOL CheckGrammarCanBeCompacted(LanguageContext *psLanguageContext);

#else

#define PTE_SYNTAX_TABLE_POS_UNINIT 0xFFFFFFFF

#endif

#define ParseTreeEntry Token
#define psParseTreeEntry psToken

typedef struct ParseContextTAG
{
	GLSLCompilerPrivateData *psCPD;
	IMG_CHAR           **ppszSources;
	IMG_UINT32           uNumSources;
	Token               *psTokenList;
	IMG_UINT32           uNumTokens;
	IMG_UINT32           uCurrentToken;
	IMG_VOID			*pvPreProcessorData;
	/* Memory allocated by the bison rule handlers is recorded in a linked list. */
	IMG_VOID			**pvBisonAllocList;
} ParseContext;



ParseContext *CreateParseContext(IMG_VOID    *pvCompilerPrivateData,
								 IMG_CHAR   **ppszSourceCode,
								 IMG_UINT32   uNumSources);

IMG_VOID DestroyParseContext(ParseContext *psParseContext);

IMG_BOOL DestroyParseTree(ParseContext *psParseContext);


#endif /* __gl_parser_h_ */
