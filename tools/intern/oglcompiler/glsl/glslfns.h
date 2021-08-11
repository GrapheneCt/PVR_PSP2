/**************************************************************************
 * Name         : glslfns.c
 * Author       : James McCarthy
 * Created      : 16/05/2006
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
 * $Log: glslfns.h $
 **************************************************************************/
#ifndef __gl_glslfns_h_
#define __gl_glslfns_h_

IMG_BOOL EmulateBuiltInFunction(GLSLCompilerPrivateData *psCPD,
								GLSLBuiltInFunctionID  eBFID,
								GLSLTypeSpecifier     *peTypeSpecifiers,
								const IMG_VOID        **pvData, 
								IMG_VOID              *pvResult,
								GLSLTypeSpecifier     *peResultTypeSpecifier);

#endif // __gl_glslfns_h_
