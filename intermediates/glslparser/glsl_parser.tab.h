
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
     TOK_INVALID_TOKEN = 258,
     TOK_ARRAY_LENGTH = 259,
     TOK_ATTRIBUTE = 260,
     TOK_BREAK = 261,
     TOK_CENTROID = 262,
     TOK_CONST = 263,
     TOK_CONTINUE = 264,
     TOK_DISCARD = 265,
     TOK_DO = 266,
     TOK_ELSE = 267,
     TOK_FALSE = 268,
     TOK_FLAT = 269,
     TOK_FOR = 270,
     TOK_HIGHP = 271,
     TOK_IF = 272,
     TOK_IN = 273,
     TOK_INOUT = 274,
     TOK_INVARIANT = 275,
     TOK_LOWP = 276,
     TOK_MEDIUMP = 277,
     TOK_OUT = 278,
     TOK_PRECISION = 279,
     TOK_RETURN = 280,
     TOK_SAMPLER1D = 281,
     TOK_SAMPLER2D = 282,
     TOK_SAMPLER3D = 283,
     TOK_SAMPLER1DSHADOW = 284,
     TOK_SAMPLER2DSHADOW = 285,
     TOK_SAMPLER2DRECT = 286,
     TOK_SAMPLER2DRECTSHADOW = 287,
     TOK_SAMPLERCUBE = 288,
     TOK_STRUCT = 289,
     TOK_SUPER_PRECISION = 290,
     TOK_TRUE = 291,
     TOK_UNIFORM = 292,
     TOK_VARYING = 293,
     TOK_VOID = 294,
     TOK_WHILE = 295,
     TOK_BOOL = 296,
     TOK_BVEC2 = 297,
     TOK_BVEC3 = 298,
     TOK_BVEC4 = 299,
     TOK_FLOAT = 300,
     TOK_VEC2 = 301,
     TOK_VEC3 = 302,
     TOK_VEC4 = 303,
     TOK_INT = 304,
     TOK_IVEC2 = 305,
     TOK_IVEC3 = 306,
     TOK_IVEC4 = 307,
     TOK_MAT2X2 = 308,
     TOK_MAT2X3 = 309,
     TOK_MAT2X4 = 310,
     TOK_MAT3X2 = 311,
     TOK_MAT3X3 = 312,
     TOK_MAT3X4 = 313,
     TOK_MAT4X2 = 314,
     TOK_MAT4X3 = 315,
     TOK_MAT4X4 = 316,
     TOK_ASM = 317,
     TOK_CAST = 318,
     TOK_CLASS = 319,
     TOK_DEFAULT = 320,
     TOK_DOUBLE = 321,
     TOK_DVEC2 = 322,
     TOK_DVEC3 = 323,
     TOK_DVEC4 = 324,
     TOK_ENUM = 325,
     TOK_EXTERN = 326,
     TOK_EXTERNAL = 327,
     TOK_FIXED = 328,
     TOK_FVEC2 = 329,
     TOK_FVEC3 = 330,
     TOK_FVEC4 = 331,
     TOK_GOTO = 332,
     TOK_HALF = 333,
     TOK_HVEC2 = 334,
     TOK_HVEC3 = 335,
     TOK_HVEC4 = 336,
     TOK_INLINE = 337,
     TOK_INPUT = 338,
     TOK_INTERFACE = 339,
     TOK_LONG = 340,
     TOK_NAMESPACE = 341,
     TOK_NOINLINE = 342,
     TOK_OUTPUT = 343,
     TOK_PACKED = 344,
     TOK_PUBLIC = 345,
     TOK_SAMPLER3DRECT = 346,
     TOK_SHORT = 347,
     TOK_SIZEOF = 348,
     TOK_STATIC = 349,
     TOK_SWITCH = 350,
     TOK_TEMPLATE = 351,
     TOK_TILDE = 352,
     TOK_THIS = 353,
     TOK_TYPEDEF = 354,
     TOK_UNION = 355,
     TOK_UNSIGNED = 356,
     TOK_USING = 357,
     TOK_VOLATILE = 358,
     TOK_CASE = 359,
     TOK_CHAR = 360,
     TOK_BYTE = 361,
     TOK_INCOMMENT = 362,
     TOK_OUTCOMMENT = 363,
     TOK_COMMENT = 364,
     TOK_AMPERSAND = 365,
     TOK_BANG = 366,
     TOK_CARET = 367,
     TOK_COMMA = 368,
     TOK_COLON = 369,
     TOK_DASH = 370,
     TOK_DOT = 371,
     TOK_DOTDOT = 372,
     TOK_EQUAL = 373,
     TOK_LEFT_ANGLE = 374,
     TOK_LEFT_BRACE = 375,
     TOK_LEFT_BRACKET = 376,
     TOK_LEFT_PAREN = 377,
     TOK_PERCENT = 378,
     TOK_PLUS = 379,
     TOK_QUESTION = 380,
     TOK_RIGHT_ANGLE = 381,
     TOK_RIGHT_BRACE = 382,
     TOK_RIGHT_BRACKET = 383,
     TOK_RIGHT_PAREN = 384,
     TOK_SEMICOLON = 385,
     TOK_SLASH = 386,
     TOK_STAR = 387,
     TOK_VERTICAL_BAR = 388,
     TOK_APOSTROPHE = 389,
     TOK_AT = 390,
     TOK_BACK_SLASH = 391,
     TOK_DOLLAR = 392,
     TOK_HASH = 393,
     TOK_SPEECH_MARK = 394,
     TOK_UNDERSCORE = 395,
     TOK_ADD_ASSIGN = 396,
     TOK_AND_ASSIGN = 397,
     TOK_AND_OP = 398,
     TOK_DEC_OP = 399,
     TOK_DIV_ASSIGN = 400,
     TOK_EQ_OP = 401,
     TOK_INC_OP = 402,
     TOK_GE_OP = 403,
     TOK_LE_OP = 404,
     TOK_LEFT_ASSIGN = 405,
     TOK_LEFT_OP = 406,
     TOK_MOD_ASSIGN = 407,
     TOK_MUL_ASSIGN = 408,
     TOK_NE_OP = 409,
     TOK_OR_ASSIGN = 410,
     TOK_OR_OP = 411,
     TOK_RIGHT_ASSIGN = 412,
     TOK_RIGHT_OP = 413,
     TOK_SUB_ASSIGN = 414,
     TOK_XOR_OP = 415,
     TOK_XOR_ASSIGN = 416,
     TOK_FLOATCONSTANT = 417,
     TOK_BOOLCONSTANT = 418,
     TOK_INTCONSTANT = 419,
     TOK_UINTCONSTANT = 420,
     TOK_IDENTIFIER = 421,
     TOK_TYPE_NAME = 422,
     TOK_PROGRAMHEADER = 423,
     TOK_NEWLINE = 424,
     TOK_CARRIGE_RETURN = 425,
     TOK_TAB = 426,
     TOK_VTAB = 427,
     TOK_LANGUAGE_VERSION = 428,
     TOK_EXTENSION_CHANGE = 429,
     TOK_STRING = 430,
     TOK_ENDOFSTRING = 431,
     TOK_TERMINATEPARSING = 432,
     TOK_SAMPLERSTREAMIMG = 433,
     TOK_SAMPLEREXTERNALOES = 434,
     TOK_SMOOTH = 435,
     TOK_NOPERSPECTIVE = 436,
     TOK_SAMPLERCUBESHADOW = 437,
     TOK_SAMPLER1DARRAY = 438,
     TOK_SAMPLER2DARRAY = 439,
     TOK_SAMPLER1DARRAYSHADOW = 440,
     TOK_SAMPLER2DARRAYSHADOW = 441,
     TOK_ISAMPLER1D = 442,
     TOK_ISAMPLER2D = 443,
     TOK_ISAMPLER3D = 444,
     TOK_ISAMPLERCUBE = 445,
     TOK_ISAMPLER1DARRAY = 446,
     TOK_ISAMPLER2DARRAY = 447,
     TOK_USAMPLER1D = 448,
     TOK_USAMPLER2D = 449,
     TOK_USAMPLER3D = 450,
     TOK_USAMPLERCUBE = 451,
     TOK_USAMPLER1DARRAY = 452,
     TOK_USAMPLER2DARRAY = 453,
     TOK_UINT = 454,
     TOK_UVEC2 = 455,
     TOK_UVEC3 = 456,
     TOK_UVEC4 = 457,
     TOK_HASHHASH = 458
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif




