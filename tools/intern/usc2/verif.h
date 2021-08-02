/*****************************************************************************
 * Name			: verif.h
 * Title		: Interface to verifier
 * Created  	: Nov 2006
 * 
 * Copyright		: 2002 by Imagination Technologies Ltd. All rights reserved.
 *				  No part of this software, either material or conceptual	
 *				  may be copied or distributed, transmitted, transcribed,
 *				  stored in a retrieval system or translated into any	
 *				  human or computer language in any form by any means,
 *				  electronic, mechanical, manual or other-wise, or 
 *				  disclosed to third parties without the express written
 *				  permission of Imagination Technologies Ltd, HomePark
 *				  Industrial Estate, King's Langley, Hertfordshire,
 *				  WD4 8LZ, U.K.
 *
 * Modifications :-
 * $Log: verif.h $
 * ,.
 *  --- Revision Logs Removed --- 
 *****************************************************************************/

#ifndef USCVERIFH
#define USCVERIFH

#include "usc.h"
#include "verif_textureformat.h"

#ifdef UF_TESTBENCH	

#include <sgxdefs.h>
#include <img_types.h>

#if defined(OUTPUT_USPBIN)
#include <usp.h>
#endif //defined(OUTPUT_USPBIN)


#define UFVERIF_MAXIMUM_LOOP_ITERATIONS						(2)
typedef struct tagTEMP_ARRAY_FORMAT
{
	UF_REGFORMAT eFormat;
	IMG_BOOL bFormatSet;
}TEMP_ARRAY_FORMAT_STATUS, *PTEMP_ARRAY_FORMAT_STATUS;

typedef enum PERF_INSTRUCTION_TYPE
{
	PERF_INSTRUCTION_TYPE_BRANCH = 0,
	PERF_INSTRUCTION_TYPE_MOECONTROL = 1,
	PERF_INSTRUCTION_TYPE_OTHER = 2,
	PERF_INSTRUCTION_TYPE_STALL = 3,
	PERF_INSTRUCTION_TYPE_COUNT = 4,
} PERF_INSTRUCTION_TYPE, *PPERF_INSTRUCTION_TYPE;

typedef struct _PERF_CYCLE_COUNT
{
	/* Total cycles. */
	IMG_FLOAT	fTotal;
	/* Cycles broken down by instruction type. */
	IMG_FLOAT	afPerInstType[PERF_INSTRUCTION_TYPE_COUNT];
} PERF_CYCLE_COUNT, *PPERF_CYCLE_COUNT;

typedef struct _PERF_RESULT
{
	/*
		Average (across all flow control paths) cycle count per-phase.
	*/
	PERF_CYCLE_COUNT	asAverageCycleCount[2];
	/*
		Maximum (for all possible flow control paths) cycle count.
	*/
	PERF_CYCLE_COUNT	sWorstCycleCount;
	/*
		Maximum (for all possible flow control paths) count of instructions
		executed. Can be different to the above because of MOE repeats.
	*/
	IMG_FLOAT			fWorstInstructionCount;
} PERF_RESULT, *PPERF_RESULT;

typedef struct _VERIF_TEXTURE_STATE_LOAD
{
	IMG_UINT32							uDestIdx;
	IMG_UINT32							uTextureIdx;
	IMG_UINT32							uChunkIdx;
	PVERIF_OUTPUT_TEXTURE_FORMAT_CHUNK	psChunkFormat;
	UNIFLEX_TEXTURE_UNPACK				sUnpackMode;
} VERIF_TEXTURE_STATE_LOAD, *PVERIF_TEXTURE_STATE_LOAD;

typedef struct _VERIF_TEXTURE_STATE_LOAD_LIST
{
	IMG_UINT32					uEntryCount;
	PVERIF_TEXTURE_STATE_LOAD	asEntries;
} VERIF_TEXTURE_STATE_LOAD_LIST, *PVERIF_TEXTURE_STATE_LOAD_LIST;

typedef struct _VERIF_ITERATION
{
	UNIFLEX_TEXTURE_LOAD				sLoad;
	PVERIF_OUTPUT_TEXTURE_FORMAT_CHUNK	psChunkFormat;
	UNIFLEX_TEXTURE_UNPACK				sUnpackMode;
} VERIF_ITERATION, *PVERIF_ITERATION;

typedef struct _VERIF_ITERATION_LIST
{
	IMG_UINT32			uEntryCount;
	PVERIF_ITERATION	asIterations;
} VERIF_ITERATION_LIST, *PVERIF_ITERATION_LIST;

#if defined(OUTPUT_USPBIN)
typedef struct _USP_API_FUNCS
{

	#if defined(LINUX)
	IMG_PVOID pvUSPSO;
	#endif /* defined(LINUX) */

	IMG_PVOID (IMG_CALLCONV *pfPVRUniPatchCreateContext)(USP_MEMALLOCFN	pfnAlloc,
														 USP_MEMFREEFN	pfnFree,
														 USP_DBGPRINTFN	pfnPrint);
	IMG_PVOID (IMG_CALLCONV *pfPVRUniPatchCreateShader)(IMG_PVOID		pvContext, PUSP_PC_SHADER	psPCShader);
	IMG_PVOID (IMG_CALLCONV *pfPVRUniPatchCreatePCShader)(IMG_PVOID	pvContext, IMG_PVOID	pvShader);
	IMG_VOID (IMG_CALLCONV *pfPVRUniPatchSetTextureFormat)(IMG_PVOID		pvContext,
														   IMG_UINT16		uSamplerIdx,
														   PUSP_TEX_FORMAT	psFormat,
														   IMG_BOOL			bAnisoEnabled,
														   IMG_BOOL			bGammaEnabled);
	IMG_VOID (IMG_CALLCONV *pfPVRUniPatchSetOutputLocation)(IMG_PVOID pvContext, USP_OUTPUT_REGTYPE	eRegType);
	IMG_BOOL (IMG_CALLCONV *pfPVRUniPatchSetPreambleInstCount)(IMG_PVOID	pvContext, IMG_UINT32 uInstCount);
	PUSP_HW_SHADER (IMG_CALLCONV *pfPVRUniPatchFinaliseShader)(IMG_PVOID pvContext, IMG_PVOID pvShader);
	IMG_VOID (IMG_CALLCONV *pfPVRUniPatchDestroyHWShader)(IMG_PVOID	pvContext, PUSP_HW_SHADER psHWShader);
	IMG_VOID (IMG_CALLCONV *pfPVRUniPatchDestroyShader)(IMG_PVOID pvContext, IMG_PVOID pvShader);
	IMG_VOID (IMG_CALLCONV *pfPVRUniPatchDestroyPCShader)(IMG_PVOID	pvContext, PUSP_PC_SHADER psPCShader);
	IMG_VOID (IMG_CALLCONV *pfPVRUniPatchDestroyContext)(IMG_PVOID pvContext);
	IMG_BOOL (IMG_CALLCONV *pfPVRUniPatchGetVerifierTextureFormat)(PUSP_HW_SHADER						psUSPHwShader,
																   IMG_UINT32							uTexCtrIdx,
																   IMG_PUINT32							puTextureIdx,
																   IMG_PUINT32							puChunkIdx,
																   PVERIF_OUTPUT_TEXTURE_FORMAT_CHUNK	psFormat,
																   PUNIFLEX_TEXTURE_UNPACK				psUnpackMode,
																   PVERIF_TEXEL_MEMORY_FORMAT			peMemoryFormat);
} USP_API_FUNCS, *PUSP_API_FUNCS;
#endif

typedef void (IMG_CALLCONV *GEN_FUNC)(void);
typedef void *(IMG_CALLCONV *ALLOC_FUNC)(IMG_UINT);
typedef void (IMG_CALLCONV *FREE_FUNC)(IMG_VOID *);
typedef void (IMG_CALLCONV *PRINTF_FUNC)(const IMG_CHAR *, ...);

#define PATH_SEPARATOR			'/'
#define PATH_SEPARATOR_STR		"/"
#define SHARED_OBJECT_SUFFIX            ".so"

typedef struct _UNIFLEX_VERIF_HELPER
{
	const SGX_CORE_DESC*			psTargetDesc;
	const SGX_CORE_FEATURES*		psTargetFeatures;
	const SGX_CORE_BUGS*			psTargetBugs;
	
	ALLOC_FUNC						pfnStdMallocWrapper;
	FREE_FUNC						pfnStdFreeWrapper;
	PRINTF_FUNC						pfnPrintfStdoutWrapper;
	
	IMG_UINT32						uMemoryUsedHWM;

	/*
		For each indexable temporary array the range of unified store
		temporaries it uses (if any).
	*/
	PTEMP_ARRAY_LOCATION			psIndexableTemps;

	/*
		The compiler flags used for compilation (may have been modified to
		include precision conversions).
	*/
	IMG_UINT32						uCompilerFlags;
	
	PUNIFLEX_INST 					psSWProc;
	PUNIFLEX_PROGRAM_PARAMETERS		psUniflexSAOffsets;
	/* Output Hw */
	PUNIFLEX_HW						psHw;

	VERIF_ITERATION_LIST			sIterationList;

	PVERIF_INPUT_TEXTURE_FORMAT		psInputTextureFormats;
	PVERIF_OUTPUT_TEXTURE_FORMAT	psOutputTextureFormats;
	
	PUNIFLEX_TEXTURE_PARAMETERS		psTexParams;
	
	VERIF_TEXTURE_STATE_LOAD_LIST	sTextureStateInSAs;
	VERIF_TEXTURE_STATE_LOAD_LIST	sTextureStateInMemory;

	PUNIFLEX_CONSTDEF				psConstants;
	
	IMG_UINT32						uVerifFlags;
	PERF_RESULT						sPerfResult;
	
#if defined(OUTPUT_USPBIN)
	PUSP_TEX_FORMAT					psUSPTexFormats;
	
	IMG_PVOID						pvUSPContext;
	IMG_PVOID						pvUSPShader;
	USP_API_FUNCS					sUSPAPI;
	PUSP_HW_SHADER					psUSPHwShader;
	PUSP_PC_SHADER					psPCShader;
#endif
} UNIFLEX_VERIF_HELPER, *PUNIFLEX_VERIF_HELPER;

#if defined(OUTPUT_USPBIN)
IMG_BOOL VerifierUSPInit(PUNIFLEX_VERIF_HELPER psVerifHelper, 
						 ALLOC_FUNC pfnStdMallocWrapper, 
						 FREE_FUNC pfnStdFreeWrapper, 
						 PRINTF_FUNC pfnPrintfStdoutWrapper);
						 
IMG_BOOL VerifierFinaliseUSPShader(PUNIFLEX_VERIF_HELPER		psVerifHelper, 
								   PUNIFLEX_PROGRAM_PARAMETERS	psUniflexSAOffsets,
								   PUSP_TEX_FORMAT				psUSPTexFormats);
IMG_BOOL VerifierDeinitFinaliseUSPShader(PUNIFLEX_VERIF_HELPER 	psVerifHelper);
IMG_BOOL VerifierDeinitUSPShader(PUNIFLEX_VERIF_HELPER 			psVerifHelper);
#endif

typedef enum _VERIFY_SIMULATEION_FLAGS_
{
	VERIF_SIM_FLAG_SIMULATE_INPUT			= 0x00000001,
	VERIF_SIM_FLAG_SIMULATE_OUTPUT			= 0x00000002,
	VERIF_SIM_FLAG_SIMULATE_DONT_VERIFY		= 0x00000004,
	VERIF_SIM_FLAG_SIM_OUT_TRACE_SHADER		= 0x00000008,
	VERIF_SIM_FLAG_SIM_OUT_TRACE_SHADER_OUT	= 0x00000010,
	#if defined(OUTPUT_USPBIN)
	VERIF_SIM_FLAG_SIM_USPBIN				= 0x00000020,
	#endif /* defined(OUTPUT_USPBIN) */
} VERIFY_SIMULATEION_FLAGS;

typedef enum
{
	TEXTURE_SAMPLE_RESULT_NORMAL,
	TEXTURE_SAMPLE_RESULT_RAW_SAMPLE,
	TEXTURE_SAMPLE_RESULT_COEFFICENT,
} TEXTURE_SAMPLE_RESULT_TYPE;

typedef enum
{
	TEXTURE_SAMPLE_RAW_SAMPLE_UNDEF,
	TEXTURE_SAMPLE_RAW_SAMPLE_TOPLEFT,
	TEXTURE_SAMPLE_RAW_SAMPLE_TOPRIGHT,
	TEXTURE_SAMPLE_RAW_SAMPLE_BOTTOMLEFT,
	TEXTURE_SAMPLE_RAW_SAMPLE_BOTTOMRIGHT,
	TEXTURE_SAMPLE_RAW_SAMPLE_TOPLEFT_SLICE0,
	TEXTURE_SAMPLE_RAW_SAMPLE_TOPRIGHT_SLICE0,
	TEXTURE_SAMPLE_RAW_SAMPLE_BOTTOMLEFT_SLICE0,
	TEXTURE_SAMPLE_RAW_SAMPLE_BOTTOMRIGHT_SLICE0,
	TEXTURE_SAMPLE_RAW_SAMPLE_TOPLEFT_SLICE1,
	TEXTURE_SAMPLE_RAW_SAMPLE_TOPRIGHT_SLICE1,
	TEXTURE_SAMPLE_RAW_SAMPLE_BOTTOMLEFT_SLICE1,
	TEXTURE_SAMPLE_RAW_SAMPLE_BOTTOMRIGHT_SLICE1,
	TEXTURE_SAMPLE_RAW_SAMPLE_TOPLEFT_UPPERMAP,
	TEXTURE_SAMPLE_RAW_SAMPLE_TOPRIGHT_UPPERMAP,
	TEXTURE_SAMPLE_RAW_SAMPLE_BOTTOMLEFT_UPPERMAP,
	TEXTURE_SAMPLE_RAW_SAMPLE_BOTTOMRIGHT_UPPERMAP,
	TEXTURE_SAMPLE_RAW_SAMPLE_TOPLEFT_SLICE0_UPPERMAP,
	TEXTURE_SAMPLE_RAW_SAMPLE_TOPRIGHT_SLICE0_UPPERMAP,
	TEXTURE_SAMPLE_RAW_SAMPLE_BOTTOMLEFT_SLICE0_UPPERMAP,
	TEXTURE_SAMPLE_RAW_SAMPLE_BOTTOMRIGHT_SLICE0_UPPERMAP,
	TEXTURE_SAMPLE_RAW_SAMPLE_TOPLEFT_SLICE1_UPPERMAP,
	TEXTURE_SAMPLE_RAW_SAMPLE_TOPRIGHT_SLICE1_UPPERMAP,
	TEXTURE_SAMPLE_RAW_SAMPLE_BOTTOMLEFT_SLICE1_UPPERMAP,
	TEXTURE_SAMPLE_RAW_SAMPLE_BOTTOMRIGHT_SLICE1_UPPERMAP,
	TEXTURE_SAMPLE_RAW_SAMPLE_TOPLEFT_LOWERMAP,
	TEXTURE_SAMPLE_RAW_SAMPLE_TOPRIGHT_LOWERMAP,
	TEXTURE_SAMPLE_RAW_SAMPLE_BOTTOMLEFT_LOWERMAP,
	TEXTURE_SAMPLE_RAW_SAMPLE_BOTTOMRIGHT_LOWERMAP,
	TEXTURE_SAMPLE_RAW_SAMPLE_TOPLEFT_SLICE0_LOWERMAP,
	TEXTURE_SAMPLE_RAW_SAMPLE_TOPRIGHT_SLICE0_LOWERMAP,
	TEXTURE_SAMPLE_RAW_SAMPLE_BOTTOMLEFT_SLICE0_LOWERMAP,
	TEXTURE_SAMPLE_RAW_SAMPLE_BOTTOMRIGHT_SLICE0_LOWERMAP,
	TEXTURE_SAMPLE_RAW_SAMPLE_TOPLEFT_SLICE1_LOWERMAP,
	TEXTURE_SAMPLE_RAW_SAMPLE_TOPRIGHT_SLICE1_LOWERMAP,
	TEXTURE_SAMPLE_RAW_SAMPLE_BOTTOMLEFT_SLICE1_LOWERMAP,
	TEXTURE_SAMPLE_RAW_SAMPLE_BOTTOMRIGHT_SLICE1_LOWERMAP,
	TEXTURE_SAMPLE_RAW_SAMPLE_COUNT,
	TEXTURE_SAMPLE_RAW_SAMPLE_INVALID,
} TEXTURE_SAMPLE_RAW_SAMPLE;

typedef enum
{
	TEXTURE_SAMPLE_FILTER_COEFFICENT_UFRACTION,
	TEXTURE_SAMPLE_FILTER_COEFFICENT_VFRACTION,
	TEXTURE_SAMPLE_FILTER_COEFFICENT_SFRACTION,
	TEXTURE_SAMPLE_FILTER_COEFFICENT_LOD,
	TEXTURE_SAMPLE_FILTER_COEFFICENT_TRILINEARFRACTION,
	TEXTURE_SAMPLE_FILTER_COEFFICENT_INVALID,
} TEXTURE_SAMPLE_FILTER_COEFFICENT;

typedef struct
{
	TEXTURE_SAMPLE_RESULT_TYPE	eType;
	union
	{
		struct
		{
			IMG_UINT32							uChannel;
			IMG_BOOL							bSaturated;
		} sSample;
		struct
		{
			TEXTURE_SAMPLE_RAW_SAMPLE			eType;
			IMG_UINT32							uChannel;
		} sRawSample;
		TEXTURE_SAMPLE_FILTER_COEFFICENT	eCoefficentType;
	} u;
} TEXTURE_SAMPLE_RESULT, *PTEXTURE_SAMPLE_RESULT;

/* CHANNEL: Register channels */
typedef struct tagCHANNEL
{
	IMG_UINT32	uConstsBuffNum;	// Constants buffer to which channel belongs.
	IMG_UINT32	uReg;		// Register (or texcoord layer) number
	IMG_BYTE	byChan;     // Channel number
} CHANNEL, *PCHANNEL;

/* TEXTURE_STATE: Texture states */
typedef struct _TEXTURE_STATE_
{
	IMG_UINT32							uTexture;
	IMG_UINT32							uChunk;
	IMG_UINT32							uStateNum;
	PVERIF_OUTPUT_TEXTURE_FORMAT_CHUNK	psChunkFormat;
	PUNIFLEX_TEXTURE_UNPACK				psUnpackMode;
} TEXTURE_STATE, *PTEXTURE_STATE;


IMG_VOID	VerifierDebugPrintFileInfo(const IMG_CHAR* pszFile, IMG_INT uLine);
IMG_VOID	VerifierDebugPrintf(IMG_INT iType, const IMG_CHAR *pszFormat, ...) IMG_FORMAT_PRINTF(2, 3);

/* Exports for linker */
IMG_BOOL	VerifierPreCompileInit(PUNIFLEX_VERIF_HELPER					psVerifHelper);
IMG_BOOL	VerifierPostCompileUpdateState(PUNIFLEX_VERIF_HELPER			psVerifHelper,
										   PUNIFLEX_INST 					psSWProc,
										   PUNIFLEX_PROGRAM_PARAMETERS		psUniflexSAOffsets,
										   PUNIFLEX_TEXTURE_PARAMETERS		psTexParams,
										   PUNIFLEX_CONSTDEF				psConstants,
										   IMG_UINT32						uVerifFlags);
											
IMG_UINT32	VerifierVerify(PUNIFLEX_VERIF_HELPER			psVerifHelper);
IMG_VOID	VerifierPostCompileDeinit(PUNIFLEX_VERIF_HELPER			psVerifHelper);
#endif /* UF_TESTBENCH */
#endif /* USCVERIFH */
