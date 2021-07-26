/******************************************************************************
 * Name         : useasm.h
 * Title        : Pixel Shadel Compiler
 * Author       : John Russell
 * Created      : Jan 2002
 *
 * Copyright    : 2002-2006 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited,
 *              : Home Park Estate, Kings Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Modifications:-
 * $Log: useasm.h $
 *****************************************************************************/

#ifndef __USEASM_USEASM_H
#define __USEASM_USEASM_H

#include "scopeman.h"

#include "coreutils.h"

#include "specialregs.h"

typedef struct
{
	IMG_UINT32		uLabel;
	IMG_UINT32		uOp;
	IMG_PUINT32		puOffset;
	PUSE_INST		psInst;
	IMG_BOOL		bSyncEnd;
	IMG_UINT32		uLabelOffset;
} LABEL_REFERENCE;

typedef struct
{
	IMG_UINT32		uLabelReferenceCount;
	LABEL_REFERENCE	psLabelReferences[1];
} LABEL_CONTEXT;

typedef enum
{
	LABEL_REF_BA = 0,
	LABEL_REF_BR = 1,
	LABEL_REF_LIMM = 2,
	LABEL_REF_UBA = 3,    /* BA to unresolved reference */
	LABEL_REF_UBR = 4,    /* BR to unresolved reference */
	LABEL_REF_ULIMM = 5,  /* LIMM unresolved reference */

	LABEL_REF_MAXIMUM 
} LABEL_REF_OP;

typedef struct
{
	IMG_UINT32		uCount;
	USE_REGISTER	asArg[USE_MAX_ARGUMENTS];
} SOURCE_ARGUMENTS;

typedef struct
{
	IMG_UINT32		uOpcode;
	IMG_UINT32		uSourceLine;
	IMG_PCHAR		pszSourceFile;
	IMG_UINT32		uOp2;
	IMG_PCHAR		pszCharString;
} OPCODE_AND_LINE;

typedef struct
{
	IMG_UINT32		uOpFlags1;
	IMG_UINT32		uOpFlags2;
	IMG_UINT32		uCount;
	USE_REGISTER	asArg[USE_MAX_ARGUMENTS];
} LDST_ARGUMENT;

typedef struct
{
	IMG_UINT32		uFlags1;
	IMG_UINT32		uFlags2;
	IMG_UINT32		uFlags3;
	IMG_UINT32		uTest;
	IMG_BOOL		bRepeatPresent;
} INSTRUCTION_MODIFIER;

IMG_VOID AssemblerError(IMG_PVOID pvContext, PUSE_INST psInst, IMG_PCHAR pszFmt, ...) IMG_FORMAT_PRINTF(3, 4);
IMG_UINT32 FloatToCstIndex(IMG_FLOAT f);

/* Parser declarations */
int yyparse(void);
extern int yydebug;

typedef struct _USEASM_PARSER_REG_LIST_
{
	USEASM_REGTYPE uType;
	IMG_UINT32 uNumber;
	struct _USEASM_PARSER_REG_LIST_*psNext;
} USEASM_PARSER_REG_LIST;


/* USEASM_PARSER_OPT_STATE: Data used when parsing optimiser specific data */
typedef struct _USEASM_PARSER_OPT_STATE_
{
	USEASM_PARSER_REG_LIST *psOutputRegs;

	IMG_UINT32 uMaxTemp;
	IMG_UINT32 uMaxPrimAttr;
	IMG_UINT32 uMaxOutput;
} USEASM_PARSER_OPT_STATE;
extern USEASM_PARSER_OPT_STATE g_sUseasmParserOptState;

extern PUSE_INST		g_psInstListHead;
extern IMG_UINT32		g_uInstCount;
extern IMG_UINT32		g_uSourceLine;
extern IMG_PCHAR		g_pszInFileName;

extern IMG_BOOL			g_bFixInvalidPairs;

extern IMG_BOOL			g_bCPreprocessor;

extern IMG_UINT32 g_uParserError;

IMG_UINT32 FindOrCreateLabel(const IMG_CHAR* pszNameIn,
							 IMG_BOOL bCreateOnly,
							 const IMG_CHAR* pszSourceFile,
							 IMG_UINT32 uSourceLine);

void ParseError(IMG_PCHAR pszFmt, ...) IMG_FORMAT_PRINTF(1, 2);

IMG_VOID ParseErrorAt(const IMG_CHAR* pszFileName, IMG_UINT32 uSourceLine, 
					  IMG_PCHAR pszFmt, ...) IMG_FORMAT_PRINTF(3, 4);
/* End of parser declarations */

/* Count of the number of times we have entered a nosched region. */
extern IMG_UINT32 g_uSchedOffCount;
/* First instruction in the nosched region. */
extern PUSE_INST g_psFirstNoSchedInst;
/* Last instruction in the nosched region. */
extern PUSE_INST g_psLastNoSchedInst;
/* Offset of the first instruction in the nosched region. */
extern IMG_UINT32 g_uFirstNoSchedInstOffset;
/* Offset of the last instruction in the nosched region. */
extern IMG_UINT32 g_uLastNoSchedInstOffset;
/* Count of the number of times we have entered a skipinv region. */
extern IMG_UINT32 g_uSkipInvOnCount;
/* Is this the start of nosched region. */
extern IMG_BOOL g_bStartedNoSchedRegion;

extern IMG_UINT32 g_uRepeatStackTop;
#define REPEAT_STACK_LENGTH			(256)
extern IMG_UINT32 g_puRepeatStack[];
extern IMG_UINT32 g_uInstOffset;
extern SGX_CORE_INFO g_sTargetCoreInfo;
extern PCSGX_CORE_DESC	g_psTargetCoreDesc;
extern IMG_BOOL g_bUsedOldEfoOptions;

extern const IMG_UINT32 g_puOpcodeAcceptsNoSched[];
extern const IMG_UINT32 g_puOpcodeAcceptsNoSchedEnhanced[];
extern const IMG_UINT32 g_puOpcodeAcceptsSkipInv[];

IMG_VOID AssemblerWarning(PUSE_INST psInst, IMG_PCHAR pszFmt, ...) IMG_FORMAT_PRINTF(2, 3);

IMG_VOID CheckUndefinedLabels(PUSEASM_CONTEXT psContext);

IMG_VOID SetNoSchedFlag(PUSE_INST psFirstInst, IMG_UINT32 uStartOffset, PUSE_INST psLastInst, IMG_UINT32 uLastOffset);

IMG_BOOL IsRevisionValid(PSGX_CORE_INFO psTarget);

IMG_BOOL FixBRN21697(PCSGX_CORE_DESC psTarget);

IMG_BOOL FixBRN21713(PCSGX_CORE_DESC psTarget);

IMG_BOOL FixBRN23062(PCSGX_CORE_DESC psTarget);

IMG_BOOL FixBRN23164(PCSGX_CORE_DESC psTarget);

IMG_BOOL FixBRN23461(PCSGX_CORE_DESC psTarget);

IMG_BOOL FixBRN23960(PCSGX_CORE_DESC psTarget);

IMG_BOOL FixBRN24895(PCSGX_CORE_DESC psTarget);

IMG_BOOL FixBRN25060(PCSGX_CORE_DESC psTarget);

IMG_BOOL FixBRN25355(PCSGX_CORE_DESC psTarget);

IMG_BOOL FixBRN25580(PCSGX_CORE_DESC psTarget);

IMG_BOOL FixBRN25582(PCSGX_CORE_DESC psTarget);

IMG_BOOL FixBRN26570(PCSGX_CORE_DESC psTarget);

IMG_BOOL FixBRN27005(PCSGX_CORE_DESC psTarget);

IMG_BOOL FixBRN26681(PCSGX_CORE_DESC psTarget);

IMG_BOOL FixBRN27984(PCSGX_CORE_DESC psTarget);

IMG_BOOL FixBRN29643(PCSGX_CORE_DESC psTarget);

IMG_BOOL FixBRN30871(PCSGX_CORE_DESC psTarget);

IMG_BOOL FixBRN30898(PCSGX_CORE_DESC psTarget);

IMG_BOOL FixBRN33442(PCSGX_CORE_DESC psTarget);

IMG_UINT32 IsEnhancedNoSched(PCSGX_CORE_DESC psTarget);

IMG_BOOL IsHighPrecisionFIR(PCSGX_CORE_DESC psTarget);

IMG_BOOL IsLoadMOEStateFromRegisters(PCSGX_CORE_DESC psTarget);

IMG_BOOL IsPerInstMoeIncrements(PCSGX_CORE_DESC psTarget);

IMG_BOOL IsFClampInstruction(PCSGX_CORE_DESC psTarget);

IMG_BOOL IsSqrtSinCosInstruction(PCSGX_CORE_DESC psTarget);

IMG_BOOL IsIMA32Instruction(PCSGX_CORE_DESC psTarget);

IMG_BOOL IsIDIVInstruction(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsExtendedLoad(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsLDSTExtendedCacheModes(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsSTNoReadBeforeWrite(PCSGX_CORE_DESC psTarget);

IMG_BOOL HasNewEfoOptions(PCSGX_CORE_DESC psTarget);

IMG_BOOL HasPackMultipleRoundModes(PCSGX_CORE_DESC psTarget);

IMG_BOOL HasDualIssue(PCSGX_CORE_DESC psTarget);

IMG_BOOL HasUnlimitedPhases(PCSGX_CORE_DESC psTarget);

IMG_BOOL HasNoInstructionPairing(PCSGX_CORE_DESC psTarget);

IMG_UINT32 NumberOfMutexesSupported(PCSGX_CORE_DESC psTarget);

IMG_UINT32 NumberOfMonitorsSupported(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsAlphaToCoverage(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsFmad16Swizzles(PCSGX_CORE_DESC psTarget);

IMG_UINT32 NumberOfProgramCounterBits(PCSGX_CORE_DESC psTarget);

IMG_UINT32 NumberOfInternalRegisters(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsFnormalise(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsPCF(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsRawSample(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsNopTrigger(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsBranchException(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsExtSyncEnd(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsMOEStore(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsCFI(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsVCB(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsVEC34(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsBranchPwait(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsRawSampleBoth(PCSGX_CORE_DESC psTarget);

IMG_BOOL ReducedSMPRepeatCount(PCSGX_CORE_DESC psTarget);

IMG_BOOL IMA32Is32x16Plus32(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsQwordLDST(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsIDXSC(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsTESTSABLND(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsLDRSTRRepeat(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsSTRPredicate(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsLDRSTRExtendedImmediate(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsIndexTwoBanks(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsSPRVV(PCSGX_CORE_DESC psTarget);

IMG_BOOL IsInvalidSpecialRegisterForNonBitwiseInstructions(PCSGX_CORE_DESC psTarget, IMG_UINT32 uRegNum);

IMG_BOOL SupportsSMPResultFormatConvert(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsTESTSUB32(PCSGX_CORE_DESC psTarget);

IMG_BOOL NoRepeatMaskOnBitwiseInstructions(PCSGX_CORE_DESC psTarget);

IMG_BOOL NoRepeatMaskOnSMPInstructions(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportSkipInvalidOnIDFInstruction(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsAllAnyInstancesFlagOnBranchInstruction(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsIntegerConditionalInstructions(PCSGX_CORE_DESC psTarget);

IMG_BOOL SupportsLDSTAtomicOps(PCSGX_CORE_DESC psTarget);

IMG_VOID SetLabelExportImportState(IMG_UINT32	uLabelId, 
								   const IMG_CHAR*	pszSourceFile, 
								   IMG_UINT32	uSourceLine, 
								   IMG_BOOL		bImported,
								   IMG_BOOL		bExported);

extern IMG_UINT32 g_uModuleAlignment;

#ifndef TRUE
#define TRUE IMG_TRUE
#endif
#ifndef FALSE
#define FALSE IMG_FALSE
#endif

/* define  USE_UNDEF as -1 */
#define  USE_UNDEF	(~0U)

/*
  Instruction description table
 */

/* Instruction Properties */
typedef enum _USE_DESCPROP_
{
	USE_DESCPROP_NOSCHED = 0,         /* Accepts no-schedule */
	USE_DESCPROP_NOSCHEDPLUS = 1,     /* Accepts enhanced no-schedule */
	USE_DESCPROP_SKIPINV = 2,         /* Accepts skip-invalid */
	USE_DESCPROP_REPEAT = 3,          /* Instruction can repeat */
	USC_DESCPROP_TEXTURESAMPLE = 4,	  /* Instruction is a texture sample */
	USE_DESCPROP_DISASM_ALWAYS_SHOW_SRCCOMP = 5,
									  /* In the disassembler always show the component field on this instruction's
									     source arguments. */
    USE_DESCPROP_DISASM_SIMPLE_TESTS = 6,
									  /* In the disassembler show simplier forms of the test type for this instruction. */
	USE_DESCPROP_DISASM_ALWAYS_SHOW_MASK = 7, /* In the disassembler always show the repeat mask/vector write mask. */
	USE_DESCPROP_DISASM_THREE_COMPONENT_SWIZZLE = 8, /* In the disassembler show swizzles with three components only. */
	USE_DESCPROP_DISASM_SHOW_SRC2_IMMEDIATES_IN_HEX = 9, /* In the disassembler show source 2 immediates in hex format. */
	USE_DESCPROP_DISASM_MEMORY_ST = 10, /* Memory store instruction */
	USE_DESCPROP_DISASM_ALWAYS_SHOW_SRCCOMPFORF16 = 11,		/* In the disassembler always print source components when
															   sources to this instruction are in F16 format. */
	USE_DESCPROP_DISASM_ALWAYS_SHOW_SRCCOMPFORC10 = 12,		/* In the disassembler always print source components when
															   sources to this instruction are in C10 format. */	
	USE_DESCPROP_DISASM_ALWAYS_SHOW_MASK_FOR_VECTOR = 13,	/* On vector cores always print the destination mask
															   for this instruction. */

	USE_DESCPROP_MAXIMUM,
	USE_DESCPROP_UNDEF,
} USE_DESCPROP;

/* Description flags */

/* Supports NOSCHED flag */
#define USE_DESCFLAG_NOSCHED (1 << USE_DESCPROP_NOSCHED)

/* Supports Enhanced NOSCHED flag */
#define USE_DESCFLAG_NOSCHEDPLUS (1 << USE_DESCPROP_NOSCHEDPLUS)

/* Supports SKIPINV flag */
#define USE_DESCFLAG_SKIPINV (1 << USE_DESCPROP_SKIPINV)

/* Instruction can repeat upto EURASIA_USE_MAXIMUM_REPEAT */
#define USE_DESCFLAG_REPEAT (1 << USE_DESCPROP_REPEAT)

/* Instruction is a texture sample */
#define USC_DESCFLAG_TEXTURESAMPLE (1 << USC_DESCPROP_TEXTURESAMPLE)

 /* In the disassembler always show the component field on this instruction's source arguments. */
#define USE_DESCFLAG_DISASM_ALWAYS_SHOW_SRCCOMP \
							(1 << USE_DESCPROP_DISASM_ALWAYS_SHOW_SRCCOMP)

/* In the disassembler show simplier forms of the test type for this instruction. */
#define USE_DESCFLAG_DISASM_SIMPLE_TESTS \
							(1 << USE_DESCPROP_DISASM_SIMPLE_TESTS)

#define USE_DESCFLAG_DISASM_ALWAYS_SHOW_MASK \
							(1 << USE_DESCPROP_DISASM_ALWAYS_SHOW_MASK)

#define USE_DESCFLAG_DISASM_THREE_COMPONENT_SWIZZLE \
							(1 << USE_DESCPROP_DISASM_THREE_COMPONENT_SWIZZLE)

/* In the disassembler show source 2 immediates in hex format. */
#define USE_DESCFLAG_DISASM_SHOW_SRC2_IMMEDIATES_IN_HEX (1 << USE_DESCPROP_DISASM_SHOW_SRC2_IMMEDIATES_IN_HEX)

/* Memory store instruction */
#define USE_DESCFLAG_DISASM_MEMORY_ST	(1 << USE_DESCPROP_DISASM_MEMORY_ST)

#define USE_DESCFLAG_DISASM_ALWAYS_SHOW_SRCCOMPFORF16 \
							(1 << USE_DESCPROP_DISASM_ALWAYS_SHOW_SRCCOMPFORF16)

#define USE_DESCFLAG_DISASM_ALWAYS_SHOW_SRCCOMPFORC10 \
							(1 << USE_DESCPROP_DISASM_ALWAYS_SHOW_SRCCOMPFORC10)

#define USE_DESCFLAG_DISASM_ALWAYS_SHOW_MASK_FOR_VECTOR \
							(1 << USE_DESCPROP_DISASM_ALWAYS_SHOW_MASK_FOR_VECTOR)

/* Instruction description struture */
typedef struct _USE_INSTDESC_
{
	#if defined(USER)
	char* pszName;           /* Name */
	#endif /* defined(USER) */
	IMG_UINT32 uDescFlags;   /* Description flags */
	IMG_UINT32 uArgCount;    /* Operand count */
} USE_INSTDESC, *PUSE_INSTDESC;

/* Instruction Description table */
extern const USE_INSTDESC g_asUseInstDesc[];

/* Replacement functions */
IMG_UINT32 OpcodeArgumentCount(IMG_UINT32 uOpcode);
#if defined(USER)
IMG_PCHAR OpcodeName(IMG_UINT32 uOpcode);
#endif /* defined(USER) */
IMG_UINT32 OpcodeDescFlags(IMG_UINT32 uOpcode);
IMG_BOOL OpcodeAcceptsSkipInv(IMG_UINT32 uOpcode);
IMG_BOOL OpcodeAcceptsNoSched(IMG_UINT32 uOpcode);
IMG_BOOL OpcodeAcceptsNoSchedEnhanced(IMG_UINT32 uOpcode);

#endif /* __USEASM_USEASM_H */

/******************************************************************************
 End of file (useasm.h)
******************************************************************************/

