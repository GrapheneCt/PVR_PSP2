/******************************************************************************
 * Name         : pdsasm.h
 * Title        : Pixel Shader Compiler
 * Author       : John Russell
 * Created      : Jan 2002
 *
 * Copyright    : 2002-2007 by Imagination Technologies Limited.
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
 * $Log: pdsasm.h $
 *****************************************************************************/

#ifndef __PDSASM_H_
#define __PDSASM_H_

#include "img_types.h"

typedef enum
{
	PDSASM_PREDICATE_ALWAYS,
	PDSASM_PREDICATE_P0,
	PDSASM_PREDICATE_P1,
	PDSASM_PREDICATE_P2,
	PDSASM_PREDICATE_IF0,
	PDSASM_PREDICATE_IF1,
	PDSASM_PREDICATE_ALUZ,
	PDSASM_PREDICATE_ALUN,

	PDSASM_PREDICATE_NEG_P0,
	PDSASM_PREDICATE_NEG_P1,
	PDSASM_PREDICATE_NEG_P2,
	PDSASM_PREDICATE_NEG_IF0,
	PDSASM_PREDICATE_NEG_IF1,
	PDSASM_PREDICATE_NEG_ALUZ,
	PDSASM_PREDICATE_NEG_ALUN,

} PDSASM_PREDICATE;

typedef enum
{
	PDSASM_OPCODE_MOVS,
	PDSASM_OPCODE_MOVSA,
	PDSASM_OPCODE_MOV16,
	PDSASM_OPCODE_MOV32,
	PDSASM_OPCODE_MOV64,
	PDSASM_OPCODE_MOV128,
	PDSASM_OPCODE_ADD,
	PDSASM_OPCODE_SUB,
	PDSASM_OPCODE_ADC,
	PDSASM_OPCODE_SBC,
	PDSASM_OPCODE_MUL,
	PDSASM_OPCODE_ABS,
	PDSASM_OPCODE_TSTZ,
	PDSASM_OPCODE_TSTNZ,
	PDSASM_OPCODE_TSTN,
	PDSASM_OPCODE_TSTP,
	PDSASM_OPCODE_BRA,
	PDSASM_OPCODE_CALL,
	PDSASM_OPCODE_RTN,
	PDSASM_OPCODE_HALT,
	PDSASM_OPCODE_NOP,
	PDSASM_OPCODE_ALUM,
	PDSASM_OPCODE_OR,
	PDSASM_OPCODE_AND,
	PDSASM_OPCODE_XOR,
	PDSASM_OPCODE_NOT,
	PDSASM_OPCODE_NOR,
	PDSASM_OPCODE_NAND,
	PDSASM_OPCODE_SHL,
	PDSASM_OPCODE_SHR,
	PDSASM_OPCODE_LABEL,
	PDSASM_OPCODE_LOAD,
	PDSASM_OPCODE_STORE,
	PDSASM_OPCODE_RFENCE,
	PDSASM_OPCODE_RWFENCE,
} PDSASM_OPCODE;

typedef enum
{
	PDSASM_SPECIAL_REGISTER_UNKNOWN,
	PDSASM_SPECIAL_REGISTER_IR0,
	PDSASM_SPECIAL_REGISTER_IR1,
	PDSASM_SPECIAL_REGISTER_PC,
	PDSASM_SPECIAL_REGISTER_TIMER,
	PDSASM_SPECIAL_REGISTER_DOUTD,
	PDSASM_SPECIAL_REGISTER_DOUTT,
	PDSASM_SPECIAL_REGISTER_DOUTI,
	PDSASM_SPECIAL_REGISTER_DOUTU,
	PDSASM_SPECIAL_REGISTER_DOUTA,
	PDSASM_SPECIAL_REGISTER_SLC,
	PDSASM_SPECIAL_REGISTER_DOUTC,
	PDSASM_SPECIAL_REGISTER_DOUTS,
	PDSASM_SPECIAL_REGISTER_DOUTO,
} PDSASM_SPECIAL_REGISTER;

typedef enum
{
	PDSASM_IDENTIFIER_CLASS_UNKNOWN,
	PDSASM_IDENTIFIER_CLASS_IMMEDIATE,
	PDSASM_IDENTIFIER_CLASS_TEMP,
	PDSASM_IDENTIFIER_CLASS_DATA,
	PDSASM_IDENTIFIER_CLASS_LABEL,
} PDSASM_IDENTIFIER_CLASS;

typedef enum
{
	PDSASM_IDENTIFIER_TYPE_NONE,
	PDSASM_IDENTIFIER_TYPE_DWORD,
	PDSASM_IDENTIFIER_TYPE_QWORD,
} PDSASM_IDENTIFIER_TYPE;

typedef enum
{
	PDSASM_ARGFLAG_NONE,
	PDSASM_ARGFLAG_LOW,
	PDSASM_ARGFLAG_HIGH,
} PDSASM_ARG_FLAG;

typedef enum
{
	PDSASM_DATASTORE_BANK_0,
	PDSASM_DATASTORE_BANK_1,
	PDSASM_DATASTORE_BANK_NONE = -1,
} PDSASM_DATASTORE_BANK;

typedef struct
{
	IMG_PCHAR	pszSourceFile;
	IMG_UINT32	uSourceLine;
} LOCATION;

typedef struct
{
	IMG_PCHAR				pszIdentifier;	
	LOCATION				sLocation;
	PDSASM_DATASTORE_BANK	ePreAssignedBank;
	IMG_UINT32				uPreAssignedOffset;
} IDENTIFIER_WITH_LOCATION;

typedef struct
{
	IMG_UINT32					uIdentifierCount;
	IDENTIFIER_WITH_LOCATION*	apsIdentifiers;
} IDENTIFIER_LIST;

typedef struct
{
	PDSASM_OPCODE		eOpcode;
	LOCATION			sLocation;
} OPCODE_WITH_LOCATION;

typedef enum
{
	PDSASM_REGISTER_TYPE_DATASTORE,
	PDSASM_REGISTER_TYPE_IMMEDIATE,
	PDSASM_REGISTER_TYPE_SPECIAL,
	PDSASM_REGISTER_TYPE_PREDICATE,
	PDSASM_REGISTER_TYPE_INSTRUCTION_PARAMETER,
} PDSASM_REGISTER_TYPE;

typedef struct
{
	PDSASM_REGISTER_TYPE	eType;
	union
	{
		PDSASM_SPECIAL_REGISTER		eSpecialRegister;
		IMG_UINT32					uIdentifier;
		struct
		{
			IMG_UINT32					uIdentifier;
			IMG_UINT32					uOffset;
		}							sParameter;
		IMG_UINT32					uImmediate;
		IMG_UINT32					uPredicate;
	} u;
	PDSASM_ARG_FLAG			eFlag;
	struct
	{
		IMG_UINT32					uBank;
		IMG_UINT32					uOffset;
	} sLocation;
} ARG, *PARG;

typedef struct
{
	IMG_UINT32			uCount;
	PARG				psArgs;
} ARG_LIST;

typedef struct _INST
{
	PDSASM_PREDICATE	ePredicate;
	PDSASM_OPCODE		eOpcode;
	IMG_UINT32			uArgCount;
	PARG				psArgs;
	struct _INST*		psPrev;
	struct _INST*		psNext;
	LOCATION			sLocation;
} INST, *PINST;

extern LOCATION			g_sCurrentLocation;
extern PINST			g_psInstructionListHead;
extern IMG_BOOL			g_bParserError;
extern IMG_BOOL			g_bCPreprocessor;

IMG_UINT32 LookupIdentifier(LOCATION sLocation, IMG_PCHAR pszName);
IMG_UINT32 AddNamedIdentifier(LOCATION sLocation, 
							  IMG_PCHAR pszName, 
							  PDSASM_IDENTIFIER_CLASS eClass, 
							  PDSASM_IDENTIFIER_TYPE eType,
							  PDSASM_DATASTORE_BANK ePreAssignedBank,
							  IMG_UINT32 uPreAssignedOffset);
IMG_VOID PDSDisassemble(IMG_PUINT8	pui8Program, IMG_UINT32 uProgramLength);
IMG_BOOL PDSDisassembleInstruction	(IMG_CHAR* achInstruction, IMG_UINT32	dwInstruction, IMG_PUINT32 pdwMaxJumpDest);

/* define PDSASM_UNDEF as -1 */
#define PDSASM_UNDEF	(~0UL)

#endif /* __PDSASM_H_ */
