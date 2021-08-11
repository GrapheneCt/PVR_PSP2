/**************************************************************************
 * Name         : icuf.h
 * Created      :
 *
 * Copyright    : 2005 by Imagination Technologies Limited. All rights reserved.
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
 * Platform     : ANSI
 *
 * Modifications:-
 * $Log: icuf.h $
 **************************************************************************/
#define OGLES2_VARYINGS_PACKING_RULE	1

/* Round a number up to the next multiple of a given power of two. */
#define ROUND_UP(X, B) (((X) + ((B) - 1)) & ~((B) - 1))

/*
	Another way to present swiz, it is extensively used in icemul.c 
*/
#define SWIZ_X		1 | (GLSLIC_VECCOMP_X << 4) 
#define SWIZ_Y		1 |	(GLSLIC_VECCOMP_Y << 4)
#define SWIZ_Z		1 |	(GLSLIC_VECCOMP_Z << 4)
#define SWIZ_W		1 |	(GLSLIC_VECCOMP_W << 4)
#define SWIZ_NA		0
#define SWIZ_XY		2 |	(GLSLIC_VECCOMP_X << 4) | (GLSLIC_VECCOMP_Y << 6)	
#define SWIZ_XYZ	3 |	(GLSLIC_VECCOMP_X << 4) | (GLSLIC_VECCOMP_Y << 6) | (GLSLIC_VECCOMP_Z << 8)	
#define SWIZ_NUM(swiz)		(swiz & 0x7)
#define SWIZ_0COMP(swiz)	(GLSLICVecComponent)((swiz >> 4) & 0x3)
#define SWIZ_1COMP(swiz)	(GLSLICVecComponent)((swiz >> 6) & 0x3)
#define SWIZ_2COMP(swiz)	(GLSLICVecComponent)((swiz >> 8) & 0x3)
#define SWIZ_3COMP(swiz)	(GLSLICVecComponent)((swiz >> 10) & 0x3)

/*
	Macro to work out UF register number and component start
*/
#define REG_INDEX(comp)			((comp) / REG_COMPONENTS)
#define REG_COMPSTART(comp)		((comp) % REG_COMPONENTS)
#define REG_COUNT(compcount)	(((compcount) + REG_COMPONENTS - 1)/REG_COMPONENTS)


#define MASK_X		0x1
#define MASK_Y		0x2
#define MASK_Z		0x4
#define MASK_W		0x8
#define MASK_XYZW   (MASK_X | MASK_Y | MASK_Z | MASK_W)




/*
	UniFlex register assignment table
	There are temp, attrib, texcoord, float constant tables.
*/
typedef struct UFRegAsssignmentTableTAG
{
	IMG_UINT32 uMaxRegisters;

	IMG_UINT32 auNextAvailRegHighP[4];
	IMG_UINT32 auNextAvailRegMediumP[4];

	IMG_UINT32 uNextAvailVec4Reg;

} UFRegAsssignmentTable;


#ifdef OGLES2_VARYINGS_PACKING_RULE
typedef struct GLSLTexCoordsTableTAG
{
	IMG_UINT8					auAllocatedBits[ROUND_UP(NUM_TC_REGISTERS * REG_COMPONENTS, 8) / 8];

	/*
		The bit corresponding to a texture coordinate is set if that texture coordinate has
		been (partially or completely) allocated to a dynamically indexed object which spans
		more than one texture coordinate.
	*/
	IMG_UINT8					auDynamicallyIndexed[ROUND_UP(NUM_TC_REGISTERS, 8) / 8];

	IMG_UINT32					auNextAvailRow;

	IMG_BOOL					bVaryingsPacked;

	GLSLPrecisionQualifier		auTexCoordPrecisions[NUM_TC_REGISTERS];
	GLSLVaryingModifierFlags	aeTexCoordModidifierFlags[NUM_TC_REGISTERS];
}GLSLTexCoordsTable;

#endif

/* 
	Sampler assignment table 
*/
typedef struct GLSLSamplerAssignmentTabTAG
{
	IMG_PUINT32				auSamplerToSymbolId;

}GLSLSamplerAssignmentTab;



typedef struct GLSLStructMemberAllocTAG
{
	GLSLFullySpecifiedType sFullType;
	IMG_UINT32	uNumValidComponents;

	IMG_UINT32  uCompOffset;
	IMG_UINT32	uTextureUnitOffset;		/* Only needed for structures and samplers. */

	IMG_UINT32  uAllocCount;
	IMG_UINT32  uTotalAllocCount;

	IMG_UINT32	iArraySize;
	IMG_UINT32  uCompUseMask;

	struct GLSLStructureAllocTAG *psStructAlloc; /* NULL if not a struct */
	
}GLSLStructMemberAlloc;

/* 
	Struct allo info table entry
*/
typedef struct GLSLStructureAllocTAG
{
	GLSLFullySpecifiedType sFullType;

#ifdef DEBUG
	IMG_CHAR				*psStructName;
#endif

	IMG_UINT32				uNumMembers;
	GLSLStructMemberAlloc	*psMemberAlloc;	

	IMG_UINT32				uNumBaseTypeMembers;
	GLSLStructMemberAlloc	*psBaseMemberAlloc;	/* You determine if it contains a substruct by checking if psMemberAlloc != psBaseMemberAlloc */

	IMG_UINT32				uOverallAllocSize;
	IMG_UINT32				uTotalTextureUnits;	/* Allow texture samplers in structs */
	IMG_UINT32				uTotalValidComponents;

	struct GLSLStructureAllocTAG *psNext;

}GLSLStructureAlloc;


/* 
	Struct info table
*/
typedef struct GLSLStructAllocInfoTableTAG
{
	GLSLStructureAlloc		*psStructAllocHead;
	GLSLStructureAlloc		*psStructAllocTail;
}GLSLStructAllocInfoTable;

typedef enum GLSLHWSymbolUsageTAG
{
	GLSLSU_DYNAMICALLY_INDEXED	= 0x00000001,	/* The symbol has been dynamically indexed */
	GLSLSU_BOOL_AS_PREDICATE	= 0x00000002,	/* The boolean symbol is a predicate */
	GLSLSU_FIXED_REG			= 0x00000004,	/* The symbol is a fixed register */
	GLSLSU_SAMPLER				= 0x00000008,	/* The symbol is a sampler */
	GLSLSU_FULL_RANGE_INTEGER	= 0x00000010,	/* The symbol is an integer with full range, 
												   positive integer only if it is not set */ 


}GLSLHWSymbolUsage;

/*
	HW symbol table entry for an instance.
	Each table entry contains allocation info for a symbol ID
*/
typedef struct HWSYMBOL_TAG
{
	/* The two variables are used to identify one symbol */
	IMG_UINT32				uSymbolId;				/* Symbol ID */
	IMG_BOOL				bDirectRegister;		/* Should use direct register for varyings and attributes */

	/* The initial information collected from it symbol table ID */
#if defined(DEBUG)
	IMG_CHAR				*psSymbolName;			/* Symbol name */
#endif
	GLSLBuiltInVariableID	eBuiltinID;				/* Built-in ID */
	GLSLFullySpecifiedType	sFullType;				/* Fully specified type */
	IMG_INT32				iArraySize;				/* Array size, 0 if it is not an array */
	
	/* The information is colloected during examine */
	GLSLHWSymbolUsage		eSymbolUsage;			/* Symbol usage */

	/* Indicate whether registers have been allocated or not, if true, the rest of information should be valid */
	IMG_BOOL				bRegAllocated;

	IMG_UINT32				uIndexableTempTag;		/* Indexable temporary tag, for indexable temps only */
	UF_REGTYPE				eRegType;				/* Register type */
	IMG_UINT32				uAllocCount;			/* Alloc count - num of reg components(floats) required for the type */ 
	IMG_UINT32				uTotalAllocCount;		/* Total alloc count for the symbol, it is not always equal to uAllocCount * uArraySize */
	IMG_UINT32				uCompUseMask;			/* Type component use mask, for one element if it is array */

	/* Which register / component is the first component for the symbol? */
	union
	{
		IMG_UINT32	uCompOffset;		/* Component offset in the register bank */ 		

		IMG_UINT32	uFixedRegNum;		/* Fixed number for the fixed registers such as position, fog, color, color output etc. */
	} u;

	IMG_UINT32  uTextureUnit;			/* Texture unit assigned to sampler. Structs can also contain samplers, so this is used for struct objects as well. */

	struct HWSYMBOL_TAG		*psNext;
} HWSYMBOL;

/* 
	HW symbol table, each table entry contain allocation info for a symbol id
*/
typedef struct GLSLHWSymbolTabTAG
{
	/* Linked list of HW symbols */
	HWSYMBOL *psHWSymbolHead, *psHWSymbolTail;

	/* Store array of HW symbol pointers for varyings */
	IMG_UINT32 uNumVaryings;
	IMG_UINT32 uMaxNumVarings;
	HWSYMBOL   **apsVaryingList;

}GLSLHWSymbolTab;


/* 
	A UniFlex context used to create UF code
*/
typedef struct GLSLUniFlexContextTAG
{	
	/* GLSL IC program related variables */
	GLSLICProgram			*psICProgram;
	GLSLICInstruction		*psCurrentICInst;

	GLSLProgramType			eProgramType;
	SymTable     			*psSymbolTable;

	/* ===================================================== 
		Some global variables used during UF code generation 
	======================================================*/
	GLSLVaryingMask			eActiveVaryingMask;
	IMG_UINT32				auTexCoordDims[NUM_TC_REGISTERS];
	GLSLPrecisionQualifier  aeTexCoordPrecisions[NUM_TC_REGISTERS];
	IMG_UINT32				uTexCoordCentroidMask;
	
	IMG_BOOL				bFrontFacingUsed;
	IMG_BOOL				bColorUsed;
	IMG_BOOL				bSecondaryColorUsed;
	IMG_BOOL				bFragCoordUsed;

	/* Component use mask for all type specifiers, unpacked and packed for mat2x2, mat3x2, mat4x2 */
	IMG_UINT32				auCompUseMask[GLSLTS_NUM_TYPES];

	IMG_BOOL				bRegisterLimitExceeded;

	UNIFLEX_INST			*psFirstUFInst;
	UNIFLEX_INST			*psEndUFInst;

	IMG_UINT32				auLabelToSymbolId[UF_MAX_LABEL];
	
	/* Register assign table for float constant, integer constant, boolean constant and temp and texcoord */
	UFRegAsssignmentTable	sFloatConstTab;
	UFRegAsssignmentTable	sTempTab;
	UFRegAsssignmentTable	sAttribTab;	
	UFRegAsssignmentTable	sPredicateTab;
	UFRegAsssignmentTable	sFragmentOutTab;
	IMG_UINT32				uNextArrayTag;

#ifdef OGLES2_VARYINGS_PACKING_RULE
	/* Varyings */
	GLSLTexCoordsTable		sTexCoordsTable;
#else
	UFRegAsssignmentTable	sTexCoordTab;
#endif

	GLSLSamplerAssignmentTab sSamplerTab;

	GLSLHWSymbolTab			sHWSymbolTab;
	GLSLStructAllocInfoTable sStructAllocInfoTable;

	IMG_UINT32				uLoopNestCount;
	IMG_UINT32				uIfNestCount;
	IMG_BOOL				bFirstRet;

	/* Constant ranges */
	UNIFLEX_RANGES_LIST		sConstRanges;

	/* Varying ranges */
	UNIFLEX_RANGES_LIST		sVaryingRanges;

	/* Static constants */
	IMG_UINT32				uConstStaticFlagCount;
	IMG_UINT32				*puConstStaticFlags;

	/* Indexable temp array, and size of each indexable temporary array. */
	IMG_UINT32				uIndexableTempArrayCount;
	UNIFLEX_INDEXABLE_TEMP_SIZE	*psIndexableTempArraySizes;

	/* Number of samplers used. */
	IMG_UINT32				uSamplerCount;

	/* Sampler dimension info */
	PUNIFLEX_DIMENSIONALITY	asSamplerDims;

	#ifdef SRC_DEBUG
	/* Current source line number for which code will be generated. */
	IMG_UINT32			uCurSrcLine;
	
	/* Just added here to handle DebugAssert psCPD definition requirment in CreateInstruction */
	GLSLCompilerPrivateData *psCPD;
	#endif /* SRC_DEBUG */

}GLSLUniFlexContext;


typedef struct ICUFOperandTAG
{
	/* Some Information from IC operand */
	GLSLICVecSwizWMask	sICSwizMask;			/* IC Swiz Mask */
	GLSLICModifier		eICModifier;
	IMG_UINT32			uSymbolID;

	GLSLFullySpecifiedType sFullType;			/* Fully specified type, this doesn't consider the swiz mask*/

	UF_REGTYPE			eRegType;				/* Register type */
	IMG_UINT32			uRegNum;				/* Register index */
	UF_REGFORMAT		eRegFormat;				/* Register format: F32, F16 or C10 */
	IMG_UINT32			uCompStart;				/* 0, 1, 2, 3 */
	IMG_UINT32			uAllocCount;			/* Number of components required for an array element */
	IMG_INT32			iArraySize;				/* Is it array and arraysize */
	
	GLSLTypeSpecifier	eTypeAfterSwiz;			/* Type after swiz, if sFullType is not a vector, then eTypeAfSwiz == sFullType.eTypeSpecifier */
	IMG_UINT32			uUFSwiz;				/* UF swiz which has combined compstart and icode swiz */

	UFREG_RELATIVEINDEX eRelativeIndex;	
	IMG_UINT32			uRelativeStrideInComponents;

	IMG_UINT32			uArrayTag;

}ICUFOperand;

GLSLStructureAlloc* GetStructAllocInfo(	GLSLCompilerPrivateData			*psCPD,
										GLSLUniFlexContext				*psUFContext,
										const GLSLFullySpecifiedType	*psFullType);

IMG_UINT32 UFRegATableGetMaxComponent(UFRegAsssignmentTable *psTable);

GLSLBindingSymbolList *GenerateBindingSymbolList(GLSLCompilerPrivateData *psCPD, GLSLUniFlexContext *psUFContext);
