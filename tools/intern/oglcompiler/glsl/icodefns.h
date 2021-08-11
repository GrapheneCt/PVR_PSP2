/**************************************************************************
 * Name         : icodefns.h
 * Created      : 16/02/2006
 *
 * Copyright    : 2000-2006 by Imagination Technologies Limited. All rights reserved.
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
 * $Log: icodefns.h $
 **************************************************************************/

#ifndef _icodefns_h_
#define _icodefns_h_

GLSLICProgram *GenerateICodeProgram(GLSLCompilerPrivateData *psCPD,
									GLSLTreeContext	*psTreeContext, 
									SymTable		*psSymbolTable,
									ErrorLog        *psErrorLog);

IMG_VOID DestroyICodeProgram(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICodeProgram);

IMG_UINT32 GetICInstructionCount(GLSLICProgram *psICProgram);

#endif // _icodefns_h_
