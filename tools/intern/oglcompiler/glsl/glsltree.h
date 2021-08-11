/******************************************************************************
 * Name         : glsltree.h
 * Author       : James McCarthy
 * Created      : 24/06/2004
 *
 * Copyright    : 2004-2007 by Imagination Technologies Limited.
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
 * $Log: glsltree.h $
 *****************************************************************************/

#ifndef __gl_glsltree_h_
#define __gl_glsltree_h_

#include "glsl.h"
#include "parser.h"
#include "metrics.h"
#include "errorlog.h"
#include "icode.h"




/* Saves ~6k */
#ifdef COMPACT_MEMORY_MODEL

#define CONSTRUCTOR_RETURN_VAL_STRING "cst_rv@%s"
#define CONSTRUCTOR_STRING            "cst@%s"
#define RETURN_VAL_STRING             "rv_%s"
#define RETURN_VAL_STRING_LENGTH      3
#define CONSTRUCTOR_PARAM_STRING      "cst_p%u@%s"
#define RESULT_STRING                 "rs_%s@%u"
#define RESULT_STRUCT_STRING          "rs_%s_%s@%u"
#define PRECISION_MODIFIER_STRING     "prec_mod@%s"
#define FUNCTION_CALL_STRING          "fc_%s_%s@%u"
#else

#define CONSTRUCTOR_RETURN_VAL_STRING "constructor_returnval@%s"
#define CONSTRUCTOR_STRING            "constructor@%s"
#define RETURN_VAL_STRING             "returnval_%s"
#define RETURN_VAL_STRING_LENGTH      10
#define CONSTRUCTOR_PARAM_STRING      "constructor_param%lu@%s"
#define RESULT_STRING                 "result_%s@%lu"
#define RESULT_STRUCT_STRING          "result_%s_%s@%lu"
#define PRECISION_MODIFIER_STRING     "precision_modifier@%s"
#define FUNCTION_CALL_STRING          "function_call_%s_%s@%lu"

#endif


/*
	Node Type					|no Children| Possible Child Node Type
--------------------------------+-----------+-------------------------------------------------------------
	GLSLNT_IDENTIFIER			| 0			|
	GLSLNT_INTCONSTANT			| 0			|
	GLSLNT_FLOATCONSTANT		| 0			|
	GLSLNT_BOOLCONSTANT			| 0			|	

	GLSLNT_FIELDSELECTION		| 1,2,3,4	| 
	GLSLNT_ARRAYSELECTION		| 2			| GLSLNT_IDENTIFIER(left), GLSLNT_INTCONSTANT(right)
	GLSLNT_POSTINC				| 1			| GLSLNT_IDENTIFIER
	GLSLNT_POSTDEC				| 1			| GLSLNT_IDENTIFIER
	GLSLNT_FUNCTIONCALL			|			|

	GLSLNT_PREINC				| 1			| GLSLNT_IDENTIFIER
	GLSLNT_PREDEC				| 1			| GLSLNT_IDENTIFIER
	GLSLNT_NEGATE				| 1			| GLSLNT_IDENTIFIER
	GLSLNT_NOT					| 1			| GLSLNT_IDENTIFIER

	GLSLNT_MULTIPLY				| 2			| GLSLNT_IDENTIFIER, GLSLNT_INTCONSTANT, GLSLNT_FLOATCONSTANT
	GLSLNT_DIVIDE				| 2			| GLSLNT_IDENTIFIER, GLSLNT_INTCONSTANT, GLSLNT_FLOATCONSTANT
	GLSLNT_MODULUS,             | 2		

	GLSLNT_ADD					| 2			| GLSLNT_IDENTIFIER, GLSLNT_INTCONSTANT, GLSLNT_FLOATCONSTANT
	GLSLNT_SUBTRACT				| 2			| GLSLNT_IDENTIFIER, GLSLNT_INTCONSTANT, GLSLNT_FLOATCONSTANT

	GLSLNT_SHIFTLEFT,
	GLSLNT_SHIFTRIGHT,

	GLSLNT_LESSTHAN				| 2			| GLSLNT_IDENTIFIER, GLSLNT_INTCONSTANT, GLSLNT_FLOATCONSTANT
	GLSLNT_GREATERTHAN			| 2			| GLSLNT_IDENTIFIER, GLSLNT_INTCONSTANT, GLSLNT_FLOATCONSTANT
	GLSLNT_LESSTHANEQUAL		| 2			| GLSLNT_IDENTIFIER, GLSLNT_INTCONSTANT, GLSLNT_FLOATCONSTANT
	GLSLNT_GREATERTHATEQUAL		| 2			| GLSLNT_IDENTIFIER, GLSLNT_INTCONSTANT, GLSLNT_FLOATCONSTANT

	GLSLNT_EQUALTO				| 2			| GLSLNT_IDENTIFIER, GLSLNT_INTCONSTANT, GLSLNT_FLOATCONSTANT
	GLSLNT_NOTEQUALTO			| 2			| GLSLNT_IDENTIFIER, GLSLNT_INTCONSTANT, GLSLNT_FLOATCONSTANT

	GLSLNT_BITWISEOR                     // reserved 
	GLSLNT_BITWISEXOR,                   // reserved 
	GLSLNT_BITWISEAND,                   // reserved 

	GLSLNT_LOGICALOR			| 2			| GLSLNT_IDENTIFIER, GLSLNT_INTCONSTANT, GLSLNT_FLOATCONSTANT
	GLSLNT_LOGICALXOR			| 2			| GLSLNT_IDENTIFIER, GLSLNT_INTCONSTANT, GLSLNT_FLOATCONSTANT
	GLSLNT_LOGICALAND			| 2			| GLSLNT_IDENTIFIER, GLSLNT_INTCONSTANT, GLSLNT_FLOATCONSTANT

	GLSNT_FUNCTIONDEFINITION    | 2..n      | 0 = GLSLNT_IDENTIFIER (function name)
--------------------------------+-----------+-------------------------------------------------------------
*/
typedef enum GLSLNodeTypeTAG
{
	/* Primary expression types */
	GLSLNT_IDENTIFIER = 0,               /* No children */

	/* postfix expression types */
	GLSLNT_FIELD_SELECTION,              /* 1 = any of the above */
	GLSLNT_ARRAY_SPECIFIER,              /* 1 = any of the above */
	GLSLNT_POST_INC,                     /* 1 = any of the above */
	GLSLNT_POST_DEC,                     /* 1 = any of the above */
	GLSLNT_FUNCTION_CALL,                /* 0..n EXPRESSION */
	GLSLNT_ARRAY_LENGTH,                 /* 1 = any of the above */

	/* unary expression types */
	GLSLNT_PRE_INC,                      /* 1 = any of the above,  2 = any of the above */
	GLSLNT_PRE_DEC,                      /* 1 = any of the above,  2 = any of the above */
	GLSLNT_NEGATE,                       /* 1 = any of the above,  2 = any of the above */
	GLSLNT_POSITIVE,                     /* 1 = any of the above,  2 = any of the above */
	GLSLNT_NOT,                          /* 1 = any of the above,  2 = any of the above */

	/* multiplicative expression types */
	GLSLNT_MULTIPLY,                     /* 1 = any of the above,  2 = any of the above */  
	GLSLNT_DIVIDE,                       /* 1 = any of the above,  2 = any of the above */

	/* additive expression types */
	GLSLNT_ADD,                          /* 1 = any of the above,  2 = any of the above */
	GLSLNT_SUBTRACT,                     /* 1 = any of the above,  2 = any of the above */
	
	/* relational expression types */
	GLSLNT_LESS_THAN,                    /* 1 = any of the above,  2 = any of the above */
	GLSLNT_GREATER_THAN,                 /* 1 = any of the above,  2 = any of the above */
	GLSLNT_LESS_THAN_EQUAL,              /* 1 = any of the above,  2 = any of the above */
	GLSLNT_GREATER_THAN_EQUAL,           /* 1 = any of the above,  2 = any of the above */

	/* equality expression types */
	GLSLNT_EQUAL_TO,                     /* 1 = any of the above,  2 = any of the above */
	GLSLNT_NOTEQUAL_TO,                  /* 1 = any of the above,  2 = any of the above */

	

	/* logical expression types */
	GLSLNT_LOGICAL_OR,                   /* 1 = any of the above,  2 = any of the above */
	GLSLNT_LOGICAL_XOR,                  /* 1 = any of the above,  2 = any of the above */
	GLSLNT_LOGICAL_AND,                  /* 1 = any of the above,  2 = any of the above */

	/* conditional expression */
	GLSLNT_QUESTION,

	/* assignment operator types */
	GLSLNT_EQUAL,                        /* 1 = any of the above,  2 = any of the above */
	GLSLNT_MUL_ASSIGN,                   /* 1 = any of the above,  2 = any of the above */
	GLSLNT_DIV_ASSIGN,                   /* 1 = any of the above,  2 = any of the above */
	GLSLNT_ADD_ASSIGN,                   /* 1 = any of the above,  2 = any of the above */
	GLSLNT_SUB_ASSIGN,                   

	/* expression types */
	GLSLNT_SUBEXPRESSION,                /* 1..n = any of the above */
	GLSLNT_EXPRESSION,                   /* 1..n = any of the above */

	/* jump statement types */
	GLSLNT_CONTINUE,                     /* No children */
	GLSLNT_BREAK,	                     /* No children */
	GLSLNT_DISCARD,                      /* No children */
	GLSLNT_RETURN,                       /* 1 = EXPRESSION  */

	/* Statement types */
	GLSLNT_FOR,                          /* 1 = (init)EXPRESSION/NULL, 2 = (cond)EXPRESSION/NULL, 3 = (loop)EXPRESSION/NULL,    4 = STATEMENT_LIST/NULL */
	GLSLNT_WHILE,                        /* 1 = (cond)EXPRESSION,      2 = STATEMENT_LIST/NULL                                                          */
	GLSLNT_DO,                           /* 1 = (cond)EXPRESSION       2 = STATEMENT_LIST/NULL                                                          */
	GLSLNT_IF,                           /* 1 = (cond)EXPRESSION,      2 = STATEMENT_LIST/NULL    3 = (else)STATEMENT_LIST/NULL                         */

	/* statement list */
	GLSLNT_STATEMENT_LIST,              /* 1..n = DECLARATION/EXPRESSION/FOR/WHILE/DO/IF/CONTINUE/BREAK/RETURN/DISCARD/STATEMENT_LIST/NULL */

	/* external declaration types */
	GLSLNT_FUNCTION_DEFINITION,         /* 1    = STATEMENT_LIST */
	GLSLNT_DECLARATION,                 /* 1..n = EXPRESSION */

	/* top level node */
	GLSLNT_SHADER,                      /* 1..n = FUNCTION_DEFINITION/DECLARATION */

	GLSLNT_ERROR,

} GLSLNodeType;

typedef enum GLSLSymbolTableDataTypeTAG
{
	GLSLSTDT_IDENTIFIER,
	GLSLSTDT_FUNCTION_DEFINITION,
	GLSLSTDT_FUNCTION_CALL,
	GLSLSTDT_SWIZZLE,
	GLSLSTDT_MEMBER_SELECTION,
	GLSLSTDT_STRUCTURE_DEFINITION,
	GLSLSTDT_PRECISION_MODIFIER,

	GLSLSTDT_PRECISION_FORCE32 = 0x7FFFFFFF

} GLSLSymbolTableDataType;


/* 
** WARNING! ANY CHANGE TO THE ORDER OR NUMBER OF THE FOLLOWING ENUMS MUST BE REFLECTED IN afnProcessBuiltInFuncs (icbuiltin.c)
*/
typedef enum GLSLBuiltInFunctionIDTAG
{
	GLSLBFID_RADIANS,
	GLSLBFID_DEGREES,
	GLSLBFID_SIN,
	GLSLBFID_COS,
	GLSLBFID_TAN,
	GLSLBFID_ASIN,
	GLSLBFID_ACOS,
	GLSLBFID_ATAN,
	GLSLBFID_POW,
	GLSLBFID_EXP,
	GLSLBFID_LOG,
	GLSLBFID_EXP2,
	GLSLBFID_LOG2,
	GLSLBFID_SQRT,
	GLSLBFID_INVERSESQRT,
	GLSLBFID_ABS,
	GLSLBFID_SIGN,
	GLSLBFID_FLOOR,
	GLSLBFID_CEIL,
	GLSLBFID_FRACT,
	GLSLBFID_MOD,
	GLSLBFID_MIN,
	GLSLBFID_MAX,
	GLSLBFID_CLAMP,
	GLSLBFID_MIX,
	GLSLBFID_STEP,
	GLSLBFID_SMOOTHSTEP,
	GLSLBFID_LENGTH,
	GLSLBFID_DISTANCE,
	GLSLBFID_DOT,
	GLSLBFID_CROSS,
	GLSLBFID_NORMALIZE,
	GLSLBFID_FTRANSFORM,
	GLSLBFID_FACEFORWARD,
	GLSLBFID_REFLECT,
	GLSLBFID_REFRACT,
	GLSLBFID_MATRIXCOMPMULT,
	GLSLBFID_TRANSPOSE,
	GLSLBFID_OUTERPRODUCT,
	GLSLBFID_LESSTHAN,
	GLSLBFID_LESSTHANEQUAL,
	GLSLBFID_GREATERTHAN,
	GLSLBFID_GREATERTHANEQUAL,
	GLSLBFID_EQUAL,
	GLSLBFID_NOTEQUAL,
	GLSLBFID_ANY,
	GLSLBFID_ALL,
	GLSLBFID_NOT,
	GLSLBFID_TEXTURE,
	GLSLBFID_TEXTUREPROJ,
	GLSLBFID_TEXTURELOD,
	GLSLBFID_TEXTUREPROJLOD,
	GLSLBFID_TEXTUREGRAD,
	GLSLBFID_TEXTUREPROJGRAD,
	GLSLBFID_TEXTURESTREAM,
	GLSLBFID_TEXTURESTREAMPROJ,
#if defined(SUPPORT_GL_TEXTURE_RECTANGLE)
	GLSLBFID_TEXTURERECT,
	GLSLBFID_TEXTURERECTPROJ,
#endif
	GLSLBFID_DFDX,
	GLSLBFID_DFDY,
	GLSLBFID_FWIDTH,
	GLSLBFID_NOISE1,
	GLSLBFID_NOISE2,
	GLSLBFID_NOISE3,
	GLSLBFID_NOISE4,

	GLSLBFID_NOT_BUILT_IN,

} GLSLBuiltInFunctionID;


/* Used for isnan & isinf */
#define F32_EXPONENT_MASK		0x7F800000
#define F32_CLEAR_SIGN_MASK		0x7FFFFFFF

typedef enum GLSLParamQualifierTAG
{
	GLSLPQ_INVALID          = 0x00000000,                              /* Indicates this was not a parameter */
	GLSLPQ_IN               = 0x00000001,
	GLSLPQ_OUT              = 0x00000002,
	GLSLPQ_INOUT            = 0x00000003,

} GLSLParameterQualifier;

extern const IMG_CHAR* const GLSLParameterQualifierFullDescTable[GLSLPQ_INOUT + 1];

typedef enum GLSLFunctionTypeTAG
{
	GLSLFT_INVALID                 = 0x00000000,
	GLSLFT_USER                    = 0x00000001,
	GLSLFT_CONSTRUCTOR             = 0x00000002,
	GLSLFT_USERDEFINED_CONSTRUCTOR = 0x00000003,
	GLSLFT_BUILT_IN                = 0x00000004,

} GLSLFunctionType;

typedef enum GLSLFunctionFlagsTAG
{
	GLSLFF_NOT_VALID                  = 0x00000000,
	GLSLFF_VALID_FOR_VERTEX_SHADERS   = 0x00000001,
	GLSLFF_VALID_FOR_FRAGMENT_SHADERS = 0x00000002,
	GLSLFF_VALID_FOR_GLSL             = 0x00000003,
	GLSLFF_VALID_FOR_GLSL_ES          = 0x00000004,
	GLSLFF_VALID_IN_ALL_CASES         = 0x7FFFFFFF,

} GLSLFunctionFlags;

typedef enum GLSLLValueStatusTAG
{
	GLSLLV_INVALID                    = 0x00000000,
	GLSLLV_NOT_L_VALUE                = 0x00000001,
	GLSLLV_NOT_L_VALUE_DUP_SWIZZLE    = 0x00000002,
	GLSLLV_L_VALUE                    = 0x00000003,

} GLSLLValueStatus;

extern const IMG_CHAR* const GLSLLValueStatusFullDescTable[GLSLLV_L_VALUE + 1];

typedef enum GLSLArrayStatusTAG
{
	GLSLAS_INVALID                    = 0x00000000,
	GLSLAS_NOT_ARRAY                  = 0x00000001,
	GLSLAS_ARRAY_SIZE_NOT_FIXED       = 0x00000002,
	GLSLAS_ARRAY_SIZE_FIXED           = 0x00000003,

} GLSLArrayStatus; 

extern const IMG_CHAR* const GLSLArrayStatusFullDescTable[GLSLAS_ARRAY_SIZE_FIXED + 1];

typedef enum GLSLIdentifierUsageTAG
{
	GLSLIU_WRITTEN             = 0x00000001,   /* Was the identifier ever written to? */
	GLSLIU_READ                = 0x00000002,   /* Was the identifier ever read from? */
	GLSLIU_CONDITIONAL         = 0x00000004,   /* Was the identifiers value ever conditionally checked? */
	GLSLIU_PARAM               = 0x00000008,   /* Was the identifier ever passed as a parameter? */ 
	GLSLIU_INIT_WARNED         = 0x00000010,   /* Warned that value is read before being written */
	GLSLIU_DYNAMICALLY_INDEXED = 0x00000020,   /* Was ithe identifier ever ndexed with a non static value */ 
	GLSLIU_INTERNALRESULT      = 0x00000040,   /* Was generated by the compiler */
	GLSLIU_UNINITIALISED       = 0x00000080,   /* May contain data but has not yet been assigned it yet */
	GLSLIU_ERROR_INITIALISING  = 0x00000100,   /* There was an error was trying to initalise the data for this identifier */
	GLSLIU_BUILT_IN            = 0x00000200,   /* This identifier was a built in (i.e. gl_Position) */

	GLSLIU_BOOL_AS_PREDICATE   = 0x00000400,   /* Setup if a bool can be represented as a prediate, by intermediate program */

} GLSLIdentifierUsage;



#ifdef COMPACT_MEMORY_MODEL 

typedef struct GLSLFullySpecifiedTypeTAG
{
	IMG_UINT32               eParameterQualifier      : 2;                             /* in, out, inout */
	IMG_UINT32               ePrecisionQualifier      : 3;							   /* high, medium, low */
	IMG_UINT32               eTypeQualifier           : 3;                             /* const, attribute, varying etc. */
	IMG_UINT32               eVaryingModifierFlags    : 3;                             /* centroid, invariant */
	IMG_UINT32               eTypeSpecifier           : 5;                             /* float, bool, int, matrix etc. */
	IMG_UINT32               uStructDescSymbolTableID : GLSL_NUM_BITS_FOR_SYMBOL_IDS;  /* If the type specifier was STRUCT then this is the symbol ID of the GLSLStructureDefinition */ 
	IMG_INT32                iArraySize;  

} GLSLFullySpecifiedType;

#else

typedef struct GLSLFullySpecifiedTypeTAG
{
	GLSLParameterQualifier   eParameterQualifier;       /* in, out, inout */
	GLSLPrecisionQualifier   ePrecisionQualifier;		/* high, medium, low */
	GLSLTypeQualifier        eTypeQualifier;            /* const, attribute, varying etc. */
	GLSLVaryingModifierFlags eVaryingModifierFlags;     /* centroid, invariant */
	GLSLTypeSpecifier        eTypeSpecifier;            /* float, bool, int, matrix etc. */
	IMG_UINT32               uStructDescSymbolTableID;  /* If the type specifier was STRUCT then this is the symbol ID of the GLSLStructureDefinition */
	IMG_INT32                iArraySize;                /* Size of array, -1 = array size not specified */

} GLSLFullySpecifiedType;

#endif // !defined COMPACT_MEMORY_MODEL

#define INIT_FULLY_SPECIFIED_TYPE(psFullySpecifiedType)                  \
	(psFullySpecifiedType)->eParameterQualifier      = GLSLPQ_INVALID;    \
	(psFullySpecifiedType)->ePrecisionQualifier      = GLSLPRECQ_UNKNOWN; \
	(psFullySpecifiedType)->eTypeQualifier           = GLSLTQ_INVALID;    \
	(psFullySpecifiedType)->eVaryingModifierFlags    = GLSLVMOD_NONE;     \
	(psFullySpecifiedType)->eTypeSpecifier           = GLSLTS_INVALID;    \
	(psFullySpecifiedType)->uStructDescSymbolTableID = 0;                 \
	(psFullySpecifiedType)->iArraySize               = 0;


/* 
   Use this to dereference data with description GLSLSTDT_IDENTIFIER 

   NOTE - Size of this structure seems to be important for perfomace - 11 DWORDS is slow 8 or 12 is fine 
*/
#ifdef COMPACT_MEMORY_MODEL

typedef struct GLSLIdentifierDataTAG
{
	GLSLSymbolTableDataType   eSymbolTableDataType;
	GLSLFullySpecifiedType    sFullySpecifiedType;
	IMG_INT32                 iActiveArraySize;

	IMG_UINT32                eArrayStatus                 : 2;
	IMG_UINT32                eLValueStatus                : 2; 
	IMG_UINT32                eBuiltInVariableID           : 7;
	IMG_UINT32                eIdentifierUsage             : 11;
	IMG_UINT32                                             : 10;
	IMG_UINT32                uConstantDataSize            : 32 - GLSL_NUM_BITS_FOR_SYMBOL_IDS;
	IMG_UINT32                uConstantAssociationSymbolID : GLSL_NUM_BITS_FOR_SYMBOL_IDS; 

	IMG_VOID                 *pvConstantData;

} GLSLIdentifierData;

#else

typedef struct GLSLIdentifierDataTAG
{
	GLSLSymbolTableDataType		eSymbolTableDataType;
	GLSLFullySpecifiedType		sFullySpecifiedType;
	IMG_INT32					iActiveArraySize;
	GLSLArrayStatus				eArrayStatus;
	GLSLLValueStatus			eLValueStatus;
	GLSLBuiltInVariableID		eBuiltInVariableID;
	GLSLIdentifierUsage			eIdentifierUsage;
	IMG_UINT32					uConstantDataSize;
	IMG_UINT32					uConstantAssociationSymbolID;
	IMG_VOID					*pvConstantData;

} GLSLIdentifierData;

#endif // COMPACT_MEMORY_MODEL

typedef struct GLSLPrecisionModifierTAG
{
	GLSLSymbolTableDataType   eSymbolTableDataType;
	GLSLPrecisionQualifier    ePrecisionQualifier;		 /* high, medium, low */
	GLSLTypeSpecifier         eTypeSpecifier;            /* float, bool, int, matrix etc. */
} GLSLPrecisionModifier;


/* Use this to dereference data with description GLSLSTDT_FUNCTION_CALL */
typedef struct GLSLFunctionCallDataTAG
{
	GLSLSymbolTableDataType eSymbolTableDataType;        /* GLSLSTDT_FUNCTION_CALL */
	GLSLFullySpecifiedType  sFullySpecifiedType;         /* Return type of function called */
	GLSLArrayStatus         eArrayStatus;
	IMG_UINT32              uFunctionDefinitionSymbolID; /* Symbol ID of the GLSLFunctionDefinitionData description of the function being called */
	IMG_UINT32              uLoopLevel;                  /* Loop level function was called at */
	IMG_UINT32              uConditionLevel;             /* Condition level function was called at */
} GLSLFunctionCallData;

/* Use this to dereference data with description GLSLSTDT_MEMBER_SELECTION */
typedef struct GLSLMemberSelectionDataTAG
{
	GLSLSymbolTableDataType  eSymbolTableDataType;
	IMG_UINT32               uMemberOffset;
	IMG_UINT32               uStructureInstanceSymbolTableID; /* Symbol ID of GLSLIdentifierData, GLSLParameterData, GLSLResultData or GLSLFunctionCallData
																 used to declare this instance of the structure */
} GLSLMemberSelectionData;

typedef struct GLSLStructureMemberTAG
{
	IMG_CHAR                *pszMemberName;
	IMG_UINT32               uConstantDataOffSetInBytes;   
	GLSLIdentifierData       sIdentifierData;
} GLSLStructureMember;

/* Use this to dereference data with description GLSLSTDT_STRUCTURE_DEFINITION */
typedef struct GLSLStructureDefinitionDataTAG
{
	GLSLSymbolTableDataType		eSymbolTableDataType;
	IMG_UINT32					uStructureSizeInBytes;
	IMG_UINT32					uNumMembers;
	GLSLStructureMember			*psMembers;
	IMG_BOOL					bContainsSamplers;
} GLSLStructureDefinitionData;


typedef struct GLSLGenericDataTAG
{
	GLSLSymbolTableDataType eSymbolTableDataType;
} GLSLGenericData;

/* Use this to dereference data with description GLSLSTDT_FUNCTION_DEFINITION */
typedef struct GLSLFunctionDefinitionDataTAG
{
	GLSLSymbolTableDataType  eSymbolTableDataType;
	IMG_CHAR                *pszOriginalFunctionName;       /* Unhashed version of the function name */
	GLSLFunctionType         eFunctionType;
	GLSLFunctionFlags        eFunctionFlags;
	GLSLFullySpecifiedType   sReturnFullySpecifiedType;     /* Description of the value returned by this function (to save having to use ID below) */
	IMG_UINT32               uReturnDataSymbolID;           /* Symbol ID of the GLSLIdentifierData that has been allocated to this functions return value */
	IMG_BOOL                 bPrototype;                    /* TRUE when the function has only been prototyped, FALSE when it has a body */
	IMG_UINT32               uFunctionCalledCount;          /* > 0 if this function has ever neen called - Used for optimisations and to decide wether to raise an error when a fn has been prototyped only */
	IMG_UINT32               uMaxFunctionCallDepth;         /* Stores the deepest nested level at which this function was called */
	IMG_UINT32              *puCalledFunctionIDs;           /* Stores the Symbol IDs of all functions called from this one */
	IMG_UINT32               uNumCalledFunctions;           /* The number of functions are called from this one */
	IMG_BOOL                 bGradientFnCalled;             /* True if this function called a function that relies on gradient calcs */
	IMG_BOOL                 bCalledFromConditionalBlock;   /* True if this function was called from inside a loop/conditional block */
	IMG_UINT32               uNumParameters;                
	GLSLBuiltInFunctionID    eBuiltInFunctionID;            /* If this is a built in function this indicates which one */
	IMG_UINT32              *puParameterSymbolTableIDs;     /* Symbol IDs of the GLSLParameterData that has been allocated to this functions parameters  */
	GLSLFullySpecifiedType  *psFullySpecifiedTypes;         /* Used when the function has been prototyped only */

} GLSLFunctionDefinitionData;


/* Use this to dereference data with description GLSLSTDT_SWIZZLE */
typedef struct GLSLSwizzleDataTAG
{
	GLSLSymbolTableDataType  eSymbolTableDataType;
	IMG_UINT32               uNumComponents;
	IMG_UINT32               uComponentIndex[4];
} GLSLSwizzleData;


typedef struct GLSLNodeTAG
{
	GLSLNodeType				eNodeType;			/* Describes type of node */
	struct GLSLNodeTAG	       *psParent;			/* Parent Node*/
	IMG_UINT32                  uNumChildren;		/* Number of children this node has */
	struct GLSLNodeTAG        **ppsChildren;		/* Pointer to array of child nodes */
	IMG_UINT32                  uSymbolTableID;		/* Symbol table ID for any data associated with this node, 0 = No symbol table entry */
	Token                      *psToken;			/* Token associated with this parse tree entry */
	IMG_BOOL                    bEvaluated;			/* Has this node been evaluated yet, used by IC generation */
	struct GLSLNodeTAG		   *psNext;				/* Nodes are stored in a linked list, because for BISON the root of the abstract syntax tree is not 
													   set if there was a parse error. Nodes therefore need to be recorded in a linked list to clean up. */
} GLSLNode;


struct GLSLICOperandInfoTAG;
#define GLSLICOperandInfo struct GLSLICOperandInfoTAG

typedef IMG_VOID (*PFNProcessBuiltInFunction)(GLSLCompilerPrivateData *psCPD, 
											  GLSLICProgram *psICProgram, 
											  GLSLNode *psNode, 
											  GLSLICOperandInfo *psDestOperand);

typedef struct GLSLBuiltInFunctionInfoTAG
{
#ifdef DEBUG
	GLSLBuiltInFunctionID			eFunctionID;
	#define GLSLBFID_(id)			GLSLBFID_##id,
#else
	#define GLSLBFID_(id)			
#endif
	IMG_UINT8						bReducibleToConst;
	IMG_UINT8						bUseGradients;
	IMG_UINT8						bTextureSampler;
	PFNProcessBuiltInFunction		pfnProcessBuiltInFunction;

} GLSLBuiltInFunctionInfo;

extern const GLSLBuiltInFunctionInfo						asGLSLBuiltInFunctionInfoTable[];
#ifdef DEBUG
#define GLSLBuiltInFunctionID(functionID)					asGLSLBuiltInFunctionInfoTable[functionID].eFunctionID
#endif
#define GLSLBuiltInFunctionReducibleToConst(functionID)		asGLSLBuiltInFunctionInfoTable[functionID].bReducibleToConst
#define GLSLBuiltInFunctionUseGradients(functionID)			asGLSLBuiltInFunctionInfoTable[functionID].bUseGradients
#define GLSLBuiltInFunctionTextureSampler(functionID)		asGLSLBuiltInFunctionInfoTable[functionID].bTextureSampler
#define GLSLBuiltInFunctionProcessFunction(functionID)		asGLSLBuiltInFunctionInfoTable[functionID].pfnProcessBuiltInFunction

typedef struct GLSLPrototypedFunctionTAG
{
	IMG_UINT32                        uSymbolTableID;
	struct GLSLPrototypedFunctionTAG *psNext;
	struct GLSLPrototypedFunctionTAG *psPrev;
} GLSLPrototypedFunction;

typedef struct GLSLIdentifierListTAG
{
	IMG_UINT32                   *puIdentifiersReferenced;
	IMG_UINT32                    uNumIdentifiersReferenced;
	IMG_UINT32                    uIdentifiersReferencedListSize;
} GLSLIdentifierList;

typedef struct GLSLTreeContextTAG
{
	IMG_UINT32                    uNumResults;
	IMG_UINT32                    uNumUnnamedStructures;
	IMG_UINT32                    uCurrentFunctionDefinitionSymbolID;
	IMG_BOOL                      bFunctionReturnStatus;
	IMG_UINT32                    uLoopLevel;
	IMG_UINT32                    uConditionLevel;
	GLSLProgramType               eProgramType;
	GLSLInitCompilerContext      *psInitCompilerContext;
	GLSLPrecisionQualifier        eDefaultFloatPrecision;
	GLSLPrecisionQualifier        eDefaultIntPrecision;
	GLSLPrecisionQualifier        eDefaultSamplerPrecision;
	GLSLPrecisionQualifier        eForceUserFloatPrecision;
	GLSLPrecisionQualifier        eForceUserIntPrecision;
	GLSLPrecisionQualifier        eForceUserSamplerPrecision;
	IMG_BOOL                      bShaderHasModifiedPrecision;
	ParseContext                 *psParseContext;
	SymTable                     *psSymbolTable;
	GLSLNode                     *psAbstractSyntaxTree;
	GLSLNode					 *psNodeList;
	GLSLBuiltInVariableWrittenTo  eBuiltInsWrittenTo;
	GLSLIdentifierList           *psBuiltInsReferenced;
	IMG_UINT32                   *puBuiltInFunctionsCalled;
	IMG_UINT32                    uNumBuiltInFunctionsCalled;
	GLSLNode                     *psMainFunctionNode;
	IMG_BOOL                      bDiscardExecuted;
	GLSLPrototypedFunction       *psPrototypedFunction;
	GLSLCompilerWarnings          eEnabledWarnings;
	GLSLExtension				  eEnabledExtensions;
	IMG_UINT32                    uSupportedLanguageVersion;
	/* 
		In case we cant derive a precision for builtin function parameters, save the fallback precision here. 
		This happens if you do for example:
			highp float ftemp;
			ftemp = sin( 0.0 );	
		The un-precisioned parameter gets the precision from the "next consuming operation", which is the identifier
		that stores the return value.
	*/
	GLSLPrecisionQualifier		  eNextConsumingOperationPrecision;
} GLSLTreeContext;

 
/* Compiler private data */
struct GLSLCompilerPrivateDataTAG
{
	SymbolTableContext         *psSymbolTableContext;
	SymTable                   *psVertexSymbolTable;
	SymTable                   *psFragmentSymbolTable;
	GLSLIdentifierList          sVertexBuiltInsReferenced;
	GLSLIdentifierList          sFragmentBuiltInsReferenced;
	IMG_VOID                   *pvUniFlexContext;
	ErrorLog                   *psErrorLog;
	
	/*
	 Source line number in code for which the current instruction is generated. 
	*/
	IMG_UINT32	uCurSrcLine;


#ifdef METRICS
	IMG_UINT32                  au32Time[METRICS_LASTMETRIC][2];
	IMG_UINT32                  au32Counter[METRICCOUNT_LASTMETRIC];
	IMG_FLOAT                   fCPUSpeed;
#endif

	GLSLCompilerResources		*psCompilerResources;

};

GLSLTreeContext *CreateGLSLTreeContext(ParseContext            *psParseContext,
									   SymTable                *psSymbolTable,
									   GLSLProgramType          eProgramType,
									   GLSLCompilerWarnings     eEnabledWarnings,
									   GLSLInitCompilerContext *psInitCompilerContext);

IMG_VOID DestroyGLSLTreeContext(GLSLTreeContext *psGLSLTreeContext);

IMG_BOOL CheckGLSLTreeCompleteness(GLSLTreeContext *psGLSLTreeContext);

IMG_BOOL ASTCheckFeatureVersion(GLSLTreeContext		*psGLSLTreeContext,
							   ParseTreeEntry		*psParseTreeEntry,
							   IMG_UINT32			uFeatureVersion,
							   const IMG_CHAR		*pszKeyword,
							   const IMG_CHAR		*pszFeatureDescription /* Optional */ );

typedef struct ASTFunctionStateParamTAG
{
	GLSLIdentifierData				sParameterData;
	ParseTreeEntry					*psParamIDENTIFIEREntry;
	struct ASTFunctionStateParamTAG	*psNext;
} ASTFunctionStateParam;

typedef struct ASTFunctionStateTAG
{
	ParseTreeEntry				*psIDENTIFIEREntry;
	GLSLIdentifierData			sReturnData;
	IMG_UINT32					uNumParameters;
	ASTFunctionStateParam		*psParameters;
} ASTFunctionState;

typedef struct ASTFunctionCallStateTAG
{
	GLSLNode			*psFunctionCallNode;
	IMG_BOOL			bConstructor;
	GLSLTypeSpecifier	eConstructorTypeSpecifier;
	IMG_INT32			iConstructorArraySize;
} ASTFunctionCallState;

GLSLNode *ASTCreateIDENTIFIERUseNode(GLSLTreeContext *psGLSLTreeContext,
									 ParseTreeEntry  *psIDENTIFIEREntry);

GLSLNode *ASTCreateBOOLCONSTANTNode(GLSLTreeContext *psGLSLTreeContext,
									ParseTreeEntry  *psBOOLCONSTANTEntry);

GLSLNode *ASTCreateINTCONSTANTNode(GLSLTreeContext *psGLSLTreeContext,
								   ParseTreeEntry  *psINTCONSTANTEntry);

GLSLNode *ASTCreateFLOATCONSTANTNode(GLSLTreeContext  *psGLSLTreeContext,
									 ParseTreeEntry   *psFLOATCONSTANTEntry);

IMG_BOOL ASTRegisterFunctionCall(GLSLTreeContext            *psGLSLTreeContext,
								 GLSLFunctionDefinitionData	*psCalledFunctionData,
								 IMG_UINT32                  uCalledFunctionSymbolTableID);

IMG_UINT32 ASTFindFunction(GLSLTreeContext         *psGLSLTreeContext,
						   GLSLNode                *psFunctionCallNode,
						   IMG_CHAR                *pszFunctionName,
						   GLSLFullySpecifiedType  *psFullySpecifiedTypes);

GLSLNode *ASTCreateFunctionNode(GLSLTreeContext  *psGLSLTreeContext,
								ASTFunctionState *psFunctionState,
								IMG_BOOL          bPrototype);

IMG_BOOL ASTDecreaseScope(GLSLTreeContext *psGLSLTreeContext);

IMG_BOOL ASTIncreaseScope(GLSLTreeContext *psGLSLTreeContext);

GLSLNode *ASTProcessFieldSelection(GLSLTreeContext   *psGLSLTreeContext,
								   ParseTreeEntry    *psFieldSelectionEntry,
								   GLSLNode          *psLeftNode);

GLSLNode *ASTCreateIDENTIFIERNode(GLSLTreeContext        *psGLSLTreeContext,
								  ParseTreeEntry         *psIDENTIFIEREntry,
								  IMG_BOOL                bDeclaration,
								  GLSLFullySpecifiedType *psFullySpecifiedType);

GLSLNode *ASTProcessLanguageVersion(GLSLTreeContext *psGLSLTreeContext,
									ParseTreeEntry  *psLanguageVersionEntry);

IMG_BOOL ASTUpdateInvariantStatus(GLSLTreeContext *psGLSLTreeContext,
								  ParseTreeEntry  *psIDENTIFIEREntry);

IMG_VOID ASTAddFunctionState(GLSLCompilerPrivateData	*psCPD,
								ASTFunctionState		*psFunctionState,
								ParseTreeEntry			*psParamIDENTIFIEREntry,
								GLSLIdentifierData		*psParameterData);

IMG_BOOL ModifyDefaultPrecision(GLSLTreeContext			*psGLSLTreeContext,
								GLSLPrecisionQualifier	ePrecisionQualifier,
								GLSLTypeSpecifier       eTypeSpecifier);

IMG_BOOL IsSamplerTypeSupported(GLSLTreeContext					*psGLSLTreeContext,
								const ParseTreeEntry			*psFullySpecifiedTypeEntry,
								const GLSLFullySpecifiedType	*psFullySpecifiedType);


IMG_BOOL GLSLCompileToIntermediateCode(GLSLCompileProgramContext  *psCompileProgramContext,
										GLSLICProgram			 **ppsICProgram,
										GLSLProgramFlags			*peProgramFlags,
										ErrorLog					*psErrorLog);

IMG_BOOL GLSLFreeIntermediateCode(GLSLCompileProgramContext *psCompileProgramContext,
								 GLSLICProgram			 *psICProgram);



#endif //__gl_glsltree_h_

/******************************************************************************
 End of file (glsltree.h)
******************************************************************************/
