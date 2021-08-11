/**************************************************************************
 * Name         : uniform.h
 *
 * Copyright    : 2006 by Imagination Technologies Limited. All rights reserved.
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
 * $Log: uniform.h $
 **************************************************************************/

#ifndef _UNIFORM_
#define _UNIFORM_

/* Clear mask for uniforms less than 32 bits */
#define CLEARMASK(SHIFT, BITS)	(((1U << SHIFT) - 1) | ~(((1U << SHIFT) << BITS) - 1))

typedef struct GLES2UniformRec
{
	IMG_CHAR			*pszName;

	/* Place where the first element of this uniform (think arrays) will be stored. */
	/* In other words, the location returned by glGetUniformLocation() */
	IMG_INT32			i32Location;
	IMG_UINT32			ui32ActiveArraySize;
	IMG_UINT32			ui32DeclaredArraySize;
	GLSLTypeSpecifier	eTypeSpecifier;
	GLSLPrecisionQualifier	ePrecisionQualifier; 

	/* The texture unit this uniform is reading from, if GLES2_IS_SAMPLER(eTypeSpecifier) is true */
	IMG_UINT32          ui32VPSamplerIndex;
	IMG_UINT32          ui32FPSamplerIndex;
	
	/* If the uniform is active in the vertex shader, this variable will be non-null */
	GLSLBindingSymbol	*psSymbolVP;

	/* If the uniform is active in the fragment shader, this variable will be non-null */
	GLSLBindingSymbol	*psSymbolFP;

} GLES2Uniform;

typedef struct GLES2BuiltInUniformRec
{	
	GLSLBuiltInVariableID eBuiltinVariableID; 
	GLSLBindingSymbol	  *psSymbolVP;
	GLSLBindingSymbol	  *psSymbolFP;

}GLES2BuiltInUniform;

GLES2Uniform *FindUniformFromLocation(GLES2Context *gc, GLES2Program *psProgram, IMG_INT32 i32Location);
IMG_VOID GetUniformData(GLES2Context *gc, GLES2Program *psProgram, GLES2Uniform *psUniform, 
						IMG_INT32 i32Location, IMG_UINT32 *pui32NumFloats, IMG_FLOAT *pfDstData);

GLenum ConvertGLSLtoGLType(GLSLTypeSpecifier eGLSLType);

GLES2_MEMERROR WriteUSEShaderMemConsts(GLES2Context *gc, IMG_UINT32 ui32ProgramType);
IMG_VOID SetupBuiltInUniforms(GLES2Context *gc, IMG_UINT32 ui32ProgramType);

#endif /* _SHADER_ */
