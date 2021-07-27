
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C
   
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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.4.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Copy the first part of user declarations.  */

/* Line 189 of yacc.c  */
#line 1 "codegen/pdsasm/pdsasm.y"

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



/* Line 189 of yacc.c  */
#line 144 "eurasiacon/binary2_540_omap4430_android_release/host/intermediates/pdsasmparser/pdsasm.tab.c"

/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif


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

/* Line 214 of yacc.c  */
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



/* Line 214 of yacc.c  */
#line 239 "eurasiacon/binary2_540_omap4430_android_release/host/intermediates/pdsasmparser/pdsasm.tab.c"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 264 of yacc.c  */
#line 251 "eurasiacon/binary2_540_omap4430_android_release/host/intermediates/pdsasmparser/pdsasm.tab.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  2
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   103

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  42
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  16
/* YYNRULES -- Number of rules.  */
#define YYNRULES  62
/* YYNRULES -- Number of states.  */
#define YYNSTATES  88

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   296

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint8 yyprhs[] =
{
       0,     0,     3,     4,     8,    14,    17,    19,    21,    23,
      25,    29,    31,    33,    37,    44,    46,    48,    52,    55,
      56,    58,    60,    62,    64,    66,    68,    70,    73,    76,
      79,    82,    85,    88,    91,    92,    94,    98,   101,   103,
     105,   107,   109,   111,   113,   114,   116,   118,   120,   124,
     128,   132,   136,   140,   144,   148,   152,   156,   160,   162,
     165,   168,   170
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      43,     0,    -1,    -1,    43,    49,    16,    -1,    43,    44,
      45,    46,    16,    -1,    43,    16,    -1,    33,    -1,    34,
      -1,    35,    -1,    36,    -1,    46,     6,    47,    -1,    47,
      -1,     4,    -1,     4,    41,    48,    -1,     4,    41,    48,
      39,    55,    40,    -1,    37,    -1,    38,    -1,    50,     3,
      51,    -1,     4,     5,    -1,    -1,     8,    -1,     9,    -1,
      10,    -1,    11,    -1,    12,    -1,    14,    -1,    15,    -1,
      13,     8,    -1,    13,     9,    -1,    13,    10,    -1,    13,
      11,    -1,    13,    12,    -1,    13,    14,    -1,    13,    15,
      -1,    -1,    52,    -1,    51,     6,    52,    -1,    53,    54,
      -1,     7,    -1,     4,    -1,    55,    -1,     8,    -1,     9,
      -1,    10,    -1,    -1,    17,    -1,    18,    -1,    56,    -1,
      55,    26,    56,    -1,    55,    25,    56,    -1,    55,    28,
      56,    -1,    55,    27,    56,    -1,    55,    24,    56,    -1,
      55,    23,    56,    -1,    55,    30,    56,    -1,    55,    21,
      56,    -1,    55,    20,    56,    -1,    55,    22,    56,    -1,
      57,    -1,    29,    56,    -1,    25,    56,    -1,    19,    -1,
      31,    55,    32,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   159,   159,   160,   173,   188,   192,   193,   197,   198,
     202,   209,   216,   221,   227,   236,   237,   241,   251,   276,
     277,   278,   279,   280,   281,   282,   283,   284,   285,   286,
     287,   288,   289,   290,   294,   298,   303,   314,   321,   325,
     329,   333,   337,   341,   348,   349,   350,   354,   355,   356,
     357,   358,   359,   360,   361,   362,   363,   364,   368,   369,
     370,   374,   375
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "OPCODE", "IDENTIFIER", "COLON", "COMMA",
  "SPECIAL_REGISTER", "P0", "P1", "P2", "IF0", "IF1", "NEG", "ALUZ",
  "ALUN", "SEPERATOR", "LOWFLAG", "HIGHFLAG", "NUMBER", "OR", "AND", "XOR",
  "RSHIFT", "LSHIFT", "MINUS", "PLUS", "DIVIDE", "TIMES", "NOT", "MODULUS",
  "OPEN_BRACKET", "CLOSE_BRACKET", "CLASS_TEMP", "CLASS_DATA",
  "TYPE_DWORD", "TYPE_QWORD", "DATABANK0", "DATABANK1",
  "OPEN_SQUARE_BRACKET", "CLOSE_SQUARE_BRACKET", "EQUALS", "$accept",
  "program", "declaration_class", "declaration_type", "identifier_list",
  "identifier", "datastore_bank", "instruction", "opt_predicate",
  "argument_list", "argument_with_flags", "argument", "opt_flags", "expr",
  "unary_expr", "primary_expr", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    42,    43,    43,    43,    43,    44,    44,    45,    45,
      46,    46,    47,    47,    47,    48,    48,    49,    49,    50,
      50,    50,    50,    50,    50,    50,    50,    50,    50,    50,
      50,    50,    50,    50,    51,    51,    51,    52,    53,    53,
      53,    53,    53,    53,    54,    54,    54,    55,    55,    55,
      55,    55,    55,    55,    55,    55,    55,    55,    56,    56,
      56,    57,    57
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     0,     3,     5,     2,     1,     1,     1,     1,
       3,     1,     1,     3,     6,     1,     1,     3,     2,     0,
       1,     1,     1,     1,     1,     1,     1,     2,     2,     2,
       2,     2,     2,     2,     0,     1,     3,     2,     1,     1,
       1,     1,     1,     1,     0,     1,     1,     1,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     1,     2,
       2,     1,     3
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       2,    19,     1,     0,    20,    21,    22,    23,    24,     0,
      25,    26,     5,     6,     7,     0,     0,     0,    18,    27,
      28,    29,    30,    31,    32,    33,     8,     9,     0,     3,
      34,    12,     0,    11,    39,    38,    41,    42,    43,    61,
       0,     0,     0,    17,    35,    44,    40,    47,    58,     0,
       0,     4,    60,    59,     0,     0,    45,    46,    37,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    15,
      16,    13,    10,    62,    36,    56,    55,    57,    53,    52,
      49,    48,    51,    50,    54,     0,     0,    14
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
      -1,     1,    15,    28,    32,    33,    71,    16,    17,    43,
      44,    45,    58,    46,    47,    48
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -43
static const yytype_int8 yypact[] =
{
     -43,     1,   -43,    -1,   -43,   -43,   -43,   -43,   -43,    88,
     -43,   -43,   -43,   -43,   -43,   -29,    25,    39,   -43,   -43,
     -43,   -43,   -43,   -43,   -43,   -43,   -43,   -43,    40,   -43,
      29,     4,     2,   -43,   -43,   -43,   -43,   -43,   -43,   -43,
      21,    21,    21,    43,   -43,    14,    65,   -43,   -43,   -18,
      40,   -43,   -43,   -43,    52,    29,   -43,   -43,   -43,    21,
      21,    21,    21,    21,    21,    21,    21,    21,    21,   -43,
     -43,     8,   -43,   -43,   -43,   -43,   -43,   -43,   -43,   -43,
     -43,   -43,   -43,   -43,   -43,    21,    41,   -43
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -43,   -43,   -43,   -43,   -43,     3,   -43,   -43,   -43,   -43,
      -4,   -43,   -43,   -42,   -38,   -43
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const yytype_uint8 yytable[] =
{
      54,     2,    52,    53,    18,     3,    26,    27,    50,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    51,    69,
      70,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    56,    57,    34,    13,    14,    35,    36,    37,    38,
      39,    29,    30,    86,    31,    49,    40,    85,    39,    55,
      41,    74,    42,    72,    40,     0,     0,     0,    41,     0,
      42,    59,    60,    61,    62,    63,    64,    65,    66,    67,
       0,    68,    59,    60,    61,    62,    63,    64,    65,    66,
      67,    87,    68,     0,    73,    59,    60,    61,    62,    63,
      64,    65,    66,    67,     0,    68,    19,    20,    21,    22,
      23,     0,    24,    25
};

static const yytype_int8 yycheck[] =
{
      42,     0,    40,    41,     5,     4,    35,    36,     6,     8,
       9,    10,    11,    12,    13,    14,    15,    16,    16,    37,
      38,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    17,    18,     4,    33,    34,     7,     8,     9,    10,
      19,    16,     3,    85,     4,    41,    25,    39,    19,     6,
      29,    55,    31,    50,    25,    -1,    -1,    -1,    29,    -1,
      31,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      -1,    30,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    40,    30,    -1,    32,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    -1,    30,     8,     9,    10,    11,
      12,    -1,    14,    15
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    43,     0,     4,     8,     9,    10,    11,    12,    13,
      14,    15,    16,    33,    34,    44,    49,    50,     5,     8,
       9,    10,    11,    12,    14,    15,    35,    36,    45,    16,
       3,     4,    46,    47,     4,     7,     8,     9,    10,    19,
      25,    29,    31,    51,    52,    53,    55,    56,    57,    41,
       6,    16,    56,    56,    55,     6,    17,    18,    54,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    30,    37,
      38,    48,    47,    32,    52,    56,    56,    56,    56,    56,
      56,    56,    56,    56,    56,    39,    55,    40
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yyrule)
    YYSTYPE *yyvsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  YYUSE (yyvaluep);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}

/* Prevent warnings from -Wmissing-prototypes.  */
#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */


/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;



/*-------------------------.
| yyparse or yypush_parse.  |
`-------------------------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{


    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.

       Refer to the stacks thru separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */
  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss_alloc, yyss);
	YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:

/* Line 1455 of yacc.c  */
#line 159 "codegen/pdsasm/pdsasm.y"
    { (yyval.psInstruction) = NULL; ;}
    break;

  case 3:

/* Line 1455 of yacc.c  */
#line 161 "codegen/pdsasm/pdsasm.y"
    {
											if ((yyvsp[(1) - (3)].psInstruction) == NULL)
											{
												g_psInstructionListHead = (yyvsp[(2) - (3)].psInstruction);
											}
											else
											{
												(yyvsp[(1) - (3)].psInstruction)->psNext = (yyvsp[(2) - (3)].psInstruction);
												(yyvsp[(2) - (3)].psInstruction)->psPrev = (yyvsp[(1) - (3)].psInstruction);
											}
											(yyval.psInstruction) = (yyvsp[(2) - (3)].psInstruction);
										;}
    break;

  case 4:

/* Line 1455 of yacc.c  */
#line 174 "codegen/pdsasm/pdsasm.y"
    {
											IMG_UINT32	uId;

											(yyval.psInstruction) = (yyvsp[(1) - (5)].psInstruction);
											for (uId = 0; uId < (yyvsp[(4) - (5)].sIdentifierList).uIdentifierCount; uId++)
											{
												AddNamedIdentifier((yyvsp[(4) - (5)].sIdentifierList).apsIdentifiers[uId].sLocation,
																   (yyvsp[(4) - (5)].sIdentifierList).apsIdentifiers[uId].pszIdentifier,
																   (yyvsp[(2) - (5)].eIdentifierClass),
																   (yyvsp[(3) - (5)].eIdentifierType),
																   (yyvsp[(4) - (5)].sIdentifierList).apsIdentifiers[uId].ePreAssignedBank,
																   (yyvsp[(4) - (5)].sIdentifierList).apsIdentifiers[uId].uPreAssignedOffset);
											}
										;}
    break;

  case 5:

/* Line 1455 of yacc.c  */
#line 188 "codegen/pdsasm/pdsasm.y"
    { (yyval.psInstruction) = (yyvsp[(1) - (2)].psInstruction); ;}
    break;

  case 6:

/* Line 1455 of yacc.c  */
#line 192 "codegen/pdsasm/pdsasm.y"
    { (yyval.eIdentifierClass) = PDSASM_IDENTIFIER_CLASS_TEMP; ;}
    break;

  case 7:

/* Line 1455 of yacc.c  */
#line 193 "codegen/pdsasm/pdsasm.y"
    { (yyval.eIdentifierClass) = PDSASM_IDENTIFIER_CLASS_DATA; ;}
    break;

  case 8:

/* Line 1455 of yacc.c  */
#line 197 "codegen/pdsasm/pdsasm.y"
    { (yyval.eIdentifierType) = PDSASM_IDENTIFIER_TYPE_DWORD; ;}
    break;

  case 9:

/* Line 1455 of yacc.c  */
#line 198 "codegen/pdsasm/pdsasm.y"
    { (yyval.eIdentifierType) = PDSASM_IDENTIFIER_TYPE_QWORD; ;}
    break;

  case 10:

/* Line 1455 of yacc.c  */
#line 202 "codegen/pdsasm/pdsasm.y"
    {
											(yyval.sIdentifierList).uIdentifierCount = (yyvsp[(1) - (3)].sIdentifierList).uIdentifierCount + 1;
											(yyval.sIdentifierList).apsIdentifiers = malloc(sizeof((yyval.sIdentifierList).apsIdentifiers[0]) * (yyval.sIdentifierList).uIdentifierCount);
											memcpy((yyval.sIdentifierList).apsIdentifiers, (yyvsp[(1) - (3)].sIdentifierList).apsIdentifiers, 
												   sizeof((yyval.sIdentifierList).apsIdentifiers[0]) * (yyvsp[(1) - (3)].sIdentifierList).uIdentifierCount);
											(yyval.sIdentifierList).apsIdentifiers[(yyval.sIdentifierList).uIdentifierCount - 1] = (yyvsp[(3) - (3)].sIdentifier);
										;}
    break;

  case 11:

/* Line 1455 of yacc.c  */
#line 209 "codegen/pdsasm/pdsasm.y"
    {	(yyval.sIdentifierList).uIdentifierCount = 1;
											(yyval.sIdentifierList).apsIdentifiers = malloc(sizeof((yyval.sIdentifierList).apsIdentifiers[0]) * 1);
											(yyval.sIdentifierList).apsIdentifiers[0] = (yyvsp[(1) - (1)].sIdentifier);
										;}
    break;

  case 12:

/* Line 1455 of yacc.c  */
#line 216 "codegen/pdsasm/pdsasm.y"
    {
											(yyval.sIdentifier) = (yyvsp[(1) - (1)].sIdentifier);
											(yyval.sIdentifier).ePreAssignedBank = PDSASM_DATASTORE_BANK_NONE;
											(yyval.sIdentifier).uPreAssignedOffset = (IMG_UINT32)-1;
										;}
    break;

  case 13:

/* Line 1455 of yacc.c  */
#line 222 "codegen/pdsasm/pdsasm.y"
    {
											(yyval.sIdentifier) = (yyvsp[(1) - (3)].sIdentifier);
											(yyval.sIdentifier).ePreAssignedBank = (yyvsp[(3) - (3)].eDatastoreBank);
											(yyval.sIdentifier).uPreAssignedOffset = (IMG_UINT32)-1;
										;}
    break;

  case 14:

/* Line 1455 of yacc.c  */
#line 228 "codegen/pdsasm/pdsasm.y"
    {
											(yyval.sIdentifier) = (yyvsp[(1) - (6)].sIdentifier);
											(yyval.sIdentifier).ePreAssignedBank = (yyvsp[(3) - (6)].eDatastoreBank);
											(yyval.sIdentifier).uPreAssignedOffset = (yyvsp[(5) - (6)].uNumber);
										;}
    break;

  case 15:

/* Line 1455 of yacc.c  */
#line 236 "codegen/pdsasm/pdsasm.y"
    {	(yyval.eDatastoreBank) = 0; ;}
    break;

  case 16:

/* Line 1455 of yacc.c  */
#line 237 "codegen/pdsasm/pdsasm.y"
    {	(yyval.eDatastoreBank) = 1; ;}
    break;

  case 17:

/* Line 1455 of yacc.c  */
#line 242 "codegen/pdsasm/pdsasm.y"
    {
											(yyval.psInstruction) = malloc(sizeof(INST));
											(yyval.psInstruction)->psPrev = (yyval.psInstruction)->psNext = NULL;
											(yyval.psInstruction)->eOpcode = (yyvsp[(2) - (3)].sOpcode).eOpcode;
											(yyval.psInstruction)->sLocation = (yyvsp[(2) - (3)].sOpcode).sLocation;
											(yyval.psInstruction)->ePredicate = (yyvsp[(1) - (3)].ePredicate);
											(yyval.psInstruction)->uArgCount = (yyvsp[(3) - (3)].sArgList).uCount;
											(yyval.psInstruction)->psArgs = (yyvsp[(3) - (3)].sArgList).psArgs;
										;}
    break;

  case 18:

/* Line 1455 of yacc.c  */
#line 252 "codegen/pdsasm/pdsasm.y"
    {
											IMG_UINT32	uLabelId;

											uLabelId = AddNamedIdentifier((yyvsp[(1) - (2)].sIdentifier).sLocation,
																		  (yyvsp[(1) - (2)].sIdentifier).pszIdentifier,
																		  PDSASM_IDENTIFIER_CLASS_LABEL,
																		  PDSASM_IDENTIFIER_TYPE_NONE,
																		  PDSASM_DATASTORE_BANK_NONE,
																		  (IMG_UINT32)-1);

											(yyval.psInstruction) = malloc(sizeof(INST));
											(yyval.psInstruction)->psPrev = (yyval.psInstruction)->psNext = NULL;
											(yyval.psInstruction)->eOpcode = PDSASM_OPCODE_LABEL;
											(yyval.psInstruction)->sLocation = (yyvsp[(1) - (2)].sIdentifier).sLocation;
											(yyval.psInstruction)->ePredicate = PDSASM_PREDICATE_ALWAYS;
											(yyval.psInstruction)->uArgCount = 1;
											(yyval.psInstruction)->psArgs = malloc(sizeof(ARG));
											(yyval.psInstruction)->psArgs[0].eType = PDSASM_REGISTER_TYPE_DATASTORE;
											(yyval.psInstruction)->psArgs[0].u.uIdentifier = uLabelId;
										;}
    break;

  case 19:

/* Line 1455 of yacc.c  */
#line 276 "codegen/pdsasm/pdsasm.y"
    { (yyval.ePredicate) = PDSASM_PREDICATE_ALWAYS; ;}
    break;

  case 20:

/* Line 1455 of yacc.c  */
#line 277 "codegen/pdsasm/pdsasm.y"
    { (yyval.ePredicate) = PDSASM_PREDICATE_P0; ;}
    break;

  case 21:

/* Line 1455 of yacc.c  */
#line 278 "codegen/pdsasm/pdsasm.y"
    { (yyval.ePredicate) = PDSASM_PREDICATE_P1; ;}
    break;

  case 22:

/* Line 1455 of yacc.c  */
#line 279 "codegen/pdsasm/pdsasm.y"
    { (yyval.ePredicate) = PDSASM_PREDICATE_P2; ;}
    break;

  case 23:

/* Line 1455 of yacc.c  */
#line 280 "codegen/pdsasm/pdsasm.y"
    { (yyval.ePredicate) = PDSASM_PREDICATE_IF0; ;}
    break;

  case 24:

/* Line 1455 of yacc.c  */
#line 281 "codegen/pdsasm/pdsasm.y"
    { (yyval.ePredicate) = PDSASM_PREDICATE_IF1; ;}
    break;

  case 25:

/* Line 1455 of yacc.c  */
#line 282 "codegen/pdsasm/pdsasm.y"
    { (yyval.ePredicate) = PDSASM_PREDICATE_ALUZ; ;}
    break;

  case 26:

/* Line 1455 of yacc.c  */
#line 283 "codegen/pdsasm/pdsasm.y"
    { (yyval.ePredicate) = PDSASM_PREDICATE_ALUN; ;}
    break;

  case 27:

/* Line 1455 of yacc.c  */
#line 284 "codegen/pdsasm/pdsasm.y"
    { (yyval.ePredicate) = PDSASM_PREDICATE_NEG_P0; ;}
    break;

  case 28:

/* Line 1455 of yacc.c  */
#line 285 "codegen/pdsasm/pdsasm.y"
    { (yyval.ePredicate) = PDSASM_PREDICATE_NEG_P1; ;}
    break;

  case 29:

/* Line 1455 of yacc.c  */
#line 286 "codegen/pdsasm/pdsasm.y"
    { (yyval.ePredicate) = PDSASM_PREDICATE_NEG_P2; ;}
    break;

  case 30:

/* Line 1455 of yacc.c  */
#line 287 "codegen/pdsasm/pdsasm.y"
    { (yyval.ePredicate) = PDSASM_PREDICATE_NEG_IF0; ;}
    break;

  case 31:

/* Line 1455 of yacc.c  */
#line 288 "codegen/pdsasm/pdsasm.y"
    { (yyval.ePredicate) = PDSASM_PREDICATE_NEG_IF1; ;}
    break;

  case 32:

/* Line 1455 of yacc.c  */
#line 289 "codegen/pdsasm/pdsasm.y"
    { (yyval.ePredicate) = PDSASM_PREDICATE_NEG_ALUZ; ;}
    break;

  case 33:

/* Line 1455 of yacc.c  */
#line 290 "codegen/pdsasm/pdsasm.y"
    { (yyval.ePredicate) = PDSASM_PREDICATE_NEG_ALUN; ;}
    break;

  case 34:

/* Line 1455 of yacc.c  */
#line 294 "codegen/pdsasm/pdsasm.y"
    { 
														(yyval.sArgList).uCount = 0;
														(yyval.sArgList).psArgs = NULL;
													;}
    break;

  case 35:

/* Line 1455 of yacc.c  */
#line 298 "codegen/pdsasm/pdsasm.y"
    {
														(yyval.sArgList).uCount = 1;
														(yyval.sArgList).psArgs = malloc(sizeof(ARG));
														(yyval.sArgList).psArgs[0] = (yyvsp[(1) - (1)].sArg);
													;}
    break;

  case 36:

/* Line 1455 of yacc.c  */
#line 304 "codegen/pdsasm/pdsasm.y"
    {
														(yyval.sArgList).uCount = (yyvsp[(1) - (3)].sArgList).uCount + 1;
														(yyval.sArgList).psArgs = malloc((yyval.sArgList).uCount * sizeof(ARG));
														memcpy((yyval.sArgList).psArgs, (yyvsp[(1) - (3)].sArgList).psArgs, (yyvsp[(1) - (3)].sArgList).uCount * sizeof(ARG));
														(yyval.sArgList).psArgs[(yyval.sArgList).uCount - 1] = (yyvsp[(3) - (3)].sArg);
														free((yyvsp[(1) - (3)].sArgList).psArgs);
													;}
    break;

  case 37:

/* Line 1455 of yacc.c  */
#line 314 "codegen/pdsasm/pdsasm.y"
    {
														(yyval.sArg) = (yyvsp[(1) - (2)].sArg);
														(yyval.sArg).eFlag = (yyvsp[(2) - (2)].eArgFlag);
													;}
    break;

  case 38:

/* Line 1455 of yacc.c  */
#line 321 "codegen/pdsasm/pdsasm.y"
    { 
														(yyval.sArg).eType = PDSASM_REGISTER_TYPE_SPECIAL; 
														(yyval.sArg).u.eSpecialRegister = (yyvsp[(1) - (1)].eSpecialRegister); 
													;}
    break;

  case 39:

/* Line 1455 of yacc.c  */
#line 325 "codegen/pdsasm/pdsasm.y"
    { 
														(yyval.sArg).eType = PDSASM_REGISTER_TYPE_DATASTORE; 
														(yyval.sArg).u.uIdentifier = LookupIdentifier((yyvsp[(1) - (1)].sIdentifier).sLocation, (yyvsp[(1) - (1)].sIdentifier).pszIdentifier);
													;}
    break;

  case 40:

/* Line 1455 of yacc.c  */
#line 329 "codegen/pdsasm/pdsasm.y"
    { 
														(yyval.sArg).eType = PDSASM_REGISTER_TYPE_IMMEDIATE;
														(yyval.sArg).u.uImmediate = (yyvsp[(1) - (1)].uNumber);
													;}
    break;

  case 41:

/* Line 1455 of yacc.c  */
#line 333 "codegen/pdsasm/pdsasm.y"
    {
														(yyval.sArg).eType = PDSASM_REGISTER_TYPE_PREDICATE;
														(yyval.sArg).u.uPredicate = 0;
													;}
    break;

  case 42:

/* Line 1455 of yacc.c  */
#line 337 "codegen/pdsasm/pdsasm.y"
    {
														(yyval.sArg).eType = PDSASM_REGISTER_TYPE_PREDICATE;
														(yyval.sArg).u.uPredicate = 1;
													;}
    break;

  case 43:

/* Line 1455 of yacc.c  */
#line 341 "codegen/pdsasm/pdsasm.y"
    {
														(yyval.sArg).eType = PDSASM_REGISTER_TYPE_PREDICATE;
														(yyval.sArg).u.uPredicate = 2;
													;}
    break;

  case 44:

/* Line 1455 of yacc.c  */
#line 348 "codegen/pdsasm/pdsasm.y"
    { (yyval.eArgFlag) = PDSASM_ARGFLAG_NONE; ;}
    break;

  case 45:

/* Line 1455 of yacc.c  */
#line 349 "codegen/pdsasm/pdsasm.y"
    { (yyval.eArgFlag) = PDSASM_ARGFLAG_LOW; ;}
    break;

  case 46:

/* Line 1455 of yacc.c  */
#line 350 "codegen/pdsasm/pdsasm.y"
    { (yyval.eArgFlag) = PDSASM_ARGFLAG_HIGH; ;}
    break;

  case 47:

/* Line 1455 of yacc.c  */
#line 354 "codegen/pdsasm/pdsasm.y"
    { (yyval.uNumber) = (yyvsp[(1) - (1)].uNumber); ;}
    break;

  case 48:

/* Line 1455 of yacc.c  */
#line 355 "codegen/pdsasm/pdsasm.y"
    { (yyval.uNumber) = (yyvsp[(1) - (3)].uNumber) + (yyvsp[(3) - (3)].uNumber); ;}
    break;

  case 49:

/* Line 1455 of yacc.c  */
#line 356 "codegen/pdsasm/pdsasm.y"
    { (yyval.uNumber) = (yyvsp[(1) - (3)].uNumber) - (yyvsp[(3) - (3)].uNumber); ;}
    break;

  case 50:

/* Line 1455 of yacc.c  */
#line 357 "codegen/pdsasm/pdsasm.y"
    { (yyval.uNumber) = (yyvsp[(1) - (3)].uNumber) * (yyvsp[(3) - (3)].uNumber); ;}
    break;

  case 51:

/* Line 1455 of yacc.c  */
#line 358 "codegen/pdsasm/pdsasm.y"
    { (yyval.uNumber) = (yyvsp[(1) - (3)].uNumber) / (yyvsp[(3) - (3)].uNumber); ;}
    break;

  case 52:

/* Line 1455 of yacc.c  */
#line 359 "codegen/pdsasm/pdsasm.y"
    { (yyval.uNumber) = (yyvsp[(1) - (3)].uNumber) << (yyvsp[(3) - (3)].uNumber); ;}
    break;

  case 53:

/* Line 1455 of yacc.c  */
#line 360 "codegen/pdsasm/pdsasm.y"
    { (yyval.uNumber) = (yyvsp[(1) - (3)].uNumber) >> (yyvsp[(3) - (3)].uNumber); ;}
    break;

  case 54:

/* Line 1455 of yacc.c  */
#line 361 "codegen/pdsasm/pdsasm.y"
    { (yyval.uNumber) = (yyvsp[(1) - (3)].uNumber) % (yyvsp[(3) - (3)].uNumber); ;}
    break;

  case 55:

/* Line 1455 of yacc.c  */
#line 362 "codegen/pdsasm/pdsasm.y"
    { (yyval.uNumber) = (yyvsp[(1) - (3)].uNumber) & (yyvsp[(3) - (3)].uNumber); ;}
    break;

  case 56:

/* Line 1455 of yacc.c  */
#line 363 "codegen/pdsasm/pdsasm.y"
    { (yyval.uNumber) = (yyvsp[(1) - (3)].uNumber) | (yyvsp[(3) - (3)].uNumber); ;}
    break;

  case 57:

/* Line 1455 of yacc.c  */
#line 364 "codegen/pdsasm/pdsasm.y"
    { (yyval.uNumber) = (yyvsp[(1) - (3)].uNumber) ^ (yyvsp[(3) - (3)].uNumber); ;}
    break;

  case 58:

/* Line 1455 of yacc.c  */
#line 368 "codegen/pdsasm/pdsasm.y"
    { (yyval.uNumber) = (yyvsp[(1) - (1)].uNumber); ;}
    break;

  case 59:

/* Line 1455 of yacc.c  */
#line 369 "codegen/pdsasm/pdsasm.y"
    { (yyval.uNumber) = ~(yyvsp[(2) - (2)].uNumber); ;}
    break;

  case 60:

/* Line 1455 of yacc.c  */
#line 370 "codegen/pdsasm/pdsasm.y"
    { (yyval.uNumber) = -(IMG_INT32)(yyvsp[(2) - (2)].uNumber); ;}
    break;

  case 61:

/* Line 1455 of yacc.c  */
#line 374 "codegen/pdsasm/pdsasm.y"
    { (yyval.uNumber) = (yyvsp[(1) - (1)].uNumber); ;}
    break;

  case 62:

/* Line 1455 of yacc.c  */
#line 375 "codegen/pdsasm/pdsasm.y"
    { (yyval.uNumber) = (yyvsp[(2) - (3)].uNumber); ;}
    break;



/* Line 1455 of yacc.c  */
#line 2065 "eurasiacon/binary2_540_omap4430_android_release/host/intermediates/pdsasmparser/pdsasm.tab.c"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (yymsg);
	  }
	else
	  {
	    yyerror (YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  *++yyvsp = yylval;


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined(yyoverflow) || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}



