/*****************************************************************************
 * Name         : uscshrd.h
 * Title        : Internal interfaces used by USC
 * Created      : April 2005
 *
 * Copyright      : 2002-2007 by Imagination Technologies Ltd.
 *                All rights reserved. No part of this software, either
 *                material or conceptual may be copied or distributed,
 *                transmitted, transcribed, stored in a retrieval system or
 *                translated into any human or computer language in any form
 *                by any means, electronic, mechanical, manual or otherwise,
 *                or disclosed to third parties without the express written
 *                permission of Imagination Technologies Ltd,
 *                Home Park Estate, Kings Langley, Hertfordshire,
 *                WD4 8LZ, U.K.
 *
 * Modifications :-
 * $Log: uscshrd.h $
 *  
 * Increase the cases where a vec4 instruction can be split into two vec2
 * parts.
 *  --- Revision Logs Removed --- 
 * of 4.
 *  --- Revision Logs Removed --- 
 *****************************************************************************/

#ifndef USCSHRDH
#define USCSHRDH


#ifdef UF_TESTBENCH

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <setjmp.h>
#include <limits.h>

#else /* UF_TESTBENCH */

#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#include <limits.h>

#endif /* UF_TESTBENCH */
#if defined(DEBUG)
#include <stdarg.h>
#endif

#include "img_types.h"
#include "sgxdefs.h"
#include "use.h"
#include "usc.h"

#if defined(OUTPUT_USPBIN)
#include "usp.h"
#endif /* defined(OUTPUT_USPBIN) */

#include "alloc.h"

/*this file has the declarations for our custom memory allocator*/
#include "mem_alloc.h"

/*
	Define min and max macros if they don't exist (stdlib.h doesn't include them on
	all platforms).
*/
#ifndef min
#define min(a,b)    (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a,b)    (((a) > (b)) ? (a) : (b))
#endif

/* Forward declarations. */
typedef struct _INST* PINST;
typedef struct _INST const* PCINST;
typedef struct _FIXED_REG_DATA_* PFIXED_REG_DATA;
typedef struct _CODEBLOCK_* PCODEBLOCK;
typedef struct tagINTERMEDIATE_STATE *PINTERMEDIATE_STATE;
typedef struct _FUNC_* PFUNC;
typedef struct _ARG* PARG;
typedef struct _ARG const* PCARG;
typedef struct _VREGISTER* PVREGISTER;
typedef struct _SOURCE_VECTOR* PSOURCE_VECTOR;
typedef struct _SOURCE_VECTOR const* PCSOURCE_VECTOR;
struct _DGRAPH_STATE_;

/*
	USC Error handler routine (unused on UF_TESTBENCH builds at present)
*/
#if defined(__CC_ARM)
__declspec(noreturn)
#endif /* _MSC_VER */
void UscAbort(IMG_PVOID		pvState,
			  IMG_UINT32	uError,
			  IMG_PCHAR		pcErrorStr,
			  IMG_PCHAR		pszFile,
			  IMG_UINT32	uLine)
#ifdef __GNUC__
__attribute__ ((noreturn))
#endif /* __GNUC__ */
;

#if defined(DEBUG)
#define UscAbort2(pvState, E, S) UscAbort(pvState, (E), (S), __FILE__, __LINE__)
#else
#define UscAbort2(pvState, E, S) UscAbort(pvState, (E), IMG_NULL, "", 0)
#endif

/*
  Support for timing: only on DEBUG Linux build
 */

/* DEBUG_TIME: Toggle time profiling */
#if defined(UF_TESTBENCH)
#if defined(LINUX)

#define DEBUG_TIME (1)

#endif /* defined(LINUX) || defined(WIN32) */
#endif /* defined(UF_TESTBENCH) */

#if defined(DEBUG_TIME)
/* Include timers */
#include <time.h>

/* Start, stop, read timer */
#define USC_CLOCK_SET(clk) { clk = clock() ; }
#define USC_CLOCK_DIFF(start,stop) ((IMG_DOUBLE)(stop - start))
#define USC_CLOCK_ELAPSED(start) (((IMG_DOUBLE)((clock()) - start)) / CLOCKS_PER_SEC)

#else /* !defined(DEBUG_TIME) */

/* Start, stop, read timer */
#define USC_CLOCK_SET(clk)
#define USC_CLOCK_DIFF(start,stop)
#define USC_CLOCK_ELAPSED(start,stop)
#endif /* defined(DEBUG_TIME) */


/******************************
 *
 * Configuration macros
 *
 ******************************/

/*
  Possible configurations:
    USC_OFFLINE: Offline (batch) compiler build (default)
	JIT:         Online (just-in-time) compiler build

 Configuration options:
    REGALLOC_GCOL: Use graph-colouring register allocator (default)
    REGALLOC_LSCAN: Use linear-scan based register allocator
    IREGALLOC_LSCAN: Use linear-scan based c10 register allocator
 */

#define USC_OFFLINE

#if defined(JIT)
#undef USC_OFFLINE
#endif /* !defined(JIT) */

/*
 * Options for all configurations
 */
#define IREGALLOC_LSCAN

/*
 * Batch mode configuration
 */
#if defined(USC_OFFLINE)
#define REGALLOC_GCOL
#endif /* defined(USC_OFFLINE) */

/*
 * Just-In-Time configuration
 */
#if defined(JIT)
//#define REGALLOC_LSCAN
#define REGALLOC_GCOL
#endif /* defined(JIT) */

/*
 * Testbench: Include both register allocators
 */
#if defined(UF_TESTBENCH)
#define REGALLOC_GCOL
//#define REGALLOC_LSCAN
#endif /* defined(UF_TESTBENCH) */

#if defined(IREGALLOC_LSCAN)
/* DEBUG_IREGALLOC: If set, add information to help debug the iregister allocator */
//#define DEBUG_IREGALLOC
#endif /* defined(IREGALLOC_LSCAN) */

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
#ifndef EXECPRED
#define EXECPRED
#endif /* #ifndef EXECPRED */
#endif /* #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

/*
  Configuration settings:

  USC_UNIFLEX  : Uniflex input, HW output (DEFAULT)
  OUTPUT_USPBIN: Uniflex input, USP output
  GPGPU        : GPGPU input, HW output

  Note that GPGPU shares code with USC_UNIFLEX.
*/
#if defined(OUTPUT_USCHW) || defined(OUTPUT_USPBIN)
#define INPUT_UNIFLEX
#endif /* defined(OUTPUT_USCHW) || defined(OUTPUT_USPBIN) */

/******************************
 *
 * Helper macros to select different code/behaviour when OUTPUT_USPBIN or
 * OUTPUT_USCHW options are enabled.
 *
 * CHOOSE_USPBIN_OR_USCHW_FUNC(FUNC, ARGS)
 * call the target specific version of function FUNC with argument list ARGS.
 *
 * CHOOSE_USPBIN_OR_USCHW_VALUE(NAME)
 * evaluate to the target specific version of define NAME.
 *
 ******************************/
#if defined(OUTPUT_USPBIN) && defined(OUTPUT_USCHW)
	#define CHOOSE_USPBIN_OR_USCHW_FUNC(FUNC, ARGS)				\
		(((psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) != 0)	? FUNC##USPBIN ARGS : FUNC##USCHW ARGS)
	#define CHOOSE_USPBIN_OR_USCHW_VALUE(NAME)					\
		(((psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) != 0) ? NAME##_USPBIN : NAME##_USCHW)
#elif defined(OUTPUT_USPBIN)
	#define CHOOSE_USPBIN_OR_USCHW_FUNC(FUNC, ARGS)				(FUNC##USPBIN ARGS)
	#define CHOOSE_USPBIN_OR_USCHW_VALUE(NAME)					(NAME##_USPBIN)
#elif defined(OUTPUT_USCHW)
	#define CHOOSE_USPBIN_OR_USCHW_FUNC(FUNC, ARGS)				(FUNC##USCHW ARGS)
	#define CHOOSE_USPBIN_OR_USCHW_VALUE(NAME)					(NAME##_USCHW)
#else
	#pragma message("At least one of OUTPUT_USPBIN and OUTPUT_USCHW must be defined.")
#endif

/******************************
 *
 * Failure and error reporting macros
 *
 ******************************/

/*
    UscFail definition

	Indicate failure and take action to cleanly end execution.
	Currently this is a wrapper around UscAbort.

	UscFail(psState, uErrorNum, pcErrorStr)
	PINTERMEDIATE_STATE psState   : Compiler state.
	IMG_UINT32          uErrorNum : Error number (one of the UF_ERR_* numbers from usc.h)
    IMG_PCHAR           pcErrorStr: Error message.

	psState or pcErrorStr can be NULL.
	uErrorNum should be UF_ERR_INTERNAL if no other number is appropriate.
*/
#define UscFail(psState,uErrorNum,pcErrorStr) { UscAbort2(psState, uErrorNum, pcErrorStr); }

/*
	Generic unconditional failure macro (a wrapper for UscAbort that fills in the
	state, file and line parameters). Use this for handled failure cases (out of
	memory etc), rather than otherwise unhandled conditions.

	NB: UF_TESTBENCH builds don't call UscAbort since there is no cleanup handler
		at that point (post compilation), but we must reference psState in the
		macro to avoid warnings about it being unused (the non UF_TESTBENCH macros
		reference it).
*/
#ifndef USC_ERROR
	#ifndef UF_TESTBENCH
		#define USC_ERROR(E, S)	UscAbort2(psState, (E), (S))
	#else	/* #ifndef UF_TESTBENCH */
		#define USC_ERROR(E, S)	{ PVR_UNREFERENCED_PARAMETER(psState); abort(); }
	#endif	/* #ifndef UF_TESTBENCH */
#endif	/* #ifndef USC_ERROR */

/******************************
 *
 *  Memory allocation macros
 *
 ******************************/

#define UscFree(state, ptr)	{ IMG_PVOID *tptr = (IMG_PVOID*)&(ptr); _UscFree(state, tptr); }

#define usc_alloc_num(state, array, count) array = UscAlloc(state, sizeof(array[0]) * count)
#define usc_alloc_bitnum(state, array, count) array = UscAlloc(psState, UINTS_TO_SPAN_BITS(count) * sizeof(IMG_UINT32))

/*
	Macros for recording metrics
*/
#ifdef METRICS
#define METRICS_START(state,x)	{if (state->pfnStart) state->pfnStart(state->pvMetricsFnDrvParam, USC_METRICS_##x ); }
#define METRICS_FINISH(state,x)	{if (state->pfnFinish) state->pfnFinish(state->pvMetricsFnDrvParam, USC_METRICS_##x ); }
#else
#define METRICS_START(state,x)
#define METRICS_FINISH(state,x)
#endif /* #ifdef METRICS */

/* Size macros */
#define UINTS_TO_SPAN_BITS(B)	(((B) + 31) >> 5)

#define FLOAT32_ZERO					(0x00000000)
#define FLOAT32_ONE						(0x3F800000)
#define FLOAT32_HALF					(0x3F000000)
#define FLOAT32_TWO						(0x40000000)
#define FLOAT32_MINUSONE				(0xBF800000)

#define FLOAT16_ZERO					(0x0000U)
#define FLOAT16_ONE						(0x3C00U)
#define FLOAT16_HALF					(0x3A00U)
#define FLOAT16_TWO						(0x4000U)
#define FLOAT16_MINUSONE				(0xBC00U)
#define FLOAT16_REPLICATE(X)			((X) | ((X) << 16))
#define FLOAT16_ONE_REPLICATED			FLOAT16_REPLICATE(FLOAT16_ONE)
#define FLOAT16_MINUSONE_REPLICATED		FLOAT16_REPLICATE(FLOAT16_MINUSONE)
#define FLOAT16_TWO_REPLICATED			FLOAT16_REPLICATE(FLOAT16_TWO)

/* USC_MAX_NUM_REGARRAYS: Maximum number of register arrays */
#define USC_MAX_NUM_REGARRAYS				(16)

/* USC_REGARRAY_LIMIT: Maximum number of registers that can be used for register arrays */
#define USC_REGARRAY_LIMIT					(16)

#define CHANNELS_PER_INPUT_REGISTER			(4)

#define USC_MAX_DRDEST						(4)

#define USC_TEMPREG_TEMPDEST				(0)
#define USC_TEMPREG_TEMPSRC					(USC_TEMPREG_TEMPDEST + 4)
#define USC_TEMPREG_COLOUTPUT				(USC_TEMPREG_TEMPSRC + 4 * CHANNELS_PER_INPUT_REGISTER)
#define USC_TEMPREG_ZOUTPUT					(USC_TEMPREG_COLOUTPUT + UNIFLEX_MAX_OUT_SURFACES * CHANNELS_PER_INPUT_REGISTER)
#define USC_TEMPREG_OMASKOUTPUT				(USC_TEMPREG_ZOUTPUT + 1)
#define USC_TEMPREG_ADDRESS					(USC_TEMPREG_OMASKOUTPUT + 1)
#define USC_TEMPREG_DRDEST					(USC_TEMPREG_ADDRESS + 4)
#define USC_TEMPREG_DUMMY					(USC_TEMPREG_DRDEST + USC_MAX_DRDEST)
#define USC_TEMPREG_VSCLIPPOS				(USC_TEMPREG_DUMMY + 1)
#define USC_TEMPREG_TEMPADDRESS				(USC_TEMPREG_VSCLIPPOS + 4)
#define USC_TEMPREG_INDEXTEMP				(USC_TEMPREG_TEMPADDRESS + 1)
#define USC_TEMPREG_F32INDEXTEMPDEST		(USC_TEMPREG_INDEXTEMP + 1)
#define USC_TEMPREG_F16INDEXTEMPDEST		(USC_TEMPREG_F32INDEXTEMPDEST + 4)
#define USC_TEMPREG_C10INDEXTEMPDEST		(USC_TEMPREG_F16INDEXTEMPDEST + 4)
#define USC_TEMPREG_VECTEMPDEST				(USC_TEMPREG_C10INDEXTEMPDEST + 4)
#define USC_TEMPREG_C10TEMP					(USC_TEMPREG_VECTEMPDEST + 4)
#define USC_TEMPREG_F16TEMP					(USC_TEMPREG_C10TEMP + 4)
#define USC_TEMPREG_CVTC10DSTOUT			(USC_TEMPREG_F16TEMP + 4)
#define USC_TEMPREG_CVTF16DSTOUT			(USC_TEMPREG_CVTC10DSTOUT + 4)
#define USC_TEMPREG_CVTVECF16DSTOUT			(USC_TEMPREG_CVTF16DSTOUT + 4)
#define USC_TEMPREG_CVTVECF32DSTOUT			(USC_TEMPREG_CVTVECF16DSTOUT + 4)
#define USC_TEMPREG_TEMPDEST2				(USC_TEMPREG_CVTVECF32DSTOUT + 4)
#define USC_TEMPREG_OUTPUT_BUFF_NUM			(USC_TEMPREG_TEMPDEST2 + 4)
#define USC_TEMPREG_OUTPUT_BUFF_IDX			(USC_TEMPREG_OUTPUT_BUFF_NUM + 1)
#define USC_TEMPREG_PRIMITIVE_ID			(USC_TEMPREG_OUTPUT_BUFF_IDX + 1)
#define USC_TEMPREG_MAXIMUM					(USC_TEMPREG_PRIMITIVE_ID + 1)

#define USC_PREDREG_TEXKILL					(0)
#define USC_PREDREG_TEMP					(1)
#define USC_PREDREG_P0X						(2)
#define USC_PREDREG_P0Y						(3)
#define USC_PREDREG_P0Z						(4)
#define USC_PREDREG_P0W						(5)
#define USC_PREDREG_PNMOD4					(~1U)		/* -2 */
#define USC_PREDREG_NONE					(~0U)		/* -1 */

#define USC_NUM_PREDREGS					(6)

#define USC_INDEXREG_LOW					(0)
#define USC_INDEXREG_HIGH					(1)

/* USC_REGTYPE: Placeholder for usc register type */
typedef IMG_UINT32 USC_REGTYPE, *PUSC_REGTYPE;

// USC_REGTYPE: Temporarily commented out
#if 0
typedef enum _USC_REGTYPE_
{
	USC_REGTYPE_TEMP = USEASM_REGTYPE_TEMP,
	USC_REGTYPE_OUTPUT =	USEASM_REGTYPE_OUTPUT,
	USC_REGTYPE_PRIMATTR = USEASM_REGTYPE_PRIMATTR,
	USC_REGTYPE_SECATTR = USEASM_REGTYPE_SECATTR,
	USC_REGTYPE_INDEX = USEASM_REGTYPE_INDEX,
	USC_REGTYPE_GLOBAL = USEASM_REGTYPE_GLOBAL,
	USC_REGTYPE_FPCONSTANT = USEASM_REGTYPE_FPCONSTANT,
	USC_REGTYPE_FPINTERNAL = USEASM_REGTYPE_FPINTERNAL,
	USC_REGTYPE_IMMEDIATE = USEASM_REGTYPE_IMMEDIATE,
	USC_REGTYPE_LINK = USEASM_REGTYPE_LINK,
	USC_REGTYPE_DRC = USEASM_REGTYPE_DRC,
	USC_REGTYPE_LABEL = USEASM_REGTYPE_LABEL,
	USC_REGTYPE_PREDICATE = USEASM_REGTYPE_PREDICATE,
	USC_REGTYPE_CLIPPLANE = USEASM_REGTYPE_CLIPPLANE,
	USC_REGTYPE_ADDRESSMODE = USEASM_REGTYPE_ADDRESSMODE,
	USC_REGTYPE_SWIZZLE = USEASM_REGTYPE_SWIZZLE,
	USC_REGTYPE_INTSRCSEL = USEASM_REGTYPE_INTSRCSEL,
	USC_REGTYPE_FILTERCOEFF = USEASM_REGTYPE_FILTERCOEFF,
	USC_REGTYPE_LABEL_WITH_OFFSET = USEASM_REGTYPE_LABEL_WITH_OFFSET,
	USC_REGTYPE_TEMP_NAMED = USEASM_REGTYPE_TEMP_NAMED,

	USC_REGTYPE_DUMMY = USEASM_REGTYPE_MAXIMUM,
	USC_REGTYPE_DPACCUM = USEASM_REGTYPE_MAXIMUM + 1,

	/*
	  USC_REGTYPE_REGARRAY: Array in temporary registers
      Registers of this type should be treated as indexed temporary registers.
    */
	USC_REGTYPE_REGARRAY = USEASM_REGTYPE_MAXIMUM + 2,

    /*
	   USC_REGTYPE_ARRAYBASE: Base number of array in temporary registers
	   Registers of this type should be treated as immediate registers whose value
	   is the number of the array's base register.
	*/

	USC_REGTYPE_ARRAYBASE = USEASM_REGTYPE_MAXIMUM + 3,

    /*
	  USC_REGTYPE_UNUSEDSOURCE: Place holder for a source not used by the instruction.
	*/
	USC_REGTYPE_UNUSEDSOURCE = USEASM_REGTYPE_MAXIMUM + 4,

	USC_REGTYPE_MAXIMUM
} USC_REGTYPE;
#endif

// Start of USC_REGTYPE macros
//#if 0
#define USC_REGTYPE_DUMMY					(USEASM_REGTYPE_MAXIMUM + 0)
#define USC_REGTYPE_DPACCUM					(USEASM_REGTYPE_MAXIMUM + 1)
/*
   USC_REGTYPE_REGARRAY: Array in temporary registers
   Registers of this type should be treated as indexed temporary registers.
*/
#define USC_REGTYPE_REGARRAY				(USEASM_REGTYPE_MAXIMUM + 2)
/*
   USC_REGTYPE_ARRAYBASE: Base number of array in temporary registers
   Registers of this type should be treated as immediate registers whose value
   is the number of the array's base register.
*/
#define USC_REGTYPE_ARRAYBASE				(USEASM_REGTYPE_MAXIMUM + 3)
/*
	Place holder for a source not used by the instruction.
*/
#define USC_REGTYPE_UNUSEDSOURCE			(USEASM_REGTYPE_MAXIMUM + 4)
/*
	Place holder for a destination not used by the instruction.
*/
#define USC_REGTYPE_UNUSEDDEST				(USEASM_REGTYPE_MAXIMUM + 5)
/*
	Geometry shader input registers.
*/
#define USC_REGTYPE_GSINPUT					(USEASM_REGTYPE_MAXIMUM + 6)
/*
	Secondary attributes which are written in the secondary update program.
*/
#define USC_REGTYPE_CALCSECATTR				(USEASM_REGTYPE_MAXIMUM + 7)
#define USC_REGTYPE_NOINDEX					(USEASM_REGTYPE_MAXIMUM + 8)
#define USC_REGTYPE_BOOLEAN					(USEASM_REGTYPE_MAXIMUM + 9)
#define USC_REGTYPE_EXECPRED				(USEASM_REGTYPE_MAXIMUM + 10)
/*
   USC_REGTYPE_MAXIMUM: Largest register type known to the USC.
 */
#define USC_REGTYPE_MAXIMUM					(USEASM_REGTYPE_MAXIMUM + 11)
/* USC_REGTYPE_UNDEF: Undefined register type. */
#define USC_REGTYPE_UNDEF					(0xFFFF)

//#endif
// End of USC_REGTYPE macros

#define USC_DESTMASK_FULL					(0xFU)
#define USC_DESTMASK_LOW					(0x3U)
#define USC_DESTMASK_HIGH					(0xCU)

#define CHANS_PER_REGISTER					(4)

/*
	Maximum size of the hardware's GPI register bank in 32-bit/40-bit units.
*/
#define MAXIMUM_GPI_SIZE_IN_SCALAR_REGS		(16)

#include "inst.h"
#include "bitops.h"

/*
 * General data structures
 */

/*
   Size definitions
*/

/* BITS_PER_C10_COMPONENT: Number of bits in a C10 component. */
#define BITS_PER_C10_COMPONENT (10)

/* BITS_PER_UINT: Number of bits in an unsigned integer */
#define BITS_PER_UINT_LOG2 (5)
#define BITS_PER_UINT (BITS_PER_BYTE * sizeof(IMG_UINT32))

/* BYTE_SIZE: Number of bytes in an 8 bit integer */
#define BYTE_SIZE_LOG2 (0)
#define BYTE_SIZE (sizeof(IMG_UINT8))

/* WORD_SIZE: Number of bytes in a 16 bit integer */
#define WORD_SIZE_LOG2 (1)
#define WORD_SIZE (sizeof(IMG_UINT16))

/* LONG_SIZE: Number of bytes in a 32 bit integer */
#define LONG_SIZE_LOG2 (2)
#define LONG_SIZE (sizeof(IMG_UINT32))

/* QWORD_SIZE: Number of bytes in a 64 bit integer */
#define QWORD_SIZE_LOG2 (3)
#define QWORD_SIZE (sizeof(IMG_UINT64))

/* DQWORD_SIZE: Number of bytes in a 128 bit integer. */
#define DQWORD_SIZE_LOG2 (4)
#define DQWORD_SIZE (LONG_SIZE * 4)

/*
	Maximum number of ALU instructions a branch can contain 
	and still be 'flattened' (using predicates).
*/
#define USC_MAXALUINSTSTOFLATTEN_DEFAULT			(100U)

#if defined(EXECPRED)
/*
	Maximum number of ALU instructions a branch can contain 
	and still be 'flattened' (using predicates) in the 
	presence of Execution Predication feature.
*/
#define USC_MAXALUINSTSTOFLATTEN_EXECPRED			(6U)
#endif /* #if defined(EXECPRED) */

/* USC_UNDEF: Generic undefined value == -1 */
#define USC_UNDEF (~0U)

/*
  General Data Types
 */
#include "data.h"


/*
  USC Data Types
 */

typedef struct
{
	USC_VECTOR 		sPredicate;
	USC_VECTOR		sPrimAttr;
	USC_VECTOR		sTemp;
	USC_VECTOR		sOutput;
	USC_VECTOR		sFpInternal;
	IMG_UINT32		puIndexReg[UINTS_TO_SPAN_BITS(CHANS_PER_REGISTER)];
	IMG_BOOL        bLinkReg;
} REGISTER_LIVESET, *PREGISTER_LIVESET;

/*
  USC_MAX_GROUPING_DISTANCE: Maximum number of instructions between
   the first and last instruction used to form a repeat group.
*/
#define USC_MAX_GROUPING_DISTANCE (10)

/* Channel names and numbers */
typedef enum _USC_REG_CHANS_
{
	USC_RED_CHAN = 0,
	USC_GREEN_CHAN = 1,
	USC_BLUE_CHAN = 2,
	USC_ALPHA_CHAN = 3,

	USC_MAX_REG_CHAN
} USC_REG_CHANS;

#define USC_X_CHAN			USC_RED_CHAN
#define USC_Y_CHAN			USC_GREEN_CHAN
#define USC_Z_CHAN			USC_BLUE_CHAN
#define USC_W_CHAN			USC_ALPHA_CHAN

#define USC_X_CHAN_MASK		(1U << USC_X_CHAN)
#define USC_Y_CHAN_MASK		(1U << USC_Y_CHAN)
#define USC_Z_CHAN_MASK		(1U << USC_Z_CHAN)
#define USC_W_CHAN_MASK		(1U << USC_W_CHAN)

#define USC_XY_CHAN_MASK	(USC_X_CHAN_MASK | USC_Y_CHAN_MASK)
#define USC_ZW_CHAN_MASK	(USC_Z_CHAN_MASK | USC_W_CHAN_MASK)
#define USC_XYZ_CHAN_MASK	(USC_X_CHAN_MASK | USC_Y_CHAN_MASK | USC_Z_CHAN_MASK)

/* Generic channel masks */
#define USC_RED_CHAN_MASK (1U << USC_RED_CHAN)
#define USC_GREEN_CHAN_MASK (1U << USC_GREEN_CHAN)
#define USC_BLUE_CHAN_MASK (1U << USC_BLUE_CHAN)
#define USC_ALPHA_CHAN_MASK (1U << USC_ALPHA_CHAN)

#define USC_RGB_CHAN_MASK ((1U << USC_RED_CHAN) | \
							(1U << USC_GREEN_CHAN) | \
							(1U << USC_BLUE_CHAN))
#define USC_ALL_CHAN_MASK ((1U << USC_ALPHA_CHAN) | USC_RGB_CHAN_MASK)
#define USC_GBA_CHAN_MASK	(USC_GREEN_CHAN_MASK | USC_BLUE_CHAN_MASK | USC_ALPHA_CHAN_MASK)
#define USC_XYZW_CHAN_MASK	USC_ALL_CHAN_MASK

#define USC_ANY_NONEMPTY_CHAN_MASK	(UINT_MAX)

/*
	8-bit fixed point format vector of (1, 1, 1, 1).
*/
#define USC_U8_VEC4_ONE			(0xFFFFFFFF)

#define SCALAR_REGISTER_SIZE_IN_BITS			(8 * LONG_SIZE)

#define U8_CHANNEL_SIZE_IN_BITS					(8)

#define C10_CHANNEL_SIZE_IN_BITS				(10)

#define F16_CHANNEL_SIZE_IN_BITS				(8 * WORD_SIZE)

#define F32_CHANNEL_SIZE_IN_BITS				(8 * LONG_SIZE)

#define VECTOR_LENGTH							(4U)

#define U8_CHANNELS_PER_SCALAR_REGISTER			(SCALAR_REGISTER_SIZE_IN_BITS / U8_CHANNEL_SIZE_IN_BITS)

#define SCALAR_REGISTERS_PER_U8_VECTOR			(VECTOR_LENGTH / U8_CHANNELS_PER_SCALAR_REGISTER)

#define C10_CHANNELS_PER_SCALAR_REGISTER		(SCALAR_REGISTER_SIZE_IN_BITS / C10_CHANNEL_SIZE_IN_BITS)

#define SCALAR_REGISTERS_PER_C10_VECTOR			\
							((VECTOR_LENGTH + C10_CHANNELS_PER_SCALAR_REGISTER - 1) / C10_CHANNELS_PER_SCALAR_REGISTER)

#define SCALAR_REGISTERS_PER_C10_VEC3		\
							((3 /* Vec3 */ + C10_CHANNELS_PER_SCALAR_REGISTER - 1) / C10_CHANNELS_PER_SCALAR_REGISTER)

#define F16_CHANNELS_PER_SCALAR_REGISTER		(SCALAR_REGISTER_SIZE_IN_BITS / F16_CHANNEL_SIZE_IN_BITS)

#define SCALAR_REGISTERS_PER_F16_VECTOR			(VECTOR_LENGTH / F16_CHANNELS_PER_SCALAR_REGISTER)

#define F32_CHANNELS_PER_SCALAR_REGISTER		(SCALAR_REGISTER_SIZE_IN_BITS / F32_CHANNEL_SIZE_IN_BITS)

#define SCALAR_REGISTERS_PER_F32_VECTOR			(VECTOR_LENGTH / F32_CHANNELS_PER_SCALAR_REGISTER)

#define MAX_SCALAR_REGISTERS_PER_VECTOR			(max(SCALAR_REGISTERS_PER_F16_VECTOR, SCALAR_REGISTERS_PER_F32_VECTOR))

#define GetRegMask(ARRAY, REG) GetRange(ARRAY, ((REG) * CHANS_PER_REGISTER) + CHANS_PER_REGISTER - 1, (REG) * CHANS_PER_REGISTER)
#define SetRegMask(ARRAY, REG, MASK) SetRange(ARRAY, ((REG) * CHANS_PER_REGISTER) + CHANS_PER_REGISTER - 1, (REG) * CHANS_PER_REGISTER, MASK)

/*
	Represents the set of variable containing the texture coordinate input
	to a texture sample.
*/
typedef struct _SAMPLE_COORDINATES
{
	/* Size of the set. */
	IMG_UINT32		uCoordSize;
	/* Type of the coordinates in the set. */
	IMG_UINT32		uCoordType;
	/* Number of the first temporary register in the set. */
	IMG_UINT32		uCoordStart;
	/* Format of the registers in the set. */
	UF_REGFORMAT	eCoordFormat;
	/*
		Mask of channels used from the last register in the set. The previous registers
		are fully referenced.
	*/
	IMG_UINT32		uUsedCoordChanMask;
	/* Is this a sample of a texture array? */
	IMG_BOOL		bTextureArray;
	/* Seperate temporary register which contains the 16-bit integer texture array index. */
	IMG_UINT32		uArrayIndexTemp;
	/* Seperate temporary register which contains the PCF comparison value. */
	IMG_UINT32		uPCFComparisonTemp;
} SAMPLE_COORDINATES, *PSAMPLE_COORDINATES;

/*
	Represents the set of variable containing the gradients input
	to a texture sample.
*/
typedef struct _SAMPLE_GRADIENTS
{
	/* Size of the set. */
	IMG_UINT32		uGradSize;
	/* Number of the first temporary register in the set. */
	IMG_UINT32		uGradStart;
	/* Format of the registers in the set. */
	UF_REGFORMAT	eGradFormat;
	/* Mask of channels used from each register in the set. */
	IMG_UINT32		uUsedGradChanMask;
} SAMPLE_GRADIENTS, *PSAMPLE_GRADIENTS;

typedef struct _SAMPLE_LOD_ADJUST
{
	IMG_BOOL		bLODAdjustment;
	IMG_UINT32		uLODTemp;
	UF_REGFORMAT	eLODFormat;
} SAMPLE_LOD_ADJUST, *PSAMPLE_LOD_ADJUST;

typedef struct _SAMPLE_PROJECTION
{
	IMG_BOOL		bProjected;
	ARG				sProj;
	IMG_UINT32		uProjChannel;
	IMG_UINT32		uProjMask;
} SAMPLE_PROJECTION, *PSAMPLE_PROJECTION;

typedef struct _SAMPLE_RESULT_LOCATION
{
	USC_CHANNELFORM	eFormat;
	IMG_UINT32		uChunkIdx;
	IMG_UINT32		uRegOffsetInChunk;
	IMG_UINT32		uChanOffset;
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	IMG_BOOL		bVector;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
} SAMPLE_RESULT_LOCATION, *PSAMPLE_RESULT_LOCATION;

typedef struct _SAMPLE_RESULT_LAYOUT
{
	IMG_UINT32				uChunkCount;
	IMG_UINT32				uTexelSizeInBytes;
	SAMPLE_RESULT_CHUNK		asChunk[UF_MAX_CHUNKS_PER_TEXTURE];
	IMG_UINT32				uChanCount;
	PSAMPLE_RESULT_LOCATION	asChannelLocation;
} SAMPLE_RESULT_LAYOUT, *PSAMPLE_RESULT_LAYOUT;

typedef struct _SAMPLE_RESULT
{
	IMG_UINT32				auRawDataPerChunk[UF_MAX_CHUNKS_PER_TEXTURE];
} SAMPLE_RESULT, *PSAMPLE_RESULT;

typedef struct _MOE_DATA
{
	  enum
	  {
		 MOE_INCREMENT,    /* Increment mode */
		 MOE_SWIZZLE       /* Swizzle mode  */
	  } eOperandMode;

	  union
	  {
			IMG_INT32 iIncrement;    /* Amount of increment */
			struct
			{
				IMG_UINT32 auSwizzle[USC_MAX_SWIZZLE_SLOTS];    /* Swizzle values      */
				IMG_UINT32 uLargest;		/* Largest swizzle value */
			} s;
	  }u;
} MOE_DATA, *PMOE_DATA;

/* USC_MOE_DATA_DEFAULT_VALUE: The default MOE operand mode and increment */
#define USC_MOE_DATA_DEFAULT_VALUE {MOE_INCREMENT, {1}}

typedef struct _REGISTER_TYPE_USEDEF
{
	/*
		Bit vector with a bit for each channel of each register: A bit is set if the
		channel has been used/defined.
	*/
	USC_VECTOR	sChans;
	/*
		Bit vector with a bit for each register: A bit is set if the register has been
		used/defined as C10 format.
	*/
	USC_VECTOR	sC10;
	/*
		Bit vector with a bit for each register: A bit is set if the register has been
		used/defined as a non-C10 format.
	*/
	USC_VECTOR	sNonC10;
} REGISTER_TYPE_USEDEF, *PREGISTER_TYPE_USEDEF;

/* REGISTER_USEDEF: Register use-definition record */
typedef struct _REGISTER_USEDEF
{
	REGISTER_TYPE_USEDEF sTemp;
	REGISTER_TYPE_USEDEF sOutput;
	REGISTER_TYPE_USEDEF sPrimAttr;
	REGISTER_TYPE_USEDEF sIndex;
	REGISTER_TYPE_USEDEF sInternal;
	USC_VECTOR sPredicate;
	IMG_BOOL bLocalMemory;
	IMG_BOOL bInternalRegsClobbered;
} REGISTER_USEDEF, *PREGISTER_USEDEF;

/* USEDEF_RECORD: Combined use-definition record */
typedef struct _USEDEF_RECORD
{
		REGISTER_USEDEF sUse;
		REGISTER_USEDEF sDef;
} USEDEF_RECORD, *PUSEDEF_RECORD;

/*
	flag set in a block if it contains new MOVs that might be able to
	be eliminated by a pass of *global* move elimination (later, after
	integer opts have finished).
*/
#define NEED_GLOBAL_MOVE_ELIM			(1U << 0)
#if defined(TRACK_REDUNDANT_PCONVERSION)
	#define USC_CODEBLOCK_TRAVERSED		(1U << 1)
#endif
#define USC_CODEBLOCK_NEED_DEP_RECALC	(1U << 2)
#define USC_CODEBLOCK_CFG				(1U << 3)

struct tagINTERMEDIATE_STATE;

/* Enum of basic block types - i.e., how a given BB chooses amongst it's successors */
typedef enum
{
	/* CBYTPE_EXIT: last block in CFG - no successors, period. */
	CBTYPE_EXIT ,
	/* CBTYPE_UNCOND: Unconditional - block has single successor */
	CBTYPE_UNCOND,
	/* CBTYPE_COND: Conditional - chooses between two successors
	 * according to a boolean (predicate reg) */
	CBTYPE_COND,
	/* CBTYPE_SWITCH: Switch block - chooses between arbitrary number
	 * of successors according to a sparse mapping of 32-bit ints */
	CBTYPE_SWITCH,
	/*
		CBTYPE_CONTINUE: Represents the next iteration of a loop
		during sync-end set up.
	*/
	CBTYPE_CONTINUE,
	CBTYPE_UNDEFINED
} ICBTYPE;

typedef struct _CODEBLOCK_EDGE_
{
	/* Points to the flow control block linked to this one. */
	struct _CODEBLOCK_*	psDest;
	/* Entry in the destination block's predecessor or successor array. */
	IMG_UINT32			uDestIdx;
} CODEBLOCK_EDGE, *PCODEBLOCK_EDGE;

typedef struct _CODEBLOCK_
/******************************************************************************
	Represents a single Basic Block
******************************************************************************/
{
	/**** The following copied from existing CODEBLOCK structure for basic blocks ****/
	IMG_UINT32				uFlags;
	/* Number of instructions in a basic block */
	IMG_UINT32				uInstCount;
	/* Count of CALL instructions in a basic block */
	IMG_UINT32				uCallCount;
	/* List of delta instructions in the basic block. */
	USC_LIST				sDeltaInstList;

	/*
		Information about internal register usage in the block.
	*/
	PUSEDEF_CHAIN			apsIRegUseDef[MAXIMUM_GPI_SIZE_IN_SCALAR_REGS];

	/*
		Entry in the list INTERMEDIATE_STATE.sBlockList.
	*/
	USC_LIST_ENTRY			sBlockListEntry;

	PINST					psBody;
	PINST					psBodyTail;

#if defined(OUTPUT_USPBIN)
	IMG_BOOL				bEfoFmtCtl;
	IMG_BOOL				bColFmtCtl;
	MOE_DATA				asMoeIncSwiz[USC_MAX_MOE_OPERANDS];
	IMG_UINT32				auMoeBaseOffset[USC_MAX_MOE_OPERANDS];
#endif /* defined(OUTPUT_USPBIN) */

	REGISTER_LIVESET		sRegistersLiveOut;

	/**** Now the new(ish) stuff... ****/

	/* State used for dependency graphs. */
	IMG_BOOL				bIgnoreDesched;
	struct _DGRAPH_STATE_	*psDepState;
	struct _DGRAPH_STATE_	*psSavedDepState;
	
	/* The unique CFG containing this Basic Block */
	struct _CFG *psOwner;
	/*
	  INVARIANT: psOwner->apsAllBlocks[uIdx] == <this>
	  *except* during execution of SortBlocks, below.
	  */

	/* (Dense) index of basic block within owner (see prev. invariant). */
	IMG_UINT32 uIdx; 

	/* Global block index. */
	IMG_UINT32 uGlobalIdx;
	
	/* Number of control flow predecessors */
	IMG_UINT32 uNumPreds;

	/* Array of pointers to predecessors (all in same CFG) */
	PCODEBLOCK_EDGE asPreds;

	/* Number of control flow successors */
	IMG_UINT32 uNumSuccs;

	/* Array of pointers to successors */
	PCODEBLOCK_EDGE	asSuccs;

	/* Method this block uses to select which successor to use at runtime. */
	ICBTYPE eType;
	/*
	  Any relevant extra parameters this block uses to select its successor.
	  Also contains information on sync_end flag, initialised by SetupSyncInfo,
	  represented in a method-specific way
	*/
	union
	{
		/* If there is no choice among successors (eType == UNCONDITIONAL) */
		struct
		{
			/* Whether the sync_end flag is needed on the branch to this block's successor */
			IMG_BOOL bSyncEnd;
			struct _CFG *psCfg;
			/* No extra info required for choosing among successors! */
			/* INVARIANT: uNumSuccs == 1 */
		} sUncond;

		/* If based on a boolean predicate (i.e. eType == CONDITIONAL) */
		struct
		{
			/*
			  Whether the sync_end flag is needed on the branch to each successor:
			  use (uSyncEndBitMask & (1 >> i)) for apsSuccs[i].
			*/
			IMG_UINT32 uSyncEndBitMask;

			/* Predicate register to use; choose apsSuccs[0] if true, apsSuccs[1] if false */
			ARG			sPredSrc;

			/* USE record for the predicate register. */
			USEDEF		sPredSrcUse;

			#if defined(EXECPRED)

			/* Either to generate branch or not; no branch if true, branch if false */
			IMG_BOOL	bDontGenBranch;
			#endif /* #if defined(EXECPRED) */
			/* INVARIANT: uPredSrc != USC_PREDREG_NONE && uNumSuccs == 2 */
		} sCond;

		/* A switch, based on a sparse mapping of int32 values (i.e. eType == SWITCH) */
		struct
		{
			/* Array (uNumSuccs elements): whether sync_end flag is
			 * needed on branch to each successor */
			IMG_PBOOL pbSyncEnd;

			/* Register containing (at end of block) the value upon which to switch */
			PARG psArg;

			/* USE record for the switch register. */
			ARGUMENT_USEDEF	sArgUse;
			
			/*
			  Whether there is a default case for the switch. Note
			  that even if no "default" clause is specified, there
			  will usually be a default case (i.e. the block after
			  the end of the switch, also the target for "break"s) -
			  the exception is if we're certain the list of cases
			  specified is 100% exhaustive. [Thus - it may be
			  possible to omit this boolean, and just treat it as
			  always IMG_TRUE?]
			*/
			IMG_BOOL bDefault;

			/*
			  Array of values of argument to switch which cause
			  selection of corresponding successor.  If there is a
			  default, it is the *last* successor, and there is one
			  fewer case value than successors; otherwise, the
			  number of case values is exactly the number of
			  successors.
			*/
			IMG_PUINT32 auCaseValues;
		} sSwitch;
	} u;

	/* Whether this block is guaranteed to select the same successor
	 * for all pixels; UNDEFINED if (uNumSuccs == 1) */
	IMG_BOOL bStatic;

	/****
	 * The following four fields are initialised only when a dominator tree is built
	 ****/
	/* Immediate Dominator; NULL for entry node (i.e. iff psOwner->psEntry == <this> ) */
	struct _CODEBLOCK_ *psIDom;
	/* Immediate Postdominator */
	struct _CODEBLOCK_ *psIPostDom;
	/* Number of other nodes for which this is the immediate dominator */
	IMG_UINT32 uNumDomChildren;
	/* Array of pointers to dominator children (i.e. whose immediate
	 * dominator is this block) */
	struct _CODEBLOCK_ **apsDomChildren;

#if defined(REGALLOC_LSCAN) || defined(IREGALLOC_LSCAN)
	struct _CODEBLOCK_ *psLoopHeader;
#endif

	/****
	 * The following (along with sync_end component of u above)
	 * initialized only after SetupSyncInfo
	 ****/
	/* Closest postdominator OUTSIDE (i.e. of all nodes in) this node's dominator subtree */
	struct _CODEBLOCK_ *psExtPostDom;

	/* Whether this node contains or strictly dominates something needing a SYNC_START */
	IMG_BOOL bDomSync;
	/* Whether this node's dominator subtree contains a sync_end point (not this node itself) */
	IMG_BOOL bDomSyncEnd;

	/* Whether we need to add an extra SYNC_START at the beginning of
	 * this block to handle nested flow control */
	IMG_BOOL bAddSyncAtStart;
#if defined(TRACK_REDUNDANT_PCONVERSION)
	/* Temporary store for tracking redundant paths */
	IMG_UINT32		auPrecisionConversionPaths[2];

	/* Performance measures */
	IMG_UINT32		uInstructionCycleEstimate;
	IMG_UINT32		uWorstPathCycleEstimate;
	IMG_UINT32		uPathTotal;
	IMG_UINT32		uPathDivisions;
#endif
	/* Keep coarse track of instruction created */
	IMG_UINT32		uInstGroupCreated;
	/* The label number associated with this code block (to link JUMPs) */
	IMG_UINT32 uLabel;

	/**** Temporary variable FOR USE BY DoDataflow() ONLY ****/
	struct _CODEBLOCK_ *psWorkListNext;
} CODEBLOCK;

/* Routine for sorting the blocks of a CFG */
typedef IMG_VOID (*BLOCK_SORT_FUNC)(struct tagINTERMEDIATE_STATE *psState, struct _CFG *psCfg);
#define ANY_ORDER ((BLOCK_SORT_FUNC)NULL)

typedef struct _FUNC_INOUT
{
	IMG_UINT32		uType;
	IMG_UINT32		uNumber;
	UF_REGFORMAT	eFmt;
	IMG_UINT32		uChanMask;
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	IMG_BOOL		bVector;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
} FUNC_INOUT, *PFUNC_INOUT;

typedef struct _FUNC_INOUT_ARRAY
{
	IMG_UINT32		uCount;
	PFUNC_INOUT		asArray;
	PUSEDEF			asArrayUseDef;
} FUNC_INOUT_ARRAY, *PFUNC_INOUT_ARRAY;

/*
	Represents a block that has no successor because it ends with a jump
	to a block that hasn't been added to the CFG yet.
*/
typedef struct _UNLINKEDJUMP_
{
	PCODEBLOCK psBlock;
	IMG_UINT32 uLabel;
} UNLINKEDJUMP, PUNLINKEDJUMP;

typedef struct _CFG
/******************************************************************************
	Represents a control flow graph, i.e. a connected collection of basic 
	blocks including a single entry node and exit node.
******************************************************************************/
{
	/*
		The entry node; dominates all nodes in flowgraph. Any predecessors
		must be by loop backedges.
	*/
	PCODEBLOCK psEntry;

	/*
	    The exit node; may have 0 instructions, but exists as a target for jumps
    	representing early exits. TODO eType ??
    */
	PCODEBLOCK psExit;

	/* Number of basic blocks in this CFG */
	IMG_UINT32 uNumBlocks;

	/* Array of pointers to the blocks. */
	PCODEBLOCK *apsAllBlocks;
	/*
		INVARIANT: 0<=i<uNumBlocks --> apsAllBlocks[i]->uIdx == i,
		*except* during execution of SortBlocks (see below).
	*/

	/*
		The sort function according to which the blocks (+indices) within this CFG
		are currently sorted, if any. Note that if bBlockStructureChanged is set, this
		field can be considered null (i.e. we'll do a MergeBB's and resort before doing
		anything depending on the current sort order).
	*/
	BLOCK_SORT_FUNC pfnCurrentSortOrder;

	/*
		We say the block structure has changed if
			-any blocks have been added or removed
			-there have been any changes to the edges between the blocks
			-dominators or postdominators need recalculating
	*/
	IMG_BOOL bBlockStructureChanged;

	/*
		TRUE if some blocks have had all their instructions removed.
	*/
	IMG_BOOL bEmptiedBlocks;
	struct _FUNC_* psFunc;
} CFG, *PCFG;

typedef struct _FUNC_
/******************************************************************************
	Represents a single function: i.e.  a target for CALL instructions and/or an
	entry point. The body of the function is represented as a control flow graph,
	i.e. a connected collection of basic blocks including a single entry node and
	exit node.
******************************************************************************/
{
	CFG sCfg;
	/*
		Pointer to a PINST calling this; all such calls can be reached as
		<PINST>(->psCallInfo->psCallSiteNext)*
	*/
	PINST psCallSiteHead;
	/* INVARIANT: psCallSiteHead->eOpcode == ICALL &&
	 * psCallSiteHead->psCallInfo->psTarget == <this> */
	/*
		External name by which this function may be used as an entry point, if any. (ACL!)
		If null, function is used internally only (and may be inlined, deleted if
		empty or there are no CALLs to it, etc.); if non-null, function must be preserved
		for external callers (or e.g. as the main shader program!!).
	*/
	IMG_PCHAR pchEntryPointDesc;

	//IMG_BOOL bContainsSync => use sBody.pHead->bDomSync

	/* Unique function label */
	IMG_UINT32 uLabel;

	/* During the CFG build step, this gets updated for each forward JUMP */
	UNLINKEDJUMP *asUnlinkedJumps;
	IMG_UINT32 uUnlinkedJumpCount;

	/* Maximum depth of CALLs within/reachable from function body */
	IMG_UINT32 uNestingLevel;

	/* During input conversion: the register number of the function parameter for the D3D loop index. */
	IMG_UINT32 uD3DLoopIndex;

	/*
		Inputs to the function.
	*/
	FUNC_INOUT_ARRAY	sIn;

	/*
		Outputs from the function.
	*/
	FUNC_INOUT_ARRAY	sOut;

	/*
		Count of the number of CALL instructions in the block.
	*/
	IMG_UINT32 uCallCount;

	/*
		Linked list of functions in nesting order... note/TODO we never update this
		(after initial construction!). But nonetheless the essential invariant,
		that [f calls g] ==> [f is more "outer" than g] is maintained.
	*/
	struct _FUNC_ *psFnNestInner, *psFnNestOuter;

	REGISTER_LIVESET sCallStartRegistersLive;

#if defined(TRACK_REDUNDANT_PCONVERSION)
	/* Performance measures */
	IMG_UINT32	uWorstPathCycleEstimate;
	IMG_UINT32	uPathTotal;
	IMG_FLOAT	fAverageCycleCount;
#endif
} FUNC;

typedef FUNC USC_FUNCTION;
typedef PFUNC PUSC_FUNCTION;

/*
  Callbacks for liveness analysis
 */

/* Point at which callback is invoked */
typedef enum _USC_LCB_POS_
{
	USC_LCB_ENTRY,     // Called on entry
	USC_LCB_EXIT,      // Called on exit
	USC_LCB_PROC,      // Called for processing
	USC_LCB_POS_UNDEF
} USC_LCB_POS;

/* Type of operand passed to a liveness callback */
typedef enum _USC_LCB_OPERAND_TYPE_
{
	USC_LCB_DEST,		// Destination  (pvRegData is offset from base destination)
	USC_LCB_OLDDEST,	// Old value of partially written destination
	USC_LCB_SRC,		// Source       (pvRegData is argument index)
	USC_LCB_PRED,		// Predicate
	USC_LCB_INDEX,		// Index register
	USC_LCB_SWITCHARG,
	USC_LCB_TYPE_UNDEF
} USC_LCB_OPERAND_TYPE;

typedef struct _LIVE_CALLBACK_ USC_LIVE_CALLBACK, *PUSC_LIVE_CALLBACK;

typedef IMG_VOID (*LIVE_CALLBACK_FN)(USC_LIVE_CALLBACK*);

struct _LIVE_CALLBACK_
{
	LIVE_CALLBACK_FN pfFn;    // Call back function
	IMG_PVOID pvFnData;       // Data passed to call back function.

	IMG_BOOL bIsBlock;        // Block (IMG_TRUE) or instruction (IMG_FALSE)
	USC_LCB_POS ePos;         // Start or end of processing a block or instruction
	PCODEBLOCK psBlock;			  //Block, or block containing instruction, to process

	/* Instruction level data */
	PINST psInst;            // Instruction
	IMG_BOOL bDest;          // Destination or source
	USC_LCB_OPERAND_TYPE eRegType;	// The kind of operand
	IMG_PVOID pvRegData;     // Data interpreted according to eRegType (default: register number)

	/* Codeblock level data */
	IMG_UINT32 uBranch;             // 0: Sequential blocks, >0: n'th branch of a conditional
};

#define UNIFLEX_VSOUTPUT_CLIPPOS			(EURASIA_USE_OUTPUT_BANK_SIZE)

#define USC_FLAGS_INPUTRELATIVEADDRESSING			(1U << 0)
#define USC_FLAGS_VSCLIPPOS_USED					(1U << 1)
#define USC_FLAGS_INDEXABLETEMPS_USED				(1U << 2)
#define USC_FLAGS_SPLITFEEDBACKCALC					(1U << 3)
#define USC_FLAGS_DEPENDENTREADS					(1U << 4)
#define USC_FLAGS_INTEGERUSED						(1U << 5)
#define USC_FLAGS_INTEGERINLASTBLOCKONLY			(1U << 6)
#define USC_FLAGS_MOESTAGE							(1U << 7)
#define USC_FLAGS_POSTC10REGALLOC					(1U << 8)
#define USC_FLAGS_PRIMARYCONSTLOADS					(1U << 9)
#define USC_FLAGS_POSTINDEXREGALLOC					(1U << 10)		/* Are we after the index register allocation step. */
#define USC_FLAGS_DEPTHFEEDBACKPRESENT				(1U << 11)		/* Program contains depth feedback. */
#define USC_FLAGS_OMASKFEEDBACKPRESENT				(1U << 12)		/* Program contains oMask feedback. */
#define USC_FLAGS_INITIALISE_TEXKILL				(1U << 13)		/* Texkill predicate register needs to be initialized
																       at the start of the program */
#define USC_FLAGS_TEXKILL_PRESENT					(1U << 14)		/* Program contains a texkill. */
#define USC_FLAGS_HAS_EARLY_EXIT					(1U << 15)		/* The program (not a sub-routine) has an early exit. */

#define USC_FLAGS_POSTCONSTREGPACK					(1U << 16)		/* Are we after the constant register packing step. */

#define USC_FLAGS_INTERMEDIATE_CODE_GENERATED		(1U << 17) /*Have we executed FinaliseIntermediateCode(psState)?*/

#ifdef SRC_DEBUG
#define USC_FLAGS_REMOVING_INTERMEDIATE_CODE		(1U << 18)
#define USC_FLAGS_NOSCHED_FLAG						(1U << 19)
#endif /* SRC_DEBUG */

#if defined(OUTPUT_USPBIN)
#define USC_FLAGS_RESULT_REFS_NOT_PATCHABLE				(1U << 20)
#define USC_FLAGS_RESULT_REFS_NOT_PATCHABLE_WITH_PAS	(1U << 21)
#endif /* defined(OUTPUT_USPBIN) */

#define USC_FLAGS_REPLACEDVECTORREGS				(1U << 22)

/*
	For a pixel shader with the INTPACK flag: does the main function
	have multiple RETs.
*/
#define USC_FLAGS_MULTIPLERETSFROMMAIN				(1U << 23)

/*
	Are the shader outputs written using dynamic indexing.
*/
#define USC_FLAGS_OUTPUTRELATIVEADDRESSING			(1U << 24)

/*
	Have the intermediate instructions been updated to fix BRN21752.
*/
#define USC_FLAGS_APPLIEDBRN21752_FIX				(1U << 25)

/*
	Have the intermediate instructions been updated to fix unsupported
	write masks on vector instructions.
*/
#define USC_FLAGS_FIXEDVECTORWRITEMASKS				(1U << 26)

/*
	Has register allocation been done on the secondary update program?
*/
#define USC_FLAGS_ASSIGNEDSECPROGREGISTERS			(1U << 27)

/*
	Compile input source for USPBIN output
*/
#define USC_FLAGS_COMPILE_FOR_USPBIN				(1U << 28)

/*
	Initialise regs on first write
*/
#define USC_FLAGS_INITIALISEREGSONFIRSTWRITE		(1U << 29)

/*
	Do any EFO instructions have sources in F16 format? This flag is an
	approximation to guide hardware register number assignment.
*/
#define USC_FLAGS_EFOS_HAVE_F16_SOURCES				(1U << 30)

/*
	Have secondary attributes been assigned hardware register numbers.
*/
#define USC_FLAGS_ASSIGNED_SECATTR_REGNUMS			(1U << 31)

#define USC_FLAGS2_ASSIGNED_TEMPORARY_REGNUMS		(1U << 0)

#define USC_FLAGS2_SPLITCALC						(1U << 1)

#define USC_FLAGS2_SETUP_REGISTER_GROUPS			(1U << 2)

#define USC_FLAGS2_TEMP_USE_DEF_INFO_VALID			(1U << 3)

#define USC_FLAGS2_PRED_USE_DEF_INFO_VALID			(1U << 4)

#define USC_FLAGS2_SSA_FORM							(1U << 5)

#define USC_FLAGS2_ASSIGNED_PRIMARY_REGNUMS			(1U << 6)

#define USC_FLAGS2_NO_VECTOR_REGS					(1U << 7)

#define USC_FLAGS2_TWO_PARTITION_MODE				(1U << 8)

/*
	Set if intermediate predicate registers have been assigned to hardware predicate
	registers.
*/
#define USC_FLAGS2_ASSIGNED_PREDICATE_REGNUMS		(1U << 9)

#define USC_FLAGS2_USES_MUTEXES						(1 << 10)

/*
	Don't append newly created intermediate registers to the list of registers with deleted
	uses.
*/
#define USC_FLAGS2_IN_SSA_RENAME_PASS				(1 << 11)

/*
	Set if the input program used relative addressing into the pixel shader outputs.
*/
#define USC_FLAGS2_PSOUTPUTRELATIVEADDRESSING		(1 << 12)

typedef struct _DGRAPH_STATE_
{
	/* psState: Compiler state */
	struct tagINTERMEDIATE_STATE* psState;

	/* uBlockInstructionCount: Number of instructions in the block. */
	IMG_UINT32	uBlockInstructionCount;
	/*
	  psInstructions: Array of instructions for which graph is built, in the
	  order they occur in the program. Instruction references in the dgraph
	  are indices into this array. When dgraph is first set-up, instruction
	  field uId is set to the index of the instruction in this array.
	*/
	USC_PARRAY psInstructions;

	/*
		ArrayGet(psRLastWriter, x): instructions reading and/or writing
		register x of type R. (Used internally by ComputeDependencyGraph, only)
	*/
	USC_PARRAY	psTempUsers;
	USC_PARRAY	psPAUsers;
	USC_PARRAY	psOutputUsers;
	USC_PARRAY	psIRegUsers;  /* Was FpInternalLastWriter */
	USC_PARRAY	psPredUsers;	/* Was PredicateLastWriter */
	USC_PARRAY	psIndexUsers;

	/* ArrayGet(apsRegArrayLastWriter[x],e): the last inst to write elem e of reg array x */
	USC_PARRAY  *apsRegArrayLastWriter;

	/*
		ArrayGet(psMainDep, y): One of USC_MAINDEPENDENCY_NONE,
		USC_MAINDEPENDENCY_MULITPLE or x where x is the only instruction which
		depends on instruction y.
	*/
	USC_PARRAY psMainDep;
	/*
	    psDepGraph:
		x depends on y iff GraphGet(psDepGraph, x, y) == true.
	*/
	USC_PGRAPH psDepGraph;
	/*
	    psDepGraph:
		x (depends on)+ y iff GraphGet(psDepGraph, x, y) == true.
	*/

	USC_PGRAPH psClosedDepGraph;
	/*
		ArrayGet(psDepCount,x): The number of instructions that instruction x depends on.
	*/
	USC_PARRAY psDepCount;
	/*
		ArrayGet(psSatDepCount, x): Number of instructions which depended on
		x but have been removed from the graph (because they are no longer needed).
	*/
	USC_PARRAY psSatDepCount;
	/*
		uAvailInstCount: Number of instructions which have no dependencies.

		uRemovedInstCount: Number of instructions which have been removed from the graph.
	*/
	IMG_UINT32	uAvailInstCount, uRemovedInstCount;
	/*
		ArrayGet(psDepList, x): List of instructions which depend on x.
	*/
	USC_PARRAY psDepList;

	/*
		List of instructions which have no dependencies.
	*/
	USC_LIST	sAvailableList;
} DGRAPH_STATE, *PDGRAPH_STATE;

#define USC_MAINDEPENDENCY_NONE			(~0U)
#define USC_MAINDEPENDENCY_MULTIPLE		(~1U)

typedef enum _ARRAY_TYPE
{
	ARRAY_TYPE_NORMAL,
	ARRAY_TYPE_VERTICES_BASE,
	ARRAY_TYPE_TEXTURE_COORDINATE,
	ARRAY_TYPE_VERTEX_SHADER_INPUT,
	ARRAY_TYPE_VERTEX_SHADER_OUTPUT,
	ARRAY_TYPE_DRIVER_LOADED_SECATTR,
	ARRAY_TYPE_DIRECT_MAPPED_SECATTR
} ARRAY_TYPE;

/* USC_VEC_ARRAY_DATA: Information about vector (indexable temp) arrays */
typedef struct _USC_VEC_ARRAY_DATA_
{
	IMG_UINT32 uArrayTag;		// Tag of array put into (temporary) registers
	IMG_UINT32 uSize;			// Size in bytes
	IMG_UINT32 uVecs;			// Size in vectors
	IMG_UINT32 uLoads;			// Number of loads from this array
	IMG_UINT32 uStores;			// Number of stores from this array
	UF_REGFORMAT eFmt;			// Format of the array
	IMG_UINT32 uRegArray;	 	// Register array number (> USC_NUM_REGARRAYS, if in memory)
	IMG_UINT32 uBaseTempNum;	// Base of the allocated temporaries (for static arrays in registers)
	IMG_BOOL bStatic;           // All accesses are statically indexed
	IMG_BOOL bInRegs;           // Array is in registers
} USC_VEC_ARRAY_DATA, *PUSC_VEC_ARRAY_DATA;

/* USC_VEC_ARRAY_REG: Information about vector (indexable temp) arrays put into registers*/
typedef struct _USC_VEC_ARRAY_REG_
{
	IMG_UINT32 uArrayNum;		// Number of array put into (temporary) registers
	IMG_UINT32 uRegType;		/*
									Type of the registers in the array. Always USEASM_REGTYPE_TEMP
									before register allocation.
								*/
	IMG_UINT32 uChannelsPerDword;	/*
										Number of channels which will fit in a dword for elements of the
										array.
									*/
	IMG_UINT32 uBaseReg;		// Register number in position 0 of the array.
	IMG_UINT32 uRegs;			// Number of registers
	ARRAY_TYPE eArrayType;		// Array type.
	struct _USEDEF_CHAIN* psUseDef;		// Uses/defines of the array base
	struct _USEDEF_CHAIN* psBaseUseDef;	// Uses of the register representing the (unknown) register number
										// of the base of the array.
	struct _REGISTER_GROUP* psGroup;	// Register grouping information for the first register in the array
	union
	{
		struct _CONSTANT_INREGISTER_RANGE*	psConstRange;	 // Data for ARRAY_TYPE_DRIVER_LOADED_SECATTR
		struct _TEXTURE_COORDINATE_ARRAY*	psTextureCoordinateArray; // Data for ARRAY_TYPE_TEXTURE_COORDINATE
		UF_REGFORMAT						eDirectMappedFormat; // Data for ARRAY_TYPE_DIRECT_MAPPED_SECATTR
		IMG_PVOID							pvNULL;
	} u;						// Points to array-type specific data.
} USC_VEC_ARRAY_REG, *PUSC_VEC_ARRAY_REG;
#define USC_VEC_ARRAY_REG_DEFAULT { 0, 0, 0, 0, 0, IMG_FALSE }

#define USC_ALLOC_INFO_SIZE_IN_BYTES 80

typedef struct _USC_ALLOC_HEADER
{
	#if defined(DEBUG)
	/*
		Size of the allocated block (excluding the header). Used to update the current
		memory usage when freeing a block.
	*/
	IMG_UINT32					uSize;
	/*
		Unique ID for this allocation.
	*/
	IMG_UINT32					uAllocNum;
	#endif /* defined(DEBUG) */
#ifdef USC_COLLECT_ALLOC_INFO
	IMG_CHAR					acAllocInfo[USC_ALLOC_INFO_SIZE_IN_BYTES];
#endif
	/*
		Entry in the list of all allocated blocks.
	*/
	struct _USC_ALLOC_HEADER*	psPrev;
	struct _USC_ALLOC_HEADER*	psNext;
} USC_ALLOC_HEADER, *PUSC_ALLOC_HEADER;

typedef struct _INPUT_PROGRAM
{
	PUNIFLEX_INST		psHead;
	PUNIFLEX_INST		psTail;
} INPUT_PROGRAM, *PINPUT_PROGRAM;

#if defined(OUTPUT_USPBIN)
#define MAX_ALTERNATE_RESULT_BANK_TYPES				(2)
#endif

/*
	Data for a constant we are asking the driver to
	load into a secondary attribute.
*/
typedef struct _INREGISTER_CONST
{
	/*
		Entry in the list of in-register constants in increasing
		order of register numbers.
	*/
	USC_LIST_ENTRY			sListEntry;
	/*
		Number of the input constant to load.
	*/
	IMG_UINT32				uNum;
	/*
		Format to convert the input constant to.
	*/
	UNIFLEX_CONST_FORMAT	eFormat;
	/*
		Buffer of the input constant to load.
	*/
	IMG_UINT32				uBuffer;

	/*
		Describes the temporary variable containing the constant.
	*/
	struct _SAPROG_RESULT*	psResult;
} INREGISTER_CONST, *PINREGISTER_CONST;

/*
	The type of usage of a shader result in the pre-feedback driver epilog.
*/
typedef enum _FEEDBACK_USE_TYPE
{
	/*
		This shader result isn't used in the pre-feedback driver epilog.
	*/
	FEEDBACK_USE_TYPE_NONE,
	/*
		This shader result is used both in the pre-feedback driver epilog and
		the epilog after the shader has completely finished.
	*/
	FEEDBACK_USE_TYPE_BOTH,
	/*
		This shader result is used in pre-feedback driver epilog only.
	*/
	FEEDBACK_USE_TYPE_ONLY
} FEEDBACK_USE_TYPE, *PFEEDBACK_USE_TYPE;

/*
  FIXED_REG_DATA: Description of virtual registers which need a specific hardware register.

  The virtual registers are a set of size uConsecutiveRegsCount with type
  uVRegType (which can be either USEASM_REGTYPE_TEMP or USEASM_REGTYPE_PREDICATE) and have
  register numbers {auVRegNum[0], auVRegNum[1], ..., auVRegNum[uConsecutiveRegCount - 1]}

  The hardware registers are a set of size uConsecutiveRegsCount with type sPReg.uType
  and numbers {sPReg.uNumber + 0, sPReg.uNumber + 1, ..., sPReg.uNumber + uConsecutiveRegsCount - 1}.
*/
typedef struct _FIXED_REG_DATA_
{
	/* uVRegType: Type of virtual registers. */
	IMG_UINT32				uVRegType;
	/* auVRegNum: Numbers of the virtual registers. */
	IMG_PUINT32				auVRegNum;
	/* Format of the data in the virtual registers. */
	PUF_REGFORMAT			aeVRegFmt;
	/* USE/DEF records for the virtual registers. */
	PUSEDEF					asVRegUseDef;
	/* sPReg: Allocated hardware (physical) register */
	ARG						sPReg;
	/* Mask of channels used from the output in each register. */
	IMG_PUINT32				auMask;
	/*
		Is this register(s) live at the end of Shader.
	*/
	IMG_BOOL				bLiveAtShaderEnd;
	/*
		Type of usage of this shader result in the pre-feedback driver epilog.
	*/
	PFEEDBACK_USE_TYPE		aeUsedForFeedback;
	/*
		If TRUE: the register is associated with the primary program.
		If FALSE: the register is associated with the secondary update program.
	*/
	IMG_BOOL				bPrimary;
	/*
		Count of consecutive registers that need fixed hardware registers.
	*/
	IMG_UINT32				uConsecutiveRegsCount;
	/*
		If non-NULL records the channels live in this fixed register at the
		start of the program.
	*/
	IMG_PUINT32				puUsedChans;
	/*
		Where the fixed registers are accessed with dynamic indices the
		corresponding entry in the array representing groups of dynamically
		indexed registers.
	*/
	IMG_UINT32				uRegArrayIdx;
	/*
		Offset into the register array corresponding to this fixed register.
	*/
	IMG_UINT32				uRegArrayOffset;
	#if defined(OUTPUT_USPBIN)
	/*
		Possible alternate hardware registers.
	*/
	IMG_UINT32				uNumAltPRegs;
	PARG					asAltPRegs;
	/*
		If TRUE include the hardware registers used for these fixed variables
		in the sProgDesc.uTempRegCount count passed to the USP.
	*/
	IMG_BOOL				bUpdateCount;
	#endif /* defined(OUTPUT_USPBIN) */
	/*
		Entry in the global list of fixed registers.
	*/
	USC_LIST_ENTRY			sListEntry;
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		Is the source/destination of the fixed register a vector sized register.
	*/
	IMG_BOOL				bVector;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	/*
		Unique ID for this shader input/output.
	*/
	IMG_UINT32				uGlobalId;
} FIXED_REG_DATA;

typedef struct _PIXELSHADER_INPUT
{
	/*
		Structure we will return to the caller describing the iteration/texture load.
	*/
	UNIFLEX_TEXTURE_LOAD	sLoad;

	/*
		Number of attributes written with the result of the iteration.
	*/
	IMG_UINT32				uAttributeSizeInDwords;

	/*
		Format of the result of the iteration.
	*/
	UF_REGFORMAT			eResultFormat;

	/*
		Flags (see PIXELSHADER_INPUT_FLAG_xxx below)
	*/
	IMG_UINT32				uFlags;

	/*
		Points to the entry in the array of intermediate registers to colour to fixed
		hardware registers corresponding to the results of the iterations.
	*/
	PFIXED_REG_DATA			psFixedReg;

	/*
		Entry in the list of pixel shader inputs.
	*/
	USC_LIST_ENTRY			sListEntry;
} PIXELSHADER_INPUT, *PPIXELSHADER_INPUT;

/*
	Flags for pixel shader input data (above)
*/
#define PIXELSHADER_INPUT_FLAG_OUTPUT_PADDING		(1 << 0)
#define PIXELSHADER_INPUT_FLAG_PART_OF_ARRAY		(1 << 1)
#define PIXELSHADER_INPUT_FLAG_NDR_IS_VECTOR		(1 << 2)

/*
	Information about the hardware registers available to the shader.
*/
typedef struct _HWREGISTER_LIMITS
{
	/* Number of temporary registers available. */
	IMG_UINT32	uNumTemporaryRegisters;
	/* Number of primary attributes available. */
	IMG_UINT32	uNumPrimaryAttributes;
	/* Number of output registers available. */
	IMG_UINT32	uNumOutputRegisters;
} HWREGISTER_LIMITS, PHWREGISTER_LIMITS;

/*
	Flag for entries in CONSTANT_BUFFER.psRemappedMap. If set then C10 constant data is
	unpacked to one channel per word.
*/
#define REMAPPED_CONSTANT_UNPACKEDC10						(0x80000000)

typedef struct _CONSTANT_BUFFER
{
	/* Remapped constant table. */
	IMG_UINT32	uRemappedCount;
	USC_PARRAY	psRemappedMap;
	USC_PARRAY	psRemappedFormat;

	/*
		List of LOADCONST instructions loading constants from this constant buffer.
		Sorted by offset.
	*/
	USC_LIST	sLoadConstList;

	/*
		Secondary attribute holding the base address of the
		constant buffer in memory.
	*/
	ARG			sBase;

	/*
		Type and number of the register used for range checking for each constant buffer.
	*/
	IMG_UINT32	uRangeCheckType;
	IMG_UINT32	uRangeCheckNumber;

	/* Keeps track of constant buffers in use. */
	IMG_BOOL	bInUse;
} CONSTANT_BUFFER, *PCONSTANT_BUFFER;

typedef struct _TEXTURE_COORDINATE_ARRAY
{
	/*
		Index of the register array.
	*/
	IMG_UINT32			uRegArrayIdx;
	/*
		Array of pointers to each iteration whose result is part of
		the array.
	*/
	PPIXELSHADER_INPUT*	apsIterations;
	/*
		Points to the user supplied start and end of the range of dynamically
		indexed pixel shader inputs.
	*/
	PUNIFLEX_RANGE		psRange;
} TEXTURE_COORDINATE_ARRAY, *PTEXTURE_COORDINATE_ARRAY;

typedef struct _PIXELSHADER_STATE
{
	/*
		Intermediate temporary register containing the Z shader result in the format used
		by the shader.
	*/
	IMG_UINT32					uNativeZTempRegNum;

	/*
		Precision/format used for the Z shader result in the shader.
	*/
	UF_REGFORMAT				eZFormat;

	/*
		Intermediate temporary register containing the coverage mask shader result in the format used
		by the shader.
	*/
	IMG_UINT32					uNativeOMaskTempRegNum;

	/*
		Precision/format used for the coverage mask shader result in the shader.
	*/
	UF_REGFORMAT				eOMaskFormat;

	/*
		Start of the intermediate temporary registers containing the shader colour results.
	*/
	IMG_UINT32					uNativeColourResultBaseTempRegNum;

	/*
		Precision/format used in the shader for the colour result for each surface.
	*/
	UF_REGFORMAT				aeColourResultFormat[UNIFLEX_MAX_OUT_SURFACES];

	/*
		Intermediate temporary register containing the Z shader result converted to F32.
	*/
	IMG_UINT32					uZTempRegNum;

	/*
		Intermediate temporary register containing the coverage mask shader result converted to F32.
	*/
	IMG_UINT32					uOMaskTempRegNum;

	/*
		Start of the intermediate temporary registers containing the shader colour results converted to
		the format required by the hardware.	
	*/
	IMG_UINT32					uColourResultBaseTempRegNum;

	/* Non-dependant texture accesses and iterations. */
	IMG_UINT32					uNrPixelShaderInputs;
	USC_LIST					sPixelShaderInputs;

	#if defined(OUTPUT_USPBIN)
	/*
		For each texture coordinate: a pointer to an iteration of that coordinate
		with an F32 result if one has been generated.
	*/
	PPIXELSHADER_INPUT			apsF32TextureCoordinateInputs[USC_MAXIMUM_TEXTURE_COORDINATES];
	#endif /* defined(OUTPUT_USPBIN) */

	/*
		Length of initial segment of the primary attributes which can be accessed by dynamic
		indices.
	*/
	IMG_UINT32					uIndexedInputRangeLength;

	/*
		Mask of the render targets that are written in the current program.
	*/
	IMG_UINT32					uEmitsPresent;

	/*
		Next primary attribute register available to be used.
	*/
	IMG_UINT32					uIterationSize;

	PFIXED_REG_DATA				psFixedHwPARegReg;

	/*
		Points to the entry in the fixed register list for
		the shader colour result.
	*/
	PFIXED_REG_DATA				psColFixedReg;

	/*
		Points to the entry in the fixed register list for
		the shader texkill result.
	*/
	PFIXED_REG_DATA				psTexkillOutput;

	/*
		Points to the entry in the fixed register list for
		the shader coverage mask result.
	*/
	PFIXED_REG_DATA				psOMaskOutput;

	/*
		Points to the entry in the fixed register list for
		the shader depth result.
	*/
	PFIXED_REG_DATA				psDepthOutput;

	/*
		Offset within the registers holding the shader colour
		result of the register with the alpha test input.
	*/
	IMG_UINT32					uAlphaOutputOffset;

	/*
		Points to the entry in the fixed register list for the
		extra primary attributes reserved for the driver's
		iterations.
	*/
	PFIXED_REG_DATA				psExtraPAFixedReg;

	#if defined(OUTPUT_USPBIN)
	/*
		Index in the alternate hardware registers for the colour output of
		the alternate register in the temporary bank.
	*/
	IMG_UINT32					uAltTempFixedReg;
	/*
		Index in the alternate hardware registers for the colour output of
		the alternate register in the primary attribute bank.
	*/
	IMG_UINT32					uAltPAFixedReg;
	#endif /* defined(OUTPUT_USPBIN) */

	/*
		For a pixel shader with the PACKPSOUTPUT flag: the mask of
		channels used from the shader output.
	*/
	IMG_UINT32					uInputOrderPackedPSOutputMask;
	IMG_UINT32					uHwOrderPackedPSOutputMask;

	/*
		Index of the input temporary register number containing the
		pixel shader output in packed form.
	*/
	IMG_UINT32					uPackedPSOutputTempRegNum;

	/*
		Details of the input format register holding the 
		pixel shader output in the original format.
	*/
	UF_REGTYPE					ePSOutputRegType;
	IMG_UINT32					uPSOutputRegNum;
	IMG_UINT32					uPSOutputRegArrayTag;

	/*
		Count of outputs used in a pixel shader.
	*/
	IMG_UINT32					uColOutputCount;

	/*
		Count of arrays representing dynamically accessed texture
		coordinates.
	*/
	IMG_UINT32					uTextureCoordinateArrayCount;

	/*
		Information about each of the arrays representing dynamically accessed texture
		coordinates.
	*/
	PTEXTURE_COORDINATE_ARRAY	asTextureCoordinateArrays;

	/*
		Number of PDS constants available in the primary program.
	*/
	IMG_UINT32					uNumPDSConstantsAvailable;

	/*
		Number of PDS constants required for a non-dependent texture sample
		on this core.
	*/
	IMG_UINT32					uNumPDSConstantsPerTextureSample;

	/*
		Number of registers available after split point.
	*/
	IMG_UINT32					uPostSplitRate;
} PIXELSHADER_STATE, *PPIXELSHADER_STATE;


typedef struct _VERTEXSHADER_STATE
{
	/*
		Points to the entry in array of fixed registers representing the vertex
		shader inputs.
	*/
#ifdef NON_F32_VERTEX_INPUT
	IMG_UINT32					uVertexShaderInputCount;
	PFIXED_REG_DATA				aVertexShaderInputsFixedRegs[16];
#else
	PFIXED_REG_DATA				psVertexShaderInputsFixedReg;
#endif

	/*
		Index of the register array representing the vertex shader outputs or USC_UNDEF if
		the vertex shader outputs are only statically accessed.
	*/
	IMG_UINT32					uVertexShaderOutputsArrayIdx;

	/*
		Start of the range of registers representing the vertex shader outputs.
	*/
	IMG_UINT32					uVertexShaderOutputsFirstRegNum;

	/*
		Points to the entry in the array of fixed registers representing the vertex
		shader outputs.
	*/
	PFIXED_REG_DATA				psVertexShaderOutputsFixedReg;

	/*
		Maximum output used in a vertex shader.
	*/
	IMG_UINT32					uVertexShaderNumOutputs;

	/*
		Bitmask indicating VS input PA-register usage
	*/
	IMG_UINT32					uVSInputPARegCount;
	IMG_PUINT32					auVSInputPARegUsage;

	/*
		Index of Internal Array created for Vetices Base.
	*/
	IMG_UINT32					uVerticesBaseInternArrayIdx;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		Maps vertex shader output register numbers in the input program
		to the temporary register number representing them in the input.

		All components of a vector register which have been read or
		written together in same instruction map to the same
		temporary register.
	*/
	IMG_PUINT32					auVSOutputToVecReg;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
} VERTEXSHADER_STATE, *PVERTEXSHADER_STATE;

typedef enum _SAPROG_RESULT_TYPE
{
	SAPROG_RESULT_TYPE_UNDEF			= 0,
	/*
		This result is calculated by USSE instructions.
	*/
	SAPROG_RESULT_TYPE_CALC				= 1,
	/*
		This result is a user-constant or static buffer we have asked the
		driver to load into a secondary attribute.
	*/
	SAPROG_RESULT_TYPE_DRIVERLOADED		= 2,
	/*
		This result is part of a constant buffer mapped directly to secondary attributes.
	*/
	SAPROG_RESULT_TYPE_DIRECTMAPPED		= 3
} SAPROG_RESULT_TYPE, *PSAPROG_RESULT_TYPE;

typedef enum _USC_COMPILE_STAGE_TYPE
{
	/*
		To indicate compilation requires determine which textures should not be sampled using
		anisotropic filtering
		Affects - DetermineNonAnisoTextureStagesBP
	*/
	USC_COMPILE_HINT_DETERMINE_NONANISO_TEX	= (IMG_INT32)(0x00000001),
	/*
		To indicate compilation requires optimization for all USP pseudo samples in the block.
		Affects - OptimiseUSPSamplesBP
	*/
	USC_COMPILE_HINT_OPTIMISE_USP_SMP		= (IMG_INT32)(0x00000002),
	/*
		To indicate Local load store instructions were used.
		Affects - InsertWaitsBP
	*/
	USC_COMPILE_HINT_LOCAL_LOADSTORE_USED	= (IMG_INT32)(0x00000004),
	/*
		To indicate blocks with switch exists
		Affects - ConvertSwitchBP
	*/
	USC_COMPILE_HINT_SWITCH_USED			= (IMG_INT32)(0x00000008),
	/*
		To indicate absolute store instructions have been used.
	*/
	USC_COMPILE_HINT_ABS_STORE_USED			= (IMG_INT32)(0x00000010),

	USC_COMPILE_STAGE_ALL					= (IMG_INT32)(0xFFFFFFFF)
} USC_COMPILE_STAGE_TYPE;


typedef struct _SAPROG_RESULT
{
	/*
		Entry in the list of variables needing fixed
		hardware registers in the secondary update
		program.
	*/
	PFIXED_REG_DATA			psFixedReg;

	/*
		Format of this result.
	*/
	UF_REGFORMAT			eFmt;

	/*
		Specific data associated with the result.
	*/
	union
	{
		struct _UNIFORM_SECATTR*	psRegAllocData;
	} u;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		TRUE if this result is a vector quantity.
	*/
	IMG_BOOL				bVector;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Entry in the list of secondary update program results.
	*/
	USC_LIST_ENTRY			sListEntry;

	/*
		Entry in the list of results which are part of a particular range.
	*/
	USC_LIST_ENTRY			sRangeListEntry;

	/*
		Number of hardware registers needed to hold this result.
	*/
	IMG_UINT32				uNumHwRegisters;

	/*
		Information about the way this result is generated.
	*/
	SAPROG_RESULT_TYPE		eType;

	/*
		TRUE if this result is part of dynamically indexed range.
	*/
	IMG_BOOL				bPartOfRange;

	union
	{
		/* (if eType == SAPROG_RESULT_TYPE_DRIVERLOADED) */
		struct
		{
			/*
				Points to the structures describing the constant data loaded.
			*/
			PINREGISTER_CONST		apsDriverConst[MAX_SCALAR_REGISTERS_PER_VECTOR];

			/*
				The entry in the list SAPROG.sDriverLoadedSAList.
			*/
			USC_LIST_ENTRY			sDriverLoadedSAListEntry;

			/*
				If this result is part of an indexable array then points to the array.
			*/
			struct _CONSTANT_INREGISTER_RANGE*	psDriverConstRange;
		} sDriverConst;
		/* else if (eType == SAPROG_RESULT_TYPE_DIRECTMAPPED) */
		struct
		{
			/*
				If this is a vector register then the mask of channels within the vector
				which are mapped to secondary attributes.
			*/
			IMG_UINT32				uChanMask;
		} sDirectMapped;
	} u1;
} SAPROG_RESULT, *PSAPROG_RESULT;

/*
	Information about a range of constants accessed with dynamic indices used
	in the program.
*/
typedef struct _CONSTANT_RANGE
{
	/*
		Entry in the list SAPROG_STATE.sConstantRangeList.
	*/
	USC_LIST_ENTRY					sListEntry;

	/*
		Index of the constant buffer associated with this range.
	*/
	IMG_UINT32						uBufferIdx;

	/*
		Index into the input data structure (UNIFLEX_CONSTBUFFERDESC.sConstsBuffRanges)
		describing the range.
	*/
	IMG_UINT32						uRangeIdx;

	/*
		Format of the constants associated with this range.
	*/
	UNIFLEX_CONST_FORMAT			eFormat;

	/*
		Used in regpack.c to associate extra data with the range.
	*/
	struct _CONST_PACK_RANGEDATA*	psData;

	/*
		Start of the range in the constant buffer in components.
	*/
	IMG_UINT32						uRangeStart;

	/*
		End of the range in the constant buffer in components.
	*/
	IMG_UINT32						uRangeEnd;

	/*
		TRUE if this is a range using dynamic indexing down to individual components of a C10
		vector. In that case we use a different layout in memory.
	*/
	IMG_BOOL						bC10SubcomponentIndex;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	IMG_BOOL						bVector;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
} CONSTANT_RANGE, *PCONSTANT_RANGE;

/*
	Information about a range of constants used with dynamic indices which have been
	placed in secondary attributes.
*/
typedef struct _CONSTANT_INREGISTER_RANGE
{
	/*
		Entry in the list SAPROG_STATE.sInRegisterConstantRangeList.
	*/
	USC_LIST_ENTRY			sListEntry;

	/*
		List of SAPROG_RESULT which are part of this range.
	*/
	USC_LIST				sResultList;

	/*
		Count of secondary attributes assigned to this range.
	*/
	IMG_UINT32				uSACount;

	/*
		Format of the constant data in the range.
	*/
	UNIFLEX_CONST_FORMAT	eFormat;

	/*
		Index of the indexable variable representing this range in the
		intermediate program.
	*/
	IMG_UINT32				uRegArrayIdx;
} CONSTANT_INREGISTER_RANGE, *PCONSTANT_INREGISTER_RANGE;

/*
	Range of secondary attributes available for allocation for uniform data
	loaded by the driver.
*/
typedef struct _SA_ALLOC_REGION
{
	/*
		Start of the region.
	*/
	IMG_UINT32				uStart;
	/*
		Length of the region.
	*/
	IMG_UINT32				uLength;
} SA_ALLOC_REGION, *PSA_ALLOC_REGION;

typedef struct _SAPROG_STATE
{
	/*
		List of constants moved to secondary attribute registers to be returned
		to the user. Type of list entries is UNIFORM_SECATTR.
	*/
	IMG_UINT32		uInRegisterConstantCount;
	USC_LIST		sInRegisterConstantList;

	/*
		List of secondary update program results which are loaded with constant
		data by the driver.
	*/
	USC_LIST		sDriverLoadedSAList;

	/*
		List of constant ranges used in the program. Type of entries is
		CONSTANT_RANGE.
	*/
	IMG_UINT32		uConstantRangeCount;
	USC_LIST		sConstantRangeList;

	/*
		List of constant ranges which have been moved to secondary attributes. Type
		of entries is CONSTANT_INREGISTER_RANGE.
	*/
	USC_LIST		sInRegisterConstantRangeList;

	/*
		Count of secondary update program results.
	*/
	IMG_UINT32		uNumResults;

	/*
		Linked list of secondary update program results.
	*/
	USC_LIST		sResultsList;

	/*
		Linked list of variables in the secondary update program
		which need fixed hardware registers.
	*/
	USC_LIST		sFixedRegList;

	/*
		Count of secondary attributes used for constants - including ones calculated in
		the secondary update program.
	*/
	IMG_UINT32		uConstSecAttrCount;

	/*
		Number of secondary attributes available for constants less reserved attributes.
	*/
	IMG_UINT32		uInRegisterConstantLimit;

	/*
		Sum of the lengths of all ranges of available secondary attributes.
	*/
	IMG_UINT32		uTotalSAsAvailable;

	/*
		Longest consecutive range of available secondary attributes.
	*/
	IMG_UINT32		uMaximumConsecutiveSAsAvailable;

	/*
		Count of ranges of secondary attributes which can be used for the driver to load
		constants into.
	*/
	IMG_UINT32		uAllocRegionCount;

	/*
		Array of ranges of secondary attribute which can be used for the driver to load
		constants into.
	*/
	PSA_ALLOC_REGION	asAllocRegions;

	/*
		Index of the first region with some unallocated secondary attributes.
	*/
	IMG_UINT32			uFirstPartialRegion;

	/*
		Points to the first secondary attribute after the last one allocated.
	*/
	IMG_UINT32			uPartialRegionOffset;

	/*
		Number of secondary attributes reserved for temporary data in the secondary
		update program.
	*/
	IMG_UINT32		uNumSecAttrsReservedForTemps;

	#if defined(OUTPUT_USPBIN)
	/*
		Bit-mask of the samplers for which enough space has been reserved in the secondary
		attributes to hold all of their texture state.
	*/
	IMG_UINT32		uTextureStateAssignedToSAs;

	/*
		Count of the secondary attributes which have been reserved for the USP to use for texture
		state.
	*/
	IMG_UINT32		uNumSAsReservedForTextureState;
	#endif /* defined(OUTPUT_USPBIN) */

	/*
		Pointer to the secondary attribute holding the render target array index.
	*/
	PINREGISTER_CONST	psSAForRTAIdx;

	/*
		Maximum number of registers required by any inst in secondary update
		program.
	*/
	IMG_UINT32		uMaxTempRegsUsedByAnyInst;
} SAPROG_STATE, *PSAPROG_STATE;

/*
	Information about a virtual register.
*/
typedef struct _VREGISTER
{
	struct _USEDEF_CHAIN*		psUseDefChain;
	struct _REGISTER_GROUP*		psGroup;
	PSAPROG_RESULT				psSecAttr;
} VREGISTER;

/*
	Global state for groups of registers requiring consecutive hardware
	register numbers.
*/
typedef struct _REGISTER_GROUP_STATE
{
	/*
		Tree mapping from virtual registers to information about any register
		groups they are part of.
	*/
	PUSC_TREE		psRegisterGroups;
	/*
		List of virtual registers for which no other virtual register requires
		a lower hardware register numbers.
	*/
	USC_LIST		sGroupHeadsList;
} REGISTER_GROUP_STATE, *PREGISTER_GROUP_STATE;

typedef struct tagINTERMEDIATE_STATE
{
	jmp_buf						sExceptionReturn;
	IMG_UINT32					uFlags;
	IMG_UINT32					uFlags2;
	IMG_UINT32					uOptimizationHint;

	PUSC_ALLOC_HEADER			psAllocationListHead;
#ifdef DEBUG
	IMG_UINT32					uMemoryUsed;
	IMG_UINT32					uMemoryUsedHWM;
	IMG_UINT32					uAllocCount;
	/* Depth of nesting of calls to DoOnCfgBasicBlocks */
	IMG_UINT32					uBlockIterationsInProgress;
#endif /* DEBUG */

#ifdef DEBUG_TIME
	clock_t                     sTimeStart;
#endif /* DEBUG_TIME */

	/* Information from the user. */
	IMG_UINT32					uCompilerFlags;
	IMG_UINT32					uCompilerFlags2;
	PUNIFLEX_CONSTDEF			psConstants;

	/*
		Features of the compiler target.
	*/
	PCSGX_CORE_DESC				psTargetDesc;
	const SGX_CORE_FEATURES*	psTargetFeatures;
	const SGX_CORE_BUGS*		psTargetBugs;
	IMG_UINT32					uTexStateSize;

	/*
		Size of the hardware's GPI register bank in 32-bit/40-bit units.
	*/
	IMG_UINT32					uGPISizeInScalarRegs;

	/*
		Maximum number of simultaneous instances of a USE program across the
		whole system (i.e. for all pipelines and cores).
	*/
	IMG_UINT32					uMaxSimultaneousInstances;

	/* Information about functions in the program. */
	IMG_UINT32					uMaxLabel; //highest uLabel of any function (not necessarily dense)
	PFUNC						psFnOutermost, psFnInnermost; //pointers to start and end of sorted-by-nesting list

	/* List of flow control blocks in the program sorted in increasing order of CODEBLOCK.uGlobalIdx. */
	USC_LIST					sBlockList;

	/* Number of registers in the program after disambiguation. */
	IMG_UINT32					uNumRegisters;

	/* Number of predicate registers in the program. */
	IMG_UINT32					uNumPredicates;

	/*
		Array mapping from temporary register numbers to USE-DEF information for the
		register.
	*/
	USC_PARRAY					psTempVReg;
	/*
		Array mapping from predicate register numbers to USE-DEF information for the
		register.
	*/
	USC_PARRAY					psPredVReg;

	/*
		List of temporary/predicate registers where part or all of the register has no
		uses.
	*/
	USC_LIST					sDroppedUsesTempList;

	/*
		List of temporary registers used as indices.
	*/
	USC_LIST					sIndexUseTempList;

	/*
		List of temporary registers of C10 format.
	*/
	USC_LIST					sC10TempList;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		List of vector sized temporary registers.
	*/
	USC_LIST					sVectorTempList;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/* Count of instructions created during the current compile. */
	IMG_UINT32					uGlobalInstructionCount;

	/* Count of blocks created during the current compile. */
	IMG_UINT32					uGlobalBlockCount;

	/* Information about each constant buffer. */
	CONSTANT_BUFFER				asConstantBuffer[UF_CONSTBUFFERID_COUNT];

	/* Static constants bufffer. */
	IMG_UINT32					uStaticConstsBuffer;

	/* Texture state constants bufffer. */
	IMG_UINT32					uTextureStateConstsBuffer;

	/*
		Pointers for main program and secondary update program (if any).
		These should only really be used for HW output, etc., and other shader-specific
		stuff, as these are also in the main list of functions (as the least nested) as
		external entry points. However, for now, we seem to have a fair amount of
		shader-specific stuff, e.g. ExtractConstantCalcs, hence keeping here. (ACL)
	*/
	PFUNC						psMainProg, psSecAttrProg;

	/*
		D3D loop index into for the innermost loop.
	*/
	IMG_UINT32					uD3DLoopIndex;

	#if defined(EXECPRED)	
	/*
		Temp. register containing Execution Predication 
		related counter.
	*/
	IMG_UINT32					uExecPredTemp;	
	#endif /* #if defined(EXECPRED) */

	/*
		Pointer to the structure describing the offsets of the various constants in
		the secondary attributes.
	*/
	PUNIFLEX_PROGRAM_PARAMETERS	psSAOffsets;

	/*
		Number of temporary registers used in the program.
	*/
	IMG_UINT32					uTemporaryRegisterCount;

	/*
		Number of temporary registers used in the Post Split program.
	*/
	IMG_UINT32					uTemporaryRegisterCountPostSplit;

	/*
		Number of temporary registers used in the program.
	*/
	IMG_UINT32					uSecTemporaryRegisterCount;

	/*
		Information about the ranges of hardware registers available for
		use by the shader.
	*/
	HWREGISTER_LIMITS			sHWRegs;

	/*
	  Information about indexable temp arrays
	 */
	PUSC_VEC_ARRAY_DATA			*apsTempVecArray;
	/*
		Total size in bytes of the indexable temp arrays.
	*/
	IMG_UINT32					uIndexableTempArraySize;

	/*
	   Arrays in registers

	   uAvailArrayRegs: Number of registers still available for use as an array.
	   uNumVecArrayRegs: Number of register arrays.
	   apsVecArrayReg: Array of data about register arrays.
	 */
	IMG_UINT32					uAvailArrayRegs;
	IMG_UINT32					uNumVecArrayRegs;
	PUSC_VEC_ARRAY_REG			*apsVecArrayReg;

	/*
		Bitmask indicating which texture stages should NOT be sampled using anisotropic
		filtering
	*/
	IMG_PUINT32					auNonAnisoTexStages;

	/*
		Points to the last basic block in the pre-alpha test part of the
		shader.
	*/
	PCODEBLOCK					psPreFeedbackBlock;

	/*
		Points to the basic block containing instructions representing the
		driver uses of shader results before pre-feedback.
	*/
	PCODEBLOCK					psPreFeedbackDriverEpilogBlock;

	/*
		Points to the last basic block in the pre-split part of the
		shader.
	*/
	PCODEBLOCK					psPreSplitBlock;

	/*
		Inputs to the assembler.
	*/
	IMG_PUINT32					puInstructions;
	IMG_PUINT32					puSAInstructions;
	IMG_PUINT32					puLabels;
	PUSEASM_CONTEXT				psUseasmContext;

	/*
		Next available label number.
	*/
	IMG_UINT32					uNextLabel;


	/* Total number of instructions required for the main program */
	IMG_UINT32					uMainProgInstCount;

	/* Total number of labels used in the main program */
	IMG_UINT32					uMainProgLabelCount;

	/* Instruction index for the start of the program */
	IMG_UINT32					uMainProgStart;

	/* Instruction index for the end of the pre-feedback phase of the program */
	IMG_UINT32					uMainProgFeedbackPhase0End;

	/*
		Instruction marking the end of the compiler generated pre-feedback code
	*/
	PINST						psMainProgFeedbackPhase0EndInst;

	/*
		Instruction index for the start of the post-feedback phase of the
		program
	*/
	IMG_UINT32					uMainProgFeedbackPhase1Start;

	/*
		Instruction index for the start of the post-split phase of the
		program
	*/
	IMG_UINT32					uMainProgSplitPhase1Start;

	/* Instruction count for the secondary load program */
	IMG_UINT32					uSAProgInstCount;

	/*
		Size of the spill area required in dwords.
	*/
	IMG_UINT32					uSpillAreaSize;

	/*
		User supplied general purpose memory allocation function.
	*/
	USC_ALLOCFN					pfnAlloc;

	/*
		User supplied function for freeing memory allocated with g_pfnAlloc.
	*/
	USC_FREEFN					pfnFree;

	/*
		User supplied function for printing debug messages.
	*/
	USC_PRINTFN					pfnPrint;

	/*
		User-supplied parameter that must be passed to the supplied PDUmp procedure
	*/
	IMG_PVOID					pvPDumpFnDrvParam;

	/*
		User-supplied procedure to dump driver-internal text output to a file
		NB: pvPDumpFnDrvParam must be passed as the first parameter.
	*/
	USC_PDUMPFN					pfnPDump;

	/*
		User-supplied parameter that must be passed to the supplied Start and
		Stop metrics procedures.
	*/
	IMG_PVOID					pvMetricsFnDrvParam;

	/*
		User-supplied procedures to start and stop a given metrics counter.
	*/
	USC_STARTFN					pfnStart;
	USC_FINISHFN				pfnFinish;

	/*
		Fixed registers: Virtual registers that must be assigned to
		specific hardware registers.
	*/
	USC_LIST					sFixedRegList;

	/*
		Count of FIXED_REG_DATA structures created.
	*/
	IMG_UINT32					uGlobalFixedRegCount;

	#if defined(OUTPUT_USPBIN)
	IMG_PVOID					pvBPCSState;
	IMG_BOOL					bResultWrittenInPhase0;
	#endif /* defined(OUTPUT_USPBIN) */

	/*
		Offset between the start of the constant buffer in memory and the address in
		the constant buffer secondary attribute.
	*/
	IMG_UINT32					uMemOffsetAdjust;

	/*
		Point to return to on a memory allocation failure.
	*/
	IMG_BOOL					bExceptionReturnValid;

	#if defined(OUTPUT_USCHW)
	/*
		Bit mask of the samples which have had an unpack format selected.
	*/
	IMG_PUINT32					auTextureUnpackFormatSelectedMask;

	/*
		For each sampler the selected unpack format.
	*/
	PUNIFLEX_TEXTURE_UNPACK		asTextureUnpackFormat;
	#endif /* defined(OUTPUT_USCHW) */

	/*
		The number of flow control constructs in the program where the control
		decision might vary per-pixel or per-vertex.
	*/
	IMG_UINT32					uNumDynamicBranches;

	/*
		Number of constants buffers allowed to use in the current program.
	*/
	IMG_UINT32					uNumOfConstsBuffsAvailable;

	PUNIFLEX_INST				psMainStartInst;

	#ifdef SRC_DEBUG
	IMG_UINT32		uCurSrcLine;
	IMG_PUINT32		puSrcLineCost;
	IMG_UINT32		uTotalLines;
	IMG_UINT32		uTotalCost;
	IMG_UINT32		uTotalSAUpdateCost;
	#endif /* SRC_DEBUG */

#ifdef UF_TESTBENCH
	IMG_UINT32	uNumVerifLoopHdrs;
	IMG_PUINT32 puVerifLoopHdrs;
#endif /*UF_TESTBENCH*/


	/*
		Number of temporary registers used in the input program.
	*/
	IMG_UINT32					uInputTempRegisterCount;

	/*
		Number of predicate registers used in the input program.
	*/
	IMG_UINT32					uInputPredicateRegisterCount;

	/*
		Tells whether the shader is invariant or not.
	*/
	IMG_BOOL					bInvariantShader;

	/*
		Level of optimization to apply. Higher values imply more
		optimizations.
	*/
	IMG_UINT32					uOptimizationLevel;
	
	/*
		The maximum distance that an instruction will be moved when
		scheduling, e.g. during dual issue or efo creation.
	*/
	IMG_UINT32					uMaxInstMovement;

	/*
		Count of Indexable Temp Arrays in intermediate code,
		can be more than input arrays count in case where
		new internal arrays are created by intermediate
		code.
	*/
	IMG_UINT32					uIndexableTempArrayCount;

	/*
		Size of each indexable temporary array (included any internally created
		ones).
	*/
	PUNIFLEX_INDEXABLE_TEMP_SIZE	psIndexableTempArraySizes;

	/*
			For shader outputs collects valid ranges.
	*/
	UNIFLEX_RANGES_LIST			sValidShaderOutPutRanges;
	PUNIFLEX_RANGE				psPackedValidOutPutRanges;
	IMG_UINT32					puPackedShaderOutputs[USC_SHADER_OUTPUT_MASK_DWORD_COUNT];

	/*
		Data specific to different shader types.
	*/
	union
	{
		PPIXELSHADER_STATE			psPS;
		PVERTEXSHADER_STATE			psVS;
	} sShader;

	/*
		List per-opcode of all the intermediate instructions with that opcode.
	*/
	SAFE_LIST					asOpcodeLists[IOPCODE_MAX];

	/*
		State related to the secondary update program.
	*/
	SAPROG_STATE				sSAProg;

	/*
		Maximum number of ALU instructions a branch can contain
		and still be 'flattened' (using predicates)
	*/
	IMG_UINT32					uMaxConditionalALUInstsToFlatten;
	/*
		How aggressively to flatten conditionals using predicates.
		UF_PREDLVL_SINGLE_INST - sets uMaxConditionalALUInstsToFlatten to 1
		UF_PREDLVL_LARGE_BLOCKS - uses uMaxConditionalALUInstsToFlatten to decide how big a block to predicate
		UF_PREDLVL_LARGE_BLOCKS_MOVC - also allows conditional moves
		UF_PREDLVL_LARGE_MOVC - allows large numbers of conditional moves
		UF_PREDLVL_NESTED - allows nested conditionals to be predicated
		UF_PREDLVL_NESTED_SAMPLES - also allows sample instructions to be in predicated blocks, but not predicated
		UF_PREDLVL_NESTED_SAMPLES_PRED - allow sample instructions to be predicated (needs help from patcher)
		*/
	UF_PREDICATIONLEVEL			ePredicationLevel;

	/*
		Range of cores on which the generated shader will run.
	*/
	IMG_UINT32					uMPCoreCount;

	/*
		State related to groups of registers that need consecutive hardware register numbers.
	*/
	struct _REGISTER_GROUP_STATE*	psGroupState;

#if defined(FAST_CHUNK_ALLOC)

	MEM_MANAGER					sMemManager;

#endif

#if defined(OUTPUT_USPBIN)
	IMG_UINT32					uNextSmpID;
#endif /* defined(OUTPUT_USPBIN) */
	
#if defined(SUPPORT_ICODE_SERIALIZATION)
	/*
		Used to determine on going DoOnAllBasicBlocks recursion level
	*/
	IMG_UINT32					uInsideDoOnAllBasicBlocks;
#endif
} INTERMEDIATE_STATE, USC_STATE;

/*
	Debug level for debug messages
*/
typedef enum
{
	__DBG_SUPPRESS,
	__DBG_MESSAGE,
	__DBG_WARNING,
	__DBG_ERROR,
	__DBG_ABORT
} DBG_MESSAGE_TYPE;

#ifndef __SECTION_TITLE__
	#define __SECTION_TITLE__ " ------- "
#endif

/*
  Standard debug out function and abbreviate
*/
#if defined(DEBUG)

	#ifndef USER_PERFORMANCE_STATS_ONLY
	    #if defined(UF_TESTBENCH) && defined(IN_VERIFIER)
		    #define DBG_PRINTF(X)	{VerifierDebugPrintf X; }
	    #else
		    #define DBG_PRINTF(X)	{DebugPrintf X;}
	    #endif
	#else
		    #define DBG_PRINTF(X)	
	#endif

	#if defined(IN_VERIFIER)
		#define DBG_SUPPRESS		__DBG_SUPPRESS
		#define DBG_MESSAGE			__DBG_MESSAGE
		#define DBG_ERROR			__DBG_ERROR
		#define DBG_WARNING			__DBG_WARNING
		#define DBG_ABORT			__DBG_ABORT
		#define DBG_PCONVERT_DATA	__DBG_SUPPRESS
		
		#define DBG_TEST_DATA		DBG_SUPPRESS
	#else /* defined(IN_VERIFIER) */
		#define DBG_SUPPRESS	psState, __DBG_SUPPRESS
		#define DBG_MESSAGE		psState, __DBG_MESSAGE
		#define DBG_ERROR		psState, __DBG_ERROR
		#define DBG_WARNING		psState, __DBG_WARNING
		#define DBG_ABORT		psState, __DBG_ABORT
		#if defined(UF_TESTBENCH)
			#define DBG_TEST_DATA	DBG_MESSAGE
			#define USC_INPUT_FILENAME	g_pszFileName
			#if defined(PCONVERT_HASH_COMPUTE)
				#define DBG_PCONVERT_DATA	DBG_WARNING
			#else
				#define DBG_PCONVERT_DATA	DBG_MESSAGE
			#endif
		#else
			#define USC_INPUT_FILENAME	"usc-input"
			#define DBG_TEST_DATA		DBG_SUPPRESS
			#define DBG_PCONVERT_DATA	DBG_SUPPRESS
		#endif

		IMG_INTERNAL IMG_VOID DebugPrintf(PINTERMEDIATE_STATE psState, DBG_MESSAGE_TYPE iType,
											const IMG_CHAR *pszFormat, ...) IMG_FORMAT_PRINTF(3, 4);
	#endif /* #if defined(IN_VERIFIER) */
#else /* defined(DEBUG) */
	/* Suppress all debug messages */
	#define DBG_PRINTF(X)
#endif /* defined(DEBUG) */

#if defined(UF_TESTBENCH)
	#ifdef __cplusplus
	extern "C"
	{
	#endif /* __cplusplus */
		extern IMG_PCHAR g_pszFileName;
	#ifdef __cplusplus
	}
	#endif /* __cplusplus */
#endif /* defined(UF_TESTBENCH) */

/*
	ASSERT definition

	NB: UF_TESTBENCH builds don't call UscAbort since there is no cleanup handler
		at that point (post compilation), but we must reference psState in the
		macro to avoid warnings about it being unused (the non UF_TESTBENCH macros
		reference it).
*/
#ifndef ASSERT
	#if defined(UF_TESTBENCH) && defined(IN_VERIFIER)
		#define ASSERT(A)	{ if (!(A)) { DBG_PRINTF((DBG_ERROR, "Verifier warning: Input file: %s failed.", g_pszFileName)); assert(A);} }
    #else
        #if defined(UF_TESTBENCH)
			#define ASSERT(A)	{ if (!(A)) { DBG_PRINTF((DBG_ERROR, "error: Input file: %s failed.", g_pszFileName)); assert(A);} }
	    #else
			#define ASSERT(A)	{ if (!(A)) UscAbort2(psState, UF_ERR_INTERNAL, #A); }
	    #endif	/* defined(UF_TESTBENCH) */
    #endif /* defined(UF_TESTBENCH) && defined(IN_VERIFIER) */
#endif	/* #ifndef ASSERT */

/*
	ASSERT intended for debugging purpose only.
*/
#if defined(DEBUG) || defined(UF_TESTBENCH)
	#define DBG_ASSERT ASSERT
#else
	#define DBG_ASSERT(x)
#endif

/*
	imgabort is used in loads of places in place of plain abort. It is intended for
	'we should never get here' type of things.

	NB: UF_TESTBENCH builds don't call UscAbort since there is no cleanup handler
		at that point (post compilation), but we must reference psState in the
		macro to avoid warnings about it being unused (the non UF_TESTBENCH macros
		reference it).
*/
#ifndef imgabort
	#if defined(UF_TESTBENCH) && defined(IN_VERIFIER)
		#define imgabort()	{ DBG_PRINTF((DBG_WARNING, "Verifier warning: Input file: %s failed (%s:%d)", g_pszFileName, __FILE__, __LINE__)); fflush(NULL); abort(); }
    #else
        #if defined(UF_TESTBENCH)
			#define imgabort()	{ DBG_PRINTF((DBG_ERROR, "imgabort error: Input file: %s failed. (%s:%d)", g_pszFileName, __FILE__, __LINE__)); fflush(NULL); abort(); }
	    #else
			#define imgabort()	UscAbort2(psState, UF_ERR_INTERNAL, IMG_NULL)
	    #endif	/* #ifndef UF_TESTBENCH */
    #endif /* defined(UF_TESTBENCH) && defined(IN_VERIFIER) */
#endif	/* #ifndef imgabort */

#ifndef DEBUG
	#define CHECKSTATUS(state,fn) {if (state->uCompilationStatus != UF_OK) { fn; } }
#else /* defined(DEBUG) */
	/* Debug build */
	#define CHECKSTATUS(state,fn) { ASSERT(state->uCompilationStatus == UF_OK); }
#endif /* !defined(DEBUG) */
	
#if defined(SUPPORT_ICODE_SERIALIZATION)
	#if !defined(DEBUG)
		#error "SUPPORT_ICODE_SERIALIZATION Requires DEBUG Enabled"
	#endif
	#define ICODE_SERIALIZE_OR_UNSERIALIZE(psState, titlemsg, title)									\
			{																							\
				if(psState->uCompilerFlags & UF_ICODE_SERIALIZE)										\
				{																						\
					DumpOrLoadIntermediate(psState, title, __FILE__, __LINE__, titlemsg, IMG_TRUE);		\
				}																						\
				else																					\
				if(psState->uCompilerFlags & UF_ICODE_UNSERIALIZE)										\
				{																						\
					DumpOrLoadIntermediate(psState, title, __FILE__, __LINE__, titlemsg, IMG_FALSE);	\
				}																						\
			}
#else
	#define ICODE_SERIALIZE_OR_UNSERIALIZE(psState, titlemsg, title)
#endif


#ifdef USER_PERFORMANCE_STATS_ONLY
	    #define TESTONLY(A)
	    #define TESTONLY_PRINT_INTERMEDIATE(titlemsg, title, bShowNums)										
#else
#if defined(UF_TESTBENCH) && !defined(PCONVERT_HASH_COMPUTE)
	#define TESTONLY(A) A
	#define TESTONLY_PRINT_INTERMEDIATE(titlemsg, title, bShowNums)										\
			{																							\
				ICODE_SERIALIZE_OR_UNSERIALIZE(psState, titlemsg, title)								\
				DBG_PRINTF((DBG_TEST_DATA, __SECTION_TITLE__ titlemsg __SECTION_TITLE__));				\
				TESTONLY(PrintIntermediate(psState, title, bShowNums));									\
			}
#else
	#define TESTONLY(A)
	#if defined(SUPPORT_ICODE_SERIALIZATION)
		#define TESTONLY_PRINT_INTERMEDIATE(titlemsg, title, bShowNums)									\
			ICODE_SERIALIZE_OR_UNSERIALIZE(psState, titlemsg, title)
	#else
		#define TESTONLY_PRINT_INTERMEDIATE(titlemsg, title, bShowNums)
	#endif
#endif /* defined(UF_TESTBENCH) */
#endif	/*USER_PERFRMANCE_STATS_ONLY */

#define EXTRACT_CHAN(SWIZ, CHAN) ((IMG_UINT32)(((SWIZ) >> ((CHAN) * 3)) & 0x7))

/*
	Callback for (iterative-until-fixpoint) dataflow analysis.
	pvResult is pointer to current dataflow value for the node, and to which result
		(i.e. new dataflow value) should be written;
	pvArgs is array of pointers to dataflow values for successors/predecessors.
	Return TRUE if df values depending on this one should be recalculated.
*/
typedef IMG_BOOL (*DATAFLOW_FN)(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvResult, IMG_PVOID* pvArgs, IMG_PVOID pvUserData);

/* Simple callback, executable on a single basic block. */
typedef IMG_VOID (*BLOCK_PROC)(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pUserData);

#if defined(REGALLOC_LSCAN) || defined(IREGALLOC_LSCAN)
/*
  Register liveness intervals
 */
/* USC_INTERVAL_FLAG: Property flags of register intervals */
typedef enum _USC_INTERVAL_FLAG_
{
	INTVL_WRITTEN,           // Interval has been unconditionally written
	INTVL_CONDWRITTEN,       // Interval has been conditionally written
	INTVL_FUNCTION,          // Interval occurs in a function body
    INTVL_PENDING,           // Interval still to be completed.
    INTVL_READ,              // Last access was a read
	INTVL_FULLCHAN,          // All channels of the register are used
    INTVL_MAINBANK,          // Interval must be placed in a temporary or primary
	INTVL_SAVE,              // Register has been saved (saved copy is up-to-date)
	INTVL_RESTORE,           // Register should be restored before being read
	INTVL_MAYBEPARAM,        // Register may be an input/output for a function
	INTVL_PARAM,             // Register is an input/output for a function
	INTVL_INTERNBANK,		 // Interval must be placed in a internal register
	INTVL_MERGER,			 // Interval is the (provisional) merger of two or more intervals
	INTVL_TIED,				 // Interval has been merged with another interval

	INTVL_FLAG_MAX           // Maximum number of properties
} USC_INTERVAL_FLAG;

/*
   USC_OPEN_START: Interval open start point
   USC_OPEN_END:   Interval open end point
*/
#define USC_OPEN_START (UINT_MAX)
#define USC_OPEN_END ((IMG_UINT32) 0)

/* USC_REG_INTERVAL: Liveness interval for a variable. */
typedef struct _USC_REG_INTERVAL_
{
	/*
	   uStart, uEnd: Start and end of the interval.
	   Values are the uId field of the instruction in the block
	   with the last instruction in the block at index 1.

	   Note that index i of first instruction in the block is greater
	   than index j of last instruction in the block (i >= j) since
	   liveness intervals are calculated by iterating backwards
	   through a block.

	   An index of 0 means that the interval start/end point has not
	   been closed (is undefined).
	*/
	IMG_UINT32 uStart;
	IMG_UINT32 uEnd;

#ifdef DEBUG
	PINST psStartInst;  // Instruction starting the interval
	PINST psEndInst;    // Instruction ending the interval
#endif

	IMG_UINT32 auFlags[UINTS_TO_SPAN_BITS(INTVL_FLAG_MAX)];

	/* Data for register allocation */
	IMG_UINT32 uNode;      // The node this interval is for
	IMG_UINT32 uBaseNode;  // First node of register group.
	IMG_UINT32 uColour;    // Colour assigned to this interval or (USC_UNDEF_COLOUR)
	IMG_UINT32 uCount;     // Number of consecutive colours in this interval
	IMG_BOOL bSpill;       // No colour could be assigned to this interval
	IMG_UINT32 uNesting;   // Maximum loop nesting depth at which register occurs

	IMG_UINT32 uWriteMask; // Mask of channels written
	IMG_UINT32 uReadMask;  // Mask of channels read
	IMG_UINT32 uUseMask;   // Mask of channels used (mask of live channels)
	IMG_UINT32 uSaveMask;  // Mask of channels saved
	IMG_UINT32 uAccessCtr; // Count number of reads/writes

	PINST psInst;            // Last instruction to set interval data

	/* List of intervals which could be tied (given the same colour) */
	struct _USC_REG_INTERVAL_ *psTieNext;
	struct _USC_REG_INTERVAL_ *psTiePrev;
	struct _USC_REG_INTERVAL_ *psTieParent;

	/* List of intervals in which a register is live. */
	struct _USC_REG_INTERVAL_ *psRegNext;
	struct _USC_REG_INTERVAL_ *psRegPrev;

	/*  Links for processing a list of register intervals. */
	struct _USC_REG_INTERVAL_ *psProcNext;
	struct _USC_REG_INTERVAL_ *psProcPrev;
} USC_REG_INTERVAL, *PUSC_REG_INTERVAL;

#endif /* defined(REGALLOC_LSCAN) || defined(IREGALLOC_LSCAN) */

#if defined(UF_TESTBENCH)
/*
  Support for printing register allocation maps.
 */
extern IMG_CHAR * const g_pszRegType[];

struct _USC_REGMAP_DATA_;

/* USC_NODETOREG_FN: Convert a node to a register */
typedef IMG_BOOL (*USC_NODETOREG_FN)(struct _USC_REGMAP_DATA_ *,
									 IMG_UINT32 uNode,
									 IMG_PUINT32 puRegType,
									 IMG_PUINT32 puRegNum);

/* USC_REGMAP_DATA: Information needed to print a register map */
typedef struct _USC_REGMAP_DATA_
{
    IMG_UINT32 uNumPARegs;            // Number of primary attributes
    IMG_UINT32 uNumIRegs;             // Number of internal registers
	USC_NODETOREG_FN pfnNodeToReg;
} USC_REGMAP_DATA, *PUSC_REGMAP_DATA;
#endif /* defined(UF_TESTBENCH) */

/* debug.c */
IMG_VOID PrintIntermediateInst(PINTERMEDIATE_STATE psState, PINST psInst, IMG_CHAR* pszNum, IMG_CHAR* apcTempString);
IMG_VOID PrintIntermediate(PINTERMEDIATE_STATE psState, IMG_PCHAR pchName, IMG_BOOL bShowNums);
IMG_VOID PrintInput(PINTERMEDIATE_STATE			psState,
					IMG_PCHAR					pszBeginLabel,
					IMG_PCHAR					pszEndLabel,
					PUNIFLEX_INST				psProg,
					PUNIFLEX_CONSTDEF			psConstants,
					IMG_UINT32					uCompilerFlags,
					IMG_UINT32					uCompilerFlags2,
					PUNIFLEX_PROGRAM_PARAMETERS psProgramParameters,
					IMG_BOOL					bCompileForUSP
#ifdef USER_PERFORMANCE_STATS_ONLY
					,IMG_CHAR* pszuniflex
#endif
					);

#ifdef	SRC_DEBUG
IMG_VOID PrintSourceLineCost(PINTERMEDIATE_STATE		psState,
							 PUNIFLEX_INST			psProg);
#endif /* SRC_DEBUG */

#if defined(SUPPORT_ICODE_SERIALIZATION)
/* ICode serialization functions */
IMG_VOID LoadIntermediateCodeFromFile(PINTERMEDIATE_STATE psState, const IMG_CHAR *pszFileName,
										const IMG_CHAR *pszDumpInfo, IMG_UINT32 uLineInfo);
IMG_VOID DumpIntermediateToFile(PINTERMEDIATE_STATE psState, const IMG_CHAR *pszFileName, 
										const IMG_CHAR *pszComment, const IMG_CHAR *pszDumpInfo, IMG_UINT32 uLineInfo,
										IMG_BOOL bReloadTest);
IMG_VOID DumpOrLoadIntermediate(PINTERMEDIATE_STATE psState, const IMG_CHAR *pszFileName,
										const IMG_CHAR *pszDumpInfo, IMG_UINT32 uLineInfo,
										const IMG_CHAR *pszComment, IMG_BOOL bStore);
IMG_VOID TranslateSimpleElement(IMG_VOID* pvTransState, IMG_PVOID *ppvElement);
#endif /* defined(SUPPORT_ICODE_SERIALIZATION) */

/* usc_utils.c */
#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
IMG_PCHAR PVRUniFlexDecodeInputSwizzle(IMG_BOOL		bDot,
									   IMG_UINT32	uSwiz,
									   IMG_PCHAR	pszSwiz);
#ifdef USER_PERFORMANCE_STATS_ONLY
IMG_INTERNAL IMG_INT getPerformanceEstimation(FILE *fstream);
#endif

IMG_INTERNAL IMG_VOID InternalDecodeInputInst(IMG_UINT32	uCompileFlags,
											  PUNIFLEX_INST	psInst,
											  IMG_INT32		i32Indent,
											  IMG_PCHAR		pszInst,
											  IMG_PUINT32	puTexUseMask);
IMG_INTERNAL IMG_VOID InternalDecodeComparisonOperator(IMG_UINT32	uCompOp,
													   IMG_PCHAR	pszStr);
#endif /* #if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
extern const INPUT_INST_DESC g_asInputInstDesc[UFOP_MAXIMUM];

/* regalloc.c */
IMG_VOID AssignRegisters(PINTERMEDIATE_STATE psState);
IMG_UINT32 GetIRegsReadMask(PINTERMEDIATE_STATE	psState, PINST psInst, IMG_BOOL	bPreMoe);
IMG_UINT32 GetIRegsWrittenMask(PINTERMEDIATE_STATE	psState, PINST psInst, IMG_BOOL bPreMoe);
PUSC_REG_INTERVAL MergeIntervals(PUSC_REG_INTERVAL psInterval1,
                                 PUSC_REG_INTERVAL psInterval2);

IMG_FLOAT NodeActivity(PUSC_REG_INTERVAL psInterval,
					   IMG_UINT32 uUpper,
					   IMG_UINT32 uLower);
IMG_INT32 CmpNodeActivity(PUSC_REG_INTERVAL	psInterval1,
						  PUSC_REG_INTERVAL	psInterval2,
						  IMG_UINT32 uUpper,
						  IMG_UINT32 uLower);
IMG_VOID InitRegInterval(PUSC_REG_INTERVAL psInterval,
                         IMG_UINT32 uNode);
IMG_VOID DeleteRegInterval(PINTERMEDIATE_STATE psState,
						   PUSC_REG_INTERVAL psInterval);
IMG_VOID FreeRegIntervalList(PINTERMEDIATE_STATE psState,
							 PUSC_REG_INTERVAL psInterval);
IMG_BOOL EarlierThan(IMG_UINT32 uPoint1, IMG_UINT32 uPoint2);
IMG_UINT32 IntervalLength(IMG_UINT32 uStart, IMG_UINT32 uEnd);
IMG_INT32 IntervalCmp(IMG_UINT32 uStart, IMG_UINT32 uEnd, IMG_UINT32 uIdx);
IMG_INT32 IntervalLive(IMG_UINT32 uStart, IMG_UINT32 uEnd,	IMG_BOOL bDest,	IMG_UINT32 uIdx);

PUSC_REG_INTERVAL RegIntvlCons(PUSC_REG_INTERVAL psInterval,
							   PUSC_REG_INTERVAL psList);
PUSC_REG_INTERVAL RegIntvlFindWith(PUSC_REG_INTERVAL psList,
                                   IMG_UINT32 uIdx,
                                   IMG_BOOL bDest);
PUSC_REG_INTERVAL RegIntvlDrop(PUSC_REG_INTERVAL psInterval,
							   PUSC_REG_INTERVAL psList);
PUSC_REG_INTERVAL ProcListCons(PUSC_REG_INTERVAL psInterval,
							   PUSC_REG_INTERVAL psList);
PUSC_REG_INTERVAL ProcListAdd(PUSC_REG_INTERVAL psInterval,
							  PUSC_REG_INTERVAL psList);
PUSC_REG_INTERVAL ProcListRemove(PUSC_REG_INTERVAL psInterval,
								 PUSC_REG_INTERVAL psList);
PUSC_REG_INTERVAL ProcListFindPos(PUSC_REG_INTERVAL psList,
                                  IMG_UINT32 uStart,
                                  IMG_UINT32 uEnd);
PUSC_REG_INTERVAL ProcListFindStart(PUSC_REG_INTERVAL psList);
IMG_BOOL InsertTieInterval(PINTERMEDIATE_STATE psState,
						   PUSC_REG_INTERVAL psInterval,
						   PUSC_REG_INTERVAL psList);
IMG_BOOL IntervalOverlap(IMG_UINT32 uStart1, IMG_UINT32 uEnd1,
						 IMG_UINT32 uStart2, IMG_UINT32 uEnd2);

#if defined(UF_TESTBENCH)
IMG_VOID InitMapData(PUSC_REGMAP_DATA psMapData);
#if defined(PCONVERT_HASH_COMPUTE)
	#define PrintNodeColours(a, b, c, d) 
#else
	IMG_VOID PrintNodeColours(PINTERMEDIATE_STATE psState,
							PUSC_REGMAP_DATA psMapData,
							IMG_UINT32 uNumNodes,
							PUSC_REG_INTERVAL *apsInterval);
#endif /* defined(PCONVERT_HASH_COMPUTE) */
#endif /* defined(UF_TESTBENCH) */

typedef struct _REPLACE_REGISTER_LIVEOUT_SETS
{
	IMG_UINT32	uRenameFromRegType;
	IMG_UINT32	uRenameFromRegNum;
	IMG_UINT32	uRenameToRegType;
	IMG_UINT32	uRenameToRegNum;
} REPLACE_REGISTER_LIVEOUT_SETS, *PREPLACE_REGISTER_LIVEOUT_SETS;
IMG_VOID ReplaceRegisterLiveOutSetsBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvContext);

/*
	Set up value representing number of cores, can vary from vertex to fragement shaders 
*/
#if defined(SGX_FEATURE_MP_CORE_COUNT) || defined(SGX_FEATURE_MP_CORE_COUNT_TA)

	#if defined(SGX_FEATURE_MP_CORE_COUNT_TA)
		/* Core count depends on shader type */
		#define USC_MP_CORE_COUNT_VERTEX   SGX_FEATURE_MP_CORE_COUNT_TA
		#define USC_MP_CORE_COUNT_PIXEL SGX_FEATURE_MP_CORE_COUNT_3D
	#else
		/* Core count is always the same */
		#define USC_MP_CORE_COUNT_VERTEX   SGX_FEATURE_MP_CORE_COUNT
		#define USC_MP_CORE_COUNT_PIXEL SGX_FEATURE_MP_CORE_COUNT
	#endif

#else /* SGX_FEATURE_MP_CORE_COUNT || SGX_FEATURE_MP_CORE_COUNT_TA) */

	/* Assume single core  */
	#define USC_MP_CORE_COUNT_VERTEX   1
	#define USC_MP_CORE_COUNT_PIXEL 1

#endif /* (SGX_FEATURE_MP_CORE_COUNT || SGX_FEATURE_MP_CORE_COUNT_TA) */


/* define max number of cores */
#if USC_MP_CORE_COUNT_VERTEX > USC_MP_CORE_COUNT_PIXEL
	#define USC_MP_CORE_COUNT_MAX USC_MP_CORE_COUNT_VERTEX
#else
	#define USC_MP_CORE_COUNT_MAX USC_MP_CORE_COUNT_PIXEL
#endif
	

/* iregalloc.c */
IMG_VOID AssignInternalRegisters(PINTERMEDIATE_STATE psState);
IMG_UINT32 CalcInstSrcChans(PINTERMEDIATE_STATE psState,
							PINST psInst,
							IMG_UINT32 uArgNum);

/* ssa.c */
IMG_VOID ValueNumbering(PINTERMEDIATE_STATE psState);
IMG_VOID ExpandCallInstructions(PINTERMEDIATE_STATE psState);
IMG_VOID ReleaseVRegisterInfo(PINTERMEDIATE_STATE	psState);
PVREGISTER GetVRegister(PINTERMEDIATE_STATE psState, IMG_UINT32 uType, IMG_UINT32 uNumber);

/* finalise.c */
IMG_BOOL SupportNoSched(PINST psInst);
IMG_BOOL IsIllegalInstructionPair(PINTERMEDIATE_STATE	psState,
								  IOPCODE				eOpcode1,
								  IOPCODE				eOpcode2);
IMG_UINT32 FinaliseShader(PINTERMEDIATE_STATE psState);
IMG_BOOL IsAsyncWBInst(PINTERMEDIATE_STATE psState, PINST psInst);
IMG_VOID SetupSkipInvalidFlag(PINTERMEDIATE_STATE psState);
IMG_BOOL ReserveTempForMemConstLoad(PINTERMEDIATE_STATE psState, PINST psInst);
IMG_BOOL CanStartFetch(PINTERMEDIATE_STATE psState, PINST psInst);
IMG_VOID FinaliseMemoryLoads(PINTERMEDIATE_STATE psState);

IMG_VOID FinaliseMemoryStoresBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNull);

#if defined(OUTPUT_USPBIN)
/* uspbin.c */
IMG_UINT32 CreateUspBinOutput(PINTERMEDIATE_STATE	psState,
							  PUSP_PC_SHADER*		ppsPCShader);

IMG_VOID DestroyUspBinOutput(PINTERMEDIATE_STATE	psState,
							 PUSP_PC_SHADER			psPCShader);

IMG_UINT32 GetHWOperandsUsedForArg(PINST psInst, IMG_UINT32 uArgIdx);
IMG_BOOL IsInstSrcShaderResultRef(PINTERMEDIATE_STATE	psState,
								  PINST					psInst,
								  IMG_UINT32			ArgIdx);
IMG_BOOL IsInstDestShaderResult(PINTERMEDIATE_STATE psState, PINST	psInst);
IMG_BOOL IsInstReferingShaderResult(PINTERMEDIATE_STATE	psState, PINST psInst);
IMG_VOID MarkShaderResultWrite(PINTERMEDIATE_STATE psState, PINST psInst);
#endif /* defined(OUTPUT_USPBIN)*/

#if defined (OUTPUT_USCHW)
/* hw.c */
IMG_UINT32 CompileToHw(PINTERMEDIATE_STATE psState, PUNIFLEX_HW psHw);
IMG_VOID CleanupUniflexHw(PINTERMEDIATE_STATE psState, PUNIFLEX_HW psHw);
#endif /* #if defined(OUTPUT_USCHW) */

IMG_BOOL CanSetEndFlag(PINTERMEDIATE_STATE psState, PFUNC psFunc);

/* reorder.c */
IMG_VOID SplitFeedback(PINTERMEDIATE_STATE psState);
IMG_VOID OptimizeNoschedRegions(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock);
IMG_VOID BuildFetchInstructions(PINTERMEDIATE_STATE psState);

/*
	Latency of non memory laod high latency instruction.
*/
#define	NON_MEM_LOAD_LATENCY_INST_LATENCY	(10)
IMG_VOID ReorderHighLatncyInstsBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNull);
IMG_BOOL PeepholeOptimize(PINTERMEDIATE_STATE psState);
IMG_BOOL PeepholeOptimizeBlock(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock);

/* intcvt.c */
IMG_BOOL IntegerOptimize(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PBOOL pbMovesGenerated);
IMG_BOOL IsSOPWMMove(PINST psInst);
IMG_BOOL IsSOPWMMultiply(PINST psInst, IMG_PBOOL pbAlphaSwizzle);
IMG_VOID FormatConvert(PINTERMEDIATE_STATE psState);
IMG_BOOL IsSOP2DoubleMove(PINTERMEDIATE_STATE psState, PINST psInst);
typedef enum _SOP3_COMMUTE_MODE
{
	SOP3_COMMUTE_MODE_INVALID,
	SOP3_COMMUTE_MODE_LRP,
	SOP3_COMMUTE_MODE_MAD
} SOP3_COMMUTE_MODE, *PSOP3_COMMUTE_MODE;
IMG_VOID CommuteSOP3Sources(PINTERMEDIATE_STATE psState, 
							PINST psInst, 
							SOP3_COMMUTE_MODE eCommuteMode);
PCSOURCE_ARGUMENT_PAIR GetCommutableSOP3Sources(PINTERMEDIATE_STATE psState, PINST psInst, PSOP3_COMMUTE_MODE peCommuteMode);
IMG_BOOL SubstFixedPointComponentMove(PINTERMEDIATE_STATE psState, PINST psInst);

/* groupinst.c */
IMG_VOID GroupInstructionsProgram(PINTERMEDIATE_STATE psState);
IMG_UINT32 MoeArgToInstArg(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uArg);
IMG_BOOL IsValidSwapInst(PINST psInst);
IMG_BOOL TypeUsesFormatControl(IMG_UINT32		uType);

/* f16opt.c */
IMG_BOOL SubstituteFMad16Swizzle(PINTERMEDIATE_STATE	psState,
								 PINST					psInst,
								 FMAD16_SWIZZLE			eSwizzle,
								 IMG_UINT32				uArg,
								 IMG_BOOL				bCheckOnly);
IMG_BOOL ReplaceFMOV16(PINTERMEDIATE_STATE psState, PINST psInst, IMG_PBOOL pbNewMoves, PWEAK_INST_LIST psEvalList);
IMG_VOID EliminateF16MovesBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNull);
FMAD16_SWIZZLE CombineFMad16Swizzle(PINTERMEDIATE_STATE psState, FMAD16_SWIZZLE eFirstSwiz, FMAD16_SWIZZLE eSecondSwiz);
IMG_BOOL EliminateF16Move(PINTERMEDIATE_STATE psState, PINST psInst, IMG_PBOOL pbNewMoves);
IMG_BOOL PromoteFARITH16ToF32(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock);

/* iselect.c */
IMG_VOID MergeIdenticalInstructionsBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNull);
IMG_BOOL IsConstantArgument(PINTERMEDIATE_STATE	psState, PARG psArg);
IMG_VOID EliminateMovesAndFuncs(PINTERMEDIATE_STATE psState);
IMG_VOID EliminateMovesFromGPI(PINTERMEDIATE_STATE psState);
IMG_VOID GenerateNonEfoInstructionsBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNull);
IMG_VOID ExtractConstantCalculations(PINTERMEDIATE_STATE psState);
IMG_BOOL ShouldInsertInstNow(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PINST psInst);
IMG_VOID ScheduleForPredRegPressure(PINTERMEDIATE_STATE psState);
IMG_BOOL FlattenProgramConditionals(PINTERMEDIATE_STATE psState);
IMG_VOID RemovePredicates(PINTERMEDIATE_STATE psState);
IMG_VOID LocalConstProp(PINTERMEDIATE_STATE psUscState);
IMG_BOOL CheckAndUpdateInstSARegisterCount(PINTERMEDIATE_STATE	psState,
										   IMG_UINT32			uSrcRegCount,
										   IMG_UINT32			uResultRegCount,
										   IMG_BOOL				bCheckOnly);
IMG_VOID ExpandUnsupportedInstructions(PINTERMEDIATE_STATE	psState);
IMG_BOOL DefIsBetweenInsts(PINTERMEDIATE_STATE psState, PARG pArg, PINST psUpperInst, PINST psLowerInst);
IMG_UINT32 LogicalNegateTest(PINTERMEDIATE_STATE psState, TEST_TYPE eTest);
IMG_VOID LogicalNegateInstTest(PINTERMEDIATE_STATE psState, PINST psDefInst);
IMG_BOOL SimplifyTESTPRED(PINTERMEDIATE_STATE psState, PINST psInst);
IMG_VOID EliminateMoves(PINTERMEDIATE_STATE psState);
IMG_VOID EliminateMovesBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock);
IMG_VOID EliminateMovesFromList(PINTERMEDIATE_STATE psState, PUSC_LIST psList);
IMG_VOID ReplaceMOV(PINTERMEDIATE_STATE psState, PINST psInst);
IMG_BOOL CanReplaceTempWithAnyGPI(PINTERMEDIATE_STATE	psState, 
								  IMG_UINT32			uTempRegNum,
								  PINST*				ppsDefineInst, 
								  PINST*				ppsLastUseInst,
								  IMG_BOOL				bAllowGroupedReg);
IMG_BOOL CanReplaceTempWithGPI(PINTERMEDIATE_STATE	psState, 
							   IMG_UINT32			uTempRegNum, 
							   IMG_UINT32			uGPIRegNum, 
							   IMG_BOOL				bAllowGroupedReg);

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
IMG_BOOL EliminateVMOV(PINTERMEDIATE_STATE psState,  PINST psVMOVInst, IMG_BOOL bCheckOnly);
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
IMG_VOID OptimizePredicateCombines(PINTERMEDIATE_STATE psState);

typedef struct _SOURCE_VECTOR
{
	IMG_PUINT32		puVec;
	IMG_UINT32		uSmallVec;
} SOURCE_VECTOR;

FORCE_INLINE
IMG_VOID InitSourceVector(PINTERMEDIATE_STATE psState, PINST psInst, PSOURCE_VECTOR psVector)
/*****************************************************************************
 FUNCTION	: InitSourceVector

 PURPOSE	: Initialize a vector defining a subset of the sources to a particular
			  instruction.

 PARAMETERS	: psState				- Compiler state.
			  psInst				- Instruction.
			  psVector				- Vector to initialize.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psInst->uArgumentCount <= BITS_PER_UINT)
	{
		psVector->puVec = &psVector->uSmallVec;
	}
	else
	{
		psVector->puVec = (IMG_PUINT32)UscAlloc(psState, sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(psInst->uArgumentCount));
	}
	memset(psVector->puVec, 0, sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(psInst->uArgumentCount));
}

FORCE_INLINE
IMG_BOOL CheckSourceVector(PCSOURCE_VECTOR psVector, IMG_UINT32 uSrc)
/*****************************************************************************
 FUNCTION	: AddToSourceVector

 PURPOSE	: Check if a particular source is a member of a subset.

 PARAMETERS	: psVector				- Vector to check.
			  uSrc					- Index of the source to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	return GetBit(psVector->puVec, uSrc) ? IMG_TRUE : IMG_FALSE;
}

FORCE_INLINE
IMG_BOOL CheckSourceVectorRange(PCSOURCE_VECTOR psVector, IMG_UINT32 uStart, IMG_UINT32 uEnd)
/*****************************************************************************
 FUNCTION	: AddToSourceVector

 PURPOSE	: Check if a particular range of sources are all members of a subset.

 PARAMETERS	: psVector				- Vector to check.
			  uStart				- First source to check.
			  uEnd					- Last source to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uIdx;

	for (uIdx = uStart; uIdx <= uEnd; uIdx++)
	{
		if (!GetBit(psVector->puVec, uIdx))
		{
			return IMG_FALSE;
		}
	}
	return IMG_TRUE;
}

FORCE_INLINE
IMG_VOID AddToSourceVector(PSOURCE_VECTOR psVector, IMG_UINT32 uSrc)
/*****************************************************************************
 FUNCTION	: AddToSourceVector

 PURPOSE	: Add a source to a subset of the sources to a particular
			  instruction.

 PARAMETERS	: psVector				- Vector to modify.
			  uSrc					- Index of the source to add.

 RETURNS	: Nothing.
*****************************************************************************/
{
	SetBit(psVector->puVec, uSrc, 1);
}

FORCE_INLINE
IMG_VOID DeinitSourceVector(PINTERMEDIATE_STATE psState, PSOURCE_VECTOR psVector)
/*****************************************************************************
 FUNCTION	: DeinitSourceVector

 PURPOSE	: Deinitialize a vector representing a subset of the sources to an
			  instruction.

 PARAMETERS	: psState			- Compiler state.
			  psVector			- Vector to deinitialize.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psVector->puVec != &psVector->uSmallVec)
	{
		UscFree(psState, psVector->puVec);
	}
	psVector->puVec = NULL;
}

typedef IMG_BOOL (*PFN_MASKED_REPLACE)(PINTERMEDIATE_STATE, PINST, PSOURCE_VECTOR, IMG_PVOID, IMG_BOOL);
IMG_BOOL ReplaceAllUsesMasked(PINTERMEDIATE_STATE			psState,
							  PARG							psMovDest,
							  IMG_UINT32					uMovDestMask,
							  PARG*						apsPredSrc,
							  IMG_UINT32					uPredCount,
							  IMG_BOOL						bPredNegate,
							  IMG_BOOL						bPredPerChan,
							  PFN_MASKED_REPLACE			pfnReplace,
							  IMG_PVOID						pvContext,
							  IMG_BOOL						bCheckOnly);
IMG_BOOL DoReplaceAllUsesMasked(PINTERMEDIATE_STATE			psState,
								PARG						psMovDest,
								IMG_UINT32					uMovDestMask,
								PARG*						apsPredSrc,
								IMG_UINT32					uPredCount,
								IMG_BOOL					bPredNegate,
								IMG_BOOL					bPredPerChan,
								PFN_MASKED_REPLACE			pfnReplace,
								IMG_PVOID					pvContext,
								IMG_BOOL					bCheckOnly);

#if defined(OUTPUT_USPBIN)
IMG_VOID OptimiseUSPSamplesBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNull);
IMG_VOID InsertAlternateResults(PINTERMEDIATE_STATE psState);
#endif /* #if defined(OUTPUT_USPBIN) */

typedef struct _ARGUMENT_REFS
{
	/* Mask of the instruction sources which reference the argument. */
	SOURCE_VECTOR	sSourceVector;
	/* TRUE if the instruction references the argument as an index. */
	IMG_BOOL		bUsedAsIndex;
	/* TRUE if the instruction references the argument as the old value of a partially written destination. */
	IMG_BOOL		bUsedAsPartialDest;
} ARGUMENT_REFS, *PARGUMENT_REFS;
typedef IMG_BOOL (*PFN_GLOBAL_REPLACE_ARGUMENT)(PINTERMEDIATE_STATE			psState,
												PCODEBLOCK					psCodeBlock,
												PINST						psInst,
												PARGUMENT_REFS				psArgRefs,
												PARG						psRegA,
												PARG						psRegB,
												IMG_PVOID					pvContext,
												IMG_BOOL					bCheckOnly);
IMG_BOOL DoGlobalMoveReplace(PINTERMEDIATE_STATE			psState,
							 PCODEBLOCK						psBlock,
							 PINST							psMoveInst,
							 PARG							psRegA,
							 PARG							psRegB,
							 PFN_GLOBAL_REPLACE_ARGUMENT	pfnArgReplace,
							 IMG_PVOID						pvContext);
IMG_BOOL EliminateGlobalMove(PINTERMEDIATE_STATE		psState,
							 PCODEBLOCK					psBlock,
							 PINST						psMoveInst,
							 PARG						psRegA,
							 PARG						psRegB,
							 PWEAK_INST_LIST			psEvalList);
IMG_BOOL GetPredicateNegate(PINTERMEDIATE_STATE	psState, 
							const INST			*psInst);
IMG_BOOL GetPredicatePerChan(PINTERMEDIATE_STATE	psState, 
							 const INST				*psInst);
IMG_VOID ArithmeticSimplification(PINTERMEDIATE_STATE psState);
IMG_VOID ReduceSampleResultPrecision(PINTERMEDIATE_STATE	psState);
IMG_VOID RemoveUnnecessarySaturationsBP(PINTERMEDIATE_STATE	psState, PCODEBLOCK psBlock, IMG_PVOID pvNull);
IMG_BOOL OverwritesIndexDest(PARG psDest, PARG psArg);
IMG_BOOL OverwritesIndex(PINST psInst, PARG psArg);
IMG_VOID FixInvalidSourceBanks(PINTERMEDIATE_STATE psState);

IMG_VOID ConvertPredicatedMovesToMovc(PINTERMEDIATE_STATE	psState);

/* regpack.c */
IMG_VOID PackConstantRegisters(PINTERMEDIATE_STATE psState);
IMG_VOID RemoveDuplicateConsts(PINTERMEDIATE_STATE psState);
IMG_BOOL ReplaceHardwareConstBySecAttr(PINTERMEDIATE_STATE	psState,
									   IMG_UINT32			uConstIdx,
									   IMG_BOOL				bNegate,
									   IMG_PUINT32			puArgType,
									   IMG_PUINT32			puArgNum);
IMG_FLOAT GetHardwareConstantValue(PINTERMEDIATE_STATE psState, IMG_UINT32 uConstIdx);
IMG_VOID PackCalculatedSecondaryAttributes(PINTERMEDIATE_STATE psState);
IMG_BOOL IsStaticC10Value(PINTERMEDIATE_STATE psState, IMG_FLOAT fValue, PARG psArg, IMG_UINT32 uChanMask);
IMG_BOOL AddStaticSecAttr(PINTERMEDIATE_STATE	psState,
						  IMG_UINT32			uValue,
						  IMG_PUINT32			puArgType,
						  IMG_PUINT32			puArgNum);
IMG_BOOL AddStaticSecAttrRange(PINTERMEDIATE_STATE	psState,
							   IMG_PUINT32			puValue,
							   IMG_UINT32			uCount,
							   PINREGISTER_CONST*	ppsFirstConst,
							   HWREG_ALIGNMENT		eHwRegAlignment);
IMG_UINT32 GetHardwareConstantValueu(PINTERMEDIATE_STATE psState, IMG_UINT32 uConstIdx);
IMG_UINT32 GetIteratedValue(PINTERMEDIATE_STATE			psState,
							UNIFLEX_ITERATION_TYPE		eIterationType,
							IMG_UINT32					uCoordinate,
							UNIFLEX_TEXLOAD_FORMAT		eFormat,
							IMG_UINT32					uNumAttributes);
PPIXELSHADER_INPUT AddIteratedValue(PINTERMEDIATE_STATE		psState,
								    UNIFLEX_ITERATION_TYPE	eIterationType,
								    IMG_UINT32				uCoordinate,
									UNIFLEX_TEXLOAD_FORMAT	eFormat,
									IMG_UINT32				uNumAttributes,
									IMG_UINT32				uFirstTempRegNum);
IMG_VOID RemoveUnusedPixelShaderInputs(PINTERMEDIATE_STATE	psState);
IMG_VOID AllocatePixelShaderIterationRegisters(PINTERMEDIATE_STATE psState);
IMG_VOID AddDummyIteration(PINTERMEDIATE_STATE psState, IMG_UINT32 uFlags, IMG_UINT32 uPADestNum);
IMG_VOID CalculateHardwareRegisterLimits(PINTERMEDIATE_STATE	psState);
PPIXELSHADER_INPUT AddOrCreateNonDependentTextureSample(PINTERMEDIATE_STATE		psState,
												        IMG_UINT32				uTexture,
													    IMG_UINT32				uChunk,
													    IMG_UINT32				uCoordinate,
													    IMG_UINT32				uTextureDimensionality,
													    IMG_BOOL				bProjected,
													    IMG_UINT32				uAttributeSizeInRegs,
														IMG_UINT32				uAttributeSizeInDwords,
														UF_REGFORMAT			eResultFormat,
														#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
														IMG_BOOL				bVector,
														#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
														UNIFLEX_TEXLOAD_FORMAT	eFormat);
PSAPROG_RESULT BaseAddNewSAProgResult(PINTERMEDIATE_STATE	psState,
									  UF_REGFORMAT			eFmt,
									  #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
									  IMG_BOOL				bVector,
									  #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
									  IMG_UINT32			uHwRegAlignment,
									  IMG_UINT32			uNumHwRegisters,
									  IMG_UINT32			uTempRegNum,
									  IMG_UINT32			uUsedChanMask,
									  SAPROG_RESULT_TYPE	eType,
									  IMG_BOOL				bPartOfRange);
IMG_VOID AddNewSAProgResult(PINTERMEDIATE_STATE	psState,
							UF_REGFORMAT			eFmt,
							#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
							IMG_BOOL				bVector,
							#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
							IMG_UINT32				uNumHwRegisters,
							PARG					psResult);
IMG_UINT32 GetSAProgNumResultRegisters(PINTERMEDIATE_STATE	psState,
									   #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
									   IMG_BOOL				bVector,
									   #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
									   UF_REGFORMAT			eFmt);
IMG_VOID DropSAProgResult(PINTERMEDIATE_STATE psState, PSAPROG_RESULT psResult);
IMG_VOID SetChansUsedFromSAProgResult(PINTERMEDIATE_STATE psState, PSAPROG_RESULT psResult);
IMG_VOID ReplaceSpillAreaOffsetByStatic(PINTERMEDIATE_STATE	psState,
										PINREGISTER_CONST	psConst,
										IMG_UINT32			uNewValue);
IMG_UINT32 FindHardwareConstant(PINTERMEDIATE_STATE	psState,
								IMG_UINT32			uImmediate);
IMG_VOID AddSpillAreaOffsetPlaceholder(PINTERMEDIATE_STATE	psState,
									   PINREGISTER_CONST*	ppsConst);
IMG_VOID AllocateSecondaryAttributes(PINTERMEDIATE_STATE	psState);
IMG_BOOL IsDriverLoadedSecAttr(PINTERMEDIATE_STATE psState, const ARG *psArg);
IMG_UINT32 ProjectCoordinates(PINTERMEDIATE_STATE	psState,
							  PCODEBLOCK			psBlock,
							  PINST					psInsertBeforeInst,
							  PARG					psProj,
							  PINST					psProjSrcInst,
							  IMG_UINT32			uProjSrcIdx,
							  IMG_UINT32			uProjChannel,
							  IMG_UINT32			uCoordSize,
							  IMG_UINT32			uCoordMask,
							  UF_REGFORMAT			eCoordFormat,
							  PINST					psCoordSrcIdx,
							  IMG_UINT32			uCoordSrcIdx,
							  IMG_UINT32			uInCoordTempStart);
IMG_VOID FinaliseTextureSamples(PINTERMEDIATE_STATE psState);
IMG_VOID ExpandConditionalTextureSamples(PINTERMEDIATE_STATE psState);
IMG_VOID InitializeSAProg(PINTERMEDIATE_STATE	psState);
IMG_VOID GetDriverLoadedConstantLocation(PINTERMEDIATE_STATE	psState,
									     PINREGISTER_CONST		psConst,
										 IMG_PUINT32			puArgType,
										 IMG_PUINT32			puArgNum);

/* efo.c */
IMG_VOID GenerateEfoInstructionsBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNull);
IMG_VOID SetupEfoUsage(PINTERMEDIATE_STATE psState, PINST psInst);
IMG_BOOL ConvertInstToEfo(PINTERMEDIATE_STATE psState, PINST psInst);

#if defined(SUPPORT_SGX545)
/* dualissue.c */
IMG_BOOL GenerateDualIssue(PINTERMEDIATE_STATE psState);
#endif /* defined(SUPPORT_SGX545) */

/* dgraph.c */
PDGRAPH_STATE NewDGraphState(PINTERMEDIATE_STATE psState);
IMG_VOID FreeDGraphState(PINTERMEDIATE_STATE psState, PDGRAPH_STATE *ppDGraph);

IMG_VOID FreeBlockDGraphState(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock);

IMG_VOID		ComputeClosedDependencyGraph(PDGRAPH_STATE psDepState, IMG_BOOL bUnorderedDeps);
IMG_VOID		UpdateClosedDependencyGraph(PDGRAPH_STATE psDepState, IMG_UINT32 uTo, IMG_UINT32 uFrom);
PINST			GetNextAvailable(PDGRAPH_STATE psDepState, PINST psStartInst);

PDGRAPH_STATE	ComputeBlockDependencyGraph(PINTERMEDIATE_STATE psState,
											PCODEBLOCK psBlock,
											IMG_BOOL bIgnoreDesched);
											
PDGRAPH_STATE	ComputeDependencyGraph(PINTERMEDIATE_STATE psState,
										const CODEBLOCK *psBlock,
										IMG_BOOL bIgnoreDesched);
								
IMG_VOID		RemoveInstruction(PDGRAPH_STATE psDepState, PINST psInst);
IMG_VOID		MergeInstructions(PDGRAPH_STATE psDepState, IMG_UINT32 uDestInst, IMG_UINT32 uSrcInst, IMG_BOOL bRemoveSrc);
IMG_UINT32		AddNewInstToDGraph(PDGRAPH_STATE psDepState, PINST psNewInst);

IMG_VOID		AddDependency(PDGRAPH_STATE psDepState,
								IMG_UINT32 uFrom,
								IMG_UINT32 uTo);
IMG_VOID		SplitInstructions(PDGRAPH_STATE	psDepState,
									IMG_UINT32		auNewInst[],
									IMG_UINT32		uOldInst);
IMG_VOID RemoveDependency(PDGRAPH_STATE		psDepState,
						  IMG_UINT32		uFrom,
						  PINST				psTo);
IMG_VOID AddClosedDependency(PINTERMEDIATE_STATE psState, PDGRAPH_STATE	psDepState, IMG_UINT32 uFrom, IMG_UINT32 uTo);
IMG_BOOL IsImmediateSubordinate(PINST psInst, PINST psPossibleSubordinate);

#ifdef UF_TESTBENCH
IMG_VOID PrintDependencyGraph(PDGRAPH_STATE psDepState, IMG_BOOL bClosed);
IMG_VOID DotPrintDepGraph(PDGRAPH_STATE psDepState, IMG_PCHAR pchFileName, IMG_BOOL bClosed);
#endif

#if defined(DEBUG)
/* main.c */
/*
  uDebugOption: Option for used when debugging the compiler. Set by
                command line option '-option <num>'. Example use: turn
                new code on or off without having to rebuild the compiler.
  Default value: 0
*/
extern IMG_UINT32 g_uDebugOption;
#endif

#if defined(UF_TESTBENCH)
/*
  uOnlineOption: Option for used when debugging the compiler. Set by
     command line option '-online <num>' to choose a configuration.
	 Values: 0 is offline compiler, 1 is online (just-in-time) compiler
*/
extern IMG_UINT32 g_uOnlineOption;
/*
 bDotOption: Option used to enable extra testbench/debugging output in the form
	of files NNXXXX.dot (for increasing NN, and XXX being the name of the last
	phase to do anything) suitable for input to the 'dot' layout engine (part
	of the 'graphviz' visualisation suite, www.graphviz.org).
*/
extern IMG_BOOL g_bDotOption;
#endif

/* usc.c */
IMG_VOID CompileIntermediate(PINTERMEDIATE_STATE psState);
PINTERMEDIATE_STATE UscMakeState(USC_ALLOCFN pfnAlloc,
								 USC_FREEFN pfnFree,
								 USC_PRINTFN pfnPrint);

IMG_VOID CompOpToTest(PINTERMEDIATE_STATE psState, UFREG_COMPOP uCompOp, PTEST_DETAILS psTest);
TEST_TYPE ConvertCompOpToIntermediate(PINTERMEDIATE_STATE psState, UFREG_COMPOP uCompOp);
IMG_BOOL IsCall(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock);
IMG_BOOL IsNonMergable(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock);
IMG_VOID AppendInst(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PINST psInstToInsert);
PCODEBLOCK AllocateBlock(PINTERMEDIATE_STATE psState, PCFG psCfg);
PFUNC AllocFunction(PINTERMEDIATE_STATE psState, IMG_PCHAR pchEntryPointDesc);
IMG_VOID GetInputPredicate(PINTERMEDIATE_STATE psState, IMG_PUINT32 puPredSrc, IMG_PBOOL pbPredNegate, IMG_UINT32 uPredicate, IMG_UINT32 uChan);
IMG_VOID GetInputPredicateInst(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uPredicate, IMG_UINT32 uChan);
IMG_UINT32 MaskToSwiz(IMG_UINT32 uMask);
UF_REGFORMAT GetRegisterFormat(PINTERMEDIATE_STATE psState, PUF_REGISTER psSrc);
IMG_UINT32 GetNextRegister(PINTERMEDIATE_STATE psState);
IMG_VOID MakeNewPredicateArg(PINTERMEDIATE_STATE psState, PARG psArg);
IMG_VOID MakeNewTempArg(PINTERMEDIATE_STATE psState, UF_REGFORMAT eFmt, PARG psArg);
IMG_INTERNAL IMG_UINT32 GetNextPredicateRegister(PINTERMEDIATE_STATE psState);
IMG_UINT32 GetNextRegisterCount(PINTERMEDIATE_STATE	psState, IMG_UINT32	uCount);
#ifdef SRC_DEBUG
IMG_VOID IncrementCostCounter(PINTERMEDIATE_STATE psState,
							  IMG_UINT32 uSrcLine, IMG_UINT32 uIncrementValue);
IMG_VOID DecrementCostCounter(PINTERMEDIATE_STATE psState,
							  IMG_UINT32 uSrcLine, IMG_UINT32 uDecrementValue);
IMG_VOID CreateSourceCycleCountTable(PINTERMEDIATE_STATE			psState,
									 PUNIFLEX_SOURCE_CYCLE_COUNT    psTableSourceCycle);
#endif /* SRC_DEBUG */

IMG_BOOL FreeBlock(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock);
IMG_BOOL FreeFunction(PINTERMEDIATE_STATE psState, PFUNC psFunc);
IMG_VOID InlineFunction(PINTERMEDIATE_STATE psState, PINST psCallInst);
IMG_BOOL MergeBasicBlocks(PINTERMEDIATE_STATE psState, PFUNC psFunc);
IMG_VOID MergeAllBasicBlocks(PINTERMEDIATE_STATE psState);
IMG_VOID RedirectEdgesFromPredecessors(PINTERMEDIATE_STATE psState, PCODEBLOCK psFrom, PCODEBLOCK psTo, IMG_BOOL bSyncEnd);
IMG_VOID RedirectEdge(PINTERMEDIATE_STATE psState, PCODEBLOCK psSource, IMG_UINT32 uSucc, PCODEBLOCK psDest);
IMG_BOOL UnconditionallyFollows(PINTERMEDIATE_STATE	psState, PCODEBLOCK psBlock1, PCODEBLOCK psBlock2);
IMG_VOID ReplaceEdge(PINTERMEDIATE_STATE psState, PCODEBLOCK psSource, IMG_UINT32 uSucc, PCODEBLOCK psDest);
IMG_VOID SetBlockSwitch(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_UINT32 uNumSuccs, PCODEBLOCK *apsSuccs, PARG psArg, IMG_BOOL bDefault, IMG_PUINT32 auCaseValues);
IMG_VOID SetBlockConditional(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_UINT32 uPredSrc, PCODEBLOCK psTrueSucc, PCODEBLOCK psFalseSucc, IMG_BOOL bStatic);
IMG_VOID ExchangeConditionalSuccessors(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock);
IMG_VOID SetBlockUnconditional(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PCODEBLOCK psSucc);
#if defined(EXECPRED)
IMG_VOID SetBlockConditionalExecPred(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PCODEBLOCK psTrueSucc, PCODEBLOCK psFalseSucc, IMG_BOOL bStatic);
#endif /*defined(EXECPRED) */
IMG_VOID DropPredecessorFromDeltaInstructions(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_UINT32 uPredToRemove);
IMG_VOID ClearSuccessors(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock);
IMG_VOID SwitchConditionalBlockToUnconditional(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_BOOL bCondSrc);
IMG_VOID FinaliseIntermediateCode(PINTERMEDIATE_STATE psState);
IMG_VOID ConvertSwitchBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psSwitch, IMG_PVOID pvNull);
PINST FormMove(PINTERMEDIATE_STATE psState,
			   PARG psDst,
			   PARG psSrc,
			   PINST psSrcLine);

IMG_VOID ConvertRegister(PINTERMEDIATE_STATE psState, PARG psReg);

extern const IMG_UINT32	g_auSetBitCount[];
extern const IMG_BOOL g_abSingleBitSet[];
extern const IMG_UINT32 g_puSwapRedAndBlueChan[];
extern const IMG_UINT32 g_puSwapRedAndBlueMask[];
extern const UFREG_COMPOP g_puInvertCompOp[];
extern const UFREG_COMPOP g_puNegateCompOp[];
extern const IMG_INT32	g_aiSingleComponent[];
extern const IMG_UINT32 g_auSetBitMask[][4];
extern const IMG_UINT32	g_auMaxBitCount[];
extern const IMG_UINT32	g_auFirstSetBit[];
IMG_VOID DoDataflow(PINTERMEDIATE_STATE psState, PFUNC psFunc,
				   IMG_BOOL bForwards, IMG_UINT32 uSize, IMG_PVOID pvWorking,
				   DATAFLOW_FN pfClosure, IMG_PVOID pvUserData);
IMG_VOID DoOnAllBasicBlocks(PINTERMEDIATE_STATE psState, BLOCK_SORT_FUNC pfnSort, BLOCK_PROC pfClosure,
							IMG_BOOL bHandlesCalls, IMG_PVOID pvUserData);
IMG_VOID DoOnCfgBasicBlocks(PINTERMEDIATE_STATE psState, PCFG psCfg, BLOCK_SORT_FUNC pfnSort,
							BLOCK_PROC pfClosure, IMG_BOOL bHandlesCalls, IMG_PVOID pvUserData);
IMG_VOID DebugPrintSF(PINTERMEDIATE_STATE psState, PCFG psCfg);
IMG_VOID ResizeArray(PINTERMEDIATE_STATE psState,
					 IMG_PVOID psArray,
					 IMG_UINT32 uOldSize,
					 IMG_UINT32 uNewSize,
					 IMG_PVOID* ppsNew);
					 
IMG_VOID IncreaseArraySizeInPlace(PINTERMEDIATE_STATE psState,
					 IMG_UINT32 uOldSize,
					 IMG_UINT32 uNewSize,
					 IMG_PVOID* ppsArray);
					 
#define ResizeTypedArray(STATE, ARRAY, OLDSIZE, NEWSIZE) \
		ResizeArray(STATE,								 \
					ARRAY,								 \
				    (OLDSIZE) * sizeof((ARRAY)[0]),		 \
					(NEWSIZE) * sizeof((ARRAY)[0]),		 \
					(IMG_PVOID*)&(ARRAY))
IMG_VOID FreeCodeBlock(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock);
IMG_VOID InitialiseIndexableTemps(PINTERMEDIATE_STATE psState);
IMG_VOID ReleaseState(PINTERMEDIATE_STATE psState);
IMG_UINT32 AddNewRegisterArray(PINTERMEDIATE_STATE	psState,
							   ARRAY_TYPE			eArrayType,
							   IMG_UINT32			uArrayNum,
							   IMG_UINT32			uChannelsPerDword,
							   IMG_UINT32			uNumRegs);
IMG_VOID SetSyncEndOnSuccessor(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_UINT32 uSuccIdx);
IMG_VOID ReleaseFixedReg(PINTERMEDIATE_STATE psState, PFIXED_REG_DATA psFixedReg);
IMG_UINT32 SearchRangeList(PUNIFLEX_RANGES_LIST	psRangesList, IMG_UINT32 uNum, PUNIFLEX_RANGE* ppsFoundRange);
IMG_PVOID CallocBitArray(PINTERMEDIATE_STATE psState, IMG_UINT32 uBitCount);
typedef struct _BLOCK_WORK_LIST
{
	PCODEBLOCK	psHead;
	PCODEBLOCK	psTail;
} BLOCK_WORK_LIST, *PBLOCK_WORK_LIST;
IMG_VOID InitializeBlockWorkList(PBLOCK_WORK_LIST psList);
IMG_BOOL IsBlockWorkListEmpty(PBLOCK_WORK_LIST psList);
IMG_VOID PrependToBlockWorkList(PBLOCK_WORK_LIST psList, PCODEBLOCK psBlockToAdd);
IMG_VOID AppendToBlockWorkList(PBLOCK_WORK_LIST psList, PCODEBLOCK psBlockToAdd);
PCODEBLOCK RemoveBlockWorkListHead(PINTERMEDIATE_STATE psState, PBLOCK_WORK_LIST psList);
IMG_BOOL IsBlockInBlockWorkList(PBLOCK_WORK_LIST psList, PCODEBLOCK psBlock);
IMG_VOID AppendPredecessorsToBlockWorkList(PBLOCK_WORK_LIST psList, PCODEBLOCK psBlock);
IMG_VOID ClearBlockInsts(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock);
IMG_BOOL CheckProgram(PINTERMEDIATE_STATE psState);
PCODEBLOCK AddEdgeViaEmptyBlock(PINTERMEDIATE_STATE psState, PCODEBLOCK psTo, IMG_UINT32 uPredIdx);
IMG_VOID MergeIdenticalSuccessors(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_UINT32 uSuccToRetain);
IMG_VOID DropPredecessorFromDeltaInstruction(PINTERMEDIATE_STATE psState, PINST psDeltaInst, IMG_UINT32 uPredToRemove);
typedef struct _REGISTER_REMAP
{
	IMG_UINT32	uRemapCount;
	IMG_PUINT32	auRemap;
} REGISTER_REMAP, *PREGISTER_REMAP;
IMG_UINT32 GetRemapLocationCount(PINTERMEDIATE_STATE	psState,
								 PREGISTER_REMAP		psRemap,
								 IMG_UINT32				uInputNum,
								 IMG_UINT32				uCount);
IMG_UINT32 GetRemapLocation(PINTERMEDIATE_STATE	psState,
						    PREGISTER_REMAP		psRemap,
						    IMG_UINT32			uInputNum);
IMG_VOID InitializeRemapTable(PINTERMEDIATE_STATE psState, PREGISTER_REMAP psRemap);
IMG_VOID DeinitializeRemapTable(PINTERMEDIATE_STATE psState, PREGISTER_REMAP psRemap);
IMG_VOID ExpandFunctions(PINTERMEDIATE_STATE psState, PREGISTER_REMAP psRemap, IMG_UINT32 uFirstC10PairChanMask);
IMG_VOID ClearFeedbackSplit(PINTERMEDIATE_STATE psState);

/* dce.c */
IMG_VOID ClearRegLiveSet(PINTERMEDIATE_STATE psState,
						 PREGISTER_LIVESET psLiveSet);
IMG_VOID InitRegLiveSet(PREGISTER_LIVESET psLiveSet);
PREGISTER_LIVESET AllocRegLiveSet(PINTERMEDIATE_STATE psState);
IMG_VOID CopyRegLiveSet(PINTERMEDIATE_STATE psState,
						PREGISTER_LIVESET psSrc,
						PREGISTER_LIVESET psDst);
IMG_VOID FreeRegLiveSet(PINTERMEDIATE_STATE psState, PREGISTER_LIVESET psLiveSet);
IMG_VOID ComputeRegistersLiveInSet(PINTERMEDIATE_STATE	psState, 
								   PCODEBLOCK			psBlock, 
								   PCODEBLOCK			psLiveInBlock);
IMG_UINT32 GetLiveChansInVectorArgumentPreSwizzle(PINTERMEDIATE_STATE	psState,
												  const INST			*psInst,
												  IMG_UINT32			uSrcSlot,
												  IMG_UINT32			auLiveChansInDest[]);
IMG_BOOL IsPredicateSourceLive(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uArg);

IMG_VOID DoSALiveness(PINTERMEDIATE_STATE psState);
IMG_VOID DeadCodeEliminationBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvRemoveNops);
IMG_VOID DeadCodeElimination(PINTERMEDIATE_STATE psState, IMG_BOOL bFreeBlocks);
IMG_UINT32 GetRegisterLiveMask(PINTERMEDIATE_STATE psState,
							   PREGISTER_LIVESET psLiveset,
							   IMG_UINT32 uType,
							   IMG_UINT32 uNumber,
							   IMG_UINT32 uOffset);
IMG_VOID SetRegisterLiveMask(PINTERMEDIATE_STATE psState,
							 PREGISTER_LIVESET psLiveset,
							 IMG_UINT32 uType,
							 IMG_UINT32 uNumber,
							 IMG_UINT32 uOffset,
							 IMG_UINT32 uMask);
IMG_UINT32 GetLiveChansInArgument(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArg, IMG_UINT32 auLiveChansInDest[]);
IMG_VOID ComputeShaderRegsInitialised(PINTERMEDIATE_STATE psState);

IMG_VOID InitRegUseDef(PREGISTER_USEDEF psUseDef);
IMG_VOID InitUseDef(PUSEDEF_RECORD psUseDef);
IMG_VOID ClearRegUseDef(PINTERMEDIATE_STATE psState, PREGISTER_USEDEF psUseDef);
IMG_VOID ClearUseDef(PINTERMEDIATE_STATE psState, PUSEDEF_RECORD psUseDef);

IMG_UINT32 GetRegUseDef(PINTERMEDIATE_STATE psState,
						PREGISTER_USEDEF psUseDef,
						IMG_UINT32 uRegType,
						IMG_UINT32 uRegNum);
IMG_VOID ReduceRegUseDef(PINTERMEDIATE_STATE psState,
						 PREGISTER_USEDEF psUseDef,
						 IMG_UINT32 uRegType,
						 IMG_UINT32 uRegNum,
						 IMG_UINT32 uMask);
IMG_VOID InstUseSubset(PINTERMEDIATE_STATE psState, PINST psInst, PREGISTER_USEDEF psUse, IMG_UINT32 const* puSet);
IMG_VOID InstUse(PINTERMEDIATE_STATE psState, PINST psInst, PREGISTER_USEDEF psUse);
IMG_VOID InstDef(PINTERMEDIATE_STATE psState, PINST psInst, PREGISTER_USEDEF psDef);
IMG_VOID ComputeInstUseDef(PINTERMEDIATE_STATE psState,
						   PUSEDEF_RECORD psUseDef,
						   PINST psFromInst,
						   PINST psToInst);
IMG_BOOL InstUseDefined(PINTERMEDIATE_STATE psState, PREGISTER_USEDEF psDef, PINST psInst);
IMG_BOOL InstDefReferenced(PINTERMEDIATE_STATE psState, PREGISTER_USEDEF psUse, PINST psInst);
IMG_BOOL DisjointUseDef(PINTERMEDIATE_STATE psState,
						PREGISTER_USEDEF psUseDefA,
						PREGISTER_USEDEF psUseDefB);
IMG_UINT32 GetLiveChansInArg(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uArg);
IMG_VOID IncreaseRegisterLiveMask(PINTERMEDIATE_STATE psState,
		  						  PREGISTER_LIVESET psLiveset,
								  IMG_UINT32 uType,
								  IMG_UINT32 uNumber,
								  IMG_UINT32 uArrayOffset,
								  IMG_UINT32 uReadMask);
IMG_VOID IncreaseIndexLiveMask(PINTERMEDIATE_STATE	psState,
							   PARG					psArg,
							   PREGISTER_LIVESET	psLiveset);
IMG_VOID GetLiveChansInVectorResult(PINTERMEDIATE_STATE	psState,
									const INST			*psInst,
									IMG_UINT32			auLiveChansInDestNonVec[],
									IMG_UINT32			auLiveChansInDestVec[]);
IMG_UINT32 ApplySwizzleToMask(IMG_UINT32			uSwizzle,
							  IMG_UINT32			uInMask);
IMG_UINT32 ChanSelToMask(PINTERMEDIATE_STATE psState, PTEST_DETAILS psTest);
IMG_UINT32 GetPreservedChansInPartialDest(PINTERMEDIATE_STATE	psState,
										  PINST					psInst,
										  IMG_UINT32			uDestIdx);
IMG_VOID GetLiveChansInSourceSlot(PINTERMEDIATE_STATE	psState, 
								  PINST					psInst, 
								  IMG_UINT32			uSlotIdx,
								  IMG_PUINT32			puChansUsedPreSwizzle,
								  IMG_PUINT32			puChansUsedPostSwizzle);
IMG_UINT32 GetLiveChansInSOP3Argument(PCINST psInst, IMG_UINT32 uArg, IMG_UINT32 uLiveChansInDest);
IMG_VOID BaseGetLiveChansInVectorArgument(PINTERMEDIATE_STATE	psState, 
										  PCINST				psInst, 
										  IMG_UINT32			uArg, 
										  IMG_UINT32			auLiveChansInDestNonVec[],
										  IMG_PUINT32			puPreSwizzleLiveChanMask,
										  IMG_PUINT32			puPostSwizzleLiveChanMask);
IMG_UINT32 GetVTestChanSelMask(PINTERMEDIATE_STATE psState, PCINST psInst);
#if defined(REGALLOC_LSCAN) || defined(IREGALLOC_LSCAN)
IMG_VOID RegUseInstSeq(PUSC_LIVE_CALLBACK psCallBack,
					   PINST psLastInst);
IMG_VOID RegUseBlock(PINTERMEDIATE_STATE psState,
					 PUSC_LIVE_CALLBACK psCallBack,
					 PFUNC psFunc);
#endif /* defined(REGALLOC_LSCAN) || defined(IREGALLOC_LSCAN) */

/* icvt_c10.c */
IMG_UINT32 CheckDuplicateChannel(PUNIFLEX_INST	psInst,
								 IMG_UINT32		uChan);
PCODEBLOCK ConvertInstToIntermediateC10(PINTERMEDIATE_STATE psState,
										PCODEBLOCK psCodeBlock,
										PUNIFLEX_INST psSrc,
										IMG_BOOL bConditional,
										IMG_BOOL bStaticCond);
IMG_VOID CreateComparisonC10(PINTERMEDIATE_STATE psState,
							 PCODEBLOCK psCodeBlock,
							 IMG_UINT32 uPredDest,
							 UFREG_COMPOP uCompOp,
							 IMG_UINT32 uChanOp,
							 PARG psSrc1,
							 PARG psSrc2,
							 IMG_UINT32 uChan,
							 IMG_UINT32 uCompPredSrc,
							 IMG_BOOL bCompPredNegate,
							 IMG_BOOL bInvert);
IMG_VOID GetSourceC10(PINTERMEDIATE_STATE psState,
					  PCODEBLOCK psCodeBlock,
					  PUF_REGISTER psSource,
					  IMG_BYTE bySrcMod,
					  PARG psHwSource,
					  IMG_UINT32 uMask,
					  IMG_BOOL bIgnoreSwiz,
					  IMG_BOOL bMultiFormat,
					  UF_REGFORMAT	eRequiredFmt);
IMG_VOID GetGradientsC10(PINTERMEDIATE_STATE	psState,
						 PCODEBLOCK				psCodeBlock,
						 PUNIFLEX_INST			psInputSrc,
						 IMG_UINT32				uGradDim,
						 IMG_UINT32				uCoord,
						 PSAMPLE_GRADIENTS		psGradients);
IMG_VOID GetSampleIdxC10(PINTERMEDIATE_STATE	psState,
						 PCODEBLOCK				psCodeBlock,
						 PUNIFLEX_INST			psInputInst,
						 PARG					psSampleIdx);
IMG_VOID GetLODAdjustmentC10(PINTERMEDIATE_STATE	psState,
							 PCODEBLOCK				psCodeBlock,
							 PUNIFLEX_INST			psInputInst,
							 PSAMPLE_LOD_ADJUST		psLODAdjust);
IMG_VOID GetSampleCoordinateArrayIndexC10(PINTERMEDIATE_STATE	psState,
										  PCODEBLOCK			psCodeBlock,
										  PUNIFLEX_INST			psInputSrc,
										  IMG_UINT32			uCoordMask,
										  IMG_UINT32			uTexture,
										  IMG_UINT32			uArrayIndexTemp);
IMG_VOID GetSampleCoordinatesC10(PINTERMEDIATE_STATE	psState,
								 PCODEBLOCK				psCodeBlock,
								 PUNIFLEX_INST			psInputSrc,
								 IMG_UINT32				uDimensionality,
								 PSAMPLE_COORDINATES	psCoordinates,
								 IMG_BOOL				bPCF);
IMG_VOID GetProjectionC10(PINTERMEDIATE_STATE	psState,
						  PCODEBLOCK			psCodeBlock,
						  PUNIFLEX_INST			psInputInst,
						  PSAMPLE_PROJECTION	psProj,
						  UF_REGFORMAT			eCoordForamt);
IMG_BOOL CanOverrideSwiz(PUF_REGISTER psSrc);
IMG_UINT32 ConvertInputChannelToIntermediate(PINTERMEDIATE_STATE	psState,
											 IMG_UINT32				uInputChan);
IMG_INTERNAL IMG_UINT32 GetInputToU8C10IntermediateSwizzle(PINTERMEDIATE_STATE	psState);
IMG_UINT32 ConvertInputWriteMaskToIntermediate(PINTERMEDIATE_STATE	psState,
											   IMG_UINT32			uInputWriteMask);
IMG_VOID GetCMPTestType(PINTERMEDIATE_STATE	psState, 
					    PUNIFLEX_INST		psInst, 
						IMG_BOOL			bSrc0Neg, 
						IMG_BOOL			bSrc0Abs, 
						PTEST_TYPE			peTest, 
						IMG_PUINT32			puTrueSrc);

/* icvt_f32.c */
PCODEBLOCK ConvertInstToIntermediateF32(PINTERMEDIATE_STATE psState,
									  	PCODEBLOCK psCodeBlock,
									  	PUNIFLEX_INST psSrc,
									  	IMG_BOOL bConditional,
									  	IMG_BOOL bStaticCond);
IMG_VOID CreateComparisonFloat(PINTERMEDIATE_STATE	psState,
							   PCODEBLOCK			psCodeBlock,
							   IMG_UINT32			uPredDest,
							   UFREG_COMPOP			uCompOp,
							   PUF_REGISTER			psSrc1,
							   PUF_REGISTER			psSrc2,
							   IMG_UINT32			uChan,
							   UFREG_COMPCHANOP		eChanOp,
							   IMG_UINT32			uCompPredSrc,
							   IMG_BOOL				bCompPredNegate,
							   IMG_BOOL				bInvert);
IMG_VOID GenerateDestModF32(PINTERMEDIATE_STATE psState,
							PCODEBLOCK psCodeBlock,
							PARG psDest,
							IMG_UINT32 uSat,
							IMG_UINT32 uScale,
							IMG_UINT32 uPredSrc,
							IMG_BOOL bPredNegate);
IMG_VOID LoadIteratedValue(PINTERMEDIATE_STATE psState,
						   PCODEBLOCK psCodeBlock,
						   UF_REGTYPE eType,
						   PUF_REGISTER psSource,
						   IMG_UINT32 uSrcChan,
						   IMG_UINT32 uChan,
						   PARG psHwSource);
IMG_VOID GetSourceF32(PINTERMEDIATE_STATE psState,
					  PCODEBLOCK psCodeBlock,
					  PUF_REGISTER psSource,
					  IMG_UINT32 uChan,
					  PARG psHwSource,
					  IMG_BOOL bAllowSourceMod,
					  PFLOAT_SOURCE_MODIFIER psSourceMod);
IMG_VOID ApplySourceModF32(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PARG psHwSource, IMG_PUINT32 puComponent, IMG_UINT32 uSMod);
IMG_VOID ConvertTexkillInstructionFloat(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc, IMG_BOOL bConditional, IMG_BOOL bFloat32);
IMG_VOID ConvertMovaInstructionFloat(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc, IMG_BOOL bFloat32);
IMG_BOOL DestChanOverlapsSrc(PINTERMEDIATE_STATE psState,
							 PUNIFLEX_INST psSrc,
							 IMG_UINT32 uDestChan);
IMG_VOID GetDestinationF32(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUF_REGISTER psDest, IMG_UINT32 uChan, PARG psHwSource);
IMG_VOID GetLODAdjustmentF32(PINTERMEDIATE_STATE	psState,
							 PCODEBLOCK				psCodeBlock,
							 PUNIFLEX_INST			psInputInst,
							 PSAMPLE_LOD_ADJUST		psLODAdjust);
IMG_VOID GetSampleCoordinateArrayIndexF32(PINTERMEDIATE_STATE	psState,
										  PCODEBLOCK			psCodeBlock,
										  PUNIFLEX_INST			psInputInst,
										  IMG_UINT32			uTextureDimensionality,
										  IMG_UINT32			uTexture,
										  IMG_UINT32			uArrayIndexTemp);
IMG_VOID GetSampleCoordinatesF32(PINTERMEDIATE_STATE	psState,
								 PCODEBLOCK				psCodeBlock,
								 PUNIFLEX_INST			psInputInst,
								 IMG_UINT32				uDimensionality,
								 PSAMPLE_COORDINATES	psCoordinates,
								 IMG_BOOL				bPCF);
IMG_VOID GetProjectionF32(PINTERMEDIATE_STATE	psState,
						  PCODEBLOCK			psCodeBlock,
						  PUNIFLEX_INST			psInputInst,
						  PSAMPLE_PROJECTION	psProj,
						  UF_REGFORMAT			eCoordFormat);
IMG_VOID GetGradientsF32(PINTERMEDIATE_STATE	psState,
						 PCODEBLOCK				psCodeBlock,
						 PUNIFLEX_INST			psInputInst,
						 IMG_UINT32				uGradDim,
						 IMG_UINT32				uCoord,
						 PSAMPLE_GRADIENTS		psGradients);
IMG_VOID GetSampleIdxF32(PINTERMEDIATE_STATE	psState,
						 PCODEBLOCK				psCodeBlock,
						 PUNIFLEX_INST			psInputInst,
						 PARG					psSampleIdx);
IMG_VOID ConvertComparisonInstructionF32(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc);
IMG_UINT32 ConvertTempRegisterNumberF32(PINTERMEDIATE_STATE		psState,
										IMG_UINT32				uInputRegisterNumber,
										IMG_UINT32				uChan);
IMG_VOID ConvertSetpInstructionNonC10(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc);
IMG_VOID ConvertBitwiseInstructionF32(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc);
IMG_VOID ConvertAddressValue(PINTERMEDIATE_STATE		psState,
							 PCODEBLOCK					psCodeBlock,
							 IMG_UINT32					uRegIndexDest,
							 PARG						psInputInst,
							 IMG_UINT32					uSourceComponent,
							 IMG_BOOL					bU8Src,
							 IMG_UINT32					uPredSrc,
							 IMG_BOOL					bPredSrcNegate);
IMG_VOID ConvertIntegerDot34(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc);
IMG_VOID ConvertVertexInputInstructionTypeless(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc);
IMG_VOID GetVSOutputSource(PINTERMEDIATE_STATE	psState,
						   PARG					psHwSource,
						   PUF_REGISTER			psInputSource,
						   IMG_UINT32			uChan,
						   PCODEBLOCK			psCodeBlock);
IMG_VOID UnpackTextureChannelToF32(PINTERMEDIATE_STATE	psState,
								   PCODEBLOCK			psCodeBlock,
								   IMG_UINT32			uFormat,
								   PARG					psDest,
								   IMG_UINT32			uPredSrc,
								   IMG_BOOL				bPredSrcNeg,
								   IMG_UINT32			uSrcRegType,
								   IMG_UINT32			uSrcRegNum,
								   IMG_UINT32			uSrcByteOffset,
								   IMG_BOOL				bIntegerChannels);
PCODEBLOCK ConvertSamplerInstructionF32(PINTERMEDIATE_STATE psState,
										PCODEBLOCK psCodeBlock,
										PUNIFLEX_INST psSrc,
										IMG_BOOL bConditional);
IMG_VOID GetRelativeIndex(PINTERMEDIATE_STATE psState,
						  PUF_REGISTER psSource,
						  PARG psHwSource);
IMG_VOID ConvertTextureArrayIndexToU16(PINTERMEDIATE_STATE	psState,
									   PCODEBLOCK			psCodeBlock,
									   IMG_UINT32			uTexture,
									   PARG					psFltArg,
									   IMG_UINT32			uFltComponent,
									   IMG_UINT32			uU16ArrayIndex);

/* icvt_i32.c */
PCODEBLOCK ConvertInstToIntermediateInt32(PINTERMEDIATE_STATE psState,
										  PCODEBLOCK psBlock,
										  PUNIFLEX_INST psSrc,
										  IMG_BOOL bConditional,
										  IMG_BOOL bStaticCond);
IMG_VOID IntegerNegate(PINTERMEDIATE_STATE		psState,
					   PCODEBLOCK				psCodeBlock,
					   PINST					psInsertInstBefore,
					   PARG						psSrc,
					   PARG						psResult);
IMG_VOID IntegerAbsolute(PINTERMEDIATE_STATE	psState,
					     PCODEBLOCK				psCodeBlock,
					     PARG					psSrc,
					     PARG					psResult);
IMG_VOID GenerateIntegerArithmetic(PINTERMEDIATE_STATE		psState,
								   PCODEBLOCK				psBlock,
								   PINST					psInsertBeforeInst,
								   UF_OPCODE				eOpcode,
								   PARG						psDestLow,
								   PARG						psDestHigh,
								   IMG_UINT32				uPredSrc,
								   IMG_BOOL					bPredNegate,
								   PARG						psArgA,
								   IMG_BOOL					bNegateA,
								   PARG						psArgB,
								   IMG_BOOL					bNegateB,
								   PARG						psArgC,
								   IMG_BOOL					bNegateC,
								   IMG_BOOL					bSigned);
IMG_VOID CreateComparisonInt32(PINTERMEDIATE_STATE	psState,
							   PCODEBLOCK			psCodeBlock,
							   IMG_UINT32			uPredDest,
							   UFREG_COMPOP			uCompOp,
							   PUF_REGISTER			psSrc1,
							   PUF_REGISTER			psSrc2,
							   IMG_UINT32			uChan,
							   UFREG_COMPCHANOP		eChanOp,
							   IMG_UINT32			uCompPredSrc,
							   IMG_BOOL				bCompPredNegate,
							   IMG_BOOL				bInvert);
IMG_VOID CreateComparisonInt16(PINTERMEDIATE_STATE	psState,
							   PCODEBLOCK			psCodeBlock,
							   IMG_UINT32			uPredDest,
							   UFREG_COMPOP			uCompOp,
							   PUF_REGISTER			psSrc1,
							   PUF_REGISTER			psSrc2,
							   IMG_UINT32			uChan,
							   UFREG_COMPCHANOP		eChanOp,
							   IMG_UINT32			uCompPredSrc,
							   IMG_BOOL				bCompPredNegate,
							   IMG_BOOL				bInvert);
IMG_VOID GetSourceTypeless(PINTERMEDIATE_STATE psState,
						   PCODEBLOCK psCodeBlock,
						   PUF_REGISTER psSource,
						   IMG_UINT32 uChan,
						   PARG psHwSource,
						   IMG_BOOL bAllowSourceMod,
						   PFLOAT_SOURCE_MODIFIER psSourceMod);
IMG_VOID GetDestinationTypeless(PINTERMEDIATE_STATE psState,
								PCODEBLOCK psCodeBlock,
								PUF_REGISTER psDest,
								IMG_UINT32 uChan,
								PARG psHwSource);
IMG_VOID ZeroExtendInteger(PINTERMEDIATE_STATE	psState,
						   PCODEBLOCK			psCodeBlock,
						   IMG_UINT32			uSrcBitWidth,
						   PARG					psSrc,
						   IMG_UINT32			uByteOffset,
						   PARG					psDest);
IMG_VOID SignExtendInteger(PINTERMEDIATE_STATE	psState,
					       PCODEBLOCK			psCodeBlock,
						   IMG_UINT32			uSrcBitWidth,
						   PARG					psSrc,
						   IMG_UINT32			uByteOffset,
						   PARG					psDest);
IMG_VOID ExpandCVTFLT2INTInstruction(PINTERMEDIATE_STATE	psState,
									 PINST					psInst);
IMG_VOID ExpandCVTINT2ADDRInstruction(PINTERMEDIATE_STATE	psState,
									  PINST					psInst);
IMG_VOID ExpandFTRCInstruction(PINTERMEDIATE_STATE		psState,
							   PINST					psInst);

/* icvt_f16.c */
PCODEBLOCK ConvertInstToIntermediateF16(PINTERMEDIATE_STATE psState,
										PCODEBLOCK psCodeBlock,
										PUNIFLEX_INST psSrc,
										IMG_BOOL bConditional,
										IMG_BOOL bStaticCond);
IMG_VOID GetSingleSourceF16(PINTERMEDIATE_STATE psState,
							PCODEBLOCK psCodeBlock,
							PUF_REGISTER psSource,
							PARG psHwSource,
							IMG_PUINT32 puComponent,
							IMG_UINT32 uChan,
							IMG_BOOL bAllowSourceMod,
							PFLOAT_SOURCE_MODIFIER psSourceMod,
							IMG_BOOL bNoF16HWConsts);
IMG_VOID GetSampleCoordinateArrayIndexF16(PINTERMEDIATE_STATE	psState,
										  PCODEBLOCK			psCodeBlock,
										  PUNIFLEX_INST			psInputInst,
										  IMG_UINT32			uCoordMask,
										  IMG_UINT32			uTexture,
										  IMG_UINT32			uArrayIndexTemp);
IMG_VOID GetProjectionF16(PINTERMEDIATE_STATE	psState,
						  PCODEBLOCK			psCodeBlock,
						  PUNIFLEX_INST			psInputInst,
						  PSAMPLE_PROJECTION	psProj,
						  UF_REGFORMAT			eCoordFormat);
IMG_VOID GetSampleCoordinatesF16(PINTERMEDIATE_STATE	psState,
								 PCODEBLOCK				psCodeBlock,
								 PUNIFLEX_INST			psInputInst,
								 IMG_UINT32				uDimensionality,
								 PSAMPLE_COORDINATES	psSampleCoordinates,
								 IMG_BOOL				bPCF);
IMG_VOID GetLODAdjustmentF16(PINTERMEDIATE_STATE	psState,
							 PCODEBLOCK				psCodeBlock,
							 PUNIFLEX_INST			psInputInst,
							 PSAMPLE_LOD_ADJUST		psLODAdjust);
IMG_VOID GetGradientsF16(PINTERMEDIATE_STATE	psState,
						 PCODEBLOCK				psCodeBlock,
						 PUNIFLEX_INST			psInputInst,
						 IMG_UINT32				uGradMask,
						 IMG_UINT32				uCoord,
						 PSAMPLE_GRADIENTS		psGradients);
IMG_VOID GetSampleIdxF16(PINTERMEDIATE_STATE	psState,
						 PCODEBLOCK				psCodeBlock,
						 PUNIFLEX_INST			psInputInst,
						 PARG					psSampleIdx);
IMG_VOID GetSingleSourceConvertToF32(PINTERMEDIATE_STATE psState,
									 PCODEBLOCK psCodeBlock,
									 PUF_REGISTER psSource,
									 PARG psHwSource,
									 IMG_UINT32 uChan,
									 IMG_BOOL bAllowSourceMod,
									 PFLOAT_SOURCE_MODIFIER psSourceMod);
#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
IMG_VOID EmitNormaliseF16(PINTERMEDIATE_STATE psState,
						  PCODEBLOCK psCodeBlock,
						  PARG psDest,
 						  ARG asOperand[],
						  IMG_UINT32 uSrcMask,
						  IMG_BOOL bVec4,
						  IMG_UINT32 uInputPred,
						  IMG_BOOL bSrcModNegate,
						  IMG_BOOL bSrcModAbsolute);
#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */
IMG_VOID ApplyFloatSourceModifier(PINTERMEDIATE_STATE		psState,
								  PCODEBLOCK				psBlock,
								  PINST						psInsertBeforeInst,
								  PARG						psSrcArg,
								  PARG						psDestArg,
								  UF_REGFORMAT				eSrcFmt,
								  PFLOAT_SOURCE_MODIFIER	psMod);

/* icvt_mem.c */
PCODEBLOCK ConvertInstToIntermediateMem(PINTERMEDIATE_STATE psState,
																				PCODEBLOCK psCodeBlock,
																				PUNIFLEX_INST psUFInst,
																				IMG_BOOL bConditional,
																				IMG_BOOL bStaticCond);

/* Which mutex are we using to lock on atomic instructions? */
#define ATOM_MUTEX_INDEX 0
/* Atomic function helpers */
IMG_BOOL IsInstructionAtomic(UF_OPCODE eOpcode);
IMG_VOID GenerateAtomicLock(PINTERMEDIATE_STATE psState,
                            PCODEBLOCK psCodeBlock);
IMG_VOID GenerateAtomicRelease(PINTERMEDIATE_STATE psState,
                               PCODEBLOCK psCodeBlock);
IMG_VOID GenerateAtomicLoad(PINTERMEDIATE_STATE psState,
                            PCODEBLOCK psCodeBlock,
							PARG psAddress,
							PARG psTarget);
IMG_VOID GenerateAtomicStore(PINTERMEDIATE_STATE psState,
                             PCODEBLOCK psCodeBlock,
							 PARG psAddress,
							 PARG psSource);

/* icvt_core.c */
#if defined(INPUT_UNIFLEX)
IMG_VOID ConvertToIntermediate(const PUNIFLEX_INST psOrigProg,
							   PINTERMEDIATE_STATE psState);
#endif /* defined(INPUT_UNIFLEX) */
#if defined(OUTPUT_USCHW)
USC_CHANNELFORM GetUnpackedChannelFormat(PINTERMEDIATE_STATE		psState,
										 IMG_UINT32					uSamplerIdx,
										 USC_CHANNELFORM			ePackedFormat);
IMG_VOID GetTextureSampleChannelLocation(PINTERMEDIATE_STATE	psState,
										 PSAMPLE_RESULT_LAYOUT	psResultLayout,
										 PSAMPLE_RESULT			psResult,
										 IMG_UINT32				uSrcChan,
										 USC_CHANNELFORM*		peFormat,
										 IMG_PUINT32			puSrcRegNum,
										 #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
										 IMG_PBOOL				pbSrcIsVector,
										 #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
										 IMG_PUINT32			puSrcChanOffset);
PCODEBLOCK ConvertSamplerInstructionCore(PINTERMEDIATE_STATE psState,
										 PCODEBLOCK psCodeBlock,
										 PUNIFLEX_INST psSrc,
										 PSAMPLE_RESULT_LAYOUT psLayout,
										 PSAMPLE_RESULT psResult,
										 IMG_BOOL bConditional);
IMG_VOID FreeSampleResultLayout(PINTERMEDIATE_STATE psState, PSAMPLE_RESULT_LAYOUT psSampleResultLayout);
IMG_VOID ConvertLoadBufferInstructionCore(PINTERMEDIATE_STATE psState,
										  PCODEBLOCK psCodeBlock,
										  PUNIFLEX_INST psSrc,
										  PSAMPLE_RESULT_LAYOUT psLayout,
										  PSAMPLE_RESULT psResult,
										  IMG_BOOL bConditional);
IMG_VOID ConvertLoadTiledInstructionCore(PINTERMEDIATE_STATE	psState,
										 PCODEBLOCK				psCodeBlock,
										 PUNIFLEX_INST			psInputInst, 
										 PARG					psBase,
										 PARG					psStride,
										 PSAMPLE_RESULT_LAYOUT	psLayout,
										 PSAMPLE_RESULT			psResult);
#endif  /* defined(OUTPUT_USCHW) */

#if defined(OUTPUT_USPBIN)
PCODEBLOCK ConvertSamplerInstructionCoreUSP(	PINTERMEDIATE_STATE	psState,
											PCODEBLOCK			psCodeBlock,
											PUNIFLEX_INST		psSrc,
											IMG_UINT32			uChanMask,
											IMG_UINT32			uChanSwizzle,
											PARG					psDest,
											IMG_BOOL				bConditional);
#endif /* defined(OUTPUT_USPBIN) */
IMG_VOID LoadMaxTexarrayIndex(PINTERMEDIATE_STATE			psState,
							  PCODEBLOCK					psCodeBlock,
							  IMG_UINT32					uTexture,
							  PARG							psArg);
IMG_VOID StoreIndexableTemp(PINTERMEDIATE_STATE psState,
							PCODEBLOCK psCodeBlock,
							PUF_REGISTER psDest,
							UF_REGFORMAT eFmt,
							IMG_UINT32 uStoreSrc);
IMG_VOID LoadStoreIndexableTemp(PINTERMEDIATE_STATE	psState,
								PCODEBLOCK			psCodeBlock,
								IMG_BOOL			bLoad,
								UF_REGFORMAT		eFmt,
								PUF_REGISTER		psSource,
								IMG_UINT32			uChanMask,
								IMG_UINT32			uTemp,
								IMG_UINT32			uDataReg);
IMG_VOID UnpackU24Channel(PINTERMEDIATE_STATE		psState,
						  PCODEBLOCK				psCodeBlock,
						  IMG_UINT32				uSrcRegNum,
						  IMG_UINT32				uSrcByteOffset,
						  IMG_UINT32				uTemp);
IMG_VOID LoadConstantNoHWReg(PINTERMEDIATE_STATE psState,
					  PCODEBLOCK psCodeBlock,
					  PINST psInsertBeforeInst,
					  PUF_REGISTER psSource,
					  IMG_UINT32 uSrcChan,
					  PARG psHwSource,
					  IMG_PUINT32 puComponent,
					  UF_REGFORMAT eFormat,
					  #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
					  IMG_BOOL bVectorResult,
					  #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
					  IMG_BOOL bC10Subcomponent,
					  IMG_UINT32 uCompOffset);
IMG_VOID LoadConstant(PINTERMEDIATE_STATE psState,
					  PCODEBLOCK psCodeBlock,
					  PUF_REGISTER psSource,
					  IMG_UINT32 uSrcChan,
					  PARG psHwSource,
					  IMG_PUINT32 puComponent,
					  IMG_BOOL bAllowSourceMod,
					  IMG_PBOOL pbNegate,
					  UF_REGFORMAT eFormat,
					  IMG_BOOL bC10Subcomponent,
					  IMG_UINT32 uCompOffset);
IMG_UINT32 GetTexformSwizzle(PINTERMEDIATE_STATE	psState,
							 IMG_UINT32				uSampler);
IMG_BOOL IsInputInstFlowControl(PUNIFLEX_INST psInst); //used by precovr.c
IMG_VOID ConvertVertexInputInstructionCore(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc, IMG_UINT32 uPredicate, IMG_UINT32 *puIndexReg, IMG_UINT32 *puStaticIdx);
PUNIFLEX_INST LabelToInst(PUNIFLEX_INST psProg, IMG_UINT32 uLabel); //used by precovr.c
PFIXED_REG_DATA AddFixedReg(PINTERMEDIATE_STATE	psState,
							IMG_BOOL bPrimary,
							IMG_BOOL bLiveAtShaderEnd,
							IMG_UINT32 uPhysicalRegType,
							IMG_UINT32 uPhysicalRegNum,
							IMG_UINT32 uConsecutiveRegsCount);
PCODEBLOCK GetShaderEndBlock(PINTERMEDIATE_STATE psState, PFIXED_REG_DATA psFixedVReg, IMG_UINT32 uRegIdx);
IMG_VOID ModifyFixedRegPhysicalReg(PUSC_LIST		psFixedRegList,
								   PFIXED_REG_DATA	psFixedReg,
								   IMG_UINT32		uPhysicalRegType,
								   IMG_UINT32		uPhysicalRegNum);
IMG_UINT32 FindRange(PINTERMEDIATE_STATE psState, IMG_UINT32 uConstsBuffNum, IMG_UINT32 uConst, IMG_PUINT32 puRangeStart, IMG_PUINT32 puRangeEnd);
IMG_VOID SetSampleGradientsDetails(PINTERMEDIATE_STATE	psState,
								   PINST				psSmpInst,
								   PSAMPLE_GRADIENTS	psGradients);
IMG_VOID GetGradientDetails(PINTERMEDIATE_STATE	psState,
						    IMG_UINT32			uTextureDimensionality,
						    UF_REGFORMAT		eCoordFormat,
							PSAMPLE_GRADIENTS	psGradients);
IMG_VOID EmitGradCalc(PINTERMEDIATE_STATE	psState,
					  PCODEBLOCK			psCodeBlock,
					  PINST					psInsertBeforeInst,
					  IMG_UINT32			uDims,
					  UF_REGFORMAT			eCoordFormat,
					  IMG_UINT32			uCoordTempStart,
					  PARG					asCoordArgs,
					  PARG					psLODBias,
					  PSAMPLE_GRADIENTS		psGradients);
IMG_VOID SetupSmpStateArgument(PINTERMEDIATE_STATE	psState,
							   PCODEBLOCK			psCodeBlock,
							   PINST				psInsertBeforeInst,
							   ARG					asStateArgs[],
							   IMG_UINT32			uTexture,
							   IMG_UINT32			uChunk,
							   IMG_BOOL				bTextureArray,
							   PARG					psTexureArrayIndex,
							   PSMP_IMM_OFFSETS		psImmOffsets,
							   IMG_UINT32			uTextureDimensionality,
							   IMG_BOOL				bSampleIdxPresent,
							   PARG					psSampleIdx);
IMG_VOID CheckFaceType(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, IMG_UINT32 uPredDest, IMG_BOOL bFrontFace);
IMG_BOOL CheckForStaticConst(PINTERMEDIATE_STATE	psState,
							 IMG_UINT32				uNum,
							 IMG_UINT32				uChan,
							 IMG_UINT32				uBufferIdx,
							 IMG_PUINT32			puValue);
IMG_BOOL GetStaticConstByteOffset(PINTERMEDIATE_STATE	psState, 
								  IMG_UINT32			uOffsetInBytes, 
								  IMG_UINT32			uLengthInBytes, 
								  IMG_UINT32			uBufferIdx,
								  IMG_PUINT32			puValue);
IMG_BOOL IsModifiableRegisterArray(PINTERMEDIATE_STATE psState, IMG_UINT32 uType, IMG_UINT32 uNumber);
IMG_UINT32 GetTextureCoordinateUsedChanCount(PINTERMEDIATE_STATE psState, IMG_UINT32 uTexture);
IMG_VOID ConvertPixelShaderResultArg(PINTERMEDIATE_STATE psState, PUF_REGISTER psInputArg, PARG psArg);

/* precovr.c */
IMG_VOID ConvertOperationsToInteger(PINTERMEDIATE_STATE psState,
									PINPUT_PROGRAM		psProg);
IMG_UINT32 SwizzleMask(IMG_UINT32 uSwiz, IMG_UINT32 uMask);
IMG_VOID OverrideFloat16Precisions(PINTERMEDIATE_STATE	psState,
								   PINPUT_PROGRAM		psProg);
PUNIFLEX_INST SkipInstBlock(PUNIFLEX_INST psFirstInst,
										UF_OPCODE eTerminate0,
										UF_OPCODE eTerminate1,
										UF_OPCODE eTerminate2);
IMG_UINT32 GetSourceLiveChans(PINTERMEDIATE_STATE psState, PUNIFLEX_INST psInst, IMG_UINT32 uSrc);

/* pregalloc.c */
IMG_VOID AssignPredicateRegisters(PINTERMEDIATE_STATE psState);
IMG_VOID SetupSyncStartInformation(PINTERMEDIATE_STATE psState);

/* domcalc.c */
IMG_VOID CalcDoms(PINTERMEDIATE_STATE psState, PCFG psCfg);
IMG_BOOL Dominates(PINTERMEDIATE_STATE psState, PCODEBLOCK psDom, PCODEBLOCK psCh);
IMG_BOOL PostDominated(PINTERMEDIATE_STATE psState, PCODEBLOCK psCh, PCODEBLOCK psPDom);
IMG_VOID ComputeLoopNestingTree(PINTERMEDIATE_STATE psState, PCODEBLOCK psDom);
IMG_BOOL IsLoopHeader(PCODEBLOCK psBlock);
IMG_UINT32 LoopDepth(PCODEBLOCK psBlock);
PCODEBLOCK FindLeastCommonDominator(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock1, PCODEBLOCK psBlock2, IMG_BOOL bPostDom);

/* asm.c */
IMG_UINT32 EncodeInst(PINTERMEDIATE_STATE	psState,
					  PINST					psInst,
					  IMG_PUINT32			puInst,
					  IMG_PUINT32			puBaseInst);

IMG_VOID EncodeNop(IMG_PUINT32 puCode);

typedef enum _EXECPRED_BRANCHTYPE_
{
	EXECPRED_BRANCHTYPE_NORMAL = 0,
	EXECPRED_BRANCHTYPE_ALLINSTSFALSE = 1,
	EXECPRED_BRANCHTYPE_ANYINSTTRUE = 2	
} EXECPRED_BRANCHTYPE;
IMG_UINT32 EncodeJump(PINTERMEDIATE_STATE	psState,
					  PUSEASM_CONTEXT		psUseasmContext,
					  IOPCODE				eOpcode,
					  IMG_UINT32			uLabel,
					  IMG_UINT32			uPredicate,
					  IMG_BOOL				bPredNegate,
					  IMG_PUINT32			puInst,
					  IMG_PUINT32			puBaseInst,
					  IMG_BOOL				bAssumeAlwaysSkipped,
					  IMG_BOOL				bSyncEnd,
					  IMG_BOOL				bSyncIfNotTaken,
					  EXECPRED_BRANCHTYPE	eExecPredBranchType);

IMG_UINT32 EncodeLabel(PUSEASM_CONTEXT	psUseasmContext,
					   PCSGX_CORE_DESC	psTarget,
					   IMG_UINT32		uLabel,
					   IMG_PUINT32		puInst,
					   IMG_PUINT32		puBaseInst);

/* indexreg.c */
IMG_VOID AssignHardwareIndexRegisters(PINTERMEDIATE_STATE	psState);

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
/* icvt_f32_vec.c */
PCODEBLOCK ConvertInstToIntermediateF32_Vec(	PINTERMEDIATE_STATE psState,
												PCODEBLOCK psCodeBlock,
												PUNIFLEX_INST psInputInst,
												IMG_BOOL bConditional,
												IMG_BOOL bStaticCond);
IMG_UINT32 ConvertSwizzle(PINTERMEDIATE_STATE	psState,
						  IMG_UINT32			uInSwiz);
IMG_VOID FixVectorSwizzles(PINTERMEDIATE_STATE	psState);
IMG_VOID GetDestinationF32_Vec(PINTERMEDIATE_STATE		psState,
							   PUF_REGISTER				psInputDest,
							   PARG						psDest);
IMG_VOID SetDestinationF32_Vec(PINTERMEDIATE_STATE		psState,
							   PUNIFLEX_INST			psInputInst,
							   PUF_REGISTER				psInputDest,
							   PINST					psInst);
IMG_VOID GetSourceF32_Vec(PINTERMEDIATE_STATE	psState,
						  PCODEBLOCK			psBlock,
						  PUF_REGISTER			psInputSrc,
						  IMG_UINT32			uUsedChanMask,
						  PINST					psInst,
						  IMG_UINT32			uSrcIdx);
IMG_VOID CopyVecSourceModifiers(PUF_REGISTER			psInputSrc,
							    PVEC_SOURCE_MODIFIER	psSourceMod);
IMG_VOID StoreDestinationF32(PINTERMEDIATE_STATE	psState,
							 PCODEBLOCK				psBlock,
							 PUNIFLEX_INST			psInputInst,
							 PUF_REGISTER			psInputDest,
							 IMG_UINT32				uTempVariable);
IMG_BOOL GetBaseDestinationF32_Vec(PINTERMEDIATE_STATE	psState,
								   PUF_REGISTER			psInputDest,
								   PARG					psDest);
IMG_VOID ConvertIntegerDotProductResult_Vec(PINTERMEDIATE_STATE	psState,
											PCODEBLOCK			psCodeBlock,
											PUNIFLEX_INST		psInputInst,
											IMG_UINT32			uDotProduct,
											IMG_BOOL			bSat);
IMG_VOID GetIteratedSource_Vec(PINTERMEDIATE_STATE	psState,
							   PCODEBLOCK			psBlock,
							   PUF_REGISTER			psInputSrc,
							   PARG					psGPISrc);

/* icvt_f16_vec.c */
IMG_VOID SetupTempVecSource(PINTERMEDIATE_STATE	psState,
							PINST				psInst,
						    IMG_UINT32			uSrcIdx,
							IMG_UINT32			uTempNum,
							UF_REGFORMAT		eTempFormat);
IMG_VOID SetupVectorSource(PINTERMEDIATE_STATE	psState,
						   PINST				psInst,
						   IMG_UINT32			uSrcIdx,
						   PARG					psSrc);
IMG_VOID SetupImmediateSourceU(PINST			psInst,
							  IMG_UINT32		uArgIdx,
							  IMG_UINT32		uImmValue,
							  UF_REGFORMAT		eImmFormat);
IMG_VOID SetupImmediateSource(PINST			psInst,
							  IMG_UINT32	uArgIdx,
							  IMG_FLOAT		fImmValue,
							  UF_REGFORMAT	eFormat);
PCODEBLOCK ConvertInstToIntermediateF16_Vec(	PINTERMEDIATE_STATE psState,
												PCODEBLOCK psCodeBlock,
												PUNIFLEX_INST psSrc,
												IMG_BOOL bConditional,
												IMG_BOOL bStaticCond);
IMG_VOID ConvertComplexOpInstructionF32F16_Vec(PINTERMEDIATE_STATE	psState,
											   PCODEBLOCK			psCodeBlock,
											   PUNIFLEX_INST		psInputInst,
											   IMG_BOOL				bF16);
IMG_VOID ConvertVectorInstructionF32F16_Vec(PINTERMEDIATE_STATE		psState,
											PCODEBLOCK				psCodeBlock,
											PUNIFLEX_INST			psInputInst,
											IMG_BOOL				bF16);
IMG_VOID ConvertVectorComparisonInstructionF32F16_Vec(PINTERMEDIATE_STATE	psState,
													  PCODEBLOCK			psCodeBlock,
													  PUNIFLEX_INST			psInputInst,
													  IMG_BOOL				bF16);
IMG_VOID ConvertReformatInstructionF16F32_Vec(PINTERMEDIATE_STATE	psState,
											  PCODEBLOCK			psCodeBlock,
											  PUNIFLEX_INST			psInputInst,
											  IMG_BOOL				bF16);
IMG_VOID GetSampleCoordinatesF16F32Vec(PINTERMEDIATE_STATE	psState,
									   PCODEBLOCK			psCodeBlock,
									   PUNIFLEX_INST		psInputInst,
									   IMG_UINT32			uDimensionality,
									   PSAMPLE_COORDINATES	psCoordinates,
									   IMG_BOOL				bPCF);
IMG_VOID GetProjectionF16F32Vec(PINTERMEDIATE_STATE			psState,
								PCODEBLOCK					psCodeBlock,
								PUNIFLEX_INST				psInputInst,
								PSAMPLE_PROJECTION			psProj,
								UF_REGFORMAT				eCoordFormat,
								IMG_BOOL					bF16);
IMG_VOID GetLODAdjustmentF16F32Vec(PINTERMEDIATE_STATE	psState,
								   PCODEBLOCK			psCodeBlock,
								   PUNIFLEX_INST		psInputInst,
								   UF_REGFORMAT			eSrcFmt,
								   PSAMPLE_LOD_ADJUST	psLODAdjust);
IMG_VOID GetGradientsF16F32Vec(PINTERMEDIATE_STATE	psState,
							   PCODEBLOCK			psCodeBlock,
							   PUNIFLEX_INST		psInputInst,
							   IMG_UINT32			uGradDim,
							   IMG_UINT32			uCoord,
							   UF_REGFORMAT			eSourceFormat,
							   PSAMPLE_GRADIENTS	psGradients);
IMG_VOID GetSourceF16_Vec(PINTERMEDIATE_STATE	psState,
						  PCODEBLOCK			psBlock,
						  PUF_REGISTER			psInputSrc,
						  IMG_UINT32			uUsedChanMask,
						  PINST					psInst,
						  IMG_UINT32			uSrcIdx);
IMG_VOID ConvertMovaInstructionVecFloat(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc, IMG_BOOL bF16);
IMG_VOID GenerateComparisonF32F16_Vec(PINTERMEDIATE_STATE	psState,
								      PCODEBLOCK			psCodeBlock,
									  IMG_UINT32			uPredDest,
									  UFREG_COMPOP			uCompOp,
									  PUF_REGISTER			psSrc1,
									  PUF_REGISTER			psSrc2,
									  USEASM_TEST_CHANSEL	eChanOp,
									  IMG_UINT32			uCompPredSrc,
									  IMG_BOOL				bCompPredNegate,
									  IMG_BOOL				bInvert);
IMG_VOID ConvertTexkillInstructionFloatVec(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psInputInst, IMG_BOOL bConditional);
IMG_VOID GenerateSrcModF32F16_Vec(PINTERMEDIATE_STATE	psState,
								  PCODEBLOCK			psCodeBlock,
								  IMG_UINT32			uSMod,
								  PARG					psGPIArg,
								  PARG					psUSArgs,
								  IMG_BOOL				bF16);
IMG_VOID GeneratePerChanPredicateMOV(PINTERMEDIATE_STATE	psState,
									 PCODEBLOCK				psCodeBlock,
									 PUF_REGISTER			psInputDest,
									 IMG_UINT32				uPredicate,
									 UF_REGFORMAT			eFormat,
									 IMG_UINT32				uTempDest);
IMG_VOID ConvertDOT2ADDInstructionF32F16_Vec(PINTERMEDIATE_STATE	psState,
											 PCODEBLOCK				psCodeBlock,
											 PUNIFLEX_INST			psInputInst,
											 IMG_BOOL				bF16);
IMG_VOID GenerateMinMax(PINTERMEDIATE_STATE	psState,
						PCODEBLOCK			psCodeBlock,
						PUNIFLEX_INST		psInputInst,
						IMG_UINT32			uDestMask,
						UF_OPCODE			eOperation,
						UF_REGFORMAT		eFormat,
						IMG_FLOAT			fConst);
PCODEBLOCK ConvertSamplerInstructionFloatVec(PINTERMEDIATE_STATE psState,
											 PCODEBLOCK psCodeBlock,
											 PUNIFLEX_INST psInputInst,
											 IMG_BOOL bConditional,
											 IMG_BOOL bF16);
IMG_VOID UnpackTextureFloatVec(PINTERMEDIATE_STATE		psState,
							   PCODEBLOCK				psCodeBlock,
							   PSAMPLE_RESULT_LAYOUT	psResultLayout,
							   PSAMPLE_RESULT			psResult,
							   PARG						psDest,
							   IMG_UINT32				uMask);
IMG_VOID UnpackGradients_Vec(PINTERMEDIATE_STATE	psState,
							 PCODEBLOCK				psCodeBlock,
							 PUNIFLEX_INST			psInputInst,
							 IMG_UINT32				uSrcRegNum,
							 IMG_UINT32				uGradDim,
							 IMG_UINT32				uAxis,
							 UF_REGFORMAT			eSourceFormat,
							 PSAMPLE_GRADIENTS		psGradients);
IMG_VOID StoreIntoVectorDest(PINTERMEDIATE_STATE	psState,
							 PCODEBLOCK				psCodeBlock,
							 PUNIFLEX_INST			psInputInst,
							 PUF_REGISTER			psInputDest);
IMG_VOID TestVecFloat(PINTERMEDIATE_STATE	psState,
					  PCODEBLOCK			psBlock,
					  IMG_UINT32			uTempReg,
					  UFREG_COMPOP			eCompOp);
IMG_VOID UnpackTextureChanFloatVec(PINTERMEDIATE_STATE		psState,
								   PCODEBLOCK				psCodeBlock,
								   PARG						psDest,
								   IMG_UINT32				uDestChan,
								   USC_CHANNELFORM			eSrcFormat,
								   IMG_UINT32				uSrcRegNum,
								   IMG_UINT32				uSrcByteOffset);
IMG_VOID ApplyProjectionVec(PINTERMEDIATE_STATE	psState,
							PCODEBLOCK			psBlock,
							PINST				psInsertBeforeInst,
							IMG_UINT32			uResultRegNum,
							PARG				psProjArg,
							PINST				psProjSrcInst,
							IMG_UINT32			uProjSrcIdx,
							IMG_UINT32			uProjTemp,
							IMG_UINT32			uProjChannel,
							UF_REGFORMAT		eCoordFormat,
							PINST				psCoordSrcIdx,
							IMG_UINT32			uCoordSrcIdx,
							IMG_UINT32			uInTemp,
							IMG_UINT32			uCoordMask);
IMG_VOID GetSampleCoordinateArrayIndexFlt_Vec(PINTERMEDIATE_STATE	psState,
											  PCODEBLOCK			psCodeBlock,
											  PUNIFLEX_INST			psInputInst,
											  IMG_UINT32			uSrcChan,
											  IMG_UINT32			uTexture,
											  IMG_UINT32			uArrayIndexTemp);
IMG_VOID ConvertTextureArrayIndexToU16_Vec(PINTERMEDIATE_STATE	psState,
										   PCODEBLOCK			psCodeBlock,
										   IMG_UINT32			uTexture,
										   IMG_UINT32			uFltArrayIndex,
										   UF_REGFORMAT			eArrayIndexFormat,
										   IMG_UINT32			uU16ArrayIndex);
/* vec34.c */
IMG_VOID DropUnusedVectorComponents(PINTERMEDIATE_STATE	psState,
									PINST				psInst,
									IMG_UINT32			uArgIdx);
IMG_VOID AssignVectorIRegs(PINTERMEDIATE_STATE	psState);
IMG_UINT32 CombineSwizzles(IMG_UINT32 uSwizzle1, IMG_UINT32	uSwizzle2);
IMG_BOOL CompareSwizzles(IMG_UINT32 uSwiz1, IMG_UINT32 uSwiz2, IMG_UINT32 uCmpMask);
IMG_UINT32 GetSwizzleSlotCount(PINTERMEDIATE_STATE psState, const INST *psInst);
IMG_VOID FixVectorSourceModifiers(PINTERMEDIATE_STATE	psState);
IMG_VOID LowerVectorRegisters(PINTERMEDIATE_STATE	psState);
IMG_VOID ReplaceImmediatesByVecConstants(PINTERMEDIATE_STATE	psState);
IMG_UINT32 GetPackMaximumSimultaneousConversions(PINTERMEDIATE_STATE	psState,
												 PINST					psInst);
IMG_VOID FixUnsupportedVectorMasks(PINTERMEDIATE_STATE	psState);
IMG_BOOL SwapVectorDestinations(PINTERMEDIATE_STATE	psState,
							    PINST				psInst,
								IMG_UINT32			uCurrentChan,
								IMG_UINT32			uNewChan,
								IMG_BOOL			bCheckOnly);
IMG_VOID SwapVectorSources(PINTERMEDIATE_STATE	psState, 
						   PINST				psInst1, 
						   IMG_UINT32			uArg1Idx, 
						   PINST				psInst2,
						   IMG_UINT32			uArg2Idx);
IMG_UINT32 GetWideSourceMask(IOPCODE eOpcode);
IMG_UINT32 GetIRegOnlySourceMask(IOPCODE eOpcode);
IMG_BOOL IsSwizzleSupported(PINTERMEDIATE_STATE	psState,
							PINST				psInst,
						    IOPCODE				eOpcode,
							IMG_UINT32			uSwizzleSlotIdx,
							IMG_UINT32			uSwizzle,
							IMG_UINT32			uChannelMask,
							IMG_PUINT32			puMatchedSwizzle);
IMG_VOID GenerateVectorDualIssue(PINTERMEDIATE_STATE	psState);
IMG_BOOL CombineVMOVs(PINTERMEDIATE_STATE psState);
IMG_VOID ReplaceMOVsWithVMOVs(PINTERMEDIATE_STATE psState);
IMG_VOID Vectorise(PINTERMEDIATE_STATE psState);
IMG_VOID CombineDependantIdenticalInsts(PINTERMEDIATE_STATE psState);
IMG_VOID CombineChannels(PINTERMEDIATE_STATE psState);
IMG_VOID ReplaceUnusedArguments(PINTERMEDIATE_STATE psState);
IMG_VOID RegroupVMULComputations(PINTERMEDIATE_STATE psState);
IMG_VOID OptimiseConstLoads(PINTERMEDIATE_STATE psState);
IMG_VOID ExpandCVTFLT2ADDRInstruction(PINTERMEDIATE_STATE psState, PINST psInst);
IMG_BOOL FoldConstants(PINTERMEDIATE_STATE psState);
IMG_VOID SimplifySwizzlesOnConsts(PINTERMEDIATE_STATE psState);
IMG_VOID NormaliseVectorLengths(PINTERMEDIATE_STATE	psState);
IMG_BOOL VectorSources12Commute(PINST psInst);
IMG_BOOL ConvertUnifiedStoreVMADToGPIVMAD(PINTERMEDIATE_STATE	psState,
										  PINST					psInst,
										  IMG_BOOL				bSwapSrc01);
IMG_BOOL TrySwapVectorSources(PINTERMEDIATE_STATE	psState,
							  PINST					psInst,
							  IMG_UINT32			uArg1Idx,
							  IMG_UINT32			uArg2Idx,
							  IMG_BOOL				bCheckSwizzles);
IMG_BOOL CanUpdateSwizzleToAccessSourceTopHalf(PINTERMEDIATE_STATE	psState,
											   PINST				psInst,
											   IMG_UINT32			uSrcSlotIdx,
											   IMG_PUINT32		puNewSwizzle);
IMG_UINT32 ChanMaskToByteMask(PINTERMEDIATE_STATE psState,
							  IMG_UINT32 uInMask,
							  IMG_UINT32 uInOffset,
							  UF_REGFORMAT eFormat);
IMG_BOOL InstUsesF16ToF32AutomaticConversions(PINTERMEDIATE_STATE psState, 
											  PINST psInst,
											  IMG_PBOOL pbDestIsF16,
											  IMG_PUINT32 puF16SrcMask);
IMG_BOOL IsSwizzleSetSupported(PINTERMEDIATE_STATE	psState,
							   IOPCODE				eOpcode,
							   PINST				psInst,
							   IMG_UINT32			auNewLiveChansInArg[],
							   IMG_UINT32			auSwizzleSlot[]);
IMG_BOOL IsValidVectorSourceModifier(PINST					psInst,
									 IOPCODE				eOpcode,
									 IMG_UINT32				uSlotIdx,
									 PVEC_SOURCE_MODIFIER	psSrcMod);
typedef struct _IREG_ASSIGNMENT
{
	IMG_BOOL	bDestIsIReg;
	IMG_UINT32	auIRegSource[VECTOR_MAX_SOURCE_SLOT_COUNT];
} IREG_ASSIGNMENT, *PIREG_ASSIGNMENT;
IMG_BOOL IsPossibleDualIssueWithVMOV(PINTERMEDIATE_STATE	psState,
									 PINST					psInst,
									 PIREG_ASSIGNMENT		psInstIRegAssigment,
									 PINST					psVMOVInst,
									 PIREG_ASSIGNMENT		psVMOVIRegAssigment,
									 IMG_UINT32				uLiveChansInVMOVDest,
									 IMG_BOOL				bCreateDual,
									 PINST*					ppsDualIssueInst);
IMG_BOOL GetImmediateVectorChan(PINTERMEDIATE_STATE psState, 
								PINST				psInst, 
								IMG_UINT32			uSrcIdx, 
								IMG_UINT32			uChanIdx,
								IMG_PUINT32			puChanValue);
IMG_BOOL IsImmediateVectorSource(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSrcIdx);
IMG_BOOL IsReplicatedImmediateVector(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSrcIdx, IMG_UINT32 uImmValue);
IMG_VOID ExpandVTRCInstruction(PINTERMEDIATE_STATE psState, PINST psInst);
IMG_VOID ExpandVMOVCBITInstruction(PINTERMEDIATE_STATE psState, PINST psInst);
IMG_VOID ExpandCVTFLT2INTInstruction_Vec(PINTERMEDIATE_STATE	psState,
										 PINST					psInst);
IMG_VOID ExpandInvalidVectorPackToFloat(PINTERMEDIATE_STATE psState, PINST psInst);
IMG_VOID ExpandInvalidVectorPackToFixedPoint(PINTERMEDIATE_STATE	psState, 
											 PINST					psInst);
IMG_VOID BaseSplitVectorInst(PINTERMEDIATE_STATE	psState,
							 PINST					psVecInst,
							 IMG_UINT32				uFirstDestMask,
							 PINST*					ppsFirstInst,
							 IMG_UINT32				uSecondDestMask,
							 PINST*					ppsSecondInst);
IMG_BOOL IsNonConstantSwizzle(IMG_UINT32	uSwizzle,
							  IMG_UINT32	uChannelMask,
							  IMG_PUINT32	puMatchedSwizzle);
IMG_VOID CombineSourceModifiers(PVEC_SOURCE_MODIFIER	psFirst,
							    PVEC_SOURCE_MODIFIER	psSecond,
								PVEC_SOURCE_MODIFIER	psResult);
IMG_VOID MoveVectorSource(PINTERMEDIATE_STATE	psState, 
						  PINST					psDestInst, 
						  IMG_UINT32			uDestArgIdx, 
						  PINST					psSrcInst, 
						  IMG_UINT32			uSrcArgIdx,
						  IMG_BOOL				bMoveSourceModifier);
IMG_VOID CopyVectorSourceArguments(PINTERMEDIATE_STATE	psState, 
								   PINST				psDestInst, 
								   IMG_UINT32			uDestArgIdx, 
								   PINST				psSrcInst, 
								   IMG_UINT32			uSrcArgIdx);
IMG_BOOL IsUniformVectorSource(PINTERMEDIATE_STATE	psState,
							   PINST				psInst,
							   IMG_UINT32			uSourceSlot);
#define GetChan(SWIZ, CHAN) (((SWIZ) >> ((CHAN) * USEASM_SWIZZLE_FIELD_SIZE)) & USEASM_SWIZZLE_VALUE_MASK)
#define SetChan(SWIZ, CHAN, SEL) \
		(((SWIZ) & ~(USEASM_SWIZZLE_VALUE_MASK << (USEASM_SWIZZLE_FIELD_SIZE * (CHAN)))) | \
		 (((SEL) & USEASM_SWIZZLE_VALUE_MASK) << (USEASM_SWIZZLE_FIELD_SIZE * (CHAN))))
extern IMG_UINT32 const g_auReplicateSwizzles[];
typedef struct _SWIZZLE_SEL
{
	IMG_BOOL	bIsConstant;
	IMG_UINT32	uSrcChan;
	IMG_UINT32	uF32Value;
	IMG_UINT32	uF16Value;
} SWIZZLE_SEL, *PSWIZZLE_SEL;
extern const SWIZZLE_SEL g_asSwizzleSel[];
extern const USEASM_SWIZZLE_SEL g_aeChanToSwizzleSel[];
typedef struct _VDUAL_OP_DESC
{
	IMG_UINT32	uSrcCount;
	IMG_BOOL	bVector;
	IMG_BOOL	bVectorResult;
	/*	
		TRUE if this dual-issue operation is valid as the primary operation in a VEC4 dual-issue instruction.
	*/
	IMG_BOOL	bValidAsVec4Primary;
	#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
	IMG_PCHAR	pszName;
	#endif /* defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
} VDUAL_OP_DESC, *PVDUAL_OP_DESC;
extern VDUAL_OP_DESC const g_asDualIssueOpDesc[VDUAL_OP_COUNT];
IMG_UINT32 ExpandFunctionVectorParameter(PINTERMEDIATE_STATE				psState,
									     PREGISTER_REMAP					psRemap,
									     PFUNC_INOUT						psOldParam,
									     PFUNC_INOUT						asNewParams,
									     IMG_UINT32							uNewArgCount);
IMG_INT32 ExpandVectorCallArgument(PINTERMEDIATE_STATE				psState,
								   PREGISTER_REMAP					psRemap,
								   PINST							psCallInst,
								   IMG_BOOL							bDest,
								   PARG								asArgs,
								   PFUNC_INOUT						psParam,
								   IMG_INT32						iInArg,
								   IMG_INT32						iOutArg);
IMG_BOOL ConvertSingleInstToDualIssue(PINTERMEDIATE_STATE	psState,
									  PINST					psInst,
									  PIREG_ASSIGNMENT		psIRegAssignment,
									  IMG_UINT32			uUnifiedStoreSrcMask,
									  IMG_BOOL				bAvailableIReg,
									  IMG_BOOL				bCheckOnly,
									  PINST*				ppsDualInst,
									  PARG*					ppsDummyIRegDest);
IMG_VOID FixDroppedUnusedVectorComponents(PINTERMEDIATE_STATE	psState, 
										  PINST					psInst,
										  IMG_UINT32			uVecArgIdx);
IMG_UINT32 GetVectorCopySourceArgument(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uDest);
IMG_UINT32 GetVPCKFLTFLT_EXPSourceOffset(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uDestChan);
#define VECTOR_SOURCE0_MASK			(1 << (SOURCE_ARGUMENTS_PER_VECTOR * 0))
#define VECTOR_SOURCE1_MASK			(1 << (SOURCE_ARGUMENTS_PER_VECTOR * 1))
#define VECTOR_SOURCE01_MASK		(VECTOR_SOURCE0_MASK | VECTOR_SOURCE1_MASK)
typedef struct _INST_MODIFICATIONS
{
	IOPCODE		eNewOpcode;
	IMG_BOOL	bSwapSources01;
	IMG_UINT32	auNewSwizzle[VECTOR_MAX_SOURCE_SLOT_COUNT];
	IMG_BOOL	abNewSelUpper[VECTOR_MAX_SOURCE_SLOT_COUNT];
} INST_MODIFICATIONS, *PINST_MODIFICATIONS;
typedef struct _VEC_NEW_ARGUMENT
{
	IMG_BOOL		bIsIReg;
	UF_REGFORMAT	eFormat;
} VEC_NEW_ARGUMENT, *PVEC_NEW_ARGUMENT;

typedef struct _VEC_NEW_ARGUMENTS
{
	IMG_UINT32			uDestCount;
	VEC_NEW_ARGUMENT	asDest[VECTOR_MAX_DESTINATION_COUNT];
	VEC_NEW_ARGUMENT	asSrc[VECTOR_MAX_SOURCE_SLOT_COUNT];
} VEC_NEW_ARGUMENTS, *PVEC_NEW_ARGUMENTS;
IMG_BOOL IsValidModifiedVectorInst(PINTERMEDIATE_STATE	psState,
								   IMG_BOOL				bExtraEffort,
								   PINST				psVecInst,
								   IMG_UINT32			auNewPreSwizzleLiveChans[],
								   PVEC_NEW_ARGUMENTS	psNewArguments,
								   IMG_UINT32			auNewSwizzle[],
								   IMG_BOOL				abNewSelectUpper[],
								   PINST_MODIFICATIONS	psInstMods);
IMG_BOOL SplitVectorInstWithFold(PINTERMEDIATE_STATE	psState,
								 PINST					psVecInst,
								 PVEC_NEW_ARGUMENTS		psNewArguments,
								 IMG_BOOL				bCheckOnly,
								 PINST*					ppsXYInst,
								 PINST*					ppsZWInst);
IMG_BOOL VectorInstPerArgumentF16F32Selection(IOPCODE eOpcode);
IMG_VOID ModifyVectorInst(PINTERMEDIATE_STATE psState, PINST psPartInst, PINST_MODIFICATIONS psInstMods);
IMG_VOID FixDSXDSY(PINTERMEDIATE_STATE psState);
IMG_VOID ExpandPCKU8U8Inst(PINTERMEDIATE_STATE psState, PINST psOrigPCKInst);
IMG_BOOL IsVTESTExtraSwizzles(PINTERMEDIATE_STATE psState, IOPCODE eOpcode, PINST psInst);
IMG_BOOL HasIndependentSourceSwizzles(PINTERMEDIATE_STATE psState, PINST psInst);
IMG_BOOL IsModifiedSwizzleSetSupported(PINTERMEDIATE_STATE	psState,
									   PINST				psInst,
									   IOPCODE				eOpcode,
									   IMG_UINT32			uSwizzleSlotIdx,
									   IMG_UINT32			uSwizzle,
									   IMG_PUINT32			puMatchedSwizzle);
IMG_VOID CombineVecAddsMoves(PINTERMEDIATE_STATE psState);
IMG_VOID SetImmediateVector(PINTERMEDIATE_STATE psState, 
							PINST				psInst, 
							IMG_UINT32			uSrcIdx, 
							IMG_UINT32			uImm);
IMG_VOID FixNegatedPerChannelPredicates(PINTERMEDIATE_STATE psState);
IMG_VOID ReducePredicateValuesBitWidth(PINTERMEDIATE_STATE psState);
IMG_BOOL IsNonSSAVectorSource(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSlot);
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

IMG_BOOL IsBlockDominatedByPreSplitBlock(	PINTERMEDIATE_STATE	psState,
											PCODEBLOCK			psCodeBlock);

FORCE_INLINE IMG_UINT32 TestInstructionUsage(PINTERMEDIATE_STATE psState, IOPCODE eOpcode)
{
	return (!SafeListIsEmpty(&psState->asOpcodeLists[eOpcode])) ? 1 : 0;
}

FORCE_INLINE IMG_UINT32 TestBlockForInstGroup(CODEBLOCK *psBlock, IMG_UINT32 uGroupFlag)
{
	return (psBlock->uInstGroupCreated & uGroupFlag);
}

PUNIFLEX_INST	AllocInputInst(	PINTERMEDIATE_STATE		psState,
								PINPUT_PROGRAM			psProg,
								PUNIFLEX_INST			psOrigInst);

IMG_VOID		CopyInputInst(	PUNIFLEX_INST psDest, PUNIFLEX_INST psSrc);
IMG_VOID		FreeInputProg(PINTERMEDIATE_STATE psState, PINPUT_PROGRAM *ppsInputProg);
PCODEBLOCK 		AddUnconditionalPredecessor(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock);

/* DAG Related functions */
typedef IMG_BOOL (*PFN_ARG_CONSTRAINT)(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uArgIdx, 
				   								IMG_BOOL bChangePoint, PINST psScopeStart, PINST psEndScopeEnd, IMG_PVOID pvData);
typedef IMG_VOID (*PFN_ARG_REPLACE_SRC)(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uArgIdx, const ARG *pcsSubstitue);
typedef IMG_VOID (*PFN_ARG_REPLACE_DEST)(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uDstIdx, const ARG *pcsSubstitue);
typedef IMG_VOID (*PFN_INST_UPDATE)(PINTERMEDIATE_STATE psState, PINST psInst);

IMG_VOID		GenerateInstructionDAG(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_UINT32 uFlags);
IMG_VOID		FreeInstructionDAG(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock);
IMG_BOOL		SearchInstPatternInDAG(const PINTERMEDIATE_STATE psState, const PINST psInst,
									const PATTERN_RULE * psRule, const IMG_UINT32 uMaxDepth, PINST *ppsMatchInst);
IMG_VOID		GetLivenessSpanForArgumentDAG(PINTERMEDIATE_STATE psState,
											  PINST psPoint,
											  IMG_UINT32 uArgIdx,
											  PINST *ppsStartInst,
											  PINST *ppsOldDestStart,
											  PINST *ppsEndInst);
IMG_BOOL		ApplyConstraintArgumentReplacementDAG(PINTERMEDIATE_STATE psState,
													  PINST psPoint,
													  IMG_UINT32 uArgIdx,
													  PFN_ARG_CONSTRAINT pfnConstraint,
													  PFN_ARG_CONSTRAINT pfnDestinationConstraint,
													  PFN_ARG_REPLACE_SRC pfnUpdateSrcReplacement,
													  PFN_ARG_REPLACE_DEST pfnUpdateDstReplacement,
													  PFN_INST_UPDATE pfnInstUpdate,
													  IMG_BOOL bContraintCheckOnly,
													  const ARG *pcsSubstitue,
													  IMG_PVOID	*pvData);
IMG_VOID		GetLivenessSpanForDestDAG(	PINTERMEDIATE_STATE	psState,
											PINST				psPoint, 
											IMG_UINT32			uDestIdx,
											PINST				*ppsOldDestStart,
											PINST				*ppsEndInst);
IMG_BOOL 		ApplyConstraintDestinationReplacementDAG(	PINTERMEDIATE_STATE psState,
															PINST				psPoint,
															IMG_UINT32			uDestIdx,
															PFN_ARG_CONSTRAINT	pfnSourceConstraint,
															PFN_ARG_CONSTRAINT	pfnDestinationConstraint,
															PFN_ARG_REPLACE_SRC pfnUpdateSrcReplacement,
															PFN_ARG_REPLACE_DEST pfnUpdateDstReplacement,
															PFN_INST_UPDATE		pfnInstUpdate,
															IMG_BOOL			bContraintCheckOnly,
															const ARG			*pcsSubstitue,
															IMG_PVOID	*pvData);
#if defined(DEBUG)
IMG_BOOL		VerifyDAG(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock);
#endif

#if defined(TRACK_REDUNDANT_PCONVERSION)
typedef enum _USC_PCONVERT_SETTINGS_
{
	USC_DISABLE_INSTCYCLE_TEST			= (IMG_UINT32)(0x00000001),
	USC_DISABLE_TEMPREG_TEST			= (IMG_UINT32)(0x00000002),
	USC_DISABLE_ALL_TESTS				= (IMG_UINT32)(0xFFFFFFFF)
}USC_PCONVERT_SETTINGS_FLAGS;

typedef enum _USC_PCONVERT_DEFAULT_CONVERSION_SETTING_
{
	USC_PCONVERT_PRIMARY,
	USC_PCONVERT_SECONDARY,
}USC_PCONVERT_DEFAULT_CONVERSION_SETTING;


PINPUT_PROGRAM	OverridePrecisions(const PINTERMEDIATE_STATE	psState, PUNIFLEX_INST psOrigProg);

IMG_VOID		DoOnAllBlocksForFunction(PINTERMEDIATE_STATE psState, const PFUNC psFunc, BLOCK_PROC pfBlockProc,
									 IMG_PVOID pvParam);
IMG_UINT32		ConsolidateFormatConversionPatterns(PINTERMEDIATE_STATE psState, 
													const PFUNC			psFunc,
													IMG_UINT32 *puPrecisionConversionPaths);
IMG_BOOL		TryDefaultPrecisionEnforcingSetting(PINTERMEDIATE_STATE psState, USC_PCONVERT_DEFAULT_CONVERSION_SETTING eSetting);
IMG_VOID		TrackRedundantFormatConversions(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNull);
IMG_BOOL 		PConvertCompareCostings(PINTERMEDIATE_STATE			psState,
										PINTERMEDIATE_STATE			psStateOriginal,
										PINTERMEDIATE_STATE			psStateEnfored,
										USC_PCONVERT_SETTINGS_FLAGS	eEnforcementTestSettings);
#endif /* defined(TRACK_REDUNDANT_PCONVERSION) */
IMG_UINT32		GetAvailableInternalRegsBetweenInterval(PINTERMEDIATE_STATE psState, PINST psFromInst, PINST psToInst);
IMG_BOOL		SelectInternalReg(IMG_UINT32 uIRegAvailable, IMG_UINT32 uVectorMask, IMG_PUINT32 puRegIregIndex);
IMG_VOID		InitCfg(PINTERMEDIATE_STATE psState, PCFG psCfg, PFUNC psFunc);
PCFG			AllocateCfg(PINTERMEDIATE_STATE psState,  PFUNC psFunc);
IMG_VOID		DetachBlockFromCfg(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PCFG psCfg);
IMG_VOID		AttachBlockToCfg(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PCFG psCfg);
IMG_VOID		AttachSubCfgs(PINTERMEDIATE_STATE psState, PCFG psCfg);
IMG_VOID		SetDestTempArg(PINTERMEDIATE_STATE	psState, PINST psInst, IMG_UINT32 uDestIdx, IMG_UINT32 uTempNum,UF_REGFORMAT eTempFmt);
IMG_VOID		SetSourceTempArg(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSrcIdx, IMG_UINT32 uTempNum, UF_REGFORMAT eTempFmt);

typedef struct _EDGE_INFO
{
	PCODEBLOCK	psEdgeParentBlock;
	IMG_UINT32	uEdgeSuccIdx;
} EDGE_INFO, *PEDGE_INFO;

typedef struct _EDGE_INFO_LISTENTRY
{
	EDGE_INFO		sEdgeInfo;
	USC_LIST_ENTRY	sListEntry;
} EDGE_INFO_LISTENTRY, *PEDGE_INFO_LISTENTRY;
PEDGE_INFO AppendEdgeInfoToList(PINTERMEDIATE_STATE psState, PUSC_LIST psEdgeInfoLst);
IMG_BOOL IsEdgeInfoPresentInList(PUSC_LIST psEdgeInfoLst, PCODEBLOCK psEdgeParentBlock, IMG_UINT32 uEdgeSuccIdx);
IMG_VOID FreeEdgeInfoList(PINTERMEDIATE_STATE psState, PUSC_LIST psEdgeInfoLst);
IMG_BOOL IsSampleRateStillSplit(PINTERMEDIATE_STATE psState);
#endif /* USCSHRDH */

/******************************************************************************
 End of file (uscshrd.h)
******************************************************************************/
