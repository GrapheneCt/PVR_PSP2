/******************************************************************************
 * Name         : usedisasm.c
 * Title        : Pixel Shader Compiler
 * Author       : John Russell
 * Created      : Jan 2002
 *
 * Copyright    : 2002-2010 by Imagination Technologies Limited.
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
 * $Log: usedisasm.c $
 *****************************************************************************/

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "sgxsupport.h"

#include "sgxdefs.h"

#include "use.h"
#include "useasm.h"
#include "usetab.h"

static IMG_UINT32 RotateLeft(IMG_UINT32 uValue, IMG_UINT32 uShift)
{
	return (uValue << (uShift & 0x1f)) | (uValue >> (0x20 - (uShift & 0x1f)));
}

static IMG_VOID usedisasm_asprintf(IMG_CHAR* pszInst, IMG_CHAR* pszFmt, ...) IMG_FORMAT_PRINTF(2, 3);

static IMG_VOID usedisasm_asprintf(IMG_CHAR* pszInst, IMG_CHAR* pszFmt, ...)
{
	va_list ap;

	va_start(ap, pszFmt);
	vsprintf(pszInst + strlen(pszInst), pszFmt, ap);
	va_end(ap);
}

typedef enum
{
	USEDISASM_REGTYPE_TEMP,
	USEDISASM_REGTYPE_PRIMATTR,
	USEDISASM_REGTYPE_OUTPUT,
	USEDISASM_REGTYPE_SECATTR,
	USEDISASM_REGTYPE_FPINTERNAL,
	USEDISASM_REGTYPE_SPECIAL,
	USEDISASM_REGTYPE_GLOBAL,
	USEDISASM_REGTYPE_FPCONSTANT,
	USEDISASM_REGTYPE_IMMEDIATE,
	USEDISASM_REGTYPE_INDEX,
	USEDISASM_REGTYPE_INDEXED,
	USEDISASM_REGTYPE_MAXIMUM,
} USEDISASM_REGTYPE;

static const IMG_BOOL g_pbBankUsesFormatControl[USEDISASM_REGTYPE_MAXIMUM] = 
{
	IMG_TRUE,	/* USEDISASM_REGTYPE_TEMP */
	IMG_TRUE,	/* USEDISASM_REGTYPE_PRIMATTR */
	IMG_TRUE,	/* USEDISASM_REGTYPE_OUTPUT */
	IMG_TRUE,	/* USEDISASM_REGTYPE_SECATTR */
	IMG_TRUE,	/* USEDISASM_REGTYPE_FPINTERNAL */
	IMG_FALSE,	/* USEDISASM_REGTYPE_SPECIAL */
	IMG_FALSE,	/* USEDISASM_REGTYPE_GLOBAL */
	IMG_FALSE,	/* USEDISASM_REGTYPE_FPCONSTANT */
	IMG_FALSE,	/* USEDISASM_REGTYPE_IMMEDIATE */
	IMG_FALSE,	/* USEDISASM_REGTYPE_INDEX */
	IMG_TRUE,	/* USEDISASM_REGTYPE_INDEXED */
};
static const USEASM_REGTYPE g_aeDecodeBank[USEDISASM_REGTYPE_MAXIMUM] =
{
	USEASM_REGTYPE_TEMP,		/* USEDISASM_REGTYPE_TEMP */
	USEASM_REGTYPE_PRIMATTR,	/* USEDISASM_REGTYPE_PRIMATTR */
	USEASM_REGTYPE_OUTPUT,		/* USEDISASM_REGTYPE_OUTPUT */
	USEASM_REGTYPE_SECATTR,		/* USEDISASM_REGTYPE_SECATTR */
	USEASM_REGTYPE_FPINTERNAL,	/* USEDISASM_REGTYPE_FPINTERNAL */
	USEASM_REGTYPE_UNDEF,		/* USEDISASM_REGTYPE_SPECIAL */
	USEASM_REGTYPE_GLOBAL,		/* USEDISASM_REGTYPE_GLOBAL */
	USEASM_REGTYPE_FPCONSTANT,	/* USEDISASM_REGTYPE_FPCONSTANT */
	USEASM_REGTYPE_IMMEDIATE,	/* USEDISASM_REGTYPE_IMMEDIATE */
	USEASM_REGTYPE_INDEX,		/* USEDISASM_REGTYPE_INDEX */
	USEASM_REGTYPE_UNDEF,		/* USEDISASM_REGTYPE_INDEXED */
};
static const IMG_PCHAR g_apszRegTypeName[] =
{
	"r",					/* USEASM_REGTYPE_TEMP */
	"o",					/* USEASM_REGTYPE_OUTPUT */
	"pa",					/* USEASM_REGTYPE_PRIMATTR */
	"sa",					/* USEASM_REGTYPE_SECATTR */
	"index",				/* USEASM_REGTYPE_INDEX */
	"g",					/* USEASM_REGTYPE_GLOBAL */
	"c",					/* USEASM_REGTYPE_FPCONSTANT */
	"i",					/* USEASM_REGTYPE_FPINTERNAL */
	"#",					/* USEASM_REGTYPE_IMMEDIATE */
	"pclink",				/* USEASM_REGTYPE_LINK */
	"drc",					/* USEASM_REGTYPE_DRC */
	"label",				/* USEASM_REGTYPE_LABEL */
	"p",					/* USEASM_REGTYPE_PREDICATE */
	"cp",					/* USEASM_REGTYPE_CLIPPLANE */
	"addressmode",			/* USEASM_REGTYPE_ADDRESSMODE */
	"swizzle",				/* USEASM_REGTYPE_SWIZZLE */
	"intsrcsel",			/* USEASM_REGTYPE_INTSRCSEL */
	"fcs",					/* USEASM_REGTYPE_FILTERCOEFF */
	"label_with_offset",	/* USEASM_REGTYPE_LABEL_WITH_OFFSET */
	"temp_named",			/* USEASM_REGTYPE_TEMP_NAMED */
	"floatimmediate",		/* USEASM_REGTYPE_FLOATIMMEDIATE */
	"ref",          		/* USEASM_REGTYPE_REF */
	"<invalid>",     		/* USEASM_REGTYPE_MAXIMUM */
};

typedef enum
{
	USEDISASM_INDEXREG_UNDEF,
	USEDISASM_INDEXREG_LOW,
	USEDISASM_INDEXREG_HIGH,
} USEDISASM_INDEXREG;

static IMG_VOID DecodeIndex(IMG_UINT32				uNum,
						    PUSE_REGISTER			psRegister,
						    IMG_UINT32				uFieldLength,
						    USEDISASM_INDEXREG		eIndexRegNum,
						    PCSGX_CORE_DESC			psTarget)
{
	IMG_UINT32			uOffsetFieldLength;
	IMG_UINT32			uBank;
	USEDISASM_REGTYPE	eBank;

	PVR_UNREFERENCED_PARAMETER(psTarget);

	#if defined(SUPPORT_SGX_FEATURE_USE_TWO_INDEX_BANKS)
	if (SupportsIndexTwoBanks(psTarget))
	{
		IMG_UINT32	uNonOffset;

		uOffsetFieldLength = uFieldLength - SGXVEC_USE_INDEX_NONOFFSET_NUM_BITS;
		uNonOffset = uNum >> uOffsetFieldLength;

		uBank = (uNonOffset & ~SGXVEC_USE_INDEX_NONOFFSET_BANK_CLRMSK) >> SGXVEC_USE_INDEX_NONOFFSET_BANK_SHIFT;
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_TWO_INDEX_BANKS) */
	{
		IMG_UINT32	uNonOffset;

		uOffsetFieldLength = uFieldLength - EURASIA_USE_INDEX_NONOFFSET_NUM_BITS;
		uNonOffset = uNum >> uOffsetFieldLength;

		uBank = (uNonOffset & ~EURASIA_USE_INDEX_NONOFFSET_BANK_CLRMSK) >> EURASIA_USE_INDEX_NONOFFSET_BANK_SHIFT;
		if (uNonOffset & EURASIA_USE_INDEX_NONOFFSET_IDXSEL)
		{
			eIndexRegNum = USEDISASM_INDEXREG_HIGH;
		}
		else
		{
			eIndexRegNum = USEDISASM_INDEXREG_LOW;
		}
	}

	switch (uBank)
	{
		case EURASIA_USE_INDEX_BANK_TEMP: eBank = USEDISASM_REGTYPE_TEMP; break;
		case EURASIA_USE_INDEX_BANK_OUTPUT: eBank = USEDISASM_REGTYPE_OUTPUT; break;
		case EURASIA_USE_INDEX_BANK_PRIMATTR: eBank = USEDISASM_REGTYPE_PRIMATTR; break;
		case EURASIA_USE_INDEX_BANK_SECATTR: eBank = USEDISASM_REGTYPE_SECATTR; break;
		default: IMG_ABORT();
	}

	psRegister->uType = g_aeDecodeBank[eBank];
	if (eIndexRegNum == USEDISASM_INDEXREG_LOW)
	{
		psRegister->uIndex = USEREG_INDEX_L;
	}
	else
	{
		psRegister->uIndex = USEREG_INDEX_H;
	}
	psRegister->uNumber = uNum & ((1 << uOffsetFieldLength) - 1);
}

static IMG_VOID DecodeRegister(PCSGX_CORE_DESC		psTarget,
							   IMG_BOOL				bDoubleRegisters,
							   USEDISASM_REGTYPE	eBank,
							   USEDISASM_INDEXREG	eIndexRegNum,
							   IMG_UINT32			uNum,
							   IMG_BOOL				bFmtControl,
							   IMG_UINT32			uAltFmtFlag,
							   IMG_UINT32			uNumFieldLength,
							   PUSE_REGISTER		psRegister)
/*****************************************************************************
 FUNCTION	: DecodeRegister

 PURPOSE	: Decodes an instruction argument back to the USEASM input
			  format.

 PARAMETERS	: psTarget				- Core to disassembly.
			  eBank					- Register bank for the argument.
			  eIndexRegNum			- Index register used by the argument.
			  uNum					- Encoded register number for the argument.
			  bFmtControl			- TRUE if the instruction uses format
									control.
			  uAltFmtFlag			- Alternate register format used by the
									instruction.
			  pszInst				- String to append the disassembly to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uFmtControl = bFmtControl ? EURASIA_USE_FMTSELECT : 0;
	IMG_UINT32	uNumTempsMappedToFPI;
	IMG_UINT32	uMaxRegNum;

	uNumTempsMappedToFPI = EURASIA_USE_NUM_TEMPS_MAPPED_TO_FPI;
	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	if (bDoubleRegisters)
	{
		uNumTempsMappedToFPI <<= SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT;
	}
	#else /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
	PVR_UNREFERENCED_PARAMETER(bDoubleRegisters);
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

	/*
		Check for cases where the top-bit of the register number
		selects between two different ways of interpreting the
		register contents.
	*/
	psRegister->uFlags = 0;
	if (bFmtControl && g_pbBankUsesFormatControl[eBank])
	{
		if (uNum & uFmtControl)
		{
			if (uAltFmtFlag == USEASM_ARGFLAGS_FMTF16)
			{
				psRegister->uFlags |= USEASM_ARGFLAGS_FMTF16;
			}
			else
			{
				psRegister->uFlags |= USEASM_ARGFLAGS_FMTC10;
			}
		}

		uNum &= ~uFmtControl;
		uNumFieldLength--;
	}

	uMaxRegNum = 1 << uNumFieldLength;

	/*
		The top few register numbers in the temporary register bank are aliases for
		the internal registers.
	*/
	if (eBank == USEDISASM_REGTYPE_TEMP && uNum >= (uMaxRegNum - uNumTempsMappedToFPI))
	{
		eBank = USEDISASM_REGTYPE_FPINTERNAL;
		uNum -= (uMaxRegNum - uNumTempsMappedToFPI);

		#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
		if (bDoubleRegisters)
		{
			uNum >>= SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
	}

	/*
		For the special register bank the top-bit of the register number selects
		between the global and constant register banks.
	*/
	if (eBank == USEDISASM_REGTYPE_SPECIAL)
	{
		 if (uNum & EURASIA_USE_SPECIAL_INTERNALDATA)
		 {
			 uNum &= ~EURASIA_USE_SPECIAL_INTERNALDATA;
			 eBank = USEDISASM_REGTYPE_GLOBAL;
		 }
		 else
		 {
			 eBank = USEDISASM_REGTYPE_FPCONSTANT;
		 }
	}

	if (eBank == USEDISASM_REGTYPE_INDEXED)
	{
		DecodeIndex(uNum, psRegister, uNumFieldLength, eIndexRegNum, psTarget);
	}
	else
	{
		assert(eBank < (sizeof(g_aeDecodeBank) / sizeof(g_aeDecodeBank[0])));
		assert(g_aeDecodeBank[eBank] != USEASM_REGTYPE_UNDEF);

		psRegister->uType = g_aeDecodeBank[eBank];
		psRegister->uNumber = uNum;
		psRegister->uIndex = USEREG_INDEX_NONE;
	}

	if ((psRegister->uFlags & USEASM_ARGFLAGS_FMTF16) && psRegister->uIndex == USEREG_INDEX_NONE)
	{
		if (psRegister->uNumber & EURASIA_USE_FMTF16_SELECTHIGH)
		{
			psRegister->uFlags |= (2 << USEASM_ARGFLAGS_COMP_SHIFT);
		}
		else
		{
			psRegister->uFlags |= (0 << USEASM_ARGFLAGS_COMP_SHIFT);
		}
		psRegister->uNumber >>= EURASIA_USE_FMTF16_REGNUM_SHIFT;
	}
}

#if defined(SUPPORT_SGX_FEATURE_USE_FNORMALISE) || defined(SUPPORT_SGX_FEATURE_USE_VEC34)
static
IMG_VOID SetupSwizzleSource(PUSE_REGISTER	psRegister,
						    IMG_UINT32		uSwizzle)
/*****************************************************************************
 FUNCTION	: SetupSwizzleSource

 PURPOSE	: Sets up a structure representing a swizzle in the USEASM
			  input format.

 PARAMETERS	: psRegister		- Register structure to set up.
			  uSwizzle			- Swizzle to set up.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psRegister->uType = USEASM_REGTYPE_SWIZZLE;
	psRegister->uNumber = uSwizzle;
	psRegister->uIndex = USEREG_INDEX_NONE;
	psRegister->uFlags = 0;
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_FNORMALISE) || defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

typedef struct
{
	IMG_UINT32	uFlag;
	IMG_PCHAR	pszFlagName;
} FLAG_DECODE, *PFLAG_DECODE, *const PCFLAG_DECODE;

static IMG_VOID PrintFlags(IMG_PCHAR			pszInst, 
						   FLAG_DECODE const*	psFlags, 
						   IMG_UINT32			uFlagsCount,
						   IMG_UINT32			uFlags)
{
	IMG_UINT32	uFlagIdx;

	for (uFlagIdx = 0; uFlagIdx < uFlagsCount; uFlagIdx++)
	{
		if (psFlags[uFlagIdx].uFlag & uFlags)
		{
			usedisasm_asprintf(pszInst, "%s", psFlags[uFlagIdx].pszFlagName);
		}
	}
}

static IMG_VOID PrintSwizzle(IMG_UINT32 uSwizzle, IMG_UINT32 uChanCount, IMG_PCHAR pszInst)
{
	IMG_UINT32	uChan;

	if (uChanCount > 1)
	{
		usedisasm_asprintf(pszInst, "swizzle(");
	}
	else
	{
		usedisasm_asprintf(pszInst, ".");
	}
	for (uChan = 0; uChan < uChanCount; uChan++)
	{
		IMG_UINT32	uSel;
		static const IMG_PCHAR apszSel[] =
		{
			"x",	/* USEASM_SWIZZLE_SEL_X */
			"y",	/* USEASM_SWIZZLE_SEL_Y */
			"z",	/* USEASM_SWIZZLE_SEL_Z */
			"w",	/* USEASM_SWIZZLE_SEL_W */
			"0",	/* USEASM_SWIZZLE_SEL_0 */
			"1",	/* USEASM_SWIZZLE_SEL_1 */
			"2",	/* USEASM_SWIZZLE_SEL_2 */
			"h",	/* USEASM_SWIZZLE_SEL_HALF */
		};

		uSel = (uSwizzle >> (USEASM_SWIZZLE_FIELD_SIZE * uChan)) & USEASM_SWIZZLE_VALUE_MASK;
		usedisasm_asprintf(pszInst, "%s", apszSel[uSel]);
	}
	if (uChanCount > 1)
	{
		usedisasm_asprintf(pszInst, ")");
	}
}

static IMG_VOID BasePrintRegister(PUSE_REGISTER		psRegister,
								  IMG_PCHAR			pszInst,
								  PUSE_INST			psInst)
{
	static const FLAG_DECODE asArgFlags[] =
	{
		{USEASM_ARGFLAGS_NEGATE,			".neg"},
		{USEASM_ARGFLAGS_ABSOLUTE,			".abs"},
		/* USEASM_ARGFLAGS_COMP */
		/* USEASM_ARGFLAGS_BYTEMSK */
		/* USEASM_ARGFLAGS_INVERT */
		{USEASM_ARGFLAGS_LOW,				".low"},
		{USEASM_ARGFLAGS_HIGH,				".high"},
		{USEASM_ARGFLAGS_ALPHA,				".alpha"},
		{USEASM_ARGFLAGS_COMPLEMENT,		".comp"},
		/* USEASM_ARGFLAGS_DISABLEWB */
		/* USEASM_ARGFLAGS_BYTEMSK_PRESENT */
		{USEASM_ARGFLAGS_FMTF32,			".flt32"},
		{USEASM_ARGFLAGS_FMTF16,			".flt16"},
		{USEASM_ARGFLAGS_FMTU8,				".u8"},
		{USEASM_ARGFLAGS_FMTC10,			".c10"},
		{USEASM_ARGFLAGS_REQUIREMOE,		".moe"},
	};
	static const IMG_PCHAR apszIntSrcSel[] =
	{
		"zero",			/* USEASM_INTSRCSEL_ZERO */
		"one",			/* USEASM_INTSRCSEL_ONE */
		"asat",			/* USEASM_INTSRCSEL_SRCALPHASAT	 */
		"rasat",		/* USEASM_INTSRCSEL_RSRCALPHASAT */
		"s0",			/* USEASM_INTSRCSEL_SRC0 */
		"s1",			/* USEASM_INTSRCSEL_SRC1 */
		"s2",			/* USEASM_INTSRCSEL_SRC2 */
		"s0a",			/* USEASM_INTSRCSEL_SRC0ALPHA */
		"s1a",			/* USEASM_INTSRCSEL_SRC1ALPHA */
		"s2a",			/* USEASM_INTSRCSEL_SRC2ALPHA */
		"Cx1",			/* USEASM_INTSRCSEL_CX1 */
		"Cx2",			/* USEASM_INTSRCSEL_CX2 */
		"Cx4",			/* USEASM_INTSRCSEL_CX4 */
		"Cx8",			/* USEASM_INTSRCSEL_CX8 */
		"Ax1",			/* USEASM_INTSRCSEL_AX1 */
		"Ax2",			/* USEASM_INTSRCSEL_AX2 */
		"Ax4",			/* USEASM_INTSRCSEL_AX4 */
		"Ax8",			/* USEASM_INTSRCSEL_AX8 */
		"add",			/* USEASM_INTSRCSEL_ADD */
		"sub",			/* USEASM_INTSRCSEL_SUB */
		"neg",			/* USEASM_INTSRCSEL_NEG */
		"none",			/* USEASM_INTSRCSEL_NONE */
		"min",			/* USEASM_INTSRCSEL_MIN */
		"max",			/* USEASM_INTSRCSEL_MAX */
		"s2scale",		/* USEASM_INTSRCSEL_SRC2SCALE */
		"zeros",		/* USEASM_INTSRCSEL_ZEROS */
		"comp",			/* USEASM_INTSRCSEL_COMP */
		"+src2.2",		/* USEASM_INTSRCSEL_PLUSSRC2DOT2 */
		"iadd",			/* USEASM_INTSRCSEL_IADD */
		"scale",		/* USEASM_INTSRCSEL_SCALE */
		"u8",			/* USEASM_INTSRCSEL_U8 */
		"s8",			/* USEASM_INTSRCSEL_S8 */
		"o8",			/* USEASM_INTSRCSEL_O8 */
		"reserved(=33)",
		"reserved(=34)",
		"reserved(=35)",
		"reserved(=36)",
		"reserved(=37)",
		"reserved(=38)",
		"z16",			/* USEASM_INTSRCSEL_Z16 */
		"u16",			/* USEASM_INTSRCSEL_S16 */
		"u32",			/* USEASM_INTSRCSEL_U32 */
		"cie",			/* USEASM_INTSRCSEL_CINENABLE */
		"coe",			/* USEASM_INTSRCSEL_COUTENABLE */
		"u16",			/* USEASM_INTSRCSEL_U16 */
		"interleaved",	/* USEASM_INTSRCSEL_INTERLEAVED */
		"planar",		/* USEASM_INTSRCSEL_PLANAR */
		"src01",		/* USEASM_INTSRCSEL_SRC01 */
		"src23",		/* USEASM_INTSRCSEL_SRC23 */
		"dst01",		/* USEASM_INTSRCSEL_DST01 */
		"dst23",		/* USEASM_INTSRCSEL_DST23 */
		"rnd",			/* USEASM_INTSRCSEL_RND */
		"twosided",		/* USEASM_INTSRCSEL_TWOSIDED */
		"fb",			/* USEASM_INTSRCSEL_FEEDBACK */
		"nfb",			/* USEASM_INTSRCSEL_NOFEEDBACK */
		"optdwd",		/* USEASM_INTSRCSEL_OPTDWD */
		"idst",			/* USEASM_INTSRCSEL_IDST */
		"down",			/* USEASM_INTSRCSEL_ROUNDDOWN */
		"nearest",		/* USEASM_INTSRCSEL_ROUNDNEAREST */
		"up",			/* USEASM_INTSRCSEL_ROUNDUP */
		"pixel",		/* USEASM_INTSRCSEL_PIXEL */
		"sample",		/* USEASM_INTSRCSEL_SAMPLE */
		"selective",	/* USEASM_INTSRCSEL_SELECTIVE */
		"pt",			/* USEASM_INTSRCSEL_PT */
		"vcull",		/* USEASM_INTSRCSEL_VCULL */
		"end",			/* USEASM_INTSRCSEL_END */
		"parallel",		/* USEASM_INTSRCSEL_PARALLEL */
		"perinstance",	/* USEASM_INTSRCSEL_PERINSTANCE */
		"srcneg",		/* USEASM_INTSRCSEL_SRCNEG */
		"srcabs",		/* USEASM_INTSRCSEL_SRCABS */
		"incuss",		/* USEASM_INTSRCSEL_INCREMENTUS */
		"incgpi",		/* USEASM_INTSRCSEL_INCREMENTGPI */
		"incboth",		/* USEASM_INTSRCSEL_INCREMENTBOTH */
		"moe",			/* USEASM_INTSRCSEL_INCREMENTMOE */
		"incall",		/* USEASM_INTSRCSEL_INCREMENTALL */
		"incs1",		/* USEASM_INTSRCSEL_INCREMENTS1 */
		"incs2",		/* USEASM_INTSRCSEL_INCREMENTS2 */
		"incs1s2",		/* USEASM_INTSRCSEL_INCREMENTS1S2 */
		"f32",			/* USEASM_INTSRCSEL_F32	*/
		"f16",			/* USEASM_INTSRCSEL_F16 */
		"or",			/* USEASM_INTSRCSEL_OR */
		"xor",			/* USEASM_INTSRCSEL_XOR */
		"and",			/* USEASM_INTSRCSEL_AND */
		"revsub",		/* USEASM_INTSRCSEL_REVSUB */
		"xchg",			/* USEASM_INTSRCSEL_XCHG */
		"true",			/* USEASM_INTSRCSEL_TRUE */
		"false",		/* USEASM_INTSRCSEL_FALSE */
		"imin",			/* USEASM_INTSRCSEL_IMIN */
		"imax",			/* USEASM_INTSRCSEL_IMAX */
		"umin",			/* USEASM_INTSRCSEL_UMIN */
		"umax",			/* USEASM_INTSRCSEL_UMAX */
	};

	if (psRegister->uFlags & USEASM_ARGFLAGS_INVERT)
	{
		usedisasm_asprintf(pszInst, "~");
	}
	if (psRegister->uFlags & USEASM_ARGFLAGS_DISABLEWB)
	{
		usedisasm_asprintf(pszInst, "!");
	}
	if (psRegister->uFlags & USEASM_ARGFLAGS_NOT)
	{
		usedisasm_asprintf(pszInst, "!");
	}

	if (psRegister->uIndex != USEREG_INDEX_NONE)
	{
		usedisasm_asprintf(pszInst, 
						   "%s[i%s + #%d]", 
						   g_apszRegTypeName[psRegister->uType], 
						   (psRegister->uIndex == USEREG_INDEX_L) ? "l" : "h",
						   psRegister->uNumber);
	}
	else
	{
		if (psRegister->uType == USEASM_REGTYPE_INDEX)
		{
			usedisasm_asprintf(pszInst, "i.%s%s", 
				(psRegister->uNumber & 0x1) ? "l" : "", 
				(psRegister->uNumber & 0x2) ? "h" : "");
		}
		else if (psRegister->uType == USEASM_REGTYPE_LINK)
		{
			usedisasm_asprintf(pszInst, "pclink");
		}
		else if (psRegister->uType == USEASM_REGTYPE_INTSRCSEL)
		{
			if (psRegister->uNumber < (sizeof(apszIntSrcSel) / sizeof(apszIntSrcSel[0])))
			{
				usedisasm_asprintf(pszInst, "%s", apszIntSrcSel[psRegister->uNumber]);
			}
			else
			{
				usedisasm_asprintf(pszInst, "reservedsrcsel%d", psRegister->uNumber);
			}
		}
		else if (psRegister->uType == USEASM_REGTYPE_SWIZZLE)
		{
			IMG_UINT32	uChanCount;

			uChanCount = 4;
			if (
					psInst != NULL &&
					(OpcodeDescFlags(psInst->uOpcode) & USE_DESCFLAG_DISASM_THREE_COMPONENT_SWIZZLE)
			   )
			{
				uChanCount = 3;
			}		

			PrintSwizzle(psRegister->uNumber, uChanCount, pszInst);
		}
		else if (psRegister->uType == USEASM_REGTYPE_ADDRESSMODE)
		{
			switch (psRegister->uNumber)
			{
				case EURASIA_USE1_MOECTRL_ADDRESSMODE_NONE: usedisasm_asprintf(pszInst, "none"); break;
				case EURASIA_USE1_MOECTRL_ADDRESSMODE_MIRROR: usedisasm_asprintf(pszInst, "mirror"); break;
				case EURASIA_USE1_MOECTRL_ADDRESSMODE_REPEAT: usedisasm_asprintf(pszInst, "repeat"); break;
				case EURASIA_USE1_MOECTRL_ADDRESSMODE_CLAMP: usedisasm_asprintf(pszInst, "clamp"); break;
				default: usedisasm_asprintf(pszInst, "addressmode_undefinedvalue%d", psRegister->uNumber);
			}
		}
		else if (
					psRegister->uType == USEASM_REGTYPE_IMMEDIATE &&
					psInst != NULL &&
					(
						psInst->uOpcode == USEASM_OP_MOV ||
						(
							(psRegister - psInst->asArg) == 2 &&
							(OpcodeDescFlags(psInst->uOpcode) & USE_DESCFLAG_DISASM_SHOW_SRC2_IMMEDIATES_IN_HEX)
						)
					)
				)
		{
			usedisasm_asprintf(pszInst, "#0x%.8X", psRegister->uNumber);
		}
		else if (
					psRegister->uType == USEASM_REGTYPE_IMMEDIATE &&
					psInst != NULL &&
					psInst->uOpcode == USEASM_OP_SMLSI &&
					(psRegister - psInst->asArg) >= 4 &&
					(psRegister - psInst->asArg) < 8
				)
		{
			usedisasm_asprintf(pszInst, "%s", psRegister->uNumber ? "swizzlemode" : "incrementmode");
		}
		else if (
					psRegister->uType == USEASM_REGTYPE_IMMEDIATE &&
					psInst != NULL &&
					(
						psInst->uOpcode == USEASM_OP_BR ||
						psInst->uOpcode == USEASM_OP_BA
					)
				)
		{
			IMG_INT32	iBranchDest = (IMG_INT32)psRegister->uNumber;

			if (iBranchDest < 0)
			{
				usedisasm_asprintf(pszInst, "-#0x%.8X", -iBranchDest);
			}
			else
			{
				usedisasm_asprintf(pszInst, "#0x%.8X", iBranchDest);
			}
		}
		else if (
					psInst != NULL &&
					(psInst->uOpcode == USEASM_OP_FIRH || psInst->uOpcode == USEASM_OP_FIRHH) &&
					(psRegister - psInst->asArg) == 5
				)
		{
			static const IMG_PCHAR apszEdgeMode[] = 
			{
				"rep",		/* EURASIA_USE1_FIRH_EDGEMODE_REPLICATE */
				"msc",		/* EURASIA_USE1_FIRH_EDGEMODE_MIRRORSINGLE */
				"mdc",		/* EURASIA_USE1_FIRH_EDGEMODE_MIRRORDOUBLE */
				"reserved"
			};
			assert(psRegister->uType == USEASM_REGTYPE_IMMEDIATE);
			assert(psRegister->uNumber < (sizeof(apszEdgeMode) / sizeof(apszEdgeMode[0])));
			usedisasm_asprintf(pszInst, "%s", apszEdgeMode[psRegister->uNumber]);
		}
		else
		{
			if (psRegister->uType < (sizeof(g_apszRegTypeName) / sizeof(g_apszRegTypeName[0])))
			{
				usedisasm_asprintf(pszInst, "%s", g_apszRegTypeName[psRegister->uType]);
			}
			else
			{
				usedisasm_asprintf(pszInst, "reservedregtype%d_", psRegister->uType);
			}
			usedisasm_asprintf(pszInst, "%d", psRegister->uNumber);
		}
	}
	

	PrintFlags(pszInst, asArgFlags, sizeof(asArgFlags) / sizeof(asArgFlags[0]), psRegister->uFlags);

	if (psRegister->uFlags & USEASM_ARGFLAGS_BYTEMSK_PRESENT)
	{
		IMG_UINT32	uByteMask;

		uByteMask = (psRegister->uFlags & ~USEASM_ARGFLAGS_BYTEMSK_CLRMSK) >> USEASM_ARGFLAGS_BYTEMSK_SHIFT;
		if (uByteMask != 0xF || (psInst != NULL && psInst->uOpcode == USEASM_OP_SOP2WM))
		{
			usedisasm_asprintf(pszInst, ".bytemask%s%s%s%s",
				(uByteMask & 8) ? "1" : "0",
				(uByteMask & 4) ? "1" : "0",
				(uByteMask & 2) ? "1" : "0",
				(uByteMask & 1) ? "1" : "0");
		}
	}
}

static IMG_VOID PrintDestRegister(PUSE_INST psInst, PUSE_REGISTER psRegister, IMG_PCHAR pszInst)
{
	IMG_UINT32	uComp;

	BasePrintRegister(psRegister, pszInst, psInst);

	uComp = (psRegister->uFlags & ~USEASM_ARGFLAGS_COMP_CLRMSK) >> USEASM_ARGFLAGS_COMP_SHIFT;
	if (uComp > 0)
	{
		usedisasm_asprintf(pszInst, ".%d", uComp);
	}
}

static IMG_VOID PrintSourceRegister(PUSE_INST psInst, PUSE_REGISTER psRegister, IMG_PCHAR pszInst)
{
	IMG_UINT32	uComp;
	IMG_UINT32	uFmad16Swizzle;

	BasePrintRegister(psRegister, pszInst, psInst);

	uComp = (psRegister->uFlags & ~USEASM_ARGFLAGS_COMP_CLRMSK) >> USEASM_ARGFLAGS_COMP_SHIFT;
	if (
			(
				(OpcodeDescFlags(psInst->uOpcode) & USE_DESCFLAG_DISASM_ALWAYS_SHOW_SRCCOMP) &&
				psRegister->uType != USEASM_REGTYPE_INTSRCSEL &&
				psRegister->uType != USEASM_REGTYPE_PREDICATE
			) ||
			(
				(OpcodeDescFlags(psInst->uOpcode) & USE_DESCFLAG_DISASM_ALWAYS_SHOW_SRCCOMPFORF16) &&
				(psRegister->uFlags & USEASM_ARGFLAGS_FMTF16)
			) ||
			(
				(OpcodeDescFlags(psInst->uOpcode) & USE_DESCFLAG_DISASM_ALWAYS_SHOW_SRCCOMPFORC10) &&
				(psRegister->uFlags & USEASM_ARGFLAGS_FMTC10)
			) ||
			uComp > 0
	   )
	{
		usedisasm_asprintf(pszInst, ".%d", uComp);
	}

	uFmad16Swizzle = (psRegister->uFlags & ~USEASM_ARGFLAGS_MAD16SWZ_CLRMSK) >> USEASM_ARGFLAGS_MAD16SWZ_SHIFT;
	switch (uFmad16Swizzle)
	{
		case USEASM_MAD16SWZ_LOWHIGH: break;
		case USEASM_MAD16SWZ_LOWLOW: usedisasm_asprintf(pszInst, ".swzll"); break;
		case USEASM_MAD16SWZ_HIGHHIGH: usedisasm_asprintf(pszInst, ".swzhh"); break;
		case USEASM_MAD16SWZ_CVTFROMF32: usedisasm_asprintf(pszInst, ".swzf32"); break;
	}
}

static IMG_VOID AdjustRegisterNumberUnits(USEDISASM_REGTYPE	eRegType,
										  IMG_PUINT32		puNum,
										  IMG_BOOL			bDoubleRegisters)
{
	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	if (bDoubleRegisters)
	{
		if (eRegType != USEDISASM_REGTYPE_SPECIAL && eRegType != USEDISASM_REGTYPE_IMMEDIATE)
		{
			(*puNum) <<= SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT;
		}
	}
	#else /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
	PVR_UNREFERENCED_PARAMETER(bDoubleRegisters);
	PVR_UNREFERENCED_PARAMETER(puNum);
	PVR_UNREFERENCED_PARAMETER(eRegType);
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
}

static USEDISASM_REGTYPE DecodeSrc0Bank(IMG_UINT32	uInst1,
										IMG_BOOL	bAllowExtended,
										IMG_UINT32	uExt)
/*****************************************************************************
 FUNCTION	: DecodeSrc0Bank

 PURPOSE	: Decode the register bank for source 0.

 PARAMETERS	: uInst1			- Encoded instruction.
			  bAllowExtended	- TRUE if the instruction supports extended
								source 0 register banks.
			  uExt				- Bit position of the extended bank flag.

 RETURNS	: The register type.
*****************************************************************************/
{
	IMG_UINT32			uBank;

	uBank = (IMG_UINT32)((uInst1 & ~EURASIA_USE1_S0BANK_CLRMSK) >> EURASIA_USE1_S0BANK_SHIFT);
	if (bAllowExtended && (uInst1 & uExt))
	{
		switch (uBank)
		{
			case EURASIA_USE1_S0EXTBANK_OUTPUT: return USEDISASM_REGTYPE_OUTPUT;
			case EURASIA_USE1_S0EXTBANK_SECATTR: return USEDISASM_REGTYPE_SECATTR;
			default: IMG_ABORT();
		}
	}
	else
	{
		switch (uBank)
		{
			case EURASIA_USE1_S0STDBANK_TEMP: return USEDISASM_REGTYPE_TEMP;
			case EURASIA_USE1_S0STDBANK_PRIMATTR: return USEDISASM_REGTYPE_PRIMATTR;
			default: IMG_ABORT();
		}
	}
}

static IMG_VOID DecodeSrc0WithNum(PCSGX_CORE_DESC	psTarget,
								  IMG_BOOL			bDoubleRegisters,
								  IMG_UINT32		uInst1,
								  IMG_BOOL			bAllowExtended,
								  IMG_UINT32		uExt,
								  PUSE_REGISTER		psSource,
								  IMG_BOOL			bFmtControl,
								  IMG_UINT32		uAltFmtFlag,
								  IMG_UINT32		uNumFieldLength,
								  IMG_UINT32		uNum)
/*****************************************************************************
 FUNCTION	: PrintSrc0WithNum

 PURPOSE	: Appends a text description of the source register of an instruction
		      to a string.

 PARAMETERS	: psTarget				- Core to disassembly.
			  uInst1				- Encoded instruction.
			  bAllowExtended		- TRUE if extended destination register banks
									are available.
			  uExt					- Mask to check for an extended register bank.
			  pszInst				- String to append to.
			  bFmtControl			- TRUE if the top-bit of the register number
									selects the format.
			  uAltFmtFlag			- Format selected by the top-bit of the
									register number.
			  uNum					- Register number field.

 RETURNS	: Nothing.
*****************************************************************************/
{
	USEDISASM_REGTYPE	eRegType;
	
	eRegType = DecodeSrc0Bank(uInst1, bAllowExtended, uExt);

	AdjustRegisterNumberUnits(eRegType, &uNum, bDoubleRegisters);

	DecodeRegister(psTarget, 
				   bDoubleRegisters, 
				   eRegType, 
				   USEDISASM_INDEXREG_UNDEF, 
				   uNum, 
				   bFmtControl, 
				   uAltFmtFlag, 
				   uNumFieldLength,
				   psSource);
}

static IMG_VOID DecodeSrc0(PCSGX_CORE_DESC psTarget,
						   IMG_UINT32 uInst0,
						   IMG_UINT32 uInst1,
						   IMG_BOOL bAllowExtended,
						   IMG_UINT32 uExt,
						   PUSE_REGISTER psRegister,
						   USEDIS_FORMAT_CONTROL_STATE eFmtControlState,
						   IMG_UINT32 uAltFmtFlag)
/*****************************************************************************
 FUNCTION	: DecodeSrc0

 PURPOSE	: Decodes source 0 to an instruction to the USEASM nput format.

 PARAMETERS	: psTarget				- Core to disassembly.
			  uInst0				- Encoded instruction.
			  uInst1
			  bAllowExtended		- TRUE if extended destination register banks
									are available.
			  uExt					- Mask to check for an extended register bank.
			  psRegister			- Destination for the decoded source.
			  bFmtControl			- TRUE if the top-bit of the register number
									selects the format.
			  uAltFmtFlag			- Format selected by the top-bit of the
									register number.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32			uNum;
	IMG_BOOL			bFmtControl;

	uNum = (IMG_UINT32)((uInst0 & ~EURASIA_USE0_SRC0_CLRMSK) >> EURASIA_USE0_SRC0_SHIFT);
	
	if (eFmtControlState == USEDIS_FORMAT_CONTROL_STATE_ON)
	{
		bFmtControl = IMG_TRUE;
	}
	else
	{
		bFmtControl = IMG_FALSE;
	}

	DecodeSrc0WithNum(psTarget,
					  IMG_FALSE /* bDoubleRegisters */,
					  uInst1,
					  bAllowExtended,
					  uExt,
					  psRegister,
					  bFmtControl,
					  uAltFmtFlag,
					  EURASIA_USE_REGISTER_NUMBER_BITS,
					  uNum);
}

static USEDISASM_REGTYPE DecodeDestBank(PCSGX_CORE_DESC		psTarget,
										IMG_UINT32			uBank,
										IMG_BOOL			bExtendedBank,
										USEDISASM_INDEXREG*	peIndexRegNum)
/*****************************************************************************
 FUNCTION	: DecodeDestBank

 PURPOSE	: Get the register bank used by an instruction destination.

 PARAMETERS	: psTarget			- Target core for disassembly.
			  uBank				- Destination bank field from the instruction.
			  bExtendedBank		- Extended bank flag from the instruction.
			  peIndexRegNum		- In some cases returns the index register used
								for the source register. 

 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psTarget);

	*peIndexRegNum = USEDISASM_INDEXREG_UNDEF;

	#if defined(SUPPORT_SGX_FEATURE_USE_TWO_INDEX_BANKS)
	if (SupportsIndexTwoBanks(psTarget))
	{
		if (!bExtendedBank && uBank == SGXVEC_USE1_D1STDBANK_INDEXED_IL)
		{
			*peIndexRegNum = USEDISASM_INDEXREG_LOW;
			return USEDISASM_REGTYPE_INDEXED;
		}
		if (bExtendedBank && uBank == SGXVEC_USE1_D1EXTBANK_INDEXED_IH)
		{
			*peIndexRegNum = USEDISASM_INDEXREG_HIGH;
			return USEDISASM_REGTYPE_INDEXED;
		}
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_TWO_INDEX_BANKS) */
	{
		if (!bExtendedBank && uBank == EURASIA_USE1_D1STDBANK_INDEXED)
		{
			return USEDISASM_REGTYPE_INDEXED;
		}
		if (bExtendedBank && uBank == EURASIA_USE1_D1EXTBANK_FPINTERNAL)
		{
			return USEDISASM_REGTYPE_FPINTERNAL;
		}
	}

	if (bExtendedBank)
	{
		switch (uBank)
		{
			case EURASIA_USE1_D1EXTBANK_SECATTR: return USEDISASM_REGTYPE_SECATTR;
			case EURASIA_USE1_D1EXTBANK_SPECIAL: return USEDISASM_REGTYPE_SPECIAL;
			case EURASIA_USE1_D1EXTBANK_INDEX: return USEDISASM_REGTYPE_INDEX;
			case EURASIA_USE1_D1EXTBANK_FPINTERNAL: return USEDISASM_REGTYPE_FPINTERNAL;
			default: IMG_ABORT();
		}
	}
	else
	{
		switch (uBank)
		{
			case EURASIA_USE1_D1STDBANK_TEMP: return USEDISASM_REGTYPE_TEMP;
			case EURASIA_USE1_D1STDBANK_OUTPUT: return USEDISASM_REGTYPE_OUTPUT;
			case EURASIA_USE1_D1STDBANK_PRIMATTR: return USEDISASM_REGTYPE_PRIMATTR;
			default: IMG_ABORT();
		}
	}
}

static IMG_VOID DecodeDestWithNum(PCSGX_CORE_DESC psTarget,
								  IMG_BOOL		 bDoubleRegisters,
								  IMG_UINT32     uInst1,
								  IMG_BOOL       bAllowExtended,
								  PUSE_REGISTER	 psDest,
								  IMG_BOOL       bFmtControl,
								  IMG_UINT32     uAltFmtFlag,
								  IMG_UINT32	 uNumFieldLength,
								  IMG_UINT32	 uNum)
/*****************************************************************************
 FUNCTION	: PrintDestWithNum

 PURPOSE	: Appends a text description of the destination register of an instruction
		      to a string.

 PARAMETERS	: psTarget				- Core to disassembly.
			  uInst1				- Encoded instruction.
			  bAllowExtended		- TRUE if extended destination register banks
									are available.
			  pszInst				- String to append to.
			  bFmtControl			- TRUE if the top-bit of the register number
									selects the format.
			  uAltFmtFlag			- Format selected by the top-bit of the
									register number.
			  uNum					- Register number field.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32			uBank;
	IMG_BOOL			bExtendedBank;
	USEDISASM_REGTYPE	eBank;
	USEDISASM_INDEXREG	eIndexRegNum;

	/*
		Decode the destination register bank.
	*/
	uBank = ((uInst1 & ~EURASIA_USE1_D1BANK_CLRMSK) >> EURASIA_USE1_D1BANK_SHIFT);
	if (bAllowExtended && (uInst1 & EURASIA_USE1_DBEXT) == EURASIA_USE1_DBEXT)
	{
		bExtendedBank = IMG_TRUE;
	}
	else
	{
		bExtendedBank = IMG_FALSE;
	}
	eBank = DecodeDestBank(psTarget, uBank, bExtendedBank, &eIndexRegNum);

	AdjustRegisterNumberUnits(eBank, &uNum, bDoubleRegisters);

	/*
		Print the register.
	*/
	DecodeRegister(psTarget, bDoubleRegisters, eBank, eIndexRegNum, uNum, bFmtControl, uAltFmtFlag, uNumFieldLength, psDest);
}

static IMG_VOID DecodeDest(PCSGX_CORE_DESC				psTarget,
						   IMG_UINT32					uInst0,
						   IMG_UINT32					uInst1,
						   IMG_BOOL						bAllowExtended,
						   PUSE_REGISTER				psDest,
						   USEDIS_FORMAT_CONTROL_STATE	eFmtControlState,
						   IMG_UINT32					uAltFmtFlag)
{
	IMG_UINT32	uNum = ((uInst0 & ~EURASIA_USE0_DST_CLRMSK) >> EURASIA_USE0_DST_SHIFT);
	IMG_BOOL	bFmtControl;

	if (eFmtControlState == USEDIS_FORMAT_CONTROL_STATE_ON)
	{
		bFmtControl = IMG_TRUE;
	}
	else
	{
		bFmtControl = IMG_FALSE;
	}

	DecodeDestWithNum(psTarget,
					  IMG_FALSE /* bDoubleRegisters */,
					  uInst1,
					  bAllowExtended,
					  psDest,
					  bFmtControl,
					  uAltFmtFlag,
					  EURASIA_USE_REGISTER_NUMBER_BITS,
					  uNum);
}

static USEDISASM_REGTYPE DecodeSrc12Bank(IMG_UINT32				uInst0,
										 IMG_UINT32				uInst1,
										 IMG_UINT32				uSrc,
										 IMG_BOOL				bAllowExtended,
										 IMG_UINT32				uExt,
										 PCSGX_CORE_DESC			psTarget,
										 USEDISASM_INDEXREG*	peIndexRegNum)
/*****************************************************************************
 FUNCTION	: DecodeSrc12Bank

 PURPOSE	: Get the register bank used by either source 1 or 2 to an
			  instruction.

 PARAMETERS	: uInst1, uInst0	- Instruction.
			  uSrc				- Index of the source to decode.
			  bAllowExtended	- TRUE if extended banks are supported by the
								instruction on this source.
			  uExt				- Bit position of the extended bank flag field.o.
			  psTarget			- Target core for disassembly.
			  peIndexRegNum		- In some cases returns the index register used
								for the source register.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uBank;
	IMG_BOOL	bExtendedBank;

	PVR_UNREFERENCED_PARAMETER(psTarget);

	if (peIndexRegNum != NULL)
	{
		*peIndexRegNum = USEDISASM_INDEXREG_UNDEF;
	}

	/*
		Unpack the register bank.
	*/
	if (uSrc == 1)
	{
		uBank = (uInst0 & ~EURASIA_USE0_S1BANK_CLRMSK) >> EURASIA_USE0_S1BANK_SHIFT;
	}
	else if (uSrc == 2)
	{
		uBank = (uInst0 & ~EURASIA_USE0_S2BANK_CLRMSK) >> EURASIA_USE0_S2BANK_SHIFT;
	}
	else
	{
		uBank = (uInst0 & ~EURASIA_USE0_S1BANK_CLRMSK) >> EURASIA_USE0_S1BANK_SHIFT;
	}

	/*
		Unpack the extended bank flag.
	*/
	if (bAllowExtended)
	{
		bExtendedBank = (uInst1 & uExt) ? IMG_TRUE : IMG_FALSE;
	}
	else
	{
		bExtendedBank = IMG_FALSE;
	}

	#if defined(SUPPORT_SGX_FEATURE_USE_TWO_INDEX_BANKS)
	if (SupportsIndexTwoBanks(psTarget))
	{
		if (bExtendedBank)
		{
			if (uBank == SGXVEC_USE0_S1EXTBANK_INDEXED_IL)
			{
				if (peIndexRegNum != NULL)
				{
					*peIndexRegNum = USEDISASM_INDEXREG_LOW;
				}
				return USEDISASM_REGTYPE_INDEXED;
			}
			if (uBank == SGXVEC_USE0_S1EXTBANK_INDEXED_IH)
			{
				if (peIndexRegNum != NULL)
				{
					*peIndexRegNum = USEDISASM_INDEXREG_HIGH;
				}
				return USEDISASM_REGTYPE_INDEXED;
			}
		}
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_TWO_INDEX_BANKS) */
	{
		if (bExtendedBank)
		{
			if (uBank == EURASIA_USE0_S1EXTBANK_INDEXED)
			{
				return USEDISASM_REGTYPE_INDEXED;		
			}
			if (uBank == EURASIA_USE0_S1EXTBANK_FPINTERNAL)
			{
				return USEDISASM_REGTYPE_FPINTERNAL;
			}
		}
	}

	if (bExtendedBank)
	{
		switch (uBank)
		{
			case EURASIA_USE0_S1EXTBANK_IMMEDIATE: return USEDISASM_REGTYPE_IMMEDIATE;
			case EURASIA_USE0_S1EXTBANK_SPECIAL: return USEDISASM_REGTYPE_SPECIAL;
			default: IMG_ABORT();
		}
	}
	else
	{
		switch (uBank)
		{
			case EURASIA_USE0_S1STDBANK_TEMP: return USEDISASM_REGTYPE_TEMP;
			case EURASIA_USE0_S1STDBANK_OUTPUT: return USEDISASM_REGTYPE_OUTPUT;
			case EURASIA_USE0_S1STDBANK_PRIMATTR: return USEDISASM_REGTYPE_PRIMATTR;
			case EURASIA_USE0_S1STDBANK_SECATTR: return USEDISASM_REGTYPE_SECATTR;
			default: IMG_ABORT();
		}
	}
}

static IMG_VOID DecodeSrc12WithNum(PCSGX_CORE_DESC	psTarget,
								   IMG_BOOL			bDoubleRegisters,
								   IMG_UINT32		uInst0,
								   IMG_UINT32		uInst1,
								   IMG_UINT32		uSrc,
								   IMG_BOOL			bAllowExtended,
								   IMG_UINT32		uExt,
								   PUSE_REGISTER	psSource,
								   IMG_BOOL			bFmtControl,
								   IMG_UINT32		uAltFmtFlag,
								   IMG_UINT32		uNumFieldLength,
								   IMG_UINT32		uNum)
/*****************************************************************************
 FUNCTION	: PrintSrc12

 PURPOSE	: Appends a text description of the source register of an instruction
		      to a string.

 PARAMETERS	: psTarget				- Core to disassembly.
			  uInst0				- Encoded instruction.
			  uInst1
			  uSrc					- Source to print (either 1 or 2).
			  bAllowExtended		- TRUE if extended source register banks
									are available.
			  uExt					- Mask to check for an extended register bank.
			  pszInst				- String to append to.
			  bFmtControl			- TRUE if the top-bit of the register number
									selects the format.
			  uAltFmtFlag			- Format selected by the top-bit of the
									register number.

 RETURNS	: Nothing.
*****************************************************************************/
{
	USEDISASM_REGTYPE	eBank;
	USEDISASM_INDEXREG	eIndexRegNum;

	eBank = DecodeSrc12Bank(uInst0, uInst1, uSrc, bAllowExtended, uExt, psTarget, &eIndexRegNum);

	AdjustRegisterNumberUnits(eBank, &uNum, bDoubleRegisters);

	DecodeRegister(psTarget, bDoubleRegisters, eBank, eIndexRegNum, uNum, bFmtControl, uAltFmtFlag, uNumFieldLength, psSource);
}

static IMG_VOID DecodeSrc12(PCSGX_CORE_DESC				psTarget,
						    IMG_UINT32					uInst0,
						    IMG_UINT32					uInst1,
						    IMG_UINT32					uSrc,
						    IMG_BOOL					bAllowExtended,
						    IMG_UINT32					uExt,
						    PUSE_REGISTER				psRegister,
						    USEDIS_FORMAT_CONTROL_STATE	eFmtControlState,
						    IMG_UINT32					uAltFmtFlag)
/*****************************************************************************
 FUNCTION	: DecodeSrc12

 PURPOSE	: Decode source 1 or 2 to an instruction back to the USEASM
			  input format.

 PARAMETERS	: psTarget				- Core to disassembly.
			  uInst0				- Encoded instruction.
			  uInst1
			  uSrc					- Source to decode (either 1 or 2).
			  bAllowExtended		- TRUE if extended source register banks
									are available.
			  uExt					- Mask to check for an extended register bank.
			  psRegister			- Returns the decoded source.
			  bFmtControl			- TRUE if the top-bit of the register number
									selects the format.
			  uAltFmtFlag			- Format selected by the top-bit of the
									register number.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32			uRegNum;
	IMG_BOOL			bFmtControl;

	if (uSrc == 1)
	{
		uRegNum = (uInst0 & ~EURASIA_USE0_SRC1_CLRMSK) >> EURASIA_USE0_SRC1_SHIFT;
	}
	else if (uSrc == 2)
	{
		uRegNum = (uInst0 & ~EURASIA_USE0_SRC2_CLRMSK) >> EURASIA_USE0_SRC2_SHIFT;
	}
	else
	{
		/*
			Special case for FIR: the register number comes from source 0 but
			the source bank comes from source 1.
		*/
		uRegNum = (uInst0 & ~EURASIA_USE0_SRC0_CLRMSK) >> EURASIA_USE0_SRC0_SHIFT;
	}

	if (eFmtControlState == USEDIS_FORMAT_CONTROL_STATE_ON)
	{
		bFmtControl = IMG_TRUE;
	}
	else
	{
		bFmtControl = IMG_FALSE;
	}

	DecodeSrc12WithNum(psTarget,
					   IMG_FALSE /* bDoubleRegisters */,
					   uInst0,
					   uInst1,
					   uSrc,
					   bAllowExtended,
					   uExt,
					   psRegister,
					   bFmtControl,
					   uAltFmtFlag,
					   EURASIA_USE_REGISTER_NUMBER_BITS,
					   uRegNum);
}

static IMG_VOID PrintPredicate(PUSE_INST	psInst,
							   IMG_PCHAR	pszInst)
/*****************************************************************************
 FUNCTION	: PrintPredicate

 PURPOSE	: Print the predicate associated with a USEASM input instruction.

 PARAMETERS	: psInst			- Instruction to print the predicate for.
			  pszInst			- String to append the printed predicate to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	static const IMG_PCHAR	pszPredicate[] =
	{
		"",		/* USEASM_PRED_NONE */
		"p0 ",	/* USEASM_PRED_P0 */
		"p1 ",	/* USEASM_PRED_P1 */
		"!p0 ",	/* USEASM_PRED_NEGP0 */
		"!p1 ",	/* USEASM_PRED_NEGP1 */
		"p2 ",	/* USEASM_PRED_P2 */
		"p3 ",	/* USEASM_PRED_P3 */
		"Pn ",	/* USEASM_PRED_PN */
		"!p2 ",	/* USEASM_PRED_NEGP2 */
	};
	usedisasm_asprintf(pszInst, "%s", 
		pszPredicate[(psInst->uFlags1 & ~USEASM_OPFLAGS1_PRED_CLRMSK) >> USEASM_OPFLAGS1_PRED_SHIFT]);
}

#define USEASM_TEST_LTZERO			((USEASM_TEST_ZERO_NONZERO << USEASM_TEST_ZERO_SHIFT) |			\
									 (USEASM_TEST_SIGN_NEGATIVE << USEASM_TEST_SIGN_SHIFT) |		\
									 USEASM_TEST_CRCOMB_AND)
#define USEASM_TEST_LTEZERO			((USEASM_TEST_ZERO_ZERO << USEASM_TEST_ZERO_SHIFT) |			\
									 (USEASM_TEST_SIGN_NEGATIVE << USEASM_TEST_SIGN_SHIFT) |		\
									 USEASM_TEST_CRCOMB_OR)
#define USEASM_TEST_GTEZERO			((USEASM_TEST_ZERO_ZERO << USEASM_TEST_ZERO_SHIFT) |			\
									 (USEASM_TEST_SIGN_POSITIVE << USEASM_TEST_SIGN_SHIFT) |		\
									 USEASM_TEST_CRCOMB_OR)
#define USEASM_TEST_EQZERO			((USEASM_TEST_ZERO_ZERO << USEASM_TEST_ZERO_SHIFT) |			\
									 (USEASM_TEST_SIGN_TRUE << USEASM_TEST_SIGN_SHIFT) |			\
									 USEASM_TEST_CRCOMB_AND)
#define USEASM_TEST_NEQZERO			((USEASM_TEST_ZERO_NONZERO << USEASM_TEST_ZERO_SHIFT) |		\
									 (USEASM_TEST_SIGN_TRUE << USEASM_TEST_SIGN_SHIFT) |			\
									 USEASM_TEST_CRCOMB_AND)

static IMG_VOID PrintTest(PUSE_INST psInst, IMG_UINT32 uTest, IMG_PCHAR pszInst)
/*****************************************************************************
 FUNCTION	: PrintTest

 PURPOSE	: Print the test associated with a USEASM input instruction.

 PARAMETERS	: psInst			- Instruction to print the test for.
			  pszInst			- String to append the printed test to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uZeroTest;
	IMG_UINT32	uSignTest;
	USEASM_TEST_CHANSEL	eChanSel;
	USEASM_TEST_MASK	eMaskType;
	static const IMG_PCHAR apszZeroTest[] = 
	{
		"t",	/* USEASM_TEST_ZERO_TRUE */
		"z",	/* USEASM_TEST_ZERO_ZERO */
		"nz",	/* USEASM_TEST_ZERO_NONZERO */
		"reserved_zerotest",
	};
	static const IMG_PCHAR apszSignTest[] =
	{
		"t",	/* USEASM_TEST_SIGN_TRUE */
		"n",	/* USEASM_TEST_SIGN_NEGATIVE */
		"p",	/* USEASM_TEST_SIGN_POSITIVE */
		"reserved_signtest",
	};
	static const IMG_PCHAR apszChanSel[] =
	{
		".chan0",		/* USEASM_TEST_CHANSEL_C0 */
		".chan1",		/* USEASM_TEST_CHANSEL_C1 */
		".chan2",		/* USEASM_TEST_CHANSEL_C2 */
		".chan3",		/* USEASM_TEST_CHANSEL_C3 */
		".andall",		/* USEASM_TEST_CHANSEL_ANDALL */
		".orall",		/* USEASM_TEST_CHANSEL_ORALL */
		".and02",		/* USEASM_TEST_CHANSEL_AND02 */
		".or02",		/* USEASM_TEST_CHANSEL_OR02 */
		".cperchan",	/* USEASM_TEST_CHANSEL_PERCHAN */
	};
	static const IMG_PCHAR apszMaskType[] =
	{
		"",				/* USEASM_TEST_MASK_NONE */
		".bmsk",		/* USEASM_TEST_MASK_BYTE */
		".wmsk",		/* USEASM_TEST_MASK_WORD */
		".dmsk",		/* USEASM_TEST_MASK_DWORD */
		".msk",			/* USEASM_TEST_MASK_PREC */
		".num",			/* USEASM_TEST_MASK_NUM */
	};

	/*
		Print a simplier version of the test specification for some instructions.
	*/
	if (OpcodeDescFlags(psInst->uOpcode) & USE_DESCFLAG_DISASM_SIMPLE_TESTS)
	{
		switch (uTest)
		{
			case USEASM_TEST_LTZERO: usedisasm_asprintf(pszInst, ".tn"); return;
			case USEASM_TEST_LTEZERO: usedisasm_asprintf(pszInst, ".tn|z"); return;
			case USEASM_TEST_EQZERO: usedisasm_asprintf(pszInst, ".tz"); return;
			case USEASM_TEST_NEQZERO: usedisasm_asprintf(pszInst, ".tnz"); return;
			case USEASM_TEST_GTEZERO: usedisasm_asprintf(pszInst, ".tp|z"); return;
		}
	}

	uSignTest = (uTest & ~USEASM_TEST_SIGN_CLRMSK) >> USEASM_TEST_SIGN_SHIFT;
	uZeroTest = (uTest & ~USEASM_TEST_ZERO_CLRMSK) >> USEASM_TEST_ZERO_SHIFT;

	usedisasm_asprintf(pszInst, ".test%s%s%s",
		apszSignTest[uSignTest],
		(uTest & USEASM_TEST_CRCOMB_AND) ? "a" : "o",
		apszZeroTest[uZeroTest]);

	eChanSel = (uTest & ~USEASM_TEST_CHANSEL_CLRMSK) >> USEASM_TEST_CHANSEL_SHIFT;
	eMaskType = (uTest & ~USEASM_TEST_MASK_CLRMSK) >> USEASM_TEST_MASK_SHIFT;

	if (eMaskType == USEASM_TEST_MASK_NONE)
	{
		if (eChanSel < (sizeof(apszChanSel) / sizeof(apszChanSel[0])))
		{
			usedisasm_asprintf(pszInst, "%s", apszChanSel[eChanSel]);
		}
		else
		{
			usedisasm_asprintf(pszInst, ".reservedchansel(%d)", eChanSel);
		}
	}
	else
	{
		if (eMaskType < (sizeof(apszMaskType) / sizeof(apszMaskType[0])))
		{
			usedisasm_asprintf(pszInst, "%s", apszMaskType[eMaskType]);
		}
		else
		{
			usedisasm_asprintf(pszInst, ".reservedmasktype(%d)", eMaskType);
		}
	}
}

static IMG_VOID PrintInstructionFlags(PUSE_INST	psInst,
									  IMG_PCHAR	pszInst)
/*****************************************************************************
 FUNCTION	: PrintInstructionFlags

 PURPOSE	: Print the modifier flags associated with a USEASM input instruction.

 PARAMETERS	: psInst			- Instruction to print the flags for.
			  pszInst			- String to append the printed flags to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	static const FLAG_DECODE	asOpFlags1[] =
	{
		{USEASM_OPFLAGS1_SKIPINVALID,	".skipinv"},
		/* USEASM_OPFLAGS1_MOVC_FMTI8 */
		{USEASM_OPFLAGS1_SYNCSTART,		".syncs"},
		{USEASM_OPFLAGS1_NOSCHED,		".nosched"},
		/* USEASM_OPFLAGS1_REPEAT */
		/* USEASM_OPFLAGS1_MASK */
		/* USEASM_OPFLAGS1_PRED */
		{USEASM_OPFLAGS1_END,			".end"},
		{USEASM_OPFLAGS1_SYNCEND,		".synce"},
		{USEASM_OPFLAGS1_SAVELINK,		".savelink"},
		{USEASM_OPFLAGS1_PARTIAL,		".partial"},
		{USEASM_OPFLAGS1_MOVC_FMTMASK,	".mask"},
		{USEASM_OPFLAGS1_MONITOR,		".mon"},
		{USEASM_OPFLAGS1_ABS,			".abs"},
		{USEASM_OPFLAGS1_ROUNDENABLE,	".renable"},
		/* USEASM_OPFLAGS1_MAINISSUE */
		{USEASM_OPFLAGS1_MOVC_FMTF16,	".flt16"},
		/* USEASM_OPFLAGS1_PREINCREMENT */
		/* USEASM_OPFLAGS1_POSTINCREMENT */
		/* USEASM_OPFLAGS1_RANGEENABLE */
		/* USEASM_OPFLAGS1_TESTENABLE */
		/* USEASM_OPFLAGS1_FETCHENABLE */
	};
	static const FLAG_DECODE	asOpFlags2[] =
	{
		{USEASM_OPFLAGS2_ONCEONLY,			".onceonly"},
		{USEASM_OPFLAGS2_BYPASSCACHE,		".bpcache"},
		{USEASM_OPFLAGS2_FORCELINEFILL,		".fcfill"},
		{USEASM_OPFLAGS2_TASKSTART,			".tasks"},
		{USEASM_OPFLAGS2_TASKEND,			".taske"},
		{USEASM_OPFLAGS2_SCALE,				".scale"},
		/* USEASM_OPFLAGS2_INCSGN */
		/* USEASM_OPFLAGS2_SAT */
		/* USEASM_OPFLAGS2_UNSIGNED */
		/* USEASM_OPFLAGS2_SIGNED */
		/* USEASM_OPFLAGS2_RSHIFT */
		{USEASM_OPFLAGS2_MOVC_FMTI16,		".i16"},
		{USEASM_OPFLAGS2_MOVC_FMTI32,		".i32"},
		{USEASM_OPFLAGS2_MOVC_FMTF32,		".flt"},
		{USEASM_OPFLAGS2_TYPEPRESERVE,		".tpres"},
		{USEASM_OPFLAGS2_MOVC_FMTI10,		".i10"},
		{USEASM_OPFLAGS2_FORMATSELECT,		".fsel"},
		{USEASM_OPFLAGS2_C10,				".c10"},
		{USEASM_OPFLAGS2_TOGGLEOUTFILES,	".tog"},
		{USEASM_OPFLAGS2_MOEPOFF,			".moepoff"},
		/* USEASM_OPFLAGS2_PERINSTMOE */
		/* USEASM_OPFLAGS2_PERINSTMOE_INCS0 */
		/* USEASM_OPFLAGS2_PERINSTMOE_INCS1 */
		/* USEASM_OPFLAGS2_PERINSTMOE_INCS2 */
	};
	static const FLAG_DECODE	asOpFlags3[] =
	{
		{USEASM_OPFLAGS3_PCFF32, ".pcf"},
		{USEASM_OPFLAGS3_PCFF16, ".pcff16"},
		{USEASM_OPFLAGS3_SAMPLEDATA, ".rsd"},
		{USEASM_OPFLAGS3_SAMPLEINFO, ".sinf"},
		{USEASM_OPFLAGS3_SYNCENDNOTTAKEN, ".syncent"},
		{USEASM_OPFLAGS3_TRIGGER, ".trigger"},
		{USEASM_OPFLAGS3_TWOPART, ".twopart"},
		{USEASM_OPFLAGS3_THREEPART, ".threepart"},
		{USEASM_OPFLAGS3_CUT, ".cut"},
		{USEASM_OPFLAGS3_BYPASSL1, ".bypassl1"},
		{USEASM_OPFLAGS3_GPIEXT, ".gpiext"},
		{USEASM_OPFLAGS3_GLOBAL, ".global"},
		{USEASM_OPFLAGS3_DM_NOMATCH, ".dm_nomatch"},
		{USEASM_OPFLAGS3_NOREAD, ".noread"},
		{USEASM_OPFLAGS3_PWAIT, ".pwait"},
		{USEASM_OPFLAGS3_SAMPLEDATAANDINFO, ".irsd"},
		{USEASM_OPFLAGS3_MINPACK, ".minp"},
		{USEASM_OPFLAGS3_STEP1, ".s1"},
		{USEASM_OPFLAGS3_STEP2, ".s2"},
		{USEASM_OPFLAGS3_SAMPLEMASK, ".smsk"},
		{USEASM_OPFLAGS3_FREEP,	".freep"},
		{USEASM_OPFLAGS3_ALLINSTANCES, ".allinst"},
		{USEASM_OPFLAGS3_ANYINSTANCES, ".anyinst"},
	};
	IMG_UINT32	uRepeatCount;
	IMG_UINT32	uRightShift;

	/*
		Print flags with a standard format.
	*/
	PrintFlags(pszInst, asOpFlags1, sizeof(asOpFlags1) / sizeof(asOpFlags1[0]), psInst->uFlags1);
	PrintFlags(pszInst, asOpFlags2, sizeof(asOpFlags2) / sizeof(asOpFlags2[0]), psInst->uFlags2);
	PrintFlags(pszInst, asOpFlags3, sizeof(asOpFlags3) / sizeof(asOpFlags3[0]), psInst->uFlags3);

	/*
		Print the rangeenable flag on memory store instructions only. On memory load instructions
		it's shown as an extra source argument (the register containing the range).
	*/
	if (OpcodeDescFlags(psInst->uOpcode) & USE_DESCFLAG_DISASM_MEMORY_ST)
	{
		if (psInst->uFlags1 & USEASM_OPFLAGS1_RANGEENABLE)
		{
			usedisasm_asprintf(pszInst, ".rangeenable");
		}
	}

	/*
		Print the repeat/fetch count.
	*/
	uRepeatCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;
	if (psInst->uFlags1 & USEASM_OPFLAGS1_FETCHENABLE)
	{
		usedisasm_asprintf(pszInst, ".fetch%d", uRepeatCount);
	}
	else
	{
		if (uRepeatCount > 1)
		{
			usedisasm_asprintf(pszInst, ".repeat%d", uRepeatCount);
		}
	}

	/*
		Print the signed/unsigned/sat flags.
	*/
	if (psInst->uFlags2 & (USEASM_OPFLAGS2_UNSIGNED | USEASM_OPFLAGS2_SIGNED))
	{
		if (psInst->uFlags2 & USEASM_OPFLAGS2_UNSIGNED)
		{
			usedisasm_asprintf(pszInst, ".u");
		}
		else
		{
			usedisasm_asprintf(pszInst, ".s");
		}
		if (psInst->uFlags2 & USEASM_OPFLAGS2_SAT)
		{
			usedisasm_asprintf(pszInst, "sat");
		}
	}
	else
	{
		if (psInst->uFlags2 & USEASM_OPFLAGS2_SAT)
		{
			usedisasm_asprintf(pszInst, ".sat");
		}
	}

	/*
		Print the destination right shift.
	*/
	uRightShift = (psInst->uFlags2 & ~USEASM_OPFLAGS2_RSHIFT_CLRMSK) >> USEASM_OPFLAGS2_RSHIFT_SHIFT;
	if (uRightShift > 0)
	{
		usedisasm_asprintf(pszInst, ".rs%d", uRightShift);
	}

	/*
		Print the per-instruction MOE increment state.
	*/
	if (psInst->uFlags2 & USEASM_OPFLAGS2_PERINSTMOE) 
	{
		IMG_UINT32	uRepeatCount;

		uRepeatCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;
		if (!(psInst->uOpcode == USEASM_OP_FMAD16 && uRepeatCount == 1))
		{
			usedisasm_asprintf(pszInst, ".incs%s%s%s",
				(psInst->uFlags2 & USEASM_OPFLAGS2_PERINSTMOE_INCS0) ? "1" : "0",
				(psInst->uFlags2 & USEASM_OPFLAGS2_PERINSTMOE_INCS1) ? "1" : "0",
				(psInst->uFlags2 & USEASM_OPFLAGS2_PERINSTMOE_INCS2) ? "1" : "0");
		}
	}

	/*
		Print the MOVC/MOVMSK I8/U8 format select.
	*/
	if (psInst->uFlags1 & USEASM_OPFLAGS1_MOVC_FMTI8)
	{
		if (psInst->uOpcode == USEASM_OP_MOVMSK)
		{
			usedisasm_asprintf(pszInst, ".u8");
		}
		else
		{
			usedisasm_asprintf(pszInst, ".i8");
		}
	}

	/*
		Print TEST instruction parameters.
	*/
	if (psInst->uFlags1 & USEASM_OPFLAGS1_TESTENABLE)
	{
		PrintTest(psInst, psInst->uTest, pszInst);
	}
}

static IMG_VOID PrintWriteMask(IMG_UINT32	uWriteMask,
							   IMG_PCHAR	pszInst)
{
	IMG_UINT32	uChan;

	usedisasm_asprintf(pszInst, ".");
	for (uChan = 0; uChan < 4; uChan++)
	{
		static const IMG_CHAR pszChan[] = {'x', 'y', 'z', 'w'};

		if (uWriteMask & (1 << uChan))
		{
			usedisasm_asprintf(pszInst, "%c", pszChan[uChan]);
		}
	}
}

static IMG_BOOL IsDestinationMaskDefault(PCSGX_CORE_DESC	psTarget,
										 PUSE_INST		psInst,
										 PUSE_REGISTER	psDestArg,
										 IMG_UINT32		uMask)
/*****************************************************************************
 FUNCTION	: IsDestinationMaskDefault

 PURPOSE	: Check if printing of the destination mask/repeat mask on an
			  instruction can be skipping without leaving out any information.

 PARAMETERS	: psTarget			- Target core for which the instruction is assembled/disassembled.
		      psInst			- Instruction to check.
			  psDestArg			- Destination argument.
			  uMask				- Mask to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psTarget);

	if (uMask != 0x1)
	{
		return IMG_FALSE;
	}
	if (
			(OpcodeDescFlags(psInst->uOpcode) & USE_DESCFLAG_DISASM_ALWAYS_SHOW_MASK) &&
			!(psDestArg->uFlags & USEASM_ARGFLAGS_DISABLEWB)
	   )
	{
		return IMG_FALSE;
	}
	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	if (
			SupportsVEC34(psTarget) &&
			(OpcodeDescFlags(psInst->uOpcode) & USE_DESCFLAG_DISASM_ALWAYS_SHOW_MASK_FOR_VECTOR) != 0
	   )
	{
		return IMG_FALSE;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
	return IMG_TRUE;
}

static IMG_VOID PrintStandardInstruction(PCSGX_CORE_DESC	psTarget,
										 PUSE_INST		psInst,
										 IMG_PCHAR		pszInst)
/*****************************************************************************
 FUNCTION	: PrintStandardInstruction

 PURPOSE	: Print a USEASM input instruction.

 PARAMETERS	: psTarget			- Target core for which the instruction is assembled/disassembled.
		      psInst			- Instruction to print.
			  pszInst			- String to append a description of the instruction to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uArg;
	IMG_BOOL	bFirstArg;
	IMG_UINT32	uArgumentCount;

	/*
		Print instruction predicate.
	*/
	PrintPredicate(psInst, pszInst);

	/*
		Print instruction opcode.
	*/
	usedisasm_asprintf(pszInst, "%s", OpcodeName(psInst->uOpcode));

	/*
		Print instruction modifier flags.
	*/
	PrintInstructionFlags(psInst, pszInst);
	usedisasm_asprintf(pszInst, " ");

	/*
		Add an extra argument for the predicate destination of a
		test instruction.
	*/
	uArgumentCount = OpcodeArgumentCount(psInst->uOpcode);
	if (
			(psInst->uFlags1 & USEASM_OPFLAGS1_TESTENABLE) &&
			((psInst->uTest & ~USEASM_TEST_MASK_CLRMSK) >> USEASM_TEST_MASK_SHIFT) == USEASM_TEST_MASK_NONE &&
			psInst->uOpcode != USEASM_OP_MOVC &&
			psInst->uOpcode != USEASM_OP_VMOVC &&
			psInst->uOpcode != USEASM_OP_VMOVCU8
	   )
	{
		uArgumentCount++;
	}

	/*
		Print instruction arguments.
	*/
	bFirstArg = IMG_TRUE;
	for (uArg = 0; uArg < uArgumentCount; uArg++)
	{
		PUSE_REGISTER	psArg = &psInst->asArg[uArg];

		if (
				psArg->uType == USEASM_REGTYPE_INTSRCSEL && 
				psArg->uNumber == USEASM_INTSRCSEL_NONE &&
				psInst->uOpcode != USEASM_OP_PHASIMM
		   )
		{
			continue;
		}
		if (!bFirstArg)
		{
			usedisasm_asprintf(pszInst, ", ");
		}
		bFirstArg = IMG_FALSE;

		if (uArg > 0)
		{
			PrintSourceRegister(psInst, psArg, pszInst);
		}
		else
		{
			PrintDestRegister(psInst, psArg, pszInst);
		}

		/*
			Print the repeat mask/vector write mask after the destination argument.
		*/
		if (uArg == 0)
		{
			IMG_UINT32	uMask;

			uMask = (psInst->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT;
			if (!IsDestinationMaskDefault(psTarget, psInst, psArg, uMask))
			{
				PrintWriteMask(uMask, pszInst);
			}
		}
	}
}

static IMG_VOID SetupDRCSource(PUSE_REGISTER	psRegister,
							   IMG_UINT32		uDRC)
/*****************************************************************************
 FUNCTION	: SetupDRCSource

 PURPOSE	: Sets up a structure representing a dependent read counter in the USEASM
			  input format.

 PARAMETERS	: psRegister		- Register structure to set up.
			  uDRC				- Index of the dependent read counter.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psRegister->uType = USEASM_REGTYPE_DRC;
	psRegister->uNumber = uDRC;
	psRegister->uIndex = USEREG_INDEX_NONE;
	psRegister->uFlags = 0;
}

static IMG_VOID SetupIntSrcSel(PUSE_REGISTER	psRegister,
							   USEASM_INTSRCSEL	eSrcSel)
/*****************************************************************************
 FUNCTION	: SetupIntSrcSel

 PURPOSE	: Sets up a structure representing a source selector in the USEASM
			  input format.

 PARAMETERS	: psRegister		- Register structure to set up.
			  eSrcSel			- Source selector.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psRegister->uType = USEASM_REGTYPE_INTSRCSEL;
	psRegister->uNumber = eSrcSel;
	psRegister->uIndex = USEREG_INDEX_NONE;
	psRegister->uFlags = 0;
}

#if defined(SUPPORT_SGX_FEATURE_USE_INTEGER_CONDITIONALS)
static IMG_VOID SetupPredicate(PUSE_REGISTER	psRegister,
							   IMG_UINT32		uPredicate,
							   IMG_BOOL			bNegate)
/*****************************************************************************
 FUNCTION	: SetupPredicate

 PURPOSE	: Sets up a structure representing a predicate in the USEASM
			  input format.

 PARAMETERS	: psRegister		- Register structure to set up.
			  uPredicate		- Predicate number.
			  bNegate			- TRUE if the predicate is negated.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psRegister->uType = USEASM_REGTYPE_PREDICATE;
	psRegister->uNumber = uPredicate;
	psRegister->uIndex = USEREG_INDEX_NONE;
	psRegister->uFlags = 0;
	if (bNegate)
	{
		psRegister->uFlags |= USEASM_ARGFLAGS_NOT;
	}
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_INTEGER_CONDITIONALS) */

static IMG_VOID SetupImmediateSource(PUSE_REGISTER	psRegister,
									 IMG_UINT32		uImmediate)
/*****************************************************************************
 FUNCTION	: SetupImmediateSource

 PURPOSE	: Sets up a structure representing an immediate value in the USEASM
			  input format.

 PARAMETERS	: psRegister		- Register structure to set up.
			  uImmediate		- Immediate value.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psRegister->uType = USEASM_REGTYPE_IMMEDIATE;
	psRegister->uNumber = uImmediate;
	psRegister->uIndex = USEREG_INDEX_NONE;
	psRegister->uFlags = 0;
}

static IMG_VOID SetupAddressModeSource(PUSE_REGISTER	psRegister,
									   IMG_UINT32		uAddressMode)
/*****************************************************************************
 FUNCTION	: SetupAddressModeSource

 PURPOSE	: Sets up a structure representing an MOE address mode in the USEASM
			  input format.

 PARAMETERS	: psRegister		- Register structure to set up.
			  uAddressMode		- Address mode value.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psRegister->uType = USEASM_REGTYPE_ADDRESSMODE;
	psRegister->uNumber = uAddressMode;
	psRegister->uIndex = USEREG_INDEX_NONE;
	psRegister->uFlags = 0;
}

static IMG_UINT32 SignExt(IMG_UINT32 uValue, IMG_UINT32 uSign)
{
	return ((uValue & (1 << uSign)) ? (0xFFFFFFFF & ~((1 << uSign) - 1)) : 0) | uValue;
}

static const USEASM_PRED g_aeDecodeEPred[] = 
	{
		USEASM_PRED_NONE,	/* EURASIA_USE1_EPRED_ALWAYS */
		USEASM_PRED_P0,		/* EURASIA_USE1_EPRED_P0 */
		USEASM_PRED_P1,		/* EURASIA_USE1_EPRED_P1 */
		USEASM_PRED_P2,		/* EURASIA_USE1_EPRED_P2 */
		USEASM_PRED_P3,		/* EURASIA_USE1_EPRED_P3 */
		USEASM_PRED_NEGP0,	/* EURASIA_USE1_EPRED_NOTP0 */
		USEASM_PRED_NEGP1,	/* EURASIA_USE1_EPRED_NOTP1 */
		USEASM_PRED_PN		/* EURASIA_USE1_EPRED_PNMOD4 */
	};

static const USEASM_PRED g_aeDecodeSPred[] = 
	{
		USEASM_PRED_NONE,	/* EURASIA_USE1_SPRED_ALWAYS */
		USEASM_PRED_P0,		/* EURASIA_USE1_SPRED_P0 */
		USEASM_PRED_P1,		/* EURASIA_USE1_SPRED_P1 */
		USEASM_PRED_NEGP0,	/* EURASIA_USE1_SPRED_NOTP0 */
	};

static IMG_VOID DecodeLdStCacheFlags(PCSGX_CORE_DESC	psTarget,
									 IMG_UINT32		uInst1,
									 #if defined(SUPPORT_SGX_FEATURE_USE_LDST_EXTENDED_CACHE_MODES)
									 IMG_UINT32		uExtFlag,
									 #endif /* defined(SUPPORT_SGX_FEATURE_USE_LDST_EXTENDED_CACHE_MODES) */
									 PUSE_INST		psInst)
{
	PVR_UNREFERENCED_PARAMETER(psTarget);

	#if defined(SUPPORT_SGX_FEATURE_USE_LDST_EXTENDED_CACHE_MODES)
	if (SupportsLDSTExtendedCacheModes(psTarget))
	{
		if (uInst1 & uExtFlag)
		{
			if (uInst1 & EURASIA_USE1_LDST_DCCTL_EXTBYPASSL1)
			{
				psInst->uFlags3 |= USEASM_OPFLAGS3_BYPASSL1;
			}
			else
			{
				psInst->uFlags2 |= USEASM_OPFLAGS2_FORCELINEFILL;
			}
		}
		else
		{
			if (uInst1 & EURASIA_USE1_LDST_DCCTL_STDBYPASSL1L2)
			{
				psInst->uFlags2 |= USEASM_OPFLAGS2_BYPASSCACHE;
			}
		}
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_LDST_EXTENDED_CACHE_MODES) */
	{
		if (uInst1 & EURASIA_USE1_LDST_BPCACHE)
		{
			psInst->uFlags2 |= USEASM_OPFLAGS2_BYPASSCACHE;
		}
		if (uInst1 & EURASIA_USE1_LDST_FCLFILL)
		{
			psInst->uFlags2 |= USEASM_OPFLAGS2_FORCELINEFILL;
		}
	}
}

#if defined(SUPPORT_SGX_FEATURE_USE_EXTENDED_LOAD)
/*****************************************************************************
 FUNCTION	: DecodeELD

 PURPOSE	: Decode a extended load instruction to the USEASM
			  input format.

 PARAMETERS	: psTarget			- Target processor.
			  uInst1, uInst0	- Instruction to decode.
			  psInst			- Returns the decoded instruction.

 RETURNS	: Status code.
*****************************************************************************/
static USEDISASM_ERROR DecodeELD(PCSGX_CORE_DESC	psTarget,
								 IMG_UINT32		uInst0,
								 IMG_UINT32		uInst1,
								 PUSE_INST		psInst)
{
	IMG_UINT32	uOffset0, uOffset1;
	IMG_UINT32	uMaskCount = (uInst1 & ~EURASIA_USE1_RMSKCNT_CLRMSK) >> EURASIA_USE1_RMSKCNT_SHIFT;

	/*
		Decode instruction opcode.
	*/
	if (uInst1 & SGX545_USE1_EXTLD_DTYPE_128BIT)
	{
		psInst->uOpcode = USEASM_OP_ELDQ;
	}
	else
	{
		psInst->uOpcode = USEASM_OP_ELDD;
	}

	/*
		Decode instruction modifiers.
	*/
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_SYNCSTART)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SYNCSTART;
	}
	DecodeLdStCacheFlags(psTarget, uInst1, SGX545_USE1_EXTLD_DCCTLEXT, psInst);
	if (uMaskCount > 0)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_FETCHENABLE;
		psInst->uFlags1 |= ((uMaskCount + 1) << USEASM_OPFLAGS1_REPEAT_SHIFT);
	}

	/*
		Decode instruction destination.
	*/
	psInst->asArg[0].uNumber = (uInst0 & ~EURASIA_USE0_DST_CLRMSK) >> EURASIA_USE0_DST_SHIFT;
	if (uInst1 & SGX545_USE1_EXTLD_DBANK_PRIMATTR)
	{
		psInst->asArg[0].uType = USEASM_REGTYPE_PRIMATTR;
	}
	else
	{
		psInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
	}


	/*
		Decode base source.
	*/
	DecodeSrc12(psTarget, uInst0, uInst1, 1, IMG_FALSE, 0, &psInst->asArg[1], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);

	/*
		Decode offset source.
	*/
	DecodeSrc0(psTarget, uInst0, uInst1, IMG_FALSE, 0, &psInst->asArg[2], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	
	/*
		Decode immediate offset source.
	*/
	uOffset0 = (uInst1 & ~SGX545_USE1_EXTLD_OFFSET0_CLRMSK) >> SGX545_USE1_EXTLD_OFFSET0_SHIFT;
	uOffset0 <<= SGX545_USE1_EXTLD_OFFSET0_INTERNALSHIFT;
	uOffset1 = (uInst1 & ~SGX545_USE1_EXTLD_OFFSET1_CLRMSK) >> SGX545_USE1_EXTLD_OFFSET1_SHIFT;
	uOffset1 <<= SGX545_USE1_EXTLD_OFFSET1_INTERNALSHIFT;
	SetupImmediateSource(&psInst->asArg[3], uOffset0 + uOffset1);

	/*
		Decode range source.
	*/
	DecodeSrc12(psTarget, uInst0, uInst1, 2, IMG_FALSE, 0, &psInst->asArg[4], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);

	/*
		Decode dependent read counter source.
	*/
	SetupDRCSource(&psInst->asArg[5], (uInst1 & ~EURASIA_USE1_LDST_DRCSEL_CLRMSK) >> EURASIA_USE1_LDST_DRCSEL_SHIFT);

	return USEDISASM_OK;
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_EXTENDED_LOAD) */

/*****************************************************************************
 FUNCTION	: DecodeLdSt

 PURPOSE	: Decode a load/store instruction to the USEASM
			  input format.

 PARAMETERS	: psTarget			- Target processor.
			  uInst1, uInst0	- Instruction to decode.
			  psInst			- Returns the decoded instruction.

 RETURNS	: Status code..
*****************************************************************************/
static USEDISASM_ERROR DecodeLdSt(PCSGX_CORE_DESC psTarget,
								  IMG_UINT32     uInst0,
								  IMG_UINT32     uInst1,
								  PUSE_INST		 psInst)
{
	IMG_UINT32 uMaskCount = (uInst1 & ~EURASIA_USE1_RMSKCNT_CLRMSK) >> EURASIA_USE1_RMSKCNT_SHIFT;
	IMG_UINT32 uIMode = (uInst1 & ~EURASIA_USE1_LDST_IMODE_CLRMSK) >> EURASIA_USE1_LDST_IMODE_SHIFT;
	IMG_UINT32 uOpcode = (uInst1 & ~EURASIA_USE1_OP_CLRMSK) >> EURASIA_USE1_OP_SHIFT;
	USEASM_PRED ePredicate;
	IMG_UINT32 uDataType;
	IMG_UINT32 uAddressMode;
	PUSE_REGISTER psNextArg;
	static const USEASM_OPCODE aeDecodeLdOp[4][4] =
	{
		/* EURASIA_USE1_LDST_DTYPE_32BIT */
		{
			USEASM_OP_LDAD,		/* EURASIA_USE1_LDST_AMODE_ABSOLUTE */
			USEASM_OP_LDLD,		/* EURASIA_USE1_LDST_AMODE_LOCAL */
			USEASM_OP_LDTD,		/* EURASIA_USE1_LDST_AMODE_TILED */
			USEASM_OP_UNDEF,	/* EURASIA_USE1_LDST_AMODE_RESERVED */
		},
		/* EURASIA_USE1_LDST_DTYPE_16BIT */
		{
			USEASM_OP_LDAW,		/* EURASIA_USE1_LDST_AMODE_ABSOLUTE */
			USEASM_OP_LDLW,		/* EURASIA_USE1_LDST_AMODE_LOCAL */
			USEASM_OP_LDTW,		/* EURASIA_USE1_LDST_AMODE_TILED */
			USEASM_OP_UNDEF,	/* EURASIA_USE1_LDST_AMODE_RESERVED */
		},
		/* EURASIA_USE1_LDST_DTYPE_8BIT */
		{
			USEASM_OP_LDAB,		/* EURASIA_USE1_LDST_AMODE_ABSOLUTE */
			USEASM_OP_LDLB,		/* EURASIA_USE1_LDST_AMODE_LOCAL */
			USEASM_OP_LDTB,		/* EURASIA_USE1_LDST_AMODE_TILED */
			USEASM_OP_UNDEF,	/* EURASIA_USE1_LDST_AMODE_RESERVED */
		},
		/* EURASIA_USE1_LDST_DTYPE_RESERVED */
		{
			USEASM_OP_UNDEF,	/* EURASIA_USE1_LDST_AMODE_ABSOLUTE */
			USEASM_OP_UNDEF,	/* EURASIA_USE1_LDST_AMODE_LOCAL */
			USEASM_OP_UNDEF,	/* EURASIA_USE1_LDST_AMODE_TILED */
			USEASM_OP_UNDEF,	/* EURASIA_USE1_LDST_AMODE_RESERVED */
		},
	};
	static const USEASM_OPCODE aeDecodeStOp[4][4] =
	{
		/* EURASIA_USE1_LDST_DTYPE_32BIT */
		{
			USEASM_OP_STAD,		/* EURASIA_USE1_LDST_AMODE_ABSOLUTE */
			USEASM_OP_STLD,		/* EURASIA_USE1_LDST_AMODE_LOCAL */
			USEASM_OP_STTD,		/* EURASIA_USE1_LDST_AMODE_TILED */
			USEASM_OP_UNDEF,	/* EURASIA_USE1_LDST_AMODE_RESERVED */
		},
		/* EURASIA_USE1_LDST_DTYPE_16BIT */
		{
			USEASM_OP_STAW,		/* EURASIA_USE1_LDST_AMODE_ABSOLUTE */
			USEASM_OP_STLW,		/* EURASIA_USE1_LDST_AMODE_LOCAL */
			USEASM_OP_STTW,		/* EURASIA_USE1_LDST_AMODE_TILED */
			USEASM_OP_UNDEF,	/* EURASIA_USE1_LDST_AMODE_RESERVED */
		},
		/* EURASIA_USE1_LDST_DTYPE_8BIT */
		{
			USEASM_OP_STAB,		/* EURASIA_USE1_LDST_AMODE_ABSOLUTE */
			USEASM_OP_STLB,		/* EURASIA_USE1_LDST_AMODE_LOCAL */
			USEASM_OP_STTB,		/* EURASIA_USE1_LDST_AMODE_TILED */
			USEASM_OP_UNDEF,	/* EURASIA_USE1_LDST_AMODE_RESERVED */
		},
		/* EURASIA_USE1_LDST_DTYPE_RESERVED */
		{
			USEASM_OP_UNDEF,	/* EURASIA_USE1_LDST_AMODE_ABSOLUTE */
			USEASM_OP_UNDEF,	/* EURASIA_USE1_LDST_AMODE_LOCAL */
			USEASM_OP_UNDEF,	/* EURASIA_USE1_LDST_AMODE_TILED */
			USEASM_OP_UNDEF,	/* EURASIA_USE1_LDST_AMODE_RESERVED */
		},
	};
	#if defined(SUPPORT_SGX_FEATURE_USE_LDST_QWORD)
	static const USEASM_OPCODE aeDecodeQwordLdOp[] =
	{
		USEASM_OP_LDAQ,		/* EURASIA_USE1_LDST_AMODE_ABSOLUTE */
		USEASM_OP_LDLQ,		/* EURASIA_USE1_LDST_AMODE_LOCAL */
		USEASM_OP_LDTQ,		/* EURASIA_USE1_LDST_AMODE_TILED */
		USEASM_OP_UNDEF,	/* EURASIA_USE1_LDST_AMODE_RESERVED */
	};
	static const USEASM_OPCODE aeDecodeQwordStOp[] =
	{
		USEASM_OP_STAQ,		/* EURASIA_USE1_LDST_AMODE_ABSOLUTE */
		USEASM_OP_STLQ,		/* EURASIA_USE1_LDST_AMODE_LOCAL */
		USEASM_OP_STTQ,		/* EURASIA_USE1_LDST_AMODE_TILED */
		USEASM_OP_UNDEF,	/* EURASIA_USE1_LDST_AMODE_RESERVED */
	};
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_LDST_QWORD) */

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*
		Decode predicate.
	*/
	ePredicate = g_aeDecodeEPred[(uInst1 & ~EURASIA_USE1_EPRED_CLRMSK) >> EURASIA_USE1_EPRED_SHIFT];
	psInst->uFlags1 |= (ePredicate << USEASM_OPFLAGS1_PRED_SHIFT);

	#if defined(SUPPORT_SGX_FEATURE_USE_EXTENDED_LOAD)
	/*
		Check for an extended LD instruction.
	*/
	if (uOpcode == EURASIA_USE1_OP_LD && SupportsExtendedLoad(psTarget) && (uInst1 & SGX545_USE1_LDST_EXTENDED))
	{
		return DecodeELD(psTarget, uInst0, uInst1, psInst);	
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_EXTENDED_LOAD) */

	/*
		Decode opcode.
	*/
	uAddressMode = (uInst1 & ~EURASIA_USE1_LDST_AMODE_CLRMSK) >> EURASIA_USE1_LDST_AMODE_SHIFT;
	uDataType = (uInst1 & ~EURASIA_USE1_LDST_DTYPE_CLRMSK) >> EURASIA_USE1_LDST_DTYPE_SHIFT;
	#if defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS)
	if (SupportsLDSTAtomicOps(psTarget) && uAddressMode == SGXVEC_USE1_LD_AMODE_ATOMIC)
	{
		if (uOpcode == EURASIA_USE1_OP_LD)
		{
			psInst->uOpcode = USEASM_OP_LDATOMIC;
		}
		else
		{
			/*
				Atomic operations are only supported when the OP=LD. 
			*/
			return USEDISASM_ERROR_INVALID_LDST_ATOMIC_OP;
		}
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS) */
	#if defined(SUPPORT_SGX_FEATURE_USE_LDST_QWORD)
	if (SupportsQwordLDST(psTarget) && uDataType == SGXVEC_USE1_LDST_DTYPE_QWORD)
	{
		if (uOpcode == EURASIA_USE1_OP_LD)
		{
			psInst->uOpcode = aeDecodeQwordLdOp[uAddressMode];
		}
		else
		{
			psInst->uOpcode = aeDecodeQwordStOp[uAddressMode];
		}
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_LDST_QWORD) */
	{
		if (uOpcode == EURASIA_USE1_OP_LD)
		{
			psInst->uOpcode = aeDecodeLdOp[uDataType][uAddressMode];
		}
		else
		{
			psInst->uOpcode = aeDecodeStOp[uDataType][uAddressMode];
		}
	}
	if (psInst->uOpcode == USEASM_OP_UNDEF)
	{
		return USEDISASM_ERROR_INVALID_LDST_DATA_SIZE;
	}
	
	/*
		Decode instruction modifiers.
	*/
	if (IsEnhancedNoSched(psTarget) && (uInst1 & EURASIA_USE1_LDST_NOSCHED))
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	DecodeLdStCacheFlags(psTarget, 
						 uInst1, 
						 #if defined(SUPPORT_SGX_FEATURE_USE_LDST_EXTENDED_CACHE_MODES)
						 EURASIA_USE1_LDST_DCCTLEXT, 
						 #endif /* defined(SUPPORT_SGX_FEATURE_USE_LDST_EXTENDED_CACHE_MODES) */
						 psInst);
	#if defined(SUPPORT_SGX_FEATURE_USE_ST_NO_READ_BEFORE_WRITE)
	if (uOpcode == EURASIA_USE1_OP_ST && SupportsSTNoReadBeforeWrite(psTarget) && (uInst1 & SGX545_USE1_ST_NORDBEF))
	{
		psInst->uFlags3 |= USEASM_OPFLAGS3_NOREAD;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_ST_NO_READ_BEFORE_WRITE) */
	if (uInst1 & EURASIA_USE1_SYNCSTART)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SYNCSTART;
	}
	if (!(uInst1 & EURASIA_USE1_LDST_MOEEXPAND))
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_FETCHENABLE;
	}
	#if defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS)
	if (psInst->uOpcode == USEASM_OP_LDATOMIC)
	{
		/*
			Atomic operations can't be repeated.
		*/
		if ((psInst->uFlags1 & USEASM_OPFLAGS1_FETCHENABLE) == 0)
		{
			psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);
		}
		else
		{
			psInst->uFlags1 |= (1 << USEASM_OPFLAGS1_REPEAT_SHIFT);
		}
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC) */
	{
		psInst->uFlags1 |= ((uMaskCount + 1) << USEASM_OPFLAGS1_REPEAT_SHIFT);
	}

	if (uInst1 & EURASIA_USE1_LDST_RANGEENABLE)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_RANGEENABLE;
	}
	
	/*
		Decode destination.
	*/
	psNextArg = &psInst->asArg[0];
	if (uOpcode == EURASIA_USE1_OP_LD)
	{
		psNextArg->uNumber = (uInst0 & ~EURASIA_USE0_DST_CLRMSK) >> EURASIA_USE0_DST_SHIFT; 
		if (uInst1 & EURASIA_USE1_LDST_DBANK_PRIMATTR)
		{
			psNextArg->uType = USEASM_REGTYPE_PRIMATTR;
		}
		else
		{
			psNextArg->uType = USEASM_REGTYPE_TEMP;
		}
		psNextArg++;
	}

	/*
		Decode base argument.
	*/
	DecodeSrc0(psTarget, uInst0, uInst1, TRUE, EURASIA_USE1_S0BEXT, psNextArg, USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	psNextArg++;

	/*
		Decode offset argument.
	*/
	DecodeSrc12(psTarget, uInst0, uInst1, 1, TRUE, EURASIA_USE1_S1BEXT, psNextArg, USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	psNextArg++;

	if (uOpcode == EURASIA_USE1_OP_ST)
	{
		/*
			Decode the argument with the data to store.
		*/
		DecodeSrc12(psTarget, uInst0, uInst1, 2, TRUE, EURASIA_USE1_S2BEXT, psNextArg, USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
		psNextArg++;
	}
	else
	{
		if (
				#if defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS)
				psInst->uOpcode == USEASM_OP_LDATOMIC ||
				#endif /* defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS) */
				(uInst1 & EURASIA_USE1_LDST_RANGEENABLE) != 0
			)
		{
			/*
				Decode the range argument.
			*/
			DecodeSrc12(psTarget, uInst0, uInst1, 2, TRUE, EURASIA_USE1_S2BEXT, psNextArg, USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
			psNextArg++;
		}

		/*
			Decode the DRC argument.
		*/
		SetupDRCSource(psNextArg, (uInst1 & ~EURASIA_USE1_LDST_DRCSEL_CLRMSK) >> EURASIA_USE1_LDST_DRCSEL_SHIFT);
		psNextArg++;
	}

	/*
		Decode the increment argument.
	*/
	switch (uIMode)
	{
		case EURASIA_USE1_LDST_IMODE_POST: psInst->uFlags1 |= USEASM_OPFLAGS1_POSTINCREMENT; break;
		case EURASIA_USE1_LDST_IMODE_PRE: psInst->uFlags1 |= USEASM_OPFLAGS1_PREINCREMENT; break;
	}

	/*
		Decode the increment/decrement flag.
	*/
	if (uInst1 & EURASIA_USE1_LDST_INCSGN)
	{
		psInst->uFlags2 |= USEASM_OPFLAGS2_INCSGN;
	}

	#if defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS)
	/*
		Decode the atomic operation type.
	*/
	if (psInst->uOpcode == USEASM_OP_LDATOMIC)
	{
		IMG_UINT32			uAtomicOp = (uInst1 & ~SGXVEC_USE1_LD_ATOMIC_OP_CLRMSK) >> SGXVEC_USE1_LD_ATOMIC_OP_SHIFT;
		USEASM_INTSRCSEL	eAtomicOp;

		switch (uAtomicOp)
		{
			case SGXVEC_USE1_LD_ATOMIC_OP_ADD: 
			{
				eAtomicOp = USEASM_INTSRCSEL_ADD; 
				break;
			}
			case SGXVEC_USE1_LD_ATOMIC_OP_SUB: 
			{
				eAtomicOp = USEASM_INTSRCSEL_SUB; 
				break;
			}
			case SGXVEC_USE1_LD_ATOMIC_OP_XCHG: 
			{
				eAtomicOp = USEASM_INTSRCSEL_XCHG; 
				break;
			}
			case SGXVEC_USE1_LD_ATOMIC_OP_AND: 
			{
				eAtomicOp = USEASM_INTSRCSEL_AND; 
				break;
			}
			case SGXVEC_USE1_LD_ATOMIC_OP_OR: 
			{
				eAtomicOp = USEASM_INTSRCSEL_OR; 
				break;
			}
			case SGXVEC_USE1_LD_ATOMIC_OP_XOR: 
			{
				eAtomicOp = USEASM_INTSRCSEL_XOR; 
				break;
			}
			case SGXVEC_USE1_LD_ATOMIC_OP_MIN: 
			{
				if ((uInst1 & SGXVEC_USE1_LD_ATOMIC_DATASGN) != 0)
				{
					eAtomicOp = USEASM_INTSRCSEL_IMIN; 
				}
				else
				{
					eAtomicOp = USEASM_INTSRCSEL_UMIN; 
				}
				break;
			}
			case SGXVEC_USE1_LD_ATOMIC_OP_MAX: 
			{
				if ((uInst1 & SGXVEC_USE1_LD_ATOMIC_DATASGN) != 0)
				{
					eAtomicOp = USEASM_INTSRCSEL_IMAX; 
				}
				else
				{
					eAtomicOp = USEASM_INTSRCSEL_UMAX; 
				}
				break;
			}
			default:
			{
				return USEDISASM_ERROR_INVALID_LDST_ATOMIC_OP;
			}
		}

		SetupIntSrcSel(psNextArg, eAtomicOp);
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS) */

	return USEDISASM_OK;
}

/*****************************************************************************
 FUNCTION	: DisassembleLdSt

 PURPOSE	: Disassembles the load/store instruction.

 PARAMETERS	: psTarget			- Target processor.
			  uInst1, uInst0	- Instruction to disassemble.
			  pszInst			- String to append the disassembly to.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID DisassembleLdSt(PUSE_INST	   psInst,
								IMG_PCHAR	   pszInst)
{
	PUSE_REGISTER	psNextArg;

	/*
		Print instruction predicate.
	*/
	PrintPredicate(psInst, pszInst);

	/*
		Print instruction opcode.
	*/
	usedisasm_asprintf(pszInst, "%s", OpcodeName(psInst->uOpcode));

	/*
		Print instruction modifier flags.
	*/
	PrintInstructionFlags(psInst, pszInst);
	usedisasm_asprintf(pszInst, " ");

	psNextArg = &psInst->asArg[0];

	if (!(OpcodeDescFlags(psInst->uOpcode) & USE_DESCFLAG_DISASM_MEMORY_ST))
	{
		/*
			Print destination argument.
		*/
		PrintDestRegister(psInst, psNextArg, pszInst);
		psNextArg++;
		usedisasm_asprintf(pszInst, ", ");
	}

	usedisasm_asprintf(pszInst, "[");

	/*
		Print base source argument.
	*/
	PrintSourceRegister(psInst, psNextArg, pszInst);
	psNextArg++;

	usedisasm_asprintf(pszInst, ",");

	/*
		Print pre-increment/pre-decrement flags.
	*/
	if (psInst->uFlags1 & USEASM_OPFLAGS1_PREINCREMENT)
	{
		if (psInst->uFlags2 & USEASM_OPFLAGS2_INCSGN)
		{
			usedisasm_asprintf(pszInst, "--");
		}
		else
		{
			usedisasm_asprintf(pszInst, "++");
		}
	}
	else if (psInst->uOpcode == USEASM_OP_ELDD || psInst->uOpcode == USEASM_OP_ELDQ)
	{
		usedisasm_asprintf(pszInst, " ");
	}
	else if (!(psInst->uFlags1 & USEASM_OPFLAGS1_POSTINCREMENT))
	{
		if (psInst->uFlags2 & USEASM_OPFLAGS2_INCSGN)
		{
			usedisasm_asprintf(pszInst, "-");
		}
		else
		{
			usedisasm_asprintf(pszInst, "+");
		}
	}

	/*
		Print offset source argument.
	*/
	PrintSourceRegister(psInst, psNextArg, pszInst);
	psNextArg++;

	/*
		Print post-increment/post-decrement flags.
	*/
	if (psInst->uFlags1 & USEASM_OPFLAGS1_POSTINCREMENT)
	{
		if (psInst->uFlags2 & USEASM_OPFLAGS2_INCSGN)
		{
			usedisasm_asprintf(pszInst, "--");
		}
		else
		{
			usedisasm_asprintf(pszInst, "++");
		}
	}

	/*
		Print extra immediate offset argument.
	*/
	if (psInst->uOpcode == USEASM_OP_ELDD || psInst->uOpcode == USEASM_OP_ELDQ)
	{
		usedisasm_asprintf(pszInst, ", ");
		PrintSourceRegister(psInst, psNextArg, pszInst);
		psNextArg++;
	}

	usedisasm_asprintf(pszInst, "], ");

	if (OpcodeDescFlags(psInst->uOpcode) & USE_DESCFLAG_DISASM_MEMORY_ST)
	{
		/*
			Print source data to be stored.
		*/
		PrintSourceRegister(psInst, psNextArg, pszInst);
		psNextArg++;
	}
	else 
	{
		if (
				(psInst->uFlags1 & USEASM_OPFLAGS1_RANGEENABLE) != 0 || 
				#if defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS)
				psInst->uOpcode == USEASM_OP_LDATOMIC ||
				#endif /* defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS) */
				psInst->uOpcode == USEASM_OP_ELDD || 
				psInst->uOpcode == USEASM_OP_ELDQ
		   )
		{
			/*
				Print range argument.
			*/
			PrintSourceRegister(psInst, psNextArg, pszInst);
			psNextArg++;

			usedisasm_asprintf(pszInst, ", ");
		}

		/*
			Print DRC argument.
		*/
		PrintSourceRegister(psInst, psNextArg, pszInst);
		psNextArg++;
	}

	#if defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS)
	if (psInst->uOpcode == USEASM_OP_LDATOMIC)
	{
		/*
			Print atomic operation type.
		*/
		usedisasm_asprintf(pszInst, ", ");
		PrintSourceRegister(psInst, psNextArg, pszInst);
		psNextArg++;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS) */
}

/*****************************************************************************
 FUNCTION	: DecodeFlowControl

 PURPOSE	: Decodes a flow control instruction back to the input format.

 PARAMETERS	: psTarget			- Target processor.
			  uInst1, uInst0	- Instruction to disassemble.
			  psInst			- Written with the decoded instruction.

 RETURNS	: Error status.
*****************************************************************************/
static USEDISASM_ERROR DecodeFlowControl(PCSGX_CORE_DESC psTarget,
									     IMG_UINT32     uInst0,
									     IMG_UINT32     uInst1,
									     PUSE_INST      psInst)
{
	IMG_UINT32 uOpcat = 
		(uInst1 & ~EURASIA_USE1_FLOWCTRL_OP2_CLRMSK) >> EURASIA_USE1_FLOWCTRL_OP2_SHIFT;

	/*
		Initialize the instruction with default values.
	*/
	UseAsmInitInst(psInst);

	switch (uOpcat)
	{
		case EURASIA_USE1_FLOWCTRL_OP2_NOP:
		{
			psInst->uOpcode = USEASM_OP_PADDING;

			psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

			if (uInst1 & EURASIA_USE1_FLOWCTRL_NOP_SYNCS)
			{
				psInst->uFlags1 |= USEASM_OPFLAGS1_SYNCSTART;
			}
			if (uInst0 &  EURASIA_USE0_FLOWCTRL_NOP_TOGGLEOUTFILES)
			{
				psInst->uFlags2 |= USEASM_OPFLAGS2_TOGGLEOUTFILES;
			}
			if (uInst1 & EURASIA_USE1_END)
			{
				psInst->uFlags1 |= USEASM_OPFLAGS1_END;
			}
			if (uInst1 & EURASIA_USE1_NOSCHED)
			{
				psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
			}
			#if defined(SUPPORT_SGX_FEATURE_USE_NOPTRIGGER)
			if (SupportsNopTrigger(psTarget) && (uInst0 & EURASIA_USE0_FLOWCTRL_NOP_TRIGGER))
			{
				psInst->uFlags3 |= USEASM_OPFLAGS3_TRIGGER;
			}
			#endif /* defined(SUPPORT_SGX_FEATURE_USE_NOPTRIGGER) */
			return USEDISASM_OK;
		}
		case EURASIA_USE1_FLOWCTRL_OP2_BA:
		case EURASIA_USE1_FLOWCTRL_OP2_BR:
		{
			IMG_UINT32 uOffset;
			IMG_UINT32 uPCBits = NumberOfProgramCounterBits(psTarget);
			IMG_UINT32 uPCMax = (1 << uPCBits) - 1;
			IMG_UINT32 uEPred;

			psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

			if (IsEnhancedNoSched(psTarget) && (uInst1 & EURASIA_USE1_FLOWCTRL_NOSCHED))
			{
				psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
			}

			#if defined(SUPPORT_SGX_FEATURE_USE_BRANCH_EXCEPTION)
			if (SupportsBranchException(psTarget) && (uInst1 & EURASIA_USE1_FLOWCTRL_EXCEPTION))
			{
				psInst->uOpcode = USEASM_OP_BEXCEPTION;

				return USEDISASM_OK;
			}
			#endif /* defined(SUPPORT_SGX_FEATURE_USE_BRANCH_EXCEPTION) */

			uEPred = (uInst1 & ~EURASIA_USE1_EPRED_CLRMSK) >> EURASIA_USE1_EPRED_SHIFT;
			psInst->uFlags1 |= (g_aeDecodeEPred[uEPred] << USEASM_OPFLAGS1_PRED_SHIFT);

			if (uOpcat == EURASIA_USE1_FLOWCTRL_OP2_BA)
			{
				psInst->uOpcode = USEASM_OP_BA;
			}
			else
			{
				psInst->uOpcode = USEASM_OP_BR;
			}
			if (uInst1 & EURASIA_USE1_BRANCH_SAVELINK)
			{
				psInst->uFlags1 |= USEASM_OPFLAGS1_SAVELINK;
			}
			if (NumberOfMonitorsSupported(psTarget) > 0 && (uInst1 & SGX545_USE1_BRANCH_MONITOR))
			{
				psInst->uFlags1 |= USEASM_OPFLAGS1_MONITOR;
			}
			#if defined(SUPPORT_SGX_FEATURE_USE_BRANCH_PWAIT)
			if (SupportsBranchPwait(psTarget) && (uInst1 & SGXVEC_USE1_FLOWCTRL_PWAIT))
			{
				psInst->uFlags3 |= USEASM_OPFLAGS3_PWAIT;
			}
			#endif /* defined(SUPPORT_SGX_FEATURE_USE_BRANCH_PWAIT) */
			#if defined(SUPPORT_SGX_FEATURE_USE_BRANCH_EXTSYNCEND)
			if (
					SupportsExtSyncEnd(psTarget) && 
					(uInst1 & EURASIA_USE1_FLOWCTRL_SYNCEXT) &&
					(uInst1 & EURASIA_USE1_FLOWCTRL_SYNCEND)
			   )
			{
				psInst->uFlags3 |= USEASM_OPFLAGS3_SYNCENDNOTTAKEN;
			}
			else
			#endif /* defined(SUPPORT_SGX_FEATURE_USE_BRANCH_EXTSYNCEND) */
			if (uInst1 & EURASIA_USE1_FLOWCTRL_SYNCEND)
			{
				psInst->uFlags1 |= USEASM_OPFLAGS1_SYNCEND;
			}

			#if defined(SUPPORT_SGX_FEATURE_USE_BRANCH_ALL_ANY_INSTANCES)
			/*
				Decode the .ALLINSTS flag.
			*/
			if (
					SupportsAllAnyInstancesFlagOnBranchInstruction(psTarget) &&
					!FixBRN29643(psTarget)
			   )
			{
				if ((uInst0 & SGXVEC_USE0_FLOWCTRL_ALLINSTANCES) != 0)
				{
					psInst->uFlags3 |= USEASM_OPFLAGS3_ALLINSTANCES;
				}
				if ((uInst0 & SGXVEC_USE0_FLOWCTRL_ANYINSTANCES) != 0)
				{
					psInst->uFlags3 |= USEASM_OPFLAGS3_ANYINSTANCES;
				}
			}
			#endif /* defined(SUPPORT_SGX_FEATURE_USE_BRANCH_ALL_ANY_INSTANCES) */
			

			uOffset = (uInst0 & uPCMax) >> EURASIA_USE0_BRANCH_OFFSET_SHIFT;
			if (uOpcat != EURASIA_USE1_FLOWCTRL_OP2_BA && (uOffset & (1 << (uPCBits - 1))) != 0)
			{
				uOffset |= 0xFFFFFFFF << uPCBits;
			}
			SetupImmediateSource(&psInst->asArg[0], uOffset);
			
			return USEDISASM_OK;
		}
		case EURASIA_USE1_FLOWCTRL_OP2_LAPC:
		{
			IMG_UINT32	uEPred;

			uEPred = (uInst1 & ~EURASIA_USE1_EPRED_CLRMSK) >> EURASIA_USE1_EPRED_SHIFT;
			psInst->uFlags1 |= (g_aeDecodeEPred[uEPred] << USEASM_OPFLAGS1_PRED_SHIFT);

			psInst->uOpcode = USEASM_OP_LAPC;

			psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

			if (IsEnhancedNoSched(psTarget) && (uInst1 & EURASIA_USE1_FLOWCTRL_NOSCHED))
			{
				psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
			}
			#if defined(SUPPORT_SGX_FEATURE_USE_BRANCH_PWAIT)
			if (SupportsBranchPwait(psTarget) && (uInst1 & SGXVEC_USE1_FLOWCTRL_PWAIT))
			{
				psInst->uFlags3 |= USEASM_OPFLAGS3_PWAIT;
			}
			#endif /* defined(SUPPORT_SGX_FEATURE_USE_BRANCH_PWAIT) */
			#if defined(SUPPORT_SGX_FEATURE_USE_BRANCH_EXTSYNCEND)
			if (
					SupportsExtSyncEnd(psTarget) && 
					(uInst1 & EURASIA_USE1_FLOWCTRL_SYNCEXT) &&
					(uInst1 & EURASIA_USE1_FLOWCTRL_SYNCEND)
			   )
			{
				psInst->uFlags3 |= USEASM_OPFLAGS3_SYNCENDNOTTAKEN;
			}
			else
			#endif /* defined(SUPPORT_SGX_FEATURE_USE_BRANCH_EXTSYNCEND) */
			if (uInst1 & EURASIA_USE1_FLOWCTRL_SYNCEND)
			{
				psInst->uFlags1 |= USEASM_OPFLAGS1_SYNCEND;
			}

			return USEDISASM_OK;
		}
		case EURASIA_USE1_FLOWCTRL_OP2_SAVL:
		{
			IMG_UINT32	uEPred;

			uEPred = (uInst1 & ~EURASIA_USE1_EPRED_CLRMSK) >> EURASIA_USE1_EPRED_SHIFT;
			psInst->uFlags1 |= (g_aeDecodeEPred[uEPred] << USEASM_OPFLAGS1_PRED_SHIFT);

			psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

			psInst->uOpcode = USEASM_OP_MOV;

			if (IsEnhancedNoSched(psTarget) && (uInst1 & EURASIA_USE1_FLOWCTRL_NOSCHED))
			{
				psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
			}
			
			DecodeDest(psTarget, uInst0, uInst1, TRUE, &psInst->asArg[0], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
			
			psInst->asArg[1].uType = USEASM_REGTYPE_LINK;
			psInst->asArg[1].uNumber = 0;
			
			return USEDISASM_OK;
		}
		case EURASIA_USE1_FLOWCTRL_OP2_SETL:
		{
			IMG_UINT32	uEPred;

			uEPred = (uInst1 & ~EURASIA_USE1_EPRED_CLRMSK) >> EURASIA_USE1_EPRED_SHIFT;
			psInst->uFlags1 |= (g_aeDecodeEPred[uEPred] << USEASM_OPFLAGS1_PRED_SHIFT);

			psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

			psInst->uOpcode = USEASM_OP_MOV;

			if (IsEnhancedNoSched(psTarget) && (uInst1 & EURASIA_USE1_FLOWCTRL_NOSCHED))
			{
				psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
			}

			psInst->asArg[0].uType = USEASM_REGTYPE_LINK;
			psInst->asArg[0].uNumber = 0;

			DecodeSrc12(psTarget, uInst0, uInst1, 1, TRUE, EURASIA_USE1_S1BEXT, &psInst->asArg[1], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);

			return USEDISASM_OK;
		}
		default:
		{
			return USEDISASM_ERROR_INVALID_FLOWCONTROL_OP2;
		}
	}
}

#if defined(SUPPORT_SGX_FEATURE_USE_UNLIMITED_PHASES)
/*****************************************************************************
 FUNCTION	: DecodePHAS

 PURPOSE	: Decode a PHAS instruction.

 PARAMETERS	: psTarget			- Target processor.
			  uInst1, uInst0	- Instruction to disassemble.
			  psInst			- Written with the decoded instruction.

 RETURNS	: Error code.
*****************************************************************************/
static USEDISASM_ERROR DecodePHAS(PCSGX_CORE_DESC psTarget,
								  IMG_UINT32     uInst0,
								  IMG_UINT32     uInst1,
								  PUSE_INST	  psInst)
{
	static const USEASM_INTSRCSEL aeRate[] = 
	{
		USEASM_INTSRCSEL_PIXEL,		/* SGX545_USE_OTHER2_PHAS_RATE_PIXEL */
		USEASM_INTSRCSEL_SELECTIVE,	/* SGX545_USE_OTHER2_PHAS_RATE_SELECTIVE */
		USEASM_INTSRCSEL_SAMPLE,	/* SGX545_USE_OTHER2_PHAS_RATE_SAMPLE */
		USEASM_INTSRCSEL_UNDEF		/* SGX545_USE_OTHER2_PHAS_RATE_RESERVED */
	};
	static const USEASM_INTSRCSEL aeWaitCond[] = 
	{
		USEASM_INTSRCSEL_NONE,	/* SGX545_USE_OTHER2_PHAS_WAITCOND_NONE */
		USEASM_INTSRCSEL_PT,	/* SGX545_USE_OTHER2_PHAS_WAITCOND_PT */
		USEASM_INTSRCSEL_VCULL,	/* SGX545_USE_OTHER2_PHAS_WAITCOND_VCULL */
		USEASM_INTSRCSEL_UNDEF,	/* SGX545_USE_OTHER2_PHAS_WAITCOND_RESERVED0 */
		USEASM_INTSRCSEL_UNDEF,	/* SGX545_USE_OTHER2_PHAS_WAITCOND_RESERVED1 */
		USEASM_INTSRCSEL_UNDEF,	/* SGX545_USE_OTHER2_PHAS_WAITCOND_RESERVED2 */
		USEASM_INTSRCSEL_UNDEF,	/* SGX545_USE_OTHER2_PHAS_WAITCOND_RESERVED3 */
		USEASM_INTSRCSEL_END,	/* SGX545_USE_OTHER2_PHAS_WAITCOND_END */
	};
	static const USEASM_INTSRCSEL aeMode[] = 
	{
		USEASM_INTSRCSEL_PARALLEL,					/* SGX545_USE_OTHER2_PHAS_MODE_PARALLEL */ 
		USEASM_INTSRCSEL_PERINSTANCE				/* SGX545_USE_OTHER2_PHAS_MODE_PERINSTANCE */
	};

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*
		No repeats for PHAS.
	*/
	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

	/*
		Decode instruction flags.
	*/
	if (uInst1 & EURASIA_USE1_OTHER2_PHAS_END)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}

	#if defined(SUPPORT_SGX_FEATURE_USE_SPRVV)
	if (SupportsSPRVV(psTarget) && (uInst1 & EURASIA_USE_OTHER2_PHAS_TYPEEXT_SPRVV))
	{
		psInst->uOpcode = USEASM_OP_SPRVV;
		return USEDISASM_OK;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_SPRVV) */
	
	if (uInst1 & EURASIA_USE1_OTHER2_PHAS_IMM)
	{
		IMG_UINT32 uPCBits = NumberOfProgramCounterBits(psTarget);
		IMG_UINT32 uPCMax = (1 << uPCBits) - 1;
		IMG_UINT32 uExeAddr;
		IMG_UINT32 uTempCount;
		IMG_UINT32 uWaitCond;
		IMG_UINT32 uRate;
		IMG_UINT32 uMode;

		psInst->uOpcode = USEASM_OP_PHASIMM;

		/*
			Decode next phase execution address.
		*/
		uExeAddr = (uInst0 >> EURASIA_USE0_OTHER2_PHAS_IMM_EXEADDR_SHIFT) & uPCMax;
		SetupImmediateSource(&psInst->asArg[0], uExeAddr);

		/*	
			Decode next phase temporary register count.
		*/
		uTempCount = ((uInst1 & ~EURASIA_USE1_OTHER2_PHAS_IMM_TCOUNT_CLRMSK) >> EURASIA_USE1_OTHER2_PHAS_IMM_TCOUNT_SHIFT);
		uTempCount <<= EURASIA_USE_OTHER2_PHAS_TCOUNT_ALIGNSHIFT;
		SetupImmediateSource(&psInst->asArg[1], uTempCount);

		/*
			Decode next phase execution rate.
		*/
		uRate = (uInst1 & ~EURASIA_USE1_OTHER2_PHAS_IMM_RATE_CLRMSK) >> EURASIA_USE1_OTHER2_PHAS_IMM_RATE_SHIFT;
		if (aeRate[uRate] == USEASM_INTSRCSEL_UNDEF)
		{
			return USEDISASM_ERROR_INVALID_PHAS_RATE;
		}
		SetupIntSrcSel(&psInst->asArg[2], aeRate[uRate]);

		/*
			Decode next phase wait condition.
		*/
		uWaitCond = (uInst1 & ~EURASIA_USE1_OTHER2_PHAS_IMM_WAITCOND_CLRMSK) >> EURASIA_USE1_OTHER2_PHAS_IMM_WAITCOND_SHIFT;
		if (aeWaitCond[uWaitCond] == USEASM_INTSRCSEL_UNDEF)
		{
			return USEDISASM_ERROR_INVALID_PHAS_WAIT_COND;
		}
		SetupIntSrcSel(&psInst->asArg[3], aeWaitCond[uWaitCond]);

		/*
			Decode next phase execution mode.
		*/
		uMode = (uInst1 & ~EURASIA_USE1_OTHER2_PHAS_IMM_MODE_CLRMSK) >> EURASIA_USE1_OTHER2_PHAS_IMM_MODE_SHIFT;
		SetupIntSrcSel(&psInst->asArg[4], aeMode[uMode]);
	}
	else
	{
		psInst->uOpcode = USEASM_OP_PHAS;

		/*
			Decode extra flags for the non-immediate PHAS.
		*/
		if (uInst1 & EURASIA_USE1_OTHER2_PHAS_NONIMM_NOSCHED)
		{
			psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
		}

		/*
			Decode instruction sources.
		*/
		DecodeSrc12(psTarget, 
					uInst0, 
					uInst1, 
					1, 
					IMG_TRUE, 
					EURASIA_USE1_S1BEXT, 
					&psInst->asArg[0], 
					USEDIS_FORMAT_CONTROL_STATE_OFF, 
					0);
		DecodeSrc12(psTarget, 
					uInst0, 
					uInst1, 
					2, 
					IMG_TRUE, 
					EURASIA_USE1_S2BEXT, 
					&psInst->asArg[1], 
					USEDIS_FORMAT_CONTROL_STATE_OFF, 
					0);
	}

	return USEDISASM_OK;
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_UNLIMITED_PHASES) */

#if defined(SUPPORT_SGX_FEATURE_USE_CFI)
/*****************************************************************************
 FUNCTION	: DecodeCFI

 PURPOSE	: Decodes a CFI instruction.

 PARAMETERS	: psTarget			- Target processor.
			  uInst1, uInst0	- Instruction to decode.
			  psInst			- Written with the decoded instruction.

 RETURNS	: Error code.
*****************************************************************************/
static USEDISASM_ERROR DecodeCFI(PCSGX_CORE_DESC psTarget,
							     IMG_UINT32     uInst0,
							     IMG_UINT32     uInst1,
							     PUSE_INST      psInst)
{
	IMG_UINT32	uLevel;

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*
		No repeats on CFI.
	*/
	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

	/*
		Set instruction opcode.
	*/
	if ((uInst1 & SGX545_USE1_OTHER2_CFI_FLUSH) && (uInst1 & SGX545_USE1_OTHER2_CFI_INVALIDATE))
	{
		psInst->uOpcode = USEASM_OP_CFI;
	}
	else if (uInst1 & SGX545_USE1_OTHER2_CFI_FLUSH)
	{
		psInst->uOpcode = USEASM_OP_CF;
	}
	else if (uInst1 & SGX545_USE1_OTHER2_CFI_INVALIDATE)
	{
		psInst->uOpcode = USEASM_OP_CI;
	}
	else
	{
		return USEDISASM_ERROR_INVALID_CFI_MODE;
	}

	/*
		Decode instruction flags.
	*/
	if (uInst1 & EURASIA_USE1_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	if (uInst1 & SGX545_USE1_OTHER2_CFI_GLOBAL)
	{
		psInst->uFlags3 |= USEASM_OPFLAGS3_GLOBAL;
	}
	if (uInst1 & SGX545_USE1_OTHER2_CFI_DM_NOMATCH)
	{
		psInst->uFlags3 |= USEASM_OPFLAGS3_DM_NOMATCH;
	}
	
	/*
		Decode instruction sources.
	*/
	DecodeSrc12(psTarget, uInst0, uInst1, 1, IMG_TRUE, EURASIA_USE1_S1BEXT, &psInst->asArg[0], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	DecodeSrc12(psTarget, uInst0, uInst1, 2, IMG_TRUE, EURASIA_USE1_S2BEXT, &psInst->asArg[1], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);

	/*
		Decode cache levels to flush.
	*/
	uLevel = (uInst1 & ~SGX545_USE1_OTHER2_CFI_LEVEL_CLRMSK) >> SGX545_USE1_OTHER2_CFI_LEVEL_SHIFT;
	SetupImmediateSource(&psInst->asArg[2], uLevel);

	return USEDISASM_OK;
}

/*****************************************************************************
 FUNCTION	: DisassembleCFI

 PURPOSE	: Disassembles a CFI instruction.

 PARAMETERS	: psTarget			- Target processor.
			  uInst1, uInst0	- Instruction to disassemble.
			  pszInst			- String to append the disassembly to.

 RETURNS	: TRUE if the END flag is set on the instruction.
*****************************************************************************/
static IMG_VOID DisassembleCFI(PUSE_INST	psInst,
							   IMG_PCHAR	pszInst)
{
	static const IMG_PCHAR apszLevel[] = {"0 /* Reserved */",		/* SGX545_USE1_OTHER2_CFI_LEVEL_RESERVED */
										  "1 /* L0 and L1 */",		/* SGX545_USE1_OTHER2_CFI_LEVEL_L0L1 */
										  "2 /* L2 */",				/* SGX545_USE1_OTHER2_CFI_LEVEL_L2 */
										  "3 /* L0, L1 and L2 */"	/* SGX545_USE1_OTHER2_CFI_LEVEL_L0L1L2 */};
	
	/*
		Print instruction opcode.
	*/
	usedisasm_asprintf(pszInst, "%s", OpcodeName(psInst->uOpcode));

	/*
		Print instruction modifier flags.
	*/
	PrintInstructionFlags(psInst, pszInst);
	usedisasm_asprintf(pszInst, " ");

	/*
		Print first two sources.
	*/
	PrintSourceRegister(psInst, &psInst->asArg[0], pszInst);
	usedisasm_asprintf(pszInst, ", ");
	PrintSourceRegister(psInst, &psInst->asArg[1], pszInst);

	/*
		Print cache levels source.
	*/
	assert(psInst->asArg[2].uType == USEASM_REGTYPE_IMMEDIATE);
	assert(psInst->asArg[2].uNumber < (sizeof(apszLevel) / sizeof(apszLevel[0])));
	usedisasm_asprintf(pszInst, ", #%s", apszLevel[psInst->asArg[2].uNumber]);
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_CFI) */

#if defined(SUPPORT_SGX_FEATURE_USE_INTEGER_CONDITIONALS)
/*****************************************************************************
 FUNCTION	: DecodeIntegerConditional

 PURPOSE	: Decodes a CNDST, CNDEF, CNDSM, CNDLT or CNDEND instruction.

 PARAMETERS	: psTarget			- Target processor.
			  uInst1, uInst0	- Instruction to decode.
			  psInst			- Destination for the decoded instruction.

 RETURNS	: Error code.
*****************************************************************************/
static USEDISASM_ERROR DecodeIntegerConditional(PCSGX_CORE_DESC psTarget,
												IMG_UINT32     uInst0,
												IMG_UINT32     uInst1,
												PUSE_INST      psInst)
{
	IMG_UINT32		uOp2 = (uInst1 & ~SGXVEC_USE1_CND_OP2_CLRMSK) >> SGXVEC_USE1_CND_OP2_SHIFT;
	PUSE_REGISTER	psNextArg = &psInst->asArg[0];

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*
		Decode opcode.
	*/
	switch (uOp2)
	{
		case SGXVEC_USE1_CND_OP2_CNDST: psInst->uOpcode = USEASM_OP_CNDST; break;
		case SGXVEC_USE1_CND_OP2_CNDEF: psInst->uOpcode = USEASM_OP_CNDEF; break;
		case SGXVEC_USE1_CND_OP2_CNDSM: psInst->uOpcode = USEASM_OP_CNDSM; break;
		case SGXVEC_USE1_CND_OP2_CNDLT: psInst->uOpcode = USEASM_OP_CNDLT; break;
		case SGXVEC_USE1_CND_OP2_CNDEND: psInst->uOpcode = USEASM_OP_CNDEND; break;
		default: return USEDISASM_ERROR_INVALID_CND_OP2;
	}

	/*
		No repeats on integer conditionals.
	*/
	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

	/*
		Decode instruction flags.
	*/
	if (uInst1 & SGXVEC_USE1_CND_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	if (uInst1 & EURASIA_USE1_END)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}

	/*
		Decode destination.
	*/
	DecodeDest(psTarget,
			   uInst0,
			   uInst1,
			   IMG_FALSE /* bAllowExtended */,
			   psNextArg,
			   USEDIS_FORMAT_CONTROL_STATE_OFF /* eFmtControlState */,
			   0 /* uAltFmtFlag */);
	psNextArg++;

	if (uOp2 == SGXVEC_USE1_CND_OP2_CNDLT)
	{
		/*
			Decode PLOOPDEST.
		*/
		psNextArg->uType = USEASM_REGTYPE_PREDICATE;
		psNextArg->uNumber = (uInst0 & ~SGXVEC_USE0_CNDLT_PDSTLOOP_CLRMSK) >> SGXVEC_USE0_CNDLT_PDSTLOOP_SHIFT;
		psNextArg++;
	}

	/*
		Decode source 1.
	*/
	DecodeSrc12(psTarget,
				uInst0,
				uInst1,
				1 /* uSrc */,
				IMG_FALSE /* bAllowExtended */,
				0 /* uExt */,
				psNextArg,
				USEDIS_FORMAT_CONTROL_STATE_OFF /* bFmtControl */,
				0 /* uAltFmtFlag */);
	psNextArg++;

	if (uOp2 != SGXVEC_USE1_CND_OP2_CNDEND)
	{
		IMG_UINT32		uPCnd = (uInst1 & ~SGXVEC_USE1_CND_PCND_CLRMSK) >> SGXVEC_USE1_CND_PCND_SHIFT;

		/*
			Decode PCND.
		*/
		switch (uPCnd)
		{
			case SGXVEC_USE1_CND_PCND_TRUE:		SetupIntSrcSel(psNextArg, USEASM_INTSRCSEL_TRUE); break;
			case SGXVEC_USE1_CND_PCND_FALSE:	SetupIntSrcSel(psNextArg, USEASM_INTSRCSEL_FALSE); break;
			case SGXVEC_USE1_CND_PCND_P0:		SetupPredicate(psNextArg, 0 /* uPred */, IMG_FALSE /* bNegate */); break;
			case SGXVEC_USE1_CND_PCND_P1:		SetupPredicate(psNextArg, 1 /* uPred */, IMG_FALSE /* bNegate */); break;
			case SGXVEC_USE1_CND_PCND_P2:		SetupPredicate(psNextArg, 2 /* uPred */, IMG_FALSE /* bNegate */); break;
			case SGXVEC_USE1_CND_PCND_P3:		SetupPredicate(psNextArg, 3 /* uPred */, IMG_FALSE /* bNegate */); break;
			case SGXVEC_USE1_CND_PCND_NOTP0:	SetupPredicate(psNextArg, 0 /* uPred */, IMG_TRUE /* bNegate */); break;
			case SGXVEC_USE1_CND_PCND_NOTP1:	SetupPredicate(psNextArg, 1 /* uPred */, IMG_TRUE /* bNegate */); break;
			case SGXVEC_USE1_CND_PCND_NOTP2:	SetupPredicate(psNextArg, 2 /* uPred */, IMG_TRUE /* bNegate */); break;
			case SGXVEC_USE1_CND_PCND_NOTP3:	SetupPredicate(psNextArg, 3 /* uPred */, IMG_TRUE /* bNegate */); break;
			default:
			{
				return USEDISASM_ERROR_INVALID_CND_INVALID_PCND;
			}
		}
		psNextArg++;
	}

	if (uOp2 == SGXVEC_USE1_CND_OP2_CNDSM)
	{
		/*
			Decode source 2.
		*/
		DecodeSrc12(psTarget,
					uInst0,
					uInst1,
					2 /* uSrc */,
					IMG_FALSE /* bAllowExtended */,
					0 /* uExt */,
					psNextArg,
					USEDIS_FORMAT_CONTROL_STATE_OFF /* bFmtControl */,
					0 /* uAltFmtFlag */);
		psNextArg++;
	}
	else
	{
		IMG_UINT32	uAdjustment;

		/*
			Decode adjustment.
		*/
		uAdjustment = (uInst0 & ~SGXVEC_USE0_CND_ADJUST_CLRMSK) >> SGXVEC_USE0_CND_ADJUST_SHIFT;
		SetupImmediateSource(psNextArg, uAdjustment);
		psNextArg++;
	}

	return USEDISASM_OK;
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_INTEGER_CONDITIONALS) */

#if defined(SUPPORT_SGX_FEATURE_USE_IDXSC)
/*****************************************************************************
 FUNCTION	: DecodeIDXSC

 PURPOSE	: Decodes an IDXSCR or IDXSCW instruction.

 PARAMETERS	: psTarget			- Target processor.
			  uInst1, uInst0	- Instruction to decode.
			  psInst			- Destination for the decoded instruction.

 RETURNS	: Error code.
*****************************************************************************/
static USEDISASM_ERROR DecodeIDXSC(PCSGX_CORE_DESC psTarget,
								   IMG_UINT32     uInst0,
								   IMG_UINT32     uInst1,
								   PUSE_INST      psInst)
{
	IMG_UINT32	uOp2;
	IMG_UINT32	uIndexReg;
	IMG_UINT32	uFormat;
	IMG_UINT32	uCompCount;
	static const IMG_UINT32 auFormat[] = {8 /* SGXVEC_USE1_OTHER2_IDXSC_FORMAT_8888 */, 
										  10 /* SGXVEC_USE1_OTHER2_IDXSC_FORMAT_10101010 */, 
										  16 /* SGXVEC_USE1_OTHER2_IDXSC_FORMAT_16161616 */, 
										  32 /* SGXVEC_USE1_OTHER2_IDXSC_FORMAT_3232 */};
	static const IMG_UINT32 auCompCount[] = {1 /* SGXVEC_USE1_OTHER2_IDXSC_COMPCOUNT_1 */,
											 2 /* SGXVEC_USE1_OTHER2_IDXSC_COMPCOUNT_2 */};

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*
		Decode opcode.
	*/
	uOp2 = (uInst1 & ~EURASIA_USE1_OTHER2_OP2_CLRMSK) >> EURASIA_USE1_OTHER2_OP2_SHIFT;
	switch (uOp2)
	{
		case SGXVEC_USE1_OTHER2_OP2_IDXSCR: psInst->uOpcode = USEASM_OP_IDXSCR; break;
		case SGXVEC_USE1_OTHER2_OP2_IDXSCW: psInst->uOpcode = USEASM_OP_IDXSCW; break;
	}

	/*
		No repeats on IDSXSC/IDXSCW.
	*/
	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

	/*
		Decode instruction flags.
	*/
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & SGXVEC_USE1_OTHER2_IDXSC_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}

	/*
		Decode index register.
	*/
	uIndexReg = (uInst1 & ~SGXVEC_USE1_OTHER2_IDXSC_INDEXREG_CLRMSK) >> SGXVEC_USE1_OTHER2_IDXSC_INDEXREG_SHIFT;
	psInst->asArg[1].uType = USEASM_REGTYPE_INDEX;
	switch (uIndexReg)
	{
		case SGXVEC_USE1_OTHER2_IDXSC_INDEXREG_LOW: psInst->asArg[1].uNumber = USEREG_INDEX_L; break;
		case SGXVEC_USE1_OTHER2_IDXSC_INDEXREG_HIGH: psInst->asArg[1].uNumber = USEREG_INDEX_H; break;
	}

	/*
		Decode format.
	*/
	uFormat = (uInst1 & ~SGXVEC_USE1_OTHER2_IDXSC_FORMAT_CLRMSK) >> SGXVEC_USE1_OTHER2_IDXSC_FORMAT_SHIFT;
	SetupImmediateSource(&psInst->asArg[3], auFormat[uFormat]);

	/*
		Decode destination.
	*/
	if (uOp2 == SGXVEC_USE1_OTHER2_OP2_IDXSCR)
	{
		DecodeDest(psTarget,
				   uInst0,
				   uInst1,
				   IMG_TRUE /* bAllowExtended */,
				   &psInst->asArg[0],
				   USEDIS_FORMAT_CONTROL_STATE_OFF /* eFmtControlState */,
				   0 /* uAltFmtFlag */);
	}
	else
	{
		IMG_UINT32	uRegNum;
		IMG_BOOL	bDoubleRegisters;
		IMG_UINT32	uRegisterNumberBitCount;

		if (uFormat == SGXVEC_USE1_OTHER2_IDXSC_FORMAT_8888)
		{
			bDoubleRegisters = IMG_FALSE;
			uRegisterNumberBitCount = EURASIA_USE_REGISTER_NUMBER_BITS;
		}
		else
		{
			bDoubleRegisters = IMG_TRUE;
			uRegisterNumberBitCount = EURASIA_USE_REGISTER_NUMBER_BITS + 1;
		}

		uRegNum = (uInst0 & ~SGXVEC_USE0_OTHER2_IDXSC_INDEXOFF_CLRMSK) >> SGXVEC_USE0_OTHER2_IDXSC_INDEXOFF_SHIFT;
		DecodeDestWithNum(psTarget,
						  bDoubleRegisters,
						  uInst1,
						  IMG_TRUE /* bAllowExtended */,
						  &psInst->asArg[0],
						  IMG_FALSE /* bFmtControl */,
						  0 /* uAltFmtFlag */,
						  uRegisterNumberBitCount,
						  uRegNum);
	}

	/*
		Decode source.
	*/
	if (uOp2 == SGXVEC_USE1_OTHER2_OP2_IDXSCR)
	{
		IMG_UINT32	uRegNum;
		IMG_BOOL	bDoubleRegisters;
		IMG_UINT32	uRegisterNumberBitCount;

		if (uFormat == SGXVEC_USE1_OTHER2_IDXSC_FORMAT_8888)
		{
			bDoubleRegisters = IMG_FALSE;
			uRegisterNumberBitCount = EURASIA_USE_REGISTER_NUMBER_BITS;
		}
		else
		{
			bDoubleRegisters = IMG_TRUE;
			uRegisterNumberBitCount = EURASIA_USE_REGISTER_NUMBER_BITS + 1;
		}

		uRegNum = (uInst0 & ~SGXVEC_USE0_OTHER2_IDXSC_INDEXOFF_CLRMSK) >> SGXVEC_USE0_OTHER2_IDXSC_INDEXOFF_SHIFT;
		DecodeSrc12WithNum(psTarget,
						   bDoubleRegisters,
						   uInst0,
						   uInst1,
						   1 /* uSrc */,
						   IMG_TRUE /* bAllowExtended */,
						   EURASIA_USE1_S1BEXT /* uExt */,
						   &psInst->asArg[2],
						   IMG_FALSE /* bFmtControl */,
						   0 /* uAltFmtFlag */,
						   uRegisterNumberBitCount,
						   uRegNum);
	}
	else
	{
		DecodeSrc12(psTarget,
				    uInst0,
				    uInst1,
				    1 /* uSrc */,
				    IMG_TRUE /* bAllowExtended */,
				    EURASIA_USE1_S1BEXT /* uExt */,
				    &psInst->asArg[2],
				    USEDIS_FORMAT_CONTROL_STATE_OFF /* bFmtControl */,
				    0 /* uAltFmtFlag */);
	}

	/*
		Decode component count.
	*/
	uCompCount = (uInst1 & ~SGXVEC_USE1_OTHER2_IDXSC_COMPCOUNT_CLRMSK) >> SGXVEC_USE1_OTHER2_IDXSC_COMPCOUNT_SHIFT;
	SetupImmediateSource(&psInst->asArg[4], auCompCount[uCompCount]);

	return USEDISASM_OK;
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_IDXSC) */

/*****************************************************************************
 FUNCTION	: DecodeOther2

 PURPOSE	: Decodes an instruction in the OTHER2 group.

 PARAMETERS	: psTarget			- Target processor.
			  uInst1, uInst0	- Instruction to disassemble.
			  psInst			- WRitten with the decoded instruction.

 RETURNS	: Error code.
*****************************************************************************/
static USEDISASM_ERROR DecodeOther2(PCSGX_CORE_DESC psTarget,
									IMG_UINT32     uInst0,
									IMG_UINT32     uInst1,
									PUSE_INST	   psInst)
{
	IMG_UINT32 uOp = (uInst1 & ~EURASIA_USE1_OTHER2_OP2_CLRMSK) >> EURASIA_USE1_OTHER2_OP2_SHIFT;

	PVR_UNREFERENCED_PARAMETER(uInst0);
	PVR_UNREFERENCED_PARAMETER(psTarget);
	PVR_UNREFERENCED_PARAMETER(psInst);

	switch (uOp)
	{
		#if defined(SUPPORT_SGX_FEATURE_USE_UNLIMITED_PHASES)
		case EURASIA_USE1_OTHER2_OP2_PHAS:
		{
			if (HasUnlimitedPhases(psTarget))
			{
				return DecodePHAS(psTarget, uInst0, uInst1, psInst);
			}
			else
			{
				return USEDISASM_ERROR_INVALID_OTHER2_OP2;
			}
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_UNLIMITED_PHASES) */
		#if defined(SUPPORT_SGX_FEATURE_USE_CFI)
		case SGX545_USE1_OTHER2_OP2_CFI:
		{
			if (SupportsCFI(psTarget))
			{
				return DecodeCFI(psTarget, uInst0, uInst1, psInst);
			}
			else
			{
				return USEDISASM_ERROR_INVALID_OTHER2_OP2;
			}
			break;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_CFI) */
		#if defined(SUPPORT_SGX_FEATURE_USE_IDXSC)
		case SGXVEC_USE1_OTHER2_OP2_IDXSCR:
		case SGXVEC_USE1_OTHER2_OP2_IDXSCW:
		{
			if (SupportsIDXSC(psTarget))
			{
				return DecodeIDXSC(psTarget, uInst0, uInst1, psInst);
			}
			else
			{
				return USEDISASM_ERROR_INVALID_OTHER2_OP2;
			}
			break;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_IDXSC) */
		default:
		{
			return USEDISASM_ERROR_INVALID_OTHER2_OP2;
		}
	}
}

#if defined(SUPPORT_SGX_FEATURE_USE_FNORMALISE)
/*****************************************************************************
 FUNCTION	: DecodeFNormalise

 PURPOSE	: Decodes an FNRM instruction back to the USEASM input format.

 PARAMETERS	: psTarget			- Target processor.
			  uInst1, uInst0	- Instruction to disassemble.
			  psInst			- Destination for the decoded instruction.

 RETURNS	: Status.
*****************************************************************************/
static USEDISASM_ERROR DecodeFNormalise(PCSGX_CORE_DESC psTarget,
										IMG_UINT32     uInst0,
										IMG_UINT32     uInst1,
										PUSE_INST		psInst)
{
	IMG_UINT32	uSrcMod;
	USEASM_PRED ePredicate;
	IMG_UINT32	uDRCSel;

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*
		Decode predicate.
	*/
	ePredicate = g_aeDecodeEPred[(uInst1 & ~EURASIA_USE1_EPRED_CLRMSK) >> EURASIA_USE1_EPRED_SHIFT];
	psInst->uFlags1 |= (ePredicate << USEASM_OPFLAGS1_PRED_SHIFT);

	/*
		Decode opcode.
	*/
	if (uInst1 & SGX540_USE1_FNRM_F16DP)
	{
		psInst->uOpcode = USEASM_OP_FNRM16;
	}
	else
	{
		psInst->uOpcode = USEASM_OP_FNRM32;
	}

	/*
		No repeats on FNRM.
	*/
	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

	/*
		Decode instruction modifiers.
	*/
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_SYNCSTART)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SYNCSTART;
	}
	if (uInst1 & EURASIA_USE1_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	
	/*
		Decode destination.
	*/
	DecodeDest(psTarget, uInst0, uInst1, IMG_FALSE, &psInst->asArg[0], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);

	if (uInst1 & SGX540_USE1_FNRM_F16DP)
	{
		IMG_UINT32	uChan;
		IMG_UINT32	uSwizzle;

		/*
			Decode register sources.
		*/
		DecodeSrc12(psTarget,
				    uInst0,
				    uInst1,
				    1,
				    IMG_TRUE,
				    EURASIA_USE1_S1BEXT,
				    &psInst->asArg[1],
				    USEDIS_FORMAT_CONTROL_STATE_OFF,
				    0);
		DecodeSrc12(psTarget,
				    uInst0,
				    uInst1,
				    2,
				    IMG_TRUE,
				    EURASIA_USE1_S2BEXT,
				    &psInst->asArg[2],
				    USEDIS_FORMAT_CONTROL_STATE_OFF,
				    0);

		/*
			Decode instruction swizzle.
		*/
		uSwizzle = 0;
		for (uChan = 0; uChan < 4; uChan++)
		{
			IMG_UINT32	uChanSel = USE_UNDEF;
			static const USEASM_SWIZZLE_SEL aeChanSel[] = 
			{
				USEASM_SWIZZLE_SEL_X,		/* SGX540_USE_FNRM_F16CHANSEL_SRC0L */
				USEASM_SWIZZLE_SEL_Y,		/* SGX540_USE_FNRM_F16CHANSEL_SRC0H */
				USEASM_SWIZZLE_SEL_Z,		/* SGX540_USE_FNRM_F16CHANSEL_SRC1L */ 
				USEASM_SWIZZLE_SEL_W,		/* SGX540_USE_FNRM_F16CHANSEL_SRC1H */
				USEASM_SWIZZLE_SEL_0,		/* SGX540_USE_FNRM_F16CHANSEL_ZERO */
				USEASM_SWIZZLE_SEL_1,		/* SGX540_USE_FNRM_F16CHANSEL_ONE */
				USEASM_SWIZZLE_SEL_UNDEF,	/* SGX540_USE_FNRM_F16CHANSEL_RESERVED0 */
				USEASM_SWIZZLE_SEL_UNDEF,	/* SGX540_USE_FNRM_F16CHANSEL_RESERVED1 */
			};
			switch (uChan)
			{
				case 0: 
				{
					uChanSel = (uInst1 & ~SGX540_USE1_FNRM_F16C0SWIZ_CLRMSK) >> SGX540_USE1_FNRM_F16C0SWIZ_SHIFT; 
					break;
				}
				case 1: 
				{
					uChanSel = (uInst1 & ~SGX540_USE1_FNRM_F16C1SWIZ_CLRMSK) >> SGX540_USE1_FNRM_F16C1SWIZ_SHIFT; 
					break;
				}
				case 2: 
				{
					uChanSel = (uInst0 & ~SGX540_USE0_FNRM_F16C2SWIZ_CLRMSK) >> SGX540_USE0_FNRM_F16C2SWIZ_SHIFT; 
					break;
				}
				case 3: 
				{
					uChanSel = (uInst0 & ~SGX540_USE0_FNRM_F16C3SWIZ_CLRMSK) >> SGX540_USE0_FNRM_F16C3SWIZ_SHIFT; 
					break;
				}
				default: IMG_ABORT();
			}
			if (aeChanSel[uChanSel] == USEASM_SWIZZLE_SEL_UNDEF)
			{
				return USEDISASM_ERROR_INVALID_FNRM_SWIZZLE;
			}
			uSwizzle |= (aeChanSel[uChanSel] << (USEASM_SWIZZLE_FIELD_SIZE * uChan));
		}
		SetupSwizzleSource(&psInst->asArg[3], uSwizzle);
	}
	else
	{
		USEDIS_FORMAT_CONTROL_STATE eFmtControl;

		if (uInst1 & EURASIA_USE1_FLOAT_SFASEL)
		{
			eFmtControl = USEDIS_FORMAT_CONTROL_STATE_ON;
		}
		else
		{
			eFmtControl = USEDIS_FORMAT_CONTROL_STATE_OFF;
		}

		DecodeSrc0(psTarget,
				   uInst0,
				   uInst1,
				   IMG_TRUE,
				   EURASIA_USE1_S0BEXT,
				   &psInst->asArg[1],
				   eFmtControl,
				   USEASM_ARGFLAGS_FMTF16);
		DecodeSrc12(psTarget,
				    uInst0,
				    uInst1,
				    1,
				    IMG_TRUE,
				    EURASIA_USE1_S1BEXT,
				    &psInst->asArg[2],
				    eFmtControl,
				    USEASM_ARGFLAGS_FMTF16);
		DecodeSrc12(psTarget,
				    uInst0,
				    uInst1,
				    2,
				    IMG_TRUE,
				    EURASIA_USE1_S2BEXT,
				    &psInst->asArg[3],
				    eFmtControl,
				    USEASM_ARGFLAGS_FMTF16);
	}

	/*
		Decode source modifier.
	*/
	uSrcMod = (uInst1 & ~SGX540_USE1_FNRM_SRCMOD_CLRMSK) >> SGX540_USE1_FNRM_SRCMOD_SHIFT;
	if (uSrcMod & EURASIA_USE_SRCMOD_NEGATE)
	{
		SetupIntSrcSel(&psInst->asArg[4], USEASM_INTSRCSEL_SRCNEG);
	}
	else
	{
		SetupIntSrcSel(&psInst->asArg[4], USEASM_INTSRCSEL_NONE);
	}
	if (uSrcMod & EURASIA_USE_SRCMOD_ABSOLUTE)
	{
		SetupIntSrcSel(&psInst->asArg[5], USEASM_INTSRCSEL_SRCABS);
	}
	else
	{
		SetupIntSrcSel(&psInst->asArg[5], USEASM_INTSRCSEL_NONE);
	}

	/*
		Decode DRC selector.
	*/
	uDRCSel = (uInst1 & ~SGX540_USE1_FNRM_DRCSEL_CLRMSK) >> SGX540_USE1_FNRM_DRCSEL_SHIFT;
	SetupDRCSource(&psInst->asArg[6], uDRCSel);

	return USEDISASM_OK;
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_FNORMALISE) */

#if defined(SUPPORT_SGX_FEATURE_USE_STORE_MOE_TO_REGISTERS)
static USEDISASM_ERROR DecodeMOEST(PCSGX_CORE_DESC psTarget,
								   IMG_UINT32     uInst0,
								   IMG_UINT32     uInst1,
								   PUSE_INST	  psInst)
{
	IMG_UINT32	uParam;

	UseAsmInitInst(psInst);

	psInst->uOpcode = USEASM_OP_MOEST;

	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);
	if (IsEnhancedNoSched(psTarget) && (uInst1 & EURASIA_USE1_MOECTRL_NOSCHED))
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	
	DecodeDest(psTarget, uInst0, uInst1, IMG_TRUE, &psInst->asArg[0], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	
	uParam = (uInst1 & ~SGX545_USE1_MOECTRL_MOEST_PARAM_CLRMSK) >> SGX545_USE1_MOECTRL_MOEST_PARAM_SHIFT;
	SetupImmediateSource(&psInst->asArg[1], uParam);

	return USEDISASM_OK;
}

static IMG_VOID DisassembleMOEST(PUSE_INST		psInst,
								 IMG_PCHAR      pszInst)
{
	static const IMG_PCHAR apszMoeStParam[] = {"#0 /* SMOA low data */",
											   "#1 /* SMOA high data */",
											   "#2 /* SMLSI low data */",
											   "#3 /* SMLSI high data */",
											   "#4 /* SMBO low data */",
											   "#5 /* SMBO high data */",
											   "#6 /* SMR low data */",
											   "#7 /* SMR high data */",
											   "#8 /* Index data */",
											   "#9 /* Reserved */",
											   "#10 /* Reserved */",
											   "#11 /* Reserved */",
											   "#12 /* Reserved */",
											   "#13 /* Reserved */",
											   "#14 /* Reserved */",
											   "#15 /* Reserved */"};
	/*
		Print instruction predicate.
	*/
	PrintPredicate(psInst, pszInst);

	/*
		Print instruction opcode.
	*/
	usedisasm_asprintf(pszInst, "%s", OpcodeName(psInst->uOpcode));

	/*
		Print instruction modifier flags.
	*/
	PrintInstructionFlags(psInst, pszInst);
	usedisasm_asprintf(pszInst, " ");

	/*
		Print destination register.
	*/
	PrintDestRegister(psInst, &psInst->asArg[0], pszInst);

	/*
		Print source.
	*/
	usedisasm_asprintf(pszInst, ", ");
	if (
			psInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE &&
			psInst->asArg[1].uNumber < (sizeof(apszMoeStParam) / sizeof(apszMoeStParam[0]))
	   )
	{
		usedisasm_asprintf(pszInst, "%s", apszMoeStParam[psInst->asArg[1].uNumber]);
	}
	else
	{
		PrintSourceRegister(psInst, &psInst->asArg[1], pszInst);
	}
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_STORE_MOE_TO_REGISTERS) */

static USEDISASM_ERROR DecodeMoeControlInstruction(PCSGX_CORE_DESC psTarget,
												   IMG_UINT32     uInst0,
												   IMG_UINT32     uInst1,
												   PUSE_INST	  psInst)
{
	static const USEASM_OPCODE aeDecodeMoeOp[] = 
	{
		USEASM_OP_SMOA,		/* EURASIA_USE1_MOECTRL_OP2_SMOA */
		USEASM_OP_SMR,		/* EURASIA_USE1_MOECTRL_OP2_SMR */
		USEASM_OP_SMLSI,	/* EURASIA_USE1_MOECTRL_OP2_SMLSI */
		USEASM_OP_SMBO,		/* EURASIA_USE1_MOECTRL_OP2_SMBO */
		USEASM_OP_IMO,		/* EURASIA_USE1_MOECTRL_OP2_IMO */
		USEASM_OP_SETFC,	/* EURASIA_USE1_MOECTRL_OP2_SETFC */
	};
	IMG_UINT32 uOp2 = (uInst1 & ~EURASIA_USE1_MOECTRL_OP2_CLRMSK) >> EURASIA_USE1_MOECTRL_OP2_SHIFT;

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*
		Decode the MOE opcode.
	*/
	if (uOp2 < (sizeof(aeDecodeMoeOp) / sizeof(aeDecodeMoeOp[0])))
	{
		psInst->uOpcode = aeDecodeMoeOp[uOp2];
	}
	else
	{
		return USEDISASM_ERROR_INVALID_MOE_OP2;
	}

	/*
		No repeats on MOE control instructions.
	*/
	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

	/*
		Decode the NOSCHED modifier flag.
	*/
	if (IsEnhancedNoSched(psTarget) && (uInst1 & EURASIA_USE1_MOECTRL_NOSCHED))
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}

	/*
		Check for the special case of loading the MOE state from registers.
	*/
	if (
			IsLoadMOEStateFromRegisters(psTarget) &&
			(uInst1 & EURASIA_USE1_MOECTRL_REGDAT) &&
			uOp2 != EURASIA_USE1_MOECTRL_OP2_IMO &&
			uOp2 != EURASIA_USE1_MOECTRL_OP2_SETFC
		)
	{
		IMG_UINT32	uArg;

		DecodeSrc12(psTarget, uInst0, uInst1, 1, IMG_TRUE, EURASIA_USE1_S1BEXT, &psInst->asArg[0], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
		DecodeSrc12(psTarget, uInst0, uInst1, 2, IMG_TRUE, EURASIA_USE1_S2BEXT, &psInst->asArg[1], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);

		for (uArg = 2; uArg < OpcodeArgumentCount(psInst->uOpcode); uArg++)
		{
			SetupIntSrcSel(&psInst->asArg[uArg], USEASM_INTSRCSEL_NONE);
		}
		return USEDISASM_OK;
	}

	switch (psInst->uOpcode)
	{
		case USEASM_OP_SMOA:
		{
			IMG_UINT32	uDOffset, uS0Offset, uS1Offset, uS2Offset;
			IMG_INT32	iDOffset, iS0Offset, iS1Offset, iS2Offset;
			IMG_UINT32	uDAddressMode, uS0AddressMode, uS1AddressMode, uS2AddressMode;

			uDOffset = 
				(((uInst1 & ~EURASIA_USE1_MOECTRL_SMOA_DOFFSET_CLRMSK) >> EURASIA_USE1_MOECTRL_SMOA_DOFFSET_SHIFT) << 2) |
				(((uInst0 & ~EURASIA_USE0_MOECTRL_SMOA_DOFFSET_CLRMSK) >> EURASIA_USE0_MOECTRL_SMOA_DOFFSET_SHIFT) << 0);
			iDOffset = SignExt(uDOffset, 9);
			SetupImmediateSource(&psInst->asArg[0], iDOffset);

			uS0Offset = (uInst0 & ~EURASIA_USE0_MOECTRL_SMOA_S0OFFSET_CLRMSK) >> EURASIA_USE0_MOECTRL_SMOA_S0OFFSET_SHIFT;
			iS0Offset = SignExt(uS0Offset, 9);
			SetupImmediateSource(&psInst->asArg[1], iS0Offset);

			uS1Offset = (uInst0 & ~EURASIA_USE0_MOECTRL_SMOA_S1OFFSET_CLRMSK) >> EURASIA_USE0_MOECTRL_SMOA_S1OFFSET_SHIFT;
			iS1Offset = SignExt(uS1Offset, 9);
			SetupImmediateSource(&psInst->asArg[2], iS1Offset);

			uS2Offset = (uInst0 & ~EURASIA_USE0_MOECTRL_SMOA_S2OFFSET_CLRMSK) >> EURASIA_USE0_MOECTRL_SMOA_S2OFFSET_SHIFT;
			iS2Offset = SignExt(uS2Offset, 9);
			SetupImmediateSource(&psInst->asArg[3], iS2Offset);
			
			uDAddressMode = (uInst1 & ~EURASIA_USE1_MOECTRL_SMOA_DAM_CLRMSK) >> EURASIA_USE1_MOECTRL_SMOA_DAM_SHIFT;
			SetupAddressModeSource(&psInst->asArg[4], uDAddressMode);

			uS0AddressMode = (uInst1 & ~EURASIA_USE1_MOECTRL_SMOA_S0AM_CLRMSK) >> EURASIA_USE1_MOECTRL_SMOA_S0AM_SHIFT;
			SetupAddressModeSource(&psInst->asArg[5], uS0AddressMode);

			uS1AddressMode = (uInst1 & ~EURASIA_USE1_MOECTRL_SMOA_S1AM_CLRMSK) >> EURASIA_USE1_MOECTRL_SMOA_S1AM_SHIFT;
			SetupAddressModeSource(&psInst->asArg[6], uS1AddressMode);

			uS2AddressMode = (uInst1 & ~EURASIA_USE1_MOECTRL_SMOA_S2AM_CLRMSK) >> EURASIA_USE1_MOECTRL_SMOA_S2AM_SHIFT;
			SetupAddressModeSource(&psInst->asArg[7], uS2AddressMode);
			break;
		}
		case USEASM_OP_SETFC:
		{
			SetupImmediateSource(&psInst->asArg[0], (uInst0 & EURASIA_USE0_MOECTRL_SETFC_EFO_SELFMTCTL) ? 1 : 0);
			SetupImmediateSource(&psInst->asArg[1], (uInst0 & EURASIA_USE0_MOECTRL_SETFC_COL_SETFMTCTL) ? 1 : 0);
			break;
		}
		case USEASM_OP_IMO:
		{
			IMG_UINT32 uDIncrement;
			IMG_UINT32 uS0Increment;
			IMG_UINT32 uS1Increment;
			IMG_UINT32 uS2Increment;

			uDIncrement = (uInst1 & ~EURASIA_USE1_MOECTRL_SMR_DRANGE_CLRMSK) >> EURASIA_USE1_MOECTRL_SMR_DRANGE_SHIFT;
			uS0Increment =
				(((uInst1 & ~EURASIA_USE1_MOECTRL_SMR_S0RANGE_CLRMSK) >> EURASIA_USE1_MOECTRL_SMR_S0RANGE_SHIFT) << 8) |
			     ((uInst0 & ~EURASIA_USE0_MOECTRL_SMR_S0RANGE_CLRMSK) >> EURASIA_USE0_MOECTRL_SMR_S0RANGE_SHIFT);
			uS1Increment = (uInst0 & ~EURASIA_USE0_MOECTRL_SMR_S1RANGE_CLRMSK) >> EURASIA_USE0_MOECTRL_SMR_S1RANGE_SHIFT;
			uS2Increment = (uInst0 & ~EURASIA_USE0_MOECTRL_SMR_S2RANGE_CLRMSK) >> EURASIA_USE0_MOECTRL_SMR_S2RANGE_SHIFT;

			uDIncrement = SignExt(uDIncrement, 6);
			uS0Increment = SignExt(uS0Increment, 6);
			uS1Increment = SignExt(uS1Increment, 6);
			uS2Increment = SignExt(uS2Increment, 6);

			SetupImmediateSource(&psInst->asArg[0], uDIncrement);
			SetupImmediateSource(&psInst->asArg[1], uS0Increment);
			SetupImmediateSource(&psInst->asArg[2], uS1Increment);
			SetupImmediateSource(&psInst->asArg[3], uS2Increment);
			break;
		}
		case USEASM_OP_SMLSI:
		{
			static const IMG_UINT32 puIncrMasks[] = {EURASIA_USE0_MOECTRL_SMLSI_DINC_CLRMSK,
													 EURASIA_USE0_MOECTRL_SMLSI_S0INC_CLRMSK,
													 EURASIA_USE0_MOECTRL_SMLSI_S1INC_CLRMSK,
													 EURASIA_USE0_MOECTRL_SMLSI_S2INC_CLRMSK};
			static const IMG_UINT32 puIncrShifts[] = {EURASIA_USE0_MOECTRL_SMLSI_DINC_SHIFT,
													  EURASIA_USE0_MOECTRL_SMLSI_S0INC_SHIFT,
													  EURASIA_USE0_MOECTRL_SMLSI_S1INC_SHIFT,
													  EURASIA_USE0_MOECTRL_SMLSI_S2INC_SHIFT};
			static const IMG_UINT32 puModeMasks[] = {EURASIA_USE1_MOECTRL_SMLSI_DUSESWIZ,
													 EURASIA_USE1_MOECTRL_SMLSI_S0USESWIZ,
													 EURASIA_USE1_MOECTRL_SMLSI_S1USESWIZ,
													 EURASIA_USE1_MOECTRL_SMLSI_S2USESWIZ};
			IMG_UINT32 uArg, uChan;
			IMG_UINT32 uTempLimit, uPALimit, uSALimit;

			for (uArg = 0; uArg < 4; uArg++)
			{
				IMG_UINT32	uIncr = (uInst0 & ~puIncrMasks[uArg]) >> puIncrShifts[uArg];
	
				if (uInst1 & puModeMasks[uArg])
				{
					IMG_UINT32	uSwizzle;

					uSwizzle = 0;
					for (uChan = 0; uChan < 4; uChan++)
					{
						IMG_UINT32	uChanSel;
						uChanSel = (uIncr >> (uChan * EURASIA_USE_MOESWIZZLE_FIELD_SIZE)) & EURASIA_USE_MOESWIZZLE_VALUE_MASK;
						uSwizzle |= (uChanSel << (USEASM_SWIZZLE_FIELD_SIZE * uChan));
					}
					psInst->asArg[uArg].uType = USEASM_REGTYPE_SWIZZLE;
					psInst->asArg[uArg].uNumber = uSwizzle;
				}
				else
				{
					SetupImmediateSource(&psInst->asArg[uArg], SignExt(uIncr, 7));
				}
			}
			for (uArg = 0; uArg < 4; uArg++)
			{
				SetupImmediateSource(&psInst->asArg[4 + uArg], (uInst1 & puModeMasks[uArg]) ? 1 : 0);
			}

			uTempLimit = ((uInst1 & ~EURASIA_USE1_MOECTRL_SMLSI_TLIMIT_CLRMSK) >> EURASIA_USE1_MOECTRL_SMLSI_TLIMIT_SHIFT);
			uTempLimit *= EURASIA_USE1_MOECTRL_SMLSI_LIMIT_GRAN;
			SetupImmediateSource(&psInst->asArg[8], uTempLimit);

			uPALimit = ((uInst1 & ~EURASIA_USE1_MOECTRL_SMLSI_PLIMIT_CLRMSK) >> EURASIA_USE1_MOECTRL_SMLSI_PLIMIT_SHIFT);
			uPALimit *= EURASIA_USE1_MOECTRL_SMLSI_LIMIT_GRAN;
			SetupImmediateSource(&psInst->asArg[9], uPALimit);
			
			uSALimit = ((uInst1 & ~EURASIA_USE1_MOECTRL_SMLSI_SLIMIT_CLRMSK) >> EURASIA_USE1_MOECTRL_SMLSI_SLIMIT_SHIFT);
			uSALimit *= EURASIA_USE1_MOECTRL_SMLSI_LIMIT_GRAN;
			SetupImmediateSource(&psInst->asArg[10], uSALimit);
			break;
		}
		case USEASM_OP_SMBO:
		case USEASM_OP_SMR:
		{
			IMG_UINT32	uDRange, uS0Range, uS1Range, uS2Range;

			uDRange = (uInst1 & ~EURASIA_USE1_MOECTRL_SMR_DRANGE_CLRMSK) >> EURASIA_USE1_MOECTRL_SMR_DRANGE_SHIFT;
			SetupImmediateSource(&psInst->asArg[0], uDRange);

			uS0Range = 
				(((uInst1 & ~EURASIA_USE1_MOECTRL_SMR_S0RANGE_CLRMSK) >> EURASIA_USE1_MOECTRL_SMR_S0RANGE_SHIFT) << 8) |
				 ((uInst0 & ~EURASIA_USE0_MOECTRL_SMR_S0RANGE_CLRMSK) >> EURASIA_USE0_MOECTRL_SMR_S0RANGE_SHIFT);
			SetupImmediateSource(&psInst->asArg[1], uS0Range);

			uS1Range = (uInst0 & ~EURASIA_USE0_MOECTRL_SMR_S1RANGE_CLRMSK) >> EURASIA_USE0_MOECTRL_SMR_S1RANGE_SHIFT;
			SetupImmediateSource(&psInst->asArg[2], uS1Range);
			
			uS2Range = (uInst0 & ~EURASIA_USE0_MOECTRL_SMR_S2RANGE_CLRMSK) >> EURASIA_USE0_MOECTRL_SMR_S2RANGE_SHIFT;
			SetupImmediateSource(&psInst->asArg[3], uS2Range);
			break;
		}
		default: IMG_ABORT();
	}

	return USEDISASM_OK;
}

#if defined(SUPPORT_SGX_FEATURE_USE_DUAL_ISSUE)
static USEDISASM_ERROR DecodeDualIssue(PCSGX_CORE_DESC psTarget,
									   IMG_UINT32     uInst0,
									   IMG_UINT32     uInst1,
									   PUSE_INST	  psInst,
									   PUSE_INST	  psCoInst)
{
	static const USEASM_OPCODE aaeOp1[][4] = 
	{
		{
			USEASM_OP_FMAD,		/* SGX545_USE1_FDUAL_OP1_STD_FMAD */ 
			USEASM_OP_MOV,		/* SGX545_USE1_FDUAL_OP1_STD_MOV */
			USEASM_OP_FADM,		/* SGX545_USE1_FDUAL_OP1_STD_FADM */
			USEASM_OP_FMSA,		/* SGX545_USE1_FDUAL_OP1_STD_FMSA */
		}, 
		{
			USEASM_OP_FDDP,		/* SGX545_USE1_FDUAL_OP1_EXT_FDDP */
			USEASM_OP_FMINMAX,	/* SGX545_USE1_FDUAL_OP1_EXT_FCLMP */
			USEASM_OP_IMA32,	/* SGX545_USE1_FDUAL_OP1_EXT_IMA32 */
			USEASM_OP_FSSQ,		/* SGX545_USE1_FDUAL_OP1_EXT_FSSQ */
		}
	};
	static const USEASM_OPCODE aeOp2[] = 
	{
		USEASM_OP_FMAD,			/* SGX545_USE1_FDUAL_OP2_FMAD */
		USEASM_OP_FMAD16,		/* SGX545_USE1_FDUAL_OP2_FMAD16 */
		USEASM_OP_FMSA,			/* SGX545_USE1_FDUAL_OP2_FMSA */
		USEASM_OP_FSUBFLR,		/* SGX545_USE1_FDUAL_OP2_FFRC */
		USEASM_OP_FMAXMIN,		/* SGX545_USE1_FDUAL_OP2_FCLMP */ 
		USEASM_OP_FRCP,			/* SGX545_USE1_FDUAL_OP2_FRCP */
		USEASM_OP_FRSQ,			/* SGX545_USE1_FDUAL_OP2_FRSQ */
		USEASM_OP_FEXP,			/* SGX545_USE1_FDUAL_OP2_FEXP */
		USEASM_OP_FLOG,			/* SGX545_USE1_FDUAL_OP2_FLOG */
		USEASM_OP_FSIN,			/* SGX545_USE1_FDUAL_OP2_FSIN */
		USEASM_OP_FCOS,			/* SGX545_USE1_FDUAL_OP2_FCOS */
		USEASM_OP_FADD,			/* SGX545_USE1_FDUAL_OP2_FADD */
		USEASM_OP_FSUB,			/* SGX545_USE1_FDUAL_OP2_FSUB */
		USEASM_OP_FMUL,			/* SGX545_USE1_FDUAL_OP2_FMUL */
		USEASM_OP_IMA32,		/* SGX545_USE1_FDUAL_OP2_IMA32 */
		USEASM_OP_MOV,			/* SGX545_USE1_FDUAL_OP2_MOV */
	};
	IMG_UINT32 uOp1 = (uInst1 & ~SGX545_USE1_FDUAL_OP1_CLRMSK) >> SGX545_USE1_FDUAL_OP1_SHIFT;
	IMG_UINT32 uOp2 = (uInst1 & ~SGX545_USE1_FDUAL_OP2_CLRMSK) >> SGX545_USE1_FDUAL_OP2_SHIFT;
	USEDIS_FORMAT_CONTROL_STATE eFmtControl;
	IMG_UINT32 uOp2Dst = (uInst1 & ~SGX545_USE1_FDUAL_OP2DSTS0_CLRMSK) >> SGX545_USE1_FDUAL_OP2DSTS0_SHIFT;
	IMG_UINT32 uRptCount = (uInst1 & ~SGX545_USE1_FDUAL_RCNT_CLRMSK) >> SGX545_USE1_FDUAL_RCNT_SHIFT;
	IMG_BOOL bMOV = IMG_FALSE, bFSSQ = IMG_FALSE;
	IMG_UINT32 uSPred = (uInst1 & ~SGX545_USE1_FDUAL_SPRED_CLRMSK) >> SGX545_USE1_FDUAL_SPRED_SHIFT;
	PUSE_REGISTER psNextArg = psInst->asArg;
	PUSE_REGISTER psCoNextArg = psCoInst->asArg;

	UseAsmInitInst(psInst);
	UseAsmInitInst(psCoInst);

	if (uInst1 & EURASIA_USE1_FLOAT_SFASEL)
	{
		eFmtControl = USEDIS_FORMAT_CONTROL_STATE_ON;
	}
	else
	{
		eFmtControl = USEDIS_FORMAT_CONTROL_STATE_OFF;
	}

	/*
		Decode predicate.
	*/
	psInst->uFlags1 |= (g_aeDecodeSPred[uSPred] << USEASM_OPFLAGS1_PRED_SHIFT);

	/*
		Decode main instruction opcode.
	*/
	if (uInst1 & SGX545_USE1_FDUAL_OP1EXT)
	{
		psInst->uOpcode = aaeOp1[1][uOp1];
	}
	else
	{
		psInst->uOpcode = aaeOp1[0][uOp1];
	}

	/*
		Decode instruction modifiers.
	*/
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_SYNCSTART)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SYNCSTART;
	}
	if (uInst1 & SGX545_USE1_FDUAL_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	psInst->uFlags1 |= ((uRptCount + 1) << USEASM_OPFLAGS1_REPEAT_SHIFT);
	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

	/*
		Decode main instruction.
	*/
	DecodeDest(psTarget, uInst0, uInst1, IMG_FALSE, psNextArg, USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	psNextArg++;
	
	/*
		Add the extra, fixed destination for dual-issued FDDP.
	*/
	if ((uInst1 & SGX545_USE1_FDUAL_OP1EXT) && uOp1 == SGX545_USE1_FDUAL_OP1_EXT_FDDP)
	{
		psNextArg->uType = USEASM_REGTYPE_FPINTERNAL;
		psNextArg->uNumber = SGX545_USE_FDUAL_FDDP_IREG_DEST_NUM;
		psNextArg++;
	}

	/*
		Add internal register destination for IMA32 - never used for dual-issued IMA32.
	*/
	if (psInst->uOpcode == USEASM_OP_IMA32)
	{
		SetupIntSrcSel(psNextArg, USEASM_INTSRCSEL_NONE);
		psNextArg++;
	}

	/*
		Decode source 0 if present.
	*/
	if ((uInst1 & SGX545_USE1_FDUAL_OP1EXT) && (uOp1 == SGX545_USE1_FDUAL_OP1_EXT_FSSQ))
	{
		bFSSQ = IMG_TRUE;
	}
	if (!(uInst1 & SGX545_USE1_FDUAL_OP1EXT) && (uOp1 == SGX545_USE1_FDUAL_OP1_STD_MOV))
	{
		bMOV = IMG_TRUE;
	}
	if (!(bFSSQ || bMOV))
	{
		DecodeSrc0(psTarget, uInst0, uInst1, IMG_FALSE, 0, psNextArg, eFmtControl, USEASM_ARGFLAGS_FMTF16);
		psNextArg++;
	}

	/*
		Decode source 1.
	*/
	DecodeSrc12(psTarget, uInst0, uInst1, 1, IMG_TRUE, EURASIA_USE1_S1BEXT, psNextArg, eFmtControl, USEASM_ARGFLAGS_FMTF16);
	if (uInst1 & SGX545_USE1_FDUAL_S1NEG)
	{
		psNextArg->uFlags |= USEASM_ARGFLAGS_NEGATE;
	}
	psNextArg++;

	/*
		Decode source 2.
	*/
	if (!bMOV)
	{
		DecodeSrc12(psTarget, uInst0, uInst1, 2, IMG_TRUE, 
				   EURASIA_USE1_S2BEXT, psNextArg, eFmtControl, 
				   USEASM_ARGFLAGS_FMTF16);
		if (uInst1 & SGX545_USE1_FDUAL_S2NEG)
		{
			psNextArg->uFlags |= USEASM_ARGFLAGS_NEGATE;
		}
		psNextArg++;
	}

	if (psInst->uOpcode == USEASM_OP_IMA32)
	{
		/*
			Set up carry-in source - never used for dual-issued IMA32.
		*/
		SetupIntSrcSel(psNextArg, USEASM_INTSRCSEL_NONE);
		psNextArg++;
		SetupIntSrcSel(psNextArg, USEASM_INTSRCSEL_NONE);
		psNextArg++;
	}

	/*
		Decode co-issued instruction opcode.
	*/
	psInst->uFlags1 |= USEASM_OPFLAGS1_MAINISSUE;
	psCoInst->uFlags2 |= USEASM_OPFLAGS2_COISSUE;
	psCoInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);
	psCoInst->uOpcode = aeOp2[uOp2];

	/*
		Decode co-issued instruction destination.
	*/
	psCoNextArg->uType = USEASM_REGTYPE_FPINTERNAL;
	if (uInst1 & SGX545_USE1_FDUAL_OP2DEXTS0EXT)
	{
		switch (uOp2Dst)
		{
			case SGX545_USE1_FDUAL_OP2DSTS0_EXT_I2: psCoNextArg->uNumber = 2; break;
			case SGX545_USE1_FDUAL_OP2DSTS0_EXT_I0SRC0: psCoNextArg->uNumber = 0; break;
		}
	}
	else
	{
		switch (uOp2Dst)
		{
			case SGX545_USE1_FDUAL_OP2DSTS0_STD_I0: psCoNextArg->uNumber = 0; break;
			case SGX545_USE1_FDUAL_OP2DSTS0_STD_I1: psCoNextArg->uNumber = 1; break;
		}
	}
	psCoNextArg++;

	/*
		Add internal register destination for IMA32 - never used for dual-issued IMA32.
	*/
	if (psCoInst->uOpcode == USEASM_OP_IMA32)
	{
		SetupIntSrcSel(psCoNextArg, USEASM_INTSRCSEL_NONE);
		psCoNextArg++;
	}

	/*
		Decode co-issued source 0 if present.
	*/
	if (uOp2 == SGX545_USE1_FDUAL_OP2_FMAD ||
		uOp2 == SGX545_USE1_FDUAL_OP2_FMAD16 ||
		uOp2 == SGX545_USE1_FDUAL_OP2_FMSA ||
		uOp2 == SGX545_USE1_FDUAL_OP2_FCLMP ||
		uOp2 == SGX545_USE1_FDUAL_OP2_IMA32)
	{
		if ((uInst1 & SGX545_USE1_FDUAL_OP2DEXTS0EXT) && uOp2Dst == SGX545_USE1_FDUAL_OP2DSTS0_EXT_I0SRC0)
		{
			DecodeSrc0(psTarget, uInst0, uInst1, IMG_FALSE, 0, psCoNextArg, eFmtControl, USEASM_ARGFLAGS_FMTF16);
		}
		else
		{
			*psCoNextArg = psCoInst->asArg[0];
		}
		psCoNextArg++;
	}
	if (uOp2 != SGX545_USE1_FDUAL_OP2_MOV)
	{
		IMG_UINT32 uOp2Src1 = (uInst1 & ~SGX545_USE1_FDUAL_OP2S1_CLRMSK) >> SGX545_USE1_FDUAL_OP2S1_SHIFT;

		/*
			Decode co-issued source 1.
		*/
		if (uOp2Src1 != SGX545_USE1_FDUAL_OP2S1_SRC1)
		{
			psCoNextArg->uType = USEASM_REGTYPE_FPINTERNAL;
			switch (uOp2Src1)
			{
				case SGX545_USE1_FDUAL_OP2S1_I0: psCoNextArg->uNumber = 0; break;
				case SGX545_USE1_FDUAL_OP2S1_I1: psCoNextArg->uNumber = 1; break;
				case SGX545_USE1_FDUAL_OP2S1_I2: psCoNextArg->uNumber = 2; break;
			}
		}
		else
		{
			DecodeSrc12(psTarget, 
					    uInst0, 
					    uInst1, 
					    1, 
					    IMG_TRUE, 
					    EURASIA_USE1_S1BEXT, 
					    psCoNextArg, 
					    eFmtControl, 
					    USEASM_ARGFLAGS_FMTF16);
		}
		psCoNextArg++;

		/*
			Decode co-issued source 2.
		*/
		if (uOp2 == SGX545_USE1_FDUAL_OP2_FFRC ||
			uOp2 == SGX545_USE1_FDUAL_OP2_FMAD ||
			uOp2 == SGX545_USE1_FDUAL_OP2_FMAD16 ||
			uOp2 == SGX545_USE1_FDUAL_OP2_FMSA ||
			uOp2 == SGX545_USE1_FDUAL_OP2_FCLMP ||
			uOp2 == SGX545_USE1_FDUAL_OP2_FADD ||
			uOp2 == SGX545_USE1_FDUAL_OP2_FSUB ||
			uOp2 == SGX545_USE1_FDUAL_OP2_FMUL ||
			uOp2 == SGX545_USE1_FDUAL_OP2_IMA32)
		{
			IMG_UINT32	uOp2Src2 = (uInst1 & ~SGX545_USE1_FDUAL_OP2S2_CLRMSK) >> SGX545_USE1_FDUAL_OP2S2_SHIFT;

			if (uInst1 & SGX545_USE1_FDUAL_O2S2EXT_MSRCEXT)
			{
				if (uOp2Src2 == SGX545_USE1_FDUAL_OP2S2_EXT_I2)
				{
					psCoNextArg->uType = USEASM_REGTYPE_FPINTERNAL;
					psCoNextArg->uNumber = 2;
				}
				else
				{
					DecodeSrc12(psTarget, 
							    uInst0, 
							    uInst1, 
							    2, 
							    IMG_TRUE, 
							    EURASIA_USE1_S2BEXT, 
							    psCoNextArg, 
							    eFmtControl, 
							    USEASM_ARGFLAGS_FMTF16);
				}
			}
			else
			{
				if (uOp2Src2 == SGX545_USE1_FDUAL_OP2S2_STD_I0)
				{
					psCoNextArg->uType = USEASM_REGTYPE_FPINTERNAL;
					psCoNextArg->uNumber = 0;
				}
				else
				{
					psCoNextArg->uType = USEASM_REGTYPE_FPINTERNAL;
					psCoNextArg->uNumber = 1;
				}
			}
			psCoNextArg++;
		}
	}
	else
	{
		IMG_UINT32 uOp2Src1 = (uInst1 & ~SGX545_USE1_FDUAL_OP2S1_CLRMSK) >> SGX545_USE1_FDUAL_OP2S1_SHIFT;
		if (uInst1 & SGX545_USE1_FDUAL_O2S2EXT_MSRCEXT)
		{
			switch (uOp2Src1)
			{
				case SGX545_USE1_FDUAL_OP2S1_MSRCEXT_SRC0:
				{
					DecodeSrc0(psTarget, uInst0, uInst1, IMG_FALSE, 0, psCoNextArg, eFmtControl, USEASM_ARGFLAGS_FMTF16);
					break;
				}
				case SGX545_USE1_FDUAL_OP2S1_MSRCEXT_SRC1:
				{
					DecodeSrc12(psTarget, 
							    uInst0, 
							    uInst1, 
							    1, 
							    IMG_TRUE, 
							    EURASIA_USE1_S1BEXT, 
							    psCoNextArg, 
							    eFmtControl, 
							    USEASM_ARGFLAGS_FMTF16);
					break;
				}
				case SGX545_USE1_FDUAL_OP2S1_MSRCEXT_SRC2:
				{
					DecodeSrc12(psTarget, 
							    uInst0, 
							    uInst1, 
							    2, 
							    IMG_TRUE, 
							    EURASIA_USE1_S2BEXT, 
							    psCoNextArg, 
							    eFmtControl, 
							    USEASM_ARGFLAGS_FMTF16);
					break;
				}
				case SGX545_USE1_FDUAL_OP2S1_MSRCEXT_RESERVED:
				{
					return USEDISASM_ERROR_INVALID_FDUAL_MOV_SRC;
				}
			}
		}
		else
		{
			psCoNextArg->uType = USEASM_REGTYPE_FPINTERNAL;
			switch (uOp2Src1)
			{
				case SGX545_USE1_FDUAL_OP2S1_MSRCSTD_I0:
				{
					psCoNextArg->uNumber = 0;
					break;
				}
				case SGX545_USE1_FDUAL_OP2S1_MSRCSTD_I1:
				{
					psCoNextArg->uNumber = 1;
					break;
				}
				case SGX545_USE1_FDUAL_OP2S1_MSRCSTD_I2:
				{
					psCoNextArg->uNumber = 2;
					break;
				}
				case SGX545_USE1_FDUAL_OP2S1_MSRCSTD_RESERVED:
				{
					return USEDISASM_ERROR_INVALID_FDUAL_MOV_SRC;
				}
			}
		}
	}

	if (psCoInst->uOpcode == USEASM_OP_IMA32)
	{
		/*
			Set up carry-in source - never used for dual-issued IMA32.
		*/
		SetupIntSrcSel(psCoNextArg, USEASM_INTSRCSEL_NONE);
		psCoNextArg++;
		SetupIntSrcSel(psCoNextArg, USEASM_INTSRCSEL_NONE);
		psCoNextArg++;
	}

	return USEDISASM_OK;
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_DUAL_ISSUE) */

static IMG_VOID DecodeRepeatCountMask(IMG_UINT32	uInst1,
									  PUSE_INST		psInst)
{
	IMG_UINT32 uMaskCount = (uInst1 & ~EURASIA_USE1_RMSKCNT_CLRMSK) >> EURASIA_USE1_RMSKCNT_SHIFT;

	if (uInst1 & EURASIA_USE1_RCNTSEL)
	{
		psInst->uFlags1 |= ((uMaskCount + 1) << USEASM_OPFLAGS1_REPEAT_SHIFT);
		psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);
	}
	else
	{
		psInst->uFlags1 |= (uMaskCount << USEASM_OPFLAGS1_MASK_SHIFT);
	}
}

static const IMG_UINT32	g_auDecodeSourceMod[] =
	{
		0,													/* EURASIA_USE_SRCMOD_NONE */
		USEASM_ARGFLAGS_NEGATE,								/* EURASIA_USE_SRCMOD_NEGATE */
		USEASM_ARGFLAGS_ABSOLUTE,							/* EURASIA_USE_SRCMOD_ABSOLUTE */
		USEASM_ARGFLAGS_NEGATE | USEASM_ARGFLAGS_ABSOLUTE,	/* EURASIA_USE_SRCMOD_NEGABS */
	};


/*****************************************************************************
 FUNCTION	: DecodeFloatInstruction

 PURPOSE	: Decodes a complex op instruction back to the USEASM input format.

 PARAMETERS	: psTarget			- Target processor.
			  uInst1, uInst0	- Instruction to disassemble.
			  uOpcodec			- Instruction opcode.
			  psInst			- Destination for the encoded instruction.

 RETURNS	: Error code.
*****************************************************************************/
static USEDISASM_ERROR DecodeComplexOpInstruction(PCSGX_CORE_DESC psTarget,
												  IMG_UINT32     uInst0,
												  IMG_UINT32     uInst1,
												  PUSE_INST		 psInst)
{
	IMG_UINT32	uOp2 = (uInst1 & ~EURASIA_USE1_FLOAT_OP2_CLRMSK) >> EURASIA_USE1_FLOAT_OP2_SHIFT;
	IMG_UINT32	uDType = (uInst1 & ~EURASIA_USE1_FSCALAR_DTYPE_CLRMSK) >> EURASIA_USE1_FSCALAR_DTYPE_SHIFT;
	IMG_UINT32	uSrcComp = (uInst1 & ~EURASIA_USE1_FSCALAR_CHANSEL_CLRMSK) >> EURASIA_USE1_FSCALAR_CHANSEL_SHIFT;
	IMG_UINT32	uEPred;
	IMG_UINT32	uSrc1Mod;
	
	UseAsmInitInst(psInst);

	/*
		Decode the instruction predicate.
	*/
	uEPred = (uInst1 & ~EURASIA_USE1_EPRED_CLRMSK) >> EURASIA_USE1_EPRED_SHIFT;
	psInst->uFlags1 |= (g_aeDecodeEPred[uEPred] << USEASM_OPFLAGS1_PRED_SHIFT);

	/*
		Decode opcode.
	*/
	#if defined(SUPPORT_SGX_FEATURE_USE_SQRT_SIN_COS)
	if (IsSqrtSinCosInstruction(psTarget) && (uInst1 & SGX545_USE1_FSCALAR_GROUP))
	{
		static const USEASM_OPCODE aeExtScalarOpcode[] = 
		{
			USEASM_OP_FSQRT,				/* SGX545_USE1_FLOAT_OP2_SQRT */ 
			USEASM_OP_FSIN,					/* SGX545_USE1_FLOAT_OP2_SIN */ 
			USEASM_OP_FCOS,					/* SGX545_USE1_FLOAT_OP2_COS */
			USEASM_OP_UNDEF,				/* SGX545_USE1_FLOAT_OP2_SCALARRESERVED */
		};

		psInst->uOpcode = aeExtScalarOpcode[uOp2];
		if (psInst->uOpcode == USEASM_OP_UNDEF)
		{
			return USEDISASM_ERROR_INVALID_COMPLEX_OP2;
		}
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_SQRT_SIN_COS) */
	{
		static const USEASM_OPCODE aeScalarOpcode[] = 
		{	
			USEASM_OP_FRCP,	/* EURASIA_USE1_FLOAT_OP2_RCP */ 
			USEASM_OP_FRSQ,	/* EURASIA_USE1_FLOAT_OP2_RSQ */
			USEASM_OP_FLOG,	/* EURASIA_USE1_FLOAT_OP2_LOG */
			USEASM_OP_FEXP,	/* EURASIA_USE1_FLOAT_OP2_EXP */
		};

		psInst->uOpcode = aeScalarOpcode[uOp2];
	}

	/*
		Decode instruction modifiers.
	*/
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_SYNCSTART)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SYNCSTART;
	}
	if (uInst1 & EURASIA_USE1_END)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}
	if (uInst1 & EURASIA_USE1_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	if (uInst1 & EURASIA_USE1_FSCALAR_TYPEPRESERVE)
	{
		psInst->uFlags2 |= USEASM_OPFLAGS2_TYPEPRESERVE;
	}
	DecodeRepeatCountMask(uInst1, psInst);

	/*
		Decode destination.
	*/
	DecodeDest(psTarget, uInst0, uInst1, TRUE, &psInst->asArg[0], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	
	/*
		Decode source.
	*/
	DecodeSrc12(psTarget, uInst0, uInst1, 1, TRUE, EURASIA_USE1_S1BEXT, &psInst->asArg[1], USEDIS_FORMAT_CONTROL_STATE_OFF, USEASM_ARGFLAGS_FMTF16);

	/*
		Decode source modifiers.
	*/
	uSrc1Mod = (uInst1 & ~EURASIA_USE1_SRC1MOD_CLRMSK) >> EURASIA_USE1_SRC1MOD_SHIFT;
	psInst->asArg[1].uFlags |= g_auDecodeSourceMod[uSrc1Mod];

	/*
		Decode source/dest data type.
	*/
	switch (uDType)
	{
		case EURASIA_USE1_FSCALAR_DTYPE_F32: 
		{
			break;
		}
		case EURASIA_USE1_FSCALAR_DTYPE_F16:
		{
			psInst->asArg[1].uFlags |= USEASM_ARGFLAGS_FMTF16;
			psInst->asArg[1].uFlags |= ((uSrcComp << 1) << USEASM_ARGFLAGS_COMP_SHIFT);
			break;
		}
		case EURASIA_USE1_FSCALAR_DTYPE_C10:
		{
			psInst->asArg[1].uFlags |= USEASM_ARGFLAGS_FMTC10;
			psInst->asArg[1].uFlags |= (uSrcComp << USEASM_ARGFLAGS_COMP_SHIFT);
			break;
		}
		case EURASIA_USE1_FSCALAR_DTYPE_RESERVED:
		{
			return USEDISASM_ERROR_INVALID_COMPLEX_DATA_TYPE;
		}
	}
	
	return USEDISASM_OK;
}

/*****************************************************************************
 FUNCTION	: DecodeFloatInstruction

 PURPOSE	: Decodes a floating point instruction back to the USEASM
			  input format.

 PARAMETERS	: psTarget			- Target processor.
			  uInst1, uInst0	- Instruction to disassemble.
			  uOpcodec			- Instruction opcode.
			  psInst			- Destination for the decoded instruction.

 RETURNS	: Error code.
*****************************************************************************/
static USEDISASM_ERROR DecodeFloatInstruction(PCSGX_CORE_DESC psTarget,
											  IMG_UINT32     uInst0,
											  IMG_UINT32     uInst1,
											  IMG_UINT32	 uOpcode,
											  PUSE_INST      psInst)
{
	static const USEASM_OPCODE aeArithOpcode[] = 
	{
		USEASM_OP_FMAD,		/* EURASIA_USE1_FLOAT_OP2_MAD */
		USEASM_OP_FADM,		/* EURASIA_USE1_FLOAT_OP2_ADM */
		USEASM_OP_FMSA,		/* EURASIA_USE1_FLOAT_OP2_MSA */
		USEASM_OP_FSUBFLR,	/* EURASIA_USE1_FLOAT_OP2_FRC */
	};
	static const USEASM_OPCODE aeDotproductOpcode[] = 
	{
		USEASM_OP_FDP,		/* EURASIA_USE1_FLOAT_OP2_DP */
		USEASM_OP_FDDP,		/* EURASIA_USE1_FLOAT_OP2_DDP */
		USEASM_OP_FDDPC,	/* EURASIA_USE1_FLOAT_OP2_DDPC */
		USEASM_OP_UNDEF,
	};
	static const USEASM_OPCODE aeGradientOpcode[] = 
	{
		USEASM_OP_FDSX,			/* EURASIA_USE1_FLOAT_OP2_DSX */ 
		USEASM_OP_FDSY,			/* EURASIA_USE1_FLOAT_OP2_DSY */
		USEASM_OP_UNDEF,
		USEASM_OP_UNDEF,
	};
	static const USEASM_OPCODE aeFmad16Opcode[] = 
	{
		USEASM_OP_FMAD16,			/* EURASIA_USE1_FLOAT_OP2_FMAD16 */
		USEASM_OP_UNDEF, 
		USEASM_OP_UNDEF, 
		USEASM_OP_UNDEF,
	};
	#if defined(SUPPORT_SGX_FEATURE_USE_FMAD16_SWIZZLES)
	static const USEASM_MAD16SWZ aeDecodeFmad16Swizzle[] =
	{
		USEASM_MAD16SWZ_LOWHIGH,	/* SGX545_USE_FARITH16_SWZ_LOWHIGH */
		USEASM_MAD16SWZ_LOWLOW,		/* SGX545_USE_FARITH16_SWZ_LOWLOW */
		USEASM_MAD16SWZ_HIGHHIGH,	/* SGX545_USE_FARITH16_SWZ_HIGHHIGH */
		USEASM_MAD16SWZ_CVTFROMF32,	/* SGX545_USE_FARITH16_SWZ_CVTFROMF32 */
	};
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_FMAD16_SWIZZLES) */
	IMG_UINT32					uOp2;
	USEDIS_FORMAT_CONTROL_STATE eFmtControl;
	IMG_BOOL					bDDPC = IMG_FALSE;
	#if defined(SUPPORT_SGX_FEATURE_USE_FMAD16_SWIZZLES)
	IMG_BOOL					bFmad16Swizzle = IMG_FALSE;
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_FMAD16_SWIZZLES) */
	IMG_BOOL					bEndFlag = IMG_TRUE;
	PUSE_REGISTER				psNextArg = psInst->asArg;
	IMG_UINT32					uEPred;
	IMG_BOOL					bDestExtendedBanks;

	UseAsmInitInst(psInst);

	/*
		Decode the instruction predicate.
	*/
	uEPred = (uInst1 & ~EURASIA_USE1_EPRED_CLRMSK) >> EURASIA_USE1_EPRED_SHIFT;
	psInst->uFlags1 |= (g_aeDecodeEPred[uEPred] << USEASM_OPFLAGS1_PRED_SHIFT);

	uOp2 = (uInst1 & ~EURASIA_USE1_FLOAT_OP2_CLRMSK) >> EURASIA_USE1_FLOAT_OP2_SHIFT;
	switch (uOpcode)
	{
		case EURASIA_USE1_OP_FARITH: psInst->uOpcode = aeArithOpcode[uOp2]; break;
		case EURASIA_USE1_OP_FDOTPRODUCT: 
		{
			psInst->uOpcode = aeDotproductOpcode[uOp2]; 
			if (uOp2 == EURASIA_USE1_FLOAT_OP2_DP)
			{
				if (uInst0 & EURASIA_USE0_DP_CLIPENABLE)
				{
					psInst->uOpcode = USEASM_OP_FDPC;
				}
			}
			if (uOp2 == EURASIA_USE1_FLOAT_OP2_DDPC)
			{
				bDDPC = IMG_TRUE;
			}
			break;
		}
		case EURASIA_USE1_OP_FMINMAX: 
		{
			#if defined(SUPPORT_SGX_FEATURE_USE_FCLAMP)
			if (IsFClampInstruction(psTarget))
			{
				static const USEASM_OPCODE aeClampOpcode[] = 
				{
					USEASM_OP_FMAXMIN,		/* SGX545_USE1_FLOAT_OP2_MAXMIN */ 
					USEASM_OP_FMINMAX,		/* SGX545_USE1_FLOAT_OP2_MINMAX */
					USEASM_OP_UNDEF,
					USEASM_OP_UNDEF,
				};			
				psInst->uOpcode = aeClampOpcode[uOp2];
			}
			else
			#endif /* defined(SUPPORT_SGX_FEATURE_USE_FCLAMP) */
			{
				static const USEASM_OPCODE aeMinmaxOpcode[] = 
				{
					USEASM_OP_FMIN,		/* EURASIA_USE1_FLOAT_OP2_MIN */ 
					USEASM_OP_FMAX,		/* EURASIA_USE1_FLOAT_OP2_MAX */
					USEASM_OP_UNDEF, 
					USEASM_OP_UNDEF,
				};
				psInst->uOpcode = aeMinmaxOpcode[uOp2];
			}
			break;
		}
		case EURASIA_USE1_OP_FGRADIENT: psInst->uOpcode = aeGradientOpcode[uOp2]; break;
		case EURASIA_USE1_OP_FARITH16: 
		{
			#if defined(SUPPORT_SGX_FEATURE_USE_FMAD16_SWIZZLES)
			if (SupportsFmad16Swizzles(psTarget))
			{
				bFmad16Swizzle = IMG_TRUE;
				psInst->uOpcode = USEASM_OP_FMAD16;
				bEndFlag = IMG_FALSE;
			}
			else
			#endif /* defined(SUPPORT_SGX_FEATURE_USE_FMAD16_SWIZZLES) */
			{
				psInst->uOpcode = aeFmad16Opcode[uOp2];
			}
			break;
		}
	}
	if (psInst->uOpcode == USEASM_OP_UNDEF)
	{
		return USEDISASM_ERROR_INVALID_FLOAT_OP2;
	}

	eFmtControl = USEDIS_FORMAT_CONTROL_STATE_OFF;
	if (uOpcode != EURASIA_USE1_OP_FARITH16)
	{
		if (uInst1 & EURASIA_USE1_FLOAT_SFASEL)
		{
			eFmtControl = USEDIS_FORMAT_CONTROL_STATE_ON;
		}
	}

	/*
		Decode the instruction modifiers.
	*/
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_SYNCSTART)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SYNCSTART;
	}
	if (bEndFlag && (uInst1 & EURASIA_USE1_END))
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}
	if (uInst1 & EURASIA_USE1_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	#if defined(SUPPORT_SGX_FEATURE_USE_FMAD16_SWIZZLES)
	if (SupportsFmad16Swizzles(psTarget) && uOpcode == EURASIA_USE1_OP_FARITH16)
	{
		IMG_UINT32	uRptCount = (uInst1 & ~SGX545_USE1_FARITH16_RCNT_CLRMSK) >> SGX545_USE1_FARITH16_RCNT_SHIFT;

		psInst->uFlags1 |= ((uRptCount + 1) << USEASM_OPFLAGS1_REPEAT_SHIFT);
		psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

		psInst->uFlags2 |= USEASM_OPFLAGS2_PERINSTMOE;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_FMAD16_SWIZZLES) */
	#if defined(SUPPORT_SGX_FEATURE_USE_FMAD16_SWIZZLES) && defined(SUPPORT_SGX_FEATURE_USE_PER_INST_MOE_INCREMENTS)
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_FMAD16_SWIZZLES) && defined(SUPPORT_SGX_FEATURE_USE_PER_INST_MOE_INCREMENTS) */
	#if defined(SUPPORT_SGX_FEATURE_USE_PER_INST_MOE_INCREMENTS)
	if (IsPerInstMoeIncrements(psTarget))
	{
		IMG_UINT32	uRptCount = (uInst1 & ~SGX545_USE1_FLOAT_RCNT_CLRMSK) >> SGX545_USE1_FLOAT_RCNT_SHIFT;

		psInst->uFlags1 |= ((uRptCount + 1) << USEASM_OPFLAGS1_REPEAT_SHIFT);
		psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

		if (!(uInst1 & SGX545_USE1_FLOAT_MOE) && !bDDPC)
		{
			psInst->uFlags2 |= USEASM_OPFLAGS2_PERINSTMOE;
		}
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_PER_INST_MOE_INCREMENTS) */
	{
		DecodeRepeatCountMask(uInst1, psInst);
	}
	
	#if defined(SUPPORT_SGX_FEATURE_USE_PER_INST_MOE_INCREMENTS)
	/*
		Decode the per-source increments.
	*/
	if (psInst->uFlags2 & USEASM_OPFLAGS2_PERINSTMOE)
	{
		psInst->uFlags2 |= 
			((uInst1 & SGX545_USE1_FLOAT_S0INC) ? USEASM_OPFLAGS2_PERINSTMOE_INCS0 : 0) | 
			((uInst1 & SGX545_USE1_FLOAT_S1INC) ? USEASM_OPFLAGS2_PERINSTMOE_INCS1 : 0) |
			((uInst1 & SGX545_USE1_FLOAT_S2INC) ? USEASM_OPFLAGS2_PERINSTMOE_INCS2 : 0);
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_PER_INST_MOE_INCREMENTS) */

	/*
		Decode the destination register.
	*/
	if ((uOpcode == EURASIA_USE1_OP_FDOTPRODUCT && uOp2 == EURASIA_USE1_FLOAT_OP2_DDP) ||
		(uOpcode == EURASIA_USE1_OP_FDOTPRODUCT && uOp2 == EURASIA_USE1_FLOAT_OP2_DDPC))
	{
		bDestExtendedBanks = IMG_FALSE;
	}
	else
	{
		bDestExtendedBanks = IMG_TRUE;
	}
	DecodeDest(psTarget, uInst0, uInst1, bDestExtendedBanks, psNextArg, USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	psNextArg++;

	/*
		Decode extra internal register destinations.
	*/
	if ((uOpcode == EURASIA_USE1_OP_FDOTPRODUCT && uOp2 == EURASIA_USE1_FLOAT_OP2_DDP) ||
		(uOpcode == EURASIA_USE1_OP_FDOTPRODUCT && uOp2 == EURASIA_USE1_FLOAT_OP2_DDPC))
	{
		psNextArg->uType = USEASM_REGTYPE_FPINTERNAL;
		psNextArg->uNumber = (uInst1 & EURASIA_USE1_DOTPRODUCT_UPDATEI1) ? 1 : 0;
		psNextArg++;
	}

	/*
		Decode extra clipplane destinations.
	*/
	if (uOpcode == EURASIA_USE1_OP_FDOTPRODUCT && uOp2 == EURASIA_USE1_FLOAT_OP2_DP)
	{
		if (uInst0 & EURASIA_USE0_DP_CLIPENABLE)
		{
			psNextArg->uType = USEASM_REGTYPE_CLIPPLANE;
			psNextArg->uNumber = 
				(uInst0 & ~EURASIA_USE0_DP_CLIPPLANEUPDATE_CLRMSK) >> EURASIA_USE0_DP_CLIPPLANEUPDATE_SHIFT;
			psNextArg++;
		}
	}
	if (uOpcode == EURASIA_USE1_OP_FDOTPRODUCT && uOp2 == EURASIA_USE1_FLOAT_OP2_DDPC)
	{
		IMG_UINT32 uClip;
		uClip = (uInst1 & ~EURASIA_USE1_DDPC_CLIPPLANEUPDATE_CLRMSK) >> EURASIA_USE1_DDPC_CLIPPLANEUPDATE_SHIFT;

		psNextArg->uType = USEASM_REGTYPE_CLIPPLANE;
		psNextArg->uNumber = (uClip >> 0) & 0x7;
		psNextArg++;

		psNextArg->uType = USEASM_REGTYPE_CLIPPLANE;
		psNextArg->uNumber = (uClip >> 3) & 0x7;
		psNextArg++;
	}

	/*
		Decode source 0
	*/
	if ((uOpcode == EURASIA_USE1_OP_FARITH && uOp2 != EURASIA_USE1_FLOAT_OP2_FRC) || 
		(uOpcode == EURASIA_USE1_OP_FDOTPRODUCT && uOp2 == EURASIA_USE1_FLOAT_OP2_DDP) ||
		(uOpcode == EURASIA_USE1_OP_FDOTPRODUCT && uOp2 == EURASIA_USE1_FLOAT_OP2_DDPC) ||
		uOpcode == EURASIA_USE1_OP_FARITH16 ||
		(uOpcode == EURASIA_USE1_OP_FMINMAX && IsFClampInstruction(psTarget)))
	{
		DecodeSrc0(psTarget, uInst0, uInst1, FALSE, EURASIA_USE1_S0BEXT, psNextArg, eFmtControl, USEASM_ARGFLAGS_FMTF16);
		#if defined(SUPPORT_SGX_FEATURE_USE_FMAD16_SWIZZLES)
		if (bFmad16Swizzle)
		{
			IMG_UINT32	uSrc0Swizzle;
			
			uSrc0Swizzle = (uInst1 & ~SGX545_USE1_FARITH16_SRC0SWZ_CLRMSK) >> SGX545_USE1_FARITH16_SRC0SWZ_SHIFT;
			psNextArg->uFlags |= (aeDecodeFmad16Swizzle[uSrc0Swizzle] << USEASM_ARGFLAGS_MAD16SWZ_SHIFT);
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_FMAD16_SWIZZLES) */
		if (!bDDPC)
		{
			#if defined(SUPPORT_SGX_FEATURE_USE_PER_INST_MOE_INCREMENTS)
			if (IsPerInstMoeIncrements(psTarget))
			{
				if (uInst1 & SGX545_USE1_FLOAT_SRC0ABS)
				{
					psNextArg->uFlags |= USEASM_ARGFLAGS_ABSOLUTE;
				}
			}
			else
			#endif /* defined(SUPPORT_SGX_FEATURE_USE_PER_INST_MOE_INCREMENTS) */
			{
				IMG_UINT32	uSrc0Mod;

				uSrc0Mod = (uInst1 & ~EURASIA_USE1_SRC0MOD_CLRMSK) >> EURASIA_USE1_SRC0MOD_SHIFT;
				psNextArg->uFlags |= g_auDecodeSourceMod[uSrc0Mod];
			}
		}
		psNextArg++;
	}

	/*
		Decode source 1.
	*/
	DecodeSrc12(psTarget, uInst0, uInst1, 1, TRUE, EURASIA_USE1_S1BEXT, psNextArg, eFmtControl, USEASM_ARGFLAGS_FMTF16);
	#if defined(SUPPORT_SGX_FEATURE_USE_FMAD16_SWIZZLES)
	if (bFmad16Swizzle)
	{
		IMG_UINT32	uLow, uHigh, uSwz;

		uLow = (uInst1 & ~SGX545_USE1_FARITH16_SRC1SWZL_CLRMSK) >> SGX545_USE1_FARITH16_SRC1SWZL_SHIFT;
		uSwz = (uLow << SGX545_USE1_FARITH16_SRC1SWZL_INTERNALSHIFT);
		uHigh = (uInst1 & ~SGX545_USE1_FARITH16_SRC1SWZH_CLRMSK) >> SGX545_USE1_FARITH16_SRC1SWZH_SHIFT;
		uSwz |= (uHigh << SGX545_USE1_FARITH16_SRC1SWZH_INTERNALSHIFT);

		psNextArg->uFlags |= (aeDecodeFmad16Swizzle[uSwz] << USEASM_ARGFLAGS_MAD16SWZ_SHIFT);
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_FMAD16_SWIZZLES) */
	if (!bDDPC)
	{
		IMG_UINT32	uSrc1Mod;

		uSrc1Mod = (uInst1 & ~EURASIA_USE1_SRC1MOD_CLRMSK) >> EURASIA_USE1_SRC1MOD_SHIFT;
		psNextArg->uFlags |= g_auDecodeSourceMod[uSrc1Mod];
	}
	psNextArg++;

	/*
		Decode source 2.
	*/
	if (uOpcode == EURASIA_USE1_OP_FARITH || 
		uOpcode == EURASIA_USE1_OP_FDOTPRODUCT ||
		uOpcode == EURASIA_USE1_OP_FMINMAX ||
		uOpcode == EURASIA_USE1_OP_FGRADIENT ||
		uOpcode == EURASIA_USE1_OP_FARITH16)
	{
		DecodeSrc12(psTarget, uInst0, uInst1, 2, TRUE, EURASIA_USE1_S2BEXT, psNextArg, eFmtControl, USEASM_ARGFLAGS_FMTF16);
		#if defined(SUPPORT_SGX_FEATURE_USE_FMAD16_SWIZZLES)
		if (bFmad16Swizzle)
		{
			IMG_UINT32	uSrc2Swizzle;

			uSrc2Swizzle = (uInst1 & ~SGX545_USE1_FARITH16_SRC2SWZ_CLRMSK) >> SGX545_USE1_FARITH16_SRC2SWZ_SHIFT;
			psNextArg->uFlags |= (aeDecodeFmad16Swizzle[uSrc2Swizzle] << USEASM_ARGFLAGS_MAD16SWZ_SHIFT);
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_FMAD16_SWIZZLES) */
		if (!bDDPC)
		{
			IMG_UINT32	uSrc2Mod;

			uSrc2Mod = (uInst1 & ~EURASIA_USE1_SRC2MOD_CLRMSK) >> EURASIA_USE1_SRC2MOD_SHIFT;
			psNextArg->uFlags |= g_auDecodeSourceMod[uSrc2Mod];
		}
		psNextArg++;
	}
	
	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeMovcInstruction(PCSGX_CORE_DESC psTarget,
										     IMG_UINT32     uInst0,
										     IMG_UINT32     uInst1,
										     PUSE_INST		psInst)
{
	IMG_UINT32 uTstDType = (uInst1 & ~EURASIA_USE1_MOVC_TSTDTYPE_CLRMSK) >> EURASIA_USE1_MOVC_TSTDTYPE_SHIFT;
	IMG_UINT32 uTstCc = (uInst1 & ~EURASIA_USE1_MOVC_TESTCC_CLRMSK) >> EURASIA_USE1_MOVC_TESTCC_SHIFT;
	IMG_BOOL bCoverage;
	USEASM_PRED ePredicate;
	PUSE_REGISTER psNextArg;

	bCoverage = IMG_FALSE;
	#if defined(SUPPORT_SGX_FEATURE_USE_ALPHATOCOVERAGE)
	if (
			(
				uTstDType == SGX545_USE1_MOVC_TSTDTYPE_COVERAGE ||
				uTstDType == SGX545_USE1_MOVC_TSTDTYPE_COVERAGE_SMASK
			) && 
			SupportsAlphaToCoverage(psTarget)
	   )
	{
		bCoverage = IMG_TRUE;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_ALPHATOCOVERAGE) */

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*
		Decode predicate.
	*/
	ePredicate = g_aeDecodeEPred[(uInst1 & ~EURASIA_USE1_EPRED_CLRMSK) >> EURASIA_USE1_EPRED_SHIFT];
	psInst->uFlags1 |= (ePredicate << USEASM_OPFLAGS1_PRED_SHIFT);

	#if defined(SUPPORT_SGX_FEATURE_USE_ALPHATOCOVERAGE)
	if (bCoverage)
	{
		psInst->uOpcode = USEASM_OP_MOVMSK;

		if (uTstDType == SGX545_USE1_MOVC_TSTDTYPE_COVERAGE_SMASK)
		{
			psInst->uFlags3 |= USEASM_OPFLAGS3_SAMPLEMASK;
		}
		
		if (!(uInst1 & SGX545_USE1_MOVC_COVERAGETYPEEXT))
		{
			if (uTstCc == SGX545_USE1_MOVC_COVERAGETYPE_STDMASK)
			{
				psInst->uFlags1 |= USEASM_OPFLAGS1_MOVC_FMTMASK;
			}
			else
			{
				psInst->uFlags1 |= USEASM_OPFLAGS1_MOVC_FMTI8;
			}
		}
		else
		{
			if (uTstCc == SGX545_USE1_MOVC_COVERAGETYPE_EXTF16)
			{
				psInst->uFlags1 |= USEASM_OPFLAGS1_MOVC_FMTF16;
			}
			else
			{
				psInst->uFlags2 |= USEASM_OPFLAGS2_MOVC_FMTF32;
			}
		}
	}
	else 
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_ALPHATOCOVERAGE) */
	{
		if (uTstDType != EURASIA_USE1_MOVC_TSTDTYPE_UNCOND)
		{
			psInst->uOpcode = USEASM_OP_MOVC;
			psInst->uFlags1 |= USEASM_OPFLAGS1_TESTENABLE;

			switch (uTstDType)
			{
				case EURASIA_USE1_MOVC_TSTDTYPE_INT8: psInst->uFlags1 |= USEASM_OPFLAGS1_MOVC_FMTI8; break;
				case EURASIA_USE1_MOVC_TSTDTYPE_INT16: psInst->uFlags2 |= USEASM_OPFLAGS2_MOVC_FMTI16; break;
				case EURASIA_USE1_MOVC_TSTDTYPE_INT32: psInst->uFlags2 |= USEASM_OPFLAGS2_MOVC_FMTI32; break;
				case EURASIA_USE1_MOVC_TSTDTYPE_FLOAT: psInst->uFlags2 |= USEASM_OPFLAGS2_MOVC_FMTF32; break;
				case EURASIA_USE1_MOVC_TSTDTYPE_INT10: psInst->uFlags2 |= USEASM_OPFLAGS2_MOVC_FMTI10; break;
				default:
				{
					return USEDISASM_ERROR_INVALID_MOVC_DATA_TYPE;
				}
			}

			if (uInst1 & EURASIA_USE1_MOVC_TESTCCEXT) 
			{
				switch (uTstCc)
				{
					case EURASIA_USE1_MOVC_TESTCC_EXTSIGNED: psInst->uTest = USEASM_TEST_LTZERO; break;
					case EURASIA_USE1_MOVC_TESTCC_EXTNONSIGNED: psInst->uTest = USEASM_TEST_GTEZERO; break;
				}
			}
			else
			{
				switch (uTstCc)
				{
					case EURASIA_USE1_MOVC_TESTCC_STDZERO: psInst->uTest = USEASM_TEST_EQZERO; break;
					case EURASIA_USE1_MOVC_TESTCC_STDNONZERO: psInst->uTest = USEASM_TEST_NEQZERO; break;
				}
			}
		}
		else
		{
			psInst->uOpcode = USEASM_OP_MOV;
		}
	}
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_SYNCSTART)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SYNCSTART;
	}
	if (uInst1 & EURASIA_USE1_END)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}
	if (uInst1 & EURASIA_USE1_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	DecodeRepeatCountMask(uInst1, psInst);

	psNextArg = psInst->asArg;
	DecodeDest(psTarget, uInst0, uInst1, TRUE, psNextArg, USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	psNextArg++;

	if (uTstDType != EURASIA_USE1_MOVC_TSTDTYPE_UNCOND)
	{
		#if defined(SUPPORT_SGX_FEATURE_USE_ALPHATOCOVERAGE)
		if (bCoverage && uTstDType == SGX545_USE1_MOVC_TSTDTYPE_COVERAGE)
		{
			SetupIntSrcSel(psNextArg, USEASM_INTSRCSEL_NONE);
		}
		else
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_ALPHATOCOVERAGE) */
		{
			DecodeSrc0(psTarget, uInst0, uInst1, FALSE, 0, psNextArg, USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
		}
		psNextArg++;
	}

	DecodeSrc12(psTarget, uInst0, uInst1, 1, TRUE, EURASIA_USE1_S1BEXT, psNextArg, USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	psNextArg++;

	if (uTstDType != EURASIA_USE1_MOVC_TSTDTYPE_UNCOND)
	{
		DecodeSrc12(psTarget, uInst0, uInst1, 2, TRUE, EURASIA_USE1_S2BEXT, psNextArg, USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
		#if defined(SUPPORT_SGX_FEATURE_USE_ALPHATOCOVERAGE)
		if (bCoverage)
		{
			IMG_UINT32	uSrc2Comp;

			uSrc2Comp = (uInst1 & ~SGX545_USE1_MOVC_S2SCSEL_CLRMSK) >> SGX545_USE1_MOVC_S2SCSEL_SHIFT;
			psNextArg->uFlags |= (uSrc2Comp << USEASM_ARGFLAGS_COMP_SHIFT);
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_ALPHATOCOVERAGE) */

		psNextArg++;
	}

	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeEfoInstruction(PCSGX_CORE_DESC				psTarget,
										    IMG_UINT32					uInst0,
										    IMG_UINT32					uInst1,
										    PUSE_INST					psInst,
											USEDIS_FORMAT_CONTROL_STATE	eEFOFormatControlState)
{
	IMG_UINT32 uEfoRptCount = ((uInst1 & ~EURASIA_USE1_EFO_RCOUNT_CLRMSK) >> EURASIA_USE1_EFO_RCOUNT_SHIFT);

	UseAsmInitInst(psInst);

	#if defined(SUPPORT_SGX_FEATURE_USE_NEW_EFO_OPTIONS)
	if (HasNewEfoOptions(psTarget))
	{
		IMG_UINT32	uSPred;

		uSPred = (uInst1 & ~SGX545_USE1_EFO_SPRED_CLRMSK) >> SGX545_USE1_EFO_SPRED_SHIFT;
		psInst->uFlags1 |= (g_aeDecodeSPred[uSPred] << USEASM_OPFLAGS1_PRED_SHIFT);
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_NEW_EFO_OPTIONS) */
	{
		IMG_UINT32	uEPred;

		uEPred = (uInst1 & ~EURASIA_USE1_EPRED_CLRMSK) >> EURASIA_USE1_EPRED_SHIFT;
		psInst->uFlags1 |= (g_aeDecodeEPred[uEPred] << USEASM_OPFLAGS1_PRED_SHIFT);
	}

	psInst->uOpcode = USEASM_OP_EFO;

	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	#if defined(SUPPORT_SGX_FEATURE_USE_NEW_EFO_OPTIONS)
	if (HasNewEfoOptions(psTarget))
	{
		if (uInst1 & SGX545_USE1_EFO_GPIROW)
		{
			psInst->uFlags3 |= USEASM_OPFLAGS3_GPIEXT;
		}
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_NEW_EFO_OPTIONS) */
	psInst->uFlags1 |= ((uEfoRptCount + 1) << USEASM_OPFLAGS1_REPEAT_SHIFT);
	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

	DecodeDest(psTarget, uInst0, uInst1, FALSE, &psInst->asArg[0], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);

	#if defined(SUPPORT_SGX_FEATURE_USE_NEW_EFO_OPTIONS)
	if (HasNewEfoOptions(psTarget))
	{
		DecodeSrc0(psTarget, 
				   uInst0, 
				   uInst1, 
				   IMG_TRUE, 
				   SGX545_USE1_EFO_S0BEXT, 
				   &psInst->asArg[1], 
				   eEFOFormatControlState, 
				   USEASM_ARGFLAGS_FMTF16);
		if (uInst1 & SGX545_USE1_EFO_SRC0NEG)
		{
			psInst->asArg[1].uFlags |= USEASM_ARGFLAGS_NEGATE;
		}
		
		DecodeSrc12(psTarget, 
				    uInst0, 
				    uInst1, 
				    1, 
				    FALSE, 
				    0, 
				    &psInst->asArg[2], 
				    eEFOFormatControlState, 
				    USEASM_ARGFLAGS_FMTF16);
		if (uInst1 & SGX545_USE1_EFO_SRC1NEG)
		{
			psInst->asArg[2].uFlags |= USEASM_ARGFLAGS_NEGATE;
		}
		
		DecodeSrc12(psTarget, 
				    uInst0, 
				    uInst1, 
				    2, 
				    TRUE, 
				    SGX545_USE1_EFO_S2BEXT, 
				    &psInst->asArg[3], 
				    eEFOFormatControlState, 
				    USEASM_ARGFLAGS_FMTF16);
		if (uInst1 & SGX545_USE1_EFO_SRC2NEG)
		{
			psInst->asArg[3].uFlags |= USEASM_ARGFLAGS_NEGATE;
		}
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_NEW_EFO_OPTIONS) */
	{
		IMG_UINT32	uSrc0Mod, uSrc1Mod, uSrc2Mod;

		DecodeSrc0(psTarget, 
				   uInst0, 
				   uInst1, 
				   IMG_FALSE, 
				   EURASIA_USE1_S0BEXT, 
				   &psInst->asArg[1], 
				   eEFOFormatControlState, 
				   USEASM_ARGFLAGS_FMTF16);
		uSrc0Mod = (uInst1 & ~EURASIA_USE1_SRC0MOD_CLRMSK) >> EURASIA_USE1_SRC0MOD_SHIFT;
		psInst->asArg[1].uFlags |= g_auDecodeSourceMod[uSrc0Mod];

		DecodeSrc12(psTarget, 
				    uInst0, 
				    uInst1, 
				    1, 
				    IMG_FALSE, 
				    0, 
				    &psInst->asArg[2], 
				    eEFOFormatControlState, 
				    USEASM_ARGFLAGS_FMTF16);
		uSrc1Mod = (uInst1 & ~EURASIA_USE1_SRC1MOD_CLRMSK) >> EURASIA_USE1_SRC1MOD_SHIFT;
		psInst->asArg[2].uFlags |= g_auDecodeSourceMod[uSrc1Mod];

		DecodeSrc12(psTarget, 
				    uInst0, 
				    uInst1, 
				    2, 
				    IMG_FALSE, 
				    0, 
				    &psInst->asArg[3], 
				    eEFOFormatControlState, 
				    USEASM_ARGFLAGS_FMTF16);
		uSrc2Mod = (uInst1 & ~EURASIA_USE1_SRC2MOD_CLRMSK) >> EURASIA_USE1_SRC2MOD_SHIFT;
		psInst->asArg[3].uFlags |= g_auDecodeSourceMod[uSrc2Mod];
	}

	psInst->asArg[4].uType = USEASM_REGTYPE_IMMEDIATE;
	psInst->asArg[4].uNumber =
		uInst1 & ((~EURASIA_USE1_EFO_DSRC_CLRMSK) | 
				  EURASIA_USE1_EFO_WI0EN |
				  EURASIA_USE1_EFO_WI1EN |
				  (~EURASIA_USE1_EFO_ISRC_CLRMSK) |
				  (~EURASIA_USE1_EFO_ASRC_CLRMSK) |
				  EURASIA_USE1_EFO_A1LNEG |
				  #if defined(SUPPORT_SGX_FEATURE_USE_NEW_EFO_OPTIONS)
				  SGX545_USE1_EFO_A0RNEG |
				  #endif /* defined(SUPPORT_SGX_FEATURE_USE_NEW_EFO_OPTIONS) */
				  (~EURASIA_USE1_EFO_MSRC_CLRMSK));

	return USEDISASM_OK;
}

static IMG_VOID DisassembleEfoInstruction(PCSGX_CORE_DESC psTarget,
										  IMG_UINT32     uInst0,
										  IMG_UINT32     uInst1,
										  IMG_PCHAR      pszInst)
{
	static const IMG_PCHAR pszI0I1A0A1[] = {"i0", "i1", "a0", "a1"};
	static const IMG_PCHAR pszISrcI0[] = {"a0", "a1", "m0", "a0"};
	static const IMG_PCHAR pszISrcI1[] = {"a1", "a0", "m1", "m1"};
	static const IMG_PCHAR pszASrc[] = {"a0=m0+m1, a1=%si1+i0",
										"a0=m0+src2, a1=%si1+i0",
										"a0=i0+m0, a1=%si1+m1",
										"a0=src0+src1 a1=%ssrc2+src0"};
	static const IMG_PCHAR pszMSrc[] = {"m0=src0*src1, m1=src0*src2",
										"m0=src0*src1, m1=src0*src0",
										"m0=src1*src2, m1=src0*src0",
										"m0=src1*i0, m1=src0*i1"};
	#if defined(SUPPORT_SGX_FEATURE_USE_NEW_EFO_OPTIONS)
	static const IMG_PCHAR pszNewASrc[] = {"a0=m0%sm1, a1=%si1+i0",
										   "a0=m0%ssrc0, a1=%si1+i0",
										   "a0=m0%si0, a1=%si1+m1",
										   "a0=src0%ssrc2 a1=%ssrc1+src2"};
	static const IMG_PCHAR pszNewMSrc[] = {"m0=src0*src2, m1=src1*src2",
										   "m0=src2*src1, m1=src2*src2",
										   "m0=src1*src0, m1=src2*src2",
										   "m0=src1*i0, m1=src2*i1"};
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_NEW_EFO_OPTIONS) */
	USE_INST	sEFOFormatControlOff;
	USE_INST	sEFOFormatControlOn;
	IMG_UINT32	uEfoParam;
	IMG_UINT32	uArg;

	DecodeEfoInstruction(psTarget, uInst0, uInst1, &sEFOFormatControlOff, USEDIS_FORMAT_CONTROL_STATE_OFF);
	DecodeEfoInstruction(psTarget, uInst0, uInst1, &sEFOFormatControlOn, USEDIS_FORMAT_CONTROL_STATE_ON);

	/*
		Print instruction predicate.
	*/
	PrintPredicate(&sEFOFormatControlOff, pszInst);

	/*
		Print instruction opcode.
	*/
	usedisasm_asprintf(pszInst, "%s", OpcodeName(sEFOFormatControlOff.uOpcode));

	/*
		Print instruction modifier flags.
	*/
	PrintInstructionFlags(&sEFOFormatControlOff, pszInst);
	usedisasm_asprintf(pszInst, " ");

	PrintDestRegister(&sEFOFormatControlOff, &sEFOFormatControlOff.asArg[0], pszInst);

	uEfoParam = sEFOFormatControlOff.asArg[4].uNumber;
	usedisasm_asprintf(pszInst, "= %s, ", 
			 pszI0I1A0A1[(uEfoParam & ~EURASIA_USE1_EFO_DSRC_CLRMSK) >> EURASIA_USE1_EFO_DSRC_SHIFT]);
	usedisasm_asprintf(pszInst, "%si0 = %s, %si1 = %s, ",
			(uInst1 & EURASIA_USE1_EFO_WI0EN) ? "" : "!",
			pszISrcI0[(uEfoParam & ~EURASIA_USE1_EFO_ISRC_CLRMSK) >> EURASIA_USE1_EFO_ISRC_SHIFT],
			(uInst1 & EURASIA_USE1_EFO_WI1EN) ? "" : "!",
			pszISrcI1[(uEfoParam & ~EURASIA_USE1_EFO_ISRC_CLRMSK) >> EURASIA_USE1_EFO_ISRC_SHIFT]);

	#if defined(SUPPORT_SGX_FEATURE_USE_NEW_EFO_OPTIONS)
	if (HasNewEfoOptions(psTarget))
	{
		usedisasm_asprintf(pszInst, pszNewASrc[(uEfoParam & ~EURASIA_USE1_EFO_ASRC_CLRMSK) >> EURASIA_USE1_EFO_ASRC_SHIFT],
				(uEfoParam & SGX545_USE1_EFO_A0RNEG) ? "-" : "+",
				(uEfoParam & EURASIA_USE1_EFO_A1LNEG) ? "-" : "");
		usedisasm_asprintf(pszInst, ", %s",
				pszNewMSrc[(uEfoParam & ~EURASIA_USE1_EFO_MSRC_CLRMSK) >> EURASIA_USE1_EFO_MSRC_SHIFT]);
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_NEW_EFO_OPTIONS) */
	{
		usedisasm_asprintf(pszInst, pszASrc[(uEfoParam & ~EURASIA_USE1_EFO_ASRC_CLRMSK) >> EURASIA_USE1_EFO_ASRC_SHIFT],
				(uEfoParam & EURASIA_USE1_EFO_A1LNEG) ? "-" : "");
		usedisasm_asprintf(pszInst, ", %s",
				pszMSrc[(uEfoParam & ~EURASIA_USE1_EFO_MSRC_CLRMSK) >> EURASIA_USE1_EFO_MSRC_SHIFT]);
	}

	for (uArg = 0; uArg < 3; uArg++)
	{
		PUSE_REGISTER	psFCOffArg = &sEFOFormatControlOff.asArg[1 + uArg];
		PUSE_REGISTER	psFCOnArg = &sEFOFormatControlOn.asArg[1 + uArg];

		usedisasm_asprintf(pszInst, ", ");
		PrintSourceRegister(&sEFOFormatControlOff, psFCOffArg, pszInst);

		if (psFCOffArg->uType != psFCOnArg->uType ||
			psFCOffArg->uNumber != psFCOnArg->uNumber ||
			psFCOffArg->uIndex != psFCOnArg->uIndex ||
			psFCOffArg->uFlags != psFCOnArg->uFlags)
		{
			usedisasm_asprintf(pszInst, "/");
			PrintSourceRegister(&sEFOFormatControlOff, psFCOnArg, pszInst);
		}
	}
}

#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
#define USEASM_SWIZZLE3(SELX, SELY, SELZ) USEASM_SWIZZLE(SELX, SELY, SELZ, X)

static const IMG_UINT32 g_puVec3StdSwizzle[] =
{
	USEASM_SWIZZLE3(X, X, X),	/* SGXVEC_USE_VEC3_SWIZ_STD_XXX	*/
	USEASM_SWIZZLE3(Y, Y, Y),	/* SGXVEC_USE_VEC3_SWIZ_STD_YYY */
	USEASM_SWIZZLE3(Z, Z, Z),	/* SGXVEC_USE_VEC3_SWIZ_STD_ZZZ */
	USEASM_SWIZZLE3(W, W, W),	/* SGXVEC_USE_VEC3_SWIZ_STD_WWW */
	USEASM_SWIZZLE3(X, Y, Z),	/* SGXVEC_USE_VEC3_SWIZ_STD_XYZ */
	USEASM_SWIZZLE3(Y, Z, W),	/* SGXVEC_USE_VEC3_SWIZ_STD_YZW */
	USEASM_SWIZZLE3(X, X, Y),	/* SGXVEC_USE_VEC3_SWIZ_STD_XXY */
	USEASM_SWIZZLE3(X, Y, X),	/* SGXVEC_USE_VEC3_SWIZ_STD_XYX */

	USEASM_SWIZZLE3(Y, Y, X),	/* SGXVEC_USE_VEC3_SWIZ_STD_YYX */
	USEASM_SWIZZLE3(Y, Y, Z),	/* SGXVEC_USE_VEC3_SWIZ_STD_YYZ */
	USEASM_SWIZZLE3(Z, X, Y),	/* SGXVEC_USE_VEC3_SWIZ_STD_ZXY */
	USEASM_SWIZZLE3(X, Z, Y),	/* SGXVEC_USE_VEC3_SWIZ_STD_XZY */
	USEASM_SWIZZLE3(Y, Z, X),	/* SGXVEC_USE_VEC3_SWIZ_STD_YZX */
	USEASM_SWIZZLE3(Z, Y, X),	/* SGXVEC_USE_VEC3_SWIZ_STD_ZYX */
	USEASM_SWIZZLE3(Z, Z, Y),	/* SGXVEC_USE_VEC3_SWIZ_STD_ZZY */
	USEASM_SWIZZLE3(X, Y, 1),	/* SGXVEC_USE_VEC3_SWIZ_STD_XY1 */
};

static const IMG_UINT32 g_puVec3ExtSwizzle[] =
{
	USEASM_SWIZZLE3(X, Y, Y),	/* SGXVEC_USE_VEC3_SWIZ_EXT_XYY */
	USEASM_SWIZZLE3(Y, X, Y),	/* SGXVEC_USE_VEC3_SWIZ_EXT_YXY */
	USEASM_SWIZZLE3(X, X, Z),	/* SGXVEC_USE_VEC3_SWIZ_EXT_XXZ */
	USEASM_SWIZZLE3(Y, X, X),	/* SGXVEC_USE_VEC3_SWIZ_EXT_YXX */
	USEASM_SWIZZLE3(X, Y, 0),	/* SGXVEC_USE_VEC3_SWIZ_EXT_XY0 */
	USEASM_SWIZZLE3(X, 1, 0),	/* SGXVEC_USE_VEC3_SWIZ_EXT_X10 */
	USEASM_SWIZZLE3(0, 0, 0),	/* SGXVEC_USE_VEC3_SWIZ_EXT_000 */
	USEASM_SWIZZLE3(1, 1, 1),	/* SGXVEC_USE_VEC3_SWIZ_EXT_111 */
	USEASM_SWIZZLE3(HALF, HALF, HALF),	/* SGXVEC_USE_VEC3_SWIZ_EXT_HALF */
	USEASM_SWIZZLE3(2, 2, 2),	/* SGXVEC_USE_VEC3_SWIZ_EXT_222 */
	USEASM_SWIZZLE3(X, 0, 0),	/* SGXVEC_USE_VEC3_SWIZ_EXT_X00 */
	USE_UNDEF,
	USE_UNDEF,
	USE_UNDEF,
	USE_UNDEF,
	USE_UNDEF,
};

static const IMG_UINT32 g_puVec4StdSwizzle[] = 
{
	 USEASM_SWIZZLE(X, X, X, X),	/* SGXVEC_USE_VEC4_SWIZ_STD_XXXX */
	 USEASM_SWIZZLE(Y, Y, Y, Y),	/* SGXVEC_USE_VEC4_SWIZ_STD_YYYY */
	 USEASM_SWIZZLE(Z, Z, Z, Z),	/* SGXVEC_USE_VEC4_SWIZ_STD_ZZZZ */
	 USEASM_SWIZZLE(W, W, W, W),	/* SGXVEC_USE_VEC4_SWIZ_STD_WWWW */
	 USEASM_SWIZZLE(X, Y, Z, W),	/* SGXVEC_USE_VEC4_SWIZ_STD_XYZW */
	 USEASM_SWIZZLE(Y, Z, W, W),	/* SGXVEC_USE_VEC4_SWIZ_STD_YZWW */
	 USEASM_SWIZZLE(X, Y, Z, Z),	/* SGXVEC_USE_VEC4_SWIZ_STD_XYZZ */
	 USEASM_SWIZZLE(X, X, Y, Z),	/* SGXVEC_USE_VEC4_SWIZ_STD_XXYZ */
	 USEASM_SWIZZLE(X, Y, X, Y),	/* SGXVEC_USE_VEC4_SWIZ_STD_XYXY */
	 USEASM_SWIZZLE(X, Y, W, Z),	/* SGXVEC_USE_VEC4_SWIZ_STD_XYWZ */
	 USEASM_SWIZZLE(Z, X, Y, W),	/* SGXVEC_USE_VEC4_SWIZ_STD_ZXYW */
	 USEASM_SWIZZLE(Z, W, Z, W),	/* SGXVEC_USE_VEC4_SWIZ_STD_ZWZW */
	 USEASM_SWIZZLE(Y, Z, X, Z),	/* SGXVEC_USE_VEC4_SWIZ_STD_YZXZ */
	 USEASM_SWIZZLE(X, X, Y, Y),	/* SGXVEC_USE_VEC4_SWIZ_STD_XXYY */
	 USEASM_SWIZZLE(X, Z, W, W),	/* SGXVEC_USE_VEC4_SWIZ_STD_XZWW */
	 USEASM_SWIZZLE(X, Y, Z, 1),	/* SGXVEC_USE_VEC4_SWIZ_STD_XYZ1 */
};

static const IMG_UINT32 g_puVec4ExtSwizzle[] = 
{
	 USEASM_SWIZZLE(Y, Z, X, W),	/* SGXVEC_USE_VEC4_SWIZ_EXT_YZXW */
	 USEASM_SWIZZLE(Z, W, X, Y),	/* SGXVEC_USE_VEC4_SWIZ_EXT_ZWXY */
	 USEASM_SWIZZLE(X, Z, W, Y),	/* SGXVEC_USE_VEC4_SWIZ_EXT_XZWY */
	 USEASM_SWIZZLE(Y, Y, W, W),	/* SGXVEC_USE_VEC4_SWIZ_EXT_YYWW */
	 USEASM_SWIZZLE(W, Y, Z, W),	/* SGXVEC_USE_VEC4_SWIZ_EXT_WYZW */
	 USEASM_SWIZZLE(W, Z, W, Z),	/* SGXVEC_USE_VEC4_SWIZ_EXT_WZWZ */
	 USEASM_SWIZZLE(X, Y, Z, X),	/* SGXVEC_USE_VEC4_SWIZ_EXT_XYZX */
	 USEASM_SWIZZLE(Z, Z, W, W),	/* SGXVEC_USE_VEC4_SWIZ_EXT_ZZWW */
	 USEASM_SWIZZLE(X, W, Z, X),	/* SGXVEC_USE_VEC4_SWIZ_EXT_XWZX */
	 USEASM_SWIZZLE(Y, Y, Y, X),	/* SGXVEC_USE_VEC4_SWIZ_EXT_YYYX */
	 USEASM_SWIZZLE(Y, Y, Y, Z),	/* SGXVEC_USE_VEC4_SWIZ_EXT_YYYZ */
	 USEASM_SWIZZLE(X, Z, Y, W),	/* SGXVEC_USE_VEC4_SWIZ_EXT_XZYW */
	 USEASM_SWIZZLE(X, X, X, Y),	/* SGXVEC_USE_VEC4_SWIZ_EXT_XXXY */
	 USEASM_SWIZZLE(Z, Y, X, W),	/* SGXVEC_USE_VEC4_SWIZ_EXT_ZYXW */
	 USEASM_SWIZZLE(Y, Y, Z, Z),	/* SGXVEC_USE_VEC4_SWIZ_EXT_YYZZ */
	 USEASM_SWIZZLE(Z, Z, Z, Y),	/* SGXVEC_USE_VEC4_SWIZ_EXT_ZZZY */
};

static const IMG_UINT32 g_puScalarStdSwizzle[] = 
{
	 USEASM_SWIZZLE(X, X, X, X),		/* SGXVEC_USE_SCALAR_SWIZ_X */
	 USEASM_SWIZZLE(Y, Y, Y, Y),		/* SGXVEC_USE_SCALAR_SWIZ_Y */
	 USEASM_SWIZZLE(Z, Z, Z, Z),		/* SGXVEC_USE_SCALAR_SWIZ_Z */
	 USEASM_SWIZZLE(W, W, W, W),		/* SGXVEC_USE_SCALAR_SWIZ_W */
	 USEASM_SWIZZLE(0, 0, 0, 0),		/* SGXVEC_USE_SCALAR_SWIZ_0 */
	 USEASM_SWIZZLE(1, 1, 1, 1),		/* SGXVEC_USE_SCALAR_SWIZ_1 */
	 USEASM_SWIZZLE(2, 2, 2, 2),		/* SGXVEC_USE_SCALAR_SWIZ_2 */
	 USEASM_SWIZZLE(HALF, HALF, HALF, HALF),	/* SGXVEC_USE_SCALAR_SWIZ_HALF */
	 USE_UNDEF,
	 USE_UNDEF,
	 USE_UNDEF,
	 USE_UNDEF,
	 USE_UNDEF,
	 USE_UNDEF,
	 USE_UNDEF,
	 USE_UNDEF,
};

static IMG_UINT32 DecodeVec34Swizzle(IMG_UINT32	uSwizzle,
								     IMG_UINT32	uExtFlag,
								     IMG_BOOL	bVec3,
								     IMG_BOOL	bVec4)
{
	if (bVec3 || bVec4)
	{
		if (bVec4)
		{
			if (uExtFlag)
			{
				return g_puVec4ExtSwizzle[uSwizzle];
			}
			else
			{
				return g_puVec4StdSwizzle[uSwizzle];
			}
		}
		else
		{
			if (uExtFlag)
			{
				return g_puVec3ExtSwizzle[uSwizzle];
			}
			else
			{
				return g_puVec3StdSwizzle[uSwizzle];
			}
		}
	}
	else
	{
		if (uExtFlag)
		{
			return USE_UNDEF;
		}
		else
		{
			return g_puScalarStdSwizzle[uSwizzle];
		}	
	}
}

static IMG_VOID DecodeVec34DualIssueInstructionSource(IMG_UINT32				uInst0,
													  IMG_UINT32				uInst1,
													  IMG_BOOL					bVectorOp,
													  IMG_BOOL					bSrc2SlotUsed,
													  PUSE_REGISTER				psSource,
													  DUALISSUEVECTOR_SRCSLOT	eHwArg,
													  PCSGX_CORE_DESC			psTarget)
{
	if (eHwArg == DUALISSUEVECTOR_SRCSLOT_UNIFIEDSTORE)
	{
		IMG_UINT32	uUnifiedStoreNum = (uInst0 & ~SGXVEC_USE0_DVEC_USSNUM_CLRMSK) >> SGXVEC_USE0_DVEC_USSNUM_SHIFT;
		IMG_BOOL	bDoubleRegisters;
		IMG_UINT32	uNumberFieldLength;

		if (bVectorOp)
		{
			bDoubleRegisters = IMG_TRUE;
			uNumberFieldLength = SGXVEC_USE_DVEC_USSRC_VECTOR_REGISTER_NUMBER_BITS;
		}
		else
		{
			bDoubleRegisters = IMG_FALSE;
			uNumberFieldLength = SGXVEC_USE_DVEC_USSRC_SCALAR_REGISTER_NUMBER_BITS;
		}

		DecodeSrc12WithNum(psTarget,
						   bDoubleRegisters,
						   uInst0,
						   uInst1,
						   1 /* uSrc */,
						   IMG_FALSE /* bAllowExtended */,
						   0 /* uExt */,
						   psSource,
						   IMG_FALSE /* bFmtControl */,
						   0 /* uAltFmtFlag */,
						   uNumberFieldLength,
						   uUnifiedStoreNum);

		/*
			If the the "unified store" source is actually set to a GPI register then
			set a special flag to distinguish it from the other sources (which are always
			GPI registers).
		*/
		if (psSource->uType == USEASM_REGTYPE_FPINTERNAL)
		{
			psSource->uFlags |= USEASM_ARGFLAGS_REQUIREMOE;
		}
	}
	else
	{
		IMG_UINT32	uGPIRegNum;

		switch (eHwArg)
		{
			case DUALISSUEVECTOR_SRCSLOT_GPI0: 
			{
				uGPIRegNum = (uInst0 & ~SGXVEC_USE0_DVEC_GPI0NUM_CLRMSK) >> SGXVEC_USE0_DVEC_GPI0NUM_SHIFT; 
				break;
			}
			case DUALISSUEVECTOR_SRCSLOT_GPI1: 
			{
				uGPIRegNum = (uInst0 & ~SGXVEC_USE0_DVEC_GPI1NUM_CLRMSK) >> SGXVEC_USE0_DVEC_GPI1NUM_SHIFT; 
				break;
			}
			case DUALISSUEVECTOR_SRCSLOT_GPI2: 
			{
				if (bSrc2SlotUsed)
				{
					uGPIRegNum = (uInst0 & ~SGXVEC_USE0_DVEC_GPI2NUM_CLRMSK) >> SGXVEC_USE0_DVEC_GPI2NUM_SHIFT; 
				}
				else
				{
					uGPIRegNum = SGXVEC_USE0_DVEC_GPI2NUM_DEFAULT;
				}
				break;
			}
			default: IMG_ABORT();
		}
		psSource->uType = USEASM_REGTYPE_FPINTERNAL;
		psSource->uNumber = uGPIRegNum;
	}
}

#define USEASM_SWIZZLE3(SELX, SELY, SELZ) USEASM_SWIZZLE(SELX, SELY, SELZ, X)

static const IMG_UINT32 g_auDualIssueStdVec3Swizzle[] =
{
	USEASM_SWIZZLE3(X, X, X),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_STD_XXX */
	USEASM_SWIZZLE3(Y, Y, Y),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_STD_YYY */
	USEASM_SWIZZLE3(Z, Z, Z),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_STD_ZZZ */
	USEASM_SWIZZLE3(W, W, W),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_STD_WWW */
	USEASM_SWIZZLE3(X, Y, Z),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_STD_XYZ */
	USEASM_SWIZZLE3(Y, Z, W),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_STD_YZW */
	USEASM_SWIZZLE3(X, X, Y),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_STD_XXY */
	USEASM_SWIZZLE3(X, Y, X),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_STD_XYX */
	USEASM_SWIZZLE3(Y, Y, X),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_STD_YYX */
	USEASM_SWIZZLE3(Y, Y, Z),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_STD_YYZ */
	USEASM_SWIZZLE3(Z, X, Y),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_STD_ZXY */
	USEASM_SWIZZLE3(X, Z, Y),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_STD_XZY */
	USEASM_SWIZZLE3(0, 0, 0),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_STD_000 */
	USEASM_SWIZZLE3(HALF, HALF, HALF),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_STD_HALFHALFHALF */
	USEASM_SWIZZLE3(1, 1, 1),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_STD_111 */
	USEASM_SWIZZLE3(2, 2, 2),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_STD_222 */	
};
static const IMG_UINT32 g_auDualIssueExtVec3Swizzle[] =
{
	USEASM_SWIZZLE3(X, Y, Y),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_EXT_XYY */
	USEASM_SWIZZLE3(Y, X, Y),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_EXT_YXY */
	USEASM_SWIZZLE3(X, X, Z),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_EXT_XXZ */
	USEASM_SWIZZLE3(Y, X, X),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_EXT_YXX */
	USEASM_SWIZZLE3(X, Y, 0),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_EXT_XY0 */
	USEASM_SWIZZLE3(X, 1, 0),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_EXT_X10 */
	USEASM_SWIZZLE3(X, Z, Y),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_EXT_XZY */
	USEASM_SWIZZLE3(Y, Z, X),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_EXT_YZX */
	USEASM_SWIZZLE3(Z, Y, X),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_EXT_ZYX */
	USEASM_SWIZZLE3(Z, Z, Y),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_EXT_ZZY */
	USEASM_SWIZZLE3(X, Y, 1),	/* SGXVEC_USE_DVEC_SWIZ_VEC3_EXT_XY1 */
	USE_UNDEF,					/* SGXVEC_USE_DVEC_SWIZ_VEC3_EXT_RESERVED0 */
	USE_UNDEF,					/* SGXVEC_USE_DVEC_SWIZ_VEC3_EXT_RESERVED1 */
	USE_UNDEF,					/* SGXVEC_USE_DVEC_SWIZ_VEC3_EXT_RESERVED2 */
	USE_UNDEF,					/* SGXVEC_USE_DVEC_SWIZ_VEC3_EXT_RESERVED3 */
	USE_UNDEF,					/* SGXVEC_USE_DVEC_SWIZ_VEC3_EXT_RESERVED4 */
};

#undef USEASM_SWIZZLE3

static const IMG_UINT32 g_auDualIssueStdVec4Swizzle[] =
{
	USEASM_SWIZZLE(X, X, X, X),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_STD_XXXX */
	USEASM_SWIZZLE(Y, Y, Y, Y),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_STD_YYYY */
	USEASM_SWIZZLE(Z, Z, Z, Z),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_STD_ZZZZ */
	USEASM_SWIZZLE(W, W, W, W),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_STD_WWWW */
	USEASM_SWIZZLE(X, Y, Z, W),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_STD_XYZW */
	USEASM_SWIZZLE(Y, Z, W, W),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_STD_YZWW */
	USEASM_SWIZZLE(X, Y, Z, Z),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_STD_XYZZ */
	USEASM_SWIZZLE(X, X, Y, Z),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_STD_XXYZ */
	USEASM_SWIZZLE(X, Y, X, Y),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_STD_XYXY */
	USEASM_SWIZZLE(X, Y, W, Z),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_STD_XYWZ */
	USEASM_SWIZZLE(Z, X, Y, W),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_STD_ZXYW */
	USEASM_SWIZZLE(Z, W, Z, W),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_STD_ZWZW */
	USEASM_SWIZZLE(0, 0, 0, 0),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_STD_0000 */
	USEASM_SWIZZLE(HALF, HALF, HALF, HALF),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_STD_HALFHALFHALFHALF */
	USEASM_SWIZZLE(1, 1, 1, 1),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_STD_1111 */
	USEASM_SWIZZLE(2, 2, 2, 2),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_STD_2222 */
};
static const IMG_UINT32 g_auDualIssueExtVec4Swizzle[] =
{
	USEASM_SWIZZLE(Y, Z, X, W),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_EXT_YZXW */
	USEASM_SWIZZLE(Z, W, X, Y),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_EXT_ZWXY */
	USEASM_SWIZZLE(X, Z, W, Y),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_EXT_XZWY */
	USEASM_SWIZZLE(Y, Y, W, W),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_EXT_YYWW */
	USEASM_SWIZZLE(W, Y, Z, W),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_EXT_WYZW */
	USEASM_SWIZZLE(W, Z, W, Z),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_EXT_WZWZ */
	USEASM_SWIZZLE(X, Y, Z, X),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_EXT_XYZX */
	USEASM_SWIZZLE(Z, Z, W, W),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_EXT_ZZWW */
	USEASM_SWIZZLE(X, W, Z, X),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_EXT_XWZX */
	USEASM_SWIZZLE(Y, Y, Y, X),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_EXT_YYYX */
	USEASM_SWIZZLE(Y, Y, Y, Z),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_EXT_YYYZ */
	USEASM_SWIZZLE(Z, W, Z, W),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_EXT_ZWZW */
	USEASM_SWIZZLE(Y, Z, X, Z),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_EXT_YZXZ */
	USEASM_SWIZZLE(X, X, Y, Y),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_EXT_XXYY */
	USEASM_SWIZZLE(X, Z, W, W),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_EXT_XZWW */
	USEASM_SWIZZLE(X, Y, Z, 1),	/* SGXVEC_USE_DVEC_SWIZ_VEC4_EXT_XYZ1 */
};

static IMG_UINT32 DecodeVec34DualIssueInstructionSourceModifier(IMG_UINT32				uInst0,
																IMG_UINT32				uInst1,
																DUALISSUEVECTOR_SRCSLOT	eSlot,
																IMG_BOOL				bSrc2SlotUsed)
{
	IMG_UINT32	uArgFlags;

	uArgFlags = 0;
	if (eSlot == DUALISSUEVECTOR_SRCSLOT_GPI1)
	{
		if (uInst1 & SGXVEC_USE1_DVEC_GPI1NEG)
		{
			uArgFlags |= USEASM_ARGFLAGS_NEGATE;
		}
	}
	else if (eSlot == DUALISSUEVECTOR_SRCSLOT_UNIFIEDSTORE)
	{
		if (uInst1 & SGXVEC_USE1_DVEC_USNEG)
		{
			uArgFlags |= USEASM_ARGFLAGS_NEGATE;
		}
		if (!bSrc2SlotUsed && (uInst0 & SGXVEC_USE0_DVEC_USABS))
		{
			uArgFlags |= USEASM_ARGFLAGS_ABSOLUTE;
		}
	}

	return uArgFlags;
}

static IMG_UINT32 DecodeVec34DualIssueInstructionSwizzle(IMG_UINT32					uInst0,
													     IMG_UINT32					uInst1,
														 DUALISSUEVECTOR_SRCSLOT	eSwizzleSlot,
													     IMG_BOOL					bOpIsVec4,
													     IMG_BOOL					bSrc2SlotUsed)
{
	IMG_UINT32	uSwizzle;
	IMG_BOOL	bExtendedSwizzle;

	bExtendedSwizzle = IMG_FALSE;
	switch (eSwizzleSlot)
	{
		case DUALISSUEVECTOR_SRCSLOT_UNIFIEDSTORE: 
		{
			uSwizzle = (uInst1 & ~SGXVEC_USE1_DVEC_USSWIZ_CLRMSK) >> SGXVEC_USE1_DVEC_USSWIZ_SHIFT;
			if (!bSrc2SlotUsed && (uInst0 & SGXVEC_USE0_DVEC_USSWIZEXT) != 0)
			{
				bExtendedSwizzle = IMG_TRUE;
			}
			break;
		}
		case DUALISSUEVECTOR_SRCSLOT_GPI0: 
		{
			uSwizzle = (uInst1 & ~SGXVEC_USE1_DVEC_GPI0SWIZ_CLRMSK) >> SGXVEC_USE1_DVEC_GPI0SWIZ_SHIFT;
			break;
		}
		case DUALISSUEVECTOR_SRCSLOT_GPI1: 
		{
			uSwizzle = (uInst1 & ~SGXVEC_USE1_DVEC_GPI1SWIZ_CLRMSK) >> SGXVEC_USE1_DVEC_GPI1SWIZ_SHIFT;
			if ((uInst1 & SGXVEC_USE1_DVEC_GPI1SWIZEXT) != 0)
			{
				bExtendedSwizzle = IMG_TRUE;
			}
			break;
		}
		case DUALISSUEVECTOR_SRCSLOT_GPI2:
		{
			return USEASM_SWIZZLE(X, Y, Z, W);
		}
		default: IMG_ABORT();
	}

	if (bOpIsVec4)
	{
		if (bExtendedSwizzle)
		{
			return g_auDualIssueExtVec4Swizzle[uSwizzle];
		}
		else
		{
			return g_auDualIssueStdVec4Swizzle[uSwizzle];
		}
	}
	else
	{
		if (bExtendedSwizzle)
		{
			return g_auDualIssueExtVec3Swizzle[uSwizzle];
		}
		else
		{
			return g_auDualIssueStdVec3Swizzle[uSwizzle];
		}
	}
}

static USEDISASM_ERROR DecodeVec34DualIssueOp2(IMG_UINT32		uInst0,
										       IMG_UINT32		uInst1,
											   USEASM_OPCODE*	peOpcode,
											   IMG_PUINT32		puOp2SrcCount,
											   IMG_PBOOL		pbOp2Vec,
											   IMG_PBOOL		pbOp2VecResult,
											   IMG_BOOL			bVec4)
{
	IMG_UINT32	uOp2 = 
		(uInst0 & ~SGXVEC_USE0_DVEC_OP2_CLRMSK) >> SGXVEC_USE0_DVEC_OP2_SHIFT;

	*pbOp2Vec = IMG_FALSE;
	*pbOp2VecResult = IMG_FALSE;
	if (uInst1 & SGXVEC_USE1_DVEC_OP2EXT)
	{
		switch (uOp2)
		{
			case SGXVEC_USE0_DVEC_OP2_EXT_FMAD:		*peOpcode = USEASM_OP_FMAD; *puOp2SrcCount = 3; break;
			case SGXVEC_USE0_DVEC_OP2_EXT_FADD:		*peOpcode = USEASM_OP_FADD; *puOp2SrcCount = 2; break;
			case SGXVEC_USE0_DVEC_OP2_EXT_FMUL:		*peOpcode = USEASM_OP_FMUL; *puOp2SrcCount = 2; break;
			case SGXVEC_USE0_DVEC_OP2_EXT_FSUBFLR:	*peOpcode = USEASM_OP_FSUBFLR; *puOp2SrcCount = 2; break;
			case SGXVEC_USE0_DVEC_OP2_EXT_FEXP:		*peOpcode = USEASM_OP_FEXP; *puOp2SrcCount = 1; break;
			case SGXVEC_USE0_DVEC_OP2_EXT_FLOG:		*peOpcode = USEASM_OP_FLOG; *puOp2SrcCount = 1; break;
			case SGXVEC_USE0_DVEC_OP2_EXT_RESERVED0: 
			case SGXVEC_USE0_DVEC_OP2_EXT_RESERVED1:
			{
				return USEDISASM_ERROR_INVALID_DVEC_OP2;
			}
			default: IMG_ABORT();
		}
	}
	else
	{
		switch (uOp2)
		{
			case SGXVEC_USE0_DVEC_OP2_STD_RESERVED: return USEDISASM_ERROR_INVALID_DVEC_OP2;
			case SGXVEC_USE0_DVEC_OP2_STD_VDP: 
			{
				*peOpcode = bVec4 ? USEASM_OP_VDP4 : USEASM_OP_VDP3; 
				*puOp2SrcCount = 2; 
				*pbOp2Vec = IMG_TRUE; 
				break;
			}
			case SGXVEC_USE0_DVEC_OP2_STD_VSSQ: 
			{
				*peOpcode = bVec4 ? USEASM_OP_VSSQ4 : USEASM_OP_VSSQ3; 
				*puOp2SrcCount = 1; 
				*pbOp2Vec = IMG_TRUE; 
				break;
			}
			case SGXVEC_USE0_DVEC_OP2_STD_VMUL: 
			{
				*peOpcode = bVec4 ? USEASM_OP_VMUL4 : USEASM_OP_VMUL3; 
				*puOp2SrcCount = 2; 
				*pbOp2Vec = IMG_TRUE; 
				*pbOp2VecResult = IMG_TRUE;
				break;
			}
			case SGXVEC_USE0_DVEC_OP2_STD_VADD: 
			{
				*peOpcode = bVec4 ? USEASM_OP_VADD4 : USEASM_OP_VADD3; 
				*puOp2SrcCount = 2; 
				*pbOp2Vec = IMG_TRUE; 
				*pbOp2VecResult = IMG_TRUE;
				break;
			}
			case SGXVEC_USE0_DVEC_OP2_STD_VMOV: 
			{
				*peOpcode = bVec4 ? USEASM_OP_VMOV4 : USEASM_OP_VMOV3; 
				*puOp2SrcCount = 1; 
				*pbOp2Vec = IMG_TRUE; 
				*pbOp2VecResult = IMG_TRUE;
				break;
			}
			case SGXVEC_USE0_DVEC_OP2_STD_FRSQ: *peOpcode = USEASM_OP_FRSQ; *puOp2SrcCount = 1; break;
			case SGXVEC_USE0_DVEC_OP2_STD_FRCP: *peOpcode = USEASM_OP_FRCP; *puOp2SrcCount = 1; break;
			default: IMG_ABORT();
		}
	}

	return USEDISASM_OK;
}

static IMG_VOID DecodeVec34DualIssueInstructionDest(PCSGX_CORE_DESC	psTarget,
													IMG_UINT32		uInst0,
													IMG_UINT32		uInst1,
													PUSE_REGISTER	psDest,
													IMG_BOOL		bUnifiedStoreDest,
													IMG_BOOL		bVectorResult)
{
	if (bUnifiedStoreDest)
	{
		if (bVectorResult)
		{
			IMG_UINT32	uDestNum;

			uDestNum = (uInst0 & ~EURASIA_USE0_DST_CLRMSK) >> EURASIA_USE0_DST_SHIFT;
			DecodeDestWithNum(psTarget,
							  IMG_TRUE /* bDoubleRegisters */,
							  uInst1,
							  IMG_FALSE /* bAllowExtended */,
							  psDest,
							  IMG_FALSE /* bFmtControl */,
							  0 /* uAltFmtFlag */,
							  SGXVEC_USE_DVEC_USDEST_VECTOR_REGISTER_NUMBER_BITS,
							  uDestNum);
		}
		else
		{
			DecodeDest(psTarget,
					   uInst0,
					   uInst1,
					   IMG_FALSE /* bAllowExtended */,
					   psDest,
					   USEDIS_FORMAT_CONTROL_STATE_OFF /* eFmtControlState */,
					   0 /* uAltFmtFlag */);
		}

		/*
			If the the "unified store" destination is actually set to a GPI register then
			set a special flag to distinguish it from the other destination (which is always
			a GPI register).
		*/
		if (psDest->uType == USEASM_REGTYPE_FPINTERNAL)
		{
			psDest->uFlags |= USEASM_ARGFLAGS_REQUIREMOE;
		}
	}
	else
	{
		psDest->uType = USEASM_REGTYPE_FPINTERNAL;
		psDest->uNumber = (uInst0 & ~SGXVEC_USE0_DVEC_GPIDNUM_CLRMSK) >> SGXVEC_USE0_DVEC_GPIDNUM_SHIFT;
	}
}


static IMG_UINT32 DecodeVec34DualIssueWriteMask(IMG_UINT32	uInst0,
											    IMG_UINT32	uInst1,
											    IMG_BOOL	bVec4)
{
	IMG_UINT32	uWriteMask = (uInst0 & ~SGXVEC_USE0_DVEC_WMSK_CLRMSK) >> SGXVEC_USE0_DVEC_WMSK_SHIFT;
	IMG_UINT32	uRepeatMask;

	uRepeatMask =
		((uWriteMask & 1) ? USEREG_MASK_X : 0) |
		((uWriteMask & 2) ? USEREG_MASK_Y : 0) |
		((uWriteMask & 4) ? USEREG_MASK_Z : 0) |
		((bVec4 && (uInst1 & SGXVEC_USE1_DVEC4_WMSKW)) ? USEREG_MASK_W : 0);
	return uRepeatMask;
}

static const USEASM_PRED g_aeShortVectorPred[] =
	{
		USEASM_PRED_NONE,	/* SGXVEC_USE_VEC_SVPRED_NONE */
		USEASM_PRED_P0,		/* SGXVEC_USE_VEC_SVPRED_P0 */
		USEASM_PRED_NEGP0,	/* SGXVEC_USE_VEC_SVPRED_NOTP0 */
		USEASM_PRED_PN,		/* SGXVEC_USE_VEC_SVPRED_PERCHAN */
	};

static const USEASM_PRED g_aeExtVectorPred[] = 
	{
		USEASM_PRED_NONE,	/* SGXVEC_USE_VEC_EVPRED_NONE */
		USEASM_PRED_P0,		/* SGXVEC_USE_VEC_EVPRED_P0 */
		USEASM_PRED_P1,		/* SGXVEC_USE_VEC_EVPRED_P1 */
		USEASM_PRED_P2,		/* SGXVEC_USE_VEC_EVPRED_P2 */
		USEASM_PRED_NEGP0,	/* SGXVEC_USE_VEC_EVPRED_NOTP0 */
		USEASM_PRED_NEGP1,	/* SGXVEC_USE_VEC_EVPRED_NOTP1 */
		USEASM_PRED_NEGP2,	/* SGXVEC_USE_VEC_EVPRED_NOTP2 */
		USEASM_PRED_PN,		/* SGXVEC_USE_VEC_EVPRED_PERCHAN */
	};

static IMG_VOID AddExtraVMADVDPSources(PUSE_INST		psInst,
									   PUSE_REGISTER	psNextArg)
{
	/*
		Add an extra source representing the clipplane mode.
	*/
	if (psInst->uOpcode == USEASM_OP_VDP3 || psInst->uOpcode == USEASM_OP_VDP4)
	{
		SetupIntSrcSel(psNextArg, USEASM_INTSRCSEL_NONE);
		psNextArg++;
	}

	/*
		Add an extra source for the MOE increment mode.
	*/
	if (psInst->uOpcode == USEASM_OP_VDP3 || 
		psInst->uOpcode == USEASM_OP_VDP4 ||
		psInst->uOpcode == USEASM_OP_VMAD3 ||
		psInst->uOpcode == USEASM_OP_VMAD4)
	{
		SetupIntSrcSel(psNextArg, USEASM_INTSRCSEL_NONE);
		psNextArg++;
	}
}

static IMG_UINT32 GetVec34USWriteMask(PUSE_INST	psUSWriterInst,
									  IMG_BOOL	bUSWriterHasVectorResult,
									  IMG_BOOL	bVec4,
									  IMG_BOOL	bF16)
{
	if (psUSWriterInst->asArg[0].uType == USEASM_REGTYPE_FPINTERNAL)
	{
		return bVec4 ? USEREG_MASK_XYZW : USEREG_MASK_XYZ;
	}
	else
	{
		if (bUSWriterHasVectorResult)
		{
			if (bF16)
			{
				/* Write four 16-bit channels. */
				return USEREG_MASK_XYZW;
			}
			else
			{
				/* Write two 32-bit channels. */
				return USEREG_MASK_XY;
			}
		}
		else
		{
			/*
				Write a single 32-bit register.
			*/
			return bF16 ? USEREG_MASK_XY : USEREG_MASK_X;
		}
	}
}

static USEDISASM_ERROR DecodeVec34DualIssueOp1(IMG_UINT32		uInst1,
											   IMG_BOOL			bVec4,
											   USEASM_OPCODE*	peOp1,
											   IMG_PUINT32		puOp1SrcCount,
											   IMG_PBOOL		pbOp1Vector,
											   IMG_PBOOL		pbOp1VectorResult)
{
	IMG_BOOL	bExtendedOp1;
	IMG_UINT32	uOp1 = (uInst1 & ~SGXVEC_USE1_DVEC_OP1_CLRMSK) >> SGXVEC_USE1_DVEC_OP1_SHIFT;

	/*
		Decode extended opcode flag.
	*/
	if (!bVec4 && (uInst1 & SGXVEC_USE1_DVEC3_OP1EXT))
	{
		bExtendedOp1 = IMG_TRUE;
	}
	else
	{
		bExtendedOp1 = IMG_FALSE;
	}

	*pbOp1Vector = IMG_FALSE;
	*pbOp1VectorResult = IMG_FALSE;
	if (bExtendedOp1)
	{
		switch (uOp1)
		{
			case SGXVEC_USE1_DVEC_OP1_EXT_FMAD:		*peOp1 = USEASM_OP_FMAD; *puOp1SrcCount = 3; break;
			case SGXVEC_USE1_DVEC_OP1_EXT_FADD:		*peOp1 = USEASM_OP_FADD; *puOp1SrcCount = 2; break;
			case SGXVEC_USE1_DVEC_OP1_EXT_FMUL:		*peOp1 = USEASM_OP_FMUL; *puOp1SrcCount = 2; break;
			case SGXVEC_USE1_DVEC_OP1_EXT_FSUBFLR:	*peOp1 = USEASM_OP_FSUBFLR; *puOp1SrcCount = 2; break;
			case SGXVEC_USE1_DVEC_OP1_EXT_FEXP:		*peOp1 = USEASM_OP_FEXP; *puOp1SrcCount = 1; break;
			case SGXVEC_USE1_DVEC_OP1_EXT_FLOG:		*peOp1 = USEASM_OP_FLOG; *puOp1SrcCount = 1; break;
			case SGXVEC_USE1_DVEC_OP1_EXT_RESERVED0: 
			case SGXVEC_USE1_DVEC_OP1_EXT_RESERVED1:
			{
				return USEDISASM_ERROR_INVALID_DVEC_OP1;
			}
			default: IMG_ABORT();
		}
	}
	else
	{
		switch (uOp1)
		{
			case SGXVEC_USE1_DVEC_OP1_STD_VMAD: 
			{
				*peOp1 = bVec4 ? USEASM_OP_VMAD4 : USEASM_OP_VMAD3;
				*puOp1SrcCount = 3; 
				*pbOp1Vector = IMG_TRUE;
				*pbOp1VectorResult = IMG_TRUE;
				break;
			}
			case SGXVEC_USE1_DVEC_OP1_STD_VDP: 
			{
				*peOp1 = bVec4 ? USEASM_OP_VDP4 : USEASM_OP_VDP3;
				*puOp1SrcCount = 2;
				*pbOp1Vector = IMG_TRUE;
				break;
			}
			case SGXVEC_USE1_DVEC_OP1_STD_VSSQ: 
			{
				*peOp1 = bVec4 ? USEASM_OP_VSSQ4 : USEASM_OP_VSSQ3;
				*puOp1SrcCount = 1;  
				*pbOp1Vector = IMG_TRUE;
				break;
			}
			case SGXVEC_USE1_DVEC_OP1_STD_VMUL: 
			{
				*peOp1 = bVec4 ? USEASM_OP_VMUL4 : USEASM_OP_VMUL3;
				*puOp1SrcCount = 2; 
				*pbOp1Vector = IMG_TRUE;
				*pbOp1VectorResult = IMG_TRUE;
				break;
			}
			case SGXVEC_USE1_DVEC_OP1_STD_VADD: 
			{
				*peOp1 = bVec4 ? USEASM_OP_VADD4 : USEASM_OP_VADD3;
				*puOp1SrcCount = 2; 
				*pbOp1Vector = IMG_TRUE;
				*pbOp1VectorResult = IMG_TRUE;
				break;
			}
			case SGXVEC_USE1_DVEC_OP1_STD_VMOV: 
			{
				*peOp1 = bVec4 ? USEASM_OP_VMOV4 : USEASM_OP_VMOV3;
				*puOp1SrcCount = 1; 
				*pbOp1Vector = IMG_TRUE;
				*pbOp1VectorResult = IMG_TRUE;
				break;
			}
			case SGXVEC_USE1_DVEC_OP1_STD_FRSQ: 
			{
				*peOp1 = USEASM_OP_FRSQ;
				*puOp1SrcCount = 1; 
				*pbOp1Vector = IMG_FALSE;
				break;
			}
			case SGXVEC_USE1_DVEC_OP1_STD_FRCP: 
			{
				*peOp1 = USEASM_OP_FRCP;
				*puOp1SrcCount = 1; 
				*pbOp1Vector = IMG_FALSE;
				break;
			}
			default: IMG_ABORT();
		}
	}

	return USEDISASM_OK;
}

static IMG_UINT32 FindUSDualIssueSource(IMG_UINT32						uOpSrcCount,
										DUALISSUEVECTOR_SRCSLOT const*	peMap)
{
	IMG_UINT32	uSrcIdx;

	for (uSrcIdx = 0; uSrcIdx < uOpSrcCount; uSrcIdx++)
	{
		if (peMap[uSrcIdx] == DUALISSUEVECTOR_SRCSLOT_UNIFIEDSTORE)
		{
			return uSrcIdx;
		}
	}
	return USE_UNDEF;
}

IMG_INTERNAL 
USEDISASM_ERROR IMG_CALLCONV UseGetVecDualIssueUSSource(IMG_UINT32	uInst0,
													    IMG_UINT32	uInst1,
														IMG_PUINT32	puOp1USSrcIdx,
														IMG_PUINT32	puOp2USSrcIdx)
{
	IMG_UINT32					uSrcCfg = (uInst0 & ~SGXVEC_USE0_DVEC_SRCCFG_CLRMSK) >> SGXVEC_USE0_DVEC_SRCCFG_SHIFT;
	USEASM_OPCODE				eOpcode1, eOpcode2;
	IMG_UINT32					uOp1SrcCount, uOp2SrcCount;
	IMG_BOOL					bOp1Vector, bOp2Vector;
	IMG_BOOL					bOp1VectorResult, bOp2VectorResult;
	PCDUALISSUEVECTOR_SRCSLOT	pePrimaryMap;
	PCDUALISSUEVECTOR_SRCSLOT	peSecondaryMap;
	USEDISASM_ERROR				eRet;
	IMG_UINT32					uOpcode;
	IMG_BOOL					bVec4;
	
	uOpcode = (uInst1 & ~EURASIA_USE1_OP_CLRMSK) >> EURASIA_USE1_OP_SHIFT;
	if (uOpcode == SGXVEC_USE1_OP_DVEC3)
	{
		bVec4 = IMG_FALSE;
	}
	else if (uOpcode == SGXVEC_USE1_OP_DVEC4)
	{
		bVec4 = IMG_TRUE;
	}
	else
	{
		return USEDISASM_ERROR_INVALID_OPCODE;
	}

	/*
		Decode primary opcode.
	*/
	eRet = 
		DecodeVec34DualIssueOp1(uInst1,
								bVec4,
								&eOpcode1,
								&uOp1SrcCount,
								&bOp1Vector,
								&bOp1VectorResult);
	if (eRet != USEDISASM_OK)
	{
		return eRet;
	}

	/*
		Decode the secondary operation.
	*/
	eRet = 
		DecodeVec34DualIssueOp2(uInst0,
							    uInst1,
								&eOpcode2,
								&uOp2SrcCount,
								&bOp2Vector,
								&bOp2VectorResult,
								bVec4);
	if (eRet != USEDISASM_OK)
	{
		return eRet;
	}

	/*
		Get the mapping of instruction sources.
	*/
	pePrimaryMap = g_aapeDualIssueVectorPrimaryMap[uOp1SrcCount - 1][uSrcCfg];
	peSecondaryMap = g_aaapeDualIssueVectorSecondaryMap[uOp1SrcCount - 1][uOp2SrcCount - 1][uSrcCfg];
	if (pePrimaryMap == NULL || peSecondaryMap == NULL)
	{
		return USEDISASM_ERROR_INVALID_DVEC_SRCCFG;
	}

	/*
		Return the location of the unified store source.
	*/
	*puOp1USSrcIdx = FindUSDualIssueSource(uOp1SrcCount, pePrimaryMap);
	*puOp2USSrcIdx = FindUSDualIssueSource(uOp2SrcCount, peSecondaryMap);

	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeVec34DualIssueInstruction(PCSGX_CORE_DESC	psTarget,
													   IMG_BOOL			bVec4,
													   IMG_UINT32		uInst0,
													   IMG_UINT32		uInst1,
													   PUSE_INST		psInst,
													   PUSE_INST		psCoInst)
{
	IMG_UINT32						uSrcCfg = (uInst0 & ~SGXVEC_USE0_DVEC_SRCCFG_CLRMSK) >> SGXVEC_USE0_DVEC_SRCCFG_SHIFT;
	IMG_UINT32						uOp1SrcCount;
	IMG_BOOL						bPrimaryUS;
	IMG_UINT32						uSrc;
	IMG_BOOL						bOp1Vector, bOp1Vec3, bOp1Vec4, bOp1VectorResult;
	IMG_UINT32						uOp2SrcCount;
	IMG_BOOL						bOp2Vector, bOp2Vec3, bOp2Vec4, bOp2VectorResult;
	IMG_BOOL						bSrc2SlotUsed;
	IMG_BOOL						bF16;
	IMG_UINT32						uSVPred;
	DUALISSUEVECTOR_SRCSLOT const*	pePrimaryMap;
	DUALISSUEVECTOR_SRCSLOT const*	peSecondaryMap;
	PUSE_REGISTER					psNextArg;
	USEDISASM_ERROR					eRet;
	IMG_UINT32						uOp1WriteMask, uOp2WriteMask;

	UseAsmInitInst(psInst);
	psInst->uFlags1 |= USEASM_OPFLAGS1_MAINISSUE;

	UseAsmInitInst(psCoInst);
	psCoInst->uFlags2 |= USEASM_OPFLAGS2_COISSUE;

	/*
		Do either of the opcodes write GPI registers.
	*/
	bPrimaryUS = IMG_FALSE;
	if (uInst1 & SGXVEC_USE1_DVEC_DCFG_PRIWRITETOUS)
	{
		bPrimaryUS = IMG_TRUE;
	}

	/*
		Decode the F16 flag.
	*/
	bF16 = IMG_FALSE;
	if (uInst1 & SGXVEC_USE1_DVEC_F16)
	{
		bF16 = IMG_TRUE;
	}

	/*
		Decode vector predicate
	*/
	uSVPred = (uInst1 & ~SGXVEC_USE1_DVEC_SVPRED_CLRMSK) >> SGXVEC_USE1_DVEC_SVPRED_SHIFT;
	psInst->uFlags1 |= (g_aeShortVectorPred[uSVPred] << USEASM_OPFLAGS1_PRED_SHIFT);

	/*
		Always add the NOSCHED flag to the decoded instruction as the hardware treats it
		as implicitly set.
	*/
	psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;

	/*
		Decode primary opcode.
	*/
	eRet = 
		DecodeVec34DualIssueOp1(uInst1,
								bVec4,
								&psInst->uOpcode,
								&uOp1SrcCount,
								&bOp1Vector,
								&bOp1VectorResult);
	if (eRet != USEDISASM_OK)
	{
		return eRet;
	}

	/*
		Decode the secondary operation.
	*/
	eRet = 
		DecodeVec34DualIssueOp2(uInst0,
							    uInst1,
								&psCoInst->uOpcode,
								&uOp2SrcCount,
								&bOp2Vector,
								&bOp2VectorResult,
								bVec4);
	if (eRet != USEDISASM_OK)
	{
		return eRet;
	}

	/*
		How many sources are used in total.
	*/
	bSrc2SlotUsed = IMG_FALSE;
	if (uOp1SrcCount >= 2)
	{
		bSrc2SlotUsed = IMG_TRUE;
	}

	/*
		What's the vector length for the first operation.
	*/
	bOp1Vec3 = bOp1Vec4 = IMG_FALSE;
	if (bOp1Vector)
	{
		if (bVec4)
		{
			bOp1Vec4 = IMG_TRUE;
		}
		else
		{
			bOp1Vec3 = IMG_TRUE;
		}
	}

	/*
		What's the vector length for the second operation.
	*/
	bOp2Vec3 = bOp2Vec4 = IMG_FALSE;
	if (bOp2Vector)
	{
		if (bVec4)
		{
			bOp2Vec4 = IMG_TRUE;
		}
		else
		{
			bOp2Vec3 = IMG_TRUE;
		}
	}

	/*
		Decode instruction flags.	
	*/
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (bF16)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_MOVC_FMTF16;
	}

	/*
		Disassemble primary destination register.
	*/
	DecodeVec34DualIssueInstructionDest(psTarget,
										uInst0,
										uInst1,
										&psInst->asArg[0],
										bPrimaryUS,
										bOp1VectorResult);

	/*
		Disassemble primary write mask.
	*/
	if (!bPrimaryUS)
	{
		uOp1WriteMask = DecodeVec34DualIssueWriteMask(uInst0, uInst1, bVec4);
	}
	else
	{
		uOp1WriteMask = GetVec34USWriteMask(psInst, bOp1VectorResult, bVec4, bF16);
	}
	psInst->uFlags1 |= (uOp1WriteMask << USEASM_OPFLAGS1_MASK_SHIFT);

	/*
		Get the mapping of instruction sources.
	*/
	pePrimaryMap = g_aapeDualIssueVectorPrimaryMap[uOp1SrcCount - 1][uSrcCfg];
	peSecondaryMap = g_aaapeDualIssueVectorSecondaryMap[uOp1SrcCount - 1][uOp2SrcCount - 1][uSrcCfg];
	if (pePrimaryMap == NULL || peSecondaryMap == NULL)
	{
		return USEDISASM_ERROR_INVALID_DVEC_SRCCFG;
	}

	/*
		Disassemble primary sources.
	*/
	psNextArg = &psInst->asArg[1];
	for (uSrc = 0; uSrc < uOp1SrcCount; uSrc++)
	{
		IMG_UINT32				uSwizzle;
		DUALISSUEVECTOR_SRCSLOT	eSlot = pePrimaryMap[uSrc];

		/*
			Disassemble source register.
		*/
		DecodeVec34DualIssueInstructionSource(uInst0,
											  uInst1,
											  bOp1Vector,
											  bSrc2SlotUsed,
											  psNextArg,
											  eSlot,
											  psTarget);

		/*
			Disassemble source modifiers.
		*/
		psNextArg->uFlags |=
			DecodeVec34DualIssueInstructionSourceModifier(uInst0,
														  uInst1,
														  eSlot,
														  bSrc2SlotUsed);

		/*
			Disassemble source swizzle.
		*/
		uSwizzle =
			DecodeVec34DualIssueInstructionSwizzle(uInst0,
												   uInst1,
												   eSlot,
												   bVec4,
												   bSrc2SlotUsed);
		if (bOp1Vec3 || bOp1Vec4)
		{
			psNextArg++;
			SetupSwizzleSource(psNextArg, uSwizzle);
		}
		else
		{
			IMG_UINT32	uXSel = uSwizzle & USEASM_SWIZZLE_VALUE_MASK;

			psNextArg->uFlags |= (uXSel << USEASM_ARGFLAGS_COMP_SHIFT);
		}
		psNextArg++;
	}

	/*
		Setup some extra sources if the OP1 instruction is VMAD3/VMAD4 or VDP3/VDP4 that
		represent parameters that only apply to the single-issue form.
	*/
	AddExtraVMADVDPSources(psInst, psNextArg);

	/*
		Disassemble secondary destination.
	*/
	DecodeVec34DualIssueInstructionDest(psTarget,
										uInst0,
										uInst1,
										&psCoInst->asArg[0],
										!bPrimaryUS,
										bOp2VectorResult);

	/*
		Decode secondary write mask.
	*/
	if (bPrimaryUS)
	{
		uOp2WriteMask = DecodeVec34DualIssueWriteMask(uInst0, uInst1, bVec4);
	}
	else
	{
		uOp2WriteMask = GetVec34USWriteMask(psCoInst, bOp2VectorResult, bVec4, bF16);
	}
	psCoInst->uFlags1 |= (uOp2WriteMask << USEASM_OPFLAGS1_MASK_SHIFT);

	/*
		Disassemble secondary sources.
	*/
	psNextArg = &psCoInst->asArg[1];
	for (uSrc = 0; uSrc < uOp2SrcCount; uSrc++)
	{
		IMG_UINT32				uSwizzle;
		DUALISSUEVECTOR_SRCSLOT	eSlot = peSecondaryMap[uSrc];

		/*
			Decode source register bank and number.
		*/
		DecodeVec34DualIssueInstructionSource(uInst0, uInst1, bOp2Vector, bSrc2SlotUsed, psNextArg, eSlot, psTarget);
		
		/*
			Disassemble source swizzle.
		*/
		uSwizzle = 
			DecodeVec34DualIssueInstructionSwizzle(uInst0,
												   uInst1,
												   eSlot,
											       bVec4,
											       bSrc2SlotUsed);
		/*
			Disassemble source modifiers.
		*/
		psNextArg->uFlags |=
			DecodeVec34DualIssueInstructionSourceModifier(uInst0,
														  uInst1,
														  eSlot,
														  bSrc2SlotUsed);
		
		/*
			Add the swizzle for this source either as the next argument or
			as a component select for scalar instructions.
		*/
		if (bOp2Vector)
		{
			psNextArg++;
			SetupSwizzleSource(psNextArg, uSwizzle);
		}
		else
		{
			IMG_UINT32	uXSel = uSwizzle & USEASM_SWIZZLE_VALUE_MASK;

			psNextArg->uFlags |= (uXSel << USEASM_ARGFLAGS_COMP_SHIFT);
		}
		psNextArg++;
	}

	/*
		Setup some extra sources if the OP2 instruction is VMAD3/VMAD4 or VDP3/VDP4 that
		represent parameters that only apply to the single-issue form.
	*/
	AddExtraVMADVDPSources(psCoInst, psNextArg);

	return USEDISASM_OK;
}

static IMG_BOOL DecodeVec34SingleIssueInstruction(PCSGX_CORE_DESC psTarget,
												  IMG_UINT32     uInst0,
												  IMG_UINT32     uInst1,
												  PUSE_INST		 psInst)
{
	static const USEASM_OPCODE aeOp2[] =
	{
		USEASM_OP_VDP3,		/* SGXVEC_USE1_SVEC_OP2_DP3 */
		USEASM_OP_VDP4,		/* SGXVEC_USE1_SVEC_OP2_DP4 */
		USEASM_OP_VMAD3,	/* SGXVEC_USE1_SVEC_OP2_VMAD3 */
		USEASM_OP_VMAD4,	/* SGXVEC_USE1_SVEC_OP2_VMAD4 */
	};
	static const USEASM_INTSRCSEL aeIncrMode[] =
	{
		USEASM_INTSRCSEL_INCREMENTUS,	/* SGXVEC_USE1_SVEC_INCCTL_USONLY */
		USEASM_INTSRCSEL_INCREMENTGPI,	/* SGXVEC_USE1_SVEC_INCCTL_GPIONLY */
		USEASM_INTSRCSEL_INCREMENTBOTH,	/* SGXVEC_USE1_SVEC_INCCTL_BOTH */
		USEASM_INTSRCSEL_INCREMENTMOE,	/* SGXVEC_USE1_SVEC_INCCTL_MOE */
	};

	IMG_UINT32		uOp2 = (uInst1 & ~SGXVEC_USE1_SVEC_OP2_CLRMSK) >> SGXVEC_USE1_SVEC_OP2_SHIFT;
	IMG_UINT32		uWriteMask = (uInst1 & ~SGXVEC_USE1_SVEC_WMSK_CLRMSK) >> SGXVEC_USE1_SVEC_WMSK_SHIFT;
	IMG_BOOL		bVec4;
	IMG_UINT32		uGPI0HwSwiz;
	IMG_UINT32		uGPI0Swiz;
	IMG_UINT32		uRepeatCount = ((uInst1 & ~SGXVEC_USE1_SVEC_RCNT_CLRMSK) >> SGXVEC_USE1_SVEC_RCNT_SHIFT) + 1;
	IMG_BOOL		bGPI0SwizExtendedFlag;
	IMG_UINT32		uUSSourceNum;
	IMG_BOOL		bVMAD;
	IMG_UINT32		uDestNum;
	USEASM_PRED		ePredicate;
	PUSE_REGISTER	psNextArg = psInst->asArg;

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);
	
	/*
		vec3 or vec4 swizzles?
	*/
	bVec4 = IMG_FALSE;
	if (uOp2 == SGXVEC_USE1_SVEC_OP2_DP4 || uOp2 == SGXVEC_USE1_SVEC_OP2_VMAD4)
	{
		bVec4 = IMG_TRUE;
	}

	/*
		MAD instruction?
	*/
	bVMAD = IMG_FALSE;
	if (uOp2 == SGXVEC_USE1_SVEC_OP2_VMAD3 || uOp2 == SGXVEC_USE1_SVEC_OP2_VMAD4)
	{
		bVMAD = IMG_TRUE;
	}

	/*
		Decode vector predicate
	*/
	ePredicate = 
		g_aeExtVectorPred[(uInst1 & ~SGXVEC_USE1_SVEC_EVPRED_CLRMSK) >> SGXVEC_USE1_SVEC_EVPRED_SHIFT];
	psInst->uFlags1 |= (ePredicate << USEASM_OPFLAGS1_PRED_SHIFT);

	/*
		Decode opcode.
	*/
	psInst->uOpcode = aeOp2[uOp2];

	/*
		Disassemble instruction flags.
	*/
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	psInst->uFlags1 |= (uRepeatCount << USEASM_OPFLAGS1_REPEAT_SHIFT);
	if (uInst1 & SGXVEC_USE1_SVEC_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	if (uInst1 & EURASIA_USE1_END)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}

	/*
		Disassemble the destination register.
	*/
	uDestNum = (uInst0 & ~SGXVEC_USE0_SVEC_DST_CLRMSK) >> SGXVEC_USE0_SVEC_DST_SHIFT;
	DecodeDestWithNum(psTarget,
					  IMG_TRUE /* bDoubleRegisters */,
					  uInst1,
					  IMG_TRUE /* bAllowExtended */,
					  psNextArg,
					  IMG_FALSE /* bFmtControl */,
					  0 /* uAltFmtFlag */,
					  SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
					  uDestNum);
	psNextArg++;

	/*
		Decode the write mask.
	*/
	psInst->uFlags1 |= (uWriteMask << USEASM_OPFLAGS1_MASK_SHIFT);

	/*
		Decode the first source.
	*/
	uUSSourceNum = (uInst0 & ~SGXVEC_USE0_SVEC_USNUM_CLRMSK) >> SGXVEC_USE0_SVEC_USNUM_SHIFT;
	DecodeSrc12WithNum(psTarget,
					   IMG_TRUE /* bDoubleRegisters */,
					   uInst0,
					   uInst1,
					   1 /* uSrc */,
					   IMG_TRUE /* bAllowExtended */,
					   SGXVEC_USE1_SVEC_USSBEXT /* uExt */,
					   psNextArg,
					   IMG_FALSE /* bFmtControl */,
					   0 /* uAltFmtFlag */,
					   SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
					   uUSSourceNum);
	if (uInst1 & SGXVEC_USE1_SVEC_USSNEG)
	{
		psInst->asArg[1].uFlags |= USEASM_ARGFLAGS_NEGATE;
	}
	if (uInst1 & SGXVEC_USE1_SVEC_USSABS)
	{
		psInst->asArg[1].uFlags |= USEASM_ARGFLAGS_ABSOLUTE;
	}
	psNextArg++;

	/*
		Decode the first source swizzle.
	*/
	if (bVMAD)
	{
		IMG_UINT32	uUSSwiz;
		IMG_BOOL	bUSSwizExtFlag;
		IMG_UINT32	uUSEASMSwizzle;

		uUSSwiz = (uInst0 & ~SGXVEC_USE0_SVEC_VMAD34_USSWIZ_CLRMSK) >> SGXVEC_USE0_SVEC_VMAD34_USSWIZ_SHIFT;
		bUSSwizExtFlag = IMG_FALSE;
		if (uInst0 & SGXVEC_USE0_SVEC_VMAD34_USSWIZEXT)
		{
			bUSSwizExtFlag = IMG_TRUE;
		}

		uUSEASMSwizzle = DecodeVec34Swizzle(uUSSwiz, bUSSwizExtFlag, !bVec4, bVec4);
		SetupSwizzleSource(psNextArg, uUSEASMSwizzle);
		psNextArg++;
	}
	else
	{
		IMG_UINT32	uChan;
		IMG_UINT32	uSwizzle;
	
		uSwizzle = 0;
		for (uChan = 0; uChan < 4; uChan++)
		{
			IMG_UINT32	uChanSel;
			static const USEASM_SWIZZLE_SEL aeChanSel[] = 
			{
				USEASM_SWIZZLE_SEL_X /* SGXVEC_USE_SVEC_USSRC_SWIZSEL_X */,
				USEASM_SWIZZLE_SEL_Y /* SGXVEC_USE_SVEC_USSRC_SWIZSEL_Y */,
				USEASM_SWIZZLE_SEL_Z /* SGXVEC_USE_SVEC_USSRC_SWIZSEL_Z */,
				USEASM_SWIZZLE_SEL_W /* SGXVEC_USE_SVEC_USSRC_SWIZSEL_W */,
				USEASM_SWIZZLE_SEL_0 /* SGXVEC_USE_SVEC_USSRC_SWIZSEL_0 */,
				USEASM_SWIZZLE_SEL_1 /* SGXVEC_USE_SVEC_USSRC_SWIZSEL_1 */,
				USEASM_SWIZZLE_SEL_2 /* SGXVEC_USE_SVEC_USSRC_SWIZSEL_2 */,
				USEASM_SWIZZLE_SEL_HALF /* SGXVEC_USE_SVEC_USSRC_SWIZSEL_h */
			};

			switch (uChan)
			{
				case 0: uChanSel = (uInst0 & ~SGXVEC_USE0_SVEC_VDP34_USSWIZX_CLRMSK) >> SGXVEC_USE0_SVEC_VDP34_USSWIZX_SHIFT; break;
				case 1: uChanSel = (uInst0 & ~SGXVEC_USE0_SVEC_VDP34_USSWIZY_CLRMSK) >> SGXVEC_USE0_SVEC_VDP34_USSWIZY_SHIFT; break;
				case 2: uChanSel = (uInst0 & ~SGXVEC_USE0_SVEC_VDP34_USSWIZZ_CLRMSK) >> SGXVEC_USE0_SVEC_VDP34_USSWIZZ_SHIFT; break;
				case 3: uChanSel = (uInst0 & ~SGXVEC_USE0_SVEC_VDP34_USSWIZW_CLRMSK) >> SGXVEC_USE0_SVEC_VDP34_USSWIZW_SHIFT; break;
				default: IMG_ABORT();
			}
			uSwizzle |= (aeChanSel[uChanSel] << (USEASM_SWIZZLE_FIELD_SIZE * uChan));
		}
		SetupSwizzleSource(psNextArg, uSwizzle);
		psNextArg++;
	}
							
	/*
		Decode the second source.
	*/
	psNextArg->uType = USEASM_REGTYPE_FPINTERNAL;
	psNextArg->uNumber = (uInst0 & ~SGXVEC_USE0_SVEC_GPI0NUM_CLRMSK) >> SGXVEC_USE0_SVEC_GPI0NUM_SHIFT;
	if (uInst1 & SGXVEC_USE1_SVEC_GPI0ABS)
	{
		psNextArg->uFlags |= USEASM_ARGFLAGS_ABSOLUTE;
	}
	psNextArg++;

	/*
		Disassemble the second source swizzle.
	*/
	uGPI0HwSwiz = (uInst0 & ~SGXVEC_USE0_SVEC_GPI0SWIZ_CLRMSK) >> SGXVEC_USE0_SVEC_GPI0SWIZ_SHIFT;
	bGPI0SwizExtendedFlag = IMG_FALSE;
	if (uOp2 == SGXVEC_USE1_SVEC_OP2_VMAD3 ||
		uOp2 == SGXVEC_USE1_SVEC_OP2_VMAD4)
	{
		if (uInst1 & SGXVEC_USE1_SVEC_VMAD34_GPI0SWIZEXT)
		{
			bGPI0SwizExtendedFlag = IMG_TRUE;
		}
	}
	uGPI0Swiz = DecodeVec34Swizzle(uGPI0HwSwiz, bGPI0SwizExtendedFlag, !bVec4, bVec4);
	SetupSwizzleSource(psNextArg, uGPI0Swiz);
	psNextArg++;

	if (uOp2 == SGXVEC_USE1_SVEC_OP2_DP3 || uOp2 == SGXVEC_USE1_SVEC_OP2_DP4)
	{
		/*
			Decode the clip plane mode.
		*/
		if (uInst1 & SGXVEC_USE1_SVEC_VDP34_CENABLE)
		{
			psNextArg->uType = USEASM_REGTYPE_CLIPPLANE;
			psNextArg->uNumber = (uInst1 & ~SGXVEC_USE1_SVEC_VDP34_CPLANE_CLRMSK) >> SGXVEC_USE1_SVEC_VDP34_CPLANE_SHIFT;
		}
		else
		{
			psNextArg->uType = USEASM_REGTYPE_INTSRCSEL;
			psNextArg->uNumber = USEASM_INTSRCSEL_NONE;
		}
		psNextArg++;
	}
	else
	{
		IMG_UINT32	uGPI1HwSwiz;
		IMG_BOOL	bGPI1SwizExtendedFlag;
		IMG_UINT32	uGPI1Swiz;

		/*
			Decode the third source.
		*/
		psNextArg->uType = USEASM_REGTYPE_FPINTERNAL;
		psNextArg->uNumber = (uInst0 & ~SGXVEC_USE0_SVEC_VMAD34_GPI1NUM_CLRMSK) >> SGXVEC_USE0_SVEC_VMAD34_GPI1NUM_SHIFT;
		if (uInst1 & SGXVEC_USE1_SVEC_VMAD34_GPI1NEG)
		{
			psNextArg->uFlags |= USEASM_ARGFLAGS_NEGATE;
		}
		if (uInst1 & SGXVEC_USE1_SVEC_VMAD34_GPI1ABS)
		{
			psNextArg->uFlags |= USEASM_ARGFLAGS_ABSOLUTE;
		}
		psNextArg++;

		/*
			Decode the third source swizzle.
		*/
		uGPI1HwSwiz = (uInst0 & ~SGXVEC_USE0_SVEC_VMAD34_GPI1SWIZ_CLRMSK) >> SGXVEC_USE0_SVEC_VMAD34_GPI1SWIZ_SHIFT;
		bGPI1SwizExtendedFlag = (uInst1 & SGXVEC_USE1_SVEC_VMAD34_GPI1SWIZEXT) ? IMG_TRUE : IMG_FALSE;
		uGPI1Swiz = DecodeVec34Swizzle(uGPI1HwSwiz, bGPI1SwizExtendedFlag, !bVec4, bVec4);
		SetupSwizzleSource(psNextArg, uGPI1Swiz);
		psNextArg++;
	}

	/*
		Disassemble the increment mode.
	*/
	psNextArg->uType = USEASM_REGTYPE_INTSRCSEL;
	psNextArg->uNumber = aeIncrMode[(uInst1 & ~SGXVEC_USE1_SVEC_INCCTL_CLRMSK) >> SGXVEC_USE1_SVEC_INCCTL_SHIFT];
	psNextArg++;

	return USEDISASM_OK;
}

static IMG_UINT32 DecodeVecDestinationWriteMask(PUSE_REGISTER	psDest,
												IMG_BOOL		bF32,
												IMG_BOOL		bF16MasksUnrestricted,
												IMG_UINT32		uEncodedWriteMask)
/*****************************************************************************
 FUNCTION	: DecodeVecDestinationWriteMask

 PURPOSE	: Decode the destination write mask of a hardware instruction back
			  to the input format.

 PARAMETERS	: psDest			- Destination register.
			  bF32				- TRUE if the instruction result is F32 format.
			  bF16MasksUnrestricted
								- TRUE if an F16 format instruction allows
								masking of individual components when writing
								to the unified store.
			  uEncodedWriteMask	- Hardware format write mask.

 RETURNS	: The decoded write mask.
*****************************************************************************/
{
	IMG_UINT32	uDecodedWriteMask;

	if (psDest->uIndex != USEREG_INDEX_NONE)
	{
		bF16MasksUnrestricted = IMG_FALSE;
	}
	if (psDest->uType != USEASM_REGTYPE_TEMP &&
		psDest->uType != USEASM_REGTYPE_PRIMATTR &&
		psDest->uType != USEASM_REGTYPE_OUTPUT &&
		psDest->uType != USEASM_REGTYPE_SECATTR &&
		psDest->uType != USEASM_REGTYPE_FPINTERNAL)
	{
		bF16MasksUnrestricted = IMG_FALSE;
	}

	if (psDest->uType == USEASM_REGTYPE_FPINTERNAL || (!bF32 && bF16MasksUnrestricted))
	{
		uDecodedWriteMask = 0;
		if (uEncodedWriteMask & SGXVEC_USE1_VEC_DMSK_GPI_WRITEX)
		{
			uDecodedWriteMask |= USEREG_MASK_X;
		}
		if (uEncodedWriteMask & SGXVEC_USE1_VEC_DMSK_GPI_WRITEY)
		{
			uDecodedWriteMask |= USEREG_MASK_Y;
		}
		if (uEncodedWriteMask & SGXVEC_USE1_VEC_DMSK_GPI_WRITEZ)
		{
			uDecodedWriteMask |= USEREG_MASK_Z;
		}
		if (uEncodedWriteMask & SGXVEC_USE1_VEC_DMSK_GPI_WRITEW)
		{
			uDecodedWriteMask |= USEREG_MASK_W;
		}
	}
	else
	{
		if (bF32)
		{
			uDecodedWriteMask = 0;
			if (uEncodedWriteMask & SGXVEC_USE1_VEC_DMSK_F32US_WRITEX)
			{
				uDecodedWriteMask |= USEREG_MASK_X;
			}
			if (uEncodedWriteMask & SGXVEC_USE1_VEC_DMSK_F32US_WRITEY)
			{
				uDecodedWriteMask |= USEREG_MASK_Y;
			}
		}
		else
		{
			uDecodedWriteMask = 0;
			if (uEncodedWriteMask & SGXVEC_USE1_VEC_DMSK_F16US_WRITEXY)
			{
				uDecodedWriteMask |= USEREG_MASK_X | USEREG_MASK_Y;
			}
			if (uEncodedWriteMask & SGXVEC_USE1_VEC_DMSK_F16US_WRITEZW)
			{
				uDecodedWriteMask |= USEREG_MASK_Z | USEREG_MASK_W;
			}
		}
	}
	return uDecodedWriteMask;
}

static USEDISASM_ERROR DecodeVEC4MADInstruction(PCSGX_CORE_DESC	psTarget,
												IMG_UINT32		uInst0,
												IMG_UINT32		uInst1,
												PUSE_INST		psInst)
/*****************************************************************************
 FUNCTION	: DecodeVEC4MADInstruction

 PURPOSE	: Decodes an encoded VEC4 MAD instruction back to the USEASM
			  input instruction.

 PARAMETERS	: psTarget				- Core to disassembly.
			  uInst0				- Encoded instruction.
			  uInst1
			  psInst				- On return contains the decoded
									instruction.

 RETURNS	: Error code.
*****************************************************************************/
{
	IMG_UINT32	uDestNum;
	IMG_UINT32	uHwDestMask;
	IMG_UINT32	uSource0Num, uSource1Num, uSource2Num;
	IMG_UINT32	uSrc0Swizzle, uSrc1Swizzle, uSrc2Swizzle;
	static const IMG_UINT32 auSrc0Swizzle[] = 
	{
		USEASM_SWIZZLE(X, X, X, X), /* SGXVEC_USE_VECMAD_SRC0SWIZZLE_XXXX */
		USEASM_SWIZZLE(Y, Y, Y, Y), /* SGXVEC_USE_VECMAD_SRC0SWIZZLE_YYYY */
		USEASM_SWIZZLE(Z, Z, Z, Z), /* SGXVEC_USE_VECMAD_SRC0SWIZZLE_ZZZZ */
		USEASM_SWIZZLE(W, W, W, W), /* SGXVEC_USE_VECMAD_SRC0SWIZZLE_WWWW */
		USEASM_SWIZZLE(X, Y, Z, W), /* SGXVEC_USE_VECMAD_SRC0SWIZZLE_XYZW */
		USEASM_SWIZZLE(Y, Z, X, W), /* SGXVEC_USE_VECMAD_SRC0SWIZZLE_YZXW */
		USEASM_SWIZZLE(X, Y, W, W), /* SGXVEC_USE_VECMAD_SRC0SWIZZLE_XYWW */
		USEASM_SWIZZLE(Z, W, X, Y), /* SGXVEC_USE_VECMAD_SRC0SWIZZLE_ZWXY */
	};
	static const IMG_UINT32 auSrc1Swizzle[] = 
	{
		USEASM_SWIZZLE(X, X, X, X), /* SGXVEC_USE_VECMAD_SRC1SWIZZLE_XXXX */
		USEASM_SWIZZLE(Y, Y, Y, Y), /* SGXVEC_USE_VECMAD_SRC1SWIZZLE_YYYY */
		USEASM_SWIZZLE(Z, Z, Z, Z), /* SGXVEC_USE_VECMAD_SRC1SWIZZLE_ZZZZ */
		USEASM_SWIZZLE(W, W, W, W), /* SGXVEC_USE_VECMAD_SRC1SWIZZLE_WWWW */
		USEASM_SWIZZLE(X, Y, Z, W), /* SGXVEC_USE_VECMAD_SRC1SWIZZLE_XYZW */
		USEASM_SWIZZLE(X, Y, Y, Z),	/* SGXVEC_USE_VECMAD_SRC1SWIZZLE_XYYZ */
		USEASM_SWIZZLE(Y, Y, W, W),	/* SGXVEC_USE_VECMAD_SRC1SWIZZLE_YYWW */
		USEASM_SWIZZLE(W, Y, Z, W)	/* SGXVEC_USE_VECMAD_SRC1SWIZZLE_WYZW */
	};
	static const IMG_UINT32 auSrc2Swizzle[] = 
	{
		USEASM_SWIZZLE(X, X, X, X), /* SGXVEC_USE_VECMAD_SRC2SWIZZLE_XXXX */
		USEASM_SWIZZLE(Y, Y, Y, Y), /* SGXVEC_USE_VECMAD_SRC2SWIZZLE_YYYY */
		USEASM_SWIZZLE(Z, Z, Z, Z), /* SGXVEC_USE_VECMAD_SRC2SWIZZLE_ZZZZ */
		USEASM_SWIZZLE(W, W, W, W), /* SGXVEC_USE_VECMAD_SRC2SWIZZLE_WWWW */
		USEASM_SWIZZLE(X, Y, Z, W), /* SGXVEC_USE_VECMAD_SRC2SWIZZLE_XYZW */
		USEASM_SWIZZLE(X, Z, W, W),	/* SGXVEC_USE_VECMAD_SRC2SWIZZLE_XZWW */
		USEASM_SWIZZLE(X, X, Y, Z),	/* SGXVEC_USE_VECMAD_SRC2SWIZZLE_XXYZ */
		USEASM_SWIZZLE(X, Y, Z, Z),	/* SGXVEC_USE_VECMAD_SRC2SWIZZLE_XYZZ */
	};
	IMG_UINT32	uPredicate;
	IMG_UINT32	uDecodedDestMask;
	IMG_BOOL	bF32;

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*
		Decode vector predicate
	*/
	uPredicate = g_aeShortVectorPred[(uInst1 & ~SGXVEC_USE1_VECMAD_SVPRED_CLRMSK) >> SGXVEC_USE1_VECMAD_SVPRED_SHIFT];
	psInst->uFlags1 |= (uPredicate << USEASM_OPFLAGS1_PRED_SHIFT);

	/*
		Disassemble opcode.
	*/
	if (uInst1 & SGXVEC_USE1_VECMAD_F16)
	{
		psInst->uOpcode = USEASM_OP_VF16MAD;
		bF32 = IMG_FALSE;
	}
	else
	{
		psInst->uOpcode = USEASM_OP_VMAD;
		bF32 = IMG_TRUE;
	}

	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_SYNCSTART)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SYNCSTART;
	}
	if (uInst1 & SGXVEC_USE1_VECMAD_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}

	/*
		Decode destination.
	*/
	uDestNum = (uInst0 & ~SGXVEC_USE0_VECMAD_DST_CLRMSK) >> SGXVEC_USE0_VECMAD_DST_SHIFT;
	DecodeDestWithNum(psTarget,
					  IMG_TRUE /* bDoubleRegisters */,
					  uInst1,
					  IMG_TRUE /* bAllowExtended */,
					  &psInst->asArg[0],
					  IMG_FALSE /* bFmtControl */,
					  0 /* uAltFmtFlag */,
					  SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
					  uDestNum);

	/*
		Decode destination mask.
	*/
	uHwDestMask = (uInst1 & ~SGXVEC_USE1_VECMAD_DMSK_CLRMSK) >> SGXVEC_USE1_VECMAD_DMSK_SHIFT;
	uDecodedDestMask = 
		DecodeVecDestinationWriteMask(&psInst->asArg[0],
									  bF32,
									  IMG_FALSE /* bF16MasksUnrestricted */,
									  uHwDestMask);
	psInst->uFlags1 |= (uDecodedDestMask << USEASM_OPFLAGS1_MASK_SHIFT);

	/*
		Decode source 0
	*/
	uSource0Num = (uInst0 & ~SGXVEC_USE0_VECMAD_SRC0_CLRMSK) >> SGXVEC_USE0_VECMAD_SRC0_SHIFT;
	DecodeSrc0WithNum(psTarget,
					  IMG_TRUE /* bDoubleRegisters */,
					  uInst1,
					  IMG_FALSE /* bAllowExtended */,
					  0 /* uExt */,
					  &psInst->asArg[1],
					  IMG_FALSE /* bFmtControl */,
					  0 /* uAltFmtFlag */,
					  SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
					  uSource0Num);

	/*	
		Decode source 0 modifier.
	*/
	if (uInst1 & SGXVEC_USE1_VECMAD_SRC0ABS)
	{
		psInst->asArg[1].uFlags |= USEASM_ARGFLAGS_ABSOLUTE;
	}

	/*
		Decode source 0 swizzle.
	*/
	uSrc0Swizzle = ((uInst0 & ~SGXVEC_USE0_VECMAD_SRC0SWIZ_BITS01_CLRMSK) >> SGXVEC_USE0_VECMAD_SRC0SWIZ_BITS01_SHIFT) << 0;
	uSrc0Swizzle |= ((uInst1 & ~SGXVEC_USE1_VECMAD_SRC0SWIZ_BIT2_CLRMSK) >> SGXVEC_USE1_VECMAD_SRC0SWIZ_BIT2_SHIFT) << 2;
	SetupSwizzleSource(&psInst->asArg[2], auSrc0Swizzle[uSrc0Swizzle]);

	/*
		Decode source 1
	*/
	uSource1Num = (uInst0 & ~SGXVEC_USE0_VECMAD_SRC1_CLRMSK) >> SGXVEC_USE0_VECMAD_SRC1_SHIFT;
	DecodeSrc12WithNum(psTarget,
					   IMG_TRUE /* bDoubleRegisters */,
					   uInst0,
					   uInst1,
					   1 /* uSrc */,
					   IMG_TRUE /* bAllowExtended */,
					   EURASIA_USE1_S1BEXT /* uExt */,
					   &psInst->asArg[3],
					   IMG_FALSE /* bFmtControl */,
					   0 /* uAltFmtFlag */,
					   SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
					   uSource1Num);

	/*
		Decode source 1 modifier.
	*/
	psInst->asArg[3].uFlags |= 
		g_auDecodeSourceMod[(uInst1 & ~SGXVEC_USE1_VECMAD_SRC1MOD_CLRMSK) >> SGXVEC_USE1_VECMAD_SRC1MOD_SHIFT];

	/*
		Decode source1 swizzle.
	*/
	uSrc1Swizzle = ((uInst0 & ~SGXVEC_USE0_VECMAD_SRC1SWIZ_BITS01_CLRMSK) >> SGXVEC_USE0_VECMAD_SRC1SWIZ_BITS01_SHIFT) << 0;
	uSrc1Swizzle |= ((uInst1 & ~SGXVEC_USE1_VECMAD_SRC1SWIZ_BIT2_CLRMSK) >> SGXVEC_USE1_VECMAD_SRC1SWIZ_BIT2_SHIFT) << 2;
	SetupSwizzleSource(&psInst->asArg[4], auSrc1Swizzle[uSrc1Swizzle]);

	/*
		Decode source 2
	*/
	uSource2Num = (uInst0 & ~SGXVEC_USE0_VECMAD_SRC2_CLRMSK) >> SGXVEC_USE0_VECMAD_SRC2_SHIFT;
	DecodeSrc12WithNum(psTarget,
					   IMG_TRUE /* bDoubleRegisters */,
					   uInst0,
					   uInst1,
					   2 /* uSrc */,
					   IMG_TRUE /* bAllowExtended */,
					   EURASIA_USE1_S2BEXT /* uExt */,
					   &psInst->asArg[5],
					   IMG_FALSE /* bFmtControl */,
					   0 /* uAltFmtFlag */,
					   SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
					   uSource2Num);

	/*
		Decode source 2 modifier.
	*/
	psInst->asArg[5].uFlags |= 
		g_auDecodeSourceMod[(uInst1 & ~SGXVEC_USE1_VECMAD_SRC2MOD_CLRMSK) >> SGXVEC_USE1_VECMAD_SRC2MOD_SHIFT];

	/*
		Decode source2 swizzle.
	*/
	uSrc2Swizzle = (uInst1 & ~SGXVEC_USE1_VECMAD_SRC2SWIZ_BITS02_CLRMSK) >> SGXVEC_USE1_VECMAD_SRC2SWIZ_BITS02_SHIFT;
	SetupSwizzleSource(&psInst->asArg[6], auSrc2Swizzle[uSrc2Swizzle]);

	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeVecComplexInstruction(PCSGX_CORE_DESC	psTarget,
												   IMG_UINT32		uInst0,
												   IMG_UINT32		uInst1,
												   PUSE_INST		psInst)
{
	static const USEASM_OPCODE aeOp2[] = {USEASM_OP_VRCP, /* SGXVEC_USE1_VECCOMPLEXOP_OP2_RCP */
										  USEASM_OP_VRSQ, /* SGXVEC_USE1_VECCOMPLEXOP_OP2_RSQ */
										  USEASM_OP_VLOG, /* SGXVEC_USE1_VECCOMPLEXOP_OP2_LOG */
										  USEASM_OP_VEXP, /* SGXVEC_USE1_VECCOMPLEXOP_OP2_EXP */};
	static const IMG_UINT32 auDecodeDestType[] = 
		{
			0,						/* SGXVEC_USE1_VECCOMPLEXOP_DDTYPE_F32 */
			USEASM_ARGFLAGS_FMTF16, /* SGXVEC_USE1_VECCOMPLEXOP_DDTYPE_F16 */
			USEASM_ARGFLAGS_FMTC10, /* SGXVEC_USE1_VECCOMPLEXOP_DDTYPE_C10 */
			USE_UNDEF				/* SGXVEC_USE1_VECCOMPLEXOP_DDTYPE_RESERVED */
		};
	static const IMG_UINT32 auDecodeSrcType[] = 
		{
			0,						/* SGXVEC_USE1_VECCOMPLEXOP_SDTYPE_F32 */
			USEASM_ARGFLAGS_FMTF16,	/* SGXVEC_USE1_VECCOMPLEXOP_SDTYPE_F16 */
			USEASM_ARGFLAGS_FMTC10,	/* SGXVEC_USE1_VECCOMPLEXOP_SDTYPE_C10 */
			USE_UNDEF				/* SGXVEC_USE1_VECCOMPLEXOP_SDTYPE_RESERVED */
		};
	IMG_UINT32 uRptCount = ((uInst1 & ~SGXVEC_USE1_VECCOMPLEXOP_RCNT_CLRMSK) >> SGXVEC_USE1_VECCOMPLEXOP_RCNT_SHIFT) + 1;
	IMG_UINT32 uHwWriteMask;
	IMG_UINT32 uDecodedWriteMask;
	IMG_UINT32 uSource1Num;
	IMG_UINT32 uDestNum;
	IMG_UINT32 uSrcType;
	IMG_UINT32 uDestType;
	USEASM_PRED ePredicate;
	IMG_UINT32 uSrcComp;
	IMG_BOOL   bDestF32;

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*
		Decode predicate.
	*/
	ePredicate = g_aeDecodeEPred[(uInst1 & ~EURASIA_USE1_EPRED_CLRMSK) >> EURASIA_USE1_EPRED_SHIFT];
	psInst->uFlags1 |= (ePredicate << USEASM_OPFLAGS1_PRED_SHIFT);

	/*
		Decode opcode
	*/
	psInst->uOpcode = aeOp2[(uInst1 & ~SGXVEC_USE1_VECCOMPLEXOP_OP2_CLRMSK) >> SGXVEC_USE1_VECCOMPLEXOP_OP2_SHIFT];

	/*
		Decode opcode flags.
	*/
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_SYNCSTART)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SYNCSTART;
	}
	if (uInst1 & SGXVEC_USE1_VECCOMPLEXOP_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	if (uInst1 & EURASIA_USE1_END)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}
	psInst->uFlags1 |= (uRptCount << USEASM_OPFLAGS1_REPEAT_SHIFT);

	/*
		Decode destination.
	*/
	uDestNum = (uInst0 & ~SGXVEC_USE0_VECCOMPLEXOP_DST_CLRMSK) >> SGXVEC_USE0_VECCOMPLEXOP_DST_SHIFT;
	DecodeDestWithNum(psTarget,
					  IMG_TRUE /* bDoubleRegisters */,
					  uInst1,
					  IMG_TRUE /* bAllowExtended */,
					  &psInst->asArg[0],
					  IMG_FALSE /* bFmtControl */,
					  0 /* uAltFmtFlag */,
					  SGXVEC_USE0_VECCOMPLEXOP_DST_FIELD_LENGTH,
					  uDestNum);

	/*
		Decode destination type.
	*/
	uDestType = (uInst1 & ~SGXVEC_USE1_VECCOMPLEXOP_DDTYPE_CLRMSK) >> SGXVEC_USE1_VECCOMPLEXOP_DDTYPE_SHIFT;
	if (uDestType == SGXVEC_USE1_VECCOMPLEXOP_DDTYPE_RESERVED)
	{
		return USEDISASM_ERROR_INVALID_VECCOMPLEX_DEST_DATA_TYPE;
	}
	psInst->asArg[0].uFlags |= auDecodeDestType[uDestType];
	if (uDestType == SGXVEC_USE1_VECCOMPLEXOP_DDTYPE_F32)
	{
		bDestF32 = IMG_TRUE;
	}
	else
	{
		bDestF32 = IMG_FALSE;
	}

	/*
		Decode destination mask.
	*/
	uHwWriteMask = (uInst0 & ~SGXVEC_USE0_VECCOMPLEXOP_WMSK_CLRMSK) >> SGXVEC_USE0_VECCOMPLEXOP_WMSK_SHIFT;
	uDecodedWriteMask = 
		DecodeVecDestinationWriteMask(&psInst->asArg[0],
									  bDestF32,
									  IMG_TRUE /* bF16MasksUnrestricted */,
									  uHwWriteMask);
	psInst->uFlags1 |= (uDecodedWriteMask << USEASM_OPFLAGS1_MASK_SHIFT);
	
	/*	
		Decode source argument.
	*/
	uSource1Num = (uInst0 & ~SGXVEC_USE0_VECCOMPLEXOP_SRC1_CLRMSK) >> SGXVEC_USE0_VECCOMPLEXOP_SRC1_SHIFT;
	DecodeSrc12WithNum(psTarget,
					   IMG_TRUE /* bDoubleRegisters */,
					   uInst0,
					   uInst1,
					   1 /* uSrc */,
					   IMG_TRUE /* bAllowExtended */,
					   EURASIA_USE1_S1BEXT /* uExt */,
					   &psInst->asArg[1],
					   IMG_FALSE /* bFmtControl */,
					   0 /* uAltFmtFlag */,
					   SGXVEC_USE0_VECCOMPLEXOP_SRC1_FIELD_LENGTH,
					   uSource1Num);

	/*
		Decode source modifier.
	*/
	psInst->asArg[1].uFlags |= 
		g_auDecodeSourceMod[(uInst1 & ~SGXVEC_USE1_VECCOMPLEXOP_SRC1MOD_CLRMSK) >> SGXVEC_USE1_VECCOMPLEXOP_SRC1MOD_SHIFT];

	/*
		Decode source channel.
	*/
	uSrcComp = (uInst1 & ~SGXVEC_USE1_VECCOMPLEXOP_SRCCHANSEL_CLRMSK) >> SGXVEC_USE1_VECCOMPLEXOP_SRCCHANSEL_SHIFT;
	psInst->asArg[1].uFlags |= (uSrcComp << USEASM_ARGFLAGS_COMP_SHIFT);

	/*
		Decode source type.
	*/
	uSrcType = (uInst1 & ~SGXVEC_USE1_VECCOMPLEXOP_SDTYPE_CLRMSK) >> SGXVEC_USE1_VECCOMPLEXOP_SDTYPE_SHIFT;
	if (uSrcType == SGXVEC_USE1_VECCOMPLEXOP_SDTYPE_RESERVED)
	{
		return USEDISASM_ERROR_INVALID_VECCOMPLEX_SRC_DATA_TYPE;
	}
	psInst->asArg[1].uFlags |= auDecodeSrcType[uSrcType];

	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeVecMovInstruction(PCSGX_CORE_DESC	psTarget,
											   IMG_UINT32		uInst0,
											   IMG_UINT32		uInst1,
											   PUSE_INST		psInst)
{
	IMG_UINT32		uRptCount = ((uInst1 & ~SGXVEC_USE1_VMOVC_RCNT_CLRMSK) >> SGXVEC_USE1_VMOVC_RCNT_SHIFT) + 1;
	IMG_UINT32		uMoveType = (uInst1 & ~SGXVEC_USE1_VMOVC_MTYPE_CLRMSK) >> SGXVEC_USE1_VMOVC_MTYPE_SHIFT;
	IMG_UINT32		uDataType;
	IMG_UINT32		uDestNum;
	IMG_UINT32		uSource1Num;
	IMG_UINT32		uHwWriteMask;
	PUSE_REGISTER	psNextArg;
	IMG_UINT32		uSrcSwizzle;
	USEASM_PRED		ePredicate;
	IMG_BOOL		bDataTypeDoubleRegisters;
	IMG_UINT32		uNumberFieldLength;
	static const USEASM_OPCODE aeDecodeMove[] = 
	{
		USEASM_OP_VMOV,		/* SGXVEC_USE1_VMOVC_MTYPE_UNCONDITIONAL */
		USEASM_OP_VMOVC,	/* SGXVEC_USE1_VMOVC_MTYPE_CONDITIONAL */
		USEASM_OP_VMOVCU8,	/* SGXVEC_USE1_VMOVC_MTYPE_U8CONDITIONAL */
		USEASM_OP_UNDEF,	/* SGXVEC_USE1_VMOVC_MTYPE_RESERVED */
	};
	static const IMG_UINT32 auDecodeSrcSwizzle[] = 
	{
		USEASM_SWIZZLE(X, X, X, X),	/* SGXVEC_USE1_VMOVC_SWIZ_XXXX */
		USEASM_SWIZZLE(Y, Y, Y, Y),	/* SGXVEC_USE1_VMOVC_SWIZ_YYYY */
		USEASM_SWIZZLE(Z, Z, Z, Z),	/* SGXVEC_USE1_VMOVC_SWIZ_ZZZZ */
		USEASM_SWIZZLE(W, W, W, W),	/* SGXVEC_USE1_VMOVC_SWIZ_WWWW */
		USEASM_SWIZZLE(X, Y, Z, W),	/* SGXVEC_USE1_VMOVC_SWIZ_XYZW */
		USEASM_SWIZZLE(Y, Z, W, W),	/* SGXVEC_USE1_VMOVC_SWIZ_YZWW */
		USEASM_SWIZZLE(X, Y, Z, Z),	/* SGXVEC_USE1_VMOVC_SWIZ_XYZZ */
		USEASM_SWIZZLE(X, X, Y, Z),	/* SGXVEC_USE1_VMOVC_SWIZ_XXYZ */
		USEASM_SWIZZLE(X, Y, X, Y),	/* SGXVEC_USE1_VMOVC_SWIZ_XYXY */
		USEASM_SWIZZLE(X, Y, W, Z),	/* SGXVEC_USE1_VMOVC_SWIZ_XYWZ */
		USEASM_SWIZZLE(Z, X, Y, W),	/* SGXVEC_USE1_VMOVC_SWIZ_ZXYW */
		USEASM_SWIZZLE(Z, W, Z, W),	/* SGXVEC_USE1_VMOVC_SWIZ_ZWZW */
		USEASM_SWIZZLE(Y, Z, X, Z),	/* SGXVEC_USE1_VMOVC_SWIZ_YZXZ */
		USEASM_SWIZZLE(X, X, Y, Y),	/* SGXVEC_USE1_VMOVC_SWIZ_XXYY */
		USEASM_SWIZZLE(X, Z, W, W),	/* SGXVEC_USE1_VMOVC_SWIZ_XZWW */
		USEASM_SWIZZLE(X, Y, Z, 1),	/* SGXVEC_USE1_VMOVC_SWIZ_XYZ1 */
	};

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*
		Decode predicate.
	*/
	ePredicate = g_aeDecodeEPred[(uInst1 & ~EURASIA_USE1_EPRED_CLRMSK) >> EURASIA_USE1_EPRED_SHIFT];
	psInst->uFlags1 |= (ePredicate << USEASM_OPFLAGS1_PRED_SHIFT);

	/*
		Decode opcode
	*/
	if (aeDecodeMove[uMoveType] == USEASM_OP_UNDEF)
	{
		return USEDISASM_ERROR_INVALID_VECMOV_TYPE;
	}
	psInst->uOpcode = aeDecodeMove[uMoveType];

	/*
		Decode opcode flags.
	*/
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_SYNCSTART)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SYNCSTART;
	}
	if (uInst1 & SGXVEC_USE1_VMOVC_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	if (uInst1 & EURASIA_USE1_END)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}
	psInst->uFlags1 |= (uRptCount << USEASM_OPFLAGS1_REPEAT_SHIFT);

	/*
		Decode move data type.
	*/
	uDataType = (uInst1 & ~SGXVEC_USE1_VMOVC_MDTYPE_CLRMSK) >> SGXVEC_USE1_VMOVC_MDTYPE_SHIFT;
	switch (uDataType)
	{
		case SGXVEC_USE1_VMOVC_MDTYPE_INT8: psInst->uFlags1 |= USEASM_OPFLAGS1_MOVC_FMTI8; break;
		case SGXVEC_USE1_VMOVC_MDTYPE_INT16: psInst->uFlags2 |= USEASM_OPFLAGS2_MOVC_FMTI16; break;
		case SGXVEC_USE1_VMOVC_MDTYPE_INT32: psInst->uFlags2 |= USEASM_OPFLAGS2_MOVC_FMTI32; break;
		case SGXVEC_USE1_VMOVC_MDTYPE_C10: psInst->uFlags2 |= USEASM_OPFLAGS2_MOVC_FMTI10; break;
		case SGXVEC_USE1_VMOVC_MDTYPE_F16: psInst->uFlags1 |= USEASM_OPFLAGS1_MOVC_FMTF16; break;
		case SGXVEC_USE1_VMOVC_MDTYPE_F32: psInst->uFlags2 |= USEASM_OPFLAGS2_MOVC_FMTF32; break;
		default: return USEDISASM_ERROR_INVALID_VECMOV_DATA_TYPE;
	}

	/*
		Are register number 64-bit or 32-bit aligned?
	*/
	if (uDataType == SGXVEC_USE1_VMOVC_MDTYPE_C10 ||
		uDataType == SGXVEC_USE1_VMOVC_MDTYPE_F16 ||
		uDataType == SGXVEC_USE1_VMOVC_MDTYPE_F32)
	{
		bDataTypeDoubleRegisters = IMG_TRUE;
		uNumberFieldLength = SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH;
	}
	else
	{
		bDataTypeDoubleRegisters = IMG_FALSE;
		uNumberFieldLength = SGXVEC_USE0_VMOVC_NONVEC_REGISTER_NUMBER_FIELD_LENGTH;
	}

	/*
		Decode test type.
	*/
	if (uMoveType != SGXVEC_USE1_VMOVC_MTYPE_UNCONDITIONAL)
	{
		IMG_UINT32	uTestCc;

		psInst->uFlags1 |= USEASM_OPFLAGS1_TESTENABLE;

		uTestCc = (uInst1 & ~SGXVEC_USE1_VMOVC_TESTCC_CLRMSK) >> SGXVEC_USE1_VMOVC_TESTCC_SHIFT;
		if (uInst1 & SGXVEC_USE1_VMOVC_TESTCCEXT)
		{
			switch (uTestCc)
			{
				case SGXVEC_USE1_VMOVC_TESTCC_EXTLTZERO: psInst->uTest = USEASM_TEST_LTZERO; break;
				case SGXVEC_USE1_VMOVC_TESTCC_EXTLTEZERO: psInst->uTest = USEASM_TEST_LTEZERO; break;
			}
		}
		else
		{
			switch (uTestCc)
			{
				case SGXVEC_USE1_VMOVC_TESTCC_STDZERO: psInst->uTest = USEASM_TEST_EQZERO; break;
				case SGXVEC_USE1_VMOVC_TESTCC_STDNONZERO: psInst->uTest = USEASM_TEST_NEQZERO; break;
			}
		}
	}

	/*
		Decode destination.
	*/
	uDestNum = (uInst0 & ~SGXVEC_USE0_VMOVC_DST_CLRMSK) >> SGXVEC_USE0_VMOVC_DST_SHIFT;
	DecodeDestWithNum(psTarget,
					  bDataTypeDoubleRegisters,
					  uInst1,
					  IMG_TRUE /* bAllowExtended */,
					  &psInst->asArg[0],
					  IMG_FALSE /* bFmtControl */,
					  0 /* uAltFmtFlag */,
					  uNumberFieldLength,
					  uDestNum);

	/*
		Decode destination mask.
	*/
	uHwWriteMask = (uInst0 & ~SGXVEC_USE0_VMOVC_WMASK_CLRMSK) >> SGXVEC_USE0_VMOVC_WMASK_SHIFT;
	if (uDataType == SGXVEC_USE1_VMOVC_MDTYPE_F32 ||
		uDataType == SGXVEC_USE1_VMOVC_MDTYPE_F16)
	{
		IMG_UINT32	uInputWriteMask;
		IMG_BOOL	bF32;

		bF32 = IMG_FALSE;
		if (uDataType == SGXVEC_USE1_VMOVC_MDTYPE_F32)
		{
			bF32 = IMG_TRUE;
		}

		uInputWriteMask = 
			DecodeVecDestinationWriteMask(&psInst->asArg[0],
										  bF32,
										  IMG_FALSE /* bF16MasksUnrestricted */,
										  uHwWriteMask);
		psInst->uFlags1 |= (uInputWriteMask << USEASM_OPFLAGS1_MASK_SHIFT);
	}
	else
	{
		psInst->uFlags1 |= (uHwWriteMask << USEASM_OPFLAGS1_MASK_SHIFT);
	}
	

	psNextArg = &psInst->asArg[1];
	if (uMoveType != SGXVEC_USE1_VMOVC_MTYPE_UNCONDITIONAL)
	{
		IMG_UINT32	uSource0Num;

		/*
			Decode source 0.
		*/
		uSource0Num = (uInst0 & ~SGXVEC_USE0_VMOVC_SRC0_CLRMSK) >> SGXVEC_USE0_VMOVC_SRC0_SHIFT;
		DecodeSrc0WithNum(psTarget,
						  bDataTypeDoubleRegisters,
						  uInst1,
						  IMG_FALSE /* bAllowExtended */,
						  0 /* uExt */,
						  psNextArg,
						  IMG_FALSE /* bFmtControl */,
						  0 /* uAltFmtFlag */,
						  uNumberFieldLength,
						  uSource0Num);

		/*
			Decode source 0 component select.
		*/
		if (uInst1 & SGXVEC_USE1_VMOVC_S0ULSEL)
		{
			psNextArg->uFlags |= (2 << USEASM_ARGFLAGS_COMP_SHIFT);
		}

		psNextArg++;
	}

	/*
		Decode source 1.
	*/
	uSource1Num = (uInst0 & ~SGXVEC_USE0_VMOVC_SRC1_CLRMSK) >> SGXVEC_USE0_VMOVC_SRC1_SHIFT;
	DecodeSrc12WithNum(psTarget,
					   bDataTypeDoubleRegisters,
					   uInst0,
					   uInst1,
					   1 /* uSrc */,
					   IMG_TRUE /* bAllowExtended */,
					   EURASIA_USE1_S1BEXT /* uExt */,
					   psNextArg,
					   IMG_FALSE /* bFmtControl */,
					   0 /* uAltFmtFlag */,
					   uNumberFieldLength,
					   uSource1Num);
	psNextArg++;

	if (uMoveType != SGXVEC_USE1_VMOVC_MTYPE_UNCONDITIONAL)
	{
		IMG_UINT32	uSource2Num;

		/*
			Decode source 2.
		*/
		uSource2Num = (uInst0 & ~SGXVEC_USE0_VMOVC_SRC2_CLRMSK) >> SGXVEC_USE0_VMOVC_SRC2_SHIFT;
		DecodeSrc12WithNum(psTarget,
						   bDataTypeDoubleRegisters,
						   uInst0,
						   uInst1,
						   2 /* uSrc */,
						   IMG_TRUE /* bAllowExtended */,
						   EURASIA_USE1_S2BEXT /* uExt */,
						   psNextArg,
						   IMG_FALSE /* bFmtControl */,
						   0 /* uAltFmtFlag */,
						   uNumberFieldLength,
						   uSource2Num);
		psNextArg++;
	}

	/*
		Decode swizzle.
	*/
	uSrcSwizzle = (uInst1 & ~SGXVEC_USE1_VMOVC_SWIZ_CLRMSK) >> SGXVEC_USE1_VMOVC_SWIZ_SHIFT;
	SetupSwizzleSource(psNextArg, auDecodeSrcSwizzle[uSrcSwizzle]);

	return USEDISASM_OK;
}

static IMG_UINT32 DecodeVEC4NONMADInstructionSrc1Swizzle(IMG_UINT32	uInst0,
														 IMG_UINT32	uInst1)
{
	IMG_UINT32	uHwSwizzle;
	IMG_UINT32	uChan;
	IMG_UINT32	uSwizzle;

	uHwSwizzle =
		((uInst0 & ~SGXVEC_USE0_VECNONMAD_SRC1SWIZ_BITS06_CLRMSK) >> SGXVEC_USE0_VECNONMAD_SRC1SWIZ_BITS06_SHIFT) << 0;
	uHwSwizzle |=
		((uInst1 & ~SGXVEC_USE1_VECNONMAD_SRC1SWIZ_BIT78_CLRMSK) >> SGXVEC_USE1_VECNONMAD_SRC1SWIZ_BIT78_SHIFT) << 7;
	uHwSwizzle |=
		((uInst1 & ~SGXVEC_USE1_VECNONMAD_SRC1SWIZ_BIT9_CLRMSK) >> SGXVEC_USE1_VECNONMAD_SRC1SWIZ_BIT9_SHIFT) << 9;
	uHwSwizzle |=
		((uInst1 & ~SGXVEC_USE1_VECNONMAD_SRC1SWIZ_BITS1011_CLRMSK) >> SGXVEC_USE1_VECNONMAD_SRC1SWIZ_BITS1011_SHIFT) << 10;

	uSwizzle = 0;
	for (uChan = 0; uChan < 4; uChan++)
	{
		IMG_UINT32	uSel;
		static const IMG_UINT32 g_auSel[] = {USEASM_SWIZZLE_SEL_X /* SGXVEC_USE_VECNONMAD_SRC1SWIZ_SEL_X */,
											 USEASM_SWIZZLE_SEL_Y /* SGXVEC_USE_VECNONMAD_SRC1SWIZ_SEL_Y */,
											 USEASM_SWIZZLE_SEL_Z /* SGXVEC_USE_VECNONMAD_SRC1SWIZ_SEL_Z */,
											 USEASM_SWIZZLE_SEL_W /* SGXVEC_USE_VECNONMAD_SRC1SWIZ_SEL_W */,
											 USEASM_SWIZZLE_SEL_0 /* SGXVEC_USE_VECNONMAD_SRC1SWIZ_SEL_0 */,
											 USEASM_SWIZZLE_SEL_1 /* SGXVEC_USE_VECNONMAD_SRC1SWIZ_SEL_1 */,
											 USEASM_SWIZZLE_SEL_2 /* SGXVEC_USE_VECNONMAD_SRC1SWIZ_SEL_2 */,
											 USEASM_SWIZZLE_SEL_HALF /* SGXVEC_USE_VECNONMAD_SRC1SWIZ_SEL_HALF */};

		switch (uChan)
		{
			case 0: uSel = (uHwSwizzle & ~SGXVEC_USE_VECNONMAD_SRC1SWIZ_X_CLRMSK) >> SGXVEC_USE_VECNONMAD_SRC1SWIZ_X_SHIFT; break;
			case 1: uSel = (uHwSwizzle & ~SGXVEC_USE_VECNONMAD_SRC1SWIZ_Y_CLRMSK) >> SGXVEC_USE_VECNONMAD_SRC1SWIZ_Y_SHIFT; break;
			case 2: uSel = (uHwSwizzle & ~SGXVEC_USE_VECNONMAD_SRC1SWIZ_Z_CLRMSK) >> SGXVEC_USE_VECNONMAD_SRC1SWIZ_Z_SHIFT; break;
			case 3: uSel = (uHwSwizzle & ~SGXVEC_USE_VECNONMAD_SRC1SWIZ_W_CLRMSK) >> SGXVEC_USE_VECNONMAD_SRC1SWIZ_W_SHIFT; break;
			default: IMG_ABORT();
		}

		uSwizzle |= (g_auSel[uSel] << (USEASM_SWIZZLE_FIELD_SIZE * uChan));
	}
	return uSwizzle;
}

static USEDISASM_ERROR DecodeVEC4NONMADInstruction(PCSGX_CORE_DESC	psTarget,
												   IMG_UINT32		uInst0,
												   IMG_UINT32		uInst1,
												   PUSE_INST		psInst,
												   IMG_BOOL			bF16)
{
	IMG_UINT32	uDestMask;
	static const USEASM_OPCODE aeDecodeF16Op2[] =
	{
		USEASM_OP_VF16MUL,		/* SGXVEC_USE0_VECNONMAD_OP2_VMUL */
		USEASM_OP_VF16ADD,		/* SGXVEC_USE0_VECNONMAD_OP2_VADD */
		USEASM_OP_VF16FRC,		/* SGXVEC_USE0_VECNONMAD_OP2_VFRC */
		USEASM_OP_VF16DSX,		/* SGXVEC_USE0_VECNONMAD_OP2_VDSX */
		USEASM_OP_VF16DSY,		/* SGXVEC_USE0_VECNONMAD_OP2_VDSY */
		USEASM_OP_VF16MIN,		/* SGXVEC_USE0_VECNONMAD_OP2_VMIN */
		USEASM_OP_VF16MAX,		/* SGXVEC_USE0_VECNONMAD_OP2_VMAX */
		USEASM_OP_VF16DP,		/* SGXVEC_USE0_VECNONMAD_OP2_VDP */
	};
	static const USEASM_OPCODE aeDecodeF32Op2[] =
	{
		USEASM_OP_VMUL,			/* SGXVEC_USE0_VECNONMAD_OP2_VMUL */
		USEASM_OP_VADD,			/* SGXVEC_USE0_VECNONMAD_OP2_VADD */
		USEASM_OP_VFRC,			/* SGXVEC_USE0_VECNONMAD_OP2_VFRC */
		USEASM_OP_VDSX,			/* SGXVEC_USE0_VECNONMAD_OP2_VDSX */
		USEASM_OP_VDSY,			/* SGXVEC_USE0_VECNONMAD_OP2_VDSY */
		USEASM_OP_VMIN,			/* SGXVEC_USE0_VECNONMAD_OP2_VMIN */
		USEASM_OP_VMAX,			/* SGXVEC_USE0_VECNONMAD_OP2_VMAX */
		USEASM_OP_VDP,			/* SGXVEC_USE0_VECNONMAD_OP2_VDP */
	};
	static const IMG_UINT32 auSrc2Swizzle[] =
	{
		USEASM_SWIZZLE(X, X, X, X),	/* SGXVEC_USE_VECNONMAD_SRC2_SWIZ_XXXX */
		USEASM_SWIZZLE(Y, Y, Y, Y),	/* SGXVEC_USE_VECNONMAD_SRC2_SWIZ_YYYY */
		USEASM_SWIZZLE(Z, Z, Z, Z),	/* SGXVEC_USE_VECNONMAD_SRC2_SWIZ_ZZZZ */
		USEASM_SWIZZLE(W, W, W, W),	/* SGXVEC_USE_VECNONMAD_SRC2_SWIZ_WWWW */
		USEASM_SWIZZLE(X, Y, Z, W),	/* SGXVEC_USE_VECNONMAD_SRC2_SWIZ_XYZW */
		USEASM_SWIZZLE(Y, Z, W, W),	/* SGXVEC_USE_VECNONMAD_SRC2_SWIZ_YZWW */
		USEASM_SWIZZLE(X, Y, Z, Z),	/* SGXVEC_USE_VECNONMAD_SRC2_SWIZ_XYZZ */
		USEASM_SWIZZLE(X, X, Y, Z),	/* SGXVEC_USE_VECNONMAD_SRC2_SWIZ_XXYZ */
		USEASM_SWIZZLE(X, Y, X, Y),	/* SGXVEC_USE_VECNONMAD_SRC2_SWIZ_XYXY */
		USEASM_SWIZZLE(X, Y, W, Z),	/* SGXVEC_USE_VECNONMAD_SRC2_SWIZ_XYWZ */
		USEASM_SWIZZLE(Z, X, Y, W),	/* SGXVEC_USE_VECNONMAD_SRC2_SWIZ_ZXYW */
		USEASM_SWIZZLE(Z, W, Z, W),	/* SGXVEC_USE_VECNONMAD_SRC2_SWIZ_ZWZW */
		USEASM_SWIZZLE(Y, Z, X, Z),	/* SGXVEC_USE_VECNONMAD_SRC2_SWIZ_YZXZ	*/
		USEASM_SWIZZLE(X, X, Y, Y),	/* SGXVEC_USE_VECNONMAD_SRC2_SWIZ_XXYY */
		USEASM_SWIZZLE(X, Z, W, W),	/* SGXVEC_USE_VECNONMAD_SRC2_SWIZ_XZWW	*/
		USEASM_SWIZZLE(X, Y, Z, 1),	/* SGXVEC_USE_VECNONMAD_SRC2_SWIZ_XYZ1 */
	};
	IMG_UINT32	uDestNum, uSource1Num, uSource2Num;
	IMG_UINT32	uSrc1Mod;
	IMG_UINT32	uSrc1Swiz;
	IMG_UINT32	uSrc2Swiz;
	IMG_UINT32	uOp2;
	USEASM_PRED	ePredicate;

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*	
		Extract the destination register number.
	*/
	uDestNum = (uInst0 & ~SGXVEC_USE0_VECNONMAD_DST_CLRMSK) >> SGXVEC_USE0_VECNONMAD_DST_SHIFT;

	/*
		Disassemble vector predicate
	*/
	ePredicate = 
		g_aeExtVectorPred[(uInst1 & ~SGXVEC_USE1_VECNONMAD_EVPRED_CLRMSK) >> SGXVEC_USE1_VECNONMAD_EVPRED_SHIFT];
	psInst->uFlags1 |= (ePredicate << USEASM_OPFLAGS1_PRED_SHIFT);

	/*
		Disassemble opcode.
	*/
	uOp2 =
		(uInst0 & ~SGXVEC_USE0_VECNONMAD_OP2_CLRMSK) >> SGXVEC_USE0_VECNONMAD_OP2_SHIFT;
	if (bF16)
	{
		psInst->uOpcode = aeDecodeF16Op2[uOp2];
	}
	else
	{
		psInst->uOpcode = aeDecodeF32Op2[uOp2];
	}

	/*
		Decode instruction flags.
	*/
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_SYNCSTART)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SYNCSTART;
	}
	if (uInst1 & SGXVEC_USE1_VECNONMAD_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}

	/*
		Decode destination.
	*/
	DecodeDestWithNum(psTarget,
					  IMG_TRUE /* bDoubleRegisters */,
					  uInst1,
					  IMG_TRUE /* bAllowExtended */,
					  &psInst->asArg[0],
					  IMG_FALSE /* bFmtControl */,
					  0 /* uAltFmtFlag */,
					  SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
					  uDestNum);

	/*
		Decode destination mask.
	*/
	uDestMask = (uInst1 & ~SGXVEC_USE1_VECNONMAD_DMASK_CLRMSK) >> SGXVEC_USE1_VECNONMAD_DMASK_SHIFT;
	psInst->uFlags1 |= (uDestMask << USEASM_OPFLAGS1_MASK_SHIFT);

	/*
		Decode source 1
	*/
	uSource1Num = (uInst0 & ~SGXVEC_USE0_VECNONMAD_SRC1_CLRMSK) >> SGXVEC_USE0_VECNONMAD_SRC1_SHIFT;
	DecodeSrc12WithNum(psTarget,
					   IMG_TRUE /* bDoubleRegisters */,
					   uInst0,
					   uInst1,
					   1 /* uSrc */,
					   IMG_TRUE /* bAllowExtended */,
					   EURASIA_USE1_S1BEXT /* uExt */,
					   &psInst->asArg[1],
					   IMG_FALSE /* bFmtControl */,
					   0 /* uAltFmtFlag */,
					   SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
					   uSource1Num);

	/*
		Decode source 1 modifier.
	*/
	uSrc1Mod = (uInst1 & ~SGXVEC_USE1_VECNONMAD_SRC1MOD_CLRMSK) >> SGXVEC_USE1_VECNONMAD_SRC1MOD_SHIFT;
	psInst->asArg[1].uFlags |= g_auDecodeSourceMod[uSrc1Mod];

	/*
		Decode source1 swizzle.
	*/
	uSrc1Swiz = DecodeVEC4NONMADInstructionSrc1Swizzle(uInst0, uInst1);
	SetupSwizzleSource(&psInst->asArg[2], uSrc1Swiz);

	/*
		Decode source 2
	*/
	uSource2Num = (uInst0 & ~SGXVEC_USE0_VECNONMAD_SRC2_CLRMSK) >> SGXVEC_USE0_VECNONMAD_SRC2_SHIFT;
	DecodeSrc12WithNum(psTarget,
					   IMG_TRUE /* bDoubleRegisters */,
					   uInst0,
					   uInst1,
					   2 /* uSrc */,
					   IMG_TRUE /* bAllowExtended */,
					   EURASIA_USE1_S2BEXT /* uExt */,
					   &psInst->asArg[3],
					   IMG_FALSE /* bFmtControl */,
					   0 /* uAltFmtFlag */,
					   SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
					   uSource2Num);

	/*
		Decode source 2 modifier.
	*/
	if (uInst1 & SGXVEC_USE1_VECNONMAD_SRC2ABS)
	{
		psInst->asArg[3].uFlags |= USEASM_ARGFLAGS_ABSOLUTE;
	}

	/*
		Decode source2 swizzle.
	*/
	uSrc2Swiz = (uInst1 & ~SGXVEC_USE1_VECNONMAD_SRC2SWIZ_CLRMSK) >> SGXVEC_USE1_VECNONMAD_SRC2SWIZ_SHIFT;
	SetupSwizzleSource(&psInst->asArg[4], auSrc2Swizzle[uSrc2Swiz]);

	return USEDISASM_OK;
}

static IMG_BOOL DecodeVecPckInstruction(PCSGX_CORE_DESC	psTarget,
										IMG_UINT32		uInst0,
										IMG_UINT32		uInst1,
										PUSE_INST		psInst)
{
	static const USEASM_OPCODE aeOp[][EURASIA_USE1_PCK_FMT_C10 + 1] =
	{
		/* EURASIA_USE1_PCK_FMT_U8 */
		{
			USEASM_OP_VPCKU8U8	/* EURASIA_USE1_PCK_FMT_U8 */,
			USEASM_OP_VPCKU8S8	/* EURASIA_USE1_PCK_FMT_S8 */,
			USEASM_OP_VPCKU8O8	/* EURASIA_USE1_PCK_FMT_O8 */,
			USEASM_OP_VPCKU8U16	/* EURASIA_USE1_PCK_FMT_U16 */,
			USEASM_OP_VPCKU8S16	/* EURASIA_USE1_PCK_FMT_S16 */,
			USEASM_OP_VPCKU8F16	/* EURASIA_USE1_PCK_FMT_F16 */,
			USEASM_OP_VPCKU8F32	/* EURASIA_USE1_PCK_FMT_F32 */,
			USEASM_OP_UNDEF		/* EURASIA_USE1_PCK_FMT_C10 */,
		},
		/* EURASIA_USE1_PCK_FMT_S8 */
		{
			USEASM_OP_VPCKS8U8	/* EURASIA_USE1_PCK_FMT_U8 */,
			USEASM_OP_VPCKS8S8	/* EURASIA_USE1_PCK_FMT_S8 */,
			USEASM_OP_VPCKS8O8	/* EURASIA_USE1_PCK_FMT_O8 */,
			USEASM_OP_VPCKS8U16	/* EURASIA_USE1_PCK_FMT_U16 */,
			USEASM_OP_VPCKS8S16	/* EURASIA_USE1_PCK_FMT_S16 */,
			USEASM_OP_VPCKS8F16	/* EURASIA_USE1_PCK_FMT_F16 */,
			USEASM_OP_VPCKS8F32	/* EURASIA_USE1_PCK_FMT_F32 */,
			USEASM_OP_UNDEF		/* EURASIA_USE1_PCK_FMT_C10 */,
		},
		/* EURASIA_USE1_PCK_FMT_O8 */
		{
			USEASM_OP_VPCKO8U8	/* EURASIA_USE1_PCK_FMT_U8 */,
			USEASM_OP_VPCKO8S8	/* EURASIA_USE1_PCK_FMT_S8 */,
			USEASM_OP_VPCKO8O8	/* EURASIA_USE1_PCK_FMT_O8 */,
			USEASM_OP_VPCKO8U16	/* EURASIA_USE1_PCK_FMT_U16 */,
			USEASM_OP_VPCKO8S16	/* EURASIA_USE1_PCK_FMT_S16 */,
			USEASM_OP_VPCKO8F16	/* EURASIA_USE1_PCK_FMT_F16 */,
			USEASM_OP_VPCKO8F32	/* EURASIA_USE1_PCK_FMT_F32 */,
			USEASM_OP_UNDEF		/* EURASIA_USE1_PCK_FMT_C10 */,
		},
		/* EURASIA_USE1_PCK_FMT_U16 */
		{
			USEASM_OP_VPCKU16U8		/* EURASIA_USE1_PCK_FMT_U8 */,
			USEASM_OP_VPCKU16S8		/* EURASIA_USE1_PCK_FMT_S8 */,
			USEASM_OP_VPCKU16O8		/* EURASIA_USE1_PCK_FMT_O8 */,
			USEASM_OP_VPCKU16U16	/* EURASIA_USE1_PCK_FMT_U16 */,
			USEASM_OP_VPCKU16S16	/* EURASIA_USE1_PCK_FMT_S16 */,
			USEASM_OP_VPCKU16F16	/* EURASIA_USE1_PCK_FMT_F16 */,
			USEASM_OP_VPCKU16F32	/* EURASIA_USE1_PCK_FMT_F32 */,
			USEASM_OP_UNDEF			/* EURASIA_USE1_PCK_FMT_C10 */,
		},
		/* EURASIA_USE1_PCK_FMT_S16 */
		{
			USEASM_OP_VPCKS16U8		/* EURASIA_USE1_PCK_FMT_U8 */,
			USEASM_OP_VPCKS16S8		/* EURASIA_USE1_PCK_FMT_S8 */,
			USEASM_OP_VPCKS16O8		/* EURASIA_USE1_PCK_FMT_O8 */,
			USEASM_OP_VPCKS16U16	/* EURASIA_USE1_PCK_FMT_U16 */,
			USEASM_OP_VPCKS16S16	/* EURASIA_USE1_PCK_FMT_S16 */,
			USEASM_OP_VPCKS16F16	/* EURASIA_USE1_PCK_FMT_F16 */,
			USEASM_OP_VPCKS16F32	/* EURASIA_USE1_PCK_FMT_F32 */,
			USEASM_OP_UNDEF			/* EURASIA_USE1_PCK_FMT_C10 */,
		},
		/* EURASIA_USE1_PCK_FMT_F16 */
		{
			USEASM_OP_VPCKF16U8		/* EURASIA_USE1_PCK_FMT_U8 */,
			USEASM_OP_VPCKF16S8		/* EURASIA_USE1_PCK_FMT_S8 */,
			USEASM_OP_VPCKF16O8		/* EURASIA_USE1_PCK_FMT_O8 */,
			USEASM_OP_VPCKF16U16	/* EURASIA_USE1_PCK_FMT_U16 */,
			USEASM_OP_VPCKF16S16	/* EURASIA_USE1_PCK_FMT_S16 */,
			USEASM_OP_VPCKF16F16	/* EURASIA_USE1_PCK_FMT_F16 */,
			USEASM_OP_VPCKF16F32	/* EURASIA_USE1_PCK_FMT_F32 */,
			USEASM_OP_VPCKF16C10	/* EURASIA_USE1_PCK_FMT_C10 */,
		},
		/* EURASIA_USE1_PCK_FMT_F32 */
		{
			USEASM_OP_VPCKF32U8		/* EURASIA_USE1_PCK_FMT_U8 */,
			USEASM_OP_VPCKF32S8		/* EURASIA_USE1_PCK_FMT_S8 */,
			USEASM_OP_VPCKF32O8		/* EURASIA_USE1_PCK_FMT_O8 */,
			USEASM_OP_VPCKF32U16	/* EURASIA_USE1_PCK_FMT_U16 */,
			USEASM_OP_VPCKF32S16	/* EURASIA_USE1_PCK_FMT_S16 */,
			USEASM_OP_VPCKF32F16	/* EURASIA_USE1_PCK_FMT_F16 */,
			USEASM_OP_VPCKF32F32	/* EURASIA_USE1_PCK_FMT_F32 */,
			USEASM_OP_VPCKF32C10	/* EURASIA_USE1_PCK_FMT_C10 */,
		},
		/* EURASIA_USE1_PCK_FMT_C10 */
		{
			USEASM_OP_UNDEF			/* EURASIA_USE1_PCK_FMT_U8 */,
			USEASM_OP_UNDEF			/* EURASIA_USE1_PCK_FMT_S8 */,
			USEASM_OP_UNDEF			/* EURASIA_USE1_PCK_FMT_O8 */,
			USEASM_OP_UNDEF			/* EURASIA_USE1_PCK_FMT_U16 */,
			USEASM_OP_UNDEF			/* EURASIA_USE1_PCK_FMT_S16 */,
			USEASM_OP_VPCKC10F16	/* EURASIA_USE1_PCK_FMT_F16 */,
			USEASM_OP_VPCKC10F32	/* EURASIA_USE1_PCK_FMT_F32 */,
			USEASM_OP_VPCKC10C10	/* EURASIA_USE1_PCK_FMT_C10 */,
		},
	};

	IMG_UINT32	uDestMask = (uInst1 & ~EURASIA_USE1_PCK_DMASK_CLRMSK) >> EURASIA_USE1_PCK_DMASK_SHIFT;
	IMG_UINT32	uDestFmt = (uInst1 & ~EURASIA_USE1_PCK_DSTF_CLRMSK) >> EURASIA_USE1_PCK_DSTF_SHIFT;
	IMG_UINT32	uSrcFmt = (uInst1 & ~EURASIA_USE1_PCK_SRCF_CLRMSK) >> EURASIA_USE1_PCK_SRCF_SHIFT;
	IMG_UINT32	uRepeatCount = (uInst1 & ~SGXVEC_USE1_PCK_RCOUNT_CLRMSK) >> SGXVEC_USE1_PCK_RCOUNT_SHIFT;
	IMG_UINT32	uChan;
	IMG_UINT32	uSwizzleSrc;
	USEASM_PRED ePredicate;
	IMG_UINT32	uSwizzle;
	IMG_BOOL	bReplicateConversion;
	IMG_BOOL	bSrcFloat;
	IMG_BOOL	bDestFloat;
	IMG_BOOL	bSparseSwizzle;
	IMG_UINT32	auSwizzleSel[4];

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*
		Decode predicate.
	*/
	ePredicate = g_aeDecodeEPred[(uInst1 & ~EURASIA_USE1_EPRED_CLRMSK) >> EURASIA_USE1_EPRED_SHIFT];
	psInst->uFlags1 |= (ePredicate << USEASM_OPFLAGS1_PRED_SHIFT);

	/*
		Decode opcode.
	*/
	psInst->uOpcode = aeOp[uDestFmt][uSrcFmt];
	if (psInst->uOpcode == USEASM_OP_UNDEF)
	{
		return USEDISASM_ERROR_INVALID_VPCK_FORMAT_COMBINATION;
	}

	/*
		Decode the instruction flags.
	*/
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_SYNCSTART)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SYNCSTART;
	}
	if (uInst0 & EURASIA_USE0_PCK_SCALE)
	{
		psInst->uFlags2 |= USEASM_OPFLAGS2_SCALE;
	}
	if (uInst1 & EURASIA_USE1_END)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}
	if (uInst1 & EURASIA_USE1_PCK_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	psInst->uFlags1 |= ((uRepeatCount + 1) << USEASM_OPFLAGS1_REPEAT_SHIFT);
	
	/*
		Decode destination register.
	*/
	DecodeDest(psTarget, uInst0, uInst1, TRUE, &psInst->asArg[0], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);

	/*
		Decode the destination write mask.
	*/
	psInst->uFlags1 |= (uDestMask << USEASM_OPFLAGS1_MASK_SHIFT);
	/*
		By default the swizzle is the second source.
	*/
	uSwizzleSrc = 2;

	if (uSrcFmt == EURASIA_USE1_PCK_FMT_F32 ||
		uSrcFmt == EURASIA_USE1_PCK_FMT_F16 ||
		uSrcFmt == EURASIA_USE1_PCK_FMT_C10)
	{
		IMG_UINT32	uSource1Num;

		uSource1Num = (uInst0 & ~SGXVEC_USE0_PCK_64BITSRC1_CLRMSK) >> SGXVEC_USE0_PCK_64BITSRC1_SHIFT;
		DecodeSrc12WithNum(psTarget,
						   IMG_TRUE /* bDoubleRegisters */,
						   uInst0,
						   uInst1,
						   1 /* uSrc */,
						   IMG_TRUE /* bAllowExtended */,
						   EURASIA_USE1_S1BEXT /* uExt */,
						   &psInst->asArg[1],
						   IMG_FALSE /* bFmtControl */,
						   0 /* uAltFmtFlag */,
						   SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
						   uSource1Num);

		if (uSrcFmt == EURASIA_USE1_PCK_FMT_F32)
		{
			IMG_UINT32	uSource2Num;

			uSource2Num = (uInst0 & ~SGXVEC_USE0_PCK_64BITSRC2_CLRMSK) >> SGXVEC_USE0_PCK_64BITSRC2_SHIFT;
			DecodeSrc12WithNum(psTarget,
							   IMG_TRUE /* bDoubleRegisters */,
							   uInst0,
							   uInst1,
							   2 /* uSrc */,
							   IMG_TRUE /* bAllowExtended */,
							   EURASIA_USE1_S2BEXT /* uExt */,
							   &psInst->asArg[2],
							   IMG_FALSE /* bFmtControl */,
							   0 /* uAltFmtFlag */,
							   SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
							   uSource2Num);

			/*
				The swizzle is the third source.
			*/
			uSwizzleSrc = 3;
		}
	}
	else
	{
		DecodeSrc12(psTarget, 
					uInst0, 
					uInst1, 
					1, 
					TRUE, 
					EURASIA_USE1_S1BEXT, 
					&psInst->asArg[1], 
					USEDIS_FORMAT_CONTROL_STATE_OFF, 
					0);
	}

	/*
		Check if we are converting to/from floating point formats.
	*/
	bSrcFloat = bDestFloat = IMG_FALSE;
	if (uSrcFmt == EURASIA_USE1_PCK_FMT_F16 ||
		uSrcFmt == EURASIA_USE1_PCK_FMT_F32)
	{
		bSrcFloat = IMG_TRUE;
	}
	if (uDestFmt == EURASIA_USE1_PCK_FMT_F16 ||
		uDestFmt == EURASIA_USE1_PCK_FMT_F32)
	{
		bDestFloat = IMG_TRUE;
	}

	/*
		Check for combinations of source and destination formats where the hardware
		supports converting an entire vector simultaneously.
	*/
	bSparseSwizzle = IMG_FALSE;
	if (bSrcFloat && bDestFloat)
	{
		bSparseSwizzle = IMG_TRUE;
	}
	if (bSrcFloat || bDestFloat)
	{
		if (uSrcFmt == EURASIA_USE1_PCK_FMT_C10 || uDestFmt == EURASIA_USE1_PCK_FMT_C10)
		{
			bSparseSwizzle = IMG_TRUE;
		}
		if ((uInst0 & EURASIA_USE0_PCK_SCALE) != 0)
		{
			if (uSrcFmt == EURASIA_USE1_PCK_FMT_U8 || uDestFmt == EURASIA_USE1_PCK_FMT_U8)
			{
				bSparseSwizzle = IMG_TRUE;
			}
		}
	}

	/*
		Check for cases where the hardware converts two channels then writes the converted data
		to both the first half and the second half of the destination.
	*/
	bReplicateConversion = IMG_FALSE;
	if (uDestFmt == EURASIA_USE1_PCK_FMT_U8 ||
		uDestFmt == EURASIA_USE1_PCK_FMT_S8 ||
		uDestFmt == EURASIA_USE1_PCK_FMT_O8)
	{
		bReplicateConversion = IMG_TRUE;
	}

	/*
		Decode the source swizzle.
	*/
	for (uChan = 0; uChan < 4; uChan++)
	{
		IMG_UINT32 uSel;
		IMG_UINT32 uComp;

		switch (uChan)
		{
			case 0: 
			{
				if (uSrcFmt == EURASIA_USE1_PCK_FMT_F32)
				{
					IMG_UINT32	uCompBit0;
					IMG_UINT32	uCompBit1;

					uCompBit0 = 
						(uInst0 & ~SGXVEC_USE0_PCK_F32SRC_C0SSEL_BIT0_CLRMSK) >> SGXVEC_USE0_PCK_F32SRC_C0SSEL_BIT0_SHIFT;
					uCompBit1 = 
						(uInst0 & ~SGXVEC_USE0_PCK_F32SRC_C0SSEL_BIT1_CLRMSK) >> SGXVEC_USE0_PCK_F32SRC_C0SSEL_BIT1_SHIFT;

					uComp = (uCompBit0 << 0) | (uCompBit1 << 1);
				}
				else
				{
					uComp = (uInst0 & ~SGXVEC_USE0_PCK_NONF32SRC_C0SSEL_CLRMSK) >> SGXVEC_USE0_PCK_NONF32SRC_C0SSEL_SHIFT; 
				}
				break;
			}
			case 1: uComp = (uInst0 & ~SGXVEC_USE0_PCK_C1SSEL_CLRMSK) >> SGXVEC_USE0_PCK_C1SSEL_SHIFT; break;
			case 2: uComp = (uInst0 & ~SGXVEC_USE0_PCK_C2SSEL_CLRMSK) >> SGXVEC_USE0_PCK_C2SSEL_SHIFT; break;
			case 3: uComp = (uInst0 & ~SGXVEC_USE0_PCK_C3SSEL_CLRMSK) >> SGXVEC_USE0_PCK_C3SSEL_SHIFT; break;
			default: IMG_ABORT();
		}

		switch (uComp)
		{
			case 0: uSel = USEASM_SWIZZLE_SEL_X; break;
			case 1: uSel = USEASM_SWIZZLE_SEL_Y; break;
			case 2: uSel = USEASM_SWIZZLE_SEL_Z; break;
			case 3: uSel = USEASM_SWIZZLE_SEL_W; break;
			default: IMG_ABORT();
		}

		auSwizzleSel[uChan] = uSel;
	}

	uSwizzle = 0;
	if (bSparseSwizzle)
	{
		for (uChan = 0; uChan < 4; uChan++)
		{
			uSwizzle |= (auSwizzleSel[uChan] << (USEASM_SWIZZLE_FIELD_SIZE * uChan));
		}
	}
	else
	{
		IMG_UINT32	uNextChan;

		uNextChan = 0;
		for (uChan = 0; uChan < 4; uChan++)
		{
			if ((uDestMask & (1 << uChan)) != 0)
			{
				IMG_UINT32	uSel;

				if (bReplicateConversion && uDestMask == USEREG_MASK_XYZW)
				{
					uSel = auSwizzleSel[uChan % 2];
				}
				else
				{
					uSel = auSwizzleSel[uNextChan];
					uNextChan++;
				}

				uSwizzle |= (uSel << (USEASM_SWIZZLE_FIELD_SIZE * uChan));
			}
		}
	}
	SetupSwizzleSource(&psInst->asArg[uSwizzleSrc], uSwizzle);

	return USEDISASM_OK;
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

static USEDISASM_ERROR DecodeSmp(PCSGX_CORE_DESC	psTarget,
							     IMG_UINT32		uInst0,
							     IMG_UINT32		uInst1,
							     PUSE_INST		psInst)
{
	static const USEASM_OPCODE aeDecodeOp[4][4] =
	{
		/* EURASIA_USE1_SMP_CDIM_U */
		{
			USEASM_OP_SMP1D,		/* EURASIA_USE1_SMP_LODM_NONE */
			USEASM_OP_SMP1DBIAS,	/* EURASIA_USE1_SMP_LODM_BIAS */
			USEASM_OP_SMP1DREPLACE,	/* EURASIA_USE1_SMP_LODM_REPLACE */
			USEASM_OP_SMP1DGRAD,	/* EURASIA_USE1_SMP_LODM_GRADIENTS */
		},
		/* EURASIA_USE1_SMP_CDIM_UV */
		{
			USEASM_OP_SMP2D,		/* EURASIA_USE1_SMP_LODM_NONE */
			USEASM_OP_SMP2DBIAS,	/* EURASIA_USE1_SMP_LODM_BIAS */
			USEASM_OP_SMP2DREPLACE,	/* EURASIA_USE1_SMP_LODM_REPLACE */
			USEASM_OP_SMP2DGRAD,	/* EURASIA_USE1_SMP_LODM_GRADIENTS */
		},
		/* EURASIA_USE1_SMP_CDIM_UVS */
		{
			USEASM_OP_SMP3D,		/* EURASIA_USE1_SMP_LODM_NONE */
			USEASM_OP_SMP3DBIAS,	/* EURASIA_USE1_SMP_LODM_BIAS */
			USEASM_OP_SMP3DREPLACE,	/* EURASIA_USE1_SMP_LODM_REPLACE */
			USEASM_OP_SMP3DGRAD,	/* EURASIA_USE1_SMP_LODM_GRADIENTS */
		},
		/* EURASIA_USE1_SMP_CDIM_RESERVED */
		{
			USEASM_OP_UNDEF,		/* EURASIA_USE1_SMP_LODM_NONE */
			USEASM_OP_UNDEF,		/* EURASIA_USE1_SMP_LODM_BIAS */
			USEASM_OP_UNDEF,		/* EURASIA_USE1_SMP_LODM_REPLACE */
			USEASM_OP_UNDEF,		/* EURASIA_USE1_SMP_LODM_GRADIENTS */
		},
	};
	IMG_UINT32		uMaskCount;
	IMG_UINT32		uEPred;
	IMG_UINT32		uCDim;
	IMG_UINT32		uLodMode;
	IMG_UINT32		uCType;
	PUSE_REGISTER	psNextArg;
	IMG_UINT32		uDRCSel;
	IMG_UINT32		uSrc0Num, uSrc1Num;
	IMG_BOOL		bDoubleRegisters;
	IMG_UINT32		uNumFieldLength;
	IMG_BOOL		bUseRepeatCount;

	UseAsmInitInst(psInst);

	/*
		Unpack the repeat mask/count from the instruction.
	*/
	uMaskCount = (uInst1 & ~EURASIA_USE1_RMSKCNT_CLRMSK) >> EURASIA_USE1_RMSKCNT_SHIFT;
	#if defined(SUPPORT_SGX_FEATURE_USE_SMP_REDUCEDREPEATCOUNT)
	if (ReducedSMPRepeatCount(psTarget))
	{
		uMaskCount = (uInst1 & ~EURASIA_USE1_SMP_RMSKCNT_CLRMSK) >> EURASIA_USE1_SMP_RMSKCNT_SHIFT;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_SMP_REDUCEDREPEATCOUNT) */

	/*
		Unpack the repeat mask/count selection.
	*/
	#if defined(SUPPORT_SGX_FEATURE_USE_SMP_NO_REPEAT_MASK)
	if (NoRepeatMaskOnSMPInstructions(psTarget))
	{
		bUseRepeatCount = IMG_TRUE;
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_SMP_NO_REPEAT_MASK) */
	{
		bUseRepeatCount = (uInst1 & EURASIA_USE1_RCNTSEL) != 0 ? IMG_TRUE : IMG_FALSE;
	}

	/*
		Decode instruction predicate.
	*/
	uEPred = (uInst1 & ~EURASIA_USE1_EPRED_CLRMSK) >> EURASIA_USE1_EPRED_SHIFT;
	psInst->uFlags1 |= (g_aeDecodeEPred[uEPred] << USEASM_OPFLAGS1_PRED_SHIFT);

	/*
		Decode instruction opcode.
	*/
	uCDim = (uInst1 & ~EURASIA_USE1_SMP_CDIM_CLRMSK) >> EURASIA_USE1_SMP_CDIM_SHIFT;
	uLodMode = (uInst1 & ~EURASIA_USE1_SMP_LODM_CLRMSK) >> EURASIA_USE1_SMP_LODM_SHIFT;
	psInst->uOpcode = aeDecodeOp[uCDim][uLodMode];
	if (psInst->uOpcode == USEASM_OP_UNDEF)
	{
		return USEDISASM_ERROR_INVALID_SMP_LOD_MODE;
	}

	/*
		Decode instruction modifiers.
	*/
	if (IsEnhancedNoSched(psTarget) && (uInst1 & EURASIA_USE1_SMP_NOSCHED))
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_SYNCSTART)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SYNCSTART;
	}

	#if defined(SUPPORT_SGX_FEATURE_TAG_RAWSAMPLE)
	/*
		Decode raw sample/sample info mode.
	*/
	if (SupportsRawSample(psTarget))
	{
		#if defined(SUPPORT_SGX_FEATURE_TAG_RAWSAMPLEBOTH)
		if (SupportsRawSampleBoth(psTarget))
		{
			IMG_UINT32	uSbMode;

			uSbMode = (uInst1 & ~SGXVEC_USE1_SMP_SBMODE_CLRMSK) >> SGXVEC_USE1_SMP_SBMODE_SHIFT;
			switch (uSbMode)
			{
				case SGXVEC_USE1_SMP_SBMODE_NONE: break;
				case SGXVEC_USE1_SMP_SBMODE_RAWDATA: psInst->uFlags3 |= USEASM_OPFLAGS3_SAMPLEDATA; break;
				case SGXVEC_USE1_SMP_SBMODE_COEFFS: psInst->uFlags3 |= USEASM_OPFLAGS3_SAMPLEINFO; break;
				case SGXVEC_USE1_SMP_SBMODE_BOTH: psInst->uFlags3 |= USEASM_OPFLAGS3_SAMPLEDATAANDINFO; break;
				default: IMG_ABORT();
			}
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_TAG_RAWSAMPLEBOTH) */
		#if defined(SUPPORT_SGX_FEATURE_TAG_RAWSAMPLEBOTH) && defined(SUPPORT_SGX_FEATURE_TAG_NOT_RAWSAMPLEBOTH)
		else
		#endif /* defined(SUPPORT_SGX_FEATURE_TAG_RAWSAMPLEBOTH) && defined(SUPPORT_SGX_FEATURE_TAG_NOT_RAWSAMPLEBOTH) */
		#if defined(SUPPORT_SGX_FEATURE_TAG_NOT_RAWSAMPLEBOTH)
		{
			IMG_UINT32	uSrd;

			uSrd = (uInst1 & ~SGX545_USE1_SMP_SRD_CLRMSK) >> SGX545_USE1_SMP_SRD_SHIFT;
			switch (uSrd)
			{
				case SGX545_USE1_SMP_SRD_NONE: break;
				case SGX545_USE1_SMP_SRD_RAWDATA: psInst->uFlags3 |= USEASM_OPFLAGS3_SAMPLEDATA; break;
				case SGX545_USE1_SMP_SRD_COEFFS: psInst->uFlags3 |= USEASM_OPFLAGS3_SAMPLEINFO; break;
				case SGX545_USE1_SMP_SRD_RESERVED: return USEDISASM_ERROR_INVALID_SMP_SRD_MODE;
			}
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_TAG_NOT_RAWSAMPLEBOTH) */
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_TAG_RAWSAMPLE) */

	#if defined(SUPPORT_SGX_FEATURE_TAG_PCF)
	/*
		Decode PCF mode.
	*/
	if (SupportsPCF(psTarget))
	{
		IMG_UINT32	uPCFMode;

		uPCFMode = (uInst1 & ~SGX545_USE1_SMP_PCF_CLRMSK) >> SGX545_USE1_SMP_PCF_SHIFT;
		switch (uPCFMode)
		{
			case SGX545_USE1_SMP_PCF_NONE: break;
			case SGX545_USE1_SMP_PCF_F16: psInst->uFlags3 |= USEASM_OPFLAGS3_PCFF16; break;
			case SGX545_USE1_SMP_PCF_F32: psInst->uFlags3 |= USEASM_OPFLAGS3_PCFF32; break;
			case SGX545_USE1_SMP_PCF_RESERVED: return USEDISASM_ERROR_INVALID_SMP_PCF_MODE;
			default: IMG_ABORT();
		}
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_TAG_PCF) */

	#if defined(SUPPORT_SGX_FEATURE_USE_SMP_RESULT_FORMAT_CONVERT)
	if (SupportsSMPResultFormatConvert(psTarget) && (uInst1 & SGXVEC_USE1_SMP_MINPACK))
	{
		psInst->uFlags3 |= USEASM_OPFLAGS3_MINPACK;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_SMP_RESULT_FORMAT_CONVERT) */

	/*
		Copy the repeat count/mask to the input instruction.
	*/
	if (bUseRepeatCount)
	{	
		psInst->uFlags1 |= ((uMaskCount + 1) << USEASM_OPFLAGS1_REPEAT_SHIFT);
		psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);
	}
	else
	{
		psInst->uFlags1 |= (uMaskCount << USEASM_OPFLAGS1_MASK_SHIFT);
	}
	
	/*
		Decode destination.
	*/
	if (uInst1 & EURASIA_USE1_SMP_DBANK_PRIMATTR)
	{
		psInst->asArg[0].uType = USEASM_REGTYPE_PRIMATTR;
	}
	else
	{
		psInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
	}
	psInst->asArg[0].uNumber = (uInst0 & ~EURASIA_USE0_DST_CLRMSK) >> EURASIA_USE0_DST_SHIFT;

	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	/*
		The source arguments are in 64-bit units for cores with the vector instruction set.
	*/
	if (SupportsVEC34(psTarget))
	{
		bDoubleRegisters = IMG_TRUE;
		uNumFieldLength = SGXVEC_USE_SMP_SRC_FIELD_LENGTH;
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
	{
		bDoubleRegisters = IMG_FALSE;
		uNumFieldLength = EURASIA_USE_REGISTER_NUMBER_BITS;
	}
	
	/*
		Decode coordinate source.
	*/
	uSrc0Num = (IMG_UINT32)((uInst0 & ~EURASIA_USE0_SRC0_CLRMSK) >> EURASIA_USE0_SRC0_SHIFT);
	DecodeSrc0WithNum(psTarget,
					  bDoubleRegisters,
					  uInst1,
					  IMG_TRUE /* bAllowExtended */,
					  EURASIA_USE1_S0BEXT /* uExt */,
					  &psInst->asArg[1],
					  IMG_FALSE /* bFmtControl */,
					  0 /* uAltFmtFlag */,
					  uNumFieldLength,
					  uSrc0Num);

	/*
		Decode coordinate type flag.
	*/
	uCType = (uInst1 & ~EURASIA_USE1_SMP_CTYPE_CLRMSK) >> EURASIA_USE1_SMP_CTYPE_SHIFT;
	switch (uCType)
	{
		case EURASIA_USE1_SMP_CTYPE_F32: break;
		case EURASIA_USE1_SMP_CTYPE_F16: psInst->asArg[1].uFlags |= USEASM_ARGFLAGS_FMTF16; break;
		case EURASIA_USE1_SMP_CTYPE_C10: psInst->asArg[1].uFlags |= USEASM_ARGFLAGS_FMTC10; break;
		case EURASIA_USE1_SMP_CTYPE_RESERVED: return USEDISASM_ERROR_INVALID_SMP_DATA_TYPE;
		default: IMG_ABORT();
	}
	
	/*
		Decode state source.
	*/
	uSrc1Num = (uInst0 & ~EURASIA_USE0_SRC1_CLRMSK) >> EURASIA_USE0_SRC1_SHIFT;
	DecodeSrc12WithNum(psTarget,
					   bDoubleRegisters,
					   uInst0,
					   uInst1,
					   1 /* uSrc */,
					   IMG_TRUE /* bAllowExtended */,
					   EURASIA_USE1_S1BEXT /* uExt */,
					   &psInst->asArg[2],
					   IMG_FALSE /* bFmtControl */,
					   0 /* uAltFmtFlag */,
					   uNumFieldLength,
					   uSrc1Num);
	
	/*
		Decode LOD bias/LOD replace/gradients source.
	*/
	psNextArg = &psInst->asArg[3];
	if (uLodMode != EURASIA_USE1_SMP_LODM_NONE)
	{
		IMG_UINT32	uSrc2Num;

		uSrc2Num = (uInst0 & ~EURASIA_USE0_SRC2_CLRMSK) >> EURASIA_USE0_SRC2_SHIFT;
		DecodeSrc12WithNum(psTarget,
						   bDoubleRegisters,
						   uInst0,
						   uInst1,
						   2 /* uSrc */,
						   IMG_TRUE /* bAllowExtended */,
						   EURASIA_USE1_S2BEXT /* uExt */,
						   psNextArg,
						   IMG_FALSE /* bFmtControl */,
						   0 /* uAltFmtFlag */,
						   uNumFieldLength,
						   uSrc2Num);
		psNextArg++;
	}


	/*
		Decode the dependent read counter.
	*/
	uDRCSel = (uInst1 & ~EURASIA_USE1_SMP_DRCSEL_CLRMSK) >> EURASIA_USE1_SMP_DRCSEL_SHIFT;
	SetupDRCSource(psNextArg, uDRCSel);
	psNextArg++;

	#if defined(SUPPORT_SGX_FEATURE_USE_SMP_RESULT_FORMAT_CONVERT)
	/*
		Decode the format conversion argument.
	*/
	if (SupportsSMPResultFormatConvert(psTarget))
	{
		static const USEASM_INTSRCSEL aeFConv[] = 
		{
			USEASM_INTSRCSEL_NONE,	/* SGXVEC_USE1_SMP_FCONV_NONE */
			USEASM_INTSRCSEL_UNDEF,	/* SGXVEC_USE1_SMP_FCONV_RESERVED */
			USEASM_INTSRCSEL_F16,	/* SGXVEC_USE1_SMP_FCONV_F16 */
			USEASM_INTSRCSEL_F32,	/* SGXVEC_USE1_SMP_FCONV_F32 */
		};
		IMG_UINT32 uFConv;

		uFConv = (uInst1 & ~SGXVEC_USE1_SMP_FCONV_CLRMSK) >> SGXVEC_USE1_SMP_FCONV_SHIFT;
		if (aeFConv[uFConv] == USEASM_INTSRCSEL_UNDEF)
		{
			return USEDISASM_ERROR_INVALID_SMP_FORMAT_CONVERSION;
		}
		SetupIntSrcSel(psNextArg, aeFConv[uFConv]);
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_SMP_RESULT_FORMAT_CONVERT) */
	{
		SetupIntSrcSel(psNextArg, USEASM_INTSRCSEL_NONE);
	}

	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeFIRHInstruction(PCSGX_CORE_DESC	psTarget,
										     IMG_UINT32		uInst0,
										     IMG_UINT32		uInst1,
										     PUSE_INST		psInst)
{
	static const USEASM_INTSRCSEL aeSrcFormat[] = 
	{
		USEASM_INTSRCSEL_U8,	/* EURASIA_USE1_FIRH_SRCFORMAT_U8 */ 
		USEASM_INTSRCSEL_S8,	/* EURASIA_USE1_FIRH_SRCFORMAT_S8 */ 
		USEASM_INTSRCSEL_O8,	/* EURASIA_USE1_FIRH_SRCFORMAT_O8 */ 
		USEASM_INTSRCSEL_UNDEF,	/* EURASIA_USE1_FIRH_SRCFORMAT_RESERVED */
	};
	USEASM_PRED ePredicate;

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*
		Decode predicate.
	*/
	ePredicate = g_aeDecodeSPred[(uInst1 & ~EURASIA_USE1_SPRED_CLRMSK) >> EURASIA_USE1_SPRED_SHIFT];
	psInst->uFlags1 |= (ePredicate << USEASM_OPFLAGS1_PRED_SHIFT);

	/*	
		Decode instruction modifiers.
	*/
	if (uInst1 & EURASIA_USE1_END)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_INT_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	
	if (IsHighPrecisionFIR(psTarget) && (uInst1 & EURASIA_USE1_FIRHH_HIPRECISION))
	{
		IMG_UINT32 uRCount = (uInst1 & ~EURASIA_USE1_FIRHH_RCOUNT_CLRMSK) >> EURASIA_USE1_FIRHH_RCOUNT_SHIFT;
		IMG_UINT32 uSOff;
		IMG_UINT32 uFormat;
		IMG_UINT32 uPOff;
		IMG_UINT32 uEdgeMode;

		/*	
			Set instruction opcode.
		*/
		psInst->uOpcode = USEASM_OP_FIRHH;

		/*
			Decode instruction modifiers.
		*/
		if (uInst1 & EURASIA_USE1_FIRHH_MOEPOFF)
		{
			psInst->uFlags2 |= USEASM_OPFLAGS2_MOEPOFF;
		}

		/*
			Decode repeat count.
		*/
		psInst->uFlags1 |= ((uRCount + 1) << USEASM_OPFLAGS1_REPEAT_SHIFT);
		psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

		/*
			Decode destination.
		*/
		psInst->asArg[0].uType = USEASM_REGTYPE_FPINTERNAL;
		psInst->asArg[0].uNumber = (uInst0 & ~EURASIA_USE0_DST_CLRMSK) >> EURASIA_USE0_DST_SHIFT;

		/*
			Decode source 0.
		*/
		DecodeSrc12(psTarget, uInst0, uInst1, 0, TRUE, EURASIA_USE1_S1BEXT, &psInst->asArg[1], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);

		/*
			Decode source 1.
		*/
		DecodeSrc12(psTarget, uInst0, uInst1, 1, TRUE, EURASIA_USE1_S1BEXT, &psInst->asArg[2], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
		
		/*
			Decode source 2.
		*/
		DecodeSrc12(psTarget, uInst0, uInst1, 2, TRUE, EURASIA_USE1_S2BEXT, &psInst->asArg[3], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
		
		/*
			Decode source format.
		*/
		uFormat = (uInst1 & ~EURASIA_USE1_FIRHH_SRCFORMAT_CLRMSK) >> EURASIA_USE1_FIRHH_SRCFORMAT_SHIFT;
		SetupIntSrcSel(&psInst->asArg[4], aeSrcFormat[uFormat]);

		/*
			Decode edge mode.
		*/
		uEdgeMode = (uInst1 & ~EURASIA_USE1_FIRHH_EDGEMODE_CLRMSK) >> EURASIA_USE1_FIRHH_EDGEMODE_SHIFT;
		SetupImmediateSource(&psInst->asArg[5], uEdgeMode);

		/*
			Decode filter coefficent set.
		*/
		psInst->asArg[6].uType = USEASM_REGTYPE_FILTERCOEFF;
		psInst->asArg[6].uNumber = (uInst1 & ~EURASIA_USE1_FIRHH_COEFSEL_CLRMSK) >> EURASIA_USE1_FIRHH_COEFSEL_SHIFT;

		/*
			Decode source offset.
		*/
		uSOff = (((uInst1 & ~EURASIA_USE1_FIRHH_SOFFL_CLRMSK) >> EURASIA_USE1_FIRHH_SOFFL_SHIFT) << 0) |
				(((uInst1 & ~EURASIA_USE1_FIRHH_SOFFH_CLRMSK) >> EURASIA_USE1_FIRHH_SOFFH_SHIFT) << 2) |
				(((uInst1 & ~EURASIA_USE1_FIRHH_SOFFS_CLRMSK) >> EURASIA_USE1_FIRHH_SOFFS_SHIFT) << 4);
		SetupImmediateSource(&psInst->asArg[7], SignExt(uSOff, 4));

		/*
			Decode packing register offset.
		*/
		uPOff = (uInst1 & ~EURASIA_USE1_FIRHH_POFF_CLRMSK) >> EURASIA_USE1_FIRHH_POFF_SHIFT;
		SetupImmediateSource(&psInst->asArg[8], uPOff);
	}
	else
	{
		IMG_UINT32 uRCount = (uInst1 & ~EURASIA_USE1_FIRH_RCOUNT_CLRMSK) >> EURASIA_USE1_FIRH_RCOUNT_SHIFT;
		IMG_UINT32 uSOff;
		IMG_UINT32 uSrcFormat;
		IMG_UINT32 uEdgeMode;
		IMG_UINT32 uPOff;

		/*	
			Set instruction opcode.
		*/
		psInst->uOpcode = USEASM_OP_FIRH;

		/*
			Decode repeat count.
		*/
		psInst->uFlags1 |= ((uRCount + 1) << USEASM_OPFLAGS1_REPEAT_SHIFT);
		psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);
		
		/*
			Decode destination.
		*/
		DecodeDest(psTarget, uInst0, uInst1, TRUE, &psInst->asArg[0], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
		
		/*
			Decode source 0.
		*/
		DecodeSrc12(psTarget, uInst0, uInst1, 0, TRUE, EURASIA_USE1_S1BEXT, &psInst->asArg[1], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
		
		/*
			Decode source 1.
		*/
		DecodeSrc12(psTarget, uInst0, uInst1, 1, TRUE, EURASIA_USE1_S1BEXT, &psInst->asArg[2], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
		
		/*
			Decode source 2.
		*/
		DecodeSrc12(psTarget, uInst0, uInst1, 2, TRUE, EURASIA_USE1_S2BEXT, &psInst->asArg[3], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);

		/*
			Decode source format.
		*/
		uSrcFormat = (uInst1 & ~EURASIA_USE1_FIRH_SRCFORMAT_CLRMSK) >> EURASIA_USE1_FIRH_SRCFORMAT_SHIFT;
		if (aeSrcFormat[uSrcFormat] == USEASM_INTSRCSEL_UNDEF)
		{
			return USEDISASM_ERROR_INVALID_FIRH_SOURCE_FORMAT;
		}
		SetupIntSrcSel(&psInst->asArg[4], aeSrcFormat[uSrcFormat]);

		/*
			Decode edge mode.
		*/
		uEdgeMode = (uInst1 & ~EURASIA_USE1_FIRH_EDGEMODE_CLRMSK) >> EURASIA_USE1_FIRH_EDGEMODE_SHIFT;
		SetupImmediateSource(&psInst->asArg[5], uEdgeMode);

		/*
			Decode filter coefficent set.
		*/
		psInst->asArg[6].uType = USEASM_REGTYPE_FILTERCOEFF;
		psInst->asArg[6].uNumber = (uInst1 & ~EURASIA_USE1_FIRH_COEFSEL_CLRMSK) >> EURASIA_USE1_FIRH_COEFSEL_SHIFT;

		/*
			Decode source offset.
		*/
		uSOff = (((uInst1 & ~EURASIA_USE1_FIRH_SOFFL_CLRMSK) >> EURASIA_USE1_FIRH_SOFFL_SHIFT) << 0) |
				(((uInst1 & ~EURASIA_USE1_FIRH_SOFFH_CLRMSK) >> EURASIA_USE1_FIRH_SOFFH_SHIFT) << 2) |
				(((uInst1 & ~EURASIA_USE1_FIRH_SOFFS_CLRMSK) >> EURASIA_USE1_FIRH_SOFFS_SHIFT) << 4);
		SetupImmediateSource(&psInst->asArg[7], SignExt(uSOff, 4));

		/*
			Decode packing register offset.
		*/
		uPOff = (uInst1 & ~EURASIA_USE1_FIRH_POFF_CLRMSK) >> EURASIA_USE1_FIRH_POFF_SHIFT;
		SetupImmediateSource(&psInst->asArg[8], uPOff);
	}
	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodePCKInstruction(PCSGX_CORE_DESC	psTarget,
										    IMG_UINT32		uInst0,
										    IMG_UINT32		uInst1,
										    PUSE_INST		psInst)
{
	static const USEASM_OPCODE aeOp[8][8] =
	{
		/* EURASIA_USE1_PCK_FMT_U8 */
		{
			USEASM_OP_UNPCKU8U8,	/* EURASIA_USE1_PCK_FMT_U8 */
			USEASM_OP_UNPCKU8S8,	/* EURASIA_USE1_PCK_FMT_S8 */
			USEASM_OP_UNDEF,		/* EURASIA_USE1_PCK_FMT_O8 */
			USEASM_OP_PCKU8U16,		/* EURASIA_USE1_PCK_FMT_U16 */
			USEASM_OP_PCKU8S16,		/* EURASIA_USE1_PCK_FMT_S16 */ 
			USEASM_OP_PCKU8F16,		/* EURASIA_USE1_PCK_FMT_F16 */
			USEASM_OP_PCKU8F32,		/* EURASIA_USE1_PCK_FMT_F32 */
			USEASM_OP_UNDEF,		/* EURASIA_USE1_PCK_FMT_C10 */
		},
		/* EURASIA_USE1_PCK_FMT_S8 */
		{
			USEASM_OP_UNPCKS8U8,	/* EURASIA_USE1_PCK_FMT_U8 */
			USEASM_OP_UNDEF,		/* EURASIA_USE1_PCK_FMT_S8 */
			USEASM_OP_UNDEF,		/* EURASIA_USE1_PCK_FMT_O8 */
			USEASM_OP_PCKS8U16,		/* EURASIA_USE1_PCK_FMT_U16 */
			USEASM_OP_PCKS8S16,		/* EURASIA_USE1_PCK_FMT_S16 */ 
			USEASM_OP_PCKS8F16,		/* EURASIA_USE1_PCK_FMT_F16 */
			USEASM_OP_PCKS8F32,		/* EURASIA_USE1_PCK_FMT_F32 */
			USEASM_OP_UNDEF,		/* EURASIA_USE1_PCK_FMT_C10 */
		},
		/* EURASIA_USE1_PCK_FMT_O8 */
		{
			USEASM_OP_UNDEF,		/* EURASIA_USE1_PCK_FMT_U8 */
			USEASM_OP_UNDEF,		/* EURASIA_USE1_PCK_FMT_S8 */
			USEASM_OP_UNDEF,		/* EURASIA_USE1_PCK_FMT_O8 */
			USEASM_OP_PCKO8U16,		/* EURASIA_USE1_PCK_FMT_U16 */
			USEASM_OP_PCKO8S16,		/* EURASIA_USE1_PCK_FMT_S16 */ 
			USEASM_OP_PCKO8F16,		/* EURASIA_USE1_PCK_FMT_F16 */
			USEASM_OP_PCKO8F32,		/* EURASIA_USE1_PCK_FMT_F32 */
			USEASM_OP_UNDEF,		/* EURASIA_USE1_PCK_FMT_C10 */
		},
		/* EURASIA_USE1_PCK_FMT_U16 */
		{
			USEASM_OP_UNPCKU16U8,	/* EURASIA_USE1_PCK_FMT_U8 */
			USEASM_OP_UNPCKU16S8,	/* EURASIA_USE1_PCK_FMT_S8 */
			USEASM_OP_UNPCKU16O8,	/* EURASIA_USE1_PCK_FMT_O8 */
			USEASM_OP_PCKU16U16,	/* EURASIA_USE1_PCK_FMT_U16 */
			USEASM_OP_PCKU16S16,	/* EURASIA_USE1_PCK_FMT_S16 */ 
			USEASM_OP_PCKU16F16,	/* EURASIA_USE1_PCK_FMT_F16 */
			USEASM_OP_PCKU16F32,	/* EURASIA_USE1_PCK_FMT_F32 */
			USEASM_OP_UNPCKU16C10,	/* EURASIA_USE1_PCK_FMT_C10 */
		},
		/* EURASIA_USE1_PCK_FMT_S16 */
		{
			USEASM_OP_UNPCKS16U8,	/* EURASIA_USE1_PCK_FMT_U8 */
			USEASM_OP_UNPCKS16S8,	/* EURASIA_USE1_PCK_FMT_S8 */
			USEASM_OP_UNPCKS16O8,	/* EURASIA_USE1_PCK_FMT_O8 */
			USEASM_OP_PCKS16U16,	/* EURASIA_USE1_PCK_FMT_U16 */
			USEASM_OP_PCKS16S16,	/* EURASIA_USE1_PCK_FMT_S16 */ 
			USEASM_OP_PCKS16F16,	/* EURASIA_USE1_PCK_FMT_F16 */
			USEASM_OP_PCKS16F32,	/* EURASIA_USE1_PCK_FMT_F32 */
			USEASM_OP_UNPCKS16C10,	/* EURASIA_USE1_PCK_FMT_C10 */
		},
		/* EURASIA_USE1_PCK_FMT_F16 */
		{
			USEASM_OP_UNPCKF16U8,	/* EURASIA_USE1_PCK_FMT_U8 */
			USEASM_OP_UNPCKF16S8,	/* EURASIA_USE1_PCK_FMT_S8 */
			USEASM_OP_UNPCKF16O8,	/* EURASIA_USE1_PCK_FMT_O8 */
			USEASM_OP_PCKF16U16,	/* EURASIA_USE1_PCK_FMT_U16 */
			USEASM_OP_PCKF16S16,	/* EURASIA_USE1_PCK_FMT_S16 */ 
			USEASM_OP_PCKF16F16,	/* EURASIA_USE1_PCK_FMT_F16 */
			USEASM_OP_PCKF16F32,	/* EURASIA_USE1_PCK_FMT_F32 */
			USEASM_OP_UNPCKF16C10,	/* EURASIA_USE1_PCK_FMT_C10 */
		},
		/* EURASIA_USE1_PCK_FMT_F32 */
		{
			USEASM_OP_UNPCKF32U8,	/* EURASIA_USE1_PCK_FMT_U8 */
			USEASM_OP_UNPCKF32S8,	/* EURASIA_USE1_PCK_FMT_S8 */
			USEASM_OP_UNPCKF32O8,	/* EURASIA_USE1_PCK_FMT_O8 */
			USEASM_OP_UNPCKF32U16,	/* EURASIA_USE1_PCK_FMT_U16 */
			USEASM_OP_UNPCKF32S16,	/* EURASIA_USE1_PCK_FMT_S16 */ 
			USEASM_OP_UNPCKF32F16,	/* EURASIA_USE1_PCK_FMT_F16 */
			USEASM_OP_UNPCKF32F32,	/* EURASIA_USE1_PCK_FMT_F32 */
			USEASM_OP_UNPCKF32C10,	/* EURASIA_USE1_PCK_FMT_C10 */
		},
		/* EURASIA_USE1_PCK_FMT_C10 */
		{
			USEASM_OP_UNDEF,		/* EURASIA_USE1_PCK_FMT_U8 */
			USEASM_OP_UNDEF,		/* EURASIA_USE1_PCK_FMT_S8 */
			USEASM_OP_UNDEF,		/* EURASIA_USE1_PCK_FMT_O8 */
			USEASM_OP_PCKC10U16,	/* EURASIA_USE1_PCK_FMT_U16 */
			USEASM_OP_PCKC10S16,	/* EURASIA_USE1_PCK_FMT_S16 */ 
			USEASM_OP_PCKC10F16,	/* EURASIA_USE1_PCK_FMT_F16 */
			USEASM_OP_PCKC10F32,	/* EURASIA_USE1_PCK_FMT_F32 */
			USEASM_OP_UNPCKC10C10,	/* EURASIA_USE1_PCK_FMT_C10 */
		},
	};
	IMG_UINT32 uEPred;
	IMG_UINT32 uDestMask = (uInst1 & ~EURASIA_USE1_PCK_DMASK_CLRMSK) >> EURASIA_USE1_PCK_DMASK_SHIFT;
	IMG_UINT32 uDestFmt = (uInst1 & ~EURASIA_USE1_PCK_DSTF_CLRMSK) >> EURASIA_USE1_PCK_DSTF_SHIFT;
	IMG_UINT32 uSrcFmt = (uInst1 & ~EURASIA_USE1_PCK_SRCF_CLRMSK) >> EURASIA_USE1_PCK_SRCF_SHIFT;
	IMG_UINT32 uSrc1Comp, uSrc2Comp;

	UseAsmInitInst(psInst);

	/*
		Decode predicate.
	*/
	uEPred = (uInst1 & ~EURASIA_USE1_EPRED_CLRMSK) >> EURASIA_USE1_EPRED_SHIFT;
	psInst->uFlags1 |= (g_aeDecodeEPred[uEPred] << USEASM_OPFLAGS1_PRED_SHIFT);

	/*
		Decode opcode.
	*/
	psInst->uOpcode = aeOp[uDestFmt][uSrcFmt];
	if (psInst->uOpcode == USEASM_OP_UNDEF)
	{
		return USEDISASM_ERROR_INVALID_PCK_FORMAT_COMBINATION;
	}

	/*
		Decode instruction flags.
	*/
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_SYNCSTART)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SYNCSTART;
	}
	if (uInst0 & EURASIA_USE0_PCK_SCALE)
	{
		if (HasPackMultipleRoundModes(psTarget) && 
			(uSrcFmt == EURASIA_USE1_PCK_FMT_F16 || uSrcFmt == EURASIA_USE1_PCK_FMT_F32) &&
			(uDestFmt == EURASIA_USE1_PCK_FMT_F16 || uDestFmt == EURASIA_USE1_PCK_FMT_F32))
		{
			psInst->uFlags1 |= USEASM_OPFLAGS1_ROUNDENABLE;
		}
		else
		{
			psInst->uFlags2 |= USEASM_OPFLAGS2_SCALE;
		}
	}
	if (uInst1 & EURASIA_USE1_END)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}
	if (uInst1 & EURASIA_USE1_PCK_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	DecodeRepeatCountMask(uInst1, psInst);

	/*
		Decode destination register.
	*/
	DecodeDest(psTarget, uInst0, uInst1, TRUE, &psInst->asArg[0], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	if (uDestMask != 0xF)
	{
		psInst->asArg[0].uFlags |= USEASM_ARGFLAGS_BYTEMSK_PRESENT;
		psInst->asArg[0].uFlags |= (uDestMask << USEASM_ARGFLAGS_BYTEMSK_SHIFT);
	}
	
	/*
		Decode first source register.
	*/
	DecodeSrc12(psTarget, uInst0, uInst1, 1, TRUE, EURASIA_USE1_S1BEXT, &psInst->asArg[1], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	uSrc1Comp = (uInst0 & ~EURASIA_USE0_PCK_S1SCSEL_CLRMSK) >> EURASIA_USE0_PCK_S1SCSEL_SHIFT;
	psInst->asArg[1].uFlags |= (uSrc1Comp << USEASM_ARGFLAGS_COMP_SHIFT);

	/*
		Decode second source register.
	*/
	DecodeSrc12(psTarget, uInst0, uInst1, 2, TRUE, EURASIA_USE1_S2BEXT, &psInst->asArg[2], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	uSrc2Comp = (uInst0 & ~EURASIA_USE0_PCK_S2SCSEL_CLRMSK) >> EURASIA_USE0_PCK_S2SCSEL_SHIFT;
	psInst->asArg[2].uFlags |= (uSrc2Comp << USEASM_ARGFLAGS_COMP_SHIFT);

	#if defined(SUPPORT_SGX_FEATURE_USE_PACK_MULTIPLE_ROUNDING_MODES)
	/*
		Decode rounding mode.
	*/
	if (HasPackMultipleRoundModes(psTarget))
	{
		IMG_UINT32	uRMode = (uInst0 & ~ SGX545_USE0_PCK_RMODE_CLRMSK) >>  SGX545_USE0_PCK_RMODE_SHIFT;
		static const USEASM_INTSRCSEL aeRMode[] = {USEASM_INTSRCSEL_ROUNDNEAREST,	/* SGX545_USE0_PCK_RMODE_NEAREST */
												   USEASM_INTSRCSEL_ROUNDDOWN,		/* SGX545_USE0_PCK_RMODE_DOWN */
												   USEASM_INTSRCSEL_ROUNDUP,		/* SGX545_USE0_PCK_RMODE_UP */ 
												   USEASM_INTSRCSEL_ROUNDZERO,		/* SGX545_USE0_PCK_RMODE_ZERO */};

		SetupIntSrcSel(&psInst->asArg[3], aeRMode[uRMode]);
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_PACK_MULTIPLE_ROUNDING_MODES) */
	{
		SetupIntSrcSel(&psInst->asArg[3], USEASM_INTSRCSEL_ROUNDNEAREST);
	}

	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeBitwiseInstruction(PCSGX_CORE_DESC	psTarget,
												IMG_UINT32		uInst0,
												IMG_UINT32		uInst1,
												IMG_UINT32		uOpcode,
												PUSE_INST		psInst)
{
	IMG_UINT32 uOp2 = (uInst1 & ~EURASIA_USE1_BITWISE_OP2_CLRMSK) >> EURASIA_USE1_BITWISE_OP2_SHIFT;
	IMG_UINT32 uSrc2Bank = (uInst0 & ~EURASIA_USE0_S2BANK_CLRMSK) >> EURASIA_USE0_S2BANK_SHIFT;
	static const USEASM_OPCODE aeAndOr[] = {USEASM_OP_AND, USEASM_OP_OR};
	static const USEASM_OPCODE aeXor[] = {USEASM_OP_XOR, USEASM_OP_UNDEF};
	static const USEASM_OPCODE aeShlRol[] = {USEASM_OP_SHL, USEASM_OP_ROL};
	static const USEASM_OPCODE aeShrAsr[] = {USEASM_OP_SHR, USEASM_OP_ASR};
	USEASM_PRED ePredicate;

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*
		Decode instruction opcode.
	*/
	switch (uOpcode)
	{
		case EURASIA_USE1_OP_ANDOR: psInst->uOpcode = aeAndOr[uOp2]; break;
		case EURASIA_USE1_OP_XOR: psInst->uOpcode = aeXor[uOp2]; break;
		case EURASIA_USE1_OP_SHLROL: psInst->uOpcode = aeShlRol[uOp2]; break;
		case EURASIA_USE1_OP_SHRASR: psInst->uOpcode = aeShrAsr[uOp2]; break;
	}
	if (psInst->uOpcode == USEASM_OP_UNDEF)
	{
		return USEDISASM_ERROR_INVALID_BITWISE_OP2;
	}

	/*
		Decode predicate.
	*/
	ePredicate = g_aeDecodeEPred[(uInst1 & ~EURASIA_USE1_EPRED_CLRMSK) >> EURASIA_USE1_EPRED_SHIFT];
	psInst->uFlags1 |= (ePredicate << USEASM_OPFLAGS1_PRED_SHIFT);

	/*
		Decode instruction flags.
	*/
	#if defined(SUPPORT_SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK)
	if (NoRepeatMaskOnBitwiseInstructions(psTarget))
	{
		IMG_UINT32	uMaskCount = (uInst1 & ~SGXVEC_USE1_BITWISE_RCOUNT_CLRMSK) >> SGXVEC_USE1_BITWISE_RCOUNT_SHIFT;

		psInst->uFlags1 |= ((uMaskCount + 1) << USEASM_OPFLAGS1_REPEAT_SHIFT);
		psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK) */
	{
		DecodeRepeatCountMask(uInst1, psInst);
	}
	if (IsEnhancedNoSched(psTarget) && (uInst1 & EURASIA_USE1_BITWISE_NOSCHED))
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_SYNCSTART)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SYNCSTART;
	}
	if (uInst1 & EURASIA_USE1_BITWISE_PARTIAL)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_PARTIAL;
	}
	if (uInst1 & EURASIA_USE1_END)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}
	
	/*
		Decode instruction destination.
	*/
	DecodeDest(psTarget, uInst0, uInst1, TRUE, &psInst->asArg[0], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);

	/*
		Decode instruction source 1.
	*/
	DecodeSrc12(psTarget, uInst0, uInst1, 1, TRUE, EURASIA_USE1_S1BEXT, &psInst->asArg[1], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);

	/*
		Decode instruction source 2.
	*/
	if ((uInst1 & EURASIA_USE1_S2BEXT) && uSrc2Bank == EURASIA_USE0_S2EXTBANK_IMMEDIATE)
	{
		IMG_UINT32 uNumber, uRot;

		uNumber = ((uInst0 & ~EURASIA_USE0_SRC2_CLRMSK) >> EURASIA_USE0_SRC2_SHIFT);
		uNumber |= 
			((uInst0 & ~EURASIA_USE0_BITWISE_SRC2IEXTLPSEL_CLRMSK) >> EURASIA_USE0_BITWISE_SRC2IEXTLPSEL_SHIFT) << 7;
		uNumber |=
			((uInst1 & ~EURASIA_USE1_BITWISE_SRC2IEXTH_CLRMSK) >> EURASIA_USE1_BITWISE_SRC2IEXTH_SHIFT) << 14;
		uRot = ((uInst1 & ~EURASIA_USE1_BITWISE_SRC2ROT_CLRMSK) >> EURASIA_USE1_BITWISE_SRC2ROT_SHIFT);
		SetupImmediateSource(&psInst->asArg[2], RotateLeft(uNumber, uRot));
	}
	else
	{
		DecodeSrc12(psTarget, uInst0, uInst1, 2, TRUE, EURASIA_USE1_S2BEXT, &psInst->asArg[2], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	}

	/*
		Decode invert flag on source 2.
	*/
	if (uInst1 & EURASIA_USE1_BITWISE_SRC2INV)
	{
		psInst->asArg[2].uFlags |= USEASM_ARGFLAGS_INVERT;
	}

	return USEDISASM_OK;
}

static USEASM_OPCODE DecodeTestOpcode(PCSGX_CORE_DESC	psTarget,
									  #if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
									  IMG_UINT32		uPrec,
									  #endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
									  IMG_UINT32		uAluSel,
									  IMG_UINT32		uAluOp)
{
#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	static const USEASM_OPCODE aeVecF16Opcode[16] =
	{
		USEASM_OP_UNDEF,		/* SGXVEC_USE0_TEST_ALUOP_F16_RESERVED0 */
		USEASM_OP_UNDEF,		/* SGXVEC_USE0_TEST_ALUOP_F16_RESERVED1 */
		USEASM_OP_VF16ADD,		/* SGXVEC_USE0_TEST_ALUOP_F16_VADD */
		USEASM_OP_VF16FRC,		/* SGXVEC_USE0_TEST_ALUOP_F16_VFRC */
		USEASM_OP_VRCP,			/* SGXVEC_USE0_TEST_ALUOP_F16_FRCP */
		USEASM_OP_VRSQ,			/* SGXVEC_USE0_TEST_ALUOP_F16_FRSQ */
		USEASM_OP_VLOG,			/* SGXVEC_USE0_TEST_ALUOP_F16_FLOG */
		USEASM_OP_VEXP,			/* SGXVEC_USE0_TEST_ALUOP_F16_FEXP */
		USEASM_OP_VF16DP,		/* SGXVEC_USE0_TEST_ALUOP_F16_VDP */
		USEASM_OP_VF16MIN,		/* SGXVEC_USE0_TEST_ALUOP_F16_VMIN */
		USEASM_OP_VF16MAX,		/* SGXVEC_USE0_TEST_ALUOP_F16_VMAX */
		USEASM_OP_VF16DSX,		/* SGXVEC_USE0_TEST_ALUOP_F16_VDSX */
		USEASM_OP_VF16DSY,		/* SGXVEC_USE0_TEST_ALUOP_F16_VDSY */
		USEASM_OP_VF16MUL,		/* SGXVEC_USE0_TEST_ALUOP_F16_VMUL */
		USEASM_OP_VF16SUB,		/* SGXVEC_USE0_TEST_ALUOP_F16_VSUB */
		USEASM_OP_UNDEF,		/* SGXVEC_USE0_TEST_ALUOP_F16_RESERVED2 */
	};
	static const USEASM_OPCODE aeVecF32Opcode[16] =
	{
		USEASM_OP_UNDEF,		/* SGXVEC_USE0_TEST_ALUOP_F32_RESERVED0 */
		USEASM_OP_UNDEF,		/* SGXVEC_USE0_TEST_ALUOP_F32_RESERVED1 */
		USEASM_OP_VADD,			/* SGXVEC_USE0_TEST_ALUOP_F32_VADD */
		USEASM_OP_VFRC,			/* SGXVEC_USE0_TEST_ALUOP_F32_VFRC */
		USEASM_OP_VRCP,			/* SGXVEC_USE0_TEST_ALUOP_F32_FRCP */
		USEASM_OP_VRSQ,			/* SGXVEC_USE0_TEST_ALUOP_F32_FRSQ */
		USEASM_OP_VLOG,			/* SGXVEC_USE0_TEST_ALUOP_F32_FLOG */
		USEASM_OP_VEXP,			/* SGXVEC_USE0_TEST_ALUOP_F32_FEXP */
		USEASM_OP_VDP,			/* SGXVEC_USE0_TEST_ALUOP_F32_VDP */
		USEASM_OP_VMIN,			/* SGXVEC_USE0_TEST_ALUOP_F32_VMIN */
		USEASM_OP_VMAX,			/* SGXVEC_USE0_TEST_ALUOP_F32_VMAX */
		USEASM_OP_VDSX,			/* SGXVEC_USE0_TEST_ALUOP_F32_VDSX */
		USEASM_OP_VDSY,			/* SGXVEC_USE0_TEST_ALUOP_F32_VDSY */
		USEASM_OP_VMUL,			/* SGXVEC_USE0_TEST_ALUOP_F32_VMUL */
		USEASM_OP_VSUB,			/* SGXVEC_USE0_TEST_ALUOP_F32_VSUB */
		USEASM_OP_UNDEF,		/* SGXVEC_USE0_TEST_ALUOP_F32_RESERVED2 */
	};
#endif /* #if defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

	static const USEASM_OPCODE aeTestOpcode[4][16] = 
	{
		{
			USEASM_OP_FADD,		/* EURASIA_USE0_TEST_ALUOP_F32_ADD */
			USEASM_OP_UNDEF,	/* EURASIA_USE0_TEST_ALUOP_F32_RESERVED0 */
			USEASM_OP_UNDEF,	/* EURASIA_USE0_TEST_ALUOP_F32_RESERVED1 */
			USEASM_OP_FSUBFLR,	/* EURASIA_USE0_TEST_ALUOP_F32_FRC */
			USEASM_OP_FRCP,		/* EURASIA_USE0_TEST_ALUOP_F32_RCP */
			USEASM_OP_FRSQ,		/* EURASIA_USE0_TEST_ALUOP_F32_RSQ */
			USEASM_OP_FLOG,		/* EURASIA_USE0_TEST_ALUOP_F32_LOG */
			USEASM_OP_FEXP,		/* EURASIA_USE0_TEST_ALUOP_F32_EXP */
			USEASM_OP_FDP,		/* EURASIA_USE0_TEST_ALUOP_F32_DP */
			USEASM_OP_FMIN,		/* EURASIA_USE0_TEST_ALUOP_F32_MIN */
			USEASM_OP_FMAX,		/* EURASIA_USE0_TEST_ALUOP_F32_MAX */
			USEASM_OP_FDSX,		/* EURASIA_USE0_TEST_ALUOP_F32_DSX */
			USEASM_OP_FDSY,		/* EURASIA_USE0_TEST_ALUOP_F32_DSY */
			USEASM_OP_FMUL,		/* EURASIA_USE0_TEST_ALUOP_F32_MUL */
			USEASM_OP_FSUB,		/* EURASIA_USE0_TEST_ALUOP_F32_SUB */
			USEASM_OP_UNDEF,	/* EURASIA_USE0_TEST_ALUOP_F32_RESERVED2 */
		},
		{
			USEASM_OP_UNDEF,	/* EURASIA_USE0_TEST_ALUOP_I16_RESERVED0 */
			USEASM_OP_UNDEF,	/* EURASIA_USE0_TEST_ALUOP_I16_RESERVED1 */
			USEASM_OP_UNDEF,	/* EURASIA_USE0_TEST_ALUOP_I16_RESERVED2 */
			USEASM_OP_UNDEF,	/* EURASIA_USE0_TEST_ALUOP_I16_RESERVED3 */
			USEASM_OP_UNDEF,	/* EURASIA_USE0_TEST_ALUOP_I16_RESERVED4 */
			USEASM_OP_UNDEF,	/* EURASIA_USE0_TEST_ALUOP_I16_RESERVED5 */
			USEASM_OP_IADD16,	/* EURASIA_USE0_TEST_ALUOP_I16_IADD */
			USEASM_OP_ISUB16,	/* EURASIA_USE0_TEST_ALUOP_I16_ISUB */
			USEASM_OP_IMUL16,	/* EURASIA_USE0_TEST_ALUOP_I16_IMUL */
			USEASM_OP_IADDU16,	/* EURASIA_USE0_TEST_ALUOP_I16_IADDU */
			USEASM_OP_ISUBU16,	/* EURASIA_USE0_TEST_ALUOP_I16_ISUBU */
			USEASM_OP_IMULU16,	/* EURASIA_USE0_TEST_ALUOP_I16_IMULU */
			USEASM_OP_IADD32,	/* EURASIA_USE0_TEST_ALUOP_I16_IADD32 */
			USEASM_OP_IADDU32,	/* EURASIA_USE0_TEST_ALUOP_I16_IADDU32 */
			USEASM_OP_UNDEF,
			USEASM_OP_UNDEF
		},
		{
			USEASM_OP_IADD8,	/* EURASIA_USE0_TEST_ALUOP_I8_ADD */
			USEASM_OP_ISUB8,	/* EURASIA_USE0_TEST_ALUOP_I8_SUB */
			USEASM_OP_IADDU8,	/* EURASIA_USE0_TEST_ALUOP_I8_ADDU */
			USEASM_OP_ISUBU8,	/* EURASIA_USE0_TEST_ALUOP_I8_SUBU */
			USEASM_OP_IMUL8,	/* EURASIA_USE0_TEST_ALUOP_I8_MUL */
			USEASM_OP_FPMUL8,	/* EURASIA_USE0_TEST_ALUOP_I8_FPMUL */
			USEASM_OP_IMULU8,	/* EURASIA_USE0_TEST_ALUOP_I8_MULU */
			USEASM_OP_FPADD8,	/* EURASIA_USE0_TEST_ALUOP_I8_FPADD */
			USEASM_OP_FPSUB8,	/* EURASIA_USE0_TEST_ALUOP_I8_FPSUB */
			USEASM_OP_UNDEF, 
			USEASM_OP_UNDEF, 
			USEASM_OP_UNDEF, 
			USEASM_OP_UNDEF, 
			USEASM_OP_UNDEF, 
			USEASM_OP_UNDEF, 
			USEASM_OP_UNDEF,
		},
		{
			USEASM_OP_AND,			/* EURASIA_USE0_TEST_ALUOP_BITWISE_AND */
			USEASM_OP_OR,			/* EURASIA_USE0_TEST_ALUOP_BITWISE_OR */
			USEASM_OP_XOR,			/* EURASIA_USE0_TEST_ALUOP_BITWISE_XOR */
			USEASM_OP_SHL,			/* EURASIA_USE0_TEST_ALUOP_BITWISE_SHL */
			USEASM_OP_SHR,			/* EURASIA_USE0_TEST_ALUOP_BITWISE_SHR */
			USEASM_OP_ROL,			/* EURASIA_USE0_TEST_ALUOP_BITWISE_ROL */
			USEASM_OP_UNDEF, 
			USEASM_OP_ASR,			/* EURASIA_USE0_TEST_ALUOP_BITWISE_ASR */
			USEASM_OP_UNDEF, 
			USEASM_OP_UNDEF, 
			USEASM_OP_UNDEF, 
			USEASM_OP_UNDEF, 
			USEASM_OP_UNDEF, 
			USEASM_OP_UNDEF, 
			USEASM_OP_UNDEF, 
			USEASM_OP_UNDEF,
		}
	};

	PVR_UNREFERENCED_PARAMETER(psTarget);

	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	if (
			SupportsVEC34(psTarget) && 
			uAluSel == EURASIA_USE0_TEST_ALUSEL_F32
		)
	{
		switch (uPrec)
		{
			case SGXVEC_USE1_TEST_PREC_ALUFLOAT_F16: return aeVecF16Opcode[uAluOp]; break;
			case SGXVEC_USE1_TEST_PREC_ALUFLOAT_F32: return aeVecF32Opcode[uAluOp]; break;
		}
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
	#if defined(SUPPORT_SGX_FEATURE_USE_TEST_SABLND)
	if (
			SupportsTESTSABLND(psTarget) &&
			uAluSel == EURASIA_USE0_TEST_ALUSEL_I8 &&
			uAluOp == SGXVEC_USE0_TEST_ALUOP_I8_SABLND
		)
	{
		return USEASM_OP_SABLND;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_TEST_SABLND) */
	#if defined(SUPPORT_SGX_FEATURE_USE_TEST_SUB32)
	if (
			SupportsTESTSUB32(psTarget) &&
			uAluSel == EURASIA_USE0_TEST_ALUSEL_I16 &&
			(
				uAluOp == SGXVEC_USE0_TEST_ALUOP_I16_SUB32 ||
				uAluOp == SGXVEC_USE0_TEST_ALUOP_I16_SUBU32
			)
		)
	{
		if (uAluOp == SGXVEC_USE0_TEST_ALUOP_I16_SUB32)
		{
			return USEASM_OP_ISUB32;
		}
		else
		{
			return USEASM_OP_ISUBU32;
		}
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_TEST_SUB32) */
	
	return aeTestOpcode[uAluSel][uAluOp];
}

static USEDISASM_ERROR DecodeTestInstruction(PCSGX_CORE_DESC	psTarget,
										     IMG_UINT32		uInst0,
										     IMG_UINT32		uInst1,
										     IMG_UINT32		uOpcode,
											 PUSE_INST		psInst)
{
	static const USEASM_TEST_SIGN aeSignTest[4] = 
	{
		USEASM_TEST_SIGN_TRUE,		/* EURASIA_USE1_TEST_STST_NONE */
		USEASM_TEST_SIGN_NEGATIVE,	/* EURASIA_USE1_TEST_STST_NEGATIVE */
		USEASM_TEST_SIGN_POSITIVE,	/* EURASIA_USE1_TEST_STST_POSITIVE */
		USEASM_TEST_SIGN_UNDEF,		/* EURASIA_USE1_TEST_STST_RESERVED */
	};
	static const USEASM_TEST_ZERO aeZeroTest[4] = 
	{
		USEASM_TEST_ZERO_TRUE,		/* EURASIA_USE1_TEST_ZTST_NONE */
		USEASM_TEST_ZERO_ZERO,		/* EURASIA_USE1_TEST_ZTST_ZERO */
		USEASM_TEST_ZERO_NONZERO,	/* EURASIA_USE1_TEST_ZTST_NOTZERO */
		USEASM_TEST_ZERO_UNDEF,		/* EURASIA_USE1_TEST_ZTST_RESERVED */
	};
	static const USEASM_TEST_CHANSEL aeChanSel[8] = 
	{
		USEASM_TEST_CHANSEL_C0,		/* EURASIA_USE1_TEST_CHANCC_SELECT0 */
		USEASM_TEST_CHANSEL_C1,		/* EURASIA_USE1_TEST_CHANCC_SELECT1 */
		USEASM_TEST_CHANSEL_C2,		/* EURASIA_USE1_TEST_CHANCC_SELECT2 */
		USEASM_TEST_CHANSEL_C3,		/* EURASIA_USE1_TEST_CHANCC_SELECT3 */
		USEASM_TEST_CHANSEL_ANDALL,	/* EURASIA_USE1_TEST_CHANCC_ANDALL */
		USEASM_TEST_CHANSEL_ORALL,	/* EURASIA_USE1_TEST_CHANCC_ORALL */
		USEASM_TEST_CHANSEL_AND02,	/* EURASIA_USE1_TEST_CHANCC_AND02 */
		USEASM_TEST_CHANSEL_OR02,	/* EURASIA_USE1_TEST_CHANCC_OR02 */
	};
	static const USEASM_TEST_MASK aeTestMask[] = 
	{
		USEASM_TEST_MASK_BYTE,	/* EURASIA_USE1_TESTMASK_TSTTYPE_4CHAN */ 
		USEASM_TEST_MASK_WORD,	/* EURASIA_USE1_TESTMASK_TSTTYPE_2CHAN */
		USEASM_TEST_MASK_DWORD,	/* EURASIA_USE1_TESTMASK_TSTTYPE_1CHAN */
		USEASM_TEST_MASK_UNDEF,
	};
#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	static const USEASM_TEST_MASK aeVecTestMask[] = 
	{
		USEASM_TEST_MASK_BYTE,	/* SGXVEC_USE1_TESTMASK_TSTTYPE_8BITMASK */
		USEASM_TEST_MASK_PREC,	/* SGXVEC_USE1_TESTMASK_TSTTYPE_PRECMASK */
		USEASM_TEST_MASK_NUM,	/* SGXVEC_USE1_TESTMASK_TSTTYPE_NUMMASK */
		USEASM_TEST_MASK_UNDEF,
	};
#endif /* #if defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

	IMG_UINT32 uAluSel = (uInst0 & ~EURASIA_USE0_TEST_ALUSEL_CLRMSK) >> EURASIA_USE0_TEST_ALUSEL_SHIFT;
	IMG_UINT32 uAluOp = (uInst0 & ~EURASIA_USE0_TEST_ALUOP_CLRMSK) >> EURASIA_USE0_TEST_ALUOP_SHIFT;
	USEDIS_FORMAT_CONTROL_STATE eFmtControl = USEDIS_FORMAT_CONTROL_STATE_OFF;
	IMG_UINT32 uFmtFlag = 0;
	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	IMG_UINT32 uPrec = (uInst1 & ~SGXVEC_USE1_TEST_PREC_CLRMSK) >> SGXVEC_USE1_TEST_PREC_SHIFT;
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
	IMG_UINT32 uEPred;
	IMG_UINT32 uSTst, uZTst;
	PUSE_REGISTER psNextArg = psInst->asArg;

	UseAsmInitInst(psInst);

	/*
		Decode predicate.
	*/
	uEPred = (uInst1 & ~EURASIA_USE1_EPRED_CLRMSK) >> EURASIA_USE1_EPRED_SHIFT;
	psInst->uFlags1 |= (g_aeDecodeEPred[uEPred] << USEASM_OPFLAGS1_PRED_SHIFT);

	/*
		Decode opcode.
	*/
	psInst->uOpcode = DecodeTestOpcode(psTarget, 
									   #if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
									   uPrec,
									   #endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
									   uAluSel, 
									   uAluOp);	
	if (psInst->uOpcode == USEASM_OP_UNDEF)
	{
		return USEDISASM_ERROR_INVALID_TEST_ALUOP;
	}

	/*
		Decode instruction modifiers.
	*/
	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	if (SupportsVEC34(psTarget))
	{
		IMG_UINT32	uRptCount = (uInst1 & ~SGXVEC_USE1_TEST_RCNT_CLRMSK) >> SGXVEC_USE1_TEST_RCNT_SHIFT;

		psInst->uFlags1 |= ((uRptCount + 1) << USEASM_OPFLAGS1_REPEAT_SHIFT);
		psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_SYNCSTART)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SYNCSTART;
	}
	if (uInst1 & EURASIA_USE1_TEST_ONCEONLY)
	{
		psInst->uFlags2 |= USEASM_OPFLAGS2_ONCEONLY;
	}
	if (uAluSel == EURASIA_USE0_TEST_ALUSEL_BITWISE)
	{
		if (uInst1 & EURASIA_USE1_TEST_PARTIAL)
		{
			psInst->uFlags1 |= USEASM_OPFLAGS1_PARTIAL;
		}
	}
	else if (uAluSel == EURASIA_USE0_TEST_ALUSEL_F32)
	{
		if (!SupportsVEC34(psTarget) && (uInst1 & EURASIA_USE1_TEST_SFASEL))
		{
			eFmtControl = USEDIS_FORMAT_CONTROL_STATE_ON;
			uFmtFlag = USEASM_ARGFLAGS_FMTF16;
		}
	}
	else if (uAluSel == EURASIA_USE0_TEST_ALUSEL_I8)
	{
		if (uAluOp == EURASIA_USE0_TEST_ALUOP_I8_FPMUL ||
			uAluOp == EURASIA_USE0_TEST_ALUOP_I8_FPADD ||
			uAluOp == EURASIA_USE0_TEST_ALUOP_I8_FPSUB)
		{
			if (uInst1 & EURASIA_USE1_TEST_SFASEL)
			{
				eFmtControl = USEDIS_FORMAT_CONTROL_STATE_ON;
				uFmtFlag = USEASM_ARGFLAGS_FMTC10;
			}
		}
		else
		{	
			if ((!IsEnhancedNoSched(psTarget) && (uInst1 & EURASIA_USE1_TEST_SAT)) ||
				(IsEnhancedNoSched(psTarget) && (uInst1 & EURASIA_USE1_TEST_NOPAIR_SAT)))
			{
				psInst->uFlags2 |= USEASM_OPFLAGS2_SAT;
			}
		}
	}
	if (IsEnhancedNoSched(psTarget) && (uInst1 & EURASIA_USE1_TEST_NOPAIR_NOSCHED))
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}

	/*
		Unpack the sign test type from the instruction.
	*/
	uSTst = (uInst1 & ~EURASIA_USE1_TEST_STST_CLRMSK) >> EURASIA_USE1_TEST_STST_SHIFT;
	if (aeSignTest[uSTst] == USEASM_TEST_SIGN_UNDEF)
	{
		return USEDISASM_ERROR_INVALID_TEST_SIGN_TEST;
	}

	/*
		Unpack the zero test type from the instruction.
	*/
	uZTst = (uInst1 & ~EURASIA_USE1_TEST_ZTST_CLRMSK) >> EURASIA_USE1_TEST_ZTST_SHIFT;
	if (aeZeroTest[uZTst] == USEASM_TEST_ZERO_UNDEF)
	{
		return USEDISASM_ERROR_INVALID_TEST_ZERO_TEST;
	}

	/*
		Convert the test specification back to the USEASM input format.
	*/
	psInst->uFlags1 |= USEASM_OPFLAGS1_TESTENABLE;
	psInst->uTest = (aeSignTest[uSTst] << USEASM_TEST_SIGN_SHIFT) |
					(aeZeroTest[uZTst] << USEASM_TEST_ZERO_SHIFT) |
					((uInst1 & EURASIA_USE1_TEST_CRCOMB_AND) ? USEASM_TEST_CRCOMB_AND : USEASM_TEST_CRCOMB_OR);

	/*
		Decode channel combine/test mask.
	*/
	if (uOpcode == EURASIA_USE1_OP_TEST)
	{
		IMG_UINT32			uChanCC = (uInst1 & ~EURASIA_USE1_TEST_CHANCC_CLRMSK) >> EURASIA_USE1_TEST_CHANCC_SHIFT;
		USEASM_TEST_CHANSEL	eChanSel;

		#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
		if (
				SupportsVEC34(psTarget) && 
				uAluSel == EURASIA_USE0_TEST_ALUSEL_F32 &&
				uChanCC == SGXVEC_USE1_TEST_CHANCC_PERCHAN
			)
		{
			eChanSel = USEASM_TEST_CHANSEL_PERCHAN;
		}
		else
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
		{
			eChanSel = aeChanSel[uChanCC];
		}
		psInst->uTest |= (eChanSel << USEASM_TEST_CHANSEL_SHIFT);
	}
	else
	{
		IMG_UINT32	uTestMaskType;

		uTestMaskType = (uInst1 & ~EURASIA_USE1_TESTMASK_TSTTYPE_CLRMSK) >> EURASIA_USE1_TESTMASK_TSTTYPE_SHIFT;

		#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
		if (SupportsVEC34(psTarget))
		{
			if (aeVecTestMask[uTestMaskType] == USEASM_TEST_MASK_UNDEF)
			{
				return USEDISASM_ERROR_INVALID_TEST_MASK_TYPE;
			}
			psInst->uTest |= (aeVecTestMask[uTestMaskType] << USEASM_TEST_MASK_SHIFT);

			if (uTestMaskType == SGXVEC_USE1_TESTMASK_TSTTYPE_NUMMASK ||
				uTestMaskType == SGXVEC_USE1_TESTMASK_TSTTYPE_PRECMASK)
			{
				if (uAluSel == EURASIA_USE0_TEST_ALUSEL_I8)
				{
					switch (uPrec)
					{
						case SGXVEC_USE1_TEST_PREC_ALUI8_I8: psInst->uFlags1 |= USEASM_OPFLAGS1_MOVC_FMTI8; break;
						case SGXVEC_USE1_TEST_PREC_ALUI8_I10: psInst->uFlags2 |= USEASM_OPFLAGS2_MOVC_FMTI10; break;
					}
				}
				else if (uAluSel == EURASIA_USE0_TEST_ALUSEL_I16)
				{
					switch (uPrec)
					{
						case SGXVEC_USE1_TEST_PREC_ALUI16_I16: psInst->uFlags2 |= USEASM_OPFLAGS2_MOVC_FMTI16; break;
						case SGXVEC_USE1_TEST_PREC_ALUI16_I32: psInst->uFlags2 |= USEASM_OPFLAGS2_MOVC_FMTI32; break;
					}
				}
			}
		}
		else
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
		{
			if (aeTestMask[uTestMaskType] == USEASM_TEST_MASK_UNDEF)
			{
				return USEDISASM_ERROR_INVALID_TEST_MASK_TYPE;
			}
			psInst->uTest |= (aeTestMask[uTestMaskType] << USEASM_TEST_MASK_SHIFT);
		}
	}

	/*
		Decode unified store destination.
	*/
	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	if (
			SupportsVEC34(psTarget) && 
			uAluSel == EURASIA_USE0_TEST_ALUSEL_F32
	   )
	{
		IMG_UINT32	uDestNum;

		uDestNum = (uInst0 & ~EURASIA_USE0_DST_CLRMSK) >> EURASIA_USE0_DST_SHIFT;
		DecodeDestWithNum(psTarget,
						  IMG_TRUE /* bDoubleRegisters */,
						  uInst1,
						  IMG_TRUE /* bAllowExtended */,
						  psNextArg,
						  IMG_FALSE /* bFmtControl */,
						  uFmtFlag,
						  SGXVEC_USE_TEST_VEC_REGISTER_FIELD_LENGTH,
						  uDestNum);
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
	{
		DecodeDest(psTarget, uInst0, uInst1, TRUE, psNextArg, eFmtControl, uFmtFlag);
	}
	if (!(uInst0 & EURASIA_USE0_TEST_WBEN))
	{
		psNextArg->uFlags |= USEASM_ARGFLAGS_DISABLEWB;
	}
	psNextArg++;

	/*
		Decode repeat mask.
	*/
	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	if (!SupportsVEC34(psTarget))
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
	{
		IMG_UINT32 uRepeatMask = (uInst1 & ~EURASIA_USE1_TEST_PREDMASK_CLRMSK) >> EURASIA_USE1_TEST_PREDMASK_SHIFT;

		psInst->uFlags1 |= (uRepeatMask << USEASM_OPFLAGS1_MASK_SHIFT);
	}
	
	/*
		Decode predicate destination.
	*/
	if (uOpcode == EURASIA_USE1_OP_TEST)
	{
		psNextArg->uType = USEASM_REGTYPE_PREDICATE;
		psNextArg->uNumber = (uInst1 & ~EURASIA_USE1_TEST_PDST_CLRMSK) >> EURASIA_USE1_TEST_PDST_SHIFT;
		psNextArg++;
	}

	/*
		Decode source arguments.
	*/
	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	if (SupportsVEC34(psTarget) && uAluSel == EURASIA_USE0_TEST_ALUSEL_F32)
	{
		IMG_UINT32	uSource1Num;
		IMG_UINT32	uSource2Num;

		uSource1Num = (uInst0 & ~EURASIA_USE0_SRC1_CLRMSK) >> EURASIA_USE0_SRC1_SHIFT;
		DecodeSrc12WithNum(psTarget,
						   IMG_TRUE /* bDoubleRegisters */,
						   uInst0,
						   uInst1,
						   1 /* uSrc */,
						   IMG_TRUE /* bAllowExtended */,
						   EURASIA_USE1_S1BEXT /* uExt */,
						   psNextArg,
						   IMG_FALSE /* bFmtControl */,
						   uFmtFlag,
						   SGXVEC_USE_TEST_VEC_REGISTER_FIELD_LENGTH,
						   uSource1Num);
		/*
			Show the F16/F32 precision select as a flag on the source
			argument for these instructions.
		*/
		if (
				uPrec == SGXVEC_USE1_TEST_PREC_ALUFLOAT_F16 &&
				(
					uAluOp == SGXVEC_USE0_TEST_ALUOP_F16_FRCP ||
					uAluOp == SGXVEC_USE0_TEST_ALUOP_F16_FRSQ ||
					uAluOp == SGXVEC_USE0_TEST_ALUOP_F16_FLOG ||
					uAluOp == SGXVEC_USE0_TEST_ALUOP_F16_FEXP
				)
			)
		{
			psNextArg->uFlags |= USEASM_ARGFLAGS_FMTF16;
		}
		if (uInst1 & SGXVEC_USE1_TEST_NEGATESRC1)
		{
			psNextArg->uFlags |= USEASM_ARGFLAGS_NEGATE;
		}
		psNextArg++;

		SetupSwizzleSource(psNextArg, USEASM_SWIZZLE(X, Y, Z, W));
		psNextArg++;

		uSource2Num = (uInst0 & ~EURASIA_USE0_SRC2_CLRMSK) >> EURASIA_USE0_SRC2_SHIFT;
		DecodeSrc12WithNum(psTarget,
						   IMG_TRUE /* bDoubleRegisters */,
						   uInst0,
						   uInst1,
						   2 /* uSrc */,
						   IMG_TRUE /* bAllowExtended */,
						   EURASIA_USE1_S2BEXT /* uExt */,
						   psNextArg,
						   IMG_FALSE /* bFmtControl */,
						   uFmtFlag,
						   SGXVEC_USE_TEST_VEC_REGISTER_FIELD_LENGTH,
						   uSource2Num);
		psNextArg++;

		if (uInst1 & SGXVEC_USE1_TEST_VSCOMP)
		{
			SetupSwizzleSource(psNextArg, USEASM_SWIZZLE(X, X, X, X));
		}
		else
		{
			SetupSwizzleSource(psNextArg, USEASM_SWIZZLE(X, Y, Z, W));
		}
		psNextArg++;
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
	{
		DecodeSrc12(psTarget, uInst0, uInst1, 1, TRUE, EURASIA_USE1_S1BEXT, psNextArg, eFmtControl, uFmtFlag);
		psNextArg++;

		DecodeSrc12(psTarget, uInst0, uInst1, 2, TRUE, EURASIA_USE1_S2BEXT, psNextArg, eFmtControl, uFmtFlag);
		psNextArg++;
	}

	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeRLPInstruction(PCSGX_CORE_DESC	psTarget,
											IMG_UINT32		uInst0,
										    IMG_UINT32		uInst1,
											PUSE_INST		psInst)
{
	IMG_UINT32 uPSel = 
			((uInst0 & ~EURASIA_USE0_BITWISE_SRC2IEXTLPSEL_CLRMSK) >> EURASIA_USE0_BITWISE_SRC2IEXTLPSEL_SHIFT);
	USEASM_PRED ePredicate;

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*	
		Set instruction opcode.
	*/
	psInst->uOpcode = USEASM_OP_RLP;

	/*
		Decode predicate.
	*/
	ePredicate = g_aeDecodeEPred[(uInst1 & ~EURASIA_USE1_EPRED_CLRMSK) >> EURASIA_USE1_EPRED_SHIFT];
	psInst->uFlags1 |= (ePredicate << USEASM_OPFLAGS1_PRED_SHIFT);

	/*
		Decode instruction flags.
	*/
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_SYNCSTART)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SYNCSTART;
	}
	if (uInst1 & EURASIA_USE1_BITWISE_PARTIAL)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_PARTIAL;
	}
	#if defined(SUPPORT_SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK)
	if (NoRepeatMaskOnBitwiseInstructions(psTarget))
	{
		IMG_UINT32	uMaskCount = (uInst1 & ~SGXVEC_USE1_BITWISE_RCOUNT_CLRMSK) >> SGXVEC_USE1_BITWISE_RCOUNT_SHIFT;

		psInst->uFlags1 |= ((uMaskCount + 1) << USEASM_OPFLAGS1_REPEAT_SHIFT);
		psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK) */
	{
		DecodeRepeatCountMask(uInst1, psInst);
	}
	if (uInst1 & EURASIA_USE1_END)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}
	if (IsEnhancedNoSched(psTarget) && (uInst1 & EURASIA_USE1_BITWISE_NOSCHED))
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}

	/*
		Decode destination register.
	*/
	DecodeDest(psTarget, uInst0, uInst1, TRUE, &psInst->asArg[0], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);

	/*
		Decode source register.
	*/
	DecodeSrc12(psTarget, uInst0, uInst1, 1, TRUE, EURASIA_USE1_S1BEXT, &psInst->asArg[1], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);

	/*
		Set up predicate source register.
	*/
	psInst->asArg[2].uType = USEASM_REGTYPE_PREDICATE;
	psInst->asArg[2].uNumber = uPSel;
	psInst->asArg[2].uIndex = USEREG_INDEX_NONE;
	psInst->asArg[2].uFlags = 0;

	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeSOP2Instruction(PCSGX_CORE_DESC			psTarget,
										     IMG_UINT32				uInst0,
										     IMG_UINT32				uInst1,
											 PCUSEDIS_RUNTIME_STATE	psRuntimeState,
										     PUSE_INST				psInst,
											 PUSE_INST				psCoInst)
{
	IMG_UINT32 uRCount = (uInst1 & ~EURASIA_USE1_INT_RCOUNT_CLRMSK) >> EURASIA_USE1_INT_RCOUNT_SHIFT;
	IMG_UINT32 uSrc1Mod = (uInst0 & ~EURASIA_USE0_SOP2_SRC1MOD_CLRMSK) >> EURASIA_USE0_SOP2_SRC1MOD_SHIFT;
	IMG_UINT32 uCSel1 = (uInst1 & ~EURASIA_USE1_SOP2_CSEL1_CLRMSK) >> EURASIA_USE1_SOP2_CSEL1_SHIFT;
	IMG_UINT32 uCSel2 = (uInst1 & ~EURASIA_USE1_SOP2_CSEL2_CLRMSK) >> EURASIA_USE1_SOP2_CSEL2_SHIFT;
	IMG_UINT32 uCMod1 = (uInst1 & ~EURASIA_USE1_SOP2_CMOD1_CLRMSK) >> EURASIA_USE1_SOP2_CMOD1_SHIFT;
	IMG_UINT32 uCMod2 = (uInst1 & ~EURASIA_USE1_SOP2_CMOD2_CLRMSK) >> EURASIA_USE1_SOP2_CMOD2_SHIFT;
	IMG_UINT32 uCOp = (uInst0 & ~EURASIA_USE0_SOP2_COP_CLRMSK) >> EURASIA_USE0_SOP2_COP_SHIFT;
	IMG_UINT32 uASrc1Mod = (uInst0 & ~EURASIA_USE0_SOP2_ASRC1MOD_CLRMSK) >> EURASIA_USE0_SOP2_ASRC1MOD_SHIFT;
	IMG_UINT32 uASel1 = (uInst1 & ~EURASIA_USE1_SOP2_ASEL1_CLRMSK) >> EURASIA_USE1_SOP2_ASEL1_SHIFT;
	IMG_UINT32 uASel2 = (uInst1 & ~EURASIA_USE1_SOP2_ASEL2_CLRMSK) >> EURASIA_USE1_SOP2_ASEL2_SHIFT;
	IMG_UINT32 uAMod1 = (uInst1 & ~EURASIA_USE1_SOP2_AMOD1_CLRMSK) >> EURASIA_USE1_SOP2_AMOD1_SHIFT;
	IMG_UINT32 uAMod2 = (uInst1 & ~EURASIA_USE1_SOP2_AMOD2_CLRMSK) >> EURASIA_USE1_SOP2_AMOD2_SHIFT;
	IMG_UINT32 uAOp = (uInst0 & ~EURASIA_USE0_SOP2_AOP_CLRMSK) >> EURASIA_USE0_SOP2_AOP_SHIFT;
	IMG_UINT32 uADstMod = (uInst0 & ~EURASIA_USE0_SOP2_ADSTMOD_CLRMSK) >> EURASIA_USE0_SOP2_ADSTMOD_SHIFT;
	static const USEASM_INTSRCSEL aeCSel1[] = 
	{
		USEASM_INTSRCSEL_ZERO,			/* EURASIA_USE1_SOP2_CSEL1_ZERO */
		USEASM_INTSRCSEL_SRC1,			/* EURASIA_USE1_SOP2_CSEL1_SRC1 */
		USEASM_INTSRCSEL_SRC2,			/* EURASIA_USE1_SOP2_CSEL1_SRC2 */
		USEASM_INTSRCSEL_SRC1ALPHA,		/* EURASIA_USE1_SOP2_CSEL1_SRC1ALPHA */
		USEASM_INTSRCSEL_SRC2ALPHA,		/* EURASIA_USE1_SOP2_CSEL1_SRC2ALPHA */
		USEASM_INTSRCSEL_SRCALPHASAT,	/* EURASIA_USE1_SOP2_CSEL1_MINSRC1A1MSRC2A */
		USEASM_INTSRCSEL_SRC2SCALE,		/* EURASIA_USE1_SOP2_CSEL1_SRC2DESTX2 */
		USEASM_INTSRCSEL_UNDEF,			/* EURASIA_USE1_SOP2_CSEL1_RESERVED */
	};
	static const USEASM_INTSRCSEL aeCSel2[] = 
	{
		USEASM_INTSRCSEL_ZERO,			/* EURASIA_USE1_SOP2_CSEL2_ZERO */
		USEASM_INTSRCSEL_SRC1,			/* EURASIA_USE1_SOP2_CSEL2_SRC1 */
		USEASM_INTSRCSEL_SRC2,			/* EURASIA_USE1_SOP2_CSEL2_SRC2 */
		USEASM_INTSRCSEL_SRC1ALPHA,		/* EURASIA_USE1_SOP2_CSEL2_SRC1ALPHA */
		USEASM_INTSRCSEL_SRC2ALPHA,		/* EURASIA_USE1_SOP2_CSEL2_SRC2ALPHA */
		USEASM_INTSRCSEL_SRCALPHASAT,	/* EURASIA_USE1_SOP2_CSEL2_MINSRC1A1MSRC2A */
		USEASM_INTSRCSEL_ZEROS,			/* EURASIA_USE1_SOP2_CSEL2_ZEROSRC2MINUSHALF */
		USEASM_INTSRCSEL_UNDEF,			/* EURASIA_USE1_SOP2_CSEL2_RESERVED */
	};
	static const USEASM_INTSRCSEL aeOp[] = 
	{
		USEASM_INTSRCSEL_ADD,			/* EURASIA_USE0_SOP2_COP_ADD / EURASIA_USE0_SOP2_AOP_ADD */
		USEASM_INTSRCSEL_SUB,			/* EURASIA_USE0_SOP2_COP_SUB / EURASIA_USE0_SOP2_AOP_SUB */
		USEASM_INTSRCSEL_MIN,			/* EURASIA_USE0_SOP2_COP_MIN / EURASIA_USE0_SOP2_AOP_MIN */
		USEASM_INTSRCSEL_MAX,			/* EURASIA_USE0_SOP2_COP_MAX / EURASIA_USE0_SOP2_AOP_MAX */
	};
	static const USEASM_INTSRCSEL aeASel1[] = 
	{
		USEASM_INTSRCSEL_ZERO,			/* EURASIA_USE1_SOP2_ASEL1_ZERO */
		USEASM_INTSRCSEL_SRC1ALPHA,		/* EURASIA_USE1_SOP2_ASEL1_SRC1ALPHA */
		USEASM_INTSRCSEL_SRC2ALPHA,		/* EURASIA_USE1_SOP2_ASEL1_SRC2ALPHA */
		USEASM_INTSRCSEL_SRC2SCALE		/* EURASIA_USE1_SOP2_ASEL1_SRC2ALPHAX2 */
	};
	static const USEASM_INTSRCSEL aeASel2[] = 
	{
		USEASM_INTSRCSEL_ZERO,			/* EURASIA_USE1_SOP2_ASEL2_ZERO */ 
		USEASM_INTSRCSEL_SRC1ALPHA,		/* EURASIA_USE1_SOP2_ASEL2_SRC1ALPHA */
		USEASM_INTSRCSEL_SRC2ALPHA,		/* EURASIA_USE1_SOP2_ASEL2_SRC2ALPHA */
		USEASM_INTSRCSEL_ZEROS,			/* EURASIA_USE1_SOP2_ASEL2_ZEROSRC2MINUSHALF */
	};
	USEASM_PRED ePredicate;

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);
	psInst->uFlags1 |= USEASM_OPFLAGS1_MAINISSUE;

	/*	
		Set instruction opcode.
	*/
	psInst->uOpcode = USEASM_OP_SOP2;

	/*
		Decode predicate.
	*/
	ePredicate = g_aeDecodeSPred[(uInst1 & ~EURASIA_USE1_SPRED_CLRMSK) >> EURASIA_USE1_SPRED_SHIFT];
	psInst->uFlags1 |= (ePredicate << USEASM_OPFLAGS1_PRED_SHIFT);

	/*
		Decode repeat count.
	*/
	psInst->uFlags1 |= ((uRCount + 1) << USEASM_OPFLAGS1_REPEAT_SHIFT);
	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

	/*
		Decode instruction flags.
	*/
	if (uInst1 & EURASIA_USE1_END)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_INT_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}

	/*
		Decode destination.
	*/
	DecodeDest(psTarget, uInst0, uInst1, TRUE, &psInst->asArg[0], psRuntimeState->eColourFormatControl, USEASM_ARGFLAGS_FMTC10);
	
	/*
		Decode source registers.
	*/
	DecodeSrc12(psTarget, uInst0, uInst1, 1, TRUE, EURASIA_USE1_S1BEXT, &psInst->asArg[1], psRuntimeState->eColourFormatControl, USEASM_ARGFLAGS_FMTC10);
	DecodeSrc12(psTarget, uInst0, uInst1, 2, TRUE, EURASIA_USE1_S2BEXT, &psInst->asArg[2], psRuntimeState->eColourFormatControl, USEASM_ARGFLAGS_FMTC10);

	/*
		Decode source 1 modifier.
	*/
	if (uSrc1Mod == EURASIA_USE0_SOP2_SRC1MOD_COMPLEMENT)
	{
		SetupIntSrcSel(&psInst->asArg[3], USEASM_INTSRCSEL_COMP);
	}
	else
	{
		SetupIntSrcSel(&psInst->asArg[3], USEASM_INTSRCSEL_NONE);
	}

	/*
		Decode colour selector 1.
	*/
	if (aeCSel1[uCSel1] == USEASM_INTSRCSEL_UNDEF)
	{
		return USEDISASM_ERROR_INVALID_SOP2_CSEL1;
	}
	SetupIntSrcSel(&psInst->asArg[4], aeCSel1[uCSel1]);
	if (uCMod1 == EURASIA_USE1_SOP2_CMOD1_COMPLEMENT)
	{
		psInst->asArg[4].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
	}

	/*
		Decode colour selector 2.
	*/
	if (aeCSel2[uCSel2] == USEASM_INTSRCSEL_UNDEF)
	{
		return USEDISASM_ERROR_INVALID_SOP2_CSEL2;
	}
	SetupIntSrcSel(&psInst->asArg[5], aeCSel2[uCSel2]);
	if (uCMod2 == EURASIA_USE1_SOP2_CMOD2_COMPLEMENT)
	{
		psInst->asArg[5].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
	}

	/*
		Decode colour operator.
	*/
	SetupIntSrcSel(&psInst->asArg[6], aeOp[uCOp]);

	/*
		Initialize the co-issued instruction.
	*/
	UseAsmInitInst(psCoInst);
	psCoInst->uOpcode = USEASM_OP_ASOP2;
	psCoInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);
	psCoInst->uFlags2 |= USEASM_OPFLAGS2_COISSUE;

	/*
		Decode alpha source 1 modifier.
	*/
	if (uASrc1Mod == EURASIA_USE0_SOP2_ASRC1MOD_COMPLEMENT)
	{
		SetupIntSrcSel(&psCoInst->asArg[0], USEASM_INTSRCSEL_COMP);
	}
	else
	{
		SetupIntSrcSel(&psCoInst->asArg[0], USEASM_INTSRCSEL_NONE);
	}

	/*
		Decode alpha selector 1.
	*/
	SetupIntSrcSel(&psCoInst->asArg[1], aeASel1[uASel1]);
	if (uAMod1 == EURASIA_USE1_SOP2_AMOD1_COMPLEMENT)
	{
		psCoInst->asArg[1].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
	}

	/*
		Decode alpha selector 2.
	*/
	SetupIntSrcSel(&psCoInst->asArg[2], aeASel2[uASel2]);
	if (uAMod2 == EURASIA_USE1_SOP2_AMOD2_COMPLEMENT)
	{
		psCoInst->asArg[2].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
	}

	/*
		Decode alpha operator.
	*/
	SetupIntSrcSel(&psCoInst->asArg[3], aeOp[uAOp]);

	/*
		Decode alpha destination modifier.
	*/
	if (uADstMod == EURASIA_USE0_SOP2_ADSTMOD_NEGATE)
	{
		SetupIntSrcSel(&psCoInst->asArg[4], USEASM_INTSRCSEL_NEG);
	}
	else
	{
		SetupIntSrcSel(&psCoInst->asArg[4], USEASM_INTSRCSEL_NONE);
	}

	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeSOPWMInstruction(PCSGX_CORE_DESC			psTarget,
											  IMG_UINT32				uInst0,
											  IMG_UINT32				uInst1,
											  PCUSEDIS_RUNTIME_STATE	psRuntimeState,
											  PUSE_INST					psInst)
{
	IMG_UINT32 uSel1 = (uInst1 & ~EURASIA_USE1_SOP2WM_SEL1_CLRMSK) >> EURASIA_USE1_SOP2WM_SEL1_SHIFT;
	IMG_UINT32 uSel2 = (uInst1 & ~EURASIA_USE1_SOP2WM_SEL2_CLRMSK) >> EURASIA_USE1_SOP2WM_SEL2_SHIFT;
	IMG_UINT32 uMod1 = (uInst1 & ~EURASIA_USE1_SOP2WM_MOD1_CLRMSK) >> EURASIA_USE1_SOP2WM_MOD1_SHIFT;
	IMG_UINT32 uMod2 = (uInst1 & ~EURASIA_USE1_SOP2WM_MOD2_CLRMSK) >> EURASIA_USE1_SOP2WM_MOD2_SHIFT;
	IMG_UINT32 uCOp = (uInst1 & ~EURASIA_USE1_SOP2WM_COP_CLRMSK) >> EURASIA_USE1_SOP2WM_COP_SHIFT;
	IMG_UINT32 uAOp = (uInst1 & ~EURASIA_USE1_SOP2WM_AOP_CLRMSK) >> EURASIA_USE1_SOP2WM_AOP_SHIFT;
	IMG_UINT32 uWriteMask = (uInst1 & ~EURASIA_USE1_SOP2WM_WRITEMASK_CLRMSK) >> EURASIA_USE1_SOP2WM_WRITEMASK_SHIFT;
	static const USEASM_INTSRCSEL aeSel[] =
	{
		USEASM_INTSRCSEL_ZERO,			/* EURASIA_USE1_SOP2WM_SEL1_ZERO */
		USEASM_INTSRCSEL_SRCALPHASAT,	/* EURASIA_USE1_SOP2WM_SEL1_MINSRC1A1MSRC2A */
		USEASM_INTSRCSEL_SRC1,			/* EURASIA_USE1_SOP2WM_SEL1_SRC1 */
		USEASM_INTSRCSEL_SRC1ALPHA,		/* EURASIA_USE1_SOP2WM_SEL1_SRC1ALPHA */
		USEASM_INTSRCSEL_UNDEF,			/* EURASIA_USE1_SOP2WM_SEL1_RESERVED0 */
		USEASM_INTSRCSEL_UNDEF,			/* EURASIA_USE1_SOP2WM_SEL1_RESERVED1 */
		USEASM_INTSRCSEL_SRC2,			/* EURASIA_USE1_SOP2WM_SEL1_SRC2 */
		USEASM_INTSRCSEL_SRC2ALPHA,		/* EURASIA_USE1_SOP2WM_SEL1_SRC12ALPHA */
	};
	static const USEASM_INTSRCSEL aeOp[] =
	{
		USEASM_INTSRCSEL_ADD,			/* EURASIA_USE1_SOP2WM_COP_ADD/EURASIA_USE1_SOP2WM_AOP_ADD */
		USEASM_INTSRCSEL_SUB,			/* EURASIA_USE1_SOP2WM_COP_SUB/EURASIA_USE1_SOP2WM_AOP_SUB */
		USEASM_INTSRCSEL_MIN,			/* EURASIA_USE1_SOP2WM_COP_MIN/EURASIA_USE1_SOP2WM_AOP_MIN */
		USEASM_INTSRCSEL_MAX,			/* EURASIA_USE1_SOP2WM_COP_MAX/EURASIA_USE1_SOP2WM_AOP_MAX */
	};

	USEASM_PRED ePredicate;
	IMG_UINT32 uByteMask;

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*	
		Set instruction opcode.
	*/
	psInst->uOpcode = USEASM_OP_SOP2WM;

	/*
		Decode predicate.
	*/
	ePredicate = g_aeDecodeSPred[(uInst1 & ~EURASIA_USE1_SPRED_CLRMSK) >> EURASIA_USE1_SPRED_SHIFT];
	psInst->uFlags1 |= (ePredicate << USEASM_OPFLAGS1_PRED_SHIFT);

	/*
		No repeat on SOPWM.
	*/
	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

	/*
		Decode instruction modifiers.
	*/
	if (uInst1 & EURASIA_USE1_END)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_INT_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	
	/*
		Decode destination register.
	*/
	DecodeDest(psTarget, 
			   uInst0, 
			   uInst1, 
			   IMG_TRUE /* bAllowExtended */, 
			   &psInst->asArg[0], 
			   psRuntimeState->eColourFormatControl, 
			   USEASM_ARGFLAGS_FMTC10);

	/*
		Decode write mask.
	*/
	uByteMask = ((uWriteMask & EURASIA_USE1_SOP2WM_WRITEMASK_W) ? 8 : 0) |
				((uWriteMask & EURASIA_USE1_SOP2WM_WRITEMASK_Z) ? 4 : 0) |
				((uWriteMask & EURASIA_USE1_SOP2WM_WRITEMASK_Y) ? 2 : 0) |
				((uWriteMask & EURASIA_USE1_SOP2WM_WRITEMASK_X) ? 1 : 0);
	psInst->asArg[0].uFlags |= USEASM_ARGFLAGS_BYTEMSK_PRESENT | (uByteMask << USEASM_ARGFLAGS_BYTEMSK_SHIFT);

	/*
		Decode source 1.
	*/
	DecodeSrc12(psTarget, 
				uInst0, 
				uInst1, 
				1 /* uSrc */, 
				IMG_TRUE /* bAllowExtended */, 
				EURASIA_USE1_S1BEXT, 
				&psInst->asArg[1], 
				psRuntimeState->eColourFormatControl, 
				USEASM_ARGFLAGS_FMTC10);
	
	/*
		Decode source 2.
	*/
	DecodeSrc12(psTarget, 
				uInst0, 
				uInst1, 
				2 /* uSrc */, 
				IMG_TRUE /* bAllowExtended */, 
				EURASIA_USE1_S2BEXT, 
				&psInst->asArg[2], 
				psRuntimeState->eColourFormatControl, 
				USEASM_ARGFLAGS_FMTC10);

	/*
		Decode SEL1.
	*/
	if (aeSel[uSel1] == USEASM_INTSRCSEL_UNDEF)
	{
		return USEDISASM_ERROR_INVALID_SOPWM_SEL1;
	}
	SetupIntSrcSel(&psInst->asArg[3], aeSel[uSel1]);
	if (uMod1 == EURASIA_USE1_SOP2WM_MOD1_COMPLEMENT)
	{
		psInst->asArg[3].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
	}

	/*
		Decode SEL2.
	*/
	if (aeSel[uSel2] == USEASM_INTSRCSEL_UNDEF)
	{
		return USEDISASM_ERROR_INVALID_SOPWM_SEL2;
	}
	SetupIntSrcSel(&psInst->asArg[4], aeSel[uSel2]);
	if (uMod2 == EURASIA_USE1_SOP2WM_MOD2_COMPLEMENT)
	{
		psInst->asArg[4].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
	}

	/*
		Decode colour operation.
	*/
	SetupIntSrcSel(&psInst->asArg[5], aeOp[uCOp]);

	/*
		Decode alpha operation.
	*/
	SetupIntSrcSel(&psInst->asArg[6], aeOp[uAOp]);

	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeSOP3Instruction(PCSGX_CORE_DESC			psTarget,
											 IMG_UINT32				uInst0,
											 IMG_UINT32				uInst1,
											 PCUSEDIS_RUNTIME_STATE	psRuntimeState,
										     PUSE_INST				psInst,
											 PUSE_INST				psCoInst)
{
	IMG_UINT32 uCOp = (uInst1 & ~EURASIA_USE1_SOP3_COP_CLRMSK) >> EURASIA_USE1_SOP3_COP_SHIFT;
	IMG_UINT32 uAOp = (uInst1 & ~EURASIA_USE1_SOP3_AOP_CLRMSK) >> EURASIA_USE1_SOP3_AOP_SHIFT;
	IMG_UINT32 uCSel1 = (uInst1 & ~EURASIA_USE1_SOP3_CSEL1_CLRMSK) >> EURASIA_USE1_SOP3_CSEL1_SHIFT;
	IMG_UINT32 uCSel2 = (uInst1 & ~EURASIA_USE1_SOP3_CSEL2_CLRMSK) >> EURASIA_USE1_SOP3_CSEL2_SHIFT;
	IMG_UINT32 uDestMod = (uInst1 & ~EURASIA_USE1_SOP3_DESTMOD_CLRMSK) >>  EURASIA_USE1_SOP3_DESTMOD_SHIFT;
	IMG_UINT32 uASel1 = (uInst1 & ~EURASIA_USE1_SOP3_ASEL1_CLRMSK) >> EURASIA_USE1_SOP3_ASEL1_SHIFT;
	IMG_UINT32 uCMod1 = (uInst1 & ~EURASIA_USE1_SOP3_CMOD1_CLRMSK) >> EURASIA_USE1_SOP3_CMOD1_SHIFT;
	IMG_UINT32 uCMod2 = (uInst1 & ~EURASIA_USE1_SOP3_CMOD2_CLRMSK) >> EURASIA_USE1_SOP3_CMOD2_SHIFT;
	IMG_UINT32 uAMod1 = (uInst1 & ~EURASIA_USE1_SOP3_AMOD1_CLRMSK) >> EURASIA_USE1_SOP3_AMOD1_SHIFT;
	static const USEASM_INTSRCSEL aeASel1[4] = 
	{
		USEASM_INTSRCSEL_ZERO,			/* EURASIA_USE1_SOP3_ASEL1_ZERO */
		USEASM_INTSRCSEL_SRC0ALPHA,		/* EURASIA_USE1_SOP3_ASEL1_SRC0ALPHA */
		USEASM_INTSRCSEL_SRC1ALPHA,		/* EURASIA_USE1_SOP3_ASEL1_SRC1ALPHA */
		USEASM_INTSRCSEL_SRC2ALPHA,		/* EURASIA_USE1_SOP3_ASEL1_SRC2ALPHA */
	};
	static const USEASM_INTSRCSEL aeCSel1[8] = 
	{
		USEASM_INTSRCSEL_ZERO,			/* EURASIA_USE1_SOP3_CSEL1_ZERO */
		USEASM_INTSRCSEL_SRCALPHASAT,	/* EURASIA_USE1_SOP3_CSEL1_MINSRC1A1MSRC2A */
		USEASM_INTSRCSEL_SRC1,			/* EURASIA_USE1_SOP3_CSEL1_SRC1 */
		USEASM_INTSRCSEL_SRC1ALPHA,		/* EURASIA_USE1_SOP3_CSEL1_SRC1ALPHA */
		USEASM_INTSRCSEL_SRC0,			/* EURASIA_USE1_SOP3_CSEL1_SRC0 */
		USEASM_INTSRCSEL_SRC0ALPHA,		/* EURASIA_USE1_SOP3_CSEL1_SRC0ALPHA */
		USEASM_INTSRCSEL_SRC2,			/* EURASIA_USE1_SOP3_CSEL1_SRC2 */
		USEASM_INTSRCSEL_SRC2ALPHA,		/* EURASIA_USE1_SOP3_CSEL1_SRC2ALPHA */
	};
	static const USEASM_INTSRCSEL aeCSel2[8] = 
	{
		USEASM_INTSRCSEL_ZERO,			/* EURASIA_USE1_SOP3_CSEL2_ZERO */
		USEASM_INTSRCSEL_SRCALPHASAT,	/* EURASIA_USE1_SOP3_CSEL2_MINSRC1A1MSRC2A */
		USEASM_INTSRCSEL_SRC1,			/* EURASIA_USE1_SOP3_CSEL2_SRC1 */
		USEASM_INTSRCSEL_SRC1ALPHA,		/* EURASIA_USE1_SOP3_CSEL2_SRC1ALPHA */
		USEASM_INTSRCSEL_SRC0,			/* EURASIA_USE1_SOP3_CSEL2_SRC0 */
		USEASM_INTSRCSEL_SRC0ALPHA,		/* EURASIA_USE1_SOP3_CSEL2_SRC0ALPHA */
		USEASM_INTSRCSEL_SRC2,			/* EURASIA_USE1_SOP3_CSEL2_SRC2 */
		USEASM_INTSRCSEL_SRC2ALPHA		/* EURASIA_USE1_SOP3_CSEL2_SRC2ALPHA */
	};
	static const USEASM_INTSRCSEL aeCSel10[2] = 
	{
		USEASM_INTSRCSEL_ZERO,			/* EURASIA_USE1_SOP3_ASEL1_10ZERO */
		USEASM_INTSRCSEL_SRC1,			/* EURASIA_USE1_SOP3_ASEL1_10SRC1 */
	};
	static const USEASM_INTSRCSEL aeCSel11[2] = 
	{
		USEASM_INTSRCSEL_ZERO,			/* EURASIA_USE1_SOP3_ASEL1_11ZERO */
		USEASM_INTSRCSEL_SRC2,			/* EURASIA_USE1_SOP3_ASEL1_11SRC2 */
	};
	static const USEASM_INTSRCSEL aeASel10[2] = 
	{
		USEASM_INTSRCSEL_ZERO,			/* EURASIA_USE1_SOP3_ASEL1_10ZERO */
		USEASM_INTSRCSEL_SRC1ALPHA,		/* EURASIA_USE1_SOP3_ASEL1_10SRC1 */
	};
	static const USEASM_INTSRCSEL aeASel11[2] = 
	{
		USEASM_INTSRCSEL_ZERO,			/* EURASIA_USE1_SOP3_ASEL1_11ZERO */
		USEASM_INTSRCSEL_SRC2ALPHA,		/* EURASIA_USE1_SOP3_ASEL1_11SRC2 */
	};
	IMG_UINT32 uSPred;

	UseAsmInitInst(psInst);
	UseAsmInitInst(psCoInst);

	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);
	psInst->uFlags1 |= USEASM_OPFLAGS1_MAINISSUE;
	psCoInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);
	psCoInst->uFlags2 |= USEASM_OPFLAGS2_COISSUE;

	uSPred = (uInst1 & ~EURASIA_USE1_SPRED_CLRMSK) >> EURASIA_USE1_SPRED_SHIFT;
	psInst->uFlags1 |= (g_aeDecodeSPred[uSPred] << USEASM_OPFLAGS1_PRED_SHIFT);

	if (uCOp == EURASIA_USE1_SOP3_COP_ADD || uCOp == EURASIA_USE1_SOP3_COP_SUB)
	{
		psInst->uOpcode = USEASM_OP_SOP3;
	}
	else
	{
		if (uAOp == EURASIA_USE1_SOP3_AOP_ADD || uAOp == EURASIA_USE1_SOP3_AOP_SUB)
		{
			psInst->uOpcode = USEASM_OP_LRP1;
		}
		else
		{
			psInst->uOpcode = USEASM_OP_LRP2;
		}
	}
	if (uInst1 & EURASIA_USE1_END)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_INT_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	
	/*
		Decode destination argument.
	*/
	DecodeDest(psTarget, 
			   uInst0, 
			   uInst1, 
			   TRUE, 
			   &psInst->asArg[0], 
			   psRuntimeState->eColourFormatControl, 
			   USEASM_ARGFLAGS_FMTC10);

	/*
		Decode source 0 argument.
	*/
	DecodeSrc0(psTarget, 
			   uInst0, 
			   uInst1, 
			   FALSE, 
			   0, 
			   &psInst->asArg[1], 
			   psRuntimeState->eColourFormatControl, 
			   USEASM_ARGFLAGS_FMTC10);

	/*
		Decode source 1 argument.
	*/
	DecodeSrc12(psTarget, 
				uInst0, 
				uInst1, 
				1, 
				TRUE, 
				EURASIA_USE1_S1BEXT, 
				&psInst->asArg[2], 
				psRuntimeState->eColourFormatControl, 
				USEASM_ARGFLAGS_FMTC10);

	/*
		Decode source 2 argument.
	*/
	DecodeSrc12(psTarget, 
				uInst0, 
				uInst1, 
				2, 
				TRUE, 
				EURASIA_USE1_S2BEXT, 
				&psInst->asArg[3], 
				psRuntimeState->eColourFormatControl, 
				USEASM_ARGFLAGS_FMTC10);

	if (uCOp == EURASIA_USE1_SOP3_COP_ADD || uCOp == EURASIA_USE1_SOP3_COP_SUB)
	{
		SetupIntSrcSel(&psInst->asArg[4], aeCSel1[uCSel1]);
		if (uCMod1 == EURASIA_USE1_SOP3_CMOD1_COMPLEMENT)
		{
			psInst->asArg[4].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
		}

		SetupIntSrcSel(&psInst->asArg[5], aeCSel2[uCSel2]);
		if (uCMod2 == EURASIA_USE1_SOP3_CMOD2_COMPLEMENT)
		{
			psInst->asArg[5].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
		}

		if (uCOp == EURASIA_USE1_SOP3_COP_ADD)
		{
			SetupIntSrcSel(&psInst->asArg[6], USEASM_INTSRCSEL_ADD);
		}
		else
		{
			SetupIntSrcSel(&psInst->asArg[6], USEASM_INTSRCSEL_SUB);
		}

		if ((uAOp == EURASIA_USE1_SOP3_AOP_INTERPOLATE1 || uAOp == EURASIA_USE1_SOP3_AOP_INTERPOLATE2) &&
			(uDestMod == EURASIA_USE1_SOP3_DESTMOD_NEGATE))
		{
			SetupIntSrcSel(&psInst->asArg[7], USEASM_INTSRCSEL_NEG);
		}
		else
		{
			SetupIntSrcSel(&psInst->asArg[7], USEASM_INTSRCSEL_NONE);
		}

		if (uAOp == EURASIA_USE1_SOP3_AOP_INTERPOLATE1 || uAOp == EURASIA_USE1_SOP3_AOP_INTERPOLATE2)
		{
			psCoInst->uOpcode = USEASM_OP_ALRP;
			SetupIntSrcSel(&psCoInst->asArg[0], aeASel10[(uASel1 >> 1) & 0x1]);
			if (uAOp == EURASIA_USE1_SOP3_AOP_INTERPOLATE2)
			{
				psCoInst->asArg[0].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
			}
			SetupIntSrcSel(&psCoInst->asArg[1], aeASel11[(uASel1 >> 0) & 0x1]);
			if (uAMod1 == EURASIA_USE1_SOP3_AMOD1_COMPLEMENT)
			{
				psCoInst->asArg[1].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
			}
		}
		else
		{
			if (uDestMod == EURASIA_USE1_SOP3_DESTMOD_NEGATE)
			{
				psCoInst->uOpcode = USEASM_OP_ARSOP;
				SetupIntSrcSel(&psCoInst->asArg[0], aeASel1[uASel1]);
				if (uAMod1 == EURASIA_USE1_SOP3_AMOD1_COMPLEMENT)
				{
					psCoInst->asArg[0].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
				}

				if (uAOp == EURASIA_USE1_SOP3_AOP_ADD)
				{
					SetupIntSrcSel(&psCoInst->asArg[1], USEASM_INTSRCSEL_ADD);
				}
				else
				{
					SetupIntSrcSel(&psCoInst->asArg[1], USEASM_INTSRCSEL_SUB);
				}
			}
			else
			{
				psCoInst->uOpcode = USEASM_OP_ASOP;

				SetupIntSrcSel(&psCoInst->asArg[0], aeASel1[uASel1]);
				if (uAMod1 == EURASIA_USE1_SOP3_AMOD1_COMPLEMENT)
				{
					psCoInst->asArg[0].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
				}

				SetupIntSrcSel(&psCoInst->asArg[1], aeCSel2[uCSel2]);
				if (uCMod2 == EURASIA_USE1_SOP3_CMOD2_COMPLEMENT)
				{
					psCoInst->asArg[1].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
				}

				if (uAOp == EURASIA_USE1_SOP3_AOP_ADD)
				{
					SetupIntSrcSel(&psCoInst->asArg[2], USEASM_INTSRCSEL_ADD);
				}
				else
				{
					SetupIntSrcSel(&psCoInst->asArg[2], USEASM_INTSRCSEL_SUB);
				}
			}
		}
	}
	else
	{
		if (uAOp != EURASIA_USE1_SOP3_AOP_INTERPOLATE1 && uAOp != EURASIA_USE1_SOP3_AOP_INTERPOLATE2)
		{
			SetupIntSrcSel(&psInst->asArg[4], aeCSel10[uASel1 >> 1]);
			if (uCOp == EURASIA_USE1_SOP3_COP_INTERPOLATE2)
			{
				psInst->asArg[4].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
			}
			SetupIntSrcSel(&psInst->asArg[5], aeCSel11[uASel1 & 1]);
			if (uAMod1 == EURASIA_USE1_SOP3_AMOD1_COMPLEMENT)
			{
				psInst->asArg[5].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
			}
			if (uDestMod == EURASIA_USE1_SOP3_DESTMOD_CS0SRC0ALPHA)
			{
				SetupIntSrcSel(&psInst->asArg[6], USEASM_INTSRCSEL_SRC0ALPHA);
			}
			else
			{
				SetupIntSrcSel(&psInst->asArg[6], USEASM_INTSRCSEL_SRC0);
			}

			psCoInst->uOpcode = USEASM_OP_ASOP;
			SetupIntSrcSel(&psCoInst->asArg[0], aeCSel1[uCSel1]);
			if (uCMod1 == EURASIA_USE1_SOP3_CMOD1_COMPLEMENT)
			{
				psCoInst->asArg[0].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
			}
			SetupIntSrcSel(&psCoInst->asArg[1], aeCSel2[uCSel2]);
			if (uCMod2 == EURASIA_USE1_SOP3_CMOD2_COMPLEMENT)
			{
				psCoInst->asArg[1].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
			}
			if (uAOp == EURASIA_USE1_SOP3_AOP_ADD)
			{
				SetupIntSrcSel(&psCoInst->asArg[2], USEASM_INTSRCSEL_ADD);
			}
			else
			{
				SetupIntSrcSel(&psCoInst->asArg[2], USEASM_INTSRCSEL_SUB);
			}
		}
		else
		{
			SetupIntSrcSel(&psInst->asArg[4], aeCSel1[uCSel1]);
			if (uCMod1 == EURASIA_USE1_SOP3_CMOD1_COMPLEMENT)
			{
				psInst->asArg[4].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
			}

			SetupIntSrcSel(&psInst->asArg[5], aeCSel2[uCSel2]);
			if (uCMod2 == EURASIA_USE1_SOP3_CMOD2_COMPLEMENT)
			{
				psInst->asArg[5].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
			}

			if (uCOp == EURASIA_USE1_SOP3_COP_INTERPOLATE1)
			{
				SetupIntSrcSel(&psInst->asArg[6], USEASM_INTSRCSEL_SRC1);

				SetupIntSrcSel(&psInst->asArg[7], USEASM_INTSRCSEL_SRC2);
				if (uDestMod == EURASIA_USE1_SOP3_DESTMOD_COMPLEMENTSRC2)
				{
					psInst->asArg[7].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
				}
			}
			else
			{
				if (uDestMod == EURASIA_USE1_SOP3_DESTMOD_COMPLEMENTSRC2)
				{
					SetupIntSrcSel(&psInst->asArg[6], USEASM_INTSRCSEL_SRC1);
					psInst->asArg[6].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;

					SetupIntSrcSel(&psInst->asArg[7], USEASM_INTSRCSEL_SRC2);
				}
				else
				{
					SetupIntSrcSel(&psInst->asArg[6], USEASM_INTSRCSEL_ONE);

					SetupIntSrcSel(&psInst->asArg[7], USEASM_INTSRCSEL_SRC2);
				}
			}
			
			psCoInst->uOpcode = USEASM_OP_ALRP;
			SetupIntSrcSel(&psCoInst->asArg[0], aeASel10[uASel1 >> 1]);
			if (uAOp == EURASIA_USE1_SOP3_AOP_INTERPOLATE2)
			{
				psCoInst->asArg[0].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
			}
			SetupIntSrcSel(&psCoInst->asArg[1], aeASel11[(uASel1 >> 0) & 1]);
			if (uAMod1 == EURASIA_USE1_SOP3_AMOD1_COMPLEMENT)
			{
				psCoInst->asArg[1].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
			}
		}
	}

	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeIMA8Instruction(PCSGX_CORE_DESC			psTarget,
										     IMG_UINT32				uInst0,
										     IMG_UINT32				uInst1,
											 PCUSEDIS_RUNTIME_STATE	psRuntimeState,
										     IMG_UINT32				uOpcode,
										     PUSE_INST				psInst)
{
	static const USEASM_INTSRCSEL aeCSel0[] = 
	{
		USEASM_INTSRCSEL_SRC0,		/* EURASIA_USE1_IMA8_CSEL0_SRC0 */
		USEASM_INTSRCSEL_SRC1,		/* EURASIA_USE1_IMA8_CSEL0_SRC1 */ 
		USEASM_INTSRCSEL_SRC0ALPHA,	/* EURASIA_USE1_IMA8_CSEL0_SRC0ALPHA */ 
		USEASM_INTSRCSEL_SRC1ALPHA,	/* EURASIA_USE1_IMA8_CSEL0_SRC1ALPHA */
	};
	static const USEASM_INTSRCSEL aeCSel1[] = 
	{
		USEASM_INTSRCSEL_SRC1,		/* EURASIA_USE1_IMA8_CSEL1_SRC1 */
		USEASM_INTSRCSEL_SRC1ALPHA,	/* EURASIA_USE1_IMA8_CSEL1_SRC1ALPHA */
	};
	static const USEASM_INTSRCSEL aeCSel2[] = 
	{
		USEASM_INTSRCSEL_SRC2,		/* EURASIA_USE1_IMA8_CSEL2_SRC2 */
		USEASM_INTSRCSEL_SRC2ALPHA,	/* EURASIA_USE1_IMA8_CSEL2_SRC2 */
	};
	static const USEASM_INTSRCSEL aeASel0[] = 
	{
		USEASM_INTSRCSEL_SRC0ALPHA,	/* EURASIA_USE1_IMA8_ASEL0_SRC0ALPHA */ 
		USEASM_INTSRCSEL_SRC1ALPHA,	/* EURASIA_USE1_IMA8_ASEL0_SRC1ALPHA */
	};
	IMG_UINT32 uRCount = (uInst1 & ~EURASIA_USE1_INT_RCOUNT_CLRMSK) >> EURASIA_USE1_INT_RCOUNT_SHIFT;
	IMG_UINT32 uSPred;
	IMG_UINT32 uCSel0, uCSel1, uCSel2, uCMod0, uCMod1, uCMod2;
	IMG_UINT32 uASel0, uAMod0, uAMod1, uAMod2;
	
	UseAsmInitInst(psInst);

	/*
		Decode predicate.
	*/
	uSPred = (uInst1 & ~EURASIA_USE1_SPRED_CLRMSK) >> EURASIA_USE1_SPRED_SHIFT;
	psInst->uFlags1 |= (g_aeDecodeSPred[uSPred] << USEASM_OPFLAGS1_PRED_SHIFT);

	/*
		Decode opcode.
	*/
	if (uOpcode == EURASIA_USE1_OP_FPMA)
	{
		psInst->uOpcode = USEASM_OP_FPMA;
	}
	else
	{
		psInst->uOpcode = USEASM_OP_IMA8;
	}

	/*
		Decode instruction modifiers.
	*/
	psInst->uFlags1 |= ((uRCount + 1) << USEASM_OPFLAGS1_REPEAT_SHIFT);
	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);
	if (uInst1 & EURASIA_USE1_END)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_INT_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	if (uInst1 & EURASIA_USE1_IMA8_SAT)
	{
		psInst->uFlags2 |= USEASM_OPFLAGS2_SAT;
	}

	/*
		Decode destination.
	*/
	DecodeDest(psTarget, uInst0, uInst1, TRUE, &psInst->asArg[0], psRuntimeState->eColourFormatControl, USEASM_ARGFLAGS_FMTC10);

	/*
		Decode source arguments.
	*/
	DecodeSrc0(psTarget, uInst0, uInst1, FALSE, 0, &psInst->asArg[1], psRuntimeState->eColourFormatControl, USEASM_ARGFLAGS_FMTC10);
	if (uInst1 & EURASIA_USE1_IMA8_NEGS0)
	{
		psInst->asArg[1].uFlags |= USEASM_ARGFLAGS_NEGATE;
	}
	DecodeSrc12(psTarget, uInst0, uInst1, 1, TRUE, EURASIA_USE1_S1BEXT, &psInst->asArg[2], psRuntimeState->eColourFormatControl, USEASM_ARGFLAGS_FMTC10);
	DecodeSrc12(psTarget, uInst0, uInst1, 2, TRUE, EURASIA_USE1_S2BEXT, &psInst->asArg[3], psRuntimeState->eColourFormatControl, USEASM_ARGFLAGS_FMTC10);
	
	uCSel0 = (uInst1 & ~EURASIA_USE1_IMA8_CSEL0_CLRMSK) >> EURASIA_USE1_IMA8_CSEL0_SHIFT;
	uCMod0 = (uInst1 & ~EURASIA_USE1_IMA8_CMOD0_CLRMSK) >> EURASIA_USE1_IMA8_CMOD0_SHIFT;
	SetupIntSrcSel(&psInst->asArg[4], aeCSel0[uCSel0]);
	if (uCMod0 == EURASIA_USE1_IMA8_CMOD0_COMPLEMENT)
	{
		psInst->asArg[4].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
	}

	uCSel1 = (uInst1 & ~EURASIA_USE1_IMA8_CSEL1_CLRMSK) >> EURASIA_USE1_IMA8_CSEL1_SHIFT;
	uCMod1 = (uInst1 & ~EURASIA_USE1_IMA8_CMOD1_CLRMSK) >> EURASIA_USE1_IMA8_CMOD1_SHIFT;
	SetupIntSrcSel(&psInst->asArg[5], aeCSel1[uCSel1]);
	if (uCMod1 == EURASIA_USE1_IMA8_CMOD1_COMPLEMENT)
	{
		psInst->asArg[5].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
	}

	uCSel2 = (uInst1 & ~EURASIA_USE1_IMA8_CSEL2_CLRMSK) >> EURASIA_USE1_IMA8_CSEL2_SHIFT;
	uCMod2 = (uInst1 & ~EURASIA_USE1_IMA8_CMOD2_CLRMSK) >> EURASIA_USE1_IMA8_CMOD2_SHIFT;
	SetupIntSrcSel(&psInst->asArg[6], aeCSel2[uCSel2]);
	if (uCMod2 == EURASIA_USE1_IMA8_CMOD2_COMPLEMENT)
	{
		psInst->asArg[6].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
	}

	uASel0 = (uInst1 & ~EURASIA_USE1_IMA8_ASEL0_CLRMSK) >> EURASIA_USE1_IMA8_ASEL0_SHIFT;
	uAMod0 = (uInst1 & ~EURASIA_USE1_IMA8_AMOD0_CLRMSK) >> EURASIA_USE1_IMA8_AMOD0_SHIFT;
	SetupIntSrcSel(&psInst->asArg[7], aeASel0[uASel0]);
	if (uAMod0 == EURASIA_USE1_IMA8_AMOD0_COMPLEMENT)
	{
		psInst->asArg[7].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
	}

	uAMod1 = (uInst1 & ~EURASIA_USE1_IMA8_AMOD1_CLRMSK) >> EURASIA_USE1_IMA8_AMOD1_SHIFT;
	SetupIntSrcSel(&psInst->asArg[8], USEASM_INTSRCSEL_SRC1ALPHA);
	if (uAMod1 == EURASIA_USE1_IMA8_AMOD1_COMPLEMENT)
	{
		psInst->asArg[8].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
	}

	uAMod2 = (uInst1 & ~EURASIA_USE1_IMA8_AMOD2_CLRMSK) >> EURASIA_USE1_IMA8_AMOD2_SHIFT;
	SetupIntSrcSel(&psInst->asArg[9], USEASM_INTSRCSEL_SRC2ALPHA);
	if (uAMod2 == EURASIA_USE1_IMA8_AMOD2_COMPLEMENT)
	{
		psInst->asArg[9].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
	}

	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeDOT34Instruction(PCSGX_CORE_DESC			psTarget,
											  IMG_UINT32				uInst0,
											  IMG_UINT32				uInst1,
											  PCUSEDIS_RUNTIME_STATE	psRuntimeState,
											  PUSE_INST					psInst,
											  PUSE_INST					psCoInst)
{
	IMG_UINT32 uRCount = (uInst1 & ~EURASIA_USE1_INT_RCOUNT_CLRMSK) >> EURASIA_USE1_INT_RCOUNT_SHIFT;
	IMG_UINT32 uASel1 = (uInst1 & ~EURASIA_USE1_DOT34_ASEL1_CLRMSK) >> EURASIA_USE1_DOT34_ASEL1_SHIFT;
	IMG_UINT32 uASel2 = (uInst1 & ~EURASIA_USE1_DOT34_ASEL2_CLRMSK) >> EURASIA_USE1_DOT34_ASEL2_SHIFT;
	IMG_UINT32 uOpSel = (uInst1 & ~EURASIA_USE1_DOT34_OPSEL_CLRMSK) >> EURASIA_USE1_DOT34_OPSEL_SHIFT;
	IMG_UINT32 uCScale = (uInst1 & ~EURASIA_USE1_DOT34_CSCALE_CLRMSK) >> EURASIA_USE1_DOT34_CSCALE_SHIFT;
	IMG_UINT32 uAScale = (uInst0 & ~EURASIA_USE0_DOT34_ASCALE_CLRMSK) >> EURASIA_USE0_DOT34_ASCALE_SHIFT;
	IMG_UINT32 uSrc1Mod = (uInst1 & ~EURASIA_USE1_DOT34_SRC1MOD_CLRMSK) >> EURASIA_USE1_DOT34_SRC1MOD_SHIFT;
	IMG_UINT32 uSrc2Mod = (uInst1 & ~EURASIA_USE1_DOT34_SRC2MOD_CLRMSK) >> EURASIA_USE1_DOT34_SRC2MOD_SHIFT;
	IMG_UINT32 uAOp = (uInst1 & ~EURASIA_USE1_DOT34_AOP_CLRMSK) >> EURASIA_USE1_DOT34_AOP_SHIFT;
	IMG_UINT32 uAMod1 = (uInst1 & ~EURASIA_USE1_DOT34_AMOD1_CLRMSK) >> EURASIA_USE1_DOT34_AMOD1_SHIFT;
	IMG_UINT32 uAMod2 = (uInst1 & ~EURASIA_USE1_DOT34_AMOD2_CLRMSK) >> EURASIA_USE1_DOT34_AMOD2_SHIFT;
	IMG_UINT32 uASrc1Mod = (uInst0 & ~EURASIA_USE0_DOT34_ASRC1MOD_CLRMSK) >> EURASIA_USE0_DOT34_ASRC1MOD_SHIFT;
	static const USEASM_INTSRCSEL aeCScale[] = 
	{
		USEASM_INTSRCSEL_CX1,	/* EURASIA_USE1_DOT34_CSCALE_X1 */ 
		USEASM_INTSRCSEL_CX2,	/* EURASIA_USE1_DOT34_CSCALE_X2 */
		USEASM_INTSRCSEL_CX4,	/* EURASIA_USE1_DOT34_CSCALE_X4 */
		USEASM_INTSRCSEL_CX8,	/* EURASIA_USE1_DOT34_CSCALE_X8 */
	};
	static const USEASM_INTSRCSEL aeAScale[] = 
	{
		USEASM_INTSRCSEL_AX1,	/* EURASIA_USE0_DOT34_ASCALE_X1 */ 
		USEASM_INTSRCSEL_AX2,	/* EURASIA_USE0_DOT34_ASCALE_X2 */
		USEASM_INTSRCSEL_AX4,	/* EURASIA_USE0_DOT34_ASCALE_X4 */
		USEASM_INTSRCSEL_AX8,	/* EURASIA_USE0_DOT34_ASCALE_X8 */
	};
	IMG_UINT32 uSPred;
	static const USEASM_OPCODE aeU8Op[] =
	{
		USEASM_OP_U8DOT3,		/* EURASIA_USE1_DOT34_OPSEL_DOT3 */
		USEASM_OP_U8DOT4,		/* EURASIA_USE1_DOT34_OPSEL_DOT4 */
	};
	static const USEASM_OPCODE aeU8OpOff[] =
	{
		USEASM_OP_U8DOT3OFF,	/* EURASIA_USE1_DOT34_OPSEL_DOT3 */
		USEASM_OP_U8DOT4OFF,	/* EURASIA_USE1_DOT34_OPSEL_DOT4 */
	};
	static const USEASM_OPCODE aeU16Op[] =
	{
		USEASM_OP_U16DOT3,		/* EURASIA_USE1_DOT34_OPSEL_DOT3 */
		USEASM_OP_U16DOT4,		/* EURASIA_USE1_DOT34_OPSEL_DOT4 */
	};
	static const USEASM_OPCODE aeU16OpOff[] =
	{
		USEASM_OP_U16DOT3OFF,	/* EURASIA_USE1_DOT34_OPSEL_DOT3 */
		USEASM_OP_U16DOT4OFF,	/* EURASIA_USE1_DOT34_OPSEL_DOT4 */
	};

	UseAsmInitInst(psInst);

	/*
		Decode predicate.
	*/
	uSPred = (uInst1 & ~EURASIA_USE1_SPRED_CLRMSK) >> EURASIA_USE1_SPRED_SHIFT;
	psInst->uFlags1 |= (g_aeDecodeSPred[uSPred] << USEASM_OPFLAGS1_PRED_SHIFT);

	/*
		Decode instruction opcode.
	*/
	if (uASel1 == EURASIA_USE1_DOT34_ASEL1_ONEWAY)
	{
		if (uInst1 & EURASIA_USE1_DOT34_OFF)
		{
			psInst->uOpcode = aeU16OpOff[uOpSel];
		}
		else
		{
			psInst->uOpcode = aeU16Op[uOpSel];
		}
	}
	else
	{
		if (uInst1 & EURASIA_USE1_DOT34_OFF)
		{
			psInst->uOpcode = aeU8OpOff[uOpSel];
		}
		else
		{
			psInst->uOpcode = aeU8Op[uOpSel];
		}
	}

	/*
		Decode instruction modifier flags.
	*/
	psInst->uFlags1 |= ((uRCount + 1) << USEASM_OPFLAGS1_REPEAT_SHIFT);
	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);
	if (uInst1 & EURASIA_USE1_END)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_INT_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	
	/*
		Decode destination.
	*/
	DecodeDest(psTarget, uInst0, uInst1, TRUE, &psInst->asArg[0], psRuntimeState->eColourFormatControl, USEASM_ARGFLAGS_FMTC10);

	/*
		Decode colour scale destination modifier.
	*/
	SetupIntSrcSel(&psInst->asArg[1], aeCScale[uCScale]);

	/*
		Decode alpha scale destination modifier.
	*/
	if ((uOpSel == EURASIA_USE1_DOT34_OPSEL_DOT4) || (uASel2 == EURASIA_USE1_DOT34_ASEL2_FOURWAY))
	{
		SetupIntSrcSel(&psInst->asArg[2], aeAScale[uAScale]);
	}
	else
	{
		SetupIntSrcSel(&psInst->asArg[2], USEASM_INTSRCSEL_NONE);
	}

	/*
		Decode source 1.
	*/
	DecodeSrc12(psTarget, uInst0, uInst1, 1, TRUE, EURASIA_USE1_S1BEXT, &psInst->asArg[3], psRuntimeState->eColourFormatControl, USEASM_ARGFLAGS_FMTC10);
	if (uSrc1Mod == EURASIA_USE1_DOT34_SRC1MOD_COMPLEMENT)
	{
		psInst->asArg[3].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
	}

	/*
		Decode source 2.
	*/
	DecodeSrc12(psTarget, uInst0, uInst1, 2, TRUE, EURASIA_USE1_S2BEXT, &psInst->asArg[4], psRuntimeState->eColourFormatControl, USEASM_ARGFLAGS_FMTC10);
	if (uSrc2Mod == EURASIA_USE1_DOT34_SRC2MOD_COMPLEMENT)
	{
		psInst->asArg[4].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
	}

	/*
		Decode co-issued instruction.
	*/
	if ((uOpSel == EURASIA_USE1_DOT34_OPSEL_DOT3) && 
		(uASel1 != EURASIA_USE1_DOT34_ASEL1_ONEWAY) &&
		(uASel2 != EURASIA_USE1_DOT34_ASEL2_FOURWAY))
	{
		static const USEASM_INTSRCSEL aeASel1[] =
		{
			USEASM_INTSRCSEL_ZERO,		/* EURASIA_USE1_DOT34_ASEL1_ZERO */
			USEASM_INTSRCSEL_SRC1ALPHA,	/* EURASIA_USE1_DOT34_ASEL1_SRC1ALPHA */
			USEASM_INTSRCSEL_SRC2ALPHA,	/* EURASIA_USE1_DOT34_ASEL1_SRC2ALPHA */
		};
		static const USEASM_INTSRCSEL aeASel2[] =
		{
			USEASM_INTSRCSEL_ZERO,		/* EURASIA_USE1_DOT34_ASEL2_ZERO */
			USEASM_INTSRCSEL_SRC1ALPHA,	/* EURASIA_USE1_DOT34_ASEL2_SRC1ALPHA */
			USEASM_INTSRCSEL_SRC2ALPHA,	/* EURASIA_USE1_DOT34_ASEL2_SRC2ALPHA */
		};

		psInst->uFlags1 |= USEASM_OPFLAGS1_MAINISSUE;

		UseAsmInitInst(psCoInst);
		psCoInst->uFlags2 |= USEASM_OPFLAGS2_COISSUE;
		psCoInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

		if (uAOp == EURASIA_USE1_DOT34_AOP_INTERPOLATE1 || uAOp == EURASIA_USE1_DOT34_AOP_INTERPOLATE2)
		{		
			if (uAOp == EURASIA_USE1_DOT34_AOP_INTERPOLATE1)
			{
				psCoInst->uOpcode = USEASM_OP_AINTRP1;
			}
			else
			{
				psCoInst->uOpcode = USEASM_OP_AINTRP2;
			}

			DecodeSrc0(psTarget, uInst0, uInst1, FALSE, 0, &psCoInst->asArg[0], psRuntimeState->eColourFormatControl, 0);

			SetupIntSrcSel(&psCoInst->asArg[1], aeASel1[uASel1]);

			SetupIntSrcSel(&psCoInst->asArg[2], aeASel2[uASel2]);
			if (uAMod2 == EURASIA_USE1_DOT34_AMOD2_COMPLEMENT)
			{
				psCoInst->asArg[2].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
			}
		}
		else
		{
			if (uAOp == EURASIA_USE1_DOT34_AOP_ADD)
			{
				psCoInst->uOpcode = USEASM_OP_AADD;
			}
			else
			{
				psCoInst->uOpcode = USEASM_OP_ASUB;
			}

			SetupIntSrcSel(&psCoInst->asArg[0], aeAScale[uAScale]);

			SetupIntSrcSel(&psCoInst->asArg[1], aeASel1[uASel1]);
			if (uAMod1 == EURASIA_USE1_DOT34_AMOD1_COMPLEMENT)
			{
				psCoInst->asArg[1].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
			}

			SetupIntSrcSel(&psCoInst->asArg[2], USEASM_INTSRCSEL_SRC1ALPHA);
			if (uASrc1Mod == EURASIA_USE0_DOT34_ASRC1MOD_COMPLEMENT)
			{
				psCoInst->asArg[2].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
			}

			SetupIntSrcSel(&psCoInst->asArg[3], aeASel2[uASel2]);
			if (uAMod2 == EURASIA_USE1_DOT34_AMOD2_COMPLEMENT)
			{
				psCoInst->asArg[3].uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
			}

			SetupIntSrcSel(&psCoInst->asArg[4], USEASM_INTSRCSEL_SRC2ALPHA);
		}
	}
	
	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeIMAEInstruction(PCSGX_CORE_DESC	psTarget,
										     IMG_UINT32		uInst0,
											 IMG_UINT32		uInst1,
											 PUSE_INST		psInst)
{
	IMG_UINT32 uRCount = (uInst1 & ~EURASIA_USE1_INT_RCOUNT_CLRMSK) >> EURASIA_USE1_INT_RCOUNT_SHIFT;
	IMG_UINT32 uSrc2Type = (uInst1 & ~EURASIA_USE1_IMAE_SRC2TYPE_CLRMSK) >> EURASIA_USE1_IMAE_SRC2TYPE_SHIFT;
	IMG_UINT32 uORShift = (uInst1 & ~EURASIA_USE1_IMAE_ORSHIFT_CLRMSK) >> EURASIA_USE1_IMAE_ORSHIFT_SHIFT;
	static const USEASM_INTSRCSEL aeSrcFmt[] = 
	{
		USEASM_INTSRCSEL_Z16,	/* EURASIA_USE1_IMAE_SRC2TYPE_16BITZEXTEND */ 
		USEASM_INTSRCSEL_S16,	/* EURASIA_USE1_IMAE_SRC2TYPE_16BITSEXTEND */
		USEASM_INTSRCSEL_U32,	/* EURASIA_USE1_IMAE_SRC2TYPE_32BIT */
		USEASM_INTSRCSEL_UNDEF,	/* EURASIA_USE1_IMAE_SRC2TYPE_RESERVED */
	};
	USEASM_PRED ePredicate;

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*	
		Set instruction opcode.
	*/
	psInst->uOpcode = USEASM_OP_IMAE;

	/*
		Decode predicate.
	*/
	ePredicate = g_aeDecodeSPred[(uInst1 & ~EURASIA_USE1_SPRED_CLRMSK) >> EURASIA_USE1_SPRED_SHIFT];
	psInst->uFlags1 |= (ePredicate << USEASM_OPFLAGS1_PRED_SHIFT);

	/*
		Decode instruction modifiers.
	*/
	if (uInst1 & EURASIA_USE1_IMAE_SIGNED)
	{
		psInst->uFlags2 |= USEASM_OPFLAGS2_SIGNED;
	}
	else
	{
		psInst->uFlags2 |= USEASM_OPFLAGS2_UNSIGNED;
	}
	if (uInst1 & EURASIA_USE1_IMAE_SATURATE)
	{
		psInst->uFlags2 |= USEASM_OPFLAGS2_SAT;
	}
	if (uORShift > 0)
	{
		psInst->uFlags2 |= (uORShift << USEASM_OPFLAGS2_RSHIFT_SHIFT);
	}
	if (uInst1 & EURASIA_USE1_END)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_INT_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}

	/*
		Decode instruction repeat count.
	*/
	psInst->uFlags1 |= ((uRCount + 1) << USEASM_OPFLAGS1_REPEAT_SHIFT);
	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

	/*
		Decode destination register.
	*/
	DecodeDest(psTarget, uInst0, uInst1, TRUE, &psInst->asArg[0], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	
	/*
		Decode source 0 register.
	*/
	DecodeSrc0(psTarget, uInst0, uInst1, FALSE, 0, &psInst->asArg[1], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	if (uInst1 & EURASIA_USE1_IMAE_SRC0H_SELECTHIGH)
	{
		psInst->asArg[1].uFlags |= USEASM_ARGFLAGS_HIGH;
	}
	else
	{
		psInst->asArg[1].uFlags |= USEASM_ARGFLAGS_LOW;
	}

	/*
		Decode source 1 register.
	*/
	DecodeSrc12(psTarget, uInst0, uInst1, 1, TRUE, EURASIA_USE1_S1BEXT, &psInst->asArg[2], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	if (uInst1 & EURASIA_USE1_IMAE_SRC1H_SELECTHIGH)
	{
		psInst->asArg[2].uFlags |= USEASM_ARGFLAGS_HIGH;
	}
	else
	{
		psInst->asArg[2].uFlags |= USEASM_ARGFLAGS_LOW;
	}

	/*
		Decode source 2 register.
	*/
	DecodeSrc12(psTarget, uInst0, uInst1, 2, TRUE, EURASIA_USE1_S2BEXT, &psInst->asArg[3], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	if (uSrc2Type != EURASIA_USE1_IMAE_SRC2TYPE_32BIT)
	{
		if (uInst1 & EURASIA_USE1_IMAE_SRC2H_SELECTHIGH)
		{
			psInst->asArg[3].uFlags |= USEASM_ARGFLAGS_HIGH;
		}
		else
		{
			psInst->asArg[3].uFlags |= USEASM_ARGFLAGS_LOW;
		}
	}

	/*
		Decode source 2 argument type.
	*/
	if (aeSrcFmt[uSrc2Type] == USEASM_INTSRCSEL_UNDEF)
	{
		return USEDISASM_ERROR_INVALID_IMAE_SRC2_TYPE;
	}
	SetupIntSrcSel(&psInst->asArg[4], aeSrcFmt[uSrc2Type]);

	/*
		Decode carry-out flag.
	*/
	if (uInst1 & EURASIA_USE1_IMAE_CARRYOUTENABLE)
	{
		SetupIntSrcSel(&psInst->asArg[5], USEASM_INTSRCSEL_COUTENABLE);
	}
	else
	{
		SetupIntSrcSel(&psInst->asArg[5], USEASM_INTSRCSEL_NONE);
	}

	/*
		Decode carry-in flag.
	*/
	if (uInst1 & EURASIA_USE1_IMAE_CARRYINENABLE)
	{
		SetupIntSrcSel(&psInst->asArg[6], USEASM_INTSRCSEL_CINENABLE);
	}
	else
	{
		SetupIntSrcSel(&psInst->asArg[6], USEASM_INTSRCSEL_NONE);
	}

	/*
		Decode internal register for carry-in/carry-out.
	*/
	if ((uInst1 & EURASIA_USE1_IMAE_CARRYINENABLE) || (uInst1 & EURASIA_USE1_IMAE_CARRYOUTENABLE))
	{
		psInst->asArg[7].uType = USEASM_REGTYPE_FPINTERNAL;
		psInst->asArg[7].uNumber = (uInst1 & ~EURASIA_USE1_IMAE_CISRC_CLRMSK) >> EURASIA_USE1_IMAE_CISRC_SHIFT;
		psInst->asArg[7].uFlags = 0;
		psInst->asArg[7].uIndex = USEREG_INDEX_NONE;
	}
	else
	{
		SetupIntSrcSel(&psInst->asArg[7], USEASM_INTSRCSEL_NONE);
	}

	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeIMA16Instruction(PCSGX_CORE_DESC	psTarget,
											  IMG_UINT32		uInst0,
											  IMG_UINT32		uInst1,
											  PUSE_INST			psInst)
{
	IMG_UINT32 uRCount = (uInst1 & ~EURASIA_USE1_INT_RCOUNT_CLRMSK) >> EURASIA_USE1_INT_RCOUNT_SHIFT;
	IMG_UINT32 uMode = (uInst1 & ~EURASIA_USE1_IMA16_MODE_CLRMSK) >> EURASIA_USE1_IMA16_MODE_SHIFT;
	IMG_UINT32 uSrc1Fmt = (uInst1 & ~EURASIA_USE1_IMA16_S1FORM_CLRMSK) >> EURASIA_USE1_IMA16_S1FORM_SHIFT;
	IMG_UINT32 uSrc2Fmt = (uInst1 & ~EURASIA_USE1_IMA16_S2FORM_CLRMSK) >> EURASIA_USE1_IMA16_S2FORM_SHIFT;
	IMG_UINT32 uORShift = (uInst1 & ~EURASIA_USE1_IMA16_ORSHIFT_CLRMSK) >> EURASIA_USE1_IMA16_ORSHIFT_SHIFT;
	static const USEASM_INTSRCSEL aeSrcFmt[] = 
	{
		USEASM_INTSRCSEL_U16,	/* EURASIA_USE1_IMA16_SFORM_16BIT */
		USEASM_INTSRCSEL_U8,	/* EURASIA_USE1_IMA16_SFORM_8BITZEROEXTEND */
		USEASM_INTSRCSEL_S8,	/* EURASIA_USE1_IMA16_SFORM_8BITSGNEXTEND */
		USEASM_INTSRCSEL_O8,	/* EURASIA_USE1_IMA16_SFORM_8BITOFFSET */
	};
	USEASM_PRED ePredicate;

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*	
		Set instruction opcode.
	*/
	psInst->uOpcode = USEASM_OP_IMA16;

	/*
		Decode predicate.
	*/
	ePredicate = g_aeDecodeSPred[(uInst1 & ~EURASIA_USE1_SPRED_CLRMSK) >> EURASIA_USE1_SPRED_SHIFT];
	psInst->uFlags1 |= (ePredicate << USEASM_OPFLAGS1_PRED_SHIFT);

	/*
		Decode instruction modifiers.
	*/
	psInst->uFlags2 |= (uORShift << USEASM_OPFLAGS2_RSHIFT_SHIFT);
	psInst->uFlags1 |= ((uRCount + 1) << USEASM_OPFLAGS1_REPEAT_SHIFT);
	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);
	if (uInst1 & EURASIA_USE1_END)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_INT_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	if (uMode == EURASIA_USE1_IMA16_MODE_UNSGNNOSAT)
	{
		psInst->uFlags2 |= USEASM_OPFLAGS2_UNSIGNED;
	}
	else if (uMode == EURASIA_USE1_IMA16_MODE_UNSGNSAT)
	{
		psInst->uFlags2 |= USEASM_OPFLAGS2_UNSIGNED | USEASM_OPFLAGS2_SAT;
	}
	else if (uMode == EURASIA_USE1_IMA16_MODE_SGNNOSAT)
	{
		psInst->uFlags2 |= USEASM_OPFLAGS2_SIGNED;
	}
	else if (uMode == EURASIA_USE1_IMA16_MODE_SGNSAT)
	{
		psInst->uFlags2 |= USEASM_OPFLAGS2_SIGNED | USEASM_OPFLAGS2_SAT;
	}

	/*
		Decode destination.
	*/
	DecodeDest(psTarget, uInst0, uInst1, TRUE, &psInst->asArg[0], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	if (uInst1 & EURASIA_USE1_IMA16_ABS)
	{
		psInst->asArg[0].uFlags |= USEASM_ARGFLAGS_ABSOLUTE;
	}
	
	/*
		Decode source 0
	*/
	DecodeSrc0(psTarget, uInst0, uInst1, FALSE, 0, &psInst->asArg[1], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	
	/*
		Decode source 1.
	*/
	DecodeSrc12(psTarget, uInst0, uInst1, 1, TRUE, EURASIA_USE1_S1BEXT, &psInst->asArg[2], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	if (uSrc1Fmt != EURASIA_USE1_IMA16_SFORM_16BIT)
	{
		if (uInst1 & EURASIA_USE1_IMA16_SEL1H_UPPER8)
		{
			psInst->asArg[2].uFlags |= USEASM_ARGFLAGS_HIGH;
		}
		else
		{
			psInst->asArg[2].uFlags |= USEASM_ARGFLAGS_LOW;
		}
	}
	
	/*
		Decode source 2.
	*/
	DecodeSrc12(psTarget, uInst0, uInst1, 2, TRUE, EURASIA_USE1_S2BEXT, &psInst->asArg[3], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	if (uSrc2Fmt != EURASIA_USE1_IMA16_SFORM_16BIT)
	{
		if (uInst1 & EURASIA_USE1_IMA16_SEL2H_UPPER8)
		{
			psInst->asArg[3].uFlags |= USEASM_ARGFLAGS_HIGH;
		}
		else
		{
			psInst->asArg[3].uFlags |= USEASM_ARGFLAGS_LOW;
		}
	}
	if (uInst1 & EURASIA_USE1_IMA16_S2NEG)
	{
		psInst->asArg[3].uFlags |= USEASM_ARGFLAGS_NEGATE;
	}

	/*
		Decode source 1 format.
	*/
	SetupIntSrcSel(&psInst->asArg[4], aeSrcFmt[uSrc1Fmt]);

	/*
		Decode source 2 format.
	*/
	SetupIntSrcSel(&psInst->asArg[5], aeSrcFmt[uSrc2Fmt]);

	return USEDISASM_OK;
}

static IMG_VOID DecodeStandardIntegerInstructionModifiers(IMG_UINT32	uInst1,
														  PUSE_INST		psInst)
{
	IMG_UINT32 uRCount = (uInst1 & ~EURASIA_USE1_INT_RCOUNT_CLRMSK) >> EURASIA_USE1_INT_RCOUNT_SHIFT;
	USEASM_PRED ePredicate;

	/*
		Decode predicate.
	*/
	ePredicate = g_aeDecodeSPred[(uInst1 & ~EURASIA_USE1_SPRED_CLRMSK) >> EURASIA_USE1_SPRED_SHIFT];
	psInst->uFlags1 |= (ePredicate << USEASM_OPFLAGS1_PRED_SHIFT);

	/*
		Decode repeat count.
	*/
	psInst->uFlags1 |= ((uRCount + 1) << USEASM_OPFLAGS1_REPEAT_SHIFT);
	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

	/*
		Decode end flag.
	*/
	if (uInst1 & EURASIA_USE1_END)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}

	/*
		Decode skip invalid instances flag.
	*/
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}

	/*
		Decode nosched flag.
	*/
	if (uInst1 & EURASIA_USE1_INT_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
}

static USEDISASM_ERROR DecodeFIRVInstruction(PCSGX_CORE_DESC	psTarget,
											 IMG_UINT32		uInst0,
											 IMG_UINT32		uInst1,
											 PUSE_INST		psInst)
{
	static const USEASM_INTSRCSEL aeSrcFormat[] = 
	{
		USEASM_INTSRCSEL_U8,		/* EURASIA_USE1_FIRV_SRCFORMAT_U8 */
		USEASM_INTSRCSEL_S8,		/* EURASIA_USE1_FIRV_SRCFORMAT_S8 */
		USEASM_INTSRCSEL_O8,		/* EURASIA_USE1_FIRV_SRCFORMAT_O8 */
		USEASM_INTSRCSEL_UNDEF
	};
	IMG_UINT32 uSrcFormat = (uInst1 & ~EURASIA_USE1_FIRV_SRCFORMAT_CLRMSK) >> EURASIA_USE1_FIRV_SRCFORMAT_SHIFT;
	IMG_BOOL bFirvh;
	PUSE_REGISTER psNextArg;

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*	
		Set instruction opcode.
	*/
	if (uSrcFormat == EURASIA_USE1_FIRV_SRCFORMAT_FIRVH)
	{
		if (!IsHighPrecisionFIR(psTarget))
		{
			return USEDISASM_ERROR_INVALID_FIRV_FLAG;
		}
		psInst->uOpcode = USEASM_OP_FIRVH;
		bFirvh = IMG_TRUE;
	}
	else
	{
		psInst->uOpcode = USEASM_OP_FIRV;
		bFirvh = IMG_FALSE;
	}

	/*
		Decode instruction modifiers.
	*/
	DecodeStandardIntegerInstructionModifiers(uInst1, psInst);
	
	/*
		Decode destination.
	*/
	psNextArg = &psInst->asArg[0];
	DecodeDest(psTarget, uInst0, uInst1, TRUE, psNextArg, USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	if (!(uInst1 & EURASIA_USE1_FIRV_WBENABLE))
	{
		psNextArg->uFlags |= USEASM_ARGFLAGS_DISABLEWB;
	}
	psNextArg++;

	/*
		Decode source 0 and source 1.
	*/
	if (bFirvh)
	{
		psNextArg->uType = USEASM_REGTYPE_FPINTERNAL;
		psNextArg->uNumber = (uInst0 & ~EURASIA_USE0_SRC0_CLRMSK) >> EURASIA_USE0_SRC0_SHIFT;
		psNextArg++;
	}
	else
	{
		DecodeSrc12(psTarget, uInst0, uInst1, 0, TRUE, EURASIA_USE1_S1BEXT, psNextArg, USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
		psNextArg++;
		
		DecodeSrc12(psTarget, uInst0, uInst1, 1, TRUE, EURASIA_USE1_S1BEXT, psNextArg, USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
		psNextArg++;
	}
	
	/*
		Decode source 2.
	*/
	DecodeSrc12(psTarget, uInst0, uInst1, 2, TRUE, EURASIA_USE1_S2BEXT, psNextArg, USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	psNextArg++;

	/*
		Decode source format.
	*/
	if (!bFirvh)
	{
		SetupIntSrcSel(psNextArg, aeSrcFormat[uSrcFormat]);
		psNextArg++;
	}

	/*
		Decode instruction parameters.
	*/
	SetupIntSrcSel(psNextArg, (uInst1 & EURASIA_USE1_FIRV_COEFADD) ? USEASM_INTSRCSEL_PLUSSRC2DOT2 : USEASM_INTSRCSEL_NONE);
	psNextArg++;

	SetupIntSrcSel(psNextArg, (uInst1 & EURASIA_USE1_IREGSRC) ? USEASM_INTSRCSEL_IADD : USEASM_INTSRCSEL_NONE);
	psNextArg++;

	SetupIntSrcSel(psNextArg, (uInst1 & EURASIA_USE1_FIRV_SHIFTEN) ? USEASM_INTSRCSEL_SCALE : USEASM_INTSRCSEL_NONE);
	psNextArg++;

	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeSSUM16Instruction(PCSGX_CORE_DESC	psTarget,
											   IMG_UINT32		uInst0,
											   IMG_UINT32		uInst1,
											   PUSE_INST		psInst)
{
	if (IsHighPrecisionFIR(psTarget))
	{
		IMG_UINT32 uRndMode = (uInst1 & ~EURASIA_USE1_SSUM16_RMODE_CLRMSK) >> EURASIA_USE1_SSUM16_RMODE_SHIFT;
		IMG_UINT32 uRShift = (uInst1 & ~EURASIA_USE1_SSUM16_RSHIFT_CLRMSK) >> EURASIA_USE1_SSUM16_RSHIFT_SHIFT;
		IMG_UINT32 uSrcFormat = (uInst1 & ~EURASIA_USE1_SSUM16_SRCFORMAT_CLRMSK) >> EURASIA_USE1_SSUM16_SRCFORMAT_SHIFT;
		static const USEASM_INTSRCSEL aeRndMode[] = 
		{
			USEASM_INTSRCSEL_ROUNDDOWN,		/* EURASIA_USE1_SSUM16_RMODE_DOWN */
			USEASM_INTSRCSEL_ROUNDNEAREST,	/* EURASIA_USE1_SSUM16_RMODE_NEAREST */
			USEASM_INTSRCSEL_ROUNDUP,		/* EURASIA_USE1_SSUM16_RMODE_UP */ 
			USEASM_INTSRCSEL_UNDEF,			/* EURASIA_USE1_SSUM16_RMODE_RESERVED */
		};
		static const USEASM_INTSRCSEL aeSrcFormat[] = 
		{
			USEASM_INTSRCSEL_U8,	/* EURASIA_USE1_SSUM16_SRCFORMAT_U8 */
			USEASM_INTSRCSEL_S8,	/* EURASIA_USE1_SSUM16_SRCFORMAT_S8 */
		};

		/*
			Initialize the instruction.
		*/
		UseAsmInitInst(psInst);

		/*	
			Set instruction opcode.
		*/
		psInst->uOpcode = USEASM_OP_SSUM16;

		/*
			Decode instruction modifiers.
		*/
		DecodeStandardIntegerInstructionModifiers(uInst1, psInst);

		/*
			Decode destination right shift.
		*/
		psInst->uFlags2 |= (uRShift << USEASM_OPFLAGS2_RSHIFT_SHIFT);
		
		/*	
			Decode destination.
		*/
		DecodeDest(psTarget, uInst0, uInst1, TRUE, &psInst->asArg[0], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
		
		/*
			Decode source 0.
		*/
		DecodeSrc0(psTarget, uInst0, uInst1, FALSE, EURASIA_USE1_S0BEXT, &psInst->asArg[1], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
		
		/*
			Decode source 1.
		*/
		DecodeSrc12(psTarget, uInst0, uInst1, 1, TRUE, EURASIA_USE1_S1BEXT, &psInst->asArg[2], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
		
		/*
			Decode source 2.
		*/
		DecodeSrc12(psTarget, uInst0, uInst1, 2, TRUE, EURASIA_USE1_S2BEXT, &psInst->asArg[3], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);

		SetupIntSrcSel(&psInst->asArg[4], (uInst1 & EURASIA_USE1_SSUM16_IDST) ? USEASM_INTSRCSEL_IDST : USEASM_INTSRCSEL_NONE);
		SetupIntSrcSel(&psInst->asArg[5], (uInst1 & EURASIA_USE1_SSUM16_IADD) ? USEASM_INTSRCSEL_IADD : USEASM_INTSRCSEL_NONE);

		if (uInst1 & (EURASIA_USE1_SSUM16_IDST | EURASIA_USE1_SSUM16_IADD))
		{
			psInst->asArg[6].uType = USEASM_REGTYPE_FPINTERNAL;
			psInst->asArg[6].uNumber = (uInst1 & EURASIA_USE1_SSUM16_ISEL_I1) ? 1 : 0;
		}
		else
		{
			SetupIntSrcSel(&psInst->asArg[6], USEASM_INTSRCSEL_NONE);
		}

		if (aeRndMode[uRndMode] == USEASM_INTSRCSEL_UNDEF)
		{
			return USEDISASM_ERROR_INVALID_SSUM16_ROUND_MODE;
		}
		SetupIntSrcSel(&psInst->asArg[7], aeRndMode[uRndMode]);

		SetupIntSrcSel(&psInst->asArg[8], aeSrcFormat[uSrcFormat]);

		return USEDISASM_OK;
	}
	else
	{
		return USEDISASM_ERROR_INVALID_ADIFFIRVBILIN_OPSEL;
	}
}

static USEDISASM_ERROR DecodeADIFInstruction(PCSGX_CORE_DESC	psTarget,
											 IMG_UINT32		uInst0,
											 IMG_UINT32		uInst1,
											 PUSE_INST		psInst)
{
	PUSE_REGISTER	psNextArg;

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*	
		Set instruction opcode.
	*/
	if (uInst1 & EURASIA_USE1_ADIF_SUM)
	{
		psInst->uOpcode = USEASM_OP_ADIFSUM;
	}
	else
	{
		psInst->uOpcode = USEASM_OP_ADIF;
	}

	/*
		Decode instruction modifiers.
	*/
	DecodeStandardIntegerInstructionModifiers(uInst1, psInst);

	/*
		Decode destination.
	*/
	psNextArg = &psInst->asArg[0];
	DecodeDest(psTarget, uInst0, uInst1, TRUE, psNextArg, USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	psNextArg++;

	/*
		Decode source 0.
	*/
	if (uInst1 & EURASIA_USE1_ADIF_SUM)
	{
		DecodeSrc0(psTarget, uInst0, uInst1, FALSE, EURASIA_USE1_S0BEXT, psNextArg, USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
		if (uInst1 & EURASIA_USE1_ADIF_SRC0H_SELECTHIGH)
		{
			psNextArg->uFlags |= USEASM_ARGFLAGS_HIGH;
		}
		else
		{
			psNextArg->uFlags |= USEASM_ARGFLAGS_LOW;
		}
		psNextArg++;
	}

	/*
		Decode source 1.
	*/
	DecodeSrc12(psTarget, uInst0, uInst1, 1, TRUE, EURASIA_USE1_S1BEXT, psNextArg, USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	psNextArg++;
	
	/*
		Decode source 2.
	*/
	DecodeSrc12(psTarget, uInst0, uInst1, 2, TRUE, EURASIA_USE1_S2BEXT, psNextArg, USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	psNextArg++;

	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeBILINInstruction(PCSGX_CORE_DESC	psTarget,
											  IMG_UINT32		uInst0,
											  IMG_UINT32		uInst1,
											  PUSE_INST			psInst)
{
	static const USEASM_INTSRCSEL aeFormat[] = 
	{
		USEASM_INTSRCSEL_U8,	/* EURASIA_USE1_BILIN_SRCFORMAT_U8 */
		USEASM_INTSRCSEL_S8,	/* EURASIA_USE1_BILIN_SRCFORMAT_S8 */ 
		USEASM_INTSRCSEL_O8,	/* EURASIA_USE1_BILIN_SRCFORMAT_O8 */ 
		USEASM_INTSRCSEL_UNDEF, /* EURASIA_USE1_BILIN_SRCFORMAT_RESERVED */		
	};
	IMG_UINT32 uSrcSel = (uInst1 & ~EURASIA_USE1_BILIN_SRCCHANSEL_CLRMSK) >> EURASIA_USE1_BILIN_SRCCHANSEL_SHIFT;
	IMG_UINT32 uDstSel = (uInst1 & ~EURASIA_USE1_BILIN_DESTCHANSEL_CLRMSK) >> EURASIA_USE1_BILIN_DESTCHANSEL_SHIFT;
	IMG_UINT32 uSrcFormat;

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*	
		Set instruction opcode.
	*/
	psInst->uOpcode = USEASM_OP_BILIN;

	/*
		Decode instruction modifiers.
	*/
	DecodeStandardIntegerInstructionModifiers(uInst1, psInst);

	/*
		Decode destination.
	*/
	DecodeDest(psTarget, uInst0, uInst1, TRUE, &psInst->asArg[0], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	
	/*
		Decode source 0.
	*/
	DecodeSrc0(psTarget, uInst0, uInst1, FALSE, 0, &psInst->asArg[1], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	
	/*
		Decode source 1.
	*/
	DecodeSrc12(psTarget, uInst0, uInst1, 1, TRUE, EURASIA_USE1_S1BEXT, &psInst->asArg[2], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	
	/*
		Decode source 2.
	*/
	DecodeSrc12(psTarget, uInst0, uInst1, 2, TRUE, EURASIA_USE1_S2BEXT, &psInst->asArg[3], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	
	/*
		Decode source format.
	*/
	uSrcFormat = (uInst1 & ~EURASIA_USE1_BILIN_SRCFORMAT_CLRMSK) >> EURASIA_USE1_BILIN_SRCFORMAT_SHIFT;
	if (aeFormat[uSrcFormat] == USEASM_INTSRCSEL_UNDEF)
	{
		return USEDISASM_ERROR_INVALID_BILIN_SOURCE_FORMAT;
	}
	SetupIntSrcSel(&psInst->asArg[4], aeFormat[uSrcFormat]);

	SetupIntSrcSel(&psInst->asArg[5], 
				   (uInst1 & EURASIA_USE1_BILIN_INTERLEAVED) ? USEASM_INTSRCSEL_INTERLEAVED : USEASM_INTSRCSEL_PLANAR);
	SetupIntSrcSel(&psInst->asArg[6], 
				   (uSrcSel == EURASIA_USE1_BILIN_SRCCHANSEL_01) ? USEASM_INTSRCSEL_SRC01 : USEASM_INTSRCSEL_SRC23); 
	SetupIntSrcSel(&psInst->asArg[7], 
				   (uDstSel == EURASIA_USE1_BILIN_DESTCHANSEL_01) ? USEASM_INTSRCSEL_DST01 : USEASM_INTSRCSEL_DST23);
	SetupIntSrcSel(&psInst->asArg[8], (uInst1 & EURASIA_USE1_BILIN_RND) ? USEASM_INTSRCSEL_RND : USEASM_INTSRCSEL_NONE);

	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeADIFFIRVBILINInstruction(PCSGX_CORE_DESC	psTarget,
													  IMG_UINT32		uInst0,
													  IMG_UINT32		uInst1,
													  PUSE_INST			psInst)
{
	IMG_UINT32	uOpSel = (uInst1 & ~EURASIA_USE1_ADIFFIRVBILIN_OPSEL_CLRMSK) >> EURASIA_USE1_ADIFFIRVBILIN_OPSEL_SHIFT;

	switch (uOpSel)
	{
		case EURASIA_USE1_ADIFFIRVBILIN_OPSEL_FIRV:
		{
			return DecodeFIRVInstruction(psTarget, uInst0, uInst1, psInst);	
		}
		case EURASIA_USE1_ADIFFIRVBILIN_OPSEL_SSUM16:
		{
			return DecodeSSUM16Instruction(psTarget, uInst0, uInst1, psInst);
		}
		case EURASIA_USE1_ADIFFIRVBILIN_OPSEL_ADIF:
		{
			return DecodeADIFInstruction(psTarget, uInst0, uInst1, psInst);	
		}
		case EURASIA_USE1_ADIFFIRVBILIN_OPSEL_BILIN:
		{
			return DecodeBILINInstruction(psTarget, uInst0, uInst1, psInst);
		}
		default:
		{
			return USEDISASM_ERROR_INVALID_ADIFFIRVBILIN_OPSEL;
		}
	}
}

#if defined(SUPPORT_SGX_FEATURE_USE_32BIT_INT_MAD) || defined(SUPPORT_SGX_FEATURE_USE_INT_DIV)
static IMG_UINT32 DecodeBRN33442_Predicate(PCSGX_CORE_DESC psTarget, IMG_UINT32 uInst1)
{
	if (FixBRN33442(psTarget))
	{
		IMG_UINT32 uSPred;

		uSPred = (uInst1 & ~EURASIA_USE1_SPRED_CLRMSK) >> EURASIA_USE1_SPRED_SHIFT;
		return g_aeDecodeSPred[uSPred];
	}
	else
	{
		IMG_UINT32 uEPred;

		uEPred = (uInst1 & ~EURASIA_USE1_EPRED_CLRMSK) >> EURASIA_USE1_EPRED_SHIFT;
		return g_aeDecodeEPred[uEPred];
	}
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_32BIT_INT_MAD) || defined(SUPPORT_SGX_FEATURE_USE_INT_DIV) */
 
#if defined(SUPPORT_SGX_FEATURE_USE_32BIT_INT_MAD)
static USEDISASM_ERROR DecodeIMA32Instruction(PCSGX_CORE_DESC	psTarget,
											  IMG_UINT32		uInst0,
											  IMG_UINT32		uInst1,
											  PUSE_INST			psInst)
{
	if (!IsIMA32Instruction(psTarget))
	{
		return USEDISASM_ERROR_INVALID_OPCODE;
	}
	else
	{
		IMG_UINT32 uRptCount = (uInst1 & ~EURASIA_USE1_INT_RCOUNT_CLRMSK) >> EURASIA_USE1_INT_RCOUNT_SHIFT;
		IMG_UINT32 uCarryIn;

		UseAsmInitInst(psInst);

		/*
			Decode instruction predicate.
		*/
		psInst->uFlags1 |= (DecodeBRN33442_Predicate(psTarget, uInst1) << USEASM_OPFLAGS1_PRED_SHIFT);

		/*
			Set opcode.
		*/
		psInst->uOpcode = USEASM_OP_IMA32;

		/*
			Decode instruction modifier flags.
		*/
		if (uInst1 & EURASIA_USE1_SKIPINV)
		{
			psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
		}
		if (uInst1 & EURASIA_USE1_END)
		{
			psInst->uFlags1 |= USEASM_OPFLAGS1_END;
		}
		if (uInst1 & EURASIA_USE1_INT_NOSCHED)
		{
			psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
		}
		psInst->uFlags1 |= ((uRptCount + 1) << USEASM_OPFLAGS1_REPEAT_SHIFT);
		psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);
		if (uInst1 & EURASIA_USE1_IMA32_SGN)
		{
			psInst->uFlags2 |= USEASM_OPFLAGS2_SIGNED;
		}
		else
		{
			psInst->uFlags2 |= USEASM_OPFLAGS2_UNSIGNED;
		}
		
		/*
			Decode unified store destination.
		*/
		DecodeDest(psTarget, uInst0, uInst1, IMG_TRUE, &psInst->asArg[0], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);

		#if defined(SUPPORT_SGX_FEATURE_USE_IMA32_32X16_PLUS_32)
		if (IMA32Is32x16Plus32(psTarget))
		{
			IMG_UINT32	uStep;

			/*
				Decode step opcode.
			*/
			uStep = (uInst1 & ~SGXVEC_USE1_IMA32_STEP_CLRMSK) >> SGXVEC_USE1_IMA32_STEP_SHIFT;
			switch (uStep)
			{
				case SGXVEC_USE1_IMA32_STEP_STEP1: psInst->uFlags3 |= USEASM_OPFLAGS3_STEP1; break;
				case SGXVEC_USE1_IMA32_STEP_STEP2: psInst->uFlags3 |= USEASM_OPFLAGS3_STEP2; break;
				default: return USEDISASM_ERROR_INVALID_IMA32_STEP;
			}

			/*
				No internal register destination on this core.
			*/
			SetupIntSrcSel(&psInst->asArg[1], USEASM_INTSRCSEL_NONE);
		}
		else
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_IMA32_32X16_PLUS_32) */
		{
			/*
				Decode internal register destination.
			*/
			if (uInst1 & EURASIA_USE1_IMA32_GPIWEN)
			{
				IMG_UINT32	uGPISrc;
			
				uGPISrc = (uInst1 & ~EURASIA_USE1_IMA32_GPISRC_CLRMSK) >> EURASIA_USE1_IMA32_GPISRC_SHIFT;
				psInst->asArg[1].uType = USEASM_REGTYPE_FPINTERNAL;
				psInst->asArg[1].uNumber = uGPISrc;
			}
			else
			{
				SetupIntSrcSel(&psInst->asArg[1], USEASM_INTSRCSEL_NONE);
			}
		}

		/*
			Decode instruction sources.
		*/
		DecodeSrc0(psTarget, uInst0, uInst1, IMG_TRUE, EURASIA_USE1_IMA32_S0BEXT, &psInst->asArg[2], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
		DecodeSrc12(psTarget, uInst0, uInst1, 1, IMG_TRUE, EURASIA_USE1_S1BEXT, &psInst->asArg[3], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
		if (uInst1 & EURASIA_USE1_IMA32_NEGS1)
		{
			psInst->asArg[3].uFlags |= USEASM_ARGFLAGS_NEGATE;
		}
		DecodeSrc12(psTarget, uInst0, uInst1, 2, IMG_TRUE, EURASIA_USE1_S2BEXT, &psInst->asArg[4], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
		if (uInst1 & EURASIA_USE1_IMA32_NEGS2)
		{
			psInst->asArg[4].uFlags |= USEASM_ARGFLAGS_NEGATE;
		}

		/*
			Decode carry-in source.
		*/
		uCarryIn = (uInst1 & ~EURASIA_USE1_IMA32_CIEN_CLRMSK) >> EURASIA_USE1_IMA32_CIEN_SHIFT;
		if (uCarryIn == EURASIA_USE1_IMA32_CIEN_DISABLED)
		{
			SetupIntSrcSel(&psInst->asArg[5], USEASM_INTSRCSEL_NONE);
			SetupIntSrcSel(&psInst->asArg[6], USEASM_INTSRCSEL_NONE);
		}
		else
		{
			SetupIntSrcSel(&psInst->asArg[5], USEASM_INTSRCSEL_CINENABLE);
			psInst->asArg[6].uType = USEASM_REGTYPE_FPINTERNAL;
			switch (uCarryIn)
			{
				case EURASIA_USE1_IMA32_CIEN_I0_BIT0:
				{
					psInst->asArg[6].uNumber = 0;
					break;
				}
				case EURASIA_USE1_IMA32_CIEN_I1_BIT0:
				{
					psInst->asArg[6].uNumber = 1;
					break;
				}
				case EURASIA_USE1_IMA32_CIEN_I2_BIT0:
				{
					psInst->asArg[6].uNumber = 2;
					break;
				}
			}
		}
	}

	return USEDISASM_OK;
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_32BIT_INT_MAD) */

#if defined(SUPPORT_SGX_FEATURE_USE_INT_DIV)
static USEDISASM_ERROR DecodeIDIVInstruction(PCSGX_CORE_DESC	psTarget,
										     IMG_UINT32		uInst0,
										     IMG_UINT32		uInst1,
										     PUSE_INST		psInst)
{
	if (!IsIDIVInstruction(psTarget))
	{
		return USEDISASM_ERROR_INVALID_OPCODE;
	}
	else
	{
		IMG_UINT32 uRptCount = (uInst1 & ~EURASIA_USE1_INT_RCOUNT_CLRMSK) >> EURASIA_USE1_INT_RCOUNT_SHIFT;
		IMG_UINT32 uDRCSel;

		UseAsmInitInst(psInst);

		/*
			Decode instruction predicate.
		*/
		psInst->uFlags1 |= (DecodeBRN33442_Predicate(psTarget, uInst1) << USEASM_OPFLAGS1_PRED_SHIFT);

		/*
			Set instruction opcode.
		*/
		psInst->uOpcode = USEASM_OP_IDIV;

		/*
			Decode instruction modifiers.
		*/
		if (uInst1 & EURASIA_USE1_SKIPINV)
		{
			psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
		}
		if (uInst1 & EURASIA_USE1_END)
		{
			psInst->uFlags1 |= USEASM_OPFLAGS1_END;
		}
		if (uInst1 & EURASIA_USE1_INT_NOSCHED)
		{
			psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
		}
		psInst->uFlags1 |= ((uRptCount + 1) << USEASM_OPFLAGS1_REPEAT_SHIFT);
		psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);
		
		/*
			Decode destination register.
		*/
		if (uInst1 & SGX545_USE1_IDIV_DBANK_PRIMATTR)
		{
			psInst->asArg[0].uType = USEASM_REGTYPE_PRIMATTR;
		}
		else
		{
			psInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
		}
		psInst->asArg[0].uNumber = (uInst0 & ~EURASIA_USE0_DST_CLRMSK) >> EURASIA_USE0_DST_SHIFT;

		/*	
			Decode sources.
		*/
		DecodeSrc12(psTarget, uInst0, uInst1, 1, IMG_TRUE, EURASIA_USE1_S1BEXT, &psInst->asArg[1], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
		DecodeSrc12(psTarget, uInst0, uInst1, 2, IMG_TRUE, EURASIA_USE1_S2BEXT, &psInst->asArg[2], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
		
		/*
			Decode DRC argument.
		*/
		uDRCSel = (uInst1 & ~SGX545_USE1_IDIV_DRCSEL_CLRMSK) >> SGX545_USE1_IDIV_DRCSEL_SHIFT;
		SetupDRCSource(&psInst->asArg[3], uDRCSel);
	}
	return USEDISASM_OK;
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_INT_DIV) */

#if defined(SUPPORT_SGX_FEATURE_USE_INT_DIV) || defined(SUPPORT_SGX_FEATURE_USE_32BIT_INT_MAD)
static USEDISASM_ERROR Decode32BitIntegerInstruction(PCSGX_CORE_DESC	psTarget,
												     IMG_UINT32		uInst0,
												     IMG_UINT32		uInst1,
												     PUSE_INST		psInst)
{
	IMG_UINT32	uOp2 = (uInst1 & ~EURASIA_USE1_32BITINT_OP2_CLRMSK) >> EURASIA_USE1_32BITINT_OP2_SHIFT;

	#if defined(SUPPORT_SGX_FEATURE_USE_IMA32_32X16_PLUS_32)
	if (IMA32Is32x16Plus32(psTarget))
	{
		/*
			The OP2 field isn't used on this core.
		*/
		return DecodeIMA32Instruction(psTarget, uInst0, uInst1, psInst);	
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_IMA32_32X16_PLUS_32) */

	switch (uOp2)
	{
		case EURASIA_USE1_32BITINT_OP2_IMA32:
		{
			return DecodeIMA32Instruction(psTarget, uInst0, uInst1, psInst);		
		}
		#if defined(SUPPORT_SGX_FEATURE_USE_INT_DIV)
		case SGX545_USE1_32BITINT_OP2_IDIV:
		{
			return DecodeIDIVInstruction(psTarget, uInst0, uInst1, psInst);
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_INT_DIV) */
		default:
		{
			return USEDISASM_ERROR_INVALID_OPCODE;
		}
	}
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_INT_DIV) || defined(SUPPORT_SGX_FEATURE_USE_32BIT_INT_MAD) */

static USEDISASM_ERROR DecodeATST8Instruction(PCSGX_CORE_DESC psTarget,
											  IMG_UINT32     uInst0,
											  IMG_UINT32     uInst1,
											  PUSE_INST      psInst)
{
	IMG_UINT32	uSPred;

	/*
		Initialize the result to default values.
	*/
	UseAsmInitInst(psInst);

	/*
		No repeats on ATST8.
	*/
	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

	/*
		Decode instruction predicate.
	*/
	uSPred = (uInst1 & ~EURASIA_USE1_VISTEST_ATST8_SPRED_CLRMSK) >> EURASIA_USE1_VISTEST_ATST8_SPRED_SHIFT;
	psInst->uFlags1 |= (g_aeDecodeSPred[uSPred] << USEASM_OPFLAGS1_PRED_SHIFT);

	/*
		Set opcode.
	*/
	psInst->uOpcode = USEASM_OP_ATST8;

	/*
		Decode instruction flags.
	*/
	if (uInst1 & EURASIA_USE1_END)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}
	if (uInst1 & EURASIA_USE1_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	if (uInst1 & EURASIA_USE1_VISTEST_ATST8_SYNCS)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SYNCSTART;
	}
	if (uInst1 & EURASIA_USE1_VISTEST_ATST8_C10)
	{
		psInst->uFlags2 |= USEASM_OPFLAGS2_C10;
	}
	
	/*
		Decode instruction destination.
	*/
	DecodeDest(psTarget, uInst0, uInst1, FALSE, &psInst->asArg[0], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	
	/*
		Decode predicate destination.
	*/
	psInst->asArg[1].uNumber = (uInst1 & ~EURASIA_USE1_VISTEST_ATST8_PDST_CLRMSK) >> EURASIA_USE1_VISTEST_ATST8_PDST_SHIFT;
	psInst->asArg[1].uType = USEASM_REGTYPE_PREDICATE;
	if (!(uInst1 & EURASIA_USE1_VISTEST_ATST8_PDSTENABLE))
	{
		psInst->asArg[1].uFlags |= USEASM_ARGFLAGS_DISABLEWB;
	}

	/*
		Decode instruction sources.
	*/
	DecodeSrc0(psTarget, uInst0, uInst1, TRUE, EURASIA_USE1_VISTEST_ATST8_S0BEXT, &psInst->asArg[2], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	DecodeSrc12(psTarget, uInst0, uInst1, 1, TRUE, EURASIA_USE1_S1BEXT, &psInst->asArg[3], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	DecodeSrc12(psTarget, uInst0, uInst1, 2, TRUE, EURASIA_USE1_S2BEXT, &psInst->asArg[4], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);

	/*
		Decode two-sided flag.
	*/
	if (uInst1 & EURASIA_USE1_VISTEST_ATST8_TWOSIDED)
	{
		SetupIntSrcSel(&psInst->asArg[5], USEASM_INTSRCSEL_TWOSIDED);
	}
	else
	{
		SetupIntSrcSel(&psInst->asArg[5], USEASM_INTSRCSEL_NONE);
	}

	/*
		Decode feedback type.
	*/
	if (uInst1 & EURASIA_USE1_VISTEST_ATST8_OPTDWD)
	{
		SetupIntSrcSel(&psInst->asArg[6], USEASM_INTSRCSEL_OPTDWD);
	}
	else if (uInst1 & EURASIA_USE1_VISTEST_ATST8_DISABLEFEEDBACK)
	{
		SetupIntSrcSel(&psInst->asArg[6], USEASM_INTSRCSEL_NOFEEDBACK);
	}
	else
	{
		SetupIntSrcSel(&psInst->asArg[6], USEASM_INTSRCSEL_FEEDBACK);
	}

	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeDEPTHFInstruction(PCSGX_CORE_DESC psTarget,
											   IMG_UINT32     uInst0,
											   IMG_UINT32     uInst1,
											   PUSE_INST      psInst)
{
	IMG_UINT32	uSPred;

	/*
		Initialize the result to default values.
	*/
	UseAsmInitInst(psInst);

	/*
		No repeats on DEPTHF.
	*/
	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

	/*
		Decode instruction predicate.
	*/
	uSPred = (uInst1 & ~EURASIA_USE1_VISTEST_DEPTHF_SPRED_CLRMSK) >> EURASIA_USE1_VISTEST_DEPTHF_SPRED_SHIFT;
	psInst->uFlags1 |= (g_aeDecodeSPred[uSPred] << USEASM_OPFLAGS1_PRED_SHIFT);

	/*
		Set instruction opcode.
	*/
	psInst->uOpcode = USEASM_OP_DEPTHF;

	/*
		Decode instruction modifiers.
	*/
	if (uInst1 & EURASIA_USE1_END)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}
	if (uInst1 & EURASIA_USE1_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	if (uInst1 & EURASIA_USE1_VISTEST_ATST8_SYNCS)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SYNCSTART;
	}

	/*
		Decode instruction sources.
	*/
	DecodeSrc0(psTarget, uInst0, uInst1, TRUE, EURASIA_USE1_VISTEST_DEPTHF_S0BEXT, &psInst->asArg[0], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	DecodeSrc12(psTarget, uInst0, uInst1, 1, TRUE, EURASIA_USE1_S1BEXT, &psInst->asArg[1], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	DecodeSrc12(psTarget, uInst0, uInst1, 2, TRUE, EURASIA_USE1_S2BEXT, &psInst->asArg[2], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);

	/*
		Decode the two-sided flag.
	*/
	if (uInst1 & EURASIA_USE1_VISTEST_ATST8_TWOSIDED)
	{
		SetupIntSrcSel(&psInst->asArg[3], USEASM_INTSRCSEL_TWOSIDED);
	}
	else
	{
		SetupIntSrcSel(&psInst->asArg[3], USEASM_INTSRCSEL_NONE);
	}

	/*
		Decode the feedback type flag.
	*/
	if (uInst1 & EURASIA_USE1_VISTEST_ATST8_OPTDWD)
	{
		SetupIntSrcSel(&psInst->asArg[4], USEASM_INTSRCSEL_OPTDWD);
	}
	else if (uInst1 & EURASIA_USE1_VISTEST_ATST8_DISABLEFEEDBACK)
	{
		SetupIntSrcSel(&psInst->asArg[4], USEASM_INTSRCSEL_NOFEEDBACK);
	}
	else
	{
		SetupIntSrcSel(&psInst->asArg[4], USEASM_INTSRCSEL_FEEDBACK);
	}
	
	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodePTCTRLInstruction(PCSGX_CORE_DESC psTarget,
											   IMG_UINT32     uInst0,
											   IMG_UINT32     uInst1,
											   PUSE_INST	  psInst)
{
	IMG_UINT32	uType = (uInst1 & ~EURASIA_USE1_VISTEST_PTCTRL_TYPE_CLRMSK) >> EURASIA_USE1_VISTEST_PTCTRL_TYPE_SHIFT;

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*
		No repeats for PTOFF/PCOEFF
	*/
	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

	/*
		Decode instruction modifiers.
	*/
	if (uInst1 & EURASIA_USE1_END)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}
	if (IsEnhancedNoSched(psTarget) && (uInst1 & EURASIA_USE1_VISTEST_PTCTRL_NOSCHED))
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}

	if (uType == EURASIA_USE1_VISTEST_PTCTRL_TYPE_PLANE)
	{
		psInst->uOpcode = USEASM_OP_PCOEFF;

		DecodeSrc0(psTarget, uInst0, uInst1, TRUE, EURASIA_USE1_VISTEST_PTCTRL_S0BEXT, &psInst->asArg[0], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
		DecodeSrc12(psTarget, uInst0, uInst1, 1, TRUE, EURASIA_USE1_S1BEXT, &psInst->asArg[1], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
		DecodeSrc12(psTarget, uInst0, uInst1, 2, TRUE, EURASIA_USE1_S2BEXT, &psInst->asArg[2], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	}
	else
	{
		psInst->uOpcode = USEASM_OP_PTOFF;
	}
	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeLDRSTRInstruction(PCSGX_CORE_DESC psTarget,
											   IMG_UINT32     uInst0,
											   IMG_UINT32     uInst1,
											   PUSE_INST      psInst)
{
	IMG_UINT32 uS2Bank = (uInst0 & ~EURASIA_USE0_S2BANK_CLRMSK) >> EURASIA_USE0_S2BANK_SHIFT;
	PUSE_REGISTER psNextArg;

	UseAsmInitInst(psInst);

	#if defined(SUPPORT_SGX_FEATURE_USE_STR_PREDICATE)
	if ((uInst1 & EURASIA_USE1_LDRSTR_DSEL_STORE) && SupportsSTRPredicate(psTarget))
	{
		IMG_UINT32	uSPred;

		uSPred = (uInst1 & ~SGXVEC_USE1_LDRSTR_STR_SPRED_CLRMSK) >> SGXVEC_USE1_LDRSTR_STR_SPRED_SHIFT;
		psInst->uFlags1 |= (g_aeDecodeSPred[uSPred] << USEASM_OPFLAGS1_PRED_SHIFT);
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_STR_PREDICATE) */

	if (uInst1 & EURASIA_USE1_LDRSTR_DSEL_STORE)
	{
		psInst->uOpcode = USEASM_OP_STR;
	}
	else
	{
		psInst->uOpcode = USEASM_OP_LDR;
	}
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (IsEnhancedNoSched(psTarget) && (uInst1 & EURASIA_USE1_LDRSTR_NOSCHED))
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}

	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);
	#if defined(SUPPORT_SGX_FEATURE_USE_LDRSTR_REPEAT)
	if (SupportsLDRSTRRepeat(psTarget))
	{
		IMG_UINT32	uRptCount = (uInst1 & ~SGXVEC_USE1_LDRSTR_RCNT_CLRMSK) >> SGXVEC_USE1_LDRSTR_RCNT_SHIFT;

		psInst->uFlags1 |= ((uRptCount + 1) << USEASM_OPFLAGS1_REPEAT_SHIFT);
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_LDRSTR_REPEAT) */

	psNextArg = psInst->asArg;

	/*
		Decode the destination for LDR.
	*/
	if (!(uInst1 & EURASIA_USE1_LDRSTR_DSEL_STORE))
	{
		if (uInst1 & EURASIA_USE1_LDRSTR_DBANK_PRIMATTR)
		{
			psNextArg->uType = USEASM_REGTYPE_PRIMATTR;
		}
		else
		{
			psNextArg->uType = USEASM_REGTYPE_TEMP;
		}
		psNextArg->uNumber = (uInst0 & ~EURASIA_USE0_DST_CLRMSK) >> EURASIA_USE0_DST_SHIFT;
		psNextArg++;
	}

	/*
		Decode the global register address argument.
	*/
	if ((uInst1 & EURASIA_USE1_S2BEXT) && uS2Bank == EURASIA_USE0_S2EXTBANK_IMMEDIATE)
	{
		IMG_UINT32	uImmNum;

		uImmNum = ((uInst0 & ~EURASIA_USE0_SRC2_CLRMSK) >> EURASIA_USE0_SRC2_SHIFT) << 0;
		uImmNum |=
			(((uInst0 & ~EURASIA_USE0_LDRSTR_SRC2EXT_CLRMSK) >> EURASIA_USE0_LDRSTR_SRC2EXT_SHIFT) << EURASIA_USE0_LDRSTR_SRC2EXT_INTERNALSHIFT);
		#if defined(SUPPORT_SGX_FEATURE_USE_LDRSTR_EXTENDED_IMMEDIATE)
		if (SupportsLDRSTRExtendedImmediate(psTarget))
		{
			uImmNum |=
				(((uInst1 & ~SGXVEC_USE1_LDRSTR_SRC2EXT2_CLRMSK) >> SGXVEC_USE1_LDRSTR_SRC2EXT2_SHIFT) << SGXVEC_USE1_LDRSTR_SRC2EXT2_INTERNALSHIFT);
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_LDRSTR_EXTENDED_IMMEDIATE) */

		SetupImmediateSource(psNextArg, uImmNum);
	}
	else
	{
		DecodeSrc12(psTarget, uInst0, uInst1, 2, TRUE, EURASIA_USE1_S2BEXT, psNextArg, USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	}
	psNextArg++;

	if (uInst1 & EURASIA_USE1_LDRSTR_DSEL_STORE)
	{
		/*
			Decode the data to store.
		*/
		DecodeSrc12(psTarget, uInst0, uInst1, 1, TRUE, EURASIA_USE1_S1BEXT, psNextArg, USEDIS_FORMAT_CONTROL_STATE_OFF, 0);
	}
	else
	{
		IMG_UINT32	uDRCSel;

		/*
			Decode the DRC register argument.
		*/
		uDRCSel = (uInst1 & ~EURASIA_USE1_LDRSTR_DRCSEL_CLRMSK) >> EURASIA_USE1_LDRSTR_DRCSEL_SHIFT;
		SetupDRCSource(psNextArg, uDRCSel);
	}
	psNextArg++;

	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeWOPInstruction(PCSGX_CORE_DESC psTarget,
										    IMG_UINT32     uInst0,
										    IMG_UINT32     uInst1,
										    PUSE_INST      psInst)
{
	IMG_UINT32	uPTNB;

	UseAsmInitInst(psInst);

	psInst->uOpcode = USEASM_OP_WOP;

	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);
	
	if (IsEnhancedNoSched(psTarget) && (uInst1 & EURASIA_USE1_WOP_NOSCHED))
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	#if defined(SUPPORT_SGX_FEATURE_VCB)
	if (SupportsVCB(psTarget))
	{
		if (uInst0 & SGX545_USE0_WOP_TWOPART)
		{
			psInst->uFlags3 |= USEASM_OPFLAGS3_TWOPART;
		}
		uPTNB = (uInst0 & ~SGX545_USE0_WOP_PTNB_CLRMSK) >> SGX545_USE0_WOP_PTNB_SHIFT;
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_VCB) */
	{
		uPTNB = (uInst0 & ~EURASIA_USE0_WOP_PTNB_CLRMSK) >> EURASIA_USE0_WOP_PTNB_SHIFT;
	}
	SetupImmediateSource(&psInst->asArg[0], uPTNB);

	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeMutexInstruction(PCSGX_CORE_DESC	psTarget,
											  IMG_UINT32		uInst0,
											  IMG_UINT32		uInst1,
											  PUSE_INST			psInst)
{
	IMG_UINT32	uMutexNumber;

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*
		Set instruction opcode.
	*/
	if (uInst0 & EURASIA_USE0_LOCKRELEASE_ACTION_RELEASE)
	{
		psInst->uOpcode = USEASM_OP_RELEASE;
	}
	else
	{
		psInst->uOpcode = USEASM_OP_LOCK;
	}

	/*
		Decode instruction modifiers.
	*/
	if (IsEnhancedNoSched(psTarget) && (uInst1 & EURASIA_USE1_LOCKRELEASE_NOSCHED))
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}

	/*
		No repeats for lock/release
	*/
	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

	/*
		Decode mutex number.
	*/
	uMutexNumber = 
		(uInst0 & ~SGX545_USE0_LOCKRELEASE_MUTEXNUM_CLRMSK) >> SGX545_USE0_LOCKRELEASE_MUTEXNUM_SHIFT;
	if (uMutexNumber >= NumberOfMutexesSupported(psTarget))
	{
		return USEDISASM_ERROR_INVALID_MUTEX_NUMBER;
	}
	else
	{
		SetupImmediateSource(&psInst->asArg[0], uMutexNumber);
	}

	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeLIMMInstruction(PCSGX_CORE_DESC psTarget,
										     IMG_UINT32     uInst0,
										     IMG_UINT32     uInst1,
										     PUSE_INST      psInst)
{
	IMG_UINT32	uLimm;
	IMG_UINT32	uEPred;

	UseAsmInitInst(psInst);

	uEPred = (uInst1 & ~EURASIA_USE1_LIMM_EPRED_CLRMSK) >> EURASIA_USE1_LIMM_EPRED_SHIFT;
	psInst->uFlags1 |= (g_aeDecodeEPred[uEPred] << USEASM_OPFLAGS1_PRED_SHIFT);

	psInst->uOpcode = USEASM_OP_LIMM;

	if (uInst1 & EURASIA_USE1_END)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}
	if (uInst1 & EURASIA_USE1_SKIPINV)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	if (uInst1 & EURASIA_USE1_LIMM_NOSCHED)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}

	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

	DecodeDest(psTarget, uInst0, uInst1, TRUE, &psInst->asArg[0], USEDIS_FORMAT_CONTROL_STATE_OFF, 0);

	uLimm = (uInst0 & ~EURASIA_USE0_LIMM_IMML21_CLRMSK) >> EURASIA_USE0_LIMM_IMML21_SHIFT;
	uLimm |= ((uInst1 & ~EURASIA_USE1_LIMM_IMM2521_CLRMSK) >> EURASIA_USE1_LIMM_IMM2521_SHIFT) << 21;
	uLimm |= ((uInst1 & ~EURASIA_USE1_LIMM_IMM3126_CLRMSK) >> EURASIA_USE1_LIMM_IMM3126_SHIFT) << 26;
	SetupImmediateSource(&psInst->asArg[1], uLimm);

	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeEMITInstruction(PCSGX_CORE_DESC psTarget,
											 IMG_UINT32     uInst0,
										     IMG_UINT32     uInst1,
										     PUSE_INST      psInst)
{
	IMG_UINT32 uTarget;
	IMG_UINT32 uSideband;
	IMG_UINT32 uSrcsUsed = 7;
	IMG_BOOL bSidebandUsed = IMG_TRUE;
	IMG_UINT32 uIncp;
	PUSE_REGISTER psNextArg = psInst->asArg;
	IMG_BOOL bDoubleRegisters;
	IMG_UINT32 uNumFieldLength;

	/*
		Set default values into the input instruction.
	*/
	UseAsmInitInst(psInst);

	/*
		Unpack the emit sideband.
	*/
	uSideband = (((uInst1 & ~EURASIA_USE1_EMIT_SIDEBAND_109_108_CLRMSK) >> EURASIA_USE1_EMIT_SIDEBAND_109_108_SHIFT) << 12) |
				(((uInst1 & ~EURASIA_USE1_EMIT_SIDEBAND_107_102_CLRMSK) >> EURASIA_USE1_EMIT_SIDEBAND_107_102_SHIFT) << 6) |
				(((uInst0 & ~EURASIA_USE0_EMIT_SIDEBAND_101_96_CLRMSK) >> EURASIA_USE0_EMIT_SIDEBAND_101_96_SHIFT) << 0);

	uTarget = (uInst1 & ~EURASIA_USE1_EMIT_TARGET_CLRMSK) >> EURASIA_USE1_EMIT_TARGET_SHIFT;
	#if defined(SUPPORT_SGX_FEATURE_VCB)
	if (
			SupportsVCB(psTarget) &&
			(
				uTarget == EURASIA_USE1_EMIT_TARGET_MTE ||
				uTarget == SGX545_USE1_EMIT_TARGET_VCB
			)
	   )
	{
		IMG_UINT32	uEmitType;

		uEmitType = 
			(uInst1 & ~SGX545_USE1_EMIT_VCB_EMITTYPE_CLRMSK) >> SGX545_USE1_EMIT_VCB_EMITTYPE_SHIFT;
		if (uTarget == EURASIA_USE1_EMIT_TARGET_MTE)
		{
			switch (uEmitType)
			{
				case SGX545_USE1_EMIT_VCB_EMITTYPE_STATE: psInst->uOpcode = USEASM_OP_EMITMTESTATE; break;
				case SGX545_USE1_EMIT_VCB_EMITTYPE_VERTEX: psInst->uOpcode = USEASM_OP_EMITMTEVERTEX; break;
			}
		}
		else
		{
			switch (uEmitType)
			{
				case SGX545_USE1_EMIT_VCB_EMITTYPE_STATE: psInst->uOpcode = USEASM_OP_EMITVCBSTATE; break;
				case SGX545_USE1_EMIT_VCB_EMITTYPE_VERTEX: psInst->uOpcode = USEASM_OP_EMITVCBVERTEX; break;
			}
		}

		if (
				uTarget == EURASIA_USE1_EMIT_TARGET_MTE &&
				uEmitType == SGX545_USE1_EMIT_VCB_EMITTYPE_VERTEX &&
				(uInst1 & SGX545_USE1_EMIT_MTE_CUT)
		   )
		{
			psInst->uFlags3 |= USEASM_OPFLAGS3_CUT;
		}
		if (	
				uEmitType == SGX545_USE1_EMIT_VCB_EMITTYPE_VERTEX &&
				(uInst1 & SGX545_USE1_EMIT_VERTEX_TWOPART)
		   )
		{
			psInst->uFlags3 |= USEASM_OPFLAGS3_TWOPART;
		}
		if (	
				uEmitType == SGX545_USE1_EMIT_VCB_EMITTYPE_STATE &&
				(uInst1 & SGX545_USE1_EMIT_STATE_THREEPART)
		   )
		{
			psInst->uFlags3 |= USEASM_OPFLAGS3_THREEPART;
		}

		if (uTarget == EURASIA_USE1_EMIT_TARGET_MTE)
		{
			uSrcsUsed = 6;
		}
		else
		{
			uSrcsUsed = 4;
		}
		bSidebandUsed = IMG_FALSE;
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_VCB) */
	if (uTarget == EURASIA_USE1_EMIT_TARGET_MTE)
	{
		IMG_UINT32 uMTECtrl;
								
		uMTECtrl = (uInst1 & ~EURASIA_USE1_EMIT_MTECTRL_CLRMSK) >> EURASIA_USE1_EMIT_MTECTRL_SHIFT;
		switch (uMTECtrl)
		{
			case EURASIA_USE1_EMIT_MTECTRL_STATE: psInst->uOpcode = USEASM_OP_EMITSTATE; break;
			case EURASIA_USE1_EMIT_MTECTRL_VERTEX: psInst->uOpcode = USEASM_OP_EMITVERTEX; break;
			case EURASIA_USE1_EMIT_MTECTRL_PRIMITIVE: psInst->uOpcode = USEASM_OP_EMITPRIMITIVE; break;
			default: return USEDISASM_ERROR_INVALID_EMIT_MTE_CONTROL;
		}

		if (uMTECtrl == EURASIA_USE1_EMIT_MTECTRL_STATE)
		{
			uSrcsUsed = 2;
			bSidebandUsed = IMG_FALSE;
		}
		else if (uMTECtrl == EURASIA_USE1_EMIT_MTECTRL_VERTEX)
		{
			uSrcsUsed = 4;
			bSidebandUsed = IMG_FALSE;
		}
		else if (uMTECtrl == EURASIA_USE1_EMIT_MTECTRL_PRIMITIVE)
		{
			uSrcsUsed = 6;
		}
	}
	else if (uTarget == EURASIA_USE1_EMIT_TARGET_PDS)
	{
		psInst->uOpcode = USEASM_OP_EMITPDS;
	}
	else if (uTarget == EURASIA_USE1_EMIT_TARGET_PIXELBE)
	{
		IMG_UINT32	uTwoEmitsFlag;

		#if defined(SUPPORT_SGX545)
		if (psTarget->eCoreType == SGX_CORE_ID_545)
		{
			uTwoEmitsFlag = SGX545_PIXELBE2S2_TWOEMITS;
		}
		else
		#endif /* defined(SUPPORT_SGX545) */
		{
			uTwoEmitsFlag = EURASIA_PIXELBE1SB_TWOEMITS;
		}

		#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
		if (psTarget->eCoreType == SGX_CORE_ID_543)
		{
			psInst->uOpcode = USEASM_OP_EMITPIXEL;
		}
		else
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
		{
			psInst->uOpcode = (uSideband & uTwoEmitsFlag) ? USEASM_OP_EMITPIXEL1 : USEASM_OP_EMITPIXEL2;
		}
	}
	else
	{
		return USEDISASM_ERROR_INVALID_EMIT_TARGET;
	}

	/*
		Decode the instruction flags.
	*/
	if (uInst1 & EURASIA_USE1_END)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;
	}
	if (uInst0 & EURASIA_USE0_EMIT_FREEP)
	{
		psInst->uFlags3 |= USEASM_OPFLAGS3_FREEP;
	}
							
	#if defined(SUPPORT_SGX_FEATURE_USE_NO_INSTRUCTION_PAIRING)
	if (IsEnhancedNoSched(psTarget) && (uInst1 & EURASIA_USE1_EMIT_NOSCHED))
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_NO_INSTRUCTION_PAIRING) */
	if (uTarget == EURASIA_USE1_EMIT_TARGET_PDS)
	{
		if (uInst1 & EURASIA_USE1_EMIT_PDSCTRL_TASKS)
		{
			psInst->uFlags2 |= USEASM_OPFLAGS2_TASKSTART;
		}
		if (uInst1 & EURASIA_USE1_EMIT_PDSCTRL_TASKE)
		{
			psInst->uFlags2 |= USEASM_OPFLAGS2_TASKEND;
		}
	}

	/*
		Decode the partition increment.
	*/
	uIncp = (uInst1 & ~EURASIA_USE1_EMIT_INCP_CLRMSK) >> EURASIA_USE1_EMIT_INCP_SHIFT;
	#if defined(SUPPORT_SGX_FEATURE_VCB)
	if (SupportsVCB(psTarget))
	{
		IMG_UINT32	uIncpExt;

		uIncpExt = (uInst1 & ~SGX545_USE1_EMIT_INCPEXT_CLRMSK) >> SGX545_USE1_EMIT_INCPEXT_SHIFT;
		uIncp |= (uIncpExt << SGX545_USE1_EMIT_INCPEXT_INTERNALSHIFT);
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_VCB) */
	SetupImmediateSource(psNextArg, uIncp);
	psNextArg++;

	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	if (SupportsVEC34(psTarget))
	{
		bDoubleRegisters = IMG_TRUE;
		uNumFieldLength = SGXVEC_USE_EMIT_SRC_FIELD_LENGTH;
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
	{
		bDoubleRegisters = IMG_FALSE;
		uNumFieldLength = EURASIA_USE_REGISTER_NUMBER_BITS;
	}

	if (uSrcsUsed & 1)
	{
		IMG_UINT32	uSrc0Num;

		uSrc0Num = (uInst0 & ~EURASIA_USE0_SRC0_CLRMSK) >> EURASIA_USE0_SRC0_SHIFT;
		DecodeSrc0WithNum(psTarget, 
						  bDoubleRegisters, 
						  uInst1, 
						  TRUE, 
						  EURASIA_USE1_EMIT_S0BEXT, 
						  psNextArg, 
						  IMG_FALSE /* bFmtControl */, 
						  0 /* uAltFmtFlag */, 
						  uNumFieldLength, 
						  uSrc0Num);
		psNextArg++;
	}
	if (uSrcsUsed & 2)
	{
		IMG_UINT32	uSrc1Num;

		uSrc1Num = (uInst0 & ~EURASIA_USE0_SRC1_CLRMSK) >> EURASIA_USE0_SRC1_SHIFT;
		DecodeSrc12WithNum(psTarget, 
						   bDoubleRegisters,
						   uInst0, 
						   uInst1, 
						   1 /* uSrc */, 
						   IMG_TRUE /* bAllowExtended */, 
						   EURASIA_USE1_S1BEXT /* uExt */, 
						   psNextArg, 
						   IMG_FALSE /* bFmtControl */, 
						   0 /* uAltFmtFlag */,
						   uNumFieldLength,
						   uSrc1Num);
		psNextArg++;
	}
	if (uSrcsUsed & 4)
	{
		IMG_UINT32	uSrc2Num;

		uSrc2Num = (uInst0 & ~EURASIA_USE0_SRC2_CLRMSK) >> EURASIA_USE0_SRC2_SHIFT;
		DecodeSrc12WithNum(psTarget, 
						   bDoubleRegisters,
						   uInst0, 
						   uInst1, 
						   2 /* uSrc */, 
						   IMG_TRUE /* bAllowExtended */, 
						   EURASIA_USE1_S2BEXT, 
						   psNextArg, 
						   IMG_FALSE /* bFmtControl */, 
						   0 /* uAltFmtFlag */,
						   uNumFieldLength,
						   uSrc2Num);
		psNextArg++;
	}
	if (bSidebandUsed)
	{
		SetupImmediateSource(psNextArg, uSideband);
		psNextArg++;
	}

	return USEDISASM_OK;
}

static IMG_VOID DisassembleEMITInstruction(PUSE_INST	psInst,
										   IMG_PCHAR    pszInst)
{
	IMG_UINT32	uArg;
	IMG_BOOL	bUsesSideband;
	IMG_UINT32	uArgumentCount;

	/*
		Print instruction predicate.
	*/
	PrintPredicate(psInst, pszInst);

	/*
		Print instruction opcode.
	*/
	usedisasm_asprintf(pszInst, "%s", OpcodeName(psInst->uOpcode));

	/*
		Print instruction modifier flags.
	*/
	PrintInstructionFlags(psInst, pszInst);
	usedisasm_asprintf(pszInst, " ");

	/*
		Print INCP argument.
	*/
	assert(psInst->asArg[0].uType == USEASM_REGTYPE_IMMEDIATE);
	usedisasm_asprintf(pszInst, "#%d /* incp */", psInst->asArg[0].uNumber);

	bUsesSideband = IMG_FALSE;
	if (psInst->uOpcode == USEASM_OP_EMITPIXEL ||
		psInst->uOpcode == USEASM_OP_EMITPIXEL2 ||
		psInst->uOpcode == USEASM_OP_EMITPDS ||
		psInst->uOpcode == USEASM_OP_EMITPRIMITIVE)
	{
		bUsesSideband = IMG_TRUE;
	}

	/*
		Print instruction arguments.
	*/
	uArgumentCount = OpcodeArgumentCount(psInst->uOpcode);
	for (uArg = 1; uArg < uArgumentCount; uArg++)
	{
		PUSE_REGISTER	psArg = &psInst->asArg[uArg];

		usedisasm_asprintf(pszInst, ", ");

		if (bUsesSideband && uArg == (uArgumentCount - 1))
		{
			assert(psArg->uType == USEASM_REGTYPE_IMMEDIATE);
			usedisasm_asprintf(pszInst, "#0x%.8X", psArg->uNumber);
		}
		else
		{
			PrintSourceRegister(psInst, psArg, pszInst);
		}
	}
}

static USEDISASM_ERROR DecodeSETMInstruction(PCSGX_CORE_DESC psTarget,
											 IMG_UINT32     uInst0,
											 IMG_UINT32     uInst1,
											 PUSE_INST		psInst)
{
	if (NumberOfMonitorsSupported(psTarget) == 0)
	{
		return USEDISASM_ERROR_INVALID_OPCODE;
	}
	else
	{
		IMG_UINT32	uMonitor = 
				(uInst0 & ~SGX545_USE0_SETM_MONITORNUM_CLRMSK) >> SGX545_USE0_SETM_MONITORNUM_SHIFT;

		UseAsmInitInst(psInst);

		psInst->uOpcode = USEASM_OP_SETM;
	
		/*
			Decode instruction flags.
		*/
		if (IsEnhancedNoSched(psTarget) && (uInst1 & SGX545_USE1_SETM_NOSCHED))
		{
			psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
		}

		/*
			No repeats on SETM.
		*/
		psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

		/*
			Setup an immediate source representing the monitor number.
		*/
		if (uMonitor >= NumberOfMonitorsSupported(psTarget))
		{
			return USEDISASM_ERROR_INVALID_MONITOR_NUMBER;
		}
		else
		{
			SetupImmediateSource(&psInst->asArg[0], uMonitor);
		}
	}

	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeWDFInstruction(PCSGX_CORE_DESC psTarget,
											IMG_UINT32     uInst1,
											PUSE_INST	   psInst)
{
	IMG_UINT32	uDRCSel;

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*
		Set instruction opcode.
	*/
	psInst->uOpcode = USEASM_OP_WDF;

	/*
		No repeats on WDF.
	*/
	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

	/*
		Decode NOSCHED flag.
	*/
	if (IsEnhancedNoSched(psTarget) && (uInst1 & EURASIA_USE1_IDFWDF_NOSCHED))
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}

	/*
		Decode DRC register argument.
	*/
	uDRCSel = (uInst1 & ~EURASIA_USE1_IDFWDF_DRCSEL_CLRMSK) >> EURASIA_USE1_IDFWDF_DRCSEL_SHIFT;
	SetupDRCSource(&psInst->asArg[0], uDRCSel);

	return USEDISASM_OK;
}

static USEDISASM_ERROR DecodeIDFInstruction(PCSGX_CORE_DESC	psTarget,
											IMG_UINT32		uInst1,
											PUSE_INST		psInst)
{
	IMG_UINT32 uDrcSel = (uInst1 & ~EURASIA_USE1_IDFWDF_DRCSEL_CLRMSK) >> EURASIA_USE1_IDFWDF_DRCSEL_SHIFT;
	IMG_UINT32 uPath = (uInst1 & ~EURASIA_USE1_IDF_PATH_CLRMSK) >> EURASIA_USE1_IDF_PATH_SHIFT;

	/*
		Initialize the instruction.
	*/
	UseAsmInitInst(psInst);

	/*
		Set instruction opcode.
	*/
	psInst->uOpcode = USEASM_OP_IDF;

	/*
		No repeats on IDF.
	*/
	psInst->uFlags1 |= (USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT);

	/*
		Decode NOSCHED flag.
	*/
	if (IsEnhancedNoSched(psTarget) && (uInst1 & EURASIA_USE1_IDFWDF_NOSCHED))
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}

	#if defined(SUPPORT_SGX_FEATURE_USE_IDF_SUPPORTS_SKIPINVALID)
	/*
		Decode SKIPINVALID flag.
	*/
	if (SupportSkipInvalidOnIDFInstruction(psTarget) && (uInst1 & EURASIA_USE1_SKIPINV) != 0)
	{
		psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_IDF_SUPPORTS_SKIPINVALID) */

	/*
		Set DRC source.
	*/
	SetupDRCSource(&psInst->asArg[0], uDrcSel);

	/*
		Set source representing the path for the fence.
	*/
	SetupImmediateSource(&psInst->asArg[1], uPath);

	return USEDISASM_OK;
}

static IMG_VOID DisassembleIDFInstruction(PUSE_INST		psInst,
										  IMG_PCHAR		pszInst)
{
	static const IMG_PCHAR pszPath[] = {"bif",			/* EURASIA_USE1_IDF_PATH_ST */
										"pixelbe",		/* EURASIA_USE1_IDF_PATH_PIXELBE */
										"reserved(=2)",	/* EURASIA_USE1_IDF_PATH_RESERVED0 */
										"reserved(=3)"	/* EURASIA_USE1_IDF_PATH_RESERVED1 */};

	/*
		Print the opcode and modifier flags.
	*/
	usedisasm_asprintf(pszInst, "%s", OpcodeName(psInst->uOpcode));
	PrintInstructionFlags(psInst, pszInst);
	usedisasm_asprintf(pszInst, " ");

	/*
		Print the first argument.
	*/
	PrintSourceRegister(psInst, &psInst->asArg[0], pszInst);
	usedisasm_asprintf(pszInst, ", ");

	/*
		Decode immediate values in the second argument to symbolic names.
	*/
	assert(psInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE);
	assert(psInst->asArg[1].uNumber < (sizeof(pszPath) / sizeof(pszPath[0])));
	usedisasm_asprintf(pszInst, "%s", pszPath[psInst->asArg[1].uNumber]);
}

static
USEDISASM_ERROR DecodeSpecialOtherInstruction(PCSGX_CORE_DESC	psTarget,
											  IMG_UINT32		uInst0,
											  IMG_UINT32		uInst1,
											  PUSE_INST			psInst)
{
	IMG_UINT32 uOp2 = (uInst1 & ~EURASIA_USE1_OTHER_OP2_CLRMSK) >> EURASIA_USE1_OTHER_OP2_SHIFT;
	switch (uOp2)
	{
		case EURASIA_USE1_OTHER_OP2_IDF:
		{
			return DecodeIDFInstruction(psTarget, uInst1, psInst);
		}
		case EURASIA_USE1_OTHER_OP2_WDF:
		{
			return DecodeWDFInstruction(psTarget, uInst1, psInst);
		}
		case SGX545_USE1_OTHER_OP2_SETM:
		{
			return DecodeSETMInstruction(psTarget, uInst0, uInst1, psInst);
		}
		case EURASIA_USE1_OTHER_OP2_EMIT:
		{
			return DecodeEMITInstruction(psTarget, uInst0, uInst1, psInst);
		}
		case EURASIA_USE1_OTHER_OP2_LIMM:
		{
			return DecodeLIMMInstruction(psTarget, uInst0, uInst1, psInst);
		}
		case EURASIA_USE1_OTHER_OP2_LDRSTR:
		{
			return DecodeLDRSTRInstruction(psTarget, uInst0, uInst1, psInst);
		}
		case EURASIA_USE1_OTHER_OP2_WOP:
		{
			return DecodeWOPInstruction(psTarget, uInst0, uInst1, psInst);
		}
		case EURASIA_USE1_OTHER_OP2_LOCKRELEASE:
		{
			return DecodeMutexInstruction(psTarget, uInst0, uInst1, psInst);
		}
		default:
		{
			return USEDISASM_ERROR_INVALID_OTHER_OP2;
		}
	}
}

static
USEDISASM_ERROR DecodeVistestInstruction(PCSGX_CORE_DESC	psTarget,
										 IMG_UINT32		uInst0,
										 IMG_UINT32		uInst1,
										 PUSE_INST		psInst)
{
	IMG_UINT32 uOp2 = (uInst1 & ~EURASIA_USE1_OTHER_OP2_CLRMSK) >> EURASIA_USE1_OTHER_OP2_SHIFT;
	switch (uOp2)
	{
		case EURASIA_USE1_VISTEST_OP2_PTCTRL:
		{
			return DecodePTCTRLInstruction(psTarget, uInst0, uInst1, psInst);
		}
		case EURASIA_USE1_VISTEST_OP2_ATST8:
		{
			return DecodeATST8Instruction(psTarget, uInst0, uInst1, psInst);
		}
		case EURASIA_USE1_VISTEST_OP2_DEPTHF:
		{
			return DecodeDEPTHFInstruction(psTarget, uInst0, uInst1, psInst);
		}
		default:
		{
			return USEDISASM_ERROR_INVALID_VISTEST_OP2;
		}
	}
}

static
USEDISASM_ERROR DecodeSpecialInstruction(PCSGX_CORE_DESC		psTarget,
										 IMG_UINT32			uInst0,
										 IMG_UINT32			uInst1,
										 PUSE_INST			psInst)
{
	IMG_UINT32 uOp = (uInst1 & ~EURASIA_USE1_SPECIAL_OPCAT_CLRMSK) >> EURASIA_USE1_SPECIAL_OPCAT_SHIFT;

	switch (uOp)
	{
		case EURASIA_USE1_SPECIAL_OPCAT_FLOWCTRL:
		{
			if (uInst1 & EURASIA_USE1_SPECIAL_OPCAT_EXTRA)
			{
				return DecodeOther2(psTarget, uInst0, uInst1, psInst);
			}
			else
			{
				return DecodeFlowControl(psTarget, uInst0, uInst1, psInst);
			}
		}
		case EURASIA_USE1_SPECIAL_OPCAT_MOECTRL:
		{
			#if defined(SUPPORT_SGX_FEATURE_USE_STORE_MOE_TO_REGISTERS)
			IMG_UINT32	uOp2 = (uInst1 & ~EURASIA_USE1_MOECTRL_OP2_CLRMSK) >> EURASIA_USE1_MOECTRL_OP2_SHIFT;
			#endif /* defined(SUPPORT_SGX_FEATURE_USE_STORE_MOE_TO_REGISTERS) */

			#if defined(SUPPORT_SGX_FEATURE_USE_INTEGER_CONDITIONALS)
			if (
					(uInst1 & EURASIA_USE1_SPECIAL_OPCAT_EXTRA) && 
					SupportsIntegerConditionalInstructions(psTarget) &&
					!FixBRN29643(psTarget)
			   )
			{
				return DecodeIntegerConditional(psTarget, uInst0, uInst1, psInst);
			}
			else
			#endif /* defined(SUPPORT_SGX_FEATURE_USE_INTEGER_CONDITIONALS) */
			#if defined(SUPPORT_SGX_FEATURE_USE_STORE_MOE_TO_REGISTERS)
			if (
					SupportsMOEStore(psTarget) &&
					uOp2 == SGX545_USE1_MOECTRL_OP2_MOEST
			   )
			{
				return DecodeMOEST(psTarget, uInst0, uInst1, psInst);
			}
			else
			#endif /* defined(SUPPORT_SGX_FEATURE_USE_STORE_MOE_TO_REGISTERS) */
			{
				return DecodeMoeControlInstruction(psTarget, uInst0, uInst1, psInst);
			}
		}
		case EURASIA_USE1_SPECIAL_OPCAT_OTHER:
		{
			return DecodeSpecialOtherInstruction(psTarget, uInst0, uInst1, psInst);
		}
		case EURASIA_USE1_SPECIAL_OPCAT_VISTEST:
		{
			return DecodeVistestInstruction(psTarget, uInst0, uInst1, psInst);
		}
	}
	return USEDISASM_ERROR_INVALID_SPECIAL_OPCAT;
}

IMG_INTERNAL 
USEDISASM_ERROR IMG_CALLCONV UseDecodeInstruction(PCSGX_CORE_DESC			psTarget,
												  IMG_UINT32				uInst0,
												  IMG_UINT32				uInst1,
												  PCUSEDIS_RUNTIME_STATE	psRuntimeState,
												  PUSE_INST					psInst)
{
	IMG_UINT32 uOpcode = (uInst1 & ~EURASIA_USE1_OP_CLRMSK) >> EURASIA_USE1_OP_SHIFT;

	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	if (SupportsVEC34(psTarget))
	{
		switch (uOpcode)
		{
			case SGXVEC_USE1_OP_VECMAD:
			{
				return DecodeVEC4MADInstruction(psTarget, uInst0, uInst1, psInst);
			}
			case SGXVEC_USE1_OP_VECNONMADF32:
			{
				return DecodeVEC4NONMADInstruction(psTarget, uInst0, uInst1, psInst, IMG_FALSE /* bF16 */);
			}
			case SGXVEC_USE1_OP_VECNONMADF16:
			{
				return DecodeVEC4NONMADInstruction(psTarget, uInst0, uInst1, psInst, IMG_TRUE /* bF16 */);
			}
			case SGXVEC_USE1_OP_VECCOMPLEX:
			{
				return DecodeVecComplexInstruction(psTarget, uInst0, uInst1, psInst);
			}
			case SGXVEC_USE1_OP_VECMOV:
			{
				return DecodeVecMovInstruction(psTarget, uInst0, uInst1, psInst);
			}
			case EURASIA_USE1_OP_PCKUNPCK:
			{
				return DecodeVecPckInstruction(psTarget, uInst0, uInst1, psInst);
			}
			case SGXVEC_USE1_OP_DVEC3:
			{
				return DecodeVec34DualIssueInstruction(psTarget, 
													   IMG_FALSE /* bVec4 */, 
													   uInst0, 
													   uInst1, 
													   psInst, 
													   psInst->psNext);
			}
			case SGXVEC_USE1_OP_DVEC4:
			{
				return DecodeVec34DualIssueInstruction(psTarget, 
													   IMG_TRUE /* bVec4 */, 
													   uInst0, 
													   uInst1, 
													   psInst, 
													   psInst->psNext);
			}
			case SGXVEC_USE1_OP_SVEC:
			{
				return DecodeVec34SingleIssueInstruction(psTarget, uInst0, uInst1, psInst);
			}
		}
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

	switch (uOpcode)
	{
		case EURASIA_USE1_OP_FARITH:
		case EURASIA_USE1_OP_FDOTPRODUCT:
		case EURASIA_USE1_OP_FMINMAX:
		case EURASIA_USE1_OP_FGRADIENT:
		case EURASIA_USE1_OP_FARITH16:
		{
			return DecodeFloatInstruction(psTarget, uInst0, uInst1, uOpcode, psInst);
		}

		case EURASIA_USE1_OP_FSCALAR:
		{
			return DecodeComplexOpInstruction(psTarget, uInst0, uInst1, psInst);
		}

		case EURASIA_USE1_OP_MOVC:
		{
			return DecodeMovcInstruction(psTarget, uInst0, uInst1, psInst);
		}

		case EURASIA_USE1_OP_EFO:
		{
			return DecodeEfoInstruction(psTarget, uInst0, uInst1, psInst, psRuntimeState->eEFOFormatControl);
		}

		#if defined(SUPPORT_SGX_FEATURE_USE_DUAL_ISSUE)
		case SGX545_USE1_OP_FDUAL:
		#else
		#if defined(SUPPORT_SGX_FEATURE_USE_FNORMALISE)
		case SGX540_USE1_OP_FNRM:
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_FNORMALISE) */
        #endif /* defined(SUPPORT_SGX_FEATURE_USE_DUAL_ISSUE) */
		#if defined(SUPPORT_SGX_FEATURE_USE_FNORMALISE) || defined(SUPPORT_SGX_FEATURE_USE_DUAL_ISSUE)
		{
			#if defined(SUPPORT_SGX_FEATURE_USE_DUAL_ISSUE)
			if (HasDualIssue(psTarget))
			{
				return DecodeDualIssue(psTarget, uInst0, uInst1, psInst, psInst->psNext);	
			} 
			#endif /* defined(SUPPORT_SGX_FEATURE_USE_DUAL_ISSUE) */
			#if defined(SUPPORT_SGX_FEATURE_USE_FNORMALISE)
			if (SupportsFnormalise(psTarget))
			{
				return DecodeFNormalise(psTarget, uInst0, uInst1, psInst);	
			}
			#endif /* defined(SUPPORT_SGX_FEATURE_USE_FNORMALISE) */ 
			return USEDISASM_ERROR_INVALID_OPCODE;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_FNORMALISE) || defined(SUPPORT_SGX_FEATURE_USE_DUAL_ISSUE) */

		case EURASIA_USE1_OP_PCKUNPCK:
		{
			return DecodePCKInstruction(psTarget, uInst0, uInst1, psInst);
		}

		case EURASIA_USE1_OP_TEST:
		case EURASIA_USE1_OP_TESTMASK:
		{
			return DecodeTestInstruction(psTarget, uInst0, uInst1, uOpcode, psInst);
		}

		case EURASIA_USE1_OP_ANDOR:
		case EURASIA_USE1_OP_XOR:
		case EURASIA_USE1_OP_SHLROL:
		case EURASIA_USE1_OP_SHRASR:
		{
			return DecodeBitwiseInstruction(psTarget, uInst0, uInst1, uOpcode, psInst);
		}

		case EURASIA_USE1_OP_RLP:
		{
			return DecodeRLPInstruction(psTarget, uInst0, uInst1, psInst);
		}
		case EURASIA_USE1_OP_SOP2:
		{
			return DecodeSOP2Instruction(psTarget, uInst0, uInst1, psRuntimeState, psInst, psInst->psNext);
		}
		case EURASIA_USE1_OP_SOPWM:
		{
			return DecodeSOPWMInstruction(psTarget, uInst0, uInst1, psRuntimeState, psInst);
		}
		case EURASIA_USE1_OP_SOP3:
		{	
			return DecodeSOP3Instruction(psTarget, uInst0, uInst1, psRuntimeState, psInst, psInst->psNext);
		}
		case EURASIA_USE1_OP_IMA8:
		case EURASIA_USE1_OP_FPMA:
		{
			return DecodeIMA8Instruction(psTarget, uInst0, uInst1, psRuntimeState, uOpcode, psInst);
		}

		case EURASIA_USE1_OP_DOT3DOT4:
		{
			return DecodeDOT34Instruction(psTarget, uInst0, uInst1, psRuntimeState, psInst, psInst->psNext);
		}

		case EURASIA_USE1_OP_IMAE:
		{
			return DecodeIMAEInstruction(psTarget, uInst0, uInst1, psInst);
		}

		case EURASIA_USE1_OP_IMA16:
		{
			return DecodeIMA16Instruction(psTarget, uInst0, uInst1, psInst);
		}

		case EURASIA_USE1_OP_ADIFFIRVBILIN:
		{
			return DecodeADIFFIRVBILINInstruction(psTarget, uInst0, uInst1, psInst);
		}

		case EURASIA_USE1_OP_FIRH:
		{
			return DecodeFIRHInstruction(psTarget, uInst0, uInst1, psInst);
		}

		#if defined(SUPPORT_SGX_FEATURE_USE_32BIT_INT_MAD) || defined(SUPPORT_SGX_FEATURE_USE_INT_DIV)
		case EURASIA_USE1_OP_32BITINT:
		{
			return Decode32BitIntegerInstruction(psTarget, uInst0, uInst1, psInst);
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_32BIT_INT_MAD) || defined(SUPPORT_SGX_FEATURE_USE_INT_DIV) */

		case EURASIA_USE1_OP_SMP:
		{
			return DecodeSmp(psTarget, uInst0, uInst1, psInst);
		}

		case EURASIA_USE1_OP_LD:
		case EURASIA_USE1_OP_ST:
		{
			return DecodeLdSt(psTarget, uInst0, uInst1, psInst);
		}

		case EURASIA_USE1_OP_SPECIAL:
		{
			return DecodeSpecialInstruction(psTarget, uInst0, uInst1, psInst);
		}
		default:
		{
			return USEDISASM_ERROR_INVALID_OPCODE;
		}
	}
}

static const IMG_PCHAR g_apszErrorMessage[] =
{
	"no_error",								/* USEDISASM_OK */
	"invalid_instruction",					/* USEDISASM_ERROR_INVALID_INSTRUCTION */
	"invalid_idxscw_destination_bank",		/* USEDISASM_ERROR_INVALID_IDXSCW_DESTINATION_BANK */
	"invalid_dsxscr_source_bank",			/* USEDISASM_ERROR_INVALID_IDXSCR_SOURCE_BANK */
	"invalid_ldst_data_size",				/* USEDISASM_ERROR_INVALID_LDST_DATA_SIZE */
	"invalid_flow_control_op2",				/* USEDISASM_ERROR_INVALID_FLOWCONTROL_OP2 */
	"invalid_phas_rate",					/* USEDISASM_ERROR_INVALID_PHAS_RATE */
	"invalid_phas_wait_condition",			/* USEDISASM_ERROR_INVALID_PHAS_WAIT_COND */
	"invalid_cfi_mode",						/* USEDISASM_ERROR_INVALID_CFI_MODE */
	"invalid_other_op2",					/* USEDISASM_ERROR_INVALID_OTHER2_OP2 */
	"invalid_fnrm_swizzle",					/* USEDISASM_ERROR_INVALID_FNRM_SWIZZLE */
	"invalid_moe_op2",						/* USEDISASM_ERROR_INVALID_MOE_OP2 */
	"invalid_fdual_mov_src",				/* USEDISASM_ERROR_INVALID_FDUAL_MOV_SRC */
	"invalid_complex_op2",					/* USEDISASM_ERROR_INVALID_COMPLEX_OP2 */
	"invalid_complex_data_type",			/* USEDISASM_ERROR_INVALID_COMPLEX_DATA_TYPE */
	"invalid_float_op2",					/* USEDISASM_ERROR_INVALID_FLOAT_OP2 */
	"invalid_movc_data_type",				/* USEDISASM_ERROR_INVALID_MOVC_DATA_TYPE */
	"invalid_dvec_op1",						/* USEDISASM_ERROR_INVALID_DVEC_OP1 */
	"invalid_dvec_op2",						/* USEDISASM_ERROR_INVALID_DVEC_OP2 */
	"invalid_dvec_srccfg",					/* USEDISASM_ERROR_INVALID_DVEC_SRCCFG */
	"invalid_veccomplex_dest_data_type",	/* USEDISASM_ERROR_INVALID_VECCOMPLEX_DEST_DATA_TYPE */
	"invalid_veccomplex_source_data_type",	/* USEDISASM_ERROR_INVALID_VECCOMPLEX_SRC_DATA_TYPE */
	"invalid_vecmov_type",					/* USEDISASM_ERROR_INVALID_VECMOV_TYPE */
	"invalid_vecmov_data_type",				/* USEDISASM_ERROR_INVALID_VECMOV_DATA_TYPE */
	"invalid_vpck_format_combination",		/* USEDISASM_ERROR_INVALID_VPCK_FORMAT_COMBINATION */
	"invalid_smp_lod_mode",					/* USEDISASM_ERROR_INVALID_SMP_LOD_MODE */
	"invalid_smp_srd_mode",					/* USEDISASM_ERROR_INVALID_SMP_SRD_MODE */
	"invalid_smp_pcf_mode",					/* USEDISASM_ERROR_INVALID_SMP_PCF_MODE */
	"invalid_smp_data_type",				/* USEDISASM_ERROR_INVALID_SMP_DATA_TYPE */
	"invalid_smp_format_conversion",		/* USEDISASM_ERROR_INVALID_SMP_FORMAT_CONVERSION */
	"invalid_firh_source_format",			/* USEDISASM_ERROR_INVALID_FIRH_SOURCE_FORMAT */
	"invalid_pck_format_combination",		/* USEDISASM_ERROR_INVALID_PCK_FORMAT_COMBINATION */
	"invalid_bitwise_op2",					/* USEDISASM_ERROR_INVALID_BITWISE_OP2 */
	"invalid_test_aluop",					/* USEDISASM_ERROR_INVALID_TEST_ALUOP */
	"invalid_test_sign_test",				/* USEDISASM_ERROR_INVALID_TEST_SIGN_TEST */
	"invalid_test_zero_test",				/* USEDISASM_ERROR_INVALID_TEST_ZERO_TEST */
	"invalid_test_mask_type",				/* USEDISASM_ERROR_INVALID_TEST_MASK_TYPE */
	"invalid_sop2_csel1",					/* USEDISASM_ERROR_INVALID_SOP2_CSEL1 */
	"invalid_sop2_csel2",					/* USEDISASM_ERROR_INVALID_SOP2_CSEL2 */
	"invalid_sopwm_sel1",					/* USEDISASM_ERROR_INVALID_SOPWM_SEL1 */
	"invalid_sopwm_sel2",					/* USEDISASM_ERROR_INVALID_SOPWM_SEL2 */
	"invalid_imae_src2_type",				/* USEDISASM_ERROR_INVALID_IMAE_SRC2_TYPE */
	"invalid_firv_flag",					/* USEDISASM_ERROR_INVALID_FIRV_FLAG */
	"invalid_ssum16_round_mode",			/* USEDISASM_ERROR_INVALID_SSUM16_ROUND_MODE */
	"invalid_adiffirvbilin_opsel",			/* USEDISASM_ERROR_INVALID_ADIFFIRVBILIN_OPSEL */
	"invalid_bilin_source_format",			/* USEDISASM_ERROR_INVALID_BILIN_SOURCE_FORMAT */
	"invalid_opcode",						/* USEDISASM_ERROR_INVALID_OPCODE */
	"invalid_ima32_step",					/* USEDISASM_ERROR_INVALID_IMA32_STEP */
	"invalid_mutex_number",					/* USEDISASM_ERROR_INVALID_MUTEX_NUMBER */
	"invalid_emit_mte_control",				/* USEDISASM_ERROR_INVALID_EMIT_MTE_CONTROL */
	"invalid_monitor_number",				/* USEDISASM_ERROR_INVALID_MONITOR_NUMBER */
	"invalid_other_op2",					/* USEDISASM_ERROR_INVALID_OTHER_OP2 */
	"invalid_vistest_op2",					/* USEDISASM_ERROR_INVALID_VISTEST_OP2 */
	"invalid_special_opcat",				/* USEDISASM_ERROR_INVALID_SPECIAL_OPCAT */
	"invalid_emit_target",					/* USEDISASM_ERROR_INVALID_EMIT_TARGET */
	"invalid_cnd_op2",						/* USEDISASM_ERROR_INVALID_CND_OP2 */
	"invalid_cnd_pcnd",						/* USEDISASM_ERROR_INVALID_CND_INVALID_PCND */
	"invalid_ldst_atomic_op",				/* USEDISASM_ERROR_INVALID_LDST_ATOMIC_OP */
};

IMG_INTERNAL IMG_BOOL IMG_CALLCONV UseDisassembleInstruction(PCSGX_CORE_DESC psTarget,
															 IMG_UINT32     uInst0,
															 IMG_UINT32     uInst1,
															 IMG_PCHAR      pszInst)
{
	USE_INST				sInst;
	USE_INST				sCoInst;
	USEDISASM_ERROR			eRet;
	USEDIS_RUNTIME_STATE	sRuntimeState;

	*pszInst = '\0';

	/*
		Decode the instruction back to the input format.
	*/
	sInst.psNext = &sCoInst;
	sRuntimeState.eColourFormatControl = USEDIS_FORMAT_CONTROL_STATE_ON;
	sRuntimeState.eEFOFormatControl = USEDIS_FORMAT_CONTROL_STATE_OFF;
	eRet = UseDecodeInstruction(psTarget, uInst0, uInst1, &sRuntimeState, &sInst);
	if (eRet != USEDISASM_OK)
	{
		if (eRet < (sizeof(g_apszErrorMessage) / sizeof(g_apszErrorMessage[0])))
		{
			usedisasm_asprintf(pszInst, "%s", g_apszErrorMessage[eRet]);
		}
		else
		{
			usedisasm_asprintf(pszInst, "unknown_error");
		}
		return IMG_FALSE;
	}

	switch (sInst.uOpcode)
	{
		case USEASM_OP_EFO:
		{
			DisassembleEfoInstruction(psTarget, uInst0, uInst1, pszInst);
			break;
		}
		case USEASM_OP_LDAB:
		case USEASM_OP_LDAW:
		case USEASM_OP_LDAD:
		case USEASM_OP_STAB:
		case USEASM_OP_STAW:
		case USEASM_OP_STAD:
		case USEASM_OP_LDTB:
		case USEASM_OP_LDTW:
		case USEASM_OP_LDTD:
		case USEASM_OP_STTB:
		case USEASM_OP_STTW:
		case USEASM_OP_STTD:
		case USEASM_OP_LDLB:
		case USEASM_OP_LDLW:
		case USEASM_OP_LDLD:
		case USEASM_OP_STLB:
		case USEASM_OP_STLW:
		case USEASM_OP_STLD:
		#if defined(SUPPORT_SGX_FEATURE_USE_LDST_QWORD)
		case USEASM_OP_LDAQ:
		case USEASM_OP_STAQ:
		case USEASM_OP_LDTQ:
		case USEASM_OP_STTQ:
		case USEASM_OP_LDLQ:
		case USEASM_OP_STLQ:
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_LDST_QWORD) */
		#if defined(SUPPORT_SGX_FEATURE_USE_EXTENDED_LOAD)
		case USEASM_OP_ELDD:
		case USEASM_OP_ELDQ:
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_EXTENDED_LOAD) */
		#if defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS)
		case USEASM_OP_LDATOMIC:
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS) */
		{
			DisassembleLdSt(&sInst, pszInst);
			break;
		}

		#if defined(SUPPORT_SGX_FEATURE_USE_CFI)
		case USEASM_OP_CF:
		case USEASM_OP_CI:
		case USEASM_OP_CFI:
		{
			DisassembleCFI(&sInst, pszInst);
			break;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_CFI) */

		case USEASM_OP_IDF:
		{
			DisassembleIDFInstruction(&sInst, pszInst);
			break;
		}

		#if defined(SUPPORT_SGX_FEATURE_USE_STORE_MOE_TO_REGISTERS)
		case USEASM_OP_MOEST:
		{
			DisassembleMOEST(&sInst, pszInst);
			break;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_STORE_MOE_TO_REGISTERS) */

		case USEASM_OP_EMITVERTEX:
		case USEASM_OP_EMITSTATE:
		case USEASM_OP_EMITPRIMITIVE:
		case USEASM_OP_EMITPIXEL1:
		case USEASM_OP_EMITPIXEL2:
		case USEASM_OP_EMITPDS:
		#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
		case USEASM_OP_EMITPIXEL:
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
		#if defined(SUPPORT_SGX_FEATURE_VCB)
		case USEASM_OP_EMITVCBVERTEX:
		case USEASM_OP_EMITVCBSTATE:
		case USEASM_OP_EMITMTEVERTEX:
		case USEASM_OP_EMITMTESTATE:
		#endif /* defined(SUPPORT_SGX_FEATURE_VCB) */
		{
			DisassembleEMITInstruction(&sInst, pszInst);
			break;
		}

		case USEASM_OP_LIMM:
		{
			sInst.uOpcode = USEASM_OP_MOV;
			PrintStandardInstruction(psTarget, &sInst, pszInst);
			break;
		}

		default:
		{
			PrintStandardInstruction(psTarget, &sInst, pszInst);
			if (sInst.uFlags1 & USEASM_OPFLAGS1_MAINISSUE)
			{
				usedisasm_asprintf(pszInst, "+");
				PrintStandardInstruction(psTarget, &sCoInst, pszInst);
			}
			break;
		}
	}
	
	return (sInst.uFlags1 & USEASM_OPFLAGS1_END) ? IMG_TRUE : IMG_FALSE;
}

IMG_INTERNAL IMG_VOID IMG_CALLCONV UseDisassembler(PCSGX_CORE_DESC psTarget,
												   IMG_UINT32 uInstCount,
												   IMG_PUINT32 puInstructions)
{
	IMG_UINT32 i;
	IMG_UINT32 uInstCountDigits;

	for (i = 10, uInstCountDigits = 1; i < uInstCount; i *= 10, uInstCountDigits++);

	for (i = 0; i < uInstCount; i++, puInstructions+=2)
	{
		IMG_UINT32 uInst0 = puInstructions[0];
		IMG_UINT32 uInst1 = puInstructions[1];
		IMG_CHAR pszInst[256];

		printf("%*d: 0x%.8X%.8X\t", uInstCountDigits, i, uInst1, uInst0);
		UseDisassembleInstruction(psTarget, uInst0, uInst1, pszInst);
		printf("%s\n", pszInst);
	}
}

/******************************************************************************
 End of file (usedisasm.c)
******************************************************************************/

