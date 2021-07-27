/******************************************************************************
 * Name         : pdsasm.c
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
 * $Log: pdsdisasm.c $
 *****************************************************************************/

#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "img_types.h"
#include "sgxdefs.h"

#include "pdsasm.h"

#define	PDS_NUM_WORDS_PER_DWORD		2
#define	PDS_NUM_DWORDS_PER_QWORD	2

static IMG_VOID PDSDisassembleCC			(IMG_UINT32 dwInstruction, IMG_PCHAR pchInstruction);
static IMG_VOID PDSDisassembleMOVS			(IMG_UINT32 dwInstruction, IMG_PCHAR pchInstruction);
static IMG_VOID PDSDisassembleMOV16			(IMG_UINT32 dwInstruction, IMG_PCHAR pchInstruction);
static IMG_VOID PDSDisassembleMOV32			(IMG_UINT32 dwInstruction, IMG_PCHAR pchInstruction);
static IMG_VOID PDSDisassembleMOV64			(IMG_UINT32 dwInstruction, IMG_PCHAR pchInstruction);
static IMG_VOID PDSDisassembleMOV128		(IMG_UINT32 dwInstruction, IMG_PCHAR pchInstruction);
static IMG_VOID PDSDisassembleArith			(IMG_UINT32 dwInstruction, IMG_PCHAR pchInstruction);
static IMG_VOID PDSDisassembleMUL			(IMG_UINT32 dwInstruction, IMG_PCHAR pchInstruction);
static IMG_VOID PDSDisassembleABS			(IMG_UINT32 dwInstruction, IMG_PCHAR pchInstruction);
static IMG_VOID PDSDisassembleTest			(IMG_UINT32 dwInstruction, IMG_PCHAR pchInstruction);
static IMG_VOID PDSDisassembleFlow			(IMG_UINT32 dwInstruction, IMG_PCHAR pchInstruction, IMG_PUINT32 pdwMaxJumpDest);
static IMG_VOID PDSDisassembleLogic			(IMG_UINT32 dwInstruction, IMG_PCHAR pchInstruction);
static IMG_VOID PDSDisassembleShift			(IMG_UINT32 dwInstruction, IMG_PCHAR pchInstruction);
#if defined(SGX_FEATURE_PDS_LOAD_STORE)
static IMG_VOID PDSDisassembleLOADSTORE		(IMG_UINT32 dwInstruction, IMG_UINT32 uType, IMG_PCHAR pchInstruction);
static IMG_VOID PDSDisassembleFENCE			(IMG_UINT32 dwInstruction, IMG_PCHAR pchInstruction);
#endif /* defined(SGX_FEATURE_PDS_LOAD_STORE) */

/*****************************************************************************
 Function Name	: PDSDisassembleInstruction
 Inputs			: dwInstruction		- the encoded instruction
 Outputs		: none
 Returns		: none
 Description	: Disassembles the instruction
*****************************************************************************/
IMG_INTERNAL
IMG_BOOL PDSDisassembleInstruction	(IMG_CHAR* achInstruction, IMG_UINT32	dwInstruction, IMG_PUINT32 pdwMaxJumpDest)
{
	
	IMG_UINT32	dwInst;
	IMG_UINT32	dwType;
	IMG_CHAR*	pchInstruction;

	dwInst	= (dwInstruction & ~EURASIA_PDS_INST_CLRMSK)	>> EURASIA_PDS_INST_SHIFT;
	dwType	= (dwInstruction & ~EURASIA_PDS_TYPE_CLRMSK)	>> EURASIA_PDS_TYPE_SHIFT;

	PDSDisassembleCC(dwInstruction, achInstruction);
	pchInstruction = achInstruction + strlen(achInstruction);

	switch (dwInst)
	{
		default:
		case EURASIA_PDS_INST_MOV:
		{
			switch (dwType)
			{
				case EURASIA_PDS_TYPE_MOVS:
				case EURASIA_PDS_TYPE_MOVSA:
				{
					PDSDisassembleMOVS(dwInstruction, pchInstruction);
					break;
				}
				case EURASIA_PDS_TYPE_MOV16:
				{
					PDSDisassembleMOV16(dwInstruction, pchInstruction);
					break;
				}
				case EURASIA_PDS_TYPE_MOV32:
				{
					PDSDisassembleMOV32(dwInstruction, pchInstruction);
					break;
				}
				case EURASIA_PDS_TYPE_MOV64:
				{
					PDSDisassembleMOV64(dwInstruction, pchInstruction);
					break;
				}
				case EURASIA_PDS_TYPE_MOV128:
				{
					PDSDisassembleMOV128(dwInstruction, pchInstruction);
					break;
				}
				#if defined(SGX_FEATURE_PDS_LOAD_STORE)
				case EURASIA_PDS_TYPE_LOAD:
				case EURASIA_PDS_TYPE_STORE:
				{
					PDSDisassembleLOADSTORE(dwInstruction, dwType, pchInstruction);
					break;
				}
				#endif /* defined(SGX_FEATURE_PDS_LOAD_STORE) */
				default:
				{
					sprintf(pchInstruction, "reserved_movtype(%d)", dwType);
					break;
				}
			}
			break;
		}
		case EURASIA_PDS_INST_ARITH:
		{
			switch (dwType)
			{
				case EURASIA_PDS_TYPE_ADD:
				case EURASIA_PDS_TYPE_SUB:
				case EURASIA_PDS_TYPE_ADC:
				case EURASIA_PDS_TYPE_SBC:
				{
					PDSDisassembleArith(dwInstruction, pchInstruction);
					break;
				}
				case EURASIA_PDS_TYPE_MUL:
				{
					PDSDisassembleMUL(dwInstruction, pchInstruction);
					break;
				}
				case EURASIA_PDS_TYPE_ABS:
				{
					PDSDisassembleABS(dwInstruction, pchInstruction);
					break;
				}
				#if defined(SGX_FEATURE_PDS_LOAD_STORE)
				case EURASIA_PDS_TYPE_DATAFENCE:
				{
					PDSDisassembleFENCE(dwInstruction, pchInstruction);
					break;
				}
				#endif /* defined(SGX_FEATURE_PDS_LOAD_STORE) */
				default:
				{
					sprintf(pchInstruction, "reserved_arithtype(%d)", dwType);
					break;
				}
			}
			break;
		}
		case EURASIA_PDS_INST_FLOW:
		{
			switch (dwType)
			{
				case EURASIA_PDS_TYPE_TSTZ:
				case EURASIA_PDS_TYPE_TSTN:
				{
					PDSDisassembleTest(dwInstruction, pchInstruction);
					break;
				}
				case EURASIA_PDS_TYPE_BRA:
				case EURASIA_PDS_TYPE_CALL:
				case EURASIA_PDS_TYPE_RTN:
				case EURASIA_PDS_TYPE_HALT:
				case EURASIA_PDS_TYPE_NOP:
				{
					PDSDisassembleFlow(dwInstruction, pchInstruction, pdwMaxJumpDest);
					break;
				}
				case EURASIA_PDS_TYPE_ALUM:
				{
					IMG_UINT32	dwMode = (dwInstruction & ~EURASIA_PDS_FLOW_ALUM_MODE_CLRMSK) >> EURASIA_PDS_FLOW_ALUM_MODE_SHIFT;
					static const IMG_PCHAR pszMode[] = {"unsigned", "signed"};

					sprintf(pchInstruction, "alum %s", pszMode[dwMode]);
					break;
				}
				default:
				{
					sprintf(pchInstruction, "reserved_flowtype(%d)", dwType);
					break;
				}
			}
			break;
		}
		case EURASIA_PDS_INST_LOGIC:
		{
			switch (dwType)
			{
				case EURASIA_PDS_TYPE_OR:
				case EURASIA_PDS_TYPE_AND:
				case EURASIA_PDS_TYPE_XOR:
				case EURASIA_PDS_TYPE_NOT:
				case EURASIA_PDS_TYPE_NOR:
				case EURASIA_PDS_TYPE_NAND:
				{
					PDSDisassembleLogic(dwInstruction, pchInstruction);
					break;
				}
				case EURASIA_PDS_TYPE_SHL:
				case EURASIA_PDS_TYPE_SHR:
				{
					PDSDisassembleShift(dwInstruction, pchInstruction);
					break;
				}
				default:
				{
					sprintf(pchInstruction, "reserved_logictype(%d)", dwType);
					break;
				}
			}
			break;
		}
	}

	if ((dwInst == EURASIA_PDS_INST_FLOW) && (dwType == EURASIA_PDS_TYPE_HALT))
	{
		return IMG_FALSE;
	}
	else
	{
		return IMG_TRUE;
	}
}

/*****************************************************************************
 Function Name	: PDSSupportsConditionCode
 Inputs			: dwInstruction		- the encoded instruction
 Outputs		: none
 Returns		: TRUE or FALSE.
 Description	: Checks for an instruction which supports a condition code.
*****************************************************************************/
static
IMG_BOOL PDSSupportsConditionCode(IMG_UINT32 dwInst, IMG_UINT32 dwType)
{
	#if defined(SGX_FEATURE_PDS_LOAD_STORE)
	/*
		No condition code on LOAD or STORE instructions.
	*/
	if (
			dwInst == EURASIA_PDS_INST_MOV && 
			(
				dwType == EURASIA_PDS_TYPE_LOAD || 
				dwType == EURASIA_PDS_TYPE_STORE
			)
	   )
	{
		return IMG_FALSE;
	}
	/*
		No condition code on FENCE instructions.
	*/
	if (
			dwInst == EURASIA_PDS_INST_ARITH &&
			dwType == EURASIA_PDS_TYPE_DATAFENCE
		)
	{
		return IMG_FALSE;
	}
	#endif /* defined(SGX_FEATURE_PDS_LOAD_STORE) */
	if (
			(dwInst == EURASIA_PDS_INST_FLOW) && 
			(dwType == EURASIA_PDS_TYPE_NOP)
	   )
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

/*****************************************************************************
 Function Name	: PDSDisassembleCC
 Inputs			: dwInstruction		- the encoded instruction
 Outputs		: pchInstruction	- the disassembled instruction
 Returns		: none
 Description	: Disassembles the instruction condition code
*****************************************************************************/
static IMG_VOID PDSDisassembleCC	(IMG_UINT32	dwInstruction,
								 IMG_PCHAR	pchInstruction)
{

	IMG_BOOL bNegate;
	static const IMG_PCHAR apchCC[] =
	{
		"(p0) ",
		"(p1) ",
		"(p2) ",
		"(if0) ",
		"(if1) ",
		"(aluz) ",
		"(alun) ",
		""
	};

	IMG_UINT32	dwInst;
	IMG_UINT32	dwType;
	IMG_UINT32	dwCC;

	dwInst		= (dwInstruction & ~EURASIA_PDS_INST_CLRMSK)	>> EURASIA_PDS_INST_SHIFT;
	dwType		= (dwInstruction & ~EURASIA_PDS_TYPE_CLRMSK)	>> EURASIA_PDS_TYPE_SHIFT;

	if (!PDSSupportsConditionCode(dwInst, dwType))
	{
		dwCC	= EURASIA_PDS_CC_ALWAYS >> EURASIA_PDS_CC_SHIFT;
	}
	else
	{
		dwCC	= (dwInstruction & ~EURASIA_PDS_CC_CLRMSK) >> EURASIA_PDS_CC_SHIFT;
	}

#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
	if((dwInst == EURASIA_PDS_INST_FLOW) && 
		((dwType == EURASIA_PDS_TYPE_BRA) || (dwType == EURASIA_PDS_TYPE_CALL) || (dwType == EURASIA_PDS_TYPE_RTN)))
	{
		bNegate = (dwInstruction & EURASIA_PDS_FLOW_CC_NEGATE) ? IMG_TRUE : IMG_FALSE;
	}
	else
#endif
	{
		bNegate = IMG_FALSE;
	}

	sprintf(pchInstruction, "%s%s", bNegate ? "!" : "", apchCC[dwCC]);
}

#if defined(SGX_FEATURE_PDS_LOAD_STORE)
/*****************************************************************************
 Function Name	: PDSDisassembleFENCE
 Inputs			: dwInstruction		- the encoded instruction
 Outputs		: pchInstruction	- the disassembled instruction
 Returns		: none
 Description	: Disassembles the FENCE instruction.
*****************************************************************************/
static IMG_VOID PDSDisassembleFENCE	(IMG_UINT32	uInstruction, IMG_PCHAR	pchInstruction)
{
	IMG_UINT32	uMode;

	uMode = (uInstruction & ~EURASIA_PDS_DATAFENCE_MODE_CLRMSK) >> EURASIA_PDS_DATAFENCE_MODE_SHIFT;
	switch (uMode)
	{
		case EURASIA_PDS_DATAFENCE_MODE_READ:
		{
			sprintf(pchInstruction, "rfence");
			break;
		}
		case EURASIA_PDS_DATAFENCE_MODE_READWRITE:
		{
			sprintf(pchInstruction, "rwfence");
			break;
		}
	}
}

/*****************************************************************************
 Function Name	: PDSDisassembleLOADSTORE
 Inputs			: dwInstruction		- the encoded instruction
 Outputs		: pchInstruction	- the disassembled instruction
 Returns		: none
 Description	: Disassembles the LOAD or STORE instructions
*****************************************************************************/
static IMG_VOID PDSDisassembleLOADSTORE	(IMG_UINT32	uInstruction, IMG_UINT32 uType, IMG_PCHAR	pchInstruction)
{
	IMG_UINT32	uSize;
	IMG_UINT32	uRegAddr;
	IMG_UINT32	uRegAddrSel;
	IMG_UINT32	uMemAddr;
	IMG_CHAR	achReg[20];

	uSize = (uInstruction & ~EURASIA_PDS_LOADSTORE_SIZE_CLRMSK) >> EURASIA_PDS_LOADSTORE_SIZE_SHIFT;
	if (uSize == 0)
	{
		uSize = EURASIA_PDS_LOADSTORE_SIZE_MAX;
	}

	uRegAddr = (uInstruction & ~EURASIA_PDS_LOADSTORE_REGADDR_CLRMSK) >> EURASIA_PDS_LOADSTORE_REGADDR_SHIFT;
	uRegAddrSel = (uInstruction & ~EURASIA_PDS_LOADSTORE_REGADDRSEL_CLRMSK) >> EURASIA_PDS_LOADSTORE_REGADDRSEL_SHIFT;
	uMemAddr = (uInstruction & ~EURASIA_PDS_LOADSTORE_MEMADDR_CLRMSK) >> EURASIA_PDS_LOADSTORE_MEMADDR_SHIFT;
	uMemAddr <<= EURASIA_PDS_LOADSTORE_MEMADDR_ALIGNSHIFT;

	#if defined(FIX_HW_BRN_27652)
	if (uType == EURASIA_PDS_TYPE_STORE)
	{
		static const IMG_UINT32 auDecodeRegAddr[16] = 
		{
			(IMG_UINT32)-1,
			(IMG_UINT32)-1,
			(IMG_UINT32)-1,
			(IMG_UINT32)-1,
			(IMG_UINT32)-1,
			(IMG_UINT32)-1,
			(IMG_UINT32)-1,
			(IMG_UINT32)-1,
			(IMG_UINT32)-1,
			(IMG_UINT32)-1,
			(IMG_UINT32)-1,
			(IMG_UINT32)-1,
			48,
			51,
			52,
			55,
		};

		uRegAddr = auDecodeRegAddr[uRegAddr];
	}
	else
	#endif /* defined(FIX_HW_BRN_27652) */
	{
		uRegAddr += EURASIA_PDS_DATASTORE_TEMPSTART;
	}

	if (uRegAddr == (IMG_UINT32)-1)
	{
		sprintf(achReg, "invalid_regaddr");
	}
	else if (uSize > 1)
	{
		sprintf(achReg, "ds%u[%u-%u]", uRegAddrSel, uRegAddr, uRegAddr + uSize - 1);
	}
	else
	{
		sprintf(achReg, "ds%u[%u]", uRegAddrSel, uRegAddr);
	}

	sprintf(pchInstruction, "%s 0x%.8X, %s", 
			(uType == EURASIA_PDS_TYPE_LOAD) ? "load" : "store",
			uMemAddr,
			achReg);
}
#endif /* defined(SGX_FEATURE_PDS_LOAD_STORE) */

static const IMG_PCHAR g_apchMOVSDestType[] =
{
	"mr",
	"slc",
	"douti",
	"doutd",
	"doutt",
	"doutu",
	"douta",
	"tim",
#if defined(SGX_FEATURE_PDS_LOAD_STORE)
	"doutc",
	"douts",
#else /* defined(SGX_FEATURE_PDS_LOAD_STORE) */
	"reserved (=8)",
	"reserved (=9)",
	"reserved (=10)",
#endif /* defined(SGX_FEATURE_PDS_LOAD_STORE) */
#if defined(SGX543) || defined(SGX544) || defined(SGX554)
	"douto",
#else /* defined(SGX543) || defined(SGX544) || defined(SGX554) */
	"reserved (=11)",
#endif /* defined(SGX543) || defined(SGX544) || defined(SGX554) */
	"reserved (=12)",
	"reserved (=13)",
	"reserved (=14)",
	"reserved (=15)",
};

static IMG_VOID PDSDisassembleMOVS_PrintRegSource(IMG_PCHAR pchSrc, IMG_UINT32 uSrc, IMG_UINT32 uSwiz)
/*****************************************************************************
 Function Name	: PDSDisassembleMOVS_PrintRegSource
 Inputs			: uSrc				- 64-bit source register number.
				  uSwiz				- Source argument to disassemble.
 Outputs		: pchSrc			- the disassembled source
 Returns		: none
 Description	: Disassembles a MOVS instruction register source
*****************************************************************************/
{
	static const IMG_PCHAR apchSrcReg[] =
	{
		"ir0",
		"ir1",
		"tim",
		#if defined(SGX_FEATURE_PDS_EXTENDED_INPUT_REGISTERS)
		"ir2",
		#endif /* defined(SGX_FEATURE_PDS_EXTENDED_INPUT_REGISTERS) */
	};
	static const IMG_PCHAR apchSwiz[] =
	{
		".l",
		".h",
		".l",
		".h"
	};

	if (uSrc < (sizeof(apchSrcReg) / sizeof(apchSrcReg[0])))
	{
		strcpy(pchSrc, apchSrcReg[uSrc]);
	}
	else
	{
		sprintf(pchSrc, "reserved_reg(=%d)", uSrc);
	}
	strcat(pchSrc, apchSwiz[uSwiz]);
}

static IMG_VOID PDSDisassembleMOVS_Source(IMG_PCHAR		pchSrc, 
										  IMG_UINT32	uSrc1, 
										  IMG_UINT32	uSrc1Sel, 
										  IMG_UINT32	uSrc2, 
										  IMG_UINT32	uSrc2Sel, 
										  IMG_UINT32	uSwiz)
/*****************************************************************************
 Function Name	: PDSDisassembleMOVS_Source
 Inputs			: uSrc1				- First 64-bit source
				  uSrc1Sel
				  uSrc2				- Second 64-bit source
				  uSrc2Sel
				  uSwiz				- Source argument to disassemble.
 Outputs		: pchSrc			- the disassembled source
 Returns		: none
 Description	: Disassembles a MOVS instruction source
*****************************************************************************/
{
	if (uSwiz < 2)
	{
		if (uSrc1Sel == EURASIA_PDS_MOVS_SRC1SEL_REG)
		{
			PDSDisassembleMOVS_PrintRegSource(pchSrc, uSrc1, uSwiz);
		}
		else
		{
			sprintf(pchSrc, "ds0[%u]", uSrc1 * PDS_NUM_DWORDS_PER_QWORD + (uSwiz & 1));
		}
	}
	else
	{
		#if ! defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
			PVR_UNREFERENCED_PARAMETER(uSrc2Sel);
		#else
		if (uSrc2Sel == EURASIA_PDS_MOVS_SRC2SEL_REG)
		{
			PDSDisassembleMOVS_PrintRegSource(pchSrc, uSrc2, uSwiz);
		}
		else
		#endif /* defined(SGX_FEATURE_PDS_EXTENDED_SOURCES) */
		{
			sprintf(pchSrc, "ds1[%u]", uSrc2 * PDS_NUM_DWORDS_PER_QWORD + (uSwiz & 1));
		}
	}
}

/*****************************************************************************
 Function Name	: PDSDisassembleMOVS
 Inputs			: dwInstruction		- the encoded instruction
 Outputs		: pchInstruction	- the disassembled instruction
 Returns		: none
 Description	: Disassembles the MOVS instruction
*****************************************************************************/
static IMG_VOID PDSDisassembleMOVS	(IMG_UINT32	dwInstruction, IMG_PCHAR	pchInstruction)
{
	static const IMG_PCHAR apchType[] =
	{
		"movs",
		"mov16",
		"mov32",
		"mov64",
		"mov128",
		"movsa",
		"",
		""
	};
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
	static const IMG_PCHAR apchFConv[] =
	{
		"",
		"reserved",
		"f16",
		"f32"
	};
	IMG_BOOL	bMinPack = IMG_FALSE;
	IMG_BOOL	dwFConv = 0;
	IMG_CHAR	achTagSize[15];
	IMG_CHAR	achItrSize[15];
#endif

	IMG_UINT32	dwType;
	IMG_UINT32	dwDest;
	IMG_UINT32	dwSrc1Sel;
	IMG_UINT32	dwSrc2Sel;
	IMG_UINT32	dwSrc1;
	IMG_UINT32	dwSrc2;
	IMG_UINT32	dwSwiz0;
	IMG_UINT32	dwSwiz1;
	IMG_UINT32	dwSwiz2;
	IMG_UINT32	dwSwiz3;
	IMG_CHAR	achSrc0[40];
	IMG_CHAR	achSrc1[40];
	IMG_CHAR	achSrc2[40];
	IMG_CHAR	achSrc3[40];

	dwType		= (dwInstruction & ~EURASIA_PDS_TYPE_CLRMSK)			>> EURASIA_PDS_TYPE_SHIFT;
	dwDest		= (dwInstruction & ~EURASIA_PDS_MOVS_DEST_CLRMSK)		>> EURASIA_PDS_MOVS_DEST_SHIFT;
	dwSrc1Sel	= (dwInstruction & ~EURASIA_PDS_MOVS_SRC1SEL_CLRMSK)	>> EURASIA_PDS_MOVS_SRC1SEL_SHIFT;
	dwSrc2Sel	= (dwInstruction & ~EURASIA_PDS_MOVS_SRC2SEL_CLRMSK)	>> EURASIA_PDS_MOVS_SRC2SEL_SHIFT;
	dwSrc1		= (dwInstruction & ~EURASIA_PDS_MOVS_SRC1_CLRMSK)		>> EURASIA_PDS_MOVS_SRC1_SHIFT;
	dwSrc2		= (dwInstruction & ~EURASIA_PDS_MOVS_SRC2_CLRMSK)		>> EURASIA_PDS_MOVS_SRC2_SHIFT;

#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
	if(dwDest == EURASIA_PDS_MOVS_DEST_DOUTT)
	{
		IMG_BOOL bSwiz  = (dwInstruction & EURASIA_PDS_MOVS_DOUTT_SWIZ) ? IMG_TRUE : IMG_FALSE;

		bMinPack = (dwInstruction & EURASIA_PDS_MOVS_DOUTT_MINPACK) ? IMG_TRUE : IMG_FALSE;
		dwFConv = (dwInstruction & ~EURASIA_PDS_MOVS_DOUTT_FCONV_CLRMSK)	>> EURASIA_PDS_MOVS_DOUTT_FCONV_SHIFT;
		
		if(bSwiz)
		{
			dwSwiz0		= EURASIA_PDS_MOVS_SWIZ_SRC2L;
			dwSwiz1		= EURASIA_PDS_MOVS_SWIZ_SRC2H;
			dwSwiz2		= EURASIA_PDS_MOVS_SWIZ_SRC1L;
			dwSwiz3		= EURASIA_PDS_MOVS_SWIZ_SRC1H;
		}
		else
		{
			dwSwiz0		= EURASIA_PDS_MOVS_SWIZ_SRC1L;
			dwSwiz1		= EURASIA_PDS_MOVS_SWIZ_SRC1H;
			dwSwiz2		= EURASIA_PDS_MOVS_SWIZ_SRC2L;
			dwSwiz3		= EURASIA_PDS_MOVS_SWIZ_SRC2H;
		}
	}
	else if(dwDest == EURASIA_PDS_MOVS_DEST_DOUTI)
	{
		IMG_UINT32	ui32TagSize, ui32ItrSize;

		ui32TagSize = 1 + ((dwInstruction & ~EURASIA_PDS_MOVS_DOUTI_TAGSIZE_CLRMSK) >> EURASIA_PDS_MOVS_DOUTI_TAGSIZE_SHIFT);
		ui32ItrSize = 1 + ((dwInstruction & ~EURASIA_PDS_MOVS_DOUTI_ITRSIZE_CLRMSK) >> EURASIA_PDS_MOVS_DOUTI_ITRSIZE_SHIFT);
		
		sprintf(achTagSize, "TagSize = %u", ui32TagSize);
		sprintf(achItrSize, "ItrSize = %u", ui32ItrSize);

		dwSwiz0		= (dwInstruction & ~EURASIA_PDS_MOVS_SWIZ0_CLRMSK)		>> EURASIA_PDS_MOVS_SWIZ0_SHIFT;
		dwSwiz1		= (dwInstruction & ~EURASIA_PDS_MOVS_SWIZ1_CLRMSK)		>> EURASIA_PDS_MOVS_SWIZ1_SHIFT;
		dwSwiz2		= EURASIA_PDS_MOVS_SWIZ_SRC2L;
		dwSwiz3		= EURASIA_PDS_MOVS_SWIZ_SRC2H;
	}
	else
#endif
	{
		dwSwiz0		= (dwInstruction & ~EURASIA_PDS_MOVS_SWIZ0_CLRMSK)		>> EURASIA_PDS_MOVS_SWIZ0_SHIFT;
		dwSwiz1		= (dwInstruction & ~EURASIA_PDS_MOVS_SWIZ1_CLRMSK)		>> EURASIA_PDS_MOVS_SWIZ1_SHIFT;
		dwSwiz2		= (dwInstruction & ~EURASIA_PDS_MOVS_SWIZ2_CLRMSK)		>> EURASIA_PDS_MOVS_SWIZ2_SHIFT;
		dwSwiz3		= (dwInstruction & ~EURASIA_PDS_MOVS_SWIZ3_CLRMSK)		>> EURASIA_PDS_MOVS_SWIZ3_SHIFT;
	}

	PDSDisassembleMOVS_Source(achSrc0, dwSrc1, dwSrc1Sel, dwSrc2, dwSrc2Sel, dwSwiz0);
	PDSDisassembleMOVS_Source(achSrc1, dwSrc1, dwSrc1Sel, dwSrc2, dwSrc2Sel, dwSwiz1);
	PDSDisassembleMOVS_Source(achSrc2, dwSrc1, dwSrc1Sel, dwSrc2, dwSrc2Sel, dwSwiz2);
	PDSDisassembleMOVS_Source(achSrc3, dwSrc1, dwSrc1Sel, dwSrc2, dwSrc2Sel, dwSwiz3);

#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
	if(dwDest == EURASIA_PDS_MOVS_DEST_DOUTT)
	{
		sprintf(pchInstruction,
				"%s %s, %s, %s, %s, %s, %s, %s",
				apchType			[dwType],
				g_apchMOVSDestType	[dwDest],
				achSrc0,
				achSrc1,
				achSrc2,
				achSrc3,
				bMinPack ? "minpack" : "",
				apchFConv			[dwFConv]);
	}
	else if(dwDest == EURASIA_PDS_MOVS_DEST_DOUTI)
	{
		sprintf(pchInstruction,
				"%s %s, %s, %s, %s, %s, %s, %s",
				apchType			[dwType],
				g_apchMOVSDestType	[dwDest],
				achSrc0,
				achSrc1,
				achSrc2,
				achSrc3,
				achTagSize,
				achItrSize);
	}
	else
#endif
	{
		sprintf(pchInstruction,
				"%s %s, %s, %s, %s, %s",
				apchType			[dwType],
				g_apchMOVSDestType	[dwDest],
				achSrc0,
				achSrc1,
				achSrc2,
				achSrc3);
	}
}

/*****************************************************************************
 Function Name	: PDSDisassembleMOV16
 Inputs			: dwInstruction		- the encoded instruction
 Outputs		: pchInstruction	- the disassembled instruction
 Returns		: none
 Description	: Disassembles the MOV16 instruction
*****************************************************************************/
static IMG_VOID PDSDisassembleMOV16	(IMG_UINT32	dwInstruction,
								 IMG_PCHAR	pchInstruction)
{
	static const IMG_PCHAR apchType[] =
	{
		"movs",
		"mov16",
		"mov32",
		"mov64",
		"mov128",
		"movsa",
		"",
		""
	};
	static const IMG_PCHAR apchDestSel[] =
	{
		"ds0",
		"ds1"
	};
	static const IMG_PCHAR apchSrcSel[] =
	{
		"ds0",
		"ds1",
		"reg",
		"reserved_srcsel(=3)"
	};
	static const IMG_PCHAR apchSrcReg[] =
	{
		"ir0",
		"ir1",
		"pc",
		"tim",
		#if defined(SGX_FEATURE_PDS_EXTENDED_INPUT_REGISTERS)
		"ir2",
		#endif /* defined(SGX_FEATURE_PDS_EXTENDED_INPUT_REGISTERS) */
	};
	static const IMG_PCHAR apchSrcWord[] =
	{
		".l",
		".h"
	};

	IMG_UINT32	dwType;
	IMG_UINT32	dwDestSel;
	IMG_UINT32	dwDest;
	IMG_UINT32	dwSrcSel;
	IMG_UINT32	dwSrc;
	IMG_CHAR	achSrc	[40];

	dwType		= (dwInstruction & ~EURASIA_PDS_TYPE_CLRMSK)			>> EURASIA_PDS_TYPE_SHIFT;
	dwDestSel	= (dwInstruction & ~EURASIA_PDS_MOV16_DESTSEL_CLRMSK)	>> EURASIA_PDS_MOV16_DESTSEL_SHIFT;
	dwDest		= (dwInstruction & ~EURASIA_PDS_MOV16_DEST_CLRMSK)		>> EURASIA_PDS_MOV16_DEST_SHIFT;
	dwSrcSel	= (dwInstruction & ~EURASIA_PDS_MOV16_SRCSEL_CLRMSK)	>> EURASIA_PDS_MOV16_SRCSEL_SHIFT;
	dwSrc		= (dwInstruction & ~EURASIA_PDS_MOV16_SRC_CLRMSK)		>> EURASIA_PDS_MOV16_SRC_SHIFT;

	if (dwSrcSel == EURASIA_PDS_MOV16_SRCSEL_REG)
	{
		IMG_UINT32	uSrcReg = dwSrc >> 1;
		
		if (uSrcReg < (sizeof(apchSrcReg) / sizeof(apchSrcReg[0])))
		{
			sprintf(achSrc, "%s", apchSrcReg[uSrcReg]);
		}
		else
		{
			sprintf(achSrc, "reserved_srcreg(=%d)", uSrcReg);
		}
	}
	else
	{
		sprintf(achSrc, "%s[%u]", apchSrcSel[dwSrcSel], (dwSrc >> 1));
	}
	strcat(achSrc, apchSrcWord[dwSrc & 1]);

	sprintf(pchInstruction,
			 "%s %s[%u], %s",
			 apchType		[dwType],
			 apchDestSel	[dwDestSel],
			 dwDest,
			 achSrc);
}

/*****************************************************************************
 Function Name	: PDSDisassembleMOV32
 Inputs			: dwInstruction		- the encoded instruction
 Outputs		: pchInstruction	- the disassembled instruction
 Returns		: none
 Description	: Disassembles the MOV32 instruction
*****************************************************************************/
static IMG_VOID PDSDisassembleMOV32	(IMG_UINT32	dwInstruction,
								 IMG_PCHAR	pchInstruction)
{
	static const IMG_PCHAR apchType[] =
	{
		"movs",
		"mov16",
		"mov32",
		"mov64",
		"mov128",
		"movsa",
		"",
		""
	};
	static const IMG_PCHAR apchDestSel[] =
	{
		"ds0",
		"ds1"
	};
	static const IMG_PCHAR apchSrcSel[] =
	{
		"ds0",
		"ds1",
		"reg",
		"reserved_srcsel(=3)"
	};
	static const IMG_PCHAR apchSrcReg[] =
	{
		"ir0",
		"ir1",
		"pc",
		"tim",
		#if defined(SGX_FEATURE_PDS_EXTENDED_INPUT_REGISTERS)
		"ir2",
		#endif /* defined(SGX_FEATURE_PDS_EXTENDED_INPUT_REGISTERS) */
	};

	IMG_UINT32	dwType;
	IMG_UINT32	dwDestSel;
	IMG_UINT32	dwDest;
	IMG_UINT32	dwSrcSel;
	IMG_UINT32	dwSrc;
	IMG_CHAR	achSrc	[40];

	dwType		= (dwInstruction & ~EURASIA_PDS_TYPE_CLRMSK)			>> EURASIA_PDS_TYPE_SHIFT;
	dwDestSel	= (dwInstruction & ~EURASIA_PDS_MOV32_DESTSEL_CLRMSK)	>> EURASIA_PDS_MOV32_DESTSEL_SHIFT;
	dwDest		= (dwInstruction & ~EURASIA_PDS_MOV32_DEST_CLRMSK)		>> EURASIA_PDS_MOV32_DEST_SHIFT;
	dwSrcSel	= (dwInstruction & ~EURASIA_PDS_MOV32_SRCSEL_CLRMSK)	>> EURASIA_PDS_MOV32_SRCSEL_SHIFT;
	dwSrc		= (dwInstruction & ~EURASIA_PDS_MOV32_SRC_CLRMSK)		>> EURASIA_PDS_MOV32_SRC_SHIFT;

	if (dwSrcSel == EURASIA_PDS_MOV32_SRCSEL_REG)
	{
		if (dwSrc < (sizeof(apchSrcReg) / sizeof(apchSrcReg[0])))
		{
			sprintf(achSrc, "%s", apchSrcReg[dwSrc]);
		}
		else
		{
			sprintf(achSrc, "reserved_srcreg(=%d)", dwSrc);
		}
	}
	else
	{
		sprintf(achSrc, "%s[%u]", apchSrcSel[dwSrcSel], dwSrc);
	}

	sprintf(pchInstruction,
			 "%s %s[%u], %s",
			 apchType		[dwType],
			 apchDestSel	[dwDestSel],
			 dwDest,
			 achSrc);
}

/*****************************************************************************
 Function Name	: PDSDisassembleMOV64
 Inputs			: dwInstruction		- the encoded instruction
 Outputs		: pchInstruction	- the disassembled instruction
 Returns		: none
 Description	: Disassembles the MOV64 instruction
*****************************************************************************/
static IMG_VOID PDSDisassembleMOV64	(IMG_UINT32	dwInstruction,
								 IMG_PCHAR	pchInstruction)
{
	static const IMG_PCHAR apchType[] =
	{
		"movs",
		"mov16",
		"mov32",
		"mov64",
		"mov128",
		"movsa",
		"",
		""
	};
	static const IMG_PCHAR apchDestSel[] =
	{
		"ds0",
		"ds1"
	};
	static const IMG_PCHAR apchSrcSel[] =
	{
		"ds0",
		"ds1",
		"reg",
		"reserved_srcsel(=3)"
	};
	static const IMG_PCHAR apchSrcReg[] =
	{
		"mr01",
		"mr23"
	};

	IMG_UINT32	dwType;
	IMG_UINT32	dwDestSel;
	IMG_UINT32	dwDest;
	IMG_UINT32	dwSrcSel;
	IMG_UINT32	dwSrc;
	IMG_CHAR	achSrc	[40];

	dwType		= (dwInstruction & ~EURASIA_PDS_TYPE_CLRMSK)			>> EURASIA_PDS_TYPE_SHIFT;
	dwDestSel	= (dwInstruction & ~EURASIA_PDS_MOV64_DESTSEL_CLRMSK)	>> EURASIA_PDS_MOV64_DESTSEL_SHIFT;
	dwDest		= (dwInstruction & ~EURASIA_PDS_MOV64_DEST_CLRMSK)		>> EURASIA_PDS_MOV64_DEST_SHIFT;
	dwSrcSel	= (dwInstruction & ~EURASIA_PDS_MOV64_SRCSEL_CLRMSK)	>> EURASIA_PDS_MOV64_SRCSEL_SHIFT;
	dwSrc		= (dwInstruction & ~EURASIA_PDS_MOV64_SRC_CLRMSK)		>> EURASIA_PDS_MOV64_SRC_SHIFT;

	if (dwSrcSel == EURASIA_PDS_MOV64_SRCSEL_REG)
	{
		if (dwSrc < (sizeof(apchSrcReg) / sizeof(apchSrcReg[0])))
		{
			sprintf(achSrc, "%s", apchSrcReg[dwSrc]);
		}
		else
		{
			sprintf(achSrc, "reserved_srcreg(=%d)", dwSrc);
		}
	}
	else
	{
		sprintf(achSrc, "%s[%u]", apchSrcSel[dwSrcSel], dwSrc * 2);
	}

	sprintf(pchInstruction,
			 "%s %s[%u], %s",
			 apchType		[dwType],
			 apchDestSel	[dwDestSel],
			 dwDest * 2,
			 achSrc);
}

/*****************************************************************************
 Function Name	: PDSDisassembleMOV128
 Inputs			: dwInstruction		- the encoded instruction
 Outputs		: pchInstruction	- the disassembled instruction
 Returns		: none
 Description	: Disassembles the MOV128 instruction
*****************************************************************************/
static IMG_VOID PDSDisassembleMOV128	(IMG_UINT32	dwInstruction,
									 IMG_PCHAR	pchInstruction)
{
	static const IMG_PCHAR apchType[] =
	{
		"movs",
		"mov16",
		"mov32",
		"mov64",
		"mov128",
		"movsa",
		"",
		""
	};
	static const IMG_PCHAR apchSrcReg[] =
	{
		"mr"
	};
	static const IMG_PCHAR apchSwap[] =
	{
		"",
		", swap"
	};

	IMG_UINT32	dwType;
	IMG_UINT32	dwDest;
	IMG_UINT32	dwSrcSel;
	IMG_UINT32	dwSrc;
	IMG_UINT32	dwSwap;
	IMG_CHAR	achSrc	[40];

	dwType		= (dwInstruction & ~EURASIA_PDS_TYPE_CLRMSK)			>> EURASIA_PDS_TYPE_SHIFT;
	dwDest		= (dwInstruction & ~EURASIA_PDS_MOV128_DEST_CLRMSK)		>> EURASIA_PDS_MOV128_DEST_SHIFT;
	dwSrcSel	= (dwInstruction & ~EURASIA_PDS_MOV128_SRCSEL_CLRMSK)	>> EURASIA_PDS_MOV128_SRCSEL_SHIFT;
	dwSrc		= (dwInstruction & ~EURASIA_PDS_MOV128_SRC_CLRMSK)		>> EURASIA_PDS_MOV128_SRC_SHIFT;
	dwSwap		= (dwInstruction & ~EURASIA_PDS_MOV128_SWAP_CLRMSK)		>> EURASIA_PDS_MOV128_SWAP_SHIFT;

	if (dwSrcSel == EURASIA_PDS_MOV128_SRCSEL_DS)
	{
		sprintf(achSrc, "ds[%u]", dwSrc * 2);
	}
	else
	{
		if (dwSrc < (sizeof(apchSrcReg) / sizeof(apchSrcReg[0])))
		{
			sprintf(achSrc, "%s", apchSrcReg[dwSrc]);
		}
		else
		{
			sprintf(achSrc, "reserved_srcreg(=%d)", dwSrc);
		}
	}

	sprintf(pchInstruction,
			 "%s ds[%u], %s%s",
			 apchType		[dwType],
			 dwDest * 2,
			 achSrc,
			 apchSwap		[dwSwap]);
}

/*****************************************************************************
 Function Name	: PDSDisassembleArith_RegSrc
 Inputs			: uSrc				- The source register number.
 Outputs		: pchSrc			- The disassembled source.
 Returns		: none
 Description	: Disassembles a register source to an arithmetic instruction.
*****************************************************************************/
static IMG_VOID PDSDisassembleArith_RegSrc(IMG_PCHAR pchSrc, IMG_UINT32 uSrc)
{
	static const IMG_PCHAR apchSrcReg[] =
	{
		"ir0",
		"ir1",
		"tim"
	};
	if (uSrc < (sizeof(apchSrcReg) / sizeof(apchSrcReg[0])))
	{
		sprintf(pchSrc, "%s", apchSrcReg[uSrc]);
	}
	else
	{
		sprintf(pchSrc, "reserved_srcreg(=%d)", uSrc);
	}
}

/*****************************************************************************
 Function Name	: PDSDisassembleArith
 Inputs			: dwInstruction		- the encoded instruction
 Outputs		: pchInstruction	- the disassembled instruction
 Returns		: none
 Description	: Disassembles the ADD and SUB instructions
*****************************************************************************/
static IMG_VOID PDSDisassembleArith	(IMG_UINT32	dwInstruction,
								 IMG_PCHAR	pchInstruction)
{
	static const IMG_PCHAR apchType[] =
	{
		"add",
		"sub",
		"adc",
		"sbc",
		"mul",
		"abs",
		"",
		""
	};
	static const IMG_PCHAR apchDestSel[] =
	{
		"ds0",
		"ds1"
	};

	IMG_UINT32	dwType;
	IMG_UINT32	dwDestSel;
	IMG_UINT32	dwDest;
	IMG_UINT32	dwSrc1Sel;
	IMG_UINT32	dwSrc1;
	IMG_UINT32	dwSrc2Sel;
	IMG_UINT32	dwSrc2;
	IMG_CHAR	achSrc1	[40];
	IMG_CHAR	achSrc2	[40];

	dwType		= (dwInstruction & ~EURASIA_PDS_TYPE_CLRMSK)			>> EURASIA_PDS_TYPE_SHIFT;
	dwDestSel	= (dwInstruction & ~EURASIA_PDS_ARITH_DESTSEL_CLRMSK)	>> EURASIA_PDS_ARITH_DESTSEL_SHIFT;
	dwDest		= (dwInstruction & ~EURASIA_PDS_ARITH_DEST_CLRMSK)		>> EURASIA_PDS_ARITH_DEST_SHIFT;
	dwSrc1Sel	= (dwInstruction & ~EURASIA_PDS_ARITH_SRC1SEL_CLRMSK)	>> EURASIA_PDS_ARITH_SRC1SEL_SHIFT;
	dwSrc1		= (dwInstruction & ~EURASIA_PDS_ARITH_SRC1_CLRMSK)		>> EURASIA_PDS_ARITH_SRC1_SHIFT;
	dwSrc2Sel	= (dwInstruction & ~EURASIA_PDS_ARITH_SRC2SEL_CLRMSK)	>> EURASIA_PDS_ARITH_SRC2SEL_SHIFT;
	dwSrc2		= (dwInstruction & ~EURASIA_PDS_ARITH_SRC2_CLRMSK)		>> EURASIA_PDS_ARITH_SRC2_SHIFT;

	if (dwSrc1Sel == EURASIA_PDS_ARITH_SRC1SEL_DS0)
	{
		sprintf(achSrc1, "ds0[%u]", dwSrc1);
	}
	else
	{
		PDSDisassembleArith_RegSrc(achSrc1, dwSrc1);
	}

#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
	if (dwSrc2Sel == EURASIA_PDS_ARITH_SRC2SEL_REG)
	{
		PDSDisassembleArith_RegSrc(achSrc2, dwSrc2);
	}
	else
#endif
	{
		sprintf(achSrc2, "ds1[%u]", dwSrc2);
	}

	sprintf(pchInstruction,
			 "%s %s[%u], %s, %s",
			 apchType		[dwType],
			 apchDestSel	[dwDestSel],
			 dwDest,
			 achSrc1,
			 achSrc2);
}

/*****************************************************************************
 Function Name	: PDSDisassembleMUL_RegSrc
 Inputs			: uSrc				- The source register number.
 Outputs		: pchSrc			- The disassembled source.
 Returns		: none
 Description	: Disassembles a register source to a MUL16 instruction.
*****************************************************************************/
static IMG_VOID PDSDisassembleMUL_RegSrc(IMG_PCHAR pchSrc, IMG_UINT32 uSrc)
{
	static const IMG_PCHAR apchSrc1Reg[] =
	{
		"ir0",
		"ir1",
		"tim"
	};

	if (uSrc < (sizeof(apchSrc1Reg) / sizeof(apchSrc1Reg[0])))
	{
		sprintf(pchSrc, "%s", apchSrc1Reg[uSrc]);
	}
	else
	{
		sprintf(pchSrc, "reserved_srcreg(=%d)", uSrc);
	}
}

/*****************************************************************************
 Function Name	: PDSDisassembleMUL
 Inputs			: dwInstruction		- the encoded instruction
 Outputs		: pchInstruction	- the disassembled instruction
 Returns		: none
 Description	: Disassembles the MUL instruction
*****************************************************************************/
static IMG_VOID PDSDisassembleMUL	(IMG_UINT32	dwInstruction,
								 IMG_PCHAR	pchInstruction)
{
	static const IMG_PCHAR apchType[] =
	{
		"add",
		"sub",
		"adc",
		"sbc",
		"mul",
		"abs",
		"",
		""
	};
	static const IMG_PCHAR apchDestSel[] =
	{
		"ds0",
		"ds1"
	};
	static const IMG_PCHAR apchSrcWord[] =
	{
		".l",
		".h"
	};

	IMG_UINT32	dwType;
	IMG_UINT32	dwDestSel;
	IMG_UINT32	dwDest;
	IMG_UINT32	dwSrc1Sel;
	IMG_UINT32	dwSrc1;
	IMG_UINT32	dwSrc2Sel;
	IMG_UINT32	dwSrc2;
	IMG_CHAR	achSrc1	[40];
	IMG_CHAR	achSrc2	[40];

	dwType		= (dwInstruction & ~EURASIA_PDS_TYPE_CLRMSK)			>> EURASIA_PDS_TYPE_SHIFT;
	dwDestSel	= (dwInstruction & ~EURASIA_PDS_MUL_DESTSEL_CLRMSK)		>> EURASIA_PDS_MUL_DESTSEL_SHIFT;
	dwDest		= (dwInstruction & ~EURASIA_PDS_MUL_DEST_CLRMSK)		>> EURASIA_PDS_MUL_DEST_SHIFT;
	dwSrc1Sel	= (dwInstruction & ~EURASIA_PDS_MUL_SRC1SEL_CLRMSK)		>> EURASIA_PDS_MUL_SRC1SEL_SHIFT;
	dwSrc1		= (dwInstruction & ~EURASIA_PDS_MUL_SRC1_CLRMSK)		>> EURASIA_PDS_MUL_SRC1_SHIFT;
	dwSrc2Sel	= (dwInstruction & ~EURASIA_PDS_MUL_SRC2SEL_CLRMSK)		>> EURASIA_PDS_MUL_SRC2SEL_SHIFT;
	dwSrc2		= (dwInstruction & ~EURASIA_PDS_MUL_SRC2_CLRMSK)		>> EURASIA_PDS_MUL_SRC2_SHIFT;

	if (dwSrc1Sel == EURASIA_PDS_MUL_SRC1SEL_DS0)
	{
		sprintf(achSrc1, "ds0[%u]", (dwSrc1 >> 1));
	}
	else
	{
		PDSDisassembleMUL_RegSrc(achSrc1, dwSrc1 >> 1);
	}
	strcat(achSrc1, apchSrcWord[dwSrc1 & 1]);

	#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
	if (dwSrc2Sel == EURASIA_PDS_MUL_SRC2SEL_REG)
	{
		PDSDisassembleMUL_RegSrc(achSrc2, dwSrc2 >> 1);
	}
	else
	#endif /* defined(SGX_FEATURE_PDS_EXTENDED_SOURCES) */
	{
		sprintf(achSrc2, "ds1[%u]", (dwSrc2 >> 1));
	}
	strcat(achSrc2, apchSrcWord[dwSrc2 & 1]);


	sprintf(pchInstruction,
			 "%s %s[%u], %s, %s",
			 apchType		[dwType],
			 apchDestSel	[dwDestSel],
			 dwDest,
			 achSrc1,
			 achSrc2);
}

/*****************************************************************************
 Function Name	: PDSDisassembleABS
 Inputs			: dwInstruction		- the encoded instruction
 Outputs		: pchInstruction	- the disassembled instruction
 Returns		: none
 Description	: Disassembles the ABS instruction
*****************************************************************************/
static IMG_VOID PDSDisassembleABS	(IMG_UINT32	dwInstruction,
								 IMG_PCHAR	pchInstruction)
{
	static const IMG_PCHAR apchType[] =
	{
		"add",
		"sub",
		"adc",
		"sbc",
		"mul",
		"abs",
		"",
		""
	};
	static const IMG_PCHAR apchDestSel[] =
	{
		"ds0",
		"ds1"
	};
	static const IMG_PCHAR apchSrc1Reg[] =
	{
		"ir0",
		"ir1"
	};

	IMG_UINT32	dwType;
	IMG_UINT32	dwDestSel;
	IMG_UINT32	dwDest;
	IMG_UINT32	dwSrc1Sel;
	IMG_UINT32	dwSrc1;
	IMG_UINT32	dwSrc2;
	IMG_UINT32	dwSrcSel;
	IMG_CHAR	achSrc1	[30];
	IMG_CHAR	achSrc2	[30];

	dwType		= (dwInstruction & ~EURASIA_PDS_TYPE_CLRMSK)			>> EURASIA_PDS_TYPE_SHIFT;
	dwDestSel	= (dwInstruction & ~EURASIA_PDS_ABS_DESTSEL_CLRMSK)		>> EURASIA_PDS_ABS_DESTSEL_SHIFT;
	dwDest		= (dwInstruction & ~EURASIA_PDS_ABS_DEST_CLRMSK)		>> EURASIA_PDS_ABS_DEST_SHIFT;
	dwSrc1Sel	= (dwInstruction & ~EURASIA_PDS_ABS_SRC1SEL_CLRMSK)		>> EURASIA_PDS_ABS_SRC1SEL_SHIFT;
	dwSrc1		= (dwInstruction & ~EURASIA_PDS_ABS_SRC1_CLRMSK)		>> EURASIA_PDS_ABS_SRC1_SHIFT;
	dwSrc2		= (dwInstruction & ~EURASIA_PDS_ABS_SRC2_CLRMSK)		>> EURASIA_PDS_ABS_SRC2_SHIFT;
	dwSrcSel	= (dwInstruction & ~EURASIA_PDS_ABS_SRCSEL_CLRMSK)		>> EURASIA_PDS_ABS_SRCSEL_SHIFT;

	if (dwSrc1Sel == EURASIA_PDS_ABS_SRC1SEL_DS0)
	{
		sprintf(achSrc1, "ds0[%u]", dwSrc1);
	}
	else
	{
		if (dwSrc1 < (sizeof(apchSrc1Reg) / sizeof(apchSrc1Reg[0])))
		{
			sprintf(achSrc1, "%s", apchSrc1Reg[dwSrc1]);
		}
		else
		{
			sprintf(achSrc1, "reserved_srcreg(=%d)", dwSrc1);
		}
	}
	sprintf(achSrc2, "ds1[%u]", dwSrc2);

	sprintf(pchInstruction,
			 "%s %s[%u], %s",
			 apchType		[dwType],
			 apchDestSel	[dwDestSel],
			 dwDest,
			 (dwSrcSel == EURASIA_PDS_ABS_SRCSEL_SRC1) ? achSrc1 : achSrc2);
}

/*****************************************************************************
 Function Name	: PDSDisassembleTest
 Inputs			: dwInstruction		- the encoded instruction
 Outputs		: pchInstruction	- the disassembled instruction
 Returns		: none
 Description	: Disassembles the TSTZ and TSTN instructions
*****************************************************************************/
static IMG_VOID PDSDisassembleTest	(IMG_UINT32	dwInstruction,
								 IMG_PCHAR	pchInstruction)
{
	static const IMG_PCHAR apchDest[] =
	{
		"p0",
		"p1",
		"p2",
		"",
		"",
		"",
		"",
		""
	};
	static const IMG_PCHAR apchSrc1Reg[] =
	{
		"ir0",
		"ir1"
	};

	IMG_UINT32	dwType;
	IMG_UINT32	dwDest;
	IMG_UINT32	dwSrc1Sel;
	IMG_UINT32	dwSrc1;
	IMG_UINT32	dwSrc2;
	IMG_UINT32	dwSrcSel;
	IMG_CHAR	achSrc1	[40];
	IMG_CHAR	achSrc2	[40];
	IMG_PCHAR	pchType;

	dwType		= (dwInstruction & ~EURASIA_PDS_TYPE_CLRMSK)			>> EURASIA_PDS_TYPE_SHIFT;
	dwDest		= (dwInstruction & ~EURASIA_PDS_TST_DEST_CLRMSK)		>> EURASIA_PDS_TST_DEST_SHIFT;
	dwSrc1Sel	= (dwInstruction & ~EURASIA_PDS_TST_SRC1SEL_CLRMSK)		>> EURASIA_PDS_TST_SRC1SEL_SHIFT;
	dwSrc1		= (dwInstruction & ~EURASIA_PDS_TST_SRC1_CLRMSK)		>> EURASIA_PDS_TST_SRC1_SHIFT;
	dwSrc2		= (dwInstruction & ~EURASIA_PDS_TST_SRC2_CLRMSK)		>> EURASIA_PDS_TST_SRC2_SHIFT;
	dwSrcSel	= (dwInstruction & ~EURASIA_PDS_TST_SRCSEL_CLRMSK)		>> EURASIA_PDS_TST_SRCSEL_SHIFT;

	if (dwSrc1Sel == EURASIA_PDS_TST_SRC1SEL_DS0)
	{
		sprintf(achSrc1, "ds0[%u]", dwSrc1);
	}
	else
	{
		if (dwSrc1 < (sizeof(apchSrc1Reg) / sizeof(apchSrc1Reg[0])))
		{
			sprintf(achSrc1, "%s", apchSrc1Reg[dwSrc1]);
		}
		else
		{
			sprintf(achSrc1, "reserved_srcreg(=%d)", dwSrc1);
		}
	}
	sprintf(achSrc2, "ds1[%u]", dwSrc2);

	#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
	if ((dwInstruction & EURASIA_PDS_TST_NEGATE) != 0)
	{
		if (dwType == EURASIA_PDS_TYPE_TSTZ)
		{
			pchType = "tstnz";
		}
		else
		{
			pchType = "tstp";
		}
	}
	else
	#endif /* defined(SGX_FEATURE_PDS_EXTENDED_SOURCES) */
	{
		if (dwType == EURASIA_PDS_TYPE_TSTZ)
		{
			pchType = "tstz";
		}
		else
		{
			pchType = "tstn";
		}
	}

	sprintf(pchInstruction,
			 "%s %s, %s",
			 pchType,
			 (dwDest < (sizeof(apchDest) / sizeof(apchDest[0]))) ? apchDest	[dwDest] : "bad",
			 (dwSrcSel == EURASIA_PDS_TST_SRCSEL_SRC1) ? achSrc1 : achSrc2);
}

/*****************************************************************************
 Function Name	: PDSDisassembleFlow
 Inputs			: dwInstruction		- the encoded instruction
 Outputs		: pchInstruction	- the disassembled instruction
 Returns		: none
 Description	: Disassembles the BRA and CALL instructions
*****************************************************************************/
static IMG_VOID PDSDisassembleFlow	(IMG_UINT32	dwInstruction,
								 IMG_PCHAR	pchInstruction,
								 IMG_PUINT32 pdwMaxJumpDest)
{
	static const IMG_PCHAR apchType[] =
	{
		"tstz",
		"tstn",
		"bra",
		"call",
		"rtn",
		"halt",
		"nop",
		""
	};

	IMG_UINT32	dwType;
	IMG_UINT32	dwDest;

	dwType		= (dwInstruction & ~EURASIA_PDS_TYPE_CLRMSK)		>> EURASIA_PDS_TYPE_SHIFT;
	dwDest		= (dwInstruction & ~EURASIA_PDS_FLOW_DEST_CLRMSK)	>> EURASIA_PDS_FLOW_DEST_SHIFT;

	if ((dwType == EURASIA_PDS_TYPE_BRA) ||
		(dwType == EURASIA_PDS_TYPE_CALL))
	{
		sprintf(pchInstruction,
				 "%s 0x%08X",
				 apchType		[dwType],
				 dwDest);
		if (pdwMaxJumpDest != NULL)
		{
			if ((*pdwMaxJumpDest) < dwDest)
			{
				*pdwMaxJumpDest = dwDest;
			}
		}
	}
	else
	{
		sprintf(pchInstruction,
				 "%s",
				 apchType		[dwType]);
	}
}

/*****************************************************************************
 Function Name	: PDSDisassembleLogic
 Inputs			: dwInstruction		- the encoded instruction
 Outputs		: pchInstruction	- the disassembled instruction
 Returns		: none
 Description	: Disassembles the OR and AND instructions
*****************************************************************************/
static IMG_VOID PDSDisassembleLogic	(IMG_UINT32	dwInstruction,
								 IMG_PCHAR	pchInstruction)
{
	static const IMG_PCHAR apchType[] =
	{
		"or",
		"and",
		"xor",
		"not",
		"nor",
		"nand",
		"shl",
		"shr"
	};
	static const IMG_PCHAR apchDestSel[] =
	{
		"ds0",
		"ds1"
	};
	static const IMG_PCHAR apchSrc1Reg[] =
	{
		"ir0",
		"ir1",
		"tim"
	};

	IMG_UINT32	dwType;
	IMG_UINT32	dwDestSel;
	IMG_UINT32	dwDest;
	IMG_UINT32	dwSrc1Sel;
	IMG_UINT32	dwSrc1;
	IMG_UINT32	dwSrc2Sel;
	IMG_UINT32	dwSrc2;
	IMG_UINT32	dwSrcSel;
	IMG_CHAR	achSrc1	[40];
	IMG_CHAR	achSrc2	[40];

	dwType		= (dwInstruction & ~EURASIA_PDS_TYPE_CLRMSK)			>> EURASIA_PDS_TYPE_SHIFT;
	dwDestSel	= (dwInstruction & ~EURASIA_PDS_LOGIC_DESTSEL_CLRMSK)	>> EURASIA_PDS_LOGIC_DESTSEL_SHIFT;
	dwDest		= (dwInstruction & ~EURASIA_PDS_LOGIC_DEST_CLRMSK)		>> EURASIA_PDS_LOGIC_DEST_SHIFT;
	dwSrc1Sel	= (dwInstruction & ~EURASIA_PDS_LOGIC_SRC1SEL_CLRMSK)	>> EURASIA_PDS_LOGIC_SRC1SEL_SHIFT;
	dwSrc1		= (dwInstruction & ~EURASIA_PDS_LOGIC_SRC1_CLRMSK)		>> EURASIA_PDS_LOGIC_SRC1_SHIFT;
	dwSrc2Sel	= (dwInstruction & ~EURASIA_PDS_LOGIC_SRC2SEL_CLRMSK)	>> EURASIA_PDS_LOGIC_SRC2SEL_SHIFT;
	dwSrc2		= (dwInstruction & ~EURASIA_PDS_LOGIC_SRC2_CLRMSK)		>> EURASIA_PDS_LOGIC_SRC2_SHIFT;
	dwSrcSel	= (dwInstruction & ~EURASIA_PDS_LOGIC_SRCSEL_CLRMSK)	>> EURASIA_PDS_LOGIC_SRCSEL_SHIFT;

	if (dwSrc1Sel == EURASIA_PDS_LOGIC_SRC1SEL_DS0)
	{
		sprintf(achSrc1, "ds0[%u]", dwSrc1);
	}
	else
	{
		if (dwSrc1 < (sizeof(apchSrc1Reg) / sizeof(apchSrc1Reg[0])))
		{
			sprintf(achSrc1, "%s", apchSrc1Reg[dwSrc1]);
		}
		else
		{
			sprintf(achSrc1, "reserved_srcreg(=%d)", dwSrc1);
		}
	}

	if (dwSrc2Sel == EURASIA_PDS_LOGIC_SRC2SEL_DS1)
	{
		sprintf(achSrc2, "ds1[%u]", dwSrc2);
	}
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
	else
	{
		if (dwSrc2 < (sizeof(apchSrc1Reg) / sizeof(apchSrc1Reg[0])))
		{
			sprintf(achSrc2, "%s", apchSrc1Reg[dwSrc2]);
		}
		else
		{
			sprintf(achSrc2, "reserved_srcreg(=%d)", dwSrc2);
		}
	}
#endif

	if (dwType == EURASIA_PDS_TYPE_NOT)
	{
		sprintf(pchInstruction,
				 "%s %s[%u], %s",
				 apchType		[dwType],
				 apchDestSel	[dwDestSel],
				 dwDest,
				 (dwSrcSel == EURASIA_PDS_LOGIC_SRCSEL_SRC1) ? achSrc1 : achSrc2);
	}
	else
	{
		sprintf(pchInstruction,
				"%s %s[%u], %s, %s",
				apchType		[dwType],
				apchDestSel	[dwDestSel],
				dwDest,
				achSrc1,
				achSrc2);
	}
}

/*****************************************************************************
 Function Name	: PDSDisassembleShift
 Inputs			: dwInstruction		- the encoded instruction
 Outputs		: pchInstruction	- the disassembled instruction
 Returns		: none
 Description	: Disassembles the SHR and SHL instructions
*****************************************************************************/
static IMG_VOID PDSDisassembleShift	(IMG_UINT32	dwInstruction,
								 IMG_PCHAR	pchInstruction)
{
	static const IMG_PCHAR apchType[] =
	{
		"or",
		"and",
		"xor",
		"not",
		"nor",
		"nand",
		"shl",
		"shr"
	};
	static const IMG_PCHAR apchDestSel[] =
	{
		"ds0",
		"ds1"
	};
	static const IMG_PCHAR apchSrcSel[] =
	{
		"ds0",
		"ds1",
		"reg",
		"reserved_srcsel(=3)"
	};
	static const IMG_PCHAR apchSrcReg[] =
	{
		"ir0",
		"ir1"
	};

	IMG_UINT32	dwType;
	IMG_UINT32	dwDestSel;
	IMG_UINT32	dwDest;
	IMG_UINT32	dwSrcSel;
	IMG_UINT32	dwSrc;
	IMG_UINT32	dwShift;
	IMG_INT32	lShift;
	IMG_CHAR	achSrc	[40];

	dwType		= (dwInstruction & ~EURASIA_PDS_TYPE_CLRMSK)			>> EURASIA_PDS_TYPE_SHIFT;
	dwDestSel	= (dwInstruction & ~EURASIA_PDS_SHIFT_DESTSEL_CLRMSK)	>> EURASIA_PDS_SHIFT_DESTSEL_SHIFT;
	dwDest		= (dwInstruction & ~EURASIA_PDS_SHIFT_DEST_CLRMSK)		>> EURASIA_PDS_SHIFT_DEST_SHIFT;
	dwSrcSel	= (dwInstruction & ~EURASIA_PDS_SHIFT_SRCSEL_CLRMSK)	>> EURASIA_PDS_SHIFT_SRCSEL_SHIFT;
	dwSrc		= (dwInstruction & ~EURASIA_PDS_SHIFT_SRC_CLRMSK)		>> EURASIA_PDS_SHIFT_SRC_SHIFT;
	dwShift		= (dwInstruction & ~EURASIA_PDS_SHIFT_SHIFT_CLRMSK)		>> EURASIA_PDS_SHIFT_SHIFT_SHIFT;
	lShift		= (dwShift <= EURASIA_PDS_SHIFT_SHIFT_MAX) ? (IMG_INT32) dwShift : (IMG_INT32) dwShift - 2 * EURASIA_PDS_SHIFT_SHIFT_MIN;

	if (dwSrcSel == EURASIA_PDS_SHIFT_SRCSEL_REG)
	{
		if (dwSrc < (sizeof(apchSrcReg) / sizeof(apchSrcReg[0])))
		{
			sprintf(achSrc, "%s", apchSrcReg[dwSrc]);
		}
		else
		{
			sprintf(achSrc, "reserved_srcreg(=%d)", dwSrc);
		}
	}
	else
	{
		sprintf(achSrc, "%s[%u]", apchSrcSel[dwSrcSel], dwSrc);
	}

	sprintf(pchInstruction,
			 "%s %s[%u], %s, %u",
			 apchType		[dwType],
			 apchDestSel	[dwDestSel],
			 dwDest,
			 achSrc,
			 lShift);
}

/*****************************************************************************
 Function Name	: PDSDisassemble
 Inputs			: 
 Outputs		: none
 Returns		: none
 Description	: Disassemble a PDS program
*****************************************************************************/
IMG_VOID PDSDisassemble(IMG_PUINT8	pui8Program, IMG_UINT32 uProgramLength)
{
	IMG_UINT32 uMaxJumpDest = 0;
	IMG_UINT32 uOffset;

	for (uOffset = 0; uOffset < (uProgramLength / EURASIA_PDS_INSTRUCTION_SIZE); uOffset++)
	{
		IMG_UINT32	uInstruction = ((IMG_PUINT32)pui8Program)[uOffset];
		IMG_CHAR	achInstruction[256];

		PDSDisassembleInstruction(achInstruction, uInstruction, &uMaxJumpDest);
		printf("%u: %s\n", uOffset, achInstruction);
	}
}
