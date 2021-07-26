/**************************************************************************
 * Name         : codegen.h
 * Author       : James McCarthy
 * Created      : 07/11/2005
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
 * $Log: codegen.h $
 * l.
 *  --- Revision Logs Removed --- 
 * 
 *  --- Revision Logs Removed --- 
 **************************************************************************/

#ifndef __gl_codegen_h_
#define __gl_codegen_h_

#include "ffgen.h"


#if !PVRD3D_FFTNLGEN_HALBUILD
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#define USEASM_OP_COMMENT      USEASM_OP_MAXIMUM + 1
#define USEASM_OP_COMMENTBLOCK USEASM_OP_MAXIMUM + 2

#if defined(FFGEN_UNIFLEX)
#define USEASM_OP_FLOOR		   USEASM_OP_MAXIMUM + 3
#endif /* defined(FFGEN_UNIFLEX) */

#define FFTNLGEN_MAX_NESTED_DEPTH 20


#define DISABLE_DST_BASE_OFFSET(psFFGenCode)					\
	if(psFFGenCode->uEnabledSMBODestBaseOffset)					\
	{															\
		SMBO(0, 0, 0, 0, "Disable dest base offset");			\
		psFFGenCode->uEnabledSMBODestBaseOffset = 0;			\
	}

#define ENABLE_DST_BASE_OFFSET(psFFGenCode, uBaseOffset)		\
	if(uBaseOffset)												\
	{															\
		if(psFFGenCode->uEnabledSMBODestBaseOffset != uBaseOffset)	\
		{															\
			SMBO(uBaseOffset, 0, 0, 0, "Enable dest base offset")	\
			psFFGenCode->uEnabledSMBODestBaseOffset = uBaseOffset;	\
		}															\
	}																\
	else															\
	{																\
		DISABLE_DST_BASE_OFFSET(psFFGenCode);						\
	}																


/* 
	Program type to be generated, can be TNL or GEO 
*/
typedef enum FFGenProgramTypeTAG
{
	FFGENPT_TNL		= 0,
	FFGENPT_GEO		= 1,
}FFGenProgramType;

typedef struct FFGenContext_TAG
{
	IMG_HANDLE 			hClientHandle;

	/* A list of program generated previously */
	FFGenProgram		*psFFTNLProgramList;

	/* File name for dumping */
	IMG_CHAR			*pszDisassemblyFileName;

#if defined(DEBUG)
	IMG_UINT32			uProgramCount;
#endif

	/* Debug function pointers */
	FFGEN_MALLOCFN		pfnMalloc;
	FFGEN_CALLOCFN		pfnCalloc;
	FFGEN_REALLOCFN		pfnRealloc;
	FFGEN_FREEFN		pfnFree;
	FFGEN_PRINTFN		pfnPrint;

} FFGenContext;


typedef struct 	FFGenInstruction_TAG
{
	UseasmOpcode           eOpcode;

	IMG_UINT32             uFlags1;
	IMG_UINT32			   uFlags2;
	IMG_UINT32			   uFlags3;
	IMG_UINT32             uTest;
	
	IMG_UINT32             uExtraInfo;
	
	FFGenReg              *ppsRegs[USE_MAX_ARGUMENTS];
	IMG_INT32              aiRegOffsets[USE_MAX_ARGUMENTS];
	IMG_UINT32             auRegFlags[USE_MAX_ARGUMENTS];
	IMG_UINT32             uUseRegOffset;
	IMG_UINT32             uNumRegs;

	IMG_BOOL			   bIndexPatch;
	IMG_CHAR              *pszComment;

} FFGenInstruction;

typedef struct FFGenInstructionList_TAG
{
	FFGenInstruction                 sInstruction;
	FFGenReg                         asRegs[USE_MAX_ARGUMENTS];
	IMG_UINT32                       uLineNumber;
	IMG_UINT32                       uInstructionNumber;

	struct FFGenInstructionList_TAG *psNext;
	struct FFGenInstructionList_TAG *psPrev;

} FFGenInstructionList;


typedef struct UseInstructionList_TAG
{
	USE_INST *psInstruction;
	IMG_UINT32 uInstructionNumber;

	struct UseInstructionList_TAG *psNext;
	
} UseInstructionList;


/* Internal flag ORed to eCodeGenFlags while generating ff code for dynamic flow control or predication */
#define FFGENCGF_DYNAMIC_FLOW_CONTROL	0x00001000 

#if defined(OGL_LINESTIPPLE)

#define SGX_AVAILABLE_PARTITIONS			2
#define SGX_MAX_NUM_VERTICES_MTE_ACCESSED	16
#define SGX_MAX_ADVANCE						4

typedef struct FFGEOVertPartitionRec
{
	IMG_UINT32 uPartitionNum;

	IMG_UINT32 uBaseOffsetRequired;
	IMG_UINT32 uFirstRegOffset;

}FFGEOVertPartition;

typedef struct FFGEOUnitTexGenInfoRec
{
	IMG_UINT32 uOutOffset;				/* Static output offset for this texgen.*/
	IMG_UINT32 uCoordUnit;				/* tex coord unit */	
	IMG_UINT32 uCoordNum;				/* Which coord, _S_, _T_ ? */
	IMG_UINT32 uOutVertexID;			/* Which output vertex */
	IMG_UINT32 uVIndexInOBuffer;
}FFGEOUnitTexGenInfo;


typedef struct FFGEOTexGenRegsRec
{
	FFGenReg *psLineRepeatReg;		/* Constant LINEREPEAT */
	FFGenReg *psLineOffsetReg;		/* Constant LINEOFFSET */

	/* Save two temp regs for line repeats in order for multiple usage */ 
	FFGenReg *psTempFloatOneReg;
	FFGenReg *psTempWidthReg;

	FFGenReg *psCapOffset;			/* cap offset for texgen for square cap and round cap */

	FFGenReg *psTotalTexturedLength;/* Total textured length */

	FFGenReg *psTexLenPerVertex;	/* textured length per vertex */
}FFGEOTexGenRegs;

typedef struct PrimEmitStateRec
{
	IMG_UINT16 auPreIndices[SGX_MAX_NUM_VERTICES_MTE_ACCESSED];

	IMG_UINT16 uNumVerticesInOBuffer;
	IMG_UINT16 auNumVerticesPerPart[SGX_AVAILABLE_PARTITIONS];
	IMG_UINT16 uCurrentPartitionNum;
	IMG_UINT16 uNumTriangles;
	IMG_UINT16 auIndices[32];  /* Need rethink how many indices possible */
	IMG_UINT16 uWOPedMask;
}PrimEmitState;

typedef enum 
{
	INHERIT_A,
	INHERIT_B,
}ATTRIB_INHERIT;

typedef enum FFGEOState_TAG
{
	/* Shader required */
	FFGEO_REQUIRE_LINE_LENGTH	= 0x1,
	FFGEO_REQUIRE_TOTAL_LENGTH  = 0x2,
	FFGEO_REQUIRE_VERTEX_COUNT	= 0x4,
	FFGEO_PIXEL_TEXGEN_S		= 0x8,
	FFGEO_PARAMETRIC_TEXGEN_S	= 0x10,

	/* Shader state */
	FFGEO_SHADER_SYNCED			= 0x20,
	FFGEO_ACCUM_PARAMS_READY	= 0x40,
	FFGEO_MUTEX_LOCKED			= 0x80,

	FFGEO_DIFF_SEG_GEOMETRY		= 0x100,

	FFGEO_GEN_CAP_VERTICES		= 0x200,
	FFGEO_GEN_LEFT_CAP			= 0x400,

	FFGEO_GEN_ROUND_JOIN		= 0x800,
	FFGEO_GEN_LEFT_ROUND_JOIN	= 0x1000,

}FFGEOState;

struct FFGenCode_TAG; 

typedef IMG_VOID ( *PFNTexgen) (struct FFGenCode_TAG *psFFGenCode, 
								IMG_UINT32 uNumCoordsToGen, 
								FFGEOUnitTexGenInfo *psUnitTexGenInfo);


#endif /* #if defined(OGL_CARNAV_EXTS) || defined(OGL_LINESTIPPLE) */

typedef struct FFGenCode_TAG
{
	FFGenProgramType	  eProgramType;

	FFTNLGenDesc         *psFFTNLGenDesc;

	FFCodeGenMethod		  eCodeGenMethod;
	IMG_BOOL			  bSeondPass;
	FFCodeGenFlags	      eCodeGenFlags;

	FFGenInstruction      sInstruction;

	FFGenInstructionList *psStoredInstructionList;
	FFGenInstructionList *psCurrentInstructionEntry;
	FFGenInstructionList *psIndexSetupInstructionList;
	IMG_UINT32            uNumInstructionsStored;
	
	FFGenRegList         *psFreeTempList;
	IMG_UINT32            uCurrentTempSize;

	IMG_UINT32            uInputSize;
	IMG_UINT32            uOutputSize;
	IMG_UINT32            uTempSize;

	IMG_UINT32			  uSecAttribStart;
	IMG_UINT32            uSecAttribSize;
	IMG_UINT32            uMaxSecAttribSize;
	IMG_UINT32			  uHighSecAttribSize;
	IMG_INT32             iSAConstBaseAddressAdjust;

	IMG_UINT32            uMemoryConstantsSize;
	IMG_UINT32            uMaxMemoryConstantsSize;

	FFGenRegList         *psConstantsList;
	FFGenRegList         *psInputsList;
	FFGenRegList         *psOutputsList;
	FFGenRegList         *psThrowAwayList;
	FFGenRegList		 *psIndexableSecondaryList;

	IMG_UINT32            uNumOutputClipPlanes;
	IMG_UINT32            uNumConstantClipPlanes;

	IMG_UINT32            auInputTexDimensions[FFTNLGEN_MAX_NUM_TEXTURE_UNITS];
	IMG_UINT32            auOutputTexDimensions[FFTNLGEN_MAX_NUM_TEXTURE_UNITS];
	IMG_UINT32            uNumTexCoordUnits;

	/* Registers that are used frequently */
	FFGenReg              sSAConstBaseAddress;
	FFGenReg              sImmediateIntReg;
	FFGenReg              sImmediateIntReg2;
	FFGenReg              sImmediateFloatReg;
	FFGenReg              sImmediateFloatReg2;
	FFGenReg              asPredicateRegs[4];
	FFGenReg              sDRCReg;
	FFGenReg              sInternalReg;
	FFGenReg              sLabelReg;
	FFGenReg              sRangeReg;
	FFGenReg              sClipPlaneReg;
	FFGenReg              sIndexLowReg;
	FFGenReg              sIndexHighReg;
	FFGenReg              sIndexLowHighReg;
	FFGenReg			  sIntSrcSelReg;
	FFGenReg			  sIntSrcSelReg2;
	FFGenReg			  sIntSrcSelReg3;
	FFGenReg			  sSwizzleXYZW;


	/* Temps we need to store for persistant use */
	FFGenReg             *psNormal;
	FFGenReg             *psEyePosition;
	FFGenReg             *psVtxEyeVec;
	FFGenReg             *psVtxEyeVecMag;
	FFGenReg             *psRSQVtxEyeVecMag;
	FFGenReg             *psTexGenR;

	/* Outstanding data channels */
	IMG_BOOL			 abOutstandingDRC[EURASIA_USE_DRC_BANK_SIZE];

	/* Number of labels used */
	IMG_UINT32            uLabelCount;
	IMG_UINT32            uLabelListSize;
	IMG_CHAR            **ppsLabelNames;

	/* Nested info */
	IMG_UINT32            uConditionalDepth;
	IMG_UINT32            uConditionalLabelStack[FFTNLGEN_MAX_NESTED_DEPTH];

	/* Use asm structs */
	USE_INST            *psUseInsts;
	USE_INST            *psCurrentUseInst;
	IMG_UINT32           uNumUseArgs;
	IMG_UINT32           uNumInstructions; 
	IMG_UINT32           uAlignedInstructionCount;
	IMG_UINT32           uNumHWInstructions;
	
	UseInstructionList  *psIndexPatchUseInsts;

	/* hw code */
	IMG_UINT32          *puHWCode;

	/* Info used for source code */
	IMG_BOOL             bUseRegisterDescriptions;
#if defined(STANDALONE) || defined(DEBUG)
	IMG_CHAR            *pszCode;
	IMG_UINT32           uCurrentPos;
	IMG_UINT32           uIndent;
#endif

	/* Dest base offset enabled by SMBO instruction, enabled by ENABLE_DST_BASE_OFFSET */
	IMG_UINT32			uEnabledSMBODestBaseOffset;

	FFGenContext		 *psFFGenContext; 

#if defined(OGL_LINESTIPPLE)

	FFGEOGenDesc		*psFFGEOGenDesc;

	IMG_UINT32			uOutVertexSize;
	IMG_UINT8			auTexCoordOutReg[ROUND_BYTE_ARRAY_TO_DWORD(FFGEO_MAX_NUM_TEXCOORD_UNITS)];
	IMG_UINT8			auAttribInReg[ROUND_BYTE_ARRAY_TO_DWORD(FFGEO_ATTRIB_TYPES)];	
	IMG_UINT8			auAttribOutReg[ROUND_BYTE_ARRAY_TO_DWORD(FFGEO_ATTRIB_TYPES)];

	FFGenReg			sPrimAttributeReg;
	FFGenReg			sOutputReg;
	FFGenReg			sGlobalReg;

	/* Two temporary registers, suppose to be active only within functions */
	FFGenReg			*psT0; 
	FFGenReg			*psT1;

	FFGEOState			eStateMask;

	FFGenReg			*psTotalLengthRegA;
	FFGenReg			*psTotalLengthRegB;
	FFGenReg			*psCurrentLengthReg;
	FFGenReg			*psVertexCountRegA;
	FFGenReg			*psVertexCountRegB;
	
	/* Temporary registers, it is used persistently, suppose to be active until it is released */ 
	FFGenReg			*psInVertexPAOffset;
	FFGenReg			*psInPosition;
	FFGenReg			*psOutPosition;

	/* Two boolean temp registers */
	FFGenReg			*psVerticesClipped; /* size is 2 */

	IMG_UINT32			uNumInputVertices;
	IMG_UINT32			uOutVertexStride;
	IMG_UINT32			uNumVerticesPerPartition;
	IMG_UINT32			uNumPartitionsRequired;

	IMG_UINT32			uMaxVerticesInOBuffer; 
	ATTRIB_INHERIT		*peVertexAttribInherit;

	IMG_UINT16			uCapVCount;	
	IMG_UINT16			uJoinVCount;
	IMG_UINT16			uCommonVStart; 

	PrimEmitState		sPrimEmitState;

	FFGEOTexGenRegs		sTexGenRegs;
	IMG_UINT32			uMainFuncLabel;

	FFGEOUnitTexGenInfo* psUnitTexGenInfo;
	IMG_UINT32			uDelayedTexGenCount;

	PFNTexgen			pfnTexgenFunc;

	/* General purpose temporaries */
	FFGenReg			*psFourTemps;

	/* Subroutine information for calculating line intersection */
	FFGenReg			*psLineIntInLinePos;
	FFGenReg			*psLineIntOutIntersection;
	IMG_UINT32			uLineIntFuncLabel;

	FFGenReg*			psHalfLineWidth;

	/* Intersection parameters */
	FFGenReg*			psIntParams;
	FFGenReg *			psLeftIntParams;

	IMG_UINT32			uProgramStart;


	FFGEOVertPartition  *psVertexPartitionInfo;
#endif

#ifdef FFGEN_UNIFLEX
	UNIFLEX_INST* psUniFlexInstructions;
#endif

} FFGenCode;


IMG_INTERNAL FFGenCode *FFTNLGenerateCode(FFGenContext		*psFFGenContext,
										  FFTNLGenDesc      *psFFTNLGenDesc,
										  IMG_BOOL           bUseDescriptions);

IMG_INTERNAL IMG_VOID FFGENDestroyCode(FFGenCode *psFFGenCode);


#if defined(STANDALONE) || defined(DEBUG)

IMG_INTERNAL IMG_VOID IMG_CALLCONV FFGenDumpDisassembly(FFGenCode *psFFGenCode,
											  IMG_BOOL      bDumpSource,
											  IMG_CHAR      bNewFile,
											  IMG_CHAR     *pszFileName,
											  IMG_CHAR     *pszComment);

#else

#define FFGenDumpDisassembly(a, b, c, d, e)

#endif 



#endif /* __gl_codegen_h_ */
