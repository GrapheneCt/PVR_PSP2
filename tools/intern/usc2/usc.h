/******************************************************************************
 * Name         : usc.h
 * Title        : External interface to USC
 * Created      : April 2001
 *
 * Copyright	: 2002-2007 by Imagination Technologies Ltd.
 *                All rights reserved. No part of this software, either
 *                material or conceptual may be copied or distributed,
 *                transmitted, transcribed, stored in a retrieval system or
 *                translated into any human or computer language in any form
 *                by any means, electronic, mechanical, manual or otherwise,
 *                or disclosed to third parties without the express written
 *                permission of Imagination Technologies Ltd,
 *                Home Park Estate, Kings Langley, Hertfordshire,
 *                WD4 8LZ, U.K.
 *
 * Modifications :-
 * $Log: usc.h $
 * /
 *  --- Revision Logs Removed --- 
 * of 4.
 *  --- Revision Logs Removed --- 
 *****************************************************************************/
#ifndef _USC_H_
#define _USC_H_

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

#include "img_types.h"
#include "sgxcoretypes.h"
#include <limits.h>

#if defined(FFGEN_UNIFLEX)
#define USC_IMPORT IMG_IMPORT
#define USC_EXPORT IMG_EXPORT
#else
#define USC_IMPORT
#define USC_EXPORT IMG_INTERNAL
#endif

#if defined(SUPPORT_OPENGL)
#define USC_IMPORT_OPENGL IMG_IMPORT
#define USC_EXPORT_OPENGL IMG_EXPORT
#else
#define USC_IMPORT_OPENGL
#define USC_EXPORT_OPENGL IMG_INTERNAL
#endif

/*
 * Enable precision conversion redundancy check as default
*/
#define TRACK_REDUNDANT_PCONVERSION 1

/*
	UF_ERRNO: Error numbers for uniflex compilation
*/
typedef enum _UF_ERRNO_
{
	UF_OK	= 0,     /* No error */
	UF_ERR_INVALID_OPCODE = 1,
    UF_ERR_INVALID_DST_REG = 2, 
	UF_ERR_INVALID_SRC_REG = 3,
	UF_ERR_INVALID_DST_MOD = 4, 
	UF_ERR_INVALID_SRC_MOD = 5,
	UF_ERR_TOO_MANY_INSTS = 6,
	UF_ERR_GENERIC = 7,
	UF_ERR_INTERNAL	= 8,
	UF_ERR_NO_MEMORY = 9,
	UF_ERR_INVALID_PROG_STRUCT = 10
} UF_ERRNO;

/*
	Size (in dwords) of a bit array holding N bits.
*/
#define UNIFLEX_BIT_ARRAY_SIZE(N)		((N + 31) / 32)

/*
	Maximum number of (vec4) predicate registers
*/
#define UF_MAX_PRED_REGS	1024

/*
	Maximum number of integer constants.
*/
#define UF_MAX_INTEGER_REGS	16

/*
	Maximum label number.
*/
#define UF_MAX_LABEL		16

/*
	Maximum number of input instructions.
*/
#define UF_MAX_INSTRUCTIONS	2048

#define UF_MAX_CHUNKS_PER_TEXTURE	4

#define UF_MAX_TEXTURE_DIMENSIONALITY	3

/*
	=	the maximum number of registers read or written by a single instruction
		For non-OUTPUT_USPBIN builds: (= 9 for an SMP on a 3D texture with supplied gradients)
		For OUTPUT_USPBIN builds: (=15 because of extra temporary registers required when the USP
								   expands a texture sample)
	  + enough registers to save all the internal registers before a spill
		(= 4 (3 internal registers each 40 bits wide))
	  + 1 register to hold the address to spill to/from
*/
#if defined(OUTPUT_USPBIN)
#define	UF_MINIMUM_HARDWARE_TEMPORARIES_USP_SMP		(15)
#define UF_MINIMUM_HARDWARE_TEMPORARIES_USPBIN		(14 + UF_MINIMUM_HARDWARE_TEMPORARIES_USP_SMP)
#endif /* defined(OUTPUT_USPBIN) */

#if defined(OUTPUT_USCHW)
#define UF_MINIMUM_HARDWARE_TEMPORARIES_SMP			(9)
#define UF_MINIMUM_HARDWARE_TEMPORARIES_USCHW		(5 + UF_MINIMUM_HARDWARE_TEMPORARIES_SMP)
#endif /* defined(OUTPUT_USCHW) */

#define UF_MAX_VERTEX_SHADER_INPUTS    (32)

/*
	Input instructions.
*/
typedef enum tagUF_OPCODE
{
	/* Arithmetic instructions. */
	UFOP_ADD = 0,	/* note overloaded for I32 as well as floating point */
	UFOP_SUB,
	UFOP_MUL,
	UFOP_MAD,		/* note overloaded for I/U32 as well as floating point */
	UFOP_DOT2ADD,
	UFOP_DOT3,
	UFOP_DOT4,
	UFOP_RECIP,
	UFOP_RSQRT,
	UFOP_EXP,
	UFOP_LOG,
	UFOP_CMP,
	UFOP_CND,
	UFOP_FRC,
	UFOP_FLR,
	UFOP_TRC,
	UFOP_RNE,
	UFOP_RND,
	UFOP_CEIL,
	UFOP_LRP,
	UFOP_POW,
	UFOP_MIN,		/* note overloaded for I/U32 as well as floating point */
	UFOP_MAX,		/* note overloaded for I/U32 as well as floating point */
	UFOP_CRS,
	UFOP_NRM,
	UFOP_ABS,
	UFOP_SINCOS,
	UFOP_SINCOS2,
	UFOP_M4X4,
	UFOP_M4X3,
	UFOP_M3X4,
	UFOP_M3X3,
	UFOP_M3X2,
	UFOP_DSX,
	UFOP_DSY,
	UFOP_DST,
	UFOP_LIT,
	UFOP_OGLLIT,
	UFOP_SLT,
	UFOP_SGE,
	UFOP_SGN,
	UFOP_SETBEQ,
	UFOP_SETBGE,
	UFOP_SETBLT,
	UFOP_SETBNE,
	UFOP_MOVCBIT,
	UFOP_DIV,		/* note overloaded for U32 (with 2 dests!) as well as floating point */
	UFOP_SQRT,

	/* Bitwise instructions. */
	UFOP_AND,
	UFOP_SHL,
	UFOP_ASR,
	UFOP_NOT,
	UFOP_OR,
	UFOP_SHR,
	UFOP_XOR,

	/* Texture lookup instructions. */
	UFOP_LD,
	UFOP_FIRST_LD_OPCODE = UFOP_LD,
	UFOP_LDB,
	UFOP_LDL,
	UFOP_LDP,
	UFOP_LDPIFTC,
	UFOP_LDD,
	UFOP_LDC,
	UFOP_LDCLZ,
	UFOP_LDLOD,
	UFOP_LDGATHER4,
	UFOP_LD2DMS,
	UFOP_LAST_LD_OPCODE = UFOP_LD2DMS,

	/*
		LDTILED	DEST, SRC0, SRC1, TEXn
		
		DEST = Unpack using TEXn format from MEM[SRC0 + PIXEL_X * SRC1.LOWWORD + PIXEL_Y * SRC1.HIGHWORD]
	*/
	UFOP_LDTILED,

	/* Load from buffer instruction. */
	UFOP_LDBUF,

	/* Control instructions. */
	UFOP_KPLT,
	UFOP_KPLE,
	UFOP_KILLNZBIT,

	/* Program flow instructions. */
	UFOP_IF,
	UFOP_ELSE,
	UFOP_ENDIF,
	UFOP_CALL,
	UFOP_CALLNZ,
	UFOP_RET,
	UFOP_LOOP,
	UFOP_ENDLOOP,
	UFOP_REP,
	UFOP_ENDREP,
	UFOP_IFC,
	UFOP_BREAK,
	UFOP_BREAKC,
	UFOP_SETP,
	UFOP_LABEL,
	UFOP_IFP,
	UFOP_BREAKP,
	UFOP_CALLP,
	UFOP_GLSLLOOP,
	UFOP_GLSLSTATICLOOP,
	UFOP_GLSLENDLOOP,
	UFOP_GLSLCONTINUE,
	UFOP_GLSLLOOPTAIL,

	/* DX10 program flow instructions. */
	UFOP_BREAKNZBIT,
	UFOP_CALLNZBIT,
	UFOP_RETNZBIT,
	UFOP_CONTINUENZBIT,
	UFOP_IFNZBIT,
	UFOP_SWITCH,
	UFOP_CASE,
	UFOP_DEFAULT,
	UFOP_ENDSWITCH,

	/* Misc. instructions. */
	UFOP_NOP,
	UFOP_MOV,
	UFOP_MOVA_TRC,
	UFOP_MOVA_RNE,
	UFOP_MOVA_INT,
	UFOP_EXPP,
	UFOP_LOGP,

	UFOP_DOT4_CP,
	UFOP_MOV_CP,

	/* DX10 I/U32 2-dest Instructions */
	UFOP_MUL2,
	UFOP_DIV2,
	UFOP_MOVVI,
	UFOP_EMIT,
	UFOP_CUT,
	UFOP_EMITCUT,

	UFOP_SPLIT, 

	UFOP_FTOI,		/* DEST = FLOAT_TO_INTEGER(SRC) */

	/* OpenCL program flow instructions. */
	UFOP_JUMP,
	UFOP_BLOCK,
	
	/* OpenCL Memory load/store instructions. */
	UFOP_MEMLD,
	UFOP_MEMST, 

	/* OpenCL I/U32 2-dest Instructions */
	UFOP_ADD2,
	UFOP_MAD2,

	/* OpenCL texture write instructions */
#if defined(OUTPUT_USPBIN)
	UFOP_TEXWRITE,
#endif

	/* Internally generated instruction. */
	UFOP_CMPLT,			/* DEST = (SRC0 < 0) ? SRC1 : SRC2 */

	/* Mutex control instructions*/
	UFOP_LOCK,
	UFOP_RELEASE,

	/* Atomic instructions*/
	UFOP_ATOM_INC,
	UFOP_ATOM_DEC,
	UFOP_ATOM_ADD,
	UFOP_ATOM_SUB,
	UFOP_ATOM_AND,
	UFOP_ATOM_OR,
	UFOP_ATOM_XOR,
	UFOP_ATOM_XCHG,
	UFOP_ATOM_CMPXCHG,
	UFOP_ATOM_MAX,
	UFOP_ATOM_MIN,

	UFOP_REINTERP,

	UFOP_MAXIMUM,

	UFOP_INVALID = -1
} UF_OPCODE;

#define	UFREG_SWIZ_B					(2)
#define UFREG_SWIZ_G					(1)
#define UFREG_SWIZ_R					(0)
#define UFREG_SWIZ_A					(3)
#define UFREG_SWIZ_1					(4)
#define UFREG_SWIZ_0					(5)
#define UFREG_SWIZ_HALF					(6)
#define UFREG_SWIZ_2					(7)

#define	UFREG_SWIZ_X					(0)
#define UFREG_SWIZ_Y					(1)
#define UFREG_SWIZ_Z					(2)
#define UFREG_SWIZ_W					(3)

#define UFREG_ENCODE_SWIZ(R, G, B, A)	(IMG_UINT16)(((R) << 0) | ((G) << 3) | ((B) << 6) | ((A) << 9))

#define UFREG_SWIZ_NONE					UFREG_SWIZ_RGBA
#define UFREG_SWIZ_RGBA					UFREG_ENCODE_SWIZ(UFREG_SWIZ_R, UFREG_SWIZ_G, UFREG_SWIZ_B, UFREG_SWIZ_A)
#define UFREG_SWIZ_RRRR					UFREG_ENCODE_SWIZ(UFREG_SWIZ_R, UFREG_SWIZ_R, UFREG_SWIZ_R, UFREG_SWIZ_R)
#define UFREG_SWIZ_GGGG					UFREG_ENCODE_SWIZ(UFREG_SWIZ_G, UFREG_SWIZ_G, UFREG_SWIZ_G, UFREG_SWIZ_G)
#define UFREG_SWIZ_BBBB					UFREG_ENCODE_SWIZ(UFREG_SWIZ_B, UFREG_SWIZ_B, UFREG_SWIZ_B, UFREG_SWIZ_B)
#define UFREG_SWIZ_AAAA					UFREG_ENCODE_SWIZ(UFREG_SWIZ_A, UFREG_SWIZ_A, UFREG_SWIZ_A, UFREG_SWIZ_A)
#define UFREG_SWIZ_GBRA					UFREG_ENCODE_SWIZ(UFREG_SWIZ_G, UFREG_SWIZ_B, UFREG_SWIZ_R, UFREG_SWIZ_A)
#define UFREG_SWIZ_BRGA					UFREG_ENCODE_SWIZ(UFREG_SWIZ_B, UFREG_SWIZ_R, UFREG_SWIZ_G, UFREG_SWIZ_A)
#define UFREG_SWIZ_ABGR					UFREG_ENCODE_SWIZ(UFREG_SWIZ_A, UFREG_SWIZ_B, UFREG_SWIZ_G, UFREG_SWIZ_R)
#define UFREG_SWIZ_BAGR					UFREG_ENCODE_SWIZ(UFREG_SWIZ_B, UFREG_SWIZ_A, UFREG_SWIZ_G, UFREG_SWIZ_R)
#define UFREG_SWIZ_RGBB					UFREG_ENCODE_SWIZ(UFREG_SWIZ_R, UFREG_SWIZ_G, UFREG_SWIZ_B, UFREG_SWIZ_B)
#define UFREG_SWIZ_RGAA					UFREG_ENCODE_SWIZ(UFREG_SWIZ_R, UFREG_SWIZ_G, UFREG_SWIZ_A, UFREG_SWIZ_A)
#define UFREG_SWIZ_RG11					UFREG_ENCODE_SWIZ(UFREG_SWIZ_R, UFREG_SWIZ_G, UFREG_SWIZ_1, UFREG_SWIZ_1)
#define UFREG_SWIZ_BA11					UFREG_ENCODE_SWIZ(UFREG_SWIZ_B, UFREG_SWIZ_A, UFREG_SWIZ_1, UFREG_SWIZ_1)
#define UFREG_SWIZ_BGRA					UFREG_ENCODE_SWIZ(UFREG_SWIZ_B, UFREG_SWIZ_G, UFREG_SWIZ_R, UFREG_SWIZ_A)
#define UFREG_SWIZ_RRRA					UFREG_ENCODE_SWIZ(UFREG_SWIZ_R, UFREG_SWIZ_R, UFREG_SWIZ_R, UFREG_SWIZ_A)
#define UFREG_SWIZ_GRBA					UFREG_ENCODE_SWIZ(UFREG_SWIZ_G, UFREG_SWIZ_R, UFREG_SWIZ_B, UFREG_SWIZ_A)

#define UFREG_SWIZ_XYZW					UFREG_SWIZ_NONE
#define UFREG_SWIZ_XXXX					UFREG_SWIZ_RRRR
#define UFREG_SWIZ_YYYY					UFREG_SWIZ_GGGG
#define UFREG_SWIZ_ZZZZ					UFREG_SWIZ_BBBB
#define UFREG_SWIZ_WWWW					UFREG_SWIZ_AAAA

#define UFREG_SOURCE_NEGATE		(0x8U)	/* (-Src) */
#define UFREG_SOURCE_ABS		(0x10U)  /* |Src| */

typedef enum tagUNIFLEX_SRCMOD
{
	UFREG_SMOD_NONE			= 0,
	UFREG_SMOD_COMPLEMENT	= 1,	/* (1-Src) */
	UFREG_SMOD_SIGNED		= 2,	/* (2*(Src-0.5)) */ 
	UFREG_SMOD_BIAS			= 3,	/* (Src-0.5)) */
	UFREG_SMOD_TIMES2		= 4	/* (2*Src) */
} UNIFLEX_SRCMOD;

#define UFREG_SMOD_SHIFT			0
#define UFREG_SMOD_MASK				0x7U

typedef enum tagUNIFLEX_DMODSAT
{
	UFREG_DMOD_SATNONE			= 0,
	UFREG_DMOD_SATZEROONE		= 1,
	UFREG_DMOD_SATNEGONEONE		= 2,
	UFREG_DMOD_SATZEROMAX		= 3
} UNIFLEX_DMODSAT;

#define UFREG_DMOD_SAT_SHIFT		0
#define UFREG_DMOD_SAT_MASK			0xfU

typedef enum tagUNIFLEX_DMODSCALE
{
	UFREG_DMOD_SCALEMUL1		= 0,
	UFREG_DMOD_SCALEMUL2		= 1,
	UFREG_DMOD_SCALEMUL4		= 2,
	UFREG_DMOD_SCALEMUL8		= 3,
	UFREG_DMOD_SCALEDIV2		= 7,
	UFREG_DMOD_SCALEDIV4		= 6,
	UFREG_DMOD_SCALEDIV8		= 5,
	UFREG_DMOD_SCALEDIV16		= 4
} UNIFLEX_DMODSCALE;

#define UFREG_DMOD_SCALE_SHIFT		4U
#define UFREG_DMOD_SCALE_MASK		0xf0U

#define UFREG_DMASK_A_SHIFT			3
#define UFREG_DMASK_R_SHIFT			0
#define UFREG_DMASK_G_SHIFT			1
#define UFREG_DMASK_B_SHIFT			2

#define UFREG_DMASK_X_SHIFT			0
#define UFREG_DMASK_Y_SHIFT			1
#define UFREG_DMASK_Z_SHIFT			2
#define UFREG_DMASK_W_SHIFT			3

typedef enum tagUF_REGTYPE
{
	UFREG_TYPE_TEMP				= 0,
	UFREG_TYPE_COL				= 1,
	UFREG_TYPE_TEXCOORD			= 2,
	UFREG_TYPE_CONST			= 3,
	UFREG_TYPE_TEX				= 4,
	UFREG_TYPE_HW_CONST			= 5,
	UFREG_TYPE_PSOUTPUT			= 6,
	UFREG_TYPE_ICONST			= 7,
	UFREG_TYPE_BCONST			= 8,
	UFREG_TYPE_MISC				= 9,
	UFREG_TYPE_COMPOP			= 10,
	UFREG_TYPE_LABEL			= 11,
	UFREG_TYPE_PREDICATE		= 12,
	UFREG_TYPE_TEXCOORDP		= 13,
	UFREG_TYPE_TEXCOORDPIFTC	= 14,
	UFREG_TYPE_VSINPUT			= 15,
	UFREG_TYPE_VSOUTPUT			= 16,
	UFREG_TYPE_ADDRESS			= 17,
	UFREG_TYPE_INDEXABLETEMP	= 19,
	UFREG_TYPE_CLIPPLANE		= 20,
	UFREG_TYPE_IMMEDIATE		= 21,
	UFREG_TYPE_MAXIMUM			= 22
} UF_REGTYPE;

typedef enum tagUFREG_COMPOP
{
	UFREG_COMPOP_INVALID		= 0,
	UFREG_COMPOP_GT				= 1,
	UFREG_COMPOP_EQ				= 2,
	UFREG_COMPOP_GE				= 3,
	UFREG_COMPOP_LT				= 4,
	UFREG_COMPOP_NE				= 5,
	UFREG_COMPOP_LE				= 6,
	UFREG_COMPOP_NEVER			= 7,
	UFREG_COMPOP_ALWAYS			= 8
} UFREG_COMPOP;

#define UFREG_COMPCHANOP_SHIFT		5
#define UFREG_COMPCHANOP_MASK		0x1e0U

typedef enum tagUFREG_COMPCHANOP
{
	UFREG_COMPCHANOP_NONE       = 0,
	UFREG_COMPCHANOP_ANDALL		= 1,
	UFREG_COMPCHANOP_ORALL		= 2
} UFREG_COMPCHANOP;

typedef enum tagUF_HWCONST
{
	HW_CONST_0					= 0,
	HW_CONST_1					= 1,
	HW_CONST_HALF				= 2,
	HW_CONST_2					= 3
} UF_HWCONST;

typedef enum tagUF_MISC
{
	UF_MISC_POSITION			= 0,
	UF_MISC_FACETYPE			= 1,
	UF_MISC_FACETYPEBIT			= 2,
	UF_MISC_VPRIM				= 3,
	UF_MISC_RTAIDX				= 4,
	UF_MISC_MSAASAMPLEIDX		= 5,
	UF_MISC_FOG				= 255
} UF_MISC;

#define UF_PRED_NONE			(0x00000000)
#define UF_PRED_XYZW			(0x10000000)
#define UF_PRED_X				(0x40000000)
#define UF_PRED_Y				(0x50000000)
#define UF_PRED_Z				(0x60000000)
#define UF_PRED_W				(0x70000000)
#define UF_PRED_NEGFLAG			(0x80000000)
#define UF_PRED_IDX_MASK		(0x0FFFFFFF)
#define UF_PRED_IDX_SHIFT		(0)
#define UF_PRED_COMP_MASK		(0x70000000)
#define UF_PRED_COMP_SHIFT		(28)

/*
	Defines for the source argument to input texture sample instruction containing the
	immediate, texel-space offsets and special return data modes.
*/
#define UF_LDPARAM_OFFSETS_SHIFT					(0)
#define UF_LDPARAM_OFFSETS_CLRMSK					(0xFFFF0000)
#define UF_LDPARAM_OFFSETS_CHAN_WIDTH				(4)
#define UF_LDPARAM_OFFSETS_CHAN_MASK				(0xF)
#define UF_LDPARAM_OFFSETS_CHAN_SIGNBIT				(1 << (UF_LDPARAM_OFFSETS_CHAN_WIDTH - 1))
#define UF_LDPARAM_OFFSETS_RAWGETOFF(PARAM, CHAN)	\
				(((PARAM) >> ((CHAN) * UF_LDPARAM_OFFSETS_CHAN_WIDTH)) & UF_LDPARAM_OFFSETS_CHAN_MASK)
#define UF_LDPARAM_OFFSETS_GETOFF(PARAM, CHAN)		\
				((UF_LDPARAM_OFFSETS_RAWGETOFF(PARAM, CHAN) ^ UF_LDPARAM_OFFSETS_CHAN_SIGNBIT) - UF_LDPARAM_OFFSETS_CHAN_SIGNBIT)

#define UF_LDPARAM_SPECIALRETURN_SHIFT				(16)
#define UF_LDPARAM_SPECIALRETURN_CLRMSK				(0xFFFCFFFF)

/*
	The number of destinations written by a texture sample using the SAMPLEINFO special return mode

								X				Y				Z				W
	First destination:	{Mipmap level,	Trilinear fraction	U fraction,		V fraction}
	Second destination:	{S fraction,	undefined,			undefined,		undefined}
*/
#define UF_LD_SAMPLEINFO_RESULT_REGISTER_COUNT		(2)

/*
	The maximum number of consecutive destination registers written by a texture sample using the
	RAWSAMPLES special return mode. The maximum is for a volume texture using trilinear: 8 samples
	per mip-map level by two mipmap levels.

	The bilinear samples are returned in the order (for trilinear filtering only):-

	{lower mip-map level, upper mip-map level}

	Then within each mipmap level (for volumes only) :-

	{samples in the slice with the smaller depth value, samples in the slice with the larger depth values}

	Then within the samples for each slice/mipmap-level :-

	{top left, top right, bottom-left, bottom right}
*/
#define UF_LD_RAWSAMPLES_MAXIMUM_RESULT_REGISTER_COUNT	(16)

#define UF_LD_MAXIMUM_RESULT_REGISTER_COUNT			(max(UF_LD_SAMPLEINFO_RESULT_REGISTER_COUNT, UF_LD_RAWSAMPLES_MAXIMUM_RESULT_REGISTER_COUNT))

typedef enum _UF_LDPARAM_SPECIALRETURN
{
	UF_LDPARAM_SPECIALRETURN_NONE			= 0,
	UF_LDPARAM_SPECIALRETURN_SAMPLEINFO		= 1,
	UF_LDPARAM_SPECIALRETURN_RAWSAMPLES		= 2
} UF_LDPARAM_SPECIALRETURN, *PUF_LDPARAM_SPECIALRETURN;

#define USC_MAXIMUM_NON_DEPENDENT_TEXTURE_LOADS			(19)
#define USC_MAXIMUM_ITERATIONS							(12)
#define USC_USPBIN_MAXIMUM_PDS_DOUTI_COMMANDS			(USC_MAXIMUM_NON_DEPENDENT_TEXTURE_LOADS + USC_MAXIMUM_ITERATIONS)
#define USC_MAXIMUM_SHADER_PHASES						(3)

/*
	The number of the hardware predicate register that contains the 
	texkill result.
*/
#define USC_OUTPUT_TEXKILL_PRED							(1)

#define UNIFLEX_MAX_OUT_SURFACES						(8)

#define USC_MAXIMUM_INDEXABLE_TEMP_ARRAYS				(4096)

#define USC_FIXED_TEMP_ARRAY_SIZE						(65536)
#define USC_FIXED_TEMP_ARRAY_SHIFT						(16)

#define USC_MAXIMUM_TEXTURE_COORDINATES					(31)

typedef enum tagUF_CONSTBUFFERID
{
	/*
		Constants buffer to use in case of shader with single constants buffer. 
	*/
	UF_CONSTBUFFERID_LEGACY								= 0,
	UF_CONSTBUFFERID_DYNAMIC_CONSTS_0					= UF_CONSTBUFFERID_LEGACY,
	/*
		Constants buffers to use in case of shader with multiple constants buffers.
	*/
	UF_CONSTBUFFERID_TEX_STATE							= 16,
	UF_CONSTBUFFERID_STATIC_CONSTS,
	UF_CONSTBUFFERID_RESOURCE_INFO,
	UF_CONSTBUFFERID_DRIVER_CONSTS,
	/*
		Constants buffers count.
	*/
	UF_CONSTBUFFERID_COUNT
} UF_CONSTBUFFERID;

typedef struct _UNIFLEX_INDEXABLE_TEMP_SIZE
{
	/*
		Identifier for the array.
	*/
	IMG_UINT32						uTag;

	/*
		Size of the array of vector4 registers.
	*/
	IMG_UINT32						uSize;
} UNIFLEX_INDEXABLE_TEMP_SIZE, *PUNIFLEX_INDEXABLE_TEMP_SIZE;

typedef struct _UNIFLEX_RANGE
{
	IMG_UINT32 uRangeStart;
	IMG_UINT32 uRangeEnd;
} UNIFLEX_RANGE, *PUNIFLEX_RANGE;

typedef struct _UNIFLEX_RANGES_LIST
{
	IMG_UINT32		uRangesCount;
	PUNIFLEX_RANGE	psRanges;
} UNIFLEX_RANGES_LIST, *PUNIFLEX_RANGES_LIST;

typedef enum tagUSC_SHADERTYPE
{
	USC_SHADERTYPE_PIXEL,
	USC_SHADERTYPE_VERTEX,
	USC_SHADERTYPE_GEOMETRY,
	USC_SHADERTYPE_COMPUTE
} USC_SHADERTYPE;

typedef enum tagUSC_SHADER_OUTPUTS_COUNT
{	
	USC_MAX_GS_OUTPUTS = 128,
	USC_MAX_SHADER_OUTPUTS = 128,
	USC_SHADER_OUTPUT_MASK_DWORD_COUNT = ((USC_MAX_SHADER_OUTPUTS + 31 )/32)
}USC_SHADER_OUTPUTS_COUNT;

typedef enum tagUF_CONSTBUFFERLOCATION
{
	/* 
    	Default option ?Allow compiler to choose which 
    	constants go where.
	*/
	UF_CONSTBUFFERLOCATION_DONTCARE = 0, 
	/*
		Place entire buffer in secondary attributes 
		Location in secondary attributes to place buffer is provided
		by additional USC input.
	*/
	UF_CONSTBUFFERLOCATION_SAS_ONLY, 
	/* 
		Buffer always in memory.
	*/
	UF_CONSTBUFFERLOCATION_MEMORY_ONLY
} UF_CONSTBUFFERLOCATION;

typedef struct _UNIFLEX_CONSTBUFFERDESC
{
	/* 
		Constant buffer location in SA or memory or dont care.
	*/
	UF_CONSTBUFFERLOCATION			eConstBuffLocation;
	/* 
		Starting SA register if eConstBuffLocation is UF_CONSTBUFFERLOCATION_SAS_ONLY.
	*/
	IMG_UINT32						uStartingSAReg;
	/*
		Count of secondary attribute registers used if eConstBuffLocation is UF_CONSTBUFFERLOCATION_SAS_ONLY.
		Secondary attributes in [uStartingSAReg, uStartingSAReg+uSARegCount) will never appear as destinations
		for constant loads in UNIFLEX_HW.psInRegisterConstMap.
	*/
	IMG_UINT32						uSARegCount;
	/* 
		SA register containing the base address of constant buffer in memory. 
		If it is set to (~0UL) USC will ask the driver to load the base address of buffer into a SA register.
	*/
	IMG_UINT32						uBaseAddressSAReg;
	/*
		Maximum constant relative address (in terms of vec4 registers) for this constant buffer.
	*/
	IMG_UINT32						uRelativeConstantMax;		
	/*
		For this constant buffer the ranges of constants which can be accessed by dynamic indices.
	*/
	UNIFLEX_RANGES_LIST				sConstsBuffRanges;
} UNIFLEX_CONSTBUFFERDESC, *PUNIFLEX_CONSTBUFFERDESC;

typedef enum _UNIFLEX_TEXTURE_STATE_TYPE
{
	UNIFLEX_TEXTURE_STATE_TYPE_HWSTATE,
	UNIFLEX_TEXTURE_STATE_TYPE_STRIDE,
	UNIFLEX_TEXTURE_STATE_TYPE_MAXARRAYINDEX,
	UNIFLEX_TEXTURE_STATE_TYPE_WIDTH,
	UNIFLEX_TEXTURE_STATE_TYPE_HEIGHT,
	UNIFLEX_TEXTURE_STATE_TYPE_DEPTH,
	UNIFLEX_TEXTURE_STATE_TYPE_UNDEFINED
} UNIFLEX_TEXTURE_STATE_TYPE, *PUNIFLEX_TEXTURE_STATE_TYPE;

typedef enum tagUSC_CHANNELFORM
{
	USC_CHANNELFORM_INVALID			= 0,
	USC_CHANNELFORM_U8				= 1,
	USC_CHANNELFORM_S8				= 2,
	USC_CHANNELFORM_U16				= 3,
	USC_CHANNELFORM_S16				= 4,
	USC_CHANNELFORM_F16				= 5,
	USC_CHANNELFORM_F32				= 6,
	USC_CHANNELFORM_ZERO			= 7,
	USC_CHANNELFORM_ONE				= 8,
	USC_CHANNELFORM_U24				= 9,
	USC_CHANNELFORM_U8_UN			= 10,
	USC_CHANNELFORM_C10				= 11,
	USC_CHANNELFORM_UINT8			= 12,
	USC_CHANNELFORM_UINT16			= 13,
	USC_CHANNELFORM_UINT32			= 14,
	USC_CHANNELFORM_SINT8			= 15,
	USC_CHANNELFORM_SINT16			= 16,
	USC_CHANNELFORM_SINT32			= 17,
	USC_CHANNELFORM_UINT16_UINT10	= 18,
	USC_CHANNELFORM_UINT16_UINT2	= 19,
	USC_CHANNELFORM_DF32			= 20
} USC_CHANNELFORM;

typedef struct _UNIFLEX_TEXFORM_
{
	/*
		Format of each channel of the data returned when sampling this texture.
	*/
	USC_CHANNELFORM		peChannelForm[4];	

	/*	
		Swizzle to apply to the result of sampling this texture.
	*/
	IMG_UINT32			uSwizzle;

	/*
		Set if sampling this texture always returns two dwords of results.
	*/
	IMG_BOOL			bTwoResult;

	/*
		Size (in bytes per texel) of the individual planes making up this texture.
	*/
	IMG_UINT32			uChunkSize;

	/*
		Base within the constant buffer of a matrix to use to transform the result of
		sampling this texture.
	*/
	IMG_UINT32			uCSCConst;

	/*
		TRUE if any sample from this texture should use percentage closer filtering.
	*/
	IMG_BOOL			bUsePCF;

	/*
		TRUE if the hardware swaps the red and blue channels when unpacking the result of sampling
		this texture to F16 foramt.
	*/
	IMG_BOOL			bTFSwapRedAndBlue;

	/*
		TRUE if a SMP instruction sampling from this texture requires the MINP pack to be set in the
		instruction.
	*/
	IMG_BOOL			bMinPack;
} UNIFLEX_TEXFORM, *PUNIFLEX_TEXFORM;


/*
	Possible texture filtering modes.
*/
typedef enum _UNIFLEX_TEXTURE_FILTER_TYPE
{
	/*
		Unspecified filtering mode. This can be used where the input shader
		is known not to depend on the filtering mode in use on textures it
		references.
	*/
	UNIFLEX_TEXTURE_FILTER_TYPE_UNSPECIFIED,
	/* Point filtering. */
	UNIFLEX_TEXTURE_FILTER_TYPE_POINT,
	/* Bilinear filtering. */
	UNIFLEX_TEXTURE_FILTER_TYPE_LINEAR,
	/* Anisotropic filtering. */
	UNIFLEX_TEXTURE_FILTER_TYPE_ANISOTROPIC
} UNIFLEX_TEXTURE_FILTER_TYPE, *PUNIFLEX_TEXTURE_FILTER_TYPE;

typedef enum _UNIFLEX_TEXTURE_TRILINEAR_TYPE
{
	/*
		Unspecified trilinear mode.
	*/
	UNIFLEX_TEXTURE_TRILINEAR_TYPE_UNSPECIFIED,
	/* Filtering between mipmap levels is enabled. */
	UNIFLEX_TEXTURE_TRILINEAR_TYPE_ON,
	/* Filtering between mipmap levels is disabled. */
	UNIFLEX_TEXTURE_TRILINEAR_TYPE_OFF
} UNIFLEX_TEXTURE_TRILINEAR_TYPE, *PUNIFLEX_TEXTURE_TRILINEAR_TYPE;

typedef enum _UNIFLEX_DIMENSIONALITY_TYPE
{
	UNIFLEX_DIMENSIONALITY_TYPE_1D = 0,
	UNIFLEX_DIMENSIONALITY_TYPE_2D,
	UNIFLEX_DIMENSIONALITY_TYPE_3D,
	UNIFLEX_DIMENSIONALITY_TYPE_CUBEMAP
} UNIFLEX_DIMENSIONALITY_TYPE, *PUNIFLEX_DIMENSIONALITY_TYPE;

typedef struct _UNIFLEX_DIMENSIONALITY
{
	UNIFLEX_DIMENSIONALITY_TYPE		eType;
	IMG_BOOL						bIsArray;
} UNIFLEX_DIMENSIONALITY, *PUNIFLEX_DIMENSIONALITY;

/*
	Information about a texture used in the input program.
*/
typedef struct _UNIFLEX_TEXTURE_PARAMETERS
{
	/* TRUE if the texture has gamma correction enabled. */
	IMG_BOOL						bGamma;
	/* Describes the format of the data returned by the hardware when sampling the texture. */
	UNIFLEX_TEXFORM					sFormat;
	/* Filtering type when the texture is magnified. */
	UNIFLEX_TEXTURE_FILTER_TYPE		eMagFilter;
	/* Filtering type when the texture is minified. */
	UNIFLEX_TEXTURE_FILTER_TYPE		eMinFilter;
	/* Trilinear mode. */
	UNIFLEX_TEXTURE_TRILINEAR_TYPE	eTrilinearType;
	/* Comparison mode when using PCF on this texture. */
	UFREG_COMPOP					ePCFComparisonMode;
	/* TRUE if the texture is a mipmap. */
	IMG_BOOL						bMipMap;
} UNIFLEX_TEXTURE_PARAMETERS, *PUNIFLEX_TEXTURE_PARAMETERS;

typedef enum tagUF_PREDICATIONLEVEL
{
	UF_PREDLVL_AUTO = 0,			// use default predication level, based on current heuristics
	UF_PREDLVL_SINGLE_INST	= 1,	// basic predication level: only 1 instruction per block
	UF_PREDLVL_LARGE_BLOCKS = 2,	// allow predication of larger blocks
	UF_PREDLVL_LARGE_BLOCKS_MOVC = 3, // also allow conditional moves
	UF_PREDLVL_NESTED = 4,			// allow nested predication
	UF_PREDLVL_LARGE_MOVC = 5,		// allows large numbers of conditional moves
	UF_PREDLVL_NESTED_SAMPLES = 6,	// also allow sample instructions to be in predicated blocks
	UF_PREDLVL_NESTED_SAMPLES_PRED = 7 // allow sample instructions to be predicated (needs help from patcher)
} UF_PREDICATIONLEVEL, *PUF_PREDICATIONLEVEL;

typedef enum tagUF_REGFORMAT
{
	UF_REGFORMAT_F32	 = 0,
	UF_REGFORMAT_F16	 = 1,
	UF_REGFORMAT_C10	 = 2,
	UF_REGFORMAT_U8		 = 3,
	UF_REGFORMAT_I32	 = 4,
	UF_REGFORMAT_U32	 = 5,
	UF_REGFORMAT_I16	 = 6,
	UF_REGFORMAT_U16	 = 7,
	UF_REGFORMAT_UNTYPED = 8,
	UF_REGFORMAT_I8_UN	 = 9,
	UF_REGFORMAT_U8_UN	 = 10,
	UF_REGFORMAT_COUNT	 = 11,

	UF_REGFORMAT_INVALID	= -1
} UF_REGFORMAT, *PUF_REGFORMAT;

typedef struct _UNIFLEX_PROGRAM_PARAMETERS
{
	IMG_UINT32						uFlags;
	IMG_UINT32						uFlags2;
	/* 
		Type of Shader is Pixel Shader or Vertex Shader or Geometry Shader
	*/
	USC_SHADERTYPE					eShaderType;
	/*
		Secondary attribute which holds the base address of the
		constant buffer.
	*/
	IMG_UINT32						uConstantBase;

	/*
		Secondary attribute which holds the base address of the
		scratch area.
	*/
	IMG_UINT32						uScratchBase;

	/*
		Secondary attribute which holds a bitmask of the boolean
		constant.
	*/
	IMG_UINT32						uBooleanConsts;

	/*
		Start of the secondary attributes which hold the value
		of the integer constants.
	*/
	IMG_UINT32						uIntegerConsts;

	/*
		Start of the secondary attributes which hold the texture
		control words.
	*/
	IMG_UINT32						uTextureState;

	/*
		First secondary attribute register available for holding
		frequently used constants.
	*/
	IMG_UINT32						uInRegisterConstantOffset;

	/*
		Count of secondary attribute registers available for holding
		frequently used constants.
	*/
	IMG_UINT32						uInRegisterConstantLimit;

	/*
		Secondary attribute which holds the base address of the memory
		allocated for indexable temporary registers.
	*/
	IMG_UINT32						uIndexableTempBase;

	/*
		Count of the indexable temporary arrays.
	*/
	IMG_UINT32						uIndexableTempArrayCount;

	/*
		Size of each indexable temporary array.
	*/
	PUNIFLEX_INDEXABLE_TEMP_SIZE	psIndexableTempArraySizes;

	/*
		Format of the pixel shader colour result. Can be one of UF_REGFORMAT_F32, UF_REGFORMAT_F16 or
		UF_REGFORMAT_U8.
	*/
	UF_REGFORMAT					ePackDestFormat;

	/*
		Destination for the pixel shader colour result. The type should be one of USEASM_REGTYPE_xxxx.
	*/
	IMG_UINT32						uPackDestType;

	/*
		Controls the maximum precision lost when converting instructions to
		integer.
	*/
	IMG_UINT32						uPackPrecision;

	/*
		Number of instructions to reserve for the caller to insert driver code for feedback
		(either punchthrough or vertex cull) code.
	*/
	IMG_UINT32						uFeedbackInstCount;

	/*
		Number of registers required by extra iterations into the primary attributes.
	*/
	IMG_UINT32						uExtraPARegisters;

	/*
		Number of temporary registers available for the program.
	*/
	IMG_UINT32						uNumAvailableTemporaries;

	/*
		Bitmask of the texture coordinates which have centroid adjust
		enabled.
	*/
	IMG_UINT32						uCentroidMask;

	/*
		Range to saturate each of the pixel shader outputs to in non-packed
		mode.
	*/
	IMG_UINT32						puOutputSaturate[UNIFLEX_MAX_OUT_SURFACES];

	/*
		Target processor for compilation.
	*/
	SGX_CORE_INFO					sTarget;

	/*
		For shader output specify indexable ranges (includes start and end).
	*/
	UNIFLEX_RANGES_LIST				sShaderOutPutRanges;

	/*
		Indexable ranges in the shader input.
	*/
	UNIFLEX_RANGES_LIST				sShaderInputRanges;

	/*
		Bitmask of the Shader outputs that should actually be written.
	*/
	IMG_UINT32						puValidShaderOutputs[USC_SHADER_OUTPUT_MASK_DWORD_COUNT];

	/*
		Bitmask of the Shader outputs that are invariant.
	*/
	IMG_UINT32						puInvariantShaderOutputs[USC_SHADER_OUTPUT_MASK_DWORD_COUNT];

	/*
		Offset of the texture state within the constant buffer (in dwords).
	*/
	IMG_UINT32						uTextureStateConstOffset;

	/*
		Vertex shader output that contains the point size (in terms of scalar registers).
	*/
	IMG_UINT32						uVSPointSizeOutputNum;

	/*
		Optimization level. Higher values correspond to more optimization effort. 
	*/
	IMG_UINT32						uOptimizationLevel;
	
	/*
		The maximum distance that an instruction will be moved when
		scheduling, e.g. during dual issue or efo creation.
	*/
	IMG_UINT32						uMaxInstMovement;

	/* 
		Count of input vertices in geometry shaders. 
	*/
	IMG_UINT32						uInputVerticesCount;	

	/* 
		Count of output buffers for geometry shaders. 
	*/
	IMG_UINT32						uOutPutBuffersCount;

	/*
		Count of texture used in the input program.
	*/
	IMG_UINT32						uTextureCount;

	/* 
		Information about each texture used in the input program.
	*/
	PUNIFLEX_TEXTURE_PARAMETERS		asTextureParameters;

	/*
		Dimensionality of each texture used in the input program.
	*/
	PUNIFLEX_DIMENSIONALITY			asTextureDimensionality;

	/*
		Start of the vertex shader outputs (in terms of scalar registers) which
		contains the diffuse (specular) colour. If -1 then the diffuse (specular)
		colour isn't written by the shader.
	*/
	IMG_UINT32						uVSDiffuseOutputNum;
	IMG_UINT32						uVSSpecularOutputNum;

	/*
		Parameters for each constant buffer.
	*/
	UNIFLEX_CONSTBUFFERDESC			asConstBuffDesc[UF_CONSTBUFFERID_COUNT];

	/*
		Bit array with an entry for each texture coordinate. The corresponding entry is set if
		the texture coordinate is flat shaded.
	*/
	IMG_UINT32						auFlatShadedTextureCoordinates[UNIFLEX_BIT_ARRAY_SIZE(USC_MAXIMUM_TEXTURE_COORDINATES)];

	/*
		Bit array with an entry for each texture coordinate. If the corresponding entry is set then apply
		projection to a texture coordinate used as a source to the UFREG_TYPE_LDPIFTC instruction.
	*/
	IMG_UINT32						auProjectedCoordinateMask[UNIFLEX_BIT_ARRAY_SIZE(USC_MAXIMUM_TEXTURE_COORDINATES)];

	/*
		For pixel-shaders only: the number of constants in the PDS datastore for primary pixel program
		reserved by the driver. The USC will use this to ensure the PDS constants required for iterations
		and non-dependent texture samples don't exceed the available space.
	*/
	IMG_UINT32						uNumPDSPrimaryConstantsReserved;

	/*
		Number of instruction slots to reserve at the entrypoint of the generated hardware code. The caller
		can overwrite the reserved instructions with their own code.
	*/
	IMG_UINT32						uPreFeedbackPhasePreambleCount;

	/*
		For shaders using UF_SPLITFEEDBACK: Number of instruction slots to reserve at the entrypoint of the second phase.
		If the shader wasn't successfully split then no instruction slots are reserved.
	*/
	IMG_UINT32						uPostFeedbackPhasePreambleCount;

	/*
		For shaders using UFOP_SPLIT: Number of instruction slots to reserve at the entrypoint of the second phase.
		If the shader wasn't successfully split then no instruction slots are reserved.
	*/
	IMG_UINT32						uPostSplitPhasePreambleCount;

	/*
		Controls the level of flattening of conditionals using predication
		uMaxALUInstsToFlatten is used if ePredicationLevel != UF_PREDLVL_SINGLE_INST
	*/
	UF_PREDICATIONLEVEL				ePredicationLevel;
	IMG_UINT32						uMaxALUInstsToFlatten;

	/*
		If UF_RUNTIMECORECOUNT is set in the uCompilerFlags parameters to 
		PVRUniFlexCompileToHw/PVRUniFlexCompileToUspBin then the generated shader will only be
		able to run on cores in the range [0..uMPCoreCount).
	*/
	IMG_UINT32						uMPCoreCount;
} UNIFLEX_PROGRAM_PARAMETERS, *PUNIFLEX_PROGRAM_PARAMETERS;

/*
	For UNIFLEX_PROGRAM_PARAMETESR.uMaxInstMovement this value means instruction movement
	is not limited.
*/
#define USC_MAXINSTMOVEMENT_NOLIMIT				(0xFFFFFFFF)

/*
	For UNIFLEX_PROGRAM_PARAMETERS.uOptimizationLevel this value means all optimizations
	enabled.
*/
#define USC_OPTIMIZATIONLEVEL_MAXIMUM				(0xFFFFFFFF)

typedef enum tagUFREG_RELATIVEINDEX
{
	UFREG_RELATIVEINDEX_NONE	= 0,
	UFREG_RELATIVEINDEX_A0X		= 1,
	UFREG_RELATIVEINDEX_A0Y		= 2,
	UFREG_RELATIVEINDEX_A0Z		= 3,
	UFREG_RELATIVEINDEX_A0W		= 4,
	UFREG_RELATIVEINDEX_AL		= 5,
	UFREG_RELATIVEINDEX_TEMP_REG	= 6
} UFREG_RELATIVEINDEX;

typedef struct _UF_REGISTER_
{
	IMG_UINT32				uNum;	
	UF_REGTYPE				eType;
	UF_REGFORMAT			eFormat;
	union
	{
		IMG_UINT16			uSwiz;			/* For source regs */
		IMG_BYTE			byMask;			/* For dest regs */
	} u;

	IMG_BYTE				byMod;
	UFREG_RELATIVEINDEX		eRelativeIndex;
	UF_REGTYPE				eRelativeType;
	IMG_UINT32				uRelativeNum;
	IMG_BYTE				byRelativeChan;
	IMG_UINT32				uArrayTag;
	union
	{
		IMG_UINT32			uRelativeStrideInComponents;
		IMG_UINT32			uRelativeStrideInBytes;
	} u1;
} UF_REGISTER, *PUF_REGISTER;

#define UFREG_OUTPUT_MC0	0
#define UFREG_OUTPUT_MC1	1
#define UFREG_OUTPUT_MC2	2
#define UFREG_OUTPUT_MC3	3
#define UFREG_OUTPUT_MC4	4
#define UFREG_OUTPUT_MC5	5
#define UFREG_OUTPUT_MC6	6
#define UFREG_OUTPUT_MC7	7
#define UFREG_OUTPUT_Z		8
#define UFREG_OUTPUT_OMASK	9

#define UNIFLEX_MAXIMUM_SOURCE_ARGUMENT_COUNT	(6)

typedef struct _UNIFLEX_INST_
{
	UF_OPCODE		eOpCode;
	UF_REGISTER		sDest;
	UF_REGISTER		sDest2;			/* 2nd result, for DX10 instrs - 17/1/08, I/U32 MUL/DIV/ADD/MAD/MIN/MAX */
	UF_REGISTER		asSrc[UNIFLEX_MAXIMUM_SOURCE_ARGUMENT_COUNT];		/* for ld this specifies which coordinate set to use */
	IMG_UINT32		uPredicate;		/* Predication type. */
	struct _UNIFLEX_INST_	*psILink;
	struct _UNIFLEX_INST_	*psBLink;	/* For compiler use. */
	
	#ifdef SRC_DEBUG
	IMG_UINT32		uSrcLine;
	#endif /* SRC_DEBUG */
} UNIFLEX_INST, *PUNIFLEX_INST;

#ifdef SRC_DEBUG
	#define UNDEFINED_SOURCE_LINE	(IMG_UINT32)(-1)
	#define USC_GENERATED_SOURCE_LINE	((psState->uTotalLines)) 
#endif /* SRC_DEBUG */

typedef struct _UNIFLEX_CONSTDEF_
{
	IMG_UINT32	uCount;				/* Number of constants in psConst array. */
	IMG_PUINT32	puConstStaticFlags;	/* Flag for channel of each constant indicating if it is static. */
	IMG_PFLOAT	pfConst;			/* Static value for each channel of each channel if it exists. */
} UNIFLEX_CONSTDEF, *PUNIFLEX_CONSTDEF;

typedef enum tagUNIFLEX_ITERATION_TYPE
{
	UNIFLEX_ITERATION_TYPE_TEXTURE_COORDINATE	= 0,
	UNIFLEX_ITERATION_TYPE_COLOUR				= 1,
	UNIFLEX_ITERATION_TYPE_FOG					= 2,
	UNIFLEX_ITERATION_TYPE_POSITION				= 3,
	UNIFLEX_ITERATION_TYPE_VPRIM				= 4,
	UNIFLEX_ITERATION_TYPE_MAX					= 5
} UNIFLEX_ITERATION_TYPE;

/*
	Iterated data formats, to indicate how input data iterated by the
	HW should be packed for use by the compiled program
*/
typedef enum tagUNIFLEX_TEXLOAD_FORMAT
{
	UNIFLEX_TEXLOAD_FORMAT_F32	= 0,
	UNIFLEX_TEXLOAD_FORMAT_F16	= 1,
	UNIFLEX_TEXLOAD_FORMAT_C10	= 2,
	UNIFLEX_TEXLOAD_FORMAT_U8	= 3,	/* supercedes bIntegerColour */

	UNIFLEX_TEXLOAD_FORMAT_UNDEF = -1L
} UNIFLEX_TEXLOAD_FORMAT, *PUNIFLEX_TEXLOAD_FORMAT;

/*
	Data describing the texture or vertex data to be sampled or iterated
	by the HW to form input data for the compiled program
*/
typedef struct tagUNIFLEX_TEXTURE_LOAD
{
	IMG_UINT32					uTexture;
	IMG_UINT32					uChunk;
	UNIFLEX_ITERATION_TYPE		eIterationType;
	IMG_UINT32					uCoordinate;
	IMG_UINT32					uCoordinateDimension;
	IMG_BOOL					bProjected;
	UNIFLEX_TEXLOAD_FORMAT		eFormat;
	IMG_BOOL					bCentroid;
	IMG_UINT32					uNumAttributes;
} UNIFLEX_TEXTURE_LOAD, *PUNIFLEX_TEXTURE_LOAD;

/*
	Index used to indicate an iterated rather than pre-sampled 
	UNIFLEX_TEXTURE_LOAD entry.
*/
#define UNIFLEX_TEXTURE_NONE	(UINT_MAX)

/*
	Texture unpacking formats.
*/
typedef enum tagUNIFLEX_TEXTURE_UNPACK_FORMAT
{
	UNIFLEX_TEXTURE_UNPACK_FORMAT_NATIVE	= 0,
	UNIFLEX_TEXTURE_UNPACK_FORMAT_C10		= 1,
	UNIFLEX_TEXTURE_UNPACK_FORMAT_F16		= 2,
	UNIFLEX_TEXTURE_UNPACK_FORMAT_F32		= 3
} UNIFLEX_TEXTURE_UNPACK_FORMAT, *PUNIFLEX_TEXTURE_UNPACK_FORMAT;

typedef struct tagUNIFLEX_TEXTURE_UNPACK
{
	/*
		Format to unpack to.
	*/
	UNIFLEX_TEXTURE_UNPACK_FORMAT			eFormat;
	/*
		Should the unpacked data be normalised?
	*/
	IMG_BOOL								bNormalise;
} UNIFLEX_TEXTURE_UNPACK, *PUNIFLEX_TEXTURE_UNPACK;

/*
	Constant data formats, to indicate how the data for a constant
	should be stored for use by the compiled code
*/
typedef enum tagUNIFLEX_CONST_FORMAT
{
	UNIFLEX_CONST_FORMAT_F32		= 0,
	UNIFLEX_CONST_FORMAT_F16		= 1,
	UNIFLEX_CONST_FORMAT_C10		= 2,
	UNIFLEX_CONST_FORMAT_U8		    = 3,
	UNIFLEX_CONST_FORMAT_STATIC		= 4,
	UNIFLEX_CONST_FORMAT_CONSTS_BUFFER_BASE	= 5,
	UNIFLEX_CONST_FORMAT_RTA_IDX	= 6,
	UNIFLEX_CONST_FORMAT_UNDEF		= 7
} UNIFLEX_CONST_FORMAT, *PUNIFLEX_CONST_FORMAT;

/*
	Data representing how a constant-component should be loaded for use by
	the compiled program 
*/
typedef struct tagUNIFLEX_CONST_LOAD
{
	union
	{
		struct
		{
			// index of the original (32-bit sized) constant
			IMG_UINT16				uSrcIdx;
	
			// shift within the destination location for formats <32-bits
			IMG_UINT16				uDestShift;

			// constants buffer to which constant belongs.
			IMG_UINT32				uConstsBuffNum;

			// shift within the source location for formats <32-bits
			IMG_UINT32				uSrcShift;
		} s;
		IMG_UINT32					uValue;
	} u;

	// index of the (32-bit size) location where the constant should be loaded
	IMG_UINT16				uDestIdx;

	// format in which the constant should be loaded
	UNIFLEX_CONST_FORMAT	eFormat;
} UNIFLEX_CONST_LOAD, *PUNIFLEX_CONST_LOAD;

/*
	Set if the program contains KPLT or KPLE instructions.
*/
#define UNIFLEX_HW_FLAGS_TEXKILL_USED				(0x00000001)
/*
	Set if either the last instruction in the main program can't have the end flag set on it
	OR there is a branch to the instruction after the last instruction.
*/
#define UNIFLEX_HW_FLAGS_LABEL_AT_END				(0x00000002)
/*
	Set if the program requires per-instance mode.
*/
#define UNIFLEX_HW_FLAGS_PER_INSTANCE_MODE			(0x00000004)
/*
	Set if the program contains writes to depth.
*/
#define UNIFLEX_HW_FLAGS_DEPTHFEEDBACK				(0x00000008)
/*
	Set if the program contains writes to oMask.
*/
#define UNIFLEX_HW_FLAGS_OMASKFEEDBACK				(0x00000010)
/*
	Set if the program contains texture reads (either non-dependent or dependent).
*/
#define UNIFLEX_HW_FLAGS_TEXTUREREADS				(0x00000020)

/*
	Set if either the last instruction in the secondary update program can't have the end flag set on it
	OR there is a branch to the instruction after the last instruction.
*/
#define UNIFLEX_HW_FLAGS_SAPROG_LABEL_AT_END		(0x00000040)
/*
	Set if the program contains memory reads from the primary constant buffer.
*/
#define UNIFLEX_HW_FLAGS_PRIMARYCONSTLOADS			(0x00000080)
/*
	Set if the program uses absolute branches.
*/
#define UNIFLEX_HW_FLAGS_ABS_BRANCHES_USED			(0x00000100)

#ifdef SRC_DEBUG
typedef struct _UNIFLEX_SOURCE_LINE_COST
{
	/*
		Source line number for which this entry is created.
	*/
	IMG_UINT32 uSrcLine;

	/*
		Cycle cost for this source line number.
	*/
	IMG_UINT32 uCost;
}UNIFLEX_SOURCE_LINE_COST, *PUNIFLEX_SOURCE_LINE_COST;

typedef struct _UNIFLEX_SOURCE_CYCLE_COUNT
{
	/*
		Total number of entries in table.
	*/
	IMG_UINT32				uTableCount;

	/*
		Total number of cycles used.
	*/
	IMG_UINT32				uTotalCycleCount;


	/*
		List to maintain source line number and respective cost.
	*/
	PUNIFLEX_SOURCE_LINE_COST	psSourceLineCost;
}UNIFLEX_SOURCE_CYCLE_COUNT, *PUNIFLEX_SOURCE_CYCLE_COUNT;

#endif /* SRC_DEBUG */

#if defined(UF_TESTBENCH)
/*
	Describes how an indexable temporary array has been stored.
*/
typedef struct _TEMP_ARRAY_LOCATION
{
	/*
		Format of the data in the array.
	*/
	UF_REGFORMAT			eFormat;
	/*
		Set to TRUE if the array is stored in temporary register.
	*/
	IMG_BOOL				bInRegisters;
	/*
		First temporary register used for the array.
	*/
	IMG_UINT32				uStart;
	/*
		First temporary register after those used for the array.
	*/
	IMG_UINT32				uEnd;
} TEMP_ARRAY_LOCATION, *PTEMP_ARRAY_LOCATION;
#endif /* defined(UF_TESTBENCH) */

typedef enum tagUNIFLEX_PHASE_RATE
{
	UNIFLEX_PHASE_RATE_PIXEL		= 0,
	UNIFLEX_PHASE_RATE_SAMPLE		= 1
} UNIFLEX_PHASE_RATE, *PUNIFLEX_PHASE_RATE;

typedef enum tagUNIFLEX_PHASE_EXECND
{
	UNIFLEX_PHASE_EXECND_NONE		= 0,
	UNIFLEX_PHASE_EXECND_FEEDBACK	= 1
} UNIFLEX_PHASE_EXECND, *PUNIFLEX_PHASE_EXECND;

typedef struct _UNIFLEX_PHASE_INFO
{
	UNIFLEX_PHASE_EXECND	ePhaseExeCnd;	
	UNIFLEX_PHASE_RATE		ePhaseRate;
	/*
		Offset of the phase start (in instructions).
	*/
	IMG_UINT32				uPhaseStart;
	IMG_UINT32				uTemporaryRegisterCount;
} UNIFLEX_PHASE_INFO, *PUNIFLEX_PHASE_INFO;

typedef struct _UNIFLEX_HW
{
	/*
		Contains flags with information about the program. See UNIFLEX_HW_FLAGS_* flags above.
	*/
	IMG_UINT32				uFlags;

	/*
		Count of instructions in program.
	*/
	IMG_UINT32				uInstructionCount;

	/*
		Size of the array pointed to by puInstructions.
	*/
	IMG_UINT32				uMaximumInstructionCount;

	/*
		Array of the instructions generated for the program.
	*/
	IMG_PUINT32				puInstructions;

	/*
		Count of instructions in the secondary update program.
	*/
	IMG_UINT32				uSAProgInstructionCount;

	/*
		Size of the array pointed to by puSAProgInstructions.
	*/
	IMG_UINT32				uSAProgMaximumInstructionCount;

	/*
		Array of the instructions generated for the secondary load program.
	*/
	IMG_PUINT32				puSAProgInstructions;

	/*
		Count of the primary attribute registers used in the program.
	*/
	IMG_UINT32				uPrimaryAttributeCount;

	/*
		Count of the temporary registers used in the secondary update program.
	*/
	IMG_UINT32				uSecTemporaryRegisterCount;

	/*
		Count of the output registers used by the program.
	*/
	IMG_UINT32				uOutputRegisterCount;

	/*
		Number of non-dependent texture loads and coordinate iterations required for the program.
	*/
	IMG_UINT32				uNrNonDependentTextureLoads;

	/*
		Details about each non-dependent texture load/coordinate iteration.
	*/
	PUNIFLEX_TEXTURE_LOAD	psNonDependentTextureLoads;

	/* 
		To store memory remapping information for one constant buffer. 
	*/
	struct
	{
		/*
			Current maximum size of the in-memory constant remapping table.
		*/
		IMG_UINT32				uMaximumConstCount;

		/*
			If constant remapping is enabled then the number of constants used in the program.
		*/
		IMG_UINT32				uConstCount;

		/*
			If constant remapping is enabled then a table mapping constant numbers in the output program
			to the constant registers and channels in the input program.
		*/
		PUNIFLEX_CONST_LOAD		psConstMap;
	}psMemRemappingForConstsBuff[UF_CONSTBUFFERID_COUNT];

	/*
		Current maximum size of the in-register constant remapping table.
	*/
	IMG_UINT32				uMaximumInRegisterConstCount;

	/*
		The number of constants which have been moved to secondary attributes.
	*/
	IMG_UINT32				uInRegisterConstCount;

	/*
		For each secondary attribute registers starting at uInRegisterConstantOffset the
		constant register number and channel which have been moved into it.
	*/
	PUNIFLEX_CONST_LOAD		psInRegisterConstMap;

	/*
		The total size of the indexable temporary storage required (in bytes).
	*/
	IMG_UINT32				uIndexableTempTotalSize;

	/*
		The number of bytes difference between the address of a memory buffer used by the
		program and the address stored in the secondary attribute.
	*/
	IMG_INT32				iSAAddressAdjust;

	/*
		The offset of the end of the phase before feedback in instructions. 
	*/
	IMG_UINT32				uFeedbackPhase0End;

	/*
		Points to the secondary attribute offsets used in the PVRUniFlexCompileToHw call.
	*/
	PUNIFLEX_PROGRAM_PARAMETERS psProgramParameters;

	/*
		Primary attribute register number where the output is produced.
	*/
	IMG_UINT32				uPAOutputRegNum;

	/*
		Texture stages that should have anisotropic filtering disabled.
	*/
	IMG_PUINT32				auNonAnisoTextureStages;

	/*
		Size in bytes of the spill area required for all instances/pipelines.
	*/
	IMG_UINT32				uSpillAreaSize;

	/*
		Overall number of secondary attributes used for constants (including those
		calculated in the secondary update program).
	*/
	IMG_UINT32				uConstSecAttrCount;

	/*
		Size of texture state on this core.
	*/
	IMG_UINT32				uTextureStateSize;

	/*
		For vertex-shaders: Bitmask of PA-regs used for input-data

		NB: Bit X of Nybble Y corresponds to component X of VS input Y.
	*/
	IMG_UINT32				auVSInputsUsed[UF_MAX_VERTEX_SHADER_INPUTS*4/32];

	/*
		Desired unpacking format for each texture.
	*/
	PUNIFLEX_TEXTURE_UNPACK	asTextureUnpackFormat;

	#ifdef SRC_DEBUG
	/*
		Table to maintain source line number and respective cost.
	*/
	UNIFLEX_SOURCE_CYCLE_COUNT   asTableSourceCycle;
	#endif /* SRC_DEBUG */
	
	/*
		Bitmask of the Shader Outputs & Ranges that are valid after the compilation of program.
	*/
	IMG_UINT32				auValidShaderOutputs[USC_SHADER_OUTPUT_MASK_DWORD_COUNT];
	UNIFLEX_PHASE_INFO		asPhaseInfo[USC_MAXIMUM_SHADER_PHASES];
	IMG_UINT32				uPhaseCount;
} UNIFLEX_HW, *PUNIFLEX_HW;

#include "verif.h"

/*
	Flags for PVRUniFlexCompileToHw/PVRUniFlexCompileToUspBin uFlags parameter.
*/
#define UF_QUIET						(0x00000001U)
#define UF_NOCONSTREMAP					(0x00000002U)
#define UF_GLSL							(0x00000004U)
#define UF_CONSTRANGES					(0x00000008U)
#define UF_FIXEDTEMPARRAYSTRIDE			(0x00000010U)
#define UF_SPLITFEEDBACK				(0x00000020U)
#define UF_MSAATRANS					(0x00000040U)
#define UF_ATLEASTONELOOP				(0x00000080U)
#define UF_EXTRACTCONSTANTCALCS			(0x00000100U)
#define UF_REDIRECTVSOUTPUTS			(0x00000200U)
#define UF_DONTRESETMOEAFTERPROGRAM		(0x00000400U)
#define UF_NOALPHATEST					(0x00000800U)
#define UF_ENABLERANGECHECK				(0x00001000U)
#define UF_VSOUTPUTSALWAYSVALID			(0x00002000U)
#define UF_MULTIPLECONSTANTSBUFFERS		(0x00004000U)
#define UF_SMPIMMCOORDOFFSETS			(0x00008000U)	/* Sampling instructions can have immediate coordinate offsets. */
#define UF_CLAMPVSCOLOUROUTPUTS			(0x00010000U)
#define UF_RESTRICTOPTIMIZATIONS        (0x00020000U)
#define UF_CONSTEXPLICTADDRESSMODE		(0x00040000U)
#define UF_INITREGSONFIRSTWRITE			(0x00080000U)
#define UF_NONONDEPENDENTSAMPLES		(0x00100000U)	/* Never generate non-dependent texture samples. */
#define UF_SGX540_VECTORISE_F16			(0x00200000U)
/*
	If set then the range of cores on which the generated shader will run comes from
	UNIFLEX_PROGRAM_PARAMETERS.uMPCoreCount. Otherwise the generated shader will be able
	to run on all the cores in the system.
*/
#define UF_RUNTIMECORECOUNT				(0x00400000U)

/*
  	If set enables OpenCL specific input instructions and behavior within the compiler.
 */
#define UF_OPENCL						(0x00800000U)

#define UF_ICODE_UNSERIALIZE			(0x01000000U)
#define UF_ICODE_SERIALIZE				(0x02000000U)

#if defined(TRACK_REDUNDANT_PCONVERSION)
#define UF_FORCE_C10_TO_F16_PRECISION	(0x04000000U)    /* Force convert c10 to f16 precision */
#define UF_FORCE_F16_TO_F32_PRECISION	(0x08000000U)    /* Force convert f16 to f32 precision */
#define UF_FORCE_C10_TO_F32_PRECISION	(0x10000000U)    /* Force convert c10 to f32 precision */
#define UF_FORCE_F32_PRECISION			(UF_FORCE_F16_TO_F32_PRECISION | UF_FORCE_C10_TO_F32_PRECISION)
														/* Force convert c10 & f16 to f32 precision */
#define UF_FORCE_PRECISION_MASK			(UF_FORCE_C10_TO_F16_PRECISION | UF_FORCE_F16_TO_F32_PRECISION | UF_FORCE_C10_TO_F32_PRECISION)
#define UF_FORCE_PRECISION_CONVERT_ENABLE				\
										(0x20000000U)	/* Enable precision conversion */
#endif /* defined(TRACK_REDUNDANT_PCONVERSION) */

#define UF_FLAGS2_EXECPRED				(0x00000001U)

#define USC_MAIN_LABEL_NUM				(UINT_MAX)

typedef enum
{
	USC_METRICS_INTERMEDIATE_CODE_GENERATION=0,

	USC_METRICS_VALUE_NUMBERING=1,

	USC_METRICS_ASSIGN_INDEX_REGISTERS=2,

	USC_METRICS_FLATTEN_CONDITIONALS_AND_ELIMINATE_MOVES=3,

	USC_METRICS_INTEGER_OPTIMIZATIONS=4,

	USC_METRICS_MERGE_IDENTICAL_INSTRUCTIONS=5,

	USC_METRICS_INSTRUCTION_SELECTION=6,

	USC_METRICS_REGISTER_ALLOCATION=7,

	USC_METRICS_C10_REGISTER_ALLOCATION=8,

	USC_METRICS_FINALISE_SHADER=9,
	
	USC_METRICS_TOTAL_COMPILE_TIME = 10,

	USC_METRICS_CUSTOM_TIMER_A=11,

	USC_METRICS_CUSTOM_TIMER_B=12,

	USC_METRICS_CUSTOM_TIMER_C=13,

	USC_METRICS_LAST =14 /* Must always be the latest one */

} USC_METRICS;

typedef IMG_PVOID (IMG_CALLCONV *USC_ALLOCFN)(IMG_UINT32 uSize);
typedef IMG_VOID (IMG_CALLCONV *USC_FREEFN)(IMG_PVOID pvData);
typedef IMG_VOID (IMG_CALLCONV *USC_PRINTFN)(const IMG_CHAR* pszFormat, ...) IMG_FORMAT_PRINTF(1, 2);
typedef IMG_VOID (IMG_CALLCONV *USC_PDUMPFN)(IMG_PVOID pvDrvParam, const IMG_CHAR* pszFormat, ...) IMG_FORMAT_PRINTF(2, 3);
typedef IMG_VOID (IMG_CALLCONV *USC_STARTFN)(IMG_PVOID pvDrvParam, USC_METRICS eCounter);
typedef IMG_VOID (IMG_CALLCONV *USC_FINISHFN)(IMG_PVOID pvDrvParam, USC_METRICS eCounter);

USC_IMPORT
IMG_PVOID IMG_CALLCONV PVRUniFlexCreateContext(USC_ALLOCFN	pfnAlloc,
											   USC_FREEFN	pfnFree,
											   USC_PRINTFN	pfnPrint,
											   USC_PDUMPFN	pfnPDump,
											   IMG_PVOID	pvPDumpFnDrvParam,
											   USC_STARTFN  pfnStart,
											   USC_FINISHFN pfnFinish,
											   IMG_PVOID    pvMetricsFnDrvParam);

/*
  PVRUniFlexInitInst: Initialise a uniflex instruction.
 */
USC_IMPORT
IMG_VOID IMG_CALLCONV PVRUniFlexInitInst(IMG_PVOID pvContext, PUNIFLEX_INST psInst);

/*
  PVRUniFlexInitTextureParameters: Initialise a uniflex texture format description.
 */
USC_IMPORT
IMG_VOID IMG_CALLCONV PVRUniFlexInitTextureParameters(PUNIFLEX_TEXTURE_PARAMETERS	psTexParams);

/*
	PVRUniFlexUnpackTextureStateConstantOffset: Unpack an offset into the texture state constant buffer.
*/
USC_IMPORT_OPENGL
IMG_VOID IMG_CALLCONV PVRUniFlexUnpackTextureStateConstantOffset(IMG_UINT32						uHwTextureStateSize,
																 IMG_UINT32						uOffset,
																 PUNIFLEX_TEXTURE_STATE_TYPE	peType,
																 IMG_PUINT32					puSampler,
																 IMG_PUINT32					puChunk,
																 IMG_PUINT32					puHwStateWord);

#if defined(OUTPUT_USCHW)
/*
	Generate a HW executable program (with associated meta-data), suitable
	for runtime compilation ithin a driver.
*/
USC_IMPORT
IMG_UINT32 IMG_CALLCONV PVRUniFlexCompileToHw(IMG_PVOID							pvContext,
											  PUNIFLEX_INST						psSWProc,
											  PUNIFLEX_CONSTDEF					psConstants,
											  PUNIFLEX_PROGRAM_PARAMETERS		psProgramParameters,
											  #if defined(UF_TESTBENCH)
											  PUNIFLEX_VERIF_HELPER				psVerifHelper,
											  #endif /* defined(UF_TESTBENCH) */
											  PUNIFLEX_HW						psHw);

/*
	Cleanup and reset a UNIFLEX_HW structure, freeing any associated
	dynamically allocated memory.
*/
USC_IMPORT
IMG_VOID IMG_CALLCONV PVRCleanupUniflexHw(IMG_PVOID	pvContext, PUNIFLEX_HW psHw);
#endif /* #if defined(OUTPUT_USCHW) */

#if defined(OUTPUT_USPBIN)

/* Forward-declare PUSP_PC_SHADER (with a different name so we don't get redeclaration errors */
typedef struct _USP_PC_SHADER_ *PUSC_USP_PC_SHADER; 

/*
	Generate USP compatible pre-compiled binary shader data, suitable for offline
	compilation and subsequent runtime finalisation by the USP
*/
USC_IMPORT
IMG_UINT32 IMG_CALLCONV PVRUniFlexCompileToUspBin(IMG_PVOID							pvContext,
												  IMG_UINT32						*pui32Flags,
												  PUNIFLEX_INST						psSWProc,
												  PUNIFLEX_CONSTDEF					psConstants,
												  PUNIFLEX_PROGRAM_PARAMETERS		psProgramParameters,
												  #if defined(UF_TESTBENCH)
												  PUNIFLEX_VERIF_HELPER				psVerifHelper,
												  #endif /* defined(UF_TESTBENCH) */
												  PUSC_USP_PC_SHADER*				ppsPCShader
												  #ifdef SRC_DEBUG
												  , PUNIFLEX_SOURCE_CYCLE_COUNT		psTableSourceCycle
												  #endif /* SRC_DEBUG */
												  );

/*
	Destroy previously generated USP binary-shader data, created using
	PVRUniFlexCompileToUspBin.
*/
USC_IMPORT
IMG_VOID IMG_CALLCONV PVRUniFlexDestroyUspBin(IMG_PVOID				pvContext,
											  PUSC_USP_PC_SHADER 	psPCShader);
#endif /* #if defined(OUTPUT_USPBIN) */

USC_IMPORT
IMG_VOID IMG_CALLCONV PVRUniFlexDestroyContext(IMG_PVOID pvContext);

#if defined(DEBUG)
/*
	Decode an individual uniflex input instruction.
*/
USC_IMPORT
IMG_VOID PVRUniFlexDecodeInputInst(PUNIFLEX_INST	psInst, 
								   IMG_INT32		i32Indent,
								   IMG_PCHAR		pszInst,
								   IMG_PUINT32		puTexUseMask);

USC_IMPORT
IMG_PCHAR PVRUniFlexDecodeIteration(PUNIFLEX_TEXTURE_LOAD	psIteration,
									IMG_PCHAR				pszIteration);
#endif /* #if defined(DEBUG) */

/*
	Table describing the USC input instructions.
*/
typedef struct _INPUT_INST_DESC
{
	IMG_UINT32	uNumDests;
	IMG_UINT32	uNumSrcArgs;
	IMG_CHAR const*	pchName;
} INPUT_INST_DESC, *PINPUT_INST_DESC;
typedef INPUT_INST_DESC const* PCINPUT_INST_DESC;

/*
	Get information about the properties of an input instruction.
*/
USC_IMPORT
PCINPUT_INST_DESC PVRUniFlexGetInputInstDesc(UF_OPCODE	eOpcode);

/* TODO put somewhere appropriate */
#ifndef USE_SYNCEND_ON_NOT_BRANCH
#define USE_SYNCEND_ON_NOT_BRANCH 0xDEADDEAD
#endif

/* Memory access instruction flags */
#define UF_MEM_NOALIAS (1U << 0)
#define UF_MEM_CONST (1U << 1)
#define UF_MEM_LOCAL (1U << 2)
#define UF_MEM_GLOBAL (1U << 3)

/* the default predication level */
#define DEFAULT_PREDLVL UF_PREDLVL_AUTO

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* ifndef _USC_H_ */

/******************************************************************************
 End of file (USC.H)
******************************************************************************/
