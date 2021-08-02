/******************************************************************************
 * Name         : inst.c
 * Title        : Pixel Shader Compiler
 * Author       : John Russell
 * Created      : Jan 2002
 *
 * Copyright    : 2002-2008 by Imagination Technologies Limited.
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
 * $Log: inst.c $
 * 
 *  --- Revision Logs Removed --- 
 *  
 * Increase the cases where a vec4 instruction can be split into two vec2
 * parts.
 *  --- Revision Logs Removed --- 
 * 
 * Fix for MOE setup on SGX543 instructions.
 *  --- Revision Logs Removed --- 
 *****************************************************************************/

#include "uscshrd.h"
#include "bitops.h"
#include "inst.h"
#include "usedef.h"
#include <limits.h>

#if defined(UF_TESTBENCH)
#define IOPCODE_NAME(NAME)		NAME,
#else /* defined(UF_TESTBENCH) */
#define IOPCODE_NAME(NAME)
#endif /* defined(UF_TESTBENCH) */

static const SOURCE_ARGUMENT_PAIR	g_sSrc01 = {0, 1};
static const SOURCE_ARGUMENT_PAIR	g_sSrc02 = {0, 2};

static
IMG_BOOL IsSrc0ExtendedBanks(IMG_UINT32 uType, IMG_UINT32 uIndexType);
static
IMG_BOOL IsSrc12ExtendedBanks(IMG_UINT32 uType);
static
IMG_BOOL CanUseSmpSrc(PINTERMEDIATE_STATE	psState,
					  const INST			*psInst,
					  IMG_UINT32			uArg,
					  IMG_UINT32			uType,
					  IMG_UINT32			uIndexType);
static
IMG_VOID WeakInstListFreeInst(PINTERMEDIATE_STATE psState, PINST psInst);

static
IMG_BOOL IsSrc0Standard(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArg, IMG_UINT32 uType, IMG_UINT32 uIndexType)
/*****************************************************************************
 FUNCTION	: IsSrc0Standard

 PURPOSE	: Check if a hardware register type is supported by hardware source 0
			  without the extended bank flag.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Ignored.
			  uArg			- Ignored.
			  uType			- Hardware register type.
			  uIndexType	- Either USEASM_REGTYPE_INDEX for a dynamically indexed
							source or USC_REGTYPE_NOINDEX otherwise.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst);
	PVR_UNREFERENCED_PARAMETER(uArg);
	return IsSrc0StandardBanks(uType, uIndexType);
}

static
IMG_BOOL IsSrc0Extended(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArg, IMG_UINT32 uType, IMG_UINT32 uIndexType)
/*****************************************************************************
 FUNCTION	: IsSrc0Extended

 PURPOSE	: Check if a hardware register type is supported by hardware source 0
			  with the extended bank flag.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Ignored.
			  uArg			- Ignored.
			  uType			- Hardware register type.
			  uIndexType	- Either USEASM_REGTYPE_INDEX for a dynamically indexed
							source or USC_REGTYPE_NOINDEX otherwise.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst);
	PVR_UNREFERENCED_PARAMETER(uArg);
	return IsSrc0ExtendedBanks(uType, uIndexType);
}

static
IMG_BOOL IsSrc12Standard(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArg, IMG_UINT32 uType, IMG_UINT32 uIndexType)
/*****************************************************************************
 FUNCTION	: IsSrc12Standard

 PURPOSE	: Check if a hardware register type is supported by hardware source 1
			  or source 2 without the extended bank flag.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Ignored.
			  uArg			- Ignored.
			  uType			- Hardware register type.
			  uIndexType	- Either USEASM_REGTYPE_INDEX for a dynamically indexed
							source or USC_REGTYPE_NOINDEX otherwise.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst);
	PVR_UNREFERENCED_PARAMETER(uArg);
	return IsSrc12StandardBanks(uType, uIndexType);
}

static
IMG_BOOL IsSrc12Extended(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArg, IMG_UINT32 uType, IMG_UINT32 uIndexType)
/*****************************************************************************
 FUNCTION	: IsSrc12Extended

 PURPOSE	: Check if a hardware register type is supported by hardware source 1
			  or source 2 with the extended bank flag.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Ignored.
			  uArg			- Ignored.
			  uType			- Hardware register type.
			  uIndexType	- Either USEASM_REGTYPE_INDEX for a dynamically indexed
							source or USC_REGTYPE_NOINDEX otherwise.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst);
	PVR_UNREFERENCED_PARAMETER(uArg);
	PVR_UNREFERENCED_PARAMETER(uIndexType);
	return IsSrc12ExtendedBanks(uType);
}

static
IMG_BOOL IsImmediate(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArg, IMG_UINT32 uType, IMG_UINT32 uIndexType)
/*****************************************************************************
 FUNCTION	: IsImmediate

 PURPOSE	: Check iif a hardware register type is supported in an instruction
			  source which only accepts immediate values.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Ignored.
			  uArg			- Ignored.
			  uType			- Hardware register type.
			  uIndexType	- Either USEASM_REGTYPE_INDEX for a dynamically indexed
							source or USC_REGTYPE_NOINDEX otherwise.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst);
	PVR_UNREFERENCED_PARAMETER(uArg);
	if (uType == USEASM_REGTYPE_IMMEDIATE)
	{
		ASSERT(uIndexType == USC_REGTYPE_NOINDEX);
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

#if defined(EXECPRED)
static
IMG_BOOL IsImmediateOrPredicate(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArg, IMG_UINT32 uType, IMG_UINT32 uIndexType)
/*****************************************************************************
 FUNCTION	: IsImmediateOrPredicate

 PURPOSE	: Check iif a hardware register type is supported in an instruction
			  source which only accepts immediate values or predicate register.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Ignored.
			  uArg			- Ignored.
			  uType			- Hardware register type.
			  uIndexType	- Either USEASM_REGTYPE_INDEX for a dynamically indexed
							source or USC_REGTYPE_NOINDEX otherwise.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst);
	PVR_UNREFERENCED_PARAMETER(uArg);
	if ((uType == USEASM_REGTYPE_IMMEDIATE) || (uType == USEASM_REGTYPE_PREDICATE))
	{
		ASSERT(uIndexType == USC_REGTYPE_NOINDEX);
		return IMG_TRUE;
	}
	return IMG_FALSE;
}
#endif /* #if defined(EXECPRED) */

static
IMG_BOOL IsLink(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArg, IMG_UINT32 uType, IMG_UINT32 uIndexType)
/*****************************************************************************
 FUNCTION	: IsLink

 PURPOSE	: Check iif a hardware register type is supported in an instruction
			  source which only support the link register..

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Ignored.
			  uArg			- Ignored.
			  uType			- Hardware register type.
			  uIndexType	- Either USEASM_REGTYPE_INDEX for a dynamically indexed
							source or USC_REGTYPE_NOINDEX otherwise.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst);
	PVR_UNREFERENCED_PARAMETER(uArg);
	if (uType == USEASM_REGTYPE_LINK)
	{
		ASSERT(uIndexType == USC_REGTYPE_NOINDEX);
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static
IMG_BOOL IsDRC(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArg, IMG_UINT32 uType, IMG_UINT32 uIndexType)
/*****************************************************************************
 FUNCTION	: IsDRC

 PURPOSE	: Check iif a hardware register type is supported in an instruction
			  source which only supports a dependent read counter.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Ignored.
			  uArg			- Ignored.
			  uType			- Hardware register type.
			  uIndexType	- Either USEASM_REGTYPE_INDEX for a dynamically indexed
							source or USC_REGTYPE_NOINDEX otherwise.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst);
	PVR_UNREFERENCED_PARAMETER(uArg);
	if (uType == USEASM_REGTYPE_DRC)
	{
		ASSERT(uIndexType == USC_REGTYPE_NOINDEX);
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
static
IMG_BOOL IsSwizzle(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArg, IMG_UINT32 uType, IMG_UINT32 uIndexType)
/*****************************************************************************
 FUNCTION	: IsSwizzle

 PURPOSE	: Check if a hardware register type is supported in an instruction
			  source which only supports an immediate swizzle.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Ignored.
			  uArg			- Ignored.
			  uType			- Hardware register type.
			  uIndexType	- Either USEASM_REGTYPE_INDEX for a dynamically indexed
							source or USC_REGTYPE_NOINDEX otherwise.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst);
	PVR_UNREFERENCED_PARAMETER(uArg);
	if (uType == USEASM_REGTYPE_SWIZZLE)
	{
		ASSERT(uIndexType == USC_REGTYPE_NOINDEX);
		return IMG_TRUE;
	}
	return IMG_FALSE;
}
#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */

static
IMG_BOOL IsInternalOrUnused(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArg, IMG_UINT32 uType, IMG_UINT32 uIndexType)
/*****************************************************************************
 FUNCTION	: IsInternalOrUnused

 PURPOSE	: Check if a hardware register type is supported in an instruction
			  source which only accept either an internal register or USC_REGTYPE_UNUSEDSOURCE.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Ignored.
			  uArg			- Ignored.
			  uType			- Hardware register type.
			  uIndexType	- Either USEASM_REGTYPE_INDEX for a dynamically indexed
							source or USC_REGTYPE_NOINDEX otherwise.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst);
	PVR_UNREFERENCED_PARAMETER(uArg);
	if (uType == USEASM_REGTYPE_FPINTERNAL || uType == USC_REGTYPE_UNUSEDSOURCE)
	{
		ASSERT(uIndexType == USC_REGTYPE_NOINDEX);
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static
IMG_BOOL IsTempOrPrimAttr(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArg, IMG_UINT32 uType, IMG_UINT32 uIndexType)
/*****************************************************************************
 FUNCTION	: IsTempOrPrimAttr

 PURPOSE	: Check if a hardware register type is supported in an instruction
			  source which only supports either the temporary or the primary
			  attribute banks.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Ignored.
			  uArg			- Ignored.
			  uType			- Hardware register type.
			  uIndexType	- Either USEASM_REGTYPE_INDEX for a dynamically indexed
							source or USC_REGTYPE_NOINDEX otherwise.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst);
	PVR_UNREFERENCED_PARAMETER(uArg);
	PVR_UNREFERENCED_PARAMETER(uIndexType);
	if (uType == USEASM_REGTYPE_PRIMATTR || uType == USEASM_REGTYPE_TEMP)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static
IMG_BOOL IsValidSETLSource(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArg, IMG_UINT32 uType, IMG_UINT32 uIndexType)
/*****************************************************************************
 FUNCTION	: IsValidSETLSource

 PURPOSE	: Check if a hardware register type is valid as a source to the SETL
			  instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  uArg			- Argument to check.
			  uType			- Hardware register type.
			  uIndexType	- Either USEASM_REGTYPE_INDEX for a dynamically indexed
							source or USC_REGTYPE_NOINDEX otherwise.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(uArg == 0);
	if (uType == USEASM_REGTYPE_LINK)
	{
		ASSERT(uIndexType == USC_REGTYPE_NOINDEX);
		if (psInst->asDest[0].uType == USEASM_REGTYPE_LINK)
		{
			return IMG_TRUE;
		}
		else
		{
			return IMG_FALSE;
		}
	}
	return IsSrc12ExtendedBanks(uType);
}

IMG_INTERNAL 
IMG_BOOL IsValidEFOSource(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArg, IMG_UINT32 uType, IMG_UINT32 uIndexType)
/*****************************************************************************
 FUNCTION	: IsValidEFOSource

 PURPOSE	: Check if a hardware register type is valid as a source to the EFO
			  instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  uArg			- Argument to check.
			  uType			- Hardware register type.
			  uIndexType	- Either USEASM_REGTYPE_INDEX for a dynamically indexed
							source or USC_REGTYPE_NOINDEX otherwise.

 RETURNS	: Nothing.
*****************************************************************************/
{
	static const PFN_CAN_USE_SRC g_apfOldEfoSourceBankRestrictions[] =
	{
		IsSrc0Standard,
		IsSrc12Standard,
		IsSrc12Standard,
	};
	#if defined(SUPPORT_SGX545)
	static const PFN_CAN_USE_SRC g_apfNewEfoSourceBankRestrictions[] =
	{
		IsSrc0Extended,
		IsSrc12Standard,
		IsSrc12Extended,
	};
	#endif /* defined(SUPPORT_SGX545) */

	ASSERT(uArg < EFO_UNIFIEDSTORE_SOURCE_COUNT);

	#if defined(SUPPORT_SGX545)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NEW_EFO_OPTIONS) != 0)
	{
		return g_apfNewEfoSourceBankRestrictions[uArg](psState, psInst, uArg, uType, uIndexType); 
	}
	else
	#endif /* defined(SUPPORT_SGX545) */
	{
		return g_apfOldEfoSourceBankRestrictions[uArg](psState, psInst, uArg, uType, uIndexType); 
	}
}

static
IMG_BOOL IsValidMINorMAXSource0Bank(PINTERMEDIATE_STATE psState, 
									PCINST				psInst, 
									IMG_UINT32			uArg, 
									IMG_UINT32			uType, 
									IMG_UINT32			uIndexType)
/*****************************************************************************
 FUNCTION	: IsValidMINorMAXSource0Bank

 PURPOSE	: Check if a hardware register type is valid as a source to the MIN
			  or MAX instructions in source 0.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  uArg			- Argument to check.
			  uType			- Hardware register type.
			  uIndexType	- Either USEASM_REGTYPE_INDEX for a dynamically indexed
							source or USC_REGTYPE_NOINDEX otherwise.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(uIndexType);
	
	ASSERT(psInst->eOpcode == IFMIN || psInst->eOpcode == IFMAX);
	ASSERT(uArg == 0);

	#if defined(SUPPORT_SGX545)
	/*
		On SGX545 we'll expand 
			MIN	DEST, A, B -> MINMAX DEST, A, A, B
			MAX DEST, A, B -> MAXMIN DEST, A, B, A

		So check the first source against the restrictions on hardware source 0.
	*/
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_FCLAMP) != 0)
	{
		return IsSrc0StandardBanks(uType, uIndexType);
	}
	else
	#endif /* defined(SUPPORT_SGX545) */
	{
		return IsSrc12ExtendedBanks(uType);
	}
}

static
IMG_BOOL IsValidLMCDynamicOffsetBank(PINTERMEDIATE_STATE	psState, 
									 PCINST					psInst, 
									 IMG_UINT32				uArg, 
									 IMG_UINT32				uType, 
									 IMG_UINT32				uIndexType)
/*****************************************************************************
 FUNCTION	: IsValidLMCDynamicOffsetBank

 PURPOSE	: Check if a hardware register type is valid as the dynamic
			  offset argument to a LOADMEMCONST instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  uArg			- Argument to check.
			  uType			- Hardware register type.
			  uIndexType	- Either USEASM_REGTYPE_INDEX for a dynamically indexed
							source or USC_REGTYPE_NOINDEX otherwise.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psInst->u.psLoadMemConst->bRelativeAddress)
	{
		return IsSrc0StandardBanks(uType, uIndexType);
	}
	else
	{
		return IsImmediate(psState, psInst, uArg, uType, uIndexType);
	}
}

static
IMG_BOOL IsValidTESTSource0(PINTERMEDIATE_STATE	psState, 
							PCINST				psInst, 
							IMG_UINT32			uArg, 
							IMG_UINT32			uType, 
							IMG_UINT32			uIndexType)
/*****************************************************************************
 FUNCTION	: IsValidTESTSource1

 PURPOSE	: Check if a hardware register type is valid as source 0 to a
			  TESTPRED or TESTMASK instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  uArg			- Argument to check.
			  uType			- Hardware register type.
			  uIndexType	- Either USEASM_REGTYPE_INDEX for a dynamically indexed
							source or USC_REGTYPE_NOINDEX otherwise.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);

	ASSERT(psInst->eOpcode == ITESTMASK || psInst->eOpcode == ITESTPRED);
	ASSERT(uArg == 0);

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psInst->u.psTest->eAluOpcode == IIADD32 || 
		psInst->u.psTest->eAluOpcode == IIADDU32 ||
		psInst->u.psTest->eAluOpcode == IISUB32 || 
		psInst->u.psTest->eAluOpcode == IISUBU32)
	{
		return IsSrc0ExtendedBanks(uType, uIndexType);
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		PVR_UNREFERENCED_PARAMETER(uIndexType);

		return IsSrc12ExtendedBanks(uType);
	}
}

static
IMG_BOOL IsValidTESTSource1(PINTERMEDIATE_STATE	psState, 
							PCINST				psInst, 
							IMG_UINT32			uArg, 
							IMG_UINT32			uType, 
							IMG_UINT32			uIndexType)
/*****************************************************************************
 FUNCTION	: IsValidTESTSource1

 PURPOSE	: Check if a hardware register type is valid as source 1 to a
			  TESTPRED or TESTMASK instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  uArg			- Argument to check.
			  uType			- Hardware register type.
			  uIndexType	- Either USEASM_REGTYPE_INDEX for a dynamically indexed
							source or USC_REGTYPE_NOINDEX otherwise.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);

	ASSERT(psInst->eOpcode == ITESTMASK || psInst->eOpcode == ITESTPRED);
	ASSERT(uArg == 1);

	if (psInst->u.psTest->eAluOpcode == IFPADD8 || 
		psInst->u.psTest->eAluOpcode == IFPSUB8 ||
		psInst->u.psTest->eAluOpcode == IIADD8 || 
		psInst->u.psTest->eAluOpcode == IIADDU8 ||
		psInst->u.psTest->eAluOpcode == IISUB8 || 
		psInst->u.psTest->eAluOpcode == IISUBU8)
	{
		return IsSrc0ExtendedBanks(uType, uIndexType);
	}
	else
	{
		return IsSrc12ExtendedBanks(uType);
	}
}

/*
	Source bank restrictions for the IFDSX and IFDSY instructions.
*/
static const PFN_CAN_USE_SRC g_apfDSXDSYSourceBankRestrictions[] =
{
	IsTempOrPrimAttr, 
	IsTempOrPrimAttr,
};

static const PFN_CAN_USE_SRC g_apfAllSrc12Extended[] = 
{	
	IsSrc12Extended,
	IsSrc12Extended,
	IsSrc12Extended,
	IsSrc12Extended,
	IsSrc12Extended,
	IsSrc12Extended,
	IsSrc12Extended,
	IsSrc12Extended,
};

/*
	Source bank restrictions for instructions using hardware source 0 for the first
	argument.
*/
static const PFN_CAN_USE_SRC g_apfSrc0StandardElseSrc12Extended[] = 
{
	IsSrc0Standard,
	IsSrc12Extended,
	IsSrc12Extended,
};

/*
	Source bank restrictions for the FDDP_RPT instruction. A single instruction
	represents upto eight dotproduct iterations with three sources accessed per iteration.
*/
static const PFN_CAN_USE_SRC g_apfFDDP_RPT_SourceBankRestrictions[FDDP_RPT_ARGUMENT_COUNT] = 
{
	IsSrc0Standard, IsSrc12Extended, IsSrc12Extended,	/* Iteration 0 */
	IsSrc0Standard, IsSrc12Extended, IsSrc12Extended,	/* Iteration 1 */
	IsSrc0Standard, IsSrc12Extended, IsSrc12Extended,	/* Iteration 2 */
	IsSrc0Standard, IsSrc12Extended, IsSrc12Extended,	/* Iteration 3 */
	IsSrc0Standard, IsSrc12Extended, IsSrc12Extended,	/* Iteration 4 */
	IsSrc0Standard, IsSrc12Extended, IsSrc12Extended,	/* Iteration 5 */
	IsSrc0Standard, IsSrc12Extended, IsSrc12Extended,	/* Iteration 6 */
	IsSrc0Standard, IsSrc12Extended, IsSrc12Extended,	/* Iteration 7 */
};

#if defined(OUTPUT_USPBIN)
/*
	Source register bank restrictions for the SMP_USP_NDR instruction.
*/
static const PFN_CAN_USE_SRC g_apfSMP_USP_NDRSourceBankRestrictions[] = 
{
	IsSrc0Standard,
	IsSrc12Extended,
	IsSrc12Extended,
	IsSrc12Extended,
	IsSrc12Extended,

	IsSrc12Extended,
	IsSrc12Extended,
	IsSrc12Extended,
	IsSrc12Extended,
	IsSrc12Extended,

	IsSrc12Extended,
	IsSrc12Extended,
	IsSrc12Extended,
	IsSrc12Extended,
	IsSrc12Extended,

	IsSrc12Extended,
	IsSrc12Extended,
	IsSrc12Extended,
	IsSrc12Extended,
	IsSrc12Extended,
};

/*
	Source bank restrictions for the SMPUNPACK_USP instruction.
*/
static const PFN_CAN_USE_SRC g_apfSMPUNPACK_USPSourceBankRestrictions[UF_MAX_CHUNKS_PER_TEXTURE * 2] = 
{
	IsSrc0Standard,
	IsSrc12Extended,
	IsSrc12Extended,
	IsSrc12Extended,

	IsSrc12Extended,
	IsSrc12Extended,
	IsSrc12Extended,
	IsSrc12Extended,
};

/*
	Source register bank restrictions for the TEXWRITE instruction.
*/
static const PFN_CAN_USE_SRC g_apfTexwriteSourceBankRestrictions[] = 
{
	IsSrc12Extended,
	IsSrc12Extended,
	IsSrc12Extended,
	IsSrc12Extended,

	IsSrc12Extended,
	IsSrc12Extended,
	IsSrc12Extended,
	IsSrc12Extended,
};

#endif /* defined(OUTPUT_USPBIN) */

#if defined(SUPPORT_SGX545)
/*
	Source bank restrictions for the ELDD and ELDQ instructioins.
*/
static const PFN_CAN_USE_SRC g_apfExtendedLoadBankRestrictions[] =
{
	IsSrc12Standard,	/* ELD_BASE_ARGINDEX */
	IsSrc0Standard,		/* ELD_DYNAMIC_OFFSET_ARGINDEX */
	IsImmediate,		/* ELD_STATIC_OFFSET_ARGINDEX */
	IsSrc12Standard,	/* ELD_RANGE_ARGINDEX */
	IsDRC,				/* ELD_DRC_ARGINDEX */
};

/*
	Source bank restrictions for the IDIV32 instruction.
*/
static const PFN_CAN_USE_SRC g_apfIDIV32BankRestrictions[] =
{
	IsSrc12Extended,	/* IDIV32_DIVIDEND_ARGINDEX */
	IsSrc12Extended,	/* IDIV32_DIVISOR_ARGINDEX */
	IsDRC,				/* IDIV32_DRC_ARGINDEX */
};

/*
	Source bank restrictions for the dual-issue instruction.
*/
static const PFN_CAN_USE_SRC g_apfDUALSourceBankRestrictions[DUAL_ARGUMENT_COUNT] =
{
	IsSrc0Standard,		/* DUAL_USSOURCE0 */
	IsSrc12Extended,	/* DUAL_USSOURCE1 */
	IsSrc12Extended,	/* DUAL_USSOURCE2 */
	IsInternalOrUnused,	/* DUAL_I0SOURCE */
	IsInternalOrUnused,	/* DUAL_I1SOURCE */
	IsInternalOrUnused,	/* DUAL_I2SOURCE */
};
#endif /* defined(SUPPORT_SGX545) */

/*
	Source bank restrictions for the IMAE instruction.
*/
static const PFN_CAN_USE_SRC g_apfIMAESourceBankRestrictions[] = 
{
	IsSrc0Standard,
	IsSrc12Extended,
	IsSrc12Extended,
	IsInternalOrUnused,	/* IMAE_CARRYIN_SOURCE */
};

/*
	Source bank restrictions for the EFO instruction.
*/
static const PFN_CAN_USE_SRC g_apfEFOSourceBankRestrictions[] = 
{
	IsValidEFOSource,	/* EFO_UNIFIEDSTORE_SOURCE0 */
	IsValidEFOSource,	/* EFO_UNIFIEDSTORE_SOURCE1 */
	IsValidEFOSource,	/* EFO_UNIFIEDSTORE_SOURCE2 */
	/* These sources represent any internal register implicitly used by the EFO. */
	IsInternalOrUnused,	/* EFO_I0_SRC */
	IsInternalOrUnused,	/* EFO_I1_SRC */
};

/*
	Source bank restrictions for memory store instructions.
*/
static const PFN_CAN_USE_SRC g_apfMemStoreSourceBankRestrictions[] =
{
	IsSrc0Extended,		/* MEMLOADSTORE_BASE_ARGINDEX */
	IsSrc12Extended,	/* MEMLOADSTORE_OFFSET_ARGINDEX */
	IsSrc12Extended,	/* MEMSTORE_DATA_ARGINDEX */
};

/*
	Source bank restrictions for the ISPILLWRITE instruction.
*/
static const PFN_CAN_USE_SRC g_apfMemSpillWriteSourceBankRestrictions[] =
{
	IsSrc0Extended,		/* MEMSPILL_BASE_ARGINDEX */
	IsSrc12Extended,	/* MEMSPILL_OFFSET_ARGINDEX */
	IsSrc12Extended,	/* MEMSPILLWRITE_DATA_ARGINDEX */
};

/*
	Source bank restrictions for the ISPILLREAD instruction.
*/
static const PFN_CAN_USE_SRC g_apfMemSpillReadSourceBankRestrictions[] =
{
	IsSrc0Extended,		/* MEMSPILL_BASE_ARGINDEX */
	IsSrc12Extended,	/* MEMSPILL_OFFSET_ARGINDEX */
	IsSrc12Extended,	/* MEMSPILLREAD_RANGE_ARGINDEX */
	IsDRC,				/* MEMSPILLREAD_DRC_ARGINDEX */
};

/*
	Source bank restrictions for memory load instructions.
*/
static const PFN_CAN_USE_SRC g_apfMemLoadSourceBankRestrictions[] = 
{
	IsSrc0Extended,		/* MEMLOADSTORE_BASE_ARGINDEX */
	IsSrc12Extended,	/* MEMLOADSTORE_OFFSET_ARGINDEX */
	IsSrc12Extended,	/* MEMLOAD_RANGE_ARGINDEX */
	IsDRC,				/* MEMLOAD_DRC_ARGINDEX */
};

/*
	Source bank restrictions for tiled memory load instructions.
*/
static const PFN_CAN_USE_SRC g_apfTiledMemLoadSourceBankRestrictions[] = 
{
	IsSrc0Extended,		/* TILEDMEMLOAD_BASE_ARGINDEX */
	IsSrc12Extended,	/* TILEDMEMLOAD_OFFSET_ARGINDEX */
	IsDRC,				/* TILEDMEMLOAD_DRC_ARGINDEX */
};

/*
	Source bank restrictions for texture sample instructions.
*/
static const PFN_CAN_USE_SRC g_apfTextureSampleSourceBankRestrictons[SMP_MAXIMUM_ARG_COUNT] =
{
	CanUseSmpSrc,	/* SMP_COORD_ARG_START */
	CanUseSmpSrc,	/* SMP_COORD_ARG_START + 1 */
	CanUseSmpSrc,	/* SMP_COORD_ARG_START + 2 */
	CanUseSmpSrc,	/* SMP_COORD_ARG_START + 3 */
	CanUseSmpSrc,	/* SMP_PROJ_ARG */
	CanUseSmpSrc,	/* SMP_ARRAYINDEX_ARG */
	CanUseSmpSrc,	/* SMP_STATE_ARG_START */
	CanUseSmpSrc,	/* SMP_STATE_ARG_START + 1 */
	CanUseSmpSrc,	/* SMP_STATE_ARG_START + 2 */
	CanUseSmpSrc,	/* SMP_STATE_ARG_START + 3 */
	IsDRC,			/* SMP_DRC_ARG_START */
	CanUseSmpSrc,	/* SMP_LOD_ARG_START */
	CanUseSmpSrc,	/* SMP_GRAD_ARG_START */
	CanUseSmpSrc,	/* SMP_GRAD_ARG_START + 1 */
	CanUseSmpSrc,	/* SMP_GRAD_ARG_START + 2 */
	CanUseSmpSrc,	/* SMP_GRAD_ARG_START + 3 */
	CanUseSmpSrc,	/* SMP_GRAD_ARG_START + 4 */
	CanUseSmpSrc,	/* SMP_GRAD_ARG_START + 5 */
	CanUseSmpSrc,	/* SMP_GRAD_ARG_START + 6 */
	CanUseSmpSrc,	/* SMP_SAMPLE_IDX_ARG_START */
};

#if defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
/*
	Source bank restrictions for the IMA32 instruction.
*/
static const PFN_CAN_USE_SRC g_apfIMA32SourceBankRestrictions[] =
{
	IsSrc0Extended,
	IsSrc12Extended,
	IsSrc12Extended,
};
#endif /* defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

/*
	Source bank restrictions for the LOADMEMCONST instruction.
*/
static const PFN_CAN_USE_SRC g_apfLOADMEMCONSTSourceBankRestrictions[] = 
{
	IsSrc12Extended,				/* LOADMEMCONST_BASE_ARGINDEX */
	IsImmediate,					/* LOADMEMCONST_STATIC_OFFSET_ARGINDEX */
	IsValidLMCDynamicOffsetBank,	/* LOADMEMCONST_DYNAMIC_OFFSET_ARGINDEX */
	IsImmediate,					/* LOADMEMCONST_STRIDE_ARGINDEX */
	IsSrc12Extended,				/* LOADMEMCONST_MAX_RANGE_ARGINDEX */
};

/*
	Source bank restrictions for the FMIN and FMAX instructions.
*/
static const PFN_CAN_USE_SRC g_apfMinMaxSourceBankRestrictions[] = 
{
	IsValidMINorMAXSource0Bank,
	IsSrc12Extended,
};

/*
	Source bank restrictions for IADD8/IADDU8/ISUB8/ISUBU8/IFPADD8/IPFSUB8 instructions.
*/
static const PFN_CAN_USE_SRC g_apfIntegerTestSourceBankRestrictions[] = 
{
	IsSrc12Extended,
	IsSrc0Extended,
};

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_BOOL IsValidDUALGPISourceBank(PINTERMEDIATE_STATE	psState, 
								  PCINST				psInst, 
								  IMG_UINT32			uArg, 
								  IMG_UINT32			uType, 
								  IMG_UINT32			uIndexType)
/*****************************************************************************
 FUNCTION	: IsValidDUALGPISourceBank

 PURPOSE	: Check if a hardware register type is valid as one of the non-unified
			  store sources to a vector dual-issue instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  uArg			- Argument to check.
			  uType			- Hardware register type.
			  uIndexType	- Either USEASM_REGTYPE_INDEX for a dynamically indexed
							source or USC_REGTYPE_NOINDEX otherwise.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(uArg);

	ASSERT(psInst->eOpcode == IVDUAL);

	if ((psState->uFlags & USC_FLAGS_POSTC10REGALLOC) != 0)
	{
		if (uType == USEASM_REGTYPE_FPINTERNAL)
		{
			ASSERT(uIndexType == USC_REGTYPE_NOINDEX);
			return IMG_TRUE;
		}
	}
	else
	{
		if (uType == USEASM_REGTYPE_TEMP)
		{
			ASSERT(uIndexType == USC_REGTYPE_NOINDEX);
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static
IMG_BOOL IsNonIndexedTempOrPrimAttr(PINTERMEDIATE_STATE psState, 
									PCINST				psInst, 
									IMG_UINT32			uArg, 
									IMG_UINT32			uType, 
									IMG_UINT32			uIndexType)
/*****************************************************************************
 FUNCTION	: IsNonIndexedTempOrPrimAttr

 PURPOSE	: Check if a hardware register type is supported in an instruction
			  source which only supports either the temporary or the primary
			  attribute banks without indexing.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Ignored.
			  uArg			- Ignored.
			  uType			- Hardware register type.
			  uIndexType	- Either USEASM_REGTYPE_INDEX for a dynamically indexed
							source or USC_REGTYPE_NOINDEX otherwise.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst);
	PVR_UNREFERENCED_PARAMETER(uArg);
	
	if (uIndexType != USC_REGTYPE_NOINDEX)
	{
		return IMG_FALSE;
	}
	if (uType == USEASM_REGTYPE_PRIMATTR || uType == USEASM_REGTYPE_TEMP)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}


static
IMG_BOOL IsInternal(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArg, IMG_UINT32 uType, IMG_UINT32 uIndexType)
/*****************************************************************************
 FUNCTION	: IsInternal

 PURPOSE	: Check if a hardware register type is the internal register bank.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Ignored.
			  uArg			- Ignored.
			  uType			- Hardware register type.
			  uIndexType	- Either USEASM_REGTYPE_INDEX for a dynamically indexed
							source or USC_REGTYPE_NOINDEX otherwise.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst);
	PVR_UNREFERENCED_PARAMETER(uArg);
	if (uType == USEASM_REGTYPE_FPINTERNAL)
	{
		ASSERT(uIndexType == USC_REGTYPE_NOINDEX);
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static
IMG_BOOL IsSrc2MappedToSrc0(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArg, IMG_UINT32 uType, IMG_UINT32 uIndexType)
/*****************************************************************************
 FUNCTION	: IsSrc2MappedToSrc0

 PURPOSE	: Check if a hardware register type is supported by the second
			  source to a two arguments vector instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Ignored.
			  uArg			- Ignored.
			  uType			- Hardware register type.
			  uIndexType	- Either USEASM_REGTYPE_INDEX for a dynamically indexed
							source or USC_REGTYPE_NOINDEX otherwise.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst);
	PVR_UNREFERENCED_PARAMETER(uArg);

	/*	
		The source bank can either be the hardware constants or one of the register
		banks supported by source 0.
	*/
	if (uType == USEASM_REGTYPE_FPCONSTANT)
	{
		ASSERT(uIndexType == USC_REGTYPE_NOINDEX);
		return IMG_TRUE;
	}
	if (IsSrc0ExtendedBanks(uType, uIndexType))
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

#define C10_SOURCE_BANK_RESTRICTIONS(A) A, A

/*
	Source bank restrictions for the IFPADD8_VEC/IFPSUB8_VEC - fixed-point test
	instructions on cores with the vector instruction set.
*/
static const PFN_CAN_USE_SRC g_apfFixedPointVecTestSourceBankRestrictions[] =
{
	C10_SOURCE_BANK_RESTRICTIONS(IsSrc12Extended),
	C10_SOURCE_BANK_RESTRICTIONS(IsSrc0Extended),
};

/*
	Source bank restrictions for the SOP3 instruction on cores with the
	vector instruction set.
*/
static const PFN_CAN_USE_SRC g_apfSOP3VecBankRestrictions[] = 
{
	C10_SOURCE_BANK_RESTRICTIONS(IsSrc0Standard),
	C10_SOURCE_BANK_RESTRICTIONS(IsSrc12Extended),
	C10_SOURCE_BANK_RESTRICTIONS(IsSrc12Extended),
};

/*
	Source bank restrictions for the LRP1 instruction on cores with the
	vector instruction set.
*/
static const PFN_CAN_USE_SRC g_apfLRP1VecBankRestrictions[] = 
{
	C10_SOURCE_BANK_RESTRICTIONS(IsSrc0Standard),
	C10_SOURCE_BANK_RESTRICTIONS(IsSrc12Extended),
	C10_SOURCE_BANK_RESTRICTIONS(IsSrc12Extended),
};

/*
	Source bank restrictions for the FPMA instruction on cores with the
	vector instruction set.
*/
static const PFN_CAN_USE_SRC g_apfFPMAVecBankRestrictions[] = 
{
	C10_SOURCE_BANK_RESTRICTIONS(IsSrc0Standard),
	C10_SOURCE_BANK_RESTRICTIONS(IsSrc12Extended),
	C10_SOURCE_BANK_RESTRICTIONS(IsSrc12Extended),
};

/*
	Source bank restrictions for the C10/U8 conditional move instruction on cores with the
	vector instruction set.
*/
static const PFN_CAN_USE_SRC g_apfFPMOVCVecBankRestrictions[] =
{
	C10_SOURCE_BANK_RESTRICTIONS(IsSrc0Standard),
	C10_SOURCE_BANK_RESTRICTIONS(IsSrc12Extended),
	C10_SOURCE_BANK_RESTRICTIONS(IsSrc12Extended),
};

#define VECTOR_SOURCE_BANK_RESTRICTIONS(A) A, A, A, A, A

/*
	Source register bank restrictions for the VMAD and VMOVC instructions.
*/
static const PFN_CAN_USE_SRC g_apfVMADOrVMOVCSourceBankRestrictions[] = 
{
	/* First source */
	VECTOR_SOURCE_BANK_RESTRICTIONS(IsSrc0Standard),
	/* Second source */
	VECTOR_SOURCE_BANK_RESTRICTIONS(IsSrc12Extended),
	/* Third source */
	VECTOR_SOURCE_BANK_RESTRICTIONS(IsSrc12Extended),
};

/*
	Source register bank restrictions for the vector dual-issue
	instruction.
*/
static const PFN_CAN_USE_SRC g_apfVDUALSourceBankRestrictions[] = 
{
	/* VDUAL_SRCSLOT_UNIFIEDSTORE */
	VECTOR_SOURCE_BANK_RESTRICTIONS(IsSrc12Standard),
	/* VDUAL_SRCSLOT_GPI0 */
	VECTOR_SOURCE_BANK_RESTRICTIONS(IsValidDUALGPISourceBank),
	/* VDUAL_SRCSLOT_GPI1 */
	VECTOR_SOURCE_BANK_RESTRICTIONS(IsValidDUALGPISourceBank),
	/* VDUAL_SRCSLOT_GPI2 */
	VECTOR_SOURCE_BANK_RESTRICTIONS(IsValidDUALGPISourceBank),
};

/*
	Source register bank restrictions for the VDSX and VDSY instructions.
*/
static const PFN_CAN_USE_SRC g_apfVDSXOrVDSYSourceBankRestrictions[] = 
{
	VECTOR_SOURCE_BANK_RESTRICTIONS(IsTempOrPrimAttr),
	VECTOR_SOURCE_BANK_RESTRICTIONS(IsNonIndexedTempOrPrimAttr),
};

/*
	Source register bank restrictions for the VMAD4 instruction.
*/
static const PFN_CAN_USE_SRC g_apfVMAD4SourceBankRestrictions[] = 
{
	VECTOR_SOURCE_BANK_RESTRICTIONS(IsSrc12Extended),
	VECTOR_SOURCE_BANK_RESTRICTIONS(IsInternal),
	VECTOR_SOURCE_BANK_RESTRICTIONS(IsInternal),
};

/*
	Source register bank restrictions for vector instructions with
	one floating point vector sources.
*/
static const PFN_CAN_USE_SRC g_apfOneSourceSourceBankRestrictions[] = 
{
	VECTOR_SOURCE_BANK_RESTRICTIONS(IsSrc12Extended),
};

/*
	Source register bank restrictions for vector instructions with
	two floating point vector sources where the first source is read
	using hardware source 1 and source 2, and the second source is read
	using hardware source 0.
*/
static const PFN_CAN_USE_SRC g_apfTwoSourceSourceBankRestrictions[] = 
{
	VECTOR_SOURCE_BANK_RESTRICTIONS(IsSrc12Extended),
	VECTOR_SOURCE_BANK_RESTRICTIONS(IsSrc2MappedToSrc0),
};

/*
	Source register bank restrictions for vector pack instructions with
	a floating point source.
*/
static const PFN_CAN_USE_SRC g_apfVPCKSourceBankRestrictions[] = 
{
	VECTOR_SOURCE_BANK_RESTRICTIONS(IsSrc12Extended),
	VECTOR_SOURCE_BANK_RESTRICTIONS(IsSrc12Extended),
};

/*
	Source register bank restrictions for the VADD3 instruction.
*/
static const PFN_CAN_USE_SRC g_apfVADD3SourceBankRestrictions[] = 
{
	VECTOR_SOURCE_BANK_RESTRICTIONS(IsSrc12Extended),
	VECTOR_SOURCE_BANK_RESTRICTIONS(IsInternal),
};

/*
	Source register bank restrictions for the VMUL3 instruction.
*/
static const PFN_CAN_USE_SRC g_apfVMUL3SourceBankRestrictions[] = 
{
	VECTOR_SOURCE_BANK_RESTRICTIONS(IsSrc12Extended),
	VECTOR_SOURCE_BANK_RESTRICTIONS(IsInternal),
};

/*
	Source register bank restrictions for the VDP3_GPI and VDP_GPI instructions.
*/
static const PFN_CAN_USE_SRC g_apfVDP_GPISourceBankRestrictions[] =
{
	VECTOR_SOURCE_BANK_RESTRICTIONS(IsSrc12Extended),
	VECTOR_SOURCE_BANK_RESTRICTIONS(IsInternal),
};
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

/*
	Source bank restrictions for the TESTPRED/TESTMASK instructions.
*/
static const PFN_CAN_USE_SRC g_apfTestSourceBankRestrictions[] = 
{
	IsValidTESTSource0,
	IsValidTESTSource1,
};

/*
	Source bank restrictions for the SAVL instruction.
*/
static const PFN_CAN_USE_SRC g_apfSAVLBankRestrictions[] =
{
	IsLink,
};

/*
	Source bank restrictions for the MOV and SETL instructions.
*/
static const PFN_CAN_USE_SRC g_apfMOVOrSETLBankRestrictions[] =
{
	IsValidSETLSource,
};

#if defined(EXECPRED)
/*
	Source bank restrictions for the CNDST, CNDEF, CNDLT, CNDSTLOOP and CNDEFLOOP instructions.
*/
static const PFN_CAN_USE_SRC g_apfCNDSTOrCNDLTBankRestrictions[] = 
{
	IsSrc0Standard,
	IsImmediateOrPredicate,
	IsImmediate,
	IsImmediate,
};

/*
	Source bank restrictions for the CNDSM instruction.
*/
static const PFN_CAN_USE_SRC g_apfCNDSMBankRestrictions[] = 
{
	IsSrc0Standard,
	IsImmediateOrPredicate,
	IsImmediate,
	IsSrc0Standard,
};

/*
	Source bank restrictions for the CNDEND instruction.
*/
static const PFN_CAN_USE_SRC g_apfCNDENDBankRestrictions[] = 
{
	IsSrc0Standard,
	IsImmediate,
};
#endif /* #if defined(EXECPRED) */

#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
/*
	Source bank restrictions for the NRMF32 instruction.
*/
static const PFN_CAN_USE_SRC g_apfNRMF32BankRestrictions[NRM_ARGUMENT_COUNT] =
{
	IsSrc0Standard,
	IsSrc12Extended,
	IsSrc12Extended,
	IsDRC,				/* NRM_DRC_SOURCE */
};

/*
	Source bank restrictions for the NRMF16 instruction.
*/
static const PFN_CAN_USE_SRC g_apfNRMF16BankRestrictions[NRM_ARGUMENT_COUNT] =
{
	IsSrc12Extended,
	IsSrc12Extended,
	IsSwizzle,			/* NRM_F16_SWIZZLE */
	IsDRC,				/* NRM_DRC_SOURCE */
};
#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_BOOL CanUseVTESTBYTEMASKDest(PINTERMEDIATE_STATE psState, IMG_UINT32 uDestIdx, IMG_UINT32 uDestType, IMG_UINT32 uDestIndexType)
/*****************************************************************************
 FUNCTION	: CanUseVTESTBYTEMASKDest

 PURPOSE	: Check if a register can be written by a VTESTBYTEMASK instruction.

 PARAMETERS	: psState		- Compiler state.
			  uDestIdx		- Index of the destination to check.
			  uDestType		- Register to check.
			  uDestIndexType

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(uDestIdx);
	PVR_UNREFERENCED_PARAMETER(uDestIndexType);

	return (uDestType != USEASM_REGTYPE_FPINTERNAL);
}
#endif //defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)


#if defined(OUTPUT_USPBIN)
static
IMG_BOOL CanUseUSPSampleDest(PINTERMEDIATE_STATE psState, IMG_UINT32 uDestIdx, IMG_UINT32 uDestType, IMG_UINT32 uDestIndexType)
/*****************************************************************************
 FUNCTION	: CanUseUSPSampleDest

 PURPOSE	: Check if a register can be written by a USP sample instruction.

 PARAMETERS	: psState		- Compiler state.
			  uDestIdx		- Index of the destination to check.
			  uDestType		- Register to check.
			  uDestIndexType

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(uDestIdx);

	if (	
			(
				uDestType == USEASM_REGTYPE_TEMP ||
				uDestType == USEASM_REGTYPE_PRIMATTR ||
				uDestType == USEASM_REGTYPE_FPINTERNAL
			) &&
			uDestIndexType == USC_REGTYPE_NOINDEX
	   )
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}
#endif /* defined(OUTPUT_USPBIN) */

static
IMG_BOOL CanUseExternalWBDest(PINTERMEDIATE_STATE psState, IMG_UINT32 uDestIdx, IMG_UINT32 uDestType, IMG_UINT32 uDestIndexType)
/*****************************************************************************
 FUNCTION	: CanUseExternalWBDest

 PURPOSE	: Check if a register can be written by a texture sample, memory load,
			  normalise or integer divide instruction.

 PARAMETERS	: psState		- Compiler state.
			  uDestIdx		- Index of the destination to check.
			  uDestType		- Register to check.
			  uDestIndexType

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(uDestIdx);

	/*
		Only non-indexed temporaries or primary attributes are valid.
	*/
	if (	
			(
				uDestType == USEASM_REGTYPE_TEMP ||
				uDestType == USEASM_REGTYPE_PRIMATTR
			) &&
			uDestIndexType == USC_REGTYPE_NOINDEX
	   )
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static
IMG_BOOL CanUseEFODest(PINTERMEDIATE_STATE psState, IMG_UINT32 uDestIdx, IMG_UINT32 uDestType, IMG_UINT32 uDestIndexType)
/*****************************************************************************
 FUNCTION	: CanUseEFODest

 PURPOSE	: Check if a register can be written by an EFO instruction.

 PARAMETERS	: psState		- Compiler state.
			  uDestIdx		- Index of the destination to check.
			  uDestType		- Register to check.
			  uDestIndexType

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	switch (uDestIdx)
	{
		case EFO_US_DEST:
		{
			/*
				Support non-extended destination banks only.
			*/
			if (
					(
						uDestType == USEASM_REGTYPE_TEMP ||
						uDestType == USEASM_REGTYPE_PRIMATTR ||
						uDestType == USEASM_REGTYPE_OUTPUT ||
						uDestType == USEASM_REGTYPE_FPINTERNAL
					) &&
					uDestIndexType == USC_REGTYPE_NOINDEX
				)
			{
				return IMG_TRUE;
			}
			return IMG_FALSE;
		}
		case EFO_I0_DEST:
		case EFO_I1_DEST:
		{
			/*
				Supports internal registers only.
			*/
			if (uDestType == USEASM_REGTYPE_FPINTERNAL)
			{
				ASSERT(uDestIndexType == USC_REGTYPE_NOINDEX);
				return IMG_TRUE;
			}
			return IMG_FALSE;
		}
		default:
		{
			return IMG_TRUE;
		}
	}
}

static
IMG_BOOL CanUseSAVLDest(PINTERMEDIATE_STATE psState, IMG_UINT32 uDestIdx, IMG_UINT32 uDestType, IMG_UINT32 uDestIndexType)
/*****************************************************************************
 FUNCTION	: CanUseSAVLDest

 PURPOSE	: Check if a register can be written by a SAVL instruction.

 PARAMETERS	: psState		- Compiler state.
			  uDestIdx		- Index of the destination to check.
			  uDestType		- Register to check.
			  uDestIndexType

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(uDestIndexType);

	ASSERT(uDestIdx == 0);

	/*
		Don't allow SAVL to write output registers since it doesn't have the
		skipinvalid flag.
	*/
	if (uDestType == USEASM_REGTYPE_OUTPUT)
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

static
IMG_BOOL CanUseDELTADest(PINTERMEDIATE_STATE psState, IMG_UINT32 uDestIdx, IMG_UINT32 uDestType, IMG_UINT32 uDestIndexType)
/*****************************************************************************
 FUNCTION	: CanUseDELTADest

 PURPOSE	: Check if a register can be written by a DELTA instruction.

 PARAMETERS	: psState		- Compiler state.
			  uDestIdx		- Index of the destination to check.
			  uDestType		- Register to check.
			  uDestIndexType

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	ASSERT(uDestIdx == 0);

	if (uDestType == USEASM_REGTYPE_PREDICATE || uDestType == USEASM_REGTYPE_TEMP)
	{
		ASSERT(uDestIndexType == USC_REGTYPE_NOINDEX);
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static
IMG_BOOL CanUseIMAEDest(PINTERMEDIATE_STATE psState, IMG_UINT32 uDestIdx, IMG_UINT32 uDestType, IMG_UINT32 uDestIndexType)
/*****************************************************************************
 FUNCTION	: CanUseIMAEDest

 PURPOSE	: Check if a register can be written by an IMAE instruction.

 PARAMETERS	: psState		- Compiler state.
			  uDestIdx		- Index of the destination to check.
			  uDestType		- Register to check.
			  uDestIndexType

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (uDestIdx == IMAE_CARRYOUT_DEST)
	{
		if (uDestType == USEASM_REGTYPE_FPINTERNAL)
		{
			ASSERT(uDestIndexType == USC_REGTYPE_NOINDEX);
			return IMG_TRUE;
		}
		return IMG_FALSE;
	}
	else
	{
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (uDestType == USEASM_REGTYPE_FPINTERNAL)
		{
			/* In USSE2 GPI destinations are not supported. See http://forum1.kl.imgtec.org/forum/read.php?60,95367 */
			return IMG_FALSE;
		}
#endif //defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)

		return IMG_TRUE;
	}
}

#if defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_BOOL CanUseIMA32Dest(PINTERMEDIATE_STATE psState, IMG_UINT32 uDestIdx, IMG_UINT32 uDestType, IMG_UINT32 uDestIndexType)
/*****************************************************************************
 FUNCTION	: CanUseIMA32Dest

 PURPOSE	: Check if a register can be written by an IMA32 instruction.

 PARAMETERS	: psState		- Compiler state.
			  uDestIdx		- Index of the destination to check.
			  uDestType		- Register to check.
			  uDestIndexType

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (uDestIdx == IMA32_HIGHDWORD_DEST)
	{
		if (uDestType == USEASM_REGTYPE_FPINTERNAL)
		{
			ASSERT(uDestIndexType == USC_REGTYPE_NOINDEX);
			return IMG_TRUE;
		}
		return IMG_FALSE;
	}
	else
	{
		return IMG_TRUE;
	}
}
#endif /* defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

#define ARRAY_AND_SIZE(X)		{X, sizeof(X) / sizeof(X[0])}

IMG_INTERNAL INST_DESC const g_psInstDesc[IOPCODE_MAX] = 
{
	/* Invalid */
	{
		IOPCODE_NAME("Invalid")	/* pszName */
		0,						/* uFlags */
		0,						/* uFlags2 */
		0,						/* uArgumentCount */
		0,						/* uMoeArgumentCount */
		IMG_FALSE,				/* bHasDest */
		IMG_FALSE,				/* bCanRepeat */
		0,						/* uMaxRepeatCount */
		IMG_FALSE,				/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE},
								/* puMoeArgRemap */
		IMG_FALSE,				/* bCanPredicate */
		UINT_MAX,				/* uUseasmOpcode */
		INST_TYPE_UNDEF,		/* eType */
		INST_PRED_UNDEF,		/* ePredSupport */
		USC_UNDEF,				/* uDestRegisterNumberBitCount */
		USC_UNDEF,				/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		NULL,					/* pfGetLiveChansInArgument */
		{NULL, 0},				/* { apfCanUseSrc, uFuncCount } */
		NULL,					/* pfCanUseDest */
	},
	/* IMOV */
	{
		IOPCODE_NAME("IMOV")				/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		MOV_ARGUMENT_COUNT,					/* uArgumentCount */
		MOV_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_MOV,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,								/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE | USC_OPT_GROUP_DEP_F16_OPTIMIZATION,
								/* uOptimizationGroup */
		GetLiveChansInMOVArgument,		/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfMOVOrSETLBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IMOVPRED */
	{
		IOPCODE_NAME("IMOVPRED")				/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		MOV_ARGUMENT_COUNT,					/* uArgumentCount */
		MOV_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_MOVP,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IDELTA */
	{
		IOPCODE_NAME("IDELTA")				/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		0,					/* uFlags */
		0,					/* uFlags2 */
		0,					/* uDefaultArgumentCount */
		0,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_DELTA,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInMOVArgument,	/* pfGetLiveChansInArgument */
		{NULL, 0},		/* { apfCanUseSrc, uFuncCount } */
		CanUseDELTADest,		/* pfCanUseDest */
	},
	/* ILIMM */
	{
		IOPCODE_NAME("ILIMM")				/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		LIMM_ARGUMENT_COUNT,					/* uArgumentCount */
		LIMM_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_LIMM,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFMOV */
	{
		IOPCODE_NAME("IFMOV")			/* pszName */
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_HASTESTVARIANT |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_PERINSTMOE |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		1,					/* uArgumentCount */
		1,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FMOV,		/* uUseasmOpcode */
		INST_TYPE_FLOAT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFADD */
	{
		IOPCODE_NAME("IFADD")			/* pszName */
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_HASTESTVARIANT |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_PERINSTMOE |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		2,					/* uArgumentCount */
		2,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, DESC_ARGREMAP_INVALID, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FADD,		/* uUseasmOpcode */
		INST_TYPE_FLOAT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		USC_OPT_GROUP_ARITH_F32_INST | USC_OPT_GROUP_EFO_FORMATION,
								/* uOptimizationGroup */
		GetLiveChansInFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFSUB */
	{
		IOPCODE_NAME("IFSUB")			/* pszName */
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_HASTESTVARIANT |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_PERINSTMOE |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		2,					/* uArgumentCount */
		2,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, DESC_ARGREMAP_INVALID, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FSUB,		/* uUseasmOpcode */
		INST_TYPE_FLOAT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_ARITH_F32_INST,
								/* uOptimizationGroup */
		GetLiveChansInFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFMUL */
	{
		IOPCODE_NAME("IFMUL")			/* pszName */
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_HASTESTVARIANT |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_PERINSTMOE |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV | 
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		2,					/* uArgumentCount */
		2,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, 1, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FMUL,		/* uUseasmOpcode */
		INST_TYPE_FLOAT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		USC_OPT_GROUP_ARITH_F32_INST | USC_OPT_GROUP_EFO_FORMATION,
								/* uOptimizationGroup */
		GetLiveChansInFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFMAD */
	{
		IOPCODE_NAME("IFMAD")			/* pszName */
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_PERINSTMOE |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		3,					/* uArgumentCount */
		3,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, 1, 2},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FMAD,		/* uUseasmOpcode */
		INST_TYPE_FLOAT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		USC_OPT_GROUP_ARITH_F32_INST | USC_OPT_GROUP_EFO_FORMATION,
								/* uOptimizationGroup */
		GetLiveChansInFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFADM */
	{
		IOPCODE_NAME("IFADM")			/* pszName */
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_PERINSTMOE |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		3,					/* uArgumentCount */
		3,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, 1, 2},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FADM,		/* uUseasmOpcode */
		INST_TYPE_FLOAT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFMSA */
	{
		IOPCODE_NAME("IFMSA")			/* pszName */
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_PERINSTMOE |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		3,					/* uArgumentCount */
		3,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, 1, 2},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FMSA,		/* uUseasmOpcode */
		INST_TYPE_FLOAT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_EFO_FORMATION,
								/* uOptimizationGroup */
		GetLiveChansInFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFDP */
	{
		IOPCODE_NAME("IFDP")				/* pszName */
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_HASTESTVARIANT |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_PERINSTMOE |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		2,					/* uArgumentCount */
		2,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,	    /* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FDP,		/* uUseasmOpcode */
		INST_TYPE_FLOAT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFDP_RPT */
	{
		IOPCODE_NAME("IFDP_RPT")				/* pszName */
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_HASTESTVARIANT |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_PERINSTMOE |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		FDP_RPT_ARGUMENT_COUNT,					/* uArgumentCount */
		FDP_RPT_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FDP,		/* uUseasmOpcode */
		INST_TYPE_FDOTPRODUCT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFDDP */
	{
		IOPCODE_NAME("IFDDP")			/* pszName */
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_PERINSTMOE |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		3,					/* uArgumentCount */
		3,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, 1, 2},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FDDP,		/* uUseasmOpcode */
		INST_TYPE_FLOAT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFDDP_RPT */
	{
		IOPCODE_NAME("IFDDP_RPT")			/* pszName */
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_PERINSTMOE |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		FDDP_RPT_ARGUMENT_COUNT,					/* uArgumentCount */
		FDDP_RPT_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, 1, 2},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FDDP,		/* uUseasmOpcode */
		INST_TYPE_FDOTPRODUCT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfFDDP_RPT_SourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFRCP */
	{
		IOPCODE_NAME("IFRCP")			/* pszName */
		DESC_FLAGS_HASTESTVARIANT |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_COMPLEXOP |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		COMPLEXOP_ARGUMENT_COUNT,					/* uArgumentCount */
		COMPLEXOP_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FRCP,		/* uUseasmOpcode */
		INST_TYPE_FLOAT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFRSQ */
	{
		IOPCODE_NAME("IFRSQ")			/* pszName */
		DESC_FLAGS_HASTESTVARIANT |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_COMPLEXOP |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		COMPLEXOP_ARGUMENT_COUNT,					/* uArgumentCount */
		COMPLEXOP_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FRSQ,		/* uUseasmOpcode */
		INST_TYPE_FLOAT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFLOG */
	{
		IOPCODE_NAME("IFLOG")			/* pszName */
		DESC_FLAGS_HASTESTVARIANT |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_COMPLEXOP |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		COMPLEXOP_ARGUMENT_COUNT,					/* uArgumentCount */
		COMPLEXOP_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FLOG,		/* uUseasmOpcode */
		INST_TYPE_FLOAT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFEXP */
	{
		IOPCODE_NAME("IFEXP")			/* pszName */
		DESC_FLAGS_HASTESTVARIANT |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_COMPLEXOP |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		COMPLEXOP_ARGUMENT_COUNT,					/* uArgumentCount */
		COMPLEXOP_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FEXP,		/* uUseasmOpcode */
		INST_TYPE_FLOAT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	}, 
	/* IFDSX */
	{
		IOPCODE_NAME("IFDSX")			/* pszName */
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_HASTESTVARIANT |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_REQUIRESGRADIENTS |
		DESC_FLAGS_PERINSTMOE |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG |
		DESC_FLAGS2_DESTANDSRCOVERLAP,
							/* uFlags2 */
		2,					/* uArgumentCount */
		2,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FDSX,		/* uUseasmOpcode */
		INST_TYPE_FLOAT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfDSXDSYSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFDSY */
	{
		IOPCODE_NAME("IFDSY")			/* pszName */
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_HASTESTVARIANT |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_REQUIRESGRADIENTS |
		DESC_FLAGS_PERINSTMOE |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG |
		DESC_FLAGS2_DESTANDSRCOVERLAP,
							/* uFlags2 */
		2,					/* uArgumentCount */
		2,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FDSY,		/* uUseasmOpcode */
		INST_TYPE_FLOAT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfDSXDSYSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFMIN */
	{
		IOPCODE_NAME("IFMIN")			/* pszName */
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_HASTESTVARIANT |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_PERINSTMOE |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		2,					/* uArgumentCount */
		2,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FMIN,		/* uUseasmOpcode */
		INST_TYPE_FLOAT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfMinMaxSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFMAX */
	{
		IOPCODE_NAME("IFMAX")			/* pszName */
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_HASTESTVARIANT |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_PERINSTMOE |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		2,					/* uArgumentCount */
		2,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FMAX,		/* uUseasmOpcode */
		INST_TYPE_FLOAT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		0,
  						/* uOptimizationGroup */
		GetLiveChansInFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfMinMaxSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFTRC */
	{
		IOPCODE_NAME("IFTRC")			/* pszName */
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_F16F32SELECT |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		1,					/* uArgumentCount */
		1,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_FLOAT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFSUBFLR */
	{
		IOPCODE_NAME("IFSUBFLR")			/* pszName */
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_HASTESTVARIANT |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_PERINSTMOE |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		2,					/* uArgumentCount */
		2,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FSUBFLR,	/* uUseasmOpcode */
		INST_TYPE_FLOAT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IEFO */
	{
		IOPCODE_NAME("IEFO")				/* pszName */
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG |
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		0,					/* uFlags2 */
		EFO_SRC_COUNT,		/* uArgumentCount */
		EFO_HARDWARE_SOURCE_COUNT,	/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		EURASIA_USE1_EFO_RCOUNT_MAX,					
							/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{EFO_UNIFIEDSTORE_SOURCE0, EFO_UNIFIEDSTORE_SOURCE1, EFO_UNIFIEDSTORE_SOURCE2},			
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_EFO,		/* uUseasmOpcode */
		INST_TYPE_EFO,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInEFOArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfEFOSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		CanUseEFODest,		/* pfCanUseDest */
	},
	/* ITESTPRED */
	{
		IOPCODE_NAME("ITESTPRED")			/* pszName */
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		0,					/* uFlags */
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_TEST |
		DESC_FLAGS2_REPEATMASKONLY |
		0,					/* uFlags2 */
		TEST_ARGUMENT_COUNT,					/* uArgumentCount */
		TEST_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_TEST_MAX_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_TEST,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_TEST,
								/* uOptimizationGroup */
		GetLiveChansInTESTArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfTestSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ITESTMASK */
	{
		IOPCODE_NAME("ITESTMASK")			/* pszName */
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		0,					/* uFlags */
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_TESTMASK |
		DESC_FLAGS2_REPEATMASKONLY |
		0,					/* uFlags2 */
		TEST_ARGUMENT_COUNT,					/* uArgumentCount */
		TEST_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_TEST_MAX_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_TEST,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_TEST,
								/* uOptimizationGroup */
		GetLiveChansInTESTArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfTestSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFDP_TESTPRED */
	{
		IOPCODE_NAME("IFDP_TESTPRED")			/* pszName */
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		0,					/* uFlags */
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_TEST |
		DESC_FLAGS2_REPEATMASKONLY |
		0,					/* uFlags2 */
		FDP_RPT_ARGUMENT_COUNT,					/* uArgumentCount */
		FDP_RPT_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_TEST_MAX_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_FDOTPRODUCT,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFDP_TESTMASK */
	{
		IOPCODE_NAME("IFDP_TESTMASK")			/* pszName */
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		0,					/* uFlags */
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_TESTMASK |
		DESC_FLAGS2_REPEATMASKONLY |
		0,					/* uFlags2 */
		FDP_RPT_ARGUMENT_COUNT,					/* uArgumentCount */
		FDP_RPT_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_TEST_MAX_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_FDOTPRODUCT,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ILOADCONST */
	{
		IOPCODE_NAME("ILOADCONST")		/* pszName */
		DESC_FLAGS_MEMLOAD |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		0,					/* uFlags */
		0,					/* uFlags2 */
		LOADCONST_ARGUMENT_COUNT,					/* uArgumentCount */
		LOADCONST_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_LOADCONST,/* eType */
		INST_PRED_UNDEF,	/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		{NULL, 0},		/* { apfCanUseSrc, uFuncCount } */
		CanUseExternalWBDest,		/* pfCanUseDest */
	},
	/* ILOADRTAIDX */
	{
		IOPCODE_NAME("ILOADRTAIDX")		/* pszName */
		DESC_FLAGS_MEMLOAD |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		0,					/* uFlags */
		0,					/* uFlags2 */
		0,					/* uArgumentCount */
		0,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_UNDEF,	/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		{NULL, 0},		/* { apfCanUseSrc, uFuncCount } */
		CanUseExternalWBDest,		/* pfCanUseDest */
	},
	/* ILOADMEMCONST */
	{
		IOPCODE_NAME("ILOADMEMCONST")	/* pszName */
		DESC_FLAGS_MEMLOAD |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		0,					/* uFlags */
		0,					/* uFlags2 */
		LOADMEMCONST_ARGUMENT_COUNT,	/* uArgumentCount */
		LOADMEMCONST_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_LOADMEMCONST,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_ABS_MEMLDST | 
		USC_OPT_GROUP_HIGH_LATENCY | 
		0,					/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfLOADMEMCONSTSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		CanUseExternalWBDest,		/* pfCanUseDest */
	},
	/* ILDAB */
	{
		IOPCODE_NAME("ILDAB")			/* pszName */
		DESC_FLAGS_MEMLOAD |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_ABSOLUTELOADSTORE |
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		0,					/* uFlags2 */
		MEMLOAD_ARGUMENT_COUNT,	/* uArgumentCount */
		MEMLOAD_ARGUMENT_COUNT,	/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{MEMLOADSTORE_BASE_ARGINDEX, MEMLOADSTORE_OFFSET_ARGINDEX, MEMLOAD_RANGE_ARGINDEX},			
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_LDAB,		/* uUseasmOpcode */
		INST_TYPE_LDST,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_ABS_MEMLDST,		/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfMemLoadSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		CanUseExternalWBDest,		/* pfCanUseDest */
	},
	/* ISTAB */
	{
		IOPCODE_NAME("ISTAB")			/* pszName */
		DESC_FLAGS_MEMSTORE |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		0,					/* uFlags */
		DESC_FLAGS2_ABSOLUTELOADSTORE |
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		0,					/* uFlags2 */
		MEMSTORE_ARGUMENT_COUNT,					/* uArgumentCount */
		MEMSTORE_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{MEMLOADSTORE_BASE_ARGINDEX, MEMLOADSTORE_OFFSET_ARGINDEX, MEMSTORE_DATA_ARGINDEX},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_STAB,		/* uUseasmOpcode */
		INST_TYPE_LDST,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_ABS_MEMLDST,	/* uOptimizationGroup */
		GetLiveChansInStoreArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ILDLB */
	{
		IOPCODE_NAME("ILDLB")			/* pszName */
		DESC_FLAGS_MEMLOAD |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_LOCALLOADSTORE |
		0,					/* uFlags */
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_IMPLICITLYNONUNIFORM |
		0,					/* uFlags2 */
		MEMLOAD_ARGUMENT_COUNT,		/* uArgumentCount */
		MEMLOAD_ARGUMENT_COUNT,		/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{MEMLOADSTORE_BASE_ARGINDEX, MEMLOADSTORE_OFFSET_ARGINDEX, MEMLOAD_RANGE_ARGINDEX},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_LDLB,		/* uUseasmOpcode */
		INST_TYPE_LDST,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_LDST_IND_TMP_INST,
								/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfMemLoadSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		CanUseExternalWBDest,		/* pfCanUseDest */
	},
	/* ISTLB */
	{
		IOPCODE_NAME("ISTLB")			/* pszName */
		DESC_FLAGS_MEMSTORE |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_LOCALLOADSTORE |
		0,					/* uFlags */
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_IMPLICITLYNONUNIFORM |
		0,					/* uFlags2 */
		MEMSTORE_ARGUMENT_COUNT,					/* uArgumentCount */
		MEMSTORE_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{MEMLOADSTORE_BASE_ARGINDEX, MEMLOADSTORE_OFFSET_ARGINDEX, MEMSTORE_DATA_ARGINDEX},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_STLB,		/* uUseasmOpcode */
		INST_TYPE_LDST,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_LDST_IND_TMP_INST,
								/* uOptimizationGroup */
		GetLiveChansInStoreArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ILDAW */
	{
		IOPCODE_NAME("ILDAW")			/* pszName */
		DESC_FLAGS_MEMLOAD |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_ABSOLUTELOADSTORE |
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		0,					/* uFlags2 */
		MEMLOAD_ARGUMENT_COUNT,		/* uArgumentCount */
		MEMLOAD_ARGUMENT_COUNT,		/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{MEMLOADSTORE_BASE_ARGINDEX, MEMLOADSTORE_OFFSET_ARGINDEX, MEMLOAD_RANGE_ARGINDEX},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_LDAW,		/* uUseasmOpcode */
		INST_TYPE_LDST,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_ABS_MEMLDST,		/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfMemLoadSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		CanUseExternalWBDest,		/* pfCanUseDest */
	},
	/* ISTAW */
	{
		IOPCODE_NAME("ISTAW")			/* pszName */
		DESC_FLAGS_MEMSTORE |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		0,					/* uFlags */
		DESC_FLAGS2_ABSOLUTELOADSTORE |
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		0,					/* uFlags2 */
		MEMSTORE_ARGUMENT_COUNT,					/* uArgumentCount */
		MEMSTORE_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{MEMLOADSTORE_BASE_ARGINDEX, MEMLOADSTORE_OFFSET_ARGINDEX, MEMSTORE_DATA_ARGINDEX},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_STAW,		/* uUseasmOpcode */
		INST_TYPE_LDST,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_ABS_MEMLDST,		/* uOptimizationGroup */
		GetLiveChansInStoreArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ILDLW */
	{
		IOPCODE_NAME("ILDLW")			/* pszName */
		DESC_FLAGS_MEMLOAD |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_LOCALLOADSTORE |
		0,					/* uFlags */
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_IMPLICITLYNONUNIFORM |
		0,					/* uFlags2 */
		MEMLOAD_ARGUMENT_COUNT,		/* uArgumentCount */
		MEMLOAD_ARGUMENT_COUNT,		/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{MEMLOADSTORE_BASE_ARGINDEX, MEMLOADSTORE_OFFSET_ARGINDEX, MEMLOAD_RANGE_ARGINDEX},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_LDLW,		/* uUseasmOpcode */
		INST_TYPE_LDST,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_LDST_IND_TMP_INST,
								/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfMemLoadSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		CanUseExternalWBDest,		/* pfCanUseDest */
	},
	/* ISTLW */
	{
		IOPCODE_NAME("ISTLW")			/* pszName */
		DESC_FLAGS_MEMSTORE |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_LOCALLOADSTORE |
		0,					/* uFlags */
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_IMPLICITLYNONUNIFORM |
		0,					/* uFlags2 */
		MEMSTORE_ARGUMENT_COUNT,					/* uArgumentCount */
		MEMSTORE_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{MEMLOADSTORE_BASE_ARGINDEX, MEMLOADSTORE_OFFSET_ARGINDEX, MEMSTORE_DATA_ARGINDEX},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_STLW,		/* uUseasmOpcode */
		INST_TYPE_LDST,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_LDST_IND_TMP_INST,
								/* uOptimizationGroup */
		GetLiveChansInStoreArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ILDAD */
	{
		IOPCODE_NAME("ILDAD")			/* pszName */
		DESC_FLAGS_MEMLOAD |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_ABSOLUTELOADSTORE |
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		0,					/* uFlags2 */
		MEMLOAD_ARGUMENT_COUNT,		/* uArgumentCount */
		MEMLOAD_ARGUMENT_COUNT,		/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{MEMLOADSTORE_BASE_ARGINDEX, MEMLOADSTORE_OFFSET_ARGINDEX, MEMLOAD_RANGE_ARGINDEX},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_LDAD,		/* uUseasmOpcode */
		INST_TYPE_LDST,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_ABS_MEMLDST,		/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfMemLoadSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		CanUseExternalWBDest,		/* pfCanUseDest */
	},
	/* ISTAD */
	{
		IOPCODE_NAME("ISTAD")			/* pszName */
		DESC_FLAGS_MEMSTORE |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		0,					/* uFlags */
		DESC_FLAGS2_ABSOLUTELOADSTORE |
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		0,					/* uFlags2 */
		MEMSTORE_ARGUMENT_COUNT,					/* uArgumentCount */
		MEMSTORE_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{MEMLOADSTORE_BASE_ARGINDEX, MEMLOADSTORE_OFFSET_ARGINDEX, MEMSTORE_DATA_ARGINDEX},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_STAD,		/* uUseasmOpcode */
		INST_TYPE_LDST,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_ABS_MEMLDST,			/* uOptimizationGroup */
		GetLiveChansInStoreArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfMemStoreSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ILDLD */
	{
		IOPCODE_NAME("ILDLD")			/* pszName */
		DESC_FLAGS_MEMLOAD |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_LOCALLOADSTORE |
		0,					/* uFlags */
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_IMPLICITLYNONUNIFORM |
		0,					/* uFlags2 */
		MEMLOAD_ARGUMENT_COUNT,		/* uArgumentCount */
		MEMLOAD_ARGUMENT_COUNT,		/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{MEMLOADSTORE_BASE_ARGINDEX, MEMLOADSTORE_OFFSET_ARGINDEX, MEMLOAD_RANGE_ARGINDEX},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_LDLD,		/* uUseasmOpcode */
		INST_TYPE_LDST,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_LDST_IND_TMP_INST,
								/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfMemLoadSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		CanUseExternalWBDest,		/* pfCanUseDest */
	},
	/* ISTLD */
	{
		IOPCODE_NAME("ISTLD")			/* pszName */
		DESC_FLAGS_MEMSTORE |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_LOCALLOADSTORE |
		0,					/* uFlags */
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_IMPLICITLYNONUNIFORM |
		0,					/* uFlags2 */
		MEMSTORE_ARGUMENT_COUNT,					/* uArgumentCount */
		MEMSTORE_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{MEMLOADSTORE_BASE_ARGINDEX, MEMLOADSTORE_OFFSET_ARGINDEX, MEMSTORE_DATA_ARGINDEX},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_STLD,		/* uUseasmOpcode */
		INST_TYPE_LDST,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_LDST_IND_TMP_INST,
								/* uOptimizationGroup */
		GetLiveChansInStoreArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ILDTD */
	{
		IOPCODE_NAME("ILDTD")			/* pszName */
		DESC_FLAGS_MEMLOAD |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		0,					/* uFlags */
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_IMPLICITLYNONUNIFORM |
		0,					/* uFlags2 */
		TILEDMEMLOAD_ARGUMENT_COUNT,		/* uArgumentCount */
		TILEDMEMLOAD_ARGUMENT_COUNT,		/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,		/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{TILEDMEMLOAD_BASE_ARGINDEX, TILEDMEMLOAD_OFFSET_ARGINDEX, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_LDTD,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_LDST_IND_TMP_INST,
								/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfTiledMemLoadSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		CanUseExternalWBDest,		/* pfCanUseDest */
	},
	/* ISPILLREAD */
	{
		IOPCODE_NAME("ISPILLREAD")		/* pszName */
		DESC_FLAGS_MEMLOAD |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_LOCALLOADSTORE |
		0,					/* uFlags */
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_IMPLICITLYNONUNIFORM |
		0,					/* uFlags2 */
		MEMSPILLREAD_ARGUMENT_COUNT,					/* uArgumentCount */
		MEMSPILLREAD_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{MEMSPILL_BASE_ARGINDEX, MEMSPILL_OFFSET_ARGINDEX, MEMSPILLREAD_RANGE_ARGINDEX},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_LDLD,		/* uUseasmOpcode */
		INST_TYPE_MEMSPILL,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfMemSpillReadSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		CanUseExternalWBDest,		/* pfCanUseDest */
	},
	/* ISPILLWRITE */
	{
		IOPCODE_NAME("ISPILLWRITE")		/* pszName */
		DESC_FLAGS_MEMSTORE |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_LOCALLOADSTORE |
		0,					/* uFlags */
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_IMPLICITLYNONUNIFORM |
		0,					/* uFlags2 */
		MEMSPILLWRITE_ARGUMENT_COUNT,					/* uArgumentCount */
		MEMSPILLWRITE_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{MEMSPILL_BASE_ARGINDEX, MEMSPILL_OFFSET_ARGINDEX, MEMSPILLWRITE_DATA_ARGINDEX},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_STLD,		/* uUseasmOpcode */
		INST_TYPE_MEMSPILL,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInSPILLWRITEArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfMemSpillWriteSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	#if defined(SUPPORT_SGX545)
	/* IELDD */
	{
		IOPCODE_NAME("IELDD")			/* pszName */
		DESC_FLAGS_MEMLOAD |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		0,					/* uFlags2 */
		ELD_ARGUMENT_COUNT,	/* uArgumentCount */
		ELD_ARGUMENT_COUNT,	/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{ELD_DYNAMIC_OFFSET_ARGINDEX, ELD_BASE_ARGINDEX, ELD_RANGE_ARGINDEX},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_ELDD,		/* uUseasmOpcode */
		INST_TYPE_LDST,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfExtendedLoadBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		CanUseExternalWBDest,		/* pfCanUseDest */
	},
	/* IELDQ */
	{
		IOPCODE_NAME("IELDQ")			/* pszName */
		DESC_FLAGS_MEMLOAD |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		0,					/* uFlags2 */
		ELD_ARGUMENT_COUNT,	/* uArgumentCount */
		ELD_ARGUMENT_COUNT,	/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{ELD_DYNAMIC_OFFSET_ARGINDEX, ELD_BASE_ARGINDEX, ELD_RANGE_ARGINDEX},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_ELDQ,		/* uUseasmOpcode */
		INST_TYPE_LDST,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfExtendedLoadBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		CanUseExternalWBDest,		/* pfCanUseDest */
	},
	#endif /* defined(SUPPORT_SGX545) */
	/* IIDF */
	{
		IOPCODE_NAME("IIDF")				/* pszName */
		0,					/* uFlags */
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		0,					/* uFlags2 */
		IDF_ARGUMENT_COUNT,					/* uArgumentCount */
		IDF_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_IDF,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,					/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,				/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IWDF */
	{
		IOPCODE_NAME("IWDF")				/* pszName */
		0,					/* uFlags */
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		0,					/* uFlags2 */
		WDF_ARGUMENT_COUNT,					/* uArgumentCount */
		WDF_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_WDF,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IIMA16 */
	{
		IOPCODE_NAME("IIMA16")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		IMA16_ARGUMENT_COUNT,	/* uArgumentCount */
		IMA16_ARGUMENT_COUNT,	/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_INT_RCOUNT_MAXIMUM,	    /* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{0, 1, 2},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_IMA16,	/* uUseasmOpcode */
		INST_TYPE_IMA16,	/* eType */
		INST_PRED_SHORT,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInIMA16Argument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IIMAE */
	{
		IOPCODE_NAME("IIMAE")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		IMAE_ARGUMENT_COUNT,					/* uArgumentCount */
		IMAE_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_INT_RCOUNT_MAXIMUM,	    /* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{0, 1, 2},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_IMAE,		/* uUseasmOpcode */
		INST_TYPE_IMAE,		/* eType */
		INST_PRED_SHORT,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInIMAEArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfIMAESourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		CanUseIMAEDest,		/* pfCanUseDest */
	},
	/* IUNPCKU16S8 */
	{
		IOPCODE_NAME("IUNPCKU16S8")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_UNPCKU16S8,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_CHANS_USED_PCK_U8C10SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IUNPCKS16S8 */
	{
		IOPCODE_NAME("IUNPCKS16S8")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_UNPCKS16S8,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_CHANS_USED_PCK_U8C10SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IUNPCKU16U8 */
	{
		IOPCODE_NAME("IUNPCKU16U8")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_UNPCKU16U8,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_CHANS_USED_PCK_U8C10SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IUNPCKS16U8 */
	{
		IOPCODE_NAME("IUNPCKS16U8")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_UNPCKS16U8,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_CHANS_USED_PCK_U8C10SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IUNPCKU16U16 */
	{
		IOPCODE_NAME("IUNPCKU16U16")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_UNPCKU16U16,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_CHANS_USED_PCK_F16SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IUNPCKU16S16 */
	{
		IOPCODE_NAME("IUNPCKU16S16")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_UNPCKU16S16,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_CHANS_USED_PCK_F16SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IUNPCKU16F16 */
	{
		IOPCODE_NAME("IUNPCKU16F16")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_UNPCKU16U16,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_CHANS_USED_PCK_F16SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IPCKU16F32 */
	{
		IOPCODE_NAME("IPCKU16F32")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_PCKU16F32,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IUNPCKU16S16 */
	{
		IOPCODE_NAME("IUNPCKS16U16")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_UNPCKS16U16,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_CHANS_USED_PCK_F16SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IUNPCKS16F32 */
	{
		IOPCODE_NAME("IUNPCKS16F32")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_PCKS16F32,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IUNPCKU8U8 */
	{
		IOPCODE_NAME("IUNPCKU8U8")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_UNPCKU8U8,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE | USC_OPT_GROUP_CHANS_USED_PCK_U8C10SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IUNPCKU8S8 */
	{
		IOPCODE_NAME("IUNPCKU8S8")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_UNPCKU8S8,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE | USC_OPT_GROUP_CHANS_USED_PCK_U8C10SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IPCKU8U16 */
	{
		IOPCODE_NAME("IPCKU8U16")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_PCKU8U16,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IPCKU8S16 */
	{
		IOPCODE_NAME("IPCKU8S16")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_PCKU8S16,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IPCKS8U8 */
	{
		IOPCODE_NAME("IPCKS8U8")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_UNPCKS8U8,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE | USC_OPT_GROUP_CHANS_USED_PCK_U8C10SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IPCKS8U16 */
	{
		IOPCODE_NAME("IPCKS8U16")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_PCKS8U16,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IPCKS8S16 */
	{
		IOPCODE_NAME("IPCKS8S16")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_PCKS8S16,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IPCKU8F16 */
	{
		IOPCODE_NAME("IPCKU8F16")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_PCKU8F16,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE | USC_OPT_GROUP_CHANS_USED_PCK_F16SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IPCKU8F32 */
	{
		IOPCODE_NAME("IPCKU8F32")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_PCKU8F32,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IPCKS8F16 */
	{
		IOPCODE_NAME("IPCKS8F16")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_PCKS8F16,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE | USC_OPT_GROUP_CHANS_USED_PCK_F16SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IPCKS8F32 */
	{
		IOPCODE_NAME("IPCKS8F32")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_PCKS8F32,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IPCKC10C10 */
	{
		IOPCODE_NAME("IPCKC10C10")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_UNPCKC10C10,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE | USC_OPT_GROUP_CHANS_USED_PCK_U8C10SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IPCKC10F32 */
	{
		IOPCODE_NAME("IPCKC10F32")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_PCKC10F32,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IUNPCKF16C10 */
	{
		IOPCODE_NAME("IUNPCKF16C10")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_UNPCKF16C10,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE | USC_OPT_GROUP_ELIMINATE_F16_MOVES | 
		USC_OPT_GROUP_CHANS_USED_PCK_U8C10SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IUNPCKF16O8 */
	{
		IOPCODE_NAME("IUNPCKF16O8")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_UNPCKF16O8,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE | USC_OPT_GROUP_CHANS_USED_PCK_U8C10SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IUNPCKF16U8 */
	{
		IOPCODE_NAME("IUNPCKF16U8")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_UNPCKF16U8,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE | USC_OPT_GROUP_CHANS_USED_PCK_U8C10SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IUNPCKF16S8 */
	{
		IOPCODE_NAME("IUNPCKF16S8")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_UNPCKF16S8,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE | USC_OPT_GROUP_CHANS_USED_PCK_U8C10SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IUNPCKF16U16 */
	{
		IOPCODE_NAME("IUNPCKF16U16")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_UNPCKF16U16,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_CHANS_USED_PCK_F16SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IUNPCKF16S16 */
	{
		IOPCODE_NAME("IUNPCKF16S16")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_UNPCKF16S16,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_CHANS_USED_PCK_F16SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IPCKF16F16 */
	{
		IOPCODE_NAME("IPCKF16F16")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_PCKF16F16,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_ELIMINATE_F16_MOVES | USC_OPT_GROUP_DEP_F16_OPTIMIZATION | 
		USC_OPT_GROUP_CHANS_USED_PCK_F16SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IPCKF16F32 */
	{
		IOPCODE_NAME("IPCKF16F32")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_PCKF16F32,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_ELIMINATE_F16_MOVES | USC_OPT_GROUP_DEP_F16_OPTIMIZATION,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IUNPCKF32C10 */
	{
		IOPCODE_NAME("IUNPCKF32C10")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		1,					/* uArgumentCount */
		1,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_UNPCKF32C10,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE | USC_OPT_GROUP_CHANS_USED_PCK_U8C10SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IUNPCKF32O8 */
	{
		IOPCODE_NAME("IUNPCKF32O8")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		1,					/* uArgumentCount */
		1,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_UNPCKF32O8,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_CHANS_USED_PCK_U8C10SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IUNPCKF32U8 */
	{
		IOPCODE_NAME("IUNPCKF32U8")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		1,					/* uArgumentCount */
		1,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_UNPCKF32U8,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE | USC_OPT_GROUP_CHANS_USED_PCK_U8C10SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IUNPCKF32S8 */
	{
		IOPCODE_NAME("IUNPCKF32S8")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		1,					/* uArgumentCount */
		1,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_UNPCKF32S8,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_CHANS_USED_PCK_U8C10SRC,
  						/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IUNPCKF32U16 */
	{
		IOPCODE_NAME("IUNPCKF32U16")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		1,					/* uArgumentCount */
		1,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_UNPCKF32U16,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_CHANS_USED_PCK_F16SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IUNPCKF32S16 */
	{
		IOPCODE_NAME("IUNPCKF32S16")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		1,					/* uArgumentCount */
		1,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_UNPCKF32S16,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_CHANS_USED_PCK_F16SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IUNPCKF32F16 */
	{
		IOPCODE_NAME("IUNPCKF32F16")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		1,					/* uArgumentCount */
		1,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_UNPCKF32F16,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE | USC_OPT_GROUP_ELIMINATE_F16_MOVES | 
		USC_OPT_GROUP_CHANS_USED_PCK_F16SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IPCKC10F16 */
	{
		IOPCODE_NAME("IPCKC10F16")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		PCK_SOURCE_COUNT,					/* uArgumentCount */
		PCK_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_PCKC10F16,		/* uUseasmOpcode */
		INST_TYPE_PCK,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE | USC_OPT_GROUP_CHANS_USED_PCK_F16SRC,
								/* uOptimizationGroup */
		GetLiveChansInPCKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ICOMBC10 */
	{
		IOPCODE_NAME("ICOMBC10")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		COMBC10_SOURCE_COUNT,					/* uArgumentCount */
		COMBC10_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, COMBC10_RGB_SOURCE, COMBC10_ALPHA_SOURCE},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_UNPCKC10C10,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_EXTENDED,		/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE,
								/* uOptimizationGroup */
		GetLiveChansInCOMBC10Argument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ICOMBC10_ALPHA */
	{
		IOPCODE_NAME("ICOMBC10_ALPHA")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		COMBC10_SOURCE_COUNT,					/* uArgumentCount */
		COMBC10_SOURCE_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, COMBC10_RGB_SOURCE, COMBC10_ALPHA_SOURCE},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_EXTENDED,		/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInCOMBC10Argument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ISMP */
	{
		IOPCODE_NAME("ISMP")			/* pszName */
		DESC_FLAGS_TEXTURESAMPLE |
		DESC_FLAGS_REQUIRESGRADIENTS |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_DEPENDENTTEXTURESAMPLE |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		0,					/* uFlags */
		0,					/* uFlags2 */
		SMP_MAXIMUM_ARG_COUNT,	/* uArgumentCount */
		SMP_MAXIMUM_ARG_COUNT,	/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,					
							/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{SMP_COORD_ARG_START, SMP_STATE_ARG_START, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_SMP,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_HIGH_LATENCY | 
		0,					/* uOptimizationGroup */
		GetLiveChansInSMPArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfTextureSampleSourceBankRestrictons),		/* { apfCanUseSrc, uFuncCount } */
		CanUseExternalWBDest,		/* pfCanUseDest */
	},
	/* ISMPBIAS */
	{
		IOPCODE_NAME("ISMPBIAS")		/* pszName */
		DESC_FLAGS_TEXTURESAMPLE |
		DESC_FLAGS_REQUIRESGRADIENTS |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_DEPENDENTTEXTURESAMPLE |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		0,					/* uFlags */
		0,					/* uFlags2 */
		SMP_MAXIMUM_ARG_COUNT,	/* uArgumentCount */
		SMP_MAXIMUM_ARG_COUNT,	/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,					
							/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{SMP_COORD_ARG_START, SMP_STATE_ARG_START, SMP_LOD_ARG_START},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_SMP,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_HIGH_LATENCY | 
		0,
								/* uOptimizationGroup */
		GetLiveChansInSMPArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfTextureSampleSourceBankRestrictons),		/* { apfCanUseSrc, uFuncCount } */
		CanUseExternalWBDest,		/* pfCanUseDest */
	},
	/* ISMPREPLACE */
	{
		IOPCODE_NAME("ISMPREPLACE")		/* pszName */
		DESC_FLAGS_TEXTURESAMPLE |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_DEPENDENTTEXTURESAMPLE |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		0,					/* uFlags */
		0,					/* uFlags2 */
		SMP_MAXIMUM_ARG_COUNT,	/* uArgumentCount */
		SMP_MAXIMUM_ARG_COUNT,	/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,					
							/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{SMP_COORD_ARG_START, SMP_STATE_ARG_START, SMP_LOD_ARG_START},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_SMP,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_HIGH_LATENCY | 
		0,
								/* uOptimizationGroup */
		GetLiveChansInSMPArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfTextureSampleSourceBankRestrictons),		/* { apfCanUseSrc, uFuncCount } */
		CanUseExternalWBDest,		/* pfCanUseDest */
	},
	/* ISMPGRAD */
	{
		IOPCODE_NAME("ISMPGRAD")		/* pszName */
		DESC_FLAGS_TEXTURESAMPLE |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_DEPENDENTTEXTURESAMPLE |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		0,					/* uFlags */
		0,					/* uFlags2 */
		SMP_MAXIMUM_ARG_COUNT,	/* uArgumentCount */
		SMP_MAXIMUM_ARG_COUNT,	/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,					
							/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{SMP_COORD_ARG_START, SMP_STATE_ARG_START, SMP_GRAD_ARG_START},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_SMP,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		USC_OPT_GROUP_HIGH_LATENCY | 
		0,
								/* uOptimizationGroup */
		GetLiveChansInSMPArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfTextureSampleSourceBankRestrictons),		/* { apfCanUseSrc, uFuncCount } */
		CanUseExternalWBDest,		/* pfCanUseDest */
	},
	/* ISHL */
	{
		IOPCODE_NAME("ISHL")				/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		0,					/* uFlags */
		DESC_FLAGS2_BITWISE |
		0,					/* uFlags2 */
		BITWISE_ARGUMENT_COUNT,					/* uArgumentCount */
		BITWISE_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_SHL,		/* uUseasmOpcode */
		INST_TYPE_BITWISE,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInSHLArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ISHR */
	{
		IOPCODE_NAME("ISHR")				/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		0,					/* uFlags */
		DESC_FLAGS2_BITWISE |
		0,					/* uFlags2 */
		BITWISE_ARGUMENT_COUNT,					/* uArgumentCount */
		BITWISE_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_SHR,		/* uUseasmOpcode */
		INST_TYPE_BITWISE,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInSHRArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IASR */
	{
		IOPCODE_NAME("IASR")				/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		0,					/* uFlags */
		DESC_FLAGS2_BITWISE |
		0,					/* uFlags2 */
		BITWISE_ARGUMENT_COUNT,					/* uArgumentCount */
		BITWISE_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_ASR,		/* uUseasmOpcode */
		INST_TYPE_BITWISE,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IAND */
	{
		IOPCODE_NAME("IAND")				/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		0,					/* uFlags */
		DESC_FLAGS2_BITWISE |
		0,					/* uFlags2 */
		BITWISE_ARGUMENT_COUNT,					/* uArgumentCount */
		BITWISE_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_AND,		/* uUseasmOpcode */
		INST_TYPE_BITWISE,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IXOR */
	{
		IOPCODE_NAME("IXOR")				/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		0,					/* uFlags */
		DESC_FLAGS2_BITWISE |
		0,					/* uFlags2 */
		BITWISE_ARGUMENT_COUNT,					/* uArgumentCount */
		BITWISE_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_XOR,		/* uUseasmOpcode */
		INST_TYPE_BITWISE,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IOR */
	{
		IOPCODE_NAME("IOR")				/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		0,					/* uFlags */
		DESC_FLAGS2_BITWISE |
		0,					/* uFlags2 */
		BITWISE_ARGUMENT_COUNT,					/* uArgumentCount */
		BITWISE_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_OR,		/* uUseasmOpcode */
		INST_TYPE_BITWISE,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* INOT */
	{
		IOPCODE_NAME("INOT")				/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		0,					/* uFlags */
		DESC_FLAGS2_BITWISE |
		0,					/* uFlags2 */
		1,					/* uArgumentCount */
		1,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_BITWISE,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,			/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IRLP */
	{
		IOPCODE_NAME("IRLP")				/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		0,					/* uFlags */
		DESC_FLAGS2_BITWISE |
		0,					/* uFlags2 */
		RLP_ARGUMENT_COUNT,					/* uArgumentCount */
		RLP_MOEARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, RLP_UNIFIEDSTORE_ARGIDX, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_RLP,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IBR */
	{
		IOPCODE_NAME("IBR")				/* pszName */
		0,					/* uFlags */
		0,					/* uFlags2 */
		1,					/* uArgumentCount */
		1,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_BR,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		NULL,				/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ICALL */
	{
		IOPCODE_NAME("ICALL")			/* pszName */
		0,					/* uFlags */
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		0,					/* uFlags2 */
		0,					/* uArgumentCount */
		0,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_BR,		/* uUseasmOpcode */
		INST_TYPE_CALL,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		NULL,				/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ILABEL */
	{
		IOPCODE_NAME("ILABEL")			/* pszName */
		0,					/* uFlags */
		0,					/* uFlags2 */
		1,					/* uArgumentCount */
		1,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_LABEL,	/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		NULL,				/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ILAPC */
	{
		IOPCODE_NAME("ILAPC")			/* pszName */
		0,					/* uFlags */
		0,					/* uFlags2 */
		0,					/* uArgumentCount */
		0,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_LAPC,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		NULL,				/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ISETL */
	{
		IOPCODE_NAME("ISETL")			/* pszName */
		0,					/* uFlags */
		0,					/* uFlags2 */
		SETL_ARGUMENT_COUNT,					/* uArgumentCount */
		SETL_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_MOV,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		USC_UNDEF,							/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfMOVOrSETLBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ISAVL */
	{
		IOPCODE_NAME("ISAVL")			/* pszName */
		0,					/* uFlags */
		0,					/* uFlags2 */
		SAVL_ARGUMENT_COUNT,					/* uArgumentCount */
		SAVL_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_MOV,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		USC_UNDEF,							/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSAVLBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		CanUseSAVLDest,		/* pfCanUseDest */
	},
	/* IDRVPADDING */
	{
		IOPCODE_NAME("IDRVPADDING")		/* pszName */
		0,					/* uFlags */
		0,					/* uFlags2 */
		0,					/* uArgumentCount */
		0,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_PADDING,	/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		NULL,				/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ISMLSI */
	{
		IOPCODE_NAME("ISMLSI")			/* pszName */
		0,					/* uFlags */
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		0,					/* uFlags2 */
		SMLSI_ARGUMENT_COUNT,					/* uArgumentCount */
		SMLSI_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_SMLSI,	/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ISMR */
	{
		IOPCODE_NAME("ISMR")				/* pszName */
		0,					/* uFlags */
		0,					/* uFlags2 */
		SMR_ARGUMENT_COUNT,					/* uArgumentCount */
		SMR_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_SMR,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ISETFC */
	{
		IOPCODE_NAME("ISETFC")			/* pszName */
		0,					/* uFlags */
		0,					/* uFlags2 */
		SETFC_ARGUMENT_COUNT,					/* uArgumentCount */
		SETFC_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_SETFC,	/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ISMBO */
	{
		IOPCODE_NAME("ISMBO")			/* pszName */
		0,					/* uFlags */
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		0,					/* uFlags2 */
		SMBO_ARGUMENT_COUNT,					/* uArgumentCount */
		SMBO_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0, 					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_SMBO,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* INOP */
	{
		IOPCODE_NAME("INOP")				/* pszName */
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		0,					/* uArgumentCount */
		0,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_PADDING,	/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		NULL,				/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ISOPWM */
	{
		IOPCODE_NAME("ISOPWM")			/* pszName */
		DESC_FLAGS_C10FMTCTL |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		2,					/* uArgumentCount */
		2,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_SOP2WM,	/* uUseasmOpcode */
		INST_TYPE_SOPWM,	/* eType */
		INST_PRED_SHORT,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE,
								/* uOptimizationGroup */
		GetLiveChansInC10Arg,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ISOP2 */
	{
		IOPCODE_NAME("ISOP2")			/* pszName */
		DESC_FLAGS_C10FMTCTL |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		SOP2_ARGUMENT_COUNT,					/* uArgumentCount */
		SOP2_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_SOP2,		/* uUseasmOpcode */
		INST_TYPE_SOP2,		/* eType */
		INST_PRED_SHORT,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE,
								/* uOptimizationGroup */
		GetLiveChansInC10Arg,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ISOP3 */
	{
		IOPCODE_NAME("ISOP3")			/* pszName */
		DESC_FLAGS_C10FMTCTL |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		SOP3_ARGUMENT_COUNT,					/* uArgumentCount */
		SOP3_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{0, 1, 2},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_SOP3,		/* uUseasmOpcode */
		INST_TYPE_SOP3,		/* eType */
		INST_PRED_SHORT,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE,
								/* uOptimizationGroup */
		GetLiveChansInC10Arg,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ILRP1 */
	{
		IOPCODE_NAME("ILRP1")			/* pszName */
		DESC_FLAGS_C10FMTCTL |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		LRP1_ARGUMENT_COUNT,					/* uArgumentCount */
		LRP1_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{0, 1, 2},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_LRP1,		/* uUseasmOpcode */
		INST_TYPE_LRP1,		/* eType */
		INST_PRED_SHORT,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE,
								/* uOptimizationGroup */
		GetLiveChansInC10Arg,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFPMA */
	{
		IOPCODE_NAME("IFPMA")			/* pszName */
		DESC_FLAGS_C10FMTCTL |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		FPMA_ARGUMENT_COUNT,					/* uArgumentCount */
		FPMA_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_INT_RCOUNT_MAXIMUM,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{0, 1, 2},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FPMA,		/* uUseasmOpcode */
		INST_TYPE_FPMA,		/* eType */
		INST_PRED_SHORT,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE,
								/* uOptimizationGroup */
		GetLiveChansInC10Arg,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFPADD8 */
	{
		IOPCODE_NAME("IFPADD8")			/* pszName */
		DESC_FLAGS_C10FMTCTL |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		FPTEST_ARGUMENT_COUNT,					/* uArgumentCount */
		FPTEST_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FPADD8,	/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInFPTESTArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfIntegerTestSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFPSUB8 */
	{
		IOPCODE_NAME("IFPSUB8")			/* pszName */
		DESC_FLAGS_C10FMTCTL |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		FPTEST_ARGUMENT_COUNT,					/* uArgumentCount */
		FPTEST_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FPSUB8,	/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInFPTESTArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfIntegerTestSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IIMA8 */
	{
		IOPCODE_NAME("IIMA8")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		FPMA_ARGUMENT_COUNT,					/* uArgumentCount */
		FPMA_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_INT_RCOUNT_MAXIMUM,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{0, 1, 2},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_IMA8,		/* uUseasmOpcode */
		INST_TYPE_FPMA,		/* eType */
		INST_PRED_SHORT,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,					/* uOptimizationGroup */
		GetLiveChansInIMA8Arg,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFDPC */
	{
		IOPCODE_NAME("IFDPC")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_PERINSTMOE |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		FDPC_ARGUMENT_COUNT,					/* uArgumentCount */
		FDPC_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,					
							/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FDPC,		/* uUseasmOpcode */
		INST_TYPE_DPC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFDPC_RPT */
	{
		IOPCODE_NAME("IFDPC_RPT")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_PERINSTMOE |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		FDP_RPT_ARGUMENT_COUNT,					/* uArgumentCount */
		FDP_RPT_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,					
							/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FDPC,		/* uUseasmOpcode */
		INST_TYPE_FDOTPRODUCT,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFPDOT */
	{
		IOPCODE_NAME("IFPDOT")			/* pszName */
		DESC_FLAGS_C10FMTCTL |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		FPDOT_ARGUMENT_COUNT,					/* uArgumentCount */
		FPDOT_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_INT_RCOUNT_MAXIMUM,	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_DOT34,	/* eType */
		INST_PRED_SHORT,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE,
								/* uOptimizationGroup */
		GetLiveChansInFPDOTArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFP16DOT */
	{
		IOPCODE_NAME("IFP16DOT")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		FPDOT_ARGUMENT_COUNT,					/* uArgumentCount */
		FPDOT_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_INT_RCOUNT_MAXIMUM,	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_DOT34,	/* eType */
		INST_PRED_SHORT,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInFPDOTArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IMOVC */
	{
		IOPCODE_NAME("IMOVC")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		MOVC_ARGUMENT_COUNT,					/* uArgumentCount */
		MOVC_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, 1, 2},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_MOVC,		/* uUseasmOpcode */
		INST_TYPE_MOVC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IMOVC_C10 */
	{
		IOPCODE_NAME("IMOVC_C10")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		MOVC_ARGUMENT_COUNT,					/* uArgumentCount */
		MOVC_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, 1, 2},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_MOVC,		/* uUseasmOpcode */
		INST_TYPE_MOVC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInMOVC_U8_C10_Argument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IMOVC_U8 */
	{
		IOPCODE_NAME("IMOVC_U8")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		MOVC_ARGUMENT_COUNT,					/* uArgumentCount */
		MOVC_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, 1, 2},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_MOVC,		/* uUseasmOpcode */
		INST_TYPE_MOVC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInMOVC_U8_C10_Argument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IMOVC_I32 */
	{
		IOPCODE_NAME("IMOVC_I32")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		MOVC_ARGUMENT_COUNT,					/* uArgumentCount */
		MOVC_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, 1, 2},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_MOVC,		/* uUseasmOpcode */
		INST_TYPE_MOVC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInMOVC_I32Argument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IMOVC_I16 */
	{
		IOPCODE_NAME("IMOVC_I16")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		MOVC_ARGUMENT_COUNT,					/* uArgumentCount */
		MOVC_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, 1, 2},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_MOVC,		/* uUseasmOpcode */
		INST_TYPE_MOVC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFMOV16 */
	{
		IOPCODE_NAME("IFMOV16")			/* pszName */
		DESC_FLAGS_PERINSTMOE |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_FARITH16 |
		0,					/* uFlags */
		0,					/* uFlags2 */
		1,					/* uArgumentCount */
		1,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_FARITH16,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		USC_OPT_GROUP_ARITH_F16_INST | USC_OPT_GROUP_ELIMINATE_F16_MOVES,
								/* uOptimizationGroup */
		GetLiveChansInFARITH16Argument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFADD16 */
	{
		IOPCODE_NAME("IFADD16")			/* pszName */
		DESC_FLAGS_PERINSTMOE |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_FARITH16 |
		0,					/* uFlags */
		0,					/* uFlags2 */
		2,					/* uArgumentCount */
		2,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, DESC_ARGREMAP_INVALID, 1},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_FARITH16,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		USC_OPT_GROUP_ARITH_F16_INST | USC_OPT_GROUP_DEP_F16_OPTIMIZATION,
								/* uOptimizationGroup */
		GetLiveChansInFARITH16Argument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFMUL16 */
	{
		IOPCODE_NAME("IFMUL16")			/* pszName */
		DESC_FLAGS_PERINSTMOE |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_FARITH16 |
		0,					/* uFlags */
		0,					/* uFlags2 */
		2,					/* uArgumentCount */
		2,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, 1, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_FARITH16,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		USC_OPT_GROUP_ARITH_F16_INST | USC_OPT_GROUP_DEP_F16_OPTIMIZATION,
								/* uOptimizationGroup */
		GetLiveChansInFARITH16Argument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFMAD16 */
	{
		IOPCODE_NAME("IFMAD16")			/* pszName */
		DESC_FLAGS_PERINSTMOE |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_FARITH16 |
		0,					/* uFlags */
		0,					/* uFlags2 */
		3,					/* uArgumentCount */
		3,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, 1, 2},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FMAD16,	/* uUseasmOpcode */
		INST_TYPE_FARITH16,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		USC_OPT_GROUP_ARITH_F16_INST | USC_OPT_GROUP_DEP_F16_OPTIMIZATION,
								/* uOptimizationGroup */
		GetLiveChansInFARITH16Argument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	
	#if defined(SUPPORT_SGX545)
	/* SGX545 Instructions */
	/* IFMINMAX */
	{
		IOPCODE_NAME("IFMINMAX")		/* pszName */
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_PERINSTMOE |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		FMINMAX_ARGUMENT_COUNT,					/* uArgumentCount */
		FMINMAX_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, 1, 2},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FMINMAX,	/* uUseasmOpcode */
		INST_TYPE_FLOAT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},

	/* IFMAXMIN */
	{
		IOPCODE_NAME("IFMAXMIN")		/* pszName */
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_PERINSTMOE |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		FMAXMIN_ARGUMENT_COUNT,					/* uArgumentCount */
		FMAXMIN_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, 1, 2},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FMAXMIN,	/* uUseasmOpcode */
		INST_TYPE_FLOAT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc02,			/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},

	/* IFSSQ */
	{
		IOPCODE_NAME("IFSSQ")		/* pszName */
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		FSSQ_ARGUMENT_COUNT,					/* uArgumentCount */
		FSSQ_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FSSQ,		/* uUseasmOpcode */
		INST_TYPE_FLOAT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFSIN */
	{
		IOPCODE_NAME("IFSIN")		/* pszName */
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_COMPLEXOP |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		COMPLEXOP_ARGUMENT_COUNT,					/* uArgumentCount */
		COMPLEXOP_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE}, /* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FSIN,		/* uUseasmOpcode */
		INST_TYPE_FLOAT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},

	/* IFCOS */
	{
		IOPCODE_NAME("IFCOS")		/* pszName */
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_COMPLEXOP |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		COMPLEXOP_ARGUMENT_COUNT,					/* uArgumentCount */
		COMPLEXOP_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE}, /* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FCOS,		/* uUseasmOpcode */
		INST_TYPE_FLOAT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	#endif /* defined(SUPPORT_SGX545) */

	#if defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/* IIMA32 */
	{
		IOPCODE_NAME("IIMA32")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |		
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		0,					/* uFlags */
		0,					/* uFlags2 */
		IMA32_SOURCE_COUNT,	/* uArgumentCount */
		IMA32_SOURCE_COUNT,	/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_INT_RCOUNT_MAXIMUM,		/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{0, 1, 2},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_IMA32,	/* uUseasmOpcode */
		INST_TYPE_IMA32,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfIMA32SourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		CanUseIMA32Dest,		/* pfCanUseDest */
	},
	#endif /* defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	#if defined(SUPPORT_SGX545)
	/* IIDIV32 */
	{
		IOPCODE_NAME("IIDIV32")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		0,					/* uFlags2 */
		IDIV32_ARGUMENT_COUNT,		/* uArgumentCount */
		IDIV32_ARGUMENT_COUNT,		/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_INT_RCOUNT_MAXIMUM,		/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, IDIV32_DIVIDEND_ARGINDEX, IDIV32_DIVISOR_ARGINDEX},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_IDIV,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfIDIV32BankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		CanUseExternalWBDest,		/* pfCanUseDest */
	},
	/* IFSQRT */
	{
		IOPCODE_NAME("IFSQRT")			/* pszName */
		DESC_FLAGS_HASTESTVARIANT |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_COMPLEXOP |
		0,					/* uFlags */
		DESC_FLAGS2_FLOAT32NEG,
							/* uFlags2 */
		COMPLEXOP_ARGUMENT_COUNT,					/* uArgumentCount */
		COMPLEXOP_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FSQRT,	/* uUseasmOpcode */
		INST_TYPE_FLOAT,	/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IDUAL */
	{
		IOPCODE_NAME("IDUAL")			/* pszName */
		DESC_FLAGS_F16F32SELECT |
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_USESHWSRC0 |
		0,					/* uFlags */
		0,					/* uFlags2 */
		DUAL_ARGUMENT_COUNT,					/* uArgumentCount */
		DUAL_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DUAL_USSOURCE0, DUAL_USSOURCE1, DUAL_USSOURCE2},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_DUAL,		/* eType */
		INST_PRED_SHORT,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInDUALArgument,		/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfDUALSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IRNE */
	{
		IOPCODE_NAME("IRNE")				/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		ROUND_ARGUMENT_COUNT,					/* uArgumentCount */
		ROUND_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		(IMG_UINT32)USEASM_OP_UNPCKF32F32,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ITRUNC_HW */
	{
		IOPCODE_NAME("ITRUNC_HW")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		ROUND_ARGUMENT_COUNT,					/* uArgumentCount */
		ROUND_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		(IMG_UINT32)USEASM_OP_UNPCKF32F32,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFLOOR */
	{
		IOPCODE_NAME("IFLOOR")				/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		ROUND_ARGUMENT_COUNT,					/* uArgumentCount */
		ROUND_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		(IMG_UINT32)USEASM_OP_UNPCKF32F32,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	#endif /* defined(SUPPORT_SGX545) */

	#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
	/* IFNRM */
	{
		IOPCODE_NAME("IFNRM")		/* pszName */
		DESC_FLAGS_MEMLOAD |
        DESC_FLAGS_MULTIPLEDEST |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_F16FMTCTL |
		DESC_FLAGS_F16F32SELECT |
		0,					/* uFlags */
		0,					/* uFlags2 */
		NRM_ARGUMENT_COUNT,					/* uArgumentCount */
		NRM_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{0, 1, 2},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FNRM32,		/* uUseasmOpcode */
		INST_TYPE_NRM,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInFNRMArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfNRMF32BankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		CanUseExternalWBDest,		/* pfCanUseDest */
	},
	/* IFNRMF16 */
	{
		IOPCODE_NAME("IFNRMF16")		/* pszName */
		DESC_FLAGS_MEMLOAD |
        DESC_FLAGS_MULTIPLEDEST |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		NRM_ARGUMENT_COUNT,					/* uArgumentCount */
		NRM_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1}, /* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FNRM16,	/* uUseasmOpcode */
		INST_TYPE_NRM,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInFNRM16Argument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfNRMF16BankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		CanUseExternalWBDest,		/* pfCanUseDest */
	},
	#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */

	/* Load and store to arrays */
	/* ILDARRRF32 */
	{
		IOPCODE_NAME("ILDARRF32")		/* pszName */
		DESC_FLAGS_MEMLOAD |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_DESTCOMPMASK |
		0,					/* uFlags */
		0,					/* uFlags2 */
		LDARR_ARGUMENT_COUNT,					/* uArgumentCount */
		LDARR_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_LDSTARR,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		{NULL, 0},		/* { apfCanUseSrc, uFuncCount } */
		CanUseExternalWBDest,		/* pfCanUseDest */
	},
	/* ILDARRRF16 */
	{
		IOPCODE_NAME("ILDARRF16")		/* pszName */
		DESC_FLAGS_MEMLOAD |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_DESTCOMPMASK |
		0,					/* uFlags */
		0,					/* uFlags2 */
		LDARR_ARGUMENT_COUNT,					/* uArgumentCount */
		LDARR_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_LDSTARR,	/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		{NULL, 0},		/* { apfCanUseSrc, uFuncCount } */
		CanUseExternalWBDest,		/* pfCanUseDest */
	},
	/* ILDARRRU8 */
	{
		IOPCODE_NAME("ILDARRU8")		/* pszName */
		DESC_FLAGS_MEMLOAD |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_DESTCOMPMASK |
		0,					/* uFlags */
		0,					/* uFlags2 */
		LDARR_ARGUMENT_COUNT,					/* uArgumentCount */
		LDARR_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_LDSTARR,	/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		{NULL, 0},		/* { apfCanUseSrc, uFuncCount } */
		CanUseExternalWBDest,		/* pfCanUseDest */
	},
	/* ILDARRRC10 */
	{
		IOPCODE_NAME("ILDARRC10")		/* pszName */
		DESC_FLAGS_MEMLOAD |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_DESTCOMPMASK |
		0,					/* uFlags */
		0,					/* uFlags2 */
		LDARR_ARGUMENT_COUNT,					/* uArgumentCount */
		LDARR_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_LDSTARR,	/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		{NULL, 0},		/* { apfCanUseSrc, uFuncCount } */
		CanUseExternalWBDest,		/* pfCanUseDest */
	},

	/* ISTARRF32 */
	{
		IOPCODE_NAME("ISTARRF32")		/* pszName */
		DESC_FLAGS_MEMSTORE |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_DESTCOMPMASK |
		0,					/* uFlags */
		0,					/* uFlags2 */
		STARRF32_ARGUMENT_COUNT,					/* uArgumentCount */
		STARRF32_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_LDSTARR,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		{NULL, 0},		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ISTARRF16 */
	{
		IOPCODE_NAME("ISTARRF16")		/* pszName */
		DESC_FLAGS_MEMSTORE |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_DESTCOMPMASK |
		0,					/* uFlags */
		0,					/* uFlags2 */
		STARRF16_ARGUMENT_COUNT,					/* uArgumentCount */
		STARRF16_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_LDSTARR,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		{NULL, 0},		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ISTARRU8 */
	{
		IOPCODE_NAME("ISTARRU8")		/* pszName */
		DESC_FLAGS_MEMSTORE |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_DESTCOMPMASK |
		0,					/* uFlags */
		0,					/* uFlags2 */
		STARRU8_ARGUMENT_COUNT,					/* uArgumentCount */
		STARRU8_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_LDSTARR,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		{NULL, 0},		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ISTARRC10 */
	{
		IOPCODE_NAME("ISTARRC10")		/* pszName */
		DESC_FLAGS_MEMSTORE |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_DESTCOMPMASK |
		0,					/* uFlags */
		0,					/* uFlags2 */
		STARRC10_ARGUMENT_COUNT,					/* uArgumentCount */
		STARRC10_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_LDSTARR,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		{NULL, 0},		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},

	#if defined(OUTPUT_USPBIN)
	/*
		Special sample instructions for use with the USP	
	*/
	/* ISMP_USP */
	{
		IOPCODE_NAME("ISMP_USP")		/* pszName */
		DESC_FLAGS_TEXTURESAMPLE |
		DESC_FLAGS_REQUIRESGRADIENTS |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_DEPENDENTTEXTURESAMPLE |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_USPTEXTURESAMPLE |
		0,					/* uFlags */
		0,					/* uFlags2 */
		SMP_MAXIMUM_ARG_COUNT,	/* uArgumentCount */
		SMP_MAXIMUM_ARG_COUNT,	/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{SMP_COORD_ARG_START, DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_SMP,		/* eType */
		INST_PRED_USP,		/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInSMPArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfTextureSampleSourceBankRestrictons),		/* { apfCanUseSrc, uFuncCount } */
		CanUseUSPSampleDest,		/* pfCanUseDest */
	},
	/* ISMPBIAS_USP */
	{
		IOPCODE_NAME("ISMPBIAS_USP")	/* pszName */
		DESC_FLAGS_TEXTURESAMPLE |
		DESC_FLAGS_REQUIRESGRADIENTS |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_DEPENDENTTEXTURESAMPLE |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_USPTEXTURESAMPLE |
		0,					/* uFlags */
		0,					/* uFlags2 */
		SMP_MAXIMUM_ARG_COUNT,	/* uArgumentCount */
		SMP_MAXIMUM_ARG_COUNT,	/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{SMP_COORD_ARG_START, DESC_ARGREMAP_DONTCARE, SMP_LOD_ARG_START},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_SMP,		/* eType */
		INST_PRED_USP,		/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInSMPArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfTextureSampleSourceBankRestrictons),		/* { apfCanUseSrc, uFuncCount } */
		CanUseUSPSampleDest,		/* pfCanUseDest */
	},
	/* ISMPREPLACE_USP */
	{
		IOPCODE_NAME("ISMPREPLACE_USP")/* pszName */
		DESC_FLAGS_TEXTURESAMPLE |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_DEPENDENTTEXTURESAMPLE |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_USPTEXTURESAMPLE |
		0,					/* uFlags */
		0,					/* uFlags2 */
		SMP_MAXIMUM_ARG_COUNT,	/* uArgumentCount */
		SMP_MAXIMUM_ARG_COUNT,	/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{SMP_COORD_ARG_START, DESC_ARGREMAP_DONTCARE, SMP_LOD_ARG_START},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_SMP,		/* eType */
		INST_PRED_USP,		/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInSMPArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfTextureSampleSourceBankRestrictons),		/* { apfCanUseSrc, uFuncCount } */
		CanUseUSPSampleDest,		/* pfCanUseDest */
	},
	/* ISMPGRAD_USP */
	{
		IOPCODE_NAME("ISMPGRAD_USP")	/* pszName */
		DESC_FLAGS_TEXTURESAMPLE |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_DEPENDENTTEXTURESAMPLE |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_USPTEXTURESAMPLE |
		0,					/* uFlags */
		0,					/* uFlags2 */
		SMP_MAXIMUM_ARG_COUNT,	/* uArgumentCount */
		SMP_MAXIMUM_ARG_COUNT,	/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{SMP_COORD_ARG_START, DESC_ARGREMAP_DONTCARE, SMP_GRAD_ARG_START},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		UINT_MAX,		/* uUseasmOpcode */
		INST_TYPE_SMP,		/* eType */
		INST_PRED_USP,		/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInSMPArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfTextureSampleSourceBankRestrictons),		/* { apfCanUseSrc, uFuncCount } */
		CanUseUSPSampleDest,		/* pfCanUseDest */
	},
	/* ISMP_USP_NDR */
	{
		IOPCODE_NAME("ISMP_USP_NDR")	/* pszName */
		DESC_FLAGS_TEXTURESAMPLE |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_USPTEXTURESAMPLE |
		0,					/* uFlags */
		0,					/* uFlags2 */
		SMP_MAXIMUM_ARG_COUNT, /* uArgumentCount */
		SMP_MAXIMUM_ARG_COUNT,	/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_SMP,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInSMPUSPNDRArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSMP_USP_NDRSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		CanUseUSPSampleDest,		/* pfCanUseDest */
	},
	/* ISMPUNPACK_USP */
	{
		IOPCODE_NAME("ISMPUNPACK_USP")		/* pszName */		
		DESC_FLAGS_SUPPORTSKIPINV |		
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_MULTIPLEDEST |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		UF_MAX_CHUNKS_PER_TEXTURE * 2,	/* uArgumentCount */
		UF_MAX_CHUNKS_PER_TEXTURE * 2,	/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_SMPUNPACK,/* eType */
		INST_PRED_USP,		/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInSMPUNPACKArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSMPUNPACK_USPSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},	
	/* ITEXWRITE */
	{
		IOPCODE_NAME("ITEXWRITE")			/* pszName */
		0,					/* uFlags */
		0,					/* uFlags2 */
		8,					/* uArgumentCount */
		8,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_TEXWRITE,	/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,					/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfTexwriteSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	#endif /* #if defined(OUTPUT_USPBIN) */

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/* IUNPCKVEC */
	{
		IOPCODE_NAME("IUNPCKVEC")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORSRC |
		0,					/* uFlags */
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(1),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(1),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		{NULL, 0},		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVMAD */
	{
		IOPCODE_NAME("IVMAD")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(3),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(3),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{VEC_MOESOURCE_ARGINDEX(0), VEC_MOESOURCE_ARGINDEX(1), VEC_MOESOURCE_ARGINDEX(2)},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VMAD,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_VECTORSHORT,	/* ePredSupport */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfVMADOrVMOVCSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVMAD4 */
	{
		IOPCODE_NAME("IVMAD4")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_WIDEVECTORSOURCE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_VECTORF32ONLY |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(3),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(3),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		SGXVEC_USE1_SVEC_RCNT_MAXIMUM,	              	
							/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_SRC1ALIAS},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VMAD4,	/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_VECTOREXTENDED,	/* ePredSupport */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfVMAD4SourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVMUL */
	{
		IOPCODE_NAME("IVMUL")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_WIDEVECTORSOURCE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_DESTMASKNOSRC0 |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{VEC_MOESOURCE_ARGINDEX(1), VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_SRC1ALIAS},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VMUL,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_VECTOREXTENDED,	/* ePredSupport */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfTwoSourceSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVMUL3 */
	{
		IOPCODE_NAME("IVMUL3")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_WIDEVECTORSOURCE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		SGXVEC_USE1_SVEC_RCNT_MAXIMUM,	              	
							/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_SRC1ALIAS},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VMAD3,	/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_VECTOREXTENDED,	/* ePredSupport */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfVMUL3SourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVADD */
	{
		IOPCODE_NAME("IVADD")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_WIDEVECTORSOURCE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_DESTMASKNOSRC0 |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{VEC_MOESOURCE_ARGINDEX(1), VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_SRC1ALIAS},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VADD,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_VECTOREXTENDED,	/* ePredSupport */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfTwoSourceSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVADD3 */
	{
		IOPCODE_NAME("IVADD3")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_WIDEVECTORSOURCE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		SGXVEC_USE1_SVEC_RCNT_MAXIMUM,	              	
							/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_SRC1ALIAS},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VMAD3,	/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_VECTOREXTENDED,	/* ePredSupport */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfVADD3SourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVFRC */
	{
		IOPCODE_NAME("IVFRC")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_WIDEVECTORSOURCE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_DESTMASKNOSRC0 |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{VEC_MOESOURCE_ARGINDEX(1), VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_SRC1ALIAS},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VFRC,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_VECTOREXTENDED,	/* ePredSupport */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfTwoSourceSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVTRC */
	{
		IOPCODE_NAME("IVTRC")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_WIDEVECTORSOURCE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_DESTMASKNOSRC0 |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(1),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(1),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_VECTOREXTENDED,	/* ePredSupport */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfOneSourceSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVDSX */
	{
		IOPCODE_NAME("IVDSX")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		DESC_FLAGS_REQUIRESGRADIENTS |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_DESTANDSRCOVERLAP |
		DESC_FLAGS2_WIDEVECTORSOURCE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_DESTMASKNOSRC0 |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(1),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(1),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_VECTOREXTENDED,	/* ePredSupport */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfVDSXOrVDSYSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVDSY */
	{
		IOPCODE_NAME("IVDSY")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		DESC_FLAGS_REQUIRESGRADIENTS |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_DESTANDSRCOVERLAP |
		DESC_FLAGS2_WIDEVECTORSOURCE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_DESTMASKNOSRC0 |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(1),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(1),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_VECTOREXTENDED,	/* ePredSupport */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfVDSXOrVDSYSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVDSX_EXP */
	{
		IOPCODE_NAME("IVDSX_EXP")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		DESC_FLAGS_REQUIRESGRADIENTS |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_DESTANDSRCOVERLAP |
		DESC_FLAGS2_WIDEVECTORSOURCE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_DESTMASKNOSRC0 |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{VEC_MOESOURCE_ARGINDEX(1), VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_SRC1ALIAS},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VDSX,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_VECTOREXTENDED,	/* ePredSupport */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfVDSXOrVDSYSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVDSY_EXP */
	{
		IOPCODE_NAME("IVDSY_EXP")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		DESC_FLAGS_REQUIRESGRADIENTS |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_DESTANDSRCOVERLAP |
		DESC_FLAGS2_WIDEVECTORSOURCE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_DESTMASKNOSRC0 |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{VEC_MOESOURCE_ARGINDEX(1), VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_SRC1ALIAS},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VDSY,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_VECTOREXTENDED,	/* ePredSupport */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfVDSXOrVDSYSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVMIN */
	{
		IOPCODE_NAME("IVMIN")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_WIDEVECTORSOURCE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_DESTMASKNOSRC0 |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{VEC_MOESOURCE_ARGINDEX(1), VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_SRC1ALIAS},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VMIN,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_VECTOREXTENDED,	/* ePredSupport */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfTwoSourceSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVMAX */
	{
		IOPCODE_NAME("IVMAX")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_WIDEVECTORSOURCE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_DESTMASKNOSRC0 |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{VEC_MOESOURCE_ARGINDEX(1), VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_SRC1ALIAS},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VMAX,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_VECTOREXTENDED,	/* ePredSupport */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfTwoSourceSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVDP */
	{
		IOPCODE_NAME("IVDP")				/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_WIDEVECTORSOURCE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_DESTMASKNOSRC0 |
		DESC_FLAGS2_VECTORREPLICATEDRESULT |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{VEC_MOESOURCE_ARGINDEX(1), VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_SRC1ALIAS},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VDP,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_VECTOREXTENDED,	/* ePredSupport */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfTwoSourceSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVDP2 */
	{
		IOPCODE_NAME("IVDP2")				/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_WIDEVECTORSOURCE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_VECTORREPLICATEDRESULT |
		DESC_FLAGS2_VECTORF32ONLY |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		(IMG_UINT32)-1,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_VECTOREXTENDED,	/* ePredSupport */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfTwoSourceSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVDP3 */
	{
		IOPCODE_NAME("IVDP3")				/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_WIDEVECTORSOURCE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_VECTORREPLICATEDRESULT |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_VECTOREXTENDED,	/* ePredSupport */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfTwoSourceSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVDP_GPI */
	{
		IOPCODE_NAME("IVDP_GPI")				/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_WIDEVECTORSOURCE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_VECTORREPLICATEDRESULT |
		DESC_FLAGS2_VECTORF32ONLY |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		SGXVEC_USE1_SVEC_RCNT_MAXIMUM,
							/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_SRC1ALIAS},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VDP4,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_VECTOREXTENDED,	/* ePredSupport */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfVDP_GPISourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVDP3_GPI */
	{
		IOPCODE_NAME("IVDP3_GPI")				/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_WIDEVECTORSOURCE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_VECTORREPLICATEDRESULT |
		DESC_FLAGS2_VECTORF32ONLY |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		SGXVEC_USE1_SVEC_RCNT_MAXIMUM,
							/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_SRC1ALIAS},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VDP3,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_VECTOREXTENDED,	/* ePredSupport */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfVDP_GPISourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVDP_CP */
	{
		IOPCODE_NAME("IVDP_CP")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_WIDEVECTORSOURCE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_VECTORREPLICATEDRESULT |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		SGXVEC_USE1_SVEC_RCNT_MAXIMUM,	              	
							/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_SRC1ALIAS},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VDP4,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_VECTOREXTENDED,	/* ePredSupport */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfVDP_GPISourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVMOV */
	{
		IOPCODE_NAME("IVMOV")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(1),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(1),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		SGXVEC_USE1_VMOVC_RCNT_MAXIMUM,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfOneSourceSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},	
	/* IVMOV_EXP */
	{
		IOPCODE_NAME("IVMOV_EXP")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(1),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(1),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		SGXVEC_USE1_VMOVC_RCNT_MAXIMUM,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VMOV,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		SGXVEC_USE0_VMOVC_VEC_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE0_VMOVC_VEC_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfOneSourceSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},	
	/* IVMOVC */
	{
		IOPCODE_NAME("IVMOVC")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(3),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(3),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		SGXVEC_USE1_VMOVC_RCNT_MAXIMUM,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{VEC_MOESOURCE_ARGINDEX(0), VEC_MOESOURCE_ARGINDEX(1), VEC_MOESOURCE_ARGINDEX(2)},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VMOVC,	/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		SGXVEC_USE0_VMOVC_VEC_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE0_VMOVC_VEC_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfVMADOrVMOVCSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVMOVCU8_FLT */
	{
		IOPCODE_NAME("IVMOVCU8_FLT")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(3),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(3),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		SGXVEC_USE1_VMOVC_RCNT_MAXIMUM,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{VEC_MOESOURCE_ARGINDEX(0), VEC_MOESOURCE_ARGINDEX(1), VEC_MOESOURCE_ARGINDEX(2)},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VMOVCU8,	/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		SGXVEC_USE0_VMOVC_VEC_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE0_VMOVC_VEC_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfVMADOrVMOVCSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVMOVCBIT */
	{
		IOPCODE_NAME("IVMOVCBIT")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(3),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(3),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		SGXVEC_USE1_VMOVC_RCNT_MAXIMUM,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		SGXVEC_USE0_VMOVC_VEC_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE0_VMOVC_VEC_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfVMADOrVMOVCSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVMOVC_I32 */
	{
		IOPCODE_NAME("IVMOVC_I32")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		MOVC_ARGUMENT_COUNT,					/* uArgumentCount */
		MOVC_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, 1, 2},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VMOVC,	/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		SGXVEC_USE0_VMOVC_NONVEC_REGISTER_NUMBER_FIELD_LENGTH,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE0_VMOVC_NONVEC_REGISTER_NUMBER_FIELD_LENGTH,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVMOVCU8_I32 */
	{
		IOPCODE_NAME("IVMOVCU8_I32")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		MOVC_ARGUMENT_COUNT,					/* uArgumentCount */
		MOVC_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, 1, 2},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VMOVCU8,	/* uUseasmOpcode */
		INST_TYPE_MOVC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		SGXVEC_USE0_VMOVC_NONVEC_REGISTER_NUMBER_FIELD_LENGTH,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE0_VMOVC_NONVEC_REGISTER_NUMBER_FIELD_LENGTH,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVRSQ */
	{
		IOPCODE_NAME("IVRSQ")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORCOMPLEXOP |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_VECTORREPLICATEDRESULT |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(1),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(1),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		SGXVEC_USE1_VECCOMPLEXOP_RCNT_MAXIMUM,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VRSQ,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		SGXVEC_USE0_VECCOMPLEXOP_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE0_VECCOMPLEXOP_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfOneSourceSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVRCP */
	{
		IOPCODE_NAME("IVRCP")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORCOMPLEXOP |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_VECTORREPLICATEDRESULT |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(1),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(1),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		SGXVEC_USE1_VECCOMPLEXOP_RCNT_MAXIMUM,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VRCP,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		SGXVEC_USE0_VECCOMPLEXOP_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE0_VECCOMPLEXOP_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfOneSourceSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVLOG */
	{
		IOPCODE_NAME("IVLOG")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORCOMPLEXOP |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE | 
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_VECTORREPLICATEDRESULT |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(1),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(1),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		SGXVEC_USE1_VECCOMPLEXOP_RCNT_MAXIMUM,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VLOG,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		SGXVEC_USE0_VECCOMPLEXOP_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE0_VECCOMPLEXOP_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfOneSourceSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVEXP */
	{
		IOPCODE_NAME("IVEXP")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORCOMPLEXOP |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_VECTORREPLICATEDRESULT |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(1),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(1),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		SGXVEC_USE1_VECCOMPLEXOP_RCNT_MAXIMUM,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VEXP,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		SGXVEC_USE0_VECCOMPLEXOP_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE0_VECCOMPLEXOP_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfOneSourceSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVTEST */
	{
		IOPCODE_NAME("IVTEST")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_WIDEVECTORSOURCE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_TEST |
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{VEC_MOESOURCE_ARGINDEX(1), VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_SRC1ALIAS},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		_SGXVEC_USE_TEST_VEC_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		_SGXVEC_USE_TEST_VEC_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfTwoSourceSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVTESTMASK */
	{
		IOPCODE_NAME("IVTESTMASK")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_WIDEVECTORSOURCE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_TESTMASK |
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{VEC_MOESOURCE_ARGINDEX(1), VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_SRC1ALIAS},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		_SGXVEC_USE_TEST_VEC_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		_SGXVEC_USE_TEST_VEC_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfTwoSourceSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVTESTBYTEMASK */
	{
		IOPCODE_NAME("IVTESTBYTEMASK")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_WIDEVECTORSOURCE |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{VEC_MOESOURCE_ARGINDEX(1), VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_SRC1ALIAS},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		_SGXVEC_USE_TEST_VEC_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		_SGXVEC_USE_TEST_VEC_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfTwoSourceSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		CanUseVTESTBYTEMASKDest,		/* pfCanUseDest */
	},
	/* IVDUAL */
	{
		IOPCODE_NAME("IVDUAL")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_WIDEVECTORSOURCE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(VDUAL_SRCSLOT_COUNT),	/* uArgumentCount */
		VEC_ARGUMENT_COUNT(VDUAL_SRCSLOT_COUNT),	/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, VEC_MOESOURCE_ARGINDEX(VDUAL_SRCSLOT_UNIFIEDSTORE), DESC_ARGREMAP_SRC1ALIAS},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VMAD,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_VECTORSHORT,	/* ePredSupport */
		SGXVEC_USE_DVEC_USSRC_SCALAR_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_DVEC_USSRC_SCALAR_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfVDUALSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVPCKU8U8 */
	{
		IOPCODE_NAME("IVPCKU8U8")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORPACK |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		VPCK_NONVEC_ARGUMENT_COUNT,					/* uArgumentCount */
		VPCK_NONVEC_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VPCKU8U8,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVPCKU8U8Argument,	/* pfGetLiveChansInArg */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVPCKU8FLT */
	{
		IOPCODE_NAME("IVPCKU8FLT")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORPACK |
		DESC_FLAGS_VECTORSRC |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(1),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(1),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfOneSourceSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVPCKS8FLT */
	{
		IOPCODE_NAME("IVPCKS8FLT")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORPACK |
		DESC_FLAGS_VECTORSRC |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(1),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(1),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfOneSourceSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVPCKU8FLT_EXP */
	{
		IOPCODE_NAME("IVPCKU8FLT_EXP")	/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORPACK |
		DESC_FLAGS_VECTORSRC |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, VEC_MOESOURCE_ARGINDEX(0), VEC_MOESOURCE_ARGINDEX(1)},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfVPCKSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVPCKC10FLT */
	{
		IOPCODE_NAME("IVPCKC10FLT")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORPACK |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_C10VECTORDEST |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(1),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(1),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		USC_OPT_GROUP_INTEGER_PEEPHOLE,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfOneSourceSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVPCKC10FLT_EXP */
	{
		IOPCODE_NAME("IVPCKC10FLT_EXP")	/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORPACK |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_C10VECTORDEST |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, VEC_MOESOURCE_ARGINDEX(0), VEC_MOESOURCE_ARGINDEX(1)},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfVPCKSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVPCKC10C10 */
	{
		IOPCODE_NAME("IVPCKC10C10")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORPACK |
		DESC_FLAGS_C10VECTORDEST |
		DESC_FLAGS_C10VECTORSRCS |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		0,					/* uFlags2 */
		C10VEC_ARGUMENT_COUNT(1),					/* uArgumentCount */
		C10VEC_ARGUMENT_COUNT(1),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, C10VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInC10Arg,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVPCKFLTU8 */
	{
		IOPCODE_NAME("IVPCKFLTU8")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORPACK |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		VPCK_NONVEC_ARGUMENT_COUNT,					/* uArgumentCount */
		VPCK_NONVEC_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
  									/* uOptimizationGroup */
		GetLiveChansInVPCKFixedPointToFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVPCKFLTS8 */
	{
		IOPCODE_NAME("IVPCKFLTS8")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORPACK |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		VPCK_NONVEC_ARGUMENT_COUNT,					/* uArgumentCount */
		VPCK_NONVEC_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVPCKFixedPointToFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVPCKFLTU16 */
	{
		IOPCODE_NAME("IVPCKFLTU16")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORPACK |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		VPCK_NONVEC_ARGUMENT_COUNT,					/* uArgumentCount */
		VPCK_NONVEC_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVPCKFixedPointToFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVPCKFLTS16 */
	{
		IOPCODE_NAME("IVPCKFLTS16")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORPACK |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		VPCK_NONVEC_ARGUMENT_COUNT,					/* uArgumentCount */
		VPCK_NONVEC_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVPCKFixedPointToFloatArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVPCKFLTC10 */
	{
		IOPCODE_NAME("IVPCKFLTC10")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORPACK |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_MULTIPLEDEST | 
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		VPCK_NONVEC_ARGUMENT_COUNT,					/* uArgumentCount */
		VPCK_NONVEC_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVPCKFLTC10_VEC */
	{
		IOPCODE_NAME("IVPCKFLTC10_VEC")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORPACK |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_MULTIPLEDEST |
		DESC_FLAGS_C10VECTORSRCS |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		0,					/* uFlags2 */
		C10VEC_ARGUMENT_COUNT(1),					/* uArgumentCount */
		C10VEC_ARGUMENT_COUNT(1),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, C10VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVPCKFLTFLT */
	{
		IOPCODE_NAME("IVPCKFLTFLT")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORPACK |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(1),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(1),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
  						/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfOneSourceSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVPCKFLTFLT_EXP */
	{
		IOPCODE_NAME("IVPCKFLTFLT_EXP")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORPACK |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, VEC_MOESOURCE_ARGINDEX(0), VEC_MOESOURCE_ARGINDEX(1)},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfVPCKSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVPCKS16FLT */
	{
		IOPCODE_NAME("IVPCKS16FLT")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORPACK |
		DESC_FLAGS_VECTORSRC |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(1),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(1),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfOneSourceSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVPCKU16FLT */
	{
		IOPCODE_NAME("IVPCKU16FLT")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORPACK |
		DESC_FLAGS_VECTORSRC |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(1),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(1),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfOneSourceSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVPCKS16FLT_EXP */
	{
		IOPCODE_NAME("IVPCKS16FLT_EXP")	/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORPACK |
		DESC_FLAGS_VECTORSRC |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, VEC_MOESOURCE_ARGINDEX(0), VEC_MOESOURCE_ARGINDEX(1)},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VPCKS16F32,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfVPCKSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVPCKS16FLT_EXP */
	{
		IOPCODE_NAME("IVPCKU16FLT_EXP")	/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORPACK |
		DESC_FLAGS_VECTORSRC |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, VEC_MOESOURCE_ARGINDEX(0), VEC_MOESOURCE_ARGINDEX(1)},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VPCKU16F32,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfVPCKSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVPCKU16U16 */
	{
		IOPCODE_NAME("IVPCKU16U16")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORPACK |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		VPCK_NONVEC_ARGUMENT_COUNT,					/* uArgumentCount */
		VPCK_NONVEC_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VPCKU16U16,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVPCKS16S8 */
	{
		IOPCODE_NAME("IVPCKS16S8")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORPACK |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		VPCK_NONVEC_ARGUMENT_COUNT,					/* uArgumentCount */
		VPCK_NONVEC_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VPCKS16S8,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVPCKU16U8 */
	{
		IOPCODE_NAME("IVPCKU16U8")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORPACK |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		VPCK_NONVEC_ARGUMENT_COUNT,					/* uArgumentCount */
		VPCK_NONVEC_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VPCKU16U8,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ILDAQ */
	{
		IOPCODE_NAME("ILDAQ")			/* pszName */
		DESC_FLAGS_MEMLOAD |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_ABSOLUTELOADSTORE |
		0,					/* uFlags2 */
		MEMLOAD_ARGUMENT_COUNT,					/* uArgumentCount */
		MEMLOAD_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{MEMLOADSTORE_BASE_ARGINDEX, MEMLOADSTORE_OFFSET_ARGINDEX, MEMLOAD_RANGE_ARGINDEX},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_LDAQ,		/* uUseasmOpcode */
		INST_TYPE_LDST,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfMemLoadSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		CanUseExternalWBDest,		/* pfCanUseDest */
	},
	/* IMKC10VEC */
	{
		IOPCODE_NAME("IMKC10VEC")		/* pszName */
		0,					/* uFlags */
		0,					/* uFlags2 */
		2,					/* uArgumentCount */
		2,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ISOPWM_VEC */
	{
		IOPCODE_NAME("ISOPWM_VEC")			/* pszName */
		DESC_FLAGS_C10FMTCTL |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_MULTIPLEDEST |
		DESC_FLAGS_C10VECTORSRCS |
		DESC_FLAGS_C10VECTORDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		0,					/* uFlags2 */
		C10VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		C10VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, C10VEC_MOESOURCE_ARGINDEX(0), C10VEC_MOESOURCE_ARGINDEX(1)},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_SOP2WM,	/* uUseasmOpcode */
		INST_TYPE_SOPWM,	/* eType */
		INST_PRED_SHORT,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInC10Arg,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ISOP2_VEC */
	{
		IOPCODE_NAME("ISOP2_VEC")			/* pszName */
		DESC_FLAGS_C10FMTCTL |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_MULTIPLEDEST |
		DESC_FLAGS_C10VECTORDEST |
		DESC_FLAGS_C10VECTORSRCS |
		0,					/* uFlags */
		0,					/* uFlags2 */
		C10VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		C10VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, C10VEC_MOESOURCE_ARGINDEX(0), C10VEC_MOESOURCE_ARGINDEX(1)},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_SOP2,		/* uUseasmOpcode */
		INST_TYPE_SOP2,		/* eType */
		INST_PRED_SHORT,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInC10Arg,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ISOP3_VEC */
	{
		IOPCODE_NAME("ISOP3_VEC")		/* pszName */
		DESC_FLAGS_C10FMTCTL |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_MULTIPLEDEST |
		DESC_FLAGS_C10VECTORDEST |
		DESC_FLAGS_C10VECTORSRCS |
		0,					/* uFlags */
		0,					/* uFlags2 */
		C10VEC_ARGUMENT_COUNT(3),					/* uArgumentCount */
		C10VEC_ARGUMENT_COUNT(3),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{C10VEC_MOESOURCE_ARGINDEX(0), C10VEC_MOESOURCE_ARGINDEX(1), C10VEC_MOESOURCE_ARGINDEX(2)},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_SOP3,		/* uUseasmOpcode */
		INST_TYPE_SOP3,		/* eType */
		INST_PRED_SHORT,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInC10Arg,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSOP3VecBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ILRP1_VEC */
	{
		IOPCODE_NAME("ILRP1_VEC")		/* pszName */
		DESC_FLAGS_C10FMTCTL |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_MULTIPLEDEST |
		DESC_FLAGS_C10VECTORDEST |
		DESC_FLAGS_C10VECTORSRCS |
		0,					/* uFlags */
		0,					/* uFlags2 */
		C10VEC_ARGUMENT_COUNT(3),					/* uArgumentCount */
		C10VEC_ARGUMENT_COUNT(3),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{C10VEC_MOESOURCE_ARGINDEX(0), C10VEC_MOESOURCE_ARGINDEX(1), C10VEC_MOESOURCE_ARGINDEX(2)},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_LRP1,		/* uUseasmOpcode */
		INST_TYPE_LRP1,		/* eType */
		INST_PRED_SHORT,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInC10Arg,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfLRP1VecBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFPMA_VEC */
	{
		IOPCODE_NAME("IFPMA_VEC")			/* pszName */
		DESC_FLAGS_C10FMTCTL |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_MULTIPLEDEST |
		DESC_FLAGS_C10VECTORDEST |
		DESC_FLAGS_C10VECTORSRCS |
		0,					/* uFlags */
		0,					/* uFlags2 */
		C10VEC_ARGUMENT_COUNT(3),					/* uArgumentCount */
		C10VEC_ARGUMENT_COUNT(3),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_INT_RCOUNT_MAXIMUM,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{C10VEC_MOESOURCE_ARGINDEX(0), C10VEC_MOESOURCE_ARGINDEX(1), C10VEC_MOESOURCE_ARGINDEX(2)},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FPMA,		/* uUseasmOpcode */
		INST_TYPE_FPMA,		/* eType */
		INST_PRED_SHORT,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInC10Arg,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfFPMAVecBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFPADD8_VEC */
	{
		IOPCODE_NAME("IFPADD8_VEC")			/* pszName */
		DESC_FLAGS_C10FMTCTL |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_C10VECTORSRCS |
		0,					/* uFlags */
		0,					/* uFlags2 */
		C10VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		C10VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, C10VEC_MOESOURCE_ARGINDEX(0), C10VEC_MOESOURCE_ARGINDEX(1)},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FPADD8,	/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInC10Arg,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfFixedPointVecTestSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFPSUB8_VEC */
	{
		IOPCODE_NAME("IFPSUB8_VEC")			/* pszName */
		DESC_FLAGS_C10FMTCTL |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_C10VECTORSRCS |
		0,					/* uFlags */
		0,					/* uFlags2 */
		C10VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		C10VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	    /* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, C10VEC_MOESOURCE_ARGINDEX(0), C10VEC_MOESOURCE_ARGINDEX(1)},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_FPSUB8,	/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInC10Arg,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfFixedPointVecTestSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFPDOT_VEC */
	{
		IOPCODE_NAME("IFPDOT_VEC")			/* pszName */
		DESC_FLAGS_C10FMTCTL |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_MULTIPLEDEST |
		DESC_FLAGS_C10VECTORDEST |
		DESC_FLAGS_C10VECTORSRCS |
		0,					/* uFlags */
		0,					/* uFlags2 */
		C10VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		C10VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_INT_RCOUNT_MAXIMUM,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, C10VEC_MOESOURCE_ARGINDEX(0), C10VEC_MOESOURCE_ARGINDEX(1)},
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_DOT34,	/* eType */
		INST_PRED_SHORT,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInC10Arg,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFPTESTPRED_VEC */
	{
		IOPCODE_NAME("IFPTESTPRED_VEC")			/* pszName */
		DESC_FLAGS_C10FMTCTL |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_MULTIPLEDEST |
		DESC_FLAGS_C10VECTORSRCS |
		DESC_FLAGS_C10VECTORDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_TEST |
		DESC_FLAGS2_REPEATMASKONLY |
		0,					/* uFlags2 */
		C10VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		C10VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_TEST_MAX_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, C10VEC_MOESOURCE_ARGINDEX(0), C10VEC_MOESOURCE_ARGINDEX(1)},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_TEST,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInC10Arg,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfFixedPointVecTestSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IFPTESTMASK_VEC */
	{
		IOPCODE_NAME("IFPTESTMASK_VEC")			/* pszName */
		DESC_FLAGS_C10FMTCTL |
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_MULTIPLEDEST |
		DESC_FLAGS_C10VECTORSRCS |
		DESC_FLAGS_C10VECTORDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		DESC_FLAGS2_TESTMASK |
		DESC_FLAGS2_REPEATMASKONLY |
		0,					/* uFlags2 */
		C10VEC_ARGUMENT_COUNT(2),					/* uArgumentCount */
		C10VEC_ARGUMENT_COUNT(2),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_TEST_MAX_REPEAT,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, C10VEC_MOESOURCE_ARGINDEX(0), C10VEC_MOESOURCE_ARGINDEX(1)},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_TEST,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInC10Arg,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfFixedPointVecTestSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVMOVC10 */
	{
		IOPCODE_NAME("IVMOVC10")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_MULTIPLEDEST |
		DESC_FLAGS_C10VECTORDEST |
		DESC_FLAGS_C10VECTORSRCS |
		0,					/* uFlags */
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		0,					/* uFlags2 */
		C10VEC_ARGUMENT_COUNT(1),					/* uArgumentCount */
		C10VEC_ARGUMENT_COUNT(1),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		SGXVEC_USE1_VMOVC_RCNT_MAXIMUM,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, C10VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_DONTCARE},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VMOV,		/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		SGXVEC_USE0_VMOVC_VEC_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE0_VMOVC_VEC_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInC10Arg,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVMOVCC10 */
	{
		IOPCODE_NAME("IVMOVCC10")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_MULTIPLEDEST |
		DESC_FLAGS_C10VECTORDEST |
		DESC_FLAGS_C10VECTORSRCS |
		0,					/* uFlags */
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		0,					/* uFlags2 */
		C10VEC_ARGUMENT_COUNT(3),					/* uArgumentCount */
		C10VEC_ARGUMENT_COUNT(3),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_INT_RCOUNT_MAXIMUM,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{C10VEC_MOESOURCE_ARGINDEX(0), C10VEC_MOESOURCE_ARGINDEX(1), C10VEC_MOESOURCE_ARGINDEX(2)},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VMOVC,	/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		SGXVEC_USE0_VMOVC_VEC_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE0_VMOVC_VEC_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInC10Arg,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfFPMOVCVecBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IVMOVCU8 */
	{
		IOPCODE_NAME("IVMOVCU8")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		C10VEC_ARGUMENT_COUNT(3),					/* uArgumentCount */
		C10VEC_ARGUMENT_COUNT(3),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_INT_RCOUNT_MAXIMUM,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{C10VEC_MOESOURCE_ARGINDEX(0), C10VEC_MOESOURCE_ARGINDEX(1), C10VEC_MOESOURCE_ARGINDEX(2)},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_VMOVC,	/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		SGXVEC_USE0_VMOVC_NONVEC_REGISTER_NUMBER_FIELD_LENGTH,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE0_VMOVC_NONVEC_REGISTER_NUMBER_FIELD_LENGTH,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInC10Arg,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfFPMOVCVecBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IIADD32 */
	{
		IOPCODE_NAME("IIADD32")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |		
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		0,					/* uFlags */
		0,					/* uFlags2 */
		2,					/* uArgumentCount */
		2,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_INT_RCOUNT_MAXIMUM,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, 1, 2},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_IADD32,	/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IIADDU32 */
	{
		IOPCODE_NAME("IIADDU32")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |		
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		0,					/* uFlags */
		0,					/* uFlags2 */
		2,					/* uArgumentCount */
		2,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_INT_RCOUNT_MAXIMUM,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, 1, 2},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_IADDU32,	/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		&g_sSrc01,			/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IISUB32 */
	{
		IOPCODE_NAME("IISUB32")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |		
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		0,					/* uFlags */
		0,					/* uFlags2 */
		2,					/* uArgumentCount */
		2,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_INT_RCOUNT_MAXIMUM,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, 1, 2},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_ISUB32,	/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IISUBU32 */
	{
		IOPCODE_NAME("IISUBU32")		/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |		
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_HASSOURCEMODIFIERS |
		0,					/* uFlags */
		0,					/* uFlags2 */
		2,					/* uArgumentCount */
		2,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_INT_RCOUNT_MAXIMUM,		/* uMaxRepeatCount */
		IMG_TRUE,			/* bCanUseRepeatMask */
		{0, 1, 2},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_ISUBU32,	/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/* IWOP */
	{
		IOPCODE_NAME("IWOP")				/* pszName */
		0,					/* uFlags */
		DESC_FLAGS2_DOESNTSUPPORTENDFLAG |
		0,					/* uFlags2 */
		WOP_ARGUMENT_COUNT,					/* uArgumentCount */
		WOP_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_WOP,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},

	/* IISUB8 */
	{
		IOPCODE_NAME("IISUB8")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		2,					/* uArgumentCount */
		2,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_INT_RCOUNT_MAXIMUM,	    /* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_ISUB8,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_SHORT,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfIntegerTestSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},

	/* IISUBU8 */
	{
		IOPCODE_NAME("IISUBU8")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		2,					/* uArgumentCount */
		2,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_INT_RCOUNT_MAXIMUM,	    /* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_ISUBU8,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_SHORT,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfIntegerTestSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},

	/* IIADD8 */
	{
		IOPCODE_NAME("IIADD8")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		2,					/* uArgumentCount */
		2,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_INT_RCOUNT_MAXIMUM,	    /* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_IADD8,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_SHORT,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfIntegerTestSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},

	/* IIADDU8 */
	{
		IOPCODE_NAME("IIADDU8")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		2,					/* uArgumentCount */
		2,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_INT_RCOUNT_MAXIMUM,	    /* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_IADDU8,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_SHORT,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfIntegerTestSourceBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},

	/* IISUB16 */
	{
		IOPCODE_NAME("IISUB16")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		2,					/* uArgumentCount */
		2,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_INT_RCOUNT_MAXIMUM,	    /* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_ISUB16,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_SHORT,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},

	/* IISUBU16 */
	{
		IOPCODE_NAME("IISUBU16")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		2,					/* uArgumentCount */
		2,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_INT_RCOUNT_MAXIMUM,	    /* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_ISUBU16,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_SHORT,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	
	/* IIADD16 */
	{
		IOPCODE_NAME("IIADD16")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_USESHWSRC0 |
		DESC_FLAGS_SUPPORTSNOSCHED |
		0,					/* uFlags */
		0,					/* uFlags2 */
		2,					/* uArgumentCount */
		2,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE1_INT_RCOUNT_MAXIMUM,	    /* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 1},			/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		USEASM_OP_IADD16,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_SHORT,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfSrc0StandardElseSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},

	/* IEMIT */
	{
		IOPCODE_NAME("IEMIT")			/* pszName */
		0,					/* uFlags */
		0,					/* uFlags2 */
		EMIT_ARGUMENT_COUNT,					/* uArgumentCount */
		EMIT_ARGUMENT_COUNT,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_EMITMTEVERTEX,	/* uUseasmOpcode */
		INST_TYPE_EMIT,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/* IIDXSCR */
	{
		IOPCODE_NAME("IIDXSCR")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		0,					/* uFlags */
		0,					/* uFlags2 */
		IDXSCR_ARGUMENT_COUNT,					/* uArgumentCount */
		0,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_IDXSCR,	/* uUseasmOpcode */
		INST_TYPE_IDXSC,	/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},

	/* IIDXSCW */
	{
		IOPCODE_NAME("IIDXSCW")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		0,					/* uFlags */
		0,					/* uFlags2 */
		IDXSCW_ARGUMENT_COUNT,					/* uArgumentCount */
		0,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, IDXSCW_DATATOWRITE_ARGINDEX, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_IDXSCW,	/* uUseasmOpcode */
		INST_TYPE_IDXSC,	/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,					/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/* ICVTFLT2INT */
	{
		IOPCODE_NAME("ICVTFLT2INT")			/* pszName */
		0,					/* uFlags */
		0,					/* uFlags2 */
		CVTFLT2INT_ARGUMENT_COUNT,					/* uArgumentCount */
		0,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_CVTFLT2INT,	/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},

	/* ICVTINT2ADDR */
	{
		IOPCODE_NAME("ICVTINT2ADDR")			/* pszName */
		0,					/* uFlags */
		0,					/* uFlags2 */
		CVTINT2ADDR_ARGUMENT_COUNT,					/* uArgumentCount */
		0,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_NONE,	/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,					/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/* ICVTFLT2ADDR */
	{
		IOPCODE_NAME("ICVTFLT2ADDR")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORPACK |
		DESC_FLAGS_VECTORSRC |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(1),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(1),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		EURASIA_USE_MAXIMUM_REPEAT,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		SGXVEC_USE_VEC2_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,
								/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	#endif //defined (SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)

	/* IFEEDBACKDRIVEREPILOG */
	{
		IOPCODE_NAME("IFEEDBACKDRIVEREPILOG")			/* pszName */
		0,					/* uFlags */
		0,					/* uFlags2 */
		1,					/* uArgumentCount */
		0,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_FEEDBACKDRIVEREPILOG,	/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,					/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},

	/* ISAVEIREG */
	{
		IOPCODE_NAME("ISAVEIREG")			/* pszName */
		0,					/* uFlags */
		0,					/* uFlags2 */
		1,					/* uArgumentCount */
		0,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,					/* uOptimizationGroup */
		GetLiveChansInSaveRestoreIRegArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IRESTOREIREG */
	{
		IOPCODE_NAME("IRESTOREIREG")			/* pszName */
		0,					/* uFlags */
		0,					/* uFlags2 */
		1,					/* uArgumentCount */
		0,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,					/* uOptimizationGroup */
		GetLiveChansInSaveRestoreIRegArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* ISAVECARRY */
	{
		IOPCODE_NAME("ISAVECARRY")			/* pszName */
		0,					/* uFlags */
		0,					/* uFlags2 */
		1,					/* uArgumentCount */
		0,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,					/* uOptimizationGroup */
		GetLiveChansInSaveRestoreIRegArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	/* IRESTORECARRY */
	{
		IOPCODE_NAME("IRESTORECARRY")			/* pszName */
		0,					/* uFlags */
		0,					/* uFlags2 */
		1,					/* uArgumentCount */
		0,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,		/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,					/* uOptimizationGroup */
		GetLiveChansInSaveRestoreIRegArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/* IVLOAD */
	{
		IOPCODE_NAME("IVLOAD")			/* pszName */
		DESC_FLAGS_SUPPORTSKIPINV |
		DESC_FLAGS_SUPPORTSNOSCHED |
		DESC_FLAGS_SUPPORTSYNCSTART |
		DESC_FLAGS_VECTOR |
		DESC_FLAGS_VECTORDEST |
		DESC_FLAGS_VECTORSRC |
		DESC_FLAGS_MULTIPLEDEST |
		0,					/* uFlags */
		DESC_FLAGS2_DESTMASKABLE |
		DESC_FLAGS2_DEST64BITMOEUNITS |
		DESC_FLAGS2_SOURCES64BITMOEUNITS |
		0,					/* uFlags2 */
		VEC_ARGUMENT_COUNT(1),					/* uArgumentCount */
		VEC_ARGUMENT_COUNT(1),					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_TRUE,			/* bCanRepeat */
		SGXVEC_USE1_VMOVC_RCNT_MAXIMUM,	              	/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID, DESC_ARGREMAP_INVALID},
							/* puMoeArgRemap */
		IMG_TRUE,			/* bCanPredicate */
		UINT_MAX,			/* uUseasmOpcode */
		INST_TYPE_VEC,		/* eType */
		INST_PRED_EXTENDED,	/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,					/* uOptimizationGroup */
		GetLiveChansInVectorArgument,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfAllSrc12Extended),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

#if defined(EXECPRED)
	/* ICNDST */
	{
		IOPCODE_NAME("ICNDST")			/* pszName */
		0,					/* uFlags */
		DESC_FLAGS2_EXECPRED |
		DESC_FLAGS2_NONMERGABLE,	/* uFlags2 */
		4,					/* uArgumentCount */
		3,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},	/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_CNDST,	/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,					/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfCNDSTOrCNDLTBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},

	/* ICNDEF */
	{
		IOPCODE_NAME("ICNDEF")			/* pszName */
		0,					/* uFlags */
		DESC_FLAGS2_EXECPRED |
		DESC_FLAGS2_NONMERGABLE,					/* uFlags2 */
		4,					/* uArgumentCount */
		3,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_CNDEF,	/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,					/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfCNDSTOrCNDLTBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},

	/* ICNDSM */
	{
		IOPCODE_NAME("ICNDSM")			/* pszName */
		0,					/* uFlags */
		DESC_FLAGS2_EXECPRED |
		DESC_FLAGS2_NONMERGABLE,					/* uFlags2 */
		4,					/* uArgumentCount */
		3,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, 3},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_CNDSM,	/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,					/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfCNDSMBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	
	/* ICNDLT */
	{
		IOPCODE_NAME("ICNDLT")			/* pszName */
		0,					/* uFlags */
		DESC_FLAGS2_EXECPRED |
		DESC_FLAGS2_NONMERGABLE,					/* uFlags2 */
		4,					/* uArgumentCount */
		3,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_CNDLT,	/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,					/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfCNDSTOrCNDLTBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},

	/* ICNDEND */
	{
		IOPCODE_NAME("ICNDEND")			/* pszName */
		0,					/* uFlags */
		DESC_FLAGS2_EXECPRED |
		DESC_FLAGS2_NONMERGABLE,					/* uFlags2 */
		2,					/* uArgumentCount */
		2,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_CNDEND,	/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,					/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfCNDENDBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},

	/* ICNDSTLOOP */
	{
		IOPCODE_NAME("ICNDSTLOOP")			/* pszName */
		0,					/* uFlags */
		DESC_FLAGS2_EXECPRED |
		DESC_FLAGS2_NONMERGABLE,					/* uFlags2 */
		4,					/* uArgumentCount */
		3,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},	/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_CNDST,	/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,					/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfCNDSTOrCNDLTBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},

	/* ICNDEFLOOP */
	{
		IOPCODE_NAME("ICNDEFLOOP")			/* pszName */
		0,					/* uFlags */
		DESC_FLAGS2_EXECPRED |
		DESC_FLAGS2_NONMERGABLE,					/* uFlags2 */
		4,					/* uArgumentCount */
		3,					/* uMoeArgumentCount */
		IMG_TRUE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, 0, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_CNDEF,	/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uDestRegisterNumberBitCount */
		EURASIA_USE_REGISTER_NUMBER_BITS,	/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,					/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		ARRAY_AND_SIZE(g_apfCNDSTOrCNDLTBankRestrictions),		/* { apfCanUseSrc, uFuncCount } */
		NULL,		/* pfCanUseDest */
	},
	
	/* IBREAK */
	{
		IOPCODE_NAME("IBREAK")			/* pszName */
		0,					/* uFlags */
		DESC_FLAGS2_EXECPRED |
		DESC_FLAGS2_NONMERGABLE,					/* uFlags2 */
		0,					/* uArgumentCount */
		0,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_CNDSM,	/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		{NULL, 0},				/* { apfCanUseSrc, uFuncCount } */
		NULL,					/* pfCanUseDest */},

	{
		IOPCODE_NAME("ICONTINUE")			/* pszName */
		0,					/* uFlags */
		DESC_FLAGS2_EXECPRED |
		DESC_FLAGS2_NONMERGABLE,					/* uFlags2 */
		0,					/* uArgumentCount */
		0,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_CNDSM,	/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		{NULL, 0},				/* { apfCanUseSrc, uFuncCount } */
		NULL,					/* pfCanUseDest */
	},

	{
		IOPCODE_NAME("IRETURN")			/* pszName */
		0,					/* uFlags */
		DESC_FLAGS2_EXECPRED |
		DESC_FLAGS2_NONMERGABLE,					/* uFlags2 */
		0,					/* uArgumentCount */
		0,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_CNDSM,	/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		{NULL, 0},				/* { apfCanUseSrc, uFuncCount } */
		NULL,					/* pfCanUseDest */
	},

	{
		IOPCODE_NAME("ILOOPTEST")			/* pszName */
		0,					/* uFlags */
		DESC_FLAGS2_EXECPRED |
		DESC_FLAGS2_NONMERGABLE,					/* uFlags2 */
		0,					/* uArgumentCount */
		0,					/* uMoeArgumentCount */
		IMG_FALSE,			/* bHasDest */
		IMG_FALSE,			/* bCanRepeat */
		0,					/* uMaxRepeatCount */
		IMG_FALSE,			/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE},
							/* puMoeArgRemap */
		IMG_FALSE,			/* bCanPredicate */
		USEASM_OP_CNDSM,	/* uUseasmOpcode */
		INST_TYPE_NONE,		/* eType */
		INST_PRED_NONE,		/* ePredSupport */
		USC_UNDEF,			/* uDestRegisterNumberBitCount */
		USC_UNDEF,			/* uSourceRegisterNumberBitCount */
		NULL,				/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault,	/* pfGetLiveChansInArgument */
		{NULL, 0},				/* { apfCanUseSrc, uFuncCount } */
		NULL,					/* pfCanUseDest */
	},
#endif /* defined(EXECPRED) */

	/* Mutex lock instruction */
	{
		IOPCODE_NAME("ILOCK")	/* pszName */
		0,						/* uFlags */
		0,						/* uFlags2 */
		1,                      /* uArgumentCount */
		0,                      /* uMoeArgumentCount */
		IMG_FALSE,				/* bHasDest */
		IMG_FALSE,				/* bCanRepeat */
		0,						/* uMaxRepeatCount */
		IMG_FALSE,				/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE},
								/* puMoeArgRemap */
		IMG_FALSE,				/* bCanPredicate */
		USEASM_OP_LOCK,			/* uUseasmOpcode */
		INST_TYPE_NONE,			/* eType */
		INST_PRED_NONE,		    /* ePredSupport */
		USC_UNDEF,				/* uDestRegisterNumberBitCount */
		USC_UNDEF,				/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault, /* pfGetLiveChansInArgument */
		{NULL, 0},				/* { apfCanUseSrc, uFuncCount } */
		NULL,					/* pfCanUseDest */
	},
	/* Mutex unlock instruction */
	{
		IOPCODE_NAME("IRELEASE")/* pszName */
		0,						/* uFlags */
		0,						/* uFlags2 */
		1,                      /* uArgumentCount */
		0,                      /* uMoeArgumentCount */
		IMG_FALSE,				/* bHasDest */
		IMG_FALSE,				/* bCanRepeat */
		0,						/* uMaxRepeatCount */
		IMG_FALSE,				/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE},
								/* puMoeArgRemap */
		IMG_FALSE,				/* bCanPredicate */
		USEASM_OP_RELEASE,		/* uUseasmOpcode */
		INST_TYPE_NONE,			/* eType */
		INST_PRED_NONE,		    /* ePredSupport */
		USC_UNDEF,				/* uDestRegisterNumberBitCount */
		USC_UNDEF,				/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		GetLiveChansInArgumentDefault, /* pfGetLiveChansInArgument */
		{NULL, 0},				/* { apfCanUseSrc, uFuncCount } */
		NULL,					/* pfCanUseDest */
	},

	/* MaxArg (Invalid with maximum arguments) */
	{
		IOPCODE_NAME("Invalid_Max_Arg")		/* pszName */
		0,						/* uFlags */
		0,						/* uFlags2 */
		USC_MAXIMUM_NONCALL_ARG_COUNT,	/* uArgumentCount */
		USC_MAXIMUM_NONCALL_ARG_COUNT,	/* uMoeArgumentCount */
		IMG_FALSE,				/* bHasDest */
		IMG_FALSE,				/* bCanRepeat */
		0,						/* uMaxRepeatCount */
		IMG_FALSE,				/* bCanUseRepeatMask */
		{DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE, DESC_ARGREMAP_DONTCARE},
								/* puMoeArgRemap */
		IMG_FALSE,				/* bCanPredicate */
		UINT_MAX,				/* uUseasmOpcode */
		INST_TYPE_NONE,			/* eType */
		INST_PRED_UNDEF,		/* ePredSupport */
		USC_UNDEF,				/* uDestRegisterNumberBitCount */
		USC_UNDEF,				/* uSourceRegisterNumberBitCount */
		NULL,					/* psCommutableSources */
		0,						/* uOptimizationGroup */
		NULL,					/* pfGetLiveChansInArgument */
		{NULL, 0},				/* { apfCanUseSrc, uFuncCount } */
		NULL,					/* pfCanUseDest */
	},
};

#define COMPARE(A,B)	if (((A) - (B)) != 0) return (IMG_INT32)((A)-(B))

typedef IMG_VOID (*PFN_INIT_INSTRUCTION)(PINTERMEDIATE_STATE psState, PINST psInst);
typedef IMG_VOID (*PFN_CLEAR_INSTRUCTION)(PINTERMEDIATE_STATE psState, PINST psInst);
typedef IMG_INT32 (*PFN_COMPARE_INST_PARAMS)(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2);

typedef struct _INIT_INSTRUCTION_JUMP_TABLE_
{
	PFN_INIT_INSTRUCTION		pfnInitInstruction;
	PFN_CLEAR_INSTRUCTION		pfnClearInstruction;
	PFN_COMPARE_INST_PARAMS		pfnCompareInstructionParams;
}INIT_INSTRUCTION_JUMP_TABLE;

static
IMG_VOID InitInstructionTypeNONE(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst);
}

static
IMG_VOID InitInstructionTypeFLOAT(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if(psInst->u.psFloat == NULL)
		psInst->u.psFloat = UscAlloc(psState, sizeof(FLOAT_PARAMS));
	memset(psInst->u.psFloat, 0, sizeof(*(psInst->u.psFloat)));
}

static
IMG_VOID InitInstructionTypeEFO(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uDestIdx;

	ASSERT(psInst->uDestCount == EFO_DEST_COUNT);
	
	for (uDestIdx = 0; uDestIdx < EFO_DEST_COUNT; uDestIdx++)
	{
		SetDestUnused(psState, psInst, uDestIdx);
	}

	if(psInst->u.psEfo == NULL)
		psInst->u.psEfo = UscAlloc(psState, sizeof(EFO_PARAMETERS));
	memset(psInst->u.psEfo, 0, sizeof(*(psInst->u.psEfo)));
}

static
IMG_VOID InitInstructionTypeSOPWM(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if(psInst->u.psSopWm == NULL)
		psInst->u.psSopWm = UscAlloc(psState, sizeof(SOPWM_PARAMS));
	memset(psInst->u.psSopWm, 0, sizeof(*(psInst->u.psSopWm)));
}

static
IMG_VOID InitInstructionTypeSOP2(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if(psInst->u.psSop2 == NULL)
		psInst->u.psSop2 = UscAlloc(psState, sizeof(SOP2_PARAMS));
	memset(psInst->u.psSop2, 0, sizeof(*(psInst->u.psSop2)));
}

static
IMG_VOID InitInstructionTypeSOP3(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if(psInst->u.psSop3 == NULL)
				psInst->u.psSop3 = UscAlloc(psState, sizeof(SOP3_PARAMS));
			memset(psInst->u.psSop3, 0, sizeof(*(psInst->u.psSop3)));
}

static
IMG_VOID InitInstructionTypeLRP1(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if(psInst->u.psLrp1 == NULL)
		psInst->u.psLrp1 = UscAlloc(psState, sizeof(LRP1_PARAMS));
	memset(psInst->u.psLrp1, 0, sizeof(*(psInst->u.psLrp1)));
}

static
IMG_VOID InitInstructionTypeFPMA(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if(psInst->u.psFpma == NULL)
		psInst->u.psFpma = UscAlloc(psState, sizeof(FPMA_PARAMS));
	memset(psInst->u.psFpma, 0, sizeof(*(psInst->u.psFpma)));
	psInst->u.psFpma->bSaturate = IMG_TRUE;
}

static
IMG_VOID InitInstructionTypeIMA32(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if(psInst->u.psIma32 == NULL)
		psInst->u.psIma32 = UscAlloc(psState, sizeof(IMA32_PARAMS));

	psInst->u.psIma32->bSigned = IMG_FALSE;
	psInst->u.psIma32->uStep = 1;
	memset(psInst->u.psIma32->abNegate, 0, sizeof(psInst->u.psIma32->abNegate));
}

static
IMG_VOID InitInstructionTypeSMP(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if(psInst->u.psSmp == NULL)
		psInst->u.psSmp = UscAlloc(psState, sizeof(SMP_PARAMS));
	memset(psInst->u.psSmp, 0, sizeof(*(psInst->u.psSmp)));
	psInst->u.psSmp->uTextureStage = UINT_MAX;

	#if defined(OUTPUT_USPBIN)
	if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
	{
		InitInstArg(&psInst->u.psSmp->sUSPSample.sTempReg);
		psInst->u.psSmp->sUSPSample.uSmpID = INVALID_SAMPLE_ID;
	}
	#endif /* defined(OUTPUT_USPBIN) */
}
#if defined(OUTPUT_USPBIN)
static
IMG_VOID InitInstructionTypeSMPUNPACK(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if(psInst->u.psSmpUnpack == NULL)
		psInst->u.psSmpUnpack = UscAlloc(psState, sizeof(SMPUNPACK_PARAMS));			
	memset(psInst->u.psSmpUnpack, 0, sizeof(*(psInst->u.psSmpUnpack)));
	psInst->u.psSmpUnpack->uSmpID = INVALID_SAMPLE_ID;
	psInst->u.psSmpUnpack->uChanMask = 0;
	InitInstArg(&(psInst->u.psSmpUnpack->sTempReg));
	psInst->u.psSmpUnpack->psTextureSample = NULL;
}
#endif /* defined(OUTPUT_USPBIN) */

static
IMG_VOID InitInstructionTypeDOT34(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if(psInst->u.psDot34 == NULL)
		psInst->u.psDot34 = UscAlloc(psState, sizeof(DOT34_PARAMS));
	memset(psInst->u.psDot34, 0, sizeof(*(psInst->u.psDot34)));
}

static
IMG_VOID InitInstructionTypeDPC(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if(psInst->u.psDpc == NULL)
		psInst->u.psDpc = UscAlloc(psState, sizeof(DPC_PARAMS));
	memset(psInst->u.psDpc, 0, sizeof(*(psInst->u.psDpc)));
}

static
IMG_VOID InitInstructionTypeLDST(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psInst->u.psLdSt == NULL)
		psInst->u.psLdSt = UscAlloc(psState, sizeof(*psInst->u.psLdSt));
	psInst->u.psLdSt->bEnableRangeCheck = IMG_FALSE;
	psInst->u.psLdSt->bBypassCache = IMG_FALSE;
	psInst->u.psLdSt->uFlags = 0;
	psInst->u.psLdSt->bPositiveOffset = IMG_TRUE;
	psInst->u.psLdSt->bPreIncrement = IMG_FALSE;
}

#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
static
IMG_VOID InitInstructionTypeNRM(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if(psInst->u.psNrm == NULL)
		psInst->u.psNrm = UscAlloc(psState, sizeof(NRM_PARAMS));
	memset(psInst->u.psNrm, 0, sizeof(*(psInst->u.psNrm)));
}
#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */

static
IMG_VOID InitInstructionTypeLOADCONST(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if(psInst->u.psLoadConst == NULL)
		psInst->u.psLoadConst = UscAlloc(psState, sizeof(LOADCONST_PARAMS));
	memset(psInst->u.psLoadConst, 0, sizeof(*(psInst->u.psLoadConst)));
}

static
IMG_VOID InitInstructionTypeLOADMEMCONST(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if(psInst->u.psLoadMemConst == NULL)
		psInst->u.psLoadMemConst = UscAlloc(psState, sizeof(LOADMEMCONST_PARAMS));
	memset(psInst->u.psLoadMemConst, 0, sizeof(*(psInst->u.psLoadMemConst)));
	psInst->u.psLoadMemConst->uFetchCount = 1;
	psInst->u.psLoadMemConst->bBypassCache = IMG_FALSE;
}

static
IMG_VOID InitInstructionTypeIMA16(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psInst->u.psIma16 == NULL)
	{
		psInst->u.psIma16 = UscAlloc(psState, sizeof(*psInst->u.psIma16));
	}
	psInst->u.psIma16->bNegateSource2 = IMG_FALSE;
}

static
IMG_VOID InitInstructionTypeIMAE(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		Main and carry destinations.
	*/
	ASSERT(psInst->uDestCount == 2);

	/*
		Default to no carry-out
	*/
	psInst->asDest[IMAE_CARRYOUT_DEST].uType = USC_REGTYPE_UNUSEDDEST;

	/*
		Default to no carry-in.
	*/
	psInst->asArg[IMAE_CARRYIN_SOURCE].uType = USC_REGTYPE_UNUSEDSOURCE;

	/*
		Set default IMAE parameters.
	*/
	if (psInst->u.psImae == NULL)
		psInst->u.psImae = UscAlloc(psState, sizeof(IMAE_PARAMS));
	psInst->u.psImae->uSrc2Type = USEASM_INTSRCSEL_U32;
	psInst->u.psImae->bSigned = IMG_TRUE;
	memset(psInst->u.psImae->auSrcComponent, 0, sizeof(psInst->u.psImae->auSrcComponent));
}

static
IMG_VOID InitInstructionTypeCALL(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psInst->u.psCall == NULL)
		psInst->u.psCall = UscAlloc(psState, sizeof(CALL_PARAMS));
	memset(psInst->u.psCall, 0, sizeof(*(psInst->u.psCall)));
	if (psInst->psBlock != NULL)
	{
		psInst->psBlock->uCallCount++;
		psInst->psBlock->psOwner->psFunc->uCallCount++;
	}
}

#if defined(OUTPUT_USPBIN)
static
IMG_VOID InitInstructionTypeTEXWRITE(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if(psInst->u.psTexWrite == NULL)
		psInst->u.psTexWrite = UscAlloc(psState, sizeof(TEXWRITE_PARAMS));
	memset(psInst->u.psTexWrite, 0, sizeof(*(psInst->u.psTexWrite)));
	psInst->u.psTexWrite->uWriteID = INVALID_SAMPLE_ID;
	InitInstArg(&(psInst->u.psTexWrite->sTempReg));
}
#endif /* defined(SUPPORT_USPBIN) */

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_VOID InitInstructionTypeVEC(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32 uSwiz;
	if (psInst->u.psVec == NULL)
	{
		psInst->u.psVec = UscAlloc(psState, sizeof(*psInst->u.psVec));
	}
	memset(psInst->u.psVec->asSrcMod, 0, sizeof(psInst->u.psVec->asSrcMod));
	psInst->u.psVec->uClipPlaneOutput = USC_UNDEF;
	psInst->u.psVec->eRepeatMode = SVEC_REPEATMODE_USEMOE;
	psInst->u.psVec->bPackScale = IMG_FALSE;
	psInst->u.psVec->uPackSwizzle = USC_UNDEF;
	memset(psInst->u.psVec->auSelectUpperHalf, 0, sizeof(psInst->u.psVec->auSelectUpperHalf));
	psInst->u.psVec->bWriteYOnly = IMG_FALSE;
	psInst->u.psVec->uDestSelectShift = USC_X_CHAN;
	psInst->u.psVec->sTest.eTestOpcode = IINVALID;
	psInst->u.psVec->sTest.eChanSel = VTEST_CHANSEL_XCHAN;
	psInst->u.psVec->sTest.eMaskType = USEASM_TEST_MASK_NONE;
	psInst->u.psVec->sTest.eType = TEST_TYPE_INVALID;

	/*
		Initialize the swizzles to undefined values.
	*/
	for (uSwiz = 0; uSwiz < VECTOR_MAX_SOURCE_SLOT_COUNT; uSwiz++)
	{
		psInst->u.psVec->auSwizzle[uSwiz] = USC_UNDEF;
	}
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

#if defined(SUPPORT_SGX545)
static
IMG_VOID InitInstructionTypeDUAL(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uSrc;

	if (psInst->u.psDual == NULL)
		psInst->u.psDual = UscAlloc(psState, sizeof(*psInst->u.psDual));

	memset(psInst->u.psDual, 0, sizeof(*psInst->u.psDual));
	for (uSrc = 0; uSrc < DUAL_MAX_SEC_SOURCES; uSrc++)
	{
		psInst->u.psDual->aeSecondarySrcs[uSrc] = DUAL_SEC_SRC_INVALID;
	}
}
#endif /* defined(SUPPORT_SGX545) */

static
IMG_VOID InitInstructionTypeEMIT(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	
	psInst->u.sEmit.bCut = IMG_FALSE;
}

static
IMG_VOID InitInstructionTypeTEST(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if(psInst->u.psTest == NULL)
		psInst->u.psTest = UscAlloc(psState, sizeof(*psInst->u.psTest));
	memset(psInst->u.psTest, 0, sizeof(*psInst->u.psTest));
}

static
IMG_VOID InitInstructionTypeMOVC(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psInst->u.psMovc == NULL)
		psInst->u.psMovc = UscAlloc(psState, sizeof(*psInst->u.psMovc));
	psInst->u.psMovc->eTest = TEST_TYPE_INVALID;
}

static
IMG_VOID InitInstructionTypeFARITH16(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PFARITH16_PARAMS	psParams;
	IMG_UINT32			uSrc;
	
	if (psInst->u.psArith16 == NULL)
		psInst->u.psArith16 = UscAlloc(psState, sizeof(*psInst->u.psArith16));

	psParams = psInst->u.psArith16;
	for (uSrc = 0; uSrc < (sizeof(psParams->aeSwizzle) / sizeof(psParams->aeSwizzle[0])); uSrc++)
	{
		psParams->aeSwizzle[uSrc] = FMAD16_SWIZZLE_LOWHIGH;
	}
	memset(&psInst->u.psArith16->sFloat, 0, sizeof(psInst->u.psArith16->sFloat));
}

static
IMG_VOID InitInstructionTypeMEMSPILL(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psInst->u.psMemSpill == NULL)
	{
		psInst->u.psMemSpill = UscAlloc(psState, sizeof(*psInst->u.psMemSpill));
	}
	psInst->u.psMemSpill->uLiveChans = USC_ALL_CHAN_MASK;
	psInst->u.psMemSpill->bBypassCache = IMG_FALSE;
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_VOID InitInstructionTypeIDXSC(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psInst->u.psIdxsc == NULL)
	{
		psInst->u.psIdxsc = UscAlloc(psState, sizeof(*psInst->u.psIdxsc));
	}
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static
IMG_VOID InitInstructionTypeCVTFLT2INT(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psInst->u.psCvtFlt2Int == NULL)
	{
		psInst->u.psCvtFlt2Int = UscAlloc(psState, sizeof(*psInst->u.psCvtFlt2Int));
	}
}

static
IMG_VOID InitInstructionTypePCK(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psInst->u.psPck == NULL)
	{
		psInst->u.psPck = UscAlloc(psState, sizeof(*psInst->u.psPck));
	}
	memset(psInst->u.psPck, 0, sizeof(*psInst->u.psPck));
}

static
IMG_VOID InitInstructionTypeLDSTARR(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psInst->u.psLdStArray == NULL)
	{
		psInst->u.psLdStArray = UscAlloc(psState, sizeof(*psInst->u.psLdStArray));
	}
	psInst->u.psLdStArray->uArrayOffset = USC_UNDEF;
	psInst->u.psLdStArray->uArrayNum = USC_UNDEF;
	psInst->u.psLdStArray->uLdStMask = USC_UNDEF;
	psInst->u.psLdStArray->uRelativeStrideInComponents = USC_UNDEF;
}


static
IMG_VOID InitInstructionTypeFDOTPRODUCT(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psInst->u.psFdp == NULL)
		psInst->u.psFdp = UscAlloc(psState, sizeof(*psInst->u.psFdp));
	memset(psInst->u.psFdp, 0, sizeof(*psInst->u.psFdp));
}

static
IMG_VOID InitInstructionTypeDELTA(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psInst->u.psDelta == NULL)
	{
		psInst->u.psDelta = UscAlloc(psState, sizeof(*psInst->u.psDelta));
	}
	psInst->u.psDelta->psInst = psInst;
	if (psInst->psBlock != NULL)
	{
		AppendToList(&psInst->psBlock->sDeltaInstList, &psInst->u.psDelta->sListEntry);
	}
	else
	{
		ClearListEntry(&psInst->u.psDelta->sListEntry);
	}
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	psInst->u.psDelta->bVector = IMG_FALSE;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
}

static
IMG_VOID InitInstructionTypeMOVP(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psInst->u.psMovp == NULL)
		psInst->u.psMovp = UscAlloc(psState, sizeof(*psInst->u.psMovp));
	psInst->u.psMovp->bNegate = IMG_FALSE;
}

static
IMG_VOID InitInstructionTypeFEEDBACKDRIVEREPILOG(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psInst->u.psFeedbackDriverEpilog == NULL)
	{
		psInst->u.psFeedbackDriverEpilog = UscAlloc(psState, sizeof(*psInst->u.psFeedbackDriverEpilog));
	}
	psInst->u.psFeedbackDriverEpilog->psFixedReg = NULL;
	psInst->u.psFeedbackDriverEpilog->uFixedRegOffset = USC_UNDEF;
	psInst->u.psFeedbackDriverEpilog->bPartial = IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID InitInstructionTypeBITWISE(PINTERMEDIATE_STATE psState, PINST psInst)
{
	if (psInst->u.psBitwise == NULL)
	{
		psInst->u.psBitwise = UscAlloc(psState, sizeof(*psInst->u.psBitwise));
	}
	psInst->u.psBitwise->bInvertSrc2 = IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: InitInstructionType
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID InitInstructionTypeUNDEF(PINTERMEDIATE_STATE psState, PINST psInst)
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst);
	
	imgabort();
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeNONE(PINTERMEDIATE_STATE psState, PINST psInst) 
{
	UscFree(psState, psInst->u.psLrp1);
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeFLOAT(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psFloat);
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeEFO(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psEfo); 
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeSOPWM(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psSopWm); 
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeSOP2(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psSop2); 
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeSOP3(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psSop3); 
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeLRP1(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psLrp1); 
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeFPMA(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psFpma); 
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeIMA32(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psIma32); 
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeSMP(PINTERMEDIATE_STATE psState, PINST psInst)
{
	#if defined(OUTPUT_USPBIN)
	/*
		Clear the reference to this instruction in the linked sample unpack instruction.
	*/
	if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_USPTEXTURESAMPLE) != 0)
	{
		PINST	psSampleUnpack;
	
		psSampleUnpack = psInst->u.psSmp->sUSPSample.psSampleUnpack;
		if (psSampleUnpack != NULL)
		{
			ASSERT(psSampleUnpack->u.psSmpUnpack->psTextureSample == psInst);
			psSampleUnpack->u.psSmpUnpack->psTextureSample = NULL;
		}
	}
	#endif /* defined(OUTPUT_USPBIN) */
	
	UscFree(psState, psInst->u.psSmp);
}

#if defined(OUTPUT_USPBIN)
/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeSMPUNPACK(PINTERMEDIATE_STATE psState, PINST psInst)
{
	PINST	psTextureSample;

	/*
		Clear the reference to this instruction in the linked texture sample instruction.
	*/
	psTextureSample = psInst->u.psSmpUnpack->psTextureSample;
	if (psTextureSample != NULL)
	{
		PSMP_USP_PARAMS	psUSPSample;

		psUSPSample = &psTextureSample->u.psSmp->sUSPSample;

		ASSERT(psUSPSample->psSampleUnpack == psInst);
		psUSPSample->psSampleUnpack = NULL;
	}

	UscFree(psState, psInst->u.psSmpUnpack);
}
#endif

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeDOT34(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psDot34);
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeDPC(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psDpc);
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeLDST(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psLdSt);
}

#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeNRM(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psNrm);
}
#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeLOADCONST(PINTERMEDIATE_STATE psState, PINST psInst)
{
	IMG_UINT32	uBufferIdx;
	PCONSTANT_BUFFER	psBuffer;

	uBufferIdx = psInst->u.psLoadConst->uBufferIdx;
	psBuffer = &psState->asConstantBuffer[uBufferIdx];
	RemoveFromList(&psBuffer->sLoadConstList, &psInst->u.psLoadConst->sBufferListEntry);

	UscFree(psState, psInst->u.psLoadConst); 
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeLOADMEMCONST(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psLoadMemConst);
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeIMA16(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psIma16);
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeIMAE(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psImae); 
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeCALL(PINTERMEDIATE_STATE psState, PINST psInst)
{
	//remove from linked list of calls.
	PINST *ppsInst;
	if (psInst->psBlock != NULL)
	{
		ASSERT(psInst->psBlock->uCallCount > 0);
		psInst->psBlock->uCallCount--;
		ASSERT(psInst->psBlock->psOwner->psFunc->uCallCount > 0);
		psInst->psBlock->psOwner->psFunc->uCallCount--;
	}
	for (ppsInst = &psInst->u.psCall->psTarget->psCallSiteHead;
			;
		 ppsInst = &(*ppsInst)->u.psCall->psCallSiteNext)
	{
		if (*ppsInst == psInst)
		{
			*ppsInst = psInst->u.psCall->psCallSiteNext;
			break;
		}
	}
	if (psInst->u.psCall->psTarget->psCallSiteHead == NULL
		&& psInst->u.psCall->psTarget->pchEntryPointDesc == NULL)
	{
		//just removed last call to non-externally-visible fn
		FreeFunction(psState, psInst->u.psCall->psTarget);
	}
	UscFree(psState, psInst->u.psCall);
}

#if defined(OUTPUT_USPBIN)
/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeTEXWRITE(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, (psInst->u.psTexWrite));
}
#endif

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeVEC(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psVec);
}
#endif

#if defined(SUPPORT_SGX545)
/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeDUAL(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psDual); 
}
#endif

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeEMIT(PINTERMEDIATE_STATE psState, PINST psInst)
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst);
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeTEST(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psTest);
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeMOVC(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psMovc);
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeFARITH16(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psArith16);
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeMEMSPILL(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psMemSpill);
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeIDXSC(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psIdxsc);
}
#endif

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeCVTFLT2INT(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psCvtFlt2Int);
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypePCK(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psPck);
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeLDSTARR(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psLdStArray);
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeFDOTPRODUCT(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psFdp);
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeDELTA(PINTERMEDIATE_STATE psState, PINST psInst)
{
	ASSERT(psInst->u.psDelta->psInst == psInst);
	if (psInst->psBlock != NULL)
	{
		RemoveFromList(&psInst->psBlock->sDeltaInstList, &psInst->u.psDelta->sListEntry);
	}
	UscFree(psState, psInst->u.psDelta);
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeMOVP(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psMovp);
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeFEEDBACKDRIVEREPILOG(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psFeedbackDriverEpilog);
}

/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ClearInstructionTypeBITWISE(PINTERMEDIATE_STATE psState, PINST psInst)
{
	UscFree(psState, psInst->u.psBitwise);
}

static
IMG_VOID ClearInstructionTypeUNDEF(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: ClearInstructionType

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction type.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst);
	
	imgabort();
}

static
IMG_INT32 CompareInstParametersTypeNONE(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	/* No special parameters */
	
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst1);
	PVR_UNREFERENCED_PARAMETER(psInst2);
	return 0;	
}


static
IMG_INT32 CompareInstParametersTypeUNDEF(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	/* No special parameters */
	
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst1);
	PVR_UNREFERENCED_PARAMETER(psInst2);
	
	imgabort();
}

static
IMG_INT32 CompareFloatSourceModifier(const FLOAT_SOURCE_MODIFIER *psMod1, const FLOAT_SOURCE_MODIFIER *psMod2)
/*****************************************************************************
 FUNCTION	: CompareFloatSourceModifier

 PURPOSE	: Check if two floating point source modifiers are equal.

 PARAMETERS	: psMod1, psMod2		- The two modifiers to compare.

 RETURNS	: -ve		If the first modifier is less than the second modifier.
			  0			If the two modifiers are equal.
			  +ve		If the first modifier is greater than the second modifier.
*****************************************************************************/
{
	COMPARE(psMod1->bNegate, psMod2->bNegate);
	COMPARE(psMod1->bAbsolute, psMod2->bAbsolute);
	COMPARE(psMod1->uComponent, psMod2->uComponent);
	return 0;
}

static
IMG_INT32 CompareFloatSourceModifiers(IMG_UINT32 uArgCount, const FLOAT_PARAMS *psFloat1, const FLOAT_PARAMS *psFloat2)
{
	IMG_UINT32	uSrcIdx;
	for (uSrcIdx = 0; uSrcIdx < uArgCount; uSrcIdx++)
	{
		const FLOAT_SOURCE_MODIFIER	*psMod1 = &psFloat1->asSrcMod[uSrcIdx];
		const FLOAT_SOURCE_MODIFIER	*psMod2 = &psFloat2->asSrcMod[uSrcIdx];
		IMG_INT32				iCmp;

		iCmp = CompareFloatSourceModifier(psMod1, psMod2);
		if (iCmp != 0)
		{
			return iCmp;
		}
	}
	return 0;
}

static 
IMG_INT32 CompareInstParametersTypeFLOAT(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	IMG_UINT32	uTypePreserve1, uTypePreserve2;
	
	PVR_UNREFERENCED_PARAMETER(psState);

	uTypePreserve1 = GetBit(psInst1->auFlag, INST_TYPE_PRESERVE);
	uTypePreserve2 = GetBit(psInst2->auFlag, INST_TYPE_PRESERVE);
	COMPARE(uTypePreserve1, uTypePreserve2);

	return CompareFloatSourceModifiers(psInst1->uArgumentCount, psInst1->u.psFloat, psInst2->u.psFloat);
}

static
IMG_INT32 CompareInstParametersTypeEFO(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	const EFO_PARAMETERS *psEfo1 = psInst1->u.psEfo;
	const EFO_PARAMETERS *psEfo2 = psInst2->u.psEfo;
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	if (!psEfo1->bIgnoreDest)
	{
		COMPARE(psEfo1->eDSrc, psEfo2->eDSrc);
	}
	if (psEfo1->bWriteI0)
	{
		COMPARE(psEfo1->eI0Src, psEfo2->eI0Src);
	}
	if (psEfo1->bWriteI1)
	{
		COMPARE(psEfo1->eI1Src, psEfo2->eI1Src);
	}
	if (psEfo1->bA0Used)
	{
		COMPARE(psEfo1->eA0Src0, psEfo2->eA0Src0);
		COMPARE(psEfo1->eA0Src1, psEfo2->eA0Src1);
	}
	if (psEfo1->bA1Used)
	{
		COMPARE(psEfo1->eA1Src0, psEfo2->eA1Src0);
		COMPARE(psEfo1->eA1Src1, psEfo2->eA1Src1);
	}
	if (psEfo1->bM0Used)
	{
		COMPARE(psEfo1->eM0Src0, psEfo2->eM0Src0);
		COMPARE(psEfo1->eM0Src1, psEfo2->eM0Src1);
	}
	if (psEfo1->bM1Used)
	{
		COMPARE(psEfo1->eM1Src0, psEfo2->eM1Src0);
		COMPARE(psEfo1->eM1Src1, psEfo2->eM1Src1);
	}
	return CompareFloatSourceModifiers(EFO_UNIFIEDSTORE_SOURCE_COUNT, &psEfo1->sFloat, &psEfo2->sFloat);
}

static
IMG_INT32 CompareInstParametersTypeSOPWM(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	const SOPWM_PARAMS *psParams1 = psInst1->u.psSopWm;
	const SOPWM_PARAMS *psParams2 = psInst2->u.psSopWm;
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	COMPARE(psParams1->uCop, psParams2->uCop);
	COMPARE(psParams1->uAop, psParams2->uAop);
	COMPARE(psParams1->uSel1, psParams2->uSel1);
	COMPARE(psParams1->bComplementSel1, psParams2->bComplementSel1);
	COMPARE(psParams1->uSel2, psParams2->uSel2);
	COMPARE(psParams1->bComplementSel2, psParams2->bComplementSel2);
	COMPARE(psParams1->bRedChanFromAlpha, psParams2->bRedChanFromAlpha);
	
	return 0;
}

static
IMG_INT32 CompareInstParametersTypeSOP2(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	const PSOP2_PARAMS psParams1 = psInst1->u.psSop2;
	const PSOP2_PARAMS psParams2 = psInst2->u.psSop2;
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	COMPARE(psParams1->uCSel1, psParams2->uCSel1);
	COMPARE(psParams1->uCSel2, psParams2->uCSel2);
	COMPARE(psParams1->bComplementCSel1, psParams2->bComplementCSel1);
	COMPARE(psParams1->bComplementCSel2, psParams2->bComplementCSel2);
	COMPARE(psParams1->bComplementCSrc1, psParams2->bComplementCSrc1);
	COMPARE(psParams1->uCOp, psParams2->uCOp);
	COMPARE(psParams1->uASel1, psParams2->uASel1);
	COMPARE(psParams1->uASel2, psParams2->uASel2);
	COMPARE(psParams1->bComplementASel1, psParams2->bComplementASel1);
	COMPARE(psParams1->bComplementASel2, psParams2->bComplementASel2);
	COMPARE(psParams1->uAOp, psParams2->uAOp);
	COMPARE(psParams1->bComplementASrc1, psParams2->bComplementASrc1);
	return 0;

}

static
IMG_INT32 CompareInstParametersTypeSOP3(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	const PSOP3_PARAMS psParams1 = psInst1->u.psSop3;
	const PSOP3_PARAMS psParams2 = psInst2->u.psSop3;
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	COMPARE(psParams1->uCSel1, psParams2->uCSel1);
	COMPARE(psParams1->uCSel2, psParams2->uCSel2);
	COMPARE(psParams1->bComplementCSel1, psParams2->bComplementCSel1);
	COMPARE(psParams1->bComplementCSel2, psParams2->bComplementCSel2);
	COMPARE(psParams1->bNegateCResult, psParams2->bNegateCResult);
	COMPARE(psParams1->uCOp, psParams2->uCOp);

	COMPARE(psParams1->uCoissueOp, psParams2->uCoissueOp);

	COMPARE(psParams1->uASel1, psParams2->uASel1);
	COMPARE(psParams1->uASel2, psParams2->uASel2);
	COMPARE(psParams1->bComplementASel1, psParams2->bComplementASel1);
	COMPARE(psParams1->bComplementASel2, psParams2->bComplementASel2);
	COMPARE(psParams1->bNegateAResult, psParams2->bNegateAResult);
	COMPARE(psParams1->uAOp, psParams2->uAOp);

	return 0;

}

static
IMG_INT32 CompareInstParametersTypeLRP1(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	const PLRP1_PARAMS psParams1 = psInst1->u.psLrp1;
	const PLRP1_PARAMS psParams2 = psInst2->u.psLrp1;
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	COMPARE(psParams1->uCSel10, psParams2->uCSel10);
	COMPARE(psParams1->bComplementCSel10, psParams2->bComplementCSel10);
	COMPARE(psParams1->uCSel11, psParams2->uCSel11);
	COMPARE(psParams1->bComplementCSel11, psParams2->bComplementCSel11);
	COMPARE(psParams1->uCS, psParams2->uCS);

	COMPARE(psParams1->uASel1, psParams2->uASel1);
	COMPARE(psParams1->bComplementASel1, psParams2->bComplementASel1);
	COMPARE(psParams1->uASel2, psParams2->uASel2);
	COMPARE(psParams1->bComplementASel2, psParams2->bComplementASel2);
	COMPARE(psParams1->uAOp, psParams2->uAOp);
	return 0;

}

static
IMG_INT32 CompareInstParametersTypeFPMA(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	const FPMA_PARAMS *psParams1 = psInst1->u.psFpma;
	const FPMA_PARAMS *psParams2 = psInst2->u.psFpma;
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	COMPARE(psParams1->uCSel0, psParams2->uCSel0);
	COMPARE(psParams1->uCSel1, psParams2->uCSel1);
	COMPARE(psParams1->uCSel2, psParams2->uCSel2);
	COMPARE(psParams1->uASel0, psParams2->uASel0);
	COMPARE(psParams1->bComplementCSel0, psParams2->bComplementCSel0);
	COMPARE(psParams1->bComplementCSel1, psParams2->bComplementCSel1);
	COMPARE(psParams1->bComplementCSel2, psParams2->bComplementCSel2);
	COMPARE(psParams1->bComplementASel0, psParams2->bComplementASel0);
	COMPARE(psParams1->bComplementASel1, psParams2->bComplementASel1);
	COMPARE(psParams1->bComplementASel2, psParams2->bComplementASel2);
	COMPARE(psParams1->bNegateSource0, psParams2->bNegateSource0);
	return 0;
}

static
IMG_INT32 CompareInstParametersTypeIMA32(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
#if defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	IMG_UINT32	uSrcIdx;
	const PIMA32_PARAMS psParams1 = psInst1->u.psIma32;
	const PIMA32_PARAMS psParams2 = psInst2->u.psIma32;

	PVR_UNREFERENCED_PARAMETER(psState);
	
	COMPARE(psParams1->bSigned, psParams2->bSigned);
	COMPARE(psParams1->uStep, psParams2->uStep);

	for (uSrcIdx = 0; uSrcIdx < IMA32_SOURCE_COUNT; uSrcIdx++)
	{
		COMPARE(psParams1->abNegate[uSrcIdx], psParams2->abNegate[uSrcIdx]);
	}
	return 0;
#else
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst1);
	PVR_UNREFERENCED_PARAMETER(psInst2);
	
	imgabort();
#endif /* defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
}

static
IMG_INT32 CompareInstParametersTypeSMP(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	const SMP_PARAMS *psParams1 = psInst1->u.psSmp;
	const SMP_PARAMS *psParams2 = psInst2->u.psSmp;
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	COMPARE(psParams1->uDimensionality, psParams2->uDimensionality);
	COMPARE(psParams1->bUsesPCF, psParams2->bUsesPCF);
	COMPARE(psParams1->uCoordSize, psParams2->uCoordSize);
	COMPARE(psParams1->uGradSize, psParams2->uGradSize);
	COMPARE(psParams1->uTextureStage, psParams2->uTextureStage);
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	COMPARE(psParams1->bCoordSelectUpper, psParams2->bCoordSelectUpper);
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	return 0;
}

static
IMG_INT32 CompareInstParametersTypeDOT34(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	const PDOT34_PARAMS psParams1 = psInst1->u.psDot34;
	const PDOT34_PARAMS psParams2 = psInst2->u.psDot34;
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	COMPARE(psParams1->uDot34Scale, psParams2->uDot34Scale);
	COMPARE(psParams1->uVecLength, psParams2->uVecLength);
	COMPARE(psParams1->bOffset, psParams2->bOffset);
	return 0;
}

static
IMG_INT32 CompareInstParametersTypeDPC(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	const DPC_PARAMS *psParams1 = psInst1->u.psDpc;
	const DPC_PARAMS *psParams2 = psInst2->u.psDpc;
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	COMPARE(psParams1->uClipplane, psParams2->uClipplane);
	return CompareFloatSourceModifiers(g_psInstDesc[IFDPC].uDefaultArgumentCount, &psParams1->sFloat, &psParams2->sFloat);
}

static
IMG_INT32 CompareInstParametersTypeLDST(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	
	COMPARE(psInst1->u.psLdSt->bEnableRangeCheck, psInst2->u.psLdSt->bEnableRangeCheck);
	COMPARE(psInst1->u.psLdSt->bBypassCache, psInst2->u.psLdSt->bBypassCache);
	COMPARE(psInst1->u.psLdSt->uFlags, psInst2->u.psLdSt->uFlags);
	COMPARE(psInst1->u.psLdSt->bPositiveOffset, psInst2->u.psLdSt->bPositiveOffset);
	COMPARE(psInst1->u.psLdSt->bPreIncrement, psInst2->u.psLdSt->bPreIncrement);
	return 0;
}

#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
static
IMG_INT32 CompareInstParametersTypeNRM(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)	
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{

	const NRM_PARAMS *psParams1 = psInst1->u.psNrm;
	const NRM_PARAMS *psParams2 = psInst2->u.psNrm;
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	COMPARE(psParams1->bSrcModNegate, psParams2->bSrcModNegate);
	COMPARE(psParams1->bSrcModAbsolute, psParams2->bSrcModAbsolute);
	return 0;
}
#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */

static
IMG_INT32 CompareInstParametersTypeLOADCONST(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	const LOADCONST_PARAMS *psParams1 = psInst1->u.psLoadConst;
	const LOADCONST_PARAMS *psParams2 = psInst2->u.psLoadConst;
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	COMPARE(psParams1->bRelativeAddress, psParams2->bRelativeAddress);
	COMPARE(psParams1->bStaticConst, psParams2->bStaticConst);
	COMPARE(psParams1->uStaticConstValue, psParams2->uStaticConstValue);
	COMPARE(psParams1->eFormat, psParams2->eFormat);
	COMPARE(psParams1->uRelativeStrideInBytes, psParams2->uRelativeStrideInBytes);
	COMPARE(psParams1->uBufferIdx, psParams2->uBufferIdx);
	COMPARE(psParams1->uC10CompOffset, psParams2->uC10CompOffset);
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	COMPARE(psParams1->bVectorResult, psParams2->bVectorResult);
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	return 0;
}

IMG_INTERNAL
IMG_INT32 BaseCompareInstParametersTypeLOADMEMCONST(PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	const LOADMEMCONST_PARAMS *psParams1 = psInst1->u.psLoadMemConst;
	const LOADMEMCONST_PARAMS *psParams2 = psInst2->u.psLoadMemConst;
	
	COMPARE(psParams1->bRelativeAddress, psParams2->bRelativeAddress);
	COMPARE(psParams1->uFetchCount, psParams2->uFetchCount);
	COMPARE(psParams1->bRangeCheck, psParams2->bRangeCheck);
	COMPARE(psParams1->uDataSize, psParams2->uDataSize);
	COMPARE(psParams1->bBypassCache, psParams2->bBypassCache);
	COMPARE(psParams1->uFlags, psParams2->uFlags);
	return 0;
}

IMG_INTERNAL
IMG_INT32 CompareInstParametersTypeLOADMEMCONST(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);

	return BaseCompareInstParametersTypeLOADMEMCONST(psInst1, psInst2);
}

static
IMG_INT32 CompareInstParametersTypeIMA16(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	
	return psInst1->u.psIma16->bNegateSource2 - psInst2->u.psIma16->bNegateSource2;
}

static
IMG_INT32 CompareInstParametersTypeIMAE(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	IMG_UINT32	uSrcIdx;
	const IMAE_PARAMS *psParams1 = psInst1->u.psImae;
	const IMAE_PARAMS *psParams2 = psInst2->u.psImae;
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	COMPARE(psParams1->bSigned, psParams2->bSigned);
	COMPARE(psParams1->uSrc2Type, psParams2->uSrc2Type);

	for (uSrcIdx = 0; uSrcIdx < IMAE_UNIFIED_STORE_SOURCE_COUNT; uSrcIdx++)
	{
		IMG_UINT32	uComponent1;
		IMG_UINT32	uComponent2;

		uComponent1 = psParams1->auSrcComponent[uSrcIdx];
		uComponent2 = psParams2->auSrcComponent[uSrcIdx];
		COMPARE(uComponent1, uComponent2);
	}
	return 0;
}

static
IMG_INT32 CompareInstParametersTypeCALL(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst1);
	PVR_UNREFERENCED_PARAMETER(psInst2);
	
	imgabort();
}

#if defined(OUTPUT_USPBIN)
static
IMG_INT32 CompareInstParametersTypeTEXWRITE(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst1);
	PVR_UNREFERENCED_PARAMETER(psInst2);
	
	imgabort();
}
#endif

static
IMG_INT32 CompareTestDetails(PCTEST_DETAILS psTest1, PCTEST_DETAILS psTest2)
/*****************************************************************************
 FUNCTION	: CompareTestDetails

 PURPOSE	: Compare the details of two tests to apply to the results of an
			  instruction.

 PARAMETERS	: psTest1			- Tests to compare.
			  psTest2

 RETURNS	: -ve	if the first test is less than the second test.
			  0		if the two instructions are equal.
			  +ve	if the first test is greater than the second test.
*****************************************************************************/
{
	COMPARE(psTest1->eType, psTest2->eType);
	COMPARE(psTest1->eMaskType, psTest2->eMaskType);
	COMPARE(psTest1->eChanSel, psTest2->eChanSel);
	return 0;
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
IMG_INTERNAL
IMG_INT32 CompareInstNonSourceParametersTypeVEC(PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstNonSourceParametersTypeVEC

 PURPOSE	: Check if the instruction specific parameters not related to a
			  specific source are the same.

 PARAMETERS	: psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	if (psInst1->eOpcode == IVTEST || psInst1->eOpcode == IVTESTMASK || psInst1->eOpcode == IVTESTBYTEMASK)
	{
		COMPARE(psInst1->u.psVec->sTest.eChanSel, psInst2->u.psVec->sTest.eChanSel);
		COMPARE(psInst1->u.psVec->sTest.eMaskType, psInst2->u.psVec->sTest.eMaskType);
		COMPARE(psInst1->u.psVec->sTest.eType, psInst2->u.psVec->sTest.eType);
		COMPARE(psInst1->u.psVec->sTest.eTestOpcode, psInst2->u.psVec->sTest.eTestOpcode);
	}

	if (psInst1->eOpcode == IVDP_CP)
	{
		COMPARE(psInst1->u.psVec->uClipPlaneOutput, psInst2->u.psVec->uClipPlaneOutput);
	}

	if (psInst1->eOpcode == IVDUAL)
	{
		COMPARE(psInst1->u.psVec->sDual.ePriOp, psInst2->u.psVec->sDual.ePriOp);
		COMPARE(psInst1->u.psVec->sDual.eSecOp, psInst2->u.psVec->sDual.eSecOp);
	}
	
	if (psInst1->eOpcode == IVMOVC || 
		psInst1->eOpcode == IVMOVCU8_FLT ||
		psInst1->eOpcode == IVMOVCC10 || 
		psInst1->eOpcode == IVMOVCU8)
	{
		COMPARE(psInst1->u.psVec->eMOVCTestType, psInst2->u.psVec->eMOVCTestType);
	}

	COMPARE(psInst1->u.psVec->bPackScale, psInst2->u.psVec->bPackScale);
	COMPARE(psInst1->u.psVec->bWriteYOnly, psInst2->u.psVec->bWriteYOnly);
	COMPARE(psInst1->u.psVec->eRepeatMode, psInst2->u.psVec->eRepeatMode);
	COMPARE(psInst1->u.psVec->uDestSelectShift, psInst2->u.psVec->uDestSelectShift);
	COMPARE(psInst1->u.psVec->uPackSwizzle, psInst2->u.psVec->uPackSwizzle);

	return 0;
}

static
IMG_INT32 CompareInstParametersTypeVEC(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	IMG_UINT32	uSlotIdx;
	IMG_INT32	iCmp;

	for (uSlotIdx = 0; uSlotIdx < GetSwizzleSlotCount(psState, psInst1); uSlotIdx++)
	{
		COMPARE(psInst1->u.psVec->auSwizzle[uSlotIdx], psInst2->u.psVec->auSwizzle[uSlotIdx]);
		COMPARE(psInst1->u.psVec->aeSrcFmt[uSlotIdx], psInst2->u.psVec->aeSrcFmt[uSlotIdx]);
		COMPARE(psInst1->u.psVec->asSrcMod[uSlotIdx].bNegate, psInst2->u.psVec->asSrcMod[uSlotIdx].bNegate);
		COMPARE(psInst1->u.psVec->asSrcMod[uSlotIdx].bAbs, psInst2->u.psVec->asSrcMod[uSlotIdx].bAbs);
		COMPARE(GetBit(psInst1->u.psVec->auSelectUpperHalf, uSlotIdx), GetBit(psInst2->u.psVec->auSelectUpperHalf, uSlotIdx));
	}

	if ((iCmp = CompareInstNonSourceParametersTypeVEC(psInst1, psInst2)) != 0)
	{
		return iCmp;
	}

	return 0;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

#if defined(SUPPORT_SGX545)
static
IMG_INT32 CompareInstParametersTypeDUAL(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	IMG_UINT32	uPriSrcIdx, uSecSrcIdx;
	const DUAL_PARAMS *psDual1 = psInst1->u.psDual;
	const DUAL_PARAMS *psDual2 = psInst2->u.psDual;
	
	PVR_UNREFERENCED_PARAMETER(psState);

	COMPARE(psDual1->ePrimaryOp, psDual2->ePrimaryOp);
	COMPARE(psDual1->eSecondaryOp, psDual2->eSecondaryOp);

	for (uPriSrcIdx = 0; uPriSrcIdx < DUAL_MAX_PRI_SOURCES; uPriSrcIdx++)
	{
		COMPARE(psDual1->abPrimarySourceNegate[uPriSrcIdx], psDual2->abPrimarySourceNegate[uPriSrcIdx]);
		COMPARE(psDual1->auSourceComponent[uPriSrcIdx], psDual2->auSourceComponent[uPriSrcIdx]);
	}
	for (uSecSrcIdx = 0; uSecSrcIdx < DUAL_MAX_SEC_SOURCES; uSecSrcIdx++)
	{
		COMPARE(psDual1->aeSecondarySrcs[uSecSrcIdx], psDual2->aeSecondarySrcs[uSecSrcIdx]);
	}
	return 0;
}
#endif /* defined(SUPPORT_SGX545) */

static
IMG_INT32 CompareInstParametersTypeEMIT(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	
	return psInst1->u.sEmit.bCut - psInst2->u.sEmit.bCut;
}

IMG_INTERNAL
IMG_INT32 CompareInstParametersTypeTEST(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	PTEST_PARAMS	psParams1 = psInst1->u.psTest;
	PTEST_PARAMS	psParams2 = psInst2->u.psTest;
	IMG_UINT32		uSrcIdx;
	IMG_INT32		iCmp;

	PVR_UNREFERENCED_PARAMETER(psState);
	
	COMPARE(psParams1->eAluOpcode, psParams2->eAluOpcode);
	if ((iCmp = CompareTestDetails(&psParams1->sTest, &psParams2->sTest)) != 0)
	{
		return iCmp;
	}
	for (uSrcIdx = 0; uSrcIdx < psInst1->uArgumentCount; uSrcIdx++)
	{
		COMPARE(psParams1->auSrcComponent[uSrcIdx], psParams2->auSrcComponent[uSrcIdx]);
	}

	return 0;
}

static
IMG_INT32 CompareInstParametersTypeMOVP(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	
	return psInst1->u.psMovp->bNegate - psInst2->u.psMovp->bNegate;
}

static
IMG_INT32 CompareInstParametersTypeBITWISE(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	
	return psInst1->u.psBitwise->bInvertSrc2 - psInst2->u.psBitwise->bInvertSrc2;
}

IMG_INTERNAL
IMG_INT32 CompareInstParametersTypeMOVC(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	const MOVC_PARAMS *psParams1 = psInst1->u.psMovc;
	const MOVC_PARAMS *psParams2 = psInst2->u.psMovc;
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	COMPARE(psParams1->eTest, psParams2->eTest);
	return 0;
}

static
IMG_INT32 CompareInstParametersTypeFARITH16(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	IMG_INT32			iCmp;
	PFARITH16_PARAMS	psParams1 = psInst1->u.psArith16;
	PFARITH16_PARAMS	psParams2 = psInst2->u.psArith16;
	IMG_UINT32			uSrcIdx;
	
	PVR_UNREFERENCED_PARAMETER(psState);

	iCmp = CompareFloatSourceModifiers(psInst1->uArgumentCount, &psInst1->u.psArith16->sFloat, &psInst2->u.psArith16->sFloat);
	if (iCmp != 0)
	{
		return iCmp;
	}

	for (uSrcIdx = 0; uSrcIdx < psInst1->uArgumentCount; uSrcIdx++)
	{
		FMAD16_SWIZZLE	eSwizzle1 = psParams1->aeSwizzle[uSrcIdx];
		FMAD16_SWIZZLE	eSwizzle2 = psParams2->aeSwizzle[uSrcIdx];

		COMPARE(eSwizzle1, eSwizzle2);
	}

	return 0;
}

static
IMG_INT32 CompareInstParametersTypeMEMSPILL(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	
	return psInst1->u.psMemSpill->uLiveChans - psInst2->u.psMemSpill->uLiveChans;
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_INT32 CompareInstParametersTypeIDXSC(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	const IDXSC_PARAMS *psParams1 = psInst1->u.psIdxsc;
	const IDXSC_PARAMS *psParams2 = psInst2->u.psIdxsc;
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	COMPARE(psParams1->uChannelCount, psParams2->uChannelCount);
	COMPARE(psParams1->uChannelOffset, psParams2->uChannelOffset);
	return 0;
	

}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static
IMG_INT32 CompareInstParametersTypeCVTFLT2INT(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	
	return psInst1->u.psCvtFlt2Int->bSigned - psInst2->u.psCvtFlt2Int->bSigned;
}

static
IMG_INT32 CompareInstParametersTypePCK(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	IMG_UINT32 uSrcIdx;
	const PCK_PARAMS *psPck1 = psInst1->u.psPck;
	const PCK_PARAMS *psPck2 = psInst2->u.psPck;
	
	PVR_UNREFERENCED_PARAMETER(psState);

	for (uSrcIdx = 0; uSrcIdx < PCK_SOURCE_COUNT; uSrcIdx++)
	{
		COMPARE(psPck1->auComponent[uSrcIdx], psPck2->auComponent[uSrcIdx]);
	}
	COMPARE(psPck1->bScale, psPck2->bScale);
	return 0;
}

static
IMG_INT32 CompareInstParametersTypeLDSTARR(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	const LDSTARR_PARAMS *psParams1 = psInst1->u.psLdStArray;
	const LDSTARR_PARAMS *psParams2 = psInst2->u.psLdStArray;
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	COMPARE(psParams1->uLdStMask, psParams2->uLdStMask);
	COMPARE(psParams1->uArrayOffset, psParams2->uArrayOffset);
	COMPARE(psParams1->uArrayNum, psParams2->uArrayNum);
	COMPARE(psParams1->eFmt, psParams2->eFmt);
	COMPARE(psParams1->uRelativeStrideInComponents, psParams2->uRelativeStrideInComponents);
	return 0;
}

static
IMG_INT32 CompareInstParametersTypeFDOTPRODUCT(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	IMG_UINT32			uSrcIdx;
	IMG_INT32			iCmp;
	const FDOTPRODUCT_PARAMS	*psFDP1 = psInst1->u.psFdp;
	const FDOTPRODUCT_PARAMS	*psFDP2 = psInst2->u.psFdp;
	
	PVR_UNREFERENCED_PARAMETER(psState);

	COMPARE(psFDP1->uRepeatCount, psFDP2->uRepeatCount);
	COMPARE(psFDP1->uClipPlane, psFDP2->uClipPlane);
	if ((iCmp = CompareTestDetails(&psFDP1->sTest, &psFDP2->sTest)) != 0)
	{
		return iCmp;
	}

	for (uSrcIdx = 0; uSrcIdx < psInst1->uArgumentCount; uSrcIdx++)
	{
		COMPARE(psFDP1->abNegate[uSrcIdx], psFDP2->abNegate[uSrcIdx]);
		COMPARE(psFDP1->abAbsolute[uSrcIdx], psFDP2->abAbsolute[uSrcIdx]);
		COMPARE(psFDP1->auComponent[uSrcIdx], psFDP2->auComponent[uSrcIdx]);
	}
	return 0;
}

static
IMG_INT32 CompareInstParametersTypeDELTA(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	COMPARE(psInst1->u.psDelta->bVector, psInst2->u.psDelta->bVector);
	#else
	PVR_UNREFERENCED_PARAMETER(psInst1);
	PVR_UNREFERENCED_PARAMETER(psInst2);
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	return 0;
}

#if defined(OUTPUT_USPBIN)
static
IMG_INT32 CompareInstParametersTypeSMPUNPACK(PINTERMEDIATE_STATE psState, PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	
	COMPARE(psInst1->u.psSmpUnpack->uChanMask, psInst2->u.psSmpUnpack->uChanMask);
	COMPARE(psInst1->u.psSmpUnpack->uSmpID, psInst2->u.psSmpUnpack->uSmpID);
	return 0;
}
#endif /* defined(OUTPUT_USPBIN) */

static
IMG_INT32 CompareInstParametersTypeFEEDBACKDRIVEREPILOG(PINTERMEDIATE_STATE psState, 
																PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstParametersType

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst1);
	PVR_UNREFERENCED_PARAMETER(psInst2);
	
	imgabort();
}

static const INIT_INSTRUCTION_JUMP_TABLE gauInstructionOperationsJumpTable[INST_TYPE_COUNT] = 
{
	/* INST_TYPE_NONE */
	{InitInstructionTypeNONE,			ClearInstructionTypeNONE,			CompareInstParametersTypeNONE},
	/* INST_TYPE_FLOAT */
	{InitInstructionTypeFLOAT,			ClearInstructionTypeFLOAT,			CompareInstParametersTypeFLOAT},
	/* INST_TYPE_EFO */
	{InitInstructionTypeEFO,			ClearInstructionTypeEFO,			CompareInstParametersTypeEFO},
	/* INST_TYPE_SOPWM */
	{InitInstructionTypeSOPWM,			ClearInstructionTypeSOPWM,			CompareInstParametersTypeSOPWM},
	/* INST_TYPE_SOP2 */
	{InitInstructionTypeSOP2,			ClearInstructionTypeSOP2,			CompareInstParametersTypeSOP2},
	/* INST_TYPE_SOP3 */
	{InitInstructionTypeSOP3,			ClearInstructionTypeSOP3,			CompareInstParametersTypeSOP3},
	/* INST_TYPE_LRP1 */
	{InitInstructionTypeLRP1,			ClearInstructionTypeLRP1,			CompareInstParametersTypeLRP1},
	/* INST_TYPE_FPMA */
	{InitInstructionTypeFPMA,			ClearInstructionTypeFPMA,			CompareInstParametersTypeFPMA},
	/* INST_TYPE_IMA32 */
	{InitInstructionTypeIMA32,			ClearInstructionTypeIMA32,			CompareInstParametersTypeIMA32},
	/* INST_TYPE_SMP */
	{InitInstructionTypeSMP,			ClearInstructionTypeSMP,			CompareInstParametersTypeSMP},
	#if defined(OUTPUT_USPBIN)
	/* INST_TYPE_SMPUNPACK */
	{InitInstructionTypeSMPUNPACK,		ClearInstructionTypeSMPUNPACK,		CompareInstParametersTypeSMPUNPACK},
	#endif /* defined(OUTPUT_USPBIN) */
	/* INST_TYPE_DOT34 */
	{InitInstructionTypeDOT34,			ClearInstructionTypeDOT34,			CompareInstParametersTypeDOT34},
	/* INST_TYPE_DPC */
	{InitInstructionTypeDPC,			ClearInstructionTypeDPC,			CompareInstParametersTypeDPC},
	/* INST_TYPE_LDST */
	{InitInstructionTypeLDST,			ClearInstructionTypeLDST,			CompareInstParametersTypeLDST},
	#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
	/* INST_TYPE_NRM */
	{InitInstructionTypeNRM,			ClearInstructionTypeNRM,			CompareInstParametersTypeNRM},
	#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */
	/* INST_TYPE_LOADCONST */
	{InitInstructionTypeLOADCONST,		ClearInstructionTypeLOADCONST,		CompareInstParametersTypeLOADCONST},
	/* INST_TYPE_LOADMEMCONST */
	{InitInstructionTypeLOADMEMCONST,	ClearInstructionTypeLOADMEMCONST,	CompareInstParametersTypeLOADMEMCONST},
	/* INST_TYPE_IMA16 */
	{InitInstructionTypeIMA16,			ClearInstructionTypeIMA16,			CompareInstParametersTypeIMA16},
	/* INST_TYPE_IMAE */
	{InitInstructionTypeIMAE,			ClearInstructionTypeIMAE,			CompareInstParametersTypeIMAE},
	/* INST_TYPE_CALL */
	{InitInstructionTypeCALL,			ClearInstructionTypeCALL,			CompareInstParametersTypeCALL},
	#if defined(OUTPUT_USPBIN)
	/* INST_TYPE_TEXWRITE */
	{InitInstructionTypeTEXWRITE,		ClearInstructionTypeTEXWRITE,		CompareInstParametersTypeTEXWRITE},
	#endif /* defined(SUPPORT_USPBIN) */
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/* INST_TYPE_VEC */
	{InitInstructionTypeVEC,			ClearInstructionTypeVEC,			CompareInstParametersTypeVEC},
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	#if defined(SUPPORT_SGX545)
	/* INST_TYPE_DUAL */
	{InitInstructionTypeDUAL,			ClearInstructionTypeDUAL,			CompareInstParametersTypeDUAL},
	#endif /* defined(SUPPORT_SGX545) */
	/* INST_TYPE_EMIT */
	{InitInstructionTypeEMIT,			ClearInstructionTypeEMIT,			CompareInstParametersTypeEMIT},
	/* INST_TYPE_TEST */
	{InitInstructionTypeTEST,			ClearInstructionTypeTEST,			CompareInstParametersTypeTEST},
	/* INST_TYPE_MOVC */
	{InitInstructionTypeMOVC,			ClearInstructionTypeMOVC,			CompareInstParametersTypeMOVC},
	/* INST_TYPE_FARITH16 */
	{InitInstructionTypeFARITH16,		ClearInstructionTypeFARITH16,		CompareInstParametersTypeFARITH16},
	/* INST_TYPE_MEMSPILL */
	{InitInstructionTypeMEMSPILL,		ClearInstructionTypeMEMSPILL,		CompareInstParametersTypeMEMSPILL},
	/* INST_TYPE_IDXSC */
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	{InitInstructionTypeIDXSC,			ClearInstructionTypeIDXSC,			CompareInstParametersTypeIDXSC},
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	/* INST_TYPE_CVTFLT2INT */
	{InitInstructionTypeCVTFLT2INT,		ClearInstructionTypeCVTFLT2INT,		CompareInstParametersTypeCVTFLT2INT},
	/* INST_TYPE_PCK */
	{InitInstructionTypePCK,			ClearInstructionTypePCK,			CompareInstParametersTypePCK},
	/* INST_TYPE_LDSTARR */
	{InitInstructionTypeLDSTARR,		ClearInstructionTypeLDSTARR,		CompareInstParametersTypeLDSTARR},
	/* INST_TYPE_FDOTPRODUCT */
	{InitInstructionTypeFDOTPRODUCT,	ClearInstructionTypeFDOTPRODUCT,	CompareInstParametersTypeFDOTPRODUCT},
	/* INST_TYPE_DELTA */
	{InitInstructionTypeDELTA,			ClearInstructionTypeDELTA,			CompareInstParametersTypeDELTA},
	/* INST_TYPE_MOVP */
	{InitInstructionTypeMOVP,			ClearInstructionTypeMOVP,			CompareInstParametersTypeMOVP},
	/* INST_TYPE_FEEDBACKDRIVEREPILOG */
	{InitInstructionTypeFEEDBACKDRIVEREPILOG,
										ClearInstructionTypeFEEDBACKDRIVEREPILOG,
																			CompareInstParametersTypeFEEDBACKDRIVEREPILOG},
	/* INST_TYPE_BITWISE */
	{InitInstructionTypeBITWISE,		ClearInstructionTypeBITWISE,		CompareInstParametersTypeBITWISE},
	/* INST_TYPE_UNDEF */
 	{InitInstructionTypeUNDEF,			ClearInstructionTypeUNDEF,			CompareInstParametersTypeUNDEF}
};


IMG_INTERNAL 
IMG_BOOL CanHaveSourceModifier(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArg, IMG_BOOL bNegate, IMG_BOOL bAbs)
/*****************************************************************************
 FUNCTION	: CanHaveSourceModifier

 PURPOSE	: Check if an instruction can have source modifiers on a source.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction to check.
			  uArg		- Source argument.
			  bNegate	- Source modifiers.
			  bAbs

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);

	/*
		Special case for FPMA;
	*/
	if (
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			psInst->eOpcode == IFPMA_VEC ||
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
			psInst->eOpcode == IFPMA
	   )
	{
		/*
			No ABSOLUTE modifiers supported.
		*/
		if (bAbs)
		{
			return IMG_FALSE;
		}
		/*
			NEGATE modifier on source 0 only.
		*/
		if (bNegate && uArg != 0)
		{
			return IMG_FALSE;
		}
		return IMG_TRUE;
	}

	/*
		Check for an instruction which doesn't support source modifiers at all.
	*/
	if (
			(bNegate || bAbs) &&
			(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_HASSOURCEMODIFIERS) == 0
	   )
	{
		return IMG_FALSE;
	}

	#if defined(SUPPORT_SGX545)
	/*
		No negate modifiers on source 0 of MIN/MAX/MINMAX/MAXMIN
	*/
	if (bNegate)
	{
		if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_PER_INST_MOE_INCREMENTS)
		{
			if (
					uArg == 0 && 
					(
						psInst->eOpcode == IFMIN || 
						psInst->eOpcode == IFMAX || 
						psInst->eOpcode == IFMINMAX || 
						psInst->eOpcode == IFMAXMIN
					)
			   )
			{
				return IMG_FALSE;
			}
		}
	}
	/*
		No absolute modifiers on any EFO source.
	*/
	if (bAbs)
	{
		if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NEW_EFO_OPTIONS)
		{
			if (psInst->eOpcode == IEFO)
			{
				return IMG_FALSE;
			}
		}
	}
	#endif /* defined(SUPPORT_SGX545) */
	return IMG_TRUE;
}

#if defined(OUTPUT_USPBIN)
IMG_INTERNAL
IMG_UINT32 GetUSPPerSampleTemporaryCount(PINTERMEDIATE_STATE	psState,
										 IMG_UINT32				uSamplerIdx, 
										 const INST			*psInst)
/*****************************************************************************
 FUNCTION	: GetUSPPerSampleTemporaryCount

 PURPOSE	: Get the number of extra temporaries that the USP might need when
			  expanding a texture sample instruction.

 PARAMETERS	: psState		- Compiler state.
			  uSamplerIdx	- Index of the sampler used by the instruction.
			  psInst		- Instruction.

 RETURNS	: Count of temporaries.
*****************************************************************************/
{
	IMG_UINT32	uTempCount;

	uTempCount = 0;

	if ((psState->sSAProg.uTextureStateAssignedToSAs & (1U << uSamplerIdx)) == 0)
	{
		/*
			Registers to hold the texture state for each texture sample.
		*/
		uTempCount += UF_MAX_CHUNKS_PER_TEXTURE * psState->uTexStateSize;
	}
	/*
		Registers to save internal registers.
	*/
	if (psInst->eOpcode == ISMP_USP_NDR)
	{
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
		{
			uTempCount += psState->uGPISizeInScalarRegs;
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			/*
				Each internal register is 40 bits so we need two 32-bit registers to save its
				contents.
			*/
			uTempCount += psState->uGPISizeInScalarRegs * 2;
		}
	}

	if ((psState->uCompilerFlags & UF_OPENCL) != 0)
	{
		/*
			OpenCL will at most require four extra temporaries for
			performing normalisation and offsetting of the coordinates
			within a sample.
		*/
		uTempCount += 4;
	}

	return uTempCount;
}

IMG_INTERNAL
IMG_UINT32 GetUSPPerSampleUnpackTemporaryCount(void)
/*****************************************************************************
 FUNCTION	: GetUSPPerSampleUnpackTemporaryCount

 PURPOSE	: Get the number of extra temporaries that the USP might need when
			  expanding a texture sample unpack instruction.

 PARAMETERS	: psState		- Compiler state.			  
			  psInst		- Instruction.

 RETURNS	: Count of temporaries.
*****************************************************************************/
{
	return UF_MAX_CHUNKS_PER_TEXTURE + 1 /* One register for Colour Space Conversion if needed */;	
}

/*
   The maximum number of temps needed by USP for a texture write
*/
#define USP_TEXTURE_WRITE_MAX_TEMPS	(9)

IMG_INTERNAL
IMG_UINT32 GetUSPTextureWriteTemporaryCount(void)
/*****************************************************************************
 FUNCTION	: GetUSPTextureWriteTemporaryCount

 PURPOSE	: Get the number of extra temporaries that the USP might need when
			  expanding a texture write instruction.

 PARAMETERS	: psState		- Compiler state.			  
			  psInst		- Instruction.

 RETURNS	: Count of temporaries.
*****************************************************************************/
{
	return USP_TEXTURE_WRITE_MAX_TEMPS;
}
#endif /* defined(OUTPUT_USPBIN) */

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)

#if defined(OUTPUT_USPBIN)
static
IMG_BOOL SampleRequiresAlignedDestinationUSPBIN(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uDestIdx)
/*****************************************************************************
 FUNCTION	: SampleRequiresAlignedDestination_USPBIN

 PURPOSE	: Checks if a USP texture sample instruction requires the hardware
			  register number of its destination to be aligned.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  uDestIdx		- Start of the group of destinations to check for.

 RETURNS	: TRUE if alignment is required.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);

	if (psInst->asDest[uDestIdx].eFmt == UF_REGFORMAT_F32 ||
		psInst->asDest[uDestIdx].eFmt == UF_REGFORMAT_F16)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}
#endif /* defined(OUTPUT_USPBIN) */

#if defined(OUTPUT_USCHW)
static
IMG_BOOL SampleRequiresAlignedDestinationUSCHW(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uDestIdx)
/*****************************************************************************
 FUNCTION	: SampleRequiresAlignedDestination_USCHW

 PURPOSE	: Checks if a texture sample instruction requires the hardware
			  register number of its destination to be aligned.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  uDestIdx		- Start of the group of destinations to check for.

 RETURNS	: TRUE if alignment is required.
*****************************************************************************/
{
	PUNIFLEX_TEXTURE_UNPACK	psUnpack;

	ASSERT(uDestIdx == 0);

	ASSERT(psInst->u.psSmp->uTextureStage < psState->psSAOffsets->uTextureCount);
	psUnpack = &psState->asTextureUnpackFormat[psInst->u.psSmp->uTextureStage];

	/*
		Any texture sample writing more than 32-bits of data must have an aligned
		destination register number.
	*/
	if (psInst->uDestCount > 1)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}
#endif /* defined(OUTPUT_USCHW) */

#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static
IMG_VOID ProcessTextureSampleSourceRegisterGroups(PINTERMEDIATE_STATE	psState,
										 		  const INST			*psInst,
												  PREGISTER_GROUPS_DESC	psGroups)
/*****************************************************************************
 FUNCTION	: ProcessTextureSampleSourceRegisterGroups

 PURPOSE	: Returns a list of groups of sources to a texture sample instruction
			  with special restrictions on their hardware register numbers.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction.
			  psGroups	- Returns the list.

 RETURNS	: Nothing.
*****************************************************************************/
{
	HWREG_ALIGNMENT			eSourceAlignment;
	PREGISTER_GROUP_DESC	psGroup;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		eSourceAlignment = HWREG_ALIGNMENT_EVEN;
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		eSourceAlignment = HWREG_ALIGNMENT_NONE;
	}

	/*
		Process the texture coordinate sources.
	*/
	psGroup = &psGroups->asGroups[psGroups->uCount++];
	psGroup->uStart = SMP_COORD_ARG_START;
	psGroup->uCount = psInst->u.psSmp->uCoordSize;
	psGroup->eAlign = eSourceAlignment;

	/*
		Process the texture state sources.
	*/
	if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_USPTEXTURESAMPLE) == 0)
	{
		psGroup = &psGroups->asGroups[psGroups->uCount++];
		psGroup->uStart = SMP_STATE_ARG_START;
		psGroup->uCount = psState->uTexStateSize;
		psGroup->eAlign = eSourceAlignment;
	}

	/*
		Process the gradient sources.
	*/
	if (
			#if defined(OUTPUT_USPBIN)
			psInst->eOpcode == ISMPGRAD_USP ||
			#endif /* defined(OUTPUT_USPBIN) */
			psInst->eOpcode == ISMPGRAD
	   )
	{
		psGroup = &psGroups->asGroups[psGroups->uCount++];
		psGroup->uStart = SMP_GRAD_ARG_START;
		psGroup->uCount = psInst->u.psSmp->uGradSize;
		psGroup->eAlign = eSourceAlignment;
	}
	else
	/*
		Process the LOD bias/replace source.
	*/
	if (
			#if defined(OUTPUT_USPBIN)
			psInst->eOpcode == ISMPBIAS_USP ||
			psInst->eOpcode == ISMPREPLACE_USP ||
			#endif /* defined(OUTPUT_USPBIN) */
			psInst->eOpcode == ISMPBIAS ||
			psInst->eOpcode == ISMPREPLACE
	   )
	{
		psGroup = &psGroups->asGroups[psGroups->uCount++];
		psGroup->uStart = SMP_LOD_ARG_START;
		psGroup->uCount = 1;
		psGroup->eAlign = eSourceAlignment;
	}
	
	/*
		Process the sample idx source.
	*/	
	psGroup = &psGroups->asGroups[psGroups->uCount++];
	psGroup->uStart = SMP_SAMPLE_IDX_ARG_START;
	psGroup->uCount = 1;
	psGroup->eAlign = HWREG_ALIGNMENT_NONE;
}

#if defined(OUTPUT_USPBIN)
static
IMG_VOID ProcessUspSampleUnpackSourceRegisterGroups(PINTERMEDIATE_STATE		psState,
										 			const INST				*psInst,
													PREGISTER_GROUPS_DESC	psGroups)
/*****************************************************************************
 FUNCTION	: ProcessUspSampleUnpackSourceRegisterGroups

 PURPOSE	: Return a list of groups of sources to a usp sample unpack instruction
			  with special restrictions on their hardware register numbers.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction.
			  psGroups	- Returns the list.

 RETURNS	: Nothing.
*****************************************************************************/
{
	HWREG_ALIGNMENT			eSourceAlignment;
	PREGISTER_GROUP_DESC	psGroup;
	IMG_UINT32				uArgStart;

	PVR_UNREFERENCED_PARAMETER(psState);

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		eSourceAlignment = HWREG_ALIGNMENT_EVEN;
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		eSourceAlignment = HWREG_ALIGNMENT_NONE;
	}

	/*
		Process the texture coordinate sources.
	*/
	psGroup = &psGroups->asGroups[psGroups->uCount++];
	psGroup->uStart = 0;
	psGroup->uCount = UF_MAX_CHUNKS_PER_TEXTURE;
	psGroup->eAlign = eSourceAlignment;

	for (uArgStart = /*SMPUNPACK_TFDATA_ARG_START*/ UF_MAX_CHUNKS_PER_TEXTURE; uArgStart < psInst->uArgumentCount; uArgStart++)
	{
		/*
			Process the raw results of the texture sample.
		*/
		psGroup = &psGroups->asGroups[psGroups->uCount++];
		psGroup->uStart = uArgStart;
		psGroup->uCount = 1;
		psGroup->eAlign = HWREG_ALIGNMENT_NONE;
	}
}
#endif /* defined(OUTPUT_USPBIN) */

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_VOID ProcessC10VectorSourceRegisterGroups(const INST			*psInst,
											  PREGISTER_GROUPS_DESC	psGroups)
/*****************************************************************************
 FUNCTION	: ProcessC10VectorSourceRegisterGroups

 PURPOSE	: Returns a list of groups of sources to a C10/U8 instruction
			  on cores with the vector instruction set with special restrictions 
			  on their hardware register numbers.

 PARAMETERS	: psInst	- Instruction.
			  pfCB		- Function to call.
			  pvContext	- Parameter to pass to the function.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uGroupCount;
	IMG_UINT32	uGroupIdx;

	uGroupCount = psInst->uArgumentCount;
	uGroupCount = uGroupCount / SCALAR_REGISTERS_PER_C10_VECTOR;

	for (uGroupIdx = 0; uGroupIdx < uGroupCount; uGroupIdx++)
	{
		IMG_UINT32		uArgStart;
		PARG			psArg1, psArg2;
		IMG_UINT32		uArgCount;
		HWREG_ALIGNMENT	eArgAlignment;

		uArgStart = uGroupIdx * SCALAR_REGISTERS_PER_C10_VECTOR;
		psArg1 = &psInst->asArg[uArgStart + 0];
		psArg2 = &psInst->asArg[uArgStart + 1];

		if (psArg1->eFmt == UF_REGFORMAT_C10)
		{
			if (psArg2->uType != USC_REGTYPE_UNUSEDSOURCE)
			{
				uArgCount = 2;
			}
			else
			{
				uArgCount = 1;
			}
			eArgAlignment = HWREG_ALIGNMENT_EVEN;
		}
		else
		{
			uArgCount = 1;
			eArgAlignment = HWREG_ALIGNMENT_NONE;
		}

		psGroups->asGroups[uGroupIdx].uStart = uArgStart;
		psGroups->asGroups[uGroupIdx].uCount = uArgCount;
		psGroups->asGroups[uGroupIdx].eAlign = eArgAlignment;
	}
	psGroups->uCount = uGroupCount;
}

static
IMG_VOID ProcessVectorSourceRegisterGroups(PINTERMEDIATE_STATE		psState,
										   const INST				*psInst,
										   PREGISTER_GROUPS_DESC	psGroups)
/*****************************************************************************
 FUNCTION	: ProcessVectorSourceRegisterGroups

 PURPOSE	: Return a list of groups of sources to a vector instruction
			  with special restrictions on their hardware register numbers.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction.
			  psGroups	- Returns the list.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uGroupCount;
	IMG_UINT32	uGroupIdx;

	uGroupCount = psInst->uArgumentCount;
	uGroupCount = uGroupCount / SOURCE_ARGUMENTS_PER_VECTOR;

	for (uGroupIdx = 0; uGroupIdx < uGroupCount; uGroupIdx++)
	{
		PARG		psGPIArg;
		IMG_UINT32	uUSArgStart;
		IMG_UINT32	uUSArgEnd;

		psGPIArg = &psInst->asArg[uGroupIdx * SOURCE_ARGUMENTS_PER_VECTOR + 0];
		DBG_ASSERT(psGPIArg->uType == USC_REGTYPE_UNUSEDSOURCE);

		uUSArgStart = uGroupIdx * SOURCE_ARGUMENTS_PER_VECTOR + 1;

		switch (psInst->u.psVec->aeSrcFmt[uGroupIdx])
		{
			case UF_REGFORMAT_C10: uUSArgEnd = uUSArgStart + SCALAR_REGISTERS_PER_C10_VECTOR; break;
			case UF_REGFORMAT_F16: uUSArgEnd = uUSArgStart + SCALAR_REGISTERS_PER_F16_VECTOR; break;
			case UF_REGFORMAT_F32: uUSArgEnd = uUSArgStart + SCALAR_REGISTERS_PER_F32_VECTOR; break;
			default: imgabort();
		}

		while (uUSArgEnd > uUSArgStart && psInst->asArg[uUSArgEnd - 1].uType == USC_REGTYPE_UNUSEDSOURCE)
		{
			uUSArgEnd--;
		}

		psGroups->asGroups[uGroupIdx].uStart = uUSArgStart;
		psGroups->asGroups[uGroupIdx].uCount = uUSArgEnd - uUSArgStart;
		if (
				psInst->eOpcode == IVDUAL && 
				uGroupIdx == VDUAL_SRCSLOT_UNIFIEDSTORE && 
				!IsVDUALUnifiedStoreVectorSource(psState, psInst)
			)
		{
			psGroups->asGroups[uGroupIdx].eAlign = HWREG_ALIGNMENT_NONE;
		}
		else
		{
			psGroups->asGroups[uGroupIdx].eAlign = HWREG_ALIGNMENT_EVEN;
		}
	}
	psGroups->uCount = uGroupCount;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

IMG_INTERNAL IMG_VOID GetSourceRegisterGroups(PINTERMEDIATE_STATE		psState,
												const INST				*psInst,
												PREGISTER_GROUPS_DESC	psGroups)
/*****************************************************************************
 FUNCTION	: GetSourceRegisterGroups

 PURPOSE	: Return a list of group of sources to an instruction
			  with special restrictions on their hardware register numbers.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction.
			  psGroups	- Returns the list.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psGroups->uCount = 0;
	if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_DEPENDENTTEXTURESAMPLE) != 0)
	{
		ProcessTextureSampleSourceRegisterGroups(psState, psInst, psGroups);
		return;
	}
	if (psInst->eOpcode == IFDPC)
	{
		/*
			The two source arguments to FDPC require consecutive hardware register numbers.
		*/
		psGroups->asGroups[0].uStart = 0;
		psGroups->asGroups[0].uCount = FDPC_ARGUMENT_COUNT;
		psGroups->asGroups[0].eAlign = HWREG_ALIGNMENT_NONE;
		psGroups->uCount = 1;
		return;
	}
	#if defined(OUTPUT_USPBIN)
	if (psInst->eOpcode == ISMPUNPACK_USP)
	{
		ProcessUspSampleUnpackSourceRegisterGroups(psState, psInst, psGroups);
		return;
	}
	#endif /* defined(OUTPUT_USPBIN) */
	
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) != 0)
	{
		ProcessVectorSourceRegisterGroups(psState, psInst, psGroups);
		return;
	}
	if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_C10VECTORSRCS) != 0)
	{
		ProcessC10VectorSourceRegisterGroups(psInst, psGroups);
		return;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	#if 0
	#if defined (EXECPRED)
	if (psInst->eOpcode == ICNDSM)
	{
		/* CNDSM sources are broken on some cores. */
		if ((psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_33134) != 0)
		{
			psGroups->asGroups[0].uStart = 3;
			psGroups->asGroups[0].uCount = 1;
			psGroups->asGroups[0].eAlign = HWREG_ALIGNMENT_EVEN;
			psGroups->uCount = 1;
			return;
		}
	}
	#endif /* defined (EXECPRED) */
	#endif
}

IMG_INTERNAL
IMG_VOID ProcessSourceRegisterGroups(PINTERMEDIATE_STATE	psState,
									 PINST					psInst,
									 PPROCESS_GROUP_CB		pfCB,
									 IMG_PVOID				pvContext)
/*****************************************************************************
 FUNCTION	: ProcessSourceRegisterGroups

 PURPOSE	: Call a function for each group of sources to an instruction
			  with special restrictions on their hardware register numbers.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction.
			  pfCB		- Function to call.
			  pvContext	- Parameter to pass to the function.

 RETURNS	: Nothing.
*****************************************************************************/
{
	REGISTER_GROUPS_DESC	sDesc;

	GetSourceRegisterGroups(psState, psInst, &sDesc);

	if (sDesc.uCount > 0)
	{
		IMG_UINT32	uGroupIdx;

		for (uGroupIdx = 0; uGroupIdx < sDesc.uCount; uGroupIdx++)
		{
			PREGISTER_GROUP_DESC	psGroup = &sDesc.asGroups[uGroupIdx];

			pfCB(psState, psInst, IMG_FALSE /* bDest */, psGroup->uStart, psGroup->uCount, psGroup->eAlign, pvContext);
		}
	}
	else
	{	
		IMG_UINT32	uArgIdx;

		for (uArgIdx = 0; uArgIdx < psInst->uArgumentCount; uArgIdx++)
		{
			pfCB(psState, psInst, IMG_FALSE /* bDest */, uArgIdx, 1 /* uGroupCount */, HWREG_ALIGNMENT_NONE, pvContext);
		}
	}
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_VOID ProcessVDUALDestRegisterGroups(PINTERMEDIATE_STATE		psState,
										const INST				*psInst,
										PREGISTER_GROUPS_DESC	psGroups)
/*****************************************************************************
 FUNCTION	: ProcessVDUALDestRegisterGroups

 PURPOSE	: Return a list of groups of destinations to a VDUAL instruction
			  with special restrictions on their hardware register numbers.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction.
			  pfCB		- Function to call.
			  pvContext	- Parameter to pass to the function.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uGroupIdx;

	ASSERT(!psInst->u.psVec->bWriteYOnly);

	for (uGroupIdx = 0; uGroupIdx < VDUAL_DESTSLOT_COUNT; uGroupIdx++)
	{
		IMG_UINT32	uArgStart;
		IMG_UINT32	uArgCount;

		ASSERT(uGroupIdx == VDUAL_DESTSLOT_GPI || uGroupIdx == VDUAL_DESTSLOT_UNIFIEDSTORE);

		uArgStart = uGroupIdx * VECTOR_LENGTH;
		ASSERT(psInst->uDestCount > uArgStart);
		uArgCount = min(psInst->uDestCount - uArgStart, VECTOR_LENGTH);

		while (uArgCount > 0 && psInst->asDest[uArgStart + uArgCount - 1].uType == USC_REGTYPE_UNUSEDDEST)
		{
			uArgCount--;
		}

		psGroups->asGroups[uGroupIdx].uStart = uArgStart;
		psGroups->asGroups[uGroupIdx].uCount = uArgCount;
		if (uGroupIdx == VDUAL_DESTSLOT_UNIFIEDSTORE && !IsVDUALUnifiedStoreVectorResult(psInst))
		{
			psGroups->asGroups[uGroupIdx].eAlign = HWREG_ALIGNMENT_NONE;
		}
		else
		{
			psGroups->asGroups[uGroupIdx].eAlign = HWREG_ALIGNMENT_EVEN;
		}
	}
	psGroups->uCount = VDUAL_DESTSLOT_COUNT;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_BOOL RequiresAlignedDest(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uDestIdx)
/*****************************************************************************
 FUNCTION	: RequiresAlignedDest

 PURPOSE	: Check for an instruction which requires the first destination
			  argument to have an even aligned hardware destination register.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction to check.
			  uDestIdx	- Start of the group of destinations to check for.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);

	/*
		Vector instructions require aligned destinations unless we are writing only one register
		and the instruction generates a single channel result - in this case we can write an odd register
		by using the Y channel.
	*/
	if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORDEST)
	{
		if ((g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_VECTORREPLICATEDRESULT) != 0 && psInst->uDestCount == 1)
		{
			return IMG_FALSE;
		}
		return IMG_TRUE;
	}

	/*
		Instructions writing C10 format data require aligned destinations.
	*/
	if (
			(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_C10VECTORDEST) != 0 && 
			psInst->uDestCount > 0 && 
			psInst->asDest[0].eFmt == UF_REGFORMAT_C10
	    )
	{
		return IMG_TRUE;
	}
	
	/*
		A memory load instruction loading qword sized data requires an aligned destination.
	*/
	if (psInst->eOpcode == ILDAQ)
	{
		return IMG_TRUE;
	}

	/*
		On cores with the vector instruction set texture samples might require aligned destinations.
	*/
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_TEXTURESAMPLE) != 0)
		{
			return CHOOSE_USPBIN_OR_USCHW_FUNC(SampleRequiresAlignedDestination, (psState, psInst, uDestIdx));
		}
		#if defined(OUTPUT_USPBIN)
		if (psInst->eOpcode == ISMPUNPACK_USP)
		{
			return SampleRequiresAlignedDestinationUSPBIN(psState, psInst, uDestIdx);
		}
		#endif /* defined(OUTPUT_USPBIN) */
	}
	return IMG_FALSE;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

#if defined(OUTPUT_USPBIN)
static
IMG_VOID ProcessUSPSMPDestRegisterGroups(PINTERMEDIATE_STATE	psState,
										 const INST			*psInst,
										 PREGISTER_GROUPS_DESC	psGroups)
/*****************************************************************************
 FUNCTION	: ProcessUSPSMPDestRegisterGroups

 PURPOSE	: Return a list of the group of destinations to a USPSMP instruction
			  with special restrictions on their hardware register numbers.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction.
			  psGroups	- Returns the list of groups.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uGroupIdx;

	for (uGroupIdx = 0; uGroupIdx < USPSMP_DESTSLOT_COUNT; uGroupIdx++)
	{
		IMG_UINT32	uArgStart;
		IMG_UINT32	uArgCount;

		uArgStart = uGroupIdx * UF_MAX_CHUNKS_PER_TEXTURE;
		ASSERT(psInst->uDestCount > uArgStart);
		uArgCount = min(psInst->uDestCount - uArgStart, UF_MAX_CHUNKS_PER_TEXTURE);

		while (uArgCount > 0 && psInst->asDest[uArgStart + uArgCount - 1].uType == USC_REGTYPE_UNUSEDDEST)
		{
			uArgCount--;
		}

		psGroups->asGroups[uGroupIdx].uStart = uArgStart;
		psGroups->asGroups[uGroupIdx].uCount = uArgCount;
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) 
		if (RequiresAlignedDest(psState, psInst, uArgStart))
		{
			psGroups->asGroups[uGroupIdx].eAlign = HWREG_ALIGNMENT_EVEN;	
		}
		else
#endif
		{
			psGroups->asGroups[uGroupIdx].eAlign = HWREG_ALIGNMENT_NONE;	
		}
	}
	psGroups->uCount = USPSMP_DESTSLOT_COUNT;
}
#endif /* defined(OUTPUT_USPBIN) */

IMG_INTERNAL IMG_VOID GetDestRegisterGroups(PINTERMEDIATE_STATE		psState,
											const INST				*psInst,
											PREGISTER_GROUPS_DESC	psGroups)
/*****************************************************************************
 FUNCTION	: GetDestRegisterGroups

 PURPOSE	: Return a list of group of destinations to an instruction
			  with special restrictions on their hardware register numbers.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction.
			  psGroups	- Returns the list.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	
	psGroups->uCount = 0;
	if (
			(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_TEXTURESAMPLE) != 0 ||
			(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_MULTIPLEDEST) != 0
	   )
	{
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (psInst->eOpcode == IVDUAL)
		{
			ProcessVDUALDestRegisterGroups(psState, psInst, psGroups);
		}
		else if (psInst->eOpcode == IVTEST)
		{
			if (psInst->uDestCount > VTEST_PREDICATE_ONLY_DEST_COUNT)
			{
				PREGISTER_GROUP_DESC	psGroup;

				psGroup = &psGroups->asGroups[psGroups->uCount++];
				psGroup->uStart = VTEST_UNIFIEDSTORE_DESTIDX;
				psGroup->uCount = psInst->uDestCount - VTEST_UNIFIEDSTORE_DESTIDX;
				psGroup->eAlign = HWREG_ALIGNMENT_EVEN;
			}
		}
		else if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORDEST) != 0 && psInst->u.psVec->bWriteYOnly)
		{
			PREGISTER_GROUP_DESC	psGroup;

			ASSERT(psInst->uDestCount == 2);
			ASSERT(psInst->auDestMask[0] == 0);

			psGroup = &psGroups->asGroups[psGroups->uCount++];
			psGroup->uStart = 1;
			psGroup->uCount = 1;
			psGroup->eAlign = HWREG_ALIGNMENT_ODD;
		}
		else if (psInst->eOpcode == IVTESTBYTEMASK)
		{
			PREGISTER_GROUP_DESC	psGroup;

			ASSERT(psInst->uDestCount == 2);

			psGroup = &psGroups->asGroups[psGroups->uCount++];
			psGroup->uStart = 0;
			psGroup->uCount = 2;
			psGroup->eAlign = HWREG_ALIGNMENT_EVEN;
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		#if defined(OUTPUT_USPBIN)
		if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_USPTEXTURESAMPLE) != 0)
		{
			ProcessUSPSMPDestRegisterGroups(psState, psInst, psGroups);
		}
		else
		#endif /* defined(OUTPUT_USPBIN) */
		{
			PREGISTER_GROUP_DESC	psGroup;

			psGroup = &psGroups->asGroups[psGroups->uCount++];
			psGroup->uStart = 0;
			psGroup->uCount = psInst->uDestCount;
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			if (RequiresAlignedDest(psState, psInst, 0 /* uDestIdx */))
			{
				psGroup->eAlign = HWREG_ALIGNMENT_EVEN; 
			}
			else
#endif
			{
				psGroup->eAlign = HWREG_ALIGNMENT_NONE;
			}
		}
		return;
	}
}

IMG_INTERNAL
IMG_VOID ProcessDestRegisterGroups(PINTERMEDIATE_STATE	psState,
								   PINST				psInst,
								   PPROCESS_GROUP_CB	pfCB,
								   IMG_PVOID			pvContext)
/*****************************************************************************
 FUNCTION	: ProcessDestRegisterGroups

 PURPOSE	: Call a function for each group of destinations to an instruction
			  with special restrictions on their hardware register numbers.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction.
			  pfCB		- Function to call.
			  pvContext	- Parameter to pass to the function.

 RETURNS	: Nothing.
*****************************************************************************/
{
	REGISTER_GROUPS_DESC	sDesc;

	GetDestRegisterGroups(psState, psInst, &sDesc);

	if (sDesc.uCount > 0)
	{
		IMG_UINT32	uGroupIdx;

		for (uGroupIdx = 0; uGroupIdx < sDesc.uCount; uGroupIdx++)
		{
			PREGISTER_GROUP_DESC	psGroup = &sDesc.asGroups[uGroupIdx];

			pfCB(psState, psInst, IMG_TRUE /* bDest */, psGroup->uStart, psGroup->uCount, psGroup->eAlign, pvContext);
		}
	}
	else
	{
		IMG_UINT32	uDestIdx;

		for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
		{
			pfCB(psState, psInst, IMG_TRUE /* bDest */, uDestIdx, 1 /* uGroupCount */, HWREG_ALIGNMENT_NONE, pvContext);
		}
	}
}

IMG_INTERNAL
IMG_BOOL IsArgInRegisterGroup(PINTERMEDIATE_STATE		psState,
							  const INST				*psInst,
							  IMG_UINT32				uArg)
/*****************************************************************************
 FUNCTION	: IsArgInRegisterGroup

 PURPOSE	: Check if an argument to an instruction is part of a group that must
			  have consecutive register numbers.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction to check.
			  uArg		- Argument index to check.
			  puGroupOffset	
						- If non-NULL returns the offset of the argument from the
						first argument in the group.
			  puGroupCount
						- If non-NULL returns the count of arguments in the group.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	/*
		Coordinate, state and gradient arguments are part of a register group.
	*/
	if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_TEXTURESAMPLE) != 0)
	{
		if (uArg >= SMP_COORD_ARG_START && uArg < (SMP_COORD_ARG_START + psInst->u.psSmp->uCoordSize))
		{
			return IMG_TRUE;
		}
		if (uArg >= SMP_STATE_ARG_START && uArg < (SMP_STATE_ARG_START + psState->uTexStateSize))
		{
			return IMG_TRUE;
		}
		if (uArg >= SMP_GRAD_ARG_START && uArg <= (SMP_GRAD_ARG_START + psInst->u.psSmp->uGradSize))
		{
			return IMG_TRUE;
		}
		if (uArg == SMP_LOD_ARG_START && (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
		{
			return IMG_TRUE;
		}
	}
	/*
		Both DPC sources are part of a register group.
	*/
	if (psInst->eOpcode == IFDPC)
	{
		return IMG_TRUE;
	}
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC)
	{
		if ((uArg % SOURCE_ARGUMENTS_PER_VECTOR) != 0)
		{
			return IMG_TRUE;
		}
	}
	if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_C10VECTORSRCS)
	{
		return IMG_TRUE;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_BOOL GetRegisterGroupByArg(PINTERMEDIATE_STATE	psState, 
							   PINST				psInst, 
							   IMG_BOOL				bDest, 
							   IMG_UINT32			uArgIdx, 
							   PREGISTER_GROUP_DESC psFoundGroup)
/*****************************************************************************
 FUNCTION	: GetRegisterGroupByArg

 PURPOSE	: If an instruction source or destination is part of a group of
			  arguments requiring consecutive hardware register numbers then
			  return the details of the group.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction to check.
			  bDest		- TRUE to check for destinations; FALSE to check for
						sources.
			  uArgIdx	- Index of the argument.
			  psFoundGroup
						- Returns details of any found group.

 RETURNS	: TRUE if the argument is part of a group.
*****************************************************************************/
{
	REGISTER_GROUPS_DESC	sGroups;
	IMG_UINT32				uGroup;

	if (bDest)
	{
		GetDestRegisterGroups(psState, psInst, &sGroups);
	}
	else
	{
		GetSourceRegisterGroups(psState, psInst, &sGroups);
	}

	/*
		Check if this argument is part of one of the groups.
	*/
	for (uGroup = 0; uGroup < sGroups.uCount; uGroup++)
	{
		PREGISTER_GROUP_DESC	psGroup = &sGroups.asGroups[uGroup];

		if (uArgIdx >= psGroup->uStart && uArgIdx < (psGroup->uStart + psGroup->uCount))
		{
			*psFoundGroup = *psGroup;
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_BOOL SupportsEndFlag(PINTERMEDIATE_STATE psState, PCINST psInst)
/*****************************************************************************
 FUNCTION	: SupportsEndFlag

 PURPOSE	: Check if an instruction supports the END flag.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	#if defined(SUPPORT_SGX545)
	/*
		The FMAD16 instruction doesn't support the END flag on cores where it supports swizzles.
	*/
	if (
			(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_FARITH16) != 0 &&
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_FMAD16_SWIZZLES) != 0
	   )
	{
		return IMG_FALSE;
	}
	#endif /* defined(SUPPORT_SGX545) */

	if ((g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_DOESNTSUPPORTENDFLAG) != 0)
	{
		return IMG_FALSE;
	}

	ASSERT(psInst->eOpcode != IBR);
	return IMG_TRUE;
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
IMG_INTERNAL
IMG_BOOL InstHasVectorDest(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uDestIdx)
/*****************************************************************************
 FUNCTION	: InstHasVectorDest

 PURPOSE	: Check if an instruction writes a vector sized destination.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (	
			(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORDEST) != 0 && 
			psInst->asDest[uDestIdx].eFmt != UF_REGFORMAT_C10
		)
	{
		if (psInst->eOpcode == IVTEST && uDestIdx < VTEST_PREDICATE_ONLY_DEST_COUNT)
		{
			return IMG_FALSE;
		}
		return IMG_TRUE;
	}
	if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_DEPENDENTTEXTURESAMPLE) != 0)
	{
		IMG_UINT32	uPlane;

		#if defined(OUTPUT_USPBIN)
		if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_USPTEXTURESAMPLE) != 0)
		{
			return IMG_FALSE;
		}
		else
		#endif /* defined(OUTPUT_USPBIN) */
		{
			for (uPlane = 0; uPlane < psInst->u.psSmp->uPlaneCount; uPlane++)
			{
				PSAMPLE_RESULT_CHUNK	psPlane;
			
				psPlane = &psInst->u.psSmp->asPlanes[psInst->u.psSmp->uFirstPlane + uPlane];
				if (uDestIdx < psPlane->uSizeInRegs)
				{
					return psPlane->bVector;
				}
				uDestIdx -= psPlane->uSizeInRegs;
			}
			imgabort();
		}
	}
	return IMG_FALSE;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

IMG_INTERNAL
IMG_BOOL HasC10FmtControl(PCINST psInst)
/*****************************************************************************
 FUNCTION	: HasC10FmtControl

 PURPOSE	: Check if an instruction can interpret its sources as either U8
			  or C10.

 PARAMETERS	: psInst	- Instruction to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (psInst->eOpcode == ITESTPRED || psInst->eOpcode == ITESTMASK)
	{
		if (psInst->u.psTest->eAluOpcode == IFPADD8 ||
			psInst->u.psTest->eAluOpcode == IFPSUB8)
		{
			return IMG_TRUE;
		}
		return IMG_FALSE;
	}
	if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_C10FMTCTL)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_BOOL RequiresGradients(PCINST psInst)
/*****************************************************************************
 FUNCTION	: RequiresGradients

 PURPOSE	: Check if an instruction requires gradients for its sources.

 PARAMETERS	: psInst	- Instruction to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IOPCODE		eOpcode;

	eOpcode = psInst->eOpcode;
	if (eOpcode == ITESTPRED || eOpcode == ITESTMASK)
	{
		eOpcode = psInst->u.psTest->eAluOpcode;
	}
	if (g_psInstDesc[eOpcode].uFlags & DESC_FLAGS_REQUIRESGRADIENTS)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

#if defined(SUPPORT_SGX545)
IMG_INTERNAL
IMG_BOOL SupportsPerInstMoeIncrements(PINTERMEDIATE_STATE	psState,
									  const INST			*psInst)
/*****************************************************************************
 FUNCTION	: SupportsPerInstMoeIncrements

 PURPOSE	: Checks if MOE increments encoded directly in the instruction
			  are supported.

 PARAMETERS:  psState					- Compiler state.
			  psInst					- The instruction to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_PER_INST_MOE_INCREMENTS)
	{
		if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_PERINSTMOE) != 0)
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}
#endif /* defined(SUPPORT_SGX545) */

IMG_INTERNAL
IMG_BOOL UseRepeatMaskOnly(PINTERMEDIATE_STATE psState, PCINST psInst)
/*****************************************************************************
 FUNCTION	: UseRepeatMaskOnly

 PURPOSE	: Checks if an instruction must use an MOE repeat mask.

 PARAMETERS:  psState					- Compiler state.
			  psInst					- The instruction to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		return IMG_FALSE;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	return (g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_REPEATMASKONLY) ? IMG_TRUE : IMG_FALSE;
}

IMG_INTERNAL
INST_PRED GetInstPredicateSupport(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: GetInstPredicateSupport

 PURPOSE	: Checks for the range of predicate options supported by an
			  instruction.

 PARAMETERS:  psState					- Compiler state.
			  psInst					- The instruction to check.

 RETURNS	: The supported range.
*****************************************************************************/
{
	INST_PRED	ePredicateSupport;

	PVR_UNREFERENCED_PARAMETER(psState);

	ePredicateSupport = g_psInstDesc[psInst->eOpcode].ePredicateSupport;

	#if defined(SUPPORT_SGX545)
	/*
		For cores which support the new EFO options: the EFO instruction only supports
		a short predicate.
	*/
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NEW_EFO_OPTIONS) != 0)
	{
		if (psInst->eOpcode == IEFO)
		{
			ASSERT(ePredicateSupport == INST_PRED_EXTENDED);
			ePredicateSupport = INST_PRED_SHORT;
		}
	}

	/*
		For cores affected by BRN33442: the IMA32 and IDIV instructions don't support a predicate.
	*/
	if (psInst->eOpcode == IIMA32 || psInst->eOpcode == IIDIV32)
	{
		ASSERT(ePredicateSupport == INST_PRED_EXTENDED);
		if ((psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_33442) != 0)
		{
			ePredicateSupport = INST_PRED_NONE;
		}
	}
	#endif /* defined(SUPPORT_SGX545) */

	return ePredicateSupport;
}

IMG_INTERNAL
IMG_BOOL CanUseRepeatMask(PINTERMEDIATE_STATE	psState,
						  const INST			*psInst)
/*****************************************************************************
 FUNCTION	: CanUseRepeatMask

 PURPOSE	: Checks if an instruction can use an MOE repeat mask.

 PARAMETERS:  psState					- Compiler state.
			  psInst					- The instruction to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);

	#if defined(SUPPORT_SGX545)
	/*
		Instructions which can encoded the MOE increments directly in the
		instruction don't support repeat masks.
	*/
	if (SupportsPerInstMoeIncrements(psState, psInst))
	{
		return IMG_FALSE;
	}
	#endif /* defined(SUPPORT_SGX545) */

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		The bitwise instructions don't support a repeat mask on some cores. On SGX543 the MOV
		instruction will be mapped to OR by the assembler.
	*/
	if (
			(psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_BITWISE_NO_REPEAT_MASK) != 0 &&
			(
				(g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_BITWISE) != 0 ||
				psInst->eOpcode == IMOV
			)
	   )
	{
		return IMG_FALSE;
	}

	/*
		The TEST instruction no longer supports a repeat mask on cores with the vector instruction set.
	*/
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		if (psInst->eOpcode == ITESTMASK || psInst->eOpcode == ITESTPRED)
		{
			return IMG_FALSE;
		}
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Otherwise return the state from the instruction description.
	*/
	return g_psInstDesc[psInst->eOpcode].bCanUseRepeatMask;
}

IMG_INTERNAL
IMG_UINT32 GetMaxRepeatCountInst(PINTERMEDIATE_STATE	psState,
								 const INST				*psInst)
/*****************************************************************************
 FUNCTION	: GetMaxRepeatCountInst

 PURPOSE	: Get the maximum MOE repeat count supported by an instruction.

 PARAMETERS:  psState					- Compiler state.
			  psInst					- The instruction to check.

 RETURNS	: The maximum repeat count.
*****************************************************************************/
{
	IMG_UINT32	uMaxRepeatCount;

	PVR_UNREFERENCED_PARAMETER(psState);

	/*
		Get the default maximum repeat count from the instruction
		description.
	*/
	uMaxRepeatCount = g_psInstDesc[psInst->eOpcode].uMaxRepeatCount;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) || defined(SUPPORT_SGX545)
	/*
		Reduced repeat counts for SMP instructions on some cores.
	*/
	if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_TEXTURESAMPLE)
	{
		if (psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_SMP_REDUCEDREPEATCOUNT)
		{
			uMaxRepeatCount = EURASIA_USE1_SMP_RMSKCNT_MAXCOUNT;
		}
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) || defined(SUPPORT_SGX545) */
	
	#if defined (SUPPORT_SGX545)
	/*
		Reduced repeat counts for cores/instructions which encode the MOE increments
		directly in the instruction.
	*/
	if (SupportsPerInstMoeIncrements(psState, psInst))
	{
		uMaxRepeatCount = SGX545_USE1_FLOAT_RCOUNT_MAXIMUM;
	}

	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_FMAD16_SWIZZLES)
	{
		if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_FARITH16)
		{
			uMaxRepeatCount = SGX545_USE1_FARITH16_RCNT_MAXIMUM;
		}
	}
	#endif /* defined(SUPPORT_SGX545) */

	return uMaxRepeatCount;
}

IMG_INTERNAL
IMG_BOOL CanRepeatInst(PINTERMEDIATE_STATE	psState,
					   const INST			*psInst)
/*****************************************************************************
 FUNCTION	: CanRepeatInst

 PURPOSE	: Check if an instruction can use MOE repeats.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	/*
		If we are in per-instance mode then only one iteration is allowed for
		LODM=NONE/BIAS.
	*/
	if (
			RequiresGradients(psInst) &&
			(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_TEXTURESAMPLE) != 0 &&
			psState->uNumDynamicBranches > 0
	   )
	{
		return IMG_FALSE;
	}
	if (psInst->eOpcode == ITESTPRED)
	{
		/* Repeated tests are broken on some cores. */
		if (
				(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_23062) != 0 && 
				psInst->u.psTest->eAluOpcode != IFDP
		   )
		{
			return IMG_FALSE;
		}
		/* Repeated dotproduct tests are broken on some cores. */
		if (
				(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_23164) != 0 && 
				psInst->u.psTest->eAluOpcode == IFDP
		   )
		{
			return IMG_FALSE;
		}
	}
	#if defined(SUPPORT_SGX545)
	/*
		Repeated IDIV instructions are broken on some cores.
	*/
	if (psInst->eOpcode == IIDIV32 && (psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_26570))
	{
		return IMG_FALSE;
	}
	#endif /* defined(SUPPORT_SGX545) */
	/*
		Repeated texture sample instructions are broken on some cores.
	*/
	if (
			(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_TEXTURESAMPLE) != 0 &&
			(
				(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_26570) != 0 ||
				(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_26681) != 0
			)
	   )
	{
		return IMG_FALSE;
	}
	if (!g_psInstDesc[psInst->eOpcode].bCanRepeat)
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL HasF16F32SelectInst(PCINST psInst)
/*****************************************************************************
 FUNCTION	: HasF16F32SelectInst

 PURPOSE	: Check if an instruction can interpret its source as either F32
			  or F16.

 PARAMETERS	: psInst	- Instruction to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IOPCODE	eAluOpcode;

	if (psInst->eOpcode == ITESTPRED)
	{
		eAluOpcode = psInst->u.psTest->eAluOpcode;

		/*
			As a special case TEST with a complex op
			ALUOP doesn't support F16 format arguments.
		*/
		if ((g_psInstDesc[eAluOpcode].uFlags & DESC_FLAGS_COMPLEXOP) != 0)
		{
			return IMG_FALSE;
		}
	}
	else
	{
		eAluOpcode = psInst->eOpcode;
	}

	/*
		Otherwise look at the instruction flags.
	*/
	if (g_psInstDesc[eAluOpcode].uFlags & DESC_FLAGS_F16F32SELECT)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

/*********************************************************************************
 Function			: WroteAllLiveChannels

 Description		: Check if an instruction completely writes one of its destinations.
 
 Parameters			: psInst			- Instruction to check.
					  uDestIdx			- Destination index to check.

 Globals Effected	: None

 Return				: TRUE or FALSE.
*********************************************************************************/
IMG_INTERNAL
IMG_BOOL WroteAllLiveChannels(const INST			*psInst,
							  IMG_UINT32			uDestIdx)
{
	IMG_UINT32	uLiveChansInDest;
	IMG_UINT32	uDestMask;

	PVR_UNREFERENCED_PARAMETER(uDestIdx);

	uLiveChansInDest = psInst->auLiveChansInDest[uDestIdx]; 
	uDestMask = psInst->auDestMask[uDestIdx];

	if	((uLiveChansInDest & ~uDestMask) == 0)
	{
		return IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
}

IMG_INTERNAL
IMG_BOOL CanUseDest(PINTERMEDIATE_STATE psState, 
					PCINST				psInst, 
					IMG_UINT32			uDestIdx, 
					IMG_UINT32			uDestType, 
					IMG_UINT32			uDestIndexType)
/*****************************************************************************
 FUNCTION	: CanUseDest

 PURPOSE	: Check a register is valid as the destination to an instruction.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction to check.
			  uDestIdx	- Destination to check.
			  uDestType	- Register to check.
			  uDestIndex

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0U)
	{
		PPIXELSHADER_STATE	psPS = psState->sShader.psPS;

		if	(
				(psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL) &&
				(psState->psSAOffsets->uPackDestType == USEASM_REGTYPE_OUTPUT) && 
				(psPS->uColOutputCount > 1)
			)
		{
			if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORDEST) == 0U)
			{
				return IMG_FALSE;
			}
		}
	}
	#endif /* #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		PFN_CAN_USE_DEST	pfnCanUseDest;

		ASSERT(psInst->eOpcode < (sizeof(g_psInstDesc) / sizeof(g_psInstDesc[0])));
		pfnCanUseDest = g_psInstDesc[psInst->eOpcode].pfCanUseDest;

		if (pfnCanUseDest != NULL)
		{
			return pfnCanUseDest(psState, uDestIdx, uDestType, uDestIndexType);
		}
		else
		{
			/* Any destination register is valid. */
			return IMG_TRUE;
		}
	}
}

IMG_INTERNAL
IMG_BOOL IsSrc0StandardBanks(IMG_UINT32 uType, IMG_UINT32 uIndexType)
/*****************************************************************************
 FUNCTION	: IsSrc0StandardBanks

 PURPOSE	: Check if a register bank is in the set allowed for hardware source 0
			  without the extended bit.

 PARAMETERS	: uType			- Register bank to check.
			  uIndexType

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (uIndexType != USC_REGTYPE_NOINDEX)
	{
		return IMG_FALSE;
	}
	if (uType != USEASM_REGTYPE_TEMP && 
		uType != USEASM_REGTYPE_PRIMATTR && 
		uType != USEASM_REGTYPE_FPINTERNAL &&
		uType != USC_REGTYPE_GSINPUT &&
		uType != USC_REGTYPE_REGARRAY)
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

static
IMG_BOOL IsSrc0ExtendedBanks(IMG_UINT32 uType, IMG_UINT32 uIndexType)
/*****************************************************************************
 FUNCTION	: IsSrc0ExtendedBanks

 PURPOSE	: Check if a register bank is in the set allowed for hardware source 0
			  with the extended bit.

 PARAMETERS	: uType			- Register bank to check.
			  uIndex

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (uIndexType != USC_REGTYPE_NOINDEX)
	{
		return IMG_FALSE;
	}
	switch (uType)
	{
		case USEASM_REGTYPE_TEMP:
		case USEASM_REGTYPE_PRIMATTR:
		case USEASM_REGTYPE_OUTPUT:
		case USEASM_REGTYPE_SECATTR:
		case USC_REGTYPE_CALCSECATTR:
		case USEASM_REGTYPE_FPINTERNAL:
		case USC_REGTYPE_GSINPUT:
		case USC_REGTYPE_REGARRAY:
		{
			return IMG_TRUE;
		}
		default:
		{
			return IMG_FALSE;
		}
	}
}

IMG_INTERNAL
IMG_BOOL IsSrc12StandardBanks(IMG_UINT32 uType, IMG_UINT32 uIndexType)
/*****************************************************************************
 FUNCTION	: IsSrc12StandardBanks

 PURPOSE	: Check if a register bank is in the set allowed for hardware source 1
			  or source 2 without the extended bit.

 PARAMETERS	: uType			- Register bank to check.
			  uIndex

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (uIndexType != USC_REGTYPE_NOINDEX)
	{
		return IMG_FALSE;
	}
	switch (uType)
	{
		case USEASM_REGTYPE_TEMP:
		case USEASM_REGTYPE_PRIMATTR:
		case USEASM_REGTYPE_OUTPUT:
		case USEASM_REGTYPE_SECATTR:
		case USEASM_REGTYPE_FPINTERNAL:
		case USC_REGTYPE_GSINPUT:
		case USC_REGTYPE_REGARRAY:
		case USC_REGTYPE_CALCSECATTR:
		{
			return IMG_TRUE;
		}
		default:
		{
			return IMG_FALSE;
		}
	}
}

static
IMG_BOOL IsSrc12ExtendedBanks(IMG_UINT32 uType)
/*****************************************************************************
 FUNCTION	: IsSrc12ExtendedBanks

 PURPOSE	: Check if a register bank is in the set allowed for hardware source 1
			  or source 2 with the extended bit.

 PARAMETERS	: uType			- Register bank to check.
			  uIndex

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (uType == USEASM_REGTYPE_INDEX)
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

#if defined(SUPPORT_SGX545)
IMG_INTERNAL
IMG_VOID GetSmpBackendSrcSet(PINTERMEDIATE_STATE	psState,
							 const INST			*psInst,
							 IMG_PUINT32			puSet)
/*****************************************************************************
 FUNCTION	: GetSmpBackendSrcSet

 PURPOSE	: Gets the set of sources to an SMP instruction which are
			  fetched using the seperate SMP hardware unit.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Texture sample instruction.
			  puSet				- Returns the bit vector of sources fetched
								using the backend.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uOverallDimensionality;
	IMG_UINT32	uArg;

	/*
		Get the overall number of values fetched from the coordinate source.
	*/
	uOverallDimensionality = psInst->u.psSmp->uDimensionality;
	if (psInst->u.psSmp->bUsesPCF)
	{
		uOverallDimensionality++;
	}

	/*
		Reset the set of arguments that are fetched by the external units.
	*/
	memset(puSet, 0, UINTS_TO_SPAN_BITS(SMP_MAXIMUM_ARG_COUNT) * sizeof(IMG_UINT32));

	/*
		If we are sampling a 3D texture then the PCF comparison value
		is fetched by the backend.
	*/
	if (psInst->u.psSmp->uDimensionality == 3 && psInst->u.psSmp->bUsesPCF)
	{
		IMG_UINT32	uPCFRegOff;
		PARG		psCoordArg = &psInst->asArg[SMP_COORD_ARG_START];

		/*
			Work out the offset of the argument containing the PCF
			comparison value.
		*/
		if (psCoordArg->eFmt == UF_REGFORMAT_F32)
		{
			uPCFRegOff = psInst->u.psSmp->uDimensionality;
		}
		else
		{
			ASSERT(psCoordArg->eFmt == UF_REGFORMAT_F16);	/* C10 isn't supported with PCF */
			uPCFRegOff = psInst->u.psSmp->uDimensionality >> 1;
		}

		SetBit(puSet, SMP_COORD_ARG_START + uPCFRegOff, 1);
	}
	/*
		All state arguments are fetched by the backend.
	*/
	for (uArg = SMP_STATE_ARG_START;
		 uArg < (SMP_STATE_ARG_START + psState->uTexStateSize);
		 uArg++)
	{
		SetBit(puSet, uArg, 1);
	}
	/*
		All gradient arguments are fetched by the backend.
	*/
	for (uArg = SMP_GRAD_ARG_START;
		 uArg < (SMP_GRAD_ARG_START + psInst->u.psSmp->uGradSize);
		 uArg++)
	{
		SetBit(puSet, uArg, 1);
	}
	/*
		If more than two channels (including PCF) are used from the
		coordinates then the LOD argument is fetched by the backend.
	*/
	if (uOverallDimensionality > 2)
	{
		SetBit(puSet, SMP_LOD_ARG_START, 1);
	}
}
#endif /* defined(SUPPORT_SGX545) */

static
IMG_BOOL CanUseSmpSrc(PINTERMEDIATE_STATE	psState,
					  const INST			*psInst,
					  IMG_UINT32			uArg,
					  IMG_UINT32			uType,
					  IMG_UINT32			uIndexType)
/*****************************************************************************
 FUNCTION	: CanUseSmpSrc

 PURPOSE	: Check if a texutre sample instruction can accept a register type in an argument.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction to check.
			  uArg		- The argument to check.
			  uType		- The register type.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);

	#if defined(SUPPORT_SGX545)
	/*
		Check which sources are going to be fetched using the DMA engine.
	*/
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_EXTERNAL_LDST_SMP_UNIT)
	{
		IMG_UINT32	uOverallDimensionality;

		/*
			Get the overall number of values fetched from the coordinate source.
		*/
		uOverallDimensionality = psInst->u.psSmp->uDimensionality;
		if (psInst->u.psSmp->bUsesPCF)
		{
			uOverallDimensionality++;
		}

		if (
				(
					(
						uArg >= SMP_COORD_ARG_START && 
						uArg < (SMP_COORD_ARG_START + psInst->u.psSmp->uCoordSize)
					) &&
					uOverallDimensionality > 3
				) ||
				(
					uArg >= SMP_STATE_ARG_START && 
					uArg < (SMP_STATE_ARG_START + psState->uTexStateSize)
				) ||
				(
					uArg >= SMP_GRAD_ARG_START && 
					uArg < (SMP_GRAD_ARG_START + psInst->u.psSmp->uGradSize)
				) ||
				(
					uArg == SMP_LOD_ARG_START &&
					(
						#if defined(OUTPUT_USPBIN)
						psInst->eOpcode == ISMPBIAS_USP ||
						psInst->eOpcode == ISMPREPLACE_USP ||
						#endif /* defined(OUTPUT_USPBIN) */
						psInst->eOpcode == ISMPBIAS ||
						psInst->eOpcode == ISMPREPLACE
					) &&
					uOverallDimensionality > 2
				)
			)
		{
			/*
				For sources which are going to be fetched using the DMA engine
				only unified store register banks are valid.
			*/
			if (
					(
						uType == USEASM_REGTYPE_TEMP ||
						uType == USEASM_REGTYPE_PRIMATTR ||
						uType == USEASM_REGTYPE_OUTPUT ||
						uType == USEASM_REGTYPE_SECATTR
					) &&
					uIndexType == USC_REGTYPE_NOINDEX
				)
			{
				return IMG_TRUE;
			}
			return IMG_FALSE;
		}
	}
	#endif /* defined(SUPPORT_SGX545) */
	
	/*
		Check available bank encodings.
	*/
	if (uArg >= SMP_COORD_ARG_START && uArg < (SMP_COORD_ARG_START + psInst->u.psSmp->uCoordSize))
	{
		/*
			For the coordinate sources only extended source 0 register banks are valid.
		*/
		if (IsSrc0ExtendedBanks(uType, uIndexType))
		{
			return IMG_TRUE;
		}
		return IMG_FALSE;
	}
	else
	{
		/*
			All register banks are valid for the STATE and LOD/GRADIENT/SAMPLE_IDX sources.
		*/
		return IMG_TRUE;
	}
}

IMG_INTERNAL 
IMG_BOOL CanUseSrc(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArg, IMG_UINT32 uType, IMG_UINT32 uIndexType)
/*****************************************************************************
 FUNCTION	: CanUseSrc

 PURPOSE	: Check if an instruction can accept a register type in an argument.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction to check.
			  uArg		- The argument to check.
			  uType		- The register type.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PFN_CAN_USE_SRC	pfCanUseSrc;

	ASSERT(psInst->eOpcode < (sizeof(g_psInstDesc) / sizeof(g_psInstDesc[0])));
	if (g_psInstDesc[psInst->eOpcode].sCanUseSrc.apfCanUseSrc == NULL)
	{
		return IMG_TRUE;
	}
	ASSERT(uArg < g_psInstDesc[psInst->eOpcode].sCanUseSrc.uFuncCount);
	pfCanUseSrc = g_psInstDesc[psInst->eOpcode].sCanUseSrc.apfCanUseSrc[uArg];
	return pfCanUseSrc(psState, psInst, uArg, uType, uIndexType);
}

static
IMG_BOOL IsRotatedImmediateInRange(IMG_UINT32	uImmediate)
/*****************************************************************************
 FUNCTION	: IsRotatedImmediateInRange

 PURPOSE    : Checks if there is a bit-rotation which can be applied to an
			  immediate so it fits in the range which can be encoded in
			  a bitwise instruction.

 PARAMETERS	: uImmediate	- Value to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uRot;

	for (uRot = 0; uRot < EURASIA_USE1_BITWISE_SRC2ROT_MAXIMUM; uRot++)
	{
		if (uImmediate <= EURASIA_USE_BITWISE_MAXIMUM_UNROTATED_IMMEDIATE)
		{
			return IMG_TRUE;
		}
		uImmediate = (uImmediate >> 1) | ((uImmediate & 1) << 31);
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_UINT32 ApplyIMAEComponentSelect(PINTERMEDIATE_STATE	psState,
									const INST			*psInst,
									IMG_UINT32			uArgIdx,
									IMG_UINT32			uComponent,
									IMG_UINT32			uImmediate)
/*****************************************************************************
 FUNCTION	: ApplyIMAEComponentSelect

 PURPOSE    : Simulate the LOW/HIGH selects on source arguments to the IMAE
			  instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.
			  uArgIdx			- Argument index.
			  uComponent		- Component select.
			  uImmediate		- Value to apply the selects to.

 RETURNS	: The new value.
*****************************************************************************/
{
	IMG_BOOL	bSignExtendImmediate;

	ASSERT(psInst->eOpcode == IIMAE);

	/*
		The behaviour of IMAE is
	
			SRC0 = CHOOSE_LOW_OR_HIGH_WORD(SRC0)
			if (SRC1 isn't immediate)
			{
				SRC1 = CHOOSE_LOW_OR_HIGH_WORD(SRC1)
			}
			if (SRC2TYPE == U16 || SRC2TYPE == I16)
			{
				SRC2 = CHOOSE_LOW_OR_HIGH_WORD(SRC2)
			}

			RESULT, CARRYOUT = SRC0 * SRC1 + SRC2 [+ CARRYIN]

		To make immediates and non-immediate interchangeable as
		sources most of the time we treat IMAE as through the
		low/high selects applied to immediates too. But in two
		cases (when we're checking whether immediates fit in the
		range supported by the hardware and when we are calling
		the assembler to encode an IMAE instruction) this function
		is called to get the value of an immediate source with
		the low/high selects applied. Then it's the return value
		from this function which is actually encoded into the
		instruction.
	*/
	if (!(uArgIdx == 2 && psInst->u.psImae->uSrc2Type == USEASM_INTSRCSEL_U32))
	{
		if (uComponent == 2)
		{
			uImmediate >>= 16;
		}
		else
		{	
			ASSERT(uComponent == 0);
			uImmediate &= 0xFFFF;
		}
	}

	/*
		Sign extend back to 32-bits.
	*/
	if (uArgIdx == 0 || uArgIdx == 1)
	{
		bSignExtendImmediate = psInst->u.psImae->bSigned;
	}
	else
	{
		ASSERT(uArgIdx == 2);
		if (psInst->u.psImae->uSrc2Type == USEASM_INTSRCSEL_Z16)
		{
			bSignExtendImmediate = IMG_FALSE;
		}
		else
		{
			ASSERT(psInst->u.psImae->uSrc2Type == USEASM_INTSRCSEL_S16 ||
				   psInst->u.psImae->uSrc2Type == USEASM_INTSRCSEL_U32);
			bSignExtendImmediate = IMG_TRUE;
		}
	}
	if (bSignExtendImmediate && (uImmediate & 0x8000) != 0)
	{
		uImmediate |= 0xFFFF0000;
	}
	
	return uImmediate;
}

IMG_INTERNAL
IMG_BOOL BypassCacheForModifiableShaderMemory(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: BypassCacheForModifiableShaderMemory

 PURPOSE    : TRUE if the cache must be bypassed when loading/storing to
			  regions of data used for modifiable storage during shader
			  execution.

 PARAMETERS	: psState		- Compiler state.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if ((psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_MEMORY_LD_ST_COHERENT) == 0)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_BOOL IsValidImmediateForLocalAddressing(PINTERMEDIATE_STATE	psState,
											IOPCODE				eOpcode,
											IMG_UINT32			uImmediate,
											IMG_PUINT32			puPackedImmediate)
/*****************************************************************************
 FUNCTION	: IsValidImmediateForLocalAddressing

 PURPOSE    : Checks if an immediate value is in the right range to be 
			  supported directly in the offset source to a memory load/store
			  instruction using local addressing.

 PARAMETERS	: psState		- Compiler state.
			  eOpcode		- Instruction to check for.
			  uArgIdx		- Index of the source to check.
			  uImmediate	- Value to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uOffset;
	IMG_UINT32	uStride;
	IMG_UINT32	uDataSize;

	/*
		Immediate offsets with local addressing are never valid on cores affected by these bugs.
	*/
	if ((psState->psTargetBugs->ui32Flags & (SGX_BUG_FLAGS_FIX_HW_BRN_24895 | SGX_BUG_FLAGS_FIX_HW_BRN_30795)) != 0)
	{
		return IMG_FALSE;
	}

	/*
		Get the size of the data loaded/stored from the instruction.
	*/
	switch (eOpcode)
	{
		case ILDLB:
		case ISTLB:
		{
			uDataSize = 1;
			break;
		}
		case ILDLW:
		case ISTLW:
		{
			uDataSize = 2;
			break;
		}
		case ILDLD:
		case ISTLD:
		case ISPILLREAD:
		case ISPILLWRITE:
		{
			uDataSize = 4;
			break;
		}
		default: imgabort();
	}

	/*
		Unpack the immediate value into the size of the per-instance data area and the offset
		into the per-instance data area.
	*/
	uOffset = (uImmediate & ~EURASIA_USE_LDST_NONIMM_LOCAL_OFFSET_CLRMSK) >> EURASIA_USE_LDST_NONIMM_LOCAL_OFFSET_SHIFT;
	uStride = (uImmediate & ~EURASIA_USE_LDST_NONIMM_LOCAL_STRIDE_CLRMSK) >> EURASIA_USE_LDST_NONIMM_LOCAL_STRIDE_SHIFT;

	/*	
		Check both the offset and stride are multiplies of the size of the data read/written
		by the instruction.
	*/
	if ((uOffset % uDataSize) != 0)
	{
		return IMG_FALSE;
	}
	uOffset /= uDataSize;
	if ((uStride % uDataSize) != 0)
	{
		return IMG_FALSE;
	}
	uStride /= uDataSize;

	/*
		Check if the stride is within the range supported for an immediate source.
	*/
	if ((uStride % EURASIA_USE_LDST_IMM_LOCAL_STRIDE_GRAN) != 0)
	{
		return IMG_FALSE;
	}
	if (uStride > EURASIA_USE_LDST_MAX_IMM_LOCAL_STRIDE)
	{
		return IMG_FALSE;
	}

	/*
		Check if the offset is within the range supported for an immediate source.
	*/
	if (uOffset > EURASIA_USE_LDST_MAX_IMM_LOCAL_OFFSET)
	{
		return IMG_FALSE;
	}

	/*
		Create the immediate value as packed into the register number field in the encoded
		instruction.
	*/
	if (puPackedImmediate != NULL)
	{
		uStride /= EURASIA_USE_LDST_IMM_LOCAL_STRIDE_GRAN;
		*puPackedImmediate = 
			(uStride << EURASIA_USE_LDST_IMM_LOCAL_STRIDE_SHIFT) | (uOffset << EURASIA_USE_LDST_IMM_LOCAL_OFFSET_SHIFT);
	}

	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL IsValidImmediateForAbsoluteAddressing(PINTERMEDIATE_STATE	psState, 
											   PCINST				psInst,
											   IMG_UINT32			uImmediate, 
											   IMG_PUINT32			puPackedImmediate)
/*****************************************************************************
 FUNCTION	: IsValidImmediateForAbsoluteAddressing

 PURPOSE    : Checks if an immediate value is in the right range to be 
			  supported directly in the offset source to a memory load/store
			  instruction using absolute addressing.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check for.
			  uArgIdx		- Index of the source to check.
			  uImmediate	- Value to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uDataSize;
	IMG_UINT32	uLowWord;
	IMG_UINT32	uHighWord;
	IMG_UINT32	uByteOffset;
	IMG_UINT32	uPackedOffset;

	/*
		Get the size of the data loaded/stored by the instruction. The offset encoded in the
		instruction is multiplied by the data size.
	*/
	switch (psInst->eOpcode)
	{
		case ILDAB:
		case ISTAB:
		{
			uDataSize = BYTE_SIZE;
			break;
		}
		case ILDAW:
		case ISTAW:
		{
			uDataSize = WORD_SIZE;
			break;
		}
		case ILDAD:
		case ISTAD:
		{
			uDataSize = LONG_SIZE;
			break;
		}
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case ILDAQ:
		{
			/*
				On cores affected by this bug the memory address is incremented by 1 on
				each fetch iteration.
			*/
			if (psInst->uRepeat > 1)
			{
				return IMG_FALSE;
			}
			/*
				The encoded immediate offset isn't scaled by the data size.
			*/
			uDataSize = BYTE_SIZE;
			break;
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		default: imgabort();
	}

	/*
		Calculate the byte offset accessed.
	*/
	uLowWord = (uImmediate >> 0) & 0xFFFF;
	uHighWord = (uImmediate >> 16) & 0xFFFF;
	uByteOffset = uLowWord * uHighWord;

	/*
		Check the byte offset is a multiple of the data size.
	*/
	if ((uByteOffset % uDataSize) != 0)
	{
		return IMG_FALSE;
	}

	/*
		Check the memory offset can be encoded into the instruction.
	*/
	uPackedOffset = uByteOffset / uDataSize;
	if (uPackedOffset > EURASIA_USE_MAXIMUM_IMMEDIATE)
	{
		return IMG_FALSE;
	}

	if (puPackedImmediate != NULL)
	{
		*puPackedImmediate = uPackedOffset;
	}
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL IsImmediateSourceValid(PINTERMEDIATE_STATE	psState,
								PCINST				psInst,
								IMG_UINT32			uArgIdx,
								IMG_UINT32			uComponentSelect,
								IMG_UINT32			uImmediate)
/*****************************************************************************
 FUNCTION	: IsImmediateSourceValid

 PURPOSE    : Checks if an immediate value is in the right range to be 
			  supported directly in an instruction source.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check for.
			  uArgIdx		- Index of the source to check.
			  uComponentSelect
							- Component select on the instruction source.
			  uImmediate	- Value to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_BOOL	bSigned;

	PVR_UNREFERENCED_PARAMETER(psState);

	if (uArgIdx == MEMLOADSTORE_OFFSET_ARGINDEX)
	{
		if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_LOCALLOADSTORE) != 0)
		{
			return IsValidImmediateForLocalAddressing(psState, psInst->eOpcode, uImmediate, NULL /* puPackedImmediate */);
		}
		if ((g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_ABSOLUTELOADSTORE) != 0)
		{
			return IsValidImmediateForAbsoluteAddressing(psState, psInst, uImmediate, NULL /* puPackedImmediate */);
		}
	}

	/*
		A delta instruction will be expanded to a MOV or LIMM instruction which can support any immediate
		source.
	*/
	if (psInst->eOpcode == IDELTA)
	{
		return IMG_TRUE;
	}

	/*
		Check whether this instruction interprets immediate arguments
		as signed.
	*/
	bSigned = IMG_FALSE;
	if (psInst->eOpcode == IIMAE)
	{
		if (uArgIdx == 1 && psInst->u.psImae->bSigned)
		{
			bSigned = IMG_TRUE;
		}
		if (uArgIdx == 2 && psInst->u.psImae->uSrc2Type != USEASM_INTSRCSEL_Z16)
		{
			bSigned = IMG_TRUE;
		}
	}
	#if defined(SUPPORT_SGX545)
	if (psInst->eOpcode == IIMA32 && psInst->u.psIma32->bSigned)
	{
		bSigned = IMG_TRUE;
	}
	#endif /* defined(SUPPORT_SGX545) */

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psInst->eOpcode == IVMOVC_I32)
	{
		/*
			Check if the immediate is in the range which can be directly encoded
			in an instruction source.
		*/
		if (uImmediate <= EURASIA_USE_MAXIMUM_IMMEDIATE)
		{
			return IMG_TRUE;
		}
		else
		{
			return IMG_FALSE;
		}
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	#if defined(SUPPORT_SGX545)
	if ((psInst->eOpcode == IELDD || psInst->eOpcode == IELDQ) && uArgIdx == ELD_STATIC_OFFSET_ARGINDEX)
	{
		if (uImmediate <= SGX545_USE1_EXTLD_OFFSET_MAX)
		{
			return IMG_TRUE;
		}
		else
		{
			return IMG_FALSE;
		}
	}
	#endif /* defined(SUPPORT_SGX545) */

	/*
		For IMAE check the immediate value after the HIGH/LOW select is applied.
	*/
	if (psInst->eOpcode == IIMAE)
	{
		uImmediate = ApplyIMAEComponentSelect(psState, psInst, uArgIdx, uComponentSelect, uImmediate);
	}

	if (bSigned)
	{
		IMG_INT32	iImmediate;

		/*
			Check if the immediate (interpreted as signed) is in the range which can
			be directly encoded in an instruction source.
		*/
		iImmediate = (IMG_INT32)uImmediate;
		if (iImmediate >= EURASIA_USE_MINIMUM_SIGNED_IMMEDIATE && iImmediate <= EURASIA_USE_MAXIMUM_SIGNED_IMMEDIATE)
		{
			return IMG_TRUE;
		}
		return IMG_FALSE;
	}
	else
	{
		/*
			For memory loads if the immediate is out of range then we'll use a
			reserved temporary register to generate a LIMM in a later phase so
			report all immediate values as supported here.
		*/
		if (psInst->eOpcode == ILOADMEMCONST)
		{
			if (uArgIdx == LOADMEMCONST_STATIC_OFFSET_ARGINDEX || uArgIdx == LOADMEMCONST_STRIDE_ARGINDEX)
			{
				return IMG_TRUE;
			}
			if (uArgIdx == LOADMEMCONST_MAX_RANGE_ARGINDEX)
			{
				return IMG_TRUE;
			}
		}

		/*
			Check if the immediate is in the range which can be directly encoded
			in an instruction source.
		*/
		if (uImmediate <= EURASIA_USE_MAXIMUM_IMMEDIATE)
		{
			return IMG_TRUE;
		}
		/*
			A LIMM instruction loads a full 32-bit immediate value.
		*/
		if (psInst->eOpcode == ILIMM)
		{
			return IMG_TRUE;
		}
		/*
			Check if the immediate value fits in the extended range supported on source 2
			by the bitwise instructions (a 16-bit value, rotated upto 31 places with
			an optional invert).
		*/
		if (
				(
					psInst->eOpcode == IAND || 
					psInst->eOpcode == IXOR ||
					psInst->eOpcode == IOR
				) &&
				uArgIdx == 1
		   )
		{
			if (IsRotatedImmediateInRange(uImmediate))
			{
				return IMG_TRUE;
			}
			if (IsRotatedImmediateInRange(~uImmediate))
			{
				return IMG_TRUE;
			}
		}
		return IMG_FALSE;
	}
}

IMG_INTERNAL
IMG_VOID LoadImmediate(PINTERMEDIATE_STATE	psState,
					   PINST				psInst,
					   IMG_UINT32			uArgIdx,
					   IMG_UINT32			uImmValue)
/*****************************************************************************
 FUNCTION	: LoadImmediate
    
 PURPOSE	: Set up a source to an instruction to an immediate value.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to set up.
			  uArgIdx			- Index of the argument to set up.
			  uImmValue			- Immediate value to set up.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG	psArg = &psInst->asArg[uArgIdx];

	/*
		Check if the immediate value can be encoded in the instruction
		directly.
	*/
	if (IsImmediateSourceValid(psState, psInst, uArgIdx, GetComponentSelect(psState, psInst, uArgIdx), uImmValue))
	{
		psArg->uType = USEASM_REGTYPE_IMMEDIATE;
		psArg->uNumber = uImmValue;
	}
	else
	{
		/*
			Try adding a secondary attribute containing the immediate value.
		*/
		if (AddStaticSecAttr(psState, uImmValue, NULL /* puArgType */, NULL /* puArgNum */))
		{
			IMG_UINT32	uArgType, uArgNumber;

			/*
				Use the secondary attribute as the instruction source.
			*/
			AddStaticSecAttr(psState, uImmValue, &uArgType, &uArgNumber);
			SetSrc(psState, psInst, uArgIdx, uArgType, uArgNumber, psArg->eFmt);
		}
		else
		{
			PINST		psLimmInst;
			IMG_UINT32	uImmTemp = GetNextRegister(psState);

			/*
				Add a LIMM instruction to load the immediate value into a temporary register.
			*/
			psLimmInst = AllocateInst(psState, psInst);
			SetOpcode(psState, psLimmInst, ILIMM);
			SetDestTempArg(psState, psLimmInst, 0 /* uDestIdx */, uImmTemp, UF_REGFORMAT_F32);
			psLimmInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
			psLimmInst->asArg[0].uNumber = uImmValue;
			InsertInstBefore(psState, psInst->psBlock, psLimmInst, psInst);

			/*
				Use the temporary register as the instruction source.
			*/
			SetSourceTempArg(psState, psInst, uArgIdx, uImmTemp, UF_REGFORMAT_F32);
		}
	}
}

IMG_INTERNAL
IMG_UINT32 GetChannelsWrittenInRegister(const INST		*psInst,
									    IMG_UINT32		uRegisterType,
										IMG_UINT32		uRegisterNumber,
										IMG_PUINT32*	ppuLiveChansInDest,
										IMG_PUINT32		puDestOffset)
/*****************************************************************************
 FUNCTION	: GetChannelsWrittenInRegister

 PURPOSE	: Get the mask of channels written in a register by an instruction.

 PARAMETERS	: psInst				- Instruction to check.
			  uRegisterType			- The register to check.
			  uRegisterNumber	
			  ppuLiveChansInDest	- If non-NULL returns a pointer to the
									live channels mask for the destination
									that writes the register.
			  puDestOffset			- Returns the offset in the destination
									array of the destination that writes the
									register.

 RETURNS	: The mask of channels written.
*****************************************************************************/
{
	IMG_UINT32	uDestIdx;

	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		if (psInst->asDest[uDestIdx].uType == uRegisterType &&
			psInst->asDest[uDestIdx].uNumber == uRegisterNumber)
		{
			IMG_UINT32	uDestMask;

			uDestMask		 = psInst->auDestMask[uDestIdx];
			if (ppuLiveChansInDest != NULL)
			{
				*ppuLiveChansInDest		= &psInst->auLiveChansInDest[0];
			}

			if (puDestOffset != NULL)
			{
				*puDestOffset = uDestIdx;
			}

			return uDestMask;
		}
	}
	return 0;
}

IMG_INTERNAL
IMG_UINT32 GetChannelsWrittenInArg(const INST		*psInst,
								   PARG				psArg,
								   IMG_PUINT32*		ppuLiveChansInDest)
/*****************************************************************************
 FUNCTION	: GetChannelsWrittenInRegister

 PURPOSE	: Get the mask of channels written in an argument by an instruction.

 PARAMETERS	: psInst				- Instruction to check.
			  psArg					- The argument to check.
			  ppuLiveChansInDest	- If non-NULL returns a pointer to the
									live channels mask for the destination
									that writes the register.

 RETURNS	: The mask of channels written.
*****************************************************************************/
{
	return GetChannelsWrittenInRegister(psInst, psArg->uType, psArg->uNumber, ppuLiveChansInDest, NULL);
}

static
IMG_VOID ClearInst(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: ClearInst

 PURPOSE	: Free opcode specific data structures associated with an
			  instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psInst->eOpcode != INVALID_MAXARG)
	{
		/* Remove this instruction from the list of instructions with the same opcode. */
		SafeListRemoveItem(&psState->asOpcodeLists[psInst->eOpcode], &psInst->sOpcodeListEntry);
	}

	gauInstructionOperationsJumpTable[g_psInstDesc[psInst->eOpcode].eType].pfnClearInstruction(psState, psInst);
}

static
IMG_VOID SetPredCount(PINTERMEDIATE_STATE	psState,
					  PINST					psInst,
					  IMG_UINT32			uNewPredCount)
/*****************************************************************************
 FUNCTION	: SetPredCount

 PURPOSE	: Set the number of predicates to an intermediate instruction. If
			  the number of predicates increases then the new destinations are
			  set to NULL.

 PARAMETERS	: psState		- Compiler state
			  psInst		- Instruction to set the destination count for.
			  uNewPredCount - New count of predicates.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psInst->uPredCount != uNewPredCount)
	{
		/*
			If the number of predicates is reducing then free storage for the previous
			values of conditionally written destinations.
		*/
		if (psInst->uPredCount > uNewPredCount)
		{
			IMG_UINT32 uPred;

			for (uPred = uNewPredCount; uPred < psInst->uPredCount; uPred++)
			{
				PARG psPredSrc = psInst->apsPredSrc[uPred];
				
				if (psPredSrc != NULL)
				{
					UseDefDropUse(psState, psInst->apsPredSrcUseDef[uPred]);

					UscFree(psState, psInst->apsPredSrcUseDef[uPred]);
					psInst->apsPredSrcUseDef[uPred] = NULL;

					UscFree(psState, psPredSrc);
					psInst->apsPredSrc[uPred] = NULL;
				}
				else
				{
					ASSERT(psInst->apsPredSrcUseDef[uPred] == NULL);
				}
			}
		}

		/*	
			Resize the arrays representing the destinations.
		*/
		ResizeTypedArray(psState, psInst->apsPredSrc, psInst->uPredCount, uNewPredCount);
		ResizeTypedArray(psState, psInst->apsPredSrcUseDef, psInst->uPredCount, uNewPredCount);

		/*
			If the number of predicates has increased then set the new predicates
			to default values.
		*/
		if (uNewPredCount > psInst->uPredCount)
		{
			IMG_UINT32	uPred;

			for (uPred = psInst->uPredCount; uPred < uNewPredCount; uPred++)
			{
				psInst->apsPredSrc[uPred] = NULL;
				psInst->apsPredSrcUseDef[uPred] = NULL;
			}
		}

		/*
			Update the destination count in the instruction structure.
		*/
		psInst->uPredCount = uNewPredCount;
	}	
}

IMG_INTERNAL 
IMG_VOID FreeInst(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: FreeInst

 PURPOSE	: Free an intermediate instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- The instruction to free.

 RETURNS	: None.
*****************************************************************************/
{
	/* The instruction shouldn't be inserted into a flow control block. */
	ASSERT(psInst->psBlock == NULL);

	/*
		Delete any references to the instruction from lists of instructions.
	*/
	WeakInstListFreeInst(psState, psInst);

	/* Free per-instruction data. */
	ClearInst(psState, psInst);

	/* Free per-stage data. */
	UscFree(psState, psInst->psRepeatGroup);

	/* Free the argument array. */
	SetArgumentCount(psState, psInst, 0 /* uArgCount */);

	/* Free any source predicate. */
	SetPredCount(psState, psInst, 0);

	/* Free details of reserved temporaries. */
	if (psInst->asTemp != NULL)
	{
		UscFree(psState, psInst->asTemp);
	}

	/* Free the destination array. */
	SetDestCount(psState, psInst, 0 /* uNewDestCount */);

	#ifdef SRC_DEBUG
	if((psState->uFlags & USC_FLAGS_REMOVING_INTERMEDIATE_CODE) == 0)
	{
		DecrementCostCounter(psState, (psInst->uAssociatedSrcLine), 1);
	}
	#endif /* SRC_DEBUG */

	/* Free the main structure */
	UscFree(psState, psInst);
}

IMG_INTERNAL
void InitInstArg(PARG psArg)
/*****************************************************************************
 FUNCTION	: InitInstArg

 PURPOSE	: Initialise an instruction source operand

 PARAMETERS	: psArg		- Operand to initialise

 RETURNS	: Nothing
*****************************************************************************/
{
	psArg->uType = USC_REGTYPE_DUMMY;
	psArg->uNumber = 0;
	psArg->psRegister = NULL;
	psArg->uIndexType = USC_REGTYPE_NOINDEX;
	psArg->uIndexNumber = USC_UNDEF;
	psArg->psIndexRegister = NULL;
	psArg->uIndexArrayOffset = USC_UNDEF;
	psArg->uIndexStrideInBytes = USC_UNDEF;
	psArg->uArrayOffset = 0;
	psArg->eFmt = UF_REGFORMAT_F32;
	psArg->uNumberPreMoe = 0;
}

IMG_INTERNAL
IMG_VOID MakeArgumentSet(PARG asSet, IMG_UINT32 uSetCount, IMG_UINT32 uRegType, IMG_UINT32 uRegBaseNum, UF_REGFORMAT eRegFmt)
/*****************************************************************************
 FUNCTION	: MakeArgumentSet

 PURPOSE	: Initialise a set of instruction source or destination operands
			  with the same type and format, and consecutive register numbers.

 PARAMETERS	: asSet			- Start of the set to initialize.
			  uSetCount		- Count of arguments in the set.
			  uRegType		- Register type.
			  uRegBaseNum	- Register number for the first argument in the est.
			  eRegFmt		- Register format.

 RETURNS	: Nothing
*****************************************************************************/
{
	IMG_UINT32	uArg;

	for (uArg = 0; uArg < uSetCount; uArg++)
	{
		PARG	psArg = &asSet[uArg];

		InitInstArg(psArg);
		psArg->uType = uRegType;
		psArg->uNumber = uRegBaseNum + uArg;
		psArg->eFmt = eRegFmt;
	}
}

static
IMG_VOID InitInst(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InitInst
    
 PURPOSE	: Allocate and initialise any instruction data structures needed for 
			  an instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	DBG_ASSERT(psInst != NULL);

	/* Add to the list of instructions with the same opcode. */
	SafeListAppendItem(&psState->asOpcodeLists[psInst->eOpcode], &psInst->sOpcodeListEntry);
	
	gauInstructionOperationsJumpTable[g_psInstDesc[psInst->eOpcode].eType].pfnInitInstruction(psState, psInst);
}

IMG_INTERNAL
IMG_VOID SetDestCount(PINTERMEDIATE_STATE	psState,
					  PINST					psInst,
					  IMG_UINT32			uNewDestCount)
/*****************************************************************************
 FUNCTION	: SetDestCount

 PURPOSE	: Set the number of destinations to an intermediate instruction. If
			  the number of destinations increases then the new destinations are
			  set to default values.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to set the destination count for.
			  uNewDestCount
						- New count of destinations.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psInst->uDestCount != uNewDestCount)
	{
		/*
			If the number of destinations is reducing then free storage for the previous
			values of conditionally written destinations.
		*/
		if (psInst->uDestCount > uNewDestCount)
		{
			IMG_UINT32	uDestIdx;

			for (uDestIdx = uNewDestCount; uDestIdx < psInst->uDestCount; uDestIdx++)
			{
				PARG	psOldDest = psInst->apsOldDest[uDestIdx];
				
				if (psOldDest != NULL)
				{
					UseDefDropArgUses(psState,
									  psInst->apsOldDestUseDef[uDestIdx]);
				}
				UseDefDropDest(psState, psInst, uDestIdx);

				if (psOldDest != NULL)
				{
					UscFree(psState, psInst->apsOldDestUseDef[uDestIdx]);
					psInst->apsOldDestUseDef[uDestIdx] = NULL;

					UscFree(psState, psOldDest);
					psInst->apsOldDest[uDestIdx] = NULL;
				}
				else
				{
					ASSERT(psInst->apsOldDestUseDef[uDestIdx] == NULL);
				}
			}
		}

		/*	
			Resize the arrays representing the destinations.
		*/
		ResizeTypedArray(psState, psInst->asDest, psInst->uDestCount, uNewDestCount);
		psInst->asDestUseDef = UseDefResizeArgArray(psState, psInst->asDestUseDef, psInst->uDestCount, uNewDestCount);
		ResizeTypedArray(psState, psInst->apsOldDest, psInst->uDestCount, uNewDestCount);
		ResizeTypedArray(psState, psInst->apsOldDestUseDef, psInst->uDestCount, uNewDestCount);
		ResizeTypedArray(psState, psInst->auDestMask, psInst->uDestCount, uNewDestCount);
		ResizeTypedArray(psState, psInst->auLiveChansInDest, psInst->uDestCount, uNewDestCount);

		/*
			If the number of destinations has increased then set the new destinations
			to default values.
		*/
		if (uNewDestCount > psInst->uDestCount)
		{
			IMG_UINT32	uDestIdx;

			for (uDestIdx = psInst->uDestCount; uDestIdx < uNewDestCount; uDestIdx++)
			{
				InitInstArg(&psInst->asDest[uDestIdx]);
				psInst->apsOldDest[uDestIdx] = NULL;
				psInst->auDestMask[uDestIdx] = USC_DESTMASK_FULL;
				psInst->auLiveChansInDest[uDestIdx] = USC_DESTMASK_FULL;

				UseDefResetArgument(&psInst->asDestUseDef[uDestIdx], DEF_TYPE_INST, USE_TYPE_DESTIDX, uDestIdx, psInst); 
				psInst->apsOldDestUseDef[uDestIdx] = NULL;
			}
		}

		/*
			Update the destination count in the instruction structure.
		*/
		psInst->uDestCount = uNewDestCount;
	}	
}

IMG_INTERNAL
IMG_VOID SetArgumentCount(PINTERMEDIATE_STATE	psState,
						  PINST					psInst,
						  IMG_UINT32			uArgCount)
/*****************************************************************************
 FUNCTION	: SetArgumentCount

 PURPOSE	: Set the number of source arguments to an intermediate instruction to the
			  correct value when changing opcode. If the number of source arguments 
			  increases then the new arguments are set to default values.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to set the source argument count for.
			  eOldOpcode
						- Previous instruction opcode.
			  eOpcode	- Opcode to set the new argument count from.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uOldArgCount = psInst->uArgumentCount;

	if (uOldArgCount != uArgCount)
	{
		IMG_UINT32 uFirst = uOldArgCount;
		IMG_UINT32 i;
		/* 	
		   Changing opcode so make sure that the array of source operands is 
		   suitable for the new opcode.
		*/

		if (uArgCount < uOldArgCount)
		{
			IMG_UINT32	uArg;

			for (uArg = uArgCount; uArg < uOldArgCount; uArg++)
			{
				UseDefDropArgUses(psState, &psInst->asArgUseDef[uArg]);
			}
		}

		/* Allocate or resize the array of operands */
		ResizeTypedArray(psState, psInst->asArg, uOldArgCount, uArgCount);
		psInst->asArgUseDef = UseDefResizeArgArray(psState, psInst->asArgUseDef, uOldArgCount, uArgCount);

		/* Initialize the new arguments if necessary */
		for (i = uFirst; i < uArgCount; i ++)
		{
			InitInstArg(&psInst->asArg[i]);
			UseDefResetArgument(&psInst->asArgUseDef[i], USE_TYPE_SRC, USE_TYPE_SRCIDX, i, psInst);
		}

		/* Update the instruction argument count. */
		psInst->uArgumentCount = uArgCount;
	}
}

IMG_INTERNAL
IMG_VOID SetOpcodeAndDestCount(PINTERMEDIATE_STATE psState, PINST psInst, IOPCODE eOpcode, IMG_UINT32 uDestCount)
/*****************************************************************************
 FUNCTION	: SetOpcode

 PURPOSE	: Set the instruction opcode, allocating and initialising any 
			  instruction data structures needed the instruction type.

 PARAMETERS	: psState		- Compiler state
			  psInst		- Instruction to initialise 
			  eOpcode		- Opcode to set
			  uDestCount	- Count of destinations to the instruction.

 RETURNS	: Nothing.

 NOTES		: May resize the array of source operands in the instruction.
			  Source operands present in the instruction after the opcode is
			  changed have the same content as before the opcode was changed.
*****************************************************************************/
{
	/*
		Shrink or grow the array of arguments.
	*/
	SetArgumentCount(psState, psInst, g_psInstDesc[eOpcode].uDefaultArgumentCount);

	/* Free instruction-specific data. */
	if (psInst->eOpcode != IINVALID)
	{
		ClearInst(psState, psInst);
	}

	/* Set the opcode */
	psInst->eOpcode = eOpcode;

	/* Allocate space for the instruction's destinations. */
	SetDestCount(psState, psInst, uDestCount);

	/* Instruction-specific initialisation */
	if (eOpcode != INVALID_MAXARG)
	{
		InitInst(psState, psInst);
	}
}

IMG_INTERNAL
IMG_VOID SetOpcode(PINTERMEDIATE_STATE psState, PINST psInst, IOPCODE eOpcode)
/*****************************************************************************
 FUNCTION	: SetOpcode

 PURPOSE	: Set the instruction opcode, allocating and initialising any 
			  instruction data structures needed the instruction type.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to initialise 
			  eOpcode	- Opcode to set

 RETURNS	: Nothing.

 NOTES		: May resize the array of source operands in the instruction.
			  Source operands present in the instruction after the opcode is
			  changed have the same content as before the opcode was changed.
*****************************************************************************/
{
	IMG_UINT32	uDefaultDestCount;

	switch (eOpcode)
	{
		case IIMAE: 
		{
			/* Main and carry-out destinations. */
			uDefaultDestCount = 2; 
			break;
		}
		case IEFO: 
		{			
			uDefaultDestCount = EFO_DEST_COUNT;
			break;
		}
		case IRELEASE:
		case ILOCK:
		{
			uDefaultDestCount = 0;
			break;
		}
#if defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case IIMA32:
		{
			if (psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_IMA32_32X16_PLUS_32)
			{
				uDefaultDestCount = 1;
			}
			else
			{
				/*
					Two destinations for the first dword and second dword
					of the result.
				*/
				uDefaultDestCount = 2;
			}
			break;
		}
#endif
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case IVTESTBYTEMASK:
		{
			uDefaultDestCount = 2;
			break;
		}
#endif
		default:
		{
			uDefaultDestCount = 1;
			break;
		}
	}

	SetOpcodeAndDestCount(psState, psInst, eOpcode, uDefaultDestCount);
}

IMG_INTERNAL 
PINST AllocateInst(PINTERMEDIATE_STATE psState, PINST psSrcLineInst)
/*****************************************************************************
 FUNCTION	: AllocateInst

 PURPOSE	: Allocate an intermediate instruction.

 PARAMETERS	: psState	    - Compiler state.
              psSrcLineInst - Source instruction to fetch source line 
			                  number from.

 RETURNS	: The new instruction.
*****************************************************************************/
{
	PINST psInstruction;

	#if! defined(SRC_DEBUG)
	/* will be only used if SRC_DEBUG is on */ 
	PVR_UNREFERENCED_PARAMETER(psSrcLineInst);
	#endif /* SRC_DEBUG */

	psInstruction = UscAlloc(psState, sizeof(INST));

	psInstruction->eOpcode = IINVALID;

	psInstruction->uGlobalId = psState->uGlobalInstructionCount++;

	psInstruction->uArgumentCount = 0;
	psInstruction->asArg = NULL;
	psInstruction->asArgUseDef = NULL;

	psInstruction->uTempCount = 0;
	psInstruction->asTemp = NULL;

	psInstruction->uMask = 1;
	psInstruction->uRepeat = 0;

	psInstruction->apsPredSrc = NULL;
	psInstruction->apsPredSrcUseDef = NULL;
	psInstruction->uPredCount = 0;

	psInstruction->uDestCount = 0;
	psInstruction->asDest = NULL;
	psInstruction->asDestUseDef = NULL;
	psInstruction->apsOldDest = NULL;
	psInstruction->apsOldDestUseDef = NULL;
	psInstruction->auDestMask = NULL;
	psInstruction->auLiveChansInDest = NULL;

	psInstruction->uId = UINT_MAX;
	psInstruction->psGroupNext = NULL;
	psInstruction->uGroupChildIndex = 0;
	psInstruction->psGroupParent = psInstruction;
	psInstruction->psRepeatGroup = NULL;
	psInstruction->sStageData.psEfoData = NULL;
	memset(psInstruction->puSrcInc, 0, sizeof(psInstruction->puSrcInc));

	/* Instructions flags */
	memset(psInstruction->auFlag, 0, sizeof(psInstruction->auFlag));
	SetBit(psInstruction->auFlag, INST_MOE, 1);
	SetBit(psInstruction->auFlag, INST_SINGLE, 1);

	/* Sub-structures */
	psInstruction->u.pvNULL = NULL;

	psInstruction->psBlock = NULL;
	psInstruction->uBlockIndex = USC_UNDEF;
	
	ClearListEntry(&psInstruction->sAvailableListEntry);
	ClearListEntry(&psInstruction->sTempListEntry);
	
	InitializeList(&psInstruction->sWeakRefList);

	#if defined(OUTPUT_USPBIN)
	if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
	{
		psInstruction->uShaderResultHWOperands = 0;
	}
	#endif /* #if defined(OUTPUT_USPBIN) */

	#ifdef SRC_DEBUG
	if((psState->uFlags & USC_FLAGS_INTERMEDIATE_CODE_GENERATED))
	{
		if(psSrcLineInst != IMG_NULL)
		{
			IncrementCostCounter(psState, psSrcLineInst->uAssociatedSrcLine, 1);
			psInstruction->uAssociatedSrcLine = psSrcLineInst->uAssociatedSrcLine;
		}
		else
		{
			IncrementCostCounter(psState, psState->uTotalLines, 1);
			psInstruction->uAssociatedSrcLine = psState->uTotalLines;
		}
	}
	else
	{
		ASSERT((psState->uCurSrcLine)!=UNDEFINED_SOURCE_LINE);
		if( ((psState->uCurSrcLine)==UNDEFINED_SOURCE_LINE) || 
			((psState->uCurSrcLine)>(psState->uTotalLines)) ) 
		{
			psState->uCurSrcLine = psState->uTotalLines;
		}
		IncrementCostCounter(psState, (psState->uCurSrcLine), 1);
		psInstruction->uAssociatedSrcLine = psState->uCurSrcLine;
	}
	#endif /* SRC_DEBUG */

#if defined(TRACK_REDUNDANT_PCONVERSION)
	psInstruction->uGraphFlags					= 0;
	psInstruction->psImmediateSuperordinates	= IMG_NULL;
	psInstruction->psImmediateSubordinates		= IMG_NULL;
	
	/* Cached results for pattern search */
	psInstruction->psLastFailedRule			= IMG_NULL;
	psInstruction->psLastPassedRule			= IMG_NULL;
#endif
#if defined(DEBUG_ACCESS)
	psInstruction->uAccess = 0;
#endif
	return psInstruction;
}

/*****************************************************************************
 FUNCTION	: CopyInst
    
 PURPOSE	: Duplicate an instruction

 PARAMETERS	: psState   - Compiler state
              psSrcInst	- An existing intermediate instruction to copy

 RETURNS	: The duplicate instruction
*****************************************************************************/
PINST IMG_INTERNAL CopyInst(PINTERMEDIATE_STATE psState, PINST psSrcInst)
{
	PINST		psDestInst;
	IMG_UINT32	uIdx;

	/*
		Create a blank instruction (no sub-structures or source-arrays allocated)
	*/
	psDestInst = AllocateInst(psState, psSrcInst);

	/*
		Initialise the duplicate instruction, allocating appropriate sub-structures
		and argument lists
	*/
	SetOpcodeAndDestCount(psState, psDestInst, psSrcInst->eOpcode, psSrcInst->uDestCount);

	/*
		Copy source and dest arguments
	*/
	if (psSrcInst->uArgumentCount != psDestInst->uArgumentCount)
	{
		SetArgumentCount(psState, psDestInst, psSrcInst->uArgumentCount);
	}
	for (uIdx = 0; uIdx < psSrcInst->uArgumentCount; uIdx++)
	{
		SetSrcFromArg(psState, psDestInst, uIdx, &psSrcInst->asArg[uIdx]);
	}

	SetDestCount(psState, psDestInst, psSrcInst->uDestCount);
	for (uIdx = 0; uIdx < psSrcInst->uDestCount; uIdx++)
	{
		if ((psState->uFlags2 & USC_FLAGS2_SSA_FORM) == 0)
		{
			SetDestFromArg(psState, psDestInst, uIdx, &psSrcInst->asDest[uIdx]);
		}
		SetPartiallyWrittenDest(psState, psDestInst, uIdx, psSrcInst->apsOldDest[uIdx]);
		psDestInst->auDestMask[uIdx] = psSrcInst->auDestMask[uIdx];
		psDestInst->auLiveChansInDest[uIdx] = psSrcInst->auLiveChansInDest[uIdx];
	}

	/*
		Copy any reserved temporaries.
	*/
	if (psSrcInst->asTemp != NULL)
	{
		psDestInst->uTempCount = psSrcInst->uTempCount;
		psDestInst->asTemp = UscAlloc(psState, sizeof(psDestInst->asTemp[0]) * psDestInst->uDestCount);
		for (uIdx = 0; uIdx < psDestInst->uTempCount; uIdx++)
		{
			psDestInst->asTemp[uIdx] = psSrcInst->asTemp[uIdx];
		}
	}
	else
	{
		psDestInst->uTempCount = 0;
		psDestInst->asTemp = NULL;
	}
	
	/*
		Copy instruction fields common to all instructions
	*/
	psDestInst->uMask		= psSrcInst->uMask;
	psDestInst->uRepeat		= psSrcInst->uRepeat;

	for (uIdx = 0; uIdx < (sizeof(psSrcInst->puSrcInc) / sizeof(psSrcInst->puSrcInc[0])); uIdx++)
	{
		psDestInst->puSrcInc[uIdx] = psSrcInst->puSrcInc[uIdx];
	}

	for(uIdx = 0; uIdx<(UINTS_TO_SPAN_BITS(INST_FLAGS_NUM)); uIdx++)
	{
		psDestInst->auFlag[uIdx] = psSrcInst->auFlag[uIdx];
	}

	/*
		Copy source predicate.
	*/
	CopyPredicate(psState, psDestInst, psSrcInst);

	#if defined(INITIALISE_REGS_ON_FIRST_WRITE)
	if (psState->uFlags & USC_FLAGS_INITIALISEREGSONFIRSTWRITE)
	{
		psDestInst->uInitialisedChansInDest = psSrcInst->uInitialisedChansInDest;
	}
	#endif /* defined(INITIALISE_REGS_ON_FIRST_WRITE) */

	/*
		Clone the appropriate sub-structures (specific to the opcode)

		NB: This will have been allocated by SetOpcode for psDestInst
	*/
	switch (g_psInstDesc[psSrcInst->eOpcode].eType)
	{
		case INST_TYPE_FLOAT:
		{
			*psDestInst->u.psFloat = *psSrcInst->u.psFloat;
			break;
		}
		case INST_TYPE_SOP2:
		{
			*psDestInst->u.psSop2 = *psSrcInst->u.psSop2;
			break;
		}
		case INST_TYPE_SOPWM:
		{
			*psDestInst->u.psSopWm = *psSrcInst->u.psSopWm;
			break;
		}
		case INST_TYPE_SOP3:
		{
			*psDestInst->u.psSop3 = *psSrcInst->u.psSop3;
			break;
		}
		case INST_TYPE_LDST:
		{
			*psDestInst->u.psLdSt = *psSrcInst->u.psLdSt;
			break;
		}
		case INST_TYPE_DOT34:
		{
			*psDestInst->u.psDot34 = *psSrcInst->u.psDot34;
			break;
		}
		case INST_TYPE_EFO:
		{
			*psDestInst->u.psEfo = *psSrcInst->u.psEfo;
			break;
		}

#if defined(SUPPORT_SGX545)
		case INST_TYPE_IMA32:
		{
			*psDestInst->u.psIma32 = *psSrcInst->u.psIma32;
			break;
		}
		case INST_TYPE_DUAL:
		{
			*psDestInst->u.psDual = *psSrcInst->u.psDual;
			break;
		}
		case IEMIT:
		{
			psDestInst->u.sEmit = psSrcInst->u.sEmit;
			break;
		}
#endif /* defined(SUPPORT_SGX545) */

		case INST_TYPE_TEST:
		{
			*psDestInst->u.psTest = *psSrcInst->u.psTest;
			break;
		}
		case INST_TYPE_MOVP:
		{
			*psDestInst->u.psMovp = *psSrcInst->u.psMovp;
			break;
		}
		case INST_TYPE_BITWISE:
		{
			*psDestInst->u.psBitwise = *psSrcInst->u.psBitwise;
			break;
		}
		case INST_TYPE_FEEDBACKDRIVEREPILOG:
		{
			imgabort();
		}
		case INST_TYPE_MOVC:
		{
			*psDestInst->u.psMovc = *psSrcInst->u.psMovc;
			break;
		}
		case INST_TYPE_FARITH16:
		{
			*psDestInst->u.psArith16 = *psSrcInst->u.psArith16;
			break;
		}
		case INST_TYPE_FPMA:
		{
			*psDestInst->u.psFpma = *psSrcInst->u.psFpma;
			break;
		}
		case INST_TYPE_LRP1:
		{
			*psDestInst->u.psLrp1 = *psSrcInst->u.psLrp1;
			break;
		}
		case INST_TYPE_SMP:
		{
			*psDestInst->u.psSmp = *psSrcInst->u.psSmp;
			break;
		}

#if defined(OUTPUT_USPBIN)
		case INST_TYPE_SMPUNPACK:
		{
			*psDestInst->u.psSmpUnpack = *psSrcInst->u.psSmpUnpack;
			break;
		}
#endif /* defined(OUTPUT_USPBIN) */

#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
		case INST_TYPE_NRM:
		{
			*psDestInst->u.psNrm = *psSrcInst->u.psNrm;
			break;
		}
#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case INST_TYPE_VEC:
		{
			*psDestInst->u.psVec = *psSrcInst->u.psVec;
			break;
		}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

		case INST_TYPE_IMA16:
		{
			*psDestInst->u.psIma16 = *psSrcInst->u.psIma16;
			break;
		}
		case INST_TYPE_IMAE:
		{
			*psDestInst->u.psImae = *psSrcInst->u.psImae;
			break;
		}
		case INST_TYPE_PCK:
		{
			*psDestInst->u.psPck = *psSrcInst->u.psPck;
			break;
		}
		case INST_TYPE_CALL:
		{
			*psDestInst->u.psCall = *psSrcInst->u.psCall;
			break;
		}
		case INST_TYPE_DPC:
		{
			*psDestInst->u.psDpc = *psSrcInst->u.psDpc;
			break;
		}
		case INST_TYPE_FDOTPRODUCT:
		{
			*psDestInst->u.psFdp = *psSrcInst->u.psFdp;
			break;
		}
		case INST_TYPE_DELTA:
		{
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			psDestInst->u.psDelta->bVector = psSrcInst->u.psDelta->bVector;
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
			break;
		}
		case INST_TYPE_LOADCONST:
		{
			*psDestInst->u.psLoadConst = *psSrcInst->u.psLoadConst;
			break;
		}
		case INST_TYPE_LOADMEMCONST:
		{
			*psDestInst->u.psLoadMemConst = *psSrcInst->u.psLoadMemConst;
			break;
		}
		case INST_TYPE_LDSTARR:
		{
			*psDestInst->u.psLdStArray = *psSrcInst->u.psLdStArray;
			break;
		}
		case INST_TYPE_MEMSPILL:
		{
			*psDestInst->u.psMemSpill = *psSrcInst->u.psMemSpill;
			break;
		}
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case INST_TYPE_IDXSC:
		{
			*psDestInst->u.psIdxsc = *psSrcInst->u.psIdxsc;
			break;
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		case INST_TYPE_CVTFLT2INT:
		{
			*psDestInst->u.psCvtFlt2Int = *psSrcInst->u.psCvtFlt2Int;
			break;
		}		
		default:
		{
			break;
		}
	}

	return psDestInst;
}

IMG_INTERNAL
IMG_VOID MakeSOPWMMove(PINTERMEDIATE_STATE psState, PINST psMOVInst)
/*****************************************************************************
 FUNCTION	: MakeSOPWMMove
    
 PURPOSE	: Initialize an instruction as a SOPWM copying the first source.

 PARAMETERS	: psState    - Compiler state
              psMOVInst	 - Instruction to initialize.

 RETURNS	: Nothing.
*****************************************************************************/
{
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		SetOpcode(psState, psMOVInst, ISOPWM_VEC);

		SetSrcUnused(psState, psMOVInst, 1 /* uSrcIdx */);
		SetSrc(psState, psMOVInst, 2 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, 0, UF_REGFORMAT_U8);
		SetSrc(psState, psMOVInst, 3 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, 0, UF_REGFORMAT_U8);
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		SetOpcode(psState, psMOVInst, ISOPWM);

		SetSrc(psState, psMOVInst, 1 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, 0, UF_REGFORMAT_U8);
	}

	psMOVInst->u.psSopWm->uSel1 = USEASM_INTSRCSEL_ZERO;
	psMOVInst->u.psSopWm->bComplementSel1 = IMG_TRUE;
	psMOVInst->u.psSopWm->uSel2 = USEASM_INTSRCSEL_ZERO;
	psMOVInst->u.psSopWm->bComplementSel2 = IMG_FALSE;
	psMOVInst->u.psSopWm->uCop = USEASM_INTSRCSEL_ADD;
	psMOVInst->u.psSopWm->uAop = USEASM_INTSRCSEL_ADD;
}

IMG_INTERNAL
PINST FormMove(PINTERMEDIATE_STATE psState,
			   PARG psDst,
			   PARG psSrc,
			   PINST psSrcLine)
/*****************************************************************************
 FUNCTION	: FormMove
    
 PURPOSE	: Form a move instruction

 PARAMETERS	: psState    - Compiler state
              psDst      - Destination
              psSrc      - Source
              psSrcLine  - Source line instruction (or NULL)

 RETURNS	: Newly allocated mov from psSrc to psDst
*****************************************************************************/
{
	PINST psInst;

	psInst = AllocateInst(psState, psSrcLine);
	SetOpcode(psState, psInst, IMOV);

	if (psDst != NULL)
	{
		psInst->asDest[0].uType = psDst->uType;
		psInst->asDest[0].uNumber = psDst->uNumber;
		psInst->asDest[0].uIndexType = psDst->uIndexType;
		psInst->asDest[0].uIndexNumber = psDst->uIndexNumber;
		psInst->asDest[0].uIndexArrayOffset = psDst->uIndexArrayOffset;
		psInst->asDest[0].uIndexStrideInBytes = psDst->uIndexStrideInBytes;
		psInst->asDest[0].uArrayOffset = psDst->uArrayOffset;
	}
	if (psSrc != NULL)
	{
		psInst->asArg[0].uType = psSrc->uType;
		psInst->asArg[0].uNumber = psSrc->uNumber;
		psInst->asArg[0].uIndexType = psSrc->uIndexType;
		psInst->asArg[0].uIndexNumber = psSrc->uIndexNumber;
		psInst->asArg[0].uIndexArrayOffset = psSrc->uIndexArrayOffset;
		psInst->asArg[0].uIndexStrideInBytes = psSrc->uIndexStrideInBytes;
		psInst->asArg[0].uArrayOffset = psSrc->uArrayOffset;
	}

	return psInst;
}

IMG_INTERNAL
IMG_VOID MakePartialDestCopy(PINTERMEDIATE_STATE	psState, 
							 PINST					psCopyFromInst, 
							 IMG_UINT32				uCopyFromDestIdx,
							 IMG_BOOL				bBefore,
							 PARG					psNewDest)
/*********************************************************************************
 Function			: MakePartialDestCopy

 Description		: Insert an instruction to copy the partial destination into a
					  newly allocated temporary and replace the partial destination by
					  the new temporary.

 Parameters			: psState			- Compiler state.
					  psCopyFromInst	- Partial destination argument to be copied.
					  uCopyFromDestIdx
					  bBefore			- If TRUE insert the copy instruction before the 
					                      original instruction.
										  If FALSE insert it after.
					  psNewDest			- If non-NULL returns details of the newly
										allocated temporary.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	ARG				sNewDest;
	PINST			psCopyInst;
	UF_REGFORMAT	eFmt;
	IMG_UINT32		uPartialDestUsedChanMask;

	ASSERT(uCopyFromDestIdx < psCopyFromInst->uDestCount);
	eFmt = psCopyFromInst->asDest[uCopyFromDestIdx].eFmt;

	uPartialDestUsedChanMask = GetPreservedChansInPartialDest(psState, psCopyFromInst, uCopyFromDestIdx);

	if (psCopyFromInst->asDest[uCopyFromDestIdx].uType == USEASM_REGTYPE_FPINTERNAL)
	{
		/*
			If the destination is a GPI then there may be restrictions on the destination of
			the modified instruction as well, so a temp may not be allocated to it.
			So just reuse the same GPI.
		*/
		sNewDest = psCopyFromInst->asDest[uCopyFromDestIdx];
	}
	else
	{
		MakeNewTempArg(psState, eFmt, &sNewDest);
	}

	if (psNewDest != NULL)
	{
		*psNewDest = sNewDest;
	}

	psCopyInst = AllocateInst(psState, psCopyFromInst);

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		SetOpcode(psState, psCopyInst, IVMOV);

		psCopyInst->auDestMask[0] = uPartialDestUsedChanMask;

		psCopyInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
		psCopyInst->u.psVec->aeSrcFmt[0] = eFmt;
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		SetOpcode(psState, psCopyInst, IMOV);
	}

	if (bBefore)
	{
		/*
			INST	DEST[=OLDDEST], ....
				->
			MOV		TEMP, OLDDEST
			INST	DEST[=TEMP], ....
		*/

		InsertInstBefore(psState, psCopyFromInst->psBlock, psCopyInst, psCopyFromInst);
	
		MovePartialDestToSrc(psState, psCopyInst, 0 /* uMoveToSrcIdx */, psCopyFromInst, uCopyFromDestIdx);
		SetDestFromArg(psState, psCopyInst, 0 /* uDestIdx */, &sNewDest);

		SetPartiallyWrittenDest(psState, psCopyFromInst, uCopyFromDestIdx, &sNewDest);

		psCopyInst->auLiveChansInDest[0] = uPartialDestUsedChanMask;
	}
	else
	{
		IMG_UINT32	uPred;

		/*
			INST	DEST[=OLDDEST], ...
				->
			INST	TEMP, ...
			MOV		DEST[=TEMP], ...
		*/

		/*
			Copy the predicates from the existing instruction.
		*/
		for (uPred = 0; uPred < psCopyFromInst->uPredCount; ++uPred)
		{
			IMG_UINT32	uPredSrc;
			IMG_BOOL	bPredNegate, bPredPerChan;

			GetPredicate(psCopyFromInst, &uPredSrc, &bPredNegate, &bPredPerChan, uPred);

			if (bPredPerChan)
			{
				SetBit(psCopyInst->auFlag, INST_PRED_PERCHAN, 1);
			}

			bPredNegate = (!bPredNegate) ? IMG_TRUE : IMG_FALSE;
			SetPredicateAtIndex(psState, psCopyInst, uPredSrc, bPredNegate, uPred);
		}

		InsertInstAfter(psState, psCopyFromInst->psBlock, psCopyInst, psCopyFromInst);

		/*
			Move the existing instruction's destination to the move instruction.
		*/
		MoveDest(psState, psCopyInst, 0 /* uMoveToDestIdx */, psCopyFromInst, uCopyFromDestIdx);
		psCopyInst->auLiveChansInDest[0] = psCopyFromInst->auLiveChansInDest[uCopyFromDestIdx];

		/*
			Move the partial destination of the existing instruction to the move instruction's source.
		*/
		MovePartialDestToSrc(psState, psCopyInst, 0 /* uMoveToSrcIdx */, psCopyFromInst, uCopyFromDestIdx);
		SetPartiallyWrittenDest(psState, psCopyFromInst, uCopyFromDestIdx, NULL);

		/*
			Set the new temporary as the destination of the existing instruction and the partial
			destination of the move instruction.
		*/
		SetDestFromArg(psState, psCopyFromInst, uCopyFromDestIdx, &sNewDest);
		SetPartiallyWrittenDest(psState, psCopyInst, 0 /* uSrcIdx */, &sNewDest);

		/*
			Update the mask of channels used from the existing instruction's destination.
		*/
		psCopyFromInst->auLiveChansInDest[uCopyFromDestIdx] = 
			GetPreservedChansInPartialDest(psState, psCopyInst, 0 /* uDestIdx */);
	}
}

IMG_INTERNAL
IMG_BOOL IsMove(PINST psInst, PARG *ppsDst, PARG *ppsSrc)
/*****************************************************************************
 FUNCTION	: IsMove

 PURPOSE	: Test whether an instruction is a move operation

 PARAMETERS	: psInst     - Instruction to test

 OUTPUT     : ppsDst     - Destination
              ppsSrc     - Source

 NOTES      : Does not test the destination mask so partial moves are allowed.

 RETURNS	: IMG_TRUE iff psInst is a move instruction
*****************************************************************************/
{
	IMG_BOOL bIsMove = IMG_FALSE;
	PARG psSrc = NULL;
	PARG psDst = NULL;

	if (psInst == NULL)
		return IMG_FALSE;

	/* Test for simple move */
	if (psInst->eOpcode == IMOV)
	{
		psDst = &(psInst->asDest[0]);
		psSrc = &(psInst->asArg[0]);
		bIsMove = IMG_TRUE;
		goto Exit;
	}


	/* Test SOP */
	if ((psInst->eOpcode == ISOP2 &&
		 psInst->u.psSop2 != NULL) ||
		((psInst->eOpcode == ISOPWM &&
		  psInst->u.psSopWm != NULL)))
	{
		IMG_UINT32 uSrc1Val;
		IMG_UINT32 uSrc2Val;
		IMG_BOOL bSopIsMove = IMG_FALSE;
		IMG_UINT32 uCSrc1Fact = USEASM_INTSRCSEL_END;
		IMG_UINT32 uCSrc2Fact = USEASM_INTSRCSEL_END;
		IMG_BOOL bCSrc1Comp = IMG_FALSE, bCSrc2Comp = IMG_FALSE;
		IMG_UINT32 uASrc1Fact = USEASM_INTSRCSEL_END;
		IMG_UINT32 uASrc2Fact = USEASM_INTSRCSEL_END;
		IMG_BOOL bASrc1Comp = IMG_FALSE, bASrc2Comp = IMG_FALSE;

		if (psInst->eOpcode == ISOP2)
		{
			PSOP2_PARAMS psParams = psInst->u.psSop2;

			/* Test operations */
			if (psParams->uCOp != USEASM_INTSRCSEL_ADD ||
				psParams->bComplementCSrc1 ||
				psParams->uAOp != USEASM_INTSRCSEL_ADD ||
				psParams->bComplementASrc1)
			{
				return IMG_FALSE;
			}

			/* Extract the factors */
			uCSrc1Fact = psParams->uCSel1;
			bCSrc1Comp = psParams->bComplementCSel1;

			uCSrc2Fact = psParams->uCSel2;
			bCSrc2Comp = psParams->bComplementCSel2;

			uASrc1Fact = psParams->uASel1;
			bASrc1Comp = psParams->bComplementASel1;

			uASrc2Fact = psParams->uASel2;
			bASrc2Comp = psParams->bComplementASel2;

			bSopIsMove = IMG_TRUE;
		}
		else if (psInst->eOpcode == ISOPWM)
		{
			PSOPWM_PARAMS psParams = psInst->u.psSopWm;

			/* Test operations */
			if (!(psParams->uCop == USEASM_INTSRCSEL_ADD &&
				  psParams->uAop == USEASM_INTSRCSEL_ADD))
			{
				return IMG_FALSE;
			}

			/* Extract the factors */
			uCSrc1Fact = psParams->uSel1;
			bCSrc1Comp = psParams->bComplementSel1;

			uCSrc2Fact = psParams->uSel2;
			bCSrc2Comp = psParams->bComplementSel2;

			uASrc1Fact = psParams->uSel1;
			bASrc1Comp = psParams->bComplementSel1;

			uASrc2Fact = psParams->uSel2;
			bASrc2Comp = psParams->bComplementSel2;

			bSopIsMove = IMG_TRUE;
		}

		/* Test factors */
		if (bSopIsMove)
		{
			/* Work out which source is in use */
			if ((uCSrc1Fact == USEASM_INTSRCSEL_ONE &&
				 !bCSrc1Comp) ||
				(uCSrc1Fact == USEASM_INTSRCSEL_ZERO &&
				 bCSrc1Comp))
			{
				/* Test alpha part */
				if ((uASrc1Fact == USEASM_INTSRCSEL_ONE &&
					 !bASrc1Comp) ||
					(uASrc1Fact == USEASM_INTSRCSEL_ZERO &&
					 bASrc1Comp))
				{
					uSrc1Val = USEASM_INTSRCSEL_ONE;
				}
				else
				{
					/* Invalid factors */
					return IMG_FALSE;
				}
			}
			else if ((uCSrc1Fact == USEASM_INTSRCSEL_ONE &&
					  bCSrc1Comp) ||
					 (uCSrc1Fact == USEASM_INTSRCSEL_ZERO &&
					  !bCSrc1Comp))
			{
				/* Test alpha part */
				if ((uASrc1Fact == USEASM_INTSRCSEL_ONE &&
					 bASrc1Comp) ||
					(uASrc1Fact == USEASM_INTSRCSEL_ZERO &&
					 !bASrc1Comp))
				{
					uSrc1Val = USEASM_INTSRCSEL_ZERO;
				}
				else
				{
					/* Invalid factors */
					return IMG_FALSE;
				}
			}
			else
			{
				/* Invalid factors */
				return IMG_FALSE;
			}

			if ((uCSrc2Fact == USEASM_INTSRCSEL_ONE &&
				 !bCSrc2Comp) ||
				(uCSrc2Fact == USEASM_INTSRCSEL_ZERO &&
				 bCSrc2Comp))
			{
				/* Test alpha part */
				if ((uASrc2Fact == USEASM_INTSRCSEL_ONE &&
					 !bASrc2Comp) ||
					(uASrc2Fact == USEASM_INTSRCSEL_ZERO &&
					 bASrc2Comp))
				{
					uSrc2Val = USEASM_INTSRCSEL_ONE;
				}
				else
				{
					/* Invalid factors */
					return IMG_FALSE;
				}
			}
			else if ((uCSrc2Fact == USEASM_INTSRCSEL_ONE &&
					  bCSrc2Comp) ||
					 (uCSrc2Fact == USEASM_INTSRCSEL_ZERO &&
					  !bCSrc2Comp))
			{
				/* Test alpha part */
				if ((uASrc2Fact == USEASM_INTSRCSEL_ONE &&
					 bASrc2Comp) ||
					(uASrc2Fact == USEASM_INTSRCSEL_ZERO &&
					 !bASrc2Comp))
				{
					uSrc2Val = USEASM_INTSRCSEL_ZERO;
				}
				else
				{
					/* Invalid factors */
					return IMG_FALSE;
				}
			}
			else
			{
				/* Invalid factors */
				return IMG_FALSE;
			}

			/* Test for different factors */
			if (uSrc1Val == uSrc2Val)
			{
				return IMG_FALSE;
			}
				
			/* Extract the source */
			if (uSrc1Val == USEASM_INTSRCSEL_ONE)
				psSrc = &(psInst->asArg[0]);
			else if (uSrc2Val == USEASM_INTSRCSEL_ONE)
				psSrc = &(psInst->asArg[1]);
			else
				return IMG_FALSE;

			/* Check for a format conversion (U8 -> C10 or C10 -> U8). */
			if (psSrc->eFmt != psInst->asDest[0].eFmt)
			{
				return IMG_FALSE;
			}

			bSopIsMove = IMG_TRUE;
		}

		if (bSopIsMove)
		{
			psDst = &psInst->asDest[0];
        }
        
		if (bSopIsMove)
		{
			bIsMove = IMG_TRUE;
			goto Exit;
		}
		else
		{
			return IMG_FALSE;
		}
	}

  Exit:
	if (bIsMove)
	{
		if (ppsDst != NULL)
			(*ppsDst) = psDst;
		if (ppsSrc != NULL)
			(*ppsSrc) = psSrc;

		return IMG_TRUE;
	}

	return IMG_FALSE;
}

IMG_INTERNAL
IMG_VOID ConvertRegister(PINTERMEDIATE_STATE psState, PARG psReg)
/*********************************************************************************
 Function			: ConvertRegister

 Description		: Convert a usc register to a useasm register
 
 Parameters			: psState	- Internal compiler state
					  psArg		- Register to convert

 Return				: Nothing.
*********************************************************************************/
{
	/* Only register arrays need to be converted */
	if (psReg->uType == USC_REGTYPE_REGARRAY)
	{
		const USC_VEC_ARRAY_REG	*psArrayReg = psState->apsVecArrayReg[psReg->uNumber];
		const IMG_UINT32			uBaseReg = psArrayReg->uBaseReg;

		/* Register array access becomes an indexed temporay register */
		psReg->uType = psArrayReg->uRegType;
		if (psReg->uIndexType == USC_REGTYPE_NOINDEX || psArrayReg->eArrayType != ARRAY_TYPE_NORMAL)
		{
			/* Add the base of the array directly to the register number. */
			psReg->uNumber = uBaseReg + psReg->uArrayOffset;
		}
		else
		{
			/* The array base is added in a seperate instruction. */
			psReg->uNumber = psReg->uArrayOffset;
		}

		/* Clear the unused values */
		psReg->uArrayOffset = 0;
	}
	else if  (psReg->uType == USC_REGTYPE_ARRAYBASE)
	{
		const IMG_UINT32 uBaseReg = psState->apsVecArrayReg[psReg->uNumber]->uBaseReg;

		/* Register array access becomes an indexed temporay register */
		psReg->uType = USEASM_REGTYPE_IMMEDIATE;
		psReg->uNumber = uBaseReg + psReg->uArrayOffset;

		/* Clear the unused values */
		psReg->uArrayOffset = 0;
	}
}

IMG_INTERNAL
IMG_BOOL SameRegister(const ARG *psArgA, const ARG *psArgB)
/*****************************************************************************
 FUNCTION	: SameRegister
    
 PURPOSE	: Check if two argument structures refer to the same register.

 PARAMETERS	: 
			  
 RETURNS	: IMG_TRUE if the arguments are equal.
*****************************************************************************/
{
	return (psArgA->uType == psArgB->uType &&
		    psArgA->uNumber == psArgB->uNumber &&
		    psArgA->uIndexType == psArgB->uIndexType &&
		    psArgA->uIndexNumber == psArgB->uIndexNumber &&
		    psArgA->uIndexStrideInBytes == psArgB->uIndexStrideInBytes &&
		    psArgA->uIndexArrayOffset == psArgB->uIndexArrayOffset &&
		    psArgA->uArrayOffset == psArgB->uArrayOffset) ? IMG_TRUE : IMG_FALSE;
}

IMG_INTERNAL 
IMG_BOOL EqualArgs(const ARG *psArgA, const ARG *psArgB)
/*****************************************************************************
 FUNCTION	: EqualArgs
    
 PURPOSE	: Check if two arguments are equal.

 PARAMETERS	: 
			  
 RETURNS	: IMG_TRUE if the arguments are equal.
*****************************************************************************/
{
	return (psArgA->uType == psArgB->uType &&
		    psArgA->uNumber == psArgB->uNumber &&
		    psArgA->uIndexType == psArgB->uIndexType &&
		    psArgA->uIndexNumber == psArgB->uIndexNumber &&
		    psArgA->uIndexStrideInBytes == psArgB->uIndexStrideInBytes &&
		    psArgA->uIndexArrayOffset == psArgB->uIndexArrayOffset &&
		    psArgA->uArrayOffset == psArgB->uArrayOffset) ? IMG_TRUE : IMG_FALSE;
}

IMG_INTERNAL
IMG_BOOL IsDeschedBeforeInst(PINTERMEDIATE_STATE psState, PCINST psInst)
/*****************************************************************************
 FUNCTION	: IsDeschedBeforeInst
    
 PURPOSE	: Checks if there is a deschedule before an instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  
 RETURNS	: IMG_TRUE or IMG_FALSE.
*****************************************************************************/
{
	if (
			(psState->uNumDynamicBranches > 0) &&
			RequiresGradients(psInst)
	   )
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_BOOL IsDeschedAfterInst(PCINST psInst)
/*****************************************************************************
 FUNCTION	: IsDeschedAfterInst
    
 PURPOSE	: Checks if there is a deschedule after an instruction.

 PARAMETERS	: psInst		- Instruction to check.
			  
 RETURNS	: IMG_TRUE or IMG_FALSE.
*****************************************************************************/
{
	if (
			(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_MEMLOAD) != 0 ||
			(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_MEMSTORE) != 0 ||
			(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_DEPENDENTTEXTURESAMPLE) != 0 ||
			#if defined(SUPPORT_SGX545)
			psInst->eOpcode == IIDIV32 ||
			psInst->eOpcode == IWOP ||
			#endif /* defined(SUPPORT_SGX545) */
			#if defined(OUTPUT_USPBIN)
			psInst->eOpcode == ITEXWRITE ||
			#endif //defined(OUTPUT_USPBIN)
			psInst->eOpcode == IIDF
	   )
	{
		return IMG_TRUE;
	}
	#if defined(OUTPUT_USPBIN)	
	if (psInst->eOpcode == ISMPUNPACK_USP)
	{
		PINST	psTextureSample;

		psTextureSample = psInst->u.psSmpUnpack->psTextureSample;
		
		/*
			If the texture sample instruction generateing the data to be unpacking is a non-dependent
			texture sample then the unpack doesn't deschedule.
		*/
		if (psTextureSample->eOpcode == ISMP_USP_NDR)
		{
			return IMG_FALSE;
		}
		else
		{
			return IMG_TRUE;
		}
	}
	#endif /* defined(OUTPUT_USPBIN) */
	return IMG_FALSE;
}

IMG_INTERNAL 
IMG_BOOL IsDeschedulingPoint(PINTERMEDIATE_STATE psState, PCINST psInst)
/*****************************************************************************
 FUNCTION	: IsDeschedulingPoint
    
 PURPOSE	: Checks if an instruction is a potential descheduling point.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  
 RETURNS	: IMG_TRUE or IMG_FALSE.
*****************************************************************************/
{
	return IsDeschedBeforeInst(psState, psInst) || IsDeschedAfterInst(psInst);
} 

IMG_INTERNAL
IMG_BOOL IsUniform(PINTERMEDIATE_STATE psState, PUSEDEF_CHAIN psUseDefChain)
/*****************************************************************************
 FUNCTION	: IsUniform
    
 PURPOSE	: Check if an intermediate register contains uniform data (data which is
			  same for every instance).

 PARAMETERS	: psState		- Compiler state.
			  psUseDefChain	- Intermediate register.
			  
 RETURNS	: IMG_TRUE or IMG_FALSE.
*****************************************************************************/
{
	PUSEDEF			psDef;

	psDef = psUseDefChain->psDef;
	if (psDef == NULL)
	{
		return IMG_FALSE;
	}
	switch (psDef->eType)
	{
		case DEF_TYPE_INST:
		{
			PINST	psDefInst = psDef->u.psInst;

			if (psDefInst->psBlock->psOwner == &psState->psSecAttrProg->sCfg)
			{
				return IMG_TRUE;
			}
			return IMG_FALSE;
		}
		case DEF_TYPE_FIXEDREG:
		{
			PFIXED_REG_DATA	psFixedReg = psUseDefChain->psDef->u.psFixedReg;

			if (!psFixedReg->bPrimary)
			{
				return IMG_TRUE;
			}
			return IMG_FALSE;
		}
		default:
		{
			return IMG_FALSE;
		}
	}
}

static IMG_BOOL IsUniformTemp(PINTERMEDIATE_STATE psState, PARG psArg)
/*****************************************************************************
 FUNCTION	: IsUniformTemp
    
 PURPOSE	: Check if an intermediate register contains uniform data (data which is
			  same for every instance).

 PARAMETERS	: psState		- Compiler state.
			  psArg			- Argument to check.
			  
 RETURNS	: IMG_TRUE or IMG_FALSE.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psUseDefChain;
	
	psUseDefChain = psArg->psRegister->psUseDefChain;
	return IsUniform(psState, psUseDefChain);
}

IMG_INTERNAL
IMG_BOOL IsNonSSARegister(PINTERMEDIATE_STATE psState, IMG_UINT32 uType, IMG_UINT32 uNumber)
/*****************************************************************************
 FUNCTION	: IsNonSSARegister

 PURPOSE	: Check for an intermediate register written more than once.

 PARAMETERS	: psState			- Compiler state.
			  uType				- Register to check.
			  uNumber

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (uType == USEASM_REGTYPE_FPINTERNAL)
	{
		return IMG_TRUE;
	}
	if (IsModifiableRegisterArray(psState, uType, uNumber))
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_BOOL IsNonSSAArgument(PINTERMEDIATE_STATE psState, PARG psArg)
/*****************************************************************************
 FUNCTION	: IsNonSSAArgument

 PURPOSE	: Check for an instruction argument which references an intermediate
			  register written more than once.

 PARAMETERS	: psState			- Compiler state.
			  psArg				- Argument to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (IsNonSSARegister(psState, psArg->uType, psArg->uNumber))
	{
		return IMG_TRUE;
	}
	if (IsNonSSARegister(psState, psArg->uIndexType, psArg->uIndexNumber))
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_BOOL IsInstReferencingNonSSAData(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: IsInstReferencingNonSSAData

 PURPOSE	: Check if any instruction argument references an intermediate
			  register written more than once.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uDestIdx;
	IMG_UINT32	uSrcIdx;

	/*
		Check for reading or writing an indexable array which can be modified inside the program. As above
		this avoids tracking dependencies.
	*/
	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		PARG	psDest = &psInst->asDest[uDestIdx];
		PARG	psOldDest = psInst->apsOldDest[uDestIdx];

		if (psDest->uType != USEASM_REGTYPE_TEMP && psDest->uType != USEASM_REGTYPE_PREDICATE)
		{
			return IMG_TRUE;
		}
		/* Check for using an indexable array as an index in the destination. */
		if (IsNonSSARegister(psState, psDest->uIndexType, psDest->uIndexNumber))
		{
			return IMG_FALSE;
		}
		/* Check for using an indexable array in a conditionally/partially overwritten destination. */
		if (psOldDest != NULL && IsNonSSAArgument(psState, psOldDest))
		{
			return IMG_TRUE;
		}
	}
	/*
		Check for using an indexable array in a source argument.
	*/
	for (uSrcIdx = 0; uSrcIdx < psInst->uArgumentCount; uSrcIdx++)
	{
		if (IsNonSSAArgument(psState, &psInst->asArg[uSrcIdx]))
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_BOOL IsUniformSource(PINTERMEDIATE_STATE psState, PARG psArg)
/*****************************************************************************
 FUNCTION	: IsUniformSource
    
 PURPOSE	: Check if a source argument contains uniform data (data which is
			  same for every instance).

 PARAMETERS	: psState		- Compiler state.
			  psArg			- Argument to check.
			  
 RETURNS	: IMG_TRUE or IMG_FALSE.
*****************************************************************************/
{
	if (psArg->uIndexType != USC_REGTYPE_NOINDEX)
	{
		return IMG_FALSE;
	}

	switch (psArg->uType)
	{
		case USEASM_REGTYPE_TEMP:
		{
			return IsUniformTemp(psState, psArg);
		}
		case USC_REGTYPE_REGARRAY:
		{	
			PUSC_VEC_ARRAY_REG	psArray;

			ASSERT(psArg->uNumber < psState->uNumVecArrayRegs);
			psArray = psState->apsVecArrayReg[psArg->uNumber];
			ASSERT(psArray != NULL);

			if (psArray->eArrayType == ARRAY_TYPE_DRIVER_LOADED_SECATTR ||
				psArray->eArrayType == ARRAY_TYPE_DIRECT_MAPPED_SECATTR)
			{
				return IMG_TRUE;
			}
			return IMG_FALSE;
		}
		case USEASM_REGTYPE_IMMEDIATE:
		case USEASM_REGTYPE_FPCONSTANT:
		case USC_REGTYPE_UNUSEDSOURCE:
		{
			return IMG_TRUE;
		}
		default:
		{
			return IMG_FALSE;
		}
	}
}

IMG_INTERNAL 
IMG_BOOL CanUseSource0(PINTERMEDIATE_STATE psState, PFUNC psOwner, const ARG *psArg)
/*****************************************************************************
 FUNCTION	: CanUseSource0
    
 PURPOSE	: Check if an argument could be used in hardware source 0.

 PARAMETERS	: 
			  
 RETURNS	: IMG_TRUE if the argument could go source 0.
*****************************************************************************/
{
	IMG_UINT32	uArgType;

	if (IsDriverLoadedSecAttr(psState, psArg))
	{
		if (psOwner == psState->psSecAttrProg)
		{
			uArgType = USEASM_REGTYPE_PRIMATTR;
		}
		else
		{
			uArgType = USEASM_REGTYPE_SECATTR;
		}
	}
	else
	{
		uArgType = psArg->uType;
	}

	return IsSrc0StandardBanks(uArgType, psArg->uIndexType);
}

static
IMG_UINT32 GetIMAEComponent(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArgIdx)
/*****************************************************************************
 FUNCTION	: GetIMAEComponent

 PURPOSE	: Get the component select on an IMAE source.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction.
			  uArgIdx		- Index of the source to get the component select
							for.

 RETURNS	: The component select.
*****************************************************************************/
{
	if (uArgIdx < IMAE_UNIFIED_STORE_SOURCE_COUNT)
	{
		return psInst->u.psImae->auSrcComponent[uArgIdx];
	}
	else
	{
		ASSERT(uArgIdx == IMAE_CARRYIN_SOURCE);
		return 0;
	}
}

IMG_INTERNAL
IMG_INT32 CompareInstructionArgumentParameters(PINTERMEDIATE_STATE	psState,
											   const INST			*psInst1,
											   IMG_UINT32			uInst1ArgIdx,
											   const INST			*psInst2,
											   IMG_UINT32			uInst2ArgIdx)
/*****************************************************************************
 FUNCTION	: CompareInstructionArgumentParameters

 PURPOSE	: Check if argument specific modifiers on two instructions are
			  equal.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- First instruction to check.
			  uInst1ArgIdx		- Index of the first argument to check.
			  psInst2			- Second instruction to check.
			  uInst2ArgIdx		- Index of the second argument to check.

 RETURNS	: -ve		If the first parameters are less than the second parameters.
			  0			If the first parameters are equal to the second parameters.
			  +ve		If the first parameters are greater than the second parameters.
*****************************************************************************/
{
	ASSERT(g_psInstDesc[psInst1->eOpcode].eType == g_psInstDesc[psInst1->eOpcode].eType);

	switch (g_psInstDesc[psInst1->eOpcode].eType)
	{
		case INST_TYPE_FLOAT:
		{
			PFLOAT_SOURCE_MODIFIER	psMod1 = &psInst1->u.psFloat->asSrcMod[uInst1ArgIdx];
			PFLOAT_SOURCE_MODIFIER	psMod2 = &psInst2->u.psFloat->asSrcMod[uInst2ArgIdx];

			return CompareFloatSourceModifier(psMod1, psMod2);
		}
		case INST_TYPE_DPC:
		{
			PFLOAT_SOURCE_MODIFIER	psMod1 = &psInst1->u.psDpc->sFloat.asSrcMod[uInst1ArgIdx];
			PFLOAT_SOURCE_MODIFIER	psMod2 = &psInst2->u.psDpc->sFloat.asSrcMod[uInst2ArgIdx];

			return CompareFloatSourceModifier(psMod1, psMod2);
		}
		case INST_TYPE_EFO:
		{
			PFLOAT_SOURCE_MODIFIER	psMod1 = &psInst1->u.psEfo->sFloat.asSrcMod[uInst1ArgIdx];
			PFLOAT_SOURCE_MODIFIER	psMod2 = &psInst2->u.psEfo->sFloat.asSrcMod[uInst2ArgIdx];

			return CompareFloatSourceModifier(psMod1, psMod2);
		}
		case INST_TYPE_FARITH16:
		{
			PFARITH16_PARAMS		psParams1 = psInst1->u.psArith16;
			PFARITH16_PARAMS		psParams2 = psInst2->u.psArith16;
			FMAD16_SWIZZLE			eSwizzle1 = psParams1->aeSwizzle[uInst1ArgIdx];
			FMAD16_SWIZZLE			eSwizzle2 = psParams2->aeSwizzle[uInst2ArgIdx];
			PFLOAT_SOURCE_MODIFIER	psMod1 = &psInst1->u.psArith16->sFloat.asSrcMod[uInst1ArgIdx];
			PFLOAT_SOURCE_MODIFIER	psMod2 = &psInst2->u.psArith16->sFloat.asSrcMod[uInst2ArgIdx];
			IMG_INT32				iCmp;

			iCmp = CompareFloatSourceModifier(psMod1, psMod2);
			if (iCmp == 0)
			{
				return iCmp;
			}
			return eSwizzle1 - eSwizzle2;
		}
		case INST_TYPE_TEST:
		{
			PTEST_PARAMS	psTest1 = psInst1->u.psTest;
			PTEST_PARAMS	psTest2 = psInst2->u.psTest;

			return (IMG_INT32)(psTest1->auSrcComponent[uInst1ArgIdx] - psTest2->auSrcComponent[uInst2ArgIdx]);
		}
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) || defined(SUPPORT_SGX545)
		case INST_TYPE_IMA32:
		{
			return psInst1->u.psIma32->abNegate[uInst1ArgIdx] - psInst2->u.psIma32->abNegate[uInst2ArgIdx];
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) || defined(SUPPORT_SGX545) */
		case INST_TYPE_PCK:
		{
			PPCK_PARAMS	psPck1 = psInst1->u.psPck;
			PPCK_PARAMS psPck2 = psInst2->u.psPck;

			ASSERT(uInst1ArgIdx < PCK_SOURCE_COUNT);
			ASSERT(uInst2ArgIdx < PCK_SOURCE_COUNT);
			return (IMG_INT32)(psPck1->auComponent[uInst1ArgIdx] - psPck2->auComponent[uInst2ArgIdx]);
		}
		case INST_TYPE_IMAE:
		{
			return (IMG_INT32)(GetIMAEComponent(psState, psInst1, uInst1ArgIdx) - GetIMAEComponent(psState, psInst2, uInst2ArgIdx));
		}
		default:
		{
			return 0;
		}
	}
}

IMG_INTERNAL 
IMG_INT32 CompareArgs(const ARG *psArgA, const ARG *psArgB)
/*****************************************************************************
 FUNCTION	: CompareArgs
    
 PURPOSE	: Check if two arguments are equal.

 PARAMETERS	: psArgA, psArgB		- Arguments to compare.
			  
 RETURNS	: -ve		if the first argument is less than the second
				0		if the two arguments are equal.
			  +ve		if the first argument is greater than the second.
*****************************************************************************/
{
	COMPARE(psArgA->uType, psArgB->uType);
	COMPARE(psArgA->uNumber, psArgB->uNumber);
	COMPARE(psArgA->uArrayOffset, psArgB->uArrayOffset);
	COMPARE(psArgA->uIndexType, psArgB->uIndexType);
	COMPARE(psArgA->uIndexNumber, psArgB->uIndexNumber);
	COMPARE(psArgA->uIndexStrideInBytes, psArgB->uIndexStrideInBytes);
	COMPARE(psArgA->uIndexArrayOffset, psArgB->uIndexArrayOffset);
	return 0;
}

IMG_INTERNAL
IMG_INT32 CompareInstructionParameters(PINTERMEDIATE_STATE	psState,
									   const INST			*psInst1,
									   const INST			*psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstructionParameters

 PURPOSE	: Check if the instruction specific parameters are same for two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: -ve	if the first instruction is less than the second instruction.
			  0		if the two instructions are equal.
			  +ve	if the first instruction is greater than the second instruction.
*****************************************************************************/
{
	DBG_ASSERT(psInst1->eOpcode == psInst2->eOpcode);
	
	return gauInstructionOperationsJumpTable[g_psInstDesc[psInst1->eOpcode].eType].pfnCompareInstructionParams(psState, psInst1, psInst2);
}

IMG_INTERNAL
IMG_BOOL EqualInstructionParameters(PINTERMEDIATE_STATE		psState,
									const INST				*psInst1,
									const INST				*psInst2)
/*****************************************************************************
 FUNCTION	: EqualInstructionParameters

 PURPOSE	: Check if the instruction specific parameters are same for two
			  instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (CompareInstructionParameters(psState, psInst1, psInst2) == 0)
	{
		return IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
}

IMG_INTERNAL
IMG_BOOL EqualDestinationParameters(const INST				*psInst1,
									const INST				*psInst2)
/*****************************************************************************
 FUNCTION	: EqualDestinationParameters

 PURPOSE	: Check if modifiers on the destinations to two instructions are
			  equal.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uDestIdx;

	if (
			!(
				psInst1->eOpcode == psInst2->eOpcode &&
				GetBit(psInst1->auFlag, INST_SKIPINV) == GetBit(psInst2->auFlag, INST_SKIPINV) &&
				psInst1->uDestCount == psInst2->uDestCount
			 )
	   )
	{
		return IMG_FALSE;
	}

	for (uDestIdx = 0; uDestIdx < psInst1->uDestCount; uDestIdx++)
	{
		if (
				!(
					psInst1->auDestMask[uDestIdx] == psInst2->auDestMask[uDestIdx] &&
					psInst1->asDest[uDestIdx].eFmt == psInst2->asDest[uDestIdx].eFmt
				 )
		   )
		{
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL EqualInstructionModes(PINTERMEDIATE_STATE	psState,
							   const INST			*psInst1,
							   const INST			*psInst2)
/*****************************************************************************
 FUNCTION	: EqualInstructionModes

 PURPOSE	: Check if two instructions have the same parameters.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to compare.
			  psInst2

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (!EqualDestinationParameters(psInst1, psInst2))
	{
		return IMG_FALSE;
	}

	return EqualInstructionParameters(psState, psInst1, psInst2);
}

IMG_INTERNAL
IMG_BOOL IsInstAffectedByBRN21752(PCINST psInst)
/*****************************************************************************
 FUNCTION	: IsInstAffectedByBRN21752

 PURPOSE	: Checks if an instruction has restricted destination masks
			  on cores with BRN21752.

 PARAMETERS	: psInst	- Instruction to check.

 RETURNS	: TRUE if the instruction is affected by BRN21752.
*****************************************************************************/
{
	/*
		BRN21752 applies to pack/unpack instructions where:
			(destination = unified store) AND
			(destination format = C10 XOR source format = C10) AND
			(destination mask /= .X111
	*/
	if	(
			(
				(psInst->eOpcode == IPCKC10F16) ||
				(psInst->eOpcode == IPCKC10F32) ||
				(psInst->eOpcode == IUNPCKF16C10)
			) &&
			(psInst->asDest[0].uType != USEASM_REGTYPE_FPINTERNAL)
		)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
IMG_INTERNAL
IMG_UINT32 GetVDUALGPIDestinationMask(const INST	*psInst)
/*****************************************************************************
 FUNCTION	: GetVDUALGPIDestinationMask
    
 PURPOSE	: Get the mask of channels VDUAL will write when the unified store
			  destination is set to a GPI register.

 PARAMETERS	: psInst			- Instruction.

 RETURNS	: The mask of channels written.
*****************************************************************************/
{
	/*
		For a GPI register in the unified store destination the
		channels corresponding to the vector length of the dual-issue
		instruction are written.
	*/
	if (psInst->u.psVec->sDual.bVec3)
	{
		return USC_XYZ_CHAN_MASK;
	}
	else
	{
		return USC_XYZW_CHAN_MASK;
	}
}

IMG_INTERNAL
IMG_BOOL IsVDUALUnifiedStoreVectorSource(PINTERMEDIATE_STATE psState, PCINST psInst)
/*****************************************************************************
 FUNCTION	: IsIsVDUALUnifiedStoreVectorSource
    
 PURPOSE	: Checks if the operation uses the unified store source to a dual
			  issue instruction is scalar.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	VDUAL_OP		eUSOp;
	VDUAL_OP_TYPE	eUSOwner;

	ASSERT(psInst->eOpcode == IVDUAL);
	eUSOwner = psInst->u.psVec->sDual.aeSrcUsage[VDUAL_SRCSLOT_UNIFIEDSTORE];
	if (eUSOwner == VDUAL_OP_TYPE_NONE)
	{
		/* Neither operation uses the unified store source. */
		return IMG_FALSE;
	}
	if (eUSOwner == VDUAL_OP_TYPE_PRIMARY)
	{
		eUSOp = psInst->u.psVec->sDual.ePriOp;
	}
	else
	{
		ASSERT(eUSOwner == VDUAL_OP_TYPE_SECONDARY);
		eUSOp = psInst->u.psVec->sDual.eSecOp;
	}
	return g_asDualIssueOpDesc[eUSOp].bVector;
}

IMG_INTERNAL
IMG_BOOL IsVDUALUnifiedStoreVectorResult(PCINST	psInst)
/*****************************************************************************
 FUNCTION	: IsVDUALUnifiedStoreVectorResult
    
 PURPOSE	: Check if the unified store destination to a vector dual-issue
			  instruction is written with a vector or a scalar result.

 PARAMETERS	: psInst			- Instruction.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	VDUAL_OP	eUSOp;

	if (psInst->u.psVec->sDual.bPriUsesGPIDest)
	{
		eUSOp = psInst->u.psVec->sDual.eSecOp;
	}
	else
	{
		eUSOp = psInst->u.psVec->sDual.ePriOp;
	}
	return g_asDualIssueOpDesc[eUSOp].bVectorResult;
}

IMG_INTERNAL
IMG_UINT32 GetVDUALUnifiedStoreDestinationMask(PINTERMEDIATE_STATE	psState, 
											   PCINST				psInst, 
											   IMG_UINT32			uRealDestMask,
											   IMG_BOOL				bIsGPI)
/*****************************************************************************
 FUNCTION	: GetVDUALGPIDestinationMask
    
 PURPOSE	: Get the mask of channels VDUAL will write when the unified store
			  destination is set to a unified store register.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.
			  uRealDestMask		- Mask of channels which are desired to be written.
			  bIsGPI			- 

 RETURNS	: The mask of channels actually written.
*****************************************************************************/
{
	IMG_UINT32	uUSInstDestSelectShift;
	IMG_BOOL	bVectorResult = IsVDUALUnifiedStoreVectorResult(psInst);
	
	uUSInstDestSelectShift = psInst->u.psVec->uDestSelectShift;
	if (bIsGPI)
	{
		ASSERT(uUSInstDestSelectShift == 0);
	}
	if (psInst->u.psVec->sDual.bF16 && !bIsGPI)
	{
		if (bVectorResult)
		{
			return USC_XYZW_CHAN_MASK;
		}
		else
		{
			ASSERT(psInst->u.psVec->uDestSelectShift == USC_X_CHAN);
			ASSERT((uRealDestMask & USC_ZW_CHAN_MASK) == 0);
			return USC_XY_CHAN_MASK;
		}
	}
	else
	{
		if (bVectorResult)
		{
			ASSERT(uUSInstDestSelectShift == USC_X_CHAN || uUSInstDestSelectShift == USC_Z_CHAN);
		}
		else
		{
			IMG_INT32	iSingleComponent;

			iSingleComponent = g_aiSingleComponent[uRealDestMask];

			if (iSingleComponent >= USC_Z_CHAN)
			{
				ASSERT(uUSInstDestSelectShift == USC_Z_CHAN);
			}
			else
			{
				ASSERT(uUSInstDestSelectShift == USC_X_CHAN);
			}

			uUSInstDestSelectShift = (iSingleComponent == -1) ? USC_X_CHAN: iSingleComponent;
			ASSERT(uUSInstDestSelectShift <= USC_W_CHAN);
		}
		/* Update uDestSelectShift */
		psInst->u.psVec->uDestSelectShift = uUSInstDestSelectShift;
		
		if (bIsGPI)
		{
			ASSERT(psInst->u.psVec->uDestSelectShift == USC_X_CHAN);
			if (psInst->u.psVec->sDual.bVec3)
			{
				return USC_XYZ_CHAN_MASK;
			}
			else
			{
				return USC_XYZW_CHAN_MASK;
			}
		}
		else
		{
			IMG_UINT32	uActualWriteMask;

			if (bVectorResult)
			{	
				ASSERT(psInst->u.psVec->uDestSelectShift == USC_X_CHAN || 
					   psInst->u.psVec->uDestSelectShift == USC_Z_CHAN); 
				uActualWriteMask = USC_XY_CHAN_MASK;
			}
			else
			{
				uActualWriteMask = USC_X_CHAN_MASK;
			}

			uActualWriteMask <<= psInst->u.psVec->uDestSelectShift;
			ASSERT((uRealDestMask & ~uActualWriteMask) == 0);

			return uActualWriteMask;
		}
	}
}

IMG_INTERNAL
IMG_BOOL GetMinimumVDUALDestinationMask(PINTERMEDIATE_STATE	psState,
									    const INST			*psInst,
										IMG_UINT32			uDestSlot,
										const ARG			*psDest,
										IMG_UINT32			uRealDestMask)
/*****************************************************************************
 FUNCTION	: GetMinimumVDUALDestinationMask
    
 PURPOSE	: Given a mask of the data to write in a particular destination
			  register of a dual-issue vector instruction return the minimal mask which
			  would actually be written.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.
			  uDestSlot			- Index of the destination register.
			  psDest			- Destination type.
			  uRealDestMask		- Mask of the channels we want to write.

 RETURNS	: The mask of channels which would actually be written.
*****************************************************************************/
{
	ASSERT((psState->uFlags & USC_FLAGS_POSTC10REGALLOC) != 0);

	if (uDestSlot == VDUAL_DESTSLOT_GPI)
	{
		/*
			All write-masks are supported on the GPI destination.
		*/
		if (uRealDestMask != 0)
		{
			return uRealDestMask;
		}
		else
		{
			return USC_ANY_NONEMPTY_CHAN_MASK;
		}
	}

	ASSERT(uDestSlot == VDUAL_DESTSLOT_UNIFIEDSTORE);

	if (psDest->eFmt == UF_REGFORMAT_F16)
	{
		ASSERT(psDest->uType != USEASM_REGTYPE_FPINTERNAL);
		return GetVDUALUnifiedStoreDestinationMask(psState, psInst, uRealDestMask, IMG_FALSE /* bDestIsGPI */);
	}
	else
	{
		IMG_BOOL	bDestIsGPI;

		ASSERT(psDest->eFmt == UF_REGFORMAT_F32);

		bDestIsGPI = IMG_FALSE;
		if (psDest->uType == USEASM_REGTYPE_FPINTERNAL)
		{
			bDestIsGPI = IMG_TRUE;
		}

		return GetVDUALUnifiedStoreDestinationMask(psState, psInst, uRealDestMask, bDestIsGPI);
	}
}

IMG_UINT32 static GetMinimalVecDestinationMask(PINTERMEDIATE_STATE	psState,
											   const INST				*psInst,
											   IMG_UINT32			uDestIdx,
											   IMG_UINT32			uRealDestMask)
/*****************************************************************************
 FUNCTION	: GetMinimalVecDestinationMask
    
 PURPOSE	: Given a mask of the data to write in a particular destination
			  register of a vector instruction return the minimal mask which
			  would actually be written.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.
			  uDestIdx			- Index of the destination register.
			  uRealDestMask		- Mask of the channels we want to write.

 RETURNS	: The mask of channels which would actually be written.
*****************************************************************************/
{
	PARG		psDest = &psInst->asDest[uDestIdx];
	IMG_UINT32	uMinimalDestMask;

	/*
		Only start enforcing the restriction once we've been through
		the program to fix instructions.
	*/
	if ((psState->uFlags & USC_FLAGS_FIXEDVECTORWRITEMASKS) == 0)
	{
		return uRealDestMask;
	}

	/*
		The TESTMASK instructon doesn't support destination masks at all. All channels
		in the destination are always written.
	*/
	if (psInst->eOpcode == IVTESTMASK ||
		psInst->eOpcode == IVTESTBYTEMASK)
	{
		return USC_XYZW_CHAN_MASK;
	}

	/*
		VDUAL has limited support for write-masks on the unified store destination.
	*/
	if (psInst->eOpcode == IVDUAL)
	{
		if ((psState->uFlags & USC_FLAGS_REPLACEDVECTORREGS) == 0)
		{
			/*
				Get the minimal destination mask in terms of channels.
			*/
			return GetMinimumVDUALDestinationMask(psState, psInst, uDestIdx, psDest, uRealDestMask);
		}
		else
		{
			IMG_UINT32	uMinimalVecMask;
			IMG_UINT32	uDestSlot;
			IMG_UINT32	uDestChan;

			uDestSlot = uDestIdx / 4;
			uDestChan = uDestIdx % 4;

			/*
				Get the minimal destination mask in terms of channels.
			*/
			uMinimalVecMask = GetMinimumVDUALDestinationMask(psState, psInst, uDestSlot, psDest, uRealDestMask);

			/*
				Convert to a minimal mask in terms of bytes.
			*/
			if (psDest->eFmt == UF_REGFORMAT_F16)
			{
				IMG_UINT32	uMinimalRegMask;

				/*
					
				*/
				if (uDestChan >= SCALAR_REGISTERS_PER_F16_VECTOR)
				{
					ASSERT(uRealDestMask == 0);
					return 0;
				}

				/* Get the part of the vector mask corresponding to this scalar destination. */
				uMinimalRegMask = (uMinimalVecMask >> (uDestChan * F16_CHANNELS_PER_SCALAR_REGISTER)) & USC_XY_CHAN_MASK;

				if (uMinimalRegMask == USC_XY_CHAN_MASK)
				{
					return USC_XYZW_CHAN_MASK;
				}
				else
				{
					ASSERT(uMinimalRegMask == 0);
					return 0;
				}
			}
			else
			{
				ASSERT(psDest->eFmt == UF_REGFORMAT_F32);

				if ((uMinimalVecMask & (1 << uDestChan)) != 0)
				{
					return USC_XYZW_CHAN_MASK;
				}
				else
				{
					return 0;
				}
			}
		}
	}

	/*
		Check for instructions which only support destination masks with
		register granularity.
	*/
	if (!(psInst->eOpcode == IVMAD || psInst->eOpcode == IVMOV_EXP || psInst->eOpcode == IVMOVC || psInst->eOpcode == IVMOVCU8_FLT))
	{
		return uRealDestMask;
	}
	/*
		Limitations only apply when the channels of the destination
		format are less than 32-bits.
	*/
	if (psDest->eFmt != UF_REGFORMAT_F16)
	{
		return uRealDestMask;
	}
	/*
		Limitations only apply when writing unified store registers.
	*/
	if (psDest->uType == USEASM_REGTYPE_FPINTERNAL)
	{
		return uRealDestMask;
	}

	uMinimalDestMask = 0;
	if ((psState->uFlags & USC_FLAGS_REPLACEDVECTORREGS) == 0)
	{
		/*
			The mask is in terms of channels.
		*/

		ASSERT(uDestIdx == 0);
		/*
			If either the X or Y channels are written then both X and Y
			must be written.
		*/
		if ((uRealDestMask & USC_XY_CHAN_MASK) != 0)
		{
			uMinimalDestMask |= USC_XY_CHAN_MASK;
		}
		/*
			Similarly for Z and W.
		*/
		if ((uRealDestMask & USC_ZW_CHAN_MASK) != 0)
		{
			uMinimalDestMask |= USC_ZW_CHAN_MASK;
		}
	}
	else
	{
		/*
			The mask is in terms of bytes.
		*/

		ASSERT(uDestIdx <= 1);
		/*
			The destination mask has 32-bit granularity only.
		*/
		if (uRealDestMask != 0)
		{
			uMinimalDestMask = USC_XYZW_CHAN_MASK;
		}
	}

	return uMinimalDestMask;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

IMG_INTERNAL
IMG_BOOL IsMaskableRegisterType(IMG_UINT32 uType)
/*****************************************************************************
 FUNCTION	: IsMaskableRegisterType
    
 PURPOSE	: Check for a register type where the hardware doesn't support
			  destination write masks (the entire register is always written).

 PARAMETERS	: uType		- Register type to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (uType == USEASM_REGTYPE_INDEX)
	{
		return IMG_FALSE;
	}
	else
	{
		return IMG_TRUE;
	}
}

IMG_INTERNAL
IMG_UINT32 GetMinimalDestinationMask(PINTERMEDIATE_STATE	psState,
									 PCINST					psInst,
									 IMG_UINT32				uDestIdx,
									 IMG_UINT32				uRealDestMask)
/*****************************************************************************
 FUNCTION	: GetMinimalDestinationMask
    
 PURPOSE	: Given a mask of the data to write in a particular destination
			  register of an instruction return the minimal mask which
			  would actually be written.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.
			  uDestIdx			- Index of the destination register.
			  uRealDestMask		- Mask of the channels we want to write.

 RETURNS	: The mask of channels which would actually be written.
*****************************************************************************/
{
	IMG_BOOL	b8BitDestPack;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORDEST)
	{
		return GetMinimalVecDestinationMask(psState, psInst, uDestIdx, uRealDestMask);
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		For SGX543 when writing a 64-bit register with C10 format data even
		instructions with destination masks won't preserve the upper 24-bits.
	*/
	if (
			(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_C10VECTORDEST) != 0 &&
			uDestIdx == 1
	   )
	{
		ASSERT(psInst->asDest[1].eFmt == UF_REGFORMAT_C10);
		if	(g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_DESTMASKABLE)
		{
			if (uRealDestMask & USC_RED_CHAN_MASK)
			{
				return USC_ALL_CHAN_MASK;
			}
			else
			{
				ASSERT((psState->uFlags2 & USC_FLAGS2_ASSIGNED_PRIMARY_REGNUMS) != 0 || uRealDestMask == 0);
				return USC_GBA_CHAN_MASK;
			}
		}
	}

	/*
		VPCKC10C10/VPCKU8U8 don't support a mask with 3 bits set.
	*/
	if ((psInst->eOpcode == IVPCKC10C10 || psInst->eOpcode == IVPCKU8U8) && g_auSetBitCount[uRealDestMask] == 3)
	{
		return USC_XYZW_CHAN_MASK;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		The hardware doesn't support partial updates of some register types even if the instruction
		otherwise supports a destination write mask.
	*/
	if (!IsMaskableRegisterType(psInst->asDest[uDestIdx].uType))
	{
		return (uRealDestMask != 0) ? USC_ALL_CHAN_MASK : 0;
	}

	/*
		BRN21752 applies to pack/unpack instructions where:
			(destination = unified store) AND
			(destination format = C10 XOR source format = C10) AND
			(destination mask /= .X111
	*/
	if ((psState->uFlags & USC_FLAGS_APPLIEDBRN21752_FIX) != 0 && IsInstAffectedByBRN21752(psInst))
	{
		ASSERT(uDestIdx == 0);

		/*
			If the instruction writes any channels from XYZ then it will
			write all of XYZ.
		*/
		if ((uRealDestMask & USC_XYZ_CHAN_MASK) != 0)
		{
			return uRealDestMask | USC_XYZ_CHAN_MASK;
		}
	}

	/*
		For PCK instructions with an 8-bit destination format only 1, 2 or 4 bytes can
		be written.
	*/
	b8BitDestPack = IMG_FALSE;
	if (psInst->eOpcode == IPCKC10F16 ||
		psInst->eOpcode == IPCKC10F32 ||
		psInst->eOpcode == IPCKC10C10 ||
		psInst->eOpcode == IPCKU8F16 ||
		psInst->eOpcode == IPCKU8F32 ||
		psInst->eOpcode == IPCKU8U8)
	{
		b8BitDestPack = IMG_TRUE;
	}
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psInst->eOpcode == IVPCKU8FLT && !psInst->u.psVec->bPackScale)
	{
		b8BitDestPack = IMG_TRUE;
	}
	if (psInst->eOpcode == IVPCKS8FLT)
	{
		b8BitDestPack = IMG_TRUE;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	if (b8BitDestPack)
	{
		if (g_auSetBitCount[uRealDestMask] > 2)
		{
			return USC_XYZW_CHAN_MASK;
		}
	}

	/*
		For PCK instructions with a 16-bit destination format only whole words can be
		masked.
	*/
	if (
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			psInst->eOpcode == IVPCKU16FLT ||
			psInst->eOpcode == IVPCKU16FLT_EXP ||
			psInst->eOpcode == IVPCKU16U16 ||
			psInst->eOpcode == IVPCKU16U8 ||
			psInst->eOpcode == IVPCKS16FLT ||
			psInst->eOpcode == IVPCKS16FLT_EXP ||
			psInst->eOpcode == IVPCKS16S8 ||
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
			psInst->eOpcode == IUNPCKU16U16 ||
			psInst->eOpcode == IUNPCKU16U8 ||
			psInst->eOpcode == IUNPCKU16S8 ||
			psInst->eOpcode == IUNPCKU16F16 ||
			psInst->eOpcode == IPCKU16F32 ||
			psInst->eOpcode == IUNPCKS16U8 ||
			psInst->eOpcode == IPCKS16F32
		)
	{
		IMG_UINT32 uMask = 0;

		if ((uRealDestMask & USC_XY_CHAN_MASK) != 0)
		{
			uMask |= USC_XY_CHAN_MASK;
		}
		if ((uRealDestMask & USC_ZW_CHAN_MASK) != 0)
		{
			uMask |= USC_ZW_CHAN_MASK;
		}

		return uMask;
	}

	/*
		For SOPWM using the alpha channel special case the whole destination
		register is always written.
	*/
	if (
			psInst->eOpcode == ISOPWM &&
			psInst->u.psSopWm->bRedChanFromAlpha
	   )
	{
		return USC_ALL_CHAN_MASK;
	}

	/*
		The implicit internal register destinations for an EFO can only be either
		completely enabled or completely disabled.
	*/
	if (psInst->eOpcode == IEFO && (uDestIdx == EFO_I0_DEST || uDestIdx == EFO_I1_DEST))
	{
		return (uRealDestMask != 0) ? USC_ALL_CHAN_MASK : 0;
	}

	/*
		Otherwise if the instruction doesn't support a destination write mask then
		it will always write all channels in the destination.
	*/
	if	(g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_DESTMASKABLE)
	{
		return uRealDestMask;
	}
	else
	{
		return USC_XYZW_CHAN_MASK;
	}
}

IMG_INTERNAL
IMG_VOID ModifyOpcode(PINTERMEDIATE_STATE	psState,
					  PINST					psInst,
					  IOPCODE				eNewOpcode)
/*****************************************************************************
 FUNCTION	: ModifyOpcode
    
 PURPOSE	: Switch between opcodes with the same instruction specific data
			  preserving that data.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.
			  eNewOpcode		- Opcode to change to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		Shrink or grow the array of arguments.
	*/
	SetArgumentCount(psState, psInst, g_psInstDesc[eNewOpcode].uDefaultArgumentCount);

	/*
		Remove from the list associated with the old opcode.
	*/
	SafeListRemoveItem(&psState->asOpcodeLists[psInst->eOpcode], &psInst->sOpcodeListEntry);

	/*
		Add to the list associated with the new opcode.
	*/
	SafeListAppendItem(&psState->asOpcodeLists[eNewOpcode], &psInst->sOpcodeListEntry);

	/*
		Set the new opcode.
	*/
	ASSERT(g_psInstDesc[psInst->eOpcode].eType == g_psInstDesc[eNewOpcode].eType);
	psInst->eOpcode = eNewOpcode;
}

IMG_INTERNAL
IMG_VOID MoveFloatSrc(PINTERMEDIATE_STATE	psState, 
					  PINST					psDestInst, 
					  IMG_UINT32			uDestArgIdx, 
					  PINST					psSrcInst, 
					  IMG_UINT32			uSrcArgIdx)
/*****************************************************************************
 FUNCTION	: MoveFloatSrc
    
 PURPOSE	: Move a source argument and associated source modifiers from
			  one floating point instruction to another.

 PARAMETERS	: psState			- Compiler state.
			  psDestInst		- Instruction to move the argument to.
			  uDestArgIdx		- Index of the source to move the argument to.
			  psSrcInst			- Instruction to move the argument from.
			  uSrcArgIdx		- Index of the source to move the argument from.

 RETURNS	: Nothing.
*****************************************************************************/
{
	INST_TYPE	eDestType;

	eDestType = g_psInstDesc[psDestInst->eOpcode].eType;
	ASSERT(eDestType == INST_TYPE_FLOAT || eDestType == INST_TYPE_FARITH16);

	MoveSrcAndModifiers(psState, psDestInst, uDestArgIdx, psSrcInst, uSrcArgIdx);
}

static
IMG_VOID CopySrcModifiers(PINTERMEDIATE_STATE	psState, 
						  PINST					psDestInst, 
						  IMG_UINT32			uDestArgIdx, 
						  PINST					psSrcInst, 
						  IMG_UINT32			uSrcArgIdx)
/*****************************************************************************
 FUNCTION	: CopySrcModifiers
    
 PURPOSE	: Copy source modifiers from one instruction to another.

 PARAMETERS	: psState			- Compiler state.
			  psDestInst		- Instruction to move the argument to.
			  uDestArgIdx		- Index of the source to move the argument to.
			  psSrcInst			- Instruction to move the argument from.
			  uSrcArgIdx		- Index of the source to move the argument from.

 RETURNS	: Nothing.
*****************************************************************************/
{
	INST_TYPE	eDestType;

	ASSERT(uDestArgIdx < psDestInst->uArgumentCount);
	ASSERT(uSrcArgIdx < psSrcInst->uArgumentCount);

	eDestType = g_psInstDesc[psDestInst->eOpcode].eType;
	ASSERT(eDestType == g_psInstDesc[psSrcInst->eOpcode].eType);

	switch (eDestType)
	{
		case INST_TYPE_FLOAT:
		{
			psDestInst->u.psFloat->asSrcMod[uDestArgIdx] = psSrcInst->u.psFloat->asSrcMod[uSrcArgIdx];
			break;
		}
		case INST_TYPE_FARITH16:
		{
			psDestInst->u.psArith16->sFloat.asSrcMod[uDestArgIdx] = psSrcInst->u.psArith16->sFloat.asSrcMod[uSrcArgIdx];
			psDestInst->u.psArith16->aeSwizzle[uDestArgIdx] = psSrcInst->u.psArith16->aeSwizzle[uSrcArgIdx];
			break;
		}
		case INST_TYPE_DPC:
		{
			psDestInst->u.psDpc->sFloat.asSrcMod[uDestArgIdx] = psSrcInst->u.psDpc->sFloat.asSrcMod[uSrcArgIdx];
			break;
		}
		#if defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case INST_TYPE_IMA32:
		{
			psDestInst->u.psIma32->abNegate[uDestArgIdx] = psSrcInst->u.psIma32->abNegate[uSrcArgIdx];
			break;
		}
		#endif /* defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		case INST_TYPE_IMAE:
		{
			ASSERT(uDestArgIdx < IMAE_UNIFIED_STORE_SOURCE_COUNT);
			ASSERT(uSrcArgIdx < IMAE_UNIFIED_STORE_SOURCE_COUNT);
			psDestInst->u.psImae->auSrcComponent[uDestArgIdx] = psSrcInst->u.psImae->auSrcComponent[uSrcArgIdx];
			break;
		}
		case INST_TYPE_TEST:
		{
			ASSERT(uDestArgIdx < TEST_ALUOPCODE_MAXIMUM_ARGUMENT_COUNT);
			ASSERT(uSrcArgIdx < TEST_ALUOPCODE_MAXIMUM_ARGUMENT_COUNT);
			psDestInst->u.psTest->auSrcComponent[uDestArgIdx] = psSrcInst->u.psTest->auSrcComponent[uSrcArgIdx];
			break;
		}
		case INST_TYPE_PCK:
		{
			SetPCKComponent(psState, psDestInst, uDestArgIdx, GetPCKComponent(psState, psSrcInst, uSrcArgIdx));
			break;
		}
		default:
		{
			break;
		}
	}
}

IMG_INTERNAL
IMG_VOID CopySrcAndModifiers(PINTERMEDIATE_STATE	psState, 
							 PINST					psDestInst, 
							 IMG_UINT32				uDestArgIdx, 
							 PINST					psSrcInst, 
							 IMG_UINT32				uSrcArgIdx)
/*****************************************************************************
 FUNCTION	: CopySrcAndModifiers
    
 PURPOSE	: Copy a source argument and associated source modifiers from
			  one instruction to another.

 PARAMETERS	: psState			- Compiler state.
			  psDestInst		- Instruction to move the argument to.
			  uDestArgIdx		- Index of the source to move the argument to.
			  psSrcInst			- Instruction to move the argument from.
			  uSrcArgIdx		- Index of the source to move the argument from.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(uDestArgIdx < psDestInst->uArgumentCount);
	ASSERT(uSrcArgIdx < psSrcInst->uArgumentCount);

	SetSrcFromArg(psState, psDestInst, uDestArgIdx, &psSrcInst->asArg[uSrcArgIdx]);
	CopySrcModifiers(psState, psDestInst, uDestArgIdx, psSrcInst, uSrcArgIdx);
}

IMG_INTERNAL
IMG_VOID MoveSrcAndModifiers(PINTERMEDIATE_STATE	psState, 
							 PINST					psDestInst, 
							 IMG_UINT32				uDestArgIdx, 
							 PINST					psSrcInst, 
							 IMG_UINT32				uSrcArgIdx)
/*****************************************************************************
 FUNCTION	: MoveSrcAndModifiers
    
 PURPOSE	: Move a source argument and associated source modifiers from
			  one instruction to another.

 PARAMETERS	: psState			- Compiler state.
			  psDestInst		- Instruction to move the argument to.
			  uDestArgIdx		- Index of the source to move the argument to.
			  psSrcInst			- Instruction to move the argument from.
			  uSrcArgIdx		- Index of the source to move the argument from.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(uDestArgIdx < psDestInst->uArgumentCount);
	ASSERT(uSrcArgIdx < psSrcInst->uArgumentCount);

	MoveSrc(psState, psDestInst, uDestArgIdx, psSrcInst, uSrcArgIdx);
	CopySrcModifiers(psState, psDestInst, uDestArgIdx, psSrcInst, uSrcArgIdx);
}

static
IMG_VOID SwapFloatSourceModifiers(PFLOAT_PARAMS	psFloat, IMG_UINT32 uSrc1Idx, IMG_UINT32 uSrc2Idx)
/*****************************************************************************
 FUNCTION	: SwapFloatSourceModifiers
    
 PURPOSE	: Swap the floating point source modifiers applied to two arguments.

 PARAMETERS	: psFloat		- Floating point source modifiers applied to the
							source argument.
			  uSrc1Idx		- Indices of the two arguments to swap.
			  uSrc2Idx

 RETURNS	: Nothing.
*****************************************************************************/
{
	FLOAT_SOURCE_MODIFIER	eTempMod;
	
	eTempMod = psFloat->asSrcMod[uSrc1Idx];
	psFloat->asSrcMod[uSrc1Idx] = psFloat->asSrcMod[uSrc2Idx];
	psFloat->asSrcMod[uSrc2Idx] = eTempMod;
}

IMG_INTERNAL
IMG_VOID ExchangeInstSources(PINTERMEDIATE_STATE	psState, 
							 PINST					psInst1, 
							 IMG_UINT32				uSrc1Idx, 
							 PINST					psInst2, 
							 IMG_UINT32				uSrc2Idx)
/*****************************************************************************
 FUNCTION	: ExchangeInstSources
    
 PURPOSE	: Swap two source arguments between two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- First instruction.
			  uSrc1Idx			- Index of the argument to the first instruction to swap.
			  psInst2			- Second instruction.
			  uSrc2Idx			- Index of the argument to the second instruction to swap.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ARG		sTemp;

	ASSERT(uSrc1Idx < psInst1->uArgumentCount);
	ASSERT(uSrc2Idx < psInst2->uArgumentCount);

	sTemp = psInst1->asArg[uSrc1Idx];
	SetSrcFromArg(psState, psInst1, uSrc1Idx, &psInst2->asArg[uSrc2Idx]);
	SetSrcFromArg(psState, psInst2, uSrc2Idx, &sTemp);
}

IMG_INTERNAL
IMG_VOID SwapInstSources(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSrc1Idx, IMG_UINT32 uSrc2Idx)
/*****************************************************************************
 FUNCTION	: SwapInstSources
    
 PURPOSE	: Swap two source arguments to an instruction and their associated
			  source modifiers.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction whose arguments are to be swapped.
			  uSrc1Idx			- Index of the first argument to swap.
			  uSrc2Idx			- Index of the second argument to swap.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(uSrc1Idx < psInst->uArgumentCount);
	ASSERT(uSrc2Idx < psInst->uArgumentCount);

	ExchangeInstSources(psState, psInst, uSrc1Idx, psInst, uSrc2Idx);

	/*
		Swap any extra per-argument data in the instruction.
	*/
	switch (g_psInstDesc[psInst->eOpcode].eType)
	{
		case INST_TYPE_FARITH16:	
		{
			FMAD16_SWIZZLE			eTempSwizzle;
			PFARITH16_PARAMS		psParams;

			psParams = psInst->u.psArith16;

			eTempSwizzle = psParams->aeSwizzle[uSrc1Idx];
			psParams->aeSwizzle[uSrc1Idx] = psParams->aeSwizzle[uSrc2Idx];
			psParams->aeSwizzle[uSrc2Idx] = eTempSwizzle;

			SwapFloatSourceModifiers(&psParams->sFloat, uSrc1Idx, uSrc2Idx);
			break;
		}
		case INST_TYPE_FLOAT:
		{
			SwapFloatSourceModifiers(psInst->u.psFloat, uSrc1Idx, uSrc2Idx);
			break;
		}
		case INST_TYPE_DPC:
		{
			SwapFloatSourceModifiers(&psInst->u.psDpc->sFloat, uSrc1Idx, uSrc2Idx);
			break;
		}
		case INST_TYPE_EFO:
		{
			SwapFloatSourceModifiers(&psInst->u.psEfo->sFloat, uSrc1Idx, uSrc2Idx);
			break;
		}
		#if defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case INST_TYPE_IMA32:
		{
			IMG_BOOL		bTempNegate;
			PIMA32_PARAMS	psIma32;

			psIma32 = psInst->u.psIma32;

			bTempNegate = psIma32->abNegate[uSrc1Idx];
			psIma32->abNegate[uSrc1Idx] = psIma32->abNegate[uSrc2Idx];
			psIma32->abNegate[uSrc2Idx] = bTempNegate;
			break;
		}
		#endif /* defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		case INST_TYPE_TEST:
		{
			PTEST_PARAMS	psTest = psInst->u.psTest;
			IMG_UINT32		uTempSrcComponent;

			uTempSrcComponent = psTest->auSrcComponent[uSrc1Idx];
			psTest->auSrcComponent[uSrc1Idx] = psTest->auSrcComponent[uSrc2Idx];
			psTest->auSrcComponent[uSrc2Idx] = uTempSrcComponent;
			break;
		}
		case INST_TYPE_IMAE:
		{
			IMG_UINT32	uTempSrcComponent;

			uTempSrcComponent = psInst->u.psImae->auSrcComponent[uSrc1Idx];
			psInst->u.psImae->auSrcComponent[uSrc1Idx] = psInst->u.psImae->auSrcComponent[uSrc2Idx];
			psInst->u.psImae->auSrcComponent[uSrc2Idx] = uTempSrcComponent;
			break;
		}
		#if defined(SUPPORT_SGX545)
		case INST_TYPE_DUAL:
		{
			PDUAL_PARAMS	psDual = psInst->u.psDual;
			IMG_BOOL		bTempNegate;
			IMG_UINT32		uTempComponent;

			bTempNegate = psDual->abPrimarySourceNegate[uSrc1Idx];
			uTempComponent = psDual->auSourceComponent[uSrc1Idx];

			psDual->abPrimarySourceNegate[uSrc1Idx] = psDual->abPrimarySourceNegate[uSrc2Idx];
			psDual->auSourceComponent[uSrc1Idx] = psDual->auSourceComponent[uSrc2Idx];

			psDual->abPrimarySourceNegate[uSrc2Idx] = bTempNegate;
			psDual->auSourceComponent[uSrc2Idx] = uTempComponent;
			break;
		}
		#endif /* defined(SUPPORT_SGX545) */
		default:
		{
			break;
		}
	}
}		

IMG_INTERNAL
IMG_BOOL InstSource01Commute(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: InstSource01Commute
    
 PURPOSE	: Check for an instruction where the first two sources commute.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IOPCODE						eOpcode;
	PCSOURCE_ARGUMENT_PAIR		psCommutableSources;

	ASSERT(psInst);
	eOpcode				= psInst->eOpcode;
	
	if (g_psInstDesc[eOpcode].eType == INST_TYPE_TEST)
	{
		eOpcode = psInst->u.psTest->eAluOpcode;
	}

	psCommutableSources = g_psInstDesc[eOpcode].psCommutableSources;

	if (g_psInstDesc[eOpcode].eType == INST_TYPE_BITWISE && psInst->u.psBitwise->bInvertSrc2)
	{
		return IMG_FALSE;
	}
	if (psCommutableSources == NULL)
	{
		return IMG_FALSE;
	}
	if (!(psCommutableSources->uFirstSource == 0 && psCommutableSources->uSecondSource == 1))
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_VOID CommuteSrc01(PINTERMEDIATE_STATE psState, PINST	psInst)
/*****************************************************************************
 FUNCTION	: CommuteSrc01
    
 PURPOSE	: Swap the first two sources to an instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(InstSource01Commute(psState, psInst));
	SwapInstSources(psState, psInst, 0 /* uSrc1Idx */, 1 /* uSrc2Idx */);	
}

IMG_INTERNAL
IMG_BOOL EqualFloatSrcsIgnoreNegate(PINTERMEDIATE_STATE psState,
								    const INST				*psInst1, 
									IMG_UINT32			uInst1SrcIdx, 
									const INST				*psInst2, 
									IMG_UINT32			uInst2SrcIdx,
									IMG_PBOOL			pbDifferentNegate)
/*****************************************************************************
 FUNCTION	: EqualFloatSrcsIgnoreNegate
    
 PURPOSE	: Check if two sources to floating point instructions refer to the
			  same register and have the same source modifier apart from negate.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- First instruction whose argument is to be
								compared.
			  uInst1SrcIdx		- Index of the first argument to be compared.
			  psInst2			- Second instruction whose argument is to be
								compared.
			  uInst2SrcIdx		- Index of the second argument to be compared.
			  pbDifferentNegate	- Returns TRUE if the negate modifier differs
								on the two arguments.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PARG					psArg1 = &psInst1->asArg[uInst1SrcIdx];
	PFLOAT_SOURCE_MODIFIER	psMod1 = GetFloatMod(psState, psInst1, uInst1SrcIdx);
	PARG					psArg2 = &psInst2->asArg[uInst2SrcIdx];
	PFLOAT_SOURCE_MODIFIER	psMod2 = GetFloatMod(psState, psInst2, uInst2SrcIdx);

	if (!EqualArgs(psArg1, psArg2))
	{
		return IMG_FALSE;
	}
	if (psMod1->uComponent != psMod2->uComponent)
	{
		return IMG_FALSE;
	}
	if (pbDifferentNegate != NULL)
	{
		*pbDifferentNegate = psMod1->bNegate != psMod2->bNegate ? IMG_TRUE : IMG_FALSE;
	}
	if (psMod1->bAbsolute != psMod2->bAbsolute)
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL EqualFloatModifier(PINTERMEDIATE_STATE	psState,
							const INST				*psInst1, 
							IMG_UINT32			uInst1SrcIdx, 
							const INST				*psInst2, 
							IMG_UINT32			uInst2SrcIdx)
/*****************************************************************************
 FUNCTION	: EqualFloatModifier
    
 PURPOSE	: Check if two sources to floating point instructions have the 
			  same source modifier.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- First instruction whose argument is to be
								compared.
			  uInst1SrcIdx		- Index of the first argument to be compared.
			  psInst2			- Second instruction whose argument is to be
								compared.
			  uInst2SrcIdx		- Index of the second argument to be compared.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PFLOAT_SOURCE_MODIFIER	psMod1;
	PFLOAT_SOURCE_MODIFIER	psMod2;

	psMod1 = GetFloatMod(psState, psInst1, uInst1SrcIdx);
	psMod2 = GetFloatMod(psState, psInst2, uInst2SrcIdx);
	
	if (psMod1->bNegate != psMod2->bNegate)
	{
		return IMG_FALSE;
	}
	if (psMod1->bAbsolute != psMod2->bAbsolute)
	{
		return IMG_FALSE;
	}
	if (psMod1->uComponent != psMod2->uComponent)
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL EqualFloatSrcs(PINTERMEDIATE_STATE	psState,
						const INST				*psInst1, 
					    IMG_UINT32			uInst1SrcIdx, 
						const INST				*psInst2, 
						IMG_UINT32			uInst2SrcIdx)
/*****************************************************************************
 FUNCTION	: EqualFloatSrcs
    
 PURPOSE	: Check if two sources to floating point instructions refer to the
			  same register and have the same source modifier.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- First instruction whose argument is to be
								compared.
			  uInst1SrcIdx		- Index of the first argument to be compared.
			  psInst2			- Second instruction whose argument is to be
								compared.
			  uInst2SrcIdx		- Index of the second argument to be compared.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PARG					psArg1;
	PARG					psArg2;
	
	ASSERT(uInst1SrcIdx < psInst1->uArgumentCount);
	ASSERT(uInst2SrcIdx < psInst2->uArgumentCount);

	psArg1 = &psInst1->asArg[uInst1SrcIdx];
	psArg2 = &psInst2->asArg[uInst2SrcIdx];

	if (!EqualArgs(psArg1, psArg2))
	{
		return IMG_FALSE;
	}
	if (!EqualFloatModifier(psState, psInst1, uInst1SrcIdx, psInst2, uInst2SrcIdx))
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

IMG_INTERNAL
PFLOAT_SOURCE_MODIFIER GetFloatMod(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArgIdx)
/*****************************************************************************
 FUNCTION	: GetFloatMod
    
 PURPOSE	: Returns a pointer to the floating point source modifier for a 
			  particular instruction argument or NULL if the instruction doesn'ts
			  support floating point source modifier.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.
			  uArgIdx			- Index of the argument to get the source modifier
								for.

 RETURNS	: A pointer to the source modifier.
*****************************************************************************/
{
	DBG_ASSERT(uArgIdx < psInst->uArgumentCount);
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	switch (g_psInstDesc[psInst->eOpcode].eType)
	{
		case INST_TYPE_FLOAT:
		{
			return &psInst->u.psFloat->asSrcMod[uArgIdx];
		}
		case INST_TYPE_FARITH16:
		{
			return &psInst->u.psArith16->sFloat.asSrcMod[uArgIdx];
		}
		case INST_TYPE_DPC:
		{
			return &psInst->u.psDpc->sFloat.asSrcMod[uArgIdx];
		}
		case INST_TYPE_EFO:
		{
			if (uArgIdx < EFO_HARDWARE_SOURCE_COUNT)
			{
				return &psInst->u.psEfo->sFloat.asSrcMod[uArgIdx];
			}
			else
			{
				return NULL;
			}
		}
		default:
		{
			return NULL;
		}
	}
}

IMG_INTERNAL
IMG_BOOL IsNegated(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArgIdx)
/*****************************************************************************
 FUNCTION	: IsNegated
    
 PURPOSE	: Check if a particular instruction argument has a negate floating
			  point source modifier.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction whose argument is to be checked.
			  uArgIdx			- Index of the argument to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PFLOAT_SOURCE_MODIFIER	psMod = GetFloatMod(psState, psInst, uArgIdx);

	if (psMod != NULL)
	{
		return psMod->bNegate;
	}
	else
	{
		return IMG_FALSE;
	}
}

IMG_INTERNAL
IMG_BOOL IsAbsolute(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArgIdx)
/*****************************************************************************
 FUNCTION	: IsAbsolute
    
 PURPOSE	: Check if a particular instruction argument has a absolute floating
			  point source modifier.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction whose argument is to be checked.
			  uArgIdx			- Index of the argument to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PFLOAT_SOURCE_MODIFIER	psMod = GetFloatMod(psState, psInst, uArgIdx);

	if (psMod != NULL)
	{
		return psMod->bAbsolute;
	}
	else
	{
		return IMG_FALSE;
	}
}

IMG_INTERNAL
IMG_BOOL HasNegateOrAbsoluteModifier(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArgIdx)
/*****************************************************************************
 FUNCTION	: HasNegateOrAbsoluteModifier
    
 PURPOSE	: Check if a particular instruction argument has either a negate or
			  an absolute floating point source modifier.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction whose argument is to be checked.
			  uArgIdx			- Index of the argument to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PFLOAT_SOURCE_MODIFIER	psMod = GetFloatMod(psState, psInst, uArgIdx);

	ASSERT(uArgIdx < psInst->uArgumentCount);

	if (psMod != NULL)
	{
		return (psMod->bNegate || psMod->bAbsolute) ? IMG_TRUE : IMG_FALSE;
	}
	else
	{
		return IMG_FALSE;
	}
}

IMG_INTERNAL
IMG_VOID GetCombinedSourceModifiers(PFLOAT_SOURCE_MODIFIER psMod1, PFLOAT_SOURCE_MODIFIER psMod2)
/*****************************************************************************
 FUNCTION	: GetCombinedSourceModifiers

 PURPOSE	: Calculate the result of applying two sets of source modifiers
			  in series.

 PARAMETERS	: psMod1		- First set of source modifiers to apply.
							On output: the combined source modifiers.
			  psMod2		- Second set of source modifiers to apply.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (!psMod1->bAbsolute)
	{
		psMod1->bAbsolute = psMod2->bAbsolute;
		if (psMod2->bNegate)
		{
			psMod1->bNegate = (!psMod1->bNegate) ? IMG_TRUE : IMG_FALSE;
		}
	}
}

IMG_INTERNAL
IMG_VOID CombineInstSourceModifiers(PINTERMEDIATE_STATE psState, 
									PINST psInst, 
									IMG_UINT32 uArg, 
									PFLOAT_SOURCE_MODIFIER psNewSrcMod)
/*****************************************************************************
 FUNCTION	: CombineInstSourceModifiers
    
 PURPOSE	: Update the floating point source modifiers on an instruction
			  argument to the combined effect of first applying a new
			  set of modifiers then the existing modifiers.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction whose argument is to be updated.
			  uArg				- Index of the argument to update.
			  psNewSrcMod		- New source modifiers.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PFLOAT_SOURCE_MODIFIER	psExistingSourceMod;

	psExistingSourceMod = GetFloatMod(psState, psInst, uArg);
	ASSERT(psExistingSourceMod != NULL);

	GetCombinedSourceModifiers(psExistingSourceMod, psNewSrcMod);
}

IMG_INTERNAL
IMG_VOID InvertNegateModifier(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uArgIdx)
/*****************************************************************************
 FUNCTION	: InvertNegateModifier

 PURPOSE	: Invert the negate source modifier on a source to a floating
			  point instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction whose argument is to be modified.
			  uArgIdx			- Index of the argument to invert the negate
								source modifier on.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PFLOAT_SOURCE_MODIFIER	psMod;

	psMod = GetFloatMod(psState, psInst, uArgIdx);
	psMod->bNegate = !psMod->bNegate;
}

IMG_INTERNAL
IMG_VOID MovePackSource(PINTERMEDIATE_STATE	psState,
					    PINST				psDestInst,
						IMG_UINT32			uDestArgIdx,
						PINST				psSrcInst,
						IMG_UINT32			uSrcArgIdx)
/*****************************************************************************
 FUNCTION	: MovePackSource

 PURPOSE	: Move a source to a PCK instruction and associated source modifiers
			  from one instruction to another.

 PARAMETERS	: psState			- Compiler state.
			  psDestInst		- Instruction to move to.
			  uDestArgIdx		- Source to move to.
			  psSrcInst			- Instruction to move from.
			  uArgArgIdx		- Source to move from.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(uDestArgIdx < psDestInst->uArgumentCount);
	ASSERT(uSrcArgIdx < psSrcInst->uArgumentCount);

	MoveSrc(psState, psDestInst, uDestArgIdx, psSrcInst, uSrcArgIdx);
	SetPCKComponent(psState, psDestInst, uDestArgIdx, GetPCKComponent(psState, psSrcInst, uSrcArgIdx));
}

IMG_INTERNAL
IMG_VOID SwapPCKSources(PINTERMEDIATE_STATE psState, PINST psPCKInst)
/*****************************************************************************
 FUNCTION	: SwapPCKSources

 PURPOSE	: Swaps the order of the two source arguments to a PCK instruction.

 PARAMETERS	: psState			- Compiler state.
			  psPCKInst			- PCK instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ARG			sTemp;
	IMG_UINT32	uTempComponent;

	ASSERT(g_psInstDesc[psPCKInst->eOpcode].eType == INST_TYPE_PCK);

	/*
		Swap the source registers.
	*/
	sTemp = psPCKInst->asArg[0];
	SetSrcFromArg(psState, psPCKInst, 0 /* uSrcIdx */, &psPCKInst->asArg[1]);
	SetSrcFromArg(psState, psPCKInst, 1 /* uSrcIdx */, &sTemp);

	/*
		Swap the source component select.
	*/
	uTempComponent = psPCKInst->u.psPck->auComponent[0];
	psPCKInst->u.psPck->auComponent[0] = psPCKInst->u.psPck->auComponent[1];
	psPCKInst->u.psPck->auComponent[1] = uTempComponent;
}

IMG_INTERNAL
IMG_VOID SetPCKComponent(PINTERMEDIATE_STATE psState, PINST psPCKInst, IMG_UINT32 uArgIdx, IMG_UINT32 uComponent)
/*****************************************************************************
 FUNCTION	: SetPCKComponent

 PURPOSE	: Sets the component select on a PCK source argument.

 PARAMETERS	: psState			- Compiler state.
			  psPCKInst			- PCK instruction.
			  uArgIdx			- Index of the source to set the component
								select for.
			  uComponent		- Component select to set.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(g_psInstDesc[psPCKInst->eOpcode].eType == INST_TYPE_PCK);
	ASSERT(uArgIdx < PCK_SOURCE_COUNT);

	psPCKInst->u.psPck->auComponent[uArgIdx] = uComponent;
}

IMG_INTERNAL
IMG_UINT32 GetPCKComponent(PINTERMEDIATE_STATE psState, PCINST psPCKInst, IMG_UINT32 uArgIdx)
/*****************************************************************************
 FUNCTION	: GetPCKComponent

 PURPOSE	: Gets the component select on a PCK source argument.

 PARAMETERS	: psState			- Compiler state.
			  psPCKInst			- PCK instruction.
			  uArgIdx			- Index of the source to get the component
								select for.

 RETURNS	: The component select.
*****************************************************************************/
{
	ASSERT(g_psInstDesc[psPCKInst->eOpcode].eType == INST_TYPE_PCK);
	ASSERT(uArgIdx < PCK_SOURCE_COUNT);

	return psPCKInst->u.psPck->auComponent[uArgIdx];
}

IMG_INTERNAL
IMG_BOOL EqualPCKArgs(PINTERMEDIATE_STATE psState, PCINST psInst)
/*****************************************************************************
 FUNCTION	: EqualPCKArgs

 PURPOSE	: Checks if the two source arguments to a PCK instruction are the same
			  value with the same component select.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.

 RETURNS	: TRUE if the two source arguments are equal.
*****************************************************************************/
{
	ASSERT(g_psInstDesc[psInst->eOpcode].eType == INST_TYPE_PCK);

	if (!EqualArgs(&psInst->asArg[0], &psInst->asArg[1]))
	{
		return IMG_FALSE;
	}
	if (psInst->u.psPck->auComponent[0] != psInst->u.psPck->auComponent[1])
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_VOID SetComponentSelect(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uArgIdx, IMG_UINT32 uComponent)
/*****************************************************************************
 FUNCTION	: SetComponentSelect

 PURPOSE	: Set the component select on an instruction source argument.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction whose argument is to be modified.
			  uArgIdx			- Index of the argument to modify.
			  uComponent		- Component select to set.

 RETURNS	: Nothing.
*****************************************************************************/
{
	switch (g_psInstDesc[psInst->eOpcode].eType)
	{
		case INST_TYPE_FLOAT:
		case INST_TYPE_DPC:
		case INST_TYPE_EFO:
		case INST_TYPE_FARITH16:
		{
			PFLOAT_SOURCE_MODIFIER	psSourceModifier;

			psSourceModifier = GetFloatMod(psState, psInst, uArgIdx);
			if (psSourceModifier != NULL)
			{
				psSourceModifier->uComponent = uComponent;
			}
			else
			{
				ASSERT(uComponent == 0);
			}
			break;
		}
		case INST_TYPE_TEST:
		{
			ASSERT(uArgIdx < TEST_ALUOPCODE_MAXIMUM_ARGUMENT_COUNT);
			psInst->u.psTest->auSrcComponent[uArgIdx] = uComponent;
			break;
		}
		case INST_TYPE_FDOTPRODUCT:
		{
			ASSERT(uArgIdx < (sizeof(psInst->u.psFdp->auComponent) / sizeof(psInst->u.psFdp->auComponent[0])));
			psInst->u.psFdp->auComponent[uArgIdx] = uComponent;
			break;
		}
		#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
		case INST_TYPE_NRM:
		{
			ASSERT(uArgIdx < NRM_F32_USSOURCE_COUNT);
			psInst->u.psNrm->auComponent[uArgIdx] = uComponent;
			break;
		}
		#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */
		case INST_TYPE_IMAE:
		{
			ASSERT(uArgIdx < IMAE_UNIFIED_STORE_SOURCE_COUNT);
			psInst->u.psImae->auSrcComponent[uArgIdx] = uComponent;
			break;
		}
		case INST_TYPE_PCK:
		{
			SetPCKComponent(psState, psInst, uArgIdx, uComponent);
			break;
		}
		#if defined(SUPPORT_SGX545)
		case INST_TYPE_DUAL:
		{
			if (uArgIdx < DUAL_MAX_SEC_SOURCES)
			{
				psInst->u.psDual->auSourceComponent[uArgIdx] = uComponent;
			}
			else
			{
				ASSERT(uComponent == 0);
			}
			break;
		}
		#endif /* defined(SUPPORT_SGX545) */
		default:
		{
			ASSERT(uComponent == 0);
			break;
		}
	}
}

IMG_INTERNAL
IMG_UINT32 GetComponentSelect(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArgIdx)
/*****************************************************************************
 FUNCTION	: GetComponentSelect

 PURPOSE	: Get the component select from an instruction source argument.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.
			  uArgIdx			- Index of the argument.

 RETURNS	: The component select.
*****************************************************************************/
{
	switch (g_psInstDesc[psInst->eOpcode].eType)
	{
		case INST_TYPE_FLOAT:
		case INST_TYPE_DPC:
		case INST_TYPE_EFO:
		case INST_TYPE_FARITH16:
		{
			PFLOAT_SOURCE_MODIFIER	psSourceModifier;

			psSourceModifier = GetFloatMod(psState, psInst, uArgIdx);
			if (psSourceModifier != NULL)
			{
				return psSourceModifier->uComponent;
			}
			else
			{
				return 0;
			}
		}
		case INST_TYPE_FDOTPRODUCT:
		{
			ASSERT(uArgIdx < (sizeof(psInst->u.psFdp->auComponent) / sizeof(psInst->u.psFdp->auComponent[0])));
			return psInst->u.psFdp->auComponent[uArgIdx];
		}
		case INST_TYPE_TEST:
		{
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			if (psInst->eOpcode == IFPTESTPRED_VEC || psInst->eOpcode == IFPTESTMASK_VEC)
			{
				return 0;
			}
			else
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
			{
				ASSERT(uArgIdx < TEST_ALUOPCODE_MAXIMUM_ARGUMENT_COUNT);
				return psInst->u.psTest->auSrcComponent[uArgIdx];
			}
		}
		#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
		case INST_TYPE_NRM:
		{
			if (uArgIdx < NRM_F32_USSOURCE_COUNT)
			{
				return psInst->u.psNrm->auComponent[uArgIdx];
			}
			else
			{
				return 0;
			}
		}
		#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */
		case INST_TYPE_IMAE:
		{
			return GetIMAEComponent(psState, psInst, uArgIdx);
		}
		case INST_TYPE_PCK:
		{
			return GetPCKComponent(psState, psInst, uArgIdx);
		}
		#if defined(SUPPORT_SGX545)
		case INST_TYPE_DUAL:
		{
			if (uArgIdx < DUAL_MAX_SEC_SOURCES)
			{
				return psInst->u.psDual->auSourceComponent[uArgIdx];
			}
			else
			{
				return 0;
			}
		}
		#endif /* defined(SUPPORT_SGX545) */
		default:
		{
			return 0;
		}
	}
}

IMG_INTERNAL
IMG_VOID InitFloatSrcMod(PFLOAT_SOURCE_MODIFIER psMod)
/*****************************************************************************
 FUNCTION	: InitFloatSrcMod

 PURPOSE	: Reset a floating point source modifier.

 PARAMETERS	: psMod		- Structure representing the source modifier.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psMod->bNegate = IMG_FALSE;
	psMod->bAbsolute = IMG_FALSE;
	psMod->uComponent = 0;
}

IMG_INTERNAL
IMG_VOID InitPerArgumentParameters(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uArgIdx)
/*****************************************************************************
 FUNCTION	: InitPerArgumentParameters

 PURPOSE	: Reset any per-source instruction parameters.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.
			  uArgIdx			- Index of the source to reset the parameters
								for.

 RETURNS	: Nothing.
*****************************************************************************/
{
	switch (g_psInstDesc[psInst->eOpcode].eType)
	{
		case INST_TYPE_FLOAT:
		{
			ASSERT(uArgIdx < MAX_FLOAT_SOURCES);
			InitFloatSrcMod(&psInst->u.psFloat->asSrcMod[uArgIdx]);
			break;
		}
		case INST_TYPE_DPC:
		{
			ASSERT(uArgIdx < MAX_FLOAT_SOURCES);
			InitFloatSrcMod(&psInst->u.psDpc->sFloat.asSrcMod[uArgIdx]);
			break;
		}
		case INST_TYPE_EFO:
		{
			if (uArgIdx < EFO_HARDWARE_SOURCE_COUNT)
			{
				InitFloatSrcMod(&psInst->u.psEfo->sFloat.asSrcMod[uArgIdx]);
			}
			break;
		}
		case INST_TYPE_FARITH16:
		{
			ASSERT(uArgIdx < MAX_FLOAT_SOURCES);
			InitFloatSrcMod(&psInst->u.psArith16->sFloat.asSrcMod[uArgIdx]);
			psInst->u.psArith16->aeSwizzle[uArgIdx] = FMAD16_SWIZZLE_LOWHIGH;
			break;
		}
		#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
		case INST_TYPE_NRM:
		{
			ASSERT(uArgIdx < NRM_F32_USSOURCE_COUNT);
			psInst->u.psNrm->auComponent[uArgIdx] = 0;
			break;
		}
		#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) || defined(SUPPORT_SGX545)
		case INST_TYPE_IMA32:
		{
			ASSERT(uArgIdx < IMA32_SOURCE_COUNT);
			psInst->u.psIma32->abNegate[uArgIdx] = IMG_FALSE;
			break;
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554 || defined(SUPPORT_SGX545) */
		case INST_TYPE_TEST:
		{
			ASSERT(uArgIdx < TEST_ALUOPCODE_MAXIMUM_ARGUMENT_COUNT);
			psInst->u.psTest->auSrcComponent[uArgIdx] = 0;
			break;
		}
		default:
		{
			break;
		}
	}
}

IMG_INTERNAL
IMG_BOOL InstUsesF16FmtControl(PCINST psInst)
/*****************************************************************************
 FUNCTION	: InstUsesF16FmtControl

 PURPOSE	: Check for an instruction which supports interpreting source
			  arguments as either F32 or F16.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IOPCODE	eOpcode;

	if (psInst->eOpcode == ITESTPRED || psInst->eOpcode == ITESTMASK)
	{
		eOpcode = psInst->u.psTest->eAluOpcode;
	}
	else
	{
		eOpcode = psInst->eOpcode;
	}

	return ((g_psInstDesc[eOpcode].uFlags & DESC_FLAGS_F16FMTCTL) != 0) ? IMG_TRUE : IMG_FALSE;
}

IMG_INTERNAL
IMG_UINT32 GetMOEUnitsLog2(PINTERMEDIATE_STATE	psState,
						   PCINST				psInst,
						   IMG_BOOL				bDest,
						   IMG_UINT32			uArgIdx)
/*****************************************************************************
 FUNCTION	: GetMOELog2

 PURPOSE	: Gets the units of MOE base/increments used by an instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  bDest			- Set if the argument is a destination.
			  uArgIdx		- Instruction argument to check.

 RETURNS	: The units in bytes.
*****************************************************************************/
{
	PARG	psArg = bDest ? &psInst->asDest[uArgIdx] : &psInst->asArg[uArgIdx];

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	IMG_UINT32	uFlagToCheck;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(uArgIdx);
	PVR_UNREFERENCED_PARAMETER(bDest);

	/*
		Instructions which use the top-bit of the encoded register number to choose
		between interpreting a source argument as F16 or F32 use 16-bit MOE units
		for F16 source arguments.
	*/
	if (
			InstUsesF16FmtControl(psInst) &&
			!bDest &&
			psArg->eFmt == UF_REGFORMAT_F16 &&
			TypeUsesFormatControl(psArg->uType)
	   )
	{			
		return WORD_SIZE_LOG2;
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		For cores with the vector instruction set 128-bit MOE units for internal registers
		regardless of the type of instruction they are used with.
	*/
	if (
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0 && 
			psArg->uType == USEASM_REGTYPE_FPINTERNAL
	   )
	{
		return DQWORD_SIZE_LOG2;
	}

	if (
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0 && 
			psArg->uType == USEASM_REGTYPE_FPCONSTANT
	   )
	{
		return QWORD_SIZE_LOG2;
	}

	/*
		64-bit units for source arguments to SMP on cores with the vector instructions. 
	*/
	if (	
			!bDest &&
			(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_DEPENDENTTEXTURESAMPLE) != 0 &&
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0
		)
	{
		return QWORD_SIZE_LOG2;
	}

	if (psInst->eOpcode == IVDUAL)
	{
		/*
			Special case for vector dual-issue instruction: if the instruction writing the unified
			store destination/reading the unified store source is scalar then the units are 32-bits.
		*/
		if (bDest)
		{
			if (uArgIdx == VDUAL_DESTSLOT_UNIFIEDSTORE && !IsVDUALUnifiedStoreVectorResult(psInst))
			{
				return LONG_SIZE_LOG2;
			}
		}
		else
		{
			IMG_UINT32	uVecArgIdx;

			uVecArgIdx = uArgIdx / MAX_SCALAR_REGISTERS_PER_VECTOR;
			if (uVecArgIdx >= VDUAL_SRCSLOT_UNIFIEDSTORE && !IsVDUALUnifiedStoreVectorSource(psState, psInst))
			{
				return LONG_SIZE_LOG2;
			}
		}
	}

	/*
		Check the instruction description. 
	*/
	if (bDest)
	{
		uFlagToCheck = DESC_FLAGS2_DEST64BITMOEUNITS;
	}
	else
	{
		uFlagToCheck = DESC_FLAGS2_SOURCES64BITMOEUNITS;
	}
	if ((g_psInstDesc[psInst->eOpcode].uFlags2 & uFlagToCheck) != 0)
	{
		return QWORD_SIZE_LOG2;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Otherwise default to 32-bit units.
	*/
	return LONG_SIZE_LOG2;
}

IMG_INTERNAL
IMG_UINT32 GetIndexUnitsLog2(PINTERMEDIATE_STATE	psState,
							 PCINST					psInst,
							 IMG_BOOL				bDest,
							 IMG_UINT32				uArgIdx)
/*****************************************************************************
 FUNCTION	: GetIndexUnitsLog2

 PURPOSE	: Gets the units of dynamic indices used by an instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  bDest			- Set if the argument is a destination.
			  uArgIdx		- Instruction argument to check.

 RETURNS	: The units in bytes.
*****************************************************************************/
{
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psInst->eOpcode == IIDXSCR || psInst->eOpcode == IIDXSCW)
	{
		return WORD_SIZE_LOG2;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	return GetMOEUnitsLog2(psState, psInst, bDest, uArgIdx);
}

static
IMG_BOOL InstAssignedHardwareRegisters(PINTERMEDIATE_STATE psState, PINST psInst, PCARG psArg)
/*****************************************************************************
 FUNCTION	: InstAssignedHardwareRegisters
    
 PURPOSE	: Check if a instruction source or destination has been assigned
			  hardware registers.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  psArg				- Source or destination to check.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (psInst->psBlock == NULL)
	{
		return IMG_FALSE;
	}
	if (psArg->uType == USEASM_REGTYPE_FPINTERNAL)
	{
		return IMG_FALSE;
	}
	if (psArg->uType == USEASM_REGTYPE_PREDICATE && (psState->uFlags2 & USC_FLAGS2_ASSIGNED_PREDICATE_REGNUMS) != 0)
	{
		return IMG_FALSE;
	}
	if (psInst->psBlock->psOwner->psFunc == psState->psSecAttrProg)
	{
		if ((psState->uFlags & USC_FLAGS_ASSIGNEDSECPROGREGISTERS) != 0)
		{
			return IMG_TRUE;
		}
		return IMG_FALSE;
	}
	else
	{
		if (
				psInst->psBlock->psOwner->psFunc != psState->psSecAttrProg && 
				psArg->psRegister != NULL && 
				psArg->psRegister->psSecAttr != NULL
		   )
		{
			return IMG_FALSE;
		}
		if ((psState->uFlags2 & USC_FLAGS2_ASSIGNED_PRIMARY_REGNUMS) != 0)
		{
			return IMG_TRUE;
		}
		return IMG_FALSE;
	}
}

IMG_INTERNAL
IMG_VOID SetPredicateAtIndex(PINTERMEDIATE_STATE	psState,
							 PINST					psInst,
							 IMG_UINT32				uPredRegNum, 
							 IMG_BOOL				bPredNegate,
							 IMG_UINT32				uIndex)
/*****************************************************************************
 FUNCTION	: SetPredicateAtIndex

 PURPOSE	: Set the predicate controlling update of an instruction's
			  destinations at a given index.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to set the predicate for.
			  uPredRegNum	- Number of the predicate.
			  uIndex		- Index of the predicate.

 RETURNS	: Nothing.
*****************************************************************************/
{
	
	PARG	psPredSrc;

	if (psInst->uPredCount > 0 && psInst->apsPredSrc[uIndex] != NULL)
	{
		UseDefDropUse(psState, psInst->apsPredSrcUseDef[uIndex]);
	}

	if (uPredRegNum == USC_PREDREG_NONE)
	{
		if (psInst->uPredCount > 0 && psInst->apsPredSrc[uIndex] != NULL)
		{
			UscFree(psState, psInst->apsPredSrc[uIndex]);
			psInst->apsPredSrc[uIndex] = NULL;

			UscFree(psState, psInst->apsPredSrcUseDef[uIndex]);
			psInst->apsPredSrcUseDef[uIndex] = NULL;
		}
		if (psInst->uPredCount == 1)
		{
			SetPredCount(psState, psInst, 0);
		}
		return;
	}
	
	if (psInst->uPredCount == 0)
	{
		if (GetBit(psInst->auFlag, INST_PRED_PERCHAN) == 0)
		{
			SetPredCount(psState, psInst, 1);
		}
		else
		{
			SetPredCount(psState, psInst, 4);
		}
	}

	if (psInst->apsPredSrc[uIndex] == NULL)
	{
		psInst->apsPredSrc[uIndex] = UscAlloc(psState, sizeof(*psPredSrc));

		ASSERT(psInst->apsPredSrcUseDef[uIndex] == NULL);
		psInst->apsPredSrcUseDef[uIndex] = UscAlloc(psState, sizeof(*psInst->apsPredSrcUseDef[uIndex]));
		UseDefReset(psInst->apsPredSrcUseDef[uIndex], USE_TYPE_PREDICATE, uIndex, psInst);
	}
	psPredSrc = psInst->apsPredSrc[uIndex];

	InitInstArg(psPredSrc);
	psPredSrc->uType = USEASM_REGTYPE_PREDICATE;
	psPredSrc->uNumber = uPredRegNum;

	if ((psState->uFlags2 & USC_FLAGS2_PRED_USE_DEF_INFO_VALID) != 0)
	{
		psPredSrc->psRegister = GetVRegister(psState, USEASM_REGTYPE_PREDICATE, uPredRegNum);
	}

	SetBit(psInst->auFlag, INST_PRED_NEG, bPredNegate ? 1U : 0U);

	if (!InstAssignedHardwareRegisters(psState, psInst, psInst->apsPredSrc[uIndex]))
	{
		UseDefAddUse(psState,
					 USEASM_REGTYPE_PREDICATE, 
					 uPredRegNum,
					 psInst->apsPredSrcUseDef[uIndex]);
	}
}

IMG_INTERNAL
IMG_VOID SetPredicate(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uPredRegNum, IMG_BOOL bPredNegate)
/*****************************************************************************
 FUNCTION	: SetPredicate

 PURPOSE	: Set the predicate controlling update of an instruction's
			  destinations.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to set the predicate for.
			  uPredRegNum	- Predicate register number or USC_PREDREG_NONE to
							clear any predicate.
			  bPredNegate	- If TRUE invert the sense of the predicate.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(GetBit(psInst->auFlag, INST_PRED_PERCHAN) == 0);

	SetPredicateAtIndex(psState, psInst, uPredRegNum, bPredNegate, 0);
}

IMG_INTERNAL
IMG_VOID MakePredicatePerChan(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: MakePredicatePerChan

 PURPOSE	: Set the predicate controlling update of an instruction's
			  destinations to have per channel behaviour.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to modify the predicate for.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(GetBit(psInst->auFlag, INST_PRED_NEG) == 0);

	SetPredCount(psState, psInst, 4);
	SetBit(psInst->auFlag, INST_PRED_PERCHAN, 1);
}

IMG_INTERNAL
IMG_VOID ClearPredicates(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: ClearPredicates

 PURPOSE	: Change an instruction to update its destination unconditionally.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to set the predicate for.
			  uIndex		- Index of the predicate.

 RETURNS	: Nothing.
*****************************************************************************/
{
	SetPredCount(psState, psInst, 0);
}

IMG_INTERNAL
IMG_VOID ClearPredicate(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uIndex)
/*****************************************************************************
 FUNCTION	: ClearPredicate

 PURPOSE	: Remove a predicate from an instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to set the predicate for.
			  uIndex		- Index of the predicate.

 RETURNS	: Nothing.
*****************************************************************************/
{
	SetPredicateAtIndex(psState, psInst, USC_PREDREG_NONE, IMG_FALSE /* bPredNegate */, uIndex);
}

IMG_INTERNAL
IMG_BOOL NoPredicate(PINTERMEDIATE_STATE psState, PCINST psInst)
/*****************************************************************************
 FUNCTION	: NoPredicate

 PURPOSE	: Check for an instruction which updates its destination unconditionally.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.

 RETURNS	: TRUE if the instruction is unpredicated.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);

	return (psInst->uPredCount == 0);
}

IMG_INTERNAL
IMG_VOID CopyPredicate(PINTERMEDIATE_STATE psState, PINST psDestInst, const INST *psSrcInst)
/*****************************************************************************
 FUNCTION	: CopyPredicate

 PURPOSE	: Copy the predicate controlling update of the destination from one
			  instruction to another.

 PARAMETERS	: psState		- Compiler state.
			  psDestInst	- Instruction to copy the predicate to.
			  psSrcInst		- Instruction to copy the predicate from.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32  uPred;

	for (uPred = 0; uPred < psSrcInst->uPredCount; ++uPred)
	{
		IMG_UINT32	uPredSrc;
		IMG_BOOL	bPredNegate, bPredPerChan;

		GetPredicate(psSrcInst, &uPredSrc, &bPredNegate, &bPredPerChan, uPred);

		if (bPredPerChan)
		{
			SetBit(psDestInst->auFlag, INST_PRED_PERCHAN, 1);
		}

		SetPredicateAtIndex(psState, psDestInst, uPredSrc, bPredNegate, uPred);
	}
}

IMG_INTERNAL
IMG_VOID GetPredicate(const INST			*psInst, 
					  IMG_PUINT32			puPredRegNum, 
					  IMG_PBOOL				pbPredNegate,
					  IMG_PBOOL				pbPredPerChan,
					  IMG_UINT32			uIndex)
/*****************************************************************************
 FUNCTION	: GetPredicate

 PURPOSE	: Gets predicate controlling update of an instruction's
			  destination.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to get the predicate from.
			  puPredRegNum	- Returns the predicate register number or
							USC_PREDREG_NONE if the instruction is unpredicated.
			  pbPredNegate	- Returns the negate flag on the predicate.
			  pbPredPerChan	- Returns the per channel flag on the predicate.
			  uIndex		- The index of the predicate

 RETURNS	: Nothing.
*****************************************************************************/
{
	*puPredRegNum = USC_PREDREG_NONE;
	*pbPredNegate = IMG_FALSE;
	*pbPredPerChan = IMG_FALSE;

	if (psInst->uPredCount > uIndex)
	{
		PARG	psPredSrc;

		psPredSrc = psInst->apsPredSrc[uIndex];
		if (psPredSrc != NULL)
		{
			*puPredRegNum = psPredSrc->uNumber;
		}

		*pbPredNegate = GetBit(psInst->auFlag, INST_PRED_NEG) ? IMG_TRUE : IMG_FALSE;
		*pbPredPerChan = GetBit(psInst->auFlag, INST_PRED_PERCHAN) ? IMG_TRUE : IMG_FALSE;
	}
}

IMG_INTERNAL
IMG_BOOL GetPredicateNegate(PINTERMEDIATE_STATE	psState, 
							const INST			*psInst)
/*****************************************************************************
 FUNCTION	: GetPredicateNegate

 PURPOSE	: Gets predicate negate flag, if instruction is predicated
			  or returns FALSE

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to get the predicate from.
			  uIndex		- The index of the predicate

 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);

	if (psInst->uPredCount != 0)
	{
		return (GetBit(psInst->auFlag, INST_PRED_NEG) == 1) ? IMG_TRUE : IMG_FALSE;
	}
	else
	{
		return IMG_FALSE;
	}
}

IMG_INTERNAL
IMG_BOOL GetPredicatePerChan(PINTERMEDIATE_STATE	psState, 
							 const INST				*psInst)
/*****************************************************************************
 FUNCTION	: GetPredicatePerChan

 PURPOSE	: Gets predicate per channel flag, if instruction is predicated
			  or returns FALSE

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to get the predicate from.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);

	if (psInst->uPredCount != 0)
	{
		return (GetBit(psInst->auFlag, INST_PRED_PERCHAN) == 1) ? IMG_TRUE : IMG_FALSE;
	}
	else
	{
		return IMG_FALSE;
	}
}

IMG_INTERNAL
IMG_INT32 ComparePredicates(PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: ComparePredicates
    
 PURPOSE	: Check if two instructions have the same predicate.

 PARAMETERS	: psState			- Compiler state.
			  psInst1,	psInst2	- Instructions to compare.
			  
 RETURNS	: -ve		if the first instruction's predicate is less than the second.
			   0		if the instructions have the same predicate.
		      +ve		if the second instruction's predicate is greater than the second.
*****************************************************************************/
{
	IMG_UINT32	uInst1PredSrc, uInst2PredSrc;
	IMG_BOOL	bInst1PredNeg, bInst2PredNeg;
	IMG_BOOL	bInst1PredPerChan, bInst2PredPerChan;
	IMG_UINT32	uPred;

	for (uPred = 0; uPred < max(psInst1->uPredCount, psInst2->uPredCount); ++uPred)
	{
		GetPredicate(psInst1, &uInst1PredSrc, &bInst1PredNeg, &bInst1PredPerChan, uPred);
		GetPredicate(psInst2, &uInst2PredSrc, &bInst2PredNeg, &bInst2PredPerChan, uPred);

		if (uInst1PredSrc != uInst2PredSrc)
		{
			return (IMG_INT32)(uInst1PredSrc - uInst2PredSrc);
		}

		if (bInst1PredPerChan != bInst2PredPerChan)
		{
			return (IMG_INT32)(bInst1PredPerChan - bInst2PredPerChan);
		}

		if (bInst1PredNeg != bInst2PredNeg)
		{
			return (IMG_INT32)(bInst1PredNeg - bInst2PredNeg);
		}
	}

	return 0;
}


IMG_INTERNAL
IMG_BOOL EqualPredicates(PCINST psInst1, PCINST psInst2)
/*****************************************************************************
 FUNCTION	: EqualPredicates
    
 PURPOSE	: Check if two instructions have the same predicate.

 PARAMETERS	: psInst1,	psInst2	- Instructions to compare.
			  
 RETURNS	: IMG_TRUE if the instructions have the same predicate.
*****************************************************************************/
{
	if (ComparePredicates(psInst1, psInst2) == 0)
	{
		return IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
}

IMG_INTERNAL
IMG_VOID MovePartiallyWrittenDest(PINTERMEDIATE_STATE	psState,
								  PINST					psMoveToInst,
								  IMG_UINT32			uMoveToDestIdx,
								  PINST					psMoveFromInst,
								  IMG_UINT32			uMoveFromDestIdx)
/*****************************************************************************
 FUNCTION	: MovePartiallyWrittenDest
    
 PURPOSE	: Move the old value of a conditionally/partially written destination
			  from one instruction to another.

 PARAMETERS	: psState			- Compiler state.
			  psMoveToInst		- Instruction to copy to.
			  uMoveToDestIdx	- Index of the destination to copy to.
			  psMoveFromInst	- Instruction to copy from.
			  uMoveFromDestIdx	- Index of the destination to copy from.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	SetPartiallyWrittenDest(psState, psMoveToInst, uMoveToDestIdx, psMoveFromInst->apsOldDest[uMoveFromDestIdx]);
	SetPartiallyWrittenDest(psState, psMoveFromInst, uMoveFromDestIdx, NULL /* psPartialDest */);
}

IMG_INTERNAL
IMG_VOID CopyPartiallyWrittenDest(PINTERMEDIATE_STATE	psState,
								  PINST					psCopyToInst,
								  IMG_UINT32			uCopyToDestIdx,
								  PINST					psCopyFromInst,
								  IMG_UINT32			uCopyFromDestIdx)
/*****************************************************************************
 FUNCTION	: CopyPartiallyWrittenDest
    
 PURPOSE	: Copy the old value of a conditionally/partially written destination
			  from one instruction to another.

 PARAMETERS	: psState			- Compiler state.
			  psCopyToInst		- Instruction to copy to.
			  uCopyToDestIdx	- Index of the destination to copy to.
			  psCopyFromInst	- Instruction to copy from.
			  uCopyFromDestIdx	- Index of the destination to copy from.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	SetPartiallyWrittenDest(psState, psCopyToInst, uCopyToDestIdx, psCopyFromInst->apsOldDest[uCopyFromDestIdx]);
}

IMG_INTERNAL
IMG_VOID SetPartiallyWrittenDest(PINTERMEDIATE_STATE	psState,
								 PINST					psInst,
								 IMG_UINT32				uDestIdx,
								 PCARG					psPartialDest)
/*****************************************************************************
 FUNCTION	: SetPartiallyWrittenDest
    
 PURPOSE	: Set the old value of a conditionally/partially written destination.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.
			  uDestIdx			- Index of the destination to modify.
			  psPartialDest		- Value to set.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG	psPrevOldDest;

	ASSERT(uDestIdx < psInst->uDestCount);

	/*
		Get the old destination currently set in the instruction.
	*/
	psPrevOldDest = psInst->apsOldDest[uDestIdx];
	if (psPrevOldDest != NULL)
	{
		/*
			Drop this reference to the currently set old destination from the USE-DEF chain.
		*/
		UseDefDropArgUses(psState, psInst->apsOldDestUseDef[uDestIdx]);

		/*
			Free the old destination.
		*/
		UscFree(psState, psInst->apsOldDest[uDestIdx]);
		psInst->apsOldDest[uDestIdx] = NULL;

		/*
			Free USE-DEF for the old destination.
		*/
		UscFree(psState, psInst->apsOldDestUseDef[uDestIdx]);
		psInst->apsOldDestUseDef[uDestIdx] = NULL;
	}

	if (psPartialDest == NULL)
	{
		return;
	}

	psInst->apsOldDest[uDestIdx] = UscAlloc(psState, sizeof(*psInst->apsOldDest[uDestIdx]));
	*psInst->apsOldDest[uDestIdx] = *psPartialDest;

	psInst->apsOldDestUseDef[uDestIdx] = UscAlloc(psState, sizeof(*psInst->apsOldDestUseDef[uDestIdx]));
	UseDefResetArgument(psInst->apsOldDestUseDef[uDestIdx], USE_TYPE_OLDDEST, USE_TYPE_OLDDESTIDX, uDestIdx, psInst);

	if (!InstAssignedHardwareRegisters(psState, psInst, psPartialDest))
	{
		UseDefAddArgUses(psState, psPartialDest, psInst->apsOldDestUseDef[uDestIdx]);
	}
}

IMG_INTERNAL
IMG_VOID CopySrc(PINTERMEDIATE_STATE	psState, 
				 PINST					psCopyToInst, 
				 IMG_UINT32				uCopyToIdx, 
				 PINST					psCopyFromInst, 
				 IMG_UINT32				uCopyFromIdx)
/*****************************************************************************
 FUNCTION	: CopySrc
    
 PURPOSE	: Duplicate an instruction source into another instruction.

 PARAMETERS	: psState			- Compiler state.
			  psCopyToInst		- Instruction to modify.
			  uCopyToIdx		- Instruction source to modify.
			  psCopyFromInst	- Instruction to copy a source from.
			  uCopyFromIdx		- Instruction source to copy.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG				psCopyTo;
	PARG				psCopyFrom;
	PARGUMENT_USEDEF	psCopyToUseDef;

	ASSERT(uCopyToIdx < psCopyToInst->uArgumentCount);
	psCopyTo = &psCopyToInst->asArg[uCopyToIdx];
	psCopyToUseDef = &psCopyToInst->asArgUseDef[uCopyToIdx];

	ASSERT(uCopyFromIdx < psCopyFromInst->uArgumentCount);
	psCopyFrom = &psCopyFromInst->asArg[uCopyFromIdx];

	/*
		For any existing source drop this location from its USE-DEF information.
	*/
	UseDefDropArgUses(psState, psCopyToUseDef);

	/*
		Copy the source.
	*/
	*psCopyTo = *psCopyFrom;

	/*
		Add uses of the source at the new location.
	*/
	if (!InstAssignedHardwareRegisters(psState, psCopyToInst, psCopyTo))
	{
		UseDefAddArgUses(psState, psCopyTo, psCopyToUseDef);
	}
}

IMG_INTERNAL
IMG_VOID MovePartialDestToSrc(PINTERMEDIATE_STATE	psState, 
							  PINST					psMoveToInst, 
							  IMG_UINT32			uMoveToIdx, 
							  PINST					psMoveFromInst, 
							  IMG_UINT32			uMoveFromIdx)
/*****************************************************************************
 FUNCTION	: MovePartialDestToSrc
    
 PURPOSE	: Move a partially overwritten destination from an instruction into
			  another instruction's source.

 PARAMETERS	: psState			- Compiler state.
			  psMoveToInst		- Instruction to modify.
			  uMoveToIdx		- Instruction source to modify.
			  psMoveFromInst	- Instruction to move a partial destination from.
			  uMoveFromIdx		- Instruction partial destination to move.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG	psMoveTo;
	PARG	psMoveFrom;

	ASSERT(uMoveToIdx < psMoveToInst->uArgumentCount);
	psMoveTo = &psMoveToInst->asArg[uMoveToIdx];

	ASSERT(uMoveFromIdx < psMoveFromInst->uDestCount);
	psMoveFrom = psMoveFromInst->apsOldDest[uMoveFromIdx];
	ASSERT(psMoveFrom != NULL);

	UseDefDropArgUses(psState, &psMoveToInst->asArgUseDef[uMoveToIdx]);

	*psMoveTo = *psMoveFrom;
	InitInstArg(psMoveFrom);

	if (!InstAssignedHardwareRegisters(psState, psMoveToInst, psMoveTo))
	{
		UseDefReplaceArgumentUses(psState,
								  psMoveTo,
								  &psMoveToInst->asArgUseDef[uMoveToIdx],
								  psMoveFromInst->apsOldDestUseDef[uMoveFromIdx]);
	}
}

IMG_INTERNAL
IMG_VOID MoveSrc(PINTERMEDIATE_STATE	psState, 
				 PINST					psMoveToInst, 
				 IMG_UINT32				uMoveToIdx, 
				 PINST					psMoveFromInst, 
				 IMG_UINT32				uMoveFromIdx)
/*****************************************************************************
 FUNCTION	: MoveSrc
    
 PURPOSE	: Move a source from an instruction into another instruction.

 PARAMETERS	: psState			- Compiler state.
			  psMoveToInst		- Instruction to modify.
			  uMoveToIdx		- Instruction source to modify.
			  psMoveFromInst	- Instruction to move a source from.
			  uMoveFromIdx		- Instruction source to move.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG	psMoveTo;
	PARG	psMoveFrom;

	if (psMoveToInst == psMoveFromInst && uMoveToIdx == uMoveFromIdx)
	{
		return;
	}

	ASSERT(uMoveToIdx < psMoveToInst->uArgumentCount);
	psMoveTo = &psMoveToInst->asArg[uMoveToIdx];

	ASSERT(uMoveFromIdx < psMoveFromInst->uArgumentCount);
	psMoveFrom = &psMoveFromInst->asArg[uMoveFromIdx];

	UseDefDropArgUses(psState, &psMoveToInst->asArgUseDef[uMoveToIdx]);

	*psMoveTo = *psMoveFrom;
	InitInstArg(psMoveFrom);

	if (!InstAssignedHardwareRegisters(psState, psMoveToInst, psMoveTo))
	{
		UseDefReplaceArgumentUses(psState,
								  psMoveTo,
								  &psMoveToInst->asArgUseDef[uMoveToIdx],
								  &psMoveFromInst->asArgUseDef[uMoveFromIdx]);
	}
}

IMG_INTERNAL
IMG_VOID MoveDest(PINTERMEDIATE_STATE	psState, 
				  PINST					psMoveToInst, 
				  IMG_UINT32			uMoveToIdx, 
				  PINST					psMoveFromInst, 
				  IMG_UINT32			uMoveFromIdx)
/*****************************************************************************
 FUNCTION	: MoveDest
    
 PURPOSE	: Move a destination from an instruction into another instruction.

 PARAMETERS	: psState			- Compiler state.
			  psMoveToInst		- Instruction to modify.
			  uMoveToIdx		- Instruction destination to modify.
			  psMoveFromInst	- Instruction to move a destination from.
			  uMoveFromIdx		- Instruction destination to move.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG			psMoveTo;
	PARG			psMoveFrom;

	/*
		Check for nothing to do.
	*/
	if (psMoveToInst == psMoveFromInst && uMoveToIdx == uMoveFromIdx)
	{
		return;
	}

	ASSERT(uMoveToIdx < psMoveToInst->uDestCount);
	psMoveTo = &psMoveToInst->asDest[uMoveToIdx];

	ASSERT(uMoveFromIdx < psMoveFromInst->uDestCount);
	psMoveFrom = &psMoveFromInst->asDest[uMoveFromIdx];

	/*
		Drop any references to USE-DEF chains from the overwritten destination.
	*/
	UseDefDropDest(psState, psMoveToInst, uMoveToIdx);

	/*
		Move the destination register details.
	*/
	*psMoveTo = *psMoveFrom;

	/*
		Clear the old location of the destination.
	*/
	InitInstArg(psMoveFrom);

	/*
		Update the location of the definition for the new destination.
	*/
	if (!InstAssignedHardwareRegisters(psState, psMoveToInst, psMoveTo))
	{
		UseDefReplaceDest(psState,
						  psMoveToInst,
						  uMoveToIdx,
						  psMoveFromInst,
						  uMoveFromIdx);
	}
}

static
IMG_VOID ClearSrc(PINTERMEDIATE_STATE	psState,
				  PINST					psInst,
				  IMG_UINT32			uSrcIdx)
/*****************************************************************************
 FUNCTION	: ClearSrc
    
 PURPOSE	: Clear references to a source from USE-DEF chains.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.
			  uSrcIdx			- Index of the source to modify.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG	psSrc;

	DBG_ASSERT(uSrcIdx < psInst->uArgumentCount);
	psSrc = &psInst->asArg[uSrcIdx];

	UseDefDropArgUses(psState, &psInst->asArgUseDef[uSrcIdx]);

	InitInstArg(psSrc);
}

IMG_INTERNAL
IMG_VOID SetSrcUnused(PINTERMEDIATE_STATE	psState,
					  PINST					psInst,
					  IMG_UINT32			uSrcIdx)
/*****************************************************************************
 FUNCTION	: SetSrcUnused
    
 PURPOSE	: Mark a source as unused.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.
			  uSrcIdx			- Index of the source to modify.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	ClearSrc(psState, psInst, uSrcIdx);
	psInst->asArg[uSrcIdx].uType = USC_REGTYPE_UNUSEDSOURCE;
}

static
IMG_VOID SetArgIndex(PINTERMEDIATE_STATE	psState,
					 PINST					psInst,
					 USEDEF_TYPE			eArgType,
					 IMG_UINT32				uArgIdx,
					 IMG_UINT32				uNewIndexType,
					 IMG_UINT32				uNewIndexNumber,
					 IMG_UINT32				uNewIndexArrayOffset,
					 IMG_UINT32				uNewIndexRelativeStrideInBytes)
/*****************************************************************************
 FUNCTION	: SetArgIndex
    
 PURPOSE	: Set the index used by an argument to an instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.
			  eArgType			- Type of the argument to modify.
			  uArgIdx			- Index of the argument to modify.
			  uNewIndexType		- Description of the source for the dynamic index.
			  uNewIndexNumber	
			  uNewIndexArrayOffset
			  uNewIndexRelativeStrideInBytes
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG	psArg;
	PUSEDEF	psArgUse;

	switch (eArgType)
	{
		case USE_TYPE_SRCIDX:
		{
			ASSERT(uArgIdx < psInst->uArgumentCount);
			psArg = &psInst->asArg[uArgIdx];
			psArgUse = &psInst->asArgUseDef[uArgIdx].sIndexUseDef;
			break;
		}
		case USE_TYPE_DESTIDX:
		{
			ASSERT(uArgIdx < psInst->uDestCount);
			psArg = &psInst->asDest[uArgIdx];
			psArgUse = &psInst->asDestUseDef[uArgIdx].sIndexUseDef;
			break;
		}
		case USE_TYPE_OLDDESTIDX:
		{
			ASSERT(uArgIdx < psInst->uDestCount);
			psArg = psInst->apsOldDest[uArgIdx];
			psArgUse = &psInst->apsOldDestUseDef[uArgIdx]->sIndexUseDef;
			break;
		}
		default: imgabort();
	}

	UseDefDropUse(psState, psArgUse);

	psArg->uIndexType = uNewIndexType;
	psArg->uIndexNumber = uNewIndexNumber;
	psArg->uIndexArrayOffset = uNewIndexArrayOffset;
	if (uNewIndexRelativeStrideInBytes != USC_UNDEF)
	{
		psArg->uIndexStrideInBytes = uNewIndexRelativeStrideInBytes;
	}
	psArg->psIndexRegister = GetVRegister(psState, uNewIndexType, uNewIndexNumber);

	UseDefAddUse(psState, uNewIndexType, uNewIndexNumber, psArgUse);
}

IMG_INTERNAL
IMG_VOID SetArgNoIndex(PINTERMEDIATE_STATE	psState,
					   PUSEDEF				psUse)
/*****************************************************************************
 FUNCTION	: SetArgNoIndex
    
 PURPOSE	: Set an argument to an instruction to unindexed.

 PARAMETERS	: psState			- Compiler state.
			  psUse				- Argument to modify.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(psUse->eType == USE_TYPE_SRCIDX || psUse->eType == USE_TYPE_DESTIDX || psUse->eType == USE_TYPE_OLDDESTIDX);
	SetArgIndex(psState,
				psUse->u.psInst,
				psUse->eType,
				psUse->uLocation,
				USC_REGTYPE_NOINDEX,
				USC_UNDEF /* uNewIndexNumber */,
				USC_UNDEF /* uNewIndexArrayOffset */,
				0 /* uNewIndexRelativeStrideInBytes */);
}


IMG_INTERNAL
IMG_VOID SetSrcIndex(PINTERMEDIATE_STATE	psState,
					 PINST					psInst,
					 IMG_UINT32				uSrcIdx,
					 IMG_UINT32				uNewIndexType,
					 IMG_UINT32				uNewIndexNumber,
					 IMG_UINT32				uNewIndexArrayOffset,
					 IMG_UINT32				uNewIndexRelativeStrideInBytes)
/*****************************************************************************
 FUNCTION	: SetSrcIndex
    
 PURPOSE	: Set the index used by a destination to an instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.
			  uArgIdx			- Index of the destination to modify.
			  uNewIndexType		- Description of the source for the dynamic index.
			  uNewIndexNumber	
			  uNewIndexArrayOffset
			  uNewIndexRelativeStrideInBytes
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	SetArgIndex(psState,
				psInst,
				USE_TYPE_SRCIDX,
				uSrcIdx,
				uNewIndexType,
				uNewIndexNumber,
				uNewIndexArrayOffset,
				uNewIndexRelativeStrideInBytes);
}

IMG_INTERNAL
IMG_VOID SetDestIndex(PINTERMEDIATE_STATE	psState,
					  PINST					psInst,
					  IMG_UINT32			uDestIdx,
					  IMG_UINT32			uNewIndexType,
					  IMG_UINT32			uNewIndexNumber,
					  IMG_UINT32			uNewIndexArrayOffset,
					  IMG_UINT32			uNewIndexRelativeStrideInBytes)
/*****************************************************************************
 FUNCTION	: SetDestIndex
    
 PURPOSE	: Set the index used by a destination to an instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.
			  uArgIdx			- Index of the destination to modify.
			  uNewIndexType		- Description of the source for the dynamic index.
			  uNewIndexNumber	
			  uNewIndexArrayOffset
			  uNewIndexRelativeStrideInBytes
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	SetArgIndex(psState,
				psInst,
				USE_TYPE_DESTIDX,
				uDestIdx,
				uNewIndexType,
				uNewIndexNumber,
				uNewIndexArrayOffset,
				uNewIndexRelativeStrideInBytes);
}

IMG_INTERNAL
IMG_VOID SetPartialDestIndex(PINTERMEDIATE_STATE	psState,
							 PINST					psInst,
							 IMG_UINT32				uDestIdx,
							 IMG_UINT32				uNewIndexType,
							 IMG_UINT32				uNewIndexNumber,
							 IMG_UINT32				uNewIndexArrayOffset,
							 IMG_UINT32				uNewIndexRelativeStrideInBytes)
/*****************************************************************************
 FUNCTION	: SetPartialDestIndex
    
 PURPOSE	: Set the index used by a partially overwritten destination to an instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.
			  uArgIdx			- Index of the destination to modify.
			  uNewIndexType		- Description of the source for the dynamic index.
			  uNewIndexNumber	
			  uNewIndexArrayOffset
			  uNewIndexRelativeStrideInBytes
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	SetArgIndex(psState,
				psInst,
				USE_TYPE_OLDDESTIDX,
				uDestIdx,
				uNewIndexType,
				uNewIndexNumber,
				uNewIndexArrayOffset,
				uNewIndexRelativeStrideInBytes);
}

IMG_INTERNAL
IMG_VOID SetArgumentIndex(PINTERMEDIATE_STATE	psState,
						  PINST					psInst,
						  IMG_BOOL				bDest,
						  IMG_UINT32			uArgIdx,
						  IMG_UINT32			uNewIndexType,
						  IMG_UINT32			uNewIndexNumber,
						  IMG_UINT32			uNewIndexArrayOffset,
						  IMG_UINT32			uNewIndexRelativeStrideInBytes)
/*****************************************************************************
 FUNCTION	: SetArgumentIndex
    
 PURPOSE	: Set the index used by a source or destination to an instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.
			  bDest				- If TRUE modify a destination.
								  If FALSE modify a source.
			  uArgIdx			- Index of the source or destination to modify.
			  uNewIndexType		- Description of the source for the dynamic index.
			  uNewIndexNumber	
			  uNewIndexArrayOffset
			  uNewIndexRelativeStrideInBytes
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	SetArgIndex(psState,
				psInst,
				bDest ? USE_TYPE_DESTIDX : USE_TYPE_SRCIDX,
				uArgIdx,
				uNewIndexType,
				uNewIndexNumber,
				uNewIndexArrayOffset,
				uNewIndexRelativeStrideInBytes);
}

IMG_INTERNAL
IMG_VOID MakeArg(PINTERMEDIATE_STATE psState, IMG_UINT32 uArgType, IMG_UINT32 uArgNumber, UF_REGFORMAT eArgFmt, PARG psArg)
/*****************************************************************************
 FUNCTION	: MakeArg
    
 PURPOSE	: Initialize an argument structure.

 PARAMETERS	: psState			- Compiler state.
			  uSrcType			- New register type.
			  uSrcNumber		- New register number.
			  eSrcFormat		- New register format.
			  psArg				- Argument structure to initialize.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	InitInstArg(psArg);
	psArg->uType = uArgType;
	psArg->uNumber = uArgNumber;
	psArg->eFmt = eArgFmt;
	psArg->psRegister = GetVRegister(psState, uArgType, uArgNumber);
}

IMG_INTERNAL
IMG_VOID SetSrc(PINTERMEDIATE_STATE		psState,
				PINST					psInst,
				IMG_UINT32				uSrcIdx,
				IMG_UINT32				uNewSrcType,
				IMG_UINT32				uNewSrcNumber,
				UF_REGFORMAT			eNewSrcFmt)
/*****************************************************************************
 FUNCTION	: SetSrc
    
 PURPOSE	: Set the source to an instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.
			  uDestIdx			- Index of the source to modify.
			  uNewSrcType		- New source register type.
			  uNewSrcNumber		- New source register number.
			  eNewSrcFormat		- New source register format.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG	psSrc;

	ClearSrc(psState, psInst, uSrcIdx);

	ASSERT(uSrcIdx < psInst->uArgumentCount);
	psSrc = &psInst->asArg[uSrcIdx];

	psSrc->uType = uNewSrcType;
	psSrc->uNumber = uNewSrcNumber;
	psSrc->eFmt = eNewSrcFmt;
	psSrc->psRegister = GetVRegister(psState, uNewSrcType, uNewSrcNumber);

	if (!InstAssignedHardwareRegisters(psState, psInst, psSrc))
	{
		UseDefAddUse(psState, uNewSrcType, uNewSrcNumber, &psInst->asArgUseDef[uSrcIdx].sUseDef);
	}
}

IMG_INTERNAL
IMG_VOID SetSrcFromArg(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSrcIdx, PCARG psNewSrc)
/*****************************************************************************
 FUNCTION	: SetSrcFromArg
    
 PURPOSE	: Set the source to an instruction from an argument structure.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.
			  uDestIdx			- Index of the source to modify.
			  psNewSrc			- Argument to set from.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psNewSrc->uType == USC_REGTYPE_REGARRAY)
	{
		SetArraySrc(psState, psInst, uSrcIdx, psNewSrc->uNumber, psNewSrc->uArrayOffset, psNewSrc->eFmt);
	}
	else
	{
		SetSrc(psState, psInst, uSrcIdx, psNewSrc->uType, psNewSrc->uNumber, psNewSrc->eFmt);
	}
	SetSrcIndex(psState, 
				psInst, 
				uSrcIdx, 
				psNewSrc->uIndexType, 
				psNewSrc->uIndexNumber, 
				psNewSrc->uIndexArrayOffset, 
				psNewSrc->uIndexStrideInBytes);
}

IMG_INTERNAL
IMG_VOID SetPartialDest(PINTERMEDIATE_STATE		psState,
						PINST					psInst,
						IMG_UINT32				uDestIdx,
						IMG_UINT32				uNewType,
						IMG_UINT32				uNewNumber,
						UF_REGFORMAT			eNewFmt)
/*****************************************************************************
 FUNCTION	: SetPartialDest
    
 PURPOSE	: Set the source for channels in the destination argument either
			  masked out or only conditionally written.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.
			  uDestIdx			- Index of the destination to modify.
			  uNewType			- New partial destination register type.
			  uNewNumber		- New partial destination register number.
			  eNewFormat		- New partial destination register format.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	ARG		sPartialDest;

	InitInstArg(&sPartialDest);
	sPartialDest.uType = uNewType;
	sPartialDest.uNumber = uNewNumber;
	sPartialDest.eFmt = eNewFmt;
	sPartialDest.psRegister = GetVRegister(psState, uNewType, uNewNumber);

	SetPartiallyWrittenDest(psState, psInst, uDestIdx, &sPartialDest);
}

static
IMG_VOID ClearDest(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uDestIdx)
/*****************************************************************************
 FUNCTION	: ClearDest
    
 PURPOSE	: Drop any existing information about an instruction destination.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.
			  uDestIdx			- Index of the destination to modify.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG	psDest;

	UseDefDropDest(psState, psInst, uDestIdx);

	ASSERT(uDestIdx < psInst->uDestCount);
	psDest = &psInst->asDest[uDestIdx];

	InitInstArg(psDest);
}

IMG_INTERNAL
IMG_VOID SetDestUnused(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uDestIdx)
/*****************************************************************************
 FUNCTION	: SetDestUnused
    
 PURPOSE	: Mark the destination of an instruction as unused.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.
			  uDestIdx			- Index of the destination to modify.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG	psDest;

	ASSERT(uDestIdx < psInst->uDestCount);
	psDest = &psInst->asDest[uDestIdx];

	ClearDest(psState, psInst, uDestIdx);
	psDest->uType = USC_REGTYPE_UNUSEDDEST;
}

IMG_INTERNAL
IMG_VOID SetDestFromArg(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uDestIdx, PCARG psNewDest)
/*****************************************************************************
 FUNCTION	: SetDestFromArg
    
 PURPOSE	: Set the destination of an instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.
			  uDestIdx			- Index of the destination to modify.
			  psNewDest			- Destination to set.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG	psDest;

	ASSERT(uDestIdx < psInst->uDestCount);
	psDest = &psInst->asDest[uDestIdx];

	/*
		Clear any currently set destination.
	*/
	ClearDest(psState, psInst, uDestIdx);

	/*
		Copy the new destination.
	*/
	*psDest = *psNewDest;

	/*
		Record the location of the definition of the destination.
	*/
	if (!InstAssignedHardwareRegisters(psState, psInst, psNewDest))
	{
		UseDefAddDef(psState, psNewDest->uType, psNewDest->uNumber, &psInst->asDestUseDef[uDestIdx].sUseDef);
	}

	/*
		If the destination uses a dynamic index then add a new use of the register containing
		the index.
	*/
	UseDefAddUse(psState,
				 psNewDest->uIndexType,
				 psNewDest->uIndexNumber,
				 &psInst->asDestUseDef[uDestIdx].sIndexUseDef);
}

IMG_INTERNAL
IMG_VOID SetDest(PINTERMEDIATE_STATE	psState,
				 PINST					psInst,
				 IMG_UINT32				uDestIdx,
				 IMG_UINT32				uNewDestType,
				 IMG_UINT32				uNewDestNumber,
				 UF_REGFORMAT			eNewDestFormat)
/*****************************************************************************
 FUNCTION	: SetDest
    
 PURPOSE	: Set the destination to an instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.
			  uDestIdx			- Index of the destination to modify.
			  uNewDestType		- New destination register type.
			  uNewDestNumber	- New destination register number.
			  eNewDestFormat	- New destination register format.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	ARG	sArg;

	InitInstArg(&sArg);
	sArg.uType = uNewDestType;
	sArg.uNumber = uNewDestNumber;
	sArg.eFmt = eNewDestFormat;
	sArg.psRegister = GetVRegister(psState, uNewDestType, uNewDestNumber);

	SetDestFromArg(psState, psInst, uDestIdx, &sArg);
}

IMG_INTERNAL
IMG_VOID SetUseFromArg(PINTERMEDIATE_STATE	psState,
					   PUSEDEF				psUseToReplace,
					   PARG					psReplacement)
/*****************************************************************************
 FUNCTION	: SetUseFromArg
    
 PURPOSE	: Set the source register at a use site.

 PARAMETERS	: psState			- Compiler state.
			  psUseToReplace	- Use site to modify.
			  psReplacement		- New source register.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	switch (psUseToReplace->eType)
	{
		case USE_TYPE_SRC: 
		{
			SetSrcFromArg(psState, psUseToReplace->u.psInst, psUseToReplace->uLocation, psReplacement);
			break;
		}
		case USE_TYPE_OLDDEST:
		{
			SetPartiallyWrittenDest(psState, psUseToReplace->u.psInst, psUseToReplace->uLocation, psReplacement);
			break;
		}
		case USE_TYPE_SRCIDX:
		case USE_TYPE_DESTIDX:
		case USE_TYPE_OLDDESTIDX:
		{
			SetArgIndex(psState, 
						psUseToReplace->u.psInst, 
						psUseToReplace->eType,
						psUseToReplace->uLocation, 
						psReplacement->uType, 
						psReplacement->uNumber, 
						psReplacement->uArrayOffset, 
						USC_UNDEF /* uNewIndexRelativeStrideInBytes */);
			break;
		}	
		case USE_TYPE_SWITCH:
		case USE_TYPE_COND:
		case USE_TYPE_FIXEDREG: imgabort();
		default: imgabort();
	}
}

IMG_INTERNAL
IMG_VOID SetConditionalBlockPredicate(PINTERMEDIATE_STATE	psState,
									  PCODEBLOCK			psBlock,
									  IMG_UINT32			uNewPredicate)
/*****************************************************************************
 FUNCTION	: SetConditionalBlockPredicate
    
 PURPOSE	: Set the predicate register used to control which successor of a
			  conditional block is chosen.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to modify.
			  uNewPredicate		- New predicate register to use.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG	psPredSrc = &psBlock->u.sCond.sPredSrc;
	PUSEDEF	psCondUse = &psBlock->u.sCond.sPredSrcUse;

	ASSERT(psBlock->eType == CBTYPE_COND);

	/*
		Drop the use of the old predicate register.
	*/
	UseDefDropUse(psState, psCondUse);

	MakePredicateArgument(psPredSrc, uNewPredicate);
	
	/*
		Add a new use for the replacement predicate register.
	*/
	UseDefAddUse(psState,
				 psPredSrc->uType,
				 psPredSrc->uNumber,
				 psCondUse);
}

IMG_INTERNAL
IMG_BOOL ReferencedOutsideBlock(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PARG psDest)
/*****************************************************************************
 FUNCTION	: ReferencedOutsideBlock
    
 PURPOSE	: Check if an instruction destination has uses outside a flow control
			  block.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Flow control block.
			  psDest			- Instruction destination.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (psDest->uType == USC_REGTYPE_REGARRAY)
	{
		return IMG_TRUE;
	}
	if (UseDefIsReferencedOutsideBlock(psState, UseDefGet(psState, psDest->uType, psDest->uNumber), psBlock))
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
PINST CreateVMOVorIMOVPRED(PINTERMEDIATE_STATE	psState,
						   PINST				psSrcLineInst,
						   PCODEBLOCK			psInsertBlock,
						   PINST				psInsertBeforeInst,
						   IMG_UINT32			uDestMask,
						   PARG					psSrc,
						   IMG_UINT32			uSwizzle,
						   IMG_BOOL				bWriteUpperDest,
						   IMG_UINT32			uDestRegType)
/*****************************************************************************
 FUNCTION	: CreateVMOV
    
 PURPOSE	: Allocate a vector move instruction.

 PARAMETERS	: psState				- Compiler state.
			  psSrcLineInst			- Instruction to copy source line information
									for the new instruction from.
			  psInsertBlock			- Block to insert the new instruction into.
			  psInsertBeforeInst	- Point to insert the new instruction at.
			  uDestMask				- Mask of channels to write.
			  psSrc					- Source data for the move.
			  uSwizzle				- Swizzle to apply to the source data.
			  bWriteUpperDest		- If TRUE then the instruction is copying
									the XY channels from the swizzled source
									into the ZW channels of the destination.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psMOVInst;

	psMOVInst = AllocateInst(psState, psSrcLineInst);

	if (uDestRegType == USEASM_REGTYPE_PREDICATE)
	{
		SetOpcode(psState, psMOVInst, IMOVPRED);
	}
	else
	{
		SetOpcode(psState, psMOVInst, IVMOV);
		psMOVInst->u.psVec->uDestSelectShift = bWriteUpperDest ? USC_Z_CHAN : USC_X_CHAN;
		psMOVInst->u.psVec->auSwizzle[0] = uSwizzle;
		psMOVInst->u.psVec->aeSrcFmt[0] = psSrc->eFmt;
	}
	
	psMOVInst->auDestMask[0] = uDestMask;
	SetSrcFromArg(psState, psMOVInst, 0 /* uSrcIdx */, psSrc);

	InsertInstBefore(psState, psInsertBlock, psMOVInst, psInsertBeforeInst);
	
	return psMOVInst;
}

static
IMG_VOID CopyPartiallyOverwrittenVectorData(PINTERMEDIATE_STATE psState,
											PCODEBLOCK			psInsertBlock,
											PINST				psInsertBeforeInst,
											PINST				psInst, 
											IMG_UINT32			uDestIdx)
/*****************************************************************************
 FUNCTION	: CopyPartiallyOverwrittenVectorData
    
 PURPOSE	: Convert a vector instruction to a series of moves copying partially/conditionally
			  overwritten destinations directly to the instruction destinations.

 PARAMETERS	: psState			- Compiler state.
			  psInsertBlock		- Block to insert the new instructions into.
			  psInsertBeforeInst
								- Instruction to insert the new instructions
								before.
			  psInst			- Instruction to generate moves from.
			  uDestIdx			- Destination to copy.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG			psOldDest = psInst->apsOldDest[uDestIdx];
	UF_REGFORMAT	eOldDestFmt = psOldDest->eFmt;
	IMG_UINT32		uMaskToCopy = GetPreservedChansInPartialDest(psState, psInst, uDestIdx);
	PINST			psFirstInst;
	PINST			psLastInst;

	if (uMaskToCopy == 0)
	{
		/* Nothing to do */
		return;
	}

	if (
			(psState->uFlags & USC_FLAGS_POSTC10REGALLOC) != 0 && 
			eOldDestFmt == UF_REGFORMAT_F32 && 
			psOldDest->uType != USEASM_REGTYPE_FPINTERNAL
		)
	{
		static const IMG_UINT32 auHalfMaxMask[] = {USC_XY_CHAN_MASK, USC_ZW_CHAN_MASK};
		IMG_UINT32 uHalf;

		/*
			Copy only 64-bits in a single instruction.
		*/
		psFirstInst = psLastInst = NULL;
		for (uHalf = 0; uHalf < 2; uHalf++)
		{
			IMG_UINT32	uHalfMask;
			IMG_UINT32	uHalfMaskToCopy;
			IMG_UINT32	uSwizzle;
			IMG_BOOL	bWriteUpperDest;

			if (uHalf == 0)
			{
				uSwizzle = USEASM_SWIZZLE(X, Y, Z, W);
				bWriteUpperDest = IMG_FALSE;
				uHalfMask = USC_XY_CHAN_MASK;
			}
			else
			{
				uSwizzle = USEASM_SWIZZLE(Z, W, Z, W);
				bWriteUpperDest = IMG_TRUE;
				uHalfMask = USC_ZW_CHAN_MASK;
			}

			uHalfMaskToCopy = uMaskToCopy & auHalfMaxMask[uHalf];
			if (uHalfMaskToCopy != 0)
			{
				PINST		psMOVInst;

				psMOVInst = CreateVMOVorIMOVPRED(psState, 
												 psInst, 
												 psInsertBlock, 
												 psInsertBeforeInst, 
												 uHalfMaskToCopy,
												 psOldDest, 
												 uSwizzle, 
												 bWriteUpperDest, 
												 psInst->asDest[uDestIdx].uType);

				if (psFirstInst == NULL)
				{
					psFirstInst = psMOVInst;
				}
				psLastInst = psMOVInst;
			}
		}
	}
	else
	{
		psFirstInst = psLastInst = CreateVMOVorIMOVPRED(psState, 
														psInst,
														psInsertBlock,
														psInsertBeforeInst,
														uMaskToCopy,
														psOldDest, 
														USEASM_SWIZZLE(X, Y, Z, W), 
														IMG_FALSE /* bWriteUpperDest */, 
														psInst->asDest[uDestIdx].uType);
	}

	/*
		Copy the existing instruction destination to the last instruction in the sequence.
	*/
	MoveDest(psState, psLastInst, 0 /* uMoveDestIdx */, psInst, uDestIdx);
	psLastInst->auLiveChansInDest[0] = uMaskToCopy;

	/*
		For the first instruction in the sequence allocate a new register for it's partial
		result.
	*/
	if (psLastInst != psFirstInst)
	{
		ARG		sMaskTemp;

		MakeNewTempArg(psState, eOldDestFmt, &sMaskTemp);
		SetDestFromArg(psState, psFirstInst, 0 /* uMoveDestIdx */, &sMaskTemp);
		SetPartiallyWrittenDest(psState, psLastInst, 0 /* uMoveDestIdx */, &sMaskTemp);
		psFirstInst->auLiveChansInDest[0] = uMaskToCopy & ~psLastInst->auDestMask[0];
	}
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

IMG_INTERNAL
IMG_VOID CopyPartiallyOverwrittenDataInBlock(PINTERMEDIATE_STATE	psState, 
											 PCODEBLOCK				psInsertBlock, 
											 PINST					psInst,
											 PINST					psInsertBeforeInst)
/*****************************************************************************
 FUNCTION	: CopyPartiallyOverwrittenDataInBlock
    
 PURPOSE	: Convert an instruction to a series of moves copying partially/conditionally
			  overwritten destinations directly to the instruction destinations.

 PARAMETERS	: psState			- Compiler state.
			  psInsertBlock		- Block to insert the new instructions into.
			  psInst			- Instruction to generate moves from.
			  psInsertBeforeInst
								- Instruction to insert the new instructions
								before.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uDestIdx;

	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		if (psInst->apsOldDest[uDestIdx] != NULL && !EqualArgs(psInst->apsOldDest[uDestIdx], &psInst->asDest[uDestIdx]))
		{
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			if (InstHasVectorDest(psState, psInst, uDestIdx))
			{
				CopyPartiallyOverwrittenVectorData(psState, psInsertBlock, psInsertBeforeInst, psInst, uDestIdx);
			}
			else
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
			{
				PINST		psMOVInst;
				IMG_UINT32	uMaskToCopy = GetPreservedChansInPartialDest(psState, psInst, uDestIdx);

				if (uMaskToCopy == 0)
				{
					continue;
				}

				psMOVInst = AllocateInst(psState, psInst);

				if (psInst->asDest[uDestIdx].uType == USEASM_REGTYPE_PREDICATE)
				{
					SetOpcode(psState, psMOVInst, IMOVPRED);
				}
				else
				{
					SetOpcode(psState, psMOVInst, IMOV);
				}
				psMOVInst->auLiveChansInDest[0] = uMaskToCopy;
				MoveDest(psState, psMOVInst, 0 /* uMoveDestIdx */, psInst, uDestIdx);
				MovePartialDestToSrc(psState, psMOVInst, 0 /* uCopyToSrcIdx */, psInst, uDestIdx);
				InsertInstBefore(psState, psInsertBlock, psMOVInst, psInsertBeforeInst);
			}
		}
	}
}

IMG_INTERNAL
IMG_VOID CopyPartiallyOverwrittenData(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: CopyPartiallyOverwrittenData
    
 PURPOSE	: Convert an instruction to a series of moves copying partially/conditionally
			  overwritten destinations directly to the instruction destinations.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to generate moves from.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	CopyPartiallyOverwrittenDataInBlock(psState, psInst->psBlock, psInst, psInst);
}

IMG_INTERNAL
IMG_VOID SetArraySrc(PINTERMEDIATE_STATE	psState,
					 PINST					psInst,
					 IMG_UINT32				uSrcIdx,
					 IMG_UINT32				uNewSrcNumber,
					 IMG_UINT32				uNewSrcArrayOffset,
					 UF_REGFORMAT			eNewSrcFormat)
/*****************************************************************************
 FUNCTION	: SetArraySrc
    
 PURPOSE	: Set the source to an instruction to an element
			  in an array.

 PARAMETERS	: psState				- Compiler state.
			  psInst				- Instruction to modify.
			  uArgIdx				- Index of the source to modify.
			  uNewSrcNumber			- New source array identifier.
			  uNewSrcArrayOffset	- New source array element.
			  eNewSrcFormat			- New source register format.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG	psSrc;

	ClearSrc(psState, psInst, uSrcIdx);

	ASSERT(uSrcIdx < psInst->uArgumentCount);
	psSrc = &psInst->asArg[uSrcIdx];

	psSrc->uType = USC_REGTYPE_REGARRAY;
	psSrc->uNumber = uNewSrcNumber;
	psSrc->uArrayOffset = uNewSrcArrayOffset;
	psSrc->eFmt = eNewSrcFormat;

	UseDefAddUse(psState, USC_REGTYPE_REGARRAY, uNewSrcNumber, &psInst->asArgUseDef[uSrcIdx].sUseDef);
}

static
IMG_VOID SetArrayDest(PINTERMEDIATE_STATE	psState,
					  PINST					psInst,
					  IMG_UINT32			uDestIdx,
					  IMG_UINT32			uNewDestNumber,
					  IMG_UINT32			uNewDestArrayOffset,
					  UF_REGFORMAT			eNewDestFormat)
/*****************************************************************************
 FUNCTION	: SetArrayDest
    
 PURPOSE	: Set the destination to an instruction to an element
			  in an array.

 PARAMETERS	: psState				- Compiler state.
			  psInst				- Instruction to modify.
			  uArgIdx				- Index of the destination to modify.
			  uNewDestNumber		- New destination array identifier.
			  uNewDestArrayOffset	- New destination array element.
			  eNewDestFormat		- New destination register format.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG	psDest;

	ClearDest(psState, psInst, uDestIdx);

	ASSERT(uDestIdx < psInst->uDestCount);
	psDest = &psInst->asDest[uDestIdx];

	psDest->uType = USC_REGTYPE_REGARRAY;
	psDest->uNumber = uNewDestNumber;
	psDest->uArrayOffset = uNewDestArrayOffset;
	psDest->eFmt = eNewDestFormat;

	UseDefAddDef(psState, USC_REGTYPE_REGARRAY, uNewDestNumber, &psInst->asDestUseDef[uDestIdx].sUseDef);
}

IMG_INTERNAL
IMG_VOID SetArrayArgument(PINTERMEDIATE_STATE	psState,
						  PINST					psInst,
						  IMG_BOOL				bDest,
						  IMG_UINT32			uArgIdx,
						  IMG_UINT32			uNewArgNumber,
						  IMG_UINT32			uNewArgArrayOffset,
						  UF_REGFORMAT			eNewArgFormat)
/*****************************************************************************
 FUNCTION	: SetArrayArgument
    
 PURPOSE	: Set the source or destination to an instruction to an element
			  in an array.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.
			  uArgIdx			- Index of the source or destination to modify.
			  uNewArgNumber		- New argument array identifier.
			  uNewArgArrayOffset
								- New argument array element.
			  eNewArgFormat		- New argument register format.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (bDest)
	{
		SetArrayDest(psState, psInst, uArgIdx, uNewArgNumber, uNewArgArrayOffset, eNewArgFormat);
	}
	else
	{
		SetArraySrc(psState, psInst, uArgIdx, uNewArgNumber, uNewArgArrayOffset, eNewArgFormat);
	}
}

IMG_INTERNAL
IMG_VOID SetArgument(PINTERMEDIATE_STATE	psState,
					 PINST					psInst,
					 IMG_BOOL				bDest,
					 IMG_UINT32				uArgIdx,
					 IMG_UINT32				uNewArgType,
					 IMG_UINT32				uNewArgNumber,
					 UF_REGFORMAT			eNewArgFmt)
/*****************************************************************************
 FUNCTION	: SetArgument
    
 PURPOSE	: Set the source or destination to an instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.
			  uDestIdx			- Index of the source or destination to modify.
			  uNewArgType		- New argument register type.
			  uNewArgNumber		- New argument register number.
			  eNewArgFormat		- New argument register format.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (bDest)
	{
		SetDest(psState, psInst, uArgIdx, uNewArgType, uNewArgNumber, eNewArgFmt);
	}
	else
	{
		SetSrc(psState, psInst, uArgIdx, uNewArgType, uNewArgNumber, eNewArgFmt);
	}
}

IMG_INTERNAL
IMG_VOID MoveArgument(PINTERMEDIATE_STATE	psState, 
					  IMG_BOOL				bDest,
					  PINST					psMoveToInst, 
					  IMG_UINT32			uMoveToIdx, 
					  PINST					psMoveFromInst, 
					  IMG_UINT32			uMoveFromIdx)
/*****************************************************************************
 FUNCTION	: MoveArgument
    
 PURPOSE	: Move a source or destination from an instruction into another instruction.

 PARAMETERS	: psState			- Compiler state.
			  bDest				- TRUE to move a destination.
								  FALSE to move a source.
			  psMoveToInst		- Instruction to modify.
			  uMoveToIdx		- Instruction source or destination to modify.
			  psMoveFromInst	- Instruction to move a source from.
			  uMoveFromIdx		- Instruction source or destination to move.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (bDest)
	{
		MoveDest(psState, psMoveToInst, uMoveToIdx, psMoveFromInst, uMoveFromIdx);
	}
	else
	{
		MoveSrc(psState, psMoveToInst, uMoveToIdx, psMoveFromInst, uMoveFromIdx);
	}
}

IMG_INTERNAL
IMG_BOOL RegTypeReferencedInInstSources(PINST psInst, IMG_UINT32 uType)
/*****************************************************************************
 FUNCTION	: RegTypeReferencedInInstSources
    
 PURPOSE	: Checks whether specified type is referenced as instruction sources.

 PARAMETERS	: psInst				- Instruction input
			  uType					- Type to check
			  
 RETURNS	: True if found
*****************************************************************************/
{
	IMG_UINT32 uArgIdx;
	for(uArgIdx = 0; uArgIdx < psInst->uArgumentCount; uArgIdx++)
	{
		if(psInst->asArg[uArgIdx].uType == uType)
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_BOOL RegTypeReferencedInInstDestination(PINST psInst, IMG_UINT32 uType)
/*****************************************************************************
 FUNCTION	: RegTypeReferencedInInstDestination
    
 PURPOSE	: Checks whether specified type is referenced as instruction destination.

 PARAMETERS	: psInst				- Instruction input
			  uType					- Type to check
			  
 RETURNS	: True if found
*****************************************************************************/
{
	IMG_UINT32 uDstIdx;
	for(uDstIdx = 0; uDstIdx < psInst->uDestCount; uDstIdx++)
	{
		if(psInst->asDest[uDstIdx].uType == USC_REGTYPE_UNUSEDSOURCE)
		{
			return IMG_FALSE;
		}
		if(psInst->asDest[uDstIdx].uType == uType)
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_VOID ForAllInstructionsOfType(PINTERMEDIATE_STATE		psState, 
								  IOPCODE					eType, 
								  PFN_PROCESS_INSTRUCTION	pfnProcess)
/*****************************************************************************
 FUNCTION	: ForAllInstructionsOfType

 PURPOSE	: Apply a function to all instructions with a particular opcode.

 PARAMETERS	: psState			- Compiler state.
			  eType				- Opcode.
			  pfnProcess		- Function to apply.

 RETURNS	: Nothing.
*****************************************************************************/
{
	SAFE_LIST_ITERATOR	sIter;

	/*
		Apply a function to all the instructions with a particular opcode.
	*/
	InstListIteratorInitialize(psState, eType, &sIter);
	for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
	{
		pfnProcess(psState, InstListIteratorCurrent(&sIter));
	}
	InstListIteratorFinalise(&sIter);
}

IMG_INTERNAL
IMG_VOID InstListIteratorInitialize(PINTERMEDIATE_STATE psState, IOPCODE eOpType, PSAFE_LIST_ITERATOR psIterator)
{
	SafeListIteratorInitialize(&psState->asOpcodeLists[eOpType], psIterator);
}

IMG_INTERNAL
IMG_VOID InstListIteratorInitializeAtEnd(PINTERMEDIATE_STATE psState, IOPCODE eOpType, PSAFE_LIST_ITERATOR psIterator)
{
	SafeListIteratorInitializeAtEnd(&psState->asOpcodeLists[eOpType], psIterator);
}

struct _WEAK_INST_LIST;

typedef struct _WEAK_INST_LIST_ENTRY
{
	PINST					psInst;
	USC_LIST_ENTRY			sInstWeakRefListEntry;
	struct _WEAK_INST_LIST*	psList;
	USC_LIST_ENTRY			sListEntry;
} WEAK_INST_LIST_ENTRY, *PWEAK_INST_LIST_ENTRY;

IMG_INTERNAL
IMG_VOID WeakInstListInitialize(PWEAK_INST_LIST psList)
/*****************************************************************************
 FUNCTION	: WeakInstListInitialize

 PURPOSE	: Initialize a list of weak references to instructions.

 PARAMETERS	: psList		- List to initialize.

 RETURNS	: Nothing.
*****************************************************************************/
{
	InitializeList(&psList->sList);
}

IMG_INTERNAL
IMG_VOID WeakInstListAppend(PINTERMEDIATE_STATE psState, PWEAK_INST_LIST psList, PINST psInst)
/*****************************************************************************
 FUNCTION	: WeakInstListAppend

 PURPOSE	: Add an instruction to a list of weak references to instructions.
			  If the instruction is subsequently deleted then it is removed
			  from the list.

 PARAMETERS	: psState		- Compiler state.
			  psList		- List to initialize.
			  psInst		- Instruction to insert.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PWEAK_INST_LIST_ENTRY	psElem;

	psElem = UscAlloc(psState, sizeof(*psElem));
	psElem->psInst = psInst;
	psElem->psList = psList;
	AppendToList(&psList->sList, &psElem->sListEntry);
	AppendToList(&psInst->sWeakRefList, &psElem->sInstWeakRefListEntry);
}

IMG_INTERNAL
PINST WeakInstListRemoveHead(PINTERMEDIATE_STATE psState, PWEAK_INST_LIST psList)
/*****************************************************************************
 FUNCTION	: WeakInstListRemoveHead

 PURPOSE	: Remove the first instruction from a list of weak references to
			  instruction.

 PARAMETERS	: psState		- Compiler state.
			  psList		- List to modify.

 RETURNS	: The old first instruction or NULL if the list is empty.
*****************************************************************************/
{
	PUSC_LIST_ENTRY			psHeadListEntry;
	PWEAK_INST_LIST_ENTRY	psHead;
	PINST					psHeadInst;

	psHeadListEntry = RemoveListHead(&psList->sList);
	if (psHeadListEntry == NULL)
	{
		return NULL;
	}
	psHead = IMG_CONTAINING_RECORD(psHeadListEntry, PWEAK_INST_LIST_ENTRY, sListEntry);
	psHeadInst = psHead->psInst;
	RemoveFromList(&psHeadInst->sWeakRefList, &psHead->sInstWeakRefListEntry);
	UscFree(psState, psHead);
	return psHeadInst;
}

static
IMG_VOID WeakInstListFreeInst(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: WeakInstListFreeInst

 PURPOSE	: Remove weak references to an instruction when freeing it.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction which is to be freed.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;

	while ((psListEntry = RemoveListHead(&psInst->sWeakRefList)) != NULL)
	{
		PWEAK_INST_LIST_ENTRY	psRef;

		psRef = IMG_CONTAINING_RECORD(psListEntry, PWEAK_INST_LIST_ENTRY, sInstWeakRefListEntry);
		RemoveFromList(&psRef->psList->sList, &psRef->sListEntry);
		UscFree(psState, psRef);
	}
}

/* EOF */
