/**************************************************************************
 * Name         : shader.h
 *
 * Copyright    : 2005-2006 by Imagination Technologies Limited. All rights reserved.
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
 * $Log: shader.h $
 **************************************************************************/

#ifndef _SHADER_
#define _SHADER_

#define GLES2_SHADERTYPE_PROGRAM	0
#define GLES2_SHADERTYPE_VERTEX		1
#define GLES2_SHADERTYPE_FRAGMENT	2

typedef enum GLES2SpecialTAG
{
	GLES2_SPECIAL_POSITION		=		0, /* Vertex */
	GLES2_SPECIAL_POINTSIZE		=		1, /* Vertex */
	GLES2_SPECIAL_FRAGCOORD		=		2, /* Fragment */
	GLES2_SPECIAL_POINTCOORD	=		3, /* Fragment */
	GLES2_SPECIAL_FRONTFACING	=		4, /* Fragment */
	GLES2_SPECIAL_FRAGCOLOR		=		5, /* Fragment */
	GLES2_SPECIAL_FRAGDATA		=		6, /* Fragment */

	GLES2_NUM_SPECIALS			=		7 /* must be last */
} GLES2Special;

#define GLES2_IS_SAMPLER(a)  ((a == GLSLTS_SAMPLER2D) || (a == GLSLTS_SAMPLERCUBE) || (a == GLSLTS_SAMPLERSTREAM) || (a == GLSLTS_SAMPLEREXTERNAL))
#define GLES2_IS_PERM_TEXTURE_SAMPLER(builtinID) (builtinID == GLSLBV_PMXPERMTEXTURE)
#define GLES2_IS_GRAD_TEXTURE_SAMPLER(builtinID) (builtinID == GLSLBV_PMXGRADTEXTURE)


/* The maximum number of DOUTIs that can be issued by the PDS to iterate data.
   Enough for 16 texture samples, 10 texcoords, base colour, offset colour,
   fog, vertex position, plane coefficients. */
#define GLES2_MAX_DOUTI	(USC_MAXIMUM_NON_DEPENDENT_TEXTURE_LOADS + USC_MAXIMUM_ITERATIONS)

#define GLES2_MAX_LINK_MESSAGE_LENGTH 256

typedef struct GLES2PDSInfo_TAG
{
	/*
		Limits on the PDS pixel shader program task size.
	*/
	IMG_UINT32				ui32DMSInfo;
	

#if defined(EUR_CR_ISP_TAGCTRL_TRIANGLE_FRAGMENT_PIXELS_MASK)
	IMG_UINT32				ui32TriangleSplitPixelThreshold;
#endif


	/* Used to setup the DOUTI commands (goes into adwFPUIterators) */
#if defined(SGX545)
	IMG_UINT32              aui32TSPParamFetchInterface0[GLES2_MAX_DOUTI];		/* pdwTSPCoordCtl */
	IMG_UINT32              aui32TSPParamFetchInterface1[GLES2_MAX_DOUTI];		/* pdwTSPCoordCtl */
#else /* defined(SGX545) */
	IMG_UINT32              aui32TSPParamFetchInterface[GLES2_MAX_DOUTI];		/* pdwTSPCoordCtl */
#endif /* defined(SGX545) */ 

	IMG_UINT32              aui32LayerControl[GLES2_MAX_DOUTI];

	IMG_UINT32              ui32NonDependentTextureCount;
	IMG_UINT32              ui32IterationCount;
	IMG_UINT32				ui32DependencyControl;
		

	IMG_UINT32				ui32NonDependentImageUnits; /* Bit field indicating which image units are enabled */

} GLES2PDSInfo;


struct GLES2PDSCodeVariant_TAG
{
	/* PDS variant are 3D-kick resources */
	KRMResource sResource;

	UCH_UseCodeBlock *psCodeBlock;

	IMG_UINT32 ui32DataSize;

	IMG_UINT32 *pui32HashCompare;
	IMG_UINT32 ui32HashCompareSizeInDWords;

	HashValue tHashValue;

	/* USE variant that this PDS variant was compiled from (if one exists). */
	GLES2USEShaderVariant *psUSEVariant;

	/* Next variant in the list. */
	struct GLES2PDSCodeVariant_TAG *psNext;
};


typedef struct GLES2PDSCodeVariantGhost_TAG
{
	UCH_UseCodeBlock	*psCodeBlock;

	/* Next variant in the list of the USE variant ghost*/
	struct GLES2PDSCodeVariantGhost_TAG *psNext;

} GLES2PDSCodeVariantGhost;


typedef struct GLES2ConstantRange_TAG
{
	IMG_UINT32 ui32Start;
	IMG_UINT32 ui32Count;

} GLES2ConstantRange;


typedef struct GLES2ConstantData_TAG
{
	IMG_VOID			*pvConstantData;
	IMG_DEV_VIRTADDR	uCurrentConstantBaseAddress;
	IMG_UINT32			ui32NumConstants;
	GLES2ConstantRange	*psConstRange;
	IMG_UINT32			ui32NumConstRanges;
	IMG_UINT32			ui32MaxNumConstRanges;

} GLES2ConstantData;


typedef struct GLES2USESecondaryUploadTaskTAG
{
	IMG_UINT32         ui32RefCount;             
	UCH_UseCodeBlock  *psSecondaryCodeBlock;

} GLES2USESecondaryUploadTask;


typedef struct GLES2ShaderIndexableTempsMemTAG
{
	IMG_UINT32				ui32RefCount;             
	PVRSRV_CLIENT_MEM_INFO *psMemInfo;

} GLES2ShaderIndexableTempsMem;

typedef struct GLES2ShaderScratchMemTAG
{
	IMG_UINT32				ui32RefCount;             
	PVRSRV_CLIENT_MEM_INFO *psMemInfo;

} GLES2ShaderScratchMem;



/* GLES2USEShaderVariant is basically a piece of machine code that can be run by the USSE.
   It comes from a UniPatch shader after it is finalised.
   For fragment shaders, it contains also a linked list of cached PDS variants
*/
struct GLES2USEShaderVariant_TAG
{
	/* Single-linked list */
	struct GLES2USEShaderVariant_TAG	*psNext;

	/* Pointer to the ProgramShader this variant belongs to (if any) */
	GLES2ProgramShader              *psProgramShader;

	/* USSE code variants are frame resources */
	KRMResource             		sResource;

	/* Information about the phases */
	IMG_DEV_VIRTADDR				sStartAddress[3];
	IMG_UINT32						ui32PhaseCount; 
	
	/* Pointer to the location in device memory of the hardware code for this variant. */
	UCH_UseCodeBlock				*psCodeBlock;

	IMG_UINT32                      ui32MaxTempRegs;
	IMG_UINT32                      ui32USEPrimAttribCount;          /* dwUSENumVertexShaderPARegs       */
	
	union
	{
		struct
		{
			/* Colormask that was used for this variant */
			IMG_UINT32 ui32ColorMask;
	
			/* Blend state that was used for this variant */
			IMG_UINT32 ui32BlendEquation;
			IMG_UINT32 ui32BlendFactor;

			/* Was this variant used with a translucent/transpt object on an MSAA surface */
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
			IMG_BOOL bSeparateBlendPhase;
#else	
			IMG_BOOL bReadOnlyPAs;
#endif

#if defined(FIX_HW_BRN_25077)
			IMG_UINT32 ui32AlphaTestFlags;
#endif

			/* Texture formats that were used for this variant */
			const GLES2TextureFormat *apsTexFormat[GLES2_MAX_TEXTURE_UNITS];
			
			/* Bit field indicating which image units are enabled */
			IMG_UINT32 ui32ImageUnitEnables;

			GLES2PDSInfo sPDSInfo;

			/* Whether the above Data Master Selector info words have been set or not */
			IMG_BOOL bHasSetupDMSInfo;

		} sFragment;

		struct
		{
			IMG_UINT32 aui32StreamTypeSize[GLES2_MAX_VERTEX_ATTRIBS];
			IMG_UINT32 ui32NumItemsPerVertex;

			/* Texture formats that were used for this variant */
			const GLES2TextureFormat *apsTexFormat[GLES2_MAX_TEXTURE_UNITS];
			
			/* Bit field indicating which image units are enabled */
			IMG_UINT32 ui32ImageUnitEnables;

		} sVertex;

		struct
		{
			IMG_UINT32 ui32SideBandWord;

		} sPixelEvent;

		struct
		{
			IMG_UINT32 ui32NumStateWords;
		} sStateCopy;

	} u;

	USP_HW_SHADER *psPatchedShader;

	/* Secondary attributes upload task */
	GLES2USESecondaryUploadTask *psSecondaryUploadTask;

	/* Scratch memory */
	GLES2ShaderScratchMem *psScratchMem;

	/* Indexable temps memory */
	GLES2ShaderIndexableTempsMem *psIndexableTempsMem;

	/* Linked list of PDS variants (only for fragment shaders) */
	GLES2PDSCodeVariant     *psPDSVariant;
	
	/* Number of elements in the list above. NOTE: it is not a contiguous array! */
	IMG_UINT32 ui32NumPDSVariants;
};


typedef struct GLES2USEShaderVariantGhost_TAG
{
	/* USSE code variant ghosts are frame resources */
	KRMResource       		sResource;

	/* Code block used for the primary task */
	UCH_UseCodeBlock        *psUSECodeBlock;

	/* Secondary attributes upload task */
	GLES2USESecondaryUploadTask *psSecondaryUploadTask;

	/* Scratch memory */
	GLES2ShaderScratchMem *psScratchMem;

	/* Indexable temps memory */
	GLES2ShaderIndexableTempsMem *psIndexableTempsMem;

	/* Linked list of PDS variant ghosts */
	GLES2PDSCodeVariantGhost *psPDSVariantGhost;

} GLES2USEShaderVariantGhost;


/* GLES2SharedShaderState represents a GL shader after it has been compiled successfully */
/* It contains data that can be fed to UniPatch to finalise and get hardware machine code */
typedef struct GLES2SharedShaderStateRec
{
	GLSLBindingSymbolList	 sBindingSymbolList;
	GLSLProgramFlags		 eProgramFlags;
	
	/* Active varying mask */
	GLSLVaryingMask			eActiveVaryingMask;

	/* Texture coordinate dimensions */
	IMG_UINT32				aui32TexCoordDims[NUM_TC_REGISTERS];

	/* Texture coordinate precisions */
	GLSLPrecisionQualifier	aeTexCoordPrecisions[NUM_TC_REGISTERS];

	IMG_VOID			*pvUniPatchShader;
	IMG_VOID			*pvUniPatchShaderMSAATrans;

	/* Secondary attributes upload task */
	GLES2USESecondaryUploadTask *psSecondaryUploadTask;

	IMG_UINT32 ui32RefCount;

} GLES2SharedShaderState;


/************************************************************************/
/*                        GLES2 shader state                            */
/*                                                                      */
/*  GLES2Shader represents an application-visible 'shader'.             */
/************************************************************************/

typedef struct GLES2ShaderRec
{
 	/* This struct must be the first variable */
	GLES2NamedItem  sNamedItem;

	/* Must be 2nd in structure to allow casting to Program */
	IMG_UINT32 ui32Type;

	/* Containing the shader strings, initially null */
	IMG_CHAR *pszSource;

	/* An array of type char containing the info log, initially null. */
	IMG_CHAR *pszInfoLog;

	/* State shared between a shader and the programs it is attached to */
	GLES2SharedShaderState *psSharedState;

	IMG_BOOL bSuccessfulCompile;
	IMG_BOOL bDeleting;

} GLES2Shader;


/************************************************************************/
/*								GLES2 program state						*/
/************************************************************************/
typedef struct GLES2AttributeRec
{
	IMG_INT32 i32Index;
	IMG_UINT32 ui32NumIndices;
	
	GLSLBindingSymbol	*psSymbolVP;
}GLES2Attribute;


typedef struct GLES2VaryingRec
{
	IMG_CHAR			*pszName;
	GLSLTypeSpecifier	eTypeSpecifier;
	IMG_UINT32			ui32ActiveArraySize;
	IMG_UINT32			ui32DeclaredArraySize;
	GLSLBindingSymbol	*psSymbolVP;
	GLSLBindingSymbol	*psSymbolFP;
}GLES2Varying;


typedef struct GLES2CopyRangeTAG
{
	IMG_UINT32 ui32Start;
	IMG_UINT32 ui32End;
} GLES2CopyRange;


typedef struct GLES2UniformCopyRangeTAG
{
	GLES2CopyRange sFloatCopyRange;	
	GLES2CopyRange sBoolCopyRange;
	GLES2CopyRange sIntCopyRange;
} GLES2UniformCopyRange;


typedef struct GLES2TextureSamplerRec
{
	IMG_UINT8 ui8SamplerTypeIndex;      /* 2D or cube */

	/* The image unit to get the texture information from. This value is provided by the application */
	IMG_UINT8 ui8ImageUnit;        
	 
	/* The uniform entry that points to this vertex texture sampler */
	GLES2Uniform *psUniformEntry; 
	
} GLES2TextureSampler;


/* GLES2ProgramShader represents a shader after it has been linked to another shader as part of a program */
/* Large parts of it are set during validation. It contains a list of cached USE variants */
struct GLES2ProgramShaderRec
{
	/* if valid, the program is ready to use */
	IMG_BOOL bValid;

	/* Can be vertex or fragment */
	GLSLProgramType         eProgramType;

	/* Pointer to the shared state of the corresponding shader */
	GLES2SharedShaderState *psSharedState;

	/* Special variables, NULL means not active, use GLSLSpecial enums to index */
	GLSLBindingSymbol		*psActiveSpecials[GLES2_NUM_SPECIALS]; 

	/* texture samplers */
	GLES2TextureSampler		asTextureSamplers[GLES2_MAX_TEXTURE_UNITS]; 
	IMG_UINT32				ui32SamplersActive;

	
	/* Updated constant range since last validate, component based */
	GLES2UniformCopyRange    sUniformCopyRange;
	
	/* A pointer to some host memory holding all constants data (include uniform variables, 
	   constant variables and literal constants). It has the same size of psSharedState->sBindingSymbolList.pfConstantData 
	   and is initialized with it during the glLinkProgram.
	 */
	IMG_FLOAT				*pfConstantData;

	/* Mode to run the USE in */
	IMG_UINT32                      ui32USEMode;
	
	/*
	  Size of the secondary attributes loaded to the USE from the above PDS program 
	*/
	IMG_UINT32 ui32USESecAttribDataSizeInDwords; /* u32USEVertexShaderSADataSize / ui32PDSPixelStateStateSize */

 
	/*
	  Virtual address of constants
	*/
	IMG_DEV_VIRTADDR	uUSEConstsDataBaseAddress;		/* uUSEVertexShaderMemDataBaseAddr */
	IMG_UINT32			ui32USEConstsDataSizeinDWords;	/* ui32USEVertexShaderMemDataSize */

	/*
	  Scratch memory
	*/
	GLES2ShaderScratchMem *psScratchMem;
	
	/* 
		Indexable temps
	*/
	GLES2ShaderIndexableTempsMem *psIndexableTempsMem;
	
	/* Linked list of compiled use shaders for different back end linkage */
	struct GLES2USEShaderVariant_TAG *psVariant;
};


/* GLES2Program represents a GL program but it also has low-level information like the 'program shaders' */
struct GLES2ProgramRec
{
 	/* This struct must be the first variable */
	GLES2NamedItem  sNamedItem;

	/* Must be 2nd in structure to allow casting to Shader */
	IMG_UINT32 ui32Type;

	GLES2Shader *psVertexShader;
	GLES2Shader *psFragmentShader;

#if defined(GLES2_EXTENSION_GET_PROGRAM_BINARY)
	/* This flag to indicate if the program executable is loaded from binary or compiled and linked from shader source */
	IMG_BOOL bLoadFromBinary;
#endif

	IMG_BOOL bDeleting;
	IMG_BOOL bAttemptedLink;
	IMG_BOOL bSuccessfulLink;
	IMG_BOOL bSuccessfulValidate;

	/* An array of type char containing the info log, initially null. */
	IMG_CHAR *pszInfoLog;

	/*
	//// Uniforms 
	*/

	/* An integer holding the number of Total active uniforms (including HW specific constants). */
	IMG_UINT32 ui32NumActiveUniforms;

	/* An integer holding the length of the longest active uniform name, including a null termination. */
	IMG_UINT32 ui32LengthOfLongestUniformName;

	/* An array of active uniform symbol pointers */
	GLES2Uniform *psActiveUniforms;

	/* A list of active uniform pointers which can be exposed to application */
	IMG_UINT32 ui32NumActiveUserUniforms;
	
	GLES2Uniform **ppsActiveUserUniforms;

	/* A list of builtin uniforms, one builtin ID has one entry, even for struct type */
	GLES2BuiltInUniform *psBuiltInUniforms;
	IMG_UINT32 ui32NumBuiltInUniforms;

	/*
	//// Vertex attributes 
	*/

	/* A list of user defined linkage of attributes */
	GLSLAttribUserBinding	*psUserBinding, *psLastUserBinding;

	/* An integer holding the number of active attributes */
	IMG_UINT32 ui32NumActiveAttribs;

	/* An integer holding the length of the longest attribute name. */
	IMG_UINT32 ui32LengthOfLongestAttribName;
	
	/* An array of active uniform symbol pointers */
	GLES2Attribute *psActiveAttributes;

	/* Attrib compile mask */
	IMG_UINT32 ui32AttribCompileMask;

	/* Attribute input reg mapping */
	IMG_UINT32	aui32InputRegMappings[GLES2_MAX_VERTEX_ATTRIBS];

	/*
	//// Varying variable
	*/
	IMG_UINT32 ui32NumActiveVaryings;
	GLES2Varying *psActiveVaryings;

	IMG_UINT32 aui32IteratorPosition[NUM_TC_REGISTERS];
	IMG_UINT32 aui32FragVaryingPosition[NUM_TC_REGISTERS];

	GLES2ProgramShader sVertex;
	
	GLES2ProgramShader sFragment;


	IMG_UINT32 ui32OutputSelects;

	IMG_UINT32 ui32TexCoordSelects;

	IMG_UINT32 ui32TexCoordPrecision;
};


/*
 * Struct holding pointers to the functions offered by the GLSL compiler.
 */
typedef struct GLES2CompilerFuncTableTAG
{
	IMG_BOOL                     (IMG_CALLCONV *pfnInitCompiler)(GLSLInitCompilerContext *psInitCompilerContext);

	IMG_BOOL                     (IMG_CALLCONV *pfnShutDownCompiler)(GLSLInitCompilerContext *psInitCompilerContext);

	GLSLCompiledUniflexProgram * (IMG_CALLCONV *pfnCompileToUniflex)(GLSLCompileUniflexProgramContext *psCompileUniflexProgramContext);

	IMG_VOID                     (IMG_CALLCONV *pfnDisplayMetrics)(GLSLInitCompilerContext *psInitCompilerContext);

	IMG_VOID                     (IMG_CALLCONV *pfnFreeCompiledUniflexProgram)
	                                   (GLSLInitCompilerContext *psInitCompilerContext,
	                                    GLSLCompiledUniflexProgram *psGLSLCompiledUniflexProgram);
#if defined(GLES2_EXTENSION_GET_PROGRAM_BINARY)
	SGXBS_Error					 (IMG_CALLCONV *pfnCreateBinaryProgram)(const GLSLCompiledUniflexProgram* psVertex, const GLSLCompiledUniflexProgram* psFragment, const GLSLAttribUserBinding	*psUserBinding, 
													 IMG_UINT32 pu32BinarySizeInBytes, IMG_UINT32 * ui32Length, IMG_VOID* pvBinaryShader, IMG_BOOL bCreateBinary);
#endif
#if defined(EGL_EXTENSION_ANDROID_BLOB_CACHE)
	SGXBS_Error					 (IMG_CALLCONV *pfnCreateBinaryShader)(const GLSLCompiledUniflexProgram* psVertex, IMG_VOID*  (*pfnMalloc)(IMG_UINT32),
																		IMG_VOID   (*pfnFree)(IMG_VOID*), IMG_VOID** ppvBinaryShader, 
																		IMG_UINT32* pu32BinaryShaderLengthInBytes);
#endif

} GLES2CompilerFuncTable;


/************************************************************************/
/*								GLES2 program machine					*/
/************************************************************************/

typedef struct GLES2ProgramMachineRec
{
	IMG_VOID *pvUniPatchContext;

	GLES2Program *psCurrentProgram;
	
	GLES2USEShaderVariant *psCurrentFragmentVariant;
	GLES2USEShaderVariant *psCurrentVertexVariant;

	HashTable sPDSFragmentVariantHashTable;
#if defined(SUPPORT_NV12_FROM_2_HWADDRS)
	IMG_UINT32 aui32HashCompare[2 + (GLES2_MAX_TEXTURE_UNITS * (EURASIA_TAG_TEXTURE_STATE_SIZE+2))];
#else
	IMG_UINT32 aui32HashCompare[2 + (GLES2_MAX_TEXTURE_UNITS * (EURASIA_TAG_TEXTURE_STATE_SIZE+1))];
#endif

#if defined(SUPPORT_SOURCE_SHADER)
	/* Handle for the GLSL compiler (a dynamically loaded library).
	 * If the handle is non-null, the compiler was initialized successfully.
	 * The module is freed when an application calls glReleaseShaderCompiler,
	 * and then the handle is reset to null.
	 */
	IMG_HANDLE           hGLSLCompiler;

	/* Function pointers for the compiler API */
	GLES2CompilerFuncTable sGLSLFuncTable;
	GLSLInitCompilerContext sInitCompilerContext;
#endif

	PVRSRV_CLIENT_MEM_INFO	*psDummyFragUSECode;
	PVRSRV_CLIENT_MEM_INFO	*psDummyVertUSECode;

#if defined(FIX_HW_BRN_31988)
	PVRSRV_CLIENT_MEM_INFO	*psBRN31988FragUSECode;
#endif
} GLES2ProgramMachine;

IMG_VOID SetupProgramNameArray(GLES2NamesArray *psNamesArray);

GLES2Program *GetNamedProgram(GLES2Context *gc, GLuint program);
GLES2Shader *GetNamedShader(GLES2Context *gc, GLuint shader);

IMG_BOOL CreateProgramState(GLES2Context *gc);
IMG_VOID FreeProgramState(GLES2Context *gc);

IMG_VOID DestroyUSEShaderVariant(GLES2Context *gc, GLES2USEShaderVariant *psUSEVariant);
IMG_VOID DestroyUSEShaderVariantGhost(GLES2Context *gc, GLES2USEShaderVariantGhost *psUSEVariantGhost);
IMG_VOID DestroyVertexVariants(GLES2Context *gc, const IMG_VOID* pvAttachment, GLES2NamedItem *psNamedItem);
IMG_VOID DestroyHashedPDSVariant(GLES2Context *gc, IMG_UINT32 ui32Item);

IMG_BOOL InitializeGLSLCompiler(GLES2Context *gc);
IMG_VOID DestroyGLSLCompiler(GLES2Context *gc);

IMG_VOID USESecondaryUploadTaskAddRef(GLES2Context *gc, GLES2USESecondaryUploadTask *psUSESecondaryUploadTask);
IMG_VOID USESecondaryUploadTaskDelRef(GLES2Context *gc, GLES2USESecondaryUploadTask *psUSESecondaryUploadTask);

IMG_VOID ShaderScratchMemAddRef(GLES2Context *gc, GLES2ShaderScratchMem *psScratchMem);
IMG_VOID ShaderIndexableTempsMemAddRef(GLES2Context *gc, GLES2ShaderIndexableTempsMem *psIndexableTempsMem);

IMG_VOID ReclaimUSEShaderVariantMemKRM(IMG_VOID *pvContext, KRMResource *psResource);
IMG_VOID DestroyUSECodeVariantGhostKRM(IMG_VOID *pvContext, KRMResource *psResource);



#endif /* _SHADER_ */
