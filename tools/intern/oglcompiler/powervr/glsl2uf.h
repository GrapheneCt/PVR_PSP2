/**************************************************************************
 * Name         : glsl2uf.h
 * Author       : James McCarthy
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
 * $Log: glsl2uf.h $
 **************************************************************************/
#ifndef __gl_glsl2uf_h_
#define __gl_glsl2uf_h_

#include "glsl.h"
#include "usp.h"

/*
	Collected data representing uniflex code generated from intermediate-code
*/
typedef struct GLSLUniFlexCodeTAG
{
	/* NOTE: The desktop OpenGL driver uses all the variables in this struct,
	 *       with the exception of psUniPatchInput/psUniFlexHW.
	 *       However, the OpenGL ES 2.0 driver only needs a few of them. They are marked for clarity.
	 */

	/* *** Variables only used by the desktop OpenGL driver follow *** */

	/* Pointer to the low-level code, Uniflex input code. */
	UNIFLEX_INST			*pvUFCode;

	/*
		Ranges of constants which can be accessed with dynamic indices.
	*/
	UNIFLEX_RANGES_LIST		sConstRanges;

	/*
		Ranges of varyings which can be accessed with dynamic indices.
	*/
	UNIFLEX_RANGES_LIST		sVaryingRanges;

	/* 
		Static constant flags indicating whether a constant is static, 
		the total size in bytes for puConstStaticFlags is ROUND_UP(uConstStaticFlagCount, 32)/8 
	*/
	IMG_UINT32				uConstStaticFlagCount;
	IMG_UINT32				*puConstStaticFlags;

	/* Number of indexable temporary arrays and the size for the array */
	IMG_UINT32					uNumIndexableTempArrays;
	UNIFLEX_INDEXABLE_TEMP_SIZE	*psIndexableTempArraySizes;

	/* Centroid mask		*/
	IMG_UINT32				uTexCoordCentroidMask;

	/* Sampler count */
	IMG_UINT32				uSamplerCount;

	/* Sampler dimension	*/
	PUNIFLEX_DIMENSIONALITY	asSamplerDims;

	/* *** Variables accessed by the OpenGL ES 2.0 driver follow (also needed by the desktop driver) *** */

	/* Indicate there are multiple varyings which are packed into one row */
	IMG_BOOL				bVaryingsPacked;

	/* Active varying mask */
	GLSLVaryingMask			eActiveVaryingMask;

	/* Texture coordinate dimensions */
	IMG_UINT32				auTexCoordDims[NUM_TC_REGISTERS];

	/* Texture coordinate precisions */
	GLSLPrecisionQualifier	aeTexCoordPrecisions[NUM_TC_REGISTERS];

	/* *** Variables used exclusively by the OpenGL ES 2.0 driver *** */

	/* The OpenGL driver can use both uniflex and usp outputs at the same time,
	   when supporting the binary shader extension. therefore, even if OUTPUT_USPBIN  
	   is undefined, the following psUniPatch* pointers shoud be present.
	*/ 

#if defined(OUTPUT_USPBIN)
	/* UniPatch code input */
	USP_PC_SHADER          *psUniPatchInput;
	
	/* UniPatch code input for multisample antialias with translucency */
	USP_PC_SHADER          *psUniPatchInputMSAATrans;

	#ifdef SRC_DEBUG
	/* 
		UNIFLEX outputs. 
		Will be only used to create source cycle count table. 
	*/
	UNIFLEX_HW        *psUniFlexHW;
	#endif /* SRC_DEBUG */

#else

	/* UniPatch code input */
	IMG_VOID          *psUniPatchInput;
	
	/* UniPatch code input for multisample antialias with translucency */
	IMG_VOID          *psUniPatchInputMSAATrans;
	
	IMG_UINT32		  ui32UniPatchInputSize;
	IMG_UINT32		  ui32UniPatchInputMSAATransSize;
	

	/* UNIFLEX outputs     */
	UNIFLEX_HW        *psUniFlexHW;


#endif

} GLSLUniFlexCode;

/*
** All information required to generate the uniflex program 
*/
typedef struct GLSLCompileUniflexProgramContextTAG
{
	/* What type of uniflex code is required */
	GLSLProgramFlags           eOutputCodeType;

#if defined(OUTPUT_USPBIN)
	IMG_BOOL					bCompileMSAATrans;
#endif

	/* Information required to generate uniflex output code */
	GLSLUniFlexHWCodeInfo     *psUniflexHWCodeInfo;

	/* Information about the source program */
    GLSLCompileProgramContext *psCompileProgramContext;

} GLSLCompileUniflexProgramContext;

/* 
** The compiled program structure returned to the driver after compilation. 
*/
typedef struct GLSLCompiledUniflexProgramTAG
{
	/* Copied from the GLSLCompileProgramContext */
	GLSLProgramType          eProgramType;
	GLSLProgramFlags		 eProgramFlags;
	
	/* Did program successfully compile */
	IMG_BOOL                 bSuccessfullyCompiled;

	GLSLInfoLog              sInfoLog;

	/* Pointer to uniflex code */
	GLSLUniFlexCode	        *psUniFlexCode;

	/* List of binding symbols */
	GLSLBindingSymbolList   *psBindingSymbolList;

	/* Context this program was created with */
	GLSLCompileUniflexProgramContext *psCompileUniflexProgramContext;

} GLSLCompiledUniflexProgram;


IMG_IMPORT GLSLCompiledUniflexProgram * IMG_CALLCONV GLSLCompileToUniflex(GLSLCompileUniflexProgramContext *psCompileUniflexProgramContext);

IMG_IMPORT IMG_VOID IMG_CALLCONV GLSLFreeCompiledUniflexProgram(GLSLInitCompilerContext *psInitCompilerContext,
																GLSLCompiledUniflexProgram *psGLSLCompiledUniflexProgram);

IMG_IMPORT IMG_VOID IMG_CALLCONV GLSLDisplayMetrics(GLSLInitCompilerContext *psInitCompilerContext);


#endif // __gl_glsl2uf_h_

