/***************************************************************************//**
 * @file           usegen.c
 *
 * @brief          Helper functions to build various pieces of common USE code
 *
 * @details        Implementation of various USE code builders
 *
 * @author         Donald Scorgie
 *
 * @date           16/10/2008
 *
 * @Version        $Revision: 1.102 $
 *
 * @Copyright     Copyright 2008-2010 by Imagination Technologies Limited.
 *                All rights reserved. No part of this software, either material
 *                or conceptual may be copied or distributed, transmitted,
 *                transcribed, stored in a retrieval system or translated into
 *                any human or computer language in any form by any means,
 *                electronic, mechanical, manual or otherwise, or disclosed
 *                to third parties without the express written permission of
 *                Imagination Technologies Limited, Home Park Estate,
 *                Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 ******************************************************************************/

/******************************************************************************
Modifications :-

$Log: usegen.c $

******************************************************************************/

#include "img_types.h"
#include "servicesext.h"
#include "sgxdefs.h"
#include "usecodegen.h"
#include "usegen.h"
#include "img_3dtypes.h"

/*
 * These are similar to the PDS defines and are used to
 * give us access to debug facilities. 
 */
#if defined(USEGEN_BUILD_D3DVISTA)
#pragma message ("Building USEGen for D3D Vista")
#include "debug.h"
#define USEGEN_DPF(a) DPFUSEGEN((a))
#define USEGEN_ASSERT(a) ASSERT((a))
#define USEGEN_DBG_BREAK DBG_BREAK
#else
#define USEGEN_DPF(a) PVR_DPF((PVR_DBG_ERROR, (a)))
#define USEGEN_ASSERT(a) PVR_ASSERT((a))
#define USEGEN_DBG_BREAK PVR_DBG_BREAK
#endif

/******************************************************************************
 * Calculation Functions
 ******************************************************************************/

/***********************************************************************//**
* Calculate the size required to write a state update USE fragment for the
* specified number of writes
*
* @param ui32NumWrites		Number of updates to be written
* @return Size (in bytes) required to hold the resulting program (written
* using USEGenWriteStateEmitFragment)
* @note This size is only for the USEGenWriteStateEmitFragment fragment
**************************************************************************/
IMG_INTERNAL IMG_UINT32 USEGenCalculateStateSize (IMG_UINT32 ui32NumWrites)
{
	IMG_UINT32 ui32Size = 0;

	/* Quick check to ensure we're being asked for something sensible */
	USEGEN_ASSERT (ui32NumWrites > 0);

	/* Each write can be done in batches of 16 */
	while (ui32NumWrites > 16)
	{
		ui32Size++;
		ui32NumWrites -= 16;
	}
	/* Any left over can be done in a single write */
	if (ui32NumWrites > 0)
	{
		ui32Size++;
	}

	/* Convert the size into bytes */
	ui32Size = USE_INST_TO_BYTES(ui32Size);

	/* Easy */
	return ui32Size;

}


/***********************************************************************//**
* Writes a state update USE program.  This program will copy the given
* number of PAs to the output buffer (starting at o0).
*
* @param pui32Base			The base (linear) address to write the program to
* @param ui32NumWrites		Number of updates to be written
* @param ui32Start			The first primary attribute to copy from.  This
*							will probably be 0 in most cases
* @return Pointer to the end of the written program
* @note This is only a fragment of the copy code.  It is up to the caller
* to add the correct emit and any preamble required
**************************************************************************/
IMG_INTERNAL IMG_PUINT32 USEGenWriteStateEmitFragment (IMG_PUINT32 pui32Base,
														IMG_UINT32 ui32NumWrites,
														IMG_UINT32 ui32Start)
{
	IMG_UINT32 ui32RepCount;
	IMG_PUINT32 pui32Current = pui32Base;
	IMG_UINT32 ui32PA = ui32Start;
	IMG_UINT32 ui32Output = 0;

	USEGEN_ASSERT (pui32Current);
	USEGEN_ASSERT (ui32NumWrites > 0);

	while (ui32NumWrites > 0)
	{
		ui32RepCount = 16;
		if (ui32NumWrites < 16)
		{
			ui32RepCount = ui32NumWrites;
		}

		BuildMOV ((PPVR_USE_INST)pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS,
				  (ui32RepCount-1) /* Repeat count */,
				  EURASIA_USE1_RCNTSEL,
				  USE_REGTYPE_OUTPUT, ui32Output,
				  USE_REGTYPE_PRIMATTR, ui32PA,
				  0 /* Src mod */);
		pui32Current += 2;

		ui32NumWrites -= ui32RepCount;	/* PRQA S 3382 */ /* ui32NumWriters is always no less than ui32RepCount. */
		ui32PA += ui32RepCount;
		ui32Output += ui32RepCount;

	}

	return pui32Current;
}

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)

/***********************************************************************//**
* Write a phase instruction at the given address
*
* @param	pui32Base			The linear address of the location to write the
*								phase instruction to
* @param	pui32NextPhase		If there is a next phase, this is the SGX
*								address of it.  If this is the final phase, this
*								parameter is ignored
* @param	ui32NumTemps		The number of temporary registers used by the
*								next phase
* @param	eDependency			The dependency for beginning the next phase
* @param	bPerInstance		If set, the next phase is per-instance
* @param	bLastPhase			If set, the phase is written as a final phase
* @param	bEnd				If set, the end flag is set on the phase
*								instruction
* @return Pointer to the end of the written block
**************************************************************************/
IMG_INTERNAL IMG_PUINT32 USEGenWritePhaseFragment (IMG_PUINT32 pui32Base,
													IMG_UINT32 ui32NextPhase,
													IMG_UINT32 ui32NumTemps,
													USEGEN_PHASE_DEPENDANCY eDependency,
													IMG_BOOL bPerInstance,
												   IMG_BOOL bPerSample,
													IMG_BOOL bLastPhase,
													IMG_BOOL bEnd)
{
	IMG_UINT32 ui32Dependency;
	switch (eDependency)
	{
	case USEGEN_NO_DEPENDANT:
		ui32Dependency = EURASIA_USE_OTHER2_PHAS_WAITCOND_NONE;
		break;
	case USEGEN_VCB_DEPENDANT:
		ui32Dependency = EURASIA_USE_OTHER2_PHAS_WAITCOND_VCULL;
		break;
	case USEGEN_PT_DEPENDANT:
		ui32Dependency = EURASIA_USE_OTHER2_PHAS_WAITCOND_PT;
		break;
	default:
		USEGEN_DPF ("Warning: Unknown dependency for PHAS. Setting to"
					" None");
		USEGEN_DBG_BREAK;
		ui32Dependency = EURASIA_USE_OTHER2_PHAS_WAITCOND_NONE;
		break;
	}

#if defined(FIX_HW_BRN_29019)
	/* if BRN 29019 is present, a full rate PHAS instruction must not be
	 * the first instruction in a phase
	 * Also, make sure this is not skipped if invalid
	 */
	if (bPerSample)
	{
		BuildMOV ((PPVR_USE_INST)pui32Base,
				  EURASIA_USE1_EPRED_ALWAYS,
				  0 /* Repeat count */,
				  EURASIA_USE1_RCNTSEL,
				  USE_REGTYPE_PRIMATTR, 0,
				  USE_REGTYPE_PRIMATTR, 0,
				  0 /* Src mod */);

		pui32Base[1] &= ~EURASIA_USE1_SKIPINV;

		pui32Base += USE_INST_LENGTH;
	}
#endif

	if (bLastPhase)
	{
		BuildPHASLastPhase ((PPVR_USE_INST) pui32Base,
							bEnd ? EURASIA_USE1_OTHER2_PHAS_END : 0);

	}
	else
	{
		BuildPHASImmediate ((PPVR_USE_INST) pui32Base,
							bEnd ? EURASIA_USE1_OTHER2_PHAS_END : 0,
							bPerInstance ?
							 EURASIA_USE_OTHER2_PHAS_MODE_PERINSTANCE :
							 EURASIA_USE_OTHER2_PHAS_MODE_PARALLEL,
							bPerSample ?
							EURASIA_USE_OTHER2_PHAS_RATE_SAMPLE :
							EURASIA_USE_OTHER2_PHAS_RATE_PIXEL,
							ui32Dependency,
							(ui32NumTemps >> 2), /* Temps are in units of 4 */
							ui32NextPhase / EURASIA_USE_INSTRUCTION_SIZE);
	}
	pui32Base += USE_INST_LENGTH;

	return pui32Base;
}

#if defined(FIX_HW_BRN_31988)
IMG_INTERNAL IMG_PUINT32 USEGenWriteBRN31988Fragment (IMG_PUINT32 pui32Current)
{
	/* load 128-bits with predicated out instruction
	nop.nosched
	and.tstnz p0, #0, #0
	p0 ldad.f4		pa0,	[pa0, #0], drc0;
	wdf		drc0;
    */
	BuildNOP((PPVR_USE_INST)pui32Current, EURASIA_USE1_NOSCHED, IMG_FALSE);
	
	pui32Current += USE_INST_LENGTH;

	BuildALUTST((PPVR_USE_INST)pui32Current,
							EURASIA_USE1_EPRED_ALWAYS,
							1,
							EURASIA_USE1_TEST_CRCOMB_AND,
							USE_REGTYPE_TEMP, 0,
							0,
							USE_REGTYPE_IMMEDIATE,
							0,
							USE_REGTYPE_IMMEDIATE,
							0,
							0,
							EURASIA_USE1_TEST_STST_NONE,
							EURASIA_USE1_TEST_ZTST_NOTZERO,
							EURASIA_USE1_TEST_CHANCC_SELECT0,
							EURASIA_USE0_TEST_ALUSEL_BITWISE,
							EURASIA_USE0_TEST_ALUOP_BITWISE_AND);
	
	pui32Current += USE_INST_LENGTH;

	BuildLD((PPVR_USE_INST)pui32Current,
			EURASIA_USE1_EPRED_P0,
			3,
			0,
			0,
			EURASIA_USE1_LDST_DTYPE_32BIT,
			EURASIA_USE1_LDST_AMODE_ABSOLUTE,
			EURASIA_USE1_LDST_IMODE_NONE,
			USE_REGTYPE_PRIMATTR,
			0,
			USE_REGTYPE_PRIMATTR,
			0,
			USE_REGTYPE_IMMEDIATE,
			0,
			USE_REGTYPE_IMMEDIATE,
			0,
			IMG_FALSE);

	pui32Current += USE_INST_LENGTH;

	BuildWDF((PPVR_USE_INST)pui32Current, 0);
	pui32Current += USE_INST_LENGTH;

	return pui32Current;

}
#endif

#endif

#if defined(SGX_FEATURE_VCB)
/***********************************************************************//**
* Writes the end of the pre-VCB-cull vertex shader
*
* @param	pui32Base			The linear address to write to
* @return	Pointer to the end of the written block
**************************************************************************/
IMG_INTERNAL IMG_PUINT32 USEGenWriteEndPreCullVtxShaderFragment (IMG_PUINT32 pui32Base)
{
	IMG_PUINT32 pui32Current = pui32Base;

	USEGEN_ASSERT (pui32Current);

	/*
	 * The pre-cull vertex shader should do a single emit
	 * to the VCB of the vertex.  This emit shouldn't free the
	 * partition (as it's used in the post-cull phase
	 * and should end the program
	 */

	BuildEMITVCB ((PPVR_USE_INST) pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS,
				  EURASIA_USE1_END |
				  (SGX545_USE1_EMIT_VCB_EMITTYPE_VERTEX <<
				   SGX545_USE1_EMIT_VCB_EMITTYPE_SHIFT) |
				  SGX545_USE1_EMIT_VERTEX_TWOPART,
				  USE_REGTYPE_TEMP,
				  0,
				  USE_REGTYPE_IMMEDIATE,
				  0,
				  USE_REGTYPE_IMMEDIATE,
				  0,
				  0 /* INCP */,
				  IMG_FALSE /* Don't free the partition */);
	pui32Current += USE_INST_LENGTH;

	/* Done. */
	return pui32Current;


}
#endif


/***********************************************************************//**
* Writes the end of the vertex shader USE program
*
* @param	pui32Base			The linear address to write to
* @return	Pointer to the end of the written block
**************************************************************************/
IMG_INTERNAL IMG_PUINT32 USEGenWriteEndVtxShaderFragment (IMG_PUINT32 pui32Base)
{
	IMG_PUINT32 pui32Current = pui32Base;

	USEGEN_ASSERT (pui32Current);

	/*
	 * The final portion of the vertex shader should do an
	 * emit of the vertex to the MTE.  
	 */
#if defined(SGX545) || defined(SGX543) || defined(SGX544) || defined(SGX554)
	BuildEMITMTE((PPVR_USE_INST)pui32Current,
				EURASIA_USE1_EPRED_ALWAYS,
				(EURASIA_USE1_EMIT_MTECTRL_VERTEX <<
				EURASIA_USE1_EMIT_MTECTRL_SHIFT) |
#if defined(SGX545)
				SGX545_USE1_EMIT_VERTEX_TWOPART |
#endif
				EURASIA_USE1_END,
				USE_REGTYPE_TEMP,
				0,
				USE_REGTYPE_IMMEDIATE,
				0,
				USE_REGTYPE_IMMEDIATE,
				0,
				0, /* INCP */
				IMG_TRUE /* Free partition */);

	pui32Current += USE_INST_LENGTH;
#else /* defined(SGX545) || defined(SGX543) || defined(SGX544) || defined(SGX554) */
	BuildEMITMTE ((PPVR_USE_INST) pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS,
				  EURASIA_USE1_END,
				  USE_REGTYPE_TEMP,
				  0,
				  USE_REGTYPE_IMMEDIATE,
				  0,
				  USE_REGTYPE_IMMEDIATE,
				  0,
				  0,
				  IMG_TRUE,
				  0,
				  0,
				  EURASIA_USE1_EMIT_MTECTRL_VERTEX);
	pui32Current += USE_INST_LENGTH;
#endif /* defined(SGX545) || defined(SGX543) || defined(SGX544) || defined(SGX554) */

	/* Done */
	return pui32Current;
}

/***********************************************************************//**
 * Write the end of a vertext shader program when guard band clipping
 * is enabled
 *
 * As this is only invoked when there is no geometry shader, we can
 * only output to the first viewport, so we only need to clip against
 * this.
 *
 * @note In VCB-enabled chips, this will write the post-VCB phase
 * of the task
 *
 * @param 	pui32Base				The base (linear) address to
 *									write the program to
 * @param 	ui32SecondaryGBBase		Secondary attribute containing
 *									the viewport transform words
 * @return Pointer to the end of the written program
 **************************************************************************/
IMG_INTERNAL IMG_PUINT32 USEGenWriteEndVtxShaderGBFragment(IMG_PUINT32 pui32Base,
															IMG_UINT32 ui32SecondaryGBBase)
{
	IMG_PUINT32 pui32Current = pui32Base;
#if defined(SGX_FEATURE_USE_VEC34)
	/* Vectorised support isn't done yet */
	PVR_UNREFERENCED_PARAMETER(ui32SecondaryGBBase);
#else

	USEGEN_ASSERT (pui32Current);

	/*
	 * With VP Guad Band Clipping, we need to transform our position
	 * using:
	 * 	x = x * c0 + c1 * w
	 * 	y = y * c2 + c3 * w
	 * where x, y, w are clip space vertex coords
	 * and c0 - c3 are the transform coefficients which must be
	 * loaded prior to use.
	 */

	if (ui32SecondaryGBBase > 127)
	{
		/* With a value > 127, we can't use immediates.  Instead,
		 * LIMM to r0 and then MOV. */
		BuildLIMM((PPVR_USE_INST) pui32Current, EURASIA_USE1_EPRED_ALWAYS,
				  0 /* Flags */, USE_REGTYPE_TEMP, 0, ui32SecondaryGBBase);
		pui32Current += USE_INST_LENGTH;

		BuildMOV((PPVR_USE_INST) pui32Current,
				 EURASIA_USE1_EPRED_ALWAYS,
				 0, EURASIA_USE1_RCNTSEL,
				 USE_REGTYPE_IDX, 1, /* 1 for i.l */
				 USE_REGTYPE_TEMP, 0,
				 0 /* Src mod */);
		pui32Current += USE_INST_LENGTH;
	}
	else
	{
		/* Otherwise we can load directly */
		BuildMOV((PPVR_USE_INST) pui32Current,
				 EURASIA_USE1_EPRED_ALWAYS,
				 0, EURASIA_USE1_RCNTSEL,
				 USE_REGTYPE_IDX, 1, /* 1 for i.l */
				 USE_REGTYPE_IMMEDIATE, ui32SecondaryGBBase,
				 0 /* Src mod */);
		pui32Current += USE_INST_LENGTH;
	}


	/*
	 * 0x60 == BSEL Secondary, IDXSEL Lower, and offset 0
	 * Note the funky ordering we put them into for when we actually use
	 * them
	 */
	BuildMOV((PPVR_USE_INST) pui32Current,
			 EURASIA_USE1_EPRED_ALWAYS,
			 0, EURASIA_USE1_RCNTSEL,
			 USE_REGTYPE_TEMP, 2,
			 USE_REGTYPE_INDEXED, 0x60, 0);
	pui32Current += USE_INST_LENGTH;

	BuildMOV((PPVR_USE_INST) pui32Current,
			 EURASIA_USE1_EPRED_ALWAYS,
			 0, EURASIA_USE1_RCNTSEL,
			 USE_REGTYPE_TEMP, 0,
			 USE_REGTYPE_INDEXED, 0x61, 0);
	pui32Current += USE_INST_LENGTH;

	BuildMOV((PPVR_USE_INST) pui32Current,
			 EURASIA_USE1_EPRED_ALWAYS,
			 0, EURASIA_USE1_RCNTSEL,
			 USE_REGTYPE_TEMP, 3,
			 USE_REGTYPE_INDEXED, 0x62, 0);
	pui32Current += USE_INST_LENGTH;

	BuildMOV((PPVR_USE_INST) pui32Current,
			 EURASIA_USE1_EPRED_ALWAYS,
			 0, EURASIA_USE1_RCNTSEL,
			 USE_REGTYPE_TEMP, 1,
			 USE_REGTYPE_INDEXED, 0x63, 0);
	pui32Current += USE_INST_LENGTH;

	/* Now we can do our first multiply (with SMLSI) */
	BuildSMLSI((PPVR_USE_INST) pui32Current,
			   0, 0, 0, 0, /*< Limits */
			   1, 1, 0, 0); /*< Increments */
	pui32Current += USE_INST_LENGTH;

	BuildMAD((PPVR_USE_INST) pui32Current,
			 EURASIA_USE1_EPRED_ALWAYS,
			 1, EURASIA_USE1_RCNTSEL,
			 USE_REGTYPE_TEMP, 0,
			 USE_REGTYPE_TEMP, 0, 0,
			 USE_REGTYPE_OUTPUT, 3, 0,
			 USE_REGTYPE_SPECIAL,
			 EURASIA_USE_SPECIAL_CONSTANT_ZERO, 0);
	pui32Current += USE_INST_LENGTH;

	/* Reset our SMLSI and do the second MAD */
	BuildSMLSI((PPVR_USE_INST) pui32Current,
			   0, 0, 0, 0, /*< Limits */
			   1, 1, 1, 1); /*< Increments */
	pui32Current += USE_INST_LENGTH;

	BuildMAD((PPVR_USE_INST) pui32Current,
			 EURASIA_USE1_EPRED_ALWAYS,
			 1, EURASIA_USE1_RCNTSEL,
			 USE_REGTYPE_OUTPUT, 0,
			 USE_REGTYPE_TEMP, 2, 0,
			 USE_REGTYPE_OUTPUT, 0, 0,
			 USE_REGTYPE_TEMP, 0, 0);
	pui32Current += USE_INST_LENGTH;

	/* And do the emit */
#if defined(SGX545) || defined(SGX543) || defined(SGX544) || defined(SGX554)
	BuildEMITMTE((PPVR_USE_INST)pui32Current,
				 EURASIA_USE1_EPRED_ALWAYS,
				 (EURASIA_USE1_EMIT_MTECTRL_VERTEX <<
				  EURASIA_USE1_EMIT_MTECTRL_SHIFT) |
#if defined(SGX545)
				 SGX545_USE1_EMIT_VERTEX_TWOPART |
#endif
				 EURASIA_USE1_END,
				 USE_REGTYPE_TEMP,
				 0,
				 USE_REGTYPE_IMMEDIATE,
				 0,
				 USE_REGTYPE_IMMEDIATE,
				 0,
				 0, /* INCP */
				 IMG_TRUE /* Free partition */);

	pui32Current += USE_INST_LENGTH;
#else /* defined(SGX545) || defined(SGX543) || defined(SGX544) || defined(SGX554) */
	BuildEMITMTE ((PPVR_USE_INST) pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS,
				  EURASIA_USE1_END,
				  USE_REGTYPE_TEMP,
				  0,
				  USE_REGTYPE_IMMEDIATE,
				  0,
				  USE_REGTYPE_IMMEDIATE,
				  0,
				  0,
				  IMG_TRUE,
				  0,
				  0,
				  EURASIA_USE1_EMIT_MTECTRL_VERTEX);
	pui32Current += USE_INST_LENGTH;
#endif /* defined(SGX545) || defined(SGX543) || defined(SGX544) || defined(SGX554) */

#endif /* defined(SGX_FEATURE_USE_VEC34) */

	/* In all cases, return pui32Current */
	return pui32Current;

}

/***********************************************************************//**
* Writes a state update USE program.  This program will copy the given
* number of PAs to the output buffer (starting at o0). It will also do a
* VCB/MTE emit as necessary
*
* @param pui32Base			The base (linear) address to write the program to
* @param ui32NumWrites		Number of updates to be written
* @param ui32Start			The first primary attribute to copy from.  This
*							will probably be 0 in most cases
* @param bComplex			Whether this is a complex state emit
* @return Pointer to the end of the written program
**************************************************************************/
IMG_INTERNAL IMG_PUINT32 USEGenWriteStateEmitProgram (IMG_PUINT32 pui32Base,
													  IMG_UINT32 ui32NumWrites,
													  IMG_UINT32 ui32Start,
													  IMG_BOOL bComplex)
{
	IMG_PUINT32 pui32Current = pui32Base;

	IMG_UINT32 ui32EmitSrc1;
	IMG_UINT32 ui32AdditionalFlags = 0;

	USEGEN_ASSERT (pui32Current);

#if !defined(SGX543) && !defined(SGX544) && !defined (SGX545) && !defined(SGX554)
	ui32EmitSrc1 = ui32NumWrites;
#else
	ui32EmitSrc1 = 0;
#endif

#if defined(SGX545)
	if (bComplex)
	{
		ui32EmitSrc1 |= (EURASIA_MTEEMIT1_COMPLEX |
						 EURASIA_MTEEMIT1_COMPLEX_PHASE2);
	}

	if (ui32NumWrites > (EURASIA_OUTPUT_PARTITION_SIZE*2))
	{
		ui32AdditionalFlags |= SGX545_USE1_EMIT_STATE_THREEPART;
	}
#else
	/* On non-SGX545, CG works differently and these aren't needed here */
	PVR_UNREFERENCED_PARAMETER(ui32AdditionalFlags);
	PVR_UNREFERENCED_PARAMETER(bComplex);
#endif

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	/* With unlimited phases, this will always be the final phase */

	/* Write the PHAS instruction */
	pui32Current = USEGenWritePhaseFragment (pui32Current, 0,
											 0, USEGEN_NO_DEPENDANT, IMG_FALSE,
											 IMG_FALSE, IMG_TRUE, IMG_FALSE);
#endif

#if defined(FIX_HW_BRN_31988)

	pui32Current = USEGenWriteBRN31988Fragment(pui32Current);

#endif

	pui32Current = USEGenWriteStateEmitFragment(pui32Current, ui32NumWrites, ui32Start);

#if defined(SGX_FEATURE_VCB)
	BuildEMITVCB ((PPVR_USE_INST) pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS,
				  (SGX545_USE1_EMIT_VCB_EMITTYPE_STATE <<
				   SGX545_USE1_EMIT_VCB_EMITTYPE_SHIFT) |
				  ui32AdditionalFlags,
				  USE_REGTYPE_TEMP,
				  0,
				  USE_REGTYPE_IMMEDIATE,
				  ui32EmitSrc1,
				  USE_REGTYPE_IMMEDIATE,
				  0,
				  0 /* INCP */,
				  IMG_FALSE /* Don't free the partition yet */);

	pui32Current += USE_INST_LENGTH;

	/* Put in a WOP to allow the VCB to finish with out output buffer */
	BuildWOP ((PPVR_USE_INST) pui32Current,
			  0, 0);
	pui32Current += USE_INST_LENGTH;

#endif

	/* Slightly different encodings for EMITMTE */
#if defined(SGX545) || defined(SGX543) || defined(SGX544) || defined(SGX554)

	BuildEMITMTE((PPVR_USE_INST)pui32Current,
				EURASIA_USE1_EPRED_ALWAYS,
				(EURASIA_USE1_EMIT_MTECTRL_STATE <<
				EURASIA_USE1_EMIT_MTECTRL_SHIFT) |
				EURASIA_USE1_END |
				ui32AdditionalFlags,
				USE_REGTYPE_OUTPUT,
				0,
				USE_REGTYPE_IMMEDIATE,
				ui32EmitSrc1,
				USE_REGTYPE_IMMEDIATE,
				0,
				0, /* INCP */
				IMG_TRUE /* Free partition */);

#else
	BuildEMITMTE((PPVR_USE_INST)pui32Current,
				 EURASIA_USE1_EPRED_ALWAYS,
				 EURASIA_USE1_END,
				 USE_REGTYPE_OUTPUT,
				 0,
				 USE_REGTYPE_IMMEDIATE,
				 ui32EmitSrc1,
				 USE_REGTYPE_IMMEDIATE,
				 0,
				 0, /* INCP */
				 IMG_TRUE, /* Free partition */
				 0, /* advance */ 
				 0, /* id */
				 EURASIA_USE1_EMIT_MTECTRL_STATE);
#endif

	pui32Current += USE_INST_LENGTH;

	return pui32Current;
}

/***********************************************************************//**
* Write the accum pixel program when we've got a render target that requires
* 2 partitions for it's emit
*
* @param	pui32Base				The linear address to write to
*
* @return	Pointer to the end of the written block
*
**************************************************************************/
IMG_INTERNAL IMG_PUINT32 USEGenWriteAccum2PProgram (IMG_PUINT32 pui32Base)
{
	IMG_PUINT32 pui32Current = pui32Base;

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	pui32Current = USEGenWritePhaseFragment(pui32Current,
											0,
											0,
											USEGEN_NO_DEPENDANT,
											IMG_FALSE,
											IMG_FALSE,
											IMG_TRUE,
											IMG_FALSE);
#endif

		BuildMOV ((PPVR_USE_INST)pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS,
				  0 /* Repeat count */,
				  EURASIA_USE1_RCNTSEL,
				  USE_REGTYPE_OUTPUT, 1,
				  USE_REGTYPE_PRIMATTR, 0,
				  0 /* Src mod */);
		pui32Current += 2;
		BuildMOV ((PPVR_USE_INST)pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS,
				  0 /* Repeat count */,
				  EURASIA_USE1_RCNTSEL  | EURASIA_USE1_END,
				  USE_REGTYPE_OUTPUT, 0,
				  USE_REGTYPE_PRIMATTR, 1,
				  0 /* Src mod */);
		pui32Current += 2;

		return pui32Current;
}

/***********************************************************************//**
* Write the accum vertex program
*
* @param	pui32Base				The linear address to write to
*			eProgramType			The type of program to write
*			uBaseAddress			The device virtual address to write to
*			uCodeHeapBase			The device virtual address of the code heap
*			ui32CodeHeapBaseIndex	The index of the code heap
*
* @return	Pointer to the end of the written block
*
**************************************************************************/
IMG_INTERNAL IMG_PUINT32 USEGenWriteSpecialObjVtxProgram (IMG_PUINT32 pui32Base, USEGEN_SPECIALOBJ_TYPE eProgramType,
														IMG_DEV_VIRTADDR uBaseAddress, IMG_DEV_VIRTADDR uCodeHeapBase, 
														IMG_UINT32 ui32CodeHeapBaseIndex)
{
#if defined(SGX_FEATURE_VCB)
	IMG_UINT32 ui32PostCullAddress;
	IMG_DEV_VIRTADDR uPostCullAddress;
#endif
	IMG_PUINT32 pui32Current = pui32Base;

	USEGEN_ASSERT (pui32Current);

	PVR_UNREFERENCED_PARAMETER(uBaseAddress);
	PVR_UNREFERENCED_PARAMETER(uCodeHeapBase);
	PVR_UNREFERENCED_PARAMETER(ui32CodeHeapBaseIndex);

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
#if defined(SGX_FEATURE_VCB)

	switch(eProgramType)
	{
		case USEGEN_CLEAR:
			uPostCullAddress.uiAddr = uBaseAddress.uiAddr + USE_INST_TO_BYTES(3);
			break;
		case USEGEN_CLEAR_PACKEDCOL:
			uPostCullAddress.uiAddr = uBaseAddress.uiAddr + USE_INST_TO_BYTES(8);
			break;
		case USEGEN_ACCUM:
			uPostCullAddress.uiAddr = uBaseAddress.uiAddr + USE_INST_TO_BYTES(5);
			break;
		case USEGEN_SCISSOR:
			uPostCullAddress.uiAddr = uBaseAddress.uiAddr + USE_INST_TO_BYTES(5);
			break;
		default:
			uPostCullAddress.uiAddr = 0; // Invalid program type.
			break;
	}

#if defined(FIX_HW_BRN_31988)
	uPostCullAddress.uiAddr = uPostCullAddress.uiAddr + USE_INST_TO_BYTES(4);
#endif

	ui32PostCullAddress = GetUSEPhaseAddress(uPostCullAddress,
											 uCodeHeapBase, 
											ui32CodeHeapBaseIndex);

	BuildPHASImmediate ((PPVR_USE_INST) pui32Base,
						0, /* end */
						EURASIA_USE_OTHER2_PHAS_MODE_PARALLEL,
						EURASIA_USE_OTHER2_PHAS_RATE_PIXEL,
						EURASIA_USE_OTHER2_PHAS_WAITCOND_VCULL,
						0, /* Temps */
						ui32PostCullAddress);
	
	pui32Current += USE_INST_LENGTH;

#else
	/* Write the PHAS instruction */
	pui32Current = USEGenWritePhaseFragment (pui32Current, 0,
											 0, USEGEN_NO_DEPENDANT, IMG_FALSE,
											 IMG_FALSE, IMG_TRUE, IMG_FALSE);
#endif /* SGX_FEATURE_VCB */ 
#endif /* SGX_FEATURE_USE_UNLIMITED_PHASES */

#if defined(FIX_HW_BRN_31988)

	pui32Current = USEGenWriteBRN31988Fragment(pui32Current);

#endif

	switch(eProgramType)
	{
		case USEGEN_CLEAR:
		{
			/* mov.skipinv.repeat8 o0, pa0 */
			BuildMOV ((PPVR_USE_INST)pui32Current,
					EURASIA_USE1_EPRED_ALWAYS,
					7 /* Repeat count */,
					EURASIA_USE1_RCNTSEL,
					USE_REGTYPE_OUTPUT, 0,
					USE_REGTYPE_PRIMATTR, 0,
					0 /* Src mod */);
			pui32Current += USE_INST_LENGTH;

			break;
		}
		case USEGEN_CLEAR_PACKEDCOL:
		{
			IMG_UINT32 ui32RedChannel, ui32BlueChannel;

#if defined(SGX_FEATURE_USE_VEC34)
			ui32RedChannel = 0;
			ui32BlueChannel = 2;
#else
			ui32RedChannel = 2;
			ui32BlueChannel = 0;
#endif

			/* mov.skipinv.repeat3 o0, pa0 */
			BuildMOV ((PPVR_USE_INST)pui32Current,
						EURASIA_USE1_EPRED_ALWAYS,
						2 /* Repeat count */,
						EURASIA_USE1_RCNTSEL,
						USE_REGTYPE_OUTPUT, 0,
						USE_REGTYPE_PRIMATTR, 0,
						0 /* Src mod */);
			pui32Current += USE_INST_LENGTH;

			/* mov.skipinv o3, c3  (c3 is the constant 1.0) */
			BuildMOV ((PPVR_USE_INST)pui32Current,
						EURASIA_USE1_EPRED_ALWAYS,
						0 /* Repeat count */,
						EURASIA_USE1_RCNTSEL,
						USE_REGTYPE_OUTPUT, 3,
						USE_REGTYPE_SPECIAL, EURASIA_USE_SPECIAL_CONSTANT_FLOAT1_1,
						0 /* Src mod */);

			pui32Current += USE_INST_LENGTH;
	
			/* unpckf32u8.skipinv.scale o4, pa3.ui32RedChannel, pa0.0 */
			BuildUNPCKF32((PPVR_USE_INST) pui32Current,
							EURASIA_USE1_EPRED_ALWAYS, 0,
							EURASIA_USE1_RCNTSEL,
							EURASIA_USE1_PCK_FMT_U8,
							EURASIA_USE0_PCK_SCALE,
							USE_REGTYPE_OUTPUT,
							4,
							USE_REGTYPE_PRIMATTR,
							3, 
							ui32RedChannel);

			pui32Current += USE_INST_LENGTH;

			/* unpckf32u8.skipinv.scale o5, pa3.1, pa0.0 */
			BuildUNPCKF32((PPVR_USE_INST) pui32Current,
							EURASIA_USE1_EPRED_ALWAYS, 0,
							EURASIA_USE1_RCNTSEL,
							EURASIA_USE1_PCK_FMT_U8,
							EURASIA_USE0_PCK_SCALE,
							USE_REGTYPE_OUTPUT,
							5,
							USE_REGTYPE_PRIMATTR,
							3, 
							1);

			pui32Current += USE_INST_LENGTH;

			/* unpckf32u8.skipinv.scale o6, pa3.ui32BlueChannel, pa0.0 */
			BuildUNPCKF32((PPVR_USE_INST) pui32Current,
							EURASIA_USE1_EPRED_ALWAYS, 0,
							EURASIA_USE1_RCNTSEL,
							EURASIA_USE1_PCK_FMT_U8,
							EURASIA_USE0_PCK_SCALE,
							USE_REGTYPE_OUTPUT,
							6,
							USE_REGTYPE_PRIMATTR,
							3, 
							ui32BlueChannel);

			pui32Current += USE_INST_LENGTH;
	
			/* unpckf32u8.skipinv.scale o7, pa3.3, pa0.0 */
			BuildUNPCKF32((PPVR_USE_INST) pui32Current,
							EURASIA_USE1_EPRED_ALWAYS, 0,
							EURASIA_USE1_RCNTSEL,
							EURASIA_USE1_PCK_FMT_U8,
							EURASIA_USE0_PCK_SCALE,
							USE_REGTYPE_OUTPUT,
							7,
							USE_REGTYPE_PRIMATTR,
							3, 
							3);

			pui32Current += USE_INST_LENGTH;
			break;
		}
		case USEGEN_ACCUM:
		{
			/* mov.skipinv.repeat3 o0, pa0 */
			BuildMOV ((PPVR_USE_INST)pui32Current,
						EURASIA_USE1_EPRED_ALWAYS,
						2 /* Repeat count */,
						EURASIA_USE1_RCNTSEL,
						USE_REGTYPE_OUTPUT, 0,
						USE_REGTYPE_PRIMATTR, 0,
						0 /* Src mod */);
			pui32Current += USE_INST_LENGTH;

			/* mov.skipinv o3, c3  (c3 is the constant 1.0) */
			BuildMOV ((PPVR_USE_INST)pui32Current,
						EURASIA_USE1_EPRED_ALWAYS,
						0 /* Repeat count */,
						EURASIA_USE1_RCNTSEL,
						USE_REGTYPE_OUTPUT, 3,
						USE_REGTYPE_SPECIAL, EURASIA_USE_SPECIAL_CONSTANT_FLOAT1_1,
						0 /* Src mod */);

			pui32Current += USE_INST_LENGTH;
			
			/* mov.skipinv.repeat2 o4, pa3  */
			BuildMOV ((PPVR_USE_INST)pui32Current,
						EURASIA_USE1_EPRED_ALWAYS,
						1 /* Repeat count */,
						EURASIA_USE1_RCNTSEL,
						USE_REGTYPE_OUTPUT, 4,
						USE_REGTYPE_PRIMATTR, 3,
						0 /* Src mod */);

			pui32Current += USE_INST_LENGTH;
			break;
		}
		case USEGEN_SCISSOR:
		{
			/* mov.skipinv.repeat2 o0, pa0 */
			BuildMOV ((PPVR_USE_INST)pui32Current,
					EURASIA_USE1_EPRED_ALWAYS,
					1 /* Repeat count */,
					EURASIA_USE1_RCNTSEL,
					USE_REGTYPE_OUTPUT, 0,
					USE_REGTYPE_PRIMATTR, 0,
					0 /* Src mod */);
			pui32Current += USE_INST_LENGTH;

			/* mov.skipinv o2, c3  (c3 is the constant 1.0) */
			BuildMOV ((PPVR_USE_INST)pui32Current,
					EURASIA_USE1_EPRED_ALWAYS,
					0 /* Repeat count */,
					EURASIA_USE1_RCNTSEL,
					USE_REGTYPE_OUTPUT, 2,
					USE_REGTYPE_SPECIAL, EURASIA_USE_SPECIAL_CONSTANT_FLOAT1_1,
					0 /* Src mod */);
			
			pui32Current += USE_INST_LENGTH;

			/* mov.skipinv o3, c3  (c3 is the constant 1.0) */
			BuildMOV ((PPVR_USE_INST)pui32Current,
					EURASIA_USE1_EPRED_ALWAYS,
					0 /* Repeat count */,
					EURASIA_USE1_RCNTSEL,
					USE_REGTYPE_OUTPUT, 3,
					USE_REGTYPE_SPECIAL, EURASIA_USE_SPECIAL_CONSTANT_FLOAT1_1,
					0 /* Src mod */);

			pui32Current += USE_INST_LENGTH;
			break;
		}
	}

#if defined(SGX_FEATURE_VCB)
	pui32Current =  USEGenWriteEndPreCullVtxShaderFragment (pui32Current);
	
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	/* Write the PHAS instruction */
	pui32Current = USEGenWritePhaseFragment (pui32Current, 0,
											 0, USEGEN_NO_DEPENDANT, IMG_FALSE,
											 IMG_FALSE, IMG_TRUE, IMG_FALSE);
#endif

#endif

	/*
	 * And finish the program
	 */
	pui32Current = USEGenWriteEndVtxShaderFragment (pui32Current);

	return pui32Current;
}

/***********************************************************************//**
* Write the clear pixel program
*
* @param	pui32Base			The linear address to write to
* @return	Pointer to the end of the written block
*
**************************************************************************/
#if defined(FIX_HW_BRN_29838)
IMG_INTERNAL IMG_PUINT32 USEGenWriteClearPixelProgramF16(IMG_PUINT32 pui32Base)
{
	IMG_PUINT32 pui32Current = pui32Base;

	USEGEN_ASSERT (pui32Current);

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	/* This is the only phase */
	pui32Current = USEGenWritePhaseFragment (pui32Current, 0,
											 0, USEGEN_NO_DEPENDANT, IMG_FALSE,
											 IMG_FALSE, IMG_TRUE, IMG_FALSE);
#endif

	/* PA0 PA1 (F16) must be packed into O0 (U8) 
	   pcku8f16.skipinv.scale o0.bytemask0110, pa0.2, pa0.0
	   pcku8f16.skipinv.end.scale o0.bytemask1001, pa1.0, pa1.2
	 */

	BuildPCKUNPCK((PPVR_USE_INST)pui32Current,
				   EURASIA_USE1_EPRED_ALWAYS, 
				   0,
				   EURASIA_USE1_RCNTSEL,
				   EURASIA_USE1_PCK_FMT_U8,
				   EURASIA_USE1_PCK_FMT_F16,
				   EURASIA_USE0_PCK_SCALE,
				   USE_REGTYPE_OUTPUT,
				   0,
				   0x6,
				   USE_REGTYPE_PRIMATTR,
				   0,
				   2,
				   USE_REGTYPE_PRIMATTR,
				   0,
				   0);

	pui32Current+=USE_INST_LENGTH;

	BuildPCKUNPCK((PPVR_USE_INST)pui32Current,
				   EURASIA_USE1_EPRED_ALWAYS, 
				   0,
				   EURASIA_USE1_END | EURASIA_USE1_RCNTSEL,
				   EURASIA_USE1_PCK_FMT_U8,
				   EURASIA_USE1_PCK_FMT_F16,
				   EURASIA_USE0_PCK_SCALE,
				   USE_REGTYPE_OUTPUT,
				   0,
				   0x9,
				   USE_REGTYPE_PRIMATTR,
				   1,
				   0,
				   USE_REGTYPE_PRIMATTR,
				   1,
				   2);

	pui32Current+=USE_INST_LENGTH;

	return pui32Current;
}
#endif

IMG_INTERNAL IMG_PUINT32 USEGenWriteClearPixelProgram (IMG_PUINT32 pui32Base, IMG_BOOL bUseSecondary)
{
	IMG_PUINT32 pui32Current = pui32Base;

	USEGEN_ASSERT (pui32Current);

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	/* This is the only phase */
	pui32Current = USEGenWritePhaseFragment (pui32Current, 0,
											 0, USEGEN_NO_DEPENDANT, IMG_FALSE,
											 IMG_FALSE, IMG_TRUE, IMG_FALSE);
#endif

	/*
	 * Simple MOV from sa0/pa0 to o0 should do the trick here,
	 * ensuring the END flag is set.
	 */
	BuildMOV ((PPVR_USE_INST)pui32Current,
			  EURASIA_USE1_EPRED_ALWAYS,
			  0 /* Repeat count */,
			  EURASIA_USE1_END | EURASIA_USE1_RCNTSEL,
			  USE_REGTYPE_OUTPUT, 0,
			  bUseSecondary ? USE_REGTYPE_SECATTR : USE_REGTYPE_PRIMATTR, 0,
			  0 /* Src mod */);

	pui32Current+=USE_INST_LENGTH;

	return pui32Current;
}

#if defined(USEGEN_BUILD_D3DVISTA) || defined(USEGEN_BUILD_OPENGL)

#if defined(SGX_FEATURE_VCB)
/***********************************************************************//**
* Calculates the size of the Pre-VCB-cull Vertex DMA program
*
* @param ui32VtxSize			The size (in UINT32s) of the vertex
* @return Size (in bytes) required for this program
**************************************************************************/
IMG_UINT32 USEGenCalculateVtxDMAPreCullSize (IMG_UINT32 ui32VtxSize)
{
	IMG_UINT32 ui32Size;

	/* Sanity check */
	USEGEN_ASSERT (ui32VtxSize > 0);

	/* The Vertex DMA program is a state emit, but with a specialised
	 * emit */
	ui32Size = USEGenCalculateStateSize (ui32VtxSize);

	/* The extra 2 here is for the specialised emit we have to do and
	 * the (potential) viewport array index move */
	ui32Size += USE_INST_TO_BYTES(USEGEN_PREAMBLE_COUNT + 2);

	return ui32Size;

}
#endif



/******************************************************************************
 * Fragment Functions
 ******************************************************************************/

/***********************************************************************//**
* Write the final instruction required when the end flag cannot be used
*
* @param	pui32Base			The linear address to write to
* @return	Pointer to the end of the written block
**************************************************************************/
IMG_PUINT32 USEGenWriteDummyEndFragment (IMG_PUINT32 pui32Base)
{
	IMG_PUINT32 pui32Current = pui32Base;

	/* Simple checking */
	USEGEN_ASSERT (pui32Current);

	BuildNOP((PPVR_USE_INST)pui32Current, EURASIA_USE1_END, IMG_FALSE);
	pui32Current += USE_INST_LENGTH;

	return pui32Current;
}

/***********************************************************************//**
* Sets the end flag on the current instruction if possible. If not inserts
* a NOP.end
*
* @param pui32Base			The base (linear) address to write the program to
* @return Pointer to the end of the written program
**************************************************************************/
IMG_INTERNAL IMG_PUINT32 USEGenSetUSEEndFlag (IMG_PUINT32 pui32Base)
{
	#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	#define PVRDX_PHASMASK ((EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) | \
							(EURASIA_USE1_OTHER2_OP2_PHAS << EURASIA_USE1_OTHER2_OP2_SHIFT) | \
							EURASIA_USE1_SPECIAL_OPCAT_EXTRA | \
							(EURASIA_USE1_SPECIAL_OPCAT_OTHER2 << EURASIA_USE1_SPECIAL_OPCAT_SHIFT))
	
	/*
	 * Phas instructions are special, so check for them first..
	 */
	if((pui32Base[1] & PVRDX_PHASMASK) == PVRDX_PHASMASK)
	{
		pui32Base[1]  |= EURASIA_USE1_OTHER2_PHAS_END;
	}
	else
	#endif
	{
		IMG_UINT32 ui32OpCode = (pui32Base[1] & ~EURASIA_USE1_OP_CLRMSK) >> EURASIA_USE1_OP_SHIFT;
		IMG_UINT32 ui32OtherOpCode = (pui32Base[1] & ~EURASIA_USE1_OTHER_OP2_CLRMSK) >> EURASIA_USE1_OTHER_OP2_SHIFT;
		
		/*
		 * Check for instructions that can't have the END flag applied and
		 * append a nop.end instead.
		 */
		if(
			(ui32OpCode == EURASIA_USE1_OP_TESTMASK) ||
			(ui32OpCode == EURASIA_USE1_OP_TEST) ||
			(ui32OpCode == EURASIA_USE1_OP_PCKUNPCK) ||
			(ui32OpCode == EURASIA_USE1_OP_EFO) ||
			(ui32OpCode == EURASIA_USE1_OP_FARITH16) ||
			(ui32OpCode == EURASIA_USE1_OP_SMP) ||
			(ui32OpCode == EURASIA_USE1_OP_LD) ||
			(ui32OpCode == EURASIA_USE1_OP_ST) ||
			(
				(ui32OpCode == EURASIA_USE1_OP_SPECIAL) &&
				(
					(ui32OtherOpCode != EURASIA_USE1_OTHER_OP2_LIMM)
				)
			)
		  )
		{
			return USEGenWriteDummyEndFragment(pui32Base+USE_INST_LENGTH);
		}
		/*
		 * For everything else just set the normal flag..
		 */
		else
		{
			pui32Base[1]  |= EURASIA_USE1_END;
		}
	}
	
	return pui32Base + USE_INST_LENGTH;
}

/***********************************************************************//**
* Writes the final emit for a geometry shader program
* @param pui32Base			The base (linear) address to write to
* @param ui32NumPartitions	Number of partitions used by the shader
* @param bDoublePartition	Whether we're in double partition mode
* @return Pointer to the end of the written program
**************************************************************************/
IMG_PUINT32 USEGenWriteEndGeomShaderFragment (IMG_PUINT32 pui32Base,
											  IMG_UINT32 ui32NumPartitions,
											  IMG_BOOL bDoublePartition)
{
#if defined(SGX545)
	IMG_PUINT32 pui32Current = pui32Base;
	IMG_UINT32 i;
	IMG_UINT32 ui32End = 0;
	IMG_UINT32 ui32Partitions = ui32NumPartitions;

	USEGEN_ASSERT (pui32Current);

	BuildEMITMTE ((PPVR_USE_INST) pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS,
				  SGX545_USE1_EMIT_MTE_CUT |
				  (SGX545_USE1_EMIT_VCB_EMITTYPE_VERTEX <<
				   SGX545_USE1_EMIT_VCB_EMITTYPE_SHIFT) |
				  (bDoublePartition ? SGX545_USE1_EMIT_VERTEX_TWOPART : 0),
				  USE_REGTYPE_IMMEDIATE,
				  0,
				  USE_REGTYPE_IMMEDIATE,
				  1,
				  USE_REGTYPE_IMMEDIATE,
				  0,
				  0,
				  IMG_FALSE);
	pui32Current += USE_INST_LENGTH;

	if (bDoublePartition)
	{
		ui32Partitions = ui32NumPartitions >> 1;
	}

	/* Loop over all partitions and WOP for them to ensure we've got the
	 * current partition available to emit
	 */
	for (i = 0; i < ui32Partitions; i++)
	{
		BuildWOP ((PPVR_USE_INST) pui32Current,
				  0, (bDoublePartition ? SGX545_USE0_WOP_TWOPART : 0));
		pui32Current += USE_INST_LENGTH;

		if (i == (ui32Partitions-1))
		{
			ui32End = EURASIA_USE1_END;
		}

		/* This should just be an emitmtevtx.end.freep */
		BuildEMITMTE ((PPVR_USE_INST) pui32Current,
					  EURASIA_USE1_EPRED_ALWAYS,
					  ui32End |
					  SGX545_USE1_EMIT_MTE_CUT |
					  (SGX545_USE1_EMIT_VCB_EMITTYPE_VERTEX <<
					   SGX545_USE1_EMIT_VCB_EMITTYPE_SHIFT) |
					  (bDoublePartition ? SGX545_USE1_EMIT_VERTEX_TWOPART : 0),
					  USE_REGTYPE_IMMEDIATE,
					  0,
					  USE_REGTYPE_IMMEDIATE,
					  1,
					  USE_REGTYPE_IMMEDIATE,
					  0,
					  0,
					  IMG_TRUE);
		pui32Current += USE_INST_LENGTH;
	}

	return pui32Current;
#else
	PVR_UNREFERENCED_PARAMETER(ui32NumPartitions);
	PVR_UNREFERENCED_PARAMETER(bDoublePartition);

	/* Other chips aren't supported as yet */
	return pui32Base;
#endif
}

#if !defined(SGX543) && !defined(SGX544) && !defined(SGX554)
static IMG_VOID BuildStateWords(PUSEGEN_ISP_FEEDBACK_PROG psProg,
								IMG_UINT32 *ui32SW0,
								IMG_UINT32 *ui32SW1)
{
	/**
	 * todo: place this as an SA or an immediate
	*/
	*ui32SW0 = 0;
	
	/* Set up the front face stencil state */
	if (psProg->bStencilFFEnable)
	{
		switch(psProg->eStencilFFCmp)
		{
		case IMG_COMPFUNC_NEVER:
			*ui32SW0 |=  EURASIA_USE_VISTEST_STATE0_SCMPMODE_NEVER <<
				EURASIA_USE_VISTEST_STATE0_SCMPMODE_SHIFT;
			break;
		case IMG_COMPFUNC_NOT_EQUAL:
			*ui32SW0 |=  EURASIA_USE_VISTEST_STATE0_SCMPMODE_NE <<
				EURASIA_USE_VISTEST_STATE0_SCMPMODE_SHIFT;
			break;
		case IMG_COMPFUNC_LESS:
			*ui32SW0 |=  EURASIA_USE_VISTEST_STATE0_SCMPMODE_LT <<
				EURASIA_USE_VISTEST_STATE0_SCMPMODE_SHIFT;
			break;
		case IMG_COMPFUNC_LESS_EQUAL:
			*ui32SW0 |=  EURASIA_USE_VISTEST_STATE0_SCMPMODE_LE <<
				EURASIA_USE_VISTEST_STATE0_SCMPMODE_SHIFT;
			break;
		case IMG_COMPFUNC_EQUAL:
			*ui32SW0 |=  EURASIA_USE_VISTEST_STATE0_SCMPMODE_EQ <<
				EURASIA_USE_VISTEST_STATE0_SCMPMODE_SHIFT;
			break;
		case IMG_COMPFUNC_GREATER:
			*ui32SW0 |=  EURASIA_USE_VISTEST_STATE0_SCMPMODE_GT <<
				EURASIA_USE_VISTEST_STATE0_SCMPMODE_SHIFT;
			break;
		case IMG_COMPFUNC_GREATER_EQUAL:
			*ui32SW0 |=  EURASIA_USE_VISTEST_STATE0_SCMPMODE_GE <<
				EURASIA_USE_VISTEST_STATE0_SCMPMODE_SHIFT;
			break;
		case IMG_COMPFUNC_ALWAYS:
			*ui32SW0 |=  EURASIA_USE_VISTEST_STATE0_SCMPMODE_ALWAYS <<
				EURASIA_USE_VISTEST_STATE0_SCMPMODE_SHIFT;
			break;
		default:
			USEGEN_DPF("UseGenWriteISPFeedbackFragment: Invalid COMPFUNC for psProg->eStencilFFCmp.");
			USEGEN_DBG_BREAK;
			break;
		}
	}
	else
	{
		*ui32SW0 |=  EURASIA_USE_VISTEST_STATE0_SCMPMODE_ALWAYS <<
			EURASIA_USE_VISTEST_STATE0_SCMPMODE_SHIFT;
	}

	/* Pass is defined as SOP3 */
	if (psProg->bStencilFFEnable)
	{
		switch (psProg->eStPassFFOp)
		{
		case IMG_STENCILOP_KEEP:
			*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_KEEP <<
				EURASIA_USE_VISTEST_STATE0_SOP3_SHIFT;
			break;
		case IMG_STENCILOP_ZERO:
			*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_ZERO <<
				EURASIA_USE_VISTEST_STATE0_SOP3_SHIFT;
			break;
		case IMG_STENCILOP_REPLACE:
			*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_REPLACE <<
				EURASIA_USE_VISTEST_STATE0_SOP3_SHIFT;
			break;
		case IMG_STENCILOP_INCR_SAT:
			*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_INCRSAT <<
				EURASIA_USE_VISTEST_STATE0_SOP3_SHIFT;
			break;
		case IMG_STENCILOP_DECR_SAT:
			*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_DECRSAT <<
				EURASIA_USE_VISTEST_STATE0_SOP3_SHIFT;
			break;
		case IMG_STENCILOP_INVERT:
			*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_INVERT <<
				EURASIA_USE_VISTEST_STATE0_SOP3_SHIFT;
			break;
		case IMG_STENCILOP_INCR:
			*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_INCR <<
				EURASIA_USE_VISTEST_STATE0_SOP3_SHIFT;
			break;
		case IMG_STENCILOP_DECR:
			*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_DECR <<
				EURASIA_USE_VISTEST_STATE0_SOP3_SHIFT;
			break;
		default:
			USEGEN_DPF("UseGenWriteISPFeedbackFragment: Invalid Stencil op for psProg->eStPassFFOp.");
			USEGEN_DBG_BREAK;
			break;
		}
	}
	else
	{
		*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_KEEP <<
			EURASIA_USE_VISTEST_STATE0_SOP3_SHIFT;
	}

	/* Fail is defined as SOP2 */
	if (psProg->bStencilFFEnable)
	{
		switch (psProg->eStZFailFFOp)
		{
		case IMG_STENCILOP_KEEP:
			*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_KEEP <<
				EURASIA_USE_VISTEST_STATE0_SOP2_SHIFT;
			break;
		case IMG_STENCILOP_ZERO:
			*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_ZERO <<
				EURASIA_USE_VISTEST_STATE0_SOP2_SHIFT;
			break;
		case IMG_STENCILOP_REPLACE:
			*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_REPLACE <<
				EURASIA_USE_VISTEST_STATE0_SOP2_SHIFT;
			break;
		case IMG_STENCILOP_INCR_SAT:
			*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_INCRSAT <<
				EURASIA_USE_VISTEST_STATE0_SOP2_SHIFT;
			break;
		case IMG_STENCILOP_DECR_SAT:
			*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_DECRSAT <<
				EURASIA_USE_VISTEST_STATE0_SOP2_SHIFT;
			break;
		case IMG_STENCILOP_INVERT:
			*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_INVERT <<
				EURASIA_USE_VISTEST_STATE0_SOP2_SHIFT;
			break;
		case IMG_STENCILOP_INCR:
			*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_INCR <<
				EURASIA_USE_VISTEST_STATE0_SOP2_SHIFT;
			break;
		case IMG_STENCILOP_DECR:
			*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_DECR <<
				EURASIA_USE_VISTEST_STATE0_SOP2_SHIFT;
			break;
		default:
			USEGEN_DPF("UseGenWriteISPFeedbackFragment: Invalid Stencil op for psProg->eStZFailFFOp.");
			USEGEN_DBG_BREAK;
			break;
		}
	}
	else
	{
		*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_KEEP <<
			EURASIA_USE_VISTEST_STATE0_SOP2_SHIFT;
	}

	/* ZFail is defined as SOP1 */
	if (psProg->bStencilFFEnable)
	{
		switch (psProg->eStFailFFOp)
		{
		case IMG_STENCILOP_KEEP:
			*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_KEEP <<
				EURASIA_USE_VISTEST_STATE0_SOP1_SHIFT;
			break;
		case IMG_STENCILOP_ZERO:
			*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_ZERO <<
				EURASIA_USE_VISTEST_STATE0_SOP1_SHIFT;
			break;
		case IMG_STENCILOP_REPLACE:
			*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_REPLACE <<
				EURASIA_USE_VISTEST_STATE0_SOP1_SHIFT;
			break;
		case IMG_STENCILOP_INCR_SAT:
			*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_INCRSAT <<
				EURASIA_USE_VISTEST_STATE0_SOP1_SHIFT;
			break;
		case IMG_STENCILOP_DECR_SAT:
			*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_DECRSAT <<
				EURASIA_USE_VISTEST_STATE0_SOP1_SHIFT;
			break;
		case IMG_STENCILOP_INVERT:
			*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_INVERT <<
				EURASIA_USE_VISTEST_STATE0_SOP1_SHIFT;
			break;
		case IMG_STENCILOP_INCR:
			*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_INCR <<
				EURASIA_USE_VISTEST_STATE0_SOP1_SHIFT;
			break;
		case IMG_STENCILOP_DECR:
			*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_DECR <<
				EURASIA_USE_VISTEST_STATE0_SOP1_SHIFT;
			break;
		default:
			USEGEN_DPF("UseGenWriteISPFeedbackFragment: Invalid Stencil op for psProg->eStFailFFOp.");
			USEGEN_DBG_BREAK;
			break;
		}
	}
	else
	{
		*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_SOP_KEEP <<
			EURASIA_USE_VISTEST_STATE0_SOP1_SHIFT;
	}

	/* Stencil reference value and read mask */
	*ui32SW0 |= psProg->ui8StencilRef << EURASIA_USE_VISTEST_STATE0_SREF_SHIFT;
	*ui32SW0 |= psProg->ui8StencilReadMask <<
		EURASIA_USE_VISTEST_STATE0_SMASK_SHIFT;

	if (psProg->bDepthEnable)
	{
		switch (psProg->eDepthCmp)
		{
		case IMG_COMPFUNC_NEVER:
			*ui32SW0 |=  EURASIA_USE_VISTEST_STATE0_DCMPMODE_NEVER <<
								EURASIA_USE_VISTEST_STATE0_DCMPMODE_SHIFT;
			break;
		case IMG_COMPFUNC_NOT_EQUAL:
			*ui32SW0 |=  EURASIA_USE_VISTEST_STATE0_DCMPMODE_NE <<
				EURASIA_USE_VISTEST_STATE0_DCMPMODE_SHIFT;
			break;
		case IMG_COMPFUNC_LESS:
			*ui32SW0 |=  EURASIA_USE_VISTEST_STATE0_DCMPMODE_LT <<
				EURASIA_USE_VISTEST_STATE0_DCMPMODE_SHIFT;
			break;
		case IMG_COMPFUNC_LESS_EQUAL:
			*ui32SW0 |=  EURASIA_USE_VISTEST_STATE0_DCMPMODE_LE <<
				EURASIA_USE_VISTEST_STATE0_DCMPMODE_SHIFT;
			break;
		case IMG_COMPFUNC_EQUAL:
			*ui32SW0 |=  EURASIA_USE_VISTEST_STATE0_DCMPMODE_EQ <<
				EURASIA_USE_VISTEST_STATE0_DCMPMODE_SHIFT;
			break;
		case IMG_COMPFUNC_GREATER:
			*ui32SW0 |=  EURASIA_USE_VISTEST_STATE0_DCMPMODE_GT <<
				EURASIA_USE_VISTEST_STATE0_DCMPMODE_SHIFT;
			break;
		case IMG_COMPFUNC_GREATER_EQUAL:
			*ui32SW0 |=  EURASIA_USE_VISTEST_STATE0_DCMPMODE_GE <<
				EURASIA_USE_VISTEST_STATE0_DCMPMODE_SHIFT;
			break;
		case IMG_COMPFUNC_ALWAYS:
			*ui32SW0 |=  EURASIA_USE_VISTEST_STATE0_DCMPMODE_ALWAYS <<
				EURASIA_USE_VISTEST_STATE0_DCMPMODE_SHIFT;
			break;
		default:
			USEGEN_DPF("UseGenWriteISPFeedbackFragment: Invalid COMPFUNC for psProg->eDepthCmp.");
			USEGEN_DBG_BREAK;
			break;
		}
	}
	else
	{
		*ui32SW0 |= (EURASIA_USE_VISTEST_STATE0_DCMPMODE_ALWAYS <<
					EURASIA_USE_VISTEST_STATE0_DCMPMODE_SHIFT);
	}

	if (!psProg->bDepthEnable || psProg->bDWriteDis)
	{
		*ui32SW0 |= EURASIA_USE_VISTEST_STATE0_DWDISABLE;
	}


 	/*
		p1 mov r<TempReg+1>, <state 1>
		this is the alpha test state high word.
		todo: place this as an SA or an immediate for atst
	*/
	*ui32SW1 = 0;

	/* Set up the back face stencil state */
	if (psProg->bStencilBFEnable)
	{
		switch(psProg->eStencilBFCmp)
		{
		case IMG_COMPFUNC_NEVER:
			*ui32SW1 |=  EURASIA_USE_VISTEST_STATE0_SCMPMODE_NEVER <<
				EURASIA_USE_VISTEST_STATE1_CCWSCMP_SHIFT;
			break;
		case IMG_COMPFUNC_NOT_EQUAL:
			*ui32SW1 |=  EURASIA_USE_VISTEST_STATE0_SCMPMODE_NE <<
				EURASIA_USE_VISTEST_STATE1_CCWSCMP_SHIFT;
			break;
		case IMG_COMPFUNC_LESS:
			*ui32SW1 |=  EURASIA_USE_VISTEST_STATE0_SCMPMODE_LT <<
				EURASIA_USE_VISTEST_STATE1_CCWSCMP_SHIFT;
			break;
		case IMG_COMPFUNC_LESS_EQUAL:
			*ui32SW1 |=  EURASIA_USE_VISTEST_STATE0_SCMPMODE_LE <<
				EURASIA_USE_VISTEST_STATE1_CCWSCMP_SHIFT;
			break;
		case IMG_COMPFUNC_EQUAL:
			*ui32SW1 |=  EURASIA_USE_VISTEST_STATE0_SCMPMODE_EQ <<
				EURASIA_USE_VISTEST_STATE1_CCWSCMP_SHIFT;
			break;
		case IMG_COMPFUNC_GREATER:
			*ui32SW1 |=  EURASIA_USE_VISTEST_STATE0_SCMPMODE_GT <<
				EURASIA_USE_VISTEST_STATE1_CCWSCMP_SHIFT;
			break;
		case IMG_COMPFUNC_GREATER_EQUAL:
			*ui32SW1 |=  EURASIA_USE_VISTEST_STATE0_SCMPMODE_GE <<
				EURASIA_USE_VISTEST_STATE1_CCWSCMP_SHIFT;
			break;
		case IMG_COMPFUNC_ALWAYS:
			*ui32SW1 |=  EURASIA_USE_VISTEST_STATE0_SCMPMODE_ALWAYS <<
				EURASIA_USE_VISTEST_STATE1_CCWSCMP_SHIFT;
			break;
		default:
			USEGEN_DPF("UseGenWriteISPFeedbackFragment: Invalid COMPFUNC for psProg->eStencilBFCmp.");
			USEGEN_DBG_BREAK;
			break;
		}
	}
	else
	{
		*ui32SW1 |=  EURASIA_USE_VISTEST_STATE0_SCMPMODE_ALWAYS <<
			EURASIA_USE_VISTEST_STATE1_CCWSCMP_SHIFT;
	}

	/* Pass is defined as SOP3 */
	if (psProg->bStencilBFEnable)
	{
		switch (psProg->eStPassBFOp)
		{
		case IMG_STENCILOP_KEEP:
			*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_KEEP <<
				EURASIA_USE_VISTEST_STATE1_CCWSOP3_SHIFT;
			break;
		case IMG_STENCILOP_ZERO:
			*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_ZERO <<
				EURASIA_USE_VISTEST_STATE1_CCWSOP3_SHIFT;
			break;
		case IMG_STENCILOP_REPLACE:
			*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_REPLACE <<
				EURASIA_USE_VISTEST_STATE1_CCWSOP3_SHIFT;
			break;
		case IMG_STENCILOP_INCR_SAT:
			*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_INCRSAT <<
				EURASIA_USE_VISTEST_STATE1_CCWSOP3_SHIFT;
			break;
		case IMG_STENCILOP_DECR_SAT:
			*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_DECRSAT <<
				EURASIA_USE_VISTEST_STATE1_CCWSOP3_SHIFT;
			break;
		case IMG_STENCILOP_INVERT:
			*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_INVERT <<
				EURASIA_USE_VISTEST_STATE1_CCWSOP3_SHIFT;
			break;
		case IMG_STENCILOP_INCR:
			*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_INCR <<
				EURASIA_USE_VISTEST_STATE1_CCWSOP3_SHIFT;
			break;
		case IMG_STENCILOP_DECR:
			*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_DECR <<
				EURASIA_USE_VISTEST_STATE1_CCWSOP3_SHIFT;
			break;
		default:
			USEGEN_DPF("UseGenWriteISPFeedbackFragment: Invalid Stencil op for psProg->eStPassBFOp.");
			USEGEN_DBG_BREAK;
			break;
		}
	}
	else
	{
		*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_KEEP <<
			EURASIA_USE_VISTEST_STATE1_CCWSOP3_SHIFT;
	}

	/* Fail is defined as SOP2 */
	if (psProg->bStencilBFEnable)
	{
		switch (psProg->eStZFailBFOp)
		{
		case IMG_STENCILOP_KEEP:
			*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_KEEP <<
				EURASIA_USE_VISTEST_STATE1_CCWSOP2_SHIFT;
			break;
		case IMG_STENCILOP_ZERO:
			*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_ZERO <<
				EURASIA_USE_VISTEST_STATE1_CCWSOP2_SHIFT;
			break;
		case IMG_STENCILOP_REPLACE:
			*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_REPLACE <<
				EURASIA_USE_VISTEST_STATE1_CCWSOP2_SHIFT;
			break;
		case IMG_STENCILOP_INCR_SAT:
			*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_INCRSAT <<
				EURASIA_USE_VISTEST_STATE1_CCWSOP2_SHIFT;
			break;
		case IMG_STENCILOP_DECR_SAT:
			*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_DECRSAT <<
				EURASIA_USE_VISTEST_STATE1_CCWSOP2_SHIFT;
			break;
		case IMG_STENCILOP_INVERT:
			*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_INVERT <<
				EURASIA_USE_VISTEST_STATE1_CCWSOP2_SHIFT;
			break;
		case IMG_STENCILOP_INCR:
			*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_INCR <<
				EURASIA_USE_VISTEST_STATE1_CCWSOP2_SHIFT;
			break;
		case IMG_STENCILOP_DECR:
			*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_DECR <<
				EURASIA_USE_VISTEST_STATE1_CCWSOP2_SHIFT;
			break;
		default:
			USEGEN_DPF("UseGenWriteISPFeedbackFragment: Invalid Stencil op for psProg->eStZFailBFOp.");
			USEGEN_DBG_BREAK;
			break;
		}
	}
	else
	{
		*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_KEEP <<
			EURASIA_USE_VISTEST_STATE1_CCWSOP2_SHIFT;
	}

	/* ZFail is defined as SOP1 */
	if (psProg->bStencilBFEnable)
	{
		switch (psProg->eStFailBFOp)
		{
		case IMG_STENCILOP_KEEP:
			*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_KEEP <<
				EURASIA_USE_VISTEST_STATE1_CCWSOP1_SHIFT;
			break;
		case IMG_STENCILOP_ZERO:
			*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_ZERO <<
				EURASIA_USE_VISTEST_STATE1_CCWSOP1_SHIFT;
			break;
		case IMG_STENCILOP_REPLACE:
			*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_REPLACE <<
				EURASIA_USE_VISTEST_STATE1_CCWSOP1_SHIFT;
			break;
		case IMG_STENCILOP_INCR_SAT:
			*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_INCRSAT <<
				EURASIA_USE_VISTEST_STATE1_CCWSOP1_SHIFT;
			break;
		case IMG_STENCILOP_DECR_SAT:
			*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_DECRSAT <<
				EURASIA_USE_VISTEST_STATE1_CCWSOP1_SHIFT;
			break;
		case IMG_STENCILOP_INVERT:
			*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_INVERT <<
				EURASIA_USE_VISTEST_STATE1_CCWSOP1_SHIFT;
			break;
		case IMG_STENCILOP_INCR:
			*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_INCR <<
				EURASIA_USE_VISTEST_STATE1_CCWSOP1_SHIFT;
			break;
		case IMG_STENCILOP_DECR:
			*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_DECR <<
				EURASIA_USE_VISTEST_STATE1_CCWSOP1_SHIFT;
			break;
		default:
			USEGEN_DPF("UseGenWriteISPFeedbackFragment: Invalid Stencil op for psProg->eStFailBFOp.");
			USEGEN_DBG_BREAK;
			break;
		}
	}
	else
	{
		*ui32SW1 |= EURASIA_USE_VISTEST_STATE0_SOP_KEEP <<
			EURASIA_USE_VISTEST_STATE1_CCWSOP1_SHIFT;
	}

	/* Stencil write mask */
	*ui32SW1 |= psProg->ui8StencilWriteMask <<
		EURASIA_USE_VISTEST_STATE1_SWMASK_SHIFT;

	if (psProg->bAlphaTest || psProg->bAlphaToCoverage)
	{
		if (psProg->bAlphaTest)
		{
			*ui32SW1 |=
				(psProg->ui8AlphaRef << EURASIA_USE_VISTEST_STATE1_AREF_SHIFT);
		}

		switch (psProg->eAlphaCmp)
		{
		case IMG_COMPFUNC_NEVER:
			*ui32SW1 |=  EURASIA_USE_VISTEST_STATE1_ACMPMODE_NEVER <<
				EURASIA_USE_VISTEST_STATE1_ACMPMODE_SHIFT;
			break;
		case IMG_COMPFUNC_NOT_EQUAL:
			*ui32SW1 |=  EURASIA_USE_VISTEST_STATE1_ACMPMODE_NE <<
				EURASIA_USE_VISTEST_STATE1_ACMPMODE_SHIFT;
			break;
		case IMG_COMPFUNC_LESS:
			*ui32SW1 |=  EURASIA_USE_VISTEST_STATE1_ACMPMODE_LT <<
				EURASIA_USE_VISTEST_STATE1_ACMPMODE_SHIFT;
			break;
		case IMG_COMPFUNC_LESS_EQUAL:
			*ui32SW1 |=  EURASIA_USE_VISTEST_STATE1_ACMPMODE_LE <<
				EURASIA_USE_VISTEST_STATE1_ACMPMODE_SHIFT;
			break;
		case IMG_COMPFUNC_EQUAL:
			*ui32SW1 |=  EURASIA_USE_VISTEST_STATE1_ACMPMODE_EQ <<
				EURASIA_USE_VISTEST_STATE1_ACMPMODE_SHIFT;
			break;
		case IMG_COMPFUNC_GREATER:
			*ui32SW1 |=  EURASIA_USE_VISTEST_STATE1_ACMPMODE_GT <<
				EURASIA_USE_VISTEST_STATE1_ACMPMODE_SHIFT;
			break;
		case IMG_COMPFUNC_GREATER_EQUAL:
			*ui32SW1 |=  EURASIA_USE_VISTEST_STATE1_ACMPMODE_GE <<
				EURASIA_USE_VISTEST_STATE1_ACMPMODE_SHIFT;
			break;
		case IMG_COMPFUNC_ALWAYS:
			*ui32SW1 |=  EURASIA_USE_VISTEST_STATE1_ACMPMODE_ALWAYS <<
				EURASIA_USE_VISTEST_STATE1_ACMPMODE_SHIFT;
			break;
		default:
			USEGEN_DPF("UseGenWriteISPFeedbackFragment: Invalid COMPFUNC for psProg->eAlphaCmp.");
			USEGEN_DBG_BREAK;
			break;
		}
	}
	else
	{
		*ui32SW1 |= (EURASIA_USE_VISTEST_STATE1_ACMPMODE_ALWAYS <<
					EURASIA_USE_VISTEST_STATE1_ACMPMODE_SHIFT);
	}
}
#endif /*!defined(SGX543) && !defined(SGX544) && !defined(SGX554)*/

/***********************************************************************//**
 * Generate the any feedback instructions for the end of the pixel shader
 * USE phase.
 *
 * Possible operations are:
 * Depth Writing (output of depth from pixel shader)
 * Texkill (kill pixel based on a predicate register)
 * Alpha testing (kill pixel based on the output from pixel shader)
 *
 * @param	pui32BufferBase		The linear address of the start of the pixel
 *								USE program
 * @param	psProg				The program parameters
 *
 * When doing a depth write, the depth value is assumed to be in 
 * psProg->ui32TempBase
 *
 * There are 8 possible states here:
 * No testing at all:
 * Return without emitting.
 *
 * Depth Writing only:
 * Perform a DEPTHF instruction only.
 *
 * Texkill only:
 * Perform a PCOEFF and depending on the predicate passed in as 
 * psProg->ui32PredReg also perform a ATST8.
 *
 * Alpha Test only:
 * Perform a PCOEFF and ATST8.
 * 
 * Texkill and Depth Writing together:
 * Perform a PCOEFF and a predicated ATST8 and DEPTHF. ATST8 is non-feedback 
 * and writes into the texkill predicate register.
 *
 * Depth Writing and Alpha Testing - 
 * Perform an ATST8 with no feedback, then a DEPTHF predicated with the
 * result of the ATST8.
 *
 * Texkill and Alpha Testing - 
 * Perform a PCOEFF and a predicated ATST8.
 *
 * Depth Writing and Texkill and Alpha Testing - 
 * Perform a PCOEFF, and a predicated non-feedback ATST8 which writes into the
 * texkill predicate and a predicated DEPTHF.
 *
 * The statewords for the ATST8 and DEPTHF are written into temporaries 
 * starting either at r0 if there is no DEPTHF or r1 if there is (The Uniflex
 * specification states that a depth calculation result will be placed into r0).
 *
 * A SMLSI is performed prior to a PCOEFF to setup the PCOEFF iteration for the 
 * Src1 parameter.
 *
 * @note This function will end the USE phase.
 *
 * @return Pointer to the end of the written block
**************************************************************************/
IMG_PUINT32 USEGenWriteISPFeedbackFragment(IMG_PUINT32 				 pui32BufferBase,
										   PUSEGEN_ISP_FEEDBACK_PROG psProg)
{
	IMG_PUINT32 pui32Buffer;
#if !defined(SGX543) && !defined(SGX544) && !defined(SGX554)
	IMG_UINT32 ui32SW0;
	IMG_UINT32 ui32SW1;
#endif
	IMG_UINT32 ui32Pred;
	IMG_UINT32 ui32TempReg = psProg->ui32TempBase;
	USE_REGTYPE	eMaskBank = USE_REGTYPE_PRIMATTR;
	IMG_UINT32	ui32MaskReg = 0;
	IMG_BOOL bATST = IMG_FALSE;

	USEGEN_ASSERT(pui32BufferBase);
	//make sure the number can fit in the field.
	USEGEN_ASSERT(!((psProg->ui32StartPA + 2) & EURASIA_USE0_SRC2_CLRMSK));

	pui32Buffer = pui32BufferBase;

	/* initialize the max temp used output parameter */
	psProg->ui32MaxTempUsed = psProg->ui32TempBase;

	/* All of these require an ATST8 to be performed */
	if (psProg->bAlphaTest ||
		psProg->bTexkillEnable ||
		psProg->bAlphaToCoverage ||
		psProg->bSampleMask ||
		psProg->bOMask)
	{
		bATST = IMG_TRUE;
	}

	if (psProg->bDepthWriteEnable)
	{
		/* When doing a depth test, our depth is always in psProg->ui32TempBase,
		 * so move along 1 for our state word loading
		 */
		ui32TempReg++;
		psProg->ui32MaxTempUsed++;
	}

	if (psProg->bTexkillEnable)
	{
		if (psProg->ui32PredReg == 0)
		{
			ui32Pred = EURASIA_USE1_SPRED_P0;
		}
		else if (psProg->ui32PredReg == 1)
		{
			ui32Pred = EURASIA_USE1_SPRED_P1;
		}
		else
		{
			USEGEN_DPF("Unknown predicate register for texkill.  Using P1.");
			USEGEN_DBG_BREAK;
			ui32Pred = EURASIA_USE1_SPRED_P1;
		}
	}
	else
	{
		ui32Pred = EURASIA_USE1_EPRED_ALWAYS;
	}

	if (psProg->bOMask)
	{
		switch (psProg->eMaskBank)
		{
		case USEGEN_REG_BANK_PRIM:
			eMaskBank = USE_REGTYPE_PRIMATTR;
			break;
		case USEGEN_REG_BANK_OUTPUT:
			eMaskBank = USE_REGTYPE_OUTPUT;
			break;
		case USEGEN_REG_BANK_TEMP:
			eMaskBank = USE_REGTYPE_TEMP;
			break;
		default:
			USEGEN_DPF("Warning: Unknown bank for mask register");
			eMaskBank = USE_REGTYPE_PRIMATTR;
			break;
		}
		ui32MaskReg = psProg->ui32MaskReg;
	}

#if !defined(SGX543) && !defined(SGX544) && !defined(SGX554)
	/* 
	 * Always build and load state words.
	 */
	BuildStateWords(psProg, &ui32SW0, &ui32SW1);

	/* Two temps are always needed for state words
	 * Those will be loaded just before the ATST8 or DEPTHF instructions
	 */
	psProg->ui32MaxTempUsed += 2;
#else
	psProg->ui32MaxTempUsed += 1;
#endif

	/* 
	 * Only perform the pcoeff if we need to perform an ATST8
	 * Hardware people complain if state words and coefficients are
	 * different than what the rest of the shader uses
	 */
	if (bATST)
	{
		IMG_UINT32		ui32Src0;
		IMG_UINT32		ui32Src1;
		IMG_BOOL		bSMLSIUsed = IMG_FALSE;

		/*
		  pcoeff is implicitely twice iterated.  Coefficients for each iteration are:
		  Src0 = Don't care, Src1 = Coeff. B, Src2 = Don't care
		  Src0 = Coeff. A,   Src1 = Coeff. C, Src2 = Visibility test reg.
		*/
#if defined(FIX_HW_BRN_31547)
		if (psProg->bZABS4D)
		{
			ui32Src0 = 0;
			ui32Src1 = 2;
		}
		else
#endif
		{
#if !defined(SGX_FEATURE_ALPHATEST_COEFREORDER) || !defined(EUR_CR_ISP_DEPTHSORT_FB_ABC_ORDER_MASK)
			BuildSMLSI((PPVR_USE_INST)pui32Buffer,
					   0, 0, 0, /*noswiz*/0,
					   /**/0, 0, 1, 0);
			pui32Buffer += USE_INST_LENGTH;
			bSMLSIUsed = IMG_TRUE;
#endif
			ui32Src0 = 0;
			ui32Src1 = 1;
		}

		pui32Buffer[0] = (((psProg->ui32StartPA + ui32Src0) << EURASIA_USE0_SRC0_SHIFT)|
						  ((psProg->ui32StartPA + ui32Src1) << EURASIA_USE0_SRC1_SHIFT)|
						  (EURASIA_USE_SPECIAL_CONSTANT_ZERO << EURASIA_USE0_SRC2_SHIFT)|
						  (EURASIA_USE0_S1STDBANK_PRIMATTR << EURASIA_USE0_S1BANK_SHIFT)|
						  (EURASIA_USE0_S2EXTBANK_SPECIAL << EURASIA_USE0_S2BANK_SHIFT));


		pui32Buffer[1] = ((EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
						  (EURASIA_USE1_VISTEST_OP2_PTCTRL << EURASIA_USE1_OTHER_OP2_SHIFT) |
						  (EURASIA_USE1_SPECIAL_OPCAT_VISTEST << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
						  (EURASIA_USE1_VISTEST_PTCTRL_TYPE_PLANE << EURASIA_USE1_VISTEST_PTCTRL_TYPE_SHIFT) |
						  (EURASIA_USE1_S0STDBANK_PRIMATTR << EURASIA_USE1_S0BANK_SHIFT)|
						  (EURASIA_USE1_S2BEXT));

		pui32Buffer += USE_INST_LENGTH;

		if (bSMLSIUsed)
		{
			psProg->ui32Offset = 0xFFFFFFFF;
		}
		else
		{
			psProg->ui32Offset = (IMG_UINT32)(pui32Buffer - pui32BufferBase);
		}
	}

#if defined(SGX545)
	/* Sample masking, output masking */

	if (psProg->bSampleMask || psProg->bOMask)
	{
		IMG_UINT32		ui32MaskPred;

		if (psProg->ui32PredReg == 0)
		{
			ui32MaskPred = EURASIA_USE1_SPRED_P0;
		}
		else if (psProg->ui32PredReg == 1)
		{
			ui32MaskPred = EURASIA_USE1_SPRED_P1;
		}
		else
		{
			USEGEN_DPF("Unknown predicate register for depthwrite."
					   "Using P1.");
			USEGEN_DBG_BREAK;
			ui32MaskPred = EURASIA_USE1_SPRED_P1;
		}

		BuildSHL((PPVR_USE_INST)pui32Buffer,
				 ui32Pred,
				 0,
				 EURASIA_USE1_RCNTSEL,
				 USE_REGTYPE_TEMP, ui32TempReg,
				 USE_REGTYPE_IMMEDIATE, 0x1,
				 USE_REGTYPE_SPECIAL,
				 EURASIA_USE_SPECIAL_BANK_SAMPLENUMBER |
				 EURASIA_USE_SPECIAL_INTERNALDATA);
		pui32Buffer += USE_INST_LENGTH;

		if (psProg->bOMask && psProg->bFloatToAlpha)
		{
			BuildPCKUNPCK((PPVR_USE_INST) pui32Buffer,
						  EURASIA_USE1_EPRED_ALWAYS,
						  0,
						  EURASIA_USE1_RCNTSEL,
						  EURASIA_USE1_PCK_FMT_U8,
						  EURASIA_USE1_PCK_FMT_F32,
						  EURASIA_USE0_PCK_SCALE,
						  USE_REGTYPE_TEMP, ui32TempReg +2, 0x8,
						  eMaskBank,
						  ui32MaskReg, 0, 
						  eMaskBank,
						  ui32MaskReg, 0);
			pui32Buffer += USE_INST_LENGTH;

			eMaskBank = USE_REGTYPE_TEMP;
			ui32MaskReg = ui32TempReg +2;
			psProg->ui32MaxTempUsed++;
		}

		if (psProg->bSampleMask)
		{
			if (psProg->bOMask)
			{
				BuildANDIMM((PPVR_USE_INST)pui32Buffer,
							ui32Pred,
							0,
							EURASIA_USE1_RCNTSEL,
							eMaskBank, ui32MaskReg,
							eMaskBank, ui32MaskReg,
							psProg->bySampleMask,
							0);
				pui32Buffer += USE_INST_LENGTH;
			}
			else
			{
				BuildLIMM((PPVR_USE_INST) pui32Buffer,
						  ui32Pred,
						  EURASIA_USE1_RCNTSEL,
						  USE_REGTYPE_TEMP, ui32TempReg +1,
						  psProg->bySampleMask);
				pui32Buffer += USE_INST_LENGTH;

				eMaskBank = USE_REGTYPE_TEMP;
				ui32MaskReg = ui32TempReg +1;
			}
		}

		BuildALUTST((PPVR_USE_INST) pui32Buffer,
					ui32Pred,
					1,
					EURASIA_USE1_TEST_CRCOMB_AND,
					USE_REGTYPE_TEMP, ui32TempReg, 0,/* don't write result */
					USE_REGTYPE_TEMP, ui32TempReg,
					eMaskBank, ui32MaskReg,
					ui32MaskPred -1, /* predicates 0x1 == 0, 0x2 == 1 */
					EURASIA_USE1_TEST_STST_NONE,
					EURASIA_USE1_TEST_ZTST_NOTZERO,
					EURASIA_USE1_TEST_CHANCC_SELECT0,
					EURASIA_USE0_TEST_ALUSEL_BITWISE,
					EURASIA_USE0_TEST_ALUOP_BITWISE_AND);
		pui32Buffer += USE_INST_LENGTH;

		/* From now on, we might not have to perform all the tasks */
		ui32Pred = ui32MaskPred;

		/* we used temps destined to hold state words, no need to add them */
	}

	/* AlphaToCoverage */

	if (psProg->bAlphaToCoverage)
	{
		/* This should really be a !pX LIMM R1 SW1 at the end
		 * but I cannot be sure that the predicate used can be negated
		 * Therefore, reset the temp here
		 */
		BuildLIMM((PPVR_USE_INST) pui32Buffer,
				  EURASIA_USE1_EPRED_ALWAYS,
				  EURASIA_USE1_RCNTSEL,
				  USE_REGTYPE_TEMP, ui32TempReg,
				  0x0);
		pui32Buffer += USE_INST_LENGTH;

		BuildLIMM((PPVR_USE_INST) pui32Buffer,
				  ui32Pred,
				  EURASIA_USE1_RCNTSEL,
				  USE_REGTYPE_TEMP, ui32TempReg,
				  psProg->ui8AlphaRef);
		pui32Buffer += USE_INST_LENGTH;

		BuildIMA16((PPVR_USE_INST) pui32Buffer,
				   ui32Pred,
				   0, 0,
				   USE_REGTYPE_TEMP, ui32TempReg,
				   USE_REGTYPE_TEMP, ui32TempReg, EURASIA_USE_SRCMOD_NONE,
				   USE_REGTYPE_SPECIAL,
				   EURASIA_USE_SPECIAL_BANK_SAMPLENUMBER |
				   EURASIA_USE_SPECIAL_INTERNALDATA,
				   EURASIA_USE_SRCMOD_NONE,
				   USE_REGTYPE_TEMP, ui32TempReg, EURASIA_USE_SRCMOD_NONE);
		pui32Buffer += USE_INST_LENGTH;

		BuildSHL((PPVR_USE_INST)pui32Buffer,
				 ui32Pred,
				 0,
				 EURASIA_USE1_RCNTSEL,
				 USE_REGTYPE_TEMP, ui32TempReg,
				 USE_REGTYPE_TEMP, ui32TempReg,
				 USE_REGTYPE_IMMEDIATE, 24);
		pui32Buffer += USE_INST_LENGTH;

		BuildORIMM((PPVR_USE_INST) pui32Buffer,
				   EURASIA_USE1_EPRED_ALWAYS,
				   0,
				   EURASIA_USE1_RCNTSEL,
				   USE_REGTYPE_TEMP, ui32TempReg +1,
				   USE_REGTYPE_TEMP, ui32TempReg,
				   ui32SW1, 0);
		pui32Buffer += USE_INST_LENGTH;
		
	}
	else
#endif
#if !defined(SGX543) && !defined(SGX544) && !defined(SGX554)
	{
		BuildLIMM((PPVR_USE_INST) pui32Buffer,
				  EURASIA_USE1_EPRED_ALWAYS,
				  EURASIA_USE1_RCNTSEL,
				  USE_REGTYPE_TEMP, ui32TempReg +1,
				  ui32SW1);
		pui32Buffer += USE_INST_LENGTH;
	}

	BuildLIMM((PPVR_USE_INST) pui32Buffer,
			  EURASIA_USE1_EPRED_ALWAYS,
			  EURASIA_USE1_RCNTSEL,
			  USE_REGTYPE_TEMP, ui32TempReg,
			  ui32SW0);
	pui32Buffer += USE_INST_LENGTH;
#else
	BuildLIMM((PPVR_USE_INST) pui32Buffer,
			  EURASIA_USE1_EPRED_ALWAYS,
			  EURASIA_USE1_RCNTSEL,
			  USE_REGTYPE_TEMP, ui32TempReg,
			  0);
	pui32Buffer += USE_INST_LENGTH;
#endif

	/* still within our temp usage limit */

	/* if OMask is used above, we already have a temporary alpha */

	if (psProg->bFloatToAlpha && !psProg->bOMask)
	{
		/* 
		   Pack the unpacked output we get at the end of an extended pixel
		   shader into a temporary so we can alpha test it.

		   pcku8f32.skipinv.scale r3.bytemask1000, pa3.0, pa2.0, nearest
				
		   with:
		   ui32TempReg          -> r1
		   psProg->ui32OutputPA -> pa0
		*/
		#if defined(SGX_FEATURE_USE_VEC34)
		BuildVPCKUNPCK((PPVR_USE_INST) pui32Buffer,
						EURASIA_USE1_EPRED_ALWAYS,
						0,
						EURASIA_USE1_SKIPINV,
						EURASIA_USE1_PCK_FMT_U8,
						EURASIA_USE1_PCK_FMT_F32,
						EURASIA_USE0_PCK_SCALE,
						USE_REGTYPE_TEMP, ui32TempReg + 2, 0x8,
						USE_REGTYPE_PRIMATTR, psProg->ui32OutputPA + 2,
						USE_REGTYPE_PRIMATTR, psProg->ui32OutputPA + 2,
						1, // chan 0
						0, // chan 1
						0, // chan 2
						0);	// chan 3
		#else
		BuildPCKUNPCK((PPVR_USE_INST) pui32Buffer,
					  EURASIA_USE1_EPRED_ALWAYS,
					  0,
					  EURASIA_USE1_RCNTSEL,
					  EURASIA_USE1_PCK_FMT_U8,
					  EURASIA_USE1_PCK_FMT_F32,
					  EURASIA_USE0_PCK_SCALE,
					  USE_REGTYPE_TEMP, ui32TempReg + 2, 0x8,
					  USE_REGTYPE_PRIMATTR, psProg->ui32OutputPA + 3, 0, 
					  USE_REGTYPE_PRIMATTR, psProg->ui32OutputPA + 2, 0);
		#endif
		pui32Buffer += USE_INST_LENGTH;

		psProg->ui32MaxTempUsed++;
	}

	/* Now perform the actual tests */
	if (bATST)
	{
		IMG_UINT32 ui32Flags = 0;

		USE_REGTYPE eDstType;
		IMG_UINT32  ui32DstIx;
		USE_REGTYPE eSrc0Type;
		IMG_UINT32  ui32Src0Ix;

		if (psProg->bAlphaTest || psProg->bAlphaToCoverage)
		{
			if (psProg->bFloatToAlpha)
			{
				eDstType = USE_REGTYPE_TEMP;
				ui32DstIx = ui32TempReg +2;
				eSrc0Type = USE_REGTYPE_TEMP;
				ui32Src0Ix = ui32TempReg +2;
			}
			else
			{
				eDstType = USE_REGTYPE_PRIMATTR;
				ui32DstIx = psProg->ui32OutputPA;
				eSrc0Type = USE_REGTYPE_PRIMATTR;
				ui32Src0Ix = psProg->ui32OutputPA;
			}
		}
		else
		{
			ui32Flags |= EURASIA_USE1_VISTEST_ATST8_TWOSIDED;
			
			eDstType = USE_REGTYPE_TEMP;
			ui32DstIx = ui32TempReg;
			eSrc0Type = USE_REGTYPE_TEMP;
			ui32Src0Ix = ui32TempReg;
		}

		if (psProg->bDepthWriteEnable)
		{
			ui32Flags |= EURASIA_USE1_VISTEST_ATST8_DISABLEFEEDBACK |
						 (psProg->ui32PredReg << EURASIA_USE1_VISTEST_ATST8_PDST_SHIFT) |
						 EURASIA_USE1_VISTEST_ATST8_PDSTENABLE;
		}
		else
		{
			ui32Flags |= EURASIA_USE1_END;
		}

		/*
		  if texkilling:            p1 atst8 r0,  !p0, r0,  r0, r1
		  if alphatest:                atst8 pa0, !p0, pa0, r0, r1
		  if texkill and alphatest: p1 atst8 pa0, !p0, pa0, r0, r1
		  if depthwrite:               atst8 pa0,  p1, pa0, r0, r1, nfb
		  
		  with:
		  psProg->ui32OutputPA -> pa0
		  psProg->ui32PredReg  -> p1
		  
		  see: section 3.10.9.in USSE instruction set reference doc.
		*/
#if !defined(SGX543) && !defined(SGX544) && !defined(SGX554)
		BuildATST8((PPVR_USE_INST) pui32Buffer,
				   ui32Pred,
				   ui32Flags,
				   eDstType, ui32DstIx,
				   eSrc0Type, ui32Src0Ix,
				   USE_REGTYPE_TEMP, ui32TempReg,
				   USE_REGTYPE_TEMP, ui32TempReg+1,
				   0);
#else
		BuildATST8((PPVR_USE_INST) pui32Buffer,
				   ui32Pred,
				   ui32Flags,
				   eDstType, ui32DstIx,
				   eSrc0Type, ui32Src0Ix,
				   USE_REGTYPE_TEMP, ui32TempReg,
				   USE_REGTYPE_TEMP, ui32TempReg,
				   0);
#endif
		pui32Buffer += USE_INST_LENGTH;

		/* if depthwriting, set a predicate if one hasn't been used yet */
		if (psProg->bDepthWriteEnable)
		{
			if (psProg->ui32PredReg == 0)
			{
				ui32Pred = EURASIA_USE1_SPRED_P0;
			}
			else if (psProg->ui32PredReg == 1)
			{
				ui32Pred = EURASIA_USE1_SPRED_P1;
			}
			else
			{
				USEGEN_DPF("Unknown predicate register for depthwrite."
						   "Using P1.");
				USEGEN_DBG_BREAK;
				ui32Pred = EURASIA_USE1_SPRED_P1;
			}
		}
	}

	if (psProg->bDepthWriteEnable)
	{
#if !defined(SGX_FEATURE_USE_FCLAMP)
		/* Clamp our depth to 0 ... 1*/
		BuildFMINMAX((PPVR_USE_INST) pui32Buffer,
					 EURASIA_USE1_FLOAT_OP2_MAX,
					 ui32Pred, 0,
					 EURASIA_USE1_RCNTSEL,
					 USE_REGTYPE_TEMP, psProg->ui32TempBase,
					 USE_REGTYPE_TEMP, psProg->ui32TempBase, 0,
					 USE_REGTYPE_SPECIAL, EURASIA_USE_SPECIAL_CONSTANT_ZERO, 0);
		pui32Buffer+=USE_INST_LENGTH;
		BuildFMINMAX((PPVR_USE_INST) pui32Buffer,
					 EURASIA_USE1_FLOAT_OP2_MIN,
					 ui32Pred, 0,
					 EURASIA_USE1_RCNTSEL,
					 USE_REGTYPE_TEMP, psProg->ui32TempBase,
					 USE_REGTYPE_TEMP, psProg->ui32TempBase, 0,
					 USE_REGTYPE_SPECIAL, EURASIA_USE_SPECIAL_CONSTANT_FLOAT1, 0);
		pui32Buffer += USE_INST_LENGTH;
#else
		BuildFCLAMP((PPVR_USE_INST) pui32Buffer,
					SGX545_USE1_FLOAT_OP2_MINMAX,
					ui32Pred, 0,
					EURASIA_USE1_RCNTSEL,
					USE_REGTYPE_TEMP, psProg->ui32TempBase,
					USE_REGTYPE_TEMP, psProg->ui32TempBase, 0,
					USE_REGTYPE_SPECIAL, EURASIA_USE_SPECIAL_CONSTANT_ZERO, 0,
					USE_REGTYPE_SPECIAL, EURASIA_USE_SPECIAL_CONSTANT_FLOAT1, 0);
		pui32Buffer += USE_INST_LENGTH;
#endif

#if !defined(SGX543) && !defined(SGX544) && !defined(SGX554)
		/* This will always be the last instruction, so set the end flag */
		BuildDEPTHF((PPVR_USE_INST) pui32Buffer,
					ui32Pred,
					EURASIA_USE1_VISTEST_DEPTHF_TWOSIDED |
					EURASIA_USE1_END,
					USE_REGTYPE_TEMP, psProg->ui32TempBase,
					USE_REGTYPE_TEMP, ui32TempReg,
					USE_REGTYPE_TEMP, ui32TempReg+1);
#else
		/* This will always be the last instruction, so set the end flag */
		BuildDEPTHF((PPVR_USE_INST) pui32Buffer,
					ui32Pred,
					EURASIA_USE1_VISTEST_DEPTHF_TWOSIDED |
					EURASIA_USE1_END,
					USE_REGTYPE_TEMP, psProg->ui32TempBase,
					USE_REGTYPE_TEMP, ui32TempReg,
					USE_REGTYPE_TEMP, ui32TempReg);
#endif
		pui32Buffer += USE_INST_LENGTH;

	}

	USEGEN_ASSERT((pui32Buffer - pui32BufferBase) <=
			USE_INST_TO_UINT32(USEGEN_WRITE_ISP_FEEDBACK_FRAGMENT_INST));

	return pui32Buffer;
}


/******************************************************************************
 * Complete Program Functions
 ******************************************************************************/
#if defined(SGX_FEATURE_VCB)
/***********************************************************************//**
* Write the pre-cull phase of the clear vertex program
*
* @param	pui32Base			The linear address to write to
* @param	ui32PostCullAddress	The DevVAddr address of the post-cull
*								phase.
* @param	ui32NumOutputs		The number of dword outputs to write.
* 								This will usually be 8 for a standard clear.
* 								(X,Y,Z,W + R,G,B,A)
* @return	Pointer to the end of the written block
*
* @note The ui32PostCullAddress is only required when unlimited phases
* are permitted.  In other cases, this parameter is ignored.
**************************************************************************/
IMG_PUINT32 USEGenWritePreCullClearVtxProgram (IMG_PUINT32 pui32Base,
											   IMG_UINT32 ui32PostCullAddress,
											   IMG_UINT32 ui32NumOutputs)
{
	IMG_PUINT32 pui32Current = pui32Base;

	USEGEN_ASSERT (pui32Current);

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	/* With unlimited phases, this is always the first phase.
	 * The second phase should have already been written and
	 * the DevVAddr passes in in the ui32PostCullAddress argument */
	USEGEN_ASSERT (ui32PostCullAddress != 0);

	/* Write the PHAS instruction */
	pui32Current = USEGenWritePhaseFragment (pui32Current, ui32PostCullAddress,
											 0, USEGEN_VCB_DEPENDANT, IMG_FALSE,
											 IMG_FALSE, IMG_FALSE, IMG_FALSE);
#else
	/* Without unlimited phases, the postcull address is
	 * of no interest to us */
	PVR_UNREFERENCED_PARAMETER(ui32PostCullAddress);
#endif

	/* The pre-cull clear moves the entire vertex into
	 * the output and does the emit to the VCB
	 */

	BuildMOV ((PPVR_USE_INST)pui32Current,
			  EURASIA_USE1_EPRED_ALWAYS,
			  ui32NumOutputs-1, /* Repeat count */
			  EURASIA_USE1_RCNTSEL,
			  USE_REGTYPE_OUTPUT, 0,
			  USE_REGTYPE_PRIMATTR, 0,
			  0 /* Src mod */);

	pui32Current += USE_INST_LENGTH;

	/* Finish off with the precull fragment */
	pui32Current = USEGenWriteEndPreCullVtxShaderFragment (pui32Current);

	return pui32Current;

}
#endif

/***********************************************************************//**
* Write the clear vertex program
*
* @param	pui32Base			The linear address to write to
* @param	ui32NumOutputs		The number of dword outputs to write.
* 								This will usually be 8 for a standard clear.
* 								(X,Y,Z,W + R,G,B,A)
* @return	Pointer to the end of the written block
*
* @note On chips with VCB-splitting of vertex tasks, this will write the
* post-VCB-culled phase of the vertex shader
**************************************************************************/
IMG_PUINT32 USEGenWriteClearVtxProgram (IMG_PUINT32 pui32Base, IMG_UINT32 ui32NumOutputs)
{
	IMG_PUINT32 pui32Current = pui32Base;

	USEGEN_ASSERT (pui32Current);

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	/* With unlimited phases, this will always be the final phase */

	/* Write the PHAS instruction */
	pui32Current = USEGenWritePhaseFragment (pui32Current, 0,
											 0, USEGEN_NO_DEPENDANT, IMG_FALSE,
											 IMG_FALSE, IMG_TRUE, IMG_FALSE);
#endif

#if !defined(SGX_FEATURE_VCB)
	/* With the VCB, this phase need only do the emit.  In other
	 * cases, we need to do the move here */
	BuildMOV ((PPVR_USE_INST)pui32Current,
			  EURASIA_USE1_EPRED_ALWAYS,
			  ui32NumOutputs - 1 /* Repeat count */,
			  EURASIA_USE1_RCNTSEL,
			  USE_REGTYPE_OUTPUT, 0,
			  USE_REGTYPE_PRIMATTR, 0,
			  0 /* Src mod */);
	pui32Current += USE_INST_LENGTH;
#else
	PVR_UNREFERENCED_PARAMETER(ui32NumOutputs);
#endif

	/*
	 * And finish the program
	 */
	pui32Current = USEGenWriteEndVtxShaderFragment (pui32Current);

	return pui32Current;
}


/***********************************************************************//**
* Writes the complex geometry "phase 1" state update.
*
* @param pui32Base			The base (linear) address to write the program to
* @param ui32State0			State word 0
* @param ui32State1			State word 1
* @param ui32State2			State word 2
* @param ui32State3			State word 3
* @return Pointer to the end of the written program
* @note The state words are described in section 4.6 of the SGX545 "3D Input
* Parameter Format" document. Other chips are currently unsupported
* @note State 3 is only used when BRN27945 isn't set
*
**************************************************************************/
IMG_PUINT32 USEGenWriteCGPhase1StateEmitProgram (IMG_PUINT32 pui32Base,
												 IMG_UINT32 ui32State0,
												 IMG_UINT32 ui32State1,
												 IMG_UINT32 ui32State2,
												 IMG_UINT32 ui32State3)
{
	IMG_PUINT32 pui32Current = pui32Base;

#if !defined(SGX545)
	USEGEN_DPF ("Complex Geometry Phase 1 State Emit isn't supported on"
				" this chip type.");
	USEGEN_DBG_BREAK;
	
	/* unreferenced parameters */
	PVR_UNREFERENCED_PARAMETER(ui32State0);
	PVR_UNREFERENCED_PARAMETER(ui32State1);
	PVR_UNREFERENCED_PARAMETER(ui32State2);
	PVR_UNREFERENCED_PARAMETER(ui32State3);
	
#else /* !defined(SGX545) */

	USEGEN_ASSERT (pui32Current);

	/*
	 * On SGX545, the CG phase 1 emit should move
	 * the 3 state words to the output buffer and
	 * do an emit to the MTE
	 */

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	/* Write the PHAS as the final phase */
	pui32Current = USEGenWritePhaseFragment (pui32Current, 0,
											 0, USEGEN_NO_DEPENDANT, IMG_FALSE,
											 IMG_FALSE, IMG_TRUE, IMG_FALSE);
#endif

	BuildLIMM ((PPVR_USE_INST) pui32Current, EURASIA_USE1_EPRED_ALWAYS,
			   0 /* Flags */, USE_REGTYPE_OUTPUT, 0,
			   ui32State0);
	pui32Current += USE_INST_LENGTH;

	BuildLIMM ((PPVR_USE_INST) pui32Current, EURASIA_USE1_EPRED_ALWAYS,
			   0 /* Flags */, USE_REGTYPE_OUTPUT, 1,
			   ui32State1);
	pui32Current += USE_INST_LENGTH;

	BuildLIMM ((PPVR_USE_INST) pui32Current, EURASIA_USE1_EPRED_ALWAYS,
			   0 /* Flags */, USE_REGTYPE_OUTPUT, 2,
			   ui32State2);
	pui32Current += USE_INST_LENGTH;

#if !defined(FIX_HW_BRN_27945)
	BuildLIMM ((PPVR_USE_INST) pui32Current, EURASIA_USE1_EPRED_ALWAYS,
			   0 /* Flags */, USE_REGTYPE_OUTPUT, 3,
			   ui32State3);
	pui32Current += USE_INST_LENGTH;
#else
	PVR_UNREFERENCED_PARAMETER(ui32State3);
#endif

	/* And the emit */
	BuildEMITMTE ((PPVR_USE_INST) pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS,
				  EURASIA_USE1_END,
				  USE_REGTYPE_IMMEDIATE,
				  0,
				  USE_REGTYPE_IMMEDIATE,
				  1,
				  USE_REGTYPE_IMMEDIATE,
				  0,
				  0,
				  IMG_TRUE);
	pui32Current += USE_INST_LENGTH;

#endif /* !defined(SGX545) */

	/* In all cases, return pui32Current */
	return pui32Current;
}

/***********************************************************************//**
* Writes the (static) ITP USE program
*
* @param pui32Base			The base (linear) address to write the program to
* @return Pointer to the end of the written program
**************************************************************************/
IMG_PUINT32 USEGenWriteCGITPProgram (IMG_PUINT32 pui32Base)
{
	IMG_PUINT32 pui32Current = pui32Base;

#if !defined(SGX545)
	USEGEN_DPF ("Writing Complex Geometry ITP program isn't supported on"
				" this chip type.");
	USEGEN_DBG_BREAK;
#else /* !defined(SGX545) */

	USEGEN_ASSERT (pui32Current);

	/*
	 * The ITP program is a helper program fired by the hardware
	 * on occasion.  It's just is to do a state emit with
	 * an ITP state (which is a single static word)
	 */

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	/* Write the PHAS as the final phase */
	pui32Current = USEGenWritePhaseFragment (pui32Current, 0,
											 0, USEGEN_NO_DEPENDANT, IMG_FALSE,
											 IMG_FALSE, IMG_TRUE, IMG_FALSE);
#endif

	BuildLIMM ((PPVR_USE_INST) pui32Current,
			   EURASIA_USE1_EPRED_ALWAYS,
			   EURASIA_USE1_SKIPINV,
			   USE_REGTYPE_OUTPUT,
			   0,
			   EURASIA_TAPDSSTATE_COMPLEXGEOM1_ITP);
	pui32Current += USE_INST_LENGTH;

	BuildEMITMTE((PPVR_USE_INST)pui32Current,
				 EURASIA_USE1_EPRED_ALWAYS,
				 (SGX545_USE1_EMIT_VCB_EMITTYPE_STATE <<
				  SGX545_USE1_EMIT_VCB_EMITTYPE_SHIFT) |
				 EURASIA_USE1_END,
				 USE_REGTYPE_OUTPUT,
				 0,
				 USE_REGTYPE_IMMEDIATE,
				 (EURASIA_MTEEMIT1_COMPLEX | EURASIA_MTEEMIT1_COMPLEX_PHASE1),
				 USE_REGTYPE_IMMEDIATE,
				 0,
				 0, /* INCP */
				 IMG_TRUE /* Free partition */);
	pui32Current += USE_INST_LENGTH;

#endif /* !defined(SGX545) */

	/* In all cases, return pui32Current */
	return pui32Current;

}

/***********************************************************************//**
* Writes the (static) Vertex Buffer Update Pointer USE program
*
* @param pui32Base			The base (linear) address to write the program to
* @return Pointer to the end of the written program
**************************************************************************/
IMG_PUINT32 USEGenWriteCGVtxBufferPtrProgram (IMG_PUINT32 pui32Base)
{
	IMG_PUINT32 pui32Current = pui32Base;

#if !defined(SGX545)
	USEGEN_DPF ("Writing Complex Geometry Vertex Buffer Pointer program isn't"
				" supported on this chip type.");
	USEGEN_DBG_BREAK;
#else /* !defined(SGX545) */

	USEGEN_ASSERT (pui32Current);

	/*
	 * The Vertex Buffer Update Pointer program is a little
	 * helper program that's run occasionally by the HW.
	 * It does a "write pointer" state emit (2 words -
	 * the one after the header is already in pa0)
	 * to the VCB and then MTE.
	 */
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	pui32Current = USEGenWritePhaseFragment(pui32Current, 0, 0,
											USEGEN_NO_DEPENDANT,
											IMG_FALSE, IMG_FALSE,
											IMG_TRUE, IMG_FALSE);
#endif

	BuildLIMM ((PPVR_USE_INST) pui32Current,
			   EURASIA_USE1_EPRED_ALWAYS,
			   EURASIA_USE1_SKIPINV,
			   USE_REGTYPE_OUTPUT,
			   0,
			   EURASIA_TACTLPRES_WR_PTR);
	pui32Current += USE_INST_LENGTH;

	BuildMOV((PPVR_USE_INST) pui32Current,
			 EURASIA_USE1_EPRED_ALWAYS,
			 0,
			 EURASIA_USE1_RCNTSEL,
			 USE_REGTYPE_OUTPUT,
			 1,
			 USE_REGTYPE_PRIMATTR,
			 0,
			 0);
	pui32Current += USE_INST_LENGTH;

	BuildEMITVCB ((PPVR_USE_INST) pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS,
				  (SGX545_USE1_EMIT_VCB_EMITTYPE_STATE <<
				   SGX545_USE1_EMIT_VCB_EMITTYPE_SHIFT),
				  USE_REGTYPE_TEMP,
				  0,
				  USE_REGTYPE_IMMEDIATE,
				  (EURASIA_MTEEMIT1_COMPLEX | EURASIA_MTEEMIT1_COMPLEX_PHASE2),
				  USE_REGTYPE_IMMEDIATE,
				  0,
				  0 /* INCP */,
				  IMG_FALSE /* Don't free the partition yet */);
	pui32Current += USE_INST_LENGTH;

	/* Wait for the output buffer to become ready again before
	 * emitting to the MTE */
	BuildWOP((PPVR_USE_INST)pui32Current, 0, 0);
	pui32Current += USE_INST_LENGTH;

	BuildEMITMTE((PPVR_USE_INST)pui32Current,
				 EURASIA_USE1_EPRED_ALWAYS,
				 (SGX545_USE1_EMIT_VCB_EMITTYPE_STATE <<
				  SGX545_USE1_EMIT_VCB_EMITTYPE_SHIFT) |
				 EURASIA_USE1_END,
				 USE_REGTYPE_OUTPUT,
				 0,
				 USE_REGTYPE_IMMEDIATE,
				 (EURASIA_MTEEMIT1_COMPLEX | EURASIA_MTEEMIT1_COMPLEX_PHASE2),
				 USE_REGTYPE_IMMEDIATE,
				 0,
				 0, /* INCP */
				 IMG_TRUE /* Free partition */);
	pui32Current += USE_INST_LENGTH;


#endif /* !defined(SGX545) */

	/* In all cases, return pui32Current */
	return pui32Current;

}

#if defined(SGX_FEATURE_VCB)
/***********************************************************************//**
* Writes the pre-VCB vertex DMA USE task
*
* @param pui32Base			The base (linear) address to write the program to
* @param ui32VtxSize		The size of the vertex in DWORDs
* @param bViewportIdxPresent	Whether the viewport array index is present
* @param ui32PostCullAddress The (DevV) address of the post-VCB phase
* @param bIterateVertexID 	Whether the vertex id should be iterated
*							(used in the post cull phase for intreg).
* @return Pointer to the end of the written program
**************************************************************************/
IMG_PUINT32 USEGenWritePreCullVtxDMAProgram (IMG_PUINT32 pui32Base,
											 IMG_UINT32 ui32VtxSize,
											 IMG_BOOL bViewportIdxPresent,
											 IMG_UINT32 ui32PostCullAddress,
											 IMG_BOOL bIterateVertexID)
{
	IMG_PUINT32 pui32Current = pui32Base;
	IMG_UINT32 ui32Start = 0;
	IMG_UINT32 ui32Viewport = 0;
	IMG_UINT32 ui32Num = ui32VtxSize;

#if !defined(SGX545)
	USEGEN_DPF ("Complex Geometry Vertex DMA programs (pre-cull) aren't"
				" supported on this chip type.");
	USEGEN_DBG_BREAK;
#else /* !defined(SGX545) */

	USEGEN_ASSERT (pui32Current);

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	/*
	 * With unlimited phases, this is always the first phase.
	 * The second phase should have already been written and
	 * the DevVAddr passes in in the ui32PostCullAddress argument
	 */

	/* We may require up to 4 temps if we're doing Gaud Band Clipping post-cull,
	   We will also require at least 1 if we have clip or cull outputs from the
	   GS */
	pui32Current = USEGenWritePhaseFragment(pui32Current, ui32PostCullAddress,
											4,
											USEGEN_VCB_DEPENDANT,
											IMG_FALSE, IMG_FALSE,
											IMG_FALSE, IMG_FALSE);
#else
	/* Without unlimited phases, the postcull address is
	 * of no interest to us */
	PVR_UNREFERENCED_PARAMETER(ui32PostCullAddress);
#endif

	if (bIterateVertexID)
	{
		ui32Start++;
		ui32Viewport++;
	}

	if (bViewportIdxPresent)
	{
		/* Viewport array index needs to be moved from the first element to the
		 * last element in the output buffer, if it's present
		 * First, we copy all the other elements.  After that, we add a simple
		 * MOV from pa0 to o<last Element>
		 */
		ui32Num--;
		ui32Start++;
	}

	/* Ensure the vertex size is sane */
	USEGEN_ASSERT (ui32Num > 0);

	/* Now write the state update */
	pui32Current = USEGenWriteStateEmitFragment (pui32Current, ui32Num,
												 ui32Start);

	if (bViewportIdxPresent)
	{
		/* Now do the move of the viewport array index */
		BuildMOV((PPVR_USE_INST) pui32Current,
				 EURASIA_USE1_EPRED_ALWAYS, 0,
				 EURASIA_USE1_RCNTSEL,
				 USE_REGTYPE_OUTPUT, ui32Num, /* Last element of the vertex */
				 USE_REGTYPE_PRIMATTR, ui32Viewport, /* Viewport */
				 0);
		pui32Current += USE_INST_LENGTH;
	}

	/* Now we write the specialised emit */
	BuildEMITVCB((PPVR_USE_INST) pui32Current,
				 EURASIA_USE1_EPRED_ALWAYS,
				 (SGX545_USE1_EMIT_VCB_EMITTYPE_VERTEX <<
				  SGX545_USE1_EMIT_VCB_EMITTYPE_SHIFT) |
				 SGX545_USE1_EMIT_VERTEX_TWOPART |
				 EURASIA_USE1_END,
				 USE_REGTYPE_TEMP,
				 0,
				 USE_REGTYPE_IMMEDIATE,
				 (EURASIA_MTEEMIT1_COMPLEX | EURASIA_MTEEMIT1_COMPLEX_PHASE2),
				 USE_REGTYPE_IMMEDIATE,
				 0,
				 0 /* INCP */,
				 IMG_FALSE /* Don't free the partition yet */);
	pui32Current += USE_INST_LENGTH;

#endif /* !defined(SGX545) */

	/* In all cases, return pui32Current */
	return pui32Current;
}
#endif

/***********************************************************************//**
* Writes the vertex DMA USE task
*
* @note In VCB-enabled chips, this will write the post-VCB phase
* of the task
*
* @param pui32Base			The base (linear) address to write the program to
* @param ui32VtxSize		The size of the vertex
*
* @param ui32ClipFirstPA	Offset in uint32s of the first clipplane in the vtx
* @param ui32ClipCount		The number of clip planes in the vtx.
* @param psIntRegProg		The program parameters for storing integer
*							registers or null if there are none to store
* @return Pointer to the end of the written program
*
* @note The ui32VtxSize is ignored for VCB-enabled chips
**************************************************************************/
IMG_PUINT32 USEGenWriteVtxDMAProgram (IMG_PUINT32 pui32Base,
									  IMG_UINT32 ui32VtxSize,
									  IMG_BOOL bViewportIdxPresent,
									  IMG_UINT32 ui32NumVpt,
									  IMG_BOOL bVPGuadBandClipping,
									  IMG_UINT32 ui32SecondaryGBBase,
									  IMG_UINT32 ui32ClipFirstPA,
									  IMG_UINT32 ui32ClipCount,
									  PUSEGEN_INT_REG_STORE_PROG psIntRegProg)
{
	IMG_PUINT32 pui32Current = pui32Base;
	
#if !defined(SGX545)
	USEGEN_DPF ("Complex Geometry Vertex DMA programs (post-cull) aren't"
				" supported on this chip type.");
	USEGEN_DBG_BREAK;

	/* unreferenced parameter */
	PVR_UNREFERENCED_PARAMETER(ui32VtxSize);
	PVR_UNREFERENCED_PARAMETER(bViewportIdxPresent);
	PVR_UNREFERENCED_PARAMETER(ui32NumVpt);
	PVR_UNREFERENCED_PARAMETER(bVPGuadBandClipping);
	PVR_UNREFERENCED_PARAMETER(ui32SecondaryGBBase);
	PVR_UNREFERENCED_PARAMETER(ui32ClipFirstPA);
	PVR_UNREFERENCED_PARAMETER(ui32ClipCount);
	PVR_UNREFERENCED_PARAMETER(psIntRegProg);

#else /* !defined(SGX545) */

	PVR_UNREFERENCED_PARAMETER(bViewportIdxPresent);
	USEGEN_ASSERT (pui32Current);

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	/* Always the last phase */
	pui32Current = USEGenWritePhaseFragment (pui32Current, 0, 0,
											 USEGEN_NO_DEPENDANT, IMG_FALSE,
											 IMG_FALSE, IMG_TRUE, IMG_FALSE);
#endif

#if !defined(SGX_FEATURE_VCB)
	/* Ensure the vertex size is sane */
	USEGEN_ASSERT (ui32VtxSize > 0);
	/* This pathway hasn't been implemented (and is probably impossible
	 * to hit anyway) */
	USEGEN_DBG_BREAK;

#else /* !defined(SGX_FEATURE_VCB) */
	ui32VtxSize = ui32VtxSize;

	if (bVPGuadBandClipping)
	{
		/*
		 * With VP Guad Band Clipping, we need to transform our position
		 * using:
		 * 	x = x * c0 + c1 * w
		 * 	y = y * c2 + c3 * w
		 * where x, y, w are clip space vertex coords
		 * and c0 - c3 are the transform coefficients which must be
		 * loaded prior to use.
		 *
		 * Each viewport will have it's own set of vertex coordinates
		 * which will depend on the vertex VPT_TGT attribute
		 * which will be MOVed from pa0 in this phase (above)
		 *
		 * When doing this with multiple viewports, the viewport index must be
		 * present
		 */

		/*
		 * First, load up our constants and get the right ones into r0-r3
		 * @todo These should be in secondaries
		 */
		BuildLIMM((PPVR_USE_INST) pui32Current, EURASIA_USE1_EPRED_ALWAYS,
				  0 /* Flags */, USE_REGTYPE_TEMP, 0, ui32SecondaryGBBase);
		pui32Current += USE_INST_LENGTH;

		if (ui32NumVpt > 1)
		{
			USEGEN_ASSERT(bViewportIdxPresent);

			/* Load up the VptIdx into the iterator register and mov
			 * SA[(VptIdx*4)+saBase]
			 * The vptIdx is the last thing in the vertex
			 */
			BuildIMAE((PPVR_USE_INST) pui32Current,
					  EURASIA_USE1_EPRED_ALWAYS,
					  0, EURASIA_USE1_RCNTSEL,
					  USE_REGTYPE_TEMP, 0,
					  USE_REGTYPE_PRIMATTR, 0, 0,
					  USE_REGTYPE_IMMEDIATE, 4, 0,
					  USE_REGTYPE_TEMP, 0, 0);
			pui32Current += USE_INST_LENGTH;
		}

		BuildMOV((PPVR_USE_INST) pui32Current,
				 EURASIA_USE1_EPRED_ALWAYS,
				 0, EURASIA_USE1_RCNTSEL,
				 USE_REGTYPE_IDX, 1, /* 1 for i.l */
				 USE_REGTYPE_TEMP, 0,
				  0 /* Src mod */);
		pui32Current += USE_INST_LENGTH;

		/* 0x60 == BSEL Secondary, IDXSEL Lower, and offset 0
		 * Note the funky ordering we put them into for when we actually use
		 * them
		 */
		BuildMOV((PPVR_USE_INST) pui32Current,
				 EURASIA_USE1_EPRED_ALWAYS,
				 0, EURASIA_USE1_RCNTSEL,
				 USE_REGTYPE_TEMP, 2,
				 USE_REGTYPE_INDEXED, 0x60, 0);
		pui32Current += USE_INST_LENGTH;

		BuildMOV((PPVR_USE_INST) pui32Current,
				 EURASIA_USE1_EPRED_ALWAYS,
				 0, EURASIA_USE1_RCNTSEL,
				 USE_REGTYPE_TEMP, 0,
				 USE_REGTYPE_INDEXED, 0x61, 0);
		pui32Current += USE_INST_LENGTH;

		BuildMOV((PPVR_USE_INST) pui32Current,
				 EURASIA_USE1_EPRED_ALWAYS,
				 0, EURASIA_USE1_RCNTSEL,
				 USE_REGTYPE_TEMP, 3,
				 USE_REGTYPE_INDEXED, 0x62, 0);
		pui32Current += USE_INST_LENGTH;

		BuildMOV((PPVR_USE_INST) pui32Current,
				 EURASIA_USE1_EPRED_ALWAYS,
				 0, EURASIA_USE1_RCNTSEL,
				 USE_REGTYPE_TEMP, 1,
				 USE_REGTYPE_INDEXED, 0x63, 0);
		pui32Current += USE_INST_LENGTH;

		/* Now we can do our first multiply (with SMLSI) */
		BuildSMLSI((PPVR_USE_INST) pui32Current,
				   0, 0, 0, 0, /*< Limits */
				   1, 1, 0, 0); /*< Increments */
		pui32Current += USE_INST_LENGTH;

		BuildMAD((PPVR_USE_INST) pui32Current,
				 EURASIA_USE1_EPRED_ALWAYS,
				 1, EURASIA_USE1_RCNTSEL,
				 USE_REGTYPE_TEMP, 0,
				 USE_REGTYPE_TEMP, 0, 0,
				 USE_REGTYPE_OUTPUT, 3, 0,
				 USE_REGTYPE_SPECIAL,
				 EURASIA_USE_SPECIAL_CONSTANT_ZERO, 0);
		pui32Current += USE_INST_LENGTH;

		/* Reset our SMLSI and do the second MAD */
		BuildSMLSI((PPVR_USE_INST) pui32Current,
				   0, 0, 0, 0, /*< Limits */
				   1, 1, 1, 1); /*< Increments */
		pui32Current += USE_INST_LENGTH;

		BuildMAD((PPVR_USE_INST) pui32Current,
				 EURASIA_USE1_EPRED_ALWAYS,
				 1, EURASIA_USE1_RCNTSEL,
				 USE_REGTYPE_OUTPUT, 0,
				 USE_REGTYPE_TEMP, 2, 0,
				 USE_REGTYPE_OUTPUT, 0, 0,
				 USE_REGTYPE_TEMP, 0, 0);
		pui32Current += USE_INST_LENGTH;
	}

	if (ui32ClipCount)
	{
		IMG_UINT i;

		/*
			Before we can perform the dot-products, the MOE destination increment
			must be set to 0, so that the DPx result is written to the intended
			register.
		*/
		BuildSMLSI((PPVR_USE_INST) pui32Current,
					0, 0, 0, 0, /*< Limits */
					0, 0, 0, 1); /*< Increments */
		pui32Current += USE_INST_LENGTH;
	
		/*
			We need to setup a DP4 instruction to calculate the p-value and p-flag
			for this clip distance. The hardware requires fdpc to have at least
			two iterations. So we generate:
				FDPC.RPT2	PA[i], PA[i], zero6
	
			as the SMLSI will use zero6 (51) then float1 (52)
				
			Which the hardware expands as
				DEST/PFLAG = PA[i] * 0.0f + PA[i] * 1.0f
						= PA[i]
		*/
		USEGEN_ASSERT(ui32ClipCount <= 8);
		for (i = 0; i < ui32ClipCount; i++)
		{
			BuildDPC((PPVR_USE_INST) pui32Current,
					EURASIA_USE1_EPRED_ALWAYS, 1, EURASIA_USE1_RCNTSEL,
					i,
					USE_REGTYPE_TEMP, 1,
					USE_REGTYPE_PRIMATTR, ui32ClipFirstPA + i, EURASIA_USE_SRCMOD_NONE,
					USE_REGTYPE_SPECIAL, EURASIA_USE_SPECIAL_CONSTANT_ZERO6, EURASIA_USE_SRCMOD_NONE);
			pui32Current += USE_INST_LENGTH;
		}

		BuildSMLSI((PPVR_USE_INST) pui32Current,
					0, 0, 0, 0, /*< Limits */
					1, 1, 1, 1); /*< Increments */
		pui32Current += USE_INST_LENGTH;
	}

#if defined(SGX_FEATURE_STREAM_OUTPUT)
	if (psIntRegProg)
	{
		pui32Current = USEGenIntRegStoreFragment(psIntRegProg, pui32Current);
	}
#else
	PVR_UNREFERENCED_PARAMETER(psIntRegProg);
#endif

	/* And the specialised emit */
	BuildEMITMTE((PPVR_USE_INST) pui32Current,
				 EURASIA_USE1_EPRED_ALWAYS,
				 (SGX545_USE1_EMIT_VCB_EMITTYPE_VERTEX <<
				  SGX545_USE1_EMIT_VCB_EMITTYPE_SHIFT) |
				 SGX545_USE1_EMIT_VERTEX_TWOPART |
				 EURASIA_USE1_END,
				 USE_REGTYPE_TEMP,
				 0,
				 USE_REGTYPE_IMMEDIATE,
				 (EURASIA_MTEEMIT1_COMPLEX | EURASIA_MTEEMIT1_COMPLEX_PHASE2),
				 USE_REGTYPE_IMMEDIATE,
				 0,
				 0 /* INCP */,
				 IMG_TRUE /* Free partition */);
	pui32Current += USE_INST_LENGTH;

#endif /* !defined(SGX_FEATURE_VCB) */

#endif /* !defined(SGX545) */

	/* In all cases, return pui32Current */
	return pui32Current;

}

/***********************************************************************//**
 * At the end of a frame using render target arrays, we must write
 * a state program to instruct the HW we're finished.
 *
 * @param pui32Base			The base (linear) address to write the program to
 * @param ui32TAClipWord	The clip word for the render target
 * @param ui32RTNumber		The number of the render target within the RTA
 * @param bFinalRT			Whether this is the final RT in the array, or there
 *							are more state updates to be expected
 *
 * @return Pointer to the end of the written program
 **************************************************************************/
IMG_PUINT32 USEGenWriteRTATerminateStateProgram (IMG_PUINT32 pui32Base,
												 IMG_UINT32 ui32TAClipWord,
												 IMG_UINT32 ui32RTNumber,
												 IMG_BOOL bFinalRT)
{
#if !defined(SGX_FEATURE_RENDER_TARGET_ARRAYS)
	USEGEN_DPF("Chip type does not support Render Target Arrays");
	PVR_UNREFERENCED_PARAMETER(pui32Base);
	PVR_UNREFERENCED_PARAMETER(ui32TAClipWord);
	PVR_UNREFERENCED_PARAMETER(ui32RTNumber);
	PVR_UNREFERENCED_PARAMETER(bFinalRT);
	return pui32Base;
#else
	IMG_PUINT32 pui32Current = pui32Base;
	IMG_UINT32 ui32Value;

	USEGEN_ASSERT (pui32Current);

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	/* We're going to assume this is called as the final phase, so we only
	 * need to write a LASTPHASE */
	pui32Current = USEGenWritePhaseFragment(pui32Current, 0, 0,
											USEGEN_NO_DEPENDANT,
											IMG_FALSE, IMG_FALSE,
											IMG_TRUE, IMG_FALSE);
#endif

	/* First word is hard coded */
	ui32Value = EURASIA_TACTLPRES_TERMINATE | EURASIA_TACTLPRES_RENDERTGT;
	if (!bFinalRT)
	{
		ui32Value |= EURASIA_TACTLPRES_NOT_FINAL_TERM;
	}

	BuildLIMM ((PPVR_USE_INST) pui32Current,
			   EURASIA_USE1_EPRED_ALWAYS,
			   EURASIA_USE1_SKIPINV,
			   USE_REGTYPE_OUTPUT,
			   0,
			   ui32Value);
	pui32Current += USE_INST_LENGTH;

	/* Second word specifies the clip value */
	BuildLIMM ((PPVR_USE_INST) pui32Current,
			   EURASIA_USE1_EPRED_ALWAYS,
			   EURASIA_USE1_SKIPINV,
			   USE_REGTYPE_OUTPUT,
			   1,
			   ui32TAClipWord);
	pui32Current += USE_INST_LENGTH;

	/* Third word is the RT ID */
	BuildLIMM ((PPVR_USE_INST) pui32Current,
			   EURASIA_USE1_EPRED_ALWAYS,
			   EURASIA_USE1_SKIPINV,
			   USE_REGTYPE_OUTPUT,
			   2,
			   ui32RTNumber);
	pui32Current += USE_INST_LENGTH;

	/* Now do the emit(s) */

#if defined(SGX_FEATURE_VCB)
	BuildEMITVCB ((PPVR_USE_INST) pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS,
				  (SGX545_USE1_EMIT_VCB_EMITTYPE_STATE <<
				   SGX545_USE1_EMIT_VCB_EMITTYPE_SHIFT),
				  USE_REGTYPE_TEMP,
				  0,
				  USE_REGTYPE_IMMEDIATE,
				  0,
				  USE_REGTYPE_IMMEDIATE,
				  0,
				  0 /* INCP */,
				  IMG_FALSE /* Don't free the partition yet */);
	pui32Current += USE_INST_LENGTH;

	/* Always want to WOP to ensure we don't free the partition prematurely */
	BuildWOP((PPVR_USE_INST)pui32Current, 0, 0);
	pui32Current += USE_INST_LENGTH;

#endif

	BuildEMITMTE((PPVR_USE_INST)pui32Current,
				 EURASIA_USE1_EPRED_ALWAYS,
				 (SGX545_USE1_EMIT_VCB_EMITTYPE_STATE <<
				  SGX545_USE1_EMIT_VCB_EMITTYPE_SHIFT) |
				 EURASIA_USE1_END,
				 USE_REGTYPE_OUTPUT,
				 0,
				 USE_REGTYPE_IMMEDIATE,
				 0,
				 USE_REGTYPE_IMMEDIATE,
				 0,
				 0, /* INCP */
				 IMG_TRUE /* Free partition */);
	pui32Current += USE_INST_LENGTH;

	return pui32Current;
#endif
}

/***********************************************************************************
 Function Name      : FloorLog2
 Inputs             : ui32Val
 Outputs            : 
 Returns            : Floor(Log2(ui32Val))
 Description        : Computes the floor of the log base 2 of a unsigned integer - 
					  used mostly for computing log2(2^ui32Val).
***********************************************************************************/
IMG_UINT32 FloorLog2(IMG_UINT32 ui32Val)
{
	IMG_UINT32 ui32Ret = 0;

	while (ui32Val >>= 1)
	{
		ui32Ret++;
	}

	return ui32Ret;
}

#if defined (SGX_FEATURE_HYBRID_TWIDDLING)
/***********************************************************************//**
 * Generates code to add the twiddled offset of the start of the current tile
 * onto a secondary attribute register.
 *
 * @param psUSEInst					Array to generate the code in.
 * @param ui32TileSize				Attribute register with the stride of the 
 *									surface.
 * @param ui32ChunkByteSize			Size of a surface chunk.
 * @param i32Stride					Stride of the surface (in pixels).
 * @param ui32SurfaceAddrSAReg		Attribute register to calculate.
 *
 * @return	The next entry in the code array.
 **************************************************************************/
 IMG_PUINT32 USEGenWriteHybridTwiddleAddress(IMG_PUINT32	psUSEInst,
											 IMG_UINT32		ui32TileSize,
											 IMG_UINT32		ui32ChunkByteSize,
	 										 IMG_INT32		i32Stride,
											 IMG_UINT32		ui32SurfaceAddrSAReg)
 {
 	IMG_PUINT32 pui32Current = psUSEInst;

	 IMG_UINT32	i;
	 IMG_UINT32	ui32ChunkByteSizeLog2;
	 IMG_UINT32	ui32TileSizeLog2;

	 /*
	  * Hybrid twiddling:
	  *   Given a tile dimension of 2^t, the pixel address is 
	  * 		(x[..t] + y[..t] * stride[..t]) << 2t + twiddle(x[t-1..0], y[t-1..0])
	  */

	 /*
	  * Get the number of bits in the pixel size.
	  */
	 switch (ui32ChunkByteSize)
	 {
	 case 16:	ui32ChunkByteSizeLog2 = 4; break;
	 case 8:	ui32ChunkByteSizeLog2 = 3; break;
	 case 4:	ui32ChunkByteSizeLog2 = 2; break;
	 case 2:	ui32ChunkByteSizeLog2 = 1; break;
	 case 1:	ui32ChunkByteSizeLog2 = 0; break;
	 default:
		 {
			USEGEN_DPF("Warning: Number of bits in the pixel size not supported for hybrid twiddling.");
			USEGEN_DBG_BREAK;
			ui32ChunkByteSizeLog2 = (IMG_UINT32)-1;
		 }
	 }

	 /*
	  * Get the number of bits in the tile size.
	  */
	 switch (ui32TileSize)
	 {
	 case 16:	ui32TileSizeLog2 = 4; break;
	 case 8:	ui32TileSizeLog2 = 3; break;
	 case 4:	ui32TileSizeLog2 = 2; break;
	 case 2:	ui32TileSizeLog2 = 1; break;
	 case 1:	ui32TileSizeLog2 = 0; break;
	 default:
		 {
			 USEGEN_DPF("Warning: Number of bits in the tile size not supported for hybrid twiddling.");
			 USEGEN_DBG_BREAK;
			 ui32TileSizeLog2 = (IMG_UINT32)-1;
		 }
	 }

	 /*
	  * As set up by the previous code temp1 contains
	  * 	(y[..4] << 17) | (x[..4] << 0)
	  */
	 if (ui32TileSize != 16)
	 {
		 /*
		  * Convert temp1 to
		  * 	(y[..3] << 16) | (x[..4] << 0)
		  */

		 /*
		  * Extract the bottom bit of the pipe number (effectively the bottom bit
		  * of the y address of the y address of the region covered by this pipe.
		  */
		  BuildAND((PPVR_USE_INST)pui32Current,
			  	   EURASIA_USE1_EPRED_ALWAYS,
				   0,
				   EURASIA_USE1_RCNTSEL,
				   USE_REGTYPE_TEMP,
				   2,
				   USE_REGTYPE_SPECIAL,
				   EURASIA_USE_SPECIAL_BANK_PIPENUMBER | EURASIA_USE_SPECIAL_INTERNALDATA,
				   USE_REGTYPE_IMMEDIATE,
				   EURASIA_PERISP_NUM_USE_PIPES_IN_Y - 1);
		  pui32Current += USE_INST_LENGTH;

		  /*
		   * Shift left to align with the location of the y coordinate in temp1.
		   */
		  BuildSHL((PPVR_USE_INST)pui32Current,
		  		   EURASIA_USE1_EPRED_ALWAYS,
				   0,
				   EURASIA_USE1_RCNTSEL,
				   USE_REGTYPE_TEMP,
				   2,
				   USE_REGTYPE_TEMP,
				   2,
				   USE_REGTYPE_IMMEDIATE,
				   16);
		  pui32Current += USE_INST_LENGTH;

		  /*
		   * Combine with the y coordinate in temp1.
		   */
		  BuildOR((PPVR_USE_INST)pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS,
				  0,
				  EURASIA_USE1_RCNTSEL,
				  USE_REGTYPE_TEMP,
				  0,
				  USE_REGTYPE_TEMP,
				  0,
				  USE_REGTYPE_TEMP,
				  2);
		  pui32Current += USE_INST_LENGTH;
	 }

	 for (i = 0; i < 2; i++)
	 {
		 IMG_UINT32		ui32ScaleFactor;
		 IMG_UINT32		ui32ScaleFactorRegNum;
		 USE_REGTYPE	eScaleFactorRegType;

		 if (i == 0)
		 {
			 ui32ScaleFactor = ui32TileSize * ui32TileSize * ui32ChunkByteSize;
			 ui32ScaleFactor <<= (4 - ui32TileSizeLog2);
		 }
		 else
		 {
			 ui32ScaleFactor = ui32TileSize * i32Stride * ui32ChunkByteSize;
			 if (ui32TileSizeLog2 == 4)
			 {
				 ui32ScaleFactor >>= 1;
			 }
			 else
			 {
				 ui32ScaleFactor <<= (3 - ui32TileSizeLog2);
			 }
		 }

		 if (ui32ScaleFactor > EURASIA_USE_MAXIMUM_IMMEDIATE)
		 {
			 /*
			  * Load the scale factor into a temporary register.
			  */
			 eScaleFactorRegType = USE_REGTYPE_TEMP;
			 ui32ScaleFactorRegNum = 1;

			 BuildLIMM((PPVR_USE_INST)pui32Current,
				 	  EURASIA_USE1_EPRED_ALWAYS,
					  EURASIA_USE1_SKIPINV,
					  eScaleFactorRegType,
					  ui32ScaleFactorRegNum,
					  ui32ScaleFactor);
			 pui32Current += USE_INST_LENGTH;
		 }
		 else
		 {
			 /*
			  * Use the scale factor directly.
			  */
			  eScaleFactorRegType = USE_REGTYPE_IMMEDIATE;
			  ui32ScaleFactorRegNum = ui32ScaleFactor;
		 }

		 /*
		  * Multiply the coordinate by the scale factor and add onto the surface base address.
		  */
		  BuildIMAE((PPVR_USE_INST)pui32Current,
			  		EURASIA_USE1_EPRED_ALWAYS,
					0,
					EURASIA_USE1_SKIPINV | EURASIA_USE1_IMAE_SATURATE |
					((i==1) ? EURASIA_USE1_IMAE_SRC0H_SELECTHIGH : EURASIA_USE1_IMAE_SRC0H_SELECTLOW) |
					EURASIA_USE1_IMAE_SRC1H_SELECTLOW |
					(EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT),
					USE_REGTYPE_SECATTR,
					ui32SurfaceAddrSAReg,
					USE_REGTYPE_TEMP,
					0,
					EURASIA_USE_SRCMOD_NONE,
					eScaleFactorRegType,
					ui32ScaleFactorRegNum,
					EURASIA_USE_SRCMOD_NONE,
					USE_REGTYPE_SECATTR,
					ui32SurfaceAddrSAReg,
					EURASIA_USE_SRCMOD_NONE);
		  pui32Current += USE_INST_LENGTH;
	 }

	 if (ui32TileSize == 16)
	 {
		 /*
		  * Extract the bottom bit of the pipe number (effectively the bottom bit
		  * of the y address of the region covered by this pipe).
		  */
		 BuildAND((PPVR_USE_INST)pui32Current,
			 	  EURASIA_USE1_EPRED_ALWAYS,
				  0,
				  EURASIA_USE1_RCNTSEL,
				  USE_REGTYPE_TEMP,
				  2,
				  USE_REGTYPE_SPECIAL,
				  EURASIA_USE_SPECIAL_BANK_PIPENUMBER | EURASIA_USE_SPECIAL_INTERNALDATA,
				  USE_REGTYPE_IMMEDIATE,
				  EURASIA_PERISP_NUM_USE_PIPES_IN_Y - 1);
		 pui32Current += USE_INST_LENGTH;

		 /*
		  * Shift the y bit address to the right in a twiddled address.
		  */
		 BuildSHL((PPVR_USE_INST)pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS,
				  0,
				  EURASIA_USE1_RCNTSEL,
				  USE_REGTYPE_TEMP,
				  2,
				  USE_REGTYPE_TEMP,
				  2,
				  USE_REGTYPE_IMMEDIATE,
				  ui32ChunkByteSizeLog2 + 6);
		 pui32Current += USE_INST_LENGTH;

		 /*
		  * Add the y coordinate of the pipe onto the base address.
		  */
		  BuildIMAE((PPVR_USE_INST)pui32Current,
			  		EURASIA_USE1_EPRED_ALWAYS,
					0,
					EURASIA_USE1_SKIPINV | EURASIA_USE1_IMAE_SATURATE |
					EURASIA_USE1_IMAE_SRC0H_SELECTLOW | EURASIA_USE1_IMAE_SRC1H_SELECTLOW |
					(EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT),
					USE_REGTYPE_SECATTR,
					ui32SurfaceAddrSAReg,
					USE_REGTYPE_TEMP,
					2,
					EURASIA_USE_SRCMOD_NONE,
					USE_REGTYPE_IMMEDIATE,
					1,
					EURASIA_USE_SRCMOD_NONE,
					USE_REGTYPE_SECATTR,
					ui32SurfaceAddrSAReg,
					EURASIA_USE_SRCMOD_NONE);
		  pui32Current += USE_INST_LENGTH;
	 }

	 return pui32Current;
 }

#else  /* defined (SGX_FEATURE_HYBRID_TWIDDLING) */
/***********************************************************************//**
 * Generates code to add the twiddled offset of the start of the current tile
 * onto a secondary attribute register.
 *
 * @param psUSEInst					Array to generate the code in.
 * @param ui32Width					Dimensions of the surface.
 * @param ui32Height
 * @param ui32SurfaceAddrSAReg		Attribute register to calculate.
 * @param ui32SurfaceStrideSAReg	Attribute register with the stride of the 
 *									surface.
 * @param ui32TTAddr				Device virtual address of the twiddle table. 
 * @param bWriteSetup				On the first call to this function, we need
 * 									to set up some initial gumpf.  Subsequent calls
 *									should set this to false.
 *
 * @return	The next entry in the code array.
 **************************************************************************/
IMG_PUINT32 USEGenWriteTwiddleAddress(IMG_PUINT32	psUSEInst,
						   			  IMG_UINT32	ui32Width,
								 	  IMG_UINT32	ui32Height,
									  IMG_UINT32	ui32SurfaceAddrSAReg,
									  IMG_UINT32	ui32SurfaceStrideSAReg,
									  IMG_UINT32	ui32TTAddr,
									  IMG_BOOL		bWriteSetup)
{
	IMG_PUINT32	pui32Current = psUSEInst;

	if(bWriteSetup)
	{
		IMG_UINT32	i;
		IMG_UINT32	ui32WidthLog2;
		IMG_UINT32	ui32HeightLog2;
		IMG_UINT32	aui32TwiddledXY[2];
		IMG_UINT32	ui32TwiddleTableDevAddr = ui32TTAddr + USEGEN_TWIDDLE_TABLE_SIZE1 - 2;

		IMG_BOOL	bRectangular = (ui32Width != ui32Height) ? IMG_TRUE : IMG_FALSE;

		ui32WidthLog2 = FloorLog2(ui32Width);
		ui32HeightLog2 = FloorLog2(ui32Height);

		/*
		 * Check for rectangular surface.
		 */
		if (bRectangular)
		{
			IMG_INT32	i32Shift;
			IMG_UINT32	ui32Mask;

			ui32WidthLog2 -= EURASIA_ISPREGION_SHIFTX;
			ui32HeightLog2 -= EURASIA_ISPREGION_SHIFTY;

			if (ui32WidthLog2 < ui32HeightLog2)
			{
				i32Shift = 16 + ui32WidthLog2 + 1 - 2 * ui32WidthLog2;
				ui32Mask = ((1 << ui32WidthLog2) - 1) << 1;
			}
			else
			{
				i32Shift = ui32HeightLog2 + 1 - 2 * ui32HeightLog2;
				ui32Mask = ((1 << ui32HeightLog2) - 1) << 1;
			}

			/*
			 * Mask the portion of the largest coordinate that doesn't need to be twiddled.
			 */
			BuildANDIMM((PPVR_USE_INST)pui32Current,
						EURASIA_USE1_EPRED_ALWAYS,
						0,
						EURASIA_USE1_RCNTSEL | EURASIA_USE1_SKIPINV | EURASIA_USE1_BITWISE_SRC2INV,
						USE_REGTYPE_TEMP,
						1,
						USE_REGTYPE_TEMP,
						0,
						ui32Mask,
						0);

			if (ui32WidthLog2 < ui32HeightLog2)
			{
				pui32Current[1] |= (16 << EURASIA_USE1_BITWISE_SRC2ROT_SHIFT);
			}
			pui32Current += USE_INST_LENGTH;

			/*
			 * Mask the portion of the largest coordinate that is to be twiddled. Notice the
			 * only difference between this instruction and the previous one is the invert flag
			 * on source 2.
			 */
			BuildANDIMM((PPVR_USE_INST)pui32Current,
						EURASIA_USE1_EPRED_ALWAYS,
						0,
						EURASIA_USE1_RCNTSEL | EURASIA_USE1_SKIPINV,
						USE_REGTYPE_TEMP,
						2,
						USE_REGTYPE_TEMP,
						0,
						ui32Mask,
						0);

			if (ui32WidthLog2 < ui32HeightLog2)
			{
				pui32Current[1] |= (16 << EURASIA_USE1_BITWISE_SRC2ROT_SHIFT);
			}
			pui32Current += USE_INST_LENGTH;

			/*
			 * Shift the untwiddled part of the largest coordinate to the bit position 
			 * after the twiddled part of both coordiantes.
			 */
			if (i32Shift > 0)
			{
				BuildSHRIMM((PPVR_USE_INST)pui32Current,
							EURASIA_USE1_EPRED_ALWAYS,
							0,
							EURASIA_USE1_RCNTSEL | EURASIA_USE1_SKIPINV,
							USE_REGTYPE_TEMP,
							1,
							USE_REGTYPE_TEMP,
							1,
							i32Shift,
							0);
			}
			else
			{
				BuildSHLIMM((PPVR_USE_INST)pui32Current,
							EURASIA_USE1_EPRED_ALWAYS,
							0,
							EURASIA_USE1_RCNTSEL | EURASIA_USE1_SKIPINV,
							USE_REGTYPE_TEMP,
							1,
							USE_REGTYPE_TEMP,
							1,
							(-i32Shift),
							0);
			}
			pui32Current += USE_INST_LENGTH;
		}

		/*
		 * Load the address of the twiddled address table into a temporary register.
		 */
		BuildLIMM((PPVR_USE_INST)pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS,
				  EURASIA_USE1_SKIPINV,
				  USE_REGTYPE_TEMP,
				  3,
				  ui32TwiddleTableDevAddr);
		pui32Current += USE_INST_LENGTH;



		for (i = 0; i < 2; i++)
		{
			IMG_UINT32	ui32TempAddressIdx, ui32Src0TempIdx;

			/*
			 * Calculate the offsets into the table. Notice we don't scale the offset even though
			 * we are looking up in an array of uint16 since the hardware shifts the tile
			 * addresses it gives us by 1.
			 */
			if (ui32WidthLog2 < ui32HeightLog2)
			{
				ui32TempAddressIdx = (i == 0) ? 0 : 2;
				ui32Src0TempIdx = (i == 0) ? 0 : 2;
			}
			else if (ui32WidthLog2 > ui32HeightLog2)
			{
				ui32TempAddressIdx = (i == 0) ? 2 : 3;
				ui32Src0TempIdx = (i == 0) ? 2 : 0;
			}
			else	/* ui32WidthLog2 == ui32HeightLog2 */
			{
				ui32TempAddressIdx = (i == 0) ? 2 : 3;
				ui32Src0TempIdx = 0;
			}

			BuildIMAE((PPVR_USE_INST)pui32Current,
					  EURASIA_USE1_EPRED_ALWAYS,
					  0,
					  EURASIA_USE1_SKIPINV |
					  EURASIA_USE1_IMAE_SATURATE |
					  ((i == 1) ? EURASIA_USE1_IMAE_SRC0H_SELECTHIGH : EURASIA_USE1_IMAE_SRC0H_SELECTLOW) |
					  (EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT),
					  USE_REGTYPE_TEMP,
					  ui32TempAddressIdx,
					  USE_REGTYPE_TEMP,
					  ui32Src0TempIdx,
					  EURASIA_USE_SRCMOD_NONE,
					  USE_REGTYPE_IMMEDIATE,
					  1,
					  EURASIA_USE_SRCMOD_NONE,
					  USE_REGTYPE_TEMP,
					  3,
					  EURASIA_USE_SRCMOD_NONE);
			pui32Current += USE_INST_LENGTH;

			/*
			 * Do the lookup into the table
			 */
			aui32TwiddledXY[i] = ui32TempAddressIdx;

			BuildLD((PPVR_USE_INST)pui32Current,
					EURASIA_USE1_EPRED_ALWAYS,
					0,
					EURASIA_USE1_SKIPINV,
					0,
					EURASIA_USE1_LDST_DTYPE_16BIT,
					EURASIA_USE1_LDST_AMODE_ABSOLUTE,
					EURASIA_USE1_LDST_IMODE_NONE,
					USE_REGTYPE_TEMP,
					ui32TempAddressIdx,
					USE_REGTYPE_TEMP,
					ui32TempAddressIdx,
					USE_REGTYPE_IMMEDIATE,
					0,
					USE_REGTYPE_IMMEDIATE,
					0,
					IMG_FALSE);
			pui32Current += USE_INST_LENGTH;
		}

		/*
		 * Wait for the twiddled results to return from memory.
		 */
		BuildWDF((PPVR_USE_INST)pui32Current, 0);
		pui32Current += USE_INST_LENGTH;

#if EURASIA_PERISP_TILE_SHIFTX == EURASIA_PERISP_TILE_SHIFTY
		/*
		 * Shift the x address left by one and or together with the y address.
		 */
		BuildIMAE((PPVR_USE_INST)pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS,
				  0,
				  EURASIA_USE1_SKIPINV |
				  EURASIA_USE1_IMAE_SATURATE |
				  (EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT),
				  USE_REGTYPE_TEMP,
				  2,
				  USE_REGTYPE_TEMP,
				  aui32TwiddledXY[0],
				  EURASIA_USE_SRCMOD_NONE,
				  USE_REGTYPE_IMMEDIATE,
				  2,
				  EURASIA_USE_SRCMOD_NONE,
				  USE_REGTYPE_TEMP,
				  aui32TwiddledXY[1],
				  EURASIA_USE_SRCMOD_NONE);
		pui32Current += USE_INST_LENGTH;
#elif (EURASIA_PERISP_TILE_SHIFTX +1) == EURASIA_PERISP_TILE_SHIFTY
		/*
		 * Shift the y address left by one and or together with the x address
		 */
		BuildIMAE((PPVR_USE_INST)pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS,
				  0,
				  EURASIA_USE1_SKIPINV |
				  EURASIA_USE1_IMAE_SATURATE |
				  (EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT),
				  USE_REGTYPE_TEMP,
				  2,
				  USE_REGTYPE_TEMP,
				  aui32TwiddledXY[1],
				  EURASIA_USE_SRCMOD_NONE,
				  USE_REGTYPE_IMMEDIATE,
				  2,
				  EURASIA_USE_SRCMOD_NONE,
				  USE_REGTYPE_TEMP,
				  aui32TwiddledXY[0],
				  EURASIA_USE_SRCMOD_NONE);
		pui32Current += USE_INST_LENGTH;
#else
		USEGEN_DPF("This tile size isn't supported for twiddled MRTs")
#endif

		/* 
		 * Combine the untwiddled part of the address if it exists.
		 */
		 if (bRectangular)
		 {
			 BuildOR((PPVR_USE_INST)pui32Current,
					 EURASIA_USE1_EPRED_ALWAYS,
					 0,
					 EURASIA_USE1_RCNTSEL | EURASIA_USE1_SKIPINV,
					 USE_REGTYPE_TEMP,
					 2,
					 USE_REGTYPE_TEMP,
					 2,
					 USE_REGTYPE_TEMP,
					 1);
			 pui32Current += USE_INST_LENGTH;
		 }

#if EURASIA_PERISP_NUM_USE_PIPES_IN_X == 2 && EURASIA_PERISP_NUM_USE_PIPES_IN_Y == 1
		 /*
		  * Shift the address left by one and or in the top bit of the x coordinate
		  * within the tile.
		  */
		 BuildIMAE((PPVR_USE_INST)pui32Current,
				   EURASIA_USE1_EPRED_ALWAYS,
				   0,
				   EURASIA_USE1_SKIPINV |
				   EURASIA_USE1_IMAE_SATURATE |
				   (EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT),
				   USE_REGTYPE_TEMP,
				   2,
				   USE_REGTYPE_TEMP,
				   2,
				   EURASIA_USE_SRCMOD_NONE,
				   USE_REGTYPE_IMMEDIATE,
				   2,
				   EURASIA_USE_SRCMOD_NONE,
				   USE_REGTYPE_SPECIAL,
				   (EURASIA_USE_SPECIAL_BANK_PIPENUMBER | EURASIA_USE_SPECIAL_INTERNALDATA),
				   EURASIA_USE_SRCMOD_NONE);
		 pui32Current += USE_INST_LENGTH;
#elif EURASIA_PERISP_NUM_USE_PIPES_IN_X == 1 && EURASIA_PERISP_NUM_USE_PIPES_IN_Y == 2
		 /*
		  * Extract the bottom bit of the pipe number (effectively the top bit
		  * of the y coordinate within the per-ISP tile).
		  */
		 BuildAND((PPVR_USE_INST)pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS,
				  0,
				  EURASIA_USE1_RCNTSEL,
				  USE_REGTYPE_TEMP,
				  1,
				  USE_REGTYPE_SPECIAL,
				  EURASIA_USE_SPECIAL_BANK_PIPENUMBER | EURASIA_USE_SPECIAL_INTERNALDATA,
				  USE_REGTYPE_IMMEDIATE,
				  1);
		 pui32Current += USE_INST_LENGTH;

		 /*
		  * Shift the address left by two and or the top bit of the y coordinate
		  * within the tile.
		  */
		 BuildIMAE((PPVR_USE_INST)pui32Current,
				   EURASIA_USE1_EPRED_ALWAYS,
				   0,
				   EURASIA_USE1_SKIPINV | 
				   EURASIA_USE1_IMAE_SATURATE |
				   (EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT),
				   USE_REGTYPE_TEMP,
				   2,
				   USE_REGTYPE_TEMP,
				   2,
				   EURASIA_USE_SRCMOD_NONE,
				   USE_REGTYPE_IMMEDIATE,
				   4,
				   EURASIA_USE_SRCMOD_NONE,
				   USE_REGTYPE_TEMP,
				   1,
				   EURASIA_USE_SRCMOD_NONE);
		 pui32Current += USE_INST_LENGTH;
#elif EURASIA_PERISP_NUM_USE_PIPES_IN_X == 2 && EURASIA_PERISP_NUM_USE_PIPES_IN_Y ==2 && defined (EURASIA_USE_PIPES_ORIENTATION_XY)
		 /*
		  * Extract the bottom bit of the pipe number (effectively the top bit
		  * of the x coordinate within the per-ISP tile.
		  */
		 BuildAND((PPVR_USE_INST)pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS,
				  0,
				  EURASIA_USE1_RCNTSEL,
				  USE_REGTYPE_TEMP,
				  1,
				  USE_REGTYPE_SPECIAL,
				  EURASIA_USE_SPECIAL_BANK_PIPENUMBER | EURASIA_USE_SPECIAL_INTERNALDATA,
				  USE_REGTYPE_IMMEDIATE,
				  1);
		 pui32Current += USE_INST_LENGTH;

		 /*
		  * Shift the address left by one and or in the top bit of the x coordinate 
		  * within the tile.
		  */
		 BuildIMAE((PPVR_USE_INST)pui32Current,
				   EURASIA_USE1_EPRED_ALWAYS,
				   0,
				   EURASIA_USE1_SKIPINV |
				   EURASIA_USE1_IMAE_SATURATE |
				   (EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT),
				   USE_REGTYPE_TEMP,
				   2,
				   USE_REGTYPE_TEMP,
				   2,
				   EURASIA_USE_SRCMOD_NONE,
				   USE_REGTYPE_IMMEDIATE,
				   2,
				   EURASIA_USE_SRCMOD_NONE,
				   USE_REGTYPE_TEMP,
				   1,
				   EURASIA_USE_SRCMOD_NONE);
		 pui32Current += USE_INST_LENGTH;

		 /*
		  * Extract the second bit of the pipe number (effctively the top bit
		  * of the y coordinate within the per-ISP tile).
		  */
		 BuildSHR((PPVR_USE_INST)pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS,
				  0,
				  EURASIA_USE1_RCNTSEL,
				  USE_REGTYPE_TEMP,
				  1,
				  USE_REGTYPE_SPECIAL,
				  (EURASIA_USE_SPECIAL_BANK_PIPENUMBER | EURASIA_USE_SPECIAL_INTERNALDATA),
				  USE_REGTYPE_IMMEDIATE,
				  1);
		 pui32Current += USE_INST_LENGTH;

		 /*
		  * Shift the address left by one and or in the top bit of the y 
		  * coordinate within the tile.
		  */
		 BuildIMAE((PPVR_USE_INST)pui32Current,
				   EURASIA_USE1_EPRED_ALWAYS,
				   0,
				   EURASIA_USE1_SKIPINV |
				   EURASIA_USE1_IMAE_SATURATE |
				   (EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT),
				   USE_REGTYPE_TEMP,
				   2,
				   USE_REGTYPE_TEMP,
				   2,
				   EURASIA_USE_SRCMOD_NONE,
				   USE_REGTYPE_IMMEDIATE,
				   2,
				   EURASIA_USE_SRCMOD_NONE,
				   USE_REGTYPE_TEMP,
				   1,
				   EURASIA_USE_SRCMOD_NONE);
		 pui32Current += USE_INST_LENGTH;
#elif EURASIA_PERISP_NUM_USE_PIPES_IN_X == 1 && EURASIA_PERISP_NUM_USE_PIPES_IN_Y == 1
		 /*
		  * Nothing to do since there is one USE per tile
		  */
#else
		USEGEN_DPF("This pipeline configuration isn't supported for twiddled MRTs")
#endif

	} /* bWriteSetup */

	/*
	 * Shift the address by the number of bits in the tile relative part 
	 * of the address and add onto the surface base address.
	 */
	 BuildIMAE((PPVR_USE_INST)pui32Current,
			   EURASIA_USE1_EPRED_ALWAYS,
			   0,
			   EURASIA_USE1_SKIPINV |
			   EURASIA_USE1_IMAE_SATURATE |
			   (EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT),
			   USE_REGTYPE_SECATTR,
			   ui32SurfaceAddrSAReg,
			   USE_REGTYPE_TEMP,
			   2,
			   EURASIA_USE_SRCMOD_NONE,
			   USE_REGTYPE_SECATTR,
			   ui32SurfaceStrideSAReg,
			   EURASIA_USE_SRCMOD_NONE,
			   USE_REGTYPE_SECATTR,
			   ui32SurfaceAddrSAReg,
			   EURASIA_USE_SRCMOD_NONE);
			   
	pui32Current += USE_INST_LENGTH;

	return pui32Current;
}
#endif /* defined (SGX_FEATURE_HYBRID_TWIDDLING) */

/***********************************************************************//**
* Convert a given "tile" word (iterated by the secondary PDS pixel program)
* into pixel offset for writing / reading directly from memory
*
* This is useful for multiple render targets, where the render target must
* be written directly to memory, instead of through the PBE
*
* When writing for an RTA, the bEnableRTAs flag must be set and the stride
* in memory between the RTA elements must be given.
*
* Each RTA element will have it's own 3D phase, so here we can read the
* current RTA element from register EUR_CR_DPM_3D_RENDER_TARGET_INDEX
* (0x073C) and multiply the value by the stride to get the corrected
* address.
*
* @param pui32Base			The base (linear) address to write the program to
* @param ui32TileSA			The secondary attribute containing the tile word
* @param ui32AddressSA		The secondary attribute containing the DevVAddr
* 							of the base of the surface.  This will be updated
*							to the correct tile address for doing a ST to
* @param ui32StrideSA		The SA register containing the "stride" word.
*							If the target is twiddled, this is has the format:
*							StrideSA = (pixelstride in bytes) << USEGEN_TWIDDLED_ADDRESS_TILE_SHIFT
*							Else, this is has the format:
*							StrideSA = ((linestride in bytes) << 16) | (pixelstride in bytes)
* @param bTwiddled			Whether the target is twiddled
* @param ui32TTAddr			Device virtual address of the twiddle table
*
* @param ui32TileSize		Size of a tile.
* @param ui32ChunkByteSize	Size of a surface chunk.
* @param i32Stride			Stride of the surface (in pixel).
*
* @param bWriteSetup		On the first call to this function, we need
* 							to set up some initial gumpf.  Subsequent calls
*							should set this to false.
* @param bEnableRTAs		Enable render target array support
* @param ui32ArrayStride	Stride of one element of the RTA in bytes
*
* @return Pointer to the end of the written program
*
* @note This will use the first 3 temps (first 4 if the target is twiddled)
* @note this should only be called for writing to the secondary update task
**************************************************************************/
IMG_PUINT32 USEGenWritePixPosFromTileSecondaryFragment(IMG_PUINT32	pui32Base,
													   IMG_UINT32	ui32TileSA,
													   IMG_UINT32	ui32AddressSA,
													   IMG_UINT32	ui32StrideSA,
													   IMG_BOOL		bTwiddled,
													   IMG_UINT32	ui32TTAddr,
													   IMG_UINT32	ui32TileSize,
													   IMG_UINT32	ui32ChunkByteSize,
													   IMG_INT32	i32Stride,
													   IMG_UINT32	ui32Width,
													   IMG_UINT32	ui32Height,
													   IMG_BOOL		bWriteSetup,
													   IMG_BOOL		bEnableRTAs,
													   IMG_UINT32	ui32ArrayStride)
{
	IMG_PUINT32 pui32Current = pui32Base;

	if (bWriteSetup)
	{
#if EURASIA_PDS_IR0_PDM_TILEY_SHIFT == 9
		IMG_UINT32 ui32Shift;
#endif

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
		/* We're going to assume this is called as the final phase, so we only
		 * need to write a LASTPHASE */
		pui32Current = USEGenWritePhaseFragment(pui32Current, 0, 0,
												USEGEN_NO_DEPENDANT,
												IMG_FALSE, IMG_FALSE,
												IMG_TRUE, IMG_FALSE);
#endif

#if EURASIA_PDS_IR0_PDM_TILEY_SHIFT == 9

		/* Extract the tile X coordinate into TEMP1	*/
		BuildANDIMM((PPVR_USE_INST)pui32Current,
					EURASIA_USE1_EPRED_ALWAYS,
					0,
					EURASIA_USE1_RCNTSEL,
					USE_REGTYPE_TEMP,
					0,
					USE_REGTYPE_SECATTR,
					ui32TileSA,
					~EURASIA_PDS_IR0_PDM_TILEX_CLRMSK,
					0);
		pui32Current += USE_INST_LENGTH;

		if (!bTwiddled)
		{
			/* Convert the X coordinate into pixel units */

#if defined(SGX545)
			ui32Shift = EURASIA_PERISP_TILE_SHIFTX;
#else
			ui32Shift = EURASIA_PERISP_TILE_SHIFTX - 1;
#endif

			BuildSHL ((PPVR_USE_INST) pui32Current,
					  EURASIA_USE1_EPRED_ALWAYS,
					  0,
					  EURASIA_USE1_RCNTSEL,
					  USE_REGTYPE_TEMP,
					  0,
					  USE_REGTYPE_TEMP,
					  0,
					  USE_REGTYPE_IMMEDIATE,
					  ui32Shift);
			pui32Current += USE_INST_LENGTH;
		}

		/* Extract the tile Y coordinate into TEMP2 */
		BuildANDIMM((PPVR_USE_INST)pui32Current,
					EURASIA_USE1_EPRED_ALWAYS,
					0,
					EURASIA_USE1_RCNTSEL | EURASIA_USE1_BITWISE_SRC2INV,
					USE_REGTYPE_TEMP,
					1,
					USE_REGTYPE_SECATTR,
					ui32TileSA,
					~EURASIA_PDS_IR0_PDM_TILEX_CLRMSK,
					0);
		pui32Current += USE_INST_LENGTH;

		if (bTwiddled)
		{
			ui32Shift = 16 - EURASIA_PDS_IR0_PDM_TILEY_SHIFT;
		}
		else
		{
			ui32Shift = 16 - EURASIA_PDS_IR0_PDM_TILEY_SHIFT + EURASIA_PERISP_TILE_SHIFTY - 1;
		}

		/* Convert the tile Y into pixel units */
		BuildSHL ((PPVR_USE_INST) pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS,
				  0,
				  EURASIA_USE1_RCNTSEL,
				  USE_REGTYPE_TEMP,
				  1,
				  USE_REGTYPE_TEMP,
				  1,
				  USE_REGTYPE_IMMEDIATE,
				  ui32Shift);
		pui32Current += USE_INST_LENGTH;

		if (!bTwiddled)
		{
#if EURASIA_PERISP_NUM_USE_PIPES_IN_X == 1 && EURASIA_PERISP_NUM_USE_PIPES_IN_Y == 1
			/* Don't need to do anything since there is only one pipeline. */
#elif EURASIA_PERISP_NUM_USE_PIPES_IN_X == 1 && EURASIA_PERISP_NUM_USE_PIPES_IN_Y > 1
			/* USE pipelines are striped in Y so combine the pipe number with the Y coordinate. */

			/* Extract the bottom bits of the pipe number. */
			BuildAND((PPVR_USE_INST) pui32Current,
					 EURASIA_USE1_EPRED_ALWAYS,
					 0,
					 EURASIA_USE1_RCNTSEL,
					 USE_REGTYPE_TEMP,
					 2,
					 USE_REGTYPE_SPECIAL,
					 EURASIA_USE_SPECIAL_BANK_PIPENUMBER | EURASIA_USE_SPECIAL_INTERNALDATA,
					 USE_REGTYPE_IMMEDIATE,
					 EURASIA_PERISP_NUM_USE_PIPES_IN_Y - 1);
			pui32Current += USE_INST_LENGTH;

			/* Convert the pipe number to pixel units in the top half of the register. */
			BuildSHL((PPVR_USE_INST) pui32Current,
					 EURASIA_USE1_EPRED_ALWAYS,
					 0,
					 EURASIA_USE1_RCNTSEL,
					 USE_REGTYPE_TEMP,
					 2,
					 USE_REGTYPE_TEMP,
					 2,
					 USE_REGTYPE_IMMEDIATE,
					 16 + EURASIA_PERISP_TILE_SHIFTY - EURASIA_PERISP_NUM_USE_PIPES_IN_Y_LOG2);
			pui32Current += USE_INST_LENGTH;

			/* Combine the pipe number with the Y coordinate. */
			BuildOR((PPVR_USE_INST)pui32Current,
					EURASIA_USE1_EPRED_ALWAYS,
					0,
					EURASIA_USE1_RCNTSEL,
					USE_REGTYPE_TEMP,
					1,
					USE_REGTYPE_TEMP,
					1,
					USE_REGTYPE_TEMP,
					2);
			pui32Current += USE_INST_LENGTH;
#endif
		}

		/* Combine the X and Y coordinates into the same register. */
		BuildOR((PPVR_USE_INST)pui32Current,
				EURASIA_USE1_EPRED_ALWAYS,
				0,
				EURASIA_USE1_RCNTSEL,
				USE_REGTYPE_TEMP,
				0,
				USE_REGTYPE_TEMP,
				0,
				USE_REGTYPE_TEMP,
				1);
		pui32Current += USE_INST_LENGTH;

#else /* EURASIA_PDS_IR0_TILEY_SHIFT == 9*/

		/* unpcku16u8 r0, saXY.0, saXY.1 */
		#if defined(SGX_FEATURE_USE_VEC34)
		BuildVPCKUNPCK((PPVR_USE_INST) pui32Current,
						EURASIA_USE1_EPRED_ALWAYS,
						0,
						EURASIA_USE1_SKIPINV,
						EURASIA_USE1_PCK_FMT_U16,
						EURASIA_USE1_PCK_FMT_U8,
						0,
						USE_REGTYPE_OUTPUT, 0, 0xF,
						USE_REGTYPE_SECATTR, ui32TileSA,
						USE_REGTYPE_SECATTR, ui32TileSA,
						0, // chan 0
						1, // chan 1
						0, // chan 2
						0); // chan 3
		#else
		BuildPCKUNPCK ((PPVR_USE_INST) pui32Current,
					   EURASIA_USE1_EPRED_ALWAYS, 0,
					   EURASIA_USE1_RCNTSEL,
					   EURASIA_USE1_PCK_FMT_U16, EURASIA_USE1_PCK_FMT_U8,
					   0,
					   USE_REGTYPE_TEMP, 0, 0xF,
					   USE_REGTYPE_SECATTR, ui32TileSA, 0,
					   USE_REGTYPE_SECATTR, ui32TileSA, 1);
		#endif
		pui32Current += USE_INST_LENGTH;

		if (!bTwiddled)
		{

#if EURASIA_PERISP_NUM_USE_PIPES_IN_X == 2 && EURASIA_PERISP_NUM_USE_PIPES_IN_Y == 1
			/* or r0, r0, g17 (= pipe number) */
			BuildOR((PPVR_USE_INST)pui32Current,
					EURASIA_USE1_EPRED_ALWAYS,
					0,
					EURASIA_USE1_RCNTSEL | EURASIA_USE1_BITWISE_PARTIAL,
					USE_REGTYPE_TEMP,
					0,
					USE_REGTYPE_TEMP,
					0,
					USE_REGTYPE_SPECIAL,
					EURASIA_USE_SPECIAL_BANK_PIPENUMBER | EURASIA_USE_SPECIAL_INTERNALDATA);
			pui32Current += USE_INST_LENGTH;

#elif EURASIA_PERISP_NUM_USE_PIPES_IN_X == 2 && EURASIA_PERISP_NUM_USE_PIPES_IN_Y == 2
			/* and TEMP2, gPIPE, #1 */
			BuildAND((PPVR_USE_INST)pui32Current,
					 EURASIA_USE1_EPRED_ALWAYS,
					 0,
					 EURASIA_USE1_RCNTSEL,
					 USE_REGTYPE_TEMP,
					 1,
					 USE_REGTYPE_SPECIAL,
					 EURASIA_USE_SPECIAL_BANK_PIPENUMBER | EURASIA_USE_SPECIAL_INTERNALDATA,
					 USE_REGTYPE_IMMEDIATE,
					 1);
			pui32Current += USE_INST_LENGTH;

			/* or TEMP1, TEMP1, TEMP2 */
			BuildOR((PPVR_USE_INST)pui32Current,
					EURASIA_USE1_EPRED_ALWAYS,
					0,
					EURASIA_USE1_RCNTSEL,
					USE_REGTYPE_TEMP,
					0,
					USE_REGTYPE_TEMP,
					0,
					USE_REGTYPE_TEMP,
					1);
			pui32Current += USE_INST_LENGTH;

			/* shr TEMP2, gPIPE, #1 */
			BuildSHR((PPVR_USE_INST)pui32Current,
					 EURASIA_USE1_EPRED_ALWAYS,
					 0,
					 EURASIA_USE1_RCNTSEL,
					 USE_REGTYPE_TEMP,
					 1,
					 USE_REGTYPE_SPECIAL,
					 EURASIA_USE_SPECIAL_BANK_PIPENUMBER | EURASIA_USE_SPECIAL_INTERNALDATA,
					 USE_REGTYPE_IMMEDIATE,
					 1);
			pui32Current += USE_INST_LENGTH;

			/* shl TEMP2, TEMP2, #16 */
			BuildSHL((PPVR_USE_INST)pui32Current,
					 EURASIA_USE1_EPRED_ALWAYS,
					 0,
					 EURASIA_USE1_RCNTSEL,
					 USE_REGTYPE_TEMP,
					 0,
					 USE_REGTYPE_TEMP,
					 1,
					 USE_REGTYPE_IMMEDIATE,
					 16);
			pui32Current += USE_INST_LENGTH;

			/* or TEMP1, TEMP1, TEMP2 */
			BuildOR((PPVR_USE_INST)pui32Current,
					EURASIA_USE1_EPRED_ALWAYS,
					0,
					EURASIA_USE1_RCNTSEL,
					USE_REGTYPE_TEMP,
					0,
					USE_REGTYPE_TEMP,
					0,
					USE_REGTYPE_TEMP,
					1);
			pui32Current += USE_INST_LENGTH;

#endif

			/* shl.partial r0, r0, #3 */
			BuildSHL((PPVR_USE_INST)pui32Current,
					 EURASIA_USE1_EPRED_ALWAYS,
					 0,
					 EURASIA_USE1_RCNTSEL | EURASIA_USE1_BITWISE_PARTIAL,
					 USE_REGTYPE_TEMP,
					 0,
					 USE_REGTYPE_TEMP,
					 0,
					 USE_REGTYPE_IMMEDIATE,
					 3);
			pui32Current += USE_INST_LENGTH;
		}
#endif /* EURASIA_PDS_IR0_TILEY_SHIFT == 9 */



#if defined(SGX_FEATURE_RENDER_TARGET_ARRAYS)
		/* Load up the required RTA stuff and store them in r2 and r3 -
		 * first available temps
		 */
		if (bEnableRTAs)
		{
			USEGEN_ASSERT(ui32ArrayStride != 0);
			/* Load the register value into r0 */

			BuildLDR((PPVR_USE_INST)pui32Current,
					 EURASIA_USE1_SKIPINV,
					 USE_REGTYPE_TEMP, 2,
					 USE_REGTYPE_IMMEDIATE,
					 (EUR_CR_DPM_3D_RENDER_TARGET_INDEX>>2),
					 0);
			pui32Current += USE_INST_LENGTH;

			BuildLIMM((PPVR_USE_INST) pui32Current, EURASIA_USE1_EPRED_ALWAYS,
					  0 /* Flags */, USE_REGTYPE_TEMP, 3, ui32ArrayStride);
			pui32Current += USE_INST_LENGTH;

			/* Wait for the reg read to finish */
			BuildWDF ((PPVR_USE_INST) pui32Current, 0);
			pui32Current += USE_INST_LENGTH;
		}
#else
		PVR_UNREFERENCED_PARAMETER(bEnableRTAs);
		PVR_UNREFERENCED_PARAMETER(ui32ArrayStride);
#endif

	} /* bWriteSetup */

	if (bTwiddled)
	{
#if defined(SGX_FEATURE_HYBRID_TWIDDLING)
		pui32Current = USEGenWriteHybridTwiddleAddress(pui32Current,
													   ui32TileSize,
													   ui32ChunkByteSize,
													   i32Stride,
													   ui32AddressSA);
		PVR_UNREFERENCED_PARAMETER(ui32Width);
		PVR_UNREFERENCED_PARAMETER(ui32Height);
		PVR_UNREFERENCED_PARAMETER(ui32TTAddr);
#else
		pui32Current = USEGenWriteTwiddleAddress(pui32Current,
												 ui32Width,
												 ui32Height,
												 ui32AddressSA,
												 ui32StrideSA,
												 ui32TTAddr,
												 bWriteSetup);
		PVR_UNREFERENCED_PARAMETER(ui32ChunkByteSize);
		PVR_UNREFERENCED_PARAMETER(ui32TileSize);
		PVR_UNREFERENCED_PARAMETER(i32Stride);
#endif
	}
	else
	{
		/*
		 * Now we can multiply the result (above) by the required stride and add
		 * it to the base address
		 * First, do the (X * pixel stride)
		 */

		BuildIMAE((PPVR_USE_INST)pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS,
				  0,
				  EURASIA_USE1_IMAE_SATURATE |
				  (EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT) |
				  EURASIA_USE1_IMAE_SRC1H_SELECTLOW |
				  EURASIA_USE1_IMAE_SRC0H_SELECTLOW,
				  USE_REGTYPE_SECATTR,
				  ui32AddressSA,
				  USE_REGTYPE_TEMP,
				  0,
				  0,
				  USE_REGTYPE_SECATTR,
				  ui32StrideSA,
				  0,
				  USE_REGTYPE_SECATTR,
				  ui32AddressSA,
				  0);
		pui32Current += USE_INST_LENGTH;

		/* Now, Y * linestride */
		BuildIMAE((PPVR_USE_INST)pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS,
				  0,
				  EURASIA_USE1_IMAE_SATURATE |
				  (EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT) |
				  EURASIA_USE1_IMAE_SRC1H_SELECTHIGH |
				  EURASIA_USE1_IMAE_SRC0H_SELECTHIGH,
				  USE_REGTYPE_SECATTR,
				  ui32AddressSA,
				  USE_REGTYPE_TEMP,
				  0,
				  0,
				  USE_REGTYPE_SECATTR,
				  ui32StrideSA,
				  0,
				  USE_REGTYPE_SECATTR,
				  ui32AddressSA,
				  0);
		pui32Current += USE_INST_LENGTH;

	}

#if defined(SGX_FEATURE_RENDER_TARGET_ARRAYS)
	if (bEnableRTAs)
	{
		/* Now do the multiply add:
		 * SA<Addr> = (stride * RTAIdx) + SA<Addr>
		 * IMAE saA, r2, r3, saA
		 */
		BuildIMA32((PPVR_USE_INST) pui32Current,
				   EURASIA_USE1_EPRED_ALWAYS, 0,
				   0, USE_REGTYPE_SECATTR, ui32AddressSA,
				   USE_REGTYPE_TEMP, 2,
				   USE_REGTYPE_TEMP, 3, IMG_FALSE,
				   USE_REGTYPE_SECATTR, ui32AddressSA, IMG_FALSE,
				   IMG_FALSE, 0, IMG_FALSE,
				   0);
		pui32Current += USE_INST_LENGTH;

	}
#endif
	return pui32Current;
}


/***********************************************************************//**
* Store or Load a pixel directly from memory.
* When loading, the results will be put (unmodified) into ui32PixelReg within
* ePixBank
*
* This function will also issue an IDF / WDF pair (always on 0) after writing
* the value to memory
*
* @param pui32Base			The base (linear) address to write the program to
* @param ui32AddressSA		The SA register containing the tile base to write to
* @param ui32StrideSA		The SA register containing the stride word
*							This must be in the format:
* StrideSA = ((linestride in bytes) << 16) | (pixelstride in bytes)
* @param ui32DataSize		The size of the data to be written (in bits)
* @param ePixBank			The bank to choose the pixel from
* @param ui32PixelReg		The register within the bank to write to memory
* @param bTwiddled			Whether the target is twiddled
* @param ui32TTAddr			SA containing the address of the twiddle table
* @param ui32TTStride		SA containing the stride of the twiddle table
* @param bLoad				Whether this is a load request (as opposed to a store
*							request)
* @param bIssueWDF			Whether a WDF should be issued for the load / store to
*							complete
* @param bUsePredication	When true the store instruction will be predicated
*							depending on AlphaToCoverage calculations
*							see USEGenWriteMOVMSKFragment() for details
* @param ui32TwiddleTmpRegNum Temp register number to be used for twiddled offset
*
* @return Pointer to the end of the written program
*
* @note This currently doesn't support tiled render targets
**************************************************************************/
IMG_PUINT32 USEGenPixDirectFragment(IMG_PUINT32 pui32Base,
									IMG_UINT32 ui32AddressSA,
									IMG_UINT32 ui32StrideSA,
									IMG_UINT32 ui32DataSize,
									USEGEN_BANK_TYPE ePixBank,
									IMG_UINT32 ui32PixelReg,
									IMG_BOOL bTwiddled,
									IMG_UINT32 ui32TTAddr,
									IMG_UINT32 ui32TTStride,
									IMG_BOOL bLoad,
									IMG_BOOL bIssueWDF,
									IMG_BOOL bUsePredication,
									IMG_UINT32 ui32TwiddleTmpRegNum)
{
	IMG_PUINT32 pui32Current = pui32Base;
	IMG_UINT32 ui32DataType;
	IMG_UINT32 ui32AddressMode;
	USE_REGTYPE eBank;
	IMG_UINT32 ui32Flags = 0;
	IMG_UINT32 ui32Predication;
	USE_REGTYPE eStrideRegType;
	IMG_UINT32  uStrideRegNum ;

	ui32Predication = bUsePredication ?
		EURASIA_USE1_EPRED_P1 : EURASIA_USE1_EPRED_ALWAYS;

	/* Convert the # of bytes into a datatype */
	switch (ui32DataSize)
	{
	case 8:
		ui32DataType = EURASIA_USE1_LDST_DTYPE_8BIT;
		break;
	case 16:
		ui32DataType = EURASIA_USE1_LDST_DTYPE_16BIT;
		break;
	case 32:
		ui32DataType = EURASIA_USE1_LDST_DTYPE_32BIT;
		break;
	default:
		USEGEN_DPF("Warning: Unhandled data size when doing LD / ST");
		USEGEN_DBG_BREAK;
		ui32DataType = EURASIA_USE1_LDST_DTYPE_32BIT;
		break;
	}

	switch (ePixBank)
	{
	case USEGEN_REG_BANK_PRIM:
		eBank = USE_REGTYPE_PRIMATTR;
		break;
	case USEGEN_REG_BANK_OUTPUT:
		eBank = USE_REGTYPE_OUTPUT;
		break;
	case USEGEN_REG_BANK_TEMP:
		eBank = USE_REGTYPE_TEMP;
		break;
	default:
		USEGEN_DPF("Warning: Unknown bank for packing source");
		eBank = USE_REGTYPE_PRIMATTR;
		break;
	}

	if (bTwiddled)
	{
		ui32AddressMode = EURASIA_USE1_LDST_AMODE_ABSOLUTE;
		ui32Flags |= EURASIA_USE1_LDST_MOEEXPAND;

		BuildLD((PPVR_USE_INST) pui32Current,
				EURASIA_USE1_EPRED_ALWAYS,
				0, EURASIA_USE1_RCNTSEL,
				0,
				EURASIA_USE1_LDST_DTYPE_16BIT, EURASIA_USE1_LDST_AMODE_TILED,
				EURASIA_USE1_LDST_IMODE_NONE,
				USE_REGTYPE_TEMP, ui32TwiddleTmpRegNum,
				USE_REGTYPE_SECATTR, ui32TTAddr,
				USE_REGTYPE_SECATTR, ui32TTStride,
				0, 0, IMG_TRUE);
		pui32Current += USE_INST_LENGTH;

		BuildWDF((PPVR_USE_INST) pui32Current, 0);
		pui32Current += USE_INST_LENGTH;

		BuildORIMM((PPVR_USE_INST) pui32Current,
				   EURASIA_USE1_EPRED_ALWAYS,
				   0,
				   EURASIA_USE1_RCNTSEL,
				   USE_REGTYPE_TEMP, ui32TwiddleTmpRegNum,
				   USE_REGTYPE_TEMP, ui32TwiddleTmpRegNum,
				   ui32DataSize >> 3,
				   16);
		pui32Current += USE_INST_LENGTH;
		
		/* Actually it is offset for LD/ST instruction */
		eStrideRegType = USE_REGTYPE_TEMP;
		uStrideRegNum = ui32TwiddleTmpRegNum;
	}
	else
	{
		ui32AddressMode = EURASIA_USE1_LDST_AMODE_TILED;
		
		/* Stride info for tiled LD/ST instruction */
		eStrideRegType = USE_REGTYPE_SECATTR;
		uStrideRegNum = ui32StrideSA;
	}

	/* Now build either a LD or an ST */
	if (bLoad)
	{
		IMG_BOOL bBPCache;
#if defined(SGX_FEATURE_WRITEBACK_DCU)
		/* We want to read directly from memory to ensure
		 * no cache issues occur between the previous ST and
		 * the current LD
		 * (issue seen on HIDEBUG sim with MRTs + blending)
		 */
		bBPCache = IMG_TRUE;
#else
		bBPCache = IMG_TRUE;
#endif

		BuildLD ((PPVR_USE_INST) pui32Current,
				 EURASIA_USE1_EPRED_ALWAYS,
				 0, EURASIA_USE1_RCNTSEL | ui32Flags,
				 0,
				 ui32DataType, ui32AddressMode,
				 EURASIA_USE1_LDST_IMODE_NONE,
				 eBank, ui32PixelReg,
				 USE_REGTYPE_SECATTR, ui32AddressSA,
				 eStrideRegType, uStrideRegNum,
				 0, 0, bBPCache);
		pui32Current += USE_INST_LENGTH;
	}
	else
	{
		IMG_BOOL bBPCache;
#if defined(SGX_FEATURE_WRITEBACK_DCU)
		/* For now we always want to write directly to memory
		 * to save cache issues
		 * (issue seen on HIDEBUG sim with MRTs + blending)
		 */
		bBPCache = IMG_TRUE;
#else
		bBPCache = IMG_FALSE;
#endif
		BuildST ((PPVR_USE_INST) pui32Current,
				 ui32Predication,
				 0, EURASIA_USE1_RCNTSEL | ui32Flags,
				 ui32DataType, ui32AddressMode,
				 EURASIA_USE1_LDST_IMODE_NONE,
				 USE_REGTYPE_SECATTR, ui32AddressSA,
				 eStrideRegType, uStrideRegNum,
				 eBank, ui32PixelReg, bBPCache);
		pui32Current += USE_INST_LENGTH;
	}

	if (bIssueWDF)
	{
		BuildWDF ((PPVR_USE_INST) pui32Current, 0);
		pui32Current += USE_INST_LENGTH;
	}

	return pui32Current;
}

/***********************************************************************//**
 * For dependant-read background objects, perform the sample and any other
 * setup required
 *
 * @param		pui32Base				The base (linear) address to write to
 * @param		ui32PrimOffset			Register offset where the texture
 *										coordinates can be found
 * @param		ui32SecOffset			Offset into the secondary bank for the
 *										texture control words
 * @param		eResultBank				Bank to put the results in
 * @param		ui32ResultOffset		Offset into the bank to put the results
 * @param		bRTAPresent				If set, RTAs are present
 * @param		ui32ElementStride		Element stride if dealing with an RTA
 * @param		bFirstCall				If this is the first call in the
 *										phase generation
 * @param		bIssueWDF				Whether a WDF should be issues or it
 *										will be done separately
 * @param[out] 	pui32Temps 				Number of temporary registers used
 *
 * @return Pointer to the end of the written program
 * @note Only primary attributes are supported as destinations
 **************************************************************************/
IMG_PUINT32 USEGenWriteBGDependentLoad(IMG_PUINT32 pui32Base,
									   IMG_UINT32 ui32PrimOffset,
									   IMG_UINT32 ui32SecOffset,
									   USEGEN_BANK_TYPE eResultBank,
									   IMG_UINT32 ui32ResultOffset,
									   IMG_BOOL bRTAPresent,
									   IMG_UINT32 ui32ElementStride,
									   IMG_BOOL bFirstCall,
									   IMG_BOOL bIssueWDF,
									   IMG_PUINT32 pui32Temps)
{
	IMG_PUINT32 pui32Current = pui32Base;
	IMG_UINT32 ui32TempCount = 0;
	IMG_UINT32 ui32TempOffset = 0;
	USE_REGTYPE eTexWords = USE_REGTYPE_SECATTR;
	IMG_BOOL bSecOffsetNeedsSMBO = (ui32SecOffset > 127);
	IMG_UINT32 ui32TexWordsOffset;
	USE_REGTYPE eBank = USE_REGTYPE_PRIMATTR;

#if !defined(SGX_FEATURE_RENDER_TARGET_ARRAYS)
	PVR_UNREFERENCED_PARAMETER(bFirstCall);
	PVR_UNREFERENCED_PARAMETER(ui32ElementStride);
#endif

	if (bSecOffsetNeedsSMBO)
	{
		ui32SecOffset -= 128;
	}
	ui32TexWordsOffset = ui32SecOffset;

	switch (eResultBank)
	{
	case USEGEN_REG_BANK_PRIM:
		eBank = USE_REGTYPE_PRIMATTR;
		break;
	case USEGEN_REG_BANK_OUTPUT:
		eBank = USE_REGTYPE_OUTPUT;
		break;
	case USEGEN_REG_BANK_TEMP:
		eBank = USE_REGTYPE_TEMP;
		if (bRTAPresent &&
			ui32ResultOffset < EURASIA_TAG_TEXTURE_STATE_SIZE + 2)
		{
			/*
			 * We have no space to fit all our temps in before this
			 * so we need to shuffle it up a bit
			 */
			ui32TempOffset = ui32ResultOffset + 2;
		}
		break;
	default:
		USEGEN_DPF("Warning: Unknown bank for iteration results");
		eBank = USE_REGTYPE_PRIMATTR;
		break;
	}

	/* Begin the setup */
#if defined(SGX_FEATURE_RENDER_TARGET_ARRAYS)
	if (bFirstCall)
	{
		/*
		 * Initial RTA setup involves fetching
		 * the RTAIdx register.  We'll wait for it later...
		 */
		if (bRTAPresent)
		{
			BuildLDR((PPVR_USE_INST) pui32Current,
					 EURASIA_USE1_SKIPINV,
					 USE_REGTYPE_TEMP, ui32TempOffset + 0,
					 USE_REGTYPE_IMMEDIATE, EUR_CR_DPM_3D_RENDER_TARGET_INDEX>>2, 0);
			pui32Current += USE_INST_LENGTH;

		}
	}

	/*
	 * For RTAs, we need to move our tex control words
	 * to temps to manipulate them
	 */
	if (bRTAPresent)
	{

		if (bSecOffsetNeedsSMBO)
		{
			BuildSMBO((PPVR_USE_INST) pui32Current, 0,0,128,0 );
			pui32Current += USE_INST_LENGTH;
		}

		BuildMOV((PPVR_USE_INST) pui32Current,
				 EURASIA_USE1_EPRED_ALWAYS,
				 (EURASIA_TAG_TEXTURE_STATE_SIZE-1), EURASIA_USE1_RCNTSEL,
				 USE_REGTYPE_TEMP, ui32TempOffset + 2,
				 USE_REGTYPE_SECATTR,
				 ui32SecOffset, 0);
		pui32Current += USE_INST_LENGTH;

		if (bSecOffsetNeedsSMBO)
		{
			BuildSMBO((PPVR_USE_INST) pui32Current, 0,0,0,0 );
			pui32Current += USE_INST_LENGTH;
		}

		BuildLIMM((PPVR_USE_INST) pui32Current, EURASIA_USE1_EPRED_ALWAYS,
				  0 /* Flags */, USE_REGTYPE_TEMP, ui32TempOffset + 1,
				  ui32ElementStride);
		pui32Current += USE_INST_LENGTH;

		if (bFirstCall)
		{
			/* Now we need to wait for our register to return */
			BuildWDF ((PPVR_USE_INST) pui32Current, 0);
			pui32Current += USE_INST_LENGTH;
		}

		/*
		 * We can just perform an IMA32 here, provided
		 * bits 0...1 of the stride are 0
		 */
		USEGEN_ASSERT((ui32ElementStride & 0x3) == 0);

		BuildIMA32((PPVR_USE_INST) pui32Current,
				   EURASIA_USE1_EPRED_ALWAYS, 0,
				   0, USE_REGTYPE_TEMP, ui32TempOffset + 2 + 2,
				   USE_REGTYPE_TEMP, ui32TempOffset + 0,
				   USE_REGTYPE_TEMP, ui32TempOffset + 1, IMG_FALSE,
				   USE_REGTYPE_TEMP, ui32TempOffset + 2 + 2, IMG_FALSE,
				   IMG_FALSE, 0, IMG_FALSE,
				   0);
		pui32Current += USE_INST_LENGTH;

		eTexWords = USE_REGTYPE_TEMP;
		ui32TexWordsOffset = ui32TempOffset + 2;

		ui32TempCount = (EURASIA_TAG_TEXTURE_STATE_SIZE + 2);
	}
#endif

	/* Now issue the sample */
	if (eTexWords == USE_REGTYPE_SECATTR && bSecOffsetNeedsSMBO)
	{
		BuildSMBO((PPVR_USE_INST) pui32Current, 0,0,128,0 );
		pui32Current += USE_INST_LENGTH;
	}

	BuildSMP2D((PPVR_USE_INST) pui32Current,
			   (EURASIA_USE1_RCNTSEL |
				EURASIA_USE1_SMP_LODM_REPLACE << EURASIA_USE1_SMP_LODM_SHIFT),
			   0, EURASIA_USE1_EPRED_ALWAYS,
			   eBank, ui32ResultOffset,
			   USE_REGTYPE_PRIMATTR, ui32PrimOffset,
			   eTexWords, ui32TexWordsOffset,
			   USE_REGTYPE_SPECIAL, EURASIA_USE_SPECIAL_CONSTANT_ZERO, 0);
	pui32Current += USE_INST_LENGTH;

	if (eTexWords == USE_REGTYPE_SECATTR && bSecOffsetNeedsSMBO)
	{
		BuildSMBO((PPVR_USE_INST) pui32Current, 0,0,0,0 );
		pui32Current += USE_INST_LENGTH;
	}

	if (bIssueWDF)
	{
		BuildWDF ((PPVR_USE_INST) pui32Current, 0);
		pui32Current += USE_INST_LENGTH;
	}

	*pui32Temps = (ui32TempCount + ui32TempOffset);
	return pui32Current;
}

/***********************************************************************//**
 * Write the secondary program to use when performing tile-buffer extended
 * outputs.
 *
 * @param pui32Base			The base (linear) address to write the program to
 * @param ui32FirstSA		SA register containing the address of the first
 *							tile buffer
 * @param ui32NumBuffers	Number of tile buffers to set up
 * @param[out] pui32TempCount Number of temporary registers used
 *
 * @return Pointer to the end of the written program
 *
 * @note This will write the entire program, including PHAS and END instructions
 **************************************************************************/
IMG_PUINT32 USEGenWriteTileBufferSecProgram(IMG_PUINT32 pui32Base,
											IMG_UINT32 ui32FirstSA,
											IMG_UINT32 ui32NumBuffers,
											IMG_PUINT32 pui32TempCount)
{
	IMG_PUINT32 pui32Current = pui32Base;
	IMG_UINT32 i;
	IMG_UINT32 ui32Flags = 0;

	if (ui32NumBuffers == 0)
	{
		/* No need to do anything */
		return pui32Current;
	}

	/*
	 * First, set up the phas instruction
	 */
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	pui32Current = USEGenWritePhaseFragment(pui32Current, 0, 0,
											USEGEN_NO_DEPENDANT,
											IMG_FALSE, IMG_FALSE,
											IMG_TRUE, IMG_FALSE);
#endif

	/* Now the initial buffer-independant setup */
	BuildMOV((PPVR_USE_INST) pui32Current,
			 EURASIA_USE1_EPRED_ALWAYS,
			 0, EURASIA_USE1_RCNTSEL,
			 USE_REGTYPE_TEMP, 0,
			 USE_REGTYPE_SPECIAL,
			 EURASIA_USE_SPECIAL_BANK_PIPENUMBER | EURASIA_USE_SPECIAL_INTERNALDATA, 0);
	pui32Current += USE_INST_LENGTH;

	BuildIMAE((PPVR_USE_INST)pui32Current,
			  EURASIA_USE1_EPRED_ALWAYS,
			  0,
			  EURASIA_USE1_IMAE_SATURATE |
			  (EURASIA_USE1_IMAE_SRC2TYPE_16BITZEXTEND << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT) |
			  EURASIA_USE1_IMAE_SRC1H_SELECTLOW |
			  EURASIA_USE1_IMAE_SRC0H_SELECTLOW,
			  USE_REGTYPE_TEMP, 0, /* Dest */
			  USE_REGTYPE_TEMP, 0, 0, /* Src0 */
			  USE_REGTYPE_IMMEDIATE, SGX_FEATURE_USE_NUMBER_PIXEL_OUTPUT_PARTITIONS, 0, /* Src1 */
			  USE_REGTYPE_SPECIAL, /* Src2 */
			  EURASIA_USE_SPECIAL_BANK_OUTPUTBASE | EURASIA_USE_SPECIAL_INTERNALDATA,
			  0);
	pui32Current += USE_INST_LENGTH;

		/* Each output buffer is 512 bytes in size, no matter what */
	BuildLIMM((PPVR_USE_INST) pui32Current, EURASIA_USE1_EPRED_ALWAYS,
			  0 /* Flags */, USE_REGTYPE_TEMP, 1,
			  EURASIA_USE_OUTPUT_BANK_SIZE * sizeof(IMG_UINT32));
	pui32Current += USE_INST_LENGTH;

	/* Now for each tile buffer, perform the required setup */
	for ( i = 0; i < ui32NumBuffers; i++)
	{
		if (i == ui32NumBuffers-1)
		{
			ui32Flags = EURASIA_USE1_END;
		}

		/*
		 * Basic algorithm is:
		 * RealBase = Base + (((Pipe * 24) + OutputBuffer#) * BufferSize)
		 * This is done as 2 IMAs:
		 * Tmp = (pipe * 24) + outputBuffer#
		 * RealBase = (Tmp * BufferSize) + Base
		 * Or in real money:
		 * imae.u.skipinv r0, r0.l, #24.l, g18.l, z16
		 * imae.u.skipinv.end pa1, r0.l, r1.l, pa1, u32
		 *
		 * Where the first imae is static across the all buffers
		 * and is calculated first.
		 *
		 * On the final IMA, we set the end flag
		 */

		BuildIMAE((PPVR_USE_INST)pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS,
				  0,
				  ui32Flags | EURASIA_USE1_IMAE_SATURATE |
				  (EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT) |
				  EURASIA_USE1_IMAE_SRC1H_SELECTLOW |
				  EURASIA_USE1_IMAE_SRC0H_SELECTLOW,
				  USE_REGTYPE_PRIMATTR, (ui32FirstSA + (i*2)), /* Dst */
				  USE_REGTYPE_TEMP, 0, 0, /* Src0 */
				  USE_REGTYPE_TEMP, 1, 0, /* Src1 */
				  USE_REGTYPE_PRIMATTR, (ui32FirstSA + (i*2)), 0); /* Src2 */
		pui32Current += USE_INST_LENGTH;
	}

	/* Finished - return our temp count */
	*pui32TempCount = 3;
	return pui32Current;
}

/***********************************************************************//**
 * Perform a direct read or write to a tile buffer for later use in the
 * EOT program
 *
 * for direct writes, we will always perform an IDF on DRC0.  Loads will always
 * be issued on DRC0.
 *
 * @param pui32Base			The base (linear) address to write the program to
 * @param ui32TileSA		SA register containing the address of the
 *							tile buffer to write to
 * @param ui32DataSize		Size in bytes of the data to operate on
 * @param ui32DataReg		Register number to read from / write to
 * @param eDataBank			Register bank to read from / write to
 * @param bLoad				Whether this is a load or a store operation
 * @param bIssueWDF			Whether we should issue a WDF to wait for the
 *							operation to return from memory
 *
 * @return Pointer to the end of the written program
 **************************************************************************/
IMG_PUINT32 USEGenTileBufferAccess(IMG_PUINT32 pui32Base,
								   IMG_UINT32 ui32TileSA,
								   IMG_UINT32 ui32DataSize,
								   IMG_UINT32 ui32DataReg,
								   USEGEN_BANK_TYPE eDataBank,
								   IMG_BOOL bLoad,
								   IMG_BOOL bIssueWDF)
{
	IMG_PUINT32 pui32Current = pui32Base;
	IMG_UINT32 ui32DataType;
	USE_REGTYPE eBank;

	IMG_BOOL bBPCache;
#if defined(SGX_FEATURE_WRITEBACK_DCU)
	bBPCache = IMG_FALSE;
#else
	bBPCache = IMG_TRUE;
#endif

	/* Convert the # of bytes into a datatype */
	switch (ui32DataSize)
	{
	case 1:
		ui32DataType = EURASIA_USE1_LDST_DTYPE_8BIT;
		break;
	case 2:
		ui32DataType = EURASIA_USE1_LDST_DTYPE_16BIT;
		break;
	case 4:
		ui32DataType = EURASIA_USE1_LDST_DTYPE_32BIT;
		break;
	default:
		USEGEN_DPF("Warning: Unhandled data size when doing LD / ST");
		USEGEN_DBG_BREAK;
		ui32DataType = EURASIA_USE1_LDST_DTYPE_32BIT;
		break;
	}

	switch (eDataBank)
	{
	case USEGEN_REG_BANK_PRIM:
		eBank = USE_REGTYPE_PRIMATTR;
		break;
	case USEGEN_REG_BANK_OUTPUT:
		eBank = USE_REGTYPE_OUTPUT;
		break;
	case USEGEN_REG_BANK_TEMP:
		eBank = USE_REGTYPE_TEMP;
		break;
	default:
		USEGEN_DPF("Warning: Invalid bank for load / store source");
		USEGEN_DBG_BREAK;
		eBank = USE_REGTYPE_PRIMATTR;
		break;
	}

	if (bLoad)
	{
		BuildLD ((PPVR_USE_INST) pui32Current,
				 EURASIA_USE1_EPRED_ALWAYS,
				 0, EURASIA_USE1_RCNTSEL,
				 0,
				 ui32DataType, EURASIA_USE1_LDST_AMODE_TILED,
				 EURASIA_USE1_LDST_IMODE_NONE,
				 eBank, ui32DataReg,
				 USE_REGTYPE_SECATTR, ui32TileSA,
				 USE_REGTYPE_SECATTR, ui32TileSA+1,
				 0, 0, bBPCache);
		pui32Current += USE_INST_LENGTH;
	}
	else
	{
		BuildST ((PPVR_USE_INST) pui32Current,
				 EURASIA_USE1_EPRED_ALWAYS,
				 0, EURASIA_USE1_RCNTSEL,
				 ui32DataType, EURASIA_USE1_LDST_AMODE_TILED,
				 EURASIA_USE1_LDST_IMODE_NONE,
				 USE_REGTYPE_SECATTR, ui32TileSA,
				 USE_REGTYPE_SECATTR, ui32TileSA+1,
				 eBank, ui32DataReg, bBPCache);
		pui32Current += USE_INST_LENGTH;

		if (bIssueWDF)
		{
			BuildIDF((PPVR_USE_INST) pui32Current,
					 0, EURASIA_USE1_IDF_PATH_ST);
			pui32Current += USE_INST_LENGTH;
		}
	}

	if (bIssueWDF)
	{
		BuildWDF ((PPVR_USE_INST) pui32Current, 0);
		pui32Current += USE_INST_LENGTH;
	}

	/* Done */
	return pui32Current;
}

/***********************************************************************//**
 * Setup the initial statements required for RTA pixel event program changes
 *
 * @param pui32Base			The base (linear) address to write the program to
 * @param[out] pui32Temps	Number of temporary registers needed
 *
 * @return Pointer to the end of the written fragment
 **************************************************************************/
IMG_PUINT32 USEGenEOTSetupForRTAs(IMG_PUINT32 pui32Base,
								  IMG_PUINT32 pui32Temps)
{
	IMG_PUINT32 pui32Current = pui32Base;

#if defined(SGX_FEATURE_RENDER_TARGET_ARRAYS)
	BuildLDR((PPVR_USE_INST) pui32Current,
			 EURASIA_USE1_SKIPINV,
			 USE_REGTYPE_TEMP, 0,
			 USE_REGTYPE_IMMEDIATE, EUR_CR_DPM_3D_RENDER_TARGET_INDEX>>2, 0);
	pui32Current += USE_INST_LENGTH;

	BuildWDF ((PPVR_USE_INST) pui32Current, 0);
	pui32Current += USE_INST_LENGTH;

	*pui32Temps = 1;
#else
	*pui32Temps = 0;
#endif
	return pui32Current;
}

/***********************************************************************//**
 * Write a PBE Emit fragment for the EOT task
 *
 * @param pui32Base			The base (linear) address to write the program to
 * @param ui32PartitionOffset	Offset to set the INCP to
 *								This must be either 0 or 2
 * @param bRTAPresent		Whether the base address in the emit words
 *							should be offset by the (element stride) * RTAIdx
 * @param ui32ElementStride	Element stride for RT
 * @param ui32TempOffset	Offset of the first temp to use
 * @param aui32EmitWords	Array of emit words, typically generated using
 *							the WritePBEEmitState function.
 * @param bFlushCache		Whether to perform a cache flush.
 *							On chips without a Writeback DCU, this does nothing
 * @param bEnd				Whether to write an end flag and free the partitions
 *
 * @return Pointer to the end of the written fragment
 **************************************************************************/
IMG_PUINT32 USEGenEOTEmitFragment(IMG_PUINT32 pui32Base,
								  IMG_UINT32 ui32PartitionOffset,
								  IMG_BOOL bRTAPresent,
								  IMG_UINT32 ui32RTAElementStride,
								  IMG_UINT32 ui32TempOffset,
								  IMG_PUINT32 aui32EmitWords,
								  IMG_BOOL bFlushCache,
								  IMG_BOOL bEnd)
{
	IMG_UINT32 i;
	IMG_UINT32 *pui32Current = pui32Base;
	
#if !defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
	IMG_UINT32 ui32EmitFlag;
#endif
	
#if defined(SGX545)
	USEGEN_ASSERT(ui32PartitionOffset == 0 || ui32PartitionOffset == 2);
#else
	USEGEN_ASSERT(ui32PartitionOffset == 0);
#endif

#if !defined(SGX_FEATURE_RENDER_TARGET_ARRAYS)
	PVR_UNREFERENCED_PARAMETER(bRTAPresent);
	PVR_UNREFERENCED_PARAMETER(ui32RTAElementStride);
#endif
#if defined(SGX_FEATURE_WRITEBACK_DCU)
	if(bFlushCache)
	{
		/*
		 * If this is the last tile in the render then flush the cache..
		 */
		BuildALUTST((PPVR_USE_INST)pui32Current,
					EURASIA_USE1_EPRED_ALWAYS,
					1,
					EURASIA_USE1_TEST_CRCOMB_AND,
					USE_REGTYPE_TEMP,
					ui32TempOffset,
					0,
					USE_REGTYPE_PRIMATTR,
					0,
					USE_REGTYPE_IMMEDIATE,
					EURASIA_PDS_IR1_PIX_EVENT_ENDRENDER,
					0,
					EURASIA_USE1_TEST_STST_NONE,
					EURASIA_USE1_TEST_ZTST_ZERO,
					EURASIA_USE1_TEST_CHANCC_SELECT0,
					EURASIA_USE0_TEST_ALUSEL_BITWISE,
					EURASIA_USE0_TEST_ALUOP_BITWISE_AND);
		pui32Current += USE_INST_LENGTH;

		BuildBR((PPVR_USE_INST)pui32Current, EURASIA_USE1_EPRED_P0, 2, 0);
		pui32Current += USE_INST_LENGTH;

		BuildCFI((PPVR_USE_INST)pui32Current,
				 (SGX545_USE1_OTHER2_CFI_GLOBAL |
				  SGX545_USE1_OTHER2_CFI_FLUSH |
				  SGX545_USE1_OTHER2_CFI_INVALIDATE),
				 0,
				 USE_REGTYPE_IMMEDIATE,
				 SGX545_USE_OTHER2_CFI_DATAMASTER_PIXEL,
				 USE_REGTYPE_IMMEDIATE,
				 0,
				 SGX545_USE1_OTHER2_CFI_LEVEL_MAX);
		pui32Current += USE_INST_LENGTH;
	}
#else
	PVR_UNREFERENCED_PARAMETER(bFlushCache);
#endif

	/*
	 * Load in our state emit words
	 */
	for(i=0; i < STATE_PBE_DWORDS; i++)
	{
		IMG_UINT32 ui32Value = aui32EmitWords[i];
		/* LIMM r[i], emitword[i] */
		#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
		if (!bEnd && i == 5)
		{
			ui32Value |= EURASIA_PIXELBES2HI_NOADVANCE;
		}
		#else
		if (!bEnd && i == 1)
		{
			ui32Value |= EURASIA_PIXELBE1S1_NOADVANCE;
		}
		#endif
		BuildLIMM((PPVR_USE_INST) pui32Current, EURASIA_USE1_EPRED_ALWAYS,
				  0 /* Flags */, USE_REGTYPE_TEMP, ui32TempOffset + i, ui32Value);
		pui32Current += USE_INST_LENGTH;
#if defined(SGX_FEATURE_RENDER_TARGET_ARRAYS)
		if (bRTAPresent && i == EMIT_ADDRESS)
		{
			/*
			 * Modify the base address by adding on
			 * (stride * RTIndex)
			 */
			BuildLIMM((PPVR_USE_INST) pui32Current, EURASIA_USE1_EPRED_ALWAYS,
					  0 /* Flags */,
					  USE_REGTYPE_TEMP, ui32TempOffset + i + 1,
					  ui32RTAElementStride);
			pui32Current += USE_INST_LENGTH;

			BuildIMA32((PPVR_USE_INST) pui32Current,
					   EURASIA_USE1_EPRED_ALWAYS, 0,
					   0, USE_REGTYPE_TEMP, ui32TempOffset + i,
					   USE_REGTYPE_TEMP, 0,
					   USE_REGTYPE_TEMP, ui32TempOffset + i + 1, IMG_FALSE,
					   USE_REGTYPE_TEMP, ui32TempOffset + i, IMG_FALSE,
					   IMG_FALSE, 0, IMG_FALSE,
					   0);
			pui32Current += USE_INST_LENGTH;
		}
#endif
	}
	
#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
	/*
	 * Set up the emit..
	 */
	_BuildEMITPIX((PPVR_USE_INST) pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS, 0 /* Flags */,
				  USE_REGTYPE_TEMP, ui32TempOffset + 0,
				  USE_REGTYPE_TEMP, ui32TempOffset + 1,
				  USE_REGTYPE_TEMP, ui32TempOffset + 2,
				  ui32PartitionOffset,
				  bEnd,
				  aui32EmitWords[EMIT2_SIDEBAND]);
	pui32Current += USE_INST_LENGTH;
#else
	/*
	 * Set up two emits
	 */
	ui32EmitFlag = EURASIA_PIXELBE1SB_TWOEMITS;

	_BuildEMITPIX((PPVR_USE_INST) pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS, 0 /* Flags */,
				  USE_REGTYPE_TEMP, ui32TempOffset,
				  USE_REGTYPE_TEMP, ui32TempOffset + 1,
				  USE_REGTYPE_TEMP, ui32TempOffset + 1,
				  ui32PartitionOffset,
				  bEnd,
				  ui32EmitFlag);
	pui32Current += USE_INST_LENGTH;

	_BuildEMITPIX((PPVR_USE_INST) pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS, 0 /* Flags */,
				  USE_REGTYPE_TEMP, ui32TempOffset + 2,
				  USE_REGTYPE_TEMP, ui32TempOffset + 3,
				  #if defined(SGX_FEATURE_PIXELBE_32K_LINESTRIDE)
				  USE_REGTYPE_TEMP, ui32TempOffset + 4,
				  #else
				  USE_REGTYPE_TEMP, ui32TempOffset + 3,
				  #endif
				  ui32PartitionOffset,
				  bEnd,
				  aui32EmitWords[EMIT2_SIDEBAND]);
	pui32Current += USE_INST_LENGTH;
#endif

	if (bEnd)
	{
		/* Terminate this block of code (sneaky negative array index) */
		pui32Current[-1] |= EURASIA_USE1_END;
	}

	return pui32Current;
}

/***********************************************************************//**
 * Load a tile buffer of data into the EOT task and shuffle it into the
 * output buffer, ready to be emitted.
 *
 * The bFirstLoad indicates the required setup for all future tile-buffer loads
 * must be done now.  This will typically consume 3 temps from the base and
 * return this as the pui32StaticTempBase.
 *
 * Future calls require this data to be present (unless the bFirstLoad is set
 * again), so nothing must touch these temps.  This can normally be achieved
 * by passing this value into the USEGenEOTEmitFragment as the ui32TempOffset
 * element.
 *
 * @param pui32Base			The base (linear) address to write the program to
 * @param bFirstLoad		Whether setup is required
 * @param bFirstLoad		If set, we should account for RTAs being present
 * @param sTileBufferAddress	Device address for the base of this tile buffer
 *								set
 * @param ui32DataSize		Size, in bytes, of the data to load
 * @param ui32PartitionOffset	Partition offset in the output buffer to
 *								write to.  This must be either 0 or 2 and
 *								will write beginning at o0 or o128 depending on
 *								this
 * @param bLastWasDoublePartition	If this isn't the first load, setting this
 *								indicates the emit was a double partition and we
 *								shouldwait for both partitions to become free
 * @param[out] pui32StaticTempBase	On the first call, we specify how
 *									many temps we steal from the bottom of the
 *									temp bank.  On subsequent calls this is
 *									ignored.
 * @param[out] pui32Temps		Total number of temps required for this
 *								operation
 *
 * @return Pointer to the end of the written fragment
 **************************************************************************/
IMG_PUINT32 USEGenEOTLoadFragment(IMG_PUINT32 pui32Base,
								  IMG_BOOL bFirstLoad,
								  IMG_BOOL bRTAPresent,
								  IMG_DEV_VIRTADDR sTileBufferAddress,
								  IMG_UINT32 ui32DataSize,
								  IMG_UINT32 ui32PartitionOffset,
								  IMG_BOOL bLastWasDoublePartition,
								  IMG_PUINT32 pui32StaticTempBase,
								  IMG_PUINT32 pui32Temps)
{
	IMG_PUINT32 pui32Current = pui32Base;
	IMG_UINT32 i, j;
	IMG_UINT32 ui32DataType;
	IMG_UINT32 ui32TempOffsetForLoad = 3;
	IMG_UINT32 ui32TempOffsetForRead = 3;
	IMG_UINT32 ui32IntOffset = 0;
	IMG_UINT32 ui32PipeOffset = 1;
	IMG_UINT32 ui32BufferOffset = 2;
	IMG_UINT32 ui32DRC = 0;

	IMG_UINT32 ui32NumLines = 0;
	IMG_UINT32 ui32NumLoadsPerLine = 0;
	IMG_UINT32 ui32RepCount = 0;
	IMG_UINT32 ui32SizePerLine = 0;
	IMG_UINT32 ui32SizePerLoop = 0;

	IMG_UINT32 ui32MaxTemp = 0;

	IMG_BOOL bBPCache;
#if defined(SGX_FEATURE_WRITEBACK_DCU)
	bBPCache = IMG_FALSE;
#else
	bBPCache = IMG_TRUE;
#endif
	/* Set up our required data */
	ui32SizePerLine =
		EURASIA_PERISP_TILE_SIZEX / EURASIA_PERISP_NUM_USE_PIPES_IN_X;

	ui32NumLines = (EURASIA_USE_OUTPUT_BANK_SIZE / ui32SizePerLine) >> 1;

	ui32RepCount = ((ui32SizePerLine*2) / 4) - 1;
	/*
	 * We can load up to 16 at a time (for now)
	 * and to support the Z-ordering, we need 2 lines at a time
	 */
	ui32SizePerLoop = ui32SizePerLine * 2;
	ui32NumLoadsPerLine = (ui32SizePerLoop) / 16;

	/* We double-buffer our lines for efficiency */
	ui32MaxTemp = ui32SizePerLoop*2;

#if defined(SGX_FEATURE_RENDER_TARGET_ARRAYS)
	if (bRTAPresent)
	{
		ui32TempOffsetForRead += 1;
		ui32TempOffsetForLoad += 1;
		ui32IntOffset += 1;
		ui32PipeOffset += 1;
		ui32BufferOffset += 1;
	}
#else
	PVR_UNREFERENCED_PARAMETER(bRTAPresent);
#endif

	if (bFirstLoad)
	{
		/*
		 * On first load, load up a couple of required constants
		 * r0 = 512 (int)
		 * r1 = Pipe Number
		 */
		BuildLIMM((PPVR_USE_INST) pui32Current, EURASIA_USE1_EPRED_ALWAYS,
				  0 /* Flags */, USE_REGTYPE_TEMP, ui32IntOffset,
				  EURASIA_USE_OUTPUT_BANK_SIZE * sizeof(IMG_UINT32));
		pui32Current += USE_INST_LENGTH;

		BuildMOV((PPVR_USE_INST) pui32Current, EURASIA_USE1_EPRED_ALWAYS, 0,
				 EURASIA_USE1_RCNTSEL,
				 USE_REGTYPE_TEMP, ui32PipeOffset,
				 USE_REGTYPE_SPECIAL,
				 (EURASIA_USE_SPECIAL_BANK_PIPENUMBER | EURASIA_USE_SPECIAL_INTERNALDATA),
				 0);
		pui32Current += USE_INST_LENGTH;

		BuildIMAE((PPVR_USE_INST)pui32Current,
				  EURASIA_USE1_EPRED_ALWAYS,
				  0,
				  EURASIA_USE1_IMAE_SATURATE |
				  (EURASIA_USE1_IMAE_SRC2TYPE_16BITZEXTEND << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT) |
				  EURASIA_USE1_IMAE_SRC1H_SELECTLOW |
				  EURASIA_USE1_IMAE_SRC0H_SELECTLOW,
				  USE_REGTYPE_TEMP, ui32PipeOffset, /* Dest */
				  USE_REGTYPE_TEMP, ui32PipeOffset, 0, /* Src0 */
				  USE_REGTYPE_IMMEDIATE, SGX_FEATURE_USE_NUMBER_PIXEL_OUTPUT_PARTITIONS, 0, /* Src1 */
				  USE_REGTYPE_SPECIAL, /* Src2 */
				  EURASIA_USE_SPECIAL_BANK_OUTPUTBASE | EURASIA_USE_SPECIAL_INTERNALDATA,
				  0);
		pui32Current += USE_INST_LENGTH;

		*pui32StaticTempBase = 3;
	}

	/* Load up the base address of the tile buffer in all cases */
	BuildLIMM((PPVR_USE_INST) pui32Current, EURASIA_USE1_EPRED_ALWAYS,
			  0, USE_REGTYPE_TEMP, ui32BufferOffset, sTileBufferAddress.uiAddr);
	pui32Current += USE_INST_LENGTH;

	/*
	 * Perform a repeat of the calculation we did in the pixel shader secondary
	 * program to work out where to find our data
	 */
	BuildIMAE((PPVR_USE_INST)pui32Current,
			  EURASIA_USE1_EPRED_ALWAYS,
			  0,
			  EURASIA_USE1_IMAE_SATURATE |
			  (EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT) |
			  EURASIA_USE1_IMAE_SRC1H_SELECTLOW |
			  EURASIA_USE1_IMAE_SRC0H_SELECTLOW,
			  USE_REGTYPE_TEMP, ui32BufferOffset, /* Dst */
			  USE_REGTYPE_TEMP, ui32IntOffset, 0, /* Src0 */
			  USE_REGTYPE_TEMP, ui32PipeOffset, 0, /* Src1 */
			  USE_REGTYPE_TEMP, ui32BufferOffset, 0); /* Src2 */
	pui32Current += USE_INST_LENGTH;

	/*
	 * We're now all set up to begin loading our data
	 * To do this as efficiently as we can, we do this
	 * in four blocks of 2 lines.
	 * The output buffer is Z-ordered, so we need to do
	 * two lines of 16 at a time, with the correct smlsi
	 * in place
	 */
	switch (ui32DataSize)
	{
	case 1:
		ui32DataType = EURASIA_USE1_LDST_DTYPE_8BIT;
		break;
	case 2:
		ui32DataType = EURASIA_USE1_LDST_DTYPE_16BIT;
		break;
	case 4:
		ui32DataType = EURASIA_USE1_LDST_DTYPE_32BIT;
		break;
	default:
		USEGEN_DPF("Warning: Unhandled data size in EOT task");
		USEGEN_DBG_BREAK;
		ui32DataType = EURASIA_USE1_LDST_DTYPE_32BIT;
		break;
	}

	/* First, perform a two-line fetch to get us started */
	for (i = 0; i < ui32NumLoadsPerLine; i++)
	{
		BuildLD ((PPVR_USE_INST) pui32Current,
				 EURASIA_USE1_EPRED_ALWAYS,
				 15, 0,
				 0,
				 ui32DataType, EURASIA_USE1_LDST_AMODE_ABSOLUTE,
				 EURASIA_USE1_LDST_IMODE_POST,
				 USE_REGTYPE_TEMP, (ui32TempOffsetForLoad + (i*16)),
				 USE_REGTYPE_TEMP, ui32BufferOffset,
				 USE_REGTYPE_IMMEDIATE, 0,
				 USE_REGTYPE_IMMEDIATE, 0, bBPCache);
		pui32Current += USE_INST_LENGTH;
	}
	ui32TempOffsetForLoad = (ui32TempOffsetForLoad + (16*ui32NumLoadsPerLine)) % ui32MaxTemp;
	ui32DRC = (ui32DRC+1)%2;

	/* Now on with the main loop */
	for (i = 0; i < ui32NumLines; i++)
	{
		/* Perform a new load on all but the last loop */
		if (i != (ui32NumLines-1))
		{
			for (j = 0; j < ui32NumLoadsPerLine; j++)
			{
				BuildLD ((PPVR_USE_INST) pui32Current,
						 EURASIA_USE1_EPRED_ALWAYS,
						 15, 0,
						 ui32DRC,
						 ui32DataType, EURASIA_USE1_LDST_AMODE_ABSOLUTE,
						 EURASIA_USE1_LDST_IMODE_POST,
						 USE_REGTYPE_TEMP, (ui32TempOffsetForLoad + (j*16)),
						 USE_REGTYPE_TEMP, ui32BufferOffset,
						 USE_REGTYPE_IMMEDIATE, 0,
						 USE_REGTYPE_IMMEDIATE, 0, bBPCache);
				pui32Current += USE_INST_LENGTH;
			}
			ui32TempOffsetForLoad =
				(ui32TempOffsetForLoad + (16*ui32NumLoadsPerLine)) % ui32MaxTemp;

			/*
			 * Set up our state - this is already set up
			 * on our last iteration
			 */
			BuildSMLSI((PPVR_USE_INST) pui32Current,
					   0, 0, 0, 0, /*< Limits */
					   4, 1, 2, 1); /*< Increments */
			pui32Current += USE_INST_LENGTH;

			if (ui32PartitionOffset != 0)
			{
				USEGEN_ASSERT(ui32PartitionOffset == 2);
				BuildSMBO((PPVR_USE_INST) pui32Current, 128, 0, 0, 0);
				pui32Current += USE_INST_LENGTH;
			}
		}
		ui32DRC = (ui32DRC+1)%2;

		/*
		 * If this is our first load, wait for our partition to free up before
		 * writing to it
		 */
		if (i == 0 && bLastWasDoublePartition)
		{
			/* Wait for both partitions, otherwise we get corruption */
			BuildWOP((PPVR_USE_INST) pui32Current, 0, 0);
			pui32Current += USE_INST_LENGTH;
			BuildWOP((PPVR_USE_INST) pui32Current, 2, 0);
			pui32Current += USE_INST_LENGTH;
		}
		else if (i == 0)
		{
			BuildWOP((PPVR_USE_INST) pui32Current, ui32PartitionOffset, 0);
			pui32Current += USE_INST_LENGTH;
		}

		/* Wait for the previous DRC to complete */
		BuildWDF ((PPVR_USE_INST) pui32Current, ui32DRC);
		pui32Current += USE_INST_LENGTH;

		/* Do our MOVs  */
		BuildMOV((PPVR_USE_INST) pui32Current, EURASIA_USE1_EPRED_ALWAYS,
				 ui32RepCount,
				 EURASIA_USE1_RCNTSEL,
				 USE_REGTYPE_OUTPUT, 0 + (i*ui32SizePerLoop),
				 USE_REGTYPE_TEMP, ui32TempOffsetForRead,
				 0);
		pui32Current += USE_INST_LENGTH;
		BuildMOV((PPVR_USE_INST) pui32Current, EURASIA_USE1_EPRED_ALWAYS,
				 ui32RepCount,
				 EURASIA_USE1_RCNTSEL,
				 USE_REGTYPE_OUTPUT, 1 + (i*ui32SizePerLoop),
				 USE_REGTYPE_TEMP, ui32TempOffsetForRead+1,
				 0);
		pui32Current += USE_INST_LENGTH;
		BuildMOV((PPVR_USE_INST) pui32Current, EURASIA_USE1_EPRED_ALWAYS,
				 ui32RepCount,
				 EURASIA_USE1_RCNTSEL,
				 USE_REGTYPE_OUTPUT, 2 + (i*ui32SizePerLoop),
				 USE_REGTYPE_TEMP, ui32TempOffsetForRead+ui32SizePerLine,
				 0);
		pui32Current += USE_INST_LENGTH;
		BuildMOV((PPVR_USE_INST) pui32Current, EURASIA_USE1_EPRED_ALWAYS,
				 ui32RepCount,
				 EURASIA_USE1_RCNTSEL,
				 USE_REGTYPE_OUTPUT, 3 + (i*ui32SizePerLoop),
				 USE_REGTYPE_TEMP, ui32TempOffsetForRead+ui32SizePerLine+1,
				 0);
		pui32Current += USE_INST_LENGTH;

		ui32TempOffsetForRead =
			(ui32TempOffsetForRead + (16*ui32NumLoadsPerLine)) % ui32MaxTemp;

		/* And be good citizens and reset our state, unless we're about to
		 * reuse our state */
		if (i != (ui32NumLines-2))
		{
			BuildSMLSI((PPVR_USE_INST) pui32Current,
					   0, 0, 0, 0, /*< Limits */
					   1, 1, 1, 1); /*< Increments */
			pui32Current += USE_INST_LENGTH;

			if (ui32PartitionOffset != 0)
			{
				USEGEN_ASSERT(ui32PartitionOffset == 2);
				BuildSMBO((PPVR_USE_INST) pui32Current, 0, 0, 0, 0);
				pui32Current += USE_INST_LENGTH;
			}
		}

	}

	/* We're done.  Set up our temps and lets go. */
	*pui32Temps = (ui32MaxTemp + 3);
	return pui32Current;
}

/***********************************************************************//**
 * Calculates the size of the integer register store program fragment.
 *
 * @param bGeometryShader	The fragment is for a geometry shader.
 * @param bNonCGInstancing	The fragment is for a shader which uses
 *							instanced drawing (ignored for GS).
 * @param ui32Elements		The total number of elements across all 
 *							integer registers.
 * @return The size of the program in bytes.
 **************************************************************************/
IMG_UINT32 USEGenCalculateIntRegStoreSize(IMG_BOOL bGeometryShader,
										  IMG_BOOL bNonCGInstancing,
										  IMG_UINT32 ui32Elements)
{
#if defined(SGX545)
	/* static instrs */
	IMG_UINT32 ui32Size = 5;

	/* optimisation to use immediate for vtx size */
	if ((ui32Elements * sizeof(IMG_UINT32)) > EURASIA_USE_MAXIMUM_IMMEDIATE)
	{
		ui32Size += 2;
	}
	else
	{
		ui32Size += 1;
	}

	if (!bGeometryShader)
	{
		/* different setup if instancing is used */
		if (bNonCGInstancing)
		{
			ui32Size += 2;
		}
		else
		{
			ui32Size += 1;
		}
	}

	/* Stores */
	ui32Size += ui32Elements;
	return USE_INST_TO_BYTES(ui32Size);
#else
	PVR_UNREFERENCED_PARAMETER(bGeometryShader);
	PVR_UNREFERENCED_PARAMETER(bNonCGInstancing);
	PVR_UNREFERENCED_PARAMETER(ui32Elements);
	return 0;
#endif /* SGX545 */
}

/***********************************************************************//**
 * Write the integer register store program fragment.
 *
 * @param psProg		Program parameters
 * @param pui32Buffer	Linear address to write the USE program to.
 * @return 	Linear address past the end of the USE program in the buffer
 *			pointed to by pui32Buffer.
 **************************************************************************/
IMG_PUINT32 USEGenIntRegStoreFragment(PUSEGEN_INT_REG_STORE_PROG psProg,
									  IMG_PUINT32 pui32Buffer)
{
#if defined(SGX545)
	IMG_UINT32 ui32ElementCounter = 0;
	IMG_UINT32 ui32Reg;

	IMG_UINT32 ui32VertexBaseReg = 0;
	IMG_UINT32 ui32VertexOffsetReg = 1;

	/* Determine the offset of the vertex in the buffer */
	if (psProg->bGeometryShader)
	{
		if (psProg->ui32IntRegVertexSize > EURASIA_USE_MAXIMUM_IMMEDIATE)
		{
			BuildLIMM((PPVR_USE_INST) pui32Buffer,
					  EURASIA_USE1_EPRED_ALWAYS, 0,
					  USE_REGTYPE_TEMP, ui32VertexOffsetReg,
					  psProg->ui32IntRegVertexSize);
			pui32Buffer += USE_INST_LENGTH;

			BuildIMA32((PPVR_USE_INST) pui32Buffer,
					   EURASIA_USE1_EPRED_ALWAYS, 0, 0,
					   USE_REGTYPE_TEMP, ui32VertexOffsetReg,
					   USE_REGTYPE_PRIMATTR, psProg->ui32VertexIDReg,
					   USE_REGTYPE_TEMP, ui32VertexOffsetReg, IMG_FALSE,
					   USE_REGTYPE_IMMEDIATE, 0, IMG_FALSE,
					   IMG_FALSE, 0,
					   IMG_FALSE, 0);
			pui32Buffer += USE_INST_LENGTH;
		}
		else
		{
			BuildIMA32((PPVR_USE_INST) pui32Buffer,
					   EURASIA_USE1_EPRED_ALWAYS, 0, 0,
					   USE_REGTYPE_TEMP, ui32VertexOffsetReg,
					   USE_REGTYPE_PRIMATTR, psProg->ui32VertexIDReg,
					   USE_REGTYPE_IMMEDIATE, psProg->ui32IntRegVertexSize, IMG_FALSE,
					   USE_REGTYPE_IMMEDIATE, 0, IMG_FALSE,
					   IMG_FALSE, 0,
					   IMG_FALSE, 0);
			pui32Buffer += USE_INST_LENGTH;
		}
	}
	else
	{
		if (psProg->bNonCGInstancing)
		{
			BuildLIMM((PPVR_USE_INST) pui32Buffer,
					  EURASIA_USE1_EPRED_ALWAYS, 0,
					  USE_REGTYPE_TEMP, ui32VertexOffsetReg,
					  psProg->ui32IdxCount);
			pui32Buffer += USE_INST_LENGTH;
			BuildIMA32((PPVR_USE_INST) pui32Buffer,
					   EURASIA_USE1_EPRED_ALWAYS, 0, 0,
					   USE_REGTYPE_TEMP, ui32VertexOffsetReg,
					   USE_REGTYPE_TEMP, ui32VertexOffsetReg,
					   USE_REGTYPE_PRIMATTR, psProg->ui32InstanceIDReg, IMG_FALSE,
					   USE_REGTYPE_PRIMATTR, psProg->ui32VertexIDReg, IMG_FALSE,
					   IMG_FALSE, 0,
					   IMG_FALSE, 0);
			pui32Buffer += USE_INST_LENGTH;
		}
		else
		{
			BuildMOV((PPVR_USE_INST) pui32Buffer, EURASIA_USE1_EPRED_ALWAYS, 0,
					 EURASIA_USE1_RCNTSEL,
					 USE_REGTYPE_TEMP, ui32VertexOffsetReg,
					 USE_REGTYPE_PRIMATTR, psProg->ui32VertexIDReg,
					 0);
			pui32Buffer += USE_INST_LENGTH;
		}

		if (psProg->ui32IntRegVertexSize > EURASIA_USE_MAXIMUM_IMMEDIATE)
		{
			BuildLIMM((PPVR_USE_INST) pui32Buffer,
					  EURASIA_USE1_EPRED_ALWAYS, 0,
					  USE_REGTYPE_TEMP, ui32VertexBaseReg,
					  psProg->ui32IntRegVertexSize);
			pui32Buffer += USE_INST_LENGTH;

			BuildIMA32((PPVR_USE_INST) pui32Buffer,
					   EURASIA_USE1_EPRED_ALWAYS, 0, 0,
					   USE_REGTYPE_TEMP, ui32VertexOffsetReg,
					   USE_REGTYPE_TEMP, ui32VertexBaseReg,
					   USE_REGTYPE_TEMP, ui32VertexOffsetReg, IMG_FALSE,
					   USE_REGTYPE_IMMEDIATE, 0, IMG_FALSE,
					   IMG_FALSE, 0,
					   IMG_FALSE, 0);
			pui32Buffer += USE_INST_LENGTH;
		}
		else
		{
			BuildIMA32((PPVR_USE_INST) pui32Buffer,
					   EURASIA_USE1_EPRED_ALWAYS, 0, 0,
					   USE_REGTYPE_TEMP, ui32VertexOffsetReg,
					   USE_REGTYPE_TEMP, ui32VertexOffsetReg,
					   USE_REGTYPE_IMMEDIATE, psProg->ui32IntRegVertexSize, IMG_FALSE,
					   USE_REGTYPE_IMMEDIATE, 0, IMG_FALSE,
					   IMG_FALSE, 0,
					   IMG_FALSE, 0);
			pui32Buffer += USE_INST_LENGTH;
		}
	}

	/* get the base address */
	if (psProg->bGeometryShader)
	{
		BuildLIMM((PPVR_USE_INST) pui32Buffer,
				  EURASIA_USE1_EPRED_ALWAYS, 0,
				  USE_REGTYPE_TEMP, ui32VertexBaseReg,
				  psProg->uBaseAddress.uiAddr - sizeof(IMG_UINT32));
		pui32Buffer += USE_INST_LENGTH;
	}
	else
	{
		BuildIMA32((PPVR_USE_INST) pui32Buffer,
				   EURASIA_USE1_EPRED_ALWAYS, 0, 0,
				   USE_REGTYPE_TEMP, ui32VertexBaseReg,
				   USE_REGTYPE_SECATTR, psProg->ui32BaseAddressSA,
				   USE_REGTYPE_IMMEDIATE, 1, IMG_FALSE,
				   USE_REGTYPE_IMMEDIATE, sizeof(IMG_UINT32), IMG_TRUE,
				   IMG_FALSE, 0,
				   IMG_FALSE, 0);
		pui32Buffer += USE_INST_LENGTH;
	}

	/* Get the vertex base address */
	BuildIMA32((PPVR_USE_INST) pui32Buffer,
			   EURASIA_USE1_EPRED_ALWAYS, 0, 0,
			   USE_REGTYPE_TEMP, ui32VertexBaseReg,
			   USE_REGTYPE_TEMP, ui32VertexBaseReg,
			   USE_REGTYPE_IMMEDIATE, 1, IMG_FALSE,
			   USE_REGTYPE_TEMP, ui32VertexOffsetReg, IMG_FALSE,
			   IMG_FALSE, 0,
			   IMG_FALSE, 0);
	pui32Buffer += USE_INST_LENGTH;

	for (ui32Reg = 0; ui32Reg < psProg->ui32NumRegs; ui32Reg++)
	{
		IMG_UINT32 i;
		IMG_UINT32 ui32PASrc;
		IMG_UINT32 ui32RepCount;
		IMG_UINT32 ui32LayerMask;

		ui32PASrc = psProg->psRegs[ui32Reg].ui32PASrc;
		ui32LayerMask = psProg->psRegs[ui32Reg].ui32Mask;

		ui32RepCount = 0;
		while (ui32LayerMask)
		{
			ui32RepCount++;
			ui32LayerMask = ui32LayerMask >> 1;
		}

		/* Fetch mode stores don't seem to iterate OUTPUT registers when
		 * they are sources (Hence this). */
		for (i = 0; i < ui32RepCount; i++)
		{
			BuildST((PPVR_USE_INST) pui32Buffer,
					EURASIA_USE1_EPRED_ALWAYS, 0,
					0,//EURASIA_USE1_LDST_DCCTLEXT | EURASIA_USE1_LDST_DCCTL_EXTBYPASSL1,
					EURASIA_USE1_LDST_DTYPE_32BIT,
					EURASIA_USE1_LDST_AMODE_ABSOLUTE,
					EURASIA_USE1_LDST_IMODE_NONE,
					USE_REGTYPE_TEMP, ui32VertexBaseReg,
					USE_REGTYPE_IMMEDIATE, ui32ElementCounter + i,
					USE_REGTYPE_OUTPUT, ui32PASrc + i,
					IMG_TRUE);
			pui32Buffer += USE_INST_LENGTH;
		}

		ui32ElementCounter += ui32RepCount;
	}
	USEGEN_ASSERT(psProg->ui32NumRegs > 0);

	/* Move the base address to the first integer register
	 * This should be iterated with UDONT_ITERATE so we can find it when
	 * we are loading our integer registers in the pixel shader.
	 */
	BuildMOV((PPVR_USE_INST) pui32Buffer,
			  EURASIA_USE1_EPRED_ALWAYS,
			  0,
			  EURASIA_USE1_RCNTSEL,
			  USE_REGTYPE_OUTPUT, psProg->psRegs[0].ui32PASrc,
			  USE_REGTYPE_TEMP, ui32VertexBaseReg,
			  0 /* Src mod */);
	pui32Buffer += USE_INST_LENGTH;

	/* Invalidate the cache.
	 * todo: Fix when updated to DCU fixed revision
	 */
	BuildLIMM((PPVR_USE_INST) pui32Buffer,
			  EURASIA_USE1_EPRED_ALWAYS, 0,
			  USE_REGTYPE_TEMP, ui32VertexBaseReg,
			  EUR_CR_CACHE_CTRL_INVALIDATE_MASK);
	pui32Buffer += USE_INST_LENGTH;
	BuildSTR((PPVR_USE_INST) pui32Buffer,
			 0,
			 USE_REGTYPE_TEMP, 			ui32VertexBaseReg,
			 USE_REGTYPE_IMMEDIATE,		EUR_CR_CACHE_CTRL >> 2,
			 0);
	pui32Buffer += USE_INST_LENGTH;

	return pui32Buffer;
#else
	PVR_UNREFERENCED_PARAMETER(psProg);
	return pui32Buffer;
#endif /* SGX545 */
}

/***********************************************************************//**
 * Calculates the size of the integer register load program fragment.
 *
 * @param ui32IntRegs	The number of integer registers.
 * @return	The size of the program in bytes.
 **************************************************************************/
IMG_UINT32 USEGenCalculateIntRegLoadSize(IMG_UINT32 ui32IntRegs)
{
#if defined(SGX545)
	return USE_INST_TO_BYTES(ui32IntRegs*2 + 1);
#else
	PVR_UNREFERENCED_PARAMETER(ui32IntRegs);
	return 0;
#endif /* SGX545 */
}

/***********************************************************************//**
 * Write the integer register load program fragment.
 *
 * @param psProg		Program parameters.
 * @param pui32Buffer	Linear address to write the USE program to.
 * @return	Linear address past the end of the USE program in the buffer
 *			pointed to by pui32Buffer.
 **************************************************************************/
IMG_PUINT32 USEGenIntRegLoadFragment(PUSEGEN_INT_REG_LOAD_PROG psProg,
									 IMG_PUINT32 pui32Buffer)
{
#if defined(SGX545)
	IMG_UINT32 ui32Reg;
	IMG_UINT32 ui32ElementCounter = 0;

	IMG_UINT32 ui32VertexBaseReg = 0;

	/* Get the base address from the first integer register */
	BuildMOV((PPVR_USE_INST) pui32Buffer,
		  EURASIA_USE1_EPRED_ALWAYS,
		  0,
		  EURASIA_USE1_RCNTSEL | EURASIA_USE1_SKIPINV,
		  USE_REGTYPE_TEMP, ui32VertexBaseReg,
		  USE_REGTYPE_PRIMATTR, psProg->psRegs[0].ui32PASrc,
		  0 /* Src mod */);
	pui32Buffer += USE_INST_LENGTH;

	for (ui32Reg = 0; ui32Reg < psProg->ui32NumRegs; ui32Reg++)
	{
		IMG_UINT32 ui32RepCount;
		IMG_UINT32 ui32LayerMask;
		IMG_UINT32 ui32PASrc;

		ui32LayerMask = psProg->psRegs[ui32Reg].ui32Mask;
		ui32PASrc = psProg->psRegs[ui32Reg].ui32PASrc;

		ui32RepCount = 0;
		while (ui32LayerMask)
		{
			ui32RepCount++;
			ui32LayerMask = ui32LayerMask >> 1;
		}

		BuildLD((PPVR_USE_INST) pui32Buffer,
				EURASIA_USE1_EPRED_ALWAYS, (ui32RepCount - 1),
				0,//EURASIA_USE1_LDST_DCCTLEXT | EURASIA_USE1_LDST_DCCTL_EXTBYPASSL1,
				0,
				EURASIA_USE1_LDST_DTYPE_32BIT,
				EURASIA_USE1_LDST_AMODE_ABSOLUTE,
				EURASIA_USE1_LDST_IMODE_NONE,
				USE_REGTYPE_PRIMATTR, ui32PASrc,
				USE_REGTYPE_TEMP, ui32VertexBaseReg,
				USE_REGTYPE_IMMEDIATE, ui32ElementCounter,
				USE_REGTYPE_IMMEDIATE, 0,
				IMG_TRUE);
		pui32Buffer += USE_INST_LENGTH;
		BuildWDF((PPVR_USE_INST) pui32Buffer, 0);
		pui32Buffer += USE_INST_LENGTH;

		ui32ElementCounter += ui32RepCount;
	}

	return pui32Buffer;
#else
	PVR_UNREFERENCED_PARAMETER(psProg);
	return pui32Buffer;
#endif /* SGX545 */
}

#endif /* USEGEN_BUILD_D3DVISTA */

