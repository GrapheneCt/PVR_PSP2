%{
/******************************************************************************
 * Name         : pdsasm.l
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
 * $Log: pdsasm.l $
 *****************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "img_types.h"

#include "pdsasm.h"

#include "pdsasm.tab.h"

#if defined(_MSC_VER)
#pragma warning (disable:4131)
#pragma warning (disable:4127)
#pragma warning (disable:4244)
#endif /* defined(_MSC_VER) */

#define YY_SKIP_YYWRAP

static int yywrap(void)
{
	return 1;
}

int yylex(void);

extern void yyerror(const char *fmt, ...) IMG_FORMAT_PRINTF(1, 2);

static void yyeatcppcomment(void);
static void yyeatcomment(void);
static IMG_VOID ParseHashDirective(IMG_PCHAR yytext);
IMG_VOID addtoinvalidinput(IMG_CHAR c);
IMG_VOID dumpinvalidinput(IMG_VOID);

static IMG_BOOL g_bEof = IMG_FALSE;
static IMG_PCHAR g_pcSavedInvalidInput = NULL;

/*
	Returns the appropriate token when a newline is encounted. 
		(i) In C preprocessor mode the newline is ignored.
		(ii) In non-C preprocessor mode the newline is treated as delimited the end of an instruction.
*/
#define NEWLINE_TO_WHITESPACE_OR_DELIMITER	\
	if (!g_bCPreprocessor)					\
	{										\
		return SEPERATOR;		\
	}

%}
%x UNKNOWN_CHARACTER
%x IN_HEADER_FILE
%%

	/* Handle comment blocks and single line comments. */
"/*"				{ yyeatcomment();  }
"//"				{ yyeatcppcomment(); g_sCurrentLocation.uSourceLine++; NEWLINE_TO_WHITESPACE_OR_DELIMITER; }

	/* Predicates */
"p0"				{ return P0; }
"p1"				{ return P1; }
"p2"				{ return P2; }
"if0"				{ return IF0; }
"if1"				{ return IF1; }
"aluz"				{ return ALUZ; }
"alun"				{ return ALUN; }

"!"					{ return NEG; }

	/* Opcodes */
"movs"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_MOVS; return OPCODE; }
"movsa"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_MOVSA; return OPCODE; }
"mov16"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_MOV16; return OPCODE; }
"mov32"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_MOV32; return OPCODE; }
"mov64"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_MOV64; return OPCODE; }
"mov128"			{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_MOV128; return OPCODE; }
"add"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_ADD; return OPCODE; }
"sub"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_SUB; return OPCODE; }
"adc"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_ADC; return OPCODE; }
"sbc"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_SBC; return OPCODE; }
"mul16"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_MUL; return OPCODE; }
"abs"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_ABS; return OPCODE; }
"tstz"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_TSTZ; return OPCODE; }
"tstnz"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_TSTNZ; return OPCODE; }
"tstn"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_TSTN; return OPCODE; }
"tstp"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_TSTP; return OPCODE; }
"bra"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_BRA; return OPCODE; }
"call"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_CALL; return OPCODE; }
"rtn"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_RTN; return OPCODE; }
"halt"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_HALT; return OPCODE; }
"nop"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_NOP; return OPCODE; }
"alum"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_ALUM; return OPCODE; }
"or"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_OR; return OPCODE; }
"and"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_AND; return OPCODE; }
"xor"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_XOR; return OPCODE; }
"not"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_NOT; return OPCODE; }
"nor"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_NOR; return OPCODE; }
"nand"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_NAND; return OPCODE; }
"shl"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_SHL; return OPCODE; }
"shr"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_SHR; return OPCODE; }
"load"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_LOAD; return OPCODE; }
"store"				{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_STORE; return OPCODE; }
"rfence"			{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_RFENCE; return OPCODE; }
"rwfence"			{ yylval.sOpcode.sLocation = g_sCurrentLocation; yylval.sOpcode.eOpcode = PDSASM_OPCODE_RWFENCE; return OPCODE; }

	/* Register types. */
"ir0"				{ yylval.eSpecialRegister = PDSASM_SPECIAL_REGISTER_IR0; return SPECIAL_REGISTER; }
"ir1"				{ yylval.eSpecialRegister = PDSASM_SPECIAL_REGISTER_IR1; return SPECIAL_REGISTER; }
"pc"				{ yylval.eSpecialRegister = PDSASM_SPECIAL_REGISTER_PC; return SPECIAL_REGISTER; }
"tim"				{ yylval.eSpecialRegister = PDSASM_SPECIAL_REGISTER_TIMER; return SPECIAL_REGISTER; }
"doutu"				{ yylval.eSpecialRegister = PDSASM_SPECIAL_REGISTER_DOUTU; return SPECIAL_REGISTER; }
"doutt"				{ yylval.eSpecialRegister = PDSASM_SPECIAL_REGISTER_DOUTT; return SPECIAL_REGISTER; }
"douti"				{ yylval.eSpecialRegister = PDSASM_SPECIAL_REGISTER_DOUTI; return SPECIAL_REGISTER; }
"doutd"				{ yylval.eSpecialRegister = PDSASM_SPECIAL_REGISTER_DOUTD; return SPECIAL_REGISTER; }
"douta"				{ yylval.eSpecialRegister = PDSASM_SPECIAL_REGISTER_DOUTA; return SPECIAL_REGISTER; }
"slc"				{ yylval.eSpecialRegister = PDSASM_SPECIAL_REGISTER_SLC; return SPECIAL_REGISTER; }
"doutc"				{ yylval.eSpecialRegister = PDSASM_SPECIAL_REGISTER_DOUTC; return SPECIAL_REGISTER; }
"douts"				{ yylval.eSpecialRegister = PDSASM_SPECIAL_REGISTER_DOUTS; return SPECIAL_REGISTER; }
"douto"				{ yylval.eSpecialRegister = PDSASM_SPECIAL_REGISTER_DOUTO; return SPECIAL_REGISTER; }

	/* Register flags. */
".l"				{ return LOWFLAG; }
".h"				{ return HIGHFLAG; }

"temp"				{ return CLASS_TEMP; }
"data"				{ return CLASS_DATA; }

"dword"				{ return TYPE_DWORD; }
"qword"				{ return TYPE_QWORD; }

","					{ return COMMA; }
":"					{ return COLON; }

"+"					{ return PLUS; }
"-"					{ return MINUS; }
"<<"				{ return LSHIFT; }
">>"				{ return RSHIFT; }
"%"					{ return MODULUS; }
"/"					{ return DIVIDE; }
"^"					{ return XOR; }
"*"					{ return TIMES; }
"~"					{ return NOT; }
"&"					{ return AND; }
"|"					{ return OR; }
"("					{ return OPEN_BRACKET; }
")"					{ return CLOSE_BRACKET; }
"["					{ return OPEN_SQUARE_BRACKET; }
"]"					{ return CLOSE_SQUARE_BRACKET; }
"="					{ return EQUALS; }

"ds0"				{ return DATABANK0; }
"ds1"				{ return DATABANK1; }

"\n"				{ g_sCurrentLocation.uSourceLine++; NEWLINE_TO_WHITESPACE_OR_DELIMITER; }
"\r\n"				{ g_sCurrentLocation.uSourceLine++; NEWLINE_TO_WHITESPACE_OR_DELIMITER; }
";"					{ return SEPERATOR; }

[0-9]+(("U"|"L")*)			{ yylval.uNumber = strtoul(yytext, NULL, 10); return NUMBER; }
"0x"([0-9]|[A-F]|[a-f])+(("U"|"L")*)	{ yylval.uNumber = strtoul(yytext, NULL, 16); return NUMBER; }
[[:alpha:]_][[:alnum:]_]*	{ 
								yylval.sIdentifier.sLocation = g_sCurrentLocation;
								yylval.sIdentifier.pszIdentifier = strdup(yytext);
								return IDENTIFIER; 
							}


<INITIAL,IN_HEADER_FILE>^[ \f\t]*"#".*"\n"	{  ParseHashDirective(yytext); return SEPERATOR; }

<IN_HEADER_FILE>.*"\n"		{ g_sCurrentLocation.uSourceLine++; }

" "					
\t

.					{ BEGIN(UNKNOWN_CHARACTER); addtoinvalidinput(*yytext); }
<UNKNOWN_CHARACTER>.	{ addtoinvalidinput(*yytext); }
<UNKNOWN_CHARACTER>"\n"	{ BEGIN(INITIAL); dumpinvalidinput(); return SEPERATOR; }	
<UNKNOWN_CHARACTER>"\r\n"	{ BEGIN(INITIAL); dumpinvalidinput(); return SEPERATOR; }	
<UNKNOWN_CHARACTER><<EOF>>	{ BEGIN(INITIAL); dumpinvalidinput(); return SEPERATOR; }	


<<EOF>>				{ if (!g_bEof) { g_bEof = IMG_TRUE; NEWLINE_TO_WHITESPACE_OR_DELIMITER; } else { return -1; } }

%%

IMG_VOID addtoinvalidinput(IMG_CHAR c)
{
	if (g_pcSavedInvalidInput != NULL)
	{
		int len = strlen(g_pcSavedInvalidInput);
		g_pcSavedInvalidInput = realloc(g_pcSavedInvalidInput, len + 2);
		g_pcSavedInvalidInput[len] = c;
		g_pcSavedInvalidInput[len + 1] = '\0';
	}
	else
	{
		g_pcSavedInvalidInput = malloc(2);
		g_pcSavedInvalidInput[0] = c;
		g_pcSavedInvalidInput[1] = '\0';
	}
}

IMG_VOID dumpinvalidinput(IMG_VOID)
{
	yyerror("unexpected input '%s'", g_pcSavedInvalidInput); 
	free(g_pcSavedInvalidInput);
	g_pcSavedInvalidInput = NULL;
	g_sCurrentLocation.uSourceLine++; 
}

static void yyeatcppcomment(void)
{
	char c;
	while ((c = input()) != '\n' && c != EOF); 
}

static void yyeatcomment(void)
{
	char c, c1;
	IMG_UINT32	uStartLine = g_sCurrentLocation.uSourceLine;

loop:
	while ((c = input()) != '*' && c != EOF)
	{
		if (c == '\n')
		{
			g_sCurrentLocation.uSourceLine++;
		}
	}
	if (c == EOF)
	{
		fprintf(stderr, "%s(%u): error: End of file inside comment", g_sCurrentLocation.pszSourceFile, uStartLine);
		g_bParserError = IMG_TRUE;
		return;
	}

	if ((c1 = input()) != '/' && c != EOF)
	{
		if (c1 == EOF)
		{
			fprintf(stderr, "%s(%u): error: End of file inside comment", g_sCurrentLocation.pszSourceFile, uStartLine);
			g_bParserError = IMG_TRUE;
			return;
		}
		unput(c1);
		goto loop;
	}
}

static IMG_VOID CollapseStringCodes(IMG_PCHAR pszCodes)
{
	IMG_PCHAR	pszIn, pszOut;

	pszIn = pszOut = pszCodes;
	while (*pszIn != '\0')
	{
		if (*pszIn == '\\')
		{
			pszIn++;
			*pszOut++ = *pszIn++;
		}
		else
		{
			*pszOut++ = *pszIn++;
		}
	}
	*pszOut = '\0';
}

static IMG_VOID ParseHashDirective(IMG_PCHAR yytext)
{
	char* temp = yytext + 1;
	while (isspace(*temp)) { temp++; }
	if (strncmp(temp, "line", 4) == 0 || isdigit(*temp))
    {
		if (strncmp(temp, "line", 4) == 0)
		{
			temp += 4;
			while (isspace(*temp)) { temp++; }
		}
		g_sCurrentLocation.uSourceLine = strtoul(temp, &temp, 0);
		while (isspace(*temp)) { temp++; }
		if (*temp == '"')
		{
			IMG_PCHAR	pszDot;

			char* temp2;
			temp2 = strchr(temp + 1, '"');
			if (temp2 != NULL)
			{
				IMG_PCHAR	pszSourceFile;

				pszSourceFile = malloc(temp2 - temp);
				memcpy(pszSourceFile, temp + 1, temp2 - temp - 1);
				pszSourceFile[temp2 - temp - 1] = 0;
				CollapseStringCodes(pszSourceFile);

				g_sCurrentLocation.pszSourceFile = pszSourceFile;
			}

			pszDot = strrchr(g_sCurrentLocation.pszSourceFile, '.');
			if (
					pszDot != NULL &&
					(
						strcmp(pszDot, ".h") == 0 ||
						strcmp(pszDot, ".hpp") == 0 ||
						strcmp(pszDot, ".hxx") == 0
					)
			   )
			{
				BEGIN(IN_HEADER_FILE);
			}
			else
			{
				BEGIN(INITIAL);
			}
		}
	 }
	 else
	 {
		g_sCurrentLocation.uSourceLine++; 
	 }
}
