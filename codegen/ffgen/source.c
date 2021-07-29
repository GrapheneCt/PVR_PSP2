/******************************************************************************
 * Name         : source.c
 * Author       : James McCarthy
 * Created      : 05/12/2005
 *
 * Copyright    : 2000-2007 by Imagination Technologies Limited.
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
 * $Log: source.c $
 *****************************************************************************/

#include <stdio.h>
#include <stdarg.h>

#include "img_types.h"
#include "sgxdefs.h"
#include "usetab.h"

#include "apidefs.h"
#include "codegen.h"

#include "macros.h"
#include "source.h"


#if defined(STANDALONE) || defined(DEBUG)

#define GET_CODE_PTR(psFFGenCode) &(psFFGenCode->pszCode[psFFGenCode->uCurrentPos])

static IMG_VOID IMG_CALLCONV PrintNewBlock(FFGenCode *psFFGenCode,
										   const IMG_CHAR *pszFormat, ...) IMG_FORMAT_PRINTF(2, 3);

static IMG_VOID IMG_CALLCONV PrintComment(FFGenCode *psFFGenCode,
										  const IMG_CHAR *pszFormat, ...) IMG_FORMAT_PRINTF(2, 3);

static IMG_CHAR * const RegTypeToString[] = {"r","o","pa","sa","i","g","#","i","#","l","DRC","label","p","cp","am","swiz","intsrcsel",
"fltcoeff","labeloff"};

static IMG_CHAR *const IntSrcSelToString[] =
{
	"zero",				/*USEASM_INTSRCSEL_ZERO */
	"one",				/*USEASM_INTSRCSEL_ONE  */
	"asat",				/*USEASM_INTSRCSEL_SRCALPHASAT */
	"rasat",			/*USEASM_INTSRCSEL_RSRCALPHASAT */
	"s0",				/*USEASM_INTSRCSEL_SRC0*/
	"s1",				/*USEASM_INTSRCSEL_SRC1 */
	"s2",				/*USEASM_INTSRCSEL_SRC2 */
	"s0a",				/*USEASM_INTSRCSEL_SRC0ALPHA */
	"s1a",				/*USEASM_INTSRCSEL_SRC1ALPHA */
	"s2a",				/*USEASM_INTSRCSEL_SRC2ALPHA */
	"Cx1",				/* USEASM_INTSRCSEL_CX1 */
	"Cx2",				/*USEASM_INTSRCSEL_CX2  */
	"Cx4",				/*USEASM_INTSRCSEL_CX4  */
	"Cx8",				/*USEASM_INTSRCSEL_CX8  */
	"Ax1",				/* USEASM_INTSRCSEL_AX1 */
	"Ax2",				/* USEASM_INTSRCSEL_AX2 */
	"Ax4",				/* USEASM_INTSRCSEL_AX4 */
	"Ax8",				/* USEASM_INTSRCSEL_AX8 */
	"add",				/* USEASM_INTSRCSEL_ADD	*/
	"sub",				/* USEASM_INTSRCSEL_SUB */
	"neg",				/* USEASM_INTSRCSEL_NEG */
	"none",				/* USEASM_INTSRCSEL_NONE */
	"min",				/*USEASM_INTSRCSEL_MIN */
	"max",				/*USEASM_INTSRCSEL_MAX */
	"s2scale",			/*USEASM_INTSRCSEL_SRC2SCALE */
	"zeros",			/*USEASM_INTSRCSEL_ZEROS */
	"comp",				/*USEASM_INTSRCSEL_COMP */
	"+src2.2",			/*USEASM_INTSRCSEL_PLUSSRC2DOT2 */
	"iadd",				/*USEASM_INTSRCSEL_IADD */
	"scale",			/*USEASM_INTSRCSEL_SCALE */
	"u8",				/*USEASM_INTSRCSEL_U8 */
	"s8",				/*USEASM_INTSRCSEL_S8 */
	"off8",				/*USEASM_INTSRCSEL_O8 */
	"",
	"",
	"",
	"",
	"",
	"",
	"z16",				/*USEASM_INTSRCSEL_Z16 */
	"s16",				/*USEASM_INTSRCSEL_S16 */
	"u32",				/*USEASM_INTSRCSEL_U32 */
	"cie",				/*USEASM_INTSRCSEL_CINENABLE */
	"coe",				/*USEASM_INTSRCSEL_COUTENABLE */
	"u16",				/*USEASM_INTSRCSEL_U16 */
	"interleaved",		/*USEASM_INTSRCSEL_INTERLEAVED */
	"planar",			/*USEASM_INTSRCSEL_PLANAR */
	"src01",			/*USEASM_INTSRCSEL_SRC01 */
	"src23",			/*USEASM_INTSRCSEL_SRC23 */
	"dst01",			/*USEASM_INTSRCSEL_DST01 */
	"dst23",			/*USEASM_INTSRCSEL_DST23 */
	"rnd",				/*USEASM_INTSRCSEL_RND */
	"twosided",			/*USEASM_INTSRCSEL_TWOSIDED */
	"fb",				/*USEASM_INTSRCSEL_FEEDBACK */
	"nfb",				/*USEASM_INTSRCSEL_NOFEEDBACK */
	"optdw",			/*USEASM_INTSRCSEL_OPTDWD */
	"idst",				/*USEASM_INTSRCSEL_IDST */
	"down",				/*USEASM_INTSRCSEL_ROUNDDOWN */
	"nearest",			/*USEASM_INTSRCSEL_ROUNDNEAREST */
	"up",				/*USEASM_INTSRCSEL_ROUNDUP */

	"pixel",
	"sample",
	"selective",
	"pt",
	"vcull",
	"end",
	"parallel",
	"perinstance",
	"srcneg",
	"srcabs",
	"incrementus",
	"incrementgpi",
	"incrementboth",
	"incrementmoe",
	"incrementall",
	"increments1",
	"increments2",
	"increments1s2",
	"f32",
	"f16",
	"or",
	"xor",
	"end",
	"revsub",
	"xchg",
	"true",
	"false",
	"imin",
	"imax",
	"umin",
	"umax"
};
/******************************************************************************
 * Function Name: PrintIndent
 *
 * Inputs       : psFFGenCode
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
static IMG_VOID PrintIndent(FFGenCode *psFFGenCode)
{
	IMG_UINT32 i;

	for (i = 0; i < psFFGenCode->uIndent;  i++)
	{
		psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "    ");
	}
}

static IMG_VOID PrintSpaceForInstrNum(FFGenCode *psFFGenCode)
{
	psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "      ");
}

/******************************************************************************
 * Function Name: PrintNewBlock
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
static IMG_VOID IMG_CALLCONV PrintNewBlock(FFGenCode *psFFGenCode,
										   const IMG_CHAR *pszFormat, ...)
{
	va_list vaArgs;

	IMG_INT32 i, uStartPos;

	psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "\n");

	PrintSpaceForInstrNum(psFFGenCode);
	PrintIndent(psFFGenCode);

	psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "/*******************************************************\n");

	va_start (vaArgs, pszFormat);

	PrintSpaceForInstrNum(psFFGenCode);
	PrintIndent(psFFGenCode);

	uStartPos = psFFGenCode->uCurrentPos;

	psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), " ** ");

	/* Print the comment */
	psFFGenCode->uCurrentPos += vsprintf(GET_CODE_PTR(psFFGenCode), pszFormat, vaArgs);

	va_end (vaArgs);

	psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), " ");


	i = 56 - (psFFGenCode->uCurrentPos - uStartPos);

	while (i > 0)
	{
		psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "*");
		i--;
	}

	psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "\n");

	PrintSpaceForInstrNum(psFFGenCode);
	PrintIndent(psFFGenCode);

	psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), " *******************************************************/\n\n");
}


/******************************************************************************
 * Function Name: PrintComment
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
static IMG_VOID IMG_CALLCONV PrintComment(FFGenCode *psFFGenCode,
										  const IMG_CHAR *pszFormat, ...)
{
	va_list vaArgs;


	/* insert indent if required */
	PrintIndent(psFFGenCode);

	/* Print Start of comment marker */
	psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "/* ");

	va_start (vaArgs, pszFormat);

	/* Print the comment */
	psFFGenCode->uCurrentPos += vsprintf(GET_CODE_PTR(psFFGenCode), pszFormat, vaArgs);

	va_end (vaArgs);

	/* Print end of comment marker */
	psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), " */\n");
}

/******************************************************************************
 * Function Name: AlignText
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
static IMG_UINT32 AlignText(FFGenCode		*psFFGenCode,
								  IMG_UINT32	uAlignmentStart,
								  IMG_UINT32	uAlignmentSize)
{
	if(psFFGenCode->uCurrentPos >= uAlignmentStart + uAlignmentSize)
	{
		psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), " ");
	}
	else
	{
		/* Any space left to align? */
		while (psFFGenCode->uCurrentPos < uAlignmentStart + uAlignmentSize)
		{
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), " ");
		}
	}

	return psFFGenCode->uCurrentPos;
}


/******************************************************************************
 * Function Name: PrintReg
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
static IMG_VOID PrintReg(FFGenCode	*psFFGenCode,
							   FFGenReg		*psReg,
							   IMG_UINT32	*puRegFlag,
							   IMG_INT32	*piRegOffset)
{
	IMG_CHAR cAddSymbol = '+';

	if (*puRegFlag & USEASM_ARGFLAGS_NEGATE)
	{
		psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "-");
	}

	if (piRegOffset)
	{
		if (*piRegOffset < 0)
		{
			cAddSymbol = '-';
			*piRegOffset = -*piRegOffset;
		}
	}

	/* Print dest reg info */
	switch (psReg->eType)
	{
		case USEASM_REGTYPE_INDEX:
		{
			IMG_CHAR acSuffix[3][4] = {"l", "h", "lh"};
			IMG_UINT32 uSuffixIndex;
			
			if (psReg->uOffset == USEREG_INDEX_L)
			{
				uSuffixIndex = 0;
			}
			else if (psReg->uOffset == USEREG_INDEX_H)
			{
				uSuffixIndex = 1;
			}
			else
			{
				uSuffixIndex = 2;
			}
			
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "%s.%s", RegTypeToString[psReg->eType], acSuffix[uSuffixIndex]);
			break;
		}
		case USEASM_REGTYPE_FPINTERNAL:
		case USEASM_REGTYPE_DRC:
		case USEASM_REGTYPE_PREDICATE:
		{
			if (piRegOffset)
			{
				psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode),
													   "%s(%u %c %u)",
													   RegTypeToString[psReg->eType],
													   psReg->uOffset,
													   cAddSymbol,
													   *piRegOffset);
			}
			else
			{
				psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode),
													   "%s%u",
													   RegTypeToString[psReg->eType],
													   psReg->uOffset);

			}
			break;
		}

		case USEASM_REGTYPE_IMMEDIATE:
		{
			if (piRegOffset)
			{
				
			}
			else
			{
				psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode),
													   "%s0x%x",
													   RegTypeToString[psReg->eType],
													   psReg->uOffset);

			}
			break;
		}
		case USEASM_REGTYPE_FPCONSTANT:
		{
			/* Print src reg info */
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode),
												   "%s%f",
												   RegTypeToString[psReg->eType],
												   g_pfHardwareConstants[psReg->uOffset]);
			break;
		}
		case USEASM_REGTYPE_LABEL:
		{
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode),
												   "%s",
												   psFFGenCode->ppsLabelNames[psReg->uOffset]);

			break;

		}
		case USEASM_REGTYPE_INTSRCSEL:
		{
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode),
												"%s",
												 IntSrcSelToString[psReg->uOffset]);
			break;
		}

		default:
		{
			if (psReg->eType >= USEASM_REGTYPE_MAXIMUM)
			{
				printf("PrintReg: Error, reg type out of bound\n");
				return;
			}

			if (piRegOffset)
			{
				if (psFFGenCode->bUseRegisterDescriptions)
				{
					psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode),
														   "%s[%s %c %u]",
														   RegTypeToString[psReg->eType],
														   &(psReg->acDesc[2]),
														   cAddSymbol,
														   *piRegOffset);
				}
				else
				{
					psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode),
														   "%s[%u %c %u]",
														   RegTypeToString[psReg->eType],
														   psReg->uOffset,
														   cAddSymbol,
														   *piRegOffset);
				}
			}
			else
			{
				if(psReg->uIndex == USEREG_INDEX_NONE)
				{
					if (psFFGenCode->bUseRegisterDescriptions)
					{
						psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode),
															   "%s[%s]",
															   RegTypeToString[psReg->eType],
															   &(psReg->acDesc[2]));
					}
					else
					{
						psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode),
															   "%s[%u]",
															   RegTypeToString[psReg->eType],
															   psReg->uOffset);
					}
				}
				else
				{
					IMG_CHAR asIndexDesc[][4] = {"", "i.l", "i.h"};

					psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode),
												       "%s[%s + #%u]",
													   RegTypeToString[psReg->eType],
													   asIndexDesc[psReg->uIndex],
													   psReg->uOffset);
				}
			}
			break;
		}
	}


	if (*puRegFlag)
	{
		/* Add reg modifiers */
		if (*puRegFlag & USEASM_ARGFLAGS_LOW)
		{
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), ".l");
		}

		if (*puRegFlag & USEASM_ARGFLAGS_HIGH)
		{
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), ".h");
		}

		if (*puRegFlag & USEASM_ARGFLAGS_ABSOLUTE)
		{
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), ".abs");
		}

		if(*puRegFlag & ~USEASM_ARGFLAGS_BYTEMSK_CLRMSK)
		{
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode),
				".bytemask%d%d%d%d",
				(*puRegFlag & (8 << USEASM_ARGFLAGS_BYTEMSK_SHIFT)) ? 1: 0,
				(*puRegFlag & (4 << USEASM_ARGFLAGS_BYTEMSK_SHIFT)) ? 1: 0,
				(*puRegFlag & (2 << USEASM_ARGFLAGS_BYTEMSK_SHIFT)) ? 1: 0,
				(*puRegFlag & (1 << USEASM_ARGFLAGS_BYTEMSK_SHIFT)) ? 1: 0);
		}

		if(*puRegFlag & ~USEASM_ARGFLAGS_COMP_CLRMSK)
		{
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), ".%u",
				((*puRegFlag & ~USEASM_ARGFLAGS_COMP_CLRMSK) >> USEASM_ARGFLAGS_COMP_SHIFT));
		}
	}
}

/******************************************************************************
 * Function Name: PrintSMLSI
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
static IMG_VOID PrintSMLSI(FFGenCode *psFFGenCode, FFGenInstruction *psInstruction)
{
	PrintIndent(psFFGenCode);

	psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode),
										   "smlsi #%u, #%u, #%u, #%u, incrementmode, incrementmode, incrementmode, incrementmode, #0, #0, #0;\n",
										   psInstruction->aiRegOffsets[0],
										   psInstruction->aiRegOffsets[1],
										   psInstruction->aiRegOffsets[2],
										   psInstruction->aiRegOffsets[3]);
}


/******************************************************************************
 * Function Name: PrintSMBO
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
static IMG_VOID PrintSMBO(FFGenCode *psFFGenCode, FFGenInstruction *psInstruction)
{
	PrintIndent(psFFGenCode);

	psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), 
										   "smbo #%u, #%u, #%u, #%u;\n",
										   psInstruction->aiRegOffsets[0],
										   psInstruction->aiRegOffsets[1],
										   psInstruction->aiRegOffsets[2],
										   psInstruction->aiRegOffsets[3]);
}


#define FFTNL_DEST_REG_ALIGNMENT 21
#define FFTNL_SRC_REGS_ALIGNMENT 24 

/******************************************************************************
 * Function Name: PrintInstruction
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
IMG_INTERNAL IMG_VOID PrintInstruction(FFGenCode		*psFFGenCode,
									   FFGenInstruction	*psInstruction,
									   IMG_UINT32		uLineNumber,
									   IMG_UINT32		uInstrNumber)
{
	IMG_UINT32 i, uAlignStart, uPredicate, uRepeatCount; 

	/* Sort out comments*/
	switch (psInstruction->eOpcode)
	{
		case USEASM_OP_COMMENT:
		{
			PrintSpaceForInstrNum(psFFGenCode);
			PrintComment(psFFGenCode, "%s", psInstruction->pszComment);
			return;
		}
		case USEASM_OP_COMMENTBLOCK:
		{
			PrintNewBlock(psFFGenCode, "%s", psInstruction->pszComment);
			return;
		}
		default:
		{
			if (psInstruction->pszComment)
			{
				PrintSpaceForInstrNum(psFFGenCode);
				PrintComment(psFFGenCode, "%s", psInstruction->pszComment);
			}
		}
	}
	psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "%4u: ", uInstrNumber);

	/* Filter out some instructions */
	switch (psInstruction->eOpcode)
	{
		case USEASM_OP_SMLSI:
		{
			PrintSMLSI(psFFGenCode, psInstruction);
			return;
		}
		case USEASM_OP_SMBO:
		{
			PrintSMBO(psFFGenCode, psInstruction);
			return;
		}
		case USEASM_OP_LABEL:
		{
			PrintReg(psFFGenCode,
					 psInstruction->ppsRegs[0],
					 &(psInstruction->auRegFlags[0]),
					 IMG_NULL);

			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), ":\n");
			return;
		}

		default:
			break;
	}

	/* insert indent if required */
	PrintIndent(psFFGenCode);

	/* Store start of line position for alignment purposes */
	uAlignStart = psFFGenCode->uCurrentPos;

	uPredicate = GET_PRED(psInstruction);

	/* Use predicate reg */
	if (uPredicate)
	{
		/* Recess predicate test */
		if (psFFGenCode->uIndent)
		{
			psFFGenCode->uCurrentPos -= 3;
		}

		if (uPredicate >= USEASM_PRED_NEGP0 && uPredicate<= USEASM_PRED_NEGP1)
		{
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "!p%u ", uPredicate - USEASM_PRED_NEGP0);
		}
		else if(uPredicate >= USEASM_PRED_P2)
		{
			/* Print predicate reg */
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "p%u ", uPredicate - USEASM_PRED_P2 + 2);	
		}
		else
		{
			/* Print predicate reg */
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "p%u ", uPredicate - USEASM_PRED_P0);
		}
	}

	/* print instruction opcode */
	switch (psInstruction->eOpcode)
	{
#if defined(FFGEN_UNIFLEX)
		case USEASM_OP_FLOOR:
		{
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "floor");

			break;
		}
#endif /* defined(FFGEN_UNIFLEX) */
		default:
		{
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "%s", UseGetNameOpcode(psInstruction->eOpcode));

			break;
		}
	}

	/* Set skip invalid  */
	if (psInstruction->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID)
	{
		psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), ".skipinv");
	}

	/* Set nosched */
	if (psInstruction->uFlags1 & USEASM_OPFLAGS1_NOSCHED)
	{
		psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), ".nosched");
	}

	if(psInstruction->uFlags3 & USEASM_OPFLAGS3_FREEP)
	{
		psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), ".freep");
	}

	if(psInstruction->uFlags1 & USEASM_OPFLAGS1_END)
	{
		psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), ".end");
	}

	uRepeatCount = GET_REPEAT_COUNT(psInstruction);

	/* print repeat/fetch count if required */
	if (uRepeatCount)
	{
		if (psInstruction->eOpcode == USEASM_OP_LDAD || psInstruction->eOpcode == USEASM_OP_STAD)
		{
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode),
												   ".fetch%u",
												   uRepeatCount);
		}
		else
		{
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode),
												   ".rpt%u",
												   uRepeatCount);
		}
	}

	if (psInstruction->eOpcode == USEASM_OP_LDAD || psInstruction->eOpcode == USEASM_OP_STAD)
	{
		if(psInstruction->uFlags2 & USEASM_OPFLAGS2_FORCELINEFILL)
		{
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), ".fcfill");
		}
		else if(psInstruction->uFlags2 & USEASM_OPFLAGS2_BYPASSCACHE)
		{
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), ".bpcache");
		}
	}

	if (psInstruction->uFlags1 & USEASM_OPFLAGS1_TESTENABLE)
	{
		IMG_UINT32 uSignTest = ((psInstruction->uTest & ~USEASM_TEST_SIGN_CLRMSK) >> USEASM_TEST_SIGN_SHIFT);
		IMG_UINT32 uZeroTest = ((psInstruction->uTest & ~USEASM_TEST_ZERO_CLRMSK) >> USEASM_TEST_ZERO_SHIFT);
		IMG_UINT32 uAndTest  =  (psInstruction->uTest &  USEASM_TEST_CRCOMB_AND);

		psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), ".test");

		switch(uSignTest)
		{
		case USEASM_TEST_SIGN_TRUE:
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "t");
			break;
		case USEASM_TEST_SIGN_NEGATIVE:
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "n");
			break;
		case USEASM_TEST_SIGN_POSITIVE:
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "p");
			break;
		}

		if(uAndTest) psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "&");
		else psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "|");

		switch(uZeroTest)
		{
		case USEASM_TEST_ZERO_TRUE:
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "t");
			break;
		case USEASM_TEST_ZERO_NONZERO:
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "nz");
			break;
		case USEASM_TEST_ZERO_ZERO:
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "z");
			break;
		}
	}

	if (psInstruction->eOpcode == USEASM_OP_LDAD)
	{
		/* Align for reg */
		uAlignStart = AlignText(psFFGenCode, uAlignStart, FFTNL_DEST_REG_ALIGNMENT);

		/* print the register */
		PrintReg(psFFGenCode,
				 psInstruction->ppsRegs[0],
				 &(psInstruction->auRegFlags[0]),
				 psInstruction->uUseRegOffset & (1 << 0) ? &(psInstruction->aiRegOffsets[0]) : IMG_NULL);

		/* Add comma */
		psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), ",");

		/* Align for reg */
		uAlignStart = AlignText(psFFGenCode, uAlignStart, FFTNL_SRC_REGS_ALIGNMENT);

		psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "[");

		/* print the register */
		PrintReg(psFFGenCode,
				 psInstruction->ppsRegs[1],
				 &(psInstruction->auRegFlags[1]),
				 psInstruction->uUseRegOffset & (1 << 1) ? &(psInstruction->aiRegOffsets[1]) : IMG_NULL);


		/* Add comma */
		psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), ", ");

		if (psInstruction->uFlags1 & USEASM_OPFLAGS1_PREINCREMENT)
		{
			/* Adjust format for ldads */
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "++");
		}
		else
		{
			//psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "+");
		}

		/* print the register */
		PrintReg(psFFGenCode,
				 psInstruction->ppsRegs[2],
				 &(psInstruction->auRegFlags[2]),
				 psInstruction->uUseRegOffset & (1 << 2) ? &(psInstruction->aiRegOffsets[2]) : IMG_NULL);

		if (psInstruction->uFlags1 & USEASM_OPFLAGS1_POSTINCREMENT)
		{
			/* Adjust format for ldads */
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "++");
		}

		/* Adjust format for ldads */
		psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "],");

		/* Align for reg */
		uAlignStart = AlignText(psFFGenCode, uAlignStart, FFTNL_SRC_REGS_ALIGNMENT);

		/* Skip range reg */
		PrintReg(psFFGenCode,
				 psInstruction->ppsRegs[4],
				 &(psInstruction->auRegFlags[4]),
				 psInstruction->uUseRegOffset & (1 << 4) ? &(psInstruction->aiRegOffsets[4]) : IMG_NULL);		

	}
	else if (psInstruction->eOpcode == USEASM_OP_STAD)
	{
		/* Align for reg */
		uAlignStart = AlignText(psFFGenCode, uAlignStart, FFTNL_DEST_REG_ALIGNMENT);

		psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "[");

		/* print the register */
		PrintReg(psFFGenCode,
				 psInstruction->ppsRegs[0],
				 &(psInstruction->auRegFlags[0]),
				 psInstruction->uUseRegOffset & (1 << 0) ? &(psInstruction->aiRegOffsets[0]) : IMG_NULL);


		/* Add comma */
		psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), ", ");

		if (psInstruction->uFlags1 & USEASM_OPFLAGS1_PREINCREMENT)
		{
			/* Adjust format for ldads */
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "++");
		}
		else
		{
			//psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "+");
		}

		/* print the register */
		PrintReg(psFFGenCode,
				 psInstruction->ppsRegs[1],
				 &(psInstruction->auRegFlags[1]),
				 psInstruction->uUseRegOffset & (1 << 1) ? &(psInstruction->aiRegOffsets[1]) : IMG_NULL);

		if (psInstruction->uFlags1 & USEASM_OPFLAGS1_POSTINCREMENT)
		{
			/* Adjust format for stad */
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "++");
		}

		/* Adjust format for stad */
		psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "],");

		/* Align for reg */
		uAlignStart = AlignText(psFFGenCode, uAlignStart, FFTNL_SRC_REGS_ALIGNMENT);

		/* Skip range reg */
		PrintReg(psFFGenCode,
				 psInstruction->ppsRegs[2],
				 &(psInstruction->auRegFlags[2]),
				 psInstruction->uUseRegOffset & (1 << 2) ? &(psInstruction->aiRegOffsets[2]) : IMG_NULL);		

	}
	else if(psInstruction->eOpcode == USEASM_OP_IDF)
	{
				/* Align for reg */
		uAlignStart = AlignText(psFFGenCode, uAlignStart, FFTNL_DEST_REG_ALIGNMENT);

		if (!psInstruction->ppsRegs[0])
		{
			psFFGenCode->psFFGenContext->pfnPrint("PrintInstruction: Reg was null (%u)\n", uLineNumber);
			return;
		}

		/* print the register */
		PrintReg(psFFGenCode,
				 psInstruction->ppsRegs[0],
				 &(psInstruction->auRegFlags[0]),
				 psInstruction->uUseRegOffset & (1 << 0) ? &(psInstruction->aiRegOffsets[0]) : IMG_NULL);

		/* Add comma */
		psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), ",");
		
		uAlignStart = AlignText(psFFGenCode, uAlignStart, FFTNL_SRC_REGS_ALIGNMENT);

		switch(psInstruction->ppsRegs[1]->uOffset)
		{
			case EURASIA_USE1_IDF_PATH_ST:
				psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "st");
				break;
			case EURASIA_USE1_IDF_PATH_PIXELBE:
				psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), "pixelbe");
				break;
		}
		
	}
	else if (psInstruction->uNumRegs)
	{
		/* Loop through all regs */
		for (i = 0; i < psInstruction->uNumRegs; i++)
		{
			/* Align for reg */
			uAlignStart = AlignText(psFFGenCode, uAlignStart, (i == 0) ? FFTNL_DEST_REG_ALIGNMENT : FFTNL_SRC_REGS_ALIGNMENT);

			if (!psInstruction->ppsRegs[i])
			{
				psFFGenCode->psFFGenContext->pfnPrint("PrintInstruction: Reg was null (%u)\n", uLineNumber);
				return;
			}

			/* print the register */
			PrintReg(psFFGenCode,
					 psInstruction->ppsRegs[i],
					 &(psInstruction->auRegFlags[i]),
					 psInstruction->uUseRegOffset & (1 << i) ? &(psInstruction->aiRegOffsets[i]) : IMG_NULL);

			if (i == 0 && psInstruction->uExtraInfo)
			{
				static IMG_CHAR * const acInternalReg[] = {"i0", "i1", "a0", "a1"}; 
				IMG_UINT32 uInternalReg = (psInstruction->uExtraInfo & ~EURASIA_USE1_EFO_DSRC_CLRMSK) >> EURASIA_USE1_EFO_DSRC_SHIFT;


				psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), " = %s", acInternalReg[uInternalReg]);
			}

			/* Add comma */
			psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), ",");
		}

		/* Remove last comma */
		psFFGenCode->uCurrentPos--;
	}

	/* add semicolon/colon and carriage return */
	if (psInstruction->eOpcode == USEASM_OP_LABEL)
	{
		psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), ":\n");
	}
	else 
	{
		psFFGenCode->uCurrentPos += sprintf(GET_CODE_PTR(psFFGenCode), ";\n"); 
	}
}

/******************************************************************************
 * Function Name: DumpBindingInfo
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
static IMG_VOID DumpBindingInfo(FILE *psAsmFile, FFGenRegList *psList)
{
	while (psList)
	{
		FFGenReg *psReg = psList->psReg;
		IMG_CHAR *pszRegType = ((psReg->eType == USEASM_REGTYPE_TEMP) ? "mem" : "sa");
		IMG_UINT32 ui32Start = psReg->uOffset;
		IMG_UINT32 ui32End = (psReg->uOffset + psReg->uSizeInDWords - 1);
		IMG_CHAR aszBuf[256];

		IMG_BOOL bIsSA = (psReg->eType != USEASM_REGTYPE_TEMP);

		PVR_UNREFERENCED_PARAMETER(bIsSA);

		switch (psReg->eBindingRegDesc)
		{
			case FFGEN_INPUT_COLOR:                           sprintf(aszBuf, "\tCOLOR                           = pa[%u..%u]", ui32Start, ui32End); break;
			case FFGEN_INPUT_SECONDARYCOLOR:                  sprintf(aszBuf, "\tSECONDARYCOLOR                  = pa[%u..%u]", ui32Start, ui32End); break;
			case FFGEN_INPUT_NORMAL:                          sprintf(aszBuf, "\tNORMAL                          = pa[%u..%u]", ui32Start, ui32End); break;
			case FFGEN_INPUT_VERTEX:                          sprintf(aszBuf, "\tVERTEX                          = pa[%u..%u]", ui32Start, ui32End); break;
			case FFGEN_INPUT_MULTITEXCOORD0:                  sprintf(aszBuf, "\tMULTITEXCOORD0                  = pa[%u..%u]", ui32Start, ui32End); break;
			case FFGEN_INPUT_MULTITEXCOORD1:                  sprintf(aszBuf, "\tMULTITEXCOORD1                  = pa[%u..%u]", ui32Start, ui32End); break;
			case FFGEN_INPUT_MULTITEXCOORD2:                  sprintf(aszBuf, "\tMULTITEXCOORD2                  = pa[%u..%u]", ui32Start, ui32End); break;
			case FFGEN_INPUT_MULTITEXCOORD3:                  sprintf(aszBuf, "\tMULTITEXCOORD3                  = pa[%u..%u]", ui32Start, ui32End); break;
			case FFGEN_INPUT_MULTITEXCOORD4:                  sprintf(aszBuf, "\tMULTITEXCOORD4                  = pa[%u..%u]", ui32Start, ui32End); break;
			case FFGEN_INPUT_MULTITEXCOORD5:                  sprintf(aszBuf, "\tMULTITEXCOORD5                  = pa[%u..%u]", ui32Start, ui32End); break;
			case FFGEN_INPUT_MULTITEXCOORD6:                  sprintf(aszBuf, "\tMULTITEXCOORD6                  = pa[%u..%u]", ui32Start, ui32End); break;
			case FFGEN_INPUT_MULTITEXCOORD7:                  sprintf(aszBuf, "\tMULTITEXCOORD7                  = pa[%u..%u]", ui32Start, ui32End); break;
			case FFGEN_INPUT_FOGCOORD:                        sprintf(aszBuf, "\tFOGCOORD                        = pa[%u..%u]", ui32Start, ui32End); break;
			case FFGEN_INPUT_VERTEXBLENDWEIGHT:               sprintf(aszBuf, "\tSPECIAL_VERTEXBLENDWEIGHT       = pa[%u..%u]", ui32Start, ui32End); break;
			case FFGEN_INPUT_VERTEXMATRIXINDEX:               sprintf(aszBuf, "\tSPECIAL_VERTEXMATRIXINDEX       = pa[%u..%u]", ui32Start, ui32End); break;
			case FFGEN_INPUT_POINTSIZE: 		              sprintf(aszBuf, "\tSPECIAL_POINTSIZE               = pa[%u..%u]", ui32Start, ui32End); break;
			case FFGEN_OUTPUT_POSITION:	                      sprintf(aszBuf, "\tPOSITION                        = o[%u..%u]",  ui32Start, ui32End); break;
			case FFGEN_OUTPUT_POINTSIZE:   	                  sprintf(aszBuf, "\tPOINTSIZE                       = o[%u..%u]",  ui32Start, ui32End); break;
			case FFGEN_OUTPUT_CLIPVERTEX:  	                  sprintf(aszBuf, "\tCLIPVERTEX                      = o[%u..%u]",  ui32Start, ui32End); break;
			case FFGEN_OUTPUT_FRONTCOLOR: 	                  sprintf(aszBuf, "\tFRONTCOLOR                      = o[%u..%u]",  ui32Start, ui32End); break;
			case FFGEN_OUTPUT_BACKCOLOR: 	                  sprintf(aszBuf, "\tBACKCOLOR                       = o[%u..%u]",  ui32Start, ui32End); break;
			case FFGEN_OUTPUT_FRONTSECONDARYCOLOR:            sprintf(aszBuf, "\tFRONTSECONDARYCOLOR             = o[%u..%u]",  ui32Start, ui32End); break;
			case FFGEN_OUTPUT_BACKSECONDARYCOLOR:             sprintf(aszBuf, "\tBACKSECONDARYCOLOR              = o[%u..%u]",  ui32Start, ui32End); break;
			case FFGEN_OUTPUT_TEXCOORD: 	                  sprintf(aszBuf, "\tTEXCOORD                        = o[%u..%u]",  ui32Start, ui32End); break;
			case FFGEN_OUTPUT_FOGFRAGCOORD: 	              sprintf(aszBuf, "\tFOGFRAGCOORD                    = o[%u..%u]",  ui32Start, ui32End); break;
			case FFGEN_OUTPUT_POINTSPRITE: 	              	  sprintf(aszBuf, "\tPOINTSPRITE (DUMMY TEXCOORD)    = o[%u..%u]",  ui32Start, ui32End); break;
			case FFGEN_STATE_MODELVIEWMATRIX:                 sprintf(aszBuf, "\tMODELVIEWMATRIX                 = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_MODELVIEWMATRIXPALETTE:          sprintf(aszBuf, "\tMODELVIEWMATRIXPALETTE          = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_PROJECTMATRIX:                   sprintf(aszBuf, "\tPROJECTMATRIX                   = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_MODELVIEWPROJECTIONMATRIX:       sprintf(aszBuf, "\tMODELVIEWPROJECTIONMATRIX       = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_TEXTUREMATRIX:                   sprintf(aszBuf, "\tTEXTUREMATRIX                   = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_MODELVIEWMATRIXINVERSETRANSPOSE: sprintf(aszBuf, "\tMODELVIEWMATRIXINVERSETRANSPOSE = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_MODELVIEWMATRIXINVERSETRANSPOSEPALETTE: sprintf(aszBuf, "\tMODELVIEWMATRIXINVERSETRANSPOSEPALETTE = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_CLIPPLANE:                       sprintf(aszBuf, "\tCLIPPLANE                       = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_POINT:                           sprintf(aszBuf, "\tPOINT                           = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_FRONTMATERIAL:                   sprintf(aszBuf, "\tFRONTMATERIAL                   = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_BACKMATERIAL:                    sprintf(aszBuf, "\tBACKMATERIAL                    = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_LIGHTSOURCE:                     sprintf(aszBuf, "\tLIGHTSOURCE                     = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_LIGHTSOURCE0:                    sprintf(aszBuf, "\tLIGHTSOURCE 0                   = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_LIGHTSOURCE1:                    sprintf(aszBuf, "\tLIGHTSOURCE 1                   = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_LIGHTSOURCE2:                    sprintf(aszBuf, "\tLIGHTSOURCE 2                   = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_LIGHTSOURCE3:                    sprintf(aszBuf, "\tLIGHTSOURCE 3                   = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_LIGHTSOURCE4:                    sprintf(aszBuf, "\tLIGHTSOURCE 4                   = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_LIGHTSOURCE5:                    sprintf(aszBuf, "\tLIGHTSOURCE 5                   = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_LIGHTSOURCE6:                    sprintf(aszBuf, "\tLIGHTSOURCE 6                   = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_LIGHTSOURCE7:                    sprintf(aszBuf, "\tLIGHTSOURCE 7                   = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_LIGHTMODEL:                      sprintf(aszBuf, "\tLIGHTMODEL                      = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_FRONTLIGHTMODELPRODUCT:          sprintf(aszBuf, "\tFRONTLIGHTMODELPRODUCT          = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_BACKLIGHTMODELPRODUCT:           sprintf(aszBuf, "\tBACKLIGHTMODELPRODUCT           = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_FRONTLIGHTPRODUCT:               sprintf(aszBuf, "\tFRONTLIGHTPRODUCT               = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_BACKLIGHTPRODUCT:                sprintf(aszBuf, "\tBACKLIGHTPRODUCT                = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_EYEPLANES:                       sprintf(aszBuf, "\tEYEPLANES                       = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_EYEPLANET:                       sprintf(aszBuf, "\tEYEPLANET                       = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_EYEPLANER:                       sprintf(aszBuf, "\tEYEPLANER                       = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_EYEPLANEQ:                       sprintf(aszBuf, "\tEYEPLANEQ                       = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_OBJECTPLANES:                    sprintf(aszBuf, "\tOBJECTPLANES                    = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_OBJECTPLANET:                    sprintf(aszBuf, "\tOBJECTPLANET                    = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_OBJECTPLANER:                    sprintf(aszBuf, "\tOBJECTPLANER                    = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_OBJECTPLANEQ:                    sprintf(aszBuf, "\tOBJECTPLANEQ                    = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_PMXFOGPARAM:                     sprintf(aszBuf, "\tPMXFOGPARAM                     = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_PMXPOINTSIZE:                    sprintf(aszBuf, "\tPMXPOINTSIZE                    = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;

#if defined(OGL_LINESTIPPLE)
			case FFGEN_STATE_COMMWORDPOSITION:                sprintf(aszBuf, "\tCOMMWORDPOSITION                = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_SYNCWORD:						  sprintf(aszBuf, "\tSYNCWORD                        = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_INVERTEXSTRIDE:				  sprintf(aszBuf, "\tINVERTEXSTRIDE                  = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_HALFLINEWIDTH:					  sprintf(aszBuf, "\tHALFLINEWIDTH                   = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_HALFVIEWPORT:				      sprintf(aszBuf, "\tHALFVIEWPORT                    = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_ONEOVERHALFVIEWPORT:			  sprintf(aszBuf, "\tONEOVERHALFVIEWPORT             = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_LINEOFFSET:                      sprintf(aszBuf, "\tLINEOFFSET                      = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_LINEREPEAT:                      sprintf(aszBuf, "\tLINEREPEAT                      = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_LINEWIDTH:                       sprintf(aszBuf, "\tLINEWIDTH                       = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_ACCUMUPARAMS:					  sprintf(aszBuf, "\tACCUMUPARAMS                    = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_INTERSECPARAMS:				  sprintf(aszBuf, "\tINTERSECPARAMS                  = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_TEXTUREMATRIX0:				  sprintf(aszBuf, "\tTEXTUREMATRIX0                  = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_TEXTUREMATRIX1:				  sprintf(aszBuf, "\tTEXTUREMATRIX1                  = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_TEXTUREMATRIX2:				  sprintf(aszBuf, "\tTEXTUREMATRIX2                  = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_TEXTUREMATRIX3:				  sprintf(aszBuf, "\tTEXTUREMATRIX3                  = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_TEXTUREMATRIX4:				  sprintf(aszBuf, "\tTEXTUREMATRIX4                  = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_TEXTUREMATRIX5:				  sprintf(aszBuf, "\tTEXTUREMATRIX5                  = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_TEXTUREMATRIX6:				  sprintf(aszBuf, "\tTEXTUREMATRIX6                  = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_TEXTUREMATRIX7:				  sprintf(aszBuf, "\tTEXTUREMATRIX7                  = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
			case FFGEN_STATE_LSTRIPSEGMENTCOUNT:			  sprintf(aszBuf, "\tLSTRIPSEGMENTCOUNT              = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
#endif

			default:                                          sprintf(aszBuf, "\tUNKNOWN                         = %s[%u..%u]", pszRegType, ui32Start, ui32End); break;
		}
		
		fprintf(psAsmFile, "%s", aszBuf);

		if (psReg->eBindingRegDesc >= FFGEN_STATE_MODELVIEWMATRIX && 
			psReg->eBindingRegDesc <= FFGEN_STATE_PMXPOINTSIZE)
		{
			sprintf(aszBuf, "\t bytes [%u..%u]", psReg->uOffset * 4, (psReg->uOffset + psReg->uSizeInDWords - 1) * 4);

#if 0
			if (bIsSA)
			{
				sprintf(aszBuf, "\t SA[%u..%u]", psReg->uOffset, psReg->uOffset + psReg->uSizeInDWords);
			}
#endif

			fprintf(psAsmFile, "%s", aszBuf);
		}

		fprintf(psAsmFile, "\n");

		psList = psList->psNext;
	}
}


/******************************************************************************
 * Function Name: DumpSource
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
IMG_INTERNAL IMG_VOID DumpSource(FFGenCode *psFFGenCode, FILE *psAsmFile)
{
	IMG_UINT32 i;

	fprintf (psAsmFile, "/*\n\tInput Register descriptions\n\n");

	DumpBindingInfo(psAsmFile, psFFGenCode->psInputsList);

	fprintf (psAsmFile, "\n\tOutput Register descriptions\n\n");

	DumpBindingInfo(psAsmFile, psFFGenCode->psOutputsList);

	if (psFFGenCode->uNumTexCoordUnits)
	{
		fprintf (psAsmFile, "\n\tOutput Texture coordinate dimensions = {");

		for (i = 0; i < psFFGenCode->uNumTexCoordUnits; i++)
		{
			fprintf(psAsmFile, "%ud", psFFGenCode->auOutputTexDimensions[i]);

			if (i < (psFFGenCode->uNumTexCoordUnits - 1))
			{
				fprintf(psAsmFile,", ");
			}
		}

		fprintf(psAsmFile,"}\n");
	}

	fprintf (psAsmFile, "\n\tConstants Register descriptions\n\n");

	DumpBindingInfo(psAsmFile, psFFGenCode->psConstantsList);

	/* Print some stats */
	fprintf(psAsmFile, "\n\tNum instructions     = %u\n", psFFGenCode->uNumInstructions);
	fprintf(psAsmFile, "\tInputs Size          = %u\n",   psFFGenCode->uInputSize);
	fprintf(psAsmFile, "\tOutputs Size         = %u\n",   psFFGenCode->uOutputSize);
	fprintf(psAsmFile, "\tConstants   size     = %u\n",   (psFFGenCode->uSecAttribSize + psFFGenCode->uMemoryConstantsSize));
	fprintf(psAsmFile, "\t  Sec attribs size   = %u\n",   psFFGenCode->uSecAttribSize);
	fprintf(psAsmFile, "\t  Mem Constants Size = %u\n",   psFFGenCode->uMemoryConstantsSize);
	fprintf(psAsmFile, "\tTemps Size           = %u\n",   psFFGenCode->uTempSize);

	if (psFFGenCode->eCodeGenFlags & FFGENCGF_REDIRECT_OUTPUT_TO_INPUT)
	{
		fprintf(psAsmFile, "\tOutputs redirected to inputs\n");
	}

	fprintf(psAsmFile, "*/\n\n");

	fprintf(psAsmFile, "%s", psFFGenCode->pszCode);
}

#define DUMP_COORDMASK(coordarray) \
fprintf(psAsmFile, "\t{"); \
for (i = 0; i < ROUND_BYTE_ARRAY_TO_DWORD(FFTNLGEN_MAX_NUM_TEXTURE_UNITS); i++) { \
fprintf(psAsmFile, "0x%1X", coordarray[i]); \
if (i < ROUND_BYTE_ARRAY_TO_DWORD(FFTNLGEN_MAX_NUM_TEXTURE_UNITS) - 1) fprintf(psAsmFile, ", "); } \
fprintf(psAsmFile, "},\n"); 

/******************************************************************************
 * Function Name: DumpFFTNLDescription
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
IMG_INTERNAL IMG_VOID DumpFFTNLDescription(FFGenContext	*psFFGenContext,
										   FFTNLGenDesc	*psFFTNLGenDesc,
										   IMG_CHAR		bNewFile,
										   IMG_CHAR		*pszFileName)
{
	IMG_UINT32 i, j;
	FILE *psAsmFile;
	IMG_UINT32 *puDescData = (IMG_UINT32 *)psFFTNLGenDesc;

	if (!pszFileName)
	{
		return;
	}

	if (bNewFile)
	{
		psAsmFile = fopen(pszFileName,"w+t");
	}
	else
	{
		psAsmFile = fopen(pszFileName,"a+t");
	}
	
	if (!psAsmFile)
	{
		psFFGenContext->pfnPrint("FFTNLDumpFFTNLDescription: Failed to open file\n");
		return;
	}

	fprintf(psAsmFile, "/*\n");
	fprintf(psAsmFile, "===========================\n");
	fprintf(psAsmFile, "TNL program %u\n", psFFGenContext->uProgramCount);
	fprintf(psAsmFile, "===========================\n");

	fprintf(psAsmFile, "FFTNLGenDesc sFFTNLGenDesc = {\n");
#if 0
	fprintf(psAsmFile, "\t0x%08X, // eFFTNLEnables \n",                psFFTNLGenDesc->eFFTNLEnables);
	fprintf(psAsmFile, "\t0x%08lX, // uSecAttrConstBaseAddressReg \n",  psFFTNLGenDesc->uSecAttrConstBaseAddressReg);
	fprintf(psAsmFile, "\t0x%08lX, // uSecAttrStart \n",                psFFTNLGenDesc->uSecAttrStart);
	fprintf(psAsmFile, "\t0x%08lX, // uSecAttrEnd \n",                  psFFTNLGenDesc->uSecAttrEnd);
	fprintf(psAsmFile, "\t0x%08lX, // uSecAttrAllOther \n",             psFFTNLGenDesc->uSecAttrAllOther);
	fprintf(psAsmFile, "\t0x%08X, // eCodeGenMethod \n",                psFFTNLGenDesc->eCodeGenMethod);
	fprintf(psAsmFile, "\t0x%08lX, // uEnabledClipPlanes \n",           psFFTNLGenDesc->uEnabledClipPlanes);
	fprintf(psAsmFile, "\t0x%08lX, // uNumBlendUnits \n",               psFFTNLGenDesc->uNumBlendUnits);
	fprintf(psAsmFile, "\t0x%08lX, // uNumMatrixPaletteEntries \n",     psFFTNLGenDesc->uNumMatrixPaletteEntries);
	fprintf(psAsmFile, "\t0x%08lX, // uEnabledSpotLocalLights \n",      psFFTNLGenDesc->uEnabledSpotLocalLights);
	fprintf(psAsmFile, "\t0x%08lX, // uEnabledSpotInfiniteLights \n",   psFFTNLGenDesc->uEnabledSpotInfiniteLights);
	fprintf(psAsmFile, "\t0x%08lX, // uEnabledPointLocalLights \n",     psFFTNLGenDesc->uEnabledPointLocalLights);
	fprintf(psAsmFile, "\t0x%08lX, // uEnabledPointInfiniteLights \n",  psFFTNLGenDesc->uEnabledPointInfiniteLights);
	fprintf(psAsmFile, "\t0x%08lX, // uLightHasSpecular \n",            psFFTNLGenDesc->uLightHasSpecular);
	fprintf(psAsmFile, "\t0x%08lX, // uEnabledPassthroughCoords \n",    psFFTNLGenDesc->uEnabledPassthroughCoords);
	DUMP_COORDMASK(psFFTNLGenDesc->aubPassthroughCoordMask);
	fprintf(psAsmFile, "\t0x%08lX, // uEnabledEyeLinearCoords \n",      psFFTNLGenDesc->uEnabledEyeLinearCoords);
	DUMP_COORDMASK(psFFTNLGenDesc->aubEyeLinearCoordMask);
	fprintf(psAsmFile, "\t0x%08lX, // uEnabledObjLinearCoords \n",      psFFTNLGenDesc->uEnabledObjLinearCoords);
	DUMP_COORDMASK(psFFTNLGenDesc->aubObjLinearCoordMask);
	fprintf(psAsmFile, "\t0x%08lX, // uEnabledSphereMapCoords \n",      psFFTNLGenDesc->uEnabledSphereMapCoords);
	DUMP_COORDMASK(psFFTNLGenDesc->aubSphereMapCoordMask);
	fprintf(psAsmFile, "\t0x%08lX, // uEnabledNormalMapCoords \n",      psFFTNLGenDesc->uEnabledNormalMapCoords);
	DUMP_COORDMASK(psFFTNLGenDesc->aubNormalMapCoordMask);
	fprintf(psAsmFile, "\t0x%08lX, // uEnabledReflectionMapCoords \n",  psFFTNLGenDesc->uEnabledReflectionMapCoords);
	DUMP_COORDMASK(psFFTNLGenDesc->aubReflectionMapCoordMask);
	fprintf(psAsmFile, "\t0x%08lX, // uEnabledTextureMatrices \n",      psFFTNLGenDesc->uEnabledTextureMatrices);
	fprintf(psAsmFile, "\t0x%08lX, // uEnabledVertexBlendUnits \n",     psFFTNLGenDesc->uEnabledVertexBlendUnits);
#else
	for(i = 0; i < sizeof(FFTNLGenDesc)/4; i+= 8)
	{
		fprintf(psAsmFile, "\t\t");
		for(j = 0; j < 8; j++)
		{
			if((i + j) >= sizeof(FFTNLGenDesc)/4) break; 
			fprintf(psAsmFile, "0x%08X, ", puDescData[i + j]);
		}
		fprintf(psAsmFile, "\n");
	}
	fprintf(psAsmFile,"\n");
#endif
	fprintf(psAsmFile, "};\n");
	fprintf(psAsmFile, "*/\n");

	fclose(psAsmFile);
}


#if defined(OGL_LINESTIPPLE)
/******************************************************************************
 * Function Name: DumpFFGEODescription
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
IMG_INTERNAL IMG_VOID DumpFFGEODescription(FFGenContext	*psFFGenContext,
										   FFGEOGenDesc	*psFFGEOGenDesc,
										   IMG_CHAR		*pszFileName)
{
	IMG_UINT32 i, j;
	FILE *psAsmFile;
	IMG_UINT32 *puDescData = (IMG_UINT32 *)psFFGEOGenDesc;
	IMG_UINT32 uEnables;
	static IMG_CHAR *CapStyleName[4] = {"none", "butt", "round", "square"}; 
	static IMG_CHAR *JoinStyleName[4] = {"none", "bevel", "round", "mitre"}; 
	static IMG_CHAR *PrimTypeName[3] = {"GL_LINES", "GL_LINE_STRIP", "GL_LINE_LOOP"};

	static IMG_CHAR *AttribName[FFGEO_ATTRIB_TYPES] = {"POS", "COL0", "COL1", 
		"TC0", "TC1", "TC2", "TC3", "TC4", "TC5", "TC6", "TC7", "TC8", "TC9", 
		"FOG", "PT", "CLIP0", "CLIP1", "CLIP2", "CLIP3", "CLIP4", "CLIP5", "HLWIDTH"
	};
	if (!pszFileName)
	{
		return;
	}

	psAsmFile = fopen(pszFileName,"a+t");
	
	if (!psAsmFile)
	{
		psFFGenContext->pfnPrint("FFTNLDumpFFTNLDescription: Failed to open file\n");
		return;
	}

	fprintf(psAsmFile, "/*\n");
	fprintf(psAsmFile, "===========================\n");
	fprintf(psAsmFile, "Geometry program %d\n", psFFGenContext->uProgramCount);
	fprintf(psAsmFile, "===========================\n");

	fprintf(psAsmFile, "FFGEOGenDesc sFFGEOGenDesc = {\n");


	for(i = 0; i < sizeof(FFGEOGenDesc)/4; i+= 8)
	{
		fprintf(psAsmFile, "\t\t");
		for(j = 0; j < 8; j++)
		{
			if((i + j) >= sizeof(FFGEOGenDesc)/4) break; 
			fprintf(psAsmFile, "0x%08X, ", puDescData[i + j]);
		}
		fprintf(psAsmFile, "\n");
	}
	fprintf(psAsmFile,"\n");

	fprintf(psAsmFile, "};\n");

	/* Dump more informaiton about description */
	fprintf(psAsmFile, "Shader sync    : %s\n",  psFFGEOGenDesc->eGEOEnables & FFGEO_ENABLE_SHADER_SYNC ? "enabled" : "disable");

	/* Cap style */
	fprintf(psAsmFile, "Line cap style : %s\n", CapStyleName[psFFGEOGenDesc->eLineCapStyle]);
	
	/* Join style */	
	fprintf(psAsmFile, "Line join style: %s\n", JoinStyleName[psFFGEOGenDesc->eLineJoinStyle]);

	/* Line primitive type */
	fprintf(psAsmFile, "Line prim type:  %s\n", PrimTypeName[psFFGEOGenDesc->eLinePrimType]);

	fprintf(psAsmFile,"\n");

	/* Texgen enables */
	uEnables = psFFGEOGenDesc->uLineTexGenUnitEnables;
	if(uEnables)
	{
		IMG_UINT32 uUnit = 0;
		static IMG_CHAR *asModeDesc[6] = {"pixel", "param", "none"};
		static IMG_CHAR *asAccumDesc[15] = {"accum disable", "accum enable"};
		
		fprintf(psAsmFile, "Texture coordinate generation enables:\n"); 
		while(uEnables)
		{
			if(uEnables & 1)
			{
				fprintf(psAsmFile, "   tc%d(%s, %s, %s)\n", 
									uUnit, 
									asModeDesc[(psFFGEOGenDesc->aubLineTexGenCoordMask[uUnit] & 1) ? psFFGEOGenDesc->aeLineTexGenMode[uUnit][0] : 2],
									asModeDesc[(psFFGEOGenDesc->aubLineTexGenCoordMask[uUnit] & 2) ? psFFGEOGenDesc->aeLineTexGenMode[uUnit][1] : 2],
									asAccumDesc[(psFFGEOGenDesc->uLineTexGenAccumulateUnitEnables & (1 << uUnit)) ? 1 : 0]);
			}
			uUnit++;
			uEnables >>= 1;
		}

	}
	
	/* Texture matrix emabled */
	uEnables = psFFGEOGenDesc->uEnabledTextureMatrices;
	if(uEnables)
	{
		IMG_UINT32 uUnit = 0;
		fprintf(psAsmFile, "Texture matrix enables: "); 
		while(uEnables)
		{
			if(uEnables & 1)
			{
				fprintf(psAsmFile, "tc%d ", uUnit);
			}
			uUnit++;
			uEnables >>= 1;
		}

		fprintf(psAsmFile,"\n");

	}

	fprintf(psAsmFile, "\nInput attributes  : ");

	for(i = 0; i < FFGEO_ATTRIB_TYPES; i++)
	{
		if(psFFGEOGenDesc->uInputAttribMask & (1 << i))
		{
			fprintf(psAsmFile, "%s ", AttribName[i]);
		}
	}

	fprintf(psAsmFile, "\nOutput attributes : ");

	for(i = 0; i < FFGEO_ATTRIB_TYPES; i++)
	{
		if(psFFGEOGenDesc->uOutputAttribMask & (1 << i))
		{
			fprintf(psAsmFile, "%s ", AttribName[i]);
		}
	}

	fprintf(psAsmFile, "\nTexture dimensions: ");

	for(i = 0; i < FFGEO_MAX_NUM_TEXCOORD_UNITS; i++)
	{
		fprintf(psAsmFile, "%d ", psFFGEOGenDesc->auTexCoordDims[i]);
	}
	
	fprintf(psAsmFile,"\n");
	
	fprintf(psAsmFile, "*/\n");

	fclose(psAsmFile);
}

#endif /* #if defined(OGL_CARNAV_EXTS) || defined(OGL_LINESTIPPLE) */


#endif

/******************************************************************************
 End of file (source.c) 
******************************************************************************/

