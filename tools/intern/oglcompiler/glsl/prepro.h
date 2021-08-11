/**************************************************************************
 * Name         : prepro.h
 * Author       : James McCarthy
 * Created      : 25/11/2004
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
 * $Log: prepro.h $
 **************************************************************************/

#ifndef __gl_prepro_h_
#define __gl_prepro_h_

#include "img_types.h"

#include "../parser/tokens.h"

typedef struct GLSLPreProcessorDataTAG
{
	GLSLExtension eEnabledExtensions;
}GLSLPreProcessorData;

IMG_BOOL PPPreProcessTokenList(GLSLCompilerPrivateData *psCPD,
							   IMG_VOID *pvPreproData,
							 Token     *psTokenList,
							 IMG_UINT32   uNumTokens,
							 Token    **ppsNewTokenList,
							 IMG_UINT32  *puNewNumTokens);

IMG_BOOL PPDestroyPreProcessorData(IMG_VOID *pvPreproData);

#endif //__gl_prepro_h_
