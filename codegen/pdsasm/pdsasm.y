%{
/******************************************************************************
 * Name         : pdsasm.y
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
 * $Log: pdsasm.y $
 *****************************************************************************/

#if defined(_MSC_VER)
#include <malloc.h>
#else
#include <stdlib.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "pdsasm.h"

#if defined(_MSC_VER)
#pragma warning (disable:4131)
#pragma warning (disable:4244)
#pragma warning (disable:4701)
#pragma warning (disable:4127)
#pragma warning (disable:4102)
#pragma warning (disable:4706)
#pragma warning (disable:4702)

/* Specify the malloc and free, to avoid problems with redeclarations on Windows */
#define YYMALLOC malloc
#define YYFREE free

#endif /* defined(_MSC_VER) */

#define YYDEBUG 1
#define YYERROR_VERBOSE

int yylex(void);
void yyerror(const char *fmt, ...) IMG_FORMAT_PRINTF(1, 2);

void yyerror(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	fprintf(stderr, "%s(%u): error: ", g_sCurrentLocation.pszSourceFile, g_sCurrentLocation.uSourceLine);
	vfprintf(stderr, fmt, va);
	fprintf(stderr, ".\n");
	va_end(va);

	g_bParserError = IMG_TRUE;
}

%}

%start program
%%

%union {
	PINST						psInstruction;
	OPCODE_WITH_LOCATION		sOpcode;
	PDSASM_PREDICATE			ePredicate;
	ARG_LIST					sArgList;
	IDENTIFIER_WITH_LOCATION	sIdentifier;
	ARG							sArg;
	PDSASM_SPECIAL_REGISTER		eSpecialRegister;
	PDSASM_ARG_FLAG				eArgFlag;
	IMG_UINT32					uNumber;
	IDENTIFIER_LIST				sIdentifierList;
	PDSASM_IDENTIFIER_CLASS		eIdentifierClass;
	PDSASM_IDENTIFIER_TYPE		eIdentifierType;
	PDSASM_DATASTORE_BANK		eDatastoreBank;
};

%token <sOpcode> OPCODE;
%token <sIdentifier> IDENTIFIER;
%token COLON;
%token COMMA;
%token <eSpecialRegister> SPECIAL_REGISTER;
%token P0;
%token P1;
%token P2;
%token IF0;
%token IF1;
%token NEG;
%token ALUZ;
%token ALUN;
%token SEPERATOR;
%token LOWFLAG;
%token HIGHFLAG;
%token <uNumber> NUMBER;
%token OR;
%token AND;
%token XOR;
%token RSHIFT;
%token LSHIFT;
%token MINUS;
%token PLUS;
%token DIVIDE;
%token TIMES;
%token NOT;
%token MODULUS;
%token OPEN_BRACKET;
%token CLOSE_BRACKET;
%token CLASS_TEMP;
%token CLASS_DATA;
%token TYPE_DWORD;
%token TYPE_QWORD;
%token DATABANK0;
%token DATABANK1;
%token OPEN_SQUARE_BRACKET;
%token CLOSE_SQUARE_BRACKET;
%token EQUALS;

%type <psInstruction> program;
%type <psInstruction> instruction;
%type <ePredicate> opt_predicate;
%type <sArgList> argument_list;
%type <sArg> argument;
%type <sArg> argument_with_flags;
%type <eArgFlag> opt_flags;
%type <uNumber> expr;
%type <uNumber> unary_expr;
%type <uNumber> primary_expr;
%type <sIdentifierList> identifier_list;
%type <eIdentifierClass> declaration_class;
%type <eIdentifierType> declaration_type;
%type <eDatastoreBank> datastore_bank;
%type <sIdentifier> identifier;

%left OR;
%left AND;
%left XOR;
%left RSHIFT;
%left LSHIFT;
%left MINUS;
%left PLUS;
%left DIVIDE;
%left TIMES;
%left NOT;
%left MODULUS;

program:
	/* Nothing */						{ $$ = NULL; }
	| program instruction SEPERATOR
										{
											if ($1 == NULL)
											{
												g_psInstructionListHead = $2;
											}
											else
											{
												$1->psNext = $2;
												$2->psPrev = $1;
											}
											$$ = $2;
										}
	| program declaration_class declaration_type identifier_list SEPERATOR
										{
											IMG_UINT32	uId;

											$$ = $1;
											for (uId = 0; uId < $4.uIdentifierCount; uId++)
											{
												AddNamedIdentifier($4.apsIdentifiers[uId].sLocation,
																   $4.apsIdentifiers[uId].pszIdentifier,
																   $2,
																   $3,
																   $4.apsIdentifiers[uId].ePreAssignedBank,
																   $4.apsIdentifiers[uId].uPreAssignedOffset);
											}
										}
	| program SEPERATOR					{ $$ = $1; }
	;

declaration_class:
	  CLASS_TEMP						{ $$ = PDSASM_IDENTIFIER_CLASS_TEMP; }
	| CLASS_DATA						{ $$ = PDSASM_IDENTIFIER_CLASS_DATA; }
	;

declaration_type:
	  TYPE_DWORD						{ $$ = PDSASM_IDENTIFIER_TYPE_DWORD; }
	| TYPE_QWORD						{ $$ = PDSASM_IDENTIFIER_TYPE_QWORD; }
	;

identifier_list:
	identifier_list COMMA identifier	{
											$$.uIdentifierCount = $1.uIdentifierCount + 1;
											$$.apsIdentifiers = malloc(sizeof($$.apsIdentifiers[0]) * $$.uIdentifierCount);
											memcpy($$.apsIdentifiers, $1.apsIdentifiers, 
												   sizeof($$.apsIdentifiers[0]) * $1.uIdentifierCount);
											$$.apsIdentifiers[$$.uIdentifierCount - 1] = $3;
										}
	| identifier						{	$$.uIdentifierCount = 1;
											$$.apsIdentifiers = malloc(sizeof($$.apsIdentifiers[0]) * 1);
											$$.apsIdentifiers[0] = $1;
										}
	;

identifier:
		IDENTIFIER						{
											$$ = $1;
											$$.ePreAssignedBank = PDSASM_DATASTORE_BANK_NONE;
											$$.uPreAssignedOffset = (IMG_UINT32)-1;
										}
	|	IDENTIFIER EQUALS datastore_bank		
										{
											$$ = $1;
											$$.ePreAssignedBank = $3;
											$$.uPreAssignedOffset = (IMG_UINT32)-1;
										}
	|	IDENTIFIER EQUALS datastore_bank OPEN_SQUARE_BRACKET expr CLOSE_SQUARE_BRACKET
										{
											$$ = $1;
											$$.ePreAssignedBank = $3;
											$$.uPreAssignedOffset = $5;
										}
	;

datastore_bank:
		DATABANK0						{	$$ = 0; }
	|	DATABANK1						{	$$ = 1; }
	;

instruction:
	opt_predicate OPCODE argument_list
										{
											$$ = malloc(sizeof(INST));
											$$->psPrev = $$->psNext = NULL;
											$$->eOpcode = $2.eOpcode;
											$$->sLocation = $2.sLocation;
											$$->ePredicate = $1;
											$$->uArgCount = $3.uCount;
											$$->psArgs = $3.psArgs;
										}
	| IDENTIFIER COLON
										{
											IMG_UINT32	uLabelId;

											uLabelId = AddNamedIdentifier($1.sLocation,
																		  $1.pszIdentifier,
																		  PDSASM_IDENTIFIER_CLASS_LABEL,
																		  PDSASM_IDENTIFIER_TYPE_NONE,
																		  PDSASM_DATASTORE_BANK_NONE,
																		  (IMG_UINT32)-1);

											$$ = malloc(sizeof(INST));
											$$->psPrev = $$->psNext = NULL;
											$$->eOpcode = PDSASM_OPCODE_LABEL;
											$$->sLocation = $1.sLocation;
											$$->ePredicate = PDSASM_PREDICATE_ALWAYS;
											$$->uArgCount = 1;
											$$->psArgs = malloc(sizeof(ARG));
											$$->psArgs[0].eType = PDSASM_REGISTER_TYPE_DATASTORE;
											$$->psArgs[0].u.uIdentifier = uLabelId;
										}
	;


opt_predicate:
	/* Nothing */						{ $$ = PDSASM_PREDICATE_ALWAYS; }
	| P0								{ $$ = PDSASM_PREDICATE_P0; }
	| P1								{ $$ = PDSASM_PREDICATE_P1; }
	| P2								{ $$ = PDSASM_PREDICATE_P2; }
	| IF0								{ $$ = PDSASM_PREDICATE_IF0; }
	| IF1								{ $$ = PDSASM_PREDICATE_IF1; }
	| ALUZ								{ $$ = PDSASM_PREDICATE_ALUZ; }
	| ALUN								{ $$ = PDSASM_PREDICATE_ALUN; }
	| NEG P0							{ $$ = PDSASM_PREDICATE_NEG_P0; }
	| NEG P1							{ $$ = PDSASM_PREDICATE_NEG_P1; }
	| NEG P2							{ $$ = PDSASM_PREDICATE_NEG_P2; }
	| NEG IF0							{ $$ = PDSASM_PREDICATE_NEG_IF0; }
	| NEG IF1							{ $$ = PDSASM_PREDICATE_NEG_IF1; }
	| NEG ALUZ							{ $$ = PDSASM_PREDICATE_NEG_ALUZ; }
	| NEG ALUN							{ $$ = PDSASM_PREDICATE_NEG_ALUN; }
	;
	
argument_list:
	/* Nothing */									{ 
														$$.uCount = 0;
														$$.psArgs = NULL;
													}
	| argument_with_flags							{
														$$.uCount = 1;
														$$.psArgs = malloc(sizeof(ARG));
														$$.psArgs[0] = $1;
													}
	| argument_list COMMA argument_with_flags
													{
														$$.uCount = $1.uCount + 1;
														$$.psArgs = malloc($$.uCount * sizeof(ARG));
														memcpy($$.psArgs, $1.psArgs, $1.uCount * sizeof(ARG));
														$$.psArgs[$$.uCount - 1] = $3;
														free($1.psArgs);
													}
	;

argument_with_flags:
		argument opt_flags							{
														$$ = $1;
														$$.eFlag = $2;
													}
	;

argument:
		SPECIAL_REGISTER							{ 
														$$.eType = PDSASM_REGISTER_TYPE_SPECIAL; 
														$$.u.eSpecialRegister = $1; 
													} 
	|	IDENTIFIER									{ 
														$$.eType = PDSASM_REGISTER_TYPE_DATASTORE; 
														$$.u.uIdentifier = LookupIdentifier($1.sLocation, $1.pszIdentifier);
													}
	|	expr										{ 
														$$.eType = PDSASM_REGISTER_TYPE_IMMEDIATE;
														$$.u.uImmediate = $1;
													}
	| P0											{
														$$.eType = PDSASM_REGISTER_TYPE_PREDICATE;
														$$.u.uPredicate = 0;
													}
	| P1											{
														$$.eType = PDSASM_REGISTER_TYPE_PREDICATE;
														$$.u.uPredicate = 1;
													}
	| P2											{
														$$.eType = PDSASM_REGISTER_TYPE_PREDICATE;
														$$.u.uPredicate = 2;
													}
	;

opt_flags:
		/* Nothing */								{ $$ = PDSASM_ARGFLAG_NONE; }
	| LOWFLAG										{ $$ = PDSASM_ARGFLAG_LOW; }
	| HIGHFLAG										{ $$ = PDSASM_ARGFLAG_HIGH; }
	;

expr:
	unary_expr						{ $$ = $1; }
	| expr PLUS unary_expr			{ $$ = $1 + $3; }
	| expr MINUS unary_expr			{ $$ = $1 - $3; }
	| expr TIMES unary_expr			{ $$ = $1 * $3; }
	| expr DIVIDE unary_expr		{ $$ = $1 / $3; }
	| expr LSHIFT unary_expr		{ $$ = $1 << $3; }
	| expr RSHIFT unary_expr		{ $$ = $1 >> $3; }
	| expr MODULUS unary_expr		{ $$ = $1 % $3; }
	| expr AND unary_expr			{ $$ = $1 & $3; }
	| expr OR unary_expr			{ $$ = $1 | $3; }
	| expr XOR unary_expr			{ $$ = $1 ^ $3; }
	;
		
unary_expr:
		primary_expr					{ $$ = $1; }
		| NOT unary_expr				{ $$ = ~$2; }
		| MINUS unary_expr				{ $$ = -(IMG_INT32)$2; }
		;
		
primary_expr:
		NUMBER								{ $$ = $1; }
		| OPEN_BRACKET expr CLOSE_BRACKET	{ $$ = $2; }
		;
