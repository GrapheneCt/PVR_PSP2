/**************************************************************************
 * Name         : astbuiltin.h
 * Author       : James McCarthy
 * Created      : 16/08/2004
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
 * $Log: astbuiltin.h $
 **************************************************************************/

#ifndef __gl_astbuiltin_h_
#define __gl_astbuiltin_h_

/* Allows additional functionaliy beyond the GLSL specs */
typedef enum GLSLExtendFunctionalityTAG
{
	GLSL_EXTENDFUNC_ALLOW_TEXTURE_LOD_IN_FRAGMENT  = 0x00000001,
	GLSL_EXTENDFUNC_SUPPORT_TEXTURE_GRAD_FUNCTIONS = 0x00000002,
	GLSL_EXTENDFUNC_SUPPORT_TEXTURE_STREAM		   = 0x00000004,
	GLSL_EXTENDFUNC_SUPPORT_TEXTURE_EXTERNAL	   = 0x00000008,
} GLSLExtendFunctionality;

extern IMG_INT32 iGLMaxLights;
extern IMG_INT32 iGLMaxClipPlanes;
extern IMG_INT32 iGLMaxTextureUnits;
extern IMG_INT32 iGLMaxTextureCoords;
extern IMG_INT32 iGLMaxVertexAttribs;
extern IMG_INT32 iGLMaxVertexUniformComponents;
extern IMG_INT32 iGLMaxVaryingFloats ;
extern IMG_INT32 iGLMaxVertexTextureImageUnits;
extern IMG_INT32 iGLMaxCombinedTextureImageUnits;
extern IMG_INT32 iGLMaxTextureImageUnits;
extern IMG_INT32 iGLMaxFragmentUniformComponents;
extern IMG_INT32 iGLMaxDrawBuffers;

IMG_BOOL ASTBIAddBuiltInData(GLSLCompilerPrivateData *psCPD,
							 SymTable                *psSymbolTable, 
							 Token                   *psTokenList, 
							 GLSLProgramType          eProgramType,
							 GLSLExtendFunctionality  eExtendFunc,
							 GLSLRequestedPrecisions *psRP,
							 GLSLCompilerResources   *psCR);

IMG_BOOL ASTBIResetBuiltInData(GLSLCompilerPrivateData *psCPD, SymTable *psSymbolTable, GLSLIdentifierList *psBuiltInsReferenced);

#endif // __gl_astbuiltin_h_
