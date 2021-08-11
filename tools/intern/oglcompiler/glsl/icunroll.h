/**************************************************************************
 * Name         : icunroll.h
 * Created      : 11/03/2005
 *
 * Copyright    : 2004 by Imagination Technologies Limited. All rights reserved.
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
 * $Log: icunroll.h $
 **************************************************************************/
IMG_VOID ICUnrollLoopFOR(GLSLCompilerPrivateData *psCPD,
						 GLSLICProgram      *psICProgram, 
						 IMG_BOOL			bUnrollLoop,
						 GLSLICInstruction	*psInitStart,
						 GLSLICInstruction	*psInitEnd,
						 GLSLICInstruction	*psCondStart, 
						 GLSLICInstruction	*psCondEnd,
						 GLSLICInstruction	*psUpdateStart,
						 GLSLICInstruction	*psUpdateEnd);

