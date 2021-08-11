/**************************************************************************
 * Name         : binshader.h
 * Created      : 6/07/2006
 *
 * Copyright    : 2000-2005 by Imagination Technologies Limited. All rights reserved.
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
 * $Log: binshader.h $
 */

#ifndef _BINSHADER_H_
#define _BINSHADER_H_

/*
** Creates a UniFlex-compatible shader from its binary representation.
**
** Allocates memory for the output in *ppsShader.
** Returns SGXBS_NO_ERROR if successful.
*/
IMG_INTERNAL SGXBS_Error SGXBS_CreateSharedShaderState(	GLES2Context *gc,
														const IMG_VOID *pvBinaryShader,
														IMG_INT32 i32BinaryShaderLengthInBytes,
														IMG_BOOL bExpectingVertexShader,
														IMG_BOOL bCheckDDKVersion,
														IMG_VOID *pvUniPatchContext,
														GLES2SharedShaderState **ppsSharedState);

IMG_INTERNAL SGXBS_Error SGXBS_CreateProgramState(	GLES2Context *gc, 
													const IMG_VOID* pvBinaryProgram,
													IMG_INT32 i32BinaryShaderLengthInBytes, 
													IMG_VOID *pvUniPatchContext, 
													GLES2SharedShaderState **ppsVertexState, 
													GLES2SharedShaderState **ppsFragmentState, 
													GLSLAttribUserBinding **ppsUserBinding);

#endif /* !defined _BINSHADER_H_ */
