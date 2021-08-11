#ifndef __gl_lex_h_
#define __gl_lex_h_

#include "tokens.h"
#include "img_types.h"

Token *LexCreateTokenList(IMG_VOID *pvCompilerPrivateData,
						  const IMG_CHAR                *ppszSources[],
						  IMG_UINT32                uNumSources,
						  IMG_UINT32               *puNumTokens,
   						  IMG_VOID				  **ppvPreProcessorData);


IMG_VOID LexDestroyTokenList(Token *psTokenList, IMG_UINT32 uNumTokens);


#endif /*  __gl_lex_h_ */
