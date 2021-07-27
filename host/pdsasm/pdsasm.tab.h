
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton interface for Bison's Yacc-like parsers in C
   
      Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     OPCODE = 258,
     IDENTIFIER = 259,
     COLON = 260,
     COMMA = 261,
     SPECIAL_REGISTER = 262,
     P0 = 263,
     P1 = 264,
     P2 = 265,
     IF0 = 266,
     IF1 = 267,
     NEG = 268,
     ALUZ = 269,
     ALUN = 270,
     SEPERATOR = 271,
     LOWFLAG = 272,
     HIGHFLAG = 273,
     NUMBER = 274,
     OR = 275,
     AND = 276,
     XOR = 277,
     RSHIFT = 278,
     LSHIFT = 279,
     MINUS = 280,
     PLUS = 281,
     DIVIDE = 282,
     TIMES = 283,
     NOT = 284,
     MODULUS = 285,
     OPEN_BRACKET = 286,
     CLOSE_BRACKET = 287,
     CLASS_TEMP = 288,
     CLASS_DATA = 289,
     TYPE_DWORD = 290,
     TYPE_QWORD = 291,
     DATABANK0 = 292,
     DATABANK1 = 293,
     OPEN_SQUARE_BRACKET = 294,
     CLOSE_SQUARE_BRACKET = 295,
     EQUALS = 296
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 1676 of yacc.c  */
#line 74 "codegen/pdsasm/pdsasm.y"

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



/* Line 1676 of yacc.c  */
#line 111 "eurasiacon/binary2_540_omap4430_android_release/host/intermediates/pdsasmparser/pdsasm.tab.h"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE yylval;


