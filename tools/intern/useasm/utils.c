/******************************************************************************
 * Name         : utils.c
 * Title        : Pixel Shader Compiler
 * Author       : John Russell
 * Created      : Jan 20021
 *
 * Copyright    : 2002-2010 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited, Home Park
 *              : Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * Modifications:-
 * $Log: utils.c $
 *****************************************************************************/

#define INCLUDE_SGX_FEATURE_TABLE

#define INCLUDE_SGX_BUG_TABLE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "sgxdefs.h"

#include "use.h"
#include "useasm.h"
#include "osglue.h"

/********************************************************************************
	Tables for mapping SGX cores/core revisions to available USE features.
 ********************************************************************************/
#if defined(SUPPORT_SGX520)

	/* SGX520 */
static const SGX_CORE_FEATURES g_sSGX520_Features_Table =
{
	SGX_FEATURE_FLAGS_USE_NO_INSTRUCTION_PAIRING |			/* ui32Flags */
	SGX_FEATURE_FLAGS_USE_HIGH_PRECISION_FIR |
	SGX_FEATURE_FLAGS_USE_LOAD_MOE_FROM_REGISTERS |
	SGX_FEATURE_FLAGS_USE_UNIFIED_TEMPS_AND_PAS,
	0,														/* ui32Flags2 */
	8,														/* ui32NumMutexes */
	8,														/* ui32NumMonitors */
	3,														/* ui32TextureStateSize */
	1,														/* ui32IterationStateSize */
	1,														/* ui32NumUSEPipes */
	12,														/* ui32NumProgramCounterBits */
	3,														/* ui32NumInternalRegisters */
	&g_uNonVecSpecialRegistersInvalidForNonBitwiseCount,	/* uInvalidSpecialRegistersForNonBitwiseCount */
	g_auNonVecSpecialRegistersInvalidForNonBitwise,			/* auInvalidSpecialRegistersForNonBitwise */
	"SGX520",												/* pszName */
};
#endif
#if defined(SUPPORT_SGX530)

	/* SGX530 */
static const SGX_CORE_FEATURES g_sSGX530_Features_Table =
{
	SGX_FEATURE_FLAGS_USE_UNIFIED_TEMPS_AND_PAS,			/* ui32Flags */
	0,														/* ui32Flags2 */
	1,														/* ui32NumMutexes */
	0,														/* ui32NumMonitors */
	3,														/* ui32TextureStateSize */
	1,														/* ui32IterationStateSize */
	2,														/* ui32NumUSEPipes */
	12,														/* ui32NumProgramCounterBits */
	3,														/* ui32NumInternalRegisters */
	&g_uNonVecSpecialRegistersInvalidForNonBitwiseCount,	/* uInvalidSpecialRegistersForNonBitwiseCount */
	g_auNonVecSpecialRegistersInvalidForNonBitwise,			/* auInvalidSpecialRegistersForNonBitwise */
	"SGX530",												/* pszName */
};

#endif
#if defined(SUPPORT_SGX531)

	/* SGX531 */
static const SGX_CORE_FEATURES g_sSGX531_Features_Table =
{
	SGX_FEATURE_FLAGS_USE_FNORMALISE |
	SGX_FEATURE_FLAGS_USE_UNIFIED_TEMPS_AND_PAS,			/* ui32Flags */				
	0,														/* ui32Flags2 */
	1,														/* ui32NumMutexes */
	0,														/* ui32NumMonitors */
	3,														/* ui32TextureStateSize */
	1,														/* ui32IterationStateSize */
	2,														/* ui32NumUSEPipes */
	12,														/* ui32NumProgramCounterBits */
	3,														/* ui32NumInternalRegisters */
	&g_uNonVecSpecialRegistersInvalidForNonBitwiseCount,	/* uInvalidSpecialRegistersForNonBitwiseCount */
	g_auNonVecSpecialRegistersInvalidForNonBitwise,			/* auInvalidSpecialRegistersForNonBitwise */
	"SGX531",												/* pszName */
};
#endif
#if defined(SUPPORT_SGX535)


	/* SGX535 */
static const SGX_CORE_FEATURES g_sSGX535_Features_Table =
{
	 0,														/* ui32Flags */
	 SGX_FEATURE_FLAGS2_TAG_VOLUME_TEXTURES,				/* ui32Flags2 */
	 1,														/* ui32NumMutexes */
	 0,														/* ui32NumMonitors */
	 3,														/* ui32TextureStateSize */
	 1,														/* ui32IterationStateSize */
	 2,														/* ui32NumUSEPipes */
	 12,													/* ui32NumProgramCounterBits */
	 3,														/* ui32NumInternalRegisters */
	 &g_uNonVecSpecialRegistersInvalidForNonBitwiseCount,	/* uInvalidSpecialRegistersForNonBitwiseCount */
	 g_auNonVecSpecialRegistersInvalidForNonBitwise,		/* auInvalidSpecialRegistersForNonBitwise */
	 "SGX535",												/* pszName */
};
#endif
#if defined(SUPPORT_SGX540)

	/* SGX540 */
static const SGX_CORE_FEATURES g_sSGX540_Features_Table =
{
	 SGX_FEATURE_FLAGS_USE_FNORMALISE |
	 SGX_FEATURE_FLAGS_USE_UNIFIED_TEMPS_AND_PAS,			/* ui32Flags */		
	 0,														/* ui32Flags2 */
	 1,														/* ui32NumMutexes */
	 0,														/* ui32NumMonitors */
	 3,														/* ui32TextureStateSize */
	 1,														/* ui32IterationStateSize */
	 4,														/* ui32NumUSEPipes */
	 12,													/* ui32NumProgramCounterBits */
	 3,														/* ui32NumInternalRegisters */
	 &g_uNonVecSpecialRegistersInvalidForNonBitwiseCount,	/* uInvalidSpecialRegistersForNonBitwiseCount */
	 g_auNonVecSpecialRegistersInvalidForNonBitwise,		/* auInvalidSpecialRegistersForNonBitwise */
	 "SGX540",												/* pszName */
};
#endif
#if defined(SUPPORT_SGX541)

	/* SGX541 */
static const SGX_CORE_FEATURES g_sSGX541_Features_Table =
{
	SGX_FEATURE_FLAGS_USE_FNORMALISE |
	SGX_FEATURE_FLAGS_USE_UNIFIED_TEMPS_AND_PAS,			/* ui32Flags */				
	0,														/* ui32Flags2 */
	1,														/* ui32NumMutexes */
	0,														/* ui32NumMonitors */
	3,														/* ui32TextureStateSize */
	1,														/* ui32IterationStateSize */
	4,														/* ui32NumUSEPipes */
	12,														/* ui32NumProgramCounterBits */
	3,														/* ui32NumInternalRegisters */
	&g_uNonVecSpecialRegistersInvalidForNonBitwiseCount,	/* uInvalidSpecialRegistersForNonBitwiseCount */
	g_auNonVecSpecialRegistersInvalidForNonBitwise,			/* auInvalidSpecialRegistersForNonBitwise */
	"SGX541",												/* pszName */
};
#endif
#if defined(SUPPORT_SGX543)

	/* SGX543 */
static const SGX_CORE_FEATURES g_sSGX543_Features_Table =
{
	 SGX_FEATURE_FLAGS_USE_NO_INSTRUCTION_PAIRING |		/* ui32Flags */
	 SGX_FEATURE_FLAGS_USE_BRANCH_PWAIT |				
	 SGX_FEATURE_FLAGS_TAG_RAWSAMPLE |
	 SGX_FEATURE_FLAGS_TAG_RAWSAMPLEBOTH |
	 SGX_FEATURE_FLAGS_USE_VEC34 |
	 SGX_FEATURE_FLAGS_USE_UNLIMITED_PHASES |
	 SGX_FEATURE_FLAGS_USE_32BIT_INT_MAD |
	 SGX_FEATURE_FLAGS_TAG_UNPACK_RESULT |
	 SGX_FEATURE_FLAGS_USE_UNIFIED_TEMPS_AND_PAS |
	 SGX_FEATURE_FLAGS_USE_NOPTRIGGER |
	 SGX_FEATURE_FLAGS_USE_BRANCH_EXCEPTION |
	 SGX_FEATURE_FLAGS_USE_BRANCH_EXTSYNCEND,			
	 SGX_FEATURE_FLAGS2_USE_LDST_QWORD |				/* ui32Flags2 */
	 SGX_FEATURE_FLAGS2_USE_IDXSC |
	 SGX_FEATURE_FLAGS2_USE_TEST_SABLND |
	 SGX_FEATURE_FLAGS2_USE_LDRSTR_REPEAT |
	 SGX_FEATURE_FLAGS2_USE_STR_PREDICATE |
	 SGX_FEATURE_FLAGS2_USE_LDRSTR_EXTENDED_IMMEDIATE |
	 SGX_FEATURE_FLAGS2_USE_TWO_INDEX_BANKS |
	 SGX_FEATURE_FLAGS2_USE_SPRVV |
	 SGX_FEATURE_FLAGS2_USE_LDST_EXTENDED_CACHE_MODES |
	 SGX_FEATURE_FLAGS2_USE_SMP_RESULT_FORMAT_CONVERT |
	 SGX_FEATURE_FLAGS2_USE_SMP_REDUCEDREPEATCOUNT |
	 SGX_FEATURE_FLAGS2_USE_IMA32_32X16_PLUS_32 |
	 SGX_FEATURE_FLAGS2_USE_C10U8_CHAN_ORDER_RGBA |
	 SGX_FEATURE_FLAGS2_TAG_UNPACK_RESULT_TO_F16 |
	 SGX_FEATURE_FLAGS2_TAG_UNPACK_RESULT_TO_F32 |
	 SGX_FEATURE_FLAGS2_USE_TEST_SUB32 |
	 SGX_FEATURE_FLAGS2_USE_BITWISE_NO_REPEAT_MASK |
	 SGX_FEATURE_FLAGS2_USE_SMP_NO_REPEAT_MASK |
	 SGX_FEATURE_FLAGS2_USE_BRANCH_ALL_ANY_INSTANCES |
	 SGX_FEATURE_FLAGS2_USE_IDF_SUPPORTS_SKIPINVALID |
	 SGX_FEATURE_FLAGS2_USE_INTEGER_CONDITIONALS |
	 SGX_FEATURE_FLAGS2_MEMORY_LD_ST_COHERENT,													
	 8,													/* ui32NumMutexes */
	 16,												/* ui32NumMonitors */
	 4,													/* ui32TextureStateSize */
	 1,													/* ui32IterationStateSize */
	 4,													/* ui32NumUSEPipes */
	 20,												/* ui32NumProgramCounterBits */
	 3,														/* ui32NumInternalRegisters */
	 &g_uVecSpecialRegistersInvalidForNonBitwiseCount,	/* uInvalidSpecialRegistersForNonBitwiseCount */
	 g_auVecSpecialRegistersInvalidForNonBitwise,		/* auInvalidSpecialRegistersForNonBitwise */
	 "SGX543",											/* pszName */
};
#endif

#if defined(SUPPORT_SGX544)

	/* SGX544 */
static const SGX_CORE_FEATURES g_sSGX544_Features_Table =
{
	 SGX_FEATURE_FLAGS_USE_NO_INSTRUCTION_PAIRING |		/* ui32Flags */
	 SGX_FEATURE_FLAGS_USE_BRANCH_PWAIT |				
	 SGX_FEATURE_FLAGS_TAG_RAWSAMPLE |
	 SGX_FEATURE_FLAGS_TAG_RAWSAMPLEBOTH |
	 SGX_FEATURE_FLAGS_USE_VEC34 |
	 SGX_FEATURE_FLAGS_USE_UNLIMITED_PHASES |
	 SGX_FEATURE_FLAGS_USE_32BIT_INT_MAD |
	 SGX_FEATURE_FLAGS_TAG_UNPACK_RESULT |
	 SGX_FEATURE_FLAGS_USE_UNIFIED_TEMPS_AND_PAS |
	 SGX_FEATURE_FLAGS_USE_NOPTRIGGER |
	 SGX_FEATURE_FLAGS_USE_BRANCH_EXCEPTION |
	 SGX_FEATURE_FLAGS_USE_BRANCH_EXTSYNCEND,			
	 SGX_FEATURE_FLAGS2_USE_LDST_QWORD |				/* ui32Flags2 */
	 SGX_FEATURE_FLAGS2_USE_IDXSC |
	 SGX_FEATURE_FLAGS2_USE_TEST_SABLND |
	 SGX_FEATURE_FLAGS2_USE_LDRSTR_REPEAT |
	 SGX_FEATURE_FLAGS2_USE_STR_PREDICATE |
	 SGX_FEATURE_FLAGS2_USE_LDRSTR_EXTENDED_IMMEDIATE |
	 SGX_FEATURE_FLAGS2_USE_TWO_INDEX_BANKS |
	 SGX_FEATURE_FLAGS2_USE_SPRVV |
	 SGX_FEATURE_FLAGS2_USE_LDST_EXTENDED_CACHE_MODES |
	 SGX_FEATURE_FLAGS2_USE_SMP_RESULT_FORMAT_CONVERT |
	 SGX_FEATURE_FLAGS2_USE_SMP_REDUCEDREPEATCOUNT |
	 SGX_FEATURE_FLAGS2_USE_IMA32_32X16_PLUS_32 |
	 SGX_FEATURE_FLAGS2_USE_C10U8_CHAN_ORDER_RGBA |
	 SGX_FEATURE_FLAGS2_TAG_UNPACK_RESULT_TO_F16 |
	 SGX_FEATURE_FLAGS2_TAG_UNPACK_RESULT_TO_F32 |
	 SGX_FEATURE_FLAGS2_USE_TEST_SUB32 |
	 SGX_FEATURE_FLAGS2_USE_BITWISE_NO_REPEAT_MASK |
	 SGX_FEATURE_FLAGS2_USE_SMP_NO_REPEAT_MASK |
	 SGX_FEATURE_FLAGS2_USE_IDF_SUPPORTS_SKIPINVALID |
	 SGX_FEATURE_FLAGS2_USE_BRANCH_ALL_ANY_INSTANCES |
	 SGX_FEATURE_FLAGS2_USE_INTEGER_CONDITIONALS |
	 SGX_FEATURE_FLAGS2_USE_LD_ST_ATOMIC_OPS |
	 SGX_FEATURE_FLAGS2_MEMORY_LD_ST_COHERENT |
	 SGX_FEATURE_FLAGS2_TAG_VOLUME_TEXTURES,													
	 8,												/* ui32NumMutexes */
	 16,												/* ui32NumMonitors */
	 4,													/* ui32TextureStateSize */
	 1,													/* ui32IterationStateSize */
	 4,													/* ui32NumUSEPipes */
	 20,												/* ui32NumProgramCounterBits */
	 3,													/* ui32NumInternalRegisters */
	 &g_uVecSpecialRegistersInvalidForNonBitwiseCount,	/* uInvalidSpecialRegistersForNonBitwiseCount */
	 g_auVecSpecialRegistersInvalidForNonBitwise,		/* auInvalidSpecialRegistersForNonBitwise */
	 "SGX544",											/* pszName */
};
#endif

#if defined(SUPPORT_SGX545)

	/* SGX545 */
static const SGX_CORE_FEATURES g_sSGX545_Features_Table =
{
	SGX_FEATURE_FLAGS_USE_NO_INSTRUCTION_PAIRING |			/* ui32Flags */
	SGX_FEATURE_FLAGS_USE_PER_INST_MOE_INCREMENTS |
	SGX_FEATURE_FLAGS_USE_FCLAMP |
	SGX_FEATURE_FLAGS_USE_SQRT_SIN_COS |
	SGX_FEATURE_FLAGS_USE_32BIT_INT_MAD |
	SGX_FEATURE_FLAGS_USE_INT_DIV |
	SGX_FEATURE_FLAGS_USE_DUAL_ISSUE |
	SGX_FEATURE_FLAGS_USE_EXTENDED_LOAD |
	SGX_FEATURE_FLAGS_USE_NEW_EFO_OPTIONS |
	SGX_FEATURE_FLAGS_USE_PACK_MULTIPLE_ROUNDING_MODES |
	SGX_FEATURE_FLAGS_USE_NO_INSTRUCTION_PAIRING |
	SGX_FEATURE_FLAGS_USE_UNLIMITED_PHASES |
	SGX_FEATURE_FLAGS_USE_ALPHATOCOVERAGE |
	SGX_FEATURE_FLAGS_USE_FMAD16_SWIZZLES |
	SGX_FEATURE_FLAGS_TAG_UNPACK_RESULT |
	SGX_FEATURE_FLAGS_TAG_PCF |
	SGX_FEATURE_FLAGS_TAG_RAWSAMPLE |
	SGX_FEATURE_FLAGS_USE_NOPTRIGGER |
	SGX_FEATURE_FLAGS_USE_BRANCH_EXCEPTION |
	SGX_FEATURE_FLAGS_USE_BRANCH_EXTSYNCEND |
	SGX_FEATURE_FLAGS_USE_LOAD_MOE_FROM_REGISTERS |
	SGX_FEATURE_FLAGS_USE_STORE_MOE_TO_REGISTERS |
	SGX_FEATURE_FLAGS_USE_CFI |
	SGX_FEATURE_FLAGS_VCB |
	SGX_FEATURE_FLAGS_EXTERNAL_LDST_SMP_UNIT |
	SGX_FEATURE_FLAGS_TAG_GAMMA_UNPACK_TO_F16 |
	SGX_FEATURE_FLAGS_TAG_GRAD_SAMPLE_IN_DFC,
	SGX_FEATURE_FLAGS2_USE_SMP_REDUCEDREPEATCOUNT |			/* ui32Flags2 */
	SGX_FEATURE_FLAGS2_USE_LDST_EXTENDED_CACHE_MODES |	
	SGX_FEATURE_FLAGS2_USE_ST_NO_READ_BEFORE_WRITE |
	SGX_FEATURE_FLAGS2_TAG_UNPACK_RESULT_TO_C10 |
	SGX_FEATURE_FLAGS2_TAG_UNPACK_RESULT_TO_F16 |
	SGX_FEATURE_FLAGS2_TAG_UNPACK_RESULT_TO_F32 |
	SGX_FEATURE_FLAGS2_TAG_UNPACK_RESULT_TO_UNORM |
	SGX_FEATURE_FLAGS2_TAG_UNPACK_RESULT_TO_LOWERPREC |
	SGX_FEATURE_FLAGS2_USE_BUFFER_LOAD |
	SGX_FEATURE_FLAGS2_TAG_1D_2D_IMMEDIATE_COORDINATE_OFFSETS |
	SGX_FEATURE_FLAGS2_TAG_NOT_RAWSAMPLEBOTH |
	SGX_FEATURE_FLAGS2_MEMORY_LD_ST_COHERENT |
	SGX_FEATURE_FLAGS2_TAG_VOLUME_TEXTURES |
	SGX_FEATURE_FLAGS2_USE_SAMPLE_NUMBER_SPECIAL_REGISTER,		
	8,														/* ui32NumMutexes */
	16,														/* ui32NumMonitors */
	4,														/* ui32TextureStateSize */
	2,														/* ui32IterationStateSize */
	4,														/* ui32NumUSEPipes */
	18,														/* ui32NumProgramCounterBits */
	3,														/* ui32NumInternalRegisters */
	&g_uNonVecSpecialRegistersInvalidForNonBitwiseCount,	/* uInvalidSpecialRegistersForNonBitwiseCount */
	g_auNonVecSpecialRegistersInvalidForNonBitwise,			/* auInvalidSpecialRegistersForNonBitwise */
	"SGX545",												/* pszName */
};
#endif

#if defined(SUPPORT_SGX554)

	/* SGX554 */
static const SGX_CORE_FEATURES g_sSGX554_Features_Table =
{
	 SGX_FEATURE_FLAGS_USE_NO_INSTRUCTION_PAIRING |		/* ui32Flags */
	 SGX_FEATURE_FLAGS_USE_BRANCH_PWAIT |				
	 SGX_FEATURE_FLAGS_TAG_RAWSAMPLE |
	 SGX_FEATURE_FLAGS_TAG_RAWSAMPLEBOTH |
	 SGX_FEATURE_FLAGS_USE_VEC34 |
	 SGX_FEATURE_FLAGS_USE_UNLIMITED_PHASES |
	 SGX_FEATURE_FLAGS_USE_32BIT_INT_MAD |
	 SGX_FEATURE_FLAGS_TAG_UNPACK_RESULT |
	 SGX_FEATURE_FLAGS_USE_UNIFIED_TEMPS_AND_PAS |
	 SGX_FEATURE_FLAGS_USE_NOPTRIGGER |
	 SGX_FEATURE_FLAGS_USE_BRANCH_EXCEPTION |
	 SGX_FEATURE_FLAGS_USE_BRANCH_EXTSYNCEND,			
	 SGX_FEATURE_FLAGS2_USE_LDST_QWORD |				/* ui32Flags2 */
	 SGX_FEATURE_FLAGS2_USE_IDXSC |
	 SGX_FEATURE_FLAGS2_USE_TEST_SABLND |
	 SGX_FEATURE_FLAGS2_USE_LDRSTR_REPEAT |
	 SGX_FEATURE_FLAGS2_USE_STR_PREDICATE |
	 SGX_FEATURE_FLAGS2_USE_LDRSTR_EXTENDED_IMMEDIATE |
	 SGX_FEATURE_FLAGS2_USE_TWO_INDEX_BANKS |
	 SGX_FEATURE_FLAGS2_USE_SPRVV |
	 SGX_FEATURE_FLAGS2_USE_LDST_EXTENDED_CACHE_MODES |
	 SGX_FEATURE_FLAGS2_USE_SMP_RESULT_FORMAT_CONVERT |
	 SGX_FEATURE_FLAGS2_USE_SMP_REDUCEDREPEATCOUNT |
	 SGX_FEATURE_FLAGS2_USE_IMA32_32X16_PLUS_32 |
	 SGX_FEATURE_FLAGS2_USE_C10U8_CHAN_ORDER_RGBA |
	 SGX_FEATURE_FLAGS2_TAG_UNPACK_RESULT_TO_F16 |
	 SGX_FEATURE_FLAGS2_TAG_UNPACK_RESULT_TO_F32 |
	 SGX_FEATURE_FLAGS2_USE_TEST_SUB32 |
	 SGX_FEATURE_FLAGS2_USE_BITWISE_NO_REPEAT_MASK |
	 SGX_FEATURE_FLAGS2_USE_SMP_NO_REPEAT_MASK |
	 SGX_FEATURE_FLAGS2_USE_IDF_SUPPORTS_SKIPINVALID |
	 SGX_FEATURE_FLAGS2_USE_BRANCH_ALL_ANY_INSTANCES |
	 SGX_FEATURE_FLAGS2_USE_INTEGER_CONDITIONALS |
	 SGX_FEATURE_FLAGS2_USE_LD_ST_ATOMIC_OPS |
	 SGX_FEATURE_FLAGS2_MEMORY_LD_ST_COHERENT,													
	 16,												/* ui32NumMutexes */
	 16,												/* ui32NumMonitors */
	 4,													/* ui32TextureStateSize */
	 1,													/* ui32IterationStateSize */
	 8,													/* ui32NumUSEPipes */
	 20,												/* ui32NumProgramCounterBits */
	 4,													/* ui32NumInternalRegisters */
	 &g_uVecSpecialRegistersInvalidForNonBitwiseCount,	/* uInvalidSpecialRegistersForNonBitwiseCount */
	 g_auVecSpecialRegistersInvalidForNonBitwise,		/* auInvalidSpecialRegistersForNonBitwise */
	 "SGX554",											/* pszName */
};
#endif

/********************************************************************************
	Tables for mapping SGX cores/core revisions to USE bugs/features.
 ********************************************************************************/
#include "errata.h"

/*****************************************************************************
 FUNCTION	: UseAsmGetCoreDesc

 PURPOSE	: Gets the core description table entry corresponding to a particular
			  target core and revision..

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: A pointer to the table entry.
*****************************************************************************/
IMG_INTERNAL
PCSGX_CORE_DESC IMG_CALLCONV UseAsmGetCoreDesc(PSGX_CORE_INFO psTarget)
{
	IMG_UINT32	uEntry;

	for (uEntry = 0; uEntry < (sizeof(g_sUseasmCoreTable) / sizeof(g_sUseasmCoreTable[0])); uEntry++)
	{
		PCSGX_CORE_DESC	psEntry = &g_sUseasmCoreTable[uEntry];
		if (
				/*
					Look for an entry matching this core
				*/
				psEntry->eCoreType == psTarget->eID &&
				psEntry->ui32CoreRevision == psTarget->uiRev
		   )
		{
			return psEntry;
		}
	}	
	return NULL;
}

/*****************************************************************************
 FUNCTION	: IsRevisionValid

 PURPOSE	: Checks if valid for this processor.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL IsRevisionValid(PSGX_CORE_INFO psTarget)
{
	return (IMG_BOOL)((psTarget->eID != SGX_CORE_ID_INVALID) && UseAsmGetCoreDesc(psTarget));
}

/*****************************************************************************
 FUNCTION	: FixBRN21697

 PURPOSE	: Checks if this processor is affected by BRN21697.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FixBRN21697(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->sBugs.ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_21697) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: FixBRN21713

 PURPOSE	: Checks if this processor is affected by BRN21713.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FixBRN21713(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->sBugs.ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_21713) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: FixBRN23062

 PURPOSE	: Checks if this processor is affected by BRN23062.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FixBRN23062(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->sBugs.ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_23062) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: FixBRN23164

 PURPOSE	: Checks if this processor is affected by BRN23164.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FixBRN23164(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->sBugs.ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_23164) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: FixBRN23461

 PURPOSE	: Checks if this processor is affected by BRN23461.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FixBRN23461(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->sBugs.ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_23461) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: FixBRN23960

 PURPOSE	: Checks if this processor is affected by BRN23960.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FixBRN23960(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->sBugs.ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_23960) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: FixBRN24895

 PURPOSE	: Checks if this processor is affected by BRN24895.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FixBRN24895(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->sBugs.ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_24895) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: FixBRN25355

 PURPOSE	: Checks if this processor is affected by BRN25355.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FixBRN25355(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->sBugs.ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_25355) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: FixBRN25060

 PURPOSE	: Checks if this processor is affected by BRN25060.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FixBRN25060(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->sBugs.ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_25060) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: FixBRN25580

 PURPOSE	: Checks if this processor is affected by BRN25580.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FixBRN25580(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->sBugs.ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_25580) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: FixBRN25582

 PURPOSE	: Checks if this processor is affected by BRN25580.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FixBRN25582(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->sBugs.ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_25582) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: FixBRN26570

 PURPOSE	: Checks if this processor is affected by BRN26570.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FixBRN26570(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->sBugs.ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_26570) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: FixBRN27005

 PURPOSE	: Checks if this processor is affected by BRN27005.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FixBRN27005(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->sBugs.ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_27005) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: FixBRN26681

 PURPOSE	: Checks if this processor is affected by BRN26681.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FixBRN26681(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->sBugs.ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_26681) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: FixBRN27984

 PURPOSE	: Checks if this processor is affected by BRN27984.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FixBRN27984(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->sBugs.ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_27984) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: FixBRN29643

 PURPOSE	: Checks if this processor is affected by BRN29643.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FixBRN29643(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->sBugs.ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_29643) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: FixBRN230871

 PURPOSE	: Checks if this processor is affected by BRN30871.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FixBRN30871(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->sBugs.ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_30871) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: FixBRN230898

 PURPOSE	: Checks if this processor is affected by BRN30898.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FixBRN30898(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->sBugs.ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_30898) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: FixBRN33442

 PURPOSE	: Checks if this processor is affected by BRN33442.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FixBRN33442(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->sBugs.ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_33442) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: IsEnhancedNoSched

 PURPOSE	: Checks if this processor has more instructions which support
			  the NOSCHED flag.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 IsEnhancedNoSched(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NO_INSTRUCTION_PAIRING) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: IsHighPrecisionFIR

 PURPOSE	: Checks if high precision FIR extensions are available on this processor.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL IsHighPrecisionFIR(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_HIGH_PRECISION_FIR) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: IsLoadMOEStateFromRegisters

 PURPOSE	: Checks if this processor can load the MOE state from registers.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL IsLoadMOEStateFromRegisters(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_LOAD_MOE_FROM_REGISTERS) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: IsPerInstMoeIncrements

 PURPOSE	: Checks if this processor can specify the MOE increments in the
			  instructions for some opcodes.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL IsPerInstMoeIncrements(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_PER_INST_MOE_INCREMENTS) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: IsFClampInstruction

 PURPOSE	: Checks if this processor supports the FCLAMP instruction.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL IsFClampInstruction(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_FCLAMP) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: IsSqrtSinCosInstruction

 PURPOSE	: Checks if this processor supports the SQRT, SIN and COS instructions.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL IsSqrtSinCosInstruction(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_SQRT_SIN_COS) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: IsIMA32Instruction

 PURPOSE	: Checks if this processor supports the IMA32 instructions.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL IsIMA32Instruction(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_32BIT_INT_MAD) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: IsIDIVInstruction

 PURPOSE	: Checks if this processor supports the IDIV instructions.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL IsIDIVInstruction(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_INT_DIV) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsExtendedLoad

 PURPOSE	: Checks if this processor supports the extended load instruction.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsExtendedLoad(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_EXTENDED_LOAD) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsLDSTExtendedCacheModes

 PURPOSE	: Checks if this core supports the extended caching options on the
			  LD/ST instruction.

 PARAMETERS	: eTarget			- Target core.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsLDSTExtendedCacheModes(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_LDST_EXTENDED_CACHE_MODES) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsSTNoReadBeforeWrite

 PURPOSE	: Checks if this core supports the no-read-before-write flag on
			  ST.

 PARAMETERS	: eTarget			- Target core.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsSTNoReadBeforeWrite(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_ST_NO_READ_BEFORE_WRITE) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsSMPResultFormatConvert

 PURPOSE	: Checks if this core supports specifying a format conversion
			  for a texture sample result as part of the SMP instruction
			  encoding.

 PARAMETERS	: eTarget			- Target core.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsSMPResultFormatConvert(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_SMP_RESULT_FORMAT_CONVERT) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: HasNewEfoOptions

 PURPOSE	: Checks if this processor uses the new EFO adder/multipler options.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HasNewEfoOptions(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NEW_EFO_OPTIONS) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: HasPackMultipleRoundModes

 PURPOSE	: Checks if this processor supports multiple rounding modes on the
			  PACK instruction.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HasPackMultipleRoundModes(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_PACK_MULTIPLE_ROUNDING_MODES) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: HasDualIssue

 PURPOSE	: Checks if this processor supports the dual issue instruction.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HasDualIssue(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_DUAL_ISSUE) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: HasUnlimitedPhases

 PURPOSE	: Checks if this processor supports the PHAS instruction.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HasUnlimitedPhases(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_UNLIMITED_PHASES) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: HasNoInstructionPairing

 PURPOSE	: Checks if this processor executes instructions in pairs.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HasNoInstructionPairing(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NO_INSTRUCTION_PAIRING) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: NumberOfMutexesSupported

 PURPOSE	: Get the number of mutexes supported on this processor.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: The count of mutexes supported.
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 NumberOfMutexesSupported(PCSGX_CORE_DESC psTarget)
{
	return psTarget->psFeatures->ui32NumMutexes;
}

/*****************************************************************************
 FUNCTION	: NumberOfMonitorsSupported

 PURPOSE	: Get the number of monitors supported on this processor.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: The count of monitors supported.
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 NumberOfMonitorsSupported(PCSGX_CORE_DESC psTarget)
{
	return psTarget->psFeatures->ui32NumMonitors;
}

/*****************************************************************************
 FUNCTION	: NumberOfProgramCounterBits

 PURPOSE	: Get the size of the program counter supported on this processor.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: The size of the program counter.
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 NumberOfProgramCounterBits(PCSGX_CORE_DESC psTarget)
{
	return psTarget->psFeatures->ui32NumProgramCounterBits;
}


/*****************************************************************************
 FUNCTION	: NumberOfInternalRegisters

 PURPOSE	: Get the number of internal registers supported on this core.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: The number of internal registers.
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 NumberOfInternalRegisters(PCSGX_CORE_DESC psTarget)
{
	return psTarget->psFeatures->ui32NumInternalRegisters;
}

/*****************************************************************************
 FUNCTION	: SupportsAlphaToCoverage

 PURPOSE	: Checks if this processor supports alpha-to-coverage.

 PARAMETERS	: eTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsAlphaToCoverage(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_ALPHATOCOVERAGE) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsFmad16Swizzles

 PURPOSE	: Checks if this processor supports swizzles on the fmad16 instruction.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsFmad16Swizzles(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_FMAD16_SWIZZLES) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsFnormalise

 PURPOSE	: Checks if this processor supports the fnormalise instruction.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsFnormalise(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_FNORMALISE) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsPCF

 PURPOSE	: Checks if this processor supports the PCF when using the SMP
			  instruction.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsPCF(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_TAG_PCF) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsRawSample

 PURPOSE	: Checks if this processor supports returning sample parameters when using the SMP
			  instruction.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsRawSample(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_TAG_RAWSAMPLE) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsNopTrigger

 PURPOSE	: Checks if this processor supports a debug bus trigger on the
			  NOP instruction.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsNopTrigger(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NOPTRIGGER) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsBranchException

 PURPOSE	: Checks if this processor supports the exception check flag on
			  the branch instruction.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsBranchException(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_BRANCH_EXCEPTION) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsExtSyncEnd

 PURPOSE	: Checks if this processor supports the extended syncend modes.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsExtSyncEnd(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_BRANCH_EXTSYNCEND) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsMOEStore

 PURPOSE	: Checks if this processor supports storing the MOE state.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsMOEStore(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_STORE_MOE_TO_REGISTERS) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsCFI

 PURPOSE	: Checks if this processor supports the CFI instruction.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsCFI(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_CFI) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsVCB

 PURPOSE	: Checks if this processor supports the VCB.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsVCB(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_VCB) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsVEC34

 PURPOSE	: Checks if this processor supports the VEC34 instructions.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsVEC34(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsBranchPwait

 PURPOSE	: Checks if this processor supports the pwait flag on
			  the branch instruction.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsBranchPwait(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_BRANCH_PWAIT) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsRawSampleBoth

 PURPOSE	: Checks if this processor supports returning sample parameters when using the SMP
			  instruction.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsRawSampleBoth(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags & SGX_FEATURE_FLAGS_TAG_RAWSAMPLEBOTH) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: ReducedSMPRepeatCount

 PURPOSE	: Checks if the maximum repeat count on the SMP instruction is
			  reduced on this processor.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL ReducedSMPRepeatCount(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_SMP_REDUCEDREPEATCOUNT) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: IMA32Is32x16Plus32

 PURPOSE	: Check if IMA32 does a 32x16+32 multiply.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL IMA32Is32x16Plus32(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_IMA32_32X16_PLUS_32) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsTESTSUB32

 PURPOSE	: Checks if the SUB32 and SUBU32 ALU opcodes are supported on the TEST instruction.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsTESTSUB32(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_TEST_SUB32) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: NoRepeatMaskOnBitwiseInstructions

 PURPOSE	: Checks if a repeat mask is supported on bitwise instructions.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL NoRepeatMaskOnBitwiseInstructions(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_BITWISE_NO_REPEAT_MASK) ? IMG_TRUE : IMG_FALSE;
}


/*****************************************************************************
 FUNCTION	: NoRepeatMaskOnSMPInstructions

 PURPOSE	: Checks if a repeat mask is supported on texture sample instructions.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL NoRepeatMaskOnSMPInstructions(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_SMP_NO_REPEAT_MASK) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportSkipInvalidOnIDFInstruction

 PURPOSE	: Check if the IDF instruction supports the SKIPINVALID flag.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportSkipInvalidOnIDFInstruction(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_IDF_SUPPORTS_SKIPINVALID) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsAllAnyInstancesFlagOnBranchInstruction

 PURPOSE	: Check if the branch instruction supports the ALLINST/ANYINST flag.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsAllAnyInstancesFlagOnBranchInstruction(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_BRANCH_ALL_ANY_INSTANCES) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsLDSTAtomicOps

 PURPOSE	: Check if the LD/ST instructions support atomic operations.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsLDSTAtomicOps(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_LD_ST_ATOMIC_OPS) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsIntegerConditionalInstructions

 PURPOSE	: Check if the core supports the integer conditional instructions.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsIntegerConditionalInstructions(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_INTEGER_CONDITIONALS) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsQwordLDST

 PURPOSE	: Checks if the LD/ST instruction supports a qword data type.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsQwordLDST(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_LDST_QWORD) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsIDXSC

 PURPOSE	: Checks if the IDXSCR/IDXSCW instructions are supported.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsIDXSC(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_IDXSC) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsTESTSABLND

 PURPOSE	: Checks if the SABLND ALU opcode is supported on the TEST instruction.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsTESTSABLND(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_TEST_SABLND) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsLDRSTRRepeat

 PURPOSE	: Checks if the LDR and STR instructions support a repeat count.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsLDRSTRRepeat(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_LDRSTR_REPEAT) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsSTRPredicate

 PURPOSE	: Checks if the STR instruction supports predicate.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsSTRPredicate(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_STR_PREDICATE) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsLDRSTRExtendedImmediate

 PURPOSE	: Checks if the LDR/STR instructions supports larger immediate values.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsLDRSTRExtendedImmediate(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_LDRSTR_EXTENDED_IMMEDIATE) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsIndexTwoBanks

 PURPOSE	: Checks if this core uses the new index register bank scheme.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsIndexTwoBanks(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_TWO_INDEX_BANKS) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SupportsSPRVV

 PURPOSE	: Checks if this core supports the SPRVV instruction.

 PARAMETERS	: psTarget			- Target processor.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL SupportsSPRVV(PCSGX_CORE_DESC psTarget)
{
	return (psTarget->psFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_SPRVV) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: IsInvalidSpecialRegisterForNonBitwiseInstructions

 PURPOSE	: Checks for a special register which is invalid as a source to
			  non-bitwise instructions on this core.

 PARAMETERS	: psTarget			- Target processor.
			  uRegNum			- Register number to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL IsInvalidSpecialRegisterForNonBitwiseInstructions(PCSGX_CORE_DESC psTarget, IMG_UINT32 uRegNum)
{
	SGX_CORE_FEATURES const *	psTargetFeatures = psTarget->psFeatures;
	IMG_UINT32					uRegIdx;
	IMG_UINT32					uRegCount;

	uRegCount = *psTargetFeatures->puInvalidSpecialRegistersForNonBitwiseCount;
	for (uRegIdx = 0; uRegIdx < uRegCount; uRegIdx++)
	{
		if (psTargetFeatures->auInvalidSpecialRegistersForNonBitwise[uRegIdx] == uRegNum)
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: UseAsmInitInst

 PURPOSE	: Initialise an instruction structure to reasonable defaults

 PARAMETERS	: psInst	- Structure to initialise

 RETURNS	: Nothing
*****************************************************************************/
IMG_INTERNAL 
IMG_VOID UseAsmInitInst(PUSE_INST psInst)
{
	UseAsm_MemSet(psInst, 0, sizeof(*psInst));

	psInst->uOpcode = USEASM_OP_UNDEF;
	psInst->uTest = USEASM_TEST_MASK_NONE;
}

/*****************************************************************************
 FUNCTION	: UseAsmInitReg

 PURPOSE	: Initialise a register structure to reasonable defaults

 PARAMETERS	: psReg	- Structure to initialise

 RETURNS	: Nothing
*****************************************************************************/
IMG_INTERNAL 
IMG_VOID UseAsmInitReg(PUSE_REGISTER psReg)
{
	UseAsm_MemSet(psReg, 0, sizeof(*psReg));

	psReg->uType = USEASM_REGTYPE_UNDEF;
	psReg->uIndex = USEREG_INDEX_NONE;
}

/******************************************************************************
 End of file (utils.c)
******************************************************************************/
