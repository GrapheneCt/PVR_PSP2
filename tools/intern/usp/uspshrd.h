 /*****************************************************************************
 Name			: USPSHRD.H
 
 Title			: Internal definitions and prototypes for USP
 
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

 Description 	: Definitions and declarations used internally by the USP
   
 Program Type	: 32-bit DLL

 Version	 	: $Revision: 1.68 $

 Modifications	:

 $Log: uspshrd.h $
*****************************************************************************/
#ifndef _USPSHRD_H_
#define _USPSHRD_H_


#define USP_MAX_SAMPLE_CHANS	(4)
#define USP_MAX_TEXTURE_CHUNKS	(4)
#define USP_MAX_ITERATED_COMPS	(4)

#define USP_MAX_SAMPLE_ID		((IMG_UINT32)(-1))

/*
	Internal per channel texture format description.
*/
typedef enum _USP_CHAN_CONTENT_
{
	USP_CHAN_CONTENT_NONE	= 0,
	USP_CHAN_CONTENT_DATA	= 1,
	USP_CHAN_CONTENT_Y		= 2,
	USP_CHAN_CONTENT_U		= 3,
	USP_CHAN_CONTENT_V		= 4,
	USP_CHAN_CONTENT_ONE	= 5,
	USP_CHAN_CONTENT_ZERO	= 6
}USP_CHAN_CONTENT, *PUSP_CHAN_CONTENT;

typedef enum _USP_CHAN_FORMAT_
{
	USP_CHAN_FORMAT_U8		= 0,
	USP_CHAN_FORMAT_S8		= 1,
	USP_CHAN_FORMAT_U16		= 2,
	USP_CHAN_FORMAT_S16		= 3,
	USP_CHAN_FORMAT_F16		= 4,
	USP_CHAN_FORMAT_F32		= 5,
	USP_CHAN_FORMAT_U24		= 6,
	USP_CHAN_FORMAT_U32     = 7,
	USP_CHAN_FORMAT_S32     = 8,
	USP_CHAN_FORMAT_INVALID	= 9,
}USP_CHAN_FORMAT, *PUSP_CHAN_FORMAT;

/*
	The internal channel representation to fetch texture data.
*/
typedef struct _USP_TEX_CHAN_FORMAT_
{
	/* The content of each channel within data sampled from this texture */
	USP_CHAN_CONTENT	aeChanContent[USP_TEXFORM_CHAN_COUNT];

	/* The data-format of each channel within data sampled from this texture */
	USP_CHAN_FORMAT		aeChanForm[USP_TEXFORM_CHAN_COUNT];

	/* The expected channel arrangement */
	USP_CHAN_SWIZZLE	aeChanSwizzle[USP_TEXFORM_CHAN_COUNT];

	/* The amount of data to be read by each sample of this texture */
	IMG_UINT32			auChunkSize[USP_TEXFORM_CHAN_COUNT];
}USP_TEX_CHAN_FORMAT, *PUSP_TEX_CHAN_FORMAT;

/*
	The actual input texture format.
*/
struct _USP_SAMPLER_DESC_
{
	USP_TEX_FORMAT		sTexFormat;
	IMG_BOOL			bAnisoEnabled;
	IMG_BOOL			bGammaEnabled;
	IMG_BOOL			bNormalizeCoords;
	IMG_BOOL			bLinearFilter;
	IMG_UINT32			uTextureWidth;
	IMG_UINT32			uTextureHeight;
};

#define USP_DESTMASK_FULL	(0xF)

/*
	Parameters associated with a texture-sampler. 
	Now used internally only and are part of USP_SHADER.
*/
typedef struct _USP_SAMPLER_CHAN_DESC_
{
	USP_TEX_CHAN_FORMAT	sTexChanFormat;
	IMG_BOOL			bAnisoEnabled;
	IMG_BOOL			bGammaEnabled;
}USP_SAMPLER_CHAN_DESC, *PUSP_SAMPLER_CHAN_DESC;

/*
	Unipatch execution context
*/
struct _USP_CONTEXT_
{
	/* Caller-supplied memory alloc function */
	USP_MEMALLOCFN				pfnAlloc;

	/* Caller-supplied memory free function */
	USP_MEMFREEFN				pfnFree;

	/* Caller-supplied debug print output function */
	USP_DBGPRINTFN				pfnPrint;

	/* Data describing each sampler and associated texture */
	USP_SAMPLER_DESC			asSamplerDesc[USP_MAX_SAMPLER_IDX];

	/* The required register-type for shader output data */
	USP_OUTPUT_REGTYPE			eOutputRegType;

	/* The number of preamble instructions to add at the start of the shader */
	IMG_UINT32					uPreambleInstCount;
};

/*
	Collected meta-data describing the precompiled program
*/
struct _USP_PROGDESC_
{	
	IMG_UINT32						uShaderType;
	IMG_UINT32						uFlags;	
	IMG_UINT32						uUFFlags;
	IMG_UINT32						uHWFlags;

	SGX_CORE_INFO					sTargetDev;
	IMG_UINT32						uBRNCount;
	IMG_PUINT32						puBRNList;

	IMG_UINT32						uSAUsageFlags;
	IMG_UINT16						uMemConstBaseAddrSAReg;
	IMG_UINT16						uMemConst2BaseAddrSAReg;
	IMG_UINT16						uScratchMemBaseAddrSAReg;
	IMG_UINT16						uTexStateFirstSAReg;
	IMG_UINT32						uTexStateMemOffset;
	IMG_UINT16						uRegConstsFirstSAReg;
	IMG_UINT16						uRegConstsSARegCount;
	IMG_UINT16						uRegConstsMaxSARegCount;
	IMG_UINT16						uIndexedTempBaseAddrSAReg;
	IMG_UINT32						uIndexedTempTotalSize;

	IMG_UINT16						uExtraPARegs;
	IMG_UINT16						uPARegCount;
	IMG_UINT16						uTempRegCount;
	IMG_UINT16						uSecTempRegCount;

	IMG_UINT16						uPSInputCount;
	PUSP_PC_PSINPUT_LOAD			psPSInputLoads;

	IMG_UINT32						uMemConstCount;
	PUSP_PC_CONST_LOAD				psMemConstLoads;

	IMG_UINT16						uRegConstCount;
	PUSP_PC_CONST_LOAD				psRegConstLoads;

	IMG_UINT16						uRegTexStateCount;
	PUSP_PC_TEXSTATE_LOAD			psRegTexStateLoads;

	IMG_UINT32						auNonAnisoTexStages[(USP_MAX_SAMPLER_IDX + 31) / 32];
	IMG_UINT32						auAnisoTexStages[(USP_MAX_SAMPLER_IDX + 31) / 32];
	IMG_UINT32						uScratchAreaSize;
	IMG_INT16						iSAAddressAdjust;

	USP_REGTYPE						eDefaultPSResultRegType;
	IMG_UINT16						uDefaultPSResultRegNum;
	USP_REGFMT						eDefaultPSResultRegFmt;
	IMG_UINT16						uDefaultPSResultRegCount;
	IMG_UINT16						uPSResultFirstTempReg;
	IMG_UINT16						uPSResultFirstPAReg;
	IMG_UINT16						uPSResultFirstOutputReg;

	IMG_UINT16						uProgStartInstIdx;
	IMG_UINT16						uPTPhase0EndInstIdx;
	IMG_UINT16						uPTPhase1StartInstIdx;
	IMG_UINT16						uPTSplitPhase1StartInstIdx;

	IMG_UINT16						uSAUpdateInstCount;
	PHW_INST						psSAUpdateInsts;

	IMG_UINT16						uProgStartLabelID;
	IMG_UINT16						uPTPhase0EndLabelID;
	IMG_UINT16						uPTPhase1StartLabelID;
	IMG_UINT16						uPTSplitPhase1StartLabelID;

	IMG_UINT32						uVSInputPARegCount;
	IMG_UINT32						auVSInputsUsed[EURASIA_USE_PRIMATTR_BANK_SIZE/32];
	IMG_UINT32						uShaderOutputsCount;
	IMG_UINT32*						puValidShaderOutputs;
};

/*
	Data describing interated input to a pixel-shader
*/
typedef enum _USP_ITERATED_DATA_TYPE_
{
	USP_ITERATED_DATA_TYPE_TC0	= 0,
	USP_ITERATED_DATA_TYPE_TC1	= 1,
	USP_ITERATED_DATA_TYPE_TC2	= 2,
	USP_ITERATED_DATA_TYPE_TC3	= 3,
	USP_ITERATED_DATA_TYPE_TC4	= 4,
	USP_ITERATED_DATA_TYPE_TC5	= 5,
	USP_ITERATED_DATA_TYPE_TC6	= 6,
	USP_ITERATED_DATA_TYPE_TC7	= 7,
	USP_ITERATED_DATA_TYPE_TC8	= 8,
	USP_ITERATED_DATA_TYPE_TC9	= 9,
	USP_ITERATED_DATA_TYPE_V0	= 10,
	USP_ITERATED_DATA_TYPE_V1	= 11,
	USP_ITERATED_DATA_TYPE_FOG	= 12,
	USP_ITERATED_DATA_TYPE_POS	= 13,
	USP_ITERATED_DATA_TYPE_VPRIM = 14,
	USP_ITERATED_DATA_TYPE_PADDING = 15,
}USP_ITERATED_DATA_TYPE, *PUSP_ITERATED_DATA_TYPE;

struct _USP_ITERATED_DATA_
{
	/* The value being iterated */
	USP_ITERATED_DATA_TYPE	eType;

	/* Number of components iterated */
	IMG_UINT32				uCompCount;

	/* Whether the iterated data needs centroid adjust enabled */
	IMG_BOOL				bCentroid;

	/* Register-format for the iterated data */
	USP_REGFMT				eRegFmt;

	/* Overall number of consecutive registers used by the iterated data */
	IMG_UINT32				uRegCount;

	/* Register index for the first iterated component */
	IMG_UINT32				uRegNum;

	/* Register offset for each iterated component */
	IMG_UINT32				aeRegOff[USP_MAX_ITERATED_COMPS];

	/* Register channel for each iterated component */
	IMG_UINT32				aeRegChan[USP_MAX_ITERATED_COMPS];

	/* Iteration size */
	IMG_UINT16				uItrSize;
};

/*
	Data describing a pre-sampled texture
*/
struct _USP_PRESAMPLED_DATA_
{
	/* Index of the sampled texture */
	IMG_UINT32				uTextureIdx;

	/* Which chunk of the texture is sampled */
	IMG_UINT32				uChunkIdx;

	/* Index of the texture control words entry for this presampled data */
	IMG_UINT16				uTexCtrWrdIdx;

	/* The texture coordinates used for the sample */
	USP_ITERATED_DATA_TYPE	eCoord;

	/* Whether this was a projected sample */
	IMG_BOOL				bProjected;

	/* Whether this sample needs centroid adjust enabled */
	IMG_BOOL				bCentroid;

	/* Number of registers used by the pre-sampled chunk */
	IMG_UINT32				uRegCount;

	/* Register index for the pre-sampled data for the chunk */
	IMG_UINT32				uRegNum;

	/* Size of data returned by sample in DWORDS */
	IMG_UINT16				uTagSize;

	/* Format conversion for presampled data */
	USP_REGFMT				eFmtConvert;

};

/*
	Data describing the texture-state for each sampled texture
*/
struct _USP_TEXSTATE_DATA_
{
	/* Index of the associated texture */
	IMG_UINT32		uTextureIdx;

	/* Which chunks of the texture the state is for */
	IMG_UINT32		uChunkIdx;

	/* Index of the texture control words entry for this texture state */
	IMG_UINT16		uTexCtrWrdIdx;

	/* Number of registers used by the state data */
	IMG_UINT32		uRegCount;

	/* Register index for the state-data */
	IMG_UINT32		uRegNum;

	/* Indicates whether this texture-state nust be sourced from memory */
	IMG_BOOL		bInMemory;

	/* Size of data returned by sample in DWORDS */
	IMG_UINT16		uTagSize;
};

/*
	Data describing all the input-data for a shader
*/
struct _USP_INPUT_DATA_
{
	/* The number of iterated-items used by the shader */
	IMG_UINT32				uIteratedDataCount;

	/* The initial number of iterated-items used by the shader */
	IMG_UINT32				uOrgIteratedDataCount;

	/* The maximum possible number of iterated items */
	IMG_UINT32				uMaxIteratedData;

	/* The list of iterated items used by the shader */
	PUSP_ITERATED_DATA		psIteratedData;

	/* The number of pre-sampled texture-chunks used by the shader */
	IMG_UINT32				uPreSampledDataCount;

	/* The initial number of pre-sampled texture-chunks used by the shader */
	IMG_UINT32				uOrgPreSampledDataCount;

	/* The maximum possible number of pre-sampled texture-chunks */
	IMG_UINT32				uMaxPreSampledData;

	/* The list of pre-sampled texture-chunks used by the shader */
	PUSP_PRESAMPLED_DATA	psPreSampledData;

	/* The number of sets of texture-state words used by the shader */
	IMG_UINT32				uTexStateDataCount;

	/* The number of sets of texture-state words to be loaded into registers */
	IMG_UINT32				uRegTexStateDataCount;

	/* The number of sets of texture-state words to be loaded into memory */
	IMG_UINT32				uMemTexStateDataCount;

	/* The maximum possible number of sets of texture-state words */
	IMG_UINT32				uMaxTexStateData;

	/* The list of sets of texture-state words used by the shader */
	PUSP_TEXSTATE_DATA		psTexStateData;
};

/*
	MOE-state for a block of instrucitons
*/
#define USP_MOESTATE_OPERAND_COUNT	(4)

struct _USP_MOESTATE_
{
	IMG_BOOL	abUseSwiz[USP_MOESTATE_OPERAND_COUNT];
	IMG_INT32	aiInc[USP_MOESTATE_OPERAND_COUNT];
	IMG_UINT32	auSwiz[USP_MOESTATE_OPERAND_COUNT];
	IMG_UINT32	auBaseOffset[USP_MOESTATE_OPERAND_COUNT];
	IMG_BOOL	bEFOFmtCtl;
	IMG_BOOL	bColFmtCtl;
};

/*
	Information associated with each instruction in a block
*/
#define USP_INSTDESC_FLAG_PRECOMPILED_INST			0x00000001
#define USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE			0x00000002
#define USP_INSTDESC_FLAG_SUPPORTS_NOSCHED			0x00000004
#define USP_INSTDESC_FLAG_SYNCSTART_SET				0x00000008
#define USP_INSTDESC_FLAG_FORCES_DESCHEDULE			0x00000010
#define USP_INSTDESC_FLAG_PARTIAL_RESULT_WRITE		0x00000040
#define USP_INSTDESC_FLAG_SUPPORTS_SRC0_EXTBANKS	0x00000080
#define USP_INSTDESC_FLAG_DEST_USES_RESULT			0x00000100
#define USP_INSTDESC_FLAG_SRC0_USES_RESULT			0x00000200
#define USP_INSTDESC_FLAG_SRC1_USES_RESULT			0x00000400
#define USP_INSTDESC_FLAG_SRC2_USES_RESULT			0x00000800
#define USP_INSTDESC_FLAG_DONT_SKIPINV				0x00001000
#define USP_INSTDESC_FLAG_SYNCEND_SET				0x00002000

struct _USP_INSTDESC_
{
	IMG_UINT32	uFlags;
	USP_OPCODE	eOpcode;
	USP_FMTCTL	eFmtCtl;
};

/*
	An entry for the list of instructions forming an instruction block
*/
struct _USP_INST_
{
	/* The block that this instruciton if part of */
	PUSP_INSTBLOCK			 psInstBlock;

	/* The encoded HW instruction */
	HW_INST					sHWInst;

	/* Information describing features and requirements for the instruction */
	USP_INSTDESC			sDesc;

	/* The MOE state immediately prior to the instruction */
	USP_MOESTATE			sMOEState;

	/* The result-reference for this instruction */
	PUSP_RESULTREF			psResultRef;

	struct _USP_INST_*		psNext;
	struct _USP_INST_*		psPrev;
};

/*
	A block of contiguously executed HW instructions
*/
struct _USP_INSTBLOCK_
{
	/* The shader that this instruction-block is part of */
	PUSP_SHADER				psShader;

	/* The number of instructions currently in the block */
	IMG_UINT32				uInstCount;

	/* The maximum number of instructions available in the block */
	IMG_UINT32				uMaxInstCount;

	/* The original number of pre-compiled instructions associated with the block */
	IMG_UINT32				uOrgInstCount;

	/* The pool of instructions for the block */
	PUSP_INST				psInsts;

	/* The maximum number of USP generated instructions that can be added */
	IMG_UINT32				uMaxNonPCInstCount;

	/* The pool of result-references for the block */
	PUSP_RESULTREF			psResultRefs;

	/* The first instruction to be executed in the block */
	PUSP_INST				psFirstInst;

	/* The last instruction to be executed in the block */
	PUSP_INST				psLastInst;

	/* The MOE state at the start of the block */
	USP_MOESTATE			sInitialMOEState;

	/* The MOE state at the end of the block */
	USP_MOESTATE			sFinalMOEState;

	/* Index of the first instruction of the block, within the entire shader */
	IMG_UINT32				uFirstInstIdx;

	/* The list of pre-compiled instructions for the block */
	PHW_INST				psOrgInsts;

	/* The list of instruction descriptions for the original instructions */
	PUSP_INSTDESC			psOrgInstDescs;

	/* Indicates whether the start of the code for this block should be pair aligned */
	IMG_BOOL				bAlignStartToPair;

	/* Indicates that the start of this block is a branch destination */
	IMG_BOOL				bIsBranchDest;

	struct _USP_INSTBLOCK_*	psNext;
	struct _USP_INSTBLOCK_*	psPrev;
};

/*
	Data associated with a label (branch destination) within the shader
*/
struct _USP_LABEL_
{
	/* Unique ID for this label */
	IMG_UINT32				uID;

	/* The instruction block marked by this label */
	PUSP_INSTBLOCK			psInstBlock;

	struct _USP_LABEL_*		psNext;
	struct _USP_LABEL_*		psPrev;
};

/*
   Calculating the offset requires 1 x IMA16 and 2 x IMAE.
   However, each of these may also need a MOV if src0
   is an SA.
*/
#define USP_TEXTURE_WRITE_MAX_OFFSET_INSTS	(6)

/*
   Doing the format conversion requires:

   1xTEST
   1xXOR
   2xAND
   2xMOVC


   per channel and 2xLIMM to load the special case values and 2 packs
*/
#define USP_TEXTURE_WRITE_MAX_CONVERT_INSTS (((6) * USP_MAX_SAMPLE_CHANS) + 2 + 2)

#define USP_TEXTURE_WRITE_MAX_STORE_INSTS	(4)

#define USP_MAX_TEXTURE_WRITE_INSTS	(USP_TEXTURE_WRITE_MAX_OFFSET_INSTS + \
									 USP_TEXTURE_WRITE_MAX_CONVERT_INSTS + \
									 USP_TEXTURE_WRITE_MAX_STORE_INSTS)

struct _USP_TEXTURE_WRITE_
{
	/* Texture write ID */
	IMG_UINT32              uWriteID;

	/* First free temp number */
	IMG_UINT32				uTempRegStartNum;

	/* Max number of temps we can use */
	IMG_UINT32				uMaxNumTemps;

	/* Register containing the base address */
	USP_REG					sBaseAddr;

	/* Register containing the stride */
	USP_REG					sStride;

	/* Registers containing the co-ordinates */
	USP_REG					asCoords[2];

	/* Registers containing the channel data */
	USP_REG					asChannel[4];

	/* The instructon block containing the generated instructions */
	PUSP_INSTBLOCK			psInstBlock;

	/* The number of instructions generated */
	IMG_UINT32				uInstCount;

	/* The number of temporary registers used by the texture writing code */
	IMG_UINT32				uTempRegCount;

	/* HW-instructions generated to write to the texture */
	HW_INST					asInsts[USP_MAX_TEXTURE_WRITE_INSTS];

	/* Opcodes for each of the texture write instructions */
	USP_OPCODE				aeOpcodeForInsts[USP_MAX_TEXTURE_WRITE_INSTS];

	/* Instruction description flags for each sample-inst */
	IMG_UINT32				auInstDescFlagsForInsts[USP_MAX_TEXTURE_WRITE_INSTS];

	/* Default instruction description flags for each texwrite inst */
	IMG_UINT32              uInstDescFlags;
	
	struct _USP_TEXTURE_WRITE_*    psNext;
	struct _USP_TEXTURE_WRITE_*    psPrev;
};

/*
	Data associated with a branch within the shader
*/
struct _USP_BRANCH_
{
	/* ID of the label referenced by this branch */
	IMG_UINT32				uLabelID;

	/* The instruction block containing the instructions generated for the branch */
	PUSP_INSTBLOCK			psInstBlock;

	/* The label referenced by this branch */
	PUSP_LABEL				psLabel;

	struct _USP_BRANCH_*	psNext;
	struct _USP_BRANCH_*	psPrev;
};

/*
	Texture types supported by the USP - this is used to indicate colour space and
	identify some special case texture formats (currently just some YUV formats)
*/
typedef enum _USP_TEX_TYPE_
{
	USP_TEX_TYPE_NORMAL	= 0,
	USP_TEX_TYPE_YUV	= 1,
}USP_TEX_TYPE, *PUSP_TEX_TYPE;

/*
	Data describing how sampled texture-data is organised
*/
struct _USP_TEX_CHAN_INFO_
{
	/* Indicates the type of texture */
	USP_TEX_TYPE		eTexType;

	/* Bitmask of which tex-channels 0..(USP_MAX_SAMPLE_CHANS-1) are sampled */
	IMG_UINT32			uTexChanSampleMask;

	/* Mask indicating which tex-chunks 0..(USP_MAX_TEXTURE_CHUNKS-1) are sampled */
	IMG_UINT32			uTexChunkMask;

	/* Mask indicating which chunks are non-dependent */
	IMG_UINT32			uTexNonDepChunkMask;

	/* Texture-channel to be used for each component in the result */
	USP_CHAN_SWIZZLE	aeTexChanForComp[USP_MAX_SAMPLE_CHANS];

	/* Destination reg-offset for each texture-channel */
	IMG_UINT32			auDestRegForChan[USP_MAX_SAMPLE_CHANS];

	/* Component index in the dest register for each texture-channel */
	IMG_UINT32			auDestRegCompForChan[USP_MAX_SAMPLE_CHANS];

	/* Data format of the sampled data for each texture-channel */
	USP_CHAN_FORMAT		aeDataFmtForChan[USP_MAX_SAMPLE_CHANS];

	/* The texture chunk containing each texture-channel */
	IMG_UINT32			auChunkIdxForChan[USP_MAX_SAMPLE_CHANS];

	/* Reg offset for each chunk of texture data to be sampled */
	IMG_UINT32			auDestRegForChunk[USP_MAX_TEXTURE_CHUNKS];

	/* The number of registers required for each chunk of texture data */
	IMG_UINT32			auNumRegsForChunk[USP_MAX_TEXTURE_CHUNKS];

	/* Total number of registers needed for all chunks of texture data */
	IMG_UINT32			uRegCount;
};

/*
	Data associated with a texture-sample within the shader
*/
#define USP_SAMPLE_MAX_NORM_INSTS (20)
/*
  Normalising and centering requires at
  most:

      FSUBFLR 0,X
      FSUBFLR 0,Y
      FMOV    X, X.neg
      FMOV    Y, Y.neg
      LIMM    fOffW
      LIMM    fRcpW
      FMAD    X, fRcp, fOff
      LIMM    fOffY
      LIMM    fRcpY
      FMAD    Y, fRcp, fOff

      : 10 instructions
*/

#define USP_INTERNAL_REGS_RESTORE_INSTS_COUNT	(9)
/*	
	Sampling code needs at most:

	1xLD per chunk for state + WDF	+
	1xSMP per chunk + WDF			+	
	9xInstruction to store & restore Internal Registers
*/
#define USP_SAMPLE_MAX_SAMPLE_INSTS	(((USP_MAX_TEXTURE_CHUNKS + 1) + (USP_MAX_TEXTURE_CHUNKS + 1)) + USP_INTERNAL_REGS_RESTORE_INSTS_COUNT + USP_SAMPLE_MAX_NORM_INSTS)

/*
	Unpacking code needs at most:
	
	3xFIRH for CSC														+
	1xMOV per chunk, when dest overlaps src for non-dependent samples	+
	1xPCK/UNPCK per dest-channel										+
	MOE + SOPWM for final C10->U8
*/

#define USP_SAMPLE_MAX_UNPACK_INSTS	((3+/*LIMMx1+TESTx1+ORx1*/3) + USP_MAX_TEXTURE_CHUNKS + USP_MAX_SAMPLE_CHANS + 2)

struct _USP_SAMPLE_
{
	/* The shader that this sample is from */
	PUSP_SHADER				psShader;

	/* MOE state prior to the sample */
	USP_MOESTATE			sMOEState;

	/* ID of USP sample */
	IMG_UINT32				uSmpID;

	/* Instruction description flags based upon the supplied PC flags */
	IMG_UINT32				uInstDescFlags;

	/* Index of the texture/sampler to use */
	IMG_UINT32				uTextureIdx;
	
	/* How the sampled data should be swizzled */
	IMG_UINT16				uSwizzle;

	/* Original swizzle required for the sample */
	IMG_UINT16				uOrgSwizzle;

	/* Whether this sample depends upon data calculated by the shader */
	IMG_BOOL				bNonDependent;

	/* Whether this sample originally dependend upon data calculated by the shader */
	IMG_BOOL				bOrgNonDependent;

	/* The iterated value forming the texture coords (for non-dependent sample) */
	USP_ITERATED_DATA_TYPE	eCoord;

	/* dimensionality of the texture-coords (for non-dependent samples) */
	IMG_UINT32				uTexDim;

	/* Whether a non-dependent sample should use projected coords */
	IMG_BOOL				bProjected;

	/* Whether a non-dependent sample should use centroid adjust */
	IMG_BOOL				bCentroid;

	/* Pre-compiled reg-bank where the sampled data should be written directly without invovling unpacking if possible */
	USP_REGTYPE				eSmpNdpDirectSrcRegType;

	/* Pre-compiled reg-index where the sampled data should be written directly without invovling unpacking if possible */
	IMG_UINT32				uSmpNdpDirectSrcRegNum;	

	/* Pre-compiled sample instruction */
	HW_INST					sBaseInst;

	/* Decoded opcode of the pre-compiled sample inst */
	USP_OPCODE				eBaseInstOpcode;

	/* Index of the DRC register used by the pre-compiled sample inst */
	IMG_UINT32				uBaseInstDRCNum;

	/* The HW operands used by the pre-compiled sample inst */
	IMG_UINT32				uBaseInstOperandsUsed;

	/* Pre-compiled reg-bank where the sampled data should be written */
	USP_REGTYPE				eBaseSampleDestRegType;

	/* Pre-compiled reg-index where the sampled data should be written */
	IMG_UINT32				uBaseSampleDestRegNum;

	/* Pre-compiled reg-bank where the sampled data should be written directly without invovling unpacking if possible */
	USP_REGTYPE				eDirectDestRegType;

	/* Pre-compiled reg-index where the sampled data should be written directly without invovling unpacking if possible */
	IMG_UINT32				uDirectDestRegNum;
	
	/* Temporary reg-bank available for use during sampling */
	USP_REGTYPE				eSampleTempRegType;

	/*  Temporary reg-index available for use during sampling  */
	IMG_UINT32				uSampleTempRegNum;

#if 0
	/* 
		Originally provided pre-compiled reg-index where the sampled data 
		should be written. uBaseSampleRegNum can change in case shader 
		writing final results to temp and it is the final sample block.
	*/
	IMG_UINT32				uOrgBaseSampleRegNum;
#endif

	/* Location of the sampled data for each chunk */
	USP_REG					asChunkReg[USP_MAX_TEXTURE_CHUNKS];

	/* Chunked not sampled to temp reg */
	IMG_BOOL				abChunkNotSampledToTempReg[USP_MAX_TEXTURE_CHUNKS];

	/* Location of the sampled data for each dest channel */
	USP_REG					asChanReg[USP_MAX_SAMPLE_CHANS];

	/* Format of the source data for each dest channel */
	USP_CHAN_FORMAT			aeDataFmtForChan[USP_MAX_SAMPLE_CHANS];

	/* The instructon block containing the generated instructions */
	PUSP_INSTBLOCK			psInstBlock;

	/* The number of instructions generated for sampling code only */
	IMG_UINT32				uSampleInstCount_;

	/* The number of destinaton registers used by sampling code */
	IMG_UINT32				uBaseSampleDestRegCount_;

	/* The number of temporary registers used by sampling code */
	IMG_UINT32				uSampleTempRegCount;

	/* The number of unaligned temps */
	IMG_UINT32				uUnAlignedTempCount;

	/* HW-instructions generated to normalize texture co-ordinates */
	HW_INST                 asNormalizingInsts_[USP_SAMPLE_MAX_NORM_INSTS];

	USP_OPCODE              aseOpcodeForNormalizingInsts_[USP_SAMPLE_MAX_NORM_INSTS];

	/* Original texture precision specified in sample input instruction */
	IMG_UINT16              uTexPrecision;
	
	/* HW-instructions generated to sample the texture */
	HW_INST					asSampleInsts_[USP_SAMPLE_MAX_SAMPLE_INSTS];

	/* Opcodes for each of the sample instructions */
	USP_OPCODE				aeOpcodeForSampleInsts_[USP_SAMPLE_MAX_SAMPLE_INSTS];

	/* Instruction description flags for each sample-inst */
	IMG_UINT32				auInstDescFlagsForSampleInsts_[USP_SAMPLE_MAX_SAMPLE_INSTS];
	
	/* The number of SMP instructions generated for this sample */
	IMG_UINT32				uNumSMPInsts;

	/* The SMP instructions generated for each dest channel */
	PHW_INST				apsSMPInsts[USP_MAX_SAMPLE_CHANS];
	
	/*
		Format description of the sampled data
	*/
	USP_SAMPLER_CHAN_DESC	sSamplerDesc;

	/*
		Flag indicating unpacking is used or not
	*/
	IMG_BOOL				bSamplerUnPacked;

	/* Mask indicating which channels will be written for unpacked sampler */
	IMG_UINT32				uUnpackedMask;

	/* Internal registers live before block */
	IMG_UINT16				uIRegsLiveBefore;

	/* Internal registers live in C10 format before block */
	IMG_UINT16				uC10IRegsLiveBefore;

	/*
		Information describing the organisation of the sampled data, setup
		once the texture format is known
	*/
	USP_TEX_CHAN_INFO		sTexChanInfo;

	/* Index to a texture control words entry */
	IMG_UINT32				uTexCtrWrdIdx;

	/* Pointer to attached USP sample unpack block */
	PUSP_SAMPLEUNPACK		psSampleUnpack;

	struct _USP_SAMPLE_*	psNext;
	struct _USP_SAMPLE_*	psPrev;
};

struct _USP_SAMPLEUNPACK_
{
	/* The shader that this sample is from */
	PUSP_SHADER				psShader;

	/* MOE state prior to the sample */
	USP_MOESTATE			sMOEState;

	/* ID of the sample this sample unpack is attached to */
	IMG_UINT32				uSmpID;

	/* Instruction description flags based upon the supplied PC flags */
	IMG_UINT32				uInstDescFlags;
	
	/* Mask indicating which channels should be written */
	IMG_UINT32				uMask;

	/* Mask indicating which channels are live */
	IMG_UINT32				uLive;
	
	/* Format and locatioon where the sampled data is expected */
	USP_REG					asDest[USP_MAX_SAMPLE_CHANS];

	/* Pre-compiled reg-bank where the sampled data should be written directly without invovling unpacking if possible */
	USP_REGTYPE				eDirectSrcRegType;

	/* Pre-compiled reg-index where the sampled data should be written directly without invovling unpacking if possible */
	IMG_UINT32				uDirectSrcRegNum;

	/* Pre-compiled reg-bank where the sampled data should be read from */
	USP_REGTYPE				eBaseSampleUnpackSrcRegType;

	/* Pre-compiled reg-index where the sampled data should be read from */
	IMG_UINT32				uBaseSampleUnpackSrcRegNum;

	/* Pre-compiled reg-bank for temporary usage during unpacking */
	USP_REGTYPE				eSampleUnpackTempRegType;

	/* Pre-compiled reg-index for temporary usage during unpacking */
	IMG_UINT32				uSampleUnpackTempRegNum;

	/* Location of the sampled data for each chunk */
	USP_REG					asChunkReg[USP_MAX_TEXTURE_CHUNKS];

	/* Location of the sampled data for each dest channel */
	USP_REG					asChanReg[USP_MAX_SAMPLE_CHANS];

	/* Format of the source data for each dest channel */
	USP_CHAN_FORMAT			aeDataFmtForChan[USP_MAX_SAMPLE_CHANS];

	/* The instructon block containing the generated instructions */
	PUSP_INSTBLOCK			psInstBlock;

	/* The number or instructions generated to unpacking code only */
	IMG_UINT32				uUnpackInstCount;

	/* HW-instructions generated to FIRH/unpack the sampled data */
	HW_INST					asUnpackInsts[USP_SAMPLE_MAX_UNPACK_INSTS];

	/* Opcodes for each of the unpacking inst */
	USP_OPCODE				aeOpcodeForUnpackInsts[USP_SAMPLE_MAX_UNPACK_INSTS];

	/* Instruction description flags for each unpacking-inst */
	IMG_UINT32				auInstDescFlagsForUnpackInsts[USP_SAMPLE_MAX_UNPACK_INSTS];	
	
	/*
		The overall number of intermediate registers used by this sample unpack for 
		unpacking the data 
	*/
	IMG_UINT32				uSampleUnpackTempRegCount;

	IMG_UINT32				uBaseSampleUnpackSrcRegCount;

	/*
		Format description of the sampled data
	*/
	USP_SAMPLER_CHAN_DESC	sSamplerDesc;

	/*
		Flag indicating unpacking is used or not
	*/
	IMG_BOOL				bSamplerUnPacked;

	/* Mask indicating which channels will be written for unpacked sampler */
	IMG_UINT32				uUnpackedMask;

	/* Internal registers live before block */
	IMG_UINT16				uIRegsLiveBefore;
	
	/* Can we sample directly to destination if needed */
	IMG_BOOL				bCanSampleDirectToDest;

	/* Pointer to HW-instructions generated to sample the texture */
	PHW_INST				asSampleInsts;

	/* Pointer to Opcodes for each of the sample instructions */
	PUSP_OPCODE				aeOpcodeForSampleInsts;

	/* Pointer to Instruction description flags for each sample-inst */
	IMG_UINT32*				auInstDescFlagsForSampleInsts;
	
	/* Pointer to The number of SMP instructions generated for this sample */
	IMG_UINT32*				puNumSMPInsts;

	IMG_UINT32				uBaseSampleDestRegCount;

	IMG_UINT32*				puBaseSampleDestRegCount;

	IMG_UINT32*				puBaseSampleTempDestRegCount;

	struct _USP_SAMPLEUNPACK_*	psNext;
	struct _USP_SAMPLEUNPACK_*	psPrev;
};

/*
	Data associated with a reference to the result of a shader
*/
struct _USP_RESULTREF_
{
	/* The USP shader that this reference is part of */
	PUSP_SHADER				psShader;

	/* Indicates whether this result-reference is active */
	IMG_BOOL				bActive;

	/* The USP instruction that references the shader result */
	PUSP_INST				psInst;

	/* Opcode of the associated instruction */
	USP_OPCODE				eOpcode;

	/* instruction description flags from the associated instruction */
	IMG_UINT32				uInstDescFlags;

	/* Per-operand format-control flags for the associated instrucion */
	USP_FMTCTL				eFmtCtl;

	/* Copy of the original HW instruction */
	HW_INST					sOrgHWInst;

	/* The decoded default HW-operands that access a result-reg */
	USP_REG					auOrgHWOperands[USP_OPERANDIDX_MAX + 1];

	/* Index of the first register originally reserved for the shader result */
	IMG_UINT32				uOrgResultRegNum;

	struct _USP_RESULTREF_*	psNext;
	struct _USP_RESULTREF_*	psPrev;
};

/*
	Data to list all the Sample Blocks needing 
	particular Texture Chunks.
*/
typedef struct _SAMPLES_LIST_
{
	struct _USP_SAMPLE_*	psSample;
	struct _SAMPLES_LIST_*	psNext;
} SAMPLES_LIST, *PSAMPLES_LIST;

typedef struct _TEX_SAMPLES_LIST_
{
	IMG_UINT32				uTexIdx;
	IMG_UINT32				uCount;
	SAMPLES_LIST*           psList;
	struct _TEX_SAMPLES_LIST_*
							psNext;
}TEX_SAMPLES_LIST;

typedef struct _USP_SHDR_TEX_CTR_WORDS_
{
	/* The texture id for which this entry is created */
	IMG_UINT16 uTextureIdx;

	/* Hardware texture control words */
	IMG_UINT32 auWords[USP_TEXFORM_MAX_PLANE_COUNT];

	/* Flag indicating use of extended formats on SGX545 */
	IMG_BOOL abExtFmt[USP_TEXFORM_MAX_PLANE_COUNT];

	/* Flag indicating use of replicate on non-SGX545 */
	IMG_BOOL abReplicate[USP_TEXFORM_MAX_PLANE_COUNT];

	/* The unpacking format */
	USP_REGFMT	aeUnPackFmts[USP_TEXFORM_MAX_PLANE_COUNT];

	/* The swizzle in case of SGX543 */
	IMG_UINT32 auSwizzle[USP_TEXFORM_MAX_PLANE_COUNT];

	/* The MINPACK option for SGX543 */
	IMG_BOOL abMinPack[USP_TEXFORM_MAX_PLANE_COUNT];

	/* The index where this word will move in hardware shader */
	IMG_UINT16 auTexWrdIdx[USP_TEXFORM_MAX_PLANE_COUNT];

	/* The number of DWORDS written by sample */
	IMG_UINT16 auTagSize[USP_TEXFORM_MAX_PLANE_COUNT];

	/* Flag indicating is this texture used in the shader */
	IMG_BOOL bUsed;
}USP_SHDR_TEX_CTR_WORDS, *PUSP_SHDR_TEX_CTR_WORDS;

/*
	A (runtime-patchable) USP shader
*/
struct _USP_SHADER_
{
	/*
		Collected meta-data describing the precompiled program
	*/
	PUSP_PROGDESC		psProgDesc;

	/*
		List of iterated and sampled data, and their locations within
		the PA registers
	*/
	PUSP_INPUT_DATA		psInputData;

	/*
		The list of instruction blocks forming the shader
	*/
	PUSP_INSTBLOCK		psInstBlocks;
	PUSP_INSTBLOCK		psInstBlocksEnd;
					
	/*
		The list of all the branches within the shader
	*/
	PUSP_BRANCH			psBranches;
	PUSP_BRANCH			psBranchesEnd;

	/*
		The list of all the labels used within the shader
	*/
	PUSP_LABEL			psLabels;
	PUSP_LABEL			psLabelsEnd;

	/*
		The label marking the beginning of the main program
	*/			   
	PUSP_LABEL			psProgStartLabel;

	/*
		Label marking the end of the compiler generated code for the
		alpha-test/punch-through phase of the main program
	*/
	PUSP_LABEL			psPTPhase0EndLabel;

	/*
		Label marking the start of the phase after the alpha-test/punch-
		through phase of the main program
	*/
	PUSP_LABEL			psPTPhase1StartLabel;

	/*
		Label marking the start of the phase after the Split 
		phase of the main program
	*/
	PUSP_LABEL			psPTSplitPhase1StartLabel;

	/*
		The list of all the texture writes used within the shader
	*/
	IMG_UINT32			uTextureWriteCount;
	PUSP_TEXTURE_WRITE	psTextureWrites;
	PUSP_TEXTURE_WRITE	psTextureWritesEnd;
						
	/*
		The list of all the dependent samples used within the shader
	*/
	IMG_UINT32			uDepSampleCount;
	PUSP_SAMPLE			psDepSamples;
	PUSP_SAMPLE			psDepSamplesEnd;

	/*
		The list of all the non-dependent samples used within the shader
	*/
	IMG_UINT32			uNonDepSampleCount;
	PUSP_SAMPLE			psNonDepSamples;
	PUSP_SAMPLE			psNonDepSamplesEnd;

	/*
		The list of all the sample unpackes used within the shader
	*/
	IMG_UINT32			uSampleUnpackCount;
	PUSP_SAMPLEUNPACK	psSampleUnpacks;
	PUSP_SAMPLEUNPACK	psSampleUnpacksEnd;

	/*
		The list of all the instructions which write the final
		result of the shader
	*/
	PUSP_RESULTREF		psResultRefs;
	PUSP_RESULTREF		psResultRefsEnd;

	/*
		The register-type used for the shader's results after finalisation
	*/
	USP_REGTYPE			eFinalResultRegType;

	/*
		The first register used for the shader's results after finalisation
	*/
	IMG_UINT32			uFinalResultRegNum;

	/*
		The number of temps required by the shader after finalisation
	*/
	IMG_UINT32			uFinalTempRegCount;

	/*
		The number of PAs required by the shader after finalisation
	*/
	IMG_UINT32			uFinalPARegCount;

	/*
		The preable instruction-block added at the start of the program
	*/
	PUSP_INSTBLOCK		psPreableBlock;

	/*
		The list textures used in the shader with their respective 
		sample blocks
	*/
	TEX_SAMPLES_LIST    *psUsedTexFormats;

	/*
		The final texture control words chosen for this shader
	*/
	PUSP_SHDR_TEX_CTR_WORDS	psTexCtrWrds;

	/*	
		Total entries available for texture control words = (Total samples in the shader)
	*/
	IMG_UINT32				uTotalSmpTexCtrWrds;

};

/*
	Special chunk-index used to indicate PA registers reserved for texture-
	data by the USC. These can be used for any chunk.
*/
#define USP_PRESAMPLED_DATA_CHUNKIDX_RESERVED	((IMG_UINT32)-1)

/*
	Debug-print macro
*/
#if defined(DEBUG)
#define USP_DBGPRINT(X)	psContext->pfnPrint X
#else	/* #if defined(DEBUG) */
#define USP_DBGPRINT(X)
#endif	/* #if defined(DEBUG) */

#define USP_UNREFERENCED_PARAMETER(param)	{ if (param){} }


#endif	/* #ifndef _USPSHRD_H_ */
/*****************************************************************************
 End of file (USPSHRD.H)
*****************************************************************************/
