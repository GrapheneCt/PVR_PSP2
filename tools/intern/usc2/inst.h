/*****************************************************************************
 Name           : INST.H
 
 Title          : Interface between psc.c and pscopt.c
 
 C Author       : John Russell
 
 Created        : 02/01/2002
 
 Copyright      : 2002-2007 by Imagination Technologies Ltd.
                  All rights reserved. No part of this software, either
                  material or conceptual may be copied or distributed,
                  transmitted, transcribed, stored in a retrieval system or
                  translated into any human or computer language in any form
                  by any means, electronic, mechanical, manual or otherwise,
                  or disclosed to third parties without the express written
                  permission of Imagination Technologies Ltd,
                  Home Park Estate, Kings Langley, Hertfordshire,
                  WD4 8LZ, U.K.

 Program Type   : 32-bit DLL

 Version        : $Revision: 1.231.1.1 $

 Modifications  :

 $Log: inst.h $
 p
 USC2: Added few utility functions for quick inst constraint check.
  --- Revision Logs Removed --- 
  
 Increase the cases where a vec4 instruction can be split into two vec2
 parts.
  --- Revision Logs Removed --- 
*****************************************************************************/

#ifndef __USC_INST_H
#define __USC_INST_H

/* Forward defintion of structures. */
struct tagINTERMEDIATE_STATE;
struct _CODEBLOCK;
struct _USEDEF;
struct _VREGISTER;

#include "data.h"
#include "usedef.h"
#include <limits.h>

/*
	USC_MAX_MOE_OPERANDS: Number of operands the MOE register records
	infomation about (destination and source).
*/
#define USC_MAX_MOE_OPERANDS (4)

/* USC_MAX_SWIZZLE_SLOTS: How many instructions can be grouped in a MOE swizzle. */
#define USC_MAX_SWIZZLE_SLOTS (4)

/* USC_MIN_SWIZZLE_INC: Minimum swizzle increment for a MOE. */
#define USC_MIN_SWIZZLE_INC (0)

/* USC_MAX_SWIZZLE_INC: Maximum swizzle increment for a MOE increment. */
#define USC_MAX_SWIZZLE_INC (3)

/* USC_MAX_INCREMENT_SLOTS: How many instructions can be grouped in a MOE increment. */
#define USC_MAX_INCREMENT_SLOTS EURASIA_USE_MAXIMUM_REPEAT

/* USC_MAX_INCREMENT_INC: Maximum increment in MOE increment mode. */
#define USC_MAX_INCREMENT_INC (31)

/* USC_MIN_INCREMENT_INC: Minimum increment in MOE increment mode. */
#define USC_MIN_INCREMENT_INC (-32)

/*
	Possible alignment restrictions on the hardware register number
	assigned to a variable.
*/
typedef enum
{
	HWREG_ALIGNMENT_NONE = 0,
	HWREG_ALIGNMENT_EVEN = 1,
	HWREG_ALIGNMENT_ODD = 2,
	HWREG_ALIGNMENT_RESERVED = 3,
	HWREG_ALIGNMENT_COUNT = 3,
	HWREG_ALIGNMENT_UNDEF = 0x7FFF,
} HWREG_ALIGNMENT;

/*
	Type of the instruction specific data associated with an instruction.
*/
typedef enum
{
	INST_TYPE_NONE,
	INST_TYPE_FLOAT,
	INST_TYPE_EFO,
	INST_TYPE_SOPWM,
	INST_TYPE_SOP2,
	INST_TYPE_SOP3,
	INST_TYPE_LRP1,
	INST_TYPE_FPMA,
	INST_TYPE_IMA32,
	INST_TYPE_SMP,
#if defined(OUTPUT_USPBIN)
	INST_TYPE_SMPUNPACK,
#endif /* defined(OUTPUT_USPBIN) */
	INST_TYPE_DOT34,
	INST_TYPE_DPC,
	INST_TYPE_LDST,
	#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
	INST_TYPE_NRM,
	#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */
	INST_TYPE_LOADCONST,
	INST_TYPE_LOADMEMCONST,
	INST_TYPE_IMA16,
	INST_TYPE_IMAE,
	INST_TYPE_CALL,
#if defined(OUTPUT_USPBIN)
	INST_TYPE_TEXWRITE,
#endif /* defined(SUPPORT_USPBIN) */
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	INST_TYPE_VEC,
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	#if defined(SUPPORT_SGX545)
	INST_TYPE_DUAL,
	#endif /* defined(SUPPORT_SGX545) */
	INST_TYPE_EMIT,
	INST_TYPE_TEST,
	INST_TYPE_MOVC,
	INST_TYPE_FARITH16,
	INST_TYPE_MEMSPILL,
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	INST_TYPE_IDXSC,
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	INST_TYPE_CVTFLT2INT,
	INST_TYPE_PCK,
	INST_TYPE_LDSTARR,
	INST_TYPE_FDOTPRODUCT,
	INST_TYPE_DELTA,
	INST_TYPE_MOVP,
	INST_TYPE_FEEDBACKDRIVEREPILOG,
	INST_TYPE_BITWISE,
	INST_TYPE_UNDEF,
	INST_TYPE_COUNT,
} INST_TYPE;

/*
	Maximum number of defintions written by an intermediate instruction which isn't a call.
*/
#define USC_MAX_NONCALL_DEST_COUNT					(64)

/*
	Maximum number of arguments to an intermediate instruction (which isn't a CALL).
*/
#define USC_MAXIMUM_NONCALL_ARG_COUNT				(32)

/*
	Maximum number of different subsets of the arguments to an instruction which require 
	consecutive hardware register numbers.
*/
#define USC_MAXIMUM_REGISTER_GROUP_COUNT	(5)

/*
	Maximum length of a single subset of the arguments to an instruction which require
	consecutive hardware register numbers.
*/
#define USC_MAXIMUM_CONSECUTIVE_REGISTER_SET_LENGTH	\
											max(USC_MAXIMUM_NONCALL_ARG_COUNT, USC_MAX_NONCALL_DEST_COUNT)

/*
	Possible values for the range of predicates supported by an instruction.
*/
typedef enum _INST_PRED
{
	INST_PRED_UNDEF,
	INST_PRED_NONE,
	INST_PRED_EXTENDED,
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	INST_PRED_SHORT,
	INST_PRED_VECTOREXTENDED,
	INST_PRED_VECTORSHORT,
	#else
	INST_PRED_SHORT,
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	INST_PRED_USP	/* predication only supported if USP handles it */
} INST_PRED, *PINST_PRED;

/*
	If F16 format registers are used then the register number
	range is reduced.
*/
#define DESC_FLAGS_F16FMTCTL				(0x00000001)

/*
	Is there a variant of this instruction with an attached
	test.
*/
#define DESC_FLAGS_HASTESTVARIANT			(0x00000002)

/*
	Does this instruction have negate and absolute source 
	modifiers.
*/
#define DESC_FLAGS_HASSOURCEMODIFIERS		(0x00000004)

/*
	If C10 format registers are used then the register number
	range is reduced.
*/
#define DESC_FLAGS_C10FMTCTL				(0x00000008)

/*
	This instruction does a store to memory.
*/
#define DESC_FLAGS_MEMSTORE					(0x00000010)

/*
	This instruction does a load from memory.
*/
#define DESC_FLAGS_MEMLOAD					(0x00000020)

/*
	This instruction is a texture sample.
*/
#define DESC_FLAGS_TEXTURESAMPLE			(0x00000040)

/*
	This instructions requires gradients
*/
#define DESC_FLAGS_REQUIRESGRADIENTS		(0x00000080)

/*
	This instructions can support per-instruction MOE increments 
	(on processor which support it)
*/
#define DESC_FLAGS_PERINSTMOE				(0x00000100)

/*
	This instruction can have source arguments in either F16
	or F32 format.
*/
#define DESC_FLAGS_F16F32SELECT				(0x00000200)

/*
	Does this instruction support skipping invalid instances.
*/
#define DESC_FLAGS_SUPPORTSKIPINV			(0x00000400)

/*
	This instruction is a dependent texture sample.
*/
#define DESC_FLAGS_DEPENDENTTEXTURESAMPLE	(0x00000800)

/*
	Does this instruction support the syncstart flag.
*/
#define DESC_FLAGS_SUPPORTSYNCSTART			(0x00001000)

/*
	Does source 0 to this instruction occupy hardware source 0.
*/
#define DESC_FLAGS_USESHWSRC0				(0x00002000)

/*
	Does this instruction support the disable scheduling flag.
*/
#define DESC_FLAGS_SUPPORTSNOSCHED			(0x00004000)

/*
	Does this instruction expand to fmad16
*/
#define DESC_FLAGS_FARITH16					(0x00008000)

/* 
   Destination mask specifies a component mask
 */
#define DESC_FLAGS_DESTCOMPMASK				(0x00010000)

/* 
   Instruction writes to  multiple, contiguous destination registers.
 */
#define DESC_FLAGS_MULTIPLEDEST		        (0x00020000)

/*
	The instruction is a complexop (LOG, EXP, RSQRT, RECIP, SIN, COS, SQRT).
*/
#define DESC_FLAGS_COMPLEXOP				(0x00040000)

/*
	This instruction is a load/store using local addressing 
	(LDLB, LDLW, LDLD, STLB, STLW, STLD).
*/
#define DESC_FLAGS_LOCALLOADSTORE			(0x00080000)

/*
	This instruction is a vector instruction.
*/
#define DESC_FLAGS_VECTOR					(0x00100000)

/*
	This instruction is a texture sample from a texture with an unspecified format; later
	expanded by the USP.
*/
#define DESC_FLAGS_USPTEXTURESAMPLE			(0x00200000)

#define DESC_FLAGS_VECTORSRC				(0x00400000)

#define DESC_FLAGS_VECTORDEST				(0x00800000)

#define DESC_FLAGS_VECTORCOMPLEXOP			(0x01000000)

/*
	This instruction is a vector PCK instruction.
*/
#define DESC_FLAGS_VECTORPACK				(0x02000000)

#define DESC_FLAGS_C10VECTORSRCS			(0x04000000)

#define DESC_FLAGS_C10VECTORDEST			(0x08000000)


/*
	This instruction can partially update its destination.
*/
#define DESC_FLAGS2_DESTMASKABLE			(0x00000001)

/*
	This instruction belongs to the bitwise group.
*/
#define DESC_FLAGS2_BITWISE					(0x00000002)

/*
	Set for instructions where the destination and the 
	source arguments must be different registers 
	(FDSX, FDSY, VDSX, VDSY).
*/
#define DESC_FLAGS2_DESTANDSRCOVERLAP		(0x00000004)

/*
	The negation source modifiers on this instruction does
	a FLOAT32 negate operation.
*/
#define DESC_FLAGS2_FLOAT32NEG				(0x00000008)

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
/*
	The first vector source to this instruction can access a 128-bit vector from the unified
	store using MOE source 1 and source 2.
*/
#define DESC_FLAGS2_WIDEVECTORSOURCE		(0x00000010)

/*
	The MOE increments/offset/base offset use units of 64-bits when applied to the destination
	of this instruction.
*/
#define DESC_FLAGS2_DEST64BITMOEUNITS		(0x00000020)

/*
	The MOE increments/offset/base offset use units of 64-bits when applied to the sources
	of this instruction.
*/
#define DESC_FLAGS2_SOURCES64BITMOEUNITS	(0x00000040)

/*
	This instruction supports F32 format arguments only.
*/
#define DESC_FLAGS2_VECTORF32ONLY			(0x00000080)
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

/*
	Set if this instruction doesn't support the END flag.
*/
#define DESC_FLAGS2_DOESNTSUPPORTENDFLAG	(0x00000100)

/*
	Set if this instruction supports a partial destination write mask without using source 0 i.e. without
	requiring the same MOE state for the destination and source 0.
*/
#define DESC_FLAGS2_DESTMASKNOSRC0			(0x00000200)

/*
	Set if this is a vector instruction whose result is a replicated scalar value.
*/
#define DESC_FLAGS2_VECTORREPLICATEDRESULT	(0x00000400)

/*
	Set if this instruction is of the TESTMASK type.
*/
#define DESC_FLAGS2_TESTMASK				(0x00000800)

/*
	Set if this instruction is of the TEST type.
*/
#define DESC_FLAGS2_TEST					(0x00001000)

/*
	Set if this instruction supports repeats using a mask only.
*/
#define DESC_FLAGS2_REPEATMASKONLY			(0x00002000)

/*
	This instruction is a load/store using absolute addressing 
	(LDAB, LDAW, LDAD, STAB, STAW, STAD).
*/
#define DESC_FLAGS2_ABSOLUTELOADSTORE		(0x00004000)

#define DESC_FLAGS2_NONMERGABLE				(0x00008000)

/*
	This instruction implicitly includes data in its calculation which varies between different
	instances even if all of its source arguments are uniform.
*/
#define DESC_FLAGS2_IMPLICITLYNONUNIFORM	(0x00010000)

#if defined(EXECPRED)
#define DESC_FLAGS2_EXECPRED				(0x00020000)
#endif /* #if defined(EXECPRED) */

/*
	Possible special values for the puMoeArgRemap array in the INST_DESC structure
*/
#define DESC_ARGREMAP_DONTCARE				(0x80000000)
#define DESC_ARGREMAP_INVALID				(0x80000001)

#define DESC_ARGREMAP_ALIASMASK				(0x40000000U)
#define DESC_ARGREMAP_DESTALIAS				(DESC_ARGREMAP_ALIASMASK | 0)
#define DESC_ARGREMAP_SRC0ALIAS				(DESC_ARGREMAP_ALIASMASK | 1)
#define DESC_ARGREMAP_SRC1ALIAS				(DESC_ARGREMAP_ALIASMASK | 2)
#define DESC_ARGREMAP_SRC2ALIAS				(DESC_ARGREMAP_ALIASMASK | 3)
#define DESC_ARGREMAP_GETALIASOP(ARG)		(ARG & ~DESC_ARGREMAP_ALIASMASK)

typedef struct _SOURCE_ARGUMENT_PAIR
{
	IMG_UINT32	uFirstSource;
	IMG_UINT32	uSecondSource;
} SOURCE_ARGUMENT_PAIR, *PSOURCE_ARGUMENT_PAIR;

typedef SOURCE_ARGUMENT_PAIR const * PCSOURCE_ARGUMENT_PAIR;

typedef enum _USC_OPTIMIZATION_GROUP_TYPE
{
	/*
		Group for instructions to be optimize for Integer peephole - IntegerPeepholeOptimize
	*/
	USC_OPT_GROUP_INTEGER_PEEPHOLE				= (IMG_INT32)(0x00000001),
	/*
		Group for f32 arithmatic instructions
	*/
	USC_OPT_GROUP_ARITH_F32_INST				= (IMG_INT32)(0x00000002),
	/*
		Group for f16 arithmatic instructions
	*/
	USC_OPT_GROUP_ARITH_F16_INST				= (IMG_INT32)(0x00000004),
	/*
		Group for stores to indexable temporary registers
	*/
	USC_OPT_GROUP_LDST_IND_TMP_INST				= (IMG_INT32)(0x00000008),
	/*
		Group for ITESTPRED & ITESTMASK
	*/
	USC_OPT_GROUP_TEST							= (IMG_INT32)(0x00000010),
	/*
		Group for EFO formation
	*/
	USC_OPT_GROUP_EFO_FORMATION					= (IMG_INT32)(0x00000020),
	/*
		Group for eliminate F16 Moves
	*/
	USC_OPT_GROUP_ELIMINATE_F16_MOVES			= (IMG_INT32)(0x00000040),
	/*
		Group for optimize dependent F16 float operations.
	*/
	USC_OPT_GROUP_DEP_F16_OPTIMIZATION			= (IMG_INT32)(0x00000080),
	/*
		Group for determining channels used in pck instructions
	*/
	USC_OPT_GROUP_CHANS_USED_PCK_U8C10SRC		= (IMG_INT32)(0x00040000),
	USC_OPT_GROUP_CHANS_USED_PCK_F16SRC			= (IMG_INT32)(0x00080000),

	/*
	  Group for determining absolute addressed loads and stores.
	*/
	USC_OPT_GROUP_ABS_MEMLDST					= (IMG_INT32)(0x00100000),
	
	USC_OPT_GROUP_HIGH_LATENCY					= (IMG_INT32)(0x00200000),

	USC_OPT_GROUP_ALL							= (IMG_INT32)(0xFFFFFFFF),
} USC_OPTIMIZATION_GROUP_TYPE, *PUSC_OPTIMIZATION_GROUP_TYPE;

typedef IMG_UINT32 (*PFN_GET_LIVE_CHANS_IN_ARGUMENT)(PINTERMEDIATE_STATE, PCINST, IMG_UINT32, IMG_PUINT32);

IMG_UINT32 GetLiveChansInTESTArgument(PINTERMEDIATE_STATE	psState, 
									  PCINST				psInst, 
									  IMG_UINT32			uArg, 
									  IMG_UINT32			auLiveChansInDest[]);
IMG_UINT32 GetLiveChansInCOMBC10Argument(PINTERMEDIATE_STATE	psState, 
										 PCINST					psInst, 
										 IMG_UINT32				uArg, 
										 IMG_UINT32				auLiveChansInDest[]);
IMG_UINT32 GetLiveChansInMOVArgument(PINTERMEDIATE_STATE		psState,
									 PCINST						psInst,
									 IMG_UINT32					uArg,
									 IMG_UINT32					auLiveChansInDest[]);
IMG_UINT32 GetLiveChansInMOVC_I32Argument(PINTERMEDIATE_STATE	psState,
										  PCINST				psInst,
										  IMG_UINT32			uArg,
										  IMG_UINT32			auLiveChansInDest[]);
IMG_UINT32 GetLiveChansInFPDOTArgument(PINTERMEDIATE_STATE	psState, 
									   PCINST				psInst, 
									   IMG_UINT32			uArg, 
									   IMG_UINT32			auLiveChansInDest[]);
IMG_UINT32 GetLiveChansInFARITH16Argument(PINTERMEDIATE_STATE	psState, 
										  PCINST				psInst, 
										  IMG_UINT32			uArg, 
										  IMG_UINT32			auLiveChansInDest[]);
IMG_UINT32 GetLiveChansInEFOArgument(PINTERMEDIATE_STATE psState, 
									 PCINST				psInst, 
									 IMG_UINT32			uArg, 
									 IMG_UINT32			auLiveChansInDest[]);
IMG_UINT32 GetLiveChansInFNRMArgument(PINTERMEDIATE_STATE	psState, 
									  PCINST				psInst, 
									  IMG_UINT32			uArg, 
									  IMG_UINT32			auLiveChansInDest[]);
IMG_UINT32 GetLiveChansInFNRM16Argument(PINTERMEDIATE_STATE psState, 
										PCINST				psInst, 
										IMG_UINT32			uArg, 
										IMG_UINT32			auLiveChansInDest[]);
IMG_UINT32 GetLiveChansInIMAEArgument(PINTERMEDIATE_STATE	psState, 
									  PCINST				psInst, 
									  IMG_UINT32			uArg, 
									  IMG_UINT32			auLiveChansInDest[]);
IMG_UINT32 GetLiveChansInIMA16Argument(PINTERMEDIATE_STATE	psState, 
									   PCINST				psInst, 
									   IMG_UINT32			uArg, 
									   IMG_UINT32			auLiveChansInDest[]);
#if defined(OUTPUT_USPBIN)
IMG_INTERNAL
IMG_UINT32 GetLiveChansInSMPUSPNDRArgument(PINTERMEDIATE_STATE	psState, 
										   PCINST				psInst, 
										   IMG_UINT32			uArg, 
										   IMG_UINT32			auLiveChansInDest[]);
IMG_UINT32 GetLiveChansInSMPUNPACKArgument(PINTERMEDIATE_STATE	psState, 
										   PCINST				psInst, 
										   IMG_UINT32			uArg, 
										   IMG_UINT32			auLiveChansInDest[]);
#endif /* defined(OUTPUT_USPBIN) */
IMG_UINT32 GetLiveChansInSHLArgument(PINTERMEDIATE_STATE	psState, 
									 PCINST					psInst, 
									 IMG_UINT32				uArg, 
									 IMG_UINT32				auLiveChansInDest[]);
IMG_UINT32 GetLiveChansInSHRArgument(PINTERMEDIATE_STATE	psState, 
									 PCINST					psInst, 
									 IMG_UINT32				uArg, 
									 IMG_UINT32				auLiveChansInDest[]);
#if defined(SUPPORT_SGX545)
IMG_UINT32 GetLiveChansInDUALArgument(PINTERMEDIATE_STATE	psState,
									  PCINST				psInst,
									  IMG_UINT32			uArg, 
									  IMG_UINT32			auLiveChansInDest[]);
#endif /* defined(SUPPORT_SGX545) */
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
IMG_INTERNAL
IMG_UINT32 GetLiveChansInVPCKFixedPointToFloatArgument(PINTERMEDIATE_STATE	psState, 
													   PCINST				psInst, 
													   IMG_UINT32			uArg, 
													   IMG_UINT32			auLiveChansInDest[]);
IMG_UINT32 GetLiveChansInVPCKU8U8Argument(PINTERMEDIATE_STATE	psState, 
										  PCINST				psInst, 
										  IMG_UINT32			uArg, 
										  IMG_UINT32			auLiveChansInDest[]);
IMG_UINT32 GetLiveChansInVectorArgument(PINTERMEDIATE_STATE	psState, 
									    PCINST				psInst, 
										IMG_UINT32			uArg, 
										IMG_UINT32			auLiveChansInDestNonVec[]);
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
IMG_UINT32 GetLiveChansInStoreArgument(PINTERMEDIATE_STATE		psState,
									   PCINST					psInst,
									   IMG_UINT32				uArg,
									   IMG_UINT32				auLiveChansInDest[]);
IMG_UINT32 GetLiveChansInSPILLWRITEArgument(PINTERMEDIATE_STATE	psState, 
										    PCINST				psInst, 
										    IMG_UINT32			uArg, 
										    IMG_UINT32			auLiveChansInDest[]);
IMG_UINT32 GetLiveChansInMOVC_U8_C10_Argument(PINTERMEDIATE_STATE	psState, 
											  PCINST				psInst, 
										      IMG_UINT32			uArg, 
										      IMG_UINT32			auLiveChansInDest[]);
IMG_UINT32 GetLiveChansInSMPArgument(PINTERMEDIATE_STATE	psState, 
									 PCINST					psInst, 
									 IMG_UINT32				uArg, 
									 IMG_UINT32				auLiveChansInDest[]);
IMG_UINT32 GetLiveChansInPCKArgument(PINTERMEDIATE_STATE	psState, 
									 PCINST					psInst, 
									 IMG_UINT32				uArg, 
									 IMG_UINT32				auLiveChansInDest[]);
IMG_UINT32 GetLiveChansInFloatArgument(PINTERMEDIATE_STATE	psState, 
									   PCINST				psInst, 
									   IMG_UINT32			uArg, 
									   IMG_UINT32			auLiveChansInDest[]);
IMG_UINT32 GetLiveChansInFPTESTArgument(PINTERMEDIATE_STATE	psState, 
									    PCINST				psInst, 
									    IMG_UINT32			uArg, 
									    IMG_UINT32			auLiveChansInDest[]);
IMG_UINT32 GetLiveChansInSaveRestoreIRegArgument(PINTERMEDIATE_STATE	psState, 
												 PCINST					psInst, 
												 IMG_UINT32				uArg, 
												 IMG_UINT32				auLiveChansInDest[]);
IMG_UINT32 GetLiveChansInC10Arg(PINTERMEDIATE_STATE psState, 
								PCINST psInst, 
								IMG_UINT32 uArg, 
								IMG_UINT32 auLiveChansInDest[]);
IMG_UINT32 GetLiveChansInIMA8Arg(PINTERMEDIATE_STATE psState, 
								 PCINST psInst, 
								 IMG_UINT32 uArg, 
								 IMG_UINT32 auLiveChansInDest[]);
IMG_UINT32 GetLiveChansInArgumentDefault(PINTERMEDIATE_STATE	psState, 
									     PCINST					psInst, 
									     IMG_UINT32				uArg, 
									     IMG_UINT32				auLiveChansInDest[]);

typedef IMG_BOOL (*PFN_CAN_USE_SRC)(PINTERMEDIATE_STATE psState, 
									PCINST				psInst, 
									IMG_UINT32			uArg, 
									IMG_UINT32			uType, 
									IMG_UINT32			uIndexType);

typedef IMG_BOOL (*PFN_CAN_USE_DEST)(PINTERMEDIATE_STATE	psState, 
									 IMG_UINT32				uDestIdx, 
									 IMG_UINT32				uDestType, 
									 IMG_UINT32				uDestIndexType);

typedef struct _INST_DESC
{
	#if defined(UF_TESTBENCH)	
	IMG_PCHAR						pszName;
	#endif /* defined(UF_TESTBENCH) */
	IMG_UINT32						uFlags;
	IMG_UINT32						uFlags2;
	IMG_UINT32						uDefaultArgumentCount;
	IMG_UINT32						uMoeArgumentCount;
	IMG_BOOL						bHasDest;
	IMG_BOOL						bCanRepeat;
	IMG_UINT32						uMaxRepeatCount;
	IMG_BOOL						bCanUseRepeatMask;
	IMG_UINT32						puMoeArgRemap[3];
	IMG_BOOL						bCanPredicate;
	IMG_UINT32						uUseasmOpcode;
	INST_TYPE						eType;
	INST_PRED						ePredicateSupport;
	IMG_UINT32						uDestRegisterNumberBitCount;
	IMG_UINT32						uSourceRegisterNumberBitCount;
	PCSOURCE_ARGUMENT_PAIR			psCommutableSources;
	IMG_UINT32						uOptimizationGroup;
	PFN_GET_LIVE_CHANS_IN_ARGUMENT	pfGetLiveChansInArgument;
	struct
	{
		PFN_CAN_USE_SRC const*			apfCanUseSrc;
		IMG_UINT32						uFuncCount;
	}								sCanUseSrc;
	PFN_CAN_USE_DEST				pfCanUseDest;
} INST_DESC, *PINST_DESC;

typedef enum _IOPCODE
{
	IINVALID = 0,

	IMOV = 1,
	IMOVPRED,
	IDELTA,

	/* DEST = SRC */
	ILIMM,

	IFMOV,
	IFADD,
	IFSUB,
	IFMUL,
	IFMAD,
	IFADM,
	IFMSA,
	IFDP,
	IFDP_RPT,
	IFDDP,
	IFDDP_RPT,
	IFRCP,
	IFRSQ,
	IFLOG,
	IFEXP,
	IFDSX,
	IFDSY,
	IFMIN,
	IFMAX,
	IFTRC,
	IFSUBFLR,

	IEFO,

	ITESTPRED,
	ITESTMASK,

	IFDP_RPT_TESTPRED,
	IFDP_RPT_TESTMASK,

	/* ILOADCONST	DEST, CONSTANT BASE, STATIC OFFSET, MEMORY INDEX, REGISTER INDEX  */
	ILOADCONST,

	/* ILOADRTXIDX	DEST */
	ILOADRTAIDX,

	/* 
		ILOADMEMCONST	DEST, CONSTANT_BASE, DYNAMIC_OFFSET, STATIC_OFFSET, STRIDE
		(DEST = MEM[CONSTANT_BASE + DYNAMIC_OFFSET * STRIDE + STATIC_OFFSET])

		STRIDE and STATIC_OFFSET must be immediates. 
	*/
	ILOADMEMCONST,

	ILDAB,				/* Load byte using absolute addressing. */
	ISTAB,				/* Store byte using absolute addressing. */
	ILDLB,				/* Load byte using local addressing. */
	ISTLB,				/* Store byte using local addressing. */

	ILDAW,				/* Load word using absolute addressing. */
	ISTAW,				/* Store word using absolute addressing. */
	ILDLW,				/* Load word using local addressing. */
	ISTLW,				/* Store word using local addressing. */

	ILDAD,				/* Load dword using absolute addressing. */
	ISTAD,				/* Store dword using absolute addressing. */
	ILDLD,				/* Load dword using local addressing. */
	ISTLD,				/* Store dword using local addressing. */
	ILDTD,				/* Load dword using tiled addressing. */

	ISPILLREAD,
	ISPILLWRITE,

	#if defined(SUPPORT_SGX545)
	IELDD,
	IELDQ,
	#endif /* defined(SUPPORT_SGX545) */

	IIDF,
	IWDF,

	IIMA16,

	IIMAE,

	IUNPCKU16S8,
	IUNPCKS16S8,

	IUNPCKU16U8,
	IUNPCKS16U8,

	IUNPCKU16U16,
	IUNPCKU16S16,

	IUNPCKU16F16,

	IPCKU16F32,

	IUNPCKS16U16,
	IPCKS16F32,

	IPCKU8U8,
	IPCKU8S8,
	IPCKU8U16,
	IPCKU8S16,
	IPCKS8U8,
	IPCKS8U16,
	IPCKS8S16,

	IPCKU8F16,
	IPCKU8F32,
	IPCKS8F16,
	IPCKS8F32,

	IPCKC10C10,
	IPCKC10F32,

	IUNPCKF16C10,
	IUNPCKF16O8,
	IUNPCKF16U8,
	IUNPCKF16S8,
	IUNPCKF16U16,
	IUNPCKF16S16,
	IPCKF16F16,
	IPCKF16F32,

	IUNPCKF32C10,
	IUNPCKF32O8,
	IUNPCKF32U8,
	IUNPCKF32S8,
	IUNPCKF32U16,
	IUNPCKF32S16,
	IUNPCKF32F16,

	IPCKC10F16,

	ICOMBC10,      /* ICOMBC10 dst, rgb, alpha --> dst = [alpha, r, g, b] */
	ICOMBC10_ALPHA,	/* ICOMBC10 dst, src1, src2 --> dst = [src2.alpha, src1.red, src1.green, src1.blue] */

	ISMP,
	ISMPBIAS,
	ISMPREPLACE,
	ISMPGRAD,

	ISHL,
	ISHR,
	IASR,
	IAND,
	IXOR,
	IOR,
	INOT,
	IRLP,

	IBR,
	ICALL,
	ILABEL,
	ILAPC,
	ISETL,
	ISAVL,
	IDRVPADDING,

	ISMLSI,
	ISMR,
	ISETFC,
	ISMBO,

	INOP,

	ISOPWM,
	ISOP2,
	ISOP3,
	ILRP1,
	IFPMA,
	IFPADD8,
	IFPSUB8,

	IIMA8,			/* d = per_channel(s0 + s1 * s2), sources/destination are 4 8-bit integer channels. */

	IFDPC,
	IFDPC_RPT,

	IFPDOT,			/* d = dot(s0, s1). U8/C10 sources and U8/C10 destination */
	IFP16DOT,		/* d = dot(s0, s1). U8 sources and U16 destination. */

	IMOVC,			/* d = test(float(s0)) ? s1 : s2 */
	IMOVC_C10,		/* d = test(c10(s0)) ? s1 : s2 */
	IMOVC_U8,       /* d = test(c10(s0)) ? s1 : s2 */
	IMOVC_I32,		/* d = test(int32(s0)) ? s1 : s2 */
	IMOVC_I16,		/* d = test(int16(s0)) ? s1 : s2 */

	IFMOV16,
	IFADD16,
	IFMUL16,
	IFMAD16,

	/* SGX545 Instructions */
	#if defined(SUPPORT_SGX545)
	IFMINMAX,		/* fminmax d s0 s1 s2 : d = min(max(s0, s1), s2) */
	IFMAXMIN,		/* fmaxmin d s0 s1 s2 : d = max(min(s0, s2), s1) */
	IFSSQ,			/* fssq d s0 s1 : d = (s0^2) + (s1^2) */
	IFSIN,			/* fsin d s1: d = sin(s1) */
	IFCOS,			/* fcos d s1: d = cos(s1) */
	#endif /* defined(SUPPORT_SGX545) */
	#if defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	IIMA32,			/* ima32 d s0 s1 s2: d = s0 * s1 + s2 + carry in */
	#endif /* defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	#if defined(SUPPORT_SGX545)
	IIDIV32,		/* idiv32 d s0 s1: d = s0/s1; d+1 = s0 mod s1 */
	IFSQRT,			/* fsqrt d s1; d = sqrt(s1) */
	IDUAL,			/* dual-issue instruction */
	IRNE,			/* d = round_to_nearest(s) */
	ITRUNC_HW,		/* d = truncate(s) */
	IFLOOR,			/* d = floor(s) */
	#endif /* defined(SUPPORT_SGX545) */

	#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
	/* SGX540 Instructions */
	IFNRM,          /* 
					   fnrm d, s0, s1, (s2|swizzle), drc: normalise a
					   vector. 
					   neg and abs modifiers are taken from src1.
					*/
	IFNRMF16,
	#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */
	/* 
	   Load and store to arrays.

	   ldarr dst, (tag, fmt, addr), dynoff:  dst.chan = mem((addr + dynoff + chan) * scale)
	   starr (tag, fmt, addr), dynoff, src: mem((addr + dynoff + chan) * scale) = src.chan

	   Array tag and format are stored in Src0.
	   chan: Each channel to read/write, specifed by the mask on src and dst.
	   scale: Number of bytes needed to store a single channel in the src/dst format.
	   addr: Base address of the array
	   dynoff: Register with the dynamic offset

	   INST.uDestCount is the number of registers read/written (worked from the mask of 
	   channels and the format size).
	*/
	ILDARRF32,			/* ldarr dst, (tag, fmt, addr), dynof: Array load   */
	ILDARRF16,			/* ldarr dst, (tag, fmt, addr), dynof: Array load   */
	ILDARRU8,			/* ldarr dst, (tag, fmt, addr), dynof: Array load   */
	ILDARRC10,			/* ldarr dst, (tag, fmt, addr), dynof: Array load   */

	ISTARRF32,			/* starr (tag, fmt, addr), dynof, src, src, src, src: Array store */
	ISTARRF16,			/* starr (tag, fmt, addr), dynof, src, src: Array store */
	ISTARRU8,			/* starr (tag, fmt, addr), dynof, src: Array store */
	ISTARRC10,			/* starr (tag, fmt, addr), dynof, src: Array store */

#if defined(OUTPUT_USPBIN)
	/* Special sample ops for use with the USP */
	ISMP_USP,
	ISMPBIAS_USP,
	ISMPREPLACE_USP,
	ISMPGRAD_USP,
	ISMP_USP_NDR,
	ISMPUNPACK_USP,
	/* Texture write : write data to a given pixel in an texture */
	ITEXWRITE,
#endif /* defined(OUTPUT_USPBIN) */

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	IUNPCKVEC,
	IVMAD,				/* DEST = SRC0 * SRC1 + SRC2 */
	IVMAD4,				/* Same as IVMAD but also supports repeat */
	IVMUL,				/* DEST = SRC0 * SRC1 */
	IVMUL3,				/* DEST = SRC0 * SRC1 multiples only 3 components and also supports repeat */
	IVADD,				/* DEST = SRC0 + SRC1 */
	IVADD3,				/* DEST = SRC0 + SRC1 adds only 3 components and also supports repeat */
	IVFRC,				/* DEST = SRC0 - floor(SRC1) */
	IVTRC,				/* DEST = ROUND_TOWARDS_ZERO(SRC1) */
	IVDSX,				/* DEST = top_right_instance(SRC0) - top_left_instance(SRC0) */
	IVDSY,				/* DEST = bottom_left_instance(SRC0) - top_left_instance(SRC0) */
	IVDSX_EXP,			/* Versions of VDSX/VDSY with two source argument (both containing the same data). */
	IVDSY_EXP,			
	IVMIN,				/* DEST = min(SRC0, SRC1) */
	IVMAX,				/* DEST = max(SRC0, SRC1) */
	IVDP,				/* DEST = SRC0.x * SRC1.x + SRC0.y * SRC1.y + SRC0.z * SRC1.z + SRC0.w * SRC1.w */
	IVDP2,				/* DEST = SRC0.x * SRC1.x * SRC0.Y * SRC1.y */
	IVDP3,				/* DEST = SRC0.x * SRC1.x + SRC0.y * SRC1.y + SRC0.z * SRC1.z */
	IVDP_GPI,			/* Same as IVDP but also supports repeat */
	IVDP3_GPI,			/* Same as IVDP3 but also supports repeat */
	IVDP_CP,			/* Same as IVDP but outputting a clipplane P flag as well */
	IVMOV,				/* DEST = SRC0 */
	IVMOV_EXP,			/* DEST = SRC0 */
	IVMOVC,				/* DEST = test(SRC0) ? SRC1 : SRC2 */
	IVMOVCU8_FLT,		/* DEST = test(u8(SRC0)) ? SRC1_FLOAT : SRC2_FLOAT */
	IVMOVCBIT,			/* DEST = (any_bits_set(SRC0)) ? SRC1 : SRC2 */
	IVMOVC_I32,			/* DEST = test(int32(SRC0)) ? SRC1 : SRC2 */
	IVMOVCU8_I32,		/* DEST = test(u8(SRC0)) ? SRC1 : SRC2 */
	IVRSQ,
	IVRCP,
	IVLOG,
	IVEXP,
	IVTEST,
	IVTESTMASK,			/* DEST = test(alu_op(SRC0, SRC1)) ? 1 : 0 */
	IVTESTBYTEMASK,
	IVDUAL,				/* Vector dual-issue instruction. */
	IVPCKU8U8,
	IVPCKU8FLT,
	IVPCKS8FLT,
	IVPCKU8FLT_EXP,
	IVPCKC10FLT,
	IVPCKC10FLT_EXP,
	IVPCKC10C10,
	IVPCKFLTU8,
	IVPCKFLTS8,
	IVPCKFLTU16,
	IVPCKFLTS16,
	IVPCKFLTC10,
	IVPCKFLTC10_VEC,
	IVPCKFLTFLT,
	IVPCKFLTFLT_EXP,
	IVPCKS16FLT,
	IVPCKU16FLT,
	IVPCKS16FLT_EXP,
	IVPCKU16FLT_EXP,
	IVPCKU16U16,
	IVPCKS16S8,
	IVPCKU16U8,
	ILDAQ,

	IMKC10VEC,

	/*
		Vector equivalents for fixed-point integer instructions. Towards the end of
		compilation C10 vector registers are replaced by pairs of 32-bit registers (since
		a C10 vector is 40-bits long) and at the same time opcode x is replaced by
		x_VEC.
	*/
	ISOPWM_VEC,
	ISOP2_VEC,
	ISOP3_VEC,
	ILRP1_VEC,
	IFPMA_VEC,
	IFPADD8_VEC,
	IFPSUB8_VEC,
	IFPDOT_VEC,
	IFPTESTPRED_VEC,
	IFPTESTMASK_VEC,
	IVMOVC10,
	IVMOVCC10,
	IVMOVCU8,
	IIADD32,
	IIADDU32,
	IISUB32,
	IISUBU32,
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	
	IWOP,

	IISUB8,
	IISUBU8,
	IIADD8,
	IIADDU8,
	IISUB16,
	IISUBU16,
	IIADD16,
	IEMIT,

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	IIDXSCR,			// Reads a word at a word granular address from an array of hardware registers 
	IIDXSCW,			// Writes a word at a word granular address to an array of hardware registers 
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	ICVTFLT2INT,		// Convert a floating point value to integer

	ICVTINT2ADDR,		// Convert an integer value to a suitable format for addressing shader input/outputs
						// or a constant buffer in memory.
						//
						//	DEST = (SRC < 65535) ? SRC : 65535.
						//

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	ICVTFLT2ADDR,		// Convert a floating point value to an (integer) address value
	#endif //defined (SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)

	/*
		Special instruction representing the code inserted by the driver to send part of the shader
		results to the ISP (for pixel program) or the VCB (for vertex program) and then make a decision
		whether to continue with the rest of the program.
	*/
	IFEEDBACKDRIVEREPILOG,
	ISAVEIREG,			// Used in iregalloc.c to represent a point where an internal register's contents have to be 
						// saved into a unified store register.
	IRESTOREIREG,		// Used in iregalloc.c to represent a point where an internal register's contents have to be
						// restore from a unified store register.
	ISAVECARRY,			// Used in iregalloc.c to represent a point where an internal register containing an integer
						// carry-in/carry-out is saved.
	IRESTORECARRY,		// Used in iregalloc.c to represent a point where an internal register containing an integer
						// carry-in/carry-out is restored.
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		Special instruction to load a vector of scalar intermediate registers into an internal register.
	*/
	IVLOAD,
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	#if defined(EXECPRED)
	/*
		Conditional execution instructions. These enable dynamic execution of instructions
		within a per-instance PC: all instances follow longest path, with implied predication
		used to disable instructions for instances that should not execute them. 'shortcut' 
		branching can be used when all instances follow same route.

		These instructions all have similar srces/dests, described as follows:

		count_dest_reg:			dest (temp) register for the new value of the execution-mask count
		count_src_reg:			source (temp) register containing the current execution mask count
		new_count_reg:			source register containing a new value for the execution-mask counter
		cond_pred_src_reg:		predicate containing the result of a test guarding execution
		loop_pred_dest_reg:		predicate indicating whether any instance wants to execute a loop
								(used to repeat the loop untill all instances should exit).
		count_adjust_immediate:	Static integer used to update/test the current execution-mask count

		The descriptions below indicate which of these sources and dests are used by each
		CONDxxx instruction. However, only count_dest_reg, count_src_reg and new_count_reg are actually
		represented as source/dest registers.
	*/
	ICNDST,				// CNDST	count_dest_reg, count_src_reg, cond_pred_src_reg, count_adjust_immediate
	ICNDEF,				// CNDEF	count_dest_reg, count_src_reg, cond_pred_src_reg, count_adjust_immediate
	ICNDSM,				// CNDSM	count_dest_reg, count_src_reg, cond_pred_src_reg, new_count_reg
	ICNDLT,				// CNDLT	count_dest_reg, loop_pred_dest_reg, count_src_reg, cond_pred_src_reg, count_adjust_immediate
	ICNDEND,			// CNDEND	count_dest_reg, count_src_reg, count_adjust_immediate
	ICNDSTLOOP,			// CNDSTLOOP
	ICNDEFLOOP,			// CNDEFLOOP
	IBREAK,
	ICONTINUE,
	IRETURN,
	ILOOPTEST,
	#endif	/* defined(EXECPRED) */

	/* Mutex lock/unlock instructions */
	ILOCK,
	IRELEASE,
	
	INVALID_MAXARG,		// Invalid instruction, has maximum number of source operands.
	IOPCODE_MAX
} IOPCODE;

/*
  ARG: Representation of registers and instruction operands.

  Fields used to identify a register:
  uType, uNumber, uIndex,
  uArrayOffset (if uType == USC_REGTYPE_REGARRAY),
  uNumberPreMoe (if psState->uFlags.USC_FLAGS_MOESTAGE)
 */
typedef struct _ARG
{
	/* Register type of this argument (use USEASM_REGTYPE_x values). */
	IMG_UINT32			uType;
	/* Register number or immediate value of this argument. */
	IMG_UINT32			uNumber;
	struct _VREGISTER*	psRegister;
	/* Type of the register used as a dynamic index. */
	IMG_UINT32			uIndexType;
	/* Number of the register used as a dynamic index. */
	IMG_UINT32			uIndexNumber;
	struct _VREGISTER*	psIndexRegister;
	/* Static offset of the register used as a dynamic index. */
	IMG_UINT32			uIndexArrayOffset;
	/* Units of the dynamic index argument. */
	IMG_UINT32			uIndexStrideInBytes;
	/* Static offset into a register array. (Put into union with uImmTempNum.) */
	IMG_UINT32			uArrayOffset;
	/* Register number before any MOE offsets. */
	IMG_UINT32			uNumberPreMoe; 
	/* Data type (F32/U8, C10, F16) of this argument. */
	UF_REGFORMAT		eFmt;
} ARG;

#define USC_ARG_DEFAULT_VALUE								\
		{													\
			USEASM_REGTYPE_UNDEF,		/* uType */			\
			USC_UNDEF,					/* uNumber */		\
			NULL,						/* psRegister */	\
			USC_REGTYPE_NOINDEX,		/* uIndexType */	\
			USC_UNDEF,					/* uIndexNumber */	\
			NULL,						/* psIndexRegister */	\
			USC_UNDEF,					/* uIndexArrayOffset */		\
			USC_UNDEF,					/* uIndexStrideInBytes */	\
			USC_UNDEF,					/* uArrayOffset */	\
			USC_UNDEF,					/* uNumberPreMoe */	\
			UF_REGFORMAT_INVALID,		/* eFmt */			\
	    }

/*
	Possible definitions for the test done by a TEST instruction.
*/
typedef enum _TEST_TYPE
{
	TEST_TYPE_INVALID,
	TEST_TYPE_ALWAYS_TRUE,
	TEST_TYPE_GT_ZERO,
	TEST_TYPE_GTE_ZERO,
	TEST_TYPE_EQ_ZERO,
	TEST_TYPE_LT_ZERO,
	TEST_TYPE_LTE_ZERO,
	TEST_TYPE_NEQ_ZERO,
	TEST_TYPE_SIGN_BIT_CLEAR,
	TEST_TYPE_SIGN_BIT_SET,
	TEST_TYPE_MAXIMUM,
} TEST_TYPE, *PTEST_TYPE;

typedef struct _TEST_DETAILS
{
	/*
		Test to perform on each channel of the ALU result.
	*/
	TEST_TYPE			eType;

	/*
		How to combine the tests on individual channels.
	*/
	USEASM_TEST_CHANSEL	eChanSel;

	/*
		For a TESTMASK instruction: the type of mask to generate.
	*/
	USEASM_TEST_MASK	eMaskType;
} TEST_DETAILS, *PTEST_DETAILS;
typedef TEST_DETAILS const* PCTEST_DETAILS;

/*
	Parameters specific to scalar floating point instructions.
*/
typedef struct _FLOAT_SOURCE_MODIFIER
{
	/* Use the negation of the source. */
	IMG_BOOL	bNegate;
	/* Use the absolute value of the source (applied before negation). */
	IMG_BOOL	bAbsolute;
	/* For C10/F16 sources selects the part of the 32-bit source to use. */
	IMG_UINT32	uComponent;
} FLOAT_SOURCE_MODIFIER, *PFLOAT_SOURCE_MODIFIER;

#define MAX_FLOAT_SOURCES		(3)

typedef struct _FLOAT_PARAMS
{
	FLOAT_SOURCE_MODIFIER	asSrcMod[MAX_FLOAT_SOURCES];
} FLOAT_PARAMS, *PFLOAT_PARAMS;

#define COMPLEXOP_SOURCE			(0)
#define COMPLEXOP_SOURCE_ARGMASK	(1 << COMPLEXOP_SOURCE)
#define COMPLEXOP_ARGUMENT_COUNT	(1)

#define FMINMAX_ARGUMENT_COUNT		(3)
#define FMAXMIN_ARGUMENT_COUNT		(3)
#define FSSQ_ARGUMENT_COUNT			(2)

/*
	Parameters specific to FDP/FDDP/FDP_TESTPRED/FDP_TESTMASK instructions.
*/
#define FDP_RPT_PER_ITERATION_ARGUMENT_COUNT		(2)
#define FDDP_PER_ITERATION_ARGUMENT_COUNT			(3)
#define FDP_RPT_ARGUMENT_COUNT						(EURASIA_USE_MAXIMUM_REPEAT * FDP_RPT_PER_ITERATION_ARGUMENT_COUNT)
#define FDDP_MAXIMUM_REPEAT							(8)
#define FDDP_RPT_ARGUMENT_COUNT						(FDDP_MAXIMUM_REPEAT * FDDP_PER_ITERATION_ARGUMENT_COUNT)

typedef struct _FDOTPRODUCT_PARAMS
{
	/*
		Number of iterations for the dotproduct.
	*/
	IMG_UINT32				uRepeatCount;

	/*
		For FDPC only: the clipplane to update.
	*/
	IMG_UINT32				uClipPlane;

	/*
		Source modifiers.
	*/
	IMG_BOOL				abNegate[max(FDDP_PER_ITERATION_ARGUMENT_COUNT, FDP_RPT_PER_ITERATION_ARGUMENT_COUNT)];
	IMG_BOOL				abAbsolute[max(FDDP_PER_ITERATION_ARGUMENT_COUNT, FDP_RPT_PER_ITERATION_ARGUMENT_COUNT)];
	IMG_UINT32				auComponent[max(FDDP_RPT_ARGUMENT_COUNT, FDP_RPT_ARGUMENT_COUNT)];

	/*
		For FDP_TESTPRED/FDP_TESTMASK the type of test to perform on
		the dotproduct result.
	*/
	TEST_DETAILS			sTest;
} FDOTPRODUCT_PARAMS, *PFDOTPRODUCT_PARAMS;

/* SHORT_REG: Short register identifiers */
typedef struct _SHORT_REG
{
	/* eType: Register type */
	USC_REGTYPE eType;
	/* uNumber: Register number */
	IMG_UINT32 uNumber;
} SHORT_REG, *PSHORT_REG;

typedef enum _EFO_SRC
{
	I0,
	I1,
	A0,
	A1,
	M0,
	M1,
	SRC0,
	SRC1,
	SRC2,
	I2,
	EFO_SRC_UNDEF
} EFO_SRC;

typedef struct _EFO_PARAMETERS
{
	/*
		ALU source for the data written to the main destination.
		Can be: I0, I1, A0, A1

		In the case of I0 and I1 the internal register is read before
		any writes to I0 or I1 by the instruction.
	*/
	EFO_SRC			eDSrc;

	/*
		ALU sources for the data written to the internal registers:
		Can be: A0/A1, A1/A0, M0/M1, A0/M1
	*/
	EFO_SRC			eI0Src, eI1Src;

	/* ALU sources for the two adders. */
	EFO_SRC			eA0Src0, eA0Src1;
	EFO_SRC			eA1Src0, eA1Src1;

	/* ALU sources for the two multipliers. */
	EFO_SRC			eM0Src0, eM0Src1;
	EFO_SRC			eM1Src0, eM1Src1;

	/* Negation flags for the arguments to the adders. */
	IMG_BOOL		bA0RightNeg;
	IMG_BOOL		bA1LeftNeg;

	/* 
		If TRUE the main destination is a dummy register which
		will not be used.
	*/
	IMG_BOOL		bIgnoreDest;

	/*
		Information about which ALU results are used.
	*/
	IMG_BOOL		bM0Used, bM1Used;
	IMG_BOOL		bA0Used, bA1Used;

	IMG_BOOL		bI0Used, bI1Used;
	IMG_BOOL		bWriteI0, bWriteI1;

	/*
		Source modifiers for the EFO arguments.
	*/
	FLOAT_PARAMS	sFloat;
} EFO_PARAMETERS, *PEFO_PARAMETERS;

/*
	Main instruction destination.
*/
#define EFO_US_DEST					(0)

/*
	Special destinations used during EFO generation.
	They represents general registers which is written
	with the data written to I0/I1 by the instruction.
*/
#define EFO_USFROMI0_DEST			(1)
#define EFO_USFROMI1_DEST			(2)

/*
	Destinations representing internal registers written
	by the EFO.
*/
#define EFO_I0_DEST					(3)
#define EFO_I1_DEST					(4)

#define EFO_DEST_COUNT				(5)

#define EFO_UNIFIEDSTORE_SOURCE0		(0)
#define EFO_UNIFIEDSTORE_SOURCE0_MASK	(1 << EFO_UNIFIEDSTORE_SOURCE0)
#define EFO_UNIFIEDSTORE_SOURCE1		(1)
#define EFO_UNIFIEDSTORE_SOURCE2		(2)

#define EFO_UNIFIEDSTORE_SOURCE_COUNT	(3)
#define EFO_HARDWARE_SOURCE_COUNT		(EFO_UNIFIEDSTORE_SOURCE_COUNT)
#define EFO_I0_SRC						(EFO_HARDWARE_SOURCE_COUNT + 0)
#define EFO_I1_SRC						(EFO_HARDWARE_SOURCE_COUNT + 1)
#define EFO_SRC_COUNT					(EFO_HARDWARE_SOURCE_COUNT + 2)

#if defined(SUPPORT_SGX545)
#define DUAL_MAX_PRI_SOURCES					(3)

#define DUAL_MAX_SEC_SOURCES					(3)

#define DUAL_PRIMARY_DESTINDEX					(0)
#define DUAL_SECONDARY_DESTINDEX				(1)

#define DUAL_USSOURCE0							(0)
#define DUAL_USSOURCE1							(1)
#define DUAL_USSOURCE2							(2)
#define DUAL_I0SOURCE							(3)
#define DUAL_I1SOURCE							(4)
#define DUAL_I2SOURCE							(5)
#define DUAL_ARGUMENT_COUNT						(6)

typedef enum DUAL_SEC_SRC
{
	DUAL_SEC_SRC_PRI0 = 0,
	DUAL_SEC_SRC_PRI1,
	DUAL_SEC_SRC_PRI2,
	DUAL_SEC_SRC_I0,
	DUAL_SEC_SRC_I1,
	DUAL_SEC_SRC_I2,
	DUAL_SEC_SRC_PRI0_MASK = 1 << DUAL_SEC_SRC_PRI0,
	DUAL_SEC_SRC_PRI1_MASK = 1 << DUAL_SEC_SRC_PRI1,
	DUAL_SEC_SRC_PRI2_MASK = 1 << DUAL_SEC_SRC_PRI2,
	DUAL_SEC_SRC_I0_MASK = 1 << DUAL_SEC_SRC_I0,
	DUAL_SEC_SRC_I1_MASK = 1 << DUAL_SEC_SRC_I1,
	DUAL_SEC_SRC_I2_MASK = 1 << DUAL_SEC_SRC_I2,
	DUAL_SEC_SRC_UNDEFINED_IREG,
	DUAL_SEC_SRC_INVALID,
} DUAL_SEC_SRC;

typedef struct _DUAL_PARAMS
{
	/*
		Primary and secondary operations.
	*/
	IOPCODE			ePrimaryOp;
	IOPCODE			eSecondaryOp;

	/*
		Negate source 
	*/
	IMG_BOOL		abPrimarySourceNegate[DUAL_MAX_PRI_SOURCES];

	/*
		Component select for F16 format sources.
	*/
	IMG_UINT32		auSourceComponent[DUAL_MAX_PRI_SOURCES];

	/* 
		Sources to the secondary operation.
		Each is either an internal registers or a source shared with the primary operation.
	*/
	DUAL_SEC_SRC	aeSecondarySrcs[DUAL_MAX_SEC_SOURCES];

	/*
		Mask of sources used by the secondary operation.
	*/
	IMG_UINT32		uSecSourcesUsed;
} DUAL_PARAMS, *PDUAL_PARAMS;

/*	
	Definitions specific to the ELD instruction.
*/
#define ELD_BASE_ARGINDEX				(0)
#define ELD_DYNAMIC_OFFSET_ARGINDEX		(1)
#define ELD_STATIC_OFFSET_ARGINDEX		(2)
#define ELD_RANGE_ARGINDEX				(3)
#define ELD_DRC_ARGINDEX				(4)
#define ELD_ARGUMENT_COUNT				(5)

/*
	Definitions specific to the IDIV32 instruction
*/
#define IDIV32_DIVIDEND_ARGINDEX		(0)
#define IDIV32_DIVISOR_ARGINDEX			(1)
#define IDIV32_DRC_ARGINDEX				(2)
#define IDIV32_ARGUMENT_COUNT			(3)
#endif /* defined(SUPPORT_SGX545) */

typedef struct _SOP2_PARAMS
{
	IMG_UINT32	uCSel1;
	IMG_UINT32	uCSel2;
	IMG_BOOL	bComplementCSel1;
	IMG_BOOL	bComplementCSel2;
	IMG_BOOL	bComplementCSrc1;
	IMG_UINT32	uCOp;
	IMG_UINT32	uASel1;
	IMG_UINT32	uASel2;
	IMG_BOOL	bComplementASel1;
	IMG_BOOL	bComplementASel2;
	IMG_UINT32	uAOp;
	IMG_BOOL	bComplementASrc1;
} SOP2_PARAMS, *PSOP2_PARAMS;

#define SOP2_ARGUMENT_COUNT				(2)

typedef struct _SOP3_PARAMS
{
	IMG_UINT32	uCSel1;
	IMG_UINT32	uCSel2;
	IMG_BOOL	bComplementCSel1;
	IMG_BOOL	bComplementCSel2;
	IMG_BOOL	bNegateCResult;
	IMG_UINT32	uCOp;

	IMG_UINT32	uCoissueOp;

	IMG_UINT32	uASel1;
	IMG_UINT32	uASel2;
	IMG_BOOL	bComplementASel1;
	IMG_BOOL	bComplementASel2;
	IMG_BOOL	bNegateAResult;
	IMG_UINT32	uAOp;
} SOP3_PARAMS, *PSOP3_PARAMS;

#define SOP3_SOURCE0_ARGMASK			(1 << 0)
#define SOP3_SOURCE1_ARGMASK			(1 << 1)
#define SOP3_SOURCE2_ARGMASK			(1 << 2)
#define SOP3_ARGUMENT_COUNT				(3)

typedef struct _LRP1_PARAMS
{
	IMG_UINT32	uCSel10;
	IMG_BOOL	bComplementCSel10;
	IMG_UINT32	uCSel11;
	IMG_BOOL	bComplementCSel11;
	IMG_UINT32	uCS;

	IMG_UINT32	uASel1;
	IMG_BOOL	bComplementASel1;
	IMG_UINT32	uASel2;
	IMG_BOOL	bComplementASel2;
	IMG_UINT32	uAOp;
} LRP1_PARAMS, *PLRP1_PARAMS;

#define LRP1_ARGUMENT_COUNT				(3)

/*
	Parameters specific to FMOV16/FADD16/FMUL16/FMAD16.
*/
#define FMAD16_SOURCE_COUNT					(3)

typedef enum
{
	FMAD16_SWIZZLE_LOWHIGH,
	FMAD16_SWIZZLE_LOWLOW,
	FMAD16_SWIZZLE_HIGHHIGH,
	FMAD16_SWIZZLE_CVTFROMF32
} FMAD16_SWIZZLE, *PFMAD16_SWIZZLE;

typedef struct _FARITH16_PARAMS
{
	FMAD16_SWIZZLE		aeSwizzle[FMAD16_SOURCE_COUNT];

	FLOAT_PARAMS		sFloat;
} FARITH16_PARAMS, *PFARITH16_PARAMS;

#if defined(OUTPUT_USPBIN)
/*
	Parameters specific to the special SMP instructions generated for the USP
*/
typedef struct _SMP_USP_PARAMS_
{
	/* For non-dependent USP samples: whether the coords should be projected */
	IMG_BOOL		bProjected;

	/* For non-dependent USP samples: whether the coords should have centroid adjusted */
	IMG_BOOL		bCentroid;

	/* Whether USP pseudo-sample represents a non-dependent texture-sample */
	IMG_BOOL		bNonDependent;

	/* For non-dependent USP samples: The texture-coordinate set to use */
	IMG_UINT32		uNDRTexCoord;

	/* Mask representing which channels of the destination are needed */
	IMG_UINT32		uChanMask;

	/* Swizzle mapping sampled channels to result-channels */
	IMG_UINT32		uChanSwizzle;


	/* Original precision of the sampler/texture in the input instruction */
	UF_REGFORMAT	eTexPrecision;

	/* Start of the range of temporary registers available for the USP generated texture unpacking code. */
	ARG				sTempReg;

	/* ID of USP sample unpack instruction this sample instruction belongs to. */
	IMG_UINT32		uSmpID;

	/* Points to the USP sample unpack instruction this sample instruction belongs to. */
	struct _INST*	psSampleUnpack;
} SMP_USP_PARAMS, *PSMP_USP_PARAMS;

/*
	For the SMP_USP_NDR instruction the start and maximum count of the arguments representing the 
	primary attributes containing the packed result of the texture sample.
*/
#define NON_DEP_SMP_TFDATA_ARG_START		(0)
#define NON_DEP_SMP_TFDATA_ARG_MAX_COUNT	(4)

/*
	For the SMP_USP_NDR instruction the start of the arguments representing the source texture
	coordinate for the texture sample.
*/
#define NON_DEP_SMP_COORD_ARG_START			(4)

/*
	For the SMPUNPACK instruction the start and maximum count of the arguments representing the 
	primary attributes containing the packed result of the texture sample.
*/
#define SMPUNPACK_TFDATA_ARG_START			(UF_MAX_CHUNKS_PER_TEXTURE * 2)

/*
	Parameters specific to the texture write instruction generated for the USP
*/
typedef struct _TEXWRITE_PARAMS
{
	/* Start of the range of temporary registers available for the USP generated texture unpacking code. */
	ARG				sTempReg;

	/* ID of USP texture write instruction this instruction belongs to. */
	IMG_UINT32		uWriteID;

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

	/* Register numbers, types and formats of the registers containing the colours */
	IMG_UINT32		uChannelXRegNum;
	IMG_UINT32		uChannelXRegType;

	IMG_UINT32		uChannelYRegNum;
	IMG_UINT32		uChannelYRegType;

	IMG_UINT32		uChannelZRegNum;
	IMG_UINT32		uChannelZRegType;

	IMG_UINT32		uChannelWRegNum;
	IMG_UINT32		uChannelWRegType;

	/* The register format of the channel data */
	IMG_UINT32		uChannelRegFmt;

} TEXWRITE_PARAMS, *PTEXWRITE_PARAMS;
#endif /* #if defined(OUTPUT_USPBIN)*/

#define IMA32_SOURCE_COUNT		(3)

typedef struct
{
	/* IMA32 */		
	/* If TRUE the instruction is signed. */
	IMG_BOOL		bSigned;
	/* 
		Which step of the IMA32 calculation to performed. 
		On SGX543 it has two steps.
	*/
	IMG_UINT32	uStep;
	/*
		Negate source modifiers.
	*/
	IMG_BOOL		abNegate[IMA32_SOURCE_COUNT];
} IMA32_PARAMS, *PIMA32_PARAMS;

#define IMA32_HIGHDWORD_DEST		(1)

/*
	Definitions specific to the FPDOT/FPDOT16 instructions.
*/

typedef struct
{
	/* log2 of the factor to scale the result by. */
	IMG_UINT32		uDot34Scale;
	/* Length of the source vectors (either 3 or 4). */
	IMG_UINT32		uVecLength;
	/* If TRUE then subtract 0.5 from each component of the source vectors before the dotproduct. */
	IMG_BOOL		bOffset;
} DOT34_PARAMS, *PDOT34_PARAMS;

#define FPDOT_ARGUMENT_COUNT		(2)

/*
	Definitions specific to the FDPC instruction.
*/

typedef struct
{
	/* Clipplane to update. */
	IMG_UINT32		uClipplane;

	/* General parameters for floating point instructions. */
	FLOAT_PARAMS		sFloat;
} DPC_PARAMS, *PDPC_PARAMS;

#define FDPC_ARGUMENT_COUNT				(2)

/* **************************************** Data specific to the SMP instruction. **************************************/

/*
	Maximum number of registers needed for the result of sampling a single plane.
*/
#define SMP_MAX_SAMPLE_RESULT_DWORDS		(4)

/*
	Index of the start of the SMP arguments containing the texture coordinates.
*/
#define SMP_COORD_ARG_START					(0)

/*
	Maximum number of registers used for texture coordinates.
*/
#define SMP_MAX_COORD_SIZE					(4)

/*
	Index of the SMP argument to project the texture coordinates by.
*/
#define SMP_PROJ_ARG						(SMP_COORD_ARG_START + SMP_MAX_COORD_SIZE)
/*
	Index of the SMP argument which is the index into the a texture array.
*/
#define SMP_ARRAYINDEX_ARG					(SMP_PROJ_ARG + 1)
/*
	Index of the start of the SMP arguments containing the texture state.
*/
#define SMP_STATE_ARG_START					(SMP_ARRAYINDEX_ARG + 1)
/*
	Maximum number of registers used for texture state.
*/
#define SMP_MAX_STATE_SIZE					(4)
/*
	Index of the SMP argument which is the DRC register to associate with the texture sample.
*/
#define SMP_DRC_ARG_START					(SMP_STATE_ARG_START + SMP_MAX_STATE_SIZE)
/*
	Index of the SMP argument which is the LOD bias or replace value.
*/
#define SMP_LOD_ARG_START					(SMP_DRC_ARG_START + 1)
/*
	Index of the start of the SMP arguments which are the gradients to use for the texture
	sample.
*/
#define SMP_GRAD_ARG_START					(SMP_LOD_ARG_START + 1)
/*
	Maximum number of registers needed for gradient data.
*/
#define SMP_MAXIMUM_GRAD_ARG_COUNT			(7)
/*
	Index of the start of the SMP IDX arguments which is the sample idx to use for the texture
	sample.
*/
#define SMP_SAMPLE_IDX_ARG_START			(SMP_GRAD_ARG_START + SMP_MAXIMUM_GRAD_ARG_COUNT)
/*
	Count of arguments to the intermediate SMP/SMPBIAS/SMPREPLACE/SMPGRAD instruction.
*/
#define SMP_MAXIMUM_ARG_COUNT				(SMP_SAMPLE_IDX_ARG_START + 1)

typedef enum _SMP_RETURNDATA
{
	/* Return the filtered texture sample. */
	SMP_RETURNDATA_NORMAL,
	/* Return the parameters to the texture filtering unit (the mipmap level, trilinear/bilinear fractions). */
	SMP_RETURNDATA_SAMPLEINFO,
	/* Return the texture samples before filtering. */
	SMP_RETURNDATA_RAWSAMPLES,
	/* Return both the sample information and the unfiltered texture samples. */
	SMP_RETURNDATA_BOTH
} SMP_RETURNDATA, *PSMP_RETURNDATA;

typedef struct _SMP_IMM_OFFSETS
{
	/* Does this texture sample use immediate offsets? */
	IMG_BOOL	bPresent;
	/* Per-dimension integer offsets. */
	IMG_INT32	aiOffsets[UF_MAX_TEXTURE_DIMENSIONALITY];
} SMP_IMM_OFFSETS, *PSMP_IMM_OFFSETS;

typedef struct _SAMPLE_RESULT_CHUNK
{
	IMG_UINT32				uSizeInRegs;
	IMG_UINT32				uSizeInDwords;
	USC_CHANNELFORM			eFormat;
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	IMG_BOOL				bVector;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
} SAMPLE_RESULT_CHUNK, *PSAMPLE_RESULT_CHUNK;

typedef struct
{
	/* Size (in registers) of the coordinate data used by this instruction. */
	IMG_UINT32			uCoordSize;
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/* If TRUE then use the upper half of the vector intermediate register in texture coordinate source. */
	IMG_BOOL			bCoordSelectUpper;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	/* Size (in registers) of the gradient data used by this instruction. */
	IMG_UINT32			uGradSize;
	/* Input texture stage this instruction corresponds to. */
	IMG_UINT32			uTextureStage;
	/* Plane within the texture this instruction is sampling. */
	IMG_UINT32			uFirstPlane;
	/* Does this lookup use PCF? */
	IMG_BOOL			bUsesPCF;
	/* Dimensionality of the texture sample. */
	IMG_UINT32			uDimensionality;
	/*
		The mask of bytes or c10 channels referenced from the last register of
		coordinate data. All the other registers are completely referenced.
	*/
	IMG_UINT32			uUsedCoordChanMask;
	/*
		Mask of channels used from each register of gradient data.
	*/
	IMG_UINT32			auUsedGradChanMask[SMP_MAXIMUM_GRAD_ARG_COUNT];
	/*
		Type of data to return.
	*/
	SMP_RETURNDATA		eReturnData;
	/*
		TRUE if the coordinates are divided by 1/SMP_PROJ_ARG before the
		texture sample.
	*/
	IMG_BOOL			bProjected;
	/* Component select on SMP_PROJ_ARG. */
	IMG_UINT32			uProjChannel;
	/* Mask of channels used from SMP_PROJ_ARG. */
	IMG_UINT32			uProjMask;
	/* TRUE if this texture sample might not be executed by all instances. */
	IMG_BOOL			bInsideDynamicFlowControl;
	/* Immediate, integer offset to apply to the texture sample. */
	SMP_IMM_OFFSETS		sImmOffsets;
	/* TRUE if this SMP instruction is a lookup into a texture array. */
	IMG_BOOL			bTextureArray;
	/* Count of planes sampled. */
	IMG_UINT32			uPlaneCount;
	/* Information about each plane sampled. */
	SAMPLE_RESULT_CHUNK	asPlanes[UF_MAX_CHUNKS_PER_TEXTURE];
	/* Sample Idx. update present or not. */
	IMG_BOOL			bSampleIdxPresent;

	#if defined(OUTPUT_USPBIN)
	/*
		Extra data passed to the USP for the special SMP instrucitons
	*/
	SMP_USP_PARAMS	sUSPSample;
	#endif /* defined(OUTPUT_USPBIN) */
} SMP_PARAMS, *PSMP_PARAMS;

#if defined(OUTPUT_USPBIN)
#define INVALID_SAMPLE_ID	(UINT_MAX)
typedef struct
{
	/* ID of USP sample instruction this unpack instruction belongs to. */
	IMG_UINT32		uSmpID;

	/* Points to the USP sample instruction this unpack instruction belongs to. */
	struct _INST*	psTextureSample;
	
	/* Mask representing which channels of the destination are needed */
	IMG_UINT32		uChanMask;

	/* Start of the range of temporary registers available for the USP generated texture unpacking code. */
	ARG				sTempReg;

	#if defined(INITIALISE_REGS_ON_FIRST_WRITE)
	/* Dest-register write-mask (USP pseudo-samples unpack write 1-4 regs) */
	IMG_UINT32		auInitialisedChansInDest[4];
	#endif /* defined(INITIALISE_REGS_ON_FIRST_WRITE) */
} SMPUNPACK_PARAMS, *PSMPUNPACK_PARAMS;
#endif /* defined(OUTPUT_USPBIN) */

/*	
	Instruction specific parameters for FPMA/IMA8 instructions.
*/
typedef struct
{
	IMG_UINT32	uCSel0;
	IMG_UINT32	uCSel1;
	IMG_UINT32	uCSel2;
	IMG_UINT32	uASel0;
	IMG_BOOL	bComplementCSel0;
	IMG_BOOL	bComplementCSel1;
	IMG_BOOL	bComplementCSel2;
	IMG_BOOL	bComplementASel0;
	IMG_BOOL	bComplementASel1;
	IMG_BOOL	bComplementASel2;
	IMG_BOOL	bNegateSource0;
	/*
		Saturates the result to 0..255 (IMA8) or 0..1 (FPMA).
	*/
	IMG_BOOL	bSaturate;
} FPMA_PARAMS, *PFPMA_PARAMS;

#define FPMA_ARGUMENT_COUNT			(3)

typedef struct
{
	IMG_UINT32	uSel1;
	IMG_UINT32	uSel2;
	IMG_BOOL	bComplementSel1;
	IMG_BOOL	bComplementSel2;
	IMG_UINT32	uCop;
	IMG_UINT32	uAop;
	IMG_BOOL	bRedChanFromAlpha;
} SOPWM_PARAMS, *PSOPWM_PARAMS;

#define SOPWM_SOURCE_COUNT		(2)

/*	
	Instruction specific parameters for LD/ST instructions.
*/
typedef struct
{
	/*	
		If TRUE then enable range checking for the load/store.
	*/
	IMG_BOOL			bEnableRangeCheck;
	/*
		If TRUE then bypass the cache on the load/store.
	*/
	IMG_BOOL			bBypassCache;
	/*
		This load must be synchronised in place
	*/
	IMG_BOOL                        bSynchronised;
	/*
		Pointer aliasing flags (values are UF_MEM_xxx copied 
		from the corresponding input memory load or store's flags).
	*/
	IMG_UINT8			uFlags;
	/*
		If TRUE then update the base address argument after the 
		instruction to the memory address loaded/stored by
		the instruction.
	*/
	IMG_BOOL			bPreIncrement;
	/*
		If TRUE add the offset to the base to make the memory address.
		If FALSE subtract the offset from the base.
	*/
	IMG_BOOL			bPositiveOffset;
} LDST_PARAMS, *PLDST_PARAMS;

#define MEMLOADSTORE_BASE_ARGINDEX			(0)
#define MEMLOADSTORE_OFFSET_ARGINDEX		(1)

#define MEMLOAD_RANGE_ARGINDEX				(2)
#define MEMLOAD_DRC_ARGINDEX				(3)
#define MEMLOAD_ARGUMENT_COUNT				(4)

#define MEMSTORE_DATA_ARGINDEX				(2)
#define MEMSTORE_ARGUMENT_COUNT				(3)
/*
	Definitions for the ILDTD instructions.
*/
#define TILEDMEMLOAD_BASE_ARGINDEX			(0)
#define TILEDMEMLOAD_OFFSET_ARGINDEX		(1)
#define TILEDMEMLOAD_DRC_ARGINDEX			(2)
#define TILEDMEMLOAD_ARGUMENT_COUNT			(3)

#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
#define NRM_F32_USSOURCE_COUNT			(3)
#define NRM_F16_SWIZZLE					(2)
#define NRM_DRC_SOURCE					(3)
#define NRM_ARGUMENT_COUNT				(4)

typedef struct
{
	IMG_BOOL		bSrcModNegate;
	IMG_BOOL		bSrcModAbsolute;
	IMG_UINT32		auComponent[NRM_F32_USSOURCE_COUNT];
} NRM_PARAMS, *PNRM_PARAMS;
#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */

/*
	Instruction specific parameters for the ILOADCONST instruction.
*/
typedef struct
{	
	/*
		If TRUE the source offset into the constant buffer includes a
		dynamic (calculated by the program) index.
	*/
	IMG_BOOL				bRelativeAddress;

	/*
		If TRUE the constant loaded by this instruction has a fixed value
		known at compile time.
	*/
	IMG_BOOL				bStaticConst;

	/*
		If bStaticConst is TRUE then the known value of the loaded constant.
	*/
	IMG_UINT32				uStaticConstValue;

	/*
		Format of the constant data loaded by the instruction.
	*/
	UNIFLEX_CONST_FORMAT	eFormat;

	/*
		Points to the constant range (if any) this instruction is
		loading from.
	*/
	struct _CONSTANT_RANGE*	psRange;

	/*
		The stride (in bytes) applied to the dynamic index.
	*/
	IMG_UINT32				uRelativeStrideInBytes;

	/*
		Index of the constant buffer this instruction loads from.
	*/
	IMG_UINT32				uBufferIdx;

	/*
		Where a program does indexing down to subcomponents of a C10 vector we generate 
		a LOADCONST for each component and set uC10CompOffset to the offset of the
		component to load.
	*/
	IMG_UINT32				uC10CompOffset;

	/*
		Entry in the list of LOADCONST instructions loading from a particular constant buffer.
	*/
	USC_LIST_ENTRY			sBufferListEntry;

	/*
		Points back to the associated instruction.
	*/
	struct _INST*			psInst;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		If TRUE the instruction is loading a vector of F32, F16 or C10
		values.
	*/
	IMG_BOOL				bVectorResult;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
} LOADCONST_PARAMS, *PLOADCONST_PARAMS;

/* Indices into the LOADCONST argument array. */
#define LOADCONST_STATIC_OFFSET_ARGINDEX		(0)
#define LOADCONST_DYNAMIC_OFFSET_ARGINDEX		(1)
#define LOADCONST_ARGUMENT_COUNT				(2)

/*
	Instruction specific parameters for the ILOADMEMCONST instruction.
*/
typedef struct
{
	/*
		TRUE if the dynamic offset is non-immediate.
	*/
	IMG_BOOL				bRelativeAddress;
	/*
		After fetch instructions are created the count of consecutive memory
		locations to fetch.
	*/
	IMG_UINT32				uFetchCount;
	/*
		TRUE if the sum of the static and dynamic offsets should be checked against
		another parameter which is the length of the constant buffer.
	*/
	IMG_BOOL				bRangeCheck;
	/*
		Size of the data to be loaded by the instruction in bytes. 
	*/
	IMG_UINT32				uDataSize;
	/*
		TRUE to bypass the cache when loading the constant.
	*/
	IMG_BOOL				bBypassCache;
	/*
		This load must be synchronised in place
	*/
	IMG_BOOL                        bSynchronised;
	/*
		Flags containing address space and aliasing data.
	 */
	IMG_UINT8				uFlags;
} LOADMEMCONST_PARAMS, *PLOADMEMCONST_PARAMS;

#define LOADMEMCONST_BASE_ARGINDEX				(0)
#define LOADMEMCONST_STATIC_OFFSET_ARGINDEX		(1)
#define LOADMEMCONST_DYNAMIC_OFFSET_ARGINDEX	(2)
#define LOADMEMCONST_STRIDE_ARGINDEX			(3)
#define LOADMEMCONST_MAX_RANGE_ARGINDEX			(4)
#define LOADMEMCONST_ARGUMENT_COUNT				(5)

/*
	Instruction specific parameters for the IMA16 instruction.
*/

typedef struct _IMA16_PARAMS
{
	/* Negate modifier for source 2. */
	IMG_BOOL	bNegateSource2;
} IMA16_PARAMS, *PIMA16_PARAMS;

#define IMA16_ARGUMENT_COUNT					(3)

/*
	Instruction specific parameters for the IMAE instruction.
*/

/*
	Number of IMAE sources excluding the carry source (which always comes from an internal
	registers).
*/
#define IMAE_UNIFIED_STORE_SOURCE_COUNT		(3)

typedef struct
{
	/*
		Interpretation of source 2: Can be USEASM_INTSRCSEL_U32, USEASM_INTSRCSEL_Z16 or USEASM_INTSRCSEL_S16.
	*/
	IMG_UINT32		uSrc2Type;
	IMG_BOOL		bSigned;
	IMG_UINT32		auSrcComponent[IMAE_UNIFIED_STORE_SOURCE_COUNT];
} IMAE_PARAMS, *PIMAE_PARAMS;

/*
	Index of the destination which gets written with the carry-out.
	Must be either unused or an internal register.
*/
#define IMAE_CARRYOUT_DEST			(1)

/*
	Index of the source from which a carry-in is read.
	Must be either unused or an internal register.
*/
#define IMAE_CARRYIN_SOURCE			(3)

#define IMAE_ARGUMENT_COUNT			(4)

struct _INST;
struct _FUNC_;
typedef struct
{
	struct _INST *psCallSiteNext;
	struct _FUNC_ *psTarget;
	struct _CODEBLOCK_ *psBlock; //the block containing (only!) this CALL.
	IMG_BOOL bDead; //called function does not write any register live after this CALL
} CALL_PARAMS, *PCALL_PARAMS;

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
/*
	Maximum number of vector sized registers that can be read by an
	instruction.
*/
#define VECTOR_MAX_SOURCE_SLOT_COUNT			(4)

/*
	Maximum number of vector sized registers which can be written by
	an instruction.
*/
#define VECTOR_MAX_DESTINATION_COUNT			(5)

/*
	Number of source arguments which represent a vector sized source. One if the source is
	a single variable plus four if it is a set of scalar variables.
*/
#define SOURCE_ARGUMENTS_PER_VECTOR				(1 + MAX_SCALAR_REGISTERS_PER_VECTOR)

#define VECTOR_SLOT0		(0)
#define VECTOR_SLOT1		(1)
#define VECTOR_SLOT2		(2)
#define VECTOR_SLOT0_MASK	(1 << VECTOR_SLOT0)
#define VECTOR_SLOT1_MASK	(1 << VECTOR_SLOT1)
#define VECTOR_SLOT2_MASK	(1 << VECTOR_SLOT2)
#define VECTOR_SLOT01_MASK	(VECTOR_SLOT0_MASK | VECTOR_SLOT1_MASK)
#define VECTOR_SLOT12_MASK	(VECTOR_SLOT1_MASK | VECTOR_SLOT2_MASK)

/*
	Number of source slots for a VMAD instruction.
*/
#define VMAD_SOURCE_SLOT_COUNT					(3)

#define VEC_MOESOURCE_ARGINDEX(N)			((N) * SOURCE_ARGUMENTS_PER_VECTOR + 1)

#define VEC_ARGUMENT_COUNT(N)				((N) * SOURCE_ARGUMENTS_PER_VECTOR)

#define C10VEC_MOESOURCE_ARGINDEX(N)		((N) * SCALAR_REGISTERS_PER_C10_VECTOR)
#define C10VEC_ARGUMENT_COUNT(N)			C10VEC_MOESOURCE_ARGINDEX(N)

#define VPCK_NONVEC_ARGUMENT_COUNT			(1)

typedef struct
{
	IMG_BOOL		bNegate;
	IMG_BOOL		bAbs;
} VEC_SOURCE_MODIFIER, *PVEC_SOURCE_MODIFIER;

/*
	Possible increment control modes for single issue GPI-oriented instructions
	(VMAD4, VADD3, VMUL3, VDP_GPI, VDP_GPI).
*/
typedef enum
{
	SVEC_REPEATMODE_UNDEF,
	SVEC_REPEATMODE_INCREMENTUS,
	SVEC_REPEATMODE_INCREMENTGPI,
	SVEC_REPEATMODE_INCREMENTBOTH,
	SVEC_REPEATMODE_USEMOE,
} SVEC_REPEATMODE, *PSVEC_REPEATMODE;

/*
	Mapping between sources to the intermediate VDUAL instruction and
	encoding slots in the hardware instruction.
*/
#define VDUAL_SRCSLOT_UNIFIEDSTORE			(0)
#define VDUAL_SRCSLOT_UNIFIEDSTORE_MASK		(1 << VDUAL_SRCSLOT_UNIFIEDSTORE)
#define VDUAL_SRCSLOT_GPI0					(1)
#define VDUAL_SRCSLOT_GPI0_MASK				(1 << VDUAL_SRCSLOT_GPI0)
#define VDUAL_SRCSLOT_GPI1					(2)
#define VDUAL_SRCSLOT_GPI1_MASK				(1 << VDUAL_SRCSLOT_GPI1)
#define VDUAL_SRCSLOT_GPI2					(3)
#define VDUAL_SRCSLOT_GPI2_MASK				(1 << VDUAL_SRCSLOT_GPI2)
#define VDUAL_SRCSLOT_COUNT					(4)

#define VDUAL_ALL_GPI_SRC_SLOTS				(VDUAL_SRCSLOT_GPI0_MASK | VDUAL_SRCSLOT_GPI1_MASK | VDUAL_SRCSLOT_GPI2_MASK)

/*
	Mapping between destinations of the intermediate VDUAL instruction and
	encoding slots in the hardware instruction.
*/
typedef enum _VDUAL_DESTSLOT
{
	VDUAL_DESTSLOT_UNIFIEDSTORE = 0,
	VDUAL_DESTSLOT_GPI,
	VDUAL_DESTSLOT_COUNT,
} VDUAL_DESTSLOT, *PVDUAL_DESTSLOT;

/*
	Possible primary or secondary operation types for a dual-issued vector
	instruction.
*/
typedef enum _VDUAL_OP
{
	VDUAL_OP_MAD1 = 0,
	VDUAL_OP_MAD3,
	VDUAL_OP_MAD4,
	VDUAL_OP_DP3,
	VDUAL_OP_DP4,
	VDUAL_OP_SSQ3,
	VDUAL_OP_SSQ4,
	VDUAL_OP_MUL1,
	VDUAL_OP_MUL3,
	VDUAL_OP_MUL4,
	VDUAL_OP_ADD1,
	VDUAL_OP_ADD3,
	VDUAL_OP_ADD4,
	VDUAL_OP_MOV3,
	VDUAL_OP_MOV4,
	VDUAL_OP_RSQ,
	VDUAL_OP_RCP,
	VDUAL_OP_LOG,
	VDUAL_OP_EXP,
	VDUAL_OP_FRC,
	VDUAL_OP_UNDEF,
	VDUAL_OP_COUNT,
} VDUAL_OP, *PVDUAL_OP;

typedef enum _VDUAL_OP_TYPE
{
	/* This source slot is used by the primary operation. */
	VDUAL_OP_TYPE_PRIMARY,
	/* This source slot is used by the secondary operation. */
	VDUAL_OP_TYPE_SECONDARY,
	/* This source slot isn't referenced by either operation. */
	VDUAL_OP_TYPE_NONE,
	VDUAL_OP_TYPE_UNDEF,
} VDUAL_OP_TYPE, *PVDUAL_OP_TYPE;

/*
	Information specific to the VDUAL instruction.
*/
typedef struct _VDUAL
{
	/*
		TRUE if the dual-issue operation reads/writes unified store sources/destinations as F16 format.
	*/
	IMG_BOOL		bF16;

	/*
		TRUE if both the dual-issue operations are both vec3 or less.
	*/
	IMG_BOOL		bVec3;

	/*
		TRUE if the dual-issue operations use GPI slot 2.
	*/
	IMG_BOOL		bGPI2SlotUsed;

	/*
		TRUE if the result of the primary operation is written to the GPI destination.
	*/
	IMG_BOOL		bPriUsesGPIDest;

	/*
		Type of primary operation.
	*/
	VDUAL_OP		ePriOp;

	/*
		Set to TRUE if the primary operation writes a result which is never used.
	*/
	IMG_BOOL		bPriOpIsDummy;

	/*
		Type of secondary operation.
	*/
	VDUAL_OP		eSecOp;

	/*
		Set to TRUE if the secondary operation writes a result which is never used.
	*/
	IMG_BOOL		bSecOpIsDummy;

	/*
		Maps from source slots to the operation (primary or secondary) which uses them.
	*/
	VDUAL_OP_TYPE	aeSrcUsage[VDUAL_SRCSLOT_COUNT];

	/*
		Maps from arguments to the primary operation to source slots.
	*/
	IMG_UINT32		auPriOpSrcs[SGXVEC_USE_DVEC_OP1_MAXIMUM_SRC_COUNT];

	/*
		Maps from arguments to the secondary operation to source slots.
	*/
	IMG_UINT32		auSecOpSrcs[SGXVEC_USE_DVEC_OP2_MAXIMUM_SRC_COUNT];
} VDUAL, *PVDUAL;

typedef enum _VTEST_CHANSEL
{
	VTEST_CHANSEL_INVALID,
	/*	
		Select the result of the test on the X channel and write it to a
		predicate register.
	*/
	VTEST_CHANSEL_XCHAN,
	/*
		Write four predicate registers; each with the result of the test
		on the corresponding channel.
	*/
	VTEST_CHANSEL_PERCHAN,
	/*
		Take the logical and of the results of the tests on all channels and
		write it to a single predicate register.
	*/
	VTEST_CHANSEL_ANDALL,
	/*
		Take the logical or of the results of the tests on all channels and
		write it to a single predicate register.
	*/
	VTEST_CHANSEL_ORALL,
} VTEST_CHANSEL, PVTEST_CHANSEL;

/*
	Instruction specific parameters for the VTEST/VTESTMASK/VTESTBYTEMASK instruction.
*/
typedef struct _VTEST_PARAMS
{
	/*
		The ALU operation to perform.
	*/
	IOPCODE				eTestOpcode;

	/*
		The type of test to perform on the ALU result.
	*/
	TEST_TYPE			eType;

	/*
		For the VTESTMASK/VTESTBYTEMASK instruction: the format of the generated mask.
	*/
	USEASM_TEST_MASK	eMaskType;

	/*
		For the VTEST instruction: how to combine/select the result of the tests on the
		individual channels of the vector ALU result.
	*/
	VTEST_CHANSEL		eChanSel;
} VTEST_PARAMS, *PVTEST_PARAMS;

/*
	Instruction specific data for SGX543 vector instructions.
*/
typedef struct
{
	/*
		Swizzle for each source argument.
	*/
	IMG_UINT32			auSwizzle[VECTOR_MAX_SOURCE_SLOT_COUNT];

	/*
		Modifier (negate and/or absolute) for each source argument.
	*/
	VEC_SOURCE_MODIFIER	asSrcMod[VECTOR_MAX_SOURCE_SLOT_COUNT];

	/*
		Format (F16 or F32) for each source argument.
	*/
	UF_REGFORMAT		aeSrcFmt[VECTOR_MAX_SOURCE_SLOT_COUNT];

	/*
		If the bit corresponding to a source argument is set then apply an extra swizzle
		mapping X->Z and Y->W to that source. This corresponds to (for unified store
		sources) increasing the register number by two.
	*/
	IMG_UINT32			auSelectUpperHalf[UINTS_TO_SPAN_BITS(VECTOR_MAX_SOURCE_SLOT_COUNT)];

	/*
		Before writing the result of the instruction shift the channels so X is moved to X+SHIFT, Y to
		Y+SHIFT, etc. An instruction which writes channels before X+SHIFT is invalid. This corresponds
		to increasing the register number by SHIFT.
	*/
	IMG_UINT32			uDestSelectShift;

	/*
		If TRUE then this is an instruction which has been switched to write its result to
		the Y channel of a vector register to avoid alignment restrictions.
	*/
	IMG_BOOL			bWriteYOnly;

	/*
		Parameters specific to VTEST and VTESTMASK;
	*/
	VTEST_PARAMS		sTest;

	/*
		For VMOVC: the test to perform on source 0.
	*/
	TEST_TYPE			eMOVCTestType;

	/*
		Index of the clipplane P flag to write (for VDP_CP only).
	*/
	IMG_UINT32			uClipPlaneOutput;
	
	/*
		Repeat mode for the VDP3, VDP4, VDP_CP, VMAD3 and VMAD4
		instructions.
	*/
	SVEC_REPEATMODE		eRepeatMode;

	/*
		Information specific to the VDUAL instruction.
	*/
	VDUAL				sDual;

	/*	
		Scale flag for vector pack instructions.
	*/
	IMG_BOOL			bPackScale;
	
	/*
		For VPCKFLTFLT_EXP, VPCKU16FLT_EXP, VPCKS16FLT_EXP, VPCKU8FLT_EXP, VPCKC10FLT_EXP:
		Selects which channel from each of the two sources is copied into the destination.
		X and Y select the first and second channels from source 1.
		Z and W select the first and second channels from source 2.
	*/
	IMG_UINT32			uPackSwizzle;
} VEC_PARAMS, *PVEC_PARAMS;
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

#define USPSMP_DESTSLOT_COUNT	(2)

/*	
	Parameters specific to the TESTPRED and TESTMASK instructions.
*/
#define TEST_ALUOPCODE_MAXIMUM_ARGUMENT_COUNT	(2)

#define TEST_PREDICATE_DESTIDX					(0)
#define TEST_PREDICATE_ONLY_DEST_COUNT			(TEST_PREDICATE_DESTIDX + 1)
#define TEST_UNIFIEDSTORE_DESTIDX				(1)
#define TEST_MAXIMUM_DEST_COUNT					(2)

#define VTEST_PREDICATE_DESTIDX					(0)
#define VTEST_PREDICATE_ONLY_DEST_COUNT			(VTEST_PREDICATE_DESTIDX + 4)
#define VTEST_UNIFIEDSTORE_DESTIDX				(4)
#define VTEST_MAXIMUM_DEST_COUNT				(5)

#define TEST_SOURCE0							0
#define TEST_SOURCE0_ARGMASK					(1 << TEST_SOURCE0)
#define TEST_SOURCE1							1
#define TEST_SOURCE1_ARGMASK					(1 << TEST_SOURCE1)
#define TEST_ARGUMENT_COUNT						(2)

#define FPTEST_SOURCE0							TEST_SOURCE0
#define FPTEST_SOURCE0_MASK						TEST_SOURCE0_ARGMASK
#define FPTEST_SOURCE1							TEST_SOURCE1
#define FPTEST_SOURCE1_MASK						TEST_SOURCE1_ARGMASK
#define FPTEST_SOURCE01_MASK					(FPTEST_SOURCE0_MASK | FPTEST_SOURCE1_MASK)
#define FPTEST_ARGUMENT_COUNT					TEST_ARGUMENT_COUNT

typedef struct
{
	IOPCODE			eAluOpcode;
	IMG_UINT32		auSrcComponent[TEST_ALUOPCODE_MAXIMUM_ARGUMENT_COUNT];
	TEST_DETAILS	sTest;
} TEST_PARAMS, *PTEST_PARAMS;

/*
	Definitions specific to conditional move instructions.
*/
typedef struct
{
	TEST_TYPE		eTest;
} MOVC_PARAMS, *PMOVC_PARAMS;

#define MOVC_ARGUMENT_COUNT						(3)

/*
	Definitions specific to EMIT instructions.
*/
typedef struct
{
	IMG_BOOL		bCut;
} EMIT_PARAMS, *PEMIT_PARAMS;

#define EMIT_VSOUT_SOURCE						(0)
#define EMIT_ARGUMENT_COUNT						(1)

#define WOP_PTNB_SOURCE							(0)
#define WOP_VSOUT_SOURCE						(1)
#define WOP_ARGUMENT_COUNT						(2)

/*
	Instruction specific data for the ISPILLREAD and ISPILLWRITE instructions.
*/
typedef struct
{
	/*
		Mask of the channels read later from the data stored.
	*/
	IMG_UINT32		uLiveChans;
	/*
		If TRUE then bypass the cache when loading/storing.
	*/
	IMG_BOOL		bBypassCache;
} MEMSPILL_PARAMS, *PMEMSPILL_PARAMS;

#define MEMSPILL_BASE_ARGINDEX			(0)
#define MEMSPILL_OFFSET_ARGINDEX		(1)

#define MEMSPILLREAD_RANGE_ARGINDEX		(2)
#define MEMSPILLREAD_DRC_ARGINDEX		(3)
#define MEMSPILLREAD_ARGUMENT_COUNT		(4)

#define MEMSPILLWRITE_DATA_ARGINDEX		(2)
#define MEMSPILLWRITE_ARGUMENT_COUNT	(3)

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
/*
	Instruction specific data for the IDXSCR and IDXSCW instructions.
*/
typedef struct _IDXSC_PARAMS
{
	IMG_UINT32		uChannelCount;
	IMG_UINT32		uChannelOffset;
} IDXSC_PARAMS, *PIDXSC_PARAMS;

#define IDXSCR_INDEXEDVALUE_ARGINDEX			(0)
#define IDXSCR_ARGUMENT_COUNT					(1)

#define IDXSCW_DATATOWRITE_ARGINDEX				(0)
#define IDXSCW_ARGUMENT_COUNT					(1)

#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

/*
	Instruction specific data for the CVTFLT2INT instruction.
*/
typedef struct _CVTFLT2INT_PARAMS
{
	/* TRUE to convert to a signed integer; FALSE to convert to an unsigned integer. */
	IMG_BOOL		bSigned;

} CVTFLT2INT_PARAMS, *PCVTFLT2INT_PARAMS;

#define CVTFLT2INT_ARGUMENT_COUNT		(1)

/*
	Instruction specific data for the CVTINT2ADDR instruction.
*/
#define CVTINT2ADDR_ARGUMENT_COUNT		(1)

/*
	Instruction specific data for the PCK instruction.
*/
#define PCK_SOURCE0					(0)
#define PCK_SOURCE0_ARGMASK			(1 << PCK_SOURCE0)
#define PCK_SOURCE1					(1)
#define PCK_SOURCE1_ARGMASK			(1 << PCK_SOURCE1)
#define PCK_ALLSOURCES_ARGMASK		(PCK_SOURCE0_ARGMASK | PCK_SOURCE1_ARGMASK)
#define PCK_SOURCE_COUNT			(2)

typedef struct _PCK_PARAMS
{
	IMG_UINT32	auComponent[PCK_SOURCE_COUNT];
	IMG_BOOL	bScale;
} PCK_PARAMS, *PPCK_PARAMS;


/*
	Instruction specific data for the LDARRxx/STARRxx instructions.
*/
typedef struct _LDSTARR_PARAMS
{
	/* uLdStMask: Mask of channels read/written by ILDARR*, ISTARR* instructions. */
	IMG_UINT32      uLdStMask;
	/* Offset into the array (in terms of vec4s) of the element to load or store. */
	IMG_UINT32		uArrayOffset;
	/* Identifier of the array to load to/store from. */
	IMG_UINT32		uArrayNum;
	/* Format of the data to load/store. */
	UF_REGFORMAT	eFmt;
	/* Units of the dynamic offset in vector components. */
	IMG_UINT32		uRelativeStrideInComponents;
} LDSTARR_PARAMS, *PLDSTARR_PARAMS;

#define LDSTARR_DYNAMIC_OFFSET_ARGINDEX	(0)

#define LDSTARR_STORE_DATA_ARGSTART		(1)
#define STARRF16_ARGUMENT_COUNT			(LDSTARR_STORE_DATA_ARGSTART + SCALAR_REGISTERS_PER_F16_VECTOR)
#define STARRF32_ARGUMENT_COUNT			(LDSTARR_STORE_DATA_ARGSTART + SCALAR_REGISTERS_PER_F32_VECTOR)
#define STARRU8_ARGUMENT_COUNT			(LDSTARR_STORE_DATA_ARGSTART + 1)
#define STARRC10_ARGUMENT_COUNT			(LDSTARR_STORE_DATA_ARGSTART + 1)

#define LDARR_ARGUMENT_COUNT			(1)

/*	
	Instruction specific definitions for the MOV instruction.
*/
#define MOV_ARGUMENT_COUNT				(1)

/*	
	Instruction specific definitions for the LIMM instruction.
*/
#define LIMM_ARGUMENT_COUNT				(1)

/*
	Instruction specific definitions for the bitwise instructions.
*/
#define BITWISE_SHIFTEND_SOURCE			0
#define BITWISE_SHIFT_SOURCE			1
#define BITWISE_ARGUMENT_COUNT			(2)

/*
	Instruction specific definitions for the RLP instruction.
*/
#define RLP_PREDICATE_DESTIDX			(0)
#define RLP_UNIFIEDSTORE_DESTIDX		(1)
#define RLP_DEST_COUNT					(2)

#define RLP_UNIFIEDSTORE_ARGIDX			(0)
#define RLP_MOEARGUMENT_COUNT			(1)
#define RLP_PREDICATE_ARGIDX			(1)
#define RLP_ARGUMENT_COUNT				(2)

/*
	Instruction specific definitions for the COMBC10 instruction.
*/
#define COMBC10_RGB_SOURCE				(0)
#define COMBC10_RGB_SOURCEMASK			(1 << COMBC10_RGB_SOURCE)
#define COMBC10_ALPHA_SOURCE			(1)
#define COMBC10_ALPHA_SOURCEMASK		(1 << COMBC10_ALPHA_SOURCE)
#define COMBC10_SOURCE_COUNT			(2)

/*
	Instruction specific definitions for the WDF instruction.
*/
#define WDF_DRC_SOURCE					(0)
#define WDF_ARGUMENT_COUNT				(1)

/*
	Instruction specific definitions for the IDF instruction.
*/
#define IDF_DRC_SOURCE					(0)
#define IDF_PATH_SOURCE					(1)
#define IDF_ARGUMENT_COUNT				(2)

/*
	Instruction specific definitions for the SETL instruction.
*/
#define SETL_ARGUMENT_COUNT				(1)

/*
	Instruction specific definitions for the SAVL instruction.
*/
#define SAVL_ARGUMENT_COUNT				(1)

/*
	Instruction specific definitions for the MOE control instructions.
*/
#define SMLSI_ARGUMENT_COUNT			(USC_MAX_MOE_OPERANDS * 2)
#define SMR_ARGUMENT_COUNT				USC_MAX_MOE_OPERANDS
#define SMBO_ARGUMENT_COUNT				USC_MAX_MOE_OPERANDS
#define SETFC_ARGUMENT_COUNT			(2)

/*
	Instruction specific definitions for the TRC/FLR/RNE instructions.
*/
#define ROUND_ARGUMENT_COUNT			(1)

/* Instruction specific data for the delta instruction. */

typedef struct _DELTA_PARAMS
{
	/* Points back to the delta instruction. */
	struct _INST*	psInst;
	/* Entry in the list of delta instructions in the same basic block. */
	USC_LIST_ENTRY	sListEntry;
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/* If TRUE then the delta instruction is choosing between vector registers. */
	IMG_BOOL		bVector;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
} DELTA_PARAMS, *PDELTA_PARAMS;

/* Instruction specific data for the MOVP instruction. */

typedef struct _MOVP_PARAMS
{
	/* If TRUE move the logical negation of the source predicate. */
	IMG_BOOL		bNegate;
} MOVP_PARAMS, *PMOVP_PARAMS;

/* Instruction specific data for the FEEDBACKDRIVEREPILOG instruction. */
typedef struct _FEEDBACKDRIVEREPILOG_PARAMS
{
	struct _FIXED_REG_DATA_*	psFixedReg;
	IMG_UINT32					uFixedRegOffset;
	/*
		If FEEDBACKDRIVEREPILOG_PARAMS.bPartial is TRUE then the channel which is used
		in the pre-feedback driver epilog.
	*/
	IMG_UINT32					uFixedRegChan;
	/*
		If TRUE then this result is partially used in the pre-feedback driver epilog and
		completely used by the post-shader driver epilog.

		If FALSE then this result is used in pre-feedback driver epilog only.
	*/
	IMG_BOOL					bPartial;
} FEEDBACKDRIVEREPILOG_PARAMS, *PFEEDBACKDRIVEREPILOG_PARAMS;

/* Instruction specific data for the BITWISE instructions (AND, OR, XOR, SHL, SHR, ASR). */

typedef struct _BITWISE_PARAMS
{
	/* If TRUE apply a bitwise negation modifier to source 2. */
	IMG_BOOL	bInvertSrc2;
} BITWISE_PARAMS, *PBITWISE_PARAMS;

typedef struct
{
	IMG_UINT32		uEfoGroupId;
	struct _INST*	psPrevWriter;
	struct _INST*	psNextWriter;
	struct _INST*	psFirstReader;
	struct _INST*	psNextReader;
	IMG_BOOL		bSelfContained;
	struct _INST*	psNextDeschedInst;
	IMG_BOOL		abIRegUsed[2];
} EFO_STAGEDATA, *PEFO_STAGEDATA;

typedef struct
{
  /* Linked list of instructions which might form a repeat group with this one. */
  struct _INST* psPrev;
  struct _INST* psNext;
} REPEAT_GROUP, *PREPEAT_GROUP;

#define USC_REPEAT_GROUP_DEFAULT { IMG_FALSE, NULL, NULL }

typedef enum _INST_FLAGS_
{
	INST_NULL = 0,			// The null flag

	INST_PRED_NEG,			// Negate the predicate
	INST_PRED_PERCHAN,		// Apply the predicate per channel

	INST_SKIPINV,
	INST_SYNCSTART,
	INST_NOSCHED,
	INST_ONCEONLY,
	INST_FETCH,

	INST_TYPE_PRESERVE,
	INST_DUMMY_FETCH,
	
	INST_SPILL,
	INST_FORMAT_SELECT,
	INST_MOE,				// MOE Swizzle enable

	INST_SINGLE,  			// Instruction is a single issue
	INST_NOEMIT,            // Instruction not emitted (used for internal purposes only)

    /* Flags used by the c10 register allocator */
	INST_RGB,               // Instruction is the RGB part of a pair of instructions
	INST_ALPHA,             // Instruction is the Alpha part of a pair of instructions

	INST_ISUNUSED,

	INST_HAS_REF,           // Instruction has a reference to an unresolved identifier
    /*
	  INST_LOCALx: Flags which are local to a single pass. Helps to keep the size of
	  the bitvector to a minimum.

	  Use macro definitions to rename the flag to something more meaningfull.
	  INST_LOCAlx flags must always be cleared at the end of the pass. 

	  Flags which could be removed: INST_SPILL, INST_RGB, INST_ALPHA,
	*/
	INST_LOCAL0,   
	INST_LOCAL1,

	INST_MODIFIEDINST,
#if defined(SUPPORT_SGX545)
	/*
 		To indicate instruction capabilities for dual issuing.
	*/
	INST_DI_PRIMARY, 
	INST_DI_SECONDARY, 
#endif
	INST_END,

	/*
		Set if scheduling should be disabled between this instruction and the previous one (same semantics as
		if the internal registers are live between them).
	*/
	INST_DISABLE_SCHED,

	INST_FLAGS_NUM			// Total number of instruction flags
} INST_FLAGS;

#if defined(TRACK_REDUNDANT_PCONVERSION)
typedef enum tagPatternRuleNodeType
{
	NODE_INSTRUCTION,
	NODE_OR,
	NODE_AND,
	NODE_ANYTHING,
	NODE_CONCAT,
	NODE_START,
}PATTERN_RULE_NODE_TYPE;

/*
	Structure for defining pattern searching rules.
*/
typedef struct tagPatternRule
{
	/*
	         (Node)
	          /  \
	         /    \
	        /      \
	    psLeftNode  \
	             psRightNode
	*/
	IOPCODE		eOpcode;
	PATTERN_RULE_NODE_TYPE	eType;
	struct tagPatternRule	const *psRightNode;
	struct tagPatternRule	const *psLeftNode;
#if defined(DEBUG)
	const IMG_CHAR			*pszPatternName;
#endif
}PATTERN_RULE, *PPATTERN_RULE;

/*
	Struct to hold direct acyclic graph (DAG) info per instruction.
*/
#define DEP_GRAPH_SUPERORDINATE_VISITED			0x00000001
#define DEP_GRAPH_HAS_SUBERORDINATES			0x00000002

#define DEP_GRAPH_COMPUTE_COLLECT_OLD_DEST		0x00000001
#define DEP_GRAPH_COMPUTE_COLLECT_ARG_INFO		0x00000002
#define DEP_GRAPH_COMPUTE_DEP_PER_ARG			0x00000004

typedef struct _ORDINATE
{
#if defined(UF_TESTBENCH)
	IMG_CHAR					aszLable[1024];
#endif
	IMG_PVOID					pvData;
	struct _INST*				psInstruction;
	struct _ORDINATE*			psNext;
}ORDINATE, *PORDINATE;
#endif /* defined(TRACK_REDUNDANT_PCONVERSION) */

typedef struct _INST
{
	IOPCODE				eOpcode;

	/* Instruction flags */
	IMG_UINT32 			auFlag[UINTS_TO_SPAN_BITS(INST_FLAGS_NUM)];

	/* Destination arguments. */
	PARG				asDest;
	PARGUMENT_USEDEF	asDestUseDef;

	/*
		Sources for partially/conditional written destinations. For each channel in a destination 
		which isn't written with the result of the instruction the corresponding channel from the 
		old destination is copied into it instead.
	*/
	PARG*				apsOldDest;
	PARGUMENT_USEDEF*	apsOldDestUseDef;

	/* Source arguments. */
	IMG_UINT32			uArgumentCount;
	PARG				asArg;
	PARGUMENT_USEDEF	asArgUseDef;

	/* Predicate to control update of the destination(s). */
	PARG*				apsPredSrc;
	PUSEDEF*			apsPredSrcUseDef;
	IMG_UINT32			uPredCount;

	/* Count of registers to reserve for expansion of the instruction after register allocation. */
	IMG_UINT32			uTempCount;
	/* Details of the registers reserved for use when expanding the instruction after register allocation. */
	PSHORT_REG			asTemp;

	IMG_UINT32			uRepeat;    // Repeat count
	IMG_UINT32			uMask;      // Repeat mask

	IMG_UINT32			uDestCount; // Number of destinations
	IMG_PUINT32			auDestMask;  // Destination mask
	IMG_PUINT32			auLiveChansInDest;
	#if defined(INITIALISE_REGS_ON_FIRST_WRITE)
	IMG_UINT32			uInitialisedChansInDest;
	#endif /* defined(INITIALISE_REGS_ON_FIRST_WRITE) */

	#ifdef SRC_DEBUG
	IMG_UINT32			uAssociatedSrcLine;
	#endif /* SRC_DEBUG */
	
	/*
	 * Data specific to particular opcodes
	 */
	union
	{
		IMG_PVOID				pvNULL;
		PFLOAT_PARAMS			psFloat;
		PEFO_PARAMETERS			psEfo;
		PSOPWM_PARAMS			psSopWm;
		PSOP2_PARAMS			psSop2;
		PSOP3_PARAMS			psSop3;
		PLRP1_PARAMS			psLrp1;
		PFPMA_PARAMS			psFpma;
		PIMA32_PARAMS			psIma32;
		PSMP_PARAMS				psSmp;
#if defined(OUTPUT_USPBIN)
		PSMPUNPACK_PARAMS		psSmpUnpack;
		PTEXWRITE_PARAMS		psTexWrite;
#endif /* defined(OUTPUT_USPBIN) */
		PDOT34_PARAMS			psDot34;
		PDPC_PARAMS				psDpc;
		PLDST_PARAMS			psLdSt;
		#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
		PNRM_PARAMS				psNrm;
		#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */
		PLOADCONST_PARAMS		psLoadConst;
		PLOADMEMCONST_PARAMS	psLoadMemConst;
		PIMA16_PARAMS			psIma16;
		PIMAE_PARAMS			psImae;
		PCALL_PARAMS			psCall;
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		PVEC_PARAMS				psVec;
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		#if defined(SUPPORT_SGX545)
		PDUAL_PARAMS			psDual;
		#endif /* defined(SUPPORT_SGX545) */
		EMIT_PARAMS				sEmit;
		PTEST_PARAMS			psTest;
		PMOVC_PARAMS			psMovc;
		PFARITH16_PARAMS		psArith16;
		PMEMSPILL_PARAMS		psMemSpill;
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		PIDXSC_PARAMS			psIdxsc;
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		PCVTFLT2INT_PARAMS		psCvtFlt2Int;
		PPCK_PARAMS				psPck;
		PLDSTARR_PARAMS			psLdStArray;
		PFDOTPRODUCT_PARAMS		psFdp;
		PDELTA_PARAMS			psDelta;
		PMOVP_PARAMS			psMovp;
		PBITWISE_PARAMS			psBitwise;
		PFEEDBACKDRIVEREPILOG_PARAMS	psFeedbackDriverEpilog;
	} u;

	/*
	 * Data specific to compiler passes
	 */
	union
	{
		/*
			Per-instruction data used during EFO generation.
		*/
		PEFO_STAGEDATA					psEfoData;

		#if defined(SUPPORT_SGX545)
		/*
			Per-instruction data used for SGX545 dual-issue.
		*/
		struct _DUALISSUE_STAGEDATA*	psDIData;
		#endif /* defiend(SUPPORT_SGX545) */

		/*
			Per-instruction data used in the merge identical instructions step.
		*/
		struct
		{
			/* TRUE if the instruction is currently in the merge list. */
			IMG_BOOL						bInserted;
		}								sMergeData;

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		/*
			Per-instruction data used for vector dual-issue.
		*/
		struct _VEC_DUALISSUE*			psVDIData;
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

		/*	
			General per-instruction data.
		*/
		IMG_PVOID						pvData;
	} sStageData;

	/* 
	   Uses of uId
	   -----------

	   - Dependency graphs: uId is the index for this instruction into
	     the array DGRAPH_STATE.psInstruction of the dependency
	     graph. Undefined if uId == (IMG_UINT32)-1.

       - Register allocators (JIT and C10): uId is the index of the
         instruction in the program used to identify the start and end
         of liveness intervals.  Note that the last instruction in the
         program has index 1 and indices are >0.
	*/
	IMG_UINT32		uId;

	/*
		Unique ID for this instruction: incremented each time an instruction
		is created.
	*/
	IMG_UINT32		uGlobalId;

	/*
		In dgraph.c: Entry in the list of available instructions.
	*/
	USC_LIST_ENTRY	sAvailableListEntry;
	
	/*
		Entry in a temporary list.
	*/
	USC_LIST_ENTRY	sTempListEntry;

	/*
		Entry in the list of all intermediate instructions with the same opcode.
	*/
	USC_LIST_ENTRY	sOpcodeListEntry;

	struct _INST*	psGroupNext;
	IMG_UINT32		uGroupChildIndex;
	struct _INST*	psGroupParent;

	/* Instruction repeat groups */
	PREPEAT_GROUP	psRepeatGroup;

	/* SGX545 Increments */
	/* Source operand increments */
	IMG_UINT32		puSrcInc[UINTS_TO_SPAN_BITS(USC_MAX_MOE_OPERANDS - 1)];

	/*
		List of outstanding weak references to the instruction. Type is 
		WEAK_INST_LIST_ENTRY.sInstWeakRefListEntry.
	*/
	USC_LIST		sWeakRefList;

	#if defined(OUTPUT_USPBIN)
	/*
		Bitmask indicating which HW-operands reference a register thats part
		of the overall result of a shader (PS only)
	*/
	IMG_UINT32		uShaderResultHWOperands;
	#endif /* #if defined(OUTPUT_USPBIN) */

	struct _INST*	psPrev;
	struct _INST*	psNext;

	/*
		If the instruction is inserted into a flow control block then points to that
		block. Otherwise NULL.
	*/
	struct _CODEBLOCK_*	psBlock;

	/*
		If this instruction is currently inserted into a block then the index of the
		instruction's location within the block.
	*/
	IMG_UINT32		uBlockIndex;
	
#if defined(TRACK_REDUNDANT_PCONVERSION)
	/*
		DAG data
	*/
	IMG_UINT32			uGraphFlags;
	PORDINATE			psImmediateSuperordinates;
	PORDINATE			psImmediateSubordinates;
	
	/* Cached results for pattern search */
	const IMG_VOID *			psLastFailedRule;
	const IMG_VOID *			psLastPassedRule;
	
	/* Cached matching instruction used by CONCAT op */
	struct _INST*				psLastMatchInst;
#endif /* defined(TRACK_REDUNDANT_PCONVERSION) */
} INST;

typedef struct _REGISTER_GROUP_DESC
{
	IMG_UINT32		uStart;
	IMG_UINT32		uCount;
	HWREG_ALIGNMENT	eAlign;
} REGISTER_GROUP_DESC, *PREGISTER_GROUP_DESC;

typedef struct _REGISTER_GROUPS_DESC
{
	IMG_UINT32				uCount;
	REGISTER_GROUP_DESC		asGroups[USC_MAXIMUM_REGISTER_GROUP_COUNT];
} REGISTER_GROUPS_DESC, *PREGISTER_GROUPS_DESC;

/* Prototypes for functions in inst.c */
IMG_VOID InsertInst(struct tagINTERMEDIATE_STATE* psState, 
					struct _CODEBLOCK* psCodeBlock, 
					PINST psInstToInsert);
IMG_BOOL CanUseSrc(struct tagINTERMEDIATE_STATE* psState, const INST *psInst, IMG_UINT32 uArg, IMG_UINT32 uType, IMG_UINT32 uIndex);
IMG_BOOL CanUseDest(struct tagINTERMEDIATE_STATE* psState, const INST *psInst, IMG_UINT32 uDestIdx, IMG_UINT32 uDestType, IMG_UINT32 uDestIndex);
IMG_VOID FreeInst(struct tagINTERMEDIATE_STATE* psState, PINST psInst);
void InitInstArg(PARG psArg);
IMG_VOID MakeArgumentSet(PARG asSet, IMG_UINT32 uSetCount, IMG_UINT32 uRegType, IMG_UINT32 uRegBaseNum, UF_REGFORMAT eRegFmt);
IMG_VOID SetOpcode(struct tagINTERMEDIATE_STATE* psState, PINST psInst, IOPCODE eOpcode);
IMG_UINT32 GetNonMOEInternalRegisterSources(const INST *psInst);
IMG_UINT32 GetNonMOEInternalRegisterDestinations(const INST *psInst);

IMG_BOOL IsArgInRegisterGroup(struct tagINTERMEDIATE_STATE*		psState,
							  const INST						*psInst,
							  IMG_UINT32						uArg);
							  
IMG_BOOL IsValidImmediateForLocalAddressing(struct tagINTERMEDIATE_STATE*	psState,
											IOPCODE							eOpcode,
											IMG_UINT32						uImmediate,
											IMG_PUINT32						puPackedImmediate);
IMG_BOOL IsValidImmediateForAbsoluteAddressing(PINTERMEDIATE_STATE	psState, 
											   PCINST				psInst,
											   IMG_UINT32			uImmediate, 
											   IMG_PUINT32			puPackedImmediate);
							  
typedef IMG_VOID (*PPROCESS_GROUP_CB)(struct tagINTERMEDIATE_STATE*	psState,
									  PINST							psInst,
									  IMG_BOOL						bDest,
									  IMG_UINT32					uGroupStart,
									  IMG_UINT32					uGroupCount,
									  HWREG_ALIGNMENT				eGroupAlign,
									  IMG_PVOID						pvContext);

IMG_VOID GetDestRegisterGroups(struct tagINTERMEDIATE_STATE*	psState,
							   const INST						*psInst,
							   PREGISTER_GROUPS_DESC			psGroups);
							   
IMG_VOID GetSourceRegisterGroups(struct tagINTERMEDIATE_STATE*	psState,
							     const INST						*psInst,
							     PREGISTER_GROUPS_DESC			psGroups);
							     
IMG_VOID ProcessSourceRegisterGroups(struct tagINTERMEDIATE_STATE*	psState,
									 PINST							psInst,
									 PPROCESS_GROUP_CB				pfCB,
									 IMG_PVOID						pvContext);
									 
IMG_VOID ProcessDestRegisterGroups(struct tagINTERMEDIATE_STATE*	psState,
								   PINST							psInst,
								   PPROCESS_GROUP_CB				pfCB,
								   IMG_PVOID						pvContext);

IMG_BOOL GetRegisterGroupByArg(struct tagINTERMEDIATE_STATE*	psState,
							   PINST							psInst, 
							   IMG_BOOL							bDest, 
							   IMG_UINT32						uArgIdx, 
							   PREGISTER_GROUP_DESC				psFoundGroup);
								   
IMG_UINT32 GetChannelsWrittenInRegister(const INST		*psInst,
									    IMG_UINT32		uRegisterType,
										IMG_UINT32		uRegisterNumber,
										IMG_PUINT32*	ppuLiveChansInDest,
										IMG_PUINT32		puDestOffset);
										
IMG_UINT32 GetChannelsWrittenInArg(const INST		*psInst,
								   PARG				psArg,
								   IMG_PUINT32*		ppuLiveChansInDest);
								   
PINST AllocateInst(struct tagINTERMEDIATE_STATE* psState, PINST psSrcLineInst);

IMG_UINT32 ApplyIMAEComponentSelect(struct tagINTERMEDIATE_STATE* psState,
									const INST			*psInst,
									IMG_UINT32			uArgIdx,
									IMG_UINT32			uComponent,
									IMG_UINT32			uImmediate);

/*
	Duplicate an instruction
*/
PINST CopyInst(struct tagINTERMEDIATE_STATE* psState, PINST psSrcInst);

IMG_VOID RemoveInst(struct tagINTERMEDIATE_STATE* psState, struct _CODEBLOCK_* psCodeBlock, PINST psInstToRemove);
IMG_VOID SetAsGroupChild(PINTERMEDIATE_STATE psState, PINST psGroupParent, PINST psLastGroupChild, PINST psNewGroupChild);
IMG_VOID InsertInstBefore(struct tagINTERMEDIATE_STATE* psState, 
						  struct _CODEBLOCK_* psCodeBlock, 
						  PINST psInstToInsert, 
						  PINST psInstToInsertBefore);
								  
IMG_VOID InsertInstAfter(struct tagINTERMEDIATE_STATE* psState, 
						 struct _CODEBLOCK_* psBlock, 
						 PINST psInstToInsert, 
						 PINST psInstToInsertAfter);
						 
extern const INST_DESC g_psInstDesc[];
PINST FormMove(struct tagINTERMEDIATE_STATE* psState,
			   PARG psDst,
			   PARG psSrc,
			   PINST psSrcLine);
IMG_VOID MakeSOPWMMove(PINTERMEDIATE_STATE psState, PINST psMOVInst);
			   
IMG_BOOL IsMove(PINST psInst, PARG *ppsDst, PARG *ppsSrc);
IMG_VOID ConvertRegister(struct tagINTERMEDIATE_STATE* psState, PARG psReg);
IMG_BOOL CanHaveSourceModifier(struct tagINTERMEDIATE_STATE* psState, const INST *psInst, IMG_UINT32 uArg, IMG_BOOL bNegate, IMG_BOOL bAbs);
IMG_BOOL HasF16F32SelectInst(const INST *psInst);
IMG_BOOL HasC10FmtControl(const INST *psInst);
IMG_BOOL RequiresGradients(const INST *psInst);
IMG_BOOL WroteAllLiveChannels(const INST *			psInst,
							  IMG_UINT32			uDestIdx);
							  
IMG_UINT32 GetUSPPerSampleTemporaryCount(struct tagINTERMEDIATE_STATE*	psState,
										 IMG_UINT32						uSamplerIdx, 
										 const INST						*psInst);
										 
IMG_UINT32 GetUSPPerSampleUnpackTemporaryCount(void);

IMG_UINT32 GetUSPTextureWriteTemporaryCount(void);

IMG_BOOL EqualArgs(const ARG *psArgA, const ARG *psArgB);
IMG_INT32 CompareArgs(const ARG *psArgA, const ARG *psArgB);
IMG_BOOL SameRegister(const ARG *psArgA, const ARG *psArgB);
IMG_BOOL IsDeschedulingPoint(struct tagINTERMEDIATE_STATE* psState, const INST *psInst);
IMG_BOOL IsDeschedBeforeInst(struct tagINTERMEDIATE_STATE* psState, const INST *psInst);
IMG_BOOL IsDeschedAfterInst(const INST *psInst);
IMG_BOOL CanUseSource0(struct tagINTERMEDIATE_STATE* psState, struct _FUNC_* psOwner, const ARG *psArg);
IMG_BOOL EqualInstructionModes(struct tagINTERMEDIATE_STATE* psState, const INST *psInst1, const INST *psInst2);
IMG_BOOL EqualInstructionParameters(struct tagINTERMEDIATE_STATE* psState, const INST *psInst1, const INST *psInst2);

IMG_INT32 CompareInstructionParameters(struct tagINTERMEDIATE_STATE*	psState,
									   const INST						*psInst1,
									   const INST						*psInst2);
									   
IMG_INT32 CompareInstructionArgumentParameters(struct tagINTERMEDIATE_STATE*	psState,
											   const INST						*psInst1,
											   IMG_UINT32						uInst1ArgIdx,
											   const INST						*psInst2,
											   IMG_UINT32						uInst2ArgIdx);
											   
IMG_BOOL CanRepeatInst(struct tagINTERMEDIATE_STATE* psState, const INST *psInst);

#if defined(SUPPORT_SGX545)
IMG_BOOL SupportsPerInstMoeIncrements(struct tagINTERMEDIATE_STATE*	psState, const INST	*psInst);
IMG_VOID GetSmpBackendSrcSet(struct tagINTERMEDIATE_STATE* psState, const INST	*psInst, IMG_PUINT32	puSet);
#endif /* defined(SUPPORT_SGX545) */

IMG_UINT32 GetMaxRepeatCountInst(struct tagINTERMEDIATE_STATE* psState, const INST *psInst);
IMG_BOOL IsInstAffectedByBRN21752(const INST *psInst);

IMG_UINT32 GetMinimalDestinationMask(struct tagINTERMEDIATE_STATE*	psState,
									 const INST						*psInst,
									 IMG_UINT32						uDestIdx,
									 IMG_UINT32						uRealDestMask);
									 
IMG_VOID ModifyOpcode(struct tagINTERMEDIATE_STATE*	psState,
					  PINST							psInst,
					  IOPCODE						eNewOpcode);
					  
IMG_VOID CommuteSrc01(struct tagINTERMEDIATE_STATE* psState, PINST	psInst);
IMG_BOOL CanUseRepeatMask(struct tagINTERMEDIATE_STATE* psState, const INST *psInst);
INST_PRED GetInstPredicateSupport(PINTERMEDIATE_STATE psState, PINST psInst);
IMG_BOOL UseRepeatMaskOnly(PINTERMEDIATE_STATE psState, PCINST psInst);
IMG_BOOL IsSrc12StandardBanks(IMG_UINT32 uType, IMG_UINT32 uIndex);
IMG_BOOL EqualPredicates(const INST *psInst1, const INST *psInst2);
IMG_INT32 ComparePredicates(const INST *psInst1, const INST *psInst2);
IMG_BOOL IsMaskableRegisterType(IMG_UINT32 uType);
IMG_BOOL IsSrc0StandardBanks(IMG_UINT32 uType, IMG_UINT32 uIndexType);

IMG_VOID SetDestCount(struct tagINTERMEDIATE_STATE* psState,
					  PINST							psInst,
					  IMG_UINT32					uNewDestCount);
					  
IMG_VOID SetOpcodeAndDestCount(struct tagINTERMEDIATE_STATE*	psState, 
							   PINST							psInst, 
							   IOPCODE							eOpcode, 
							   IMG_UINT32						uDestCount);

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
IMG_BOOL GetMinimumVDUALDestinationMask(struct tagINTERMEDIATE_STATE*	psState,
									    PCINST							psInst,
										IMG_UINT32						uDestSlot,
										PCARG							psDest,
										IMG_UINT32						uRealDestMask);
IMG_BOOL IsVDUALUnifiedStoreVectorResult(PCINST	psInst);									
IMG_BOOL IsVDUALUnifiedStoreVectorSource(PINTERMEDIATE_STATE psState, PCINST psInst);
IMG_UINT32 GetVDUALGPIDestinationMask(PCINST psInst);
IMG_UINT32 GetVDUALUnifiedStoreDestinationMask(PINTERMEDIATE_STATE	psState, 
											   PCINST				psInst, 
											   IMG_UINT32			uRealDestMask,
											   IMG_BOOL				bIsGPI);
IMG_INT32 CompareInstNonSourceParametersTypeVEC(PCINST psInst1, PCINST psInst2);
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

IMG_BOOL EqualFloatSrcs(struct tagINTERMEDIATE_STATE*	psState,
						const INST						*psInst1, 
					    IMG_UINT32						uInst1SrcIdx, 
						const INST						*psInst2, 
						IMG_UINT32						uInst2SrcIdx);
						
IMG_BOOL EqualFloatSrcsIgnoreNegate(struct tagINTERMEDIATE_STATE*	psState,
								    const INST						*psInst1, 
									IMG_UINT32						uInst1SrcIdx, 
									const INST						*psInst2, 
									IMG_UINT32						uInst2SrcIdx,
									IMG_PBOOL						pbDifferentNegate);
									
IMG_BOOL EqualFloatModifier(struct tagINTERMEDIATE_STATE*	psState,
							const INST						*psInst1, 
							IMG_UINT32						uInst1SrcIdx, 
							const INST						*psInst2, 
							IMG_UINT32						uInst2SrcIdx);
							
IMG_BOOL IsNegated(struct tagINTERMEDIATE_STATE* psState, const INST *psInst, IMG_UINT32 uArgIdx);
IMG_BOOL IsAbsolute(struct tagINTERMEDIATE_STATE* psState, const INST *psInst, IMG_UINT32 uArgIdx);
IMG_BOOL HasNegateOrAbsoluteModifier(struct tagINTERMEDIATE_STATE* psState, const INST *psInst, IMG_UINT32 uArgIdx);
IMG_VOID GetCombinedSourceModifiers(PFLOAT_SOURCE_MODIFIER psMod1, PFLOAT_SOURCE_MODIFIER psMod2);
PFLOAT_SOURCE_MODIFIER GetFloatMod(struct tagINTERMEDIATE_STATE* psState, const INST *psInst, IMG_UINT32 uArgIdx);
IMG_VOID SwapInstSources(struct tagINTERMEDIATE_STATE* psState, PINST psInst, IMG_UINT32 uSrc1Idx, IMG_UINT32 uSrc2Idx);
IMG_VOID ExchangeInstSources(PINTERMEDIATE_STATE	psState, 
							 PINST					psInst1, 
							 IMG_UINT32				uSrc1Idx, 
							 PINST					psInst2, 
							 IMG_UINT32				uSrc2Idx);

IMG_VOID MoveFloatSrc(struct tagINTERMEDIATE_STATE*	psState, 
					  PINST							psDestInst, 
					  IMG_UINT32					uDestArgIdx, 
					  PINST							psSrcInst, 
					  IMG_UINT32					uSrcArgIdx);
					  
IMG_VOID CombineInstSourceModifiers(struct tagINTERMEDIATE_STATE* psState, 
									PINST psInst, 
									IMG_UINT32 uArg, 
									PFLOAT_SOURCE_MODIFIER psNewSrcMod);
									
IMG_BOOL EqualInstructionNonArgumentParameters(struct tagINTERMEDIATE_STATE*	psState,
											   const INST						*psInst1,
											   const INST						*psInst2);
											   
IMG_BOOL EqualInstructionArgumentParameters(struct tagINTERMEDIATE_STATE*		psState,
											const INST							*psInst1,
											IMG_UINT32							uInst1ArgIdx,
											const INST							*psInst2,
											IMG_UINT32							uInst2ArgIdx);
											
IMG_BOOL EqualDestinationParameters(const INST							*psInst1,
									const INST							*psInst2);
									
IMG_VOID InvertNegateModifier(struct tagINTERMEDIATE_STATE* psState, PINST psInst, IMG_UINT32 uArgIdx);
IMG_UINT32 GetComponentSelect(struct tagINTERMEDIATE_STATE* psState, const INST *psInst, IMG_UINT32 uArgIdx);
IMG_VOID SetComponentSelect(struct tagINTERMEDIATE_STATE* psState, PINST psInst, IMG_UINT32 uArgIdx, IMG_UINT32 uComponent);
IMG_VOID InitPerArgumentParameters(struct tagINTERMEDIATE_STATE* psState, PINST psInst, IMG_UINT32 uArgIdx);

IMG_UINT32 GetMOEUnitsLog2(struct tagINTERMEDIATE_STATE*	psState,
						   PCINST							psInst,
						   IMG_BOOL							bDest,
						   IMG_UINT32						uArgIdx);

IMG_UINT32 GetIndexUnitsLog2(struct tagINTERMEDIATE_STATE*	psState,
							 PCINST							psInst,
							 IMG_BOOL						bDest,
							 IMG_UINT32						uArgIdx);

IMG_BOOL IsImmediateSourceValid(struct tagINTERMEDIATE_STATE*	psState,
								PCINST							psInst,
								IMG_UINT32						uArgIdx,
								IMG_UINT32						uComponentSelect,
								IMG_UINT32						uImmediate);

IMG_BOOL SupportsEndFlag(struct tagINTERMEDIATE_STATE* psState, const INST *psInst);
IMG_VOID SwapPCKSources(struct tagINTERMEDIATE_STATE* psState, PINST psPCKInst);
IMG_VOID SetPCKComponent(struct tagINTERMEDIATE_STATE* psState, PINST psPCKInst, IMG_UINT32 uArgIdx, IMG_UINT32 uComponent);
IMG_UINT32 GetPCKComponent(struct tagINTERMEDIATE_STATE* psState, const INST *psPCKInst, IMG_UINT32 uArgIdx);
IMG_BOOL EqualPCKArgs(struct tagINTERMEDIATE_STATE* psState, const INST *psInst);
IMG_VOID InitFloatSrcMod(PFLOAT_SOURCE_MODIFIER psMod);

IMG_VOID MovePackSource(struct tagINTERMEDIATE_STATE*	psState, 
					    PINST							psDestInst,
						IMG_UINT32						uDestArgIdx,
						PINST							psSrcInst,
						IMG_UINT32						uSrcArgIdx);
						
IMG_VOID MoveSrcAndModifiers(struct tagINTERMEDIATE_STATE*	psState, 
							 PINST							psDestInst, 
							 IMG_UINT32						uDestArgIdx, 
							 PINST							psSrcInst, 
							 IMG_UINT32						uSrcArgIdx);
							 
IMG_VOID CopySrcAndModifiers(struct tagINTERMEDIATE_STATE*	psState, 
							 PINST							psDestInst, 
							 IMG_UINT32						uDestArgIdx, 
							 PINST							psSrcInst, 
							 IMG_UINT32						uSrcArgIdx);
							 
IMG_BOOL NoPredicate(struct tagINTERMEDIATE_STATE* psState, const INST *psInst);
IMG_VOID CopyPredicate(struct tagINTERMEDIATE_STATE* psState, PINST psDestInst, const INST *psSrcInst);
IMG_VOID SetPredicate(struct tagINTERMEDIATE_STATE* psState, PINST psInst, IMG_UINT32 uPredRegNum, IMG_BOOL bPredNegate);
IMG_VOID MakePredicatePerChan(PINTERMEDIATE_STATE psState, PINST psInst);
IMG_VOID SetPredicateAtIndex(PINTERMEDIATE_STATE	psState,
							 PINST					psInst,
							 IMG_UINT32				uPredRegNum, 
							 IMG_BOOL				bPredNegate,
							 IMG_UINT32				uIndex);
IMG_VOID GetPredicate(const INST					*psInst, 
					  IMG_PUINT32					puPredRegNum, 
					  IMG_PBOOL						pbPredNegate, 
					  IMG_PBOOL						pbPredPerChan, 
					  IMG_UINT32					uIndex);
					  
IMG_VOID ClearPredicate(struct tagINTERMEDIATE_STATE* psState, PINST psInst, IMG_UINT32 uIndex);
IMG_VOID ClearPredicates(struct tagINTERMEDIATE_STATE* psState, PINST psInst);
IMG_BOOL InstUsesF16FmtControl(const INST *psInst);

IMG_VOID CopyPartiallyWrittenDest(struct tagINTERMEDIATE_STATE*	psState,
								  PINST							psCopyToInst,
								  IMG_UINT32					uCopyToDestIdx,
								  PINST							psCopyFromInst,
								  IMG_UINT32					uCopyFromDestIdx);
IMG_VOID MovePartiallyWrittenDest(PINTERMEDIATE_STATE	psState,
								  PINST					psMoveToInst,
								  IMG_UINT32			uMoveToDestIdx,
								  PINST					psMoveFromInst,
								  IMG_UINT32			uMoveFromDestIdx);
								  
IMG_VOID SetPartiallyWrittenDest(struct tagINTERMEDIATE_STATE*	psState,
								 PINST							psInst,
								 IMG_UINT32						uDestIdx,
								 PCARG							psPartialDest);
								 
IMG_VOID SetArgumentCount(struct tagINTERMEDIATE_STATE*	psState,
						  PINST							psInst,
						  IMG_UINT32					uArgCount);
						  
IMG_VOID MoveDest(struct tagINTERMEDIATE_STATE*	psState, 
				  PINST							psCopyToInst, 
				  IMG_UINT32					uCopyToIdx, 
				  PINST							psCopyFromInst, 
				  IMG_UINT32					uCopyFromIdx);
				  
IMG_VOID MoveSrc(struct tagINTERMEDIATE_STATE*	psState, 
				 PINST							psCopyToInst, 
				 IMG_UINT32						uCopyToIdx, 
				 PINST							psCopyFromInst, 
				 IMG_UINT32						uCopyFromIdx);
				 
IMG_VOID CopySrc(struct tagINTERMEDIATE_STATE*	psState, 
				 PINST							psCopyToInst, 
				 IMG_UINT32						uCopyToIdx, 
				 PINST							psCopyFromInst, 
				 IMG_UINT32						uCopyFromIdx);
				 
IMG_VOID SetSrc(struct tagINTERMEDIATE_STATE*	psState,
				PINST							psInst,
				IMG_UINT32						uSrcIdx,
				IMG_UINT32						uNewSrcType,
				IMG_UINT32						uNewSrcNumber,
				UF_REGFORMAT					eNewSrcFormat);
				
IMG_VOID SetSrcIndex(struct tagINTERMEDIATE_STATE*	psState,
					 PINST							psInst,
					 IMG_UINT32						uSrcIdx,
					 IMG_UINT32						uNewIndexType,
					 IMG_UINT32						uNewIndexNumber,
					 IMG_UINT32						uNewIndexArrayOffset,
					 IMG_UINT32						uNewIndexRelativeStrideInBytes);
					 
IMG_VOID SetDestIndex(struct tagINTERMEDIATE_STATE*	psState,
					  PINST							psInst,
					  IMG_UINT32					uSrcIdx,
					  IMG_UINT32					uNewIndexType,
					  IMG_UINT32					uNewIndexNumber,
					  IMG_UINT32					uNewIndexArrayOffset,
					  IMG_UINT32					uNewIndexRelativeStrideInBytes);
					  
IMG_VOID SetPartialDestIndex(struct tagINTERMEDIATE_STATE*	psState,
							 PINST							psInst,
							 IMG_UINT32						uSrcIdx,
							 IMG_UINT32						uNewIndexType,
							 IMG_UINT32						uNewIndexNumber,
							 IMG_UINT32						uNewIndexArrayOffset,
							 IMG_UINT32						uNewIndexRelativeStrideInBytes);
							 
IMG_VOID SetArraySrc(struct tagINTERMEDIATE_STATE*	psState,
					 PINST							psInst,
					 IMG_UINT32						uSrcIdx,
					 IMG_UINT32						uNewSrcNumber,
					 IMG_UINT32						uNewSrcArrayOffset,
					 UF_REGFORMAT					eNewSrcFormat);
					 
IMG_VOID SetDest(struct tagINTERMEDIATE_STATE*	psState,
				 PINST							psInst,
				 IMG_UINT32						uDestIdx,
				 IMG_UINT32						uNewDestType,
				 IMG_UINT32						uNewDestNumber,
				 UF_REGFORMAT					eNewDestFormat);
				 
IMG_VOID SetPartialDest(struct tagINTERMEDIATE_STATE*	psState,
						PINST							psInst,
						IMG_UINT32						uDestIdx,
						IMG_UINT32						uNewDestType,
						IMG_UINT32						uNewDestNumber,
						UF_REGFORMAT					eNewDestFormat);
						
IMG_VOID MovePartialDestToSrc(struct tagINTERMEDIATE_STATE*	psState, 
							  PINST							psCopyToInst, 
							  IMG_UINT32					uCopyToIdx, 
							  PINST							psCopyFromInst, 
							  IMG_UINT32					uCopyFromIdx);
							  
IMG_VOID SetDestFromArg(struct tagINTERMEDIATE_STATE* psState, PINST psInst, IMG_UINT32 uDestIdx, PCARG psNewDest);
IMG_VOID SetSrcFromArg(struct tagINTERMEDIATE_STATE* psState, PINST psInst, IMG_UINT32 uSrcIdx, PCARG psNewSrc);
IMG_VOID SetSrcUnused(struct tagINTERMEDIATE_STATE*	psState, PINST psInst, IMG_UINT32 uSrcIdx);
IMG_VOID SetDestUnused(struct tagINTERMEDIATE_STATE* psState, PINST psInst, IMG_UINT32 uDestIdx);

IMG_VOID SetUseFromArg(struct tagINTERMEDIATE_STATE*	psState,
					   struct _USEDEF*					psUseToReplace,
					   PARG								psReplacement);
					   
IMG_VOID SetConditionalBlockPredicate(struct tagINTERMEDIATE_STATE*	psState,
									  struct _CODEBLOCK_*			psBlock,
									  IMG_UINT32					uNewPredicate);
									  
IMG_BOOL ReferencedOutsideBlock(struct tagINTERMEDIATE_STATE* psState, struct _CODEBLOCK_* psBlock, PARG psDest);
IMG_VOID CopyPartiallyOverwrittenDataInBlock(PINTERMEDIATE_STATE	psState, 
											 PCODEBLOCK				psInsertBlock, 
											 PINST					psInst,
											 PINST					psInsertBeforeInst);
IMG_VOID CopyPartiallyOverwrittenData(struct tagINTERMEDIATE_STATE* psState, PINST psInst);
IMG_VOID SetArrayArgument(struct tagINTERMEDIATE_STATE*	psState,
						  PINST							psInst,
						  IMG_BOOL						bDest,
						  IMG_UINT32					uArgIdx,
						  IMG_UINT32					uNewSrcNumber,
						  IMG_UINT32					uNewSrcArrayOffset,
						  UF_REGFORMAT					eNewSrcFormat);
						  
IMG_VOID SetArgumentIndex(struct tagINTERMEDIATE_STATE*	psState,
						  PINST							psInst,
						  IMG_BOOL						bDest,
						  IMG_UINT32					uArgIdx,
						  IMG_UINT32					uNewIndexType,
						  IMG_UINT32					uNewIndexNumber,
						  IMG_UINT32					uNewIndexArrayOffset,
						  IMG_UINT32					uNewIndexRelativeStrideInBytes);
						  
IMG_VOID SetArgument(struct tagINTERMEDIATE_STATE*	psState,
					 PINST							psInst,
					 IMG_BOOL						bDest,
					 IMG_UINT32						uArgIdx,
					 IMG_UINT32						uNewArgType,
					 IMG_UINT32						uNewArgNumber,
					 UF_REGFORMAT					eNewArgFmt);
					 
IMG_VOID MoveArgument(struct tagINTERMEDIATE_STATE*	psState, 
					  IMG_BOOL						bDest,
					  PINST							psMoveToInst, 
					  IMG_UINT32					uMoveToIdx, 
					  PINST							psMoveFromInst, 
					  IMG_UINT32					uMoveFromIdx);
					  
IMG_VOID SetArgNoIndex(struct tagINTERMEDIATE_STATE*	psState,
					   struct _USEDEF*					psUse);
IMG_BOOL InstSource01Commute(struct tagINTERMEDIATE_STATE* psState, PINST psInst);
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
IMG_BOOL InstHasVectorDest(struct tagINTERMEDIATE_STATE* psState, const INST *psInst, IMG_UINT32 uDestIdx);
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
IMG_BOOL BypassCacheForModifiableShaderMemory(struct tagINTERMEDIATE_STATE* psState);
IMG_INT32 BaseCompareInstParametersTypeLOADMEMCONST(PCINST psInst1, PCINST psInst2);
IMG_INT32 CompareInstParametersTypeTEST(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2);
IMG_INT32 CompareInstParametersTypeMOVC(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2);
IMG_INT32 CompareInstParametersTypeLOADMEMCONST(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2);
IMG_BOOL IsUniformSource(PINTERMEDIATE_STATE psState, PARG psArg);
IMG_BOOL IsNonSSAArgument(PINTERMEDIATE_STATE psState, PARG psArg);
IMG_BOOL IsNonSSARegister(PINTERMEDIATE_STATE psState, IMG_UINT32 uType, IMG_UINT32 uNumber);
IMG_BOOL IsInstReferencingNonSSAData(PINTERMEDIATE_STATE psState, PINST psInst);
IMG_BOOL IsValidEFOSource(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArg, IMG_UINT32 uType, IMG_UINT32 uIndexType);

IMG_BOOL RegTypeReferencedInInstSources(PINST psInst, IMG_UINT32 uType);
IMG_BOOL RegTypeReferencedInInstDestination(PINST psInst, IMG_UINT32 uType);

IMG_BOOL IsUniform(PINTERMEDIATE_STATE psState, PUSEDEF_CHAIN psUseDefChain);

FORCE_INLINE IMG_UINT32 TestInstructionGroup(IOPCODE						eOpcode,
											 USC_OPTIMIZATION_GROUP_TYPE	eType)
{
	return (g_psInstDesc[eOpcode].uOptimizationGroup & *((unsigned*)&eType));
}

FORCE_INLINE IMG_VOID MakeVectorPredicateDest(struct tagINTERMEDIATE_STATE*	psState, 
											  PINST							psInst, 
											  IMG_UINT32					uPredNum)
{
	IMG_UINT32 uDest;

	SetDest(psState, psInst, VTEST_PREDICATE_DESTIDX, USEASM_REGTYPE_PREDICATE, uPredNum, UF_REGFORMAT_F32);

	for (uDest = VTEST_PREDICATE_DESTIDX+1; uDest < VTEST_PREDICATE_ONLY_DEST_COUNT; ++uDest)
	{
		SetDestUnused(psState, psInst, uDest);
	}
}

FORCE_INLINE IMG_VOID MakePredicateDest(struct tagINTERMEDIATE_STATE*	psState, 
										PINST							psInst, 
										IMG_UINT32						uDestIdx, 
										IMG_UINT32						uPredNum)
{
	SetDest(psState, psInst, uDestIdx, USEASM_REGTYPE_PREDICATE, uPredNum, UF_REGFORMAT_F32);
}

FORCE_INLINE IMG_VOID MakePredicateSource(struct tagINTERMEDIATE_STATE* psState, 
										  PINST							psInst, 
										  IMG_UINT32					uSrcIdx, 
										  IMG_UINT32					uPredNum)
{
	SetSrc(psState, psInst, uSrcIdx, USEASM_REGTYPE_PREDICATE, uPredNum, UF_REGFORMAT_F32);
}

FORCE_INLINE IMG_VOID MakePredicatePartialDest(struct tagINTERMEDIATE_STATE*	psState, 
											   PINST							psInst, 
											   IMG_UINT32						uDestIdx, 
											   IMG_UINT32						uPredNum)
{
	SetPartialDest(psState, psInst, uDestIdx, USEASM_REGTYPE_PREDICATE, uPredNum, UF_REGFORMAT_F32);
}

FORCE_INLINE IMG_VOID MakePredicateArgument(PARG psArg, IMG_UINT32 uPredNum)
{
	InitInstArg(psArg);
	psArg->uType = USEASM_REGTYPE_PREDICATE;
	psArg->uNumber = uPredNum;
}

FORCE_INLINE 
IMG_UINT32 GetDestMaskIdx(const INST *psInst, IMG_UINT32 uDestIdx)
/*****************************************************************************
 FUNCTION     : GetDestMaskIdx
    
 PURPOSE      : Get the destination mask of an instruction

 PARAMETERS   : psInst     - Instruction to test.
			    uDestIdx   - Index of the destination to get the mask for.

 RETURNS      : The destination mask.
*****************************************************************************/
{
	return psInst->auDestMask[uDestIdx];
}

FORCE_INLINE 
IMG_VOID GetDestMask(const INST *psInst, IMG_PUINT32 puDestMask)
/*****************************************************************************
 FUNCTION     : GetDestMask
    
 PURPOSE      : Get the destination mask of an instruction

 PARAMETERS   : psInst     - Instruction to test

 OUTPUT       : uDestMask  - Destination mask

 RETURNS      : Nothing
*****************************************************************************/
{
	if (g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_DESTMASKABLE)
		(*puDestMask) = (psInst->auDestMask[0] & USC_DESTMASK_FULL);
	else
		(*puDestMask) = USC_DESTMASK_FULL;
}

IMG_VOID MakeArg(PINTERMEDIATE_STATE psState, IMG_UINT32 uArgType, IMG_UINT32 uArgNumber, UF_REGFORMAT eArgFmt, PARG psArg);

typedef IMG_VOID (*PFN_PROCESS_INSTRUCTION)(PINTERMEDIATE_STATE, PINST);
IMG_VOID ForAllInstructionsOfType(PINTERMEDIATE_STATE		psState, 
								  IOPCODE					eType, 
								  PFN_PROCESS_INSTRUCTION	pfnProcess);

IMG_VOID InstListIteratorInitialize(PINTERMEDIATE_STATE psState, IOPCODE eOpType, PSAFE_LIST_ITERATOR psIterator);
IMG_VOID InstListIteratorInitializeAtEnd(PINTERMEDIATE_STATE psState, IOPCODE eOpType, PSAFE_LIST_ITERATOR psIterator);
FORCE_INLINE
IMG_BOOL InstListIteratorContinue(PSAFE_LIST_ITERATOR psIterator)
{
	return SafeListIteratorContinue(psIterator);
}
FORCE_INLINE
PINST InstListIteratorCurrent(PSAFE_LIST_ITERATOR psIterator)
{
	return IMG_CONTAINING_RECORD(SafeListIteratorCurrent(psIterator), PINST, sOpcodeListEntry);
}
FORCE_INLINE
IMG_VOID InstListIteratorNext(PSAFE_LIST_ITERATOR psIterator)
{
	SafeListIteratorNext(psIterator);
}
FORCE_INLINE
IMG_VOID InstListIteratorPrev(PSAFE_LIST_ITERATOR psIterator)
{
	SafeListIteratorPrev(psIterator);
}
FORCE_INLINE
IMG_VOID InstListIteratorFinalise(PSAFE_LIST_ITERATOR psIterator)
{
	SafeListIteratorFinalise(psIterator);
}

typedef struct _WEAK_INST_LIST
{
	USC_LIST			sList;
} WEAK_INST_LIST, *PWEAK_INST_LIST;

IMG_VOID WeakInstListInitialize(PWEAK_INST_LIST psList);
IMG_VOID WeakInstListAppend(PINTERMEDIATE_STATE psState, PWEAK_INST_LIST psList, PINST psInst);
PINST WeakInstListRemoveHead(PINTERMEDIATE_STATE psState, PWEAK_INST_LIST psList);

IMG_VOID LoadImmediate(PINTERMEDIATE_STATE	psState,
					   PINST				psInst,
					   IMG_UINT32			uArgIdx,
					   IMG_UINT32			uImmValue);
IMG_VOID MakePartialDestCopy(PINTERMEDIATE_STATE	psState, 
							 PINST					psCopyFromInst, 
							 IMG_UINT32				uCopyFromDestIdx,
							 IMG_BOOL				bBefore,
							 PARG					psNewDest);

#endif /* __USC_INST_H */
