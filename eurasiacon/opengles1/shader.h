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

#define GLES1_SHADERTYPE_PROGRAM	0
#define GLES1_SHADERTYPE_VERTEX		1
#define GLES1_SHADERTYPE_FRAGMENT	2


/* The maximum number of DOUTIs that can be issued by the PDS to iterate data.
   Enough for 16 texture samples, 10 texcoords, base colour, offset colour,
   fog, vertex position, plane coefficients. */
#define GLES1_MAX_DOUTI	(19 + 12)


typedef struct GLES1PDSInfo_TAG
{
	/* 
	   PDS_PIXEL_SIZE - Number of iterated components, in dwords, range 0 to 64 (16 layers 
	   4 components per layer) (Primary attribute instance size)
	*/
	IMG_UINT32              ui32DMSInfoIterationSize;

	/* PDS_TEXTURE_SIZE - Number of components of textured data in dwords, range 0 to 64 */ 
	IMG_UINT32              ui32DMSInfoTextureSize;

	/*
		Limits on the PDS pixel shader program task size.
	*/
	IMG_UINT32				ui32DMSInfo;
	

	/* Used to setup the DOUTI commands (goes into adwFPUIterators) */
#if defined(SGX545)
	IMG_UINT32              aui32TSPParamFetchInterface0[GLES1_MAX_DOUTI];		/* pdwTSPCoordCtl */
	IMG_UINT32              aui32TSPParamFetchInterface1[GLES1_MAX_DOUTI];		/* pdwTSPCoordCtl */
#else /* defined(SGX545) */
	IMG_UINT32              aui32TSPParamFetchInterface[GLES1_MAX_DOUTI];		/* pdwTSPCoordCtl */
#endif /* defined(SGX545) */  

	IMG_UINT32              aui32LayerControl[GLES1_MAX_DOUTI];

	IMG_UINT32              ui32NonDependentTextureCount;
	IMG_UINT32              ui32IterationCount;
	IMG_UINT32				ui32DependencyControl;

	IMG_UINT32				ui32NonDependentImageUnits; /* Bit field indicating which image units are enabled */

} GLES1PDSInfo;


struct GLES1USEShaderVariant_TAG
{
	/* USSE code variants are frame resources */
	KRMResource						sResource;
	
	/* Pointer back to the shader whose variant this is */
	struct GLES1ShaderRec			*psShader;

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
			/*
			   Enables that were used for this variant
			   only pay attention to alpha test, alpha blend, fog and logical ops
			*/
			IMG_UINT32 ui32RasterEnables;

			/* Colormask that was used for this variant */
			IMG_UINT32 ui32ColorMask;
	
			/* Blend state that was used for this variant */
			IMG_UINT32 ui32BlendFunction;
			IMG_UINT32 ui32BlendEquation;

			/* Logical op that was used for this variant */
			IMG_UINT32 ui32LogicOp;
			
			/* Shade Model that was used for this variant */
			IMG_UINT32 ui32ShadeModel;

			/* Number of enabled texture units */
			IMG_UINT32 ui32NumImageUnitsActive;

			/* Alpha test that was used for this variant */
			IMG_UINT32 ui32AlphaTestFlags;

			/* Point sprite coord replace mode */
			IMG_BOOL abPointSpriteReplace[GLES1_MAX_TEXTURE_UNITS];
			IMG_BOOL bPointSprite;

			IMG_BOOL bMSAATrans;


			GLES1PDSInfo sPDSInfo;

		} sFragment;

		struct
		{
			IMG_UINT32 aui32StreamTypeSize[GLES1_MAX_ATTRIBS_ARRAY];
			IMG_UINT32 ui32NumItemsPerVertex;			

		} sVertex;

	}u;

	GLES1PDSCodeVariant		  *psPDSVariant;
	GLES1PDSVertexCodeVariant *psPDSVertexCodeVariant;

	struct GLES1USEShaderVariant_TAG	*psNext;
};


typedef struct VSInputReg_TAG
{
	IMG_UINT32 ui32PrimaryAttribute;
	IMG_UINT32 ui32Size;

} VSInputReg;


typedef struct GLES1ShaderRec
{
	FFGEN_PROGRAM_DETAILS *psFFGENProgramDetails;

	/* Attribute input reg mapping and sizes*/
	VSInputReg	asVSInputRegisters[GLES1_MAX_ATTRIBS_ARRAY];
	
	/* Constant data */
	IMG_FLOAT  *pfConstantData;
	IMG_UINT32 ui32SizeOfConstants;
	IMG_UINT32 ui32ConstantDirtyMask;

	union
	{
		struct
		{
			struct FFTBProgramDesc_TAG *psFFTBProgramDesc;

		} sFFTB;

		struct
		{
			FFGenProgram *psFFTNLProgram;


		} sFFTNL;


	} u;

#if defined(SGX545)	
	IMG_UINT32 ui32NumTexWords;  
#endif /* defined(SGX545) */

	IMG_UINT32 ui32OutputSelects;
	IMG_UINT32 ui32TexCoordSelects;
	IMG_UINT32 ui32TexCoordPrecision;

	/* Mode to run the USE in */
	IMG_UINT32 ui32USEMode;
	
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
		Indexable temps
	*/
	IMG_UINT32						ui32NumIndexableTempArrays;
	IMG_DEV_VIRTADDR				uIndexableTempBaseAddress;
	IMG_UINT32						ui32IndexableTempSizeInDWords;

	/* Linked list of use shaders variants */
	struct GLES1USEShaderVariant_TAG *psShaderVariant;

	struct GLES1ShaderRec *psPrevious;
	struct GLES1ShaderRec *psNext;

} GLES1Shader;


/************************************************************************/
/*		GLES1 program machine					*/
/************************************************************************/
typedef struct GLES1ProgramMachineRec
{
	/* Current shaders */
	GLES1Shader *psCurrentVertexShader;
	GLES1Shader *psCurrentFragmentShader;

	/* Current shader variants */
	GLES1ShaderVariant *psCurrentVertexVariant;
	GLES1ShaderVariant *psCurrentFragmentVariant;

	/* List of all shaders */
	GLES1Shader *psVertex;
	GLES1Shader *psFragment;	

	IMG_HANDLE hFFTNLGenContext;

#if defined(FFGEN_UNIFLEX)
	IMG_HANDLE hUniFlexContext;
#endif /* defined(FFGEN_UNIFLEX) */


	union
	{
		IMG_UINT32 aui32HashState[2 + (GLES1_MAX_TEXTURE_UNITS * sizeof(FFTBBlendLayerDesc)) / sizeof(IMG_UINT32)];

		FFTNLGenDesc sTNLDescription;

		IMG_UINT32 aui32HashCompare[2 + (EURASIA_TAG_TEXTURE_STATE_SIZE * GLES1_MAX_TEXTURE_IMAGE_UNITS)];

		IMG_UINT32 aui32StaticPixelShaderSAProg[70];

	} uTempBuffer;

	HashTable sFFTNLHashTable;
	HashTable sFFTextureBlendHashTable;
	HashTable sPDSFragmentVariantHashTable;
	HashTable sPDSFragmentSAHashTable;

#if defined(PDUMP)
	GLES1PDSCodeVariant *psPDSCodeVariantList;
#endif

	PVRSRV_CLIENT_MEM_INFO	*psDummyFragUSECode;
	PVRSRV_CLIENT_MEM_INFO	*psDummyVertUSECode;

#if defined(FIX_HW_BRN_31988)
	PVRSRV_CLIENT_MEM_INFO	*psBRN31988FragUSECode;
#endif

} GLES1ProgramMachine;

IMG_VOID DestroyHashedPDSVariant(GLES1Context *gc, IMG_UINT32 ui32Item);
IMG_VOID FreeProgramState(GLES1Context *gc);
IMG_VOID DestroyVertexVariants(GLES1Context *gc);
IMG_VOID FreeShader(GLES1Context *gc, GLES1Shader *psShader);
IMG_VOID ReclaimUSEShaderVariantMemKRM(IMG_VOID *pvContext, KRMResource *psResource);
IMG_VOID DestroyUSECodeVariantGhostKRM(IMG_VOID *pvContext, KRMResource *psResource);

#endif /* _SHADER_ */
