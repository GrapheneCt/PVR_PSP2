/******************************************************************************
 * Name         : icgen.h
 * Created      : 24/06/2004
 *
 * Copyright    : 2004-2006 by Imagination Technologies Limited.
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
 * $Log: icgen.h $
 *****************************************************************************/

#ifndef __gl_icgen_h_
#define __gl_icgen_h_

#include "icode.h"
#include "error.h"

#define OGL_PI 3.14159265358979f

#if defined(GLSL_ES)
/*
Constant defining maximum point size used for clamping. 
*/
#define OGLES2_POINT_SIZE_MAX			511.0f
#endif

#define GET_SHADERCODE_LINE(node) (node->psToken ? (node->psToken->pszStartOfLine) : IMG_NULL)

/*
Macro to get the source line number. 
*/
#ifdef SRC_DEBUG
IMG_UINT32 SearchNodeSourceLine(GLSLNode *psNode);
#define GET_SHADERCODE_LINE_NUMBER(node) (SearchNodeSourceLine(node))
#else
#define  GET_SHADERCODE_LINE_NUMBER(node) ((IMG_UINT32)(-1)) 
#endif /* SRC_DEBUG */

/* For indexing noise function */
#define NOISE1D	0
#define NOISE2D 1
#define NOISE3D 2
#define NOISE4D 3

typedef struct GLSLICFunctionDefinitionTAG
{
	IMG_UINT32		uFunctionDefinitionID;

	/* Currently only support one parameter for internal functions */
	IMG_UINT32		uParamSymbolID;

	IMG_UINT32		uReturnDataSymbolID;
}GLSLICFunctionDefinition;

typedef struct GLSLICShaderChildTAG
{
	IMG_BOOL			bDeclaration;
	IMG_BOOL			bCodeGenerated;
	IMG_BOOL			bToBeInlined;
	IMG_BOOL			bEmptyBody;
	GLSLICInstruction	*psCodeStart;
	GLSLICInstruction	*psCodeEnd;

	IMG_UINT32			uFunctionDefinitionID;
} GLSLICShaderChild;


/*
** Intermediate code context
*/
typedef struct GLSLICContextTAG
{
	GLSLTreeContext				*psTreeContext;

	/* Which extensions doe the current HW Compiler support */
	GLSLICExtensionsSupported	eExtensionsSupported;

	GLSLPrecisionQualifier		eDefaultFloatPrecision;	
	GLSLPrecisionQualifier		eBooleanPrecision;

	GLSLInitCompilerContext		*psInitCompilerContext;

	/* Is it a main function? */
	IMG_BOOL					bMainFunction;

	IMG_UINT32					uTempIssusedNo;

	/* If loop nest count and has a proper return generated ? */
	IMG_INT32					iConditionLevel;
	IMG_BOOL					bHadReturn;

	/* Nested for loop level */
	IMG_INT32					iForLoopLevel;

	/* Program type */
	GLSLProgramType				eProgramType;

	/* Which builtin variables been written to */
	GLSLBuiltInVariableWrittenTo	eBuiltInsWrittenTo;

	/*
		void pmx_arccos2(inout vec4 r0); // Calculate arccos of a single value (using algorithm 2: see compiler\misc\trig\trig.c)
		void pmx_arcsin2(inout vec4 r0); // Calculate arcsin of a single value (using algorithm 2: see compiler\misc\trig\trig.c)
		void pmx_arctan (inout vec4 r0); // Calculate arctan of a single value
		void pmx_arctan2 (inout vec4 r0); // Calculate arctan of a single value

		//r0.x is the input and the output except for arctan2 that takes r0.x /and/ r0.y as inputs.
	*/
	GLSLICFunctionDefinition	*psArcCos2Func;
	GLSLICFunctionDefinition	*psArcSin2Func;
	GLSLICFunctionDefinition	*psArcTanFunc;
	GLSLICFunctionDefinition	*psArcTan2Func;

	/*
		float pmx_noise1_1D(float P);
		float pmx_noise1_2D(vec2 P);
		float pmx_noise1_3D(vec3 P);
		float pmx_noise1_4D(vec4 P);

		// See compiler\misc\noise\glslnoisept.c
	*/
	GLSLICFunctionDefinition	*psNoiseFuncs[4];

	IMG_UINT32					uNumShaderChildren;
	GLSLICShaderChild			*psShaderChildren;

	MemHeap						*psInstructionHeap;

} GLSLICContext;


#define GET_IC_CONTEXTDATA(psICProgram) ((GLSLICContext *)(psICProgram->pvContextData))

/*
** A table for IC information, handy to use 
*/
typedef struct ICodeOpTableTAG
{
#if defined(DEBUG) || defined(DUMP_LOGFILES)
	GLSLICOpcode		eICOpCode;
	const IMG_CHAR		*pszStr;
#endif
	IMG_BOOL			bHasDest;
	IMG_UINT32			uNumSrcOperands;
}ICodeOpTable;

extern const ICodeOpTable asICodeOpTable[];

#define ICOP_ICOP(a)			asICodeOpTable[a].eICOpCode /* used to check any mismatch */
#define ICOP_HAS_DEST(a)		asICodeOpTable[a].bHasDest
#define ICOP_NUM_SRCS(a)		asICodeOpTable[a].uNumSrcOperands
#define ICOP_DESC(a)			asICodeOpTable[a].pszStr

/* 
** A table for node type information, handy to use 
*/
typedef struct NodeTypeTableTAG
{
#if defined(DEBUG) || defined(DUMP_LOGFILES)
	GLSLNodeType	eNodeType;
	const IMG_CHAR	*pszStr;
#endif
	GLSLICOpcode	eICodeOp;

#ifndef GLSL_ES
	IMG_BOOL		bNodeAllowsIntToFloatConversion;
#endif

}NodeTypeTable;

extern const NodeTypeTable asNodeTable[];

#define NODETYPE_NODETYPE(a)				asNodeTable[a].eNodeType /* used to check any mismatch */
#define NODETYPE_ICODEOP(a)					asNodeTable[a].eICodeOp
#define NODETYPE_DESC(a)					asNodeTable[a].pszStr
#define NODETYPE_ITOF(a)					asNodeTable[a].bNodeAllowsIntToFloatConversion

#define TYPESPECIFIER_DESC(a)				GLSLTypeSpecifierDescTable(a)
#define TYPESPECIFIER_BASE_TYPE(a)			GLSLTypeSpecifierBaseTypeTable(a)
#define TYPESPECIFIER_NUM_COLS(a)			GLSLTypeSpecifierNumColumnsCMTable(a)
#define TYPESPECIFIER_NUM_ROWS(a)			GLSLTypeSpecifierNumRowsCMTable(a)
#define TYPESPECIFIER_NUM_COMPONENTS(a)		TYPESPECIFIER_NUM_ROWS(a)
#define TYPESPECIFIER_DATA_SIZE(a)			GLSLTypeSpecifierSizeTable(a)
#define TYPESPECIFIER_NUM_ELEMENTS(a)		GLSLTypeSpecifierNumElementsTable(a)
#define TYPESPECIFIER_DIMS(a)				GLSLTypeSpecifierDimensionTable(a)


/* This relies on the order of type specifier define order */
#define CONVERT_TO_VECTYPE(type, components) (GLSLTypeSpecifier)(TYPESPECIFIER_BASE_TYPE(type) + components - 1)
#define INDEXED_MAT_TYPE(type)				 CONVERT_TO_VECTYPE(type, TYPESPECIFIER_NUM_ROWS(type))

#define DEFAULT_FLOAT_PRECISION(icprogram)	(GET_IC_CONTEXTDATA(icprogram)->eDefaultFloatPrecision)
#define BOOLEAN_PRECISION(icprogram)	(GET_IC_CONTEXTDATA(icprogram)->eBooleanPrecision)

#define NOISE_FUNC_PRECISION				GLSLPRECQ_HIGH
#define TRIG_FUNC_PRECISION					GLSLPRECQ_HIGH

/*
** Linked list for offsets of an IC operand.
*/
typedef struct GLSLICOperandOffsetListTAG
{
	GLSLICOperandOffset					sOperandOffset;
	struct GLSLICOperandOffsetListTAG	*psNext;
}GLSLICOperandOffsetList;

/* 
** The general operand information structure. We deliberately dont make this a typedef, because 
   glsltree.h references it, and glsltree.h is always included first. See glsltree.h
*/
struct GLSLICOperandInfoTAG
{
	IMG_UINT32				uSymbolID;

	GLSLICVecSwizWMask 		sSwizWMask;

	GLSLICModifier			eInstModifier; 

	GLSLFullySpecifiedType	sFullType;	/* This is the type after swiz */

	GLSLICOperandOffsetList *psOffsetList;
	GLSLICOperandOffsetList *psOffsetListEnd;
};

extern const GLSLICVecSwizWMask sDefaultSwizMask;

/*
** Recursively traverse Abstract Syntax Tree and adding IC instructions to the IC program
*/
IMG_VOID ICTraverseAST(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psASTree);

/*
** Dump an IC instruction 
*/
IMG_VOID ICDumpICInstruction(IMG_UINT32			uLogFile,
						   SymTable     		*psSymbolTable,
						   IMG_UINT32			uIndent,
						   GLSLICInstruction	*psICInstr,
						   IMG_UINT32			uInstrNum,
						   IMG_BOOL				bDumpOriginalLine,
						   IMG_BOOL				bDumpSymbolID);

IMG_BOOL ICAddTemporary(GLSLCompilerPrivateData *psCPD,
						GLSLICProgram				*psICProgram,
					  GLSLTypeSpecifier			eTypeSpecifier, 
					  GLSLPrecisionQualifier	ePrecisionQualifier, 
					  IMG_UINT32				*puSymbolID);

IMG_BOOL ICAddBoolPredicate(GLSLCompilerPrivateData *psCPD, 
							GLSLICProgram *psICProgram, IMG_UINT32 *puSymbolID);

/* Get type specifier for specific symbol ID */
IMG_BOOL ICGetSymbolInformation(GLSLCompilerPrivateData *psCPD, 
								SymTable				*psSymbolTable,
							    IMG_UINT32				uSymbolID,
							    GLSLBuiltInVariableID	*peBuiltinID,
							    GLSLFullySpecifiedType	**ppsFullType,
							    IMG_INT32				*piArraySize,
							    GLSLIdentifierUsage		*peIdentifierUsage,
							    IMG_VOID				**ppvConstantData);

GLSLFullySpecifiedType *ICGetSymbolFullType(GLSLCompilerPrivateData *psCPD, 
											SymTable *psSymbolTable, IMG_UINT32 uSymbolID);
GLSLTypeSpecifier ICGetSymbolTypeSpecifier(GLSLCompilerPrivateData *psCPD,
										   SymTable *psSymbolTable, IMG_UINT32 uSymbolID);
GLSLPrecisionQualifier ICGetSymbolPrecision(GLSLCompilerPrivateData *psCPD,
											SymTable *psSymbolTable, IMG_UINT32 uSymbolID);
GLSLBuiltInVariableID ICGetSymbolBIVariableID(GLSLCompilerPrivateData *psCPD,
											  SymTable *psSymbolTable, IMG_UINT32 uSymbolID);

/*
** Return fully specified type and array size when a [] applies to a 
** specific type. uOffset is used only when the base type is a struct.
*/
GLSLFullySpecifiedType GetIndexedType(	GLSLCompilerPrivateData			*psCPD,
										SymTable						*psSymbolTable,
										const GLSLFullySpecifiedType	*psBaseType,
										IMG_UINT32						uOffset,
										IMG_INT32						*piArraySize);

/*
** Process node by visiting itself and offspring(when neccessary), return psOperandInfo
** The returned psOperandInfo has offsets attached, use ICFreeOperandOffsetList to free
** the offset list.
*/
IMG_VOID ICProcessNodeOperand(GLSLCompilerPrivateData *psCPD,
							  GLSLICProgram *psICProgram,
							  GLSLNode			*psNode,
							  GLSLICOperandInfo	*psOperandInfo);
IMG_VOID ICAddOperandOffset(GLSLICOperandInfo *psOperandInfo, IMG_UINT32 uStaticOffset, IMG_UINT32 uOffsetSymbolID);

IMG_VOID ICFreeOperandOffsetList(GLSLICOperandInfo *psOperandInfo);

IMG_VOID ICAdjustOperandLastOffset(GLSLICOperandInfo *psOperandInfo, IMG_UINT32 uNewStaticOffset, IMG_UINT32 uNewOffsetSymbolID);

/*
**	Initilise operand info (GLSLICOperandInfo)
*/
IMG_VOID ICInitOperandInfo(IMG_UINT32 uSymbolID, GLSLICOperandInfo *psOperandInfo);

/*
	Initilise an IC operand (GLSLICOperand)
*/
IMG_VOID ICInitICOperand(IMG_UINT32 uSymbolID, GLSLICOperand *psOperand);

/*
** Combine two constant swiz, the result swiz write to the first swiz 
*/
IMG_VOID ICCombineTwoConstantSwizzles(GLSLICVecSwizWMask *psFirstSwiz,
									  GLSLICVecSwizWMask *psSecondSwiz);

IMG_BOOL IsSymbolLValue(GLSLCompilerPrivateData *psCPD, SymTable *psSymbolTable, IMG_UINT32 uSymbolID, IMG_BOOL *bSingleInteger);
IMG_BOOL IsSymbolIntConstant(GLSLCompilerPrivateData *psCPD, SymTable *psSymbolTable, IMG_UINT32 uSymbolID, IMG_INT32 *piData);
IMG_BOOL ICIsSymbolScalar(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, IMG_UINT32 uSymbolID);

IMG_VOID ICRemoveInstruction(GLSLICProgram *psICProgram, GLSLICInstruction *psICInstr);

IMG_VOID ICRemoveInstructionRange(GLSLICProgram		*psICProgram,
								  GLSLICInstruction	*psStart,
								  GLSLICInstruction	*psEnd);

IMG_VOID CloneICodeInstructions(GLSLCompilerPrivateData *psCPD, 
								GLSLICProgram		*psICProgram,
								GLSLICInstruction	*psStart,
								GLSLICInstruction	*psEnd,
								GLSLICInstruction	**ppsClonedStart, 
								GLSLICInstruction	**ppsClonedEnd);

IMG_UINT32 GetInstructionsCount(GLSLICInstruction *psStart, GLSLICInstruction *psEnd);

/*
** variations of adding an IC instruction 
*/

/* The most two general functions to add an IC instruction */
IMG_BOOL ICAddICInstruction(GLSLCompilerPrivateData *psCPD,
							GLSLICProgram		*psICProgram,
							GLSLICOpcode		 eICOpcode,
							IMG_UINT32			 uNumSources,
							IMG_CHAR			*pszLineStart,
							GLSLICOperandInfo	*psDestOperand,
							GLSLICOperandInfo	*psOperands);


IMG_BOOL ICAddICInstructiona(GLSLCompilerPrivateData *psCPD,
							 GLSLICProgram		*psICProgram,
							 GLSLICOpcode		 eICOpcode,
							 IMG_UINT32			 uNumSources,
							 IMG_CHAR			*pszLineStart,
							 IMG_UINT32			 uSymbolIDDEST,
							 GLSLICOperandInfo	*psOperands);

/* The following are some optimised variations for adding IC instruciton */
IMG_BOOL ICAddICInstruction0(GLSLCompilerPrivateData *psCPD,
							 GLSLICProgram		*psICProgram,
							 GLSLICOpcode		 eICOpcode,
							 IMG_CHAR			*pszLineStart);

IMG_BOOL ICAddICInstruction1(GLSLCompilerPrivateData *psCPD,
							 GLSLICProgram		*psICProgram,
							 GLSLICOpcode		 eICOpcode,
							 IMG_CHAR			*pszLineStart,
							 GLSLICOperandInfo	*psOperandSRC);

IMG_BOOL ICAddICInstruction1a(GLSLCompilerPrivateData *psCPD,
							  GLSLICProgram		*psICProgram,
							  GLSLICOpcode		 eICOpcode,
							  IMG_CHAR			*pszLineStart,
							  IMG_UINT32		uSymbolID);

IMG_BOOL ICAddICInstruction2(GLSLCompilerPrivateData *psCPD,
							 GLSLICProgram		*psICProgram,
							 GLSLICOpcode		 eICOpcode,
							 IMG_CHAR			*pszLineStart,
							 GLSLICOperandInfo	*psOperandDEST,
							 GLSLICOperandInfo	*psOperandSRCA);

IMG_BOOL ICAddICInstruction2a(GLSLCompilerPrivateData *psCPD,
							  GLSLICProgram		*psICProgram,
							  GLSLICOpcode		 eICOpcode,
							  IMG_CHAR			*pszLineStart,
							  IMG_UINT32		uSymbolIDDEST,
							  GLSLICOperandInfo	*psOperandSRCA);

IMG_BOOL ICAddICInstruction2b(GLSLCompilerPrivateData *psCPD,
							  GLSLICProgram		*psICProgram,
							  GLSLICOpcode		 eICOpcode,
							  IMG_CHAR			*pszLineStart,
							  IMG_UINT32		 uSymbolIDDEST,
							  IMG_UINT32		 uSymboIDSRCA);

IMG_BOOL ICAddICInstruction2c(GLSLCompilerPrivateData *psCPD,
							  GLSLICProgram		*psICProgram,
							  GLSLICOpcode		 eICOpcode,
							  IMG_CHAR			*pszLineStart,
							  GLSLICOperandInfo *psOperandDEST,
							  IMG_UINT32		uSymbolIDSRCA);

IMG_BOOL ICAddICInstruction3(GLSLCompilerPrivateData *psCPD,
							 GLSLICProgram		*psICProgram,
							 GLSLICOpcode		 eICOpcode,
							 IMG_CHAR			*pszLineStart,
							 GLSLICOperandInfo	*psOperandDEST,
							 GLSLICOperandInfo	*psOperandSRCA,
							 GLSLICOperandInfo	*psOperandSRCB);

IMG_BOOL ICAddICInstruction3a(GLSLCompilerPrivateData *psCPD,
							  GLSLICProgram		*psICProgram,
							  GLSLICOpcode		 eICOpcode,
							  IMG_CHAR			*pszLineStart,
							  IMG_UINT32		 uSymbolIDDEST,
							  GLSLICOperandInfo	*psOperandSRCA,
							  GLSLICOperandInfo	*psOperandSRCB);

IMG_BOOL ICAddICInstruction3b(GLSLCompilerPrivateData *psCPD,
							  GLSLICProgram		*psICProgram,
							  GLSLICOpcode		 eICOpcode,
							  IMG_CHAR			*pszLineStart,
							  IMG_UINT32		 uSymbolIDDEST,
							  GLSLICOperandInfo	*psOperandSRCA,
							  IMG_UINT32		 uSymbolIDSRCB);

IMG_BOOL ICAddICInstruction3c(GLSLCompilerPrivateData *psCPD,
							  GLSLICProgram		*psICProgram,
							  GLSLICOpcode		 eICOpcode,
							  IMG_CHAR			*pszLineStart,
							  IMG_UINT32		 uSymbolIDDEST,
							  IMG_UINT32		 uSymbolIDSRCA,
							  GLSLICOperandInfo	*psOperandSRCB);

IMG_BOOL ICAddICInstruction3d(GLSLCompilerPrivateData *psCPD,
							  GLSLICProgram		*psICProgram,
							  GLSLICOpcode		 eICOpcode,
							  IMG_CHAR			*pszLineStart,
							  IMG_UINT32		 uSymbolIDDEST,
							  IMG_UINT32		 uSymbolIDSRCA,
							  IMG_UINT32		 uSymbolIDSRCB);

IMG_BOOL ICAddICInstruction3e(GLSLCompilerPrivateData *psCPD,
							  GLSLICProgram		*psICProgram,
							  GLSLICOpcode		eICOpcode,
							  IMG_CHAR			*pszLineStart,
							  GLSLICOperandInfo *psDestOperand,
							  IMG_UINT32		uSymbolIDSRCA,
							  IMG_UINT32		uSymbolIDSRCB);

IMG_BOOL ICAddICInstruction4(GLSLCompilerPrivateData *psCPD,
GLSLICProgram		*psICProgram,
							  GLSLICOpcode		 eICOpcode,
							  IMG_CHAR			*pszLineStart,
							  GLSLICOperandInfo	*psOperandDEST,
							  GLSLICOperandInfo	*psOperandSRCA,
							  GLSLICOperandInfo	*psOperandSRCB,
							  GLSLICOperandInfo *psOperandSRCC);

#endif /* __gl_icgen_h_ */

/******************************************************************************
 End of file (icgen.h)
******************************************************************************/

