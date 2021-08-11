/******************************************************************************
 * Name         : icode.h
 * Created      : 24/06/2004
 *
 * Copyright    : 2004-2008 by Imagination Technologies Limited.
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
 * Platform     : ANSI
 *
 * Modifications:-
 * $Log: icode.h $
 *****************************************************************************/

#ifndef _icode_h_
#define _icode_h_

#include "errorlog.h"

/*
	Opcode					Operand Rules

	GLSLIC_OP_MOV			If source is one component, dest can be ANY type, 
							else if destination is a matrix, dest can be ANY type of matrix.
							else the number of for source and dest (after swizzle/write-mask) must match

	GLSLIC_OP_ADD			Dest and both sources must be the same type and size, or
	GLSLIC_OP_SUB			one source only can be a scalar-type, but must be the same
	GLSLIC_OP_DIV			data-format.

	GLSLIC_OP_MUL			1) Rules for ADD/SUB/DIV apply (except matrix by matrix)
							2) Special rules apply for matrix by matrix, vector by matrix and 
							   matrix by vector.

	GLSLIC_OP_SLT			Sources can be scaler or vector, but both must be the same type specifier, 
	GLSLIC_OP_SLE			or one source can be a scalar-type, the other can be a vector, but 
	GLSLIC_OP_SGT			must be the same data-format. Boolean sources are not allowed. 
	GLSLIC_OP_SGE			Dest must be a boolean vector of the same dimension		
							as the vector source if one of source is a vector, other wise dest
							is a boolean scaler 

	GLSLIC_OP_NOT			Both source and destination can be boolean scaler or vectors, 
							both have some number of components,
	
	GLSLIC_OP_SEQ			If dest is a scaler boolean , sources can be any type(scaler, vector,
	GLSLIC_OP_SNE			matrix, structure), but both sources must be the same type and size 
							(same type specifier and iarraysize), otherwise, 
							rules for SLT/SLE/SGT/SGE apply.

	GLSLIC_OP_IF			Source must be a scalar boolean.

	GLSLIC_OP_SANY			Source must be a booean vector and dest must be a scalar boolean
	GLSLIC_OP_SALL

	GLSLIC_OP_ABS			Source and dest must be same size, can be float, vec2, vec3 or vec4

	GLSLIC_OP_MIN			Both sources and dest must be same type specifier, can be float, vec2, vec3 or vec4
	GLSLIC_OP_MAX			Or one of source can be a scaler float, but other source and dest
							must be same size, one of float, vec2, vec3 and vec4

	GLSLIC_OP_SGN			Source and dest must be same type specifier, can be float, vec2, vec3 or vec4
	GLSLIC_OP_RCP
	GLSLIC_OP_RSQ
	GLSLIC_OP_LOG2
	GLSLIC_OP_EXP2

	GLSLIC_OP_DOT			Both source must be same type specifier, can be float, vec2, vec3 or vec4
							Dest much be scalar float

	GLSLIC_OP_CROSS			Both sources and dest must be vec3	

	GLSLIC_OP_SIN			Source and dest must be same type specifier, can be float, vec2, vec3 or vec4
	GLSLIC_OP_COS
	GLSLIC_OP_FLOOR	
	GLSLIC_OP_CEIL
	GLSLIC_OP_DFDX
	GLSLIC_OP_DFDY

	GLSLIC_OP_TEXLD
	GLSLIC_OP_TEXLDB
	GLSLIC_OP_TEXLDL
	GLSLIC_OP_TEXLDD
	GLSLIC_OP_TEXLDP		SrcA - One of sampler1D, sampler2D, sampler3D, samplerCube, sampler1DShadow, sampler2DShadow, samplerStreamIMG
							SrcB - Texture coord used to look up texture. The last component is used to perform 
								   projective division.
							SrcC - Bias or lod, not needed for TEXLD and TEXLDP, dPdx for LDD
							SrcD - dPdy for LDD


	GLSLIC_OP_POW			Source(s) and dest must be same type specifier, can be float, vec2, vec3 or vec4		
	GLSLIC_OP_FRC
	GLSLIC_OP_MOD
	GLSLIC_OP_TAN
	GLSLIC_OP_ASIN
	GLSLIC_OP_ACOS
	GLSLIC_OP_ATAN

	GLSLIC_OP_MAD			-- 
	GLSLIC_OP_MIX

	GLSLIC_OP_NRM3			Both source and dest must be vec3.
*/

typedef enum GLSLICOpcodeTAG
{
	/* For all allowed data type, refer to the operand rules above */
	GLSLIC_OP_NOP,		/* empty op */

	GLSLIC_OP_MOV,		/* Dst, SrcA: Dst = SrcA */

	GLSLIC_OP_ADD,		/* Dst, SrcA, SrcB: Dst = SrcA + SrcB */
	GLSLIC_OP_SUB,		/* Dst, SrcA, SrcB: Dst = SrcA - SrcB */
	GLSLIC_OP_MUL,		/* Dst, SrcA, SrcB: Dst = SrcA * SrcB */
	GLSLIC_OP_DIV,		/* Dst, SrcA, SrcB: Dst = SrcA / SrcB */

	/* test and comparison ops */
	GLSLIC_OP_SEQ,		/* Dst, SrcA, SrcB:  Dst.m = (SrcA.m == SrcB.m)? T : F; */
	GLSLIC_OP_SNE,		/* Dst, SrcA, SrcB:  Dst.m = (SrcA.m != SrcB.m)? T : F; */

	GLSLIC_OP_SLT,		/* Dst, SrcA, SrcB:  Dst.m = (SrcA.m < SrcB.m)? T : F; */
	GLSLIC_OP_SLE,		/* Dst, SrcA, SrcB:  Dst.m = (SrcA.m <= SrcB.m)? T : F; */
	GLSLIC_OP_SGT,		/* Dst, SrcA, SrcB:  Dst.m = (SrcA.m > SrcB.m)? T : F; */
	GLSLIC_OP_SGE,		/* Dst, SrcA, SrcB:  Dst.m = (SrcA.m >= SrcB.m)? T : F; */

	GLSLIC_OP_NOT,		/* Dst, SrcA: Dst.m = !SrcA.m component wise, Dst and SrcA must be booleans */

	/* conditional execution ops */
	GLSLIC_OP_IF,		/* Src: Executes IF...ELSE/ENDIF block if Src == T */
	GLSLIC_OP_IFNOT,	/* Src: Executes IFNOT...ELSE/ENDIF block if Src == T */

	GLSLIC_OP_IFLT,		/* If (SrcA < SrcB) */
	GLSLIC_OP_IFLE,		/* If (SrcA <= SrcB) */
	GLSLIC_OP_IFGT,		/* If (SrcA > SrcB) */
	GLSLIC_OP_IFGE,		/* If (SrcA >= SrcB) */
	GLSLIC_OP_IFEQ,		/* If (SrcA == SrcB) */
	GLSLIC_OP_IFNE,		/* If (SrcA != SrcB) */

	GLSLIC_OP_ELSE,
	GLSLIC_OP_ENDIF,



	/* procedure call/return ops */
	GLSLIC_OP_LABEL,	/* Src: Label/Subroutine name */
	GLSLIC_OP_RET,
	GLSLIC_OP_CALL,		/* Src: Label */

	/* loop ops */
	GLSLIC_OP_LOOP,		/* Start unconditional (infinite) loop */
	GLSLIC_OP_STATICLOOP,
	GLSLIC_OP_ENDLOOP,	/* End unconditional (infinite) loop */
	GLSLIC_OP_CONTINUE,	/* Must be followed by a CONTDEST before ENDLOOP */
	GLSLIC_OP_CONTDEST,	/* Specifies where execution resumes after a CONTINUE */
	GLSLIC_OP_BREAK,	
	GLSLIC_OP_DISCARD,	/* Fragement shaders only */

	GLSLIC_OP_SANY,		/* Dst, SrcA: Dst = (any components of SrcA) ? T : F */
	GLSLIC_OP_SALL,		/* Dst, SrcA: Dst = (all components of SrcA) ? T : F */

	GLSLIC_OP_ABS,		/* Dst, SrcA:		Dst = abs(SrcA),							coomponent-wis */
	GLSLIC_OP_MIN,		/* Dst, SrcA, SrcB: Dst = (SrcA < SrcB) ? SrcA : SrcB,			component-wise */
	GLSLIC_OP_MAX,		/* Dst, SrcA, SrcB: Dst = (SrcA > SrcB) ? SrcA : SrcB,			component-wise */
	GLSLIC_OP_SGN,		/* Dst, SrcA: Dst= (SrcA < 0) ? -1.0 : (SrcA == 0)? 0.0 : 1.0,	component-wise */

	GLSLIC_OP_RCP,		/* Dst, SrcA:		Dst = 1.0/SrcA,			component-wise	*/
	GLSLIC_OP_RSQ,		/* Dst, SrcA:		Dst = 1.0/sqrt(SrcA),	component-wise	*/

	GLSLIC_OP_LOG,		/* Dst, SrcA:		Dst = Log[e, SrcA],		component-wise	*/
	GLSLIC_OP_LOG2,		/* Dst, SrcA:		Dst = Log[2, SrcA],		component-wise	*/
	GLSLIC_OP_EXP,		/* Dst, SrcA:		Dst = pow(e, SrcA),		component-wise	*/
	GLSLIC_OP_EXP2,		/* Dst, SrcA:		Dst = pow(2, SrcA),		component-wise	*/

	GLSLIC_OP_DOT,		/* Dst, SrcA, SrcB: Dst =   dot(SrcA, SrcB)					*/
	GLSLIC_OP_CROSS,	/* Dst, SrcA, SrcB: Dst = cross(SrcA, SrcB)					*/

	GLSLIC_OP_SIN,		/* Dst, SrcA:		Dst = sin(SrcA),		component-wise	*/
	GLSLIC_OP_COS,		/* Dst, SrcA:		Dst = cos(SrcA),		component-wise	*/

	GLSLIC_OP_FLOOR,	/* Dst, SrcA:		Dst = floor(SrcA),		component-wise	*/
	GLSLIC_OP_CEIL,		/* Dst, SrcA:		Dst =  ceil(SrcA),		component-wise	*/

	GLSLIC_OP_DFDX,		/* Dst, SrcA:		Derivative in x for input SrcA */
	GLSLIC_OP_DFDY,		/* Dst, SrcA:		Derivative in y for input SrcA */

	/* Texture look up */
	GLSLIC_OP_TEXLD,	/* Dst, SrcA, SrcB: Normal texture look up					*/
	GLSLIC_OP_TEXLDP,	/* Dst, SrcA, SrcB: Projected texture look up				*/
	GLSLIC_OP_TEXLDB,	/* Dst, SrcA, SrcB, SrcC: Look up with lod bias, fragement shaders only  */
	GLSLIC_OP_TEXLDL,	/* Dst, SrcA, SrcB, SrcC: Look up with specific lod, vertex shaders only */
	GLSLIC_OP_TEXLDD,	/* Dst, Sampler, coord, dPdx, dpDy, look up with supplied gradient */

	/* Potential extension ops */
	GLSLIC_OP_POW,		/* Dst, SrcA, SrcB: Dst = pow(SrcA, SrcB),		component-wise */
	GLSLIC_OP_FRC,		/* Dst, SrcA:		Dst = SrcA - floor(SrcA),	component-wise */
	GLSLIC_OP_MOD,		/* Dst, SrcA, SrcB: Dst = SrcA % SrcB			component-wise */

	GLSLIC_OP_TAN,		/* Dst, SrcA:		Dst =  tan(SrcA),			component-wise */
	GLSLIC_OP_ASIN,		/* Dst, SrcA:		Dst = asin(SrcA),			component-wise */ 
	GLSLIC_OP_ACOS,		/* Dst, SrcA:		Dst = acos(SrcA),			component-wise */
	GLSLIC_OP_ATAN,		/* Dst, SrcA:		Dst = atan(SrcA),			component-wise */

	GLSLIC_OP_MAD,		/* Dst SrcA SrcB SrcC: Dst = SrcA * SrcB + SrcC, component-wise */

	GLSLIC_OP_MIX,		/* Dst SrcA SrcB SrcC: Dst = SrcA * (1 - SrcC) + SrcB * SrcC component-wise */

	GLSLIC_OP_NRM3,		/* Dst SrcA: Dst = SrcA * (1 / sqrt(dot(SrcA, SrcA))) */


	GLSLIC_OP_NUM,		/* not an IC op, just the total number of ops */
	GLSLIC_OP_FORCEDWORD	= 0x7FFFFFFF
} GLSLICOpcode;


typedef enum GLSLICExtensionsSupportedTAG
{
	GLSLIC_EXT_POW         = 0x00000001,
	GLSLIC_EXT_FRC         = 0x00000002,
	GLSLIC_EXT_MOD         = 0x00000004,
	GLSLIC_EXT_TAN         = 0x00000008,
	GLSLIC_EXT_ASIN        = 0x00000010,
	GLSLIC_EXT_ACOS        = 0x00000020,
	GLSLIC_EXT_ATAN        = 0x00000040,
	GLSLIC_EXT_MAD         = 0x00000080,
	GLSLIC_EXT_SAT_MOD     = 0x00000100,
	GLSLIC_EXT_NEG_MOD     = 0x00000200,
	GLSLIC_EXT_FORCEDWORD  = 0x7FFFFFFF,
} GLSLICExtensionsSupported;

typedef enum GLSLICVecComponentTAG 
{
	GLSLIC_VECCOMP_X = 0,
	GLSLIC_VECCOMP_Y = 1,
	GLSLIC_VECCOMP_Z = 2,
	GLSLIC_VECCOMP_W = 3,
} GLSLICVecComponent;

typedef struct GLSLICVecSwizWMaskTAG
{
	IMG_UINT32         	uNumComponents;
	GLSLICVecComponent	aeVecComponent[4];
} GLSLICVecSwizWMask;

typedef struct GLSLICOperandOffsetTAG
{
	IMG_UINT32	uOffsetSymbolID;
	IMG_UINT32	uStaticOffset;
} GLSLICOperandOffset;


typedef enum GLSLICModifierTAG
{
	GLSLIC_MODIFIER_NONE		= 0x00000000,
	GLSLIC_MODIFIER_NEGATE		= 0x00000001,
	GLSLIC_MODIFIER_SATURATE	= 0x00000002,
	GLSLIC_MODIFIER_FORCEDWORD	= 0x7FFFFFFF,
} GLSLICModifier;

/* 
	IC operand 
*/
typedef struct GLSLICOperandTAG
{
	/* The symbol, whose data is to be accessed by this operand */
	IMG_UINT32			uSymbolID;

	/*
		Swizzle/write mask applied to this operand, when the accessed type is a vector.
		If sSwizWMask.uNumComponents is zero, then all components of the accessed vector
		are used.
	*/
	GLSLICVecSwizWMask	sSwizWMask;

	/* Any modifiers to apply to instruction? */
	GLSLICModifier		eInstModifier;

	/* The number of offsets pointed-to by psOffsets */
	IMG_UINT32			uNumOffsets;

	/* An array of type-dependent offsets into the data associated with this operand */
	GLSLICOperandOffset	*psOffsets;

} GLSLICOperand;

/* For indexing operands of an instruction */
enum
{
	DEST,
	SRCA,
	SRCB,
	SRCC,
	SRCD,
	MAX_OPRDS
};

/* 
	IC instruction 
*/
typedef struct GLSLICInstructionTAG
{
	/* Opcode */
	GLSLICOpcode	eOpCode;

	/* 
		Predicate for the instruction, the following are the all instructions which support predicate: 
		IFP, LOOP, ENDLOOP and SGT, SGE, SLT, SLE, SEQ and SNE 
	*/
	IMG_UINT32		uPredicateBoolSymID;
	IMG_BOOL		bPredicateNegate;

	/* Operands */
	GLSLICOperand	asOperand[MAX_OPRDS];	/* NB: 0=destination */
	IMG_CHAR		*pszOriginalLine;

	/*
	 Source line number in code for which the instruction is generated. 
	*/
	#ifdef SRC_DEBUG
	IMG_UINT32	uSrcLine;
	#endif /* SRC_DEBUG */

	/* Linked list */
	struct GLSLICInstructionTAG *psNext, *psPrev;
} GLSLICInstruction;

typedef struct GLSLICDepthTextureTAG
{
	IMG_INT32		iOffset;		/* Array offset if sampler is an element of an array, 0 for non-array */
	IMG_UINT32		uTextureSymID;  /* Texture sampler Symbol ID */
	IMG_UINT32		uTexDescSymID;	/* Texture description symbol ID */
}GLSLICDepthTexture;

/* 
** Intermediate code program, the output after generation of IC
*/
typedef struct GLSLICProgramTAG
{
	GLSLICInstruction		*psInstrHead;
	GLSLICInstruction		*psInstrTail;

	/* 
		Depth texture infomation 
	*/
	IMG_UINT32				 uNumDepthTextures;
	GLSLICDepthTexture		 asDepthTexture[8];

	SymTable     			*psSymbolTable;

	ErrorLog                *psErrorLog;

	IMG_VOID                *pvContextData;

} GLSLICProgram;

#endif //_icode_h_
