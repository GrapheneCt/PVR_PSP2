/**************************************************************************
 * Name         : icbuiltin.h
 * Created      : 24/06/2004
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
 * $Log: icbuiltin.h $
 **************************************************************************/

#ifndef __gl_icbuiltin_h_
#define __gl_icbuiltin_h_

#include "icgen.h"

/* 
** Generate intermediate code for any built-in function. 
*/
IMG_VOID ICProcessBuiltInFunctionCall(GLSLCompilerPrivateData *psCPD,
									  GLSLICProgram *psICProgram, 
									   GLSLNode *psNode, 
									   GLSLICOperandInfo *psDestOperand);

IMG_UINT32 ICFindBuiltInVariables(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, IMG_CHAR *pszBIVaribleName);

#endif /* __gl_icbuiltin_h_ */

