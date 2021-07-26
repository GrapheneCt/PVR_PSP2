/******************************************************************************
 * Name         : main.c
 * Title        : Pixel Shader Compiler
 * Author       : John Russell
 * Created      : Jan 2002
 *
 * Copyright    : 2002-2010 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means,electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited,
 *              : Home Park Estate, Kings Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Modifications:-
 * $Log: main.c $
 *****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>
#if !defined(LINUX)
#include <io.h>
#else
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#define max(a,b) a>b?a:b
#define stricmp strcasecmp
#endif


#include "sgxdefs.h"
#include "sgxpdsdefs.h"

#include "pdsasm.h"

#define PDSASM_ARGTYPE_DS0			(1 << 0)
#define PDSASM_ARGTYPE_DS1			(1 << 1)
#define PDSASM_ARGTYPE_SPECIAL		(1 << 2)
#define PDSASM_ARGTYPE_IMMEDIATE	(1 << 3)
#define PDSASM_ARGTYPE_PREDICATE	(1 << 4)
#define PDSASM_ARGTYPE_LABEL		(1 << 5)

#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)

#define PDSASM_VALIDARGTYPE_LOGICARITH	{PDSASM_ARGTYPE_DS0 | PDSASM_ARGTYPE_DS1, \
										PDSASM_ARGTYPE_DS0 | PDSASM_ARGTYPE_SPECIAL, \
										PDSASM_ARGTYPE_DS1 | PDSASM_ARGTYPE_SPECIAL}
#else

#define PDSASM_VALIDARGTYPE_LOGICARITH	{PDSASM_ARGTYPE_DS0 | PDSASM_ARGTYPE_DS1, \
										PDSASM_ARGTYPE_DS0 | PDSASM_ARGTYPE_SPECIAL, \
										PDSASM_ARGTYPE_DS1}
#endif

static IMG_VOID Error(PINST psInst, IMG_PCHAR pszFmt, ...) IMG_FORMAT_PRINTF(2, 3);

static struct
{
	IMG_PCHAR	pszName;
	IMG_BOOL	bHasDest;
	IMG_UINT32	uArgCount;
	IMG_UINT32	puValidArgTypes[3];
} g_psInstDesc[] = {
	{
		/* PDSASM_OPCODE_MOVS */
		"movs",
		IMG_FALSE,
		0,
		{0, 0, 0},
	},
	{
		/* PDSASM_OPCODE_MOVSA */
		"movsa",
		IMG_FALSE,
		0,
		{0, 0, 0},
	},
	{
		/* PDSASM_OPCODE_MOV16 */
		"mov16",
		IMG_TRUE,
		2,
		{PDSASM_ARGTYPE_DS0 | PDSASM_ARGTYPE_DS1, PDSASM_ARGTYPE_SPECIAL | PDSASM_ARGTYPE_DS0 | PDSASM_ARGTYPE_DS1, 0},
	},
	{
		/* PDSASM_OPCODE_MOV32 */
		"mov32",
		IMG_TRUE,
		2,
		{PDSASM_ARGTYPE_DS0 | PDSASM_ARGTYPE_DS1, PDSASM_ARGTYPE_SPECIAL | PDSASM_ARGTYPE_DS0 | PDSASM_ARGTYPE_DS1, 0},
	},
	{
		/* PDSASM_OPCODE_MOV64 */
		"mov64",
		IMG_TRUE,
		0,
		{0, 0, 0},
	},
	{
		/* PDSASM_OPCODE_MOV128 */
		"mov128",
		IMG_TRUE,
		0,
		{0, 0, 0},
	},
	{
		/* PDSASM_OPCODE_ADD */
		"add",
		IMG_TRUE,
		3,
		PDSASM_VALIDARGTYPE_LOGICARITH,
	},
	{
		/* PDSASM_OPCODE_SUB */
		"sub",
		IMG_TRUE,
		3,
		PDSASM_VALIDARGTYPE_LOGICARITH,
	},
	{
		/* PDSASM_OPCODE_ADC */
		"adc",
		IMG_TRUE,
		3,
		PDSASM_VALIDARGTYPE_LOGICARITH,
	},
	{
		/* PDSASM_OPCODE_SBC */
		"sbc",
		IMG_TRUE,
		3,
		PDSASM_VALIDARGTYPE_LOGICARITH,
	},
	{
		/* PDSASM_OPCODE_MUL */
		"mul",
		IMG_TRUE,
		3,
		PDSASM_VALIDARGTYPE_LOGICARITH,
	},
	{
		/* PDSASM_OPCODE_ABS */
		"abs",
		IMG_TRUE,
		2,
		{PDSASM_ARGTYPE_DS0 | PDSASM_ARGTYPE_DS1, PDSASM_ARGTYPE_DS0 | PDSASM_ARGTYPE_DS1 | PDSASM_ARGTYPE_SPECIAL, 0},
	},
	{
		/* PDSASM_OPCODE_TSTZ */
		"tstz",
		IMG_TRUE,
		2,
		{PDSASM_ARGTYPE_PREDICATE, PDSASM_ARGTYPE_DS0 | PDSASM_ARGTYPE_DS1 | PDSASM_ARGTYPE_SPECIAL, 0},
	},
	{
		/* PDSASM_OPCODE_TSTNZ */
		"tstnz",
		IMG_TRUE,
		2,
		{PDSASM_ARGTYPE_PREDICATE, PDSASM_ARGTYPE_DS0 | PDSASM_ARGTYPE_DS1 | PDSASM_ARGTYPE_SPECIAL, 0},
	},
	{
		/* PDSASM_OPCODE_TSTN */
		"tstn",
		IMG_TRUE,
		2,
		{PDSASM_ARGTYPE_PREDICATE, PDSASM_ARGTYPE_DS0 | PDSASM_ARGTYPE_DS1 | PDSASM_ARGTYPE_SPECIAL, 0},
	},
	{
		/* PDSASM_OPCODE_TSTP */
		"tstp",
		IMG_TRUE,
		2,
		{PDSASM_ARGTYPE_PREDICATE, PDSASM_ARGTYPE_DS0 | PDSASM_ARGTYPE_DS1 | PDSASM_ARGTYPE_SPECIAL, 0},
	},
	{
		/* PDSASM_OPCODE_BRA */
		"bra",
		IMG_FALSE,
		1,
		{PDSASM_ARGTYPE_LABEL, 0, 0},
	},
	{
		/* PDSASM_OPCODE_CALL */
		"call",
		IMG_FALSE,
		1,
		{PDSASM_ARGTYPE_LABEL, 0, 0},
	},
	{
		/* PDSASM_OPCODE_RTN */
		"rtn",
		IMG_FALSE,
		0,
		{0,0,0},
	},
	{
		/* PDSASM_OPCODE_HALT */
		"halt",
		IMG_FALSE,
		0,
		{0,0,0},
	},
	{
		/* PDSASM_OPCODE_NOP */
		"nop",
		IMG_FALSE,
		0,
		{0,0,0},
	},
	{
		/* PDSASM_OPCODE_ALUM */
		"alum",
		IMG_FALSE,
		1,
		{PDSASM_ARGTYPE_IMMEDIATE},
	},
	{
		/* PDSASM_OPCODE_OR */
		"or",
		IMG_TRUE,
		3,
		PDSASM_VALIDARGTYPE_LOGICARITH,
	},
	{
		/* PDSASM_OPCODE_AND */
		"and",
		IMG_TRUE,
		3,
		PDSASM_VALIDARGTYPE_LOGICARITH,
	},
	{
		/* PDSASM_OPCODE_XOR */
		"xor",
		IMG_TRUE,
		3,
		PDSASM_VALIDARGTYPE_LOGICARITH,
	},
	{
		/* PDSASM_OPCODE_NOT */
		"not",
		IMG_TRUE,
		2,
		{PDSASM_ARGTYPE_DS0 | PDSASM_ARGTYPE_DS1, PDSASM_ARGTYPE_DS0 | PDSASM_ARGTYPE_DS1 | PDSASM_ARGTYPE_SPECIAL, 0},
	},
	{
		/* PDSASM_OPCODE_NOR */
		"nor",
		IMG_TRUE,
		3,
		PDSASM_VALIDARGTYPE_LOGICARITH,
	},
	{
		/* PDSASM_OPCODE_NAND */
		"nand",
		IMG_TRUE,
		3,
		PDSASM_VALIDARGTYPE_LOGICARITH,
	},
	{
		/* PDSASM_OPCODE_SHL */
		"shl",
		IMG_TRUE,
		3,
		{PDSASM_ARGTYPE_DS0 | PDSASM_ARGTYPE_DS1, PDSASM_ARGTYPE_DS0 | PDSASM_ARGTYPE_DS1 | PDSASM_ARGTYPE_SPECIAL, PDSASM_ARGTYPE_IMMEDIATE},
	},
	{
		/* PDSASM_OPCODE_SHR */
		"shr",
		IMG_TRUE,
		3,
		{PDSASM_ARGTYPE_DS0 | PDSASM_ARGTYPE_DS1, PDSASM_ARGTYPE_DS0 | PDSASM_ARGTYPE_DS1 | PDSASM_ARGTYPE_SPECIAL, PDSASM_ARGTYPE_IMMEDIATE},
	},
	{
		/* PDSASM_OPCODE_LABEL */
		"label",
		IMG_FALSE,
		0,
		{0,0,0},
	},
	{
		/* PDSASM_OPCODE_LOAD */
		"load",
		IMG_FALSE,
		0,
		{PDSASM_ARGTYPE_IMMEDIATE,0,0},
	},
	{
		/* PDSASM_OPCODE_STORE */
		"store",
		IMG_FALSE,
		0,
		{PDSASM_ARGTYPE_IMMEDIATE,0,0},
	},
	{
		/* PDSASM_OPCODE_RFENCE */
		"rfence",
		IMG_FALSE,
		0,
		{0,0,0},
	},
	{
		/* PDSASM_OPCODE_RWFENCE */
		"rwfence",
		IMG_FALSE,
		0,
		{0,0,0},
	},
};

IMG_INT yyparse(IMG_VOID);

LOCATION		g_sCurrentLocation;
extern FILE*	yyin;
extern IMG_INT	yydebug;
extern IMG_INT	yy_flex_debug;

PINST g_psInstructionListHead = IMG_NULL;

IMG_BOOL g_bParserError = IMG_FALSE;

/* Is compatibility with the output from a C preprocessor enabled? */
IMG_BOOL g_bCPreprocessor = IMG_FALSE;

typedef struct
{
	IMG_UINT32					auOffsetCount[2];
	IMG_PUINT32					apuOffsets[2];
	IMG_BOOL					abAssignedToBank[2];
} DATASTORE_INFO, *PDATASTORE_INFO;

static IMG_UINT32	g_uIdentifierCount = 0;
typedef struct _IDENTIFIER
{
	LOCATION					sLocation;
	PDSASM_IDENTIFIER_CLASS		eClass;
	IMG_PCHAR					pszName;
	PDSASM_IDENTIFIER_TYPE		eType;
	IMG_BOOL					bDefined;
	IMG_UINT32					uLabelOffset;
	IMG_UINT32					uImmediate;
	DATASTORE_INFO				sDataStore;
	PDSASM_DATASTORE_BANK		ePreAssignedBank;
	IMG_UINT32					uPreAssignedOffset;
	IMG_UINT32					uInstParamCount;
} IDENTIFIER, *PIDENTIFIER;
static IDENTIFIER *g_psIdentifierList = NULL;

/*
	For each dword sized location in the constant part of the datastore for each bank TRUE if
	that location has been used for an identifier.
*/
static IMG_BOOL g_aabDSUsed[2][EURASIA_PDS_DATASTORE_TEMPSTART];

/*
	Count of the dword sized locations in each bank not used for an identifier.
*/
static IMG_UINT32 g_auDSLocationsLeft[2];

/*
	Count of the qword-sized and aligned locations in each bank not used for
	an identifier.
*/
static IMG_UINT32 g_auDSDoubleLocationsLeft[2];

/*
	For each dword sized location in the modifiable part of the datastore for each bank TRUE if
	that location has been used for an identifier.
*/
static IMG_BOOL g_aabTempUsed[2][EURASIA_PDS_DATASTORE_PERBANKSIZE - EURASIA_PDS_DATASTORE_TEMPSTART];

static IMG_VOID usage(IMG_VOID)
{
	printf("usage: pdsasm [-t] [-s] <infile> <outfile>");
}

#if defined(SGX_FEATURE_PDS_DATA_INTERLEAVE_2DWORDS)
#define	PDS_NUM_DWORDS_PER_ROW		2
#else
#define	PDS_NUM_DWORDS_PER_ROW		8
#endif

static IMG_VOID Error(PINST psInst, IMG_PCHAR pszFmt, ...)
/*****************************************************************************
 FUNCTION	: Error
    
 PURPOSE	: Report an error on an instruction.

 PARAMETERS	: psInst		- Instruction to report the error at.
			  pszInst		- Format string for the error message.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	va_list	ap;

	va_start(ap, pszFmt);
	fprintf(stderr, "%s(%u): error: ", 
			psInst->sLocation.pszSourceFile,
			psInst->sLocation.uSourceLine);
	vfprintf(stderr, pszFmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
}

static IMG_BOOL CheckArgumentsMOVS(PINST psInst)
/*****************************************************************************
 FUNCTION	: CheckArgumentsMOVS
    
 PURPOSE	: Check the number and types of the arguments to MOVS.

 PARAMETERS	: psInst		- Instruction to check.
			  
 RETURNS	: TRUE if the check passed or FALSE otherwise.
*****************************************************************************/
{
	IMG_UINT32					uMovsArgCount;
	PDSASM_SPECIAL_REGISTER		eSpecialArg = PDSASM_SPECIAL_REGISTER_UNKNOWN;
	IMG_UINT32					uArg;

	/*
		The first argument to movs is the type of the command to send.
	*/
	if (!
			(
				psInst->uArgCount >= 1 &&
				psInst->psArgs[0].eType == PDSASM_REGISTER_TYPE_SPECIAL
			)
	   )
	{
		Error(psInst, "The first argument to movs must be the command type.");
		return IMG_FALSE;
	}

	/*
		The number of argument needs on the command type.
	*/
	if (psInst->psArgs[0].u.eSpecialRegister == PDSASM_SPECIAL_REGISTER_DOUTU)
	{
		#if defined(SGX545) || defined(SGX543) || defined(SGX544) || defined(SGX554)
		/*
			Either 2 sources for DOUTU or 3 sources for DOUTLU.
		*/
		if (psInst->eOpcode == PDSASM_OPCODE_MOVS)
		{
			if (psInst->uArgCount != 3 && psInst->uArgCount != 4)
			{
				Error(psInst, "Command type doutu needs either 2 or 3 sources.");
				return IMG_FALSE;
			}
		}
		else
		{
			if (psInst->uArgCount != 4 && psInst->uArgCount != 5)
			{
				Error(psInst, "Command type doutu needs either 3 or 4 sources.");
				return IMG_FALSE;
			}
		}
		#else /* defined(SGX545)  || defined(SGX543) || defined(SGX544) || defined(SGX554) */
		if (psInst->eOpcode == PDSASM_OPCODE_MOVS)
		{
			/*
				The fourth source is only used for loopbacks and so is
				optional.
			*/
			if (psInst->uArgCount != 4 && psInst->uArgCount != 5)
			{
				Error(psInst, "Command type doutu needs either 3 or 4 sources.");
				return IMG_FALSE;
			}
		}
		else
		{
			/*
				Three sources plus the value to add on to source 0.
			*/
			if (psInst->uArgCount != 5)
			{
				Error(psInst, "Command type doutu needs 4 sources.");
				return IMG_FALSE;
			}
		}
		#endif /* defined(SGX545)  || defined(SGX543) || defined(SGX544) || defined(SGX554) */
	}
	else
	{
		switch (psInst->psArgs[0].u.eSpecialRegister)
		{
			case PDSASM_SPECIAL_REGISTER_TIMER:
			{
				uMovsArgCount = 1;
				break;
			}
			case PDSASM_SPECIAL_REGISTER_DOUTT:
			{
				uMovsArgCount = EURASIA_TAG_TEXTURE_STATE_SIZE;
				break;
			}
			case PDSASM_SPECIAL_REGISTER_DOUTI:
			{
				uMovsArgCount = EURASIA_PDS_DOUTI_STATE_SIZE;
				break;
			}
			case PDSASM_SPECIAL_REGISTER_DOUTA:
			{
				uMovsArgCount = 2;
				break;
			}
			case PDSASM_SPECIAL_REGISTER_DOUTD:
			{
				uMovsArgCount = 2;
				break;
			}
			#if defined(SGX545)
			case PDSASM_SPECIAL_REGISTER_DOUTC:
			{
				uMovsArgCount = 2;
				break;
			}
			case PDSASM_SPECIAL_REGISTER_DOUTS:
			{
				uMovsArgCount = 2;
				break;
			}
			#else /* defined(SGX545) */
			case PDSASM_SPECIAL_REGISTER_DOUTC:
			case PDSASM_SPECIAL_REGISTER_DOUTS:
			{
				Error(psInst, "This command type isn't available on this core.");
				return IMG_FALSE;
			}
			#endif /* defined(SGX545) */
			#if defined(SGX543) || defined(SGX544) || defined(SGX554)
			case PDSASM_SPECIAL_REGISTER_DOUTO:
			{
				uMovsArgCount = 1;
				break;
			}
			#else /* defined(SGX543) || defined(SGX544) || defined(SGX554) */	
			case PDSASM_SPECIAL_REGISTER_DOUTO:
			{
				Error(psInst, "This command type isn't available on this core.");
				return IMG_FALSE;
			}
			#endif /* defined(SGX543) || defined(SGX544) || defined(SGX554) */	
			default:
			{
				Error(psInst, "Unknown command type for the first argument to movs.");
				return IMG_FALSE;
			}
		}

		/*
			MOVSA has an extra argument which is the value to add onto the first 32-bits of the command.
		*/
		if (psInst->eOpcode == PDSASM_OPCODE_MOVSA)
		{
			uMovsArgCount++;
		}

		/*
			Check the command type.
		*/
		if (psInst->uArgCount != (uMovsArgCount + 1))
		{
			Error(psInst, "Wrong number of arguments for this command type (was %d but should have been %d).",
				  psInst->uArgCount,
				  uMovsArgCount + 1);
			return IMG_FALSE;
		}
	}

	/*
		Check the arguments.
	*/
	for (uArg = 1; uArg < psInst->uArgCount; uArg++)
	{
		if (psInst->psArgs[uArg].eType == PDSASM_REGISTER_TYPE_IMMEDIATE)
		{
			Error(psInst, "Arguments to movs can't be immediate."); 
			return IMG_FALSE;
		}
		else if (psInst->psArgs[uArg].eType == PDSASM_REGISTER_TYPE_PREDICATE)
		{
			Error(psInst, "Arguments to movs can't be predicate registers.");
			return IMG_FALSE;
		}
		else if (psInst->psArgs[uArg].eType == PDSASM_REGISTER_TYPE_SPECIAL)
		{
			/*
				At most one special register can be part of the movs arguments.
			*/
			if (eSpecialArg == PDSASM_SPECIAL_REGISTER_UNKNOWN)
			{
				eSpecialArg = psInst->psArgs[uArg].u.eSpecialRegister;
			}
#if !defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
			else if (eSpecialArg != psInst->psArgs[uArg].u.eSpecialRegister)
			{
				Error(psInst, "At most one special register is valid as an argument to movs.");
			}
#endif
		}
	}
	return IMG_TRUE;
}

static IMG_BOOL CheckArgumentsLOADSTORE(PINST psInst)
/*****************************************************************************
 FUNCTION	: CheckArgumentsLOADSTORE
    
 PURPOSE	: Check the number and types of the arguments to a LOAD or STORE
			  instruction.

 PARAMETERS	: psInst		- Instruction to check.
			  
 RETURNS	: TRUE if the check passed or FALSE otherwise.
*****************************************************************************/
{
	IMG_UINT32	uArg;

	/*
		The argument count must be at least 2 (the memoyr address to load from/store to
		and one variable to load into/store from).
	*/
	if (psInst->uArgCount < 2)
	{
		Error(psInst,
			  "A %s instruction must have at least two arguments.", 
			  g_psInstDesc[psInst->eOpcode].pszName);
		return IMG_FALSE;
	}

	/*
		The first argument (the memory address) must be either an immediate or a 
		constant identifier.
	*/
	if (psInst->psArgs[0].eType != PDSASM_REGISTER_TYPE_DATASTORE &&
		psInst->psArgs[0].eType != PDSASM_REGISTER_TYPE_IMMEDIATE)
	{
		Error(psInst,
			  "The first argument to a %s instruction must either an immediate or a constant.",
			  g_psInstDesc[psInst->eOpcode].pszName);
		return IMG_FALSE;
	}
	else
	{
		if (psInst->psArgs[0].eType == PDSASM_REGISTER_TYPE_DATASTORE)
		{
			IMG_UINT32	uIdent;

			/*
				If the memory address is an identifier check that it is a constant.
			*/
			uIdent = psInst->psArgs[0].u.uIdentifier;
			if (g_psIdentifierList[uIdent].eClass != PDSASM_IDENTIFIER_CLASS_DATA)
			{
				Error(psInst,
					  "The first argument to a %s instruction must be declared with the 'data' attribute.",
					  g_psInstDesc[psInst->eOpcode].pszName);
				return IMG_FALSE;
			}

			/*
				If the memory address is a constant identifier then flag it as an argument which
				is encoded directly in the instruction so we don't try and assign data store
				space for it.
			*/
			psInst->psArgs[0].eType = PDSASM_REGISTER_TYPE_INSTRUCTION_PARAMETER;
			psInst->psArgs[0].u.sParameter.uIdentifier = uIdent;
			psInst->psArgs[0].u.sParameter.uOffset = 0;
			g_psIdentifierList[uIdent].uInstParamCount++;
		}
	}

	/*
		Check the other arguments (things to load or store) are modifiable identifiers.
	*/
	for (uArg = 1; uArg < psInst->uArgCount; uArg++)
	{
		IMG_UINT32	uIdent;

		if (psInst->psArgs[uArg].eType != PDSASM_REGISTER_TYPE_DATASTORE)
		{
			Error(psInst,
				  "Argument %d to a %s instruction must be a datastore location",
				  uArg,
				  g_psInstDesc[psInst->eOpcode].pszName);
			return IMG_FALSE;
		}

		uIdent = psInst->psArgs[uArg].u.uIdentifier;
		if (g_psIdentifierList[uIdent].eClass != PDSASM_IDENTIFIER_CLASS_TEMP)
		{
			Error(psInst,
				  "Argument %d to a %s instruction must be declared with the 'temp' attribute.",
				  uArg,
				  g_psInstDesc[psInst->eOpcode].pszName);
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

static IMG_BOOL CheckArgumentsNonMOVS(PINST	psInst)
/*****************************************************************************
 FUNCTION	: CheckArgumentsNonMOVS
    
 PURPOSE	: Check the number and types of the arguments to an instruction
			  other than MOVS.

 PARAMETERS	: psInst		- Instruction to check.
			  
 RETURNS	: TRUE if the check passed or FALSE otherwise.
*****************************************************************************/
{
	IMG_UINT32	uArg;

	if (psInst->uArgCount != g_psInstDesc[psInst->eOpcode].uArgCount)
	{
		Error(psInst,
			  "Wrong number of arguments for instruction (was %d but should have been %d).", 
			  psInst->uArgCount,
			  g_psInstDesc[psInst->eOpcode].uArgCount);
		return IMG_FALSE;
	}
	
	for (uArg = 0; uArg < psInst->uArgCount; uArg++)
	{
		IMG_UINT32	uValidTypes = g_psInstDesc[psInst->eOpcode].puValidArgTypes[uArg];

		if (psInst->psArgs[uArg].eType == PDSASM_REGISTER_TYPE_IMMEDIATE)
		{
			if ((uValidTypes & PDSASM_ARGTYPE_IMMEDIATE) == 0)
			{
				Error(psInst, "Argument %d to %s can't be immediate.", 
					uArg,
					g_psInstDesc[psInst->eOpcode].pszName);
				return IMG_FALSE;
			}
		}
		else if (psInst->psArgs[uArg].eType == PDSASM_REGISTER_TYPE_SPECIAL)
		{
			if ((uValidTypes & PDSASM_ARGTYPE_SPECIAL) == 0)
			{
				Error(psInst, "Argument %d to %s can't be a special (non-datastore) register.", 
					uArg,
					g_psInstDesc[psInst->eOpcode].pszName);
				return IMG_FALSE;
			}
		}
		else if (psInst->psArgs[uArg].eType == PDSASM_REGISTER_TYPE_PREDICATE)
		{
			if ((uValidTypes & PDSASM_ARGTYPE_PREDICATE) == 0)
			{
				Error(psInst, "Argument %d to %s can't be a predicate register.", 
					uArg,
					g_psInstDesc[psInst->eOpcode].pszName);
				return IMG_FALSE;
			}
		}
		else
		{
			IMG_UINT32	uIdent;

			assert(psInst->psArgs[uArg].eType == PDSASM_REGISTER_TYPE_DATASTORE);

			uIdent = psInst->psArgs[uArg].u.uIdentifier;

			if (uValidTypes & PDSASM_ARGTYPE_LABEL)
			{
				assert(uValidTypes == PDSASM_ARGTYPE_LABEL);
				if (g_psIdentifierList[uIdent].eClass != PDSASM_IDENTIFIER_CLASS_LABEL)
				{
					Error(psInst, "Argument %d to %s must be a label.", 
						  uArg,
						  g_psInstDesc[psInst->eOpcode].pszName);
					return IMG_FALSE;
				}

				/*
					Replace the label by an instruction parameter so we know not to
					try and assign data store space for it later.
				*/
				psInst->psArgs[uArg].eType = PDSASM_REGISTER_TYPE_INSTRUCTION_PARAMETER;
				psInst->psArgs[uArg].u.sParameter.uIdentifier = uIdent;
				psInst->psArgs[uArg].u.sParameter.uOffset = 0;
				continue;
			}

			if ((uValidTypes & (PDSASM_ARGTYPE_DS0 | PDSASM_ARGTYPE_DS1)) == 0)
			{
				Error(psInst, "Argument %d to %s can't be a datastore location.", 
					  uArg,
					  g_psInstDesc[psInst->eOpcode].pszName);
				return IMG_FALSE;
			}

			/*
				Check that a destination is a modifiable data store location.
			*/
			if (
					uArg == 0 &&
					g_psInstDesc[psInst->eOpcode].bHasDest &&
					g_psIdentifierList[uIdent].eClass != PDSASM_IDENTIFIER_CLASS_TEMP
			   )
			{
				if (g_psIdentifierList[uIdent].eClass == PDSASM_IDENTIFIER_CLASS_IMMEDIATE)
				{
					Error(psInst, "The destination to %s can't be an immediate.", 
						g_psInstDesc[psInst->eOpcode].pszName);
					return IMG_FALSE;
				}
				else
				{
					Error(psInst, "The destination %s to %s must be declared with the 'temp' attribute.", 
						g_psIdentifierList[uIdent].pszName,
						g_psInstDesc[psInst->eOpcode].pszName);
					return IMG_FALSE;
				}
			}
		}
	}
	return IMG_TRUE;
}

static IMG_BOOL CheckArguments(IMG_VOID)
/*****************************************************************************
 FUNCTION	: CheckArguments
    
 PURPOSE	: Check the number and types of the arguments to all the instructions
			  in the program.

 PARAMETERS	: None.
			  
 RETURNS	: TRUE if the check passed or FALSE otherwise.
*****************************************************************************/
{
	PINST		psInst;
	IMG_BOOL	bOk = IMG_TRUE;

	for (psInst = g_psInstructionListHead; psInst != NULL; psInst = psInst->psNext)
	{
		switch (psInst->eOpcode)
		{
			case PDSASM_OPCODE_MOVS:
			case PDSASM_OPCODE_MOVSA:
			{
				if (!CheckArgumentsMOVS(psInst))
				{
					bOk = IMG_FALSE;
				}
				break;
			}
			case PDSASM_OPCODE_LOAD:
			case PDSASM_OPCODE_STORE:
			{
				if (!CheckArgumentsLOADSTORE(psInst))
				{
					bOk = IMG_FALSE;
				}
				break;
			}
			case PDSASM_OPCODE_MOV64:
			case PDSASM_OPCODE_MOV128:
			{
				if (psInst->eOpcode == PDSASM_OPCODE_MOV64 || psInst->eOpcode == PDSASM_OPCODE_MOV128)
				{
					Error(psInst, "The %s instruction isn't currently supported.", 
						g_psInstDesc[psInst->eOpcode].pszName);
					bOk = IMG_FALSE;
				}
				break;
			}
			case PDSASM_OPCODE_LABEL:
			{
				/*
					No need to check the arguments because this instruction is generated internally.
				*/
				break;
			}
			default:
			{
				if (!CheckArgumentsNonMOVS(psInst))
				{
					bOk = IMG_FALSE;
				}
				break;
			}
		}
	}
	return bOk;
}

static IMG_VOID CheckBanksMOVS(PINST psInst, IMG_PBOOL pbCanUseBank, IMG_UINT32	uIdentifier)
/*****************************************************************************
 FUNCTION	: CheckBanksMOVS
    
 PURPOSE	: Check which bank are valid for an argument to a MOVS instruction.

 PARAMETERS	: psInst		- The instruction to check.
			  pbCanUseBank	- Points to an array with an entry for each bank.
							Set to false on return if that bank can't be used.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32				uArg;
	IMG_UINT32				uBothBanksCount, uBank0Count, uBank1Count;
	PDSASM_IDENTIFIER_CLASS	eClass = g_psIdentifierList[uIdentifier].eClass;
	IMG_UINT32				uSeenIdentifiersCount = 0;
	IMG_UINT32				puSeenIdentifiers[4];
	IMG_BOOL				bSpecialArg = IMG_FALSE;

	uBothBanksCount = uBank0Count = uBank1Count = 0;
	for (uArg = 1; uArg < psInst->uArgCount; uArg++)
	{
		/*
			A special register source uses the same slot as bank 0.
		*/
		if (psInst->psArgs[uArg].eType == PDSASM_REGISTER_TYPE_SPECIAL)
		{
			bSpecialArg = IMG_TRUE;
#if !defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
			pbCanUseBank[0] = IMG_FALSE;
#endif
		}
		else if (psInst->psArgs[uArg].eType == PDSASM_REGISTER_TYPE_DATASTORE &&
				 psInst->psArgs[uArg].u.uIdentifier != uIdentifier)
		{
			IMG_UINT32				uOtherIdentifier = psInst->psArgs[uArg].u.uIdentifier;
			PDSASM_IDENTIFIER_CLASS	eOtherClass = g_psIdentifierList[uOtherIdentifier].eClass;
			IMG_UINT32				uIdx;

			/*
				Check if this identifier as already been seen as an argument..
			*/
			for (uIdx = 0; uIdx < uSeenIdentifiersCount; uIdx++)
			{
				if (puSeenIdentifiers[uIdx] == uOtherIdentifier)
				{
					break;
				}
			}
			if (uIdx == uSeenIdentifiersCount)
			{
				puSeenIdentifiers[uSeenIdentifiersCount++] = uOtherIdentifier;

				/*
					A temporary and a non-temporary can't be assigned to the same bank.
				*/
				if ((eClass == PDSASM_IDENTIFIER_CLASS_TEMP && eOtherClass != PDSASM_IDENTIFIER_CLASS_TEMP) ||
					(eOtherClass == PDSASM_IDENTIFIER_CLASS_TEMP && eClass != PDSASM_IDENTIFIER_CLASS_TEMP))
				{
					if (g_psIdentifierList[uOtherIdentifier].sDataStore.abAssignedToBank[0])
					{
						pbCanUseBank[0] = IMG_FALSE;
					}
					else if (g_psIdentifierList[uOtherIdentifier].sDataStore.abAssignedToBank[1])
					{
						pbCanUseBank[1] = IMG_FALSE;
					}
				}

				/*
					Count the number of arguments already assigned to banks.
				*/
				if (g_psIdentifierList[uOtherIdentifier].sDataStore.abAssignedToBank[0] &&
					g_psIdentifierList[uOtherIdentifier].sDataStore.abAssignedToBank[1])
				{	
					uBothBanksCount++;
				}
				else if (g_psIdentifierList[uOtherIdentifier].sDataStore.abAssignedToBank[0])
				{
					uBank0Count++;
				}
				else if (g_psIdentifierList[uOtherIdentifier].sDataStore.abAssignedToBank[1])
				{
					uBank1Count++;
				}
			}
		}
	}

#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
	if (bSpecialArg)
	{
		/*
			If any identifiers already use bank 0 then the special argument will use the
			bank 1 argument so we can't use bank 1 for this identifier.
		*/
		if (uBank0Count > 0)
		{
			pbCanUseBank[1] = IMG_FALSE;
		}
		/*
			Similarly if any identifiers already use bank 1.
		*/
		if (uBank1Count > 0)
		{
			pbCanUseBank[0] = IMG_FALSE;
		}
	}
#endif /* defined(SGX_FEATURE_PDS_EXTENDED_SOURCES) */ 


	/*
		At most 2 arguments can come from each bank.
	*/
	if (uBank0Count >= 2)
	{
		pbCanUseBank[0] = IMG_FALSE;
	}
	if (uBank1Count >= 2)
	{
		pbCanUseBank[1] = IMG_FALSE;
	}
}

static IMG_UINT32 GetValidArgTypes(PINST psInst, IMG_UINT32 uArg)
/*****************************************************************************
 FUNCTION	: GetValidArgTypes
    
 PURPOSE	: Get the argument types valid for an instruction argument.

 PARAMETERS	: psInst		- The instruction to check.
			  uArg			- Index of the argument to check.
			  
 RETURNS	: A bit-mask of the valid types.
*****************************************************************************/
{
	if (psInst->eOpcode == PDSASM_OPCODE_LOAD || psInst->eOpcode == PDSASM_OPCODE_STORE)
	{
		/*
			These instructions have a variable number of arguments all of which can come
			from either DS0 or DS1.
		*/
		return PDSASM_ARGTYPE_DS0 | PDSASM_ARGTYPE_DS1;
	}
	else
	{
		return g_psInstDesc[psInst->eOpcode].puValidArgTypes[uArg];
	}
}

static IMG_VOID CheckBanksNonMOVS(PINST psInst, IMG_UINT32 uIdentifier, IMG_PBOOL pbCanUseBank)
/*****************************************************************************
 FUNCTION	: CheckBanksNonMOVS
    
 PURPOSE	: Check which bank are valid for an argument to an instruction
			  other than MOVS.

 PARAMETERS	: psInst		- The instruction to check.
			  uIdentifier	- The identifier to check.
			  pbCanUseBank	- Points to an array with an entry for each bank.
							Set to false on return if that bank can't be used.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uArg;

	/*
		Ignore place holders for branch/call destinations.
	*/
	if (psInst->eOpcode == PDSASM_OPCODE_LABEL)
	{
		return;
	}

	for (uArg = 0; uArg < psInst->uArgCount; uArg++)
	{
		IMG_UINT32	uValidFlags;
		
		uValidFlags = GetValidArgTypes(psInst, uArg);

		if (psInst->psArgs[uArg].eType == PDSASM_REGISTER_TYPE_DATASTORE &&
			psInst->psArgs[uArg].u.uIdentifier == uIdentifier)
		{
			if ((uValidFlags & PDSASM_ARGTYPE_DS0) == 0)
			{
				pbCanUseBank[0] = IMG_FALSE;
			}
			if ((uValidFlags & PDSASM_ARGTYPE_DS1) == 0)
			{
				pbCanUseBank[1] = IMG_FALSE;
			}
		}
	}
}

static IMG_BOOL AssignBankForIdentifier(IMG_UINT32 uIdentifier)
/*****************************************************************************
 FUNCTION	: AssignBankForIdentifier
    
 PURPOSE	: Assign a bank for an identifier.

 PARAMETERS	: uIdentifier	- The identifier to assign a bank for.
			  
 RETURNS	: TRUE if assignment succeeded; FALSE otherwise.
*****************************************************************************/
{
	PINST		psInst;
	IMG_BOOL	abCanUseBank[2];
	PIDENTIFIER	psId = &g_psIdentifierList[uIdentifier];

	abCanUseBank[0] = abCanUseBank[1] = IMG_TRUE;
	for (psInst = g_psInstructionListHead; psInst != NULL; psInst = psInst->psNext)
	{
		IMG_BOOL	bUsesIdentifier;
		IMG_UINT32	uArg;

		/*
			Check that the identifier actually occurs as an argument to this
			instruction.
		*/
		bUsesIdentifier = IMG_FALSE;
		for (uArg = 0; uArg < psInst->uArgCount; uArg++)
		{
			if (psInst->psArgs[uArg].eType == PDSASM_REGISTER_TYPE_DATASTORE &&
				psInst->psArgs[uArg].u.uIdentifier == uIdentifier)
			{
				bUsesIdentifier = IMG_TRUE;
			}
		}

		if (!bUsesIdentifier)
		{
			continue;
		}

		/*
			Include any bank restrictions from this instruction.
		*/
		if (psInst->eOpcode == PDSASM_OPCODE_MOVS || psInst->eOpcode == PDSASM_OPCODE_MOVSA)
		{
			CheckBanksMOVS(psInst, abCanUseBank, uIdentifier);
		}
		else
		{
			CheckBanksNonMOVS(psInst, uIdentifier, abCanUseBank);
		}
	}

	if (psId->ePreAssignedBank != PDSASM_DATASTORE_BANK_NONE)
	{
		/*
			For a preassigned identifier check that the preassigned bank is
			available.
		*/
		if (
				(psId->ePreAssignedBank == PDSASM_DATASTORE_BANK_0 && !abCanUseBank[0]) ||
				(psId->ePreAssignedBank == PDSASM_DATASTORE_BANK_1 && !abCanUseBank[1])
		   )
		{
			fprintf(stderr, "%s(%u): error: The preassigned bank for identifier '%s' is incompatible with "
				"the instructions using the identifier.", 
					g_psIdentifierList[uIdentifier].sLocation.pszSourceFile,
					g_psIdentifierList[uIdentifier].sLocation.uSourceLine,
					g_psIdentifierList[uIdentifier].pszName);
			return IMG_FALSE;
		}
		return IMG_TRUE;
	}

	#if defined(FIX_HW_BRN_25339) || defined(FIX_HW_BRN_27330)
	/*
		BRN25339 means we can't use bank 1 for data.
	*/
	if (!abCanUseBank[0] && psId->eClass != PDSASM_IDENTIFIER_CLASS_TEMP)
	{
		fprintf(stderr, "%s(%u): error: ", psId->sLocation.pszSourceFile, psId->sLocation.uSourceLine);
		if (psId->eClass == PDSASM_IDENTIFIER_CLASS_DATA)
		{
			fprintf(stderr, "Identifier '%s'", psId->pszName);
		}
		else
		{
			fprintf(stderr, "Immediate value 0x%.8X", psId->uImmediate);
		}
		#if defined(FIX_HW_BRN_25339)
		fprintf(stderr, " can't be assigned to a bank because of BRN25339.\n");
		#else /* defined(FIX_HW_BRN_25339) */
		fprintf(stderr, " can't be assigned to a bank because of BRN27330.\n");
		#endif /* defined(FIX_HW_BRN_25339) */
		return IMG_FALSE;
	}
	#endif /* defined(FIX_HW_BRN_25339) || defined(FIX_HW_BRN_27330) */

	if (!abCanUseBank[0] && !abCanUseBank[1])
	{
		/*
			A temporary can only be assigned to one bank.
		*/
		if (psId->eClass == PDSASM_IDENTIFIER_CLASS_TEMP)
		{
			fprintf(stderr, "%s(%u): error: Identifier '%s' can't be assigned to a bank.\n", 
					psId->sLocation.pszSourceFile,
					psId->sLocation.uSourceLine,
					psId->pszName);
			return IMG_FALSE;
		}

		/*
			For a data identifier it must be duplicated to both banks.
		*/
		psId->sDataStore.abAssignedToBank[0] = IMG_TRUE;
		psId->sDataStore.abAssignedToBank[1] = IMG_TRUE;
	}
	else if (!abCanUseBank[0])
	{
		psId->sDataStore.abAssignedToBank[1] = IMG_TRUE;
	}
	else
	{
		psId->sDataStore.abAssignedToBank[0] = IMG_TRUE;
	}

	return IMG_TRUE;
}

static IMG_VOID AddDataStoreOffset(IMG_UINT32 uIdentifier, IMG_UINT32 uBank, IMG_UINT32 uOffset)
/*****************************************************************************
 FUNCTION	: AddDataStoreOffset
    
 PURPOSE	: Add a new location in the data store to the list associated with
			  an identifier.

 PARAMETERS	: uIdentifier			- Identifier to add the location for.
			  uBank					- Location bank.
			  uOffset				- Bank offset.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PDATASTORE_INFO	psDS = &g_psIdentifierList[uIdentifier].sDataStore;

	psDS->auOffsetCount[uBank]++;
	psDS->apuOffsets[uBank] = realloc(psDS->apuOffsets[uBank], sizeof(IMG_UINT32) * psDS->auOffsetCount[uBank]);
	psDS->apuOffsets[uBank][psDS->auOffsetCount[uBank] - 1] = uOffset;
}

static IMG_PCHAR FindExistingIdentifier(IMG_UINT32	uExistingBank,
										IMG_UINT32	uExistingOffset,
										IMG_UINT32	uEndId)
/*****************************************************************************
 FUNCTION	: FindExistingIdentifier
    
 PURPOSE	: Find the existing identifier which is already assigned to a
			  datastore location.

 PARAMETERS	: uExistingBank			- Data store location to find.
			  uExistingOffset
			  uEndId				- Stopping point for the search.
			  
 RETURNS	: The name of the identifier.
*****************************************************************************/
{
	IMG_UINT32	uId;

	for (uId = 0; uId < uEndId; uId++)
	{
		PIDENTIFIER		psId = &g_psIdentifierList[uId];

		assert(psId->sDataStore.auOffsetCount[uExistingBank] <= 1);

		if (psId->sDataStore.auOffsetCount[uExistingBank] > 0 &&
			psId->sDataStore.apuOffsets[uExistingBank][0] == uExistingOffset)
		{
			return psId->pszName;
		}
	}
	IMG_ABORT();
}

static IMG_BOOL AssignBanks(IMG_VOID)
/*****************************************************************************
 FUNCTION	: AssignBanks
    
 PURPOSE	: Assign banks for all identifiers in the program.

 PARAMETERS	: None.
			  
 RETURNS	: TRUE if assignment succeeded; FALSE otherwise.
*****************************************************************************/
{
	IMG_UINT32	uId;

	/*
		Initialise the banks and data store locations for all identifiers.
	*/
	for (uId = 0; uId < g_uIdentifierCount; uId++)
	{
		IMG_UINT32		uBank;
		PIDENTIFIER		psId = &g_psIdentifierList[uId];

		for (uBank = 0; uBank < 2; uBank++)
		{
			psId->sDataStore.auOffsetCount[uBank] = 0;
			psId->sDataStore.apuOffsets[uBank] = NULL;
			psId->sDataStore.abAssignedToBank[uBank] = IMG_FALSE;
		}
		if (psId->ePreAssignedBank != PDSASM_DATASTORE_BANK_NONE)
		{
			IMG_UINT32	uPreAssignedBank;

			if (psId->eClass != PDSASM_IDENTIFIER_CLASS_TEMP)
			{
				assert(psId->eClass == PDSASM_IDENTIFIER_CLASS_DATA);
					
				fprintf(stderr, "%s(%u): error: Only temporary identifiers can be preassigned a bank.\n", 
						psId->sLocation.pszSourceFile,
						psId->sLocation.uSourceLine);
				return IMG_FALSE;
			}

			switch (psId->ePreAssignedBank)
			{
				case PDSASM_DATASTORE_BANK_0: psId->sDataStore.abAssignedToBank[0] = IMG_TRUE; uPreAssignedBank = 0; break;
				case PDSASM_DATASTORE_BANK_1: psId->sDataStore.abAssignedToBank[1] = IMG_TRUE; uPreAssignedBank = 1; break;
				default: IMG_ABORT();
			}
			if (psId->uPreAssignedOffset != PDSASM_UNDEF)
			{
				if (psId->uPreAssignedOffset < EURASIA_PDS_DATASTORE_TEMPSTART)
				{
					fprintf(stderr, "%s(%u): error: The preassigned location for identifier '%s' is before the "
							"modifiable part of the datastore.\n", 
							psId->sLocation.pszSourceFile,
							psId->sLocation.uSourceLine,
							psId->pszName);
					return IMG_FALSE;
				}
				else if (psId->uPreAssignedOffset >= EURASIA_PDS_DATASTORE_PERBANKSIZE)
				{
					fprintf(stderr, "%s(%u): error: The preassigned location for identifier '%s' is too large.",
							psId->sLocation.pszSourceFile,
							psId->sLocation.uSourceLine,
							psId->pszName);
					return IMG_FALSE;
				}
				else 
				{
					IMG_UINT32	uTempOffset = psId->uPreAssignedOffset - EURASIA_PDS_DATASTORE_TEMPSTART;

					if (g_aabTempUsed[uPreAssignedBank][uTempOffset])
					{
						fprintf(stderr, "%s(%u): error: The preassigned location for identifier '%s' has already "
							"been used for identifier '%s'.",
								psId->sLocation.pszSourceFile,
								psId->sLocation.uSourceLine,
								psId->pszName,
								FindExistingIdentifier(uPreAssignedBank, uTempOffset, uId));
						return IMG_FALSE;
					}
					g_aabTempUsed[uPreAssignedBank][uTempOffset] = IMG_TRUE;

					AddDataStoreOffset(uId, uPreAssignedBank, uTempOffset);
				}
			}
		}
	}

	/*
		Assign a bank for all temporary identifiers.
	*/
	for (uId = 0; uId < g_uIdentifierCount; uId++)
	{
		if (g_psIdentifierList[uId].eClass == PDSASM_IDENTIFIER_CLASS_TEMP)
		{
			if (!AssignBankForIdentifier(uId))
			{
				return IMG_FALSE;
			}
		}
	}
	/*
		Assign a bank for all constant identifiers.
	*/
	for (uId = 0; uId < g_uIdentifierCount; uId++)
	{
		if (g_psIdentifierList[uId].eClass != PDSASM_IDENTIFIER_CLASS_TEMP)
		{
			if (!AssignBankForIdentifier(uId))
			{
				return IMG_FALSE;
			}
		}
	}
	return IMG_TRUE;
}

static IMG_UINT32 AssignTempSpace(IMG_BOOL	bDouble, IMG_UINT32	uBank)
/*****************************************************************************
 FUNCTION	: AssignTempSpace
    
 PURPOSE	: Assign a location in the temporary part of the data store.

 PARAMETERS	: bDouble	- If TRUE assign two locations at even alignment.
			  uBank		- The bank to assign from.
			  
 RETURNS	: The location if assignment succeeded or -1 if assigned failed.
*****************************************************************************/
{
	IMG_UINT32	uLoc;

	for (uLoc = 0; uLoc < sizeof(g_aabTempUsed[0]) / sizeof(g_aabTempUsed[0][0]); uLoc++)
	{
		if (bDouble)
		{
			/*
				Check for two free locations at an even start address.
			*/
			if ((uLoc % 2) == 0 && 
				!g_aabTempUsed[uBank][uLoc] &&
				!g_aabTempUsed[uBank][uLoc + 1])
			{
				g_aabTempUsed[uBank][uLoc] = IMG_TRUE;
				g_aabTempUsed[uBank][uLoc + 1] = IMG_TRUE;
				return uLoc;
			}
		}
		else
		{
			if (!g_aabTempUsed[uBank][uLoc])
			{
				g_aabTempUsed[uBank][uLoc] = IMG_TRUE;
				return uLoc;
			}
		}
	}
	return (PDSASM_UNDEF);
}

/*****************************************************************************
 FUNCTION	: AssignFixedDataSpace
    
 PURPOSE	: Assign a fixed location in the preloaded part of the data store.

 PARAMETERS	: uBank		- Bank to assign.
			  uLoc		- Location to assign.
			  
 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID AssignFixedDataSpace(IMG_UINT32 uBank, IMG_UINT32	uLoc)
{
	IMG_UINT32	uOtherLocInQword = (uLoc & ~1UL) + (1 - (uLoc & 1));

	assert(!g_aabDSUsed[uBank][uLoc]);

	g_aabDSUsed[uBank][uLoc] = IMG_TRUE;

	g_auDSLocationsLeft[uBank]--;
	if (!g_aabDSUsed[uBank][uOtherLocInQword])
	{
		g_auDSDoubleLocationsLeft[uBank]--;
	}
}

static IMG_UINT32 AssignDataSpace(IMG_BOOL	bDouble, IMG_UINT32	uBank)
/*****************************************************************************
 FUNCTION	: AssignDataSpace
    
 PURPOSE	: Assign a location in the preloaded part of the data store.

 PARAMETERS	: bDouble	- If TRUE assign two locations at even alignment.
			  uBank		- The bank to assign from.
			  
 RETURNS	: The location if assignment succeeded or -1 if assigned failed.
*****************************************************************************/
{
	IMG_UINT32	uLoc;

	for (uLoc = 0; uLoc < sizeof(g_aabDSUsed[0]) / sizeof(g_aabDSUsed[0][0]); uLoc++)
	{
		if (bDouble)
		{
			/*
				Check for two free locations at an even start address.
			*/
			if ((uLoc % 2) == 0 && 
				!g_aabDSUsed[uBank][uLoc] &&
				!g_aabDSUsed[uBank][uLoc + 1])
			{
				g_aabDSUsed[uBank][uLoc] = IMG_TRUE;
				g_aabDSUsed[uBank][uLoc + 1] = IMG_TRUE;

				g_auDSLocationsLeft[uBank] -= 2;
				g_auDSDoubleLocationsLeft[uBank]--;

				return uLoc;
			}
		}
		else
		{
			if (!g_aabDSUsed[uBank][uLoc])
			{
				IMG_UINT32	uOtherLocInQword = (uLoc & ~1UL) + (1 - (uLoc & 1));

				g_aabDSUsed[uBank][uLoc] = IMG_TRUE;

				g_auDSLocationsLeft[uBank]--;
				if (!g_aabDSUsed[uBank][uOtherLocInQword])
				{
					g_auDSDoubleLocationsLeft[uBank]--;
				}

				return uLoc;
			}
		}
	}
	return (PDSASM_UNDEF);
}


static IMG_VOID GetMOVSUsedIdentifiers(PINST psInst, 
									   IMG_UINT32 uBank, 
									   IMG_PUINT32 puUseCount,
									   IMG_PUINT32 puUsed)
/*****************************************************************************
 FUNCTION	: GetMOVSUsedIdentifiers
    
 PURPOSE	: Get the list of temporary identifiers used from a MOVS instruction.

 PARAMETERS	: psInst			- Instruction to get the identifiers from.
			  puUseCount		- Returns the number of found identifiers.
			  puUsed			- Returns the list of found identifiers.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uArg;

	*puUseCount = 0;
	for (uArg = 1; uArg < psInst->uArgCount; uArg++)
	{
		if (psInst->psArgs[uArg].eType == PDSASM_REGISTER_TYPE_DATASTORE)
		{
			IMG_UINT32	uIdentifier = psInst->psArgs[uArg].u.uIdentifier;

			/*
				Check that the identifier is of the right class.
			*/
			if (g_psIdentifierList[uIdentifier].eClass != PDSASM_IDENTIFIER_CLASS_TEMP)
			{
				continue;
			}
	
			if (g_psIdentifierList[uIdentifier].sDataStore.abAssignedToBank[uBank])
			{
				IMG_UINT32	uIdx;

				/*
					Check if we found this identifier already.
				*/
				for (uIdx = 0; uIdx < (*puUseCount); uIdx++)
				{
					if (puUsed[uIdx] == uIdentifier)
					{
						break;
					}
				}
				/*
					If not then add it to the list.
				*/
				if (uIdx == (*puUseCount))
				{
					assert((*puUseCount) < 2);
					puUsed[(*puUseCount)++] = uIdentifier;
				}
			}
		}
	}
}

/*****************************************************************************
 FUNCTION	: AssignTempSpaceForMOVS
    
 PURPOSE	: Allocate space in the data store for non-constant identifiers used
			  in a MOVS instruction.

 PARAMETERS	: psInst			- Instruction to assign data store for.
			  
 RETURNS	: TRUE if assignment succeeded; FALSE if no space was available.
*****************************************************************************/
static IMG_BOOL AssignTempSpaceForMOVS(PINST psInst)
{
	IMG_UINT32	uBank;

	for (uBank = 0; uBank < 2; uBank++)
	{
		IMG_UINT32	auUsedFromBank[2];
		IMG_UINT32	uUseCount = 0;
		IMG_UINT32	uArg;

		GetMOVSUsedIdentifiers(psInst, uBank, &uUseCount, auUsedFromBank);

		/*
			If no non-constant identifiers were used then nothing to do.
		*/
		if (uUseCount == 0)
		{
			continue;
		}
		if (uUseCount == 1)
		{
			IMG_UINT32		uIdentifier = auUsedFromBank[0];
			PDATASTORE_INFO	psDS = &g_psIdentifierList[uIdentifier].sDataStore;

			assert(psDS->auOffsetCount[uBank] <= 1);

			/*
				If only one non-constant identifier was used then assign space for
				it if space was already assigned.
			*/
			if (psDS->auOffsetCount[uBank] == 0)
			{
				IMG_UINT32	uLocation = AssignTempSpace(IMG_FALSE, uBank);

				if (uLocation == PDSASM_UNDEF)
				{
					Error(psInst, "Identifier '%s' can't be assigned a data store location.",
						g_psIdentifierList[uIdentifier].pszName);
					return IMG_FALSE;
				}
				AddDataStoreOffset(uIdentifier, uBank, uLocation);
			}	
		}
		else
		{
			IMG_UINT32		uIdentifier0 = auUsedFromBank[0];
			IMG_UINT32		uIdentifier1 = auUsedFromBank[1];
			PDATASTORE_INFO	psDS0 = &g_psIdentifierList[uIdentifier0].sDataStore;
			PDATASTORE_INFO	psDS1 = &g_psIdentifierList[uIdentifier1].sDataStore;

			assert(psDS0->auOffsetCount[uBank] <= 1);
			assert(psDS1->auOffsetCount[uBank] <= 1);

			if (psDS0->auOffsetCount[uBank] != 0 && psDS1->auOffsetCount[uBank] != 0)
			{
				/*
					If two identifiers are used from this bank and they aren't in the same QWORD in
					the data space then there is nothing we can do.
				*/
				if ((psDS0->apuOffsets[uBank][0] >> 1) != (psDS1->apuOffsets[uBank][0] >> 1))
				{
					if (g_psIdentifierList[uIdentifier0].uPreAssignedOffset != PDSASM_UNDEF &&
						g_psIdentifierList[uIdentifier1].uPreAssignedOffset != PDSASM_UNDEF)
					{
						Error(psInst, "The preassigned locations for identifiers '%s' and '%s' are incompatible.",
							g_psIdentifierList[uIdentifier0].pszName,
							g_psIdentifierList[uIdentifier1].pszName);
					}
					else if (g_psIdentifierList[uIdentifier0].uPreAssignedOffset != PDSASM_UNDEF)
					{
						Error(psInst, "The preassigned location for identifier '%s' makes assigning locations for "
							"other identifiers impossible.",
							g_psIdentifierList[uIdentifier0].pszName);
					}
					else if (g_psIdentifierList[uIdentifier1].uPreAssignedOffset != PDSASM_UNDEF)
					{
						Error(psInst, "The preassigned location for identifier '%s' makes assigning locations for "
							"other identifiers impossible.",
							g_psIdentifierList[uIdentifier1].pszName);
					}
					else
					{
						Error(psInst, "Identifiers '%s' and '%s' can't be assigned data store locations.",
							g_psIdentifierList[uIdentifier0].pszName,
							g_psIdentifierList[uIdentifier1].pszName);
					}
					return IMG_FALSE;
				}
			}
			else if (psDS0->auOffsetCount[uBank] != 0 || psDS1->auOffsetCount[uBank] != 0)
			{
				PDATASTORE_INFO	psAssignedDS;
				IMG_UINT32		uQWord, uOffsetInQW, uOtherDword;
				IMG_UINT32		uOtherIdentifier;

				if (psDS0->auOffsetCount[uBank] != 0)
				{
					psAssignedDS = psDS0;
					uOtherIdentifier = uIdentifier1;
				}
				else
				{
					psAssignedDS = psDS1;
					uOtherIdentifier = uIdentifier0;
				}

				/*
					If one identifier has been assigned space in the datastore check if the other dword in
					it's qword is available.
				*/
				uQWord = (psAssignedDS->apuOffsets[uBank][0] >> 1) << 1;
				uOffsetInQW = psAssignedDS->apuOffsets[uBank][0] & 1;
				uOtherDword = uQWord + (1 - uOffsetInQW);

				if (g_aabTempUsed[uBank][uOtherDword])
				{
					Error(psInst, "Identifier '%s' can't be assigned a data store location.",
						g_psIdentifierList[uOtherIdentifier].pszName);
					return IMG_FALSE;
				}
				else
				{
					g_aabTempUsed[uBank][uOtherDword] = IMG_TRUE;

					AddDataStoreOffset(uOtherIdentifier, uBank, uOtherDword);
				}
			}
			else
			{
				/*
					Allocate a qword for both identifiers.
				*/
				IMG_UINT32	uLocation = AssignTempSpace(IMG_TRUE, uBank);

				if (uLocation == PDSASM_UNDEF)
				{
					Error(psInst, "Identifiers '%s' and '%s' can't assigned data store locations.",
						g_psIdentifierList[uIdentifier0].pszName,
						g_psIdentifierList[uIdentifier1].pszName);
					return IMG_FALSE;
				}	

				AddDataStoreOffset(uIdentifier0, uBank, uLocation + 0);
				AddDataStoreOffset(uIdentifier1, uBank, uLocation + 1);
			}
		}

		/*
			For each argument set the assigned location.
		*/
		for (uArg = 1; uArg < psInst->uArgCount; uArg++)
		{
			IMG_UINT32	uIdx;

			for (uIdx = 0; uIdx < uUseCount; uIdx++)
			{
				if (psInst->psArgs[uArg].eType == PDSASM_REGISTER_TYPE_DATASTORE &&
					psInst->psArgs[uArg].u.uIdentifier == auUsedFromBank[uIdx])
				{
					PDATASTORE_INFO	psDS = &g_psIdentifierList[auUsedFromBank[uIdx]].sDataStore;

					assert(psDS->auOffsetCount[uBank] == 1);

					psInst->psArgs[uArg].sLocation.uBank = uBank;
					psInst->psArgs[uArg].sLocation.uOffset = psDS->apuOffsets[uBank][0] + EURASIA_PDS_DATASTORE_TEMPSTART;
				}
			}
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: AssignTempSpaceForNonMOVS
    
 PURPOSE	: Allocate space in the data store for non-constant identifiers used
			  in an instruction other than MOVS.

 PARAMETERS	: psInst			- Instruction to assign data store for.
			  
 RETURNS	: TRUE if assignment succeeded; FALSE if no space was available.
*****************************************************************************/
static IMG_BOOL AssignTempSpaceNonMOVS(PINST psInst)
{
	IMG_UINT32	uArg;

	for (uArg = 0; uArg < psInst->uArgCount; uArg++)
	{
		if (psInst->psArgs[uArg].eType == PDSASM_REGISTER_TYPE_DATASTORE)
		{
			IMG_UINT32		uIdentifier = psInst->psArgs[uArg].u.uIdentifier;

			if (g_psIdentifierList[uIdentifier].eClass == PDSASM_IDENTIFIER_CLASS_TEMP)
			{
				PDATASTORE_INFO	psDS = &g_psIdentifierList[uIdentifier].sDataStore;
				IMG_UINT32		uBank;

				/*
					Get the bank used for this identifier.
				*/
				if (psDS->abAssignedToBank[0])
				{
					assert(!psDS->abAssignedToBank[1]);
					uBank = 0;
				}
				else
				{
					uBank = 1;
				}

				assert(psDS->auOffsetCount[1 - uBank] == 0);
				assert(psDS->auOffsetCount[uBank] <= 1);
				assert(!(uBank == 0 && (GetValidArgTypes(psInst, uArg) & PDSASM_ARGTYPE_DS0) == 0));
				assert(!(uBank == 1 && (GetValidArgTypes(psInst, uArg) & PDSASM_ARGTYPE_DS1) == 0));
			
				/*
					If no space has been assigned for the identifier then assign some.
				*/
				if (psDS->auOffsetCount[uBank] == 0)
				{
					IMG_UINT32	uLocation = AssignTempSpace(IMG_FALSE, uBank);

					if (uLocation == PDSASM_UNDEF)
					{
						Error(psInst, "Identifier '%s' can't be assigned a data store location.",
							  g_psIdentifierList[uIdentifier].pszName);
						return IMG_FALSE;
					}

					AddDataStoreOffset(uIdentifier, uBank, uLocation);
				}

				/*
					Record where the argument is in the data store.
				*/
				psInst->psArgs[uArg].sLocation.uBank = uBank;
				psInst->psArgs[uArg].sLocation.uOffset = psDS->apuOffsets[uBank][0] + EURASIA_PDS_DATASTORE_TEMPSTART;
			}
		}
	}

	return IMG_TRUE;
}

typedef struct
{
	IMG_UINT32		uIdentifier;
	PDATASTORE_INFO	psDS;
	IMG_BOOL		abBank[2];
	IMG_BOOL		bAssigned;
	IMG_UINT32		uLocation;
} MOVS_DATA_ARG, *PMOVS_DATA_ARG;

static IMG_BOOL AssignQwordDataForMOVS(PMOVS_DATA_ARG	psArg0,
									   PMOVS_DATA_ARG	psArg1,
									   IMG_UINT32		uBank)
/*****************************************************************************
 FUNCTION	: AssignQwordDataForMOVS
    
 PURPOSE	: Assign space in the data store for two identifiers.

 PARAMETERS	: psArg		- Identifier to assign space for.
			  uBank		- Bank to assign space from.
			  
 RETURNS	: TRUE if space was available.
*****************************************************************************/
{
	PDATASTORE_INFO	psDS0 = psArg0->psDS;
	PDATASTORE_INFO psDS1 = psArg1->psDS;
	IMG_UINT32		uLocation;

	if (psDS0->auOffsetCount[uBank] > 0 && psDS1->auOffsetCount[uBank] > 0)
	{
		IMG_UINT32	uIdx0, uIdx1;

		/*
			If both identifiers have already been assigned space in the bank check if they
			have already been assigned space in the same qword.
		*/
		for (uIdx0 = 0; uIdx0 < psDS0->auOffsetCount[uBank]; uIdx0++)
		{
			for (uIdx1 = 0; uIdx1 < psDS1->auOffsetCount[uBank]; uIdx1++)
			{
				if ((psDS0->apuOffsets[uBank][uIdx0] >> 1) == (psDS1->apuOffsets[uBank][uIdx1] >> 1))
				{
					assert(psDS0->apuOffsets[uBank][uIdx0] != psDS1->apuOffsets[uBank][uIdx1]);

					psArg0->abBank[1 - uBank] = IMG_FALSE;
					psArg0->uLocation = psDS0->apuOffsets[uBank][uIdx0];
					psArg0->bAssigned = IMG_TRUE;

					psArg1->abBank[1 - uBank] = IMG_FALSE;
					psArg1->uLocation = psDS1->apuOffsets[uBank][uIdx1];
					psArg1->bAssigned = IMG_TRUE;

					return IMG_TRUE;
				}
			}
		}
	}
	else if (psDS0->auOffsetCount[uBank] > 0 || psDS1->auOffsetCount[uBank] > 0)
	{
		PDATASTORE_INFO	psAssignedDS;
		IMG_UINT32		uOtherIdentifier;
		IMG_UINT32		uIdx;

		/*
			If one identifier has been assigned space in the bank check if the
			other location in one of the qwords it has been assigned to is
			free.
		*/

		if (psDS0->auOffsetCount[uBank] > 0)
		{
			psAssignedDS = psDS0;
			uOtherIdentifier = psArg1->uIdentifier;
		}
		else
		{
			psAssignedDS = psDS1;
			uOtherIdentifier = psArg0->uIdentifier;
		}

		for (uIdx = 0; uIdx < psAssignedDS->auOffsetCount[uBank]; uIdx++)
		{
			IMG_UINT32	uOtherDword;
			IMG_UINT32	uOffset;

			uOffset = psAssignedDS->apuOffsets[uBank][uIdx];
			uOtherDword = ((uOffset >> 1) << 1) + (1 - (uOffset & 1));

			if (!g_aabDSUsed[uBank][uOtherDword])
			{
				AssignFixedDataSpace(uBank, uOtherDword);

				AddDataStoreOffset(uOtherIdentifier, uBank, uOtherDword);

				psArg0->abBank[1 - uBank] = IMG_FALSE;
				psArg0->uLocation = uOffset;
				psArg0->bAssigned = IMG_TRUE;

				psArg1->abBank[1 - uBank] = IMG_FALSE;
				psArg1->uLocation = uOtherDword;
				psArg1->bAssigned = IMG_TRUE;

				return IMG_TRUE;
			}
		}
	}

	/*
		Allocate a new qword in the data store and assign each identifier
		to one of the dwords in it.
	*/
	if ((uLocation = AssignDataSpace(IMG_TRUE, uBank)) == PDSASM_UNDEF)
	{
		return IMG_FALSE;
	}
	AddDataStoreOffset(psArg0->uIdentifier, uBank, uLocation + 0);
	AddDataStoreOffset(psArg1->uIdentifier, uBank, uLocation + 1);

	psArg0->abBank[1 - uBank] = IMG_FALSE;
	psArg0->uLocation = uLocation + 0;
	psArg0->bAssigned = IMG_TRUE;

	psArg1->abBank[1 - uBank] = IMG_FALSE;
	psArg1->uLocation = uLocation + 1;
	psArg1->bAssigned = IMG_TRUE;

	return IMG_TRUE;
}

static IMG_BOOL AssignDwordDataForMOVS(PMOVS_DATA_ARG	psArg,
									   IMG_UINT32		uBank)
/*****************************************************************************
 FUNCTION	: AssignDwordDataForMOVS
    
 PURPOSE	: Assign space in the data store for one identifier.

 PARAMETERS	: psArg		- Identifier to assign space for.
			  uBank		- Bank to assign space from.
			  
 RETURNS	: TRUE if space was available.
*****************************************************************************/
{
	IMG_UINT32			uIdentifier = psArg->uIdentifier;
	PDATASTORE_INFO		psDS = psArg->psDS;
			
	if (psDS->auOffsetCount[uBank] == 0)
	{
		IMG_UINT32	uLocation;

		uLocation = AssignDataSpace(IMG_FALSE, uBank);
		if (uLocation == PDSASM_UNDEF)
		{
			return IMG_FALSE;
		}
		AddDataStoreOffset(uIdentifier, uBank, uLocation);
	}

	assert(psArg->abBank[uBank]);

	psArg->uLocation = psDS->apuOffsets[uBank][0];
	psArg->abBank[1 - uBank] = IMG_FALSE;
	psArg->bAssigned = IMG_TRUE;

	return IMG_TRUE;
}

static IMG_BOOL IsValidOtherPair(IMG_UINT32		uDataCount,
								 PMOVS_DATA_ARG	psArgs,
								 IMG_UINT32		uPair00,
								 IMG_UINT32		uPair01,
								 IMG_UINT32		uPair1Bank)
/*****************************************************************************
 FUNCTION	: IsValidOtherPair
    
 PURPOSE	: Check given two identifiers were assigned to one bank that datastore
			  space could be assigned for the remaining identifiers.

 PARAMETERS	: uDataCount		- Count of constant identifiers used by the instruction.
			  psData			- Description of each identifier.
			  uPair00, uPair01	- Indices of the identifiers to be assigned to the same bank.
			  uPair1Bank		- Bank to use for the remaining identifiers.
			  
 RETURNS	: TRUE if the remaining identifiers were assigned data store space..
*****************************************************************************/
{
	IMG_UINT32	uPair10 = PDSASM_UNDEF, uPair11 = PDSASM_UNDEF;
	IMG_UINT32	uIdx;

	for (uIdx = 0; uIdx < uDataCount; uIdx++)
	{
		if (uIdx == uPair00 || uIdx == uPair01)
		{
			continue;
		}
		if (!psArgs[uIdx].abBank[uPair1Bank])
		{
			return IMG_FALSE;
		}
		if (uPair10 == PDSASM_UNDEF)
		{
			uPair10 = uIdx;
		}
		else
		{
			uPair11 = uIdx;
		}
	}
	if (uPair10 == PDSASM_UNDEF)
	{
		/*
			No other identifiers.
		*/
		return IMG_TRUE;
	}
	if (uPair11 == PDSASM_UNDEF)
	{
		/*
			One other identifier.
		*/
		if (psArgs[uPair10].bAssigned)
		{
			return IMG_TRUE;
		}

		return AssignDwordDataForMOVS(psArgs + uPair10, uPair1Bank);
	}
	else
	{	
		/*
			Two other identifiers.
		*/
		if (psArgs[uPair10].bAssigned)
		{
			if (!psArgs[uPair11].bAssigned)
			{
				return IMG_FALSE;
			}
			return IMG_TRUE;
		}

		return AssignQwordDataForMOVS(psArgs + uPair10, psArgs + uPair11, uPair1Bank);
	}
}

static IMG_BOOL CheckAlreadyAllocatedArgs(IMG_UINT32		uDataCount,
										  PMOVS_DATA_ARG	asData)
/*****************************************************************************
 FUNCTION	: CheckAlreadyAllocatedArgs
    
 PURPOSE	: Check if there is any space allocated to arguments which
			  can be reused.

 PARAMETERS	: uDataCount		- Count of constant identifiers used by the instruction.
			  psData			- Description of each identifier.
			  
 RETURNS	: TRUE if space could be reused.
*****************************************************************************/
{
	IMG_UINT32	uIdx;

	for (uIdx = 0; uIdx < uDataCount; uIdx++)
	{
		IMG_UINT32	uBank;

		for (uBank = 0; uBank < 2; uBank++)
		{
			IMG_UINT32	uOffIdx;

			if (!asData[uIdx].abBank[uBank])
			{
				continue;
			}
			if (asData[uIdx].psDS->auOffsetCount[uBank] == 0)
			{
				continue;
			}

			for (uOffIdx = 0; uOffIdx < asData[uIdx].psDS->auOffsetCount[uBank]; uOffIdx++)
			{
				IMG_UINT32	uLocation0 = asData[uIdx].psDS->apuOffsets[uBank][uOffIdx];
				IMG_UINT32	uOtherDword = ((uLocation0 >> 1) << 1) + (1 - (uLocation0 & 1));

				if (g_aabDSUsed[uBank][uOtherDword])
				{
					IMG_UINT32	uOtherIdx;

					for (uOtherIdx = 0; uOtherIdx < uDataCount; uOtherIdx++)
					{
						IMG_UINT32	uOtherOffIdx;
						if (uOtherIdx == uIdx || !asData[uOtherIdx].abBank[uBank])
						{
							continue;
						}
						for (uOtherOffIdx = 0; uOtherOffIdx < asData[uOtherIdx].psDS->auOffsetCount[uBank]; uOtherOffIdx++)
						{
							IMG_UINT32	uLocation1 = asData[uOtherIdx].psDS->apuOffsets[uBank][uOtherOffIdx];

							if ((uLocation0 >> 1) == (uLocation1 >> 1))
							{
								if (IsValidOtherPair(uDataCount, asData, uIdx, uOtherIdx, 1 - uBank))
								{
									asData[uIdx].bAssigned = IMG_TRUE;
									asData[uOtherIdx].bAssigned = IMG_TRUE;

									asData[uIdx].abBank[1 - uBank] = IMG_FALSE;
									asData[uOtherIdx].abBank[1 - uBank] = IMG_FALSE;

									asData[uIdx].uLocation = uLocation0;
									asData[uOtherIdx].uLocation = uLocation1;
									return IMG_TRUE;
								}
							}
						}
					}
				}
				else
				{	
					IMG_UINT32	uOtherIdx;

					for (uOtherIdx = 0; uOtherIdx < uDataCount; uOtherIdx++)
					{
						if (uOtherIdx == uIdx || !asData[uOtherIdx].abBank[uBank])
						{
							continue;
						}
						if (IsValidOtherPair(uDataCount, asData, uIdx, uOtherIdx, 1 - uBank))
						{
							asData[uIdx].bAssigned = IMG_TRUE;
							asData[uOtherIdx].bAssigned = IMG_TRUE;

							asData[uIdx].abBank[1 - uBank] = IMG_FALSE;
							asData[uOtherIdx].abBank[1 - uBank] = IMG_FALSE;

							asData[uIdx].uLocation = uLocation0;
							asData[uOtherIdx].uLocation = uOtherDword;

							AssignFixedDataSpace(uBank, uOtherDword);

							return IMG_TRUE;
						}
					}
				}
			}
		}
	}

	return IMG_FALSE;
}

static IMG_BOOL FindOtherArgForFixedBank(IMG_UINT32		uDataCount,
										 PMOVS_DATA_ARG	psArgs,
										 IMG_UINT32		uBank,
										 IMG_UINT32		uFixedArg,
										 IMG_PUINT32	puOtherArg)
/*****************************************************************************
 FUNCTION	: FindOtherArgForFixedBanks
    
 PURPOSE	: Given an identifier which needs a fixed bank check if there is
			  another identifier which could also use the same bank.

 PARAMETERS	: uDataCount		- Count of constant identifiers used by the instruction.
			  psData			- Description of each identifier.
			  uBank				- Fixed bank.
			  uFixedArg			- Index of the identifier which needs a fixed bank.
			  puOtherArg		- Updated on success with the index of the identifier
								which should use the same bank.
			  
 RETURNS	: TRUE if another identifier was found.
*****************************************************************************/
{
	IMG_UINT32	uIdx;

	if (
			psArgs[uFixedArg].psDS->auOffsetCount[uBank] > 0 &&
			(
				(uDataCount == 2 && g_auDSLocationsLeft[1 - uBank] > 0) ||
				(uDataCount == 3 && g_auDSDoubleLocationsLeft[1 - uBank] > 0)
			)
	   )
	{
		return IMG_FALSE;
	}
	
	for (uIdx = 0; uIdx < uDataCount; uIdx++)
	{
		if (psArgs[uIdx].bAssigned || uIdx == uFixedArg || !psArgs[uIdx].abBank[uBank])
		{
			continue;
		}
		*puOtherArg = uIdx;
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_BOOL CheckAlreadyAllocatedSingleArgs(IMG_UINT32		uDataCount,
												PMOVS_DATA_ARG	psArgs)
/*****************************************************************************
 FUNCTION	: CheckAlreadyAllocatedSingleArgs
    
 PURPOSE	: Check if there is any space allocated to a single argument which
			  can be reused.

 PARAMETERS	: uDataCount		- Count of constant identifiers used by the instruction.
			  psData			- Description of each identifier.
			  
 RETURNS	: TRUE if space could be reused.
*****************************************************************************/
{
	IMG_UINT32	uIdx;

	if (uDataCount == 4)
	{
		return IMG_FALSE;
	}

	for (uIdx = 0; uIdx < uDataCount; uIdx++)
	{
		IMG_UINT32	uBank;

		if (psArgs[uIdx].bAssigned)
		{
			continue;
		}

		for (uBank = 0; uBank < 2; uBank++)
		{
			if (psArgs[uIdx].abBank[uBank] && psArgs[uIdx].psDS->auOffsetCount[uBank] > 0)
			{
				/*
					Check we can assign space for the other two identifiers used.
				*/
				if (IsValidOtherPair(uDataCount, psArgs, uIdx, PDSASM_UNDEF, 1 - uBank))
				{
					return IMG_TRUE;
				}

				/*
					Reuse space already assigned to the identifier.
				*/
				psArgs[uIdx].bAssigned = IMG_TRUE;
				psArgs[uIdx].uLocation = psArgs[uIdx].psDS->apuOffsets[uBank][0];

				return IMG_TRUE;
			}
		}
	}
	return IMG_FALSE;
}

static IMG_VOID SetMOVSDataLocations(PINST psInst, IMG_UINT32 uDataCount, PMOVS_DATA_ARG psData)
/*****************************************************************************
 FUNCTION	: SetMOVSDataLocations
    
 PURPOSE	: Store each argument the assigned data store location.

 PARAMETERS	: psInst			- Instruction to update.
			  uDataCount		- Count of constant identifiers used by the instruction.
			  psData			- Description of each identifier.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uArg;

	for (uArg = 1; uArg < psInst->uArgCount; uArg++)
	{
		IMG_UINT32	uIdentifier;
		if (psInst->psArgs[uArg].eType != PDSASM_REGISTER_TYPE_DATASTORE)
		{
			continue;
		}
		uIdentifier = psInst->psArgs[uArg].u.uIdentifier;
		if (g_psIdentifierList[uIdentifier].eClass != PDSASM_IDENTIFIER_CLASS_TEMP)
		{
			IMG_UINT32	uIdx;

			/*
				Find the corresponding identifier from the list.
			*/
			for (uIdx = 0; uIdx < uDataCount; uIdx++)
			{
				if (psData[uIdx].uIdentifier == uIdentifier)
				{
					if (psData[uIdx].abBank[0])
					{
						assert(!psData[uIdx].abBank[1]);
						psInst->psArgs[uArg].sLocation.uBank = 0;
					}
					else
					{
						assert(psData[uIdx].abBank[1]);
						psInst->psArgs[uArg].sLocation.uBank = 1;
					}
					psInst->psArgs[uArg].sLocation.uOffset = psData[uIdx].uLocation;
					break;
				}
			}
			assert(uIdx < uDataCount);
		}
	}
}

static IMG_BOOL AssignDataSpaceForMOVS(PINST psInst)
/*****************************************************************************
 FUNCTION	: AssignDataSpaceForMOVS
    
 PURPOSE	: Allocate space in the data store for constant identifiers used
			  in a MOVS instruction.

 PARAMETERS	: psInst			- Instruction to assign data store for.
			  
 RETURNS	: TRUE if assignment succeeded; FALSE if no space was available.
*****************************************************************************/
{
	IMG_UINT32		uBank;
	IMG_BOOL		abBankUsedForTemps[2];
	IMG_UINT32		uDataCount;
	MOVS_DATA_ARG	asData[4];
	IMG_UINT32		uArg, uIdx;

	/*
		Check which banks are used for temporary data. We can't use them for
		constants as well.
	*/
	for (uBank = 0; uBank < 2; uBank++)
	{
		IMG_UINT32	auTempUsedFromBank[2];
		IMG_UINT32	uTempUseCount = 0;

		GetMOVSUsedIdentifiers(psInst, uBank, &uTempUseCount, auTempUsedFromBank);

		if (uTempUseCount > 0)
		{
			abBankUsedForTemps[uBank] = IMG_TRUE;
		}
		else
		{
			abBankUsedForTemps[uBank] = IMG_FALSE;
		}
	}

	/*
		Get a list of the constant identifier used in the MOVS instruction.
	*/
	uDataCount = 0;
	for (uArg = 1; uArg < psInst->uArgCount; uArg++)
	{
		if (psInst->psArgs[uArg].eType == PDSASM_REGISTER_TYPE_DATASTORE)
		{
			IMG_UINT32	uIdentifier = psInst->psArgs[uArg].u.uIdentifier;

			if (g_psIdentifierList[uIdentifier].eClass != PDSASM_IDENTIFIER_CLASS_TEMP)
			{
				/*
					Check if we already saw this identifier.
				*/
				for (uIdx = 0; uIdx < uDataCount; uIdx++)
				{
					if (asData[uIdx].uIdentifier == uIdentifier)
					{
						break;
					}
				}
				/*
					Otherwise add it to the list.
				*/
				if (uIdx == uDataCount)
				{
					asData[uIdx].uIdentifier = uIdentifier;
					asData[uIdx].psDS = &g_psIdentifierList[uIdentifier].sDataStore;
					asData[uIdx].abBank[0] = asData[uIdx].psDS->abAssignedToBank[0];
					asData[uIdx].abBank[1] = asData[uIdx].psDS->abAssignedToBank[1];
					asData[uIdx].bAssigned = IMG_FALSE;

					uDataCount++;
				}
			}
		}
	}

	/*
		If no constant identifiers are used then there is nothing to do.
	*/
	if (uDataCount == 0)
	{
		return IMG_TRUE;
	}

	/*
		For any bank used for temporaries flag that constants can't be
		assigned to that bank.
	*/
	if (abBankUsedForTemps[0] || abBankUsedForTemps[1])
	{
		IMG_UINT32	uTempBank;

		if (abBankUsedForTemps[0])
		{
			assert(!abBankUsedForTemps[1]);
			uTempBank = 0;
		}
		else
		{
			uTempBank = 1;
		}

		for (uIdx = 0; uIdx < uDataCount; uIdx++)
		{
			asData[uIdx].abBank[uTempBank] = IMG_FALSE;
			assert(asData[uIdx].abBank[1 - uTempBank]);
		}
	}

	/*
		Check if existing data store locations assigned to the constants
		can be reused for this instruction.
	*/
	if (CheckAlreadyAllocatedArgs(uDataCount, asData))
	{
		SetMOVSDataLocations(psInst, uDataCount, asData);
		return IMG_TRUE;
	}

	/*
		Assign data store for identifier that can only use one bank.
	*/
	for (uBank = 0; uBank < 2; uBank++)
	{
		IMG_UINT32	uOneBankCount;
		IMG_UINT32	auOneBank[2];

		/*
			Check all the identifiers assigned to just this bank.
		*/
		uOneBankCount = 0;
		for (uIdx = 0; uIdx < uDataCount; uIdx++)
		{
			if (asData[uIdx].abBank[uBank] && !asData[uIdx].abBank[1 - uBank])
			{
				assert(uOneBankCount < 2);
				auOneBank[uOneBankCount++] = uIdx;
			}
		}

		if (uOneBankCount == 0)
		{
			continue;
		}
		if (uOneBankCount == 1)
		{
			/*
				Check if there is a constant assigned to both banks that we will put in the
				this bank.
			*/
			if (FindOtherArgForFixedBank(uDataCount, asData, uBank, auOneBank[0], &auOneBank[1]))
			{
				uOneBankCount++;
			}
		}
		/*
			Assign data store locations.
		*/
		if (uOneBankCount == 1)
		{
			AssignDwordDataForMOVS(asData + auOneBank[0], uBank);
		}
		else
		{
			AssignQwordDataForMOVS(asData + auOneBank[0], asData + auOneBank[1], uBank);
		}

		/*
			No other constant can be assigned to this bank.
		*/
		for (uIdx = 0; uIdx < uDataCount; uIdx++)
		{
			if (uIdx == auOneBank[0] || (uOneBankCount == 2 && uIdx == auOneBank[1]))
			{
				continue;
			}
			assert(asData[uIdx].abBank[1 - uBank]);
			asData[uIdx].abBank[uBank] = IMG_FALSE;
		}
	}

	/*
		Check for existing data store locations that can be reused.
	*/
	if (!CheckAlreadyAllocatedSingleArgs(uDataCount, asData))
	{
		SetMOVSDataLocations(psInst, uDataCount, asData);
		return IMG_TRUE;
	}

	/*
		Assign data store locations for all remaining identifiers.
	*/
	for (uIdx = 0; uIdx < uDataCount; uIdx++)
	{
		IMG_UINT32	uOtherIdx, uIdx1;

		if (asData[uIdx].bAssigned)
		{
			continue;
		}
		if (asData[uIdx].abBank[0])
		{
			uBank = 0;
		}
		else
		{
			uBank = 1;
		}
		/*
			Look for another identifier which can be assigned to the 
			same bank.
		*/
		for (uOtherIdx = 0; uOtherIdx < uDataCount; uOtherIdx++)
		{
			if (asData[uOtherIdx].bAssigned || uOtherIdx == uIdx)
			{
				continue;
			}
			if (asData[uOtherIdx].abBank[uBank])
			{
				break;
			}
		}
		/*
			Assign space for the identifier.
		*/
		if (uOtherIdx < uDataCount)
		{
			uIdx1 = uOtherIdx;
			AssignQwordDataForMOVS(asData + uIdx, asData + uIdx1, uBank);
		}
		else
		{
			uIdx1 = PDSASM_UNDEF;
			AssignDwordDataForMOVS(asData + uIdx, uBank);
		}

		/*
			No other identifier can be assigned to the same bank.
		*/
		for (uOtherIdx = 0; uOtherIdx < uDataCount; uOtherIdx++)
		{
			if (uOtherIdx == uIdx || uOtherIdx == uIdx1)
			{
				continue;
			}
			assert(asData[uOtherIdx].abBank[1 - uBank]);
			asData[uOtherIdx].abBank[uBank] = IMG_FALSE;
		}
	}

	/*
		Store at each argument which data store location it uses.
	*/
	SetMOVSDataLocations(psInst, uDataCount, asData);
	return IMG_TRUE;
}

static IMG_BOOL AssignDataSpaceNonMOVS(PINST psInst)
/*****************************************************************************
 FUNCTION	: AssignDataSpaceForNonMOVS
    
 PURPOSE	: Allocate space in the data store for constant identifiers used
			  in an instruction other than MOVS.

 PARAMETERS	: psInst			- Instruction to assign data store for.
			  
 RETURNS	: TRUE if assignment succeeded; FALSE if no space was available.
*****************************************************************************/
{
	IMG_UINT32	uArg;

	/*
		Ignore place holders for branch/call destinations.
	*/
	if (psInst->eOpcode == PDSASM_OPCODE_LABEL)
	{
		return IMG_TRUE;
	}

	for (uArg = 0; uArg < psInst->uArgCount; uArg++)
	{
		if (psInst->psArgs[uArg].eType == PDSASM_REGISTER_TYPE_DATASTORE)
		{
			IMG_UINT32		uIdentifier = psInst->psArgs[uArg].u.uIdentifier;
			PDATASTORE_INFO	psDS = &g_psIdentifierList[uIdentifier].sDataStore;
			IMG_UINT32		uValidTypes = GetValidArgTypes(psInst, uArg);

			/*
				Ignore non-constant identifiers.
			*/
			if (g_psIdentifierList[uIdentifier].eClass == PDSASM_IDENTIFIER_CLASS_TEMP)
			{
				continue;
			}

			if ((uValidTypes & PDSASM_ARGTYPE_DS0) != 0 && (uValidTypes & PDSASM_ARGTYPE_DS1) != 0)
			{
				IMG_UINT32	uBank;

				/*
					Both banks are valid for this argument. Check if the identifier is only
					assigned to one bank.
				*/
				for (uBank = 0; uBank < 2; uBank++)
				{
					if (psDS->abAssignedToBank[uBank] && !psDS->abAssignedToBank[1 - uBank])
					{
						/*
							Allocate some space in the bank if space wasn't already allocated.
						*/
						if (psDS->auOffsetCount[uBank] == 0)
						{
							IMG_UINT32	uLocation;

							uLocation = AssignDataSpace(IMG_FALSE, uBank);
							if (uLocation == PDSASM_UNDEF)
							{
								Error(psInst, "Identifier '%s' couldn't be assigned a data store location.",
									  g_psIdentifierList[uIdentifier].pszName);
								return IMG_FALSE;
							}

							AddDataStoreOffset(uIdentifier, uBank, uLocation);
						}
	
						/*	
							Record in the argument where it is in the data store.
						*/
						psInst->psArgs[uArg].sLocation.uBank = uBank;
						psInst->psArgs[uArg].sLocation.uOffset = psDS->apuOffsets[uBank][0];
						break;
					}
				}

				if (uBank == 2)
				{
					assert(psDS->abAssignedToBank[0] && psDS->abAssignedToBank[1]);

					/*
						This identifier has been assigned to both banks. If no space is allocated
						then allocate some.
					*/
					if (psDS->auOffsetCount[0] == 0 && psDS->auOffsetCount[1] == 0)
					{
						IMG_UINT32	uLocation;

						for (uBank = 0; uBank < 2; uBank++)
						{
							if ((uLocation = AssignDataSpace(IMG_FALSE, 0)) != PDSASM_UNDEF)
							{
								AddDataStoreOffset(uIdentifier, uBank, uLocation);
								break;
							}
						}
						if (uBank == 2)
						{
							Error(psInst, "Identifier '%s' couldn't be assigned a data store location.",
								g_psIdentifierList[uIdentifier].pszName);
							return IMG_FALSE;
						}
					}

					/*
						Actually put the argument in the first bank with some space allocated for
						the identifier.
					*/
					for (uBank = 0; uBank < 2; uBank++)
					{
						if (psDS->auOffsetCount[uBank] > 0)
						{
							psInst->psArgs[uArg].sLocation.uBank = uBank;
							psInst->psArgs[uArg].sLocation.uOffset = psDS->apuOffsets[uBank][0];
							break;
						}
					}
				}
			}
			else
			{
				IMG_UINT32	uBank;
				IMG_UINT32	uLocation;

				assert((uValidTypes & PDSASM_ARGTYPE_DS0) != 0 || (uValidTypes & PDSASM_ARGTYPE_DS1) != 0);
				
				/*
					Work which bank is valid in this argument.
				*/
				if (uValidTypes & PDSASM_ARGTYPE_DS0)
				{
					uBank = 0;
				}
				else
				{
					uBank = 1;
				}

				assert(psDS->abAssignedToBank[uBank]);
				
				/*
					If no space is assigned for the identifier in this bank then assign some.
				*/
				if (psDS->auOffsetCount[uBank] == 0)
				{
					uLocation = AssignDataSpace(IMG_FALSE, uBank);
					if (uLocation == PDSASM_UNDEF)
					{
						Error(psInst, "Identifier '%s' can't be assigned data space.", g_psIdentifierList[uIdentifier].pszName);
						return IMG_FALSE;
					}
					AddDataStoreOffset(uIdentifier, uBank, uLocation);
				}

				/*
					Record where the argument is in the data store.
				*/
				psInst->psArgs[uArg].sLocation.uBank = uBank;
				psInst->psArgs[uArg].sLocation.uOffset = psDS->apuOffsets[uBank][0];
			}
		}
	}

	return IMG_TRUE;
}

#if defined(FIX_HW_BRN_27652)
static IMG_UINT32 GetAssignedBankForTemporary(PIDENTIFIER	psIdentifier)
/*****************************************************************************
 FUNCTION	: GetAssignedBankForTemporary
    
 PURPOSE	: Get the bank used for a temporary variable.

 PARAMETERS	: psIdentifier	 - Temporary variable.
			  
 RETURNS	: The bank number.
*****************************************************************************/
{
	if (psIdentifier->sDataStore.abAssignedToBank[0])
	{
		assert(!psIdentifier->sDataStore.abAssignedToBank[1]);
		return 0;
	}
	else
	{
		assert(psIdentifier->sDataStore.abAssignedToBank[1]);
		return 1;
	}
}

static IMG_BOOL AssignTempSpaceForStore_BRN27652(PINST psInst)
/*****************************************************************************
 FUNCTION	: AssignTempSpaceForStore_BRN27652
    
 PURPOSE	: Allocate space in the data store for the arguments to a
			  STORE instruction.

 PARAMETERS	: psInst			- Instruction to assign data store for.
			  
 RETURNS	: TRUE if assignment succeeded; FALSE if no space was available.
*****************************************************************************/
{
	IMG_UINT32	uArg;
	/*
		Possible valid values for the offset for the start of a
		STORE instruction on cores affected by BRN27652.
	*/
	static const IMG_UINT32 auValidStartOffsets[] = {0, 3, 4, 7};

	uArg = 1;
	while (uArg < psInst->uArgCount)
	{
		PARG		psBaseArg = &psInst->psArgs[uArg];
		IMG_UINT32	uBaseBank;
		PIDENTIFIER	psBaseIdentifier;
		IMG_UINT32	uValidStartOffset;
		IMG_UINT32	uRunLength;
		IMG_BOOL	bValidOffsetAvailable;
		IMG_UINT32	uValidOffsetIdx;
		
		assert(psBaseArg->eType == PDSASM_REGISTER_TYPE_DATASTORE);
		psBaseIdentifier = &g_psIdentifierList[psBaseArg->u.uIdentifier];
		assert(psBaseIdentifier->eClass == PDSASM_IDENTIFIER_CLASS_TEMP);

		/*
			Get the bank assigned to this identifier.
		*/
		uBaseBank = GetAssignedBankForTemporary(psBaseIdentifier);

		/*
			Check for each of the possible valid offsets whether they
			are available in the bank.
		*/
		bValidOffsetAvailable = IMG_FALSE;
		uValidStartOffset = PDSASM_UNDEF;
		for (uValidOffsetIdx = 0; 
			 uValidOffsetIdx < (sizeof(auValidStartOffsets) / sizeof(auValidStartOffsets[0])); 
			 uValidOffsetIdx++)
		{
			uValidStartOffset = auValidStartOffsets[uValidOffsetIdx];
			if (!g_aabTempUsed[uBaseBank][uValidStartOffset])
			{
				bValidOffsetAvailable = IMG_TRUE;
				break;
			}
		}

		/*
			If no valid offset is available then fail to assemble the file.
		*/
		if (!bValidOffsetAvailable)
		{
			Error(psInst, "Identifier '%s' couldn't be assigned a data store location because of BRN27652.",
				  psBaseIdentifier->pszName);
			return IMG_FALSE;
		}

		/*
			Assign as many arguments as possible to the consecutive locations
			following the valid starting offset.
		*/
		uRunLength = 0;
		for (;;)
		{
			PARG		psArg = &psInst->psArgs[uArg + uRunLength];
			IMG_UINT32	uOffset = uValidStartOffset + uRunLength;
			PIDENTIFIER	psIdentifier;

			/*
				Check for reaching the end of the instruction argument list.
			*/
			if ((uArg + uRunLength) >= psInst->uArgCount)
			{
				break;
			}
			/*
				Check for reaching the maximum amount of data which can
				stored by a single STORE instruction.
			*/
			if (uRunLength >= EURASIA_PDS_LOADSTORE_SIZE_MAX)
			{
				break;
			}

			assert(psArg->eType == PDSASM_REGISTER_TYPE_DATASTORE);
			psIdentifier = &g_psIdentifierList[psArg->u.uIdentifier];
			assert(psIdentifier->eClass == PDSASM_IDENTIFIER_CLASS_TEMP);

			/*
				Check if this argument is assigned to a different bank.
			*/
			if (GetAssignedBankForTemporary(psIdentifier) != uBaseBank)
			{
				break;
			}

			/*
				Check if this location in the datastore is still available.
			*/
			if (g_aabTempUsed[uBaseBank][uOffset])
			{
				break;
			}

			/*
				Assign this location in the datastore to the instruction
				arguments.
			*/
			g_aabTempUsed[uBaseBank][uOffset] = IMG_TRUE;
			AddDataStoreOffset(psArg->u.uIdentifier, uBaseBank, uOffset);

			/*
				Record in the instruction the location assigned to this
				argument.
			*/
			psArg->sLocation.uBank = uBaseBank;
			psArg->sLocation.uOffset = EURASIA_PDS_DATASTORE_TEMPSTART + uOffset;

			uRunLength++;
		}

		uArg += uRunLength;
	}

	return IMG_TRUE;
}
#endif /* defined(FIX_HW_BRN_27652) */

static IMG_BOOL AssignDataStoreLocations(IMG_VOID)
/*****************************************************************************
 FUNCTION	: AssignDataStoreLocations
    
 PURPOSE	: Allocate space in the data store for each identifier.

 PARAMETERS	: psInst			- Instruction to assign data store for.
			  
 RETURNS	: TRUE if assignment succeeded; FALSE if no space was available.
*****************************************************************************/
{
	PINST	psInst;

	/*
		Reset the information in each argument which records where it is
		in the data store.
	*/
	for (psInst = g_psInstructionListHead; psInst != NULL; psInst = psInst->psNext)
	{
		IMG_UINT32	uArg;

		for (uArg = 0; uArg < psInst->uArgCount; uArg++)
		{
			if (psInst->psArgs[uArg].eType == PDSASM_REGISTER_TYPE_DATASTORE)
			{
				psInst->psArgs[uArg].sLocation.uBank = PDSASM_UNDEF;
				psInst->psArgs[uArg].sLocation.uOffset = PDSASM_UNDEF;
			}
		}
	}
	
	/*
		Do non-constant allocation before constants since non-constants are restricted to
		only one location in the data store.
		Do MOVS allocation before non-MOVS since sources to MOVS are more restricted in
		the banks they can use.
	*/
	#if defined(FIX_HW_BRN_27652)
	for (psInst = g_psInstructionListHead; psInst != NULL; psInst = psInst->psNext)
	{
		if (psInst->eOpcode == PDSASM_OPCODE_STORE)
		{
			if (!AssignTempSpaceForStore_BRN27652(psInst))
			{
				return IMG_FALSE;
			}
		}
	}
	#endif /* defined(FIX_HW_BRN_27652) */
	for (psInst = g_psInstructionListHead; psInst != NULL; psInst = psInst->psNext)
	{
		if (psInst->eOpcode == PDSASM_OPCODE_MOVS || psInst->eOpcode == PDSASM_OPCODE_MOVSA)
		{
			if (!AssignTempSpaceForMOVS(psInst))
			{
				return IMG_FALSE;
			}
		}
	}
	for (psInst = g_psInstructionListHead; psInst != NULL; psInst = psInst->psNext)
	{
		#if defined(FIX_HW_BRN_27652)
		if (psInst->eOpcode == PDSASM_OPCODE_STORE)
		{
			/*
				Already assigned temporary space for STORE instructions above.
			*/
			continue;
		}
		#endif /* defined(FIX_HW_BRN_27652) */
		if (!(psInst->eOpcode == PDSASM_OPCODE_MOVS || psInst->eOpcode == PDSASM_OPCODE_MOVSA))
		{
			if (!AssignTempSpaceNonMOVS(psInst))
			{
				return IMG_FALSE;
			}
		}
	}
	for (psInst = g_psInstructionListHead; psInst != NULL; psInst = psInst->psNext)
	{
		if (psInst->eOpcode == PDSASM_OPCODE_MOVS || psInst->eOpcode == PDSASM_OPCODE_MOVSA)
		{
			if (!AssignDataSpaceForMOVS(psInst))
			{
				return IMG_FALSE;
			}
		}
	}
	for (psInst = g_psInstructionListHead; psInst != NULL; psInst = psInst->psNext)
	{
		if (!(psInst->eOpcode == PDSASM_OPCODE_MOVS || psInst->eOpcode == PDSASM_OPCODE_MOVSA))
		{
			if (!AssignDataSpaceNonMOVS(psInst))
			{
				return IMG_FALSE;
			}
		}
	}

	return IMG_TRUE;
}

#if defined(SGX_FEATURE_PDS_LOAD_STORE)
static IMG_BOOL GenerateOneLOADSTOREInstruction(PINST		psOrigInst, 
												IMG_UINT32	uRangeStart, 
												IMG_UINT32	uRangeEnd,
												PINST*		ppsInsertAfter)
/*****************************************************************************
 FUNCTION	: GenerateOneLOADSTOREInstruction
    
 PURPOSE	: Generate a LOAD or STORE instruction to load/store a subrange
			  of arguments from another LOAD or STORE instruction.

 PARAMETERS	: psOrigInst		- Original instruction.
			  uRangeStart		- First argument in the subrange.
			  uRangeEnd			- Next argument after the end of the subrange.
			  ppsInsertAfter	- On input contains a pointer to the instruction
								  to insert the new instruction after.
								  On output updated with a pointer to the new
								  instruction.
			  
 RETURNS	: IMG_TRUE on success; IMG_FALSE on failure.
*****************************************************************************/
{
	PINST		psNewInst;
	IMG_UINT32	uArg;
	PINST		psInsertAfter = *ppsInsertAfter;
	IMG_UINT32	uMemOffset;

	/*
		Allocate a new instruction for this LOAD/STORE.
	*/
	psNewInst = malloc(sizeof(*psNewInst));
	if (psNewInst == NULL)
	{
		return IMG_FALSE;
	}

	/*
		Add the new instruction to the list of instructions in the
		program.
	*/
	psNewInst->psPrev = psInsertAfter;
	psNewInst->psNext = psInsertAfter->psNext;

	if (psInsertAfter->psNext != NULL)
	{
		assert(psInsertAfter->psNext->psPrev == psInsertAfter);
		psInsertAfter->psNext->psPrev = psNewInst;
	}
	psInsertAfter->psNext = psNewInst;

	/*
		Copy the predicate from the original instruction.
	*/
	psNewInst->ePredicate = psOrigInst->ePredicate;

	/*
		Copy the opcode from the original instruction.
	*/
	psNewInst->eOpcode = psOrigInst->eOpcode;

	/*
		Copy the source file/source line from the original instruction.
	*/
	psNewInst->sLocation = psOrigInst->sLocation;

	/*
		Create a new argument array for the subrange of arguments we are
		loading/storing here plus the address.
	*/
	psNewInst->uArgCount = 1 + (uRangeEnd - uRangeStart);
	psNewInst->psArgs = malloc(sizeof(psNewInst->psArgs[0]) * (psNewInst->uArgCount + 1));

	/*
		Copy the load/store memory address from the original instruction and add
		an extra offset for the store of the subrange.
	*/
	psNewInst->psArgs[0] = psOrigInst->psArgs[0];
	uMemOffset = (uRangeStart - 1) * sizeof(IMG_UINT32);
	if (psNewInst->psArgs[0].eType == PDSASM_REGISTER_TYPE_IMMEDIATE)
	{
		psNewInst->psArgs[0].u.uImmediate += uMemOffset;
	}
	else
	{
		assert(psNewInst->psArgs[0].eType == PDSASM_REGISTER_TYPE_INSTRUCTION_PARAMETER);
		psNewInst->psArgs[0].u.sParameter.uOffset = uMemOffset;
	}

	/*
		Copy the variables we are loading/storing here to the new instruction.
	*/
	for (uArg = 0; uArg < psNewInst->uArgCount; uArg++)
	{
		psNewInst->psArgs[1 + uArg] = psOrigInst->psArgs[uRangeStart + uArg];
	}

	*ppsInsertAfter = psNewInst;

	return IMG_TRUE;
}

static IMG_BOOL ExpandLOADSTOREInstruction(PINST psOrigInst)
/*****************************************************************************
 FUNCTION	: ExpandLOADSTOREInstruction
    
 PURPOSE	: Expand a LOAD or STORE instruction which can't be implemented
			  with one hardware instruction.

 PARAMETERS	: None.
			  
 RETURNS	: IMG_TRUE on success; IMG_FALSE on failure.
*****************************************************************************/
{
	IMG_UINT32	uCurrentRangeStart;
	PINST		psInsertAfter = psOrigInst;
	IMG_UINT32	uArg;

	uCurrentRangeStart = PDSASM_UNDEF;
	for (uArg = 1; uArg < psOrigInst->uArgCount; uArg++)
	{
		PARG	psArg = &psOrigInst->psArgs[uArg];

		if (uCurrentRangeStart != PDSASM_UNDEF)
		{
			PARG	psPrevArg = &psOrigInst->psArgs[uArg - 1];

			/*
				Check if this argument isn't at the next location in the datastore
				after the previous argument or if we have reached the largest
				possible size of data we can load/store in one instruction.
			*/
			if (psPrevArg->sLocation.uBank != psArg->sLocation.uBank ||
				(psPrevArg->sLocation.uOffset + 1) != psArg->sLocation.uOffset ||
				(uArg - uCurrentRangeStart + 1) > EURASIA_PDS_LOADSTORE_SIZE_MAX)
			{
				/*
					If so then generate a LOAD/STORE instruction for the previous
					range of consecutive arguments.
				*/
				if (!GenerateOneLOADSTOREInstruction(psOrigInst, uCurrentRangeStart, uArg, &psInsertAfter))
				{
					return IMG_FALSE;
				}

				/*
					Start a new range from this argument.
				*/
				uCurrentRangeStart = uArg;
			}
		}
		else
		{
			uCurrentRangeStart = uArg;
		}
	}

	/*
		Generate a final LOAD/STORE instruction for the last arguments to the original
		instruction.
	*/
	if (!GenerateOneLOADSTOREInstruction(psOrigInst, uCurrentRangeStart, psOrigInst->uArgCount, &psInsertAfter))
	{
		return IMG_FALSE;
	}

	/*
		Remove the original instruction from the linked list of instructions in the program.
	*/
	if (psOrigInst->psPrev == NULL)
	{
		g_psInstructionListHead = psOrigInst->psNext;
	}
	else
	{
		psOrigInst->psPrev->psNext = psOrigInst->psNext;
	}
	psOrigInst->psNext->psPrev = psOrigInst->psPrev;

	/*
		Free the original instruction.
	*/
	free(psOrigInst);

	return IMG_TRUE;
}

static IMG_BOOL ExpandLOADSTOREInstructions(IMG_VOID)
/*****************************************************************************
 FUNCTION	: ExpandLOADSTOREInstructions
    
 PURPOSE	: Expand LOAD and STORE instructions which can't be implemented
			  with one hardware instruction.

 PARAMETERS	: None.
			  
 RETURNS	: IMG_TRUE on success; IMG_FALSE on failure.
*****************************************************************************/
{
	PINST	psInst;
	PINST	psNextInst;

	for (psInst = g_psInstructionListHead; psInst != NULL; psInst = psNextInst)
	{
		psNextInst = psInst->psNext;
		if (psInst->eOpcode == PDSASM_OPCODE_LOAD || psInst->eOpcode == PDSASM_OPCODE_STORE)
		{
			if (!ExpandLOADSTOREInstruction(psInst))
			{
				return IMG_FALSE;
			}
		}
	}
	return IMG_TRUE;
}
#endif /* defined(SGX_FEATURE_PDS_LOAD_STORE) */

static IMG_UINT32 AddImmediate(IMG_UINT32	uImmediate, LOCATION* psLocation)
/*****************************************************************************
 FUNCTION	: AddImmediate
    
 PURPOSE	: Add an identifier with an immediate value to the list.

 PARAMETERS	: uImmediate			- Immediate value to add.
			  psLocation			- Location of the identifier in the
									input file.
			  
 RETURNS	: The index of the identifier.
*****************************************************************************/
{
	IMG_UINT32	uId;

	/*
		Check if the same immediate has already been added.
	*/
	for (uId = 0; uId < g_uIdentifierCount; uId++)
	{
		if (g_psIdentifierList[uId].eClass == PDSASM_IDENTIFIER_CLASS_IMMEDIATE &&
			g_psIdentifierList[uId].uImmediate == uImmediate)
		{
			return uId;
		}
	}

	/*
		Grow the list and add the immediate to the end.
	*/
	g_uIdentifierCount++;
	g_psIdentifierList = realloc(g_psIdentifierList, sizeof(*g_psIdentifierList) * g_uIdentifierCount);
	g_psIdentifierList[uId].eClass = PDSASM_IDENTIFIER_CLASS_IMMEDIATE;
	g_psIdentifierList[uId].eType = PDSASM_IDENTIFIER_TYPE_NONE;
	g_psIdentifierList[uId].pszName = NULL;
	g_psIdentifierList[uId].bDefined = IMG_TRUE;
	g_psIdentifierList[uId].uImmediate = uImmediate;
	g_psIdentifierList[uId].ePreAssignedBank = PDSASM_DATASTORE_BANK_NONE;
	g_psIdentifierList[uId].uPreAssignedOffset = PDSASM_UNDEF;
	g_psIdentifierList[uId].uInstParamCount = 0;
	g_psIdentifierList[uId].sLocation = *psLocation;
	return uId;
}

IMG_UINT32 LookupIdentifier(LOCATION sLocation, IMG_PCHAR pszName)
/*****************************************************************************
 FUNCTION	: LookupIdentifier
    
 PURPOSE	: Add a referenced but not yet defined identifier to the list.

 PARAMETERS	: sLocation			- Location for the identifier reference..
			  pszName			- 
			  
 RETURNS	: The index of the identifier.
*****************************************************************************/
{
	IMG_UINT32	uId;

	for (uId = 0; uId < g_uIdentifierCount; uId++)
	{
		if (g_psIdentifierList[uId].eClass != PDSASM_IDENTIFIER_CLASS_IMMEDIATE &&
			strcmp(g_psIdentifierList[uId].pszName, pszName) == 0)
		{
			return uId;
		}
	}

	g_uIdentifierCount++;
	g_psIdentifierList = realloc(g_psIdentifierList, sizeof(*g_psIdentifierList) * g_uIdentifierCount);
	g_psIdentifierList[uId].sLocation = sLocation;
	g_psIdentifierList[uId].eClass = PDSASM_IDENTIFIER_CLASS_UNKNOWN;
	g_psIdentifierList[uId].eType = PDSASM_IDENTIFIER_TYPE_NONE;
	g_psIdentifierList[uId].pszName = strdup(pszName);
	g_psIdentifierList[uId].bDefined = IMG_FALSE;
	g_psIdentifierList[uId].ePreAssignedBank = PDSASM_DATASTORE_BANK_NONE;
	g_psIdentifierList[uId].uPreAssignedOffset = PDSASM_UNDEF;
	g_psIdentifierList[uId].uInstParamCount = 0;
	return uId;
}

IMG_UINT32 AddNamedIdentifier(LOCATION sLocation, 
							  IMG_PCHAR pszName, 
							  PDSASM_IDENTIFIER_CLASS eClass, 
							  PDSASM_IDENTIFIER_TYPE eType,
							  PDSASM_DATASTORE_BANK ePreAssignedBank,
							  IMG_UINT32 uPreAssignedOffset)
/*****************************************************************************
 FUNCTION	: AddNamedIdentifier
    
 PURPOSE	: Add a named identifier to the list.

 PARAMETERS	: sLocation			- Location of the definition.
			  pszName			- Identifier name.
			  eClass			- Identifier class.
			  eType				- Identifier type.
			  
 RETURNS	: The index of the identifier.
*****************************************************************************/
{
	IMG_UINT32	uId;

	for (uId = 0; uId < g_uIdentifierCount; uId++)
	{
		if (g_psIdentifierList[uId].eClass != PDSASM_IDENTIFIER_CLASS_IMMEDIATE &&
			strcmp(g_psIdentifierList[uId].pszName, pszName) == 0)
		{
			/*
				Check for a duplicate definition.	
			*/
			if (g_psIdentifierList[uId].bDefined)
			{
				fprintf(stderr, "%s(%u): error: Identifier '%s' has already been defined.", 
					sLocation.pszSourceFile,
					sLocation.uSourceLine,
					pszName);
				fprintf(stderr, "%s(%u): error: This is the location of the previous definition.", 
					g_psIdentifierList[uId].sLocation.pszSourceFile,
					g_psIdentifierList[uId].sLocation.uSourceLine);
				g_bParserError = IMG_TRUE;
				return PDSASM_UNDEF;
			}
			else
			{
				g_psIdentifierList[uId].eClass = eClass;
				g_psIdentifierList[uId].eType = eType;
				g_psIdentifierList[uId].bDefined = IMG_TRUE;
				g_psIdentifierList[uId].sLocation = sLocation;
				g_psIdentifierList[uId].ePreAssignedBank = ePreAssignedBank;
				g_psIdentifierList[uId].uPreAssignedOffset = uPreAssignedOffset;
				return uId;
			}
		}
	}

	g_uIdentifierCount++;
	g_psIdentifierList = realloc(g_psIdentifierList, sizeof(*g_psIdentifierList) * g_uIdentifierCount);
	g_psIdentifierList[uId].sLocation = sLocation;
	g_psIdentifierList[uId].eClass = eClass;
	g_psIdentifierList[uId].eType = eType;
	g_psIdentifierList[uId].pszName = strdup(pszName);
	g_psIdentifierList[uId].bDefined = IMG_TRUE;
	g_psIdentifierList[uId].ePreAssignedBank = ePreAssignedBank;
	g_psIdentifierList[uId].uPreAssignedOffset = uPreAssignedOffset;
	g_psIdentifierList[uId].uInstParamCount = 0;
	return uId;
}

static IMG_BOOL ResolveImmediates(IMG_VOID)
/*****************************************************************************
 FUNCTION	: ResolveImmediates
    
 PURPOSE	: In instructions which don't support immediates directly replace
			  them by data store locations with the same value as the immediate.

 PARAMETERS	: None.
			  
 RETURNS	: Success or failure.
*****************************************************************************/
{
	PINST	psInst;

	for (psInst = g_psInstructionListHead; psInst != NULL; psInst = psInst->psNext)
	{
		IMG_UINT32	uArg;

		for (uArg = 0; uArg < psInst->uArgCount; uArg++)
		{
			if (psInst->psArgs[uArg].eType == PDSASM_REGISTER_TYPE_IMMEDIATE)
			{
				IMG_UINT32	uValidTypes = g_psInstDesc[psInst->eOpcode].puValidArgTypes[uArg];

				if ((uValidTypes & PDSASM_ARGTYPE_IMMEDIATE) == 0)
				{
					psInst->psArgs[uArg].eType = PDSASM_REGISTER_TYPE_DATASTORE;
					psInst->psArgs[uArg].u.uIdentifier = 
						AddImmediate(psInst->psArgs[uArg].u.uImmediate, &psInst->sLocation);
				}
			}
		}
	}

	return IMG_TRUE;
}

static IMG_BOOL CheckDefines(IMG_VOID)
/*****************************************************************************
 FUNCTION	: CheckDefines
    
 PURPOSE	: Check all referenced identifiers have been defined.

 PARAMETERS	: None.
			  
 RETURNS	: Success or failure.
*****************************************************************************/
{
	PINST	psInst;

	for (psInst = g_psInstructionListHead; psInst != NULL; psInst = psInst->psNext)
	{
		IMG_UINT32	uArg;

		for (uArg = 0; uArg < psInst->uArgCount; uArg++)
		{
			if (psInst->psArgs[uArg].eType == PDSASM_REGISTER_TYPE_DATASTORE)
			{
				IMG_UINT32	uIdentifier = psInst->psArgs[uArg].u.uIdentifier;

				if (!g_psIdentifierList[uIdentifier].bDefined)
				{
					fprintf(stderr, "%s(%u): error: Identifier '%s' is never defined.\n", 
						psInst->sLocation.pszSourceFile,
						psInst->sLocation.uSourceLine,
						g_psIdentifierList[uIdentifier].pszName);
					return IMG_FALSE;
				}
			}
		}
	}
	return IMG_TRUE;
}

static IMG_BOOL ResolveLabels(IMG_VOID)
/*****************************************************************************
 FUNCTION	: ResolveLabels
    
 PURPOSE	: Replace references to label by the offset of the label.

 PARAMETERS	: None.
			  
 RETURNS	: Success or failure.
*****************************************************************************/
{
	PINST		psInst, psNextInst;
	IMG_UINT32	uProgramOffset;

	/*
		For each label get the offset in the program.
	*/
	uProgramOffset = 0;
	for (psInst = g_psInstructionListHead; psInst != NULL; psInst = psNextInst)
	{
		psNextInst = psInst->psNext;
		if (psInst->eOpcode == PDSASM_OPCODE_LABEL)
		{
			IMG_UINT32	uLabelId = psInst->psArgs[0].u.uIdentifier;

			assert(psInst->psArgs[0].eType == PDSASM_REGISTER_TYPE_DATASTORE);
			assert(g_psIdentifierList[uLabelId].eClass == PDSASM_IDENTIFIER_CLASS_LABEL);

			g_psIdentifierList[uLabelId].uLabelOffset = uProgramOffset;

			/*
				Remove the label from the instruction list.
			*/
			if (psInst->psPrev == NULL)
			{
				g_psInstructionListHead = psInst->psNext;
			}
			else
			{
				psInst->psPrev->psNext = psInst->psNext;
			}
			if (psInst->psNext != NULL)
			{
				psInst->psNext->psPrev = psInst->psPrev;
			}
		}
		else
		{
			uProgramOffset++;
		}	
	}

	return IMG_TRUE;
}

static const IMG_PCHAR g_apszPredicateName[] = 
{
	"",			/* PDSASM_PREDICATE_ALWAYS */
	"p0",		/* PDSASM_PREDICATE_P0 */
	"p1",		/* PDSASM_PREDICATE_P1 */
	"p2",		/* PDSASM_PREDICATE_P2 */
	"if0",		/* PDSASM_PREDICATE_IF0 */
	"if1",		/* PDSASM_PREDICATE_IF1 */
	"aluz",		/* PDSASM_PREDICATE_ALUZ */
	"alun",		/* PDSASM_PREDICATE_ALUN */

	"!p0",		/* PDSASM_PREDICATE_NEG_P0 */
	"!p1",		/* PDSASM_PREDICATE_NEG_P1 */
	"!p2",		/* PDSASM_PREDICATE_NEG_P2 */
	"!if0",		/* PDSASM_PREDICATE_NEG_IF0 */
	"!if1",		/* PDSASM_PREDICATE_NEG_IF1 */
	"!aluz",	/* PDSASM_PREDICATE_NEG_ALUZ */
	"!alun",	/* PDSASM_PREDICATE_NEG_ALUN */
};

static IMG_UINT32 EncodePredicate(PINST psInst, PDSASM_PREDICATE	ePredicate)
/*****************************************************************************
 FUNCTION	: EncodePredicate
    
 PURPOSE	: Encode a predicate.

 PARAMETERS	: None.
			  
 RETURNS	: Success or failure.
*****************************************************************************/
{
	switch (ePredicate)
	{
		case PDSASM_PREDICATE_P0: return EURASIA_PDS_CC_P0;
		case PDSASM_PREDICATE_P1: return EURASIA_PDS_CC_P1;
		case PDSASM_PREDICATE_P2: return EURASIA_PDS_CC_P2;
		case PDSASM_PREDICATE_IF0: return EURASIA_PDS_CC_IF0;
		case PDSASM_PREDICATE_IF1: return EURASIA_PDS_CC_IF1;
		case PDSASM_PREDICATE_ALUZ: return EURASIA_PDS_CC_ALUZ;
		case PDSASM_PREDICATE_ALUN: return EURASIA_PDS_CC_ALUN;
		case PDSASM_PREDICATE_ALWAYS: return EURASIA_PDS_CC_ALWAYS;
		default:
		{
			assert(ePredicate < (sizeof(g_apszPredicateName) / sizeof(g_apszPredicateName[0])));
			Error(psInst, "A %s predicate isn't supported on a %s instruction",
				g_apszPredicateName[ePredicate], g_psInstDesc[psInst->eOpcode].pszName);
			return PDSASM_UNDEF;
		}
	}
}


static IMG_UINT32 EncodeFlowControlPredicate(PINST psInst, PDSASM_PREDICATE	ePredicate)
/*****************************************************************************
 FUNCTION	: EncodeFlowControlPredicate
    
 PURPOSE	: Encode a predicate.

 PARAMETERS	: None.
			  
 RETURNS	: Success or failure.
*****************************************************************************/
{
	switch (ePredicate)
	{
		case PDSASM_PREDICATE_P0: return EURASIA_PDS_CC_P0;
		case PDSASM_PREDICATE_P1: return EURASIA_PDS_CC_P1;
		case PDSASM_PREDICATE_P2: return EURASIA_PDS_CC_P2;
		case PDSASM_PREDICATE_IF0: return EURASIA_PDS_CC_IF0;
		case PDSASM_PREDICATE_IF1: return EURASIA_PDS_CC_IF1;
		case PDSASM_PREDICATE_ALUZ: return EURASIA_PDS_CC_ALUZ;
		case PDSASM_PREDICATE_ALUN: return EURASIA_PDS_CC_ALUN;
		case PDSASM_PREDICATE_ALWAYS: return EURASIA_PDS_CC_ALWAYS;
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
		case PDSASM_PREDICATE_NEG_P0: return (EURASIA_PDS_CC_P0 | EURASIA_PDS_FLOW_CC_NEGATE);
		case PDSASM_PREDICATE_NEG_P1: return (EURASIA_PDS_CC_P1 | EURASIA_PDS_FLOW_CC_NEGATE);
		case PDSASM_PREDICATE_NEG_P2: return (EURASIA_PDS_CC_P2 | EURASIA_PDS_FLOW_CC_NEGATE);
		case PDSASM_PREDICATE_NEG_IF0: return (EURASIA_PDS_CC_IF0 | EURASIA_PDS_FLOW_CC_NEGATE);
		case PDSASM_PREDICATE_NEG_IF1: return (EURASIA_PDS_CC_IF1 | EURASIA_PDS_FLOW_CC_NEGATE);
		case PDSASM_PREDICATE_NEG_ALUZ: return (EURASIA_PDS_CC_ALUZ | EURASIA_PDS_FLOW_CC_NEGATE);
		case PDSASM_PREDICATE_NEG_ALUN: return (EURASIA_PDS_CC_ALUN | EURASIA_PDS_FLOW_CC_NEGATE);
#endif
		default:
		{
			assert(ePredicate < (sizeof(g_apszPredicateName) / sizeof(g_apszPredicateName[0])));
			Error(psInst, "A %s predicate isn't supported on a %s instruction",
				g_apszPredicateName[ePredicate], g_psInstDesc[psInst->eOpcode].pszName);
			return PDSASM_UNDEF;
		}
	}
}

static IMG_UINT32 EncodeFlagsOffset(PARG psArg)
/*****************************************************************************
 FUNCTION	: EncodeFlagsOffset
    
 PURPOSE	: For instruction arguments using 16-bit addressing add on the
			  extra offset for the '.l' and '.h' flags.

 PARAMETERS	: None.
			  
 RETURNS	: The offset.
*****************************************************************************/
{
	switch (psArg->eFlag)
	{
		case PDSASM_ARGFLAG_NONE:
		case PDSASM_ARGFLAG_LOW:	return 0;
		case PDSASM_ARGFLAG_HIGH:	return 1;
		default: IMG_ABORT();
	}
}

#ifdef DEBUG
static IMG_VOID CheckMOVSInstruction(PINST psInst)
/*****************************************************************************
 FUNCTION	: CheckMOVSInstruction
    
 PURPOSE	: Check the arguments to a MOVS instruction are correct.

 PARAMETERS	: psInst		- The instruction to check.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32				uArgIdx;
	IMG_BOOL				bSpecialUsed = IMG_FALSE;
	IMG_BOOL				bDS0Used = IMG_FALSE;
	IMG_BOOL				bDS1Used = IMG_FALSE;
	PDSASM_SPECIAL_REGISTER	eSpecialUsed = PDSASM_SPECIAL_REGISTER_UNKNOWN;
	IMG_UINT32				uDS0Used = PDSASM_UNDEF;
	IMG_UINT32				uDS1Used = PDSASM_UNDEF;

	for (uArgIdx = 1; uArgIdx < psInst->uArgCount; uArgIdx++)
	{
		if (psInst->psArgs[uArgIdx].eType == PDSASM_REGISTER_TYPE_SPECIAL)
		{
#if !defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
			assert(!bDS0Used);
			assert(!bSpecialUsed || eSpecialUsed == psInst->psArgs[uArgIdx].u.eSpecialRegister);
#endif

			bSpecialUsed = IMG_TRUE;
			eSpecialUsed = psInst->psArgs[uArgIdx].u.eSpecialRegister;
		}
		else
		{
			assert(psInst->psArgs[uArgIdx].eType == PDSASM_REGISTER_TYPE_DATASTORE);

			if (psInst->psArgs[uArgIdx].sLocation.uBank == 0)
			{
#if !defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
				assert(!bSpecialUsed);
#endif
				assert(!bDS0Used || uDS0Used == (psInst->psArgs[uArgIdx].sLocation.uOffset >> 1));

				bDS0Used = IMG_TRUE;
				uDS0Used = psInst->psArgs[uArgIdx].sLocation.uOffset >> 1;
			}
			else
			{
				assert(!bDS1Used || uDS1Used == (psInst->psArgs[uArgIdx].sLocation.uOffset >> 1));

				bDS1Used = IMG_TRUE;
				uDS1Used = psInst->psArgs[uArgIdx].sLocation.uOffset >> 1;
			}
		}
	}
}
#endif /* DEBUG */

static IMG_BOOL EncodeMOVSInstruction(IMG_PUINT8 pui8Dest, PINST psInst)
/*****************************************************************************
 FUNCTION	: EncodeMOVSInstruction
    
 PURPOSE	: Encode a MOVS instruction.

 PARAMETERS	: pui8Dest			- Destination for the encoded instruction.
			  psInst			- Instruction to encode.
			  
 RETURNS	: Success or failure.
*****************************************************************************/
{
	IMG_UINT32	uCC;
	IMG_UINT32	uDest;
	IMG_UINT32	uSrc1Sel;
	IMG_UINT32	uSrc1;
	IMG_UINT32	uSrc2Sel;
	IMG_UINT32	uSrc2;
	IMG_UINT32	puSwiz[4];
	IMG_UINT32	uArgIdx;
	IMG_BOOL	bArg0Used = IMG_FALSE;

	#ifdef DEBUG
	CheckMOVSInstruction(psInst);
	#endif /* DEBUG */

	uCC = EncodePredicate(psInst, psInst->ePredicate);
	if (uCC == PDSASM_UNDEF)
	{
		return IMG_FALSE;
	}

	/*
		Get the command type.
	*/
	assert(psInst->psArgs[0].eType == PDSASM_REGISTER_TYPE_SPECIAL);
	switch (psInst->psArgs[0].u.eSpecialRegister)
	{
		case PDSASM_SPECIAL_REGISTER_DOUTD: uDest = EURASIA_PDS_MOVS_DEST_DOUTD; break;
		case PDSASM_SPECIAL_REGISTER_DOUTI: uDest = EURASIA_PDS_MOVS_DEST_DOUTI; break;
		case PDSASM_SPECIAL_REGISTER_DOUTT: uDest = EURASIA_PDS_MOVS_DEST_DOUTT; break;
		case PDSASM_SPECIAL_REGISTER_DOUTU: uDest = EURASIA_PDS_MOVS_DEST_DOUTU; break;
		case PDSASM_SPECIAL_REGISTER_DOUTA: uDest = EURASIA_PDS_MOVS_DEST_DOUTA; break;
		case PDSASM_SPECIAL_REGISTER_TIMER: uDest = EURASIA_PDS_MOVS_DEST_TIM; break;
		#if defined(SGX543) || defined(SGX544) || defined(SGX554)
		case PDSASM_SPECIAL_REGISTER_DOUTO: uDest = EURASIA_PDS_MOVS_DEST_DOUTO; break;
		#endif /* defined(SGX543) || defined(SGX544) || defined(SGX554) */
		#if defined(SGX545)
		case PDSASM_SPECIAL_REGISTER_DOUTC: uDest = EURASIA_PDS_MOVS_DEST_DOUTC; break;
		case PDSASM_SPECIAL_REGISTER_DOUTS: uDest = EURASIA_PDS_MOVS_DEST_DOUTS; break;
		#endif /* defined(SGX545) */
		default: IMG_ABORT();
	}

	uSrc1Sel = EURASIA_PDS_MOVS_SRC1SEL_DS0;
	uSrc2Sel = EURASIA_PDS_MOVS_SRC2SEL_DS1;
	uSrc1 = uSrc2 = 0;

	#if defined(FIX_HW_BRN_25339) || defined(FIX_HW_BRN_27330)
	/*
		If the argument from DS1 is unused then point it to the temporary part of
		the datastore to avoid references to DS1 constants.
	*/
	uSrc2 = EURASIA_PDS_DATASTORE_TEMPSTART >> 1;
	#endif /* defined(FIX_HW_BRN_25339) || defined(FIX_HW_BRN_27330)*/

	for (uArgIdx = 1; uArgIdx < psInst->uArgCount; uArgIdx++)
	{
		if (psInst->psArgs[uArgIdx].eType == PDSASM_REGISTER_TYPE_DATASTORE)
		{
			if (psInst->psArgs[uArgIdx].sLocation.uBank == 0)
			{
				bArg0Used = IMG_TRUE;
			}
		}
	}


	memset(puSwiz, 0, sizeof(puSwiz));
	for (uArgIdx = 1; uArgIdx < psInst->uArgCount; uArgIdx++)
	{
		IMG_UINT32	uDestIdx;

		/*
			For MOVSA the last argument uses SWIZ3 to it can be added to SWIZ0.
		*/
		if (psInst->eOpcode == PDSASM_OPCODE_MOVSA && uArgIdx == (psInst->uArgCount - 1))
		{
			uDestIdx = 3;
		}
		else
		{
			uDestIdx = uArgIdx - 1;
		}

		if (psInst->psArgs[uArgIdx].eType == PDSASM_REGISTER_TYPE_SPECIAL)
		{
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
			/* Arg0 already used, use arg1 */
			if(bArg0Used)
			{
				uSrc2Sel = EURASIA_PDS_MOVS_SRC2SEL_REG;

				switch (psInst->psArgs[uArgIdx].u.eSpecialRegister)
				{
					case PDSASM_SPECIAL_REGISTER_IR0: uSrc2 = EURASIA_PDS_MOVS_SRC2_IR0; break;
					case PDSASM_SPECIAL_REGISTER_IR1: uSrc2 = EURASIA_PDS_MOVS_SRC2_IR1; break;
					case PDSASM_SPECIAL_REGISTER_TIMER: uSrc2 = EURASIA_PDS_MOVS_SRC2_TIM; break;
					default:
					{
						Error(psInst, "Invalid special register source for %s.", g_psInstDesc[psInst->eOpcode].pszName);
						return IMG_FALSE;
					}
				}

				/*
					Always use the lower 32-bits of the source.
				*/
				puSwiz[uDestIdx] = EURASIA_PDS_MOVS_SWIZ_SRC2L;
			}
			else
#else
			assert(!bArg0Used);
#endif
			{
				/*
					Use the first source for a special register.
				*/

				uSrc1Sel = EURASIA_PDS_MOVS_SRC1SEL_REG;

				switch (psInst->psArgs[uArgIdx].u.eSpecialRegister)
				{
					case PDSASM_SPECIAL_REGISTER_IR0: uSrc1 = EURASIA_PDS_MOVS_SRC1_IR0; break;
					case PDSASM_SPECIAL_REGISTER_IR1: uSrc1 = EURASIA_PDS_MOVS_SRC1_IR1; break;
					case PDSASM_SPECIAL_REGISTER_TIMER: uSrc1 = EURASIA_PDS_MOVS_SRC1_TIM; break;
					default:
					{
						Error(psInst, "Invalid special register source for %s.", g_psInstDesc[psInst->eOpcode].pszName);
						return IMG_FALSE;
					}
				}

				/*
					Always use the lower 32-bits of the source.
				*/
				puSwiz[uDestIdx] = EURASIA_PDS_MOVS_SWIZ_SRC1L;
			}
		}
		else
		{
			assert(psInst->psArgs[uArgIdx].eType == PDSASM_REGISTER_TYPE_DATASTORE);

			if (psInst->psArgs[uArgIdx].sLocation.uBank == 0)
			{
				/*
					Uses the first source for a DS0 argument.
				*/
				uSrc1 = psInst->psArgs[uArgIdx].sLocation.uOffset >> 1;

				/*
					Swizzle either the low or high half of the qword.
				*/
				if (psInst->psArgs[uArgIdx].sLocation.uOffset & 1)
				{
					puSwiz[uDestIdx] = EURASIA_PDS_MOVS_SWIZ_SRC1H;
				}
				else
				{
					puSwiz[uDestIdx] = EURASIA_PDS_MOVS_SWIZ_SRC1L;
				}
			}
			else
			{
				/*
					Use the second source for a DS1 argument.
				*/
				assert(psInst->psArgs[uArgIdx].sLocation.uBank == 1);
				uSrc2 = psInst->psArgs[uArgIdx].sLocation.uOffset >> 1;

				/*
					Swizzle either the low or high half of the qword.
				*/
				if (psInst->psArgs[uArgIdx].sLocation.uOffset & 1)
				{
					puSwiz[uDestIdx] = EURASIA_PDS_MOVS_SWIZ_SRC2H;
				}
				else
				{
					puSwiz[uDestIdx] = EURASIA_PDS_MOVS_SWIZ_SRC2L;
				}
			}
		}
	}

	switch (psInst->eOpcode)
	{
		case PDSASM_OPCODE_MOVS:
		{
			switch(uDest)
			{
				case EURASIA_PDS_MOVS_DEST_DOUTT:
				{
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
					assert(puSwiz[0] == EURASIA_PDS_MOVS_SWIZ_SRC1L || puSwiz[0] == EURASIA_PDS_MOVS_SWIZ_SRC2L );
		
					if(puSwiz[0] == EURASIA_PDS_MOVS_SWIZ_SRC1L)
					{
						*(IMG_PUINT32)pui8Dest = PDSEncodeDOUTT(uCC, uSrc1Sel, uSrc1, uSrc2Sel, uSrc2, IMG_FALSE, 0/* bMinPack */, 0/* ui32FormatConv */);
					}
					else
					{
						*(IMG_PUINT32)pui8Dest = PDSEncodeDOUTT(uCC, uSrc1Sel, uSrc1, uSrc2Sel, uSrc2, IMG_TRUE, 0/* bMinPack */, 0/* ui32FormatConv */);
					}
#else
					*(IMG_PUINT32)pui8Dest = PDSEncodeDOUTT(uCC, uSrc1Sel, uSrc1, uSrc2Sel, uSrc2, puSwiz[0], puSwiz[1], puSwiz[2], puSwiz[3]);
#endif
					break;
				}
				case EURASIA_PDS_MOVS_DEST_DOUTI:
				{
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
					*(IMG_PUINT32)pui8Dest = PDSEncodeDOUTI(uCC, uSrc1Sel, uSrc1, uSrc2Sel, uSrc2, 
															puSwiz[0], puSwiz[1], 0/* ui32TagSize */, 0/* ui32ItrSize */);
#else
					*(IMG_PUINT32)pui8Dest = PDSEncodeDOUTI(uCC, uSrc1Sel, uSrc1, uSrc2Sel, uSrc2, puSwiz[0], puSwiz[1], puSwiz[2], puSwiz[3]);
#endif
					break;
				}
				default:
				{
					*(IMG_PUINT32)pui8Dest = PDSEncodeMOVS(uCC, uDest, uSrc1Sel, uSrc1, uSrc2Sel, uSrc2, 
															puSwiz[0], puSwiz[1], puSwiz[2], puSwiz[3]);
					break;
				}
			}

			break;
		}
		case PDSASM_OPCODE_MOVSA:
		{
			*(IMG_PUINT32)pui8Dest = PDSEncodeMOVSA(uCC, uDest, uSrc1Sel, uSrc1, uSrc2Sel, uSrc2, puSwiz[0], puSwiz[1], puSwiz[2], puSwiz[3]);
			break;
		}
		default:
			IMG_ABORT();
	}
	return IMG_TRUE;
}

static IMG_BOOL EncodeMOV16Instruction(IMG_PUINT8 pui8Dest, PINST psInst)
/*****************************************************************************
 FUNCTION	: EncodeMOV16Instruction
    
 PURPOSE	: Encode a MOV16 instruction.

 PARAMETERS	: pui8Dest			- Destination for the encoded instruction.
			  psInst			- Instruction to encode.
			  
 RETURNS	: Success or failure.
*****************************************************************************/
{
	IMG_UINT32	uCC;
	IMG_UINT32	uDestSel;
	IMG_UINT32	uDest;
	IMG_UINT32	uSrcSel;
	IMG_UINT32	uSrc;

	assert(psInst->eOpcode == PDSASM_OPCODE_MOV16);

	uCC = EncodePredicate(psInst, psInst->ePredicate);
	if (uCC == PDSASM_UNDEF)
	{
		return IMG_FALSE;
	}

	/*
		Get the encoded destination.
	*/
	assert(psInst->psArgs[0].eType == PDSASM_REGISTER_TYPE_DATASTORE);
	assert(psInst->psArgs[0].sLocation.uOffset <= EURASIA_PDS_MOV16_DEST_DS_LIMIT);
	if (psInst->psArgs[0].sLocation.uBank == 0)
	{
		uDestSel = EURASIA_PDS_MOV16_DESTSEL_DS0;
	}
	else
	{
		uDestSel = EURASIA_PDS_MOV16_DESTSEL_DS1;
	}
	uDest = psInst->psArgs[0].sLocation.uOffset << 1;
	uDest += EncodeFlagsOffset(&psInst->psArgs[0]);

	/*
		Get the encoded src1.
	*/
	if (psInst->psArgs[1].eType == PDSASM_REGISTER_TYPE_SPECIAL)
	{
		uSrcSel = EURASIA_PDS_MOV16_SRCSEL_REG;

		switch (psInst->psArgs[1].u.eSpecialRegister)
		{
			case PDSASM_SPECIAL_REGISTER_IR0: uSrc = EURASIA_PDS_MOV16_SRC_IR0L; break;
			case PDSASM_SPECIAL_REGISTER_IR1: uSrc = EURASIA_PDS_MOV16_SRC_IR1L; break;
			case PDSASM_SPECIAL_REGISTER_TIMER: uSrc = EURASIA_PDS_MOV16_SRC_TIML; break;
			case PDSASM_SPECIAL_REGISTER_PC: uSrc = EURASIA_PDS_MOV16_SRC_PCL; break;
			default:
			{
				Error(psInst, "Invalid special register source for %s.", g_psInstDesc[psInst->eOpcode].pszName);
				return IMG_FALSE;
			}
		}
		uSrc += EncodeFlagsOffset(&psInst->psArgs[0]);
	}
	else
	{
		assert(psInst->psArgs[1].eType == PDSASM_REGISTER_TYPE_DATASTORE);
		
		if (psInst->psArgs[1].sLocation.uBank == 0)
		{
			uSrcSel = EURASIA_PDS_MOV16_SRCSEL_DS0;
			uSrc = psInst->psArgs[1].sLocation.uOffset;
		}
		else
		{
			assert(psInst->psArgs[1].sLocation.uBank == 1);

			uSrcSel = EURASIA_PDS_MOV16_SRCSEL_DS1;
			uSrc = psInst->psArgs[1].sLocation.uOffset;
		}
		uSrc += EncodeFlagsOffset(&psInst->psArgs[0]);

		assert(uSrc <= EURASIA_PDS_MOV16_SRC_DS_LIMIT);
	}

	/*
		Actually encode the instruction.
	*/
	*(IMG_PUINT32)pui8Dest = PDSEncodeMOV16(uCC, uDestSel, uDest, uSrcSel, uSrc);

	return IMG_TRUE;
}

static IMG_BOOL EncodeMOV32Instruction(IMG_PUINT8 pui8Dest, PINST psInst)
/*****************************************************************************
 FUNCTION	: EncodeMOV32Instruction
    
 PURPOSE	: Encode a MOV32 instruction.

 PARAMETERS	: pui8Dest			- Destination for the encoded instruction.
			  psInst			- Instruction to encode.
			  
 RETURNS	: Success or failure.
*****************************************************************************/
{
	IMG_UINT32	uCC;
	IMG_UINT32	uDestSel;
	IMG_UINT32	uDest;
	IMG_UINT32	uSrcSel;
	IMG_UINT32	uSrc;

	assert(psInst->eOpcode == PDSASM_OPCODE_MOV32);

	uCC = EncodePredicate(psInst, psInst->ePredicate);
	if (uCC == PDSASM_UNDEF)
	{
		return IMG_FALSE;
	}

	/*
		Get the encoded destination.
	*/
	assert(psInst->psArgs[0].eType == PDSASM_REGISTER_TYPE_DATASTORE);
	assert(psInst->psArgs[0].sLocation.uOffset <= EURASIA_PDS_MOV32_DEST_DS_LIMIT);
	if (psInst->psArgs[0].sLocation.uBank == 0)
	{
		uDestSel = EURASIA_PDS_MOV32_DESTSEL_DS0;
	}
	else
	{
		uDestSel = EURASIA_PDS_MOV32_DESTSEL_DS1;
	}
	uDest = psInst->psArgs[0].sLocation.uOffset;

	/*
		Get the encoded src1.
	*/
	if (psInst->psArgs[1].eType == PDSASM_REGISTER_TYPE_SPECIAL)
	{
		uSrcSel = EURASIA_PDS_MOV32_SRCSEL_REG;

		switch (psInst->psArgs[1].u.eSpecialRegister)
		{
			case PDSASM_SPECIAL_REGISTER_IR0: uSrc = EURASIA_PDS_MOV32_SRC_IR0; break;
			case PDSASM_SPECIAL_REGISTER_IR1: uSrc = EURASIA_PDS_MOV32_SRC_IR1; break;
			case PDSASM_SPECIAL_REGISTER_TIMER: uSrc = EURASIA_PDS_MOV32_SRC_TIM; break;
			case PDSASM_SPECIAL_REGISTER_PC: uSrc = EURASIA_PDS_MOV32_SRC_PC; break;
			default:
			{
				Error(psInst, "Invalid special register source for %s.", g_psInstDesc[psInst->eOpcode].pszName);
				return IMG_FALSE;
			}
		}
	}
	else
	{
		assert(psInst->psArgs[1].eType == PDSASM_REGISTER_TYPE_DATASTORE);
		assert(psInst->psArgs[1].sLocation.uOffset <= EURASIA_PDS_MOV32_SRC_DS_LIMIT);

		if (psInst->psArgs[1].sLocation.uBank == 0)
		{
			uSrcSel = EURASIA_PDS_MOV32_SRCSEL_DS0;
			uSrc = psInst->psArgs[1].sLocation.uOffset;
		}
		else
		{
			assert(psInst->psArgs[1].sLocation.uBank == 1);

			uSrcSel = EURASIA_PDS_MOV32_SRCSEL_DS1;
			uSrc = psInst->psArgs[1].sLocation.uOffset;
		}
	}

	/*
		Actually encode the instruction.
	*/
	*(IMG_PUINT32)pui8Dest = PDSEncodeMOV32(uCC, uDestSel, uDest, uSrcSel, uSrc);

	return IMG_TRUE;
}

static IMG_BOOL EncodeABSInstruction(IMG_PUINT8 pui8Dest, PINST psInst)
/*****************************************************************************
 FUNCTION	: EncodeABSInstruction
    
 PURPOSE	: Encode a ABS instruction.

 PARAMETERS	: pui8Dest			- Destination for the encoded instruction.
			  psInst			- Instruction to encode.
			  
 RETURNS	: Success or failure.
*****************************************************************************/
{
	IMG_UINT32	uCC;
	IMG_UINT32	uDestSel;
	IMG_UINT32	uDest;
	IMG_UINT32	uSrc1Sel;
	IMG_UINT32	uSrc1;
	IMG_UINT32	uSrc2;
	IMG_UINT32	uSrcSel;

	assert(psInst->eOpcode == PDSASM_OPCODE_ABS);

	uCC = EncodePredicate(psInst, psInst->ePredicate);
	if (uCC == PDSASM_UNDEF)
	{
		return IMG_FALSE;
	}

	/*
		Get the encoded destination.
	*/
	assert(psInst->psArgs[0].eType == PDSASM_REGISTER_TYPE_DATASTORE);
	assert(psInst->psArgs[0].sLocation.uOffset <= EURASIA_PDS_ABS_DEST_DS_LIMIT);
	if (psInst->psArgs[0].sLocation.uBank == 0)
	{
		uDestSel = EURASIA_PDS_ABS_DESTSEL_DS0;
	}
	else
	{
		uDestSel = EURASIA_PDS_ABS_DESTSEL_DS1;
	}
	uDest = psInst->psArgs[0].sLocation.uOffset;

	/*
		Get the encoded src1.
	*/
	if (psInst->psArgs[1].eType == PDSASM_REGISTER_TYPE_SPECIAL)
	{
		uSrcSel = EURASIA_PDS_ABS_SRCSEL_SRC1;
		uSrc1Sel = EURASIA_PDS_ARITH_SRC1SEL_REG;
		uSrc2 = 0;

		switch (psInst->psArgs[1].u.eSpecialRegister)
		{
			case PDSASM_SPECIAL_REGISTER_IR0: uSrc1 = EURASIA_PDS_ABS_SRC1_IR0; break;
			case PDSASM_SPECIAL_REGISTER_IR1: uSrc1 = EURASIA_PDS_ABS_SRC1_IR1; break;
			case PDSASM_SPECIAL_REGISTER_TIMER: uSrc1 = EURASIA_PDS_ABS_SRC1_TIM; break;
			default:
			{
				Error(psInst, "Invalid special register source for %s.", g_psInstDesc[psInst->eOpcode].pszName);
				return IMG_FALSE;
			}
		}
	}
	else
	{
		assert(psInst->psArgs[1].eType == PDSASM_REGISTER_TYPE_DATASTORE);

		if (psInst->psArgs[1].sLocation.uBank == 0)
		{
			assert(psInst->psArgs[1].sLocation.uBank == 0);
			assert(psInst->psArgs[1].sLocation.uOffset <= EURASIA_PDS_ABS_SRC1_DS0_LIMIT);

			uSrcSel = EURASIA_PDS_ABS_SRCSEL_SRC1;
			uSrc1Sel = EURASIA_PDS_ABS_SRC1SEL_DS0;
			uSrc1 = psInst->psArgs[1].sLocation.uOffset;
			uSrc2 = 0;
		}
		else
		{
			uSrcSel = EURASIA_PDS_ABS_SRCSEL_SRC2;
			uSrc1Sel = EURASIA_PDS_ABS_SRC1SEL_DS0;
			uSrc1 = 0;
			uSrc2 = psInst->psArgs[1].sLocation.uOffset;
		}
	}

	/*
		Actually encode the instruction.
	*/
	*(IMG_PUINT32)pui8Dest = PDSEncodeABS(uCC, uDestSel, uDest, uSrcSel, uSrc1Sel, uSrc1, uSrc2);

	return IMG_TRUE;
}

static IMG_BOOL EncodeTwoArgumentArithmeticInstruction(IMG_PUINT8 pui8Dest, PINST psInst)
/*****************************************************************************
 FUNCTION	: EncodeTwoArgumentArithmeticInstruction
    
 PURPOSE	: Encode one of two argument arithmetic instructions (ADD, SUB, ADC, SBC).

 PARAMETERS	: pui8Dest			- Destination for the encoded instruction.
			  psInst			- Instruction to encode.
			  
 RETURNS	: Success or failure.
*****************************************************************************/
{
	IMG_UINT32	uCC;
	IMG_UINT32	uDestSel;
	IMG_UINT32	uDest;
	IMG_UINT32	uSrc1Sel;
	IMG_UINT32	uSrc1;
	IMG_UINT32	uSrc2Sel;
	IMG_UINT32	uSrc2;

	uCC = EncodePredicate(psInst, psInst->ePredicate);
	if (uCC == PDSASM_UNDEF)
	{
		return IMG_FALSE;
	}

	/*
		Get the encoded destination.
	*/
	assert(psInst->psArgs[0].eType == PDSASM_REGISTER_TYPE_DATASTORE);
	assert(psInst->psArgs[0].sLocation.uOffset <= EURASIA_PDS_ARITH_DEST_DS_LIMIT);
	if (psInst->psArgs[0].sLocation.uBank == 0)
	{
		uDestSel = EURASIA_PDS_ARITH_DESTSEL_DS0;
	}
	else
	{
		uDestSel = EURASIA_PDS_ARITH_DESTSEL_DS1;
	}
	uDest = psInst->psArgs[0].sLocation.uOffset;

	/*
		Get the encoded src1.
	*/
	if (psInst->psArgs[1].eType == PDSASM_REGISTER_TYPE_SPECIAL)
	{
		uSrc1Sel = EURASIA_PDS_ARITH_SRC1SEL_REG;

		switch (psInst->psArgs[1].u.eSpecialRegister)
		{
			case PDSASM_SPECIAL_REGISTER_IR0: uSrc1 = EURASIA_PDS_ARITH_SRC1_IR0; break;
			case PDSASM_SPECIAL_REGISTER_IR1: uSrc1 = EURASIA_PDS_ARITH_SRC1_IR1; break;
			case PDSASM_SPECIAL_REGISTER_TIMER: uSrc1 = EURASIA_PDS_ARITH_SRC1_TIM; break;
			default:
			{
				Error(psInst, "Invalid special register source for %s.", g_psInstDesc[psInst->eOpcode].pszName);
				return IMG_FALSE;
			}
		}
	}
	else
	{
		assert(psInst->psArgs[1].eType == PDSASM_REGISTER_TYPE_DATASTORE);
		assert(psInst->psArgs[1].sLocation.uBank == 0);
		assert(psInst->psArgs[1].sLocation.uOffset <= EURASIA_PDS_ARITH_SRC1_DS0_LIMIT);

		uSrc1Sel = EURASIA_PDS_ARITH_SRC1SEL_DS0;
		uSrc1 = psInst->psArgs[1].sLocation.uOffset;
	}

	/*
		Get the encoded src2.
	*/
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
	if (psInst->psArgs[2].eType == PDSASM_REGISTER_TYPE_SPECIAL)
	{
		uSrc2Sel = EURASIA_PDS_ARITH_SRC2SEL_REG;

		switch (psInst->psArgs[2].u.eSpecialRegister)
		{
			case PDSASM_SPECIAL_REGISTER_IR0: uSrc2 = EURASIA_PDS_ARITH_SRC2_IR0; break;
			case PDSASM_SPECIAL_REGISTER_IR1: uSrc2 = EURASIA_PDS_ARITH_SRC2_IR1; break;
			case PDSASM_SPECIAL_REGISTER_TIMER: uSrc2 = EURASIA_PDS_ARITH_SRC2_TIM; break;
			default:
			{
				Error(psInst, "Invalid special register source for %s.", g_psInstDesc[psInst->eOpcode].pszName);
				return IMG_FALSE;
			}
		}
	}
	else
#endif
	{
		assert(psInst->psArgs[2].eType == PDSASM_REGISTER_TYPE_DATASTORE);
		assert(psInst->psArgs[2].sLocation.uBank == 1);
		assert(psInst->psArgs[2].sLocation.uOffset <= EURASIA_PDS_ARITH_SRC2_DS1_LIMIT);
		
		uSrc2Sel = EURASIA_PDS_ARITH_SRC2SEL_DS1;
		uSrc2 = psInst->psArgs[2].sLocation.uOffset;
	}

	/*
		Actually encode the instruction.
	*/
	switch (psInst->eOpcode)
	{
		case PDSASM_OPCODE_ADD:
		{
			*(IMG_PUINT32)pui8Dest = PDSEncodeADD(uCC, uDestSel, uDest, uSrc1Sel, uSrc1, uSrc2Sel, uSrc2);
			break;
		}
		case PDSASM_OPCODE_SUB:
		{
			*(IMG_PUINT32)pui8Dest = PDSEncodeSUB(uCC, uDestSel, uDest, uSrc1Sel, uSrc1, uSrc2Sel, uSrc2);
			break;
		}
		case PDSASM_OPCODE_ADC:
		{
			*(IMG_PUINT32)pui8Dest = PDSEncodeADC(uCC, uDestSel, uDest, uSrc1Sel, uSrc1, uSrc2Sel, uSrc2);
			break;
		}
		case PDSASM_OPCODE_SBC:
		{
			*(IMG_PUINT32)pui8Dest = PDSEncodeSBC(uCC, uDestSel, uDest, uSrc1Sel, uSrc1, uSrc2Sel, uSrc2);
			break;
		}
		default:
		{
			IMG_ABORT();
		}
	}

	return IMG_TRUE;
}

static IMG_BOOL EncodeTwoArgumentLogicalInstruction(IMG_PUINT8 pui8Dest, PINST psInst)
/*****************************************************************************
 FUNCTION	: EncodeTwoArgumentLogicalInstruction
    
 PURPOSE	: Encode one of the two argument logical instructions (XOR, OR, AND, etc).

 PARAMETERS	: pui8Dest			- Destination for the encoded instruction.
			  psInst			- Instruction to encode.
			  
 RETURNS	: Success or failure.
*****************************************************************************/
{
	IMG_UINT32	uCC;
	IMG_UINT32	uDestSel;
	IMG_UINT32	uDest;
	IMG_UINT32	uSrc1Sel;
	IMG_UINT32	uSrc1;
	IMG_UINT32	uSrc2Sel;
	IMG_UINT32	uSrc2;

	uCC = EncodePredicate(psInst, psInst->ePredicate);
	if (uCC == PDSASM_UNDEF)
	{
		return IMG_FALSE;
	}

	/*
		Get the encoded destination.
	*/
	assert(psInst->psArgs[0].eType == PDSASM_REGISTER_TYPE_DATASTORE);
	assert(psInst->psArgs[0].sLocation.uOffset <= EURASIA_PDS_LOGIC_DEST_DS_LIMIT);
	if (psInst->psArgs[0].sLocation.uBank == 0)
	{
		uDestSel = EURASIA_PDS_LOGIC_DESTSEL_DS0;
	}
	else
	{
		uDestSel = EURASIA_PDS_LOGIC_DESTSEL_DS1;
	}
	uDest = psInst->psArgs[0].sLocation.uOffset;

	/*
		Get the encoded source 1.
	*/
	if (psInst->psArgs[1].eType == PDSASM_REGISTER_TYPE_SPECIAL)
	{
		uSrc1Sel = EURASIA_PDS_LOGIC_SRC1SEL_REG;

		switch (psInst->psArgs[1].u.eSpecialRegister)
		{
			case PDSASM_SPECIAL_REGISTER_IR0: uSrc1 = EURASIA_PDS_LOGIC_SRC1_IR0; break;
			case PDSASM_SPECIAL_REGISTER_IR1: uSrc1 = EURASIA_PDS_LOGIC_SRC1_IR1; break;
			case PDSASM_SPECIAL_REGISTER_TIMER: uSrc1 = EURASIA_PDS_LOGIC_SRC1_TIM; break;
			default:
			{
				Error(psInst, "Invalid special register source for %s.", g_psInstDesc[psInst->eOpcode].pszName);
				return IMG_FALSE;
			}
		}
	}
	else
	{
		assert(psInst->psArgs[1].eType == PDSASM_REGISTER_TYPE_DATASTORE);
		assert(psInst->psArgs[1].sLocation.uBank == 0);
		assert(psInst->psArgs[1].sLocation.uOffset <= EURASIA_PDS_LOGIC_SRC1_DS0_LIMIT);

		uSrc1Sel = EURASIA_PDS_ARITH_SRC1SEL_DS0;
		uSrc1 = psInst->psArgs[1].sLocation.uOffset;
	}

	/*
		Get the encoded source 2.
	*/
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
	if (psInst->psArgs[2].eType == PDSASM_REGISTER_TYPE_SPECIAL)
	{
		uSrc2Sel = EURASIA_PDS_LOGIC_SRC2SEL_REG;

		switch (psInst->psArgs[2].u.eSpecialRegister)
		{
			case PDSASM_SPECIAL_REGISTER_IR0: uSrc2 = EURASIA_PDS_LOGIC_SRC2_IR0; break;
			case PDSASM_SPECIAL_REGISTER_IR1: uSrc2 = EURASIA_PDS_LOGIC_SRC2_IR1; break;
			case PDSASM_SPECIAL_REGISTER_TIMER: uSrc2 = EURASIA_PDS_LOGIC_SRC2_TIM; break;
			default:
			{
				Error(psInst, "Invalid special register source for %s.", g_psInstDesc[psInst->eOpcode].pszName);
				return IMG_FALSE;
			}
		}
	}
	else
#endif
	{
		assert(psInst->psArgs[2].eType == PDSASM_REGISTER_TYPE_DATASTORE);
		assert(psInst->psArgs[2].sLocation.uBank == 1);
		assert(psInst->psArgs[2].sLocation.uOffset <= EURASIA_PDS_LOGIC_SRC2_DS1_LIMIT);

		uSrc2Sel = EURASIA_PDS_ARITH_SRC2SEL_DS1;
		uSrc2 = psInst->psArgs[2].sLocation.uOffset;
	}

	/*
		Actually encode the instruction.
	*/
	switch (psInst->eOpcode)
	{
		case PDSASM_OPCODE_AND:
		{
			*(IMG_PUINT32)pui8Dest = PDSEncodeAND(uCC, uDestSel, uDest, uSrc1Sel, uSrc1, uSrc2Sel, uSrc2);
			break;
		}
		case PDSASM_OPCODE_OR:
		{
			*(IMG_PUINT32)pui8Dest = PDSEncodeOR(uCC, uDestSel, uDest, uSrc1Sel, uSrc1, uSrc2Sel, uSrc2);
			break;
		}
		case PDSASM_OPCODE_XOR:
		{
			*(IMG_PUINT32)pui8Dest = PDSEncodeXOR(uCC, uDestSel, uDest, uSrc1Sel, uSrc1, uSrc2Sel, uSrc2);
			break;
		}
		case PDSASM_OPCODE_NOR:
		{
			*(IMG_PUINT32)pui8Dest = PDSEncodeNOR(uCC, uDestSel, uDest, uSrc1Sel, uSrc1, uSrc2Sel, uSrc2);
			break;
		}
		case PDSASM_OPCODE_NAND:
		{
			*(IMG_PUINT32)pui8Dest = PDSEncodeNAND(uCC, uDestSel, uDest, uSrc1Sel, uSrc1, uSrc2Sel, uSrc2);
			break;
		}
		default:
		{
			IMG_ABORT();
		}
	}

	return IMG_TRUE;
}

static IMG_BOOL EncodeMULInstruction(IMG_PUINT8 pui8Dest, PINST psInst)
/*****************************************************************************
 FUNCTION	: EncodeMULInstruction
    
 PURPOSE	: Encode a MUL instruction.

 PARAMETERS	: pui8Dest			- Destination for the encoded instruction.
			  psInst			- Instruction to encode.
			  
 RETURNS	: Success or failure.
*****************************************************************************/
{
	IMG_UINT32	uCC;
	IMG_UINT32	uDestSel;
	IMG_UINT32	uDest;
	IMG_UINT32	uSrc1Sel;
	IMG_UINT32	uSrc1;
	IMG_UINT32	uSrc2Sel;
	IMG_UINT32	uSrc2;

	uCC = EncodePredicate(psInst, psInst->ePredicate);
	if (uCC == PDSASM_UNDEF)
	{
		return IMG_FALSE;
	}

	/*
		Get the encoded destination.
	*/
	assert(psInst->psArgs[0].eType == PDSASM_REGISTER_TYPE_DATASTORE);
	if (psInst->psArgs[0].sLocation.uBank == 0)
	{
		uDestSel = EURASIA_PDS_MUL_DESTSEL_DS0;
	}
	else
	{
		uDestSel = EURASIA_PDS_MUL_DESTSEL_DS1;
	}
	uDest = psInst->psArgs[0].sLocation.uOffset;
	uDest += EncodeFlagsOffset(&psInst->psArgs[0]);
	assert(uDest <= EURASIA_PDS_MUL_DEST_DS_LIMIT);

	/*
		Get the encoded source 1.
	*/
	if (psInst->psArgs[1].eType == PDSASM_REGISTER_TYPE_SPECIAL)
	{
		uSrc1Sel = EURASIA_PDS_MUL_SRC1SEL_REG;

		switch (psInst->psArgs[1].u.eSpecialRegister)
		{
			case PDSASM_SPECIAL_REGISTER_IR0: uSrc1 = EURASIA_PDS_MUL_SRC1_IR0L; break;
			case PDSASM_SPECIAL_REGISTER_IR1: uSrc1 = EURASIA_PDS_MUL_SRC1_IR1L; break;
			case PDSASM_SPECIAL_REGISTER_TIMER: uSrc1 = EURASIA_PDS_MUL_SRC1_TIML; break;
			default:
			{
				Error(psInst, "Invalid special register source for %s.", g_psInstDesc[psInst->eOpcode].pszName);
				return IMG_FALSE;
			}
		}
		uSrc1 += EncodeFlagsOffset(&psInst->psArgs[1]);
	}
	else
	{
		assert(psInst->psArgs[1].eType == PDSASM_REGISTER_TYPE_DATASTORE);
		assert(psInst->psArgs[1].sLocation.uBank == 0);

		uSrc1Sel = EURASIA_PDS_MUL_SRC1SEL_DS0;
		uSrc1 = psInst->psArgs[1].sLocation.uOffset << 1;
		uSrc1 += EncodeFlagsOffset(&psInst->psArgs[1]);
		assert(uSrc1 <= EURASIA_PDS_MUL_SRC1_DS0_LIMIT);
	}

	/*
		Get the encoded source 2.
	*/
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
	if (psInst->psArgs[2].eType == PDSASM_REGISTER_TYPE_SPECIAL)
	{
		uSrc2Sel = EURASIA_PDS_MUL_SRC2SEL_REG;

		switch (psInst->psArgs[2].u.eSpecialRegister)
		{
			case PDSASM_SPECIAL_REGISTER_IR0: uSrc2 = EURASIA_PDS_MUL_SRC2_IR0L; break;
			case PDSASM_SPECIAL_REGISTER_IR1: uSrc2 = EURASIA_PDS_MUL_SRC2_IR1L; break;
			case PDSASM_SPECIAL_REGISTER_TIMER: uSrc2 = EURASIA_PDS_MUL_SRC2_TIML; break;
			default:
			{
				Error(psInst, "Invalid special register source for %s.", g_psInstDesc[psInst->eOpcode].pszName);
				return IMG_FALSE;
			}
		}
		uSrc2 += EncodeFlagsOffset(&psInst->psArgs[2]);
	}
	else
#endif
	{
		assert(psInst->psArgs[2].eType == PDSASM_REGISTER_TYPE_DATASTORE);
		assert(psInst->psArgs[2].sLocation.uBank == 1);
	
		uSrc2Sel = EURASIA_PDS_MUL_SRC2SEL_DS1;
		uSrc2 = psInst->psArgs[2].sLocation.uOffset << 1;
		uSrc2 += EncodeFlagsOffset(&psInst->psArgs[2]);
		assert(uSrc2 <= EURASIA_PDS_MUL_SRC2_DS1_LIMIT);
	}

	/*
		Actually encode the instruction.
	*/
	assert(psInst->eOpcode == PDSASM_OPCODE_MUL);
	*(IMG_PUINT32)pui8Dest = PDSEncodeMUL(uCC, uDestSel, uDest, uSrc1Sel, uSrc1, uSrc2Sel, uSrc2);

	return IMG_TRUE;
}

static IMG_BOOL EncodeNOTInstruction(IMG_PUINT8 pui8Dest, PINST psInst)
/*****************************************************************************
 FUNCTION	: EncodeNOTInstruction
    
 PURPOSE	: Encode a NOT instruction.

 PARAMETERS	: pui8Dest			- Destination for the encoded instruction.
			  psInst			- Instruction to encode.
			  
 RETURNS	: Success or failure.
*****************************************************************************/
{
	IMG_UINT32	uCC;
	IMG_UINT32	uDestSel;
	IMG_UINT32	uDest;
	IMG_UINT32	uSrc1Sel;
	IMG_UINT32	uSrc1;
	IMG_UINT32	uSrc2;
	IMG_UINT32	uSrcSel;

	uCC = EncodePredicate(psInst, psInst->ePredicate);
	if (uCC == PDSASM_UNDEF)
	{
		return IMG_FALSE;
	}

	/*
		Get the encoded destination.
	*/
	assert(psInst->psArgs[0].eType == PDSASM_REGISTER_TYPE_DATASTORE);
	assert(psInst->psArgs[0].sLocation.uOffset <= EURASIA_PDS_LOGIC_DEST_DS_LIMIT);
	if (psInst->psArgs[0].sLocation.uBank == 0)
	{
		uDestSel = EURASIA_PDS_LOGIC_DESTSEL_DS0;
	}
	else
	{
		uDestSel = EURASIA_PDS_LOGIC_DESTSEL_DS1;
	}
	uDest = psInst->psArgs[0].sLocation.uOffset;

	/*
		Get the encoded source 1.
	*/
	if (psInst->psArgs[1].eType == PDSASM_REGISTER_TYPE_SPECIAL)
	{
		uSrc1Sel = EURASIA_PDS_ARITH_SRC1SEL_REG;
		uSrcSel = EURASIA_PDS_LOGIC_SRCSEL_SRC1;
		uSrc2 = 0;

		switch (psInst->psArgs[1].u.eSpecialRegister)
		{
			case PDSASM_SPECIAL_REGISTER_IR0: uSrc1 = EURASIA_PDS_LOGIC_SRC1_IR0; break;
			case PDSASM_SPECIAL_REGISTER_IR1: uSrc1 = EURASIA_PDS_LOGIC_SRC1_IR1; break;
			case PDSASM_SPECIAL_REGISTER_TIMER: uSrc1 = EURASIA_PDS_LOGIC_SRC1_TIM; break;
			default:
			{
				Error(psInst, "Invalid special register source for %s.", g_psInstDesc[psInst->eOpcode].pszName);
				return IMG_FALSE;
			}
		}
	}
	else
	{
		assert(psInst->psArgs[1].eType == PDSASM_REGISTER_TYPE_DATASTORE);
		if (psInst->psArgs[1].sLocation.uBank == 0)
		{
			assert(psInst->psArgs[1].sLocation.uOffset <= EURASIA_PDS_LOGIC_SRC1_DS0_LIMIT);

			uSrc1Sel = EURASIA_PDS_LOGIC_SRC1SEL_DS0;
			uSrc1 = psInst->psArgs[1].sLocation.uOffset;
			uSrc2 = 0;
			uSrcSel = EURASIA_PDS_LOGIC_SRCSEL_SRC1;
		}
		else
		{
			assert(psInst->psArgs[1].sLocation.uBank == 1);
			assert(psInst->psArgs[1].sLocation.uOffset <= EURASIA_PDS_LOGIC_SRC2_DS1_LIMIT);

			uSrc1Sel = EURASIA_PDS_LOGIC_SRC1SEL_DS0;
			uSrc1 = 0;
			uSrc2 = psInst->psArgs[1].sLocation.uOffset;
			uSrcSel = EURASIA_PDS_LOGIC_SRCSEL_SRC2;
		}
	}

	/*
		Actually encode the instruction.
	*/
	assert(psInst->eOpcode == PDSASM_OPCODE_NOT);
	*(IMG_PUINT32)pui8Dest = PDSEncodeNOT(uCC, uDestSel, uDest, uSrcSel, uSrc1Sel, uSrc1, uSrc2);

	return IMG_TRUE;
}

static IMG_BOOL EncodeBRACALLInstruction(IMG_PUINT8 pui8Dest, PINST psInst)
/*****************************************************************************
 FUNCTION	: EncodeBRACALLInstruction
    
 PURPOSE	: Encode a BRA or a CALL instruction.

 PARAMETERS	: pui8Dest			- Destination for the encoded instruction.
			  psInst			- Instruction to encode.
			  
 RETURNS	: Success or failure.
*****************************************************************************/
{
	IMG_UINT32	uCC;
	IMG_UINT32	uDest;
	IMG_UINT32	uIdentifier;

	uCC = EncodeFlowControlPredicate(psInst, psInst->ePredicate);
	if (uCC == PDSASM_UNDEF)
	{
		return IMG_FALSE;
	}

	assert(psInst->psArgs[0].eType == PDSASM_REGISTER_TYPE_INSTRUCTION_PARAMETER);
	uIdentifier = psInst->psArgs[0].u.sParameter.uIdentifier;
	assert(psInst->psArgs[0].u.sParameter.uOffset == 0);
	assert(g_psIdentifierList[uIdentifier].eClass == PDSASM_IDENTIFIER_CLASS_LABEL);

	uDest = g_psIdentifierList[uIdentifier].uLabelOffset << EURASIA_PDS_FLOW_DEST_ALIGNSHIFT;
	if (uDest > EURASIA_PDS_FLOW_DEST_MEM_LIMIT)
	{
		Error(psInst, "The branch destination is out of range.");
		return IMG_FALSE;
	}

	/*
		Actually encode the instruction.
	*/
	switch (psInst->eOpcode)
	{
		case PDSASM_OPCODE_BRA:
		{
			*(IMG_PUINT32)pui8Dest = PDSEncodeBRA(uCC, uDest);
			break;
		}
		case PDSASM_OPCODE_CALL:
		{
			*(IMG_PUINT32)pui8Dest = PDSEncodeCALL(uCC, uDest);
			break;
		}
		default:
			break;
	}
	return IMG_TRUE;
}

static IMG_BOOL EncodeHALTRTNInstruction(IMG_PUINT8 pui8Dest, PINST psInst)
/*****************************************************************************
 FUNCTION	: EncodeHALTRTNInstruction
    
 PURPOSE	: Encode a HALT or RTN instruction.

 PARAMETERS	: pui8Dest			- Destination for the encoded instruction.
			  psInst			- Instruction to encode.
			  
 RETURNS	: Success or failure.
*****************************************************************************/
{
	IMG_UINT32	uCC;

	uCC = EncodeFlowControlPredicate(psInst, psInst->ePredicate);
	if (uCC == PDSASM_UNDEF)
	{
		return IMG_FALSE;
	}

	switch (psInst->eOpcode)
	{
		case PDSASM_OPCODE_HALT:
		{
			*(IMG_PUINT32)pui8Dest = PDSEncodeHALT(uCC);
			break;
		}
		case PDSASM_OPCODE_RTN:
		{
			*(IMG_PUINT32)pui8Dest = PDSEncodeRTN(uCC);
			break;
		}
		default:
		{
			IMG_ABORT();
		}
	}
	return IMG_TRUE;
}

static IMG_BOOL EncodeALUMInstruction(IMG_PUINT8 pui8Dest, PINST psInst)
/*****************************************************************************
 FUNCTION	: EncodeALUMInstruction
    
 PURPOSE	: Encode an ALUM instruction.

 PARAMETERS	: pui8Dest			- Destination for the encoded instruction.
			  psInst			- Instruction to encode.
			  
 RETURNS	: Success or failure.
*****************************************************************************/
{
	IMG_UINT32	uCC;
	IMG_UINT32	uMode;

	assert(psInst->eOpcode == PDSASM_OPCODE_ALUM);

	uCC = EncodePredicate(psInst, psInst->ePredicate);
	if (uCC == PDSASM_UNDEF)
	{
		return IMG_FALSE;
	}

	assert(psInst->psArgs[0].eType == PDSASM_REGISTER_TYPE_IMMEDIATE);
	uMode = psInst->psArgs[0].u.uImmediate;
	if (!(uMode == EURASIA_PDS_FLOW_ALUM_MODE_UNSIGNED || uMode == EURASIA_PDS_FLOW_ALUM_MODE_SIGNED))
	{
		Error(psInst, "Invalid alum mode.");
		return IMG_FALSE;
	}

	*(IMG_PUINT32)pui8Dest = PDSEncodeALUM(uCC, uMode);
	return IMG_TRUE;
}

static IMG_BOOL EncodeFENCEInstruction(IMG_PUINT8 pui8Dest, PINST psInst)
/*****************************************************************************
 FUNCTION	: EncodeFENCEInstruction
    
 PURPOSE	: Encode an RFENCE or RWFENCE instruction.

 PARAMETERS	: pui8Dest			- Destination for the encoded instruction.
			  psInst			- Instruction to encode.
			  
 RETURNS	: Success or failure.
*****************************************************************************/
{
	#if defined(SGX_FEATURE_PDS_LOAD_STORE)
	IMG_UINT32	uMode;

	if (psInst->ePredicate != PDSASM_PREDICATE_ALWAYS)
	{
		Error(psInst, "A predicate isn't supported on the %s instruction.", g_psInstDesc[psInst->eOpcode].pszName);
		return IMG_FALSE;
	}

	switch (psInst->eOpcode)
	{
		case PDSASM_OPCODE_RFENCE: uMode = EURASIA_PDS_DATAFENCE_MODE_READ; break;
		case PDSASM_OPCODE_RWFENCE: uMode = EURASIA_PDS_DATAFENCE_MODE_READWRITE; break;
		default: IMG_ABORT();
	}

	*(IMG_PUINT32)pui8Dest = PDSEncodeFENCE(uMode);
	return IMG_TRUE;
	#else /* defined(SGX_FEATURE_PDS_LOAD_STORE) */
	Error(psInst, "The %s instruction isn't supported on this core.", g_psInstDesc[psInst->eOpcode].pszName);

	PVR_UNREFERENCED_PARAMETER(pui8Dest);

	return IMG_FALSE;
	#endif /* defined(SGX_FEATURE_PDS_LOAD_STORE) */
}

static IMG_BOOL EncodeLOADSTOREInstruction(IMG_PUINT8 pui8Dest, PINST psInst)
/*****************************************************************************
 FUNCTION	: EncodeLOADSTOREInstruction
    
 PURPOSE	: Encode an LOAD or STORE instruction.

 PARAMETERS	: pui8Dest			- Destination for the encoded instruction.
			  psInst			- Instruction to encode.
			  
 RETURNS	: Success or failure.
*****************************************************************************/
{
	#if defined(SGX_FEATURE_PDS_LOAD_STORE)
	IMG_UINT32	uMemAddr;
	IMG_UINT32	uDataStoreSel;
	IMG_UINT32	uDataStoreOffset;
	IMG_UINT32	uSize;

	/*
		A predicate isn't supported on LOAD or STORE instructions.
	*/
	if (psInst->ePredicate != PDSASM_PREDICATE_ALWAYS)
	{
		Error(psInst, "A predicate isn't supported on the %s instruction.", g_psInstDesc[psInst->eOpcode].pszName);
		return IMG_FALSE;
	}

	/*
		Get the count of dwords to load or store.
	*/
	uSize = psInst->uArgCount - 1;
	assert(uSize > 0);
	assert(uSize <= EURASIA_PDS_LOADSTORE_SIZE_MAX);

	if (psInst->psArgs[0].eType == PDSASM_REGISTER_TYPE_IMMEDIATE)
	{
		uMemAddr = psInst->psArgs[0].u.uImmediate;

		/*
			Check the memory address has the correct alignment.
		*/
		if ((uMemAddr & ((1 << EURASIA_PDS_LOADSTORE_MEMADDR_ALIGNSHIFT) - 1)) != 0)
		{
			Error(psInst, "The memory address argument to a %s instruction must be a multiple of %d bytes.",
				g_psInstDesc[psInst->eOpcode].pszName, 1 << EURASIA_PDS_LOADSTORE_MEMADDR_ALIGNSHIFT);
			return IMG_FALSE;
		}
		uMemAddr >>= EURASIA_PDS_LOADSTORE_MEMADDR_ALIGNSHIFT;

		/*
			Check the memory address is within the supported range.
		*/
		if (uMemAddr > EURASIA_PDS_LOADSTORE_MEMADDR_MAX)
		{
			Error(psInst, "The memory address argument to a %s instruction has a maximum value of 0x%X bytes.",
				g_psInstDesc[psInst->eOpcode].pszName, EURASIA_PDS_LOADSTORE_MEMADDR_MAX << EURASIA_PDS_LOADSTORE_MEMADDR_ALIGNSHIFT);
			return IMG_FALSE;
		}

		/*
			Check if the end of the range to load from/store to wraps around the 
			address space.
		*/
		if ((uMemAddr + uSize) > (EURASIA_PDS_LOADSTORE_MEMADDR_MAX + 1))
		{
			Error(psInst, "The %s instruction wraps around the address space.", 
				g_psInstDesc[psInst->eOpcode].pszName);
			return IMG_FALSE;
		}
	}
	else
	{
		/*
			Set the memory address to a default value. The correct address will be patched in later.
		*/
		uMemAddr = 0;
	}

	/*
		Get the location of the portion of the data store to load/store.
	*/
	uDataStoreOffset = psInst->psArgs[1].sLocation.uOffset;
	assert(uDataStoreOffset >= EURASIA_PDS_DATASTORE_TEMPSTART);
	uDataStoreOffset -= EURASIA_PDS_DATASTORE_TEMPSTART;
	assert(uDataStoreOffset < EURASIA_PDS_DATASTORE_TEMPCOUNT);

	uDataStoreSel = psInst->psArgs[1].sLocation.uBank;

	#if defined(FIX_HW_BRN_27652)
	if (psInst->eOpcode == PDSASM_OPCODE_STORE)
	{
		static const IMG_UINT32 auRemapSrc[EURASIA_PDS_DATASTORE_TEMPCOUNT] =
		{
			0x0C,			/* TEMP0 */
			PDSASM_UNDEF,	/* TEMP1 */
			PDSASM_UNDEF,	/* TEMP2 */
			0x0D,			/* TEMP3 */
			0x0E,			/* TEMP4 */
			PDSASM_UNDEF,	/* TEMP5 */
			PDSASM_UNDEF,	/* TEMP6 */
			0x0F,			/* TEMP7 */
			PDSASM_UNDEF,	/* TEMP8 */
			PDSASM_UNDEF,	/* TEMP9 */
			PDSASM_UNDEF,	/* TEMP10 */
			PDSASM_UNDEF,	/* TEMP11 */
			PDSASM_UNDEF,	/* TEMP12 */
			PDSASM_UNDEF,	/* TEMP13 */
			PDSASM_UNDEF,	/* TEMP14 */
			PDSASM_UNDEF	/* TEMP15 */
		};

		assert(auRemapSrc[uDataStoreOffset] != PDSASM_UNDEF);
		uDataStoreOffset = auRemapSrc[uDataStoreOffset];
	}
	#endif /* defined(FIX_HW_BRN_27652) */

	/*
		Check the other arguments are in consecutive locations in the data store.
	*/
	#if defined(DEBUG)
	{
		IMG_UINT32	uArg;

		for (uArg = 2; uArg < psInst->uArgCount; uArg++)
		{
			assert(psInst->psArgs[uArg].sLocation.uOffset == (psInst->psArgs[1].sLocation.uOffset + uArg - 1));
			assert(psInst->psArgs[uArg].sLocation.uBank == psInst->psArgs[1].sLocation.uBank);
		}
	}
	#endif /* defined(DEBUG) */

	/*
		The size field uses the convention 0=MAX.
	*/
	if (uSize == EURASIA_PDS_LOADSTORE_SIZE_MAX)
	{
		uSize = 0;
	}

	if (psInst->eOpcode == PDSASM_OPCODE_LOAD)
	{
		*(IMG_PUINT32)pui8Dest = PDSEncodeLOAD(uSize, uDataStoreOffset, uDataStoreSel, uMemAddr);
	}
	else
	{
		*(IMG_PUINT32)pui8Dest = PDSEncodeSTORE(uSize, uDataStoreOffset, uDataStoreSel, uMemAddr);
	}

	return IMG_TRUE;
	#else /* defined(SGX_FEATURE_PDS_LOAD_STORE) */
	Error(psInst, "The %s instruction isn't supported on this core.", g_psInstDesc[psInst->eOpcode].pszName);

	PVR_UNREFERENCED_PARAMETER(pui8Dest);

	return IMG_FALSE;
	#endif /* defined(SGX_FEATURE_PDS_LOAD_STORE) */
}

static IMG_BOOL EncodeNOPInstruction(IMG_PUINT8 pui8Dest, PINST psInst)
/*****************************************************************************
 FUNCTION	: EncodeNOPInstruction
    
 PURPOSE	: Encode a NOP instruction.

 PARAMETERS	: pui8Dest			- Destination for the encoded instruction.
			  psInst			- Instruction to encode.
			  
 RETURNS	: Success or failure.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psInst);
	assert(psInst->eOpcode == PDSASM_OPCODE_NOP);
	
	*(IMG_PUINT32)pui8Dest = PDSEncodeNOP();
	return IMG_TRUE;
}

static IMG_BOOL EncodeTestInstruction(IMG_PUINT8 pui8Dest, PINST psInst)
/*****************************************************************************
 FUNCTION	: EncodeTestInstruction
    
 PURPOSE	: Encode a TSTZ, TSTNZ, TSTN or TSTP instruction.

 PARAMETERS	: pui8Dest			- Destination for the encoded instruction.
			  psInst			- Instruction to encode.
			  
 RETURNS	: Success or failure.
*****************************************************************************/
{
	IMG_UINT32	uCC;
	IMG_UINT32	uDest;
	IMG_UINT32	uSrc1Sel;
	IMG_UINT32	uSrc1;
	IMG_UINT32	uSrc2;
	IMG_UINT32	uSrcSel;

	uCC = EncodePredicate(psInst, psInst->ePredicate);
	if (uCC == PDSASM_UNDEF)
	{
		return IMG_FALSE;
	}

	/*
		Get the encoded destination.
	*/
	assert(psInst->psArgs[0].eType == PDSASM_REGISTER_TYPE_PREDICATE);
	assert(psInst->psArgs[0].u.uPredicate <= EURASIA_PDS_TST_DEST_P2);
	uDest = psInst->psArgs[0].u.uPredicate;

	/*
		Get the encoded source 1.
	*/
	if (psInst->psArgs[1].eType == PDSASM_REGISTER_TYPE_SPECIAL)
	{
		uSrc1Sel = EURASIA_PDS_TST_SRC1SEL_REG;
		uSrcSel = EURASIA_PDS_TST_SRCSEL_SRC1;
		uSrc2 = 0;

		switch (psInst->psArgs[1].u.eSpecialRegister)
		{
			case PDSASM_SPECIAL_REGISTER_IR0: uSrc1 = EURASIA_PDS_TST_SRC1_IR0; break;
			case PDSASM_SPECIAL_REGISTER_IR1: uSrc1 = EURASIA_PDS_TST_SRC1_IR1; break;
			case PDSASM_SPECIAL_REGISTER_TIMER: uSrc1 = EURASIA_PDS_TST_SRC1_TIM; break;
			default:
			{
				Error(psInst, "Invalid special register source for %s.", g_psInstDesc[psInst->eOpcode].pszName);
				return IMG_FALSE;
			}
		}
	}
	else
	{
		assert(psInst->psArgs[1].eType == PDSASM_REGISTER_TYPE_DATASTORE);
		if (psInst->psArgs[1].sLocation.uBank == 0)
		{
			assert(psInst->psArgs[1].sLocation.uOffset <= EURASIA_PDS_TST_SRC1_DS0_LIMIT);

			uSrc1Sel = EURASIA_PDS_TST_SRC1SEL_DS0;
			uSrc1 = psInst->psArgs[1].sLocation.uOffset;
			uSrc2 = 0;
			uSrcSel = EURASIA_PDS_TST_SRCSEL_SRC1;
		}
		else
		{
			assert(psInst->psArgs[1].sLocation.uBank == 1);
			assert(psInst->psArgs[1].sLocation.uOffset <= EURASIA_PDS_TST_SRC2_DS1_LIMIT);

			uSrc1Sel = EURASIA_PDS_TST_SRC1SEL_DS0;
			uSrc1 = 0;
			uSrc2 = psInst->psArgs[1].sLocation.uOffset;
			uSrcSel = EURASIA_PDS_TST_SRCSEL_SRC2;
		}
	}

	/*
		Actually encode the instruction.
	*/
	switch (psInst->eOpcode)
	{
		case PDSASM_OPCODE_TSTZ:
		{
			*(IMG_PUINT32)pui8Dest = PDSEncodeTSTZ(uCC, uDest, uSrcSel, uSrc1Sel, uSrc1, uSrc2);
			break;
		}
		case PDSASM_OPCODE_TSTN:
		{
			*(IMG_PUINT32)pui8Dest = PDSEncodeTSTN(uCC, uDest, uSrcSel, uSrc1Sel, uSrc1, uSrc2);
			break;
		}
		#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
		case PDSASM_OPCODE_TSTP:
		{
			*(IMG_PUINT32)pui8Dest = PDSEncodeTSTP(uCC, uDest, uSrcSel, uSrc1Sel, uSrc1, uSrc2);
			break;
		}
		case PDSASM_OPCODE_TSTNZ:
		{
			*(IMG_PUINT32)pui8Dest = PDSEncodeTSTNZ(uCC, uDest, uSrcSel, uSrc1Sel, uSrc1, uSrc2);
			break;
		}
		#else /* defined(SGX_FEATURE_PDS_EXTENDED_SOURCES) */
		case PDSASM_OPCODE_TSTP:
		case PDSASM_OPCODE_TSTNZ:
		{
			Error(psInst, "The %s instruction isn't supported on this core.", g_psInstDesc[psInst->eOpcode].pszName);
			return IMG_FALSE;
		}
		#endif /* defined(SGX_FEATURE_PDS_EXTENDED_SOURCES) */
		default:
		{
			IMG_ABORT();
		}
	}

	return IMG_TRUE;
}

static IMG_BOOL EncodeShiftInstruction(IMG_PUINT8 pui8Dest, PINST psInst)
/*****************************************************************************
 FUNCTION	: EncodeShiftInstruction
    
 PURPOSE	: Encode a SHL or SHR instruction.

 PARAMETERS	: pui8Dest			- Destination for the encoded instruction.
			  psInst			- Instruction to encode.
			  
 RETURNS	: Success or failure.
*****************************************************************************/
{
	IMG_UINT32	uCC;
	IMG_UINT32	uDestSel;
	IMG_UINT32	uDest;
	IMG_UINT32	uSrcSel;
	IMG_UINT32	uSrc;
	IMG_INT32	iShift;

	uCC = EncodePredicate(psInst, psInst->ePredicate);
	if (uCC == PDSASM_UNDEF)
	{
		return IMG_FALSE;
	}

	/*
		Get the encoded destination.
	*/
	assert(psInst->psArgs[0].eType == PDSASM_REGISTER_TYPE_DATASTORE);
	assert(psInst->psArgs[0].sLocation.uOffset <= EURASIA_PDS_SHIFT_DEST_DS_LIMIT);
	if (psInst->psArgs[0].sLocation.uBank == 0)
	{
		uDestSel = EURASIA_PDS_SHIFT_DESTSEL_DS0;
	}
	else
	{
		uDestSel = EURASIA_PDS_SHIFT_DESTSEL_DS1;
	}
	uDest = psInst->psArgs[0].sLocation.uOffset;

	/*
		Get the encoded source 1.
	*/
	if (psInst->psArgs[1].eType == PDSASM_REGISTER_TYPE_SPECIAL)
	{
		uSrcSel = EURASIA_PDS_SHIFT_SRCSEL_REG;

		switch (psInst->psArgs[1].u.eSpecialRegister)
		{
			case PDSASM_SPECIAL_REGISTER_IR0: uSrc = EURASIA_PDS_SHIFT_SRC_IR0; break;
			case PDSASM_SPECIAL_REGISTER_IR1: uSrc = EURASIA_PDS_SHIFT_SRC_IR1; break;
			case PDSASM_SPECIAL_REGISTER_TIMER: uSrc = EURASIA_PDS_SHIFT_SRC_TIM; break;
			default:
			{
				Error(psInst, "Invalid special register source for %s.", g_psInstDesc[psInst->eOpcode].pszName);
				return IMG_FALSE;
			}
		}
	}
	else
	{
		assert(psInst->psArgs[1].eType == PDSASM_REGISTER_TYPE_DATASTORE);
		if (psInst->psArgs[1].sLocation.uBank == 0)
		{
			assert(psInst->psArgs[1].sLocation.uOffset <= EURASIA_PDS_SHIFT_SRC_DS_LIMIT);
	
			uSrcSel = EURASIA_PDS_SHIFT_SRCSEL_DS0;
			uSrc = psInst->psArgs[1].sLocation.uOffset;
		}
		else
		{
			assert(psInst->psArgs[1].sLocation.uBank == 1);
			assert(psInst->psArgs[1].sLocation.uOffset <= EURASIA_PDS_SHIFT_SRC_DS_LIMIT);
	
			uSrcSel = EURASIA_PDS_SHIFT_SRCSEL_DS1;
			uSrc = psInst->psArgs[1].sLocation.uOffset;
		}
	}

	/*
		Get the shift value.
	*/
	assert(psInst->psArgs[2].eType == PDSASM_REGISTER_TYPE_IMMEDIATE);
	iShift = (IMG_INT32)psInst->psArgs[2].u.uImmediate;
	if (iShift > EURASIA_PDS_SHIFT_SHIFT_MAX)
	{
		Error(psInst, "The maximum shift supported is %d.", EURASIA_PDS_SHIFT_SHIFT_MAX);
		return IMG_FALSE;
	}
	if (iShift < -EURASIA_PDS_SHIFT_SHIFT_MIN)
	{
		Error(psInst, "The minimum shift supported is %d.", -EURASIA_PDS_SHIFT_SHIFT_MIN);
		return IMG_FALSE;
	}

	switch (psInst->eOpcode)
	{
		case PDSASM_OPCODE_SHR:
		{
			*(IMG_PUINT32)pui8Dest = PDSEncodeSHR(uCC, uDestSel, uDest, uSrcSel, uSrc, (IMG_UINT32)iShift);
			break;
		}
		case PDSASM_OPCODE_SHL:
		{
			*(IMG_PUINT32)pui8Dest = PDSEncodeSHL(uCC, uDestSel, uDest, uSrcSel, uSrc, (IMG_UINT32)iShift);
			break;
		}
		default:
		{
			IMG_ABORT();
		}
	}

	return IMG_TRUE;
}

static IMG_UINT32 GetDataSegmentOffset(IMG_UINT32	uBank,
									   IMG_UINT32	uOffset)
/*****************************************************************************
 FUNCTION	: GetDataSegmentOffset
    
 PURPOSE	: Convert a data store location to its offset in memory.

 PARAMETERS	: uBank			- Bank of the data store location.
			  uOffset		- Offset of the data store location in the bank.
			  
 RETURNS	: Corresponding offset in memory.
*****************************************************************************/
{
	IMG_UINT32	uRow;
	IMG_UINT32	uColumn;

	uRow		= uOffset / PDS_NUM_DWORDS_PER_ROW;
	uColumn		= uOffset % PDS_NUM_DWORDS_PER_ROW;

	return (2 * uRow + uBank) * PDS_NUM_DWORDS_PER_ROW + uColumn;
}

static IMG_VOID SetImmediateValues(IMG_PUINT8	pui8EncodedProgram)
/*****************************************************************************
 FUNCTION	: SetImmediateValues
    
 PURPOSE	: Copy all the immediate values into the data store of the
			  encoded program.

 PARAMETERS	: pui8EncodedProgram	- Points to the memory for the encoded program.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uIdentifier;

	for (uIdentifier = 0; uIdentifier < g_uIdentifierCount; uIdentifier++)
	{
		PDATASTORE_INFO	psDS = &g_psIdentifierList[uIdentifier].sDataStore;
		IMG_UINT32		uBank;
		IMG_UINT32		uImmediate;

		if (g_psIdentifierList[uIdentifier].eClass != PDSASM_IDENTIFIER_CLASS_IMMEDIATE &&
			g_psIdentifierList[uIdentifier].eClass != PDSASM_IDENTIFIER_CLASS_LABEL)
		{
			continue;
		}

		if (g_psIdentifierList[uIdentifier].eClass == PDSASM_IDENTIFIER_CLASS_IMMEDIATE)
		{
			uImmediate = g_psIdentifierList[uIdentifier].uImmediate;
		}
		else
		{
			uImmediate = g_psIdentifierList[uIdentifier].uLabelOffset;
		}

		for (uBank = 0; uBank < 2; uBank++)
		{
			IMG_UINT32	uLocIdx;
				
			for (uLocIdx = 0; uLocIdx < psDS->auOffsetCount[uBank]; uLocIdx++)
			{
				IMG_UINT32	uOffset = psDS->apuOffsets[uBank][uLocIdx];
				IMG_UINT32	uDataStoreOffset = GetDataSegmentOffset(uBank, uOffset);

				((IMG_PUINT32)pui8EncodedProgram)[uDataStoreOffset]	= uImmediate;
			}
		}
	}
}

static IMG_PUINT8 EncodeInstructions(IMG_PUINT32	puEncodedProgramLength,
									 IMG_PUINT32	puDataSegmentSize)
/*****************************************************************************
 FUNCTION	: EncodeInstructions
    
 PURPOSE	: Convert all instructions to machine code.

 PARAMETERS	: puEncodedProgramLength		- Returns the overall length of the program.
			  puDataSegmentSize				- Returns the size of the data segment.
			  
 RETURNS	: Returns a pointer to memory which contains the encoded program.
*****************************************************************************/
{
	IMG_UINT32	uInstCount;
	IMG_UINT32	uDataSegmentSize;
	IMG_PUINT8	pui8EncodedProgram, pui8Inst;
	PINST		psInst;

	/*
		Count the number of instructions.
	*/
	uInstCount = 0;
	for (psInst = g_psInstructionListHead; psInst != NULL; psInst = psInst->psNext)
	{
		uInstCount++;
	}

	/*
		Get the size of the data segment.
	*/
	{
		IMG_UINT32	auLastUsedInBank[2] = {0, 0};
		IMG_UINT32	uBank;
		IMG_BOOL	abBankUsed[2] = {IMG_FALSE, IMG_FALSE};

		/*
			Get the last used location in each bank.
		*/
		for (uBank = 0; uBank < 2; uBank++)
		{
			IMG_UINT32	uIdx;

			for (uIdx = 0; uIdx < EURASIA_PDS_DATASTORE_TEMPSTART; uIdx++)
			{
				if (g_aabDSUsed[uBank][uIdx])
				{
					auLastUsedInBank[uBank] = uIdx;
					abBankUsed[uBank] = IMG_TRUE;
				}
			}
		}

		/*	
			The data segment size is largest of the memory offsets corresponding to
			the last used locations from each bank.
		*/
		uDataSegmentSize = 0;
		for (uBank = 0; uBank < 2; uBank++)
		{
			if (abBankUsed[uBank])
			{	
				IMG_UINT32	uDataStoreOffset;

				uDataStoreOffset = GetDataSegmentOffset(uBank, auLastUsedInBank[uBank]) + 1;
				if (uDataStoreOffset > uDataSegmentSize)
				{
					uDataSegmentSize = uDataStoreOffset;
				}
			}
		}

		/*
			Round the size up for alignment.
		*/
		uDataSegmentSize *= sizeof(IMG_UINT32);
		uDataSegmentSize += (1UL << EURASIA_PDS_DATASIZE_ALIGNSHIFT) - 1;
		uDataSegmentSize &= ~((1UL << EURASIA_PDS_DATASIZE_ALIGNSHIFT) - 1);
	}

	/*
		Allocate memory for the encoded program.
	*/
	*puDataSegmentSize = uDataSegmentSize;
	*puEncodedProgramLength = EURASIA_PDS_INSTRUCTION_SIZE * uInstCount + uDataSegmentSize;
	pui8EncodedProgram = malloc(*puEncodedProgramLength);
	if (pui8EncodedProgram == NULL)
	{
		return NULL;
	}

	memset(pui8EncodedProgram, 0, *puEncodedProgramLength);

	/*
		Copy immediate values into the data segment.
	*/
	SetImmediateValues(pui8EncodedProgram);
	
	/*
		Encode each instruction.
	*/
	pui8Inst = pui8EncodedProgram + uDataSegmentSize;
	for (psInst = g_psInstructionListHead; psInst != NULL; psInst = psInst->psNext)
	{
		switch (psInst->eOpcode)
		{
			case PDSASM_OPCODE_MOVS:
			case PDSASM_OPCODE_MOVSA:
			{
				if (!EncodeMOVSInstruction(pui8Inst, psInst))
				{
					free(pui8EncodedProgram);
					return NULL;
				}
				break;
			}
			case PDSASM_OPCODE_MOV32:
			{
				if (!EncodeMOV32Instruction(pui8Inst, psInst))
				{
					free(pui8EncodedProgram);
					return NULL;
				}
				break;
			}
			case PDSASM_OPCODE_MOV16:
			{
				if (!EncodeMOV16Instruction(pui8Inst, psInst))
				{
					free(pui8EncodedProgram);
					return NULL;
				}
				break;
			}
			case PDSASM_OPCODE_ABS:
			{
				if (!EncodeABSInstruction(pui8Inst, psInst))
				{
					free(pui8EncodedProgram);
					return NULL;
				}
				break;
			}
			case PDSASM_OPCODE_ADD:
			case PDSASM_OPCODE_SUB:
			case PDSASM_OPCODE_ADC:
			case PDSASM_OPCODE_SBC:
			{
				if (!EncodeTwoArgumentArithmeticInstruction(pui8Inst, psInst))
				{
					free(pui8EncodedProgram);
					return NULL;
				}
				break;
			}
			case PDSASM_OPCODE_MUL:
			{
				if (!EncodeMULInstruction(pui8Inst, psInst))
				{
					free(pui8EncodedProgram);
					return NULL;
				}
				break;
			}
			case PDSASM_OPCODE_AND:
			case PDSASM_OPCODE_OR:
			case PDSASM_OPCODE_XOR:
			case PDSASM_OPCODE_NOR:
			case PDSASM_OPCODE_NAND:
			{
				if (!EncodeTwoArgumentLogicalInstruction(pui8Inst, psInst))
				{
					free(pui8EncodedProgram);
					return NULL;
				}
				break;
			}
			case PDSASM_OPCODE_NOT:
			{
				if (!EncodeNOTInstruction(pui8Inst, psInst))
				{
					free(pui8EncodedProgram);
					return NULL;
				}
				break;
			}
			case PDSASM_OPCODE_BRA:
			case PDSASM_OPCODE_CALL:
			{
				if (!EncodeBRACALLInstruction(pui8Inst, psInst))
				{
					free(pui8EncodedProgram);
					return NULL;
				}
				break;
			}
			case PDSASM_OPCODE_NOP:
			{
				if (!EncodeNOPInstruction(pui8Inst, psInst))
				{
					free(pui8EncodedProgram);
					return NULL;
				}
				break;
			}
			case PDSASM_OPCODE_HALT:
			case PDSASM_OPCODE_RTN:
			{
				if (!EncodeHALTRTNInstruction(pui8Inst, psInst))
				{
					free(pui8EncodedProgram);
					return NULL;
				}
				break;
			}
			case PDSASM_OPCODE_SHR:
			case PDSASM_OPCODE_SHL:
			{
				if (!EncodeShiftInstruction(pui8Inst, psInst))
				{
					free(pui8EncodedProgram);
					return NULL;
				}
				break;
			}
			case PDSASM_OPCODE_TSTZ:
			case PDSASM_OPCODE_TSTN:
			case PDSASM_OPCODE_TSTNZ:
			case PDSASM_OPCODE_TSTP:
			{
				if (!EncodeTestInstruction(pui8Inst, psInst))
				{
					free(pui8EncodedProgram);
					return NULL;
				}
				break;
			}
			case PDSASM_OPCODE_ALUM:
			{
				if (!EncodeALUMInstruction(pui8Inst, psInst))
				{
					free(pui8EncodedProgram);
					return NULL;
				}
				break;
			}
			case PDSASM_OPCODE_RFENCE:
			case PDSASM_OPCODE_RWFENCE:
			{
				if (!EncodeFENCEInstruction(pui8Inst, psInst))
				{
					free(pui8EncodedProgram);
					return NULL;
				}
				break;
			}
			case PDSASM_OPCODE_LOAD:
			case PDSASM_OPCODE_STORE:
			{
				if (!EncodeLOADSTOREInstruction(pui8Inst, psInst))
				{
					free(pui8EncodedProgram);
					return NULL;
				}
				break;
			}
			default: IMG_ABORT();
		}

		pui8Inst += EURASIA_PDS_INSTRUCTION_SIZE;
	}

	return pui8EncodedProgram;
}

static IMG_VOID ShowMap(IMG_VOID)
/*****************************************************************************
 FUNCTION	: ShowMap
    
 PURPOSE	: Print a map of the locations in the datastore assigned to each 
			  identifier.

 PARAMETERS	: None.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uIdentifier;

	for (uIdentifier = 0; uIdentifier < g_uIdentifierCount; uIdentifier++)
	{
		PDATASTORE_INFO	psDS = &g_psIdentifierList[uIdentifier].sDataStore;
		IMG_UINT32		uBank;
		IMG_UINT32		uStart;

		switch (g_psIdentifierList[uIdentifier].eClass)
		{
			case PDSASM_IDENTIFIER_CLASS_TEMP:
			case PDSASM_IDENTIFIER_CLASS_DATA:
			{
				printf("%s:", g_psIdentifierList[uIdentifier].pszName);
				break;
			}
			case PDSASM_IDENTIFIER_CLASS_IMMEDIATE:
			{
				printf("0x%.8X:", g_psIdentifierList[uIdentifier].uImmediate);
				break;
			}
			case PDSASM_IDENTIFIER_CLASS_LABEL:
			{
				printf("%s(0x%.8X):", 
					   g_psIdentifierList[uIdentifier].pszName, 
					   g_psIdentifierList[uIdentifier].uLabelOffset);
				break;
			}
			default:
				break;
		}

		if (g_psIdentifierList[uIdentifier].eClass == PDSASM_IDENTIFIER_CLASS_TEMP)
		{
			uStart = EURASIA_PDS_DATASTORE_TEMPSTART;
		}
		else
		{
			uStart = 0;
		}

		for (uBank = 0; uBank < 2; uBank++)
		{
			IMG_UINT32	uLocIdx;
				
			for (uLocIdx = 0; uLocIdx < psDS->auOffsetCount[uBank]; uLocIdx++)
			{
				printf(" ds%u[%u]", uBank, uStart + psDS->apuOffsets[uBank][uLocIdx]);
			}
		}
		printf("\n");
	}
}

static IMG_BOOL PrintDSSetDefines(FILE*				fOutFile,
								  IMG_PCHAR			pszOutDefineName,
								  PDATASTORE_INFO	psDS,
								  IMG_PCHAR			pszIdentifierName)
/*****************************************************************************
 FUNCTION	: PrintDSSetDefines
    
 PURPOSE	: For an identifier in the PDS program print a C define for the
			  set of locations the identifier is assigned to in the data store

 PARAMETERS	: fOutFile				- File to append the routines to.
			  pszOutDefineName		- Prefix for the defines.
			  psDS					- Describes the set of locations the
									identifier is assigned to.
			  pszIdentifierName		- Name of the identifier.
			  
 RETURNS	: TRUE on success; FALSE on failure.
*****************************************************************************/
{
	IMG_PCHAR	pszTemp;
	IMG_BOOL	bFirst;
	IMG_UINT32	uBank;
	IMG_PCHAR	pszUCaseIdentifierName;

	/*
		Convert the identifier name to upper case.
	*/
	pszUCaseIdentifierName = strdup(pszIdentifierName);
	for (pszTemp = pszUCaseIdentifierName; *pszTemp != 0; pszTemp++)
	{
		*pszTemp = (IMG_CHAR)toupper((IMG_INT)*pszTemp);
	}
	
	/*
		Generate a single line of the form

			#define NAME {A, B, C}
	*/
	if (fprintf(fOutFile, "#define PDS_%s_%s_LOCATIONS\t{", pszOutDefineName, pszUCaseIdentifierName) < 0)
	{
		return IMG_FALSE;
	}
	bFirst = IMG_TRUE;
	for (uBank = 0; uBank < 2; uBank++)
	{
		IMG_UINT32	uLocIdx;
			
		for (uLocIdx = 0; uLocIdx < psDS->auOffsetCount[uBank]; uLocIdx++)
		{
			IMG_UINT32	uOffset = psDS->apuOffsets[uBank][uLocIdx];
			IMG_UINT32	uDataStoreOffset = GetDataSegmentOffset(uBank, uOffset);

			if (!bFirst)
			{
				if (fprintf(fOutFile, ", ") < 0)
				{
					return IMG_FALSE;
				}
			}
			else
			{
				bFirst = IMG_FALSE;
			}
			if (fprintf(fOutFile, "%u", uDataStoreOffset * sizeof(IMG_UINT32)) < 0)
			{
				return IMG_FALSE;
			}
		}
	}
	if (fprintf(fOutFile, "}\n\n") < 0)
	{
		return IMG_FALSE;
	}
	free(pszUCaseIdentifierName);

	return IMG_TRUE;
}

#if defined(SGX_FEATURE_PDS_LOAD_STORE)
static IMG_BOOL PrintSetInstructionFragment(FILE*		fOutFile,
											PINST		psInst,
											IMG_UINT32	uArg,
											IMG_UINT32	uInstOffset)
/*****************************************************************************
 FUNCTION	: PrintSetInstructionFragment
    
 PURPOSE	: For each identifier in the PDS program print C code for a routine  
			  to set the identifier to a particular value by modifying the data store.

 PARAMETERS	: fOutFile				- File to append the fragment to.
			  psInst				- Instruction to patch.
			  uArg					- Argument to patch.
			  uInstOffset			- Offset of the instruction (in dwords)
									from the start of the program.
			  
 RETURNS	: TRUE on success; FALSE on failure.
*****************************************************************************/
{
	switch (psInst->eOpcode)
	{
		case PDSASM_OPCODE_LOAD:
		case PDSASM_OPCODE_STORE:
		{
			PARG psArg = &psInst->psArgs[uArg];

			assert(uArg == 0);
			assert((psArg->u.sParameter.uOffset & ((1 << EURASIA_PDS_LOADSTORE_MEMADDR_ALIGNSHIFT) - 1)) == 0);
			
			/*
				Generate a statement to clear the memory address field in the instruction.
			*/
			if (fprintf(fOutFile, 
						"\tpui32Program[%u] &= 0x%.8X;\n", 
						uInstOffset, 
						EURASIA_PDS_LOADSTORE_MEMADDR_CLRMSK) < 0)
			{
				return IMG_FALSE;
			}
			/*
				Generate an assert to check the supplied address has the required alignment.
			*/
			if (fprintf(fOutFile, 
						"\tPVR_ASSERT((ui32Value & 0x%.8X) == 0);\n", 
						(1 << EURASIA_PDS_LOADSTORE_MEMADDR_ALIGNSHIFT) - 1) < 0)
			{
				return IMG_FALSE;
			}
			/*
				Generate an assert to check the supplied address is in the range supported
				by the instruction.
			*/
			if (fprintf(fOutFile, 
						"\tPVR_ASSERT((ui32Value + %d) <= 0x%.8X);\n", 
						psArg->u.sParameter.uOffset + (psInst->uArgCount - 2) * sizeof(IMG_UINT32),
						EURASIA_PDS_LOADSTORE_MEMADDR_MAX << EURASIA_PDS_LOADSTORE_MEMADDR_ALIGNSHIFT) < 0)
			{
				return IMG_FALSE;
			}
			/*
				Generate a statement to set the supplied address into the instruction.
			*/
			if (fprintf(fOutFile, 
						"\tpui32Program[%u] |= (((ui32Value + %d) >> %d) << %d);\n", 
						uInstOffset, 
						psArg->u.sParameter.uOffset,
						EURASIA_PDS_LOADSTORE_MEMADDR_ALIGNSHIFT,
						EURASIA_PDS_LOADSTORE_MEMADDR_SHIFT) < 0)
			{
				return IMG_FALSE;
			}

			return IMG_TRUE;
		}
		default: IMG_ABORT();
	}
}
#endif /* defined(SGX_FEATURE_PDS_LOAD_STORE) */

static IMG_BOOL PrintDSSetRoutines(FILE*		fOutFile, 
								   IMG_PCHAR	pszOutArrayName,
								   IMG_PCHAR	pszOutDefineName,
								   IMG_UINT32	uDataSegmentSize)
/*****************************************************************************
 FUNCTION	: PrintDSSetRoutines
    
 PURPOSE	: For each identifier in the PDS program print C code for a routine  
			  to set the identifier to a particular value by modifying the data store.

 PARAMETERS	: fOutFile				- File to append the routines to.
			  pszOutArrayName		- Prefix for the routine names.
			  pszOutDefineName		- Upper case version of the array name.
			  uDataSegmentSize		- Size of the program's data segment.
			  
 RETURNS	: TRUE on success; FALSE on failure.
*****************************************************************************/
{
	IMG_UINT32	uIdentifier;

	for (uIdentifier = 0; uIdentifier < g_uIdentifierCount; uIdentifier++)
	{
		PIDENTIFIER		psIdentifier = &g_psIdentifierList[uIdentifier];
		PDATASTORE_INFO	psDS = &psIdentifier->sDataStore;
		IMG_UINT32		uBank;
		IMG_PCHAR		pchRoutineName;
		#if defined(SGX_FEATURE_PDS_LOAD_STORE)
		PINST			psInst;
		IMG_UINT32		uInstOffset;
		#endif /* defined(SGX_FEATURE_PDS_LOAD_STORE) */

		/*
			Skip non-constant identifiers.
		*/
		if (g_psIdentifierList[uIdentifier].eClass != PDSASM_IDENTIFIER_CLASS_DATA)
		{
			continue;
		}

		/*
			Skip unused identifiers.
		*/
		if (psDS->auOffsetCount[0] == 0 && 
			psDS->auOffsetCount[1] == 0 &&
			psIdentifier->uInstParamCount == 0)
		{
			continue;
		}

		pchRoutineName = malloc(6 + strlen(pszOutArrayName) + strlen(g_psIdentifierList[uIdentifier].pszName) + 1);
		sprintf(pchRoutineName, "PDS%sSet%s", pszOutArrayName, g_psIdentifierList[uIdentifier].pszName);

		/*
			Create a function declaration.
		*/
		if (fprintf(fOutFile, "#ifdef INLINE_IS_PRAGMA\n") < 0)
		{
			return IMG_FALSE;
		}
		if (fprintf(fOutFile, "#pragma inline(%s)\n", pchRoutineName) < 0)
		{
			return IMG_FALSE;
		}
		if (fprintf(fOutFile, "#endif\n") < 0)
		{
			return IMG_FALSE;
		}
		if (fprintf(fOutFile, "FORCE_INLINE IMG_VOID %s (IMG_PUINT32 pui32Program, IMG_UINT32 ui32Value)\n", pchRoutineName) < 0)
		{
			return IMG_FALSE;
		}
		if (fprintf(fOutFile, "{\n") < 0)
		{
			return IMG_FALSE;
		}

		/*
			Create statements to set each location used by this identifier to
			the argument supplied to the function.
		*/
		for (uBank = 0; uBank < 2; uBank++)
		{
			IMG_UINT32	uLocIdx;
			
			for (uLocIdx = 0; uLocIdx < psDS->auOffsetCount[uBank]; uLocIdx++)
			{
				IMG_UINT32	uOffset = psDS->apuOffsets[uBank][uLocIdx];
				IMG_UINT32	uDataStoreOffset = GetDataSegmentOffset(uBank, uOffset);

				if (fprintf(fOutFile, "\tpui32Program[%u] = ui32Value;\n", uDataStoreOffset) < 0)
				{
					return IMG_FALSE;
				}
			}
		}

		/*
			If the value of this identifier is directly encoded in an instruction create
			statements to patch the instruction.
		*/
		#if defined(SGX_FEATURE_PDS_LOAD_STORE)
		if (psIdentifier->uInstParamCount > 0)
		{
			for (psInst = g_psInstructionListHead, uInstOffset = uDataSegmentSize / sizeof(IMG_UINT32); 
				 psInst != NULL; 
				 psInst = psInst->psNext, uInstOffset++)
			{
				IMG_UINT32	uArg;
	
				for (uArg = 0; uArg < psInst->uArgCount; uArg++)
				{
					if (psInst->psArgs[uArg].eType == PDSASM_REGISTER_TYPE_INSTRUCTION_PARAMETER &&
						psInst->psArgs[uArg].u.sParameter.uIdentifier == uIdentifier)
					{
						PrintSetInstructionFragment(fOutFile, psInst, uArg, uInstOffset);
					}
				}
			}
		}
		#else /* defined(SGX_FEATURE_PDS_LOAD_STORE) */
		PVR_UNREFERENCED_PARAMETER(uDataSegmentSize);

		assert(psIdentifier->uInstParamCount == 0);
		#endif /* defined(SGX_FEATURE_PDS_LOAD_STORE) */

		/*
			Finish the function.
		*/
		if (fprintf(fOutFile, "}\n") < 0)
		{
			return IMG_FALSE;
		}

		/*
			Create a define giving the offsets of the locations of this identifier within
			the datastore as an array initializer.
		*/
		if (psDS->auOffsetCount[0] != 0 ||
			psDS->auOffsetCount[1] != 0)
		{
			if (!PrintDSSetDefines(fOutFile, pszOutDefineName, psDS, g_psIdentifierList[uIdentifier].pszName))
			{
				return IMG_FALSE;
			}
		}

		free(pchRoutineName);
	}
	return IMG_TRUE;
}

static IMG_BOOL WriteSizeofHeader(IMG_PCHAR		pszCmdName,
								  IMG_PCHAR		pszOutFileName,
								  IMG_PCHAR		pszOutDefineName,
								  IMG_UINT32	uEncodedProgramLength)
/*****************************************************************************
 FUNCTION	: WriteSizeofHeader
    
 PURPOSE	: Create a C header file with a define for the size of the
			  PDS program.

 PARAMETERS	: pszCmdName			- Command name to use in error messages.
			  pszOutFileName		- Name of the header file to create.
			  pszOutDefineName		- Name of the define to create.
			  uEncodedProgramLength	- Value for the define.
			  
 RETURNS	: TRUE on success; FALSE on failure.
*****************************************************************************/
{
	FILE*	fOutFile;

	fOutFile = fopen(pszOutFileName, "wb");
	if (fOutFile == NULL)
	{
		fprintf(stderr, "%s: Couldn't open output file %s: %s.\n", pszCmdName, pszOutFileName, strerror(errno));
		return IMG_FALSE;
	}

	if (fprintf(fOutFile, "/* Auto-generated file - don't edit. */\n") < 0)
	{
		goto write_failed;
	}
	if (fprintf(fOutFile, "#define PDS_%s_SIZE\t(%uUL)\n", pszOutDefineName, uEncodedProgramLength) < 0)
	{
		goto write_failed;
	}

	fclose(fOutFile);
	return IMG_TRUE;

write_failed:
	fprintf(stderr, "%s: Couldn't write output file %s: %s.\n", pszCmdName, pszOutFileName, strerror(errno));
	fclose(fOutFile);
	unlink(pszOutFileName);
	return IMG_FALSE;
}

static IMG_BOOL WriteHeader(IMG_PCHAR	pszCmdName, 
							IMG_PCHAR	pszOutFileName, 
							IMG_PCHAR	pszOutArrayName,
							IMG_PCHAR	pszOutDefineName,
							IMG_UINT32	uEncodedProgramLength,
							IMG_UINT32	uDataSegmentSize,
							IMG_PUINT8	pui8EncodedProgram)
/*****************************************************************************
 FUNCTION	: WriteSizeofHeader
    
 PURPOSE	: Create a C header file with a declaration of an array containing
			  the encoded program.

 PARAMETERS	: pszCmdName			- Command name to use in error messages.
			  pszOutFileName		- Name of the header file to create.
			  pszOutArrayName		- Name to use for the array.
			  pszOutDefineName		- Prefix for C defines.
			  uEncodedProgramLength	- Length of the program.
			  uDataSegmentSize		- Length of the program data segment.
			  pui8EncodedProgram	- Points to the encoded program.
			  
 RETURNS	: TRUE on success; FALSE on failure.
*****************************************************************************/
{
	FILE*	fOutFile;

	/*
		Create the header file.
	*/
	fOutFile = fopen(pszOutFileName, "wb");
	if (fOutFile == NULL)
	{
		fprintf(stderr, "%s: Couldn't open output file %s: %s.\n", pszCmdName, pszOutFileName, strerror(errno));
		return IMG_FALSE;
	}

	if (fprintf(fOutFile, "/* Auto-generated file - don't edit. */\n") < 0)
	{
		goto write_failed;
	}
	if (fprintf(fOutFile, "\n") < 0)
	{
		goto write_failed;
	}
	/*
		Print a C declaration of an array containing the encoded program.
	*/
	if (fprintf(fOutFile, "#define PDS_%s_DATA_SEGMENT_SIZE\t(%uUL)\n", pszOutDefineName, uDataSegmentSize) < 0)
	{
		goto write_failed;
	}
	if (fprintf(fOutFile, "static const IMG_UINT32 g_pui32PDS%s[%u] = {\n", pszOutArrayName, uEncodedProgramLength / sizeof(IMG_UINT32)) < 0)
	{
		goto write_failed;
	}
	{
		IMG_UINT32	uOffset = 0;
		IMG_UINT32	uProgramSize = uEncodedProgramLength / sizeof(IMG_UINT32);

		while (uOffset < uProgramSize)
		{
			IMG_UINT32	uLineLength;
			IMG_UINT32	uIdx;
			
			if ((uProgramSize - uOffset) < 4)
			{
				uLineLength = uProgramSize - uOffset;
			}
			else
			{
				uLineLength = 4;
			}

			for (uIdx = 0; uIdx < uLineLength; uIdx++)
			{
				if (fprintf(fOutFile, "0x%.8X,", ((IMG_PUINT32)pui8EncodedProgram)[uOffset++]) < 0)
				{
					goto write_failed;
				}
				if (uIdx < (uLineLength - 1))
				{
					if (fprintf(fOutFile, " ") < 0)
					{
						goto write_failed;
					}
				}
				else
				{
					if (fprintf(fOutFile, "\n") < 0)
					{
						goto write_failed;
					}
				}
			}
		}
		if (fprintf(fOutFile, "};\n") < 0)
		{
			goto write_failed;
		}
	}
	if (fprintf(fOutFile, "\n\n\n") < 0)
	{
		goto write_failed;
	}
	/*
		Print C routines to set a value for each of the identifiers.
	*/
	if (!PrintDSSetRoutines(fOutFile, pszOutArrayName, pszOutDefineName, uDataSegmentSize))
	{
		goto write_failed;
	}
	fclose(fOutFile);
	return IMG_TRUE;

write_failed:
	fprintf(stderr, "%s: Couldn't write output file %s: %s.\n", pszCmdName, pszOutFileName, strerror(errno));
	fclose(fOutFile);
	unlink(pszOutFileName);
	return IMG_FALSE;
}

/* QAC leave main using basic types */
/* PRQA S 5013 2 */
int main(int argc, char* argv[])
{
	FILE*					fInFile;
	IMG_PUINT8				pui8EncodedProgram;
	IMG_UINT32				uEncodedProgramLength;
	IMG_BOOL				bTestMode;
	static const IMG_CHAR	pszOutPrefix[] = ".h";
	IMG_PCHAR				pszOutFileName;
	IMG_PCHAR				pszOutArrayName;
	IMG_UINT32				uDataSegmentSize;
	IMG_BOOL				bSizeofHeader;
	IMG_PCHAR				pszOutDefineName;

	bTestMode = IMG_FALSE;
	bSizeofHeader = IMG_FALSE;
	while (argc > 1)
	{
		if (argv[1][0] != '-')
		{
			break;
		}
		if (argv[1][1] == '\0')
		{
			break;
		}
		if (argv[1][1] == 't')
		{
			bTestMode = IMG_TRUE;
		}
		else if (argv[1][1] == 's')
		{
			bSizeofHeader = IMG_TRUE;
		}
		else if (stricmp(argv[1], "-cpp") == 0)
		{
			g_bCPreprocessor = IMG_TRUE;
		}
		else
		{
			usage();
			return 1;
		}
		memmove(&argv[1], &argv[2], ((size_t)argc - 1) * sizeof(argv[1]));
		argc--;
	}

	if (argc == 1)
	{
		usage();
		return 1;
	}

	if (argv[1][0] == '-' && argv[1][1] == '\0')
	{
		g_sCurrentLocation.pszSourceFile = strdup("stdin");
		yyin = fInFile = stdin;
#if !defined(LINUX)
		_setmode(_fileno(stdin), _O_BINARY);
#endif /* defined(LINUX) */
	}
	else
	{
		g_sCurrentLocation.pszSourceFile = strdup(argv[1]);
		yyin = fInFile = fopen(argv[1], "rb");
		if (fInFile == NULL)
		{
			fprintf(stderr, "%s: Couldn't open input file %s: %s.\n", argv[0], argv[1], strerror(errno));
			return 1;
		}
	}

	if (argc == 2)
	{
		if (argv[1][0] == '-' && argv[1][1] == '\0')
		{
			pszOutFileName = strdup("out.h");
		}
		else
		{
			IMG_CHAR *pszDot;

			if ((pszDot = strrchr(argv[1], '.')) == NULL)
			{
				/*
					Make the output file name the same as the input file name but with '.h' appended.
				*/
				pszOutFileName = malloc(strlen(argv[1]) + strlen(pszOutPrefix) + 1);
				strcpy(pszOutFileName, argv[1]);
				strcat(pszOutFileName, pszOutPrefix);
			}
			else
			{
				IMG_UINT32	uPrePrefixLength = (IMG_UINT32)(pszDot - argv[1]);

				/*
					Avoid making the input and output file names the same.
				*/
				if (strcmp(pszDot, pszOutPrefix) == 0)
				{
					fprintf(stderr, "%s: If the input file has the extension '.h' then the output file name should be "
						"explicitly specified.\n", argv[0]);
					return 1;
				}

				/*
					Replace the input file name's extension by '.h'.
				*/
				pszOutFileName = malloc(uPrePrefixLength + strlen(pszOutPrefix) + 1);
				memcpy(pszOutFileName, argv[1], uPrePrefixLength);
				memcpy(pszOutFileName + uPrePrefixLength, pszOutPrefix, strlen(pszOutPrefix) + 1);
			}
		}	
	}
	else
	{
		pszOutFileName = strdup(argv[2]);
	}

	if (argc == 4)
	{
		pszOutArrayName = strdup(argv[3]);	
	}
	else if (argc == 3)
	{
		IMG_PCHAR	pszDot, pszStart;

		if ((pszStart = strrchr(pszOutFileName, '\\')) != NULL)
		{
			pszStart++;
		}
		else if ((pszStart = strrchr(pszOutFileName, '/')) != NULL)
		{
			pszStart++;
		}
		else
		{
			pszStart = pszOutFileName;
		}
		pszOutArrayName = strdup(pszStart);
		if ((pszDot = strrchr(pszOutArrayName, '.')) != NULL)
		{
			*pszDot = '\0';
		}
	}
	else
	{
		pszOutArrayName = "Program";
	}

	/*
		Create an all upper-case string from the array name.
	*/
	{
		IMG_PCHAR	pszTemp;

		pszOutDefineName = strdup(pszOutArrayName);
		for (pszTemp = pszOutDefineName; *pszTemp != '\0'; pszTemp++)
		{
			*pszTemp = (IMG_CHAR)toupper((IMG_INT)*pszTemp);
		}
	}

	g_sCurrentLocation.uSourceLine = 1;

	yydebug = 0;
	yy_flex_debug = 0;
	if (yyparse() != 0 || g_bParserError)
	{
		fclose(fInFile);
		return 1;
	}
	fclose(fInFile);

	/*
		Check that all labels and identifier used are defined.
	*/
	if (!CheckDefines())
	{
		return 1;
	}

	/*
		If an instruction doesn't accept an immediate value directly then put the immediate
		in the list of things to be assigned to the datastore.
	*/
	if (!ResolveImmediates())
	{
		return 1;
	}

	/*
		Check the restrictions on the types of each argument.
	*/
	if (!CheckArguments())
	{
		return 1;
	}

	/*
		Reset the allocation status of each location in the data store.
	*/
	memset(g_aabTempUsed, 0, sizeof(g_aabTempUsed));
	memset(g_aabDSUsed, 0, sizeof(g_aabDSUsed));
	g_auDSLocationsLeft[0] = g_auDSLocationsLeft[1] = EURASIA_PDS_DATASTORE_TEMPSTART;
	g_auDSDoubleLocationsLeft[0] = g_auDSDoubleLocationsLeft[1] = EURASIA_PDS_DATASTORE_TEMPSTART / 2;

	/*
		Assign each temporary and data identifier to a data store bank.
	*/
	if (!AssignBanks())
	{
		return 1;
	}

	/*
		Assign data store location for each identifier.
	*/
	if (!AssignDataStoreLocations())
	{
		return 1;
	}

	#if defined(SGX_FEATURE_PDS_LOAD_STORE)
	/*
		Expand load/store instructions.
	*/
	if (!ExpandLOADSTOREInstructions())
	{
		return 1;
	}
	#endif /* defined(SGX_FEATURE_PDS_LOAD_STORE) */

	/*
		Get the address of each label and replace references to the label by the
		address.
	*/
	if (!ResolveLabels())
	{
		return 1;
	}

	/*
		Convert the instructions to machine code.
	*/
	if ((pui8EncodedProgram = EncodeInstructions(&uEncodedProgramLength, &uDataSegmentSize)) == NULL)
	{
		return 1;
	}

	/*
		Create a C header file containing the program.
	*/
	if (!WriteHeader(argv[0], pszOutFileName, pszOutArrayName, pszOutDefineName, uEncodedProgramLength, uDataSegmentSize, pui8EncodedProgram))
	{
		return 1;
	}

	if (bSizeofHeader)
	{
		IMG_PCHAR				pszSizeofHeaderName;
		static const IMG_PCHAR	pszSizeof = "_sizeof";
		IMG_PCHAR				pszDot;

		pszSizeofHeaderName = malloc(strlen(pszOutFileName) + strlen(pszSizeof) + 1);
		
		/*
			Make the name of the header containing the SIZEOF define the same as the
			name of the main header but with "_sizeof" inserted just before the extension.
		*/
		pszDot = strrchr(pszOutFileName, '.');
		if (pszDot == NULL)
		{
			strcpy(pszSizeofHeaderName, pszOutFileName);
			strcat(pszSizeofHeaderName, pszSizeof);
		}
		else
		{
			IMG_UINT32	uPrefixLength = (IMG_UINT32)(pszDot - pszOutFileName);

			memcpy(pszSizeofHeaderName, pszOutFileName, uPrefixLength);
			strcpy(pszSizeofHeaderName + uPrefixLength, pszSizeof);
			strcat(pszSizeofHeaderName, pszDot);
		}

		/*
			Create the header.
		*/
		if (!WriteSizeofHeader(argv[0], pszSizeofHeaderName, pszOutDefineName, uEncodedProgramLength))
		{
			unlink(pszOutFileName);
			return 1;
		}
	}

	/*
		If test mode is on then disassemble the program and dump the mapping between identifiers and 
		data store locations.
	*/
	if (bTestMode)
	{
		printf("Data segment size (bytes): %u\n\n", uDataSegmentSize);

		{
			IMG_UINT32	uIdx;

			printf("Data store use map:\n");
			for (uIdx = 0; uIdx < EURASIA_PDS_DATASTORE_TEMPSTART; uIdx += PDS_NUM_DWORDS_PER_ROW)
			{
				IMG_UINT32	uBank;

				for (uBank = 0; uBank < 2; uBank++)
				{
					IMG_UINT32	uOff;

					printf("ds%u[%.2u-%.2u]: ", uBank, uIdx, uIdx + PDS_NUM_DWORDS_PER_ROW - 1);
					for (uOff = 0; uOff < PDS_NUM_DWORDS_PER_ROW; uOff++)
					{	
						if (g_aabDSUsed[uBank][uIdx + uOff])
						{
							printf("U");
						}
						else
						{
							printf("_");
						}
					}
					printf("\n");
				}
			}
			printf("\n\n");
		}

		ShowMap();

		PDSDisassemble(pui8EncodedProgram + uDataSegmentSize, uEncodedProgramLength - uDataSegmentSize);
	}

	return 0;
}
