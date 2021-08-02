/******************************************************************************
 * Name         : usc_utils.c
 * Title        : Eurasia USE Compiler
 * Created      : Sep 2007
 *
 * Copyright    : 2002-2007 by Imagination Technologies Limited.
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
 * $Log: usc_utils.c $
 *****************************************************************************/
#include "stdio.h"
#include "string.h"
#include "usc.h"
#include "uscshrd.h"
#include "usc_texstate.h"
#include "bitops.h"
#include <limits.h>

IMG_INTERNAL
const INPUT_INST_DESC g_asInputInstDesc[UFOP_MAXIMUM] =
{
	/* Arithmetic instructions. */
	/* UFOP_ADD */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"ADD",							/* pchName */
	},
	/* UFOP_SUB */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"SUB",							/* pchName */
	},
	/* UFOP_MUL */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"MUL",							/* pchName */
	},
	/* UFOP_MAD */
	{
		1,								/* uNumDests */
		3,								/* uNumSrcArgs */
		"MAD",							/* pchName */
	},
	/* UFOP_DOT2ADD */
	{
		1,								/* uNumDests */
		3,								/* uNumSrcArgs */
		"DOT2ADD",						/* pchName */
	},
	/* UFOP_DOT3 */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"DOT3",							/* pchName */
	},
	/* UFOP_DOT4 */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"DOT4",							/* pchName */
	},
	/* UFOP_RECIP */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"RECIP",						/* pchName */
	},
	/* UFOP_RSQRT */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"RSQRT",						/* pchName */
	},
	/* UFOP_EXP */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"EXP",							/* pchName */
	},
	/* UFOP_LOG */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"LOG",							/* pchName */
	},
	/* UFOP_CMP */
	{
		1,								/* uNumDests */
		3,								/* uNumSrcArgs */
		"CMP",							/* pchName */
	},
	/* UFOP_CND */
	{
		1,								/* uNumDests */
		3,								/* uNumSrcArgs */
		"CND",							/* pchName */
	},
	/* UFOP_FRC */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"FRC",							/* pchName */
	},
	/* UFOP_FLR */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"FLR",							/* pchName */
	},
	/* UFOP_TRC */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"TRC",							/* pchName */
	},
	/* UFOP_RNE */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"RNE",							/* pchName */
	},
	/* UFOP_RND */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"RND",							/* pchName */
	},
	/* UFOP_CEIL */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"CEIL",							/* pchName */
	},
	/* UFOP_LRP */
	{
		1,								/* uNumDests */
		3,								/* uNumSrcArgs */
		"LRP",							/* pchName */
	},
	/* UFOP_POW */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"POW",							/* pchName */
	},
	/* UFOP_MIN */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"MIN",							/* pchName */
	},
	/* UFOP_MAX */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"MAX",							/* pchName */
	},
	/* UFOP_CRS */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"CRS",							/* pchName */
	},
	/* UFOP_NRM */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"NRM",							/* pchName */
	},
	/* UFOP_ABS */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"ABS",							/* pchName */
	},
	/* UFOP_SINCOS */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"SINCOS",						/* pchName */
	},
	/* UFOP_SINCOS2 */
	{
		2,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"SINCOS2",						/* pchName */
	},
	/* UFOP_M4X4 */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"M4X4",							/* pchName */
	},
	/* UFOP_M4X3 */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"M4X3",							/* pchName */
	},
	/* UFOP_M3X4 */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"M3X4",							/* pchName */
	},
	/* UFOP_M3X3 */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"M3X3",							/* pchName */
	},
	/* UFOP_M3X2 */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"M3X2",							/* pchName */
	},
	/* UFOP_DSX */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"DSX",							/* pchName */
	},
	/* UFOP_DSY */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"DSY",							/* pchName */
	},
	/* UFOP_DST */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"DST",							/* pchName */
	},
	/* UFOP_LIT */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"LIT",							/* pchName */
	},
	/* UFOP_OGLLIT */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"OGLLIT",						/* pchName */
	},
	/* UFOP_SLT */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"SLT",							/* pchName */
	},
	/* UFOP_SGE */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"SGE",							/* pchName */
	},
	/* UFOP_SGN */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"SGN",							/* pchName */
	},
	/* UFOP_SETBEQ */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"SETBEQ",						/* pchName */
	},
	/* UFOP_SETBGE */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"SETBGE",						/* pchName */
	},
	/* UFOP_SETBLT */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"SETBLT",						/* pchName */
	},
	/* UFOP_SETBNE */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"SETBNE",						/* pchName */
	},
	/* UFOP_MOVCBIT */
	{
		1,								/* uNumDests */
		3,								/* uNumSrcArgs */
		"MOVCBIT",						/* pchName */
	},
	/* UFOP_DIV */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"DIV",							/* pchName */
	},
	/* UFOP_SQRT */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"SQRT",							/* pchName */
	},
	/* UFOP_AND */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"AND",							/* pchName */
	},
	/* UFOP_SHL */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"SHL",							/* pchName */
	},
	/* UFOP_ASR */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"ASR",							/* pchName */
	},
	/* UFOP_NOT */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"NOT",							/* pchName */
	},
	/* UFOP_OR */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"OR",							/* pchName */
	},
	/* UFOP_SHR */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"SHR",							/* pchName */
	},
	/* UFOP_XOR */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"XOR",							/* pchName */
	},
	/* Texture lookup instructions. */
	/* UFOP_LD */
	{
		1,								/* uNumDests */
		3,								/* uNumSrcArgs */
		"LD",							/* pchName */
	},
	/* UFOP_LDB */
	{
		1,								/* uNumDests */
		4,								/* uNumSrcArgs */
		"LDB",							/* pchName */
	},
	/* UFOP_LDL */
	{
		1,								/* uNumDests */
		4,								/* uNumSrcArgs */
		"LDL",							/* pchName */
	},
	/* UFOP_LDP */
	{
		1,								/* uNumDests */
		3,								/* uNumSrcArgs */
		"LDP",							/* pchName */
	},
	/* UFOP_LDPIFTC */
	{
		1,								/* uNumDests */
		3,								/* uNumSrcArgs */
		"LDPIFTC",						/* pchName */
	},
	/* UFOP_LDD */
	{
		1,								/* uNumDests */
		5,								/* uNumSrcArgs */
		"LDD",							/* pchName */
	},
	/* UFOP_LDC */
	{
		1,								/* uNumDests */
		4,								/* uNumSrcArgs */
		"LDC",							/* pchName */
	},
	/* UFOP_LDCLZ */
	{
		1,								/* uNumDests */
		4,								/* uNumSrcArgs */
		"LDCLZ",						/* pchName */
	},
	/* UFOP_LDLOD */
	{
		1,								/* uNumDests */
		3,								/* uNumSrcArgs */
		"LDLOD",						/* pchName */
	},
	/* UFOP_LDGATHER4 */
	{
		1,								/* uNumDests */
		3,								/* uNumSrcArgs */
		"LDGATHER4",					/* pchName */
	},
	/* UFOP_LD2DMS */
	{
		1,								/* uNumDests */
		5,								/* uNumSrcArgs */
		"LD2DMS",						/* pchName */
	},
	/* UFOP_LDTILED */
	{
		1,								/* uNumDests */
		3,								/* uNumSrcArgs */
		"LDTILED",						/* pchName */
	},
	/* UFOP_LDBUF */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"LDBUF",						/* pchName */
	},
	/* Control instructions. */
	/* UFOP_KPLT */
	{
		0,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"KPLT",							/* pchName */
	},
	/* UFOP_KPLE */
	{
		0,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"KPLE",							/* pchName */
	},
	/* UFOP_KILLNZBIT */
	{
		0,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"KPNZBIT",						/* pchName */
	},
	/* Program flow instructions. */
	/* UFOP_IF */
	{
		0,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"IF",							/* pchName */
	},
	/* UFOP_ELSE */
	{
		0,								/* uNumDests */
		0,								/* uNumSrcArgs */
		"ELSE",							/* pchName */
	},
	/* UFOP_ENDIF */
	{
		0,								/* uNumDests */
		0,								/* uNumSrcArgs */
		"ENDIF",						/* pchName */
	},
	/* UFOP_CALL */
	{
		0,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"CALL",							/* pchName */
	},
	/* UFOP_CALLNZ */
	{
		0,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"CALLNZ",						/* pchName */
	},
	/* UFOP_RET */
	{
		0,								/* uNumDests */
		0,								/* uNumSrcArgs */
		"RET",							/* pchName */
	},
	/* UFOP_LOOP */
	{
		0,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"LOOP",							/* pchName */
	},
	/* UFOP_ENDLOOP */
	{
		0,								/* uNumDests */
		0,								/* uNumSrcArgs */
		"ENDLOOP",						/* pchName */
	},
	/* UFOP_REP */
	{
		0,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"REP",							/* pchName */
	},
	/* UFOP_ENDREP */
	{
		0,								/* uNumDests */
		0,								/* uNumSrcArgs */
		"ENDREP",						/* pchName */
	},
	/* UFOP_IFC */
	{
		0,								/* uNumDests */
		3,								/* uNumSrcArgs */
		"IFC",							/* pchName */
	},
	/* UFOP_BREAK */
	{
		0,								/* uNumDests */
		0,								/* uNumSrcArgs */
		"BREAK",						/* pchName */
	},
	/* UFOP_BREAKC */
	{
		0,								/* uNumDests */
		3,								/* uNumSrcArgs */
		"BREAKC",						/* pchName */
	},
	/* UFOP_SETP */
	{
		1,								/* uNumDests */
		3,								/* uNumSrcArgs */
		"SETP",							/* pchName */
	},
	/* UFOP_LABEL */
	{
		0,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"LABEL",						/* pchName */
	},
	/* UFOP_IFP */
	{
		0,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"IFP",							/* pchName */
	},
	/* UFOP_BREAKP */
	{
		0,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"BREAKP",						/* pchName */
	},
	/* UFOP_CALLP */
	{
		0,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"CALLP",						/* pchName */
	},
	/* UFOP_GLSLLOOP */
	{
		0,								/* uNumDests */
		0,								/* uNumSrcArgs */
		"GLSLLOOP",						/* pchName */
	},
	/* UFOP_GLSLSTATICLOOP */
	{
		0,								/* uNumDests */
		0,								/* uNumSrcArgs */
		"GLSLSTATICLOOP",				/* pchName */
	},
	/* UFOP_GLSLENDLOOP */
	{
		0,								/* uNumDests */
		0,								/* uNumSrcArgs */
		"GLSLENDLOOP",					/* pchName */
	},
	/* UFOP_GLSLCONTINUE */
	{
		0,								/* uNumDests */
		0,								/* uNumSrcArgs */
		"GLSLCONTINUE",					/* pchName */
	},
	/* UFOP_GLSLLOOPTAIL */
	{
		0,								/* uNumDests */
		0,								/* uNumSrcArgs */
		"GLSLLOOPTAIL",					/* pchName */
	},
	/* UFOP_BREAKNZBIT */
	{
		0,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"BREAKNZBIT",					/* pchName */
	},
	/* UFOP_CALLNZBIT */
	{
		0,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"CALLNZBIT",					/* pchName */
	},
	/* UFOP_RETNZBIT */
	{
		0,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"RETNZBIT",						/* pchName */
	},
	/* UFOP_CONTINUENZBIT */
	{
		0,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"CONTINUENZBIT",				/* pchName */
	},
	/* UFOP_IFNZBIT */
	{
		0,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"IFNZBIT",						/* pchName */
	},
	/* UFOP_SWITCH */
	{
		0,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"SWITCH",						/* pchName */
	},
	/* UFOP_CASE */
	{
		0,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"CASE",							/* pchName */
	},
	/* UFOP_DEFAULT */
	{
		0,								/* uNumDests */
		0,								/* uNumSrcArgs */
		"DEFAULT",						/* pchName */
	},
	/* UFOP_ENDSWITCH */
	{
		0,								/* uNumDests */
		0,								/* uNumSrcArgs */
		"ENDSWITCH",					/* pchName */
	},
	/* Misc. instructions. */
	/* UFOP_NOP */
	{
		0,								/* uNumDests */
		0,								/* uNumSrcArgs */
		"NOP",							/* pchName */
	},
	/* UFOP_MOV */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"MOV",							/* pchName */
	},
	/* UFOP_MOVA_TRC */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"MOVA_TRC",						/* pchName */
	},
	/* UFOP_MOVA_RNE */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"MOVA_RNE",						/* pchName */
	},
	/* UFOP_MOVA_INT */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"MOVA_INT",						/* pchName */
	},
	/* UFOP_EXPP */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"EXPP",							/* pchName */
	},
	/* UFOP_LOGP */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"LOGP",							/* pchName */
	},
	/* UFOP_DOT4_CP */
	{
		1,								/* uNumDests */
		3,								/* uNumSrcArgs */
		"DOT4CP",						/* pchName */
	},
	/* UFOP_MOV_CP */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"MOVCP",						/* pchName */
	},
	/* DX10 I/U32 2-dest Instructions */
	/* UFOP_MUL2 */
	{
		2,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"MUL2",							/* pchName - note I/U prepended when printing out */
	},
	/* UFOP_DIV2 */
	{
		2,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"DIV2",							/* pchName - note I/U prepended when printing out */
	},
	/* UFOP_MOVVI */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"MOVVI",						/* pchName */
	},
	/* UFOP_EMIT */
	{
		0,								/* uNumDests */
		0,								/* uNumSrcArgs */
		"EMIT",							/* pchName */
	},
	/* UFOP_CUT */
	{
		0,								/* uNumDests */
		0,								/* uNumSrcArgs */
		"CUT",							/* pchName */
	},
	/* UFOP_EMITCUT */
	{
		0,								/* uNumDests */
		0,								/* uNumSrcArgs */
		"EMITCUT",						/* pchName */
	},
	/* UFOP_SPLIT */
	{
		0,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"SPLIT",						/* pchName */
	},
	/* UFOP_FTOI */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"FTOI",							/* pchName */
	},
	/* OpenCL program flow instructions. */
	/* UFOP_JUMP */
	{
		0,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"JUMP",							/* pchName */
	},
	/* UFOP_BLOCK */
	{
		0,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"BLOCK",							/* pchName */
	},
	/* OpenCL Memory load/store instructions */
	/* UFOP_MEMLD */
	{
		1,								/* uNumDests */
		4,								/* uNumSrcArgs */
		"MEMLD",					/* pchName */
	},
	/* UFOP_MEMST */
	{
		0,								/* uNumDests */
		6,								/* uNumSrcArgs */
		"MEMST",					/* pchName */
	},
	/* OpenCL I/U32 2-dest Instructions */
	/* UFOP_ADD2 */
	{
		2,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"ADD",							/* pchName - note I/U prepended when printing out */
	},
	/* UFOP_MAD2 */
	{
		2,								/* uNumDests */
		3,								/* uNumSrcArgs */
		"MAD",							/* pchName - note I/U prepended when printing out */
	},
#if defined(OUTPUT_USPBIN)
	/* UFOP_TEXWRITE */
	{
		0,								/* uNumDests */
		5,								/* uNumSrcArgs */
		"TEXWRITE",						/* pchName */
	},
#endif /* defined(OUTPUT_USPBIN) */
	/* UFOP_CMPLT */
	{
		1,								/* uNumDests */
		3,								/* uNumSrcArgs */
		"CMPLT",						/* pchName */
	},
	/* UFOP_LOCK */
	{
		0,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"LOCK",					 	    /* pchName */
	},
	/* UFOP_RELEASE */
	{
		0,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"RELEASE",						/* pchName */
	},
	/* UFOP_ATOM_INC */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"ATOM_INC",						/* pchName */
	},
	/* UFOP_ATOM_DEC */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"ATOM_DEC",						/* pchName */
	},
	/* UFOP_ATOM_ADD */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"ATOM_ADD",						/* pchName */
	},
	/* UFOP_ATOM_SUB */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"ATOM_SUB",						/* pchName */
	},
	/* UFOP_ATOM_AND */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"ATOM_AND",						/* pchName */
	},
	/* UFOP_ATOM_OR */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"ATOM_OR",						/* pchName */
	},
	/* UFOP_ATOM_XOR */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"ATOM_XOR",						/* pchName */
	},
	/* UFOP_ATOM_XCHG */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"ATOM_XCHG",					/* pchName */
	},
	/* UFOP_ATOM_CMPXCHG */
	{
		1,								/* uNumDests */
		3,								/* uNumSrcArgs */
		"ATOM_CMPXCHG",					/* pchName */
	},
	/* UFOP_ATOM_MAX */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"ATOM_MAX",						/* pchName */
	},
	/* UFOP_ATOM_MIN */
	{
		1,								/* uNumDests */
		2,								/* uNumSrcArgs */
		"ATOM_MIN",						/* pchName */
	},
	/* UFOP_REINTERP */
	{
		1,								/* uNumDests */
		1,								/* uNumSrcArgs */
		"REINTERP",						/* pchName */
	},
};

/*********************************************************************************
 Function			: PVRUniFlexUnpackTextureStateConstantOffset

 Description		:

 Parameters			: None.

 Globals Effected	: None

 Return				: Nothing
*********************************************************************************/
USC_EXPORT_OPENGL
IMG_VOID IMG_CALLCONV PVRUniFlexUnpackTextureStateConstantOffset(IMG_UINT32						uHwTextureStateSize,
																 IMG_UINT32						uOffset,
																 PUNIFLEX_TEXTURE_STATE_TYPE	peType,
																 IMG_PUINT32					puSampler,
																 IMG_PUINT32					puChunk,
																 IMG_PUINT32					puHwStateWord)
{
	IMG_UINT32	uStateType;

	*puSampler = _UNIFLEX_TEXSTATE_GETSAMPLER(uOffset, uHwTextureStateSize);
	uStateType = _UNIFLEX_TEXSTATE_GESTATETYPE(uOffset, uHwTextureStateSize);

	if (uStateType >= UNIFLEX_TEXSTATE_HWSTATE_START && 
		uStateType < _UNIFLEX_TEXSTATE_HWSTATE_END(uHwTextureStateSize))
	{
		IMG_UINT32	uRelOffset = uStateType - UNIFLEX_TEXSTATE_HWSTATE_START;

		*puChunk = (uRelOffset / uHwTextureStateSize) % UF_MAX_CHUNKS_PER_TEXTURE;
		*puHwStateWord = uRelOffset % uHwTextureStateSize;
		*peType = UNIFLEX_TEXTURE_STATE_TYPE_HWSTATE;
	}
	else if (uStateType >= _UNIFLEX_TEXSTATE_STRIDE_START(uHwTextureStateSize) &&
			 uStateType < _UNIFLEX_TEXSTATE_STRIDE_END(uHwTextureStateSize))
	{
		IMG_UINT32	uRelOffset = uStateType - _UNIFLEX_TEXSTATE_STRIDE_START(uHwTextureStateSize);
		
		*puChunk = uRelOffset % UF_MAX_CHUNKS_PER_TEXTURE;
		*peType = UNIFLEX_TEXTURE_STATE_TYPE_STRIDE;
	}
	else if (uStateType == _UNIFLEX_TEXSTATE_MAXARRAYINDEX(uHwTextureStateSize))
	{
		*peType = UNIFLEX_TEXTURE_STATE_TYPE_MAXARRAYINDEX;
	}
	else if (uStateType == _UNIFLEX_TEXSTATE_WIDTH(uHwTextureStateSize))
	{
		*peType = UNIFLEX_TEXTURE_STATE_TYPE_WIDTH;
	}
	else if (uStateType == _UNIFLEX_TEXSTATE_HEIGHT(uHwTextureStateSize))
	{
		*peType = UNIFLEX_TEXTURE_STATE_TYPE_HEIGHT;
	}
	else if (uStateType == _UNIFLEX_TEXSTATE_DEPTH(uHwTextureStateSize))
	{
		*peType = UNIFLEX_TEXTURE_STATE_TYPE_DEPTH;
	}
	else
	{
		*peType = UNIFLEX_TEXTURE_STATE_TYPE_UNDEFINED;
	}
}

/*********************************************************************************
 Function			: PVRUniFlexInitTextureParameters

 Description		: Initialise a UNIFLEX_TEXFORM structure.

 Parameters			: None.

 Globals Effected	: None

 Return				: Nothing
*********************************************************************************/
USC_EXPORT
IMG_VOID IMG_CALLCONV PVRUniFlexInitTextureParameters(PUNIFLEX_TEXTURE_PARAMETERS	psTexParams)
{
	PUNIFLEX_TEXFORM psTex = &psTexParams->sFormat;

	psTex->peChannelForm[0] = USC_CHANNELFORM_U8;
	psTex->peChannelForm[1] = USC_CHANNELFORM_U8;
	psTex->peChannelForm[2] = USC_CHANNELFORM_U8;
	psTex->peChannelForm[3] = USC_CHANNELFORM_U8;
	psTex->uSwizzle = UFREG_SWIZ_NONE;
	psTex->bTwoResult = IMG_FALSE;
	psTex->bUsePCF = IMG_FALSE;
	psTex->uChunkSize = 4;
	psTex->uCSCConst = UINT_MAX;
	psTex->bTFSwapRedAndBlue = IMG_FALSE;
	psTex->bMinPack = IMG_FALSE;

	psTexParams->bGamma = IMG_FALSE;
	psTexParams->bMipMap = IMG_FALSE;
	psTexParams->eMinFilter = UNIFLEX_TEXTURE_FILTER_TYPE_POINT;
	psTexParams->eMagFilter = UNIFLEX_TEXTURE_FILTER_TYPE_POINT;
	psTexParams->eTrilinearType = UNIFLEX_TEXTURE_TRILINEAR_TYPE_OFF;
	psTexParams->ePCFComparisonMode = UFREG_COMPOP_ALWAYS;
}

/*********************************************************************************
 Function			: PVRUniFlexInitInst

 Description		: Initialise a uniflex instruction.

 Parameters			: None.

 Globals Effected	: None

 Return				: Nothing
*********************************************************************************/
USC_EXPORT
IMG_VOID IMG_CALLCONV PVRUniFlexInitInst(IMG_PVOID pvContext, PUNIFLEX_INST psInst)
{
	PVR_UNREFERENCED_PARAMETER(pvContext);

	memset(psInst, 0, sizeof(*psInst));

	/* Set some default values */
	psInst->eOpCode = UFOP_INVALID;
}

/*****************************************************************************
 FUNCTION	: PVRUniFlexGetInputInstDesc

 PURPOSE	: Get the properties of an opcode in the USC input instruction set.

 PARAMETERS	: eOpcode		- Opcode to get information about.

 RETURNS	: A pointer to a description of the instruction.
*****************************************************************************/
USC_EXPORT
INPUT_INST_DESC const* PVRUniFlexGetInputInstDesc(UF_OPCODE	eOpcode)
{
	if (eOpcode < (UF_OPCODE)(sizeof(g_asInputInstDesc) / sizeof(g_asInputInstDesc[0])))
	{
		return &g_asInputInstDesc[eOpcode];
	}
	else
	{
		return NULL;
	}
}

#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)

IMG_INTERNAL IMG_CHAR const g_pszChannels[8] = {'r', 'g', 'b', 'a', '1', '0', 'h', '2'};

/*****************************************************************************
 FUNCTION	: PVRUniFlexDecodeInputSwizzle
    
 PURPOSE	: Return a pointer to a string representation of a swizzle.

 PARAMETERS	: bDot		- If TRUE a '.' is prepended to the decoded swizzle
			  uSwiz		- The Uniflex input swizzle to decode
			  pszSwiz	- Buffer where the decoded swizzle is written
						  (max 4 chars if bDot is false, else 5 chars).
			  
 RETURNS	: The string representing the swizzle.
*****************************************************************************/
IMG_INTERNAL IMG_PCHAR PVRUniFlexDecodeInputSwizzle(IMG_BOOL	bDot, 
													IMG_UINT32	uSwiz, 
													IMG_PCHAR	pszSwiz)
{
	IMG_UINT32 i;
	IMG_PCHAR pszS;
	if (uSwiz == UFREG_SWIZ_NONE)
	{
		pszSwiz[0] = 0;
	}
	else
	{
		pszS = pszSwiz;
		if (bDot)
		{
			*pszS++ = '.';
		}
		for (i = 0; i < 4; i++)
		{
			*pszS++ = g_pszChannels[(uSwiz >> (i * 3)) & 0x7];
		}
	}
	return pszSwiz;
}

IMG_INTERNAL IMG_VOID InternalDecodeComparisonOperator(IMG_UINT32	uNum,
													   IMG_PCHAR	pszStr)
/*****************************************************************************
 FUNCTION	: InternalDecodeComparisonOperator
    
 PURPOSE	: Gets a string representation of the value of a comparison operator
			  argument in the USC input language.

 PARAMETERS	: uCompOp		- Comparison operator.
			  pszStr		- Overwritten with the string representation.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32 uCompOp;
	IMG_UINT32 uChanOp;
	IMG_PCHAR const apcCompOp[] = {"invalid", ">", "==", ">=", "<", "!=", "<=", "never", "always"};
	IMG_PCHAR const apcChanOp[] = {"invalid", "andall", "orall"};

	uCompOp = uNum & (~UFREG_COMPCHANOP_MASK);
	uChanOp = ((uNum & UFREG_COMPCHANOP_MASK) >>  UFREG_COMPCHANOP_SHIFT);
	if (uChanOp)
	{
		sprintf(pszStr, "%s(%s)", apcChanOp[uChanOp%3], apcCompOp[uCompOp]);
	}
	else
	{
		sprintf(pszStr, "%s", apcCompOp[uCompOp]);
	}
}

IMG_INTERNAL IMG_VOID InternalDecodeInputInst(IMG_UINT32	uCompileFlags,
											  PUNIFLEX_INST	psInst, 
											  IMG_INT32		i32Indent,
											  IMG_PCHAR		pszInst,
											  IMG_PUINT32	puTexUseMask)
{
	#define _ADDSTR(S) sprintf(pszInst + strlen(pszInst), S)
	#define _ADDSTRP1(S, P1) sprintf(pszInst + strlen(pszInst), S, P1)
	#define _ADDSTRP2(S, P1, P2) sprintf(pszInst + strlen(pszInst), S, P1, P2)
	#define _ADDSTRP3(S, P1, P2, P3) sprintf(pszInst + strlen(pszInst), S, P1, P2, P3)
	#define _ADDSTRP5(S, P1, P2, P3, P4, P5) sprintf(pszInst + strlen(pszInst), S, P1, P2, P3, P4, P5)
	#define _ADDSTRP6(S, P1, P2, P3, P4, P5, P6) sprintf(pszInst + strlen(pszInst), S, P1, P2, P3, P4, P5, P6)
	#define _ADDSTRP7(S, P1, P2, P3, P4, P5, P6, P7) sprintf(pszInst + strlen(pszInst), S, P1, P2, P3, P4, P5, P6, P7)
	
	#ifdef SRC_DEBUG
	#define _ADDUINT(UI) sprintf(pszInst + strlen(pszInst),"[%u]", UI)
	#endif /* SRC_DEBUG */

	IMG_PCHAR const apcSat[] = {"", "_sat01", "_sat11", "_sat0max"};
	IMG_PCHAR const apcScale[] = {"", "_x2", "_x4", "_x8", "/16", "/8", "/4", "/2"};
	IMG_PCHAR const apcType[UFREG_TYPE_MAXIMUM] = 
	{
		"r",			/* UFREG_TYPE_TEMP */
		"v",			/* UFREG_TYPE_COL */
		"tc",			/* UFREG_TYPE_TEXCOORD */
		"c",			/* UFREG_TYPE_CONST */
		"t",			/* UFREG_TYPE_TEX */
		"h",			/* UFREG_TYPE_HW_CONST */
		"MC",			/* UFREG_TYPE_PSOUTPUT */
		"i",			/* UFREG_TYPE_ICONST */
		"b",			/* UFREG_TYPE_BCONST */
		"m",			/* UFREG_TYPE_MISC */
		"cop",			/* UFREG_TYPE_COMPOP */
		"L",			/* UFREG_TYPE_LABEL */
		"p",			/* UFREG_TYPE_PREDICATE */
		"tcp",			/* UFREG_TYPE_TEXCOORDP */
		"tcpiftc",		/* UFREG_TYPE_TEXCOORDPIFTC */ 
		"vi",			/* UFREG_TYPE_VSINPUT */
		"vo",			/* UFREG_TYPE_VSOUTPUT */
		"a",			/* UFREG_TYPE_ADDRESS */
		"ct",	 
		"x",			/* UFREG_TYPE_INDEXABLETEMP */
		"cp",			/* UFREG_TYPE_CLIPPLANE */
		"I",			/* UFREG_TYPE_IMMEDIATE */
	};
	IMG_PCHAR const apcMask[] =
	{
		"._", ".r", ".g", ".rg",
		".b", ".rb", ".gb", ".rgb",
		".a", ".ra", ".ga", ".rga",
		".ba", ".rba", ".gba", ""
	};
	IMG_PCHAR const apcSmod[] = {"", "_comp", "_signed", "_bias", "_timestwo"};
	IMG_UINT32 i, uNumDests;
	IMG_PCHAR const apcPred[] = {"none", "", "reserved", "reserved", ".x", ".y", ".z", ".w"};
	IMG_PCHAR const apcRelativeIndex[] = {"",			/* UFREG_RELATIVEINDEX_NONE */
									   "_a0x",		/* UFREG_RELATIVEINDEX_A0X */
									   "_a0y",		/* UFREG_RELATIVEINDEX_A0Y */
									   "_a0z",		/* UFREG_RELATIVEINDEX_A0Z */
									   "_a0w",		/* UFREG_RELATIVEINDEX_A0W */
									   "_al"		/* UFREG_RELATIVEINDEX_A0L */};
	IMG_PCHAR const apcRelativeChan[] = {"x", "y", "z", "w"};
	IMG_PCHAR const apcFormat[UF_REGFORMAT_COUNT] = {
													"f32",		/* UF_REGFORMAT_F32 */
													"f16",		/* UF_REGFORMAT_F16 */
													"c10",		/* UF_REGFORMAT_C10 */
													"u8",		/* UF_REGFORMAT_U8 */
													"i32",		/* UF_REGFORMAT_I32 */
													"u32",		/* UF_REGFORMAT_U32 */
													"i16",		/* UF_REGFORMAT_I16 */
													"u16",		/* UF_REGFORMAT_U16 */
													"untyped",	/* UF_REGFORMAT_UNTYPED */
													"i8",		/* UF_REGFORMAT_I8_UN */
													"ui8",		/* UF_REGFORMAT_U8_UN */
												 };
	IMG_CHAR pszSwiz[5];

	pszInst[0] = '\0';

	#ifdef SRC_DEBUG
	if((psInst->uSrcLine) == UNDEFINED_SOURCE_LINE)
	{
		_ADDSTR("[-1]");
	}
	else
	{
		_ADDUINT(psInst->uSrcLine);
	}
	#endif /* SRC_DEBUG */

	/* Indent the instruction */
	while(i32Indent-- > 0)
	{
		_ADDSTR("\t");
	}

	if (psInst->uPredicate != 0)
	{
		if (psInst->uPredicate & UF_PRED_NEGFLAG)
		{
			_ADDSTR("!");
		}
		_ADDSTR("p");

		if	(psInst->uPredicate & UF_PRED_IDX_MASK)
		{
			_ADDSTRP1( "%d", ((psInst->uPredicate & UF_PRED_IDX_MASK) >> UF_PRED_IDX_SHIFT) );
		}

		_ADDSTRP1("%s", apcPred[(psInst->uPredicate & UF_PRED_COMP_MASK) >> UF_PRED_COMP_SHIFT]);
		_ADDSTR("\t");
	}

	_ADDSTRP1( "%s\t", g_asInputInstDesc[psInst->eOpCode].pchName );

	for (i = 0, uNumDests = g_asInputInstDesc[psInst->eOpCode].uNumDests; i < uNumDests; i++)
	{
		PUF_REGISTER psDest=(i==0) ? &psInst->sDest : &psInst->sDest2;
		/* special-case printing empty first dest *iff* there are two dests */
		if ( (i+1) < uNumDests && psDest->u.byMask == 0)
		{
			_ADDSTR ("null+");
			continue;
		}
		if (psDest->eType == UFREG_TYPE_PSOUTPUT && psDest->uNum == UFREG_OUTPUT_Z)
		{
			_ADDSTR( "Z" );
		}
		else if (psDest->eType == UFREG_TYPE_PSOUTPUT && psDest->uNum == UFREG_OUTPUT_OMASK)
		{
			_ADDSTR( "oM" );
		}
		else if (psDest->eType == UFREG_TYPE_PREDICATE && psDest->uNum == 0)
		{
			_ADDSTR("p");
		}
		else
		{
			_ADDSTRP2( "%s%u", apcType[psDest->eType], psDest->uNum );
		}
		_ADDSTRP2("%s%s", 
				apcScale[(psDest->byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT], 
				apcSat[(psDest->byMod  & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT]);
		if(psDest->eRelativeIndex == UFREG_RELATIVEINDEX_TEMP_REG)
		{
			_ADDSTRP2("_r%u%s", psDest->uRelativeNum, apcRelativeChan[psDest->byRelativeChan]);
		}
		else
		{
			_ADDSTRP1("%s", apcRelativeIndex[psDest->eRelativeIndex]);
		}
		if (psDest->eType == UFREG_TYPE_INDEXABLETEMP)
		{
			_ADDSTRP1("_t%u", psDest->uArrayTag);
		}
		if (psDest->eFormat != UF_REGFORMAT_F32)
		{
			_ADDSTRP1("_%s", apcFormat[psDest->eFormat]);
		}
		if (
				(uCompileFlags & UF_GLSL) != 0 &&
				psDest->eRelativeIndex != UFREG_RELATIVEINDEX_NONE
		   )
		{
			_ADDSTRP1("_h%u", psDest->u1.uRelativeStrideInComponents);
		}
		_ADDSTRP1( "%s", apcMask[psDest->u.byMask]);
		/* more dests to come? */
		if (i + 1 < uNumDests)
		{
			_ADDSTR ("+");
			if (psInst->sDest2.u.byMask == 0)
			{
				/* second dest empty - skip next iter and print null instead */
				_ADDSTR("null, ");
				break;
			}
		}
		else
		{
			_ADDSTR (", ");
		}
	}

	for(i = 0; i < g_asInputInstDesc[psInst->eOpCode].uNumSrcArgs; i++)
	{
		IMG_BOOL	bDecodeSwizzle = IMG_TRUE;

		if (psInst->asSrc[i].eType == UFREG_TYPE_PREDICATE && psInst->asSrc[i].uNum == 0)
		{
			_ADDSTRP1("%s", apcType[psInst->asSrc[i].eType]);
		}
		else if (psInst->asSrc[i].eType == UFREG_TYPE_COMPOP)
		{
			InternalDecodeComparisonOperator(psInst->asSrc[i].uNum, pszInst + strlen(pszInst));
			bDecodeSwizzle = IMG_FALSE;
		}
		else if (psInst->asSrc[i].eType == UFREG_TYPE_MISC)
		{
			IMG_PCHAR pszMiscType;

			switch (psInst->asSrc[i].uNum)
			{
				case UF_MISC_POSITION: pszMiscType = "P"; break;
				case UF_MISC_FACETYPE: pszMiscType = "B"; break;
				case UF_MISC_FACETYPEBIT: pszMiscType = "T"; break;
				case UF_MISC_VPRIM: pszMiscType = "I"; break;
				case UF_MISC_RTAIDX: pszMiscType = "R"; break;
				case UF_MISC_MSAASAMPLEIDX: pszMiscType = "S"; break;
				case UF_MISC_FOG: pszMiscType = "F"; break;
				default: pszMiscType = "??"; break;
			}

			_ADDSTRP2("%s%s", apcType[psInst->asSrc[i].eType], pszMiscType);
		}
		else if (psInst->asSrc[i].eType == UFREG_TYPE_LABEL && psInst->asSrc[i].uNum == USC_MAIN_LABEL_NUM)
		{
			_ADDSTR("MAIN");
			bDecodeSwizzle = IMG_FALSE;
		}
		else
		{
			_ADDSTRP2("%s%u", apcType[psInst->asSrc[i].eType], psInst->asSrc[i].uNum);
		}

		if (psInst->asSrc[i].eRelativeIndex == UFREG_RELATIVEINDEX_TEMP_REG)
		{
			_ADDSTRP2("_r%u%s", psInst->asSrc[i].uRelativeNum, apcRelativeChan[psInst->asSrc[i].byRelativeChan]);			
		}
		else
		{
			_ADDSTRP1("%s", apcRelativeIndex[psInst->asSrc[i].eRelativeIndex]);
		}
		_ADDSTRP1("%s", apcSmod[(psInst->asSrc[i].byMod >> UFREG_SMOD_SHIFT) & UFREG_SMOD_MASK]);
		if (psInst->asSrc[i].byMod & UFREG_SOURCE_NEGATE)
		{
			_ADDSTR("_neg");
		}
		if (psInst->asSrc[i].byMod & UFREG_SOURCE_ABS)
		{
			_ADDSTR("_abs");
		}
		if (bDecodeSwizzle)
		{
			_ADDSTRP1("%.5s", PVRUniFlexDecodeInputSwizzle(IMG_TRUE, psInst->asSrc[i].u.uSwiz, pszSwiz));
		}

		if (psInst->asSrc[i].eType == UFREG_TYPE_INDEXABLETEMP)
		{
			_ADDSTRP1("_t%u", psInst->asSrc[i].uArrayTag);
		}
		if (psInst->asSrc[i].eFormat != UF_REGFORMAT_F32)
		{
			_ADDSTRP1("_%s", apcFormat[psInst->asSrc[i].eFormat]);
		}
		if (
				(uCompileFlags & UF_GLSL) != 0 &&
				psInst->asSrc[i].eRelativeIndex != UFREG_RELATIVEINDEX_NONE
		   )
		{
			_ADDSTRP1("_h%u", psInst->asSrc[i].u1.uRelativeStrideInBytes);
		}
		if (
				psInst->asSrc[i].eType == UFREG_TYPE_CONST &&
				(uCompileFlags & UF_CONSTEXPLICTADDRESSMODE) != 0
		   )
		{
			_ADDSTRP1("_h%u", psInst->asSrc[i].u1.uRelativeStrideInBytes);
		}
		if (
				psInst->asSrc[i].eType == UFREG_TYPE_CONST &&
				psInst->asSrc[i].uArrayTag != 0
		   )
		{
			_ADDSTRP1("_t%u", psInst->asSrc[i].uArrayTag);
		}
		
		if (psInst->asSrc[i].eType == UFREG_TYPE_TEX)
		{
			if	(puTexUseMask)
			{
				SetBit(puTexUseMask, psInst->asSrc[i].uNum, 1);
			}
		}
		if (i != g_asInputInstDesc[psInst->eOpCode].uNumSrcArgs - 1)
		{
			_ADDSTR( ", " );
		}
	}

	#undef _ADDSTR
	#undef _ADDSTRP1
	#undef _ADDSTRP2
	#undef _ADDSTRP3
	#undef _ADDSTRP5
	#undef _ADDSTRP6
	#undef _ADDSTRP7
}


/*****************************************************************************
 FUNCTION	: PVRUniFlexDecodeInputInst

 PURPOSE	: Decode an individual uniflex input instruction.

 PARAMETERS	: psInst		- Instruction to decode.
			  i32Indent		- The number of tabs to indent the instruction by
              pszInst		- Buffer where the decoded instruction is written
							  (minimum 512 characters).
			  puTexUseMask	- Accumulated bitmask representing the textures 
							  used by sample instruction (bit 0 = texture 0). 
							  The corresponding bit is set when a texture is
							  used. Bits are never cleared. Unused if NULL.

 RETURNS	: Nothing.
*****************************************************************************/
USC_EXPORT
IMG_VOID PVRUniFlexDecodeInputInst( PUNIFLEX_INST	psInst, 
									IMG_INT32		i32Indent,
									IMG_PCHAR		pszInst,
									IMG_PUINT32		puTexUseMask)
{
	InternalDecodeInputInst(0 /* uCompileFlags */, psInst, i32Indent, pszInst, puTexUseMask);	
}

static
IMG_PCHAR DecodeIterationSource(PUNIFLEX_TEXTURE_LOAD	psIteration,
								IMG_PCHAR				pszIteration)
/*****************************************************************************
 FUNCTION	: DecodeIterationSource

 PURPOSE	: Decode the source to an iteration.

 PARAMETERS	: psIteration	- Iteration to decode.
              pszIteration	- Buffer where the decoded iteration is written
							  (minimum 512 characters).

 RETURNS	: A pointer to the next character after those written to the buffer.
*****************************************************************************/
{
	static const IMG_PCHAR pszCoordDim[] = {".none", "u", "uv", "uvs", "uvst"};

	switch (psIteration->eIterationType)
	{
		case UNIFLEX_ITERATION_TYPE_TEXTURE_COORDINATE:
		{
			pszIteration +=
				sprintf(pszIteration, "tc%d.%s", psIteration->uCoordinate, pszCoordDim[psIteration->uCoordinateDimension]);
			break;
		}
		case UNIFLEX_ITERATION_TYPE_COLOUR:
		{
			pszIteration +=
				sprintf(pszIteration, "v%d", psIteration->uCoordinate);
			break;
		}
		case UNIFLEX_ITERATION_TYPE_FOG:
		{
			pszIteration +=
				sprintf(pszIteration, "fog.%s", pszCoordDim[psIteration->uCoordinateDimension]);
			break;
		}
		case UNIFLEX_ITERATION_TYPE_POSITION:
		{
			pszIteration +=
				sprintf(pszIteration, "pos.%s", pszCoordDim[psIteration->uCoordinateDimension]);
			break;
		}
		case UNIFLEX_ITERATION_TYPE_VPRIM:
		{
			pszIteration +=
				sprintf(pszIteration, "vprim.%s", pszCoordDim[psIteration->uCoordinateDimension]);
			break;
		}
		default: abort();
	}
	return pszIteration;
}

/*****************************************************************************
 FUNCTION	: PVRUniFlexDecodeIteration

 PURPOSE	: Decode an individual uniflex iteration.

 PARAMETERS	: psIteration	- Iteration to decode.
              pszIteration	- Buffer where the decoded iteration is written
							  (minimum 512 characters).

 RETURNS	: A pointer to the next character after those written to the buffer.
*****************************************************************************/
USC_EXPORT
IMG_PCHAR PVRUniFlexDecodeIteration( PUNIFLEX_TEXTURE_LOAD	psIteration,
									 IMG_PCHAR				pszIteration)
{
	if (psIteration->uTexture == UNIFLEX_TEXTURE_NONE)
	{
		static const IMG_PCHAR pszFormat[] = {"f32", "f16", "c10", "u8"};

		if	(psIteration->eFormat != UNIFLEX_TEXLOAD_FORMAT_F32)
		{
			pszIteration += sprintf(pszIteration, "%s(", pszFormat[psIteration->eFormat]);
		}
		pszIteration = DecodeIterationSource(psIteration, pszIteration);
		if	(psIteration->eFormat != UNIFLEX_TEXLOAD_FORMAT_F32)
		{
			pszIteration += sprintf(pszIteration, ")");
		}
	}
	else
	{
		pszIteration += sprintf(pszIteration, "texld%s(", psIteration->bProjected ? "p" : "");
		pszIteration = DecodeIterationSource(psIteration, pszIteration);
		pszIteration += sprintf(pszIteration, ", t%d.chunk%d)", psIteration->uTexture, psIteration->uChunk);
	}
	return pszIteration;
}
#else

/* Some platforms require stub functions for building the dll without debug */

#endif /* #if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */

/******************************************************************************
 End of file (debug.c)
******************************************************************************/

