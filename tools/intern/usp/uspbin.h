 /*****************************************************************************
 Name			: USPBIN.H
 
 Title			: Uniflex Patcher pre-compiled shader binary format
 
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

 Description 	: Uniflex Patcher pre-compiled shader binary format
   
 Program Type	: 32-bit DLL

 Version	 	: $Revision: 1.51 $

 Modifications	:

 $Log: uspbin.h $
*****************************************************************************/
#ifndef _USPBIN_H_
#define _USPBIN_H_
#include "usp_typedefs.h"
#include "usp.h"

/*
	Precompiled data-block types
*/
typedef enum _USP_PC_BLOCK_TYPE_
{
	USP_PC_BLOCK_TYPE_PROGDESC		= 1,
	USP_PC_BLOCK_TYPE_HWCODE		= 2,
	USP_PC_BLOCK_TYPE_BRANCH		= 3,
	USP_PC_BLOCK_TYPE_LABEL			= 4,
	USP_PC_BLOCK_TYPE_SAMPLE		= 5,
	USP_PC_BLOCK_TYPE_SAMPLEUNPACK	= 6,
	USP_PC_BLOCK_TYPE_TEXTURE_WRITE  = 7,
	USP_PC_BLOCK_TYPE_END			= 8

}USP_PC_BLOCK_TYPE, *PUSP_PC_BLOCK_TYPE;

/*
	Data-block header (this must preceed every block)
*/
struct _USP_PC_BLOCK_HDR_
{
	/* What the following block-data represents (see USP_PC_BLOCK_TYPE) */
	IMG_UINT32	uType;

	/* Data for the block (if any) follows... */
};

/*
	General flags describing the pre-compiled program
*/

/* Relative addressing is used to access the iterated input data */
#define USP_PC_PROGDESC_FLAGS_ITERATED_DATA_RELADDR					0x00000001
/* Shader result references in instructions are not patchable */
#define USP_PC_PROGDESC_FLAGS_RESULT_REFS_NOT_PATCHABLE				0x00000002
/* Shader result references in instructions are not patchable with PA registers */
#define USP_PC_PROGDESC_FLAGS_RESULT_REFS_NOT_PATCHABLE_WITH_PAS	0x00000004
/* The shader has been split into pre- and post-feedback phases. */
#define USP_PC_PROGDESC_FLAGS_SPLITFEEDBACKCALC						0x00000008
/* The shader has been split into phases executing at pixel rate and sample rate. */
#define USP_PC_PROGDESC_FLAGS_SPLITCALC								0x00000010

/*
	Data defining a group of indexable constants
*/
struct _USP_PC_CONST_RANGE_
{
	/* First constant in group */
	IMG_UINT16	uStart;

	/* last constant in group */
	IMG_UINT16	uEnd;
};

/*
	Data defining a group of indexable temporaries
*/
struct _USP_PC_INDEXABLE_TEMP_SIZE_
{
	/* unique value identifying this group of temporaries */
	IMG_UINT16	uTag;

	/* The number of temporaries in the group */
	IMG_UINT16	uSize;
};

/*
	Data describing a non-dependent texture-sample or value-iteration
*/
#define USP_PC_PSINPUT_FLAG_PROJECTED		0x0001
#define USP_PC_PSINPUT_FLAG_CENTROID		0x0002
#define USP_PC_PSINPUT_FLAG_ITERATION		0x0004

/* Indicates this data was added to avoid the USP overwriting outputs in PAs */
#define USP_PC_PSINPUT_FLAG_PADDING			0x0008

typedef enum _USP_PC_PSINPUT_COORD_
{
	USP_PC_PSINPUT_COORD_TC0	= 1,
	USP_PC_PSINPUT_COORD_TC1	= 2,
	USP_PC_PSINPUT_COORD_TC2	= 3,
	USP_PC_PSINPUT_COORD_TC3	= 4,
	USP_PC_PSINPUT_COORD_TC4	= 5,
	USP_PC_PSINPUT_COORD_TC5	= 6,
	USP_PC_PSINPUT_COORD_TC6	= 7,
	USP_PC_PSINPUT_COORD_TC7	= 8,
	USP_PC_PSINPUT_COORD_TC8	= 9,
	USP_PC_PSINPUT_COORD_TC9	= 10,
	USP_PC_PSINPUT_COORD_V0		= 11,
	USP_PC_PSINPUT_COORD_V1		= 12,
	USP_PC_PSINPUT_COORD_FOG	= 13,
	USP_PC_PSINPUT_COORD_POS	= 14,
	USP_PC_PSINPUT_COORD_VPRIM	= 15
}USP_PC_PSINPUT_COORD, *PUSP_PC_PSINPUT_COORD;

typedef enum _USP_PC_PSINPUT_FMT_
{
	USP_PC_PSINPUT_FMT_F32	= 1,
	USP_PC_PSINPUT_FMT_F16	= 2,
	USP_PC_PSINPUT_FMT_C10	= 3,
	USP_PC_PSINPUT_FMT_U8	= 4
}USP_PC_PSINPUT_FMT, *PUSP_PC_PSINPUT_FMT;

struct _USP_PC_PSINPUT_LOAD_
{
	/* see USP_PC_PSINPUT_FLAG_xxx */
	IMG_UINT16	uFlags;

	/* Index of the texture/sampler used (for pre-sampled textures) */
	IMG_UINT16	uTexture;

	/* The iterated coordinate/input-value (see USP_PC_PSINPUT_COORD) */
	IMG_UINT16	uCoord;

	/* dimensions for iterated coordinates */
	IMG_UINT16	uCoordDim;

	/* Data-format for iterated data (see USP_PC_PSINPUT_FMT) */
	IMG_UINT16	uFormat;

	/* Data size of this input load */
	IMG_UINT16 uDataSize;
};

/*
	Data describing a constant used by the program (either referenced from
	memory or a register)
*/
typedef enum _USP_PC_CONST_FMT_
{
	USP_PC_CONST_FMT_F32	= 1,
	USP_PC_CONST_FMT_F16	= 2,
	USP_PC_CONST_FMT_C10	= 3,
	USP_PC_CONST_FMT_U8     = 4,
	USP_PC_CONST_FMT_STATIC	= 5,
}USP_PC_CONST_FMT, *PUSP_PC_CONST_FMT;

struct _USP_PC_CONST_LOAD_
{
	/* format in which the constant should be loaded (see USP_PC_CONST_FMT) */
	IMG_UINT16	uFormat;

	union _USP_PC_CONST_LOAD_SRC_
	{
		struct
		{
			/* index of the original (32-bit sized) constant (for all except USP_PC_CONST_FMT_STATIC) */
			IMG_UINT16	uSrcIdx;

			/* shift within the source location for formats <32-bits */
			IMG_UINT16	uSrcShift;
		} sConst;

		/* Static value to loaded for this constant (for USP_PC_CONST_FMT_STATIC only) */
		IMG_UINT32	uStaticVal;
	} sSrc;

	/* index of the (32-bit size) location where the constant should be loaded */
	IMG_UINT16	uDestIdx;

	/* shift within the destination location for formats < 32-bits */
	IMG_UINT16	uDestShift;
};

/*
	Data describing texture-state used by the program
*/
struct _USP_PC_TEXSTATE_LOAD_
{
	/* Index of the texture associated with this set of state words */
	IMG_UINT16		uTextureIdx;

	/* Index of the chunk of texture-data to be loaded using these state words */
	IMG_UINT16		uChunkIdx;

	/* index of the location where the first state word for this chunk should be loaded */
	IMG_UINT16		uDestIdx;
};

/*
	Possible locations for pre-compiled pixel-shader results
*/
typedef enum _USP_PC_PSRESULT_REGTYPE_
{
	USP_PC_PSRESULT_REGTYPE_TEMP	= 1,
	USP_PC_PSRESULT_REGTYPE_OUTPUT	= 2,
	USP_PC_PSRESULT_REGTYPE_PA		= 3
}USP_PC_PSRESULT_REGTYPE, *PUSP_PC_PSRESULT_REGTYPE;

/*
	Possible data-formats for the pre-compiled pixel-shader results
*/	
typedef enum _USP_PC_PSRESULT_REGFMT_
{
	USP_PC_PSRESULT_REGFMT_F32	= 1,
	USP_PC_PSRESULT_REGFMT_F16	= 2,
	USP_PC_PSRESULT_REGFMT_C10	= 3,
	USP_PC_PSRESULT_REGFMT_U8	= 4
}USP_PC_PSRESULT_REGFMT, *PUSP_PC_PSRESULT_REGFMT;

/*
	Flags indicating which SA-register data may be required 
	(for USP_PC_BLOCK_PROGDESC::uSAUsageFlags)
*/
#define USP_PC_SAUSAGEFLAG_MEMCONSTBASE		0x00000001
#define USP_PC_SAUSAGEFLAG_MEMCONSTBASE2	0x00000002
#define USP_PC_SAUSAGEFLAG_SCRATCHMEMBASE	0x00000004
#define USP_PC_SAUSAGEFLAG_REMAPPEDCONSTS	0x00000008
#define USP_PC_SAUSAGEFLAG_INDEXTEMPMEMBASE	0x00000010
#define USP_PC_SAUSAGEFLAG_TEXTURESTATE		0x00000020

/*
	Data describing a constant used by the program (either referenced from
	memory or a register)
*/
typedef enum _USP_PC_SHADERTYPE_
{
	USP_PC_SHADERTYPE_PIXEL		= 0,
	USP_PC_SHADERTYPE_VERTEX	= 1,
	USP_PC_SHADERTYPE_GEOMETRY	= 2,	
}USP_PC_SHADERTYPE, *PUSP_PC_SHADERTYPE;

/*
	Data-block containing details about the precompiled program
*/
struct _USP_PC_BLOCK_PROGDESC_
{	
	/* 
		Type of Shader either Pixel or Vertex or Geometry.
	*/
	IMG_UINT32					uShaderType;
	/* General flags describing the precompiled program (see USP_PC_PROGDESC_FLAGS_xxx) */
	IMG_UINT32					uFlags;

	/* Flags supplied when the program was compiled (see UF_xxx) */
	IMG_UINT32					uUFFlags;

	/* Flags associated with the compiled code (see UNIFLEX_HW_FLAGS_xxx) */
	IMG_UINT32					uHWFlags;

	/* Target processor for compilation (currently one of SGX_CORE_ID_xxx) */
	IMG_UINT32					uTargetDev;

	/* Target processor Revision for compilation */
	IMG_UINT32					uTargetRev;

	/* 
		The number of entries in the list of BRNs that were applied when the 
		precompiled-code was generated
	*/
	IMG_UINT32					uBRNCount;

	/* 
	   Flags indicating which of the secondary attribute offsets 
	   contain valid values (see USP_PC_SAUSAGEFLAG_xxx above)
	*/							
	IMG_UINT32					uSAUsageFlags;
	
	/*
		Secondary attribute which holds the base address of the
		constant buffer.
	*/
	IMG_UINT16					uMemConstBaseAddrSAReg;

	/*
		Secondary attribute which holds the base address of the
		secondary constant buffer.
	*/
	IMG_UINT16					uMemConst2BaseAddrSAReg;

	/*
		Secondary attribute which holds the base address of the
		scratch area.
	*/
	IMG_UINT16					uScratchMemBaseAddrSAReg;

	/*
		Start of the secondary attributes which hold the texture
		control words.
	*/
	IMG_UINT16					uTexStateFirstSAReg;

	/*
		Byte-offset in memory where texture-state should be placed by the USP
		(this offset represents the end of any constants placed in memory by
		the USC)
	*/
	IMG_UINT32					uTexStateMemOffset;

	/* First SA reg available for holding in-reg constants */
	IMG_UINT16					uRegConstsFirstSAReg;

	/* Maximum number of SA registers available for in-reg constants */
	IMG_UINT16					uRegConstsMaxSARegCount;

	/*
		Secondary attribute which holds the base address of the memory
		allocated for indexable temporary registers.
	*/
	IMG_UINT16					uIndexedTempBaseAddrSAReg;

	/* Number of registers required by extra iterations into the primary attributes */
	IMG_UINT16					uExtraPARegs;

	/* Count of the primary attribute registers used in the program */
	IMG_UINT16					uPARegCount;

	/* Count of the temporary registers used in the program */
	IMG_UINT16					uTempRegCount;

	/* Count of the temporary registers used in the secondary update program */
	IMG_UINT16					uSecTempRegCount;

	/* The total size of the indexable temporary storage required (in bytes) */
	IMG_UINT32					uIndexedTempTotalSize;

	/*
		Number of non-dependent texture loads and coordinate iterations required
		for the program

		NB: These are described in an array of USP_PC_PSINPUT_LOAD structs
			immediately following the list of indexable temporary arrays.
	*/
	IMG_UINT16					uPSInputCount;

	/*
		The number of constants which have been moved to secondary attributes

		NB: The remapped constants are described in an array of USP_PC_CONST_LOAD
			structs immediately following the list of in-memory remapped constants.
	*/
	IMG_UINT16					uRegConstCount;

	/*
		If constant remapping was enabled then the number of constants used
		in the program

		NB: The remapped constants are described in an array of USP_PC_CONST_LOAD
			immediately following the non-dependent texture/iteration list.
	*/
	IMG_UINT32					uMemConstCount;

	/*
		The number of sets of texture-state words required by the program

		NB: The list of texture-states to load are contained in an array of
			USP_HW_TEXSTATE_LOAD structures immediately following the list
			of register constants
	*/
	IMG_UINT16					uRegTexStateCount;

	/* Texture-stages/Samplers that should have anisotropic filtering disabled. */
	IMG_UINT32					auNonAnisoTexStages[(USP_MAX_SAMPLER_IDX + 31) / 32];

	/* Texture stages that have anisotropic filtering enabled */
	IMG_UINT32					auAnisoTexStages[(USP_MAX_SAMPLER_IDX + 31) / 32];

	/* Required size (in dwords) of the scratch area used */
	IMG_UINT32					uScratchAreaSize;

	/*
		The number of bytes difference between the address of a memory buffer used by the
		program and the address stored in the secondary attribute. Always set to -4 at the
		moment.
	*/
	IMG_INT16					iSAAddressAdjust;

	/*
		For pixel-shaders, the register-type where the shader result is stored.
		(see USP_PC_PSRESULT_REGTYPE type)
	*/
	IMG_UINT16					uPSResultRegType;

	/*
		For pixel-shaders, the register number where the shader result is
		stored.
	*/
	IMG_UINT16					uPSResultRegNum;

	/*
		For pixel-shaders, the data-format used to store the shader result
		(see USP_PC_PSRESULT_REGFMT type)
	*/
	IMG_UINT16					uPSResultRegFmt;

	/*
		For pixel-shaders the number of registers used for the result
	*/
	IMG_UINT16					uPSResultRegCount;

	/*
		For pixel-shaders, temp-register location for the shader-results
	*/
	IMG_UINT16					uPSResultTempReg;

	/*
		For pixel-shaders, PA-register location for the shader-results
	*/
	IMG_UINT16					uPSResultPAReg;

	/*
		For pixel-shaders, output-register location for the shader-results
	*/
	IMG_UINT16					uPSResultOutputReg;

	/* Label ID marking the start of the program */
	IMG_UINT16					uProgStartLabelID;

	/* Label ID marking of the end of the alpha-calc/test phase */
	IMG_UINT16					uPTPhase0EndLabelID;

	/* Label ID marking of the start of the phase following alpha-cal/test */
	IMG_UINT16					uPTPhase1StartLabelID;
	
	/* Label ID marking of the start of the phase following split of program */
	IMG_UINT16					uPTSplitPhase1StartLabelID;

	/* Count of instructions in the secondary update program */
	IMG_UINT16					uSAUpdateInstCount;

	/*
		Overall number of secondary attributes used for constants (including those
		calculated in the secondary update program).
	*/
	IMG_UINT16					uRegConstsSARegCount;

	/*
		Number of PA registers used for VS inputs (not including pre-sampled texture-data)
	*/
	IMG_UINT16					uVSInputPARegCount;

	/*
		Structure is immediately followed by following data-arrays
		(in the order shown):

		IMG_UINT32			 x uBRNCount					// BRNs applied
		USP_PC_PSINPUT_LOAD  x uPSInputCount				// iterated/pre-sampled PS inputs
		USP_PC_CONST_LOAD    x uMemConstCount				// constants to load to memory
		USP_PC_CONST_LOAD    x uRegConstCount				// constants to load into registers
		USP_PC_TEXSTATE_LOAD x uRegTexStateCount			// texture-state to load into memory
		IMG_PUINT32 x 2		 x uSAProgInstCount				// reg-constant calc program
		IMG_UINT32			 x (uVSInputPARegCount / 32)	// VS input usage info
	*/
	/* 
		Count of Shader Outputs After Compilation. 
	*/
	IMG_UINT16					uShaderOutputsCount;
};

/*
	Information describing a pre-compiled instruction	
*/
#define USP_PC_INSTDESC_FLAG_IREGS_LIVE_BEFORE		0x0001
#define USP_PC_INSTDESC_FLAG_DONT_SKIPINV			0x0004

#define USP_PC_INSTDESC_FLAG_DEST_USES_RESULT		0x0010
#define USP_PC_INSTDESC_FLAG_SRC0_USES_RESULT		0x0020
#define USP_PC_INSTDESC_FLAG_SRC1_USES_RESULT		0x0040
#define USP_PC_INSTDESC_FLAG_SRC2_USES_RESULT		0x0080

struct _USP_PC_INSTDESC_
{
	/* Flags describing the pre-compiled instruction */
	IMG_UINT16		uFlags;
};

/*
	Number of MOE increments, swizzles and base-offsets
*/
#define USP_PC_MOE_OPERAND_COUNT			4

/*
	MOE-state flags	
*/
#define USP_PC_MOE_FMTCTLFLAG_EFOFMTCTL		0x00000001U
#define USP_PC_MOE_FMTCTLFLAG_COLFMTCTL		0x00000002U

/*
	Definitions for USP_PC_MOESTATE::aiMOEIncOrSwizzle
*/
#define USP_PC_MOE_INCORSWIZ_MODE_INC		0x8000U
#define USP_PC_MOE_INCORSWIZ_MODE_SWIZ		0x4000U

#define USP_PC_MOE_INCORSWIZ_INC_CLRMSK		0xFF00U
#define USP_PC_MOE_INCORSWIZ_INC_SHIFT		0

#define USP_PC_MOE_INCORSWIZ_SWIZ_CLRMSK	0xFF00U
#define USP_PC_MOE_INCORSWIZ_SWIZ_SHIFT		0

/*
	Pre-compiled MOE state
*/
struct _USP_PC_MOESTATE_
{
	/* MOE format-control state flags */
	IMG_UINT16	uMOEFmtCtlFlags;

	/* MOE increment/swizzle, for each of the operands */
	IMG_UINT16	auMOEIncOrSwiz[USP_PC_MOE_OPERAND_COUNT];

	/* MOE base-offset, for each of the operands */
	IMG_UINT16	auMOEBaseOffset[USP_PC_MOE_OPERAND_COUNT];
};

/*
	Data-block containing precompiled USSE binary code	
*/
struct _USP_PC_BLOCK_HWCODE_
{
	/* The mumber of following precompiled instructions */
	IMG_UINT16		uInstCount;

	/* The MOE state at the start of the block of instructions */
	USP_PC_MOESTATE	sMOEState;

	/*
		Structure immediately followed by... 

		uInstCount x USP_PC_INSTDESC
		uInstCount x HW instruction
	*/
};

/*
	Data block containing information about a texture sample
*/
#define	USP_MAX_TEX_CHANNELS	(4)
#define USP_MAX_TEX_DIM			(3)

#define USP_PC_BLOCK_SAMPLE_FLAG_NONDR		0x0001
#define USP_PC_BLOCK_SAMPLE_FLAG_PROJECTED	0x0002
#define USP_PC_BLOCK_SAMPLE_FLAG_CENTROID	0x0004

typedef enum _USP_PC_SAMPLE_DEST_REGFMT_
{
	USP_PC_SAMPLE_DEST_REGFMT_F32	= 1,
	USP_PC_SAMPLE_DEST_REGFMT_F16	= 2,
	USP_PC_SAMPLE_DEST_REGFMT_C10	= 3,
	USP_PC_SAMPLE_DEST_REGFMT_U8	= 4
	,USP_PC_SAMPLE_DEST_REGFMT_U16  = 5
	,USP_PC_SAMPLE_DEST_REGFMT_I16  = 6
	,USP_PC_SAMPLE_DEST_REGFMT_U32	= 7
	,USP_PC_SAMPLE_DEST_REGFMT_I32	= 8
	,USP_PC_SAMPLE_DEST_REGFMT_UNKNOWN = 9
}USP_PC_SAMPLE_DEST_REGFMT, *PUSP_PC_SAMPLE_DEST_REGFMT;

typedef enum _USP_PC_SAMPLE_DEST_REGTYPE_
{
	USP_PC_SAMPLE_DEST_REGTYPE_TEMP		= 1,
	USP_PC_SAMPLE_DEST_REGTYPE_OUTPUT	= 2,
	USP_PC_SAMPLE_DEST_REGTYPE_PA		= 3,
	USP_PC_SAMPLE_DEST_REGTYPE_INTERNAL	= 4
}USP_PC_SAMPLE_DEST_REGTYPE, *PUSP_PC_SAMPLE_DEST_REGTYPE;

typedef enum _USP_PC_SAMPLE_TEMP_REGTYPE_
{
	USP_PC_SAMPLE_TEMP_REGTYPE_TEMP	= 1,
	USP_PC_SAMPLE_TEMP_REGTYPE_PA	= 2
}USP_PC_SAMPLE_TEMP_REGTYPE, *PUSP_PC_SAMPLE_TEMP_REGTYPE;

typedef enum _USP_PC_TEXTURE_WRITE_REGTYPE_
{
	USP_PC_TEXTURE_WRITE_REGTYPE_TEMP     = 1,
	USP_PC_TEXTURE_WRITE_REGTYPE_PA       = 2,
	USP_PC_TEXTURE_WRITE_REGTYPE_SA       = 3,
	USP_PC_TEXTURE_WRITE_REGTYPE_SPECIAL  = 4,
	USP_PC_TEXTURE_WRITE_REGTYPE_INTERNAL = 5,
	USP_PC_TEXTURE_WRITE_REGTYPE_UNKNOWN  = 6
}USP_PC_TEXTURE_WRITE_REGTYPE, *PUSP_PC_TEXTURE_WRITE_REGTYPE;

typedef enum _USP_PC_SAMPLE_COORD_
{
	USP_PC_SAMPLE_COORD_TC0	= 1,
	USP_PC_SAMPLE_COORD_TC1	= 2,
	USP_PC_SAMPLE_COORD_TC2	= 3,
	USP_PC_SAMPLE_COORD_TC3	= 4,
	USP_PC_SAMPLE_COORD_TC4	= 5,
	USP_PC_SAMPLE_COORD_TC5	= 6,
	USP_PC_SAMPLE_COORD_TC6	= 7,
	USP_PC_SAMPLE_COORD_TC7	= 8,
	USP_PC_SAMPLE_COORD_TC8	= 9,
	USP_PC_SAMPLE_COORD_TC9	= 10
}USP_PC_SAMPLE_COORD, *PUSP_PC_SAMPLE_COORD;

struct _USP_PC_BLOCK_SAMPLE_
{
	/* ID of USP sample */
	IMG_UINT32		uSmpID;

	/* Flags describing the sample */
	IMG_UINT16		uFlags;

	/* The MOE state prior to the sample */
	USP_PC_MOESTATE	sMOEState;

	/* instruciton description for the sample */
	USP_PC_INSTDESC	sInstDesc;

	/* Index of the texture/sampler to use */
	IMG_UINT16		uTextureIdx;

#if 0
	/* Mask indicating which destination components should be written */
	IMG_UINT16		uMask;

	/* Mask indicating which destination components are live */
	IMG_UINT16		uLive;
#endif

	/* How the sampled texture channels should be mapped to destination components */
	IMG_UINT16		uSwizzle;

#if 0
	/* register type for the sampled data (per channel sampled) */
	IMG_UINT16		auDestRegType[USP_MAX_TEX_CHANNELS];

	/* register number for the sampled data (per channel sampled) */
	IMG_UINT16		auDestRegNum[USP_MAX_TEX_CHANNELS];

	/* required data format for the sampled data (per channel sampled) */
	IMG_UINT16		auDestRegFmt[USP_MAX_TEX_CHANNELS];

	/* component index for the sampled data (per channel sampled )*/
	IMG_UINT16		auDestRegComp[USP_MAX_TEX_CHANNELS];
#endif

	/* Type of the registers for the sampled data */
	IMG_UINT16		uBaseSampleDestRegType;

	/* Index of the first register for the sampled data */
	IMG_UINT16		uBaseSampleDestRegNum;

	/* Type of the registers for direct sampling of data without involving unpacking */
	IMG_UINT16		uDirectDestRegType;

	/* Index of the first register for the sampling of data without involving unpacking */
	IMG_UINT16		uDirectDestRegNum;

	/* Type of the registers for the sampled data (see USP_PC_SAMPLE_TEMP_REGTYPE) */
	IMG_UINT16		uSampleTempRegType;

	/* Index of the first register for the sampled data */
	IMG_UINT16		uSampleTempRegNum;

	/* Internal registers live before block */
	IMG_UINT16		uIRegsLiveBefore;

	/* Internal registers live in C10 format before block */
	IMG_UINT16		uC10IRegsLiveBefore;

	/* Original texture precision given to the sample input instruction */
	IMG_UINT16      uTexPrecision;

	/* This structure immediately followed by one of the following... */
};

struct _USP_PC_DR_SAMPLE_DATA_
{
	/* Base HW sample instruction (replicated according to texture-format) */
	IMG_UINT32	auBaseSmpInst[2];
};

struct _USP_PC_NONDR_SAMPLE_DATA_
{
	/* The texture-coordinates to use (see USP_PC_SAMPLE_COORD) */
	IMG_UINT16	uCoord;

	/* dimensionality of the texture */
	IMG_UINT16	uTexDim;

	/* Type of the registers for direct sampling of data without involving unpacking */
	IMG_UINT16		uSmpNdpDirectSrcRegType;

	/* Index of the first register for the sampling of data without involving unpacking */
	IMG_UINT16		uSmpNdpDirectSrcRegNum;
};

struct _USP_PC_BLOCK_SAMPLEUNPACK_
{
	/* The MOE state prior to the sample */
	USP_PC_MOESTATE	sMOEState;
	
	/* instruciton description for the sample */
	USP_PC_INSTDESC	sInstDesc;

	/* ID of USP sample */
	IMG_UINT32		uSmpID;

	/* Mask indicating which destination components should be written */
	IMG_UINT16		uMask;

	/* Mask indicating which destination components are live */
	IMG_UINT16		uLive;

	/* register type for the sampled data (per channel sampled) */
	IMG_UINT16		auDestRegType[USP_MAX_TEX_CHANNELS];

	/* register number for the sampled data (per channel sampled) */
	IMG_UINT16		auDestRegNum[USP_MAX_TEX_CHANNELS];

	/* required data format for the sampled data (per channel sampled) */
	IMG_UINT16		auDestRegFmt[USP_MAX_TEX_CHANNELS];

	/* Type of the registers for direct sampling of data without involving unpacking */
	IMG_UINT16		uDirectSrcRegType;

	/* Index of the first register for the sampling of data without involving unpacking */
	IMG_UINT16		uDirectSrcRegNum;

	/* component index for the sampled data (per channel sampled )*/
	IMG_UINT16		auDestRegComp[USP_MAX_TEX_CHANNELS];

	/* Type of the registers for the sampled data (see USP_PC_SAMPLE_TEMP_REGTYPE) */
	IMG_UINT16		uBaseSampleUnpackSrcRegType;

	/* Index of the first register for the sampled data */
	IMG_UINT16		uBaseSampleUnpackSrcRegNum;

	/* Type of the registers for temporary data (see USP_PC_SAMPLE_TEMP_REGTYPE) */
	IMG_UINT16		uSampleUnpackTempRegType;

	/* Index of the first register for the temporary data */
	IMG_UINT16		uSampleUnpackTempRegNum;

	/* Internal registers live before block */
	IMG_UINT16		uIRegsLiveBefore;

};

/*
	Data block containing information about a branch within a shader
*/
struct _USP_PC_BLOCK_BRANCH_
{
	/* USSE instruction for the branch */
	IMG_UINT32	auBaseBranchInst[2];

	/* ID of the label representing the branch/call destination */
	IMG_UINT16	uDestLabelID;
};

/*
	Data block containing information about a label within a shader
*/
struct _USP_PC_BLOCK_LABEL_
{
	/* Unique ID for the label */
	IMG_UINT16	uLabelID;
};

/*
    Data block containing information about an texture write
	within a shader
 */
struct _USP_PC_BLOCK_TEXTURE_WRITE_
{
	/* Start of the range of temporary registers available for the USP generated texture unpacking code. */
	IMG_UINT32		uTempRegStartNum;

	/* ID of USP texture write instruction this instruction belongs to. */
	IMG_UINT32 		uWriteID;

	/* Instruction description for the sample */
	USP_PC_INSTDESC	sInstDesc;

	/* Register number and type of the register containing the base address */
	IMG_UINT32		uBaseAddrRegNum;
	IMG_UINT32		uBaseAddrRegType;
	
	/* Register number and type of the register containing the stride */
	IMG_UINT32		uStrideRegNum;
	IMG_UINT32		uStrideRegType;

	/* Register numbers and types of the registers containing the co-ordinates */
	IMG_UINT32		uCoordXRegNum;
	IMG_UINT32		uCoordXRegType;

	IMG_UINT32		uCoordYRegNum;
	IMG_UINT32		uCoordYRegType;

	/* Register numbers and types of the registers containing the channel data */
	IMG_UINT32		uChannelXRegNum;
	IMG_UINT32		uChannelXRegType;

	IMG_UINT32		uChannelYRegNum;
	IMG_UINT32		uChannelYRegType;

	IMG_UINT32		uChannelZRegNum;
	IMG_UINT32		uChannelZRegType;

	IMG_UINT32		uChannelWRegNum;
	IMG_UINT32		uChannelWRegType;

	/* The format of the channel data */
	IMG_UINT32		uChannelRegFmt;

	/* The maximum number of temps we can use */
	IMG_UINT32		uMaxNumTemps;
};

#endif	/* #ifndef _USPBIN_H_ */
/*****************************************************************************
 End of file (USPBIN.H)
*****************************************************************************/
