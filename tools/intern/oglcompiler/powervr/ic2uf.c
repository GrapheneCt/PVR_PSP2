/******************************************************************************
 Name           : ic2uf.c

 Title          :

 C Author       : Daoxiang Gong

 Created        : 21 July 2005

 Copyright      : 2005-2007 by Imagination Technologies Limited.
                  All rights reserved. No part of this software, either
                  material or conceptual may be copied or distributed,
                  transmitted, transcribed, stored in a retrieval system or
                  translated into any human or computer language in any form
                  by any means, electronic, mechanical, manual or otherwise,
                  or disclosed to third parties without the express written
                  permission of Imagination Technologies Limited,
                  Home Park Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.

 Description    : Convert from the GLSL intermediate language to UniFlex code

 Program Type   : 32-bit DLL

 Modifications  :
 $Log: ic2uf.c $
 
 Buildfix.
  --- Revision Logs Removed --- 
******************************************************************************/
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "../parser/debug.h"
#include "../glsl/error.h"
#include "../glsl/icode.h"
#include "../glsl/semantic.h"
#include "../glsl/common.h"
#include "../glsl/icgen.h"
#include "../glsl/astbuiltin.h"

#include "ic2uf.h"
#include "icuf.h"

#include "usc.h"
#include "bitops.h"
#include "use.h"


/*
	Defines for how to allocate registers
	Note: Attributes can not be packed at the moment
*/
#define PACK_CONSTS			0	/* Muiplte consts/uniforms are packed to vec4s by enabling this flag */
#define PACK_TEMPS			0	/* Mutiple temps are packed to vec4s by enabling this flag */
#define SCALARS_IN_ALPHA	1	/* Put const and temp scalars in alpha channel, only available when not packing consts or temps */

#ifdef GLSL_ES
#define PACK_VARYING_ARRAY	0	/* When enabling, array of scalars/ver2s are packed (also include matrices with 2 rows)*/ 
#else
#define PACK_VARYING_ARRAY	1 
#endif

/* 
	Three defines to control how to compare non-sclars 
*/
#define USE_PREDICATED_COMPARISON		1	/* Use predicate to generate non-scalar comparison, otherwise convert all results to temp bools */

#define USE_IFBLOCKS_FOR_COMPARISON		0	/* Use IF/ENFIF block if true, otherwise use predicate */ 

#define MAX_COMPONENTS_UNROLLING_ARRAY_LOOP	16

#define MAX_LOOP_NESTING 4


#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define UFREG_SWIZ_GBAA		UFREG_ENCODE_SWIZ(UFREG_SWIZ_G, UFREG_SWIZ_B, UFREG_SWIZ_A, UFREG_SWIZ_A)
#define UFREG_SWIZ_BAAA		UFREG_ENCODE_SWIZ(UFREG_SWIZ_B, UFREG_SWIZ_A, UFREG_SWIZ_A, UFREG_SWIZ_A)
#define UFREG_SWIZ_ZW00		UFREG_ENCODE_SWIZ(UFREG_SWIZ_Z, UFREG_SWIZ_W, UFREG_SWIZ_0, UFREG_SWIZ_0)

#define INVALID_ARRAY_TAG	0
#define REG_NO_ALIGNMENT	(IMG_UINT32)(-1)

#define IS_TEXTURE_LOOKUP_OP(a)		(a >= UFOP_LD && a <= UFOP_LDD)

#define ENCODE_PREDICATE(Idx, Comp) (Idx | ((Comp + 0x4) << UF_PRED_COMP_SHIFT))

/*
	=====================
	Some global constants
	=====================
*/

/* ZERO */
static const UF_REGISTER g_sSourceZero = 
{
	HW_CONST_0,					/* uNum */
	UFREG_TYPE_HW_CONST,		/* eType */
	UF_REGFORMAT_F32,			/* eFormat */
	{UFREG_SWIZ_NONE},			/* uSiz\byMask union */
	UFREG_SMOD_NONE,			/* byMod */
	UFREG_RELATIVEINDEX_NONE,	/* eRelativeIndex */
	0,							/* eRelativeType */
	0,							/* uRelativeNum */
	0,							/* byRelativeChannel */
	INVALID_ARRAY_TAG,			/* uArrayTag */
	{0}							/* uAddressAlignment */
};

/* ONE */
static const UF_REGISTER g_sSourceOne =
{
	HW_CONST_1,
	UFREG_TYPE_HW_CONST,
	UF_REGFORMAT_F32,
	{UFREG_SWIZ_NONE},
	UFREG_SMOD_NONE,
	UFREG_RELATIVEINDEX_NONE,
	0,
	0,
	0,
	INVALID_ARRAY_TAG,
	{0}
};

/* -1 */
static const UF_REGISTER g_sSourceMinusOne =
{
	HW_CONST_1,
	UFREG_TYPE_HW_CONST,
	UF_REGFORMAT_F32,
	{UFREG_SWIZ_NONE},
	UFREG_SOURCE_NEGATE,
	UFREG_RELATIVEINDEX_NONE,
	0,
	0,
	0,
	INVALID_ARRAY_TAG,
	{0}
};

static const IMG_UINT32 g_auSingleComponentSwiz[] =
{
	UFREG_SWIZ_RRRR,
	UFREG_SWIZ_GGGG,
	UFREG_SWIZ_BBBB,
	UFREG_SWIZ_AAAA
};

static const IMG_UINT32 g_auRestOfColumnSwiz[] =
{
	UFREG_SWIZ_RGBA,
	UFREG_SWIZ_GBAA,
	UFREG_SWIZ_BAAA,
	UFREG_SWIZ_AAAA
};


static IMG_BOOL ProcessICOperand(GLSLCompilerPrivateData *psCPD,
								 GLSLUniFlexContext		*psUFContext,
								 const GLSLICOperand	*psICOp,
								 ICUFOperand			*psUFOp,
								 UFREG_RELATIVEINDEX	eInitialRelative,
								 IMG_BOOL				bDirectRegister);

#if !defined(GEN_HW_CODE) && defined(STANDALONE)
#include "usc_utils.c"
#endif

#if defined(DEBUG)

static IMG_VOID IC2UF_PrintInputInst(FILE *fstream, PUNIFLEX_INST psInst, IMG_INT iIndent, IMG_PUINT32 puTexturesToDump)
/*****************************************************************************
 FUNCTION	: IC2UF_PrintInputInst

 PURPOSE	: Print an input instruction.

 PARAMETERS	: fstream				- File stream to print.
			  psInst				- Instruction to print.
			  pdwTextureToDumps		- Variable which is updated which a bitmask
									  of the textures referenced by the instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	char apcTempString[512];

	PVRUniFlexDecodeInputInst(psInst, iIndent, apcTempString, puTexturesToDump);

	if(fstream)
	{
		fprintf(fstream, "%s", apcTempString);
	}
	else
	{
#ifdef DUMP_LOGFILES
		DumpLogMessage(LOGFILE_COMPILER, 0, "%s\n", apcTempString);
#endif
	}

}


/*****************************************************************************
 FUNCTION	: PrintInput

 PURPOSE	: Print a list of input instructions.

 PARAMETERS	: fstream					- File stream to print to.
			  psProg					- Head of the input instruction list.
			  puTextureDimensions		- Dimensions of the textures used in the program.
			  psConstants				- Local constants used in the program.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID IC2UF_PrintInput(FILE *fstream, PUNIFLEX_INST psProg, IMG_UINT32 * puTextureDimensions, PUNIFLEX_CONSTDEF psConstants)
{
	PUNIFLEX_INST psInst;
	IMG_UINT32 uTexturesToDump = 0;
	IMG_INT32 i, indent = 1;

	PVR_UNREFERENCED_PARAMETER(psConstants);

	psInst = psProg;

	while( psInst != NULL )
	{
		if(indent < 0)
		{
			indent = 0;
		}

		/* Decrease the indentation if needed */
		switch(psInst->eOpCode)
		{
		case UFOP_ELSE:
		case UFOP_ENDIF:
		case UFOP_ENDLOOP:
		case UFOP_ENDREP:
		case UFOP_GLSLENDLOOP:
		case UFOP_GLSLLOOPTAIL:
		case UFOP_LABEL:
			indent--;
			break;
		default:
			/* do nothing */
			break;
		}

		/* Print indentation */
		for(i=0; i < indent; ++i)
		{
			if(fstream)
			{
				fprintf(fstream, "\t");
			}
			else
			{
#ifdef DUMP_LOGFILES
				DumpLogMessage(LOGFILE_COMPILER, 0, "\t");
#endif
			}
		}

		/* Print the instruction */
		IC2UF_PrintInputInst(fstream, psInst, indent, &uTexturesToDump);

		if(fstream)
		{
			fprintf(fstream, "\n");
		}
		else
		{
#ifdef DUMP_LOGFILES
			DumpLogMessage(LOGFILE_COMPILER, 0, "\n");
#endif
		}

		/* Increase the indentation for the following intructions if needed */
		switch(psInst->eOpCode)
		{
		case UFOP_IF:
		case UFOP_ELSE:
		case UFOP_LOOP:
		case UFOP_REP:
		case UFOP_IFC:
		case UFOP_LABEL:
		case UFOP_IFP:
		case UFOP_GLSLLOOP:
		case UFOP_GLSLSTATICLOOP:
		case UFOP_GLSLLOOPTAIL:
			indent++;
			break;
		default:
			/* do nothing */
			break;
		}

		psInst = psInst->psILink;
	}

	for (i = 0; i < 16; i++)
	{
		if ((uTexturesToDump & (1 << i)) && (puTextureDimensions != NULL))
		{
			if(fstream)
			{
				fprintf(fstream, "T\t%u, %u", i, puTextureDimensions[i]);	
			}
			else
			{
#ifdef DUMP_LOGFILES
			DumpLogMessage(LOGFILE_COMPILER, 0, "T\t%u, %u", i, puTextureDimensions[i]);
#endif
			}
		}
	}
}
#endif /* #if defined(DEBUG) && defined(DUMP_LOGFILES) */

/*****************************************************************************
 FUNCTION	: DumpUniFlextInstData

 PURPOSE	: Dump uniflex instruction data

 PARAMETERS	: fstream					- File stream to print to.
			  psFirstUFCode				- Head of the input instruction list.

 RETURNS	: Nothing.
*****************************************************************************/
IMG_INTERNAL IMG_VOID DumpUniFlextInstData(FILE *fstream, IMG_VOID *pvCode)
{
#if defined(DEBUG) 
	PUNIFLEX_INST psFirstUFCode = (UNIFLEX_INST *)pvCode;

	IC2UF_PrintInput(fstream, psFirstUFCode, IMG_NULL, IMG_NULL);
#else
	PVR_UNREFERENCED_PARAMETER(fstream);
	PVR_UNREFERENCED_PARAMETER(pvCode);
#endif
}

#ifdef DUMP_LOGFILES
/*****************************************************************************
 FUNCTION	: DumpStructAllocInfoTable

 PURPOSE	: Dump all structure allocation information.

 PARAMETERS	: psUFContext				- UF context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID DumpStructAllocInfoTable(GLSLUniFlexContext *psUFContext)
{
	GLSLStructureAlloc *psStructAlloc = psUFContext->sStructAllocInfoTable.psStructAllocHead;

	while(psStructAlloc)
	{
		IMG_UINT32 j;

		const IMG_CHAR *apcPrecision[] = {"unknown", "lowp", "mediump", "highp"};

		DumpLogMessage(LOGFILE_COMPILER, 0, "==================================\n");
		DumpLogMessage(LOGFILE_COMPILER, 0, "Structure alloc info: name = %s\n", 
			GetSymbolName(psUFContext->psSymbolTable, psStructAlloc->sFullType.uStructDescSymbolTableID));
		DumpLogMessage(LOGFILE_COMPILER, 0, "Overal size (components): %u\n", psStructAlloc->uOverallAllocSize);

		DumpLogMessage(LOGFILE_COMPILER, 0, "%-10s %-10s %-10s %-10s %-10s\n", "Member", "Offset", "AllocCount", "iArraySize", "Precision");

		DumpLogMessage(LOGFILE_COMPILER, 0, "Members:\n");
		for(j = 0; j < psStructAlloc->uNumMembers; j++)
		{
			DumpLogMessage(LOGFILE_COMPILER, 0, "%-10d %-10d %-10d %-10d %-10s \n",
							j,
							psStructAlloc->psMemberAlloc[j].uCompOffset,
							psStructAlloc->psMemberAlloc[j].uAllocCount,
							psStructAlloc->psMemberAlloc[j].iArraySize,
							apcPrecision[psStructAlloc->psMemberAlloc[j].sFullType.ePrecisionQualifier]);
		}

		if(psStructAlloc->psBaseMemberAlloc != psStructAlloc->psMemberAlloc)
		{
			DumpLogMessage(LOGFILE_COMPILER, 0, "Base members:\n");

			for(j = 0; j < psStructAlloc->uNumBaseTypeMembers; j++)
			{
				DumpLogMessage(LOGFILE_COMPILER, 0, "%-10d %-10d %-10d %-10d %-10s \n",
								j,
								psStructAlloc->psBaseMemberAlloc[j].uCompOffset,
								psStructAlloc->psBaseMemberAlloc[j].uAllocCount,
								psStructAlloc->psBaseMemberAlloc[j].iArraySize,
								apcPrecision[psStructAlloc->psBaseMemberAlloc[j].sFullType.ePrecisionQualifier]);
			}				
		}
		DumpLogMessage(LOGFILE_COMPILER, 0, "\n");

		psStructAlloc = psStructAlloc->psNext;
	}
}

/*****************************************************************************
 FUNCTION	: DumpHWSymbols

 PURPOSE	: Dump all HW symbol allocation information.

 PARAMETERS	: psUFContext				- UF context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID DumpHWSymbols(GLSLCompilerPrivateData *psCPD, GLSLUniFlexContext *psUFContext)
{
	IMG_UINT32 i = 0;
	HWSYMBOL *psHWSymbol = psUFContext->sHWSymbolTab.psHWSymbolHead;
	IMG_CHAR registerName[15];
	const IMG_CHAR *apcPrecision[] = {"unknown", "lowp", "mediump", "highp"};

	DumpLogMessage(LOGFILE_COMPILER, 0, "==================================\n");
	DumpLogMessage(LOGFILE_COMPILER, 0, "HW symbol allocation \n"); 
	DumpLogMessage(LOGFILE_COMPILER, 0, "==================================\n");
	DumpLogMessage(LOGFILE_COMPILER, 0, "%-5s %-30s %-15s %-10s %-10s %-10s %-10s\n", "Index","Name", "Register", "AllocCount", "iArraySize", "ArrayTag", "Precision");

	while(psHWSymbol)
	{
		const IMG_CHAR *apcType[] = {"r", "v", "tc", "c", "t", "h", "MC", "i", "b", "m", "cop", "L", "p", "tcp", "tcpiftc", "vi", "vo", "a", "ct", "x"};

		const IMG_CHAR *apcChan[4][4] = {
			{"x___", "_y__", "__z_", "___w"},
			{"xy__", "_yz_", "__zw", "___w"},
			{"xyz_", "_yzw", "__zw", "___w"},
			{"xyzw", "_yzw", "__zw", "___w"}};

		IMG_UINT32 uNumComponents;

		if(!psHWSymbol->bRegAllocated)
		{
			LOG_INTERNAL_ERROR(("DumpHWSymbols: register for %s is not allocated ", GetSymbolName(psUFContext->psSymbolTable, psHWSymbol->uSymbolId)));

			psHWSymbol = psHWSymbol->psNext;

			continue;
		}

		uNumComponents = psHWSymbol->uAllocCount;

		if(uNumComponents > REG_COMPONENTS) uNumComponents = REG_COMPONENTS;

		if(psHWSymbol->eSymbolUsage & GLSLSU_FIXED_REG)
		{
			sprintf(registerName, "%s", apcType[psHWSymbol->eRegType]);
		}
		else if(psHWSymbol->eSymbolUsage & GLSLSU_SAMPLER)
		{
			sprintf(registerName, "%s%u",
					apcType[psHWSymbol->eRegType],
					psHWSymbol->uTextureUnit);
		}
		else
		{
			sprintf(registerName, "%s%u.%s",
					apcType[psHWSymbol->eRegType],
					REG_INDEX(psHWSymbol->u.uCompOffset),
					apcChan[uNumComponents - 1][REG_COMPSTART(psHWSymbol->u.uCompOffset)]);
		}

		DumpLogMessage(LOGFILE_COMPILER, 0,	"%-5d %-30s %-15s %-10d %-10d %-10d %-10s\n",
					i,
					GetSymbolName(psUFContext->psSymbolTable, psHWSymbol->uSymbolId),
					registerName,
					psHWSymbol->uAllocCount,
					psHWSymbol->iArraySize,
					psHWSymbol->uIndexableTempTag,
					apcPrecision[psHWSymbol->sFullType.ePrecisionQualifier]);

		psHWSymbol = psHWSymbol->psNext;
		i++;
	}

	DumpLogMessage(LOGFILE_COMPILER, 0, "Number of temporaries:  %u\n", psUFContext->sTempTab.uNextAvailVec4Reg);
	DumpLogMessage(LOGFILE_COMPILER, 0, "Number of consts:       %u\n", psUFContext->sFloatConstTab.uNextAvailVec4Reg);
	DumpLogMessage(LOGFILE_COMPILER, 0, "Number of attributes:   %u\n", psUFContext->sAttribTab.uNextAvailVec4Reg);
#ifdef OGLES2_VARYINGS_PACKING_RULE
	DumpLogMessage(LOGFILE_COMPILER, 0, "Number of varyings:     %u\n", psUFContext->sTexCoordsTable.auNextAvailRow);
#else
	DumpLogMessage(LOGFILE_COMPILER, 0, "Number of varyings:     %u\n", psUFContext->sTexCoordTab.uNextAvailVec4Reg);
#endif
	DumpLogMessage(LOGFILE_COMPILER, 0, "Number of samplers:     %u\n", psUFContext->uSamplerCount);

	DumpLogMessage(LOGFILE_COMPILER, 0, "\n");
}

/*****************************************************************************
 FUNCTION	: DumpRangeList

 PURPOSE	: Dump range list.

 PARAMETERS	: psRangeList		- List of ranges to dump.
			  pszRangeType		- Name of the type of ranges to dump.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID DumpRangeList(PUNIFLEX_RANGES_LIST	psRangeList,
							  IMG_PCHAR				pszRangeType)
{
	IMG_UINT32 i;

	if(psRangeList->uRangesCount > 0)
	{
		DumpLogMessage(LOGFILE_COMPILER, 0, "\n==================================\n");
		DumpLogMessage(LOGFILE_COMPILER, 0, "Number of continuous %s ranges: %u\n", 
			pszRangeType, psRangeList->uRangesCount);
		DumpLogMessage(LOGFILE_COMPILER, 0, "==================================\n");
		DumpLogMessage(LOGFILE_COMPILER, 0, "Range Start Count\n");
		for(i = 0; i < psRangeList->uRangesCount; i++)
		{
			DumpLogMessage(LOGFILE_COMPILER, 0, "%5d %5d %5d\n", 
				i,
				psRangeList->psRanges[i].uRangeStart,
				psRangeList->psRanges[i].uRangeEnd - psRangeList->psRanges[i].uRangeStart);
		}
	}
}

/*****************************************************************************
 FUNCTION	: DumpIndexableTempArraySizes

 PURPOSE	: Dump indexable temp array sizes

 PARAMETERS	: psUFContext				- UF context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID DumpIndexableTempArraySizes(GLSLUniFlexContext *psUFContext)
{
	IMG_UINT32 i;

	if(psUFContext->uIndexableTempArrayCount)
	{
		DumpLogMessage(LOGFILE_COMPILER, 0, "\n==================================\n");
		DumpLogMessage(LOGFILE_COMPILER, 0, "Number of Indexable temporary arrays: %u\n", psUFContext->uIndexableTempArrayCount);
		DumpLogMessage(LOGFILE_COMPILER, 0, "==================================\n");
		DumpLogMessage(LOGFILE_COMPILER, 0, "Tag   Size\n");
		for(i = 0; i < psUFContext->uIndexableTempArrayCount; i++)
		{
			DumpLogMessage(LOGFILE_COMPILER, 0, "%-5d %-5d\n", 
				psUFContext->psIndexableTempArraySizes[i].uTag,
				psUFContext->psIndexableTempArraySizes[i].uSize);
		}
	}
}

#endif


static IMG_VOID AssignSourceONE(UF_REGISTER *psSource, UF_REGFORMAT eFormat)
{
	(*psSource) = g_sSourceOne;

	if(eFormat == UF_REGFORMAT_I32 || eFormat == UF_REGFORMAT_U32)
	{
		psSource->uNum = 1;
		psSource->eType = UFREG_TYPE_IMMEDIATE;
		psSource->eFormat = eFormat;
	}
	else if (eFormat == UF_REGFORMAT_C10 || eFormat == UF_REGFORMAT_F16)
	{
		psSource->eFormat = eFormat;
	}
}

/*****************************************************************************
 FUNCTION	: CombineTwoUFSwiz

 PURPOSE	: Combine two swiz in terms of UniFlex swiz(usc.h)

 PARAMETERS	: uSwiz1		- First swiz applied.
			  uSwiz2		- Second swiz appled on the top of the first swiz

 RETURNS	: Combined swiz
*****************************************************************************/
static IMG_UINT32 CombineTwoUFSwiz(IMG_UINT32 uSwiz1, IMG_UINT32 uSwiz2)
{
	IMG_UINT32 uSwizResult = 0;
	IMG_UINT32 i;

	for (i = 0; i < REG_COMPONENTS; i++)
	{
		IMG_UINT32 uChannel = (uSwiz2 >> (i * 3)) & 0x7;
		uSwizResult |= ((uSwiz1 >> (uChannel * 3)) & 0x7) << (i * 3);
	}

	return uSwizResult;
}

/*****************************************************************************
 FUNCTION	: ComponentInMask

 PURPOSE	: Checks if an instruction updates a particular component within a vector.

 PARAMETERS	: psMask			- Destination update mask.
			  eComponent		- Component to check.

 RETURNS	: IMG_TRUE if the component is to be updated; IMG_FALSE otherwise.
*****************************************************************************/
static IMG_BOOL ComponentInMask(const GLSLICVecSwizWMask* psMask, GLSLICVecComponent eComponent, IMG_UINT32 *puPosition)

{
	IMG_UINT32 i;
	if (psMask->uNumComponents == 0)
	{
		if (puPosition != NULL) *puPosition = eComponent;
		return IMG_TRUE;
	}
	else
	{
		for (i = 0; i < psMask->uNumComponents; i++)
		{
			if (psMask->aeVecComponent[i] == eComponent)
			{
				if (puPosition != NULL) *puPosition = i;
				return IMG_TRUE;
			}
		}
	}
	return IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: IC2UF_GetDestMask

 PURPOSE	: Convert an intermediate code vector mask into a bitvector mask.

 PARAMETERS	: uComponentStart	- Component offset
			  eDestType			- Type specifier
			  psMask			- The mask to convert.

 RETURNS	: The bitvector mask.
*****************************************************************************/
static IMG_UINT32 IC2UF_GetDestMask(IMG_UINT32			uComponentStart,
							  GLSLTypeSpecifier			eDestType,
							  const GLSLICVecSwizWMask	*psMask, 
							  IMG_UINT32				*puSwiz)

{

	IMG_UINT32 i, uMask = 0, uPositionInMask;
	IMG_UINT32 uSwiz = UFREG_SWIZ_NONE;

	IMG_UINT32 uNumComponents = TYPESPECIFIER_NUM_COMPONENTS(eDestType);

	if(!GLSL_IS_VECTOR(eDestType))
	{
		psMask = IMG_NULL;
	}
	
	for (i = 0; i < uNumComponents; i++)
	{
		uPositionInMask = i;
		if ((psMask && ComponentInMask(psMask, (GLSLICVecComponent)i, &uPositionInMask)) || !psMask)
		{
			uMask |= (1 << (uComponentStart + i));
	
			uSwiz &= ~(7 << ((uComponentStart + i) * 3));
			uSwiz |= ((uPositionInMask + uComponentStart) << ((uComponentStart + i) * 3));
		}
	}

	if(puSwiz) *puSwiz = uSwiz;

	return uMask;
}


/*****************************************************************************
 FUNCTION	: ConvertToUFDestination

 PURPOSE	: 

 PARAMETERS	: 

 RETURNS	: Convert to UF destination
*****************************************************************************/
static IMG_VOID ConvertToUFDestination(UF_REGISTER *psUFDest,
									   const ICUFOperand *psDest,
									   IMG_UINT32  *puDestSwiz)
{
	psUFDest->eType  = psDest->eRegType;
	psUFDest->uNum   = psDest->uRegNum;
	psUFDest->u.byMask = (IMG_BYTE) IC2UF_GetDestMask(psDest->uCompStart,
											  psDest->sFullType.eTypeSpecifier,
											  &psDest->sICSwizMask,
											  puDestSwiz);

	psUFDest->eRelativeIndex = psDest->eRelativeIndex;
	psUFDest->u1.uRelativeStrideInComponents	= psDest->uRelativeStrideInComponents;
	psUFDest->uArrayTag		 = psDest->uArrayTag;
	psUFDest->eFormat		 = psDest->eRegFormat;

	psUFDest->byMod = 0;
}
/*****************************************************************************
 FUNCTION	: InitUFDestination

 PURPOSE	:

 PARAMETERS	: 

 RETURNS	: Initialise UF destination with basic setup
*****************************************************************************/
static IMG_VOID InitUFDestination(UF_REGISTER *psUFDest, UF_REGTYPE eRegType, IMG_UINT32 uRegNum, IMG_UINT32 uDestMask)
{
	psUFDest->eType			 = eRegType;
	psUFDest->uNum			 = uRegNum;
	psUFDest->u.byMask		 = (IMG_BYTE) uDestMask;

	psUFDest->eFormat		 = UF_REGFORMAT_F32;
	psUFDest->eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
	psUFDest->uArrayTag      = INVALID_ARRAY_TAG;
	psUFDest->byMod			 = 0;
}

/*****************************************************************************
 FUNCTION	: ConvertToUFSource

 PURPOSE	:

 PARAMETERS	: 

 RETURNS	: Convert to UF source
*****************************************************************************/
static IMG_VOID ConvertToUFSource(UF_REGISTER *psUFSource,
								  ICUFOperand *psSrc,
								  IMG_BOOL	  bCombineDestSwiz,
								  IMG_UINT32  uDestCompStart,
								  IMG_UINT32  uDestSwiz)
{
	IMG_UINT32 uSwiz = psSrc->uUFSwiz;

	if(bCombineDestSwiz) 
	{
		uSwiz <<= (uDestCompStart * 3);
		uSwiz = CombineTwoUFSwiz(uSwiz, uDestSwiz);
	}

	psUFSource->u.uSwiz = (IMG_UINT16)uSwiz;
	psUFSource->eType = psSrc->eRegType;
	psUFSource->uNum  = psSrc->uRegNum ;
	psUFSource->byMod = (IMG_BYTE)((psSrc->eICModifier & GLSLIC_MODIFIER_NEGATE) ? UFREG_SOURCE_NEGATE : 0);

	psUFSource->eRelativeIndex					= psSrc->eRelativeIndex;
	psUFSource->u1.uRelativeStrideInComponents	= psSrc->uRelativeStrideInComponents;
	psUFSource->uArrayTag						= psSrc->uArrayTag;
	psUFSource->eFormat							= psSrc->eRegFormat;
}



/*****************************************************************************
 FUNCTION	: InitUFSource

 PURPOSE	: 

 PARAMETERS	:

 RETURNS	: Initialise UF source with basic setup
*****************************************************************************/

static IMG_VOID InitUFSource(UF_REGISTER *psUFSource, UF_REGTYPE eRegType, IMG_UINT32 uRegNum, IMG_UINT32 uSwiz)
{
	psUFSource->u.uSwiz = (IMG_UINT16) uSwiz;
	psUFSource->eType = eRegType;
	psUFSource->uNum  = uRegNum;
	psUFSource->byMod = 0;

	psUFSource->eFormat		   = UF_REGFORMAT_F32;
	psUFSource->eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
	psUFSource->uArrayTag      = INVALID_ARRAY_TAG;
}

/*****************************************************************************
 FUNCTION	: ConvertSamplerToUFSource

 PURPOSE	:

 PARAMETERS	:

 RETURNS	: Convert to UF source for samplers
*****************************************************************************/
static IMG_VOID ConvertSamplerToUFSource(UF_REGISTER *psUFSource,
										 ICUFOperand *psSampler)
{
	psUFSource->u.uSwiz = UFREG_SWIZ_NONE;
	psUFSource->eType = UFREG_TYPE_TEX;
	psUFSource->uNum  = psSampler->uRegNum;
	psUFSource->byMod = 0;

	psUFSource->eFormat		   = psSampler->eRegFormat;
	psUFSource->eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
	psUFSource->uArrayTag      = INVALID_ARRAY_TAG;
}


/*****************************************************************************
 FUNCTION	: ConvertSamplerToUFSource

 PURPOSE	:

 PARAMETERS	:

 RETURNS	: Convert to UF source for samplers
*****************************************************************************/
static IMG_UINT32 ConvertToUFPredicate(ICUFOperand *psPredicate)
{
	IMG_UINT32 uPredicate = 0;

	if(psPredicate)
	{
		uPredicate = ENCODE_PREDICATE(psPredicate->uRegNum, psPredicate->uCompStart);
		uPredicate |= (psPredicate->eICModifier & GLSLIC_MODIFIER_NEGATE) ? UF_PRED_NEGFLAG : 0;
	}

	return uPredicate;
}

/*****************************************************************************
 FUNCTION	: InitialiseHWSymbolTable

 PURPOSE	: Intialise the HW symbol table

 PARAMETERS	: psUFContext				- UF context.
			  uNumEntries				- Number of entries to be allocated initially

 RETURNS	: IMG_TRUE if successful
*****************************************************************************/
static IMG_BOOL InitialiseHWSymbolTable(GLSLCompilerPrivateData *psCPD, GLSLUniFlexContext *psUFContext)
{
	GLSLHWSymbolTab *psHWSymbolTable = &psUFContext->sHWSymbolTab;

	/* Initialise linked list head and tail */
	psHWSymbolTable->psHWSymbolHead = psHWSymbolTable->psHWSymbolTail = IMG_NULL;

	/* Initialise list of varyings */
	psHWSymbolTable->uNumVaryings = 0;
	psHWSymbolTable->uMaxNumVarings = 32;
	psHWSymbolTable->apsVaryingList = DebugMemAlloc(sizeof(HWSYMBOL *) * psHWSymbolTable->uMaxNumVarings);
	if(psHWSymbolTable->apsVaryingList == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("InitialiseHWSymbolTable: Failed to allcate memory"));

		return IMG_FALSE;
	}

	return IMG_TRUE;
}

#ifdef OGLES2_VARYINGS_PACKING_RULE
/*****************************************************************************
 FUNCTION	: AddSymbolToVaryingList

 PURPOSE	: Add a symbol to varying list, but donot allocate register.

 PARAMETERS	: 

 RETURNS	: 
*****************************************************************************/
static IMG_VOID AddSymbolToVaryingList(GLSLCompilerPrivateData *psCPD, GLSLUniFlexContext *psUFContext, HWSYMBOL *psHWSymbol)
{
	GLSLHWSymbolTab *psHWSymbolTable = &psUFContext->sHWSymbolTab;

	if(psHWSymbolTable->uNumVaryings == psHWSymbolTable->uMaxNumVarings)
	{
		psHWSymbolTable->uMaxNumVarings += 32;

		psHWSymbolTable->apsVaryingList = DebugMemRealloc(psHWSymbolTable->apsVaryingList, sizeof(HWSYMBOL *) * psHWSymbolTable->uMaxNumVarings);
		if(psHWSymbolTable->apsVaryingList == IMG_NULL)
		{
			LOG_INTERNAL_ERROR(("AddSymbolToVaryingList: Failed to reallocate memory "));
		}
	}

	psHWSymbolTable->apsVaryingList[psHWSymbolTable->uNumVaryings] = psHWSymbol;
	
	psHWSymbolTable->uNumVaryings++;

}

#endif  /* #ifdef OGLES2_VARYINGS_PACKING_RULE */

/*****************************************************************************
 FUNCTION	: AddHWSymbolToList

 PURPOSE	: Add a HW symbol to the list

 PARAMETERS	: psUFContext				- UF context.

 RETURNS	: IMG_TRUE if successful
*****************************************************************************/
static IMG_VOID AddHWSymbolToList(GLSLCompilerPrivateData *psCPD, GLSLUniFlexContext *psUFContext, HWSYMBOL *psHWSymbol)
{
	GLSLHWSymbolTab *psHWSymbolTable = &psUFContext->sHWSymbolTab;
	
	PVR_UNREFERENCED_PARAMETER(psCPD);

	psHWSymbol->psNext = IMG_NULL;

	if(psHWSymbolTable->psHWSymbolHead == IMG_NULL)
	{
		psHWSymbolTable->psHWSymbolHead = psHWSymbol;
	}
	else
	{
		psHWSymbolTable->psHWSymbolTail->psNext = psHWSymbol;
	}

	psHWSymbolTable->psHWSymbolTail = psHWSymbol;

#ifdef OGLES2_VARYINGS_PACKING_RULE
	if(psHWSymbol->sFullType.eTypeQualifier == GLSLTQ_VERTEX_OUT ||
		psHWSymbol->sFullType.eTypeQualifier == GLSLTQ_FRAGMENT_IN)
	{
		/* For gl_Color and gl_Secondary in fragment shader, we use temporaries registers for them */
		/* For gl_Color and gl_Secondary and gl_FragCoord in fragment shader, we use temporaries registers for them */
		if(psUFContext->eProgramType == GLSLPT_FRAGMENT && (!psHWSymbol->bDirectRegister))
		{
			if( psHWSymbol->eBuiltinID == GLSLBV_COLOR || 
				psHWSymbol->eBuiltinID == GLSLBV_SECONDARYCOLOR ||
				psHWSymbol->eBuiltinID == GLSLBV_FRAGCOORD)
			{
				return;
			}
		}


		AddSymbolToVaryingList(psCPD, psUFContext, psHWSymbol);
	}
#endif
}


/*****************************************************************************
 FUNCTION	: FindHWSymbol

 PURPOSE	: Get uniflex hw symbol from its symbol id.

 PARAMETERS	: psUFContext				- UF context.
			  uSymId					- Id to get the location for.

 RETURNS	: The symbol or NULL if one hasn't been assigned.
*****************************************************************************/
static HWSYMBOL *FindHWSymbol(GLSLUniFlexContext *psUFContext, IMG_UINT32 uSymId, IMG_BOOL bDirectRegister)
{
	GLSLHWSymbolTab *psHWSymbolTable = &psUFContext->sHWSymbolTab;

	HWSYMBOL *psHWSymbol = psHWSymbolTable->psHWSymbolHead;

	while(psHWSymbol)
	{
		if (psHWSymbol->uSymbolId == uSymId && psHWSymbol->bDirectRegister == bDirectRegister)
		{
			return psHWSymbol;
		}

		psHWSymbol = psHWSymbol->psNext;
	}

	return IMG_NULL;
}

/*****************************************************************************
 FUNCTION	: DestroyHWSymbolTable

 PURPOSE	: Destroy the hw symbol table

 PARAMETERS	: psUFContext				- UF context.

 RETURNS	: Nothing
*****************************************************************************/
static IMG_VOID DestroyHWSymbolTable(GLSLUniFlexContext *psUFContext)
{
	GLSLHWSymbolTab *psHWSymbolTable = &psUFContext->sHWSymbolTab;
	HWSYMBOL *psHWSymbol = psHWSymbolTable->psHWSymbolHead, *psTemp;

	while(psHWSymbol)
	{
		psTemp = psHWSymbol->psNext;

		DebugMemFree(psHWSymbol);

		psHWSymbol = psTemp;
	}

	psHWSymbolTable->psHWSymbolHead = psHWSymbolTable->psHWSymbolTail = IMG_NULL;

	DebugMemFree(psHWSymbolTable->apsVaryingList);
	psHWSymbolTable->uNumVaryings = 0;
	psHWSymbolTable->uMaxNumVarings = 0;
}

/*****************************************************************************
 FUNCTION	: InitialiseStructAllocInfoTable

 PURPOSE	: Initialise struct alloc info table

 PARAMETERS	: psUFContext				- UF context.
			  uNumEntries				- Number of entries to be allocated initially

 RETURNS	: IMG_TRUE if successful
*****************************************************************************/
static IMG_BOOL InitialiseStructAllocInfoTable(GLSLUniFlexContext *psUFContext)
{
	GLSLStructAllocInfoTable *psTable = &psUFContext->sStructAllocInfoTable;

	psTable->psStructAllocHead = IMG_NULL;
	psTable->psStructAllocTail = IMG_NULL;

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: GetNewStructAllocInfoEntry

 PURPOSE	: Return a new struct alloc entry

 PARAMETERS	: psUFContext				- UF context.

 RETURNS	: Pointer to the new struct alloc entry
*****************************************************************************/
static IMG_VOID AddStructAllocInfoEntry(GLSLUniFlexContext *psUFContext,
										GLSLStructureAlloc *psStructAlloc)
{
	GLSLStructAllocInfoTable *psTable = &psUFContext->sStructAllocInfoTable;

	psStructAlloc->psNext = IMG_NULL;

	if(psTable->psStructAllocHead == IMG_NULL)
	{
		psTable->psStructAllocHead = psStructAlloc;
	}
	else
	{
		psTable->psStructAllocTail->psNext = psStructAlloc;
	}

	psTable->psStructAllocTail = psStructAlloc;

}



/*****************************************************************************
 FUNCTION	: FindStructAllocInfo

 PURPOSE	: Find struct alloc entry from struct symbol ID.

 PARAMETERS	: psUFContext				- UF context.
			  uStructSymID				- Struct symbol ID

 RETURNS	: Pointer to the struct alloc entry or IMG_NULL if failed to find.
*****************************************************************************/
static GLSLStructureAlloc *FindStructAllocInfo(GLSLUniFlexContext *psUFContext, IMG_UINT32 uStructSymID)
{
	GLSLStructureAlloc *psStructAlloc = psUFContext->sStructAllocInfoTable.psStructAllocHead;

	while(psStructAlloc)
	{
		if(psStructAlloc->sFullType.uStructDescSymbolTableID == uStructSymID)
		{
			return psStructAlloc;
		}

		psStructAlloc = psStructAlloc->psNext;
	}

	return IMG_NULL;
}

/*****************************************************************************
 FUNCTION	: DestroyStructAllocInfoTable

 PURPOSE	: Destroy the structure alloc info table

 PARAMETERS	: psUFContext				- UF context.

 RETURNS	: Nothing
*****************************************************************************/
static IMG_VOID DestroyStructAllocInfoTable(GLSLUniFlexContext *psUFContext)
{
	GLSLStructAllocInfoTable *psStructAllocInfoTable = &psUFContext->sStructAllocInfoTable;
	GLSLStructureAlloc *psStructAlloc, *psTemp;

	psStructAlloc = psStructAllocInfoTable->psStructAllocHead;

	while(psStructAlloc)
	{
		psTemp = psStructAlloc->psNext;

		DebugMemFree(psStructAlloc->psMemberAlloc);

		if(psStructAlloc->psBaseMemberAlloc != psStructAlloc->psMemberAlloc)
		{
			DebugMemFree(psStructAlloc->psBaseMemberAlloc);
		}

		DebugMemFree(psStructAlloc);

		psStructAlloc = psTemp;
	
	}

}


/*****************************************************************************
 FUNCTION	: CreateInstruction

 PURPOSE	: Create a UniFlex instruction.

 PARAMETERS	: psUFContext				- UF context.
			  eUFOpcode					- The opcode for the instruction.

 RETURNS	: Pointer to the new instruction.
*****************************************************************************/
static PUNIFLEX_INST CreateInstruction(GLSLUniFlexContext *psUFContext, UF_OPCODE eUFOpcode)
{
	PUNIFLEX_INST psInst;
	IMG_UINT32 i;
	
	#ifdef SRC_DEBUG
	GLSLCompilerPrivateData *psCPD;
	#endif /* SRC_DEBUG */

	psInst = DebugMemAlloc(sizeof(UNIFLEX_INST));	if(!psInst) return IMG_NULL;

	PVRUniFlexInitInst(psUFContext, psInst);

	psInst->eOpCode			= eUFOpcode;

	#ifdef SRC_DEBUG
	
	/* Just added here to handle DebugAssert psCPD definition requirment */
	psCPD = psUFContext->psCPD;

	psInst->uSrcLine = psUFContext->uCurSrcLine;
	
	/* Uninitalized source line number */
	DebugAssert((psInst->uSrcLine) != COMP_UNDEFINED_SOURCE_LINE);

	#endif /* SRC_DEBUG */

	for (i = 0; i < 3; i++)
	{
		psInst->asSrc[i] = g_sSourceZero;
	}

	/* Hook into the linked list */
	if(psUFContext->psFirstUFInst)
	{
		psUFContext->psEndUFInst->psILink = psInst;
	}
	else
	{
		psUFContext->psFirstUFInst = psInst;
	}
	psUFContext->psEndUFInst = psInst;

	return psInst;
}

/*****************************************************************************
 FUNCTION	: GetTypeAllocInfo

 PURPOSE	:

 PARAMETERS	: Inputs:  psFullType - Fully specified type
					   iArraySize - 0 means non array variable
			  Outputs: uTypeAllocCount - Alloc count for the type 
					   uTotalAllocCount - Total alloc count if it is an array 
					   uCompUseMask - component use mask
					   puAlignFirstToChannel - which channel it needs to align to
					   psStructAlloc - structure alloc info if it is a struct 

 RETURNS	: 
*****************************************************************************/
static IMG_VOID GetTypeAllocInfo(GLSLCompilerPrivateData *psCPD,
								 GLSLUniFlexContext		*psUFContext,
								 GLSLFullySpecifiedType	*psFullType,
								 IMG_INT32				iArraySize,
								 IMG_UINT32				*puTypeAllocCount,
								 IMG_UINT32				*puTotalAllocCount,
								 IMG_UINT32				*puAlignFirstToChannel,
								 IMG_UINT32				*puCompUseMask,
								 IMG_UINT32				*puNumValidComponents,
								 GLSLStructureAlloc		**ppsStructAlloc)
{
	IMG_UINT32 uTypeAllocCount = 0;
	IMG_UINT32 uCompUseMask = psUFContext->auCompUseMask[psFullType->eTypeSpecifier];

	GLSLStructureAlloc *psStructAlloc = IMG_NULL;

	/* Normally no need to align the first component */
	*puAlignFirstToChannel = REG_NO_ALIGNMENT;

	/*
	  Note: It depends on type specifier and type qualifier etc

		However,
		1) A struct always constains a multiple of 4 registers
		2) An array always constains a multiple of 4 registers

		Attributes: Can not pack register at all 
		Varyings:	mat2x2, mat3x2, mat4x2 and arrays of mat2x2, mat3x2, mat4x2, floats and vec2 
					will be packed, but don't pack multiple variables. Need to align registers before 
					allocate registers.

		Constants and temporarys: Pack whatever possible, can pack multiple identifiers into one vec4. 
	*/
	if(GLSL_IS_STRUCT(psFullType->eTypeSpecifier))
	{
		/* The size is always a multiple of 4 */
		psStructAlloc = GetStructAllocInfo(psCPD, psUFContext, psFullType);

		uTypeAllocCount = psStructAlloc->uOverallAllocSize;

		*puNumValidComponents = psStructAlloc->uTotalValidComponents;
	}
	else
	{
		IMG_UINT32 uNumComponents = TYPESPECIFIER_NUM_COMPONENTS(psFullType->eTypeSpecifier);
		IMG_UINT32 uNumCols = TYPESPECIFIER_NUM_COLS(psFullType->eTypeSpecifier);
		IMG_BOOL bDontPack = IMG_FALSE;

		*puNumValidComponents = TYPESPECIFIER_NUM_ELEMENTS(psFullType->eTypeSpecifier);

		switch(psFullType->eTypeQualifier)
		{
		case GLSLTQ_VERTEX_IN:
#if !PACK_VARYING_ARRAY
		case GLSLTQ_VERTEX_OUT:
		case GLSLTQ_FRAGMENT_IN:
#endif
			bDontPack = IMG_TRUE;
			break;
#if !PACK_TEMPS
		case GLSLTQ_TEMP:
			#if SCALARS_IN_ALPHA
			if(GLSL_IS_SCALAR(psFullType->eTypeSpecifier))
			{
				*puAlignFirstToChannel = 3;
			}
			#endif

			bDontPack = IMG_TRUE;
			break;
#endif

		case GLSLTQ_UNIFORM:
		case GLSLTQ_CONST:
			if(psFullType->ePrecisionQualifier == GLSLPRECQ_LOW)
			{
				bDontPack = IMG_TRUE;

				#if SCALARS_IN_ALPHA
				if(GLSL_IS_SCALAR(psFullType->eTypeSpecifier))
				{
					*puAlignFirstToChannel = 3;
				}
				#endif
			}
			break;
		}

		if(bDontPack)
		{
			/* Dont pack, a float uses a vec4 */
			uTypeAllocCount = REG_COMPONENTS * uNumCols;
		}
		else
		{
			/* Need to align the first component if it is a varying */
			if(psFullType->eTypeQualifier == GLSLTQ_VERTEX_OUT || 
				psFullType->eTypeQualifier == GLSLTQ_FRAGMENT_IN) 
			{
				*puAlignFirstToChannel = 0;
			}

			/* Matrices */
			if(GLSL_IS_MATRIX(psFullType->eTypeSpecifier))
			{
				if(uNumComponents == 2)
				{
					/* We can pack matrices with 2 rows */
					uTypeAllocCount = uNumCols * uNumComponents;

					switch(psFullType->eTypeSpecifier)
					{
					case GLSLTS_MAT2X2:
						uCompUseMask = 0xF;
						break;
					case GLSLTS_MAT3X2:
						uCompUseMask = 0x3F;
						break;
					case GLSLTS_MAT4X2:
						uCompUseMask = 0xFF;
						break;
					default:
						LOG_INTERNAL_ERROR(("GetTypeAllocInfo: What's the type ? "));
						break;
					}
				}
				else
				{
					/* Can not pack matrices with 3 rows */
					uTypeAllocCount = uNumCols * REG_COMPONENTS;
				}
			}
			else
			{
				/* Vector and floats */
				if(iArraySize && uNumComponents > 2)
				{
					uTypeAllocCount = REG_COMPONENTS;
				}
				else
				{
					/* We can pack array of floats and vec2s */
					uTypeAllocCount = uNumComponents;
				}
			}
		}		
	}

	/* Return alloc size per array element */
	*puTypeAllocCount = uTypeAllocCount;

	/* The total size by combining array size */
	if(iArraySize)
	{
		/* Always return a multiple of 4 components for an array */
		*puTotalAllocCount = ROUND_UP((uTypeAllocCount * iArraySize), REG_COMPONENTS);
	}
	else 
	{
		/* No need to be multiple of 4 */
		*puTotalAllocCount = uTypeAllocCount;
	}

#if (SCALARS_IN_ALPHA)
	/* 
		When putting scalars in alpha channel, we need to subtract unused components in beginning from 
		the total alloc count 
	*/
	if(*puAlignFirstToChannel != REG_NO_ALIGNMENT)
	{
		if(!iArraySize)
		{
			uTypeAllocCount -= (*puAlignFirstToChannel);
		}

		*puTotalAllocCount -= (*puAlignFirstToChannel);
	}
#endif

	/* Structure alloc info pointer */
	*ppsStructAlloc = psStructAlloc;

	/* Return comp use mask */
	*puCompUseMask = uCompUseMask;

}

/*****************************************************************************
 FUNCTION	: AddToIndexableTempList

 PURPOSE	: Add an indexable temp symbol to indexable temp list.

 PARAMETERS	: psUFContext				- UF context.
			  psHWSymbol				- UniFlex HW symbol	

 RETURNS	: IMG_TRUE if succesfful.
*****************************************************************************/
static IMG_BOOL AddToIndexableTempList(GLSLCompilerPrivateData *psCPD, GLSLUniFlexContext *psUFContext, HWSYMBOL *psHWSymbol)
{
	UNIFLEX_INDEXABLE_TEMP_SIZE *psIndexableTempSize;

	psUFContext->uIndexableTempArrayCount++;
	psUFContext->psIndexableTempArraySizes = DebugMemRealloc(psUFContext->psIndexableTempArraySizes,
															 psUFContext->uIndexableTempArrayCount * sizeof(UNIFLEX_INDEXABLE_TEMP_SIZE));
	if(psUFContext->psIndexableTempArraySizes == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("AddToIndexableTempList: Failed to get memory\n"));
		return IMG_FALSE;
	}

	psIndexableTempSize = &psUFContext->psIndexableTempArraySizes[psUFContext->uIndexableTempArrayCount-1];

	psIndexableTempSize->uSize = ROUND_UP(psHWSymbol->uTotalAllocCount, REG_COMPONENTS)/REG_COMPONENTS;
	psIndexableTempSize->uTag = psHWSymbol->uIndexableTempTag;

	return IMG_TRUE;

}

/*****************************************************************************
 FUNCTION	: UFRegATableInitialize

 PURPOSE	: Initialize a reg assignment table.

 PARAMETERS	: psTable				- Table to initialize.
			  uMaxRegisters			- Maximum register that can be used for this table.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID UFRegATableInitialize(UFRegAsssignmentTable* psTable, IMG_UINT32 uMaxRegisters)

{
	IMG_UINT32 i;
	psTable->uMaxRegisters = uMaxRegisters;
	for (i = 0; i < 3; i++)
	{
		psTable->auNextAvailRegHighP[i] = (IMG_UINT32)-1;
		psTable->auNextAvailRegMediumP[i] = (IMG_UINT32)-1;
	}
	psTable->uNextAvailVec4Reg = 0;
}

/*****************************************************************************
 FUNCTION	: UFRegATableAssignRegisters

 PURPOSE	: Assign registers from a reg assignment table,
			  Note: For any request whose size is greater or equal to 4, 
				    the size is rounded up to a multiple of 4. 

 PARAMETERS	: psTable				- Table from.
			  uTotalAllocCount		- number of components needed to allocate

 RETURNS	: The reg number in terms of components for the first component
*****************************************************************************/
static IMG_UINT32 UFRegATableAssignRegisters(GLSLUniFlexContext		*psUFContext,
											 UFRegAsssignmentTable	*psTable,
											 IMG_UINT32				uAlignFirstToChannel,
											 IMG_UINT32				uTotalAllocCount,
											 GLSLPrecisionQualifier ePrecision)
{
	IMG_UINT32 uSize = uTotalAllocCount;
	IMG_UINT32 uRegNum = 0, uCompStart = 0;

	if ((uAlignFirstToChannel != REG_NO_ALIGNMENT) || uSize >= REG_COMPONENTS)
	{
		uSize = ROUND_UP(uSize, REG_COMPONENTS)/REG_COMPONENTS;

		if ((psTable->uNextAvailVec4Reg + uSize) > psTable->uMaxRegisters)
		{
				psUFContext->bRegisterLimitExceeded = IMG_TRUE;
		}
		uRegNum = psTable->uNextAvailVec4Reg;

		/* Align the first component to the specific channel. */
		if(uAlignFirstToChannel != REG_NO_ALIGNMENT)
		{
			uCompStart = uAlignFirstToChannel;
		}

		/* 
		psTable->auNextAvailRegister[0] = (IMG_UINT32)-1;
		psTable->auNextAvailRegister[1] = (IMG_UINT32)-1;
		psTable->auNextAvailRegister[2] = (IMG_UINT32)-1;
		*/

		psTable->uNextAvailVec4Reg += uSize;
	}
	else
	{
		IMG_UINT32 i;
		IMG_UINT32 *puNextAvailRegister;

		if(ePrecision == GLSLPRECQ_HIGH)
		{
			puNextAvailRegister = psTable->auNextAvailRegHighP;
		}
		else
		{
			puNextAvailRegister = psTable->auNextAvailRegMediumP;
		}

		/* Take the value for available vec4 reg number */
		puNextAvailRegister[3] = psTable->uNextAvailVec4Reg;		

		/* Seach through all available match space for the size */
		for (i = (uSize - 1); i < 4; i++)
		{
			if (i == 3 && puNextAvailRegister[i] == psTable->uMaxRegisters)
			{
				psUFContext->bRegisterLimitExceeded = IMG_TRUE;
			}

			if (puNextAvailRegister[i] != (IMG_UINT32)-1)
			{
				uRegNum = puNextAvailRegister[i];
				uCompStart = 3 - i;
				if ((uSize - 1) < i)
				{
					puNextAvailRegister[i - uSize] = puNextAvailRegister[i];
				}
				if (i == 3)
				{
					puNextAvailRegister[i]++;
				}
				else
				{
					puNextAvailRegister[i] = (IMG_UINT32)-1;
				}
				break;
			}
		}

		/* Update the next vec4 reg */
		psTable->uNextAvailVec4Reg = puNextAvailRegister[3];
	}

	return uRegNum * REG_COMPONENTS + uCompStart;

}

/*****************************************************************************
 FUNCTION	: UFRegATableGetMaxComponent

 PURPOSE	: Return the max number of components allocated in this assignment table.

 PARAMETERS	: psTable				- reg assignment table.

 RETURNS	: Max number of components allocated
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 UFRegATableGetMaxComponent(UFRegAsssignmentTable *psTable)
{
	return psTable->uNextAvailVec4Reg * REG_COMPONENTS;
}


/*****************************************************************************
 FUNCTION	: InitICUFOperand

 PURPOSE	: Generate UF operand from its symbol ID, defaut setting applies.

 PARAMETERS	:

 RETURNS	: New last instruction of the program.
*****************************************************************************/
static IMG_BOOL InitICUFOperand(GLSLCompilerPrivateData *psCPD,
								GLSLUniFlexContext *psUFContext,
								IMG_UINT32			uSymbolID,
								ICUFOperand			*psICUFOperand,
								IMG_BOOL			bDirectRegister)
{
	GLSLICOperand sICOperand;

	/* Init IC operand */
	ICInitICOperand(uSymbolID, &sICOperand);

	/* Return ICUF operand */
	return ProcessICOperand(psCPD, psUFContext, &sICOperand, psICUFOperand, UFREG_RELATIVEINDEX_NONE, bDirectRegister);

}

/*****************************************************************************
 FUNCTION	: GetTemporary

 PURPOSE	: Get a temporary variable of a specified type.

 PARAMETERS	: psUFContext		- UF context
			  eTempType			- Type specifier for the temporary variable.
			  psTemp			- Where the description of the temporary variable will be returned.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_BOOL GetTemporary(GLSLCompilerPrivateData *psCPD,
							 GLSLUniFlexContext		*psUFContext,
							 GLSLTypeSpecifier		eTempType,
							 GLSLPrecisionQualifier ePrecisionQualifier,
							 ICUFOperand			*psTemp)

{
	IMG_UINT32 uTempSymbolId ;

	if(!ICAddTemporary(psCPD, psUFContext->psICProgram, eTempType, ePrecisionQualifier, &uTempSymbolId))
	{
		LOG_INTERNAL_ERROR(("GetTemporary: Failed to add temporary with specific type \n"));
		return IMG_FALSE;
	}

	return InitICUFOperand(psCPD, psUFContext, uTempSymbolId, psTemp, IMG_FALSE);
}


/*****************************************************************************
 FUNCTION	: IC2UF_GetPredicate

 PURPOSE	: 

 PARAMETERS	: psUFContext		- UF context
			  psPredicate		- 

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_BOOL IC2UF_GetPredicate(GLSLCompilerPrivateData *psCPD,
							 GLSLUniFlexContext		*psUFContext,
							 ICUFOperand			*psPredicate)

{
	IMG_UINT32 uPredicateSymID ;

	if(!ICAddBoolPredicate(psCPD, psUFContext->psICProgram, &uPredicateSymID))
	{
		LOG_INTERNAL_ERROR(("IC2UF_GetPredicate: Failed to add bool as predicate to symbol table"));
		return IMG_FALSE;
	}

	return InitICUFOperand(psCPD, psUFContext, uPredicateSymID, psPredicate, IMG_FALSE);
}

/*****************************************************************************
 FUNCTION	: GetFixedConst

 PURPOSE	: Get a float constant

 PARAMETERS	: psUFContext		- UF context
			  fValue			- Float value
			  psConst			- ICUF operand

 RETURNS	: IMG_TRUE if successful.
*****************************************************************************/
static IMG_BOOL GetFixedConst(GLSLCompilerPrivateData *psCPD,
							  GLSLUniFlexContext		*psUFContext,
							  IMG_FLOAT					fValue,
							  GLSLPrecisionQualifier	ePrecisionQualifier,
							  ICUFOperand				*psConst)

{
	IMG_UINT32 uConstSymbolId;

	if(!AddFloatConstant(psCPD, psUFContext->psSymbolTable, fValue, ePrecisionQualifier, IMG_TRUE, &uConstSymbolId))
	{
		LOG_INTERNAL_ERROR(("GetFixedConst: Failed to add constant to the symbol table %f\n", fValue));
		return IMG_FALSE;
	}

	return InitICUFOperand(psCPD, psUFContext, uConstSymbolId, psConst, IMG_FALSE);

}


/*****************************************************************************
 FUNCTION	: GetIntConst

 PURPOSE	: Get an integer constant

 PARAMETERS	: psUFContext		- UF context
			  iValue			- Integer value
			  psConst			- ICUF operand

 RETURNS	: IMG_TRUE if successful.
*****************************************************************************/
static IMG_BOOL GetIntConst(GLSLCompilerPrivateData *psCPD,
							GLSLUniFlexContext		*psUFContext,
							IMG_INT32				iValue,
							GLSLPrecisionQualifier	ePrecisionQualifier,
							ICUFOperand				*psConst)

{
	IMG_UINT32 uConstSymbolId;

	if(!AddIntConstant(psCPD, psUFContext->psSymbolTable, iValue, ePrecisionQualifier, IMG_TRUE, &uConstSymbolId))
	{
		LOG_INTERNAL_ERROR(("GetFixedConst: Failed to add constant to the symbol table %u\n", iValue));
		return IMG_FALSE;
	}

	return InitICUFOperand(psCPD, psUFContext, uConstSymbolId, psConst, IMG_FALSE);
}

/*****************************************************************************
 FUNCTION	: AddMOVAToUFCode

 PURPOSE	: Add MOVA to UF code

 PARAMETERS	: psUFContext		- UF context
			  psIndexOperand	- Operand for the index
			  eRelativeIndex	- Relative index enum

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_BOOL AddMOVAToUFCode(GLSLUniFlexContext	*psUFContext,
								ICUFOperand			*psIndexOperand,
								UFREG_RELATIVEINDEX eRelativeIndex)
{

	UNIFLEX_INST *psUFInst;

#ifdef GLSL_NATIVE_INTEGERS
	if(GLSL_IS_INT(psIndexOperand->sFullType.eTypeSpecifier))
	{
		psUFInst = CreateInstruction(psUFContext, UFOP_MOVA_INT);		if(!psUFInst) return IMG_FALSE;
	}
	else
#endif
	{
		psUFInst = CreateInstruction(psUFContext, UFOP_MOVA_RNE);		if(!psUFInst) return IMG_FALSE;
	}

	ConvertToUFSource(&psUFInst->asSrc[0], psIndexOperand, IMG_FALSE, 0, 0);

	InitUFDestination(&psUFInst->sDest, UFREG_TYPE_ADDRESS, 0, 1 << (eRelativeIndex - 1));
	psUFInst->sDest.eFormat = psUFInst->asSrc[0].eFormat;

	return IMG_TRUE;
}
/*****************************************************************************
 FUNCTION	: BuildStructBaseMembers

 PURPOSE	: Add MOVA to UF code

 PARAMETERS	: Build struct base member alloc information

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_BOOL BuildStructBaseMembers(GLSLCompilerPrivateData *psCPD, GLSLStructureAlloc *psStructAlloc)
{
	IMG_UINT32 i, j, k;
	IMG_UINT32 uMemberOffset = 0;

	psStructAlloc->psBaseMemberAlloc = DebugMemAlloc(sizeof(GLSLStructMemberAlloc) * psStructAlloc->uNumBaseTypeMembers);
	if(psStructAlloc->psBaseMemberAlloc == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("BuildStructBaseMembers: Failed to alloc memory \n"));
		return IMG_FALSE;
	}

	for(i = 0; i < psStructAlloc->uNumMembers; i++)
	{
		if(GLSL_IS_STRUCT(psStructAlloc->psMemberAlloc[i].sFullType.eTypeSpecifier))
		{
			IMG_UINT32 uMemberArraySize = psStructAlloc->psMemberAlloc[i].iArraySize ? psStructAlloc->psMemberAlloc[i].iArraySize : 1;

			GLSLStructureAlloc *psMemberStructAlloc = psStructAlloc->psMemberAlloc[i].psStructAlloc;

			DebugAssert(psMemberStructAlloc);

			for(k = 0; k < uMemberArraySize; k++)
			{
				for(j = 0; j < psMemberStructAlloc->uNumBaseTypeMembers; j++)
				{
					psStructAlloc->psBaseMemberAlloc[uMemberOffset + j] = psMemberStructAlloc->psBaseMemberAlloc[j];

					psStructAlloc->psBaseMemberAlloc[uMemberOffset + j].uCompOffset += psStructAlloc->psMemberAlloc[i].uCompOffset + k * psMemberStructAlloc->uOverallAllocSize;
				}

				uMemberOffset += psMemberStructAlloc->uNumBaseTypeMembers;
			}

		}
		else
		{
			psStructAlloc->psBaseMemberAlloc[uMemberOffset] = psStructAlloc->psMemberAlloc[i];

			uMemberOffset++;
		}
	}

	DebugAssert(uMemberOffset == psStructAlloc->uNumBaseTypeMembers);

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: BuildStructAllocInfo

 PURPOSE	: Recursively build structure alloc information

 PARAMETERS	: psContext - GLSLStructAllocContext

 RETURNS	: IMG_TRUE is successful.
*****************************************************************************/
static GLSLStructureAlloc *BuildStructAllocInfo(GLSLCompilerPrivateData *psCPD,
												GLSLUniFlexContext *psUFContext, const GLSLFullySpecifiedType *psFullType)
{
	GLSLStructureAlloc		*psStructAlloc;
	UFRegAsssignmentTable	sRegTable;
	IMG_BOOL bContainsStruct = IMG_FALSE;

	GLSLIdentifierData *psMemberIdentifierData;
	IMG_UINT32 i;
	IMG_UINT32 uNumBaseTypeMembers = 0;
	IMG_UINT32 uTotalValidComponents = 0;

	GLSLStructureDefinitionData *psStructureDefinitionData;

	psStructAlloc = DebugMemAlloc(sizeof(GLSLStructureAlloc));
	if(psStructAlloc == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("BuildStructAllocInfo: Failed to alloc memory \n"));
		return IMG_NULL;
	}

	UFRegATableInitialize(&sRegTable, (IMG_UINT32)-1);

	psStructureDefinitionData = GetSymbolTableData(psUFContext->psSymbolTable, psFullType->uStructDescSymbolTableID);

	psStructAlloc->uNumMembers = psStructureDefinitionData->uNumMembers;
	psStructAlloc->psMemberAlloc = DebugMemAlloc(sizeof(GLSLStructMemberAlloc) * psStructAlloc->uNumMembers);
	if(psStructAlloc->psMemberAlloc == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("BuildStructAllocInfo: Failed to alloc memory \n"));
		return IMG_NULL;
	}
	psStructAlloc->sFullType = *psFullType;
	psStructAlloc->uTotalTextureUnits = 0;

#ifdef DEBUG
	psStructAlloc->psStructName = GetSymbolName(psUFContext->psSymbolTable, psStructAlloc->sFullType.uStructDescSymbolTableID);
#endif

	for(i = 0; i < psStructAlloc->uNumMembers; i++)
	{
		IMG_UINT32 uAlignFirstToChannel;
		GLSLPrecisionQualifier	ePrecisionQualifier;

		/* Get member identifier data */
		psMemberIdentifierData = &(psStructureDefinitionData->psMembers[i].sIdentifierData);

		/* Fully specified type for the member */
		psStructAlloc->psMemberAlloc[i].sFullType = psMemberIdentifierData->sFullySpecifiedType;

		/* Member array size */
		psStructAlloc->psMemberAlloc[i].iArraySize 
			= (psMemberIdentifierData->eArrayStatus == GLSLAS_NOT_ARRAY) ? 0 : psMemberIdentifierData->iActiveArraySize;

		/* Get AllocCount and total size, struct alloc info etc. */
		GetTypeAllocInfo(psCPD, psUFContext,
						 &psStructAlloc->psMemberAlloc[i].sFullType,
						 psStructAlloc->psMemberAlloc[i].iArraySize,
						 &psStructAlloc->psMemberAlloc[i].uAllocCount,
						 &psStructAlloc->psMemberAlloc[i].uTotalAllocCount,
						 &uAlignFirstToChannel,
						 &psStructAlloc->psMemberAlloc[i].uCompUseMask,
						 &psStructAlloc->psMemberAlloc[i].uNumValidComponents,
						 &psStructAlloc->psMemberAlloc[i].psStructAlloc);

		psStructAlloc->psMemberAlloc[i].uTextureUnitOffset = psStructAlloc->uTotalTextureUnits;

		/* If the member is a struct */
		if(psStructAlloc->psMemberAlloc[i].psStructAlloc)
		{
			IMG_UINT32 uMemberArraySize;

			uMemberArraySize = psStructAlloc->psMemberAlloc[i].iArraySize ? psStructAlloc->psMemberAlloc[i].iArraySize : 1;

			uNumBaseTypeMembers += psStructAlloc->psMemberAlloc[i].psStructAlloc->uNumBaseTypeMembers * uMemberArraySize;

			bContainsStruct = IMG_TRUE;

			psStructAlloc->uTotalTextureUnits += psStructAlloc->psMemberAlloc[i].psStructAlloc->uTotalTextureUnits;
		}
		else
		{
			/* Total number of bse type members */
			uNumBaseTypeMembers++;

			if(GLSL_IS_SAMPLER(psStructAlloc->psMemberAlloc[i].sFullType.eTypeSpecifier))
			{
				psStructAlloc->uTotalTextureUnits += psStructAlloc->psMemberAlloc[i].sFullType.iArraySize
					? psStructAlloc->psMemberAlloc[i].sFullType.iArraySize : 1;
			}
		}

		uTotalValidComponents += psStructAlloc->psMemberAlloc[i].uNumValidComponents;

		ePrecisionQualifier = psStructAlloc->psMemberAlloc[i].sFullType.ePrecisionQualifier;
		/* We always use low precision for booleans */
		if(GLSL_IS_BOOL(psStructAlloc->psMemberAlloc[i].sFullType.eTypeSpecifier))
		{
			/* Get default precision for boolean */
			ePrecisionQualifier =  BOOLEAN_PRECISION(psUFContext->psICProgram);
		}

		/* Assign registers for the member */
		psStructAlloc->psMemberAlloc[i].uCompOffset
							= UFRegATableAssignRegisters(psUFContext, 
														 &sRegTable, 
														 uAlignFirstToChannel, 
														 psStructAlloc->psMemberAlloc[i].uTotalAllocCount,
														 ePrecisionQualifier);

	}

	/* The overall alloc size is always a multiple of 4 */
	psStructAlloc->uOverallAllocSize = ROUND_UP(UFRegATableGetMaxComponent(&sRegTable), 4);
	psStructAlloc->uNumBaseTypeMembers = uNumBaseTypeMembers;
	psStructAlloc->uTotalValidComponents = uNumBaseTypeMembers;


	/* If the struct constains another struct, then we need to build base type member list */
	if(bContainsStruct)
	{
		BuildStructBaseMembers(psCPD, psStructAlloc);
	}
	else
	{
		/* Two lists are the same */
		psStructAlloc->uNumBaseTypeMembers = psStructAlloc->uNumMembers;
		psStructAlloc->psBaseMemberAlloc = psStructAlloc->psMemberAlloc;
	}

	/* Add to the table */
	AddStructAllocInfoEntry(psUFContext, psStructAlloc);

	return psStructAlloc;
}

/*****************************************************************************
 FUNCTION	: GetStructAllocInfo

 PURPOSE	: Search through structure alloc info table, if we cannot find,
			  then build up the structure alloc info

 PARAMETERS	: psContext			- UF context
			  psFullType		- Fully specified type

 RETURNS	: 
*****************************************************************************/
IMG_INTERNAL GLSLStructureAlloc* GetStructAllocInfo(GLSLCompilerPrivateData			*psCPD,
													GLSLUniFlexContext				*psUFContext,
													const GLSLFullySpecifiedType	*psFullType)
{
	GLSLStructureAlloc *psStructAlloc;

	/* Find struct alloc info which has been built up */
	psStructAlloc = FindStructAllocInfo(psUFContext, psFullType->uStructDescSymbolTableID);

	/* Failed to find, then build up now */
	if(psStructAlloc == IMG_NULL)
	{
		psStructAlloc = BuildStructAllocInfo(psCPD, psUFContext, psFullType);
		if(psStructAlloc == IMG_NULL)
		{
			LOG_INTERNAL_ERROR(("GetStructAllocInfo: Failed to build struct alloc information \n"));

			return IMG_NULL;
		}
	}

	return psStructAlloc;
}

/*****************************************************************************
 FUNCTION	: AddToRangeList

 PURPOSE	: Add a symbol to a list of dynamically indexed ranges.

 PARAMETERS	:

 RETURNS	: IMG_TRUE if succesfful.
*****************************************************************************/
static IMG_BOOL AddToRangeList(GLSLCompilerPrivateData *psCPD, PUNIFLEX_RANGES_LIST psRangeList, HWSYMBOL *psHWSymbol)
{
	/*
		Continuous range, within a range, all symbols need to
		be allocated continuously
	*/
	IMG_UINT32 uStart = REG_INDEX(psHWSymbol->u.uCompOffset);
	IMG_UINT32 uCount = (psHWSymbol->uTotalAllocCount + REG_COMPONENTS - 1)/REG_COMPONENTS;
	PUNIFLEX_RANGE psRange;

	PVR_UNREFERENCED_PARAMETER(psCPD);

	psRangeList->psRanges = 
		DebugMemRealloc(psRangeList->psRanges, (psRangeList->uRangesCount + 1) * sizeof(psRangeList->psRanges[0]));
	if(psRangeList->psRanges == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("AddToRangeList: Failed to get memory\n"));
		return IMG_FALSE;
	}

	psRange = &psRangeList->psRanges[psRangeList->uRangesCount];
	psRange->uRangeStart = uStart;
	psRange->uRangeEnd = uStart + uCount;

	psRangeList->uRangesCount++;

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: AssignSamplerTexUnit

 PURPOSE	: Assign a sampler a tex image unit

 PARAMETERS	: 

 RETURNS	: IMG_TRUE if the component is to be updated; IMG_FALSE otherwise.
*****************************************************************************/
static IMG_BOOL AssignSamplerTexUnit(GLSLCompilerPrivateData *psCPD,
									 GLSLUniFlexContext		*psUFContext,
									IMG_UINT32				uSamplerID,
									IMG_UINT32				uArraySize,
									GLSLTypeSpecifier		eTypeSpecifier,
									IMG_UINT32				*puTextureUnit)
{
	GLSLSamplerAssignmentTab *psTable = &psUFContext->sSamplerTab;
	IMG_UINT32 i, uTextureUnit;
	IMG_PUINT32 auNewSamplerToSymbolId;
	PUNIFLEX_DIMENSIONALITY asNewSamplerDims;
	UNIFLEX_DIMENSIONALITY_TYPE eDimension;

	#if defined(OUTPUT_USPBIN)
	if(psUFContext->uSamplerCount + uArraySize > USP_MAX_SAMPLER_IDX)
	{
		LogProgramError(psCPD->psErrorLog, "More than %d samplers are used in the shader.\n", USP_MAX_SAMPLER_IDX);
		return IMG_FALSE;
	}
	#endif /* defined(OUTPUT_USPBIN) */

	*puTextureUnit = uTextureUnit = psUFContext->uSamplerCount;

	psUFContext->uSamplerCount += uArraySize;

	asNewSamplerDims = DebugMemRealloc(psUFContext->asSamplerDims,
										psUFContext->uSamplerCount * sizeof(psUFContext->asSamplerDims[0]));
	if (asNewSamplerDims == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("AssignSamplerTexUnit: Failed to get memory\n"));
		return IMG_FALSE;
	}
	psUFContext->asSamplerDims = asNewSamplerDims;

	auNewSamplerToSymbolId = DebugMemRealloc(psTable->auSamplerToSymbolId,
										psUFContext->uSamplerCount * sizeof(psTable->auSamplerToSymbolId[0]));
	if (auNewSamplerToSymbolId == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("AssignSamplerTexUnit: Failed to get memory\n"));
		return IMG_FALSE;
	}
	psTable->auSamplerToSymbolId = auNewSamplerToSymbolId;

	for(i = 0; i < uArraySize; i++)
	{
		psTable->auSamplerToSymbolId[uTextureUnit + i] = uSamplerID;
	}

	switch(eTypeSpecifier)
	{
		case GLSLTS_SAMPLER1D:
		case GLSLTS_SAMPLER2D:
		case GLSLTS_SAMPLER1DSHADOW:
		case GLSLTS_SAMPLER2DSHADOW:
		case GLSLTS_SAMPLERSTREAM:
		case GLSLTS_SAMPLEREXTERNAL:
			eDimension = UNIFLEX_DIMENSIONALITY_TYPE_2D;
			break;

#if defined(SUPPORT_GL_TEXTURE_RECTANGLE)
		case GLSLTS_SAMPLER2DRECT:
		case GLSLTS_SAMPLER2DRECTSHADOW:
			/** TODO: Add rectangular texture handling. */
			eDimension = UNIFLEX_DIMENSIONALITY_TYPE_2D;
			break;
#endif

		case GLSLTS_SAMPLER3D:
			eDimension = UNIFLEX_DIMENSIONALITY_TYPE_3D;
			break;
		case GLSLTS_SAMPLERCUBE:
			eDimension = UNIFLEX_DIMENSIONALITY_TYPE_CUBEMAP;
			break;
		default:
			LOG_INTERNAL_ERROR(("Unknown sampler type\n"));
			return IMG_FALSE;
	}

	for(i = 0; i < uArraySize; i++)
	{
		psUFContext->asSamplerDims[uTextureUnit + i].eType = eDimension;
		{
			psUFContext->asSamplerDims[uTextureUnit + i].bIsArray = IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: AssignHWSymbolRegisters

 PURPOSE	: Assign hw symbol for specific type and array size,
			  copy constant data if has any

 PARAMETERS	: psUFContext		- Uniflex context
			  psTable			- Reg assignment table
			  iArraySize		- Array size for array type, 0 for non-array.
			  psHWSymbol		- HW symbol entry

 RETURNS	: IMG_TRUE if successfully
*****************************************************************************/
static IMG_BOOL AssignHWSymbolRegisters(GLSLCompilerPrivateData *psCPD,
										GLSLUniFlexContext			*psUFContext,
										UFRegAsssignmentTable		*psTable,
										HWSYMBOL					*psHWSymbol,
										IMG_INT32					iArraySize)
{
	IMG_UINT32				uAllocCount, uTotalSize, uCompOffsetBase, uCompUseMask;
	GLSLStructureAlloc		*psStructAlloc = IMG_NULL;
	GLSLFullySpecifiedType	*psFullType = &psHWSymbol->sFullType;
	IMG_UINT32				uAlignFirstToChannel;
	IMG_UINT32				uIgnored;

	if(psHWSymbol->eRegType == UFREG_TYPE_PREDICATE)
	{
		IMG_UINT32 uNumComponents = TYPESPECIFIER_NUM_COMPONENTS(psFullType->eTypeSpecifier);

		uCompUseMask = psUFContext->auCompUseMask[psFullType->eTypeSpecifier];
		uAllocCount = uNumComponents;
		uTotalSize = uAllocCount * (iArraySize ? iArraySize : 1);
		uAlignFirstToChannel = REG_NO_ALIGNMENT;
	}
	else
	{
		/* Get alloc count, total size and struct alloc info if it is a struct */
		GetTypeAllocInfo(psCPD, psUFContext, psFullType, iArraySize, &uAllocCount, &uTotalSize, &uAlignFirstToChannel, &uCompUseMask, &uIgnored, &psStructAlloc);
	}

	if(psHWSymbol->eRegType == UFREG_TYPE_INDEXABLETEMP)
	{
		uCompOffsetBase = 0;
		psHWSymbol->uIndexableTempTag = psUFContext->uNextArrayTag;
		psUFContext->uNextArrayTag++;

		if(psUFContext->uNextArrayTag >= USC_FIXED_TEMP_ARRAY_SIZE)
		{
			LOG_INTERNAL_ERROR(("AssignHWSymbolRegisters: Number of indexable temp arrays is too big\n"));
			return IMG_FALSE;
		}
	}
	else
	{
		GLSLPrecisionQualifier	ePrecisionQualifier;

		ePrecisionQualifier = psFullType->ePrecisionQualifier;
		/* We always use low precision for booleans */
		if(GLSL_IS_BOOL(psFullType->eTypeSpecifier))
		{
			/* Get default precision for boolean */
			ePrecisionQualifier =  BOOLEAN_PRECISION(psUFContext->psICProgram);
		}
		else if(psStructAlloc && psStructAlloc->uTotalTextureUnits)
		{
			IMG_UINT32 i;
			
			/* Need to check if any of the struct's members are texture samplers, and if so assign them units. 
			   The first unit assigned is recorded in psHWSymbol->uTextureUnit. */
			
			psHWSymbol->uTextureUnit = 0xFFFFFFFF;
			for(i = 0; i < psStructAlloc->uNumBaseTypeMembers; i++)
			{
				GLSLStructMemberAlloc *psBaseMemberAlloc = &psStructAlloc->psBaseMemberAlloc[i];
				if(GLSL_IS_SAMPLER(psBaseMemberAlloc->sFullType.eTypeSpecifier))
				{
					IMG_UINT32 uTextureUnit, uArraySize;
					
					/* The struct contains samplers, so all instances of it must be uniform. */
					DebugAssert(psFullType->eTypeQualifier == GLSLTQ_UNIFORM);

					uArraySize = psBaseMemberAlloc->iArraySize ? psBaseMemberAlloc->iArraySize : 1;
																	     
					AssignSamplerTexUnit(psCPD, psUFContext, psHWSymbol->uSymbolId, uArraySize
						, psBaseMemberAlloc->sFullType.eTypeSpecifier, &uTextureUnit);

					if(psHWSymbol->uTextureUnit == 0xFFFFFFFF)
					{
						/* Only record the texture unit of the first texture sampler in the structure. */
						psHWSymbol->uTextureUnit = uTextureUnit;
						psHWSymbol->eSymbolUsage |= GLSLSU_SAMPLER;
					}
				}
			}
		}

		uCompOffsetBase = UFRegATableAssignRegisters(psUFContext, psTable, uAlignFirstToChannel, uTotalSize, ePrecisionQualifier);

		
		//check for texture samplers inside the struct

		psHWSymbol->uIndexableTempTag = INVALID_ARRAY_TAG;
	}

	/* Assign some important register allocation information for the symbol */
	psHWSymbol->iArraySize			= iArraySize;
	psHWSymbol->uTotalAllocCount	= uTotalSize;
	psHWSymbol->uAllocCount			= uAllocCount;
	psHWSymbol->u.uCompOffset		= uCompOffsetBase;
	psHWSymbol->uCompUseMask		= uCompUseMask;
		
	return IMG_TRUE;
}


/*****************************************************************************
 FUNCTION	: ConvertPrecisionToUFRegFormat

 PURPOSE	: Assign a sampler a tex image unit

 PARAMETERS	: 

 RETURNS	: IMG_TRUE if the component is to be updated; IMG_FALSE otherwise.
*****************************************************************************/
static UF_REGFORMAT ConvertPrecisionToUFRegFormat(GLSLCompilerPrivateData *psCPD,
												  GLSLUniFlexContext		*psUFContext,
												  GLSLFullySpecifiedType	*psFullType)
{
	UF_REGFORMAT eRegFormat = UF_REGFORMAT_F32;

	if(!GLSL_IS_STRUCT(psFullType->eTypeSpecifier))
	{			
#ifdef GLSL_NATIVE_INTEGERS
		if(GLSL_IS_BOOL(psFullType->eTypeSpecifier))
		{
			eRegFormat = UF_REGFORMAT_U32;
		}
		else if(GLSL_IS_INT(psFullType->eTypeSpecifier))
		{
			/* If its an integer and we are doing native integers, then ignore the precision. */
			{
				eRegFormat = UF_REGFORMAT_I32;
			}
		}
		else
#endif
#ifndef GLSL_ES
		{
			/* On desktop GLSL, precision qualifiers have no effect according to the spec, so we always use maximum precision. 
			   You should still be able to comment this code and use the #else code successfully. */
			eRegFormat = UF_REGFORMAT_F32;

			PVR_UNREFERENCED_PARAMETER(psUFContext);
			PVR_UNREFERENCED_PARAMETER(psCPD);
		}
#else
		{
			GLSLPrecisionQualifier ePrecisionQualifier = psFullType->ePrecisionQualifier;

			/* We always use low precision for booleans */
			if(GLSL_IS_BOOL(psFullType->eTypeSpecifier))
			{
				/* Get default precision for boolean */
				ePrecisionQualifier = BOOLEAN_PRECISION(psUFContext->psICProgram);
			}

			switch(ePrecisionQualifier)
			{
			case GLSLPRECQ_UNKNOWN:
				LOG_INTERNAL_ERROR(("ConvertPrecisionToUFRegFormat: Unknown precision qualifier\n"));
#if defined(STANDALONE)
				eRegFormat = UF_REGFORMAT_C10 + 1;
#endif
				break;
			case GLSLPRECQ_HIGH:
				eRegFormat = UF_REGFORMAT_F32;
				break;
			case GLSLPRECQ_MEDIUM:
				eRegFormat = UF_REGFORMAT_F16;
				break;
			case GLSLPRECQ_LOW:
				/* Low precision integers have more range than low precision float, so use mediump float */
				if(GLSL_IS_INT(psFullType->eTypeSpecifier))
				{
					eRegFormat = UF_REGFORMAT_F16;
				}
				else
				{
					eRegFormat = UF_REGFORMAT_C10;
				}
				break;
			default:
#if defined(STANDALONE)
				eRegFormat = UF_REGFORMAT_C10 + 2;
#endif
				LOG_INTERNAL_ERROR(("ConvertPrecisionToUFRegFormat: Rubbish precision qualifier\n"));
				break;
			} /* switch(ePrecisionQualifier) */
		}
#endif
	}

	return eRegFormat;
}


#ifndef OGLES2_VARYINGS_PACKING_RULE

/*****************************************************************************
 FUNCTION	: RecordTexCoordInfoForSymbol

 PURPOSE	: Setup active varying mask, texture dimension, cnetroil mask etc.

 PARAMETERS	: 

 RETURNS	: 
*****************************************************************************/
static IMG_VOID RecordTexCoordInfoForSymbol(GLSLUniFlexContext *psUFContext, HWSYMBOL *psHWSymbol)
{
	GLSLTypeSpecifier eTypeSpecifier = psHWSymbol->sFullType.eTypeSpecifier;
	IMG_UINT32 uNum;
	IMG_UINT32 uNumComponents = TYPESPECIFIER_NUM_COMPONENTS(eTypeSpecifier);
	IMG_UINT32 uComponentsLeft, uUnitComponents;

	DebugAssert(REG_COMPSTART(psHWSymbol->u.uCompOffset) == 0);

	/* Varing cannot be a structure */
	DebugAssert(!GLSL_IS_STRUCT(eTypeSpecifier));

	uNum = REG_INDEX(psHWSymbol->u.uCompOffset);
	uComponentsLeft = psHWSymbol->uTotalAllocCount;
	
	while(uComponentsLeft)
	{
		uUnitComponents = uComponentsLeft;

		if(uUnitComponents >= REG_COMPONENTS) uUnitComponents = REG_COMPONENTS;

		if(uNumComponents == 3)
		{
			/* For arrays of vec3, mat2x3, mat3x3 and mat3x4 */
			psUFContext->auTexCoordDims[uNum] = uNumComponents;
		}
		else
		{
			DebugAssert(psHWSymbol->uAllocCount == uNumComponents * TYPESPECIFIER_NUM_COLS(eTypeSpecifier));
				
			/* There is no gap between array elements, for arrays of floats, vec2s and vec4s */
			psUFContext->auTexCoordDims[uNum] = uUnitComponents;
		}

		/*
			Leave a gap in vertex shader output if the varying is a scalar.

			The minimum number of texture coordinate size in vertex shader output is 2

			See Eurasia.3D Input Parameter Format.doc
		*/
		if(psHWSymbol->eRegType == UFREG_TYPE_VSOUTPUT && psUFContext->auTexCoordDims[uNum] < 2)
		{
			psUFContext->auTexCoordDims[uNum] = 2;
		}

		psUFContext->aeTexCoordPrecisions[uNum] = psHWSymbol->sFullType.ePrecisionQualifier;

		psUFContext->eActiveVaryingMask |= (GLSLVM_TEXCOORD0 << uNum);

		if(psHWSymbol->sFullType.eVaryingModifierFlags & GLSLVMOD_CENTROID)
		{
			psUFContext->uTexCoordCentroidMask |= (1 << uNum);
		}

		uComponentsLeft -= uUnitComponents;

		uNum++;
	}
}

#endif

/*****************************************************************************
 FUNCTION	: AllocateHWSymbolRegister

 PURPOSE	: Assign hw symbol for a symbol ID. If bForceTemp is true, use
			  indexable temporarys for dynamiccally indexed attribs and varyings

 PARAMETERS	:

 RETURNS	: HW symbol entry
*****************************************************************************/
static IMG_BOOL AllocateHWSymbolRegisters(GLSLCompilerPrivateData *psCPD,
										  GLSLUniFlexContext	*psUFContext,
										  HWSYMBOL			*psHWSymbol)
{
	UFRegAsssignmentTable	*psTable;
	UF_REGTYPE				eRegType ;
	IMG_BOOL				bDynamicallyIndexed = (psHWSymbol->eSymbolUsage & GLSLSU_DYNAMICALLY_INDEXED) ? IMG_TRUE : IMG_FALSE;
	GLSLFullySpecifiedType  *psFullType = &psHWSymbol->sFullType;

	if(psHWSymbol->bRegAllocated)
	{
		return IMG_TRUE;
	}

	if(GLSL_IS_SAMPLER(psHWSymbol->sFullType.eTypeSpecifier))
	{
		IMG_UINT32 uArraySize = psHWSymbol->iArraySize ? psHWSymbol->iArraySize : 1;
		IMG_UINT32 uTexUnit;
			
		if(!AssignSamplerTexUnit(psCPD, psUFContext, psHWSymbol->uSymbolId, uArraySize
			, psFullType->eTypeSpecifier, &uTexUnit))
		{
			LOG_INTERNAL_ERROR(("AllocateHWSymbolRegisters: Failed to find texture unit for a sampler \n"));
			return IMG_FALSE;
		}
		
		psHWSymbol->uTextureUnit		= uTexUnit;
		psHWSymbol->uAllocCount			= 1;
		psHWSymbol->uTotalAllocCount	= uArraySize;
		psHWSymbol->uCompUseMask		= 1;
		psHWSymbol->eRegType			= UFREG_TYPE_TEX; 
		psHWSymbol->uIndexableTempTag	= INVALID_ARRAY_TAG;
		psHWSymbol->eSymbolUsage		|= GLSLSU_SAMPLER;
	}
	else
	{
		if(((psHWSymbol->eBuiltinID == GLSLBV_FRONTFACING		||
			 psHWSymbol->eBuiltinID == GLSLBV_FRAGCOORD			||
			 psHWSymbol->eBuiltinID == GLSLBV_COLOR				|| 
			 psHWSymbol->eBuiltinID == GLSLBV_SECONDARYCOLOR	) &&
			psUFContext->eProgramType == GLSLPT_FRAGMENT ) ||
		   (psHWSymbol->eBuiltinID == GLSLBV_CLIPVERTEX) )
		{
			/*
				Force gl_FrontFacing, glColor, glSecondaryColor and gl_ClipVertex to temporary since we need to do extra work 
				at the start of the program to get the right format
			*/
			eRegType = UFREG_TYPE_TEMP;
			psTable = &psUFContext->sTempTab;
		}
		else
		{
			/* Which register file will this symbol be put in. */
			switch (psFullType->eTypeQualifier)
			{
				default: 
				case GLSLTQ_TEMP:
					if(psHWSymbol->eSymbolUsage & GLSLSU_BOOL_AS_PREDICATE)
					{
						eRegType = UFREG_TYPE_PREDICATE;
						psTable = &psUFContext->sPredicateTab;
					}
					else if(bDynamicallyIndexed)
					{
						eRegType = UFREG_TYPE_INDEXABLETEMP;
						psTable = IMG_NULL;
					}
					else
					{
						eRegType = UFREG_TYPE_TEMP; 
						psTable = &psUFContext->sTempTab; 
					}

					break;
				case GLSLTQ_CONST:
				case GLSLTQ_UNIFORM: 
					if(psFullType->eTypeQualifier == GLSLTQ_CONST && psFullType->eParameterQualifier != GLSLPQ_INVALID)
					{
						if(bDynamicallyIndexed)
						{
							eRegType = UFREG_TYPE_INDEXABLETEMP;
							psTable = IMG_NULL;
						}
						else
						{
							eRegType = UFREG_TYPE_TEMP; 
							psTable = &psUFContext->sTempTab; 
						}
					}
					else
					{
						eRegType = UFREG_TYPE_CONST; 
						psTable = &psUFContext->sFloatConstTab; 
					}
					break;
				case GLSLTQ_VERTEX_IN:
					DebugAssert(psUFContext->eProgramType == GLSLPT_VERTEX);
					eRegType = UFREG_TYPE_VSINPUT;
					psTable = &psUFContext->sAttribTab;
					break;
#ifndef OGLES2_VARYINGS_PACKING_RULE
				case GLSLTQ_VERTEX_OUT:
					eRegType = UFREG_TYPE_VSOUTPUT;
					psTable = &psUFContext->sTexCoordTab; 
					break;
				case GLSLTQ_FRAGMENT_IN:
					DebugAssert(psUFContext->eProgramType == GLSLPT_FRAGMENT);
					eRegType = UFREG_TYPE_TEXCOORD;
					psTable = &psUFContext->sTexCoordTab; 
					break;
#else
				case GLSLTQ_VERTEX_OUT:
				case GLSLTQ_FRAGMENT_IN:
					LOG_INTERNAL_ERROR(("AllocateHWSymbolRegisters: All varyings should've been allocated\n"));				
					return IMG_FALSE;
#endif
				case GLSLTQ_FRAGMENT_OUT:
					eRegType = UFREG_TYPE_PSOUTPUT;
					psTable = &psUFContext->sFragmentOutTab;
					break;
			}
		}

		psHWSymbol->eRegType = eRegType;

		if(!AssignHWSymbolRegisters(psCPD, psUFContext, psTable, psHWSymbol, psHWSymbol->iArraySize))
		{
			LOG_INTERNAL_ERROR(("AllocateHWSymbolRegisters: Failed to assign symbol registers\n"));
			DebugMemFree(psHWSymbol);
			return IMG_FALSE;
		}

#ifndef OGLES2_VARYINGS_PACKING_RULE
		/* Setup active varying mask, texture dimension, cnetroil mask etc. */
		if(psHWSymbol->eRegType == UFREG_TYPE_VSOUTPUT || psHWSymbol->eRegType == UFREG_TYPE_TEXCOORD)
		{
			RecordTexCoordInfoForSymbol(psUFContext, psHWSymbol);
		}
#endif
	}

	if(psHWSymbol->eRegType == UFREG_TYPE_INDEXABLETEMP)
	{
		AddToIndexableTempList(psCPD, psUFContext, psHWSymbol);
	}
	else if(psHWSymbol->eRegType == UFREG_TYPE_CONST && bDynamicallyIndexed)
	{
		AddToRangeList(psCPD, &psUFContext->sConstRanges, psHWSymbol);
	}
	else if (psHWSymbol->eRegType == UFREG_TYPE_TEXCOORD && bDynamicallyIndexed)
	{
		AddToRangeList(psCPD, &psUFContext->sVaryingRanges, psHWSymbol);
	}

	/* Indicate the register has been allocated for the symbol */
	psHWSymbol->bRegAllocated = IMG_TRUE;
	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: UpdateOperandUFSwiz

 PURPOSE	: Update operand swiz due to icode swiz change

 PARAMETERS	:

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID UpdateOperandUFSwiz(GLSLCompilerPrivateData *psCPD, ICUFOperand *psICUFOperand)
{
	IMG_UINT32 uUFSwiz = 0;
	IMG_UINT32 i;
	GLSLICVecSwizWMask *psSwizMask = &psICUFOperand->sICSwizMask;

	/* Unused in release builds */
	PVR_UNREFERENCED_PARAMETER(psCPD);

	/* GLSL type specifier after swiz */
	psICUFOperand->eTypeAfterSwiz = psICUFOperand->sFullType.eTypeSpecifier;

	/* If it is a struct or matrix, just return default swiz */
	if(GLSL_IS_STRUCT(psICUFOperand->sFullType.eTypeSpecifier) || GLSL_IS_MATRIX(psICUFOperand->sFullType.eTypeSpecifier))
	{
		psICUFOperand->uUFSwiz = UFREG_SWIZ_RGBA;

		return;
	}

	/* Work out type specifier after swiz for a vector */
	if(GLSL_IS_VECTOR(psICUFOperand->sFullType.eTypeSpecifier))
	{
		if(psSwizMask->uNumComponents)
		{
			IMG_UINT uComponents = psSwizMask->uNumComponents;

			if(uComponents < TYPESPECIFIER_NUM_COMPONENTS(psICUFOperand->sFullType.eTypeSpecifier))
			{
				psICUFOperand->eTypeAfterSwiz = CONVERT_TO_VECTYPE(psICUFOperand->sFullType.eTypeSpecifier, uComponents); 
			}
		}
	}

	/* Work out UF swiz */
	if (GLSL_IS_SCALAR(psICUFOperand->sFullType.eTypeSpecifier))
	{
		DebugAssert(psICUFOperand->uCompStart < 4);
		uUFSwiz = g_auSingleComponentSwiz[psICUFOperand->uCompStart];
	}
	else if(!GLSL_IS_SAMPLER(psICUFOperand->sFullType.eTypeSpecifier))
	{
		/* Build-up the swizzle needed to access the components of the variable. */
		if (psSwizMask->uNumComponents == 0)
		{
			DebugAssert(psICUFOperand->uCompStart < 4);
			uUFSwiz = g_auRestOfColumnSwiz[psICUFOperand->uCompStart];
		}
		else
		{
			IMG_UINT32 uNumComp = psSwizMask->uNumComponents;

			uUFSwiz = 0;

			for (i = 0; i < uNumComp; i++)
			{
				uUFSwiz |= ((psICUFOperand->uCompStart) + psSwizMask->aeVecComponent[i]) << (i * 3);
			}
			/* Replicate other components the same as the last one */
			for (; i < REG_COMPONENTS; i++)
			{
				uUFSwiz |= (psICUFOperand->uCompStart + psSwizMask->aeVecComponent[uNumComp - 1]) << (i * 3);
			}
		}
	}

	psICUFOperand->uUFSwiz = uUFSwiz;
}



/*****************************************************************************
 FUNCTION	: ApplyMoreSwiz

 PURPOSE	: Apply more swiz to the original operand

 PARAMETERS	: psICUFOperand		- Operand to apply swiz to
			  uSwiz				- Swiz in terms of defines in icuf.h

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ApplyMoreSwiz(GLSLCompilerPrivateData *psCPD, ICUFOperand *psICUFOperand, IMG_UINT32 uSwiz)
{
	GLSLICVecSwizWMask sSwizMask;

	sSwizMask.uNumComponents = SWIZ_NUM(uSwiz);
	sSwizMask.aeVecComponent[0] = SWIZ_0COMP(uSwiz);
	sSwizMask.aeVecComponent[1] = SWIZ_1COMP(uSwiz);
	sSwizMask.aeVecComponent[2] = SWIZ_2COMP(uSwiz);
	sSwizMask.aeVecComponent[3] = SWIZ_3COMP(uSwiz);

	ICCombineTwoConstantSwizzles(&psICUFOperand->sICSwizMask, &sSwizMask);

	UpdateOperandUFSwiz(psCPD, psICUFOperand);
}

/*****************************************************************************
 FUNCTION	: ChooseMatrixColumn

 PURPOSE	: Working out operand for a specific column of a matrix

 PARAMETERS	:

 RETURNS	:
*****************************************************************************/
static IMG_VOID ChooseMatrixColumn(GLSLCompilerPrivateData *psCPD, ICUFOperand *psMatrix, ICUFOperand *psColumn, IMG_UINT32 uCol)
{
	psColumn->sICSwizMask = psMatrix->sICSwizMask;

	if(GLSL_IS_MATRIX(psMatrix->eTypeAfterSwiz))
	{
		IMG_UINT32 uNumCols = TYPESPECIFIER_NUM_COLS(psMatrix->eTypeAfterSwiz);

		IMG_UINT32 uAllocCountPerColumn = psMatrix->uAllocCount / uNumCols;

		IMG_UINT32 uCompOffset = psMatrix->uRegNum * REG_COMPONENTS + uCol * uAllocCountPerColumn;

		psColumn->sFullType.eTypeSpecifier = INDEXED_MAT_TYPE(psMatrix->eTypeAfterSwiz);
		psColumn->eTypeAfterSwiz = psColumn->sFullType.eTypeSpecifier;

		psColumn->uRegNum = REG_INDEX(uCompOffset);
		psColumn->uCompStart = REG_COMPSTART(uCompOffset);

		UpdateOperandUFSwiz(psCPD, psColumn);
	}
	else
	{
		psColumn->sFullType.eTypeSpecifier = psColumn->eTypeAfterSwiz = psMatrix->eTypeAfterSwiz;

		psColumn->uRegNum = psMatrix->uRegNum;
	}
}

/*****************************************************************************
 FUNCTION	: ChooseMatrixComponent

 PURPOSE	: Working out operand for a specific component of a matrix

 PARAMETERS	: 

 RETURNS	: 
*****************************************************************************/
static const IMG_UINT32 gauSingleSwiz[] = {SWIZ_X, SWIZ_Y, SWIZ_Z, SWIZ_W};

static IMG_VOID ChooseMatrixComponent(GLSLCompilerPrivateData *psCPD,
									  ICUFOperand			*psMatrix,
									  ICUFOperand			*psComponent,
									  IMG_UINT32			uCol,
									  GLSLICVecComponent	eComponent)
{
	psComponent->sICSwizMask = psMatrix->sICSwizMask;

	if(GLSL_IS_MATRIX(psMatrix->eTypeAfterSwiz))
	{
		IMG_UINT32 uNumCols = TYPESPECIFIER_NUM_COLS(psMatrix->eTypeAfterSwiz);
		IMG_UINT32 uCompOffset = psMatrix->uRegNum * 4 + uCol * psMatrix->uAllocCount/uNumCols;

		psComponent->sFullType.eTypeSpecifier = INDEXED_MAT_TYPE(psMatrix->sFullType.eTypeSpecifier);
		psComponent->eTypeAfterSwiz = psComponent->sFullType.eTypeSpecifier;

		psComponent->uRegNum = REG_INDEX(uCompOffset);
		psComponent->uCompStart = REG_COMPSTART(uCompOffset);

		UpdateOperandUFSwiz(psCPD, psComponent);
	}
	else
	{
		psComponent->sFullType.eTypeSpecifier = psComponent->eTypeAfterSwiz = psMatrix->sFullType.eTypeSpecifier;

		psComponent->uRegNum = psMatrix->uRegNum;
	}

	if(GLSL_IS_VECTOR(psComponent->eTypeAfterSwiz))
	{
		ApplyMoreSwiz(psCPD, psComponent, gauSingleSwiz[eComponent]);
	}
}

/*****************************************************************************
 FUNCTION	: ChooseVectorComponent

 PURPOSE	: Working out operand for a specific component of a vector

 PARAMETERS	:

 RETURNS	:
*****************************************************************************/
static IMG_VOID ChooseVectorComponent(GLSLCompilerPrivateData	*psCPD,
									  const ICUFOperand			*psVector,
									  ICUFOperand				*psComponent,
									  GLSLICVecComponent		eComponent)
{
	psComponent->sICSwizMask = psVector->sICSwizMask;

	if(GLSL_IS_VECTOR(psVector->eTypeAfterSwiz))
	{
		ApplyMoreSwiz(psCPD, psComponent, gauSingleSwiz[eComponent]);
	}
}

/*****************************************************************************
 FUNCTION	: ChooseArrayElement

 PURPOSE	: Working out operand for a specific element of an array

 PARAMETERS	:

 RETURNS	:
*****************************************************************************/
static IMG_VOID ChooseArrayElement(GLSLCompilerPrivateData *psCPD,
								   ICUFOperand	*psArrayOperand,
								   ICUFOperand	*psElement,
								   IMG_UINT32	uIndex)
{
	IMG_UINT32 uCompOffset;

	PVR_UNREFERENCED_PARAMETER(psCPD);

	DebugAssert(uIndex < (IMG_UINT32)psArrayOperand->iArraySize);

	uCompOffset = psArrayOperand->uRegNum * REG_COMPONENTS 
				+ psArrayOperand->uCompStart 
			    + psArrayOperand->uAllocCount * uIndex;

	psElement->iArraySize = 0;
	psElement->uRegNum = REG_INDEX(uCompOffset);
	psElement->uCompStart = REG_COMPSTART(uCompOffset);

	UpdateOperandUFSwiz(psCPD, psElement);
}

/*****************************************************************************
 FUNCTION	: ChooseArrayElement

 PURPOSE	: Working out operand for a specific base member of a struct 

 PARAMETERS	: 

 RETURNS	: 
*****************************************************************************/
static IMG_VOID ChooseStructMember(GLSLCompilerPrivateData *psCPD,
								   GLSLUniFlexContext	*psUFContext, 
								   ICUFOperand			*psStructOperand,
								   GLSLStructureAlloc	*psStructAlloc,
								   ICUFOperand			*psMemberOperand,
								   IMG_UINT32			uMemberIndex)
{
	IMG_UINT uCompOffset;

	PVR_UNREFERENCED_PARAMETER(psCPD);

	DebugAssert(GLSL_IS_STRUCT(psStructOperand->eTypeAfterSwiz));
	DebugAssert(psStructOperand->sFullType.uStructDescSymbolTableID);
	DebugAssert(uMemberIndex < psStructAlloc->uNumMembers);

	uCompOffset = psStructOperand->uRegNum * REG_COMPONENTS 
				+ psStructAlloc->psMemberAlloc[uMemberIndex].uCompOffset;

	psMemberOperand->uRegNum = REG_INDEX(uCompOffset);

	psMemberOperand->iArraySize = psStructAlloc->psMemberAlloc[uMemberIndex].iArraySize;

	psMemberOperand->uCompStart = REG_COMPSTART(uCompOffset);

	psMemberOperand->uAllocCount = psStructAlloc->psMemberAlloc[uMemberIndex].uAllocCount;

	psMemberOperand->sFullType = psStructAlloc->psMemberAlloc[uMemberIndex].sFullType;

	psMemberOperand->eRegFormat = ConvertPrecisionToUFRegFormat(psCPD, psUFContext, &psMemberOperand->sFullType);

	UpdateOperandUFSwiz(psCPD, psMemberOperand);
}

/*****************************************************************************
 FUNCTION	: AddAluToUFCode

 PURPOSE	: Add a single UniFlex instruction

 PARAMETERS	: psDest			- Destination operand.
			  psSrcA			- First source operand.
			  psSrcB			- Second source operand.
			  psSrcC			- Third source operand.
			  eUFOpcode			- UniFlex opcode to use.
			  uNumSrcArgs		- The number of the arguments to the ALU operation.
			  bComponentWise	- Component wise or not 
								  Should not be component-wise for DOT2ADD, DOT3, DOT4, CRS, SINCOS

 RETURNS	: 
*****************************************************************************/
static PUNIFLEX_INST AddAluToUFCode(GLSLUniFlexContext	*psUFContext, 
									ICUFOperand			*psDest,
									ICUFOperand			*psSrcA,
									ICUFOperand			*psSrcB,
									ICUFOperand			*psSrcC,
									UF_OPCODE			eUFOpcode,
									IMG_UINT32			uNumSrcs,
									IMG_BOOL				bComponentWise)
{

	PUNIFLEX_INST psProg;
	IMG_UINT32 uDestSwiz;
	IMG_UINT32 i;
	ICUFOperand *apsSrcs[3];

	apsSrcs[0] = psSrcA;
	apsSrcs[1] = psSrcB;
	apsSrcs[2] = psSrcC;

	psProg = CreateInstruction(psUFContext, eUFOpcode);		if(!psProg) return IMG_NULL;

	{
		ConvertToUFDestination(&psProg->sDest, psDest, &uDestSwiz);
	}

	for (i = 0; i < uNumSrcs; i++)
	{
		if(apsSrcs[i] == NULL)
		{
			psProg->asSrc[i] = g_sSourceZero;
		}
		else
		{
			ConvertToUFSource(&psProg->asSrc[i], apsSrcs[i], bComponentWise, psDest->uCompStart, uDestSwiz);
		}
	}

	return psProg;
}

/*****************************************************************************
 FUNCTION	: AddIFCToUFCode

 PURPOSE	: Add IFC instruction

 PARAMETERS	: psUFContext		- UF context.
			  psSrcA			- Source operand.
			  eCompareOp		- Compare op.

 RETURNS	: IMG_TRUE if successful
*****************************************************************************/
static IMG_BOOL AddIFCToUFCode(GLSLUniFlexContext	*psUFContext,
							   ICUFOperand			*psSrcA,
							   UFREG_COMPOP			eCompareOp)
{
	PUNIFLEX_INST psProg;

	psProg = CreateInstruction(psUFContext, UFOP_IFC);		if(!psProg) return IMG_FALSE;

	/* Source 0 */
	ConvertToUFSource(&psProg->asSrc[0], psSrcA, IMG_FALSE, 0, 0);

	/* Source 1 */
	InitUFSource(&psProg->asSrc[1], UFREG_TYPE_COMPOP, eCompareOp, UFREG_SWIZ_NONE);

	/* Source 2 */
	psProg->asSrc[2] = g_sSourceZero;
	psProg->asSrc[2].eFormat = psProg->asSrc[0].eFormat;

	return IMG_TRUE;
}

typedef struct _ICUFRelativeIndex
{
	ICUFOperand		sIndexOperand;
	IMG_UINT32		uIndexUnitsInComponents;
} ICUFRelativeIndex;

/*****************************************************************************
 FUNCTION	: gcd

 PURPOSE	: Computes the greatest common multiple.

 PARAMETERS	: a, b	- 

 RETURNS	: The greatest t such that a % t == 0 and b % t == 0
*****************************************************************************/
static IMG_UINT32 gcd(IMG_UINT32 a, IMG_UINT32 b)
{
	while (b != 0)
	{
		IMG_UINT32 t;

		t = b;
		b = a % b;
		a = t;
	}
	return a;
}

/*****************************************************************************
 FUNCTION	: AddUpRelativeOffsetToUFCode

 PURPOSE	: Add UF instruction to calculate the index offset

 PARAMETERS	: psUFContext			- UF context
			  uMultiplier			- Multiplier
			  uOffsetSymID			- Offset symbol ID
			  ppsFinalIndexOperand	- Index operand

 RETURNS	: 
*****************************************************************************/
static IMG_BOOL AddUpRelativeOffsetToUFCode(GLSLCompilerPrivateData *psCPD,
											GLSLUniFlexContext	*psUFContext,
											IMG_UINT32			uMultiplier,
											IMG_UINT32			uOffsetSymID,
											ICUFRelativeIndex	**ppsRelativeIndex)
{

	ICUFRelativeIndex *psRelativeIndex = *ppsRelativeIndex;

	ICUFOperand sOffset;

	GLSLPrecisionQualifier ePrecision;

	InitICUFOperand(psCPD, psUFContext, uOffsetSymID, &sOffset, IMG_FALSE);
	ePrecision = sOffset.sFullType.ePrecisionQualifier;

	if (psRelativeIndex)
	{
		IMG_UINT32	uExistingIndexUnits = psRelativeIndex->uIndexUnitsInComponents;
		IMG_UINT32	uPreviousIndexScale;
		IMG_UINT32	uThisIndexScale;
		IMG_UINT32	uNewIndexUnits;

		/*
			Scale up the existing index by the GCD of its multiplier and the multipler for the
			current index.
		*/
		DebugAssert(uExistingIndexUnits >= uMultiplier);
		uNewIndexUnits = gcd(uExistingIndexUnits, uMultiplier);
		uPreviousIndexScale = uExistingIndexUnits / uNewIndexUnits;
		uThisIndexScale = uMultiplier / uNewIndexUnits;

		/*
			Update the multipler for the cumulative index.
		*/
		psRelativeIndex->uIndexUnitsInComponents = uNewIndexUnits;

		if (uPreviousIndexScale > 1)
		{
			ICUFOperand		sPreviousIndexScale;
			ICUFOperand		sScaledOffset;
			ICUFOperand*	psFinalOffset;

			if (uThisIndexScale > 1)
			{
				ICUFOperand	sThisIndexScale;

				/*	
					Get a temporary to hold the scaled value of the current index.
				*/
				GetTemporary(psCPD, psUFContext, GLSLTS_INT, ePrecision, &sScaledOffset);

				/*
					Get a uniform holding the scale for the current index.
				*/
				GetIntConst(psCPD, psUFContext, (IMG_INT32)uThisIndexScale, ePrecision, &sThisIndexScale);

				/*
					Calculate
						CURRENT_INDEX' = CURRENT_INDEX * THIS_INDEX_SCALE
				*/
				AddAluToUFCode(psUFContext, 
							   &sScaledOffset,
							   &sOffset,
							   &sThisIndexScale,
							   NULL,
							   UFOP_MUL,
							   2,
							   IMG_TRUE);

				psFinalOffset = &sScaledOffset;
			}
			else
			{
				psFinalOffset = &sOffset;
			}

			/*
				Get a uniform containing the value to scale the previous index by.
			*/
			GetIntConst(psCPD, psUFContext, (IMG_INT32)uPreviousIndexScale, ePrecision, &sPreviousIndexScale);

			/*
				Calculate
					NEW_INDEX = PREVIOUS_INDEX * PREVIOUS_INDEX_SCALE + CURRENT_INDEX
			*/
			AddAluToUFCode(psUFContext, 
						   &psRelativeIndex->sIndexOperand, 
						   &psRelativeIndex->sIndexOperand, 
						   &sPreviousIndexScale, 
						   psFinalOffset, 
						   UFOP_MAD, 
						   3, 
						   IMG_TRUE);
		}
		else
		{
			DebugAssert(uThisIndexScale == 1);

			/*
				Calculate
					NEW_INDEX = PREVIOUS_INDEX + CURRENT_INDEX
			*/
			AddAluToUFCode(psUFContext, 
						   &psRelativeIndex->sIndexOperand, 
						   &sOffset, 
						   &psRelativeIndex->sIndexOperand, 
						   NULL,
						   UFOP_ADD, 
						   2, 
						   IMG_TRUE);
		}
	}
	else
	{
		psRelativeIndex = DebugMemCalloc(sizeof(*psRelativeIndex));

		/*
			Get a temporary variable to hold the final index (the sum of the different indices used in
			different parts of the expression).
		*/
		GetTemporary(psCPD, psUFContext, GLSLTS_INT, ePrecision, &psRelativeIndex->sIndexOperand);

		/*
			Copy from the index to the temporary variable.
		*/
		AddAluToUFCode(psUFContext, &psRelativeIndex->sIndexOperand, &sOffset, NULL, NULL, UFOP_MOV, 1, IMG_TRUE);

		/*
			Store the multipler for the index.
		*/
		psRelativeIndex->uIndexUnitsInComponents = uMultiplier;
	}

	*ppsRelativeIndex = psRelativeIndex;

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: GetHWSymbolEntry

 PURPOSE	: Get a HW symbol entry for the symbol ID, this function does not actually 
			  allocate registers if it is not a fixed reg.

 PARAMETERS	: psUFContext, uSymbolID, bDirectRegister
 
 RETURNS	: HW symbol if successful
*****************************************************************************/
static HWSYMBOL *GetHWSymbolEntry(GLSLCompilerPrivateData *psCPD,
								  GLSLUniFlexContext *psUFContext, IMG_UINT32 uSymbolID, IMG_BOOL bDirectRegister)
{
	HWSYMBOL *psHWSymbol;

	psHWSymbol = FindHWSymbol(psUFContext, uSymbolID, bDirectRegister);

	if(psHWSymbol == IMG_NULL)
	{
		GLSLFullySpecifiedType *psFullType;
		GLSLBuiltInVariableID eBuiltinID;
		GLSLIdentifierUsage eIdentifierUsage;
		IMG_BOOL bFixedReg = IMG_FALSE;
		IMG_INT32 iArraySize;

		psHWSymbol = DebugMemCalloc(sizeof(HWSYMBOL));
					
		if(psHWSymbol == IMG_NULL)
		{
			LOG_INTERNAL_ERROR(("AddHWSymbolToSymbolList: Failed to allocate memory"));

			return IMG_NULL;
		}
			
		/* Get Symbol information */
		ICGetSymbolInformation(psCPD, psUFContext->psSymbolTable, uSymbolID, &eBuiltinID, &psFullType, &iArraySize, &eIdentifierUsage, IMG_NULL);

		/* Collect initial information directly from symbol ID */
		psHWSymbol->uSymbolId			= uSymbolID;
		psHWSymbol->eBuiltinID			= eBuiltinID;
		psHWSymbol->sFullType			= *psFullType;
		psHWSymbol->iArraySize			= iArraySize;

#if defined(DEBUG)
		psHWSymbol->psSymbolName		= GetSymbolName(psUFContext->psSymbolTable, uSymbolID); 
#endif

		psHWSymbol->uIndexableTempTag	= INVALID_ARRAY_TAG;
		psHWSymbol->uCompUseMask		= psUFContext->auCompUseMask[psFullType->eTypeSpecifier];

		psHWSymbol->bDirectRegister		= bDirectRegister;
		psHWSymbol->bRegAllocated		= IMG_FALSE;
		psHWSymbol->eSymbolUsage		= (GLSLHWSymbolUsage)0;
		
		if(eIdentifierUsage & GLSLIU_BOOL_AS_PREDICATE)
		{
			psHWSymbol->eSymbolUsage |= GLSLSU_BOOL_AS_PREDICATE;
		}

		/* Check whether it is a fixed reg, if so, assign register at this moment */
		switch(eBuiltinID)
		{
		case GLSLBV_NOT_BTIN:
			break;
		case GLSLBV_POSITION:
		{
			DebugAssert(psUFContext->eProgramType == GLSLPT_VERTEX);

			psUFContext->eActiveVaryingMask |= GLSLVM_POSITION;
		
			psHWSymbol->eRegType			= UFREG_TYPE_VSOUTPUT;
			psHWSymbol->uAllocCount			= 4;
			psHWSymbol->uTotalAllocCount	= psHWSymbol->uAllocCount;
			psHWSymbol->u.uFixedRegNum	= GLSLVR_POSITION;

			bFixedReg = IMG_TRUE;		

			break;
		}
		case GLSLBV_POINTSIZE:
		{
			DebugAssert(psUFContext->eProgramType == GLSLPT_VERTEX);
			psUFContext->eActiveVaryingMask |= GLSLVM_PTSIZE;

			psHWSymbol->eRegType			= UFREG_TYPE_VSOUTPUT;		
			psHWSymbol->uAllocCount			= 1;
			psHWSymbol->uTotalAllocCount	= psHWSymbol->uAllocCount;
			psHWSymbol->u.uFixedRegNum		= GLSLVR_PTSIZE;

			bFixedReg = IMG_TRUE;

			break;
		}
		case GLSLBV_CLIPVERTEX:
		{
			DebugAssert(psUFContext->eProgramType == GLSLPT_VERTEX);

			break;
		}
		case GLSLBV_FRAGCOORD:
		{
			psUFContext->bFragCoordUsed = IMG_TRUE;

			break;
		}
		case GLSLBV_FOGFRAGCOORD:
		{
			if(psUFContext->eProgramType == GLSLPT_VERTEX)
			{
				psHWSymbol->eRegType			= UFREG_TYPE_VSOUTPUT;
				psHWSymbol->uAllocCount			= 1;
				psHWSymbol->uTotalAllocCount	= psHWSymbol->uAllocCount;
				psHWSymbol->u.uFixedRegNum		= GLSLVR_FOG;

				psUFContext->eActiveVaryingMask |= GLSLVM_FOG;
			}
			else
			{
				psHWSymbol->eRegType			= UFREG_TYPE_MISC;		
				psHWSymbol->uAllocCount			= 1;
				psHWSymbol->uTotalAllocCount	= psHWSymbol->uAllocCount;
				psHWSymbol->u.uFixedRegNum		= UF_MISC_FOG;
			}

			bFixedReg = IMG_TRUE;
			break;
		}
#if USE_INTERGER_COLORS
		case GLSLBV_FRONTCOLOR:
			psUFContext->eActiveVaryingMask |= GLSLVM_COLOR0;

			if(psUFContext->eProgramType == GLSLPT_VERTEX)
			{

				psHWSymbol->eRegType			= UFREG_TYPE_VSOUTPUT;		
				psHWSymbol->uAllocCount			= 4;
				psHWSymbol->uTotalAllocCount	= psHWSymbol->uAllocCount;
				psHWSymbol->u.uFixedRegNum		= GLSLVR_COLOR0;

			}
			else
			{
				psHWSymbol->eRegType			= UFREG_TYPE_COL;		
				psHWSymbol->uAllocCount			= 4;
				psHWSymbol->uTotalAllocCount	= psHWSymbol->uAllocCount;
				psHWSymbol->u.uFixedRegNum		= 0;
			}

			bFixedReg = IMG_TRUE;
			break;
		case GLSLBV_FRONTSECONDARYCOLOR:
			psUFContext->eActiveVaryingMask |= GLSLVM_COLOR1;

			if(psUFContext->eProgramType == GLSLPT_VERTEX)
			{
				psHWSymbol->eRegType			= UFREG_TYPE_VSOUTPUT;		
				psHWSymbol->uAllocCount			= 4;
				psHWSymbol->uTotalAllocCount	= psHWSymbol->uAllocCount;
				psHWSymbol->u.uFixedRegNum		= GLSLVR_COLOR1;

			}
			else
			{
				psHWSymbol->eRegType			= UFREG_TYPE_COL;		
				psHWSymbol->uAllocCount			= 4;
				psHWSymbol->uTotalAllocCount	= psHWSymbol->uAllocCount;
				psHWSymbol->u.uFixedRegNum		= 1;

			}

			bFixedReg = IMG_TRUE;

			break;
#endif
		case GLSLBV_COLOR:
			if(psUFContext->eProgramType == GLSLPT_FRAGMENT)
			{
				psUFContext->bColorUsed = IMG_TRUE;
			}
			break;
		case GLSLBV_SECONDARYCOLOR:
			if(psUFContext->eProgramType == GLSLPT_FRAGMENT)
			{
				psUFContext->bSecondaryColorUsed = IMG_TRUE;
			}
			break;
		case GLSLBV_FRONTFACING:

			psUFContext->bFrontFacingUsed = IMG_TRUE;
			break;
		default:
			break;

		}

		if(bFixedReg)
		{
			psHWSymbol->bRegAllocated	= IMG_TRUE;
			psHWSymbol->eSymbolUsage	|= GLSLSU_FIXED_REG;
		}

		/* Add to the HW symbol table */
		AddHWSymbolToList(psCPD, psUFContext, psHWSymbol);
	}

	return psHWSymbol;
}

/*****************************************************************************
 FUNCTION	: ProcessOperandOffsets

 PURPOSE	: Builds UF operand info by taking IC operand offsets 

 PARAMETERS	: psUFContext		- UF context
			  eTypeSpecifier	- Type specifier for the base symbol
			  psHWSymbol		- HW symbol 
			  psICUFOperand		- UF operand to be built
 
 RETURNS	: IMG_TRUE if successful
*****************************************************************************/
static IMG_BOOL ProcessOperandOffsets(GLSLCompilerPrivateData *psCPD,
									  GLSLUniFlexContext		*psUFContext,
									  const HWSYMBOL			*psHWSymbol,
									  const GLSLICOperand		*psICOperand,
									  ICUFOperand				*psICUFOperand)
{
	IMG_UINT32 i, uOffsetLeftStart = 0;
	GLSLFullySpecifiedType sFullType = psHWSymbol->sFullType;
	IMG_INT32 iArraySize = sFullType.iArraySize;

	ICUFRelativeIndex* psRelativeIndex = IMG_NULL;
	IMG_UINT32 uRegNum, uCompStart, uAllocCount = 0;

	if(psHWSymbol->eSymbolUsage & GLSLSU_FIXED_REG)
	{
		/* 
			Symbol ID is a fixed reg 
		*/

		/* It is not a member of a structure */
		uOffsetLeftStart = 0;

		uRegNum = psHWSymbol->u.uFixedRegNum;
		
		uAllocCount = psHWSymbol->uAllocCount;

		uCompStart = 0;
	}
	else
	{
		/* 
			Break down all the offsets until it reaches a base type or an array of base type
		*/

		IMG_UINT32 uCompOffsetFromBase = 0, uTextureUnitOffset = 0, uCompOffset;

		GLSLStructureAlloc *psStructAlloc;

		uAllocCount = psHWSymbol->uAllocCount;

		iArraySize = psHWSymbol->iArraySize;

		for(i = 0; i < psICOperand->uNumOffsets; i++)
		{
			if(iArraySize)
			{
				iArraySize = 0;

				if(psICUFOperand->eRegType == UFREG_TYPE_TEX)
				{
					/* Indexing into an array of samplers */
#if defined(GLSL_ES)
					if(psICOperand->psOffsets[i].uOffsetSymbolID)
					{
						HWSYMBOL *psOffsetHWSymbol = GetHWSymbolEntry(psCPD, psUFContext, psICOperand->psOffsets[i].uOffsetSymbolID, IMG_FALSE);
						IMG_CHAR *pszArrayName = GetSymbolName(psUFContext->psSymbolTable, psHWSymbol->uSymbolId); 
						IMG_CHAR *pszIndexName = GetSymbolName(psUFContext->psSymbolTable, psOffsetHWSymbol->uSymbolId); 

						/* Only constants are allowed to index into arrays of samplers (after loop unrolling) */
						LogProgramError(psCPD->psErrorLog, "%s[%s] : arrays of samplers may only be indexed by a constant index expression\n",
											pszArrayName, pszIndexName);
						
						return IMG_FALSE;
					}
#else
					DebugAssert(!psICOperand->psOffsets[i].uOffsetSymbolID); /* Only constant expressions are allowed for arrays of samplers. */
#endif
					uTextureUnitOffset += uAllocCount * psICOperand->psOffsets[i].uStaticOffset;
				}
				else if(psICOperand->psOffsets[i].uOffsetSymbolID)
				{
					{
						AddUpRelativeOffsetToUFCode(psCPD, psUFContext, 
													uAllocCount,
													psICOperand->psOffsets[i].uOffsetSymbolID,
													&psRelativeIndex);
					}
				}
				else
				{
					uCompOffsetFromBase += uAllocCount * psICOperand->psOffsets[i].uStaticOffset;
				}
			}
			else
			{
				if(GLSL_IS_STRUCT(sFullType.eTypeSpecifier))
				{
					psStructAlloc = GetStructAllocInfo(psCPD, psUFContext, &sFullType);

					/* The current offset selects a member of the struct, so get the member's info */
					sFullType = GetIndexedType(psCPD, psUFContext->psSymbolTable,
											   &sFullType, 
											   psICOperand->psOffsets[i].uStaticOffset,
											   &iArraySize);
					
					/* We always have to add on to uTextureUnitOffset, because this struct member might be another struct with a sampler.
					   If the struct member is a sampler, we know that the end result is a sampler, so we dont need to add onto uCompOffsetFromBase. */
					uTextureUnitOffset += psStructAlloc->psMemberAlloc[psICOperand->psOffsets[i].uStaticOffset].uTextureUnitOffset;

					if(GLSL_IS_SAMPLER(sFullType.eTypeSpecifier))
					{
						/* Overwrite the reg type */
						psICUFOperand->eRegType = UFREG_TYPE_TEX;

						uAllocCount = 1;/* In case we have another offset for an index into an array of samplers. */
					}
					else
					{
						uAllocCount = psStructAlloc->psMemberAlloc[psICOperand->psOffsets[i].uStaticOffset].uAllocCount;

						uCompOffsetFromBase += psStructAlloc->psMemberAlloc[psICOperand->psOffsets[i].uStaticOffset].uCompOffset;
					}
				}
				else
				{
					/* Other offsets refer to components of matrices, vectors etc. */
					break;
				}
			}
		}/* for(i = 0; i < psICOperand->uNumOffsets; i++) */

		uOffsetLeftStart = i;

		if(psICUFOperand->eRegType == UFREG_TYPE_TEX)
		{
			uCompOffset = uTextureUnitOffset + psHWSymbol->uTextureUnit;

			uRegNum = uCompOffset;
			uCompStart = 0;
		}
		else
		{
			uCompOffset = uCompOffsetFromBase + psHWSymbol->u.uCompOffset;

			uRegNum = REG_INDEX(uCompOffset);
			uCompStart = REG_COMPSTART(uCompOffset);
		}

		/* Setup varying present mask and modify the reg number for VS output */
		if(psHWSymbol->eRegType == UFREG_TYPE_VSOUTPUT)
		{
			uRegNum = uRegNum + GLSLVR_TEXCOORD0;
		}
	}

	/* 
		Adjust the location based on any subfields selected with the main variable. 
	*/
	for (i = uOffsetLeftStart; i < psICOperand->uNumOffsets; i++)
	{
		if (iArraySize)
		{
			if(psICOperand->psOffsets[i].uOffsetSymbolID)
			{
				AddUpRelativeOffsetToUFCode(psCPD, psUFContext,
											uAllocCount,
											psICOperand->psOffsets[i].uOffsetSymbolID,
											&psRelativeIndex);

			}
			else
			{
				if(psICUFOperand->eRegType == UFREG_TYPE_TEX)
				{
					uRegNum += psICOperand->psOffsets[i].uStaticOffset;
				}
				else
				{
					uCompStart += uAllocCount * psICOperand->psOffsets[i].uStaticOffset;
				}
			}

			iArraySize = 0;
		}
		else
		{
			if (GLSL_IS_MATRIX(sFullType.eTypeSpecifier))
			{
				IMG_UINT32 uCols = TYPESPECIFIER_NUM_COLS(sFullType.eTypeSpecifier);
				IMG_UINT32 uAllocCountPerColumn = uAllocCount / uCols;

				DebugAssert(uAllocCount % uCols == 0);

				if(psICOperand->psOffsets[i].uOffsetSymbolID)
				{
					AddUpRelativeOffsetToUFCode(psCPD, psUFContext,
												uAllocCountPerColumn,
												psICOperand->psOffsets[i].uOffsetSymbolID,
												&psRelativeIndex);
				}
				else
				{
					uCompStart += psICOperand->psOffsets[i].uStaticOffset * uAllocCountPerColumn;
				}
			}
			else if(GLSL_IS_VECTOR(sFullType.eTypeSpecifier))
			{
				if(psICOperand->psOffsets[i].uOffsetSymbolID)
				{
					AddUpRelativeOffsetToUFCode(psCPD, psUFContext,
												1,
												psICOperand->psOffsets[i].uOffsetSymbolID,
												&psRelativeIndex);
				}
				else
				{
					uCompStart += psICOperand->psOffsets[i].uStaticOffset;
				}
			}

			sFullType = GetIndexedType(psCPD, psUFContext->psSymbolTable,
									   &sFullType,
									   psICOperand->psOffsets[i].uStaticOffset,
									   &iArraySize);
		}
	}

	if(psICUFOperand->eRegType == UFREG_TYPE_TEX)
	{
		psICUFOperand->uRegNum = uRegNum;
		psICUFOperand->uCompStart = uCompStart;
	}
	else
	{
		psICUFOperand->uRegNum = uRegNum + REG_INDEX(uCompStart);
		psICUFOperand->uCompStart = REG_COMPSTART(uCompStart);
	}

	/* Update the Fully Specified type and array size, note the iArraySize in sFullType might not be updated */
	psICUFOperand->sFullType = sFullType;
	psICUFOperand->eRegFormat = ConvertPrecisionToUFRegFormat(psCPD, psUFContext, &sFullType);
	psICUFOperand->iArraySize = iArraySize;

	if(psRelativeIndex)
	{
		/* We should not dynamically addressing to normal temp and samplers */
		DebugAssert(psICUFOperand->eRegType != UFREG_TYPE_TEMP && 
					psICUFOperand->eRegType != UFREG_TYPE_TEX      );

		AddMOVAToUFCode(psUFContext, &psRelativeIndex->sIndexOperand, psICUFOperand->eRelativeIndex);
		
		psICUFOperand->uRelativeStrideInComponents = psRelativeIndex->uIndexUnitsInComponents;

		DebugMemFree(psRelativeIndex);
	}
	else
	{
		psICUFOperand->eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
	}

	DebugAssert(uAllocCount != 0);

	psICUFOperand->uAllocCount = uAllocCount;

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: ProcessICOperand

 PURPOSE	: Build UF operand info from IC operand.

 PARAMETERS	: psUFContext		- UF context
			  psICOp			- IC operand
			  psUFOp			- UF operand
			  eInitialRelative	- If it needs relative index, which index is going to use.

 RETURNS	: IMG_TRUE if successfully.
*****************************************************************************/
static IMG_BOOL ProcessICOperand(GLSLCompilerPrivateData *psCPD,
								 GLSLUniFlexContext		*psUFContext,
								 const GLSLICOperand	*psICOp,
								 ICUFOperand			*psUFOp,
								 UFREG_RELATIVEINDEX	eInitialRelative,
								 IMG_BOOL				bDirectRegister)

{
	HWSYMBOL *psHWSymbol = IMG_NULL;

	DebugAssert(eInitialRelative <= UFREG_RELATIVEINDEX_A0W);

	memset(psUFOp, 0, sizeof(ICUFOperand));

	/* Setup some initial value */
	psUFOp->eICModifier = psICOp->eInstModifier;
	psUFOp->sICSwizMask = psICOp->sSwizWMask;
	psUFOp->uSymbolID	= psICOp->uSymbolID;
	psUFOp->uCompStart = 0;

	psHWSymbol = GetHWSymbolEntry(psCPD, psUFContext, psICOp->uSymbolID, bDirectRegister);

	/* If register has not been allocated */
	if(!psHWSymbol->bRegAllocated)
	{
		AllocateHWSymbolRegisters(psCPD, psUFContext, psHWSymbol);
	}
		
	/* Initially set the fully specified type as if there is no offsets attached */
	psUFOp->sFullType	= psHWSymbol->sFullType;
	psUFOp->eRegType	= psHWSymbol->eRegType;
	psUFOp->uArrayTag	= psHWSymbol->uIndexableTempTag;

	/* Process offsets */
	psUFOp->eRelativeIndex = eInitialRelative;

	if(!ProcessOperandOffsets(psCPD, psUFContext, psHWSymbol, psICOp, psUFOp))
	{
		LOG_INTERNAL_ERROR(("ProcessICOperand: Failed to process operand offsets\n"));
		return IMG_FALSE;
	}
	
	/* Update the swiz */
	UpdateOperandUFSwiz(psCPD, psUFOp);

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: AddSimpleALUToUFCode

 PURPOSE	: Generate UniFlex code to implement a componentwise ALU operation.

 PARAMETERS	: psDest			- Destination operand.
			  psSrcA			- First source operand.
			  psSrcB			- Second source operand.
			  psSrcC			- Second source operand.
			  eUFOpcode			- UniFlex opcode to use.
			  uNumSrcArgs		- The number of the arguments to the ALU operation.

 RETURNS	: IMG_TRUE if successful
*****************************************************************************/
static IMG_BOOL AddSimpleALUToUFCode(GLSLCompilerPrivateData *psCPD,
									 GLSLUniFlexContext	*psUFContext,
									 ICUFOperand		*psDest,
									 ICUFOperand		*psSrcA,
									 ICUFOperand		*psSrcB,
									 ICUFOperand		*psSrcC,
									 UF_OPCODE			eUFOpcode,
									 IMG_UINT32			uNumSrcs)

{
	GLSLTypeSpecifier eDestType = psDest->eTypeAfterSwiz;

	if(GLSL_IS_MATRIX(eDestType))
	{
		IMG_UINT32 i;
		IMG_UINT32 uMatColumns;
		ICUFOperand	sTDest, sTSrcA, sTSrcB, sTSrcC;

		uMatColumns = TYPESPECIFIER_NUM_COLS(psDest->eTypeAfterSwiz);

		sTDest = *psDest;
		if(uNumSrcs > 0) sTSrcA = *psSrcA;
		if(uNumSrcs > 1) sTSrcB = *psSrcB;
		if(uNumSrcs > 2) sTSrcC = *psSrcC;

		for (i = 0; i < uMatColumns; i++)
		{
			ChooseMatrixColumn(psCPD, psDest, &sTDest, i);
			if(uNumSrcs > 0) ChooseMatrixColumn(psCPD, psSrcA, &sTSrcA, i);
			if(uNumSrcs > 1) ChooseMatrixColumn(psCPD, psSrcB, &sTSrcB, i);
			if(uNumSrcs > 2) ChooseMatrixColumn(psCPD, psSrcC, &sTSrcC, i);

			AddAluToUFCode(psUFContext, &sTDest, &sTSrcA, &sTSrcB, &sTSrcC, eUFOpcode, uNumSrcs, IMG_TRUE);
		}
	}
	else
	{
		/* Destination is a vector or a scalar */
		AddAluToUFCode(psUFContext, psDest, psSrcA, psSrcB, psSrcC, eUFOpcode, uNumSrcs, IMG_TRUE);
	}

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: AddSINCOSInstToUFCode

 PURPOSE	: Generates instructions to do a non-vector (one result per instruction) alu operation.

 PARAMETERS	: psUFContext	- UF context
			  psDest		- Destination.
			  psSrc			- Source.
			  bCos			- TRUE if cosine, FALSE if sine

 RETURNS	: IMG_TRUE if successful
*****************************************************************************/
static IMG_BOOL AddSINCOSInstToUFCode(GLSLCompilerPrivateData *psCPD,
									  GLSLUniFlexContext	*psUFContext,
									  ICUFOperand			*psDest,
									  ICUFOperand			*psSrc, 
									  IMG_BOOL				bCos)
{
	GLSLTypeSpecifier eDestType = psDest->eTypeAfterSwiz;
	IMG_UINT32 uTypeComponents = TYPESPECIFIER_NUM_COMPONENTS(eDestType);
	IMG_UINT32 x;
	ICUFOperand sTemp;
	ICUFOperand sTDest, sTSrc;

	sTDest = *psDest;
	sTSrc = *psSrc;

	/*
		The result of sincos produced has to be a specific channel, we need a temporary destination
	*/
	GetTemporary(psCPD, psUFContext, GLSLTS_VEC4, psDest->sFullType.ePrecisionQualifier, &sTemp);
	ApplyMoreSwiz(psCPD, &sTemp, bCos ? SWIZ_X : SWIZ_Y);

	for (x = 0; x < uTypeComponents; x++)
	{
		ChooseVectorComponent(psCPD, psDest, &sTDest, (GLSLICVecComponent)x);
		ChooseVectorComponent(psCPD, psSrc, &sTSrc, (GLSLICVecComponent)x);

		AddAluToUFCode(psUFContext, &sTemp, &sTSrc, NULL, NULL, UFOP_SINCOS, 1, IMG_TRUE);
		AddAluToUFCode(psUFContext, &sTDest, &sTemp, NULL, NULL, UFOP_MOV, 1, IMG_TRUE);
	}

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: AddComparisonToUFCode

 PURPOSE	: Generates instructions to set a boolean variable to true or false depending on the results of a comparison.
				Supports bvec2, bvec3 & bvec. Output may be a single boolean

 PARAMETERS	: psDest		- Destination.
			  psSrcA		- First comparison source.
			  psSrcB		- Second comparison source.
			  uCompOp		- Type of comparison to do.

 RETURNS	: IMG_TRUE if successful
*****************************************************************************/
static IMG_BOOL AddComparisonToUFCode(GLSLCompilerPrivateData *psCPD,
									  GLSLUniFlexContext	*psUFContext,
									  ICUFOperand			*psDest, 
									  ICUFOperand			*psSrcA, 
									  ICUFOperand			*psSrcB,
									  IMG_UINT32			uCompOp,
									  ICUFOperand			*psPredicate)

{
	IMG_UINT32 uDestMask, uDestSwiz;
	GLSLTypeSpecifier eDestType = psDest->eTypeAfterSwiz;
	UNIFLEX_INST *psProg;

	PVR_UNREFERENCED_PARAMETER(psCPD);

	DebugAssert(GLSL_IS_VECTOR(psSrcA->eTypeAfterSwiz) || GLSL_IS_SCALAR(psSrcA->eTypeAfterSwiz));

	/* if Dest is a single predicate */
	if((psDest->eTypeAfterSwiz == GLSLTS_BOOL) && (psDest->eRegType == UFREG_TYPE_PREDICATE))
	{
		IMG_BOOL bComponentWise = (uCompOp & UFREG_COMPCHANOP_MASK) ? IMG_FALSE : IMG_TRUE;

#if defined(DEBUG)
		if(uCompOp & UFREG_COMPCHANOP_MASK)
		{
			/* Currently ANDALL and ORALL are supported for vec4 */
			DebugAssert(psSrcA->eTypeAfterSwiz == GLSLTS_VEC4);

			if(psSrcA->sFullType.ePrecisionQualifier != GLSLPRECQ_LOW)
			{
				/* Predicate is supported for low precision and no swiz only */
				DebugAssert(psPredicate == IMG_NULL);
			}
		}
#endif
		/* p SETP psDest SrcA CompOp SrcB */
		psProg = CreateInstruction(psUFContext, UFOP_SETP);		if(!psProg) return IMG_FALSE;

		/* The instruction is predicated by psPredicate */
		psProg->uPredicate = ConvertToUFPredicate(psPredicate); 
		
		ConvertToUFDestination(&psProg->sDest, psDest, &uDestSwiz);

		ConvertToUFSource(&psProg->asSrc[0], psSrcA, bComponentWise, psDest->uCompStart, uDestSwiz);

		InitUFSource(&psProg->asSrc[1], UFREG_TYPE_COMPOP, uCompOp, UFREG_SWIZ_NONE);

		ConvertToUFSource(&psProg->asSrc[2], psSrcB, bComponentWise, psDest->uCompStart, uDestSwiz);	
	}
	else
	{
		/* Dest is an ordinary bool temp, this instruction is not predicated */
		DebugAssert(psPredicate == IMG_NULL);
#if 0
		if (uCompOp == UFREG_COMPOP_LT || uCompOp == UFREG_COMPOP_GE)
		{
			AddAluToUFCode(psUFContext, psDest, psSrcA, psSrcB, NULL, (uCompOp == UFREG_COMPOP_LT ? UFOP_SLT : UFOP_SGE), 2, IMG_TRUE);
		}
		else
#endif
		{
			/* SETP p0 SrcA CompOp SrcB */
			uDestMask = IC2UF_GetDestMask(psDest->uCompStart, eDestType, &psDest->sICSwizMask, &uDestSwiz);

			psProg = CreateInstruction(psUFContext, UFOP_SETP);		if(!psProg) return IMG_FALSE;

			InitUFDestination(&psProg->sDest, UFREG_TYPE_PREDICATE, 0, uDestMask);

			ConvertToUFSource(&psProg->asSrc[0], psSrcA, IMG_TRUE, psDest->uCompStart, uDestSwiz);

			InitUFSource(&psProg->asSrc[1], UFREG_TYPE_COMPOP, uCompOp, UFREG_SWIZ_NONE);

			ConvertToUFSource(&psProg->asSrc[2], psSrcB, IMG_TRUE, psDest->uCompStart, uDestSwiz);

			/* !p MOV result h0 */
			psProg = CreateInstruction(psUFContext, UFOP_MOV);		if(!psProg) return IMG_FALSE;
			ConvertToUFDestination(&psProg->sDest, psDest, IMG_NULL);
			psProg->asSrc[0] = g_sSourceZero;
			psProg->asSrc[0].eFormat = psProg->sDest.eFormat;
			psProg->uPredicate = UF_PRED_XYZW | UF_PRED_NEGFLAG;

			/* p MOV result h1 */
			psProg = CreateInstruction(psUFContext, UFOP_MOV);		if(!psProg) return IMG_FALSE;
			ConvertToUFDestination(&psProg->sDest, psDest, NULL);
			psProg->uPredicate  = UF_PRED_XYZW;
			AssignSourceONE(&psProg->asSrc[0], psProg->sDest.eFormat);
		}

	}

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: AddIFComparisonToUFCode

 PURPOSE	: Generates uniflex code for condition check. Supported IC opcodes are
			  IFGT, IFGE, IFLT, IFLE, IFEQ, IFNE. Note two operands must be scalars.

 PARAMETERS	: psSrcA		- First comparison source.
			  psSrcB		- Second comparison source.
			  uCompOp		- Type of comparison to do.

 RETURNS	: IMG_TRUE if successful
*****************************************************************************/
static IMG_BOOL AddIFComparisonToUFCode(GLSLCompilerPrivateData *psCPD,
										GLSLUniFlexContext	*psUFContext,
									    ICUFOperand			*psSrcA,
									    ICUFOperand			*psSrcB,
									    IMG_UINT32			uCompOp)
{
	UNIFLEX_INST *psProg;

	PVR_UNREFERENCED_PARAMETER(psCPD);

	/* Only hanle scalar for those instruction set */
	DebugAssert(GLSL_IS_SCALAR(psSrcA->eTypeAfterSwiz));
	DebugAssert(GLSL_IS_SCALAR(psSrcB->eTypeAfterSwiz));

	psProg = CreateInstruction(psUFContext, UFOP_IFC);		if(!psProg) return IMG_FALSE;

	ConvertToUFSource(&psProg->asSrc[0], psSrcA, IMG_FALSE, 0, 0);

	InitUFSource(&psProg->asSrc[1], UFREG_TYPE_COMPOP, uCompOp, UFREG_SWIZ_NONE);

	ConvertToUFSource(&psProg->asSrc[2], psSrcB, IMG_FALSE, 0, 0);

	psUFContext->uIfNestCount++;

	return IMG_TRUE;
}
/*****************************************************************************
 FUNCTION	: AddVectorSALLToUFCode

 PURPOSE	: Generate uniflex code to set dest to be true if all components of src 
			  (scalar or vector) is true

 PARAMETERS	: psUFContext			- UF Context.
			  psDest				- Boolean scalar destination.
			  psSrc					- Boolean scalar or vector source

 RETURNS	: IMG_TRUE if successful
*****************************************************************************/
static IMG_BOOL AddVectorSALLToUFCode(GLSLCompilerPrivateData *psCPD,
									  GLSLUniFlexContext	*psUFContext,
									  ICUFOperand			*psDest,
									  ICUFOperand			*psSrc)
{
	IMG_UINT32 uSrcComponents = TYPESPECIFIER_NUM_COMPONENTS(psSrc->eTypeAfterSwiz);

#ifdef GLSL_NATIVE_INTEGERS
	IMG_UINT32 i;
	ICUFOperand sSrc = *psSrc;

	ChooseVectorComponent(psCPD, psSrc, &sSrc, GLSLIC_VECCOMP_X);
	AddAluToUFCode(psUFContext, psDest, &sSrc, IMG_NULL, IMG_NULL, UFOP_MOV, 1, IMG_TRUE);

	for(i = 1; i < uSrcComponents; i++)
	{
		ChooseVectorComponent(psCPD, psSrc, &sSrc, GLSLIC_VECCOMP_X + i);
		AddAluToUFCode(psUFContext, psDest, psDest, &sSrc, IMG_NULL, UFOP_AND, 2, IMG_TRUE);
	}
#else
	if(uSrcComponents == 1)
	{
		/* Just move if source is a scalar */
		AddAluToUFCode(psUFContext, psDest, psSrc, IMG_NULL, IMG_NULL, UFOP_MOV, 2, IMG_TRUE);
	}
	else
	{
		ICUFOperand sTSrc1, sTSrc2;

		sTSrc1 = *psSrc;
		sTSrc2 = *psSrc;

		if(uSrcComponents > 1)
		{
			ChooseVectorComponent(psCPD, psSrc, &sTSrc1, GLSLIC_VECCOMP_X);
			ChooseVectorComponent(psCPD, psSrc, &sTSrc2, GLSLIC_VECCOMP_Y);
			AddAluToUFCode(psUFContext, psDest, &sTSrc1, &sTSrc2, IMG_NULL, UFOP_MUL, 2, IMG_TRUE);
		}
		if(uSrcComponents > 2)
		{
			ChooseVectorComponent(psCPD, psSrc, &sTSrc1, GLSLIC_VECCOMP_Z);
			AddAluToUFCode(psUFContext, psDest, psDest, &sTSrc1, IMG_NULL, UFOP_MUL, 2, IMG_TRUE);
		}
		if(uSrcComponents > 3)
		{
			ChooseVectorComponent(psCPD, psSrc, &sTSrc1, GLSLIC_VECCOMP_W);
			AddAluToUFCode(psUFContext, psDest, psDest, &sTSrc1, IMG_NULL, UFOP_MUL, 2, IMG_TRUE);
		}
	}
#endif

	return IMG_TRUE;
}

static IMG_BOOL AddDFDYToUFCode(GLSLCompilerPrivateData *psCPD,
								GLSLUniFlexContext	*psUFContext,
								ICUFOperand			*psDest,
								ICUFOperand			*psSrc)
{
	IMG_UINT32 uPMXInvertdFdYID;
	ICUFOperand sPMXInvertdFdYOperand, sSource;

	if(!AddBuiltInIdentifier(psCPD, psUFContext->psSymbolTable,
							"gl_PMXInvertdFdY",
							0,
							GLSLBV_PMXINVERTDFDY,
							GLSLTS_FLOAT,
							GLSLTQ_UNIFORM,
							psSrc->sFullType.ePrecisionQualifier,
							&uPMXInvertdFdYID))
	{
		LOG_INTERNAL_ERROR(("GenerateFragCoordAdjust: Failed to add builtin variable gl_PMXInvertdFdY to the symbol table !\n"));
		return IMG_FALSE;
	}
	InitICUFOperand(psCPD, psUFContext, uPMXInvertdFdYID, &sPMXInvertdFdYOperand, IMG_FALSE);

	/* Move the source to a temp */
	GetTemporary(psCPD, psUFContext, psSrc->eTypeAfterSwiz, psSrc->sFullType.ePrecisionQualifier, &sSource);
	AddAluToUFCode(psUFContext, &sSource, psSrc, IMG_NULL, IMG_NULL, UFOP_MOV, 1, IMG_TRUE);

	/* Multiply by -1 or 1 depending on the render target Y direction. Default should be -1. */
	AddSimpleALUToUFCode(psCPD, psUFContext, &sSource, &sSource, &sPMXInvertdFdYOperand, NULL, UFOP_MUL, 2);

	AddSimpleALUToUFCode(psCPD, psUFContext, psDest, &sSource, NULL, NULL, UFOP_DSY, 1);
	
	return IMG_TRUE;
}


#if (!USE_PREDICATED_COMPARISON)

/*****************************************************************************
 FUNCTION	: AddSALLToUFCode

 PURPOSE	: Generate uniflex code to set dest to be true if all components of src is true

 PARAMETERS	: psUFContext			- UF Context.
			  psDest				- Boolean scalar destination.
			  psSrc					- Boolean scalar, vector or matrix source

 RETURNS	: IMG_TRUE if successful
*****************************************************************************/
static IMG_BOOL AddSALLToUFCode(GLSLCompilerPrivateData *psCPD,
								GLSLUniFlexContext	*psUFContext,
								ICUFOperand			*psDest,
								ICUFOperand			*psSrc)

{
	GLSLTypeSpecifier eSrcType = psSrc->eTypeAfterSwiz;
	IMG_UINT32 i;

	DebugAssert(GLSL_IS_SCALAR(psDest->eTypeAfterSwiz));

	if(GLSL_IS_MATRIX(eSrcType))
	{
		IMG_UINT32 uSrcColumns = TYPESPECIFIER_NUM_COLS(eSrcType);

		ICUFOperand sVecTemp0, sScalarTemp, sTSrc;
		GLSLTypeSpecifier sTempType = GLSLTS_BOOL + uSrcColumns - 1;

		GetTemporary(psCPD, psUFContext, sTempType, psDest->sFullType.ePrecisionQualifier, &sVecTemp0);
		sTSrc = *psSrc;
		sScalarTemp = sVecTemp0;

		for(i = 0; i < uSrcColumns; i++)
		{
			ChooseMatrixColumn(psCPD, psSrc, &sTSrc, i);
			ChooseVectorComponent(psCPD, &sVecTemp0, &sScalarTemp, (GLSLICVecComponent)i);

			AddVectorSALLToUFCode(psCPD, psUFContext, &sScalarTemp, &sTSrc);
		}

		AddVectorSALLToUFCode(psCPD, psUFContext, psDest, &sVecTemp0);
	}
	else
	{
		AddVectorSALLToUFCode(psCPD, psUFContext, psDest, psSrc);
	}

	return IMG_TRUE;
}

#endif
/*****************************************************************************
 FUNCTION	: AddVectorSANYToUFCode

 PURPOSE	: Generate uniflex code to set dest to be true if any component of src(scalar or vector) is true

 PARAMETERS	: psUFContext			- UF Context.
			  psDest				- Boolean scalar destination.
			  psSrc					- Boolean scalar or vector source

 RETURNS	: IMG_TRUE if successful
*****************************************************************************/
static IMG_BOOL AddVectorSANYToUFCode(GLSLCompilerPrivateData *psCPD,
									  GLSLUniFlexContext	*psUFContext,
									  ICUFOperand			*psDest,
									  ICUFOperand			*psSrc)
{
	IMG_UINT32 uSrcComponents = TYPESPECIFIER_NUM_COMPONENTS(psSrc->eTypeAfterSwiz);

#ifdef GLSL_NATIVE_INTEGERS
	IMG_UINT32 i;
	ICUFOperand sSrc = *psSrc;

	ChooseVectorComponent(psCPD, psSrc, &sSrc, GLSLIC_VECCOMP_X);
	AddAluToUFCode(psUFContext, psDest, &sSrc, IMG_NULL, IMG_NULL, UFOP_MOV, 1, IMG_TRUE);

	for(i = 1; i < uSrcComponents; i++)
	{
		ChooseVectorComponent(psCPD, psSrc, &sSrc, GLSLIC_VECCOMP_X + i);
		AddAluToUFCode(psUFContext, psDest, psDest, &sSrc, IMG_NULL, UFOP_OR, 2, IMG_TRUE);
	}
#else
	PUNIFLEX_INST psUFInst;

	if(uSrcComponents == 1)
	{
		AddAluToUFCode(psUFContext, psDest, psSrc, IMG_NULL, IMG_NULL, UFOP_MOV, 2, IMG_TRUE);
	}
	else
	{

		UF_OPCODE eUFOpcode = UFOP_NOP;

		/* Work out dot op code */
		switch (uSrcComponents)
		{
			case 2:
				eUFOpcode = UFOP_DOT2ADD; 
				break;
			case 3:
				eUFOpcode = UFOP_DOT3; 
				break;
			case 4:
				eUFOpcode = UFOP_DOT4; 
				break;
			default: 
				LOG_INTERNAL_ERROR(("ProcessICInstSANY: What is the num of components \n"));
				break;
		}
		
		/* Add dot instruction */
		psUFInst = AddAluToUFCode(psUFContext, psDest, psSrc, psSrc, NULL, eUFOpcode, 2, IMG_FALSE);

		if(uSrcComponents == 2)
		{
			psUFInst->asSrc[2].eFormat = psUFInst->asSrc[0].eFormat;
		}

		/* 
			The result of DOT will be 0.0, or 1.0, or 2.0, or 3.0 and we want the result to be 0 
			or 1.0. Clamp the result to 0.0 and 1.0 should get correct result.  
		*/
		psUFContext->psEndUFInst->sDest.byMod |= UFREG_DMOD_SATZEROONE;

	}
#endif

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: AddNOTToUFCode

 PURPOSE	: Generate uniflex code to perform NOT (from 1 to 0 or from 0 to 1)

 PARAMETERS	: psUFContext			- UF Context.

 RETURNS	: IMG_TRUE if successful
*****************************************************************************/
static IMG_BOOL AddNOTToUFCode(GLSLUniFlexContext	*psUFContext,
							   ICUFOperand			*psDest,
							   ICUFOperand			*psSrc)
{
	ICUFOperand sTemp = *psSrc;
	PUNIFLEX_INST psUFInst;

	/* ADD r 1.0 -r */
	if(sTemp.eICModifier & GLSLIC_MODIFIER_NEGATE)
	{
		sTemp.eICModifier &= ~GLSLIC_MODIFIER_NEGATE;
	}
	else
	{
		sTemp.eICModifier |= GLSLIC_MODIFIER_NEGATE;
	}

	psUFInst = AddAluToUFCode(psUFContext, psDest, NULL, &sTemp, NULL, UFOP_ADD, 2, IMG_TRUE);		if(!psUFInst) return IMG_FALSE;

	AssignSourceONE(&psUFInst->asSrc[0], psUFInst->sDest.eFormat);

	return IMG_TRUE;
}



/*****************************************************************************
 FUNCTION	: ProcessICInstIF

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_IF

 PARAMETERS	: psUFContext		- UF context
			  psSrc				- First source operand.

 RETURNS	: true if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstIF(GLSLUniFlexContext	*psUFContext,
								ICUFOperand			*psSrc)
{
	if(psSrc->eRegType == UFREG_TYPE_PREDICATE)
	{
		UNIFLEX_INST *psProg;

		psProg = CreateInstruction(psUFContext, UFOP_IFP);		if(!psProg) return IMG_FALSE;

		ConvertToUFSource(&psProg->asSrc[0], psSrc, IMG_FALSE, 0, 0);
	}
	else
	{
		AddIFCToUFCode(psUFContext, psSrc, UFREG_COMPOP_NE);
	}
	
	psUFContext->uIfNestCount++;

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: ProcessICInstIFNOT

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_IF

 PARAMETERS	: psUFContext		- UF context
			  psSrc				- First source operand.

 RETURNS	: true if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstIFNOT(GLSLUniFlexContext	*psUFContext,
								   ICUFOperand			*psSrc)
{
	if(psSrc->eRegType == UFREG_TYPE_PREDICATE)
	{
		UNIFLEX_INST *psProg;

		ICUFOperand	sTSrc = *psSrc;

		psProg = CreateInstruction(psUFContext, UFOP_IFP);		if(!psProg) return IMG_FALSE;

		if(sTSrc.eICModifier & GLSLIC_MODIFIER_NEGATE)
		{
			sTSrc.eICModifier &= ~GLSLIC_MODIFIER_NEGATE;
		}
		else
		{
			sTSrc.eICModifier |= GLSLIC_MODIFIER_NEGATE;
		}

		ConvertToUFSource(&psProg->asSrc[0], &sTSrc, IMG_FALSE, 0, 0);

	}
	else
	{			
		AddIFCToUFCode(psUFContext, psSrc, UFREG_COMPOP_EQ);

	}
	psUFContext->uIfNestCount++;

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: ProcessICInstENDIF

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_ENDIF

 PARAMETERS	: psUFContext		- UF context

 RETURNS	: true if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstENDIF(GLSLUniFlexContext *psUFContext)
{
	CreateInstruction(psUFContext, UFOP_ENDIF);

	psUFContext->uIfNestCount--;

	return IMG_TRUE;
}


/*****************************************************************************
 FUNCTION	: ProcessICInstLOOP

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_LOOP

 PARAMETERS	: psUFContext		- UF context
			  psSrc				- Source operand.

 RETURNS	: True if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstLOOP(GLSLCompilerPrivateData *psCPD,
								  GLSLUniFlexContext *psUFContext, ICUFOperand *psPredicate)
{
	UNIFLEX_INST *psProg;

	if (psUFContext->uLoopNestCount >= MAX_LOOP_NESTING)
	{
		LogProgramError(psCPD->psErrorLog, "Too deeply nested loops.\n");
		return IMG_FALSE;
	}

	psProg = CreateInstruction(psUFContext, UFOP_GLSLLOOP);		if(!psProg) return IMG_FALSE;
	psProg->uPredicate = ConvertToUFPredicate(psPredicate);

	/* Increase the loop nest count */
	psUFContext->uLoopNestCount++;

	return IMG_TRUE;
}


/*****************************************************************************
 FUNCTION	: ProcessICInstSTATICLOOP

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_STATICLOOP

 PARAMETERS	: psUFContext		- UF context
			  psSrc				- Source operand.

 RETURNS	: True if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstSTATICLOOP(GLSLCompilerPrivateData *psCPD,
										GLSLUniFlexContext *psUFContext, ICUFOperand *psPredicate)
{
	UNIFLEX_INST *psProg;

	if (psUFContext->uLoopNestCount >= MAX_LOOP_NESTING)
	{
		LogProgramError(psCPD->psErrorLog, "Too deeply nested loops.\n");
		return IMG_FALSE;
	}

	psProg = CreateInstruction(psUFContext, UFOP_GLSLSTATICLOOP);		if(!psProg) return IMG_FALSE;
	psProg->uPredicate = ConvertToUFPredicate(psPredicate);

	/* Increase the loop nest count */
	psUFContext->uLoopNestCount++;

	return IMG_TRUE;
}
/*****************************************************************************
 FUNCTION	: ProcessICInstENDLOOP

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_ENDLOOP

 PARAMETERS	: psUFContext		- UF context

 RETURNS	: True if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstENDLOOP(GLSLUniFlexContext *psUFContext, ICUFOperand *psPredicate)
{
	UNIFLEX_INST *psProg;

	/* p0 GLSLENDLOOP */
	psProg = CreateInstruction(psUFContext, UFOP_GLSLENDLOOP);		if(!psProg) return IMG_FALSE;
	psProg->uPredicate = ConvertToUFPredicate(psPredicate);

	psUFContext->uLoopNestCount--;

	return IMG_TRUE;

}


#if (!USE_PREDICATED_COMPARISON)

/*****************************************************************************
 FUNCTION	: AddSlowWholeTypeComparison

 PURPOSE	: Generate UniFlex code to compare two sources, the comparison result is a bool.
			  Sources are struct, matrix or vector, the type must be the same

 PARAMETERS	: psDest				- Variable where the result of the comparison will be put.
			  psSrcA				- First variable to compare.
			  psSrcB				- Second variable to compare.
			  pbMultiplyDest			- whether result is multiplied by the original dest result.

 RETURNS	: IMG_TRUE if successful
*****************************************************************************/
static IMG_BOOL  AddSlowWholeTypeComparison(GLSLCompilerPrivateData *psCPD,
											GLSLUniFlexContext	*psUFContext,
											   ICUFOperand			*psDest,
											   ICUFOperand			*psSrcA,
											   ICUFOperand			*psSrcB,
											   IMG_BOOL				*pbMultiplyDest)

{
	IMG_UINT32 i;

	DebugAssert(psSrcA->eTypeAfterSwiz == psSrcB->eTypeAfterSwiz);
	DebugAssert(psSrcA->iArraySize == psSrcB->iArraySize);

	/* Compare two arrays or two user-defined structures */
	if(psSrcA->iArraySize)
	{
		ICUFOperand sTSrcA = *psSrcA, sTSrcB = *psSrcB;

		DebugAssert(psSrcA->iArraySize == psSrcB->iArraySize);

		for(i = 0; i < (IMG_UINT32)psSrcA->iArraySize; i++)
		{
			ChooseArrayElement(psSrcA, &sTSrcA, i);
			ChooseArrayElement(psSrcB, &sTSrcB, i);

			AddSlowWholeTypeComparison(psUFContext, psDest, &sTSrcA, &sTSrcB, pbMultiplyDest);
		}
	}
	else if(GLSL_IS_STRUCT(psSrcA->sFullType.eTypeSpecifier))
	{
		ICUFOperand sTSrcA = *psSrcA, sTSrcB = *psSrcB;

		GLSLStructureAlloc *psStructAlloc;

		psStructAlloc = GetStructAllocInfo(psUFContext, &psSrcA->sFullType);

		for(i = 0; i < psStructAlloc->uNumMembers; i++)
		{
			ChooseStructMember(psCPD, psUFContext, psSrcA, psStructAlloc, &sTSrcA, i);
			ChooseStructMember(psCPD, psUFContext, psSrcB, psStructAlloc, &sTSrcB, i);

			AddSlowWholeTypeComparison(psUFContext, psDest, &sTSrcA, &sTSrcB, pbMultiplyDest);
		}
	}		
	else if(GLSL_IS_MATRIX(psSrcA->eTypeAfterSwiz))
	{
		ICUFOperand sTSrcA = *psSrcA, sTSrcB = *psSrcB;

		for(i = 0; i < TYPESPECIFIER_NUM_COLS(psSrcA->eTypeAfterSwiz); i++)
		{
			ChooseMatrixColumn(psCPD, psSrcA, &sTSrcA, i);
			ChooseMatrixColumn(psCPD, psSrcB, &sTSrcB, i);

			AddSlowWholeTypeComparison(psUFContext, psDest, &sTSrcA, &sTSrcB, pbMultiplyDest);
		}
	}
	else
	{
		/* Two vector's or scalar's comparison */
		ICUFOperand sBoolVecTemp, sBoolScalarTemp, *psTDest = psDest;
		GLSLTypeSpecifier eBoolVecType = GLSLTS_BOOL + TYPESPECIFIER_NUM_COMPONENTS(psSrcA->eTypeAfterSwiz) - 1;

		GetTemporary(psCPD, psUFContext, eBoolVecType, GLSLPRECQ_UNKNOWN, &sBoolVecTemp);

		if(*pbMultiplyDest)
		{
			GetTemporary(psCPD, psUFContext, GLSLTS_BOOL, GLSLPRECQ_UNKNOWN, &sBoolScalarTemp);
			psTDest = &sBoolScalarTemp;
		}

		/* Generate a component-wise comparison into a temporary register. */
		AddComparisonToUFCode(psUFContext, &sBoolVecTemp, psSrcA, psSrcB, UFREG_COMPOP_EQ, IMG_NULL);

		/* Covert it to a bool result */
		AddSALLToUFCode(psUFContext, psTDest, &sBoolVecTemp);

		if(*pbMultiplyDest)
		{
			/* Multiply by the original result */
			AddAluToUFCode(psUFContext, psDest, psDest, psTDest, IMG_NULL, UFOP_MUL, 2, IMG_TRUE);
		}
		else
		{
			/* 
				If it is not the first comparison, the original result is multiplied by 
				the following comparision result, otherwise, it is overwritten. 
			*/
			*pbMultiplyDest = IMG_TRUE;
		}

	}

	return IMG_TRUE;
}


#else /* USE_PREDICATED_COMPARISON */


static IMG_VOID AddPredicatedWholeTypeComparision(GLSLCompilerPrivateData *psCPD,
												  GLSLUniFlexContext	*psUFContext,
												   ICUFOperand			*psDest,
												   ICUFOperand			*psSrcA,
												   ICUFOperand			*psSrcB,
												   IMG_BOOL				bEquality,
												   ICUFOperand			**ppsPredicate,
												   IMG_UINT32			*puNumENDIFsRequired);

/*****************************************************************************
 FUNCTION	: AddArrayComparison

 PURPOSE	: Generate UniFlex code to compare two arrays, the comparison result is a bool.

 PARAMETERS	: psDest				- Variable where the result of the comparison will be put.
			  psSrcA				- First variable to compare.
			  psSrcB				- Second variable to compare.
			  uCompOp				- Type of comparison.
			  pbMultiplyDest			- whether result is multiplied by the original dest result.

 RETURNS	: IMG_TRUE if successful
*****************************************************************************/
static IMG_BOOL  AddArrayComparison(GLSLCompilerPrivateData *psCPD,
									GLSLUniFlexContext	*psUFContext,
									ICUFOperand		*psDest,
									ICUFOperand		*psSrcA,
									ICUFOperand		*psSrcB,
									IMG_BOOL		bEquality,
									ICUFOperand		**ppsPredicate)
{
	ICUFOperand sTSrcA = *psSrcA, sTSrcB = *psSrcB;

	ICUFOperand sLoopCount, sIterator, sZero, sOne, sLoopEndPredicate;

	ICUFOperand sSrcAOffset, sSrcBOffset, sSrcAOffset0, sSrcBOffset0;

	IMG_UINT32 uEndifs = 0;

	DebugAssert(psSrcA->iArraySize == psSrcB->iArraySize);

	if(!GetIntConst(psCPD, psUFContext, 0, GLSLPRECQ_HIGH, &sZero))
	{
		LOG_INTERNAL_ERROR(("AddArrayComparison: Failed to get an integer"));
		return IMG_FALSE;
	}

	if(!GetIntConst(psCPD, psUFContext, 1, GLSLPRECQ_HIGH, &sOne))
	{
		LOG_INTERNAL_ERROR(("AddArrayComparison: Failed to get an integer"));
		return IMG_FALSE;
	}

	if(!GetIntConst(psCPD, psUFContext, psSrcA->iArraySize - 1, GLSLPRECQ_HIGH, &sLoopCount))
	{
		LOG_INTERNAL_ERROR(("AddArrayComparison: Failed to get an integer"));
		return IMG_FALSE;
	}

	if(!GetTemporary(psCPD, psUFContext, GLSLTS_INT, GLSLPRECQ_HIGH, &sIterator))
	{
		LOG_INTERNAL_ERROR(("AddArrayComparison: Failed to get an integer"));
		return IMG_FALSE;
	}	

	if(!IC2UF_GetPredicate(psCPD, psUFContext, &sLoopEndPredicate))
	{
		LOG_INTERNAL_ERROR(("AddArrayComparison: Failed to get a predicate"));
		return IMG_FALSE;	
	}

	if(!GetTemporary(psCPD, psUFContext, GLSLTS_INT, GLSLPRECQ_HIGH, &sSrcAOffset))
	{
		LOG_INTERNAL_ERROR(("AddArrayComparison: Failed to get temporary"));
		return IMG_FALSE;
	}

	if(!GetTemporary(psCPD, psUFContext, GLSLTS_INT, GLSLPRECQ_HIGH, &sSrcBOffset))
	{
		LOG_INTERNAL_ERROR(("AddArrayComparison: Failed to get temporary"));
		return IMG_FALSE;
	}
	
	if(!GetIntConst(psCPD, psUFContext, psSrcA->uRegNum, GLSLPRECQ_HIGH, &sSrcAOffset0))
	{
		LOG_INTERNAL_ERROR(("AddArrayComparison: Failed to get temporary"));
		return IMG_FALSE;
	}

	if(!GetIntConst(psCPD, psUFContext, psSrcB->uRegNum, GLSLPRECQ_HIGH, &sSrcBOffset0))
	{
		LOG_INTERNAL_ERROR(("AddArrayComparison: Failed to get temporary"));
		return IMG_FALSE;
	}

	ChooseArrayElement(psCPD, psSrcA, &sTSrcA, 0);
	ChooseArrayElement(psCPD, psSrcB, &sTSrcB, 0);
	sTSrcA.eRelativeIndex = UFREG_RELATIVEINDEX_A0X;
	sTSrcA.uRelativeStrideInComponents = psSrcB->uAllocCount;
	sTSrcB.eRelativeIndex = UFREG_RELATIVEINDEX_A0Y;
	sTSrcB.uRelativeStrideInComponents = psSrcB->uAllocCount;

	/* i = MaxIterations - 1 */
	AddSimpleALUToUFCode(psCPD, psUFContext, &sIterator, &sLoopCount, IMG_NULL, IMG_NULL, UFOP_MOV, 1);

	/* LOOP */
	ProcessICInstLOOP(psCPD, psUFContext, IMG_NULL);
	
	/* < body > */

	/* Update offsets */
	AddSimpleALUToUFCode(psCPD, psUFContext, &sSrcAOffset, &sIterator, &sSrcAOffset0, NULL /* psSrcC */, UFOP_ADD, 3);
	AddSimpleALUToUFCode(psCPD, psUFContext, &sSrcBOffset, &sIterator, &sSrcBOffset0, NULL /* psSrcC */, UFOP_ADD, 3);

	/* Mov to index registers */
	AddMOVAToUFCode(psUFContext, &sSrcAOffset, UFREG_RELATIVEINDEX_A0X);
	AddMOVAToUFCode(psUFContext, &sSrcBOffset, UFREG_RELATIVEINDEX_A0Y);

	AddPredicatedWholeTypeComparision(psCPD, psUFContext, psDest, &sTSrcA, &sTSrcB, bEquality, ppsPredicate, &uEndifs);

#if USE_IFBLOCKS_FOR_COMPARISON
	{
		IMG_UINT32 i;
		for(i = 0; i < uEndifs; i++)
		{
			ProcessICInstENDIF(psUFContext);
		}
	}
	
#endif

	/* Jump out of the loop based on the result of above comparison is false */
	ProcessICInstIFNOT(psUFContext, *ppsPredicate);
		CreateInstruction(psUFContext, UFOP_BREAK);
	ProcessICInstENDIF(psUFContext);

	/* i = i - 1 */
	AddSimpleALUToUFCode(psCPD, psUFContext, &sIterator, &sIterator, &sOne, IMG_NULL, UFOP_SUB, 2);

	/* setp p1 i >= 0 */
	AddComparisonToUFCode(psCPD, psUFContext, &sLoopEndPredicate, &sIterator, &sZero, UFREG_COMPOP_GE, IMG_NULL);

	/* p1 ENDLOOP */
	ProcessICInstENDLOOP(psUFContext, &sLoopEndPredicate);

	return IMG_TRUE;

}
/*****************************************************************************
 FUNCTION	: GetValidComponentCount

 PURPOSE	: 
 PARAMETERS	: 

 RETURNS	: IMG_TRUE if successful
*****************************************************************************/
static IMG_UINT32 GetValidComponentCount(GLSLCompilerPrivateData *psCPD,
										 GLSLUniFlexContext		*psUFContext,
										 GLSLFullySpecifiedType *psFullType, 
										 IMG_INT32				iArraySize)
{
	IMG_UINT32 uValidComponents;

	/* Calculate total number of valid components to be compared */

	if(GLSL_IS_STRUCT(psFullType->eTypeSpecifier))
	{
		GLSLStructureAlloc *psStructAlloc = GetStructAllocInfo(psCPD, psUFContext, psFullType); 

		uValidComponents = psStructAlloc->uTotalValidComponents;
	}
	else
	{
		uValidComponents = TYPESPECIFIER_NUM_ELEMENTS(psFullType->eTypeSpecifier);
	}

	uValidComponents *= (IMG_UINT32)(iArraySize);

	return uValidComponents;
}


/*****************************************************************************
 FUNCTION	: AddPredicatedWholeTypeComparision

 PURPOSE	: Generate UniFlex code to compare two sources, the comparison result is a bool.
			  Sources are struct, matrix or vector, the type must be the same

 PARAMETERS	: psDest				- Variable where the result of the comparison will be put.
			  psSrcA				- First variable to compare.
			  psSrcB				- Second variable to compare.
			  ppsPredicate			- A place hold for whether predicated

 RETURNS	: IMG_TRUE if successful
*****************************************************************************/
static IMG_VOID AddPredicatedWholeTypeComparision(GLSLCompilerPrivateData *psCPD,
												  GLSLUniFlexContext	*psUFContext,
												   ICUFOperand			*psDest,
												   ICUFOperand			*psSrcA,
												   ICUFOperand			*psSrcB,
												   IMG_BOOL				bEquality,
												   ICUFOperand			**ppsPredicate,
												   IMG_UINT32			*puNumENDIFsRequired)

{
	IMG_UINT32 i;
	UFREG_COMPOP uCompOp = bEquality ? UFREG_COMPOP_EQ : UFREG_COMPOP_NE;

	DebugAssert(psSrcA->eTypeAfterSwiz == psSrcB->eTypeAfterSwiz);
	DebugAssert(psSrcA->iArraySize == psSrcB->iArraySize);

	/* Compare two arrays or two user-defined structures */
	if(psSrcA->iArraySize)
	{
		ICUFOperand sTSrcA = *psSrcA, sTSrcB = *psSrcB;

		IMG_UINT32 uValidComponents = GetValidComponentCount(psCPD, psUFContext, &psSrcA->sFullType, psSrcA->iArraySize);
		
		DebugAssert(psSrcA->iArraySize == psSrcB->iArraySize);

		if(uValidComponents > MAX_COMPONENTS_UNROLLING_ARRAY_LOOP)
		{
			DebugAssert(psSrcA->eRegType != UFREG_TYPE_TEMP);
			DebugAssert(psSrcB->eRegType != UFREG_TYPE_TEMP);

			/* Use GLSLLOOP */
			AddArrayComparison(psCPD, psUFContext, psDest, &sTSrcA, &sTSrcB, bEquality, ppsPredicate);
		}
		else
		{
			/* Unroll it */
			for(i = 0; i < (IMG_UINT32)psSrcA->iArraySize; i++)
			{
				ChooseArrayElement(psCPD, psSrcA, &sTSrcA, i);
				ChooseArrayElement(psCPD, psSrcB, &sTSrcB, i);

				AddPredicatedWholeTypeComparision(psCPD, psUFContext, psDest, &sTSrcA, &sTSrcB, bEquality, ppsPredicate, puNumENDIFsRequired);
			}
		}

		return;
	}
	else if(GLSL_IS_STRUCT(psSrcA->sFullType.eTypeSpecifier))
	{
		ICUFOperand sTSrcA = *psSrcA, sTSrcB = *psSrcB;

		GLSLStructureAlloc *psStructAlloc;

		psStructAlloc = GetStructAllocInfo(psCPD, psUFContext, &psSrcA->sFullType);

		for(i = 0; i < psStructAlloc->uNumMembers; i++)
		{
			ChooseStructMember(psCPD, psUFContext, psSrcA, psStructAlloc, &sTSrcA, i);
			ChooseStructMember(psCPD, psUFContext, psSrcB, psStructAlloc, &sTSrcB, i);

			AddPredicatedWholeTypeComparision(psCPD, psUFContext, psDest, &sTSrcA, &sTSrcB, bEquality, ppsPredicate, puNumENDIFsRequired);

		}

		return;
	}		
	else if(GLSL_IS_MATRIX(psSrcA->eTypeAfterSwiz))
	{
		ICUFOperand sTSrcA = *psSrcA, sTSrcB = *psSrcB;

		for(i = 0; i < TYPESPECIFIER_NUM_COLS(psSrcA->eTypeAfterSwiz); i++)
		{
			ChooseMatrixColumn(psCPD, psSrcA, &sTSrcA, i);
			ChooseMatrixColumn(psCPD, psSrcB, &sTSrcB, i);

			AddPredicatedWholeTypeComparision(psCPD, psUFContext, psDest, &sTSrcA, &sTSrcB, bEquality, ppsPredicate, puNumENDIFsRequired);
		}

		return;
	}
	else if(GLSL_IS_VECTOR(psSrcA->eTypeAfterSwiz))
	{
		if(	TYPESPECIFIER_NUM_COMPONENTS(psSrcA->eTypeAfterSwiz) == 4 &&
			psSrcA->sFullType.ePrecisionQualifier == GLSLPRECQ_LOW &&
			psSrcA->uUFSwiz == UFREG_SWIZ_RGBA)
		{
			/* 
				We can use single instructon for vec4 lowp data comparison - use andall and orall version of SETP 
				In other cases, we may generate inefficient HW code
			*/

			uCompOp |= bEquality ? UFREG_COMPCHANOP_ANDALL : UFREG_COMPCHANOP_ORALL;

			/* Continue to the end block to actually generate code */	
		}
		else
		{

			ICUFOperand sTSrcA = *psSrcA, sTSrcB = *psSrcB;

			for(i = 0; i < TYPESPECIFIER_NUM_COMPONENTS(psSrcA->eTypeAfterSwiz); i++)
			{
				ChooseVectorComponent(psCPD, psSrcA, &sTSrcA, (GLSLICVecComponent)i);
				ChooseVectorComponent(psCPD, psSrcB, &sTSrcB, (GLSLICVecComponent)i);

				AddPredicatedWholeTypeComparision(psCPD, psUFContext, psDest, &sTSrcA, &sTSrcB, bEquality, ppsPredicate, puNumENDIFsRequired);
			}

			return;
		}
	}
	
	/* 
		This is the place where we actually generate instruction 
	*/
#if USE_IFBLOCKS_FOR_COMPARISON

	if(psPredicate)
	{
		ProcessICInstIF(psUFContext, psPredicate);
		(*puNumENDIFsRequired)++;
	}

	AddComparisonToUFCode(psCPD, psUFContext, psDest, psSrcA, psSrcB, uCompOp, IMG_NULL);
#else
	AddComparisonToUFCode(psCPD, psUFContext, psDest, psSrcA, psSrcB, uCompOp, *ppsPredicate);
#endif

	/*	
		If we have not got predicate yet, for the next comparison, we will need to 
		compare based on the result of the first comparison result
	*/
	if(!(*ppsPredicate))
	{	
		ICUFOperand *psPredicate = DebugMemAlloc(sizeof(ICUFOperand));

		(*psPredicate) = (*psDest);

		if(!bEquality)
		{
			/* We need negate the predicate */
			psPredicate->eICModifier |= GLSLIC_MODIFIER_NEGATE;
		}

		*ppsPredicate  = psPredicate;
	}
	
}

#endif /* !USE_PREDICATED_COMPARISON */


/*****************************************************************************
 FUNCTION	: AddDirectTextureLookupToUFCode
    
 PURPOSE	: Generate UniFlex code to do a texture lookup.

 PARAMETERS	: psDest				- Variable where the result of the lookup will be put.
			  psSampler				- Variable giving the texture to lookup in.
			  psTextureCoordinates	- Variable giving the coordinates for the lookup.
			  eLookupOpcode			- Opcode for the lookup.
			  
 RETURNS	: IMG_TRUE if successful
*****************************************************************************/
static IMG_BOOL AddDirectTextureLookupToUFCodeFn(GLSLCompilerPrivateData *psCPD,
											   GLSLUniFlexContext	*psUFContext,
											   ICUFOperand			*psDest,
											   ICUFOperand			*psSampler,
											   ICUFOperand			*psTextureCoordinate,
											   ICUFOperand			*psLodAdjust,
											   UF_OPCODE			eLookupOpcode)

{
	ICUFOperand sTResult, *psTDest = psDest;
	IMG_BOOL bHasDestSwiz = IMG_FALSE;
	PUNIFLEX_INST psProg;

	DebugAssert(psSampler->eRegType == UFREG_TYPE_TEX);

	/* If the destination is not the default swiz, we need a temporary to store the lookup result */
	if(psDest->uCompStart || psDest->sICSwizMask.uNumComponents)
	{
		bHasDestSwiz = IMG_TRUE;
		
		GetTemporary(psCPD, psUFContext, GLSLTS_VEC4, psDest->sFullType.ePrecisionQualifier, &sTResult);
	
		psTDest = &sTResult;
	}

	psProg = CreateInstruction(psUFContext, eLookupOpcode);		if(!psProg) return IMG_FALSE;

	ConvertToUFDestination(&psProg->sDest, psTDest, NULL);

	ConvertToUFSource(&psProg->asSrc[0], psTextureCoordinate, IMG_FALSE, 0, 0);

	if (eLookupOpcode == UFOP_LDL || eLookupOpcode == UFOP_LDB)
	{
		if (psLodAdjust != NULL)
		{
			ConvertToUFSource(&psProg->asSrc[2], psLodAdjust, IMG_FALSE, 0, 0);
		}
		else
		{
			psProg->asSrc[2] = g_sSourceZero;
		}
	}

	ConvertSamplerToUFSource(&psProg->asSrc[1], psSampler);

	if(bHasDestSwiz)
	{
		AddAluToUFCode(psUFContext, psDest, psTDest, IMG_NULL, IMG_NULL, UFOP_MOV, 2, IMG_TRUE);
	}
	return IMG_TRUE;
}

	#define AddDirectTextureLookupToUFCode(psCPD, psUFContext, psDest, psSampler, psTextureCoordinate, psLodAdjust, psOffset, eLookupOpcode) \
		AddDirectTextureLookupToUFCodeFn(psCPD, psUFContext, psDest, psSampler, psTextureCoordinate, psLodAdjust, eLookupOpcode)

	#define ProcessVPInstTEXLDP(psCPD, psUFContext, psDest, psSrcA, psSrcB, psSrcC) \
		ProcessVPInstTEXLDPFn(psCPD, psUFContext, psDest, psSrcA, psSrcB)
	#define ProcessFPInstTEXLDP(psCPD, psUFContext, psDest, psSrcA, psSrcB, psSrcC) \
		ProcessFPInstTEXLDPFn(psCPD, psUFContext, psDest, psSrcA, psSrcB)
	#define ProcessICInstTEXLDD(psCPD, psUFContext, psDest, psSampler,psCoord, psDPdx, psDPdy, psOffset) \
		ProcessICInstTEXLDDFn(psCPD, psUFContext, psDest, psSampler,psCoord, psDPdx, psDPdy)

/*****************************************************************************
 FUNCTION	: FindLabel

 PURPOSE	: Find the UniFlex label corresponding to the symbol for an intermediate code function.

 PARAMETERS	: uSymbolId		- Symbol id for the intermediate code function.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_UINT32 FindLabel(GLSLUniFlexContext *psUFContext, IMG_UINT32 uSymbolId)
{
	IMG_UINT32 i;
	for (i = 0; i < UF_MAX_LABEL; i++)
	{
		if (psUFContext->auLabelToSymbolId[i] == uSymbolId)
		{
			break;
		}
	}
	if (i == UF_MAX_LABEL)
	{
		for (i = 0; i < UF_MAX_LABEL; i++)
		{
			if (psUFContext->auLabelToSymbolId[i] == (IMG_UINT32)-1)
			{
				psUFContext->auLabelToSymbolId[i] = uSymbolId;
				return i;
			}
		}
		return (IMG_UINT32)-1;
	}
	return i;
}

/*****************************************************************************
 FUNCTION	: GenerateZOutput

 PURPOSE	: Generate UniFlex code to output the pixel depth.

 PARAMETERS	: psUFContext		- UF context.

 RETURNS	: IMG_TRUE if successful
*****************************************************************************/
static IMG_BOOL GenerateZOutput(GLSLCompilerPrivateData *psCPD, GLSLUniFlexContext *psUFContext)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psUFContext->psICProgram);

	if (psICContext->eBuiltInsWrittenTo & GLSLBVWT_FRAGDEPTH )
	{
		PUNIFLEX_INST psProg;

		IMG_UINT32 uFragDepthSymbolId;
		ICUFOperand sUFFragDepth;

		if (!FindSymbol(psUFContext->psSymbolTable, "gl_FragDepth", &uFragDepthSymbolId, IMG_FALSE))
		{
			LOG_INTERNAL_ERROR(("GenerateZOutput: Failed to Find symbol gl_FragDepth \n"));
			return IMG_FALSE;
		}

		InitICUFOperand(psCPD, psUFContext, uFragDepthSymbolId, &sUFFragDepth, IMG_FALSE);

		psProg = CreateInstruction(psUFContext, UFOP_MOV);		if(!psProg) return IMG_FALSE;
		InitUFDestination(&psProg->sDest, UFREG_TYPE_PSOUTPUT, UFREG_OUTPUT_Z, MASK_X);
		ConvertToUFSource(&psProg->asSrc[0], &sUFFragDepth, IMG_FALSE, 0, 0);
	}

	return IMG_TRUE;
}

/* 
	Face color selection code :

// If one of gl_FrontFacing, gl_Color, gl_SecondaryColor occurs in FP
// Add the following identifiers to the symbol table

uniform bool gl_PMXTwoSideEnable;
uniform bool gl_PMXSwapFrontFace;

varying vec4 gl_FrontColor;
varying vec4 gl_FrontSecondaryColor;
varying vec4 gl_BackColor;
varying vec4 gl_BackSecondaryColor;

gl_FrontFacing = miscFaceType;

gl_FrontFacing = true;
gl_Color = gl_FrontColor;
gl_SecondaryColor = gl_FrontSecondaryColor;

if(gl_PMXTwoSideEnable)
{
	gl_FrontFacing = miscFaceType;

	// If one of gl_FrontFacing, gl_Color, gl_SecondaryColor occurs in FP 	
	if(gl_PMXSwapFrontFace)
	{
		gl_FrontFacing != gl_FrontFacing;	
	}

	// if gl_Color is used in FP 
	if(!gl_FrontFacing)
	{
		gl_Color = gl_PMXBackColor;
	}

	// if gl_SecondaryColor is used in FP 
	if(!gl_FrontFacing)
	{
		gl_SecondaryColor = gl_PMXBackSecondaryColor;
	}
}
*/


/*****************************************************************************
 FUNCTION	: GenerateFrontFacingData

 PURPOSE	: Generate instruction to put the front-facing data in the right format.

 PARAMETERS	: None

 RETURNS	: true if successful
*****************************************************************************/
static IMG_BOOL GenerateFrontFacingData(GLSLUniFlexContext *psUFContext, ICUFOperand *psFrontFacingOperand)
{
	PUNIFLEX_INST psProg;

	/* IF FrontFace THEN rN = 0 ELSE rN = 1 ENDIF */
	psProg = CreateInstruction(psUFContext, UFOP_IFC);		if(!psProg) return IMG_FALSE;

	InitUFSource(&psProg->asSrc[0], UFREG_TYPE_MISC, UF_MISC_FACETYPE, UFREG_SWIZ_XXXX);
	InitUFSource(&psProg->asSrc[1], UFREG_TYPE_COMPOP, UFREG_COMPOP_LT, UFREG_SWIZ_NONE);
	psProg->asSrc[2] = g_sSourceZero;
	psProg->asSrc[2].eFormat = psProg->asSrc[0].eFormat;


		/* gl_FrontFacing = 1 */
		psProg = CreateInstruction(psUFContext, UFOP_MOV);		if(!psProg) return IMG_FALSE;
		ConvertToUFDestination(&psProg->sDest, psFrontFacingOperand, NULL);
		AssignSourceONE(&psProg->asSrc[0], psProg->sDest.eFormat);

	/* ELSE */
	psProg = CreateInstruction(psUFContext, UFOP_ELSE);		if(!psProg) return IMG_FALSE;

		/* gl_FrongFacing = 0 */
		psProg = CreateInstruction(psUFContext, UFOP_MOV);		if(!psProg) return IMG_FALSE;
		ConvertToUFDestination(&psProg->sDest, psFrontFacingOperand, NULL);
		psProg->asSrc[0] = g_sSourceZero;
		psProg->asSrc[0].eFormat = psProg->sDest.eFormat;

	/* ENDIF */
	psProg = CreateInstruction(psUFContext, UFOP_ENDIF);		if(!psProg) return IMG_FALSE;

	return IMG_TRUE;
}


/*****************************************************************************
 FUNCTION	: GenerateFaceSelection

 PURPOSE	: Generate instructions to update various variables depending on which face is active.

 PARAMETERS	: psUFContext

 RETURNS	: 
*****************************************************************************/

#if defined(GLSL_ES)

static IMG_BOOL GenerateFaceSelection(GLSLCompilerPrivateData *psCPD, GLSLUniFlexContext *psUFContext)
{

#ifndef OGLES2_VARYINGS_PACKING_RULE
	PUNIFLEX_INST psOldEnd;
#endif
	PUNIFLEX_INST psProg;

	IMG_UINT32 uPMXSwapFrontFaceID;
	ICUFOperand sPMXSwapFrontFaceOperand;

	IMG_UINT32 uFrontFacingSymbolId;
	ICUFOperand sFrontFacingOperand;

#ifndef OGLES2_VARYINGS_PACKING_RULE
	psOldEnd = psUFContext->psEndUFInst;
#endif

	/*
		gl_PMXSwapFrontFace
	*/
	if(!AddBuiltInIdentifier(psCPD, psUFContext->psSymbolTable,
							 "gl_PMXSwapFrontFace",
							 0,
							 GLSLBV_PMXSWAPFRONTFACE,
							 GLSLTS_BOOL,
							 GLSLTQ_UNIFORM,
							 GLSLPRECQ_UNKNOWN,
							 &uPMXSwapFrontFaceID))
	{
		LOG_INTERNAL_ERROR(("GenerateFaceSelection: Failed to add PMX builtin variable gl_PMXSwapFrontFace to the symbol table !\n"));
		return IMG_FALSE;
	}

	InitICUFOperand(psCPD, psUFContext, uPMXSwapFrontFaceID, &sPMXSwapFrontFaceOperand, IMG_FALSE);

	/* gl_FrontFacing */
	if (!FindSymbol(psUFContext->psSymbolTable, "gl_FrontFacing", &uFrontFacingSymbolId, IMG_FALSE))
	{
		LOG_INTERNAL_ERROR(("GenerateFrontFacingData: Failed to find gl_FrontFacing in symbol table \n"));
		return IMG_FALSE;
	}

	InitICUFOperand(psCPD, psUFContext, uFrontFacingSymbolId, &sFrontFacingOperand, IMG_FALSE);

	/* gl_FrontFace = IMG_TRUE; */
	if(psUFContext->bFrontFacingUsed)
	{
		psProg = CreateInstruction(psUFContext, UFOP_MOV);		if(!psProg) return IMG_FALSE;
		ConvertToUFDestination(&psProg->sDest, &sFrontFacingOperand, NULL);
		AssignSourceONE(&psProg->asSrc[0], psProg->sDest.eFormat);
	}

	/* gl_FrontFacing = miscFaceType */
	GenerateFrontFacingData(psUFContext, &sFrontFacingOperand);

	/* IF gl_PMXSwapFrontFace THEN gl_FrontFacing = !gl_FrontFacing */
	AddIFCToUFCode(psUFContext, &sPMXSwapFrontFaceOperand, UFREG_COMPOP_NE);

		AddNOTToUFCode(psUFContext, &sFrontFacingOperand, &sFrontFacingOperand);
	
	psProg = CreateInstruction(psUFContext, UFOP_ENDIF);		if(!psProg) return IMG_FALSE;

#ifndef OGLES2_VARYINGS_PACKING_RULE
	/* Move the newly added code to the begining of the program */
	psUFContext->psEndUFInst->psILink = psUFContext->psFirstUFInst;
	psUFContext->psFirstUFInst = psOldEnd->psILink;
	psOldEnd->psILink = IMG_NULL;
	psUFContext->psEndUFInst = psOldEnd;
#endif

	return IMG_TRUE;
}

#else

static IMG_BOOL GenerateFaceSelection(GLSLCompilerPrivateData *psCPD, GLSLUniFlexContext *psUFContext)
{

	PUNIFLEX_INST psProg;

	IMG_UINT32 uPMXSwapFrontFaceID;
	ICUFOperand sPMXSwapFrontFaceOperand;

	IMG_UINT32 uPMXTwoSideEnableID;
	ICUFOperand sPMXTwoSideEnableOperand;

	IMG_UINT32 uColorID;
	ICUFOperand sColorOperand;

	IMG_UINT32 uSecondaryColorID;
	ICUFOperand sSecondaryColorOperand;

	IMG_UINT32 uFrontFacingSymbolId;
	ICUFOperand sFrontFacingOperand;

	GLSLPrecisionQualifier eGLColorPrecision = GLSLPRECQ_UNKNOWN;
	GLSLPrecisionQualifier eGLSecondaryColorPrecision = GLSLPRECQ_UNKNOWN;
	
#ifndef OGLES2_VARYINGS_PACKING_RULE
	PUNIFLEX_INST psOldEnd;
	psOldEnd = psUFContext->psEndUFInst;
#endif

	/*
		gl_PMXTwoSideEnable
	*/
	if(!AddBuiltInIdentifier(psCPD, psUFContext->psSymbolTable,
							 "gl_PMXTwoSideEnable",
							 0,
							 GLSLBV_PMXTWOSIDEENABLE,
							 GLSLTS_BOOL,
							 GLSLTQ_UNIFORM,
							 GLSLPRECQ_UNKNOWN,
							 &uPMXTwoSideEnableID))
	{
		LOG_INTERNAL_ERROR(("GenerateFaceSelection: Failed to add PMX builtin variable gl_PMX_TwoSideEnable to the symbol table !\n"));
		return IMG_FALSE;
	}
	InitICUFOperand(psCPD, psUFContext, uPMXTwoSideEnableID, &sPMXTwoSideEnableOperand, IMG_FALSE);

	/*
		gl_PMXSwapFrontFace
	*/
	if(!AddBuiltInIdentifier(psCPD, psUFContext->psSymbolTable,
							 "gl_PMXSwapFrontFace",
							 0,
							 GLSLBV_PMXSWAPFRONTFACE,
							 GLSLTS_BOOL,
							 GLSLTQ_UNIFORM,
							 GLSLPRECQ_UNKNOWN,
							 &uPMXSwapFrontFaceID))
	{
		LOG_INTERNAL_ERROR(("GenerateFaceSelection: Failed to add PMX builtin variable gl_PMXSwapFrontFace to the symbol table !\n"));
		return IMG_FALSE;
	}

	InitICUFOperand(psCPD, psUFContext, uPMXSwapFrontFaceID, &sPMXSwapFrontFaceOperand, IMG_FALSE);

	/* gl_FrontFacing */
	if (!FindSymbol(psUFContext->psSymbolTable, "gl_FrontFacing", &uFrontFacingSymbolId, IMG_FALSE))
	{
		LOG_INTERNAL_ERROR(("GenerateFrontFacingData: Failed to find gl_FrontFacing in symbol table \n"));
		return IMG_FALSE;
	}

	InitICUFOperand(psCPD, psUFContext, uFrontFacingSymbolId, &sFrontFacingOperand, IMG_FALSE);

	/* gl_FrontFacing = miscFaceType */
	GenerateFrontFacingData(psUFContext, &sFrontFacingOperand);

	/* IF gl_PMXSwapFrontFace THEN gl_FrontFacing = !gl_FrontFacing */
	AddIFCToUFCode(psUFContext, &sPMXSwapFrontFaceOperand, UFREG_COMPOP_NE);

		AddNOTToUFCode(psUFContext, &sFrontFacingOperand, &sFrontFacingOperand);
	
	psProg = CreateInstruction(psUFContext, UFOP_ENDIF);		if(!psProg) return IMG_FALSE;

	/* gl_Color = gl_FrontColor */
	if (psUFContext->bColorUsed)
	{
		IMG_UINT32 uFrontColorID;
		ICUFOperand sFrontColorOperand;

		/*
			gl_Color
		*/
		if(!FindSymbol(psUFContext->psSymbolTable, "gl_Color", &uColorID, IMG_FALSE))
		{
			LOG_INTERNAL_ERROR(("GenerateFaceSelection: Failed to find gl_Color in symbol table \n"));
			return IMG_FALSE;
		}

		/* Get the precision for gl_Color */
		eGLColorPrecision = ICGetSymbolPrecision(psCPD, psUFContext->psSymbolTable, uColorID);

		InitICUFOperand(psCPD, psUFContext, uColorID, &sColorOperand, IMG_FALSE);	

#ifdef OGLES2_VARYINGS_PACKING_RULE
		if(!FindSymbol(psUFContext->psSymbolTable, "gl_FrontColor", &uFrontColorID, IMG_FALSE))
		{
			LOG_INTERNAL_ERROR(("GenerateFaceSelection: Failed to find gl_FrontColor in symbol table \n"));
			return IMG_FALSE;
		}
#else
		/* gl_FrontColor */
		if(!AddBuiltInIdentifier(psCPD, psUFContext->psSymbolTable,
								 "gl_FrontColor",
								 0,
								 GLSLBV_FRONTCOLOR,
								 GLSLTS_VEC4,
								 GLSLTQ_VARYING,
								 eGLColorPrecision,
								 &uFrontColorID))
		{
			LOG_INTERNAL_ERROR(("GenerateFaceSelection: Failed to add PMX builtin variable gl_PMXFrontColor to the symbol table !\n"));
			return IMG_FALSE;
		}
#endif

		InitICUFOperand(psCPD, psUFContext, uFrontColorID, &sFrontColorOperand, IMG_FALSE);

		AddSimpleALUToUFCode(psCPD, psUFContext, 
							 &sColorOperand, 
							 &sFrontColorOperand, 
							 NULL, NULL, UFOP_MOV, 1);
	}

	/* gl_SecondaryColor = gl_FrontSecondaryColor */
	if (psUFContext->bSecondaryColorUsed)
	{
		IMG_UINT32 uSymID;
		ICUFOperand sOperand;
		
		/*
			gl_SecondaryColor
		*/
		if (!FindSymbol(psUFContext->psSymbolTable, "gl_SecondaryColor", &uSecondaryColorID, IMG_FALSE))
		{
			LOG_INTERNAL_ERROR(("GenerateFaceSelection: Failed to find gl_SecondaryColor(PMX) in symbol table \n"));
			return IMG_FALSE;
		}

		/* Get the precision for gl_SecondaryColor */
		eGLSecondaryColorPrecision = ICGetSymbolPrecision(psCPD, psUFContext->psSymbolTable, uSecondaryColorID);

		InitICUFOperand(psCPD, psUFContext, uSecondaryColorID, &sSecondaryColorOperand, IMG_FALSE);

#ifdef OGLES2_VARYINGS_PACKING_RULE

		/*
			gl_FrontSecondaryColor
		*/

		if (!FindSymbol(psUFContext->psSymbolTable, "gl_FrontSecondaryColor", &uSymID, IMG_FALSE))
		{
			LOG_INTERNAL_ERROR(("GenerateFaceSelection: Failed to find gl_FrontSecondaryColor(PMX) in symbol table \n"));
			return IMG_FALSE;
		}
#else

		/*
			gl_FrontSecondaryColor
		*/
		if(!AddBuiltInIdentifier(psCPD, psUFContext->psSymbolTable,
								 "gl_FrontSecondaryColor",
								 0,
								 GLSLBV_FRONTSECONDARYCOLOR,
								 GLSLTS_VEC4,
								 GLSLTQ_VARYING,
								 eGLSecondaryColorPrecision,
								 &uSymID))
		{
			LOG_INTERNAL_ERROR(("GenerateFaceSelection: Failed to add PMX builtin variable gl_FrontSecondaryColor(PMX) to the symbol table !\n"));
			return IMG_FALSE;
		}
#endif

		InitICUFOperand(psCPD, psUFContext, uSymID, &sOperand, IMG_FALSE);

		/* gl_SecondaryColor = gl_FrontSecondaryColor */
		AddSimpleALUToUFCode(psCPD, psUFContext,
							 &sSecondaryColorOperand,
							 &sOperand,
							 NULL, NULL, UFOP_MOV, 1);
	}

	/* IF gl_PMXTwoSideEnable */
	AddIFCToUFCode(psUFContext, &sPMXTwoSideEnableOperand, UFREG_COMPOP_NE);

	if(psUFContext->bColorUsed || psUFContext->bSecondaryColorUsed)
	{
		/* IF !gl_FrontFacing */
		AddIFCToUFCode(psUFContext, &sFrontFacingOperand, UFREG_COMPOP_EQ);
		
		/* gl_Color = gl_PMXBackColor */
		if (psUFContext->bColorUsed)
		{
			IMG_UINT32 uID;
			ICUFOperand sOperand;
			

#ifdef OGLES2_VARYINGS_PACKING_RULE

			/*
				gl_BackColor
			*/

			if (!FindSymbol(psUFContext->psSymbolTable, "gl_BackColor", &uID, IMG_FALSE))
			{
				LOG_INTERNAL_ERROR(("GenerateFaceSelection: Failed to find gl_FrontSecondaryColor(PMX) in symbol table \n"));
				return IMG_FALSE;
			}
#else

			if(!AddBuiltInIdentifier(psCPD, psUFContext->psSymbolTable,
									 "gl_BackColor",
									 0,
									 GLSLBV_BACKCOLOR,
									 GLSLTS_VEC4,
									 GLSLTQ_VARYING,
									 eGLColorPrecision,
									 &uID))
			{
				LOG_INTERNAL_ERROR(("GenerateFaceSelection: Failed to add PMX builtin variable gl_PMXBackColor to the symbol table !\n"));
				return IMG_FALSE;
			}
#endif

			InitICUFOperand(psCPD, psUFContext, uID, &sOperand, IMG_FALSE);

			AddSimpleALUToUFCode(psCPD, psUFContext, &sColorOperand, &sOperand, NULL, NULL, UFOP_MOV, 1);
		}

		/* gl_SecondaryColor = gl_PMXBackSecondaryColor */
		if (psUFContext->bSecondaryColorUsed)
		{
			IMG_UINT32 uSymID;
			ICUFOperand sOperand;

#ifdef OGLES2_VARYINGS_PACKING_RULE

			/*
				gl_BackColor
			*/

			if (!FindSymbol(psUFContext->psSymbolTable, "gl_BackSecondaryColor", &uSymID, IMG_FALSE))
			{
				LOG_INTERNAL_ERROR(("GenerateFaceSelection: Failed to find gl_BackSecondaryColor(PMX) in symbol table \n"));
				return IMG_FALSE;
			}
#else
			/*
				Get the location of gl_PMXBackSecondaryColor
			*/
			if(!AddBuiltInIdentifier(psCPD, psUFContext->psSymbolTable,
									 "gl_BackSecondaryColor",
									 0,
									 GLSLBV_BACKSECONDARYCOLOR,
									 GLSLTS_VEC4,
									 GLSLTQ_VARYING,
									 eGLSecondaryColorPrecision,
									 &uSymID))
			{
				LOG_INTERNAL_ERROR(("GenerateFaceSelection: Failed to add PMX builtin variable gl_PMXPointSize to the symbol table !\n"));
				return IMG_FALSE;
			}
#endif
		
			InitICUFOperand(psCPD, psUFContext, uSymID, &sOperand, IMG_FALSE);

			/* gl_SecondaryColor = gl_PMXBackSecondaryColor */
			AddSimpleALUToUFCode(psCPD, psUFContext, &sSecondaryColorOperand, &sOperand, NULL, NULL, UFOP_MOV, 1);
		}

		/* ENDIF // IF !gl_FrontFacing */
		psProg = CreateInstruction(psUFContext, UFOP_ENDIF);		if(!psProg) return IMG_FALSE;
	}

	/* ENDIF // IF gl_PMXTwoSideEnable */
	psProg = CreateInstruction(psUFContext, UFOP_ENDIF);		if(!psProg) return IMG_FALSE;

#ifndef OGLES2_VARYINGS_PACKING_RULE

	/* Move the newly added code to the begining of the program */
	psUFContext->psEndUFInst->psILink = psUFContext->psFirstUFInst;
	psUFContext->psFirstUFInst = psOldEnd->psILink;
	psOldEnd->psILink = IMG_NULL;
	psUFContext->psEndUFInst = psOldEnd;

#endif

	return IMG_TRUE;
}

#endif

/*****************************************************************************
 FUNCTION	: GenerateFragCoordAdjust

 PURPOSE	: Generate instructions to update gl_FragCoord.

 PARAMETERS	: psUFContext

 RETURNS	: True on successful
*****************************************************************************/
static IMG_BOOL GenerateFragCoordAdjust(GLSLCompilerPrivateData *psCPD, GLSLUniFlexContext *psUFContext)
{

	PUNIFLEX_INST psOldEnd, psProg;

	IMG_UINT32 uPMXPosAdjustID;
	ICUFOperand sPMXPosAdjustOperand;

	IMG_UINT32 uFragCoordID;
	ICUFOperand sFragCoordOperand;
	GLSLPrecisionQualifier eGLFragCoordPrecision;
	UF_REGFORMAT eUFRegFormat;

	psOldEnd = psUFContext->psEndUFInst;

	/* gl_FragCoord */
	if (!FindSymbol(psUFContext->psSymbolTable, "gl_FragCoord", &uFragCoordID, IMG_FALSE))
	{
		LOG_INTERNAL_ERROR(("GenerateFrontFacingData: Failed to find gl_FragCoord in symbol table \n"));
		return IMG_FALSE;
	}

	eGLFragCoordPrecision = ICGetSymbolPrecision(psCPD, psUFContext->psSymbolTable, uFragCoordID);

	InitICUFOperand(psCPD, psUFContext, uFragCoordID, &sFragCoordOperand, IMG_FALSE);

	eUFRegFormat = ConvertPrecisionToUFRegFormat(psCPD, psUFContext, &sFragCoordOperand.sFullType);

	/*
		gl_PMXPosAdjust
	*/
	if(!AddBuiltInIdentifier(psCPD, psUFContext->psSymbolTable,
							 "gl_PMXPosAdjust",
							 0,
							 GLSLBV_PMXPOSADJUST,
							 GLSLTS_VEC4,
							 GLSLTQ_UNIFORM,
							 eGLFragCoordPrecision,
							 &uPMXPosAdjustID))
	{
		LOG_INTERNAL_ERROR(("GenerateFragCoordAdjust: Failed to add PMX builtin variable gl_PMXPosAdjust to the symbol table !\n"));
		return IMG_FALSE;
	}
	InitICUFOperand(psCPD, psUFContext, uPMXPosAdjustID, &sPMXPosAdjustOperand, IMG_FALSE);


	/* Subtract X and Y clip from the position : gl_FragCoord.xyzw = m0.xyzw - glPMXPosAdjust.zw00 */
	psProg = CreateInstruction(psUFContext, UFOP_SUB);		if(!psProg) return IMG_FALSE;
	
	InitUFDestination(&psProg->sDest, sFragCoordOperand.eRegType, sFragCoordOperand.uRegNum, MASK_XYZW);
	psProg->sDest.eFormat = eUFRegFormat;

	InitUFSource(&psProg->asSrc[0], UFREG_TYPE_MISC, UF_MISC_POSITION, UFREG_SWIZ_NONE);
	psProg->asSrc[0].eFormat = eUFRegFormat;

	InitUFSource(&psProg->asSrc[1], sPMXPosAdjustOperand.eRegType, sPMXPosAdjustOperand.uRegNum, 
				  UFREG_ENCODE_SWIZ(UFREG_SWIZ_Z, UFREG_SWIZ_W, UFREG_SWIZ_0, UFREG_SWIZ_0));
	psProg->asSrc[1].eFormat = eUFRegFormat;


	/* Subtract the Y-position from the constant (which will store the 
	   render target height), to account for inverted Y 
	   gl_FragCoord.y = glPMXPosAdjust.y - gl_FragCoord.y 
	*/
	psProg = CreateInstruction(psUFContext, UFOP_SUB);		if(!psProg) return IMG_FALSE;

	InitUFDestination(&psProg->sDest, sFragCoordOperand.eRegType, sFragCoordOperand.uRegNum, MASK_Y);
	psProg->sDest.eFormat = eUFRegFormat;

	InitUFSource(&psProg->asSrc[0], sPMXPosAdjustOperand.eRegType, sPMXPosAdjustOperand.uRegNum, UFREG_SWIZ_NONE);
	psProg->asSrc[0].eFormat = eUFRegFormat;

	InitUFSource(&psProg->asSrc[1], sFragCoordOperand.eRegType, sFragCoordOperand.uRegNum, UFREG_SWIZ_NONE);
	psProg->asSrc[1].eFormat = eUFRegFormat;

	/* If we don't have an inverted Y, we will have set the position adjust Y component
	   to zero. The previous instruction would then have calculated 0.0f-Ypos. 
	   Perform an ABS, to obtain Ypos. (This is an easy way of supporting both inverted
	   and non-inverted Y drawables without having to recompile programs or use conditionals. */
	psProg = CreateInstruction(psUFContext, UFOP_ABS);		if(!psProg) return IMG_FALSE;

	InitUFDestination(&psProg->sDest, sFragCoordOperand.eRegType, sFragCoordOperand.uRegNum, MASK_Y);
	psProg->sDest.eFormat = eUFRegFormat;

	InitUFSource(&psProg->asSrc[0], sFragCoordOperand.eRegType, sFragCoordOperand.uRegNum, UFREG_SWIZ_NONE);
	psProg->asSrc[0].eFormat = eUFRegFormat;
	
	/* Move the newly added code to the begining of the program */
	psUFContext->psEndUFInst->psILink = psUFContext->psFirstUFInst;
	psUFContext->psFirstUFInst = psOldEnd->psILink;
	psOldEnd->psILink = IMG_NULL;
	psUFContext->psEndUFInst = psOldEnd;

	return IMG_TRUE;
}
/*****************************************************************************
 FUNCTION	: AddDataConversionMOV

 PURPOSE	: Generate Uniflex code to do a mov with consideration of type convesion

 PARAMETERS	: psUFContext		- UF context
			  psDest			- Destination operand
			  psSrc				- Source operand

 RETURNS	: IMG_TRUE if successful
*****************************************************************************/
static IMG_BOOL AddDataConversionMOV(GLSLCompilerPrivateData *psCPD,
									 GLSLUniFlexContext		*psUFContext,
									 ICUFOperand			*psDest,
									 ICUFOperand			*psSrc)
{
	GLSLTypeSpecifier eDestType = psDest->eTypeAfterSwiz;
	GLSLTypeSpecifier eSrcType = psSrc->eTypeAfterSwiz;
	PUNIFLEX_INST psUFInst;

	if(GLSL_IS_INT(eDestType) && GLSL_IS_FLOAT(eSrcType))
	{
		ICUFOperand sTDest = *psDest;

#ifdef GLSL_NATIVE_INTEGERS
		/* If we are converting uniflex floats to ints, then we just do a MOV */
		if(!AddAluToUFCode(psUFContext, &sTDest, psSrc, NULL, NULL, UFOP_MOV, 1, IMG_TRUE)) return IMG_FALSE;
#else

		IMG_UINT32 uNumComponents = TYPESPECIFIER_NUM_COMPONENTS(eSrcType);
		IMG_UINT32 i;
		HWSYMBOL *psDestHWSymbol = GetHWSymbolEntry(psCPD, psUFContext, psDest->uSymbolID, IMG_FALSE);

		if(psSrc->sFullType.ePrecisionQualifier != psDest->sFullType.ePrecisionQualifier)
		{
			/* Conversion from float to int of a different precision. This requires
			   two steps. First we transform the precision, then we transform from float to int.
			
			   ============
			   MOV dest src
			   FLR dest dest
			   ============
			*/
			if(!AddAluToUFCode(psUFContext, &sTDest,  psSrc, NULL, NULL, UFOP_MOV, 1, IMG_TRUE)) return IMG_FALSE;
			if(!AddAluToUFCode(psUFContext, &sTDest, &sTDest, NULL, NULL, UFOP_FLR, 1, IMG_TRUE)) return IMG_FALSE;
		}
		else if(psDest->sFullType.ePrecisionQualifier == GLSLPRECQ_LOW)
		{
			/* Conversion from float to int, where lowp int cannot be represented by lowp float. This requires
			   two steps. First we transform the precision to mediump, then we transform from float to int.
			
			   ============
			   MOV dest src
			   FLR dest dest
			   ============
			*/
			sTDest.sFullType.ePrecisionQualifier = GLSLPRECQ_MEDIUM;
			
			if(!AddAluToUFCode(psUFContext, &sTDest,  psSrc, NULL, NULL, UFOP_MOV, 1, IMG_TRUE)) return IMG_FALSE;
			if(!AddAluToUFCode(psUFContext, &sTDest, &sTDest, NULL, NULL, UFOP_FLR, 1, IMG_TRUE)) return IMG_FALSE;
		}
		else
		{
			/* Simply convert from float to int where both have the same precision.

			   ============
			   FLR dest src
			   ============
			*/

			/* Use floor instruction first */
			if(!AddAluToUFCode(psUFContext, &sTDest, psSrc, NULL, NULL, UFOP_FLR, 1, IMG_TRUE)) return IMG_FALSE;
		}



		/*	
			============
			IF dest < 0
				dest++
			ENDIF
			============

			If a symbol is not marked as full-range integer, then no need to consider negative case
		*/

		/* If the destination is full-range integer, increment it if it is negative */
		if(psDestHWSymbol->eSymbolUsage & GLSLSU_FULL_RANGE_INTEGER)
		{
			for(i = 0; i < uNumComponents; i++)
			{
				/* Working on single component */
				ChooseVectorComponent(psCPD, psDest, &sTDest, (GLSLICVecComponent)i);

				/* IF dest < 0 */
				AddIFCToUFCode(psUFContext, &sTDest, UFREG_COMPOP_LT);

				/* dest++ */
				psUFInst = AddAluToUFCode(psUFContext, &sTDest, &sTDest, NULL, NULL, UFOP_ADD, 1, IMG_TRUE);
				AssignSourceONE(&psUFInst->asSrc[1], psUFInst->asSrc[0].eFormat);

				/* ENDIF */
				if(!CreateInstruction(psUFContext, UFOP_ENDIF)) return IMG_FALSE;
			}
		}

#endif  /* GLSL_NATIVE_INTEGERS */

	}
#ifdef GLSL_NATIVE_INTEGERS
	else if(!GLSL_IS_BOOL(eDestType) && GLSL_IS_BOOL(eSrcType))
	{
		/* Converting from a boolean to a non boolean. */

		psUFInst = AddAluToUFCode(psUFContext, psDest, psSrc, NULL, NULL, UFOP_MOVCBIT, 1, IMG_TRUE);		if(!psUFInst) return IMG_FALSE;
		AssignSourceONE(&psUFInst->asSrc[1], psUFInst->sDest.eFormat);
		psUFInst->asSrc[2] = g_sSourceZero;
		psUFInst->asSrc[2].eFormat = psUFInst->sDest.eFormat;
	}
#endif
	else if(GLSL_IS_BOOL(eDestType) && !GLSL_IS_BOOL(eSrcType))
	{
		/* Converting from a non boolean to a boolean. */

		IMG_UINT32 uNumComponents = TYPESPECIFIER_NUM_COMPONENTS(eSrcType);
		ICUFOperand sTSrc = *psSrc;
		ICUFOperand sTDest = *psDest;
		IMG_UINT32 i;

		/*	
			============
			IF src != 0
				dest = 1
			ELSE
				dest = 0
			ENDIF
			============
		*/

		for(i = 0; i < uNumComponents; i++)
		{
			/* Working on single componensts */
			ChooseVectorComponent(psCPD, psSrc, &sTSrc, (GLSLICVecComponent)i);
			ChooseVectorComponent(psCPD, psDest, &sTDest, (GLSLICVecComponent)i);

			/* IFC src != 0 */
			if(!AddIFCToUFCode(psUFContext, &sTSrc, UFREG_COMPOP_NE)) return IMG_FALSE;

				psUFInst = AddAluToUFCode(psUFContext, &sTDest, NULL, NULL, NULL, UFOP_MOV, 0, IMG_FALSE);		if(!psUFInst) return IMG_FALSE;
				AssignSourceONE(&psUFInst->asSrc[0], psUFInst->sDest.eFormat);
			
			/* else */
			if(!CreateInstruction(psUFContext, UFOP_ELSE)) return IMG_FALSE;
	
				psUFInst = AddAluToUFCode(psUFContext, &sTDest, NULL, NULL, NULL, UFOP_MOV, 0, IMG_FALSE);		if(!psUFInst) return IMG_FALSE;
				psUFInst->asSrc[0] = g_sSourceZero;
				psUFInst->asSrc[0].eFormat = psUFInst->sDest.eFormat;

			/* ENDIF */
			if(!CreateInstruction(psUFContext, UFOP_ENDIF)) return IMG_FALSE;
		}
	}
	else
	{
		/* no data conversion, just do a normal mov */
		if(!AddAluToUFCode(psUFContext, psDest, psSrc, NULL, NULL, UFOP_MOV, 1, IMG_TRUE)) return IMG_FALSE;
	}

	return IMG_TRUE;
}


static IMG_BOOL ProcessICInstMOV(GLSLCompilerPrivateData *psCPD,
								 GLSLUniFlexContext		*psUFContext,
								 ICUFOperand			*psDest,
								 ICUFOperand			*psSrcA);

/*****************************************************************************
 FUNCTION	: AddArrayMOV

 PURPOSE	: Generate UniFlex code to compare two arrays, the comparison result is a bool.

 PARAMETERS	: psDest				- Variable where the result of the comparison will be put.
			  psSrcA				- First variable to compare.

 RETURNS	: IMG_TRUE if successful
*****************************************************************************/
static IMG_BOOL  AddArrayMOV(GLSLCompilerPrivateData *psCPD,
							 GLSLUniFlexContext	*psUFContext,
							 ICUFOperand		*psDest,
							 ICUFOperand		*psSrcA)
{
	ICUFOperand sTDest = *psDest, sTSrcA = *psSrcA;

	ICUFOperand sLoopCount, sIterator, sZero, sOne, sPredicate;

	ICUFOperand sDestOffset, sSrcAOffset, sDestOffset0, sSrcAOffset0;

	DebugAssert(psDest->iArraySize == psSrcA->iArraySize);

	if(!GetIntConst(psCPD, psUFContext, 0, GLSLPRECQ_HIGH, &sZero))
	{
		LOG_INTERNAL_ERROR(("AddArrayComparison: Failed to get an integer"));
		return IMG_FALSE;
	}

	if(!GetIntConst(psCPD, psUFContext, 1, GLSLPRECQ_HIGH, &sOne))
	{
		LOG_INTERNAL_ERROR(("AddArrayComparison: Failed to get an integer"));
		return IMG_FALSE;
	}

	if(!GetIntConst(psCPD, psUFContext, psSrcA->iArraySize -1, GLSLPRECQ_HIGH, &sLoopCount))
	{
		LOG_INTERNAL_ERROR(("AddArrayComparison: Failed to get an integer"));
		return IMG_FALSE;
	}

	if(!GetTemporary(psCPD, psUFContext, GLSLTS_INT, GLSLPRECQ_HIGH, &sIterator))
	{
		LOG_INTERNAL_ERROR(("AddArrayComparison: Failed to get an integer"));
		return IMG_FALSE;
	}	

	if(!IC2UF_GetPredicate(psCPD, psUFContext, &sPredicate))
	{
		LOG_INTERNAL_ERROR(("AddArrayComparison: Failed to get an integer"));
		return IMG_FALSE;	
	}

	if(!GetTemporary(psCPD, psUFContext, GLSLTS_INT, GLSLPRECQ_HIGH, &sDestOffset))
	{
		LOG_INTERNAL_ERROR(("AddArrayComparison: Failed to get temporary"));
		return IMG_FALSE;
	}

	if(!GetTemporary(psCPD, psUFContext, GLSLTS_INT, GLSLPRECQ_HIGH, &sSrcAOffset))
	{
		LOG_INTERNAL_ERROR(("AddArrayComparison: Failed to get temporary"));
		return IMG_FALSE;
	}
	
	if(!GetIntConst(psCPD, psUFContext, psDest->uRegNum, GLSLPRECQ_HIGH, &sDestOffset0))
	{
		LOG_INTERNAL_ERROR(("AddArrayComparison: Failed to get temporary"));
		return IMG_FALSE;
	}

	if(!GetIntConst(psCPD, psUFContext, psSrcA->uRegNum, GLSLPRECQ_HIGH, &sSrcAOffset0))
	{
		LOG_INTERNAL_ERROR(("AddArrayComparison: Failed to get temporary"));
		return IMG_FALSE;
	}

	ChooseArrayElement(psCPD, psDest, &sTDest, 0);
	ChooseArrayElement(psCPD, psSrcA, &sTSrcA, 0);
	sTDest.eRelativeIndex = UFREG_RELATIVEINDEX_A0X;
	sTDest.uRelativeStrideInComponents = psSrcA->uAllocCount;
	sTSrcA.eRelativeIndex = UFREG_RELATIVEINDEX_A0Y;
	sTSrcA.uRelativeStrideInComponents = psSrcA->uAllocCount;

	/* i = MaxIterations - 1 */
	AddSimpleALUToUFCode(psCPD, psUFContext, &sIterator, &sLoopCount, IMG_NULL, IMG_NULL, UFOP_MOV, 1);

	/* LOOP */
	ProcessICInstLOOP(psCPD, psUFContext, IMG_NULL);
	
	/* < body > */

	/* Update offsets: offset = offset0 + i * stride */
	AddSimpleALUToUFCode(psCPD, psUFContext, &sDestOffset, &sIterator, &sDestOffset0, NULL /* psSrcC */, UFOP_ADD, 3);
	AddSimpleALUToUFCode(psCPD, psUFContext, &sSrcAOffset, &sIterator, &sSrcAOffset0, NULL /* psSrcC */, UFOP_ADD, 3);

	/* Mov to index registers */
	AddMOVAToUFCode(psUFContext, &sDestOffset, UFREG_RELATIVEINDEX_A0X);
	AddMOVAToUFCode(psUFContext, &sSrcAOffset, UFREG_RELATIVEINDEX_A0Y);

	ProcessICInstMOV(psCPD, psUFContext, &sTDest, &sTSrcA);

	/* i = i - 1 */
	AddSimpleALUToUFCode(psCPD, psUFContext, &sIterator, &sIterator, &sOne, IMG_NULL, UFOP_SUB, 2);

	/* setp i >= 0 */
	AddComparisonToUFCode(psCPD, psUFContext, &sPredicate, &sIterator, &sZero, UFREG_COMPOP_GE, IMG_NULL);

	/* p1 ENDLOOP */
	ProcessICInstENDLOOP(psUFContext, &sPredicate);

	return IMG_TRUE;

}

/*****************************************************************************
 FUNCTION	: ProcessICInstMOV

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_MOV
			  Note: source and destination can be scalar, vector, matrix, array, struct, array of struct

 PARAMETERS	: psUFContext		- UF context
			  psDest			- Destination operand
			  psSrcA			- Source operand.

 RETURNS	: true if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstMOV(GLSLCompilerPrivateData *psCPD,
								 GLSLUniFlexContext		*psUFContext,
								 ICUFOperand			*psDest,
								 ICUFOperand			*psSrcA)
{
	GLSLTypeSpecifier eDestType = psDest->eTypeAfterSwiz;
	GLSLTypeSpecifier eSrcAType = psSrcA->eTypeAfterSwiz;
	PUNIFLEX_INST psUFInst;

	if (psDest->iArraySize)
	{
		IMG_UINT32 uValidComponents = GetValidComponentCount(psCPD, psUFContext, &psSrcA->sFullType, psSrcA->iArraySize);
		
		DebugAssert(psDest->iArraySize == psSrcA->iArraySize);

		if(uValidComponents > MAX_COMPONENTS_UNROLLING_ARRAY_LOOP)
		{
			DebugAssert(psDest->eRegType != UFREG_TYPE_TEMP);
			DebugAssert(psSrcA->eRegType != UFREG_TYPE_TEMP);

			/* Use a loop */
			AddArrayMOV(psCPD, psUFContext, psDest,psSrcA);
		}
		else
		{
			/* Unroll it */
			ICUFOperand sTSrc = *psSrcA, sTDest = *psDest;
			IMG_INT32 i;

			for(i = 0; i < psDest->iArraySize; i++)
			{
				ChooseArrayElement(psCPD, psDest, &sTDest, i);
				ChooseArrayElement(psCPD, psSrcA, &sTSrc, i);
	
				ProcessICInstMOV(psCPD, psUFContext, &sTDest, &sTSrc);
			}
		}
	}
	else if(GLSL_IS_STRUCT(eDestType))
	{
		ICUFOperand sTSrc = *psSrcA, sTDest = *psDest;

		GLSLStructureAlloc *psStructAlloc;

		IMG_UINT32 i;

		psStructAlloc = GetStructAllocInfo(psCPD, psUFContext, &psDest->sFullType);

		for(i = 0; i < psStructAlloc->uNumMembers; i++)
		{
			ChooseStructMember(psCPD, psUFContext, psDest, psStructAlloc, &sTDest, i);
			ChooseStructMember(psCPD, psUFContext, psSrcA, psStructAlloc, &sTSrc, i);

			ProcessICInstMOV(psCPD, psUFContext, &sTDest, &sTSrc); 
		}
	}

	else if (GLSL_IS_MATRIX(eDestType))
	{
		/* Dest is a matrix */

		if(GLSL_IS_SCALAR(eSrcAType))
		{
			/* Move a scalar to a matrix */
			ICUFOperand sTDest;
			IMG_UINT32 uNumColumns = TYPESPECIFIER_NUM_COLS(eDestType);
			IMG_UINT32 uNumRows = TYPESPECIFIER_NUM_ROWS(eDestType);
			IMG_UINT32 i;

			sTDest = *psDest;

			for(i = 0; i < uNumColumns; i++)
			{
				/* Zero to all components */
				ChooseMatrixColumn(psCPD, psDest, &sTDest, i);
				AddAluToUFCode(psUFContext, &sTDest, NULL, NULL, NULL, UFOP_MOV, 1, IMG_TRUE);

				if(i < uNumRows)
				{
					/* Diagonal components to the scalar */
					ChooseMatrixComponent(psCPD, psDest, &sTDest, i, (GLSLICVecComponent)i);
					if(!AddDataConversionMOV(psCPD, psUFContext, &sTDest, psSrcA)) return IMG_FALSE;
				}
			}
		}
		else if(GLSL_IS_VECTOR(eSrcAType))
		{
			LOG_INTERNAL_ERROR(("ProcessICInstMOV: Canot construct a matrix with a vector\n"));

			return IMG_FALSE;
		}
		else if(GLSL_IS_MATRIX(eSrcAType))
		{	
			/* Construct a matrix from a matrix, note: source and destination can have different dimensions */
			ICUFOperand	sTDest, sTSrcA;
			IMG_UINT32 uComponentsInBatch;
			IMG_UINT32 uDestRows = TYPESPECIFIER_NUM_ROWS(eDestType);
			IMG_UINT32 uDestCols = TYPESPECIFIER_NUM_COLS(eDestType); 
			IMG_UINT32 uSrcRows  = TYPESPECIFIER_NUM_ROWS(eSrcAType);
			IMG_UINT32 uSrcCols  = TYPESPECIFIER_NUM_COLS(eSrcAType);
			IMG_UINT32 i, j;

			sTDest = *psDest;
			sTSrcA = *psSrcA;

			for(i = 0; ((i < uDestCols) && (i < uSrcCols)); i++)
			{
				uComponentsInBatch = MIN(uSrcRows, uDestRows);

				ChooseMatrixColumn(psCPD, psDest, &sTDest, i);
				ChooseMatrixColumn(psCPD, psSrcA, &sTSrcA, i);

				/* Update the swiz of dest according to the num of components in batch */
				sTDest.sICSwizMask.uNumComponents = uComponentsInBatch;
				for(j = 0; j < uComponentsInBatch; j++)
				{
					sTDest.sICSwizMask.aeVecComponent[j] = (GLSLICVecComponent)j;
				}

				UpdateOperandUFSwiz(psCPD, &sTDest);

				/* Update the swiz of src according to the num of components in batch */
				sTSrcA.sICSwizMask.uNumComponents = uComponentsInBatch;
				for(j = 0; j < uComponentsInBatch; j++)
				{
					sTDest.sICSwizMask.aeVecComponent[j] = (GLSLICVecComponent)j;
				}

				UpdateOperandUFSwiz(psCPD, &sTDest);

				/* Move source to destination */
				if(!AddDataConversionMOV(psCPD, psUFContext, &sTDest, &sTSrcA)) return IMG_FALSE;

				/* For the rest of components */
				for(j = uSrcRows; j < uDestRows; j++)
				{
					ChooseMatrixComponent(psCPD, psDest, &sTDest, i, (GLSLICVecComponent)j);
					psUFInst = AddAluToUFCode(psUFContext, &sTDest, NULL, NULL, NULL, UFOP_MOV, 0, IMG_TRUE);		if(!psUFInst) return IMG_FALSE;

					/* Diagonal element is ONE */  
					if(i == j) 
					{
						AssignSourceONE(&psUFInst->asSrc[0], psUFInst->sDest.eFormat);
					}

				}
			}

			/* For the rest of columns */
			for(i = uSrcCols; i < uDestCols; i++)
			{
				for(j = 0; j < uDestRows; j++)
				{
					ChooseMatrixComponent(psCPD, psDest, &sTDest, i, (GLSLICVecComponent)j);

					psUFInst = AddAluToUFCode(psUFContext, &sTDest, NULL, NULL, NULL, UFOP_MOV, 0, IMG_TRUE);		if(!psUFInst) return IMG_FALSE;

					/* Diagonal element is ONE */
					if(i == j) 
					{
						AssignSourceONE(&psUFInst->asSrc[0], psUFInst->sDest.eFormat);
					}
				}
			}
		}
	}
	else 
	{
		/* Destination is a vector or a scalar */
		if(!AddDataConversionMOV(psCPD, psUFContext, psDest, psSrcA)) return IMG_FALSE;
	}

	return IMG_TRUE;
}
/*****************************************************************************
 FUNCTION	: ProcessICInstMUL

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_MUL

 PARAMETERS	: psUFContext		- UF context
			  psDest			- Destination operand
			  psSrcA			- First source operand.
			  psSrcB			- Second source operand.

 RETURNS	: true if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstMUL(GLSLCompilerPrivateData *psCPD,
								 GLSLUniFlexContext	*psUFContext,
							     ICUFOperand		*psDest,
							     ICUFOperand		*psSrcA,
							     ICUFOperand		*psSrcB)
{
	GLSLTypeSpecifier eDestType = psDest->eTypeAfterSwiz;
	GLSLTypeSpecifier eSrcAType = psSrcA->eTypeAfterSwiz;
	GLSLTypeSpecifier eSrcBType = psSrcB->eTypeAfterSwiz;

	IMG_UINT32 i;

	/* A vector multiply a matrix */
	if (GLSL_IS_VECTOR(eSrcAType) && GLSL_IS_MATRIX(eSrcBType))
	{
		ICUFOperand sTDest, sTSrcB;
		UF_OPCODE eUFOpcode = UFOP_NOP;
		IMG_UINT32 uRows = TYPESPECIFIER_NUM_ROWS(eSrcBType);
		IMG_UINT32 uCols = TYPESPECIFIER_NUM_COLS(eSrcBType);
		PUNIFLEX_INST psUFInst;
		ICUFOperand sTemp;

		DebugAssert(GLSL_IS_VECTOR(eDestType));
		DebugAssert(TYPESPECIFIER_NUM_COMPONENTS(eSrcAType) == uRows);
		DebugAssert(TYPESPECIFIER_NUM_COMPONENTS(eDestType) == uCols);

		switch (uRows)
		{
			case 2: 
				eUFOpcode = UFOP_DOT2ADD; 
				break;
			case 3: 
				eUFOpcode = UFOP_DOT3; 
				break;
			case 4: 
				eUFOpcode = UFOP_DOT4; 
				break;
			default: 
				LOG_INTERNAL_ERROR(("ProcessICInstMUL: What type is it?\n")); 
			break;
		}

		sTSrcB = *psSrcB;

		/* Create a temporary */
		if (!GetTemporary(psCPD, psUFContext, eDestType, psDest->sFullType.ePrecisionQualifier, &sTemp))
		{
			return IMG_FALSE;
		}
		sTDest = sTemp;

		/* 
			MUL Dest v0 m
			==>
			DOT Temp.x v0 M[0]
			DOT Temp.y v0 M[1]
			DOT Temp.z v0 M[2]
			DOT Temp.w v0 M[3]
			MOV Dest Temp
		*/
		for(i = 0; i < uCols; i++)
		{
			ChooseMatrixColumn(psCPD, psSrcB, &sTSrcB, i);
			ChooseVectorComponent(psCPD, &sTemp, &sTDest, (GLSLICVecComponent)i);

			/* Add DOT instructions */
			psUFInst = AddAluToUFCode(psUFContext, &sTDest, psSrcA, &sTSrcB, NULL, eUFOpcode, 2, IMG_FALSE);		
			if(!psUFInst) return IMG_FALSE;

			if(uRows == 2)
			{
				psUFInst->asSrc[2].eFormat = psUFInst->asSrc[0].eFormat;
			}

		}

		/* MOV Dest t */
		AddAluToUFCode(psUFContext, psDest, &sTemp, NULL, NULL, UFOP_MOV, 1, IMG_TRUE); 
	}
	else if (GLSL_IS_MATRIX(eSrcAType) && GLSL_IS_VECTOR(eSrcBType))
	{
		/* A matrix multiplies a vector */
		/*
			MUL Dest m v0
			==>
			MUL t M[0] v0.x
			MAD t M[1] v0.y t
			MAD t M[2] v0.z t
			MAD t M[3] v0.w t
			MOV Dest t
		*/
		ICUFOperand sTemp;
		ICUFOperand sTSrcA, sTSrcB;
		IMG_UINT32 uCols = TYPESPECIFIER_NUM_COLS(eSrcAType);

		DebugAssert(GLSL_IS_VECTOR(eDestType));
		DebugAssert(TYPESPECIFIER_NUM_COMPONENTS(eSrcBType) == uCols);
		DebugAssert(TYPESPECIFIER_NUM_COMPONENTS(eDestType) == TYPESPECIFIER_NUM_ROWS(eSrcAType));

		sTSrcB = *psSrcB;
		sTSrcA = *psSrcA;

		/* Create a temporary */
		if (!GetTemporary(psCPD, psUFContext, eDestType, psDest->sFullType.ePrecisionQualifier, &sTemp))
		{
			return IMG_FALSE;
		}
			
		for(i = 0; i < uCols; i++)
		{
			ChooseMatrixColumn(psCPD, psSrcA, &sTSrcA, i);
			ChooseVectorComponent(psCPD, psSrcB, &sTSrcB, (GLSLICVecComponent)i);

			if(i)
			{
				AddAluToUFCode(psUFContext, &sTemp, &sTSrcA, &sTSrcB, &sTemp, UFOP_MAD, 3, IMG_TRUE);
			}
			else
			{
				AddAluToUFCode(psUFContext, &sTemp, &sTSrcA, &sTSrcB, NULL, UFOP_MUL, 2, IMG_TRUE);
			}
		}

		/* MOV Dest t */
		AddAluToUFCode(psUFContext, psDest, &sTemp, NULL, NULL, UFOP_MOV, 1, IMG_TRUE); 
	}
	else if(GLSL_IS_MATRIX(eSrcAType) && GLSL_IS_MATRIX(eSrcBType))
	{
		/* A matrix multiplies a matrix */

		/*
			C[M, N] = A[T, N] * B[M, T]

			c[m][n] = SUM ( a[i][n] * b[m][i]), i = 0 ~ T-1
			m = 0 ~ M-1
			n = 0 ~ N-1

			MUL C A B
			==>						_
			MUL t A[0] B[0].x		|			_
			MAD t A[1] B[0].y t		| T			|
			MAD t A[2] B[0].z t		|
			MAD t A[3] B[0].w t		|
			MOV temp[0] t				_

			MUL t A[0] B[1].x		
			MAD t A[1] B[1].y t					|	M
			MAD t A[2] B[1].z t
			MAD t A[3] B[1].w t 
			MOV temp[1] t

			MUL t A[0] B[2].x 
			MAD t A[1] B[2].y t					|
			MAD t A[2] B[2].z t
			MAD t A[3] B[2].w t 
			MOV temp[2] t

			MUL t A[0] B[3].x 
			MAD t A[1] B[3].y t					|
			MAD t A[2] B[3].z t					_
			MAD t A[3] B[3].w t 
			MOV temp[3] t

			MOV C temp

			t is a temporary vec with N components
		*/

		ICUFOperand sTemp;
		ICUFOperand sTDest, sTSrcA, sTSrcB;
		ICUFOperand sTempDest, sTempDestCol;
		IMG_UINT32 j;
		/* C[M, N] = A[T, N] * B[M, T]*/
		IMG_UINT32 T = TYPESPECIFIER_NUM_COLS(eSrcAType);
		IMG_UINT32 M = TYPESPECIFIER_NUM_COLS(eSrcBType);
		IMG_UINT32 N = TYPESPECIFIER_NUM_ROWS(eSrcAType);

		GLSLTypeSpecifier eTempType = CONVERT_TO_VECTYPE(eSrcAType, N);

		DebugAssert(GLSL_IS_MATRIX(eDestType));
		DebugAssert(TYPESPECIFIER_NUM_ROWS(eSrcBType) == T);
		DebugAssert(TYPESPECIFIER_NUM_COLS(eDestType) == M);
		DebugAssert(TYPESPECIFIER_NUM_ROWS(eDestType) == N); /* = N */

		sTDest = *psDest;
		sTSrcB = *psSrcB;
		sTSrcA = *psSrcA;

		/* Create a temporary */
		if (!GetTemporary(psCPD, psUFContext, eTempType, psDest->sFullType.ePrecisionQualifier, &sTemp))
		{
			return IMG_FALSE;
		}

		/* Create a temporary */
		if (!GetTemporary(psCPD, psUFContext, eDestType, psDest->sFullType.ePrecisionQualifier, &sTempDest))
		{
			return IMG_FALSE;
		}
		sTempDestCol = sTempDest;

		for(j = 0; j < M; j++)
		{
			ChooseMatrixColumn(psCPD, &sTempDest, &sTempDestCol, j);

			for(i = 0; i < T; i++)
			{
				ChooseMatrixColumn(psCPD, psSrcA, &sTSrcA, i);
				ChooseMatrixComponent(psCPD, psSrcB, &sTSrcB, j, (GLSLICVecComponent)i);

				if(i)
				{
					/* Add instruction */
					AddAluToUFCode(psUFContext, &sTemp, &sTSrcA, &sTSrcB, &sTemp, UFOP_MAD, 3, IMG_TRUE);
				}
				else
				{
					AddAluToUFCode(psUFContext, &sTemp, &sTSrcA, &sTSrcB, &sTemp, UFOP_MUL, 2, IMG_TRUE);
				}

			}

			/* MOV temp[M] t */
			AddAluToUFCode(psUFContext, &sTempDestCol, &sTemp, NULL, NULL, UFOP_MOV, 1, IMG_TRUE); 
		}

		for(j = 0; j < M; j++)
		{
			ChooseMatrixColumn(psCPD, psDest, &sTDest, j);
			ChooseMatrixColumn(psCPD, &sTempDest, &sTempDestCol, j);

			/* MOV Dest temp */
			AddAluToUFCode(psUFContext, &sTDest, &sTempDestCol, NULL, NULL, UFOP_MOV, 1, IMG_TRUE); 
		}
	}
	else
	{
		/* Vector by vector */
		/* Vector by scalar */
		/* Scalar by vector */
		/* matrix by scalar */
		/* scalar by matrix */
		/* scalar by scalar */
		AddSimpleALUToUFCode(psCPD, psUFContext, psDest, psSrcA, psSrcB, NULL, UFOP_MUL, 2);
	}

	return IMG_TRUE;

}

/*****************************************************************************
 FUNCTION	: ProcessICInstDIV

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_DIV

 PARAMETERS	: psUFContext		- UF context
			  psDest			- Destination operand
			  psSrcA			- First source operand.
			  psSrcB			- Second source operand.

 RETURNS	: true if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstDIV(GLSLCompilerPrivateData *psCPD,
								 GLSLUniFlexContext		*psUFContext,
							     ICUFOperand			*psDest,
							     ICUFOperand			*psSrcA,
							     ICUFOperand			*psSrcB)
{
	PVR_UNREFERENCED_PARAMETER(psCPD);

#ifndef GLSL_NATIVE_INTEGERS
	if(GLSL_IS_INT(psDest->eTypeAfterSwiz))
	{
		/* Divide */
		AddSimpleALUToUFCode(psCPD, psUFContext, psDest, psSrcA, psSrcB, NULL, UFOP_DIV, 2);

		/* dest = flr(dest) */
		AddSimpleALUToUFCode(psCPD, psUFContext, psDest, psDest, NULL, NULL, UFOP_FLR, 1);
	}
	else
#endif
	{
		AddSimpleALUToUFCode(psCPD, psUFContext, psDest, psSrcA, psSrcB, NULL, UFOP_DIV, 2);
	}

	return IMG_TRUE;
}


/*****************************************************************************
 FUNCTION	: AddWholeTypeComparision

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_DIV

 PARAMETERS	: psUFContext		- UF context
			  psDest			- Destination operand
			  psSrcA			- First source operand.
			  psSrcB			- Second source operand.
			  psPredicate		- Control whether the code is executed based predicate 

 RETURNS	: true if successful
*****************************************************************************/
static IMG_BOOL AddWholeTypeComparision(GLSLCompilerPrivateData *psCPD,
										GLSLUniFlexContext	*psUFContext,
									    ICUFOperand			*psDest,
										ICUFOperand			*psSrcA,
										ICUFOperand			*psSrcB,
										ICUFOperand			*psPredicate,
										IMG_BOOL			bEquality)
{
	
#if USE_PREDICATED_COMPARISON

	IMG_UINT32 i;
	ICUFOperand *psTDest, sPredicateDest;

	ICUFOperand *psTPredicate = IMG_NULL;
	IMG_UINT32 uNumENDIFsRequired = 0;

	/* 
		If the instruction is executed based on predicate, then we need to use IFP/ENDIF block since we 
		are going to generate multiple instructions
	*/
	if(psPredicate)
	{
		ProcessICInstIF(psUFContext, psPredicate);
	}


	/*	If the destination is not a predicate, we write the result to a temp predicate first, 
		and then convert predicate to the result 
	*/
	if(psDest->eRegType != UFREG_TYPE_PREDICATE)
	{
		if(!IC2UF_GetPredicate(psCPD, psUFContext, &sPredicateDest))
		{
			LOG_INTERNAL_ERROR(("AddWholeTypeComparision: Failed to get predicate"));

			return IMG_FALSE;
		}
		psTDest = &sPredicateDest;
	}
	else
	{
		psTDest = psDest;
	}
		
	/* 
		Recursively compare individual members, 
		psTPredicate - new temporary predicate generated while generating 
					   the comparision code, need to free it afterwards
	*/
	AddPredicatedWholeTypeComparision(psCPD, psUFContext, psTDest, psSrcA, psSrcB, bEquality, &psTPredicate, &uNumENDIFsRequired);

	/* Free predicate operand allocate by the above function */
	if(psTPredicate)
	{
		DebugMemFree(psTPredicate);
	}

	/* Issue any outstanding endifs */
	for(i = 0; i < uNumENDIFsRequired; i++)
	{
		ProcessICInstENDIF(psUFContext);
	}

	if(psDest->eRegType != UFREG_TYPE_PREDICATE)
	{
		UNIFLEX_INST *psProg;

		/* Move to the destination */

		/* !p MOV result h0 */
		psProg = CreateInstruction(psUFContext, UFOP_MOV);		if(!psProg) return IMG_FALSE;
		ConvertToUFDestination(&psProg->sDest, psDest, IMG_NULL);
		psProg->asSrc[0] = g_sSourceZero;
		psProg->asSrc[0].eFormat = psProg->sDest.eFormat;
		psProg->uPredicate = ConvertToUFPredicate(&sPredicateDest);
		psProg->uPredicate |= UF_PRED_NEGFLAG;

		/* p MOV result h1 */
		psProg = CreateInstruction(psUFContext, UFOP_MOV);		if(!psProg) return IMG_FALSE;
		ConvertToUFDestination(&psProg->sDest, psDest, NULL);
		AssignSourceONE(&psProg->asSrc[0], psProg->sDest.eFormat);
		psProg->uPredicate = ConvertToUFPredicate(&sPredicateDest);

	}

	/* Issue ENDIF for the begining IF */
	if(psPredicate)
	{
		ProcessICInstENDIF(psUFContext);
	}

#else /* USE_PREDICATED_COMPARISON */

	/* 
		If the instruction is executed based on predicate, then we need to use IFP/ENDIF block since we 
		are going to generate multiple instructions
	*/
	if(psPredicate)
	{
		ProcessICInstIF(psUFContext, psPredicate);
	}

	{
		IMG_BOOL bMultiplyDest = IMG_FALSE;

		AddSlowWholeTypeComparison(psUFContext, psDest, psSrcA, psSrcB, &bMultiplyDest);

		/* Recursively compare individual members */
		if(!bEquality)
		{
			AddNOTToUFCode(psUFContext, psDest, psDest);
		}
	}

	/* Issue ENDIF for the begining IF */
	if(psPredicate)
	{
		ProcessICInstENDIF(psUFContext);
	}
#endif /* USE_PREDICATED_COMPARISON */

	return IMG_TRUE;

}

/*****************************************************************************
 FUNCTION	: ProcessICInstSEQ

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_SEQ

 PARAMETERS	: psUFContext		- UF context
			  psDest			- Destination operand
			  psSrcA			- First source operand.
			  psSrcB			- Second source operand.

 RETURNS	: true if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstSEQ(GLSLCompilerPrivateData *psCPD,
								 GLSLUniFlexContext	*psUFContext,
							     ICUFOperand		*psDest,
							     ICUFOperand		*psSrcA,
							     ICUFOperand		*psSrcB,
								 ICUFOperand		*psPredicate)
{
	GLSLTypeSpecifier eDestType = psDest->eTypeAfterSwiz;
	GLSLTypeSpecifier eSrcAType = psSrcA->eTypeAfterSwiz;

	if (eDestType == GLSLTS_BOOL && (!GLSL_IS_SCALAR(eSrcAType) || psSrcA->iArraySize))
	{
		AddWholeTypeComparision(psCPD, psUFContext, psDest, psSrcA, psSrcB, psPredicate, IMG_TRUE);	
	}
	else
	{
		/* Component wise comparision produce component-wise results */
		AddComparisonToUFCode(psCPD, psUFContext, psDest, psSrcA, psSrcB, UFREG_COMPOP_EQ, psPredicate);
	}

	return IMG_TRUE;
}


/*****************************************************************************
 FUNCTION	: ProcessICInstSNE

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_SNE

 PARAMETERS	: psUFContext		- UF context
			  psDest			- Destination operand
			  psSrcA			- First source operand.
			  psSrcB			- Second source operand.

 RETURNS	: true if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstSNE(GLSLCompilerPrivateData *psCPD,
								 GLSLUniFlexContext	*psUFContext,
							     ICUFOperand		*psDest,
							     ICUFOperand		*psSrcA,
							     ICUFOperand		*psSrcB,
								 ICUFOperand		*psPredicate)
{
	GLSLTypeSpecifier eDestType = psDest->eTypeAfterSwiz;
	GLSLTypeSpecifier eSrcAType = psSrcA->eTypeAfterSwiz;

	if (eDestType == GLSLTS_BOOL && (!GLSL_IS_SCALAR(eSrcAType) || psSrcA->iArraySize))
	{
		AddWholeTypeComparision(psCPD, psUFContext, psDest, psSrcA, psSrcB, psPredicate, IMG_FALSE);
	}
	else
	{
		AddComparisonToUFCode(psCPD, psUFContext, psDest, psSrcA, psSrcB, UFREG_COMPOP_NE, psPredicate);
	}

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: ProcessICInstLABEL

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_LABEL

 PARAMETERS	: psUFContext		- UF context
			  psSrc				- First source operand.

 RETURNS	: true if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstLABEL(GLSLCompilerPrivateData *psCPD,
								   GLSLUniFlexContext	*psUFContext,
									 GLSLICOperand		*psSrc)
{
	UNIFLEX_INST *psProg;
	IMG_UINT32 uLabel = FindLabel(psUFContext, psSrc->uSymbolID);
	if (uLabel == (IMG_UINT32)-1)
	{
		LogProgramError(psCPD->psErrorLog, "Too many functions.\n");
		return IMG_FALSE;
	}
	psProg = CreateInstruction(psUFContext, UFOP_LABEL);		if(!psProg) return IMG_FALSE;
	InitUFSource(&psProg->asSrc[0], UFREG_TYPE_LABEL, uLabel, 0);

	return IMG_TRUE;
}


/*****************************************************************************
 FUNCTION	: GenerateClippingOutput

 PURPOSE	: Generate UF code clipping 

 PARAMETERS	: psUFContext		- UF context
			  psSrc				- First source operand.

 RETURNS	: true if successful

*****************************************************************************/
#if !defined(GLSL_ES)
static IMG_BOOL GenerateClippingOutput(GLSLCompilerPrivateData *psCPD,
									   GLSLUniFlexContext *psUFContext)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psUFContext->psICProgram);
	IMG_INT32 i;
	IMG_INT32 iGLMaxClipPlanes = psICContext->psInitCompilerContext->sCompilerResources.iGLMaxClipPlanes;
	UNIFLEX_INST *psProg;

	{
		IMG_UINT32 uClipVertexID, uClipPlaneID;
		ICUFOperand sClipVertexOperand, sClipPlaneOperand;
		GLSLPrecisionQualifier eGLClipVertexPrecision;

		if (!FindSymbol(psUFContext->psSymbolTable, "gl_ClipVertex", &uClipVertexID, IMG_FALSE))
		{
			LOG_INTERNAL_ERROR(("GenerateClippingOutput: Failed to find gl_ClipVertex in symbol table \n"));
			return IMG_FALSE;
		}

		eGLClipVertexPrecision = ICGetSymbolPrecision(psCPD, psUFContext->psSymbolTable, uClipVertexID);

		if(!AddBuiltInIdentifier(psCPD, psUFContext->psSymbolTable,
								 "gl_PMXClipPlane",
								 iGLMaxClipPlanes,
								 GLSLBV_PMXCLIPPLANE,
								 GLSLTS_VEC4,
								 GLSLTQ_UNIFORM,
								 eGLClipVertexPrecision,
								 &uClipPlaneID))
		{
			LOG_INTERNAL_ERROR(("GenerateClippingOutput: Failed to add gl_PMXClipPlane to the symbol table \n"));
			return IMG_FALSE;
		}

		/* Generate default operand */
		InitICUFOperand(psCPD, psUFContext, uClipVertexID, &sClipVertexOperand, IMG_FALSE);
		InitICUFOperand(psCPD, psUFContext, uClipPlaneID, &sClipPlaneOperand, IMG_FALSE);

		for(i = 0; i < iGLMaxClipPlanes; i++)
		{
			psProg = CreateInstruction(psUFContext, UFOP_DOT4_CP);		if(!psProg) return IMG_FALSE;

			/* Src 0 */
			ConvertToUFSource(&psProg->asSrc[0], &sClipVertexOperand, IMG_FALSE, 0, 0);

			/* Src 1 */
			ConvertToUFSource(&psProg->asSrc[1], &sClipPlaneOperand, IMG_FALSE, 0, 0);
			psProg->asSrc[1].uNum = sClipPlaneOperand.uRegNum + i; /* modify the number of plane */

			/* Src 2 */
			InitUFSource(&psProg->asSrc[2], UFREG_TYPE_CLIPPLANE, i, UFREG_SWIZ_NONE);

			/* Dest */
			InitUFDestination(&psProg->sDest, UFREG_TYPE_VSOUTPUT, (GLSLVR_CLIPPLANE0 + i), MASK_X);
			psProg->sDest.eFormat = psProg->asSrc[0].eFormat;

			/* Set the active varying mask */
			psUFContext->eActiveVaryingMask |= (GLSLVM_CLIPPLANE0 << i);
		
		}
	}

	return IMG_TRUE;

}
#endif


/*****************************************************************************
 FUNCTION	: GenerateBackEndOutputs

 PURPOSE	: Generate front end moves 

 PARAMETERS	: psUFContext		- UF context

 RETURNS	: 
*****************************************************************************/
static IMG_BOOL GenerateBackEndOutputs(GLSLCompilerPrivateData *psCPD,
									   GLSLUniFlexContext *psUFContext)
{
	if(psUFContext->eProgramType == GLSLPT_FRAGMENT)
	{
		if(!GenerateZOutput(psCPD, psUFContext))
		{
			LOG_INTERNAL_ERROR(("GenerateBackEndOutputs: Failed to generate UF code for z output \n"));
			return IMG_FALSE;
		}
	}
	else
	{
#if !defined(GLSL_ES)
		GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psUFContext->psICProgram);

		if(psICContext->eBuiltInsWrittenTo & (GLSLBVWT_CLIPVERTEX
			))
		{
			if(!GenerateClippingOutput(psCPD, psUFContext))
			{
				LOG_INTERNAL_ERROR(("GenerateBackEndOutputs: Failed to generate UF code for clip planes \n"));
				return IMG_FALSE;
			}
		}
#endif
	}

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: ProcessICInstRET

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_RET

 PARAMETERS	: psUFContext		- UF context

 RETURNS	: true if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstRET(GLSLCompilerPrivateData *psCPD,
								 GLSLUniFlexContext *psUFContext)
{
	if(psUFContext->uIfNestCount == 0 && psUFContext->uLoopNestCount == 0)
	{
		if (psUFContext->bFirstRet)
		{	
			GenerateBackEndOutputs(psCPD, psUFContext);

			psUFContext->bFirstRet = IMG_FALSE;
		}
	}

	CreateInstruction(psUFContext, UFOP_RET);

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: ProcessICInstCALL

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_CALL

 PARAMETERS	: psUFContext		- UF context
			  psSrc				- Source operand.

 RETURNS	: true if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstCALL(GLSLCompilerPrivateData *psCPD,
								  GLSLUniFlexContext	*psUFContext,
									 GLSLICOperand		*psSrc)
{
	UNIFLEX_INST *psProg;

	IMG_UINT32 uLabel = FindLabel(psUFContext, psSrc->uSymbolID);
	if (uLabel == (IMG_UINT32)-1)
	{
		LogProgramError(psCPD->psErrorLog, "Too many functions.\n");
		return IMG_FALSE;
	}
	
	psProg = CreateInstruction(psUFContext, UFOP_CALL);		if(!psProg) return IMG_FALSE;

	InitUFSource(&psProg->asSrc[0], UFREG_TYPE_LABEL, uLabel, UFREG_SWIZ_NONE); 		

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: ProcessICInstCONTDEST

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_CONTDEST

 PARAMETERS	: psUFContext		- UF context

 RETURNS	: True if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstCONTDEST(GLSLUniFlexContext *psUFContext)
{
	CreateInstruction(psUFContext, UFOP_GLSLLOOPTAIL);

	return IMG_TRUE;
}


/*****************************************************************************
 FUNCTION	: ProcessICInstCONTINUE

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_CONTINUE

 PARAMETERS	: psUFContext		- UF context

 RETURNS	: True if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstCONTINUE(GLSLUniFlexContext *psUFContext)
{
	CreateInstruction(psUFContext, UFOP_GLSLCONTINUE);

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: ProcessICInstDISCARD

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_DISCARD

 PARAMETERS	: psUFContext		- UF context

 RETURNS	: True if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstDISCARD(GLSLUniFlexContext *psUFContext)
{
	UNIFLEX_INST *psProg;

	psProg = CreateInstruction(psUFContext, UFOP_KPLT);		if(!psProg) return IMG_FALSE;

	psProg->asSrc[0] = g_sSourceMinusOne;

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: ProcessICInstSANY

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_SANY

 PARAMETERS	: psUFContext		- UF context
			  psDest			- Destination operand
			  psSrcA			- First source operand.

 RETURNS	: True if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstSANY(GLSLCompilerPrivateData *psCPD,
								  GLSLUniFlexContext	*psUFContext, 
								  ICUFOperand			*psDest,
								  ICUFOperand			*psSrcA)
{
	AddVectorSANYToUFCode(psCPD, psUFContext, psDest, psSrcA);

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: ProcessICInstSALL

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_SALL

 PARAMETERS	: psUFContext		- UF context
			  psDest			- Destination operand
			  psSrcA			- First source operand.

 RETURNS	: True if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstSALL(GLSLCompilerPrivateData *psCPD,
								  GLSLUniFlexContext	*psUFContext, 
								  ICUFOperand			*psDest,
								  ICUFOperand			*psSrcA)
{
	AddVectorSALLToUFCode(psCPD, psUFContext, psDest, psSrcA);

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: ProcessICInstSGN

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_SGN

 PARAMETERS	: psUFContext		- UF context
			  psDest			- Destination operand
			  psSrcA			- First source operand.

 RETURNS	: True if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstSGN(GLSLCompilerPrivateData *psCPD,
								 GLSLUniFlexContext *psUFContext, 
								  ICUFOperand		*psDest,
								  ICUFOperand		*psSrcA)
{
#if 1
	PVR_UNREFERENCED_PARAMETER(psCPD);
	AddAluToUFCode(psUFContext, psDest, psSrcA, NULL, NULL, UFOP_SGN, 1, IMG_TRUE);	
#else
	GLSLTypeSpecifier eDestType = psDest->eTypeAfterSwiz;
	ICUFOperand sT1, sT2;
	PUNIFLEX_INST psLastUFInst;

	GetTemporary(psCPD, psUFContext, eDestType, psDest->sFullType.ePrecisionQualifier, &sT1);
	GetTemporary(psCPD, psUFContext, eDestType, psDest->sFullType.ePrecisionQualifier, &sT2);

	#if 0
		/* SGN D, A -> CMP T1, A, 0, -1; CMP T2, -A, 1, 0; ADD D, T1, T2 */
		AddSimpleALUToUFCode(psCPD, psUFContext, &sT1, psSrcA, NULL, NULL, UFOP_CMP, 1);
		psUFContext->psEndUFInst->asSrc[1] = g_sSourceZero;
		psUFContext->psEndUFInst->asSrc[2] = g_sSourceMinusOne;

		AddSimpleALUToUFCode(psCPD, psUFContext, &sT2, psSrcA, NULL, NULL, UFOP_CMP, 1);
		psUFContext->psEndUFInst->asSrc[0].byMod = (IMG_BYTE)(psUFContext->psEndUFInst->asSrc[0].byMod ^ UFREG_SOURCE_NEGATE);
		psUFContext->psEndUFInst->asSrc[1] = g_sSourceOne;
		psUFContext->psEndUFInst->asSrc[2] = g_sSourceZero;

		AddSimpleALUToUFCode(psCPD, psUFContext, psDest, &sT1, &sT2, NULL, UFOP_ADD, 2);
	#else
		/* SGN D, A -> CMP T1, A, 1, 0; CMP T2, -A, -1, 0; ADD D, T1, T2 */
		AddSimpleALUToUFCode(psCPD, psUFContext, &sT1, psSrcA, NULL, NULL, UFOP_CMP, 1);
		psLastUFInst = psUFContext->psEndUFInst;
		psLastUFInst->asSrc[1] = g_sSourceOne; 
		psLastUFInst->asSrc[1].eFormat = psLastUFInst->asSrc[0].eFormat;
		psLastUFInst->asSrc[2] = g_sSourceZero; 
		psLastUFInst->asSrc[2].eFormat = psLastUFInst->asSrc[0].eFormat;

		AddSimpleALUToUFCode(psCPD, psUFContext, &sT2, psSrcA, NULL, NULL, UFOP_CMP, 1);
		psLastUFInst = psUFContext->psEndUFInst;
		psLastUFInst->asSrc[0].byMod = (IMG_BYTE)(psUFContext->psEndUFInst->asSrc[0].byMod ^ UFREG_SOURCE_NEGATE);
		psLastUFInst->asSrc[1] = g_sSourceMinusOne;
		psLastUFInst->asSrc[1].eFormat = psLastUFInst->asSrc[0].eFormat;
		psLastUFInst->asSrc[2] = g_sSourceZero;
		psLastUFInst->asSrc[2].eFormat = psLastUFInst->asSrc[0].eFormat;

		AddSimpleALUToUFCode(psCPD, psUFContext, psDest, &sT1, &sT2, NULL, UFOP_ADD, 2);
	#endif
#endif

	return IMG_TRUE;

}

/*****************************************************************************
 FUNCTION	: ProcessICInstDOT

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_DOT

 PARAMETERS	: psUFContext		- UF context
			  psDest			- Destination operand
			  psSrcA			- First source operand.
			  psSrcB			- Second source operand.

 RETURNS	: True if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstDOT(GLSLCompilerPrivateData *psCPD,
								 GLSLUniFlexContext *psUFContext,
								 ICUFOperand		*psDest,
								 ICUFOperand		*psSrcA,
								 ICUFOperand		*psSrcB)
{
	GLSLTypeSpecifier eSrcAType = psSrcA->eTypeAfterSwiz;

	UF_OPCODE eUFOpcode = UFOP_NOP;

	PUNIFLEX_INST psUFInst;

	DebugAssert(eSrcAType == psSrcB->eTypeAfterSwiz);

	switch (eSrcAType)
	{
		case GLSLTS_VEC2:
		case GLSLTS_IVEC2:
		case GLSLTS_BVEC2:
			eUFOpcode = UFOP_DOT2ADD;
			break;

		case GLSLTS_VEC3:
		case GLSLTS_IVEC3:
		case GLSLTS_BVEC3:
			eUFOpcode = UFOP_DOT3;
			break;

		case GLSLTS_VEC4:
		case GLSLTS_IVEC4:
		case GLSLTS_BVEC4:
			eUFOpcode = UFOP_DOT4;
			break;
		case GLSLTS_FLOAT:
		case GLSLTS_INT:
		case GLSLTS_BOOL:
			AddAluToUFCode(psUFContext, psDest, psSrcA, psSrcB, NULL, UFOP_MUL, 2, IMG_TRUE);
			return IMG_TRUE;
		default: 
			LOG_INTERNAL_ERROR(("ProcessICInstDOT: Source should be a vector for DOT\n"));
			break;
	}

	psUFInst = AddAluToUFCode(psUFContext, psDest, psSrcA, psSrcB, NULL, eUFOpcode, 2, IMG_FALSE);		if(!psUFInst) return IMG_FALSE;

	if(eUFOpcode == UFOP_DOT2ADD)
	{
		psUFInst->asSrc[2].eFormat = psUFInst->asSrc[0].eFormat;
	}
	
	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: ProcessICInstEXP

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_EXP

 PARAMETERS	: psUFContext		- UF context
			  psDest			- Destination operand
			  psSrcA			- First source operand.			

 RETURNS	: True if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstEXP(GLSLCompilerPrivateData *psCPD,
									GLSLUniFlexContext	*psUFContext,
								    ICUFOperand			*psDest,
								    ICUFOperand			*psSrcA)
{
	/*=======================================
	  	c = 1.0/log[2,e] = 0.69314718056 == ln(2) (constant value from Simmon Fenny)
		DIV	 temp  x c			// temp(genType) = x /ln(2) = x * log[2, e]
		EXP2 result temp		// result = exp2(temp) = exp2(x * log[2, e])
	=======================================*/

	GLSLTypeSpecifier eSrcAType = psSrcA->eTypeAfterSwiz;
	GLSLPrecisionQualifier ePrecision = psDest->sFullType.ePrecisionQualifier;

	ICUFOperand sConstant, sTemp;
	
	GetFixedConst(psCPD,	psUFContext, 0.69314718056f,	ePrecision, &sConstant);
	GetTemporary(psCPD,		psUFContext, eSrcAType,			ePrecision, &sTemp);

	/* DIV temp x c */
	AddAluToUFCode(psUFContext, &sTemp, psSrcA, &sConstant, IMG_NULL, UFOP_DIV, 2, IMG_TRUE);

	/* EXP2 result temp */
	AddAluToUFCode(psUFContext, psDest, &sTemp, IMG_NULL, IMG_NULL, UFOP_EXP, 1, IMG_TRUE);

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: ProcessICInstLOG

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_LOG

 PARAMETERS	: psUFContext		- UF context
			  psDest			- Destination operand
			  psSrcA			- First source operand.

 RETURNS	: True if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstLOG(GLSLCompilerPrivateData *psCPD,
									GLSLUniFlexContext	*psUFContext,
								    ICUFOperand			*psDest,
								    ICUFOperand			*psSrcA)
{
	/*=======================================
	  	c = 1.0/log[2,e] = 0.69314718056 == ln(2) (constant value from Simmon Fenny)
		LOG2 temp x				// temp(genType) = log[2, x]
		MUL result temp c		// result = temp * ln(2) = log[2, x] / log[2, e]
	=======================================*/

	GLSLTypeSpecifier eSrcAType = psSrcA->eTypeAfterSwiz;
	GLSLPrecisionQualifier ePrecision = psDest->sFullType.ePrecisionQualifier;

	ICUFOperand sConstant, sTemp;
	
	GetFixedConst(psCPD,	psUFContext, 0.69314718056f,	ePrecision, &sConstant);
	GetTemporary(psCPD,		psUFContext, eSrcAType,			ePrecision, &sTemp);

	/* LOG2 temp x */
	AddAluToUFCode(psUFContext, &sTemp, psSrcA, IMG_NULL, IMG_NULL, UFOP_LOG, 1, IMG_TRUE);

	/* MUL result temp c */
	AddAluToUFCode(psUFContext, psDest, &sTemp, &sConstant, IMG_NULL, UFOP_MUL, 2, IMG_TRUE);

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: ProcessICInstCROSS

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_CROSS

 PARAMETERS	: psUFContext		- UF context
			  psDest			- Destination operand
			  psSrcA			- First source operand.
			  psSrcB			- Second source operand.

 RETURNS	: True if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstCROSS(GLSLCompilerPrivateData *psCPD,
								   GLSLUniFlexContext *psUFContext,
								   ICUFOperand		  *psDest,
								   ICUFOperand		  *psSrcA,
								   ICUFOperand		  *psSrcB)
{
	GLSLTypeSpecifier eSrcAType = psSrcA->eTypeAfterSwiz;

	DebugAssert(eSrcAType == psSrcB->eTypeAfterSwiz);

	if(psDest->uCompStart || psDest->sICSwizMask.uNumComponents)
	{
		ICUFOperand sTemp;

		/* If the destination swiz is something like .yzw , we need to store the result in a temporary */
		if(!GetTemporary(psCPD, psUFContext, eSrcAType, psDest->sFullType.ePrecisionQualifier, &sTemp))
		{
			LOG_INTERNAL_ERROR(("ProcessICInstCROSS: Failed to get a temporary\n"));
			return IMG_FALSE;
		}
	
		AddAluToUFCode(psUFContext, &sTemp, psSrcA, psSrcB, NULL, UFOP_CRS, 2, IMG_FALSE);

		AddAluToUFCode(psUFContext, psDest, &sTemp, NULL, NULL, UFOP_MOV, 1, IMG_TRUE);
	}
	else
	{
		AddAluToUFCode(psUFContext, psDest, psSrcA, psSrcB, NULL, UFOP_CRS, 2, IMG_FALSE);
	}

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: ProcessICInstNRM3

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_NRM3

 PARAMETERS	: psUFContext		- UF context
			  psDest			- Destination operand
			  psSrcA			- First source operand.

 RETURNS	: True if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstNRM3(GLSLCompilerPrivateData *psCPD,
								   GLSLUniFlexContext *psUFContext,
								   ICUFOperand		  *psDest,
								   ICUFOperand		  *psSrcA)
{
	GLSLTypeSpecifier eSrcAType = psSrcA->eTypeAfterSwiz;

	if(psDest->uCompStart || psDest->sICSwizMask.uNumComponents)
	{
		ICUFOperand sTemp;

		/* If the destination swiz is something like .yzw , we need to store the result in a temporary */
		if(!GetTemporary(psCPD, psUFContext, eSrcAType, psDest->sFullType.ePrecisionQualifier, &sTemp))
		{
			LOG_INTERNAL_ERROR(("ProcessICInstCROSS: Failed to get a temporary\n"));
			return IMG_FALSE;
		}
	
		AddAluToUFCode(psUFContext, &sTemp, psSrcA, NULL, NULL, UFOP_NRM, 1, IMG_FALSE);

		AddAluToUFCode(psUFContext, psDest, &sTemp, NULL, NULL, UFOP_MOV, 1, IMG_TRUE);
	}
	else
	{
		AddAluToUFCode(psUFContext, psDest, psSrcA, NULL, NULL, UFOP_NRM, 1, IMG_FALSE);
	}

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: ProcessICInstSINCOS

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_SIN and GLSLIC_OP_COS

 PARAMETERS	: psUFContext		- UF context
			  psDest			- Destination operand
			  psSrcA			- First source operand.
			  bCos				- Is cosine or sine

 RETURNS	: True if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstSINCOS(GLSLCompilerPrivateData *psCPD,
									GLSLUniFlexContext	*psUFContext,
								    ICUFOperand			*psDest,
								    ICUFOperand			*psSrcA,
								    IMG_BOOL			bCos) 
{
	GLSLTypeSpecifier eSrcAType = psSrcA->eTypeAfterSwiz;
	GLSLPrecisionQualifier ePrecision = psDest->sFullType.ePrecisionQualifier;

	ICUFOperand sT, sHalfConst, sOneOverTwoPiConst, sTwoPiConst, sMinusPiConst;

	/*
		def c0, Pi, 0.5f, 2*Pi, 1/(2*Pi)
		mad r0.x, r.x, c0.w, c0.y
		frc r0.x, r0.x
		mad r0.x, r0.x, c0.z, -c0.x
	*/

	GetTemporary(psCPD, psUFContext,  eSrcAType,	ePrecision, &sT);
	GetFixedConst(psCPD, psUFContext, 0.5f,		ePrecision, &sHalfConst);
	GetFixedConst(psCPD, psUFContext, 1/(2*OGL_PI),ePrecision, &sOneOverTwoPiConst);
	GetFixedConst(psCPD, psUFContext, 2*OGL_PI,	ePrecision, &sTwoPiConst);
	GetFixedConst(psCPD, psUFContext, -OGL_PI,		ePrecision, &sMinusPiConst);

	AddAluToUFCode(psUFContext, &sT, psSrcA, &sOneOverTwoPiConst, &sHalfConst, UFOP_MAD, 3, IMG_TRUE);
	AddAluToUFCode(psUFContext, &sT, &sT, NULL, NULL, UFOP_FRC, 1, IMG_TRUE);
	AddAluToUFCode(psUFContext, &sT, &sT, &sTwoPiConst, &sMinusPiConst, UFOP_MAD, 3, IMG_TRUE);

	AddSINCOSInstToUFCode(psCPD, psUFContext, psDest, &sT, bCos);


	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: ProcessICInstPOW

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_POW

 PARAMETERS	: psUFContext		- UF context
			  psDest			- Destination operand
			  psSrcA			- First source operand.
			  psSrcB			- Second source operand.

 RETURNS	: True if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstPOW(GLSLCompilerPrivateData *psCPD,
								 GLSLUniFlexContext *psUFContext,
								 ICUFOperand		*psDest,
								 ICUFOperand		*psSrcA,
								 ICUFOperand		*psSrcB)
{
	PVR_UNREFERENCED_PARAMETER(psCPD);

	AddAluToUFCode(psUFContext, psDest, psSrcA, psSrcB, IMG_NULL, UFOP_POW, 2, IMG_FALSE);

	return IMG_TRUE;
}



/*****************************************************************************
 FUNCTION	: ProcessICInstMOD

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_MOD

 PARAMETERS	: psUFContext		- UF context
			  psDest			- Destination operand
			  psSrcA			- First source operand.
			  psSrcB			- Second source operand. 

 RETURNS	: True if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstMOD(GLSLCompilerPrivateData *psCPD,
								 GLSLUniFlexContext *psUFContext,
								 ICUFOperand		*psDest,
								 ICUFOperand		*psSrcA,
								 ICUFOperand		*psSrcB)
{
	GLSLTypeSpecifier eDestType = psDest->eTypeAfterSwiz;
	GLSLTypeSpecifier eSrcBType = psSrcB->eTypeAfterSwiz;
	ICUFOperand sT1, sT2, sT3, sT4;

	/*
		MOD Dest SrcA SrcB
		==>
		RCP t1 SrcB
		MUL t2 SrcA t1

		// Was MOD t2 t2, now removed from USC. MOD(x) = x + TRC(-x).
		TRC	t3 -t2
		ADD t4 t2 t3

		MUL Dest t4 SrcB 
	*/

	GetTemporary(psCPD, psUFContext, eSrcBType, psDest->sFullType.ePrecisionQualifier, &sT1);
	GetTemporary(psCPD, psUFContext, eDestType, psDest->sFullType.ePrecisionQualifier, &sT2);
	GetTemporary(psCPD, psUFContext, eDestType, psDest->sFullType.ePrecisionQualifier, &sT3);
	GetTemporary(psCPD, psUFContext, eDestType, psDest->sFullType.ePrecisionQualifier, &sT4);


	AddAluToUFCode(psUFContext, &sT1, psSrcB, IMG_NULL, IMG_NULL, UFOP_RECIP, 1, IMG_FALSE);

	AddAluToUFCode(psUFContext, &sT2, psSrcA, &sT1, NULL, UFOP_MUL, 2, IMG_TRUE);

	/*
		UFOP_MOD Removed. Can be emulated with MOD(x) = x + TRC(-x). TRC = truncate

		TRC t3, -t2
		ADD t4, t2, t3
	*/
	sT2.eICModifier |= GLSLIC_MODIFIER_NEGATE;
	AddAluToUFCode(psUFContext, &sT3, &sT2, NULL, NULL, UFOP_TRC, 1, IMG_TRUE);

	sT2.eICModifier &= ~GLSLIC_MODIFIER_NEGATE;
	AddAluToUFCode(psUFContext, &sT4, &sT2, &sT3, NULL, UFOP_ADD, 2, IMG_TRUE);

	AddAluToUFCode(psUFContext, psDest, &sT4, psSrcB, NULL, UFOP_MUL, 2, IMG_TRUE);

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: ProcessICInstTAN

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_TAN

 PARAMETERS	: psUFContext		- UF context
			  psDest			- Destination operand
			  psSrcA			- First source operand.

 RETURNS	: True if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstTAN(GLSLCompilerPrivateData *psCPD,
								 GLSLUniFlexContext *psUFContext,
								 ICUFOperand		*psDest,
								 ICUFOperand		*psSrc)
{
	GLSLTypeSpecifier eDestType = psDest->eTypeAfterSwiz;
	GLSLPrecisionQualifier ePrecision = psDest->sFullType.ePrecisionQualifier;

	ICUFOperand sHalfConst, sOneOverTwoPiConst, sTwoPiConst, sMinusPiConst;
	ICUFOperand sR0, sTR0, sR1, sTR1X, sTR1Y;
	ICUFOperand sTDest;
	IMG_UINT32 i, uNumComponents = TYPESPECIFIER_NUM_COMPONENTS(eDestType);

	sTDest = *psDest;

	/*
		input src, output dest

		def c0, Pi, 0.5f, 2*Pi, 1/(2*Pi)

		mad r0.x, src.x(y,z,w), 1/(2*Pi), 0.5f
		frc r0.x, r0.x
		mad r0.x, r0.x, 2*Pi, -Pi

		for(i = 0; i < components; i++)
		{
			sincos r1.xy r0.x(y,z,w)
			rcp r1.x r1.x
			mul dest r1.x r1.y
		}

	*/

	GetTemporary(psCPD, psUFContext,  eDestType,	ePrecision, &sR0);
	GetFixedConst(psCPD, psUFContext, 0.5f,		ePrecision, &sHalfConst);
	GetFixedConst(psCPD, psUFContext, 1/(2*OGL_PI),ePrecision, &sOneOverTwoPiConst);
	GetFixedConst(psCPD, psUFContext, 2*OGL_PI,	ePrecision, &sTwoPiConst);
	GetFixedConst(psCPD, psUFContext, -OGL_PI,		ePrecision, &sMinusPiConst);

	/*
		Input for SINCOS has to be in the range of -PI to PI,
		we use three instructions to modify the source
	*/
	AddAluToUFCode(psUFContext, &sR0, psSrc, &sOneOverTwoPiConst, &sHalfConst, UFOP_MAD, 3, IMG_TRUE);
	AddAluToUFCode(psUFContext, &sR0, &sR0, NULL, NULL, UFOP_FRC, 1, IMG_TRUE);
	AddAluToUFCode(psUFContext, &sR0, &sR0, &sTwoPiConst, &sMinusPiConst, UFOP_MAD, 3, IMG_TRUE);

	/* SINCOS instruction return both sin (in y) and cos (in x), we store it in a temporary vector */ 
	GetTemporary(psCPD, psUFContext, GLSLTS_VEC4, psDest->sFullType.ePrecisionQualifier, &sR1);
	ApplyMoreSwiz(psCPD, &sR1, SWIZ_XY);

	sTR0  = sR0;
	sTR1X = sR1;
	sTR1Y = sR1;


	for(i = 0; i < uNumComponents; i++)
	{
		/* SINCOS r1.xy r0.x(y,z,w) */
		ChooseVectorComponent(psCPD, &sR0, &sTR0, (GLSLICVecComponent)i);
		AddAluToUFCode(psUFContext, &sR1, &sTR0, IMG_NULL, IMG_NULL, UFOP_SINCOS, 1, IMG_FALSE);

		/* RCP r1.x r1.x */
		ChooseVectorComponent(psCPD, &sR1, &sTR1X, GLSLIC_VECCOMP_X);
		AddAluToUFCode(psUFContext, &sTR1X, &sTR1X, NULL, NULL, UFOP_RECIP, 1, IMG_TRUE);

		/* MUL Dest.x(y,z) t.x t.y */
		ChooseVectorComponent(psCPD, &sR1, &sTR1Y, GLSLIC_VECCOMP_Y);
		ChooseVectorComponent(psCPD, psDest, &sTDest, (GLSLICVecComponent)i);
		AddAluToUFCode(psUFContext, &sTDest, &sTR1X, &sTR1Y, NULL, UFOP_MUL, 2, IMG_TRUE);
	}

	return IMG_TRUE;
}

extern IMG_UINT32 auDimSwiz[4];

/*****************************************************************************
 FUNCTION	: ProcessVPInstTEXLDP

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_TEXLDP in vertex program

 PARAMETERS	: psUFContext		- UF context
			  psDest			- Destination operand	- look result, vec4
			  psSrcA			- First source operand	- sampler
			  psSrcB			- Second source operand - texture coord, vec4

 RETURNS	: True if successful
*****************************************************************************/
static IMG_BOOL ProcessVPInstTEXLDPFn(GLSLCompilerPrivateData *psCPD,
									GLSLUniFlexContext	*psUFContext,
									ICUFOperand			*psDest,
									ICUFOperand			*psSrcA,
									ICUFOperand			*psSrcB
									)
{
	IMG_BOOL bSuccess;

	ICUFOperand sTempCoord, sTemp, sTempSrcB1 = *psSrcB, sTempSrcB2 = *psSrcB;
	IMG_UINT32 uNumComponents = TYPESPECIFIER_NUM_COMPONENTS(psSrcB->eTypeAfterSwiz);
	IMG_UINT32 uTextureDim = TYPESPECIFIER_DIMS(psSrcA->sFullType.eTypeSpecifier);
	IMG_UINT32 auLastSwiz[] = {0, SWIZ_X, SWIZ_Y, SWIZ_Z, SWIZ_W};

	/* Get a temp, precision is same as texcoord */
	GetTemporary(psCPD, psUFContext, GLSLTS_VEC4, psSrcB->sFullType.ePrecisionQualifier, &sTempCoord);

	/* Preform division first and store the result in a temp */

	/* temp = SrcB/SrcB.w */
	sTemp = sTempCoord;
	ApplyMoreSwiz(psCPD, &sTemp, auDimSwiz[uTextureDim]);
	ApplyMoreSwiz(psCPD, &sTempSrcB1, auDimSwiz[uTextureDim]);
	ApplyMoreSwiz(psCPD, &sTempSrcB2, auLastSwiz[uNumComponents]);
	ProcessICInstDIV(psCPD, psUFContext, &sTemp, &sTempSrcB1, &sTempSrcB2);

	/* LDL Dest SrcA temp */
	bSuccess = AddDirectTextureLookupToUFCode(psCPD, psUFContext, psDest, psSrcA, &sTempCoord, NULL, psSrcOffset, UFOP_LDL);

	return bSuccess;
}

/*****************************************************************************
 FUNCTION	: ProcessFPInstTEXLDP

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_TEXLDP in fragment program

 PARAMETERS	: psUFContext		- UF context
			  psDest			- Destination operand	- look result, vec4
			  psSrcA			- First source operand	- sampler
			  psSrcB			- Second source operand - texture coord, vec4

 RETURNS	: True if successful
*****************************************************************************/
static IMG_BOOL ProcessFPInstTEXLDPFn(GLSLCompilerPrivateData *psCPD,
									GLSLUniFlexContext	*psUFContext,
									ICUFOperand			*psDest,
									ICUFOperand			*psSrcA,
									ICUFOperand			*psSrcB
									)
{
	IMG_BOOL bSuccess;

	ICUFOperand sTSrcB, *psTSrcB = psSrcB;
	IMG_UINT32 uNumComponents = TYPESPECIFIER_NUM_COMPONENTS(psSrcB->eTypeAfterSwiz);
	IMG_UINT32 uTextureDim = TYPESPECIFIER_DIMS(psSrcA->sFullType.eTypeSpecifier);
	GLSLICVecSwizWMask sSwizMask;

	if(psSrcB->eTypeAfterSwiz != GLSLTS_VEC4)
	{
		/* If SrcB is not vec4, we need to make sure the last componet of B to be w component */
		IMG_UINT32 i;

		sTSrcB = *psSrcB;
		psTSrcB = &sTSrcB;

		sSwizMask.uNumComponents = 4;
		for(i = 0; i < uTextureDim; i++)
		{
			sSwizMask.aeVecComponent[i] = (GLSLICVecComponent)(GLSLIC_VECCOMP_X + i);
		}
		for(; i < 3; i++)
		{
			sSwizMask.aeVecComponent[i] = GLSLIC_VECCOMP_X;
		}

		/* w component should be the last component of SrcB */
		sSwizMask.aeVecComponent[3] = (GLSLICVecComponent)(GLSLIC_VECCOMP_X + uNumComponents - 1);

		/* Combine two swizzles */
		ICCombineTwoConstantSwizzles(&sTSrcB.sICSwizMask, &sSwizMask);

		/* Update operand swiz */
		UpdateOperandUFSwiz(psCPD, &sTSrcB);
	}

	/* Actual texture lookup */
	bSuccess = AddDirectTextureLookupToUFCode(psCPD, psUFContext, psDest, psSrcA, psTSrcB, NULL, psSrcOffset, UFOP_LDP);

	return bSuccess;
}

/*****************************************************************************
 FUNCTION	: ProcessICInstTEXLDDFn

 PURPOSE	: Generate UF code for IC instruction GLSLIC_OP_TEXLDB and GLSLIC_OP_TEXLDL

 PARAMETERS	: psUFContext		- UF context
			  psDest			- Destination operand	- look result, vec4
			  psSrcA			- First source operand	- sampler
			  psSrcB			- Second source operand - texture coord, vec4
			  psSrcC			- Third souce operand	- bias or lod, float

 RETURNS	: True if successful
*****************************************************************************/
static IMG_BOOL ProcessICInstTEXLDDFn(GLSLCompilerPrivateData *psCPD,
									GLSLUniFlexContext	*psUFContext,
									ICUFOperand			*psDest,
									ICUFOperand			*psSampler,
									ICUFOperand			*psCoord,
									ICUFOperand			*psDPdx,
									ICUFOperand			*psDPdy
									)
{
	ICUFOperand sTResult, *psTDest = psDest;
	IMG_BOOL bHasDestSwiz = IMG_FALSE;
	PUNIFLEX_INST psProg;

	DebugAssert(psSampler->eRegType == UFREG_TYPE_TEX);

	/* If the destination is not the default swiz, we need a temporary to store the lookup result */
	if(psDest->uCompStart || psDest->sICSwizMask.uNumComponents)
	{
		bHasDestSwiz = IMG_TRUE;

		GetTemporary(psCPD, psUFContext, GLSLTS_VEC4, psDest->sFullType.ePrecisionQualifier, &sTResult);

		psTDest = &sTResult;
	}

	/* LDD coord sampler dPdx dPdy */
	psProg = CreateInstruction(psUFContext, UFOP_LDD);		if(!psProg) return IMG_FALSE;

	ConvertToUFDestination(&psProg->sDest, psTDest, NULL);

	ConvertToUFSource(&psProg->asSrc[0], psCoord, IMG_FALSE, 0, 0);

	ConvertSamplerToUFSource(&psProg->asSrc[1], psSampler);

	ConvertToUFSource(&psProg->asSrc[2], psDPdx, IMG_FALSE, 0, 0);

	ConvertToUFSource(&psProg->asSrc[3], psDPdy, IMG_FALSE, 0, 0);


	/* Move temp to the destination */
	if(bHasDestSwiz)
	{
		AddAluToUFCode(psUFContext, psDest, psTDest, IMG_NULL, IMG_NULL, UFOP_MOV, 2, IMG_TRUE);
	}

	return IMG_TRUE;
}

#ifdef DEBUG
static IMG_BOOL CheckInstrPrecisionMismatch(PUNIFLEX_INST psUFInstr)
{
	UF_REGFORMAT eRegFormat;
	IMG_UINT32 i;

	switch(psUFInstr->eOpCode)
	{
		case UFOP_MOV:
#ifndef GLSL_NATIVE_INTEGERS
			if(psUFInstr->sDest.eFormat > UF_REGFORMAT_C10 || psUFInstr->asSrc[0].eFormat > UF_REGFORMAT_C10)
			{
				return IMG_TRUE;
			}
#endif
			return IMG_FALSE;
#ifdef GLSL_NATIVE_INTEGERS
		case UFOP_MOVCBIT:
			return IMG_FALSE;	/* UFOP_MOVCBIT is used to convert bools to ints and floats. */
#endif
		case UFOP_MOVA_TRC:
		case UFOP_MOVA_RNE:
#ifndef GLSL_NATIVE_INTEGERS
			if(psUFInstr->asSrc[0].eFormat > UF_REGFORMAT_C10)
			{
				return IMG_TRUE;
			}
#endif
			break;
		case UFOP_LD: /* Intentional fall-through */
		case UFOP_LDB:
		case UFOP_LDL:
		case UFOP_LDP:
		case UFOP_LDPIFTC:
		case UFOP_LDD:
			/* Any combination of precisions should be handled correctly by USC */
			return IMG_FALSE;
		default:
		{
			PCINPUT_INST_DESC psDesc = PVRUniFlexGetInputInstDesc(psUFInstr->eOpCode);

			if( psDesc->uNumDests && 
				psUFInstr->sDest.eType != UFREG_TYPE_PREDICATE &&
				!IS_TEXTURE_LOOKUP_OP(psUFInstr->eOpCode) 
			  )
			{
				eRegFormat = psUFInstr->sDest.eFormat;
				i = 0;
			}
			else
			{
				eRegFormat = psUFInstr->asSrc[0].eFormat;
		
				i = 1;
			}
		
#ifndef GLSL_NATIVE_INTEGERS
			if(eRegFormat > UF_REGFORMAT_C10)
			{
				return IMG_TRUE;
			}
#endif

			for(; i < psDesc->uNumSrcArgs; i++)
			{
				if( psUFInstr->asSrc[i].eType == UFREG_TYPE_CLIPPLANE ||
					psUFInstr->asSrc[i].eType == UFREG_TYPE_COMPOP)
				{
					/* Ignore the precision for UFREG_TYPE_CLIPPLANE and UFREG_TYPE_COMPOP */
					continue;
				}
				else if(psUFInstr->asSrc[i].eType == UFREG_TYPE_TEX)
				{
					/* The sampler precision should be the same as the destination */
					if(psUFInstr->asSrc[1].eFormat == psUFInstr->sDest.eFormat)
					{
						continue;
					}
					
					/* Destination precision is different from sampler */
					return IMG_TRUE;

				}

				if(psUFInstr->asSrc[i].eFormat != eRegFormat)
				{
					return IMG_TRUE;
				}
			}
		}
	}

	return IMG_FALSE;
}

static IMG_BOOL CheckProgramPrecisionMismatch(GLSLUniFlexContext *psUFContext)
{
	PUNIFLEX_INST psUFInstr = psUFContext->psFirstUFInst;
	while(psUFInstr)
	{
		if(CheckInstrPrecisionMismatch(psUFInstr))
		{
			return IMG_TRUE;
		}

		psUFInstr = psUFInstr->psILink;
	}

	return IMG_FALSE;
}
#endif

#if defined(DEBUG) && defined(DUMP_LOGFILES)

static IMG_VOID DumpMultipleUFInsts(GLSLCompilerPrivateData *psCPD, PUNIFLEX_INST psFirstUFInst, PUNIFLEX_INST psLastUFInst)
{
	PUNIFLEX_INST psUFInst = psFirstUFInst;

	IMG_UINT32 uIgnored;

	if (psUFInst != NULL)
	{
		for (;;)
		{
			IC2UF_PrintInputInst(NULL, psUFInst, 0, &uIgnored);

			if(CheckInstrPrecisionMismatch(psUFInst))
			{
				DumpLogMessage(LOGFILE_COMPILER, 0, " --- Precision mismatch");
				LOG_INTERNAL_ERROR(("GenerateUniFlexInstructions: Precision mismatch\n"));
			}
			DumpLogMessage(LOGFILE_COMPILER, 0, "\n");

			if (psUFInst == psLastUFInst)
			{
				break;
			}

			psUFInst = psUFInst->psILink;
		}
	}

	DumpLogMessage(LOGFILE_COMPILER, 0, "\n");
}
#endif

/*****************************************************************************
 FUNCTION	: GenerateUniFlexInstructions

 PURPOSE	: Generate uniflex instructions for an IC Program

 PARAMETERS	: psUFContext		- UF context

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_BOOL GenerateUniFlexInstructions(GLSLCompilerPrivateData *psCPD, GLSLUniFlexContext *psUFContext)
{
	IMG_UINT32 uInst, i;
	GLSLICInstruction *psCurrentInst;
	IMG_BOOL bSuccess = IMG_TRUE;

#if defined(DEBUG) && defined(DUMP_LOGFILES)
	PUNIFLEX_INST psLastDumpedInst = psUFContext->psEndUFInst;
#endif

	/*
		Generate UniFlex code for each instruction in the intermediate format program.
	*/
	for (uInst = 0, psCurrentInst = psUFContext->psICProgram->psInstrHead; 
		 psCurrentInst != NULL; 
		 uInst++, psCurrentInst = psCurrentInst->psNext)
	{
		GLSLICOperand *psSrcA = &psCurrentInst->asOperand[SRCA];
		ICUFOperand asICUFOperand[MAX_OPRDS];
		ICUFOperand sUFPredicate, *psUFPredicate = IMG_NULL;

		psUFContext->psCurrentICInst = psCurrentInst;

		#ifdef SRC_DEBUG
		psUFContext->uCurSrcLine = psCurrentInst->uSrcLine;
		#endif /* SRC_DEBUG */

		if(psCurrentInst->eOpCode != GLSLIC_OP_CALL &&
			psCurrentInst->eOpCode != GLSLIC_OP_LABEL)
		{
			/* We supply an initial address relative index to be used for each dest/source operand.
			   ProcessICOperand calls ProcessOperandOffsets which will process the operand's offset,
			   and if it doesnt use an offset then it will set the operand to UFREG_RELATIVEINDEX_NONE.
			   If the operand does use an offset, then we will increment the initial address index, so 
			   that the next operand will use a different one. */
			UFREG_RELATIVEINDEX uInitialRelativeIndex = UFREG_RELATIVEINDEX_A0X;

			for(i = 0; i < ICOP_NUM_SRCS(psCurrentInst->eOpCode) + 1; i++)
			{
				UFREG_RELATIVEINDEX eRelativeIndex;

				if(!ICOP_HAS_DEST(psCurrentInst->eOpCode) && i == DEST) continue;

				eRelativeIndex = uInitialRelativeIndex;
					
				if(!ProcessICOperand(psCPD, psUFContext, &psCurrentInst->asOperand[i], &asICUFOperand[i], eRelativeIndex, IMG_FALSE))
				{
					LOG_INTERNAL_ERROR(("GenerateUniFlexInstructions: Failed to process IC operand\n"));
					LOG_INTERNAL_ERROR(("IC instruction number = %u, source = %u\n", uInst, i));
					return IMG_FALSE;
				}

				if(asICUFOperand[i].eRelativeIndex != UFREG_RELATIVEINDEX_NONE)
				{
					/* The operand used a relative index, so use the next index for the next operand. */
					uInitialRelativeIndex++;
				}
			}
		}

		#ifdef SRC_DEBUG
		psUFContext->uCurSrcLine = psCurrentInst->uSrcLine;
		#endif /* SRC_DEBUG */

		if(psCurrentInst->uPredicateBoolSymID)
		{
			if(psCurrentInst->eOpCode == GLSLIC_OP_LOOP || 
			   psCurrentInst->eOpCode == GLSLIC_OP_STATICLOOP || 
			   psCurrentInst->eOpCode == GLSLIC_OP_ENDLOOP ||
			   psCurrentInst->eOpCode == GLSLIC_OP_SEQ ||
			   psCurrentInst->eOpCode == GLSLIC_OP_SNE ||
			   psCurrentInst->eOpCode == GLSLIC_OP_SLT ||
			   psCurrentInst->eOpCode == GLSLIC_OP_SLE ||
			   psCurrentInst->eOpCode == GLSLIC_OP_SGT ||
			   psCurrentInst->eOpCode == GLSLIC_OP_SGE ) 
			{
		 		InitICUFOperand(psCPD, psUFContext, psCurrentInst->uPredicateBoolSymID, &sUFPredicate, IMG_FALSE);
				sUFPredicate.eICModifier |= (psCurrentInst->bPredicateNegate ? GLSLIC_MODIFIER_NEGATE : 0);

				psUFPredicate = &sUFPredicate;
			}
			else
			{
				LOG_INTERNAL_ERROR(("GenerateUniFlexInstructions: IC instruction %s doesn't support predicate", ICOP_DESC(psCurrentInst->eOpCode)));
			}
		}

#define psUFDest	(&asICUFOperand[DEST])
#define psUFSrcA	(&asICUFOperand[SRCA])
#define psUFSrcB	(&asICUFOperand[SRCB])
#define psUFSrcC	(&asICUFOperand[SRCC])
#define psUFSrcD	(&asICUFOperand[SRCD])
#define psUFSrcE	(&asICUFOperand[SRCE])

		/* Assumming it will be successful */
		bSuccess = IMG_TRUE; 

		#ifdef SRC_DEBUG
		psUFContext->uCurSrcLine = psCurrentInst->uSrcLine;
		#endif /* SRC_DEBUG */

		switch (psCurrentInst->eOpCode)
		{
			case GLSLIC_OP_MOV:		/* Dst, Src: Dst and Src can be of different types, e.g. float/int/bool */
			{
				bSuccess = ProcessICInstMOV(psCPD, psUFContext, psUFDest, psUFSrcA);

				break;
			}

			case GLSLIC_OP_ADD:		/* Dst = SrcA + SrcB */
			{
				bSuccess = AddSimpleALUToUFCode(psCPD, psUFContext, psUFDest, psUFSrcA, psUFSrcB, IMG_NULL, UFOP_ADD, 2);

				break;
			}
			case GLSLIC_OP_SUB:		/* Dst = SrcA - SrcB */
			{
				bSuccess = AddSimpleALUToUFCode(psCPD, psUFContext, psUFDest, psUFSrcA, psUFSrcB, IMG_NULL, UFOP_SUB, 2);

				break;
			}
			case GLSLIC_OP_MAD:		/* Dst = SrcA * SrcB + SrcC */
			{
				bSuccess = AddSimpleALUToUFCode(psCPD, psUFContext, psUFDest, psUFSrcA, psUFSrcB, psUFSrcC, UFOP_MAD, 3);

				break;
			}
			case GLSLIC_OP_MIX:		/* Dst = SrcA * (1 - SrcC) + SrcB * SrcC */
			{
				bSuccess = AddSimpleALUToUFCode(psCPD, psUFContext, psUFDest, psUFSrcB, psUFSrcA, psUFSrcC, UFOP_LRP, 3);

				break;
			}
			case GLSLIC_OP_MUL:		/* Dst = SrcA * SrcB */
			{
				bSuccess = ProcessICInstMUL(psCPD, psUFContext, psUFDest, psUFSrcA, psUFSrcB);

				break;
			}
			case GLSLIC_OP_DIV:		/* Dst, SrcA, SrcB */
			{
				bSuccess = ProcessICInstDIV(psCPD, psUFContext, psUFDest, psUFSrcA, psUFSrcB);
				break;
			}

			/* test and comparison ops */
			case GLSLIC_OP_SEQ:		/* Dst, SrcA, SrcB:  Dst.m = (SrcA.m == SrcB.m)? T : F; */
			{
				bSuccess = ProcessICInstSEQ(psCPD, psUFContext, psUFDest, psUFSrcA, psUFSrcB, psUFPredicate);
				break;
			}
			case GLSLIC_OP_SNE:		/* Dst, SrcA, SrcB:  Dst.m = (SrcA.m != SrcB.m)? T : F; */
			{
				bSuccess = ProcessICInstSNE(psCPD, psUFContext, psUFDest, psUFSrcA, psUFSrcB, psUFPredicate);
				break;
			}
			case GLSLIC_OP_SLT:		/* Dst, SrcA, SrcB:  Dst.m = (SrcA.m < SrcB.m)? T : F; */
			{
				AddComparisonToUFCode(psCPD, psUFContext, psUFDest, psUFSrcA, psUFSrcB, UFREG_COMPOP_LT, psUFPredicate);
				break;
			}
			case GLSLIC_OP_SLE:		/* Dst, SrcA, SrcB:  Dst.m = (SrcA.m <= SrcB.m)? T : F; */
			{
				AddComparisonToUFCode(psCPD, psUFContext, psUFDest, psUFSrcA, psUFSrcB, UFREG_COMPOP_LE, psUFPredicate);
				break;
			}
			case GLSLIC_OP_SGT:		/* Dst, SrcA, SrcB:  Dst.m = (SrcA.m > SrcB.m)? T : F; */
			{
				AddComparisonToUFCode(psCPD, psUFContext, psUFDest, psUFSrcA, psUFSrcB, UFREG_COMPOP_GT, psUFPredicate);
				break;
			}
			case GLSLIC_OP_SGE:		/* Dst, SrcA, SrcB:  Dst.m = (SrcA.m >= SrcB.m)? T : F; */
			{
				AddComparisonToUFCode(psCPD, psUFContext, psUFDest, psUFSrcA, psUFSrcB, UFREG_COMPOP_GE, psUFPredicate);
				break;
			}

			case GLSLIC_OP_NOT:
			{
				bSuccess = AddNOTToUFCode(psUFContext, psUFDest, psUFSrcA);

				break;
			}

			/* conditional execution ops */
			case GLSLIC_OP_IF:		/* executes IF...ELSE/ENDIF block if SrcA == T */
			{
				bSuccess = ProcessICInstIF(psUFContext, psUFSrcA);
				break;
			}
			case GLSLIC_OP_IFNOT:
			{
				bSuccess = ProcessICInstIFNOT(psUFContext, psUFSrcA);
				break;
			}

			case GLSLIC_OP_IFEQ:		/* Dst, SrcA, SrcB:  Dst.m = (SrcA.m == SrcB.m)? T : F; */
			{
				bSuccess = AddIFComparisonToUFCode(psCPD, psUFContext, psUFSrcA, psUFSrcB, UFREG_COMPOP_EQ);

				break;
			}
			case GLSLIC_OP_IFNE:		/* Dst, SrcA, SrcB:  Dst.m = (SrcA.m != SrcB.m)? T : F; */
			{
				bSuccess = AddIFComparisonToUFCode(psCPD, psUFContext, psUFSrcA, psUFSrcB, UFREG_COMPOP_NE);

				break;
			}
			case GLSLIC_OP_IFLT:		/* Dst, SrcA, SrcB:  Dst.m = (SrcA.m < SrcB.m)? T : F; */
			{
				bSuccess = AddIFComparisonToUFCode(psCPD, psUFContext, psUFSrcA, psUFSrcB, UFREG_COMPOP_LT);
				break;
			}
			case GLSLIC_OP_IFLE:		/* Dst, SrcA, SrcB:  Dst.m = (SrcA.m <= SrcB.m)? T : F; */
			{
				bSuccess = AddIFComparisonToUFCode(psCPD, psUFContext, psUFSrcA, psUFSrcB, UFREG_COMPOP_LE);
				break;
			}
			case GLSLIC_OP_IFGT:		/* Dst, SrcA, SrcB:  Dst.m = (SrcA.m > SrcB.m)? T : F; */
			{
				bSuccess = AddIFComparisonToUFCode(psCPD, psUFContext, psUFSrcA, psUFSrcB, UFREG_COMPOP_GT);
				break;
			}
			case GLSLIC_OP_IFGE:		/* Dst, SrcA, SrcB:  Dst.m = (SrcA.m >= SrcB.m)? T : F; */
			{
				bSuccess = AddIFComparisonToUFCode(psCPD, psUFContext, psUFSrcA, psUFSrcB, UFREG_COMPOP_GE);
				break;
			}
			
			case GLSLIC_OP_ELSE:
			{
				CreateInstruction(psUFContext, UFOP_ELSE);
				break;
			}
			case GLSLIC_OP_ENDIF:
			{
				bSuccess = ProcessICInstENDIF(psUFContext);
				break;
			}

			/* procedure call/return ops */
			case GLSLIC_OP_LABEL:	/* Label/Subroutine name */
			{
				bSuccess = ProcessICInstLABEL(psCPD, psUFContext, psSrcA);

				break;
			}
			case GLSLIC_OP_RET:
			{
				bSuccess = ProcessICInstRET(psCPD, psUFContext);

				break;
			}
			case GLSLIC_OP_CALL:		/* Label */
			{
				bSuccess = ProcessICInstCALL(psCPD, psUFContext, psSrcA);
				break;
			}

			/* loop ops */
			case GLSLIC_OP_LOOP:		/* Start loop */
			{
				bSuccess = ProcessICInstLOOP(psCPD, psUFContext, psUFPredicate);

				break;
			}

			case GLSLIC_OP_STATICLOOP:		/* Start static loop */
			{
				bSuccess = ProcessICInstSTATICLOOP(psCPD, psUFContext, psUFPredicate);

				break;
			}

			case GLSLIC_OP_ENDLOOP:	/* end unconditional (infinite) loop */
			{
				bSuccess = ProcessICInstENDLOOP(psUFContext, psUFPredicate);

				break;
			}

			case GLSLIC_OP_CONTDEST:	/* Specifies where execution resumes after a CONTINUE */
			{
				bSuccess = ProcessICInstCONTDEST(psUFContext);

				break;
			}

			case GLSLIC_OP_CONTINUE:	/* Must be followed by a CONTDEST before ENDLOOP */
			{
				bSuccess = ProcessICInstCONTINUE(psUFContext);

				break;
			}

			case GLSLIC_OP_BREAK:
			{
				CreateInstruction(psUFContext, UFOP_BREAK);

				break;
			}

			case GLSLIC_OP_DISCARD:
			{
				bSuccess = ProcessICInstDISCARD(psUFContext);

				break;
			}

			case GLSLIC_OP_SANY:		/* Dst, SrcA: Dst must be bool, SrcA must be bool. Vector types acepted*/
			{
				bSuccess = ProcessICInstSANY(psCPD, psUFContext, psUFDest, psUFSrcA);

				break;
			}
			case GLSLIC_OP_SALL:		/* Dst, SrcA: Dst must be bool, SrcA must be bool. Vector types acepted */
			{
				bSuccess = ProcessICInstSALL(psCPD, psUFContext, psUFDest, psUFSrcA);

				break;
			}

			case GLSLIC_OP_ABS:		/* Dst, Src */
			{
				AddSimpleALUToUFCode(psCPD, psUFContext, psUFDest, psUFSrcA, NULL, NULL, UFOP_ABS, 1);

				break;
			}
			case GLSLIC_OP_MIN:		/* Dst, SrcA, SrcB: performs Dst.m = (SrcA.m < SrcB.m)? SrcA.m : SrcB.m */
			{
				AddSimpleALUToUFCode(psCPD, psUFContext, psUFDest, psUFSrcA, psUFSrcB, NULL, UFOP_MIN, 2);

				break;
			}
			case GLSLIC_OP_MAX:		/* Dst, SrcA, SrcB: performs Dst.m = (SrcA.m > SrcB.m)? SrcA.m : SrcB.m */
			{
				AddSimpleALUToUFCode(psCPD, psUFContext, psUFDest, psUFSrcA, psUFSrcB, NULL, UFOP_MAX, 2);

				break;
			}
			case GLSLIC_OP_SGN:		/* Dst, SrcA: Dst.m = (SrcA < 0)? -1.0 : (SrcA == 0)? 0.0 : 1.0; */
			{
				bSuccess = ProcessICInstSGN(psCPD, psUFContext, psUFDest, psUFSrcA);

				break;
			}

			case GLSLIC_OP_RCP:		/* Dst, Src */
			{
				AddAluToUFCode(psUFContext, psUFDest, psUFSrcA, IMG_NULL, IMG_NULL, UFOP_RECIP, 1, IMG_FALSE);
				break;
			}
			case GLSLIC_OP_RSQ:		/* Dst, Src */
			{
				AddAluToUFCode(psUFContext, psUFDest, psUFSrcA, IMG_NULL, IMG_NULL, UFOP_RSQRT, 1, IMG_FALSE);
				break;
			}
			case GLSLIC_OP_LOG:			/* Dst, Src */
			{
				bSuccess = ProcessICInstLOG(psCPD, psUFContext, psUFDest, psUFSrcA);
				break;
			}
			case GLSLIC_OP_LOG2:		/* Dst, Src */
			{
				AddAluToUFCode(psUFContext, psUFDest, psUFSrcA, IMG_NULL, IMG_NULL, UFOP_LOG, 1, IMG_FALSE);
				break;
			}
			case GLSLIC_OP_EXP:			/* Dst, Src */
			{
				bSuccess = ProcessICInstEXP(psCPD, psUFContext, psUFDest, psUFSrcA);
				break;
			}
			case GLSLIC_OP_EXP2:		/* Dst, Src */
			{
				AddAluToUFCode(psUFContext, psUFDest, psUFSrcA, IMG_NULL, IMG_NULL, UFOP_EXP, 1, IMG_TRUE);
				break;
			}

			case GLSLIC_OP_DOT:		/* Dst, SrcA, SrcB */
			{
				bSuccess = ProcessICInstDOT(psCPD, psUFContext, psUFDest, psUFSrcA, psUFSrcB);

				break;
			}
			case GLSLIC_OP_CROSS:	/* Dst, SrcA, SrcB */
			{
				bSuccess = ProcessICInstCROSS(psCPD, psUFContext, psUFDest, psUFSrcA, psUFSrcB);
				break;
			}
			case GLSLIC_OP_NRM3:	/* Dst, SrcA */
			{
				bSuccess = ProcessICInstNRM3(psCPD, psUFContext, psUFDest, psUFSrcA);
				break;
			}

			case GLSLIC_OP_SIN:		/* Dst, SrcA */
			case GLSLIC_OP_COS:
			{
				IMG_BOOL bCos =  (psCurrentInst->eOpCode == GLSLIC_OP_COS) ? IMG_TRUE : IMG_FALSE;

				bSuccess = ProcessICInstSINCOS(psCPD, psUFContext, psUFDest, psUFSrcA, bCos);

				break;
			}
			case GLSLIC_OP_FLOOR:	/* Dst, SrcA: performs Dst.m = floor(SrcA.m) */
			{
				AddSimpleALUToUFCode(psCPD, psUFContext, psUFDest, psUFSrcA, NULL, NULL, UFOP_FLR, 1);
				break;
			}
			case GLSLIC_OP_CEIL:		/* Dst, SrcA: performs Dst.m = ceil(SrcA.m) */
			{
				AddAluToUFCode( psUFContext, psUFDest, psUFSrcA, IMG_NULL, IMG_NULL, UFOP_CEIL, 1, IMG_TRUE );

				break;
			}

			case GLSLIC_OP_DFDX:		/* Dst, SrcA: derivative in x for input SrcA */
			{
				AddSimpleALUToUFCode(psCPD, psUFContext, psUFDest, psUFSrcA, NULL, NULL, UFOP_DSX, 1);
				break;
			}
			case GLSLIC_OP_DFDY:		/* Dst, SrcA: derivative in y for input SrcA */
			{
				AddDFDYToUFCode(psCPD, psUFContext, psUFDest, psUFSrcA);
				break;
			}

			/* Potential extension ops */
			case GLSLIC_OP_POW:		/* Dst, Src */
			{
				bSuccess = ProcessICInstPOW(psCPD, psUFContext, psUFDest, psUFSrcA, psUFSrcB);

				break;
			}

			case GLSLIC_OP_FRC:		/* Dst, SrcA: performs Dst.m = SrcA.m - floor(SrcA.m). */
			{
				AddSimpleALUToUFCode(psCPD, psUFContext, psUFDest, psUFSrcA, NULL, NULL, UFOP_FRC, 1);
				break;
			}

			case GLSLIC_OP_MOD:		/* Dst, SrcA, SrcB: perform Dst.m = SrcA.m % SrcB.m */
			{
				{
					/* Float version, using mod() function. */
					bSuccess = ProcessICInstMOD(psCPD, psUFContext, psUFDest, psUFSrcA, psUFSrcB);
				}
				break;
			}

			case GLSLIC_OP_TAN:		/* Dst, SrcA */
			{
				bSuccess = ProcessICInstTAN(psCPD, psUFContext, psUFDest, psUFSrcA);

				break;
			}
                
			case GLSLIC_OP_TEXLD:	/* Dst, SrcA, SrcB: Normal texutre look up					*/
			{
				if(psUFContext->eProgramType == GLSLPT_VERTEX)
				{
					/* Need to use LDL to perform lookup */
					bSuccess = AddDirectTextureLookupToUFCode(psCPD, psUFContext, psUFDest, psUFSrcA, psUFSrcB, NULL, IMG_NULL, UFOP_LDL);
				}
				else
				{
					/* A direct texture lookup */
					bSuccess = AddDirectTextureLookupToUFCode(psCPD, psUFContext, psUFDest, psUFSrcA, psUFSrcB, NULL, IMG_NULL, UFOP_LD);
				}

				break;
			}
			case GLSLIC_OP_TEXLDP:	/* Dst, SrcA, SrcB: Projected texture look up				*/
			{
				if(psUFContext->eProgramType == GLSLPT_VERTEX)
				{
					/* Need to use LDL to do texture lookup */
					bSuccess = ProcessVPInstTEXLDP(psCPD, psUFContext, psUFDest, psUFSrcA, psUFSrcB, IMG_NULL);
				}
				else
				{
					/* Can use LDP directly */
					bSuccess = ProcessFPInstTEXLDP(psCPD, psUFContext, psUFDest, psUFSrcA, psUFSrcB, IMG_NULL);
				}
				break;
			}
			case GLSLIC_OP_TEXLDL:	/* Dst, SrcA, SrcB, SrcC: Look up with specific lod */
			{
				bSuccess = 
					AddDirectTextureLookupToUFCode(psCPD, psUFContext, psUFDest, psUFSrcA, psUFSrcB, psUFSrcC, IMG_NULL, UFOP_LDL);

				break;
			}
			case GLSLIC_OP_TEXLDB:	/* Dst, SrcA, SrcB, SrcC: Look up with lod bias, fragment shaders only  */
			{
				DebugAssert(psUFContext->eProgramType == GLSLPT_FRAGMENT);

				bSuccess = 
					AddDirectTextureLookupToUFCode(psCPD, psUFContext, psUFDest, psUFSrcA, psUFSrcB, psUFSrcC, IMG_NULL, UFOP_LDB);
				
				break;
			}
			case GLSLIC_OP_TEXLDD: /* LDD Dst, sampler, coord, dPdx, dPdy: Lookup with gradients supplied */
			{
				bSuccess = ProcessICInstTEXLDD(psCPD, psUFContext, psUFDest, psUFSrcA, psUFSrcB, psUFSrcC, psUFSrcD, IMG_NULL);

				break;
			}
			case GLSLIC_OP_NOP:
			{
				break;
			}

			default:
			case GLSLIC_OP_ASIN:		/* Dst, SrcA */
			case GLSLIC_OP_ACOS:		/* Dst, SrcA */
			case GLSLIC_OP_ATAN:		/* Dst, SrcA */	
			{
				LOG_INTERNAL_ERROR(("Instruction %d unsupported by Uniflex input.\n", psCurrentInst->eOpCode));
				bSuccess = IMG_FALSE;
			}
		}

		/* If not successful */
		if(!bSuccess)
		{
			return IMG_FALSE;
		}

#if defined(DEBUG) && defined(DUMP_LOGFILES)
		{
			PUNIFLEX_INST psStartInst;

			if (psLastDumpedInst == NULL)
			{
				psStartInst = psUFContext->psFirstUFInst;
			}
			else
			{
				psStartInst = psLastDumpedInst->psILink;
			}

			/* Dump ICode */
			
			ICDumpICInstruction(LOGFILE_COMPILER, psUFContext->psSymbolTable, 0, psCurrentInst, uInst, IMG_FALSE, IMG_FALSE);
			DumpLogMessage(LOGFILE_COMPILER, 0, "==========================\n");
			
			/* Dump UniFlex input */
			DumpMultipleUFInsts(psCPD, psStartInst, psUFContext->psEndUFInst);

			/* Update last dumped instruction */
			psLastDumpedInst = psUFContext->psEndUFInst;
		}
#endif 
	}

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: BuildTypeCompUseMask

 PURPOSE	: Build component use mask, this only depends on the base type.

 PARAMETERS	: eTypeSpecifier		- Type specifier
			  pu8CompUseMaskData	- Pointer to mask

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID BuildTypeCompUseMask(GLSLUniFlexContext *psUFContext)
{
	IMG_UINT i;

	for(i = 0; i < GLSLTS_NUM_TYPES; i++)
		psUFContext->auCompUseMask[i] = 0x1;/* Initialize to 1s for samplers & struct. */

	psUFContext->auCompUseMask[GLSLTS_INVALID]	= 0x0;
	psUFContext->auCompUseMask[GLSLTS_VOID]		= 0x0;

	psUFContext->auCompUseMask[GLSLTS_FLOAT]	= 0x1;
	psUFContext->auCompUseMask[GLSLTS_VEC2]		= 0x3;
	psUFContext->auCompUseMask[GLSLTS_VEC3]		= 0x7;
	psUFContext->auCompUseMask[GLSLTS_VEC4]		= 0xF;

	psUFContext->auCompUseMask[GLSLTS_INT]		= 0x1;
	psUFContext->auCompUseMask[GLSLTS_IVEC2]	= 0x3;
	psUFContext->auCompUseMask[GLSLTS_IVEC3]	= 0x7;
	psUFContext->auCompUseMask[GLSLTS_IVEC4]	= 0xF;
	psUFContext->auCompUseMask[GLSLTS_BOOL]		= 0x1;
	psUFContext->auCompUseMask[GLSLTS_BVEC2]	= 0x3;
	psUFContext->auCompUseMask[GLSLTS_BVEC3]	= 0x7;
	psUFContext->auCompUseMask[GLSLTS_BVEC4]	= 0xF;

	psUFContext->auCompUseMask[GLSLTS_MAT2X2]	= 0x33;
	psUFContext->auCompUseMask[GLSLTS_MAT2X3]	= 0x77;
	psUFContext->auCompUseMask[GLSLTS_MAT2X4]	= 0xFF;

	psUFContext->auCompUseMask[GLSLTS_MAT3X2]	= 0x333;
	psUFContext->auCompUseMask[GLSLTS_MAT3X3]	= 0x777;
	psUFContext->auCompUseMask[GLSLTS_MAT3X4]	= 0xFFF;

	psUFContext->auCompUseMask[GLSLTS_MAT4X2]	= 0x3333;
	psUFContext->auCompUseMask[GLSLTS_MAT4X3]	= 0x7777;
	psUFContext->auCompUseMask[GLSLTS_MAT4X4]	= 0xFFFF;
}

#ifdef OGLES2_VARYINGS_PACKING_RULE

/*****************************************************************************
 FUNCTION	: InitialiseTexCoordsTable

 PURPOSE	: 

 PARAMETERS	: 

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID InitialiseTexCoordsTable(GLSLTexCoordsTable *psTable)
{
	IMG_UINT32 i;

	/* Zero allocated bits */ 
	memset(psTable->auAllocatedBits, 0, sizeof(psTable->auAllocatedBits) );

	memset(psTable->auDynamicallyIndexed, 0, sizeof(psTable->auDynamicallyIndexed));

	psTable->auNextAvailRow = 0;

	psTable->bVaryingsPacked = IMG_FALSE;

	for(i = 0; i < NUM_TC_REGISTERS; i++)
	{
		psTable->auTexCoordPrecisions[i] = GLSLPRECQ_INVALID;
		psTable->aeTexCoordModidifierFlags[i] = GLSLVMOD_INVALID;
	}
}

#endif
/*****************************************************************************
 FUNCTION	: CreateUniFlexContext

 PURPOSE	: Create uniflex context before generation of UF code

 PARAMETERS	: psICProgram		- IC Program

 RETURNS	: Pointer to uniflex context.
*****************************************************************************/
static GLSLUniFlexContext *CreateUniFlexContext(GLSLCompilerPrivateData *psCPD, GLSLICProgram* psICProgram)
{
	IMG_UINT32 i;

	GLSLUniFlexContext *psUFContext;

	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);

	psUFContext = DebugMemAlloc(sizeof(GLSLUniFlexContext));
	if(psUFContext == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("CreateUniFlexContext: Failed to allocate memory for uniflex context \n"));
		return IMG_NULL;
	}

	#ifdef SRC_DEBUG
	/* Just added here to handle DebugAssert psCPD definition requirment in CreateInstruction */
	psUFContext->psCPD = psCPD;
	#endif /* SRC_DEBUG */

	psUFContext->psICProgram			= psICProgram;
	psUFContext->psCurrentICInst		= IMG_NULL;
	psUFContext->eProgramType			= psICContext->eProgramType;
	psUFContext->psSymbolTable			= psICProgram->psSymbolTable;

	/* Init varying mask and centroid mask */
	psUFContext->eActiveVaryingMask		= (GLSLVaryingMask)0;
	psUFContext->uTexCoordCentroidMask	= 0;

	/* Init UF instructions */
	psUFContext->psFirstUFInst			= IMG_NULL;
	psUFContext->psEndUFInst			= IMG_NULL;

	/* Init UF register assignment tables */
	UFRegATableInitialize(&psUFContext->sAttribTab, 16);
	UFRegATableInitialize(&psUFContext->sFloatConstTab, (IMG_UINT32)-1);
	UFRegATableInitialize(&psUFContext->sTempTab, (IMG_UINT32)-1);
	UFRegATableInitialize(&psUFContext->sPredicateTab, UF_MAX_PRED_REGS);
	UFRegATableInitialize(&psUFContext->sFragmentOutTab, (IMG_UINT32)-1);

#ifdef OGLES2_VARYINGS_PACKING_RULE
	InitialiseTexCoordsTable(&psUFContext->sTexCoordsTable);
#else
	UFRegATableInitialize(&psUFContext->sTexCoordTab, NUM_TC_REGISTERS);

#if defined(GLSL_ES)

	/* Reserve tc0 for point sprite if there is a write to point size */
	if((psICContext->eProgramType == GLSLPT_VERTEX) && (psICContext->eBuiltInsWrittenTo & GLSLBVWT_POINTSIZE))
	{
		/* DIM is 2 is enough */
		IMG_UINT32 uReservedTexCoordDim = 2;

		UFRegATableAssignRegisters(psUFContext, &psUFContext->sTexCoordTab, IMG_FALSE, uReservedTexCoordDim, GLSLPRECQ_LOW);

		psUFContext->auTexCoordDims[0] = uReservedTexCoordDim;
		psUFContext->aeTexCoordPrecisions[0] = GLSLPRECQ_LOW;
		psUFContext->eActiveVaryingMask |= GLSLVM_TEXCOORD0;
	}
#endif

#endif

	/* Reserve the first predicate (4 components) for general purpose */
	UFRegATableAssignRegisters(psUFContext,  &psUFContext->sPredicateTab, IMG_FALSE, 4, GLSLPRECQ_HIGH);

	/* Init UF sampler table */
	psUFContext->sSamplerTab.auSamplerToSymbolId = NULL;
	for (i = 0; i < UF_MAX_LABEL; i++)
	{
		psUFContext->auLabelToSymbolId[i] = (IMG_UINT32)-1;
	}

	/* Init UF sampler count. */
	psUFContext->uSamplerCount = 0;

	/* Init UF sampler dimensions. */
	psUFContext->asSamplerDims = NULL;

	psUFContext->bRegisterLimitExceeded = IMG_FALSE;

	/* Init two side color usages */
	psUFContext->bFrontFacingUsed		= IMG_FALSE;
	psUFContext->bColorUsed				= IMG_FALSE;
	psUFContext->bSecondaryColorUsed	= IMG_FALSE;
	psUFContext->bFragCoordUsed			= IMG_FALSE;

	BuildTypeCompUseMask(psUFContext);

	/* Init HW symbol table, symbol register use table, structure alloc information table */ 
	InitialiseHWSymbolTable(psCPD, psUFContext);
	InitialiseStructAllocInfoTable(psUFContext);

	/* Init loop nest information */
	psUFContext->uLoopNestCount					= 0;
	psUFContext->uIfNestCount					= 0;
	psUFContext->bFirstRet						= IMG_TRUE;

	/* Init indexable temp arrays */
	psUFContext->uIndexableTempArrayCount		= 0;
	psUFContext->psIndexableTempArraySizes		= IMG_NULL;
	psUFContext->uNextArrayTag					= 0;

	psUFContext->uConstStaticFlagCount			= 0;
	psUFContext->puConstStaticFlags				= IMG_NULL;

	/* Init constant ranges */
	psUFContext->sConstRanges.uRangesCount		= 0;
	psUFContext->sConstRanges.psRanges			= NULL;
	
	/* Init varying ranges. */
	psUFContext->sVaryingRanges.uRangesCount	= 0;
	psUFContext->sVaryingRanges.psRanges		= NULL;

	return psUFContext;
}

/*****************************************************************************
 FUNCTION	: FreeUniFlexInstructions

 PURPOSE	: Destroy uniflex instructions

 PARAMETERS	: psFirstInst	- Code to destroy.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID FreeUniFlexInstructions(GLSLCompilerPrivateData *psCPD, PUNIFLEX_INST psFirstInst)
{
	PUNIFLEX_INST psProg = psFirstInst;

	PVR_UNREFERENCED_PARAMETER(psCPD);

	while (psProg != NULL)
	{
		PUNIFLEX_INST psTemp = psProg->psILink;
		DebugMemFree(psProg);
		psProg = psTemp;
	}
}

/*****************************************************************************
 FUNCTION	: DestroyUniFlexContext

 PURPOSE	: Destroy uniflex context

 PARAMETERS	: psUFContext	- UF context to destroy.
			  bUFGenerated	- Has UF code successfully been generated ?

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID DestroyUniFlexContext(GLSLCompilerPrivateData *psCPD, GLSLUniFlexContext *psUFContext, IMG_BOOL bUFGenerated)
{
	/* If UF code has not been generated successfully, free those generated */
	if (!bUFGenerated)
	{
		/* Free UF linked instructions */
		FreeUniFlexInstructions(psCPD, psUFContext->psFirstUFInst);

		/* Free indexable temp arrays */
		if (psUFContext->psIndexableTempArraySizes)
		{
			DebugMemFree(psUFContext->psIndexableTempArraySizes);
			psUFContext->psIndexableTempArraySizes = IMG_NULL;
		}

		/* Free static flags */
		if(psUFContext->puConstStaticFlags)
		{
			DebugMemFree(psUFContext->puConstStaticFlags);
			psUFContext->puConstStaticFlags = IMG_NULL;
		}

		/* Free constant ranges. */
		if (psUFContext->sConstRanges.psRanges != NULL)
		{
			DebugMemFree(psUFContext->sConstRanges.psRanges);
			psUFContext->sConstRanges.psRanges = NULL;
		}

		/* Free varying ranges. */
		if (psUFContext->sVaryingRanges.psRanges != NULL)
		{
			DebugMemFree(psUFContext->sVaryingRanges.psRanges);
			psUFContext->sVaryingRanges.psRanges = NULL;
		}

		/* Free sampler dimensions. */
		if (psUFContext->asSamplerDims != NULL)
		{
			DebugMemFree(psUFContext->asSamplerDims);
			psUFContext->asSamplerDims = NULL;
		}
	}

	/* Free sampler<->symbol mapping. */
	if (psUFContext->sSamplerTab.auSamplerToSymbolId != NULL)
	{
		DebugMemFree(psUFContext->sSamplerTab.auSamplerToSymbolId);
		psUFContext->sSamplerTab.auSamplerToSymbolId = NULL;
	}

	/* Free HW symbol table */
	DestroyHWSymbolTable(psUFContext);

	/* Destroy structure allocation information */
	DestroyStructAllocInfoTable(psUFContext);

	/* Don't free constant range list */
	/* Don't free indexable temp list */
	/* Don't free static constant flags */

	/* Free UniFlex context */
	DebugMemFree(psUFContext);
}


/*****************************************************************************
 FUNCTION	: DestroyUniFlexCode

 PURPOSE	: Destroy uni flex code.

 PARAMETERS	: psUniFlexCode		- Code to destroy.

 RETURNS	: Nothing.
*****************************************************************************/
IMG_INTERNAL IMG_VOID DestroyUniFlexCode(GLSLCompilerPrivateData *psCPD,
										 IMG_VOID			*pvUniFlexContext,
										 GLSLUniFlexCode	*psUniFlexCode,
										 IMG_BOOL			bFreeUFInput,
										 IMG_BOOL			bFreeUFOutput)
{
	if (!psUniFlexCode)
	{
		return;
	}

	if (bFreeUFInput)
	{
		/* Free input instructions */ 
		if (psUniFlexCode->pvUFCode)
		{
			FreeUniFlexInstructions(psCPD, psUniFlexCode->pvUFCode);
			psUniFlexCode->pvUFCode = IMG_NULL;
		}
		/* Free constant range list */
		if (psUniFlexCode->sConstRanges.uRangesCount > 0)
		{
			DebugMemFree(psUniFlexCode->sConstRanges.psRanges);
			psUniFlexCode->sConstRanges.psRanges = NULL;
		}
		/* Free varying range list */
		if (psUniFlexCode->sVaryingRanges.uRangesCount > 0)
		{
			DebugMemFree(psUniFlexCode->sVaryingRanges.psRanges);
			psUniFlexCode->sVaryingRanges.psRanges = NULL;
		}
		/* Free indexable temp arrays */
		if (psUniFlexCode->psIndexableTempArraySizes)
		{
			DebugMemFree(psUniFlexCode->psIndexableTempArraySizes);
			psUniFlexCode->psIndexableTempArraySizes = IMG_NULL;
		}

		/* Free static flags */
		if(psUniFlexCode->puConstStaticFlags)
		{
			DebugMemFree(psUniFlexCode->puConstStaticFlags);
			psUniFlexCode->puConstStaticFlags = IMG_NULL;
		}

		/* Free sampler dimensions. */
		if(psUniFlexCode->asSamplerDims != NULL)
		{
			DebugMemFree(psUniFlexCode->asSamplerDims);
			psUniFlexCode->asSamplerDims = NULL;
		}
	}

	if (bFreeUFOutput)
	{
#if defined(OUTPUT_USPBIN)
		PVRUniFlexDestroyUspBin(pvUniFlexContext, psUniFlexCode->psUniPatchInput);
		psUniFlexCode->psUniPatchInput = IMG_NULL;

		PVRUniFlexDestroyUspBin(pvUniFlexContext, psUniFlexCode->psUniPatchInputMSAATrans);
		psUniFlexCode->psUniPatchInputMSAATrans = IMG_NULL;

		#ifdef SRC_DEBUG
			/* Free the source line cycle cost table */
		if(psUniFlexCode->psUniFlexHW != IMG_NULL)
		{
			if(psUniFlexCode->psUniFlexHW->
					asTableSourceCycle.psSourceLineCost != IMG_NULL)
			{
				DebugMemFree(psUniFlexCode->psUniFlexHW->
					asTableSourceCycle.psSourceLineCost);
				psUniFlexCode->psUniFlexHW->
					asTableSourceCycle.psSourceLineCost = IMG_NULL;
			}
			psUniFlexCode->psUniFlexHW->
				asTableSourceCycle.uTableCount = 0;
			psUniFlexCode->psUniFlexHW->
				asTableSourceCycle.uTotalCycleCount = 0;

			DebugMemFree(psUniFlexCode->psUniFlexHW);
			psUniFlexCode->psUniFlexHW = IMG_NULL;
		}
		#endif /* SRC_DEBUG */
#else
		PVR_UNREFERENCED_PARAMETER(pvUniFlexContext);

#ifdef GEN_HW_CODE
		/* Free the HW output code */
		if (psUniFlexCode->psUniFlexHW)
		{
			PVRCleanupUniflexHw(pvUniFlexContext, psUniFlexCode->psUniFlexHW);

			DebugMemFree(psUniFlexCode->psUniFlexHW);
			psUniFlexCode->psUniFlexHW = IMG_NULL;
		}
#endif
#endif
	}

	/* Free the structs */
	if (bFreeUFInput && bFreeUFOutput)
	{
		DebugMemFree(psUniFlexCode);
	}
}


/*****************************************************************************
 FUNCTION	: PrepareUFCodeOutput

 PURPOSE	: Generate UniFlex code output from UniFlex context.

 PARAMETERS	: psUFContext

 RETURNS	: UniFlex code output
*****************************************************************************/
static GLSLUniFlexCode *PrepareUFCodeOutput(GLSLCompilerPrivateData *psCPD,
											GLSLUniFlexContext *psUFContext)
{
	GLSLUniFlexCode *psUniFlexCode;

	/* Structure memory */
	psUniFlexCode = DebugMemCalloc(sizeof(GLSLUniFlexCode));
	if(psUniFlexCode == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("PrepareUFCodeOutput: Failed to allocate memory \n"));
		return IMG_NULL;
	}

	/* UF code */
	psUniFlexCode->pvUFCode = psUFContext->psFirstUFInst;

	/* Continuous constant range */
	psUniFlexCode->sConstRanges = psUFContext->sConstRanges;

	/* Varying ranges. */
	psUniFlexCode->sVaryingRanges = psUFContext->sVaryingRanges;

	/* Varying presents mask */
#ifdef OGLES2_VARYINGS_PACKING_RULE
	psUniFlexCode->bVaryingsPacked			 = psUFContext->sTexCoordsTable.bVaryingsPacked;;
#else
	psUniFlexCode->bVaryingsPacked			 = IMG_FALSE;
#endif
	psUniFlexCode->eActiveVaryingMask		 = psUFContext->eActiveVaryingMask;
	psUniFlexCode->uTexCoordCentroidMask	 = psUFContext->uTexCoordCentroidMask;
	memcpy(psUniFlexCode->auTexCoordDims, psUFContext->auTexCoordDims, NUM_TC_REGISTERS * sizeof(IMG_UINT32));
	memcpy(psUniFlexCode->aeTexCoordPrecisions, psUFContext->aeTexCoordPrecisions, NUM_TC_REGISTERS * sizeof(IMG_UINT32));

	/* Count of samplers used. */
	psUniFlexCode->uSamplerCount = psUFContext->uSamplerCount;

	/* Sample dimensions */
	psUniFlexCode->asSamplerDims = psUFContext->asSamplerDims;

	/* Indexable temporary arrays */
	psUniFlexCode->uNumIndexableTempArrays	 = psUFContext->uIndexableTempArrayCount;
	psUniFlexCode->psIndexableTempArraySizes = psUFContext->psIndexableTempArraySizes;

	/* Static flags */
	psUniFlexCode->uConstStaticFlagCount	 = psUFContext->uConstStaticFlagCount;
	psUniFlexCode->puConstStaticFlags		 = psUFContext->puConstStaticFlags;

	return psUniFlexCode;
}


/*****************************************************************************
 FUNCTION	: ReassignVSOutputRegisters

 PURPOSE	: Since vs output registers has to be packed, this function reassigns VS output registers.  

 PARAMETERS	: psUFContext	- UF context

 RETURNS	:
*****************************************************************************/
static IMG_VOID ReassignVSOutputRegisters(GLSLCompilerPrivateData *psCPD, GLSLUniFlexContext *psUFContext)
{
	IMG_UINT32 uCurrentOffset = 0;
	IMG_UINT32 Remap[GLSLVR_NUM];
	UNIFLEX_INST *psInstr;
	IMG_UINT32 i;

	PVR_UNREFERENCED_PARAMETER(psCPD);

	/*
		build remapping first 
	*/


	/* Position */
	DebugAssert(psUFContext->eActiveVaryingMask & (GLSLVM_POSITION));
	Remap[GLSLVR_POSITION] = uCurrentOffset;
	uCurrentOffset += 4;

	/* Color0 */
	Remap[GLSLVR_COLOR0] = uCurrentOffset;
	if(psUFContext->eActiveVaryingMask & (GLSLVM_COLOR0))
	{
		uCurrentOffset += 4;
	}

	/* Color1 */
	Remap[GLSLVR_COLOR1] = uCurrentOffset;
	if(psUFContext->eActiveVaryingMask & (GLSLVM_COLOR1))
	{
		uCurrentOffset += 4;
	}

	for(i = 0; i < NUM_TC_REGISTERS; i++)
	{
		Remap[GLSLVR_TEXCOORD0 + i] = uCurrentOffset;
		if(psUFContext->eActiveVaryingMask & (GLSLVM_TEXCOORD0 << i))
		{
			uCurrentOffset += psUFContext->auTexCoordDims[i];
		}
	}

#if !defined(GLSL_ES)
	if(!psCPD->psCompilerResources->bNativeFogSupport)
	{
	
			/* Without special support, we will use texture coordinate for fog, it is needed to be before point size */
		
		/* Fog */
		Remap[GLSLVR_FOG] = uCurrentOffset;
		if(psUFContext->eActiveVaryingMask & (GLSLVM_FOG))
		{
			uCurrentOffset += 1;
		}

		/* Point size */
		Remap[GLSLVR_PTSIZE] = uCurrentOffset;
		if(psUFContext->eActiveVaryingMask & (GLSLVM_PTSIZE))
		{
			uCurrentOffset += 1;
		}
	
	

	}
	else
#endif
	{
		/* On SGX535, fog is after point size */
		/* Point size */
		Remap[GLSLVR_PTSIZE] = uCurrentOffset;
		if(psUFContext->eActiveVaryingMask & (GLSLVM_PTSIZE))
		{
			uCurrentOffset += 1;
		}

		/* Fog */
		Remap[GLSLVR_FOG] = uCurrentOffset;
		if(psUFContext->eActiveVaryingMask & (GLSLVM_FOG))
		{
			uCurrentOffset += 1;
		}
	}

	/* Clip planes */
	for(i = 0; i < 6; i++)
	{
		Remap[GLSLVR_CLIPPLANE0 + i] = uCurrentOffset;
		if(psUFContext->eActiveVaryingMask & (GLSLVM_CLIPPLANE0 << i))
		{
			/* Each plane needs one output register */
			uCurrentOffset += 1;
		}
	}

	/*
		Reassign
	*/
	psInstr = psUFContext->psFirstUFInst;
	while(psInstr)
	{
		PCINPUT_INST_DESC psDesc = PVRUniFlexGetInputInstDesc(psInstr->eOpCode);

		if(psDesc->uNumDests)
		{
			if(psInstr->sDest.eType == UFREG_TYPE_VSOUTPUT)
			{
				psInstr->sDest.uNum  = Remap[psInstr->sDest.uNum ];
			}
		}

		for(i = 0; i < psDesc->uNumSrcArgs; i++)
		{
			if(psInstr->asSrc[i].eType == UFREG_TYPE_VSOUTPUT)
			{
				psInstr->asSrc[i].uNum  = Remap[psInstr->asSrc[i].uNum ];
			}
		}

		psInstr = psInstr->psILink;
	}
}


/*****************************************************************************
 FUNCTION	: PostProcessConstants

 PURPOSE	: Generate constant ranges, static flags 

 PARAMETERS	: psUFContext	- UF context

 RETURNS	: Nothing
*****************************************************************************/
static IMG_VOID PostProcessConstants(GLSLCompilerPrivateData *psCPD, GLSLUniFlexContext *psUFContext)
{
	IMG_UINT32 uNumConstants;

	GLSLHWSymbolTab *psHWSymbolTab = &psUFContext->sHWSymbolTab;

	HWSYMBOL *psHWSymbol = psHWSymbolTab->psHWSymbolHead;

	PVR_UNREFERENCED_PARAMETER(psCPD);

	uNumConstants = UFRegATableGetMaxComponent(&psUFContext->sFloatConstTab);

	psUFContext->puConstStaticFlags = DebugMemCalloc(ROUND_UP(uNumConstants,32)/8);


	while(psHWSymbol)
	{
		/* Setup static flags */
		if(psHWSymbol->sFullType.eTypeQualifier == GLSLTQ_CONST && psHWSymbol->eRegType == UFREG_TYPE_CONST)
		{
			IMG_UINT32 uConstStart = psHWSymbol->u.uCompOffset;
			IMG_UINT32 uConstCount = psHWSymbol->uTotalAllocCount;
			IMG_UINT32 uConstEnd = uConstStart + uConstCount;
			IMG_UINT32 uWordIndex, uWordBit;
			IMG_UINT32 i;

			if (psUFContext->uConstStaticFlagCount < uConstEnd)
			{
				psUFContext->uConstStaticFlagCount = uConstEnd;
			}

			for(i = uConstStart; i < uConstStart + uConstCount; i++)
			{
				uWordIndex = i/32;
				uWordBit = i%32;

				psUFContext->puConstStaticFlags[uWordIndex] |= (1 << uWordBit);
			}
		}

		/* Jump to the next symbols */
		psHWSymbol = psHWSymbol->psNext;
	}

}

/*****************************************************************************
 FUNCTION	: PostGenerateUniFlexCode

 PURPOSE	: Generate constant ranges, static flags 

 PARAMETERS	: psUFContext	- UF context

 RETURNS	: Nothing
*****************************************************************************/
static IMG_BOOL PostGenerateUniFlexCode(GLSLCompilerPrivateData *psCPD, GLSLUniFlexContext *psUFContext)
{
	if(psUFContext->eProgramType == GLSLPT_FRAGMENT)
	{
#ifndef OGLES2_VARYINGS_PACKING_RULE
		/* Generate two sided lighting code if neccessary */
		if(psUFContext->bFrontFacingUsed || psUFContext->bColorUsed || psUFContext->bSecondaryColorUsed )
		{
			if(!GenerateFaceSelection(psCPD, psUFContext))
			{
				LOG_INTERNAL_ERROR(("PostGenerateUniFlexCode: Failed to generate front facing code \n"));
				return IMG_FALSE;
			}
		}
#endif

		if(psUFContext->bFragCoordUsed)
		{
			if(!GenerateFragCoordAdjust(psCPD, psUFContext))
			{
				LOG_INTERNAL_ERROR(("PostGenerateUniFlexCode: Failed to generate front facing code \n"));
				return IMG_FALSE;
			}
		}
	}


	/* Post handling dynamicaaly addressing to inputs/outputs */
	if(psUFContext->eProgramType == GLSLPT_VERTEX)
	{
		/* Pack vertex shader output registiers */
		ReassignVSOutputRegisters(psCPD, psUFContext);
	}

	/* Extract continuous constant ranges, static flags etc */
	PostProcessConstants(psCPD, psUFContext);

	return IMG_TRUE;

}

/*****************************************************************************
 FUNCTION	: GetFinalIndexedType

 PURPOSE	: Get final operand type after multiple offsets

 PARAMETERS	: return final type and iArraySize

 RETURNS	: Nothing
*****************************************************************************/
static GLSLFullySpecifiedType GetFinalIndexedType(GLSLCompilerPrivateData *psCPD,
											SymTable					*psSymbolTable,
										   GLSLFullySpecifiedType	sBaseFullType,
										   IMG_INT32				*piArraySize,
										   IMG_UINT32				uNumOperandOffsets, 
										   GLSLICOperandOffset		*psOperandOffsets)
{
	GLSLFullySpecifiedType sFullType;

	IMG_UINT32 i;

	IMG_INT32 iArraySize = *piArraySize;

	PVR_UNREFERENCED_PARAMETER(psCPD);

	/* Initially it is the base type */
	sFullType = sBaseFullType;

	/* Work out the typ after each offset */
	for(i = 0; i < uNumOperandOffsets; i++)
	{
		if(iArraySize)
		{
			*piArraySize = 0;
			return sBaseFullType;
		}
		else
		{
			sFullType = GetIndexedType(psCPD, psSymbolTable, &sFullType, psOperandOffsets[i].uStaticOffset, &iArraySize);
		}
	}

	/* Return the final type and the array size */
	*piArraySize = iArraySize;
	
	return sFullType;
}

/*****************************************************************************
 FUNCTION	: DoesStructureContainArrays

 PURPOSE	: Check whether a structure contains arrays

 PARAMETERS	: 

 RETURNS	: Nothing
*****************************************************************************/
static IMG_BOOL DoesTypeContainBigArrays(GLSLCompilerPrivateData *psCPD,
										 GLSLUniFlexContext		*psUFContext,
										 GLSLFullySpecifiedType	*psFullType,
										 IMG_INT32				iArraySize)
{
	IMG_UINT32 i;
	IMG_INT32 iMemberArraySize;
	GLSLFullySpecifiedType *psMemberFullType;
	IMG_UINT32 uValidComponents;

	/* Array, check for total number of valid components */
	if(iArraySize)
	{
		 uValidComponents = GetValidComponentCount(psCPD, psUFContext, psFullType, iArraySize);

		if(uValidComponents > MAX_COMPONENTS_UNROLLING_ARRAY_LOOP)
		{
			return IMG_TRUE;
		}
		else
		{
			return IMG_FALSE;
		}
	}

	/* Structure, does it contain a big array inside */
	if(GLSL_IS_STRUCT(psFullType->eTypeSpecifier))
	{
		GLSLStructureAlloc *psStructAlloc = GetStructAllocInfo(psCPD, psUFContext, psFullType);

		/* Check individual members */
		for(i=0; i < psStructAlloc->uNumMembers; i++)
		{
			iMemberArraySize = psStructAlloc->psMemberAlloc[i].iArraySize;
			psMemberFullType = &psStructAlloc->psMemberAlloc[i].sFullType;

			if(iMemberArraySize)
			{
				uValidComponents = GetValidComponentCount(psCPD, psUFContext, psMemberFullType, iMemberArraySize);
	
				if(uValidComponents > MAX_COMPONENTS_UNROLLING_ARRAY_LOOP)
				{
					return IMG_TRUE;
				}
			}
			else if(GLSL_IS_STRUCT(psMemberFullType->eTypeSpecifier))
			{
				return DoesTypeContainBigArrays(psCPD, psUFContext, psMemberFullType, iMemberArraySize);
			}
		}

	}

	/* Not array, not structure, return false */
	return IMG_FALSE;
}

#ifdef OGLES2_VARYINGS_PACKING_RULE

#if defined(GLSL_ES)
	#define NUM_VARYING_TYPES 7
#else
		#define NUM_VARYING_TYPES 13
#endif

	/* 
		GLSLES 1.0 specification, appendix A:

		Variables are packed in the following order:
		1. Arrays of mat4 and mat4
		2. Arrays of mat2 and mat2 (since they occupy full rows)
		3. Arrays of vec4 and vec4
		4. Arrays of mat3 and mat3
		5. Arrays of vec3 and vec3
		6. Arrays of vec2 and vec2
		7. Arrays of float and float
	*/

static const GLSLTypeSpecifier auVaryingAllocOrder[NUM_VARYING_TYPES] =
{
	GLSLTS_MAT4X4,

#if !defined(GLSL_ES)
	GLSLTS_MAT3X4,
	GLSLTS_MAT2X4,
	GLSLTS_MAT4X2,
	GLSLTS_MAT3X2,
#endif
	GLSLTS_MAT2X2,

	GLSLTS_VEC4,

#if !defined(GLSL_ES)
	GLSLTS_MAT4X3,
#endif

	GLSLTS_MAT3X3,

#if !defined(GLSL_ES)
	GLSLTS_MAT2X3,
#endif

	GLSLTS_VEC3,
	GLSLTS_VEC2,
	GLSLTS_FLOAT,

};

#define GET_COMPONENT(row, col) (row * REG_COMPONENTS + col)

#define SET_BIT(array, index) \
	(array)[(index) / 8] |= (1 << ((index) % 8))

#define GET_BIT(array, index) \
	((array)[(index) / 8] & (1 << ((index) % 8)))

#define SET_ALLOCBIT(table, row, col)								\
{												\
	SET_BIT(table->auAllocatedBits, GET_COMPONENT(row, col));		\
}	

#define GET_ALLOCBIT(table, row, col) GET_BIT(table->auAllocatedBits, GET_COMPONENT(row, col))

/*****************************************************************************
 FUNCTION	: FindEmptyRectSpace

 PURPOSE	: Find suitable space for the required rows and columns from texture coords table

 PARAMETERS	: 

 RETURNS	: return true if successful
*****************************************************************************/
static IMG_BOOL FindEmptyRectSpace(GLSLCompilerPrivateData *psCPD,
								   GLSLTexCoordsTable		*psTable,
									IMG_UINT32				uNumRows,
									IMG_UINT32				uNumCols,
									IMG_BOOL				bDynamicallyIndexed,
									GLSLFullySpecifiedType	*psFullType,
									IMG_UINT32				*puRowStart,
									IMG_UINT32				*puColStart)
{
	IMG_INT32 i, j, row, col;

	IMG_BOOL bFindSpace = IMG_FALSE;

	IMG_UINT32 uRowStart = 0, uColStart = 0;

	/*
		For 2,3 and 4 component variables packing is started using the 1st column of the 1st row.
		Variables are then allocated to successive rows, aligning them to the 1st column.
	*/
	if(psTable->auNextAvailRow + uNumRows <=  NUM_TC_REGISTERS)
	{
		uRowStart = psTable->auNextAvailRow;
		uColStart = 0;

		/* Update next available row */
		psTable->auNextAvailRow += uNumRows;

		/* Indicate success */
		bFindSpace = IMG_TRUE;
		goto FindSpace;
	}

	/* 
		Packing of any further 3 or 4 component variables will fail at this point.
	*/
	if(psFullType->eTypeSpecifier != GLSLTS_VEC2 &&  psFullType->eTypeSpecifier != GLSLTS_FLOAT)
	{
		return IMG_FALSE;
	}

	/* Indicate varyings are packed */
	psTable->bVaryingsPacked = IMG_TRUE;

	if(psFullType->eTypeSpecifier == GLSLTS_VEC2)
	{
		/* 
			Bottom Up
			
			Spec says:
			For 2 component variables, when there are no spare rows, the strategy is switched to using the
			highest numbered row and the lowest numbered column where the variable will fit. (In practice,
			this means they will be aligned to the x or z component.) 
		*/
		for(i = NUM_TC_REGISTERS - 1; i >= 0; i--)
		{
			for(j = 0; j < REG_COMPONENTS; j++)
			{
				if( !GET_ALLOCBIT(psTable, i, j) &&         /* Is empty? */
				    (j + uNumCols <= REG_COMPONENTS) &&     /* Have enough columns left? */
				    (i + 1 >= (IMG_INT32) uNumRows) &&      /* Have enough rows left? */
				    (psTable->auTexCoordPrecisions[i] == (GLSLPrecisionQualifier)psFullType->ePrecisionQualifier || psTable->auTexCoordPrecisions[i] == GLSLPRECQ_INVALID) &&  /* Same precision */
				    (psTable->aeTexCoordModidifierFlags[i] == (GLSLVaryingModifierFlags)psFullType->eVaryingModifierFlags || psTable->aeTexCoordModidifierFlags[i] == GLSLVMOD_INVALID)	/* Same varying modifier flags */
				  )
				{
					/* Good rect candidate, check for the empty rect we required */
					bFindSpace = IMG_TRUE;

					for(row = i; row > i - (IMG_INT32)uNumRows; row--)
					{
						if(row != i) /* The first row has been checked */
						{
							/* Precisions have to match */
							if( psTable->auTexCoordPrecisions[i] != GLSLPRECQ_UNKNOWN &&
								psTable->auTexCoordPrecisions[row] != (GLSLPrecisionQualifier)psFullType->ePrecisionQualifier)
							{
								bFindSpace = IMG_FALSE;
								break;
							}

							/* Varying modifier flags have to match */
							if( psTable->aeTexCoordModidifierFlags[i] != GLSLVMOD_INVALID &&
								psTable->aeTexCoordModidifierFlags[i] != (GLSLVaryingModifierFlags)psFullType->eVaryingModifierFlags)
							{
								bFindSpace = IMG_FALSE;
								break;
							}
						}

						/* Are all required components in a row are empty? */
						for(col = j; col < j + (IMG_INT32)uNumCols; col++)
						{
							if(GET_ALLOCBIT(psTable, row, col))
							{
								bFindSpace = IMG_FALSE;
								break;
							}
						}

						/* Give up this rect candidate */
						if(!bFindSpace) break;
					}

					if(bFindSpace) 
					{
						/* Since it is bottom up */
						uRowStart = i - uNumRows + 1;
						uColStart = j;

						goto FindSpace;
					}
				}

			}

		}
	}
	else
	{
		/* 
			Top down

			Spec says:

			1 component variables (i.e. floats and arrays of floats) have their own packing rule. They are
			packed in order of size, largest first. Each variable is placed in the column that leaves the least
			amount of space in the column and aligned to the lowest available rows within that column.
			During this phase of packing, space will be available in up to 4 columns. The space within each
			column is always contiguous.
		*/
		IMG_UINT32 uMinSpace = NUM_TC_REGISTERS, uSpace;
		IMG_UINT32 uRow = 0, uCol = 0;
		
		/* From left to right and top down, find column with minimum fittable space */
		for(j = 0; j < REG_COMPONENTS; j++)
		{
			for(i = 0; i < NUM_TC_REGISTERS; i++)
			{
				if( !GET_ALLOCBIT(psTable, i, j) &&				/* Is empty */
					(i + uNumRows < NUM_TC_REGISTERS) &&		/* Have enough rows left */
					(psTable->auTexCoordPrecisions[i] == (GLSLPrecisionQualifier)psFullType->ePrecisionQualifier || psTable->auTexCoordPrecisions[i] == GLSLPRECQ_INVALID) &&  /* Same precision */
					(psTable->aeTexCoordModidifierFlags[i] == (GLSLVaryingModifierFlags)psFullType->eVaryingModifierFlags || psTable->aeTexCoordModidifierFlags[i] == GLSLVMOD_INVALID)	/* Same varying modifier flags */
				  )
				{
					/* Good rect candidate, check for the empty column we required */
					bFindSpace = IMG_TRUE;

					for(row = i; row < NUM_TC_REGISTERS; row++)
					{
						if(row != i) /* The first row has been checked */
						{
							/* Precisions have to match */
							if( psTable->auTexCoordPrecisions[i] != GLSLPRECQ_UNKNOWN &&
								psTable->auTexCoordPrecisions[row] != (GLSLPrecisionQualifier)psFullType->ePrecisionQualifier)
							{
								bFindSpace = IMG_FALSE;
								break;
							}

							/* Varying modifier flags have to match */
							if( psTable->aeTexCoordModidifierFlags[i] != GLSLVMOD_INVALID &&
								psTable->aeTexCoordModidifierFlags[i] != (GLSLVaryingModifierFlags)psFullType->eVaryingModifierFlags)
							{
								bFindSpace = IMG_FALSE;
								break;
							}
						}

						if(GET_ALLOCBIT(psTable, row, j))
						{
							break;
						}
					}

					/* Number of rows in one colum */
					uSpace = row - i;

					/* If the space is fittable and the minimum space, take down the current row and col */ 
					if(uSpace >= uNumRows && uMinSpace > uSpace)
					{	
						bFindSpace = IMG_TRUE;
						uRow = i;
						uCol = j;

						uMinSpace = uSpace;
					}

					/* Jump to the end of gap and continue to check */
					i = row;
				}
			}
		}

		/* The final location got */
		if(bFindSpace)
		{
			uRowStart = uRow;
			uColStart = uCol;
		}
	}
	

FindSpace:
	if(bFindSpace)
	{
		IMG_UINT32 i, j;

		/* Set allocate bits */
		for(i = uRowStart; i < uRowStart + uNumRows; i++)
		{
			if (uNumRows > 1 && bDynamicallyIndexed)
			{
				SET_BIT(psTable->auDynamicallyIndexed, i);
			}

			for(j = uColStart; j < uColStart + uNumCols; j++)
			{
				SET_ALLOCBIT(psTable, i, j);
			}

			/* Take down precision */
			if(psTable->auTexCoordPrecisions[i] == GLSLPRECQ_INVALID)
			{
				psTable->auTexCoordPrecisions[i] = (GLSLPrecisionQualifier)psFullType->ePrecisionQualifier;
			}
			else
			{
				if(psTable->auTexCoordPrecisions[i] != (GLSLPrecisionQualifier)psFullType->ePrecisionQualifier)
				{
					LOG_INTERNAL_ERROR((""));
					return IMG_FALSE;
				}
			}

			/* Take down varying modifier flags */
			if(psTable->aeTexCoordModidifierFlags[i] == GLSLVMOD_INVALID)
			{
				psTable->aeTexCoordModidifierFlags[i] = (GLSLVaryingModifierFlags)psFullType->eVaryingModifierFlags;
			}
			else
			{
				if(psTable->aeTexCoordModidifierFlags[i] != (GLSLVaryingModifierFlags)psFullType->eVaryingModifierFlags)
				{
					LOG_INTERNAL_ERROR((""));
					return IMG_FALSE;
				}
			}

		}

		*puRowStart = uRowStart;
		*puColStart = uColStart;

		return IMG_TRUE;
	}


	return IMG_FALSE;

}

/*****************************************************************************
 FUNCTION	: AllocateVaryingRegisters

 PURPOSE	: 

 PARAMETERS	:

 RETURNS	: 
*****************************************************************************/
static IMG_BOOL AllocateVaryingRegisters(GLSLCompilerPrivateData *psCPD, GLSLUniFlexContext *psUFContext, HWSYMBOL *psHWSymbol)
{
	IMG_UINT32 uArraySize = psHWSymbol->iArraySize ? psHWSymbol->iArraySize : 1;
	GLSLTexCoordsTable *psTable = &psUFContext->sTexCoordsTable;

	/* Number of row and cols in terms of a matrix, a vector is treated as a column matrix */
	IMG_UINT32 uMatRows = TYPESPECIFIER_NUM_ROWS(psHWSymbol->sFullType.eTypeSpecifier);
	IMG_UINT32 uMatCols = TYPESPECIFIER_NUM_COLS(psHWSymbol->sFullType.eTypeSpecifier);

	/* Number of rows and cols required in terms of registers in register bank (NUM_TC_REGISTER rows * 4 columns) */
	IMG_UINT32 uRowsRequired = uMatCols, uColsRequired = uMatRows; 

	/* Total number of rows required */
	IMG_UINT32 uTotalRowsRequired = uRowsRequired * uArraySize;

	IMG_UINT32 uRowStart, uColStart;

	IMG_BOOL bDynamicallyIndexed = (psHWSymbol->eSymbolUsage & GLSLSU_DYNAMICALLY_INDEXED) ? IMG_TRUE : IMG_FALSE;		

	IMG_INT32 iTotalComponents = 0;

#if PACK_VARYING_ARRAY
	IMG_BOOL bVariablePacked = IMG_FALSE;

	if(psHWSymbol->iArraySize > 1 || GLSL_IS_MAT_2ROWS(psHWSymbol->sFullType.eTypeSpecifier))
	{
		if(uColsRequired == 1 || uColsRequired == 2)
		{
			bVariablePacked = IMG_TRUE;

			DebugAssert(GLSL_IS_SCALAR(psHWSymbol->sFullType.eTypeSpecifier) ||
						GLSL_IS_VEC2(psHWSymbol->sFullType.eTypeSpecifier) ||
						GLSL_IS_MAT_2ROWS(psHWSymbol->sFullType.eTypeSpecifier));
		
			uTotalRowsRequired = (uColsRequired * uTotalRowsRequired + 3) / REG_COMPONENTS;
			uColsRequired = REG_COMPONENTS;
		}
	}
#endif /* PACK_VARYING_ARRAY */

	/* Setup RegType */
	if(psUFContext->eProgramType == GLSLPT_FRAGMENT)
	{
		psHWSymbol->eRegType = UFREG_TYPE_TEXCOORD;
	}
	else
	{
		psHWSymbol->eRegType = UFREG_TYPE_VSOUTPUT;
	}

	iTotalComponents += uTotalRowsRequired * uColsRequired;
	
	/* Generate program error if it comsumes more varying components than allowed */
#if defined(GLSL_ES)
	if(iTotalComponents > (psCPD->psCompilerResources->iGLMaxVaryingVectors * 4))
#else
	if(iTotalComponents > psCPD->psCompilerResources->iGLMaxVaryingFloats)
#endif
	{
		LogProgramError(psCPD->psErrorLog, "Requires more varying components than allowed.\n");

		return IMG_FALSE;
	}

	/* Find space for the HW symbol */
	if(!FindEmptyRectSpace(psCPD, psTable, uTotalRowsRequired, uColsRequired, bDynamicallyIndexed, &psHWSymbol->sFullType, &uRowStart, &uColStart))
	{
		LogProgramError(psCPD->psErrorLog, "Failed to allocate varyings.\n");

		return IMG_FALSE;
	}

	/* Record component offset */
	psHWSymbol->u.uCompOffset = uRowStart * REG_COMPONENTS + uColStart;
	psHWSymbol->uCompUseMask = psUFContext->auCompUseMask[psHWSymbol->sFullType.eTypeSpecifier];

#if PACK_VARYING_ARRAY
	if(bVariablePacked)
	{
		if(GLSL_IS_SCALAR(psHWSymbol->sFullType.eTypeSpecifier))
		{
			psHWSymbol->uAllocCount = 1;
		}
		else if(GLSL_IS_VEC2(psHWSymbol->sFullType.eTypeSpecifier))
		{
			psHWSymbol->uAllocCount = 2;
		}
		else
		{
			DebugAssert(GLSL_IS_MAT_2ROWS(psHWSymbol->sFullType.eTypeSpecifier));
				
			/* Update component use mask */
			switch(psHWSymbol->sFullType.eTypeSpecifier)
			{
				case GLSLTS_MAT2X2:
					psHWSymbol->uCompUseMask = 0xF;
					break;
				case GLSLTS_MAT3X2:
					psHWSymbol->uCompUseMask = 0x3F;
					break;
				case GLSLTS_MAT4X2:
					psHWSymbol->uCompUseMask = 0xFF;
					break;
				default:
					LOG_INTERNAL_ERROR(("AllocateVaryingRegisters: What's the type ? "));
					break;
				}

			psHWSymbol->uAllocCount = uMatCols * 2;
		}

		psHWSymbol->uTotalAllocCount = uTotalRowsRequired * uColsRequired;
	}
	else
#endif
	{
	
		if(psHWSymbol->iArraySize)
		{
				psHWSymbol->uAllocCount = uRowsRequired * REG_COMPONENTS;
				psHWSymbol->uTotalAllocCount = uArraySize * psHWSymbol->uAllocCount - uColStart;
			
		}
		else
		{
			if(GLSL_IS_MATRIX(psHWSymbol->sFullType.eTypeSpecifier))
			{
				psHWSymbol->uAllocCount = uRowsRequired * REG_COMPONENTS;
				psHWSymbol->uTotalAllocCount = psHWSymbol->uAllocCount;
			}
			else
			{
				psHWSymbol->uAllocCount = uColsRequired;
				psHWSymbol->uTotalAllocCount = psHWSymbol->uAllocCount;
			}
		}
	}

	if (bDynamicallyIndexed)
	{
		AddToRangeList(psCPD, &psUFContext->sVaryingRanges, psHWSymbol);
	}

	/* Indicate registers have been allocated for the symbol */
	psHWSymbol->bRegAllocated = IMG_TRUE;

	return IMG_TRUE;
}

static IMG_UINT32 GetTexCoordDimension(GLSLUniFlexContext *psUFContext, IMG_UINT32 uTexCoord)
{
	GLSLTexCoordsTable *psTable = &psUFContext->sTexCoordsTable;

	if (GET_BIT(psTable->auDynamicallyIndexed, uTexCoord))
	{
		/*
			If the vertex shader output is dynamically indexed then don't change the dimensionality
			so the stride used to calculate dynamic offsets remains the same.
		*/
		return REG_COMPONENTS;
	}
	else
	{
		IMG_INT32 j;

		for(j = REG_COMPONENTS - 1; j >= 0; j--)
		{
			if(GET_ALLOCBIT(psTable, uTexCoord, j))
			{
				/*
					Leave a gap in vertex shader output if the varying is a scalar.

					The minimum number of texture coordinate size in vertex shader output is 2

					See Eurasia.3D Input Parameter Format.doc
				*/
				if(j == 0)
				{
					j++;
				}
				return j + 1;
			}
		}
		return 0;
	}
}

static IMG_VOID UpdateTexCoordDimInfo(GLSLUniFlexContext *psUFContext)
{
	GLSLTexCoordsTable *psTable = &psUFContext->sTexCoordsTable;

	IMG_INT32 i;

	for(i = 0; i < NUM_TC_REGISTERS; i++)
	{
		psUFContext->auTexCoordDims[i] = GetTexCoordDimension(psUFContext, i);

		if (psUFContext->auTexCoordDims[i] > 0)
		{
			/* Precision for the layer */
			psUFContext->aeTexCoordPrecisions[i] = psTable->auTexCoordPrecisions[i];
			
			/* Active varying mask */
			psUFContext->eActiveVaryingMask |= (GLSLVM_TEXCOORD0 << i);

			/* Centroid mask */
			if(psTable->aeTexCoordModidifierFlags[i] & GLSLVMOD_CENTROID)
			{
				psUFContext->uTexCoordCentroidMask |= (1 << i);
			}
		}
	}
}


#if !defined(GLSL_ES)
/*****************************************************************************
 FUNCTION	: AddExtraHWSymbolsForColorSelection

 PURPOSE	: Add to varying list, but donot allocate register.

 PARAMETERS	: 

 RETURNS	: 
*****************************************************************************/
static IMG_BOOL AddExtraHWSymbolsForColorSelection(GLSLCompilerPrivateData *psCPD, GLSLUniFlexContext *psUFContext)
{
	IMG_UINT32 uSymbolID;
	GLSLPrecisionQualifier eGLColorPrecision = GLSLPRECQ_UNKNOWN;
	GLSLPrecisionQualifier eGLSecondaryColorPrecision = GLSLPRECQ_UNKNOWN;

	/* 
		varying vec4 gl_FrontColor
	*/
	if (psUFContext->bColorUsed)
	{
		if(!FindSymbol(psUFContext->psSymbolTable, "gl_Color", &uSymbolID, IMG_FALSE))
		{
			LOG_INTERNAL_ERROR(("GenerateFaceSelection: Failed to find gl_Color in symbol table \n"));
			return IMG_FALSE;
		}

		/* Get the precision for gl_Color */
		eGLColorPrecision = ICGetSymbolPrecision(psCPD, psUFContext->psSymbolTable, uSymbolID);

		/* Add gl_FrontColor to the general symbol table */
		if(!AddBuiltInIdentifier(psCPD, psUFContext->psSymbolTable,
								 "gl_FrontColor",
								 0,
								 GLSLBV_FRONTCOLOR,
								 GLSLTS_VEC4,
								 GLSLTQ_VERTEX_OUT,
								 eGLColorPrecision,
								 &uSymbolID))
		{
			LOG_INTERNAL_ERROR(("GenerateFaceSelection: Failed to add PMX builtin variable gl_FrontColor to the symbol table !\n"));
			return IMG_FALSE;
		}

		GetHWSymbolEntry(psCPD, psUFContext, uSymbolID, IMG_FALSE);

		if(!AddBuiltInIdentifier(psCPD, psUFContext->psSymbolTable,
								 "gl_BackColor",
								 0,
								 GLSLBV_BACKCOLOR,
								 GLSLTS_VEC4,
								 GLSLTQ_VERTEX_OUT,
								 eGLColorPrecision,
								 &uSymbolID))
		{
			LOG_INTERNAL_ERROR(("GenerateFaceSelection: Failed to add PMX builtin variable gl_PMXBackColor to the symbol table !\n"));
			return IMG_FALSE;
		}

		GetHWSymbolEntry(psCPD, psUFContext, uSymbolID, IMG_FALSE);
	}

	/* gl_SecondaryColor = gl_FrontSecondaryColor */
	if (psUFContext->bSecondaryColorUsed)
	{		
		/*
			gl_SecondaryColor
		*/
		if (!FindSymbol(psUFContext->psSymbolTable, "gl_SecondaryColor", &uSymbolID, IMG_FALSE))
		{
			LOG_INTERNAL_ERROR(("GenerateFaceSelection: Failed to find gl_SecondaryColor(PMX) in symbol table \n"));
			return IMG_FALSE;
		}

		/* Get the precision for gl_SecondaryColor */
		eGLSecondaryColorPrecision = ICGetSymbolPrecision(psCPD, psUFContext->psSymbolTable, uSymbolID);

		if(!AddBuiltInIdentifier(psCPD, psUFContext->psSymbolTable,
								 "gl_FrontSecondaryColor",
								 0,
								 GLSLBV_FRONTSECONDARYCOLOR,
								 GLSLTS_VEC4,
								 GLSLTQ_VERTEX_OUT,
								 eGLSecondaryColorPrecision,
								 &uSymbolID))
		{
			LOG_INTERNAL_ERROR(("GenerateFaceSelection: Failed to add PMX builtin variable gl_FrontSecondaryColor(PMX) to the symbol table !\n"));
			return IMG_FALSE;
		}

		GetHWSymbolEntry(psCPD, psUFContext, uSymbolID, IMG_FALSE);

		if(!AddBuiltInIdentifier(psCPD, psUFContext->psSymbolTable,
								 "gl_BackSecondaryColor",
								 0,
								 GLSLBV_BACKSECONDARYCOLOR,
								 GLSLTS_VEC4,
								 GLSLTQ_VERTEX_OUT,
								 eGLSecondaryColorPrecision,
								 &uSymbolID))
		{
			LOG_INTERNAL_ERROR(("GenerateFaceSelection: Failed to add PMX builtin variable gl_FrontSecondaryColor(PMX) to the symbol table !\n"));
			return IMG_FALSE;
		}

		GetHWSymbolEntry(psCPD, psUFContext, uSymbolID, IMG_FALSE);

	}

	return IMG_TRUE;
}
#endif


/*****************************************************************************
 FUNCTION	: AllocateAllVaryings

 PURPOSE	:

 PARAMETERS	:

 RETURNS	:
*****************************************************************************/
static IMG_BOOL	AllocateAllVaryings(GLSLCompilerPrivateData *psCPD, GLSLUniFlexContext *psUFContext)
{
	IMG_UINT32 uNumVaryings;
	HWSYMBOL  **apsVaryingList = psUFContext->sHWSymbolTab.apsVaryingList, *psHWSymbol;
	IMG_INT32 iLargestArraySize;
	IMG_UINT32 i, j;

#if defined(GLSL_ES)
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psUFContext->psICProgram);
	GLSLTexCoordsTable *psTable =  &psUFContext->sTexCoordsTable;


	/* Reserve tc0 for point sprite if there is a write to point size */
	if((psUFContext->eProgramType == GLSLPT_VERTEX) && (psICContext->eBuiltInsWrittenTo & GLSLBVWT_POINTSIZE))
	{
		/* DIM 2 is enough */
		IMG_UINT32 uReservedTexCoordDim = 2;

		psUFContext->sTexCoordsTable.auNextAvailRow = 1;
		psUFContext->sTexCoordsTable.auAllocatedBits[0] |= 3;

		psUFContext->auTexCoordDims[0]			= uReservedTexCoordDim;
		psUFContext->aeTexCoordPrecisions[0]	= GLSLPRECQ_LOW;
		psUFContext->eActiveVaryingMask			|= GLSLVM_TEXCOORD0;

		psTable->auTexCoordPrecisions[0]        = psUFContext->aeTexCoordPrecisions[0];
		psTable->aeTexCoordModidifierFlags[0]   = GLSLVMOD_NONE;
	}
#else

	if(psUFContext->bColorUsed || psUFContext->bSecondaryColorUsed)
	{
		AddExtraHWSymbolsForColorSelection(psCPD, psUFContext);
	}
#endif

	uNumVaryings = psUFContext->sHWSymbolTab.uNumVaryings;

#if PACK_VARYING_ARRAY
	/* If PACK_VARYING_ARRAY is enabled, we should allocate arrays first */
	for(j = 0; j < uNumVaryings; j++)
	{
		if(!apsVaryingList[j]->bRegAllocated)
		{
			if((apsVaryingList[j]->iArraySize > 1) || GLSL_IS_MAT_2ROWS(apsVaryingList[j]->sFullType.eTypeSpecifier) )
			{
				psHWSymbol = apsVaryingList[j];
				if(!AllocateVaryingRegisters(psCPD, psUFContext, psHWSymbol))
				{
					LOG_INTERNAL_ERROR(("AllocateAllVaryings: Failed to allocate registers for varyings\n"));
					return IMG_FALSE;
				}
			}
		}
	}
#endif

	for(i = 0; i < NUM_VARYING_TYPES; i++)
	{
		/* Find largest size of array for the type - this should be ok when PACK_ARRAY_VARYING is enabled, though it is not efficient */
		for(;;)
		{
			iLargestArraySize = -1;
			psHWSymbol = IMG_NULL;

			for(j = 0; j < uNumVaryings; j++)
			{
				if(!apsVaryingList[j]->bRegAllocated && (GLSLTypeSpecifier)apsVaryingList[j]->sFullType.eTypeSpecifier == auVaryingAllocOrder[i])
				{
					if(apsVaryingList[j]->iArraySize > iLargestArraySize)
					{
						iLargestArraySize = apsVaryingList[j]->iArraySize;
						psHWSymbol = apsVaryingList[j];
					}
				}
			}

			if(psHWSymbol)
			{
				if(!AllocateVaryingRegisters(psCPD, psUFContext, psHWSymbol))
				{
					LOG_INTERNAL_ERROR(("AllocateAllVaryings: Failed to allocate registers for varyings\n"));
					return IMG_FALSE;
				}
			}
			else
			{
				/* break out of the loop since no varying of this type is left */
				break;
			}
		}
	}

#ifdef DEBUG 
	for(i = 0; i < uNumVaryings; i++)
	{
		if(!apsVaryingList[i]->bRegAllocated)
		{
			LOG_INTERNAL_ERROR(("AllocateAllVaryings: Registers for %s have not been allocated\n", GetSymbolName(psUFContext->psSymbolTable, apsVaryingList[i]->uSymbolId)));
		}
	}
#endif

	/* Update texture coordinate dimension etc and store the information in context */
	UpdateTexCoordDimInfo(psUFContext);

	return IMG_TRUE;
}
#endif

/*****************************************************************************
 FUNCTION	: AllocateAllUFRegisters

 PURPOSE	: Allocate all UF registers for all symbols appear in the IC program

 PARAMETERS	: psUFContext	- UF context

 RETURNS	: TRUE if sucessful
*****************************************************************************/
static IMG_BOOL AllocateAllUFRegisters(GLSLCompilerPrivateData *psCPD, GLSLUniFlexContext *psUFContext)
{
	GLSLICInstruction *psCurrentInst;
	IMG_UINT32 i, j;
	HWSYMBOL *psOffsetHWSymbol, *psHWSymbol;

	/*
		Collect symbol usage information,
	*/
	psCurrentInst = psUFContext->psICProgram->psInstrHead;
	while(psCurrentInst)
	{		
		#ifdef SRC_DEBUG
		psCPD->uCurSrcLine = psCurrentInst->uSrcLine;
		psUFContext->uCurSrcLine = psCurrentInst->uSrcLine;
		#endif /* SRC_DEBUG */
		
		if(psCurrentInst->eOpCode != GLSLIC_OP_CALL &&
			psCurrentInst->eOpCode != GLSLIC_OP_LABEL)
		{
			for(i = 0; i < ICOP_NUM_SRCS(psCurrentInst->eOpCode) + 1; i++)
			{
				if(!ICOP_HAS_DEST(psCurrentInst->eOpCode) && i == DEST) continue;

				/* Add the symbol of operands to the HW symbol list */
				psHWSymbol = GetHWSymbolEntry(psCPD, psUFContext, psCurrentInst->asOperand[i].uSymbolID, IMG_FALSE);

				if(psHWSymbol == IMG_NULL)
				{
					LOG_INTERNAL_ERROR(("AllocateAllUFRegisters: Failed to get HW symbol"));

					return IMG_FALSE;
				}

				/* Add the symbol of offsets to the HW symbol list */
				for(j = 0; j < psCurrentInst->asOperand[i].uNumOffsets; j++)
				{
					if(psCurrentInst->asOperand[i].psOffsets[j].uOffsetSymbolID)
					{
						psOffsetHWSymbol = GetHWSymbolEntry(psCPD, psUFContext, psCurrentInst->asOperand[i].psOffsets[j].uOffsetSymbolID, IMG_FALSE);

						if(psOffsetHWSymbol == IMG_NULL)
						{
							LOG_INTERNAL_ERROR(("AllocateAllUFRegisters: Failed to get HW symbol"));

							return IMG_FALSE;
						}

						/* Mark the symbol of operand is dynamically indexed */
						psHWSymbol->eSymbolUsage |= GLSLSU_DYNAMICALLY_INDEXED;
					}
				}

				/*
					Check whether the current operand contains array assignment or array comparision,
					and whether the operands contains big arrays, if so we need to mark the symbol
					as dynamically indexed.
				*/
				if(!psHWSymbol->eSymbolUsage & GLSLSU_DYNAMICALLY_INDEXED &&	/* has not marked as dynamically indexed */
					(psCurrentInst->eOpCode == GLSLIC_OP_MOV ||					/* Assignment */
					 psCurrentInst->eOpCode == GLSLIC_OP_SEQ ||					/* Or equality comparison */
					 psCurrentInst->eOpCode == GLSLIC_OP_SNE) )					/* Or non-equality comparison */
				{
					GLSLFullySpecifiedType sOperandType = psHWSymbol->sFullType;
					IMG_INT32 iArraySize = psHWSymbol->iArraySize;

					/* Work out the operand type */
					if(psCurrentInst->asOperand[i].uNumOffsets)
					{
						sOperandType = GetFinalIndexedType(psCPD, psUFContext->psSymbolTable,
														   sOperandType,
														   &iArraySize,
														   psCurrentInst->asOperand[i].uNumOffsets,
														   psCurrentInst->asOperand[i].psOffsets);
					}

					/* Whether this type contains big arrays */
					if(DoesTypeContainBigArrays(psCPD, psUFContext, &sOperandType, iArraySize))
					{
						psHWSymbol->eSymbolUsage |= GLSLSU_DYNAMICALLY_INDEXED;
					}
				}

				/* if src is integer, mark it to be full range */ 
				if(i && GLSL_IS_INT(psHWSymbol->sFullType.eTypeSpecifier))
				{
					psHWSymbol->eSymbolUsage |= GLSLSU_FULL_RANGE_INTEGER;
				}

			}
		}

		/* Predicated symbol */
		if(psCurrentInst->uPredicateBoolSymID)
		{
			if(psCurrentInst->eOpCode == GLSLIC_OP_LOOP ||
			   psCurrentInst->eOpCode == GLSLIC_OP_STATICLOOP ||
			   psCurrentInst->eOpCode == GLSLIC_OP_ENDLOOP ||
			   psCurrentInst->eOpCode == GLSLIC_OP_SEQ ||
			   psCurrentInst->eOpCode == GLSLIC_OP_SNE ||
			   psCurrentInst->eOpCode == GLSLIC_OP_SLT ||
			   psCurrentInst->eOpCode == GLSLIC_OP_SLE ||
			   psCurrentInst->eOpCode == GLSLIC_OP_SGT ||
			   psCurrentInst->eOpCode == GLSLIC_OP_SGE ) 
			{
				psHWSymbol = GetHWSymbolEntry(psCPD, psUFContext, psCurrentInst->uPredicateBoolSymID, IMG_FALSE);

				if(psHWSymbol == IMG_NULL)
				{
					LOG_INTERNAL_ERROR(("AllocateAllUFRegisters: Failed to get predicate HW symbol"));

					return IMG_FALSE;
				}
			}
			else
			{
				LOG_INTERNAL_ERROR(("AllocateAllUFRegisters: IC instruction %s doesn't support predicate", ICOP_DESC(psCurrentInst->eOpCode)));
			}
		}

		psCurrentInst = psCurrentInst->psNext;
	}


#ifdef OGLES2_VARYINGS_PACKING_RULE

	/* Allocate registers for all varyings */
	if(!AllocateAllVaryings(psCPD, psUFContext))
	{
		return IMG_FALSE;
	}
#endif

	/* Allocate registers for the rest of symbols */
	psHWSymbol = psUFContext->sHWSymbolTab.psHWSymbolHead;
	while(psHWSymbol)
	{
		if(!psHWSymbol->bRegAllocated)
		{
			AllocateHWSymbolRegisters(psCPD, psUFContext, psHWSymbol);
		}

		psHWSymbol = psHWSymbol->psNext;
	}

	return IMG_TRUE;

}

/*****************************************************************************
 FUNCTION	: GenerateUniFlexEntryLabel

 PURPOSE	: Generates entry label for the shader.

 PARAMETERS	: psUFContext	- UF context

 RETURNS	: Always IMG_TRUE
*****************************************************************************/
static IMG_BOOL GenerateUniFlexEntryLabel(GLSLUniFlexContext  *psUFContext)
{
	PUNIFLEX_INST  psOldEnd, psLabelInst = IMG_NULL;
	
	psOldEnd = psUFContext->psEndUFInst;

	/* Set source line information for entry label */
	#ifdef SRC_DEBUG
	if(psUFContext->psFirstUFInst)
	{
		psUFContext->uCurSrcLine = 
			psUFContext->psFirstUFInst->uSrcLine;
	}
	#endif /* SRC_DEBUG */

	/* Create an entry label */
	psLabelInst = CreateInstruction(psUFContext, UFOP_LABEL);		if(!psLabelInst) return IMG_FALSE;
	psLabelInst->asSrc[0].eType = UFREG_TYPE_LABEL;
	psLabelInst->asSrc[0].uNum = USC_MAIN_LABEL_NUM;

	/* Move the entry label to the begining of the program */
	psUFContext->psEndUFInst->psILink = psUFContext->psFirstUFInst;
	psUFContext->psFirstUFInst = psOldEnd->psILink;
	psOldEnd->psILink = IMG_NULL;
	psUFContext->psEndUFInst = psOldEnd;

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: GenerateUniFlexInput

 PURPOSE	: Generate UniFlex input code for a program in the GLSL intermediate code format.

 PARAMETERS	: psProgram		- Program to convert.

 RETURNS	: Pointer to GLSLUniFlexCode
*****************************************************************************/
IMG_INTERNAL GLSLUniFlexCode* GenerateUniFlexInput(GLSLCompilerPrivateData *psCPD, GLSLICProgram* psProgram,
												   GLSLBindingSymbolList **ppsBindingSymbolList)
{
	GLSLUniFlexContext	*psUFContext;
	GLSLUniFlexCode		*psUniFlexCode = IMG_NULL;
	IMG_BOOL			bSuccess = IMG_TRUE;

	/* Create UniFlex context */
	psUFContext = CreateUniFlexContext(psCPD, psProgram);
	if(psUFContext == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("GenerateUniFlexInput: Failed to create uniflex context \n"));
		bSuccess = IMG_FALSE;
		goto UFGenerateTidyUp;
	}

	/* Allocate registers */
	if(!AllocateAllUFRegisters(psCPD, psUFContext))
	{
		LOG_INTERNAL_ERROR(("GenerateUniFlexInput: Failed to allocate all registers \n"));
		bSuccess = IMG_FALSE;
		goto UFGenerateTidyUp;
	}

#ifdef OGLES2_VARYINGS_PACKING_RULE
	/* Generate face selection code */
	if(psUFContext->bColorUsed || psUFContext->bSecondaryColorUsed || psUFContext->bFrontFacingUsed)
	{
		GenerateFaceSelection(psCPD, psUFContext);
	}

#if defined(DEBUG) && defined(DUMP_LOGFILES)
	DumpMultipleUFInsts(psCPD, psUFContext->psFirstUFInst, psUFContext->psEndUFInst);
#endif

#endif

	/* Main function to convert IC to UF code */
	if(!GenerateUniFlexInstructions(psCPD, psUFContext))
	{
		LOG_INTERNAL_ERROR(("GenerateUniFlexInput: Failed to generate UniFlex instructions \n"));
		bSuccess = IMG_FALSE;
		goto UFGenerateTidyUp;
	}

#ifdef DUMP_LOGFILES
	FlushLogFiles();
#endif

	if(!PostGenerateUniFlexCode(psCPD, psUFContext))
	{
		LOG_INTERNAL_ERROR(("GenerateUniFlexInput: Failed to post process \n"));
		bSuccess = IMG_FALSE;
		goto UFGenerateTidyUp;
	}

	if(!GenerateUniFlexEntryLabel(psUFContext))
	{
		LOG_INTERNAL_ERROR(("GenerateUniFlexInput: Failed to create entry label \n"));
		bSuccess = IMG_FALSE;
		goto UFGenerateTidyUp;
	}

	if (psUFContext->bRegisterLimitExceeded)
	{
		LogProgramError(psCPD->psErrorLog, "Shader needs too many registers.\n");

		bSuccess = IMG_FALSE;

		goto UFGenerateTidyUp;
	}

#ifdef DUMP_LOGFILES

	DumpStructAllocInfoTable(psUFContext);

	DumpHWSymbols(psCPD, psUFContext);


	DumpRangeList(&psUFContext->sConstRanges, "constant");

	DumpRangeList(&psUFContext->sVaryingRanges, "varying");

	DumpIndexableTempArraySizes(psUFContext);

	/*
		Dump the unpacked program.
	*/
	DumpLogMessage(LOGFILE_COMPILER, 0, "\n-------------- Program after translation --------------\n");

	DumpUniFlextInstData(NULL, psUFContext->psFirstUFInst);

	FlushLogFiles();
#endif /* DUMP_LOGFILES */

#ifdef DEBUG
	if(CheckProgramPrecisionMismatch(psUFContext))
	{
		LOG_INTERNAL_ERROR(("GenerateUniFlexInput: Precision mismatch/unknown in some uf instructions\n"));
	}
#endif

	/* Prepare Uniflex output */
	psUniFlexCode = PrepareUFCodeOutput(psCPD, psUFContext);
	if(psUniFlexCode == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("GenerateUniFlexInput: Failed to generate UF code output \n"));
		bSuccess = IMG_FALSE;
	}

	/* Generate binding symbol list */
	*ppsBindingSymbolList = GenerateBindingSymbolList(psCPD, psUFContext);
	if(*ppsBindingSymbolList == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("GLSLToUniflex: Failed to generate binding symbol list \n"));
		goto UFGenerateTidyUp;	
	}

UFGenerateTidyUp:

	DestroyUniFlexContext(psCPD, psUFContext, bSuccess);

#ifdef DUMP_LOGFILES
	FlushLogFiles();
#endif

	return psUniFlexCode;
}



#ifdef GEN_HW_CODE

#if defined (DEBUG)
/* Array of strings indexed by UniFlex error codes */
static char * const gauUniFlexErrorStrings[] =
{
	"UF_OK",
	"UF_ERR_INVALID_OPCODE",
	"UF_ERR_INVALID_DST_REG",
	"UF_ERR_INVALID_SRC_REG",
	"UF_ERR_INVALID_DST_MOD",
	"UF_ERR_INVALID_SRC_MOD",
	"UF_ERR_ADDR_EXCEEDED",
	"UF_ERR_BLEND_EXCEEDED",
	"UF_ERR_GENERIC",
	"UF_ERR_INTERNAL",
	"UF_ERR_MCPACKFAILED"
};
#endif /* DEBUG */

#if defined(OUTPUT_USPBIN)

/*****************************************************************************
 FUNCTION	: GenerateUniPatchInput

 PURPOSE	: Generate Unipatch input code from uniflex input code 

 PARAMETERS	: psUniFlexCode			- UniFlex code 
			  eProgramType			- Program type, vertex or fragment
			  eProgramFlags			- Program flags
			  bCompileMSAATrans		- Do an extra compile for MSAA Translucent mode
			  psUniflexHWCodeInfo	- Uniflex HW code info to be used to generate HW code

 RETURNS	: IMG_TRUE if successful 
*****************************************************************************/
IMG_INTERNAL IMG_BOOL GenerateUniPatchInput(GLSLCompilerPrivateData *psCPD,
											GLSLUniFlexCode			*psUniFlexCode,
											IMG_VOID				*pvUniFlexContext,
											IMG_FLOAT				*pfDefaultConstantData,
											GLSLProgramType			eProgramType,
											GLSLProgramFlags		*peProgramFlags,
											IMG_BOOL				bCompileMSAATrans,
											GLSLUniFlexHWCodeInfo	*psUniflexHWCodeInfo)
{
	IMG_UINT32 uUniFlexError;
	UNIFLEX_PROGRAM_PARAMETERS *psUFParams          = psUniflexHWCodeInfo->psUFParams;
	PUNIFLEX_DIMENSIONALITY		psTextureDimensions = psUniFlexCode->asSamplerDims;
	GLSLProgramFlags eProgramFlags = *peProgramFlags;
	IMG_UINT32 ui32FlagsOut = 0;
	IMG_BOOL bSuccess = IMG_TRUE;

	#ifdef SRC_DEBUG
	UNIFLEX_HW        *psUniFlexHW;
	#endif /* SRC_DEBUG */

	 #if defined(UF_TESTBENCH)
        UNIFLEX_VERIF_HELPER    sVerifHelper = {0, };
        #endif /* defined(UF_TESTBENCH) */


	#ifdef SRC_DEBUG
	psUniFlexHW = DebugMemCalloc(sizeof(UNIFLEX_HW));
	if(psUniFlexHW == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("GenerateUniPatchInput: Failed to alloc memory \n"));
		return IMG_FALSE;
	}
	memset(psUniFlexHW, 0, sizeof(UNIFLEX_HW));
	/* Initalize the table for source line cycle count */
	psUniFlexHW->asTableSourceCycle.uTableCount = 0; 
	psUniFlexHW->asTableSourceCycle.uTotalCycleCount = 0;
	psUniFlexHW->asTableSourceCycle.psSourceLineCost = IMG_NULL;
	psUniFlexCode->psUniFlexHW = psUniFlexHW;
	#endif /* SRC_DEBUG */

	if(psUniFlexCode->pvUFCode)
	{
		UNIFLEX_CONSTDEF	sConstants;
		UNIFLEX_CONSTBUFFERDESC *psLegacyBuffer;
		UNIFLEX_RANGES_LIST *psConstRange;		
		IMG_UINT32 uFeedbackInstCount = 0;

		psUFParams->uFlags = UF_GLSL;
		psUFParams->uFlags2 = 0;
#if defined(GLSL_ES)

#if !defined(FIX_HW_BRN_21123) && !defined(FIX_HW_BRN_21442) && !defined(FIX_HW_BRN_22048) && !defined(FIX_HW_BRN_22287)
		(psUFParams->uFlags) |=  UF_EXTRACTCONSTANTCALCS;
#endif
		
		if(eProgramFlags & GLSLPF_DISCARD_EXECUTED)
		{
			(psUFParams->uFlags) |= UF_SPLITFEEDBACK | UF_NOALPHATEST;
			uFeedbackInstCount = 1;
#if defined(FIX_HW_BRN_33668)
			uFeedbackInstCount += 1;
#endif
		}
#else
		PVR_UNREFERENCED_PARAMETER(eProgramFlags);
#endif

		/*
			Allow unlimited instruction movement.
		*/
		psUFParams->uMaxInstMovement		  = USC_MAXINSTMOVEMENT_NOLIMIT;

		/* Secondary attribute offsets */
		psUFParams->uIndexableTempArrayCount  = psUniFlexCode->uNumIndexableTempArrays;
		psUFParams->psIndexableTempArraySizes = psUniFlexCode->psIndexableTempArraySizes;
		psUFParams->uCentroidMask			  = psUniFlexCode->uTexCoordCentroidMask;
		psUFParams->sTarget.eID				  = SGX_CORE_ID;
#if defined (SGX_CORE_REV)
		psUFParams->sTarget.uiRev			  = SGX_CORE_REV;
#else
		psUFParams->sTarget.uiRev			  = 0;		/* use head revision */
#endif		
		psUFParams->puValidShaderOutputs[0]		  = (IMG_UINT32)-1;
		psUFParams->puValidShaderOutputs[1]		  = (IMG_UINT32)-1;
		{
			IMG_UINT32	uIdx;
			for(uIdx = 2; uIdx < USC_SHADER_OUTPUT_MASK_DWORD_COUNT; uIdx++)
			{
				psUFParams->puValidShaderOutputs[uIdx]		  = 0;
			}
		}		
		psUFParams->uFeedbackInstCount		  = uFeedbackInstCount;
	
		/*	
			Information about ranges of dynamically indexed constants.
		*/
		psLegacyBuffer = &psUFParams->asConstBuffDesc[UF_CONSTBUFFERID_LEGACY];
		psConstRange = &psLegacyBuffer->sConstsBuffRanges;
		*psConstRange = psUniFlexCode->sConstRanges;
		if (psConstRange->uRangesCount > 0)
		{
			(psUFParams->uFlags) |= UF_CONSTRANGES;
		}
		
		psLegacyBuffer->eConstBuffLocation = UF_CONSTBUFFERLOCATION_DONTCARE;
		psLegacyBuffer->uBaseAddressSAReg = 0xFFFFFFFF;
		psLegacyBuffer->uStartingSAReg = 0xFFFFFFFF;

		if (eProgramType == GLSLPT_VERTEX)
		{
			psUFParams->eShaderType = USC_SHADERTYPE_VERTEX;
				
			psUFParams->sShaderOutPutRanges.psRanges = (PUNIFLEX_RANGE)	DebugMemAlloc(sizeof(psUFParams->sShaderOutPutRanges.psRanges[0]));
			
			if(psUFParams->sShaderOutPutRanges.psRanges == IMG_NULL)
			{
				LOG_INTERNAL_ERROR(("GenerateUniFlexOutput: Failed to allcate memory"));
				return IMG_FALSE;
			}
			psUFParams->sShaderOutPutRanges.psRanges[0].uRangeStart = 0;
			psUFParams->sShaderOutPutRanges.psRanges[0].uRangeEnd = 63;
			psUFParams->sShaderOutPutRanges.uRangesCount = 1;
		}
		else
		{
			psUFParams->ePackDestFormat = UF_REGFORMAT_U8;
			psUFParams->eShaderType = USC_SHADERTYPE_PIXEL;
			psUFParams->sShaderOutPutRanges.psRanges = IMG_NULL;
			psUFParams->sShaderOutPutRanges.uRangesCount = 0;
		}

		/*
			Copy information about ranges of dynamically addressed varyings.
		*/
		psUFParams->sShaderInputRanges = psUniFlexCode->sVaryingRanges;

		/* Reserved the default number of PDS constants. */
		psUFParams->uNumPDSPrimaryConstantsReserved = EURASIA_PDS_DOUTU_NONLOOPBACK_STATE_SIZE;

		/* No static constants to set up */
		sConstants.uCount			  = psUniFlexCode->uConstStaticFlagCount;
		sConstants.puConstStaticFlags = psUniFlexCode->puConstStaticFlags;
		sConstants.pfConst			  = pfDefaultConstantData;

		/*
			Don't reserve any space for driver generated instructions.
		*/
		psUFParams->uPreFeedbackPhasePreambleCount	 = 0;
		psUFParams->uPostFeedbackPhasePreambleCount	 = 0;

		/*
			Copy the dimensionality of each sampler.
		*/
		psUFParams->asTextureDimensionality = psTextureDimensions;

		/*
			Set up information about parameters for sampling each texture.
		*/
		psUFParams->uTextureCount = psUniFlexCode->uSamplerCount;
		psUFParams->asTextureParameters = 
			DebugMemCalloc(sizeof(psUFParams->asTextureParameters[0]) * psUniFlexCode->uSamplerCount);
		{
			IMG_UINT32	uSampler;
			for (uSampler = 0; uSampler < psUniFlexCode->uSamplerCount; uSampler++)
			{
				PVRUniFlexInitTextureParameters(&psUFParams->asTextureParameters[uSampler]);
#if defined(SGX_FEATURE_USE_VEC34)
				psUFParams->asTextureParameters[uSampler].sFormat.uSwizzle = UFREG_SWIZ_RGBA;
#else
				psUFParams->asTextureParameters[uSampler].sFormat.uSwizzle = UFREG_SWIZ_BGRA;
#endif				
			}
		}

		/* Generate intermediate instructions */
		psUniFlexCode->psUniPatchInput = IMG_NULL;

		uUniFlexError = PVRUniFlexCompileToUspBin(pvUniFlexContext,
													&ui32FlagsOut,
													psUniFlexCode->pvUFCode,
													&sConstants,
													psUFParams,
													#if defined(UF_TESTBENCH)
		                                                                                        &sVerifHelper,
                		                                                                        #endif /* defined(UF_TESTBENCH) */
													&psUniFlexCode->psUniPatchInput
													#ifdef SRC_DEBUG
													, &(psUniFlexHW->asTableSourceCycle)
													#endif /* SRC_DEBUG */
													);

		if(uUniFlexError != UF_OK || !psUniFlexCode->psUniPatchInput)
		{
			psUniFlexCode->psUniPatchInput = IMG_NULL;
			LOG_INTERNAL_ERROR(("GenerateHWCode: Failed to compile to unipatch code\nError is %s\n", gauUniFlexErrorStrings[uUniFlexError]));
			bSuccess = IMG_FALSE;
			goto memfree_return;
		}
		
		if(bCompileMSAATrans)
		{
			IMG_UINT32 ui32FlagsOut2 = 0;
			/* Compile to temps as PAs are not writable in MSAA 4x mode */
			psUFParams->uPackDestType = USEASM_REGTYPE_TEMP;

			(psUFParams->uFlags) |= UF_MSAATRANS;

			/* Need to restrict the number of temps, as they are referenced per sample */
			psUFParams->uNumAvailableTemporaries >>= 2;

			/* Generate intermediate instructions */
			psUniFlexCode->psUniPatchInputMSAATrans = IMG_NULL;
			uUniFlexError = PVRUniFlexCompileToUspBin(pvUniFlexContext,
														&ui32FlagsOut2,
														psUniFlexCode->pvUFCode,
														&sConstants,
														psUFParams,
														#if defined(UF_TESTBENCH)
														&sVerifHelper,
														#endif /* defined(UF_TESTBENCH) */
														&psUniFlexCode->psUniPatchInputMSAATrans
														#ifdef SRC_DEBUG
														, IMG_NULL
														#endif /* SRC_DEBUG */
														);

			if(uUniFlexError != UF_OK || !psUniFlexCode->psUniPatchInputMSAATrans)
			{
				psUniFlexCode->psUniPatchInputMSAATrans = IMG_NULL;
				LOG_INTERNAL_ERROR(("GenerateHWCode: Failed to compile to unipatch code\nError is %s\n", gauUniFlexErrorStrings[uUniFlexError]));
				bSuccess = IMG_FALSE;
				goto memfree_return;
			}
		}
		else
		{
			psUniFlexCode->psUniPatchInputMSAATrans = IMG_NULL;
		}

memfree_return:
		/*
			Free texture sampling parameters.
		*/
		if (psUFParams->asTextureParameters != NULL)
		{
			DebugMemFree(psUFParams->asTextureParameters);
			psUFParams->asTextureParameters = NULL;
		}

		if(psUFParams->sShaderOutPutRanges.psRanges != IMG_NULL)
		{
			DebugMemFree(psUFParams->sShaderOutPutRanges.psRanges);
			psUFParams->sShaderOutPutRanges.psRanges = IMG_NULL;
		}
	}

	if(ui32FlagsOut & UNIFLEX_HW_FLAGS_TEXKILL_USED)
	{
		DebugAssert(eProgramFlags & GLSLPF_DISCARD_EXECUTED);
	}
	else
	{
		*peProgramFlags &= ~GLSLPF_DISCARD_EXECUTED;
	}
	
	return bSuccess;
}

#else /* OUTPUT_USPBIN */

/*****************************************************************************
 FUNCTION	: GenerateUniFlexOutput

 PURPOSE	: Generate Uniflex output code from uniflex input code 

 PARAMETERS	: psUniFlexCode			- UniFlex code 
			  eProgramType			- Program type, vertex or fragment
			  psUniflexHWCodeInfo	- Uniflex HW code info to be used to generate HW code

 RETURNS	: IMG_TRUE if successful 
*****************************************************************************/
IMG_INTERNAL IMG_BOOL GenerateUniFlexOutput(GLSLCompilerPrivateData *psCPD,
											GLSLUniFlexCode			*psUniFlexCode,
											IMG_VOID				*pvUniFlexContext,
											IMG_FLOAT				*pfDefaultConstantData,
											GLSLProgramType			eProgramType,
											GLSLUniFlexHWCodeInfo	*psUniflexHWCodeInfo)
{
	IMG_UINT32 uUniFlexError;
#ifdef DUMP_LOGFILES
	IMG_UINT32 i;
#endif
	UNIFLEX_PROGRAM_PARAMETERS	*psUFParams          = psUniflexHWCodeInfo->psUFParams;
	PUNIFLEX_DIMENSIONALITY		psTextureDimensions = psUniFlexCode->asSamplerDims;
	UNIFLEX_CONSTBUFFERDESC		*psLegacyBuffer;
	UNIFLEX_RANGES_LIST			*psConstRange;

	if(psUniFlexCode->pvUFCode)
	{
		UNIFLEX_CONSTDEF			 sConstants;
		/* Constant hoisting is supported for the GLSL ES compiler */
		UNIFLEX_HW					 *psUniflexHW;
		(psUFParams->uFlags) = UF_GLSL | UF_EXTRACTCONSTANTCALCS
			;

		psUniflexHW = DebugMemCalloc(sizeof(UNIFLEX_HW));
		if(psUniflexHW == IMG_NULL)
		{
			LOG_INTERNAL_ERROR(("GenerateUniFlexOutput: Failed to alloc memory \n"));
			return IMG_FALSE;
		}

		/* Initalize the table for source line cycle count */
		#ifdef SRC_DEBUG
		psUniflexHW->asTableSourceCycle.uTableCount = 0; 
		psUniflexHW->asTableSourceCycle.uTotalCycleCount = 0;
		psUniflexHW->asTableSourceCycle.psSourceLineCost = IMG_NULL;
		#endif /* SRC_DEBUG */

		/*
			Allow unlimited instruction movement.
		*/
		psUFParams->uMaxInstMovement		  = USC_MAXINSTMOVEMENT_NOLIMIT;

		/* Secondary attribute offsets */
		psUFParams->uIndexableTempArrayCount  = psUniFlexCode->uNumIndexableTempArrays;
		psUFParams->psIndexableTempArraySizes = psUniFlexCode->psIndexableTempArraySizes;
		psUFParams->uCentroidMask			  = psUniFlexCode->uTexCoordCentroidMask;
		psUFParams->sTarget.eID				  = SGX_CORE_ID;
#if defined (SGX_CORE_REV)
		psUFParams->sTarget.uiRev			  = SGX_CORE_REV;
#else
		psUFParams->sTarget.uiRev			  = 0;		/* use head revision */
#endif
		psUFParams->puValidShaderOutputs[0]		  = (IMG_UINT32)-1;
		psUFParams->puValidShaderOutputs[1]		  = (IMG_UINT32)-1;
		{
			IMG_UINT32	uIdx;
			for(uIdx = 2; uIdx < USC_SHADER_OUTPUT_MASK_DWORD_COUNT; uIdx++)
			{
				psUFParams->puValidShaderOutputs[uIdx]		  = 0;
			}
		}
		
		psLegacyBuffer = &psUFParams->asConstBuffDesc[UF_CONSTBUFFERID_LEGACY];

		/*
			Copy information about ranges of dynamically indexed constants.
		*/
		psConstRange = &psLegacyBuffer->sConstsBuffRanges;
		*psConstRange = psUniFlexCode->sConstRanges;
		if (psConstRange->uRangesCount > 0)
		{
			(psUFParams->uFlags) |= UF_CONSTRANGES;
		}
		
		psLegacyBuffer->eConstBuffLocation = UF_CONSTBUFFERLOCATION_DONTCARE;
		psLegacyBuffer->uBaseAddressSAReg = 0xFFFFFFFF;
		psLegacyBuffer->uStartingSAReg = 0xFFFFFFFF;

		if (eProgramType == GLSLPT_VERTEX)
		{
			psUFParams->eShaderType = USC_SHADERTYPE_VERTEX;
				
			psUFParams->sShaderOutPutRanges.psRanges = (PUNIFLEX_RANGE)	DebugMemAlloc(sizeof(psUFParams->sShaderOutPutRanges.psRanges[0]));
			
			if(psUFParams->sShaderOutPutRanges.psRanges == IMG_NULL)
			{
				LOG_INTERNAL_ERROR(("GenerateUniFlexOutput: Failed to allcate memory"));
				return IMG_FALSE;
			}
			psUFParams->sShaderOutPutRanges.psRanges[0].uRangeStart = 0;
			psUFParams->sShaderOutPutRanges.psRanges[0].uRangeEnd = 63;
			psUFParams->sShaderOutPutRanges.uRangesCount = 1;
		}
		else
		{
			psUFParams->ePackDestFormat = UF_REGFORMAT_U8;
			psUFParams->eShaderType = USC_SHADERTYPE_PIXEL;
			psUFParams->sShaderOutPutRanges.psRanges = IMG_NULL;
			psUFParams->sShaderOutPutRanges.uRangesCount = 0;
		}

		/*
			Copy information about ranges of dynamically indexed varyings.
		*/
		psUFParams->sShaderInputRanges = psUniFlexCode->sVaryingRanges;

		/* Reserved the default number of PDS constants. */
		psUFParams->uNumPDSPrimaryConstantsReserved = EURASIA_PDS_DOUTU_NONLOOPBACK_STATE_SIZE;

		/* No static constants to set up */
		sConstants.uCount			  = psUniFlexCode->uConstStaticFlagCount;
		sConstants.puConstStaticFlags = psUniFlexCode->puConstStaticFlags;
		sConstants.pfConst			  = pfDefaultConstantData;

		/* Set these to zero to force uniflex to do the malloc */
		psUniflexHW->uMaximumInstructionCount = 0;
		psUniflexHW->puInstructions           = IMG_NULL;

		/*
			Don't reserve any space for driver generated instructions.
		*/
		psUFParams->uPreFeedbackPhasePreambleCount	  = 0;
		psUFParams->uPostFeedbackPhasePreambleCount	  = 0;

		/*
			Copy the dimensionality of each sampler.
		*/
		psUFParams->asTextureDimensionality = psTextureDimensions;

		/*
			Set up information about parameters for sampling each texture.
		*/
		psUFParams->uTextureCount = psUniFlexCode->uSamplerCount;
		psUFParams->asTextureParameters = 
			DebugMemCalloc(sizeof(psUFParams->asTextureParameters[0]) * psUniFlexCode->uSamplerCount);
		{
			IMG_UINT32	uSampler;
			for (uSampler = 0; uSampler < psUFParams->uTextureCount; uSampler++)
			{
				PVRUniFlexInitTextureParameters(&psUFParams->asTextureParameters[uSampler]);
#if defined(SGX543)
				psUFParams->asTextureParameters[uSampler].sFormat.uSwizzle = UFREG_SWIZ_RGBA;
#else
				psUFParams->asTextureParameters[uSampler].sFormat.uSwizzle = UFREG_SWIZ_BGRA;
#endif
			}
		}

		/*
			GLSL doesn't use the projected coordinates features.
		*/
		memset(psUFParams->auProjectedCoordinateMask, 0, sizeof(psUFParams->auProjectedCoordinateMask));

		/* Generate intermediate instructions */
		uUniFlexError = PVRUniFlexCompileToHw(pvUniFlexContext,
											  psUniFlexCode->pvUFCode,
											  &sConstants,
											  psUFParams,
											  psUniflexHW);

		/*
			Free texture sampling parameters.
		*/
		if (psUFParams->asTextureParameters != NULL)
		{
			DebugMemFree(psUFParams->asTextureParameters);
			psUFParams->asTextureParameters = NULL;
		}

		/*
			Free information about shader output ranges.
		*/
		if(psUFParams->sShaderOutPutRanges.psRanges != IMG_NULL)
		{
			DebugMemFree(psUFParams->sShaderOutPutRanges.psRanges);
			psUFParams->sShaderOutPutRanges.psRanges = IMG_NULL;
		}

		if(uUniFlexError != UF_OK)
		{
			LOG_INTERNAL_ERROR(("GenerateHWCode: Failed to compile to hw code\nError is %s\n", gauUniFlexErrorStrings[uUniFlexError]));
			DebugMemFree(psUniflexHW);
			return IMG_FALSE;
		}

#ifdef DUMP_LOGFILES
		DumpLogMessage(LOGFILE_COMPILER, 0, "Generate HW code successfully \n");
		DumpLogMessage(LOGFILE_COMPILER, 0, "  Number of USE instructions     : %u\n", psUniflexHW->uInstructionCount);
		DumpLogMessage(LOGFILE_COMPILER, 0, "  Number of primary attributes   : %u\n", psUniflexHW->uPrimaryAttributeCount);
		DumpLogMessage(LOGFILE_COMPILER, 0, "  Number of temp registers       : %u\n", psUniflexHW->asPhaseInfo[0].uTemporaryRegisterCount);
		DumpLogMessage(LOGFILE_COMPILER, 0, "  Constant count                 : %u\n", psUniflexHW->psMemRemappingForConstsBuff[UF_CONSTBUFFERID_LEGACY].uConstCount);
		DumpLogMessage(LOGFILE_COMPILER, 0, "  Program start                  : %u\n", psUniflexHW->asPhaseInfo[0].uPhaseStart);
		DumpLogMessage(LOGFILE_COMPILER, 0, "  Texkill used                   : %s\n", (psUniflexHW->uFlags & UNIFLEX_HW_FLAGS_TEXKILL_USED ) ? "Yes": "No");
		DumpLogMessage(LOGFILE_COMPILER, 0, "  Label at end                   : %s\n", (psUniflexHW->uFlags & UNIFLEX_HW_FLAGS_LABEL_AT_END ) ? "Yes": "No");
		DumpLogMessage(LOGFILE_COMPILER, 0, "  Per instance mode              : %s\n", (psUniflexHW->uFlags & UNIFLEX_HW_FLAGS_PER_INSTANCE_MODE ) ? "Yes": "No");
		DumpLogMessage(LOGFILE_COMPILER, 0, "  Depth feedback                 : %s\n", (psUniflexHW->uFlags & UNIFLEX_HW_FLAGS_DEPTHFEEDBACK ) ? "Yes": "No");
		DumpLogMessage(LOGFILE_COMPILER, 0, "  Contain texture read           : %s\n", (psUniflexHW->uFlags & UNIFLEX_HW_FLAGS_TEXTUREREADS ) ? "Yes": "No");
		DumpLogMessage(LOGFILE_COMPILER, 0, "  Number of SA prog instrs       : %u\n", psUniflexHW->uSAProgInstructionCount);
		DumpLogMessage(LOGFILE_COMPILER, 0, "  Indexable temp data size bytes : %u\n\n",(psUFParams->uIndexableTempArrayCount ? psUniflexHW->uIndexableTempTotalSize : 0)); 

		DumpLogMessage(LOGFILE_COMPILER, 0, "Number of constants              : %u\n", psUniflexHW->psMemRemappingForConstsBuff[UF_CONSTBUFFERID_LEGACY].uConstCount);

		DumpLogMessage(LOGFILE_COMPILER, 0, "\n");

		DumpLogMessage(LOGFILE_COMPILER, 0, "Number of constants moved to SA  :%u\n",
											psUniflexHW->uInRegisterConstCount);

		DumpLogMessage(LOGFILE_COMPILER, 0, "\n");

		if(psUniflexHW->uNrNonDependentTextureLoads)
		{
			DumpLogMessage(LOGFILE_COMPILER, 0, "Number of iterations             : %u\n", psUniflexHW->uNrNonDependentTextureLoads);

			for(i = 0; i < psUniflexHW->uNrNonDependentTextureLoads; i++)
			{
				IMG_CHAR	apcTempString[512];

				PVRUniFlexDecodeIteration(&psUniflexHW->psNonDependentTextureLoads[i], apcTempString);
				DumpLogMessage(LOGFILE_COMPILER, 0, "%-5d %s\n", i, apcTempString);
			}
		}

		DumpLogMessage(LOGFILE_COMPILER, 0, "\n");

#endif

		psUniFlexCode->psUniFlexHW = psUniflexHW;

#if 0
		/* Add emit instruction missing from uniflex compile */
		if(eProgramType == GLSLPT_VERTEX)
		{
			IMG_UINT32 ui32Instructions = psUniflexHW->uInstructionCount + 1;
			IMG_UINT32 i;

			psUniflexHW->puInstructions = DebugMemRealloc(psUniflexHW->puInstructions,
										ui32Instructions * EURASIA_USE_INSTRUCTION_SIZE);

			if(!psUniflexHW->puInstructions)
			{
				LOG_INTERNAL_ERROR(("GenerateHWCode: Failed to realloc hw code"));
				return IMG_FALSE;
			}

			/* Need UINT32 position to write extra instructions */
			i = (psUniflexHW->uInstructionCount * EURASIA_USE_INSTRUCTION_SIZE) >> 2;
	
			psUniflexHW->puInstructions[i] = ((EURASIA_USE0_S1EXTBANK_IMMEDIATE << EURASIA_USE0_S1BANK_SHIFT) |
											(EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT) |
											EURASIA_USE0_EMIT_FREEP);

			psUniflexHW->puInstructions[i+1] =
				((EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
				 (EURASIA_USE1_OTHER_OP2_EMIT << EURASIA_USE1_OTHER_OP2_SHIFT) |
				 (EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				 (EURASIA_USE1_S1BEXT) |
				 (EURASIA_USE1_S2BEXT) |
				 (EURASIA_USE1_S0STDBANK_TEMP << EURASIA_USE1_S0BANK_SHIFT) |				 (EURASIA_USE1_EMIT_TARGET_MTE << EURASIA_USE1_EMIT_TARGET_SHIFT) |
				 (EURASIA_USE1_EMIT_MTECTRL_VERTEX << EURASIA_USE1_EMIT_MTECTRL_SHIFT) |
				 (EURASIA_USE1_EPRED_ALWAYS << EURASIA_USE1_EMIT_EPRED_SHIFT) |
				 (0 << EURASIA_USE1_EMIT_INCP_SHIFT) |
				 EURASIA_USE1_END);

			psUniflexHW->uInstructionCount++;
		}
#endif
	}


	return IMG_TRUE;
}

#endif /* OUTPUT_USPBIN */

#endif /* GEN_HW_CODE */

/******************************************************************************
 End of file (ic2uf.c)
******************************************************************************/

