/******************************************************************************
 * Name         : useopt.h
 * Title        : USE Optimiser
 * Author       : Matthew Wahab
 * Created      : Jan 2008
 *
 * Copyright    : 2002-2008 by Imagination Technologies Limited.
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
 * $Log: useopt.h $
 ******************************************************************************/

#ifndef __USEASM_USEOPT_H
#define __USEASM_USEOPT_H

#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "assert.h"
#include "setjmp.h"

#include "sgxdefs.h"

#if defined(SUPPORT_SGX541)
#include <sgxvec34usedefs.h>
#endif /* defined(SUPPORT_SGX541) */

#include "pvr_debug.h"
#include "img_defs.h"
#include "img_types.h"

#include "sgxdefs.h"

#include "use.h"
#include "useasm.h"
#include "usetab.h"
#include "osglue.h"

/*
 * Type definitions
 */
/*
 * USEASM_ALLOC: malloc
 * pvContext is PUSEASM_CONTEXT
*/
typedef IMG_PVOID (IMG_CALLCONV *USEASM_ALLOCFN)(IMG_PVOID pvContext,
												 IMG_UINT32 uSize);
/*
 * USEASM_FREE: free
 * pvContext is PUSEASM_CONTEXT
 */
typedef IMG_VOID (IMG_CALLCONV *USEASM_FREEFN)(IMG_PVOID pvContext,
												IMG_PVOID pvBlock);


/*
 * Status values
 */
typedef enum _USEOPT_STATUS_
{
	USEOPT_INVALID_PROGRAM,    // Failed: Invalid program
	USEOPT_MEMALLOC,           // Failed: Memory allocation

	USEOPT_FAILED,             // Failed: Generic failure

	USEOPT_OK,                 // No errors.
} USEOPT_STATUS;

/*
 * Data structures
 */

/*
  USEOPT_DATA: Data needed for the useasm optimiser pass.
 */
typedef struct _USEOPT_DATA_
{
	/*
	 * Status
	 */
	USEOPT_STATUS eStatus;

	/*
	 * Memory allocation
	 *
	 * pfnAlloc: Allocate memory
	 * pfnFree : Free memory allocated with pfnAlloc.
	 */
	USEASM_ALLOCFN pfnAlloc;
	USEASM_FREEFN pfnFree;

	/*
	 * Input data:
	 * -----------
	 *
	 * uMaxXReg: Maximum register number of type X.
	 *
	 * If any uMaxXReg == 0 then optimisations requiring register numbers will
	 * be ignore registers of type X.
	 *
	 * uNumTempRegs  : Number of temporary registers in the shader
	 * uNumPARegs    : Number of primary attribute registers in the shader
	 * uNumOutputRegs: Number of hardware output registers in the shader
	 *
	 * auKeepTempReg : Bitvector of temporary registers which cannot be modified
	 * auKeepPAReg   : Bitvector of primary attribute registers which cannot be modified
	 * auKeepPAReg   : Bitvector of hardware output registers which cannot be modified
	 *
	 * Temp. register rX is subject to optimisation
	 * iff GetBit(auKeepTempReg, X) == 0
	 *
	 * PA register paX is subject to optimisation
	 * iff GetBit(auKeepPAReg, X) == 0
	 *
	 * Output register oX is subject to optimisation
	 * iff GetBit(auKeepOutReg, X) == 0
	 */
	IMG_UINT32 uNumTempRegs;		// type USEASM_REGTYPE_TEMP
	IMG_UINT32 uNumPARegs;      	// type USEASM_REGTYPE_PRIMATTR
	IMG_UINT32 uNumOutputRegs;    	// type USEASM_REGTYPE_OUTPUT

	IMG_UINT32 *auKeepTempReg;    	// Bitvector
	IMG_UINT32 *auKeepPAReg;    	// Bitvector
	IMG_UINT32 *auKeepOutputReg;  	// Bitvector
	/*
	 * Output data:
	 * ------------
	 * asOutRegs: Array of registers which outputs for the program.
	 * uNumOutRegs: Number of registers which are outputs for the programs
	 *              (== size of array asOutRegs)
	 *
	 * If uNumOutRegs == 0 then optimisations based on program output
	 * registers will be skipped
	 */
	IMG_UINT32 uNumOutRegs;    // Number of shader outputs
	USE_REGISTER *asOutRegs;   // List of shader output registers

	/*
	 * Program information
	 * -------------------
     * psProgram: First instruction in list making up the program.
     *
	 * psStart: Program instruction to start from.
	 * Must be an instruction in the program passed to the assembler
	 */
    PUSE_INST psProgram; 
    PUSE_INST psStart;
} USEOPT_DATA, *PUSEOPT_DATA;
#define USEOPT_DATA_DEFAULT \
	{ USEOPT_OK, NULL, NULL, 0, 0, 0, NULL, NULL, NULL, 0, 0, NULL, NULL }

/*
 * Functions
 */

/*
 * UseoptAssembler: Optimise and assemble a Use program.
 */
IMG_UINT32
IMG_CALLCONV UseoptAssembler(PSGX_CORE_INFO psTarget,
                             IMG_PUINT32 puInst,
                             IMG_UINT32 uCodeOffset,
                             PUSEASM_CONTEXT psContext,
                             PUSEOPT_DATA psOptData);


/*
 * UseoptAssembler: Optimise a Use program but don't assemble it.
 */

IMG_VOID
IMG_CALLCONV UseoptProgram(PSGX_CORE_INFO psTarget,
						   PUSEASM_CONTEXT psContext,
						   PUSEOPT_DATA psUseoptData);


/*
 * Utility functions
 */

/* Sop instructions */
typedef struct _USE_SOP_SPEC_
{
	USEASM_OPCODE uOpcode;          // The sop to form

	USEASM_PRED uPred;              // Predicate
	IMG_BOOL bFmtControl;           // Whether format control is set

	USE_REGISTER sOut;	  			// The output register (where result is written)
	USE_REGISTER asSrc[3]; 		    // Source registers (src0, src1, src2)
	                                // asSrc[0] is only used by sop3 specification

	IMG_UINT32 uWriteMask;          // Write mask to use if bHasWriteMask is set (only for sopwm)

	IMG_BOOL bCompSrc1;                 // Src1 Complement
	USEASM_INTSRCSEL uSrc1ColFactor;	// factor for Src1 colour operation
	IMG_BOOL bOneMinusSrc1Col;

	IMG_BOOL bCompSrc1Alpha;            // Src1A Complement
	USEASM_INTSRCSEL uSrc1AlphaFactor;	// factor for Src1 alpha operation
	IMG_BOOL bOneMinusSrc1Alpha;

	USEASM_INTSRCSEL uSrc2ColFactor;		// factor for Src2 colour operation
	IMG_BOOL bOneMinusSrc2Col;

	USEASM_INTSRCSEL uSrc2AlphaFactor;	// factor for Src2 alpha operation (ignored for sop3)
	IMG_BOOL bOneMinusSrc2Alpha;
    
	USEASM_INTSRCSEL uCop;				// Colour operations
	USEASM_INTSRCSEL uAop;    			// Alpha operations

	IMG_BOOL bNegCop;				// Negate the result of the colour operation
	IMG_BOOL bNegAop;				// Negate the result of the alpha operation
} USE_SOPSPEC, *PUSE_SOPSPEC;

IMG_VOID InitSopSpec(PUSE_SOPSPEC psSpec);
IMG_BOOL EncodeSopSpec(PUSE_SOPSPEC psSpec,
					   PUSE_INST* apsInst);


/* Bitvector functions */
IMG_UINT32 UseoptBitVecSize(const IMG_UINT32 uNumBits);
IMG_BOOL UseoptGetBit(const IMG_UINT32 auBitVec[],
					  const IMG_UINT32 uBit);
IMG_VOID UseoptSetBit(IMG_UINT32 auBitVec[],
					  IMG_UINT32 uBit,
					  IMG_BOOL bBitValue);


#endif /* __USEASM_USEOPT_H */

/******************************************************************************
 End of file (useopt.h)
******************************************************************************/
