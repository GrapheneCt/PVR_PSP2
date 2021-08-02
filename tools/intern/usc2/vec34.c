/*************************************************************************
 * Name         : vec34.c
 * Title        : Pixel Shader Compiler
 * Author       : John Russell
 * Created      : Jan 2002
 *
 * Copyright    : 2002-2006 by Imagination Technologies Limited. All rights reserved.
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
 *
 * Modifications:-
 * $Log: vec34.c $
 * 	{
 *  --- Revision Logs Removed --- 
 * 
 *  --- Revision Logs Removed --- 
 * 
 * or ones which also copy data from another register.
 *  --- Revision Logs Removed --- 
 *  
 * Increase the cases where a vec4 instruction can be split into two vec2
 * parts.
 *  --- Revision Logs Removed --- 
 * 
 *  --- Revision Logs Removed --- -iftikhar.ahmad
 **************************************************************************/

#include "uscshrd.h"
#include "bitops.h"
#include "usedef.h"

#include "usetab.h"

#include "sgxformatconvert.h"

/* There is a bug in the microsoft compiler which restricts the number of characters
before a "#if defined". This line is just to avoid this bug. */
static const IMG_BOOL bUnusedGlobal = IMG_FALSE;

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)

typedef struct _SWIZZLE_SET
{
	IMG_UINT32			uSubsetCount;
	struct
	{
		IMG_UINT32			uCount;
		IMG_UINT32 const*	puList;
	} asSubset[2];
} SWIZZLE_SET, *PSWIZZLE_SET;
typedef SWIZZLE_SET const * PCSWIZZLE_SET;

#define DECL_SWIZZLE_SET1(SET)				\
{											\
	1	/* uCount */,						\
	{										\
		{									\
			sizeof(SET) / sizeof(SET[0]),	\
			SET,							\
		}									\
	},										\
}

#define DECL_SWIZZLE_SET2(SET1, SET2)		\
{											\
	2	/* uCount */,						\
	{										\
		{									\
			sizeof(SET1) / sizeof(SET1[0]),	\
			SET1,							\
		},									\
		{									\
			sizeof(SET2) / sizeof(SET2[0]),	\
			SET2,							\
		},									\
	},										\
}

/*
	Maps a nibble to the count of leading zeros in its bit pattern.
*/
static const IMG_UINT32	g_auNumLeadingZeros[] =
{
	4,		/* 0 */
	0,		/* 1 */
	1,		/* 2 */
	0,		/* 3 */
	2,		/* 4 */
	0,		/* 5 */
	1,		/* 6 */
	0,		/* 7 */
	3,		/* 8 */
	0,		/* 9 */
	1,		/* 10 */
	0,		/* 11 */
	2,		/* 12 */
	0,		/* 13 */
	1,		/* 14 */
	0,		/* 15 */
};

static const IMG_BOOL g_abUnusedChannels[] = 
{
	IMG_FALSE,		/* 0 */
	IMG_FALSE,		/* 1 */
	IMG_TRUE,		/* 2 */
	IMG_FALSE,		/* 3 */
	IMG_TRUE,		/* 4 */
	IMG_TRUE,		/* 5 */
	IMG_TRUE,		/* 6 */
	IMG_FALSE,		/* 7 */
	IMG_TRUE,		/* 8 */
	IMG_TRUE,		/* 9 */
	IMG_TRUE,		/* 10 */
	IMG_TRUE,		/* 11 */
	IMG_TRUE,		/* 12 */
	IMG_TRUE,		/* 13 */
	IMG_TRUE,		/* 14 */
	IMG_FALSE,		/* 15 */
};

#define USEASM_SWIZZLE3(SELX, SELY, SELZ)	USEASM_SWIZZLE(SELX, SELY, SELZ, X)

/*
	Identity swizzle. Used when a source doesn't support swizzles at all.
*/
static const IMG_UINT32 g_auIdentitySwizzle[] =
{
	USEASM_SWIZZLE(X, Y, Z, W),
};
static const SWIZZLE_SET g_sIdentitySwizzle = DECL_SWIZZLE_SET1(g_auIdentitySwizzle);

/*
	Swizzles supported on TEST instruction source 1 when selecting multiple channels to
	generate the predicate result.
*/
static const IMG_UINT32 g_auVTESTSrc1Swizzles[] =
{
	USEASM_SWIZZLE(X, Y, Z, W),
};
static const SWIZZLE_SET g_sVTESTSrc1Swizzles = DECL_SWIZZLE_SET1(g_auVTESTSrc1Swizzles);

/*
	Swizzles supported on TEST instruction source 1 when selecting a single channel to
	generate the predicate result.
*/
static const IMG_UINT32 g_auVTESTSrc1Swizzles_OneChan[] =
{
	USEASM_SWIZZLE(X, X, X, X),
	USEASM_SWIZZLE(Y, Y, Y, Y),
	USEASM_SWIZZLE(Z, Z, Z, Z),
	USEASM_SWIZZLE(W, W, W, W),
};
static const SWIZZLE_SET g_sVTESTSrc1Swizzles_OneChan = DECL_SWIZZLE_SET1(g_auVTESTSrc1Swizzles_OneChan);

/*
	Swizzles supported on TEST instruction source 2.
*/
static const IMG_UINT32 g_auVTESTSrc2Swizzles[] = 
{
	USEASM_SWIZZLE(X, Y, Z, W),
	USEASM_SWIZZLE(X, X, X, X),
};
static const SWIZZLE_SET g_sVTESTSrc2Swizzles = DECL_SWIZZLE_SET1(g_auVTESTSrc2Swizzles);

/*
	VEC3 standard swizzle set.
*/
static const IMG_UINT32 g_auVEC3StdSwizzles[] =
{
	USEASM_SWIZZLE3(X, X, X),
	USEASM_SWIZZLE3(Y, Y, Y),
	USEASM_SWIZZLE3(Z, Z, Z),
	USEASM_SWIZZLE3(W, W, W),
	USEASM_SWIZZLE3(X, Y, Z),
	USEASM_SWIZZLE3(Y, Z, W),
	USEASM_SWIZZLE3(X, X, Y),
	USEASM_SWIZZLE3(X, Y, X),
	USEASM_SWIZZLE3(Y, Y, X),
	USEASM_SWIZZLE3(Y, Y, Z),
	USEASM_SWIZZLE3(Z, X, Y),
	USEASM_SWIZZLE3(X, Z, Y),
	USEASM_SWIZZLE3(Y, Z, X),
	USEASM_SWIZZLE3(Z, Y, X),
	USEASM_SWIZZLE3(Z, Z, Y),
	USEASM_SWIZZLE3(X, Y, 1),
};
static const SWIZZLE_SET g_sVEC3StdSwizzles = DECL_SWIZZLE_SET1(g_auVEC3StdSwizzles);

/*
	VEC3 extended swizzle set.
*/
static const IMG_UINT32 g_auVEC3ExtSwizzles[] =
{
	USEASM_SWIZZLE3(X, Y, Y),
	USEASM_SWIZZLE3(Y, X, Y),
	USEASM_SWIZZLE3(X, X, Z),
	USEASM_SWIZZLE3(Y, X, X),
	USEASM_SWIZZLE3(X, Y, 0),
	USEASM_SWIZZLE3(X, 1, 0),
	USEASM_SWIZZLE3(0, 0, 0),
	USEASM_SWIZZLE3(1, 1, 1),
	USEASM_SWIZZLE3(HALF, HALF, HALF),
	USEASM_SWIZZLE3(2, 2, 2),
};
static const SWIZZLE_SET g_sVEC3StdAndExtSwizzles = DECL_SWIZZLE_SET2(g_auVEC3StdSwizzles, g_auVEC3ExtSwizzles);

/*
	VEC4 standard swizzle set.
*/
static const IMG_UINT32 g_auVEC4StdSwizzles[] =
{
	USEASM_SWIZZLE(X, X, X, X),
	USEASM_SWIZZLE(Y, Y, Y, Y),
	USEASM_SWIZZLE(Z, Z, Z, Z),
	USEASM_SWIZZLE(W, W, W, W),
	USEASM_SWIZZLE(X, Y, Z, W),
	USEASM_SWIZZLE(Y, Z, W, W),
	USEASM_SWIZZLE(X, Y, Z, Z),
	USEASM_SWIZZLE(X, X, Y, Z),
	USEASM_SWIZZLE(X, Y, X, Y),
	USEASM_SWIZZLE(X, Y, W, Z),
	USEASM_SWIZZLE(Z, X, Y, W),
	USEASM_SWIZZLE(Z, W, Z, W),
	USEASM_SWIZZLE(Y, Z, X, Z),
	USEASM_SWIZZLE(X, X, Y, Y),
	USEASM_SWIZZLE(X, Z, W, W),
	USEASM_SWIZZLE(X, Y, Z, 1),
};
static const SWIZZLE_SET g_sVEC4StdSwizzles = DECL_SWIZZLE_SET1(g_auVEC4StdSwizzles);

/*
	VEC4 extended swizzle set.
*/
static const IMG_UINT32 g_auVEC4ExtSwizzles[] =
{
	USEASM_SWIZZLE(Y, Z, X, W),
	USEASM_SWIZZLE(Z, W, X, Y),
	USEASM_SWIZZLE(X, Z, W, Y),
	USEASM_SWIZZLE(Y, Y, W, W),
	USEASM_SWIZZLE(W, Y, Z, W),
	USEASM_SWIZZLE(W, Z, W, Z),
	USEASM_SWIZZLE(X, Y, Z, X),
	USEASM_SWIZZLE(Z, Z, W, W),
	USEASM_SWIZZLE(X, W, Z, X),
	USEASM_SWIZZLE(Y, Y, Y, X),
	USEASM_SWIZZLE(Y, Y, Y, Z),
	USEASM_SWIZZLE(X, Z, Y, W),
	USEASM_SWIZZLE(X, X, X, Y),
	USEASM_SWIZZLE(Z, Y, X, W),
	USEASM_SWIZZLE(Y, Y, Z, Z),
	USEASM_SWIZZLE(Z, Z, Z, Y),
};
static const SWIZZLE_SET g_sVEC4StdAndExtSwizzles = DECL_SWIZZLE_SET2(g_auVEC4StdSwizzles, g_auVEC4ExtSwizzles);

/*
	Swizzles supported on VMAD/VMADF16 source 0.
*/
IMG_UINT32 static const g_auVMADSrc0Swizzles[] =
{
	USEASM_SWIZZLE(X, X, X, X),
	USEASM_SWIZZLE(Y, Y, Y, Y),
	USEASM_SWIZZLE(Z, Z, Z, Z),
	USEASM_SWIZZLE(W, W, W, W),
	USEASM_SWIZZLE(X, Y, Z, W),
	USEASM_SWIZZLE(Y, Z, X, W),
	USEASM_SWIZZLE(X, Y, W, W),
	USEASM_SWIZZLE(Z, W, X, Y),
};
static const SWIZZLE_SET g_sVMADSrc0Swizzles = DECL_SWIZZLE_SET1(g_auVMADSrc0Swizzles);

/*
	Swizzles supported on VMAD/VMADF16 source 1.
*/
IMG_UINT32 static const g_auVMADSrc1Swizzles[] = 
{
	USEASM_SWIZZLE(X, X, X, X),
	USEASM_SWIZZLE(Y, Y, Y, Y),
	USEASM_SWIZZLE(Z, Z, Z, Z),
	USEASM_SWIZZLE(W, W, W, W),
	USEASM_SWIZZLE(X, Y, Z, W),
	USEASM_SWIZZLE(X, Y, Y, Z),
	USEASM_SWIZZLE(Y, Y, W, W),
	USEASM_SWIZZLE(W, Y, Z, W),
};
static const SWIZZLE_SET g_sVMADSrc1Swizzles = DECL_SWIZZLE_SET1(g_auVMADSrc1Swizzles);

/*
	Swizzles supported on VMAD/VMADF16 source 2.
*/
IMG_UINT32 static const g_auVMADSrc2Swizzles[] = 
{
	USEASM_SWIZZLE(X, X, X, X),
	USEASM_SWIZZLE(Y, Y, Y, Y),
	USEASM_SWIZZLE(Z, Z, Z, Z),
	USEASM_SWIZZLE(W, W, W, W),
	USEASM_SWIZZLE(X, Y, Z, W),
	USEASM_SWIZZLE(X, Z, W, W),
	USEASM_SWIZZLE(X, X, Y, Z),
	USEASM_SWIZZLE(X, Y, Z, Z),
};
static const SWIZZLE_SET g_sVMADSrc2Swizzles = DECL_SWIZZLE_SET1(g_auVMADSrc2Swizzles);

/*
	VEC3 dual-issue instruction standard swizzle set.
*/
IMG_UINT32 static const g_auVec3DualIssueStandardSwizzles[] =
{
	USEASM_SWIZZLE3(X, X, X),
	USEASM_SWIZZLE3(Y, Y, Y),
	USEASM_SWIZZLE3(Z, Z, Z),
	USEASM_SWIZZLE3(W, W, W),
	USEASM_SWIZZLE3(X, Y, Z),
	USEASM_SWIZZLE3(Y, Z, W),
	USEASM_SWIZZLE3(X, X, Y),
	USEASM_SWIZZLE3(X, Y, X),
	USEASM_SWIZZLE3(Y, Y, X),
	USEASM_SWIZZLE3(Y, Y, Z),
	USEASM_SWIZZLE3(Z, X, Y),
	USEASM_SWIZZLE3(X, Z, Y),
	USEASM_SWIZZLE3(0, 0, 0),
	USEASM_SWIZZLE3(HALF, HALF, HALF),
	USEASM_SWIZZLE3(1, 1, 1),
	USEASM_SWIZZLE3(2, 2, 2),
};
static const SWIZZLE_SET g_sVec3DualIssueStandardSwizzles = DECL_SWIZZLE_SET1(g_auVec3DualIssueStandardSwizzles);

/*
	VEC3 dual-issue instruction extended swizzle set.
*/
IMG_UINT32 static const g_auVec3DualIssueExtendedSwizzles[] =
{
	USEASM_SWIZZLE3(X, Y, Y),
	USEASM_SWIZZLE3(Y, X, Y),
	USEASM_SWIZZLE3(X, X, Z),
	USEASM_SWIZZLE3(Y, X, X),
	USEASM_SWIZZLE3(X, Y, 0),
	USEASM_SWIZZLE3(X, 1, 0),
	USEASM_SWIZZLE3(X, Z, Y),
	USEASM_SWIZZLE3(Y, Z, X),
	USEASM_SWIZZLE3(Z, Y, X),
	USEASM_SWIZZLE3(Z, Z, Y),
	USEASM_SWIZZLE3(X, Y, 1),
};
static const SWIZZLE_SET g_sVec3DualIssueStandardAndExtendedSwizzles = 
	DECL_SWIZZLE_SET2(g_auVec3DualIssueStandardSwizzles, g_auVec3DualIssueExtendedSwizzles);

/*
	VEC3 dual-issue instruction swizzle sets derived using
	standard or extended, when possible
*/
IMG_UINT32 static const g_auVec3DualIssueDerivedSwizzles[] =
{
	USEASM_SWIZZLE3(X, 0, 0)
};


/*
	VEC4 dual-issue instruction standard swizzle set.
*/
IMG_UINT32 static const g_auVec4DualIssueStandardSwizzles[] =
{
	USEASM_SWIZZLE(X, X, X, X),
	USEASM_SWIZZLE(Y, Y, Y, Y),
	USEASM_SWIZZLE(Z, Z, Z, Z),
	USEASM_SWIZZLE(W, W, W, W),
	USEASM_SWIZZLE(X, Y, Z, W),
	USEASM_SWIZZLE(Y, Z, W, W),
	USEASM_SWIZZLE(X, Y, Z, Z),
	USEASM_SWIZZLE(X, X, Y, Z),
	USEASM_SWIZZLE(X, Y, X, Y),
	USEASM_SWIZZLE(X, Y, W, Z),
	USEASM_SWIZZLE(Z, X, Y, W),
	USEASM_SWIZZLE(Z, W, Z, W),
	USEASM_SWIZZLE(0, 0, 0, 0),
	USEASM_SWIZZLE(HALF, HALF, HALF, HALF),
	USEASM_SWIZZLE(1, 1, 1, 1),
	USEASM_SWIZZLE(2, 2, 2, 2),
};
static const SWIZZLE_SET g_sVec4DualIssueStandardSwizzles = DECL_SWIZZLE_SET1(g_auVec4DualIssueStandardSwizzles);

/*
	VEC4 dual-issue instruction extended swizzle set.
*/
IMG_UINT32 static const g_auVec4DualIssueExtendedSwizzles[] =
{
	USEASM_SWIZZLE(Y, Z, X, W),
	USEASM_SWIZZLE(Z, W, X, Y),
	USEASM_SWIZZLE(X, Z, W, Y),
	USEASM_SWIZZLE(Y, Y, W, W),
	USEASM_SWIZZLE(W, Y, Z, W),
	USEASM_SWIZZLE(W, Z, W, Z),
	USEASM_SWIZZLE(X, Y, Z, X),
	USEASM_SWIZZLE(Z, Z, W, W),
	USEASM_SWIZZLE(X, W, Z, X),
	USEASM_SWIZZLE(Y, Y, Y, X),
	USEASM_SWIZZLE(Y, Y, Y, Z),
	USEASM_SWIZZLE(Z, W, Z, W),
	USEASM_SWIZZLE(Y, Z, X, Z),
	USEASM_SWIZZLE(X, X, Y, Y),
	USEASM_SWIZZLE(X, Z, W, W),
	USEASM_SWIZZLE(X, Y, Z, 1),
};
static const SWIZZLE_SET g_sVec4DualIssueStandardAndExtendedSwizzles = 
	DECL_SWIZZLE_SET2(g_auVec4DualIssueStandardSwizzles, g_auVec4DualIssueExtendedSwizzles);

/*
	VEC4 dual-issue instruction swizzle sets derived using
	standard or extended, when possible
*/
IMG_UINT32 static const g_auVec4DualIssueDerivedSwizzles[] =
{
	USEASM_SWIZZLE(X, 0, 0, 0)
};

 
/*
	For each channel in a vec4 the swizzle to replicate that channel.
*/
IMG_INTERNAL
IMG_UINT32 const g_auReplicateSwizzles[] =
{
	USEASM_SWIZZLE(X, X, X, X),
	USEASM_SWIZZLE(Y, Y, Y, Y),
	USEASM_SWIZZLE(Z, Z, Z, Z),
	USEASM_SWIZZLE(W, W, W, W),
};

static const IMG_UINT32	g_auExpandedVectorLength[UF_REGFORMAT_COUNT] =
{
	SCALAR_REGISTERS_PER_F32_VECTOR,	/* UF_REGFORMAT_F32 */
	SCALAR_REGISTERS_PER_F16_VECTOR,	/* UF_REGFORMAT_F16 */
	SCALAR_REGISTERS_PER_C10_VECTOR,	/* UF_REGFORMAT_C10 */
	USC_UNDEF,							/* UF_REGFORMAT_U8 */
	USC_UNDEF,							/* UF_REGFORMAT_I32 */
	USC_UNDEF,							/* UF_REGFORMAT_U32 */
	USC_UNDEF,							/* UF_REGFORMAT_I16 */
	USC_UNDEF,							/* UF_REGFORMAT_U16 */
};

typedef struct _SWIZZLE_SPEC
{
	IMG_UINT32	auChanMask[VECTOR_LENGTH];
} SWIZZLE_SPEC, *PSWIZZLE_SPEC;

IMG_INTERNAL
const SWIZZLE_SEL g_asSwizzleSel[] = 
{
	{IMG_FALSE,		USC_X_CHAN,		USC_UNDEF,		USC_UNDEF},		/* USEASM_SWIZZLE_SEL_X */
	{IMG_FALSE,		USC_Y_CHAN,		USC_UNDEF,		USC_UNDEF},		/* USEASM_SWIZZLE_SEL_Y */
	{IMG_FALSE,		USC_Z_CHAN,		USC_UNDEF,		USC_UNDEF},		/* USEASM_SWIZZLE_SEL_Z */
	{IMG_FALSE,		USC_W_CHAN,		USC_UNDEF,		USC_UNDEF},		/* USEASM_SWIZZLE_SEL_W */
	{IMG_TRUE,		USC_UNDEF,		FLOAT32_ZERO,	FLOAT16_ZERO},	/* USEASM_SWIZZLE_SEL_0 */
	{IMG_TRUE,		USC_UNDEF,		FLOAT32_ONE,	FLOAT16_ONE},	/* USEASM_SWIZZLE_SEL_1 */
	{IMG_TRUE,		USC_UNDEF,		FLOAT32_TWO,	FLOAT16_TWO},	/* USEASM_SWIZZLE_SEL_2 */
	{IMG_TRUE,		USC_UNDEF,		FLOAT32_HALF,	FLOAT16_HALF},	/* USEASM_SWIZZLE_SEL_HALF */
};

IMG_INTERNAL
const USEASM_SWIZZLE_SEL g_aeChanToSwizzleSel[] = 
{
	USEASM_SWIZZLE_SEL_X,			/* USC_X_CHAN */
	USEASM_SWIZZLE_SEL_Y,			/* USC_Y_CHAN */
	USEASM_SWIZZLE_SEL_Z,			/* USC_Z_CHAN */
	USEASM_SWIZZLE_SEL_W,			/* USC_W_CHAN */
};

static
IMG_BOOL IsSwizzledHardwareConstant(PINTERMEDIATE_STATE	psState,
									IMG_UINT8			aui8VecData[],
									IMG_UINT32			uUsedChanMask,
									IMG_UINT32			uConst,
									UF_REGFORMAT		eFormat,
									PSWIZZLE_SPEC		psSwizzle);

IMG_INTERNAL
IMG_BOOL VectorInstPerArgumentF16F32Selection(IOPCODE eOpcode)
/*****************************************************************************
 FUNCTION	: VectorInstPerArgumentF16F32Selection
    
 PURPOSE	: Checks for a vector instruction which support a per-source or
			  destination selection of either F16 or F32 format.

 PARAMETERS	: eOpcode			- Instruction type to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	/*
		Complex-ops and packs have separate controls for the formats of the source and destination.
	*/
	if (
			(g_psInstDesc[eOpcode].uFlags & DESC_FLAGS_VECTORCOMPLEXOP) != 0 || 
			(g_psInstDesc[eOpcode].uFlags & DESC_FLAGS_VECTORPACK) != 0 || 
			eOpcode == IVMOV
	   )
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

IMG_BOOL static MatchSwizzle(IMG_UINT32 uSwizzle, PSWIZZLE_SPEC psSpec, IMG_UINT32 uUsedChanMask)
/*****************************************************************************
 FUNCTION	: MatchSwizzle
    
 PURPOSE	: Checks a swizzle against a mask of valid channels for each
			  selector in the swizzle.

 PARAMETERS	: uSwizzle		- Swizzle to check.
			  psSpec		- Supported masks of source channels for each
							channel in the swizzle.
			  uUsedChanMask	- Mask of channels in the swizzle to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uChan;

	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		if ((uUsedChanMask & (1 << uChan)) != 0)
		{
			USEASM_SWIZZLE_SEL	eSel;
			IMG_UINT32			uSrcChan;

			eSel = GetChan(uSwizzle, uChan);
			if (g_asSwizzleSel[eSel].bIsConstant)
			{
				return IMG_FALSE;
			}
			
			uSrcChan = g_asSwizzleSel[eSel].uSrcChan;
			if ((psSpec->auChanMask[uChan] & (1 << uSrcChan)) == 0)
			{
				return IMG_FALSE;
			}
		}
	}
	return IMG_TRUE;
}

IMG_UINT32 static SpecToSwizzle(PINTERMEDIATE_STATE psState, PSWIZZLE_SPEC psSpec, IMG_UINT32 uUsedChanMask)
/*****************************************************************************
 FUNCTION	: SpecToSwizzle
    
 PURPOSE	: Create a swizzle from a list of possible source channels for
			  each channel in the swizzle.

 PARAMETERS	: psState		- Compiler state.
			  psSpec		- Supported masks of source channels for each
							channel in the swizzle.
			  uUsedChanMask	- Mask of channels in the swizzle which are used.

 RETURNS	: The swizzle.
*****************************************************************************/
{
	IMG_UINT32	uSwizzle;
	IMG_UINT32	uChan;

	uSwizzle = USEASM_SWIZZLE(0, 0, 0, 0);
	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		if ((uUsedChanMask & (1 << uChan)) != 0)
		{
			IMG_UINT32	uMatchChanMask = psSpec->auChanMask[uChan];
			IMG_UINT32	uSrcChan;

			ASSERT(uMatchChanMask != 0);
			uSrcChan = g_auFirstSetBit[uMatchChanMask];

			uSwizzle = SetChan(uSwizzle, uChan, g_aeChanToSwizzleSel[uSrcChan]);
		}
	}
	return uSwizzle;
}

IMG_UINT32 static MatchSwizzleSet(PCSWIZZLE_SET		psSwizzleSet,
								  PSWIZZLE_SPEC		psSpec,
								  IMG_UINT32		uUsedChanMask)
/*****************************************************************************
 FUNCTION	: MatchSwizzleSet
    
 PURPOSE	: Checks a list of swizzles against a mask of valid channels for each
			  selector in the swizzle.

 PARAMETERS	: psSwizzleSet	- List of swizzles.
			  psSpec		- Supported masks of source channels for each
							channel in the swizzle.
			  uUsedChanMask	- Mask of channels in the swizzle to check.

 RETURNS	: Value of the first matching swizzle or USC_UNDEF if no swizzles
			  match.
*****************************************************************************/
{
	IMG_UINT32	uSubset;

	for (uSubset = 0; uSubset < psSwizzleSet->uSubsetCount; uSubset++)
	{
		IMG_UINT32			uSetIdx;
		IMG_UINT32			uNrSwizzlesInSet = psSwizzleSet->asSubset[uSubset].uCount;
		IMG_UINT32 const*	puSet = psSwizzleSet->asSubset[uSubset].puList;

		for (uSetIdx = 0; uSetIdx < uNrSwizzlesInSet; uSetIdx++)
		{
			IMG_UINT32	uSwizzleFromSet = puSet[uSetIdx];

			if (MatchSwizzle(uSwizzleFromSet, psSpec, uUsedChanMask))
			{
				return uSwizzleFromSet;
			}
		}
	}
	return USC_UNDEF;
}

IMG_BOOL static IsSwizzleInSet(IMG_UINT32 const*	puSet,
							   IMG_UINT32			uNrSwizzlesInSet,
							   IMG_UINT32			uSwizzleToCheck,
							   IMG_UINT32			uChannelMask,
							   IMG_PUINT32			puMatchedSwizzle)
/*****************************************************************************
 FUNCTION	: IsSwizzleInSet
    
 PURPOSE	: Check if a swizzle is present in a set.

 PARAMETERS	: puSet				- Points to an array of the swizzles in the set.
			  uNrSwizzlesInSet	- Count of swizzles in the set.
			  uSwizzleToCheck	- Swizzle to check if present.
			  uChannelMask		- Mask of the channel to compare.
			  puMatchedSwizzle	- On success returns the swizzle which matched.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uSwizzleCompareMask;
	IMG_UINT32	uChan, uSetIdx;

	/*
		Create a mask for the channels we are comparing.
	*/
	uSwizzleCompareMask = 0;
	for (uChan = 0; uChan < CHANS_PER_REGISTER; uChan++)
	{
		if (uChannelMask & (1 << uChan))
		{
			uSwizzleCompareMask |= (USEASM_SWIZZLE_VALUE_MASK << (USEASM_SWIZZLE_FIELD_SIZE * uChan));
		}
	}

	/*
		Clear bits for channels we aren't comparing.
	*/
	uSwizzleToCheck &= uSwizzleCompareMask;

	/*
		Compare against all the swizzles in the set.
	*/
	for (uSetIdx = 0; uSetIdx < uNrSwizzlesInSet; uSetIdx++)
	{
		IMG_UINT32	uSwizzleFromSet = puSet[uSetIdx];
		if ((uSwizzleFromSet & uSwizzleCompareMask) == uSwizzleToCheck)
		{
			if (puMatchedSwizzle != NULL)
			{
				*puMatchedSwizzle = uSwizzleFromSet;
			}

			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}


IMG_INTERNAL
IMG_UINT32 CombineSwizzles(IMG_UINT32	uSwizzle1,
		 				   IMG_UINT32	uSwizzle2)
/*****************************************************************************
 FUNCTION	: CombineSwizzles
    
 PURPOSE	: Generate a single swizzle representing the effect of applying two
			  swizzle in sequence.

 PARAMETERS	: uSwizzle1			- The first swizzle to apply.
			  uSwizzle2			- The seconde swizzle to apply.

 RETURNS	: The combined swizzle.
*****************************************************************************/
{
	IMG_UINT32	uChan;
	IMG_UINT32	uResult;

	uResult = 0;
	for (uChan = 0; uChan < CHANS_PER_REGISTER; uChan++)
	{
		IMG_UINT32	uSel2;
		
		/*
			Get the channel selection from the second swizzle.
		*/
		uSel2 = (uSwizzle2 >> (USEASM_SWIZZLE_FIELD_SIZE * uChan)) & USEASM_SWIZZLE_VALUE_MASK;

		if (uSel2 <= USEASM_SWIZZLE_SEL_W)
		{
			IMG_UINT32	uChanFrom1;
			IMG_UINT32	uSel1;
		
			/*
				Get the selection from the first swizzle corresponding
				to the selection from the second swizzle.
			*/
			uChanFrom1 = uSel2 - USEASM_SWIZZLE_SEL_X;
			uSel1 = (uSwizzle1 >> (USEASM_SWIZZLE_FIELD_SIZE * uChanFrom1)) & USEASM_SWIZZLE_VALUE_MASK;

			/*	
				Copy the selection from the first swizzle to the result.
			*/
			uResult |= (uSel1 << (USEASM_SWIZZLE_FIELD_SIZE * uChan));
		}
		else
		{
			/*
				For a constant channel selection just copy the selection from the second swizzle.
			*/
			uResult |= (uSel2 << (USEASM_SWIZZLE_FIELD_SIZE * uChan));
		}
	}
	return uResult;
}

IMG_INTERNAL
IMG_BOOL CompareSwizzles(IMG_UINT32 uSwiz1, IMG_UINT32 uSwiz2, IMG_UINT32 uCmpMask)
/*****************************************************************************
 FUNCTION	: CompareSwizzles
    
 PURPOSE	: Check if two swizzles are equal.

 PARAMETERS	: uSwiz1, uSwiz2		- The two swizzles to compare.
			  uCmpMask				- The mask of channels to compare from
									each swizzle.

 RETURNS	: TRUE if the swizzles are equal.
*****************************************************************************/
{
	IMG_UINT32	uChan;

	for (uChan = 0; uChan < CHANS_PER_REGISTER; uChan++)
	{
		IMG_UINT32	uSel1, uSel2;

		if (uCmpMask & (1 << uChan))
		{
			uSel1 = GetChan(uSwiz1, uChan);
			uSel2 = GetChan(uSwiz2, uChan);

			if (uSel1 != uSel2)
			{
				return IMG_FALSE;
			}
		}
	}
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_UINT32 GetPackMaximumSimultaneousConversions(PINTERMEDIATE_STATE	psState,
												 PINST					psInst)
/*****************************************************************************
 FUNCTION	: GetPackMaximumSimultaneousConversions
    
 PURPOSE	: Get the number of channels which can be simultaneously converted
			  by a PCK instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to get information about.

 RETURNS	: The count.
*****************************************************************************/
{
	switch (psInst->eOpcode)
	{
		case IVPCKU8U8:
		case IVPCKC10C10:
		case IVPCKS8FLT:
		{
			return SGXVEC_USE_PCK_OTHER_MAX_CONVERTS;
		}
		case IVPCKU8FLT:
		case IVPCKU8FLT_EXP:
		case IVPCKFLTU8:
		{
			if (psInst->u.psVec->bPackScale)
			{
				return SGXVEC_USE_PCK_U8NORM_F16F32_MAX_CONVERTS;
			}
			else
			{
				if (psInst->eOpcode == IVPCKFLTU8 && psInst->asDest[0].eFmt == UF_REGFORMAT_F32)
				{
					return SGXVEC_USE_PCK_OTHER_TO_F32_MAX_CONVERTS;
				}
				else
				{	
					return SGXVEC_USE_PCK_OTHER_MAX_CONVERTS;
				}
			}
		}
		case IVPCKC10FLT:
		case IVPCKC10FLT_EXP:
		case IVPCKFLTC10:
		case IVPCKFLTC10_VEC:
		{
			return SGXVEC_USE_PCK_C10_F16F32_MAX_CONVERTS;
		}
		case IVPCKFLTFLT:
		case IVPCKFLTFLT_EXP:
		{
			return SGXVEC_USE_PCK_F16F32_F16F32_MAX_CONVERTS;
		}
		case IVPCKFLTS8:
		case IVPCKFLTU16:
		case IVPCKFLTS16:
		{
			if (psInst->asDest[0].eFmt == UF_REGFORMAT_F32)
			{
				return SGXVEC_USE_PCK_OTHER_TO_F32_MAX_CONVERTS;
			}
			else
			{
				return SGXVEC_USE_PCK_OTHER_MAX_CONVERTS; 
			}
		}
		case IVPCKS16FLT:
		case IVPCKU16FLT:
		case IVPCKS16FLT_EXP:
		case IVPCKU16FLT_EXP:
		{
			return SGXVEC_USE_PCK_OTHER_MAX_CONVERTS;
		}
		case IVPCKU16U16:
		case IVPCKS16S8:
		case IVPCKU16U8:
		{
			return SGXVEC_USE_PCK_OTHER_MAX_CONVERTS;
		}
		default: imgabort();
	}
}

IMG_INTERNAL
IMG_UINT32 GetSwizzleSlotCount(PINTERMEDIATE_STATE	psState,
							   const INST			*psInst)
/*****************************************************************************
 FUNCTION	: GetSwizzleSlotCount
    
 PURPOSE	: Gets the count of vector arguments to an instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to get the swizzle slot count for.

 RETURNS	: The count.
*****************************************************************************/
{
	switch (psInst->eOpcode)
	{
		case IVPCKFLTFLT:
		case IVPCKU8U8:
		case IVPCKU8FLT:
		case IVPCKS8FLT:
		case IVPCKC10FLT:
		case IVPCKC10C10:
		case IVPCKS16FLT:
		case IVPCKU16FLT:
		case IUNPCKVEC:
		case IVPCKFLTU8:
		case IVPCKFLTS8:
		case IVPCKFLTC10:
		case IVPCKFLTC10_VEC:
		case IVPCKFLTU16:
		case IVPCKFLTS16:
		case IVMOVC10:
		case IVPCKU16U16:
		case IVPCKU16U8:
		case IVPCKS16S8:
		{
			return 1;
		}
		case IVPCKFLTFLT_EXP:
		case IVPCKU8FLT_EXP:
		case IVPCKC10FLT_EXP:
		case IVPCKS16FLT_EXP:
		case IVPCKU16FLT_EXP:
		{
			return 2;
		}
		case IVRCP:
		case IVRSQ:
		case IVLOG:
		case IVEXP:
		case IVMOV:
		case IVMOV_EXP:
		case IVLOAD:
		{
			return 1;
		}
		case IVMUL:
		case IVMUL3:
		case IVADD:
		case IVADD3:
		case IVFRC:
		case IVMIN:
		case IVMAX:
		case IVDP:
		case IVDP2:
		case IVDP3:
		case IVDP_GPI:
		case IVDP3_GPI:
		case IVDP_CP:
		case IVTEST:
		case IVTESTMASK:
		case IVTESTBYTEMASK:
		case IVDSX_EXP:
		case IVDSY_EXP:
		{
			return 2;
		}
		case IVDSX:
		case IVDSY:
		case IVTRC:
		{
			return 1;
		}
		case IVMAD:
		case IVMAD4:
		case IVMOVC:
		case IVMOVCU8_FLT:
		case IVMOVCBIT:
		case IVMOVC_I32:
		{
			return 3;
		}
		case IVDUAL:
		{
			return 4;
		}
		case ICVTFLT2ADDR:
		{
			return 1;
		}
		default: imgabort();
	}
}

IMG_INTERNAL
IMG_BOOL IsNonConstantSwizzle(IMG_UINT32	uSwizzle,
							  IMG_UINT32	uChannelMask,
							  IMG_PUINT32	puMatchedSwizzle)
/*****************************************************************************
 FUNCTION	: IsNonConstantSwizzle
    
 PURPOSE	: Checks if a given swizzle has only non-constant selectors.

 PARAMETERS	: uSwizzle			- Swizzle to check.
			  uChannelMask		- Channels used from the swizzle.
			  puMatchedSwizzle	- If the swizzle differs from a non-constant swizzle 
								only on unreferenced channels then returns the supported swizzle.

  RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uChan;

	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		if (uChannelMask & (1 << uChan))
		{
			USEASM_SWIZZLE_SEL	eSel;

			eSel = GetChan(uSwizzle, uChan);

			if (g_asSwizzleSel[eSel].bIsConstant)
			{
				return IMG_FALSE;
			}
		}
		else
		{
			uSwizzle &= ~(USEASM_SWIZZLE_VALUE_MASK << (USEASM_SWIZZLE_FIELD_SIZE * uChan));
		}
	}

	if (puMatchedSwizzle != NULL)
	{
		*puMatchedSwizzle = uSwizzle;
	}
	return IMG_TRUE;
}

typedef enum _SWIZZLE_SUPPORT_TYPE
{
	SWIZZLE_SUPPORT_TYPE_LIST,
	SWIZZLE_SUPPORT_TYPE_ANY_NONCONST,
	SWIZZLE_SUPPORT_TYPE_ANY,
} SWIZZLE_SUPPORT_TYPE, *PSWIZZLE_SUPPORT_TYPE;

static
PCSWIZZLE_SET GetVDUALSwizzleSupport(PINTERMEDIATE_STATE	psState,
									 PINST					psInst,
									 IMG_UINT32				uSwizzleSlotIdx)
/*****************************************************************************
 FUNCTION	: GetVDUALSwizzleSupport
    
 PURPOSE	: Gets the set of swizzles supported on a particular source to
			  the VDUAL instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  uSwizzleSlotIdx	- Index of the argument to check.

  RETURNS	: A pointer to the set of swizzles supported.
*****************************************************************************/
{
	IMG_BOOL			bExtendedSwizzle;

	/*	
		GPI2 slot supports only the identity swizzle.
	*/
	if (uSwizzleSlotIdx == VDUAL_SRCSLOT_GPI2)
	{
		return &g_sIdentitySwizzle;
	}

	/*
		Check whether this source slot supports the extended swizzle set.
	*/
	switch (uSwizzleSlotIdx)
	{
		case VDUAL_SRCSLOT_UNIFIEDSTORE:
		{
			bExtendedSwizzle = !psInst->u.psVec->sDual.bGPI2SlotUsed;	
			break;
		}
		case VDUAL_SRCSLOT_GPI0:
		{
			bExtendedSwizzle = IMG_FALSE;
			break;
		}
		case VDUAL_SRCSLOT_GPI1:
		{
			bExtendedSwizzle = IMG_TRUE;
			break;
		}
		default: imgabort();
	}

	/*
		Select different swizzle sets for vec3 and vec4 instructions.
	*/
	if (psInst->u.psVec->sDual.bVec3)
	{
		if (bExtendedSwizzle)
		{
			return &g_sVec3DualIssueStandardAndExtendedSwizzles;
		}
		else
		{
			return &g_sVec3DualIssueStandardSwizzles;
		}
	}
	else
	{
		if (bExtendedSwizzle)
		{
			return &g_sVec4DualIssueStandardAndExtendedSwizzles;
		}
		else
		{
			return &g_sVec4DualIssueStandardSwizzles;
		}
	}
}

IMG_INTERNAL
SWIZZLE_SUPPORT_TYPE GetSwizzleSupport(PINTERMEDIATE_STATE	psState,
									   PINST				psInst,
									   IOPCODE				eOpcode,
									   IMG_UINT32			uSwizzleSlotIdx,
									   PCSWIZZLE_SET*		ppsSupportedSet)
/*****************************************************************************
 FUNCTION	: GetSwizzleSupport
    
 PURPOSE	: 

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  eOpcode			- Instruction opcode to check for.
			  uSwizzleSlotIdx	- Index of the argument to check.
			  

  RETURNS	: 
*****************************************************************************/
{
	switch (eOpcode)
	{
		case IVMAD:
		{
			switch (uSwizzleSlotIdx)
			{
				case 0: *ppsSupportedSet = &g_sVMADSrc0Swizzles; break;
				case 1: *ppsSupportedSet = &g_sVMADSrc1Swizzles; break;
				case 2: *ppsSupportedSet = &g_sVMADSrc2Swizzles; break;
				default: imgabort();
			}

			return SWIZZLE_SUPPORT_TYPE_LIST;
		}
		case IVMAD4:
		{
			*ppsSupportedSet = &g_sVEC4StdAndExtSwizzles;
			return SWIZZLE_SUPPORT_TYPE_LIST;
		}
		case IVMUL3:
		case IVADD3:
		{
			*ppsSupportedSet = &g_sVEC3StdAndExtSwizzles;
			return SWIZZLE_SUPPORT_TYPE_LIST;
		}
		case IVTEST:
		case IVTESTMASK:
		case IVTESTBYTEMASK:
		{
			if (uSwizzleSlotIdx == 0)
			{
				*ppsSupportedSet = &g_sVTESTSrc1Swizzles;
			}
			else
			{
				*ppsSupportedSet = &g_sVTESTSrc2Swizzles;
			}
			return SWIZZLE_SUPPORT_TYPE_LIST;
		}
		case IVMUL:
		case IVADD:
		case IVFRC:
		case IVDSX_EXP:
		case IVDSY_EXP:
		case IVMIN:
		case IVMAX:
		case IVDP:
		case IVDP_GPI:
		case IVDP_CP:
		{
			switch (uSwizzleSlotIdx)
			{
				case 0: 
				{
					*ppsSupportedSet = NULL;
					return SWIZZLE_SUPPORT_TYPE_ANY;
				}
				case 1:
				{
					*ppsSupportedSet = &g_sVEC4StdSwizzles;
					return SWIZZLE_SUPPORT_TYPE_LIST;
				}
				default: imgabort();
			}
		}
		case IVDP3_GPI:
		{
			switch (uSwizzleSlotIdx)
			{
				case 0: 
				{
					*ppsSupportedSet = NULL;
					return SWIZZLE_SUPPORT_TYPE_ANY;
				}
				case 1:
				{
					*ppsSupportedSet = &g_sVEC3StdSwizzles;
					return SWIZZLE_SUPPORT_TYPE_LIST;
				}
				default: imgabort();
			}
		}
		case IVDP2:
		{
			switch (uSwizzleSlotIdx)
			{
				case 0: 
				{
					*ppsSupportedSet = NULL;
					return SWIZZLE_SUPPORT_TYPE_ANY;
				}
				case 1:
				{
					/*
						The hardware doesn't support a DP2 operation directly so we need to convert this
						instruction to a DP3 with constants swizzled into the Z channel of both sources. This
						means the only possible swizzle for source 1 is XY1.
					*/
					*ppsSupportedSet = &g_sIdentitySwizzle;
					return SWIZZLE_SUPPORT_TYPE_LIST;
				}
				default: imgabort();
			}
		}
		case IVDP3:
		{
			if (uSwizzleSlotIdx == 0)
			{
				*ppsSupportedSet = NULL;
				return SWIZZLE_SUPPORT_TYPE_ANY;
			}
			else
			{
				/*
					To simulate a dot3 operand using the hardware's VDP instruction (which does dot4)
					we need to set the W selectors to constants in the swizzles on both sources. Swizzles
					on source 1 are unrestricted but XYZ1 is the only possible choice from the
					palette on source 2.
				*/
				*ppsSupportedSet = &g_sIdentitySwizzle;
				return SWIZZLE_SUPPORT_TYPE_LIST;
			}
			break;
		}
		case IVDSX:
		case IVDSY:
		{
			/*
				These instructions only have one vector source but will be expanded to a two source
				version in the hardware (with both sources containing the same data). So check against
				the source with the most restrictive set of swizzles (which is source 1).
			*/
			*ppsSupportedSet = &g_sVEC4StdSwizzles;
			return SWIZZLE_SUPPORT_TYPE_LIST;
		}
		case IVMOV:
		{
			/* 
				The hardware VMOV instruction supports a different set of swizzles. But we might map
				the intermediate VMOV instruction to VPCKFLTFLT if it reads a >64-bit source vector. So
				restrict the supported swizzle to those supported by VPCKFLTFLT.
			*/
			*ppsSupportedSet = NULL;
			return SWIZZLE_SUPPORT_TYPE_ANY_NONCONST;
		}
		case IVMOV_EXP:
		{
			/*
				Check the swizzles supported by the hardware VMOV instruction.
			*/
			*ppsSupportedSet = &g_sVEC4StdSwizzles;
			return SWIZZLE_SUPPORT_TYPE_LIST;
		}
		case IVMOVC:
		case IVMOVCU8_FLT:
		{
			if (uSwizzleSlotIdx == 0)
			{
				/*
					Only the identity swizzle is supported on source 0.
				*/
				*ppsSupportedSet = &g_sIdentitySwizzle;
				return SWIZZLE_SUPPORT_TYPE_LIST;
			}
			else
			{
				/*
					The standard, vec4 set is supported on source 1 and source 2.
				*/
				*ppsSupportedSet = &g_sVEC4StdSwizzles;
				return SWIZZLE_SUPPORT_TYPE_LIST;
			}
		}
		case IVRCP:
		case IVRSQ:
		case IVLOG:
		case IVEXP:
		{
			/*
				All non-constant swizzles are supported by the complex ops.
			*/
			*ppsSupportedSet = NULL;
			return SWIZZLE_SUPPORT_TYPE_ANY_NONCONST;
		}
		case IVPCKU8U8:
		case IVPCKU8FLT:
		case IVPCKS8FLT:
		case IVPCKC10FLT:
		case IVPCKFLTU8:
		case IVPCKFLTS8:
		case IVPCKFLTU16:
		case IVPCKFLTS16:
		case IVPCKFLTC10:
		case IVPCKFLTC10_VEC:
		case IVPCKFLTFLT:
		case IVPCKS16FLT:
		case IVPCKS16FLT_EXP:
		case IVPCKU16FLT:
		case IUNPCKVEC:
		case IVPCKFLTFLT_EXP:
		{
			/*
				All non-constant swizzles are supported by VPCK.
			*/
			*ppsSupportedSet = NULL;
			return SWIZZLE_SUPPORT_TYPE_ANY_NONCONST;
		}
		case IVDUAL:
		{
			*ppsSupportedSet = GetVDUALSwizzleSupport(psState, psInst, uSwizzleSlotIdx);
			return SWIZZLE_SUPPORT_TYPE_LIST;
		}
		default:
		{
			imgabort();
		}
	}
}

static
IMG_BOOL IsSwizzleInList(PCSWIZZLE_SET			psSupportedSet,
						 IMG_UINT32				uSwizzle,
						 IMG_UINT32				uChannelMask,
						 IMG_PUINT32			puMatchedSwizzle)
/*****************************************************************************
 FUNCTION	: IsSwizzleInList
    
 PURPOSE	: Checks if a specified swizzle is present in a list.

 PARAMETERS	: psSupportedSet	- List of swizzles.
			  uSwizzle			- Swizzle to check.
			  uChannelMask		- Mask of significant channels in the swizzle.
			  puMatchedSwizzle	- If the swizzle differs from a swizzle in the 
								list only on non-significant channels then 
								returns the matching swizzle.

  RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	i;

	for (i = 0; i < psSupportedSet->uSubsetCount; i++)
	{
		if (IsSwizzleInSet(psSupportedSet->asSubset[i].puList,
						   psSupportedSet->asSubset[i].uCount,
						   uSwizzle,
						   uChannelMask,
						   puMatchedSwizzle))
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}	

IMG_INTERNAL
IMG_BOOL IsSwizzleSupported(PINTERMEDIATE_STATE	psState,
							PINST				psInst,
						    IOPCODE				eOpcode,
							IMG_UINT32			uSwizzleSlotIdx,
							IMG_UINT32			uSwizzle,
							IMG_UINT32			uChannelMask,
							IMG_PUINT32			puMatchedSwizzle)
/*****************************************************************************
 FUNCTION	: IsSwizzleSupported
    
 PURPOSE	: Checks if a given swizzle is supported on a vector instruction
			  source.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  eOpcode			- Instruction opcode to check for.
			  uSwizzleSlotIdx	- Index of the argument to check.
			  uSwizzle			- Swizzle to check.
			  uChannelMask		- Channels used from the swizzle.
			  puMatchedSwizzle	- If the swizzle differs from a swizzle supported
								by the instruction only on unreferenced channels
								then returns the supported swizzle.

  RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	SWIZZLE_SUPPORT_TYPE	eSupportType;
	PCSWIZZLE_SET			psSupportedSet;

	eSupportType = GetSwizzleSupport(psState, psInst, eOpcode, uSwizzleSlotIdx, &psSupportedSet);
	switch (eSupportType)
	{
		case SWIZZLE_SUPPORT_TYPE_LIST:
		{
			return IsSwizzleInList(psSupportedSet, uSwizzle, uChannelMask, puMatchedSwizzle);
		}

		case SWIZZLE_SUPPORT_TYPE_ANY_NONCONST:
		{
			return IsNonConstantSwizzle(uSwizzle, uChannelMask, puMatchedSwizzle);
		}

		case SWIZZLE_SUPPORT_TYPE_ANY:
		{
			if (puMatchedSwizzle != NULL)
			{
				*puMatchedSwizzle = uSwizzle;
			}
			return IMG_TRUE;
		}
		default: imgabort();
	}
}

IMG_INTERNAL
IMG_BOOL IsVTESTExtraSwizzles(PINTERMEDIATE_STATE psState, IOPCODE eOpcode, PINST psInst)
/*****************************************************************************
 FUNCTION	: IsVTESTExtraSwizzles
    
 PURPOSE	: Check for the case where a VTEST instruction supports an extra
			  set of swizzles on its source.

 PARAMETERS	: psState			- Compiler state.
			  eOpcode			- Instruction type to check.
			  psInst			- Instruction.

  RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PVTEST_PARAMS	psTest;

	if (eOpcode != IVTEST)
	{
		return IMG_FALSE;
	}
	if (psInst->uDestCount != VTEST_PREDICATE_ONLY_DEST_COUNT)
	{
		return IMG_FALSE;
	}
		
	psTest = &psInst->u.psVec->sTest;
	if (psTest->eChanSel != VTEST_CHANSEL_XCHAN)
	{
		return IMG_FALSE;
	}

	ASSERT(psTest->eTestOpcode < IOPCODE_MAX);
	if ((g_psInstDesc[psTest->eTestOpcode].uFlags2 & DESC_FLAGS2_VECTORREPLICATEDRESULT) != 0)
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL HasIndependentSourceSwizzles(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: HasIndependentSourceSwizzles
    
 PURPOSE	: Check for an instruction where range of valid swizzles on each
			  sources is independent of the swizzles on the other sources.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.

  RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if ((psInst->eOpcode == IVMOVC) || (psInst->eOpcode == IVMOVCU8_FLT))
	{
		return IMG_FALSE;
	}
	if (psInst->eOpcode == IVTEST && IsVTESTExtraSwizzles(psState, psInst->eOpcode, psInst))
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL IsModifiedSwizzleSetSupported(PINTERMEDIATE_STATE	psState,
									   PINST				psInst,
									   IOPCODE				eOpcode,
									   IMG_UINT32			uSwizzleSlotIdx,
									   IMG_UINT32			uSwizzle,
									   IMG_PUINT32			puMatchedSwizzle)
/*****************************************************************************
 FUNCTION	: IsModifiedSwizzleSetSupported
    
 PURPOSE	: Checks if a given swizzle is supported on a vector instruction
			  source.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  eOpcode			- Instruction opcode to check for.
			  uSwizzleSlotIdx	- Index of the argument to check.
			  uSwizzle			- Swizzle to check.
			  puMatchedSwizzle	- If the swizzle differs from a swizzle supported
								by the instruction only on unreferenced channels
								then returns the supported swizzle.

  RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uSlotIdx;
	IMG_UINT32	auNewSwizzleSet[VECTOR_MAX_SOURCE_SLOT_COUNT];
	IMG_BOOL	bRet;

	for (uSlotIdx = 0; uSlotIdx < GetSwizzleSlotCount(psState, psInst); uSlotIdx++)
	{
		if (uSlotIdx == uSwizzleSlotIdx)
		{
			auNewSwizzleSet[uSlotIdx] = uSwizzle;
		}
		else
		{
			auNewSwizzleSet[uSlotIdx] = psInst->u.psVec->auSwizzle[uSlotIdx];
		}
	}

	bRet = IsSwizzleSetSupported(psState, eOpcode, psInst, NULL /* auNewLiveChansInArg */, auNewSwizzleSet);

	if (bRet)
	{
		*puMatchedSwizzle = auNewSwizzleSet[uSwizzleSlotIdx];
	}

	return bRet;
}

IMG_INTERNAL
IMG_BOOL IsSwizzleSetSupported(PINTERMEDIATE_STATE	psState,
							   IOPCODE				eOpcode,
							   PINST				psInst,
							   IMG_UINT32			auNewLiveChansInArg[],
							   IMG_UINT32			auSwizzleSet[])
/*****************************************************************************
 FUNCTION	: IsSwizzleSetSupported
    
 PURPOSE	: Check if a complete set of swizzle for the sources to an
			  instruction is supported.

 PARAMETERS	: psState			- Compiler state.
			  eOpcode			- Instruction type to check.
			  psInst			- Instruction.
			  auNewLiveChansInArg	
								- If non-NULL then the mask of channels used from
								each instruction source.
			  auSwizzleSet		- Swizzle for each source. On return elements
								might be updated to swizzles which are equivalent
								on channels referenced by the instruction.

  RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uSlotIdx;
	IMG_UINT32	auLiveChansInArg[VECTOR_MAX_SOURCE_SLOT_COUNT];

	/*
		Get the channels referenced from each source before the swizzle is applied.
	*/
	for (uSlotIdx = 0; uSlotIdx < GetSwizzleSlotCount(psState, psInst); uSlotIdx++)
	{
		if (auNewLiveChansInArg != NULL)
		{
			auLiveChansInArg[uSlotIdx] = auNewLiveChansInArg[uSlotIdx];
		}
		else
		{
			GetLiveChansInSourceSlot(psState, 
									 psInst, 
									 uSlotIdx, 
									 &auLiveChansInArg[uSlotIdx], 
									 NULL /* puChansUsedPostSwizzle */);
		}
	}

	if ((eOpcode == IVMOVC)  || (eOpcode == IVMOVCU8_FLT))
	{
		/*
			For VMOVC the swizzles on sources 2 and 3 must match.
		*/
		ASSERT(auLiveChansInArg[1] == auLiveChansInArg[2]);
		if (!CompareSwizzles(auSwizzleSet[1], auSwizzleSet[2], auLiveChansInArg[1]))
		{
			return IMG_FALSE;
		}
	}

	/*
		For VTEST writing only a single predicate result we can use the channel select to
		chose any channel from the ALU result to test to generate the predicate result. We
		can use this to support on extra swizzles on the TEST instruction sources.
	*/
	if (IsVTESTExtraSwizzles(psState, eOpcode, psInst))
	{
		ASSERT(auLiveChansInArg[0] == USC_X_CHAN_MASK);
		ASSERT(auLiveChansInArg[0] == auLiveChansInArg[1]);

		/*
			Check the swizzle on the first source is a replicate swizzle.
		*/
		if (!IsSwizzleInList(&g_sVTESTSrc1Swizzles_OneChan, auSwizzleSet[0], auLiveChansInArg[0], &auSwizzleSet[0]))
		{
			return IMG_FALSE;
		}

		/*
			Check the swizzle on the second source is either equal to the first swizzle or
			replicating the X channel.
		*/
		if (CompareSwizzles(auSwizzleSet[0], auSwizzleSet[1], auLiveChansInArg[1]))
		{
			auSwizzleSet[1] = auSwizzleSet[0];
			return IMG_TRUE;
		}
		if (CompareSwizzles(auSwizzleSet[1], USEASM_SWIZZLE(X, X, X, X), auLiveChansInArg[1]))
		{
			auSwizzleSet[1] = USEASM_SWIZZLE(X, X, X, X);
			return IMG_TRUE;
		}

		return IMG_FALSE;
	}

	/*
		Check for each swizzle it is supported by the corresponding source to
		the instruction.
	*/
	for (uSlotIdx = 0; uSlotIdx < GetSwizzleSlotCount(psState, psInst); uSlotIdx++)
	{
		if (!IsSwizzleSupported(psState,
							    psInst,
								eOpcode,
								uSlotIdx,
								auSwizzleSet[uSlotIdx],
								auLiveChansInArg[uSlotIdx],
								&auSwizzleSet[uSlotIdx]))
		{
			return IMG_FALSE;
		}
	}
	return IMG_TRUE;
}

typedef struct _VECTOR_COPY
{
	/* Entry in the list of copies with the same source data and the same source modifier. */
	USC_LIST_ENTRY			sCopyListEntry;
	/* Instruction performing the copy. */
	PINST					psCopyInst;
} VECTOR_COPY, *PVECTOR_COPY;

typedef struct _VECTOR_COPY_LIST
{
	/* Modifier applied when copying. */
	VEC_SOURCE_MODIFIER		sModifier;
	/* List of copies with different swizzles. */
	USC_LIST				sCopyList;	
	/* Number of source arguments making up the vector to copy. */
	IMG_UINT32				uCopySourceCount;
	/* Source arguments to copy. */
	ARG						asCopySource[MAX_SCALAR_REGISTERS_PER_VECTOR];
} VECTOR_COPY_LIST, *PVECTOR_COPY_LIST;

typedef struct _VECTOR_COPY_MAP
{
	/*
		Map of copy instructions in the current basic block.
	*/
	PUSC_TREE	psLocalCopies;
	/*
		Map of copy instructions in the secondary update program.
	*/
	PUSC_TREE	psUniformCopies;
} VECTOR_COPY_MAP, *PVECTOR_COPY_MAP;

IMG_INT32 static VectorCopyCmp(IMG_PVOID pvElem1, IMG_PVOID pvElem2)
/*****************************************************************************
 FUNCTION	: VectorCopyCmp
    
 PURPOSE	:

 PARAMETERS	: pvElem1		- Elements to compare.
			  pvElem2

  RETURNS	: -ve	if ELEM1 < ELEM2
			  0     if ELEM1 == ELEM2
			  +ve   if ELEM1 > ELEM2
*****************************************************************************/
{
	PVECTOR_COPY_LIST	psElem1 = (PVECTOR_COPY_LIST)pvElem1;
	PVECTOR_COPY_LIST	psElem2 = (PVECTOR_COPY_LIST)pvElem2;
	IMG_UINT32			uSrc;

	if (psElem1->uCopySourceCount != psElem2->uCopySourceCount)
	{
		return psElem1->uCopySourceCount - psElem2->uCopySourceCount;
	}
	for (uSrc = 0; uSrc < psElem1->uCopySourceCount; uSrc++)
	{
		IMG_INT32	iCmp;

		iCmp = CompareArgs(&psElem1->asCopySource[uSrc], &psElem2->asCopySource[uSrc]);
		if (iCmp != 0)
		{
			return iCmp;
		}
	}
	if (psElem1->sModifier.bNegate != psElem2->sModifier.bNegate)
	{
		return psElem1->sModifier.bNegate - psElem2->sModifier.bNegate;
	}
	if (psElem1->sModifier.bAbs != psElem2->sModifier.bAbs)
	{
		return psElem1->sModifier.bAbs - psElem2->sModifier.bAbs;
	}
	return 0;
}

IMG_UINT32 static FindVectorCopy(PINTERMEDIATE_STATE	psState,
								 PUSC_TREE				psCopyTree,
							     PVECTOR_COPY_LIST		psCopyKey,
							     IMG_UINT32				uSwizzle,
								 IMG_UINT32				uSwizzleMask,
							     PINST*					ppsCopyInst)
/*****************************************************************************
 FUNCTION	: FindVectorCopy
    
 PURPOSE	: Check if a copy of a source argument with a specific swizzle
			  and modifier has already been created.

 PARAMETERS	: psState			- Compiler state.
			  psCopyTree		- Information about copy instructions.
			  psCopyKey			- Details of the source argument and modifier.
			  uSwizzle			- Swizzle to check for.
			  uSwizzleMask		- Mask of channels used from the swizzle.
			  ppsCopyInst		- Returns a poiinter to the copy instruction.

  RETURNS	: The temporary which contains the copy or USC_UNDEF if a copy
			  with those parameters hasn't been created.
*****************************************************************************/
{
	PVECTOR_COPY_LIST	psNode;
	PUSC_LIST_ENTRY		psListEntry;

	*ppsCopyInst = NULL;

	/*
		Check for no swizzling instructions inserted yet.
	*/
	if (psCopyTree == NULL)
	{
		return USC_UNDEF;
	}

	/*
		Look for an instruction copying the same source data and using the same source modifier.
	*/
	psNode = (PVECTOR_COPY_LIST)UscTreeGetPtr(psCopyTree, psCopyKey);
	if (psNode == NULL)
	{
		return USC_UNDEF;
	}

	/*
		Look for an instruction applying a swizzle which matches on the used channels.
	*/
	for (psListEntry = psNode->sCopyList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PVECTOR_COPY	psCopy;
		IMG_UINT32		uCommonMask;
		IMG_UINT32		uCopySwizzle;
		PINST			psCopyInst;

		psCopy = IMG_CONTAINING_RECORD(psListEntry, PVECTOR_COPY, sCopyListEntry);
		psCopyInst = psCopy->psCopyInst;

		uCommonMask = uSwizzleMask & psCopyInst->auDestMask[0];
		uCopySwizzle = psCopyInst->u.psVec->auSwizzle[0];
		if (CompareSwizzles(uCopySwizzle, uSwizzle, uCommonMask))
		{
			*ppsCopyInst = psCopy->psCopyInst;

			ASSERT(psCopyInst->uDestCount == 1);
			ASSERT(psCopyInst->asDest[0].uType == USEASM_REGTYPE_TEMP);
			return psCopyInst->asDest[0].uNumber;
		}
	}

	return USC_UNDEF;
}

IMG_VOID static InsertVectorCopy(PINTERMEDIATE_STATE	psState,
							     PUSC_TREE*				ppsCopyTree,
								 PINST					psCopyInst,
							     PVECTOR_COPY_LIST		psCopyKey)
/*****************************************************************************
 FUNCTION	: InsertVectorCopy
    
 PURPOSE	: Record an instruction inserted to apply a swizzle or a modifier
			  to a source argument.

 PARAMETERS	: psState			- Compiler state.
			  ppsCopyTree		- Tree to use to record the instruction.
			  psCopyInst		- Copy instruction.
			  psCopyKey			- Source data for this copy.

  RETURNS	: None.
*****************************************************************************/
{
	PUSC_TREE			psCopyTree;
	PVECTOR_COPY_LIST	psCopyList;
	PVECTOR_COPY		psNewCopy;

	psCopyTree = *ppsCopyTree;

	/*
		Create the tree if it doesn't already exist.
	*/
	if (psCopyTree == NULL)
	{
		psCopyTree = *ppsCopyTree = UscTreeMake(psState, sizeof(VECTOR_COPY_LIST), VectorCopyCmp);
	}

	/*
		Add a new list of copies from the same source register and with the same source modifier
		to the map.
	*/
	if ((psCopyList = (PVECTOR_COPY_LIST)UscTreeGetPtr(psCopyTree, psCopyKey)) == NULL)
	{
		psCopyList = (PVECTOR_COPY_LIST)UscTreeAdd(psState, psCopyTree, psCopyKey);
		InitializeList(&psCopyList->sCopyList);
	}

	/*
		Add a new copy using this source swizzle.
	*/
	psNewCopy = UscAlloc(psState, sizeof(*psNewCopy));
	psNewCopy->psCopyInst = psCopyInst;
	AppendToList(&psCopyList->sCopyList, &psNewCopy->sCopyListEntry);
}

IMG_INTERNAL
IMG_BOOL IsUniformVectorSource(PINTERMEDIATE_STATE	psState,
							   PINST				psInst,
							   IMG_UINT32			uSourceSlot)
/*****************************************************************************
 FUNCTION	: IsUniformVectorSource
    
 PURPOSE	: Check for a vector source which is uniform (contains the
			  same data for every instance).

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  uSourceSlot		- Index of the vector source to check.

  RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uArgStart = uSourceSlot * SOURCE_ARGUMENTS_PER_VECTOR;
	PARG		psVecArg = &psInst->asArg[uArgStart];

	if (psVecArg->uType != USC_REGTYPE_UNUSEDSOURCE)
	{
		return IsUniformSource(psState, psVecArg);
	}
	else
	{
		IMG_UINT32	uChan;

		for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
		{
			if (!IsUniformSource(psState, &psInst->asArg[uArgStart + 1 + uChan]))
			{
				return IMG_FALSE;
			}
		}
		return IMG_TRUE;
	}
}

IMG_INTERNAL
IMG_BOOL IsNonSSAVectorSource(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSlot)
/******************************************************************************
 FUNCTION	: IsNonSSAVectorSource

 PURPOSE    : Check for a source to a vector instruction which is an intermediate 
			  register which can be defined more than once.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  uSlot				- Source to check.

 RETURNS	: TRUE or FALSE.
******************************************************************************/
{
	IMG_UINT32	uArgBase;
	PARG		psArgBase;

	uArgBase = uSlot * SOURCE_ARGUMENTS_PER_VECTOR;
	psArgBase = &psInst->asArg[uArgBase];
	if (psArgBase->uType == USC_REGTYPE_UNUSEDSOURCE)
	{
		IMG_UINT32	uChan;

		/*
			If this source is a vector of scalar registers then check
			each element of the vector.
		*/
		for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
		{
			PARG	psChan = &psInst->asArg[uArgBase + 1 + uChan];

			if (IsNonSSAArgument(psState, psChan))
			{
				return IMG_TRUE;
			}
		}
		return IMG_FALSE;
	}
	else
	{
		return IsNonSSAArgument(psState, psArgBase);
	}
}

static
IMG_VOID ConvertVMOVToVMUL(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: ConvertVMOVToVMUL
    
 PURPOSE	: Convert an instruction from VMOV DEST, SRC to VMUL DEST, SRC, 1.0f

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.

  RETURNS	: Nothing.
*****************************************************************************/
{
	UF_REGFORMAT	eSrcFmt = psInst->u.psVec->aeSrcFmt[0];

	if (psInst->eOpcode == IVMUL)
	{
		return;
	}
	ASSERT(psInst->eOpcode == IVMOV);

	ModifyOpcode(psState, psInst, IVMUL);

	/*
		Make the other VMUL source: 1.0f.
	*/
	if (eSrcFmt == UF_REGFORMAT_F32)
	{
		SetupImmediateSourceU(psInst, 1 /* uArgIdx */, FLOAT32_ONE, UF_REGFORMAT_F32);
	}
	else
	{
		ASSERT(eSrcFmt == UF_REGFORMAT_F16);
		SetupImmediateSourceU(psInst, 1 /* uArgIdx */, FLOAT16_ONE_REPLICATED, UF_REGFORMAT_F16);
	}
}

static
IMG_VOID MakeSwizzleCopyInst(PINTERMEDIATE_STATE	psState,
							 PINST					psSrcInst,
							 PCODEBLOCK				psInsertBlock,
							 PINST					psInsertBeforeInst,
							 PUSC_TREE*				ppsCopyTree,
							 IMG_UINT32				uArgIdx,
							 IMG_UINT32				uSwizzleToApply,
							 PVECTOR_COPY_LIST		psCopyKey,
							 IMG_PUINT32			puSwizzledTempNum,
							 PINST*					ppsCopyInst)
/*****************************************************************************
 FUNCTION	: MakeSwizzleCopyInst
    
 PURPOSE	: Create an instruction to apply a swizzle to a source argument.

 PARAMETERS	: psState			- Compiler state.
			  psSrcInst			- Instruction containing the argument to swizzle.
			  psInsertBlock		- Block to insert the new instruction into.
			  psInsertBeforeInst
								- Point to insert the new instruction.
			  ppsCopyTree		- Map updated with details of the new instruction.
			  uArgIdx			- Index of the argument to swizzle.
			  uSwizzleToApply	- Swizzle to apply.
			  psCopyKey			- Source registers and modifier for the copy.
			  puSwizzledTempNum	- Returns the number of the intermediate register
								containing the swizzle data.
			  ppsCopyInst		- Returns a pointer to the instruction which applies
								the swizzle.

  RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PINST			psVMOVInst;
	IMG_UINT32		uSwizzledTempNum;
	UF_REGFORMAT	eSwizzledFmt = psSrcInst->u.psVec->aeSrcFmt[uArgIdx];
		
	*puSwizzledTempNum = uSwizzledTempNum = GetNextRegister(psState);
	
	*ppsCopyInst = psVMOVInst = AllocateInst(psState, psSrcInst);
		
	/*
		Record we have created a copy of the source argument so we can reuse it if the source
		argument is used with the same swizzle in later instructions.
	*/
	InsertVectorCopy(psState,
					 ppsCopyTree, 
					 psVMOVInst,
					 psCopyKey);

	SetOpcode(psState, psVMOVInst, IVMOV);

	/*
		Get a new register for the VMOV destination.
	*/
	SetDest(psState, psVMOVInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uSwizzledTempNum, eSwizzledFmt);
	psVMOVInst->auDestMask[0] = 0;
	psVMOVInst->auLiveChansInDest[0] = 0;
	ASSERT(psVMOVInst->asDest[0].eFmt == UF_REGFORMAT_F16 || psVMOVInst->asDest[0].eFmt == UF_REGFORMAT_F32);

	/*
		Make VMOV source 0 the source to the instruction.
	*/
	MoveVectorSource(psState, psVMOVInst, 0 /* uDestArgIdx */, psSrcInst, uArgIdx, IMG_FALSE /* bCopySourceModifier */);
	psVMOVInst->u.psVec->auSwizzle[0] = uSwizzleToApply;
	psVMOVInst->u.psVec->asSrcMod[0] = psCopyKey->sModifier;

	/*
		VMOV doesn't support a negate or absolute source modifier.
	*/
	if (psCopyKey->sModifier.bNegate || psCopyKey->sModifier.bAbs)
	{
		ConvertVMOVToVMUL(psState, psVMOVInst);
	}

	/*
		Insert the VMOV before the instruction we are fixing.
	*/
	InsertInstBefore(psState, psInsertBlock, psVMOVInst, psInsertBeforeInst);
}

IMG_BOOL static InsertUniformSwizzleCopy(PINTERMEDIATE_STATE	psState,
										 PVECTOR_COPY_MAP		psCopyMap,
										 PINST					psInst,
										 IMG_UINT32				uArgIdx,
										 IMG_UINT32				uSwizzleToApply,
										 IMG_UINT32				uUsedChanMask,
										 PVECTOR_COPY_LIST		psCopyKey,
										 IMG_PUINT32			puSwizzledTempNum,
										 PINST*					ppsCopyInst)
/*****************************************************************************
 FUNCTION	: InsertUniformSwizzleCopy
    
 PURPOSE	: Check if the instruction to apply a swizzle to a source argument
			  can be inserted in the secondary update program, and if so
			  insert it.

 PARAMETERS	: psState			- Compiler state.
			  psCopyMap			- Details of instructions already inserted to
								swizzle source arguments.
			  psInst			- Instruction containing the argument to swizzle.
			  uArgIdx			- Index of the argument to swizzle.
			  uSwizzleToApply	- Swizzle to apply.
			  uUsedChanMask		- Mask of channels used from the argument before
								the swizzle is applied.
			  psCopyKey			- Source data and modifiers for the argument.
			  puSwizzledTempNum	- Returns the number of the intermediate register
								containing the swizzle data.
			  ppsCopyInst		- Returns a pointer to the instruction which applies
								the swizzle.

  RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32		uResultSize;
	UF_REGFORMAT	eSwizzledFmt = psInst->u.psVec->aeSrcFmt[uArgIdx];
	IMG_UINT32		uSwizzledTempNum;
	PINST			psCopyInst;

	if (psInst->psBlock->psOwner->psFunc == psState->psSecAttrProg)
	{
		return IMG_FALSE;
	}

	/*
		Check we are allowed to move calculations to the secondary update
		program.
	*/
	if ((psState->uCompilerFlags & UF_EXTRACTCONSTANTCALCS) == 0)
	{
		return IMG_FALSE;
	}

	/*
		Check if we have already inserted a swizzling instruction into the secondary update
		program.
	*/
	uSwizzledTempNum = FindVectorCopy(psState,
									  psCopyMap->psUniformCopies, 
									  psCopyKey,
									  uSwizzleToApply, 
									  uUsedChanMask,
									  &psCopyInst);
	if (uSwizzledTempNum != USC_UNDEF)
	{
		*puSwizzledTempNum = uSwizzledTempNum;
		*ppsCopyInst = psCopyInst;
		return IMG_TRUE;
	}

	/*
		Get the size of the result generated by the swizzling instruction.
	*/
	switch (eSwizzledFmt)
	{
		case UF_REGFORMAT_F32: uResultSize = SCALAR_REGISTERS_PER_F32_VECTOR; break;
		case UF_REGFORMAT_F16: uResultSize = SCALAR_REGISTERS_PER_F16_VECTOR; break;
		default: imgabort();
	}

	/*
		Check if there are enough secondary attributes to hold the result of the swizzle.
	*/
	if (!CheckAndUpdateInstSARegisterCount(psState, uResultSize, uResultSize, IMG_TRUE))
	{
		return IMG_FALSE;
	}

	/*
		Append the swizzling instruction to the end of the secondary update program.
	*/
	MakeSwizzleCopyInst(psState,
						psInst,
						psState->psSecAttrProg->sCfg.psExit,
						NULL /* psInsertBeforeInst */,
						&psCopyMap->psUniformCopies,
						uArgIdx,
						uSwizzleToApply,
						psCopyKey,
						puSwizzledTempNum,
						ppsCopyInst);

	/*
		Add the result of the swizzling instruction as live out of the secondary update
		program and into the main program.
	*/
	BaseAddNewSAProgResult(psState,
						   eSwizzledFmt,
						   IMG_TRUE /* bVector */,
						   0 /* uHwRegAlignment */,
						   uResultSize,
						   *puSwizzledTempNum,
						   0 /* uUsedChanMask */,
						   SAPROG_RESULT_TYPE_CALC,
						   IMG_FALSE /* bPartOfRange */);

	return IMG_TRUE;
}

IMG_BOOL static InsertSwizzleCopy(PINTERMEDIATE_STATE	psState,
								  PVECTOR_COPY_MAP		psCopyMap,
								  PINST					psInst,
								  IMG_UINT32			uArgIdx,
								  IMG_UINT32			uSwizzleToApply,
								  IMG_UINT32			uUsedChanMask,
								  PVEC_SOURCE_MODIFIER	psModifier,
								  IMG_PUINT32			puSwizzledTempNum,
								  PINST*				ppsCopyInst)
/*****************************************************************************
 FUNCTION	: InsertSwizzleCopy
    
 PURPOSE	: Either insert an instruction to apply a swizzle to a specified
			  source argument or return the details of an existing instruction
			  which already applies the same swizzle.

 PARAMETERS	: psState			- Compiler state.
			  psCopyMap			- Details of instructions already inserted to
								swizzle source arguments.
			  psInst			- Instruction containing the argument to swizzle.
			  uArgIdx			- Index of the argument to swizzle.
			  uSwizzleToApply	- Swizzle to apply.
			  uUsedChanMask		- Mask of channels used from the argument before the
								swizzle is applied.
			  psModifier		- Source modifiers for the argument.
			  puSwizzledTempNum	- Returns the number of the intermediate register
								containing the swizzle data.
			  ppsCopyInst		- Returns a pointer to the instruction which applies
								the swizzle.

  RETURNS	: Whether a copy has been inserted, or a previously inserted instruction
			  was reused. If FALSE then the swizzle was pushed onto the instruction
			  providing the argument.
*****************************************************************************/
{
	IMG_UINT32			uArgBase = uArgIdx * SOURCE_ARGUMENTS_PER_VECTOR;
	PARG				psVecSource = &psInst->asArg[uArgBase + 0];
	VECTOR_COPY_LIST	sCopyKey;

	/*
		Get the details of the source data to be swizzled.
	*/
	sCopyKey.sModifier = *psModifier;
	if (psVecSource->uType == USC_REGTYPE_UNUSEDSOURCE)
	{
		IMG_UINT32	uSrc;

		if (psInst->u.psVec->aeSrcFmt[uArgIdx] == UF_REGFORMAT_F32)
		{
			sCopyKey.uCopySourceCount = SCALAR_REGISTERS_PER_F32_VECTOR;
		}
		else
		{
			ASSERT(psInst->u.psVec->aeSrcFmt[uArgIdx] == UF_REGFORMAT_F16);
			sCopyKey.uCopySourceCount = SCALAR_REGISTERS_PER_F16_VECTOR;
		}

		for (uSrc = 0; uSrc < sCopyKey.uCopySourceCount; uSrc++)
		{
			sCopyKey.asCopySource[uSrc] = psInst->asArg[uArgBase + 1 + uSrc];
		}
	}
	else
	{
		sCopyKey.uCopySourceCount = 1;
		sCopyKey.asCopySource[0] = *psVecSource;
	}

	/*
		Check for swizzling a uniform source. We might be able to insert the instruction to apply
		the swizzle in the secondary update program.
	*/
	if (IsUniformVectorSource(psState, psInst, uArgIdx))
	{
		if (InsertUniformSwizzleCopy(psState,
									 psCopyMap,
									 psInst,
									 uArgIdx,
									 uSwizzleToApply,
									 uUsedChanMask,
									 &sCopyKey,
									 puSwizzledTempNum,
									 ppsCopyInst))
		{
			return IMG_TRUE;
		}	
	}

	/*
		Check if we've already inserted instructions to copy/swizzle this
		source argument in the current basic block.
	*/
	if (!IsNonSSAVectorSource(psState, psInst, uArgIdx))
	{
		IMG_UINT32	uSwizzledTempNum;
		PINST		psCopyInst;

		uSwizzledTempNum = FindVectorCopy(psState,
										  psCopyMap->psLocalCopies, 
										  &sCopyKey,
										  uSwizzleToApply, 
										  uUsedChanMask,
										  &psCopyInst);
		if (uSwizzledTempNum != USC_UNDEF)
		{
			*puSwizzledTempNum = uSwizzledTempNum;
			*ppsCopyInst = psCopyInst;
			return IMG_TRUE;
		}
	}

	/* If a broadcast is required try to push it onto the instruction higher up */
	if (psModifier->bNegate	== IMG_FALSE &&
		psModifier->bAbs	== IMG_FALSE &&
		IsSwizzleInSet(g_auReplicateSwizzles,
					   sizeof(g_auReplicateSwizzles) / sizeof(g_auReplicateSwizzles[0]),
					   uSwizzleToApply,
					   uUsedChanMask,
					   NULL))
	{
		PUSEDEF_CHAIN psUseDef = UseDefGet(psState, psInst->asArg[uArgIdx].uType, psInst->asArg[uArgIdx].uNumber);
		if ((psUseDef != NULL) && (psUseDef->psDef != NULL) && (psUseDef->psDef->eType == DEF_TYPE_INST))
		{
			PINST psDefInst = psUseDef->psDef->u.psInst;
			
			if (
					(g_psInstDesc[psDefInst->eOpcode].uFlags2 & DESC_FLAGS2_VECTORREPLICATEDRESULT) != 0 &&
					NoPredicate(psState, psDefInst) && 
					((psDefInst->auLiveChansInDest[0] & ~psDefInst->auDestMask[0]) & uUsedChanMask) == 0
				)
			{
				psDefInst->auDestMask[0] |= uUsedChanMask;
				psDefInst->auLiveChansInDest[0] |= uUsedChanMask;

				psInst->u.psVec->auSwizzle[uArgIdx] = USEASM_SWIZZLE(X, Y, Z, W);

				*puSwizzledTempNum = psInst->asArg[uArgIdx].uNumber;
				*ppsCopyInst = psDefInst;
				return IMG_FALSE;
			}
		}
	}
	
	/*
		Insert an instruction to apply the swizzle immediately before the existing
		instruction.
	*/
	MakeSwizzleCopyInst(psState,
						psInst,
						psInst->psBlock,
						psInst,
						&psCopyMap->psLocalCopies,
						uArgIdx,
						uSwizzleToApply,
						&sCopyKey,
						puSwizzledTempNum,
						ppsCopyInst);

	return IMG_TRUE;
}

IMG_VOID static ConvertSwizzleToVMUL(PINTERMEDIATE_STATE	psState,
									 PVECTOR_COPY_MAP		psCopyMap,
									 PINST					psInst,
									 IMG_UINT32				uArgIdx,
									 IMG_UINT32				uSwizzleToReplace,
									 IMG_UINT32				uReplacementSwizzle,
									 IMG_BOOL				bCopySourceModifier)
/*****************************************************************************
 FUNCTION	: ConvertSwizzleToVMUL
    
 PURPOSE	: Replace a source swizzle on a vector instruction by applying
			  the swizzle in a separate VMUL or VMULF16 instruction.

 PARAMETERS	: psState			- Compiler state.
			  psCopyMap			- Details of instructions already inserted to
								swizzle source arguments.
			  psInst			- Instruction to replace the swizzle on.
			  uArgIdx			- Index of the argument to replace the swizzle on.
			  uSwizzleToReplace	- Swizzle to apply in a separate instruction.
			  uReplacementSwizzle
								- New swizzle for the argument.
			  bCopySourceModifier
								- If TRUE also copy the modifier from the source.

  RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32			uSwizzledTempNum = USC_UNDEF;
	PINST				psCopyInst = NULL;
	VEC_SOURCE_MODIFIER	sModifier;
	UF_REGFORMAT		eSwizzledFmt = psInst->u.psVec->aeSrcFmt[uArgIdx];
	IMG_UINT32			uLiveChansInArg;
	IMG_UINT32			uCopyInstSwizzle;
	IMG_UINT32			uChan;

	/*
		Get the mask of channels referenced from the source before the swizzle is applied.
	*/
	GetLiveChansInSourceSlot(psState, psInst, uArgIdx, &uLiveChansInArg, NULL /* puChansUsedPostSwizzle */);
	uLiveChansInArg = ApplySwizzleToMask(uReplacementSwizzle, uLiveChansInArg);

	if (bCopySourceModifier)
	{
		sModifier = psInst->u.psVec->asSrcMod[uArgIdx];
	}
	else
	{
		sModifier.bNegate = IMG_FALSE;
		sModifier.bAbs = IMG_FALSE;
	}

	/*
		Either insert an instruction to apply the swizzle or reuse the result of
		an existing instruction.
	*/
	if (!InsertSwizzleCopy(psState,
						   psCopyMap,
						   psInst,
						   uArgIdx,
						   uSwizzleToReplace,
						   uLiveChansInArg,
						   &sModifier,
						   &uSwizzledTempNum,
						   &psCopyInst))
	{
		return;
	}

	/*
		If we are reusing the result of an existing swizzling instruction then update
		previously unused channels in the swizzle on the existing instruction from the
		swizzle used here.
	*/
	uCopyInstSwizzle = psCopyInst->u.psVec->auSwizzle[0];
	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		if ((uLiveChansInArg & (1 << uChan)) != 0)
		{
			USEASM_SWIZZLE_SEL	eSel;

			eSel = GetChan(uSwizzleToReplace, uChan);
			uCopyInstSwizzle = SetChan(uCopyInstSwizzle, uChan, eSel);
		}
	}
	psCopyInst->u.psVec->auSwizzle[0] = uCopyInstSwizzle;

	/*
		Update the swizzling instruction destination mask to include the channels accessed from the replaced
		source.
	*/
	psCopyInst->auDestMask[0] |= uLiveChansInArg;
	psCopyInst->auLiveChansInDest[0] = psCopyInst->auDestMask[0];

	/*
		By default the swizzling instruction using VMOV which can handle any swizzle with non-constant selectors.
		If we've switched to a swizzle unsupported by VMOV then replace the instruction by VMUL.
	*/
	if (!IsNonConstantSwizzle(psCopyInst->u.psVec->auSwizzle[0], 
							  psCopyInst->auDestMask[0], 
							  &psCopyInst->u.psVec->auSwizzle[0]))
	{
		ConvertVMOVToVMUL(psState, psCopyInst);
	}

	/*
		Replace the instruction source by the swizzling instruction destination.
	*/
	SetupTempVecSource(psState, psInst, uArgIdx, uSwizzledTempNum, eSwizzledFmt);
	
	/*
		Replace the instruction swizzle.
	*/
	psInst->u.psVec->auSwizzle[uArgIdx] = uReplacementSwizzle;

	if (bCopySourceModifier)
	{
		/*		
			Clear the source modifiers on the instruction.
		*/
		psInst->u.psVec->asSrcMod[uArgIdx].bNegate = IMG_FALSE;
		psInst->u.psVec->asSrcMod[uArgIdx].bAbs = IMG_FALSE;
	}

	/*
		If the swizzling instruction is in the secondary update program and the original instruction is in
		the main program then mark the extra channels as live out of the secondary update program.
	*/
	if (psCopyInst->psBlock->psOwner->psFunc == psState->psSecAttrProg && psInst->psBlock->psOwner->psFunc != psState->psSecAttrProg)
	{
		IncreaseRegisterLiveMask(psState, 
								 &psState->psSecAttrProg->sCfg.psExit->sRegistersLiveOut, 
								 USEASM_REGTYPE_TEMP, 
								 uSwizzledTempNum, 
								 0 /* uArrayOffset */, 
								 uLiveChansInArg);
		SetChansUsedFromSAProgResult(psState, psCopyInst->asDest[0].psRegister->psSecAttr);
	}
}

IMG_BOOL static IsValidSourceSwizzle(PINTERMEDIATE_STATE	psState,
									 PINST					psInst,
									 IMG_UINT32				uArgIdx,
									 IMG_UINT32				uSwizzle,
									 IMG_PUINT32			puMatchSwizzle)
/*****************************************************************************
 FUNCTION	: IsValidSourceSwizzle
    
 PURPOSE	: Check whether instruction source argument supports a specified
			  swizzle.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction whose source is to be fixed.
			  uArgIdx			- Index of the argument to check.
			  uSwizzle			- Swizzle to check.
			  puMatchSwizzle	- If non-NULL returns an equivalent swizzle
								which is supported.

  RETURNS	: TRUE if the source swizzle is supported.
*****************************************************************************/
{
	IMG_UINT32	uLiveChans;

	/*
		Get the mask of channels accessed from the source before the current swizzle
		is applied.
	*/
	GetLiveChansInSourceSlot(psState, psInst, uArgIdx, &uLiveChans, NULL /* puChansUsedPostSwizzle */);

	/*
		Check if this swizzle is supported directly by the instruction.
	*/
	return IsSwizzleSupported(psState, psInst, psInst->eOpcode, uArgIdx, uSwizzle, uLiveChans, puMatchSwizzle);
}

IMG_BOOL static CheckSourceSwizzle(PINTERMEDIATE_STATE	psState,
								   PINST				psInst,
								   IMG_UINT32			uArgIdx)
/*****************************************************************************
 FUNCTION	: CheckSourceSwizzle
    
 PURPOSE	: Check whether the swizzle on a source argument is supported.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction whose source is to be fixed.
			  uArgIdx			- Index of the argument to fix.

  RETURNS	: TRUE if the source swizzle is supported.
*****************************************************************************/
{
	return IsValidSourceSwizzle(psState,
								psInst,
								uArgIdx,
								psInst->u.psVec->auSwizzle[uArgIdx],
								&psInst->u.psVec->auSwizzle[uArgIdx]);
}


IMG_INTERNAL
IMG_BOOL IsImmediateVectorSource(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSrcIdx)
/*****************************************************************************
 FUNCTION	: IsImmediateVectorSource

 PURPOSE	: Check if a source to a vector instruction is a vector of
			  immediates.

 PARAMETERS	: psState	- Internal compiler state
			  psInst	- Instruction to check.
			  uSrcIdx	- Index of the source to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uArgBase = uSrcIdx * SOURCE_ARGUMENTS_PER_VECTOR; 
	IMG_UINT32	uVecIdx;
	IMG_UINT32	uVecLen;

	if (psInst->asArg[uArgBase].uType != USC_REGTYPE_UNUSEDSOURCE)
	{
		return IMG_FALSE;
	}
	if (psInst->u.psVec->aeSrcFmt[uSrcIdx] == UF_REGFORMAT_F16)
	{
		uVecLen = SCALAR_REGISTERS_PER_F16_VECTOR;
	}
	else
	{
		ASSERT(psInst->u.psVec->aeSrcFmt[uSrcIdx] == UF_REGFORMAT_F32);
		uVecLen = SCALAR_REGISTERS_PER_F32_VECTOR;
	}
	for (uVecIdx = 0; uVecIdx < uVecLen; uVecIdx++)
	{
		IMG_UINT32	uArgIdx = uArgBase + 1 + uVecIdx;

		if (GetLiveChansInArg(psState, psInst, uArgIdx) != 0 && psInst->asArg[uArgIdx].uType != USEASM_REGTYPE_IMMEDIATE)
		{
			return IMG_FALSE;
		}
	}
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL GetImmediateVectorChan(PINTERMEDIATE_STATE psState, 
								PINST				psInst, 
								IMG_UINT32			uSrcIdx, 
								IMG_UINT32			uChanIdx,
								IMG_PUINT32			puChanValue)
/*****************************************************************************
 FUNCTION	: GetImmediateVectorChan

 PURPOSE	: Get the value of a channel from a vector of immediates.

 PARAMETERS	: psState		- Internal compiler state
			  psInst		- Instruction.
			  uSrcIdx		- Index of the source argument which is a vector
							of immediates.
			  uChanIdx		- Channel to get.
			  puChanValue	- Returns the value of the channel.

 RETURNS	: FALSE if the channel is not an immediate.
*****************************************************************************/
{
	IMG_UINT32			uArgBase = uSrcIdx * SOURCE_ARGUMENTS_PER_VECTOR;
	USEASM_SWIZZLE_SEL	eChanSel;
	IMG_UINT32			uSrcChan;
	IMG_BOOL			bF16;
	PARG				psImmArg;
	IMG_UINT32			uImm;

	if (psInst->u.psVec->aeSrcFmt[uSrcIdx] == UF_REGFORMAT_F16)
	{
		bF16 = IMG_TRUE;
	}
	else
	{
		bF16 = IMG_FALSE;
	}

	eChanSel = GetChan(psInst->u.psVec->auSwizzle[uSrcIdx], uChanIdx);

	ASSERT(eChanSel < (sizeof(g_asSwizzleSel) / sizeof(g_asSwizzleSel[0])));

	if (g_asSwizzleSel[eChanSel].bIsConstant)
	{
		*puChanValue = bF16 ? g_asSwizzleSel[eChanSel].uF16Value : g_asSwizzleSel[eChanSel].uF32Value;
		return IMG_TRUE;
	}

	uSrcChan = g_asSwizzleSel[eChanSel].uSrcChan;
	ASSERT(uSrcChan < VECTOR_LENGTH);

	if (psInst->u.psVec->aeSrcFmt[uSrcIdx] == UF_REGFORMAT_F16)
	{
		psImmArg = &psInst->asArg[uArgBase + 1 + (uSrcChan / 2)];
	}
	else
	{
		ASSERT(psInst->u.psVec->aeSrcFmt[uSrcIdx] == UF_REGFORMAT_F32);
		psImmArg = &psInst->asArg[uArgBase + 1 + uSrcChan];
	}

	if (psImmArg->uType == USEASM_REGTYPE_FPCONSTANT)
	{
		uImm = GetHardwareConstantValueu(psState, psImmArg->uNumber);
	}
	else if (psImmArg->uType == USEASM_REGTYPE_IMMEDIATE)
	{
		uImm = psImmArg->uNumber;
	}
	else
	{
		return IMG_FALSE;
	}

	if (psInst->u.psVec->aeSrcFmt[uSrcIdx] == UF_REGFORMAT_F16)
	{
		uImm >>= ((uSrcChan % F16_CHANNELS_PER_SCALAR_REGISTER) * WORD_SIZE * BITS_PER_BYTE);
		uImm &= (1 << (WORD_SIZE * BITS_PER_BYTE)) - 1;
	}

	*puChanValue = uImm;
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL IsReplicatedImmediateVector(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSrcIdx, IMG_UINT32 uImmValue)
/*****************************************************************************
 FUNCTION	: IsReplicatedImmediateVector

 PURPOSE	: Check if a source to an instruction is a replicated immediate
			  value.

 PARAMETERS	: psState	- Internal compiler state
			  psInst	- Instruction.
			  uSrcIdx	- Index of the source argument to check.
			  uImmValue	- Immediate value to check for.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uChan;
	IMG_UINT32	uChansUsed;

	GetLiveChansInSourceSlot(psState, 
							 psInst, 
							 uSrcIdx, 
							 &uChansUsed, 
							 NULL /* puChansUsedPostSwizzle */);

	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		if ((uChansUsed & (1 << uChan)) != 0)
		{
			IMG_UINT32	uChanImmValue;

			if (!GetImmediateVectorChan(psState, psInst, uSrcIdx, uChan, &uChanImmValue))
			{
				return IMG_FALSE;
			}
			if (uChanImmValue != uImmValue)
			{
				return IMG_FALSE;
			}
		}
	}
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_VOID SetImmediateVector(PINTERMEDIATE_STATE psState, 
							PINST				psInst, 
							IMG_UINT32			uSrcIdx, 
							IMG_UINT32			uImm)
/*****************************************************************************
 FUNCTION	: SetImmediateVector

 PURPOSE	: Set a vector of immediates.

 PARAMETERS	: psState	- Internal compiler state
			  psInst	- Instruction.
			  uSrcIdx	- Index of the source argument which is a vector
						of immediates.
			  uImm		- Value to set.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uArgBase = uSrcIdx * SOURCE_ARGUMENTS_PER_VECTOR;
	IMG_UINT32	uChanIdx;
	IMG_UINT32	uVecLength;

	if (psInst->u.psVec->aeSrcFmt[uSrcIdx] == UF_REGFORMAT_F16)
	{
		uVecLength = SCALAR_REGISTERS_PER_F16_VECTOR;
		uImm |= (uImm << 16);
	}
	else
	{
		ASSERT(psInst->u.psVec->aeSrcFmt[uSrcIdx] == UF_REGFORMAT_F32);
		uVecLength = SCALAR_REGISTERS_PER_F32_VECTOR;
	}

	SetSrcUnused(psState, psInst, uArgBase);
	for (uChanIdx = 0; uChanIdx < uVecLength; uChanIdx++)
	{
		PARG	psImmArg;

		psImmArg = &psInst->asArg[uArgBase + 1 + uChanIdx];

		InitInstArg(psImmArg);
		psImmArg->uType = USEASM_REGTYPE_IMMEDIATE;
		psImmArg->uNumber = uImm;
		psImmArg->eFmt = psInst->u.psVec->aeSrcFmt[uSrcIdx];
	}
}

IMG_INTERNAL
IMG_VOID UnpackImmediateVector(PINTERMEDIATE_STATE	psState, 
							   PINST				psInst, 
							   IMG_UINT32			uSource, 
							   IMG_UINT32			uLiveChans,
							   IMG_PUINT32			auData)
/*****************************************************************************
 FUNCTION	: UnpackImmediateVector

 PURPOSE	: Get the value of an immediate vector source after the swizzle is
			  applied.

 PARAMETERS	: psState		- Internal compiler state
			  psInst		- Instruction.
			  uSource		- Index of the source argument to get.
			  uLiveChans	- Mask of channels to get the values of.
			  auData		- Returns the value of each channel in F32 or F16
							format packed into consecutive bits.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uChanWidthInBits;
	UF_REGFORMAT	eSrcFmt = psInst->u.psVec->aeSrcFmt[uSource];
	IMG_UINT32		uChan;

	if (eSrcFmt == UF_REGFORMAT_F32)
	{
		uChanWidthInBits = F32_CHANNEL_SIZE_IN_BITS;
	}
	else
	{
		ASSERT(eSrcFmt == UF_REGFORMAT_F16);
		uChanWidthInBits = F16_CHANNEL_SIZE_IN_BITS;
	}

	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		if ((uLiveChans & (1 << uChan)) != 0)
		{
			IMG_UINT32	uChanStart;
			IMG_UINT32	uChanEnd;
			IMG_UINT32	uChanValue;
			IMG_BOOL	bRet;

			bRet = GetImmediateVectorChan(psState, psInst, uSource, uChan, &uChanValue);
			ASSERT(bRet);

			uChanStart = uChan * uChanWidthInBits;
			uChanEnd = uChanStart + uChanWidthInBits - 1;
			SetRange(auData, uChanEnd, uChanStart, uChanValue);
		}
	}
}

static
IMG_VOID SetHardwareConstantVectorSource(PINTERMEDIATE_STATE	psState,
										 PINST					psInst,
										 IMG_UINT32				uSlot,
										 IMG_UINT32				uConst,
										 UF_REGFORMAT			eFormat)
/*****************************************************************************
 FUNCTION	: SetHardwareConstantVectorSource

 PURPOSE	: Sets a vector instruction source to a vector of hardware constants
			  starting at a specified register number.

 PARAMETERS	: psState		- Internal compiler state
			  psInst		- Instruction.
			  uSource		- Index of the source argument to set.
			  uConst		- Register number of the first hardware constant.
			  eFormat		- Format of the data from the constant.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uArgBase;
	IMG_UINT32	uArgCount;
	IMG_UINT32	uArg;

	uArgBase = uSlot * SOURCE_ARGUMENTS_PER_VECTOR + 1;

	if (eFormat == UF_REGFORMAT_F32)
	{
		uArgCount = SCALAR_REGISTERS_PER_F32_VECTOR;
	}
	else
	{
		ASSERT(eFormat == UF_REGFORMAT_F16);
		uArgCount = SCALAR_REGISTERS_PER_F16_VECTOR;
	}

	for (uArg = 0; uArg < uArgCount; uArg++)
	{
		SetSrc(psState, 
			   psInst,
			   uArgBase + uArg,
			   USEASM_REGTYPE_FPCONSTANT, 
			   (uConst << SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT) + uArg, 
			   eFormat);
	}
	for (; uArg < MAX_SCALAR_REGISTERS_PER_VECTOR; uArg++)
	{
		SetSrcUnused(psState, psInst, uArgBase + uArg);
	}
}

IMG_BOOL static IsReplicateSwizzle(IMG_UINT32 uSwizzle, IMG_PUINT32 puReplicatedChannel)
/*****************************************************************************
 FUNCTION	: IsReplicateSwizzle

 PURPOSE	: Check if a swizzle replicates a single, non-constant channel.

 PARAMETERS	: uSwizzle				- Swizzle to check.
			  puReplicatedChannel	- On success returned the channel which
									is replicated.

 RETURNS	: TRUE if the swizzle is a replicate swizzle.
*****************************************************************************/
{
	switch (uSwizzle)
	{
		case USEASM_SWIZZLE(X, X, X, X): *puReplicatedChannel = 0; return IMG_TRUE;
		case USEASM_SWIZZLE(Y, Y, Y, Y): *puReplicatedChannel = 1; return IMG_TRUE;
		case USEASM_SWIZZLE(Z, Z, Z, Z): *puReplicatedChannel = 2; return IMG_TRUE;
		case USEASM_SWIZZLE(W, W, W, W): *puReplicatedChannel = 3; return IMG_TRUE;
		default: return IMG_FALSE;
	}
}

IMG_UINT32 static GetVTESTImmediateSwizzle(PINTERMEDIATE_STATE	psState, 
										   PINST				psInst,
										   IMG_UINT32			uImmediateSource, 
										   IMG_UINT32			uChanSel,
										   IMG_UINT32			uNonImmediateChan,
										   IMG_PUINT32			puFoundChan)
/*****************************************************************************
 FUNCTION	: GetVTESTImmediateSwizzle

 PURPOSE	: Look for a hardware constant which has the same value as the
			  existing source when combined with a supported swizzle.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- VTEST instruction.
			  uImmediateSource	- Existing immediate source.
			  uChanSel			- Channel from the ALU result which is
								tested to generate the predicate result.
			  uNonImmediateChan	- Channel which is selected by the source
								swizzle from the other source to the VTEST
								instruction.
			  puFoundChan		- Returns the channel in the hardware constant
								which contains the required value.

 RETURNS	: The number of the hardware constant or USC_UNDEF if no hardware
			  constant was found.
*****************************************************************************/
{
	
	IMG_UINT32		auImmData[MAX_SCALAR_REGISTERS_PER_VECTOR];
	IMG_UINT32		uLiveChans = 1 << uChanSel;
	UF_REGFORMAT	eImmediateSourceFormat = psInst->u.psVec->aeSrcFmt[uImmediateSource];
	IMG_UINT32		uConst;
	IMG_UINT32		uFoundConst;
	IMG_UINT32		uFoundChan;

	/*
		Apply the swizzle to the immediate source.
	*/
	UnpackImmediateVector(psState, psInst, uImmediateSource, uLiveChans, auImmData);

	uFoundConst = uFoundChan = USC_UNDEF;
	for (uConst = 0; uConst < SGXVEC_USE_SPECIAL_NUM_FLOAT_CONSTANTS; uConst++)
	{
		SWIZZLE_SPEC	sSwizzle;

		/*
			Check if this hardware constant has the same value as the immediate source.
		*/
		if (!IsSwizzledHardwareConstant(psState, 
									   (IMG_PUINT8)auImmData, 
									   uLiveChans, 
									   uConst, 
									   eImmediateSourceFormat, 
									   &sSwizzle))
		{	
			continue;
		}

		if ((sSwizzle.auChanMask[uChanSel] & (1 << uNonImmediateChan)) != 0)
		{
			uFoundConst = uConst;
			uFoundChan = uNonImmediateChan;
			if (eImmediateSourceFormat != UF_REGFORMAT_F32 || uNonImmediateChan <= USC_Y_CHAN)
			{
				*puFoundChan = uFoundChan;
				return uFoundConst;
			}
		}
		if ((sSwizzle.auChanMask[uChanSel] & USC_X_CHAN_MASK) != 0)
		{
			*puFoundChan = USC_X_CHAN;
			return uConst;
		}
	}

	*puFoundChan = uFoundChan;
	return uFoundConst;
}

static
IMG_VOID SimplifyVTESTChanSel(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: SimplifyVTESTChanSel

 PURPOSE	: Try to reduce a VTEST instruction using a channel operator combining
			  tests on multiple channels to one selecting only a single channel.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.

 RETURNS	: Nothing.
*****************************************************************************/
{
	VTEST_CHANSEL	eChanSel = psInst->u.psVec->sTest.eChanSel;

	/*
		For a test instruction combining a test on each channel of the ALU result check if each
		channel actually contains the same value.
	*/
	if (eChanSel == VTEST_CHANSEL_ORALL || eChanSel == VTEST_CHANSEL_ANDALL)
	{
		IMG_UINT32	auRepImmValue[2];
		IMG_UINT32	uArg;

		for (uArg = 0; uArg < 2; uArg++)
		{
			if (IsImmediateVectorSource(psState, psInst, uArg))
			{
				IMG_BOOL	bRet;
				IMG_UINT32	uChan;

				/*
					Check an immediate source has the same immediate value for every channel.
				*/
				bRet = GetImmediateVectorChan(psState, psInst, uArg, 0 /* uChan */, &auRepImmValue[uArg]);
				ASSERT(bRet);

				for (uChan = 1; uChan < VECTOR_LENGTH; uChan++)
				{
					IMG_UINT32	uChanValue;

					bRet = GetImmediateVectorChan(psState, psInst, uArg, uChan, &uChanValue);
					ASSERT(bRet);

					if (auRepImmValue[uArg] != uChanValue)
					{
						return;
					}
				}
			}
			else
			{
				IMG_UINT32	uReplicateChan;

				/*
					Check a non-immediate source is using a replicate swizzle.
				*/
				if (!IsReplicateSwizzle(psInst->u.psVec->auSwizzle[uArg], &uReplicateChan))
				{
					return;
				}
			}
		}

		/*
			Simplify the instruction to test only a single channel.
		*/
		psInst->u.psVec->sTest.eChanSel = eChanSel = VTEST_CHANSEL_XCHAN;
	}
}

IMG_VOID static CheckAndFixSourceSwizzle(PINTERMEDIATE_STATE	psState,
										 PVECTOR_COPY_MAP		psCopyMap,
										 PINST					psInst,
										 IMG_UINT32				uArgIdx)
/*****************************************************************************
 FUNCTION	: CheckAndFixSourceSwizzle
    
 PURPOSE	: Check whether the swizzle on a source argument is supported and if
			  it isn't then insert separate instructions to apply the swizzle to
			  the source data.

 PARAMETERS	: psState			- Compiler state.
			  psCopyMap			- Details of instructions already inserted to
								swizzle source arguments.
			  psInst			- Instruction whose source is to be fixed.
			  uArgIdx			- Index of the argument to fix.

  RETURNS	: None.
*****************************************************************************/
{
	if (!CheckSourceSwizzle(psState,
							psInst, 
							uArgIdx))
	{
		ConvertSwizzleToVMUL(psState, 
							 psCopyMap, 
							 psInst, 
							 uArgIdx, 
							 psInst->u.psVec->auSwizzle[uArgIdx],
							 USEASM_SWIZZLE(X, Y, Z, W),
							 IMG_FALSE /* bCopySourceModifier */);
	}
}

static
IMG_BOOL FixHardwareConstantSwizzle(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uArgIdx)
/*****************************************************************************
 FUNCTION	: FixHardwareConstantSwizzle
    
 PURPOSE	: Try to replace an immediate source with an unsupported swizzle
			  by a hardware constant with a supported swizzle.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to fix.
			  uArgIdx			- Source to fix.

  RETURNS	: TRUE if the swizzle on the source was replaced by one supported
			  by the instruction.
*****************************************************************************/
{
	IMG_UINT32				auData[MAX_SCALAR_REGISTERS_PER_VECTOR];
	IMG_UINT32				uLiveChans;
	UF_REGFORMAT			eSourceFormat = psInst->u.psVec->aeSrcFmt[uArgIdx];
	IMG_UINT32				uConst;
	SWIZZLE_SUPPORT_TYPE	eSupportType;
	PCSWIZZLE_SET			psSupportedSet;

	if (!IsImmediateVectorSource(psState, psInst, uArgIdx))
	{
		return IMG_FALSE;
	}

	/*
		Get the mask of channels accessed from the source before the current swizzle
		is applied.
	*/
	GetLiveChansInSourceSlot(psState, psInst, uArgIdx, &uLiveChans, NULL /* puChansUsedPostSwizzle */);
	
	/*
		Get the value of the immediate source after the swizzle is applied.
	*/
	UnpackImmediateVector(psState, psInst, uArgIdx, uLiveChans, auData);

	/*
		Get the list of swizzles supported by this instruction source.
	*/
	eSupportType = GetSwizzleSupport(psState, psInst, psInst->eOpcode, uArgIdx, &psSupportedSet);

	for (uConst = 0; uConst < SGXVEC_USE_SPECIAL_NUM_FLOAT_CONSTANTS; uConst++)
	{
		SWIZZLE_SPEC	sSwizzle;
		IMG_UINT32		uSwizzle;

		/*
			Check if this constant with a swizzle matches the supplied vector.
		*/
		if (!IsSwizzledHardwareConstant(psState, (IMG_PUINT8)auData, uLiveChans, uConst, eSourceFormat, &sSwizzle))
		{
			continue;
		}

		/*
			Check if the swizzle is supported by this instruction source.
		*/
		if (eSupportType == SWIZZLE_SUPPORT_TYPE_ANY || eSupportType == SWIZZLE_SUPPORT_TYPE_ANY_NONCONST)
		{
			uSwizzle = SpecToSwizzle(psState, &sSwizzle, uLiveChans);
		}
		else
		{
			uSwizzle = MatchSwizzleSet(psSupportedSet, &sSwizzle, uLiveChans);
		}
		if (uSwizzle != USC_UNDEF)
		{
			/*
				Replace the existing instruction source by the hardware source.
			*/
			SetHardwareConstantVectorSource(psState, psInst, uArgIdx, uConst, eSourceFormat);

			/*
				Update the source swizzle.
			*/
			psInst->u.psVec->auSwizzle[uArgIdx] = uSwizzle;

			return IMG_TRUE;
		}
	}

	return IMG_FALSE;
}

static
IMG_BOOL TryReplacingVMADWithVMAD4(PINTERMEDIATE_STATE	psState,
								   PINST				psInst)
/*****************************************************************************
 FUNCTION	: TryReplacingVMADWithVMAD4
    
 PURPOSE	: If the given instruction is a VMAD, try converting it into a VMAD4
			  if the swizzle is then supported.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- An instruction with an unsupported swizzle.

  RETURNS	: TRUE if the replacement has taken place, FALSE otherwise.
*****************************************************************************/
{
	IMG_UINT32 uArgIdx, uInvalidSwizzleMask;

	if (psInst->eOpcode != IVMAD)
	{
		return IMG_FALSE;
	}

	if (psInst->asDest[0].eFmt != UF_REGFORMAT_F32)
	{
		return IMG_FALSE;
	}

	ModifyOpcode(psState, psInst, IVMAD4);

	/*
		Check for each source whether it's swizzle is supported by the hardware.
	*/
	uInvalidSwizzleMask = 0;
	for (uArgIdx = 0; uArgIdx < GetSwizzleSlotCount(psState, psInst); uArgIdx++)
	{
		if (!CheckSourceSwizzle(psState,
			psInst, 
			uArgIdx))
		{
			uInvalidSwizzleMask |= (1 << uArgIdx);
		}
	}

	if (uInvalidSwizzleMask == 0)
	{
		return IMG_TRUE;
	}

	ModifyOpcode(psState, psInst, IVMAD);
	return IMG_FALSE;
}

static
IMG_VOID FixVectorInstSwizzles(PINTERMEDIATE_STATE psState, PVECTOR_COPY_MAP psCopyMap, PINST psInst)
/*****************************************************************************
 FUNCTION	: FixVectorInstSwizzles
    
 PURPOSE	: Fix swizzles on a vector instruction.

 PARAMETERS	: psState		- Compiler state.
			  psCopyMap		- Details of instructions already inserted to
							swizzle source arguments.
			  psInst		- Instruction to fix.

  RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uInvalidSwizzleMask;
	IMG_UINT32	uArgIdx;

	/*
		Check for each source whether it's swizzle is supported by the hardware.
	*/
	uInvalidSwizzleMask = 0;
	for (uArgIdx = 0; uArgIdx < GetSwizzleSlotCount(psState, psInst); uArgIdx++)
	{
		if (CheckSourceSwizzle(psState, psInst, uArgIdx))
		{
			continue;
		}
		if (FixHardwareConstantSwizzle(psState, psInst, uArgIdx))
		{
			continue;
		}
		uInvalidSwizzleMask |= (1 << uArgIdx);
	}

	if (uInvalidSwizzleMask != 0)
	{
		/*
			Try swapping the first two sources if the swizzle on at least one is supported in the new position.
		*/
		if (VectorSources12Commute(psInst) && (uInvalidSwizzleMask & 3) != 0)
		{
			IMG_UINT32	uArgIdx;
			IMG_UINT32	uNewInvalidSwizzleCount;
			IMG_UINT32	uOldInvalidSwizzleCount;
			IMG_UINT32	auNewSwizzle[2];
			IMG_UINT32	uNewInvalidSwizzleMask;

			uNewInvalidSwizzleMask = uInvalidSwizzleMask & ~3;
			uNewInvalidSwizzleCount = uOldInvalidSwizzleCount = 0;
			for (uArgIdx = 0; uArgIdx < 2; uArgIdx++)
			{
				auNewSwizzle[uArgIdx] = psInst->u.psVec->auSwizzle[1 - uArgIdx];

				/*
					Check if the swizzle from the other source is supported on its new position.
				*/
				if (!IsValidSourceSwizzle(psState, psInst, uArgIdx, auNewSwizzle[uArgIdx], &auNewSwizzle[uArgIdx]))
				{
					uNewInvalidSwizzleCount++;
					uNewInvalidSwizzleMask |= (1 << uArgIdx);
				}

				if ((uInvalidSwizzleMask & (1 << uArgIdx)) != 0)
				{
					uOldInvalidSwizzleCount++;
				}
			}

			if (uNewInvalidSwizzleCount < uOldInvalidSwizzleCount)
			{
				/*
					Check other restrictions on swapping the sources and, if possible, swap them.
				*/
				if (TrySwapVectorSources(psState, psInst, 0 /* uArg1 */, 1 /* uArg2 */, IMG_FALSE /* bCheckSwizzles */))
				{
					/*
						Update the mask of swizzles which need to be applied in separate instructions.
					*/
					uInvalidSwizzleMask = uNewInvalidSwizzleMask;

					/*
						Update the instruction's source swizzles.
					*/
					for (uArgIdx = 0; uArgIdx < 2; uArgIdx++)
					{
						psInst->u.psVec->auSwizzle[uArgIdx] = auNewSwizzle[uArgIdx];
					}
				}
			}
		}
		
		if ((uInvalidSwizzleMask != 0) && TryReplacingVMADWithVMAD4(psState, psInst))
		{
			return;
		}

		/*
			Apply unsupported source swizzles in separate instructions.
		*/
		for (uArgIdx = 0; uArgIdx < GetSwizzleSlotCount(psState, psInst); uArgIdx++)
		{
			if ((uInvalidSwizzleMask & (1 << uArgIdx)) != 0)
			{
				ConvertSwizzleToVMUL(psState, 
									 psCopyMap, 
									 psInst, 
									 uArgIdx, 
									 psInst->u.psVec->auSwizzle[uArgIdx],
									 USEASM_SWIZZLE(X, Y, Z, W),
									 IMG_FALSE /* bCopySourceModifier */);
			}
		}
	}
}

static
IMG_BOOL IsValidVTESTSwizzleSet(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 auSwizzles[], IMG_UINT32 uSecondSource)
/*****************************************************************************
 FUNCTION	: IsValidVTESTSwizzleSet
    
 PURPOSE	: Check if a set of swizzles is valid for a VTEST instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  auSwizzles	- Set of swizzles to check.
			  uSecondSource	- 

  RETURNS	: TRUE or FALSE
*****************************************************************************/
{
	IMG_UINT32	auChan[2];
	IMG_UINT32	uArg;

	for (uArg = 0; uArg < 2; uArg++)
	{
		USEASM_SWIZZLE_SEL	eSel;

		eSel = GetChan(auSwizzles[uArg], USC_X_CHAN);
		if (g_asSwizzleSel[eSel].bIsConstant)
		{
			return IMG_FALSE;
		}

		auChan[uArg] = g_asSwizzleSel[eSel].uSrcChan;
	}

	if (auChan[1] != auChan[0] && auChan[1] != USC_X_CHAN)
	{
		IMG_UINT32	uFoundConst;
		IMG_UINT32	uFoundChan;

		if (!IsImmediateVectorSource(psState, psInst, uSecondSource))
		{
			return IMG_FALSE;
		}

		/*
			Look for a hardware constant where one of the channels contains the same value
			as the data used from the existing immediate source. And the channel is either
			the same as the channel used from the non-immediate source or is the X channel.
		*/
		uFoundConst = 
			GetVTESTImmediateSwizzle(psState, 
									 psInst,
									 uSecondSource, 
									 USC_X_CHAN,
									 auChan[0], 
									 &uFoundChan);
		if (uFoundConst == USC_UNDEF)
		{
			return IMG_FALSE;
		}

		SetHardwareConstantVectorSource(psState, 
										psInst, 
										uSecondSource, 
										uFoundConst, 
										psInst->u.psVec->aeSrcFmt[uSecondSource]);
		auChan[1] = uFoundChan;
	}

	for (uArg = 0; uArg < 2; uArg++)
	{
		auSwizzles[uArg] = g_auReplicateSwizzles[auChan[uArg]];
	}
	return IMG_TRUE;
}


IMG_VOID static FixVTESTSwizzles(PINTERMEDIATE_STATE	psState, 
								 PVECTOR_COPY_MAP		psCopyMap,
								 PINST					psInst)
/*****************************************************************************
 FUNCTION	: FixVTESTSwizzles
    
 PURPOSE	: Fix swizzles on a vector TEST instruction.

 PARAMETERS	: psState		- Compiler state.
			  psCopyMap		- Details of instructions already inserted to
							swizzle source arguments.
			  psInst		- Instruction to fix.

  RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32			uArg;

	ASSERT(psInst->eOpcode == IVTEST);
	ASSERT(psInst->uDestCount == VTEST_PREDICATE_ONLY_DEST_COUNT);

	/*
		Try to modify the instruction to simplify the channel operator from one combining multiple channels
		to one selecting a single channel. This gives more options for supporting swizzles.
	*/
	SimplifyVTESTChanSel(psState, psInst);

	if (IsVTESTExtraSwizzles(psState, psInst->eOpcode, psInst))
	{
		/*
			Check if the swizzles are supported by the instruction with the sources in
			their original order.
		*/
		if (IsValidVTESTSwizzleSet(psState, psInst, psInst->u.psVec->auSwizzle, 1 /* uSecondSource */))
		{
			return;
		}

		/*
			Check if the instruction sources could be in either order.
		*/
		if (VectorSources12Commute(psInst))
		{
			/* 
				We already fixed vector source modifier so check the modifier for each source is valid in
				the swapped position.
			*/
			if (IsValidVectorSourceModifier(psInst, psInst->eOpcode, 0 /* uSlotIdx */, &psInst->u.psVec->asSrcMod[1]) &&
				IsValidVectorSourceModifier(psInst, psInst->eOpcode, 0 /* uSlotIdx */, &psInst->u.psVec->asSrcMod[0]))
			{
				IMG_UINT32	auSwappedSwizzles[2];

				for (uArg = 0; uArg < 2; uArg++)
				{
					auSwappedSwizzles[uArg] = psInst->u.psVec->auSwizzle[1 - uArg];
				}

				/*
					Check if the swizzles are supported by the instruction with the sources swapped.
				*/
				if (IsValidVTESTSwizzleSet(psState, psInst, auSwappedSwizzles, 0 /* uSecondSource */))
				{
					for (uArg = 0; uArg < 2; uArg++)
					{
						psInst->u.psVec->auSwizzle[uArg] = auSwappedSwizzles[1 - uArg];
					}

					/*
						Swapped the order of the sources.
					*/
					SwapVectorSources(psState, psInst, 0 /* uArg1Idx */, psInst, 1 /* uArg2Idx */);
					return;
				}
			}
		}

		/*
			Insert an extra instruction to apply the swizzle to the second source.
		*/
		ConvertSwizzleToVMUL(psState, 
							 psCopyMap, 
							 psInst, 
							 1 /* uArgIdx */, 
							 psInst->u.psVec->auSwizzle[1],
							 USEASM_SWIZZLE(X, X, X, X),
							 IMG_FALSE /* bCopySourceModifier */);
	}
	else
	{
		FixVectorInstSwizzles(psState, psCopyMap, psInst);
	}
}

IMG_VOID static DeleteCopyList(IMG_PVOID pvState, IMG_PVOID pvElem)
/*****************************************************************************
 FUNCTION	: DeleteCopyList
    
 PURPOSE	: Delete an element from a map between intermediate registers and
			  instructions applying a source swizzle/modifier to that intermediate
			  register.

 PARAMETERS	: pvElem			- Element to delete.
			  pvState			- Compiler state.

  RETURNS	: None.
*****************************************************************************/
{
	PINTERMEDIATE_STATE	psState = (PINTERMEDIATE_STATE)pvState;
	PVECTOR_COPY_LIST	psElem = (PVECTOR_COPY_LIST)pvElem;
	PUSC_LIST_ENTRY		psListEntry;

	while ((psListEntry = RemoveListHead(&psElem->sCopyList)) != NULL)
	{
		PVECTOR_COPY	psCopy;

		psCopy = IMG_CONTAINING_RECORD(psListEntry, PVECTOR_COPY, sCopyListEntry);
		UscFree(psState, psCopy);
	}
}

static
IMG_UINT32 IsDecomposableSwizzle(PINTERMEDIATE_STATE psState, IMG_UINT32 uSwizzleA, IMG_UINT32 uSwizzleB, IMG_UINT32 uChanMask)
/*****************************************************************************
 FUNCTION	: IsDecomposableSwizzle
    
 PURPOSE	: Return SwizzleC s.t. Compose(SwizzleC, SwizzleB) = SwizzleA

 PARAMETERS	: psState			- Compiler state.
			  uSwizzleA			- Second part of the decomposition.
			  uSwizzleB			- Swizzle to decompose.
			  uChanMask			- Mask of channels used from uSwizzleB.

  RETURNS	: Either SwizzleC or USC_UNDEF if decomposition isn't possible.
*****************************************************************************/
{
	IMG_UINT32			uChan;
	IMG_UINT32			uInverseSwizzle;
	USEASM_SWIZZLE_SEL	aeSwizzle[VECTOR_LENGTH];

	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		aeSwizzle[uChan] = USEASM_SWIZZLE_SEL_UNDEF;
	}
	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		if ((uChanMask & (1 << uChan)) != 0)
		{
			USEASM_SWIZZLE_SEL	eSelA, eSelB;

			eSelA = GetChan(uSwizzleA, uChan);
			eSelB = GetChan(uSwizzleB, uChan);

			ASSERT(eSelA < (sizeof(g_asSwizzleSel) / sizeof(g_asSwizzleSel[0])));

			if (g_asSwizzleSel[eSelA].bIsConstant)
			{
				if (eSelA != eSelB)
				{
					return USC_UNDEF;
				}
			}
			else
			{
				IMG_UINT32	uSrcChanA;

				uSrcChanA = g_asSwizzleSel[eSelA].uSrcChan;
				if (aeSwizzle[uSrcChanA] != USEASM_SWIZZLE_SEL_UNDEF && aeSwizzle[uSrcChanA] != eSelB)
				{
					return USC_UNDEF;
				}
				aeSwizzle[uSrcChanA] = eSelB;
			}
		}
	}

	uInverseSwizzle = USEASM_SWIZZLE(0, 0, 0, 0);
	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		if (aeSwizzle[uChan] != USEASM_SWIZZLE_SEL_UNDEF)
		{
			uInverseSwizzle = SetChan(uInverseSwizzle, uChan, aeSwizzle[uChan]);
		}
	}
	
	return uInverseSwizzle;
}

static
IMG_BOOL FixVMOVCOneImmediateSource(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uImmediateSource, IMG_UINT32 uLiveChans)
/*****************************************************************************
 FUNCTION	: FixVMOVCTwoImmediateSources
    
 PURPOSE	: Fix the swizzles on the second two sources to a vector conditional 
			  move instruction where one source is an immediate vectors.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to fix.
			  uImmediateSource
							- Number of the source which is an immediate vector.
			  uLiveChans	- Mask of channels used from the second two
							sources before the swizzle is applied.

  RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32		uNonImmediateSwizzle;
	IMG_UINT32		uNonImmediateSource;
	UF_REGFORMAT	eImmediateSourceFormat;
	IMG_UINT32		uSrc;
	IMG_UINT32		uFoundConst;
	IMG_UINT32		auImmData[MAX_SCALAR_REGISTERS_PER_VECTOR];
	IMG_UINT32		uConst;
	
	/*
		Check if the swizzle on the immediate source can be made the same
		as the swizzle on the other source.
	*/

	uNonImmediateSource = (uImmediateSource == 1) ? 2 : 1;
	uNonImmediateSwizzle = psInst->u.psVec->auSwizzle[uNonImmediateSource];

	eImmediateSourceFormat = psInst->u.psVec->aeSrcFmt[uImmediateSource];

	/*
		Check the swizzle on the other source is supported by the instruction.
	*/
	if (!IsSwizzleSupported(psState, 
							psInst, 
							psInst->eOpcode, 
							uNonImmediateSource, 
							uNonImmediateSwizzle, 
							uLiveChans, 
							&uNonImmediateSwizzle))
	{
		return IMG_FALSE;
	}
	if (!IsNonConstantSwizzle(uNonImmediateSwizzle, uLiveChans, NULL /* puNonImmediateSwizzle */))
	{
		return IMG_FALSE;
	}

	/*
		Get the immediate source after the swizzle is applied.
	*/
	UnpackImmediateVector(psState, psInst, uImmediateSource, uLiveChans, auImmData);

	uFoundConst = USC_UNDEF;
	for (uConst = 0; uConst < SGXVEC_USE_SPECIAL_NUM_FLOAT_CONSTANTS; uConst++)
	{
		SWIZZLE_SPEC	sSwizzle;

		/*
			Check if this hardware constant after a swizzle matches the immediate source.
		*/
		if (!IsSwizzledHardwareConstant(psState, 
									   (IMG_PUINT8)auImmData, 
									   uLiveChans, 
									   uConst, 
									   eImmediateSourceFormat, 
									   &sSwizzle))
		{
			continue;
		}

		/*
			Check if the swizzle on the hardware constant and the swizzle on the other
			source are compatible.
		*/
		if (MatchSwizzle(uNonImmediateSwizzle, &sSwizzle, uLiveChans))
		{
			uFoundConst = uConst;
			break;
		}
	}
	if (uFoundConst == USC_UNDEF)
	{
		return IMG_FALSE;
	}

	/*
		Replace the immediate source by the hardware constant.
	*/
	SetHardwareConstantVectorSource(psState, psInst, uImmediateSource, uFoundConst, eImmediateSourceFormat);
	for (uSrc = 0; uSrc < 2; uSrc++)
	{
		psInst->u.psVec->auSwizzle[1 + uSrc] = uNonImmediateSwizzle;
	}

	return IMG_TRUE;
}

static
IMG_BOOL FixVMOVCTwoImmediateSources(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uLiveChans)
/*****************************************************************************
 FUNCTION	: FixVMOVCTwoImmediateSources
    
 PURPOSE	: Fix the swizzles on the second two sources to a vector conditional 
			  move instruction where both sources are immediate vectors.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to fix.
			  uLiveChans	- Mask of channels used from the second two
							sources before the swizzle is applied.

  RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32		uChan;
	IMG_UINT32		auSrcData[2][MAX_SCALAR_REGISTERS_PER_VECTOR];
	UF_REGFORMAT	aeSrcFmt[2];
	IMG_UINT32		uSrc1Const;
	IMG_UINT32		auFoundConst[2];
	IMG_UINT32		uFoundSwizzle;
	IMG_UINT32		uFoundUsedChanCount;
	IMG_UINT32		uSrc;

	memset(auSrcData, 0, sizeof(auSrcData));

	/*
		Get both immediate sources after their swizzles are applied.
	*/
	for (uSrc = 0; uSrc < 2; uSrc++)
	{
		aeSrcFmt[uSrc] = psInst->u.psVec->aeSrcFmt[1 + uSrc];

		UnpackImmediateVector(psState, psInst, 1 + uSrc, uLiveChans, auSrcData[uSrc]);
	}

	auFoundConst[0] = auFoundConst[1] = uFoundSwizzle = uFoundUsedChanCount = USC_UNDEF;
	for (uSrc1Const = 0; uSrc1Const < SGXVEC_USE_SPECIAL_NUM_FLOAT_CONSTANTS; uSrc1Const++)
	{
		IMG_UINT32		uSrc2Const;
		SWIZZLE_SPEC	sSrc1Swizzle;

		/*
			Check if this hardware constant with a swizzle matches the first immediate source.
		*/
		if (!IsSwizzledHardwareConstant(psState, 
									   (IMG_PUINT8)auSrcData[0], 
									   uLiveChans, 
									   uSrc1Const, 
									   aeSrcFmt[0], 
									   &sSrc1Swizzle))
		{
			continue;
		}

		for (uSrc2Const = 0; uSrc2Const < SGXVEC_USE_SPECIAL_NUM_FLOAT_CONSTANTS; uSrc2Const++)
		{
			SWIZZLE_SPEC	sSrc2Swizzle;
			SWIZZLE_SPEC	sCombinedSwizzle;

			/*
				Check if this hardware constant with a swizzle matches the second immediate source.
			*/
			if (IsSwizzledHardwareConstant(psState, 
										  (IMG_PUINT8)auSrcData[1], 
										  uLiveChans, 
										  uSrc2Const, 
										  aeSrcFmt[1], 
										  &sSrc2Swizzle))
			{
				IMG_BOOL	bBothSourcesValid;

				/*
					Combine the possible source channels which can be selected on each source.
				*/
				bBothSourcesValid = IMG_TRUE;
				for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
				{
					if ((uLiveChans & (1 << uChan)) != 0)
					{	
						sCombinedSwizzle.auChanMask[uChan] = sSrc1Swizzle.auChanMask[uChan] & sSrc2Swizzle.auChanMask[uChan];
						if (sCombinedSwizzle.auChanMask[uChan] == 0)
						{
							bBothSourcesValid = IMG_FALSE;
							break;
						}
					}
					else
					{
						sCombinedSwizzle.auChanMask[uChan] = 0;
					}
				}

				if (bBothSourcesValid)
				{
					IMG_UINT32	uMatchSwizzle;

					/*
						Check if a swizzle compatible with both hardware constants is supported by VMOVC.
					*/
					uMatchSwizzle = MatchSwizzleSet(&g_sVEC4StdSwizzles, 
													&sCombinedSwizzle,
													uLiveChans);
					if (uMatchSwizzle != 0)
					{
						IMG_UINT32	uUsedChanMask;
						IMG_UINT32	uUsedChanCount;

						uUsedChanMask = ApplySwizzleToMask(uMatchSwizzle, uLiveChans);
						uUsedChanCount = g_auMaxBitCount[uUsedChanMask];

						if (uFoundSwizzle == USC_UNDEF || uUsedChanCount < uFoundUsedChanCount)
						{
							auFoundConst[0] = uSrc1Const;
							auFoundConst[1] = uSrc2Const;
							uFoundSwizzle = uMatchSwizzle;
							uFoundUsedChanCount = uUsedChanCount;
						}
					}
				}
			}
		}	
	}

	if (uFoundSwizzle == USC_UNDEF)
	{
		return IMG_FALSE;
	}

	/*
		Replace both immediate sources by hardware constants.
	*/
	for (uSrc = 0; uSrc < 2; uSrc++)
	{
		SetHardwareConstantVectorSource(psState, psInst, 1 + uSrc, auFoundConst[uSrc], aeSrcFmt[uSrc]);
		psInst->u.psVec->auSwizzle[1 + uSrc] = uFoundSwizzle;
	}
	return IMG_TRUE;
}

static
IMG_VOID FixVMOVCSwizzles(PINTERMEDIATE_STATE psState, PVECTOR_COPY_MAP	psCopyMap, PINST psInst)
/*****************************************************************************
 FUNCTION	: FixVMOVCSwizzles
    
 PURPOSE	: Fix swizzles on a vector conditional move instruction.

 PARAMETERS	: psState		- Compiler state.
			  psCopyMap		- Details of instructions already inserted to
							swizzle source arguments.
			  psInst		- Instruction to fix.

  RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uSwizzle;
	IMG_BOOL	bInvalidSwizzle = IMG_FALSE;
	IMG_UINT32	uLiveChans;
	IMG_UINT32	uLiveChansInSlot2;
	IMG_UINT32	uLiveChansPostSwizzleSlot1;
	IMG_UINT32	uLiveChansPostSwizzleSlot2;
	IMG_UINT32	uArgIdx;

	/*
		Get the channels used from each of the second two sources before the
		swizzle is applied.
	*/
	GetLiveChansInSourceSlot(psState, psInst, 1 /* uSlotIdx */, &uLiveChans, &uLiveChansPostSwizzleSlot1);
	GetLiveChansInSourceSlot(psState, psInst, 2 /* uSlotIdx */, &uLiveChansInSlot2, &uLiveChansPostSwizzleSlot2);
	ASSERT(uLiveChans == uLiveChansInSlot2);

	/*
		Check and fix the swizzle on first source.
	*/
	CheckAndFixSourceSwizzle(psState, psCopyMap, psInst, 0 /* uArgIdx */);

	/*
		The second two sources must have the same swizzle.
	*/
	uSwizzle = psInst->u.psVec->auSwizzle[1];
	if (!CompareSwizzles(uSwizzle, psInst->u.psVec->auSwizzle[2], uLiveChans))
	{
		bInvalidSwizzle = IMG_TRUE;
	}

	if (!bInvalidSwizzle)
	{
		/*
			Check the swizzle is supported by the instruction.
		*/
		if (!IsSwizzleSupported(psState, psInst, psInst->eOpcode, 1 /* uArgIdx */, uSwizzle, uLiveChans, &uSwizzle))
		{
			bInvalidSwizzle = IMG_TRUE;
		}
	}

	if (!bInvalidSwizzle)
	{
		/*
			Update the swizzle if there was an equivalent which is
			supported.
		*/
		for (uArgIdx = 1; uArgIdx < 3; uArgIdx++)
		{
			psInst->u.psVec->auSwizzle[uArgIdx] = uSwizzle;
		}
	}
	else
	{
		IMG_UINT32	uReplacementSwizzle;
		IMG_BOOL	abIsReplicate[2];
		IMG_UINT32	auRepSwizzle[2];
		IMG_UINT32	auDecompositionSwizzle[2];
		IMG_UINT32	auSwizzle[2];
		IMG_BOOL	abIsImmediate[2];

		/*
			If the swizzle is invalid then insert extra instructions to
			swizzle some sources.
		*/

		for (uArgIdx = 0; uArgIdx < 2; uArgIdx++)
		{
			auSwizzle[uArgIdx] = psInst->u.psVec->auSwizzle[1 + uArgIdx];
			abIsImmediate[uArgIdx] = IsImmediateVectorSource(psState, psInst, 1 + uArgIdx);
		}

		if (abIsImmediate[0] && abIsImmediate[1])
		{
			if (FixVMOVCTwoImmediateSources(psState, psInst, uLiveChans))
			{
				return;
			}
		}
		else if (abIsImmediate[0])
		{
			if (FixVMOVCOneImmediateSource(psState, psInst, 1 /* uImmediateSource */, uLiveChans))
			{
				return;
			}
		}
		else if (abIsImmediate[1])
		{
			if (FixVMOVCOneImmediateSource(psState, psInst, 2 /* uImmediateSource */, uLiveChans))
			{
				return;
			}
		}

		/*
			Check for each source swizzle if it can be decomposed into another swizzle and the
			swizzle on the other source.
		*/
		for (uArgIdx = 0; uArgIdx < 2; uArgIdx++)
		{
			auDecompositionSwizzle[uArgIdx] = USC_UNDEF;
			if (IsSwizzleSupported(psState, 
								   psInst, 
								   psInst->eOpcode, 
								   1 + uArgIdx, 
								   auSwizzle[1 - uArgIdx], 
								   uLiveChans, 
								   &auSwizzle[1 - uArgIdx]))
			{
				auDecompositionSwizzle[uArgIdx] = 
					IsDecomposableSwizzle(psState, auSwizzle[1 - uArgIdx], auSwizzle[uArgIdx], uLiveChans);
			}
		}

		/*
			If one source has an inverse swizzle then we only need one extra swizzling instruction

				MOVC	DEST, SRC0, SRC1.SWIZ_A, SRC2.SWIZ_B
			->
				MOV		TEMP, SRC1.SWIZ_C
				MOVC	DEST, SRC0, SRC1.SWIZ_B, SRC2.SWIZ_B

				where
					COMPOSE(SWIZ_C, SWIZ_B) = SWIZ_A
			*/
		if (auDecompositionSwizzle[0] != USC_UNDEF || auDecompositionSwizzle[1] != USC_UNDEF)
		{
			IMG_UINT32	uMoveSrc;

			if (auDecompositionSwizzle[0] != USC_UNDEF && auDecompositionSwizzle[1] != USC_UNDEF)
			{
				/*
					If both sources have inverse swizzles then choose the one which minimizes the
					range of channels read by the MOVC instruction.
				*/
				if (g_auMaxBitCount[uLiveChansPostSwizzleSlot2] <= g_auMaxBitCount[uLiveChansPostSwizzleSlot1])
				{
					uMoveSrc = 0;
				}
				else
				{
					uMoveSrc = 1;
				}
			}
			else if (auDecompositionSwizzle[0] != USC_UNDEF)
			{
				uMoveSrc = 0;
			}
			else
			{
				ASSERT(auDecompositionSwizzle[1] != USC_UNDEF);
				uMoveSrc = 1;
			}

			ConvertSwizzleToVMUL(psState, 
								 psCopyMap, 
								 psInst, 
								 1 + uMoveSrc, 
								 auDecompositionSwizzle[uMoveSrc],
								 auSwizzle[1 - uMoveSrc],
								 IMG_FALSE /* bCopySourceModifier */);
			psInst->u.psVec->auSwizzle[1 + (1 - uMoveSrc)] = auSwizzle[1 - uMoveSrc];
		}
		else
		{
			/* Check if the swizzles on both sources are broadcasting one input channel to all output channels. */
			for (uArgIdx = 0; uArgIdx < 2; uArgIdx++)
			{
				abIsReplicate[uArgIdx] = 
					IsSwizzleInSet(g_auReplicateSwizzles,
								   sizeof(g_auReplicateSwizzles) / sizeof(g_auReplicateSwizzles[0]),
								   psInst->u.psVec->auSwizzle[1 + uArgIdx],
								   uLiveChans,
								   &auRepSwizzle[uArgIdx]);
			}

			/*
				If possible use a replicate swizzle to avoid adding extra vec4 intermediate registers (which
				have to be assigned to internal registers.)
			*/
			if (abIsReplicate[0] && abIsReplicate[1])
			{
				uReplacementSwizzle = USEASM_SWIZZLE(X, X, X, X);
				for (uArgIdx = 0; uArgIdx < 2; uArgIdx++)
				{
					psInst->u.psVec->auSwizzle[1 + uArgIdx] = auRepSwizzle[uArgIdx];
				}
			}
			else
			{
				uReplacementSwizzle = USEASM_SWIZZLE(X, Y, Z, W);
			}
			for (uArgIdx = 1; uArgIdx < 3; uArgIdx++)
			{
				ConvertSwizzleToVMUL(psState, 
									 psCopyMap, 
									 psInst, 
									 uArgIdx, 
									 psInst->u.psVec->auSwizzle[uArgIdx],
									 uReplacementSwizzle,
									 IMG_FALSE /* bCopySourceModifier */);
			}
		}
	}
}

IMG_VOID static FixVectorSwizzlesBP(PINTERMEDIATE_STATE	psState,
									PCODEBLOCK			psBlock,
									IMG_PVOID			pvCopyMap)
/*****************************************************************************
 FUNCTION	: FixVectorSwizzlesBP
    
 PURPOSE	: Fix any vector instructions in a basic block which use unsupported
			  source swizzles.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to fix instructions for.
			  pvCopyMap			- Details of instructions inserted to apply
								swizzles.

  RETURNS	: None.
*****************************************************************************/
{
	PINST				psInst;
	PVECTOR_COPY_MAP	psCopyMap = (PVECTOR_COPY_MAP)pvCopyMap;

	psCopyMap->psLocalCopies = NULL;

	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		/*
			Skip instructions without vector sources.
		*/
		if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) == 0)
		{
			continue;
		}

		if ((psInst->eOpcode == IVMOVC) || (psInst->eOpcode == IVMOVCU8_FLT))
		{
			FixVMOVCSwizzles(psState, psCopyMap, psInst);
		}
		else if (psInst->eOpcode == IVTEST)
		{
			FixVTESTSwizzles(psState, psCopyMap, psInst);
		}
		else
		{
			FixVectorInstSwizzles(psState, psCopyMap, psInst);	
		}
	}

	/*
		Delete information about instructions inserted to copy/swizzle source arguments.
	*/
	if (psCopyMap->psLocalCopies != NULL)
	{
		UscTreeDeleteAll(psState, psCopyMap->psLocalCopies, DeleteCopyList, (IMG_PVOID)psState);
		psCopyMap->psLocalCopies = NULL;
	}
}

IMG_INTERNAL
IMG_VOID FixVectorSwizzles(PINTERMEDIATE_STATE	psState)
/*****************************************************************************
 FUNCTION	: FixVectorSwizzles
    
 PURPOSE	: Fix any vector instructions in the program which use unsupported
			  source swizzles.

 PARAMETERS	: psState			- Compiler state.

  RETURNS	: None.
*****************************************************************************/
{
	VECTOR_COPY_MAP		sCopyMap;

	sCopyMap.psLocalCopies = sCopyMap.psUniformCopies = NULL;

	DoOnAllBasicBlocks(psState, ANY_ORDER, FixVectorSwizzlesBP, IMG_FALSE /* CALLs */, &sCopyMap);

	if (sCopyMap.psUniformCopies != NULL)
	{
		UscTreeDeleteAll(psState, sCopyMap.psUniformCopies, DeleteCopyList, (IMG_PVOID)psState);
		sCopyMap.psUniformCopies = NULL;
	}

	TESTONLY_PRINT_INTERMEDIATE("Fix Vector Source Swizzle", "vec_fixswiz", IMG_FALSE);
}

IMG_INTERNAL
IMG_UINT32 GetIRegOnlySourceMask(IOPCODE eOpcode)
/*****************************************************************************
 FUNCTION	: GetIRegOnlySourceMask
    
 PURPOSE	: Get a bit mask of the sources to a vector instruction which
			  require an internal register regardless of the number of components
			  referenced.

 PARAMETERS	: eOpcode			- Vector instruction type.

  RETURNS	: The mask.
*****************************************************************************/
{
	switch (eOpcode)
	{
		case IVDUAL:
		{
			/*
				For the GPI source slots to a dual-issue instruction force an internal register
				to be assigned.
			*/
			return VDUAL_ALL_GPI_SRC_SLOTS;
		}
		case IVMAD4:
		{
			/* Source 1 and source 2. */
			return (1 << 1) | (1 << 2);
		}
		case IVADD3:
		case IVMUL3:
		{
			/* Source 1. */
			return (1 << 1);
		}
		case IVDP2:
		case IVDP_GPI:
		case IVDP3_GPI:
		case IVDP_CP:
		{
			/* Source 1. */
			return (1 << 1);
		}
		default:
		{
			return 0;
		}
	}
}

IMG_INTERNAL
IMG_UINT32 GetWideSourceMask(IOPCODE eOpcode)
/*****************************************************************************
 FUNCTION	: GetWideSourceMask
    
 PURPOSE	: Get a bit mask of the sources to a vector instruction which
			  support accessing an F32 vector from the unified store.

 PARAMETERS	: eOpcode			- Type of the vector instruction.

  RETURNS	: The mask.
*****************************************************************************/
{
	switch (eOpcode)
	{
		/*
			Only source 0 is wide.
		*/
		case IVMUL:
		case IVMUL3:
		case IVADD:
		case IVADD3:
		case IVMAD4:
		case IVFRC:
		case IVMIN:
		case IVMAX:
		case IVDP:
		case IVDP2:
		case IVDP3:
		case IVDP_GPI:
		case IVDP3_GPI:
		case IVDP_CP:
		case IVDSX_EXP:
		case IVDSY_EXP:
		case IVPCKU8FLT:
		case IVPCKS8FLT:
		case IVPCKC10FLT:
		case IVPCKFLTFLT:
		case IUNPCKVEC:
		case IVPCKU8U8:
		case IVPCKS16FLT:
		case IVPCKU16FLT:
		case IVTEST:
		case IVTESTMASK:
		case IVTESTBYTEMASK:
		{
			return 1 << 0;
		}

		case IVMOV:
		{
			/*
				The intermediate VMOV might be mapped to either the hardware VMOV or VPCKF32F32
				instructions. The latter supports a wide, unified store source.
			*/
			return 1 << 0;
		}

		/*
			No wide sources.
		*/
		case IVRSQ:
		case IVRCP:
		case IVLOG:
		case IVEXP:
		case IVMOVC:
		case IVMOVCU8_FLT:
		case IVMAD:
		case IVDSX:
		case IVDSY:
		{
			return 0;
		}

		/*
			Only the unified store source is wide.
		*/
		case IVDUAL:
		{
			return 1 << VDUAL_SRCSLOT_UNIFIEDSTORE;
		}

		default:
		{
			return 0;
		}
	}
}

static
IMG_BOOL MapZWToXYInSwizzle(PINTERMEDIATE_STATE	psState,
							PINST				psInst,
							IMG_UINT32			uArgIdx,
							IMG_UINT32			uOldSwizzle,
							IMG_UINT32			uPreSwizzleChansLiveInArg, 
							IMG_PUINT32			puNewSwizzle)
/*****************************************************************************
 FUNCTION	: MapZWToXYInSwizzle
    
 PURPOSE	: Maps ZW swizzle to XY for temporary vector sorces.

 PARAMETERS	: psState					- Compiler state.
			  psInst					- Instruction to map swizzle for.
			  uArgIdx					- Instruction argument to map swizzle 
										  for.
			  uPreSwizzleChansLiveInArg	- Channels live in the argument.
			  bCheckOnly				- If true check only otherwise update 
										  the new swizzle.

 RETURNS	: IMG_TRUE if ZW swizzle can be mapped to XY, IMG_FALSE otherwise.
*****************************************************************************/
{
	IMG_UINT32	uChan;
	IMG_UINT32	uRemappedSwizzle;
	IMG_BOOL	bRet;

	uRemappedSwizzle = 0;
	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		IMG_UINT32	uSel;

		if (uPreSwizzleChansLiveInArg & (1 << uChan))
		{
			uSel = GetChan(uOldSwizzle, uChan);

			ASSERT(uSel != USEASM_SWIZZLE_SEL_X && uSel != USEASM_SWIZZLE_SEL_Y);

			switch (uSel)
			{
				case USEASM_SWIZZLE_SEL_Z: uSel = USEASM_SWIZZLE_SEL_X; break; 
				case USEASM_SWIZZLE_SEL_W: uSel = USEASM_SWIZZLE_SEL_Y; break;
				default: break;
			}

			uRemappedSwizzle |= (uSel << (USEASM_SWIZZLE_FIELD_SIZE * uChan));
		}
	}

	bRet = IsSwizzleSupported(psState, 
							  psInst, 
							  psInst->eOpcode, 
							  uArgIdx, 
							  uRemappedSwizzle, 
							  uPreSwizzleChansLiveInArg, 
							  puNewSwizzle);

	return bRet;
}

IMG_INTERNAL
IMG_BOOL CanUpdateSwizzleToAccessSourceTopHalf(PINTERMEDIATE_STATE	psState,
											   PINST				psInst,
											   IMG_UINT32			uSrcSlotIdx,
											   IMG_PUINT32			puNewSwizzle)
/*****************************************************************************
 FUNCTION     : CanUpdateSwizzleToAccessSourceTopHalf
    
 PURPOSE      : Checks if it's possible to update the swizzle on an instruction
				to replace selections of Z by X and W by Y.

 PARAMETERS   : psState		  - Compiler state.
			    psInst		  - Instruction to check.
				uSrcSlotIdx	  - Index of the instruction source to check.
				puNewSwizzle  - Returns the replacement swizzle.

 RETURNS      : TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uPreSwizzleUsedChans;

	/*
		For VMOVC the second two sources must have the same swizzle.
	*/
	if (((psInst->eOpcode == IVMOVC) || (psInst->eOpcode == IVMOVCU8_FLT)) && (uSrcSlotIdx == 1 || uSrcSlotIdx == 2))
	{
		return IMG_FALSE;
	}
	
	/*
		Get the mask of channels referenced from the source before the swizzle is applied.
	*/
	GetLiveChansInSourceSlot(psState, psInst, uSrcSlotIdx, &uPreSwizzleUsedChans, NULL /* puChansUsedPostSwizzle */);

	/*	
		Check if the updated swizzle is supported by the instruction.
	*/
	return MapZWToXYInSwizzle(psState, 
							  psInst, 
							  uSrcSlotIdx, 
							  psInst->u.psVec->auSwizzle[uSrcSlotIdx],
							  uPreSwizzleUsedChans,
							  puNewSwizzle);
}

IMG_INTERNAL
IMG_VOID BaseSplitVectorInst(PINTERMEDIATE_STATE	psState,
							 PINST					psVecInst,
							 IMG_UINT32				uFirstDestMask,
							 PINST*					ppsFirstInst,
							 IMG_UINT32				uSecondDestMask,
							 PINST*					ppsSecondInst)
/*****************************************************************************
 FUNCTION     : BaseSplitVectorInst
    
 PURPOSE      : Split an instruction into two instructions each of which writes
			    only a subset of the channels written by the original instruction.

 PARAMETERS   : psState		  - Compiler state.
			    psVecInst	  - Instruction to split.
				uFirstDestMask
							  - Channels to write in the first split instruction.
			    ppsFirstInst  - Returns the first split instruction.
				uSecondDestMask
							  - Channels to write in the second split instruction.
			    ppsSecondInst - Returns the second split instruction.

 RETURNS      : Nothing..
*****************************************************************************/
{
	IMG_UINT32	uHalfIdx;
	PARG		psVecDest;
	PINST		psFirstInst = NULL;
	PINST		psSecondInst = NULL;
	PINST		psEarlierInst;
	PINST		psLaterInst;

	ASSERT(psVecInst->uDestCount == 1);
	psVecDest = &psVecInst->asDest[0];

	for (uHalfIdx = 0; uHalfIdx < 2; uHalfIdx++)
	{
		PINST		psHalfInst;
		IMG_UINT32	uSrcIdx;
		IMG_UINT32	uHalfDestMask;
		PINST*		ppsHalfInst;
		IMG_BOOL	bFirst;
		IMG_BOOL	bLast;

		if (uHalfIdx == 0)
		{
			uHalfDestMask = uFirstDestMask;
			ppsHalfInst = &psFirstInst;
		}
		else
		{
			uHalfDestMask = uSecondDestMask;
			ppsHalfInst = &psSecondInst;
		}
		if (uHalfDestMask == 0)
		{
			if (ppsHalfInst != NULL)
			{
				*ppsHalfInst = NULL;
			}
			continue;
		}

		/* Is this the first instruction in the split pair. */
		bFirst = IMG_FALSE;
		if (uHalfIdx == 0 || uFirstDestMask == 0)
		{
			bFirst = IMG_TRUE;
		}

		/* Is this the last instruction in the split pair. */
		bLast = IMG_FALSE;
		if (uHalfIdx == 1 || uSecondDestMask == 0)
		{
			bLast = IMG_TRUE;
		}

		psHalfInst = AllocateInst(psState, psVecInst);
		*ppsHalfInst = psHalfInst;

		/*
			Copy the old instruction's opcode.
		*/
		SetOpcode(psState, psHalfInst, psVecInst->eOpcode);

		/*
			Copy the old instruction's destination. To avoid multiple defines of the destination expand 

				VECINST		DEST[=OLDDEST], ...
			->
				HALFINST	TEMP.XY[=OLDDEST], ...
				HALFINST	DEST.ZW[=TEMP], ...
		*/
		if (psVecDest->uType != USEASM_REGTYPE_TEMP)
		{
			CopyPartiallyWrittenDest(psState, psHalfInst, 0 /* uCopyToDestIdx */, psVecInst, 0 /* uCopyFromDestIdx */);
			SetDestFromArg(psState, psHalfInst, 0 /* uMoveToDestIdx */, &psVecInst->asDest[0]);
		}
		else
		{
			if (bLast && !bFirst)
			{
				SetPartiallyWrittenDest(psState, psHalfInst, 0, &psFirstInst->asDest[0]);
			}
			else
			{
				CopyPartiallyWrittenDest(psState, psHalfInst, 0 /* uCopyToDestIdx */, psVecInst, 0 /* uCopyFromDestIdx */);
			}
			if (bFirst && !bLast)
			{
				SetDest(psState, psHalfInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, GetNextRegister(psState), psVecDest->eFmt);
			}
			else
			{
				MoveDest(psState, psHalfInst, 0 /* uMoveToDestIdx */, psVecInst, 0 /* uMoveFromDestIdx */);
			}
		}
		psHalfInst->auDestMask[0] = uHalfDestMask;

		/*
			Copy the old instruction's predicate.
		*/
		CopyPredicate(psState, psHalfInst, psVecInst);

		/*
			Copy the parameters for a vector instruction.
		*/
		*psHalfInst->u.psVec = *psVecInst->u.psVec;

		/*
			Copy the source arguments.
		*/
		for (uSrcIdx = 0; uSrcIdx < psVecInst->uArgumentCount; uSrcIdx++)
		{
			SetSrcFromArg(psState, psHalfInst, uSrcIdx, &psVecInst->asArg[uSrcIdx]);
		}
	}

	psLaterInst = (psSecondInst != NULL) ? psSecondInst : psFirstInst;
	psEarlierInst = (psFirstInst != NULL) ? psFirstInst : psSecondInst;

	psLaterInst->auLiveChansInDest[0] = psVecInst->auLiveChansInDest[0];
	if (psEarlierInst != NULL && psEarlierInst != psLaterInst)
	{
		psEarlierInst->auLiveChansInDest[0] = GetPreservedChansInPartialDest(psState, psLaterInst, 0 /* uDestIdx */);
	}

	if (ppsFirstInst != NULL)
	{
		*ppsFirstInst = psFirstInst;
	}
	if (ppsSecondInst != NULL)
	{
		*ppsSecondInst = psSecondInst;
	}
}

static
PINST CopyVectorDSXDSYInst(PINTERMEDIATE_STATE	psState,
						   PCODEBLOCK			psBlock,
						   PINST				psOrigInst,
						   IMG_UINT32			uNewMask,
						   IMG_UINT32			uHalf,
						   IMG_UINT32			uZWToXYSwizzle)
/*****************************************************************************
 FUNCTION	: CopyVectorDSXDSYInst
    
 PURPOSE	: Create a copy of a DSX/DSY instruction with a modified swizzle
			  and writing a subset of the channels in the destination.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Basic block to insert the new instruction
								into.
			  psOrigInst		- Instruction to copy.
			  uNewMask			- New destination mask.

 RETURNS	: A pointer to the created instruction.
*****************************************************************************/
{
	PINST		psNewInst;

	psNewInst = CopyInst(psState, psOrigInst);

	/*
		Replace the destination mask.
	*/
	ASSERT(psNewInst->uDestCount == 1);
	psNewInst->auDestMask[0] = uNewMask;

	/*
		Update the arguments to select the correct 64-bit half of the
		source vector.
	*/
	if (psNewInst->asArg[0].uType == USC_REGTYPE_UNUSEDSOURCE)
	{
		IMG_UINT32	uChan;

		if (uHalf == 1)
		{
			/*
				Shift the registers for the Z and W channels to the X and Y
				channels.
			*/
			for (uChan = 0; uChan < 2; uChan++)
			{
				psNewInst->asArg[1 + uChan] = psNewInst->asArg[1 + 2 + uChan];
			}
		}

		/*
			Mark the registers for the Z and W channels as unreferenced.
		*/
		for (uChan = 0; uChan < 2; uChan++)
		{
			InitInstArg(&psNewInst->asArg[1 + 2 + uChan]);
			psNewInst->asArg[1 + 2 + uChan].uType = USC_REGTYPE_UNUSEDSOURCE;
		}
	}
	else
	{
		if (uHalf == 1)
		{
			SetBit(psNewInst->u.psVec->auSelectUpperHalf, 0 /* Source 0 */, 1);
		}
	}

	if (uHalf == 1)
	{
		psNewInst->u.psVec->auSwizzle[0] = uZWToXYSwizzle;
	}

	/*
		Insert the new instruction before the original instruction.
	*/
	InsertInstBefore(psState, psBlock, psNewInst, psOrigInst);

	return psNewInst;
}

static
IMG_VOID FixDSXDSYWideSources(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: FixDSXDSYWideSources
    
 PURPOSE	: Fix a DSX or DSY instruction which might be referencing more
			  data from the unified store than is supported by the hardware.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to fix.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uHalf;
	IMG_UINT32	uSwizzle;
	IMG_UINT32	uNewInstCount;
	IMG_UINT32	uInstIdx;
	IMG_UINT32	uMaskTemp;
	PINST		apsNewInsts[VECTOR_LENGTH];
	IMG_UINT32	uConstantChanMask;
	IMG_UINT32	uChan;

	ASSERT(psInst->eOpcode == IVDSX || psInst->eOpcode == IVDSY);

	if (psInst->u.psVec->aeSrcFmt[0] != UF_REGFORMAT_F32)
	{
		return;
	}

	/*
		The hardware only supports an F16 destination with an F32 source where the source is an internal
		register. But DSX/DSY doesn't support an internal register source so insert a explict conversion
		on the destination.
	*/
	if (psInst->asDest[0].eFmt != UF_REGFORMAT_F32)
	{
		PINST		psConvertInst;
		IMG_UINT32	uConvertTemp = GetNextRegister(psState);

		psConvertInst = AllocateInst(psState, psInst);
		SetOpcode(psState, psConvertInst, IVMOV);

		MoveDest(psState, psConvertInst, 0 /* uMoveToDestIdx */, psInst, 0 /* uMoveFromDestIdx */);
		SetSrc(psState, psConvertInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uConvertTemp, UF_REGFORMAT_F32);
		psConvertInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F32;
		psConvertInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
		InsertInstAfter(psState, psInst->psBlock, psConvertInst, psInst);

		SetDest(psState, psInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uConvertTemp, UF_REGFORMAT_F32);
	}
	
	uSwizzle = psInst->u.psVec->auSwizzle[0];

	/*
		The DSX/DSY instructions can only reference 64-bits of data
		from the unified store so split the original instruction into
		two new instructions: each processing half of the 128-bit
		source vector.
	*/
	uNewInstCount = 0;
	for (uHalf = 0; uHalf < 2; uHalf++)
	{
		USEASM_INTSRCSEL	eFirstChanInHalf, eSecondChanInHalf;
		IMG_UINT32			uFirstChanMask, uSecondChanMask;
		IMG_UINT32			uBothChanMask;
		IMG_UINT32			uHalfSwizzle;

		if (uHalf == 0)
		{
			eFirstChanInHalf = USEASM_SWIZZLE_SEL_X;
			eSecondChanInHalf = USEASM_SWIZZLE_SEL_Y;
		}
		else
		{
			eFirstChanInHalf = USEASM_SWIZZLE_SEL_Z;
			eSecondChanInHalf = USEASM_SWIZZLE_SEL_W;
		}

		/*
			Get the mask of channels in the destination calculated from each of the channels in 
			this half of the vec4 source argument.
		*/
		uFirstChanMask = uSecondChanMask = 0;
		for (uChan = 0; uChan < 4; uChan++)
		{
			if ((psInst->auDestMask[0] & (1 << uChan)) != 0)
			{
				USEASM_SWIZZLE_SEL	eSel;

				eSel = GetChan(uSwizzle, uChan);
				if (eSel == eFirstChanInHalf)
				{
					uFirstChanMask |= (1 << uChan);
				}
				if (eSel == eSecondChanInHalf)
				{
					uSecondChanMask |= (1 << uChan);
				}
			}
		}
		uBothChanMask = uFirstChanMask | uSecondChanMask;

		/*
			Skip if no channels in the destination are calculated using channels from
			this half of the source.
		*/
		if (uBothChanMask == 0)
		{
			continue;
		}

		if (uHalf == 0)
		{
			uHalfSwizzle = uSwizzle;
		}
		else
		{
			/*
				Create a new swizzle replacing Z by X and W by Y.
			*/
			uHalfSwizzle = 0;
			for (uChan = 0; uChan < 4; uChan++)
			{
				if ((uBothChanMask & (1 << uChan)) != 0)
				{
					IMG_UINT32	uSel;

					uSel = (uSwizzle >> (USEASM_SWIZZLE_FIELD_SIZE * uChan)) & USEASM_SWIZZLE_VALUE_MASK;
					if (uSel == USEASM_SWIZZLE_SEL_Z)
					{
						uHalfSwizzle |= (USEASM_SWIZZLE_SEL_X << (USEASM_SWIZZLE_FIELD_SIZE * uChan));
					}
					else
					{
						ASSERT(uSel == USEASM_SWIZZLE_SEL_W);
						uHalfSwizzle |= (USEASM_SWIZZLE_SEL_Y << (USEASM_SWIZZLE_FIELD_SIZE * uChan));
					}
				}
			}
		}

		/*
			Check if the new swizzle is supported by a DSX/DSY instruction.
		*/
		if (IsSwizzleSupported(psState, 
							   psInst,
							   psInst->eOpcode, 
							   0 /* uSwizzleSlotIdx */, 
							   uHalfSwizzle, 
							   uBothChanMask, 
							   &uHalfSwizzle))
		{
			/*
				Create a copy of the instruction referencing only data from this half of the
				source vector and writing only the channels calculated from this half.
			*/
			apsNewInsts[uNewInstCount++] = CopyVectorDSXDSYInst(psState, 
																psInst->psBlock, 
																psInst, 
																uBothChanMask,
																uHalf,
																uHalfSwizzle);
		}
		else
		{
			/*
				Create two copies of the instruction for each channel in this half of the
				source vector.
			*/
			if (uFirstChanMask != 0)
			{
				apsNewInsts[uNewInstCount++] = CopyVectorDSXDSYInst(psState, 
																	psInst->psBlock, 
																	psInst, 
																	uFirstChanMask,
																	uHalf,
																	USEASM_SWIZZLE(X, X, X, X));
			}
			if (uSecondChanMask != 0)
			{
				apsNewInsts[uNewInstCount++] = CopyVectorDSXDSYInst(psState, 
																	psInst->psBlock, 
																	psInst, 
																	uSecondChanMask,
																	uHalf,
																	USEASM_SWIZZLE(Y, Y, Y, Y));
			}
		}
	}

	/*
		Create a mask of the channels which have a constant value swizzled into
		them.
	*/
	uConstantChanMask = 0;
	for (uChan = 0; uChan < 4; uChan++)
	{
		if ((psInst->auDestMask[0] & (1 << uChan)) != 0)
		{
			USEASM_SWIZZLE_SEL	eSel;

			eSel = GetChan(uSwizzle, uChan);
			if (eSel == USEASM_SWIZZLE_SEL_0 ||
				eSel == USEASM_SWIZZLE_SEL_1 ||
				eSel == USEASM_SWIZZLE_SEL_2 ||
				eSel == USEASM_SWIZZLE_SEL_HALF)
			{
				uConstantChanMask |= (1 << uChan);
			}
		}
	}
	/*
		For the constant channels create a move instruction copying zero
		into them (the rate of change of a constant value is zero).
	*/
	if (uConstantChanMask != 0)
	{
		PINST	psVMOVInst;

		psVMOVInst = AllocateInst(psState, psInst);
		SetOpcode(psState, psVMOVInst, IVMOV);
		CopyPredicate(psState, psVMOVInst, psInst);
		psVMOVInst->auDestMask[0] = uConstantChanMask;
		SetupImmediateSource(psVMOVInst, 0 /* uArgIdx */, 0.0f, UF_REGFORMAT_F32);
		InsertInstBefore(psState, psInst->psBlock, psVMOVInst, psInst);

		apsNewInsts[uNewInstCount++] = psVMOVInst;
	}

	/*
		Set up the destinations for the split instructions.
	*/
	uMaskTemp = USC_UNDEF;
	for (uInstIdx = 0; uInstIdx < uNewInstCount; uInstIdx++)
	{
		PINST	psNewInst = apsNewInsts[uInstIdx];

		if (uInstIdx == 0)
		{
			CopyPartiallyWrittenDest(psState, psNewInst, 0 /* uDestIdx */, psInst, 0 /* uDestIdx */);
		}
		else
		{
			SetPartialDest(psState, psNewInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uMaskTemp, UF_REGFORMAT_F32);
		}
		if (uInstIdx == (uNewInstCount - 1))
		{
			MoveDest(psState, psNewInst, 0 /* uDestIdx */, psInst, 0 /* uDestIdx */);
		}
		else
		{
			uMaskTemp = GetNextRegister(psState);
			SetDest(psState, psNewInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uMaskTemp, UF_REGFORMAT_F32);
		}
	}

	/*
		Remove and free the original instruction.
	*/
	RemoveInst(psState, psInst->psBlock, psInst);
	FreeInst(psState, psInst);
}

IMG_INTERNAL
IMG_VOID FixDSXDSY(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: FixDSXDSY

 PURPOSE	: Fix a DSX or DSY instruction which might be referencing more
			  data from the unified store than is supported by the hardware.

 PARAMETERS	: psState		- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	static const IOPCODE	aeGradOpcodes[] = {IVDSX, IVDSY};
	IMG_UINT32				uIdx;

	for (uIdx = 0; uIdx < (sizeof(aeGradOpcodes) / sizeof(aeGradOpcodes[0])); uIdx++)
	{
		SAFE_LIST_ITERATOR	sIter;

		InstListIteratorInitializeAtEnd(psState, aeGradOpcodes[uIdx], &sIter);
		for (; InstListIteratorContinue(&sIter); InstListIteratorPrev(&sIter))
		{
			FixDSXDSYWideSources(psState, InstListIteratorCurrent(&sIter));
		}
		InstListIteratorFinalise(&sIter);
	}
}

IMG_INTERNAL
IMG_BOOL VectorSources12Commute(PINST psInst)
/*****************************************************************************
 FUNCTION	: VectorSources12Commute
    
 PURPOSE	: Check if the first two vector arguments to an instruction
			  commute.

 PARAMETERS	: psInst			- Instruction to check for.

  RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	switch (psInst->eOpcode)
	{
		case IVMUL:
		case IVMUL3:
		case IVADD:
		case IVADD3:
		case IVMIN:
		case IVMAX:
		case IVDP:
		case IVDP2:
		case IVDP3:
		case IVDP_GPI:
		case IVDP3_GPI:
		case IVMAD:
		case IVMAD4:
		{
			return IMG_TRUE;
		}
		case IVTEST:
		case IVTESTMASK:
		case IVTESTBYTEMASK:
		{
			if (psInst->u.psVec->sTest.eTestOpcode == IVADD)
			{
				return IMG_TRUE;
			}
			else if (psInst->u.psVec->sTest.eTestOpcode == IVMUL)
			{
				return IMG_TRUE;
			}
			else
			{
				return IMG_FALSE;
			}
		}

		case IVFRC:
		{
			return IMG_FALSE;
		}

		default:
		{
			return IMG_FALSE;
		}
	}
}

IMG_INTERNAL
IMG_VOID CombineSourceModifiers(PVEC_SOURCE_MODIFIER	psFirst,
							    PVEC_SOURCE_MODIFIER	psSecond,
								PVEC_SOURCE_MODIFIER	psResult)
/*****************************************************************************
 FUNCTION	: CombineSourceModifiers

 PURPOSE	: Calculate the result of applying two sets of source modifiers
			  in series.

 PARAMETERS	: psFirst		- First set of source modifiers to apply.
			  psSecond		- Second set of source modifiers to apply.
			  psResult		- On output the combined source modifiers.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_BOOL	bAbs, bNegate;

	if (!psSecond->bAbs)
	{
		bAbs = psFirst->bAbs;
		bNegate = psFirst->bNegate;
	}
	else
	{
		bAbs = IMG_TRUE;
		bNegate = IMG_FALSE;
	}
	if (psSecond->bNegate)
	{
		bNegate = !bNegate;
	}

	psResult->bAbs = bAbs;
	psResult->bNegate = bNegate;
}

static
IMG_UINT32 GetAbsSourceModMask(IOPCODE	eOpcode)
/*****************************************************************************
 FUNCTION	: GetAbsSourceModMask
    
 PURPOSE	: Get the mask of vector sources to an instruction which supports an
			  ABSOLUTE source modifier.

 PARAMETERS	: eOpcode			- Instruction type to check for.

  RETURNS	: The mask.
*****************************************************************************/
{
	switch (eOpcode)
	{
		/*
			Source 0 and 1.
		*/
		case IVMUL:
		case IVMUL3:
		case IVADD:
		case IVADD3:
		case IVFRC:
		case IVMIN:
		case IVMAX:
		case IVDP:
		case IVDP2:
		case IVDP3:
		case IVDP_GPI:
		case IVDP3_GPI:
		case IVDP_CP:
		case IVDSX_EXP:
		case IVDSY_EXP:
		{
			return (1 << 0) | (1 << 1);
		}

		/*
			Sources 0, 1 and 2.
		*/
		case IVMAD:
		case IVMAD4:
		{
			return (1 << 0) | (1 << 1) | (1 << 2);
		}

		/*
			Source 0.
		*/
		case IVRSQ:
		case IVLOG:
		case IVEXP:
		case IVRCP:
		case IVDSX:
		case IVDSY:
		{
			return (1 << 0);
		}

		/*
			No sources.
		*/
		case IVMOV:
		default:
		{
			return 0;
		}
	}
}

static
IMG_UINT32 GetNegatableSourceMask(PINST psInst, IOPCODE eOpcode)
/*****************************************************************************
 FUNCTION	: GetNegatableSourceMask
    
 PURPOSE	: Get the mask of vector sources to an instruction which supports an
			  NEGATE source modifier.

 PARAMETERS	: psInst			- Instruction to check for.
			  eOpcode			- Instruction type to check for.

  RETURNS	: The mask.
*****************************************************************************/
{
	switch (eOpcode)
	{
		/*
			Source 0 and 1.
		*/
		case IVMUL:
		case IVADD3:
		{
			return (1 << 0) | (1 << 1);
		}

		/*
			Source 1 only.
		*/
		case IVADD:
		case IVMUL3:
		case IVFRC:
		case IVMIN:
		case IVMAX:
		case IVDP:
		case IVDP2:
		case IVDP3:
		case IVDP_GPI:
		case IVDP3_GPI:
		case IVDP_CP:
		case IVDSX_EXP:
		case IVDSY_EXP:
		{
			return 1 << 0;
		}

		case IVDSX:
		case IVDSY:
		{
			return 0;
		}

		/*
			Sources 0, 1 and 2.
		*/
		case IVMAD:
		{
			return (1 << 0) | (1 << 1) | (1 << 2);
		}

		/*
			Sources 0 and 2.
		*/
		case IVMAD4:
		{
			return (1 << 0) | (1 << 2);
		}

		/*
			Source 0 only.
		*/
		case IVEXP:
		case IVLOG:
		case IVRCP:
		case IVRSQ:
		{
			return (1 << 0);
		}

		/*
			Source 0 has an explicit negate modifier.
			If the ALU opcode is VADD then source 1 can be negated by using VSUB.
		*/
		case IVTEST:
		case IVTESTMASK:
		case IVTESTBYTEMASK:
		{
			IMG_UINT32	uNegatableSourceMask;

			uNegatableSourceMask = (1 << 0);
			if (psInst->u.psVec->sTest.eTestOpcode == IVADD)
			{
				uNegatableSourceMask |= (1 << 1);
			}
			return uNegatableSourceMask;
		}

		/*
			No sources.
		*/
		case IVMOV:
		default:
		{
			return 0;
		}
	}
}

IMG_INTERNAL
IMG_BOOL IsValidVectorSourceModifier(PINST					psInst,
									 IOPCODE				eOpcode,
									 IMG_UINT32				uSlotIdx,
									 PVEC_SOURCE_MODIFIER	psSrcMod)
/*****************************************************************************
 FUNCTION	: IsValidVectorSourceModifier
    
 PURPOSE	: Check if a source modifier is valid on a particular source to an
			  instruction.

 PARAMETERS	: psInst			- Instruction to check.
			  eOpcode			- Instruction type to check.
			  uSlotIdx			- Source to check.
			  psSrcMod			- Source modifier to check.

  RETURNS	: None.
*****************************************************************************/
{
	if (psSrcMod->bNegate && (GetNegatableSourceMask(psInst, eOpcode) & (1 << uSlotIdx)) == 0)
	{
		return IMG_FALSE;
	}
	if (psSrcMod->bAbs && (GetAbsSourceModMask(eOpcode) & (1 << uSlotIdx)) == 0)
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_VOID CopyVectorSourceArguments(PINTERMEDIATE_STATE	psState, 
								   PINST				psDestInst, 
								   IMG_UINT32			uDestArgIdx, 
								   PINST				psSrcInst, 
								   IMG_UINT32			uSrcArgIdx)
/*****************************************************************************
 FUNCTION	: CopyVectorSource
    
 PURPOSE	: Copy a source argument from one vector instruction to another.

 PARAMETERS	: psState			- Compiler state.
			  psDestInst		- Instruction to copy to.
			  uDestArgIdx		- Argument to copy to.
			  psSrcInst			- Instruction to copy from.
			  uSrcArgIdx		- Argument to copy from.

  RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uNumComponents;
	IMG_UINT32	uDestArgBase = uDestArgIdx * SOURCE_ARGUMENTS_PER_VECTOR;
	IMG_UINT32	uSrcArgBase = uSrcArgIdx * SOURCE_ARGUMENTS_PER_VECTOR;
	IMG_UINT32	uCompIdx;

	/*
		Get the number of components in the argument.
	*/
	if (psSrcInst->u.psVec->aeSrcFmt[uSrcArgIdx] == UF_REGFORMAT_F32)
	{
		uNumComponents = 1 + SCALAR_REGISTERS_PER_F32_VECTOR;
	}
	else
	{
		ASSERT(psSrcInst->u.psVec->aeSrcFmt[uSrcArgIdx] == UF_REGFORMAT_F16);
		uNumComponents = 1 + SCALAR_REGISTERS_PER_F16_VECTOR;
	}

	/*
		Copy the source register for the argument.
	*/
	for (uCompIdx = 0; uCompIdx < uNumComponents; uCompIdx++)
	{
		CopySrc(psState, psDestInst, uDestArgBase + uCompIdx, psSrcInst, uSrcArgBase + uCompIdx);
	}
	for (; uCompIdx < SOURCE_ARGUMENTS_PER_VECTOR; uCompIdx++)
	{
		SetSrcUnused(psState, psDestInst, uDestArgBase + uCompIdx);
	}
}

static
IMG_VOID CopyVectorSource(PINTERMEDIATE_STATE	psState, 
						  PINST					psDestInst, 
						  IMG_UINT32			uDestArgIdx, 
						  PINST					psSrcInst, 
						  IMG_UINT32			uSrcArgIdx)
/*****************************************************************************
 FUNCTION	: CopyVectorSource
    
 PURPOSE	: Copy a source argument from one vector instruction to another.

 PARAMETERS	: psState			- Compiler state.
			  psDestInst		- Instruction to copy to.
			  uDestArgIdx		- Argument to copy to.
			  psSrcInst			- Instruction to copy from.
			  uSrcArgIdx		- Argument to copy from.

  RETURNS	: None.
*****************************************************************************/
{
	/*
		Copy source modifiers on the argument.
	*/
	psDestInst->u.psVec->aeSrcFmt[uDestArgIdx] = psSrcInst->u.psVec->aeSrcFmt[uSrcArgIdx];
	psDestInst->u.psVec->asSrcMod[uDestArgIdx] = psSrcInst->u.psVec->asSrcMod[uSrcArgIdx];
	psDestInst->u.psVec->auSwizzle[uDestArgIdx] = psSrcInst->u.psVec->auSwizzle[uSrcArgIdx];

	/*
		Copy the source register for the argument.
	*/
	CopyVectorSourceArguments(psState, psDestInst, uDestArgIdx, psSrcInst, uSrcArgIdx);
}

IMG_INTERNAL
IMG_VOID MoveVectorSource(PINTERMEDIATE_STATE	psState, 
						  PINST					psDestInst, 
						  IMG_UINT32			uDestArgIdx, 
						  PINST					psSrcInst, 
						  IMG_UINT32			uSrcArgIdx,
						  IMG_BOOL				bMoveSourceModifier)
/*****************************************************************************
 FUNCTION	: MoveVectorSource
    
 PURPOSE	: Move a source arguments from one vector instruction to another.

 PARAMETERS	: psState			- Compiler state.
			  psDestInst		- Instruction to move to.
			  uDestArgIdx		- Argument to move to.
			  psSrcInst			- Instruction to move from.
			  uSrcArgIdx		- Argument to move from.
			  bMoveSourceModifier
								- If TRUE also move any source modifier.

  RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uNumComponents;
	IMG_UINT32	uDestArgBase = uDestArgIdx * SOURCE_ARGUMENTS_PER_VECTOR;
	IMG_UINT32	uSrcArgBase = uSrcArgIdx * SOURCE_ARGUMENTS_PER_VECTOR;
	IMG_UINT32	uCompIdx;

	/*
		Copy source modifiers on the argument.
	*/
	psDestInst->u.psVec->aeSrcFmt[uDestArgIdx] = psSrcInst->u.psVec->aeSrcFmt[uSrcArgIdx];
	psDestInst->u.psVec->auSwizzle[uDestArgIdx] = psSrcInst->u.psVec->auSwizzle[uSrcArgIdx];
	if (bMoveSourceModifier)
	{	
		psDestInst->u.psVec->asSrcMod[uDestArgIdx] = psSrcInst->u.psVec->asSrcMod[uSrcArgIdx];
	}

	/*
		Get the number of components in the argument.
	*/
	if (psSrcInst->u.psVec->aeSrcFmt[uSrcArgIdx] == UF_REGFORMAT_F32)
	{
		uNumComponents = 1 + SCALAR_REGISTERS_PER_F32_VECTOR;
	}
	else
	{
		ASSERT(psSrcInst->u.psVec->aeSrcFmt[uSrcArgIdx] == UF_REGFORMAT_F16);
		uNumComponents = 1 + SCALAR_REGISTERS_PER_F16_VECTOR;
	}

	/*
		Copy the source register for the argument.
	*/
	for (uCompIdx = 0; uCompIdx < uNumComponents; uCompIdx++)
	{
		MoveSrc(psState, psDestInst, uDestArgBase + uCompIdx, psSrcInst, uSrcArgBase + uCompIdx);
	}
	for (; uCompIdx < SOURCE_ARGUMENTS_PER_VECTOR; uCompIdx++)
	{
		SetSrcUnused(psState, psDestInst, uDestArgBase + uCompIdx);
	}
}

IMG_INTERNAL
IMG_VOID ExpandCVTFLT2INTInstruction_Vec(PINTERMEDIATE_STATE	psState,
										 PINST					psInst)
/*****************************************************************************
 FUNCTION	: ExpandCVTFLT2INTInstruction

 PURPOSE	: Expands a CVTFLT2INT instruction (which isn't supported by the
			  hardware directly) to a sequence of instructions which are
			  supported.

 PARAMETERS	: psState				- Compiler state
			  psInst				- Instruction to expand.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uClampMinTemp = GetNextRegister(psState);
	IMG_UINT32		uClampMaxPredTemp = GetNextRegister(psState);
	IMG_UINT32		uScaledTemp = GetNextRegister(psState);
	IMG_UINT32		uFracTemp = GetNextRegister(psState);
	IMG_UINT32		uSplitWordTemp = GetNextRegister(psState);
	IMG_UINT32		uSplitWordYTemp = GetNextRegister(psState);
	IMG_UINT32		uPreClampTemp = GetNextRegister(psState);
	IMG_UINT32		uVecSourceTemp = GetNextRegister(psState);
	IMG_BOOL		bSigned = psInst->u.psCvtFlt2Int->bSigned;
	UF_REGFORMAT	eSrcFormat = psInst->asArg[0].eFmt;

	ASSERT(psInst->eOpcode == ICVTFLT2INT);

	/*
		Make a single element vector from the source to the conversion instruction.
	*/
	{
		PINST		psMkvecInst;
		IMG_UINT32	uChan;

		psMkvecInst = AllocateInst(psState, psInst);
		SetOpcode(psState, psMkvecInst, IVMOV);

		SetDest(psState, psMkvecInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uVecSourceTemp, UF_REGFORMAT_F32);

		psMkvecInst->asArg[0].uType = USC_REGTYPE_UNUSEDSOURCE;
		MoveSrc(psState, psMkvecInst, 1 /* uCopyToIdx */, psInst, 0 /* uCopyFromIdx */);
		for (uChan = 1; uChan < VECTOR_LENGTH; uChan++)
		{
			psMkvecInst->asArg[1 + uChan].uType = USC_REGTYPE_UNUSEDSOURCE;
		}
		psMkvecInst->u.psVec->aeSrcFmt[0] = eSrcFormat;
		psMkvecInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);

		InsertInstBefore(psState, psInst->psBlock, psMkvecInst, psInst);
	}

	/*
		Clamp to the minimum representable integer.
	*/
	{
		PINST		psMaxInst;
		IMG_FLOAT	fMinInt;

		/*
			CLAMP_MIN = MAX(SOURCE, 0) / 
			CLAMP_MIN = MAX(SOURCE, -0x80000000)
		*/
		psMaxInst = AllocateInst(psState, psInst);
		SetOpcode(psState, psMaxInst, IVMAX);

		SetDest(psState, psMaxInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uClampMinTemp, UF_REGFORMAT_F32);
		psMaxInst->auDestMask[0] = psMaxInst->auLiveChansInDest[0] = USC_X_CHAN_MASK;

		SetupTempVecSource(psState, psMaxInst, 0 /* uArgIdx */, uVecSourceTemp, eSrcFormat);
		psMaxInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);

		if (!bSigned)
		{
			fMinInt = 0;
		}
		else
		{
			fMinInt = -(IMG_FLOAT)0x80000000;
		}
		SetupImmediateSource(psMaxInst, 1 /* uArgIdx */, fMinInt, eSrcFormat);

		InsertInstBefore(psState, psInst->psBlock, psMaxInst, psInst);
	}

	/*
		CLAMP_MAX_PRED = (CLAMP_MIN - 0xFFFFFFFF/0x7FFFFFFF) >= 0 ? 0xFF : 0x00

		We will use CLAMP_MAX_PRED to choose whether to clamp to the maximum integer after generating the integer
		value - we can't just clamp here because 0xFFFFFFFF/0x7FFFFFFF isn't exactly representable in F32.
	*/
	{
		PINST		psTestMaskInst;
		IMG_FLOAT	fMaxInt;
		
		if (!bSigned)
		{
			fMaxInt = (IMG_FLOAT)0xFFFFFFFF;
		}
		else
		{
			fMaxInt = (IMG_FLOAT)0x7FFFFFFF;
		}

		psTestMaskInst = AllocateInst(psState, psInst);
		SetOpcodeAndDestCount(psState, psTestMaskInst, IVTESTBYTEMASK, 2 /* uNewDestCount */);
		psTestMaskInst->u.psVec->sTest.eTestOpcode = IVADD;
		psTestMaskInst->u.psVec->sTest.eType = TEST_TYPE_GTE_ZERO;
		psTestMaskInst->u.psVec->sTest.eMaskType = USEASM_TEST_MASK_BYTE;

		SetDest(psState, psTestMaskInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uClampMaxPredTemp, UF_REGFORMAT_F32);
		SetDest(psState, psTestMaskInst, 1 /* uDestIdx */, USEASM_REGTYPE_TEMP, GetNextRegister(psState), UF_REGFORMAT_F32);
		psTestMaskInst->auLiveChansInDest[1] = 0;

		SetupTempVecSource(psState, psTestMaskInst, 0 /* uArgIdx */, uClampMinTemp, eSrcFormat);
		psTestMaskInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);

		SetupImmediateSource(psTestMaskInst, 1 /* uArgIdx */, fMaxInt, eSrcFormat);
		psTestMaskInst->u.psVec->asSrcMod[1].bNegate = IMG_TRUE;

		InsertInstBefore(psState, psInst->psBlock, psTestMaskInst, psInst);
	}


	/*
		SCALED = CLAMP_MIN * (1/65536)
	*/
	{
		PINST	psMulInst;

		psMulInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psMulInst, IVMUL);

		SetDest(psState, psMulInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uScaledTemp, UF_REGFORMAT_F32);
		psMulInst->auDestMask[0] = psMulInst->auLiveChansInDest[0] = USC_X_CHAN_MASK;

		SetupTempVecSource(psState, psMulInst, 0 /* uArgIdx */, uClampMinTemp, eSrcFormat);
		psMulInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);

		SetupImmediateSource(psMulInst, 1 /* uArgIdx */, 1.0f/65536.0f, eSrcFormat);

		InsertInstBefore(psState, psInst->psBlock, psMulInst, psInst);
	}

	/*
		FRAC = FRC(SCALED)
	*/
	{
		PINST	psFrcInst;

		psFrcInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psFrcInst, IVFRC);

		SetDest(psState, psFrcInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uFracTemp, UF_REGFORMAT_F32);
		psFrcInst->auDestMask[0] = psFrcInst->auLiveChansInDest[0] = USC_X_CHAN_MASK;

		SetupTempVecSource(psState, psFrcInst, 0 /* uArgIdx */, uScaledTemp, eSrcFormat);
		psFrcInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);

		SetupTempVecSource(psState, psFrcInst, 1 /* uArgIdx */, uScaledTemp, eSrcFormat);
		psFrcInst->u.psVec->auSwizzle[1] = USEASM_SWIZZLE(X, Y, Z, W);

		InsertInstBefore(psState, psInst->psBlock, psFrcInst, psInst);
	}

	/*
		SPLIT_WORD.Y = SCALED - FRAC (=floor(SCALED))
	*/
	{
		PINST	psAddInst;

		psAddInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psAddInst, IVADD);

		SetDest(psState, psAddInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uSplitWordYTemp, UF_REGFORMAT_F32);
		psAddInst->auDestMask[0] = psAddInst->auLiveChansInDest[0] = USC_Y_CHAN_MASK;

		SetupTempVecSource(psState, psAddInst, 0 /* uArgIdx */, uScaledTemp, eSrcFormat);
		psAddInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, X, X, X);

		SetupTempVecSource(psState, psAddInst, 1 /* uArgIdx */, uFracTemp, eSrcFormat);
		psAddInst->u.psVec->auSwizzle[1] = USEASM_SWIZZLE(X, X, X, X);
		psAddInst->u.psVec->asSrcMod[1].bNegate = IMG_TRUE;

		InsertInstBefore(psState, psInst->psBlock, psAddInst, psInst);
	}

	/*
		SPLIT_WORD.X = FRAC * 65536
	*/
	{
		PINST		psMulInst;

		psMulInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psMulInst, IVMUL);

		SetDest(psState, psMulInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uSplitWordTemp, UF_REGFORMAT_F32);
		SetPartialDest(psState, psMulInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uSplitWordYTemp, UF_REGFORMAT_F32);
		psMulInst->auDestMask[0] = USC_X_CHAN_MASK;
		psMulInst->auLiveChansInDest[0] = USC_XY_CHAN_MASK;

		SetupTempVecSource(psState, psMulInst, 0 /* uArgIdx */, uFracTemp, eSrcFormat);
		psMulInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, X, X, X);

		SetupImmediateSource(psMulInst, 1 /* uArgIdx */, 65536.0f, eSrcFormat);

		InsertInstBefore(psState, psInst->psBlock, psMulInst, psInst);
	}

	if (!bSigned)
	{
		PINST	psPackInst;

		/*
			TEMP1 = TO_WORD(SPLIT_WORD.X) | (TO_WORD(SPLIT_WORD.Y) << 16)
		*/
		psPackInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psPackInst, IVPCKU16FLT);

		SetDest(psState, psPackInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uPreClampTemp, UF_REGFORMAT_F32);

		SetupTempVecSource(psState, psPackInst, 0 /* uArgIdx */, uSplitWordTemp, eSrcFormat);
		psPackInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);

		InsertInstBefore(psState, psInst->psBlock, psPackInst, psInst);
	}
	else
	{
		PINST		psPackInst;
		IMG_UINT32	uPreClampXYTemp = GetNextRegister(psState);

		/*
			PRE_CLAMP = TO_WORD(SPLIT_WORD.X) | (TO_SIGNED_WORD(SPLIT_WORD.Y) << 16)

			SPLIT_WORD.X is effectively already in 2-complement format since if
			ARG < 0 then SPLIT_WORD.X = 65536 - (ABS(ARG) % 65536) so we
			can use an unsigned conversion.
		*/

		/* PRE_CLAMP_TEMP.LOWWORD = TO_WORD(SPLIT_WORD_TEMP.X) */
		psPackInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psPackInst, IVPCKU16FLT);

		psPackInst->auDestMask[0] = USC_XY_CHAN_MASK;

		SetDest(psState, psPackInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uPreClampXYTemp, UF_REGFORMAT_F32);

		SetupTempVecSource(psState, psPackInst, 0 /* uArgIdx */, uSplitWordTemp, eSrcFormat);
		psPackInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, X, X, X);

		InsertInstBefore(psState, psInst->psBlock, psPackInst, psInst);

		/* PRE_CLAMP_TEMP.HIGHWORD = TO_SIGNED_WORD(SPLIT_WORD_TEMP.Y) */
		psPackInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psPackInst, IVPCKS16FLT);

		psPackInst->auDestMask[0] = USC_ZW_CHAN_MASK;

		SetDest(psState, psPackInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uPreClampTemp, UF_REGFORMAT_F32);
		SetPartialDest(psState, psPackInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uPreClampXYTemp, UF_REGFORMAT_F32);

		SetupTempVecSource(psState, psPackInst, 0 /* uArgIdx */, uSplitWordTemp, eSrcFormat);
		psPackInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(Y, Y, Y, Y);

		InsertInstBefore(psState, psInst->psBlock, psPackInst, psInst);
	}

	/*
		RESULT = CLAMP_MAX_PRED ? 0xFFFFFFFF : TEMP1
	*/
	{
		PINST	psMovcInst;

		psMovcInst = AllocateInst(psState, psInst);
		SetOpcode(psState, psMovcInst, IVMOVCU8_I32);
		psMovcInst->u.psMovc->eTest = TEST_TYPE_NEQ_ZERO;

		CopyPredicate(psState, psMovcInst, psInst);

		MoveDest(psState, psMovcInst, 0 /* uCopyToIdx */, psInst, 0 /* uCopyFromIdx */);

		SetSrc(psState, psMovcInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uClampMaxPredTemp, UF_REGFORMAT_F32);

		psMovcInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		if (!bSigned)
		{
			psMovcInst->asArg[1].uNumber = 0xFFFFFFFF;
		}
		else
		{
			psMovcInst->asArg[1].uNumber = 0x7FFFFFFF;
		}

		SetSrc(psState, psMovcInst, 2 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uPreClampTemp, UF_REGFORMAT_F32);

		InsertInstBefore(psState, psInst->psBlock, psMovcInst, psInst);
	}

	/*
		Free the expanded instruction.
	*/
	RemoveInst(psState, psInst->psBlock, psInst);
	FreeInst(psState, psInst);
}


IMG_INTERNAL
const IMG_UINT32 g_auVec4SizeInDwords[] = 
{
	SCALAR_REGISTERS_PER_F32_VECTOR,	/* UNIFLEX_CONST_FORMAT_F32 */
	SCALAR_REGISTERS_PER_F16_VECTOR,	/* UNIFLEX_CONST_FORMAT_F16 */
	SCALAR_REGISTERS_PER_C10_VECTOR,	/* UNIFLEX_CONST_FORMAT_C10 */
	SCALAR_REGISTERS_PER_U8_VECTOR,		/* UNIFLEX_CONST_FORMAT_U8 */
	USC_UNDEF,							/* UNIFLEX_CONST_FORMAT_STATIC */
	USC_UNDEF,							/* UNIFLEX_CONST_FORMAT_CONSTS_BUFFER_BASE */
	USC_UNDEF,							/* UNIFLEX_CONST_FORMAT_RTA_IDX */
	USC_UNDEF,							/* UNIFLEX_CONST_FORMAT_UNDEF */
};

static
IMG_BOOL RebaseConstantLoads(PINTERMEDIATE_STATE	psState,
							 PUSEDEF_CHAIN			psUseDef,
							 IMG_UINT32				uOffset,
							 IMG_BOOL				bCheckOnly)
/*****************************************************************************
 FUNCTION	: RebaseConstantLoads

 PURPOSE	: Modify all constant loads using a specified intermediate register as a
			  dynamic index so the loaded constant remains the same if a fixed
			  value is subtracted from the value of the dynamic index.

 PARAMETERS	: psState		- Compiler state.
              psUseDef		- List of uses/defines of the intermediate register.
			  uOffset		- Offset to the dynamic index.
			  bCheckOnly	- If TRUE just check if rebasing is possible.

 RETURNS	: None
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;

	for (psListEntry = psUseDef->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		PINST		psLoadInst;
		IMG_UINT32	uOffsetHereInBytes;
		IMG_UINT32	uStrideInBytes;
		IMG_UINT32	uStaticOffsetMod;

		if (psUse == psUseDef->psDef)
		{
			continue;
		}
		
		/*
			Fail if the intermediate register is used other than as a source.
		*/
		if (psUse->eType != USE_TYPE_SRC)
		{
			return IMG_FALSE;
		}

		/*
			Fail if the intermediate register is used other than as a dynamic
			index by a LOADCONST instruction.
		*/
		psLoadInst = psUse->u.psInst;
		if (psLoadInst->eOpcode != ILOADCONST)
		{
			return IMG_FALSE;
		}
		if (psUse->uLocation != LOADCONST_DYNAMIC_OFFSET_ARGINDEX)
		{
			return IMG_FALSE;
		}

		ASSERT(psLoadInst->u.psLoadConst->bRelativeAddress);

		/*
			Scale up the offset by the units of the dynamic index at this
			instruction.
		*/
		uStrideInBytes = psLoadInst->u.psLoadConst->uRelativeStrideInBytes;
		uOffsetHereInBytes = uOffset * uStrideInBytes;

		if ((psState->uCompilerFlags & UF_CONSTEXPLICTADDRESSMODE) == 0)
		{
			UNIFLEX_CONST_FORMAT	eConstFormat;
			IMG_UINT32				uVec4SizeInDwords;
			IMG_UINT32				uVec4SizeInBytes;
			IMG_UINT32				uOffsetHereInVec4s;
			
			/*	
				Get the format of the constant data loaded by this instruction.
			*/
			eConstFormat = psLoadInst->u.psLoadConst->eFormat;
			ASSERT(eConstFormat < (sizeof(g_auVec4SizeInDwords) / sizeof(g_auVec4SizeInDwords[0])));

			/*
				Get the size of a vec4 of that format in bytes.
			*/
			uVec4SizeInDwords = g_auVec4SizeInDwords[eConstFormat];
			ASSERT(uVec4SizeInDwords != USC_UNDEF);

			uVec4SizeInBytes = uVec4SizeInDwords * LONG_SIZE;

			/*
				The static offset is in terms of channels but for loads into vector sized registers
				it has to be a multiple of the size of a vector. So check the rebased static offset
				will still fit with this restriction.
			*/
			if ((uOffsetHereInBytes % uVec4SizeInBytes) != 0)
			{
				return IMG_FALSE;
			}
			uOffsetHereInVec4s = uOffsetHereInBytes / uVec4SizeInBytes;

			uStaticOffsetMod = uOffsetHereInVec4s * VECTOR_LENGTH;
		}
		else
		{
			uStaticOffsetMod = uOffsetHereInBytes;
		}

		/*
			Modify the static part of the offset into the constant buffer.
		*/
		if (!bCheckOnly)
		{
			PARG	psStaticOffset;

			psStaticOffset = &psLoadInst->asArg[LOADCONST_STATIC_OFFSET_ARGINDEX];
			ASSERT(psStaticOffset->uType == USEASM_REGTYPE_IMMEDIATE);
			psStaticOffset->uNumber += uStaticOffsetMod;
		}
	}
	
	return IMG_TRUE;
}

static
IMG_VOID OptimiseConstLoad(PINTERMEDIATE_STATE psState, 
							 PINST psCVTInst,
							 PINST psADDInst,
							 IMG_UINT32 uSlotIdx,
							 IMG_UINT32 uStaticOffset)
/*****************************************************************************
 FUNCTION	: OptimiseConstLoad

 PURPOSE	: Optimise a constant load with a constant offset whose dynamic offset
				comes from a CVTFLT2ADDR instruction which reads from a add instruction
				which adds a constant. The constant is extracted and added on to the
				static offset of the load. The add is then not required anymore.

 PARAMETERS	: psState		- Compiler state.
              psCVTInst		- CVTFLT2ADDR instruction
			  psADDInst		- VADD instruction
			  uSlotIdx		- The index of the source of the ADD that is the constant. 
			  uStaticOffset	- Static offset that is the constant source of the add

 RETURNS	: None
*****************************************************************************/
{
	PARG			psCVTDest = &psCVTInst->asDest[0];
	PUSEDEF_CHAIN	psUseDef;
	IMG_UINT32		uNonConstSlotIdx;
	IMG_UINT32		uNewSwizzle;
	IMG_UINT32		uOffset = (IMG_UINT32)(*(IMG_PFLOAT)&uStaticOffset);
	IMG_BOOL		bRet;

	psUseDef = UseDefGet(psState, psCVTDest->uType, psCVTDest->uNumber);
	
	/*
		Do a first pass to check that all the uses of the result of the conversion
		instruction are suitable to have the ADD folded into them.
	*/
	if (!RebaseConstantLoads(psState, psUseDef, uOffset, IMG_TRUE /* bCheckOnly */))
	{
		return;
	}

	/*
		Actually modify all the uses of the result.
	*/
	bRet = RebaseConstantLoads(psState, psUseDef, uOffset, IMG_FALSE /* bCheckOnly */);
	ASSERT(bRet);

	/*
		Get the source to the ADD which isn't a constant.
	*/
	uNonConstSlotIdx = 1 - uSlotIdx;

	/*
		Combine the swizzles on the non-constant ADD source and the swizzle on the conversion instruction.
	*/
	uNewSwizzle = CombineSwizzles(psADDInst->u.psVec->auSwizzle[uNonConstSlotIdx], psCVTInst->u.psVec->auSwizzle[0]);

	/*
		Copy the source from the ADD instruction to the conversion instruction.
	*/
	CopyVectorSource(psState, psCVTInst, 0 /* uDestArgIdx */, psADDInst, uNonConstSlotIdx); 
	psCVTInst->u.psVec->auSwizzle[0] = uNewSwizzle;
}

static
IMG_UINT32 GetNonConstantChan(IMG_UINT32 uSwizzle, IMG_UINT32 uChan)
/*****************************************************************************
 FUNCTION	: GetNonConstantChan

 PURPOSE	: Apply a swizzle to a single channel.

 PARAMETERS	: uSwizzle			- Swizzle.
			  uChan				- Channel.

 RETURNS	: The channel in the source register selected by the swizzle or
			  USC_UNDEF if constant data is selected.
*****************************************************************************/
{
	USEASM_SWIZZLE_SEL	eSel;

	eSel = GetChan(uSwizzle, uChan);
	switch (eSel)
	{
		case USEASM_SWIZZLE_SEL_X: return USC_X_CHAN;
		case USEASM_SWIZZLE_SEL_Y: return USC_Y_CHAN;
		case USEASM_SWIZZLE_SEL_Z: return USC_Z_CHAN;
		case USEASM_SWIZZLE_SEL_W: return USC_W_CHAN;
		default:				   return USC_UNDEF;
	}
}

static
IMG_BOOL GetLoadConstStaticResult(PINTERMEDIATE_STATE	psState, 
								  PINST					psLCInst, 
								  IMG_UINT32			uDestChan, 
								  IMG_BOOL				bNative,
								  IMG_PUINT32			puValue)
/*****************************************************************************
 FUNCTION	: GetLoadConstStaticResult

 PURPOSE	: Checks if a channel in the destination of a LOADCONST instruction
			  will have a compile-time known value and if so returns the value.

 PARAMETERS	: psState	- Compiler state.
			  psLCInst	- The LOADCONST instruction.
			  uDestChan	- The channel in the destination.
			  bNative	- If TRUE then the value of the channel is returned in
						same format as the destination. If FALSE then it is
						returned in F32 format.
			  puValue	- The value of the channel in the destination.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PLOADCONST_PARAMS	psParams;
	PARG				psStaticOffset;
	IMG_UINT32			uStaticOffset;
	UF_REGFORMAT		eDestFmt;

	ASSERT(psLCInst->eOpcode == ILOADCONST);
	psParams = psLCInst->u.psLoadConst;

	ASSERT(psLCInst->uDestCount == 1);
	eDestFmt = psLCInst->asDest[0].eFmt;
	ASSERT(eDestFmt == UF_REGFORMAT_F16 || eDestFmt == UF_REGFORMAT_F32);

	if (psParams->bRelativeAddress)
	{
		return IMG_FALSE;
	}

	psStaticOffset = &psLCInst->asArg[LOADCONST_STATIC_OFFSET_ARGINDEX];
	ASSERT(psStaticOffset->uType == USEASM_REGTYPE_IMMEDIATE);
	uStaticOffset = psStaticOffset->uNumber;

	if ((psState->uCompilerFlags & UF_CONSTEXPLICTADDRESSMODE) != 0)
	{
		IMG_UINT32	uOffsetInBytes;
		IMG_UINT32	uStaticValue;
		IMG_UINT32	uChanLengthInBytes;

		if (eDestFmt == UF_REGFORMAT_F32)
		{
			uChanLengthInBytes = LONG_SIZE;
		}
		else
		{
			ASSERT(eDestFmt == UF_REGFORMAT_F16);
			uChanLengthInBytes = WORD_SIZE;
		}
		
		/*
			The LOADCONST instruction's static offset in the units of bytes.
		*/
		uOffsetInBytes = uStaticOffset + uDestChan * uChanLengthInBytes;

		/*
			Get if the byte range in the constant buffer loaded into this channel has a
			compile-time known value.
		*/
		if (!GetStaticConstByteOffset(psState, uOffsetInBytes, uChanLengthInBytes, psParams->uBufferIdx, &uStaticValue))
		{
			return IMG_FALSE;
		}

		if (bNative)
		{
			*puValue = uStaticValue;
		}
		else
		{
			/*
				Convert the compile-time known value to F32 format.
			*/
			if (eDestFmt == UF_REGFORMAT_F32)
			{
				*puValue = uStaticValue;
			}
			else
			{
				IMG_FLOAT	fValue;

				ASSERT(eDestFmt == UF_REGFORMAT_F16);
				fValue = ConvertF16ToF32(uStaticValue);
				*puValue = *(IMG_PUINT32)&fValue;
			}
		}
		return IMG_TRUE;
	}
	else
	{
		IMG_UINT32	uF32Value;
		IMG_UINT32	uConstNum;

		if ((uStaticOffset % VECTOR_LENGTH) != 0)
		{
			return IMG_FALSE;
		}

		uConstNum = uStaticOffset / VECTOR_LENGTH;

		if (!CheckForStaticConst(psState, uConstNum, uDestChan, psParams->uBufferIdx, &uF32Value))
		{
			return IMG_FALSE;
		}

		if (bNative)
		{
			/*
				Convert the compile-time known value to the format of the instruction destination.
			*/
			if (eDestFmt == UF_REGFORMAT_F16)
			{
				*puValue = ConvertF32ToF16(*(IMG_PFLOAT)&uF32Value);
			}
			else
			{
				ASSERT(eDestFmt == UF_REGFORMAT_F32);
				*puValue = uF32Value;
			}
		}
		else
		{
			*puValue = uF32Value;
		}
		return IMG_TRUE;
	}

}

static
IMG_VOID OptimiseConstLoadInst(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: OptimiseConstLoadInst

 PURPOSE	: Optimise the constant loads in a block by integrating the constant
				offset into the load and combining index calculations as well as
				loads.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction which writes the dynamic offset used by
						the constant loads.

 RETURNS	: None
*****************************************************************************/
{
	PARG			psArg = &psInst->asArg[0];
	PINST			psAddInst;
	IMG_UINT32		uConvertSrcChan;
	IMG_UINT		uSrcId;
	PINST			psConstInst = NULL;
	IMG_BOOL		bFoundConstArg = IMG_FALSE;

	if (!NoPredicate(psState, psInst))
	{
		return;
	}

	uConvertSrcChan = GetNonConstantChan(psInst->u.psVec->auSwizzle[0], 0 /* uChan */);
	if (uConvertSrcChan == USC_UNDEF)
	{
		return;
	}

	psAddInst = UseDefGetDefInst(psState, psArg->uType, psArg->uNumber, NULL /* puDestIdx */);
	if (psAddInst == NULL)
	{
		return;
	}
	if (psAddInst->eOpcode != IVADD)
	{
		return;
	}

	/*
		Check the source channel used by the ICVTFLT2ADDR instruction is unconditionally defined
		by this instruction.
	*/
	if (!NoPredicate(psState, psAddInst))
	{
		return;
	}
	if ((psAddInst->auDestMask[0] & (1 << uConvertSrcChan)) == 0)
	{
		return;
	}

	/* Is one of the arguments of this add a constant? */ 
	for (uSrcId = 0; uSrcId < psAddInst->uArgumentCount / SOURCE_ARGUMENTS_PER_VECTOR; uSrcId++)
	{
		PARG	psAddArg = &psAddInst->asArg[uSrcId * SOURCE_ARGUMENTS_PER_VECTOR];

		psConstInst = UseDefGetDefInst(psState, psAddArg->uType, psAddArg->uNumber, NULL /* puDestIdx */);

		/* Is the argument a static constant, known at compile time? */
		if (psConstInst != NULL &&
			psConstInst->eOpcode == ILOADCONST && 
			psConstInst->asArg[LOADCONST_DYNAMIC_OFFSET_ARGINDEX].uType	== USEASM_REGTYPE_IMMEDIATE && 
			psConstInst->asArg[LOADCONST_STATIC_OFFSET_ARGINDEX].uType	== USEASM_REGTYPE_IMMEDIATE)
		{
			bFoundConstArg = IMG_TRUE;
			break;
		}
	}

	/* Get the constant and optimise. */
	if (bFoundConstArg)
	{
		IMG_UINT32			uChan;
		IMG_UINT32			uStaticValue;

		uChan = GetNonConstantChan(psAddInst->u.psVec->auSwizzle[uSrcId], uConvertSrcChan);
		if (uChan == USC_UNDEF)
		{
			return;
		}
		
		/*
			Check if the LOADCONST instruction is loading a compile-time known value into the
			source of the ADD.
		*/
		if (GetLoadConstStaticResult(psState, psConstInst, uChan, IMG_FALSE /* bNative */, &uStaticValue))
		{
			OptimiseConstLoad(psState, psInst, psAddInst, uSrcId, uStaticValue);
		}
	}
}

IMG_INTERNAL
IMG_VOID OptimiseConstLoads(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: OptimiseConstLoads

 PURPOSE	: Optimise the constant loads in a block by integrating the constant
				offset into the load and combining index calculations as well as
				loads.

 PARAMETERS	: psState	- Compiler state.

 RETURNS	: None
*****************************************************************************/
{
	ForAllInstructionsOfType(psState, ICVTFLT2ADDR, OptimiseConstLoadInst);
}

IMG_INTERNAL
IMG_VOID ExpandCVTFLT2ADDRInstruction(PINTERMEDIATE_STATE	psState,
									  PINST					psInst)
/*****************************************************************************
 FUNCTION	: ExpandCVTFLT2ADDRInstruction

 PURPOSE	: Expands a CVTFLT2ADDR instruction to a sequence of instructions 
			  which are supported.

 PARAMETERS	: psState				- Compiler state
			  psInst				- Instruction to expand.

 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		The CVTFLT2ADDR instruction does

			DEST[0..16] = FLOAT_TO_SIGNED_WORD(SRC.X)
			DEST[16..32] = 0
	*/

	/*
		Update the mask of channels used from the conversion result.
	*/
	ASSERT(psInst->eOpcode == ICVTFLT2ADDR);
	psInst->auLiveChansInDest[0] = UseDefGetUsedChanMask(psState, psInst->asDest[0].psRegister->psUseDefChain);

	if ((psInst->auLiveChansInDest[0] & USC_ZW_CHAN_MASK) != 0)
	{
		if (psInst->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F32)
		{
			ModifyOpcode(psState, psInst, IVPCKS16FLT_EXP);

			/*
				Take the source for the X channel from the first source argument and
				the source for the Y channel from the second source argument.
			*/
			psInst->u.psVec->uPackSwizzle = USEASM_SWIZZLE(X, Z, X, X);

			/*
				Set the second source argument to immediate 0.
			*/
			SetupImmediateSource(psInst, 1 /* uArgIdx */, 0.0f, UF_REGFORMAT_F32);
		}
		else
		{
			PINST			psU16PCKInst;
			UF_REGFORMAT	eDestFormat = psInst->asDest[0].eFmt;
			ARG				sLowWordDest;

			ModifyOpcode(psState, psInst, IVPCKS16FLT);

			/*
				Use a seperate instruction to set 0 into the top 16-bits of the converted address value.
			*/
			psU16PCKInst = AllocateInst(psState, psInst);
			SetOpcode(psState, psU16PCKInst, IUNPCKU16U16);

			/*
				Copy the destination for the full instruction from the original instruction.
			*/
			MoveDest(psState, psU16PCKInst, 0 /* uMoveToDestIdx */, psInst, 0 /* uMoveFromDestIdx */);

			/*
				Write just the high word with 0.
			*/
			psU16PCKInst->auDestMask[0] = USC_ZW_CHAN_MASK;
			psU16PCKInst->auLiveChansInDest[0] = psInst->auLiveChansInDest[0];

			/*
				Allocate a new temporary for just the low word.
			*/
			MakeNewTempArg(psState, eDestFormat, &sLowWordDest);
			SetPartiallyWrittenDest(psState, psU16PCKInst, 0 /* uDestIdx */, &sLowWordDest);
			SetDestFromArg(psState, psInst, 0 /* uDestIdx */, &sLowWordDest);
			psInst->auLiveChansInDest[0] = GetPreservedChansInPartialDest(psState, psU16PCKInst, 0 /* uDestIdx */);

			CopyPredicate(psState, psU16PCKInst, psInst);

			psU16PCKInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
			psU16PCKInst->asArg[0].uNumber = 0;
			psU16PCKInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psU16PCKInst->asArg[1].uNumber = 0;

			InsertInstAfter(psState, psInst->psBlock, psU16PCKInst, psInst);
		}
	}
	else
	{
		ModifyOpcode(psState, psInst, IVPCKS16FLT);
	}
}

IMG_INTERNAL
IMG_VOID ExpandVMOVCBITInstruction(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: ExpandVMOVCBITInstruction
    
 PURPOSE	: Expand a VMOVCBIT instruction (which isn't supported directly by
			  the hardware) to a sequence of instructions which are supported.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to expand

  RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32		uChan;
	ARG				asTempDest[VECTOR_LENGTH];
	PINST			psMkVecInst;

	ASSERT(psInst->eOpcode == IVMOVCBIT);
	ASSERT(psInst->asDest[0].eFmt == UF_REGFORMAT_F32);
	
	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		InitInstArg(&asTempDest[uChan]);
		asTempDest[uChan].uType = USC_REGTYPE_UNUSEDSOURCE;
	}

	/*
		Generate a separate scalar conditional move for each channel.
	*/
	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		PINST		psMovcInst;
		IMG_UINT32	uSrc;

		if ((psInst->auDestMask[0] & (1 << uChan)) == 0)
		{
			continue;
		}

		/*
			Allocate a new intermediate register for the result of the
			conditional move on this channel.
		*/
		MakeNewTempArg(psState, UF_REGFORMAT_F32, &asTempDest[uChan]);

		/*
			Create a scalar conditional move:-

				DEST = any_bits_set(SRC0) ? SRC1 : SRC2
		*/
		psMovcInst = AllocateInst(psState, psInst);
		SetOpcode(psState, psMovcInst, IMOVC_I32);
		psMovcInst->u.psMovc->eTest = TEST_TYPE_NEQ_ZERO;
		SetDestFromArg(psState, psMovcInst, 0 /* uDestIdx */, &asTempDest[uChan]);

		/*
			Copy the data for this channel from each vector source to scalar intermediate registers.
		*/
		for (uSrc = 0; uSrc < 3; uSrc++)
		{
			ARG			sSrcChan;
			IMG_UINT32	uSrcChan;
			PINST		psUnpackInst;

			ASSERT(psInst->u.psVec->aeSrcFmt[uSrc] == UF_REGFORMAT_F32);

			MakeNewTempArg(psState, UF_REGFORMAT_F32, &sSrcChan);

			psUnpackInst = AllocateInst(psState, psInst);
			SetOpcode(psState, psUnpackInst, IUNPCKVEC);

			SetDestFromArg(psState, psUnpackInst, 0 /* uDestIdx */, &sSrcChan);

			CopyVectorSource(psState, psUnpackInst, 0 /* uDestArgIdx */, psInst, uSrc);

			uSrcChan = GetChan(psInst->u.psVec->auSwizzle[uSrc], uChan);
			psUnpackInst->u.psVec->auSwizzle[0] = g_auReplicateSwizzles[uSrcChan];

			InsertInstBefore(psState, psInst->psBlock, psUnpackInst, psInst);

			/*
				Set the result of the unpack as a source to the conditional move.
			*/
			SetSrcFromArg(psState, psMovcInst, uSrc, &sSrcChan);
		}
		InsertInstBefore(psState, psInst->psBlock, psMovcInst, psInst);
	}

	/*
		Create a vector from the scalar conditional move results and write it to the
		original instruction's destination.
	*/
	psMkVecInst = AllocateInst(psState, psInst);
	SetOpcode(psState, psMkVecInst, IVMOV);

	CopyPredicate(psState, psMkVecInst, psInst);

	MoveDest(psState, psMkVecInst, 0 /* uMoveToDestIdx */, psInst, 0 /* uMoveFromDestIdx */);
	MovePartiallyWrittenDest(psState, psMkVecInst, 0 /* uMoveToDestIdx */, psInst, 0 /* uMoveFromDestIdx */);
	psMkVecInst->auDestMask[0] = psInst->auDestMask[0];
	psMkVecInst->auLiveChansInDest[0] = psInst->auLiveChansInDest[0];

	SetSrcUnused(psState, psMkVecInst, 0 /* uSrcIdx */);
	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		SetSrcFromArg(psState, psMkVecInst, 1 + uChan, &asTempDest[uChan]);
	}
	psMkVecInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
	psMkVecInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F32;
	InsertInstBefore(psState, psInst->psBlock, psMkVecInst, psInst);

	/*
		Drop the expanded instruction.
	*/
	RemoveInst(psState, psInst->psBlock, psInst);
	FreeInst(psState, psInst);
}

IMG_INTERNAL
IMG_VOID ExpandVTRCInstruction(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: ExpandVTRCInstruction
    
 PURPOSE	: Expand a VTRC instruction (which isn't supported directly by
			  the hardware) to a sequence of instructions which are supported.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to expand

  RETURNS	: None.
*****************************************************************************/
{
	PINST			psFLRInst;
	IMG_UINT32		uNegFlrTempNum = GetNextRegister(psState);
	IMG_UINT32		uFlrTempNum = GetNextRegister(psState);
	UF_REGFORMAT	eSrcFormat = psInst->u.psVec->aeSrcFmt[0];
	PINST			psMOVInst;
	PINST			psMOVCInst;

	/*
		 NEGFLR = 0 - FLOOR(ABS(SRC))
	*/
	psFLRInst = AllocateInst(psState, psInst);
	SetOpcode(psState, psFLRInst, IVFRC);

	SetDest(psState, psFLRInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uNegFlrTempNum, UF_REGFORMAT_F32);

	SetupImmediateSource(psFLRInst, 0 /* uArgIdx */, 0.0f, eSrcFormat);

	CopyVectorSource(psState, psFLRInst, 1 /* uArgIdx */, psInst, 0 /* uArgIdx */);
	psFLRInst->u.psVec->asSrcMod[1].bNegate = IMG_FALSE;
	psFLRInst->u.psVec->asSrcMod[1].bAbs = IMG_TRUE;

	InsertInstBefore(psState, psInst->psBlock, psFLRInst, psInst);

	/*
		FLR = -NEGFLR
	*/
	psMOVInst = AllocateInst(psState, psInst);
	SetOpcode(psState, psMOVInst, IVMOV);

	SetDest(psState, psMOVInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uFlrTempNum, UF_REGFORMAT_F32);

	SetupTempVecSource(psState, psMOVInst, 0 /* uArgIdx */, uNegFlrTempNum, eSrcFormat);
	psMOVInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
	psMOVInst->u.psVec->asSrcMod[0].bNegate = IMG_TRUE;

	InsertInstBefore(psState, psInst->psBlock, psMOVInst, psInst);

	/*
		DEST = SRC >= 0 ? FLR : NEGFLR
	*/
	psMOVCInst = AllocateInst(psState, psInst);
	SetOpcode(psState, psMOVCInst, IVMOVC);
	psMOVCInst->u.psVec->eMOVCTestType = TEST_TYPE_GTE_ZERO;

	MoveDest(psState, psMOVCInst, 0 /* uCopyToIdx */, psInst, 0 /* uCopyFromIdx */);

	CopyPredicate(psState, psMOVCInst, psInst);

	MoveVectorSource(psState, psMOVCInst, 0 /* uArgIdx */, psInst, 0 /* uArgIdx */, IMG_TRUE /* bCopySourceModifier */);

	SetupTempVecSource(psState, psMOVCInst, 1 /* uArgIdx */, uFlrTempNum, eSrcFormat);
	psMOVCInst->u.psVec->auSwizzle[1] = USEASM_SWIZZLE(X, Y, Z, W);

	SetupTempVecSource(psState, psMOVCInst, 2 /* uArgIdx */, uNegFlrTempNum, eSrcFormat);
	psMOVCInst->u.psVec->auSwizzle[2] = USEASM_SWIZZLE(X, Y, Z, W);

	InsertInstBefore(psState, psInst->psBlock, psMOVCInst, psInst);

	/*
		Drop the expanded instruction.
	*/
	RemoveInst(psState, psInst->psBlock, psInst);
	FreeInst(psState, psInst);
}

IMG_INTERNAL
IMG_VOID SwapVectorSources(PINTERMEDIATE_STATE	psState, 
						   PINST				psInst1, 
						   IMG_UINT32			uArg1Idx, 
						   PINST				psInst2,
						   IMG_UINT32			uArg2Idx)
/*****************************************************************************
 FUNCTION	: SwapVectorSources
    
 PURPOSE	: Swap vector sources between two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- First instruction.
			  uArg1Idx			- Index of the vector argument to the first instruction.
			  psInst2			- Second instruction.
			  uArg2Idx			- Index of the vector argument to the second instruction.

  RETURNS	: None.
*****************************************************************************/
{
	VEC_SOURCE_MODIFIER		sTempSourceMod;
	IMG_UINT32				uTempSwizzle;
	UF_REGFORMAT			eTempFmt;
	IMG_UINT32				uArg1Base = uArg1Idx * SOURCE_ARGUMENTS_PER_VECTOR;
	IMG_UINT32				uArg2Base = uArg2Idx * SOURCE_ARGUMENTS_PER_VECTOR;
	IMG_UINT32				uCompIdx;
	IMG_UINT32				uTempSelUpper;

	/*
		Swap the components of the two arguments.
	*/
	for (uCompIdx = 0; uCompIdx < SOURCE_ARGUMENTS_PER_VECTOR; uCompIdx++)
	{
		ExchangeInstSources(psState, psInst1, uArg1Base + uCompIdx, psInst2, uArg2Base + uCompIdx);
	}

	/*
		Swap the source modifiers.
	*/
	sTempSourceMod = psInst1->u.psVec->asSrcMod[uArg1Idx];
	psInst1->u.psVec->asSrcMod[uArg1Idx] = psInst2->u.psVec->asSrcMod[uArg2Idx];
	psInst2->u.psVec->asSrcMod[uArg2Idx] = sTempSourceMod;

	/*
		Swap the source swizzles.
	*/
	uTempSwizzle = psInst1->u.psVec->auSwizzle[uArg1Idx];
	psInst1->u.psVec->auSwizzle[uArg1Idx] = psInst2->u.psVec->auSwizzle[uArg2Idx];
	psInst2->u.psVec->auSwizzle[uArg2Idx] = uTempSwizzle;

	/*	
		Swap the source register formats.
	*/
	eTempFmt = psInst1->u.psVec->aeSrcFmt[uArg1Idx];
	psInst1->u.psVec->aeSrcFmt[uArg1Idx] = psInst2->u.psVec->aeSrcFmt[uArg2Idx];
	psInst2->u.psVec->aeSrcFmt[uArg2Idx] = eTempFmt;

	/*
		Swap the upper/lower source selector.
	*/
	uTempSelUpper = GetBit(psInst1->u.psVec->auSelectUpperHalf, uArg1Idx);
	SetBit(psInst1->u.psVec->auSelectUpperHalf, uArg1Idx, GetBit(psInst1->u.psVec->auSelectUpperHalf, uArg2Idx));
	SetBit(psInst2->u.psVec->auSelectUpperHalf, uArg2Idx, uTempSelUpper);
}

IMG_INTERNAL
IMG_BOOL TrySwapVectorSources(PINTERMEDIATE_STATE	psState, 
							  PINST					psInst, 
							  IMG_UINT32			uArg1Idx, 
							  IMG_UINT32			uArg2Idx,
							  IMG_BOOL				bCheckSwizzles)
/*****************************************************************************
 FUNCTION	: TrySwapVectorSources
    
 PURPOSE	: Swap two vector sources to an instruction if that fits with the
			  hardware swizzle and source modifier support.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to swap for.
			  uArg1Idx			- Index of the first vector argument.
			  uArg2Idx			- Index of the second vector argument.
			  bCheckSwizzles	- If TRUE check that the swizzles on both arguments
								are supported on their new position.

  RETURNS	: TRUE if the sources were swapped.
*****************************************************************************/
{
	IMG_UINT32	uAbsoluteSupportedSourceMask = GetAbsSourceModMask(psInst->eOpcode);
	IMG_UINT32	uNegateSupportedSourceMask = GetNegatableSourceMask(psInst, psInst->eOpcode);
	IMG_UINT32	auNewSwizzle[2];
	IMG_UINT32	uSrcIdx;

	for (uSrcIdx = 0; uSrcIdx < 2; uSrcIdx++)
	{
		IMG_UINT32				uOldArgIdx;
		IMG_UINT32				uNewArgIdx;
		PVEC_SOURCE_MODIFIER	psSrcMod;
		IMG_UINT32				uSwizzle;
		
		if (uSrcIdx == 0)
		{
			uOldArgIdx = uArg1Idx;
			uNewArgIdx = uArg2Idx;
		}
		else
		{
			uOldArgIdx = uArg2Idx;
			uNewArgIdx = uArg1Idx;
		}

		psSrcMod = &psInst->u.psVec->asSrcMod[uOldArgIdx];
		uSwizzle = psInst->u.psVec->auSwizzle[uOldArgIdx];

		if (bCheckSwizzles)
		{
			IMG_UINT32	uChansUsed;

			/*
				Get the mask of channels referenced from this argument before the swizzle is applied.
			*/
			GetLiveChansInSourceSlot(psState, psInst, uOldArgIdx, &uChansUsed, NULL /* puChansUsedPostSwizzle */);

			/*
				Check the swizzle used on this source in its old position is supported on the new position.
			*/
			if (!IsSwizzleSupported(psState, psInst, psInst->eOpcode, uNewArgIdx, uSwizzle, uChansUsed, &auNewSwizzle[uSrcIdx]))
			{
				return IMG_FALSE;
			}
		}
		else
		{
			auNewSwizzle[uSrcIdx] = uSwizzle;
		}

		/*
			If this source has a negate or absolute modifier check the instruction supports a negate modifier on the
			new position.
		*/
		if (psSrcMod->bNegate && (uNegateSupportedSourceMask & (1 << uNewArgIdx)) == 0)
		{
			return IMG_FALSE;
		}
		if (psSrcMod->bAbs && (uAbsoluteSupportedSourceMask & (1 << uNewArgIdx)) == 0)
		{
			return IMG_FALSE;
		}
	}

	/*
		Replace the source swizzles by equivalents.
	*/
	for (uSrcIdx = 0; uSrcIdx < 2; uSrcIdx++)
	{
		if (uSrcIdx == 0)
		{
			psInst->u.psVec->auSwizzle[uArg1Idx] = auNewSwizzle[uSrcIdx];
		}
		else
		{
			psInst->u.psVec->auSwizzle[uArg2Idx] = auNewSwizzle[uSrcIdx];
		}
	}

	/*
		Swap the two sources.
	*/
	SwapVectorSources(psState, psInst, uArg1Idx, psInst, uArg2Idx);

	return IMG_TRUE;
}

static
IMG_VOID FixAbsVectorSourceModifiers(PINTERMEDIATE_STATE psState, PVECTOR_COPY_MAP psCopyMap, PINST psInst)
/*****************************************************************************
 FUNCTION	: FixAbsVectorSourceModifiers
    
 PURPOSE	: Fix an unsupported ABSOLUTE source modifiers on an instruction.

 PARAMETERS	: psState			- Compiler state.
			  psCopyMap			- Information about instructions already
								inserted to apply absolute modifiers to
								source arguments.
			  psInst			- Instruction to fix.

  RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uAbsSupportedSourceMask;
	IMG_UINT32	uArgIdx;

	uAbsSupportedSourceMask = GetAbsSourceModMask(psInst->eOpcode);

	for (uArgIdx = 0; uArgIdx < GetSwizzleSlotCount(psState, psInst); uArgIdx++)
	{
		if (psInst->u.psVec->asSrcMod[uArgIdx].bAbs && (uAbsSupportedSourceMask & (1 << uArgIdx)) == 0)
		{
			ConvertSwizzleToVMUL(psState, 
								 psCopyMap, 
								 psInst, 
								 uArgIdx, 
								 psInst->u.psVec->auSwizzle[uArgIdx],
								 USEASM_SWIZZLE(X, Y, Z, W), 
								 IMG_TRUE /* bCopySourceModifier */);
		}
	}
}

static
IMG_VOID FixVectorSourceModifiersBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvCopyMap)
/*****************************************************************************
 FUNCTION	: FixVectorSourceModifiersBP
    
 PURPOSE	: Fix vector instructions in a block which use instructions with
			  unsupported source modifiers.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to fix.
			  pvCopyMap			- Information about instructions already
								inserted to apply negate and absolute modifiers to
								source arguments.

  RETURNS	: None.
*****************************************************************************/
{
	PINST				psInst;
	PVECTOR_COPY_MAP	psCopyMap = (PVECTOR_COPY_MAP)pvCopyMap;

	psCopyMap->psLocalCopies = NULL;

	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		IMG_UINT32	uNegatedSourceMask;
		IMG_UINT32	uNegateSupportedSourceMask;
		IMG_UINT32	uInvalidSourceMask;
		IMG_UINT32	uArgIdx;

		/*
			Skip instructions without vector sources.
		*/
		if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) == 0)
		{
			continue;
		}

		/*
			Fix any instructions with invalid ABSOLUTE source modifiers.
		*/
		FixAbsVectorSourceModifiers(psState, psCopyMap, psInst);

		/*
			Get the mask of sources which support NEGATE modifiers.
		*/
		uNegateSupportedSourceMask = GetNegatableSourceMask(psInst, psInst->eOpcode);

		/*
			Get the mask of sources which use NEGATE modifiers.
		*/
		uNegatedSourceMask = 0;
		for (uArgIdx = 0; uArgIdx < GetSwizzleSlotCount(psState, psInst); uArgIdx++)
		{
			if (psInst->u.psVec->asSrcMod[uArgIdx].bNegate)
			{
				uNegatedSourceMask |= (1 << uArgIdx);
			}
		}

		if ((uNegatedSourceMask & ~uNegateSupportedSourceMask) == 0)
		{
			continue;
		}

		/*
			Check if we can avoid adding extra instructions by swapping the
			first two sources.
		*/
		if ((uNegateSupportedSourceMask & 3) == 1 &&
			(uNegatedSourceMask & 3) == 2 &&
			VectorSources12Commute(psInst))
		{
			SwapVectorSources(psState, psInst, 0 /* uArg1Idx */, psInst, 1 /* uArg2Idx */);

			uNegatedSourceMask = (uNegatedSourceMask & ~3) | 1;
		}

		/*
			Handle a negate modifier on the first MOVC source by changing the test.

				(-S0 <= 0) ? S1 : S2 -> S0 >= 0 ? S1 : S2 -> S0 < 0 ? S2 : S1
			or
				(-S0 < 0) ? S1 : S2 -> S0 > 0 ? S1 : S2 -> S0 <= 0 ? S2 : S1
		*/
		if (
				psInst->eOpcode == IVMOVC && 
				(uNegatedSourceMask & 1) == 1 &&
				(
					psInst->u.psVec->eMOVCTestType == TEST_TYPE_LT_ZERO ||
					psInst->u.psVec->eMOVCTestType == TEST_TYPE_LTE_ZERO
				)
		   )
		{
			IMG_UINT32	uOldNegatedSourceMask;

			/*
				Clear the negate modifier on source 0.
			*/
			psInst->u.psVec->asSrcMod[0].bNegate = IMG_FALSE;

			/*
				Generate the opposite version of the test.
			*/
			if (psInst->u.psVec->eMOVCTestType == TEST_TYPE_LT_ZERO)
			{
				psInst->u.psVec->eMOVCTestType = TEST_TYPE_LTE_ZERO;
			}
			else
			{
				ASSERT(psInst->u.psVec->eMOVCTestType == TEST_TYPE_LTE_ZERO);
				psInst->u.psVec->eMOVCTestType = TEST_TYPE_LT_ZERO;
			}

			/*
				Swap the second two sources to the MOVC.
			*/
			SwapVectorSources(psState, psInst, 1 /* uArg1Idx */, psInst, 2 /* uArg2Idx */);

			/*
				Update the mask of negated sources.
			*/
			uOldNegatedSourceMask = uNegatedSourceMask;
			uNegatedSourceMask = 0;
			if ((uOldNegatedSourceMask & (1 << 1)) != 0)
			{
				uNegatedSourceMask |= (1 << 2);
			}
			if ((uOldNegatedSourceMask & (1 << 2)) != 0)
			{
				uNegatedSourceMask |= (1 << 1);
			}
		}

		/*
			For any instructions using invalid NEGATE modifiers do the negate
			modifier in a seperate instruction.
		*/
		uInvalidSourceMask = uNegatedSourceMask & ~uNegateSupportedSourceMask;
		for (uArgIdx = 0; uArgIdx < GetSwizzleSlotCount(psState, psInst); uArgIdx++)
		{
			if (uInvalidSourceMask & (1 << uArgIdx))
			{
				ConvertSwizzleToVMUL(psState, 
									 psCopyMap, 
									 psInst, 
									 uArgIdx, 
									 psInst->u.psVec->auSwizzle[uArgIdx],
									 USEASM_SWIZZLE(X, Y, Z, W),
									 IMG_TRUE /* bCopySourceModifier */);
			}
		}
	}

	/*
		Delete information about instructions inserted to apply modifiers to
		source arguments.
	*/
	if (psCopyMap->psLocalCopies != NULL)
	{
		UscTreeDeleteAll(psState, psCopyMap->psLocalCopies, DeleteCopyList, (IMG_PVOID)psState);
		psCopyMap->psLocalCopies = NULL;
	}
}

IMG_INTERNAL
IMG_VOID FixVectorSourceModifiers(PINTERMEDIATE_STATE	psState)
/*****************************************************************************
 FUNCTION	: FixVectorSourceModifiers
    
 PURPOSE	: Fix vector instructions in the program which use instructions with
			  unsupported source modifiers.

 PARAMETERS	: psState			- Compiler state.

  RETURNS	: None.
*****************************************************************************/
{
	VECTOR_COPY_MAP		sCopyMap;

	sCopyMap.psLocalCopies = sCopyMap.psUniformCopies = NULL;

	DoOnAllBasicBlocks(psState, ANY_ORDER, FixVectorSourceModifiersBP, IMG_FALSE /* CALLS */, &sCopyMap);

	if (sCopyMap.psUniformCopies != NULL)
	{
		UscTreeDeleteAll(psState, sCopyMap.psUniformCopies, DeleteCopyList, (IMG_PVOID)psState);
		sCopyMap.psUniformCopies = NULL;
	}

	TESTONLY_PRINT_INTERMEDIATE("Fix Vector Source Modifiers", "vec_smod", IMG_FALSE);
}

typedef struct _LOWER_VECTOR_REGISTERS_CONTEXT
{
	REGISTER_REMAP	sRemap;
	USC_LIST		sOtherHalfMoveList;
} LOWER_VECTOR_REGISTERS_CONTEXT, *PLOWER_VECTOR_REGISTERS_CONTEXT;

IMG_INTERNAL
IMG_UINT32 ChanMaskToByteMask(PINTERMEDIATE_STATE psState, IMG_UINT32 uInMask, IMG_UINT32 uInOffset, UF_REGFORMAT eFormat)
/*****************************************************************************
 FUNCTION	: ChanMaskToByteMask
    
 PURPOSE	: Generate the part corresponding to a scalar register in a mask in terms of bytes from 
			  one in terms of C10, F16 or F32 channels.

 PARAMETERS	: psState		- Compiler state.
			  uInMask		- Mask of channels.
			  uInOffset		- Offset of the register within the overall byte
							mask.
			  eFormat		- Format of the channels.

 RETURNS	: The generated mask.
*****************************************************************************/
{
	switch (eFormat)
	{
		case UF_REGFORMAT_F32:
		{
			if (uInMask & (1 << uInOffset))
			{
				return USC_ALL_CHAN_MASK;
			}
			else
			{
				return 0;
			}
		}
		case UF_REGFORMAT_F16:
		{
			IMG_UINT32	uExpandedMask;

			uExpandedMask = 0;
			if (uInMask & (USC_X_CHAN_MASK << (uInOffset * F16_CHANNELS_PER_SCALAR_REGISTER)))
			{
				uExpandedMask |= USC_XY_CHAN_MASK; 
			}
			if (uInMask & (USC_Y_CHAN_MASK << (uInOffset * F16_CHANNELS_PER_SCALAR_REGISTER)))
			{
				uExpandedMask |= USC_ZW_CHAN_MASK;
			}

			return uExpandedMask;
		}
		case UF_REGFORMAT_C10:
		{
			if (uInOffset == 0)
			{
				return uInMask;
			}
			else if (uInOffset == 1)
			{
				if (uInMask & USC_ALPHA_CHAN_MASK)
				{
					return USC_RED_CHAN_MASK;
				}
				else
				{
					return 0;
				}
			}
			else
			{
				return 0;
			}
		}
		default: imgabort();
	}
}

IMG_INTERNAL
IMG_UINT32 GetVPCKFLTFLT_EXPSourceOffset(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uDestChan)
/*****************************************************************************
 FUNCTION	: GetVPCKFLTFLT_EXPSourceOffset

 PURPOSE	: Get the index of the source argument which is copied to
			  a specified destination.

 PARAMETERS	: psState			- Compiler state.
              psInst			- Instruction.
			  uDestChan			- Destination.

 RETURNS	: The index of the source argument.
*****************************************************************************/
{
	USEASM_SWIZZLE_SEL	eDestSel;
	USEASM_SWIZZLE_SEL	eSelFromSrc;
	IMG_UINT32			uVecSrc;
	IMG_UINT32			uArgOffset;

	eDestSel = GetChan(psInst->u.psVec->uPackSwizzle, uDestChan);
	switch (eDestSel)
	{
		case USEASM_SWIZZLE_SEL_X:
		{
			eSelFromSrc = GetChan(psInst->u.psVec->auSwizzle[0], 0 /* uChan */);
			uVecSrc = 0;
			break;
		}
		case USEASM_SWIZZLE_SEL_Y:
		{
			eSelFromSrc = GetChan(psInst->u.psVec->auSwizzle[0], 1 /* uChan */);
			uVecSrc = 0;
			break;
		}
		case USEASM_SWIZZLE_SEL_Z:
		{
			eSelFromSrc = GetChan(psInst->u.psVec->auSwizzle[1], 0 /* uChan */);
			uVecSrc = 1;
			break;
		}
		case USEASM_SWIZZLE_SEL_W:
		{
			eSelFromSrc = GetChan(psInst->u.psVec->auSwizzle[1], 1 /* uChan */);
			uVecSrc = 1;
			break;
		}
		default: imgabort();
	}
	switch (eSelFromSrc)
	{
		case USEASM_SWIZZLE_SEL_X: uArgOffset = 0; break;
		case USEASM_SWIZZLE_SEL_Y: uArgOffset = 1; break;
		default: imgabort();
	}
	return uVecSrc * SOURCE_ARGUMENTS_PER_VECTOR + 1 + uArgOffset;
}

IMG_INTERNAL
IMG_UINT32 GetVectorCopySourceArgument(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uDest)
/*****************************************************************************
 FUNCTION	: GetVectorCopySourceArgument

 PURPOSE	: Get the source argument which is copied into a specified destination by
			  an instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction.
			  uDest			- Destination.

 RETURNS	: Either the index of the source argument or USC_UNDEF if the
			  destination isn't a copy of a source argument.

 NOTES      :
*****************************************************************************/
{
	if (psInst->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F32)
	{
		if (psInst->eOpcode == IVPCKFLTFLT_EXP)
		{
			return GetVPCKFLTFLT_EXPSourceOffset(psState, psInst, uDest);
		}
		else
		{
			USEASM_SWIZZLE_SEL	eSel;

			/*
				Get the channel of the vector source swizzled into this destination.
			*/
			eSel = GetChan(psInst->u.psVec->auSwizzle[0], uDest);
			if (g_asSwizzleSel[eSel].bIsConstant)
			{
				return USC_UNDEF;
			}

			/*
				One scalar source register holds one channel of data.
			*/
			return 1 + g_asSwizzleSel[eSel].uSrcChan;
		}
	}
	else
	{
		IMG_UINT32	uChan;
		IMG_UINT32	uSource;

		ASSERT(psInst->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F16);

		uSource = USC_UNDEF;
		for (uChan = 0; uChan < F16_CHANNELS_PER_SCALAR_REGISTER; uChan++)
		{
			USEASM_SWIZZLE_SEL	eSel;
			IMG_UINT32			uSrcChan;
			IMG_UINT32			uChanSource;
			IMG_UINT32			uChanMask = USC_XY_CHAN_MASK << (uChan * F16_CHANNELS_PER_SCALAR_REGISTER);

			if ((psInst->auDestMask[uDest] & psInst->auLiveChansInDest[uDest] & uChanMask) == 0)
			{
				continue;
			}

			eSel = GetChan(psInst->u.psVec->auSwizzle[0], uDest * SCALAR_REGISTERS_PER_F16_VECTOR + uChan);
			if (g_asSwizzleSel[eSel].bIsConstant)
			{
				return USC_UNDEF;
			} 
			uSrcChan = g_asSwizzleSel[eSel].uSrcChan;

			/*
				Check the instruction isn't from one half of the source into a different half of the destination.
			*/
			if ((uSrcChan % F16_CHANNELS_PER_SCALAR_REGISTER) != (uChan % F16_CHANNELS_PER_SCALAR_REGISTER))
			{
				return USC_UNDEF;
			}

			/*
				Convert from an F16 source channel to a 32-bit source argument.
			*/
			uChanSource = uSrcChan / F16_CHANNELS_PER_SCALAR_REGISTER;
			if (uSource == USC_UNDEF)
			{
				uSource = uChanSource;
			}
			else
			{
				/*
					Check both channels which are swizzled into this destination are from the same
					source argument.
				*/
				if (uChanSource != uSource)
				{
					return USC_UNDEF;
				}
			}
		}

		return 1 + uSource;
	}
}

static
IMG_VOID ReplaceVectorUnpack(PINTERMEDIATE_STATE	psState,
							 PINST					psInst)
/*****************************************************************************
 FUNCTION	: ReplaceVectorUnpack
    
 PURPOSE	: Replace an instruction which selects a scalar channel from a
			  vector argument by a MOV.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to replace.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uChanSel;

	ASSERT(psInst->uDestCount == 1);

	/*
		Once we have replaced vector registers by sets of scalar registers we
		don't need to do anything special to move data from a vector to a
		scalar. However the hardware doesn't support accessing internal
		registers where the register number isn't a multiple of four so
		for this case use a vector MOV with a swizzle.
	*/
	if (psInst->asArg[1].uType == USEASM_REGTYPE_FPINTERNAL)
	{
		/*
			Convert
				UNPCKVEC	SCALARDEST, VECSRC
			->
				VMOV		SCALARDEST, VECSRC
		*/
		ModifyOpcode(psState, psInst, IVMOV);
	}
	else if (psInst->asArg[1].eFmt != psInst->asDest[0].eFmt)
	{
		/*
			Generate a format conversion instruction.
		*/
		ModifyOpcode(psState, psInst, IVPCKFLTFLT);
	}
	else if (psInst->asDest[0].eFmt == UF_REGFORMAT_F16)
	{
		IMG_UINT32	auChanSel[2];
		IMG_UINT32	uHalf;

		/*
			Get the source channel swizzled into each half of the destination.
		*/
		for (uHalf = 0; uHalf < 2; uHalf++)
		{
			if ((psInst->auLiveChansInDest[0] & (USC_XY_CHAN_MASK << (uHalf * 2))) == 0)
			{
				auChanSel[uHalf] = USC_UNDEF;
			}
			else
			{
				auChanSel[uHalf] = GetChan(psInst->u.psVec->auSwizzle[0], USC_X_CHAN + uHalf);
			}
		}

		if (
				(auChanSel[0] == USC_UNDEF || (auChanSel[0] % 2) == 0) &&
				(auChanSel[1] == USC_UNDEF || (auChanSel[1] % 2) == 1) &&
				(auChanSel[0] == USC_UNDEF || auChanSel[1] == USC_UNDEF || (auChanSel[0] + 1) == auChanSel[1])
			)
		{
			IMG_UINT32	uSourceHalf;

			if (auChanSel[0] != USC_UNDEF)
			{
				uSourceHalf = auChanSel[0] / 2;
			}
			else
			{
				uSourceHalf = auChanSel[1] / 2;
			}
			
			/* Convert to VMOV if the source is not aligned right. */
			if ((psInst->asArg[1 + uSourceHalf].uType == USEASM_REGTYPE_FPCONSTANT) &&
				((psInst->asArg[1 + uSourceHalf].uNumber % (1 << QWORD_SIZE_LOG2)) != 0))
			{
				SetOpcode(psState, psInst, IVMOV);
				MoveSrc(psState, psInst, 1 /* uMoveToSrcIdx */, psInst, 1 + uSourceHalf);
				psInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, X, X, X);
				psInst->u.psVec->aeSrcFmt[0] = psInst->asArg[1 + uSourceHalf].eFmt;
				return;
			}

			MoveSrc(psState, psInst, 0 /* uMoveToSrcIdx */, psInst, 1 + uSourceHalf /* uMoveFromSrcIdx */);
			SetOpcode(psState, psInst, IMOV);
		}
		else
		{
			ModifyOpcode(psState, psInst, IVPCKFLTFLT);
		}
	}
	else
	{
		/*
			Convert

				UNPCKVEC	SCALARDEST, {VECSRC0, VECSRC1, VECSRC2, VECSRC3}.SWIZ
			->
				MOV			SCALARDEST, VECSRC[0-3]

				or

				VMOV		SCALARDEST, {VECSRC0, VECSRC1, VECSRC2, VECSRC3}.SWIZ
		*/
		IMG_UINT32 uArgUsed;

		uChanSel = GetChan(psInst->u.psVec->auSwizzle[0], USC_X_CHAN);
		
		if (psInst->asDest[0].eFmt == UF_REGFORMAT_F32)
		{
			uArgUsed = 1 + uChanSel;
		}
		else
		{
			ASSERT((uChanSel % 2) == 0);
			uArgUsed = 1 + uChanSel / 2;
		}

		/* Convert to VMOV if the source is not aligned right. */
		if ((psInst->asArg[uArgUsed].uType == USEASM_REGTYPE_FPCONSTANT) &&
			((psInst->asArg[uArgUsed].uNumber % (1 << QWORD_SIZE_LOG2)) != 0))
		{
			SetOpcode(psState, psInst, IVMOV);
			MoveSrc(psState, psInst, 1 /* uMoveToSrcIdx */, psInst, uArgUsed);
			if (uArgUsed != 1)
			{
				SetSrcUnused(psState, psInst, uArgUsed);
			}
			psInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, X, X, X);
			psInst->u.psVec->aeSrcFmt[0] = psInst->asArg[uArgUsed].eFmt;
			return;
		}

		MoveSrc(psState, psInst, 0 /* uMoveToSrcIdx */, psInst, uArgUsed);
		SetOpcode(psState, psInst, IMOV);
	}
}

static
IMG_VOID ExpandC10Mask(IMG_UINT32 auMask[])
/*****************************************************************************
 FUNCTION	: ExpandC10Mask
    
 PURPOSE	: Replace a C10 destination mask by two seperate masks.

 PARAMETERS	: auMask		- On input the vector mask.
						      On output the masks for scalar registers.

 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		The ALPHA channel of the C10 vector is split across two 32-bit registers so set both
		the ALPHA bit in the write mask for the first destination and the RED bit in
		the write mask for the second destination to represent that both destinations
		are modified when ALPHA is written.
	*/
	auMask[1] = 0;
	if (auMask[0] & USC_ALPHA_CHAN_MASK)
	{
		auMask[1] = USC_RED_CHAN_MASK;
	}
}

static
IMG_VOID ExpandC10PartialDest(PINTERMEDIATE_STATE				psState,
							  PLOWER_VECTOR_REGISTERS_CONTEXT	psContext,
							  PINST								psInst)
/*****************************************************************************
 FUNCTION	: ExpandC10Destination
    
 PURPOSE	: Replace a C10 partially written destination by two separate scalar
			  registers.

 PARAMETERS	: psState		- Compiler state.
			  psContext		- Stage context.
			  psInst		- Instruction to replace.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG	psOldDest;

	ASSERT(psInst->uDestCount >= 1);
	psOldDest = psInst->apsOldDest[0];
	if (psInst->uDestCount >= 2)
	{
		ASSERT(psInst->apsOldDest[1] == NULL);
	}

	/*
		Add an extra destination to represent the upper 32-bits of the 64-bit register containing
		the C10 vector.
	*/
	if (psOldDest != NULL)
	{
		if (psInst->uDestCount == 2)
		{
			if (psOldDest->uType == USEASM_REGTYPE_IMMEDIATE)
			{
				SetPartiallyWrittenDest(psState, psInst, 1 /* uDestIdx */, psOldDest);
			}
			else
			{
				ASSERT(psOldDest->eFmt == UF_REGFORMAT_C10);
				ASSERT(psOldDest->uType == USEASM_REGTYPE_TEMP);
			
				SetPartialDest(psState, 
							   psInst, 
							   1 /* uDestIdx */, 
							   USEASM_REGTYPE_TEMP, 
							   GetRemapLocation(psState, &psContext->sRemap, psOldDest->uNumber), 
							   UF_REGFORMAT_C10);
			}
		}
	}
}

static
IMG_VOID ExpandC10Destination(PINTERMEDIATE_STATE				psState,
							  PLOWER_VECTOR_REGISTERS_CONTEXT	psContext,
							  PINST								psInst)
/*****************************************************************************
 FUNCTION	: ExpandC10Destination
    
 PURPOSE	: Replace a C10 destination by two separate scalar
			  registers.

 PARAMETERS	: psState		- Compiler state.
			  psContext		- Stage context.
			  psInst		- Instruction to replace.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG	psDest;

	ASSERT(psInst->uDestCount == 1);
	psDest = &psInst->asDest[0];

	/*
		Add an extra destination to represent the upper 32-bits of the 64-bit register containing
		the C10 vector.
	*/
	if (psDest->eFmt == UF_REGFORMAT_C10)
	{
		SetDestCount(psState, psInst, 2 /* uNewDestCount */);

		ExpandC10Mask(psInst->auDestMask);
		ExpandC10Mask(psInst->auLiveChansInDest);
		
		/*
			C10 instructions set the remaining 24-bits of the 64-bit register to undefined data so
			set the remaining bits in the write mask for the second destination to represent that
			the rest of the channels are clobbered.
		*/
		psInst->auDestMask[1] |= USC_GBA_CHAN_MASK;

		if (psInst->asDest[0].uType == USC_REGTYPE_UNUSEDDEST)
		{
			SetDestUnused(psState, psInst, 1 /* uDestIdx */);
		}
		else
		{
			ASSERT(psInst->asDest[0].uType == USEASM_REGTYPE_TEMP);
			SetDest(psState, 
					psInst, 
					1 /* uDestIdx */, 
					USEASM_REGTYPE_TEMP, 
					GetRemapLocation(psState, &psContext->sRemap, psInst->asDest[0].uNumber), 
					UF_REGFORMAT_C10);
		}
	}
}

static
IMG_VOID ExpandC10Source(PINTERMEDIATE_STATE				psState,
						 PLOWER_VECTOR_REGISTERS_CONTEXT	psContext,
						 PINST								psInst,
						 IMG_UINT32							uVectorArgIdx,
						 IMG_UINT32							uScalarArg1Idx,
						 IMG_UINT32							uScalarArg2Idx,
						 IMG_UINT32							uLiveChansInArg)
/*****************************************************************************
 FUNCTION	: ExpandC10Source
    
 PURPOSE	: Replace a C10 source by two seperate scalar
			  register.

 PARAMETERS	: psState			- Compiler state.
			  psContext			- Stage context.
			  psInst			- Instruction to replace in.
			  uVectorArgIdx		- Location of the C10 vector source.
			  uScalarArg1Idx	- location of the source for the first 32-bits
								of the C10 vector.
			  uScalarArg2Idx	- Location of the source for the second 32-bits.
			  uLiveChansInArg
								- Live channels in replaced argument.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG			psVectorArg;

	psVectorArg = &psInst->asArg[uVectorArgIdx];

	/*
		If the instruction accesses the alpha channel of a C10 vector then add an extra
		source for the register which now containing the alpha channel.
	*/
	if (psVectorArg->eFmt == UF_REGFORMAT_C10 && (uLiveChansInArg & USC_ALPHA_CHAN_MASK) != 0)
	{
		if (psVectorArg->uType == USEASM_REGTYPE_IMMEDIATE)
		{
			SetSrcFromArg(psState, psInst, uScalarArg2Idx, psVectorArg);
		}
		else
		{
			ASSERT(psVectorArg->uType == USEASM_REGTYPE_TEMP);
			SetSrc(psState,
				   psInst,
				   uScalarArg2Idx,
				   USEASM_REGTYPE_TEMP,
				   GetRemapLocation(psState, &psContext->sRemap, psVectorArg->uNumber),
				   UF_REGFORMAT_C10);
		}
	}
	else
	{
		/*
			Otherwise flag the extra source as unused.
		*/
		SetSrcUnused(psState, psInst, uScalarArg2Idx);
	}
	MoveSrc(psState, psInst, uScalarArg1Idx, psInst, uVectorArgIdx);
}

static
IMG_VOID ExpandTwoSourceC10PCK(PINTERMEDIATE_STATE				psState,
							   PLOWER_VECTOR_REGISTERS_CONTEXT	psContext,
							   PCODEBLOCK						psBlock,
							   PINST							psInst)
/*****************************************************************************
 FUNCTION	: ExpandTwoSourceC10PCK
    
 PURPOSE	: Expand a PCKC10C10 or PCKC10F16 instruction.

 PARAMETERS	: psState		- Compiler state.
			  psContext		- Stage context.
			  psBlock		- Block containing the instruction to expand.
			  psInst		- Instruction to expand.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uChan;
	IMG_UINT32	uIdx;
	IMG_UINT32	auChansFromSrc[2];
	IMG_UINT32	auSrcSwizzle[2];
	IMG_UINT32	auLiveChansInSrc[2];
	IMG_BOOL	bSource1EqualsDest;
	IMG_UINT32	uSrcIdx;
	PINST		psFirstInst = NULL;

	uSrcIdx = 0;
	auChansFromSrc[0] = auChansFromSrc[1] = 0;
	for (uChan = 0; uChan < CHANS_PER_REGISTER; uChan++)
	{
		if ((psInst->auDestMask[0] & (1 << uChan)) != 0)
		{
			if ((psInst->auLiveChansInDest[0] & (1 << uChan)) != 0)
			{
				auChansFromSrc[uSrcIdx] |= (1 << uChan);
			}
			uSrcIdx = (uSrcIdx + 1) % 2;
		}
	}

	/*
		Convert the component selects to swizzle.
	*/
	for (uIdx = 0; uIdx < 2; uIdx++)
	{
		IMG_UINT32	uComponent;

		uComponent = GetPCKComponent(psState, psInst, uIdx);
		if (psInst->eOpcode == IPCKC10C10)
		{
			auSrcSwizzle[uIdx] = g_auReplicateSwizzles[uComponent];
			auLiveChansInSrc[uIdx] = (USC_X_CHAN_MASK << uComponent);
		}
		else
		{
			ASSERT(psInst->eOpcode == IPCKC10F16);

			switch (GetPCKComponent(psState, psInst, 0 /* uArgIdx */))
			{
				case 0: auSrcSwizzle[uIdx] = USEASM_SWIZZLE(X, X, X, X); break;
				case 2: auSrcSwizzle[uIdx] = USEASM_SWIZZLE(Y, Y, Y, Y); break;
				default: imgabort();
			}

			auLiveChansInSrc[uIdx] = (USC_XY_CHAN_MASK << uComponent);
		}
	}

	/*
		If the two sources are equal then generate only one vector pack instruction.
	*/
	bSource1EqualsDest = IMG_FALSE;
	if (EqualArgs(&psInst->asArg[0], &psInst->asArg[1]))
	{
		/*
			Combine the swizzles for both sources.
		*/
		for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
		{
			if ((auChansFromSrc[1] & (1 << uChan)) != 0)
			{
				IMG_UINT32	uChanMask = USEASM_SWIZZLE_VALUE_MASK << (USEASM_SWIZZLE_FIELD_SIZE * uChan);
			
				auSrcSwizzle[0] &= ~uChanMask;
				auSrcSwizzle[0] |= (auSrcSwizzle[1] & uChanMask);
			}
		}

		auChansFromSrc[0] |= auChansFromSrc[1];
		auChansFromSrc[1] = 0;

		auLiveChansInSrc[0] |= auLiveChansInSrc[1];
		auLiveChansInSrc[1] = 0;
	}

	for (uSrcIdx = 0; uSrcIdx < 2; uSrcIdx++)
	{
		if (auChansFromSrc[uSrcIdx] != 0)
		{
			PINST		psVPCKInst;
			IMG_BOOL	bFirst;
			IMG_BOOL	bLast;
			IMG_UINT32	uDestIdx;

			psVPCKInst = AllocateInst(psState, psInst);
			if (psInst->eOpcode == IPCKC10C10)
			{
				SetOpcode(psState, psVPCKInst, IVPCKC10C10);
			}
			else
			{
				ASSERT(psInst->eOpcode == IPCKC10F16);
				SetOpcode(psState, psVPCKInst, IVPCKC10FLT);
			}

			bFirst = IMG_FALSE;
			if (uSrcIdx == 0 || auChansFromSrc[0] == 0)
			{
				bFirst = IMG_TRUE;
			}

			bLast = IMG_FALSE;
			if (uSrcIdx == 1 || auChansFromSrc[1] == 0)
			{
				bLast = IMG_TRUE;
			}

			psVPCKInst->auDestMask[0] = psInst->auDestMask[0] & auChansFromSrc[uSrcIdx];

			/*
				The hardware doesn't allow VPCKC10C10 which writes only 3 channels so fix the
				instruction to write all 4 channels. PCKC10C10 has the same restrictions so we're
				assured this is always possible.
			*/
			if (g_auSetBitCount[psVPCKInst->auDestMask[0]] == 3)
			{
				ASSERT(uSrcIdx == 0);
				ASSERT(auChansFromSrc[1] == 0);
				ASSERT((psInst->auLiveChansInDest[0] & ~psVPCKInst->auDestMask[0]) == 0);

				/*
					The hardware requires the 1st and 3rd, and 2nd and 4th swizzle selectors
					to be the same.
				*/
				for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
				{
					if ((psVPCKInst->auDestMask[0] & (1 << uChan)) == 0)
					{
						IMG_UINT32	uOtherChan;
						IMG_UINT32	uOtherSel;

						uOtherChan = (uChan + 2) % VECTOR_LENGTH;
						uOtherSel = GetChan(auSrcSwizzle[uSrcIdx], uOtherChan);
						auSrcSwizzle[uSrcIdx] = SetChan(auSrcSwizzle[uSrcIdx], uChan, uOtherSel);
						break;
					}
				}

				psVPCKInst->auDestMask[0] = USC_ALL_CHAN_MASK;
			}

			if (bFirst)
			{
				psVPCKInst->auLiveChansInDest[0] = psInst->auLiveChansInDest[0] & ~auChansFromSrc[1];
			}
			else
			{
				psVPCKInst->auLiveChansInDest[0] = psInst->auLiveChansInDest[0];
			}

			if (psInst->asDest[0].uType != USEASM_REGTYPE_TEMP)
			{
				SetDestFromArg(psState, psVPCKInst, 0 /* uDestIdx */, &psInst->asDest[0]);
				CopyPartiallyWrittenDest(psState, psVPCKInst, 0 /* uCopyToDestIdx */, psInst, 0 /* uCopyFromDestIdx */);
				ExpandC10Destination(psState, psContext, psVPCKInst);
			}
			else
			{
				if (bLast)
				{
					MoveDest(psState, psVPCKInst, 0 /* uMoveToDestIdx */, psInst, 0 /* uMoveFromDestIdx */);
					ExpandC10Destination(psState, psContext, psVPCKInst);
				}
				else
				{
					psFirstInst = psVPCKInst;
					SetDestCount(psState, psVPCKInst, 2 /* uNewDestCount */);
					for (uDestIdx = 0; uDestIdx < 2; uDestIdx++)
					{
						SetDest(psState, 
								psVPCKInst, 
								uDestIdx, 
								USEASM_REGTYPE_TEMP, 
								GetNextRegister(psState), 
								UF_REGFORMAT_C10);
					}
					ExpandC10Mask(psVPCKInst->auDestMask);
					ExpandC10Mask(psVPCKInst->auLiveChansInDest);
				}	
				if (bFirst)
				{
					CopyPartiallyWrittenDest(psState, psVPCKInst, 0 /* uCopyToDestIdx */, psInst, 0 /* uCopyFromDestIdx */);
					ExpandC10PartialDest(psState, psContext, psVPCKInst);
				}
				else
				{
					for (uDestIdx = 0; uDestIdx < 2; uDestIdx++)
					{
						SetPartiallyWrittenDest(psState, psVPCKInst, uDestIdx, &psFirstInst->asDest[uDestIdx]); 
					}
				}
			}

			CopyPredicate(psState, psVPCKInst, psInst);

			SetSrcFromArg(psState, psVPCKInst, 0 /* uSrcIdx */, &psInst->asArg[uSrcIdx]);

			psVPCKInst->u.psVec->auSwizzle[0] = auSrcSwizzle[uSrcIdx];

			if (psInst->eOpcode == IPCKC10C10)
			{
				ExpandC10Source(psState,
								psContext, 
								psVPCKInst, 
								0 /* uVectorArgIdx */, 
								0 /* uScalarArg1Idx */,
								1 /* uScalarArg2Idx */,
								auLiveChansInSrc[uSrcIdx]);
			}
			else
			{
				ASSERT(psInst->eOpcode == IPCKC10F16);

				psVPCKInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F16;
			}

			InsertInstBefore(psState, psBlock, psVPCKInst, psInst);
		}
	}

	RemoveInst(psState, psBlock, psInst);
	FreeInst(psState, psInst);
}

static
IMG_VOID GenerateAlphaReplicate(PINTERMEDIATE_STATE	psState,
								PINST				psInsertBeforeInst,
								PARG				psSrc,
								PARG				psDest) 
/*****************************************************************************
 FUNCTION	: GenerateAlphaReplicate
    
 PURPOSE	: Generate an instruction to copy the alpha channel of a source register
			  to all four channels of the destination.

 PARAMETERS	: psState				- Compiler state.
			  psInsertBeforeInst	- Point to insert the generated instructions.
			  psSrc					- Source register.
			  psDest				- Returns the register containing the replicated
									data.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psPCKInst;

	psPCKInst = AllocateInst(psState, psInsertBeforeInst);

	InsertInstBefore(psState, psInsertBeforeInst->psBlock, psPCKInst, psInsertBeforeInst);

	if (psSrc->eFmt == UF_REGFORMAT_C10)
	{
		IMG_UINT32	uSrc;

		MakeNewTempArg(psState, UF_REGFORMAT_C10, psDest);

		SetOpcode(psState, psPCKInst, IPCKC10C10);

		SetDestFromArg(psState, psPCKInst, 0 /* uDestIdx */, psDest);

		for (uSrc = 0; uSrc < 2; uSrc++)
		{
			SetSrcFromArg(psState, psPCKInst, uSrc, psSrc);
			SetPCKComponent(psState, psPCKInst, uSrc, USC_W_CHAN);
		}
	}
	else
	{
		MakeNewTempArg(psState, UF_REGFORMAT_U8, psDest);

		SetOpcode(psState, psPCKInst, IVPCKU8U8);

		SetDestFromArg(psState, psPCKInst, 0 /* uDestIdx */, psDest);

		psPCKInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(W, W, W, W);
		psPCKInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_U8;

		SetSrcFromArg(psState, psPCKInst, 0 /* uSrc */, psSrc);
		SetSrcUnused(psState, psPCKInst, 1 /* uSrc */);
	}
}

static
IMG_VOID GenerateComplement(PINTERMEDIATE_STATE psState, 
							PINST				psInsertBeforeInst, 
							PARG				psComplementSrc, 
							IMG_UINT32			uComplementChanMask,
							PARG				psComplementDest)
/*****************************************************************************
 FUNCTION	: GenerateComplement
    
 PURPOSE	: Generate instruction to calculate the complement (1-x) of a
			  intermediate register containing fixed point data.

 PARAMETERS	: psState				- Compiler state.
			  psInsertBeforeInst	- Point to insert the generated instructions.
			  psComplementSrc		- Source for the complement operation.
			  uComplementChanMask	- Mask of channels to be complemented.
			  psComplementDest		- Returns the intermediate register containing
									the complemented data.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psInst;
	ARG		sComplementDest;

	MakeNewTempArg(psState, psComplementSrc->eFmt, &sComplementDest);

	psInst = AllocateInst(psState, psInsertBeforeInst);

	SetOpcode(psState, psInst, ISOPWM);

	SetDestFromArg(psState, psInst, 0 /* uDestIdx */, &sComplementDest);

	psInst->auDestMask[0] = uComplementChanMask;

	if (uComplementChanMask != USC_ALL_CHAN_MASK)
	{
		SetPartiallyWrittenDest(psState, psInst, 0 /* uDestIdx */, psComplementSrc);
	}

	SetSrc(psState, 
		   psInst, 
		   0 /* uSrcIdx */, 
		   USEASM_REGTYPE_FPCONSTANT, 
		   SGXVEC_USE_SPECIAL_CONSTANT_FFFFFFFF << SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT,
		   UF_REGFORMAT_U8);

	SetSrcFromArg(psState, psInst, 1 /* uSrcIdx */, psComplementSrc);

	psInst->u.psSopWm->uSel1 = USEASM_INTSRCSEL_ZERO;
	psInst->u.psSopWm->bComplementSel1 = IMG_TRUE;

	psInst->u.psSopWm->uSel2 = USEASM_INTSRCSEL_ZERO;
	psInst->u.psSopWm->bComplementSel2 = IMG_TRUE;

	psInst->u.psSopWm->uCop = USEASM_INTSRCSEL_SUB;
	psInst->u.psSopWm->uAop = USEASM_INTSRCSEL_SUB;

	InsertInstBefore(psState, psInsertBeforeInst->psBlock, psInst, psInsertBeforeInst);

	*psComplementDest = sComplementDest;
}

static
IMG_VOID CopySrcWithComplement(PINTERMEDIATE_STATE	psState, 
							   PINST				psInst, 
							   IMG_UINT32			uChansToCopy,
							   IMG_UINT32			uArg,
							   PARG					psSrc, 
							   IMG_BOOL				bComplementRGB, 
							   IMG_BOOL				bComplementAlpha,
							   IMG_BOOL				bAlphaReplicate)
/*****************************************************************************
 FUNCTION	: CopySrcWithComplement
    
 PURPOSE	: Move a source argument from one instruction to another with
			  optional complement modifiers on the RGB channels and the
			  ALPHA channel.

 PARAMETERS	: psState				- Compiler state.
			  psInst				- Instruction to copy the source to.
			  uChansToCopy			- Channels within the source argument to copy.
			  uArg					- Argument to copy the source to.
			  psSrc					- Source argument.
			  bComplementRGB		- Complement modifier for the RGB channels.
			  bComplementAlpha		- Complement modifier for the ALPHA channel.
			  bAlphaReplicate		- Alpha replicate modifier.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ARG	sSrcAlphaReplicate;

	if (bAlphaReplicate && (uChansToCopy & USC_RGB_CHAN_MASK) != 0)
	{
		GenerateAlphaReplicate(psState, psInst, psSrc, &sSrcAlphaReplicate);
		psSrc = &sSrcAlphaReplicate;
	}

	if (bComplementRGB || bComplementAlpha)
	{
		ARG			sSrcComplement;
		IMG_UINT32	uComplementChanMask;

		if (bComplementRGB && bComplementAlpha)
		{
			uComplementChanMask = USC_ALL_CHAN_MASK;
		}
		else if (bComplementRGB)
		{
			uComplementChanMask = USC_RGB_CHAN_MASK;
		}
		else
		{
			/* if (bComplementAlpha) */
			uComplementChanMask = USC_ALPHA_CHAN_MASK;
		}

		GenerateComplement(psState, psInst, psSrc, uComplementChanMask, &sSrcComplement);
		SetSrcFromArg(psState, psInst, uArg, &sSrcComplement);
	}
	else
	{
		SetSrcFromArg(psState, psInst, uArg, psSrc); 
	}
}

static
IMG_VOID CombineRGBAlpha(PINTERMEDIATE_STATE	psState, 
						 PINST					psInsertBeforeInst, 
						 PARG					psRGBSrc, 
						 IMG_BOOL				bAlphaReplicateRGB,
						 PARG					psAlphaSrc,
						 PARG					psDest)
/*****************************************************************************
 FUNCTION	: CombineRGBAlpha
    
 PURPOSE	: Generate instructions to combine the RGB channels from one source 
			  and the ALPHA channel from another source.

 PARAMETERS	: psState				- Compiler state.
			  psInsertBeforeInst	- Point to insert the new instruction.
			  psRGBSrc				- Source for the RGB channels.
			  bAlphaReplicateRGB	- If TRUE copy the RGB channels from the ALPHA
									channel.
			  psAlphaSrc			- Source for the ALPHA channel.
			  psDest				- Returns the intermediate register containing
									the combined channels.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ARG				sTempDest;
	UF_REGFORMAT	eCombineFmt;
	IMG_UINT32		uPart;

	/*
		Set the format of the combined channels to the maximum precision from the
		sources.
	*/
	if (psRGBSrc->eFmt == UF_REGFORMAT_C10 || psAlphaSrc->eFmt == UF_REGFORMAT_C10)
	{
		eCombineFmt = UF_REGFORMAT_C10;
	}
	else
	{
		eCombineFmt = UF_REGFORMAT_U8;
	}

	/*
		Allocate a temporary register for the partial result of the combine.
	*/
	MakeNewTempArg(psState, eCombineFmt, &sTempDest);

	/*
		Allocate a temporary register for the final result.
	*/
	MakeNewTempArg(psState, eCombineFmt, psDest);

	for (uPart = 0; uPart < 2; uPart++)
	{
		PINST	psSOPWMInst;

		psSOPWMInst = AllocateInst(psState, psInsertBeforeInst);
		SetOpcode(psState, psSOPWMInst, ISOPWM);

		InsertInstBefore(psState, psInsertBeforeInst->psBlock, psSOPWMInst, psInsertBeforeInst);

		if (uPart == 0)
		{
			SetDestFromArg(psState, psSOPWMInst, 0 /* uDestIdx */, &sTempDest);

			if (bAlphaReplicateRGB)
			{
				ARG	sRGBAlphaReplicate;

				GenerateAlphaReplicate(psState, psSOPWMInst, psRGBSrc, &sRGBAlphaReplicate);
				SetSrcFromArg(psState, psSOPWMInst, 0 /* uSrcIdx */, &sRGBAlphaReplicate);
			}
			else
			{
				SetSrcFromArg(psState, psSOPWMInst, 0 /* uSrcIdx */, psRGBSrc);
			}
		}
		else
		{
			SetDestFromArg(psState, psSOPWMInst, 0 /* uDestIdx */, psDest);
			SetPartiallyWrittenDest(psState, psSOPWMInst, 0 /* uDestIdx */, &sTempDest);
			SetSrcFromArg(psState, psSOPWMInst, 0 /* uSrcIdx */, psAlphaSrc);
		}

		SetSrc(psState, psSOPWMInst, 1 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, 0 /* uNumber */, UF_REGFORMAT_U8);

		psSOPWMInst->u.psSopWm->uSel1 = USEASM_INTSRCSEL_ZERO;
		psSOPWMInst->u.psSopWm->bComplementSel1 = IMG_TRUE;

		psSOPWMInst->u.psSopWm->uSel1 = USEASM_INTSRCSEL_ZERO;
		psSOPWMInst->u.psSopWm->bComplementSel2 = IMG_FALSE;

		psSOPWMInst->u.psSopWm->uCop = USEASM_INTSRCSEL_ADD;
		psSOPWMInst->u.psSopWm->uAop = USEASM_INTSRCSEL_ADD;
	}
}

typedef struct
{
	IMG_UINT32	uSrc;
	IMG_BOOL	bRGBComplement;
	IMG_BOOL	bAlphaComplement;
	IMG_BOOL	bAlphaReplicate;
} FPMA_SEL, *PFPMA_SEL;

static
IMG_VOID FixFPMA(PINTERMEDIATE_STATE psState, PINST psFPMAInst)
/*****************************************************************************
 FUNCTION	: FixFPMA
    
 PURPOSE	: Expand an FPMA instruction writing to the destination alpha
			  channel.

 PARAMETERS	: psState			- Compiler state.
			  psFPMAInst		- Instruction to fix.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PFPMA_PARAMS		psFpma = psFPMAInst->u.psFpma;
	PINST				psSOP3Inst;
	USEASM_INTSRCSEL	eOp;
	ARG					sSrc0Combined;
	PARG				psRGBSrc0;
	PARG				psAlphaSrc0;
	PARG				psSrc0;
	FPMA_SEL			sCSel1, sCSel2;
	PFPMA_SEL			psSrc0Sel, psSrc1Sel;
	IMG_BOOL			bSrc0AlphaReplicate;
	IMG_UINT32			uLiveChansInDest = psFPMAInst->auLiveChansInDest[0];

	/*
		Unpack the FPMA options for the first multiply source.
	*/
	sCSel1.uSrc = 1;
	sCSel1.bRGBComplement = psFpma->bComplementCSel1;
	sCSel1.bAlphaComplement = psFpma->bComplementASel1;
	if (psFpma->uCSel1 == USEASM_INTSRCSEL_SRC1ALPHA)
	{
		sCSel1.bAlphaReplicate = IMG_TRUE;
	}
	else
	{
		ASSERT(psFpma->uCSel1 == USEASM_INTSRCSEL_SRC1);
		sCSel1.bAlphaReplicate = IMG_FALSE;
	}

	/*
		Unpack the FPMA options for the second multiply source.
	*/
	sCSel2.uSrc = 2;
	sCSel2.bRGBComplement = psFpma->bComplementCSel2;
	sCSel2.bAlphaComplement = psFpma->bComplementASel2;
	if (psFpma->uCSel2 == USEASM_INTSRCSEL_SRC2ALPHA)
	{
		sCSel2.bAlphaReplicate = IMG_TRUE;
	}
	else
	{
		ASSERT(psFpma->uCSel2 == USEASM_INTSRCSEL_SRC2);
		sCSel2.bAlphaReplicate = IMG_FALSE;
	}

	/*
		Swap the order of the FPMA multiply sources to maximise the use we can make of the SOP3
		complement/alpha replicate source modifiers.
	*/
	if (sCSel2.bRGBComplement || sCSel2.bAlphaComplement || sCSel2.bAlphaReplicate)
	{
		psSrc0Sel = &sCSel2;
		psSrc1Sel = &sCSel1;
	}
	else
	{
		psSrc0Sel = &sCSel2;
		psSrc1Sel = &sCSel1;
	}

	psSOP3Inst = AllocateInst(psState, psFPMAInst);
	SetOpcode(psState, psSOP3Inst, ISOP3);

	InsertInstBefore(psState, psFPMAInst->psBlock, psSOP3Inst, psFPMAInst);

	/*
		Move the FPMA destination to the SOP3 instruction.
	*/
	MoveDest(psState, psSOP3Inst, 0 /* uMoveToDestIdx */, psFPMAInst, 0 /* uMoveFromDestIdx */);
	psSOP3Inst->auLiveChansInDest[0] = psFPMAInst->auLiveChansInDest[0];

	CopyPredicate(psState, psSOP3Inst, psFPMAInst);

	CopyPartiallyWrittenDest(psState, psSOP3Inst, 0 /* uMoveToDestIdx */, psFPMAInst, 0 /* uMoveFromDestIdx */);

	if (psFpma->bNegateSource0)
	{
		eOp = USEASM_INTSRCSEL_SUB;
	}
	else
	{
		eOp = USEASM_INTSRCSEL_ADD;
	}

	/*
		DEST = SRC0/SRC0ALPHA * SRC1 +/- SRC2
	*/
	psSOP3Inst->u.psSop3->uCOp = eOp;
	if (psSrc0Sel->bAlphaReplicate)
	{
		psSOP3Inst->u.psSop3->uCSel1 = USEASM_INTSRCSEL_SRC0ALPHA;
	}
	else
	{
		psSOP3Inst->u.psSop3->uCSel1 = USEASM_INTSRCSEL_SRC0;
	}
	psSOP3Inst->u.psSop3->bComplementCSel1 = psSrc0Sel->bRGBComplement;
	psSOP3Inst->u.psSop3->uCSel2 = USEASM_INTSRCSEL_ZERO;
	psSOP3Inst->u.psSop3->bComplementCSel2 = IMG_TRUE;
	psSOP3Inst->u.psSop3->bNegateCResult = IMG_FALSE;

	psSOP3Inst->u.psSop3->uCoissueOp = USEASM_OP_ASOP;
	psSOP3Inst->u.psSop3->uAOp = eOp;
	psSOP3Inst->u.psSop3->uASel1 = USEASM_INTSRCSEL_SRC0ALPHA;
	psSOP3Inst->u.psSop3->bComplementASel1 = psSrc0Sel->bAlphaComplement;
	psSOP3Inst->u.psSop3->uASel2 = USEASM_INTSRCSEL_ZERO;
	psSOP3Inst->u.psSop3->bComplementASel2 = IMG_TRUE;
	psSOP3Inst->u.psSop3->bNegateAResult = IMG_FALSE;

	/*
		Copy FPMA source 1 or source 2 to SOP3 source 0.
	*/
	MoveSrc(psState, psSOP3Inst, 0 /* uMoveToSrcIdx */, psFPMAInst, psSrc0Sel->uSrc /* uMoveFromDestIdx */);

	/*
		The other FPMA multiply source to SOP3 source 1 applying the FPMA complement and alpha replicate modifiers
		in separate instruction.
	*/
	CopySrcWithComplement(psState, 
						  psSOP3Inst, 
						  uLiveChansInDest,
						  1 /* uArg */, 
						  &psFPMAInst->asArg[psSrc1Sel->uSrc], 
						  psSrc1Sel->bRGBComplement, 
						  psSrc1Sel->bAlphaComplement,
						  psSrc1Sel->bAlphaReplicate);

	/*
		Get the source for the FPMA's CSEL0.
	*/
	bSrc0AlphaReplicate = IMG_FALSE;
	if (psFpma->uCSel0 == USEASM_INTSRCSEL_SRC0 || psFpma->uCSel0 == USEASM_INTSRCSEL_SRC0ALPHA)
	{
		psRGBSrc0 = &psFPMAInst->asArg[0];
		if (psFpma->uCSel0 == USEASM_INTSRCSEL_SRC0ALPHA)
		{
			bSrc0AlphaReplicate = IMG_TRUE;
		}
	}
	else
	{
		ASSERT(psFpma->uCSel0 == USEASM_INTSRCSEL_SRC1 || psFpma->uCSel0 == USEASM_INTSRCSEL_SRC1ALPHA);

		psRGBSrc0 = &psFPMAInst->asArg[1];
		if (psFpma->uCSel0 == USEASM_INTSRCSEL_SRC1ALPHA)
		{
			bSrc0AlphaReplicate = IMG_TRUE;
		}
	}

	/*
		Get the source for the FPMA's ASEL0.
	*/
	if (psFpma->uASel0 == USEASM_INTSRCSEL_SRC0 || psFpma->uASel0 == USEASM_INTSRCSEL_SRC0ALPHA)
	{
		psAlphaSrc0 = &psFPMAInst->asArg[0];
	}
	else
	{
		ASSERT(psFpma->uASel0 == USEASM_INTSRCSEL_SRC1 || psFpma->uASel0 == USEASM_INTSRCSEL_SRC1ALPHA);
		psAlphaSrc0 = &psFPMAInst->asArg[1];
	}


	/*
		If the RGB and ALPHA source data are from different registers then generate an instruction to
		combine them.
	*/
	if (psRGBSrc0 == psAlphaSrc0)
	{
		psSrc0 = psRGBSrc0;
	}
	else
	{
		CombineRGBAlpha(psState, psSOP3Inst, psRGBSrc0, bSrc0AlphaReplicate, psAlphaSrc0, &sSrc0Combined);

		psSrc0 = &sSrc0Combined;
		bSrc0AlphaReplicate = IMG_FALSE;
	}

	/*
		Apply any complement modifiers on the FPMA's CSEL0 in seperate instruction.
	*/
	CopySrcWithComplement(psState, 
						  psSOP3Inst, 
						  uLiveChansInDest,
						  2 /* uArg */, 
						  psSrc0, 
						  psFpma->bComplementCSel0, 
						  psFpma->bComplementASel0,
						  bSrc0AlphaReplicate);
}

static
IMG_VOID GenerateSOP3_RGB_Alpha_Combine(PINTERMEDIATE_STATE psState, PINST psSOP2Inst)
/*****************************************************************************
 FUNCTION	: GenerateSOP3_RGB_Alpha_Combine
    
 PURPOSE	: Replace a SOP2 instruction copying the first source into the
			  RGB channels of the destination and the second source into
			  the ALPHA channel by an equivalent SOP3 instruction.

 PARAMETERS	: psState			- Compiler state.
			  psSOP2Inst		- Instruction to replace.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psSOP3Inst;

	psSOP3Inst = AllocateInst(psState, psSOP2Inst);

	SetOpcode(psState, psSOP3Inst, ISOP3);

	InsertInstBefore(psState, psSOP2Inst->psBlock, psSOP3Inst, psSOP2Inst);

	MoveDest(psState, psSOP3Inst, 0 /* uMoveToDestIdx */, psSOP2Inst, 0 /* uMoveFromDestIdx */);

	psSOP3Inst->auLiveChansInDest[0] = psSOP2Inst->auLiveChansInDest[0];

	CopyPartiallyWrittenDest(psState, psSOP3Inst, 0 /* uMoveToDestIdx */, psSOP2Inst, 0 /* uMoveFromDestIdx */);

	CopyPredicate(psState, psSOP3Inst, psSOP2Inst);

	/*
		DESTRGB = (1 - ZERO) * SRC1 + ZERO * SRC2
	*/
	psSOP3Inst->u.psSop3->uCOp = USEASM_INTSRCSEL_ADD;
	psSOP3Inst->u.psSop3->uCSel1 = USEASM_INTSRCSEL_ZERO;
	psSOP3Inst->u.psSop3->bComplementCSel1 = IMG_TRUE;
	psSOP3Inst->u.psSop3->uCSel2 = USEASM_INTSRCSEL_ZERO;
	psSOP3Inst->u.psSop3->bComplementCSel2 = IMG_FALSE;
	psSOP3Inst->u.psSop3->bNegateCResult = IMG_FALSE;

	/*
		DESTA = (1 - SRC0) * ZERO + SRC0 * (1 - ZERO);
	*/
	psSOP3Inst->u.psSop3->uAOp = USEASM_INTSRCSEL_ADD;
	psSOP3Inst->u.psSop3->uCoissueOp = USEASM_OP_ALRP;
	psSOP3Inst->u.psSop3->uASel1 = USEASM_INTSRCSEL_ZERO;
	psSOP3Inst->u.psSop3->bComplementASel1 = IMG_FALSE;
	psSOP3Inst->u.psSop3->uASel2 = USEASM_INTSRCSEL_ZERO;
	psSOP3Inst->u.psSop3->bComplementASel2 = IMG_TRUE;
	psSOP3Inst->u.psSop3->bNegateAResult = IMG_FALSE;

	/*
		SOP2	DEST, RGBSRC, ALPHASRC
			->
		SOP3	DEST, ALPHASRC, RGBSRC, #0
	*/
	MoveSrc(psState, psSOP3Inst, 0 /* uMoveToSrcIdx */, psSOP2Inst, 1 /* uMoveFromSrcIdx */);
	MoveSrc(psState, psSOP3Inst, 1 /* uMoveToSrcIdx */, psSOP2Inst, 0 /* uMoveFromSrcIdx */);
	SetSrc(psState, psSOP3Inst, 2 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, 0 /* uNumber */, UF_REGFORMAT_U8);
}

static
IMG_VOID FixSOP2(PINTERMEDIATE_STATE psState, PINST psSOP2Inst)
/*****************************************************************************
 FUNCTION	: FixSOP2
    
 PURPOSE	: Expand a SOP2 instruction writing to the destination alpha
			  channel.

 PARAMETERS	: psState			- Compiler state.
			  psSOP2Inst		- Instruction to fix.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PSOP2_PARAMS	psSop2 = psSOP2Inst->u.psSop2;
	IMG_BOOL		bGeneratedSrc1Complement;
	ARG				sComplementedSrc1;
	IMG_UINT32		uPart;
	PINST			psFirstInst;
	PINST			psLastInst;

	/*
		We never use these source selectors.
	*/
	ASSERT(psSop2->uCSel1 != USEASM_INTSRCSEL_SRC2SCALE);
	ASSERT(psSop2->uCSel1 != USEASM_INTSRCSEL_ZEROS);
	ASSERT(psSop2->uASel1 != USEASM_INTSRCSEL_SRC2SCALE);
	ASSERT(psSop2->uASel1 != USEASM_INTSRCSEL_ZEROS);

	ASSERT(psSop2->uCSel2 != USEASM_INTSRCSEL_SRC2SCALE);
	ASSERT(psSop2->uCSel2 != USEASM_INTSRCSEL_ZEROS);
	ASSERT(psSop2->uASel2 != USEASM_INTSRCSEL_SRC2SCALE);
	ASSERT(psSop2->uASel2 != USEASM_INTSRCSEL_ZEROS);


	/*
		Check for the special case of a SOP2 instruction copying the first source
		into the RGB channels of the destination and the second source into
		the ALPHA channel of the destination.
	*/
	if (
			(psSOP2Inst->auLiveChansInDest[0] & USC_RGB_CHAN_MASK) != 0 &&

			/*
				DEST.RGB = (1 - 0) * SRC1 + 0 * SRC2
			*/
			psSop2->uCSel1 == USEASM_INTSRCSEL_ZERO &&
			psSop2->bComplementCSel1 &&

			psSop2->uCSel2 == USEASM_INTSRCSEL_ZERO &&
			!psSop2->bComplementCSel2 &&

			!psSop2->bComplementCSrc1 &&

			/*
				DEST.ALPHA = 0 * SRC1 + (1 - 0) * SRC2
			*/
			psSop2->uASel1 == USEASM_INTSRCSEL_ZERO &&
			!psSop2->bComplementASel1 &&

			psSop2->uASel2 == USEASM_INTSRCSEL_ZERO &&
			psSop2->bComplementASel2 &&

			!psSop2->bComplementASrc1
		)
	{
		/*
			Convert the SOP2 instruction to an equivalent SOP3 instruction.
		*/
		GenerateSOP3_RGB_Alpha_Combine(psState, psSOP2Inst);
		return;
	}

	/*
		Check for an instruction whose calculation could also be done by a single SOPWM.
	*/
	if (
			/*	
				Check the colour and alpha calculations match.
			*/
			psSop2->uCSel1 == psSop2->uASel1 &&
			psSop2->bComplementCSel1 == psSop2->bComplementASel1 &&
			psSop2->uCSel2 == psSop2->uASel2 &&
			psSop2->bComplementCSel2 == psSop2->bComplementASel2 &&
			
			/*
				Check the complement modifiers on source 1 match for the colour and
				alpha calculations.
			*/
			psSop2->bComplementCSrc1 == psSop2->bComplementASrc1 &&

			/*
				Check if there is a complement modifier on source 1 that the instruction
				doesn't also reference the uncomplemented source through a selector.
			*/
			(
				!psSop2->bComplementCSrc1 ||
				(
					psSop2->bComplementCSel1 != USEASM_INTSRCSEL_SRC1 &&
					psSop2->bComplementCSel1 != USEASM_INTSRCSEL_SRC1ALPHA &&

					psSop2->bComplementCSel2 != USEASM_INTSRCSEL_SRC1 &&
					psSop2->bComplementCSel2 != USEASM_INTSRCSEL_SRC1ALPHA
				)
			)
	   )
	{
		ARG sComplementedSrc1;
		PINST psSOPWMInst;
		IMG_UINT32 uSrc;

		psSOPWMInst = AllocateInst(psState, psSOP2Inst);
		SetOpcode(psState, psSOPWMInst, ISOPWM);
		InsertInstBefore(psState, psSOP2Inst->psBlock, psSOPWMInst, psSOP2Inst);

		/*
			Copy the SOP2's destination to the SOPWM instruction.
		*/
		psSOPWMInst->auLiveChansInDest[0] = psSOP2Inst->auLiveChansInDest[0];
		psSOPWMInst->auDestMask[0] = psSOP2Inst->auLiveChansInDest[0];
		MoveDest(psState, psSOPWMInst, 0 /* uMoveToDestIdx */, psSOP2Inst, 0 /* uMoveFromDestIdx */);
		
		/*
			Copy the SOP2 predicate to the SOPWM instruction.
		*/
		CopyPredicate(psState, psSOPWMInst, psSOP2Inst);

		/*
			Copy the SOP2 sources to the SOPWM instruction.
		*/
		for (uSrc = 0; uSrc < 2; uSrc++)
		{
			SetSrcFromArg(psState, psSOPWMInst, uSrc, &psSOP2Inst->asArg[uSrc]);
		}

		/*
			Convert the SOP2 calculation to a SOPWM.
		*/
		psSOPWMInst->u.psSopWm->uCop = psSop2->uCOp;
		psSOPWMInst->u.psSopWm->uAop = psSop2->uAOp;
		psSOPWMInst->u.psSopWm->uSel1 = psSop2->uCSel1;
		psSOPWMInst->u.psSopWm->bComplementSel1 = psSop2->bComplementCSel1;
		psSOPWMInst->u.psSopWm->uSel2 = psSop2->uCSel2;
		psSOPWMInst->u.psSopWm->bComplementSel2 = psSop2->bComplementCSel2;

		/*
			Replace source 1 by its complement.
		*/
		if (psSop2->bComplementCSrc1)
		{
			GenerateComplement(psState, psSOPWMInst, &psSOPWMInst->asArg[0], USC_ALL_CHAN_MASK, &sComplementedSrc1);
			SetSrcFromArg(psState, psSOPWMInst, 0 /* uSrcIdx */, &sComplementedSrc1);
		}

		return;
	}

	/*
		Generate a seperate SOPWM instructions for each of the RGB and 
		ALPHA parts of the SOP2 calculation.
	*/
	bGeneratedSrc1Complement = IMG_FALSE;
	psFirstInst = psLastInst = NULL;
	for (uPart = 0; uPart < 2; uPart++)
	{
		PINST		psSOPWMInst;
		IMG_UINT32	uMask;
		IMG_UINT32	uSrc;
		IMG_BOOL	bComplementSrc1;

		if (uPart == 0)
		{
			uMask = psSOP2Inst->auLiveChansInDest[0] & USC_RGB_CHAN_MASK;
		}
		else
		{
			uMask = psSOP2Inst->auLiveChansInDest[0] & USC_ALPHA_CHAN_MASK;
		}
		if (uMask == 0)
		{
			continue;
		}

		psSOPWMInst = AllocateInst(psState, psSOP2Inst);

		if (psFirstInst == NULL)
		{
			psFirstInst = psSOPWMInst;
		}
		psLastInst = psSOPWMInst;

		SetOpcode(psState, psSOPWMInst, ISOPWM);

		InsertInstBefore(psState, psSOP2Inst->psBlock, psSOPWMInst, psSOP2Inst);

		psSOPWMInst->auDestMask[0] = uMask;
		
		/*
			Copy the SOP2 predicate to the SOPWM instruction.
		*/
		CopyPredicate(psState, psSOPWMInst, psSOP2Inst);

		/*
			Copy the SOP2 sources to the SOPWM instruction.
		*/
		for (uSrc = 0; uSrc < 2; uSrc++)
		{
			SetSrcFromArg(psState, psSOPWMInst, uSrc, &psSOP2Inst->asArg[uSrc]);
		}

		/*
			Copy the parameters for this part of the SOP2 calculation to the
			SOPWM instruction.
		*/
		psSOPWMInst->u.psSopWm->uCop = psSop2->uCOp;
		psSOPWMInst->u.psSopWm->uAop = psSop2->uAOp;
		if (uPart == 0)
		{
			bComplementSrc1 = psSop2->bComplementCSrc1;
			psSOPWMInst->u.psSopWm->uSel1 = psSop2->uCSel1;
			psSOPWMInst->u.psSopWm->bComplementSel1 = psSop2->bComplementCSel1;
			psSOPWMInst->u.psSopWm->uSel2 = psSop2->uCSel2;
			psSOPWMInst->u.psSopWm->bComplementSel2 = psSop2->bComplementCSel2;
		}
		else
		{
			bComplementSrc1 = psSop2->bComplementASrc1;
			psSOPWMInst->u.psSopWm->uSel1 = psSop2->uASel1;
			psSOPWMInst->u.psSopWm->bComplementSel1 = psSop2->bComplementASel1;
			psSOPWMInst->u.psSopWm->uSel2 = psSop2->uASel2;
			psSOPWMInst->u.psSopWm->bComplementSel2 = psSop2->bComplementASel2;
		}

		/*
			If source 1 has a complement modifier then implement it in a separate instruction.
		*/
		if (bComplementSrc1)
		{
			if (!bGeneratedSrc1Complement)
			{
				GenerateComplement(psState, psSOPWMInst, &psSOPWMInst->asArg[0], USC_ALL_CHAN_MASK, &sComplementedSrc1);
				bGeneratedSrc1Complement = IMG_TRUE;
			}
			SetSrcFromArg(psState, psSOPWMInst, 0 /* uSrc */, &sComplementedSrc1);
		}
	}

	/*
		Copy the source for unwritten channels from the SOP2 instruction to the first instruction in
		the sequence.
	*/
	SetPartiallyWrittenDest(psState, psFirstInst, 0 /* uDestIdx */, psSOP2Inst->apsOldDest[0]);

	/*
		Copy the destination from the SOP2 instruction to the last instruction in the sequence.
	*/
	MoveDest(psState, psLastInst, 0 /* uMoveToDestIdx */, psSOP2Inst, 0 /* uMoveFromDestIdx */);
	psLastInst->auLiveChansInDest[0] = psSOP2Inst->auLiveChansInDest[0];

	/*
		If there are two instructions in the sequence then allocate a new temporary for the partial
		result from the first instruction.
	*/
	if (psFirstInst != psLastInst)
	{
		ARG		sMaskTemp;

		MakeNewTempArg(psState, UF_REGFORMAT_C10, &sMaskTemp);

		/*
			SOP2		ORIGDEST[=OLDDEST], ...
				->
			SOPWM1		MASKTEMP[=OLDDEST], ...
			SOPWM2		ORIGDEST[=MASKTEMP], ...
		*/
		SetPartiallyWrittenDest(psState, psLastInst, 0 /* uDestIdx */, &sMaskTemp);

		SetDestFromArg(psState, psFirstInst, 0 /* uDestIdx */, &sMaskTemp);
		psFirstInst->auLiveChansInDest[0] = GetPreservedChansInPartialDest(psState, psLastInst, 0 /* uDestIdx */);
	}
}

static
IMG_VOID FixUnsupportedFixedPointInstruction(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: FixUnsupportedFixedPointInstruction
    
 PURPOSE	: Fix fixed point instruction which don't support writing to
			  the destination ALPHA channel on cores with the vector
			  instruction set.

 PARAMETERS	: psState			- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	static const struct
	{
		IOPCODE	eOpcode;
		IMG_VOID (*pfFix)(PINTERMEDIATE_STATE, PINST);
	} aeOpcodes[] = 
	{
		{ISOP2, FixSOP2},
		{IFPMA, FixFPMA},
	};
	IMG_UINT32	uOpcodeIdx;

	for (uOpcodeIdx = 0; uOpcodeIdx < (sizeof(aeOpcodes) / sizeof(aeOpcodes[0])); uOpcodeIdx++)
	{
		IOPCODE				eOpcode = aeOpcodes[uOpcodeIdx].eOpcode;
		SAFE_LIST_ITERATOR	sIter;

		InstListIteratorInitialize(psState, eOpcode, &sIter);
		for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
		{
			PINST	psInst;

			psInst = InstListIteratorCurrent(&sIter);
			if (psInst->asDest[0].eFmt == UF_REGFORMAT_C10 && (psInst->auLiveChansInDest[0] & USC_ALPHA_CHAN_MASK) != 0)
			{
				aeOpcodes[uOpcodeIdx].pfFix(psState, psInst);

				RemoveInst(psState, psInst->psBlock, psInst);
				FreeInst(psState, psInst);
			}
		}
		InstListIteratorFinalise(&sIter);
	}
}

static
IMG_BOOL LowerC10VectorRegisters(PINTERMEDIATE_STATE				psState,
								 PLOWER_VECTOR_REGISTERS_CONTEXT	psContext,
								 PCODEBLOCK							psBlock,
								 PINST								psInst)
/*****************************************************************************
 FUNCTION	: LowerC10VectorRegisters
    
 PURPOSE	: Replace C10 vectors by two seperate scalar registers in an
			  instruction.

 PARAMETERS	: psState			- Compiler state.
			  psContext			- Stage state.
			  psBlock			- Block containing the instruction.
			  psInst			- Instruction to replace in.

 RETURNS	: TRUE if the instruction was freed.
*****************************************************************************/
{
	IMG_INT32	iArgIdx;
	IMG_UINT32	uArgCount;
	IMG_UINT32	auOldLiveChansInArg[USC_MAXIMUM_NONCALL_ARG_COUNT];
	IMG_BOOL	bC10Sources, bC10Dest;

	/*
		Save the old masks of channels used from each source.
	*/
	uArgCount = psInst->uArgumentCount;
	ASSERT(uArgCount <= USC_MAXIMUM_NONCALL_ARG_COUNT);
	for (iArgIdx = 0; iArgIdx < (IMG_INT32)uArgCount; iArgIdx++)
	{
		auOldLiveChansInArg[iArgIdx] = GetLiveChansInArg(psState, psInst, iArgIdx);
	}

	/*
		Check for an instruction which reads or writes C10 data.
	*/
	bC10Dest = bC10Sources = IMG_FALSE;
	switch (psInst->eOpcode)
	{
		case ISOPWM: 
		{
			SOPWM_PARAMS	sSavedParams;

			bC10Dest = bC10Sources = IMG_TRUE;

			sSavedParams = *psInst->u.psSopWm;
			SetOpcode(psState, psInst, ISOPWM_VEC); 
			*psInst->u.psSopWm = sSavedParams;
			break;
		}
		case ISOP2: 
		{
			SOP2_PARAMS	sSavedParams;

			bC10Dest = bC10Sources = IMG_TRUE;

			sSavedParams = *psInst->u.psSop2;
			SetOpcode(psState, psInst, ISOP2_VEC); 
			*psInst->u.psSop2 = sSavedParams;
			break;
		}
		case ISOP3: 
		{
			SOP3_PARAMS	sSavedParams;

			bC10Dest = bC10Sources = IMG_TRUE;

			sSavedParams = *psInst->u.psSop3;
			SetOpcode(psState, psInst, ISOP3_VEC); 
			*psInst->u.psSop3 = sSavedParams;
			break;
		}
		case ILRP1: 
		{
			LRP1_PARAMS	sSavedParams;

			bC10Dest = bC10Sources = IMG_TRUE;

			sSavedParams = *psInst->u.psLrp1;
			SetOpcode(psState, psInst, ILRP1_VEC); 
			*psInst->u.psLrp1 = sSavedParams;
			break;
		}
		case IFPMA: 
		{
			FPMA_PARAMS	sSavedParams;

			bC10Dest = bC10Sources = IMG_TRUE;

			sSavedParams = *psInst->u.psFpma;
			SetOpcode(psState, psInst, IFPMA_VEC); 
			*psInst->u.psFpma = sSavedParams;
			break;
		}
		case IFPDOT:
		{
			DOT34_PARAMS	sSavedParams;

			bC10Dest = bC10Sources = IMG_TRUE;

			sSavedParams = *psInst->u.psDot34;
			SetOpcode(psState, psInst, IFPDOT_VEC);
			*psInst->u.psDot34 = sSavedParams;
			break;
		}
		case ICOMBC10:
		{
			imgabort();
		}
		case IFPADD8:
		{
			if (psInst->uDestCount > 0)
			{
				bC10Dest = IMG_TRUE;
			}
			bC10Sources = IMG_TRUE;

			ModifyOpcode(psState, psInst, IFPADD8_VEC);
			break;
		}
		case IFPSUB8:
		{
			if (psInst->uDestCount > 0)
			{
				bC10Dest = IMG_TRUE;
			}
			bC10Sources = IMG_TRUE;

			ModifyOpcode(psState, psInst, IFPSUB8_VEC);
			break;
		}
		case ITESTMASK:
		case ITESTPRED:
		{
			if (psInst->u.psTest->eAluOpcode == IFPADD8 ||
				psInst->u.psTest->eAluOpcode == IFPSUB8)
			{
				if (psInst->eOpcode == ITESTPRED)
				{
					if (psInst->uDestCount > TEST_PREDICATE_ONLY_DEST_COUNT)
					{
						bC10Dest = IMG_TRUE;
					}
				}
				else
				{
					bC10Dest = IMG_TRUE;
				}
				bC10Sources = IMG_TRUE;
				if (psInst->eOpcode == ITESTMASK)
				{
					ModifyOpcode(psState, psInst, IFPTESTMASK_VEC);
				}
				else
				{
					ModifyOpcode(psState, psInst, IFPTESTPRED_VEC);
				}

				if (psInst->u.psTest->eAluOpcode == IFPADD8)
				{
					psInst->u.psTest->eAluOpcode = IFPADD8_VEC;
				}
				else
				{
					ASSERT(psInst->u.psTest->eAluOpcode == IFPSUB8);
					psInst->u.psTest->eAluOpcode = IFPSUB8_VEC;
				}
			}
			break;
		}
		case IPCKC10C10:
		case IPCKC10F16:
		{
			ExpandTwoSourceC10PCK(psState, psContext, psBlock, psInst);
			return IMG_TRUE;
		}
		case IVPCKC10FLT:
		{
			bC10Dest = IMG_TRUE;
			break;
		}
		case IPCKC10F32:
		{
			bC10Dest = IMG_TRUE;

			SetOpcode(psState, psInst, IVPCKC10FLT);
			psInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F32;
			psInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
			break;
		}
		case IUNPCKF32C10:
		case IUNPCKF16C10:
		{
			IMG_UINT32	uSrcComponent;

			bC10Sources = IMG_TRUE;

			uSrcComponent = GetPCKComponent(psState, psInst, 0 /* uArgIdx */);

			SetOpcode(psState, psInst, IVPCKFLTC10_VEC);
			psInst->u.psVec->auSwizzle[0] = g_auReplicateSwizzles[uSrcComponent];

			uArgCount = 1;
			break;
		}
		case IMOVC_C10:
		{
			IMG_UINT32	uArgIdx;
			TEST_TYPE	eTest;

			bC10Dest = bC10Sources = IMG_TRUE;

			eTest = psInst->u.psMovc->eTest;
			SetOpcode(psState, psInst, IVMOVCC10);
			psInst->u.psVec->eMOVCTestType = eTest;
			for (uArgIdx = 0; uArgIdx < 3; uArgIdx++)
			{
				psInst->u.psVec->auSwizzle[uArgIdx] = USEASM_SWIZZLE(X, Y, Z, W);
				psInst->u.psVec->aeSrcFmt[uArgIdx] = UF_REGFORMAT_C10;
			}

			if (psInst->u.psVec->eMOVCTestType == TEST_TYPE_GTE_ZERO)
			{
				psInst->u.psVec->eMOVCTestType = TEST_TYPE_LT_ZERO;
				SwapInstSources(psState, psInst, 1 /* uSrc1Idx */, 2 /* uSrc2Idx */);
			}
			break;
		}
		case IMOVC_U8:
		{
			IMG_UINT32	uArgIdx;
			TEST_TYPE	eTest;

			bC10Dest = bC10Sources = IMG_TRUE;

			eTest = psInst->u.psMovc->eTest;
			SetOpcode(psState, psInst, IVMOVCU8);
			psInst->u.psVec->eMOVCTestType = eTest;
			for (uArgIdx = 0; uArgIdx < 3; uArgIdx++)
			{
				psInst->u.psVec->auSwizzle[uArgIdx] = USEASM_SWIZZLE(X, Y, Z, W);
				psInst->u.psVec->aeSrcFmt[uArgIdx] = UF_REGFORMAT_U8;
			}

			if (psInst->u.psVec->eMOVCTestType == TEST_TYPE_GTE_ZERO)
			{
				psInst->u.psVec->eMOVCTestType = TEST_TYPE_LT_ZERO;
				SwapInstSources(psState, psInst, 1 /* uSrc1Idx */, 2 /* uSrc2Idx */);
			}
			break;
		}
		case IMKC10VEC:
		{
			IMG_BOOL	bGroupableSources;

			bGroupableSources = IMG_TRUE;
			if ((psInst->auDestMask[0] & USC_RGB_CHAN_MASK) != 0 &&
				(psInst->auDestMask[0] & USC_ALPHA_CHAN_MASK) != 0)
			{
				if (psInst->asArg[0].uType != psInst->asArg[0].uType)
				{
					bGroupableSources = IMG_FALSE;
				}
				if (psInst->asArg[0].uType != USEASM_REGTYPE_TEMP)
				{
					if ((psInst->asArg[0].uNumber % 2) != 0)
					{
						bGroupableSources = IMG_FALSE;
					}
					if ((psInst->asArg[0].uNumber + 1) != psInst->asArg[1].uNumber)
					{
						bGroupableSources = IMG_FALSE;
					}
				}
			}

			if (!bGroupableSources)
			{
				IMG_UINT32	uSrcIdx;

				for (uSrcIdx = 0; uSrcIdx < 2; uSrcIdx++)
				{
					PINST		psMOVInst;
					IMG_UINT32	uTempNum = GetNextRegister(psState);

					psMOVInst = AllocateInst(psState, psInst);
					SetOpcode(psState, psMOVInst, IMOV);

					SetDest(psState, psMOVInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uTempNum, UF_REGFORMAT_C10);
					
					MoveSrc(psState, psMOVInst, 0 /* uMoveToSrcIdx */, psInst, uSrcIdx);

					SetSrc(psState, psInst, uSrcIdx, USEASM_REGTYPE_TEMP, uTempNum, UF_REGFORMAT_C10);

					InsertInstBefore(psState, psBlock, psMOVInst, psInst);
				}
			}

			bC10Dest = IMG_TRUE;
			SetOpcode(psState, psInst, IVMOVC10);
			psInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
			psInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_C10;
			break;
		}
		case IFRCP:
		case IFRSQ:
		case IFLOG:
		case IFEXP:
		{
			IMG_UINT32				uArgIdx;
			FLOAT_SOURCE_MODIFIER	sSourceMod;

			ASSERT(psInst->uDestCount > 0 && psInst->asDest[0].eFmt == UF_REGFORMAT_C10);
			ASSERT(psInst->asArg[0].eFmt == UF_REGFORMAT_C10);
			ASSERT(GetBit(psInst->auFlag, INST_TYPE_PRESERVE));

			SetBit(psInst->auFlag, INST_TYPE_PRESERVE, 0);

			/*
				Save the source modifiers from the instruction.
			*/
			sSourceMod = *GetFloatMod(psState, psInst, 0 /* uArgIdx */);

			/*	
				Switch to the vector form of the instruction.
			*/
			switch (psInst->eOpcode)
			{
				case IFRCP: SetOpcode(psState, psInst, IVRCP); break;
				case IFRSQ: SetOpcode(psState, psInst, IVRSQ); break;
				case IFLOG: SetOpcode(psState, psInst, IVLOG); break;
				case IFEXP: SetOpcode(psState, psInst, IVEXP); break;
				default: imgabort();
			}

			/*
				Replace the component select by a replicate swizzle.
			*/
			psInst->u.psVec->auSwizzle[0] = g_auReplicateSwizzles[sSourceMod.uComponent];
			psInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_C10;
			
			/*
				Apply the source modifiers from the original instruction
			*/
			psInst->u.psVec->asSrcMod[0].bAbs = sSourceMod.bAbsolute;
			psInst->u.psVec->asSrcMod[0].bNegate = sSourceMod.bNegate;

			ExpandC10Destination(psState, psContext, psInst);

			ExpandC10Source(psState, 
							psContext,
							psInst,
							0 /* uVectorArgIdx */,
							1 /* uScalarArg1Idx */,
							2 /* uScalarArg2Idx */,
						    auOldLiveChansInArg[0]);

			for (uArgIdx = 3; uArgIdx < SOURCE_ARGUMENTS_PER_VECTOR; uArgIdx++)
			{
				SetSrcUnused(psState, psInst, uArgIdx);
			}

			SetSrcUnused(psState, psInst, 0 /* uSrcIdx */);
			return IMG_TRUE;
		}
		case IVPCKFLTC10:
		{
			ASSERT(GetBit(psInst->u.psVec->auSelectUpperHalf, 0) == 0);

			bC10Sources = IMG_TRUE;

			ModifyOpcode(psState, psInst, IVPCKFLTC10_VEC);
			psInst->u.psVec->auSwizzle[1] = psInst->u.psVec->auSwizzle[0];
			break;
		}
		case IMOV:
		{
			if (psInst->asDest[0].eFmt == UF_REGFORMAT_C10)
			{
				bC10Dest = IMG_TRUE;
				
				if (psInst->asArg[0].eFmt == UF_REGFORMAT_U8)
				{
					ASSERT(psInst->asArg[0].uType == USEASM_REGTYPE_IMMEDIATE);
					psInst->asArg[0].eFmt = UF_REGFORMAT_C10;
				}
			}
			if (psInst->asArg[0].eFmt == UF_REGFORMAT_C10)
			{
				bC10Sources = IMG_TRUE;
			}
			if (bC10Dest || bC10Sources)
			{
				SetOpcode(psState, psInst, IVMOVC10);
				psInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
				psInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_C10;
			}
			break;
		}
		#if defined(OUTPUT_USPBIN)
		case ISMP_USP:
		case ISMPBIAS_USP:
		case ISMPREPLACE_USP:
		case ISMPGRAD_USP:
		case ISMP_USP_NDR:
		case ISMPUNPACK_USP:
		{
			if (psInst->asDest[0].eFmt == UF_REGFORMAT_C10)
			{
				bC10Dest = IMG_TRUE;
			}
			break;
		}
		#endif /* defined(OUTPUT_USPBIN) */
		case IDELTA:
		{
			if (psInst->asDest[0].eFmt == UF_REGFORMAT_C10)
			{
				if ((psInst->auLiveChansInDest[0] & USC_ALPHA_CHAN_MASK) != 0)
				{
					PINST		psAlphaDeltaInst;
					IMG_UINT32	uPredIdx;

					psAlphaDeltaInst = AllocateInst(psState, psInst);
					SetOpcode(psState, psAlphaDeltaInst, IDELTA);

					ASSERT(psInst->asDest[0].uType == USEASM_REGTYPE_TEMP);
					SetDest(psState, 
							psAlphaDeltaInst, 
							0 /* uDestIdx */, 
							USEASM_REGTYPE_TEMP, 
							GetRemapLocation(psState, &psContext->sRemap, psInst->asDest[0].uNumber), 
							UF_REGFORMAT_C10);

					SetArgumentCount(psState, psAlphaDeltaInst, psInst->uArgumentCount);
					for (uPredIdx = 0; uPredIdx < psInst->uArgumentCount; uPredIdx++)
					{
						PARG	psSrc = &psInst->asArg[uPredIdx];

						if (psSrc->uType == USEASM_REGTYPE_IMMEDIATE)
						{
							SetSrcFromArg(psState, psAlphaDeltaInst, uPredIdx, psSrc);
						}
						else
						{
							ASSERT(psSrc->uType == USEASM_REGTYPE_TEMP);
							ASSERT(psSrc->eFmt == UF_REGFORMAT_C10);

							SetSrc(psState, 
								   psAlphaDeltaInst, 
								   uPredIdx, 
								   USEASM_REGTYPE_TEMP, 
								   GetRemapLocation(psState, &psContext->sRemap, psSrc->uNumber), 
								   UF_REGFORMAT_C10);
						}
					}
					InsertInstAfter(psState, psBlock, psAlphaDeltaInst, psInst);
				}
			}
			return IMG_TRUE;
		}
		default:
		{
			/*
				This instruction doesn't use C10 sources or destinations.
			*/
			return IMG_FALSE;
		}
	}

	/*
		Replace a C10 vector destination.
	*/
	if (bC10Dest)
	{
		ExpandC10Destination(psState, psContext, psInst);
		ExpandC10PartialDest(psState, psContext, psInst);
	}

	/*
		Replace C10 vector sources.
	*/
	if (bC10Sources)
	{
		for (iArgIdx = uArgCount - 1; iArgIdx >= 0; iArgIdx--)
		{
			ExpandC10Source(psState, 
							psContext,
							psInst, 
							iArgIdx, 
							iArgIdx * 2 + 0, 
							iArgIdx * 2 + 1, 
							auOldLiveChansInArg[iArgIdx]);
		}
	}

	return IMG_FALSE;
}

IMG_INTERNAL
IMG_VOID DropUnusedVectorComponents(PINTERMEDIATE_STATE	psState,
									PINST				psInst,
									IMG_UINT32			uArgIdx)
/*****************************************************************************
 FUNCTION	: DropUnusedVectorComponents
    
 PURPOSE	: Replace unreferenced components at the end of a vector by
			  USC_REGTYPE_UNUSEDSOURCE.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to replace in.
			  uArgIdx			- Index of the vector argument to modify.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_INT32	iCompIdx;

	for (iCompIdx = 3; iCompIdx >= 0; iCompIdx--)
	{
		IMG_UINT32	uUSSource = uArgIdx * SOURCE_ARGUMENTS_PER_VECTOR + 1 + iCompIdx;

		if (GetLiveChansInArg(psState, psInst, uUSSource) == 0)
		{
			SetSrcUnused(psState, psInst, uUSSource);
		}
		else
		{
			break;
		}
	}
}

static
IMG_VOID SetVectorArgumentComponent(PINTERMEDIATE_STATE				psState,
									PREGISTER_REMAP					psRemap,
								    PINST							psInst,
								    IMG_BOOL						bDest,
								    IMG_UINT32						uArg,
								    PARG							psVectorArgument,
								    IMG_UINT32						uComponent)
/*****************************************************************************
 FUNCTION	: SetVectorArgumentComponent
    
 PURPOSE	: Set the scalar register corresponding to a component of a vector
			  register as an argument to an instruction.

 PARAMETERS	: psState			- Compiler state.
			  psRemap			- Table mapping vector register numbers to
								scalar register numbers.
			  psInst			- Instruction to modify.
			  bDest				- If TRUE set a destination.
								  If FALSE set a source.
		      uArg				- Index of the source or destination to set.
			  psVectorArgument	- Vector register.
			  uComponent		- Component of the vector register to set.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psVectorArgument->uType == USC_REGTYPE_REGARRAY)
	{
		SetArrayArgument(psState, 
						 psInst, 
						 bDest,
						 uArg,
						 psVectorArgument->uNumber,
						 psVectorArgument->uArrayOffset + uComponent,
						 psVectorArgument->eFmt);
		SetArgumentIndex(psState, 
					 	 psInst,
						 bDest,
						 uArg,
						 psVectorArgument->uIndexType, 
						 psVectorArgument->uIndexNumber, 
						 psVectorArgument->uIndexArrayOffset, 
						 psVectorArgument->uIndexStrideInBytes);
	}
	else if (psVectorArgument->uType == USEASM_REGTYPE_FPINTERNAL)
	{
		IMG_UINT32	uRegisterNumber;

		/*
			Non-temporary vector arguments must be F32 format.
		*/
		ASSERT(psVectorArgument->eFmt == UF_REGFORMAT_F32);

		/*
			Convert the register number to scalar units.
		*/
		uRegisterNumber = psVectorArgument->uNumber * SCALAR_REGISTERS_PER_F32_VECTOR;

		SetArgument(psState, 
					psInst, 
					bDest,
					uArg, 
					psVectorArgument->uType, 
					uRegisterNumber + uComponent,
					psVectorArgument->eFmt);
	}
	else
	{
		IMG_UINT32	uVectorBaseRegNum;

		ASSERT(psVectorArgument->uType == USEASM_REGTYPE_TEMP);

		/*
			Allocate an array of scalar sized registers corresponding to the vector.
		*/
		uVectorBaseRegNum = GetRemapLocationCount(psState, 
												  psRemap, 
												  psVectorArgument->uNumber, 
												  g_auExpandedVectorLength[psVectorArgument->eFmt]);

		/*
			Set the scalar register from the array corresponding to this component of
			the vector as a source argument.
		*/
		SetArgument(psState, 
					psInst, 
					bDest,
					uArg, 
					psVectorArgument->uType, 
					uVectorBaseRegNum + uComponent,
					psVectorArgument->eFmt);
	}
}

static
IMG_VOID ExpandVectorDestComponent(PINTERMEDIATE_STATE				psState,
								   PLOWER_VECTOR_REGISTERS_CONTEXT	psContext,
								   PARG								psComponentDest,
								   PARG								psVectorDest,
								   IMG_UINT32						uComponentIdx,
								   IMG_UINT32						uNumComponentsToAlloc)
/*****************************************************************************
 FUNCTION	: ExpandVectorDestComponent
    
 PURPOSE	: Get the details of the scalar register containing a component of
			  a vector register.

 PARAMETERS	: psState			- Compiler state.
			  psContext			- Pass state.
			  psComponentDest	- Written with the scalar component.
			  psVectorDest		- Vector destination.
			  uComponentIdx		- Component to expand.
			  uNumComponentsToAlloc
								- Size of the vector destination in 32-bit
								registers.

 RETURNS	: Nothing.
*****************************************************************************/
{
	*psComponentDest = *psVectorDest;

	if (psComponentDest->uType == USC_REGTYPE_REGARRAY)
	{
		psComponentDest->uArrayOffset += uComponentIdx;
	}
	else
	{
		if (psComponentDest->uType == USEASM_REGTYPE_TEMP)
		{
			psComponentDest->uNumber = 
				GetRemapLocationCount(psState, &psContext->sRemap, psVectorDest->uNumber, uNumComponentsToAlloc);
		}
		else
		{
			psComponentDest->uNumber *= uNumComponentsToAlloc;
		}
		psComponentDest->uNumber += uComponentIdx;
		if (psComponentDest->uType == USEASM_REGTYPE_TEMP)
		{
			psComponentDest->psRegister = GetVRegister(psState, USEASM_REGTYPE_TEMP, psComponentDest->uNumber);
		}
	}
}

static
IMG_UINT32 GetInitializedChannelsInGPIRegister(PINTERMEDIATE_STATE	psState, 
											   PINST				psInst,
											   IMG_UINT32			uRegNum)
/*****************************************************************************
 FUNCTION	: GetInitializedChannelsInGPIRegister
    
 PURPOSE	: Get the mask of channels initialized in an internal register before
			  a specified instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.
			  uRegNum			- Internal register number.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;
	PUSEDEF_CHAIN	psIReg;
	IMG_UINT32		uInitializedChanMask;

	ASSERT(uRegNum < MAXIMUM_GPI_SIZE_IN_SCALAR_REGS);
	psIReg = psInst->psBlock->apsIRegUseDef[uRegNum];

	uInitializedChanMask = 0;
	for (psListEntry = psIReg->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUseDef;
		PINST	psUseInst;
		
		psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		
		psUseInst = UseDefGetInst(psUseDef);
		if (psUseInst->uBlockIndex >= psInst->uBlockIndex)
		{
			break;
		}

		if (psUseDef->eType == DEF_TYPE_INST)
		{
			IMG_UINT32	uDestIdx;

			uDestIdx = psUseDef->uLocation;
			ASSERT(uDestIdx < psUseInst->uDestCount);

			if (psUseInst->apsOldDest[uDestIdx] != NULL)
			{
				ASSERT(EqualArgs(psUseInst->apsOldDest[uDestIdx], &psUseInst->asDest[uDestIdx]));
				uInitializedChanMask |= psUseInst->auDestMask[uDestIdx];
			}
			else
			{
				uInitializedChanMask = psUseInst->auDestMask[uDestIdx];
			}
		}
	}

	return uInitializedChanMask;
}

static
IMG_VOID ExpandVectorDest(PINTERMEDIATE_STATE				psState,
						  PLOWER_VECTOR_REGISTERS_CONTEXT	psContext,
						  PINST								psInst,
						  IMG_UINT32						uVecDestMask,
						  IMG_UINT32						uVecLiveChansInDest,
						  IMG_UINT32						uBaseDestIdx,
						  IMG_BOOL							bLastDest,
						  IMG_UINT32						uCompStart,
						  IMG_UINT32						uNumComponents,
						  IMG_UINT32						uNumComponentsToAlloc,
						  PARG								psVectorDest,
						  PARG								psVectorOldDest,
						  IMG_BOOL							bOldDestPresent)
/*****************************************************************************
 FUNCTION	: ExpandVectorDest
    
 PURPOSE	: Replace a vector destination by a vector of scalar destinations.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to replace in.
			  uVecDestMask		- Write mask for the vector destination.
			  uVecLiveChansInDest
								- Mask of channels used from the vector destination.
			  uBaseDestIdx		- Start of the scalar destinations in the instruction's
								array of destinations.
			  uCompStart		- Start of the components of the new vector.
			  uNumComponents	- Number of components in the new vector.
			  uNumComponentsToAlloc
								- Number of temporaries to allocate when creating the
								array of scalar registers representing the vector.
			  uScalarDestType	- Type for the scalar destinations.
			  uScalarDestStartNum
								- First register number of the scalar destinations.
			  eScalarDestFmt	- Format for the scalar destinations.
			  uArrayNum			- If the vector destination is an element of the
								array then the index of the array.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uCompIdx;
	IMG_UINT32	auNewDestMask[VECTOR_LENGTH];
	IMG_UINT32	auNewLiveChansInDest[VECTOR_LENGTH];
	IMG_UINT32	uNumComponentsWritten;
	IMG_UINT32	uLastComponentInVector;

	/*
		Convert the destination mask to units of scalar registers.
	*/
	uNumComponentsWritten = 0;
	ASSERT(uNumComponents <= VECTOR_LENGTH);
	for (uCompIdx = 0; uCompIdx < uNumComponents; uCompIdx++)
	{
		auNewDestMask[uCompIdx] = ChanMaskToByteMask(psState, uVecDestMask, uCompStart + uCompIdx, psVectorDest->eFmt);
		auNewLiveChansInDest[uCompIdx] = 
			ChanMaskToByteMask(psState, uVecLiveChansInDest, uCompStart + uCompIdx, psVectorDest->eFmt);

		/*
			Keep track of the last component written.
		*/
		if (auNewDestMask[uCompIdx] != 0)
		{
			uNumComponentsWritten = uCompIdx + 1;
		}
	}
	ASSERT(uNumComponentsWritten > 0);

	if (psVectorDest->uType == USEASM_REGTYPE_FPINTERNAL)
	{
		uNumComponentsWritten = SCALAR_REGISTERS_PER_F32_VECTOR;
	}

	if (bLastDest)
	{
		SetDestCount(psState, psInst, uBaseDestIdx + uNumComponentsWritten);
	}
	for (uCompIdx = 0; uCompIdx < uNumComponentsWritten; uCompIdx++)
	{
		IMG_UINT32	uDestIdx = uBaseDestIdx + uCompIdx;
		ARG			sScalarDest;

		psInst->auDestMask[uDestIdx] = auNewDestMask[uCompIdx];
		psInst->auLiveChansInDest[uDestIdx] = auNewLiveChansInDest[uCompIdx];
	
		/*
			Set up the scalar destination.
		*/
		ExpandVectorDestComponent(psState, 
								  psContext, 
								  &sScalarDest, 
								  psVectorDest, 
								  uCompStart + uCompIdx,
								  uNumComponentsToAlloc);
		SetDestFromArg(psState, psInst, uDestIdx, &sScalarDest);

		ASSERT(psInst->apsOldDest[uDestIdx] == NULL);
		if (
				bOldDestPresent && 
				(
					(auNewLiveChansInDest[uCompIdx] & ~auNewDestMask[uCompIdx]) != 0 || 
					!NoPredicate(psState, psInst)
				)
			)
		{
			ARG	sScalarPartialDest;

			ExpandVectorDestComponent(psState, 
									  psContext, 
									  &sScalarPartialDest, 
									  psVectorOldDest, 
									  uCompStart + uCompIdx,
									  uNumComponents);

			/*
				If this component of an internal register has never been written then don't generate
				a partial destination.
			*/
			if (sScalarPartialDest.uType == USEASM_REGTYPE_FPINTERNAL)
			{
				if (GetInitializedChannelsInGPIRegister(psState, psInst, sScalarPartialDest.uNumber) == 0)
				{
					continue;
				}
			}

			SetPartiallyWrittenDest(psState, psInst, uDestIdx, &sScalarPartialDest);
		} 
	}

	uLastComponentInVector = min(4, psInst->uDestCount - uBaseDestIdx);
	for (; uCompIdx < uLastComponentInVector; uCompIdx++)
	{
		IMG_UINT32	uDestIdx = uBaseDestIdx + uCompIdx;

		SetDestUnused(psState, psInst, uDestIdx);

		ASSERT(psInst->apsOldDest[uDestIdx] == NULL);
		
		psInst->auDestMask[uDestIdx] = 0;
		psInst->auLiveChansInDest[uDestIdx] = 0;
	}
}

static
IMG_VOID ExpandVectorSource(PINTERMEDIATE_STATE				psState,
							PLOWER_VECTOR_REGISTERS_CONTEXT	psContext,
							PINST							psInst,
							PARG							psVecSource,
							IMG_UINT32						uVecStartIdx,
							IMG_UINT32						uComponentOffset,
							IMG_UINT32						uVecLength,
							IMG_UINT32						uChansLiveInVecSource)
/*****************************************************************************
 FUNCTION	: ExpandVectorSource
    
 PURPOSE	: Replace a vector source by a vector of scalar sources.

 PARAMETERS	: psState			- Compiler state.
			  psContext			- Pass state.
			  psInst			- Instruction to replace in.
			  psVecSource		- Vector source argument.
			  uVecStartIdx		- Start of the scalar sources within the source
								arguments.
			  uComponentOffset
								- 
			  uVecLength		- Length of the replacement vector in registers.
		      uChansLiveInVecSource
								- Mask of the channels live in the vector source.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uCompIdx;
	IMG_UINT32	uUsedScalarRegsMask;

	/*
		Skip unused sources.
	*/
	if (psVecSource->uType == USC_REGTYPE_UNUSEDSOURCE)
	{
		return;
	}
	
	/*
		Convert the mask of channels referenced to a mask of scalar
		registers referenced.
	*/
	if (psVecSource->uType == USEASM_REGTYPE_FPINTERNAL)
	{
		uUsedScalarRegsMask = USC_ALL_CHAN_MASK;
	}
	else
	{
		if (psVecSource->eFmt == UF_REGFORMAT_F16)
		{
			uUsedScalarRegsMask = 0;
			if ((uChansLiveInVecSource & USC_XY_CHAN_MASK) != 0)
			{
				uUsedScalarRegsMask |= USC_X_CHAN_MASK;
			}
			if ((uChansLiveInVecSource & USC_ZW_CHAN_MASK) != 0)
			{
				uUsedScalarRegsMask |= USC_Y_CHAN_MASK;
			}
		}
		else
		{
			ASSERT(psVecSource->eFmt == UF_REGFORMAT_F32);
			uUsedScalarRegsMask = uChansLiveInVecSource;	
		}
	}

	/*
		Fill in each scalar source.
	*/
	for (uCompIdx = 0; uCompIdx < uVecLength; uCompIdx++)
	{	
		IMG_UINT32	uUSSource = uVecStartIdx + uCompIdx;

		if ((uUsedScalarRegsMask >> uCompIdx) == 0)
		{
			break;
		}

		SetVectorArgumentComponent(psState,
								   &psContext->sRemap,
								   psInst,
								   IMG_FALSE /* bDest */,
								   uUSSource,
								   psVecSource,
								   uComponentOffset + uCompIdx);
	}
	/*
		Replace any unreferenced tail of the vector by dummy sources.
	*/
	for (; uCompIdx < uVecLength; uCompIdx++)
	{
		SetSrcUnused(psState, psInst, uVecStartIdx + uCompIdx);
	}
}

IMG_INTERNAL
IMG_VOID FixDroppedUnusedVectorComponents(PINTERMEDIATE_STATE	psState, 
										  PINST					psInst,
										  IMG_UINT32			uVecArgIdx)
/*****************************************************************************
 FUNCTION	: FixDroppedUnusedVectorComponents
    
 PURPOSE	: Replace unreferenced components at the end of a vector by
			  USEASM_REGTYPE_FPINTERNAL if no component is used from the 
			  vector source at all.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to replace in.
			  uVecArgIdx		- Index of the vector argument to modify.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uUnusedArgMask = 0;
	IMG_UINT32	uArgIdx;
	IMG_UINT32	uUSSource;
	PARG		psUSSource;

	PVR_UNREFERENCED_PARAMETER(psState);

	for (uArgIdx = 0; uArgIdx < 4; uArgIdx++)
	{
		uUSSource = uVecArgIdx * SOURCE_ARGUMENTS_PER_VECTOR + 1 + uArgIdx;
		psUSSource = &psInst->asArg[uUSSource];

		if (psUSSource->uType == USC_REGTYPE_UNUSEDSOURCE)
		{
			uUnusedArgMask |= 1 << uArgIdx;
		}
		else
		{
			break;
		}
	}

	if (uUnusedArgMask == USC_DESTMASK_FULL)
	{
		for (uArgIdx = 0; uArgIdx < 4; uArgIdx++)
		{
			IMG_UINT32	uReplaceRegType;
			IMG_UINT32	uReplaceRegNumber;

			uUSSource = uVecArgIdx * SOURCE_ARGUMENTS_PER_VECTOR + 1 + uArgIdx;

			if (CanUseSrc(psState, psInst, uUSSource, USEASM_REGTYPE_IMMEDIATE, USC_REGTYPE_NOINDEX))
			{
				uReplaceRegType = USEASM_REGTYPE_IMMEDIATE;
				uReplaceRegNumber = 0;
			}
			else
			{
				uReplaceRegType = USEASM_REGTYPE_FPINTERNAL;
				uReplaceRegNumber = uArgIdx;
			}
			SetSrc(psState, psInst, uUSSource, uReplaceRegType, uReplaceRegNumber, psInst->u.psVec->aeSrcFmt[uVecArgIdx]);
		}
	}
}

static
IMG_VOID InsertOtherPartMove(PINTERMEDIATE_STATE				psState,
							 PLOWER_VECTOR_REGISTERS_CONTEXT	psContext,
							 PINST								psInst,
							 IMG_UINT32							uOtherPartMask,
							 IMG_UINT32							uOtherCompLoadStart,
							 PARG								psVecDest,
							 PARG								psVecOldDest)
{
	PINST	psOtherPartMove;

	if (uOtherPartMask == 0)
	{
		return;
	}

	ASSERT(psVecOldDest->eFmt == UF_REGFORMAT_F32);

	psOtherPartMove = AllocateInst(psState, psInst);
	SetOpcode(psState, psOtherPartMove, IVMOV);

	/*
		Keep a list of the moves inserted so we can replace the destinations by the
		sources at the end of this stage.
	*/
	AppendToList(&psContext->sOtherHalfMoveList, &psOtherPartMove->sAvailableListEntry);

	/*
		Write the half of the destination which isn't written by the
		current instruction.
	*/
	psOtherPartMove->auDestMask[0] = uOtherPartMask;
	psOtherPartMove->auLiveChansInDest[0] = uOtherPartMask;

	/*
		Set the expanded version of the destination as the destination
		of the move instruction.
	*/
	ExpandVectorDest(psState,
					 psContext,
					 psOtherPartMove,
					 uOtherPartMask,
					 uOtherPartMask,
					 0 /* uBaseDestIdx */,
					 IMG_TRUE /* bLastDest */,
					 uOtherCompLoadStart,
					 SCALAR_REGISTERS_PER_F32_VECTOR /* uNumComponents */,
					 SCALAR_REGISTERS_PER_F32_VECTOR /* uNumComponentsToAlloc */,
					 psVecDest,
					 NULL /* psVecOldDest */,
					 IMG_FALSE /* bOldDestPresent */);

	/*
		Set the expanded version of the partially written destination as a
		source.
	*/
	SetSrcUnused(psState, psOtherPartMove, 0 /* uSrcIdx */);
	ExpandVectorSource(psState,
					   psContext,
					   psOtherPartMove,
					   psVecOldDest,
					   1 /* uVecStartIdx */,
					   uOtherCompLoadStart /* uComponentOffset */,
					   2 /* uVecLength */,
					   uOtherPartMask >> uOtherCompLoadStart);
	psOtherPartMove->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
	psOtherPartMove->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F32;
	InsertInstAfter(psState, psInst->psBlock, psOtherPartMove, psInst);
}

static
IMG_BOOL IsScalarDestOnly(PINST psInst, IMG_UINT32 uDestIdx)
/*****************************************************************************
 FUNCTION	: IsScalarDestOnly
    
 PURPOSE	: Check for a instruction destination which is written always
			  with a scalar result.

 PARAMETERS	: psInst			- Instruction to check.
			  uDestIdx			- Destination to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (psInst->eOpcode == IVDUAL && uDestIdx == VDUAL_DESTSLOT_UNIFIEDSTORE && !IsVDUALUnifiedStoreVectorResult(psInst))
	{
		return IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
}

static
IMG_VOID LowerVectorDestination(PINTERMEDIATE_STATE				psState,
								PLOWER_VECTOR_REGISTERS_CONTEXT	psContext,
								PINST							psInst,
								IMG_UINT32						uBaseDestIdx,
								IMG_BOOL						bLastDest,
								IMG_UINT32						uOrigDestIdx,
								PARG							psVecDest,
								PARG							psVecOldDest,
								IMG_BOOL						bOldDestPresent,
								IMG_UINT32						uVecDestMask,
								IMG_UINT32						uVecLiveChansInDest)
/*****************************************************************************
 FUNCTION	: LowerVectorDestination
    
 PURPOSE	: Replace vector registers by scalar registers in a single basic
			  block.

 PARAMETERS	: psState			- Compiler state.
			  psContext			- State for this stage.
			  psInst			- Instruction whose destinations are to be
								modified.
			  uBaseDestIdx		- Index of the first 

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uNumComponents;
	IMG_UINT32		uCompLoadStart;

	if (psVecDest->eFmt == UF_REGFORMAT_F32)
	{
		/*
			Expand an F32 vector to four scalar registers.
		*/
		uNumComponents = SCALAR_REGISTERS_PER_F32_VECTOR;

		if (psVecDest->uType != USEASM_REGTYPE_FPINTERNAL)
		{
			IMG_UINT32	uInstWriteMask;
			IMG_UINT32	uNonInstWriteMask;
			IMG_UINT32	uNumComponentsWritten;

			uCompLoadStart = psInst->u.psVec->uDestSelectShift;
			psInst->u.psVec->uDestSelectShift = USC_X_CHAN;

			if (IsScalarDestOnly(psInst, uOrigDestIdx))
			{
				ASSERT(uCompLoadStart >= USC_X_CHAN && uCompLoadStart <= USC_W_CHAN);
				uNumComponentsWritten = 1;
			}
			else
			{
				ASSERT(uCompLoadStart == USC_X_CHAN || uCompLoadStart == USC_Z_CHAN);
				uNumComponentsWritten = 2;
			}

			uInstWriteMask = (1 << uNumComponentsWritten) - 1;
			uInstWriteMask <<= uCompLoadStart;
			ASSERT((uVecDestMask & uVecLiveChansInDest & ~uInstWriteMask) == 0);
			uNonInstWriteMask = uVecLiveChansInDest & ~uInstWriteMask;

			/*
				Insert a move from the other half (XY or ZW) of the destination from the
				same half of the partially written destination.
			*/
			if (uNonInstWriteMask != 0 && bOldDestPresent)
			{
				IMG_UINT32	uHalf;

				for (uHalf = 0; uHalf < 2; uHalf++)
				{
					IMG_UINT32	uHalfStartChan = uHalf * 2;

					InsertOtherPartMove(psState,
										psContext,
										psInst,
										uNonInstWriteMask & (USC_XY_CHAN_MASK << uHalfStartChan),
										uHalfStartChan,
										psVecDest,
										psVecOldDest);
				}
			}
		}
		else
		{
			uCompLoadStart = 0;
		}
	}
	else if (psVecDest->eFmt == UF_REGFORMAT_F16)
	{
		/*
			Always expand an F16 vector to two scalar registers.
		*/
		uNumComponents = SCALAR_REGISTERS_PER_F16_VECTOR;

		uCompLoadStart = 0;
	}
	else
	{
		ASSERT(psVecDest->eFmt == UF_REGFORMAT_C10);
		ASSERT((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORCOMPLEXOP) != 0);
		ExpandC10Destination(psState, psContext, psInst);
		return;
	}
	
	/*
		Replace the vector destination by the scalar registers.
	*/
	ExpandVectorDest(psState,
					 psContext,
					 psInst,
					 uVecDestMask,
					 uVecLiveChansInDest,
					 uBaseDestIdx,
					 bLastDest,
					 uCompLoadStart,
					 uNumComponents, 
					 uNumComponents,
					 psVecDest,
					 psVecOldDest,
					 bOldDestPresent);

	/*
		If the instruction writes only 32-bits in a 64-bit unified store register then insert a MOV instruction
		to copy the other 32-bits from the partially overwritten destination.
	*/
	if (bOldDestPresent && psInst->uDestCount == 1 && psInst->asDest[0].uType == USEASM_REGTYPE_TEMP)
	{
		PINST		psMOVInst;
		ARG			sScalarDest;
		ARG			sScalarSrc;

		psMOVInst = AllocateInst(psState, psInst);
		SetOpcode(psState, psMOVInst, IMOV);

		/*
			Keep a list of the moves inserted so we can replace the destinations by the
			sources at the end of this stage.
		*/
		AppendToList(&psContext->sOtherHalfMoveList, &psMOVInst->sAvailableListEntry);

		ExpandVectorDestComponent(psState, 
								  psContext, 
								  &sScalarDest, 
								  psVecDest, 
								  uCompLoadStart + 1 /* uCompIdx */,
								  uNumComponents);
		SetDestFromArg(psState, psMOVInst, 0 /* uDestIdx */, &sScalarDest);

		ExpandVectorDestComponent(psState, 
								  psContext, 
								  &sScalarSrc, 
								  psVecOldDest, 
								  uCompLoadStart + 1 /* uCompIdx */,
								  uNumComponents);
		SetSrcFromArg(psState, psMOVInst, 0 /* uSrcIdx */, &sScalarSrc);

		/*
			Insert the MOV instruction immediately after the current instruction.
		*/
		InsertInstAfter(psState, psInst->psBlock, psMOVInst, psInst);
	}
}

static
IMG_VOID ExpandDSXDSY(PINTERMEDIATE_STATE	psState,
					  PINST					psInst)
/*****************************************************************************
 FUNCTION	: ExpandDSXDSY
    
 PURPOSE	: Expand a DSX/DSY instruction to the two source form.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to expand.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IOPCODE		eNewOpcode;

	/*
		Switch to the two argument form of the instruction.
	*/
	if (psInst->eOpcode == IVDSX)
	{
		eNewOpcode = IVDSX_EXP;
	}
	else
	{
		eNewOpcode = IVDSY_EXP;
	}
	ModifyOpcode(psState, psInst, eNewOpcode);
	CopyVectorSource(psState, psInst, 1 /* uDestArgIdx */, psInst, 0 /* uSrcArgIdx */);
}

static
IMG_VOID ExpandDELTAInstruction(PINTERMEDIATE_STATE psState, PLOWER_VECTOR_REGISTERS_CONTEXT psContext, PINST psDeltaInst)
/*****************************************************************************
 FUNCTION	: ExpandDELTAInstruction
    
 PURPOSE	: Expand a delta function choosing between vector registers.

 PARAMETERS	: psState			- Compiler state.
			  psContext			- Pass state.
			  psDeltaInst		- Instruction to expand.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uVecOff;
	IMG_UINT32	uNumComponents;
	PARG		psVecDest = &psDeltaInst->asDest[0];
	IMG_UINT32	uChanMask;
	IMG_UINT32	uChanCount;

	if (psVecDest->eFmt == UF_REGFORMAT_F32)
	{
		uNumComponents = SCALAR_REGISTERS_PER_F32_VECTOR;
		uChanMask = USC_X_CHAN_MASK;
		uChanCount = F32_CHANNELS_PER_SCALAR_REGISTER;
	}
	else
	{
		ASSERT(psVecDest->eFmt == UF_REGFORMAT_F16);
		uNumComponents = SCALAR_REGISTERS_PER_F16_VECTOR;
		uChanMask = USC_XY_CHAN_MASK;
		uChanCount = F16_CHANNELS_PER_SCALAR_REGISTER;
	}

	for (uVecOff = 0; uVecOff < uNumComponents; uVecOff++)
	{
		PINST		psScalarDeltaInst;
		ARG			sScalarDest;
		IMG_UINT32	uArg;
		IMG_UINT32	uLiveChansInComponent;

		uLiveChansInComponent = psDeltaInst->auLiveChansInDest[0] & (uChanMask << (uVecOff * uChanCount));
		if (uLiveChansInComponent == 0)
		{
			continue;
		}

		psScalarDeltaInst = AllocateInst(psState, psDeltaInst);
		SetOpcode(psState, psScalarDeltaInst, IDELTA);
		SetArgumentCount(psState, psScalarDeltaInst, psDeltaInst->uArgumentCount);

		psScalarDeltaInst->auLiveChansInDest[0] = 
			ChanMaskToByteMask(psState, psDeltaInst->auLiveChansInDest[0], uVecOff, psVecDest->eFmt);

		/*
			Set the destination as the scalar register corresponding to this part of the destination vector.
		*/
		ExpandVectorDestComponent(psState, 
								  psContext, 
								  &sScalarDest, 
								  psVecDest, 
								  uVecOff /* uCompIdx */,
								  uNumComponents);
		SetDestFromArg(psState, psScalarDeltaInst, 0 /* uDestIdx */, &sScalarDest);

		for (uArg = 0; uArg < psDeltaInst->uArgumentCount; uArg++)
		{
			PARG	psVecSrc = &psDeltaInst->asArg[uArg];

			SetVectorArgumentComponent(psState,
									   &psContext->sRemap,
									   psScalarDeltaInst,
									   IMG_FALSE /* bDest */,
									   uArg,
									   psVecSrc,
									   uVecOff);
		}

		/*
			Insert before the unexpanded instruction.
		*/
		InsertInstBefore(psState, psDeltaInst->psBlock, psScalarDeltaInst, psDeltaInst);
	}

	/*
		Free the unexpanded instruction.
	*/
	RemoveInst(psState, psDeltaInst->psBlock, psDeltaInst);
	FreeInst(psState, psDeltaInst);
}

IMG_INTERNAL
IMG_INT32 ExpandVectorCallArgument(PINTERMEDIATE_STATE				psState,
								   PREGISTER_REMAP					psRemap,
								   PINST							psCallInst,
								   IMG_BOOL							bDest,
								   PARG								asArgs,
								   PFUNC_INOUT						psParam,
								   IMG_INT32						iInArg,
								   IMG_INT32						iOutArg)
/*****************************************************************************
 FUNCTION	: ExpandVectorCallArgument
    
 PURPOSE	: Expand a source/destination to a CALL instruction which is a
			  vector sized register.

 PARAMETERS	: psState			- Compiler state.
			  psRemap			- Table mapping vector register numbers to
								scalar register numbers.
			  psCallInst		- Instruction to expand.
			  bDest				- TRUE to expand a destination.
								  FALSE to expand a source.
			  asArgs			- Array of source/destinations to the call.
			  psParam			- Function input/output corresponding to
								the source or destination.
			  iInArg			- Original location of the argument to expand.
			  iOutArg			- Start of the locations to move the expanded
								arguments to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_INT32	iVec;
	ARG			sVecSource = asArgs[iInArg];

	/*
		Create a new argument for each scalar register corresponding to a used
		part of the old vector argument.
	*/
	for (iVec = g_auExpandedVectorLength[sVecSource.eFmt] - 1; iVec >= 0; iVec--)
	{
		if (ChanMaskToByteMask(psState, psParam->uChanMask, iVec, sVecSource.eFmt) != 0)
		{
			SetVectorArgumentComponent(psState,
									   psRemap,
									   psCallInst,
									   bDest,
									   iOutArg,
									   &sVecSource,
									   iVec);

			ASSERT(iOutArg >= 0);
			iOutArg--;
		}
	}
	return iOutArg;
}

IMG_INTERNAL
IMG_UINT32 ExpandFunctionVectorParameter(PINTERMEDIATE_STATE				psState,
									     PREGISTER_REMAP					psRemap,
									     PFUNC_INOUT						psOldParam,
									     PFUNC_INOUT						asNewParams,
									     IMG_UINT32							uNewArgCount)
/*****************************************************************************
 FUNCTION	: ExpandFunctionVectorParameter
    
 PURPOSE	: Expand a function's array of inputs or outputs when replacing 
			  a vector input/output by a set of scalar inputs/outputs.

 PARAMETERS	: psState			- Compiler state.
			  psRemap			- Table remapping from vector register numbers
								to scalar register numbers.
			  psOldParam		- Input or output before expansion.
			  asNewParams		- If non-NULL: entries are filled out with
								details of the expanded inputs or outputs.
			  uNewArgCount		- Count of expanded input/outputs generated
								for earlier inputs/outputs.

 RETURNS	: The new count of inputs or outputs.
*****************************************************************************/
{
	IMG_UINT32	uVectorBaseRegNum;
	IMG_UINT32	uComp;

	ASSERT(psOldParam->uType == USEASM_REGTYPE_TEMP);
	uVectorBaseRegNum = 
		GetRemapLocationCount(psState, psRemap, psOldParam->uNumber, g_auExpandedVectorLength[psOldParam->eFmt]);

	for (uComp = 0; uComp < g_auExpandedVectorLength[psOldParam->eFmt]; uComp++)
	{
		IMG_UINT32	uMask;

		uMask = ChanMaskToByteMask(psState, psOldParam->uChanMask, uComp, psOldParam->eFmt);
		if (uMask != 0)
		{
			if (asNewParams != NULL)
			{
				asNewParams[uNewArgCount].uType = psOldParam->uType;
				asNewParams[uNewArgCount].uNumber = uVectorBaseRegNum + uComp;
				asNewParams[uNewArgCount].eFmt = psOldParam->eFmt;
				asNewParams[uNewArgCount].uChanMask = uMask;
				asNewParams[uNewArgCount].bVector = IMG_FALSE;
			}
			uNewArgCount++;
		}
	}
	return uNewArgCount;
}

static
IMG_VOID ExpandScalarGPISources(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: ExpandScalarGPIDestinations
    
 PURPOSE	: Renumber references to GPIs in the sources of a scalar instruction
			  when their representations is expanded from 1 vector register to
			  4 scalar registers.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uSrc;

	for (uSrc = 0; uSrc < psInst->uArgumentCount; uSrc++)
	{
		PARG	psSrc;

		psSrc = &psInst->asArg[uSrc];
		if (psSrc->uType == USEASM_REGTYPE_FPINTERNAL)
		{
			SetSrc(psState, psInst, uSrc, psSrc->uType, psSrc->uNumber * SCALAR_REGISTERS_PER_F32_VECTOR, psSrc->eFmt);
		}
	}
}

static
IMG_VOID ExpandScalarGPIDestinations(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: ExpandScalarGPIDestinations
    
 PURPOSE	: Renumber references to GPIs in the destinations of a scalar instruction
			  when their representations is expanded from 1 vector register to
			  4 scalar registers.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uDest;

	for (uDest = 0; uDest < psInst->uDestCount; uDest++)
	{
		PARG	psDest;
		PARG	psOldDest;

		psDest = &psInst->asDest[uDest];
		psOldDest = psInst->apsOldDest[uDest];
		if (psDest->uType == USEASM_REGTYPE_FPINTERNAL)
		{
			SetDest(psState, psInst, uDest, psDest->uType, psDest->uNumber * SCALAR_REGISTERS_PER_F32_VECTOR, psDest->eFmt);
			if (psOldDest != NULL)
			{
				ASSERT(psOldDest->uType == USEASM_REGTYPE_FPINTERNAL);

				SetPartialDest(psState, 
							   psInst, 
							   uDest, 
							   psOldDest->uType, 
							   psOldDest->uNumber * SCALAR_REGISTERS_PER_F32_VECTOR, 
							   psOldDest->eFmt);
			}
		}
	}
}


static
IMG_VOID LowerVectorRegistersBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvContext)
/*****************************************************************************
 FUNCTION	: LowerVectorRegistersBP
    
 PURPOSE	: Replace vector registers by scalar registers in a single basic
			  block.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Basic block to process.
			  pvContext			- Global data for this stage.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST							psInst;
	PLOWER_VECTOR_REGISTERS_CONTEXT	psContext;
	PINST							psNextInst;

	psContext = (PLOWER_VECTOR_REGISTERS_CONTEXT)pvContext;

	for (psInst = psBlock->psBody; psInst != NULL; psInst = psNextInst)
	{
		IMG_UINT32	uArgIdx;
		IMG_UINT32	auChansLiveInVecSource[VECTOR_MAX_SOURCE_SLOT_COUNT];
		IMG_BOOL	bVectorDelta;
		
		psNextInst = psInst->psNext;

		/*
			Is this a delta instruction choosing between vector registers?
		*/
		bVectorDelta = IMG_FALSE;
		if (psInst->eOpcode == IDELTA && psInst->u.psDelta->bVector)
		{
			ExpandDELTAInstruction(psState, psContext, psInst);
			continue;
		}

		/*
			Leave instructions representing uses of a shader result by the driver epilog code until we
			expand the corresponding fixed register structure.
		*/
		if (psInst->eOpcode == IFEEDBACKDRIVEREPILOG && psInst->u.psFeedbackDriverEpilog->psFixedReg->bVector)
		{
			continue;
		}

		/*
			Skip calls. We expanded them when expanding information about their
			target function's inputs/outputs.
		*/
		if (psInst->eOpcode == ICALL)
		{
			continue;
		}

		/*
			For instructions already referencing a vector of scalar registers
			replace any unused components at the end of the vector by USC_REGTYPE_UNUSEDSOURCE.
		*/
		if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC)
		{
			for (uArgIdx = 0; uArgIdx < GetSwizzleSlotCount(psState, psInst); uArgIdx++)
			{
				PARG	psVecSource = &psInst->asArg[uArgIdx * SOURCE_ARGUMENTS_PER_VECTOR];

				if (psVecSource->uType == USC_REGTYPE_UNUSEDSOURCE)
				{
					DropUnusedVectorComponents(psState, psInst, uArgIdx);
				}
			}
		}

		/*
			Save information about the channels used from the sources before we start
			modifying the instruction.
		*/
		if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORDEST) != 0 ||
			(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) != 0)
		{
			for (uArgIdx = 0; uArgIdx < GetSwizzleSlotCount(psState, psInst); uArgIdx++)
			{
				GetLiveChansInSourceSlot(psState, 
										 psInst, 
										 uArgIdx, 
										 NULL /* puChansUsedPreSwizzle */, 
										 &auChansLiveInVecSource[uArgIdx]);
			}
		}

		/*
			Replace C10 vectors by pairs of scalar registers.
		*/
		if (LowerC10VectorRegisters(psState, psContext, psBlock, psInst))
		{
			continue;
		}

		/*
			Replace a vector destination by a vector of scalar registers.
		*/
		if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORDEST) != 0)
		{
			IMG_INT32	iDestIdx;
			IMG_UINT32	uOldDestCount;
			ARG			asSavedDest[VECTOR_MAX_DESTINATION_COUNT];
			ARG			asSavedOldDest[VECTOR_MAX_DESTINATION_COUNT];
			IMG_UINT32	auSavedLiveChansInDest[VECTOR_MAX_DESTINATION_COUNT];
			IMG_UINT32	auSavedDestMask[VECTOR_MAX_DESTINATION_COUNT];
			IMG_BOOL	abOldDestPresent[VECTOR_MAX_DESTINATION_COUNT];

			uOldDestCount = psInst->uDestCount;

			/*
				Save the details of the vector sized destinations.
			*/
			for (iDestIdx = uOldDestCount - 1; iDestIdx >=0 ; iDestIdx--)
			{
				if (psInst->asDest[iDestIdx].uType == USEASM_REGTYPE_PREDICATE)
				{
					continue;
				}

				asSavedDest[iDestIdx] = psInst->asDest[iDestIdx];
				if (psInst->apsOldDest[iDestIdx] != NULL)
				{
					asSavedOldDest[iDestIdx] = *psInst->apsOldDest[iDestIdx];
					abOldDestPresent[iDestIdx] = IMG_TRUE;
					SetPartiallyWrittenDest(psState, psInst, iDestIdx, NULL /* psPartialDest */);
				}
				else
				{
					abOldDestPresent[iDestIdx] = IMG_FALSE;
				}
				auSavedLiveChansInDest[iDestIdx] = psInst->auLiveChansInDest[iDestIdx];
				auSavedDestMask[iDestIdx] = psInst->auDestMask[iDestIdx];
			}

			/*
				Replace the destinations by vectors of scalar registers.
			*/
			if (psInst->eOpcode == IVTEST)
			{
				if (uOldDestCount != VTEST_PREDICATE_ONLY_DEST_COUNT)
				{
					ASSERT(uOldDestCount == VTEST_MAXIMUM_DEST_COUNT);

					/*
						Expand the TEST instruction's unified store destination.
					*/
					LowerVectorDestination(psState, 
										   psContext,
										   psInst,
										   1 /* uBaseDestIdx */, 
										   IMG_TRUE /* bLastDest */,
										   VTEST_UNIFIEDSTORE_DESTIDX,
										   &asSavedDest[VTEST_UNIFIEDSTORE_DESTIDX], 
										   &asSavedOldDest[VTEST_UNIFIEDSTORE_DESTIDX],
										   abOldDestPresent[VTEST_UNIFIEDSTORE_DESTIDX],
										   auSavedDestMask[VTEST_UNIFIEDSTORE_DESTIDX], 
										   auSavedLiveChansInDest[VTEST_UNIFIEDSTORE_DESTIDX]);
				}

				/*
					Leave the TEST instruction's predicate destination alone.
				*/
			}
			else
			{
				for (iDestIdx = uOldDestCount - 1; iDestIdx >= 0; iDestIdx--)
				{
					IMG_BOOL	bLastDest;

					bLastDest = IMG_FALSE;
					if ((IMG_UINT32)iDestIdx == (uOldDestCount - 1))
					{
						bLastDest = IMG_TRUE;
					}

					LowerVectorDestination(psState, 
										   psContext,
										   psInst,
										   iDestIdx * 4, 
										   bLastDest,
										   iDestIdx,
										   &asSavedDest[iDestIdx], 
										   &asSavedOldDest[iDestIdx],
										   abOldDestPresent[iDestIdx],
										   auSavedDestMask[iDestIdx], 
										   auSavedLiveChansInDest[iDestIdx]);
				}
			}
		}
		else if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_DEPENDENTTEXTURESAMPLE)
		{
			PSAMPLE_RESULT_CHUNK	psChunk;

			ASSERT(psInst->u.psSmp->uPlaneCount == 1);
			psChunk = &psInst->u.psSmp->asPlanes[psInst->u.psSmp->uFirstPlane];

			if (psChunk->bVector)
			{
				ARG			sVectorDest, sVectorOldDest;
				IMG_BOOL	bOldDestPresent;
				IMG_UINT32	uNumComponents;

				ASSERT(psChunk->uSizeInRegs == 1);
				ASSERT(psInst->uDestCount == 1);
				ASSERT((psInst->auLiveChansInDest[0] & (~psInst->auDestMask[0])) == 0);

				sVectorDest = psInst->asDest[0];
				if (psInst->apsOldDest[0] != NULL)
				{
					sVectorOldDest = *psInst->apsOldDest[0];
					bOldDestPresent = IMG_TRUE;
					SetPartiallyWrittenDest(psState, psInst, 0 /* uDestIdx */, NULL /* psPartialDest */);
				}
				else
				{
					bOldDestPresent = IMG_FALSE;
				}

				if (sVectorDest.eFmt == UF_REGFORMAT_F32)
				{
					uNumComponents = SCALAR_REGISTERS_PER_F32_VECTOR;
				}
				else
				{
					ASSERT(sVectorDest.eFmt == UF_REGFORMAT_F16);
					uNumComponents = SCALAR_REGISTERS_PER_F16_VECTOR;
				}
				ASSERT(uNumComponents >= psChunk->uSizeInDwords);

				ExpandVectorDest(psState,
								 psContext,
								 psInst,
								 psInst->auDestMask[0],
								 psInst->auLiveChansInDest[0],
								 0 /* uBaseDestIdx */,
								 IMG_TRUE /* bLastDest */,
								 0 /* uCompStart */,
								 psChunk->uSizeInDwords,
								 uNumComponents,
								 &sVectorDest,
								 &sVectorOldDest,
								 bOldDestPresent);
			}
		}
		else
		{
			ExpandScalarGPIDestinations(psState, psInst);
		}

		if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC)
		{
			for (uArgIdx = 0; uArgIdx < GetSwizzleSlotCount(psState, psInst); uArgIdx++)
			{
				IMG_UINT32	uVecSource = uArgIdx * SOURCE_ARGUMENTS_PER_VECTOR;
				PARG		psVecSource = &psInst->asArg[uVecSource];
				IMG_UINT32	uChansLiveInVecSource = auChansLiveInVecSource[uArgIdx];
				IMG_UINT32	uVecLength;
				IMG_UINT32	uComponentOffset;
				IMG_UINT32	uVecStartIdx = uArgIdx * SOURCE_ARGUMENTS_PER_VECTOR + 1;

				/*
					Replace accessing the Z or W channels from a vector register by 
					the X or Y channels from the second half of the set of scalar
					registers.
				*/
				uVecLength = 4;
				uComponentOffset = 0;
				if (GetBit(psInst->u.psVec->auSelectUpperHalf, uArgIdx))
				{
					ASSERT(psVecSource->uType != USEASM_REGTYPE_FPINTERNAL);
					ASSERT(psVecSource->eFmt == UF_REGFORMAT_F32);
					ASSERT((uChansLiveInVecSource & USC_XY_CHAN_MASK) == 0);

					uVecLength = 2;
					uComponentOffset = 2;
					SetBit(psInst->u.psVec->auSelectUpperHalf, uArgIdx, 0);
				}

				/*
					Check for the special case where the unified store source to a dual-issue
					instruction uses 32-bit units but only supports an X (or X/Y for F16) swizzle.
				*/
				if (
						psInst->eOpcode == IVDUAL && 
						uArgIdx == VDUAL_SRCSLOT_UNIFIEDSTORE && 
						!IsVDUALUnifiedStoreVectorSource(psState, psInst) &&
						psVecSource->uType != USEASM_REGTYPE_FPINTERNAL &&
						psVecSource->uType != USEASM_REGTYPE_FPCONSTANT
					)
				{
					USEASM_SWIZZLE_SEL	eUSSel, eNewUSSel;

					ASSERT(g_auSetBitCount[uChansLiveInVecSource] <= 1);

					/*
						Get the channel swizzled into the X channel.
					*/
					eUSSel = GetChan(psInst->u.psVec->auSwizzle[uArgIdx], USC_X_CHAN);
					if (!g_asSwizzleSel[eUSSel].bIsConstant)
					{
						UF_REGFORMAT	eSrcFmt = psInst->u.psVec->aeSrcFmt[uArgIdx];

						/*
							Shuffle the vector down so the selected channel is in the X
							channel position.
						*/
						if (eSrcFmt == UF_REGFORMAT_F32)
						{
							uComponentOffset += g_asSwizzleSel[eUSSel].uSrcChan;
							eNewUSSel = USC_X_CHAN;
						}
						else
						{
							IMG_UINT32	uChanOffset;

							ASSERT(eSrcFmt == UF_REGFORMAT_F16);

							uComponentOffset += g_asSwizzleSel[eUSSel].uSrcChan / F16_CHANNELS_PER_SCALAR_REGISTER;
							uChanOffset = g_asSwizzleSel[eUSSel].uSrcChan % F16_CHANNELS_PER_SCALAR_REGISTER;
							eNewUSSel = g_aeChanToSwizzleSel[uChanOffset];
						}
						uVecLength = 1;
						psInst->u.psVec->auSwizzle[uArgIdx] = 
							SetChan(psInst->u.psVec->auSwizzle[uArgIdx], USC_X_CHAN, eNewUSSel);
					}
				}
				
				if (psVecSource->uType == USC_REGTYPE_UNUSEDSOURCE)
				{
					/*	
						Shuffle the scalar sources left.
					*/
					if (uComponentOffset > 0)
					{
						IMG_UINT32	uChan;

						for (uChan = 0; uChan < uVecLength; uChan++)
						{
							MoveSrc(psState, psInst, uVecStartIdx + uChan, psInst, uVecStartIdx + uComponentOffset + uChan);
						}
						for (; uChan < MAX_SCALAR_REGISTERS_PER_VECTOR; uChan++)
						{
							SetSrcUnused(psState, psInst, uVecStartIdx + uChan);
						}
					}
				}
				else
				{
					/*
						Replace the vector source by a set of scalar registers.
					*/
					ExpandVectorSource(psState, 
									   psContext,
									   psInst, 
									   psVecSource, 
									   uVecStartIdx, 
									   uComponentOffset,
									   uVecLength,
									   uChansLiveInVecSource >> uComponentOffset);
				}

				/*
					Clear the now unreferenced vector source.
				*/
				SetSrcUnused(psState, psInst, uVecSource);

				/*
					Unused source type cannot be encoded so try using GPI instead
				*/
				if (uChansLiveInVecSource == 0)
				{
					FixDroppedUnusedVectorComponents(psState, psInst, uArgIdx);
				}
			}
		}
		else if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_DEPENDENTTEXTURESAMPLE)
		{
			ARG			sCoordArg;
			IMG_UINT32	uDim = psInst->u.psSmp->uDimensionality;
			IMG_UINT32	uDimMask = (1 << uDim) - 1;

			/*
				Expand the texture coordinate argument.
			*/
			sCoordArg = psInst->asArg[SMP_COORD_ARG_START];
			if (sCoordArg.eFmt == UF_REGFORMAT_F16 || sCoordArg.eFmt == UF_REGFORMAT_F32)
			{
				IMG_UINT32	uComponentOffset = USC_X_CHAN;

				ASSERT(psInst->u.psSmp->uCoordSize == 1);

				/*
					Update the coordinate size to the number of 32-bit register
					needed for the coordinate data.
				*/
				if (sCoordArg.eFmt == UF_REGFORMAT_F16)
				{
					ASSERT(!psInst->u.psSmp->bCoordSelectUpper);

					psInst->u.psSmp->uCoordSize = (uDim + 1) / 2;
					if ((uDim % 2) == 0)
					{
						psInst->u.psSmp->uUsedCoordChanMask = USC_ALL_CHAN_MASK;
					}
					else
					{
						/*
							For F16 coordinates if the number of dimensions is
							odd then only the first half of the last register
							is used.
						*/
						psInst->u.psSmp->uUsedCoordChanMask = USC_XY_CHAN_MASK;
					}	
				}
				else
				{
					psInst->u.psSmp->uCoordSize = uDim;
					psInst->u.psSmp->uUsedCoordChanMask = USC_ALL_CHAN_MASK;
					if (psInst->u.psSmp->bCoordSelectUpper)
					{
						ASSERT(uDim <= 2);
						uComponentOffset = USC_Z_CHAN;
						psInst->u.psSmp->bCoordSelectUpper = IMG_FALSE;
					}
				}

				ExpandVectorSource(psState, 
								   psContext,
								   psInst, 
								   &sCoordArg, 
								   SMP_COORD_ARG_START,
								   uComponentOffset,
								   uDim /* uVecLength */,
								   uDimMask);
			}

			if	(
					 #if defined(OUTPUT_USPBIN)
					(psInst->eOpcode == ISMPBIAS_USP) || 
					(psInst->eOpcode == ISMPREPLACE_USP) ||
					#endif /* defined(OUTPUT_USPBIN) */
					(psInst->eOpcode == ISMPBIAS) ||
					(psInst->eOpcode == ISMPREPLACE)
				)
			{
				ARG	sLODArg = psInst->asArg[SMP_LOD_ARG_START];

				/*
					Expand the LOD argument.
				*/
				if (sLODArg.uType != USEASM_REGTYPE_IMMEDIATE)
				{
					ExpandVectorSource(psState, 
									   psContext,
									   psInst, 
									   &sLODArg,
									   SMP_LOD_ARG_START,
									   0 /* uComponentOffset */,
									   1 /* uVecLength */,
									   USC_X_CHAN_MASK);
				}
			}
			
			/*
				Expand the gradient sources.
			*/
			if (psInst->u.psSmp->uGradSize > 0)
			{
				IMG_UINT32	uGradIdx;

				if (uDim == 3)
				{
					ARG			sXGradArg = psInst->asArg[SMP_GRAD_ARG_START + 0];
					ARG			sYGradArg = psInst->asArg[SMP_GRAD_ARG_START + 1];
					IMG_UINT32	uYVecStart;
					IMG_UINT32	uXVecLength;
					IMG_UINT32	uYVecLength;

					ASSERT(psInst->u.psSmp->uGradSize == 2);

					/*
						Expand the two vector sources representing the gradient data. As
						scalar registers the gradient layout is

							dUdX dVdX dSdX __ dUdY dVdY dSdY
					*/
					ASSERT(sXGradArg.eFmt == sYGradArg.eFmt);
					if (sXGradArg.eFmt == UF_REGFORMAT_F32)
					{
						psInst->u.psSmp->uGradSize = 7;
						uXVecLength = 4;
						uYVecLength = 3;
						uYVecStart = 4;
					}
					else
					{
						ASSERT(sXGradArg.eFmt == UF_REGFORMAT_F16);
						psInst->u.psSmp->uGradSize = 4;
						uXVecLength = 2;
						uYVecLength = 2;
						uYVecStart = 2;
					}

					for (uGradIdx = 0; uGradIdx < psInst->u.psSmp->uGradSize; uGradIdx++)
					{
						if (sXGradArg.eFmt == UF_REGFORMAT_F16 && (uGradIdx == 1 || uGradIdx == 3))
						{
							psInst->u.psSmp->auUsedGradChanMask[uGradIdx] = USC_XY_CHAN_MASK;
						}
						else if (sXGradArg.eFmt == UF_REGFORMAT_F32 && uGradIdx == 3)
						{
							psInst->u.psSmp->auUsedGradChanMask[uGradIdx] = 0;
						}
						else
						{
							psInst->u.psSmp->auUsedGradChanMask[uGradIdx] = USC_ALL_CHAN_MASK;
						}
					}

					ExpandVectorSource(psState, 
									   psContext, 
									   psInst, 
									   &sYGradArg, 
									   SMP_GRAD_ARG_START + uYVecStart /* uVecStartIdx */, 
									   0 /* uComponentOffset */,
									   uYVecLength /* uVecLength */,
									   uDimMask);
					ExpandVectorSource(psState, 
									   psContext, 
									   psInst, 
									   &sXGradArg, 
									   SMP_GRAD_ARG_START + 0 /* uVecStartIdx */, 
									   0 /* uComponentOffset */,
									   uXVecLength /* uVecLength */,
									   uDimMask);
				}
				else
				{
					ARG		sGradArg;
					IMG_UINT32	uGradVecLen;

					ASSERT(psInst->u.psSmp->uGradSize == 1);
					sGradArg = psInst->asArg[SMP_GRAD_ARG_START];

					uGradVecLen = uDim * 2;

					if (sGradArg.eFmt == UF_REGFORMAT_F32)
					{
						psInst->u.psSmp->uGradSize = uGradVecLen;
					}
					else
					{
						ASSERT(sGradArg.eFmt == UF_REGFORMAT_F16);
						psInst->u.psSmp->uGradSize = uGradVecLen / 2;
					}

					ExpandVectorSource(psState, 
									   psContext, 
									   psInst, 
									   &sGradArg, 
									   SMP_GRAD_ARG_START + 0 /* uVecStartIdx */,
									   0 /* uComponentOffset */,
									   uGradVecLen /* uVecLength */,
									   (1 << uGradVecLen) - 1);

					for (uGradIdx = 0; uGradIdx < psInst->u.psSmp->uGradSize; uGradIdx++)
					{
						psInst->u.psSmp->auUsedGradChanMask[uGradIdx] = USC_ALL_CHAN_MASK;
					}
				}
			}
		}
		else
		{
			ExpandScalarGPISources(psState, psInst);
		}

		/*
			Replace instructions to select a component from a vector.
		*/
		if (psInst->eOpcode == IUNPCKVEC)
		{
			ReplaceVectorUnpack(psState, psInst);
		}

		/*
			Expand DSX/DSY from one source to two sources.
		*/
		if (psInst->eOpcode == IVDSX || psInst->eOpcode == IVDSY)
		{
			ExpandDSXDSY(psState, psInst);
		}
	}
}

static
IMG_VOID UpdateFeedbackDriverEpilog(PINTERMEDIATE_STATE	psState,
									PFIXED_REG_DATA		psOldFixedReg,
									IMG_UINT32			uOldRegIdx,
									PFIXED_REG_DATA		psNewFixedReg,
									IMG_UINT32			uNewBaseRegIdx)
/*****************************************************************************
 FUNCTION	: UpdateFeedbackDriverEpilog
    
 PURPOSE	: Update the instructions representing uses of a shader result
			  between the pre- and post-ISP feedback phases when replacing
			  vector registers by scalars.

 PARAMETERS	: psState			- Compiler state.
			  psContext			- Module state.
			  psOldFixedReg		- Vector sized shader result.
			  uOldRegIdx
			  psNewFixedReg		- Start of the corresponding scalar sized
			  uNewBaseRegIdx	shader results.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;
	PUSC_LIST_ENTRY	psNextListEntry;
	UF_REGFORMAT	eFmt = psOldFixedReg->aeVRegFmt[uOldRegIdx];
	PUSEDEF_CHAIN	psUseDefChain = psOldFixedReg->asVRegUseDef[uOldRegIdx].psUseDefChain;

	for (psListEntry = psUseDefChain->sList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PUSEDEF	psUse;

		psNextListEntry = psListEntry->psNext;
		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		if (psUse->eType == USE_TYPE_SRC)
		{
			PINST	psUseInst = psUse->u.psInst;

			if (psUseInst->eOpcode == IFEEDBACKDRIVEREPILOG)
			{
				PFEEDBACKDRIVEREPILOG_PARAMS	psParams = psUseInst->u.psFeedbackDriverEpilog;
				IMG_UINT32						uRegOff;
					
				ASSERT(psUse->uLocation == 0);
				ASSERT(psParams->psFixedReg == psOldFixedReg);
				ASSERT(psParams->uFixedRegOffset == uOldRegIdx);

				if (psParams->bPartial)
				{
					IMG_UINT32		uChanCount;

					if (eFmt == UF_REGFORMAT_F16)
					{
						uChanCount = F16_CHANNELS_PER_SCALAR_REGISTER;
					}
					else
					{
						ASSERT(eFmt == UF_REGFORMAT_F32);
						uChanCount = F32_CHANNELS_PER_SCALAR_REGISTER;
					}
					uRegOff = psParams->uFixedRegChan / uChanCount;
					psParams->uFixedRegChan %= uChanCount;
				}
				else
				{
					ASSERT(psOldFixedReg->auMask != NULL);
					ASSERT(psOldFixedReg->auMask[uOldRegIdx] == USC_X_CHAN_MASK);
					uRegOff = 0;
				}

				psParams->psFixedReg = psNewFixedReg;
				psParams->uFixedRegOffset = uNewBaseRegIdx + uRegOff;

				SetSrc(psState, 
					   psUseInst, 
					   0 /* uSrcIdx */, 
					   USEASM_REGTYPE_TEMP, 
					   psNewFixedReg->auVRegNum[uNewBaseRegIdx + uRegOff], 
					   eFmt);
			}
		}
	}
}

static
PFIXED_REG_DATA ExpandVectorFixedReg(PINTERMEDIATE_STATE				psState,
									 PLOWER_VECTOR_REGISTERS_CONTEXT	psContext,
									 PFIXED_REG_DATA					psOldFixedReg)
/*****************************************************************************
 FUNCTION	: ExpandVectorFixedReg
    
 PURPOSE	: Replace a fixed register of vector registers by a fixed
			  register of scalar registers.

 PARAMETERS	: psState			- Compiler state.
			  psContext			- Module state.
			  psOldFixedReg		- Fixed register to expand.

 RETURNS	: The new fixed register.
*****************************************************************************/
{
	IMG_UINT32		uRegIdx;
	IMG_PUINT32		auOldVRegNums;
	IMG_UINT32		uNewRegIdx;
	IMG_UINT32		uNewConsecutiveRegsCount;
	IMG_PUINT32		puOldUsedChans;
	PFIXED_REG_DATA	psNewFixedReg;
	IMG_BOOL		bPartialMasks;

	/*
		Save the old vector-sized register numbers.
	*/
	auOldVRegNums = psOldFixedReg->auVRegNum;

	/*
		Save the old masks of channels used from each input.
	*/
	puOldUsedChans = psOldFixedReg->puUsedChans;

	/*
		Calculate the number of scalar registers corresponding to the old
		vector registers.
	*/
	uNewConsecutiveRegsCount = 0;
	bPartialMasks = IMG_FALSE;
	for (uRegIdx = 0; uRegIdx < psOldFixedReg->uConsecutiveRegsCount; uRegIdx++)
	{
		IMG_UINT32		uComp;
		IMG_UINT32		uMask;
		UF_REGFORMAT	eFmt = psOldFixedReg->aeVRegFmt[uRegIdx];
	
		if (psOldFixedReg->auMask != NULL)
		{
			uMask = psOldFixedReg->auMask[uRegIdx];
		}
		else
		{
			uMask = USC_ALL_CHAN_MASK;
		}

		for (uComp = 0; uComp < g_auExpandedVectorLength[eFmt]; uComp++)
		{
			IMG_UINT32	uCompMask;

			uCompMask = ChanMaskToByteMask(psState, uMask, uComp, eFmt);
			if (uCompMask != 0)
			{
				if (uCompMask != USC_ALL_CHAN_MASK)
				{
					bPartialMasks = IMG_TRUE;
				}
				uNewConsecutiveRegsCount++;
			}
		}
	}

	/*
		Create a new fixed register using scalar width registers.
	*/
	psNewFixedReg = AddFixedReg(psState,
								psOldFixedReg->bPrimary,
								psOldFixedReg->bLiveAtShaderEnd,
								psOldFixedReg->sPReg.uType,
								psOldFixedReg->sPReg.uNumber,
								uNewConsecutiveRegsCount);
	psNewFixedReg->uRegArrayIdx = psOldFixedReg->uRegArrayIdx;
	psNewFixedReg->uRegArrayOffset = psOldFixedReg->uRegArrayOffset;

	#if defined(OUTPUT_USPBIN)
	/*
		Copy information about alternate physical registers.
	*/
	if (psOldFixedReg->uNumAltPRegs > 0)
	{
		psNewFixedReg->uNumAltPRegs = psOldFixedReg->uNumAltPRegs;
		psNewFixedReg->asAltPRegs = psOldFixedReg->asAltPRegs;

		psOldFixedReg->uNumAltPRegs = 0;
		psOldFixedReg->asAltPRegs = NULL;
	}
	#endif /* defined(OUTPUT_USPBIN) */
	
	/*
		Allocate memory for the mask of channels used from a shader input.
	*/
	if (psOldFixedReg->puUsedChans != NULL)
	{
		psNewFixedReg->puUsedChans = 
			UscAlloc(psState, UINTS_TO_SPAN_BITS(uNewConsecutiveRegsCount * CHANS_PER_REGISTER) * sizeof(IMG_UINT32));
	}

	/*
		Allocate memory for the mask of channels used from a shader output.
	*/
	if (bPartialMasks)
	{
		psNewFixedReg->auMask = 
			UscAlloc(psState, UINTS_TO_SPAN_BITS(uNewConsecutiveRegsCount * CHANS_PER_REGISTER) * sizeof(IMG_UINT32));
	}

	if (psOldFixedReg->aeUsedForFeedback != NULL)
	{
		psNewFixedReg->aeUsedForFeedback =
			UscAlloc(psState, sizeof(psNewFixedReg->aeUsedForFeedback[0]) * uNewConsecutiveRegsCount);
	}

	/*
		For each vector-sized register replace it by four scalar sized registers.
	*/
	uNewRegIdx = 0;
	psNewFixedReg->uVRegType = USEASM_REGTYPE_TEMP;
	for (uRegIdx = 0; uRegIdx < psOldFixedReg->uConsecutiveRegsCount; uRegIdx++)
	{
		IMG_UINT32				uOldRegNum;
		IMG_UINT32				uNewRegNum;
		IMG_UINT32				uNewBaseRegIdx;
		IMG_UINT32				uMask;
		IMG_UINT32				uOldUsedMask;
		UF_REGFORMAT			eOldFmt;
		IMG_UINT32				uComp;
		IMG_UINT32				uOldRegLength;

		eOldFmt = psOldFixedReg->aeVRegFmt[uRegIdx];
		uOldRegLength = g_auExpandedVectorLength[eOldFmt];
		uOldRegNum = auOldVRegNums[uRegIdx];
		uNewRegNum = GetRemapLocationCount(psState, &psContext->sRemap, uOldRegNum, uOldRegLength);

		if (psOldFixedReg->auMask != NULL)
		{
			uMask = psOldFixedReg->auMask[uRegIdx];
		}
		else
		{
			uMask = USC_ALL_CHAN_MASK;
		}

		if (psOldFixedReg->puUsedChans != NULL)
		{
			uOldUsedMask = GetRegMask(puOldUsedChans, uRegIdx);
		}
		else
		{
			uOldUsedMask = 0;
		}

		uNewBaseRegIdx = uRegIdx;
		for (uComp = 0; uComp < uOldRegLength; uComp++)
		{
			IMG_UINT32	uCompMask;

			uCompMask = ChanMaskToByteMask(psState, uMask, uComp, eOldFmt);
			if (uCompMask != 0)
			{
				psNewFixedReg->auVRegNum[uNewRegIdx] = uNewRegNum + uComp;
				if (psNewFixedReg->puUsedChans != NULL)
				{
					SetRegMask(psNewFixedReg->puUsedChans, 
							   uNewRegIdx, 
							   ChanMaskToByteMask(psState, uOldUsedMask, uComp, eOldFmt));
				}
				psNewFixedReg->aeVRegFmt[uNewRegIdx] = eOldFmt;

				if (psNewFixedReg->auMask != NULL)
				{
					psNewFixedReg->auMask[uNewRegIdx] = uCompMask;
				}
				else
				{
					ASSERT(uCompMask == USC_ALL_CHAN_MASK);
				}

				if (psOldFixedReg->aeUsedForFeedback != NULL)
				{
					psNewFixedReg->aeUsedForFeedback[uNewRegIdx] = psOldFixedReg->aeUsedForFeedback[uRegIdx];
				}

				uNewRegIdx++;
			}
		}

		if (psOldFixedReg->aeUsedForFeedback != NULL && psOldFixedReg->aeUsedForFeedback[uRegIdx] != FEEDBACK_USE_TYPE_NONE)
		{
			UpdateFeedbackDriverEpilog(psState, psOldFixedReg, uRegIdx, psNewFixedReg, uNewBaseRegIdx);
		}
	}
	ASSERT(uNewRegIdx == uNewConsecutiveRegsCount);

	/*
		Add new DEFs/USEs for the scalar sized registers.
	*/
	if (psNewFixedReg->uRegArrayIdx == USC_UNDEF)
	{
		for (uRegIdx = 0; uRegIdx < psNewFixedReg->uConsecutiveRegsCount; uRegIdx++)
		{
			if (psNewFixedReg->bLiveAtShaderEnd)
			{
				UseDefAddFixedRegUse(psState, psNewFixedReg, uRegIdx);
			}
			else
			{
				UseDefAddFixedRegDef(psState, psNewFixedReg, uRegIdx);
			}
		}
	}

	/*
		Drop the vector sized fixed registers.
	*/
	ReleaseFixedReg(psState, psOldFixedReg);

	return psNewFixedReg;
}

static
IMG_VOID LowerSAProgVectors(PINTERMEDIATE_STATE	psState, PLOWER_VECTOR_REGISTERS_CONTEXT psContext)
{
	/*
		Expand vector registers which are results calculated in the secondary update
		program.
	*/
	if (psState->sSAProg.uNumResults > 0)
	{
		PUSC_LIST_ENTRY		psListEntry;
		PUSC_LIST_ENTRY		psPrevListEntry;
		PREGISTER_LIVESET	psSAOutputs;

		psSAOutputs = &psState->psSecAttrProg->sCfg.psExit->sRegistersLiveOut;
		for (psListEntry = psState->sSAProg.sResultsList.psTail; 
			 psListEntry != NULL; 
			 psListEntry = psPrevListEntry)
		{
			PSAPROG_RESULT	psOldResult = IMG_CONTAINING_RECORD(psListEntry, PSAPROG_RESULT, sListEntry);

			psPrevListEntry = psListEntry->psPrev;

			if (psOldResult->bVector || psOldResult->eFmt == UF_REGFORMAT_C10)
			{
				IMG_UINT32					uVectorLength;
				IMG_UINT32					uOldRegNum;
				IMG_UINT32					uOldMask;
				IMG_UINT32					uVecIdx;
				UF_REGFORMAT				eResultFmt = psOldResult->eFmt;
				PINREGISTER_CONST			apsDriverConst[MAX_SCALAR_REGISTERS_PER_VECTOR];
				SAPROG_RESULT_TYPE			eOldType;
				PCONSTANT_INREGISTER_RANGE	psDriverConstRange = NULL;
				PUSC_LIST_ENTRY				psRangeListInsertPoint = NULL;
				IMG_BOOL					bOldPartOfRange;
				IMG_UINT32					uBasePhysicalRegNum;
				IMG_UINT32					uDirectMappedChanMask = 0;
				IMG_UINT32					uNewBaseRegNum;

				/*
					Get the length of the vector of scalar registers equivalent to the
					vector sized register.
				*/
				uVectorLength = g_auExpandedVectorLength[eResultFmt];
				ASSERT(uVectorLength != USC_UNDEF);

				uOldRegNum = psOldResult->psFixedReg->auVRegNum[0];
				uNewBaseRegNum = GetRemapLocationCount(psState, &psContext->sRemap, uOldRegNum, uVectorLength);

				/*
					Get the mask of channels live in the vector sized register.
				*/
				uOldMask = GetRegisterLiveMask(psState,
											   psSAOutputs,
											   USEASM_REGTYPE_TEMP,
											   uOldRegNum,
											   0 /* uArrayOffset */);
				ASSERT(psOldResult->eType == SAPROG_RESULT_TYPE_DRIVERLOADED || 
					   psOldResult->eType == SAPROG_RESULT_TYPE_DIRECTMAPPED ||
					   uOldMask != 0);

				/*
					Clear the mask for the old (vector-sized) register in the set of
					registers live out of the secondary update program.
				*/
				SetRegisterLiveMask(psState,
									psSAOutputs,
									USEASM_REGTYPE_TEMP,
									uOldRegNum,
									0 /* uArrayOffset */,
									0 /* uMask */);

				/*
					Save any references from the old result to any driver loaded constants.
				*/
				bOldPartOfRange = psOldResult->bPartOfRange;
				eOldType = psOldResult->eType;
				uBasePhysicalRegNum = psOldResult->psFixedReg->sPReg.uNumber;
				if (eOldType == SAPROG_RESULT_TYPE_DRIVERLOADED)
				{
					memcpy(apsDriverConst, psOldResult->u1.sDriverConst.apsDriverConst, sizeof(apsDriverConst));
					psDriverConstRange = psOldResult->u1.sDriverConst.psDriverConstRange;
					psRangeListInsertPoint = psOldResult->sRangeListEntry.psNext;
				}
				else if (eOldType == SAPROG_RESULT_TYPE_DIRECTMAPPED)
				{
					uDirectMappedChanMask = psOldResult->u1.sDirectMapped.uChanMask;
				}

				/*
					Drop the result corresponding to the vector sized register.
				*/
				DropSAProgResult(psState, psOldResult);
				psOldResult = NULL;

				for (uVecIdx = 0; uVecIdx < uVectorLength; uVecIdx++)
				{
					IMG_UINT32		uNewMask;
					PSAPROG_RESULT	psNewResult;
					IMG_UINT32		uNewRegNum;

					uNewMask = ChanMaskToByteMask(psState, uOldMask, uVecIdx, eResultFmt);

					/*
						Skip if this dword part of the vector isn't referenced.
					*/
					if (eOldType == SAPROG_RESULT_TYPE_DRIVERLOADED)
					{
						if (apsDriverConst[uVecIdx] == NULL)
						{
							continue;
						}
					}
					else if (eOldType == SAPROG_RESULT_TYPE_DIRECTMAPPED)
					{
						if (ChanMaskToByteMask(psState, uDirectMappedChanMask, uVecIdx, eResultFmt) == 0)
						{
							continue;
						}
					}

					/*
						Get the number of the scalar register holding this part
						of the old vector register.
					*/
					if (eResultFmt == UF_REGFORMAT_C10)
					{
						if (uVecIdx == 0)
						{
							uNewRegNum = uOldRegNum;
						}
						else
						{
							ASSERT(uVecIdx == 1);
							uNewRegNum = uNewBaseRegNum;
						}
					}
					else
					{
						uNewRegNum = uNewBaseRegNum + uVecIdx;
					}
					ASSERT(uNewRegNum < psState->uNumRegisters);

					/*
						Create a new secondary update program result for this part
						of the vector.
					*/
					psNewResult = 
						BaseAddNewSAProgResult(psState,
											   eResultFmt,
											   IMG_FALSE /* bVector */,
											   0 /* uAlignment */,
											   1 /* uNumHwRegisters */,
											   uNewRegNum,
											   USC_ALL_CHAN_MASK,
											   eOldType,
											   bOldPartOfRange);

					if (eOldType == SAPROG_RESULT_TYPE_DRIVERLOADED)
					{
						PINREGISTER_CONST	psDriverConst;

						/*
							Copy any reference to a driver loaded secondary attribute.
						*/
						psDriverConst = apsDriverConst[uVecIdx];
						psNewResult->u1.sDriverConst.apsDriverConst[0] = apsDriverConst[uVecIdx];
						ASSERT(psDriverConst != NULL);

						/*
							Update the back pointer in the driver loaded secondary attribute.
						*/
						ASSERT(psDriverConst->psResult == NULL);
						psDriverConst->psResult = psNewResult;

						/*
							Insert into the list of results associated with a range at the same point
							as the removed result.
						*/
						psNewResult->u1.sDriverConst.psDriverConstRange = psDriverConstRange;
						if (psDriverConstRange != NULL)
						{
							InsertInList(&psDriverConstRange->sResultList, 
										 &psNewResult->sRangeListEntry, 
										 psRangeListInsertPoint);
						}
					}
					else if (eOldType == SAPROG_RESULT_TYPE_DIRECTMAPPED)
					{
						/*
							Update the fixed location of this secondary attribute.
						*/
						ModifyFixedRegPhysicalReg(&psState->sSAProg.sFixedRegList,
												  psNewResult->psFixedReg,
												  USEASM_REGTYPE_SECATTR,
												  uBasePhysicalRegNum + uVecIdx);
					}

					/*
						Flag this part of the vector as live out of the secondary
						update program.
					*/
					SetRegisterLiveMask(psState,
										psSAOutputs,
										USEASM_REGTYPE_TEMP,
										uNewRegNum,
										0 /* uArrayOffset */,
										uNewMask);
				}
			}
		}
	}
}

IMG_INTERNAL
IMG_VOID LowerVectorRegisters(PINTERMEDIATE_STATE	psState)
/*****************************************************************************
 FUNCTION	: LowerVectorRegisters
    
 PURPOSE	: Replace vector registers by scalar registers.

 PARAMETERS	: psState

 RETURNS	: Nothing.
*****************************************************************************/
{
	LOWER_VECTOR_REGISTERS_CONTEXT	sContext;
	PUSC_LIST_ENTRY					psListEntry;
	PUSC_LIST_ENTRY					psNextListEntry;

	/*
		Fix fixed point instruction which don't support writing to the destination
		alpha channel on cores with the vector instruction set.
	*/
	FixUnsupportedFixedPointInstruction(psState);

	/*
		Tell the USE-DEF code to stop tracking which registers are vector sized.
	*/
	psState->uFlags2 |= USC_FLAGS2_NO_VECTOR_REGS;

	/*
		Initialize the table mapping vector registers to the equivalent set of
		scalar registers.
	*/
	InitializeRemapTable(psState, &sContext.sRemap);
	InitializeList(&sContext.sOtherHalfMoveList);

	/*
		Replace vector registers by scalars in arrays of dynmically indexable data in registers.
	*/
	{
		IMG_UINT32 uArrayRegIdx;

		for (uArrayRegIdx = 0; uArrayRegIdx < psState->uNumVecArrayRegs; uArrayRegIdx++)
		{
			IMG_UINT32			uOldBaseReg;
			IMG_UINT32			uOldRegNum;
			IMG_UINT32			uRegIdx = 0;
			PUSC_VEC_ARRAY_REG	psArray = psState->apsVecArrayReg[uArrayRegIdx];
			IMG_BOOL			bResizeArray;
			IMG_UINT32			uScalarRegisterCount;
			IMG_BOOL			bWasVector = IMG_FALSE;

			switch (psArray->eArrayType)
			{
				case ARRAY_TYPE_DRIVER_LOADED_SECATTR:
				case ARRAY_TYPE_DIRECT_MAPPED_SECATTR:
				{
					UF_REGFORMAT	eArrayFormat;

					if (psArray->eArrayType == ARRAY_TYPE_DRIVER_LOADED_SECATTR)
					{
						eArrayFormat = psArray->u.psConstRange->eFormat;
					}
					else
					{
						eArrayFormat = psArray->u.eDirectMappedFormat;
					}

					if (eArrayFormat == UNIFLEX_CONST_FORMAT_F16)
					{
						bResizeArray = IMG_TRUE;
						uScalarRegisterCount = SCALAR_REGISTERS_PER_F16_VECTOR;
						bWasVector = IMG_TRUE;
					}
					else if (eArrayFormat == UNIFLEX_CONST_FORMAT_F32)
					{
						bResizeArray = IMG_TRUE;
						uScalarRegisterCount = SCALAR_REGISTERS_PER_F32_VECTOR;
						bWasVector = IMG_TRUE;
					}
					else if (eArrayFormat == UNIFLEX_CONST_FORMAT_C10)
					{
						bResizeArray = IMG_TRUE;
						uScalarRegisterCount = SCALAR_REGISTERS_PER_C10_VECTOR;
						bWasVector = IMG_FALSE;
					}
					else
					{
						bResizeArray = IMG_FALSE;
						uScalarRegisterCount = USC_UNDEF;
					}
					break;
				}
				case ARRAY_TYPE_VERTEX_SHADER_OUTPUT:
				{
					bResizeArray = IMG_FALSE;
					uScalarRegisterCount = USC_UNDEF;
					bWasVector = IMG_FALSE;
					break;
				}
				default:
				{
					bResizeArray = IMG_TRUE;
					uScalarRegisterCount = SCALAR_REGISTERS_PER_F32_VECTOR;
					bWasVector = IMG_TRUE;
					break;
				}
			}

			if (bResizeArray)
			{
				uOldBaseReg = psArray->uBaseReg;
				psArray->uBaseReg = GetNextRegisterCount(psState, psArray->uRegs * uScalarRegisterCount);
	
				for (uOldRegNum = uOldBaseReg; uOldRegNum < (uOldBaseReg + psArray->uRegs); uOldRegNum++, uRegIdx++)
				{
					sContext.sRemap.auRemap[uOldRegNum] = psArray->uBaseReg + (uRegIdx * uScalarRegisterCount);
				}
				
				psArray->uRegs *= uScalarRegisterCount;
				psArray->uChannelsPerDword = VECTOR_LENGTH;

				if (bWasVector && psArray->psUseDef != NULL)
				{
					ASSERT(psArray->psUseDef->eIsVector == USEDEF_ISVECTOR_YES);
					psArray->psUseDef->eIsVector = USEDEF_ISVECTOR_NO;
					RemoveFromList(&psState->sVectorTempList, &psArray->psUseDef->sVectorTempListEntry);
				}
			}
		}
	}

	/*
		Replace vector registers by scalars in function input and outputs.
	*/
	ExpandFunctions(psState, &sContext.sRemap, USC_ALL_CHAN_MASK /* uFirstC10PairChanMask */);

	/*
		Replace vector registers by scalars in all basic blocks.
	*/
	DoOnAllBasicBlocks(psState, ANY_ORDER, LowerVectorRegistersBP, IMG_TRUE /* CALLS */, (IMG_PVOID)&sContext);

	if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL)
	{
		PUSC_LIST_ENTRY		psInputListEntry;
		IMG_UINT32			uNewPACount;
		PPIXELSHADER_STATE	psPS = psState->sShader.psPS;
		PFIXED_REG_DATA		psNewColFixedReg;

		/*
			Replace vector registers by scalars in the colour output.
		*/
		if (psPS->psColFixedReg != NULL && psPS->psColFixedReg->bVector)
		{
			psNewColFixedReg = ExpandVectorFixedReg(psState, &sContext, psPS->psColFixedReg);
			if (psState->sShader.psPS->psFixedHwPARegReg == psPS->psColFixedReg)
			{
				psState->sShader.psPS->psFixedHwPARegReg = psNewColFixedReg;
			}
			psState->sShader.psPS->psColFixedReg = psNewColFixedReg;
		}

		/*
			Replace vector registers by scalars in the depth output.
		*/
		if (psPS->psDepthOutput != NULL)
		{
			ASSERT(psPS->psDepthOutput->bVector);
			psPS->psDepthOutput = ExpandVectorFixedReg(psState, &sContext, psPS->psDepthOutput);
		}

		/*
			Replace vector registers by scalars in the structures representing 
			pixel shader inputs.
		*/
		uNewPACount = 0;
		for (psInputListEntry = psPS->sPixelShaderInputs.psHead;
			 psInputListEntry != NULL;
			 psInputListEntry = psInputListEntry->psNext)
		{
			PPIXELSHADER_INPUT	psInput = IMG_CONTAINING_RECORD(psInputListEntry, PPIXELSHADER_INPUT, sListEntry);
			IMG_BOOL			bExpandInput;
	
			bExpandInput = IMG_FALSE;
			if (psInput->sLoad.uTexture == UNIFLEX_TEXTURE_NONE)
			{
				if (
						psInput->sLoad.eFormat == UNIFLEX_TEXLOAD_FORMAT_C10 ||
						psInput->sLoad.eFormat == UNIFLEX_TEXLOAD_FORMAT_F16 ||
						psInput->sLoad.eFormat == UNIFLEX_TEXLOAD_FORMAT_F32
					)
				{
					bExpandInput = IMG_TRUE;
				}
			}
			else
			{
				if ((psInput->uFlags & PIXELSHADER_INPUT_FLAG_NDR_IS_VECTOR) != 0)
				{
					bExpandInput = IMG_TRUE;
				}
			}
			if (bExpandInput)
			{
				IMG_UINT32			uNewNumAttributes;
				IMG_UINT32			uOldRegNum;
				IMG_UINT32			uRegIdx;
				PFIXED_REG_DATA		psFixedReg;
				IMG_UINT32			uOldUsedChans;
				UF_REGFORMAT		eRegFormat;
				IMG_UINT32			uNewRegNum;

				/*
					Get the number of scalar registers used for the result
					of this iteration.
				*/
				ASSERT(psInput->sLoad.uNumAttributes == 1);
				eRegFormat = psInput->eResultFormat;
				uNewNumAttributes = psInput->uAttributeSizeInDwords;

				psFixedReg = psInput->psFixedReg;
				uOldRegNum = psFixedReg->auVRegNum[0];
				uNewRegNum = GetRemapLocationCount(psState, &sContext.sRemap, uOldRegNum, uNewNumAttributes);

				/*
					Expand the array recording the channels live in the pixel shader input at the start of
					the program.
				*/
				uOldUsedChans = GetRegMask(psFixedReg->puUsedChans, 0 /* uReg */);
				UscFree(psState, psFixedReg->puUsedChans);
				psFixedReg->puUsedChans = 
					UscAlloc(psState, UINTS_TO_SPAN_BITS(uNewNumAttributes * CHANS_PER_REGISTER) * sizeof(IMG_UINT32));
				for (uRegIdx = 0; uRegIdx < uNewNumAttributes; uRegIdx++)
				{
					SetRegMask(psFixedReg->puUsedChans, 
							   uRegIdx, 
							   ChanMaskToByteMask(psState, uOldUsedChans, uRegIdx, eRegFormat));
				}

				/*
					Grow the array recording the intermediate registers representing this
					pixel shader input.
				*/
				if (psFixedReg->uRegArrayIdx == USC_UNDEF)
				{
					UseDefDropFixedRegDef(psState, psFixedReg, 0 /* uRegIdx */);
				}
				UscFree(psState, psFixedReg->auVRegNum);
				UscFree(psState, psFixedReg->aeVRegFmt);
				psFixedReg->bVector = IMG_FALSE;
				psFixedReg->auVRegNum = UscAlloc(psState, sizeof(psFixedReg->auVRegNum[0]) * uNewNumAttributes);
				psFixedReg->aeVRegFmt = UscAlloc(psState, sizeof(psFixedReg->aeVRegFmt[0]) * uNewNumAttributes);

				psFixedReg->asVRegUseDef = 
					UseDefResizeArray(psState, psFixedReg->asVRegUseDef, 1 /* uOldCount */, uNewNumAttributes);
				for (uRegIdx = 1; uRegIdx < uNewNumAttributes; uRegIdx++)
				{
					UseDefReset(&psFixedReg->asVRegUseDef[uRegIdx], DEF_TYPE_FIXEDREG, uRegIdx, psFixedReg);
				}

				psFixedReg->uConsecutiveRegsCount = uNewNumAttributes;
				if (psInput->sLoad.eFormat == UNIFLEX_TEXLOAD_FORMAT_C10)
				{
					ASSERT(uNewNumAttributes == 2);
					psFixedReg->auVRegNum[0] = uOldRegNum;
					psFixedReg->aeVRegFmt[0] = UF_REGFORMAT_C10;
					if (psFixedReg->uRegArrayIdx == USC_UNDEF)
					{
						UseDefAddFixedRegDef(psState, psFixedReg, 0 /* uRegIdx */);
					}
					psFixedReg->auVRegNum[1] = uNewRegNum;
					psFixedReg->aeVRegFmt[1] = UF_REGFORMAT_C10;
					if (psFixedReg->uRegArrayIdx == USC_UNDEF)
					{
						UseDefAddFixedRegDef(psState, psFixedReg, 1 /* uRegIdx */);
					}
				}
				else
				{
					for (uRegIdx = 0; uRegIdx < uNewNumAttributes; uRegIdx++)
					{
						psFixedReg->auVRegNum[uRegIdx] = uNewRegNum + uRegIdx;
						psFixedReg->aeVRegFmt[uRegIdx] = psInput->eResultFormat;
						if (psFixedReg->uRegArrayIdx == USC_UNDEF)
						{
							UseDefAddFixedRegDef(psState, psFixedReg, uRegIdx);
						}
					}
				}

				psInput->sLoad.uNumAttributes = uNewNumAttributes;
			}

			uNewPACount += psInput->sLoad.uNumAttributes;
		}
	}
	else if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_VERTEX)
	{
		PVERTEXSHADER_STATE	psVS = psState->sShader.psVS;

		/*
			Replace vector registers by scalars in the structures representing
			vertex shader inputs.
		*/
#ifdef NON_F32_VERTEX_INPUT
		/*
			We have a list of vertex input attribute fixed regs, not just one
		*/
		IMG_UINT32 vertexInputIndex;
		for (vertexInputIndex = 0; 
			vertexInputIndex < psVS->uVertexShaderInputCount;
			++vertexInputIndex)
		{
			PFIXED_REG_DATA	psVertexInputFixedReg = psVS->aVertexShaderInputsFixedRegs[vertexInputIndex];
			/*
				If it is a vector fixed register we'll expand it
			*/
			if (IMG_TRUE == psVertexInputFixedReg->bVector)
			{
				psVertexInputFixedReg = ExpandVectorFixedReg(psState, &sContext, psVertexInputFixedReg);
				psVS->aVertexShaderInputsFixedRegs[vertexInputIndex] = psVertexInputFixedReg;
			}
		}
#else
		psVS->psVertexShaderInputsFixedReg = 
			ExpandVectorFixedReg(psState, &sContext, psVS->psVertexShaderInputsFixedReg);
#endif
		/*
			Similarly for structures representing vertex shader outputs.
		*/
		if ((psVS->psVertexShaderOutputsFixedReg != NULL) && !(psState->uFlags & USC_FLAGS_OUTPUTRELATIVEADDRESSING))
		{
			psVS->psVertexShaderOutputsFixedReg = 
				ExpandVectorFixedReg(psState, &sContext, psVS->psVertexShaderOutputsFixedReg);
		}
	}

	/*
		Update stored information about the secondary update program for the change
		to scalar registers.
	*/
	LowerSAProgVectors(psState, &sContext);

	/*
		Free the context data for this stage.
	*/
	DeinitializeRemapTable(psState, &sContext.sRemap);

	/*
		Flag to later stages the work we have done.
	*/
	psState->uFlags |= USC_FLAGS_REPLACEDVECTORREGS;

	/*
		If any moves were inserted to copy one half of a register from a partially overwritten destination then
		replace the move destination by the source.
	*/
	for (psListEntry = sContext.sOtherHalfMoveList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PINST		psMOVInst;
		IMG_UINT32	uDestIdx;

		psNextListEntry = psListEntry->psNext;
		psMOVInst = IMG_CONTAINING_RECORD(psListEntry, PINST, sAvailableListEntry);

		ASSERT(psMOVInst->eOpcode == IVMOV || psMOVInst->eOpcode == IMOV);
		ASSERT(NoPredicate(psState, psMOVInst));
		if (psMOVInst->eOpcode == IVMOV)
		{
			ASSERT(psMOVInst->u.psVec->auSwizzle[0] == USEASM_SWIZZLE(X, Y, Z, W));
		}

		for (uDestIdx = 0; uDestIdx < psMOVInst->uDestCount; uDestIdx++)
		{
			PARG			psDest = &psMOVInst->asDest[uDestIdx];
			PARG			psSrc;

			ASSERT(psDest->uType == USEASM_REGTYPE_TEMP);

			if (psMOVInst->eOpcode == IMOV)
			{
				psSrc = &psMOVInst->asArg[uDestIdx];
			}
			else
			{
				psSrc = &psMOVInst->asArg[1 + uDestIdx];
			}
			ASSERT(psSrc->uType == USEASM_REGTYPE_TEMP);


			/*
				Substitute the intermediate register in all instructions.
			*/
			UseDefSubstituteIntermediateRegister(psState, 
												 psDest, 
												 psSrc, 
												 #if defined(SRC_DEBUG)
												 psMOVInst->uAssociatedSrcLine,
												 #endif /* defined(SRC_DEBUG) */
												 IMG_FALSE /* bExcludePrimaryEpilog */,
												 IMG_FALSE /* bExcludeSecondaryEpilog */,
												 IMG_FALSE /* bCheckOnly */);
		}

		RemoveInst(psState, psMOVInst->psBlock, psMOVInst);
		FreeInst(psState, psMOVInst);
	}

	/*
		Show the changed intermediate code.
	*/
	TESTONLY_PRINT_INTERMEDIATE("Replaced Vector Registers By Scalars", "vec_lower", IMG_TRUE);
}

static IMG_BOOL IsPermutedConstant(IMG_UINT8		aui8Data[], 
								   IMG_UINT32		uUsedChanMask,
								   IMG_UINT8		aui8Const[], 
								   IMG_UINT32		uConstChanCount,
								   IMG_UINT32		uChanWidthInBytes, 
								   PSWIZZLE_SPEC	psSwizzle)
/*****************************************************************************
 FUNCTION	: IsPermutedConstant
    
 PURPOSE	: Check if one vector is equal to another after a swizzle is applied.

 PARAMETERS	: aui8Data			- First vector to compare.
			  uUsedChanMask		- Mask of channels to compare from the first
								vector.
			  aui8Const			- Second vector to compare.
			  uConstChanCount	- Maximum number of channels valid in the second
								vector.
			  uChanWidthInBytes	- Width of a channel in the two vectors.
			  psSwizzle			- Returns details of the possible swizzles to
								make the two vectors match.

 RETURNS	: TRUE if there is a possible swizzle.
*****************************************************************************/
{
	IMG_UINT32	uSwizzle;
	IMG_UINT32	uUsedConstChanCount;
	IMG_UINT32	uChan;

	uSwizzle = USEASM_SWIZZLE(0, 0, 0, 0);
	uUsedConstChanCount = 0;
	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		if ((uUsedChanMask & (1 << uChan)) != 0)
		{
			IMG_PUINT8	pui8Chan = aui8Data + uChan * uChanWidthInBytes;
			IMG_UINT32	uConstChan;
			IMG_UINT32	uChanMask;

			/*
				Compare this channel from the first vector to all channels
				in the second vector.
			*/
			uChanMask = 0;
			for (uConstChan = 0; uConstChan < uConstChanCount; uConstChan++)
			{
				IMG_PUINT8	pui8ConstChan = aui8Const + uConstChan * uChanWidthInBytes;

				if (memcmp(pui8Chan, pui8ConstChan, uChanWidthInBytes) == 0)
				{
					uChanMask |= (1 << uConstChan);
				}
			}
			if (uChanMask == 0)
			{
				/*
					No matches.
				*/
				return IMG_FALSE;
			}

			psSwizzle->auChanMask[uChan] = uChanMask;
		}
	}

	return IMG_TRUE;
}

static
IMG_BOOL IsSwizzledHardwareConstant(PINTERMEDIATE_STATE	psState,
									IMG_UINT8			aui8VecData[],
									IMG_UINT32			uUsedChanMask,
									IMG_UINT32			uConst,
									UF_REGFORMAT		eFormat,
									PSWIZZLE_SPEC		psSwizzle)
/*****************************************************************************
 FUNCTION	: IsSwizzledHardwareConstant
    
 PURPOSE	: Check for a hardware constant which, after a swizzle is
			  applied, is equal to a supplied vector.

 PARAMETERS	: psState		- Compiler state.
			  aui8VecData	- Complete value of the vector.
			  uUsedChanMask	- Mask of channels to consider in the vector.
			  uConst		- Constant number to check.
			  eFormat		- Format of the data in the vector.
			  psSwizzle		- Returns details of the swizzle.

 RETURNS	: TRUE if the constant after swizzling is equal to the supplied
			  vector.
*****************************************************************************/
{
	PCUSETAB_SPECIAL_CONST	psConst = &g_asVecHardwareConstants[uConst];

	if (psConst->bReserved)
	{
		return IMG_FALSE;
	}

	if (eFormat == UF_REGFORMAT_F16)
	{
		return IsPermutedConstant(aui8VecData, 
								  uUsedChanMask, 
								  (IMG_PUINT8)psConst->auValue, 
								  VECTOR_LENGTH /* uConstChanCount */, 
								  WORD_SIZE /* uChanWidthInBytes */, 
								  psSwizzle);
	}
	else
	{
		IMG_UINT32	auConstData[VECTOR_LENGTH];
		IMG_UINT32	uConstChanCount;
		IMG_UINT32	uConstChan;

		ASSERT(eFormat == UF_REGFORMAT_F32);

		/*
			Concatenate two 64-bit entries from the hardware constant table to make the
			128-bit vector accessible from an F32 format source.
		*/
		for (uConstChan = 0; uConstChan < SGXVEC_USE_SPECIAL_CONSTANT_WIDTH; uConstChan++)
		{
			auConstData[uConstChan] = psConst->auValue[uConstChan];
		}
		uConstChanCount = SGXVEC_USE_SPECIAL_CONSTANT_WIDTH;

		if ((uConst + 1) < SGXVEC_USE_SPECIAL_NUM_FLOAT_CONSTANTS)
		{
			PCUSETAB_SPECIAL_CONST	psNextConst = &g_asVecHardwareConstants[uConst + 1];

			if (!psNextConst->bReserved)
			{
				for (uConstChan = 0; uConstChan < SGXVEC_USE_SPECIAL_CONSTANT_WIDTH; uConstChan++)
				{
					auConstData[SGXVEC_USE_SPECIAL_CONSTANT_WIDTH + uConstChan] = psNextConst->auValue[uConstChan];
				}
				uConstChanCount += SGXVEC_USE_SPECIAL_CONSTANT_WIDTH;
			}
		}

		/*
			Check if the hardware constant at this offset is equal to the supplied vector after
			applying a swizzle.
		*/
		return IsPermutedConstant(aui8VecData, 
								  uUsedChanMask, 
								  (IMG_PUINT8)auConstData, 
								  uConstChanCount, 
								  LONG_SIZE /* uChanWidthInBytes */, 
								  psSwizzle);
	}
}

static
IMG_BOOL FindSwizzledHardwareConstant(PINTERMEDIATE_STATE	psState,
									  IMG_UINT8				aui8VecData[],
									  IMG_UINT32			uUsedChanMask,
									  IMG_PUINT32			puConst,
									  IMG_PUINT32			puSwizzle,
									  UF_REGFORMAT			eFormat)
/*****************************************************************************
 FUNCTION	: FindSwizzledHardwareConstant
    
 PURPOSE	: Check for a hardware constant which, after a swizzle is
			  applied, is equal to a supplied vector.

 PARAMETERS	: psState		- Compiler state.
			  aui8VecData	- Complete value of the vector.
			  uUsedChanMask	- Mask of channels to consider in the vector.
			  puConst		- Returns the index of the found constant.
			  puSwizzle		- Returns the swizzle to apply.
			  eFormat		- Format of the data in the vector.

 RETURNS	: TRUE if a matching hardware constant was found.
*****************************************************************************/
{
	IMG_UINT32	uFoundConst;
	IMG_UINT32	uFoundSwizzle;
	IMG_UINT32	uConst;

	uFoundConst = uFoundSwizzle = USC_UNDEF;
	for (uConst = 0; uConst < SGXVEC_USE_SPECIAL_NUM_FLOAT_CONSTANTS; uConst++)
	{
		IMG_UINT32		uUsedChanCount;
		IMG_UINT32		uSwizzle;
		SWIZZLE_SPEC	sSwizzle;
		IMG_UINT32		uPreSwizzleUsedChanMask;

		/*
			Check if this constant with a swizzle matches the supplied vector.
		*/
		if (!IsSwizzledHardwareConstant(psState, aui8VecData, uUsedChanMask, uConst, eFormat, &sSwizzle))
		{
			continue;
		}

		/*
			Convert the swizzle to the USEASM format.
		*/
		uSwizzle = SpecToSwizzle(psState, &sSwizzle, uUsedChanMask);

		uPreSwizzleUsedChanMask = ApplySwizzleToMask(uSwizzle, uUsedChanMask);
		uUsedChanCount = g_auMaxBitCount[uPreSwizzleUsedChanMask];

		/*
			If we found a match but one which requires accessing two 64-bit constants then
			carry on looking to find a shorter constant.
		*/
		if (uUsedChanCount <= SGXVEC_USE_SPECIAL_CONSTANT_WIDTH)
		{
			*puConst = uConst;
			*puSwizzle = uSwizzle;
			return IMG_TRUE;
		}
		else
		{
			uFoundConst = uConst;
			uFoundSwizzle = uSwizzle;
		}
	}

	if (uFoundConst != USC_UNDEF)
	{
		*puConst = uFoundConst;
		*puSwizzle = uFoundSwizzle;
		return IMG_TRUE;
	}

	return IMG_FALSE;
}

static
IMG_BOOL FindReplicatedHardwareConstant(IMG_UINT32		auVecData[],
										IMG_UINT32		auVecUsedMask[],
										IMG_PUINT32		puConst,
										IMG_PUINT32		puSwizzle,
										UF_REGFORMAT	eFormat)
/*****************************************************************************
 FUNCTION	: FindReplicatedHardwareConstant
    
 PURPOSE	: Check for a channel in a vector hardware constant with the same 
			  value as a supplied vector.

 PARAMETERS	: auVecData		- Value of each channel in the vector.
			  uUsedChanMask	- Mask of channels to consider in the vector.
			  puConst		- Returns the index of the found constant.
			  puSwizzle		- Returns the swizzle to apply.
			  eFormat		- Format of the data in the vector.

 RETURNS	: TRUE if a matching hardware constant was found.
*****************************************************************************/
{
	IMG_UINT32	uConstIdx;
	IMG_UINT32	uChanIdx;
	IMG_BOOL	bFirst;
	IMG_UINT32	uData;
	IMG_UINT32	uConstWidth;

	/*
		Check all the referenced channels in the vector have the same value.
	*/
	bFirst = IMG_TRUE;
	uData = USC_UNDEF;
	for (uChanIdx = 0; uChanIdx < VECTOR_LENGTH; uChanIdx++)
	{
		IMG_UINT32	uUsedChanMask;

		if (eFormat == UF_REGFORMAT_F32)
		{
			uUsedChanMask = auVecUsedMask[uChanIdx];
		}
		else
		{
			uUsedChanMask = ((IMG_PUINT16)auVecUsedMask)[uChanIdx];
		}

		if (uUsedChanMask != 0)
		{
			IMG_UINT32	uDataForChan;

			if (eFormat == UF_REGFORMAT_F32)
			{
				uDataForChan = auVecData[uChanIdx];
			}
			else
			{
				uDataForChan = ((IMG_PUINT16)auVecData)[uChanIdx];
			}

			if (bFirst)
			{
				uData = uDataForChan;
				bFirst = IMG_FALSE;
			}
			else
			{
				if (uData != uDataForChan)
				{
					return IMG_FALSE;
				}
			}
		}
	}

	/*
		Check all the channels in all the vector hardware constants for a matching
		value.
	*/
	uConstWidth = SGXVEC_USE_SPECIAL_CONSTANT_WIDTH;
	if (eFormat == UF_REGFORMAT_F16)
	{
		uConstWidth *= 2;
	}
	for (uConstIdx = 0; uConstIdx < SGXVEC_USE_SPECIAL_NUM_FLOAT_CONSTANTS; uConstIdx++)
	{
		IMG_UINT32	uVecIdx;

		for (uVecIdx = 0; uVecIdx < uConstWidth; uVecIdx++)
		{
			IMG_UINT32				uConstValue;
			PCUSETAB_SPECIAL_CONST	psConst = &g_asVecHardwareConstants[uConstIdx];

			if (psConst->bReserved)
			{
				continue;
			}

			if (eFormat == UF_REGFORMAT_F32)
			{
				uConstValue = psConst->auValue[uVecIdx];
			}
			else
			{
				uConstValue = ((IMG_PUINT16)psConst->auValue)[uVecIdx];
			}
			if (uConstValue == uData)
			{
				/*
					Return the index of the found constant.
				*/
				*puConst = uConstIdx;

				/*
					Return the swizzle needed to access the matching channel.
				*/
				*puSwizzle = g_auReplicateSwizzles[uVecIdx];

				return IMG_TRUE;
			}
		}
	}
	return IMG_FALSE;
}

static
IMG_UINT32 FindVectorHardwareConstant(IMG_UINT32			auVecData[],
									  IMG_UINT32			auChanUsedMask[])
/*****************************************************************************
 FUNCTION	: FindVectorHardwareConstant
    
 PURPOSE	: Check for a vector hardware constant with the same value as a
			  supplied vector.

 PARAMETERS	: auVecData		- Value of each channel in the vector.
			  uUsedMask		- Mask of channels to consider in the vector.

 RETURNS	: The index of the found constant if one exists or USC_UNDEF if
			  no constant matches.
*****************************************************************************/
{
	IMG_UINT32	uVecIdx;
	IMG_UINT32	uConstIdx;

	for (uConstIdx = 0; uConstIdx < SGXVEC_USE_SPECIAL_NUM_FLOAT_CONSTANTS; uConstIdx++)
	{
		IMG_BOOL				bMatch;

		bMatch = IMG_TRUE;
		for (uVecIdx = 0; uVecIdx < MAX_SCALAR_REGISTERS_PER_VECTOR; uVecIdx++)
		{
			IMG_UINT32				uConst;
			PCUSETAB_SPECIAL_CONST	psConst = &g_asVecHardwareConstants[uConstIdx + (uVecIdx >> 1)];

			if (psConst->bReserved)
			{
				bMatch = IMG_FALSE;
				break;
			}

			uConst = psConst->auValue[uVecIdx & 1];
			if ((uConst & auChanUsedMask[uVecIdx]) != (auVecData[uVecIdx] & auChanUsedMask[uVecIdx]))
			{
				bMatch = IMG_FALSE;
				break;
			}
		}

		if (bMatch)
		{
			return uConstIdx;
		}
	}
	return USC_UNDEF;
}

static
IMG_UINT32 ReplaceByReplicatedConstants(PINTERMEDIATE_STATE	psState,
									    IMG_UINT32			auVecData[],
									    IMG_UINT32			auVecUsedMask[],
									    PINST				psInst,
									    IMG_UINT32			uArgIdx)
/*****************************************************************************
 FUNCTION	: ReplaceByReplicatedConstants
    
 PURPOSE	: Check if an immediate vector source can be replaced by a
			  hardware constant accessed with a replicate swizzle.

 PARAMETERS	: psState			- Compiler state.
			  auVecData			- Value of the immediate vector.
			  auVecUsedMask		- Bit mask of channels used from the
								immediate vector.
			  psInst			- Instruction accessing the vector.
			  uArgIdx			- Index of the source argument accessing
								the vector.

 RETURNS	: The index of the found constant or USC_UNDEF if no constant
			  matches.
*****************************************************************************/
{
	IMG_UINT32	uReplicateSwizzle;
	IMG_UINT32	uLiveChans;
	IMG_UINT32	uConstIdx;
	IMG_UINT32	uOriginalSwizzle;
	IMG_UINT32	uChan;

	/*
		Skip instructions where we can't change one source to use a different swizzle
		without affecting the other sources.
	*/
	if (!HasIndependentSourceSwizzles(psState, psInst))
	{
		return USC_UNDEF;
	}

	/*
		Check for a hardware constant which has the same value when
		a swizzle is applied.
	*/
	if (!FindReplicatedHardwareConstant(auVecData, 
										auVecUsedMask, 
										&uConstIdx, 
										&uReplicateSwizzle,
										psInst->u.psVec->aeSrcFmt[uArgIdx]))
	{
		return USC_UNDEF;
	}

	/*
		Don't replace any selectors in the original swizzle which select constants.
	*/
	uOriginalSwizzle = psInst->u.psVec->auSwizzle[uArgIdx];
	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		USEASM_SWIZZLE_SEL	eSel;

		eSel = GetChan(uOriginalSwizzle, uChan);
		if (eSel == USEASM_SWIZZLE_SEL_0 ||
			eSel == USEASM_SWIZZLE_SEL_1 ||
			eSel == USEASM_SWIZZLE_SEL_2 ||
			eSel == USEASM_SWIZZLE_SEL_HALF)
		{
			uReplicateSwizzle = SetChan(uReplicateSwizzle, uChan, eSel);
		}
	}

	/*
		Check the replicate swizzle is supported by the instruction.
	*/
	GetLiveChansInSourceSlot(psState, psInst, uArgIdx, &uLiveChans, NULL /* puChansUsedPostSwizzle */);
	if (!IsSwizzleSupported(psState, 
						    psInst, 
							psInst->eOpcode, 
							uArgIdx, 
							uReplicateSwizzle, 
							uLiveChans, 
							&uReplicateSwizzle))
	{
		return USC_UNDEF;
	}

	/*
		Update the swizzle applied to this source by the instruction.
	*/
	psInst->u.psVec->auSwizzle[uArgIdx] = uReplicateSwizzle;
	return uConstIdx;
}

static
IMG_VOID ReplaceImmediatesBP(PINTERMEDIATE_STATE	psState,
							 PCODEBLOCK				psCodeBlock,
							 IMG_PVOID				pvNULL)
/*****************************************************************************
 FUNCTION	: ReplaceImmediatesBP
    
 PURPOSE	: Replace vector immediates by the equivalent hardware constant
			  where possible.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Basic block to process.
			  pvNULL			- Ignored.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psInst;

	PVR_UNREFERENCED_PARAMETER(pvNULL);

	for (psInst = psCodeBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		IMG_UINT32	uArgIdx;

		/*
			Process only instructions which use vector sources.
		*/
		if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) == 0)
		{
			continue;
		}

		for (uArgIdx = 0; uArgIdx < GetSwizzleSlotCount(psState, psInst); uArgIdx++)
		{
			IMG_UINT32	auVecData[4];
			IMG_UINT32	auVecUsedMask[4];
			IMG_BOOL	bAllImmediates;
			IMG_UINT32	uConstIdx;
			IMG_UINT32	uArgBase = uArgIdx * SOURCE_ARGUMENTS_PER_VECTOR;
			PARG		psGPIArg = &psInst->asArg[uArgBase];
			IMG_UINT32	uChanIdx;
			IMG_UINT32	uConstStartRegNum;

			/*
				Skip if all channels of this argument are coming from a vector sized
				register.
			*/
			if (psGPIArg->uType != USC_REGTYPE_UNUSEDSOURCE)
			{
				continue;
			}

			/*
				Skip if the hardware constant register bank isn't supported by this source.
			*/
			if (!CanUseSrc(psState, psInst, uArgBase + 1, USEASM_REGTYPE_FPCONSTANT, USC_REGTYPE_NOINDEX))
			{
				continue;
			}

			/*
				Check all the channels used from this vector argument are
				immediate values.
			*/
			bAllImmediates = IMG_TRUE;
			for (uChanIdx = 0; uChanIdx < MAX_SCALAR_REGISTERS_PER_VECTOR; uChanIdx++)
			{
				IMG_UINT32	uChanArg = uArgBase + 1 + uChanIdx;
				PARG		psChanArg = &psInst->asArg[uChanArg];
				IMG_UINT32	uLiveChans;
				IMG_UINT32	uByteIdx;

				uLiveChans = GetLiveChansInArg(psState, psInst, uChanArg);
				if (uLiveChans == 0)
				{
					auVecUsedMask[uChanIdx] = 0;
					continue;
				}

				if (psChanArg->uType != USEASM_REGTYPE_IMMEDIATE)
				{
					bAllImmediates = IMG_FALSE;
					break;
				}
				
				auVecData[uChanIdx] = psChanArg->uNumber;
				auVecUsedMask[uChanIdx] = 0;
				for (uByteIdx = 0; uByteIdx < 4; uByteIdx++)
				{
					if (uLiveChans & (1 << uByteIdx))
					{
						auVecUsedMask[uChanIdx] |= (0xFF << (uByteIdx * 8));
					}
				}
			}

			if (!bAllImmediates)
			{
				continue;
			}

			/*
				First check for a hardware constant we can access through a
				replicated swizzle. This minimizes amount of data which
				has to be read from the unified store.
			*/
			uConstIdx = ReplaceByReplicatedConstants(psState,
													 auVecData,
													 auVecUsedMask,
													 psInst,
													 uArgIdx);
			
			if (uConstIdx == USC_UNDEF)
			{
				/*
					Check for a hardware constant with the same value as the
					vector and using the existing swizzle.
				*/
				uConstIdx = FindVectorHardwareConstant(auVecData, auVecUsedMask);
				if (uConstIdx == USC_UNDEF)
				{
					continue;		
				}
			}

			/*
				Replace the immediate sources by the hardware constants.
			*/
			uConstStartRegNum = uConstIdx << SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT;
			for (uChanIdx = 0; uChanIdx < MAX_SCALAR_REGISTERS_PER_VECTOR; uChanIdx++)
			{
				IMG_UINT32	uChanArg = uArgIdx * SOURCE_ARGUMENTS_PER_VECTOR + 1 + uChanIdx;

				SetSrc(psState, 
					   psInst,
					   uChanArg, 
					   USEASM_REGTYPE_FPCONSTANT, 
					   uConstStartRegNum + uChanIdx, 
					   psInst->u.psVec->aeSrcFmt[uArgIdx]);
			}
		}
	}
}

static
IMG_BOOL OptimiseConstantOne(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uVecArg)
/*****************************************************************************
 FUNCTION	: OptimiseConstantOne
    
 PURPOSE	: Eliminate or change instructions that use a given register, because
				its value is 1.

 PARAMETERS	: psState		- Compiler state.
			  psConstReg	- The register containing 1

 RETURNS	: Whether the instruction could be optimised (and has been replaced).
*****************************************************************************/
{
	switch (psInst->eOpcode)
	{
		case IVMAD:
		case IVMAD4:
		{
			if (uVecArg == 0 || uVecArg == 1)
			{
				/* Multiplication argument. Replace with an add. */
				IMG_UINT	uDest;
				PINST		psAddInst = CopyInst(psState, psInst);
				IMG_BOOL	bNegOne;

				bNegOne = psInst->u.psVec->asSrcMod[uVecArg].bNegate;

				if (psInst->eOpcode == IVMAD)
				{
					SetOpcode(psState, psAddInst, IVADD);
				}
				else
				{
					ASSERT(psInst->eOpcode == IVMAD4);
					SetOpcode(psState, psAddInst, IVADD3);
				}
	
				for (uDest = 0; uDest < psInst->uDestCount; ++uDest)
				{
					MoveDest(psState, psAddInst, uDest, psInst, uDest);
				}
			
				MoveVectorSource(psState, 
								 psAddInst, 
								 1 /* uDestArgIdx */, 
								 psInst, 
								 2 /* uSrcArgIdx */, 
								 IMG_TRUE /* bCopySourceModifier */);

				MoveVectorSource(psState, 
								 psAddInst, 
								 0 /* uDestArgIdx */, 
								 psInst, 
								 1 - uVecArg, 
								 IMG_TRUE /* bCopySourceModifier */);
				if (bNegOne)
				{
					psAddInst->u.psVec->asSrcMod[0].bNegate = !psAddInst->u.psVec->asSrcMod[0].bNegate;
				}

				InsertInstBefore(psState, psInst->psBlock, psAddInst, psInst);

				RemoveInst(psState, psInst->psBlock, psInst);
				FreeInst(psState, psInst);
	
				return IMG_TRUE;
			}
			else
			{
				return IMG_FALSE;
			}
		}
		case IVMUL:
		{
			/* Multiplication argument. Replace with a move. */
			IMG_UINT	uDest;
			PINST		psMovInst = CopyInst(psState, psInst);
			IMG_BOOL	bNegOne;

			ASSERT(uVecArg == 0 || uVecArg == 1);

			bNegOne = psInst->u.psVec->asSrcMod[uVecArg].bNegate;

			SetOpcode(psState, psMovInst, IVMOV);
	
			for (uDest = 0; uDest < psInst->uDestCount; ++uDest)
			{
				MoveDest(psState, psMovInst, uDest, psInst, uDest);
			}
		
			MoveVectorSource(psState, 
							 psMovInst, 
							 0 /* uDestArgIdx */, 
							 psInst, 
							 1 - uVecArg, 
							 IMG_TRUE /* bCopySourceModifier */);
			if (bNegOne)
			{
				psMovInst->u.psVec->asSrcMod[0].bNegate = !psMovInst->u.psVec->asSrcMod[0].bNegate;
			}

			InsertInstBefore(psState, psInst->psBlock, psMovInst, psInst);

			RemoveInst(psState, psInst->psBlock, psInst);
			FreeInst(psState, psInst);
	
			return IMG_TRUE;
		}
		default:
		{
			return IMG_FALSE;
		}
	}
}

static
IMG_BOOL OptimiseConstantZero(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uVecArg)
/*****************************************************************************
 FUNCTION	: OptimiseConstantZero
    
 PURPOSE	: Eliminate or change instructions that use a given register, because
				its value is 0.

 PARAMETERS	: psState		- Compiler state.
			  psConstReg	- The register containing 0

 RETURNS	: Whether the instruction could be optimised (and has been replaced).
*****************************************************************************/
{
	switch (psInst->eOpcode)
	{
		case IVMAD:
		case IVMAD4:
		{
			if (uVecArg == 0 || uVecArg == 1)
			{
				/* Multiplication argument. Replace with a move. */
				IMG_UINT32	uDest;
				PINST		psMoveInst = CopyInst(psState, psInst);

				SetOpcode(psState, psMoveInst, IVMOV);

				for (uDest = 0; uDest < psInst->uDestCount; ++uDest)
				{
					MoveDest(psState, psMoveInst, uDest, psInst, uDest);
				}
	
				MoveVectorSource(psState, 
								 psMoveInst, 
								 0 /* uDestArgIdx */, 
								 psInst, 
								 2 /* uSrcArgIdx */, 
								 IMG_TRUE /* bCopySourceModifier */);

				InsertInstBefore(psState, psInst->psBlock, psMoveInst, psInst);
	
				RemoveInst(psState, psInst->psBlock, psInst);
				FreeInst(psState, psInst);

				return IMG_TRUE;
			}
			else
			{
				/* Addition argument. Replace with a MUL. */
				IMG_UINT32	uDest;
				PINST		psMulInst = CopyInst(psState, psInst);
			
				ASSERT(uVecArg == 2);

				if (psInst->eOpcode == IVMAD)
				{
					SetOpcode(psState, psMulInst, IVMUL);
				}
				else
				{
					ASSERT(psInst->eOpcode == IVMAD4);
					SetOpcode(psState, psMulInst, IVMUL3);
				}
	
				for (uDest = 0; uDest < psInst->uDestCount; ++uDest)
				{
					MoveDest(psState, psMulInst, uDest, psInst, uDest);
				}	

				MoveVectorSource(psState, 
								 psMulInst, 
								 0 /* uDestArgIdx */, 
								 psInst, 
								 0 /* uSrcArgIdx */, 
								 IMG_TRUE /* bCopySourceModifier */);
				MoveVectorSource(psState, 
								 psMulInst, 
								 1 /* uDestArgIdx */, 
								 psInst, 
								 1 /* uSrcArgIdx */, 
								 IMG_TRUE /* bCopySourceModifier */);

				InsertInstBefore(psState, psInst->psBlock, psMulInst, psInst);

				RemoveInst(psState, psInst->psBlock, psInst);
				FreeInst(psState, psInst);

				return IMG_TRUE;
			}
		}
		case IVMUL:
		{
			/* Replace with a move. */
			IMG_UINT32	uDest;
			PINST		psMoveInst = CopyInst(psState, psInst);

			ASSERT(uVecArg == 0 || uVecArg == 1);
	
			SetOpcode(psState, psMoveInst, IVMOV);

			for (uDest = 0; uDest < psInst->uDestCount; ++uDest)
			{
				MoveDest(psState, psMoveInst, uDest, psInst, uDest);
			}

			psMoveInst->u.psVec->aeSrcFmt[0] = psMoveInst->asDest[0].eFmt;
			psMoveInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);

			SetImmediateVector(psState, psMoveInst, 0 /* uSrcIdx */, 0 /* uImm */);

			InsertInstBefore(psState, psInst->psBlock, psMoveInst, psInst);

			RemoveInst(psState, psInst->psBlock, psInst);
			FreeInst(psState, psInst);

			return IMG_TRUE;
		}
		case IVADD:
		{
			/* Replace with a move. */
			IMG_UINT	uDest, uNonConstSrc;
			PINST		psMoveInst = CopyInst(psState, psInst);

			ASSERT(uVecArg == 0 || uVecArg == 1);
			uNonConstSrc = 1 - uVecArg;

			SetOpcode(psState, psMoveInst, IVMOV);

			for (uDest = 0; uDest < psInst->uDestCount; ++uDest)
			{
				MoveDest(psState, psMoveInst, uDest, psInst, uDest);
			}

			MoveVectorSource(psState, psMoveInst, 0 /* uDestArgIdx */, psInst, uNonConstSrc, IMG_TRUE /* bCopySourceModifier */);

			InsertInstBefore(psState, psInst->psBlock, psMoveInst, psInst);

			RemoveInst(psState, psInst->psBlock, psInst);
			FreeInst(psState, psInst);

			return IMG_TRUE;
		}
		default:
		{
			return IMG_FALSE;
		}
	}
}

static
IMG_BOOL OptimiseConstantSource(PINTERMEDIATE_STATE psState, PUSEDEF psUse, IMG_UINT32 uChan, IMG_BOOL bIsZero, IMG_BOOL bIsOne)
/*****************************************************************************
 FUNCTION	: OptimiseConstantSource
    
 PURPOSE	: Try to simplify or eliminate an instruction using a constant
			  register with the value 0 or 1.

 PARAMETERS	: psState		- Compiler state.
			  psUse			- Use.
			  uChan			- The channel of the use that is the constant.
			  bIsZero		- TRUE if the compile-time known value is zero.
			  bIsOne		- TRUE if the compile-time known value is one.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PINST		psUseInst;
	IMG_UINT32	uVecArg, uLiveChansInArg, uChan2;

	if (psUse->eType != USE_TYPE_SRC)
	{
		return IMG_FALSE;
	}
	psUseInst = psUse->u.psInst;

	/*
		Optimise uses in vector instructions only.
	*/
	if ((g_psInstDesc[psUseInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) == 0)
	{
		return IMG_FALSE;
	}

	ASSERT((psUse->uLocation % SOURCE_ARGUMENTS_PER_VECTOR) == 0);
	uVecArg = psUse->uLocation / SOURCE_ARGUMENTS_PER_VECTOR;

	/*
		Check the swizzle isn't selecting (different) immediate values into some channels
		of the source.
	*/
	GetLiveChansInSourceSlot(psState, 
							 psUseInst, 
							 uVecArg,
							 &uLiveChansInArg, 
							 NULL /* puChansUsedPostSwizzle */);
	if (!IsNonConstantSwizzle(psUseInst->u.psVec->auSwizzle[uVecArg], uLiveChansInArg, NULL /* puMatchedSwizzle */))
	{
		return IMG_FALSE;
	}
	
	for (uChan2 = 0; uChan2 < VECTOR_LENGTH; ++uChan2)
	{
		if ((uLiveChansInArg & (1 << uChan2)) != 0)
		{
			IMG_UINT uSel = GetChan(psUseInst->u.psVec->auSwizzle[uVecArg], uChan2);

			if (uSel != uChan)
			{
				return IMG_FALSE;
			}
		}
	}

	if (psUseInst->eOpcode == IVMOV &&
		(
			psUseInst->auDestMask[0] == USC_X_CHAN_MASK ||
			psUseInst->auDestMask[0] == USC_Y_CHAN_MASK ||
			psUseInst->auDestMask[0] == USC_Z_CHAN_MASK ||
			psUseInst->auDestMask[0] == USC_W_CHAN_MASK
		) &&
		(
			bIsZero || !psUseInst->u.psVec->asSrcMod[0].bNegate
		)
		)
	{
		PUSC_LIST_ENTRY	psUseListEntry, psNextUseListEntry;
		PUSEDEF_CHAIN	psDestUseDef	= UseDefGet(psState, psUseInst->asDest[0].uType, psUseInst->asDest[0].uNumber);
		IMG_BOOL		bRemovedAllUses = IMG_TRUE;
		IMG_UINT32		uChanUsed;

		switch(psUseInst->auDestMask[0])
		{
		case USC_X_CHAN_MASK: uChanUsed = 0; break;
		case USC_Y_CHAN_MASK: uChanUsed = 1; break;
		case USC_Z_CHAN_MASK: uChanUsed = 2; break;
		default: ASSERT(psUseInst->auDestMask[0] == USC_W_CHAN_MASK); uChanUsed = 3;
		}

		ASSERT(psUseInst->uDestCount == 1);

		for (psUseListEntry = psDestUseDef->sList.psHead; 
			psUseListEntry != NULL;
			psUseListEntry = psNextUseListEntry)
		{
			PUSEDEF	psUse;

			psNextUseListEntry = psUseListEntry->psNext;
			psUse = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);

			if (psUse == psDestUseDef->psDef)
			{
				continue;
			}

			if (OptimiseConstantSource(psState, psUse, uChanUsed, bIsZero, bIsOne))
			{
				psNextUseListEntry = psDestUseDef->sList.psHead;
			}
			else
			{
				bRemovedAllUses = IMG_FALSE;
			}
		}

		if (bRemovedAllUses)
		{
			RemoveInst(psState, psUseInst->psBlock, psUseInst);
			FreeInst(psState, psUseInst);

			return IMG_TRUE;
		}
		else
		{
			return IMG_FALSE;
		}
	}

	if (bIsOne)
	{
		return OptimiseConstantOne(psState, psUseInst, uVecArg);
	}
	else
	{
		ASSERT(bIsZero);
		return OptimiseConstantZero(psState, psUseInst, uVecArg);
	}
}

IMG_INTERNAL
IMG_BOOL FoldConstants(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: FoldConstants
    
 PURPOSE	: Replace constant loads with moves from a hardware register.
			  Eliminate or simplify instructions if they use 0 or 1 .

 PARAMETERS	: psState - Compiler state.

 RETURNS	: None.
*****************************************************************************/
{
	SAFE_LIST_ITERATOR sIter;
	IMG_BOOL bFoldedConstants = IMG_FALSE;

	InstListIteratorInitialize(psState, ILOADCONST, &sIter);
	for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
	{
		PINST				psInst = InstListIteratorCurrent(&sIter);
		IMG_INT32			iChan;
		IMG_UINT32			uChan2;
		IMG_UINT32			uStartChan;
		IMG_UINT32			uStaticValue;
		IMG_UINT32			uHWConst;
		PUSEDEF_CHAIN		psDestUseDef;
		PUSC_LIST_ENTRY		psUseListEntry;
		PUSC_LIST_ENTRY		psNextUseListEntry;
		ARG					sConstReg;
		IMG_UINT32			auVecData[4];
		IMG_UINT32			auVecUsedMask[4];
		IMG_UINT32			uSwizzle;
		IMG_BOOL			bIsOne;
		IMG_BOOL			bIsZero;
		IMG_BOOL			bDestIsScalar;

		if (psInst->asArg[LOADCONST_DYNAMIC_OFFSET_ARGINDEX].uType	!= USEASM_REGTYPE_IMMEDIATE ||
			psInst->asArg[LOADCONST_STATIC_OFFSET_ARGINDEX].uType	!= USEASM_REGTYPE_IMMEDIATE)
		{
			continue;
		}

		if (psInst->asDest[0].psRegister->psUseDefChain->eIsVector != USEDEF_ISVECTOR_YES)
		{
			bDestIsScalar = IMG_TRUE;
			uStartChan = 0;
		}
		else
		{
			bDestIsScalar = IMG_FALSE;
			uStartChan = VECTOR_LENGTH - 1;
		}

		sConstReg = psInst->asDest[0];

		for (iChan = uStartChan; iChan >= 0; iChan--)
		{
			IMG_BOOL bRemovedAllUses;

			if ((psInst->auLiveChansInDest[0] & (1 << iChan)) == 0)
			{
				continue;
			}

			if ((psInst->asDest[0].eFmt != UF_REGFORMAT_F32) && (psInst->asDest[0].eFmt != UF_REGFORMAT_F16))
			{
				continue;
			}

			/*
				Check if a compile-time known value is loaded into this channel of the destination.
			*/
			if (!GetLoadConstStaticResult(psState, psInst, iChan, IMG_TRUE /* bNative */, &uStaticValue))
			{
				continue;
			}

			if (sConstReg.eFmt == UF_REGFORMAT_F32)
			{
				bIsOne = (uStaticValue == FLOAT32_ONE) ? IMG_TRUE : IMG_FALSE;
				bIsZero = (uStaticValue == FLOAT32_ZERO) ? IMG_TRUE : IMG_FALSE;
			}
			else
			{
				ASSERT(sConstReg.eFmt == UF_REGFORMAT_F16);
				bIsOne = (uStaticValue == FLOAT16_ONE) ? IMG_TRUE : IMG_FALSE;
				bIsZero = (uStaticValue == FLOAT16_ZERO) ? IMG_TRUE : IMG_FALSE;
			}

			for (uChan2 = 0; uChan2 < MAX_SCALAR_REGISTERS_PER_VECTOR; uChan2++)
			{
				auVecData[uChan2] = uStaticValue;
			}

			auVecUsedMask[0] = 1;
			auVecUsedMask[1] = 0;
			auVecUsedMask[2] = 0;
			auVecUsedMask[3] = 0;

			if (!FindReplicatedHardwareConstant(auVecData, 
				auVecUsedMask,
				&uHWConst,
				&uSwizzle, 
				sConstReg.eFmt))
			{
				continue;
			}

			ASSERT(uHWConst != USC_UNDEF);

			bRemovedAllUses = IMG_FALSE;

			/* Simplify instructions that use this constant */
			if (bIsOne || bIsZero)
			{
				bRemovedAllUses = IMG_TRUE;
		
				psDestUseDef = UseDefGet(psState, sConstReg.uType, sConstReg.uNumber);

				for (psUseListEntry = psDestUseDef->sList.psHead; 
					psUseListEntry != NULL;
					psUseListEntry = psNextUseListEntry)
				{
					PUSEDEF		psUse;

					psNextUseListEntry = psUseListEntry->psNext;
					psUse = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);

					if (psUse == psDestUseDef->psDef)
					{
						continue;
					}

					if (OptimiseConstantSource(psState, psUse, iChan, bIsZero, bIsOne))
					{
						psNextUseListEntry = psDestUseDef->sList.psHead;
					}
					else
					{
						bRemovedAllUses = IMG_FALSE;
					}
				}
			}
			
			if (!bRemovedAllUses)
			{
				/* Replace the channel of the constant load with a move from the HW register */
				PINST psMovInst = AllocateInst(psState, psInst);

				if (bDestIsScalar)
				{
					SetOpcode(psState, psMovInst, IMOV);
					MoveDest(psState, psMovInst, 0 /* uMoveToDestIdx */, psInst, 0 /* uMovFromDestIdx */);

					psMovInst->auDestMask[0] = psInst->auDestMask[0];
					psMovInst->auLiveChansInDest[0] = psInst->auLiveChansInDest[0];

					SetSrc(psState,
						   psMovInst, 
						   0,
						   USEASM_REGTYPE_FPCONSTANT,
						   (uHWConst << SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT) + uChan2 - 1,
						   psMovInst->asDest[0].eFmt);
				}
				else
				{
					SetOpcode(psState, psMovInst, IVMOV);

					MoveDest(psState, psMovInst, 0 /* uMoveToDestIdx */, psInst, 0 /* uMovFromDestIdx */);

					psMovInst->auDestMask[0] = 1 << iChan;
					psMovInst->auLiveChansInDest[0] = psInst->auLiveChansInDest[0];

					psMovInst->asArg[0].uType = USC_REGTYPE_UNUSEDSOURCE;
					for (uChan2 = 1; uChan2 <= VECTOR_LENGTH; uChan2++)
					{
						SetSrc(psState,
							psMovInst, 
							uChan2,
							USEASM_REGTYPE_FPCONSTANT,
							(uHWConst << SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT) + uChan2 - 1,
							psMovInst->asDest[0].eFmt);
					}
					psMovInst->u.psVec->aeSrcFmt[0] = psMovInst->asDest[0].eFmt;
					psMovInst->u.psVec->auSwizzle[0] = uSwizzle;

					DropUnusedVectorComponents(psState, psMovInst, 0);
				}

				InsertInstAfter(psState, psInst->psBlock, psMovInst, psInst);
				
				if (bDestIsScalar)
				{
					psInst->auLiveChansInDest[0] = 0;
				}
				else
				{
					psInst->auLiveChansInDest[0] &= ~(1 << iChan);
				}

				if (psInst->auLiveChansInDest[0] != 0)
				{
					ARG	sNewLCDest;

					MakeNewTempArg(psState, psMovInst->asDest[0].eFmt, &sNewLCDest);
					SetPartiallyWrittenDest(psState, psMovInst, 0 /* uDestIdx */, &sNewLCDest);
					SetDestFromArg(psState, psInst, 0 /* uDestIdx */, &sNewLCDest);
				}
			}
			
			bFoldedConstants = IMG_TRUE;
		}
		
		if (psInst->auLiveChansInDest[0] == 0)
		{
			/* Remove the constant load */	
			RemoveInst(psState, psInst->psBlock, psInst);
			FreeInst(psState, psInst);	
		}
	}
	InstListIteratorFinalise(&sIter);

	DBG_PRINTF((DBG_TEST_DATA, __SECTION_TITLE__ "Fold Constants" __SECTION_TITLE__));
	TESTONLY(PrintIntermediate(psState, "fold_consts", IMG_TRUE));
	return bFoldedConstants;
}

static
IMG_VOID SimplifySwizzlesForConstLoad(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: SimplifySwizzlesOnConsts
    
 PURPOSE	: Where elements of the constant loaded by an instruction have the same value 
			  simplify the swizzles at the uses to use as few constant registers as possible, e.g.
			  CONSTF	c1.g, 0x3EA8F5C3
			  CONSTF	c1.b, 0x3EA8F5C3
			  CONSTF	c1.a, 0x3EA8F5C3

			  VMOV r0 c1.swizzle(yzwy)
			  ->
			  VMOV r0 c1.swizzle(yyyy)

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction.

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32			uChan, uChan2;
	IMG_UINT32			uStaticValue, uStaticValue2;
	IMG_BOOL			bIsStaticConst;
	PUSEDEF_CHAIN		psDestUseDef;
	PUSC_LIST_ENTRY		psUseListEntry;
	PUSC_LIST_ENTRY		psNextUseListEntry;
	ARG					psConstReg;

	if (psInst->asArg[LOADCONST_DYNAMIC_OFFSET_ARGINDEX].uType	!= USEASM_REGTYPE_IMMEDIATE ||
		psInst->asArg[LOADCONST_STATIC_OFFSET_ARGINDEX].uType	!= USEASM_REGTYPE_IMMEDIATE)
	{
		return;
	}

	psConstReg = psInst->asDest[0];

	psDestUseDef = UseDefGet(psState, psConstReg.uType, psConstReg.uNumber);

	for (psUseListEntry = psDestUseDef->sList.psHead; 
		psUseListEntry != NULL;
		psUseListEntry = psNextUseListEntry)
	{
		PUSEDEF		psUse;
		PINST		psUseInst;
		IMG_UINT32	uSlot;

		psNextUseListEntry = psUseListEntry->psNext;
		psUse = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);
		psUseInst = psUse->u.psInst;

		if (psUse == psDestUseDef->psDef)
		{
			continue;
		}
			
		if (psUse->eType != USE_TYPE_SRC)
		{
			continue;
		}
			
		if ((g_psInstDesc[psUseInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) == 0)
		{
			continue;
		}

		if ((psUse->uLocation % SOURCE_ARGUMENTS_PER_VECTOR) != 0)
		{
			continue;
		}
		uSlot = psUse->uLocation / SOURCE_ARGUMENTS_PER_VECTOR;

		for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
		{
			IMG_UINT32 uSel = GetChan(psUseInst->u.psVec->auSwizzle[uSlot], uChan);
			
			if ((psInst->auLiveChansInDest[0] & (1 << uSel)) == 0)
			{
				/* This part of the constant is not actually loaded, value undefined. */
				continue;
			}
			
			if (uSel > USEASM_SWIZZLE_SEL_X && uSel <= USEASM_SWIZZLE_SEL_W)
			{
				bIsStaticConst = GetLoadConstStaticResult(psState, psInst, uSel, IMG_FALSE /* bNative */, &uStaticValue);

				if (bIsStaticConst)
				{
					for (uChan2 = 0; uChan2 < uSel; uChan2++)
					{
						bIsStaticConst = 
							GetLoadConstStaticResult(psState, psInst, uChan2, IMG_FALSE /* bNative */, &uStaticValue2);

						if (bIsStaticConst && (uStaticValue == uStaticValue2))
						{
							/* Replace the constant with this one, with a lower swizzle */
							psUseInst->u.psVec->auSwizzle[uSlot] = 
								SetChan(psUseInst->u.psVec->auSwizzle[uSlot], uChan, uChan2);
							break;
						}
					}
				}
			}
		}
	}

	/*
		Update the mask of channels used from the LOADCONST instruction's destination after modifying
		the swizzles at some of the uses.
	*/
	psInst->auLiveChansInDest[0] = UseDefGetUsedChanMask(psState, psDestUseDef);
}

IMG_INTERNAL
IMG_VOID SimplifySwizzlesOnConsts(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: SimplifySwizzlesOnConsts
    
 PURPOSE	: Where elements of constants have the same value simplify the swizzle
			  to use as few constant registers as possible, e.g.
			  CONSTF	c1.g, 0x3EA8F5C3
			  CONSTF	c1.b, 0x3EA8F5C3
			  CONSTF	c1.a, 0x3EA8F5C3

			  VMOV r0 c1.swizzle(yzwy)
			  ->
			  VMOV r0 c1.swizzle(yyyy)

 PARAMETERS	: psState - Compiler state.

 RETURNS	: None.
*****************************************************************************/
{
	ForAllInstructionsOfType(psState, ILOADCONST, SimplifySwizzlesForConstLoad);

	DBG_PRINTF((DBG_TEST_DATA, __SECTION_TITLE__ "Simplify Swizzles On Consts" __SECTION_TITLE__));
	TESTONLY(PrintIntermediate(psState, "smplfy_swiz_on_consts", IMG_TRUE));
}

IMG_INTERNAL
IMG_VOID ReplaceImmediatesByVecConstants(PINTERMEDIATE_STATE	psState)
/*****************************************************************************
 FUNCTION	: ReplaceImmediatesByVecConstants
    
 PURPOSE	: Replace vector immediates by the equivalent hardware constant
			  where possible.

 PARAMETERS	: psState			- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		Apply the replacement to all basic blocks.
	*/
	DoOnAllBasicBlocks(psState, ANY_ORDER, ReplaceImmediatesBP, IMG_FALSE /* CALLS */, NULL /* pvUserData */);

	/*	
		Show the updated intermediate code.
	*/
	TESTONLY_PRINT_INTERMEDIATE("Replaced Immediates By Vector Constants", "vec_imm", IMG_FALSE);
}

IMG_VOID static FixUnsupportedVectorMasksInst(PINTERMEDIATE_STATE	psState,
											  PCODEBLOCK			psBlock,
											  PINST					psInst)
/*****************************************************************************
 FUNCTION	: FixUnsupportedVectorMasks
    
 PURPOSE	: Check for and fix vector instructions which
			  have destination write masks not supported by the hardware.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Basic block contain
			  psInst			- Instruction to check.
			  uDestIdx			- Index of the destination register.
			  uRealDestMask		- Mask of the channels we want to write.

 RETURNS	: The mask of channels which would actually be written.
*****************************************************************************/
{
	IMG_UINT32	uDestMask;
	IMG_UINT32	uClobberedChans;
	IMG_UINT32	uChansAlreadyLive;
	IMG_UINT32	uCopySwizzle = USC_UNDEF;
	IMG_UINT32	uNewDestMask;
	IMG_BOOL	bInvalidDestMask;
	IMG_UINT32	uNewLiveChans;

	/*
		Only fixing vector instructions.
	*/
	if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTOR) == 0)
	{
		return;
	}

	/*
		Skip instructions not writing a destination.
	*/
	if (psInst->uDestCount == 0)
	{
		return;
	}

	/* 
		Skip this instructions, which always writes all channels of its destinations. 
	*/
	if ((psInst->eOpcode == IVTEST) || (psInst->eOpcode == IVTESTBYTEMASK))
	{
		return;
	}

	ASSERT(psInst->uDestCount == 1);
	
	/*
		Get the mask of channels we are intending to write.
	*/
	uDestMask = psInst->auDestMask[0];

	bInvalidDestMask = IMG_FALSE;
	uNewLiveChans = psInst->auLiveChansInDest[0];
	if (psInst->eOpcode == IVDP2)
	{
		/*
			For VDP2 we have the special restriction that it can't write the W channel
			of the destination. 
			
			Because the instruction actually produces a scalar result which is broadcast 
			to every channel in the destination change it to write the X channel in a new 
			intermediate register then copy from the X channel of new destination to every 
			channel in the old destination.
		*/
		if ((uDestMask & USC_W_CHAN_MASK) != 0)
		{
			bInvalidDestMask = IMG_TRUE;  
			uNewDestMask = USC_X_CHAN_MASK;
			uCopySwizzle = USEASM_SWIZZLE(X, X, X, X);
			uNewLiveChans = USC_X_CHAN_MASK;
		}
		else
		{
			uNewDestMask = uDestMask;
		}
	}
	else
	{
		/*
			Get the mask of channels which will be overwritten
			by the instruction.
		*/
		uClobberedChans = GetMinimalDestinationMask(psState, psInst, 0 /* uDestIdx */, uDestMask);
	
		/*
			If the channels that will be clobbered by the instruction are 
			live beforehand then change the instruction to write to
			a new register and apply the mask in a seperate instruction.
		*/	
		uChansAlreadyLive = psInst->auLiveChansInDest[0] & ~uDestMask;
		if (uChansAlreadyLive & uClobberedChans)
		{
			bInvalidDestMask = IMG_TRUE;
			uCopySwizzle = USEASM_SWIZZLE(X, Y, Z, W);
			uNewLiveChans = psInst->auDestMask[0];
		}

		/*
			Update the instruction's destination mask to the channels which
			will actually be written.
		*/
		uNewDestMask = uClobberedChans;
	}

	if (bInvalidDestMask)
	{
		PINST			psPCKInst;
		IMG_UINT32		uMaskTemp;
		UF_REGFORMAT	eFmt = psInst->asDest[0].eFmt;

		/*
			Get a new temporary register for the instruction result.
		*/
		uMaskTemp = GetNextRegister(psState);

		/*
			Create a new instruction which moves the new temporary register to the 
			old destination and applies the mask.
		*/
		psPCKInst = AllocateInst(psState, psInst);

		SetOpcode(psState, psPCKInst, IVMOV);

		MoveDest(psState, psPCKInst, 0 /* uMoveToDestIdx */, psInst, 0 /* uMoveFromDestIdx */);
		MovePartiallyWrittenDest(psState, psPCKInst, 0 /* uMoveToDestIdx */, psInst, 0 /* uMoveFromDestIdx */);
		psPCKInst->auDestMask[0] = psInst->auDestMask[0];
		psPCKInst->auLiveChansInDest[0] = psInst->auLiveChansInDest[0];

		SetupTempVecSource(psState, psPCKInst, 0 /* uArgIdx */, uMaskTemp, eFmt);
		psPCKInst->u.psVec->auSwizzle[0] = uCopySwizzle;
		
		/*
			Copy the predicate from the original instruction.
		*/
		CopyPredicate(psState, psPCKInst, psInst);

		/*
			Insert the new instruction after the original instruction.
		*/
		InsertInstBefore(psState, psBlock, psPCKInst, psInst->psNext);

		/*
			Replace the original instruction's destination by the
			new register.
		*/
		SetDest(psState, psInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uMaskTemp, eFmt);
	}

	psInst->auDestMask[0] = uNewDestMask;
	psInst->auLiveChansInDest[0] = uNewLiveChans;
}

IMG_INTERNAL
IMG_VOID ExpandInvalidVectorPackToFloat(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: ExpandInvalidVectorPackToFloat
    
 PURPOSE	: Check for and fix vector unpack instructions with floating point
			  destination formats which have destination write masks not supported by the hardware.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.

 RETURNS	: None.
*****************************************************************************/
{
	UF_REGFORMAT	eDestFmt;
	IMG_UINT32		uRepackTemp = USC_UNDEF;
	IMG_UINT32		uChansPerInst;
	IMG_UINT32		uChan;
	IMG_UINT32		uChanMask;
	IMG_UINT32		uDestMask;

	if (psInst->eOpcode == IVPCKFLTU8 && psInst->u.psVec->bPackScale)
	{
		return;
	}
	ASSERT(psInst->eOpcode == IVPCKFLTU8 || 
		   psInst->eOpcode == IVPCKFLTS8 ||
		   psInst->eOpcode == IVPCKFLTS16 ||
		   psInst->eOpcode == IVPCKFLTU16);

	/*
		Get the number of channels which can be packed in a single hardware instruction.
	*/
	eDestFmt = psInst->asDest[0].eFmt;
	if (eDestFmt == UF_REGFORMAT_F32)
	{
		uChansPerInst = 1;
	}
	else
	{
		ASSERT(eDestFmt == UF_REGFORMAT_F16);
		uChansPerInst = 2;
	}
	uChanMask = (1 << uChansPerInst) - 1;

	/*
		Is the instruction already valid?
	*/
	uDestMask = psInst->auDestMask[0];
	if ((uDestMask & ~uChanMask) == 0)
	{
		return;
	}

	for (uChan = 0; uChan < VECTOR_LENGTH; uChan += uChansPerInst)
	{
		if ((uDestMask & (uChanMask << uChan)) != 0)
		{
			IMG_BOOL	bFirst, bLast;
			IMG_UINT32	uSrcChan;
			ARG			sUnpackTemp;
			PINST		psVPackInst;
			PINST		psVMovInst;

			/*
				Is this the first channel in the destination mask?
			*/
			bFirst = IMG_FALSE;
			if ((uDestMask & ((1 << uChan) - 1)) == 0)
			{
				bFirst = IMG_TRUE;
			}

			/*
				Is this the last channel in the destination mask?
			*/
			bLast = IMG_FALSE;
			if ((uDestMask & ~((1 << (uChan + uChansPerInst)) - 1)) == 0)
			{
				bLast = IMG_TRUE;
			}

			/*
				Get the channel in the source argument which is the source for
				the data written into this channel in the destination.
			*/
			uSrcChan = GetChan(psInst->u.psVec->auSwizzle[0], uChan);

			MakeNewTempArg(psState, eDestFmt, &sUnpackTemp);

			/*
				Create a PCK instruction which is identical to the original but
				writes into the X or XY channels.
			*/
			psVPackInst = AllocateInst(psState, psInst);
			SetOpcode(psState, psVPackInst, psInst->eOpcode);

			psVPackInst->u.psVec->bPackScale = psInst->u.psVec->bPackScale;

			SetDestFromArg(psState, psVPackInst, 0 /* uDestIdx */, &sUnpackTemp);
			psVPackInst->auDestMask[0] = uChanMask;
			SetSrcFromArg(psState, psVPackInst, 0 /* uMoveToSrcIdx */, &psInst->asArg[0]);
			psVPackInst->u.psVec->auSwizzle[0] = g_auReplicateSwizzles[uSrcChan];
			psVPackInst->u.psVec->aeSrcFmt[0] = psInst->u.psVec->aeSrcFmt[0];

			InsertInstBefore(psState, psInst->psBlock, psVPackInst, psInst);

			/*
				Create a VMOV from the X channel of the temporary containing the result of  
				unpack to the destination of the original pack instruction.
			*/
			psVMovInst = AllocateInst(psState, psInst);
			SetOpcode(psState, psVMovInst, IVMOV);

			CopyPredicate(psState, psVMovInst, psInst);

			if (bFirst)
			{
				CopyPartiallyWrittenDest(psState, psVMovInst, 0 /* uMoveToDestIdx */, psInst, 0 /* uMoveFromDestIdx */);
			}
			else
			{
				SetPartialDest(psState, 
							   psVMovInst, 
							   0 /* uDestIdx */,
							   USEASM_REGTYPE_TEMP, 
							   uRepackTemp, 
							   eDestFmt);
			}
			if (bLast)
			{
				MoveDest(psState, psVMovInst, 0 /* uMoveToDestIdx */, psInst, 0 /* uMoveFromDestIdx */);
			}
			else
			{
				uRepackTemp = GetNextRegister(psState);
				SetDest(psState, psVMovInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uRepackTemp, eDestFmt);
			}
			psVMovInst->auDestMask[0] = uChanMask << uChan;
			SetSrcFromArg(psState, psVMovInst, 0 /* uSrcIdx */, &sUnpackTemp);
			if (uChansPerInst == 1)
			{
				psVMovInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, X, X, X);
			}
			else
			{
				psVMovInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, X, Y);
			}
			psVMovInst->u.psVec->aeSrcFmt[0] = sUnpackTemp.eFmt;
				
			InsertInstBefore(psState, psInst->psBlock, psVMovInst, psInst);
		}
	}

	RemoveInst(psState, psInst->psBlock, psInst);
	FreeInst(psState, psInst);
}

IMG_INTERNAL
IMG_VOID ExpandInvalidVectorPackToFixedPoint(PINTERMEDIATE_STATE	psState, 
											 PINST					psInst)
/*****************************************************************************
 FUNCTION	: ExpandInvalidVectorPackToFixedPoint
    
 PURPOSE	: Check for and fix vector unpack instructions which
			  have destination write masks not supported by the hardware.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uDestMask;
	IOPCODE		eOpcode = psInst->eOpcode;
	ARG			sXYTempArg;
	PINST		apsPckInst[2];
	IMG_UINT32	uInst;
	IMG_UINT32	uChan;
	IMG_UINT32	uNewSwizzle;
	IMG_BOOL	bReplicateSwizzle;

	if (eOpcode == IVPCKU8FLT && psInst->u.psVec->bPackScale)
	{
		return;
	}
	ASSERT(eOpcode == IVPCKU8FLT || eOpcode == IVPCKS8FLT);

	uDestMask = psInst->auDestMask[0];

	/*
		Check if the instruction has any constant selectors in its swizzle.
	*/
	if (!IsNonConstantSwizzle(psInst->u.psVec->auSwizzle[0], uDestMask, &uNewSwizzle))
	{
		PINST		psSwizzleInst;
		ARG			sSwizzleTemp;
		IMG_UINT32	uLiveChansInArg;

		GetLiveChansInSourceSlot(psState, psInst, 0 /* uArgIdx */, &uLiveChansInArg, NULL /* puChansUsedPostSwizzle */);

		MakeNewTempArg(psState, psInst->u.psVec->aeSrcFmt[0], &sSwizzleTemp);

		/*
			Apply the swizzle in a separate instruction.
		*/
		psSwizzleInst = AllocateInst(psState, psInst);

		SetOpcode(psState, psSwizzleInst, IVMOV);

		SetDestFromArg(psState, psSwizzleInst, 0 /* uDestIdx */, &sSwizzleTemp);
		psSwizzleInst->auDestMask[0] = uLiveChansInArg;
		psSwizzleInst->auLiveChansInDest[0] = uLiveChansInArg;

		MoveVectorSource(psState, 
						 psSwizzleInst, 
						 0 /* uDestArgIdx */,
						 psInst, 
						 0 /* uSrcArgIdx */, 
						 IMG_TRUE /* bCopySourceModifier */);

		SetSrcFromArg(psState, psInst, 0 /* uArgIdx */, &sSwizzleTemp);
		psInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
		psInst->u.psVec->asSrcMod[0].bNegate = IMG_FALSE;
		psInst->u.psVec->asSrcMod[0].bAbs = IMG_FALSE;

		InsertInstBefore(psState, psInst->psBlock, psSwizzleInst, psInst);
	}
	else
	{
		psInst->u.psVec->auSwizzle[0] = uNewSwizzle;
	}

	/*
		No restrictions on instructions writing one or two channels.
	*/
	if (g_auSetBitCount[uDestMask] <= 2)
	{
		return;
	}

	/*
		Check if the swizzle is of the form XYXY
	*/
	bReplicateSwizzle = IMG_TRUE;
	for (uChan = 0; uChan < 2; uChan++)
	{
		IMG_UINT32			uChanFromOtherHalf = 2 + uChan;

		if ((uDestMask & (1 << uChan)) != 0 && (uDestMask & (1 << uChanFromOtherHalf)) != 0)
		{
			USEASM_SWIZZLE_SEL	eSel1;
			USEASM_SWIZZLE_SEL	eSel2;

			eSel1 = GetChan(psInst->u.psVec->auSwizzle[0], uChan);
			eSel2 = GetChan(psInst->u.psVec->auSwizzle[0], uChanFromOtherHalf);
			if (eSel1 != eSel2)
			{
				bReplicateSwizzle = IMG_FALSE;
				break;
			}
		}
	}

	if (bReplicateSwizzle)
	{
		/*
			If the instruction doesn't need to preserve the value of the unwritten channel
			then just set the mask to write all channels.
		*/
		if (uDestMask == psInst->auLiveChansInDest[0])
		{
			psInst->auDestMask[0] = USC_ALL_CHAN_MASK;
			return;
		}
	}

	/*
		Split the instructions into two instructions: one writing two channels
		and one writing one.
	*/
	for (uInst = 0; uInst < 2; uInst++)
	{
		PINST	psPckInst;

		apsPckInst[uInst] = psPckInst = CopyInst(psState, psInst);
		if (uInst == 0)
		{
			psPckInst->auDestMask[0] = psInst->auDestMask[0] & USC_XY_CHAN_MASK;
			ASSERT(psPckInst->auDestMask[0] != 0);

			MakeNewTempArg(psState, psInst->asDest[0].eFmt, &sXYTempArg);
			SetDestFromArg(psState, psPckInst, 0 /* uDestIdx */, &sXYTempArg);
		}
		else
		{
			psPckInst->auDestMask[0] = psInst->auDestMask[0] & USC_ZW_CHAN_MASK;
			ASSERT(psPckInst->auDestMask[0] != 0);

			SetPartiallyWrittenDest(psState, psPckInst, 0 /* uDestIdx */, &sXYTempArg);
			MoveDest(psState, psPckInst, 0 /* uMoveToDestIdx */, psInst, 0 /* uMoveFromDestIdx */);
		}

		InsertInstBefore(psState, psInst->psBlock, psPckInst, psInst);
	}
	
	apsPckInst[0]->auLiveChansInDest[0] = GetPreservedChansInPartialDest(psState, apsPckInst[1], 0 /* uDestIdx */);

	RemoveInst(psState, psInst->psBlock, psInst);
	FreeInst(psState, psInst);
}

static
IMG_VOID FixUnsupportedVectorMasksBP(PINTERMEDIATE_STATE	psState,
									 PCODEBLOCK				psCodeBlock,
									 IMG_PVOID				pvNULL)
/*****************************************************************************
 FUNCTION	: FixUnsupportedVectorMasksBP
    
 PURPOSE	: Check for and fix vector instructions with unsupported 
			  destination write masks in a basic block.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to fix instructions in.
			  pvNULL			- Ignored.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psInst;

	PVR_UNREFERENCED_PARAMETER(pvNULL);

	for (psInst = psCodeBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		FixUnsupportedVectorMasksInst(psState, psCodeBlock, psInst);
	}
}

IMG_INTERNAL
IMG_VOID FixUnsupportedVectorMasks(PINTERMEDIATE_STATE	psState)
/*****************************************************************************
 FUNCTION	: FixUnsupportedVectorMasks
    
 PURPOSE	: Check for and fix vector instructions with unsupported 
			  destination write masks in the program.

 PARAMETERS	: psState			- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psState->uFlags |= USC_FLAGS_FIXEDVECTORWRITEMASKS;

	/*
		Apply the replacement to all basic blocks.
	*/
	DoOnAllBasicBlocks(psState, ANY_ORDER, FixUnsupportedVectorMasksBP, IMG_FALSE /* CALLS */, NULL /* pvUserData */);

	/*	
		Show the updated intermediate code.
	*/
	TESTONLY_PRINT_INTERMEDIATE("Fix Unsupported Vector Masks", "vec_msk", IMG_FALSE);
}

IMG_INTERNAL
IMG_VOID ExpandPCKU8U8Inst(PINTERMEDIATE_STATE psState, PINST psOrigPCKInst)
/*****************************************************************************
 FUNCTION	: ExpandPCKU8U8Inst
    
 PURPOSE	: Expand a PCKU8U8 instruction to instructions which are supported
			  on cores with the vector instruction set.

 PARAMETERS	: psState			- Compiler state.
			  psOrigPCKInst		- Instruction to expand.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uChan;
	IMG_UINT32	uSrcIdx;
	IMG_UINT32	uAlreadyWrittenMask;
	IMG_BOOL	bFirst;
	PINST		psPrevPCKInst;
	PCODEBLOCK	psCodeBlock = psOrigPCKInst->psBlock;

	if (EqualArgs(&psOrigPCKInst->asArg[0], &psOrigPCKInst->asArg[1]))
	{
		/* 
			This is a special case. If doing IUNPCKU8U8 when src1 == src2,
			then can implement as a single IVPCKU8U8 instruction
		*/
		PINST psVPCKInst = AllocateInst(psState, psOrigPCKInst);
		IMG_UINT32 uSourceComponent1 = GetPCKComponent(psState, psOrigPCKInst, 0);
		IMG_UINT32 uSourceComponent2 = GetPCKComponent(psState, psOrigPCKInst, 1);

		SetOpcode(psState, psVPCKInst, IVPCKU8U8);

		CopyPartiallyWrittenDest(psState, psVPCKInst, 0 /* uMoveToDestIdx */, psOrigPCKInst, 0 /* uMoveFromDestIdx */);
		MoveDest(psState, psVPCKInst, 0 /* uMoveToDestIdx */, psOrigPCKInst, 0 /* uMoveFromDestIdx */);
		psVPCKInst->auDestMask[0] = psOrigPCKInst->auDestMask[0];
		psVPCKInst->auLiveChansInDest[0] = psOrigPCKInst->auLiveChansInDest[0];

		CopyPredicate(psState, psVPCKInst, psOrigPCKInst);

		SetSrcFromArg(psState, psVPCKInst, 0 /* uSrcIdx */, &psOrigPCKInst->asArg[0]);

		/* calculate the swizzle for the 2 source channels */
		psVPCKInst->u.psVec->auSwizzle[0] = g_auReplicateSwizzles[uSourceComponent1];
		if (uSourceComponent1 != uSourceComponent2)
		{
			/* need to calculate a swizzle because source channels are different */
			uSrcIdx = 0;
			for (uChan = 0; uChan < 4; uChan++)
			{
				if (psOrigPCKInst->auDestMask[0] & (1 << uChan))
				{
					IMG_UINT32 uComponent = uSourceComponent1;
					if (uSrcIdx == 0)
					{
						uSrcIdx = 1;
					}
					else
					{
						uComponent = uSourceComponent2;
						uSrcIdx = 0;
					}
					psVPCKInst->u.psVec->auSwizzle[0] = SetChan (psVPCKInst->u.psVec->auSwizzle[0],
																 uChan,
																 uComponent);
				}
			}
		}

		InsertInstBefore(psState, psCodeBlock, psVPCKInst, psOrigPCKInst);
	}
	else
	{
		IMG_UINT32	uNotToBeWritten = 0; /* bits not being written by this instruction */
		if (psOrigPCKInst->auDestMask)
		{
			uNotToBeWritten = 0xF & ~(*psOrigPCKInst->auDestMask);
		}

		uSrcIdx = 0;
		uAlreadyWrittenMask = 0;
		bFirst = IMG_TRUE;
		psPrevPCKInst = NULL;
		for (uChan = 0; uChan < 4; uChan++)
		{
			if (psOrigPCKInst->auDestMask[0] & (1 << uChan))
			{
				PINST		psPCKInst;

				psPCKInst = AllocateInst(psState, psOrigPCKInst);

				SetOpcode(psState, psPCKInst, IVPCKU8U8);

				if (psOrigPCKInst->asDest[0].uType == USEASM_REGTYPE_TEMP)
				{
					IMG_BOOL	bLast;

					bLast = IMG_FALSE;
					if ((psOrigPCKInst->auDestMask[0] >> (uChan + 1)) == 0)
					{
						bLast = IMG_TRUE;
					}

					/*
						Expand
							PCKU8U8		DEST.XYW[=OLDDEST], ...
						->
							VPCKU8U8	TEMP1.X[=OLDDEST], ...
							VPCKU8U8	TEMP2.Y[=TEMP1], ...
							VPCKU8U8	DEST.W[=TEMP2], ...
					*/
					if (bFirst)
					{
						CopyPartiallyWrittenDest(psState, 
												 psPCKInst, 
												 0 /* uCopyToDestIdx */, 
												 psOrigPCKInst, 
												 0 /* uCopyFromDestIdx */);
						bFirst = IMG_FALSE;
					}
					else
					{
						SetPartiallyWrittenDest(psState, psPCKInst, 0 /* uDestIdx */, &psPrevPCKInst->asDest[0]);
					}
					if (bLast)
					{
						MoveDest(psState, psPCKInst, 0 /* uMoveToDestIdx */, psOrigPCKInst, 0 /* uMoveFromDestIdx */);
					}
					else
					{
						IMG_UINT32	uTempDest = GetNextRegister(psState);
						psPrevPCKInst = psPCKInst;
						SetDest(psState, psPCKInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uTempDest, UF_REGFORMAT_U8);
					}
				}
				else
				{
					CopyPartiallyWrittenDest(psState, psPCKInst, 0 /* uCopyToDestIdx */, psOrigPCKInst, 0 /* uCopyFromDestIdx */);
					SetDestFromArg(psState, psPCKInst, 0 /* uDestIdx */, &psOrigPCKInst->asDest[0]);
				}

				psPCKInst->auDestMask[0] = 1 << uChan;

				uAlreadyWrittenMask |= psPCKInst->auDestMask[0];

				psPCKInst->auLiveChansInDest[0] = psOrigPCKInst->auLiveChansInDest[0] & (uAlreadyWrittenMask|uNotToBeWritten);

				CopyPredicate(psState, psPCKInst, psOrigPCKInst);

				SetSrcFromArg(psState, psPCKInst, 0 /* uSrcIdx */, &psOrigPCKInst->asArg[uSrcIdx]);

				psPCKInst->u.psVec->auSwizzle[0] = 
					g_auReplicateSwizzles[GetPCKComponent(psState, psOrigPCKInst, uSrcIdx)];

				InsertInstBefore(psState, psCodeBlock, psPCKInst, psOrigPCKInst);
				
				uSrcIdx = (uSrcIdx + 1) % 2;
			}
		}
	}

	/*
		Drop the original instruction.
	*/
	RemoveInst(psState, psCodeBlock, psOrigPCKInst);
	FreeInst(psState, psOrigPCKInst);
}

IMG_INTERNAL
IMG_BOOL SwapVectorDestinations(PINTERMEDIATE_STATE	psState,
							    PINST				psInst,
								IMG_UINT32			uCurrentChan,
								IMG_UINT32			uNewChan,
								IMG_BOOL			bCheckOnly)
/*****************************************************************************
 FUNCTION	: SwapVectorDestinations
    
 PURPOSE	: Update an instruction so the calculation for the second
			  destination is the same as the old calculation for the
			  first destination.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to swap for.
			  bCheckOnly		- If TRUE only check if swapping is possible.
								  If FALSE update the instruction.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uNewChanMask = 1 << uNewChan;

	switch (psInst->eOpcode)
	{
		case IVDP:
		case IVDP2:
		case IVDP3:
		case IVDP_GPI:
		case IVDP3_GPI:
		case IVDP_CP:
		case IVRSQ:
		case IVRCP:
		case IVLOG:
		case IVEXP:
		{
			/*
				These instructions calculate a scalar result which is broadcast to
				all channels so we can freely update the mask.
			*/
			return IMG_TRUE;
		}
		case IVMUL:
		case IVMUL3:
		case IVADD:
		case IVADD3:
		case IVFRC:
		case IVDSX:
		case IVDSY:
		case IVDSX_EXP:
		case IVDSY_EXP:
		case IVMIN:
		case IVMAX:
		case IVMAD:
		case IVMAD4:
		case IVMOV:
		{
			IMG_UINT32	uSwizzleSlotIdx;
			
			for (uSwizzleSlotIdx = 0; uSwizzleSlotIdx < GetSwizzleSlotCount(psState, psInst); uSwizzleSlotIdx++)
			{
				IMG_UINT32	uCurrentChanSel;
				IMG_UINT32	uNewChanSwizzle;
				IMG_BOOL	bSupportedSwizzle;

				/*
					Get the source data selecting for the current channel used from this source.
				*/
				uCurrentChanSel = GetChan(psInst->u.psVec->auSwizzle[uSwizzleSlotIdx], uCurrentChan);

				/*
					Form a new swizzle selecting the same data into the other channel.
				*/
				uNewChanSwizzle = g_auReplicateSwizzles[uCurrentChanSel];

				/*
					Check this swizzle is supported by the instruction.
				*/
				bSupportedSwizzle = 
					IsSwizzleSupported(psState, 
									   psInst,
									   psInst->eOpcode, 
									   uSwizzleSlotIdx, 
									   uNewChanSwizzle, 
									   uNewChanMask, 
									   &uNewChanSwizzle);
				if (!bSupportedSwizzle)
				{
					ASSERT(bCheckOnly);
					return IMG_FALSE;
				}
				
				/*
					Update the swizzle in the instruction.
				*/
				if (!bCheckOnly)
				{
					psInst->u.psVec->auSwizzle[uSwizzleSlotIdx] = uNewChanSwizzle;
				}
			}
			return IMG_TRUE;
		}
		default:
		{
			return IMG_FALSE;
		}
	}
}

static
IMG_BOOL IsValidModifiedSplitInst(PINTERMEDIATE_STATE	psState, 
								  PINST					psVecInst, 
								  IMG_UINT32			auNewPreSwizzleLiveChans[],
								  IOPCODE				eNewOpcode,
								  IMG_BOOL				bNeverUseWideSource,
								  IMG_BOOL				bSwapSources01,
								  PVEC_NEW_ARGUMENTS	psNewArguments,
								  IMG_UINT32			auNewSwizzle[],
								  IMG_BOOL				abSelectUpper[],
								  PINST_MODIFICATIONS	psInstMods)
/*****************************************************************************
 FUNCTION     : IsValidModifiedSplitInst
    
 PURPOSE      : Check if a modified version of a part of a split instruction 
				would still be valid.

 PARAMETERS   : psState			- Compiler state.
			    psVecInst		- Instruction to split.
				psIRegAssign	- Details of which sources/destinations are internal
								registers in the new instruction.
				auNewLiveChansInDest
								- Mask of channels used from the result of the instruction.
				eNewOpcode		- Opcode to check.
				bSwapSources01	- Check the instruction with the first two
								sources swapped.
				auNewSwizzle	- New swizzles for this instruction.
				aeNewSrcFormat	
				psInstMods		- Returns details of the modification to the
								instruction.

 RETURNS      : TRUE if the instruction is valid.
*****************************************************************************/
{
	IMG_UINT32		uIRegOnlySourceMask = GetIRegOnlySourceMask(eNewOpcode);
	IMG_UINT32		uWideSlotMask = GetWideSourceMask(eNewOpcode);
	IMG_UINT32		uOutSlot;
	IMG_UINT32		auSwizzleSetToCheck[VECTOR_MAX_SOURCE_SLOT_COUNT];
	IMG_BOOL		abNewSelectUpper[VECTOR_MAX_SOURCE_SLOT_COUNT];
	IMG_BOOL		bF32UnifiedStore = IMG_FALSE;
	IMG_BOOL		bF16UnifiedStore = IMG_FALSE;
	IMG_UINT32		uDest;

	/*
		Record the interpretation of non-internal register destinations.
	*/
	ASSERT(psVecInst->uDestCount <= VECTOR_MAX_DESTINATION_COUNT);
	for (uDest = 0; uDest < psNewArguments->uDestCount; uDest++)
	{
		PVEC_NEW_ARGUMENT	psDest = &psNewArguments->asDest[uDest];

		if (!psDest->bIsIReg)
		{
			UF_REGFORMAT	eDestFormat = psDest->eFormat;

			if (eDestFormat == UF_REGFORMAT_F32)
			{
				bF32UnifiedStore = IMG_TRUE;
			}
			else if (eDestFormat == UF_REGFORMAT_F16)
			{
				bF16UnifiedStore = IMG_TRUE;
			}
		}
	}

	/*
		Check for instructions with destinations which can only be internal
		registers.
	*/
	if (psVecInst->eOpcode == IVDUAL)
	{
		PVEC_NEW_ARGUMENT	psDest;

		ASSERT(VDUAL_DESTSLOT_GPI < psVecInst->uDestCount);
		psDest = &psNewArguments->asDest[VDUAL_DESTSLOT_GPI];
		if (!psDest->bIsIReg)
		{
			return IMG_FALSE;
		}
	}

	for (uOutSlot = 0; uOutSlot < GetSwizzleSlotCount(psState, psVecInst); uOutSlot++)
	{
		IMG_UINT32			uInSlot;
		PVEC_NEW_ARGUMENT	psSrc;

		if (bSwapSources01 && (uOutSlot == 0 || uOutSlot == 1))
		{
			uInSlot = 1 - uOutSlot;
		}
		else
		{
			uInSlot = uOutSlot;
		}

		auSwizzleSetToCheck[uOutSlot] = auNewSwizzle[uInSlot];
		abNewSelectUpper[uOutSlot] = abSelectUpper[uInSlot];

		psSrc = &psNewArguments->asSrc[uInSlot];

		if (!psSrc->bIsIReg)
		{
			UF_REGFORMAT	eSrcFormat = psSrc->eFormat;

			/*	
				Check the new instruction would still fit with restrictions on which
				sources must be GPI registers.
			*/
			if ((uIRegOnlySourceMask & (1 << uOutSlot)) != 0)
			{
				return IMG_FALSE;
			}

			/*
				Record the interpretation of non-internal register sources.
			*/
			if (eSrcFormat == UF_REGFORMAT_F32)
			{
				bF32UnifiedStore = IMG_TRUE;
			}
			else if (eSrcFormat == UF_REGFORMAT_F16)
			{
				bF16UnifiedStore = IMG_TRUE;
			}
		
			/*
				Check restrictions on accessing vec4 F32 format sources from the unified store.
			*/
			if (eSrcFormat == UF_REGFORMAT_F32)
			{
				IMG_UINT32	uPostSwizzleLiveChanMask;

				uPostSwizzleLiveChanMask = ApplySwizzleToMask(auSwizzleSetToCheck[uOutSlot], auNewPreSwizzleLiveChans[uInSlot]);
				if (abNewSelectUpper[uOutSlot])
				{
					ASSERT((uPostSwizzleLiveChanMask & USC_ZW_CHAN_MASK) == 0);
					uPostSwizzleLiveChanMask <<= USC_Z_CHAN;
				}
	
				if ((uPostSwizzleLiveChanMask & USC_XY_CHAN_MASK) != 0 && (uPostSwizzleLiveChanMask & USC_ZW_CHAN_MASK) != 0)
				{
					ASSERT(!abNewSelectUpper[uOutSlot]);
	
					/*
						If a full 128-bit vector is referenced then check the instruction supports that in this slot.
					*/
					if ((uWideSlotMask & (1 << uOutSlot)) == 0)
					{
						return IMG_FALSE;
					}
				}
				else if ((uPostSwizzleLiveChanMask & USC_ZW_CHAN_MASK) != 0)
				{
					/*
						If the instruction references only Z/W then we can select the upper 64-bits of a unified
						store source provided a swizzle with Z replaced by X and W replaced by Y is supported.
					*/
					if (!abNewSelectUpper[uOutSlot] && ((uWideSlotMask & (1 << uOutSlot)) == 0 || bNeverUseWideSource))
					{
						IMG_UINT32	uSwizzle;
		
						uSwizzle = auSwizzleSetToCheck[uOutSlot];
						auSwizzleSetToCheck[uOutSlot] = CombineSwizzles(USEASM_SWIZZLE(0, 0, X, Y), uSwizzle);
						abNewSelectUpper[uOutSlot] = IMG_TRUE;
					}
				}
			}

			/*
				Check if the new opcode supports only F32 format sources.
			*/
			if ((g_psInstDesc[eNewOpcode].uFlags2 & DESC_FLAGS2_VECTORF32ONLY) != 0)
			{
				if (eSrcFormat != UF_REGFORMAT_F32)
				{
					return IMG_FALSE;
				}
			}
		}

		/*
			Check the modifier on this source is supported by the new instruction.
		*/
		if (!IsValidVectorSourceModifier(psVecInst, eNewOpcode, uOutSlot, &psVecInst->u.psVec->asSrcMod[uInSlot]))
		{
			return IMG_FALSE;
		}
	}

	/*
		Check the new set of swizzles is supported by the split instruction.
	*/
	if (!IsSwizzleSetSupported(psState, eNewOpcode, psVecInst, auNewPreSwizzleLiveChans, auSwizzleSetToCheck))
	{
		return IMG_FALSE;
	}

	/*
		If this instruction doesn't support per-argument format selection then check that we use only
		one format for non-internal register sources and destinations.
	*/
	if (!VectorInstPerArgumentF16F32Selection(eNewOpcode))
	{
		if (bF32UnifiedStore && bF16UnifiedStore)
		{
			return IMG_FALSE;
		}
	}

	psInstMods->eNewOpcode = eNewOpcode;
	psInstMods->bSwapSources01 = bSwapSources01;
	memcpy(psInstMods->auNewSwizzle, auSwizzleSetToCheck, sizeof(psInstMods->auNewSwizzle));
	memcpy(psInstMods->abNewSelUpper, abNewSelectUpper, sizeof(psInstMods->abNewSelUpper));

	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL IsValidModifiedVectorInst_Opcode(PINTERMEDIATE_STATE	psState,
										  PINST					psVecInst,
										  IOPCODE				eOpcode,
										  IMG_BOOL				bNeverUseWideSource,
										  IMG_UINT32			auNewPreSwizzleLiveChans[],
										  PVEC_NEW_ARGUMENTS	psNewArguments,
										  IMG_UINT32			auNewSwizzle[],
										  IMG_BOOL				abSelectUpper[],
										  PINST_MODIFICATIONS	psInstMods)
{
	/*
		Check the new swizzles for the split instruction are supported.
	*/
	if (IsValidModifiedSplitInst(psState, 
								 psVecInst, 
								 auNewPreSwizzleLiveChans,
								 eOpcode, 
								 bNeverUseWideSource,
								 IMG_FALSE /* bSwapSources01 */, 
								 psNewArguments,
								 auNewSwizzle,
								 abSelectUpper,
								 psInstMods))
	{
		return IMG_TRUE;
	}

	/*
		Check if the new set of swizzles would be supported if we swapped the first two sources.
	*/
	if (VectorSources12Commute(psVecInst))
	{
		if (IsValidModifiedSplitInst(psState, 
									 psVecInst, 
									 auNewPreSwizzleLiveChans,
									 eOpcode, 
									 bNeverUseWideSource,
									 IMG_TRUE /* bSwapSources01 */, 
									 psNewArguments,
									 auNewSwizzle,
									 abSelectUpper,
									 psInstMods))
		{
			return IMG_TRUE;
		}
	}

	return IMG_FALSE;
}

IMG_INTERNAL
IMG_BOOL IsValidModifiedVectorInst(PINTERMEDIATE_STATE	psState,
								   IMG_BOOL				bExtraEffort,
								   PINST				psVecInst,
								   IMG_UINT32			auNewPreSwizzleLiveChans[],
								   PVEC_NEW_ARGUMENTS	psNewArguments,
								   IMG_UINT32			auNewSwizzle[],
								   IMG_BOOL				abNewSelectUpper[],
								   PINST_MODIFICATIONS	psInstMods)
/*****************************************************************************
 FUNCTION     : IsValidModifiedVectorInst
    
 PURPOSE      : Check if a vector instruction would still be valid after replacing
				some sources which are internal registers by unified store registers.

 PARAMETERS   : psState			- Compiler state.
				bExtraEffort	- If true apply modifications to the instruction which
								might block replacing other sources by internal registers
								later on.
			    psVecInst		- Instruction to check.
				auNewPreSwizzleLiveChans
								- Mask of channels used from the each source before the
								swizzle is applied.
				psNewArguments	- Which source/destinations will be assigned
								internal registers in the instruction and
								also their formats. 
			    auNewSwizzle	- New swizzles for the instruction sources.

 RETURNS      : TRUE if the instruction is still valid.
*****************************************************************************/
{
	/*
		Check if the instruction is valid with its original opcode and the
		sources either in their original order or with the first two swapped.
	*/
	if (IsValidModifiedVectorInst_Opcode(psState,
										 psVecInst,
										 psVecInst->eOpcode,
										 IMG_FALSE /* bNeverUseWideSource */,
										 auNewPreSwizzleLiveChans,
										 psNewArguments,
										 auNewSwizzle,
										 abNewSelectUpper,
										 psInstMods))
	{
		return IMG_TRUE;
	}

	/*
		For a VTEST instruction where the swizzles on the two sources can't be set
		independently check if accessing a vec2 from both sources allows the 
		instruction to be valid.

		Similarly for VTESTMASK which doesn't support swizzles at all.
	*/
	if (IsVTESTExtraSwizzles(psState, psVecInst->eOpcode, psVecInst) || psVecInst->eOpcode == IVTESTMASK)
	{		
		if (IsValidModifiedVectorInst_Opcode(psState,
											 psVecInst,
											 psVecInst->eOpcode,
											 IMG_TRUE /* bNeverUseWideSource */,
											 auNewPreSwizzleLiveChans,
											 psNewArguments,
											 auNewSwizzle,
											 abNewSelectUpper,
											 psInstMods))
		{
			return IMG_TRUE;
		}
	}

	/*
		For VMAD check if the swizzles can be supported by switching to the GPI
		oriented variant.
	*/
	if (psVecInst->eOpcode == IVMAD || psVecInst->eOpcode == IVMAD4)
	{
		IOPCODE	eAltOpcode = (psVecInst->eOpcode == IVMAD) ? IVMAD4 : IVMAD;

		if (IsValidModifiedVectorInst_Opcode(psState,
											 psVecInst,
											 eAltOpcode,
											 IMG_FALSE /* bNeverUseWideSource */,
											 auNewPreSwizzleLiveChans,
											 psNewArguments,
											 auNewSwizzle,
											 abNewSelectUpper,
											 psInstMods))
		{
			return IMG_TRUE;
		}
	}

	if (bExtraEffort)
	{
		/*
			Switch VDP3 to VDP3_GPI since the later has access to a larger palette of swizzles.
		*/
		if (psVecInst->eOpcode == IVDP3 && (psVecInst->auDestMask[0] & (~USC_XYZ_CHAN_MASK)) == 0)
		{
			if (IsValidModifiedVectorInst_Opcode(psState,
												 psVecInst,
												 IVDP3_GPI,
												 IMG_FALSE /* bNeverUseWideSource */,
												 auNewPreSwizzleLiveChans,
												 psNewArguments,
												 auNewSwizzle,
												 abNewSelectUpper,
												 psInstMods))
			{
				return IMG_TRUE;
			}
		}

		/*
			For

				VADD	DEST, -A, B
		
			we can't fold B because only source 0 can be wide but source 1 doesn't support a negate modifier. So try
			swapping to VADD3 DEST, B, -A (which expands to VMAD3 DEST, B, 1, -A).
		*/
		if (
				psVecInst->eOpcode == IVADD && 
				(psVecInst->auDestMask[0] & (~USC_XYZ_CHAN_MASK)) == 0 &&
				psNewArguments->asSrc[0].bIsIReg &&
				!psNewArguments->asSrc[1].bIsIReg &&
				psVecInst->u.psVec->asSrcMod[0].bNegate
			)
		{
			if (IsValidModifiedSplitInst(psState, 
										 psVecInst, 
										 auNewPreSwizzleLiveChans,
										 IVADD3, 
										 IMG_FALSE /* bNeverUseWideSource */,
										 IMG_TRUE /* bSwapSources01 */, 
										 psNewArguments,
										 auNewSwizzle,
										 abNewSelectUpper,
										 psInstMods))
			{
				return IMG_TRUE;
			}
		}
	}

	return IMG_FALSE;
}	

static
IMG_BOOL IsValidSplitInst(PINTERMEDIATE_STATE	psState,
						  PINST					psVecInst,
						  IMG_BOOL				bAdjustSwizzles,
						  PVEC_NEW_ARGUMENTS	psNewArguments,
						  IMG_UINT32			uDestStartChan,
						  PINST_MODIFICATIONS	psInstMods)
/*****************************************************************************
 FUNCTION     : IsValidSplitInst
    
 PURPOSE      : Check if one part of a split instruction would still be
			    valid.

 PARAMETERS   : psState			- Compiler state.
			    psVecInst		- Unsplit instruction.
				bAdjustSwizzles	- TRUE if the instruction does a different
								calculation for every channel in the destination.
				psNewArguments	- Which source/destinations will be assigned
								internal registers in the split instruction and
								also their formats. 
			    uDestStartChan	- Start of the pair of channels in the old destination
								written by this part of the split instruction. Can
								be either the X channel or the Z channel.
				psInstMods		- Returns details of the modification to the instruction.

 RETURNS      : TRUE if the instruction is still valid.
*****************************************************************************/
{
	IMG_UINT32	uSlotIdx;
	IMG_UINT32	uNewDestMask;
	IMG_UINT32	auNewSwizzle[VECTOR_MAX_SOURCE_SLOT_COUNT];
	IMG_BOOL	abSelectUpper[VECTOR_MAX_SOURCE_SLOT_COUNT];
	IMG_UINT32	auNewPreSwizzleLiveChans[VECTOR_MAX_SOURCE_SLOT_COUNT];

	ASSERT(psVecInst->uDestCount == 1);
	uNewDestMask = (psVecInst->auDestMask[0] >> uDestStartChan) & USC_XY_CHAN_MASK;

	if (uNewDestMask == 0)
	{
		return IMG_TRUE;
	}

	for (uSlotIdx = 0; uSlotIdx < GetSwizzleSlotCount(psState, psVecInst); uSlotIdx++)
	{
		IMG_UINT32	uSwizzle;

		uSwizzle = psVecInst->u.psVec->auSwizzle[uSlotIdx];

		if (bAdjustSwizzles)
		{
			/*
				Use a swizzle which selects the data which was used in
				the calculation of the part of the old destination corresponding to
				this part of the split instruction into the X and Y channels.
			*/
			uSwizzle >>= (USEASM_SWIZZLE_FIELD_SIZE * uDestStartChan);	
		}
		
		auNewSwizzle[uSlotIdx] = uSwizzle;

		abSelectUpper[uSlotIdx] = GetBit(psVecInst->u.psVec->auSelectUpperHalf, uSlotIdx);

		/*
			Get the mask of channels used from this source (before the swizzle is applied)
			in the split instruction.
		*/
		BaseGetLiveChansInVectorArgument(psState, 
										 psVecInst, 
										 uSlotIdx * SOURCE_ARGUMENTS_PER_VECTOR, 
										 &uNewDestMask, 
										 &auNewPreSwizzleLiveChans[uSlotIdx],
										 NULL /* puChansUsedPostSwizzle */);
	}

	return IsValidModifiedVectorInst(psState,
									 IMG_FALSE /* bExtraEffort */,
									 psVecInst,
									 auNewPreSwizzleLiveChans,
									 psNewArguments,
									 auNewSwizzle,
									 abSelectUpper,
									 psInstMods);
}

IMG_INTERNAL
IMG_VOID ModifyVectorInst(PINTERMEDIATE_STATE psState, PINST psPartInst, PINST_MODIFICATIONS psInstMods)
/*****************************************************************************
 FUNCTION     : ModifyVectorInst
    
 PURPOSE      : 

 PARAMETERS   : psState			- Compiler state.
			    psPartInst		- Instruction to modify.
				psInstMods		- Modifications.

 RETURNS      : Nothing.
*****************************************************************************/
{
	IMG_UINT32	uSlot;

	/*
		Modify the opcode for this part of the split instruction (if that was necessary
		so it still has valid source swizzles).
	*/
	ModifyOpcode(psState, psPartInst, psInstMods->eNewOpcode);
	if (psInstMods->bSwapSources01)
	{
		SwapVectorSources(psState, psPartInst, 0 /* uArg1Idx */, psPartInst, 1 /* uArg2Idx */);
	}

	/*
		Update the split instruction with new swizzles.
	*/
	for (uSlot = 0; uSlot < GetSwizzleSlotCount(psState, psPartInst); uSlot++)
	{
		psPartInst->u.psVec->auSwizzle[uSlot] = psInstMods->auNewSwizzle[uSlot];

		/*
			Flag that we are selecting the upper 64-bits of a vector in this source.
		*/
		if (psInstMods->abNewSelUpper[uSlot])
		{
			SetBit(psPartInst->u.psVec->auSelectUpperHalf, uSlot, 1);
		}
		else
		{
			SetBit(psPartInst->u.psVec->auSelectUpperHalf, uSlot, 0);
		}
	}
}

static
IMG_BOOL SplitReplicatedVectorInst(PINTERMEDIATE_STATE	psState,
								   PINST				psVecInst,
								   PVEC_NEW_ARGUMENTS	psNewArguments,
								   IMG_BOOL				bCheckOnly,
								   PINST*				ppsXYInst,
								   PINST*				ppsZWInst)
/*****************************************************************************
 FUNCTION     : SplitReplicatedVectorInst
    
 PURPOSE      : Check for special cases where an instruction writes the same
				result into both the XY and ZW channels in its destination.

 PARAMETERS   : psState			- Compiler state.
			    psVecInst		- Original instruction.
				psNewArguments	- Which source/destinations will be assigned
								internal registers in the split instruction and
								also their formats. 
				bCheckOnly		- If TRUE just check whether the instruction
								could be split.
				ppsXYInst		- Returns the split instructions.
				ppsZWInst

 RETURNS      : TRUE if the instruction could be split.
*****************************************************************************/
{
	IMG_UINT32			uArg;
	INST_MODIFICATIONS	sInstMods;
	IMG_UINT32			uZWDestMask;

	/*
		Check for an instruction writing only one half of a register.
	*/
	if ((psVecInst->auDestMask[0] & USC_XY_CHAN_MASK) == 0 || (psVecInst->auDestMask[0] & USC_ZW_CHAN_MASK) == 0)
	{
		return IMG_FALSE;
	}

	/*
		Check the XY and ZW parts of the swizzles are identical.
	*/
	uZWDestMask = psVecInst->auDestMask[0] >> USC_Z_CHAN;
	if ((psVecInst->auDestMask[0] & USC_XY_CHAN_MASK) != uZWDestMask)
	{
		return IMG_FALSE;
	}
	for (uArg = 0; uArg < GetSwizzleSlotCount(psState, psVecInst); uArg++)
	{
		IMG_UINT32	uSwizzle, uZWSwizzle;

		uSwizzle = psVecInst->u.psVec->auSwizzle[uArg];
		uZWSwizzle = uSwizzle >> (USC_Z_CHAN * USEASM_SWIZZLE_FIELD_SIZE);
		if (!CompareSwizzles(uSwizzle, uZWSwizzle, uZWDestMask))
		{
			return IMG_FALSE;
		}
	}

	/*
		Check for any requested instruction sources they can be replaced by temporary registers.
	*/
	if (!IsValidSplitInst(psState, 
						  psVecInst, 
						  IMG_TRUE /* bAdjustSwizzles */,
						  psNewArguments,
						  USC_X_CHAN /* uDestStartChan */,
						  &sInstMods))
	{
		return IMG_FALSE;
	}

	/*
		ALU		rN, rM.XYXY, rP.XYXY
			->
		ALU		rT, rM.XY, rP.XY
		MOV		rN.ZW[=rT], rT
	*/
	if (!bCheckOnly)
	{
		IMG_UINT32	uOrigDestMask = psVecInst->auDestMask[0];
		PINST		psMovInst;
		ARG			sXYResult;
		PINST		psXYInst;

		/*
			Modify the original instruction to allow the sources to be replaced by
			temporary registers.
		*/
		BaseSplitVectorInst(psState,
							psVecInst,
							uOrigDestMask & USC_XY_CHAN_MASK,
							&psXYInst,
							0 /* uSecondDestMask */,
							NULL /* ppsSecondInst */);
		ModifyVectorInst(psState, psXYInst, &sInstMods);

		/*
			Allocate a VMOV instruction to copy the XY result to the ZW channels.
		*/
		psMovInst = AllocateInst(psState, psVecInst);
		SetOpcode(psState, psMovInst, IVMOV);

		psMovInst->auDestMask[0] = uOrigDestMask & USC_ZW_CHAN_MASK;
		psMovInst->auLiveChansInDest[0] = psVecInst->auLiveChansInDest[0];

		/*
			Copy the original instruction's predicate to the VMOV instruction.
		*/
		CopyPredicate(psState, psMovInst, psVecInst);

		psMovInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
		psMovInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F32;

		/*
			Move the original instruction destination to the VMOV instruction.
		*/
		MoveDest(psState, psMovInst, 0 /* uMoveToDestIdx */, psXYInst, 0 /* uMoveFromDestIdx */);
	
		/*
			Allocate a new intermediate register for just the XY result.
		*/
		MakeNewTempArg(psState, UF_REGFORMAT_F32, &sXYResult);

		SetDestFromArg(psState, psXYInst, 0 /* uDestIdx */, &sXYResult);
		psXYInst->auLiveChansInDest[0] = GetPreservedChansInPartialDest(psState, psMovInst, 0 /* uDestIdx */);

		/*
			Make the new intermediate register the source for both halves of the
			original instruction destination.
		*/
		SetPartiallyWrittenDest(psState, psMovInst, 0 /* uArgIdx */, &sXYResult);
		SetupTempVecSource(psState, psMovInst, 0 /* uArgIdx */, sXYResult.uNumber, UF_REGFORMAT_F32);

		psMovInst->u.psVec->uDestSelectShift = USC_Z_CHAN;

		*ppsXYInst = psXYInst;
		*ppsZWInst = psMovInst;
	}

	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL SplitVectorInstWithFold(PINTERMEDIATE_STATE	psState,
								 PINST					psVecInst,
								 PVEC_NEW_ARGUMENTS		psNewArguments,
								 IMG_BOOL				bCheckOnly,
								 PINST*					ppsXYInst,
								 PINST*					ppsZWInst)
/*****************************************************************************
 FUNCTION     : SplitVectorInstWithFold
    
 PURPOSE      : Split an instruction into two instructions each of which
			    writes at most two channels of the destination.

 PARAMETERS   : psState		  - Compiler state.
			    psVecInst	  - Instruction to split.
				psNewArguments
							  - Which source/destinations will be assigned
							  internal registers in the split instruction and
							  also their formats.
				bCheckOnly	  - If TRUE just check whether the instruction
							  could be split.
				ppsXYInst	  - Returns the split instructions.
				ppsZWInst

 RETURNS      : TRUE if the instruction could be split.
*****************************************************************************/
{
	IMG_BOOL			bAdjustSwizzles;
	INST_MODIFICATIONS	asInstMods[2];
	IMG_UINT32			uPart;

	/*
		Don't split instructions where a different predicate controls update of each channel of the
		destination.
	*/
	if (GetBit(psVecInst->auFlag, INST_PRED_PERCHAN))
	{
		return IMG_FALSE;
	}

	/*
		Check for an instruction we know how to split.
	*/
	switch (psVecInst->eOpcode)
	{
		case IVDP:
		case IVDP2:
		case IVDP3:
		case IVRSQ:
		case IVRCP:
		case IVLOG:
		case IVEXP:
		{
			bAdjustSwizzles = IMG_FALSE;
			break;
		}
		case IVMUL:
		case IVMUL3:
		case IVADD:
		case IVADD3:
		case IVFRC:
		case IVMIN:
		case IVMAX:
		case IVMAD:
		case IVMAD4:
		case IVMOV:
		case IVDSX:
		case IVDSY:
		case IVDSX_EXP:
		case IVDSY_EXP:
		case IVPCKFLTFLT:
		case IVPCKFLTC10:
		case IVPCKFLTU8:
		case IVMOVC:
		case IVTESTMASK:
		{
			bAdjustSwizzles = IMG_TRUE;
			break;
		}
		case IVMOVCU8_FLT:
		{
			/*
				For destination channel the instruction uses the corresponding byte from the first channel
				of source 0. There's no swizzle on the byte mask so don't split the instruction if we are
				shifting the mask of channels written.
			*/
			if ((psVecInst->auDestMask[0] & USC_ZW_CHAN_MASK) != 0)
			{
				return IMG_FALSE;
			}
			else
			{
				bAdjustSwizzles = IMG_TRUE;
			}
			break;
		}
		default:
		{
			return IMG_FALSE;
		}
	}

	if (bAdjustSwizzles)
	{
		if (SplitReplicatedVectorInst(psState,
									  psVecInst,
									  psNewArguments,
									  bCheckOnly,
									  ppsXYInst,
									  ppsZWInst))
		{
			return IMG_TRUE;
		}
	}
	
	for (uPart = 0; uPart < 2; uPart++)
	{
		static const IMG_UINT32 auDestStartChan[] = {USC_X_CHAN, USC_Z_CHAN};

		/*
			Check if the split instruction (including any sources which need to be folded)
			is still valid.
		*/
		if (!IsValidSplitInst(psState, 
							  psVecInst, 
							  bAdjustSwizzles,
							  psNewArguments,
							  auDestStartChan[uPart],
							  &asInstMods[uPart]))
		{
			return IMG_FALSE;
		}
	}

	if (!bCheckOnly)
	{
		PINST	apsPartInst[2];

		/*
			Generate the split instructions.
		*/
		BaseSplitVectorInst(psState,
							psVecInst,
							psVecInst->auDestMask[0] & USC_XY_CHAN_MASK,
							&apsPartInst[0],
							psVecInst->auDestMask[0] & USC_ZW_CHAN_MASK,
							&apsPartInst[1]);

		for (uPart = 0; uPart < 2; uPart++)
		{
			PINST				psPartInst = apsPartInst[uPart];
			PINST_MODIFICATIONS	psInstMods = &asInstMods[uPart];

			if (psPartInst == NULL)
			{
				continue;
			}

			/*
				Modify the split instruction so it computes the it's result into the XY channels
				and to fold any sources.
			*/
			ModifyVectorInst(psState, psPartInst, psInstMods);

			/*
				Mark for the second part of the split instruction that, even through the 
				destination mask is .ZW, the result of the instruction calcuations for
				the XY channels is copied into the destination.
			*/
			if (uPart == 1)
			{
				psPartInst->u.psVec->uDestSelectShift = USC_Z_CHAN;
			}
		}

		
		*ppsXYInst = apsPartInst[0];
		*ppsZWInst = apsPartInst[1];
	}

	return IMG_TRUE;
}

/*
	Information about the properties of dual-issue primary and secondary
	operations (VDUAL::ePriOp, VDUAL::eSecOp).
*/
IMG_INTERNAL
const VDUAL_OP_DESC g_asDualIssueOpDesc[VDUAL_OP_COUNT] = 
{
	/* VDUAL_OP_MAD1 */
	{
		3,						/* uSrcCount */
		IMG_FALSE,				/* bVector */
		IMG_FALSE,				/* bVectorResult */
		IMG_FALSE,				/* bValidAsVec4Primary */
		#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
		"MAD1",					/* pszName */
		#endif /* defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
	},
	/* VDUAL_OP_MAD3 */
	{
		3,						/* uSrcCount */
		IMG_TRUE,				/* bVector */
		IMG_TRUE,				/* bVectorResult */
		IMG_TRUE,				/* bValidAsVec4Primary */
		#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
		"MAD3",					/* pszName */
		#endif /* defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
	},
	/* VDUAL_OP_MAD4 */
	{
		3,						/* uSrcCount */
		IMG_TRUE,				/* bVector */
		IMG_TRUE,				/* bVectorResult */
		IMG_TRUE,				/* bValidAsVec4Primary */
		#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
		"MAD4",					/* pszName */
		#endif /* defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
	},
	/* VDUAL_OP_DP3 */
	{
		2,						/* uSrcCount */
		IMG_TRUE,				/* bVector */
		IMG_FALSE,				/* bVectorResult */
		IMG_TRUE,				/* bValidAsVec4Primary */
		#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
		"DP3",					/* pszName */
		#endif /* defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
	},
	/* VDUAL_OP_DP4 */
	{
		2,						/* uSrcCount */
		IMG_TRUE,				/* bVector */
		IMG_FALSE,				/* bVectorResult */
		IMG_TRUE,				/* bValidAsVec4Primary */
		#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
		"DP4",					/* pszName */
		#endif /* defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
	},
	/* VDUAL_OP_SSQ3 */
	{
		1,						/* uSrcCount */
		IMG_TRUE,				/* bVector */
		IMG_FALSE,				/* bVectorResult */
		IMG_TRUE,				/* bValidAsVec4Primary */
		#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
		"SSQ3",					/* pszName */
		#endif /* defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
	},	
	/* VDUAL_OP_SSQ4 */
	{
		1,						/* uSrcCount */
		IMG_TRUE,				/* bVector */
		IMG_FALSE,				/* bVectorResult */
		IMG_TRUE,				/* bValidAsVec4Primary */
		#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
		"SSQ4",					/* pszName */
		#endif /* defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
	},	
	/* VDUAL_OP_MUL1 */
	{
		2,						/* uSrcCount */
		IMG_FALSE,				/* bVector */
		IMG_FALSE,				/* bVectorResult */
		IMG_FALSE,				/* bValidAsVec4Primary */
		#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
		"MUL1",					/* pszName */
		#endif /* defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
	},	
	/* VDUAL_OP_MUL3 */
	{
		2,						/* uSrcCount */
		IMG_TRUE,				/* bVector */
		IMG_TRUE,				/* bVectorResult */
		IMG_TRUE,				/* bValidAsVec4Primary */
		#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
		"MUL3",					/* pszName */
		#endif /* defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
	},
	/* VDUAL_OP_MUL4 */
	{
		2,						/* uSrcCount */
		IMG_TRUE,				/* bVector */
		IMG_TRUE,				/* bVectorResult */
		IMG_TRUE,				/* bValidAsVec4Primary */
		#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
		"MUL4",					/* pszName */
		#endif /* defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
	},	
	/* VDUAL_OP_ADD1 */
	{
		2,						/* uSrcCount */
		IMG_FALSE,				/* bVector */
		IMG_FALSE,				/* bVectorResult */
		IMG_FALSE,				/* bValidAsVec4Primary */
		#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
		"ADD1",					/* pszName */
		#endif /* defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
	},	
	/* VDUAL_OP_ADD3 */
	{
		2,						/* uSrcCount */
		IMG_TRUE,				/* bVector */
		IMG_TRUE,				/* bVectorResult */
		IMG_TRUE,				/* bValidAsVec4Primary */
		#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
		"ADD3",					/* pszName */
		#endif /* defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
	},	
	/* VDUAL_OP_ADD4 */
	{
		2,						/* uSrcCount */
		IMG_TRUE,				/* bVector */
		IMG_TRUE,				/* bVectorResult */
		IMG_TRUE,				/* bValidAsVec4Primary */
		#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
		"ADD4",					/* pszName */
		#endif /* defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
	},	
	/* VDUAL_OP_MOV3 */
	{
		1,						/* uSrcCount */
		IMG_TRUE,				/* bVector */
		IMG_TRUE,				/* bVectorResult */
		IMG_TRUE,				/* bValidAsVec4Primary */
		#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
		"MOV3",					/* pszName */
		#endif /* defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
	},
	/* VDUAL_OP_MOV4 */
	{
		1,						/* uSrcCount */
		IMG_TRUE,				/* bVector */
		IMG_TRUE,				/* bVectorResult */
		IMG_TRUE,				/* bValidAsVec4Primary */
		#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
		"MOV4",					/* pszName */
		#endif /* defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
	},	
	/* VDUAL_OP_RSQ */
	{
		1,						/* uSrcCount */
		IMG_FALSE,				/* bVector */
		IMG_FALSE,				/* bVectorResult */
		IMG_TRUE,				/* bValidAsVec4Primary */
		#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
		"RSQ",					/* pszName */
		#endif /* defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
	},	
	/* VDUAL_OP_RCP */
	{
		1,						/* uSrcCount */
		IMG_FALSE,				/* bVector */
		IMG_FALSE,				/* bVectorResult */
		IMG_TRUE,				/* bValidAsVec4Primary */
		#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
		"RCP",					/* pszName */
		#endif /* defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
	},	
	/* VDUAL_OP_LOG */
	{
		1,						/* uSrcCount */
		IMG_FALSE,				/* bVector */
		IMG_FALSE,				/* bVectorResult */
		IMG_FALSE,				/* bValidAsVec4Primary */
		#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
		"LOG",					/* pszName */
		#endif /* defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
	},	
	/* VDUAL_OP_EXP */
	{
		1,						/* uSrcCount */
		IMG_FALSE,				/* bVector */
		IMG_FALSE,				/* bVectorResult */
		IMG_FALSE,				/* bValidAsVec4Primary */
		#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
		"EXP",					/* pszName */
		#endif /* defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
	},	
	/* VDUAL_OP_FRC */
	{
		2,						/* uSrcCount */
		IMG_FALSE,				/* bVector */
		IMG_FALSE,				/* bVectorResult */
		IMG_FALSE,				/* bValidAsVec4Primary */
		#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
		"FRC",					/* pszName */
		#endif /* defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
	},	
};

#define No	IMG_FALSE,
#define Yes	IMG_TRUE,

/*
	Which combinations of dual-issue operations are supported by the hardware.
*/
static IMG_BOOL	const g_aabValidCoissue[VDUAL_OP_COUNT][VDUAL_OP_COUNT] = 
{
/* Op			FMAD	VMAD3	VMAD4	VDP3	VDP4	VSSQ3	VSSQ4	FMUL	VMUL3	VMUL4	FADD	VADD3	VADD4	VMOV3	VMOV4	FRSQ	FRCP	FLOG	FEXP	FFRC */
{/* FMAD */		No		No		No		No		No		Yes		No		No		No		No		No		No		No		Yes		No		Yes		Yes		Yes		Yes		No},
{/* VMAD3 */	No		No		No		No		No		No		No		No		No		No		No		No		No		Yes		No		Yes		Yes		Yes		Yes		No},
{/* VMAD4 */	No		No		No		No		No		No		No		No		No		No		No		No		No		No		Yes		Yes		Yes		Yes		Yes		No},
{/* VDP3 */		No		No		No		No		No		No		No		Yes		No		No		Yes		No		No		Yes		No		Yes		Yes		Yes		Yes		Yes},
{/* VDP4 */		No		No		No		No		No		No		No		No		No		No		Yes		No		No		No		Yes		Yes		Yes		Yes		Yes		Yes},
{/* VSSQ3 */	Yes		No		No		No		No		No		No		Yes		No		No		Yes		Yes		No		Yes		No		Yes		Yes		Yes		Yes		Yes},
{/* VSSQ4 */	No		No		No		No		No		No		No		No		No		No		Yes		No		Yes		No		Yes		Yes		Yes		Yes		Yes		Yes},
{/* FMUL */		No		No		No		Yes		No		Yes		No		Yes		Yes		No		Yes		Yes		No		Yes		No		Yes		Yes		Yes		Yes		Yes},
{/* VMUL3 */	No		No		No		No		No		No		No		Yes		No		No		Yes		Yes		No		Yes		No		Yes		Yes		Yes		Yes		Yes},
{/* VMUL4 */	No		No		No		No		No		No		No		No		No		No		Yes		No		Yes		No		Yes		Yes		Yes		Yes		Yes		Yes},
{/* FADD */		No		No		No		Yes		No		Yes		No		Yes		Yes		No		Yes		Yes		No		Yes		No		Yes		Yes		Yes		Yes		Yes},
{/* VADD3 */	No		No		No		No		No		Yes		No		Yes		Yes		No		Yes		No		No		Yes		No		Yes		Yes		Yes		Yes		Yes},
{/* VADD4 */	No		No		No		No		No		No		No		No		No		Yes		No		No		No		No		Yes		Yes		Yes		Yes		Yes		No},
{/* VMOV3 */	Yes		No		No		Yes		No		Yes		No		Yes		Yes		No		Yes		Yes		No		No		No		Yes		Yes		Yes		Yes		Yes},
{/* VMOV4 */	Yes		No		No		No		Yes		No		Yes		Yes		No		Yes		Yes		No		Yes		No		No		Yes		Yes		Yes		Yes		Yes},
{/* FRSQ */		Yes		No		No		Yes		Yes		Yes		Yes		Yes		Yes		Yes		Yes		Yes		Yes		Yes		Yes		No		No		No		No		Yes},
{/* FRCP */		Yes		No		No		Yes		Yes		Yes		Yes		Yes		Yes		Yes		Yes		Yes		Yes		Yes		Yes		No		No		No		No		Yes},
{/* FLOG */		Yes		No		No		Yes		No		Yes		No		Yes		Yes		No		Yes		Yes		No		Yes		No		No		No		No		No		Yes},
{/* FEXP */		Yes		No		No		Yes		No		Yes		No		Yes		Yes		No		Yes		Yes		No		Yes		No		No		No		No		No		Yes},
{/* FFRC */		Yes		No		No		Yes		No		Yes		No		Yes		Yes		No		Yes		Yes		No		Yes		No		Yes		Yes		Yes		Yes		Yes},
};

#undef No
#undef Yes

typedef enum _DUALISSUE_OP_TYPE
{
	DUALISSUE_OP_TYPE_MAD = 0,
	DUALISSUE_OP_TYPE_DP,
	DUALISSUE_OP_TYPE_SSQ,
	DUALISSUE_OP_TYPE_MUL,
	DUALISSUE_OP_TYPE_ADD,
	DUALISSUE_OP_TYPE_MOV,
	DUALISSUE_OP_TYPE_RSQ,
	DUALISSUE_OP_TYPE_RCP,
	DUALISSUE_OP_TYPE_LOG,
	DUALISSUE_OP_TYPE_EXP,
	DUALISSUE_OP_TYPE_FRC,
	DUALISSUE_OP_TYPE_COUNT,
} DUALISSUE_OP_TYPE;

typedef enum
{
	DUALISSUE_SWIZZLE_OPTIONS_IDENTITY		= 0x00000001,
	DUALISSUE_SWIZZLE_OPTIONS_VEC3STD		= 0x00000002,
	DUALISSUE_SWIZZLE_OPTIONS_VEC3EXT		= 0x00000004,
	DUALISSUE_SWIZZLE_OPTIONS_DERIVED_VEC3	= 0x00000008,
	DUALISSUE_SWIZZLE_OPTIONS_VEC4STD		= 0x00000010,
	DUALISSUE_SWIZZLE_OPTIONS_VEC4EXT		= 0x00000020,
	DUALISSUE_SWIZZLE_OPTIONS_DERIVED_VEC4	= 0x00000040,
	DUALISSUE_SWIZZLE_OPTIONS_COUNT			= 7,
} DUALISSUE_SWIZZLE_SET_TYPES;

typedef enum _DUALISSUE_UNIFIEDSTORE_FORMAT
{
	DUALISSUE_UNIFIEDSTORE_FORMAT_ANY,
	DUALISSUE_UNIFIEDSTORE_FORMAT_F16,
	DUALISSUE_UNIFIEDSTORE_FORMAT_F32,
} DUALISSUE_UNIFIEDSTORE_FORMAT, *PDUALISSUE_UNIFIEDSTORE_FORMAT;

typedef struct _DUALISSUE_INST
{
	/* Points back to the instruction to which this instruction data is attached. */
	PINST				psInst;
	
	/* Points to orignal position of instruction when instruciton is marked to move */
	IMG_BOOL			bMoveMarking;
	PINST				psAnchorPoint;
	
	/* Operation type to use if this instruction is made part of a dual-issue. */
	DUALISSUE_OP_TYPE	eOpType;

	VDUAL_OP			eSetupOp;
	
	/* Count of sources to the operation. */
	IMG_UINT32			uSrcCount;
	
	/* Destination Write Mask */
	IMG_UINT32			uDestWriteMask;

	/* TRUE if this instruction could use a scalar opcode as part of a dual-issue. */
	IMG_BOOL			bScalarAvailable;

	/* TRUE if this instruction can be part of a vec3 dual-issue. */
	IMG_BOOL			bVec3Available;

	/* TRUE if this instruction can be part of a vec4 dual-issue. */
	IMG_BOOL			bVec4Available;

	/*
		The interpretation for non-internal register sources required by this
		instruction.
	*/
	DUALISSUE_UNIFIEDSTORE_FORMAT	eUSFormat;

	/* TRUE if this instruction requires the unified store destination. */
	IMG_BOOL			bRequiresUSDestination;

	/* TRUE if this instruction requires the GPI destination. */
	IMG_BOOL			bRequiresGPIDestination;
	
	/* TRUE if the instruction can be the primary operation in a dual-issue. */
	IMG_BOOL			bCanBePrimary;

	/* TRUE if the instruction can be the secondary operation in a dual-issue. */
	IMG_BOOL			bCanBeSecondary;

	/* Mask of the source configurations which are valid if this operation is primary. */
	IMG_UINT32			uPossiblePrimarySrcCfgs;

	/* 
		Mask of the source configurations which if this operation is primary 
		put a source argument interpreted as F32 in the unified store source slot.
	*/
	IMG_UINT32			uPrimarySrcCfgsWithF32US;

	/* 
		Mask of the source configurations which are valid if this operation is secondary and
		given the number of sources to the primary operation.
	*/
	IMG_UINT32			auPossibleSecondarySrcCfgs[SGXVEC_USE_DVEC_OP1_MAXIMUM_SRC_COUNT];

	/* 
		Mask of the source configurations which if this operation is secondary 
		put a source argument interpreted as F32 in the unified store source slot.
	*/
	IMG_UINT32			auSecondarySrcCfgsWithF32US[SGXVEC_USE_DVEC_OP1_MAXIMUM_SRC_COUNT];

	/*
		The single channel this operand uses from its source argument changes from
		W to X when it is made part of a dual-issue instruction.
	*/
	IMG_BOOL			bSwizzleWToX;
	
	/*
		Hint to try forming dual instructions considering possiblility by source replacement.
	*/
	IMG_BOOL			bMayNeedInternalRegAssingment;
	
	/*
		Tracking sources for IReg substitution, each bit for each source slot.
	*/
	IMG_UINT32			uSrcIRegSubstitutionFlags;
	
	/*
		Temporary storage to keep track of selected GPI
	*/
	ARG					asTargetArgs[SGXVEC_USE_DVEC_OP1_MAXIMUM_SRC_COUNT];
	IMG_UINT32			auIRegEngaged[SGXVEC_USE_DVEC_OP1_MAXIMUM_SRC_COUNT];
	
	/*
		Swizzles in sources
	*/
	IMG_UINT32			auFlagValidSwizzleSet[SGXVEC_USE_DVEC_OP1_MAXIMUM_SRC_COUNT];
} DUALISSUE_INST, *PDUALISSUE_INST;

typedef enum _DUALISSUE_SLIDING_MOVE_DIR_
{
	DUALISSUE_SLIDING_MOVE_DIR_BOTH,
	DUALISSUE_SLIDING_MOVE_DIR_UP,
	DUALISSUE_SLIDING_MOVE_DIR_DOWN,
	DUALISSUE_SLIDING_MOVE_DIR_TOP,
	DUALISSUE_SLIDING_MOVE_DIR_BOTTOM,
	DUALISSUE_SLIDING_MOVE_DIR_DUALPART
}DUALISSUE_SLIDING_MOVE_DIR;

/*
	Structure to hold dual issue data per instruction
*/
typedef struct _VEC_DUALISSUE
{
	/* Is possible dual issue op */
	IMG_BOOL					bIsPossibleDualOp;
	
	/* Is this instruction a descheduling point */
	IMG_BOOL					bIsDeschedPoint;
	
	/* Entry in the list of possible dual-issued instructions. */
	USC_LIST_ENTRY				sCandidateInstsListEntry;
	
	/* Information about the restrictions on dual-issuing this instruction. */
	DUALISSUE_INST				sInstData;
	
	/* Movability of instruction in a code block */
	PINST						*ppsSlidingInfo;
	
	/* New precited position, if instruction has to move */
	PINST						psNewPredicatedPosition;
	DUALISSUE_SLIDING_MOVE_DIR	eNewPositionMoveDir;
} VEC_DUALISSUE, *PVEC_DUALISSUE;

#define	SRCSLOT_GPI0_MASK		(1UL << DUALISSUEVECTOR_SRCSLOT_GPI0)
#define	SRCSLOT_GPI1_MASK		(1UL << DUALISSUEVECTOR_SRCSLOT_GPI1)
#define	SRCSLOT_GPI2_MASK		(1UL << DUALISSUEVECTOR_SRCSLOT_GPI2)
#define SRCSLOT_USS_MASK		(1UL << DUALISSUEVECTOR_SRCSLOT_UNIFIEDSTORE)
#define SRCSLOT_GPIONLY_MASK	(SRCSLOT_GPI0_MASK | SRCSLOT_GPI1_MASK | SRCSLOT_GPI2_MASK)
#define SRCSLOT_ALL_MASK		(SRCSLOT_GPI0_MASK | SRCSLOT_GPI1_MASK | SRCSLOT_GPI2_MASK | SRCSLOT_USS_MASK)

static
IMG_VOID IsSwizzleInDualIssueSet(IMG_UINT32 uSwizzle, IMG_UINT32 uUsedChanMask, IMG_UINT32 *puFlagValidSwizzleSet)
/*****************************************************************************
 FUNCTION	: IsSwizzleInDualIssueSet
    
 PURPOSE	: Checks for the possible sets of swizzles might be available for
			  sources to a dual-issued instruction which ones a swizzle belongs
			  to.

 PARAMETERS	: uSwizzle		- Swizzle to check.
			  uUsedChanMask	- Mask of channels used from the swizzle.
			  psOptions		- Returns information about which sets the
							swizzle belongs to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	static const struct
	{
		IMG_UINT32 const*	puSet;
		IMG_UINT32			uSetCount;
	} asSets[] =
	{
		{g_auIdentitySwizzle, sizeof(g_auIdentitySwizzle) / sizeof(g_auIdentitySwizzle[0])},
		{g_auVec3DualIssueStandardSwizzles, sizeof(g_auVec3DualIssueStandardSwizzles) / sizeof(g_auVec3DualIssueStandardSwizzles[0])},
		{g_auVec3DualIssueExtendedSwizzles, sizeof(g_auVec3DualIssueExtendedSwizzles) / sizeof(g_auVec3DualIssueExtendedSwizzles[0])},
		{g_auVec3DualIssueDerivedSwizzles , sizeof(g_auVec3DualIssueDerivedSwizzles) / sizeof(g_auVec3DualIssueDerivedSwizzles[0])},
		{g_auVec4DualIssueStandardSwizzles, sizeof(g_auVec4DualIssueStandardSwizzles) / sizeof(g_auVec4DualIssueStandardSwizzles[0])},
		{g_auVec4DualIssueExtendedSwizzles, sizeof(g_auVec4DualIssueExtendedSwizzles) / sizeof(g_auVec4DualIssueExtendedSwizzles[0])},
		{g_auVec4DualIssueDerivedSwizzles , sizeof(g_auVec4DualIssueDerivedSwizzles) / sizeof(g_auVec4DualIssueDerivedSwizzles[0])},
		/* DUALISSUE_SWIZZLE_OPTIONS_COUNT */
	};
	IMG_UINT32	uSetIdx;

	*puFlagValidSwizzleSet = 0;
	for (uSetIdx = 0; uSetIdx < (sizeof(asSets) / sizeof(asSets[0])); uSetIdx++)
	{
		(*puFlagValidSwizzleSet) |= 
			(	IsSwizzleInSet(asSets[uSetIdx].puSet,
								asSets[uSetIdx].uSetCount,
								uSwizzle,
								uUsedChanMask,
								NULL) << uSetIdx);
	}
}	

typedef enum _DUALISSUE_OP_WIDTH
{
	 DUALISSUE_OP_WIDTH_SCALAR = 0,
	 DUALISSUE_OP_WIDTH_VEC3,
	 DUALISSUE_OP_WIDTH_VEC4,
	 DUALISSUE_OP_WIDTH_COUNT,
} DUALISSUE_OP_WIDTH, *PDUALISSUE_OP_WIDTH;

static
VDUAL_OP GetDualIssueOp(PDUALISSUE_INST		psInst,
						IMG_BOOL			bVec3)
/*****************************************************************************
 FUNCTION	: GetDualIssueOp
    
 PURPOSE	: Gets the final opcode for an operation as part of a dual-issue
			  instruction.

 PARAMETERS	: psInst1		- Information about the candidate operation to be
							part of a dual-issue instruction.
			  bVec3			- TRUE if the overall dual-issue instruction is vec3.
							FALSE if it is vec4.

 RETURNS	: The opcode.
*****************************************************************************/
{
	static const VDUAL_OP	aaeOps[DUALISSUE_OP_TYPE_COUNT][DUALISSUE_OP_WIDTH_COUNT] = 
	{
		/* DUALISSUE_OP_TYPE_MAD */
		{VDUAL_OP_MAD1,		VDUAL_OP_MAD3,	VDUAL_OP_MAD4},
		/* DUALISSUE_OP_TYPE_DP */
		{VDUAL_OP_UNDEF,	VDUAL_OP_DP3,	VDUAL_OP_DP4},
		/* DUALISSUE_OP_TYPE_SSQ */
		{VDUAL_OP_UNDEF,	VDUAL_OP_SSQ3,	VDUAL_OP_SSQ4},
		/* DUALISSUE_OP_TYPE_MUL */
		{VDUAL_OP_MUL1,		VDUAL_OP_MUL3,	VDUAL_OP_MUL4},
		/* DUALISSUE_OP_TYPE_ADD */
		{VDUAL_OP_ADD1,		VDUAL_OP_ADD3,	VDUAL_OP_ADD4},
		/* DUALISSUE_OP_TYPE_MOV */
		{VDUAL_OP_UNDEF,	VDUAL_OP_MOV3,	VDUAL_OP_MOV4},
		/* DUALISSUE_OP_TYPE_RSQ */
		{VDUAL_OP_RSQ,		VDUAL_OP_RSQ,	VDUAL_OP_RSQ},
		/* DUALISSUE_OP_TYPE_RCP */
		{VDUAL_OP_RCP,		VDUAL_OP_RCP,	VDUAL_OP_RCP},
		/* DUALISSUE_OP_TYPE_LOG */
		{VDUAL_OP_LOG,		VDUAL_OP_LOG,	VDUAL_OP_LOG},
		/* DUALISSUE_OP_TYPE_EXP */
		{VDUAL_OP_EXP,		VDUAL_OP_EXP,	VDUAL_OP_EXP},
		/* DUALISSUE_OP_TYPE_FRC */
		{VDUAL_OP_FRC,		VDUAL_OP_FRC,	VDUAL_OP_FRC},
	};
	DUALISSUE_OP_WIDTH		eOpWidth;

	if (psInst->bScalarAvailable)
	{
		VDUAL_OP	eScalarOp;

		eScalarOp = aaeOps[psInst->eOpType][DUALISSUE_OP_WIDTH_SCALAR];
		if (eScalarOp != VDUAL_OP_UNDEF && (bVec3 || g_asDualIssueOpDesc[eScalarOp].bValidAsVec4Primary))
		{
			return eScalarOp;
		}
	}

	if (bVec3)
	{
		eOpWidth = DUALISSUE_OP_WIDTH_VEC3;
	}
	else
	{
		eOpWidth = DUALISSUE_OP_WIDTH_VEC4;
	}

	return aaeOps[psInst->eOpType][eOpWidth];
}

/*
	Possible site cofigurations for a dual pair
*/
typedef enum _DUALISSUE_SITE_CONFIGURATION_
{
	/*
		overlapped region is region where preceding instruction can be moved down
		as well as succeeding instruction can be moved up.
	*/
	/*
		At same position as preceding instruction
	*/
	DUALISSUE_SITE_CONFIG_PRECEDE_SITE,
 	/*
		At same position as succeeding instruction
	*/
	DUALISSUE_SITE_CONFIG_SUCCEED_SITE,
	/*
		Both the instructions to bring closer, converge at bottom
	*/
	DUALISSUE_SITE_CONFIG_INTER_SITE_PRECEDE_DW_SUCCEED_UP_CONVERGE_BTM,
	/*
		Both the instructions to bring closer, converge at top
	*/
	DUALISSUE_SITE_CONFIG_INTER_SITE_PRECEDE_DW_SUCCEED_UP_CONVERGE_TOP,
 	/*
		The extreme position where preceding instruction can be moved down.
	*/
	DUALISSUE_SITE_CONFIG_PRECEDE_EXTREME_DOWN,
	/*
		The extreme position where succeeding instruction can be moved up.
	*/
	DUALISSUE_SITE_CONFIG_SUCCEED_EXTREME_UP,
	
	DUALISSUE_SITE_CONFIGURATION_COUNT
}DUALISSUE_SITE_CONFIGURATION;

/*
	Structure to highlight affecting area.
*/
typedef struct _DUALISSUE_SLIDING_DATA_
{
	PINST			psConvergeScopeStart;
	PINST			psConvergeScopeEnd;
	PINST			psConvergePoint;
}DUALISSUE_SLIDING_DATA, *PDUALISSUE_SLIDING_DATA;


typedef struct _DUALISSUE_SETUP
{
	/*
		Handy pointer to current working block
	*/
	PCODEBLOCK		psBlock;
	/*
		TRUE if the dual-issue instruction is vec3.
		FALSE if the dual-issue instruction is vec4.
	*/
	IMG_BOOL		bVec3;

	/*
		TRUE if instruction 1 should be the primary operation. 
		Otherwise instruction 2 should be the primary operation.
	*/
	IMG_BOOL		bInst1IsPrimary;

	/*
		Indicates slot used for primary instruction
	*/
	VDUAL_DESTSLOT	ePrimDestSlotConfiguration;

	/*
		TRUE if the dual-issue instruction should interpret unified store sources and destinations as F16.
	*/
	IMG_BOOL	bF16;

	/* The source config to use for the two instructions. */
	IMG_UINT32	uSrcCfg;
	
	/* All available site configurations */
	PINST		apsSiteInst[DUALISSUE_SITE_CONFIGURATION_COUNT];
	PINST		apsNonSiteInst[DUALISSUE_SITE_CONFIGURATION_COUNT];
	/* Current site location used by Generate Vector Dual Issue stage */
	IMG_UINT32	uSiteConfig;
	PINST		psSiteInst;
	PINST		psNonSiteInst;
	PUSC_STACK	psMoveUndo;
	
	/* Use vector of scalar destinations in place of vector destination */
	IMG_BOOL	bExpandVectors;
	
	/* Use-Def relation hints */
	IMG_BOOL	bPrecedInstIsPrimary;
	PINST		psPrecedInst;
	PINST		psSuccedInst;
	IMG_BOOL	bCheckUseDefConstraints;
	IMG_BOOL	bPrecedInstMovableTillSucced;
	IMG_BOOL	bSucceedInstDefiningIntervalRef;
	IMG_BOOL	bSucceedInstDefiningIntervalDef;
	
	DUALISSUE_SLIDING_DATA	sConvergeData;
	
	
	/* Cached calculations for IReg availability for intervals */
	IMG_BOOL	bInstIntervalValidate;
	IMG_PUINT32	ppuAvailableIReg;
	
	/* Debugging purpose */
	IMG_UINT32	uExitHintMax;
} DUALISSUE_SETUP, *PDUALISSUE_SETUP;

#define RETURN_WITH_HINT(x) {*puExitHint = __LINE__; return x;}

static
IMG_BOOL IsPossibleDualIssueOp(PINTERMEDIATE_STATE			psState,
							   PINST						psInst,
							   IMG_UINT32					uLiveChansInInstDest,
							   IMG_UINT32					uRequireUnifiedStoreSrcMask,
							   IMG_BOOL						bRequireNotUnifiedStoreSlot,
							   IMG_BOOL						bSetHintInternalRegReplacement,
							   PIREG_ASSIGNMENT				psIRegAssignment,
							   PDUALISSUE_INST				psInstData);

static
IMG_VOID VecDualUpdateInstructionAfterSubstitution(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: VecDualUpdateInstructionAfterSubstitution
    
 PURPOSE	: Updates dual data structure on arguments substituted.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Changed instruction
			  
 RETURNS	: TRUE if the two instructions can be dual-issued.
*****************************************************************************/
{
	VDUAL_OP eSetupOp;
	
	if (psInst->sStageData.psVDIData == NULL)
	{
		return;
	}
	/* Save dual setup by CanDualIssue() */
	eSetupOp = psInst->sStageData.psVDIData->sInstData.eSetupOp;
	psInst->sStageData.psVDIData->bIsPossibleDualOp = IsPossibleDualIssueOp(	psState, 
																				psInst, 
																				psInst->auLiveChansInDest[0],
																				0 /* uRequireUnifiedStoreSrcMask */, 
																				IMG_FALSE /* bRequireNotUnifiedStoreSlot */,
																				IMG_TRUE /* bSetHintInternalRegReplacement */,
																				NULL /* psIRegAssignment */,
																				&psInst->sStageData.psVDIData->sInstData);
	psInst->sStageData.psVDIData->sInstData.eSetupOp = eSetupOp;
}


static
IMG_BOOL VecDualInstPairFallInSameDataFlow(	PINTERMEDIATE_STATE psState,
											PINST psPrecedInst,
											PINST psSuccedInst)
/*****************************************************************************
 FUNCTION	: VecDualInstPairFallInSameDataFlow
    
 PURPOSE	: Works with sliding info to determine whether instruciton paire 
			  is indepedent of each other.

 PARAMETERS	: psState			- Compiler state.
			  psPrecedInst		- Preceding instruciton of candidate dual pair
			  psSuccedInst		- Succeeding instruciton of candidate dual pair
			  
 RETURNS	: Nothing 
*****************************************************************************/
{
	PINST	*ppsSlidingInfo;
	IMG_UINT32 uIndex;
	if(psSuccedInst->uBlockIndex > psPrecedInst->uBlockIndex)
	{
		ppsSlidingInfo = psSuccedInst->sStageData.psVDIData->ppsSlidingInfo;
		/* Go up in code block */
		for(uIndex = (psSuccedInst->uBlockIndex - 1UL); 
					(ppsSlidingInfo[uIndex] == psSuccedInst) && (uIndex > psPrecedInst->uBlockIndex); 
							uIndex--);
		
		if((ppsSlidingInfo[uIndex] == psSuccedInst) && (uIndex == psPrecedInst->uBlockIndex))
		{
			return IMG_FALSE;
		}
		if(ppsSlidingInfo[uIndex] && uIndex > psPrecedInst->uBlockIndex)
		{
			return VecDualInstPairFallInSameDataFlow(psState, ppsSlidingInfo[uIndex], psPrecedInst);
		}
		return IMG_TRUE;
	}
	return IMG_TRUE;
}

static
IMG_BOOL VecDualComputeIntermediateSiteInfo(PINTERMEDIATE_STATE psState, PDUALISSUE_SETUP psSetup, PINST psPrecedInst, PINST psSuccedInst)
/*****************************************************************************
 FUNCTION	: VecDualComputeIntermediateSiteInfo
    
 PURPOSE	: Setups site configuration for a candidate dual pair

 PARAMETERS	: psState			- Compiler state.
			  psSetup			- Dual formation setup
			  psPrecedInst		- Preceding instruciton of candidate dual pair
			  psSuccedInst		- Succeeding instruciton of candidate dual pair
			  
 RETURNS	: Nothing 
*****************************************************************************/
{
	IMG_BOOL	bResult = IMG_FALSE;
	IMG_UINT32	uIndex;
	PINST		*ppsSlidingInfo;
	
	ASSERT(psSuccedInst->uBlockIndex > 0);
	
	if((!psPrecedInst->sStageData.psVDIData->ppsSlidingInfo) || 
			(!psSuccedInst->sStageData.psVDIData->ppsSlidingInfo) ||
			(psSuccedInst->uBlockIndex < psPrecedInst->uBlockIndex))
	{
		return IMG_FALSE;
	}
	
	ASSERT(psPrecedInst->sStageData.psVDIData->ppsSlidingInfo[psPrecedInst->uBlockIndex] == psPrecedInst);
	ASSERT(psSuccedInst->sStageData.psVDIData->ppsSlidingInfo[psSuccedInst->uBlockIndex] == psSuccedInst);
	
	memset(psSetup->apsSiteInst, 0, sizeof(psSetup->apsSiteInst[0]) * DUALISSUE_SITE_CONFIGURATION_COUNT);
	memset(psSetup->apsNonSiteInst, 0, sizeof(psSetup->apsSiteInst[0]) * DUALISSUE_SITE_CONFIGURATION_COUNT);
	psSetup->bPrecedInstMovableTillSucced = IMG_FALSE;
	psSetup->bSucceedInstDefiningIntervalRef = IMG_FALSE;
	psSetup->bSucceedInstDefiningIntervalDef = IMG_FALSE;
	
	{
		/* Find extreme down position for preceded instruction */
		ppsSlidingInfo = psPrecedInst->sStageData.psVDIData->ppsSlidingInfo;
		
		for(uIndex = (psPrecedInst->uBlockIndex + 1UL); 
			(ppsSlidingInfo[uIndex] == psPrecedInst) && (uIndex < psSuccedInst->uBlockIndex); 
					uIndex++);
		
		if((uIndex > (psPrecedInst->uBlockIndex + 1UL)) && uIndex < psSuccedInst->uBlockIndex)
		{
			PINST psSiteInst = ppsSlidingInfo[uIndex];
			if(!VecDualInstPairFallInSameDataFlow(psState, psPrecedInst, psSuccedInst))
			{
				bResult = IMG_TRUE;
				psSetup->apsSiteInst[DUALISSUE_SITE_CONFIG_PRECEDE_EXTREME_DOWN]
						= psSiteInst;
				psSetup->apsNonSiteInst[DUALISSUE_SITE_CONFIG_PRECEDE_EXTREME_DOWN] = psSuccedInst;
			}
		}
		else
		if(uIndex == psSuccedInst->uBlockIndex && (ppsSlidingInfo[uIndex] == psPrecedInst))
		{
			bResult = IMG_TRUE;
			psSetup->apsSiteInst[DUALISSUE_SITE_CONFIG_SUCCEED_SITE] = psSuccedInst;
			psSetup->apsNonSiteInst[DUALISSUE_SITE_CONFIG_SUCCEED_SITE] = psPrecedInst;
			
			psSetup->bPrecedInstMovableTillSucced = IMG_TRUE;
		}
	}
	
	{
		/* Find extreme up position for succeeding instruction */
		ppsSlidingInfo = psSuccedInst->sStageData.psVDIData->ppsSlidingInfo;
		for(uIndex = (psSuccedInst->uBlockIndex - 1UL); 
			(ppsSlidingInfo[uIndex] == psSuccedInst) && (uIndex > psPrecedInst->uBlockIndex); 
					uIndex--);
		
		if((uIndex < (psSuccedInst->uBlockIndex - 1UL)) && (uIndex > psPrecedInst->uBlockIndex))
		{
			PINST psSiteInst = ppsSlidingInfo[uIndex];
			if(!VecDualInstPairFallInSameDataFlow(psState, psPrecedInst, psSuccedInst))
			{
				bResult = IMG_TRUE;
				psSetup->apsSiteInst[DUALISSUE_SITE_CONFIG_SUCCEED_EXTREME_UP]
						= psSiteInst;
				psSetup->apsNonSiteInst[DUALISSUE_SITE_CONFIG_SUCCEED_EXTREME_UP] = psSuccedInst;
			}
		}
		else
		if(uIndex == psPrecedInst->uBlockIndex && (ppsSlidingInfo[uIndex] == psSuccedInst))
		{
			bResult = IMG_TRUE;
			psSetup->apsSiteInst[DUALISSUE_SITE_CONFIG_PRECEDE_SITE] = psPrecedInst;
			psSetup->apsNonSiteInst[DUALISSUE_SITE_CONFIG_PRECEDE_SITE] = psSuccedInst;
			
			psSetup->bSucceedInstDefiningIntervalRef = IMG_FALSE;
			psSetup->bSucceedInstDefiningIntervalDef = IMG_FALSE;
		}
	}

	if(psSetup->apsSiteInst[DUALISSUE_SITE_CONFIG_PRECEDE_EXTREME_DOWN] 
		  && psSetup->apsSiteInst[DUALISSUE_SITE_CONFIG_PRECEDE_SITE])
	{
		psSetup->apsSiteInst[DUALISSUE_SITE_CONFIG_INTER_SITE_PRECEDE_DW_SUCCEED_UP_CONVERGE_TOP] = 
				psSetup->apsSiteInst[DUALISSUE_SITE_CONFIG_PRECEDE_EXTREME_DOWN];
		psSetup->apsNonSiteInst[DUALISSUE_SITE_CONFIG_INTER_SITE_PRECEDE_DW_SUCCEED_UP_CONVERGE_TOP] = 
				psSetup->apsNonSiteInst[DUALISSUE_SITE_CONFIG_PRECEDE_EXTREME_DOWN];
	}
	
	if(psSetup->apsSiteInst[DUALISSUE_SITE_CONFIG_SUCCEED_EXTREME_UP] 
		  && psSetup->apsSiteInst[DUALISSUE_SITE_CONFIG_SUCCEED_SITE])
	{
			psSetup->apsSiteInst[DUALISSUE_SITE_CONFIG_INTER_SITE_PRECEDE_DW_SUCCEED_UP_CONVERGE_BTM] = 
					psSetup->apsSiteInst[DUALISSUE_SITE_CONFIG_SUCCEED_EXTREME_UP];
			psSetup->apsNonSiteInst[DUALISSUE_SITE_CONFIG_INTER_SITE_PRECEDE_DW_SUCCEED_UP_CONVERGE_BTM] = 
					psSetup->apsNonSiteInst[DUALISSUE_SITE_CONFIG_SUCCEED_EXTREME_UP];
	}
	
	return bResult;
}

typedef enum
{
	DUALISSUE_SLIDING_UPDATE_UP_MOVING_INFO = 0x00000001,
	DUALISSUE_SLIDING_UPDATE_DW_MOVING_INFO = 0x00000002,
	DUALISSUE_SLIDING_UPDATE_BOTH_MOVING_INFO  = 0x00000003
} DUALISSUE_SLIDING_UPDATE_FLAG;

// #define DEBUG_SLIDING 1
// #define DEBUG_VECDUAL_NO_COMPUT_OPTIMIZATION
// #define DEBUG_PAIRING

#if defined(DEBUG_SLIDING)
#define VDUAL_TESTONLY_PRINT_INTERMEDIATE TESTONLY_PRINT_INTERMEDIATE

static
PINST *SaveInstructionSequence(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
/*****************************************************************************
 FUNCTION	: SaveInstructionSequence
    
 PURPOSE	: Debug purpose only, records positions of instructions.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to save instruction sequence for
			  
 RETURNS	: Array of instruction sequence recording
*****************************************************************************/
{
	IMG_UINT32	uIndex;
	PINST		*ppsInstSequence;
	PINST		psRunner;
	
	ppsInstSequence = UscAlloc(psState, sizeof(PINST) * psBlock->uInstCount);
	for(psRunner = psBlock->psBody, uIndex = 0; psRunner; psRunner = psRunner->psNext)
	{
		ppsInstSequence[uIndex++] = psRunner;
	}
	
	return ppsInstSequence;
}

static
IMG_VOID MatchInstructionSequence(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PINST *ppsInstSequence)
/*****************************************************************************
 FUNCTION	: MatchInstructionSequence
    
 PURPOSE	: Debug purpose only, match against previously stored instruction
			  Sequence.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to check instruction sequence for
			  
 RETURNS	: Aborts if different
*****************************************************************************/
{
	IMG_UINT32 uIndex;
	PINST		psRunner;
	
	for(psRunner = psBlock->psBody, uIndex = 0; psRunner; psRunner = psRunner->psNext)
	{
		ASSERT(ppsInstSequence[uIndex++] == psRunner);
	}
	
	UscFree(psState, ppsInstSequence);
}

static
IMG_VOID VecDualVerifySliddingInfo(	PINTERMEDIATE_STATE	psState,
									PCODEBLOCK			psCodeBlock)
/*****************************************************************************
 FUNCTION	: VecDualPrintSliddingInfo
    
 PURPOSE	: HTML dump of sliding info.
		

 PARAMETERS	: psState			- Compiler state.
			  
			  
 RETURNS	: Nothing
*****************************************************************************/
{
	PINST psInst, *ppsSlidingInfo;
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	for(psInst = psCodeBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		IMG_UINT32 uIndex;
		if((!psInst->sStageData.psVDIData) || psInst->eOpcode == IVDUAL)
		{
			continue;
		}
		ppsSlidingInfo = psInst->sStageData.psVDIData->ppsSlidingInfo;
		
		ASSERT(ppsSlidingInfo[psInst->uBlockIndex] == psInst);
		
		for(uIndex = psInst->uBlockIndex; uIndex > 0; uIndex--)
		{
			if(ppsSlidingInfo[uIndex] != psInst)
				break;
		}
		
		ASSERT(ppsSlidingInfo[uIndex] == psInst
				|| (ppsSlidingInfo[uIndex] && ppsSlidingInfo[uIndex]->uBlockIndex < psInst->uBlockIndex)
				|| (ppsSlidingInfo[uIndex] && (ppsSlidingInfo[uIndex] != psInst) && ppsSlidingInfo[uIndex]->uBlockIndex == uIndex));
		
		if(uIndex)
		{
			do
			{
				uIndex--;
				ASSERT(!ppsSlidingInfo[uIndex] || (ppsSlidingInfo[uIndex] == psInst));
			}while(uIndex);
		}
		
		for(uIndex = psInst->uBlockIndex; uIndex < (psCodeBlock->uInstCount - 1UL); uIndex++)
		{
			if(ppsSlidingInfo[uIndex] != psInst)
				break;
		}
		
		ASSERT(ppsSlidingInfo[uIndex] == psInst 
				|| (ppsSlidingInfo[uIndex] && ppsSlidingInfo[uIndex]->uBlockIndex > psInst->uBlockIndex)
				|| (ppsSlidingInfo[uIndex] && (ppsSlidingInfo[uIndex] != psInst) && ppsSlidingInfo[uIndex]->uBlockIndex == uIndex));
		
		if(uIndex < psCodeBlock->uInstCount)
		{
			while(uIndex < (psCodeBlock->uInstCount - 1UL))
			{
				uIndex++;
				ASSERT(!ppsSlidingInfo[uIndex] || (ppsSlidingInfo[uIndex] == psInst));
			}
		}
	}
}

static
IMG_VOID VecDualPrintSliddingInfo(	PINTERMEDIATE_STATE	psState,
									PCODEBLOCK			psCodeBlock)
/*****************************************************************************
 FUNCTION	: VecDualPrintSliddingInfo
    
 PURPOSE	: HTML dump of sliding info.
		

 PARAMETERS	: psState			- Compiler state.
			  
			  
 RETURNS	: Nothing
*****************************************************************************/
{
	PINST psInst;
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	fprintf(stderr, "\n\n <pre><style>th.desched{background-color:#b0b0b0;color:#ffffff}th.dual{background-color:#ff0000;color:#ffffff} TH{font-weight:bold;background-color:#000000;color:#ffffff} TD{border:#E0E0E0 solid 1px;}</style><table> \n");
	for(psInst = psCodeBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		IMG_UINT32 uInx;
		fprintf(stderr, " <tr><td>[%2d]<td> ", psInst->uBlockIndex);
		for(uInx = 0; uInx < psCodeBlock->uInstCount; uInx++)
		{
			if(uInx == psInst->uBlockIndex)
			{
				if(	psInst->sStageData.psVDIData &&
					psInst->sStageData.psVDIData->bIsPossibleDualOp && 
					psInst->sStageData.psVDIData->ppsSlidingInfo)
				{
					fprintf(stderr, "<th class=\"dual\">%2d %s</th>", psInst->sStageData.psVDIData->ppsSlidingInfo[uInx]->uBlockIndex, g_psInstDesc[psInst->eOpcode].pszName);
				}
				else
				{
					if(	psInst->sStageData.psVDIData &&
						psInst->sStageData.psVDIData->bIsDeschedPoint && 
						psInst->sStageData.psVDIData->ppsSlidingInfo)
					{
						fprintf(stderr, "<th class=\"desched\">%2d %s</th>", psInst->sStageData.psVDIData->ppsSlidingInfo[uInx]->uBlockIndex, g_psInstDesc[psInst->eOpcode].pszName);
					}
					else
					{
						fprintf(stderr, "<th>%2d %s</th>", psInst->uBlockIndex, g_psInstDesc[psInst->eOpcode].pszName);
					}
				}
			}
			else
			{
				if(	psInst->sStageData.psVDIData &&
					psInst->sStageData.psVDIData->ppsSlidingInfo &&
					(psInst->sStageData.psVDIData->ppsSlidingInfo[uInx]))
				{
					fprintf(stderr, "<td>%2d</td>", psInst->sStageData.psVDIData->ppsSlidingInfo[uInx]->uBlockIndex);
				}
				else
				{
					fprintf(stderr, "<td>&nbsp;</td>");
				}
			}
		}
		fprintf(stderr, "</tr>\n");
	}
	fprintf(stderr, "</table>\n");
	
	VecDualVerifySliddingInfo(psState, psCodeBlock);
}
#else
#define VecDualPrintSliddingInfo(x, y)
#define VDUAL_TESTONLY_PRINT_INTERMEDIATE(a, b, c)
#endif

static
IMG_VOID VecDualComputeInstructionSlidingInfo(	PINTERMEDIATE_STATE				psState,
												PCODEBLOCK						psCodeBlock,
												PINST							psInst,
												DUALISSUE_SLIDING_UPDATE_FLAG	uUpdateFlag)
/*****************************************************************************
 FUNCTION	: VecDualComputeInstructionSlidingInfo
    
 PURPOSE	: Computes sliding information for instruction of block.
 			  Sliding information can be used to move instruction accross descheduling
 			  barriers.
 			  
 			  Information is stored in psInst->sStageData.psVDIData->ppsSlidingInfo

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Code block to process
			  psInst			- Instruction belonging to same psCodeBlock
			  uUpdateFlag		- flag indicating which side of sliding info should be
			  					  recompute.
			  
 RETURNS	: Nothing 
*****************************************************************************/
{
	IMG_UINT32		uIndex;
	IMG_BOOL		bExitLoop;
	PINST			psRunner;
	
	PINST			*ppsSlidingInfo = psInst->sStageData.psVDIData->ppsSlidingInfo;
	
	USEDEF_RECORD				sLocalUseDefRec;
	PUSEDEF_RECORD				psLocalUseDefRec = &sLocalUseDefRec;
	
	ASSERT(psInst->sStageData.psVDIData);
	
	if(uUpdateFlag == DUALISSUE_SLIDING_UPDATE_BOTH_MOVING_INFO || !ppsSlidingInfo)
	{
		if(ppsSlidingInfo)
		{
			/* Free previous allocated info */
			UscFree(psState, ppsSlidingInfo);
		}
	
		ppsSlidingInfo = UscAlloc(psState, psCodeBlock->uInstCount * sizeof(PINST));
		memset(ppsSlidingInfo, 0, psCodeBlock->uInstCount * sizeof(PINST));
		psInst->sStageData.psVDIData->ppsSlidingInfo = ppsSlidingInfo;
	}
	
	ASSERT(ppsSlidingInfo);
	
	InitUseDef(psLocalUseDefRec);
	if(uUpdateFlag & DUALISSUE_SLIDING_UPDATE_UP_MOVING_INFO)
	{
		/* Go reverse Movability up wards*/
		ClearUseDef(psState, psLocalUseDefRec);
	
		psRunner = psInst->psPrev;
		
		InstUse(psState, psInst, &psLocalUseDefRec->sUse);
		InstDef(psState, psInst, &psLocalUseDefRec->sDef);
		
		ppsSlidingInfo[psInst->uBlockIndex] = psInst;
		
		for(uIndex = 0; uIndex < psInst->uBlockIndex; ppsSlidingInfo[uIndex++] = IMG_NULL);
		
		bExitLoop = IMG_FALSE;
		while(psRunner && !bExitLoop)
		{
			/* Restrict movement of instructions */
			if (psState->uMaxInstMovement != USC_MAXINSTMOVEMENT_NOLIMIT &&
				psInst->uBlockIndex - psRunner->uBlockIndex > psState->uMaxInstMovement)
			{
				ppsSlidingInfo[psRunner->uBlockIndex] = psRunner;
				bExitLoop = IMG_TRUE;
			}
			else
			/* WAR */
			if(InstUseDefined(psState, &psLocalUseDefRec->sDef, psRunner))
			{
				ppsSlidingInfo[psRunner->uBlockIndex] = psRunner;
				bExitLoop = IMG_TRUE;
			}
			else
			/* RAW */
			if(InstDefReferenced(psState, &psLocalUseDefRec->sUse, psRunner))
			{
				ppsSlidingInfo[psRunner->uBlockIndex] = psRunner;
				bExitLoop = IMG_TRUE;
			}
			else
			/* WAW */
			if(InstDefReferenced(psState, &psLocalUseDefRec->sDef, psRunner))
			{
				ppsSlidingInfo[psRunner->uBlockIndex] = psRunner;
				bExitLoop = IMG_TRUE;
			}
			else
			{
				IMG_UINT32 uArgIdx;
				USEDEF_RECORD	sRunnerUseDef;
				PUSEDEF_RECORD	psRunnerUseDef = &sRunnerUseDef;
				IMG_UINT32		auArgumentMask[UINTS_TO_SPAN_BITS(USC_MAXIMUM_NONCALL_ARG_COUNT)];
			
				InitUseDef(psRunnerUseDef);
				ClearUseDef(psState, psRunnerUseDef);
				
				memset(auArgumentMask, 0, sizeof(auArgumentMask));
				
				/* Calculate register def */
				InstDef(psState, psRunner, &psRunnerUseDef->sDef);
				InstUseSubset(psState, psRunner, &psRunnerUseDef->sUse, auArgumentMask);
				
				for(uArgIdx = 0; uArgIdx < psInst->uArgumentCount && !bExitLoop; uArgIdx++)
				{
					PARG psArgToFind = &psInst->asArg[uArgIdx];
					
					if(GetRegUseDef(psState, &psRunnerUseDef->sDef, psArgToFind->uType, psArgToFind->uNumber))
					{
						ppsSlidingInfo[psRunner->uBlockIndex] = psRunner;
						bExitLoop = IMG_TRUE;
					}
					
					if(GetRegUseDef(psState, &psRunnerUseDef->sUse, psArgToFind->uType, psArgToFind->uNumber))
					{
						ppsSlidingInfo[psRunner->uBlockIndex] = psRunner;
						bExitLoop = IMG_TRUE;
					}
				}
				ClearUseDef(psState, psRunnerUseDef);
				
				if(!bExitLoop)
				{
					ppsSlidingInfo[psRunner->uBlockIndex] = psInst;
				}
			}
			psRunner = psRunner->psPrev;
		}
		
		/* Go reverse Movability down wards*/
		ClearUseDef(psState, psLocalUseDefRec);
	}

	if(uUpdateFlag & DUALISSUE_SLIDING_UPDATE_DW_MOVING_INFO)
	{
		psRunner = psInst->psNext;
		ClearUseDef(psState, psLocalUseDefRec);
		InstUse(psState, psInst, &psLocalUseDefRec->sUse);
		InstDef(psState, psInst, &psLocalUseDefRec->sDef);
		
		for(uIndex = psInst->uBlockIndex + 1UL; uIndex < psCodeBlock->uInstCount; ppsSlidingInfo[uIndex++] = IMG_NULL);
		
		ppsSlidingInfo[psInst->uBlockIndex] = psInst;
		bExitLoop = IMG_FALSE;
		while(psRunner && !bExitLoop)
		{
			/* Restrict movement of instructions */
			if (psState->uMaxInstMovement != USC_MAXINSTMOVEMENT_NOLIMIT &&
				psRunner->uBlockIndex - psInst->uBlockIndex  > psState->uMaxInstMovement)
			{
				ppsSlidingInfo[psRunner->uBlockIndex] = psRunner;
				bExitLoop = IMG_TRUE;
			}
			else
			/* RAW */
			if(InstUseDefined(psState, &psLocalUseDefRec->sDef, psRunner))
			{
				ppsSlidingInfo[psRunner->uBlockIndex] = psRunner;
				bExitLoop = IMG_TRUE;
			}
			else
			/* WAR */
			if(InstDefReferenced(psState, &psLocalUseDefRec->sUse, psRunner))
			{
				ppsSlidingInfo[psRunner->uBlockIndex] = psRunner;
				bExitLoop = IMG_TRUE;
			}
			else
			/* WAW */
			if(InstDefReferenced(psState, &psLocalUseDefRec->sDef, psRunner))
			{
				ppsSlidingInfo[psRunner->uBlockIndex] = psRunner;
				bExitLoop = IMG_TRUE;
			}
			else
			{
				IMG_UINT32 uArgIdx;
				
				for(uArgIdx = 0; uArgIdx < psRunner->uArgumentCount && !bExitLoop; uArgIdx++)
				{
					PARG psArgToFind = &psRunner->asArg[uArgIdx];
					
					if(GetRegUseDef(psState, &psLocalUseDefRec->sDef, psArgToFind->uType, psArgToFind->uNumber))
					{
						ppsSlidingInfo[psRunner->uBlockIndex] = psRunner;
						bExitLoop = IMG_TRUE;
					}
				}
				if(!bExitLoop)
				{
					ppsSlidingInfo[psRunner->uBlockIndex] = psInst;
				}
			}
			psRunner = psRunner->psNext;
		}
		
		ClearUseDef(psState, psLocalUseDefRec);
	}
}

static
IMG_VOID VecDualComputeInstructionSlidingInfoBP(PINTERMEDIATE_STATE	psState,
												PCODEBLOCK			psCodeBlock,
												PINST				psChangedPrecedInst,
												PINST				psChangedSuccedInst)
/*****************************************************************************
 FUNCTION	: VecDualComputeIntermediateSiteInfoBP
    
 PURPOSE	: Computes sliding information for instruction of block.
 			  Sliding information can be used to move instruction accross descheduling
 			  barriers.
 			  
 			  Information is stored in psInst->sStageData.psVDIData->ppsSlidingInfo

 PARAMETERS	: psState				- Compiler state.
			  psCodeBlock			- Code block to process
			  psChangedPrecedInst	- Used for computation optimisation
			  psChangedSuccedInst	- Used for computation optimisation
			  
 RETURNS	: Nothing 
*****************************************************************************/
{
	PINST psInst;
	IMG_UINT32 uUpdateFlag;
	
	PVR_UNREFERENCED_PARAMETER(psChangedPrecedInst);
	PVR_UNREFERENCED_PARAMETER(psChangedSuccedInst);
	
	for (psInst = psCodeBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		if(psInst->eOpcode != IVDUAL)
		{
			uUpdateFlag = DUALISSUE_SLIDING_UPDATE_BOTH_MOVING_INFO;

#if !defined(DEBUG_VECDUAL_NO_COMPUT_OPTIMIZATION)
			if(psChangedPrecedInst && (psInst->uBlockIndex < psChangedPrecedInst->uBlockIndex))
			{
				uUpdateFlag &= ~DUALISSUE_SLIDING_UPDATE_UP_MOVING_INFO;
			}
			
			if(psChangedSuccedInst && (psInst->uBlockIndex > psChangedSuccedInst->uBlockIndex))
			{
				uUpdateFlag &= ~DUALISSUE_SLIDING_UPDATE_DW_MOVING_INFO;
			}
#endif
			VecDualComputeInstructionSlidingInfo(psState, psCodeBlock, psInst, uUpdateFlag);
		}
	}
	VecDualPrintSliddingInfo(psState, psCodeBlock);
}

static
IMG_BOOL VecDualMoveInstructionGroup(	PINTERMEDIATE_STATE					psState,
										PINST								psInstToMove,
										PINST								psInstBefore,
										PINST								psInstOrAfter,
										DUALISSUE_SLIDING_MOVE_DIR			uDirection,
										IMG_PVOID							pvData)
/*****************************************************************************
 FUNCTION	: VecDualMoveInstructionGroup
    
 PURPOSE	: Tries to move an instruction for given before point or after point.
			  Instruction will be mark to move before given 'before point'
			  but after given 'after point'
		

 PARAMETERS	: psState			- Compiler state.
			  psInstToMove		- Instruction to move
			  psInstBefore		- Before point instruction, can be null
			  psInstOrAfter		- After point can be null
			  
 RETURNS	: TRUE if psInstToMove can be relocated out of interval / at edge of
 			  interval.
*****************************************************************************/
{
	IMG_UINT32					uInx;
	PDUALISSUE_SLIDING_DATA	psConvergeData = &((PDUALISSUE_SETUP)pvData)->sConvergeData;
	
	
	if(!psInstToMove->sStageData.psVDIData
		   || psInstToMove->uBlockIndex == 0
		   || (psInstToMove->uBlockIndex == (psInstToMove->psBlock->uInstCount - 1UL))
		   || psInstBefore->uBlockIndex == 0
		   || psInstOrAfter->uBlockIndex == (psInstToMove->psBlock->uInstCount - 1UL))
	{
		return IMG_FALSE;
	}
	
	if(psInstToMove->eOpcode == IVDUAL)
	{
		/* TODO: Compute sliding info for ivdual */
		return IMG_FALSE;
	}
	
	if(!psInstBefore && !psInstOrAfter)
	{
		return IMG_FALSE;
	}
	
	if(uDirection == DUALISSUE_SLIDING_MOVE_DIR_BOTH && !psInstOrAfter)
	{
		uDirection = DUALISSUE_SLIDING_MOVE_DIR_UP;
	}
	else
	if(uDirection == DUALISSUE_SLIDING_MOVE_DIR_BOTH && !psInstBefore)
	{
		uDirection = DUALISSUE_SLIDING_MOVE_DIR_DOWN;
	}
	else
	{
		if(psConvergeData)
		{
			if(!psInstBefore || (psInstBefore && psConvergeData->psConvergeScopeStart->uBlockIndex < psInstBefore->uBlockIndex))
			{
				psInstBefore = psConvergeData->psConvergeScopeStart;
			}
			
			if(!psInstOrAfter || (psInstOrAfter && psConvergeData->psConvergeScopeEnd->uBlockIndex > psInstOrAfter->uBlockIndex))
			{
				psInstOrAfter = psConvergeData->psConvergeScopeEnd;
			}
		}
	}
	
	if(uDirection == DUALISSUE_SLIDING_MOVE_DIR_UP 
		  && psInstToMove->uBlockIndex < psInstBefore->uBlockIndex)
	{
		return IMG_TRUE;
	}
	
	if(uDirection == DUALISSUE_SLIDING_MOVE_DIR_DOWN
		  && psInstToMove->uBlockIndex > psInstOrAfter->uBlockIndex)
	{
		return IMG_TRUE;
	}
	
	if((!psInstToMove->sStageData.psVDIData))
	{
		return IMG_FALSE;
	}
	
	/* Resolve conflicting moves */
	if (psInstToMove->sStageData.psVDIData->eNewPositionMoveDir == DUALISSUE_SLIDING_MOVE_DIR_DUALPART)
	{
		return IMG_FALSE;
	}
	switch(uDirection)
	{
		case DUALISSUE_SLIDING_MOVE_DIR_UP:
		{
			if(psInstToMove->sStageData.psVDIData->eNewPositionMoveDir == DUALISSUE_SLIDING_MOVE_DIR_DOWN)
			{
				return IMG_FALSE;
			}
			break;
		}
		case DUALISSUE_SLIDING_MOVE_DIR_DOWN:
		{
			if(psInstToMove->sStageData.psVDIData->eNewPositionMoveDir == DUALISSUE_SLIDING_MOVE_DIR_UP)
			{
				return IMG_FALSE;
			}
			break;
		}
		default:
			break;
	}
	
	if( psInstBefore
		  && psInstBefore->uBlockIndex < psInstToMove->uBlockIndex
		  && (uDirection == DUALISSUE_SLIDING_MOVE_DIR_BOTH || uDirection == DUALISSUE_SLIDING_MOVE_DIR_UP)
		  && psInstToMove->sStageData.psVDIData->ppsSlidingInfo[psInstBefore->uBlockIndex] == psInstToMove)
	{
		/* Single instruction psInstToMove can be moved before psInstBefore DUALISSUE_SLIDING_MOVE_DIR_UP */
		for(uInx = (psInstBefore->uBlockIndex - 1U); uInx; uInx--)
		{
			if(psInstToMove->sStageData.psVDIData->ppsSlidingInfo[uInx] != psInstToMove)
			{
				break;
			}
		}
		
		ASSERT(psInstToMove->sStageData.psVDIData->ppsSlidingInfo[uInx]);
		if(psInstToMove->sStageData.psVDIData->ppsSlidingInfo[uInx] == psInstToMove->psPrev
		  && (psInstToMove->psPrev->uBlockIndex == psInstBefore->uBlockIndex))
		{
			return IMG_FALSE;
		}
		
		psInstToMove->sStageData.psVDIData->psNewPredicatedPosition
					= psInstToMove->sStageData.psVDIData->ppsSlidingInfo[uInx];
		psInstToMove->sStageData.psVDIData->eNewPositionMoveDir = uInx ? DUALISSUE_SLIDING_MOVE_DIR_UP
																			: DUALISSUE_SLIDING_MOVE_DIR_TOP;
		return IMG_TRUE;
	}
	
	if(uDirection == DUALISSUE_SLIDING_MOVE_DIR_BOTH || uDirection == DUALISSUE_SLIDING_MOVE_DIR_UP)
	{
		for(uInx = (psInstToMove->uBlockIndex - 1U); uInx > 0; uInx--)
		{
			if(psInstToMove->sStageData.psVDIData->ppsSlidingInfo[uInx] != psInstToMove)
			{
				break;
			}
		}
		
		ASSERT(psInstToMove->sStageData.psVDIData->ppsSlidingInfo[uInx]);
		{
			PINST psDependantInst;
			
			psDependantInst = psInstToMove->sStageData.psVDIData->ppsSlidingInfo[uInx];
			if(psDependantInst == psInstToMove->psPrev)
			{
				return IMG_FALSE;
			}
			
			/* Attach current instruction to dependent */
			psInstToMove->sStageData.psVDIData->psNewPredicatedPosition = psDependantInst;
			if(psInstToMove->sStageData.psVDIData->ppsSlidingInfo[uInx] == psInstToMove)
			{
				psInstToMove->sStageData.psVDIData->eNewPositionMoveDir = DUALISSUE_SLIDING_MOVE_DIR_TOP;
				return IMG_TRUE;
			}
			else
			if(VecDualMoveInstructionGroup(psState, psDependantInst, psInstBefore, psInstOrAfter, DUALISSUE_SLIDING_MOVE_DIR_UP, pvData))
			{
				psInstToMove->sStageData.psVDIData->eNewPositionMoveDir = DUALISSUE_SLIDING_MOVE_DIR_UP;
				return IMG_TRUE;
			}
		}
	}
	
	if( psInstOrAfter
		&& psInstOrAfter->uBlockIndex > psInstToMove->uBlockIndex
		&& (uDirection == DUALISSUE_SLIDING_MOVE_DIR_BOTH || uDirection == DUALISSUE_SLIDING_MOVE_DIR_DOWN)
		&& psInstToMove->sStageData.psVDIData->ppsSlidingInfo[psInstOrAfter->uBlockIndex] == psInstToMove)
	{
		/* Single instruction psInstToMove can be moved after psInstOrAfter DUALISSUE_SLIDING_MOVE_DIR_DOWN */
		for(uInx = (psInstOrAfter->uBlockIndex + 1U); uInx < (psInstToMove->psBlock->uInstCount - 1UL); uInx++)
		{
			if(psInstToMove->sStageData.psVDIData->ppsSlidingInfo[uInx] != psInstToMove)
			{
				break;
			}
		}
		
		ASSERT(psInstToMove->sStageData.psVDIData->ppsSlidingInfo[uInx]);
		if(psInstToMove->sStageData.psVDIData->ppsSlidingInfo[uInx] == psInstToMove->psNext
		  	&& (psInstToMove->psNext->uBlockIndex == psInstOrAfter->uBlockIndex))
		{
			return IMG_FALSE;
		}
	
		psInstToMove->sStageData.psVDIData->psNewPredicatedPosition
				= psInstToMove->sStageData.psVDIData->ppsSlidingInfo[uInx];
		psInstToMove->sStageData.psVDIData->eNewPositionMoveDir = uInx ? DUALISSUE_SLIDING_MOVE_DIR_DOWN
																			: DUALISSUE_SLIDING_MOVE_DIR_BOTTOM;
		return IMG_TRUE;
	}
	
	if(uDirection == DUALISSUE_SLIDING_MOVE_DIR_BOTH || uDirection == DUALISSUE_SLIDING_MOVE_DIR_DOWN)
	{
		/* It is required to move depending instruction group */
		for(uInx = (psInstToMove->uBlockIndex + 1U); uInx < (psInstToMove->psBlock->uInstCount - 1UL); uInx++)
		{
			if(psInstToMove->sStageData.psVDIData->ppsSlidingInfo[uInx] != psInstToMove)
			{
				break;
			}
		}
		
		ASSERT(psInstToMove->sStageData.psVDIData->ppsSlidingInfo[uInx]);
		
		{
			PINST psDependantInst;
			
			psDependantInst = psInstToMove->sStageData.psVDIData->ppsSlidingInfo[uInx];
			if(psDependantInst == psInstToMove->psNext)
			{
				return IMG_FALSE;
			}
			
			/* Attach current instruction to dependent */
			psInstToMove->sStageData.psVDIData->psNewPredicatedPosition = psDependantInst;
			if(psDependantInst == psInstToMove)
			{
				psInstToMove->sStageData.psVDIData->eNewPositionMoveDir = DUALISSUE_SLIDING_MOVE_DIR_BOTTOM;
				return IMG_TRUE;
			}
			else
			if(VecDualMoveInstructionGroup(psState, psDependantInst, psInstBefore, psInstOrAfter, DUALISSUE_SLIDING_MOVE_DIR_DOWN, pvData))
			{
				psInstToMove->sStageData.psVDIData->eNewPositionMoveDir = DUALISSUE_SLIDING_MOVE_DIR_DOWN;
				return IMG_TRUE;
			}
		}
	}
	return IMG_FALSE;
}

typedef struct _IREG_UNDO
{
	/* Modified instruction. */
	PINST		psInst;
	/* Index of the modified instruction destination. */
	IMG_UINT32	uDest;
	/* Original mask of channels live in the destination register at this point. */
	IMG_UINT32	uOldLiveChans;
	/* If TRUE the instruction was modified to only partially defined its destination. */
	IMG_BOOL	bAddedPartialDest;
} IREG_UNDO, *PIREG_UNDO;

static
IMG_VOID AddIRegPartialDest(PINTERMEDIATE_STATE psState, 
							PUSC_STACK			psUndoStack,
							PINST				psInst, 
							IMG_UINT32			uDest, 
							IMG_UINT32			uNewLiveChans)
/*****************************************************************************
 FUNCTION	: AddIRegPartialDest
    
 PURPOSE	: Modify an instruction so it only partially defines a GPI
			  destination.

 PARAMETERS	: psState			- Compiler state.
			  psUndoStack		- The instruction modifications are pushed on
								this stack.
			  psInst			- Instruction to modify.
			  uDest				- Destination to modify.
			  uNewLiveChans		- New mask of channels which are live in the
								GPI at this point.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (uNewLiveChans != 0)
	{
		IREG_UNDO	sUndo;

		sUndo.psInst = psInst;
		sUndo.uDest = uDest;
		sUndo.uOldLiveChans = psInst->auLiveChansInDest[uDest];

		psInst->auLiveChansInDest[uDest] |= uNewLiveChans;

		if (psInst->apsOldDest[uDest] == NULL)
		{	
			sUndo.bAddedPartialDest = IMG_TRUE;
			SetPartiallyWrittenDest(psState, psInst, uDest, &psInst->asDest[uDest]);
		}
		else
		{
			sUndo.bAddedPartialDest = IMG_FALSE;
			ASSERT(EqualArgs(&psInst->asDest[uDest], psInst->apsOldDest[uDest]));
		}

		/*
			Reconsider whether the instruction is a possible dual-issue operation.
		*/
		VecDualUpdateInstructionAfterSubstitution(psState, psInst);

		/*
			Record the instruction modifications.
		*/
		UscStackPush(psState, psUndoStack, &sUndo);
	}
}

static
IMG_VOID UndoInstMove(PINTERMEDIATE_STATE psState, PUSC_STACK psUndoStack)
/*****************************************************************************
 FUNCTION	: UndoInstMove
    
 PURPOSE	: Restore instructions modified by MoveInstAndUpdateIRegs.

 PARAMETERS	: psState			- Compiler state.
			  psUndoStack		- Record of the modified instructions.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	while (!UscStackEmpty(psUndoStack))
	{
		PIREG_UNDO	psUndo;
		PINST		psUndoInst;

		psUndo = (PIREG_UNDO)UscStackTop(psUndoStack);
		psUndoInst = psUndo->psInst;
		psUndoInst->auLiveChansInDest[psUndo->uDest] = psUndo->uOldLiveChans;
		if (psUndo->bAddedPartialDest)
		{
			SetPartiallyWrittenDest(psState, psUndoInst, psUndo->uDest, NULL);
		}

		/*
			Reconsider whether the instruction is a possible dual-issue operation.
		*/
		VecDualUpdateInstructionAfterSubstitution(psState, psUndoInst);

		UscStackPop(psState, psUndoStack);
	}
}

static
IMG_VOID MoveInstAndUpdateIRegs(PINTERMEDIATE_STATE psState, 
								PINST				psInst, 
								PINST				psInsertBeforeInst, 
								PUSC_STACK			psUndoStack)
/*****************************************************************************
 FUNCTION	: MoveInstAndUpdateIRegs
    
 PURPOSE	: Move an instruction and update liveness information for instructions
			  in between.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to move.
			  psInsertBeforeInst
								- Insert the instruction before this instruction.
			  psUndoStack		- Records modifications to instructions in
								between.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uSrc;
	IMG_UINT32	uDest;
	IMG_UINT32	uAnchorPoint;
	PCODEBLOCK	psBlock = psInst->psBlock;

	/*
		Record the original postion of the instruction.
	*/
	uAnchorPoint = psInst->uBlockIndex;

	/*
		Move the instruction to its new position.
	*/
	RemoveInst(psState, psBlock, psInst);
	InsertInstBefore(psState, psBlock, psInst, psInsertBeforeInst);

	/*
		If the instruction moved backwards...
	*/
	if (psInst->uBlockIndex < uAnchorPoint)
	{
		/*
			For each GPI instruction destination...
		*/
		for (uDest = 0; uDest < psInst->uDestCount; uDest++)
		{
			PARG			psDest = &psInst->asDest[uDest];
			PUSC_LIST_ENTRY	psListEntry;
			IMG_UINT32		uLiveChansInDest;

			if (psDest->uType != USEASM_REGTYPE_FPINTERNAL)
			{
				continue;
			}

			/*
				For all the defines of the same GPI in between the new and old positions.
			*/
			for (psListEntry = psInst->asDestUseDef[uDest].sUseDef.sListEntry.psNext;
				 psListEntry != NULL;
				 psListEntry = psListEntry->psNext)
			{
				PUSEDEF	psUse;
				PINST	psUseInst;

				psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
			
				ASSERT(psUse->eType == USE_TYPE_SRC || psUse->eType == USE_TYPE_OLDDEST || psUse->eType == DEF_TYPE_INST);
				psUseInst = psUse->u.psInst;
				if (psUseInst->uBlockIndex > uAnchorPoint)
				{
					break;
				}
				if (psUse->eType == DEF_TYPE_INST)
				{
					IMG_UINT32	uDestIdx = psUse->uLocation;

					ASSERT(uDestIdx < psUseInst->uDestCount);
					/*
						This define needs to preserve the channels written by the moving instructions.
					*/
					AddIRegPartialDest(psState, psUndoStack, psUseInst, uDestIdx, psInst->auDestMask[uDest]);
				}
			}

			/*
				Get the mask of channels live in the GPI register at the new position of the instruction.
			*/
			uLiveChansInDest = psInst->auLiveChansInDest[uDest];
			for (psListEntry = psListEntry->psPrev; 
				 psListEntry != &psInst->asDestUseDef[uDest].sUseDef.sListEntry;
				 psListEntry = psListEntry->psPrev)
			{
				PUSEDEF	psUse;

				psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

				if (psUse->eType == DEF_TYPE_INST)
				{
					uLiveChansInDest &= ~psUse->u.psInst->auDestMask[psUse->uLocation];
				}
				else
				{
					uLiveChansInDest |= GetUseChanMask(psState, psUse);
				}
			}
			
			/*
				Update the moving instruction.
			*/
			ASSERT((uLiveChansInDest & psInst->auLiveChansInDest[uDest]) == psInst->auLiveChansInDest[uDest]);
			AddIRegPartialDest(psState, psUndoStack, psInst, uDest, uLiveChansInDest & ~psInst->auLiveChansInDest[uDest]);
		}
	}

	/*	
		If the instruction is moving forwards...
	*/
	if (psInst->uBlockIndex > uAnchorPoint)
	{
		/*
			For each GPI source to the moving instruction...
		*/
		for (uSrc = 0; uSrc < psInst->uArgumentCount; uSrc++)
		{
			PARG			psSrc = &psInst->asArg[uSrc];
			IMG_UINT32		uNewLiveChans;
			PUSC_LIST_ENTRY	psListEntry;

			if (psSrc->uType != USEASM_REGTYPE_FPINTERNAL)
			{
				continue;
			}

			/*
				For all defines of the same GPI in between the new and old positions.
			*/
			uNewLiveChans = GetLiveChansInArg(psState, psInst, uSrc);
			for (psListEntry = psInst->asArgUseDef[uSrc].sUseDef.sListEntry.psPrev;
				 psListEntry != NULL;
				 psListEntry = psListEntry->psPrev)
			{
				PUSEDEF	psUse;
				PINST	psUseInst;

				psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
			
				ASSERT(psUse->eType == USE_TYPE_SRC || psUse->eType == USE_TYPE_OLDDEST || psUse->eType == DEF_TYPE_INST);
				psUseInst = psUse->u.psInst;
				if (psUseInst->uBlockIndex < uAnchorPoint)
				{
					break;
				}
				if (psUse->eType == DEF_TYPE_INST)
				{
					IMG_UINT32	uDestIdx = psUse->uLocation;

					if ((psUseInst->auDestMask[uDestIdx] & uNewLiveChans) != 0)
					{
						break;
					}

					/*
						Mark that this instruction is now preserving channels read by the moving instruction.
					*/
					ASSERT(uDestIdx < psUseInst->uDestCount);
					AddIRegPartialDest(psState, psUndoStack, psUseInst, uDestIdx, uNewLiveChans);
				}
			}
		}
	}
}

static
IMG_BOOL VecDualRepositionInstructionForSiteConfiguration(PINTERMEDIATE_STATE psState, IMG_UINT32 uSiteConfig, PDUALISSUE_SETUP psSetup, IMG_BOOL *pbIterate)
/*****************************************************************************
 FUNCTION	: VecDualRepositionInstructionForSiteConfiguration
    
 PURPOSE	: Relocate instructions towards site for dual formation
		

 PARAMETERS	: psState			- Compiler state.
			  uSiteConfig		- Site configuration
			  psSetup			- Vector dual formation setup.
			  
 RETURNS	: TRUE - if at least one instruction is moved.
*****************************************************************************/
{
	PINST			psChangedPrecedInst, psChangedSuccedInst;
	PDUALISSUE_INST	psInstData, psInstData1, psInstData2;
	IMG_BOOL		bResult = IMG_FALSE;
	
	*pbIterate = IMG_TRUE;
	
	psChangedPrecedInst = psSetup->psPrecedInst->psPrev;
	psChangedSuccedInst = psSetup->psSuccedInst->psNext;
	
	switch(uSiteConfig)
	{
#if 0
		case DUALISSUE_SITE_CONFIG_SUCCEED_EXTREME_UP:
		case DUALISSUE_SITE_CONFIG_PRECEDE_EXTREME_DOWN:
		{
			PINST psInvervalStart, psIntervalEnd;
			
			*pbIterate = IMG_FALSE;
			psInvervalStart	= psSetup->apsSiteInst[DUALISSUE_SITE_CONFIG_PRECEDE_EXTREME_DOWN] ?
									psSetup->apsSiteInst[DUALISSUE_SITE_CONFIG_PRECEDE_EXTREME_DOWN] : psSetup->psPrecedInst;
			
			psIntervalEnd	= psSetup->apsSiteInst[DUALISSUE_SITE_CONFIG_SUCCEED_EXTREME_UP] ?
									psSetup->apsSiteInst[DUALISSUE_SITE_CONFIG_SUCCEED_EXTREME_UP] : psSetup->psSuccedInst;
			
			if(uSiteConfig == DUALISSUE_SITE_CONFIG_PRECEDE_EXTREME_DOWN)
			{
				if(!psSetup->apsSiteInst[DUALISSUE_SITE_CONFIG_SUCCEED_EXTREME_UP])
				{
					return bResult;
				}
				if(VecDualMoveInstructionGroup(psState, psSetup->apsSiteInst[DUALISSUE_SITE_CONFIG_PRECEDE_EXTREME_DOWN],
													IMG_NULL, psIntervalEnd, DUALISSUE_SLIDING_MOVE_DIR_DOWN, IMG_NULL))
				{
					DBG_PRINTF((DBG_WARNING, " DW Found Case! %s ", USC_INPUT_FILENAME));
				}
			}
			else
			{
				if(!psSetup->apsSiteInst[DUALISSUE_SITE_CONFIG_PRECEDE_EXTREME_DOWN])
				{
					return bResult;
				}
				if(VecDualMoveInstructionGroup(psState, psSetup->apsSiteInst[DUALISSUE_SITE_CONFIG_SUCCEED_EXTREME_UP],
													psInvervalStart, IMG_NULL, DUALISSUE_SLIDING_MOVE_DIR_UP, IMG_NULL))
				{
					DBG_PRINTF((DBG_WARNING, " UP Found Case! %s ", USC_INPUT_FILENAME));
				}
			}
			return bResult;
		}
#endif
		case DUALISSUE_SITE_CONFIG_INTER_SITE_PRECEDE_DW_SUCCEED_UP_CONVERGE_BTM:
		case DUALISSUE_SITE_CONFIG_INTER_SITE_PRECEDE_DW_SUCCEED_UP_CONVERGE_TOP:
			
			if(!(psSetup->psPrecedInst->psNext == psSetup->psSuccedInst))
			{
				ASSERT(psSetup->apsSiteInst[uSiteConfig] != psSetup->psPrecedInst);
				ASSERT(psSetup->apsNonSiteInst[uSiteConfig] == psSetup->psSuccedInst);
				
				psInstData1 = &psSetup->psPrecedInst->sStageData.psVDIData->sInstData;
				psInstData1->psAnchorPoint = psSetup->psPrecedInst->psPrev;
				psInstData1->bMoveMarking = IMG_TRUE;
				
				psInstData2 = &psSetup->psSuccedInst->sStageData.psVDIData->sInstData;
				psInstData2->psAnchorPoint = psSetup->psSuccedInst->psPrev;
				psInstData2->bMoveMarking = IMG_TRUE;
				
				if(uSiteConfig == DUALISSUE_SITE_CONFIG_INTER_SITE_PRECEDE_DW_SUCCEED_UP_CONVERGE_TOP)
				{
					MoveInstAndUpdateIRegs(psState, 
										   psSetup->psPrecedInst, 
										   psSetup->apsSiteInst[uSiteConfig], 
										   psSetup->psMoveUndo);
					MoveInstAndUpdateIRegs(psState, 
										   psSetup->psSuccedInst, 
										   psSetup->apsSiteInst[uSiteConfig], 
										   psSetup->psMoveUndo);
				}
				else
				{
					PINST	psNextAfterSite;

					psNextAfterSite = psSetup->apsSiteInst[uSiteConfig]->psNext;
					while (psNextAfterSite == psSetup->psPrecedInst || psNextAfterSite == psSetup->psSuccedInst)
					{
						psNextAfterSite = psNextAfterSite->psNext;
					}

					MoveInstAndUpdateIRegs(psState, 
										   psSetup->psPrecedInst, 
										   psSetup->apsSiteInst[uSiteConfig]->psNext, 
										   psSetup->psMoveUndo);
					MoveInstAndUpdateIRegs(psState, 
										   psSetup->psSuccedInst, 
										   psSetup->apsSiteInst[uSiteConfig]->psNext, 
										   psSetup->psMoveUndo);
				}
				bResult = IMG_TRUE;
			}
			if(uSiteConfig == DUALISSUE_SITE_CONFIG_INTER_SITE_PRECEDE_DW_SUCCEED_UP_CONVERGE_BTM)
			{
				psSetup->psSiteInst = psSetup->psSuccedInst;
				psSetup->psNonSiteInst = psSetup->psPrecedInst;
			}
			else
			{
				psSetup->psSiteInst = psSetup->psPrecedInst;
				psSetup->psNonSiteInst = psSetup->psSuccedInst;
			}
			break;
			
		case DUALISSUE_SITE_CONFIG_SUCCEED_SITE:
			/* Bring preceded instruction down *before* site */
			if(!(psSetup->psPrecedInst->psNext == psSetup->psSuccedInst))
			{
				psInstData = &psSetup->psPrecedInst->sStageData.psVDIData->sInstData;
				psInstData->psAnchorPoint = psSetup->psPrecedInst->psPrev;
				psInstData->bMoveMarking = IMG_TRUE;

				MoveInstAndUpdateIRegs(psState, 
									   psSetup->psPrecedInst, 
									   psSetup->apsSiteInst[uSiteConfig], 
									   psSetup->psMoveUndo);

				bResult = IMG_TRUE;
			}
			psSetup->psSiteInst = psSetup->psSuccedInst;
			psSetup->psNonSiteInst = psSetup->psPrecedInst;
			break;
			
		case DUALISSUE_SITE_CONFIG_PRECEDE_SITE:
			/* Bring succeeding instruction up *after* site */
			if(!(psSetup->psPrecedInst->psNext == psSetup->psSuccedInst))
			{
				psInstData = &psSetup->psSuccedInst->sStageData.psVDIData->sInstData;
				psInstData->psAnchorPoint = psSetup->psSuccedInst->psPrev;
				psInstData->bMoveMarking = IMG_TRUE;

				MoveInstAndUpdateIRegs(psState, 
									   psSetup->psSuccedInst, 
									   psSetup->psPrecedInst->psNext, 
									   psSetup->psMoveUndo);
				
				bResult = IMG_TRUE;
			}
			psSetup->psSiteInst = psSetup->psPrecedInst;
			psSetup->psNonSiteInst = psSetup->psSuccedInst;
			break;
			
		default:
			*pbIterate = IMG_FALSE;
			return IMG_FALSE;
	}
	
	/* Recalculate sliding info as instruction are moved in block */
	if(bResult)
	{
		PCODEBLOCK psCodeBlock;
		
		psCodeBlock = psSetup->apsSiteInst[uSiteConfig]->psBlock;
		VecDualComputeInstructionSlidingInfoBP(psState, psCodeBlock, psChangedPrecedInst, psChangedSuccedInst);
	}
	return bResult;
}

static
IMG_VOID VecDualRestoreInstructionPositions(PINTERMEDIATE_STATE psState, PDUALISSUE_SETUP psSetup, PCODEBLOCK psCodeBlock)
/*****************************************************************************
 FUNCTION	: VecDualRestoreInstructionPositions
    
 PURPOSE	: Restores instruction positions, relocated before by 
			  VecDualRepositionInstructionForSiteConfiguration.
		

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Code block to work upon
			  
 RETURNS	: Nothing
*****************************************************************************/
{
	PINST psRunner, psChangedPrecedInst = IMG_NULL, psChangedSuccedInst = IMG_NULL;
	PDUALISSUE_INST psInstData;
	
	for(psRunner = psCodeBlock->psBody; psRunner; psRunner = psRunner->psNext)
	{
		if(psRunner->eOpcode == IVDUAL)
			continue;
		
		psInstData = &psRunner->sStageData.psVDIData->sInstData;
		if(psInstData->bMoveMarking)
		{
			if(psInstData->psAnchorPoint && psInstData->psAnchorPoint->uBlockIndex < psRunner->uBlockIndex)
			{
				psChangedPrecedInst = (!psChangedPrecedInst 
											|| (psChangedPrecedInst->uBlockIndex > psInstData->psAnchorPoint->uBlockIndex)) ? 
													psInstData->psAnchorPoint : psChangedPrecedInst;
				
				psChangedSuccedInst = (!psChangedSuccedInst
											|| (psChangedSuccedInst->uBlockIndex < psRunner->uBlockIndex))?
												psRunner->psNext : psChangedSuccedInst;
			}
			else
			{
				psChangedPrecedInst = (!psChangedPrecedInst 
											|| (psChangedPrecedInst->uBlockIndex > psRunner->uBlockIndex)) ? 
													psRunner->psPrev : psChangedPrecedInst;
				if(psInstData->psAnchorPoint)
				{
					psChangedSuccedInst = (!psChangedSuccedInst
												|| (psChangedSuccedInst->uBlockIndex < psInstData->psAnchorPoint->uBlockIndex))?
													psRunner : psChangedSuccedInst;
				}
				else
				{
					psChangedSuccedInst = IMG_NULL;
				}
			}
			RemoveInst(psState, psCodeBlock, psRunner);
			InsertInstAfter(psState, psCodeBlock, psRunner, psInstData->psAnchorPoint);
			psInstData->psAnchorPoint = IMG_NULL;
			psInstData->bMoveMarking = IMG_FALSE;
		}
	}

	/*
		Undo changes to INST.auLiveChansInDest made when moving instructions.
	*/
	UndoInstMove(psState, psSetup->psMoveUndo);
	
	/* Recalculate sliding info as instruction are moved in block */
	VecDualComputeInstructionSlidingInfoBP(psState, psCodeBlock, psChangedPrecedInst, psChangedSuccedInst);
}

static
IMG_VOID VecDualResetInstructionSlidingInfo(PINTERMEDIATE_STATE	psState, 
											PCODEBLOCK			psCodeBlock)
/*****************************************************************************
 FUNCTION	: VecDualResetInstructionSlidingInfo
    
 PURPOSE	: Runs through all instructions of block to reset sliding positions
 			  info

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Code block to process
			  
 RETURNS	: Nothing 
*****************************************************************************/
{
	PINST psRunner;
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	for(psRunner = psCodeBlock->psBody; psRunner; psRunner = psRunner->psNext)
	{
		if(psRunner->sStageData.psVDIData)
		{
			/* Reset predicated position */
			psRunner->sStageData.psVDIData->psNewPredicatedPosition = IMG_NULL;
			psRunner->sStageData.psVDIData->eNewPositionMoveDir = DUALISSUE_SLIDING_MOVE_DIR_BOTH;
		}
	}
}

static
IMG_BOOL VecDualUpdateSlidingInstructions(PINTERMEDIATE_STATE	psState, 
											PCODEBLOCK			psCodeBlock)
/*****************************************************************************
 FUNCTION	: VecDualUpdateSlidingInstructions
    
 PURPOSE	: Runs through all instructions of block to reposition instructions

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Code block to process
			  
 RETURNS	: TRUE if instructions are moved.
*****************************************************************************/
{
	IMG_BOOL	bModified = IMG_FALSE;
	PINST		psRunner;
	
	/* Move instructions moving top */
	for(psRunner = psCodeBlock->psBodyTail; psRunner; psRunner = psRunner->psPrev)
	{
		if(	psRunner->sStageData.psVDIData && 
			psRunner->sStageData.psVDIData->psNewPredicatedPosition &&
			psRunner->sStageData.psVDIData->psNewPredicatedPosition == psRunner &&
			psRunner->sStageData.psVDIData->eNewPositionMoveDir == DUALISSUE_SLIDING_MOVE_DIR_TOP
			)
		{
			RemoveInst(psState, psRunner->psBlock, psRunner);
			InsertInstAfter(psState, psCodeBlock, psRunner, IMG_NULL);
			psRunner->sStageData.psVDIData->psNewPredicatedPosition = IMG_NULL;
			psRunner->sStageData.psVDIData->eNewPositionMoveDir = DUALISSUE_SLIDING_MOVE_DIR_BOTH;
			bModified = IMG_TRUE;
		}
	}
	
	/* Move instructions moving bottom */
	for(psRunner = psCodeBlock->psBody; psRunner; psRunner = psRunner->psNext)
	{
		if(	psRunner->sStageData.psVDIData && 
			psRunner->sStageData.psVDIData->psNewPredicatedPosition &&
			psRunner->sStageData.psVDIData->psNewPredicatedPosition == psRunner &&
			psRunner->sStageData.psVDIData->eNewPositionMoveDir == DUALISSUE_SLIDING_MOVE_DIR_BOTTOM)
		{
			RemoveInst(psState, psRunner->psBlock, psRunner);
			InsertInstBefore(psState, psCodeBlock, psRunner, IMG_NULL);
			psRunner->sStageData.psVDIData->psNewPredicatedPosition = IMG_NULL;
			psRunner->sStageData.psVDIData->eNewPositionMoveDir = DUALISSUE_SLIDING_MOVE_DIR_BOTH;
			bModified = IMG_TRUE;
		}
	}
	
	/* Move instructions moving up */
	for(psRunner = psCodeBlock->psBody; psRunner; psRunner = psRunner->psNext)
	{
		if(	psRunner->sStageData.psVDIData && 
			psRunner->sStageData.psVDIData->psNewPredicatedPosition &&
			psRunner->sStageData.psVDIData->psNewPredicatedPosition != psRunner &&
			psRunner->sStageData.psVDIData->eNewPositionMoveDir == DUALISSUE_SLIDING_MOVE_DIR_UP)
		{
			ASSERT(psRunner->sStageData.psVDIData->psNewPredicatedPosition->uBlockIndex < psRunner->uBlockIndex);
			
			RemoveInst(psState, psRunner->psBlock, psRunner);
			InsertInstAfter(psState, psCodeBlock, psRunner, psRunner->sStageData.psVDIData->psNewPredicatedPosition);
			psRunner->sStageData.psVDIData->psNewPredicatedPosition = IMG_NULL;
			psRunner->sStageData.psVDIData->eNewPositionMoveDir = DUALISSUE_SLIDING_MOVE_DIR_BOTH;
			bModified = IMG_TRUE;
		}
	}
	
	/* Move instructions moving down */
	for(psRunner = psCodeBlock->psBodyTail; psRunner; psRunner = psRunner->psPrev)
	{
		if(	psRunner->sStageData.psVDIData && 
			psRunner->sStageData.psVDIData->psNewPredicatedPosition &&
			psRunner->sStageData.psVDIData->psNewPredicatedPosition != psRunner &&
			psRunner->sStageData.psVDIData->eNewPositionMoveDir == DUALISSUE_SLIDING_MOVE_DIR_DOWN)
		{
			ASSERT(psRunner->sStageData.psVDIData->psNewPredicatedPosition->uBlockIndex > psRunner->uBlockIndex);
			
			RemoveInst(psState, psRunner->psBlock, psRunner);
			InsertInstBefore(psState, psCodeBlock, psRunner, psRunner->sStageData.psVDIData->psNewPredicatedPosition);
			psRunner->sStageData.psVDIData->psNewPredicatedPosition = IMG_NULL;
			psRunner->sStageData.psVDIData->eNewPositionMoveDir = DUALISSUE_SLIDING_MOVE_DIR_BOTH;
			bModified = IMG_TRUE;
		}
	}
	return bModified;
}

static
IMG_UINT32 VecDualDeriveLowerHalfSwizzle(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSrcSlotIdx)
/*****************************************************************************
 FUNCTION	: VecDualDeriveLowerHalfSwizzle
    
 PURPOSE	: Returns a swizzle without psVec->auSelectUpperHalf in effect.

 PARAMETERS	: psState			- Compiler state.
			  
 RETURNS	: Swizzle 
*****************************************************************************/
{
	IMG_UINT32	uPreSwizzleUsedChans, uSwizzle;
	
	ASSERT(GetBit(psInst->u.psVec->auSelectUpperHalf, uSrcSlotIdx));

	GetLiveChansInSourceSlot(psState, psInst, uSrcSlotIdx, &uPreSwizzleUsedChans, NULL /* puChansUsedPostSwizzle */);
			
	uSwizzle = psInst->u.psVec->auSwizzle[uSrcSlotIdx];
	uSwizzle = CombineSwizzles(USEASM_SWIZZLE(Z, W, 0, 0), uSwizzle);
	
	return uSwizzle;
}

static
IMG_BOOL VecDualSrcReplacementConstraints(PINTERMEDIATE_STATE psState,
										  PINST psInst,
										  IMG_UINT32 uArgIdx, 
										  IMG_BOOL bIsChangePoint,
										  PINST psScopeStart,
										  PINST psScopeEnd,
										  IMG_PVOID pvData)
/*****************************************************************************
 FUNCTION	: VecDualSrcReplacementConstraints
    
 PURPOSE	: Constraint check for register replacement, when passed
			  Instruction can have source arguments replaced
		
			  *All of replacement sites must passed constraints put*

 PARAMETERS	: psState			- Compiler state.
			  
			  
 RETURNS	: TRUE if replacement constraings are met
*****************************************************************************/
{
	IMG_UINT32 uSrcSlotIdx = uArgIdx / SOURCE_ARGUMENTS_PER_VECTOR;
	
	if(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_MEMSTORE)
	{
		/* assuming memory store as immovable */
		return IMG_FALSE;
	}
	
	if(IsDeschedulingPoint(psState, psInst) 
#if defined(OUTPUT_USPBIN)
		  || (g_psInstDesc[psInst->eOpcode].eType == INST_TYPE_SMPUNPACK)
#endif
		  || (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_TEXTURESAMPLE)
		  || (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_MEMLOAD))
	{
		if(bIsChangePoint)
		{
			/* Desched needs replacement */
			return IMG_FALSE;
		}
		/* Try to move deschedule out of this interval */
		if(!VecDualMoveInstructionGroup(psState, psInst, psScopeStart, psScopeEnd, DUALISSUE_SLIDING_MOVE_DIR_BOTH, pvData))
		{
			return IMG_FALSE;
		}
	}
	
	if(bIsChangePoint)
	{
		if(IsUniformSource(psState, &psInst->asArg[uArgIdx]))
		{
			return IMG_FALSE;
		}
		
		switch(psInst->eOpcode)
		{
			case IDELTA:
			case IVDUAL:
				return IMG_FALSE;
			default:
				break;
		}
		
		if (GetBit(psInst->u.psVec->auSelectUpperHalf, uSrcSlotIdx))
		{
			IMG_UINT32	uMatchedSwizzle, uPreSwizzleUsedChans, uSwizzle;
			
			ASSERT(!(psState->uFlags2 & USC_FLAGS2_NO_VECTOR_REGS));
			if(psInst->auDestMask[0] & (USC_Z_CHAN_MASK & USC_W_CHAN_MASK))
			{
				return IMG_FALSE;
			}
			
			GetLiveChansInSourceSlot(psState, psInst, uSrcSlotIdx, &uPreSwizzleUsedChans, NULL /* puChansUsedPostSwizzle */);
			
			uSwizzle = psInst->u.psVec->auSwizzle[uSrcSlotIdx];
			uSwizzle = CombineSwizzles(USEASM_SWIZZLE(Z, W, 0, 0), uSwizzle);
			if(!IsSwizzleSupported(	psState,
									psInst,
									psInst->eOpcode,
									uSrcSlotIdx,
									uSwizzle,
									uPreSwizzleUsedChans,
									&uMatchedSwizzle))
			{
				return IMG_FALSE;
			}
		}
	}
	return IMG_TRUE;
}

static
IMG_BOOL IsValidF32Destination(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: IsValidF32Destination
    
 PURPOSE	: Check if a vector instruction supports a GPI destination.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (
			(
				psInst->eOpcode == IVPCKFLTU8 && 
				!psInst->u.psVec->bPackScale
			) ||
			psInst->eOpcode == IVPCKFLTS8 ||
			psInst->eOpcode == IVPCKFLTS16 ||
			psInst->eOpcode == IVPCKFLTU16
		)
	{
		ASSERT(psInst->uDestCount == 1);
		if ((psInst->auDestMask[0] & ~USC_X_CHAN_MASK) != 0)
		{
			return IMG_FALSE;
		}
	}
	return IMG_TRUE;
}

static
IMG_BOOL RemoveDestShift(PINTERMEDIATE_STATE psState, PINST psDefInst, IMG_BOOL bCheckOnly)
/*****************************************************************************
 FUNCTION	: RemoveDestShift
    
 PURPOSE	: Undo modifications to an instruction to write a vec2 result to the 
			  upper half of a vec4.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.
			  bCheckOnly		- If TRUE just check if an undo is possible.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	auNewSwizzles[VECTOR_MAX_SOURCE_SLOT_COUNT];
	
	/*
		Default to no changes in the swizzles.
	*/
	memcpy(auNewSwizzles, psDefInst->u.psVec->auSwizzle, sizeof(auNewSwizzles));

	/*
		Update the source swizzles to keep the calculation the same for each channel in
		the destination.
	*/
	if ((g_psInstDesc[psDefInst->eOpcode].uFlags2 & DESC_FLAGS2_VECTORREPLICATEDRESULT) == 0)
	{
		IMG_UINT32	uSlot;
		IMG_UINT32	auNewLiveChansInArg[VECTOR_MAX_SOURCE_SLOT_COUNT];
		IMG_UINT32	uOldShift;
		
		uOldShift = psDefInst->u.psVec->uDestSelectShift;
		for (uSlot = 0; uSlot < GetSwizzleSlotCount(psState, psDefInst); uSlot++)
		{
			IMG_UINT32	uSwizzle;

			uSwizzle = psDefInst->u.psVec->auSwizzle[uSlot];
			uSwizzle <<= (uOldShift * USEASM_SWIZZLE_FIELD_SIZE);
			auNewSwizzles[uSlot] = uSwizzle;

			GetLiveChansInSourceSlot(psState,
									 psDefInst,
									 uSlot,
									 &auNewLiveChansInArg[uSlot],
									 NULL /* puChansUsedPostSwizzle */);

			auNewLiveChansInArg[uSlot] <<= uOldShift;
		}

		/*
			Check the new set of swizzles is supported by the instruction.
		*/
		if (!IsSwizzleSetSupported(psState, psDefInst->eOpcode, psDefInst, auNewLiveChansInArg, auNewSwizzles))
		{
			return IMG_FALSE;
		}
	}

	if (!bCheckOnly)
	{
		psDefInst->u.psVec->uDestSelectShift = 0;
		memcpy(psDefInst->u.psVec->auSwizzle, auNewSwizzles, sizeof(auNewSwizzles));
	}
	return IMG_TRUE;
}

static
IMG_BOOL VecDualDestReplacementConstraints(PINTERMEDIATE_STATE psState,
										   PINST psInst,
										   IMG_UINT32 uDstIdx,
										   IMG_BOOL bIsChangePoint,
										   PINST psScopeStart,
										   PINST psScopeEnd,
										   IMG_PVOID pvData)
/*****************************************************************************
 FUNCTION	: VecDualDestReplacementConstraints
    
 PURPOSE	: Constraint check for register replacement, when passed
 			  Instruction can have destination args replaced.
			
			  *All of replacement sites must passed constraints put*

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction for which register replacement 
			  					  will be done.
			  bIsChangePoint	- Indicates whether instruction is only an
			  					  Intermediate instruction or is decided to
			  					  be a change point.
 RETURNS	: TRUE if the two instructions are independent
*****************************************************************************/
{
	if(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_MEMSTORE)
	{
		/* assuming memory store as immovable */
		return IMG_FALSE;
	}
	
	if(IsDeschedulingPoint(psState, psInst) 
#if defined(OUTPUT_USPBIN)
		  || (g_psInstDesc[psInst->eOpcode].eType == INST_TYPE_SMPUNPACK)
#endif
		  || (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_TEXTURESAMPLE)
		  || (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_MEMLOAD))
	{
		if(bIsChangePoint)
		{
			/* Desched needs replacement */
			return IMG_FALSE;
		}
		
		/* Try to move deschedule out of this interval */
		if(!VecDualMoveInstructionGroup(psState, psInst, psScopeStart, psScopeEnd, DUALISSUE_SLIDING_MOVE_DIR_BOTH, pvData))
		{
			return IMG_FALSE;
		}
	}
	
	if(bIsChangePoint)
	{
		switch(psInst->eOpcode)
		{
			case IDELTA:
			case IVDUAL:
				return IMG_FALSE;
			default:
				break;
		}
		
		if(psInst->asDest[uDstIdx].uType != USEASM_REGTYPE_FPINTERNAL &&
			ReferencedOutsideBlock(psState, psInst->psBlock, &psInst->asDest[uDstIdx]))
		{
			return IMG_FALSE;
		}
		
		if (!IsValidF32Destination(psState, psInst))
		{
			return IMG_FALSE;
		}
		
		if(psInst->apsOldDest && psInst->apsOldDest[uDstIdx])
		{
			IMG_UINT32 uDestLocation;
			PINST psOldDestDefInst;
			
			ASSERT(psInst->apsOldDest[uDstIdx]->psRegister);
			ASSERT(psInst->apsOldDest[uDstIdx]->psRegister->psUseDefChain);
			
			psOldDestDefInst = UseDefGetDefInstFromChain(psInst->apsOldDest[uDstIdx]->psRegister->psUseDefChain, &uDestLocation);
			if(!psOldDestDefInst)
			{
				return IMG_FALSE;
			}
			
			if(IsUniformSource(psState, psInst->apsOldDest[uDstIdx]))
			{
				return IMG_FALSE;
			}
			
			if(ReferencedOutsideBlock(psState, psInst->psBlock, psInst->apsOldDest[uDstIdx]))
			{
				return IMG_FALSE;
			}
		}

		ASSERT(g_psInstDesc[psInst->eOpcode].eType == INST_TYPE_VEC);
		if (psInst->u.psVec->uDestSelectShift > 0)
		{
			if (!RemoveDestShift(psState, psInst, IMG_TRUE /* bCheckOnly */))
			{
				return IMG_FALSE;
			}
		}
		
		if(psState->uFlags2 & USC_FLAGS2_NO_VECTOR_REGS)
		{
			/* Check the alignment */
			if(!((psInst->asDest[uDstIdx].uNumber % (VECTOR_LENGTH / 2)) == uDstIdx)
					&& 
					( ((psInst->eOpcode != IVDP || psInst->eOpcode != IVDP3))
					|| ((psInst->eOpcode == IVDP || psInst->eOpcode == IVDP3) && psInst->uDestCount > 1)))
			{
				return IMG_FALSE;
			}
		}
	}
	return IMG_TRUE;
}


#define VDUAL_CACHED_IREG_INFO_VALID		0x80000000
static
IMG_VOID VecDualAllocateCachedIRegInfo(PINTERMEDIATE_STATE	psState,
									   PDUALISSUE_SETUP		psSetup)
/*****************************************************************************
 FUNCTION	: VecDualFreeCachedIRegInfo
    
 PURPOSE	: Allocate structs for cached data for 
			  VecDualGetAvailableInternalRegsBetweenInterval

 PARAMETERS	: psState					- Compiler state
 			  

 RETURNS	: Nothing
*****************************************************************************/
{
	psSetup->ppuAvailableIReg = UscAlloc(psState, sizeof(IMG_UINT32) * psSetup->psBlock->uInstCount * psSetup->psBlock->uInstCount);
	memset(psSetup->ppuAvailableIReg, 0, sizeof(IMG_UINT32) * psSetup->psBlock->uInstCount * psSetup->psBlock->uInstCount);
}

static
IMG_VOID VecDualFreeCachedIRegInfo(PINTERMEDIATE_STATE	psState,
								   PDUALISSUE_SETUP		psSetup)
/*****************************************************************************
 FUNCTION	: VecDualFreeCachedIRegInfo
    
 PURPOSE	: Free cached data for VecDualGetAvailableInternalRegsBetweenInterval

 PARAMETERS	: psState					- Compiler state
 			  

 RETURNS	: Nothing
*****************************************************************************/
{
	if(psSetup->ppuAvailableIReg)
	{
		UscFree(psState, psSetup->ppuAvailableIReg);
	}
}

static
IMG_UINT32 VecDualGetAvailableInternalRegsBetweenInterval(PINTERMEDIATE_STATE	psState,
														  PDUALISSUE_SETUP		psSetup,
														  PINST					psStartInst,
														  PINST					psEndInst)
/*****************************************************************************
 FUNCTION	: VecDualGetAvailableInternalRegsBetweenInterval
    
 PURPOSE	: Wrapper function to GetAvailableInternalRegsBetweenInterval,
			  But with results caching.

 PARAMETERS	: psState					- Compiler state
 			  psStartInst				- Interval start
 			  psEndInst					- Interval end

 RETURNS	: Available IRegs in bit set
*****************************************************************************/
{
#if defined(DEBUG_VECDUAL_NO_COMPUT_OPTIMIZATION)
	PVR_UNREFERENCED_PARAMETER(psSetup);
	return GetAvailableInternalRegsBetweenInterval(psState, psStartInst, psEndInst);
#else
	IMG_UINT32 uAvailableIReg;
	IMG_UINT32 uInstCount = psSetup->psBlock->uInstCount;
	
	if(psSetup->ppuAvailableIReg && !psSetup->bInstIntervalValidate)
	{
		/* Give cached value */
		uAvailableIReg = psSetup->ppuAvailableIReg[(psStartInst->uBlockIndex * uInstCount) + psEndInst->uBlockIndex];
		if(!(uAvailableIReg & VDUAL_CACHED_IREG_INFO_VALID))
		{
			uAvailableIReg = GetAvailableInternalRegsBetweenInterval(psState, psStartInst, psEndInst);
			uAvailableIReg |= VDUAL_CACHED_IREG_INFO_VALID;
			psSetup->ppuAvailableIReg[(psStartInst->uBlockIndex * uInstCount) + psEndInst->uBlockIndex] = uAvailableIReg;
		}
	}
	else
	{
		/* Flush all ireg available between interval info */
		if(psSetup->ppuAvailableIReg)
		{
			VecDualFreeCachedIRegInfo(psState, psSetup);
		}
		VecDualAllocateCachedIRegInfo(psState, psSetup);

		uAvailableIReg = GetAvailableInternalRegsBetweenInterval(psState, psStartInst, psEndInst);
		uAvailableIReg |= VDUAL_CACHED_IREG_INFO_VALID;
		psSetup->ppuAvailableIReg[(psStartInst->uBlockIndex * uInstCount) + psEndInst->uBlockIndex] = uAvailableIReg;
		psSetup->bInstIntervalValidate = IMG_FALSE;
	}
	return (uAvailableIReg & ~VDUAL_CACHED_IREG_INFO_VALID);
#endif
}

static
IMG_BOOL VecDualSubstituteIRegBetweenInterval(	PINTERMEDIATE_STATE		psState,
												PDUALISSUE_SETUP		psSetup,
												PINST					psStartInst,
												PINST					psEndInst,
												IMG_UINT32				uGivenReplacementsIReg,
												IMG_UINT32				*puNewIReg,
												IMG_UINT32				uOldIReg)
/*****************************************************************************
 FUNCTION	: VecDualSubstituteIRegBetweenInterval
    
 PURPOSE	: Will perform a constraint check, widen its substitution area
 			  based upon IReg definations & references. And perform the
 			  substitution.

 PARAMETERS	: psState					- Compiler state
 			  psStartInst				- Interval start
 			  psEndInst					- Interval end
 			  uGivenReplacementsIReg	- Replacing IReg can be one of these
 			  puNewIReg					- New IReg used for replacement
 			  uOldIReg					- IReg to replace
 RETURNS	: TRUE if the two instructions are independent
*****************************************************************************/
{
	IMG_UINT32		uAvailableIReg, uNewIReg;
	IMG_UINT32		uIndex;
	PINST 			psRunner, psLastDefForSpan = IMG_NULL, psLastRefForSpan = IMG_NULL;

	IMG_UINT32		uVecMask = (psState->uFlags2 & USC_FLAGS2_NO_VECTOR_REGS) ?  
									USC_ALL_CHAN_MASK : USC_X_CHAN_MASK;
	
	UseDefGetIRegLivenessSpanOverInterval(psState, psStartInst, psEndInst, uOldIReg, &psLastDefForSpan, &psLastRefForSpan);
	
	if(!psLastDefForSpan || !psLastRefForSpan)
	{
		return IMG_FALSE;
	}

	psStartInst = psLastDefForSpan;
	psEndInst = psLastRefForSpan;
	uAvailableIReg = VecDualGetAvailableInternalRegsBetweenInterval(psState, psSetup, psStartInst, psEndInst);
	uAvailableIReg = uAvailableIReg & uGivenReplacementsIReg;
	if(!uAvailableIReg)
	{
		return IMG_FALSE;
	}
	
	if(!SelectInternalReg(uAvailableIReg, uVecMask, &uNewIReg))
	{
		return IMG_FALSE;
	}
	*puNewIReg = uNewIReg;

	for(psRunner = psStartInst; psRunner != psEndInst->psNext; psRunner = psRunner->psNext)
	{
		if(psRunner != psEndInst)
		{
			for(uIndex = 0; uIndex < psRunner->uDestCount; uIndex++)
			{
				if(psRunner->asDest[uIndex].uType == USEASM_REGTYPE_FPINTERNAL &&
							psRunner->asDest[uIndex].uNumber == uOldIReg)
				{
					ARG sSubstitue = psRunner->asDest[uIndex];
					SetDest(psState, psRunner, uIndex, sSubstitue.uType, uNewIReg, sSubstitue.eFmt);
				}
				if(psRunner->apsOldDest && psRunner->apsOldDest[uIndex])
				{
					if(psRunner->apsOldDest[uIndex]->uType == USEASM_REGTYPE_FPINTERNAL &&
							psRunner->apsOldDest[uIndex]->uNumber == uOldIReg)
					{
						ARG sSubstitue = *psRunner->apsOldDest[uIndex];
						sSubstitue.uNumber = uNewIReg;
						SetPartiallyWrittenDest(psState, psRunner, uIndex, &sSubstitue);
					}
				}
			}
		}
		
		if(psRunner != psStartInst)
		{
			for(uIndex = 0; uIndex < psRunner->uArgumentCount; uIndex++)
			{
				if(psRunner->asArg[uIndex].uType == USEASM_REGTYPE_FPINTERNAL &&
							psRunner->asArg[uIndex].uNumber == uOldIReg)
				{
					ARG sSubstitue = psRunner->asArg[uIndex];
					
					SetSrc(psState, psRunner, uIndex, sSubstitue.uType, 
						uNewIReg, sSubstitue.eFmt);
				}
			}
		}
	}
	return IMG_TRUE;
}

static
IMG_BOOL VecDualValidatePairForIRegReplacement(	PINTERMEDIATE_STATE psState,
												PDUALISSUE_INST		psInst1Data,
												PDUALISSUE_INST		psInst2Data)
/*****************************************************************************
 FUNCTION	: VecDualValidatePairForIRegReplacement
    
 PURPOSE	: Performes early test to eliminate pair before
 			  selecting primary and secondary instruction,
 			  and site to converge.

 PARAMETERS	: psState			- Compiler state.
			  psPriData			- Instruction 1 dual data
			  psSecData			- Instruction 2 dual data
			  
 RETURNS	: TRUE if the two instruction pair is valid.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst1Data);
	PVR_UNREFERENCED_PARAMETER(psInst2Data);
	
	return IMG_TRUE;
}

static
IMG_VOID VecDualUpdateSrcReplacement(PINTERMEDIATE_STATE psState, PINST psUseInst, IMG_UINT32 uArgIdx, const ARG *pcsSubstitue)
/*****************************************************************************
 FUNCTION	: VecDualUpdateSrcReplacement
    
 PURPOSE	: Performes post replacement updates to instruction.

 PARAMETERS	: psState			- Compiler state.
			  psUseInst			- Instruction which has sources replaced
			  
 RETURNS	: Nothing
*****************************************************************************/
{
	IMG_UINT32	uSlotIdx;
	
	SetSrc(psState, psUseInst, uArgIdx, pcsSubstitue->uType, pcsSubstitue->uNumber, pcsSubstitue->eFmt);
	
	/* Adjustment for UpperHaft */
	uSlotIdx = uArgIdx / SOURCE_ARGUMENTS_PER_VECTOR;
	{
		if(GetBit(psUseInst->u.psVec->auSelectUpperHalf, uSlotIdx))
		{
			IMG_UINT32	uMatchedSwizzle, uPreSwizzleUsedChans, uSwizzle;
			
			GetLiveChansInSourceSlot(psState, psUseInst, uSlotIdx, &uPreSwizzleUsedChans, NULL /* puChansUsedPostSwizzle */);
			
			uSwizzle = psUseInst->u.psVec->auSwizzle[uSlotIdx];
			uSwizzle = CombineSwizzles(USEASM_SWIZZLE(Z, W, 0, 0), uSwizzle);
			if(!IsSwizzleSupported(	psState,
									psUseInst,
									psUseInst->eOpcode,
									uSlotIdx,
									uSwizzle,
									uPreSwizzleUsedChans,
									&uMatchedSwizzle))
			{
				imgabort();
			}
			psUseInst->u.psVec->auSwizzle[uSlotIdx] = uMatchedSwizzle;
			SetBit(psUseInst->u.psVec->auSelectUpperHalf, uSlotIdx, 0);
		}
		psUseInst->u.psVec->aeSrcFmt[uSlotIdx] = pcsSubstitue->eFmt;
	}
}

static
IMG_VOID VecDualUpdateDestReplacement(PINTERMEDIATE_STATE psState, PINST psDefInst, IMG_UINT32 uDestIdx, const ARG *pcsSubstitue)
/*****************************************************************************
 FUNCTION	: VecDualUpdateDestReplacement
    
 PURPOSE	: Replaces a destination of an instruction used by 
				ApplyConstraintArgumentReplacementDAG 

 PARAMETERS	: psState			- Compiler state.
			  psDefInst			- Instruction to change
			  uDestIdx			- Index to destination to change
			  pcsSubstitue		- New destination
 RETURNS	: TRUE if the two instructions can be dual-issued.
*****************************************************************************/
{
	ASSERT(g_psInstDesc[psDefInst->eOpcode].eType == INST_TYPE_VEC);
	if (psDefInst->u.psVec->uDestSelectShift > 0)
	{
		IMG_BOOL	bRet;

		bRet = RemoveDestShift(psState, psDefInst, IMG_FALSE /* bCheckOnly */);
		ASSERT(bRet);
	}

	SetDest(psState, psDefInst, uDestIdx, pcsSubstitue->uType, pcsSubstitue->uNumber, pcsSubstitue->eFmt);
}

static
IMG_BOOL VecDualCheckSourceIsReplacable(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uArgIdx)
/*****************************************************************************
 FUNCTION	: VecDualCheckSourceIsReplacable
    
 PURPOSE	: Primary check for replacement possiblility

 PARAMETERS	: psState			- Compiler state.
			  psDefInst			- Instruction to change
			  uArgIdx			- Index to source to change
			  
 RETURNS	: TRUE if the two instructions can be dual-issued.
*****************************************************************************/
{
	PINST	psArgStartInst = IMG_NULL, psArgEndInst = IMG_NULL, psOldDestStartInst = IMG_NULL;
	PARG	psVecArg = &psInst->asArg[uArgIdx];
					
	if(psVecArg->uType == USEASM_REGTYPE_FPINTERNAL)
	{
		return IMG_TRUE;
	}
	else
	{
		if(psVecArg->uType != USEASM_REGTYPE_FPCONSTANT)
		{
			GetLivenessSpanForArgumentDAG(psState, psInst, uArgIdx,
							&psArgStartInst, &psOldDestStartInst, &psArgEndInst);
			
			if(psArgStartInst && psArgEndInst)
			{
				return IMG_TRUE;
			}
		}
	}
	return IMG_FALSE;
}

static
DUALISSUE_UNIFIEDSTORE_FORMAT GetUnifiedStoreFormatForArg(PINTERMEDIATE_STATE psState, PARG psArg)
/*****************************************************************************
 FUNCTION	: GetUnifiedStoreFormatForArg
    
 PURPOSE	: Check if a source or destination argument requires the flag
			  to interpret unified store source/destinations to a dual-issue
			  instruction to be either set or clear.

 PARAMETERS	: psState			- Compiler state.
			  psArg				- Source or destination argument.
			  
 RETURNS	: The required state of the flag.
*****************************************************************************/
{
	if (psArg->uType == USEASM_REGTYPE_FPINTERNAL)
	{
		return DUALISSUE_UNIFIEDSTORE_FORMAT_ANY;
	}
	else
	{
		if (psArg->eFmt == UF_REGFORMAT_F32)
		{
			return DUALISSUE_UNIFIEDSTORE_FORMAT_F32;
		}
		else
		{
			ASSERT(psArg->eFmt == UF_REGFORMAT_F16);
			return DUALISSUE_UNIFIEDSTORE_FORMAT_F16;
		}
	}
}

static
IMG_BOOL VecDualPreparePairForIRegReplacement(PINTERMEDIATE_STATE	psState, 
											  PINST					psPriInst,
											  PINST					psSecInst,
											  PDUALISSUE_INST		psPriData,
											  PDUALISSUE_INST		psSecData,
											  PDUALISSUE_SETUP		psSetup,
											  IMG_UINT32			*puExitHint)
/*****************************************************************************
 FUNCTION	: VecDualPreparePairForIRegReplacement
    
 PURPOSE	: Prepares a instruction pair for dual issue, returns false if not 
			  possible

 PARAMETERS	: psState			- Compiler state.
			  psPriInst			- Selected Primary instruction
			  psSecInst			- Selected Secondary instruction
			  psPriData			- Primary instruction dual data
			  psSecData			- Secondary instruction dual data
			  psSetup			- Returns information about the parameters
								for a dual-issue instruction combining
								both of the instructions.
			  puExitHint		- Debugging purpose.
			  
 RETURNS	: TRUE if the two instructions can be dual-issued.
*****************************************************************************/
{
	IMG_UINT32 					uMapIdx;
	IMG_UINT32 					uSrcIdx;
	PCDUALISSUEVECTOR_SRCSLOT	psPrimMap, psSecMap;
	
	IMG_BOOL					bSucceedDestNeedsReplacement = IMG_FALSE;
	ARG							sSucceedDestination;
	IMG_UINT32					uReplacementsRequired = 0, uReplacementsDone = 0;
	IMG_UINT32					uIRegEngaged = 0, uSiteEngagedIRegs = 0;
	
	IMG_UINT32 uSelectedCfg = psSetup->uSrcCfg;
	
	IMG_UINT32 uPriOpSrcCount = psPriData->uSrcCount - 1;
	IMG_UINT32 uSecOpSrcCount = psSecData->uSrcCount - 1;

	ASSERT(psSetup->bCheckUseDefConstraints);
	ASSERT(psSetup->psSiteInst);
	ASSERT(psSetup->psNonSiteInst);
	
	/* Make sure site is one of preceding or succeeding instruction */
	ASSERT(psSetup->psSiteInst == psSetup->psPrecedInst || psSetup->psSiteInst == psSetup->psSuccedInst);
	ASSERT(psSetup->psNonSiteInst == psSetup->psPrecedInst || psSetup->psNonSiteInst == psSetup->psSuccedInst);

	
	if(psSetup->bSucceedInstDefiningIntervalDef || psSetup->bSucceedInstDefiningIntervalRef)
	{
		bSucceedDestNeedsReplacement = IMG_TRUE;
		sSucceedDestination = psSetup->psSuccedInst->asDest[0];
	}
	
	psSetup->ePrimDestSlotConfiguration = VDUAL_DESTSLOT_GPI;
	/*
		This pair can be dual issued if enough of IRegs are available over interval 
	*/
	psPrimMap = g_aapeDualIssueVectorPrimaryMap[uPriOpSrcCount][uSelectedCfg];
	psSecMap = g_aaapeDualIssueVectorSecondaryMap[uPriOpSrcCount][uSecOpSrcCount][uSelectedCfg];
	
	VecDualResetInstructionSlidingInfo(psState, psPriInst->psBlock);
	psPriInst->sStageData.psVDIData->eNewPositionMoveDir = DUALISSUE_SLIDING_MOVE_DIR_DUALPART;
	psSecInst->sStageData.psVDIData->eNewPositionMoveDir = DUALISSUE_SLIDING_MOVE_DIR_DUALPART;

	{
		struct _DIMAP_
		{
			PINST						psInst;
			PDUALISSUE_INST				psInstData;
			PCDUALISSUEVECTOR_SRCSLOT	psSrcSlot;
			IMG_UINT32					ui32SrcCount;
			IMG_PUINT32					puIRegEngaged;
			PARG						psTargetArgs;
		} asDIMap[2];
		
		IMG_UINT32						uGPI2SlotReserved = 0;
		PINST							psInstUSSrc = IMG_NULL;
		PARG							psUSDest;
		DUALISSUE_UNIFIEDSTORE_FORMAT	eUSDestFormat;
		DUALISSUE_UNIFIEDSTORE_FORMAT	eUSSrcFormat = DUALISSUE_UNIFIEDSTORE_FORMAT_ANY;
		
		/* Setup the converge parameters, few hints for sliding desceduling instructions */
		psSetup->sConvergeData.psConvergeScopeStart = psSetup->psPrecedInst;
		psSetup->sConvergeData.psConvergeScopeEnd = psSetup->psSuccedInst;
		psSetup->sConvergeData.psConvergePoint = psSetup->psSiteInst;
		
		/* Setup structure for quick traversal */
		asDIMap[0].psInst = psPriInst;
		asDIMap[0].psInstData = psPriData;
		asDIMap[0].psSrcSlot = psPrimMap;
		asDIMap[0].ui32SrcCount = psPriData->uSrcCount;
		asDIMap[0].puIRegEngaged = psPriData->auIRegEngaged;
		asDIMap[0].psTargetArgs = psPriData->asTargetArgs;
		
		asDIMap[1].psInst = psSecInst;
		asDIMap[1].psInstData = psSecData;
		asDIMap[1].psSrcSlot = psSecMap;
		asDIMap[1].ui32SrcCount = psSecData->uSrcCount;
		asDIMap[1].puIRegEngaged = psSecData->auIRegEngaged;
		asDIMap[1].psTargetArgs = psSecData->asTargetArgs;
		
		/* See if DUALISSUEVECTOR_SRCSLOT_GPI2 is referenced in this configuration */
		for (uMapIdx = 0; uMapIdx < (sizeof(asDIMap) / sizeof(asDIMap[0])); uMapIdx++)
		{
			for (uSrcIdx = 0; uSrcIdx < asDIMap[uMapIdx].ui32SrcCount; uSrcIdx++)
			{
				asDIMap[uMapIdx].puIRegEngaged[uSrcIdx] = psState->uGPISizeInScalarRegs;
				memset(&asDIMap[uMapIdx].psTargetArgs[uSrcIdx], 0, sizeof(asDIMap[uMapIdx].psTargetArgs[uSrcIdx]));
				
				if(asDIMap[uMapIdx].psSrcSlot[uSrcIdx] == DUALISSUEVECTOR_SRCSLOT_GPI2)
				{
					uGPI2SlotReserved = (psSetup->bExpandVectors ? 8 : 2);
				}
				else
				if(asDIMap[uMapIdx].psSrcSlot[uSrcIdx] == DUALISSUEVECTOR_SRCSLOT_UNIFIEDSTORE)
				{
					psInstUSSrc = asDIMap[uMapIdx].psInst;
				}
			}
		}
		
		/* Calculate IReg engaged due to movement of instruction */
		for (uMapIdx = 0; uMapIdx < (sizeof(asDIMap) / sizeof(asDIMap[0])); uMapIdx++)
		{
			IMG_UINT32		uVecMask;
			IMG_UINT32		uDstIdx;
			PINST			psInst;
			PDUALISSUE_INST psInstData = asDIMap[uMapIdx].psInstData;
			
			psInst = asDIMap[uMapIdx].psInst;
			
			uVecMask = (psSetup->bExpandVectors) ?  USC_ALL_CHAN_MASK : USC_X_CHAN_MASK;
			
			for (uDstIdx = 0; uDstIdx < min(psInst->uDestCount, SCALAR_REGISTERS_PER_F32_VECTOR); uDstIdx++)
			{
				if(psInst->asDest[uDstIdx].uType == USEASM_REGTYPE_FPINTERNAL)
				{
					uIRegEngaged |= (uVecMask << psInst->asDest[uDstIdx].uNumber);
				}
			}
			
			for (uSrcIdx = 0; uSrcIdx < asDIMap[uMapIdx].ui32SrcCount; uSrcIdx++)
			{
				IMG_BOOL					bAssignNewIReg = IMG_TRUE;
				IMG_UINT32					uArgIdx, uAvailableIReg, uRegIRegSelected;
				PINST						psStartInst, psEndInst, psArgStartInst, psArgEndInst, psOldDestStartInst;
				PARG						psVecArg;
				DUALISSUEVECTOR_SRCSLOT		eSlotOption;

				uArgIdx = uSrcIdx * SOURCE_ARGUMENTS_PER_VECTOR + (psSetup->bExpandVectors ? 1 : 0);
				psVecArg = &psInst->asArg[uArgIdx];
				eSlotOption = asDIMap[uMapIdx].psSrcSlot[uSrcIdx];
				
				asDIMap[uMapIdx].psTargetArgs[uSrcIdx] = *psVecArg;
				
				if(psSetup->bVec3)
				{
					if(!(psInstData->auFlagValidSwizzleSet[uSrcIdx] 
							& (DUALISSUE_SWIZZLE_OPTIONS_IDENTITY 
								| DUALISSUE_SWIZZLE_OPTIONS_VEC3STD 
								| DUALISSUE_SWIZZLE_OPTIONS_VEC3EXT
								| DUALISSUE_SWIZZLE_OPTIONS_DERIVED_VEC3)))
					{
						RETURN_WITH_HINT(IMG_FALSE);
					}
				}
				else
				{
					if(!(psInstData->auFlagValidSwizzleSet[uSrcIdx] 
							& (DUALISSUE_SWIZZLE_OPTIONS_IDENTITY 
								| DUALISSUE_SWIZZLE_OPTIONS_VEC4STD 
								| DUALISSUE_SWIZZLE_OPTIONS_VEC4EXT
								| DUALISSUE_SWIZZLE_OPTIONS_DERIVED_VEC4)))
					{
						RETURN_WITH_HINT(IMG_FALSE);
					}
				}
				switch(eSlotOption)
				{
					case DUALISSUEVECTOR_SRCSLOT_UNIFIEDSTORE:
					{
						if(g_asDualIssueOpDesc[psInstData->eSetupOp].bVectorResult
							&& (psInstData->uDestWriteMask & (USC_Z_CHAN_MASK | USC_W_CHAN_MASK))
							&& !psSetup->bF16
							)
						{
							RETURN_WITH_HINT(IMG_FALSE);
						}
						if(psInst == psPriInst)
						{
							psSetup->ePrimDestSlotConfiguration = VDUAL_DESTSLOT_UNIFIEDSTORE;
						}

						/*
							Check if the instruction using the unified store destination requires unified store 
							source/destinations to the combined instruction to be interpreted as either F32 or F16.
						*/
						eUSSrcFormat = GetUnifiedStoreFormatForArg(psState, psVecArg);
						break;
					}
					case DUALISSUEVECTOR_SRCSLOT_GPI2:
						if(
							((!psInstData->bScalarAvailable) 
								&& (!(psInstData->auFlagValidSwizzleSet[uSrcIdx] & DUALISSUE_SWIZZLE_OPTIONS_IDENTITY)))
							|| (psInstData->bScalarAvailable 
								&& (!(psInstData->auFlagValidSwizzleSet[uSrcIdx] & DUALISSUE_SWIZZLE_OPTIONS_DERIVED_VEC3)))
							|| (psInstData->bScalarAvailable 
								&& (!(psInstData->auFlagValidSwizzleSet[uSrcIdx] & DUALISSUE_SWIZZLE_OPTIONS_DERIVED_VEC4)))
						  )
						{
							RETURN_WITH_HINT(IMG_FALSE);
						}
						/* Intensional fall through*/
					case DUALISSUEVECTOR_SRCSLOT_GPI0:
					case DUALISSUEVECTOR_SRCSLOT_GPI1:
						/* Preliminary check whether instructions in data flow supports register replacement */
						/* Do we need an replacement */
						if(psVecArg->uType == USEASM_REGTYPE_FPINTERNAL)
						{
							if((eSlotOption == DUALISSUEVECTOR_SRCSLOT_GPI2)
									&& (psVecArg->uNumber != (psSetup->bExpandVectors ? 8U : 2U)))
							{
								asDIMap[uMapIdx].puIRegEngaged[uSrcIdx] = 2U;
								/* DUALISSUEVECTOR_SRCSLOT_GPI2 isn't using IReg according to specification */
								bAssignNewIReg = IMG_TRUE;
							}
							else
							{
								asDIMap[uMapIdx].puIRegEngaged[uSrcIdx] = psVecArg->uNumber;
								/* Source is already meeting GPI constraints */
								continue;
							}
						}
						else
						{
							#if !defined(DEBUG_VECDUAL_NO_COMPUT_OPTIMIZATION)
							/*
								Quick exit, if it is previously determined this source can not be substituted by ireg 
							*/
							if(!(psInstData->uSrcIRegSubstitutionFlags & (1 << uSrcIdx)))
							{
								RETURN_WITH_HINT(IMG_FALSE);
							}
							#endif
							
							if(!psInstData->bMayNeedInternalRegAssingment)
							{
								/* Instruction don't support source replacement */
								RETURN_WITH_HINT(IMG_FALSE);
							}
							/* Check constraints for replacing US source with IReg */
					
							if(!ApplyConstraintArgumentReplacementDAG(psState, psInst, uArgIdx,
									VecDualSrcReplacementConstraints, VecDualDestReplacementConstraints,
									VecDualUpdateSrcReplacement, VecDualUpdateDestReplacement,
									VecDualUpdateInstructionAfterSubstitution,
									IMG_TRUE, IMG_NULL, (IMG_PVOID)psSetup))
							{
								/* Mark source can't be substituted with ireg */
								psInstData->uSrcIRegSubstitutionFlags &= ~(1 << uSrcIdx);
								RETURN_WITH_HINT(IMG_FALSE);
							}
							
							/* Check if argument isn't already marked for replacement */
							{
								IMG_UINT32 uMapIdx1, uSrcIdx1;
								
								for (uMapIdx1 = 0; uMapIdx1 <= uMapIdx; uMapIdx1++)
								{
									for (uSrcIdx1 = 0; uSrcIdx1 < asDIMap[uMapIdx].ui32SrcCount; uSrcIdx1++)
									{
										if(CompareArgs(&asDIMap[uMapIdx1].psTargetArgs[uSrcIdx1], psVecArg) == 0 &&
										asDIMap[uMapIdx1].puIRegEngaged[uSrcIdx1] != psState->uGPISizeInScalarRegs)
										{
											asDIMap[uMapIdx].psTargetArgs[uSrcIdx] = 
													asDIMap[uMapIdx1].psTargetArgs[uSrcIdx1];
											asDIMap[uMapIdx].puIRegEngaged[uSrcIdx] = 
													asDIMap[uMapIdx1].puIRegEngaged[uSrcIdx1];
											bAssignNewIReg = IMG_FALSE;
										}
									}
								}
							}
						}
						
						if(bAssignNewIReg)
						{
							uReplacementsRequired++;

							/* Create a list of IReg substitution */
							GetLivenessSpanForArgumentDAG(psState, psInst, uArgIdx, &psArgStartInst, &psOldDestStartInst, &psArgEndInst);
							
							if(	!psArgStartInst
								|| !psArgEndInst
								|| (psOldDestStartInst && psOldDestStartInst->psBlock != psArgStartInst->psBlock)
									)
							{
								/* Mark source can't be substituted with ireg */
								psInstData->uSrcIRegSubstitutionFlags &= ~(1 << uSrcIdx);
								
								/* Argument is initialized outside the block or producing result for outside */
								RETURN_WITH_HINT(IMG_FALSE);
							}
							
							if(eSlotOption == DUALISSUEVECTOR_SRCSLOT_GPI2 
													&& psVecArg->uType == USEASM_REGTYPE_FPINTERNAL)
							{
								uAvailableIReg = VecDualGetAvailableInternalRegsBetweenInterval(psState, 
																								psSetup,
																								(psOldDestStartInst) ? psOldDestStartInst : psArgStartInst, 
																								psArgEndInst);
								uRegIRegSelected = uGPI2SlotReserved;
								if((~uAvailableIReg) & (uVecMask << uRegIRegSelected))
								{
									IMG_UINT32 uRegIRegSubstituedWith;
									
									uAvailableIReg &= ~(uVecMask << uRegIRegSelected);
									
									/* IReg adjustment for trying get i2 for DUALISSUEVECTOR_SRCSLOT_GPI2 slot */
									
									/** Change point starts */
									// TESTONLY_PRINT_INTERMEDIATE("Generate Vector Dual-Issue Instructions", "vecdual_after_replacement_00", IMG_TRUE);
									if(!VecDualSubstituteIRegBetweenInterval(psState, psSetup, psArgStartInst, psArgEndInst, 
											uAvailableIReg, &uRegIRegSubstituedWith, uRegIRegSelected))
									{
										RETURN_WITH_HINT(IMG_FALSE);
									}
									// TESTONLY_PRINT_INTERMEDIATE("Generate Vector Dual-Issue Instructions", "vecdual_after_replacement_01", IMG_TRUE);
									/** Change point ends */
									if(psVecArg->uNumber != uGPI2SlotReserved)
									{
										/* 
											VecDualSubstituteIRegBetweenInterval might have made few changes in ICode by now 
											but those aren't in favour for dual
										*/
										RETURN_WITH_HINT(IMG_FALSE);
									}
								}
							}
							else
							{
								psStartInst = (psArgStartInst->uBlockIndex > psSetup->psSiteInst->uBlockIndex) ? psSetup->psSiteInst : psArgStartInst;
								psEndInst = (psArgEndInst->uBlockIndex < psSetup->psSiteInst->uBlockIndex) ? psSetup->psSiteInst : psArgEndInst;
							
								uAvailableIReg = VecDualGetAvailableInternalRegsBetweenInterval(psState, 
																								psSetup,
																								(psOldDestStartInst)? psOldDestStartInst : psStartInst, 
																								psEndInst);
								
								/* Remove already engaged IRegs */
								uAvailableIReg &= ~uIRegEngaged;
								uAvailableIReg &= ~uSiteEngagedIRegs;
								
								if(eSlotOption == DUALISSUEVECTOR_SRCSLOT_GPI2)
								{
									if((~uAvailableIReg & (uVecMask << uGPI2SlotReserved)) == 0)
									{
										uRegIRegSelected = uGPI2SlotReserved;
									}
									else
									{
										/* TODO: Try free i2 for GPI2 */
										RETURN_WITH_HINT(IMG_FALSE);
									}
								}
								else
								{
									uAvailableIReg &= ~(uVecMask << uGPI2SlotReserved);
								
									/* Select a vector out of it */
									if(!uAvailableIReg || !SelectInternalReg(uAvailableIReg, uVecMask, &uRegIRegSelected))
									{
										RETURN_WITH_HINT(IMG_FALSE);
									}
								}
								uIRegEngaged |= (uVecMask << uRegIRegSelected);
								asDIMap[uMapIdx].puIRegEngaged[uSrcIdx] = uRegIRegSelected;
							}
							
							if(bSucceedDestNeedsReplacement)
							{
								if(CompareArgs(&sSucceedDestination, &psInst->asArg[uArgIdx]) == 0)
								{
									bSucceedDestNeedsReplacement = IMG_FALSE;
								}
							}
						}
						break;
					case DUALISSUEVECTOR_SRCSLOT_UNDEF:
						imgabort();
						break;
				}
			}
		}
		
		if(bSucceedDestNeedsReplacement)
		{
			RETURN_WITH_HINT(IMG_FALSE);
		}
		
		/* Format consistency check */
		if(psSetup->bF16)
		{
			IMG_BOOL				bNeedF16 = IMG_FALSE;
			
			for (uMapIdx = 0; uMapIdx < (sizeof(asDIMap) / sizeof(asDIMap[0])); uMapIdx++)
			{
				if(asDIMap[uMapIdx].psInstData->eUSFormat != DUALISSUE_UNIFIEDSTORE_FORMAT_F16
				  || asDIMap[uMapIdx].psInst != psInstUSSrc)
				{
					continue;
				}
				
				if(psInstUSSrc->asDest[0].eFmt == UF_REGFORMAT_F32)
				{
					/* Instruction is converted to f32 processing */
					asDIMap[uMapIdx].psInstData->eUSFormat = DUALISSUE_UNIFIEDSTORE_FORMAT_F32;
				}
			}
			
			for (uMapIdx = 0; uMapIdx < (sizeof(asDIMap) / sizeof(asDIMap[0])); uMapIdx++)
			{
				if(asDIMap[uMapIdx].psInstData->eUSFormat == DUALISSUE_UNIFIEDSTORE_FORMAT_F16)
				{
					bNeedF16 = IMG_TRUE;
				}
			}
			psSetup->bF16 = bNeedF16;
		}
		
		ASSERT(!(uIRegEngaged & uSiteEngagedIRegs));

		/*
			Check if the combined instruction would have a non-GPI, F32 destination and a
			non-GPI F16 source (or vice-versa).
		*/
		if (psSetup->ePrimDestSlotConfiguration == VDUAL_DESTSLOT_UNIFIEDSTORE)
		{
			psUSDest = &psPriInst->asDest[0];
		}
		else
		{
			ASSERT(psSetup->ePrimDestSlotConfiguration == VDUAL_DESTSLOT_GPI);
			psUSDest = &psSecInst->asDest[0];
		}
		eUSDestFormat = GetUnifiedStoreFormatForArg(psState, psUSDest);
		if (
				eUSSrcFormat != DUALISSUE_UNIFIEDSTORE_FORMAT_ANY &&
				eUSDestFormat != DUALISSUE_UNIFIEDSTORE_FORMAT_ANY &&
				eUSSrcFormat != eUSDestFormat
			)
		{
			RETURN_WITH_HINT(IMG_FALSE);
		}
		
		switch(psSetup->ePrimDestSlotConfiguration)
		{
			case VDUAL_DESTSLOT_UNIFIEDSTORE:
				if(psPriData->eUSFormat == DUALISSUE_UNIFIEDSTORE_FORMAT_F16)
				{
					ASSERT(psSetup->bF16);
				}
				else
				if(psPriData->uDestWriteMask & (USC_Z_CHAN_MASK | USC_W_CHAN_MASK))
				{
					/*
						IReg replacement might have override these constraints
					*/
					RETURN_WITH_HINT(IMG_FALSE);
				}
				if(psSecData->bRequiresUSDestination)
				{
					RETURN_WITH_HINT(IMG_FALSE);
				}
				break;
			case VDUAL_DESTSLOT_GPI:
				if(psSecData->eUSFormat == DUALISSUE_UNIFIEDSTORE_FORMAT_F16)
				{
					ASSERT(psSetup->bF16);
				}
				else
				{
					if(psSecData->uDestWriteMask & (USC_Z_CHAN_MASK | USC_W_CHAN_MASK))
					{
						/*
							IReg replacement might have override these constraints
						*/
						RETURN_WITH_HINT(IMG_FALSE);
					}
				}
				if(psPriData->bRequiresUSDestination)
				{
					IMG_UINT32 uAvailableIReg;
					PINST psEndInst, psStartInst;
					
					GetLivenessSpanForDestDAG(psState, psPriInst, 0, &psStartInst, &psEndInst);
					if(psStartInst && psStartInst->psBlock != psEndInst->psBlock)
					{
						/* Destination 0 is initialized outside this block */
						RETURN_WITH_HINT(IMG_FALSE);
					}
					if(psEndInst)
					{
						uAvailableIReg = GetAvailableInternalRegsBetweenInterval(psState, psStartInst ? psStartInst : psSetup->psSiteInst, 
																						psEndInst);
						/* Remove already engaged IRegs */
						uAvailableIReg &= ~uIRegEngaged;

						if(uAvailableIReg)
						{
							IMG_UINT32 uIRegSelected;
							for(uIRegSelected = 0; uIRegSelected < 32; uIRegSelected+=1)
							{
								if(uAvailableIReg & (1 << uIRegSelected))
								{
									break;
								}
							}
							if(uIRegSelected < psState->uGPISizeInScalarRegs)
							{
								ARG sIReg = {0};
							
								/* Prepare reference IReg */
								sIReg.uType = USEASM_REGTYPE_FPINTERNAL;
								sIReg.uNumber = uIRegSelected;
								/* Always force f32 precision for IReg */
								sIReg.eFmt = UF_REGFORMAT_F32;
								
								if(!ApplyConstraintDestinationReplacementDAG(psState, psPriInst, 0, 
													VecDualSrcReplacementConstraints, VecDualDestReplacementConstraints,
													VecDualUpdateSrcReplacement, VecDualUpdateDestReplacement,
													VecDualUpdateInstructionAfterSubstitution,
													IMG_TRUE, &sIReg, (IMG_PVOID)psSetup))
								{
									RETURN_WITH_HINT(IMG_FALSE);
								}
								/** ICode Change point starts */
								ApplyConstraintDestinationReplacementDAG(psState, psPriInst, 0, 
										IMG_NULL, IMG_NULL, 
										VecDualUpdateSrcReplacement, VecDualUpdateDestReplacement,
										VecDualUpdateInstructionAfterSubstitution,
										IMG_FALSE, &sIReg, (IMG_PVOID)psSetup);
								/** ICode Change point ends */
							}
							else
							{
								RETURN_WITH_HINT(IMG_FALSE);
							}
						}
						else
						{
							RETURN_WITH_HINT(IMG_FALSE);
						}
					}
					else
					{
						RETURN_WITH_HINT(IMG_FALSE);
					}
				}
				break;
			default:
				break;
		}
		
		/** ICode Change point starts. Commit all changes from here, Commit the IReg replacement */
		for (uMapIdx = 0; uMapIdx < (sizeof(asDIMap) / sizeof(asDIMap[0])); uMapIdx++)
		{
			for (uSrcIdx = 0; uSrcIdx < asDIMap[uMapIdx].ui32SrcCount; uSrcIdx++)
			{
				IMG_UINT32 		uSrcCmpnt;
				PINST			psInst = asDIMap[uMapIdx].psInst;
				IMG_UINT32		uVectorLength =  (psSetup->bExpandVectors) ?  VECTOR_LENGTH : 1;
				
				IMG_UINT32		uArgIdx = uSrcIdx * SOURCE_ARGUMENTS_PER_VECTOR + (psSetup->bExpandVectors ? 1 : 0);
				for(uSrcCmpnt = 0; uSrcCmpnt < uVectorLength; uSrcCmpnt++)
				{
					if(psInst->asArg[uArgIdx + uSrcCmpnt].uType != USC_REGTYPE_UNUSEDSOURCE)
					{
						if(asDIMap[uMapIdx].puIRegEngaged[uSrcIdx] < psState->uGPISizeInScalarRegs &&
							psInst->asArg[uArgIdx + uSrcCmpnt].uType != USEASM_REGTYPE_FPINTERNAL)
						{
							ARG sIReg = {0};
							
							/* Prepare reference IReg */
							sIReg.uType = USEASM_REGTYPE_FPINTERNAL;
							sIReg.uNumber = asDIMap[uMapIdx].puIRegEngaged[uSrcIdx] + uSrcCmpnt;
							sIReg.eFmt = UF_REGFORMAT_F32;
							
							uReplacementsDone++;
							/* Substitute argument register by engaged IReg */
							ApplyConstraintArgumentReplacementDAG(psState, psInst, uArgIdx + uSrcCmpnt, 
									IMG_NULL, IMG_NULL, 
									VecDualUpdateSrcReplacement, VecDualUpdateDestReplacement,
									VecDualUpdateInstructionAfterSubstitution,
									IMG_FALSE, &sIReg, (IMG_PVOID)psSetup);
						}
					}
				}
			}
		}
		/** ICode Change point ends */
		
		/*
			Register replacement still can change f16 mode requirement if sources are common across instructions
		*/
		if(psSetup->bF16)
		{
			IMG_UINT32 bNeedF16 = IMG_FALSE;
			for (uMapIdx = 0; uMapIdx < (sizeof(asDIMap) / sizeof(asDIMap[0])); uMapIdx++)
			{
				if(asDIMap[uMapIdx].psInst->asDest[0].eFmt == UF_REGFORMAT_F16)
				{
					bNeedF16 = IMG_TRUE;
				}
				for (uSrcIdx = 0; uSrcIdx < asDIMap[uMapIdx].ui32SrcCount; uSrcIdx++)
				{
					IMG_UINT32		uArgIdx = uSrcIdx * SOURCE_ARGUMENTS_PER_VECTOR + (psSetup->bExpandVectors ? 1 : 0);
					if(asDIMap[uMapIdx].psInst->asArg[uArgIdx].eFmt == UF_REGFORMAT_F16)
					{
						bNeedF16 = IMG_TRUE;
					}
				}
			}
			psSetup->bF16 = bNeedF16;
		}
	}
	VDUAL_TESTONLY_PRINT_INTERMEDIATE("Generate Vector Dual-Issue Instructions", "vecdual_after_replacement", IMG_TRUE);
	RETURN_WITH_HINT(IMG_TRUE);
}

static
IMG_BOOL CanDualIssueSiteConfig(PINTERMEDIATE_STATE	psState,
								PDUALISSUE_INST		psInst1,
								PDUALISSUE_INST		psInst2,
								PDUALISSUE_SETUP	psSetup,
								IMG_BOOL			bEnableSourceSubstitution,
								IMG_BOOL			bTrySwappingPair,
								IMG_UINT32			*puExitHint)
/*****************************************************************************
 FUNCTION	: CanDualIssueSiteConfig
    
 PURPOSE	: Checks if two instructions could be dual-issued together.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to check.
			  psInst2
			  psSetup			- Returns information about the parameters
								for a dual-issue instruction combining
								both of the instructions.
			  bEnableSourceSubstitution
			  					- Try combining instructions by source 
			  					replacement
			  bTrySwappingPair	- Try pair configuration of swaping 
			 					instructions for primary and secondary
			  
 RETURNS	: TRUE if the two instructions can be dual-issued.
*****************************************************************************/
{
	struct _DI_POSSIBLE_PAIRS_
	{
		IMG_BOOL		bPairPossible;
		IMG_BOOL		bVec3;
		PDUALISSUE_INST	apsInst[2];
		IMG_BOOL		bInst1IsPrimary;
		VDUAL_DESTSLOT	ePrimDestSlotConfiguration;
	} asDIPossiblePairs[4];
	
	IMG_UINT32				uPossibleSrcCfgs, uPairConfig;
	PDUALISSUE_INST			psPriInst;
	PDUALISSUE_INST			psSecInst;
	IMG_UINT32				uSrcCfgIdx;
	VDUAL_OP				eOp1;
	VDUAL_OP				eOp2;
	IMG_BOOL				bVec3, bVec4;
	DUALISSUE_UNIFIEDSTORE_FORMAT	eUSFormat;
	IMG_BOOL				bMayNeedInternalRegAssingment = IMG_FALSE;
	IMG_BOOL				bGotPair12 = IMG_FALSE, bGotPair21 = IMG_FALSE;
	
	/* Quick exits */
	if(psInst1->bMayNeedInternalRegAssingment || psInst2->bMayNeedInternalRegAssingment)
	{
		if(!bEnableSourceSubstitution)
		{
			RETURN_WITH_HINT(IMG_FALSE);
		}
		bMayNeedInternalRegAssingment = IMG_TRUE;
	}
	
	/*
		Check if both operations need to use the unified store destination.
	*/
	if (!bMayNeedInternalRegAssingment)
	{
		if(psInst1->bRequiresUSDestination && psInst2->bRequiresUSDestination)
		{
			RETURN_WITH_HINT(IMG_FALSE);
		}
		
		/*
			Check if both operations need to use the GPI/US destination.
		*/
		if (psInst1->bRequiresGPIDestination && psInst2->bRequiresGPIDestination)
		{
			RETURN_WITH_HINT(IMG_FALSE);
		}
		
		if(psInst1->bRequiresGPIDestination && (!psInst2->bScalarAvailable))
		{
			RETURN_WITH_HINT(IMG_FALSE);
		}
		
		if(psInst2->bRequiresGPIDestination && (!psInst1->bScalarAvailable))
		{
			RETURN_WITH_HINT(IMG_FALSE);
		}
	}
	
	/*
		Check if the two operations have compatible vector lengths.
	*/
	bVec3 = IMG_FALSE;
	bVec4 = IMG_FALSE;
	if ((psInst1->bScalarAvailable || psInst1->bVec4Available) && (psInst2->bScalarAvailable || psInst2->bVec4Available))
	{
		bVec4 = IMG_TRUE;
	}
	
	if ((psInst1->bScalarAvailable || psInst1->bVec3Available) && (psInst2->bScalarAvailable || psInst2->bVec3Available))
	{
		bVec3 = IMG_TRUE;
	}
	
	if (psInst1->bVec4Available && psInst2->bVec4Available)
	{
		bVec4 = IMG_TRUE;
	}
	
	if(!bVec3 && !bVec4)
	{
		RETURN_WITH_HINT(IMG_FALSE);
	}

	/*
		Get the final operation types now we know the vector length of the dual-issue
		instruction.
	*/
	psInst1->eSetupOp = eOp1 = GetDualIssueOp(psInst1, bVec3);
	psInst2->eSetupOp = eOp2 = GetDualIssueOp(psInst2, bVec3);
	
	ASSERT(psInst1->eSetupOp < VDUAL_OP_UNDEF);
	ASSERT(psInst2->eSetupOp < VDUAL_OP_UNDEF);

	/* Possible configurations */
	if(psInst1->bCanBePrimary && psInst2->bCanBeSecondary && g_aabValidCoissue[eOp1][eOp2])
	{
		asDIPossiblePairs[0].bPairPossible = IMG_TRUE;
		asDIPossiblePairs[0].bVec3 = bVec3;
		asDIPossiblePairs[0].apsInst[0] = psInst1;
		asDIPossiblePairs[0].apsInst[1] = psInst2;
		asDIPossiblePairs[0].bInst1IsPrimary = IMG_TRUE;
		
		if(!bMayNeedInternalRegAssingment)
		{
			if (psInst1->bRequiresUSDestination || psInst2->bRequiresGPIDestination)
			{
				asDIPossiblePairs[0].ePrimDestSlotConfiguration = VDUAL_DESTSLOT_UNIFIEDSTORE;
			}
			else
			{
				asDIPossiblePairs[0].ePrimDestSlotConfiguration = VDUAL_DESTSLOT_GPI;
			}
		}
		else
		{
			asDIPossiblePairs[0].ePrimDestSlotConfiguration = VDUAL_DESTSLOT_COUNT;
		}
		
		uPossibleSrcCfgs = psInst1->uPossiblePrimarySrcCfgs;
		uPossibleSrcCfgs &= psInst2->auPossibleSecondarySrcCfgs[psInst1->uSrcCount - 1];
		if (uPossibleSrcCfgs)
		{
			bGotPair12 = IMG_TRUE;
		}
	}
	else
	{
		asDIPossiblePairs[0].bPairPossible = IMG_FALSE;
	}
	
	if(psInst2->bCanBePrimary && psInst1->bCanBeSecondary && g_aabValidCoissue[eOp2][eOp1])
	{
		asDIPossiblePairs[1].bPairPossible = IMG_TRUE;
		asDIPossiblePairs[1].bVec3 = bVec3;
		asDIPossiblePairs[1].apsInst[0] = psInst2;
		asDIPossiblePairs[1].apsInst[1] = psInst1;
		asDIPossiblePairs[1].bInst1IsPrimary = IMG_FALSE;
		
		if(!bMayNeedInternalRegAssingment)
		{
			if (psInst1->bRequiresUSDestination || psInst2->bRequiresGPIDestination)
			{
				asDIPossiblePairs[1].ePrimDestSlotConfiguration = VDUAL_DESTSLOT_GPI;
			}
			else
			{
				asDIPossiblePairs[1].ePrimDestSlotConfiguration = VDUAL_DESTSLOT_UNIFIEDSTORE;
			}
		}
		else
		{
			asDIPossiblePairs[1].ePrimDestSlotConfiguration = VDUAL_DESTSLOT_COUNT;
		}
		
		uPossibleSrcCfgs = psInst2->uPossiblePrimarySrcCfgs;
		uPossibleSrcCfgs &= psInst1->auPossibleSecondarySrcCfgs[psInst2->uSrcCount - 1];
		if (uPossibleSrcCfgs)
		{
			bGotPair21 = IMG_TRUE;
		}
	}
	else
	{
		asDIPossiblePairs[1].bPairPossible = IMG_FALSE;
	}
	/*
		Check if the two operations can be part of the same dual-issue.
	*/
	if (!(bGotPair12 || bGotPair21))
	{
		RETURN_WITH_HINT(IMG_FALSE);
	}
	
	if(bVec3 && bVec4 && bMayNeedInternalRegAssingment)
	{
		/* bVec3 possible and so as bVec4 */
		asDIPossiblePairs[2] = asDIPossiblePairs[0];
		asDIPossiblePairs[2].bVec3 = IMG_FALSE;
		
		asDIPossiblePairs[3] = asDIPossiblePairs[1];
		asDIPossiblePairs[3].bVec3 = IMG_FALSE;
	}
	else
	{
		/* bVec4 is already tried with asDIPossiblePairs[0] & asDIPossiblePairs[1] */
		asDIPossiblePairs[2].bPairPossible = IMG_FALSE;
		asDIPossiblePairs[3].bPairPossible = IMG_FALSE;
	}
	
	/*
		Check if the overall dual-issue instruction should interpret unified store
		sources and destinations as F16.
	*/
	eUSFormat = psInst1->eUSFormat;
	if (psInst2->eUSFormat != DUALISSUE_UNIFIEDSTORE_FORMAT_ANY)
	{
		if (eUSFormat != DUALISSUE_UNIFIEDSTORE_FORMAT_ANY && eUSFormat != psInst2->eUSFormat)
		{
			RETURN_WITH_HINT(IMG_FALSE);
		}
		eUSFormat = psInst2->eUSFormat;
	}
	if (eUSFormat == DUALISSUE_UNIFIEDSTORE_FORMAT_F16)
	{
		psSetup->bF16 = IMG_TRUE;
	}
	else
	{
		ASSERT(eUSFormat == DUALISSUE_UNIFIEDSTORE_FORMAT_F32 || eUSFormat == DUALISSUE_UNIFIEDSTORE_FORMAT_ANY);
		psSetup->bF16 = IMG_FALSE;
	}

	uPairConfig = 0;
	do
	{
		if(!asDIPossiblePairs[uPairConfig].bPairPossible)
		{
			continue;
		}
		
		psSetup->bVec3 = asDIPossiblePairs[uPairConfig].bVec3;
		
		psPriInst = asDIPossiblePairs[uPairConfig].apsInst[0];
		psSecInst = asDIPossiblePairs[uPairConfig].apsInst[1];
		
		psSetup->ePrimDestSlotConfiguration = asDIPossiblePairs[uPairConfig].ePrimDestSlotConfiguration;
		psSetup->bInst1IsPrimary = asDIPossiblePairs[uPairConfig].bInst1IsPrimary;
		psSetup->bPrecedInstIsPrimary = (psPriInst->psInst == psSetup->psPrecedInst)? IMG_TRUE : IMG_FALSE;
		
		/*
			Check if there is a source configuration which is compatible with both instructions.
		*/
		uPossibleSrcCfgs = psPriInst->uPossiblePrimarySrcCfgs;
		uPossibleSrcCfgs &= psSecInst->auPossibleSecondarySrcCfgs[psPriInst->uSrcCount - 1];
		if (uPossibleSrcCfgs == 0)
		{
			RETURN_WITH_HINT(IMG_FALSE);
		}
	
		if(!bEnableSourceSubstitution)
		{
			/*
				Select a source configuration to use for this instruction from the possible
				set.
			*/
			for (uSrcCfgIdx = 0; uSrcCfgIdx < SGXVEC_USE0_DVEC_SRCCFG_COUNT; uSrcCfgIdx++)
			{
				if (uPossibleSrcCfgs & (1 << uSrcCfgIdx))
				{
					psSetup->uSrcCfg = uSrcCfgIdx;
					RETURN_WITH_HINT(IMG_TRUE);
				}
			}
		}
		else
		{
			/* For each possible configuration */
			for (uSrcCfgIdx = 0; uSrcCfgIdx < SGXVEC_USE0_DVEC_SRCCFG_COUNT; uSrcCfgIdx++)
			{
				if (uPossibleSrcCfgs & (1 << uSrcCfgIdx))
				{
					IMG_UINT32	uExitHint;

					psSetup->uSrcCfg = uSrcCfgIdx;
							
					if(VecDualPreparePairForIRegReplacement(psState,
															psPriInst->psInst, psSecInst->psInst, 
															psPriInst, psSecInst,
															psSetup,
															&uExitHint))
					{
#if defined(DEBUG_PAIRING)
						fprintf(stderr, "VDUAL: PASSED - psInst1: %d psInst2: %d\n", psInst1->psInst->uBlockIndex, psInst2->psInst->uBlockIndex);
#endif
						RETURN_WITH_HINT(IMG_TRUE);
					}
					else
					{
#if defined(DEBUG_PAIRING)
						fprintf(stderr, "VDUAL: FAILED - psInst1: %d psInst2: %d REASON: %d\n", psInst1->psInst->uBlockIndex, psInst2->psInst->uBlockIndex, uExitHint);
#endif
						/* Recording how deep preparing pair went */
						psSetup->uExitHintMax = (psSetup->uExitHintMax < uExitHint)? uExitHint : psSetup->uExitHintMax;
					}
				}
			}
		}
	}while(++uPairConfig < (sizeof(asDIPossiblePairs)/sizeof(asDIPossiblePairs[0])));
	
	if(!bTrySwappingPair)
	{
		imgabort();
	}
	RETURN_WITH_HINT(IMG_FALSE);
}

static
IMG_BOOL CanDualIssue(PINTERMEDIATE_STATE	psState,
					  PDUALISSUE_INST		psInst1,
					  PDUALISSUE_INST		psInst2,
					  PDUALISSUE_SETUP		psSetup,
					  IMG_BOOL				bEnableSourceSubstitution,
					  IMG_BOOL				bTrySwappingPair,
					  IMG_UINT32			*puExitHint)
/*****************************************************************************
 FUNCTION	: CanDualIssue
    
 PURPOSE	: Checks if two instructions could be dual-issued together.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to check.
			  psInst2
			  psSetup			- Returns information about the parameters
								for a dual-issue instruction combining
								both of the instructions.
			  bEnableSourceSubstitution
			  					- Try combining instructions by source 
			  					replacement
			  bTrySwappingPair	- Try pair configuration of swaping 
			 					instructions for primary and secondary
			  
 RETURNS	: TRUE if the two instructions can be dual-issued.
*****************************************************************************/
{
	/* Setup parameters */
	psSetup->psSiteInst = IMG_NULL;
	psSetup->psNonSiteInst = IMG_NULL;
	psSetup->bSucceedInstDefiningIntervalRef = IMG_FALSE;
	psSetup->bSucceedInstDefiningIntervalDef = IMG_FALSE;
	psSetup->bExpandVectors = ((psState->uFlags2 & USC_FLAGS2_NO_VECTOR_REGS) ? IMG_TRUE : IMG_FALSE);
	psSetup->bCheckUseDefConstraints = bEnableSourceSubstitution;
	psSetup->bPrecedInstMovableTillSucced = IMG_FALSE;
	psSetup->uExitHintMax = 0;

	if(psSetup->bCheckUseDefConstraints)
	{
		psSetup->psPrecedInst = (psInst1->psInst->uBlockIndex < psInst2->psInst->uBlockIndex) ? 
									psInst1->psInst : psInst2->psInst;
		psSetup->psSuccedInst = (psInst1->psInst->uBlockIndex < psInst2->psInst->uBlockIndex) ?
									psInst2->psInst : psInst1->psInst;
		
		/* The second instruction is directly dependent on the first */
		if (IsImmediateSubordinate(psSetup->psPrecedInst, psSetup->psSuccedInst))
		{
			RETURN_WITH_HINT(IMG_FALSE);
		}

		if(!VecDualValidatePairForIRegReplacement(psState, psInst1, psInst2))
		{
			RETURN_WITH_HINT(IMG_FALSE);
		}
		
		if(!VecDualComputeIntermediateSiteInfo(psState, psSetup, psSetup->psPrecedInst, psSetup->psSuccedInst))
		{
			RETURN_WITH_HINT(IMG_FALSE);
		}
	}

	if (!bEnableSourceSubstitution)
	{
		return CanDualIssueSiteConfig(psState,
									  psInst1,
									  psInst2,
									  psSetup,
									  bEnableSourceSubstitution,
									  bTrySwappingPair,
									  puExitHint);
	}
	else
	{
		IMG_UINT32	uSiteConfig;
		
		/* Indicate flush all cached inverval liveness info of IReg */
		psSetup->bInstIntervalValidate = IMG_FALSE;
		
		/* Try each possible site configuration */
		for(uSiteConfig = 0; uSiteConfig < DUALISSUE_SITE_CONFIGURATION_COUNT; uSiteConfig++)
		{
			psSetup->uSiteConfig = uSiteConfig;
			if(psSetup->apsSiteInst[uSiteConfig])
			{
				IMG_BOOL	bIterate;
				IMG_BOOL	bNeedRestore;
				
#if defined(DEBUG_SLIDING)
				PINST		*ppsInstSequence;
					
				ppsInstSequence = SaveInstructionSequence(psState, psPriInst->psInst->psBlock);
#endif
				/* Resuffle instructions for site configuration */
				bNeedRestore = VecDualRepositionInstructionForSiteConfiguration(psState, uSiteConfig, psSetup, &bIterate);
				if(!bIterate)
				{
#if defined(DEBUG_SLIDING)
					UscFree(psState, ppsInstSequence);
#endif
					continue;
				}
				if(bNeedRestore)
				{
					TESTONLY(VerifyDAG(psState, psInst1->psInst->psBlock));
					VecDualPrintSliddingInfo(psState, psInst1->psInst->psBlock);
					VDUAL_TESTONLY_PRINT_INTERMEDIATE("Generate Vector Dual-Issue Instructions", "vecdual_instruciton_resuffle", IMG_TRUE);
					
					/* Indicate flush all cached inverval liveness info of IReg */
					psSetup->bInstIntervalValidate = IMG_TRUE;
				}

				/*
					After re-positioning the instruction may no longer be dual-issue candidates.
				*/
				if (psInst1->psInst->sStageData.psVDIData->bIsPossibleDualOp &&
					psInst2->psInst->sStageData.psVDIData->bIsPossibleDualOp)
				{
					if (CanDualIssueSiteConfig(psState,
											   psInst1,
											   psInst2,
											   psSetup,
											   bEnableSourceSubstitution,
											   bTrySwappingPair,
											   puExitHint))
					{
						#if defined(DEBUG_SLIDING)
						UscFree(psState, ppsInstSequence);
						DBG_PRINTF((DBG_WARNING, "VDUAL: PINST1: %d PINST2: %d SITE:%d SITE_CONFIG:%d - %s", 
									psInst1->psInst->uBlockIndex,
									psInst2->psInst->uBlockIndex,
									psSetup->psSiteInst->uBlockIndex, uSiteConfig, g_pszFileName));
						#endif

						/*
							Empty the stack of instruction modifications. These have now been committed.
						*/
						while (!UscStackEmpty(psSetup->psMoveUndo))
						{
							UscStackPop(psState, psSetup->psMoveUndo);
						}

						return IMG_TRUE;
					}
				}
				
				if(bNeedRestore)
				{
					/* Restore instruction positions */
					VecDualRestoreInstructionPositions(psState, psSetup, psSetup->apsSiteInst[uSiteConfig]->psBlock);
					TESTONLY(VerifyDAG(psState, psInst1->psInst->psBlock));
					VecDualPrintSliddingInfo(psState, psInst1->psInst->psBlock);
					VDUAL_TESTONLY_PRINT_INTERMEDIATE("Generate Vector Dual-Issue Instructions", "vecdual_instruciton_resuffle_restore", IMG_TRUE);
				}
#if defined(DEBUG_SLIDING)
				MatchInstructionSequence(psState, psPriInst->psInst->psBlock, ppsInstSequence);
#endif
			}
		}
	}
	return IMG_FALSE;
}

static
IMG_BOOL IsSumOfSquares(PINTERMEDIATE_STATE		psState,
						PINST					psInst)
/*****************************************************************************
 FUNCTION	: IsSumOfSquares
    
 PURPOSE	: Check for a dotproduct instruction which could be implemented using a
			  dual-issue SSQ instruction.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Dotproduct instruction.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PARG					psArg1;
	PARG					psArg2;
	IMG_UINT32				uSwizzleCompareMask;
	PVEC_SOURCE_MODIFIER	psMod1;
	PVEC_SOURCE_MODIFIER	psMod2;
	IMG_UINT32				uVectorLength;

	if (psInst->eOpcode == IVDP)
	{
		uVectorLength = 4;
	}
	else
	{
		ASSERT(psInst->eOpcode == IVDP3);
		uVectorLength = 3;
	}

	psArg1 = &psInst->asArg[0 * SOURCE_ARGUMENTS_PER_VECTOR];
	psArg2 = &psInst->asArg[1 * SOURCE_ARGUMENTS_PER_VECTOR];

	/*
		Ignore arguments which are vectors of individual scalar registers.
	*/
	if (psArg1->uType == USC_REGTYPE_UNUSEDSOURCE || psArg2->uType == USC_REGTYPE_UNUSEDSOURCE)
	{
		return IMG_FALSE;
	}

	/*
		Check the source register are the same.
	*/
	if (!EqualArgs(psArg1, psArg2))
	{
		return IMG_FALSE;
	}

	/*
		Check the same swizzle is applied to both sources.
	*/
	uSwizzleCompareMask = (1 << uVectorLength) - 1;
	if (!CompareSwizzles(psInst->u.psVec->auSwizzle[0], psInst->u.psVec->auSwizzle[1], uSwizzleCompareMask))
	{
		return IMG_FALSE;
	}

	/*
		Check both sources have the same format.
	*/
	if (psInst->u.psVec->aeSrcFmt[0] != psInst->u.psVec->aeSrcFmt[1])
	{
		return IMG_FALSE;
	}

	/*
		Check the same modifiers are applied to both sources.
	*/
	psMod1 = &psInst->u.psVec->asSrcMod[0];
	psMod2 = &psInst->u.psVec->asSrcMod[1];
	if (psMod1->bNegate != psMod2->bNegate || psMod1->bAbs != psMod2->bAbs)
	{
		return IMG_FALSE;
	}
	
	return IMG_TRUE;
}

static
IMG_BOOL IsPossibleDualIssueOp(PINTERMEDIATE_STATE			psState,
							   PINST						psInst,
							   IMG_UINT32					uLiveChansInInstDest,
							   IMG_UINT32					uRequireUnifiedStoreSrcMask,
							   IMG_BOOL						bRequireNotUnifiedStoreSlot,
							   IMG_BOOL						bSetHintInternalRegReplacement,
							   PIREG_ASSIGNMENT				psIRegAssignment,
							   PDUALISSUE_INST				psInstData)
/*****************************************************************************
 FUNCTION	: IsPossibleDualIssueOp
    
 PURPOSE	: Check if this instruction could be part of a dual-issue.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  uLiveChansInInstDest
								- If the mask of channels used later from
								the instruction's destination.
			  uRequireUnifiedStoreSrcMask
								- Mask of sources which should use the dual-issue
								instruction's unified store slot.
			  bRequireNotUnifiedStoreSlot
								- If TRUE require the instruction not to use the
								unified store slot.
			  psIRegAssignment	- If internal registers have been assigned then
								information about which destination or source arguments
								are internal registers.
			 bSetHintInternalRegReplacement
								- Set a hit for dual generation state to consider 
								possiblility of forming dual instructions by 
								source/destination with internal registers 
								replacement.
			  psInstData		- Written with information about the restrictions
								on what other instructions this instruction can
								be dual-issued with.

 RETURNS	: TRUE if this instruction could be part of a dual-issue.
*****************************************************************************/
{
	UF_REGFORMAT		aeSlotFormat[VECTOR_MAX_SOURCE_SLOT_COUNT];
	IMG_UINT32			auSupportedSlotsIfOp1HasOneSrc[VECTOR_MAX_SOURCE_SLOT_COUNT];
	IMG_UINT32			auSupportedSlotsIfOp1HasTwoOrThreeSrcs[VECTOR_MAX_SOURCE_SLOT_COUNT];
	IMG_UINT32			uSrcIdx;
	IMG_PUINT32			auPrimarySupportedSlots;
	IMG_UINT32			uOverallPossibleSecondarySrcCfgs;
	IMG_UINT32			uOp1SrcCount;
	IMG_UINT32			uSrcCfg;
	IMG_BOOL			bVectorResult;
	IMG_BOOL			bSSQ = IMG_FALSE;
	IMG_UINT32			uPreservedChanMask;
	UF_REGFORMAT		eDestFormat;
	IMG_BOOL			bIRegSubstitutionPossible = IMG_TRUE;
	IMG_BOOL			bVectorSizedArgs = (psState->uFlags2 & USC_FLAGS2_NO_VECTOR_REGS)? IMG_TRUE : IMG_FALSE;

	psInstData->psInst = psInst;
	psInstData->bScalarAvailable = IMG_TRUE;
	psInstData->bVec3Available = IMG_TRUE;
	psInstData->bVec4Available = IMG_TRUE;
	psInstData->bMayNeedInternalRegAssingment = IMG_FALSE;
	psInstData->eSetupOp = VDUAL_OP_UNDEF;

	if (psInst->uDestCount == 0 ||
		   (bVectorSizedArgs && psInst->uDestCount > VECTOR_LENGTH))
	{
		return IMG_FALSE;
	}

	if (psInst->eOpcode == IVDP || psInst->eOpcode == IVDP3)
	{
		/*
			Check for a dotproduct which could be simplified to sum-of-squares.
		*/
		bSSQ = IsSumOfSquares(psState, psInst);

		/*
			The source data can only come from the unified store if the instruction
			has only one argument.
		*/
		if ((uRequireUnifiedStoreSrcMask & VECTOR_SLOT01_MASK) == VECTOR_SLOT01_MASK)
		{
			if (!bSSQ)
			{
				return IMG_FALSE;
			}
			uRequireUnifiedStoreSrcMask = VECTOR_SLOT0_MASK;
		}

		psInstData->bScalarAvailable = IMG_FALSE;
	}
	
	if(bVectorSizedArgs)
	{
		IMG_UINT32 uInx;
		
		psInstData->uDestWriteMask = 0;
		for(uInx = 0; uInx < psInst->uDestCount; uInx++)
		{
			psInstData->uDestWriteMask |= (psInst->auDestMask[uInx]) ? (1 << uInx) : 0;
		}
	}
	else
	{
		psInstData->uDestWriteMask = psInst->auDestMask[0];
	}
	
	if ((psInstData->uDestWriteMask & USC_W_CHAN_MASK) != 0)
	{
		psInstData->bScalarAvailable = IMG_FALSE;
		psInstData->bVec3Available = IMG_FALSE;
	}
	if ((psInstData->uDestWriteMask & (USC_Y_CHAN_MASK | USC_Z_CHAN_MASK)) != 0)
	{
		psInstData->bScalarAvailable = IMG_FALSE;
	}
	
	/*
		Ignore the partially writing instructions
	*/
	if(psInst->apsOldDest && psInst->apsOldDest[0])
	{
		return IMG_FALSE;
	}

	/*
		Check if this operation can be executed as part of a dual-issue
		instruction.
	*/
	switch (psInst->eOpcode)
	{
		case IVMAD4:
		case IVMAD:
		{
			psInstData->eOpType = DUALISSUE_OP_TYPE_MAD;	
			bVectorResult = IMG_TRUE;		
			break;
		}
		case IVDP:
		case IVDP_GPI:
			psInstData->bVec3Available = IMG_FALSE;
			/* intensional fall through */
		case IVDP3:
		case IVDP3_GPI:
		{
			psInstData->bScalarAvailable = IMG_FALSE;
			if (psInst->eOpcode == IVDP3)
			{
				psInstData->bVec4Available = IMG_FALSE;
			}
			if (bSSQ)
			{
				psInstData->eOpType = DUALISSUE_OP_TYPE_SSQ;
			}
			else
			{
				psInstData->eOpType = DUALISSUE_OP_TYPE_DP;		
			}
			bVectorResult = IMG_FALSE;  
			break;
		}
		case IVMUL:
		case IVMUL3:
		{
			psInstData->eOpType = DUALISSUE_OP_TYPE_MUL;	
			bVectorResult = IMG_TRUE; 
			break;
		}
		case IVADD:
		case IVADD3:
		{
			psInstData->eOpType = DUALISSUE_OP_TYPE_ADD;	
			bVectorResult = IMG_TRUE;  
			break;
		}
		case IVMOV:
		case IVMOV_EXP:
		case IRESTOREIREG:
		case IVLOAD:
		{
			psInstData->eOpType = DUALISSUE_OP_TYPE_MOV;	
			bVectorResult = IMG_TRUE;  
			break;
		}
		case IVRSQ:	
		{
			psInstData->eOpType = DUALISSUE_OP_TYPE_RSQ;	
			bVectorResult = IMG_FALSE;  
			break;
		}
		case IVRCP:	
		{
			psInstData->eOpType = DUALISSUE_OP_TYPE_RCP;	
			bVectorResult = IMG_FALSE;  
			break;
		}
		case IVLOG:	
		{
			psInstData->eOpType = DUALISSUE_OP_TYPE_LOG;	
			bVectorResult = IMG_FALSE;  
			break;
		}
		case IVEXP:	
		{
			psInstData->eOpType = DUALISSUE_OP_TYPE_EXP;	
			bVectorResult = IMG_FALSE;  
			break;
		}
		case IVFRC:	
		{
			psInstData->eOpType = DUALISSUE_OP_TYPE_FRC;	
			bVectorResult = IMG_FALSE;  
			break;
		}
		default:
		{
			return IMG_FALSE;
		}
	}

	if (psIRegAssignment != NULL && psIRegAssignment->bDestIsIReg)
	{
		eDestFormat = UF_REGFORMAT_F32;
	}
	else
	{
		eDestFormat = psInst->asDest[0].eFmt;
	}

	if (bSSQ || psInst->eOpcode == IRESTOREIREG)
	{
		psInstData->uSrcCount = 1;
	}
	else
	{
		psInstData->uSrcCount = GetSwizzleSlotCount(psState, psInst);
	}
	
	/*
		Assume all sources can be substitued with IReg
	*/
	psInstData->uSrcIRegSubstitutionFlags = (1UL << psInstData->uSrcCount) - 1UL;

	/*
		Check for instructions where the channel used from the source changes when
		made part of a dual-issue.
	*/
	psInstData->bSwizzleWToX = IMG_FALSE;
	if (psInst->eOpcode == IVRCP ||
		psInst->eOpcode == IVRSQ ||
		psInst->eOpcode == IVLOG ||
		psInst->eOpcode == IVEXP)
	{
		psInstData->bSwizzleWToX = IMG_TRUE;
	}

	psInstData->bRequiresUSDestination = IMG_FALSE;
	psInstData->bRequiresGPIDestination = IMG_FALSE;

	/*
		Check restrictions on the destination write mask.
	*/
	uPreservedChanMask = uLiveChansInInstDest & ~psInstData->uDestWriteMask;
	if (bVectorResult)
	{
		if (eDestFormat == UF_REGFORMAT_F16)
		{
			ASSERT(psInst->u.psVec->uDestSelectShift == 0);

			if (uPreservedChanMask != 0)
			{
				return IMG_FALSE;
			}
		}
		else
		{
			IMG_UINT32	uPreservedChansInUSDest;
			IMG_UINT32	uDestSelectShift;

			if (psInst->eOpcode == IRESTOREIREG)
			{
				uDestSelectShift = 0;
			}
			else
			{
				uDestSelectShift = psInst->u.psVec->uDestSelectShift;
			}

			/*
				The unified store destination always writes 64-bits so we can
				mask out half of an F32 vec4.
			*/
			if (uDestSelectShift == 0)
			{
				uPreservedChansInUSDest = USC_ZW_CHAN_MASK;
			}
			else
			{
				ASSERT(uDestSelectShift == USC_Z_CHAN);
				uPreservedChansInUSDest = USC_XY_CHAN_MASK;
			}

			/*
				Only the GPI destination supports a destination write mask.
			*/
			if ((uPreservedChanMask & ~uPreservedChansInUSDest) != 0)
			{
				psInstData->bRequiresGPIDestination = IMG_TRUE;
			}
		}
	}
	else
	{
		if (eDestFormat == UF_REGFORMAT_F16)
		{
			/*
				For non-vector operations two F16 channels at a 32-bit aligned 
				offset are always written.
			*/
			if ((psInstData->uDestWriteMask & USC_XY_CHAN_MASK) != 0)
			{
				/*
					Check we aren't trying to write channels in two different
					32-bit registers.
				*/
				if ((psInstData->uDestWriteMask & USC_ZW_CHAN_MASK) != 0)
				{
					return IMG_FALSE;
				}
				/*
					Check we don't need to preserve any data in the 32-bit
					register we are writing.
				*/
				if ((uPreservedChanMask & USC_XY_CHAN_MASK) != 0)
				{
					return IMG_FALSE;
				}
				
				if ((psInstData->uDestWriteMask & USC_Y_CHAN_MASK) != 0)
				{
					bIRegSubstitutionPossible = IMG_FALSE;
				}
			}
			else
			{
				bIRegSubstitutionPossible = IMG_FALSE;
				if ((uPreservedChanMask & USC_ZW_CHAN_MASK) != 0)
				{
					return IMG_FALSE;
				}
			}
		}
		else
		{
			if (psInstData->uDestWriteMask != USC_X_CHAN_MASK)
			{
				return IMG_FALSE;
			}
			/*
				For non-vector operations only a single F32 channel is written.
			*/
			if (!g_abSingleBitSet[psInstData->uDestWriteMask])
			{
				psInstData->bRequiresGPIDestination = IMG_TRUE;
			}
		}
	}

	/*
		Once internal registers are assigned require the unified store destination for
		any registers we didn't allocate to an internal register.
	*/
	if (psIRegAssignment != NULL && !psIRegAssignment->bDestIsIReg)
	{
		psInstData->bRequiresUSDestination = IMG_TRUE;
	}
	if ((psState->uFlags & USC_FLAGS_POSTC10REGALLOC) != 0 && psInst->asDest[0].uType != USEASM_REGTYPE_FPINTERNAL)
	{
		psInstData->bRequiresUSDestination = IMG_TRUE;
	}

	psInstData->eUSFormat = DUALISSUE_UNIFIEDSTORE_FORMAT_ANY;
	switch (eDestFormat)
	{
		case UF_REGFORMAT_F16:
		{
			if(!bSetHintInternalRegReplacement)
			{
				/*
					GPI registers are always written in F32 format so if this instruction
					produces an F16 result it must use the unified store destination.
				*/
				psInstData->bRequiresUSDestination = IMG_TRUE;
			}
			else
			{
				/* Let's try workout if we could replace f16 sources */
			}
			/*
				This instruction needs the flag set on the instruction to interpret
				unified store sources/destinations as F16.
			*/
			psInstData->eUSFormat = DUALISSUE_UNIFIEDSTORE_FORMAT_F16;
			break;
		}
		case UF_REGFORMAT_F32:
		{
			/*
				The dual-issued instruction must interpret unified store sources/destinations
				as F32.
			*/
			if (psInstData->bRequiresUSDestination)
			{
				psInstData->eUSFormat = DUALISSUE_UNIFIEDSTORE_FORMAT_F32;
			}
			break;
		}
		default:
			return IMG_FALSE;
			break;
	}

	/*
		Check for incompatible restrictions on the destination.
	*/
	if (psInstData->bRequiresGPIDestination && psInstData->bRequiresUSDestination)
	{
		return IMG_FALSE;
	}

	for (uSrcIdx = 0; uSrcIdx < psInstData->uSrcCount; uSrcIdx++)
	{
		IMG_BOOL					bCantUseUSSIfOp1HasTwoOrThreeSrcs;
		IMG_UINT32					uLiveChansInDest;
		IMG_UINT32					uUsedChanMask;
		IMG_UINT32					uSwizzle;
		IMG_UINT32					uSwizzleCompareMask;
		IMG_UINT32					uSupportedSlots;
		VEC_SOURCE_MODIFIER			sSrcMod;
		IMG_BOOL					bAlwaysUSSource;

		ASSERT( (bVectorSizedArgs) || 
				(!bVectorSizedArgs && psInst->uDestCount == 1));
		
		uLiveChansInDest = uLiveChansInInstDest & psInstData->uDestWriteMask;
		
		if (psInst->eOpcode == IRESTOREIREG)
		{
			aeSlotFormat[uSrcIdx] = psInst->asArg[0].eFmt;
			uSwizzle = USEASM_SWIZZLE(X, Y, Z, W);
			sSrcMod.bNegate = IMG_FALSE;
			sSrcMod.bAbs = IMG_FALSE;

			bAlwaysUSSource = IMG_FALSE;

			uUsedChanMask = uLiveChansInDest;
		}
		else
		{
			PARG		psVecArg;
			IMG_UINT32	uArgBase = uSrcIdx * SOURCE_ARGUMENTS_PER_VECTOR;
			
			psVecArg = &psInst->asArg[uArgBase + ((bVectorSizedArgs) ? 1 : 0)];

			ASSERT(g_psInstDesc[psInst->eOpcode].eType == INST_TYPE_VEC);

			if (psIRegAssignment != NULL && psIRegAssignment->auIRegSource[uSrcIdx] != USC_UNDEF)
			{
				aeSlotFormat[uSrcIdx] = UF_REGFORMAT_F32;
			}
			else
			{
				aeSlotFormat[uSrcIdx] = psInst->u.psVec->aeSrcFmt[uSrcIdx];
			}
			uSwizzle = psInst->u.psVec->auSwizzle[uSrcIdx];
			sSrcMod = psInst->u.psVec->asSrcMod[uSrcIdx];

			bAlwaysUSSource = (psVecArg->uType == USC_REGTYPE_UNUSEDSOURCE) ? IMG_TRUE : IMG_FALSE;

			/*
				Get the mask of channels accessed from the source before the current swizzle
				is applied.
			*/
			uUsedChanMask = GetLiveChansInVectorArgumentPreSwizzle(psState, psInst, uSrcIdx, &uLiveChansInDest);
			
			if(bSetHintInternalRegReplacement)
			{
				if(IsUniformSource(psState, psVecArg))
				{
					bAlwaysUSSource = IMG_TRUE;
				}
				else
				{
					/* Check source replacement */
					if(bIRegSubstitutionPossible && 
						VecDualCheckSourceIsReplacable(psState, psInst, uArgBase + ((bVectorSizedArgs) ? 1 : 0)))
					{
						psInstData->bMayNeedInternalRegAssingment = IMG_TRUE;
					}
				}
			}
			else
			{
				bAlwaysUSSource = (psVecArg->uType == USC_REGTYPE_UNUSEDSOURCE) ? IMG_TRUE : IMG_FALSE;
			}
		}

		if ((uRequireUnifiedStoreSrcMask & (1 << uSrcIdx)) != 0)
		{
			uSupportedSlots = SRCSLOT_USS_MASK;
		}
		else if (bRequireNotUnifiedStoreSlot)
		{
			uSupportedSlots = SRCSLOT_GPIONLY_MASK;
		}
		else
		{
			uSupportedSlots = SRCSLOT_ALL_MASK;
		}
		bCantUseUSSIfOp1HasTwoOrThreeSrcs = IMG_FALSE;


		/*
			If this source isn't a vector sized register (e.g. a vector of immediate values) then it
			must use the unified store source.
		*/
		if (
				bAlwaysUSSource ||
				(
					psIRegAssignment != NULL &&
					psIRegAssignment->auIRegSource[uSrcIdx] == USC_UNDEF
				)
		   )
		{
			uSupportedSlots &= SRCSLOT_USS_MASK;

			/*
				This instruction needs the flag set on the instruction to interpret
				unified store sources/destinations as F32.
			*/
			if (psInstData->eUSFormat != DUALISSUE_UNIFIEDSTORE_FORMAT_ANY &&
				psInstData->eUSFormat != DUALISSUE_UNIFIEDSTORE_FORMAT_F32)
			{
				return IMG_FALSE;
			}
			psInstData->eUSFormat = DUALISSUE_UNIFIEDSTORE_FORMAT_F32;
		}

		/*
			Check for an F16 format source argument.
		*/
		if (aeSlotFormat[uSrcIdx] == UF_REGFORMAT_F16)
		{
			if(!psInstData->bMayNeedInternalRegAssingment)
			{
				/*
					GPI sources are always interpreted as F32 so this source must use
					the unified store argument.
				*/
				uSupportedSlots &= SRCSLOT_USS_MASK;
	
				/*
					This instruction needs the flag set on the instruction to interpret
					unified store sources/destinations as F16.
				*/
				if (psInstData->eUSFormat != DUALISSUE_UNIFIEDSTORE_FORMAT_ANY &&
					psInstData->eUSFormat != DUALISSUE_UNIFIEDSTORE_FORMAT_F16)
				{
					return IMG_FALSE;
				}
			}
			psInstData->eUSFormat = DUALISSUE_UNIFIEDSTORE_FORMAT_F16;
		}

		/*
			Get the swizzle this instruction would use as part of a dual-issue.
		*/
		uSwizzleCompareMask = uUsedChanMask;
		if (psInstData->bSwizzleWToX)
		{
			uSwizzleCompareMask = USC_X_CHAN_MASK;
			uSwizzle = (uSwizzle >> (USC_W_CHAN * USEASM_SWIZZLE_FIELD_SIZE)) & USEASM_SWIZZLE_VALUE_MASK;
		}
		
		if( g_psInstDesc[psInst->eOpcode].eType == INST_TYPE_VEC &&
			psInst->u.psVec->auSelectUpperHalf &&
			psInstData->bMayNeedInternalRegAssingment && GetBit(psInst->u.psVec->auSelectUpperHalf, uSrcIdx))
		{
			uSwizzle = VecDualDeriveLowerHalfSwizzle(psState, psInst, uSrcIdx);
		}

		/*
			Check which swizzle sets this swizzle belongs to.
		*/
		IsSwizzleInDualIssueSet(uSwizzle, uSwizzleCompareMask, &psInstData->auFlagValidSwizzleSet[uSrcIdx]);

		/*
			The GPI2 source slot only supports the identity swizzle.
		*/
		if (!psInstData->bMayNeedInternalRegAssingment &&
			!(psInstData->auFlagValidSwizzleSet[uSrcIdx] & DUALISSUE_SWIZZLE_OPTIONS_IDENTITY))
		{
			uSupportedSlots &= ~SRCSLOT_GPI2_MASK;
		}

		/*
			Check if this swizzle is in either of the sets used when the
			vector length of the instruction is 3.
		*/
		if ((!(psInstData->auFlagValidSwizzleSet[uSrcIdx] & DUALISSUE_SWIZZLE_OPTIONS_VEC3STD)) &&
			(!(psInstData->auFlagValidSwizzleSet[uSrcIdx] & DUALISSUE_SWIZZLE_OPTIONS_VEC3EXT)))
		{
			psInstData->bVec3Available = IMG_FALSE;
		}
		/*
			Check if this swizzle is in either of the sets used when the
			vector length of the instruction is 4.
		*/
		if ((!(psInstData->auFlagValidSwizzleSet[uSrcIdx] & DUALISSUE_SWIZZLE_OPTIONS_VEC4STD)) &&
			(!(psInstData->auFlagValidSwizzleSet[uSrcIdx] & DUALISSUE_SWIZZLE_OPTIONS_VEC4EXT)))
		{
			psInstData->bVec4Available = IMG_FALSE;
		}

		/*
			If neither vector length is possible then this instruction can't
			be dual-issued.
		*/
		if (!psInstData->bVec3Available && !psInstData->bVec4Available)
		{
			/*
				Check if this swizzle is in derived swizzle set of vector lengths
				3 or 4.
			*/
			if (psInstData->bMayNeedInternalRegAssingment && psInstData->bScalarAvailable)
			{
				if((!(psInstData->auFlagValidSwizzleSet[uSrcIdx] & DUALISSUE_SWIZZLE_OPTIONS_DERIVED_VEC3)))
				{
					psInstData->bVec3Available = IMG_FALSE;
				}
				else
				{
					psInstData->bVec3Available = IMG_TRUE;
				}
				
				if((!(psInstData->auFlagValidSwizzleSet[uSrcIdx] & DUALISSUE_SWIZZLE_OPTIONS_DERIVED_VEC4)))
				{
					psInstData->bVec4Available = IMG_FALSE;
				}
				else
				{
					psInstData->bVec4Available = IMG_TRUE;
				}
			}
			/*
				If neither vector length is possible then this instruction can't
				be dual-issued.
			*/
			if (!psInstData->bVec3Available && !psInstData->bVec4Available)
			{
				return IMG_FALSE;
			}
		}

		if ((psInstData->bVec3Available && (!(psInstData->auFlagValidSwizzleSet[uSrcIdx] & DUALISSUE_SWIZZLE_OPTIONS_VEC3STD))) ||
			(psInstData->bVec4Available && (!(psInstData->auFlagValidSwizzleSet[uSrcIdx] & DUALISSUE_SWIZZLE_OPTIONS_VEC4STD))))
		{
			/*
				The GPI0 source slot only supports the standard swizzle set.
			*/
			uSupportedSlots &= ~SRCSLOT_GPI0_MASK;

			/*
				The unified store source only supports the extended swizzle set
				if OP1 of the dual-issued instruction has one source.
			*/
			bCantUseUSSIfOp1HasTwoOrThreeSrcs = IMG_TRUE;
		}

		if (sSrcMod.bNegate)
		{
			/*
				Only the unified store and GPI1 source slots have the option of a
				negate source modifier.
			*/
			uSupportedSlots &= (SRCSLOT_USS_MASK | SRCSLOT_GPI1_MASK);
		}
		if (sSrcMod.bAbs)
		{
			/*
				Only the unified source slot has the option of an absolute source
				modifier.
			*/
			uSupportedSlots &= SRCSLOT_USS_MASK;

			/*
				The unified store source only supports an absolute source modifier
				if OP1 of the dual-issued instruction only has one source.
			*/
			bCantUseUSSIfOp1HasTwoOrThreeSrcs = IMG_TRUE;
		}

		/*
			If no slots at all are available for this source then this instruction can't be
			dual-issued.
		*/
		if (uSupportedSlots == 0)
		{
			return IMG_FALSE;
		}

		/*
			For a dual-issued instruction with a one source OP1 then the GPI2 slot is fixed
			to i2.
		*/
		auSupportedSlotsIfOp1HasOneSrc[uSrcIdx] = uSupportedSlots;
		if (psIRegAssignment != NULL && psIRegAssignment->auIRegSource[uSrcIdx] != 2)
		{
			auSupportedSlotsIfOp1HasOneSrc[uSrcIdx] &= ~SRCSLOT_GPI2_MASK;
		}

		/*
			Set up the slots available for this source when it is part of a dual-issued instruction
			with a two or three source OP1.
		*/
		auSupportedSlotsIfOp1HasTwoOrThreeSrcs[uSrcIdx] = uSupportedSlots;
		if (bCantUseUSSIfOp1HasTwoOrThreeSrcs)
		{
			auSupportedSlotsIfOp1HasTwoOrThreeSrcs[uSrcIdx] &= ~SRCSLOT_USS_MASK;
		}
	}
	
	/*
		Check for incompatible restrictions on the destination.
	*/
	if(!(psInstData->bMayNeedInternalRegAssingment))
	{
		if ((psInstData->bRequiresGPIDestination && psInstData->bRequiresUSDestination))
		{
			return IMG_FALSE;
		}
	}

	/*
		Get the slots supported when this instruction is OP1 of a dual-issued
		instruction.
	*/
	if (psInstData->uSrcCount == 2 || psInstData->uSrcCount == 3)
	{
		auPrimarySupportedSlots = auSupportedSlotsIfOp1HasTwoOrThreeSrcs;
	}
	else
	{
		auPrimarySupportedSlots = auSupportedSlotsIfOp1HasOneSrc;
	}

	/*	
		Initialize information about the supported source configs.
	*/
	psInstData->uPossiblePrimarySrcCfgs = (1 << SGXVEC_USE0_DVEC_SRCCFG_COUNT) - 1;
	psInstData->uPrimarySrcCfgsWithF32US = 0;
	uOverallPossibleSecondarySrcCfgs = (1 << SGXVEC_USE0_DVEC_SRCCFG_COUNT) - 1;
	for (uOp1SrcCount = 1; uOp1SrcCount <= SGXVEC_USE_DVEC_OP1_MAXIMUM_SRC_COUNT; uOp1SrcCount++)
	{
		psInstData->auPossibleSecondarySrcCfgs[uOp1SrcCount - 1] = (1 << SGXVEC_USE0_DVEC_SRCCFG_COUNT) - 1;
		psInstData->auSecondarySrcCfgsWithF32US[uOp1SrcCount - 1] = 0;
	}

	for (uSrcCfg = 0; uSrcCfg < SGXVEC_USE0_DVEC_SRCCFG_COUNT; uSrcCfg++)
	{
		PCDUALISSUEVECTOR_SRCSLOT	aePriMap;
		IMG_BOOL					bPossiblePrimarySrcCfg;
		IMG_BOOL					bPossibleSrcCfgForAnyOp1SrcCount;
		
		/*
			Get the mapping of inputs to the instruction to encoding slots.
		*/
		aePriMap = g_aapeDualIssueVectorPrimaryMap[psInstData->uSrcCount - 1][uSrcCfg];
		if (aePriMap != NULL)
		{
			/*
				Check for each source whether the encoding slot it uses with this
				source config is compatible with the source swizzle/modifier.
			*/
			bPossiblePrimarySrcCfg = IMG_TRUE;
			for (uSrcIdx = 0; uSrcIdx < psInstData->uSrcCount; uSrcIdx++)
			{
				DUALISSUEVECTOR_SRCSLOT		eSlotOption;

				eSlotOption = aePriMap[uSrcIdx];
				ASSERT(eSlotOption != DUALISSUEVECTOR_SRCSLOT_UNDEF);
				if ((auPrimarySupportedSlots[uSrcIdx] & (1 << eSlotOption)) == 0)
				{
					bPossiblePrimarySrcCfg = IMG_FALSE;
					break;
				}
			}
		}
		else
		{
			bPossiblePrimarySrcCfg = IMG_FALSE;
		}

		/*	
			If some of the sources aren't compatible then remove this source config from
			the mask of possible source configs when this instruction is OP1.
		*/
		if (!bPossiblePrimarySrcCfg)
		{
			psInstData->uPossiblePrimarySrcCfgs &= ~(1 << uSrcCfg);
		}

		bPossibleSrcCfgForAnyOp1SrcCount = IMG_FALSE;
		for (uOp1SrcCount = 1; uOp1SrcCount <= SGXVEC_USE_DVEC_OP1_MAXIMUM_SRC_COUNT; uOp1SrcCount++)
		{
			PCDUALISSUEVECTOR_SRCSLOT	psSecMap;
			IMG_BOOL					bPossibleSecondarySrcCfg;
			IMG_PUINT32					auSecondarySupportedSlots;

			bPossibleSecondarySrcCfg = IMG_TRUE;

			if (uOp1SrcCount == 2 || uOp1SrcCount == 3)
			{
				auSecondarySupportedSlots = auSupportedSlotsIfOp1HasTwoOrThreeSrcs;
			}
			else
			{
				auSecondarySupportedSlots = auSupportedSlotsIfOp1HasOneSrc;
			}

			/*
				Check for each source whether the encoding slot used with this source config
				and count of OP1 sources is compatible with the source swizzle/modifier.
			*/
			psSecMap = g_aaapeDualIssueVectorSecondaryMap[uOp1SrcCount - 1][psInstData->uSrcCount - 1][uSrcCfg];
			if (psSecMap != NULL)
			{
				for (uSrcIdx = 0; uSrcIdx < psInstData->uSrcCount; uSrcIdx++)
				{
					DUALISSUEVECTOR_SRCSLOT		eSlotOption;

					eSlotOption = psSecMap[uSrcIdx];
					ASSERT(eSlotOption != DUALISSUEVECTOR_SRCSLOT_UNDEF);
					if ((auSecondarySupportedSlots[uSrcIdx] & (1 << eSlotOption)) == 0)
					{
						bPossibleSecondarySrcCfg = IMG_FALSE;
						break;
					}
				}
			}
			else
			{
				bPossibleSecondarySrcCfg = IMG_FALSE;
			}

			/*
				If some of the sources aren't compatible with the encoding slots then remove
				this source config from the possible options.
			*/
			if (!bPossibleSecondarySrcCfg)
			{
				psInstData->auPossibleSecondarySrcCfgs[uOp1SrcCount - 1] &= ~(1 << uSrcCfg);
			}
			else
			{
				bPossibleSrcCfgForAnyOp1SrcCount = IMG_TRUE;
			}
		}

		/*
			Record if any source configuration would be valid with this instruction as
			secondary for a fast rejection of invalid combinations.
		*/
		if (!bPossibleSrcCfgForAnyOp1SrcCount)
		{
			uOverallPossibleSecondarySrcCfgs &= ~(1 << uSrcCfg);
		}
	}

	/*
		Check if there are no source configurations which are compatible with this instruction
		as OP1.
	*/
	psInstData->bCanBePrimary = IMG_TRUE;
	if (psInstData->uPossiblePrimarySrcCfgs == 0)
	{
		psInstData->bCanBePrimary = IMG_FALSE;
	}

	/*
		Check if there are no source configurations which are compatible with this instruction
		as OP2.
	*/
	psInstData->bCanBeSecondary = IMG_TRUE;
	if (uOverallPossibleSecondarySrcCfgs == 0)
	{
		psInstData->bCanBeSecondary = IMG_FALSE;
	}

	/*
		If the instruction can't be either OP1 or OP2 then don't add to the dual-issue candidates.
	*/
	if (!psInstData->bCanBePrimary && !psInstData->bCanBeSecondary)
	{
		return IMG_FALSE;
	}

	/*
		MAD3 and MAD4 can only be OP1.
	*/
	if (!psInstData->bCanBePrimary && psInstData->eOpType == DUALISSUE_OP_TYPE_MAD && !psInstData->bScalarAvailable)
	{
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

typedef enum
{
	VDUAL_SRC_SWIZZLE_IDENTITY = 0x00000001,
	VDUAL_SRC_SWIZZLE_STANDARD = 0x00000002,
	VDUAL_SRC_SWIZZLE_EXTENDED = 0x00000004,
} VDUAL_SRC_SWIZZLE;

typedef struct
{
	IMG_UINT32			eSwizzleSupportNeeded;
	IMG_BOOL			bIsVec3Swizzle;
	IMG_UINT32 const *	puSwizzleSet;
	IMG_UINT32			uSwizzleSetCount;
} VDUAL_SWIZZLE_SET, *PVDUAL_SWIZZLE_SET;

typedef VDUAL_SWIZZLE_SET const* PCVDUAL_SWIZZLE_SET;

static
IMG_UINT32 DualIssueRemapToSupportedSwizzle(PINTERMEDIATE_STATE	psState,
										   IMG_BOOL				bVec3,
										   VDUAL_SRC_SWIZZLE	eSwizzleSupport,
										   IMG_UINT32			uUsedChanMask,
										   IMG_UINT32			uOrigSwizzle)
/*****************************************************************************
 FUNCTION	: DualIssueRemapToSupportedSwizzle
    
 PURPOSE	: For a swizzle which might not be supported exactly by a dual-issue
			  instruction return an equivalent swizzle which is supported.

 PARAMETERS	: psState			- Compiler state.
			  bVec3				- TRUE if the dual-issue instruction is vec3.
								  FALSE if the dual-issue instruction is vec4.
			  eSwizzleSupport	- Swizzle sets supported by the source to which
								this swizzle corresponds.
			  uUsedChanMask		- Channels referenced from the source using the
								swizzle.
			  uOrigSwizzle		- The original swizzle.

 RETURNS	: The new swizzle.
*****************************************************************************/
{
	IMG_BOOL			bRet;
	IMG_UINT32			uSet, uNewSwizzle;
	
	static const VDUAL_SWIZZLE_SET	sSwizzleSet[] =
	{
		{
			VDUAL_SRC_SWIZZLE_IDENTITY, IMG_FALSE,
			g_auIdentitySwizzle, 
			sizeof(g_auIdentitySwizzle) / sizeof(g_auIdentitySwizzle[0])
		},
		{
			VDUAL_SRC_SWIZZLE_IDENTITY, IMG_TRUE,
			g_auIdentitySwizzle, 
			sizeof(g_auIdentitySwizzle) / sizeof(g_auIdentitySwizzle[0])
		},
		{
			VDUAL_SRC_SWIZZLE_STANDARD | VDUAL_SRC_SWIZZLE_EXTENDED, IMG_TRUE,
			g_auVec3DualIssueStandardSwizzles, 
			sizeof(g_auVec3DualIssueStandardSwizzles) / sizeof(g_auVec3DualIssueStandardSwizzles[0])
		},
		{
			VDUAL_SRC_SWIZZLE_EXTENDED, IMG_TRUE,
			g_auVec3DualIssueExtendedSwizzles, 
			sizeof(g_auVec3DualIssueExtendedSwizzles) / sizeof(g_auVec3DualIssueExtendedSwizzles[0])
		},
		{
			VDUAL_SRC_SWIZZLE_STANDARD | VDUAL_SRC_SWIZZLE_EXTENDED, IMG_TRUE,
			g_auVec3DualIssueDerivedSwizzles, 
			sizeof(g_auVec3DualIssueDerivedSwizzles) / sizeof(g_auVec3DualIssueDerivedSwizzles[0])
		},
		{
			VDUAL_SRC_SWIZZLE_STANDARD | VDUAL_SRC_SWIZZLE_EXTENDED, IMG_FALSE,
			g_auVec4DualIssueStandardSwizzles, 
			sizeof(g_auVec4DualIssueStandardSwizzles) / sizeof(g_auVec4DualIssueStandardSwizzles[0])
		},
		{
			VDUAL_SRC_SWIZZLE_EXTENDED, IMG_FALSE,
			g_auVec4DualIssueExtendedSwizzles, 
			sizeof(g_auVec4DualIssueExtendedSwizzles) / sizeof(g_auVec4DualIssueExtendedSwizzles[0])
		},
		{
			VDUAL_SRC_SWIZZLE_STANDARD | VDUAL_SRC_SWIZZLE_EXTENDED, IMG_TRUE,
			g_auVec4DualIssueDerivedSwizzles, 
			sizeof(g_auVec4DualIssueDerivedSwizzles) / sizeof(g_auVec4DualIssueDerivedSwizzles[0])
		}
	};
	
	for(uSet = 0; uSet < sizeof(sSwizzleSet)/sizeof(sSwizzleSet[0]); uSet++)
	{
		if(sSwizzleSet[uSet].bIsVec3Swizzle == bVec3 &&
				 (eSwizzleSupport & sSwizzleSet[uSet].eSwizzleSupportNeeded))
		{
			bRet = IsSwizzleInSet(sSwizzleSet[uSet].puSwizzleSet,
								sSwizzleSet[uSet].uSwizzleSetCount,
								uOrigSwizzle,
								uUsedChanMask,
								&uNewSwizzle);
			if (bRet)
			{
				return uNewSwizzle;
			}
		}
	}
	imgabort();
}

static
IMG_VOID CopyDualIssueSources(PINTERMEDIATE_STATE		psState,
							  PINST						psVDualInst,
							  IMG_BOOL					bVec3,
							  PDUALISSUE_INST			psPriInstData,
							  VDUAL_OP_TYPE				eOpType,
							  VDUAL_OP					eOp,
							  PCDUALISSUEVECTOR_SRCSLOT	aeSrcMap,
							  PINST						psSrcInst,
							  PDUALISSUE_INST			psSrcInstData,
							  IMG_UINT32				uSrcCount)
/*****************************************************************************
 FUNCTION	: CopyDualIssueSources
    
 PURPOSE	: Copy the sources to one operation in a dual-issued instruction
			  from the original instruction to the instruction representing
			  the dual-issue.

 PARAMETERS	: psState			- Compiler state.
			  psVDualInst		- Dual-issue instruction.
			  bVec3				- TRUE if the dual-issue instruction is vec3.
								  FALSE if the dual-issue instruction is vec4.
			  psPriInstData		- Information about the primary operation in
								the dual-issue.
			  eOpType			- Is the operation the primary or the secondary
								part of the dual-issue.
			  aeSrcMap			- Maps from inputs to the operation to 
								encoding slots in the instruction.
			  psSrcInst			- Operation whose sources are to be copied.
			  psSrcInstData		- Information about dual-issue setup for the
								operation.
			  uSrcCount			- Count of sources to the operation.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uSrcIdx;

	ASSERT(aeSrcMap != NULL);

	for (uSrcIdx = 0; uSrcIdx < uSrcCount; uSrcIdx++)
	{
		DUALISSUEVECTOR_SRCSLOT	eSrcSlot;
		IMG_UINT32				uDestIdx;
		VDUAL_SRC_SWIZZLE		eSwizzleSupport;
		IMG_UINT32				uSrcUsedChanMask;
		IMG_UINT32				uSwizzle;
		IMG_UINT32				uRemappedSwizzle;

		eSrcSlot = aeSrcMap[uSrcIdx];
		ASSERT(eSrcSlot != DUALISSUEVECTOR_SRCSLOT_UNDEF);

		/*
			Map from the encoding slot to the source in the intermediate instruction.
		*/
		switch (eSrcSlot)
		{
			case DUALISSUEVECTOR_SRCSLOT_UNIFIEDSTORE:	
			{
				uDestIdx = VDUAL_SRCSLOT_UNIFIEDSTORE; 
				if (psPriInstData->uSrcCount == 1)
				{
					eSwizzleSupport = VDUAL_SRC_SWIZZLE_EXTENDED;
				}
				else
				{
					eSwizzleSupport = VDUAL_SRC_SWIZZLE_STANDARD;
				}
				break;
			}
			case DUALISSUEVECTOR_SRCSLOT_GPI0:			
			{
				uDestIdx = VDUAL_SRCSLOT_GPI0;
				eSwizzleSupport = VDUAL_SRC_SWIZZLE_STANDARD;
				break;
			}
			case DUALISSUEVECTOR_SRCSLOT_GPI1:			
			{
				uDestIdx = VDUAL_SRCSLOT_GPI1; 
				eSwizzleSupport = VDUAL_SRC_SWIZZLE_EXTENDED;
				break;
			}
			case DUALISSUEVECTOR_SRCSLOT_GPI2:			
			{
				uDestIdx = VDUAL_SRCSLOT_GPI2; 
				eSwizzleSupport = VDUAL_SRC_SWIZZLE_IDENTITY;
				psVDualInst->u.psVec->sDual.bGPI2SlotUsed = IMG_TRUE;
				break;
			}
			default: imgabort();
		}
		
		/*
			Record which operation (primary or secondary) uses this source slot.
		*/
		ASSERT(psVDualInst->u.psVec->sDual.aeSrcUsage[uDestIdx] == VDUAL_OP_TYPE_UNDEF);
		psVDualInst->u.psVec->sDual.aeSrcUsage[uDestIdx] = eOpType;

		/*
			Record for each operation which source slots it uses.
		*/
		if (eOpType == VDUAL_OP_TYPE_PRIMARY)
		{
			psVDualInst->u.psVec->sDual.auPriOpSrcs[uSrcIdx] = uDestIdx;
		}
		else
		{
			ASSERT(eOpType == VDUAL_OP_TYPE_SECONDARY);
			psVDualInst->u.psVec->sDual.auSecOpSrcs[uSrcIdx] = uDestIdx;
		}

		/*
			Get the swizzle applied to this source in the old instruction.
		*/
		if (psSrcInst->eOpcode == IRESTOREIREG)
		{
			uSwizzle = USEASM_SWIZZLE(X, Y, Z, W);
			uSrcUsedChanMask = psSrcInstData->uDestWriteMask;
		}
		else
		{
			ASSERT(g_psInstDesc[psSrcInst->eOpcode].eType == INST_TYPE_VEC);
			uSwizzle = psSrcInst->u.psVec->auSwizzle[uSrcIdx];

			SetBit(psVDualInst->u.psVec->auSelectUpperHalf, uDestIdx, 
			   GetBit(psSrcInst->u.psVec->auSelectUpperHalf, uSrcIdx));

			/*
				Get the mask of channels referenced from the source in the old
				instruction before the swizzle is applied.
			*/	
			GetLiveChansInSourceSlot(psState, psSrcInst, uSrcIdx, &uSrcUsedChanMask, NULL /* puChansUsedPostSwizzle */);
		}

		if (eOp == VDUAL_OP_SSQ3 || eOp == VDUAL_OP_DP3)
		{
			/*
				Since we're changing the opcode of this operation from DP -> DP3 we know it's valid to ignore the last
				channel of the swizzles.
			*/
			uSrcUsedChanMask &= USC_XYZ_CHAN_MASK;
		}
		if (psSrcInstData->bSwizzleWToX)
		{
			/*
				As a seperate instruction this operation used just the W channel from the source. As part
				of a dual-issue instruction it uses just X.
			*/
			ASSERT(uSrcUsedChanMask == USC_W_CHAN_MASK);
			uSrcUsedChanMask = USC_X_CHAN_MASK;
			uSwizzle = (uSwizzle >> (USC_W_CHAN * USEASM_SWIZZLE_FIELD_SIZE)) & USEASM_SWIZZLE_VALUE_MASK;
		}

		/*
			The original swizzle might not be directly supported by the dual-issue instruction. But we already
			checked there is a supported swizzle with the same channel selections on the channels actually
			used by the operation. So switch to the alternate swizzle here.
		*/
		uRemappedSwizzle = 
			DualIssueRemapToSupportedSwizzle(psState, 
											 bVec3, 
											 eSwizzleSupport, 
											 uSrcUsedChanMask,
											 uSwizzle);

		/*
			Copy the source and associated information to dual-issue instruction.
		*/
		if (psSrcInst->eOpcode == IRESTOREIREG)
		{
			MoveSrc(psState, psVDualInst, uDestIdx, psSrcInst, 0 /* uMoveFromSrcIdx */);
			psVDualInst->u.psVec->aeSrcFmt[uDestIdx] = UF_REGFORMAT_F32;
		}
		else
		{
			MoveVectorSource(psState, psVDualInst, uDestIdx, psSrcInst, uSrcIdx, IMG_TRUE /* bCopySourceModifier */);
		}
		psVDualInst->u.psVec->auSwizzle[uDestIdx] = uRemappedSwizzle;
	}
}

static
IMG_BOOL InstWritesGPIDestination(PINST psInst, PIREG_ASSIGNMENT psInstIRegAssignment)
{
	if (psInstIRegAssignment != NULL && psInstIRegAssignment->bDestIsIReg)
	{
		return IMG_TRUE;
	}
	if (psInst->asDest[0].uType == USEASM_REGTYPE_FPINTERNAL)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static
PINST CreateDualIssueInstruction(PINTERMEDIATE_STATE	psState,
								 PINST					psInst1,
								 PINST					psInst2,
								 PIREG_ASSIGNMENT		psInst1IRegAssignment,
								 PIREG_ASSIGNMENT		psInst2IRegAssignment,
								 PDUALISSUE_INST		psInst1Data,
								 PDUALISSUE_INST		psInst2Data,
								 IMG_BOOL				bInst1IsDummy,
								 IMG_BOOL				bInst2IsDummy,
								 PDUALISSUE_SETUP		psSetup)
/*****************************************************************************
 FUNCTION	: CreateDualIssueInstruction
    
 PURPOSE	: Creates a new dual-issue instruction combining two existing
			  instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- Instructions to combine.
			  psInst2
			  psInst1IRegAssignment
			  psInst2IRegAssignment
			  psInst1Data		- Information about the dual-issue operation
			  psInst2Data		to be created from each instruction.
			  psSetup			- Parameters for the dual-issue instruction.

 RETURNS	: The created instruction.
*****************************************************************************/
{
	VDUAL_OP					ePriOp;
	VDUAL_OP					eSecOp;
	
	PDUALISSUE_INST				psPriInstData;
	PDUALISSUE_INST				psSecInstData;
	PDUALISSUE_INST				psUSDestInstData;
	
	PINST						psPriInst;
	PINST						psSecInst;
	
	PINST						psGPIDestInst;
	PINST						psUSDestInst;
	
	PINST						psDualInst;
	
	IMG_BOOL					bPriInstIsDummy;
	IMG_BOOL					bSecInstIsDummy;
	
	PCDUALISSUEVECTOR_SRCSLOT	aePriMap;
	PCDUALISSUEVECTOR_SRCSLOT	aeSecMap;
	IMG_UINT32					uPriOpSrcCount;
	IMG_UINT32					uSecOpSrcCount;
	IMG_UINT32					uArg;
	IMG_UINT32					uOrigUSWriteMask;
	IMG_UINT32					uUnifiedStoreWriteMask;
	IMG_UINT32					uUSLiveChansInDest;
	IMG_UINT32					uSrcIdx;
	
	IMG_BOOL					bPriUsesGPIDest;
	PIREG_ASSIGNMENT			psPriIRegAssignment;
	PIREG_ASSIGNMENT			psSecIRegAssignment;
	IMG_BOOL					bUSDestIsGPI;
	IMG_UINT32					uUSInstDestSelectShift;

	ASSERT(psSetup->ePrimDestSlotConfiguration < VDUAL_DESTSLOT_COUNT);
	if (psSetup->bInst1IsPrimary)
	{
		psPriInst = psInst1;
		ePriOp = psInst1Data->eSetupOp;
		psPriInstData = psInst1Data;
		bPriInstIsDummy = bInst1IsDummy;
		psPriIRegAssignment = psInst1IRegAssignment;
		psSecInst = psInst2;
		eSecOp = psInst2Data->eSetupOp;
		psSecInstData = psInst2Data;
		bSecInstIsDummy = bInst2IsDummy;
		psSecIRegAssignment = psInst2IRegAssignment;
	}
	else
	{
		psPriInst = psInst2;
		ePriOp = psInst2Data->eSetupOp;
		psPriInstData = psInst2Data;
		bPriInstIsDummy = bInst2IsDummy;
		psPriIRegAssignment = psInst2IRegAssignment;
		psSecInst = psInst1;
		eSecOp = psInst1Data->eSetupOp;
		psSecInstData = psInst1Data;
		bSecInstIsDummy = bInst1IsDummy;
		psSecIRegAssignment = psInst1IRegAssignment;
	}

	bPriUsesGPIDest = (psSetup->ePrimDestSlotConfiguration == VDUAL_DESTSLOT_GPI) ? IMG_TRUE : IMG_FALSE;

	psGPIDestInst = (bPriUsesGPIDest) ? psPriInst : psSecInst;
	psUSDestInst = (bPriUsesGPIDest) ? psSecInst : psPriInst;
	
	psUSDestInstData = (bPriUsesGPIDest) ? psSecInstData : psPriInstData;
	
	bUSDestIsGPI = InstWritesGPIDestination(psUSDestInst, ((psUSDestInst == psInst1)? psInst1IRegAssignment : psInst2IRegAssignment));
	
	psDualInst = AllocateInst(psState, psInst1);
	SetOpcodeAndDestCount(psState, psDualInst, IVDUAL, 
						  VDUAL_DESTSLOT_COUNT * ((psSetup->bExpandVectors) ? VECTOR_LENGTH : 1));
	
	if(psSetup->bExpandVectors)
	{
		ASSERT(psGPIDestInst->asDest[0].uType == USEASM_REGTYPE_FPINTERNAL);
	}

	/*
		Copy the predicate from the old instruction pair.
	*/
	CopyPredicate(psState, psDualInst, psInst1);

	/*
		Initialize the information about the dual-issue instruction.
	*/
	for (uArg = 0; uArg < VDUAL_SRCSLOT_COUNT; uArg++)
	{
		psDualInst->u.psVec->sDual.aeSrcUsage[uArg] = VDUAL_OP_TYPE_UNDEF;
	}

	/* Setup created dual instruction parameters */
	psDualInst->u.psVec->sDual.ePriOp = ePriOp;
	psDualInst->u.psVec->sDual.bPriOpIsDummy = bPriInstIsDummy;
	psDualInst->u.psVec->sDual.eSecOp = eSecOp;
	psDualInst->u.psVec->sDual.bSecOpIsDummy = bSecInstIsDummy;
	psDualInst->u.psVec->sDual.bF16 = psSetup->bF16;
	psDualInst->u.psVec->sDual.bVec3 = psSetup->bVec3;
	psDualInst->u.psVec->sDual.bPriUsesGPIDest = bPriUsesGPIDest;

	/*
		Set the default value. CopyDualIssueSources will update if GPI2 is used.
	*/
	psDualInst->u.psVec->sDual.bGPI2SlotUsed = IMG_FALSE;

	if (psUSDestInst->eOpcode == IRESTOREIREG)
	{
		uUSInstDestSelectShift = 0;
	}
	else
	{
		ASSERT(g_psInstDesc[psUSDestInst->eOpcode].eType == INST_TYPE_VEC);
		uUSInstDestSelectShift = psUSDestInst->u.psVec->uDestSelectShift;
	}
	
	psDualInst->u.psVec->uDestSelectShift = uUSInstDestSelectShift;
	
	if(psSetup->bExpandVectors)
	{
		IMG_UINT32	uInx, uDest;
		PINST		apsDIInstruction[VDUAL_DESTSLOT_COUNT];
		IMG_UINT32	auWriteMask[VDUAL_DESTSLOT_COUNT] = {0, 0};
		IMG_UINT32	auLiveChansInDest[VDUAL_DESTSLOT_COUNT] = {0, 0};
		
		apsDIInstruction[VDUAL_DESTSLOT_UNIFIEDSTORE] = psUSDestInst;
		apsDIInstruction[VDUAL_DESTSLOT_GPI] = psGPIDestInst;
		
		uUSLiveChansInDest = 0;
		
		/* Replicate destination arguments */
		for(uDest = 0; uDest < VDUAL_DESTSLOT_COUNT; uDest++)
		{
			for(uInx = 0; uInx < apsDIInstruction[uDest]->uDestCount; uInx++)
			{
				psDualInst->auDestMask[uDest * VECTOR_LENGTH + uInx] = apsDIInstruction[uDest]->auDestMask[uInx];
				psDualInst->auLiveChansInDest[uDest * VECTOR_LENGTH + uInx] = apsDIInstruction[uDest]->auLiveChansInDest[uInx];
				if(apsDIInstruction[uDest]->auDestMask[uInx])
				{
					SetDest(psState, 
							psDualInst,
							uDest * VECTOR_LENGTH + uInx /* uDestIdx */, 
							apsDIInstruction[uDest]->asDest[uInx].uType, 
							apsDIInstruction[uDest]->asDest[uInx].uNumber, 
							UF_REGFORMAT_F32);
					auWriteMask[uDest] |= (1 << uInx);
				}
				else
				{
					psDualInst->auDestMask[uDest * VECTOR_LENGTH + uInx] = 0;
					SetDest(psState, 
							psDualInst,
							uDest * VECTOR_LENGTH + uInx /* uDestIdx */, 
							apsDIInstruction[uDest]->asDest[uInx].uType, 
							apsDIInstruction[uDest]->asDest[uInx].uNumber, 
							UF_REGFORMAT_F32);
				}
				auLiveChansInDest[uDest] |= (apsDIInstruction[uDest]->auLiveChansInDest[uInx]) ? (1 << uInx) : 0;
			}
			for(; uInx < VECTOR_LENGTH; uInx++)
			{
				psDualInst->auDestMask[uDest * VECTOR_LENGTH + uInx] = 0;
				SetDestUnused(psState, psDualInst, uDest * VECTOR_LENGTH + uInx);
			}
		}
		
		uUSLiveChansInDest = auLiveChansInDest[VDUAL_DESTSLOT_UNIFIEDSTORE];
		uOrigUSWriteMask = auWriteMask[VDUAL_DESTSLOT_UNIFIEDSTORE];
		uUnifiedStoreWriteMask = 
			GetVDUALUnifiedStoreDestinationMask(psState, psDualInst, uOrigUSWriteMask, bUSDestIsGPI);
	}
	else
	{
		uOrigUSWriteMask = psUSDestInst->auDestMask[0];
		if(psUSDestInstData->bSwizzleWToX)
		{
			if(psUSDestInstData->eUSFormat == DUALISSUE_UNIFIEDSTORE_FORMAT_F16)
			{
				uOrigUSWriteMask = USC_XY_CHAN_MASK;
			}
			else
			{
				uOrigUSWriteMask = USC_X_CHAN_MASK;
			}
		}
		
		/* Calculate the write mask for the unified store destination. */
		uUnifiedStoreWriteMask = 
			GetVDUALUnifiedStoreDestinationMask(psState, psDualInst, uOrigUSWriteMask, bUSDestIsGPI);
		ASSERT((uOrigUSWriteMask & ~uUnifiedStoreWriteMask) == 0);
		psDualInst->auDestMask[VDUAL_DESTSLOT_UNIFIEDSTORE] = uUnifiedStoreWriteMask;
		
		/*
			Copy the destinations of the combined instructions.
		*/
		MoveDest(psState, psDualInst, VDUAL_DESTSLOT_GPI, psGPIDestInst, 0 /* uMoveFromDestIdx */);
		CopyPartiallyWrittenDest(psState, psDualInst, VDUAL_DESTSLOT_GPI, psGPIDestInst, 0 /* uMoveFromDestIdx */);
		psDualInst->auDestMask[VDUAL_DESTSLOT_GPI] = psGPIDestInst->auDestMask[0];
		psDualInst->auLiveChansInDest[VDUAL_DESTSLOT_GPI] = psGPIDestInst->auLiveChansInDest[0];
	
		MoveDest(psState, psDualInst, VDUAL_DESTSLOT_UNIFIEDSTORE, psUSDestInst, 0 /* uMoveFromDestIdx */);
		psDualInst->auLiveChansInDest[VDUAL_DESTSLOT_UNIFIEDSTORE] = psUSDestInst->auLiveChansInDest[0];
		CopyPartiallyWrittenDest(psState, psDualInst, VDUAL_DESTSLOT_UNIFIEDSTORE, psUSDestInst, 0 /* uMoveFromDestIdx */);
		
		uUSLiveChansInDest = psUSDestInst->auLiveChansInDest[0];
	}
	/*
		If we are writing more channels in the unified store destination than the
		original instruction then we shouldn't be overwriting channels used earlier.
	*/
	ASSERT((uUSLiveChansInDest & (uUnifiedStoreWriteMask & ~uOrigUSWriteMask)) == 0);

	/*
		Copy the primary instruction's sources to the slots they are assigned to in
		the dual-issue instruction.
	*/
	uPriOpSrcCount = psPriInstData->uSrcCount;
	aePriMap = g_aapeDualIssueVectorPrimaryMap[uPriOpSrcCount - 1][psSetup->uSrcCfg];
	CopyDualIssueSources(psState, 
						 psDualInst, 
						 psSetup->bVec3,
						 psPriInstData,
						 VDUAL_OP_TYPE_PRIMARY,
						 ePriOp,
						 aePriMap, 
						 psPriInst, 
						 psPriInstData,
						 uPriOpSrcCount);

	/*
		Copy the secondary instruction's sources to the slots they are assigned to in
		the dual-issue instruction.
	*/
	uSecOpSrcCount = psSecInstData->uSrcCount;
	aeSecMap = g_aaapeDualIssueVectorSecondaryMap[uPriOpSrcCount - 1][uSecOpSrcCount - 1][psSetup->uSrcCfg];
	CopyDualIssueSources(psState, 
						 psDualInst, 
						 psSetup->bVec3,
						 psPriInstData,
						 VDUAL_OP_TYPE_SECONDARY,
						 eSecOp,
						 aeSecMap, 
						 psSecInst, 
						 psSecInstData,
						 uSecOpSrcCount);

	/*
		For any sources to the dual-issue instruction not used by either operation
		flag them as unreferenced.
	*/
	for (uSrcIdx = 0; uSrcIdx < VDUAL_SRCSLOT_COUNT; uSrcIdx++)
	{
		if (psDualInst->u.psVec->sDual.aeSrcUsage[uSrcIdx] == VDUAL_OP_TYPE_UNDEF)
		{
			PARG	psArg = &psDualInst->asArg[uSrcIdx * SOURCE_ARGUMENTS_PER_VECTOR];

			/* Set the data for this argument to dummy values. */
			InitInstArg(psArg);
			psArg->uType = USC_REGTYPE_UNUSEDSOURCE;

			psDualInst->u.psVec->auSwizzle[uSrcIdx] = USEASM_SWIZZLE(X, Y, Z, W);
			psDualInst->u.psVec->aeSrcFmt[uSrcIdx] = UF_REGFORMAT_F32;

			psDualInst->u.psVec->sDual.aeSrcUsage[uSrcIdx] = VDUAL_OP_TYPE_NONE;
		}
	}

	return psDualInst;
}

IMG_INTERNAL
IMG_BOOL IsPossibleDualIssueWithVMOV(PINTERMEDIATE_STATE	psState,
									 PINST					psInst,
									 PIREG_ASSIGNMENT		psInstIRegAssignment,
									 PINST					psVMOVInst,
									 PIREG_ASSIGNMENT		psVMOVIRegAssignment,
									 IMG_UINT32				uLiveChansInVMOVDest,
									 IMG_BOOL				bCreateDual,
									 PINST*					ppsDualIssueInst)
/*****************************************************************************
 FUNCTION	: IsPossibleDualIssueWithVMOV
    
 PURPOSE	: Check for a possible location for dual-issuing a VMOV instruction
			  with another instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Non-VMOV instruction.
			  psVMOVInst		- If non-NULL check for dual-issuing with the
								non-VMOV instruction.
								  If NULL just check if the non-VMOV instruction
							    could be dual-issued with any instruction.
			  uLiveChansInVMOVDest
								- Mask of channels which are used later from the VMOV's
								destination at the dual-issue location.
			  bCreateDual		- If TRUE also create the dual-issued instruction.
			  ppsDualIssueInst	- If non-NULL returns a pointer to the dual-issued instruction.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	DUALISSUE_INST		sInstData;

	/*
		Check if the supplied instruction could be part of a dual-issue
		instruction; reserving the unified store slot for the VMOV.
	*/
	if (!IsPossibleDualIssueOp(psState, 
							   psInst, 
							   psInst->auLiveChansInDest[0],
							   0 /* uRequireUnifiedStoreSrcMaskt */, 
							   IMG_TRUE /* bRequireNotUnifiedStoreSlot */,
							   IMG_FALSE /* bSetHintInternalRegReplacement */,
							   psInstIRegAssignment,
							   &sInstData))
	{
		ASSERT(!bCreateDual);
		return IMG_FALSE;
	}

	if (psVMOVInst != NULL)
	{
		IMG_UINT32			uExitHint;
		DUALISSUE_INST		sVMOVData;
		DUALISSUE_SETUP		sSetup;

		/*
			Could the VMOV be part of a dual-issue?
		*/
		if (!IsPossibleDualIssueOp(psState, 
								   psVMOVInst, 
								   uLiveChansInVMOVDest,
								   0 /* uRequireUnifiedStoreSrcMask */, 
								   IMG_FALSE /* bRequireNotUnifiedStoreSlot */, 
								   IMG_FALSE /* bSetHintInternalRegReplacement */,
								   psVMOVIRegAssignment,
								   &sVMOVData))
		{
			ASSERT(!bCreateDual);
			return IMG_FALSE;
		}

		/*
			Check both instructions have the same predicate.
		*/
		if (!EqualPredicates(psInst, psVMOVInst))
		{
			ASSERT(!bCreateDual);
			return IMG_FALSE;
		}
		
		/*
			Can the two instructions to be dual-issued.
		*/
		if (!CanDualIssue(psState, &sInstData, &sVMOVData, &sSetup, IMG_FALSE, IMG_FALSE, &uExitHint))
		{
			ASSERT(!bCreateDual);
			return IMG_FALSE;
		}

		if (bCreateDual)
		{
			PINST	psDualIssueInst;

			/*
				Create the dual-issue instruction.
			*/
			psDualIssueInst = CreateDualIssueInstruction(psState, 
														 psInst, 
														 psVMOVInst, 
														 psInstIRegAssignment,
														 psVMOVIRegAssignment,
														 &sInstData, 
														 &sVMOVData, 
														 IMG_FALSE /* bInst1IsDummy */,
														 IMG_FALSE /* bInst2IsDummy */,
														 &sSetup);

			/*	
				Insert the dual-issue instruction at the same point as the non-VMOV.
			*/
			InsertInstBefore(psState, psInst->psBlock, psDualIssueInst, psInst);

			if (ppsDualIssueInst != NULL)
			{
				*ppsDualIssueInst = psDualIssueInst;
			}
		}
	}
	else
	{
		ASSERT(!bCreateDual);
	}

	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL ConvertSingleInstToDualIssue(PINTERMEDIATE_STATE	psState,
									  PINST					psInst,
									  PIREG_ASSIGNMENT		psIRegAssignment,
									  IMG_UINT32			uUnifiedStoreSrcMask,
									  IMG_BOOL				bAvailableIReg,
									  IMG_BOOL				bCheckOnly,
									  PINST*				ppsDualInst,
									  PARG*					ppsDummyIRegDest)
/*****************************************************************************
 FUNCTION	: ConvertSingleInstToDualIssue
    
 PURPOSE	: Convert a single instruction to a dual-issue instruction
			  co-issued with a dummy operation.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to convert.
			  psIRegAssignment	- Information about which instruction sources
								are internal registers.
			  uUnifiedStoreSrcMask
								- Mask of sources to the original instruction
								which should use the dual-issue instruction's
								unified store slot.
			  bAvailableIReg	- TRUE if an internal register is available to be 
								written by the dual-isuse instruction.
			  bCheckOnly		- If TRUE only check if the conversion is
								possible.
			  ppsSSQInst		- Returns the dual-issue instruction.
			  ppsDummyIRegDest	- Return either NULL or a pointer to the dual-issue
								instruction destination using the available internal
								register.

 RETURNS	: TRUE if the instruction was converted.
*****************************************************************************/
{
	DUALISSUE_INST	sInstData;

	/*
		Check if the supplied instruction could be part of a dual-issue
		instruction.
	*/
	if (!IsPossibleDualIssueOp(psState, 
							   psInst, 
							   psInst->auLiveChansInDest[0],
							   uUnifiedStoreSrcMask, 
							   IMG_FALSE /* bRequireNotUnifiedStoreSlot */, 
							   IMG_FALSE /* bSetHintInternalRegReplacement */,
							   psIRegAssignment, 
							   &sInstData))
	{
		return IMG_FALSE;
	}

	/*
		Force the SSQ instruction to use the GPI destination if we don't have a 
		spare internal register for the destination of the dummy VMOV.
	*/
	if (!bAvailableIReg && sInstData.bRequiresUSDestination)
	{
		return IMG_FALSE;
	}

	if (!bCheckOnly)
	{
		PINST			psDummyMOVInst;
		DUALISSUE_INST	sDummyMOVInstData;
		IMG_BOOL		bRet;
		DUALISSUE_SETUP	sSetup;
		IREG_ASSIGNMENT	sDummyMOVIRegAssignment;
		IMG_UINT32		uExitHint;

		/*
			Create a dummy instruction to be coissued with the SSQ operation.
		*/
		psDummyMOVInst = AllocateInst(psState, NULL);

		SetOpcode(psState, psDummyMOVInst, IVMOV);

		psDummyMOVInst->asArg[0].uType = USEASM_REGTYPE_FPINTERNAL;
		psDummyMOVInst->asArg[0].uNumber = 0;
		sDummyMOVIRegAssignment.auIRegSource[0] = 0;

		psDummyMOVInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
		psDummyMOVInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F32;

		SetDest(psState, psDummyMOVInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, GetNextRegister(psState), UF_REGFORMAT_F32);

		if (sInstData.bRequiresUSDestination)
		{
			ASSERT(bAvailableIReg);

			psDummyMOVInst->auDestMask[0] = USC_X_CHAN_MASK;
			psDummyMOVInst->auLiveChansInDest[0] = USC_X_CHAN_MASK;

			sDummyMOVIRegAssignment.bDestIsIReg = IMG_TRUE;
		}
		else
		{
			psDummyMOVInst->auDestMask[0] = 0;
			psDummyMOVInst->auLiveChansInDest[0] = 0;

			sDummyMOVIRegAssignment.bDestIsIReg = IMG_FALSE;
		}

		/*	
			Set up information about the dummy instruction as part of a dual-issue instruction.
		*/
		bRet = IsPossibleDualIssueOp(psState, 
									 psDummyMOVInst, 
									 0 /* uLiveChansInInstDest */,
									 0 /* uRequireUnifiedStoreSrcMask */, 
									 IMG_FALSE /* bRequireNotUnifiedStoreSlot */,
									 IMG_FALSE /* bSetHintInternalRegReplacement */,
									 &sDummyMOVIRegAssignment,
									 &sDummyMOVInstData);
		ASSERT(bRet);

		/*
			Set up information about dual-issuing the two instructions.
		*/
		bRet = CanDualIssue(psState, &sInstData, &sDummyMOVInstData, &sSetup, IMG_FALSE, IMG_FALSE, &uExitHint);
		ASSERT(bRet);

		/*
			Create the dual-issue instruction.
		*/
		*ppsDualInst = CreateDualIssueInstruction(psState, 
												  psInst, 
												  psDummyMOVInst,
												  psIRegAssignment,
												  &sDummyMOVIRegAssignment,
												  &sInstData, 
												  &sDummyMOVInstData, 
												  IMG_FALSE /* bInst1IsDummy */,
												  IMG_TRUE /* bInst2IsDummy */,
												  &sSetup);

		/*
			Return a pointer to the destination of the dual-issue instruction which
			makes use of the spare internal register.
		*/
		if (sInstData.bRequiresUSDestination)
		{
			*ppsDummyIRegDest = &(*ppsDualInst)->asDest[VDUAL_DESTSLOT_GPI];
		}
		else
		{
			*ppsDummyIRegDest = NULL;
		}

		/*
			Free the source for the dummy co-issued operation.
		*/
		FreeInst(psState, psDummyMOVInst);
	}

	return IMG_TRUE;
}

#define DUAL_DEP_GRAPH_FLAGS \
		(DEP_GRAPH_COMPUTE_COLLECT_ARG_INFO | DEP_GRAPH_COMPUTE_DEP_PER_ARG | DEP_GRAPH_COMPUTE_COLLECT_OLD_DEST)

static
IMG_VOID GenerateVectorDualIssueBP(PINTERMEDIATE_STATE	psState,
								   PCODEBLOCK			psCodeBlock,
								   IMG_PVOID			pvNULL)
/*****************************************************************************
 FUNCTION	: GenerateVectorDualIssueBP
    
 PURPOSE	: Try and generate vector dual-issue instructions in a single
			  basic block.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Basic block.
			  pvNULL			- Ignored.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST			psInst;
	USC_LIST		sCandidateInstsList;
	IMG_UINT32		uCandidateCount;
	PUSC_LIST_ENTRY	psListEntry1;
	PUSC_LIST_ENTRY	psNextListEntry1;
	IMG_BOOL		bCreatedDualIssueInstructions;
	DUALISSUE_SETUP	sSetup;

	PVR_UNREFERENCED_PARAMETER(pvNULL);
	
	/*
		Create a linked list of all the instructions in the block which could be
		part of a dual-issued instruction.
	*/
	InitializeList(&sCandidateInstsList);
	uCandidateCount = 0;
	
	/*
		Calculate instruction dependency DAG.
	*/
	GenerateInstructionDAG(psState, psCodeBlock, DUAL_DEP_GRAPH_FLAGS);
	
	for (psInst = psCodeBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		PVEC_DUALISSUE		psVDIData;

		psInst->sStageData.psVDIData = NULL;
		
		psVDIData = UscAlloc(psState, sizeof(*psInst->sStageData.psVDIData));
		psInst->sStageData.psVDIData = psVDIData;
		
		memset(psVDIData, 0, sizeof(*psVDIData));
		
		//VecDualComputeInstructionSlidingInfo(psState, psCodeBlock, psInst);
		
		psInst->sStageData.psVDIData->bIsDeschedPoint = IsDeschedulingPoint(psState, psInst);
		/* Check if this instruction could be dual-issued. */
		psInst->sStageData.psVDIData->bIsPossibleDualOp	= IsPossibleDualIssueOp(psState, 
																				psInst, 
																				psInst->auLiveChansInDest[0],
																				0 /* uRequireUnifiedStoreSrcMask */, 
																				IMG_FALSE /* bRequireNotUnifiedStoreSlot */,
																				IMG_TRUE /* bSetHintInternalRegReplacement */,
																				NULL /* psIRegAssignment */,
																				&psInst->sStageData.psVDIData->sInstData);
		if(psInst->sStageData.psVDIData->bIsPossibleDualOp)
		{
			/*
				Add to the list of dual-issue candidate instructions.
			*/
			AppendToList(&sCandidateInstsList, &psVDIData->sCandidateInstsListEntry);
			uCandidateCount++;
		}
	}
	
	VecDualComputeInstructionSlidingInfoBP(psState, psCodeBlock, IMG_NULL, IMG_NULL);
	
#if defined(DEBUG_SLIDING)
	VecDualPrintSliddingInfo(psState, psCodeBlock);
#endif

	/*	
		Exit early if there are no possible dual-issue instructions.
	*/
	if (uCandidateCount <= 1)
	{
		goto CleanupGenerateVectorDualIssue;
	}

	/*
		Look at each pair of candidate instructions.
	*/
	bCreatedDualIssueInstructions = IMG_FALSE;
	sSetup.psMoveUndo = UscStackMake(psState, sizeof(IREG_UNDO));
	sSetup.ppuAvailableIReg = IMG_NULL;
	sSetup.psBlock = psCodeBlock;
	
	/* Compilation check VDUAL_CACHED_IREG_INFO_VALID flag shouldn't clash with IReg bit */
	ASSERT(!(VDUAL_CACHED_IREG_INFO_VALID & ((1UL << psState->uGPISizeInScalarRegs) - 1UL)));
	
	for (psListEntry1 = sCandidateInstsList.psHead; psListEntry1 != NULL; psListEntry1 = psNextListEntry1)
	{
		PVEC_DUALISSUE	psInst1Data = IMG_CONTAINING_RECORD(psListEntry1, PVEC_DUALISSUE, sCandidateInstsListEntry);
		PINST			psInst1 = psInst1Data->sInstData.psInst;
		PUSC_LIST_ENTRY	psListEntry2;
		PUSC_LIST_ENTRY	psNextListEntry2;

		psNextListEntry1 = psListEntry1->psNext;

		for (psListEntry2 = psListEntry1->psNext; psListEntry2 != NULL; psListEntry2 = psNextListEntry2)
		{
			IMG_UINT32		uExitHint;
			PVEC_DUALISSUE	psInst2Data = IMG_CONTAINING_RECORD(psListEntry2, PVEC_DUALISSUE, sCandidateInstsListEntry);
			PINST			psInst2 = psInst2Data->sInstData.psInst;
			
			psNextListEntry2 = psListEntry2->psNext;
			
			/*
				Check both instructions have the same predicate.
			*/
			if (!EqualPredicates(psInst1, psInst2))
			{
				continue;
			}

			/*
				Check if this pair of instructions can be legally coissued.
			*/
			if (CanDualIssue(psState, 
							 &psInst1Data->sInstData, 
							 &psInst2Data->sInstData, 
							 &sSetup,
							 IMG_TRUE /*bEnableSourceSubstitution*/,
							 IMG_TRUE /*bTrySwappingPair*/,
							 &uExitHint))
			{
				PINST		psDualInst;

				/* Move instructions if marked to move */
				VecDualUpdateSlidingInstructions(psState, psInst1->psBlock);
				VDUAL_TESTONLY_PRINT_INTERMEDIATE("Generate Vector Dual-Issue Instructions", "vecdual_sliding_update", IMG_TRUE);
				
				FreeInstructionDAG(psState, psCodeBlock);
				if(sSetup.ppuAvailableIReg)
				{
					VecDualFreeCachedIRegInfo(psState, &sSetup);
				}
				/*
					Create a new instruction which does the same calculation as
					both instructions simultaneously.
				*/
				psDualInst = CreateDualIssueInstruction(psState, 
														psInst1, 
														psInst2, 
														NULL /* psInst1IRegAssignment */,
														NULL /* psInst2IRegAssignment */,
														&psInst1Data->sInstData,
														&psInst2Data->sInstData, 
														IMG_FALSE /* bInst1IsDummy */,
														IMG_FALSE /* bInst2IsDummy */,
														&sSetup);

				psDualInst->sStageData.psVDIData = NULL;
				InsertInstBefore(psState, psCodeBlock, psDualInst, sSetup.psSiteInst);

				/*
					Remove both instructions from the list of dual-issue candidates.
				*/
				if (psNextListEntry1 == &psInst2Data->sCandidateInstsListEntry)
				{
					psNextListEntry1 = psNextListEntry1->psNext;
				}
				RemoveFromList(&sCandidateInstsList, &psInst1Data->sCandidateInstsListEntry);
				RemoveFromList(&sCandidateInstsList, &psInst2Data->sCandidateInstsListEntry);

				/*
					Free the instructions which were combined.
				*/
				UscFree(psState, psInst1->sStageData.psVDIData->ppsSlidingInfo);
				UscFree(psState, psInst1->sStageData.psVDIData);
				RemoveInst(psState, psCodeBlock, psInst1);
				FreeInst(psState, psInst1);
				UscFree(psState, psInst2->sStageData.psVDIData->ppsSlidingInfo);
				UscFree(psState, psInst2->sStageData.psVDIData);
				RemoveInst(psState, psCodeBlock, psInst2);
				FreeInst(psState, psInst2);

				bCreatedDualIssueInstructions = IMG_TRUE;
				
				GenerateInstructionDAG(psState, psCodeBlock, DUAL_DEP_GRAPH_FLAGS);
				VecDualComputeInstructionSlidingInfoBP(psState, psCodeBlock, IMG_NULL, IMG_NULL);

				VDUAL_TESTONLY_PRINT_INTERMEDIATE("Generate Vector Dual-Issue Instructions", "vecdual_after_adding_vdual", IMG_TRUE);
				break;
			}
#if 0
			else
			{
				DBG_PRINTF((DBG_WARNING, "PINST1: %s(%d|%d) PINST2: %s(%d|%d) :FAILED %d : %d",
											g_psInstDesc[psInst1->eOpcode].pszName, psInst1->uBlockIndex, psInst1Data->sInstData.bMayNeedInternalRegAssingment,
											g_psInstDesc[psInst2->eOpcode].pszName, psInst2->uBlockIndex, psInst2Data->sInstData.bMayNeedInternalRegAssingment,
											sSetup.uExitHintMax, uExitHint));
			}
#endif
		}
	}
	UscStackDelete(psState, sSetup.psMoveUndo);
	if(sSetup.ppuAvailableIReg)
	{
		VecDualFreeCachedIRegInfo(psState, &sSetup);
	}
	
CleanupGenerateVectorDualIssue:
	/* Release memory. */
	FreeInstructionDAG(psState, psCodeBlock);
	
	/*
		Free per-instruction stage specific data.
	*/
	for (psInst = psCodeBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		if (psInst->sStageData.psVDIData != NULL)
		{
			if(psInst->sStageData.psVDIData->ppsSlidingInfo)
			{
				UscFree(psState, psInst->sStageData.psVDIData->ppsSlidingInfo);
			}
			UscFree(psState, psInst->sStageData.psVDIData);
		}
	}
}

IMG_INTERNAL
IMG_VOID GenerateVectorDualIssue(PINTERMEDIATE_STATE	psState)
/*****************************************************************************
 FUNCTION	: GenerateVectorDualIssue
    
 PURPOSE	: Try and generate vector dual-issue instructions.

 PARAMETERS	: psState			- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		Try and generate vector dual-issue instructions in all blocks.
	*/
	DoOnAllBasicBlocks(psState, ANY_ORDER, GenerateVectorDualIssueBP, IMG_FALSE /* CALLS */, NULL /* pvUserData */);

	/*	
		Show the updated intermediate code.
	*/
	TESTONLY_PRINT_INTERMEDIATE("Generate Vector Dual-Issue Instructions", "vecdual", IMG_TRUE);
}

static IMG_BOOL CombineF16ImmediateSourceVectors(PINTERMEDIATE_STATE	psState, 
												 PINST					psDestInst, 
												 PINST					psFirstInst, 
												 PINST					psSecondInst)
/*****************************************************************************
 FUNCTION	: CombineF16ImmediateSourceVectors
    
 PURPOSE	: Check if two immediate source vectors of F16 scalar registers can be combined
			  into a single vector.

 PARAMETERS	: psState				- Compiler state.
			  psDestInst			- If non-NULL then the combined source vector
									is set as the first argument to this instruction.
			  psFirstInst			- First instruction.
			  psSecondInst			- Second instruction.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT16	auSrcData[VECTOR_LENGTH];
	IMG_UINT32	uCombinedMask;
	IMG_UINT32	uChan;

	uCombinedMask = psFirstInst->auDestMask[0] | psSecondInst->auDestMask[0];

	memset(auSrcData, 0, sizeof(auSrcData));
	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		PINST				psSrcInst;
		IMG_UINT32			uChanSrcData;

		/*
			Get the instruction supplying data for this channel.
		*/
		if ((psFirstInst->auDestMask[0] & (1 << uChan)) != 0)
		{
			psSrcInst = psFirstInst;
		}
		else if ((psSecondInst->auDestMask[0] & (1 << uChan)) != 0)
		{
			psSrcInst = psSecondInst;
		}
		else
		{
			continue;
		}

		if (GetImmediateVectorChan(psState, psSrcInst, 0 /* uSrcIdx */, uChan, &uChanSrcData))
		{
			auSrcData[uChan] = (IMG_UINT16)uChanSrcData;
		}
		else
		{
			return IMG_FALSE;
		}
	}

	if (psDestInst != NULL)
	{
		IMG_UINT32	uArg;
		IMG_UINT32	uConst;
		IMG_UINT32	uSwizzle;

		SetSrcUnused(psState, psDestInst, 0 /* uSrcIdx */);

		/*
			Check if the combined immediate vector is equal to a hardware constant (with some swizzle).
		*/
		if (FindSwizzledHardwareConstant(psState, 
										 (IMG_PUINT8)auSrcData, 
										 uCombinedMask, 
										 &uConst,
										 &uSwizzle, 
										 UF_REGFORMAT_F16))
		{
			for (uArg = 0; uArg < SCALAR_REGISTERS_PER_F16_VECTOR; uArg++)
			{
				SetSrc(psState, 
					   psDestInst,
					   1 + uArg,
					   USEASM_REGTYPE_FPCONSTANT, 
					   (uConst << SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT) + uArg, 
					   UF_REGFORMAT_F16);
			}
			psDestInst->u.psVec->auSwizzle[0] = uSwizzle;
		}
		else
		{
			for (uArg = 0; uArg < SCALAR_REGISTERS_PER_F16_VECTOR; uArg++)
			{
				SetSrc(psState, 
					   psDestInst, 
					   1 + uArg, 
					   USEASM_REGTYPE_IMMEDIATE, 
					   ((IMG_PUINT32)auSrcData)[uArg], 
					   UF_REGFORMAT_F16);
			}
			psDestInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
		}
		for (; uArg < VECTOR_LENGTH; uArg++)
		{
			SetSrcUnused(psState, psDestInst, 1 + uArg);
		}
	}

	return IMG_TRUE;
}

static IMG_BOOL IsCompatibleF16SourceVectors(PINTERMEDIATE_STATE psState, PINST psDestInst, PINST psFirstInst, PINST psSecondInst)
/*****************************************************************************
 FUNCTION	: IsCompatibleF16SourceVectors
    
 PURPOSE	: Check if two source vectors of F16 scalar registers can be combined
			  into a single vector.

 PARAMETERS	: psState				- Compiler state.
			  psDestInst			- If non-NULL then the combined source vector
									is set as the first argument to this instruction.
			  psFirstInst			- First instruction.
			  psSecondInst			- Second instruction.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uChan;
	ARG			asSrcVector[SCALAR_REGISTERS_PER_F16_VECTOR];
	IMG_UINT32	uNewSwizzle;
	IMG_UINT32	uExistingArgCount;

	/*	
		Check for the special case where both vectors are immediates.
	*/
	if (CombineF16ImmediateSourceVectors(psState, psDestInst, psFirstInst, psSecondInst))
	{
		return IMG_TRUE;
	}

	/*
		Count the number of unique arguments in the combined vector.
	*/
	uNewSwizzle = USEASM_SWIZZLE(0, 0, 0, 0);
	uExistingArgCount = 0;
	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		PINST				psSrcInst;
		USEASM_INTSRCSEL	eSrcSel;

		/*
			Get the instruction which is the source for this channel.
		*/
		if ((psFirstInst->auDestMask[0] & (1 << uChan)) != 0)
		{
			psSrcInst = psFirstInst;
		}
		else if ((psSecondInst->auDestMask[0] & (1 << uChan)) != 0)
		{
			psSrcInst = psSecondInst;
		}
		else
		{
			continue;
		}

		eSrcSel = GetChan(psSrcInst->u.psVec->auSwizzle[0], uChan);

		ASSERT(eSrcSel < (sizeof(g_asSwizzleSel) / sizeof(g_asSwizzleSel[0])));
		if (!g_asSwizzleSel[eSrcSel].bIsConstant)
		{
			IMG_UINT32	uSrcChan = g_asSwizzleSel[eSrcSel].uSrcChan;
			PARG		psSrcArg = &psSrcInst->asArg[1 + (uSrcChan / F16_CHANNELS_PER_SCALAR_REGISTER)];
			IMG_UINT32	uExistingArg;
			IMG_UINT32	uNewSrcChan;

			/*
				Check if we've already seen this argument.
			*/
			for (uExistingArg = 0; uExistingArg < uExistingArgCount; uExistingArg++)
			{
				if (EqualArgs(&asSrcVector[uExistingArg], psSrcArg))
				{
					break;
				}
			}

			if (uExistingArg == uExistingArgCount)
			{
				/*
					Check we aren't accessing more than 2 32-bit registers.
				*/
				if (uExistingArgCount == SCALAR_REGISTERS_PER_F16_VECTOR)
				{
					return IMG_FALSE;
				}
				asSrcVector[uExistingArgCount++] = *psSrcArg;
			}

			/*
				Create a new swizzle selector for this source channel from the position of the argument
				in the combined argument list.
			*/
			uNewSrcChan = uExistingArg * F16_CHANNELS_PER_SCALAR_REGISTER + (uSrcChan % F16_CHANNELS_PER_SCALAR_REGISTER);
			ASSERT(uNewSrcChan < VECTOR_LENGTH);

			/*
				Update the swizzle for the combined instruction for the new swizzle selector.
			*/
			uNewSwizzle = SetChan(uNewSwizzle, uChan, g_aeChanToSwizzleSel[uNewSrcChan]);
		}	
	}

	if (psDestInst != NULL)
	{
		IMG_UINT32	uArg;

		/*
			Copy the combined argument list 
		*/
		SetSrcUnused(psState, psDestInst, 0 /* uSrcIdx */);
		for (uArg = 0; uArg < uExistingArgCount; uArg++)
		{
			SetSrcFromArg(psState, psDestInst, 1 + uArg, &asSrcVector[uArg]);
		}
		for (; uArg < VECTOR_LENGTH; uArg++)
		{
			SetSrcUnused(psState, psDestInst, 1 + uArg);
		}

		/*
			Set the combined instruction's source swizzle.
		*/
		psDestInst->u.psVec->auSwizzle[0] = uNewSwizzle;
	}

	return IMG_TRUE;
}

static IMG_BOOL FindOtherVMOVInstruction(PINTERMEDIATE_STATE	psState,
										 PINST					psFirstInst,
										 PINST*					ppsSecondInst)
/*****************************************************************************
 FUNCTION	: FindOtherVMOVInstruction
    
 PURPOSE	: Look for another VMOV instruction connected to the first and where
			  there are no dependencies preventing the first from moving
			  forward to the location of the second.

 PARAMETERS	: psState				- Compiler state.
			  psFirstInst			- First instruction.
			  ppsSecondInst			- Returns the second instruction.

 RETURNS	: TRUE or FALSE whether it found a matching instruction.
*****************************************************************************/
{
	PINST			psPotentialSecondInst = NULL;
	USEDEF_TYPE		eUseType;
	IMG_UINT32		uDestIdx;
	PARG			psFirstDest = &psFirstInst->asDest[0];

	ASSERT(psFirstInst->uDestCount == 1);

	/*
		Check the result of the first instruction is used only once.
	*/
	if (!UseDefGetSingleUse(psState, psFirstDest, &psPotentialSecondInst, &eUseType, &uDestIdx))
	{
		return IMG_FALSE;
	}

	/*
		Check the result of the first instruction is copied into the result of another
		instruction.
	*/
	if (eUseType != USE_TYPE_OLDDEST)
	{
		return IMG_FALSE;
	}

	/*
		Check both instructions are vector moves in the same block.
	*/
	if (psPotentialSecondInst->eOpcode		!= IVMOV ||
		psPotentialSecondInst->psBlock		!= psFirstInst->psBlock ||
		!NoPredicate(psState, psPotentialSecondInst) ||
		psFirstInst->asArg[0].uType			!= psPotentialSecondInst->asArg[0].uType ||
		(psFirstInst->auDestMask[0] & psPotentialSecondInst->auDestMask[0]) != 0 ||
		psFirstInst->u.psVec->aeSrcFmt[0]	!= psPotentialSecondInst->u.psVec->aeSrcFmt[0])
	{
		return IMG_FALSE;
	}

	/*
		Check both VMOV instructions have the same abs source modifier. The negate modifiers may differ,
		in that case the result is a mul.
	*/
	if (psFirstInst->u.psVec->asSrcMod[0].bAbs != psPotentialSecondInst->u.psVec->asSrcMod[0].bAbs)
	{
		return IMG_FALSE;
	}
		
	if (psFirstInst->asArg[0].uType != USC_REGTYPE_UNUSEDSOURCE)
	{
		/* 
			If the first argument (vector register) is used, as opposed to 2nd-5th, then the registers
			must match, because we can't pick & mix 
		*/
		if (psFirstInst->asArg[0].uNumber	!= psPotentialSecondInst->asArg[0].uNumber)
		{
			return IMG_FALSE;
		}
	}
	else
	{
		/*
			If the two instructions are accessing vectors of separate scalar instructions check that the
			number of scalar registers in the combined vector fits in the limitations of the hardware.
		*/
		if (psFirstInst->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F16)
		{
			if (!IsCompatibleF16SourceVectors(psState, NULL /* psDestInst */, psFirstInst, psPotentialSecondInst))
			{
				return IMG_FALSE;
			}
		}
	}

	*ppsSecondInst = psPotentialSecondInst;
	return IMG_TRUE;
}


static
PINST CombineMOVInstructions(PINTERMEDIATE_STATE	psState,
							 PINST					psFirstInst,
							 PINST					psSecondInst)
/*****************************************************************************
 FUNCTION	: CombineMOVInstructions
    
 PURPOSE	: Combine two VMOV instructions into one, where one destination is the
			  other's old dest. Ie
			  
			  MOV r0 r1
			  MOV r2(=r0) r2

			  Doesn't delete the old instructions, that needs to be done by the caller.
			  If the negate modifiers differ, replace both instructions with a mul.

 PARAMETERS	: psState				- Compiler state.
			  psFirstInst			- First instruction.
			  ppsSecondInst			- Returns the second instruction.

 RETURNS	: The new, combined, MOV instruction.
*****************************************************************************/
{
	IMG_UINT32	uNumComponents;
	IMG_UINT32	uChanIdx;
	IMG_UINT32	uEffectiveArgs;
	PINST		psCombinedMov;
	IMG_BOOL	bMakeMul = (psFirstInst->u.psVec->asSrcMod[0].bNegate != psSecondInst->u.psVec->asSrcMod[0].bNegate);

	ASSERT(psFirstInst->psBlock == psSecondInst->psBlock);
	
	if (psFirstInst->asArg[0].uType != USC_REGTYPE_UNUSEDSOURCE)
	{
		ASSERT(psSecondInst->asArg[0].uType		!= USC_REGTYPE_UNUSEDSOURCE);
		ASSERT(psSecondInst->asArg[0].uNumber	== psFirstInst->asArg[0].uNumber);
		uEffectiveArgs = 1;
	}
	else 
	{
		ASSERT(psSecondInst->asArg[0].uType == USC_REGTYPE_UNUSEDSOURCE);
		uEffectiveArgs = 4;
	}

	psCombinedMov = CopyInst(psState, psSecondInst);
	
	if (bMakeMul)
	{
		ModifyOpcode(psState, psCombinedMov, IVMUL);
	}

	MoveDest(psState, psCombinedMov, 0, psSecondInst, 0);
	psCombinedMov->auLiveChansInDest[0]	= psSecondInst->auLiveChansInDest[0];
	psCombinedMov->auDestMask[0]		= psSecondInst->auDestMask[0];

	CopyPartiallyWrittenDest(psState, psCombinedMov, 0 /* uCopyToDestIdx */, psFirstInst, 0 /* uCopyFromDestIdx */);
	psCombinedMov->auDestMask[0] |= psFirstInst->auDestMask[0];

	if (uEffectiveArgs == 1) {
		/* Set the source swizzle to be a combination of both source swizzles,
		according to the dest swizzles. */
		for (uChanIdx = 0; uChanIdx < MAX_SCALAR_REGISTERS_PER_VECTOR; uChanIdx++)
		{
			if ((psFirstInst->auDestMask[0] & (1 << uChanIdx)) != 0)
			{
				IMG_UINT32 uSel = GetChan(psFirstInst->u.psVec->auSwizzle[0], uChanIdx);
				psCombinedMov->u.psVec->auSwizzle[0] = SetChan(psCombinedMov->u.psVec->auSwizzle[0], uChanIdx, uSel);
			}
			else
			{
				IMG_UINT32 uSel = GetChan(psSecondInst->u.psVec->auSwizzle[0], uChanIdx);
				psCombinedMov->u.psVec->auSwizzle[0] = SetChan(psCombinedMov->u.psVec->auSwizzle[0], uChanIdx, uSel);
			}
		}
	}
	else
	{
		psCombinedMov->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);

		/* Pick the elements of the source according to the mask on the dest */
		if (psFirstInst->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F32)
		{
			IMG_UINT32	uChan;

			ASSERT(psSecondInst->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F32);
			uNumComponents = 4;

			SetSrcUnused(psState, psCombinedMov, 0 /* uSrcIdx */);
			for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
			{
				PINST				psSrcInst;
				USEASM_SWIZZLE_SEL	eSel;

				if ((psFirstInst->auDestMask[0] & (1 << uChan)) != 0)
				{
					psSrcInst = psFirstInst;
				}
				else
				{
					psSrcInst = psSecondInst; 
				}

				eSel = GetChan(psSrcInst->u.psVec->auSwizzle[0], uChan);
				if (g_asSwizzleSel[eSel].bIsConstant)
				{
					psCombinedMov->u.psVec->auSwizzle[0] = SetChan(psCombinedMov->u.psVec->auSwizzle[0], uChan, eSel);
					SetSrcFromArg(psState, psCombinedMov, 1 + uChan, &psSrcInst->asArg[uChan]); 
				}
				else
				{
					IMG_UINT32	uSrcChan;

					uSrcChan = g_asSwizzleSel[eSel].uSrcChan;
					SetSrcFromArg(psState, psCombinedMov, 1 + uChan, &psSrcInst->asArg[1 + uSrcChan]);
				}
			}
		}
		else
		{
			IMG_BOOL	bRet;

			ASSERT(psFirstInst->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F16);
			ASSERT(psSecondInst->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F16);

			bRet = IsCompatibleF16SourceVectors(psState, psCombinedMov, psFirstInst, psSecondInst);
			ASSERT(bRet);
		}
	}

	InsertInstAfter(psState, psSecondInst->psBlock, psCombinedMov, psSecondInst);

	/*
		If the negate modifiers on the two move instructions differ then insert a new instruction to create
		a vector of 1s and -1s to apply different negate modifiers to different channels in the combined instructions
		e.g.

		VMOV	A.xy, B
		VMOV	C.zw[=A], B_Neg
			->
		VMOV	T1, {1, 1, 1, 1}
		VMUL	T2.zw[=T1], T1_neg, {1, 1, 1, 1}	// (T2 = {1, 1, -1, -1}
		VMUL	C, B, T2							// C = {1 * B.X, 1 * B.Y, -1 * B.Z, -1 * B.W}
												    // C = {B.X, B.Y, -B.Z, -B.W}
	*/
	if (bMakeMul)
	{
		PINST		psConstSetUpInst, psConstModifyInst;
		IMG_UINT32	uTemp, uConstOne;
		IMG_UINT32	uOneMask, uMinusOneMask;

		if (psCombinedMov->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F32)
		{
			uConstOne = FLOAT32_ONE;
		}
		else
		{
			uConstOne = FLOAT16_ONE_REPLICATED;
		}

		if (psFirstInst->u.psVec->asSrcMod[0].bNegate)
		{
			ASSERT(!psSecondInst->u.psVec->asSrcMod[0].bNegate);

			uMinusOneMask = psFirstInst->auDestMask[0];
			uOneMask = psSecondInst->auDestMask[0];
		}
		else
		{
			ASSERT(psSecondInst->u.psVec->asSrcMod[0].bNegate);

			uMinusOneMask = psSecondInst->auDestMask[0];
			uOneMask = psFirstInst->auDestMask[0];
		}
		ASSERT((uMinusOneMask & uOneMask) == 0);
		ASSERT(uOneMask != 0);
		ASSERT(uMinusOneMask != 0);

		uTemp = GetNextRegister(psState);
		
		psConstSetUpInst = AllocateInst(psState, psCombinedMov);
		SetOpcode(psState, psConstSetUpInst, IVMOV);

		/* Set all channels to one */
		SetDest(psState, psConstSetUpInst, 0, USEASM_REGTYPE_TEMP, uTemp, psCombinedMov->u.psVec->aeSrcFmt[0]);
		psConstSetUpInst->auDestMask[0] = uOneMask | uMinusOneMask;
		psConstSetUpInst->auLiveChansInDest[0] = uOneMask | uMinusOneMask;

		psConstSetUpInst->u.psVec->aeSrcFmt[0] = psCombinedMov->u.psVec->aeSrcFmt[0];
		psConstSetUpInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
		
		SetupImmediateSourceU(psConstSetUpInst, 0 /* uArgIdx */, uConstOne, psCombinedMov->u.psVec->aeSrcFmt[0]);
		
		InsertInstBefore(psState, psCombinedMov->psBlock, psConstSetUpInst, psCombinedMov);

		/* Multiply those channels that require -1 */
		psConstModifyInst = AllocateInst(psState, psCombinedMov);
		SetOpcode(psState, psConstModifyInst, IVMUL);
		
		SetSrc(psState,
			   psConstModifyInst, 
			   0,
			   USEASM_REGTYPE_TEMP,
			   uTemp,
			   psCombinedMov->u.psVec->aeSrcFmt[0]);
		
		uTemp = GetNextRegister(psState);
		SetDest(psState, psConstModifyInst, 0, USEASM_REGTYPE_TEMP, uTemp, psCombinedMov->u.psVec->aeSrcFmt[0]);

		psConstModifyInst->u.psVec->aeSrcFmt[0] = psCombinedMov->u.psVec->aeSrcFmt[0];
		psConstModifyInst->u.psVec->aeSrcFmt[1] = psCombinedMov->u.psVec->aeSrcFmt[0];
		psConstModifyInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
		psConstModifyInst->u.psVec->auSwizzle[1] = USEASM_SWIZZLE(X, Y, Z, W);
		psConstModifyInst->u.psVec->asSrcMod[0].bNegate = IMG_TRUE;
		psConstModifyInst->u.psVec->asSrcMod[1].bNegate = IMG_FALSE;
		SetPartiallyWrittenDest(psState, psConstModifyInst, 0, &psConstSetUpInst->asDest[0]);
		
		SetupImmediateSourceU(psConstModifyInst, 1 /* uArgIdx */, uConstOne, psCombinedMov->u.psVec->aeSrcFmt[0]);

		psConstModifyInst->auDestMask[0] = uMinusOneMask;
		psConstModifyInst->auLiveChansInDest[0] = uOneMask | uMinusOneMask;
		
		InsertInstAfter(psState, psConstSetUpInst->psBlock, psConstModifyInst, psConstSetUpInst);

		/*
			Set the vector of 1s/-1s as the second source to the combined move instruction.
		*/
		SetSrc(psState,
			   psCombinedMov, 
			   MAX_SCALAR_REGISTERS_PER_VECTOR + 1,
			   USEASM_REGTYPE_TEMP,
			   uTemp,
			   psConstModifyInst->asDest[0].eFmt);

		psCombinedMov->u.psVec->aeSrcFmt[1] = psCombinedMov->u.psVec->aeSrcFmt[0];
		psCombinedMov->u.psVec->auSwizzle[1] = USEASM_SWIZZLE(X, Y, Z, W);
		psCombinedMov->u.psVec->asSrcMod[0].bNegate = IMG_FALSE;
	}

	return psCombinedMov;
}


IMG_INTERNAL
IMG_BOOL CombineVMOVs(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: CombineVMOVs
    
 PURPOSE	: Try to combine two VMOV instructions where one destination is the
			  other's old dest. Ie
			  
			  MOV r0 r1
			  MOV r2(=r0) r2

 PARAMETERS	: psState			- Compiler state.

 RETURNS	: TRUE if any work was done.
*****************************************************************************/
{
	IMG_BOOL			bDoneSomething = IMG_FALSE;
	SAFE_LIST_ITERATOR	sIter;

	InstListIteratorInitialize(psState, IVMOV, &sIter);
	for (; InstListIteratorContinue(&sIter);)
	{
		PINST psFirstInst = InstListIteratorCurrent(&sIter);

		if (NoPredicate(psState, psFirstInst))
		{
			PINST psSecondInst;
			if (FindOtherVMOVInstruction(psState, psFirstInst, &psSecondInst))
			{
				CombineMOVInstructions(psState, psFirstInst, psSecondInst);

				RemoveInst(psState, psFirstInst->psBlock, psFirstInst);
				FreeInst(psState, psFirstInst);
				RemoveInst(psState, psSecondInst->psBlock, psSecondInst);
				FreeInst(psState, psSecondInst);
				
				bDoneSomething = IMG_TRUE;

				/* Start from the beginning */
				InstListIteratorFinalise(&sIter);
				InstListIteratorInitialize(psState, IVMOV, &sIter);

				continue;
			}
		} 
		InstListIteratorNext(&sIter);
	}
	InstListIteratorFinalise(&sIter);

	TESTONLY_PRINT_INTERMEDIATE("Combine VMOV instructions", "VMOVCombination", IMG_FALSE);
	return bDoneSomething;
}

static
IMG_BOOL ReplaceMOVWithVMOV(PINTERMEDIATE_STATE psState, PINST psInst, IMG_BOOL bCheckOnly)
/*****************************************************************************
 FUNCTION	: ReplaceMOVWithVMOV
    
 PURPOSE	: Check whether a MOV can be replaced with a VMOV, optionally doing the replacement.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction whose desination is to be checked.
			  bCheckOnly	- If TRUE, don't do the replacement, just check whether it's possible.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psUseListEntry;
	PUSEDEF_CHAIN	psUseDef = UseDefGet(psState, psInst->asDest[0].uType, psInst->asDest[0].uNumber);
	IMG_UINT32		uVecDest = 0;
	
	if (!bCheckOnly)
	{
		SetOpcode(psState, psInst, IVMOV);

		uVecDest = GetNextRegister(psState);
		SetDest(psState, psInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uVecDest, psInst->asDest[0].eFmt);

		MoveSrc(psState, psInst, 1 /* uMoveToSrcIdx */, psInst, 0 /* uMoveFromDestIdx */);
		SetSrcUnused(psState, psInst, 0);
		SetSrcUnused(psState, psInst, 2);
		SetSrcUnused(psState, psInst, 3);
		SetSrcUnused(psState, psInst, 4);

		psInst->auDestMask[0] = USC_X_CHAN_MASK;
		psInst->auLiveChansInDest[0] = USC_X_CHAN_MASK;
		psInst->u.psVec->aeSrcFmt[0] = psInst->asArg[0].eFmt;
		psInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);

		if (psInst->apsOldDest[0] != NULL)
		{
			PINST		psOldDestDefInst;
			IMG_UINT32	uVecOldDest = GetNextRegister(psState);
			PINST		psMoveInst = AllocateInst(psState, psInst);
			
			SetOpcode(psState, psMoveInst, IVMOV);
			
			psOldDestDefInst = UseDefGetDefInst(psState, psInst->apsOldDest[0]->uType, psInst->apsOldDest[0]->uNumber, NULL /* puDestIdx */);
			CopyPredicate(psState, psMoveInst, psOldDestDefInst);

			psMoveInst->u.psVec->aeSrcFmt[0] = psInst->asDest[0].eFmt;

			SetSrcFromArg(psState, psMoveInst, 1, psInst->apsOldDest[0]);
			SetSrcUnused(psState, psMoveInst, 0);
			SetSrcUnused(psState, psMoveInst, 2);
			SetSrcUnused(psState, psMoveInst, 3);
			SetSrcUnused(psState, psMoveInst, 4);

			SetDest(psState, psMoveInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uVecOldDest, psInst->asDest[0].eFmt);

			psMoveInst->auDestMask[0] = USC_X_CHAN_MASK;
			psMoveInst->auLiveChansInDest[0] = USC_X_CHAN_MASK;

			psMoveInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
			
			InsertInstBefore(psState, psInst->psBlock, psMoveInst, psInst);

			SetPartiallyWrittenDest(psState, psInst, 0 /* uDestIdx */, &psMoveInst->asDest[0]);
		}
	}

	for (psUseListEntry = psUseDef->sList.psHead; 
		psUseListEntry != NULL;
		psUseListEntry = psUseListEntry->psNext)
	{
		PUSEDEF		psUse;
		PINST		psUseInst;

		psUse = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);
		psUseInst = psUse->u.psInst;

		if (psUse == psUseDef->psDef)
		{
			continue;
		}

		if (psUse->eType != USE_TYPE_SRC)
		{
			return IMG_FALSE;
		}

		if ((g_psInstDesc[psUseInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) == 0)
		{
			return IMG_FALSE;
		}

		if (!bCheckOnly)
		{
			IMG_UINT32	uChan;
			IMG_UINT32	uVecTemp;
			PINST		psMoveInst;
			IMG_BOOL	bSingleElmtVec = IMG_TRUE;

			for (uChan = 0; uChan < CHANS_PER_REGISTER; ++uChan)
			{
				if (uChan + 1 != psUse->uLocation%CHANS_PER_REGISTER &&
					psUseInst->asArg[uChan + 1].uType != USC_REGTYPE_UNUSEDSOURCE)
				{
					bSingleElmtVec = IMG_FALSE;
					break;
				}
			}

			if (!bSingleElmtVec)
			{
				psMoveInst = AllocateInst(psState, psUseInst);
				SetOpcode(psState, psMoveInst, IVMOV);

				psMoveInst->u.psVec->aeSrcFmt[0] = psUseInst->u.psVec->aeSrcFmt[psUse->uLocation/CHANS_PER_REGISTER];

				psMoveInst->auDestMask[0] = 0;

				for (uChan = 0; uChan < CHANS_PER_REGISTER+1; ++uChan)
				{
					if (uChan != psUse->uLocation%CHANS_PER_REGISTER)
					{
						CopySrc(psState, psMoveInst, uChan, psUseInst, psUse->uLocation/CHANS_PER_REGISTER + uChan);

						if (uChan != 0)
						{
							psMoveInst->auDestMask[0] |= (1 << (uChan-1));
						}
					}
					else
					{
						SetSrcUnused(psState, psMoveInst, uChan);
					}
				}

				uVecTemp = GetNextRegister(psState);

				SetDest(psState, psMoveInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uVecTemp, psMoveInst->u.psVec->aeSrcFmt[0]);

				psMoveInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);

				SetPartialDest(psState, psMoveInst, 0, USEASM_REGTYPE_TEMP, uVecDest, psMoveInst->u.psVec->aeSrcFmt[0]);

				CopyPredicate(psState, psMoveInst, psUseInst);

				InsertInstBefore(psState, psUseInst->psBlock, psMoveInst, psUseInst);

				SetSrcFromArg(psState, psUseInst, psUse->uLocation/CHANS_PER_REGISTER, &psMoveInst->asDest[0]);
			}
			else
			{
				SetSrc(psState, psUseInst, psUse->uLocation/CHANS_PER_REGISTER, USEASM_REGTYPE_TEMP, uVecDest, psUseInst->asArg[psUse->uLocation].eFmt);
			}

			SetSrcUnused(psState, psUseInst, 1);
			SetSrcUnused(psState, psUseInst, 2);
			SetSrcUnused(psState, psUseInst, 3);
			SetSrcUnused(psState, psUseInst, 4);
		}
	}

	return IMG_TRUE;
}

IMG_INTERNAL
IMG_VOID ReplaceMOVsWithVMOVs(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: ReplaceMOVsWithVMOVs
    
 PURPOSE	: Try to combine MOV instructions to VMOV instructions.

 PARAMETERS	: psState - Compiler state.

 RETURNS	: Nothing
*****************************************************************************/
{
	IMG_BOOL			bDoneSomething = IMG_TRUE;
	SAFE_LIST_ITERATOR	sIter;

	while (bDoneSomething)
	{
		bDoneSomething = IMG_FALSE;

		InstListIteratorInitialize(psState, IMOV, &sIter);
		for (; InstListIteratorContinue(&sIter);)
		{
			PINST psInst = InstListIteratorCurrent(&sIter);

			if (ReplaceMOVWithVMOV(psState, psInst, IMG_TRUE))
			{
				ReplaceMOVWithVMOV(psState, psInst, IMG_FALSE);
				bDoneSomething = IMG_TRUE;
			}

			InstListIteratorNext(&sIter);
		}
		InstListIteratorFinalise(&sIter);

		if (bDoneSomething)
		{
			EliminateMoves(psState);
		}
	}

	TESTONLY_PRINT_INTERMEDIATE("Replace MOVs with VMOVs", "MOVtoVMOV", IMG_FALSE);
}

static
IMG_VOID GetNumberOfChannelsWritten(PINST psInst, IMG_UINT32 uWrittenChanCount[VECTOR_MAX_DESTINATION_COUNT])
/*****************************************************************************
 FUNCTION	: GetNumberOfChannelsWritten
    
 PURPOSE	: Get the number of channels that are written by a given instruction
			  to each of its destinations

 PARAMETERS	: psInst			- Instruction to check
			  uWrittenChanCount	- Will be used to return the number of
								  channels written to each destination.

 RETURNS	: Nothing
*****************************************************************************/
{
	IMG_UINT32	uDest, uChan;
	for (uDest = 0; uDest < psInst->uDestCount; ++uDest)
	{
		uWrittenChanCount[uDest] = 0;
		if (psInst->apsOldDest[0] != NULL && psInst->uPredCount == 0)
		{
			for (uChan = 0; uChan < VECTOR_LENGTH; ++uChan)
			{
				if (psInst->auLiveChansInDest[uDest] & (1 << uChan))
				{
					++uWrittenChanCount[uDest];
				}
			}
		}
		else
		{
			for (uChan = 0; uChan < VECTOR_LENGTH; ++uChan)
			{
				if (psInst->auDestMask[uDest] & (1 << uChan))
				{
					++uWrittenChanCount[uDest];
				}
			}
		}
	}
}

static 
IMG_UINT32 GetNumberArgsDifferent(PINTERMEDIATE_STATE	psState,
								  PINST					psInst, 
								  PINST					psInst2, 
								  IMG_UINT32*			puNumArgsDifferent, 
								  IMG_BOOL				bTrySwappingArgs)
/*****************************************************************************
 FUNCTION	: GetNumberArgsDifferent
    
 PURPOSE	: Get the number of arguments that are not the same when comparing
			  two instructions.

 PARAMETERS	: psState				- Compiler state.
			  psInst				- Instruction to check
			  psInst2				- Instruction to check
			  puNumArgsDifferent	- Used to return the number of arguments that
									  are different
			  bTrySwappingArgs		- Whether to try swapping the arguments of Inst2

 RETURNS	: The (vector) argument that is different, IF there is precisely one.
*****************************************************************************/
{
	IMG_UINT32 uSrc, uArgDifferent;
	
	ASSERT(psInst->uArgumentCount == psInst2->uArgumentCount);

	uArgDifferent = psInst->uArgumentCount;
	*puNumArgsDifferent = 0;

	for (uSrc = 0; uSrc < psInst->uArgumentCount; ++uSrc)
	{
		if ((psInst->asArg[uSrc].uType	!= USC_REGTYPE_UNUSEDSOURCE) ||
			(psInst2->asArg[uSrc].uType != USC_REGTYPE_UNUSEDSOURCE))
		{
			if ((GetLiveChansInArg(psState, psInst, uSrc) != 0) &&
				(GetLiveChansInArg(psState, psInst2, uSrc) != 0))
			{
				if (!EqualArgs(&psInst->asArg[uSrc], &psInst2->asArg[uSrc]) ||
					psInst->asArg[uSrc].eFmt != psInst2->asArg[uSrc].eFmt)
				{
					if (uArgDifferent == psInst->uArgumentCount || 
						(uSrc / SOURCE_ARGUMENTS_PER_VECTOR) != (uArgDifferent / SOURCE_ARGUMENTS_PER_VECTOR))
					{
						uArgDifferent = uSrc;
						++(*puNumArgsDifferent);
					}
				}
			}
			else
			{
				/* Allow arguments to mix & match if they're HW registers. */
				if (((GetLiveChansInArg(psState, psInst, uSrc) != 0) &&
					(psInst->asArg[uSrc].uType != USEASM_REGTYPE_IMMEDIATE)) ||
					((GetLiveChansInArg(psState, psInst2, uSrc) != 0) &&
					(psInst2->asArg[uSrc].uType != USEASM_REGTYPE_IMMEDIATE)))
				{
					if (uArgDifferent == psInst->uArgumentCount ||
						(uSrc / SOURCE_ARGUMENTS_PER_VECTOR) != (uArgDifferent / SOURCE_ARGUMENTS_PER_VECTOR))
					{
						uArgDifferent = uSrc;
						++(*puNumArgsDifferent);
					}
				}
			}
		}
	}

	if (bTrySwappingArgs && (*puNumArgsDifferent > 1))
	{
		/* Try swapping the sources of the second instruction. */
		if (psInst->uArgumentCount >= 10)
		{
			if (VectorSources12Commute(psInst2))
			{
				if (TrySwapVectorSources(psState, psInst2, 0 /* uArg1 */, 1 /* uArg2 */, IMG_FALSE /* bCheckSwizzles */))
				{
					return GetNumberArgsDifferent(psState, psInst, psInst2, puNumArgsDifferent, IMG_FALSE);
				}
			}
		}
	}

	return uArgDifferent / SOURCE_ARGUMENTS_PER_VECTOR;
}

static
IMG_BOOL PredicatesVectorisable(PINTERMEDIATE_STATE psState, PINST psInst1, PINST psInst2)
/*****************************************************************************
 FUNCTION	: PredicatesVectorisable
    
 PURPOSE	: Checks whether two instructions are vectorisable according to their predicates.
			  This is true if the predicates are both NULL, or equal, or if they
			  are defined by a "perchan" TEST instruction.
			  
			  IVTEST P0+P1+P2+P3 r0 r1
			  p0 ADD r0 r1.x r2.y
			  p1 ADD r3 r1.y r4.w

 PARAMETERS	: psState	- Compiler state.
			  psInst1	- The first instruction (with lower predicate register number)
			  psInst2	- The second instruction (with higher predicate register number)

 RETURNS	: If the instructions can be vectorised.
*****************************************************************************/
{
	IMG_UINT32  uInst1PredNum, uInst2PredNum;
	IMG_BOOL	bInst1PredNeg, bInst2PredNeg;
	IMG_BOOL	bInst1PredPerChan, bInst2PredPerChan;
	IMG_UINT32	uDestId1, uDestId2;
	PINST		psDefInst1, psDefInst2;
	IMG_UINT32	uChan, uPred, uPred2;
	IMG_BOOL	bUnused;

	if (psInst1->uPredCount == 0 || psInst2->uPredCount == 0)
	{
		return (psInst1->uPredCount == psInst2->uPredCount);
	}

	bInst1PredPerChan = GetBit(psInst1->auFlag, INST_PRED_PERCHAN);
	bInst2PredPerChan = GetBit(psInst2->auFlag, INST_PRED_PERCHAN);
	
	/* Ensure only one channel is written. */
	if (!bInst1PredPerChan)
	{
		IMG_UINT32 uWrittenChanCount[VECTOR_MAX_DESTINATION_COUNT];

		GetNumberOfChannelsWritten(psInst1, uWrittenChanCount);
		if (uWrittenChanCount[0] > 1)
		{
			return IMG_FALSE;
		}
		
		GetNumberOfChannelsWritten(psInst2, uWrittenChanCount);
		if (uWrittenChanCount[0] > 1)
		{
			return IMG_FALSE;
		}
	}

	for (uChan = 0; uChan < VECTOR_LENGTH; ++uChan)
	{
		uInst1PredNum = USC_PREDREG_NONE;
		bInst1PredNeg = IMG_FALSE;
		uInst2PredNum = USC_PREDREG_NONE;
		bInst2PredNeg = IMG_FALSE;

		if (uChan == 0 || bInst1PredPerChan)
		{
			GetPredicate(psInst1, &uInst1PredNum, &bInst1PredNeg, &bUnused, uChan);
		}
		if (uChan == 0 || bInst2PredPerChan)
		{
			GetPredicate(psInst2, &uInst2PredNum, &bInst2PredNeg, &bUnused, uChan);
		}

		psDefInst1 = NULL;
		psDefInst2 = NULL;
		uDestId1 = 0;
		uDestId2 = 0;

		if (uInst1PredNum != USC_PREDREG_NONE)
		{
			psDefInst1 = UseDefGetDefInstFromChain(psInst1->apsPredSrcUseDef[uChan]->psUseDefChain, &uDestId1);
			ASSERT(psDefInst1 != NULL);
		}
		if (uInst2PredNum != USC_PREDREG_NONE)
		{
			psDefInst2 = UseDefGetDefInstFromChain(psInst2->apsPredSrcUseDef[uChan]->psUseDefChain, &uDestId2);
			ASSERT(psDefInst2 != NULL);
		}

		if (psDefInst1 != NULL && psDefInst2 != NULL)
		{
			if (psDefInst1 != psDefInst2 || uDestId1 > uDestId2 || bInst1PredNeg != bInst2PredNeg)
			{
				return IMG_FALSE;
			}
		}
	}
	
	/* Ensure the predicates used are not overlapping */
	for (uPred = 0; uPred < psInst1->uPredCount; ++uPred)
	{
		IMG_BOOL bPerChan1, bPerChan2;

		GetPredicate(psInst1, &uInst1PredNum, &bInst1PredNeg, &bPerChan1, uPred);
		
		if (!bPerChan1 || ((psInst1->auDestMask[0] & (1 << uPred)) != 0))
		{
			for (uPred2 = 0; uPred2 < psInst2->uPredCount; ++uPred2)
			{
				GetPredicate(psInst2, &uInst2PredNum, &bInst2PredNeg, &bPerChan2, uPred2);
				
				if (!bPerChan2 || ((psInst2->auDestMask[0] & (1 << uPred2)) != 0))
				{
					if (uInst1PredNum == uInst2PredNum)
					{
						return IMG_FALSE;
					}
				}
			}
		}
	}

	return IMG_TRUE;
}

static
IMG_BOOL AreSimilarOnSimilarArgs(PINTERMEDIATE_STATE	psState,
								 PINST					psFirstInst,
								 PINST					psSecondInst,
								 IMG_PUINT32			puFirstWrittenChanCount,
								 IMG_PUINT32			puArgDifferent)
/*****************************************************************************
 FUNCTION	: AreSimilarOnSimilarArgs
    
 PURPOSE	: Is a given instruction similar to another one in that it operates
			  on different elements of the same arguments apart from one.
			  
			  ADD r0 r1.x r2.y
			  ADD r3 r1.y r4.w

 PARAMETERS	: psState					- Compiler state.
			  psFirstInst				- The first instruction to compare
			  psSecondInst				- The second instruction to compare
			  puFirstWrittenChanCount	- The channels written to by the first instruction
			  puArgDifferent			- Used to return the index of the argument that
										  differs between instructions.

 RETURNS	: If the two instructions are similar.
*****************************************************************************/
{
	IMG_UINT32  uSrc, uArgDifferent, uNumArgsDifferent, uDest;
	IMG_UINT32	puSecondWrittenChanCount[VECTOR_MAX_DESTINATION_COUNT];

	if ((psSecondInst->eOpcode != psFirstInst->eOpcode) || !PredicatesVectorisable(psState, psFirstInst, psSecondInst))
	{
		return IMG_FALSE;
	}

	if (psFirstInst->eOpcode == IVTEST)
	{
		/* 
			This check is the same as in CompareInstNonSourceParametersTypeVEC, 
			but allows differing channel selection.
		*/
		if (psFirstInst->u.psVec->sTest.eMaskType != psSecondInst->u.psVec->sTest.eMaskType ||
			psFirstInst->u.psVec->sTest.eType != psSecondInst->u.psVec->sTest.eType ||
			psFirstInst->u.psVec->sTest.eTestOpcode != psSecondInst->u.psVec->sTest.eTestOpcode ||
			psFirstInst->u.psVec->bPackScale != psSecondInst->u.psVec->bPackScale ||
			psFirstInst->u.psVec->bWriteYOnly != psSecondInst->u.psVec->bWriteYOnly ||
			psFirstInst->u.psVec->eRepeatMode != psSecondInst->u.psVec->eRepeatMode ||
			psFirstInst->u.psVec->uDestSelectShift != psSecondInst->u.psVec->uDestSelectShift ||
			psFirstInst->u.psVec->uPackSwizzle != psSecondInst->u.psVec->uPackSwizzle)
		{
			return IMG_FALSE;
		}
	}
	else
	{
		if (CompareInstNonSourceParametersTypeVEC(psFirstInst, psSecondInst) != 0)
		{
			return IMG_FALSE;
		}
	}

	/* Ensure the format of the destination is the same. */
	if (psFirstInst->asDest[0].eFmt != psSecondInst->asDest[0].eFmt)
	{
		return IMG_FALSE;
	}

	uArgDifferent = GetNumberArgsDifferent(psState, psFirstInst, psSecondInst, &uNumArgsDifferent, IMG_TRUE);

	/* The case where uNumArgsDifferent==0 is handled elsewhere. */
	if (uNumArgsDifferent != 1)
	{
		return IMG_FALSE;
	}

	/* Ensure the modifiers on the sources are the same */
	for (uSrc = 0; uSrc < GetSwizzleSlotCount(psState, psSecondInst); ++uSrc)
	{
		if (psSecondInst->u.psVec->asSrcMod[uSrc].bNegate	!= psFirstInst->u.psVec->asSrcMod[uSrc].bNegate ||
			psSecondInst->u.psVec->asSrcMod[uSrc].bAbs		!= psFirstInst->u.psVec->asSrcMod[uSrc].bAbs)
		{
			return IMG_FALSE;
		}
	}

	/* For now, only combine instructions with a single destination, apart from VTESTs. */
	if (psSecondInst->uDestCount != 1 && psSecondInst->eOpcode != IVTEST)
	{
		return IMG_FALSE;
	}

	/* Combine only instructions who, combined, write no more than 4 channels. */
	GetNumberOfChannelsWritten(psSecondInst, puSecondWrittenChanCount);

	if (psSecondInst->eOpcode == IVTEST)
	{
		/* Ensure the tests together don't update more than 4 predicate registers. */
		IMG_UINT32 uCombinedDestCount = 0;
		for (uDest = 0; uDest < psSecondInst->uDestCount; ++uDest)
		{
			if (puFirstWrittenChanCount[uDest] != 0)
			{
				++uCombinedDestCount;
			}
			if (puSecondWrittenChanCount[uDest] != 0)
			{
				++uCombinedDestCount;
			}
		}

		if (uCombinedDestCount > 4)
		{
			return IMG_FALSE;
		}
	}
	else
	{
		for (uDest = 0; uDest < psSecondInst->uDestCount; ++uDest)
		{
			IMG_UINT32	uFirstInstWrittenChanCount;

			uFirstInstWrittenChanCount = g_auSetBitCount[psFirstInst->auLiveChansInDest[uDest]];

			if ((uFirstInstWrittenChanCount + puSecondWrittenChanCount[uDest]) > VECTOR_LENGTH)
			{
				return IMG_FALSE;
			}
		}
	}

	*puArgDifferent = uArgDifferent;
	return IMG_TRUE;
}

static
IMG_BOOL AreSimilarOnSameArgs(PINTERMEDIATE_STATE	psState,
							  PINST					psFirstInst,
							  PINST					psSecondInst)
/*****************************************************************************
 FUNCTION	: AreSimilarOnSameArgs
    
 PURPOSE	: Is a given instruction similar to another one in that it operates
			  on different elements of the same arguments.
			  
			  ADD r0 r1.x r2.y
			  ADD r3 r1.y r2.w

 PARAMETERS	: psState					- Compiler state.
			  psFirstInst				- The first instruction to compare
			  psSecondInst				- The second instruction to compare

 RETURNS	: If the two instructions are similar.
*****************************************************************************/
{
	IMG_UINT32  uSrc, uNumArgsDifferent, uDest;
	IMG_UINT32	puSecondWrittenChanCount[VECTOR_MAX_DESTINATION_COUNT];

	if ((psSecondInst->eOpcode != psFirstInst->eOpcode) || !PredicatesVectorisable(psState, psFirstInst, psSecondInst))
	{
		return IMG_FALSE;
	}

	if (CompareInstNonSourceParametersTypeVEC(psFirstInst, psSecondInst) != 0)
	{
		return IMG_FALSE;
	}

	GetNumberArgsDifferent(psState, psFirstInst, psSecondInst, &uNumArgsDifferent, IMG_TRUE);

	if (uNumArgsDifferent != 0)
	{
		return IMG_FALSE;
	}
	
	/* Ensure the modifiers on the sources are the same */
	for (uSrc = 0; uSrc < GetSwizzleSlotCount(psState, psSecondInst); ++uSrc)
	{
		if (psSecondInst->u.psVec->asSrcMod[uSrc].bNegate != psFirstInst->u.psVec->asSrcMod[uSrc].bNegate ||
			psSecondInst->u.psVec->asSrcMod[uSrc].bAbs	!= psFirstInst->u.psVec->asSrcMod[uSrc].bAbs)
		{
			return IMG_FALSE;
		}
	}

	/* For now, only combine instructions with a single destination */
	if (psSecondInst->uDestCount != 1)
	{
		return IMG_FALSE;
	}

	/* Combine only instructions who, combined, write no more than 4 channels. */
	GetNumberOfChannelsWritten(psSecondInst, puSecondWrittenChanCount);

	for (uDest = 0; uDest < psSecondInst->uDestCount; ++uDest)
	{
		IMG_UINT32	uFirstInstWrittenChanCount;

		uFirstInstWrittenChanCount = g_auSetBitCount[psFirstInst->auLiveChansInDest[uDest]];

		if ((uFirstInstWrittenChanCount + puSecondWrittenChanCount[uDest]) > VECTOR_LENGTH)
		{
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

static
IMG_BOOL ChannelsIdentical(PINTERMEDIATE_STATE	psState, 
						   IMG_UINT32			uChan,  
						   PINST				psSimilarInst, 
						   PINST				psInst,
						   IMG_UINT32*			puSameChan)
/*****************************************************************************
 FUNCTION	: ChannelsIdentical
    
 PURPOSE	: Check whether the computation of a given channel in one instruction
			  is the same as the computation of another channel in a second instruction.
			  E.g.
			  
			  ADD r0.x  r1.x  r2.x
			  ADD r3.xy r1.xy r2.xw

 PARAMETERS	: psState		- Compiler state.
			  uChan			- Channel for which we're trying to find a similar one.
			  psSimilarInst	- Instruction to find a similar channel in.
			  psInst		- Instruction whose channel we're trying to match.
			  puSameChan	- If a similar channel is found in the other instruction
							  if will be returned through this parameter.

 RETURNS	: True, if a similar channel was found.
*****************************************************************************/
{
	IMG_UINT32 uMatchingChan;

	for (uMatchingChan = 0; uMatchingChan < VECTOR_LENGTH; ++uMatchingChan)
	{
		if (psSimilarInst->auDestMask[0] & (1 << uMatchingChan))
		{
			IMG_UINT32	uSwizSlot;
			IMG_BOOL	bChanMatching = IMG_TRUE;

			for (uSwizSlot = 0; uSwizSlot < GetSwizzleSlotCount(psState, psInst); uSwizSlot++)
			{
				USEASM_SWIZZLE_SEL	eSelOrig		= GetChan(psInst->u.psVec->auSwizzle[uSwizSlot], uChan);
				USEASM_SWIZZLE_SEL	eSelMatching	= GetChan(psSimilarInst->u.psVec->auSwizzle[uSwizSlot], uMatchingChan);
				
				if (eSelOrig != eSelMatching)
				{
					bChanMatching = IMG_FALSE;
					break;
				}

				/* Ensure modifiers are the same on both channels. */
				if ((psInst->u.psVec->asSrcMod[uSwizSlot].bNegate	!= psSimilarInst->u.psVec->asSrcMod[uSwizSlot].bNegate) ||
					(psInst->u.psVec->asSrcMod[uSwizSlot].bAbs		!= psSimilarInst->u.psVec->asSrcMod[uSwizSlot].bAbs))
				{
					bChanMatching = IMG_FALSE;
					break;
				}

				/* Ensure both arguments are the same format. */
				if (psInst->u.psVec->aeSrcFmt[uSwizSlot] != psSimilarInst->u.psVec->aeSrcFmt[uSwizSlot])
				{
					bChanMatching = IMG_FALSE;
					break;
				}

				if (!g_asSwizzleSel[eSelOrig].bIsConstant)
				{
					IMG_UINT32	uVecBaseArg = uSwizSlot * SOURCE_ARGUMENTS_PER_VECTOR;

					/* Ensure the vector arguments are the same (if any). */
					if (psInst->asArg[uVecBaseArg].uType != USC_REGTYPE_UNUSEDSOURCE)
					{
						if (!EqualArgs(&psInst->asArg[uVecBaseArg], &psSimilarInst->asArg[uVecBaseArg]))
						{
							bChanMatching = IMG_FALSE;
							break;
						}
					}
					else
					{
						IMG_UINT32	uSrcToComp;

						uSrcToComp = g_asSwizzleSel[eSelOrig].uSrcChan;
						if (psSimilarInst->u.psVec->aeSrcFmt[uSwizSlot] == UF_REGFORMAT_F16)
						{
							uSrcToComp /= F16_CHANNELS_PER_SCALAR_REGISTER;
						}
						uSrcToComp += 1 + uVecBaseArg;
				
						/* Ensure the scalar arguments are the same (if any). */
						if (!EqualArgs(&psInst->asArg[uSrcToComp], &psSimilarInst->asArg[uSrcToComp]))
						{
							bChanMatching = IMG_FALSE;
							break;
						}
					}
				}
			}

			if (bChanMatching)
			{
				*puSameChan = uMatchingChan;
				return IMG_TRUE;
			}
		}
	}

	return IMG_FALSE;
}

static
IMG_BOOL InsertMoveInSecUpdate(PINTERMEDIATE_STATE	psState,
							   PINST				psInst)
/*****************************************************************************
 FUNCTION	: InsertMoveInSecUpdate
    
 PURPOSE	: Check if a move instruction can be inserted in the secondary update
			  program, and if so insert it.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- MOV instruction to insert.

  RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32		uResultSize;

	/*
		Check we are allowed to move calculations to the secondary update
		program.
	*/
	if ((psState->uCompilerFlags & UF_EXTRACTCONSTANTCALCS) == 0)
	{
		return IMG_FALSE;
	}
	
	/* Check the instruction is constant. */
	if ((!IsUniformVectorSource(psState, psInst, 0) ||
		(psInst->apsOldDest[0] != NULL && !IsUniformSource(psState, psInst->apsOldDest[0]))))
	{
		return IMG_FALSE;
	}

	/*
		As an improvement, vould check if we have already inserted the move.
		But this is unlikely.
	*/
	/*
		Get the size of the result generated by the swizzling instruction.
	*/
	switch (psInst->asDest[0].eFmt)
	{
		case UF_REGFORMAT_F32: uResultSize = SCALAR_REGISTERS_PER_F32_VECTOR; break;
		case UF_REGFORMAT_F16: uResultSize = SCALAR_REGISTERS_PER_F16_VECTOR; break;
		default: imgabort();
	}

	/*
		Check if there are enough secondary attributes to hold the result of the swizzle.
	*/
	if (!CheckAndUpdateInstSARegisterCount(psState, uResultSize, uResultSize, IMG_TRUE))
	{
		return IMG_FALSE;
	}

	/*
		Append the instruction to the end of the secondary update program.
	*/
	InsertInstBefore(psState, psState->psSecAttrProg->sCfg.psExit, psInst, NULL);

	/*
		Add the result of the swizzling instruction as live out of the secondary update
		program and into the main program.
	*/
	BaseAddNewSAProgResult(psState,
						   psInst->asDest[0].eFmt,
						   IMG_TRUE /* bVector */,
						   0 /* uHwRegAlignment */,
						   uResultSize,
						   psInst->asDest[0].uNumber,
						   USC_ALL_CHAN_MASK,
						   SAPROG_RESULT_TYPE_CALC,
						   IMG_FALSE /* bPartOfRange */);
	
	SetRegisterLiveMask(psState,
						&psState->psSecAttrProg->sCfg.psExit->sRegistersLiveOut,
						USEASM_REGTYPE_TEMP,
						psInst->asDest[0].uNumber,
						0 /* uArrayOffset */,
						USC_ALL_CHAN_MASK);

	return IMG_TRUE;
}

typedef struct _CHAN_MAPPING
{
	IMG_UINT32 uOldChan, uNewChan;
} CHAN_MAPPING;

static
IMG_BOOL CombineVTESTs(PINTERMEDIATE_STATE	psState,
					   PCODEBLOCK			psBlock,
					   PINST				psFirstInst,
					   PINST				psSecondInst,
					   PINST*				psCombinedInst,
					   PINST				psMoveInstsBefore[4],
					   IMG_UINT32			uArgDifferent)
/*****************************************************************************
 FUNCTION	: CombineVTESTs
    
 PURPOSE	: Combine instructions by adding another destination.
			  
			  ADD r0 r1 r2
			  ADD r3 r4 r5
			  -> ADD r0,r3 (r1, r4) (r2, r5)

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- We're currently trying to vectorise.
			  psFirstInst		- Instruction higher up.
			  psSecondInst		- Matching instruction lower down.
			  psCombinedInst	- Used to return the new combined instruction.
			  psMoveInstsBefore	- Array of instructions that were inserted before the combined one
			  uArgDifferent		- The argument that differs between instructions, if there is one.
								  UINT_MAX otherwise.

 RETURNS	: TRUE if the instructions were combined.
*****************************************************************************/
{
	PINST			psResultInst;
	IMG_UINT32		uDest, uChan, uChan2, uSwizSlot;
	CHAN_MAPPING	sChansOfSecondInst[3], sChansOfFirstInst[3];
	IMG_UINT32		uNumberOfChansOfSecondInst, uNumberOfChansOfFirstInst, uIndex, uIndex2, uMovsRemaining;
	IMG_BOOL		bStillFree;
	VTEST_CHANSEL	uFirstTestType, uSecondTestType;
	IMG_UINT32		uFirstChanMask, uSecondChanMask;
	
	ASSERT(psFirstInst->eOpcode == IVTEST);

	if ((psFirstInst->uDestCount != VTEST_PREDICATE_ONLY_DEST_COUNT) ||
		(psSecondInst->uDestCount != VTEST_PREDICATE_ONLY_DEST_COUNT))
	{
		return IMG_FALSE;
	}
	
	uFirstTestType = psFirstInst->u.psVec->sTest.eChanSel;

	if (uFirstTestType != VTEST_CHANSEL_XCHAN && uFirstTestType != VTEST_CHANSEL_PERCHAN)
	{
		return IMG_FALSE;
	}

	uSecondTestType = psSecondInst->u.psVec->sTest.eChanSel;
	
	if (uSecondTestType != VTEST_CHANSEL_XCHAN && uSecondTestType != VTEST_CHANSEL_PERCHAN)
	{
		return IMG_FALSE;
	}

	uFirstChanMask = 0;
	for (uDest = VTEST_PREDICATE_DESTIDX; uDest < VTEST_PREDICATE_ONLY_DEST_COUNT; ++uDest)
	{
		if (psFirstInst->auLiveChansInDest[uDest] != 0)
		{
			uFirstChanMask |= (1 << (uDest-VTEST_PREDICATE_DESTIDX));
		}
	}
	
	/* 
		Create a mapping of the channels written in the first instruction to the channels
		that will be added to the combined instruction for this instruction.
	*/
	uIndex = 0;
	for (uChan = 0; uChan < VECTOR_LENGTH; ++uChan)
	{
		if (uFirstChanMask & (1 << uChan))
		{
			sChansOfFirstInst[uIndex].uOldChan = uChan;
			sChansOfFirstInst[uIndex].uNewChan = uIndex;

			++uIndex;
		}
	}
	uNumberOfChansOfFirstInst = uIndex;
				
	uSecondChanMask = 0;
	for (uDest = VTEST_PREDICATE_DESTIDX; uDest < VTEST_PREDICATE_ONLY_DEST_COUNT; ++uDest)
	{
		if (psSecondInst->auLiveChansInDest[uDest] != 0)
		{
			uSecondChanMask |= (1 << (uDest-VTEST_PREDICATE_DESTIDX));
		}
	}
	
	/* 
		Create a mapping of the channels written in the second instruction to the channels
		that will be added to the combined instruction for this instruction.
	*/
	uIndex = 0;
	for (uChan = 0; uChan < VECTOR_LENGTH; ++uChan)
	{
		if (uSecondChanMask & (1 << uChan))
		{
			sChansOfSecondInst[uIndex].uOldChan = uChan;

			/* 
				Pick the first free dest in the mask of the first
				instruction, which is not yet chosen by a slot of the first
			*/
			for (uChan2 = 0; uChan2 < VECTOR_LENGTH; ++uChan2)
			{
				bStillFree = IMG_TRUE;
				for (uIndex2 = 0; uIndex2 < uNumberOfChansOfFirstInst; ++uIndex2)
				{
					if (sChansOfFirstInst[uIndex2].uNewChan == uChan2)
					{
						bStillFree = IMG_FALSE;
						break;
					}
				}
				
				if (!bStillFree)
				{
					continue;
				}

				bStillFree = IMG_TRUE;
				for (uIndex2 = uIndex; uIndex2 > 0; --uIndex2)
				{
					if (sChansOfSecondInst[uIndex2-1].uNewChan == uChan2)
					{
						bStillFree = IMG_FALSE;
						break;
					}
				}

				if (!bStillFree)
				{
					continue;
				}

				sChansOfSecondInst[uIndex].uNewChan = uChan2;
				break;
			}

			++uIndex;
		}
	}
	uNumberOfChansOfSecondInst = uIndex;
		
	/* Make the combined instruction */
	psResultInst = CopyInst(psState, psFirstInst);

	for (uSwizSlot = 0; uSwizSlot < GetSwizzleSlotCount(psState, psFirstInst); uSwizSlot++)
	{
		for (uIndex = 0; uIndex < uNumberOfChansOfFirstInst; ++uIndex)
		{
			IMG_UINT32 uSel = GetChan(psFirstInst->u.psVec->auSwizzle[uSwizSlot], sChansOfFirstInst[uIndex].uOldChan);

			psResultInst->u.psVec->auSwizzle[uSwizSlot] = SetChan(psResultInst->u.psVec->auSwizzle[uSwizSlot], 
				sChansOfFirstInst[uIndex].uNewChan, 
				uSel);
		}

		for (uIndex = 0; uIndex < uNumberOfChansOfSecondInst; ++uIndex)
		{
			IMG_UINT32 uSel = GetChan(psSecondInst->u.psVec->auSwizzle[uSwizSlot], sChansOfSecondInst[uIndex].uOldChan);

			psResultInst->u.psVec->auSwizzle[uSwizSlot] = SetChan(psResultInst->u.psVec->auSwizzle[uSwizSlot], 
				sChansOfSecondInst[uIndex].uNewChan, 
				uSel);
		}

		psResultInst->u.psVec->asSrcMod[uSwizSlot] = psFirstInst->u.psVec->asSrcMod[uSwizSlot];
	}
	
	/* Initialise liveness to 0 */
	for (uDest = 0; uDest < psResultInst->uDestCount; ++uDest)
	{
		psResultInst->auDestMask[uDest] = 0;
		psResultInst->auLiveChansInDest[uDest] = 0;
	}

	/* Combine the (predicate) destinations. */
	for (uIndex = 0; uIndex < uNumberOfChansOfFirstInst; ++uIndex)
	{
		IMG_UINT uOldDest, uNewDest;

		uOldDest = sChansOfFirstInst[uIndex].uOldChan;
		uNewDest = sChansOfFirstInst[uIndex].uNewChan;

		ASSERT(psFirstInst->asDest[uOldDest].uType != USC_REGTYPE_UNUSEDDEST);
		ASSERT(psFirstInst->auLiveChansInDest[uOldDest] != 0);

		MoveDest(psState, psResultInst, uNewDest, psFirstInst, uOldDest);
		psResultInst->auDestMask[uNewDest] = psFirstInst->auDestMask[uOldDest];
		psResultInst->auLiveChansInDest[uNewDest] = psFirstInst->auLiveChansInDest[uOldDest];
	}

	for (uIndex = 0; uIndex < uNumberOfChansOfSecondInst; ++uIndex)
	{
		IMG_UINT uOldDest, uNewDest;
		
		uOldDest = sChansOfSecondInst[uIndex].uOldChan;
		uNewDest = sChansOfSecondInst[uIndex].uNewChan;
		
		ASSERT(psSecondInst->asDest[uOldDest].uType != USC_REGTYPE_UNUSEDDEST);
		ASSERT(psSecondInst->auLiveChansInDest[uOldDest] != 0);

		MoveDest(psState, psResultInst, uNewDest, psSecondInst, uOldDest);
		psResultInst->auDestMask[uNewDest] = psSecondInst->auDestMask[uOldDest];
		psResultInst->auLiveChansInDest[uNewDest] = psSecondInst->auLiveChansInDest[uOldDest];
	}

	/* Fill the remaining destination with unused predicates. */
	for (uDest = uNumberOfChansOfFirstInst+uNumberOfChansOfSecondInst; 
		 uDest < VTEST_PREDICATE_ONLY_DEST_COUNT; 
		 ++uDest)
	{
		IMG_UINT32 uPredReg = GetNextPredicateRegister(psState);
		SetDest(psState, psResultInst, uDest, USEASM_REGTYPE_PREDICATE, uPredReg, UF_REGFORMAT_F32);
	}

	psResultInst->u.psVec->sTest.eChanSel = VTEST_CHANSEL_PERCHAN;

	InsertInstBefore(psState, psBlock, psResultInst, NULL);

	uMovsRemaining = 0;
	
	psMoveInstsBefore[0] = NULL;
	psMoveInstsBefore[1] = NULL;
	psMoveInstsBefore[2] = NULL;
	psMoveInstsBefore[3] = NULL;

	/* 
		Insert two MOVs for the source in instruction 2 that differs from that in instruction 1, i.e.
		r0 r1 r2
		r3 r1 r4
		->
		MOV temp r2
		MOV temp2[=temp] r4
		r0 r1 temp2

		Insert only one move if the source is a vector register.
	*/
	if (uArgDifferent != UINT_MAX)
	{
		CHAN_MAPPING	sChansOfArg[3];
		PARG			pOldDest;
		PINST			psFirstMoveInst;
		IMG_UINT32		uChansUsedInFirstInst;

		uChansUsedInFirstInst = GetLiveChansInArg(psState, psFirstInst, uArgDifferent * SOURCE_ARGUMENTS_PER_VECTOR);
		
		/* 
			Create a mapping of the channels of the non-matching argument read by the second 
			instruction to the channels that will be added to the combined argument for this instruction.
		*/
		for (uIndex = 0; uIndex < uNumberOfChansOfSecondInst; ++uIndex)
		{
			IMG_BOOL			bFoundMapping = IMG_FALSE;
			USEASM_SWIZZLE_SEL	eSel = GetChan(psSecondInst->u.psVec->auSwizzle[uArgDifferent], 
											   sChansOfSecondInst[uIndex].uOldChan);

			ASSERT(psSecondInst->auDestMask[0] & (1 << sChansOfSecondInst[uIndex].uOldChan));

			sChansOfArg[uIndex].uOldChan = eSel;

			if ((uChansUsedInFirstInst & (1 << eSel)) == 0)
			{
				/* Try to keep the swizzle of the second instruction */
				if ((uChansUsedInFirstInst & (1 << eSel)) == 0)
				{
					IMG_BOOL bStillFree = IMG_TRUE;

					for (uIndex2 = uIndex; uIndex2 > 0; --uIndex2)
					{
						if (sChansOfArg[uIndex2-1].uNewChan == g_asSwizzleSel[eSel].uSrcChan)
						{
							bStillFree = IMG_FALSE;
						}
					}

					if (bStillFree)
					{
						sChansOfArg[uIndex].uNewChan = g_asSwizzleSel[eSel].uSrcChan;
						continue;
					}
				}
			}
			
			/* 
				Otherwise pick the first free dest in the mask of the first
				instruction, which is not yet chosen by a slot of the first
			*/
			for (uChan = 0; uChan < VECTOR_LENGTH; ++uChan)
			{
				if ((uChansUsedInFirstInst & (1 << uChan)) == 0)
				{
					IMG_BOOL bStillFree = IMG_TRUE;

					for (uIndex2 = uIndex; uIndex2 > 0; --uIndex2)
					{
						if (sChansOfArg[uIndex2-1].uNewChan == uChan)
						{
							bStillFree = IMG_FALSE;
						}
					}

					if (!bStillFree)
					{
						continue;
					}

					sChansOfArg[uIndex].uNewChan = uChan;
					bFoundMapping = IMG_TRUE;
					break;
				}
			}

			ASSERT(bFoundMapping);
		}
		
		if (psFirstInst->asArg[uArgDifferent*SOURCE_ARGUMENTS_PER_VECTOR].uType == USC_REGTYPE_UNUSEDSOURCE)
		{
			IMG_UINT32	uTempReg = GetNextRegister(psState);
			
			psFirstMoveInst = AllocateInst(psState, psFirstInst);
			SetOpcode(psState, psFirstMoveInst, IVMOV);
			
			psFirstMoveInst->u.psVec->aeSrcFmt[0] = psFirstInst->u.psVec->aeSrcFmt[uArgDifferent];

			for (uChan = 0; uChan < SOURCE_ARGUMENTS_PER_VECTOR; ++uChan)
			{
				CopySrc(psState, 
						psFirstMoveInst,
						uChan,
						psFirstInst,
						uArgDifferent*SOURCE_ARGUMENTS_PER_VECTOR + uChan);
			}

			SetDest(psState, 
					psFirstMoveInst,
					0, 
					USEASM_REGTYPE_TEMP,
					uTempReg, 
					psResultInst->u.psVec->aeSrcFmt[uArgDifferent]);

			psFirstMoveInst->u.psVec->auSwizzle[0]	= USEASM_SWIZZLE(X, Y, Z, W);
			psFirstMoveInst->auDestMask[0]			= uChansUsedInFirstInst;
			psFirstMoveInst->auLiveChansInDest[0]	= uChansUsedInFirstInst;

			CopyPredicate(psState, psFirstMoveInst, psResultInst);
			
			InsertInstBefore(psState, psBlock, psFirstMoveInst, NULL);

			pOldDest = &psFirstMoveInst->asDest[0];
			psMoveInstsBefore[0] = psFirstMoveInst;
		}
		else
		{
			psFirstMoveInst = NULL;
			pOldDest = &psFirstInst->asArg[uArgDifferent*SOURCE_ARGUMENTS_PER_VECTOR];
		}

		{
			IMG_UINT32	uTempReg = GetNextRegister(psState);
			PINST		psMoveInst = AllocateInst(psState, psFirstInst);
			SetOpcode(psState, psMoveInst, IVMOV);

			psMoveInst->u.psVec->aeSrcFmt[0] = psSecondInst->u.psVec->aeSrcFmt[uArgDifferent];

			for (uChan = 0; uChan < SOURCE_ARGUMENTS_PER_VECTOR; ++uChan)
			{
				CopySrc(psState,
						psMoveInst,
						uChan,
						psSecondInst,
						uArgDifferent*SOURCE_ARGUMENTS_PER_VECTOR + uChan);
			}

			SetDest(psState, 
					psMoveInst,
					0,
					USEASM_REGTYPE_TEMP,
					uTempReg, 
					psResultInst->u.psVec->aeSrcFmt[uArgDifferent]);

			SetPartiallyWrittenDest(psState, psMoveInst, 0, pOldDest);

			SetSrc(psState,
				   psResultInst,
				   uArgDifferent*SOURCE_ARGUMENTS_PER_VECTOR, 
				   USEASM_REGTYPE_TEMP,
				   uTempReg, 
				   psResultInst->u.psVec->aeSrcFmt[uArgDifferent]);

			for (uChan = 0; uChan < VECTOR_LENGTH; ++uChan)
			{
				SetSrcUnused(psState, psResultInst, uArgDifferent*SOURCE_ARGUMENTS_PER_VECTOR + uChan + 1);
			}

			psMoveInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
			psMoveInst->auDestMask[0] = 0;
			for (uIndex = 0; uIndex < uNumberOfChansOfSecondInst; ++uIndex)
			{
				if ((sChansOfArg[uIndex].uNewChan >= USEASM_SWIZZLE_SEL_X) &&
					(sChansOfArg[uIndex].uNewChan <= USEASM_SWIZZLE_SEL_W))
				{
					psMoveInst->auDestMask[0] |= (1 << sChansOfArg[uIndex].uNewChan);
				}

				psResultInst->u.psVec->auSwizzle[uArgDifferent] = SetChan(psResultInst->u.psVec->auSwizzle[uArgDifferent], 
					sChansOfSecondInst[uIndex].uNewChan,
					sChansOfArg[uIndex].uNewChan);

				psMoveInst->u.psVec->auSwizzle[0] = SetChan(psMoveInst->u.psVec->auSwizzle[0],
					sChansOfArg[uIndex].uNewChan,
					sChansOfArg[uIndex].uOldChan);
			}
			psMoveInst->auLiveChansInDest[0] = UseDefGetUsedChanMask(psState, 
																	 UseDefGet(psState, USEASM_REGTYPE_TEMP, uTempReg));

			CopyPredicate(psState, psMoveInst, psResultInst);
			
			psMoveInstsBefore[1] = psMoveInst;

			/* Just one move needed. See whether it can be removed. */
			if (psFirstMoveInst == NULL)
			{
				if ((psBlock->psOwner->psFunc == psState->psSecAttrProg) ||
					uMovsRemaining != 0 ||
					!InsertMoveInSecUpdate(psState, psMoveInst))
				{
					InsertInstBefore(psState, psBlock, psMoveInst, NULL);

					if (!EliminateVMOV(psState, psMoveInst, IMG_TRUE))
					{
						++uMovsRemaining;
					}
				}
			}
			else
			{
				InsertInstBefore(psState, psBlock, psMoveInst, NULL);

				/* Two moves */
				if ((psFirstMoveInst->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F16) &&
					!IsCompatibleF16SourceVectors(psState, NULL /* psDestInst */, psFirstMoveInst, psMoveInst))
				{
					/* Cannot combine both MOVs into one */
					++uMovsRemaining;
				}
				else if (psFirstMoveInst->asArg[0].uType != psMoveInst->asArg[0].uType)
				{
					/* Cannot combine both MOVs into one */
					++uMovsRemaining;
				}
				else
				{
					/* Combine the two moves into one. Then try to remove. */
					PINST pCombinedMov = CombineMOVInstructions(psState, psFirstMoveInst, psMoveInst);
					
					ASSERT(pCombinedMov != NULL);

					RemoveInst(psState, psFirstMoveInst->psBlock, psFirstMoveInst);
					FreeInst(psState, psFirstMoveInst);
					RemoveInst(psState, psMoveInst->psBlock, psMoveInst);
					FreeInst(psState, psMoveInst);

					psMoveInstsBefore[0] = pCombinedMov;
					psMoveInstsBefore[1] = NULL;

					if (!EliminateVMOV(psState, pCombinedMov, IMG_TRUE))
					{
						++uMovsRemaining;
					}
				}
			}
		}
	}
	
	*psCombinedInst = psResultInst;
	return IMG_TRUE;
}

static
PINST AddInverseTestInst(PDGRAPH_STATE psDepState, PINST psInst)
/*****************************************************************************
 FUNCTION	: AddInverseTestInst
    
 PURPOSE	: Add an instruction that is the inverse of a given test instruction.
			  Replace the use of all predicate registers from the instruction
			  that are negated with those of the new one.

 PARAMETERS	: psDepState	- Dependency state.
			  psInst		- Test instruction to invert.

 RETURNS	: The new instruction.
*****************************************************************************/
{
	PINST				psNegateInst;
	IMG_UINT32			uDest;
	PINTERMEDIATE_STATE psState = psDepState->psState;

	ASSERT(psInst->eOpcode == IVTEST);

	psNegateInst = CopyInst(psState, psInst);
	
	/* Make new destination predicate registers. */
	for (uDest = 0; uDest < VTEST_PREDICATE_ONLY_DEST_COUNT; ++uDest)
	{
		IMG_UINT32 uPredReg = GetNextPredicateRegister(psState);
		SetDest(psState, psNegateInst, uDest, USEASM_REGTYPE_PREDICATE, uPredReg, UF_REGFORMAT_F32);
	}

	psNegateInst->u.psVec->sTest.eType = LogicalNegateTest(psState, psNegateInst->u.psVec->sTest.eType);

	InsertInstBefore(psState, psInst->psBlock, psNegateInst, NULL);
	
	/*
		Add the instruction to the dependency graph now. Won't remove the instruction later, even if
		vectorisation doesn't happen in the end, for simplicity.
	*/
	AddNewInstToDGraph(psDepState, psNegateInst);
				
	/* Modify all instructions that used the negated predicate registers to use the new ones. */
	for (uDest = 0; uDest < VTEST_PREDICATE_ONLY_DEST_COUNT; ++uDest)
	{
		if (psInst->auLiveChansInDest[uDest] != 0)
		{
			IMG_UINT32		uSrc;
			PUSC_LIST_ENTRY	psListEntry;
			PUSEDEF_CHAIN	psUseDef = UseDefGet(psState, 
												 psInst->asDest[uDest].uType,
												 psInst->asDest[uDest].uNumber);
	
			for (psListEntry = psUseDef->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
			{
				IMG_UINT32	uPred;
				PUSEDEF		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
				PINST		psUseInst = UseDefGetInst(psUse);
				
				if (psUse == psUseDef->psDef)
				{
					continue;
				}

				if (GetBit(psUseInst->auFlag, INST_PRED_NEG) == 0)
				{
					continue;
				}

				for (uPred = 0; uPred < psUseInst->uPredCount; ++uPred)
				{
					if (psUseInst->apsPredSrc[uPred] != 0)
					{
						SetPredicateAtIndex(psState,
											psUseInst, 
											psNegateInst->asDest[uDest].uNumber,
											IMG_FALSE, 
											uPred);
					}
				}
				
				/* Change the dependency between the using instruction and the old TEST to be between it and the new TEST */
				if (psInst->psBlock == NULL && psUseInst->psBlock == NULL)
				{
					RemoveDependency(psDepState, psInst->uId, psUseInst);
					AddClosedDependency(psState, psDepState, psNegateInst->uId, psUseInst->uId);
				}

				SetBit(psUseInst->auFlag, INST_PRED_NEG, 0);
			}

			/* Add dependencies between the new TEST instruction and its arguments. */
			for (uSrc = 0; uSrc < SOURCE_ARGUMENTS_PER_VECTOR; ++uSrc)
			{
				if (psNegateInst->asArg[uSrc].uType != USC_REGTYPE_UNUSEDSOURCE &&
					psNegateInst->asArg[uSrc].uType != USEASM_REGTYPE_MAXIMUM)
				{
					PINST psDefInst = UseDefGetDefInst(psState,
						psNegateInst->asArg[uSrc].uType,
						psNegateInst->asArg[uSrc].uNumber, 
						NULL /* puDestIdx */);

					if (psDefInst->psBlock == NULL)
					{
						AddClosedDependency(psState, psDepState, psDefInst->uId, psNegateInst->uId);
					}
				}
			}
		}
	}

	return psNegateInst;
}

static
IMG_BOOL CombineInstructions(PDGRAPH_STATE			psDepState,
							 PCODEBLOCK				psBlock,
							 PINST					psFirstInst,
							 PINST					psSecondInst,
							 PINST*					psCombinedInst,
							 PINST					psMoveInstsAfter[VECTOR_MAX_DESTINATION_COUNT*4],
							 PINST					psMoveInstsBefore[4],
							 IMG_UINT32				uArgDifferent)
/*****************************************************************************
 FUNCTION	: CombineInstructions
    
 PURPOSE	: Combine instructions which operate on different elements
			  of the same vector.
			  
			  ADD r0.x r1.x r2.y
			  ADD r3.x r1.y r2.w
			  ->
			  ADD r3.xy r1.xy r2.yw
			  MOV r0.x r3.y

			  Or one argument doesn't match, in which case more MOVs are inserted:

			  ADD r0.x r1.x r2.y
			  ADD r3.x r1.y r4.w
			  ->
			  MOV r5.x[=r2] r4.w
			  ADD r3.xy r1.xy r5.xy
			  MOV r0.x r3.y

 PARAMETERS	: psDepState		- Dependency state.
			  psBlock			- We're currently trying to vectorise.
			  psFirstInst		- Instruction higher up.
			  psSecondInst		- Matching instruction lower down.
			  psCombinedInst	- Used to return the new combined instruction.
			  psMoveInstsAfter	- Array containing MOVs to be inserted after the combined instruction
			  psMoveInstsBefore	- Array of instructions that were inserted before the combined one
			  uArgDifferent		- The argument that differs between instructions, if there is one.
								  UINT_MAX otherwise.

 RETURNS	: TRUE if the instructions were combined.
*****************************************************************************/
{
	PINST				psResultInst, psMoveInst;
	IMG_UINT32			uDest, uSrc, uChan, uChan2, uSwizSlot, uPredNum1, uPredNum2;
	CHAN_MAPPING		sChansOfSecondInst[3];
	IMG_UINT32			uNumberOfChansOfSecondInst;
	IMG_UINT32			uIndex, uIndex2, uMovsAfter, uMovsAfterIndex;
	IMG_BOOL			bStillFree, bVectorisingPreds;
	IMG_UINT32			uMovsRemaining = 0;
	PINTERMEDIATE_STATE psState = psDepState->psState;
	
	if (psFirstInst->eOpcode == IVTEST)
	{
		return CombineVTESTs(psState,
							 psBlock,
							 psFirstInst,
							 psSecondInst,
							 psCombinedInst,
							 psMoveInstsBefore,
							 uArgDifferent);
	}

	ASSERT(psSecondInst->uDestCount == 1);

	uPredNum1 = USC_PREDREG_NONE;
	uPredNum2 = USC_PREDREG_NONE;
	bVectorisingPreds = IMG_FALSE;

	if (psFirstInst->apsPredSrc != NULL)
	{
		IMG_UINT32 uDestId;	

		ASSERT(psSecondInst->apsPredSrc != NULL);

		uPredNum1 = psFirstInst->apsPredSrc[0]->uNumber;
		uPredNum2 = psSecondInst->apsPredSrc[0]->uNumber;

		bVectorisingPreds = (uPredNum1 != uPredNum2);
		bVectorisingPreds |= (GetBit(psFirstInst->auFlag, INST_PRED_NEG) != GetBit(psSecondInst->auFlag, INST_PRED_NEG));
		bVectorisingPreds |= (GetBit(psFirstInst->auFlag, INST_PRED_PERCHAN) != GetBit(psSecondInst->auFlag, INST_PRED_PERCHAN));

		/* Ensure the first instruction already uses the right channel. */
		UseDefGetDefInstFromChain(psFirstInst->apsPredSrcUseDef[0]->psUseDefChain, &uDestId);
		if ((psFirstInst->auDestMask[0] & (1 << uDestId)) == 0)
		{
			return IMG_FALSE;
		}

		/* For now, don't vectorise if the first channel of the predicate won't be used. */
		if (uDestId != 0)
		{
			return IMG_FALSE;
		}
	}

	/* 
		Create a mapping of the channels written in the second instruction to the channels
		that will be added to the combined instruction for this instruction.
	*/
	uIndex = 0;
	for (uChan = 0; uChan < VECTOR_LENGTH; ++uChan)
	{
		if (psSecondInst->auDestMask[0] & (1 << uChan))
		{
			sChansOfSecondInst[uIndex].uOldChan = uChan;

			if (bVectorisingPreds)
			{
				/* The channels are dictated by the order of the predicates in the defining VTEST. */
				IMG_UINT32 uDestId;	
				UseDefGetDefInstFromChain(psSecondInst->apsPredSrcUseDef[uIndex]->psUseDefChain, &uDestId);
				
				ASSERT((psFirstInst->auDestMask[0] & (1 << uDestId)) == 0);
				for (uIndex2 = uIndex; uIndex2 > 0; --uIndex2)
				{
					ASSERT(sChansOfSecondInst[uIndex2-1].uNewChan != uDestId);
				}
				
				sChansOfSecondInst[uIndex].uNewChan = uDestId;
				++uIndex;
			}
			else
			{
				IMG_UINT32 uIdenticalChannel;

				/* 
					If all swizzles and arguments on a channel of one instruction are identical to those
					of the other, then we can reuse the channel. 
				*/
				if (ChannelsIdentical(psState, uChan, psFirstInst, psSecondInst, &uIdenticalChannel))
				{
					sChansOfSecondInst[uIndex].uNewChan = uIdenticalChannel;
				}
				else
				{
					bStillFree = IMG_TRUE;
					for (uIndex2 = uIndex; uIndex2 > 0; --uIndex2)
					{
						if (sChansOfSecondInst[uIndex2-1].uNewChan == uChan)
						{
							bStillFree = IMG_FALSE;
						}
					}

					if (bStillFree && (psFirstInst->auLiveChansInDest[0] & (1 << uChan)) == 0)
					{
						/* Try to keep the mask of the second instruction */
						sChansOfSecondInst[uIndex].uNewChan = uChan;
					}
					else
					{
						/* 
							Otherwise pick the first free dest in the mask of the first
							instruction, which is not yet chosen by a slot of the first
						*/
						for (uChan2 = 0; uChan2 < VECTOR_LENGTH; ++uChan2)
						{
							if ((psFirstInst->auLiveChansInDest[0] & (1 << uChan2)) == 0)
							{
								bStillFree = IMG_TRUE;

								for (uIndex2 = uIndex; uIndex2 > 0; --uIndex2)
								{
									if (sChansOfSecondInst[uIndex2-1].uNewChan == uChan2)
									{
										bStillFree = IMG_FALSE;
									}
								}

								if (!bStillFree)
								{
									continue;
								}

								sChansOfSecondInst[uIndex].uNewChan = uChan2;
								break;
							}
						}
						ASSERT(uChan2 < VECTOR_LENGTH);
					}
				}

				++uIndex;
			}
		}
	}
	
	uNumberOfChansOfSecondInst = uIndex;
	
	/* Make the combined instruction */
	psResultInst = CopyInst(psState, psFirstInst);
	
	psMoveInstsBefore[2] = NULL;
	psMoveInstsBefore[3] = NULL;
	
	if (bVectorisingPreds)
	{
		/* If the instructions use a negated predicate, insert a negated version of the test. */
		if (GetBit(psResultInst->auFlag, INST_PRED_NEG) != 0)
		{
			IMG_UINT32	uPred;
			PINST		psDefInst = UseDefGetDefInst(psState,
													 psFirstInst->apsPredSrc[0]->uType, 
													 psFirstInst->apsPredSrc[0]->uNumber,
													 NULL /* puDestIdx */);

			psMoveInstsBefore[2] = AddInverseTestInst(psDepState, psDefInst);
			
			for (uPred = 0; uPred < psResultInst->uPredCount; ++uPred)
			{
				if (psResultInst->apsPredSrc[uPred] != 0)
				{
					SetPredicateAtIndex(psState,
										psResultInst, 
										psFirstInst->apsPredSrc[uPred]->uNumber, 
										IMG_FALSE,
										uPred);
				}
			}

			SetBit(psResultInst->auFlag, INST_PRED_NEG, 0);
		}

		MakePredicatePerChan(psState, psResultInst);
		
		if (GetBit(psSecondInst->auFlag, INST_PRED_PERCHAN) != 0)
		{
			for (uChan = 0; uChan < VECTOR_LENGTH; ++uChan)
			{
				IMG_UINT32  uInstPredNum;
				IMG_BOOL	bInstPredNeg, bInstPredPerChan;

				GetPredicate(psSecondInst, &uInstPredNum, &bInstPredNeg, &bInstPredPerChan, 0);

				if (uInstPredNum != USC_PREDREG_NONE)
				{
					ASSERT(psResultInst->apsPredSrc[uChan] == NULL);
					SetPredicateAtIndex(psState, psResultInst, uInstPredNum, IMG_FALSE, uChan);
				}
			}
		}
		else
		{
			IMG_UINT32  uInstPredNum;
			IMG_BOOL	bInstPredNeg, bInstPredPerChan;

			ASSERT(uNumberOfChansOfSecondInst == 1 && psSecondInst->uPredCount == 1);

			GetPredicate(psSecondInst, &uInstPredNum, &bInstPredNeg, &bInstPredPerChan, 0);

			ASSERT(uInstPredNum != USC_PREDREG_NONE);
			
			if (GetBit(psSecondInst->auFlag, INST_PRED_NEG) == 0)
			{
				SetPredicateAtIndex(psState, 
									psResultInst,
									uInstPredNum,
									IMG_FALSE, 
									sChansOfSecondInst[0].uNewChan);
			}
			else
			{
				PINST psDefInst = UseDefGetDefInst(psState, 
												   psSecondInst->apsPredSrc[0]->uType,
												   psSecondInst->apsPredSrc[0]->uNumber,
												   NULL /* puDestIdx */);

				psMoveInstsBefore[3] = AddInverseTestInst(psDepState, psDefInst);
			}
		}
	}

	/* Copy the sources and set the swizzle. */
	for (uSwizSlot = 0; uSwizSlot < GetSwizzleSlotCount(psState, psFirstInst); uSwizSlot++)
	{
		IMG_UINT32	uArgBase = uSwizSlot * SOURCE_ARGUMENTS_PER_VECTOR + 1;

		for (uIndex = 0; uIndex < uNumberOfChansOfSecondInst; ++uIndex)
		{
			USEASM_SWIZZLE_SEL eSel = GetChan(psSecondInst->u.psVec->auSwizzle[uSwizSlot],
											  sChansOfSecondInst[uIndex].uOldChan);

			psResultInst->u.psVec->auSwizzle[uSwizSlot] = 
				SetChan(psResultInst->u.psVec->auSwizzle[uSwizSlot], 
						sChansOfSecondInst[uIndex].uNewChan, 
						eSel);

			if (!g_asSwizzleSel[eSel].bIsConstant)
			{
				IMG_UINT32	uSrcChan = g_asSwizzleSel[eSel].uSrcChan;
				IMG_UINT32	uArg = uArgBase + uSrcChan;
				
				CopySrc(psState, psResultInst, uArg, psSecondInst, uArg);
			}
		}
	}
	
	/* In case the source modifiers don't match, use those from the second instruction. */
	for (uSrc = 0; uSrc < psFirstInst->uArgumentCount/5; ++uSrc)
	{
		psResultInst->u.psVec->asSrcMod[uSrc] = psFirstInst->u.psVec->asSrcMod[uSrc];
	}
	
	for (uDest = 0; uDest < psFirstInst->uDestCount; ++uDest)
	{		
		MoveDest(psState, psResultInst, uDest, psFirstInst, uDest);

		for (uIndex = 0; uIndex < uNumberOfChansOfSecondInst; ++uIndex)
		{
			psResultInst->auDestMask[uDest] |= (1 << sChansOfSecondInst[uIndex].uNewChan);
			psResultInst->auLiveChansInDest[uDest] |= (1 << sChansOfSecondInst[uIndex].uNewChan);
		}
	}

	InsertInstBefore(psState, psBlock, psResultInst, NULL);
	
	uMovsAfter = 0;
	uMovsAfterIndex = 0;
	while (psMoveInstsAfter[uMovsAfterIndex] != NULL)
	{
		++uMovsAfterIndex;
	}
	
	/* Insert moves to copy the new destinations to the destinations of the second instruction */
	for (uDest = 0; uDest < psFirstInst->uDestCount; ++uDest)
	{
		psMoveInst = AllocateInst(psState, psFirstInst);
		SetOpcode(psState, psMoveInst, IVMOV);
		
		psMoveInst->u.psVec->aeSrcFmt[0] = psResultInst->asDest[uDest].eFmt;

		SetSrc(psState,
			   psMoveInst, 
			   0, 
			   USEASM_REGTYPE_TEMP,
			   psResultInst->asDest[uDest].uNumber,
			   psResultInst->asDest[uDest].eFmt);

		MoveDest(psState, psMoveInst, 0, psSecondInst, uDest);
		
		psMoveInst->auDestMask[0] = psSecondInst->auDestMask[0];
		psMoveInst->auLiveChansInDest[0] = psSecondInst->auLiveChansInDest[0];
		
		psMoveInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
		for (uIndex = 0; uIndex < uNumberOfChansOfSecondInst; ++uIndex)
		{
			psMoveInst->u.psVec->auSwizzle[0] = SetChan(psMoveInst->u.psVec->auSwizzle[0], 
				sChansOfSecondInst[uIndex].uOldChan,
				sChansOfSecondInst[uIndex].uNewChan);
		}

		if (psSecondInst->apsOldDest != NULL)
		{
			SetPartiallyWrittenDest(psState, psMoveInst, 0, psSecondInst->apsOldDest[0]);
		}
		
		CopyPredicate(psState, psMoveInst, psResultInst);

		InsertInstAfter(psState, psResultInst->psBlock, psMoveInst, psResultInst);

		if (!EliminateVMOV(psState, psMoveInst, IMG_TRUE))
		{
			++uMovsRemaining;
		}

		psMoveInstsAfter[uMovsAfterIndex] = psMoveInst;
		++uMovsAfter;
		++uMovsAfterIndex;
	}

	psMoveInstsBefore[0] = NULL;
	psMoveInstsBefore[1] = NULL;

	/* 
		Insert two MOVs for the source in instruction 2 that differs from that in instruction 1, i.e.
		r0 r1 r2
		r3 r1 r4
		->
		MOV temp r2
		MOV temp2[=temp] r4
		r0 r1 temp2

		Insert only one move if the source is a vector register.
	*/
	if (uArgDifferent != UINT_MAX)
	{
		CHAN_MAPPING	sChansOfArg[3];
		PARG			pOldDest;
		PINST			psFirstMoveInst;
		IMG_UINT32		uChansUsedInFirstInst, uChansConstant;
		
		uChansUsedInFirstInst = GetLiveChansInArg(psState, psFirstInst, uArgDifferent*SOURCE_ARGUMENTS_PER_VECTOR);

		/* 
			Create a mapping of the channels of the non-matching argument read by the second 
			instruction to the channels that will be added to the combined argument for this instruction.
		*/
		uChansConstant = 0;
		for (uIndex = 0; uIndex < uNumberOfChansOfSecondInst; ++uIndex)
		{
			USEASM_SWIZZLE_SEL	eSel = GetChan(psSecondInst->u.psVec->auSwizzle[uArgDifferent], 
				sChansOfSecondInst[uIndex].uOldChan);

			ASSERT(psSecondInst->auDestMask[0] & (1 << sChansOfSecondInst[uIndex].uOldChan));

			sChansOfArg[uIndex].uOldChan = eSel;
			
			if (g_asSwizzleSel[eSel].bIsConstant)
			{
				sChansOfArg[uIndex].uNewChan = eSel;
				++uChansConstant;
				continue;
			}

			if ((uChansUsedInFirstInst & (1 << eSel)) == 0)
			{
				/* Try to keep the swizzle of the second instruction */
				if ((uChansUsedInFirstInst & (1 << eSel)) == 0)
				{
					IMG_BOOL bStillFree = IMG_TRUE;

					for (uIndex2 = uIndex; uIndex2 > 0; --uIndex2)
					{
						if (sChansOfArg[uIndex2-1].uNewChan == g_asSwizzleSel[eSel].uSrcChan)
						{
							bStillFree = IMG_FALSE;
						}
					}

					if (bStillFree)
					{
						sChansOfArg[uIndex].uNewChan = g_asSwizzleSel[eSel].uSrcChan;
						continue;
					}
				}
			}
			
			/* 
				Otherwise pick the first free dest in the mask of the first
				instruction, which is not yet chosen by a slot of the first
			*/
			for (uChan = 0; uChan < VECTOR_LENGTH; ++uChan)
			{
				if ((uChansUsedInFirstInst & (1 << uChan)) == 0)
				{
					IMG_BOOL bStillFree = IMG_TRUE;

					for (uIndex2 = uIndex; uIndex2 > 0; --uIndex2)
					{
						if (sChansOfArg[uIndex2-1].uNewChan == uChan)
						{
							bStillFree = IMG_FALSE;
						}
					}

					if (!bStillFree)
					{
						continue;
					}

					sChansOfArg[uIndex].uNewChan = uChan;
					break;
				}
			}
		}
		
		if (uChansConstant != uNumberOfChansOfSecondInst)
		{
			if (psFirstInst->asArg[uArgDifferent*SOURCE_ARGUMENTS_PER_VECTOR].uType == USC_REGTYPE_UNUSEDSOURCE)
			{
				IMG_UINT32	uTempReg = GetNextRegister(psState);

				psFirstMoveInst = AllocateInst(psState, psFirstInst);
				SetOpcode(psState, psFirstMoveInst, IVMOV);

				psFirstMoveInst->u.psVec->aeSrcFmt[0] = psFirstInst->u.psVec->aeSrcFmt[uArgDifferent];

				for (uChan = 0; uChan < SOURCE_ARGUMENTS_PER_VECTOR; ++uChan)
				{
					CopySrc(psState, 
						psFirstMoveInst,
						uChan, 
						psFirstInst,
						uArgDifferent*SOURCE_ARGUMENTS_PER_VECTOR + uChan);
				}

				SetDest(psState, 
					psFirstMoveInst,
					0, 
					USEASM_REGTYPE_TEMP, 
					uTempReg, 
					psResultInst->u.psVec->aeSrcFmt[uArgDifferent]);

				psFirstMoveInst->u.psVec->auSwizzle[0]	= USEASM_SWIZZLE(X, Y, Z, W);
				psFirstMoveInst->auDestMask[0]			= uChansUsedInFirstInst;
				psFirstMoveInst->auLiveChansInDest[0]	= uChansUsedInFirstInst;

				CopyPredicate(psState, psFirstMoveInst, psResultInst);

				InsertInstBefore(psState, psBlock, psFirstMoveInst, NULL);

				pOldDest = &psFirstMoveInst->asDest[0];
				psMoveInstsBefore[0] = psFirstMoveInst;
			}
			else
			{
				psFirstMoveInst = NULL;
				pOldDest = &psFirstInst->asArg[uArgDifferent*SOURCE_ARGUMENTS_PER_VECTOR];
			}

			{
				IMG_UINT32	uTempReg = GetNextRegister(psState);
				PINST		psMoveInst = AllocateInst(psState, psFirstInst);
				SetOpcode(psState, psMoveInst, IVMOV);

				psMoveInst->u.psVec->aeSrcFmt[0] = psSecondInst->u.psVec->aeSrcFmt[uArgDifferent];

				for (uChan = 0; uChan < SOURCE_ARGUMENTS_PER_VECTOR; ++uChan)
				{
					CopySrc(psState, 
						psMoveInst, 
						uChan, 
						psSecondInst,
						uArgDifferent*SOURCE_ARGUMENTS_PER_VECTOR + uChan);
				}

				SetDest(psState, 
					psMoveInst,
					0,
					USEASM_REGTYPE_TEMP, 
					uTempReg, 
					psResultInst->u.psVec->aeSrcFmt[uArgDifferent]);

				SetPartiallyWrittenDest(psState, psMoveInst, 0, pOldDest);

				SetSrc(psState, 
					psResultInst,
					uArgDifferent*SOURCE_ARGUMENTS_PER_VECTOR, 
					USEASM_REGTYPE_TEMP,
					uTempReg, 
					psResultInst->u.psVec->aeSrcFmt[uArgDifferent]);

				for (uChan = 0; uChan < VECTOR_LENGTH; ++uChan)
				{
					SetSrcUnused(psState, psResultInst, uArgDifferent*SOURCE_ARGUMENTS_PER_VECTOR + uChan + 1);
				}

				psMoveInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
				psMoveInst->auDestMask[0] = 0;
				for (uIndex = 0; uIndex < uNumberOfChansOfSecondInst; ++uIndex)
				{
					if ((sChansOfArg[uIndex].uNewChan >= USEASM_SWIZZLE_SEL_X) &&
						(sChansOfArg[uIndex].uNewChan <= USEASM_SWIZZLE_SEL_W))
					{
						psMoveInst->auDestMask[0] |= (1 << sChansOfArg[uIndex].uNewChan);
					}

					psResultInst->u.psVec->auSwizzle[uArgDifferent] = SetChan(psResultInst->u.psVec->auSwizzle[uArgDifferent], 
						sChansOfSecondInst[uIndex].uNewChan,
						sChansOfArg[uIndex].uNewChan);

					psMoveInst->u.psVec->auSwizzle[0] = SetChan(psMoveInst->u.psVec->auSwizzle[0],
						sChansOfArg[uIndex].uNewChan,
						sChansOfArg[uIndex].uOldChan);
				}
				psMoveInst->auLiveChansInDest[0] = UseDefGetUsedChanMask(psState, 
					UseDefGet(psState, USEASM_REGTYPE_TEMP, uTempReg));

				CopyPredicate(psState, psMoveInst, psResultInst);

				psMoveInstsBefore[1] = psMoveInst;

				/* Just one move needed. See whether it can be removed. */
				if (psFirstMoveInst == NULL)
				{
					if ((psBlock->psOwner->psFunc == psState->psSecAttrProg) ||
						uMovsRemaining != 0 ||
						!InsertMoveInSecUpdate(psState, psMoveInst))
					{
						InsertInstBefore(psState, psBlock, psMoveInst, NULL);

						if (!EliminateVMOV(psState, psMoveInst, IMG_TRUE))
						{
							++uMovsRemaining;
						}
					}
				}
				else
				{
					InsertInstBefore(psState, psBlock, psMoveInst, NULL);

					/* Two moves */
					if ((psFirstMoveInst->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F16) &&
						!IsCompatibleF16SourceVectors(psState, NULL /* psDestInst */, psFirstMoveInst, psMoveInst))
					{
						/* Cannot combine both MOVs into one */
						++uMovsRemaining;
					}
					else if (psFirstMoveInst->asArg[0].uType != psMoveInst->asArg[0].uType)
					{
						/* Cannot combine both MOVs into one */
						++uMovsRemaining;
					}
					else if (psFirstMoveInst->u.psVec->aeSrcFmt[0]	== psMoveInst->u.psVec->aeSrcFmt[0])
					{
						/* Combine the two moves into one. Then try to remove. */
						PINST pCombinedMov = CombineMOVInstructions(psState, psFirstMoveInst, psMoveInst);

						ASSERT(pCombinedMov != NULL);

						RemoveInst(psState, psFirstMoveInst->psBlock, psFirstMoveInst);
						FreeInst(psState, psFirstMoveInst);
						RemoveInst(psState, psMoveInst->psBlock, psMoveInst);
						FreeInst(psState, psMoveInst);

						psMoveInstsBefore[0] = pCombinedMov;
						psMoveInstsBefore[1] = NULL;

						if (!EliminateVMOV(psState, pCombinedMov, IMG_TRUE))
						{
							++uMovsRemaining;
						}
					}
				}
			}
		}
	}
	
	/*	
		Remove the inverted TEST instructions now. Must do this whether or not the
		vectorisation is successful because they were added to the dependency graph.
	*/
	if (psMoveInstsBefore[2] != NULL)
	{
		RemoveInst(psState, psBlock, psMoveInstsBefore[2]);
	}
	if (psMoveInstsBefore[3] != NULL)
	{
		RemoveInst(psState, psBlock, psMoveInstsBefore[3]);
	}

	/*	
		If you cannot remove all but one MOV that are required for this vectorisation,
		then don't do the vectorisation. This is a simplistic check (MOVs may be
		removed later), but it is better than nothing.
	*/
	if (uMovsRemaining > 0)
	{
		/* Undo the changes. Don't do the vectorisation */
		for (uDest = 0; uDest < psFirstInst->uDestCount; ++uDest)
		{		
			MoveDest(psState, psFirstInst, uDest, psResultInst, uDest);
		}
		RemoveInst(psState, psResultInst->psBlock, psResultInst);
		FreeInst(psState, psResultInst);
		
		if (uArgDifferent != UINT_MAX)
		{
			for (uIndex = 0; uIndex < 2; ++uIndex)
			{
				if (psMoveInstsBefore[uIndex] != NULL)
				{
					ASSERT(psMoveInstsBefore[uIndex]->psBlock == psBlock);

					RemoveInst(psState, psMoveInstsBefore[uIndex]->psBlock, psMoveInstsBefore[uIndex]);
					FreeInst(psState, psMoveInstsBefore[uIndex]);
					psMoveInstsBefore[uIndex] = NULL;
				}
			}
		}

		for (; uMovsAfter > 0; --uMovsAfter)
		{
			MoveDest(psState, psSecondInst, uMovsAfter-1, psMoveInstsAfter[uMovsAfterIndex-1], 0);
			RemoveInst(psState, psMoveInstsAfter[uMovsAfterIndex-1]->psBlock, psMoveInstsAfter[uMovsAfterIndex-1]);
			FreeInst(psState, psMoveInstsAfter[uMovsAfterIndex-1]);
			psMoveInstsAfter[uMovsAfterIndex-1] = NULL;
			--uMovsAfterIndex;
		}
		
		return IMG_FALSE;
	}
	
	*psCombinedInst = psResultInst;

	return IMG_TRUE;
}

static
IMG_BOOL IsOldDestOnlyDependency(PDGRAPH_STATE	psDepState,
								 PINST			psInst,
								 PINST			psDepInst)
/*****************************************************************************
 FUNCTION	: IsOldDestOnlyDependency
    
 PURPOSE	: Check whether the only dependency between two instructions is via
			  the oldDest, i.e.
			  
			  ADD r0	  r1.x r2.y
			  ADD r3[=r0] r4.y r5.w

 PARAMETERS	: psDepState	- Dependency state.
			  psInst		- Instruction.
			  psDepInst		- Dependant instruction.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32			uDest, uDest2;
	PUSEDEF_CHAIN		psUseDef;
	PUSC_LIST_ENTRY		psUseListEntry;
	IMG_BOOL			bFoundDep = IMG_FALSE;
	PINTERMEDIATE_STATE psState = psDepState->psState;

	/* Check psInst is really dependent on psDepInst by its old dest. */
	for (uDest = 0; uDest < psInst->uDestCount; ++uDest)
	{
		for (uDest2 = 0; uDest2 < psDepInst->uDestCount; ++uDest2)
		{
			if (psDepInst->apsOldDest[uDest2] != NULL &&
				psDepInst->apsOldDest[uDest2]->uNumber	== psInst->asDest[uDest].uNumber &&
				psDepInst->apsOldDest[uDest2]->uType	== psInst->asDest[uDest].uType)
			{
				bFoundDep = IMG_TRUE;
			}
		}
	}
	
	if (!bFoundDep)
	{
		return IMG_FALSE;
	}
	
	/* Check whether the second instruction is explicitly dependent on the first */
	for (uDest = 0; uDest < psInst->uDestCount; ++uDest)
	{
		IMG_UINT32 uSrc;
		for (uSrc = 0; uSrc < psDepInst->uArgumentCount; ++uSrc)
		{
			if (psDepInst->asArg[uSrc].uNumber	== psInst->asDest[uDest].uNumber &&
				psDepInst->asArg[uSrc].uType	== psInst->asDest[uDest].uType)
			{
				return IMG_FALSE;
			}
		}
	}

	/* Check whether the second instruction is implicity dependent on the first */
	psUseDef = UseDefGet(psState, psInst->asDest[0].uType, psInst->asDest[0].uNumber);
	
	for (psUseListEntry = psUseDef->sList.psHead; 
		psUseListEntry != NULL;
		psUseListEntry = psUseListEntry->psNext)
	{
		PUSEDEF	psUse = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);

		if (psUse == psUseDef->psDef)
		{
			continue;
		}

		if (GraphGet(psState, psDepState->psClosedDepGraph, psDepInst->uId, psUse->u.psInst->uId))
		{
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

static
PINST DoCombination(PCODEBLOCK			psBlock,
					PINST				psFirstInst,
					PINST				psSecondInst,
					PINST				psMoveInstsAfter[VECTOR_MAX_DESTINATION_COUNT*4],
					PINST				psMoveInstsBefore[4],
					PDGRAPH_STATE		psDepState,
					IMG_UINT32			uArgDifferent)
/*****************************************************************************
 FUNCTION	: DoCombination
    
 PURPOSE	: Try to combine instructions.
			  
 PARAMETERS	: psBlock			- We're currently trying to vectorise.
			  psFirstInst		- Instruction higher up.
			  psSecondInst		- Matching instruction lower down.
			  psMoveInstsAfter	- Array containing MOVs to be inserted after the combined instruction
			  psMoveInstsBefore	- Array of instructions that were inserted before the combined one
			  psDepState		- The dependency graph
			  uArgDifferent		- The argument that differs between instructions, if there is one.
								  UINT_MAX otherwise.

 RETURNS	: Return the new combined instruction, if vectorisation happened.
			  Return NULL otherwise.
*****************************************************************************/
{
	PINST				psCombinedInst;
	PINTERMEDIATE_STATE psState = psDepState->psState;
	
	if (CombineInstructions(psDepState,
							psBlock, 
							psFirstInst, 
							psSecondInst,
							&psCombinedInst,
							psMoveInstsAfter,
							psMoveInstsBefore,
							uArgDifferent))
	{
		IMG_UINT32 uCombinedInst, uIndex;
		
		uCombinedInst = AddNewInstToDGraph(psDepState, psCombinedInst);
		
		/* The set of dependencies of the new instruction is the sum of both. */
		MergeInstructions(psDepState, uCombinedInst, psFirstInst->uId, IMG_TRUE);
		MergeInstructions(psDepState, uCombinedInst, psSecondInst->uId, IMG_TRUE);
						   
		FreeInst(psState, psFirstInst);
		FreeInst(psState, psSecondInst);
		
		/* Remove the instruction again from the block. It might not be ready to be scheduled yet. */
		RemoveInst(psState, psBlock, psCombinedInst);

		/* Similarly the instructions that we added that need to come after it. */
		for (uIndex = 0; psMoveInstsAfter[uIndex] != NULL; ++uIndex)
		{
			if (psMoveInstsAfter[uIndex]->psBlock != NULL)
			{
				if (EliminateVMOV(psState, psMoveInstsAfter[uIndex], IMG_FALSE))
				{
					psMoveInstsAfter[uIndex] = NULL;
				}
				else
				{
					RemoveInst(psState, psBlock, psMoveInstsAfter[uIndex]);
				}
			}
		}

		return psCombinedInst;
	}
	return NULL;
}

static
IMG_VOID OutputDependencies(PDGRAPH_STATE		psDepState,
							PCODEBLOCK			psBlock,
							PINST				psInst,
							PUSC_LIST			psDepInstsOutput)
/*****************************************************************************
 FUNCTION	: OutputDependencies
    
 PURPOSE	: Output the instructions that an instruction depends on.
			  
 PARAMETERS	: psDepState		- Dependency state.
			  psBlock			- Block into which to insert the instructions.
			  psInst			- The instruction whose dependencies we want to fulfill.
			  psDepInstsOutput	- Returns the instructions that were output

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32			uInst;
	PINTERMEDIATE_STATE psState = psDepState->psState;

	for (uInst = 0; uInst < psDepState->uBlockInstructionCount; uInst++)
	{
		PINST psSecondInst = ArrayGet(psState, psDepState->psInstructions, uInst);

		if (psSecondInst != NULL && 
			GraphGet(psState, psDepState->psClosedDepGraph, psInst->uId, uInst))
		{
			InsertInstBefore(psState, psBlock, psSecondInst, NULL);
			AppendToList(psDepInstsOutput, &psSecondInst->sTempListEntry);
		}
	}
}

static
IMG_VOID RemoveInstsInsertedBeforeCombined(PDGRAPH_STATE		psDepState, 
										   PCODEBLOCK			psBlock,
										   PINST				psMoveInstsBefore[4],
										   IMG_UINT				uCombinedInstId)
/*****************************************************************************
 FUNCTION	: RemoveInstsInsertedBeforeCombined
    
 PURPOSE	: Undo the output of instructions that were output before vectorisation
			  (instructions to combine the arguments). Adds these to the dependency
			  graph, to be output once the combined instruction has been output.

 PARAMETERS	: psDepState		- The dependency graph.
			  psBlock			- Block in which to find instructions.
			  psMoveInstsBefore	- List of instructions that were output.
			  uCombinedInstId	- The id of the new, vectorised instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32			uIndex;
	PINTERMEDIATE_STATE psState = psDepState->psState;

	/* 
		Add the MOVs from the old sources to the new to the dependency graph and set up
		dependencies between them and their arguments.
	*/
	for (uIndex = 0; uIndex < 2; ++uIndex)
	{
		if (psMoveInstsBefore[uIndex] != NULL)
		{
			ASSERT(psMoveInstsBefore[uIndex]->psBlock != NULL);

			if (psMoveInstsBefore[uIndex]->psBlock == psBlock)
			{
				IMG_UINT32 uSrc;

				RemoveInst(psState, psBlock, psMoveInstsBefore[uIndex]);

				AddNewInstToDGraph(psDepState, psMoveInstsBefore[uIndex]);
				
				for (uSrc = 0; uSrc < SOURCE_ARGUMENTS_PER_VECTOR; ++uSrc)
				{
					if (psMoveInstsBefore[uIndex]->asArg[uSrc].uType != USC_REGTYPE_UNUSEDSOURCE &&
						psMoveInstsBefore[uIndex]->asArg[uSrc].uType != USEASM_REGTYPE_MAXIMUM)
					{
						PUSEDEF_CHAIN psUseDef = UseDefGet(psState,
														   psMoveInstsBefore[uIndex]->asArg[uSrc].uType, 
														   psMoveInstsBefore[uIndex]->asArg[uSrc].uNumber);

						if (psUseDef != NULL && psUseDef->psDef != NULL && psUseDef->psDef->u.psInst != NULL)
						{
							if (psUseDef->psDef->u.psInst->psBlock == NULL || psUseDef->psDef->u.psInst->psBlock == psBlock)
							{
								AddClosedDependency(psState, psDepState, psUseDef->psDef->u.psInst->uId, psMoveInstsBefore[uIndex]->uId);
							}
						}
					}
				}
			}
		}
	}

	/* 
		Add dependencies between the moves and the new vectorised instruction.
		Add a dependency between the two move instructions, if there are two.
	*/
	if (psMoveInstsBefore[0] != NULL && psMoveInstsBefore[1] == NULL)
	{
		AddClosedDependency(psState, psDepState, psMoveInstsBefore[0]->uId, uCombinedInstId);
	}
	else if (psMoveInstsBefore[1] != NULL && psMoveInstsBefore[1]->psBlock == NULL)
	{
		AddClosedDependency(psState, psDepState, psMoveInstsBefore[1]->uId, uCombinedInstId);

		if (psMoveInstsBefore[0] != NULL && psMoveInstsBefore[0]->psBlock == NULL)
		{
			AddClosedDependency(psState, psDepState, psMoveInstsBefore[0]->uId, psMoveInstsBefore[1]->uId);
		}
	}
}

static
IMG_VOID UndoOutputDependencies(PINTERMEDIATE_STATE	psState, PCODEBLOCK	psBlock, PUSC_LIST psDepInstsOutput)
/*****************************************************************************
 FUNCTION	: UndoOutputDependencies
    
 PURPOSE	: Undo the output of instructions that were output before vectorisation
			  (instructions that the second one depended on).

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block in which to find instructions.
			  psDepInstsOutput	- List of instructions that were output.

 RETURNS	: Nothing.
*****************************************************************************/
{						
	PUSC_LIST_ENTRY	psListEntry;

	for (psListEntry = psDepInstsOutput->psTail; psListEntry != NULL; psListEntry = psListEntry->psPrev)
	{
		PINST psOutputInst = IMG_CONTAINING_RECORD(psListEntry, PINST, sTempListEntry);
		RemoveInst(psState, psBlock, psOutputInst);
	}	
}

static
IMG_VOID AddInstsAfterIntoDepGraph(PDGRAPH_STATE		psDepState, 
								   PCODEBLOCK			psBlock, 
								   PINST				psMoveInstsAfter[VECTOR_MAX_DESTINATION_COUNT*4],
								   IMG_UINT				uCurrentInstId)
/*****************************************************************************
 FUNCTION	: AddInstsAfterIntoDepGraph
    
 PURPOSE	: Add the instructions that were created during vectorisation, to
			  be inserted after the vectorised instruction to the dependency graph,
			  so they can be output once the combined instruction has been output.

 PARAMETERS	: psDepState		- The dependency graph.
			  psBlock			- Block in which the instructions belong
			  psMoveInstsAfter	- The instructions that were created
			  uCurrentInstId	- The id of the instruction that we have been trying to vectorise

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32			uIndex = 0;
	PINTERMEDIATE_STATE psState = psDepState->psState;

	while (psMoveInstsAfter[uIndex] != NULL)
	{
		PUSC_LIST_ENTRY	psListEntry;
		PUSEDEF_CHAIN	psUseDef = UseDefGet(psState,
											 psMoveInstsAfter[uIndex]->asDest[0].uType, 
											 psMoveInstsAfter[uIndex]->asDest[0].uNumber);

		AddNewInstToDGraph(psDepState, psMoveInstsAfter[uIndex]);

		/* Add dependencies between the instruction and those that read its destination. */
		for (psListEntry = psUseDef->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
		{
			PUSEDEF psUseDef	= IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
			PINST	psUseInst	= UseDefGetInst(psUseDef);

			if (psUseInst != psMoveInstsAfter[uIndex] && psUseInst != NULL &&
				(psUseInst->psBlock == NULL || psUseInst->psBlock == psBlock))
			{
				AddClosedDependency(psState, psDepState, psMoveInstsAfter[uIndex]->uId, psUseInst->uId);
			}
		}

		AddClosedDependency(psState, psDepState, uCurrentInstId, psMoveInstsAfter[uIndex]->uId);

		psMoveInstsAfter[uIndex] = NULL;
		++uIndex;
	}
}

static
IMG_BOOL PredUsedInVec(PINTERMEDIATE_STATE psState, IMG_UINT32 uPredReg)
/*****************************************************************************
 FUNCTION	: PredUsedInVec
    
 PURPOSE	: Is a given predicate register used by any instruction as part of
			  a vector of predicate registers?

 PARAMETERS	: psState	- Compiler state.
			  uPredReg	- Predicate register number to look for.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;
	PUSEDEF_CHAIN	psUseDef = UseDefGet(psState, USEASM_REGTYPE_PREDICATE, uPredReg);

	for (psListEntry = psUseDef->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		PINST	psUseInst = UseDefGetInst(psUse);

		if (psUse->eType != USE_TYPE_PREDICATE)
		{
			continue;
		}

		if (GetBit(psUseInst->auFlag, INST_PRED_PERCHAN))
		{
			return IMG_TRUE;
		}
	}

	return IMG_FALSE;
}

static
IMG_VOID SplitVTEST(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: SplitVTEST
    
 PURPOSE	: Split a perchan VTEST into several single channel VTESTs.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- VTEST instruction to split.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uDest;

	ASSERT(psInst->eOpcode == IVTEST);
	ASSERT(psInst->u.psVec->sTest.eChanSel == VTEST_CHANSEL_PERCHAN);

	/* For every live predicate result make a new VTEST instruction. */
	for (uDest = VTEST_PREDICATE_DESTIDX; uDest < VTEST_PREDICATE_DESTIDX+VTEST_PREDICATE_ONLY_DEST_COUNT; ++uDest)
	{
		PINST		psSingleChanInst;
		IMG_UINT32	uSwizSlot, uDestUnused;

		if (psInst->auLiveChansInDest[uDest] == 0)
		{
			continue;
		}

		psSingleChanInst = CopyInst(psState, psInst);
		
		for (uSwizSlot = 0; uSwizSlot < GetSwizzleSlotCount(psState, psInst); uSwizSlot++)
		{
			IMG_UINT32 uSel = GetChan(psInst->u.psVec->auSwizzle[uSwizSlot], uDest);

			psSingleChanInst->u.psVec->auSwizzle[uSwizSlot] = SetChan(USEASM_SWIZZLE(X, Y, Z, W), VTEST_PREDICATE_DESTIDX, uSel);

			psSingleChanInst->u.psVec->asSrcMod[uSwizSlot] = psInst->u.psVec->asSrcMod[uSwizSlot];
		}

		MoveDest(psState, psSingleChanInst, VTEST_PREDICATE_DESTIDX, psInst, uDest);
		MovePartiallyWrittenDest(psState, psSingleChanInst, VTEST_PREDICATE_DESTIDX, psInst, uDest);

		psSingleChanInst->auDestMask[VTEST_PREDICATE_DESTIDX] = USC_ALL_CHAN_MASK;
		psSingleChanInst->auLiveChansInDest[VTEST_PREDICATE_DESTIDX] = USC_ALL_CHAN_MASK;

		/* Set the remaining destination as unused. */
		for (uDestUnused = VTEST_PREDICATE_DESTIDX + 1;
			uDestUnused < VTEST_PREDICATE_DESTIDX+VTEST_PREDICATE_ONLY_DEST_COUNT;
			++uDestUnused)
		{
			SetDestUnused(psState, psSingleChanInst, uDestUnused);
			SetPartiallyWrittenDest(psState, psSingleChanInst, uDestUnused, NULL);
			psSingleChanInst->auLiveChansInDest[uDestUnused] = 0;
		}

		psSingleChanInst->u.psVec->sTest.eChanSel = VTEST_CHANSEL_XCHAN;

		InsertInstAfter(psState, psInst->psBlock, psSingleChanInst, psInst);
	}

	/*
		Remove and free the original instruction.
	*/
	RemoveInst(psState, psInst->psBlock, psInst);
	FreeInst(psState, psInst);
}

static
IMG_BOOL SplitUnnecVTESTs(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: SplitUnnecVTESTs
    
 PURPOSE	: Split perchan VTESTs if their sources are not aligned right and
			  the predicates are never used as a vector.
			  Vectorising VTESTs can be useful even if we have to insert VMOVs doing
			  it, because we may then be able to vectorise instructions using the
			  predicates. However, sometimes this is not successful and then the
			  combined VTESTs will lead to increased predicate register pressure
			  (because of restrictions on the registers that can be allocated to
			  perchan VTESTs) and MOVs may have to be inserted to fix consecutive
			  registers. So undo the vectorisation again.

 PARAMETERS	: psState - Compiler state.

 RETURNS	: Whether any instructions were split.
*****************************************************************************/
{
	IMG_BOOL			bDoneSomething = IMG_FALSE;
	SAFE_LIST_ITERATOR	sIter;

	InstListIteratorInitialize(psState, IVTEST, &sIter);
	for (; InstListIteratorContinue(&sIter);)
	{
		IMG_UINT32	uDest;
		IMG_BOOL	bUsedInVec;
		PINST		psInst = InstListIteratorCurrent(&sIter);

		if (psInst->u.psVec->sTest.eChanSel != VTEST_CHANSEL_PERCHAN)
		{
			InstListIteratorNext(&sIter);
			continue;
		}

		bUsedInVec = IMG_FALSE;

		for (uDest = VTEST_PREDICATE_DESTIDX; uDest < VTEST_PREDICATE_ONLY_DEST_COUNT; ++uDest)
		{
			if (psInst->auLiveChansInDest[uDest] != 0)
			{
				ASSERT(psInst->asDest[uDest].uType == USEASM_REGTYPE_PREDICATE);

				bUsedInVec |= PredUsedInVec(psState, psInst->asDest[uDest].uNumber);

				if (bUsedInVec)
				{
					break;
				}
			}
		}

		if (!bUsedInVec)
		{
			SplitVTEST(psState, psInst);
			InstListIteratorFinalise(&sIter);
			InstListIteratorInitialize(psState, IVTEST, &sIter);
			bDoneSomething = IMG_TRUE;
			continue;
		}

		InstListIteratorNext(&sIter);
	}
	InstListIteratorFinalise(&sIter);

	return bDoneSomething;
}

static
IMG_VOID VectoriseBP(PINTERMEDIATE_STATE	psState,
					 PCODEBLOCK				psBlock,
					 IMG_PVOID				pvNULL)
/*****************************************************************************
 FUNCTION	: VectoriseBP
    
 PURPOSE	: Try to combine instructions which operate on different elements
			  of the same vector.
			  
			  ADD r0 r1.x r2.y
			  ADD r3 r1.y r2.w

 PARAMETERS	: psState	- Compiler state.
			  psBlock	- Block in which to find instructions.
			  pvNULL	- not used.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PDGRAPH_STATE	psDepState;	
	PINST			psInst;
	PINST			psMoveInstsAfter[VECTOR_MAX_DESTINATION_COUNT*4],
					psMoveInstsBefore[4];

	PVR_UNREFERENCED_PARAMETER(pvNULL);
	
	memset(psMoveInstsAfter, 0, sizeof(PINST) * VECTOR_MAX_DESTINATION_COUNT * 4);

	/* Remove all instructions from the block, computing dependency graph. */
	psDepState = ComputeBlockDependencyGraph(psState, psBlock, IMG_FALSE);
	ComputeClosedDependencyGraph(psDepState, IMG_FALSE);
	ClearBlockInsts(psState, psBlock);

	/* Add instructions back as they become available, vectorising with others, if possible. */
	for (psInst = GetNextAvailable(psDepState, NULL); psInst != NULL; psInst = GetNextAvailable(psDepState, NULL))
	{
		PINST		psSecondInst;
		IMG_UINT32	puFirstWrittenChanCount[VECTOR_MAX_DESTINATION_COUNT];
		IMG_UINT32  uIndex;
		IMG_BOOL	bRemove = IMG_TRUE;

		if (((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) != 0) &&
			((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORDEST) != 0) &&
			((g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_VECTORREPLICATEDRESULT) == 0) &&
			psInst->eOpcode != IVMOVCU8_FLT)
		{
			PADJACENCY_LIST			psList;
			ADJACENCY_LIST_ITERATOR	sIter;
			IMG_UINT32				uDepInst, uNextDepInst, uInst;
			PINST					psNext;

			psSecondInst = NULL;

			/* Try combining instruction with that using its dest as olddest */
			psList = (PADJACENCY_LIST)ArrayGet(psState, psDepState->psDepList, psInst->uId);
			for (uDepInst = FirstAdjacent(psList, &sIter); !IsLastAdjacent(&sIter); uDepInst = uNextDepInst)
			{
				uNextDepInst = NextAdjacent(&sIter);
				psSecondInst = ArrayGet(psState, psDepState->psInstructions, uDepInst);
				GetNumberOfChannelsWritten(psInst, puFirstWrittenChanCount);

				if (psSecondInst != NULL && 
					IsOldDestOnlyDependency(psDepState, psInst, psSecondInst) &&
					psInst->uPredCount == 0 &&
					psSecondInst->uPredCount == 0)
				{
					IMG_UINT32 uArgDifferent = UINT_MAX;

					if (AreSimilarOnSameArgs(psState, psInst, psSecondInst) ||
						AreSimilarOnSimilarArgs(psState, psInst, psSecondInst, puFirstWrittenChanCount, &uArgDifferent))
					{
						PINST		psCombinedInst;
						USC_LIST	sDepInstsOutput;
						IMG_BOOL	bSuccess = IMG_TRUE;

						InitializeList(&sDepInstsOutput);

						OutputDependencies(psDepState, psBlock, psSecondInst, &sDepInstsOutput);
						
						/* 
							The first instruction is a dependency, so has been output. 
							Undo that so the algorithm can proceed as expected.
						*/
						RemoveInst(psState, psBlock, psInst);
						RemoveFromList(&sDepInstsOutput, &psInst->sTempListEntry);
						
						if (!ShouldInsertInstNow(psState, psBlock, psInst) || !ShouldInsertInstNow(psState, psBlock, psSecondInst))
						{
							bSuccess = IMG_FALSE;
						}

						psCombinedInst = NULL;
						if (bSuccess)
						{
							psCombinedInst = DoCombination(psBlock, 
														   psInst,
														   psSecondInst, 
														   psMoveInstsAfter,
														   psMoveInstsBefore,
														   psDepState, 
														   uArgDifferent);

							bSuccess = (psCombinedInst != NULL);
						}

						if (bSuccess)
						{
							ASSERT(psCombinedInst != NULL);
							RemoveInstsInsertedBeforeCombined(psDepState, psBlock, psMoveInstsBefore, psCombinedInst->uId);

							psInst = psCombinedInst;
							psList = (PADJACENCY_LIST)ArrayGet(psState, psDepState->psDepList, psInst->uId);
							uNextDepInst = FirstAdjacent(psList, &sIter);

							bRemove = IMG_FALSE;
						}

						UndoOutputDependencies(psState, psBlock, &sDepInstsOutput);
					}
				}
			}
			
			/* Try combining instruction with another with the same arguments */
			for (psSecondInst = GetNextAvailable(psDepState, NULL); 
				psSecondInst != NULL;
				psSecondInst = psNext)
			{
				if (!ShouldInsertInstNow(psState, psBlock, psInst) || !ShouldInsertInstNow(psState, psBlock, psSecondInst))
				{
					break;
				}

				GetNumberOfChannelsWritten(psInst, puFirstWrittenChanCount);
				psNext = GetNextAvailable(psDepState, psSecondInst);				
				
				if (psSecondInst != psInst &&
					AreSimilarOnSameArgs(psState, psInst, psSecondInst))
				{
					PINST psCombinedInst = DoCombination(psBlock,
														 psInst,
														 psSecondInst, 
														 psMoveInstsAfter,
														 psMoveInstsBefore, 
														 psDepState,
														 UINT_MAX);

					if (psCombinedInst != NULL)
					{
						for (uIndex = 0; uIndex < 2; ++uIndex)
						{
							if (psMoveInstsBefore[uIndex] != NULL)
							{
								ASSERT(psMoveInstsBefore[uIndex]->psBlock != NULL);
								EliminateVMOV(psState, psMoveInstsBefore[uIndex], IMG_FALSE);
							}
						}			

						psInst = psCombinedInst;
						psNext = GetNextAvailable(psDepState, NULL);
					}
				}
			}
			
			/* Try combining instruction with another with similar arguments */
			for (psSecondInst = GetNextAvailable(psDepState, NULL); 
				psSecondInst != NULL;
				psSecondInst = psNext)
			{
				IMG_UINT32 uArgDifferent;
				
				if (!ShouldInsertInstNow(psState, psBlock, psInst) || !ShouldInsertInstNow(psState, psBlock, psSecondInst))
				{
					break;
				}

				GetNumberOfChannelsWritten(psInst, puFirstWrittenChanCount);
				psNext = GetNextAvailable(psDepState, psSecondInst);			

				if (psSecondInst != psInst &&
					AreSimilarOnSimilarArgs(psState, psInst, psSecondInst, puFirstWrittenChanCount, &uArgDifferent))
				{
					PINST psCombinedInst = DoCombination(psBlock, 
														 psInst,
														 psSecondInst,
														 psMoveInstsAfter,
														 psMoveInstsBefore, 
														 psDepState, 
														 uArgDifferent);

					if (psCombinedInst != NULL)
					{
						for (uIndex = 0; uIndex < 2; ++uIndex)
						{
							if (psMoveInstsBefore[uIndex] != NULL)
							{
								ASSERT(psMoveInstsBefore[uIndex]->psBlock != NULL);
								EliminateVMOV(psState, psMoveInstsBefore[uIndex], IMG_FALSE);
							}
						}			

						psInst = psCombinedInst;
						psNext = GetNextAvailable(psDepState, NULL);
						
					}
				}
			}

			/* Try combining instruction with another which is not yet ready */
			for (uInst = 0; uInst < psDepState->uBlockInstructionCount; uInst++)
			{
				/* Find an instruction that is not yet scheduled */
				IMG_UINT32 uArgDifferent;
				PINST psSecondInst = ArrayGet(psState, psDepState->psInstructions, uInst);
				GetNumberOfChannelsWritten(psInst, puFirstWrittenChanCount);

				if (psSecondInst != NULL &&
					psSecondInst != psInst &&
					!IsEntryInList(&psDepState->sAvailableList, &psSecondInst->sAvailableListEntry) &&
					!GraphGet(psState, psDepState->psClosedDepGraph, uInst, psInst->uId) &&
					AreSimilarOnSimilarArgs(psState, psInst, psSecondInst, puFirstWrittenChanCount, &uArgDifferent))
				{
					PINST		psCombinedInst;
					USC_LIST	sDepInstsOutput;
					IMG_BOOL	bSuccess = IMG_TRUE;
					
					InitializeList(&sDepInstsOutput);

					OutputDependencies(psDepState, psBlock, psSecondInst, &sDepInstsOutput);
					
					if (!ShouldInsertInstNow(psState, psBlock, psInst) || !ShouldInsertInstNow(psState, psBlock, psSecondInst))
					{
						bSuccess = IMG_FALSE;
					}
					
					psCombinedInst = NULL;
					if (bSuccess)
					{
						psCombinedInst = DoCombination(psBlock, 
													   psInst,
													   psSecondInst,
													   psMoveInstsAfter,
													   psMoveInstsBefore, 
													   psDepState, 
													   uArgDifferent);

						bSuccess = (psCombinedInst != NULL);
					}

					if (bSuccess)
					{
						ASSERT(psCombinedInst != NULL);

						RemoveInstsInsertedBeforeCombined(psDepState, psBlock, psMoveInstsBefore, psCombinedInst->uId);
						psInst = psCombinedInst;

						bRemove = IMG_FALSE;
					}
					
					UndoOutputDependencies(psState, psBlock, &sDepInstsOutput);

					if (psCombinedInst != NULL)
					{
						break;
					}
				}
			}
		}

		if (bRemove)
		{
			/* Add the instruction to the block, as well as all those that we made that need to come after it. */
			RemoveInstruction(psDepState, psInst);
			InsertInstBefore(psState, psBlock, psInst, NULL);

			if (psInst->eOpcode == IVMOV)
			{
				EliminateVMOV(psState, psInst, IMG_FALSE);
			}
			
			uIndex = 0;
			while (psMoveInstsAfter[uIndex] != NULL)
			{
				InsertInstBefore(psState, psBlock, psMoveInstsAfter[uIndex], NULL);
				EliminateVMOV(psState, psMoveInstsAfter[uIndex], IMG_FALSE);

				psMoveInstsAfter[uIndex] = NULL;
				++uIndex;
			}
		}
		else
		{
			/* 
				The new instruction is not yet ready to be output. It will come around again.
				Add the instructions that we need to be output afterwards to the dependency graph
				so they can come around again, too.
			*/
			AddInstsAfterIntoDepGraph(psDepState, psBlock, psMoveInstsAfter, psInst->uId);
		}
	}

	FreeBlockDGraphState(psState, psBlock);
}

IMG_INTERNAL
IMG_VOID Vectorise(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: Vectorise
    
 PURPOSE	: Call VectoriseBP on every block.

 PARAMETERS	: psState - Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		Apply the optimisation to all basic blocks.
	*/
	DoOnAllBasicBlocks(psState, ANY_ORDER, VectoriseBP, IMG_FALSE /* CALLS */, NULL /* pvUserData */);
	
	if (SplitUnnecVTESTs(psState))
	{
		EliminateMoves(psState);
	}

	/*	
		Show the updated intermediate code.
	*/
	TESTONLY_PRINT_INTERMEDIATE("Vectorise", "vectorise", IMG_FALSE);
}

static
IMG_VOID CombineVecAddMoveInst(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: CombineVecAddMoveInst
    
 PURPOSE	: Check for cases where a VADD followed by a VMOV which makes a
			  new vector from the VADD result and one of the VADD sources can
			  be optimized.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction to modify.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG		psDest;
	IMG_UINT32	uMatchedSrc;
	PINST		psMoveInst;
	USEDEF_TYPE	eMoveUseType;
	IMG_UINT32	uMoveUseIdx;
	PARG		psMoveSrc;
	IMG_UINT32	uSrc;

	if (!NoPredicate(psState, psInst))
	{
		return;
	}

	/*
		Check there is only one use of the VADD destination.
	*/
	ASSERT(psInst->uDestCount == 1);
	psDest = &psInst->asDest[0];
	if (!UseDefGetSingleUse(psState, psDest, &psMoveInst, &eMoveUseType, &uMoveUseIdx))
	{
		return;
	}

	/*
		Check the VADD destination is used as the source for unwritten channels in a VMOV
		instruction.
	*/
	if (eMoveUseType != USE_TYPE_OLDDEST)
	{
		return;
	}
	if (psMoveInst->eOpcode != IVMOV)
	{
		return;
	}
	ASSERT(uMoveUseIdx == 0);
	if (!NoPredicate(psState, psMoveInst))
	{
		return;
	}

	ASSERT((psInst->auLiveChansInDest[0] & psMoveInst->auDestMask[0]) == 0);
	ASSERT((psInst->auDestMask[0] & psMoveInst->auDestMask[0]) == 0);

	/*
		Check one of the sources to the VADD is the same as the source to the VMOV.
	*/
	psMoveSrc = &psMoveInst->asArg[0];
	if (psMoveSrc->uType != USEASM_REGTYPE_TEMP)
	{
		return;
	}
	uMatchedSrc = USC_UNDEF;
	for (uSrc = 0; uSrc < 2; uSrc++)
	{
		PARG	psAddSrc = &psInst->asArg[uSrc * SOURCE_ARGUMENTS_PER_VECTOR];

		if (
				EqualArgs(psAddSrc, psMoveSrc) && 
				psInst->u.psVec->asSrcMod[uSrc].bNegate == psMoveInst->u.psVec->asSrcMod[uSrc].bNegate &&
				psInst->u.psVec->asSrcMod[uSrc].bAbs == psMoveInst->u.psVec->asSrcMod[uSrc].bAbs
			)
		{
			uMatchedSrc = uSrc;
			break;
		}
	}
	if (uMatchedSrc == USC_UNDEF)
	{
		return;
	}

	/*
		VADD		a.X, b.Z, c
		VMOV		d.Y[=A], b.W
			->
		VADD		d.XY, b.ZW, c.X0
	*/
	MoveDest(psState, psInst, 0 /* uMoveToDestIdx */, psMoveInst, 0 /* uMoveFromDestIdx */);
	psInst->auLiveChansInDest[0] = psMoveInst->auLiveChansInDest[0];
	psInst->auDestMask[0] |= psMoveInst->auDestMask[0];
	for (uSrc = 0; uSrc < 2; uSrc++)
	{
		IMG_UINT32	uChan;

		if (uSrc == uMatchedSrc)
		{
			/*
				Copy the part of the VMOV instruction's swizzle corresponding to the channels
				written in the destination over the same channels in the VADD instruction's
				swizzle.
			*/
			for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
			{
				if ((psMoveInst->auDestMask[0] & (1 << uChan)) != 0)
				{
					USEASM_SWIZZLE_SEL	eSel;

					eSel = GetChan(psMoveInst->u.psVec->auSwizzle[0], uChan);
					psInst->u.psVec->auSwizzle[uSrc] = SetChan(psInst->u.psVec->auSwizzle[uSrc], uChan, eSel);
				}
			}
		}
		else
		{
			/*
				Select 0 into all the channels written by the VMOV.
			*/
			for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
			{
				if ((psMoveInst->auDestMask[0] & (1 << uChan)) != 0)
				{
					psInst->u.psVec->auSwizzle[uSrc] = 
						SetChan(psInst->u.psVec->auSwizzle[uSrc], uChan, USEASM_SWIZZLE_SEL_0);
				}
			}
		}
	}

	RemoveInst(psState, psMoveInst->psBlock, psMoveInst);
	FreeInst(psState, psMoveInst);
}

IMG_INTERNAL
IMG_VOID CombineVecAddsMoves(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: CombineVecAddsMoves
    
 PURPOSE	: Check for cases where a VADD followed by a VMOV which makes a
			  new vector from the VADD result and one of the VADD sources can
			  be optimized.

 PARAMETERS	: psState	- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		For all VADD instructions in the program...
	*/
	ForAllInstructionsOfType(psState, IVADD, CombineVecAddMoveInst);
}

static
IMG_VOID CombineChannelsInInstr(PINTERMEDIATE_STATE	psState,
							    PINST				psInst,
								IMG_UINT32			puChanMappings[4])								
/*****************************************************************************
 FUNCTION	: CombineChannelsInInstr
    
 PURPOSE	: Combine identical channels in an instructions. E.g.

			  IVMUL r1.xy r2.xx r3.yy
			  ->
			  IVMUL r6.x r2.x r3.x
			  IVMOV r1.y[=r6] r6.x

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify
			  puChanMappings	- A map of old channels in the instruction
								  to new ones.

 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		Replace the destination of the instruction with a new register, 
		reduce the dest mask and
		make a MOV instruction to move the duplicate channel of the instruction
		to the identical one.
	*/
	IMG_UINT32 uChan;
	IMG_UINT32 uTempReg = GetNextRegister(psState);

	PINST psMoveInst = CopyInst(psState, psInst);
	SetOpcode(psState, psMoveInst, IVMOV);
		
	MoveDest(psState, psMoveInst, 0, psInst, 0);

	SetDest(psState, psInst, 0, USEASM_REGTYPE_TEMP, uTempReg, psMoveInst->asDest[0].eFmt);
	SetPartiallyWrittenDest(psState, psMoveInst, 0, &psInst->asDest[0]);
	SetSrcFromArg(psState, psMoveInst, 0, &psInst->asDest[0]);

	psMoveInst->u.psVec->aeSrcFmt[0] = psInst->asDest[0].eFmt;

	psMoveInst->auDestMask[0] = psInst->auDestMask[0];
		
	psMoveInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
	for (uChan = 0; uChan < VECTOR_LENGTH; ++uChan)
	{
		psMoveInst->u.psVec->auSwizzle[0] = SetChan(psMoveInst->u.psVec->auSwizzle[0], uChan, puChanMappings[uChan]);

		if (puChanMappings[uChan] != uChan)
		{
			/* This channel is a duplicate, it can be removed. */
			psInst->auDestMask[0] &= ~(1 << uChan);
		}
		else
		{
			psMoveInst->auDestMask[0] &= ~(1 << uChan);
		}
	}

	InsertInstAfter(psState, psInst->psBlock, psMoveInst, psInst);

	psInst->auLiveChansInDest[0] = GetPreservedChansInPartialDest(psState, psMoveInst, 0 /* uDestIdx */);
	psInst->auLiveChansInDest[0] |= GetLiveChansInArg(psState, psMoveInst, 0 /* uArg */);

	EliminateVMOV(psState, psMoveInst, IMG_FALSE);
}

static
IMG_VOID CombineChannelsBP(PINTERMEDIATE_STATE	psState,
						   PCODEBLOCK			psBlock,
						   IMG_PVOID			pvNULL)
/*****************************************************************************
 FUNCTION	: CombineChannelsBP
    
 PURPOSE	: Combine two channels in instructions which are identical. E.g.

			  IVMUL r1.xy r2.xx r3.yy
			  ->
			  IVMUL r1.x r2.x r3.y

 PARAMETERS	: psState		- Compiler state.
			  psCodeBlock	- Block in which to check instructions
			  pvNull		- Not used

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST psInst, psNext;

	PVR_UNREFERENCED_PARAMETER(pvNULL);

	for (psInst = psBlock->psBody; psInst != NULL; psInst = psNext)
	{
		IMG_UINT32		uChan;
		IMG_BOOL		bChansSame;
		IMG_UINT32		puChanMapping[4];

		psNext = psInst->psNext;
		
		if (((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) == 0) ||
			!InstHasVectorDest(psState, psInst, 0) ||
			psInst->uDestCount != 1)
		{
			continue;
		}

		if (psInst->eOpcode == IVMOV)
		{
			/*
				It's not worth doing this optimisation on moves.
				Plus, they might be moves that we inserted doing this optimisation on
				previous instructions.
			*/
			continue;
		}

		bChansSame = IMG_FALSE;
		puChanMapping[0] = 0;

		for (uChan = VECTOR_LENGTH-1; uChan > 0; --uChan)
		{
			puChanMapping[uChan] = uChan;

			if ((psInst->auDestMask[0] & (1 << uChan)) != 0)
			{
				IMG_UINT32 uSameChan;
				if (ChannelsIdentical(psState, uChan, psInst, psInst, &uSameChan) &&
					uSameChan < uChan)
				{
					puChanMapping[uChan] = uSameChan;
					bChansSame = IMG_TRUE;
				}
			}
		}

		if (!bChansSame)
		{
			continue;
		}

		/* Do the combination */
		CombineChannelsInInstr(psState, psInst, puChanMapping);
	}
}

IMG_INTERNAL
IMG_VOID CombineChannels(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: CombineChannels
    
 PURPOSE	: Call CombineChannelsBP on every block.

 PARAMETERS	: psState - Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		Apply the optimisation to all basic blocks.
	*/
	DoOnAllBasicBlocks(psState, ANY_ORDER, CombineChannelsBP, IMG_FALSE /* CALLS */, NULL /* pvUserData */);

	/*	
		Show the updated intermediate code.
	*/
	TESTONLY_PRINT_INTERMEDIATE("Combine Channels", "combine_chans", IMG_FALSE);
}

static
IMG_VOID CombineDepInstrs(PINTERMEDIATE_STATE	psState,
						  PINST					psFirstInst,
						  PINST					psDepInst,
						  IMG_UINT32			puChanMappings[4])
/*****************************************************************************
 FUNCTION	: CombineDepInstrs
    
 PURPOSE	: Combine two dependant instructions which are identical. E.g.

			  IVLOG r1		r5.x
			  IVLOG r2[=r1] r5.x

 PARAMETERS	: psState			- Compiler state.
			  psFirstInst		- The first instruction.
			  psDepInst			- The dependant instruction.
			  puChanMappings	- A map of channels in the dependant instruction
								  to those in the first instruction that they
								  will be replaced by.

 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		Make a MOV instruction to move the dest of the first instruction to that
		of the dependant one.
	*/
	IMG_UINT32 uChan;
	PINST psMoveInst = CopyInst(psState, psDepInst);
	SetOpcode(psState, psMoveInst, IVMOV);
		
	psMoveInst->u.psVec->aeSrcFmt[0] = psFirstInst->asDest[0].eFmt;

	SetSrc(psState, psMoveInst, 0, USEASM_REGTYPE_TEMP, psFirstInst->asDest[0].uNumber, psFirstInst->asDest[0].eFmt);
	MoveDest(psState, psMoveInst, 0, psDepInst, 0);
		
	psMoveInst->auDestMask[0] = psDepInst->auDestMask[0];
		
	psMoveInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
	for (uChan = 0; uChan < VECTOR_LENGTH; ++uChan)
	{
		psMoveInst->u.psVec->auSwizzle[0] = SetChan(psMoveInst->u.psVec->auSwizzle[0], uChan, puChanMappings[uChan]);
	}

	InsertInstAfter(psState, psDepInst->psBlock, psMoveInst, psDepInst);

	/* Remove the instruction that was replaced */
	RemoveInst(psState, psDepInst->psBlock, psDepInst);
	FreeInst(psState, psDepInst);

	EliminateVMOV(psState, psMoveInst, IMG_FALSE);
}

static
IMG_VOID CombineDependantIdenticalInstsBP(PINTERMEDIATE_STATE	psState,
										  PCODEBLOCK			psBlock,
										  IMG_PVOID				pvNULL)
/*****************************************************************************
 FUNCTION	: CombineDependantIdenticalInstsBP
    
 PURPOSE	: Combine two dependant instructions which are identical. E.g.

			  IVLOG r1		r5.x
			  IVLOG r2[=r1] r5.x

 PARAMETERS	: psState		- Compiler state.
			  psCodeBlock	- Block in which to check instructions
			  pvNull		- Not used

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psInst;	
	IMG_BOOL	bDoneSomething = IMG_TRUE;

	PVR_UNREFERENCED_PARAMETER(pvNULL);

	while (bDoneSomething)
	{
		PINST psNext;

		bDoneSomething = IMG_FALSE;

		for (psInst = psBlock->psBody; psInst != NULL; psInst = psNext)
		{
			PINST			psOldDestDefInst;
			IMG_UINT32		uArg, uChan, uArgsDifferent;
			IMG_BOOL		bModsSame, bChansSame;
			IMG_UINT32		puChanMapping[4];

			psNext = psInst->psNext;

			if (((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) == 0) ||
				((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORDEST) == 0) ||
				psInst->uDestCount != 1 ||
				psInst->apsOldDest[0] == NULL)
			{
				continue;
			}

			psOldDestDefInst = 
				UseDefGetDefInst(psState, psInst->apsOldDest[0]->uType, psInst->apsOldDest[0]->uNumber, NULL /* puDestIdx */);

			if (psOldDestDefInst == NULL || psOldDestDefInst->psBlock != psInst->psBlock)
			{
				continue;
			}

			if ((psInst->eOpcode != psOldDestDefInst->eOpcode) ||
				!EqualPredicates(psInst, psOldDestDefInst) ||
				(CompareInstNonSourceParametersTypeVEC(psInst, psOldDestDefInst) != 0))
			{
				continue;
			}

			GetNumberArgsDifferent(psState, psInst, psOldDestDefInst, &uArgsDifferent, IMG_TRUE);

			if (uArgsDifferent!= 0)
			{
				continue;
			}

			bModsSame = IMG_TRUE;
			for (uArg = 0; uArg < GetSwizzleSlotCount(psState, psInst); ++uArg)
			{
				if (psInst->u.psVec->asSrcMod[uArg].bNegate	!= psOldDestDefInst->u.psVec->asSrcMod[uArg].bNegate ||
					psInst->u.psVec->asSrcMod[uArg].bAbs	!= psOldDestDefInst->u.psVec->asSrcMod[uArg].bAbs)
				{
					bModsSame = IMG_FALSE;
				}
			}

			if (!bModsSame)
			{
				continue;
			}

			bChansSame = IMG_TRUE;

			for (uChan = 0; uChan < VECTOR_LENGTH; ++uChan)
			{
				if ((psInst->auDestMask[0] & (1 << uChan)) != 0)
				{
					IMG_UINT32 uSameChanInFirstInst;
					if (ChannelsIdentical(psState, uChan, psOldDestDefInst, psInst, &uSameChanInFirstInst))
					{
						puChanMapping[uChan] = uSameChanInFirstInst;
					}
					else
					{
						bChansSame = IMG_FALSE;
					}
				}
				else
				{
					puChanMapping[uChan] = uChan;
				}
			}

			if (!bChansSame)
			{
				continue;
			}

			/* Do the combination */
			CombineDepInstrs(psState, psOldDestDefInst, psInst, puChanMapping);
			bDoneSomething = IMG_TRUE;
		}

		if (bDoneSomething)
		{
			MergeIdenticalInstructionsBP(psState, psBlock, NULL);
			CombineChannelsBP(psState, psBlock, NULL);
			MergeIdenticalInstructionsBP(psState, psBlock, NULL);
		}
	}
}

IMG_INTERNAL
IMG_VOID CombineDependantIdenticalInsts(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: CombineDependantIdenticalInsts
    
 PURPOSE	: Call CombineDependantIdenticalInstsBP on every block.

 PARAMETERS	: psState - Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		Apply the optimisation to all basic blocks.
	*/
	DoOnAllBasicBlocks(psState, ANY_ORDER, CombineDependantIdenticalInstsBP, IMG_FALSE /* CALLS */, NULL /* pvUserData */);

	/*	
		Show the updated intermediate code.
	*/
	TESTONLY_PRINT_INTERMEDIATE("Combine Dependant Identical Insts", "combine_dep_ident_insts", IMG_FALSE);
}

static
IMG_VOID ReplaceUnusedArgumentsBP(PINTERMEDIATE_STATE	psState,
								  PCODEBLOCK			psCodeBlock,
								  IMG_PVOID				pvNULL)
/*****************************************************************************
 FUNCTION	: ReplaceUnusedArgumentsBP
    
 PURPOSE	: Replace arguments of instructions in a block that are not used
			  because of constant swizzles, with constant registers.

 PARAMETERS	: psState		- Compiler state.
			  psCodeBlock	- Block in which to check instructions
			  pvNull		- Not used

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psInst;

	PVR_UNREFERENCED_PARAMETER(pvNULL);

	for (psInst = psCodeBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		IMG_UINT32 uSrc;
		
		if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) == 0)
		{
			continue;
		}

		for (uSrc = 0; uSrc < GetSwizzleSlotCount(psState, psInst); ++uSrc)
		{
			if ((psInst->asArg[uSrc*SOURCE_ARGUMENTS_PER_VECTOR].uType != USC_REGTYPE_UNUSEDSOURCE) &&
				(GetLiveChansInArg(psState, psInst, uSrc*SOURCE_ARGUMENTS_PER_VECTOR) == 0))
			{
				SetImmediateVector(psState, psInst, uSrc, 0); 
			}
		}
	}
}

IMG_INTERNAL
IMG_VOID ReplaceUnusedArguments(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: ReplaceUnusedArguments
    
 PURPOSE	: Call ReplaceUnusedArgumentsBP on every block.

 PARAMETERS	: psState - Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		Apply the replacement to all basic blocks.
	*/
	DoOnAllBasicBlocks(psState, ANY_ORDER, ReplaceUnusedArgumentsBP, IMG_FALSE /* CALLS */, NULL /* pvUserData */);

	/*	
		Show the updated intermediate code.
	*/
	TESTONLY_PRINT_INTERMEDIATE("ReplaceUnusedArguments", "replace_unused_args", IMG_FALSE);
}

static
IMG_VOID RegroupVMULComputation(PINTERMEDIATE_STATE	psState, 
								PINST				psFirstInst, 
								PINST				psSecondInst,
								IMG_UINT32			uNonConstSrc1,
								IMG_UINT32			uConstSrc2, 
								IMG_UINT32			uNonConstSrc2)
/*****************************************************************************
 FUNCTION	: RegroupVMULComputations

 PURPOSE	: Swap computation in a VMUL instruction chain.

			  I.e.
			  (a * c1) * c2 -> a* (c1 * c2)

 PARAMETERS	: psState - Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uTempNum;
	PINST		psConvertInst;
	IMG_UINT32	uNonConstSrc2Swizzle;
	IMG_UINT32	uSrc;

	/*
		Expand the swizzle on the result of the first instruction

			swizzle(a * c1) * c2 -> swizzle(a) * swizzle(c1) * c2
	*/
	uNonConstSrc2Swizzle = psSecondInst->u.psVec->auSwizzle[uNonConstSrc2];
	for (uSrc = 0; uSrc < 2; uSrc++)
	{
		psFirstInst->u.psVec->auSwizzle[uSrc] = CombineSwizzles(psFirstInst->u.psVec->auSwizzle[uSrc], uNonConstSrc2Swizzle);
	}
	psSecondInst->u.psVec->auSwizzle[uNonConstSrc2] = USEASM_SWIZZLE(X, Y, Z, W);

	psFirstInst->auLiveChansInDest[0] = GetLiveChansInArg(psState, psSecondInst, uNonConstSrc2 * SOURCE_ARGUMENTS_PER_VECTOR);
	psFirstInst->auDestMask[0] = psFirstInst->auLiveChansInDest[0];

	/*
		MUL			DEST0, CONST_SRC0, SRC1
		MUL			DEST1, DEST0, CONST_SRC2
			->
		MUL			DEST0, CONST_SRC0, CONST_SRC2
		MUL			DEST1, DEST0, SRC1
	*/
	SwapVectorSources(psState, psFirstInst, uNonConstSrc1, psSecondInst, uConstSrc2);

	/*
		Allocate a new F32 precision intermediate register to replace DEST0.
	*/
	uTempNum = GetNextRegister(psState);
	SetDest(psState, psFirstInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uTempNum, UF_REGFORMAT_F32);
	SetSrcFromArg(psState, psSecondInst, uNonConstSrc2 * SOURCE_ARGUMENTS_PER_VECTOR, &psFirstInst->asDest[0]);

	psSecondInst->u.psVec->aeSrcFmt[uNonConstSrc2] = UF_REGFORMAT_F32;

	/*
		Allocate a new F32 precision intermediate register to replace DEST1.
	*/
	uTempNum = GetNextRegister(psState);

	/*
		Copy from the new intermediate register to DEST1.

			VMUL	DEST1, ...
		->
			VMUL	NEWTEMP, ...
			MOV		DEST1, NEWTEMP
	*/
	psConvertInst = AllocateInst(psState, psSecondInst);
	SetOpcode(psState, psConvertInst, IVMOV);
	SetSrc(psState, psConvertInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uTempNum, UF_REGFORMAT_F32);
	MoveDest(psState, psConvertInst, 0 /* uMoveToDestIdx */, psSecondInst, 0 /* uMoveFromDestIdx */);
	MovePartiallyWrittenDest(psState, psConvertInst, 0 /* uCopyToDestIdx */, psSecondInst, 0 /* uCopyFromDestIdx */);
	CopyPredicate(psState, psConvertInst, psSecondInst);

	psConvertInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F32;
	psConvertInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
	psConvertInst->auLiveChansInDest[0] = psSecondInst->auLiveChansInDest[0];
	psConvertInst->auDestMask[0] = psSecondInst->auDestMask[0];

	InsertInstAfter(psState, psFirstInst->psBlock, psConvertInst, psSecondInst);

	SetDest(psState, psSecondInst, 0, USEASM_REGTYPE_TEMP, uTempNum, UF_REGFORMAT_F32);
}


IMG_INTERNAL
IMG_VOID RegroupVMULComputations(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: RegroupVMULComputations

 PURPOSE	: Find a VMUL instruction chain which would benefit from being 
			  computed in a different order, because then one multiply can be
			  performed in the seconday update. The multiplication must be f16,
			  so we can promote to f32 to avoid differing behaviour.

			  I.e.
			  (a * c1) * c2 -> a* (c1 * c2)

 PARAMETERS	: psState - Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	SAFE_LIST_ITERATOR	sIter;

	InstListIteratorInitialize(psState, IVMUL, &sIter);
	for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
	{
		IMG_UINT32	uSecondInstSrc, uFirstInstSrc, uFirstInstConstSrc, uFirstInstNonConstSrc;
		IMG_UINT32	uSecondInstConstSrc, uSecondInstNonConstSrc;
		IMG_BOOL	bFoundInsts = IMG_FALSE;
		PINST		psSecondInst, psFirstInst;

		psSecondInst = psFirstInst = NULL;
		uFirstInstConstSrc = uSecondInstConstSrc = uFirstInstNonConstSrc = uSecondInstNonConstSrc = 0;

		psSecondInst = InstListIteratorCurrent(&sIter);

		if (psSecondInst->uDestCount != 1)
		{
			continue;
		}

		/* Check whether one of the arguments is a constant */
		for (uSecondInstSrc = 0; uSecondInstSrc < GetSwizzleSlotCount(psState, psSecondInst); uSecondInstSrc++)
		{
			PARG		psSecondInstSrc = &psSecondInst->asArg[uSecondInstSrc*SOURCE_ARGUMENTS_PER_VECTOR];
			PARG		psSecondInstNonConstSrc;
			IMG_UINT32	uLiveChansInArgPreSwizzle;

			/*
				Check if this argument is a constant in F16 format.
			*/
			if (psSecondInstSrc->uType == USC_REGTYPE_UNUSEDSOURCE) 
			{
				break;
			}
			if (psSecondInstSrc->eFmt != UF_REGFORMAT_F16)
			{
				break;
			}
			if (!IsConstantArgument(psState, psSecondInstSrc))
			{
				continue;
			}

			/* Check whether the other argument is an instructions of the same type */
			uSecondInstNonConstSrc = (uSecondInstSrc + 1) % 2;
			psSecondInstNonConstSrc = &psSecondInst->asArg[uSecondInstNonConstSrc*SOURCE_ARGUMENTS_PER_VECTOR];

			if (psSecondInstNonConstSrc->uType == USC_REGTYPE_UNUSEDSOURCE) 
			{
				break;
			}

			if (psSecondInstNonConstSrc->eFmt != UF_REGFORMAT_F16)
			{
				break;
			}

			/*
				Check the second instruction isn't swizzling constants into the result of
				the first instruction.
			*/
			GetLiveChansInSourceSlot(psState, 
									 psSecondInst, 
									 uSecondInstNonConstSrc,
									 &uLiveChansInArgPreSwizzle, 
									 NULL /* puChansUsedPostSwizzle */);
			if (!IsNonConstantSwizzle(psSecondInst->u.psVec->auSwizzle[uSecondInstNonConstSrc], 
									  uLiveChansInArgPreSwizzle,
									  NULL /* puMatchedSwizzle */))
			{
				break;
			}

			/*
				Check the other argument to the second instruction is used only here.
			*/
			if (!UseDefIsSingleSourceRegisterUse(psState, 
												 psSecondInst, 
												 uSecondInstNonConstSrc*SOURCE_ARGUMENTS_PER_VECTOR))
			{
				break;
			}

			/*
				Check the instruction which defines the other argument.
			*/
			psFirstInst = 
				UseDefGetDefInst(psState, psSecondInstNonConstSrc->uType, psSecondInstNonConstSrc->uNumber, NULL /* puDestIdx */);
			if (psFirstInst == NULL)
			{
				break;
			}

			if (psFirstInst->eOpcode != psSecondInst->eOpcode)
			{
				break;
			}

			if (!EqualPredicates(psFirstInst, psSecondInst))
			{
				break;
			}
			
			if (psFirstInst->uDestCount != 1)
			{
				continue;
			}
			
			if (psFirstInst->auLiveChansInDest[0] != USC_X_CHAN_MASK &&
				psFirstInst->auLiveChansInDest[0] != USC_Y_CHAN_MASK &&
				psFirstInst->auLiveChansInDest[0] != USC_Z_CHAN_MASK &&
				psFirstInst->auLiveChansInDest[0] != USC_W_CHAN_MASK)
			{
				continue;
			}

			if (psFirstInst->psBlock != psSecondInst->psBlock)
			{
				continue;
			}

			/* Check one argument of this instruction is also constant. */
			for (uFirstInstSrc = 0; uFirstInstSrc < GetSwizzleSlotCount(psState, psFirstInst); uFirstInstSrc++)
			{
				PARG		psFirstInstSrc = &psFirstInst->asArg[uFirstInstSrc * SOURCE_ARGUMENTS_PER_VECTOR];
				IMG_UINT32	uFirstInstOtherSrc = 1 - uFirstInstSrc;
				PARG		psFirstInstOtherSrc = &psFirstInst->asArg[uFirstInstOtherSrc * SOURCE_ARGUMENTS_PER_VECTOR];

				if (psFirstInstSrc->uType == USC_REGTYPE_UNUSEDSOURCE) 
				{
					break;
				}

				if (psFirstInstSrc->eFmt != UF_REGFORMAT_F16)
				{
					break;
				}

				/*
					Check if the other argument is also a constant (in which there's no point in alterting
					the multiplies).
				*/
				if (IsConstantArgument(psState, psFirstInstOtherSrc))
				{
					continue;
				}

				if (IsConstantArgument(psState, psFirstInstSrc))
				{
					bFoundInsts = IMG_TRUE;
					uSecondInstConstSrc = uSecondInstSrc;
					uFirstInstConstSrc = uFirstInstSrc;
					uFirstInstNonConstSrc = uFirstInstOtherSrc;
					break;
				}
			}

			if (bFoundInsts)
			{
				break;
			}
		}

		if (bFoundInsts)
		{
			ASSERT(psSecondInst != NULL && psFirstInst != NULL);
			RegroupVMULComputation(psState, 
								   psFirstInst, 
								   psSecondInst, 
								   uFirstInstNonConstSrc, 
								   uSecondInstConstSrc, 
								   uSecondInstNonConstSrc);
		}
	}
	InstListIteratorFinalise(&sIter);
}


static
IMG_UINT32 ShiftSwizzleChans(PINTERMEDIATE_STATE	psState,
							 IMG_UINT32				uReferencedChanMask,
							 IMG_UINT32				uChanShift,
							 IMG_UINT32				uOldSwizzle)
/*****************************************************************************
 FUNCTION	: ShiftSwizzleChans
    
 PURPOSE	: Generate a new swizzle where the old swizzle selected non-constant
			  channel N the new swizzle selects channel N-SHIFT.

 PARAMETERS	: psState			- Compiler state.
			  uReferencedChanMask
								- Mask of selectors used from the swizzle.
			  uChanShift		- Count of channels to shift by.
			  uOldSwizzle		- Swizzle to shift.

 RETURNS	: The shifted swizzle.
*****************************************************************************/
{
	IMG_UINT32	uNewSwizzle;
	IMG_UINT32	uChan;

	uNewSwizzle = 0;
	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		IMG_UINT32	uSel;

		if ((uReferencedChanMask & (1 << uChan)) != 0)
		{
			uSel = GetChan(uOldSwizzle, uChan);
			if (uSel >= USEASM_SWIZZLE_SEL_X && uSel <= USEASM_SWIZZLE_SEL_W)
			{
				IMG_UINT32	uChanOff;

				uChanOff = uSel - USEASM_SWIZZLE_SEL_X;

				ASSERT(uChanOff >= uChanShift);
				uChanOff -= uChanShift;

				uSel = uChanOff + USEASM_SWIZZLE_SEL_X;
			}

			uNewSwizzle = SetChan(uNewSwizzle, uChan, uSel);
		}
	}
	return uNewSwizzle;
}


typedef struct _NORMALISE_CHAN_STATUS
{
	/*
		List of subsets of channels from the vector which are used or define together in
		at least one instruction.
	*/
	IMG_UINT32		auSubsets[VECTOR_LENGTH];
	/*
		Count of subsets of channels from the vector
	*/
	IMG_UINT32		uSubsetCount;
	/*
		TRUE if either the intermediate register is seperated into two or more subsets
		of channels which are used independent or there are unused channels in the
		middle or at the start of the used channels.
	*/
	IMG_BOOL		bNormalisable;
	/*
		TRUE if an intermediate register from the set is used in some context which makes
		it impossible to pack the used channels.
	*/
	IMG_BOOL		bInvalid;
} NORMALISE_CHAN_STATUS, *PNORMALISE_CHAN_STATUS;

typedef struct _NORMALISE_VECTOR_SET
{
	/*
		List of intermediate registers which must be normalised in the same way. For
		each partial definition the source and destination are members of the same set. For
		each SSA choice function the sources and destination are members of the same set.
	*/
	USC_LIST				sSetList;
	/*
		Count of intermediate registers in the set.
	*/
	IMG_UINT32				uSetCount;
	/*
		Information about unused/seperable channels in the intermediate registers.
	*/
	NORMALISE_CHAN_STATUS	sChans;
	/*
		Entry in the list of sets of intermediate registers which are unnormalised.
	*/
	USC_LIST_ENTRY			sShiftableVectorsListEntry;
} NORMALISE_VECTOR_SET, *PNORMALISE_VECTOR_SET;

typedef struct _NORMALISE_VECTOR
{
	/*
		Points to set of intermediate registers which this intermediate register is a 
		member of.
	*/
	PNORMALISE_VECTOR_SET	psSet;
	/*
		Entry in the list of intermediate registers in the same set.
	*/
	USC_LIST_ENTRY			sSetListEntry;
	/*
		List of uses/defines of the intermediate registers.
	*/
	PUSEDEF_CHAIN			psUseDefChain;
} NORMALISE_VECTOR, *PNORMALISE_VECTOR;

typedef struct _NORMALISE_VECTOR_KEY
{
	IMG_UINT32			uTempNum;
	PNORMALISE_VECTOR	psVector;
} NORMALISE_VECTOR_KEY, *PNORMALISE_VECTOR_KEY;

typedef struct _NORMALISE_VECTORS_CONTEXT
{
	PINTERMEDIATE_STATE	psState;

	/*
		List of new moves inserted during the normalisation pass.
	*/
	USC_LIST			sNewMoveList;

	/*
		Map from intermediate register numbers to information about the unused parts of a vector.
	*/
	PUSC_TREE			psVarMap;

	/*
		List of all sets of vectors with unused, leading components.
	*/
	USC_LIST			sShiftableVectorsList;
} NORMALISE_VECTORS_CONTEXT, *PNORMALISE_VECTORS_CONTEXT;

#define INST_INNEWMOVELIST			INST_LOCAL0

static
IMG_VOID AppendToNewMoveList(PNORMALISE_VECTORS_CONTEXT psContext, PINST psInst)
/*****************************************************************************
 FUNCTION	: AppendToNewMoveList
    
 PURPOSE	: Append a move instruction to the list of move instruction
			  generated during the normalise vectors pass.

 PARAMETERS	: psContext			- Pass state.
			  psInst			- Move instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (GetBit(psInst->auFlag, INST_INNEWMOVELIST) == 0)
	{
		SetBit(psInst->auFlag, INST_INNEWMOVELIST, 1);
		AppendToList(&psContext->sNewMoveList, &psInst->sTempListEntry);
	}
}

static
IMG_VOID IsReducedToUnswizzledMove(PNORMALISE_VECTORS_CONTEXT psContext, PINST psInst)
/*****************************************************************************
 FUNCTION	: IsReducedToUnswizzledMove
    
 PURPOSE	: Check if normalisation of vector instructions has reduced a vector
			  move instruction so it is no longer applying a swizzle. If so then
			  append it to the list of moves to replace.

 PARAMETERS	: psContext			- Pass state.
			  psInst			- Instruction to check.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psInst->eOpcode == IVMOV && 
		CompareSwizzles(psInst->u.psVec->auSwizzle[0], USEASM_SWIZZLE(X, Y, Z, W), psInst->auDestMask[0]))
	{
		AppendToNewMoveList(psContext, psInst);
	}
}

static
IMG_VOID PackVector(PINTERMEDIATE_STATE			psState, 
					PNORMALISE_VECTORS_CONTEXT	psContext, 
					PUSEDEF_CHAIN				psVector, 
					IMG_UINT32					uPackMask)
/*****************************************************************************
 FUNCTION	: PackVector
    
 PURPOSE	: Pack uses and defines of a vector intermediate register so the
			  used channels are consecutive and start from X.

 PARAMETERS	: psState			- Compiler state.
			  psContext			- Pass state.
			  psVector			- Register to shift.
			  uPackMask			- Mask of used channels from the register.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF			psDef;
	PUSC_LIST_ENTRY	psListEntry;
	IMG_UINT32		uChan;
	IMG_UINT32		uPackSwizzle;
	IMG_UINT32		uUnpackSwizzle;
	IMG_UINT32		uUsedChanCount;

	/*
		Make a swizzle packing the channels from the vector.
	*/
	uPackSwizzle = uUnpackSwizzle = USEASM_SWIZZLE(0, 0, 0, 0);
	uUsedChanCount = 0;
	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		if ((uPackMask & (1 << uChan)) != 0)
		{
			uUnpackSwizzle = SetChan(uUnpackSwizzle, uUsedChanCount, uChan);
			uPackSwizzle = SetChan(uPackSwizzle, uChan, uUsedChanCount);
			uUsedChanCount++;
		}
	}

	psDef = psVector->psDef;
	if (psDef != NULL)
	{
		PINST			psDefInst;

		ASSERT(psDef->eType == DEF_TYPE_INST);
		ASSERT(psDef->uLocation == 0);
		psDefInst = psDef->u.psInst;

		/*
			When changing the destination mask adjust the swizzles on the sources.
		*/
		if (psDefInst->eOpcode != IDELTA)
		{
			ASSERT(g_psInstDesc[psDefInst->eOpcode].eType == INST_TYPE_VEC);
			if ((g_psInstDesc[psDefInst->eOpcode].uFlags2 & DESC_FLAGS2_VECTORREPLICATEDRESULT) == 0)
			{
				IMG_UINT32	uSlot;

				for (uSlot = 0; uSlot < GetSwizzleSlotCount(psState, psDefInst); uSlot++)
				{
					psDefInst->u.psVec->auSwizzle[uSlot] = CombineSwizzles(psDefInst->u.psVec->auSwizzle[uSlot], uUnpackSwizzle);
				}
			}
	
			ASSERT((psDefInst->auDestMask[0] & ~uPackMask) == 0);
			psDefInst->auDestMask[0] = ApplySwizzleToMask(uPackSwizzle, psDefInst->auDestMask[0]);

			IsReducedToUnswizzledMove(psContext, psDefInst);
		}

		ASSERT((psDefInst->auLiveChansInDest[0] & ~uPackMask) == 0);
		psDefInst->auLiveChansInDest[0] = ApplySwizzleToMask(uPackSwizzle, psDefInst->auLiveChansInDest[0]);
	}

	/*
		Shift the channels in all uses of the register as a source.
	*/
	for (psListEntry = psVector->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUse;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		if (psUse == psVector->psDef)
		{
			continue;
		}

		if (psUse->eType == USE_TYPE_FIXEDREG)
		{
			IMG_UINT32		uOldMask;
			IMG_UINT32		uNewMask;
			PFIXED_REG_DATA	psFixedReg = psUse->u.psFixedReg;
			PVREGISTER		psVectorVReg;

			ASSERT(!psFixedReg->bPrimary);
			ASSERT(psUse->uLocation == 0);
			ASSERT(psFixedReg->uConsecutiveRegsCount == 1);

			/*
				Get the channels live out of the secondary program.
			*/
			uOldMask = GetRegisterLiveMask(psState,
										   &psState->psSecAttrProg->sCfg.psExit->sRegistersLiveOut,
										   USEASM_REGTYPE_TEMP,
										   psVector->uNumber,
										   0 /* uArrayOffset */);

			/*
				Shift the mask of used channels down.
			*/
			ASSERT((uOldMask & ~uPackMask) == 0);
			uNewMask = ApplySwizzleToMask(uPackSwizzle, uOldMask);

			/*
				Update the channels live out of the secondary program.
			*/
			SetRegisterLiveMask(psState,
								&psState->psSecAttrProg->sCfg.psExit->sRegistersLiveOut,
								USEASM_REGTYPE_TEMP,
								psVector->uNumber,
								0 /* uArrayOffset */,
								uNewMask);

			psVectorVReg = GetVRegister(psState, USEASM_REGTYPE_TEMP, psVector->uNumber);
			SetChansUsedFromSAProgResult(psState, psVectorVReg->psSecAttr);
		}
		else if (psUse->eType == USE_TYPE_SRC)
		{
			PINST		psUseInst = psUse->u.psInst;

			if (psUseInst->eOpcode != IDELTA)
			{
				IMG_UINT32	uSlot;

				ASSERT(g_psInstDesc[psUseInst->eOpcode].eType == INST_TYPE_VEC);
				ASSERT((psUse->uLocation % SOURCE_ARGUMENTS_PER_VECTOR) == 0);
				uSlot = psUse->uLocation / SOURCE_ARGUMENTS_PER_VECTOR;
	
				psUseInst->u.psVec->auSwizzle[uSlot] = CombineSwizzles(uPackSwizzle, psUseInst->u.psVec->auSwizzle[uSlot]); 

				/*
					Check if we've turned a vector move with a swizzle into one without.
				*/
				IsReducedToUnswizzledMove(psContext, psUseInst);
			}
		}
		else
		{
			ASSERT(psUse->eType == USE_TYPE_OLDDEST);
		}
	}
}

static
IMG_VOID NormaliseNonVectorRegistersBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvChanges)
/*****************************************************************************
 FUNCTION	: NormaliseNonVectorRegistersBP
    
 PURPOSE	: Modify references to non-vector registers in vector instructions
			  so the minimum number of registers are used.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to process.
			  pvChanges			- Set to TRUE on return if some instructions
								were modified.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psInst;
	IMG_PBOOL	pbChanges = (IMG_PBOOL)pvChanges;

	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) != 0)
		{
			IMG_UINT32	uSrcIdx;
		
			for (uSrcIdx = 0; uSrcIdx < GetSwizzleSlotCount(psState, psInst); uSrcIdx++)
			{
				PARG	psVectorSrc = &psInst->asArg[uSrcIdx * SOURCE_ARGUMENTS_PER_VECTOR];

				if (psVectorSrc->uType == USC_REGTYPE_UNUSEDSOURCE)
				{
					IMG_UINT32		uChansUsed = GetLiveChansInArg(psState, psInst, uSrcIdx * SOURCE_ARGUMENTS_PER_VECTOR);
					IMG_UINT32		uNumLeadingZeros = g_auNumLeadingZeros[uChansUsed];
					IMG_UINT32		uScalarSrcStart = uSrcIdx * SOURCE_ARGUMENTS_PER_VECTOR + 1;
					IMG_UINT32		uChan;
					IMG_UINT32		uChansUsedPreSwizzle;
					UF_REGFORMAT	eFmt = psInst->u.psVec->aeSrcFmt[uSrcIdx];
					IMG_UINT32		uSrcShift;
					IMG_UINT32		uChanShift;
					IMG_UINT32		uNumArgs;

					if (eFmt == UF_REGFORMAT_F32)
					{
						uSrcShift = uNumLeadingZeros;
						uChanShift = uNumLeadingZeros;
						uNumArgs = SCALAR_REGISTERS_PER_F32_VECTOR;
					}
					else
					{
						ASSERT(eFmt == UF_REGFORMAT_F16);
						uSrcShift = uNumLeadingZeros / F16_CHANNELS_PER_SCALAR_REGISTER;
						uChanShift = uSrcShift * F16_CHANNELS_PER_SCALAR_REGISTER;
						uNumArgs = SCALAR_REGISTERS_PER_F16_VECTOR;
					}

					if (uSrcShift > 0)
					{
						*pbChanges = IMG_TRUE;

						/*
							Shift the array of scalar sources down.
						*/
						for (uChan = 0; uChan < (uNumArgs - uSrcShift); uChan++)
						{
							MoveSrc(psState,
									psInst,
									uScalarSrcStart + uChan,
									psInst,
									uScalarSrcStart + uChan + uSrcShift);

						}
						/*
							Set the new sources at the end of the vector to unreferenced.
						*/
						for (; uChan < uNumArgs; uChan++)
						{
							SetSrcUnused(psState, psInst, uScalarSrcStart + uChan);
						}

						/*
							Update the swizzle so the channels used from this source are shifted down.
						*/ 
						GetLiveChansInSourceSlot(psState, 
												 psInst, 
												 uSrcIdx, 
												 &uChansUsedPreSwizzle, 
												 NULL /* puChansUsedPostSwizzle */);
						psInst->u.psVec->auSwizzle[uSrcIdx] = 
							ShiftSwizzleChans(psState, 
											  uChansUsedPreSwizzle, 
											  uChanShift,
											  psInst->u.psVec->auSwizzle[uSrcIdx]);
					}
					
					/*
						Set the unused sources at the beginning of the vector to unreferenced.
					*/
					DropUnusedVectorComponents(psState, psInst, uSrcIdx);
				}
			}
		}
	}
}

static
IMG_VOID InitializeChanStatus(PNORMALISE_CHAN_STATUS psStatus)
/*****************************************************************************
 FUNCTION	: InitializeChanStatus
    
 PURPOSE	: Initialize information about which sets of channels are used
			  together.

 PARAMETERS	: psStatus		- Data structure to initialize.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psStatus->uSubsetCount = 0;
	psStatus->bNormalisable = IMG_FALSE;
	psStatus->bInvalid = IMG_FALSE;
}

static
IMG_VOID AddToChanStatus(PNORMALISE_CHAN_STATUS psStatus, IMG_UINT32 uNewSubset)
/*****************************************************************************
 FUNCTION	: AddToChanStatus
    
 PURPOSE	: Add the information that a set of channels are used together so
			  the existing information for a vector intermediate register.

 PARAMETERS	: psStatus		- Information to merge into.
			  uNewSubset	- Mask of used channels.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32				uMoveFrom, uMoveTo;
	IMG_BOOL				bFirst;
	IMG_UINT32				uFirstMerge;

	if (uNewSubset == 0)
	{
		return;
	}

	uMoveTo = 0; 
	bFirst = IMG_TRUE;
	uFirstMerge = USC_UNDEF;
	for (uMoveFrom = 0; uMoveFrom < psStatus->uSubsetCount; uMoveFrom++)
	{
		IMG_UINT32	uExistingSubset = psStatus->auSubsets[uMoveFrom];

		if ((uExistingSubset & uNewSubset) != 0)
		{
			/*
				Merge all subsets which overlap with the new one into a
				single subset.
			*/
			if (bFirst)
			{
				uFirstMerge = uMoveTo;
				psStatus->auSubsets[uMoveTo] = uExistingSubset | uNewSubset;
				uMoveTo++;
				bFirst = IMG_FALSE;
			}
			else
			{
				psStatus->auSubsets[uFirstMerge] |= uExistingSubset;
			}
		}
		else
		{
			/*
				Just copy existing subsets which don't overlap with the new one.
			*/
			psStatus->auSubsets[uMoveTo] = uExistingSubset;
			uMoveTo++;
		}
	}
	/*
		If the new subset doesn't overlap with any of the existing ones then add it
		to the end of the list.
	*/
	if (bFirst)
	{
		psStatus->auSubsets[uMoveTo++] = uNewSubset;
	}

	psStatus->uSubsetCount = uMoveTo;

	/*
		Flag intermediate registers which we can pack.
	*/
	if (
			psStatus->bInvalid || 
			psStatus->uSubsetCount == 0 || 
			(psStatus->uSubsetCount == 1 && g_abUnusedChannels[psStatus->auSubsets[0]] == 0)
	   )
	{
		psStatus->bNormalisable = IMG_FALSE;
	}
	else
	{
		psStatus->bNormalisable = IMG_TRUE;
	}
}

static
IMG_VOID MergeSubset(PNORMALISE_VECTORS_CONTEXT	psContext, PNORMALISE_VECTOR_SET psDest, IMG_UINT32 uNewSubset)
/*****************************************************************************
 FUNCTION	: MergeSubset
    
 PURPOSE	: Add the information that a set of channels are used together so
			  the existing information for a vector intermediate register.

 PARAMETERS	: psContext		- State for the current pass.
			  psDest		- Information to merge into.
			  uNewSubset	- Mask of used channels.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_BOOL	bOldNormalisable;

	bOldNormalisable = psDest->sChans.bNormalisable;
	AddToChanStatus(&psDest->sChans, uNewSubset);
	if (bOldNormalisable && !psDest->sChans.bNormalisable)
	{
		RemoveFromList(&psContext->sShiftableVectorsList, &psDest->sShiftableVectorsListEntry);
	}
	else if (!bOldNormalisable && psDest->sChans.bNormalisable)
	{
		AppendToList(&psContext->sShiftableVectorsList, &psDest->sShiftableVectorsListEntry);
	}
}

static
IMG_VOID SetVectorUnnormalisable(PNORMALISE_VECTORS_CONTEXT	psContext,
								 PNORMALISE_VECTOR_SET		psVectorSet)
/*****************************************************************************
 FUNCTION	: SetVectorUnnormalisable
    
 PURPOSE	: Flag that a set of vector intermediate registers can't be
			  packed to remove unused channels.

 PARAMETERS	: psContext		- State for the normalisation pass.
			  psVectorSet	- Set of vector intermediate registers.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psVectorSet->sChans.bNormalisable)
	{
		RemoveFromList(&psContext->sShiftableVectorsList, &psVectorSet->sShiftableVectorsListEntry);
		psVectorSet->sChans.bNormalisable = IMG_FALSE;
	}
	psVectorSet->sChans.bInvalid = IMG_TRUE;
}

static
IMG_VOID MergeNormalisations(PNORMALISE_VECTORS_CONTEXT	psContext,
							 PNORMALISE_VECTOR_SET		psDest,	
							 PNORMALISE_CHAN_STATUS		psSrc)
/*****************************************************************************
 FUNCTION	: MergeNormalisations
    
 PURPOSE	: Merge the information about the used subsets of channels from
			  two sets of vector intermediate registers.

 PARAMETERS	: psContext	- State for the normalisation pass.
			  psDest	- Destination to merge into.
			  psSrc		- Source to merge from.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uSrcSubset;

	if (psSrc->bInvalid)
	{
		SetVectorUnnormalisable(psContext, psDest);
		return;
	}

	for (uSrcSubset = 0; uSrcSubset < psSrc->uSubsetCount; uSrcSubset++)
	{
		MergeSubset(psContext, psDest, psSrc->auSubsets[uSrcSubset]);
	}
}

static
IMG_VOID MergeVectors(PINTERMEDIATE_STATE			psState, 
					  PNORMALISE_VECTORS_CONTEXT	psContext,
					  PNORMALISE_VECTOR				psVector, 
					  PARG							psArg)
/*****************************************************************************
 FUNCTION	: MergeVectors
    
 PURPOSE	: Mark two sets of vector intermediate registers as requiring to
			  be shifted by exactly the same amount.

 PARAMETERS	: psState		- Compiler state.
			  psContext		- Pass state.
			  psVector		- Member of the first set to merge.
			  psArg			- Member of the second set to merge.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PNORMALISE_VECTOR_SET	psVectorSet = psVector->psSet;

	if (psArg->uType != USEASM_REGTYPE_TEMP)
	{
		SetVectorUnnormalisable(psContext, psVectorSet);
	}
	else
	{
		PNORMALISE_VECTOR_SET	psOtherVectorSet;
		PNORMALISE_VECTOR_KEY	psOtherVector;
		NORMALISE_VECTOR_KEY	sKey;

		/*
			Get information about the possible normalisations for the intermediate
			register used in the source/destination argument.
		*/
		sKey.uTempNum = psArg->uNumber;
		psOtherVector = UscTreeGetPtr(psContext->psVarMap, &sKey);
		psOtherVectorSet = psOtherVector->psVector->psSet;

		if (psOtherVectorSet != psVectorSet)
		{
			PUSC_LIST_ENTRY		psListEntry;

			/*	
				Set the possible normalisations for the merged set to the minimum of the possible
				normalisations for both sets.
			*/
			MergeNormalisations(psContext, psVectorSet, &psOtherVectorSet->sChans);
	
			/*
				Move all the intermediate registers from the second set to the first.
			*/
			while ((psListEntry = RemoveListHead(&psOtherVectorSet->sSetList)) != NULL)
			{
				PNORMALISE_VECTOR	psSetMember;

				psSetMember = IMG_CONTAINING_RECORD(psListEntry, PNORMALISE_VECTOR, sSetListEntry);
				ASSERT(psSetMember->psSet == psOtherVectorSet);
				psSetMember->psSet = psVectorSet;
				AppendToList(&psVectorSet->sSetList, &psSetMember->sSetListEntry);
			}
			psVectorSet->uSetCount += psOtherVectorSet->uSetCount;

			/*
				Free the second set.
			*/
			if (psOtherVectorSet->sChans.bNormalisable)
			{
				RemoveFromList(&psContext->sShiftableVectorsList, &psOtherVectorSet->sShiftableVectorsListEntry);
			}
			UscFree(psState, psOtherVectorSet);
		}
	}
}

static
IMG_VOID CheckVectorLength(PNORMALISE_VECTORS_CONTEXT	psContext,
						   PNORMALISE_VECTOR			psVectorData)
/*****************************************************************************
 FUNCTION	: CheckVectorLength
    
 PURPOSE	: Check if a vector intermediate register has some unused components.

 PARAMETERS	: psContext		- Pass state.
			  psVectorData	- Intermediate register to check for.

 RETURNS	: Nothing.
*****************************************************************************/
{	
	PUSC_LIST_ENTRY		psListEntry;
	PINTERMEDIATE_STATE	psState = psContext->psState;
	PUSEDEF_CHAIN		psVector;
	PUSEDEF				psVarDef;

	psVector = psVectorData->psUseDefChain;
	psVarDef = psVector->psDef;

	if (psVarDef != NULL && psVarDef->eType == DEF_TYPE_INST)
	{
		PINST		psDefInst;
		IMG_UINT32	uLiveChansInDest;

		psDefInst = psVarDef->u.psInst;
		uLiveChansInDest = psDefInst->auLiveChansInDest[psVarDef->uLocation];

		/*
			For SSA choice functions we need to shift all sources and the destination by the same amount.
		*/
		if (psDefInst->eOpcode == IDELTA)
		{
			IMG_UINT32	uSrc;

			for (uSrc = 0; uSrc < psDefInst->uArgumentCount; uSrc++)
			{
				MergeVectors(psState, psContext, psVectorData, &psDefInst->asArg[uSrc]);
			}
		}
		else
		{
			/*
				If the defining instruction isn't a vector instruction then we can't set its swizzles to shift
				the data in the vector register.
			*/
			if ((g_psInstDesc[psDefInst->eOpcode].uFlags & DESC_FLAGS_VECTORDEST) == 0 || psDefInst->uDestCount != 1)
			{
				SetVectorUnnormalisable(psContext, psVectorData->psSet);
			}
			else
			{
				ASSERT(psVarDef->uLocation == 0);
				MergeSubset(psContext, psVectorData->psSet, psDefInst->auDestMask[0]);
			}
		}
	}
	else
	{
		SetVectorUnnormalisable(psContext, psVectorData->psSet);
	}

	for (psListEntry = psVector->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

		if (psUse == psVector->psDef)
		{
			continue;
		}

		/*
			Allow shifting results defined by instructions in the secondary program and used in the
			main program.
		*/
		if (psUse->eType == USE_TYPE_FIXEDREG && !psUse->u.psFixedReg->bPrimary)
		{
			continue;
		}

		switch (psUse->eType)
		{
			case USE_TYPE_SRC:
			{
				PINST	psUseInst;

				psUseInst = psUse->u.psInst;

				/*
					For an SSA choice function mark that all the source arguments and the
					destination must be shifted by the same amount.
				*/
				if (psUseInst->eOpcode == IDELTA)
				{
					IMG_UINT32	uSrc;

					for (uSrc = 0; uSrc < psUseInst->uArgumentCount; uSrc++)
					{
						if (uSrc != psUse->uLocation)
						{
							MergeVectors(psState, psContext, psVectorData, &psUseInst->asArg[uSrc]);
						}
					}

					ASSERT(psUseInst->uDestCount == 1);
					MergeVectors(psState, psContext, psVectorData, &psUseInst->asDest[0]);
				}
				/*
					Check that all uses of the vector register as a source are in instructions
					which support swizzles.
				*/
				else if ((g_psInstDesc[psUseInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) != 0)
				{
					IMG_UINT32	uLiveChansInArg;

					uLiveChansInArg = GetLiveChansInArg(psState, psUseInst, psUse->uLocation);
					MergeSubset(psContext, psVectorData->psSet, uLiveChansInArg);
				}
				else
				{
					SetVectorUnnormalisable(psContext, psVectorData->psSet);
				}
				break;
			}
			case USE_TYPE_OLDDEST:
			{
				PINST	psInst;
				PARG	psDest;

				/*
					If this intermediate register is used as the source for unwritten channels in
					an instruction destination then mark the destination and the current register
					must be shifted by the same amount.
				*/
				psInst = psUse->u.psInst;
				ASSERT(psUse->uLocation < psInst->uDestCount);
				psDest = &psInst->asDest[psUse->uLocation];
				MergeVectors(psState, psContext, psVectorData, psDest);
				break;
			}
			default:
			{
				SetVectorUnnormalisable(psContext, psVectorData->psSet);
				break;
			}
		}
	}
}

static
IMG_VOID CheckVectorLengthCB(IMG_PVOID pvIterData, IMG_PVOID pvData)
/*****************************************************************************
 FUNCTION	: CheckVectorLengthCB
    
 PURPOSE	: Check if a vector intermediate register has some unused components.

 PARAMETERS	: pvIterData	- Pass state.
			  pvData		- Intermediate register to check for.

 RETURNS	: Nothing.
*****************************************************************************/
{
	CheckVectorLength((PNORMALISE_VECTORS_CONTEXT)pvIterData, ((PNORMALISE_VECTOR_KEY)pvData)->psVector);
}

static
IMG_INT32 NormaliseVectorKeyCmp(IMG_PVOID pvKey1, IMG_PVOID pvKey2)
/*****************************************************************************
 FUNCTION	: NormaliseVectorKeyCmp
    
 PURPOSE	: Helper function to compare two elements in a map from temporary
			  register numbers to information about unused components in
			  a vector intermediate register.

 PARAMETERS	: pvKey1, pvKey2		- The elements to compare.

 RETURNS	: The comparison result.
*****************************************************************************/
{
	PNORMALISE_VECTOR_KEY	psKey1 = (PNORMALISE_VECTOR_KEY)pvKey1;
	PNORMALISE_VECTOR_KEY	psKey2 = (PNORMALISE_VECTOR_KEY)pvKey2;

	return psKey1->uTempNum - psKey2->uTempNum;
}

static
IMG_VOID NormaliseVectorsDelete(IMG_PVOID pvIterData, IMG_PVOID pvData)
/*****************************************************************************
 FUNCTION	: NormaliseVectorsDelete
    
 PURPOSE	: Helper function to delete an elements in a map from temporary
			  register numbers to information about unused components in
			  a vector intermediate register.

 PARAMETERS	: pvIterData		- Context data.
			  pvData			- Element to delete.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PNORMALISE_VECTOR_KEY	psKey = (PNORMALISE_VECTOR_KEY)pvData;
	PNORMALISE_VECTOR		psVector = psKey->psVector;
	PINTERMEDIATE_STATE		psState = (PINTERMEDIATE_STATE)pvIterData;

	RemoveFromList(&psVector->psSet->sSetList, &psVector->sSetListEntry);
	if (IsListEmpty(&psVector->psSet->sSetList))
	{
		UscFree(psState, psVector->psSet);
	}
	UscFree(psState, psVector);
}

static
IMG_VOID ReplaceVectorSubsetDefs(PINTERMEDIATE_STATE		psState,
								 PNORMALISE_VECTORS_CONTEXT	psContext,
								 PUSEDEF_CHAIN				psUseDefChain,
								 PNORMALISE_CHAN_STATUS		psSubsets,
								 PARG						asReplacementTemps)
/*****************************************************************************
 FUNCTION	: ReplaceVectorSubsetDefs
    
 PURPOSE	: Replace defines of different subsets of the channels in
			  a vector intermediate register by different intermediate
			  registers.

 PARAMETERS	: psState			- Compiler state.
			  psContext			- State for the current pass.
			  psUseDefChain		- Original intermediate register.
			  psSubsets			- List of subsets of the channels in the
								original vector intermediate register which
								are used independently.
			  asReplacementTemps
								- Details of the replacement intermediate
								registers for each subset.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF		psDef;
	PINST		psDefInst;
	IMG_UINT32	uSubsetIdx;

	psDef = psUseDefChain->psDef;
	ASSERT(psDef != NULL);
	ASSERT(psDef->eType == DEF_TYPE_INST);
	ASSERT(psDef->uLocation == 0);
	psDefInst = psDef->u.psInst;

	if (psDefInst->eOpcode == IDELTA)
	{
		ASSERT(psDefInst->uDestCount == 1);
		ASSERT(psDefInst->apsOldDest[0] == NULL);
		ASSERT(psDefInst->u.psDelta->bVector);

		/*
			If the intermediate register is defined by an SSA choice function then create new
			choice functions for each subset.
		*/
		for (uSubsetIdx = 0; uSubsetIdx < psSubsets->uSubsetCount; uSubsetIdx++)
		{
			IMG_UINT32	uSubsetUsedChans = psSubsets->auSubsets[uSubsetIdx];

			if ((psDefInst->auLiveChansInDest[0] & uSubsetUsedChans) != 0)
			{
				PINST		psNewDeltaInst;
				IMG_UINT32	uArg;

				psNewDeltaInst = AllocateInst(psState, psDefInst);
				SetOpcode(psState, psNewDeltaInst, IDELTA);
				psNewDeltaInst->u.psDelta->bVector = IMG_TRUE;

				psNewDeltaInst->auLiveChansInDest[0] = psDefInst->auLiveChansInDest[0] & uSubsetUsedChans;
				SetDestFromArg(psState, psNewDeltaInst, 0 /* uDestIdx */, &asReplacementTemps[uSubsetIdx]);

				SetArgumentCount(psState, psNewDeltaInst, psDefInst->uArgumentCount);
				for (uArg = 0; uArg < psDefInst->uArgumentCount; uArg++)
				{
					CopySrc(psState, psNewDeltaInst, uArg, psDefInst, uArg);
				}

				InsertInstBefore(psState, psDefInst->psBlock, psNewDeltaInst, psDefInst);
			}
		}

		/*
			Drop the original instruction.
		*/
		RemoveInst(psState, psDefInst->psBlock, psDefInst);
		FreeInst(psState, psDefInst);
	}
	else
	{
		IMG_UINT32	uOrigLiveChansInDest;
		ARG			sOrigPartialDest;
		IMG_BOOL	bPartialDestPresent;

		ASSERT(psDefInst->uDestCount == 1);

		/*
			For any other instruction replace the destination by the new intermediate register
			for the subset actually written and add new moves to copy preserved channels from
			the source for preserved channels to the replacement register for the subset.
		*/

		uOrigLiveChansInDest = psDefInst->auLiveChansInDest[0];
		if (psDefInst->apsOldDest[0] != NULL)
		{
			bPartialDestPresent = IMG_TRUE;
			sOrigPartialDest = *psDefInst->apsOldDest[0];
		}
		else
		{
			bPartialDestPresent = IMG_FALSE;
			InitInstArg(&sOrigPartialDest);
		}
		for (uSubsetIdx = 0; uSubsetIdx < psSubsets->uSubsetCount; uSubsetIdx++)
		{
			IMG_UINT32	uSubsetUsedChans = psSubsets->auSubsets[uSubsetIdx];

			if ((uOrigLiveChansInDest & uSubsetUsedChans) != 0)
			{
				if ((psDefInst->auDestMask[0] & uSubsetUsedChans) != 0)
				{
					psDefInst->auLiveChansInDest[0] &= uSubsetUsedChans;
					SetDestFromArg(psState, psDefInst, 0 /* uDestIdx */, &asReplacementTemps[uSubsetIdx]);

					if (bPartialDestPresent && GetPreservedChansInPartialDest(psState, psDefInst, 0 /* uDestIdx */) == 0)
					{
						SetPartiallyWrittenDest(psState, psDefInst, 0 /* uDestIdx */, NULL /* psPartialDest */);
					}
				}
				else
				{
					if (bPartialDestPresent)
					{
						PINST	psVMOVInst;

						psVMOVInst = AllocateInst(psState, psDefInst);

						SetOpcode(psState, psVMOVInst, IVMOV);
	
						SetDestFromArg(psState, psVMOVInst, 0 /* uDestIdx */, &asReplacementTemps[uSubsetIdx]);
						SetSrcFromArg(psState, psVMOVInst, 0 /* uSrcIdx */, &sOrigPartialDest);

						psVMOVInst->auLiveChansInDest[0] = uOrigLiveChansInDest & uSubsetUsedChans;
						psVMOVInst->auDestMask[0] = psVMOVInst->auLiveChansInDest[0];

						psVMOVInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
						psVMOVInst->u.psVec->aeSrcFmt[0] = sOrigPartialDest.eFmt;

						InsertInstBefore(psState, psDefInst->psBlock, psVMOVInst, psDefInst);

						AppendToNewMoveList(psContext, psVMOVInst);
					}
				}
			}
		}
	}
}

static
IMG_VOID ReplaceVectorSubsetUses(PINTERMEDIATE_STATE	psState,
								 PUSEDEF_CHAIN			psUseDefChain,
								 PNORMALISE_CHAN_STATUS	psSubsets,
								 PARG					asReplacementTemps)
/*****************************************************************************
 FUNCTION	: ReplaceVectorSubsetUses
    
 PURPOSE	: Replace uses of different subsets of the channels in
			  a vector intermediate register by different intermediate
			  registers.

 PARAMETERS	: psState			- Compiler state.
			  psUseDefChain		- Original intermediate register.
			  psSubsets			- List of subsets of the channels in the
								original vector intermediate register which
								are used independently.
			  asReplacementTemps
								- Details of the replacement intermediate
								registers for each subset.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY		psUseListEntry;
	PUSC_LIST_ENTRY		psNextListEntry;
	
	for (psUseListEntry = psUseDefChain->sList.psHead; psUseListEntry != NULL; psUseListEntry = psNextListEntry)
	{
		PUSEDEF	psUse;

		psNextListEntry = psUseListEntry->psNext;
		psUse = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);

		if (psUse->eType == USE_TYPE_SRC || psUse->eType == USE_TYPE_OLDDEST)
		{
			IMG_UINT32	uUsedChans;
			IMG_UINT32	uSubsetIdx;

			uUsedChans = GetUseChanMask(psState, psUse);
	
			for (uSubsetIdx = 0; uSubsetIdx < psSubsets->uSubsetCount; uSubsetIdx++)
			{
				IMG_UINT32	uSubset = psSubsets->auSubsets[uSubsetIdx];

				if ((uSubset & uUsedChans) != 0)
				{
					UseDefSubstUse(psState, psUseDefChain, psUse, &asReplacementTemps[uSubsetIdx]);
					break;
				}
			}
		}
		else if (psUse->eType == USE_TYPE_FIXEDREG)
		{
			IMG_UINT32	uChansLiveOut;
			IMG_UINT32	uSubsetIdx;
			PVREGISTER	psVRegister = GetVRegister(psState, USEASM_REGTYPE_TEMP, psUseDefChain->uNumber);

			ASSERT(!psUse->u.psFixedReg->bPrimary);

			uChansLiveOut = GetRegisterLiveMask(psState,
												&psState->psSecAttrProg->sCfg.psExit->sRegistersLiveOut,
												USEASM_REGTYPE_TEMP,
												psUseDefChain->uNumber,
												0 /* uArrayOffset */);

			ASSERT(psVRegister->psSecAttr != NULL);
			DropSAProgResult(psState, psVRegister->psSecAttr);

			for (uSubsetIdx = 0; uSubsetIdx < psSubsets->uSubsetCount; uSubsetIdx++)
			{
				IMG_UINT32	uSubset = psSubsets->auSubsets[uSubsetIdx];
				IMG_UINT32	uChansLiveInSubset = uSubset & uChansLiveOut;

				if (uChansLiveInSubset != 0)
				{
					SetRegisterLiveMask(psState,
										&psState->psSecAttrProg->sCfg.psExit->sRegistersLiveOut,
										USEASM_REGTYPE_TEMP,
										asReplacementTemps[uSubsetIdx].uNumber,
										0 /* uArrayOffset */,
										uChansLiveInSubset);

					BaseAddNewSAProgResult(psState,
										   asReplacementTemps[uSubsetIdx].eFmt,
										   IMG_TRUE /* bVector */,
										   0 /* uHwRegAlignment */,
										   g_auSetBitCount[uChansLiveInSubset] /* uNumHwRegisters */,
										   asReplacementTemps[uSubsetIdx].uNumber,
										   uChansLiveInSubset,
										   SAPROG_RESULT_TYPE_CALC,
										   IMG_FALSE /* bPartOfRange */);

				}
			}
		}
	}
}

static
IMG_VOID SeperateVectorSubsets(PINTERMEDIATE_STATE			psState,
							   PNORMALISE_VECTORS_CONTEXT	psContext,
							   PNORMALISE_VECTOR_SET		psSet)
/*****************************************************************************
 FUNCTION	: ReplaceVectorSubsetUses
    
 PURPOSE	: For a set of vector intermediate registers: replace uses and define of 
			  different subsets of the channels in each register by different intermediate
			  registers.

 PARAMETERS	: psState			- Compiler state.
			  psContext			- Pass state.
			  psSet				- Set of vector intermediate registers.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG*			apsNewArgs;
	PUSC_LIST_ENTRY	psSetListEntry;
	IMG_UINT32		uSetIdx;

	/*
		Allocate space for the replacement intermediate registers for each
		member of the set.
	*/
	apsNewArgs = UscAlloc(psState, sizeof(apsNewArgs[0]) * psSet->uSetCount);

	for (psSetListEntry = psSet->sSetList.psHead, uSetIdx = 0; 
		 psSetListEntry != NULL; 
		 psSetListEntry = psSetListEntry->psNext, uSetIdx++)
	{
		PNORMALISE_VECTOR	psVector;
		IMG_UINT32			uSubsetIdx;

		psVector = IMG_CONTAINING_RECORD(psSetListEntry, PNORMALISE_VECTOR, sSetListEntry);

		apsNewArgs[uSetIdx] = UscAlloc(psState, sizeof(*apsNewArgs[0]) * psSet->sChans.uSubsetCount);

		/*
			Allocate new intermediate registers.
		*/
		for (uSubsetIdx = 0; uSubsetIdx < psSet->sChans.uSubsetCount; uSubsetIdx++)
		{
			MakeNewTempArg(psState, psVector->psUseDefChain->eFmt, &apsNewArgs[uSetIdx][uSubsetIdx]);
		}

		/*
			Replace the define of this member of the set by one of the intermediate registers.
		*/
		ReplaceVectorSubsetDefs(psState, psContext, psVector->psUseDefChain, &psSet->sChans, apsNewArgs[uSetIdx]);
	}

	/*
		Replace the uses of each subset of channels by different intermediate registers.
	*/
	for (psSetListEntry = psSet->sSetList.psHead, uSetIdx = 0; 
		 psSetListEntry != NULL; 
		 psSetListEntry = psSetListEntry->psNext, uSetIdx++)
	{
		PNORMALISE_VECTOR	psVector;

		psVector = IMG_CONTAINING_RECORD(psSetListEntry, PNORMALISE_VECTOR, sSetListEntry);
		ReplaceVectorSubsetUses(psState, psVector->psUseDefChain, &psSet->sChans, apsNewArgs[uSetIdx]);
	}

	/*
		Pack the new intermediate registers so the used channels are consecutive and start from X.
	*/
	for (psSetListEntry = psSet->sSetList.psHead, uSetIdx = 0; 
		 psSetListEntry != NULL; 
		 psSetListEntry = psSetListEntry->psNext, uSetIdx++)
	{
		IMG_UINT32	uSubset;

		for (uSubset = 0; uSubset < psSet->sChans.uSubsetCount; uSubset++)
		{
			PUSEDEF_CHAIN	psUseDefChain;

			psUseDefChain = apsNewArgs[uSetIdx][uSubset].psRegister->psUseDefChain;
			if (psUseDefChain != NULL)
			{
				PackVector(psState, psContext, psUseDefChain, psSet->sChans.auSubsets[uSubset]);
			}
		}
	}

	/*
		Free used memory.
	*/
	for (psSetListEntry = psSet->sSetList.psHead, uSetIdx = 0; 
		 psSetListEntry != NULL; 
		 psSetListEntry = psSetListEntry->psNext, uSetIdx++)
	{
		UscFree(psState, apsNewArgs[uSetIdx]);
	}
	UscFree(psState, apsNewArgs);
}

static
IMG_VOID SubstMovesInList(PINTERMEDIATE_STATE psState, PUSC_LIST psList)
/*****************************************************************************
 FUNCTION	: SubstMovesInList
    
 PURPOSE	: For all moves in a list replace the destination of the move by
			  the source.

 PARAMETERS	: psState			- Compiler state.
			  psList			- List of moves.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;

	while ((psListEntry = RemoveListHead(psList)) != NULL)
	{
		PINST		psMovInst;
		PARG		psDest;
		PARG		psSrc;

		psMovInst = IMG_CONTAINING_RECORD(psListEntry, PINST, sTempListEntry);

		psDest = &psMovInst->asDest[0];
		psSrc = &psMovInst->asArg[0];

		SetBit(psMovInst->auFlag, INST_INNEWMOVELIST, 0);

		ASSERT(psMovInst->eOpcode == IVMOV);
		EliminateVMOV(psState, psMovInst, IMG_FALSE);
	}
}

typedef struct _TOPSORT_ELEM
{
	USC_LIST_ENTRY	sListEntry;
	PUSEDEF_CHAIN	psVector;
} TOPSORT_ELEM, *PTOPSORT_ELEM;

static
IMG_VOID AppendToTopSortList(PINTERMEDIATE_STATE psState, PUSC_LIST psTopSortList, PUSEDEF_CHAIN psVectorToAppend)
/*****************************************************************************
 FUNCTION	: AppendToTopSortList
    
 PURPOSE	: Append an entry to a list of intermediate registers.

 PARAMETERS	: psState			- Compiler state.
			  psTopSortList		- List to append to.
			  psVectorToAppend	- Intermediate register to append.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PTOPSORT_ELEM	psElem;

	psElem = UscAlloc(psState, sizeof(*psElem));
	psElem->psVector = psVectorToAppend;
	AppendToList(psTopSortList, &psElem->sListEntry);
}

static
IMG_VOID ReplacePartialDests(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: ReplacePartialDests
    
 PURPOSE	: Check for cases where an intermediate register is defined with
			  some channels copied from another intermediate register but the
			  copied channels aren't used together with the other channels in
			  the vector.

 PARAMETERS	: psState			- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;
	USC_LIST		sTopSortList;

	/*
		Create a list of vector intermediate registers sorted so that for any two registers if the first register 
		is the source for unwritten channels in the definition of the second then it is earlier in the list.
	*/

	/*
		Add all the intermediate registers which are completely defined to the list.
	*/
	InitializeList(&sTopSortList);
	for (psListEntry = psState->sVectorTempList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF_CHAIN	psVector;
		PUSEDEF			psVectorDef;

		psVector = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF_CHAIN, sVectorTempListEntry);
		psVectorDef = psVector->psDef;

		if (psVectorDef != NULL && psVectorDef->eType == DEF_TYPE_INST)
		{
			PINST	psDefInst;

			psDefInst = psVectorDef->u.psInst;
			ASSERT(psVectorDef->uLocation < psDefInst->uDestCount);
			if (psDefInst->apsOldDest[psVectorDef->uLocation] != NULL)
			{
				continue;
			}
		}
		AppendToTopSortList(psState, &sTopSortList, psVector);
	}

	/*
		For each register in the list: append all the registers which are defined using it as
		a source for unwritten channels.
	*/
	for (psListEntry = sTopSortList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF_CHAIN	psVector;
		PTOPSORT_ELEM	psElem;
		PUSC_LIST_ENTRY	psUseListEntry;

		psElem = IMG_CONTAINING_RECORD(psListEntry, PTOPSORT_ELEM, sListEntry);
		psVector = psElem->psVector;
		
		for (psUseListEntry = psVector->sList.psHead; psUseListEntry != NULL; psUseListEntry = psUseListEntry->psNext)
		{
			PUSEDEF	psUse;

			psUse = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);
			if (psUse->eType == USE_TYPE_OLDDEST)
			{
				PINST	psDefInst;
				PARG	psDest = &psUse->u.psInst->asDest[psUse->uLocation];

				psDefInst = psUse->u.psInst;
				ASSERT(psUse->uLocation < psDefInst->uDestCount);
				psDest = &psDefInst->asDest[psUse->uLocation];

				AppendToTopSortList(psState, &sTopSortList, psDest->psRegister->psUseDefChain);
			}
		}
	}

	while ((psListEntry = RemoveListTail(&sTopSortList)) != NULL)
	{
		NORMALISE_CHAN_STATUS	sChanStatus;
		PUSEDEF_CHAIN			psVector;
		PUSEDEF					psVectorDef;
		PINST					psDefInst;
		IMG_UINT32				uDestMask;
		IMG_UINT32				uPreservedMask;
		IMG_UINT32				uReplaceMask;
		IMG_UINT32				uSubsetIdx;
		PTOPSORT_ELEM			psElem;
		PUSC_LIST_ENTRY			psUseListEntry;

		psElem = IMG_CONTAINING_RECORD(psListEntry, PTOPSORT_ELEM, sListEntry);
		psVector = psElem->psVector;
		psVectorDef = psVector->psDef;

		UscFree(psState, psElem);
		psElem = NULL;

		if (psVectorDef == NULL || psVectorDef->eType != DEF_TYPE_INST)
		{
			continue;
		}
		psDefInst = psVectorDef->u.psInst;
		ASSERT(psVectorDef->uLocation < psDefInst->uDestCount);
		if (psDefInst->apsOldDest[psVectorDef->uLocation] == NULL)
		{
			continue;
		}
		if (!NoPredicate(psState, psDefInst))
		{
			continue;
		}

		/*
			Check which subsets of channels in the intermediate registers are used together.
		*/
		InitializeChanStatus(&sChanStatus);
		for (psUseListEntry = psVector->sList.psHead; psUseListEntry != NULL; psUseListEntry = psUseListEntry->psNext)
		{
			PUSEDEF	psUse;

			psUse = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);
			if (psUse != psVector->psDef)
			{
				IMG_UINT32	uUseChanMask;

				uUseChanMask = GetUseChanMask(psState, psUse);
				AddToChanStatus(&sChanStatus, uUseChanMask);
			}
		}

		/*
			Get the mask of channels written by the defining instruction and the mask copied from
			another intermediate register.
		*/
		ASSERT(psVectorDef->uLocation < psDefInst->uDestCount);
		uDestMask = psDefInst->auDestMask[psVectorDef->uLocation];
		uPreservedMask = (~uDestMask) & psDefInst->auLiveChansInDest[psVectorDef->uLocation];

		/*	
			Check if the any subset of the channels copied from another intermediate registers are never
			used together with the written channels.
		*/
		uReplaceMask = 0;
		for (uSubsetIdx = 0; uSubsetIdx < sChanStatus.uSubsetCount; uSubsetIdx++)
		{
			IMG_UINT32	uSubset = sChanStatus.auSubsets[uSubsetIdx];

			if ((uSubset & uDestMask) == 0)
			{
				ASSERT((uSubset & ~uPreservedMask) == 0);
				ASSERT((uSubset & uPreservedMask) != 0);
				uReplaceMask |= uSubset; 
			}
		}

		/*
			Replace all uses of that subset of channels by the source for the copy.
		*/
		if (uReplaceMask == uPreservedMask)
		{
			ARG				sOldDest;
			PUSC_LIST_ENTRY	psNextListEntry;

			sOldDest = *psDefInst->apsOldDest[psVectorDef->uLocation];

			/*
				Remove the substituted channels from the mask of channels used from the intermediate register
			*/
			psDefInst->auLiveChansInDest[psVectorDef->uLocation] &= ~uReplaceMask;
			if (GetPreservedChansInPartialDest(psState, psDefInst, psVectorDef->uLocation) == 0)
			{
				SetPartiallyWrittenDest(psState, psDefInst, psVectorDef->uLocation, NULL /* psPartialDest */);
				psDefInst->psBlock->uFlags |= USC_CODEBLOCK_NEED_DEP_RECALC;
			}

			for (psUseListEntry = psVector->sList.psHead; psUseListEntry != NULL; psUseListEntry = psNextListEntry)
			{
				PUSEDEF	psUse;

				psNextListEntry = psUseListEntry->psNext;
				psUse = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);
				if (psUse != psVector->psDef)
				{		
					IMG_UINT32	uUseChanMask;

					uUseChanMask = GetUseChanMask(psState, psUse);
					if ((uUseChanMask & uReplaceMask) != 0)
					{
						ASSERT((uUseChanMask & uDestMask) == 0);
						UseDefSubstUse(psState, psVector, psUse, &sOldDest);
					}
				}
			}
		}
	}
}

IMG_INTERNAL
IMG_VOID NormaliseVectorLengths(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: NormaliseVectorLengths
    
 PURPOSE	: Try to normalise the channels used in vector registers so they
			  start from X.

 PARAMETERS	: psState			- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY				psListEntry;
	NORMALISE_VECTORS_CONTEXT	sContext;
	IMG_BOOL					bChanges;

	ReplacePartialDests(psState);

	sContext.psVarMap = UscTreeMake(psState, sizeof(NORMALISE_VECTOR_KEY), NormaliseVectorKeyCmp);
	InitializeList(&sContext.sShiftableVectorsList);
	sContext.psState = psState;

	/*
		Initialize information to store for each vector intermediate register the number of unused
		components.
	*/
	for (psListEntry = psState->sVectorTempList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF_CHAIN			psVector;
		PNORMALISE_VECTOR		psVectorData;
		PNORMALISE_VECTOR_SET	psVectorSetData;
		NORMALISE_VECTOR_KEY	sKey;

		psVector = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF_CHAIN, sVectorTempListEntry);
		if (psVector->uType == USC_REGTYPE_REGARRAY)
		{
			continue;
		}

		psVectorSetData = UscAlloc(psState, sizeof(*psVectorSetData));
		InitializeChanStatus(&psVectorSetData->sChans);
		InitializeList(&psVectorSetData->sSetList);

		psVectorData = UscAlloc(psState, sizeof(*psVector));
		psVectorData->psSet = psVectorSetData;
		psVectorData->psUseDefChain = psVector;

		AppendToList(&psVectorSetData->sSetList, &psVectorData->sSetListEntry);
		psVectorSetData->uSetCount = 1;

		sKey.uTempNum = psVector->uNumber;
		sKey.psVector = psVectorData;
		UscTreeAdd(psState, sContext.psVarMap, &sKey); 
	}

	/*
		Process each vector register to find unused components and also any linked registers which
		need to use the same shift.
	*/
	UscTreeIterPreOrder(psState, sContext.psVarMap, CheckVectorLengthCB, &sContext);

	/*
		If there were any registers with unused leading components then adjust swizzles/masks on the
		using/defining instructions to drop the unused components.
	*/
	bChanges = IMG_FALSE;
	InitializeList(&sContext.sNewMoveList);
	while ((psListEntry = RemoveListHead(&sContext.sShiftableVectorsList)) != NULL)
	{
		PNORMALISE_VECTOR_SET	psSet;
		PUSC_LIST_ENTRY			psSetListEntry;

		psSet = IMG_CONTAINING_RECORD(psListEntry, PNORMALISE_VECTOR_SET, sShiftableVectorsListEntry);
		ASSERT(psSet->sChans.bNormalisable);
		ASSERT(!psSet->sChans.bInvalid);

		if (psSet->sChans.uSubsetCount > 1)
		{
			SeperateVectorSubsets(psState, &sContext, psSet);
		}
		else
		{
			ASSERT(psSet->sChans.uSubsetCount == 1);
			for (psSetListEntry = psSet->sSetList.psHead; psSetListEntry != NULL; psSetListEntry = psSetListEntry->psNext)
			{
				PNORMALISE_VECTOR	psVector;

				psVector = IMG_CONTAINING_RECORD(psSetListEntry, PNORMALISE_VECTOR, sSetListEntry);
				PackVector(psState, &sContext, psVector->psUseDefChain, psSet->sChans.auSubsets[0]);
			}
		}

		bChanges = IMG_TRUE;
	}

	/*
		Where we inserted moves because one subset was copied into a partial defined
		destination when another subset was written: now replace the move destination
		by the source.
	*/
	SubstMovesInList(psState, &sContext.sNewMoveList);

	/*
		Free information about unused components of vector registers.
	*/
	UscTreeDeleteAll(psState, sContext.psVarMap, NormaliseVectorsDelete, psState);

	/*
		Try to modify references to vectors of scalar registers from vector instructions so
		the length of the vector is minimized.
	*/
	DoOnAllBasicBlocks(psState, ANY_ORDER, NormaliseNonVectorRegistersBP, IMG_FALSE /* CALLs */, (IMG_PVOID)&bChanges);

	/*	
		Show the updated intermediate code.
	*/
	if (bChanges)
	{
		TESTONLY_PRINT_INTERMEDIATE("Normalised vectors", "vecnorm", IMG_FALSE);
	}
}


IMG_INTERNAL
IMG_BOOL ConvertUnifiedStoreVMADToGPIVMAD(PINTERMEDIATE_STATE	psState,
										  PINST					psInst,
										  IMG_BOOL				bSwapSrc01)
/*****************************************************************************
 FUNCTION	: ConvertUnifiedStoreVMADToGPIVMAD
    
 PURPOSE	: Try to convert a VMAD instruction to the GPI-oriented variant.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to convert.
			  bSwapSrc01		- If TRUE then also swap source 0 and source 1
								to the instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uSrcSlotIdx;
	IMG_UINT32	auMatchedSwizzle[VMAD_SOURCE_SLOT_COUNT];

	/*
		Check for each source swizzle that there is an equivalent swizzle supported by VMAD4.
	*/
	for (uSrcSlotIdx = 0; uSrcSlotIdx < VMAD_SOURCE_SLOT_COUNT; uSrcSlotIdx++)
	{
		IMG_UINT32	uChansUsed;
		IMG_UINT32	uSrcSlotIdxInNewInst;

		if (bSwapSrc01 && (uSrcSlotIdx == 0 || uSrcSlotIdx == 1))
		{
			uSrcSlotIdxInNewInst = 1 - uSrcSlotIdx;
		}
		else
		{
			uSrcSlotIdxInNewInst = uSrcSlotIdx;
		}

		GetLiveChansInSourceSlot(psState, psInst, uSrcSlotIdx, &uChansUsed, NULL /* puChansUsedPostSwizzle */);
		if (!IsSwizzleSupported(psState,
								psInst,
								IVMAD4,
								uSrcSlotIdx,
								psInst->u.psVec->auSwizzle[uSrcSlotIdxInNewInst],
								uChansUsed,
								&auMatchedSwizzle[uSrcSlotIdx]))
		{
			return IMG_FALSE;
		}
	}

	/*
		Switch from VMAD to VMAD4.
	*/
	ModifyOpcode(psState, psInst, IVMAD4);

	/*
		Swap the information about source 0 and source 1.
	*/
	if (bSwapSrc01)
	{
		SwapVectorSources(psState, psInst, 0 /* uArg1Idx */, psInst, 1 /* uArg2Idx */);
	}

	/*
		Update the swizzles with equivalents supported by VMAD4.
	*/
	for (uSrcSlotIdx = 0; uSrcSlotIdx < VMAD_SOURCE_SLOT_COUNT; uSrcSlotIdx++)
	{
		psInst->u.psVec->auSwizzle[uSrcSlotIdx] = auMatchedSwizzle[uSrcSlotIdx];
	}

	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL InstUsesF16ToF32AutomaticConversions(PINTERMEDIATE_STATE	psState, 
											  PINST					psInst, 
											  IMG_PBOOL				pbDestIsF16, 
											  IMG_PUINT32			puF16SrcMask)
/*****************************************************************************
 FUNCTION	: InstUsesF16ToF32AutomaticConversions
    
 PURPOSE	: Check for an instruction where unified store sources and destinations will
			  be interpreted as F16.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  pbDestIsF16		- If non-NULL returns TRUE if a destination has an automatic
								conversion from F16 to F32.
			  puF16SrcMask		- If non-NULL returns a mask of F16 sources which have
								automatic conversion from F16 to F32.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_BOOL	bF16Arguments, bF32Arguments;
	IMG_UINT32	uSlotIdx;
	IMG_UINT32	uDestIdx;
	IMG_BOOL	bDestIsF16;
	IMG_UINT32	uF16SrcMask;

	/*
		Set default return values.
	*/
	if (pbDestIsF16 != NULL)
	{
		*pbDestIsF16 = IMG_FALSE;
	}
	if (puF16SrcMask != NULL)
	{
		*puF16SrcMask = 0;
	}

	if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) == 0)
	{
		return IMG_FALSE;
	}

	/*
		Complex-ops and packs have separate controls for the formats of the source and destination.
	*/
	if (
			(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORCOMPLEXOP) != 0 || 
			(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORPACK) != 0 || 
			psInst->eOpcode == IVMOV ||
			psInst->eOpcode == IVTESTBYTEMASK
	   )
	{
		return IMG_FALSE;
	}

	bF16Arguments = bF32Arguments = IMG_FALSE;
	bDestIsF16 = IMG_FALSE;
	uF16SrcMask = 0;

	/*
		Check the instruction uses a mix of F16 and F32 arguments.
	*/
	for (uSlotIdx = 0; uSlotIdx < GetSwizzleSlotCount(psState, psInst); uSlotIdx++)
	{
		UF_REGFORMAT	eSlotFormat = psInst->u.psVec->aeSrcFmt[uSlotIdx];

		if (psInst->eOpcode == IVMOVCU8_FLT && uSlotIdx == 0)
		{
			continue;
		}

		switch (eSlotFormat)
		{
			case UF_REGFORMAT_F16: 
			{
				bF16Arguments = IMG_TRUE;
				uF16SrcMask |= (1 << uSlotIdx);
				break;
			}
			case UF_REGFORMAT_F32: 
			{
				bF32Arguments = IMG_TRUE; 
				break;
			}
			default: break;
		}
	}
	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		switch (psInst->asDest[uDestIdx].eFmt)
		{
			case UF_REGFORMAT_F16:
			{
				bF16Arguments = IMG_TRUE; 
				bDestIsF16 = IMG_TRUE;
				break;
			}
			case UF_REGFORMAT_F32: 
			{
				bF32Arguments = IMG_TRUE; 
				break;
			}
			default: break;
		}
	}

	if (bF16Arguments && bF32Arguments)
	{
		if (pbDestIsF16 != NULL)
		{
			*pbDestIsF16 = bDestIsF16;
		}
		if (puF16SrcMask != NULL)
		{
			*puF16SrcMask = uF16SrcMask;
		}
		return IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
}

static
IMG_BOOL IsNegatedCopyAvailable(PINTERMEDIATE_STATE	psState,
								PREGISTER_MAP		psNegatedCopyMap,
								PARG				psPredSrc)
/*****************************************************************************
 FUNCTION	: IsNegatedCopyAvailable
    
 PURPOSE	: Check if an instruction source predicate can be replaced by a new predicate
			  which is the negation of the original value.

 PARAMETERS	: psState			- Compiler state.
			  psNegatedCopyMap	- Maps each intermediate predicate to a predicate
								containing its negation.
			  psPredSrc			- Original source predicate.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PARG		psCopy;

	ASSERT(psPredSrc->uType == USEASM_REGTYPE_PREDICATE);

	psCopy = IntermediateRegisterMapGet(psNegatedCopyMap, psPredSrc->uNumber);

	if (psCopy != NULL)
	{
		return IMG_TRUE;
	}
	else
	{
		PINST		psDefInst;
		IMG_UINT32	uDefDestIdx;

		psDefInst = UseDefGetDefInstFromChain(psPredSrc->psRegister->psUseDefChain, &uDefDestIdx);
		ASSERT(psDefInst != NULL);

		if ((psDefInst->eOpcode == IVTEST || psDefInst->eOpcode == ITESTPRED) && NoPredicate(psState, psDefInst))
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static
IMG_VOID MakeNegatedCopy(PINTERMEDIATE_STATE	psState,
						 PREGISTER_MAP			psNegatedCopyMap,
						 PINST					psInst,
						 IMG_UINT32				uPred)
/*****************************************************************************
 FUNCTION	: MakeNegatedCopy
    
 PURPOSE	: Replaces an instruction source predicate by a new predicate
			  which is the negation of the original value.

 PARAMETERS	: psState			- Compiler state.
			  psNegatedCopyMap	- Map each intermediate predicate to a predicate
								containing its negation.
			  psInst			- Original source predicate.
			  uPred

 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG	psPredSrc = psInst->apsPredSrc[uPred];
	PARG	psCopy;

	ASSERT(psPredSrc->uType == USEASM_REGTYPE_PREDICATE);

	/*
		Check if a negated copy of this predicate has already been created.
	*/
	psCopy = IntermediateRegisterMapGet(psNegatedCopyMap, psPredSrc->uNumber);

	if (psCopy == NULL)
	{
		PINST		psDefInst;
		IMG_UINT32	uDefDestIdx;
		PINST		psDefCopyInst;
		IMG_UINT32	uDest;

		/*
			Get the instruction which defines this predicate.
		*/
		psDefInst = UseDefGetDefInstFromChain(psPredSrc->psRegister->psUseDefChain, &uDefDestIdx);
		ASSERT(psDefInst != NULL);

		ASSERT(NoPredicate(psState, psDefInst));

		/*
			Create another copy of the defining instruction.
		*/
		psDefCopyInst = CopyInst(psState, psDefInst);
		InsertInstAfter(psState, psDefInst->psBlock, psDefCopyInst, psDefInst);

		/*
			If the defining instruction also writes a temporary register then drop
			this from the copy.
		*/
		if (psDefCopyInst->eOpcode == IVTEST)
		{
			if (psDefCopyInst->uDestCount == VTEST_MAXIMUM_DEST_COUNT)
			{
				SetDestCount(psState, psDefCopyInst, VTEST_PREDICATE_ONLY_DEST_COUNT);
			}
			else
			{
				ASSERT(psDefCopyInst->uDestCount == VTEST_PREDICATE_ONLY_DEST_COUNT);
			}
		}
		else
		{
			ASSERT(psDefCopyInst->eOpcode == ITESTPRED);
						
			if (psDefCopyInst->uDestCount == TEST_MAXIMUM_DEST_COUNT)
			{
				SetDestCount(psState, psDefCopyInst, TEST_PREDICATE_ONLY_DEST_COUNT);
			}
			else
			{
				ASSERT(psDefCopyInst->uDestCount == TEST_PREDICATE_ONLY_DEST_COUNT);
			}
		}

		/*
			Allocate a new set of destination for the copied instruction.
		*/
		for (uDest = 0; uDest < psDefCopyInst->uDestCount; uDest++)
		{
			IMG_UINT32	uNewPredRegNum;
			PARG		psOldDest = &psDefInst->asDest[uDest];
			PARG		psNewDest = &psDefCopyInst->asDest[uDest];

			uNewPredRegNum = GetNextPredicateRegister(psState);
			SetDest(psState, psDefCopyInst, uDest, USEASM_REGTYPE_PREDICATE, uNewPredRegNum, UF_REGFORMAT_F32);

			/*
				Record that the new predicate contains the negation of the
				destination to the original instruction.
			*/
			IntermediateRegisterMapSet(psState, 
									   psNegatedCopyMap, 
									   psOldDest->uNumber, 
									   psNewDest);

			if (uDefDestIdx == uDest)
			{
				psCopy = psNewDest;
			}
		}

		/*
			Make the test applied by the copied instruction the opposite of the test applied by
			the original instruction.
		*/
		LogicalNegateInstTest(psState, psDefCopyInst);
	}

	/*
		Replace the predicate source by a new predicate contains the negation of the original source.
	*/
	ASSERT(psCopy->uType == USEASM_REGTYPE_PREDICATE);
	SetPredicateAtIndex(psState, psInst, psCopy->uNumber, IMG_FALSE /* bNegate */, uPred);
}

static
IMG_VOID FixNegatedPerChannelPredicatesBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNegatedCopyMap)
/*****************************************************************************
 FUNCTION	: FixNegatedPerChannelPredicatesBP
    
 PURPOSE	: Fix instructions using per-channel, negated predicates in a basic
			  block.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to modify instructions within.
			  pvNegatedCopyMap	- 

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST			psInst;
	PREGISTER_MAP	psNegatedCopyMap = (PREGISTER_MAP)pvNegatedCopyMap;

	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		IMG_UINT32	uPred;
		IMG_BOOL	bAllNegateAvailable;

		/*
			Check for instructions using a negated, per-channel predicate..
		*/
		if (NoPredicate(psState, psInst))
		{
			continue;
		}
		if (!GetBit(psInst->auFlag, INST_PRED_PERCHAN))
		{
			continue;
		}
		if (!GetBit(psInst->auFlag, INST_PRED_NEG))
		{
			continue;
		}

		ASSERT(psInst->uPredCount == VECTOR_LENGTH);
		ASSERT((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORDEST) != 0);

		/*
			Check if for all the source predicates we can make new predicates containing
			the negation of the original source.
		*/
		bAllNegateAvailable = IMG_TRUE;
		for (uPred = 0; uPred < psInst->uPredCount; uPred++)
		{
			if (IsPredicateSourceLive(psState, psInst, uPred))
			{
				PARG		psPredSrc = psInst->apsPredSrc[uPred];
		
				if (!IsNegatedCopyAvailable(psState, psNegatedCopyMap, psPredSrc))
				{
					bAllNegateAvailable = IMG_FALSE;
					break;
				}
			}
		}

		if (bAllNegateAvailable)
		{
			/*
				Replace all the predicate sources by new predicates containing
				the negation of the original value.
			*/
			for (uPred = 0; uPred < psInst->uPredCount; uPred++)
			{
				if (IsPredicateSourceLive(psState, psInst, uPred))
				{
					MakeNegatedCopy(psState, psNegatedCopyMap, psInst, uPred);
				}
			}

			/*
				The negation modifier is now applied when generating the
				source predicates.
			*/
			SetBit(psInst->auFlag, INST_PRED_NEG, 0);
		}
		else
		{
			/*
				Convert

					(!pX)	ALU		A[=B], C, D
				->
							ALU		T, C, D
					(pX)	MOV		A[=T], B
			*/
			MakePartialDestCopy(psState, psInst, 0 /* uCopyFromDestIdx */, IMG_FALSE /* bBefore */, NULL /* psNewDest */);
			ClearPredicates(psState, psInst);
		}
	}
}

IMG_INTERNAL
IMG_VOID FixNegatedPerChannelPredicates(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: FixNegatedPerChannelPredicates
    
 PURPOSE	: Fix instructions using per-channel, negated predicates.

 PARAMETERS	: psState			- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PREGISTER_MAP	psNegatedCopyMap;

	psNegatedCopyMap = IntermediateRegisterMapCreate(psState);

	DoOnAllBasicBlocks(psState, 
					   ANY_ORDER, 
					   FixNegatedPerChannelPredicatesBP, 
					   IMG_FALSE /* bHandlesCalls */, 
					   psNegatedCopyMap /* pvUserData */);

	IntermediateRegisterMapDestroy(psState, psNegatedCopyMap);

	TESTONLY_PRINT_INTERMEDIATE("Fix negated per-chan preds", "neg_per_chan_preds", IMG_FALSE);
}

static
IMG_VOID ReducePredicateValuesBitWidthInst(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: ReducePredicateValuesBitWidthInst

 PURPOSE    : 

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction to modify.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psDest;
	PUSC_LIST_ENTRY	psListEntry;
	PUSC_LIST_ENTRY	psNextListEntry;
	ARG				sReducedTemp;
	ARG				sDummyTemp;

	ASSERT(psInst->eOpcode == IVTESTMASK);
	ASSERT(psInst->uDestCount == 1);

	if (psInst->u.psVec->sTest.eMaskType != USEASM_TEST_MASK_NUM)
	{
		return;
	}
	if (psInst->apsOldDest[0] != NULL)
	{
		/*
			Don't modify instructions where some of the channels are copied from another register.
			VTESTBYTEMASK doesn't support a destination write mask.
		*/
		return;
	}

	/*
		Check that all uses of the destinations are in instructions which could be modified
		to use a mask of bytes instead.
	*/
	psDest = psInst->asDest[0].psRegister->psUseDefChain;
	for (psListEntry = psDest->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUse;
		PINST	psUseInst;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		if (psUse == psDest->psDef)
		{
			continue;
		}
		if (psUse->eType != USE_TYPE_SRC)
		{
			return;
		}
		psUseInst = psUse->u.psInst;
		if (psUseInst->eOpcode != IVMOVC)
		{
			return;
		}
		if (psUse->uLocation != 0)
		{
			return;
		}
		if (!CompareSwizzles(psUseInst->u.psVec->auSwizzle[0], USEASM_SWIZZLE(X, Y, Z, W), psUseInst->auDestMask[0]))
		{
			return;
		}
	}

	/*
		Convert VTESTMASK -> VTESTBYTEMASK
	*/
	ModifyOpcode(psState, psInst, IVTESTBYTEMASK);
	psInst->u.psVec->sTest.eMaskType = USEASM_TEST_MASK_BYTE;

	/*
		Allocate a new temporary for the mask of bytes.
	*/
	MakeNewTempArg(psState, UF_REGFORMAT_F32, &sReducedTemp);

	/*
		Set a dummy (never used) temporary as the second destination.
	*/
	SetDestCount(psState, psInst, 2 /* uNewDestCount */);
	MakeNewTempArg(psState, UF_REGFORMAT_F32, &sDummyTemp);
	SetDestFromArg(psState, psInst, 1 /* uDestIdx */, &sDummyTemp);
	psInst->auLiveChansInDest[1] = 0;
	psInst->auDestMask[1] = USC_ALL_CHAN_MASK;

	/*
		Replace all uses of the old destination.
	*/
	for (psListEntry = psDest->sList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PUSEDEF	psUse;

		psNextListEntry = psListEntry->psNext;
		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

		if (psUse == psDest->psDef)
		{
			SetDestFromArg(psState, psInst, 0 /* uDestIdx */, &sReducedTemp);
			psInst->auDestMask[0] = USC_ALL_CHAN_MASK;
		}
		else
		{
			PINST		psUseInst;
			IMG_UINT32	uChan;

			psUseInst = psUse->u.psInst;
			ASSERT(psUseInst->eOpcode == IVMOVC);
			ASSERT(psUse->uLocation == 0);

			ModifyOpcode(psState, psUseInst, IVMOVCU8_FLT);

			psUseInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
			psUseInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F32;

			SetSrcUnused(psState, psUseInst, 0 /* uSrcIdx */);
			SetSrcFromArg(psState, psUseInst, 1 /* uSrcIdx */, &sReducedTemp);
			for (uChan = 1; uChan < VECTOR_LENGTH; uChan++)
			{
				SetSrcUnused(psState, psUseInst, 1 + uChan);
			}
		}
	}
}

IMG_INTERNAL
IMG_VOID ReducePredicateValuesBitWidth(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: ReducePredicateValuesBitWidth

 PURPOSE    : 

 PARAMETERS	: psState	- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ForAllInstructionsOfType(psState, IVTESTMASK, ReducePredicateValuesBitWidthInst);

	TESTONLY_PRINT_INTERMEDIATE("Reduce Predicate Values Bitwidth", "red_pred", IMG_FALSE);
}

#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */ 

/* EOF */
