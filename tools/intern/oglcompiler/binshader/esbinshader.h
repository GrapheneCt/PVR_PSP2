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
 * $Log: esbinshader.h $
 */

#ifndef _ESBINSHADER_H_
#define _ESBINSHADER_H_

#include "../powervr/glsl2uf.h"

/* some defines to be used for asserting that our assumptions on the binary size are correct */
#define SGXBS_METADATA_1 2
#define SGXBS_METADATA_2 6
/*
** Error type returned by the entry points in this module.
*/
typedef enum _SGXBS_Error
{
	/* No error. The operation finished successfully                          */
	SGXBS_NO_ERROR                  = 0x00000000,

	/* Internal error. Something went wrong but it was not the caller's fault */
	SGXBS_INTERNAL_ERROR            = 0x00000001,

	/* Out of memory. At least one memory allocation failed                   */
	SGXBS_OUT_OF_MEMORY_ERROR       = 0x00000002,

	/* At least one of the arguments to the function is invalid               */
	SGXBS_INVALID_ARGUMENTS_ERROR   = 0x00000003,

	/* The binary is corrupt                                                    */
	SGXBS_CORRUPT_BINARY_ERROR            = 0x00000004,

	/* The binary shader does not include a revision for this hardware/software */
	SGXBS_MISSING_REVISION_ERROR          = 0x00000005

} SGXBS_Error;

/* 
	This structure was previuously GLES2AttribUserBinding, and was defined in the GLES2 driver;
	it's been renamed and moved here because it's needed by SGXBS_CreateBinaryProgram
*/
typedef struct GLSLAttribUserBindingRec
{
	IMG_CHAR	*pszName;
	IMG_INT32	i32Index;

	/* linked list */
	struct GLSLAttribUserBindingRec *psNext;

}GLSLAttribUserBinding;


/*
** Creates the binary representation of a compiled Uniflex shader. Allocates memory for the output in
** *ppvBinaryShader and specifies the length in *pu32BinaryShaderLengthInBytes.
** Returns SGXBS_NO_ERROR if successful.
*/
IMG_IMPORT 
SGXBS_Error SGXBS_CreateBinaryShader(const GLSLCompiledUniflexProgram*   psShader,
									IMG_VOID*  (*pfnMalloc)(IMG_UINT32),
									IMG_VOID   (*pfnFree)(IMG_VOID*),
									IMG_VOID** ppvBinaryShader,
									IMG_UINT32* pu32BinaryShaderLengthInBytes);

IMG_IMPORT 
SGXBS_Error SGXBS_CreateBinaryProgram(const GLSLCompiledUniflexProgram* psVertex, 
									  const GLSLCompiledUniflexProgram* psFragment, 
									  const GLSLAttribUserBinding* psUserBinding,
									  IMG_UINT32 u32BinarySizeInBytes, 
									  IMG_UINT32 * ui32Length, 
									  IMG_VOID* pvBinaryShader, 
									  IMG_BOOL bCreateBinary);


#endif /* !defined _ESBINSHADER_H_ */
