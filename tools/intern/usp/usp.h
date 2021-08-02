 /*****************************************************************************
 Name			: USP.H
 
 Title			: Uniflex Patcher API definitions
 
 C Author 		: Joe Molleson
 
 Created  		: 02/01/2002
 
 Copyright      : 2002-2006 by Imagination Technologies Limited. All rights reserved.
                : No part of this software, either material or conceptual 
                : may be copied or distributed, transmitted, transcribed,
                : stored in a retrieval system or translated into any 
                : human or computer language in any form by any means,
                : electronic, mechanical, manual or other-wise, or 
                : disclosed to third parties without the express written
                : permission of Imagination Technologies Limited, Unit 8, HomePark
                : Industrial Estate, King's Langley, Hertfordshire,
                : WD4 8LZ, U.K.

 Description 	: Interface for the Uniflex Shader Patcher (UniPatch)
   
 Program Type	: 32-bit DLL

 Version	 	: $Revision: 1.70 $

 Modifications	:

 $Log: usp.h $
*****************************************************************************/
#ifndef _USP_H_
#define _USP_H_
#include "usp_typedefs.h"
#include "sgxdefs.h"


/*
	Opaque data representing a precompiled shader
*/
#define USP_PC_SHADER_ID	(('U' << 24) | ('S' << 16) | ('P' << 8) | (' ' << 0))
#define USP_PC_SHADER_VER	((0x0000 << 16) | (0x0023 << 0))

typedef struct _USP_PC_SHADER_
{
	/* Special word identifying our data. Should contain USP_PC_SHADER_ID */
	IMG_UINT32	uId;

	/* Version of the data format */
	IMG_UINT32	uVersion;

	/* The size (in bytes) of the following data */
	IMG_UINT32	uSize;

	/* Internal data-blocks follow. */
}USP_PC_SHADER,   *PUSP_PC_SHADER; 

/*
	Possible output register types (for PVRUniPatchSetOutputLocation)
*/
typedef enum _USP_OUTPUT_REGTYPE_
{
	/* Leave the shader outputs unchanged */
	USP_OUTPUT_REGTYPE_DEFAULT	= 0,

	/* Remap shader output data to USE output registers */
	USP_OUTPUT_REGTYPE_OUTPUT	= 1,

	/* Remap shader output data to USE temp registers */
	USP_OUTPUT_REGTYPE_TEMP		= 2,

	/* Remap shader output data to USE primary-attribute registers */
	USP_OUTPUT_REGTYPE_PA		= 3
}USP_OUTPUT_REGTYPE, *PUSP_OUTPUT_REGTYPE;

/*
	Maximum number of textures supported by the USP.
*/
#define USP_MAX_SAMPLER_IDX			(16)

/*
	Texture format description, for PVRUniPatchSetTextureFormat (psFormat.eTexFmt)
*/
typedef enum _USP_TEXTURE_FORMAT_
{
	USP_TEXTURE_FORMAT_A4R4G4B4         = 0,
	USP_TEXTURE_FORMAT_A1R5G5B5         = 1,
	USP_TEXTURE_FORMAT_R5G6B5           = 2,
	USP_TEXTURE_FORMAT_B8G8R8A8         = 3,
	USP_TEXTURE_FORMAT_R8G8B8A8         = 4,
	USP_TEXTURE_FORMAT_PVRT2BPP_RGB     = 5,
	USP_TEXTURE_FORMAT_PVRT2BPP_RGBA    = 6,
	USP_TEXTURE_FORMAT_PVRT4BPP_RGB     = 7,
	USP_TEXTURE_FORMAT_PVRT4BPP_RGBA    = 8,
	USP_TEXTURE_FORMAT_PVRTII2BPP_RGB   = 9,
	USP_TEXTURE_FORMAT_PVRTII2BPP_RGBA  = 10,
	USP_TEXTURE_FORMAT_PVRTII4BPP_RGB   = 11,
	USP_TEXTURE_FORMAT_PVRTII4BPP_RGBA  = 12,
	USP_TEXTURE_FORMAT_PVRTIII          = 13,
	USP_TEXTURE_FORMAT_YUYV             = 14,
	USP_TEXTURE_FORMAT_UYVY             = 15,
	USP_TEXTURE_FORMAT_C0_YUV420_2P_UV	= 16,
	USP_TEXTURE_FORMAT_I8				= 17,
	USP_TEXTURE_FORMAT_A8				= 18,
	USP_TEXTURE_FORMAT_L8				= 19,
	USP_TEXTURE_FORMAT_L8A8			    = 20,
	USP_TEXTURE_FORMAT_RGBA_F16			= 21,
	USP_TEXTURE_FORMAT_RGB_F16			= 22,
	USP_TEXTURE_FORMAT_IF16				= 23,
	USP_TEXTURE_FORMAT_AF16			    = 24,
	USP_TEXTURE_FORMAT_LF16			    = 25,
	USP_TEXTURE_FORMAT_LF16AF16         = 26,
	USP_TEXTURE_FORMAT_RGBA_F32			= 27,
	USP_TEXTURE_FORMAT_RGB_F32			= 28,
	USP_TEXTURE_FORMAT_IF32				= 29,
	USP_TEXTURE_FORMAT_AF32             = 30,
	USP_TEXTURE_FORMAT_LF32             = 31,
	USP_TEXTURE_FORMAT_LF32AF32         = 32,
	USP_TEXTURE_FORMAT_DF32			    = 33,
	USP_TEXTURE_FORMAT_DU16			    = 34,
	USP_TEXTURE_FORMAT_IU16             = 35,
	USP_TEXTURE_FORMAT_YVYU             = 36,
	USP_TEXTURE_FORMAT_VYUY             = 37,
	USP_TEXTURE_FORMAT_C0_YUV420_3P_UV  = 38
	/* Unsigned Integer */
	,USP_TEXTURE_FORMAT_RGBA_U8 		= 39
	,USP_TEXTURE_FORMAT_RGBA_U16		= 40
	,USP_TEXTURE_FORMAT_RGBA_U32		= 41
	/* Signed Integer */
	,USP_TEXTURE_FORMAT_RGBA_S8 		= 42
	,USP_TEXTURE_FORMAT_RGBA_S16		= 43
	,USP_TEXTURE_FORMAT_RGBA_S32		= 44
	/* Unsigned Normalized */
	,USP_TEXTURE_FORMAT_RGBA_UNORM8		= 45
	,USP_TEXTURE_FORMAT_RGBA_UNORM16	= 46
}USP_TEXTURE_FORMAT, *PUSP_TEXTURE_FORMAT;

#define USP_TEXFORM_MAX_PLANE_COUNT	(4)

typedef enum _USP_CHAN_SWIZZLE_
{
	USP_CHAN_SWIZZLE_CHAN_0	= 0,
	USP_CHAN_SWIZZLE_CHAN_1	= 1,
	USP_CHAN_SWIZZLE_CHAN_2	= 2,
	USP_CHAN_SWIZZLE_CHAN_3	= 3,
	USP_CHAN_SWIZZLE_ONE	= 4,
	USP_CHAN_SWIZZLE_ZERO	= 5
}USP_CHAN_SWIZZLE, *PUSP_CHAN_SWIZZLE;

#define USP_TEXFORM_CHAN_COUNT		(4)

typedef struct _USP_TEX_FORMAT_
{
	/* The texture format */
	USP_TEXTURE_FORMAT  eTexFmt;

	/* The expected channel arrangement */
	USP_CHAN_SWIZZLE	aeChanSwizzle[USP_TEXFORM_CHAN_COUNT];
}USP_TEX_FORMAT, *PUSP_TEX_FORMAT;

/*
	Structure describing an interated or pre-sampled value expected as input
	for a pixel-shader
*/
typedef enum _USP_HW_PSINPUT_TYPE_
{
	USP_HW_PSINPUT_TYPE_ITERATION	= 0,
	USP_HW_PSINPUT_TYPE_SAMPLE		= 1
}USP_HW_PSINPUT_TYPE, *PUSP_HW_PSINPUT_TYPE;

typedef enum _USP_HW_PSINPUT_COORD_
{
	USP_HW_PSINPUT_COORD_TC0	= 0,
	USP_HW_PSINPUT_COORD_TC1	= 1,
	USP_HW_PSINPUT_COORD_TC2	= 2,
	USP_HW_PSINPUT_COORD_TC3	= 3,
	USP_HW_PSINPUT_COORD_TC4	= 4,
	USP_HW_PSINPUT_COORD_TC5	= 5,
	USP_HW_PSINPUT_COORD_TC6	= 6,
	USP_HW_PSINPUT_COORD_TC7	= 7,
	USP_HW_PSINPUT_COORD_TC8	= 8,
	USP_HW_PSINPUT_COORD_TC9	= 9,
	USP_HW_PSINPUT_COORD_V0		= 10,
	USP_HW_PSINPUT_COORD_V1		= 11,
	USP_HW_PSINPUT_COORD_FOG	= 12,
	USP_HW_PSINPUT_COORD_POS	= 13,
	USP_HW_PSINPUT_COORD_VPRIM	= 14
}USP_HW_PSINPUT_COORD, *PUSP_HW_PSINPUT_COORD;

typedef enum _USP_HW_PSINPUT_FMT_
{
	USP_HW_PSINPUT_FMT_F32	= 0,
	USP_HW_PSINPUT_FMT_F16	= 1,
	USP_HW_PSINPUT_FMT_C10	= 2,
	USP_HW_PSINPUT_FMT_U8	= 3,
	USP_HW_PSINPUT_FMT_NONE = 4
}USP_HW_PSINPUT_FMT, *PUSP_HW_PSINPUT_FMT;

#define USP_HW_PSINPUT_FLAG_PROJECTED	0x00000001
#define USP_HW_PSINPUT_FLAG_CENTROID	0x00000002

typedef struct _USP_HW_PSINPUT_LOAD_
{
	/* The type of pixel-shader input data */
	USP_HW_PSINPUT_TYPE		eType;

	/* Flags indicating extra options for iterated/pre-sampled data */
	IMG_UINT32				uFlags;

	/* The coordinate to iterate, or use to pre-sample a texture */
	USP_HW_PSINPUT_COORD	eCoord;

	/* The number of components required (for iterated data only)*/
	IMG_UINT16				uCoordDim;

	/* Index of the texture control words entry for this input data */
	IMG_UINT16				uTexCtrWrdIdx;

	/* The expected data-format for iterated data or unpacking format  for pre-sampled data */
	USP_HW_PSINPUT_FMT		eFormat;

	/* Indicates to pack sample results */
	IMG_BOOL				bPackSampleResults;

	/* The data size for this input load */
	IMG_UINT16				uDataSize;
}USP_HW_PSINPUT_LOAD, *PUSP_HW_PSINPUT_LOAD;

/*
	Structure describing where texture-state words should be loaded
*/
typedef struct _USP_HW_TEXSTATE_LOAD_
{
	/* Index of the texture control words entry for this texture state */
	IMG_UINT16		uTexCtrWrdIdx;

	/*
		index of the register/memory location where the first state word for
		this chunk should be loaded
	*/
	IMG_UINT32		uDestIdx;

	/* Size of data returned by sample in DWORDS */
	IMG_UINT16		uTagSize;
}USP_HW_TEXSTATE_LOAD, *PUSP_HW_TEXSTATE_LOAD;

/*
	Structure describing where constant-data should be loaded
*/
typedef enum _USP_HW_CONST_FMT_
{
	USP_HW_CONST_FMT_F32	= 0,
	USP_HW_CONST_FMT_F16	= 1,
	USP_HW_CONST_FMT_C10	= 2,
	USP_HW_CONST_FMT_U8     = 3,
	USP_HW_CONST_FMT_STATIC	= 4
}USP_HW_CONST_FMT, *PUSP_HW_CONST_FMT;

typedef struct _USP_HW_CONST_LOAD_
{
	/* index of the original (32-bit sized) constant, or static value to be loaded (for FMT_STATIC only) */
	IMG_UINT32			uSrcIdx;

	/* shift within the source location for formats <32-bits */
	IMG_UINT32			uSrcShift;

	/* index of the (32-bit size) location where the constant should be loaded */
	IMG_UINT16			uDestIdx;

	/* shift within the destination location for formats <32-bits */
	IMG_UINT16			uDestShift;

	/* format in which the constant should be loaded */
	USP_HW_CONST_FMT	eFormat;
}USP_HW_CONST_LOAD, *PUSP_HW_CONST_LOAD;

/*
	Structure containing the expected locations for static/constant
	data (stored in SA registers) used by a shader
*/
#define USP_HW_SAREG_USE_FLAG_MEMCONSTS_USED	0x00000001
#define USP_HW_SAREG_USE_FLAG_MEMCONSTS2_USED	0x00000002
#define USP_HW_SAREG_USE_FLAG_SCRATCH_USED		0x00000004
#define USP_HW_SAREG_USE_FLAG_TEXSTATE_USED		0x00000008
#define USP_HW_SAREG_USE_FLAG_REGCONSTS_USED	0x00000010
#define USP_HW_SAREG_USE_FLAG_INDEXEDTEMPS_USED	0x00000020

typedef struct _USP_HW_SAREG_USE_
{
	IMG_UINT32	uFlags;
	IMG_UINT16	uMemConstBaseAddrReg;
	IMG_UINT16	uMemConst2BaseAddrReg;
	IMG_UINT16	uScratchMemBaseAddrReg;
	IMG_UINT16	uTexStateFirstReg;
	IMG_UINT16	uRegConstsFirstReg;
	IMG_UINT16	uRegConstsRegCount;
	IMG_UINT16	uIndexedTempBaseAddrReg;
}USP_HW_SAREG_USE, *PUSP_HW_SAREG_USE;

/*
	Flags for USP_HWSHADER::uFlags
*/
#define USP_HWSHADER_FLAGS_TEXKILL_USED				0x00000001
#define USP_HWSHADER_FLAGS_LABEL_AT_END				0x00000002
#define USP_HWSHADER_FLAGS_PER_INSTANCE_MODE		0x00000004
#define USP_HWSHADER_FLAGS_DEPTHFEEDBACK			0x00000008
#define USP_HWSHADER_FLAGS_OMASKFEEDBACK			0x00000010
#define USP_HWSHADER_FLAGS_TEXTUREREADS				0x00000020
#define USP_HWSHADER_FLAGS_SPLITALPHACALC			0x00000040
#define USP_HWSHADER_FLAGS_SPLITCALC				0x00000080
#define USP_HWSHADER_FLAGS_SAPROG_LABEL_AT_END		0x00000100
#define USP_HWSHADER_FLAGS_SIMPLE_PS				0x00000200
#define USP_HWSHADER_FLAGS_CFASEL_ENABLED_AT_END	0x00000400
#define USP_HWSHADER_FLAGS_ABS_BRANCHES_USED		0x00000800

/*
	The number of registers required for the texture state for a single
	chunk of a texture
*/
#define USP_TEXSTATE_SIZE		(EURASIA_TAG_TEXTURE_STATE_SIZE)

/*
	Set of texture control words
*/
typedef struct _USP_TEX_CTR_WORDS_
{
	/* Index of the texture associated with this set of state words */
	IMG_UINT16		uTextureIdx;

	/* Index of the chunk of texture-data to be loaded using these state words */
	IMG_UINT16		uChunkIdx;

	/* Pointer to texture control words */
	IMG_UINT32  auWord[USP_TEXSTATE_SIZE];

	/* Pointer to texture control word masks */
	IMG_UINT32  auMask[USP_TEXSTATE_SIZE];
}USP_TEX_CTR_WORDS, *PUSP_TEX_CTR_WORDS;

/*
	Finalised shader data
*/
typedef struct _USP_HWSHADER_
{
	/* Contains flags with information about the program */
	IMG_UINT32				uFlags;

	/* Target device ID */
//	SGX_CORE_ID_TYPE		eTargetDev;
	SGX_CORE_INFO			sTargetDev;

	/* Count of instructions in the main program */
	IMG_UINT16				uInstCount;

	/* Array of the instructions generated for the main program */
	IMG_PUINT32				puInsts;

	/* Instruction index of the start of the main program */
	IMG_UINT16				uProgStartInstIdx;

	/*	
		Instruction index of the end of the alpha-calc/test phase of the main
		program
	*/
	IMG_UINT16				uPTPhase0EndInstIdx;

	/*
		Instruction index of the start of the phase following alpha-calc/test
		in the main program
	*/
	IMG_UINT16				uPTPhase1StartInstIdx;

	/*
		Instruction index of the start of the phase following split
		in the main program
	*/
	IMG_UINT16				uPTSplitPhase1StartInstIdx;

	/* Instruction count for the SA update program */
	IMG_UINT16				uSAUpdateInstCount;

	/* Array of the instructions generated for the SA update program */
	IMG_PUINT32				puSAUpdateInsts;

	/* The number of primary attribute registers required */
	IMG_UINT16				uPARegCount;

	/* The number of primary attribute registers reserved for driver use */
	IMG_UINT16				uReservedPARegCount;

	/* The number of temporary registers required */
	IMG_UINT16				uTempRegCount;
	
	/* The number of temporary registers required in Secondary Update program */
	IMG_UINT16				uSecTempRegCount;

	/* The total number of secondary attribute registers required */
	IMG_UINT16				uSARegCount;

	/* Data describing the data expected in SA registers */	
	USP_HW_SAREG_USE		sSARegUse;

	/* Number of iterated or pre-sampled pixel-shader input values */
	IMG_UINT16				uPSInputCount;

	/* The list of expected iterated or pre-sampled pixel-shader input values */
	PUSP_HW_PSINPUT_LOAD	psPSInputLoads;

	/* The number of constants expected to be loaded into memory */
	IMG_UINT32				uMemConstCount;

	/* The list of constants expected to be loaded into memory */
	PUSP_HW_CONST_LOAD		psMemConstLoads;

	/* The number of constants expected in registers */
	IMG_UINT16				uRegConstCount;

	/* The list of constants expected to be loadaed into registers */
	PUSP_HW_CONST_LOAD		psRegConstLoads;

	/* Number of sets of texture-state words to be loaded into registers */
	IMG_UINT16				uRegTexStateCount;

	/* The list of sets of texture-state words to load into registers */
	PUSP_HW_TEXSTATE_LOAD	psRegTexStateLoads;

	/* Number of sets of texture-state words to be loaded into memory */
	IMG_UINT32				uMemTexStateCount;

	/* The list of sets of texture-state words to load into memory */
	PUSP_HW_TEXSTATE_LOAD	psMemTexStateLoads;

	/* The total size of the indexable temporary storage required (in bytes) */
	IMG_UINT32				uIndexedTempTotalSize;

	/* Size in dwords of the scratch area required per-instance */
	IMG_UINT32				uScratchAreaSize;

	/*
		The number of bytes difference between the address of a memory buffer
		used by the program and the address stored in the secondary attribute.
		Always set to -4 at the moment.
	*/
	IMG_INT32				iSAAddressAdjust;

	/* Index of register used for pixel-shader result */
	IMG_UINT32				uPSResultRegNum;

	/* Bitmask of which texture stages that should have anisotropic filtering disabled */
	IMG_UINT32				auNonAnisoTexStages[(USP_MAX_SAMPLER_IDX + 31) / 32];

	/* Number of PA registers used for VS input-data (excluding pre-sampled textures) */
	IMG_UINT32				uVSInputPARegCount;

	/* Bitmask describing input usage for vertex-shaders */
	IMG_UINT32				auVSInputsUsed[EURASIA_USE_PRIMATTR_BANK_SIZE/32];

	/* Total number of entries of texture control word sets */
	IMG_UINT32				uTexCtrWrdLstCount;

	/* The sets of texture control words */
	PUSP_TEX_CTR_WORDS		psTextCtrWords;
		
	/* Shader Outputs Count after Patching */
	IMG_UINT32				uShaderOutputsCount;

	/* Shader Outputs Valid after Patching */
	IMG_UINT32*				puValidShaderOutputs;

}USP_HW_SHADER, *PUSP_HW_SHADER;

/*
	Create a USP execution-context (required for all other APIs)
*/
IMG_PVOID IMG_CALLCONV PVRUniPatchCreateContext(USP_MEMALLOCFN	pfnAlloc,
												USP_MEMFREEFN	pfnFree,
												USP_DBGPRINTFN	pfnPrint);

/*
	Create a patchable shader from a pre-compiled shader binary
*/
IMG_PVOID IMG_CALLCONV PVRUniPatchCreateShader(IMG_PVOID		pvContext,
											   PUSP_PC_SHADER	psPCShader);

/*
	Create a pre-compiled shader binary from a patchable shader
*/
IMG_PVOID IMG_CALLCONV PVRUniPatchCreatePCShader(IMG_PVOID	pvContext,
												 IMG_PVOID	pvShader);

/*
	Specify texture-details for a specific sampler
*/
IMG_VOID IMG_CALLCONV PVRUniPatchSetTextureFormat(IMG_PVOID			pvContext,
												  IMG_UINT16		uSamplerIdx,
												  PUSP_TEX_FORMAT	psFormat,
												  IMG_BOOL			bAnisoEnabled,
												  IMG_BOOL			bGammaEnabled);

/*
  Specify for a given sampler whether the input co-ordinates will be
  normalized.
 */
IMG_VOID IMG_CALLCONV PVRUniPatchSetNormalizedCoords(IMG_PVOID  pvContext,
                                                     IMG_UINT16 uSampleIdx,
                                                     IMG_BOOL   bNormCoords,
                                                     IMG_BOOL   bLinearFilter,
                                                     IMG_UINT32 uTextureWidth,
                                                     IMG_UINT32 uTextureHeight);


/*
	Specify the register-bank where the final results of a shader should be relocated
	by the USP (Default is USP_OUTPUT_REGTYPE_DEFAULT)

	NB: Currently, only pixel-shaders can have their results remapped
*/
IMG_VOID IMG_CALLCONV PVRUniPatchSetOutputLocation(IMG_PVOID			pvContext,
												   USP_OUTPUT_REGTYPE	eRegType);

/*
	Specify the number of instructions to be appended at the beginning of the 
	shader entry-point (Default is zero, must be less than USP_MAX_PREAMBLE_INST_COUNT).
*/
#define USP_MAX_PREAMBLE_INST_COUNT	(16)

IMG_BOOL IMG_CALLCONV PVRUniPatchSetPreambleInstCount(IMG_PVOID		pvContext,
													  IMG_UINT32	uInstCount);

/*
	Generate a HW-executeable version of the shader
*/
PUSP_HW_SHADER IMG_CALLCONV PVRUniPatchFinaliseShader(IMG_PVOID pvContext,
													  IMG_PVOID pvShader);

/*
	Free a HW-executable shader created by PVRUniPatchFinaliseShader
*/
IMG_VOID IMG_CALLCONV PVRUniPatchDestroyHWShader(IMG_PVOID		pvContext,
												 PUSP_HW_SHADER	psHWShader);

/*
	Destroy a patcheable shader created by PVRUniPatchCreateShader
*/
IMG_VOID IMG_CALLCONV PVRUniPatchDestroyShader(IMG_PVOID pvContext,
											   IMG_PVOID pvShader);

/*
	Destroy a precompiled shader created by PVRUniPatchCreatePCShader
*/
IMG_VOID IMG_CALLCONV PVRUniPatchDestroyPCShader(IMG_PVOID		pvContext, 
												 PUSP_PC_SHADER	psPCShader);

/*
	Destroy a USP execution context created by PVRUniPatchCreateContext
*/
IMG_VOID IMG_CALLCONV PVRUniPatchDestroyContext(IMG_PVOID pvContext);


#endif	/* #ifndef _USP_H_ */
/*****************************************************************************
 End of file (USP.H)
*****************************************************************************/
