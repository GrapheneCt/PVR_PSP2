/*************************************************************************
 * Name         : asm.c
 * Title        : Assembler, converts instructions into USEASM input
 * Created      : Nov 2006
 *
 * Copyright    : 2002-2006 by Imagination Technologies Limited. All rights reserved.
 *              : No part of this software, either material or conceptual 
 *              : may be copied or distributed, transmitted, transcribed,
 *              : stored in a retrieval system or translated into any 
 *              : human or computer language in any form by any means,
 *              : electronic, mechanical, manual or other-wise, or 
 *              : disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited, Unit 8, HomePark
 *              : Industrial Estate, King's Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Modifications:-
 * $Log: asm.c $
 * 
 *  --- Revision Logs Removed --- 
 * 
 *  --- Revision Logs Removed --- 
 **************************************************************************/
#include "uscshrd.h"
#include "bitops.h"

#if defined(UF_TESTBENCH) && !defined(DEBUG)
/*
	assert() is undefined on release builds, and the ASSERT macro just does
	assert() on UF_TESTBENCH builds. Function DropAlpha is only referenced 
	within ASSERTs, so it never gets called on a release UF_TESTBENCH builds!
	
	Therefore, we must disable the warning 'unreferenced local function has 
	been removed'
*/
#pragma warning(disable : 4505)
#endif	/* #if defined(UF_TESTBENCH) && !defined(DEBUG) */

static IMG_VOID CopySource(PINTERMEDIATE_STATE psState,
						   PINST psIn, 
						   IMG_UINT32 uArg, 
						   PUSE_REGISTER psOutSrc);

/*********************************************************************************
 Function			: EncodeEfoArith

 Description		: Encode the arithmetic parameters for an efo instruction on a
					  processor with the old EFO options.
 
 Parameters			: psState	- Internal compiler state
					  psEfo			- The efo parameters.

 Globals Effected	: None

 Return				: The encoded parameters.
*********************************************************************************/
static IMG_UINT32 EncodeEfoArith(PINTERMEDIATE_STATE psState, PEFO_PARAMETERS psEfo)
{
	IMG_UINT32	uEfo = 0;

	#ifdef DEBUG
	/* psState used to handle failed ASSERT on release build only */
	PVR_UNREFERENCED_PARAMETER(psState);
	#endif

	if (psEfo->bM0Used)
	{
		switch (psEfo->eM0Src0)
		{
			case SRC0:
			{
				ASSERT(psEfo->eM0Src1 == SRC1);
				ASSERT(!psEfo->bM1Used || psEfo->eM1Src0 == SRC0);
				if (!psEfo->bM1Used || psEfo->eM1Src1 == SRC2)
				{
					uEfo |= (EURASIA_USE1_EFO_MSRC_SRC0SRC1_SRC0SRC2 << EURASIA_USE1_EFO_MSRC_SHIFT);
				}
				else
				{
					ASSERT(psEfo->eM1Src1 == SRC0);
					uEfo |= (EURASIA_USE1_EFO_MSRC_SRC0SRC1_SRC0SRC0 << EURASIA_USE1_EFO_MSRC_SHIFT);
				}
				break;
			}
			case SRC1:
			{
				if (psEfo->eM0Src1 == SRC2)
				{
					ASSERT(!psEfo->bM1Used || psEfo->eM1Src0 == SRC0);
					ASSERT(!psEfo->bM1Used || psEfo->eM1Src1 == SRC0);
					uEfo |= (EURASIA_USE1_EFO_MSRC_SRC1SRC2_SRC0SRC0 << EURASIA_USE1_EFO_MSRC_SHIFT);
				}
				else
				{
					ASSERT(psEfo->eM0Src1 == I0);
					ASSERT(!psEfo->bM1Used || psEfo->eM1Src0 == SRC0);
					ASSERT(!psEfo->bM1Used || psEfo->eM1Src1 == I1);
					uEfo |= (EURASIA_USE1_EFO_MSRC_SRC1I0_SRC0I1 << EURASIA_USE1_EFO_MSRC_SHIFT);
				}
				break;
			}
			default: imgabort();
		}
	}
	else if (psEfo->bM1Used)
	{
		ASSERT(psEfo->eM1Src0 == SRC0);
		switch (psEfo->eM1Src1)
		{
			case SRC2: uEfo |= (EURASIA_USE1_EFO_MSRC_SRC0SRC1_SRC0SRC2 << EURASIA_USE1_EFO_MSRC_SHIFT); break;
			case SRC0: uEfo |= (EURASIA_USE1_EFO_MSRC_SRC0SRC1_SRC0SRC0 << EURASIA_USE1_EFO_MSRC_SHIFT); break;
			case I1: uEfo |= (EURASIA_USE1_EFO_MSRC_SRC1I0_SRC0I1 << EURASIA_USE1_EFO_MSRC_SHIFT); break;
			default: imgabort();
		}
	}
	if (psEfo->bA0Used)
	{
		switch (psEfo->eA0Src0)
		{
			case M0:
			{
				switch (psEfo->eA0Src1)
				{	
					case M1:
					{
						ASSERT(!psEfo->bA1Used || psEfo->eA1Src0 == I1);
						ASSERT(!psEfo->bA1Used || psEfo->eA1Src1 == I0);
						uEfo |= (EURASIA_USE1_EFO_ASRC_M0M1_I1I0 << EURASIA_USE1_EFO_ASRC_SHIFT);
						break;
					}
					case SRC2:
					{
						ASSERT(!psEfo->bA1Used || psEfo->eA1Src0 == I1);
						ASSERT(!psEfo->bA1Used || psEfo->eA1Src1 == I0);
						uEfo |= (EURASIA_USE1_EFO_ASRC_M0SRC2_I1I0 << EURASIA_USE1_EFO_ASRC_SHIFT);
						break;
					}
					case I0:
					{
						ASSERT(!psEfo->bA1Used || psEfo->eA1Src0 == I1);
						ASSERT(!psEfo->bA1Used || psEfo->eA1Src1 == M1);
						uEfo |= (EURASIA_USE1_EFO_ASRC_M0I0_I1M1 << EURASIA_USE1_EFO_ASRC_SHIFT);
						break;
					}
					default: imgabort();
				}
				break;
			}
			case SRC0:
			{
				ASSERT(psEfo->eA0Src1 == SRC1);
				ASSERT(!psEfo->bA1Used || psEfo->eA1Src0 == SRC2);
				ASSERT(!psEfo->bA1Used || psEfo->eA1Src1 == SRC0);
				uEfo |= (EURASIA_USE1_EFO_ASRC_SRC0SRC1_SRC2SRC0 << EURASIA_USE1_EFO_ASRC_SHIFT);
				break;
			}
			default: imgabort();
		}
	}
	else if (psEfo->bA1Used)
	{
		if (psEfo->eA1Src0 == SRC2)
		{
			ASSERT(psEfo->eA1Src1 == SRC0);
			uEfo |= (EURASIA_USE1_EFO_ASRC_SRC0SRC1_SRC2SRC0 << EURASIA_USE1_EFO_ASRC_SHIFT);
		}
		else
		{
			ASSERT(psEfo->eA1Src0 == I1);
			if (psEfo->eA1Src1 == I0)
			{
				uEfo |= (EURASIA_USE1_EFO_ASRC_M0M1_I1I0 << EURASIA_USE1_EFO_ASRC_SHIFT);
			}
			else
			{
				ASSERT(psEfo->eA1Src1 == M1);
				uEfo |= (EURASIA_USE1_EFO_ASRC_M0I0_I1M1 << EURASIA_USE1_EFO_ASRC_SHIFT);
			}
		}
	}
	return uEfo;
}

#if defined(SUPPORT_SGX545)
/*********************************************************************************
 Function			: EncodeNewEfoArith

 Description		: Encode the arithmetic parameters for an efo instruction on a
					  processor with the new EFO options.
 
 Parameters			: psState	- Internal compiler state
					  psEfo		- The efo parameters.

 Globals Effected	: None

 Return				: The encoded parameters.
*********************************************************************************/
static IMG_UINT32 EncodeNewEfoArith(PINTERMEDIATE_STATE psState, PEFO_PARAMETERS psEfo)
{
	IMG_UINT32	uEfo = 0;

	#ifdef DEBUG
	/* psState used to handle failed ASSERT on release build only */
	PVR_UNREFERENCED_PARAMETER(psState);
	#endif

	if (psEfo->bM0Used)
	{
		switch (psEfo->eM0Src0)
		{
			case SRC0:
			{
				ASSERT(psEfo->eM0Src1 == SRC2);
				ASSERT(!psEfo->bM1Used || (psEfo->eM1Src0 == SRC1 && psEfo->eM1Src1 == SRC2));
				uEfo |= (SGX545_USE1_EFO_MSRC_SRC0SRC2_SRC1SRC2 << EURASIA_USE1_EFO_MSRC_SHIFT);
				break;
			}
			case SRC1:
			{
				if (psEfo->eM0Src1 == SRC0)
				{
					ASSERT(!psEfo->bM1Used || (psEfo->eM1Src0 == SRC2 || psEfo->eM1Src1 == SRC2));
					uEfo |= (SGX545_USE1_EFO_MSRC_SRC1SRC0_SRC2SRC2 << EURASIA_USE1_EFO_MSRC_SHIFT);
				}
				else
				{
					ASSERT(psEfo->eM0Src1 == I0);
					ASSERT(!psEfo->bM1Used || (psEfo->eM1Src0 == SRC2 && psEfo->eM1Src1 == I1));
					uEfo |= (SGX545_USE1_EFO_MSRC_SRC1I0_SRC2I1 << EURASIA_USE1_EFO_MSRC_SHIFT);
				}
				break;
			}
			case SRC2:
			{
				ASSERT(psEfo->eM0Src1 == SRC1);
				ASSERT(!psEfo->bM1Used || (psEfo->eM1Src0 == SRC2 && psEfo->eM1Src1 == SRC2));
				uEfo |= (SGX545_USE1_EFO_MSRC_SRC2SRC1_SRC2SRC2 << EURASIA_USE1_EFO_MSRC_SHIFT);
				break;
			}
			default: imgabort();
		}
	} 
	else if (psEfo->bM1Used)
	{
		switch (psEfo->eM1Src0)
		{
			case SRC1:
			{
				ASSERT(psEfo->eM1Src1 == SRC2);
				uEfo |= (SGX545_USE1_EFO_MSRC_SRC0SRC2_SRC1SRC2 << EURASIA_USE1_EFO_MSRC_SHIFT);
				break;
			}
			case SRC2:
			{
				if (psEfo->eM1Src1 == SRC2)
				{
					uEfo |= (SGX545_USE1_EFO_MSRC_SRC2SRC1_SRC2SRC2 << EURASIA_USE1_EFO_MSRC_SHIFT);
				}
				else
				{
					ASSERT(psEfo->eM1Src1 == I1);
					uEfo |= (SGX545_USE1_EFO_MSRC_SRC1I0_SRC2I1 << EURASIA_USE1_EFO_MSRC_SHIFT);
				}
				break;
			}
			default: imgabort();
		}
	}
	if (psEfo->bA0Used)
	{
		switch (psEfo->eA0Src0)
		{
			case M0:
			{
				switch (psEfo->eA0Src1)
				{	
					case M1:
					{
						ASSERT(!psEfo->bA1Used || psEfo->eA1Src0 == I1);
						ASSERT(!psEfo->bA1Used || psEfo->eA1Src1 == I0);
						uEfo |= (SGX545_USE1_EFO_ASRC_M0M1_I1I0 << EURASIA_USE1_EFO_ASRC_SHIFT);
						break;
					}
					case SRC0:
					{
						ASSERT(!psEfo->bA1Used || psEfo->eA1Src0 == I1);
						ASSERT(!psEfo->bA1Used || psEfo->eA1Src1 == I0);
						uEfo |= (SGX545_USE1_EFO_ASRC_M0SRC0_I1I0 << EURASIA_USE1_EFO_ASRC_SHIFT);
						break;
					}
					case I0:
					{
						ASSERT(!psEfo->bA1Used || psEfo->eA1Src0 == I1);
						ASSERT(!psEfo->bA1Used || psEfo->eA1Src1 == M1);
						uEfo |= (SGX545_USE1_EFO_ASRC_M0I0_I1M1 << EURASIA_USE1_EFO_ASRC_SHIFT);
						break;
					}
					default: imgabort();
				}
				break;
			}
			case SRC0:
			{
				ASSERT(psEfo->eA0Src1 == SRC2);
				ASSERT(!psEfo->bA1Used || psEfo->eA1Src0 == SRC1);
				ASSERT(!psEfo->bA1Used || psEfo->eA1Src1 == SRC2);
				uEfo |= (SGX545_USE1_EFO_ASRC_SRC0SRC2_SRC1SRC2 << EURASIA_USE1_EFO_ASRC_SHIFT);
				break;
			}
			default: imgabort();
		}
	}
	else if (psEfo->bA1Used)
	{
		if (psEfo->eA1Src0 == SRC1)
		{
			ASSERT(psEfo->eA1Src1 == SRC2);
			uEfo |= (SGX545_USE1_EFO_ASRC_SRC0SRC2_SRC1SRC2 << EURASIA_USE1_EFO_ASRC_SHIFT);
		}
		else
		{
			ASSERT(psEfo->eA1Src0 == I1);
			if (psEfo->eA1Src1 == I0)
			{
				uEfo |= (SGX545_USE1_EFO_ASRC_M0SRC0_I1I0 << EURASIA_USE1_EFO_ASRC_SHIFT);
			}
			else
			{
				ASSERT(psEfo->eA1Src1 == M1);
				uEfo |= (SGX545_USE1_EFO_ASRC_M0I0_I1M1 << EURASIA_USE1_EFO_ASRC_SHIFT);
			}
		}
	}
	return uEfo;
}
#endif /* defined(SUPPORT_SGX545) */

static IMG_UINT32 EncodeEfo(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psInst, PEFO_PARAMETERS psEfo)
/*********************************************************************************
 Function			: EncodeEfo

 Description		: Encode the parameters for an efo instruction.
 
 Parameters			: psEfo			- The efo parameters.

 Globals Effected	: None

 Return				: The encoded parameters.
*********************************************************************************/
{
	IMG_UINT32 uEfo = 0;
	IMG_UINT32 i;

	if (psEfo->bWriteI0)
	{
		uEfo |= EURASIA_USE1_EFO_WI0EN;
	}
	if (psEfo->bWriteI1)
	{
		uEfo |= EURASIA_USE1_EFO_WI1EN;
	}

	#if defined(SUPPORT_SGX545)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NEW_EFO_OPTIONS)
	{
		uEfo |= EncodeNewEfoArith(psState, psEfo);
	}
	else
	#endif /* defined(SUPPORT_SGX545) */
	{
		uEfo |= EncodeEfoArith(psState, psEfo);
	}
	if (psEfo->bWriteI0)
	{
		switch (psEfo->eI0Src)
		{
			case A0:
			{
				if (psEfo->eI1Src == A1)
				{
					uEfo |= (EURASIA_USE1_EFO_ISRC_I0A0_I1A1 << EURASIA_USE1_EFO_ISRC_SHIFT);
				}
				else
				{
					ASSERT(!psEfo->bWriteI1 || psEfo->eI1Src == M1);
					uEfo |= (EURASIA_USE1_EFO_ISRC_I0A0_I1M1 << EURASIA_USE1_EFO_ISRC_SHIFT);
				}
				break;
			}
			case A1:
			{
				ASSERT(!psEfo->bWriteI1 || psEfo->eI1Src == A0);
				uEfo |= (EURASIA_USE1_EFO_ISRC_I0A1_I1A0 << EURASIA_USE1_EFO_ISRC_SHIFT);
				break;
			}
			case M0:
			{
				ASSERT(!psEfo->bWriteI1 || psEfo->eI1Src == M1);
				uEfo |= (EURASIA_USE1_EFO_ISRC_I0M0_I1M1 << EURASIA_USE1_EFO_ISRC_SHIFT);
				break;
			}
			default: imgabort();
		}
	}
	else if (psEfo->bWriteI1)
	{
		switch (psEfo->eI1Src)
		{
			case A0:
			{
				uEfo |= (EURASIA_USE1_EFO_ISRC_I0A1_I1A0 << EURASIA_USE1_EFO_ISRC_SHIFT);
				break;
			}
			case A1:
			{
				uEfo |= (EURASIA_USE1_EFO_ISRC_I0A0_I1A1 << EURASIA_USE1_EFO_ISRC_SHIFT);
				break;
			}
			case M1:
			{
				uEfo |= (EURASIA_USE1_EFO_ISRC_I0M0_I1M1 << EURASIA_USE1_EFO_ISRC_SHIFT);
				break;
			}
			default: imgabort();
		}
	}
	if (!psEfo->bIgnoreDest)
	{
		switch (psEfo->eDSrc)
		{
			case I0: uEfo |= (EURASIA_USE1_EFO_DSRC_I0 << EURASIA_USE1_EFO_DSRC_SHIFT); break;
			case I1: uEfo |= (EURASIA_USE1_EFO_DSRC_I1 << EURASIA_USE1_EFO_DSRC_SHIFT); break;
			case A0: uEfo |= (EURASIA_USE1_EFO_DSRC_A0 << EURASIA_USE1_EFO_DSRC_SHIFT); break;
			case A1: uEfo |= (EURASIA_USE1_EFO_DSRC_A1 << EURASIA_USE1_EFO_DSRC_SHIFT); break;
			default: imgabort();
		}
	}
	else
	{
		/*
			Set the destination to something defined to help the hardware and simulator match.
		*/
		if (psEfo->bI0Used)
		{
			uEfo |= (EURASIA_USE1_EFO_DSRC_I0 << EURASIA_USE1_EFO_DSRC_SHIFT);
		}
		else if (psEfo->bI1Used)
		{
			uEfo |= (EURASIA_USE1_EFO_DSRC_I1 << EURASIA_USE1_EFO_DSRC_SHIFT);
		}
		else
		{
			ASSERT(psEfo->bWriteI0 || psEfo->bWriteI1);

			if (psEfo->bA0Used)
			{
				uEfo |= (EURASIA_USE1_EFO_DSRC_A0 << EURASIA_USE1_EFO_DSRC_SHIFT);
			}
			else if (psEfo->bA1Used)
			{
				uEfo |= (EURASIA_USE1_EFO_DSRC_A1 << EURASIA_USE1_EFO_DSRC_SHIFT);
			}
			else
			{
				uEfo |= (EURASIA_USE1_EFO_DSRC_A0 << EURASIA_USE1_EFO_DSRC_SHIFT);
				#if defined(SUPPORT_SGX545)
				if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NEW_EFO_OPTIONS)
				{
					uEfo |= (SGX545_USE1_EFO_ASRC_SRC0SRC2_SRC1SRC2 << EURASIA_USE1_EFO_ASRC_SHIFT);
				}
				else
				#endif /* defined(SUPPORT_SGX545) */
				{
					uEfo |= (EURASIA_USE1_EFO_ASRC_SRC0SRC1_SRC2SRC0 << EURASIA_USE1_EFO_ASRC_SHIFT);
				}
			}
		}
	}

	/*
		Duplicate a used source into used sources.
	*/
	for (i = 0; i < 3; i++)
	{
		if (psInst->asArg[i + 1].uType == USC_REGTYPE_UNUSEDSOURCE)
		{
			IMG_UINT32	uSrcIdx;
			
			/*
				Look for another source.
			*/
			for (uSrcIdx = 0; uSrcIdx < 2; uSrcIdx++)
			{
				IMG_UINT32		uOtherSrc = ((i + uSrcIdx) % 3) + 1;
				PUSE_REGISTER	psOtherSrc = &psInst->asArg[uOtherSrc];

				if (psOtherSrc->uType != USC_REGTYPE_UNUSEDSOURCE)
				{
					/*
						Check that the other source fits with the bank restrictions.
					*/	
					if (!CanUseSrc(psState, psIn, i, psOtherSrc->uType, psOtherSrc->uIndex))
					{
						continue;
					}

					psInst->asArg[i + 1] = *psOtherSrc;
					break;
				}
			}
			/*
				If no other source could be duplicated then use an arbitary register.
			*/
			if (uSrcIdx == 2)
			{
				psInst->asArg[i + 1].uType = USEASM_REGTYPE_PRIMATTR;
				psInst->asArg[i + 1].uNumber = 0;
				psInst->asArg[i + 1].uFlags = 0;
				psInst->asArg[i + 1].uIndex = USEREG_INDEX_NONE;
			}
		}
	}

	if (psEfo->bA1LeftNeg)
	{
		uEfo |= EURASIA_USE1_EFO_A1LNEG;
	}
	#if defined(SUPPORT_SGX545)
	if (psEfo->bA0RightNeg)
	{
		uEfo |= SGX545_USE1_EFO_A0RNEG;
	}
	#else /* defined(SUPPORT_SGX545) */
	ASSERT(!psEfo->bA0RightNeg);
	#endif /* defined(SUPPORT_SGX545) */

	return uEfo;
}

static const IMG_UINT32 g_auEncodeTest[TEST_TYPE_MAXIMUM] = 
{
	/* TEST_TYPE_INVALID */
	USC_UNDEF,
	/* TEST_TYPE_ALWAYS_TRUE */
	(USEASM_TEST_ZERO_TRUE		<< USEASM_TEST_ZERO_SHIFT) | 
	(USEASM_TEST_SIGN_TRUE		<< USEASM_TEST_SIGN_SHIFT) | 
	USEASM_TEST_CRCOMB_AND,
	/* TEST_TYPE_GT_ZERO */
	(USEASM_TEST_ZERO_NONZERO	<< USEASM_TEST_ZERO_SHIFT) |
	(USEASM_TEST_SIGN_POSITIVE	<< USEASM_TEST_SIGN_SHIFT) |
	USEASM_TEST_CRCOMB_AND,
	/* TEST_TYPE_GTE_ZERO */
	(USEASM_TEST_ZERO_ZERO		<< USEASM_TEST_ZERO_SHIFT) |
	(USEASM_TEST_SIGN_POSITIVE	<< USEASM_TEST_SIGN_SHIFT) |
	USEASM_TEST_CRCOMB_OR,
	/* TEST_TYPE_EQ_ZERO */
	(USEASM_TEST_ZERO_ZERO		<< USEASM_TEST_ZERO_SHIFT) |
	(USEASM_TEST_SIGN_TRUE		<< USEASM_TEST_SIGN_SHIFT) |
	USEASM_TEST_CRCOMB_AND,
	/* TEST_TYPE_LT_ZERO */
	(USEASM_TEST_ZERO_NONZERO	<< USEASM_TEST_ZERO_SHIFT) |
	(USEASM_TEST_SIGN_NEGATIVE	<< USEASM_TEST_SIGN_SHIFT) |
	USEASM_TEST_CRCOMB_AND,
	/* TEST_TYPE_LTE_ZERO */
	(USEASM_TEST_ZERO_ZERO		<< USEASM_TEST_ZERO_SHIFT) |
	(USEASM_TEST_SIGN_NEGATIVE	<< USEASM_TEST_SIGN_SHIFT) |
	USEASM_TEST_CRCOMB_OR,
	/* TEST_TYPE_NEQ_ZERO */
	(USEASM_TEST_ZERO_NONZERO	<< USEASM_TEST_ZERO_SHIFT) |
	(USEASM_TEST_SIGN_TRUE		<< USEASM_TEST_SIGN_SHIFT) |
	USEASM_TEST_CRCOMB_AND,
	/* TEST_TYPE_SIGN_BIT_CLEAR */
	(USEASM_TEST_SIGN_POSITIVE	<< USEASM_TEST_SIGN_SHIFT) |
	(USEASM_TEST_ZERO_TRUE		<< USEASM_TEST_ZERO_SHIFT) |
	USEASM_TEST_CRCOMB_AND,
	/* TEST_TYPE_SIGN_BIT_SET */
	(USEASM_TEST_SIGN_NEGATIVE	<< USEASM_TEST_SIGN_SHIFT) |
	(USEASM_TEST_ZERO_TRUE		<< USEASM_TEST_ZERO_SHIFT) |
	USEASM_TEST_CRCOMB_AND,
};

static IMG_UINT32 ConvertTestTypeToUseasm(PINTERMEDIATE_STATE psState, TEST_TYPE eTestType)
/*********************************************************************************
 Function			: ConvertTestToUseasm

 Description		: Convert a hardware test specification to the USEASM input
					  format.
 
 Parameters			: psState	- Compiler state.
					  psTest	- The test to encode.

 Globals Effected	: None

 Return				: The encoded test.
*********************************************************************************/
{
	ASSERT(eTestType < (sizeof(g_auEncodeTest) / sizeof(g_auEncodeTest[0])));
	return g_auEncodeTest[eTestType];
}

static IMG_UINT32 ConvertTestToUseasm(PINTERMEDIATE_STATE psState, PTEST_DETAILS psTest)
/*********************************************************************************
 Function			: ConvertTestToUseasm

 Description		: Convert a hardware test specification to the USEASM input
					  format.
 
 Parameters			: psState	- Compiler state.
					  psTest	- The test to encode.

 Globals Effected	: None

 Return				: The encoded test.
*********************************************************************************/
{
	IMG_UINT32	uTest;

	uTest = ConvertTestTypeToUseasm(psState, psTest->eType);
	uTest |= (psTest->eChanSel << USEASM_TEST_CHANSEL_SHIFT);
	uTest |= (psTest->eMaskType << USEASM_TEST_MASK_SHIFT);
	return uTest;
}

/*********************************************************************************
 Function			: DropAlpha

 Description		: Drop the alpha part (if it exists) from a source select.
 
 Parameters			: uIntSrcSel	- The source select.

 Globals Effected	: None

 Return				: The source select without alpha.
*********************************************************************************/
static IMG_UINT32 DropAlpha(IMG_UINT32	uIntSrcSel)
{
	switch (uIntSrcSel)
	{
		case USEASM_INTSRCSEL_SRC0ALPHA: return USEASM_INTSRCSEL_SRC0;
		case USEASM_INTSRCSEL_SRC1ALPHA: return USEASM_INTSRCSEL_SRC1;
		case USEASM_INTSRCSEL_SRC2ALPHA: return USEASM_INTSRCSEL_SRC2;
		default: return uIntSrcSel;
	}
}

/*********************************************************************************
 Function			: CopyImmediate

 Description		: Initialize a USEASM argument structure for an immediate
					  source argument.
 
 Parameters			: psOutSrc		- Points to the structure to initialize.
					  uValue		- Immediate value.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
static
IMG_VOID CopyImmediate(PUSE_REGISTER psOutSrc, IMG_UINT32 uValue)
{
	psOutSrc->uType = USEASM_REGTYPE_IMMEDIATE;
	psOutSrc->uNumber = uValue;
	psOutSrc->uIndex = USEREG_INDEX_NONE;
	psOutSrc->uFlags = 0;
}

/*********************************************************************************
 Function			: CopyFPInstParam

 Description		: Initialize a USEASM argument structure representing a
				      parameter to one of the fixed point 8/10-bit arithmetic
					  instructions.
 
 Parameters			: psOutSrc		- Points to the structure to initialize.
					  eParameter	- Parameter value.
					  bComplement	- If TRUE set the complement modifier
									on the parameter.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
static
IMG_VOID CopyFPInstParam(PUSE_REGISTER psOutSrc, USEASM_INTSRCSEL eParameter, IMG_BOOL bComplement)
{
	psOutSrc->uType = USEASM_REGTYPE_INTSRCSEL;
	psOutSrc->uNumber = eParameter;
	psOutSrc->uIndex = USEREG_INDEX_NONE;
	psOutSrc->uFlags = 0;
	if (bComplement)
	{
		psOutSrc->uFlags |= USEASM_ARGFLAGS_COMPLEMENT;
	}
}

/*********************************************************************************
 Function			: CopyFPComplementFlag

 Description		: Initialize a USEASM argument structure representing a
				      whether an argument to one of the fixed point 8/10-bit arithmetic
					  instructions is complemented or not.
 
 Parameters			: psOutSrc		- Points to the structure to initialize.
					  bComplement	- Complement flag.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
static IMG_VOID CopyFPComplementFlag(PUSE_REGISTER psOutSrc, IMG_BOOL bComplement)
{
	psOutSrc->uType = USEASM_REGTYPE_INTSRCSEL;
	psOutSrc->uNumber = bComplement ? USEASM_INTSRCSEL_COMP : USEASM_INTSRCSEL_NONE;
	psOutSrc->uIndex = USEREG_INDEX_NONE;
	psOutSrc->uFlags = 0;
}

/*********************************************************************************
 Function			: EncodeSOPWMModes

 Description		: Encode the extra parameters for a SOPWM instruction.
 
 Parameters			: psIn		- Input instruction.
					  psOutSrc	- Destination for the extra parameters.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
static IMG_VOID EncodeSOPWMModes(PINST psIn, PUSE_REGISTER psOutSrc)
{
	CopyFPInstParam(psOutSrc, psIn->u.psSopWm->uSel1, psIn->u.psSopWm->bComplementSel1);
	psOutSrc++;

	CopyFPInstParam(psOutSrc, psIn->u.psSopWm->uSel2, psIn->u.psSopWm->bComplementSel2);
	psOutSrc++;

	CopyFPInstParam(psOutSrc, psIn->u.psSopWm->uCop, IMG_FALSE /* bComplement */);
	psOutSrc++;

	CopyFPInstParam(psOutSrc, psIn->u.psSopWm->uAop, IMG_FALSE /* bComplement */);
}

/*********************************************************************************
 Function			: EncodeFPMAModes

 Description		: Encode the extra parameters for a FPMA instruction.
 
 Parameters			: psIn		- Input instruction.
					  psOutSrc	- Destination for the extra parameters.
					  psOut		- Output instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
static IMG_VOID EncodeFPMAModes(PINST psIn, PUSE_REGISTER psOutSrc, PUSE_INST psOut)
{
	if (psIn->u.psFpma->bSaturate)
	{
		psOut->uFlags2 |= USEASM_OPFLAGS2_SAT;
	}

	CopyFPInstParam(psOutSrc, psIn->u.psFpma->uCSel0, psIn->u.psFpma->bComplementCSel0);
	psOutSrc++;

	CopyFPInstParam(psOutSrc, psIn->u.psFpma->uCSel1, psIn->u.psFpma->bComplementCSel1);
	psOutSrc++;

	CopyFPInstParam(psOutSrc, psIn->u.psFpma->uCSel2, psIn->u.psFpma->bComplementCSel2);
	psOutSrc++;

	CopyFPInstParam(psOutSrc, psIn->u.psFpma->uASel0, psIn->u.psFpma->bComplementASel0);
	psOutSrc++;

	CopyFPInstParam(psOutSrc, USEASM_INTSRCSEL_SRC1ALPHA /* eParameter */, psIn->u.psFpma->bComplementASel1);
	psOutSrc++;

	CopyFPInstParam(psOutSrc, USEASM_INTSRCSEL_SRC2ALPHA /* eParameter */, psIn->u.psFpma->bComplementASel2);
}

/*********************************************************************************
 Function			: EncodeSOP2Modes

 Description		: Encode the extra parameters for a SOP2 instruction.
 
 Parameters			: psIn		- Input instruction.
					  psOutSrc	- Destination for the extra parameters.
					  psOut		- Output instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
static IMG_VOID EncodeSOP2Modes(PINST psIn, PUSE_REGISTER psOutSrc, PUSE_INST psOut)
{
	/*
		Source selects for the colour part of the instruction.
	*/
	psOut->uFlags1 |= USEASM_OPFLAGS1_MAINISSUE;

	CopyFPComplementFlag(psOutSrc, psIn->u.psSop2->bComplementCSrc1);
	psOutSrc++;

	CopyFPInstParam(psOutSrc, psIn->u.psSop2->uCSel1, psIn->u.psSop2->bComplementCSel1);
	psOutSrc++;

	CopyFPInstParam(psOutSrc, psIn->u.psSop2->uCSel2, psIn->u.psSop2->bComplementCSel2);
	psOutSrc++;

	CopyFPInstParam(psOutSrc, psIn->u.psSop2->uCOp, IMG_FALSE /* bComplement */);

	/*
		Source selects for the alpha part of the instruction.
	*/
	psOut->psNext->uOpcode = USEASM_OP_ASOP2;

	psOutSrc = psOut->psNext->asArg;

	CopyFPComplementFlag(psOutSrc, psIn->u.psSop2->bComplementASrc1);
	psOutSrc++;

	CopyFPInstParam(psOutSrc, psIn->u.psSop2->uASel1, psIn->u.psSop2->bComplementASel1);
	psOutSrc++;

	CopyFPInstParam(psOutSrc, psIn->u.psSop2->uASel2, psIn->u.psSop2->bComplementASel2);
	psOutSrc++;

	CopyFPInstParam(psOutSrc, psIn->u.psSop2->uAOp, IMG_FALSE /* bComplement */);
	psOutSrc++;

	/*
		Alpha destination modifier - we never use this.
	*/
	CopyFPInstParam(psOutSrc, USEASM_INTSRCSEL_NONE /* eParameter */, IMG_FALSE /* bComplement */);
}

/*********************************************************************************
 Function			: EncodeLRP1Modes

 Description		: Encode the extra parameters for a LRP1 instruction.
 
 Parameters			: psIn		- Input instruction.
					  psOutSrc	- Destination for the extra parameters.
					  psOut		- Output instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
static IMG_VOID EncodeLRP1Modes(PINST psIn, PUSE_REGISTER psOutSrc, PUSE_INST psOut)
{
	/*
		Encode the colour part of the parameters.
	*/
	psOut->uFlags1 |= USEASM_OPFLAGS1_MAINISSUE;

	CopyFPInstParam(psOutSrc, psIn->u.psLrp1->uCSel10, psIn->u.psLrp1->bComplementCSel10);
	psOutSrc++;

	CopyFPInstParam(psOutSrc, psIn->u.psLrp1->uCSel11, psIn->u.psLrp1->bComplementCSel11);
	psOutSrc++;

	CopyFPInstParam(psOutSrc, psIn->u.psLrp1->uCS, IMG_FALSE /* bComplement */);

	/*
		Encode the alpha part.
	*/
	psOut->psNext->uOpcode = USEASM_OP_ASOP;
	psOutSrc = psOut->psNext->asArg;

	CopyFPInstParam(psOutSrc, psIn->u.psLrp1->uASel1, psIn->u.psLrp1->bComplementASel1);
	psOutSrc++;

	CopyFPInstParam(psOutSrc, psIn->u.psLrp1->uASel2, psIn->u.psLrp1->bComplementASel2);
	psOutSrc++;

	CopyFPInstParam(psOutSrc, psIn->u.psLrp1->uAOp, IMG_FALSE /* bComplement */);
}

/*********************************************************************************
 Function			: CopyDOT34DestModToUseasm

 Description		: Encode the extra parameters for a DOT34 instruction.
 
 Parameters			: psIn		- Input instruction.
					  psOutSrc	- Destination for the extra parameters.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
static PUSE_REGISTER CopyDOT34DestModToUseasm(PINST psIn, PUSE_REGISTER psOutSrc)
{
	/*
		Copy the scale factor for the colour result.
	*/
	psOutSrc->uType = USEASM_REGTYPE_INTSRCSEL;
	psOutSrc->uNumber = USEASM_INTSRCSEL_CX1 + psIn->u.psDot34->uDot34Scale;
	psOutSrc->uIndex = USEREG_INDEX_NONE;
	psOutSrc->uFlags = 0;
	psOutSrc++;

	/*
		Copy the scalar factor for the alpha result.
	*/
	psOutSrc->uType = USEASM_REGTYPE_INTSRCSEL;
	psOutSrc->uNumber = USEASM_INTSRCSEL_AX1 + psIn->u.psDot34->uDot34Scale;
	psOutSrc->uIndex = USEREG_INDEX_NONE;
	psOutSrc->uFlags = 0;
	psOutSrc++;

	return psOutSrc;
}

#if defined(SUPPORT_SGX545)
/*********************************************************************************
 Function			: ConvertDualIssueSecondaryToUseasm

 Description		: Encode the secondary operation to a dual-issue instruction.
 
 Parameters			: psState		- Compiler state.
					  psIn			- Input instruction.
					  psOut			- Instruction to encode into.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
static IMG_VOID ConvertDualIssueSecondaryToUseasm(PINTERMEDIATE_STATE	psState,
												  PINST					psIn,
												  PUSE_INST				psOut)
{
	PDUAL_PARAMS	psDual = psIn->u.psDual;
	IMG_UINT32		uArg;
	IOPCODE			eSecOp = psDual->eSecondaryOp;
	PUSE_REGISTER	psOutSrc = psOut->asArg;
	IMG_UINT32		uSecondaryGPIDest;

	ASSERT(g_psInstDesc[eSecOp].uUseasmOpcode != (IMG_UINT32)-1);
	psOut->uOpcode = g_psInstDesc[eSecOp].uUseasmOpcode;

	psOut->uFlags1 = psOut->uFlags2 = psOut->uFlags3 = 0;
	
	/*
		Secondary destination.
	*/
	ASSERT(psIn->asDest[DUAL_SECONDARY_DESTINDEX].uType == USEASM_REGTYPE_FPINTERNAL);
	uSecondaryGPIDest = psIn->asDest[1].uNumber;

	psOutSrc->uType = USEASM_REGTYPE_FPINTERNAL;
	ASSERT(uSecondaryGPIDest <= SGX545_USE1_FDUAL_OP2DSTS0_MAXIMUM_INTERNAL_REGISTER);
	psOutSrc->uNumber = uSecondaryGPIDest;
	psOutSrc->uIndex = USEREG_INDEX_NONE;
	psOutSrc->uFlags = 0;
	psOutSrc++;

	/*
		Indicate no secondary destination for IMA32.
	*/
	if (eSecOp == IIMA32)
	{
		psOutSrc->uNumber = USEASM_INTSRCSEL_NONE;
		psOutSrc->uType = USEASM_REGTYPE_INTSRCSEL;
		psOutSrc->uIndex = USEREG_INDEX_NONE;
		psOutSrc->uFlags = 0;
		psOutSrc++;
	}

	/*
		Secondary sources.
	*/
	for (uArg = 0; uArg < g_psInstDesc[eSecOp].uDefaultArgumentCount; uArg++, psOutSrc++)
	{
		DUAL_SEC_SRC	eSrc = psDual->aeSecondarySrcs[uArg];

		if (eSrc == DUAL_SEC_SRC_I0 ||
			eSrc == DUAL_SEC_SRC_I1 ||
			eSrc == DUAL_SEC_SRC_I2)
		{
			/*
				Reference the internal register directly.
			*/
			psOutSrc->uType = USEASM_REGTYPE_FPINTERNAL;
			psOutSrc->uNumber = eSrc - DUAL_SEC_SRC_I0;
			psOutSrc->uIndex = USEREG_INDEX_NONE;
			psOutSrc->uFlags = 0;
		}
		else
		{
			IMG_UINT32	uSrcIdx;

			ASSERT(eSrc == DUAL_SEC_SRC_PRI0 ||
				   eSrc == DUAL_SEC_SRC_PRI1 ||
				   eSrc == DUAL_SEC_SRC_PRI2);

			uSrcIdx = eSrc - DUAL_SEC_SRC_PRI0;
			if (psDual->eSecondaryOp == IMOV &&
				!(psDual->ePrimaryOp == IFSSQ && uSrcIdx == 0) &&
				!(psDual->ePrimaryOp == IMOV && uSrcIdx != 1))
			{
				/*
					For a secondary MOV using a source also used by the
					primary operation just specify the source we want.
				*/
				psOutSrc->uType = USEASM_REGTYPE_INTSRCSEL;
				psOutSrc->uNumber = USEASM_INTSRCSEL_SRC0 + uSrcIdx;
				psOutSrc->uIndex = USEREG_INDEX_NONE;
				psOutSrc->uFlags = 0;
			}
			else
			{
				CopySource(psState, psIn, uSrcIdx, psOutSrc);
				psOutSrc->uFlags &= ~USEASM_ARGFLAGS_NEGATE;
			}
		}
	}

	/*
		Indicate no carry-in for IMA32.
	*/
	if (eSecOp == IIMA32)
	{
		psOutSrc->uNumber = USEASM_INTSRCSEL_NONE;
		psOutSrc->uType = USEASM_REGTYPE_INTSRCSEL;
		psOutSrc->uIndex = USEREG_INDEX_NONE;
		psOutSrc->uFlags = 0;
		psOutSrc++;
	}
}
#endif /* defined(SUPPORT_SGX545) */

/*********************************************************************************
 Function			: EncodeSOP3Modes

 Description		: Encode the extra parameters for a SOP3 instruction.
 
 Parameters			: psIn		- Input instruction.
					  psOutSrc	- Destination for the extra parameters.
					  psOut		- Output instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
static IMG_VOID EncodeSOP3Modes(PINTERMEDIATE_STATE psState,
								PINST psIn, 
								PUSE_REGISTER psOutSrc, 
								PUSE_INST psOut)
{
	USEASM_INTSRCSEL	eNegateCResult;

	#ifdef DEBUG
	/* psState used to handle failed ASSERT on release build only */
	PVR_UNREFERENCED_PARAMETER(psState);
	#endif

	/*
		Encode the colour part of the parameters.
	*/
	psOut->uFlags1 |= USEASM_OPFLAGS1_MAINISSUE;

	CopyFPInstParam(psOutSrc, psIn->u.psSop3->uCSel1, psIn->u.psSop3->bComplementCSel1);
	psOutSrc++;

	CopyFPInstParam(psOutSrc, psIn->u.psSop3->uCSel2, psIn->u.psSop3->bComplementCSel2);
	psOutSrc++;

	CopyFPInstParam(psOutSrc, psIn->u.psSop3->uCOp, IMG_FALSE /* bComplement */);
	psOutSrc++;

	eNegateCResult = psIn->u.psSop3->bNegateCResult ? USEASM_INTSRCSEL_NEG : USEASM_INTSRCSEL_NONE;
	CopyFPInstParam(psOutSrc, eNegateCResult, IMG_FALSE /* bComplement */);

	psOutSrc = psOut->psNext->asArg;

	/*
		Encode the alpha part.
	*/
	if (psIn->u.psSop3->uCoissueOp == USEASM_OP_ASOP)
	{
		if (!psIn->u.psSop3->bNegateAResult)
		{
			psOut->psNext->uOpcode = USEASM_OP_ASOP;

			ASSERT(DropAlpha(psIn->u.psSop3->uASel2) == DropAlpha(psIn->u.psSop3->uCSel2));
			ASSERT(psIn->u.psSop3->bComplementASel2 == psIn->u.psSop3->bComplementCSel2);

			CopyFPInstParam(psOutSrc, psIn->u.psSop3->uASel1, psIn->u.psSop3->bComplementASel1);
			psOutSrc++;
	
			CopyFPInstParam(psOutSrc, psIn->u.psSop3->uAOp, IMG_FALSE /* bComplement */);
		}
		else
		{
			psOut->psNext->uOpcode = USEASM_OP_ARSOP;

			ASSERT(DropAlpha(psIn->u.psSop3->uASel1) == DropAlpha(psIn->u.psSop3->uCSel2));
			ASSERT(psIn->u.psSop3->bComplementASel1 == psIn->u.psSop3->bComplementCSel2);

			CopyFPInstParam(psOutSrc, psIn->u.psSop3->uASel2, psIn->u.psSop3->bComplementASel2);
			psOutSrc++;

			CopyFPInstParam(psOutSrc, psIn->u.psSop3->uAOp, IMG_FALSE /* bComplement */);
		}
	}
	else
	{
		ASSERT(psIn->u.psSop3->uCoissueOp == USEASM_OP_ALRP);
		ASSERT(!psIn->u.psSop3->bNegateAResult);
		ASSERT(psIn->u.psSop3->uAOp == USEASM_INTSRCSEL_ADD);

		psOut->psNext->uOpcode = USEASM_OP_ALRP;

		CopyFPInstParam(psOutSrc, psIn->u.psSop3->uASel1, psIn->u.psSop3->bComplementASel1);
		psOutSrc++;

		CopyFPInstParam(psOutSrc, psIn->u.psSop3->uASel2, psIn->u.psSop3->bComplementASel2);
	}
}

static IMG_UINT32 EncodePred(PINTERMEDIATE_STATE	psState,
							 IMG_UINT32				uPredSrc, 
							 IMG_BOOL				bPredNegate,
							 IMG_BOOL				bPredPerChan)
/*****************************************************************************
 FUNCTION	: EncodePred
    
 PURPOSE	: Encode a predicate in the USEASM format.

 PARAMETERS	: psState		- Internal compiler state
			  uPredSrc		- Predicate register number.
			  bPredNegate	- Predicate negation flag.
			  
 RETURNS	: The encoded predicate.
*****************************************************************************/
{
	IMG_UINT32	uUseasmPred = USEASM_PRED_NONE;

	ASSERT(!(bPredNegate && bPredPerChan));

	if	(uPredSrc != USC_PREDREG_NONE)
	{
		if	(bPredNegate)
		{
			switch (uPredSrc)
			{
				case 0:
				{
					uUseasmPred = USEASM_PRED_NEGP0;
					break;
				}
				case 1: 
				{
					uUseasmPred = USEASM_PRED_NEGP1;
					break;
				}
				case 2: 
				{
					uUseasmPred = USEASM_PRED_NEGP2;
					break;
				}
				default: imgabort();
			}
		}
		else if (bPredPerChan)
		{
			return USEASM_PRED_PN;
		}
		else
		{
			switch (uPredSrc)
			{
				case 0:
				{
					uUseasmPred = USEASM_PRED_P0;
					break;
				}
				case 1:
				{
					uUseasmPred = USEASM_PRED_P1;
					break;
				}
				case 2:
				{
					uUseasmPred = USEASM_PRED_P2;
					break;
				}
				case 3:
				{
					uUseasmPred = USEASM_PRED_P3;
					break;
				}
				case USC_PREDREG_PNMOD4:
				{
					uUseasmPred = USEASM_PRED_PN;
					break;
				}
				default: imgabort();
			}
		}
	}

	return uUseasmPred;
}

/*********************************************************************************
 Function			: CopyIndex

 Description		: Convert an index value from the internal format to the USEASM
					  format.
 
 Parameters			: uIndex	- The index value to convert.

 Globals Effected	: None

 Return				: The converted index.
*********************************************************************************/
static IMG_UINT32 CopyIndex(PINTERMEDIATE_STATE psState, IMG_UINT32 uIndexType, IMG_UINT32 uIndexNumber)
{
	if (uIndexType == USC_REGTYPE_NOINDEX)
	{
		return USEREG_INDEX_NONE;
	}
	ASSERT(uIndexType == USEASM_REGTYPE_INDEX);
	switch (uIndexNumber)
	{
		case USC_INDEXREG_LOW: return USEREG_INDEX_L;
		case USC_INDEXREG_HIGH: return USEREG_INDEX_H;
		default: imgabort();
	}
}

static IMG_UINT32 ConvertRegisterNumber(PINTERMEDIATE_STATE psState,
										PCINST				psInst,
										IMG_BOOL			bDest,
										IMG_UINT32			uArgIdx)
/*********************************************************************************
 Function			: ConvertRegisterNumber

 Description		: Convert a register number from the internal format to the USEASM
					  format.
 
 Parameters			: psState	- Internal compiler state
					  psInst	- Instruction whose source or destination argument
								register number is to be converted.
					  bDest		- TRUE if converting a destination.
								  FALSE if converting a source.
					  uArgIdx	- Argument whose register number is to be converted.

 Globals Effected	: None

 Return				: The converted register number.
*********************************************************************************/
{
	PARG	psArg;

	if (bDest)
	{
		ASSERT(uArgIdx < psInst->uDestCount);
		psArg = &psInst->asDest[uArgIdx];
	}
	else
	{
		ASSERT(uArgIdx < psInst->uArgumentCount);
		psArg = &psInst->asArg[uArgIdx];
	}

	#if defined(OUTPUT_USPBIN)
	if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_USPTEXTURESAMPLE) != 0)
	{
		/*
			uspbin.c:BuildSampleBlock wants to encode using the register numbers before we
			started adjusting register numbers to MOE units.
		*/
		if (
				psArg->uType == USEASM_REGTYPE_FPINTERNAL && 
				(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0
		   )
		{
			return psArg->uNumber >> (DQWORD_SIZE_LOG2 - LONG_SIZE_LOG2);
		}
		else
		{
			return psArg->uNumber;
		}
	}
	else
	#endif /* defined(OUTPUT_USPBIN) */
	{
		if (psArg->uType == USEASM_REGTYPE_IMMEDIATE ||
			psArg->uType == USEASM_REGTYPE_FPINTERNAL ||
			psArg->uType == USEASM_REGTYPE_FPCONSTANT ||
			psArg->uType == USEASM_REGTYPE_DRC ||
			psArg->uType == USEASM_REGTYPE_PREDICATE)
		{
			return psArg->uNumber;
		}
		else
		{
			IMG_UINT32	uRegisterNumberUnits;

			/*
				USEASM expects the register number to be units of 32-bits.
			*/
			uRegisterNumberUnits = GetMOEUnitsLog2(psState, psInst, bDest, uArgIdx);
			if (uRegisterNumberUnits > LONG_SIZE_LOG2)
			{
				return psArg->uNumber << (uRegisterNumberUnits - LONG_SIZE_LOG2);
			}
			else
			{
				return psArg->uNumber;
			}
		}
	}
}

/*********************************************************************************
 Function			: CopySource

 Description		: Convert a source argument from the internal format to the USEASM
					  format.
 
 Parameters			: psState	- Internal compiler state
					  uArg		- Argument number in the internal format.
					  psIn		- Pointer to the argument in the internal format.
					  psOutSrc	- Filled out with the converted source.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
static IMG_VOID CopySource(PINTERMEDIATE_STATE psState,
						   PINST psIn, 
						   IMG_UINT32 uArg, 
						   PUSE_REGISTER psOutSrc)
{
	PARG		psArg = &psIn->asArg[uArg];

	/*
		Copy the source register.
	*/
	psOutSrc->uNumber = ConvertRegisterNumber(psState, psIn, IMG_FALSE /* bDest */, uArg);
	if (psArg->uType == USC_REGTYPE_GSINPUT)
	{
		psOutSrc->uType = USEASM_REGTYPE_PRIMATTR;
	}
	else
	{
		psOutSrc->uType = psArg->uType;
	}
	psOutSrc->uIndex = CopyIndex(psState, psArg->uIndexType, psArg->uIndexNumber);
	psOutSrc->uFlags = 0;
	

	/*
		For instructions which support F16 format arguments copy the format.
	*/
	if (
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			(g_psInstDesc[psIn->eOpcode].uFlags & DESC_FLAGS_VECTORCOMPLEXOP) != 0 || 
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
			(g_psInstDesc[psIn->eOpcode].uFlags & DESC_FLAGS_F16F32SELECT) != 0
	    )
	{
		if (psArg->eFmt == UF_REGFORMAT_F16)
		{
			psOutSrc->uFlags |= USEASM_ARGFLAGS_FMTF16;
		}
	}

	/*
		For instructions which support C10 format arguments copy the format.
	*/
	if (
			HasC10FmtControl(psIn) ||
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			(g_psInstDesc[psIn->eOpcode].uFlags & DESC_FLAGS_VECTORCOMPLEXOP) != 0 || 
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
			(g_psInstDesc[psIn->eOpcode].uFlags & DESC_FLAGS_COMPLEXOP) != 0
	   )
	{
		if (psArg->eFmt == UF_REGFORMAT_C10)
		{
			psOutSrc->uFlags |= USEASM_ARGFLAGS_FMTC10;
		}
	}

	ASSERT(!(psIn->eOpcode == IMOV && psArg->uType == USEASM_REGTYPE_IMMEDIATE && psArg->uNumber > EURASIA_USE_MAXIMUM_IMMEDIATE));
}

/*********************************************************************************
 Function			: GetUseasmOpcodeForSmp

 Description		: Get the useasm opcode for a SMP instruction.
 
 Parameters			: ppsIn		- Instruction to get the opcode for.

 Globals Effected	: None

 Return				: The opcode.
*********************************************************************************/
static IMG_UINT32 GetUseasmOpcodeForSmp(PINTERMEDIATE_STATE		psState,
										PINST					psIn)
{
	switch (psIn->eOpcode)
	{
		case ISMP:
		#if defined(OUTPUT_USPBIN)
		case ISMP_USP:
		#endif /* defined(OUTPUT_USPBIN) */
		{
			switch (psIn->u.psSmp->uDimensionality)
			{
				case 1: return USEASM_OP_SMP1D;
				case 2: return USEASM_OP_SMP2D;
				case 3: return USEASM_OP_SMP3D;
				default: imgabort();
			}
		}
		case ISMPBIAS:
		#if defined(OUTPUT_USPBIN)
		case ISMPBIAS_USP:
		#endif /* defined(OUTPUT_USPBIN) */
		{
			switch (psIn->u.psSmp->uDimensionality)
			{
				case 1: return USEASM_OP_SMP1DBIAS;
				case 2: return USEASM_OP_SMP2DBIAS;
				case 3: return USEASM_OP_SMP3DBIAS;
				default: imgabort();
			}
		}
		case ISMPREPLACE:
		#if defined(OUTPUT_USPBIN)
		case ISMPREPLACE_USP:
		#endif /* defined(OUTPUT_USPBIN) */
		{
			switch (psIn->u.psSmp->uDimensionality)
			{
				case 1: return USEASM_OP_SMP1DREPLACE;
				case 2: return USEASM_OP_SMP2DREPLACE;
				case 3: return USEASM_OP_SMP3DREPLACE;
				default: imgabort();
			}
		}
		case ISMPGRAD:
		#if defined(OUTPUT_USPBIN)
		case ISMPGRAD_USP:
		#endif /* defined(OUTPUT_USPBIN) */
		{
			switch (psIn->u.psSmp->uDimensionality)
			{
				case 1: return USEASM_OP_SMP1DGRAD;
				case 2: return USEASM_OP_SMP2DGRAD;
				case 3: return USEASM_OP_SMP3DGRAD;
				default: imgabort();
			}
		}
		default: imgabort();
	}
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static IMG_UINT32 CopyVectorPackWriteMask(PINTERMEDIATE_STATE psState, PINST psIn)
/*********************************************************************************
 Function			: CopyVectorPackWriteMask

 Description		: Convert the write-mask to a vector pack instruction to the
					  USEASM format.
 
 Parameters			: psState		- Compiler state.
					  psIn			- Instruction whose write mask is to be
									copied.

 Globals Effected	: None

 Return				: The converted mask.
*********************************************************************************/
{
	IMG_UINT32	uOutWriteMask;
	IMG_UINT32	uDestChanWidth;
	IMG_UINT32	uInWriteMask;
	IMG_UINT32	uChan;
	IMG_UINT32	uDestChanMask;

	/*
		Get the size of the destination format in units
		of bytes/c10 channels.
	*/
	switch (psIn->eOpcode)
	{
		case IVPCKU8U8:
		case IVPCKU8FLT:
		case IVPCKS8FLT:
		case IVPCKU8FLT_EXP:
		case IVPCKC10FLT:
		case IVPCKC10FLT_EXP:
		case IVPCKC10C10:
		{
			uDestChanWidth = 1;
			break;
		}
		case IVPCKU16U16:
		case IVPCKS16S8:
		case IVPCKU16U8:
		case IVPCKS16FLT:
		case IVPCKU16FLT:
		case IVPCKS16FLT_EXP:
		case IVPCKU16FLT_EXP:
		{
			uDestChanWidth = 2;
			break;
		}
		default: imgabort();
	}

	/*
		Concatenate the write masks for each intermediate 
		destination.
	*/
	uInWriteMask = 0;
	for (uChan = 0; uChan < psIn->uDestCount; uChan++)
	{
		uInWriteMask |= (psIn->auDestMask[uChan] << (uChan * 4));
	}

	uDestChanMask = (1 << uDestChanWidth) - 1;

	/*
		Convert the write mask from 1-bit per byte/c10-channel to
		1-bit per channel of the destination format.
	*/
	uOutWriteMask = 0;
	for (uChan = 0; uChan < 4; uChan++)
	{
		IMG_UINT32	uChanMask;

		uChanMask = uInWriteMask & uDestChanMask;
		if (uChanMask == uDestChanMask)
		{
			uOutWriteMask |= (1 << uChan);
		}
		else
		{
			ASSERT(uChanMask == 0);
		}

		uInWriteMask >>= uDestChanWidth;
	}

	return uOutWriteMask;
}

static IMG_UINT32 CopyVectorWriteMask(PINTERMEDIATE_STATE psState, PINST psIn, IMG_UINT32 uBaseDestIdx)
/*********************************************************************************
 Function			: CopyVectorWriteMask

 Description		: Convert the write-mask to a vector instruction to the
					  USEASM format.
 
 Parameters			: psState		- Compiler state.
					  psIn			- Instruction whose write mask is to be
									copied.
					  uBaseDestIdx	- Offset within the overall array of destinations
									to start copying from.

 Globals Effected	: None

 Return				: The converted mask.
*********************************************************************************/
{
	IMG_UINT32	uWriteMask;

	uWriteMask = 0;
	if (psIn->asDest[uBaseDestIdx].eFmt == UF_REGFORMAT_F32)
	{
		IMG_UINT32	uChanIdx;
		IMG_UINT32	uChanCount;

		ASSERT(psIn->uDestCount > uBaseDestIdx);
		uChanCount = min(psIn->uDestCount - uBaseDestIdx, 4);

		/*
			Convert the mask from bytes to F32 channels.
		*/
		for (uChanIdx = 0; uChanIdx < uChanCount; uChanIdx++)
		{
			if (psIn->auDestMask[uBaseDestIdx + uChanIdx])
			{
				uWriteMask |= (1 << uChanIdx);
			}
		}
	}
	else if (psIn->asDest[uBaseDestIdx].eFmt == UF_REGFORMAT_C10)
	{
		ASSERT(uBaseDestIdx == 0);
		ASSERT((g_psInstDesc[psIn->eOpcode].uFlags & DESC_FLAGS_C10VECTORDEST) || 
			   (g_psInstDesc[psIn->eOpcode].uFlags & DESC_FLAGS_VECTORCOMPLEXOP));

		uWriteMask = psIn->auDestMask[0];
		if (psIn->uDestCount == 2 && (psIn->auDestMask[1] & USC_RED_CHAN_MASK) != 0)
		{
			uWriteMask |= USC_ALPHA_CHAN_MASK;
		}
	}
	else if (psIn->asDest[uBaseDestIdx].eFmt == UF_REGFORMAT_U8)
	{
		ASSERT(uBaseDestIdx == 0);
		ASSERT(g_psInstDesc[psIn->eOpcode].uFlags & DESC_FLAGS_C10VECTORDEST);

		uWriteMask = psIn->auDestMask[0];
	}
	else
	{
		IMG_UINT32	uRegIdx;
		IMG_UINT32	uRegCount;

		ASSERT(psIn->asDest[uBaseDestIdx].eFmt == UF_REGFORMAT_F16);
		ASSERT(psIn->uDestCount > uBaseDestIdx);
		uRegCount = min(psIn->uDestCount - uBaseDestIdx, 2);

		/*
			Convert the mask from bytes to F16 channels (two per register).
		*/
		for (uRegIdx = 0; uRegIdx < uRegCount; uRegIdx++)
		{
			if (psIn->auDestMask[uRegIdx] & USC_DESTMASK_LOW)
			{
				uWriteMask |= (1 << ((uRegIdx * 2) + 0));
			}
			if (psIn->auDestMask[uRegIdx] & USC_DESTMASK_HIGH)
			{
				uWriteMask |= (1 << ((uRegIdx * 2) + 1));
			}
		}
	}
	return uWriteMask;
}

static IMG_VOID CopySwizzle(PUSE_REGISTER psOutSrc, IMG_UINT32 uSwizzle)
/*********************************************************************************
 Function			: CopySwizzle

 Description		: Create a USEASM source for a swizzle parameter to an
					  instruction.
 
 Parameters			: psOutSrc		- USEASM source to set up.
					  uSwizzle		- Swizzle.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	psOutSrc->uType = USEASM_REGTYPE_SWIZZLE;
	psOutSrc->uNumber = uSwizzle;
	psOutSrc->uIndex = 0;
	psOutSrc->uFlags = 0;
}

static USEASM_OPCODE GetUseasmOpcodeForDualIssueOp(PINTERMEDIATE_STATE	psState,
												   VDUAL_OP_TYPE		eOp)
/*********************************************************************************
 Function			: GetUseasmOpcodeForDualIssueOp

 Description		: Convert the operation type of part of a dual-issue instruction
					  to the USEASM format.
 
 Parameters			: psState		- Compiler state.
					  eOp			- Operation type.

 Globals Effected	: None

 Return				: The USEASM opcode.
*********************************************************************************/
{
	static const USEASM_OPCODE aeUseasmOp[VDUAL_OP_COUNT] =
	{
		USEASM_OP_FMAD,		/* VDUAL_OP_MAD1 */
		USEASM_OP_VMAD3,	/* VDUAL_OP_MAD3 */
		USEASM_OP_VMAD4,	/* VDUAL_OP_MAD4 */
		USEASM_OP_VDP3,		/* VDUAL_OP_DP3 */
		USEASM_OP_VDP4,		/* VDUAL_OP_DP4 */
		USEASM_OP_VSSQ3,	/* VDUAL_OP_SSQ3 */
		USEASM_OP_VSSQ4,	/* VDUAL_OP_SSQ4 */
		USEASM_OP_FMUL,		/* VDUAL_OP_MUL1 */
		USEASM_OP_VMUL3,	/* VDUAL_OP_MUL3 */
		USEASM_OP_VMUL4,	/* VDUAL_OP_MUL4 */
		USEASM_OP_FADD,		/* VDUAL_OP_ADD1 */
		USEASM_OP_VADD3,	/* VDUAL_OP_ADD3 */
		USEASM_OP_VADD4,	/* VDUAL_OP_ADD4 */
		USEASM_OP_VMOV3,	/* VDUAL_OP_MOV3 */
		USEASM_OP_VMOV4,	/* VDUAL_OP_MOV4 */
		USEASM_OP_FRSQ,		/* VDUAL_OP_RSQ */
		USEASM_OP_FRCP,		/* VDUAL_OP_RCP */
		USEASM_OP_FLOG,		/* VDUAL_OP_LOG */
		USEASM_OP_FEXP,		/* VDUAL_OP_EXP */
		USEASM_OP_FSUBFLR,	/* VDUAL_OP_FRC */
	};

	ASSERT(eOp < (sizeof(aeUseasmOp) / sizeof(aeUseasmOp[0])));
	return aeUseasmOp[eOp];
}

static IMG_BOOL InstHasF16Arguments(PINTERMEDIATE_STATE psState, PINST psInst)
/*********************************************************************************
 Function			: InstHasF16Arguments

 Description		: Check for an instruction with either F16 source or destination
					  arguments.
 
 Parameters			: psState		- Compiler state.
					  psInst		- Instruction to check.

 Globals Effected	: None

 Return				: TRUE or FALSE.
*********************************************************************************/
{
	IMG_UINT32	uSlotIdx, uDestIdx;

	for (uSlotIdx = 0; uSlotIdx < GetSwizzleSlotCount(psState, psInst); uSlotIdx++)
	{
		if (psInst->u.psVec->aeSrcFmt[uSlotIdx] == UF_REGFORMAT_F16)
		{
			return IMG_TRUE;
		}
	}
	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		if (psInst->asDest[uDestIdx].eFmt == UF_REGFORMAT_F16)
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static USEASM_OPCODE GetUseasmOpcodeForVectorAluOp(PINTERMEDIATE_STATE	psState,
												   IOPCODE				eOpcode,
												   PINST				psInst)
/*********************************************************************************
 Function			: GetUseasmOpcodeForVectorAluOp

 Description		: Convert the opcode for a vector ALU operation from the
					  intermediate to the USEASM format.
 
 Parameters			: psState		- Compiler state.
					  eOpcode		- Intermediate opcode.
					  psInst		- Instruction to use to work out whether
									to use the F16 or the F32 form.

 Globals Effected	: None

 Return				: The USEASM opcode.
*********************************************************************************/
{
	/*
		Check for an F16 source or destination.
	*/
	if (InstHasF16Arguments(psState, psInst))
	{
		switch (eOpcode)
		{
			case IVMAD: return USEASM_OP_VF16MAD;
			case IVMUL: return USEASM_OP_VF16MUL;
			case IVADD: return USEASM_OP_VF16ADD;
			case IVFRC: return USEASM_OP_VF16FRC;
			case IVDSX_EXP: return USEASM_OP_VF16DSX;
			case IVDSY_EXP: return USEASM_OP_VF16DSY;
			case IVMIN: return USEASM_OP_VF16MIN;
			case IVMAX: return USEASM_OP_VF16MAX;
			case IVDP: return USEASM_OP_VF16DP;
			default: imgabort();
		}
	}
	else
	{
		return g_psInstDesc[eOpcode].uUseasmOpcode;
	}
}

static USEASM_OPCODE GetUseasmOpcodeForVectorOp(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: GetUseasmOpcodeForVectorOp

 Description		: Convert the opcode for a vector operation from the
					  intermediate to the USEASM format.
 
 Parameters			: psState		- Compiler state.
					  psIn			- Vector instruction.
					  psOut			- USEASM instruction whose flags
									fields might be modified.

 Globals Effected	: None

 Return				: The USEASM opcode.
*********************************************************************************/
{
	switch (psIn->eOpcode)
	{
		case IVTEST:
		case IVTESTMASK:
		case IVTESTBYTEMASK:
		{
			return GetUseasmOpcodeForVectorAluOp(psState, psIn->u.psVec->sTest.eTestOpcode, psIn);
		}

		case IVDUAL:
		{
			return GetUseasmOpcodeForDualIssueOp(psState, psIn->u.psVec->sDual.ePriOp);
		}

		case IVMAD4:
		case IVMUL3:
		case IVADD3:
		case IVDP_GPI:
		case IVDP3_GPI:
		case IVDP_CP:
		{
			return g_psInstDesc[psIn->eOpcode].uUseasmOpcode;
		}

		case IVMAD:
		case IVMUL:
		case IVADD:
		case IVFRC:
		case IVDSX_EXP:
		case IVDSY_EXP:
		case IVMIN:
		case IVMAX:
		case IVDP:
		{
			return GetUseasmOpcodeForVectorAluOp(psState, psIn->eOpcode, psIn);
		}

		case IVMOV_EXP:
		case IVMOVC:
		case IVMOVCU8_FLT:
		{
			if ((psIn->eOpcode == IVMOVC) || (psIn->eOpcode == IVMOVCU8_FLT))
			{
				psOut->uFlags1 |= USEASM_OPFLAGS1_TESTENABLE;
			}
			if (InstHasF16Arguments(psState, psIn))
			{
				psOut->uFlags1 |= USEASM_OPFLAGS1_MOVC_FMTF16;
			}
			else
			{
				psOut->uFlags2 |= USEASM_OPFLAGS2_MOVC_FMTF32;
			}
			return g_psInstDesc[psIn->eOpcode].uUseasmOpcode;
		}
		case IVPCKFLTFLT:
		case IVPCKFLTFLT_EXP:
		{
			if (psIn->asDest[0].eFmt == UF_REGFORMAT_F16)
			{
				if (psIn->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F16)
				{
					return USEASM_OP_VPCKF16F16;
				}
				else
				{
					ASSERT(psIn->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F32);
					return USEASM_OP_VPCKF16F32;
				}
			}
			else
			{
				ASSERT(psIn->asDest[0].eFmt == UF_REGFORMAT_F32);
				if (psIn->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F16)
				{
					return USEASM_OP_VPCKF32F16;
				}
				else
				{
					ASSERT(psIn->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F32);
					return USEASM_OP_VPCKF32F32;
				}
			}
		}

		case IVPCKU8FLT:
		case IVPCKU8FLT_EXP:
		{
			if (psIn->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F16)
			{
				return USEASM_OP_VPCKU8F16;
			}
			else
			{
				ASSERT(psIn->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F32);
				return USEASM_OP_VPCKU8F32;
			}
		}

		case IVPCKS8FLT:
		{
			if (psIn->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F16)
			{
				return USEASM_OP_VPCKS8F16;
			}
			else
			{
				ASSERT(psIn->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F32);
				return USEASM_OP_VPCKS8F32;
			}
		}

		case IVPCKC10FLT:
		case IVPCKC10FLT_EXP:
		{
			if (psIn->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F16)
			{
				return USEASM_OP_VPCKC10F16;
			}
			else
			{
				ASSERT(psIn->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F32);
				return USEASM_OP_VPCKC10F32;
			}
		}

		case IVPCKC10C10:
		{
			return USEASM_OP_VPCKC10C10;
		}

		case IVPCKFLTU8:
		{
			if (psIn->asDest[0].eFmt == UF_REGFORMAT_F16)
			{
				return USEASM_OP_VPCKF16U8;
			}
			else
			{
				ASSERT(psIn->asDest[0].eFmt == UF_REGFORMAT_F32);
				return USEASM_OP_VPCKF32U8;
			}
		}

		case IVPCKFLTS8:
		{
			if (psIn->asDest[0].eFmt == UF_REGFORMAT_F16)
			{
				return USEASM_OP_VPCKF16S8;
			}
			else
			{
				ASSERT(psIn->asDest[0].eFmt == UF_REGFORMAT_F32);
				return USEASM_OP_VPCKF32S8;
			}
		}

		case IVPCKFLTC10_VEC:
		{
			if (psIn->asDest[0].eFmt == UF_REGFORMAT_F16)
			{
				return USEASM_OP_VPCKF16C10;
			}
			else
			{
				ASSERT(psIn->asDest[0].eFmt == UF_REGFORMAT_F32);
				return USEASM_OP_VPCKF32C10;
			}
		}

		case IVPCKFLTU16:
		{
			if (psIn->asDest[0].eFmt == UF_REGFORMAT_F16)
			{
				return USEASM_OP_VPCKF16U16;
			}
			else
			{
				ASSERT(psIn->asDest[0].eFmt == UF_REGFORMAT_F32);
				return USEASM_OP_VPCKF32U16;
			}
		}

		case IVPCKFLTS16:
		{
			if (psIn->asDest[0].eFmt == UF_REGFORMAT_F16)
			{
				return USEASM_OP_VPCKF16S16;
			}
			else
			{
				ASSERT(psIn->asDest[0].eFmt == UF_REGFORMAT_F32);
				return USEASM_OP_VPCKF32S16;
			}
		}

		case IVPCKS16FLT:
		case IVPCKS16FLT_EXP:
		{
			if (psIn->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F16)
			{
				return USEASM_OP_VPCKS16F16;
			}
			else
			{
				ASSERT(psIn->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F32);
				return USEASM_OP_VPCKS16F32;
			}
		}

		case IVPCKU16FLT:
		case IVPCKU16FLT_EXP:
		{
			if (psIn->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F16)
			{
				return USEASM_OP_VPCKU16F16;
			}
			else
			{
				ASSERT(psIn->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F32);
				return USEASM_OP_VPCKU16F32;
			}
		}

		case IVRSQ:
		case IVRCP:
		case IVEXP:
		case IVLOG:
		case IVPCKU8U8:
		case IVPCKU16U16:
		case IVPCKS16S8:
		case IVPCKU16U8:
		{
			return g_psInstDesc[psIn->eOpcode].uUseasmOpcode; 
		}

		default: imgabort();
	}
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static IMG_VOID CopyDestRegister(PINTERMEDIATE_STATE	psState,
								 PINST					psIn,
								 IMG_UINT32				uDestIdx,
								 PUSE_REGISTER			psOutSrc)
/*********************************************************************************
 Function			: ConvertDest

 Description		: Copy the intermediate destination of an instruction to a
					  USEASM format instruction.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate instruction.
					  uDestIdx	- Intermediate destination.
					  psOutSrc	- Next argument to the USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PARG	psDest;

	ASSERT(uDestIdx < psIn->uDestCount);
	psDest = &psIn->asDest[uDestIdx];

	if (psDest->uType == USEASM_REGTYPE_INDEX)
	{
		psOutSrc->uFlags = 0;
		psOutSrc->uType = USEASM_REGTYPE_INDEX;
		if (psDest->uNumber == USC_INDEXREG_LOW)
		{
			psOutSrc->uNumber = USEREG_INDEX_L;
		}
		else
		{
			ASSERT(psDest->uNumber == USC_INDEXREG_HIGH);
			psOutSrc->uNumber = USEREG_INDEX_H;
		}
		psOutSrc->uIndex = USEREG_INDEX_NONE;
	}
	else
	{
		psOutSrc->uFlags = 0;
		psOutSrc->uNumber = ConvertRegisterNumber(psState, psIn, IMG_TRUE /* bDest */, uDestIdx);
		psOutSrc->uType = psDest->uType;
		psOutSrc->uIndex = CopyIndex(psState, psDest->uIndexType, psDest->uIndexNumber);
	}

	/*
		For instructions which support it - copy the format for the destination (C10 or U8).
	*/
	if (HasC10FmtControl(psIn) && psDest->uType != USEASM_REGTYPE_PREDICATE)
	{
		if (psDest->eFmt == UF_REGFORMAT_C10)
		{
			psOutSrc->uFlags |= USEASM_ARGFLAGS_FMTC10;
		}
	}
}

static IMG_VOID ConvertDest(PINTERMEDIATE_STATE	psState,
						    PINST				psIn,
							PUSE_REGISTER		psOutSrc)
/*********************************************************************************
 Function			: ConvertDest

 Description		: Copy the intermediate destination of an instruction to a
					  USEASM format instruction.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate instruction.
					  psOutSrc	- Next argument to the USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	ASSERT(g_psInstDesc[psIn->eOpcode].bHasDest);
	
	/*
		Convert the destination register to the USEASM format.
	*/
	if (psIn->uDestCount == 0)
	{
		psOutSrc->uFlags = USEASM_ARGFLAGS_DISABLEWB;
		psOutSrc->uNumber = 0;
		psOutSrc->uType = USEASM_REGTYPE_FPCONSTANT;
		psOutSrc->uIndex = USEREG_INDEX_NONE;
	}
	else 
	{
		CopyDestRegister(psState, psIn, 0 /* uDestIdx */, psOutSrc);
	}
}

static PUSE_REGISTER CopyTestDestinations(PINTERMEDIATE_STATE	psState, 
										  PINST					psIn, 
										  PUSE_INST				psOut, 
										  PUSE_REGISTER			psOutSrc)
/*********************************************************************************
 Function			: CopyTestDestinations

 Description		: Copy the destinations (predicate and unified store) of a
					  TEST instruction to a USEASM instruction.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate instruction.
					  psOut		- USEASM instruction.
					  psOutSrc	- Next argument to the USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{	
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	ASSERT(psIn->eOpcode != IVTEST);
#endif //defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)

	/*
		Convert unified store destination.
	*/
	if (psIn->uDestCount == TEST_PREDICATE_ONLY_DEST_COUNT)
	{
		/*
			Set the unified store destination to a dummy to disable write-backs.
		*/
		psOutSrc->uFlags = USEASM_ARGFLAGS_DISABLEWB;
		psOutSrc->uNumber = 0;
		psOutSrc->uType = USEASM_REGTYPE_FPCONSTANT;
		psOutSrc->uIndex = USEREG_INDEX_NONE;
	}
	else
	{
		CopyDestRegister(psState, psIn, TEST_UNIFIEDSTORE_DESTIDX, psOutSrc);
	}
	psOutSrc++;

	/*
		Convert predicate destination.
	*/
	CopyDestRegister(psState, psIn, TEST_PREDICATE_DESTIDX, psOutSrc);
	psOutSrc++;
	
	/*
		Set up the onceonly flag.
	*/
	if (GetBit(psIn->auFlag, INST_ONCEONLY))
	{
		psOut->uFlags2 |= USEASM_OPFLAGS2_ONCEONLY;
	}
	
	return psOutSrc;
}


#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static PUSE_REGISTER CopyVectorTestDestinations(PINTERMEDIATE_STATE	psState, 
												PINST					psIn, 
												PUSE_INST				psOut, 
												PUSE_REGISTER			psOutSrc)
/*********************************************************************************
 Function			: CopyVectorTestDestinations

 Description		: Copy the destinations (predicate and unified store) of a
					  vector TEST instruction to a USEASM instruction.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate instruction.
					  psOut		- USEASM instruction.
					  psOutSrc	- Next argument to the USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	ASSERT(psIn->eOpcode == IVTEST);

	/*
		Convert unified store destination.
	*/
	if (psIn->uDestCount == VTEST_PREDICATE_ONLY_DEST_COUNT)
	{
		/*
			Set the unified store destination to a dummy to disable write-backs.
		*/
		psOutSrc->uFlags = USEASM_ARGFLAGS_DISABLEWB;
		psOutSrc->uNumber = 0;
		psOutSrc->uType = USEASM_REGTYPE_FPCONSTANT;
		psOutSrc->uIndex = USEREG_INDEX_NONE;
	}
	else
	{
		CopyDestRegister(psState, psIn, VTEST_UNIFIEDSTORE_DESTIDX, psOutSrc);
	}
	psOutSrc++;

	/*
		Convert predicate destination.
	*/
	CopyDestRegister(psState, psIn, VTEST_PREDICATE_DESTIDX, psOutSrc);

	/* 
		The TEST instruction can have 1 or 4 predicate destinations, but not 2 or 3. 
	*/
	if (psIn->u.psVec->sTest.eChanSel == VTEST_CHANSEL_PERCHAN)
	{
		ASSERT((psIn->asDest[VTEST_PREDICATE_DESTIDX+1].uType == USEASM_REGTYPE_PREDICATE) &&
			(psIn->asDest[VTEST_PREDICATE_DESTIDX+2].uType == USEASM_REGTYPE_PREDICATE) &&
			(psIn->asDest[VTEST_PREDICATE_DESTIDX+3].uType == USEASM_REGTYPE_PREDICATE));
	}
	else
	{
		ASSERT((psIn->asDest[VTEST_PREDICATE_DESTIDX+1].uType == USC_REGTYPE_UNUSEDDEST) &&
			(psIn->asDest[VTEST_PREDICATE_DESTIDX+2].uType == USC_REGTYPE_UNUSEDDEST) &&
			(psIn->asDest[VTEST_PREDICATE_DESTIDX+3].uType == USC_REGTYPE_UNUSEDDEST));
	}

	psOutSrc++;
	
	/*
		Set up the onceonly flag.
	*/
	if (GetBit(psIn->auFlag, INST_ONCEONLY))
	{
		psOut->uFlags2 |= USEASM_OPFLAGS2_ONCEONLY;
	}
	
	return psOutSrc;
}
#endif //defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)

static PUSE_REGISTER CopySources(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_REGISTER psOutSrc)
/*********************************************************************************
 Function			: CopySources

 Description		: Copy the intermediate sources of an instruction to a
					  USEASM format instruction.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate instruction.
					  psOutSrc	- Next argument to the USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32	i;

	for (i = 0; i < psIn->uArgumentCount; i++, psOutSrc++)
	{
		CopySource(psState, psIn, i, psOutSrc);
	}

	return psOutSrc;
}

static PUSE_REGISTER CopySourcesWithFloatSourceModifier(PINTERMEDIATE_STATE psState, 
														PINST psIn, 
														PFLOAT_PARAMS psFloat,
														IMG_UINT32 uSrcCount,
														PUSE_REGISTER psOutSrc,
														IMG_BOOL bCopyComponent)
/*********************************************************************************
 Function			: CopySourcesWithFloatSourceModifier

 Description		: Copy the intermediate sources of an instruction which supports
					  floating point source modifiers to a USEASM format instruction.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate instruction.
					  psFloat	- Floating point source modifiers.
					  uSrcCount	- Count of sources to copy.
					  psOutSrc	- Next argument to the USEASM instruction.
					  bCopyComponent
								- If TRUE copy the component select on the source
								arguments to the USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32	i;

	for (i = 0; i < uSrcCount; i++, psOutSrc++)
	{
		CopySource(psState, psIn, i, psOutSrc);

		if (psFloat->asSrcMod[i].bNegate)
		{
			psOutSrc->uFlags |= USEASM_ARGFLAGS_NEGATE;
		}
		if (psFloat->asSrcMod[i].bAbsolute)
		{
			psOutSrc->uFlags |= USEASM_ARGFLAGS_ABSOLUTE;
		}
		if (bCopyComponent)
		{
			psOutSrc->uFlags |= (psFloat->asSrcMod[i].uComponent << USEASM_ARGFLAGS_COMP_SHIFT);
		}
	}

	return psOutSrc;
}

static IMG_VOID ConvertIMAEToUseasm(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertIMAEToUseasm

 Description		: Convert an IMAE  instruction from the internal format to 
					  the USEASM format.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate IMAE instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32		uCarryIReg;
	IMG_UINT32		i;
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	psOut->uOpcode = USEASM_OP_IMAE;

	/*
		Copy the register destination.
	*/
	ConvertDest(psState, psIn, psOutSrc);
	psOutSrc++;

	/*
		Copy the register sources.
	*/
	psOutSrc = CopySources(psState, psIn, psOutSrc);

	/*
		Set the signed flag to USEASM.
	*/
	if (psIn->u.psImae->bSigned)
	{
		psOut->uFlags2 |= USEASM_OPFLAGS2_SIGNED;
	}
	else
	{
		psOut->uFlags2 |= USEASM_OPFLAGS2_UNSIGNED;
	}

	/*
		The carry-in internal register is the last argument
		in the internal instruction but appears in a different
		position in the USEASM input so skip backwards in
		the USEASM argument array and overwrite it.
	*/
	psOutSrc--;

	/*
		Set the SRC2 interpretation argument to USEASM.
	*/
	psOutSrc->uType = USEASM_REGTYPE_INTSRCSEL;
	psOutSrc->uNumber = psIn->u.psImae->uSrc2Type;
	psOutSrc->uIndex = USEREG_INDEX_NONE;
	psOutSrc->uFlags = 0;
	psOutSrc++;

	/*
		Copy the carry-in enable and carry-out enable
		flags to USEASM.
	*/
	uCarryIReg = USC_UNDEF;
	for (i = 0; i < 2; i++, psOutSrc++)
	{
		IMG_UINT32	uSrcSel;
		PARG		psIRegArg;

		if (i == 0)
		{
			psIRegArg = &psIn->asDest[IMAE_CARRYOUT_DEST];
			uSrcSel = USEASM_INTSRCSEL_COUTENABLE;
		}
		else
		{
			psIRegArg = &psIn->asArg[IMAE_CARRYIN_SOURCE];
			uSrcSel = USEASM_INTSRCSEL_CINENABLE;
		}

		psOutSrc->uType = USEASM_REGTYPE_INTSRCSEL;
		if (psIRegArg->uType == USEASM_REGTYPE_FPINTERNAL)
		{
			ASSERT(psIRegArg->uIndexType == USC_REGTYPE_NOINDEX);
			/* Must be the same internal register for carry-out and carry-in. */
			ASSERT(uCarryIReg == USC_UNDEF || uCarryIReg == psIRegArg->uNumber);
			uCarryIReg = psIRegArg->uNumber;

			psOutSrc->uNumber = uSrcSel;
		}
		else
		{
			psOutSrc->uNumber = USEASM_INTSRCSEL_NONE;
		}
		psOutSrc->uIndex = USEREG_INDEX_NONE;
		psOutSrc->uFlags = 0;
	}
	/*
		Set the index of the carry-in/out internal
		register for USEASM.
	*/
	if (uCarryIReg != USC_UNDEF)
	{
		psOutSrc->uType = USEASM_REGTYPE_FPINTERNAL;
		psOutSrc->uNumber = uCarryIReg;
		psOutSrc->uIndex = USEREG_INDEX_NONE;
		psOutSrc->uFlags = 0;
	}

	for (i = 0; i < 3; i++)
	{
		IMG_UINT32	uComponent = psIn->u.psImae->auSrcComponent[i];

		if (i == 2 && psIn->u.psImae->uSrc2Type == USEASM_INTSRCSEL_U32)
		{
			ASSERT(uComponent == 0);
			continue;
		}
		psOut->asArg[i + 1].uFlags &= USEASM_ARGFLAGS_COMP_CLRMSK;
		/*
			For immediate values apply the LOW/HIGH select ourselves rather
			than encoding it in the instruction.
		*/
		if (psOut->asArg[i + 1].uType == USEASM_REGTYPE_IMMEDIATE)
		{
			psOut->asArg[i + 1].uNumber = ApplyIMAEComponentSelect(psState, psIn, i, uComponent, psOut->asArg[i + 1].uNumber);
			psOut->asArg[i + 1].uFlags |= USEASM_ARGFLAGS_LOW;
			continue;
		}
		/*
			Convert the component select on this argument to a LOW or HIGH
			flag.
		*/
		if (uComponent == 2)
		{
			psOut->asArg[i + 1].uFlags |= USEASM_ARGFLAGS_HIGH;
		}
		else
		{
			ASSERT(uComponent == 0);
			psOut->asArg[i + 1].uFlags |= USEASM_ARGFLAGS_LOW;
		}
	}
}

static IMG_VOID ConvertEMITToUseasm(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertEMITToUseasm

 Description		: Convert an EMIT instruction from the internal format to 
					  the USEASM format.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate EMIT instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	psOut->uOpcode = USEASM_OP_EMITMTEVERTEX;

	/*
		Convert the CUT flag to the USEASM format.
	*/
	if (psIn->u.sEmit.bCut == IMG_TRUE)
	{
		psOut->uFlags3 |= USEASM_OPFLAGS3_CUT;
	}

	/*
		Convert the TWO PARTITION flag to the USEASM format.
	*/
	if (psState->uFlags2 & USC_FLAGS2_TWO_PARTITION_MODE)
	{
		psOut->uFlags3 |= USEASM_OPFLAGS3_TWOPART;
	}

	/*
		Set up the INCP parameter for USEASM (always 0).
	*/
	psOutSrc->uType = USEASM_REGTYPE_IMMEDIATE;
	psOutSrc->uNumber = 0;
	psOutSrc->uIndex = USEREG_INDEX_NONE;
	psOutSrc->uFlags = 0;
	psOutSrc++;

	/*
		Set up the EMITMTE source 1 for USEASM.

		Bit 0: 1 if the kick is in complex mode
		Bit 1: Index of the complex phase.
	*/
	psOutSrc->uType = USEASM_REGTYPE_IMMEDIATE;
	psOutSrc->uNumber = 1 /*EURASIA_MTEEMIT1_COMPLEX | EURASIA_MTEEMIT1_COMPLEX_PHASE1*/;
	psOutSrc->uIndex = USEREG_INDEX_NONE;
	psOutSrc->uFlags = 0;
	psOutSrc++;

	/*
		Set up the EMITMTE source 2 for USEASM.

		Bit 0: 1 to override the synchronization number.
		Bit 1-7: Reserved
		Bit 8-15: Synchronisation number to use if it is being overridden.
	*/
	psOutSrc->uType = USEASM_REGTYPE_IMMEDIATE;
	psOutSrc->uNumber = 0;
	psOutSrc->uIndex = USEREG_INDEX_NONE;
	psOutSrc->uFlags = 0;	
}

static IMG_VOID ConvertWOPToUseasm(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertWOPToUseasm

 Description		: Convert an WOP instruction from the internal format to 
					  the USEASM format.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate WOP instruction.					  
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	psOut->uOpcode = USEASM_OP_WOP;

	/*
		Convert the TWO PARTITION flag to the USEASM format.
	*/
	if (psState->uFlags2 & USC_FLAGS2_TWO_PARTITION_MODE)
	{
		psOut->uFlags3 |= USEASM_OPFLAGS3_TWOPART;
	}

	psOutSrc->uType = USEASM_REGTYPE_IMMEDIATE;
	psOutSrc->uNumber = psIn->asArg[0].uNumber;
	psOutSrc->uIndex = USEREG_INDEX_NONE;
	psOutSrc->uFlags = 0;
}

#if defined(SUPPORT_SGX545)
static IMG_VOID ConvertRoundInstToUseasm(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertRoundInstToUseasm

 Description		: Convert a RNE/TRUNC/FLOOR instruction from the internal format to 
					  the USEASM format.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate round instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	ASSERT(g_psInstDesc[psIn->eOpcode].uUseasmOpcode != (IMG_UINT32)-1);
	psOut->uOpcode = g_psInstDesc[psIn->eOpcode].uUseasmOpcode; 

	/*
		Copy the register destination.
	*/
	ConvertDest(psState, psIn, psOutSrc);
	psOutSrc++;

	/*
		Copy the register sources.
	*/
	psOutSrc = CopySources(psState, psIn, psOutSrc);

	psOut->uFlags1 |= USEASM_OPFLAGS1_ROUNDENABLE;

	psOut->asArg[0].uFlags |= (USC_DESTMASK_FULL << USEASM_ARGFLAGS_BYTEMSK_SHIFT) | USEASM_ARGFLAGS_BYTEMSK_PRESENT;

	psOutSrc->uType = USEASM_REGTYPE_IMMEDIATE;
	psOutSrc->uNumber = 0;
	psOutSrc->uIndex = USEREG_INDEX_NONE;
	psOutSrc->uFlags = 0;
	psOutSrc++;

	/*
		Add the round-mode as an argument.
	*/
	psOutSrc->uType = USEASM_REGTYPE_INTSRCSEL;
	switch (psIn->eOpcode)
	{
		case IRNE: psOutSrc->uNumber = USEASM_INTSRCSEL_ROUNDNEAREST; break;
		case ITRUNC_HW: psOutSrc->uNumber = USEASM_INTSRCSEL_ROUNDZERO; break;
		case IFLOOR: psOutSrc->uNumber = USEASM_INTSRCSEL_ROUNDDOWN; break;
		default: imgabort();
	}
	psOutSrc->uIndex = USEREG_INDEX_NONE;
	psOutSrc->uFlags = 0;
	psOutSrc++;
}
#endif /* defined(SUPPORT_SGX545) */

#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
static IMG_VOID ConvertNormaliseInstToUseasm(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertNormaliseInstToUseasm

 Description		: Convert a FNRM/FNRMF16 instruction from the internal format to 
					  the USEASM format.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate normalise instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	const IMG_UINT32 uDrcIdx = 6;
	IMG_UINT32 uIdx = 3;  // last src operand 
	PUSE_REGISTER psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	ASSERT(g_psInstDesc[psIn->eOpcode].uUseasmOpcode != (IMG_UINT32)-1);
	psOut->uOpcode = g_psInstDesc[psIn->eOpcode].uUseasmOpcode; 

	/*
		Copy the register destination.
	*/
	ConvertDest(psState, psIn, psOutSrc);
	psOutSrc++;

	/*
		Copy the register sources.
	*/
	psOutSrc = CopySources(psState, psIn, psOutSrc);

	/* Move the drc number to the last argument */
	psOut->asArg[uDrcIdx] = psOut->asArg[uIdx + 1];

	uIdx ++;
	/* Set the negation modifier */
	psOut->asArg[uIdx].uType = USEASM_REGTYPE_INTSRCSEL;
	psOut->asArg[uIdx].uFlags = 0;
	psOut->asArg[uIdx].uIndex = USEREG_INDEX_NONE;

	if (psIn->u.psNrm->bSrcModNegate)
		psOut->asArg[uIdx].uNumber = USEASM_INTSRCSEL_SRCNEG;
	else
		psOut->asArg[uIdx].uNumber = USEASM_INTSRCSEL_NONE;
	uIdx ++;

	/* Set the absolute modifier */
	psOut->asArg[uIdx].uType = USEASM_REGTYPE_INTSRCSEL;
	psOut->asArg[uIdx].uFlags = 0;
	psOut->asArg[uIdx].uIndex = USEREG_INDEX_NONE;

	if (psIn->u.psNrm->bSrcModAbsolute)
		psOut->asArg[uIdx].uNumber = USEASM_INTSRCSEL_SRCABS;
	else
		psOut->asArg[uIdx].uNumber = USEASM_INTSRCSEL_NONE;
	uIdx ++;
}
#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */

static IMG_VOID ConvertSMLSIToUseasm(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertSMLSIToUseasm

 Description		: Convert an SMLSI instruction from the internal format to 
					  the USEASM format.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate SMLSI instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32		i;
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	psOut->uOpcode = USEASM_OP_SMLSI;

	/*
		Copy the increment/swizzle values and the
		incrementmode/swizzlemode flags.
	*/
	psOutSrc = CopySources(psState, psIn, psOutSrc);

	/*
		Set the MOE limits to default values.
	*/
	for (i = 0; i < 3; i++, psOutSrc++)
	{
		psOutSrc->uType = USEASM_REGTYPE_IMMEDIATE;
		psOutSrc->uNumber = 0;
		psOutSrc->uIndex = USEREG_INDEX_NONE;
		psOutSrc->uFlags = 0;
	}
}

static IMG_VOID ConvertBitwiseInstructionToUseasm(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertBitwiseInstructionToUseasm

 Description		: Convert a bitwise instruction from the internal format to 
					  the USEASM format.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate Bitwies instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSE_REGISTER	psOutSrc = psOut->asArg;
	IMG_UINT32		i;

	ASSERT(g_psInstDesc[psIn->eOpcode].eType == INST_TYPE_BITWISE);

	/*
		Set the USEASM opcode.
	*/
	psOut->uOpcode = g_psInstDesc[psIn->eOpcode].uUseasmOpcode;

	/*
		Copy the register destination.
	*/
	ConvertDest(psState, psIn, psOutSrc);
	psOutSrc++;

	/*
		Copy the unified store arguments.
	*/
	for (i = 0; i < psIn->uArgumentCount; i++, psOutSrc++)
	{
		CopySource(psState, psIn, i, psOutSrc);
		if (i == 1 && psIn->u.psBitwise->bInvertSrc2)
		{
			psOutSrc->uFlags |= USEASM_ARGFLAGS_INVERT;
		}
	}

}

static IMG_VOID ConvertIMA16ToUseasm(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertIMA16ToUseasm

 Description		: Convert an IMA16 instruction from the internal format to 
					  the USEASM format.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate IMA16 instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32		i;
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	psOut->uOpcode = USEASM_OP_IMA16;

	/*
		Always signed mode.
	*/
	psOut->uFlags2 |= USEASM_OPFLAGS2_SIGNED;

	/*
		Copy the register destination.
	*/
	ConvertDest(psState, psIn, psOutSrc);
	psOutSrc++;

	/*
		Copy the unified store arguments.
	*/
	for (i = 0; i < IMA16_ARGUMENT_COUNT; i++, psOutSrc++)
	{
		CopySource(psState, psIn, i, psOutSrc);
		if (i == 2 && psIn->u.psIma16->bNegateSource2)
		{
			psOutSrc->uFlags |= USEASM_ARGFLAGS_NEGATE;
		}
	}

	/*
		Set up the data format arguments.
	*/
	for (i = 0; i < 2; i++, psOutSrc++)
	{
		psOutSrc->uType = USEASM_REGTYPE_INTSRCSEL;
		if (psIn->asArg[1 + i].uType == USEASM_REGTYPE_IMMEDIATE)
		{
			psOutSrc->uNumber = USEASM_INTSRCSEL_U8;
		}
		else
		{
			psOutSrc->uNumber = USEASM_INTSRCSEL_U16;
		}
		psOutSrc->uIndex = USEREG_INDEX_NONE;
		psOutSrc->uFlags = 0;
	}
}

static
IMG_VOID ConvertEFOToUseasm(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertEFOToUseasm

 Description		: Convert an EFO instruction from the internal format to 
					  the USEASM format.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate EFO instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	psOut->uOpcode = USEASM_OP_EFO;

	/*
		Copy the register destination.
	*/
	ConvertDest(psState, psIn, psOutSrc);
	psOutSrc++;

	/*
		Copy the unified store/internal register sources.
	*/
	psOutSrc = 
		CopySourcesWithFloatSourceModifier(psState, 
										   psIn, 
										   &psIn->u.psEfo->sFloat, 
										   EFO_HARDWARE_SOURCE_COUNT,
										   psOutSrc,
										   IMG_FALSE /* bCopyComponent */);

	/*
		Add an extra source with the adder/multipler set up.
	*/
	psOutSrc->uType = USEASM_REGTYPE_IMMEDIATE;
	psOutSrc->uNumber = EncodeEfo(psState, psIn, psOut, psIn->u.psEfo);
	psOutSrc->uIndex = USEREG_INDEX_NONE;
	psOutSrc->uFlags = 0;
}

static
IMG_VOID ConvertIDFToUseasm(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertIDFToUseasm

 Description		: Convert an IDF instruction from the internal format to 
					  the USEASM format.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate IDF instruction.
					  psOut		- USEASM instruction.
					  psOutSrc	- Next argument to the USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	psOut->uOpcode = USEASM_OP_IDF;

	/*
		Copy the register sources.
	*/
	psOutSrc = CopySources(psState, psIn, psOutSrc);

	psOutSrc->uType = USEASM_REGTYPE_IMMEDIATE;
	psOutSrc->uNumber = 0;
	psOutSrc->uIndex = USEREG_INDEX_NONE;
	psOutSrc->uFlags = 0;
}

#if defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_VOID ConvertIMA32ToUseasm(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertIMA32ToUseasm

 Description		: Convert an IMA32 instruction from the internal format to 
					  the USEASM format.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate IMA32 instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PARG			psHighWordDest;
	IMG_UINT32		uArgIdx;
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	psOut->uOpcode = USEASM_OP_IMA32;

	/*
		Copy the destination for the low dword of the result.
	*/
	ConvertDest(psState, psIn, psOutSrc);
	psOutSrc++;

	/*
		Copy the signed flag.
	*/
	if (psIn->u.psIma32->bSigned)
	{
		psOut->uFlags2 |=  USEASM_OPFLAGS2_SIGNED;
	}
	else
	{
		psOut->uFlags2 |= USEASM_OPFLAGS2_UNSIGNED;
	}

	if(psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_IMA32_32X16_PLUS_32)
	{
		ASSERT(psIn->u.psIma32->uStep <= 2);
		if(psIn->u.psIma32->uStep == 2)
		{
			psOut->uFlags3 |= USEASM_OPFLAGS3_STEP2;
		}
		else
		{
			ASSERT(psIn->u.psIma32->uStep == 1);
			psOut->uFlags3 |= USEASM_OPFLAGS3_STEP1;
		}
	}

	if((psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_IMA32_32X16_PLUS_32) == 0)
	{
		/* Next operand is destination for high dword */
		psHighWordDest = &psIn->asDest[IMA32_HIGHDWORD_DEST];
		if (psHighWordDest->uType == USEASM_REGTYPE_FPINTERNAL)
		{
			psOutSrc->uType = USEASM_REGTYPE_FPINTERNAL;
			psOutSrc->uNumber = psHighWordDest->uNumber;
		}
		else
		{
			ASSERT(psHighWordDest->uType == USC_REGTYPE_UNUSEDDEST);

			psOutSrc->uType = USEASM_REGTYPE_INTSRCSEL;
			psOutSrc->uNumber = USEASM_INTSRCSEL_NONE;
		}
	}
	else
	{
		psOutSrc->uType = USEASM_REGTYPE_INTSRCSEL;
		psOutSrc->uNumber = USEASM_INTSRCSEL_NONE;
	}
	psOutSrc->uFlags = 0;
	psOutSrc->uIndex = USEREG_INDEX_NONE;
	psOutSrc++;

	/*
		Copy the register sources.
	*/
	for (uArgIdx = 0; uArgIdx < IMA32_SOURCE_COUNT; uArgIdx++, psOutSrc++)
	{
		CopySource(psState, psIn, uArgIdx, psOutSrc);
		if (psIn->u.psIma32->abNegate[uArgIdx])
		{
			psOutSrc->uFlags |= USEASM_ARGFLAGS_NEGATE;
		}
	}

	/* carry-in-enable bits - we never use them. */
	psOutSrc->uType = USEASM_REGTYPE_INTSRCSEL;
	psOutSrc->uIndex = USEREG_INDEX_NONE;
	psOutSrc->uFlags = 0;
	psOutSrc->uNumber = USEASM_INTSRCSEL_NONE;
}
#endif /* defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

#if defined(SUPPORT_SGX545)
static
IMG_VOID ConvertDualIssueToUseasm(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertDualIssueToUseasm

 Description		: Convert a SGX545 dual-issue instruction from the internal format to 
					  the USEASM format.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate texture sample instruction.
					  psOut		- USEASM instruction.
					  psOutSrc	- Next argument to the USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32		uArgCount;
	IMG_UINT32		uArgStart;
	IMG_UINT32		i;
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set up the instruction representing the secondary operation.
	*/
	psOut->uFlags1 |= USEASM_OPFLAGS1_MAINISSUE;
	ConvertDualIssueSecondaryToUseasm(psState, psIn, psOut->psNext);

	/*
		Set the primary operation type.
	*/
	ASSERT(g_psInstDesc[psIn->u.psDual->ePrimaryOp].uUseasmOpcode != (IMG_UINT32)-1);
	psOut->uOpcode = g_psInstDesc[psIn->u.psDual->ePrimaryOp].uUseasmOpcode; 

	/*
		Copy the primary destination.
	*/
	ConvertDest(psState, psIn, psOutSrc);
	psOutSrc++;

	/*
		Add the extra internal register destination for
		dual-issued FDDP.
	*/
	if (psIn->u.psDual->ePrimaryOp == IFDDP)
	{
		ASSERT(psIn->uDestCount == 3);
		ASSERT(psIn->asDest[2].uType == USEASM_REGTYPE_FPINTERNAL);
		ASSERT(psIn->asDest[2].uNumber == SGX545_USE_FDUAL_FDDP_IREG_DEST_NUM);

		psOutSrc->uNumber = SGX545_USE_FDUAL_FDDP_IREG_DEST_NUM;
		psOutSrc->uType = USEASM_REGTYPE_FPINTERNAL;
		psOutSrc->uIndex = USEREG_INDEX_NONE;
		psOutSrc->uFlags = 0;
		psOutSrc++;
	}
	/*
		For dual-issued IMA32 add the extra, disabled destination for
		the high 32-bits of the result.
	*/
	if (psIn->u.psDual->ePrimaryOp == IIMA32)
	{
		psOutSrc->uNumber = USEASM_INTSRCSEL_NONE;
		psOutSrc->uType = USEASM_REGTYPE_INTSRCSEL;
		psOutSrc->uIndex = USEREG_INDEX_NONE;
		psOutSrc->uFlags = 0;
		psOutSrc++;
	}
		
	/*
		For primary instructions which don't reference source 0 skip copying it. The
		assembler will encode source 0 from the secondary instruction.
	*/
	uArgCount = g_psInstDesc[psIn->u.psDual->ePrimaryOp].uDefaultArgumentCount;
	if (psIn->u.psDual->ePrimaryOp == IFSSQ || psIn->u.psDual->ePrimaryOp == IMOV)
	{
		uArgStart = 1;
	}
	else
	{
		uArgStart = 0;
	}

	/*
		Copy the register sources.
	*/
	for (i = 0; i < uArgCount; i++, psOutSrc++)
	{
		CopySource(psState, psIn, uArgStart + i, psOutSrc);
		if (psIn->u.psDual->abPrimarySourceNegate[uArgStart + i])
		{
			psOutSrc->uFlags |= USEASM_ARGFLAGS_NEGATE;
		}
	}

	if (psIn->u.psDual->ePrimaryOp == IIMA32)
	{
		/*
			No carry-in for dual-issued IMA32.
		*/
		psOutSrc->uType = USEASM_REGTYPE_INTSRCSEL;
		psOutSrc->uIndex = USEREG_INDEX_NONE;
		psOutSrc->uFlags = 0;
		psOutSrc->uNumber = USEASM_INTSRCSEL_NONE;
		psOutSrc++;
	}
}
#endif /* defined(SUPPORT_SGX545) */

static
IMG_VOID ConvertRLPToUseasm(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertRLPToUseasm

 Description		: Convert an RLP instruction from the internal format to the USEASM
					  format.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate RLP instruction.
					  psOut		- USEASM instruction.
					  psOutSrc	- Next argument to the USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSE_REGISTER	psOutSrc = psOut->asArg;
	PARG			psPredicateSrc = &psIn->asArg[RLP_PREDICATE_ARGIDX];

	/*
		Set the USEASM opcode.
	*/
	psOut->uOpcode = USEASM_OP_RLP;

	/*
		Convert the register destination.
	*/
	CopyDestRegister(psState, psIn, RLP_UNIFIEDSTORE_DESTIDX, psOutSrc);
	psOutSrc++;

	/*
		Check the predicate destination for the top-bit of the unified store source is
		the same as the predicate source for the low-bit of the unified store destination.
	*/
	ASSERT(psIn->uDestCount > RLP_PREDICATE_DESTIDX);
	ASSERT(psIn->asDest[RLP_PREDICATE_DESTIDX].uType == USEASM_REGTYPE_PREDICATE);
	ASSERT(psIn->asDest[RLP_PREDICATE_DESTIDX].uNumber == psPredicateSrc->uNumber);

	/*
		Copy the sources.
	*/
	psOutSrc = CopySources(psState, psIn, psOutSrc);
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static UF_REGFORMAT GetTextureSampleSourceFormat(PARG psSrc)
/*********************************************************************************
 Function			: GetTextureSampleSourceFormat

 Description		: Get the required coordinate format setting for a source to a texture sample
					  instruction.
 
 Parameters			: psSrc		- Source.

 Globals Effected	: None

 Return				: The format.
*********************************************************************************/
{
	/*
		Internal register source are always interpreted as F32 so the coordinate format
		doesn't matter.
	*/
	if (psSrc->uType == USEASM_REGTYPE_FPINTERNAL)
	{
		return UF_REGFORMAT_UNTYPED;
	}
	return psSrc->eFmt;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static UF_REGFORMAT GetTextureSampleCoordinateFormat(PINTERMEDIATE_STATE psState, PINST psIn)
/*********************************************************************************
 Function			: GetTextureSampleCoordinateFormat

 Description		: Get the required coordinate format setting a texture sample
					  instruction.
 
 Parameters			: psIn	- Instruction.

 Globals Effected	: None

 Return				: The format.
*********************************************************************************/
{
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		UF_REGFORMAT	eFmt;

		/*
			Check if the coordinate source requires a particular coordinate format.
		*/
		eFmt = GetTextureSampleSourceFormat(&psIn->asArg[SMP_COORD_ARG_START]);
		if (eFmt == UF_REGFORMAT_UNTYPED)
		{
			/*
				Otherwise check either the LOD bias/replace source or the gradients
				source.
			*/
			if (
					#if defined(OUTPUT_USPBIN)
					(psIn->eOpcode == ISMPBIAS_USP) ||
					(psIn->eOpcode == ISMPREPLACE_USP) ||
					#endif	/* #if defined(OUTPUT_USPBIN) */
					(psIn->eOpcode == ISMPBIAS) ||
					(psIn->eOpcode == ISMPREPLACE)
				)
			{
				eFmt = GetTextureSampleSourceFormat(&psIn->asArg[SMP_LOD_ARG_START]);
			}
			else if (psIn->u.psSmp->uGradSize > 0)
			{
				eFmt = GetTextureSampleSourceFormat(&psIn->asArg[SMP_GRAD_ARG_START]);
			}
		}
		/*
			If all sources are internal registers then set the coordinate format to
			a default.
		*/
		if (eFmt == UF_REGFORMAT_UNTYPED)
		{
			eFmt = UF_REGFORMAT_F32;
		}
		return eFmt;
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		PVR_UNREFERENCED_PARAMETER(psState);
		
		return psIn->asArg[SMP_COORD_ARG_START].eFmt;
	}
}

static IMG_VOID ConvertTextureSampleToUseasm(PINTERMEDIATE_STATE	psState, 
											 PINST					psIn, 
											 PUSE_INST				psOut)
/*********************************************************************************
 Function			: ConvertTextureSampleToUseasm

 Description		: Convert a texture sample instruction from the internal format to the USEASM
					  format.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate texture sample instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSE_REGISTER	psOutSrc = psOut->asArg;
	UF_REGFORMAT	eCoordFormat = GetTextureSampleCoordinateFormat(psState, psIn);

	/*
		Set the USEASM opcode.
	*/
	psOut->uOpcode = GetUseasmOpcodeForSmp(psState, psIn);

	/*
		Convert the register destination.
	*/
	ConvertDest(psState, psIn, psOutSrc);
	psOutSrc++;

	/*
		Copy the PCF flag.
	*/
	if (psIn->u.psSmp->bUsesPCF)
	{
		if (psIn->asArg[SMP_COORD_ARG_START].eFmt == UF_REGFORMAT_F16)
		{
			psOut->uFlags3 |= USEASM_OPFLAGS3_PCFF16;
		}
		else
		{
			ASSERT(psIn->asArg[SMP_COORD_ARG_START].eFmt == UF_REGFORMAT_F32);
			psOut->uFlags3 |= USEASM_OPFLAGS3_PCFF32;
		}
	}

	/* 
		Copy the flag specifying the type of data to return.
	*/
	switch (psIn->u.psSmp->eReturnData)
	{
		case SMP_RETURNDATA_NORMAL: /* No special flags. */ break;
		case SMP_RETURNDATA_SAMPLEINFO: psOut->uFlags3 |= USEASM_OPFLAGS3_SAMPLEINFO; break;
		case SMP_RETURNDATA_RAWSAMPLES: psOut->uFlags3 |= USEASM_OPFLAGS3_SAMPLEDATA; break;
		case SMP_RETURNDATA_BOTH: psOut->uFlags3 |= USEASM_OPFLAGS3_SAMPLEDATAANDINFO; break;
		default: imgabort();
	}

	/*
		Copy the first register in the texture coordinate source.
	*/
	CopySource(psState, psIn, SMP_COORD_ARG_START, psOutSrc);
	switch (eCoordFormat)
	{
		case UF_REGFORMAT_F16: psOutSrc->uFlags |= USEASM_ARGFLAGS_FMTF16; break;
		case UF_REGFORMAT_C10: psOutSrc->uFlags |= USEASM_ARGFLAGS_FMTC10; break;
		case UF_REGFORMAT_F32: break;
		default: imgabort();
	}
	psOutSrc++;

	/*
		Copy the first register in the texture state source.
	*/
	CopySource(psState, psIn, SMP_STATE_ARG_START, psOutSrc);
	psOutSrc++;

	if	(
			#if defined(OUTPUT_USPBIN)
			(psIn->eOpcode == ISMPBIAS_USP) ||
			(psIn->eOpcode == ISMPREPLACE_USP) ||
			#endif	/* #if defined(OUTPUT_USPBIN) */
			(psIn->eOpcode == ISMPBIAS) ||
			(psIn->eOpcode == ISMPREPLACE)
		)
	{
		/*
			Copy the LOD bias/replace source.
		*/
		CopySource(psState, psIn, SMP_LOD_ARG_START, psOutSrc);
		psOutSrc++;
	}
	else if	(
				#if defined(OUTPUT_USPBIN)
				(psIn->eOpcode == ISMPGRAD_USP) ||
				#endif /* #if defined(OUTPUT_USPBIN) */
				(psIn->eOpcode == ISMPGRAD)
			)
	{
		/*
			Copy the first register in the gradients source.
		*/
		CopySource(psState, psIn, SMP_GRAD_ARG_START, psOutSrc);
		psOutSrc++;
	}

	/*
		Copy the DRC argument.
	*/
	CopySource(psState, psIn, SMP_DRC_ARG_START, psOutSrc);
	psOutSrc++;

	/*
		Format conversion argument.
	*/
	psOutSrc->uType = USEASM_REGTYPE_INTSRCSEL;
	#if defined(OUTPUT_USCHW) && (defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554))
	if (
			(psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) == 0 &&
			(psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_SMP_RESULT_FORMAT_CONVERT) != 0
	   )
	{
		IMG_UINT32				uTextureStage;
		PUNIFLEX_TEXTURE_UNPACK	psTextureUnpack;

		/*
			On this core the texture unpacking mode is supplied directly in the SMP instruction
			encoding.
		*/

		uTextureStage = psIn->u.psSmp->uTextureStage;
		psTextureUnpack = &psState->asTextureUnpackFormat[uTextureStage];

		switch (psTextureUnpack->eFormat)
		{
			case UNIFLEX_TEXTURE_UNPACK_FORMAT_NATIVE: psOutSrc->uNumber = USEASM_INTSRCSEL_NONE; break;
			case UNIFLEX_TEXTURE_UNPACK_FORMAT_F16: 
			{
				ASSERT(psTextureUnpack->bNormalise);
				psOutSrc->uNumber = USEASM_INTSRCSEL_F16; 
				break;
			}
			case UNIFLEX_TEXTURE_UNPACK_FORMAT_F32: 
			{
				ASSERT(psTextureUnpack->bNormalise);
				psOutSrc->uNumber = USEASM_INTSRCSEL_F32; 
				break;
			}
			case UNIFLEX_TEXTURE_UNPACK_FORMAT_C10:
			{
				/*
					Unpacking texture results to C10 isn't supported on this core.
				*/
				imgabort();
			}
			default: imgabort();
		}

		/*
			Copy the MINP flag directly from the information about the texture format supplied by the
			caller.
		*/
		if (psState->psSAOffsets->asTextureParameters[uTextureStage].sFormat.bMinPack)
		{
			psOut->uFlags3 |= USEASM_OPFLAGS3_MINPACK;
		}
	}
	else
	#endif /* defined(OUTPUT_USCHW) && (defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)) */
	{
		psOutSrc->uNumber = USEASM_INTSRCSEL_NONE;
	}
	psOutSrc->uIndex = 0;
	psOutSrc->uFlags = 0;
}

static USEASM_OPCODE GetIntegerDOTOpcode(PINTERMEDIATE_STATE psState, PINST psIn)
/*********************************************************************************
 Function			: GetIntegerDOTOpcode

 Description		: Get the USEASM opcode corresponding to a fixed-point dotproduct 
					  intermediate instruction.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate dot-product instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	ASSERT(psIn->u.psDot34->uVecLength == 3 || psIn->u.psDot34->uVecLength == 4);

	if (psIn->eOpcode == IFP16DOT)
	{
		if (psIn->u.psDot34->bOffset)
		{
			if (psIn->u.psDot34->uVecLength == 3)
			{
				return USEASM_OP_U16DOT3OFF;
			}
			else
			{
				return USEASM_OP_U16DOT4OFF;
			}
		}
		else
		{
			if (psIn->u.psDot34->uVecLength == 3)
			{
				return USEASM_OP_U16DOT3;
			}
			else
			{
				return USEASM_OP_U16DOT4;
			}
		}
	}
	else
	{
		if (psIn->u.psDot34->bOffset)
		{
			if (psIn->u.psDot34->uVecLength == 3)
			{
				return USEASM_OP_U8DOT3OFF;
			}
			else
			{
				return USEASM_OP_U8DOT4OFF;
			}
		}
		else
		{
			if (psIn->u.psDot34->uVecLength == 3)
			{
				return USEASM_OP_U8DOT3;
			}
			else
			{
				return USEASM_OP_U8DOT4;
			}
		}
	}
}


#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static IMG_VOID ConvertSGX543NonVectorPack(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertSGX543NonVectorPack

 Description		: Convert a SGX545 PCK instruction with non-vector sources
					  to USEASM.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate PCK instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	psOut->uOpcode = GetUseasmOpcodeForVectorOp(psState, psIn, psOut); 

	/*
		Set up the scale flag.
	*/
	if (psIn->u.psVec->bPackScale)
	{
		psOut->uFlags2 |= USEASM_OPFLAGS2_SCALE; 
	}

	/*
		Copy the register destination.
	*/
	ConvertDest(psState, psIn, psOutSrc);
	psOutSrc++;

	/*
		Copy the register sources.
	*/
	psOutSrc = CopySources(psState, psIn, psOutSrc);

	/*
		Copy the source swizzle.
	*/
	CopySwizzle(psOutSrc, psIn->u.psVec->auSwizzle[0]);
}

static IMG_VOID CopyVectorSourceModifier(PVEC_SOURCE_MODIFIER	psSrcMod,
										 PUSE_REGISTER			psOutSrc)
/*********************************************************************************
 Function			: CopyVectorSourceModifier

 Description		: Convert a source modifier to a vector instruction to the	
					  USEASM format.
 
 Parameters			: psSrcMod			- Source modifier to copy.
					  psOutSrc			- Updated with the source modifier.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	if (psSrcMod->bAbs)
	{
		psOutSrc->uFlags |= USEASM_ARGFLAGS_ABSOLUTE;
	}
	if (psSrcMod->bNegate)
	{
		psOutSrc->uFlags |= USEASM_ARGFLAGS_NEGATE;
	}
}

static IMG_VOID ConvertVectorDualIssueOperation(PINTERMEDIATE_STATE psState, 
												PINST				psIn, 
												IMG_BOOL			bSecondary,
												IMG_UINT32			auSrcMap[],
												PUSE_INST			psOut)
/*********************************************************************************
 Function			: ConvertVectorDualIssueOperation

 Description		: Convert one of the operation in a dual-issue instruction
					  to the USEASM input format.
 
 Parameters			: psState		- Compiler state.
					  psIn			- Intermediate dual-issue instruction.
					  bSecondary	- TRUE if this is the secondary operation.
									  FALSE if this is the primary operation.
					  auSrcMap		- Maps from the inputs to the operation
									to the encoding slot used.
					  psOut			- Destination for the converted operation.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	VDUAL_OP		eOp = bSecondary ? psIn->u.psVec->sDual.eSecOp : psIn->u.psVec->sDual.ePriOp;
	IMG_UINT32		uArgIdx;
	IMG_UINT32		uBaseDestIdx;
	IMG_BOOL		bCopyGPIDestination;
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Copy the type of the operation.
	*/
	psOut->uOpcode = GetUseasmOpcodeForDualIssueOp(psState, eOp);

	/*
		Which destination does this operation write?
	*/
	if (bSecondary)
	{
		bCopyGPIDestination = !psIn->u.psVec->sDual.bPriUsesGPIDest;
	}
	else
	{
		bCopyGPIDestination = psIn->u.psVec->sDual.bPriUsesGPIDest;
	}

	/*
		Get the start of the destination in the dual-issue instruction's array
		of destination registers.
	*/
	if (bCopyGPIDestination)
	{
		uBaseDestIdx = VDUAL_DESTSLOT_GPI * 4;	
	}
	else
	{
		uBaseDestIdx = VDUAL_DESTSLOT_UNIFIEDSTORE * 4;
	}

	/*
		Copy the destination write mask.
	*/
	psOut->uFlags1 |= (CopyVectorWriteMask(psState, psIn,  uBaseDestIdx) << USEASM_OPFLAGS1_MASK_SHIFT);

	/*
		Copy the destination.
	*/
	ASSERT(psIn->uDestCount >= 5);
	CopyDestRegister(psState, psIn, uBaseDestIdx, psOutSrc);

	if (!bCopyGPIDestination)
	{
		/*
			Tell the assembler to use the unified store destination.
		*/
		psOutSrc->uFlags |= USEASM_ARGFLAGS_REQUIREMOE;
	}

	psOutSrc++;

	/*
		Copy the sources.
	*/
	for (uArgIdx = 0; uArgIdx < g_asDualIssueOpDesc[eOp].uSrcCount; uArgIdx++)
	{
		IMG_UINT32	uSrcIdx = auSrcMap[uArgIdx];

		CopySource(psState, psIn, uSrcIdx * 5 + 1, psOutSrc);
		CopyVectorSourceModifier(&psIn->u.psVec->asSrcMod[uSrcIdx], psOutSrc);
		
		if (uSrcIdx == VDUAL_SRCSLOT_GPI2)
		{
			/*
				Set a flag to tell the assembler to use the GPI2 slot for
				this argument.
			*/
			psOutSrc->uFlags |= USEASM_ARGFLAGS_REQUIRESGPI2;
		}

		if (uSrcIdx == VDUAL_SRCSLOT_UNIFIEDSTORE)
		{
			/*
				Set a flag to tell the assembler to use the unified store source slot for
				this argument.
			*/
			psOutSrc->uFlags |= USEASM_ARGFLAGS_REQUIREMOE;
		}

		if (g_asDualIssueOpDesc[eOp].bVector)
		{
			psOutSrc++;

			CopySwizzle(psOutSrc, psIn->u.psVec->auSwizzle[uSrcIdx]);
			psOutSrc++;
		}
		else
		{
			IMG_UINT32	uSwizChan;

			/*	
				Convert the swizzle on a scalar instruction to a component select.
			*/
			uSwizChan = 
				(psIn->u.psVec->auSwizzle[uSrcIdx] >> (USEASM_SWIZZLE_FIELD_SIZE * 0)) & USEASM_SWIZZLE_VALUE_MASK;
			ASSERT(uSwizChan >= USEASM_SWIZZLE_SEL_X && uSwizChan <= USEASM_SWIZZLE_SEL_W);

			psOutSrc->uFlags |= (uSwizChan << USEASM_ARGFLAGS_COMP_SHIFT);
			psOutSrc++;
		}
	}
}

static IMG_VOID ConvertVectorDualIssueInst(PINTERMEDIATE_STATE	psState, 
										   PINST				psIn, 
										   PUSE_INST			psPriInst)
/*********************************************************************************
 Function			: ConvertVectorDualIssueInst

 Description		: Convert a vector dual-issue instruction to USEASM.
 
 Parameters			: psState		- Compiler state.
					  psIn			- Intermediate dual-issue instruction.
					  psPriInst		- USEASM instruction representing the primary
									operation.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSE_INST	psSecInst;

	/*	
		Convert the secondary operation to the USEASM representation.
	*/
	psSecInst = psPriInst->psNext;
	psSecInst->uFlags1 = 0;
	psSecInst->uFlags2 = USEASM_OPFLAGS2_COISSUE;
	psSecInst->uFlags3 = 0;
	ConvertVectorDualIssueOperation(psState, 
									psIn, 
									IMG_TRUE /* bSecondary */, 
									psIn->u.psVec->sDual.auSecOpSrcs, 
									psSecInst);

	/*
		Set the interpretation of unified store sources/destinations:
		either F16 or F32.
	*/
	if (psIn->u.psVec->sDual.bF16)
	{
		psPriInst->uFlags1 |= USEASM_OPFLAGS1_MOVC_FMTF16;
	}

	/*
		Convert the primary operation to the USEASM representation.
	*/
	psPriInst->uFlags1 |= USEASM_OPFLAGS1_MAINISSUE;
	ConvertVectorDualIssueOperation(psState, 
									psIn, 
									IMG_FALSE /* bSecondary */, 
									psIn->u.psVec->sDual.auPriOpSrcs, 
									psPriInst);
}

static IMG_VOID ConvertVTESTInst(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertVTESTInst

 Description		: Convert a vector TEST instruction to USEASM.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate vector instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32		uArg;
	PUSE_REGISTER	psOutSrc = psOut->asArg;
	IMG_UINT32		auSwizzle[2];
	IMG_UINT32		uVectorArgCount = GetSwizzleSlotCount(psState, psIn);

	ASSERT(psIn->eOpcode == IVTEST);

	/*
		Set the USEASM opcode.
	*/
	psOut->uOpcode = GetUseasmOpcodeForVectorOp(psState, psIn, psOut);

	/*
		Copy the register destination.
	*/
	psOutSrc = CopyVectorTestDestinations(psState, psIn, psOut, psOutSrc);

	/*
		Set up the onceonly flag.
	*/	
	if (GetBit(psIn->auFlag, INST_ONCEONLY))
	{
		psOut->uFlags2 |= USEASM_OPFLAGS2_ONCEONLY;
	}

	/*
		Copy the details of the test to perform.
	*/
	psOut->uFlags1 |= USEASM_OPFLAGS1_TESTENABLE; 
	psOut->uTest = ConvertTestTypeToUseasm(psState, psIn->u.psVec->sTest.eType);
	psOut->uTest |= (USEASM_TEST_MASK_NONE << USEASM_TEST_MASK_SHIFT);

	ASSERT(uVectorArgCount == 1 || uVectorArgCount == 2);
	for (uArg = 0; uArg < uVectorArgCount; uArg++)
	{
		auSwizzle[uArg] = psIn->u.psVec->auSwizzle[uArg];
	}
	if (uVectorArgCount == 1)
	{
		auSwizzle[1] = auSwizzle[0];
	}

	/*
		Copy the channel selector.
	*/
	if (IsVTESTExtraSwizzles(psState, psIn->eOpcode, psIn))
	{
		USEASM_TEST_CHANSEL	eChanSel;

		ASSERT(auSwizzle[1] == auSwizzle[0] || auSwizzle[1] == USEASM_SWIZZLE(X, X, X, X));
		
		switch (auSwizzle[0])
		{
			case USEASM_SWIZZLE(X, X, X, X): eChanSel = USEASM_TEST_CHANSEL_C0; break;
			case USEASM_SWIZZLE(Y, Y, Y, Y): eChanSel = USEASM_TEST_CHANSEL_C1; break;
			case USEASM_SWIZZLE(Z, Z, Z, Z): eChanSel = USEASM_TEST_CHANSEL_C2; break;
			case USEASM_SWIZZLE(W, W, W, W): eChanSel = USEASM_TEST_CHANSEL_C3; break;
			default: imgabort();
		}

		if (auSwizzle[1] == auSwizzle[0])
		{
			auSwizzle[1] = USEASM_SWIZZLE(X, Y, Z, W);
		}
		auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);

		psOut->uTest |= (eChanSel << USEASM_TEST_CHANSEL_SHIFT);
	}
	else
	{
		ASSERT(auSwizzle[0] == USEASM_SWIZZLE(X, Y, Z, W));
		ASSERT(auSwizzle[1] == USEASM_SWIZZLE(X, Y, Z, W) || auSwizzle[1] == USEASM_SWIZZLE(X, X, X, X));

		switch (psIn->u.psVec->sTest.eChanSel)
		{
			case VTEST_CHANSEL_PERCHAN: psOut->uTest |= (USEASM_TEST_CHANSEL_PERCHAN << USEASM_TEST_CHANSEL_SHIFT); break;
			case VTEST_CHANSEL_ANDALL: psOut->uTest |= (USEASM_TEST_CHANSEL_ANDALL << USEASM_TEST_CHANSEL_SHIFT); break;
			case VTEST_CHANSEL_ORALL: psOut->uTest |= (USEASM_TEST_CHANSEL_ORALL << USEASM_TEST_CHANSEL_SHIFT); break;
			default: imgabort();
		}
	}

	/*
		Copy details of the source arguments.
	*/
	for (uArg = 0; uArg < uVectorArgCount; uArg++)
	{
		IMG_UINT32	uArgStart = uArg * SOURCE_ARGUMENTS_PER_VECTOR;

		ASSERT(psIn->asArg[uArgStart].uType == USC_REGTYPE_UNUSEDSOURCE);

		CopySource(psState, psIn, uArgStart + 1, psOutSrc);
		CopyVectorSourceModifier(&psIn->u.psVec->asSrcMod[uArg], psOutSrc);
		psOutSrc++;

		/*
			Copy the swizzle corresponding to this source argument.
		*/
		CopySwizzle(psOutSrc, auSwizzle[uArg]);
		psOutSrc++;
	}
}

static IMG_VOID ConvertVectorSourceInst(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertVectorSourceInst

 Description		: Convert a instruction with vector sources
					  to USEASM.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate vector instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32		uVectorArgCount;
	IMG_UINT32		i;
	IMG_BOOL		bOneSwizzle;
	IMG_UINT32		uOneSwizzle;
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	psOut->uOpcode = GetUseasmOpcodeForVectorOp(psState, psIn, psOut);

	/*
		Copy the register destination.
	*/
	ConvertDest(psState, psIn, psOutSrc);
	
	/*
		For complex-ops copy the destination data type.
	*/
	if (psIn->eOpcode == IVRCP || psIn->eOpcode == IVLOG || psIn->eOpcode == IVEXP || psIn->eOpcode == IVRSQ)
	{
		if (psIn->asDest[0].eFmt == UF_REGFORMAT_F16)
		{
			psOutSrc->uFlags |= USEASM_ARGFLAGS_FMTF16;
		}
		if (psIn->asDest[0].eFmt == UF_REGFORMAT_C10)
		{
			psOutSrc->uFlags |= USEASM_ARGFLAGS_FMTC10;
		}
	}
	
	psOutSrc++;

	/*
		Set up the scale flag.
	*/
	if (psIn->u.psVec->bPackScale)
	{
		psOut->uFlags2 |= USEASM_OPFLAGS2_SCALE; 
	}

	if (psIn->eOpcode == IVTESTMASK ||
		psIn->eOpcode == IVTESTBYTEMASK)
	{
		psOut->uFlags1 |= USEASM_OPFLAGS1_TESTENABLE; 
		psOut->uTest = ConvertTestTypeToUseasm(psState, psIn->u.psVec->sTest.eType);
		psOut->uTest |= (psIn->u.psVec->sTest.eMaskType << USEASM_TEST_MASK_SHIFT);
	}
	if ((psIn->eOpcode == IVMOVC) || (psIn->eOpcode == IVMOVCU8_FLT))
	{
		psOut->uFlags1 |= USEASM_OPFLAGS1_TESTENABLE; 
		psOut->uTest = ConvertTestTypeToUseasm(psState, psIn->u.psVec->eMOVCTestType);
	}

	/*
		Check for cases where the assembler expects only one swizzle at the end 
		of the argument list.
	*/
	bOneSwizzle = IMG_FALSE;
	uOneSwizzle = USC_UNDEF;
	if (psIn->eOpcode == IVMOVC ||
		psIn->eOpcode == IVMOVCU8_FLT ||
		psIn->eOpcode == IVPCKU8FLT_EXP ||
		psIn->eOpcode == IVPCKC10FLT_EXP ||
		psIn->eOpcode == IVPCKFLTFLT_EXP ||
		psIn->eOpcode == IVPCKS16FLT_EXP ||
		psIn->eOpcode == IVPCKU16FLT_EXP)
	{
		bOneSwizzle = IMG_TRUE;

		switch (psIn->eOpcode)
		{
			case IVPCKU8FLT_EXP:
			case IVPCKC10FLT_EXP:
			case IVPCKFLTFLT_EXP:
			case IVPCKS16FLT_EXP:
			case IVPCKU16FLT_EXP:
			{
				IMG_UINT32	uChan;

				uOneSwizzle = 0;
				for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
				{
					USEASM_SWIZZLE_SEL	ePackSel;
					USEASM_SWIZZLE_SEL	eSrcSel;
					IMG_BOOL			bRemapXYToZW;

					ePackSel = GetChan(psIn->u.psVec->uPackSwizzle, uChan);
					switch (ePackSel)
					{
						case USEASM_SWIZZLE_SEL_X: 
						{
							eSrcSel = GetChan(psIn->u.psVec->auSwizzle[0], 0 /* uChan */);
							bRemapXYToZW = IMG_FALSE;
							break;
						}
						case USEASM_SWIZZLE_SEL_Y: 
						{
							eSrcSel = GetChan(psIn->u.psVec->auSwizzle[0], 1 /* uChan */);
							bRemapXYToZW = IMG_FALSE;
							break;
						}
						case USEASM_SWIZZLE_SEL_Z: 
						{
							eSrcSel = GetChan(psIn->u.psVec->auSwizzle[1], 0 /* uChan */);
							bRemapXYToZW = IMG_TRUE;
							break;
						}
						case USEASM_SWIZZLE_SEL_W: 
						{
							eSrcSel = GetChan(psIn->u.psVec->auSwizzle[1], 1 /* uChan */);
							bRemapXYToZW = IMG_TRUE;
							break;
						}
						default: imgabort();
					}

					ASSERT(eSrcSel == USEASM_SWIZZLE_SEL_X || eSrcSel == USEASM_SWIZZLE_SEL_Y);
					if (bRemapXYToZW)
					{
						switch (eSrcSel)
						{
							case USEASM_SWIZZLE_SEL_X: eSrcSel = USEASM_SWIZZLE_SEL_Z; break;
							case USEASM_SWIZZLE_SEL_Y: eSrcSel = USEASM_SWIZZLE_SEL_W; break;
							default: imgabort();
						}
					}

					uOneSwizzle = SetChan(uOneSwizzle, uChan, eSrcSel);
				}
				break;
			}
			case IVMOVC:
			case IVMOVCU8_FLT:
			{
				uOneSwizzle = psIn->u.psVec->auSwizzle[1]; 
				break;
			}
			default: imgabort();
		}
	}

	/*
		Get the number of source arguments.
	*/
	uVectorArgCount = GetSwizzleSlotCount(psState, psIn);
		
	/*
		Copy source arguments.
	*/
	for (i = 0; i < uVectorArgCount; i++)
	{	
		if (i==1 && psIn->eOpcode == IVADD3)
		{
			/*
				Create a dummy ADD operand for VMAD
			*/
			psOutSrc->uNumber = 0;
			psOutSrc->uType = USEASM_REGTYPE_FPINTERNAL;
			psOutSrc->uIndex = USEREG_INDEX_NONE;
			psOutSrc->uFlags = 0;
			psOutSrc++;

			CopySwizzle(psOutSrc, USEASM_SWIZZLE(1, 1, 1, 1));
			psOutSrc++;
		}

		ASSERT(psIn->asArg[i * 5].uType == USC_REGTYPE_UNUSEDSOURCE);
		CopySource(psState, psIn, i * 5 + 1, psOutSrc);
		CopyVectorSourceModifier(&psIn->u.psVec->asSrcMod[i], psOutSrc);
		
		if (psIn->eOpcode == IVRCP ||
			psIn->eOpcode == IVRSQ ||
			psIn->eOpcode == IVLOG ||
			psIn->eOpcode == IVEXP)
		{
			IMG_UINT32	uSwizChan;

			uSwizChan = (psIn->u.psVec->auSwizzle[0] >> (USEASM_SWIZZLE_FIELD_SIZE * 3)) & USEASM_SWIZZLE_VALUE_MASK;
			ASSERT(uSwizChan <= USEASM_SWIZZLE_SEL_W);
			psOutSrc->uFlags |= (uSwizChan << USEASM_ARGFLAGS_COMP_SHIFT);
		}
		psOutSrc++;

		/*
			Add an extra, unreferenced source for PCK instructions with a
			F32 format, GPI register source.
		*/
		if (
				(
					psIn->eOpcode == IVPCKU8FLT ||
					psIn->eOpcode == IVPCKS8FLT ||
					psIn->eOpcode == IVPCKC10FLT ||
					psIn->eOpcode == IVPCKS16FLT ||
					psIn->eOpcode == IVPCKU16FLT ||
					psIn->eOpcode == IVPCKFLTFLT
				) &&
				psIn->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F32
			)
		{
			UseAsmInitReg(psOutSrc);
			psOutSrc->uType = USEASM_REGTYPE_IMMEDIATE;
			psOutSrc->uNumber = 0;
			psOutSrc++;
		}

		/*
			Copy the swizzle corresponding to this source argument.
		*/
		if (!bOneSwizzle &&
			psIn->eOpcode != IVRCP &&
			psIn->eOpcode != IVRSQ &&
			psIn->eOpcode != IVLOG &&
			psIn->eOpcode != IVEXP)
		{
			CopySwizzle(psOutSrc, psIn->u.psVec->auSwizzle[i]);
			psOutSrc++;
		}

		if (i==1 && psIn->eOpcode == IVMUL3)
		{
			/*
				Create a dummy ADD operand for VMAD
			*/
			psOutSrc->uNumber = 0;
			psOutSrc->uType = USEASM_REGTYPE_FPINTERNAL;
			psOutSrc->uIndex = USEREG_INDEX_NONE;
			psOutSrc->uFlags = 0;
			psOutSrc++;

			CopySwizzle(psOutSrc, USEASM_SWIZZLE(0, 0, 0, 0));
			psOutSrc++;
		}
	}

	if (bOneSwizzle)
	{
		CopySwizzle(psOutSrc, uOneSwizzle);
		psOutSrc++;
	}
	
	if (psIn->eOpcode == IVDP_CP)
	{
		/*
			Add an extra source with the number of the clipplane P-flag to write.
		*/
		psOutSrc->uType = USEASM_REGTYPE_CLIPPLANE;
		psOutSrc->uNumber = psIn->u.psVec->uClipPlaneOutput;
		psOutSrc->uFlags = 0;
		psOutSrc->uIndex = USEREG_INDEX_NONE;
		psOutSrc++;
	}
	if (psIn->eOpcode == IVDP_GPI || psIn->eOpcode == IVDP3_GPI)
	{
		/*
			Add an extra source that tells the assembler this instruction doesn't update
			any clipplane P flag.
		*/
		psOutSrc->uType = USEASM_REGTYPE_INTSRCSEL;
		psOutSrc->uNumber = USEASM_INTSRCSEL_NONE;
		psOutSrc->uFlags = 0;
		psOutSrc->uIndex = USEREG_INDEX_NONE;
		psOutSrc++;
	}
	if (psIn->eOpcode == IVDP_CP || 
		psIn->eOpcode == IVDP_GPI ||
		psIn->eOpcode == IVDP3_GPI ||
		psIn->eOpcode == IVADD3 || 
		psIn->eOpcode == IVMUL3 || 
		psIn->eOpcode == IVMAD4)
	{
		/*
			Add an extra source with the GPI/unified store MOE increment mode.
		*/
		psOutSrc->uType = USEASM_REGTYPE_INTSRCSEL;
		switch (psIn->u.psVec->eRepeatMode)
		{
			case SVEC_REPEATMODE_USEMOE: psOutSrc->uNumber = USEASM_INTSRCSEL_INCREMENTMOE; break;
			case SVEC_REPEATMODE_INCREMENTGPI: psOutSrc->uNumber = USEASM_INTSRCSEL_INCREMENTGPI; break;
			case SVEC_REPEATMODE_INCREMENTUS: psOutSrc->uNumber = USEASM_INTSRCSEL_INCREMENTUS; break;
			case SVEC_REPEATMODE_INCREMENTBOTH: psOutSrc->uNumber = USEASM_INTSRCSEL_INCREMENTBOTH; break;
			default: imgabort();
		}
		psOutSrc->uFlags = 0;
		psOutSrc->uIndex = USEREG_INDEX_NONE;
	}
}

static IMG_VOID ConvertIDXSCInst(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertIDXSCInst

 Description		: Convert an IDXSCR or IDXSCW instruction to USEASM.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate vector instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32		uIndexNumber;
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	ASSERT(g_psInstDesc[psIn->eOpcode].uUseasmOpcode != (IMG_UINT32)-1);
	psOut->uOpcode = g_psInstDesc[psIn->eOpcode].uUseasmOpcode; 

	/*
		Copy the destination.
	*/
	ConvertDest(psState, psIn, psOutSrc);
	if (psIn->eOpcode == IIDXSCW)
	{
		/*
			USEASM takes the index in a separate parameter.
		*/
		psOutSrc->uIndex = USEREG_INDEX_NONE;
	}
	psOutSrc++;

	/*
		Set up the index register source.
	*/
	psOutSrc->uType = USEASM_REGTYPE_INDEX;
	if (psIn->eOpcode == IIDXSCW)
	{
		ASSERT(psIn->asDest[0].uIndexType == USEASM_REGTYPE_INDEX);
		uIndexNumber = psIn->asDest[0].uIndexNumber;
	}
	else
	{
		ASSERT(psIn->asArg[0].uIndexType == USEASM_REGTYPE_INDEX);
		uIndexNumber = psIn->asArg[0].uIndexNumber;
	}
	switch (uIndexNumber)
	{
		case USC_INDEXREG_LOW: psOutSrc->uNumber = USEREG_INDEX_L; break;
		case USC_INDEXREG_HIGH: psOutSrc->uNumber = USEREG_INDEX_H; break;
		default: imgabort();
	}
	psOutSrc->uFlags = 0;
	psOutSrc->uIndex = USEREG_INDEX_NONE;
	psOutSrc++;

	/*
		Copy the source.
	*/
	CopySource(psState, psIn, 0 /* uArg */, psOutSrc);
	if (psIn->eOpcode == IIDXSCR)
	{
		/*
			USEASM takes the index in a separate parameter.
		*/
		psOutSrc->uIndex = USEREG_INDEX_NONE;
	}
	psOutSrc++;

	/*
		Copy the width of the channels of the indexed type.
	*/
	psOutSrc->uType = USEASM_REGTYPE_IMMEDIATE;
	psOutSrc->uNumber = 8;
	psOutSrc->uIndex = USEREG_INDEX_NONE;
	psOutSrc->uFlags = 0;
	psOutSrc++;

	/*
		Copy the count of channels in the indexed type.
	*/
	psOutSrc->uType = USEASM_REGTYPE_IMMEDIATE;
	psOutSrc->uNumber = psIn->u.psIdxsc->uChannelCount;
	psOutSrc->uIndex = USEREG_INDEX_NONE;
	psOutSrc->uFlags = 0;
	psOutSrc++;
}

static IMG_VOID ConvertC10VectorSourceInst(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertC10VectorSourceInst

 Description		: Convert a instruction with C10 vector sources
					  to USEASM.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate vector instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32		uVectorArgCount;
	IMG_UINT32		i;
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	if (psIn->eOpcode == IFPDOT_VEC)
	{
		psOut->uOpcode = GetIntegerDOTOpcode(psState, psIn);
	}
	else if (psIn->eOpcode == IFPTESTPRED_VEC || psIn->eOpcode == IFPTESTMASK_VEC)
	{
		ASSERT(g_psInstDesc[psIn->u.psTest->eAluOpcode].uUseasmOpcode != (IMG_UINT32)-1);
		psOut->uOpcode = g_psInstDesc[psIn->u.psTest->eAluOpcode].uUseasmOpcode; 
		psOut->uFlags1 |= USEASM_OPFLAGS1_TESTENABLE;
		psOut->uTest = ConvertTestToUseasm(psState, &psIn->u.psTest->sTest);
	}
	else if (g_psInstDesc[psIn->eOpcode].uFlags & DESC_FLAGS_VECTOR)
	{
		psOut->uOpcode = GetUseasmOpcodeForVectorOp(psState, psIn, psOut);
	}
	else
	{
		ASSERT(g_psInstDesc[psIn->eOpcode].uUseasmOpcode != (IMG_UINT32)-1);
		psOut->uOpcode = g_psInstDesc[psIn->eOpcode].uUseasmOpcode;  
	}

	if (psIn->eOpcode == IFPTESTPRED_VEC)
	{
		/*
			Copy predicate and unified store destinations.
		*/
		psOutSrc = CopyTestDestinations(psState, psIn, psOut, psOutSrc);
	}
	else
	{
		/*
			Copy the register destination.
		*/
		ConvertDest(psState, psIn, psOutSrc);
		psOutSrc++;
	}

	/*
		Copy DOT34 destination modifiers.
	*/
	if (psIn->eOpcode == IFPDOT_VEC)
	{
		psOutSrc = CopyDOT34DestModToUseasm(psIn, psOutSrc);
	}

	/*
		Copy the register sources.
	*/
	uVectorArgCount = psIn->uArgumentCount / 2;
		
	for (i = 0; i < uVectorArgCount; i++)
	{
		CopySource(psState, psIn, i * 2, psOutSrc);

		if (psIn->eOpcode == IFPMA_VEC && i == 0 && psIn->u.psFpma->bNegateSource0)
		{
			psOutSrc->uFlags |= USEASM_ARGFLAGS_NEGATE;
		}

		psOutSrc++;
	}

	switch (psIn->eOpcode)
	{
		case IFPMA_VEC:
		{
			EncodeFPMAModes(psIn, psOutSrc, psOut);
			break;
		}
		case ISOP2_VEC:
		{
			EncodeSOP2Modes(psIn, psOutSrc, psOut);
			break;
		}
		case ISOPWM_VEC:
		{
			EncodeSOPWMModes(psIn, psOutSrc);

			psOut->asArg[0].uFlags |= (psIn->auDestMask[0] << USEASM_ARGFLAGS_BYTEMSK_SHIFT) | USEASM_ARGFLAGS_BYTEMSK_PRESENT;
			break;
		}
		case ISOP3_VEC:
		{
			EncodeSOP3Modes(psState, psIn, psOutSrc, psOut);
			break;
		}
		case ILRP1_VEC:
		{
			EncodeLRP1Modes(psIn, psOutSrc, psOut);
			break;
		}
		case IVMOVC10:
		{
			psOut->uFlags2 |= USEASM_OPFLAGS2_MOVC_FMTI10;

			CopySwizzle(psOutSrc, psIn->u.psVec->auSwizzle[0]);
			psOutSrc++;

			psOut->uFlags1 &= USEASM_OPFLAGS1_MASK_CLRMSK;
			psOut->uFlags1 |= (CopyVectorWriteMask(psState, psIn, 0 /* uBaseDestIdx */) << USEASM_OPFLAGS1_MASK_SHIFT);
			break;
		}
		case IVPCKC10C10:
		{
			CopySwizzle(psOutSrc, psIn->u.psVec->auSwizzle[0]);
			break;
		}
		case IVPCKFLTC10_VEC:
		{
			psOut->uFlags2 |= USEASM_OPFLAGS2_SCALE;

			CopySwizzle(psOutSrc, psIn->u.psVec->auSwizzle[0]);
			break;
		}
		case IVMOVCC10:
		{
			CopySwizzle(psOutSrc, psIn->u.psVec->auSwizzle[0]);

			psOut->uFlags1 |= (CopyVectorWriteMask(psState, psIn, 0 /* uBaseDestIdx */) << USEASM_OPFLAGS1_MASK_SHIFT);

			psOut->uFlags1 |= USEASM_OPFLAGS1_TESTENABLE; 
			psOut->uTest = ConvertTestTypeToUseasm(psState, psIn->u.psVec->eMOVCTestType);
			psOut->uFlags2 |= USEASM_OPFLAGS2_MOVC_FMTI10;
			break;
		}
		case IVMOVCU8:
		{
			CopySwizzle(psOutSrc, psIn->u.psVec->auSwizzle[0]);

			ASSERT(psIn->uDestCount == 1);
			ASSERT(psIn->asDest[0].eFmt == UF_REGFORMAT_U8);
			psOut->uFlags1 |= (psIn->auDestMask[0] << USEASM_OPFLAGS1_MASK_SHIFT);

			psOut->uFlags1 |= USEASM_OPFLAGS1_TESTENABLE; 
			psOut->uTest = ConvertTestTypeToUseasm(psState, psIn->u.psVec->eMOVCTestType);
			psOut->uFlags1 |= USEASM_OPFLAGS1_MOVC_FMTI8;
			break;
		}
		case IFPADD8_VEC:
		case IFPSUB8_VEC:
		{
			break;
		}
		case IFPDOT_VEC:
		{
			break;
		}
		case IFPTESTPRED_VEC:
		case IFPTESTMASK_VEC:
		{
			/* No special parameters. */
			break;
		}
		default: imgabort();
	}
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static IMG_VOID ConvertSOPWMToUseasm(PINTERMEDIATE_STATE	psState,
									 PINST					psIn,
									 PUSE_INST				psOut)
/*********************************************************************************
 Function			: ConvertSOPWMToUseasm

 Description		: Convert a SOPWM instruction to USEASM.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate vector instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32		uWriteMask;
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	psOut->uOpcode = USEASM_OP_SOP2WM; 

	/*
		Copy the register destination.
	*/
	ConvertDest(psState, psIn, psOutSrc);

	/*
		Copy the SOPWM destination write mask.
	*/
	if (psIn->u.psSopWm->bRedChanFromAlpha)
	{
		ASSERT(psIn->auDestMask[0] == USC_ALL_CHAN_MASK);
		ASSERT(psIn->auLiveChansInDest[0] == USC_RED_CHAN_MASK);
		uWriteMask = USC_ALPHA_CHAN_MASK;
	}
	else
	{
		uWriteMask = psIn->auDestMask[0];
	}
	psOutSrc->uFlags |= (uWriteMask << USEASM_ARGFLAGS_BYTEMSK_SHIFT) | USEASM_ARGFLAGS_BYTEMSK_PRESENT;

	psOutSrc++;

	/*
		Copy the register sources.
	*/
	psOutSrc = CopySources(psState, psIn, psOutSrc);

	/*
		Encode the instruction calculation.
	*/
	EncodeSOPWMModes(psIn, psOutSrc);
}

static IMG_VOID ConvertFPMAIMA8ToUseasm(PINTERMEDIATE_STATE	psState,
										PINST				psIn,
										PUSE_INST			psOut)
/*********************************************************************************
 Function			: ConvertFPMAIMA8ToUseasm

 Description		: Convert a FPMA or IMA8 instruction to USEASM.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate vector instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32		uArg;
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	psOut->uOpcode = g_psInstDesc[psIn->eOpcode].uUseasmOpcode; 

	/*
		Copy the register destination.
	*/
	ConvertDest(psState, psIn, psOutSrc);
	psOutSrc++;

	/*
		Copy the register sources.
	*/
	for (uArg = 0; uArg < psIn->uArgumentCount; uArg++)
	{
		CopySource(psState, psIn, uArg, psOutSrc);
		if (uArg == 0 && psIn->u.psFpma->bNegateSource0)
		{
			psOutSrc->uFlags |= USEASM_ARGFLAGS_NEGATE;
		}
		psOutSrc++;
	}

	/*
		Encode the instruction calculation.
	*/
	EncodeFPMAModes(psIn, psOutSrc, psOut);
}

static IMG_VOID ConvertLRP1ToUseasm(PINTERMEDIATE_STATE	psState,
									PINST				psIn,
									PUSE_INST			psOut)
/*********************************************************************************
 Function			: ConvertLRP1ToUseasm

 Description		: Convert a LRP1 instruction to USEASM.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate vector instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	psOut->uOpcode = USEASM_OP_LRP1; 

	/*
		Copy the register destination.
	*/
	ConvertDest(psState, psIn, psOutSrc);
	psOutSrc++;

	/*
		Copy the register sources.
	*/
	psOutSrc = CopySources(psState, psIn, psOutSrc);

	/*
		Encode the instruction calculation.
	*/
	EncodeLRP1Modes(psIn, psOutSrc, psOut);
}

static IMG_VOID ConvertSOP2ToUseasm(PINTERMEDIATE_STATE	psState,
									PINST				psIn,
									PUSE_INST			psOut)
/*********************************************************************************
 Function			: ConvertSOP2ToUseasm

 Description		: Convert a SOP2 instruction to USEASM.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate vector instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	psOut->uOpcode = USEASM_OP_SOP2; 

	/*
		Copy the register destination.
	*/
	ConvertDest(psState, psIn, psOutSrc);
	psOutSrc++;

	/*
		Copy the register sources.
	*/
	psOutSrc = CopySources(psState, psIn, psOutSrc);

	/*
		Encode the instruction calculation.
	*/
	EncodeSOP2Modes(psIn, psOutSrc, psOut);
}

static IMG_VOID ConvertSOP3ToUseasm(PINTERMEDIATE_STATE	psState,
									PINST				psIn,
									PUSE_INST			psOut)
/*********************************************************************************
 Function			: ConvertSOP3ToUseasm

 Description		: Convert a SOP3 instruction to USEASM.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate vector instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	psOut->uOpcode = USEASM_OP_SOP3; 

	/*
		Copy the register destination.
	*/
	ConvertDest(psState, psIn, psOutSrc);
	psOutSrc++;

	/*
		Copy the register sources.
	*/
	psOutSrc = CopySources(psState, psIn, psOutSrc);

	/*
		Encode the instruction calculation.
	*/
	EncodeSOP3Modes(psState, psIn, psOutSrc, psOut);
}

static PUSE_REGISTER ConvertPCKInstructionToUseasm(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertPCKInstructionToUseasm

 Description		: Convert a PCK instruction to USEASM.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate PCK instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32		uWriteMask;
	IMG_UINT32		uSrcIdx;
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	ASSERT(g_psInstDesc[psIn->eOpcode].uUseasmOpcode != (IMG_UINT32)-1);
	psOut->uOpcode = g_psInstDesc[psIn->eOpcode].uUseasmOpcode; 

	/*
		Copy the register destination.
	*/
	ConvertDest(psState, psIn, psOutSrc);

	/*
		Copy the destination write mask.
	*/
	uWriteMask = psIn->auDestMask[0];
	psOutSrc->uFlags |= (uWriteMask << USEASM_ARGFLAGS_BYTEMSK_SHIFT) | USEASM_ARGFLAGS_BYTEMSK_PRESENT;

	psOutSrc++;

	/*
		Set up the scale flag.
	*/
	if (psIn->u.psPck->bScale)
	{
		psOut->uFlags2 |= USEASM_OPFLAGS2_SCALE; 
	}

	/*
		Copy the register sources.
	*/
	for (uSrcIdx = 0; uSrcIdx < psIn->uArgumentCount; uSrcIdx++, psOutSrc++)
	{
		CopySource(psState, psIn, uSrcIdx, psOutSrc);
		psOutSrc->uFlags |= (psIn->u.psPck->auComponent[uSrcIdx] << USEASM_ARGFLAGS_COMP_SHIFT);
	}
	
	/*
		Add an extra argument for any unreferenced source.
	*/
	if (psIn->uArgumentCount < PCK_SOURCE_COUNT)
	{
		CopyImmediate(psOutSrc, 0 /* uValue */);
		psOutSrc++;
	}

	/*
		Add an extra argument for the rounding mode (always set to the default).
	*/
	psOutSrc->uType = USEASM_REGTYPE_INTSRCSEL;
	psOutSrc->uNumber = USEASM_INTSRCSEL_ROUNDNEAREST;
	psOutSrc->uIndex = USEREG_INDEX_NONE;
	psOutSrc->uFlags = 0;
	psOutSrc++;

	/*
		Always set the SCALE flag for COMBC10 to indicate we are using the special
		mode of PCKC10C10.
	*/
	if (psIn->eOpcode == ICOMBC10)
	{
		psOut->uFlags2 |= USEASM_OPFLAGS2_SCALE;
	}

	return psOutSrc;
}


static PUSE_REGISTER ConvertCOMBC10InstructionToUseasm(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertCOMBC10InstructionToUseasm

 Description		: Convert a COMBC10 instruction to USEASM.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate COMBC10 instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32		uWriteMask;
	IMG_UINT32		uSrcIdx;
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	psOut->uOpcode = USEASM_OP_UNPCKC10C10; 

	/*
		Copy the register destination.
	*/
	ConvertDest(psState, psIn, psOutSrc);

	/*
		Copy the destination write mask.
	*/
	uWriteMask = psIn->auDestMask[0];
	psOutSrc->uFlags |= (uWriteMask << USEASM_ARGFLAGS_BYTEMSK_SHIFT) | USEASM_ARGFLAGS_BYTEMSK_PRESENT;

	psOutSrc++;

	/*
		Copy the register sources.
	*/
	for (uSrcIdx = 0; uSrcIdx < psIn->uArgumentCount; uSrcIdx++, psOutSrc++)
	{
		CopySource(psState, psIn, uSrcIdx, psOutSrc);
	}

	/*
		Add an extra argument for the rounding mode (always set to the default).
	*/
	psOutSrc->uType = USEASM_REGTYPE_INTSRCSEL;
	psOutSrc->uNumber = USEASM_INTSRCSEL_ROUNDNEAREST;
	psOutSrc->uIndex = USEREG_INDEX_NONE;
	psOutSrc->uFlags = 0;
	psOutSrc++;

	/*
		Always set the SCALE flag for COMBC10 to indicate we are using the special
		mode of PCKC10C10.
	*/
	psOut->uFlags2 |= USEASM_OPFLAGS2_SCALE;

	return psOutSrc;
}

static PUSE_REGISTER ConvertIntegerDOTToUseasm(PINTERMEDIATE_STATE psState, 
											   PINST psIn, 
											   PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertIntegerDOTToUseasm

 Description		: Convert an integer dot-product instruction to USEASM.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate dot-product instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	psOut->uOpcode = GetIntegerDOTOpcode(psState, psIn);

	/*
		Copy the register destination.
	*/
	ConvertDest(psState, psIn, psOutSrc);
	psOutSrc++;

	/*	
		Copy the destination modifiers.
	*/
	psOutSrc = CopyDOT34DestModToUseasm(psIn, psOutSrc);

	/*
		Copy the register sources.
	*/
	psOutSrc = CopySources(psState, psIn, psOutSrc);

	return psOutSrc;
}

static IMG_VOID CopyDPBaseIterationSources(PINTERMEDIATE_STATE	psState, 
										   PINST				psIn, 
										   IMG_UINT32			uArgCountPerIteration,
										   PUSE_REGISTER		psOutSrc)
{
	IMG_UINT32	i;

	for (i = 0; i < uArgCountPerIteration; i++, psOutSrc++)
	{
		CopySource(psState, psIn, i, psOutSrc);

		if (psIn->u.psFdp->abNegate[i])
		{
			psOutSrc->uFlags |= USEASM_ARGFLAGS_NEGATE;
		}
		if (psIn->u.psFdp->abAbsolute[i])
		{
			psOutSrc->uFlags |= USEASM_ARGFLAGS_ABSOLUTE;
		}
	}
}

static IMG_VOID ConvertFDPRptToUseasm(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertFDPRptToUseasm

 Description		: Convert an FDP_RPT/FDPC_RPT instruction to USEASM.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate FDPC instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	if (psIn->eOpcode == IFDPC_RPT)
	{
		psOut->uOpcode = USEASM_OP_FDPC;
	}
	else
	{
		ASSERT(psIn->eOpcode == IFDP_RPT);
		psOut->uOpcode = USEASM_OP_FDP;
	}

	/*
		Copy the register destination.
	*/
	ConvertDest(psState, psIn, psOutSrc);
	psOutSrc++;

	/*
		Copy the P-flag destination.
	*/
	if (psIn->eOpcode == IFDPC_RPT)
	{
		psOutSrc->uType = USEASM_REGTYPE_CLIPPLANE;
		psOutSrc->uNumber = psIn->u.psFdp->uClipPlane;
		psOutSrc->uFlags = 0;
		psOutSrc->uIndex = USEREG_INDEX_NONE;
		psOutSrc++;
	}

	/*
		Copy the register sources.
	*/
	CopyDPBaseIterationSources(psState, psIn, FDP_RPT_PER_ITERATION_ARGUMENT_COUNT, psOutSrc);
}

static IMG_VOID ConvertFDDPToUseasm(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertFDDPToUseasm

 Description		: Convert an FDDP instruction to USEASM.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate FDDP instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	psOut->uOpcode = USEASM_OP_FDDP;

	/*
		Copy the destination for the first dot-product result.
	*/
	ConvertDest(psState, psIn, psOutSrc);
	psOutSrc++;

	ASSERT(psIn->uDestCount == 2);
	ASSERT(psIn->asDest[1].uType == USEASM_REGTYPE_FPINTERNAL);

	/*
		Copy the destination for the second dotproduct result.
	*/
	psOutSrc->uNumber = psIn->asDest[1].uNumber;
	psOutSrc->uType = USEASM_REGTYPE_FPINTERNAL;
	psOutSrc->uIndex = USEREG_INDEX_NONE;
	psOutSrc->uFlags = 0;
	psOutSrc++;

	/*
		Copy the sources.
	*/
	CopyDPBaseIterationSources(psState, psIn, FDDP_PER_ITERATION_ARGUMENT_COUNT, psOutSrc);
}

static PUSE_REGISTER ConvertConditionalMoveToUseasm(PINTERMEDIATE_STATE psState, 
													PINST psIn, 
													PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertConditionalMoveToUseasm

 Description		: Convert a non-vector conditional move instruction to USEASM.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	ASSERT(g_psInstDesc[psIn->eOpcode].uUseasmOpcode != (IMG_UINT32)-1);
	psOut->uOpcode = g_psInstDesc[psIn->eOpcode].uUseasmOpcode; 

	/*
		Copy the destination.
	*/
	ConvertDest(psState, psIn, psOutSrc);
	psOutSrc++;

	/*
		Set the conditional move data format.
	*/
	psOut->uFlags1 |= USEASM_OPFLAGS1_TESTENABLE; 
	switch (psIn->eOpcode)
	{
		case IMOVC: psOut->uFlags2 |= USEASM_OPFLAGS2_MOVC_FMTF32; break;
		case IMOVC_C10: psOut->uFlags2 |= USEASM_OPFLAGS2_MOVC_FMTI10; break;
		case IMOVC_U8: psOut->uFlags1 |= USEASM_OPFLAGS1_MOVC_FMTI8; break;
		case IMOVC_I32: psOut->uFlags2 |= USEASM_OPFLAGS2_MOVC_FMTI32; break;
		case IMOVC_I16: psOut->uFlags2 |= USEASM_OPFLAGS2_MOVC_FMTI16; break;
		default: imgabort();
	}

	/*
		Copy the type of test to perform on source 0.
	*/
	psOut->uTest = ConvertTestTypeToUseasm(psState, psIn->u.psMovc->eTest);

	/*
		Copy the sources.
	*/
	psOutSrc = CopySources(psState, psIn, psOutSrc);

	return psOutSrc;
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static IMG_VOID ConvertVMOVCNonVecToUseasm(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertVMOVCNonVecToUseasm

 Description		: Convert a VMOVC_I32/VMOVCU8_I32 instruction to USEASM.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32		uSrcIdx;
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	if (psIn->eOpcode == IVMOVCU8_I32)
	{
		psOut->uOpcode = USEASM_OP_VMOVCU8;
	}
	else
	{
		ASSERT(psIn->eOpcode == IVMOVC_I32);
		psOut->uOpcode = USEASM_OP_VMOVC;
	}

	/*
		Convert destination.
	*/
	ConvertDest(psState, psIn, psOutSrc);
	psOutSrc++;

	/*
		Copy test type.
	*/
	psOut->uFlags1 |= USEASM_OPFLAGS1_TESTENABLE;
	psOut->uTest = ConvertTestTypeToUseasm(psState, psIn->u.psMovc->eTest);
	psOut->uFlags2 |= USEASM_OPFLAGS2_MOVC_FMTI32;

	/*
		Convert sources.
	*/
	for (uSrcIdx = 0; uSrcIdx < psIn->uArgumentCount; uSrcIdx++, psOutSrc++)
	{
		CopySource(psState, psIn, uSrcIdx, psOutSrc);
	}

	/*
		Add a dummy swizzle for USEASM.
	*/
	CopySwizzle(psOutSrc, USEASM_SWIZZLE(X, Y, Z, W));
	psOutSrc++;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static IMG_VOID ConvertFDP_TESTToUseasm(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertFDP_TESTToUseasm

 Description		: Convert a FDP_RPT_TESTMASK or FDP_RPT_TESTPRED instruction to USEASM.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PFDOTPRODUCT_PARAMS		psFdp = psIn->u.psFdp;
	PUSE_REGISTER			psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	psOut->uOpcode = USEASM_OP_FDP; 
	psOut->uFlags1 |= USEASM_OPFLAGS1_TESTENABLE;
	psOut->uTest = ConvertTestToUseasm(psState, &psFdp->sTest);
	
	if (psIn->eOpcode == IFDP_RPT_TESTPRED)
	{
		/*
			Copy the destination predicate and, if it exists, the unified store destination.
		*/
		psOutSrc = CopyTestDestinations(psState, psIn, psOut, psOutSrc);
	}
	else
	{
		ASSERT(psIn->eOpcode == IFDP_RPT_TESTMASK);

		/*
			Convert TESTMASK destination.
		*/
		ConvertDest(psState, psIn, psOutSrc);
		psOutSrc++;
	}

	/*
		Convert TESTPRED/TESTMASK sources.
	*/
	CopyDPBaseIterationSources(psState, psIn, FDP_RPT_PER_ITERATION_ARGUMENT_COUNT, psOutSrc);
}

static IMG_VOID ConvertTESTToUseasm(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertTESTToUseasm

 Description		: Convert a TESTMASK or TESTPRED instruction to USEASM.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PTEST_PARAMS	psParams = psIn->u.psTest;
	IMG_UINT32		uSrcIdx;
	IOPCODE			eAluOpcode = psParams->eAluOpcode;
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	ASSERT(g_psInstDesc[eAluOpcode].uUseasmOpcode != (IMG_UINT32)-1);
	psOut->uOpcode = g_psInstDesc[eAluOpcode].uUseasmOpcode; 
	psOut->uFlags1 |= USEASM_OPFLAGS1_TESTENABLE;
	psOut->uTest = ConvertTestToUseasm(psState, &psIn->u.psTest->sTest);
		
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psIn->eOpcode == ITESTMASK)
	{
		if (psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_TEST_SUB32)
		{
			switch (g_psInstDesc[eAluOpcode].uUseasmOpcode)
			{
				case USEASM_OP_IADD32:
				case USEASM_OP_IADDU32:
				case USEASM_OP_ISUB32:
				case USEASM_OP_ISUBU32:
				{
					psOut->uFlags2 |= USEASM_OPFLAGS2_MOVC_FMTI32;
					break;
				}

				case USEASM_OP_IADD16:
				case USEASM_OP_IADDU16:
				case USEASM_OP_ISUB16:
				case USEASM_OP_ISUBU16:
				{
					psOut->uFlags2 |= USEASM_OPFLAGS2_MOVC_FMTI16;
					break;
				}
	
				default: 
				{
					break;
				}
			}
		}
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	if (psIn->eOpcode == ITESTPRED)
	{
		/*
			Copy the destination predicate and, if it exists, the unified store destination.
		*/
		psOutSrc = CopyTestDestinations(psState, psIn, psOut, psOutSrc);
	}
	else
	{
		/*
			Convert TESTMASK destination.
		*/
		ConvertDest(psState, psIn, psOutSrc);
		psOutSrc++;
	}

	/*
		Convert TESTPRED/TESTMASK sources.
	*/
	for (uSrcIdx = 0; uSrcIdx < g_psInstDesc[psParams->eAluOpcode].uDefaultArgumentCount; uSrcIdx++, psOutSrc++)
	{
		CopySource(psState, psIn, uSrcIdx, psOutSrc);
	}
}

static IMG_VOID ConvertFMAD16ToUseasm(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertFMAD16ToUseasm

 Description		: Convert an FMAD16 instruction to USEASM.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PFARITH16_PARAMS	psParams = psIn->u.psArith16;
	IMG_UINT32			uArg;
	PUSE_REGISTER		psOutSrc = psOut->asArg;

	/*
		Set the USEASM opcode.
	*/
	psOut->uOpcode = USEASM_OP_FMAD16;

	ConvertDest(psState, psIn, psOutSrc);
	psOutSrc++;

	for (uArg = 0; uArg < FMAD16_SOURCE_COUNT; uArg++, psOutSrc++)
	{
		CopySource(psState, psIn, uArg, psOutSrc);

		/*
			Copy FMAD16 source modifiers.
		*/
		if (psParams->sFloat.asSrcMod[uArg].bNegate)
		{
			psOutSrc->uFlags |= USEASM_ARGFLAGS_NEGATE;
		}
		if (psParams->sFloat.asSrcMod[uArg].bAbsolute)
		{
			psOutSrc->uFlags |= USEASM_ARGFLAGS_ABSOLUTE;
		}

		/*
			Copy the swizzle on FMAD16 source arguments.
		*/
		if (g_psInstDesc[psIn->eOpcode].uFlags & DESC_FLAGS_FARITH16)
		{
			FMAD16_SWIZZLE	eSwizzle;

			ASSERT(uArg < (sizeof(psParams->aeSwizzle) / sizeof(psParams->aeSwizzle[0])));
			eSwizzle = psParams->aeSwizzle[uArg];
	
			ASSERT(eSwizzle <= ((~USEASM_ARGFLAGS_MAD16SWZ_CLRMSK) >> USEASM_ARGFLAGS_MAD16SWZ_SHIFT));
			psOutSrc->uFlags |= (eSwizzle << USEASM_ARGFLAGS_MAD16SWZ_SHIFT);
		}
	}
}

static IMG_VOID ConvertLocalMemoryLocalStoreToUseasm(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertLocalMemoryLocalStoreToUseasm

 Description		: Convert a memory load/store instruction using local
					  addressing to USEASM.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate instruction.
					  psOut		- USEASM instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32		uArg;
	PUSE_REGISTER	psOutSrc = psOut->asArg;
	IMG_BOOL		bBypassCache;

	/*
		Set the USEASM opcode.
	*/
	ASSERT(g_psInstDesc[psIn->eOpcode].uUseasmOpcode != (IMG_UINT32)-1);
	psOut->uOpcode = g_psInstDesc[psIn->eOpcode].uUseasmOpcode; 

	/*
		Bypass the cache?
	*/
	if (g_psInstDesc[psIn->eOpcode].eType == INST_TYPE_LDST)
	{
		bBypassCache = psIn->u.psLdSt->bBypassCache;
	}
	else
	{
		ASSERT(g_psInstDesc[psIn->eOpcode].eType == INST_TYPE_MEMSPILL);
		bBypassCache = psIn->u.psMemSpill->bBypassCache;
	}
	if (bBypassCache)
	{
		psOut->uFlags2 |= USEASM_OPFLAGS2_BYPASSCACHE;
	}

	if (psIn->eOpcode == ILDLB ||
		psIn->eOpcode == ILDLW ||
		psIn->eOpcode == ILDLD ||
		psIn->eOpcode == ISPILLREAD)
	{
		/*
			Convert destination.
		*/
		ConvertDest(psState, psIn, psOutSrc);
		psOutSrc++;
	}

	/*
		Convert sources.
	*/
	for (uArg = 0; uArg < psIn->uArgumentCount; uArg++, psOutSrc++)
	{
		CopySource(psState, psIn, uArg, psOutSrc);

		if (uArg == 1 && psOutSrc->uType == USEASM_REGTYPE_IMMEDIATE)
		{
			IMG_BOOL	bRet;

			/*
				Encode an immediate offset argument.
			*/
			bRet = IsValidImmediateForLocalAddressing(psState, psIn->eOpcode, psOutSrc->uNumber, &psOutSrc->uNumber);
			ASSERT(bRet);
			ASSERT(psOutSrc->uNumber <= EURASIA_USE_MAXIMUM_IMMEDIATE);
		}
	}
}

static IMG_VOID ConvertLDTD(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
/*********************************************************************************
 Function			: ConvertLDTD

 Description		: Convert a memory load instruction to USEASM.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate instruction.
					  psOut		- USEASM instruction.
					  bLoad		- Is the instruction a load from memory?

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSE_REGISTER	psOutSrc = psOut->asArg;

	psOut->uOpcode = USEASM_OP_LDTD; 

	psOut->uFlags1 |= USEASM_OPFLAGS1_FETCHENABLE | (psIn->uDestCount << USEASM_OPFLAGS1_REPEAT_SHIFT);

	ConvertDest(psState, psIn, psOutSrc);
	psOutSrc++;

	/*
		Base source.
	*/
	CopySource(psState, psIn, TILEDMEMLOAD_BASE_ARGINDEX, psOutSrc);
	psOutSrc++;

	/*
		Offset source.
	*/
	CopySource(psState, psIn, TILEDMEMLOAD_OFFSET_ARGINDEX, psOutSrc);
	psOutSrc++;

	/*
		Range checking source (unused).
	*/
	CopyImmediate(psOutSrc, 0 /* uValue */);
	psOutSrc++;

	/*
		Dependent read counter.
	*/
	CopySource(psState, psIn, TILEDMEMLOAD_DRC_ARGINDEX, psOutSrc);
	psOutSrc++;
}

static IMG_VOID ConvertMemoryLoadStoreToUseasm(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut, IMG_BOOL bLoad)
/*********************************************************************************
 Function			: ConvertMemoryLoadToUseasm

 Description		: Convert a memory load instruction to USEASM.
 
 Parameters			: psState	- Compiler state.
					  psIn		- Intermediate instruction.
					  psOut		- USEASM instruction.
					  bLoad		- Is the instruction a load from memory?

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSE_REGISTER	psOutSrc = psOut->asArg;
	IMG_UINT32		uArg;

	ASSERT(g_psInstDesc[psIn->eOpcode].eType == INST_TYPE_LDST);

	/*
		Set the USEASM opcode.
	*/
	ASSERT(g_psInstDesc[psIn->eOpcode].uUseasmOpcode != (IMG_UINT32)-1);
	psOut->uOpcode = g_psInstDesc[psIn->eOpcode].uUseasmOpcode; 

	/*
		Enable range checking?
	*/
	if (psIn->u.psLdSt->bEnableRangeCheck)
	{
		psOut->uFlags1 |= USEASM_OPFLAGS1_RANGEENABLE;
	}

	/*
		Bypass the cache?
	*/
	if (psIn->u.psLdSt->bBypassCache)
	{
		psOut->uFlags2 |= USEASM_OPFLAGS2_BYPASSCACHE;
	}

	/*
		Set up the fetch flag.
	*/
	if (GetBit(psIn->auFlag, INST_FETCH))
	{
		psOut->uFlags1 |= USEASM_OPFLAGS1_FETCHENABLE;
	}

	/*
		Set up the pre-increment flag.
	*/
	if (psIn->u.psLdSt->bPreIncrement)
	{
		psOut->uFlags1 |= USEASM_OPFLAGS1_PREINCREMENT;
	}

	/*
		Set up the negate flag on the offset source.
	*/
	if (!psIn->u.psLdSt->bPositiveOffset)
	{
		psOut->uFlags2 |= USEASM_OPFLAGS2_INCSGN;
	}

	if (bLoad)
	{
		/*
			Convert destination.
		*/
		ConvertDest(psState, psIn, psOutSrc);
		psOutSrc++;
	}

	/*
		Convert sources.
	*/
	for (uArg = 0; uArg < psIn->uArgumentCount; uArg++, psOutSrc++)
	{
		CopySource(psState, psIn, uArg, psOutSrc);

		/*
			The assembler expects an immediate offset argument for load/store instructions to be already
			encoded.
		*/
		if (
				(g_psInstDesc[psIn->eOpcode].uFlags2 & DESC_FLAGS2_ABSOLUTELOADSTORE) != 0 &&
				uArg == 1 && 
				psOutSrc->uType == USEASM_REGTYPE_IMMEDIATE
		   )
		{
			IMG_BOOL	bRet;

			bRet = IsValidImmediateForAbsoluteAddressing(psState, psIn, psOutSrc->uNumber, &psOutSrc->uNumber);
			ASSERT(bRet);
			ASSERT(psOutSrc->uNumber <= EURASIA_USE_MAXIMUM_IMMEDIATE);
		}
	}
}

#if defined(EXECPRED)
/*********************************************************************************
 Function			: ConvertExecPredInstToUseasm

 Description		: Convert a conditional-execition instruction to USEASM
 
 Parameters			: psState	- Internal compiler state
					  psIn		- Instruction to convert.
					  psOut		- Holds the converted instruction.					  

 Globals Effected	: None

 Return				: the number of 32-bit words written
*********************************************************************************/
static
IMG_VOID ConvertExecPredInstToUseasm(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
{

	PUSE_REGISTER	psOutSrc = psOut->asArg;
	IMG_UINT32		uI;

	/*
		Set the USEASM opcode.
	*/
	psOut->uOpcode = g_psInstDesc[psIn->eOpcode].uUseasmOpcode;

	/*
		Copy the register destination.
	*/
	ConvertDest(psState, psIn, psOutSrc);
	psOutSrc++;
	if(psIn->eOpcode == ICNDLT)
	{
		CopyDestRegister(psState, psIn, 1, psOutSrc);
		psOutSrc++;
	}
	/*
		Copy the unified store arguments.
	*/
	for (uI = 0; uI < psIn->uArgumentCount; uI++, psOutSrc++)
	{
		if (
			(psIn->eOpcode != ICNDEND) && 
			(uI == 1)		
		)
		{
			UseAsmInitReg(psOutSrc);
			if (psIn->asArg[1].uType == USEASM_REGTYPE_PREDICATE)
			{
				CopySource(psState, psIn, uI, psOutSrc);
				if(psIn->asArg[2].uNumber == 1)
				{
					psOutSrc->uFlags |= USEASM_ARGFLAGS_NOT;
				}
			}
			else
			{
				psOutSrc->uType = USEASM_REGTYPE_INTSRCSEL;
				if(psIn->asArg[2].uNumber == 1)
				{
					psOutSrc->uNumber = USEASM_INTSRCSEL_TRUE;
				}
				else
				{
					psOutSrc->uNumber = USEASM_INTSRCSEL_FALSE;
				}
			}
			uI++;
		}
		else
		{
			CopySource(psState, psIn, uI, psOutSrc);
		}
	}
}
#endif /* #if defined(EXECPRED) */

/*********************************************************************************
 Function			: ConvertInstructionToUseasm

 Description		: Convert an instruction from the internal format to the USEASM
					  format.
 
 Parameters			: psState	- Internal compiler state
					  psIn		- Instruction to convert.
					  psOut		- Holds the converted instruction.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
static IMG_VOID ConvertInstructionToUseasm(PINTERMEDIATE_STATE psState, PINST psIn, PUSE_INST psOut)
{
	IMG_UINT32 i;

	psOut->uFlags1 = 0;
	psOut->uFlags2 = 0;
	psOut->uFlags3 = 0;

	ASSERT(!(psIn->eOpcode == IDRVPADDING && GetBit(psIn->auFlag, INST_NOSCHED)));

	/*
		Set up the skip-invalid flag
	*/
	if (GetBit(psIn->auFlag, INST_SKIPINV))
	{
		psOut->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	
	/*
		Set up the sync start flag.
	*/
	if (GetBit(psIn->auFlag, INST_SYNCSTART))
	{
		psOut->uFlags1 |= USEASM_OPFLAGS1_SYNCSTART;
	}

	/*
		Set up the nosched flag.
	*/
	if (GetBit(psIn->auFlag, INST_NOSCHED))
	{
		psOut->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}

	if (GetBit(psIn->auFlag, INST_TYPE_PRESERVE))
	{
		psOut->uFlags2 |= USEASM_OPFLAGS2_TYPEPRESERVE;
	}

	if (GetBit(psIn->auFlag, INST_FORMAT_SELECT))
	{
		psOut->uFlags2 |= USEASM_OPFLAGS2_FORMATSELECT;
	}

	if (GetBit(psIn->auFlag, INST_END))
	{
		psOut->uFlags1 |= USEASM_OPFLAGS1_END;
	}

	if (psIn->uRepeat > 1)
	{
		psOut->uFlags1 |= (psIn->uRepeat << USEASM_OPFLAGS1_REPEAT_SHIFT);
	}
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psIn->eOpcode == IVDUAL)
	{
		/* Skip copying the destination mask until later. */
	}
	else if (
				(g_psInstDesc[psIn->eOpcode].uFlags & DESC_FLAGS_VECTORDEST) != 0 && 
				psIn->eOpcode != IVTEST &&
				psIn->eOpcode != IVTESTMASK &&
				psIn->eOpcode != IVTESTBYTEMASK
			)
	{
		psOut->uFlags1 |= (CopyVectorWriteMask(psState, psIn, 0 /* uBaseDestIdx */) << USEASM_OPFLAGS1_MASK_SHIFT);
	}
	else if (g_psInstDesc[psIn->eOpcode].uFlags & DESC_FLAGS_VECTORPACK)
	{
		psOut->uFlags1 |= (CopyVectorPackWriteMask(psState, psIn) << USEASM_OPFLAGS1_MASK_SHIFT);
	}
	else if (psIn->eOpcode == IVMOVCU8_I32 || psIn->eOpcode == IVMOVC_I32)
	{
		psOut->uFlags1 |= (USEREG_MASK_XYZW << USEASM_OPFLAGS1_MASK_SHIFT);
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		psOut->uFlags1 |= (psIn->uMask << USEASM_OPFLAGS1_MASK_SHIFT);
	}
	
	/*
		Convert any source predicate.
	*/
	if (psIn->apsPredSrc != NULL)
	{
		IMG_UINT32	uUseasmPred;

		ASSERT(psIn->apsPredSrc[0]->uType == USEASM_REGTYPE_PREDICATE);

		ASSERT((GetBit(psIn->auFlag, INST_PRED_PERCHAN) == 0 ||
			   ((psIn->apsPredSrc[1] == NULL || psIn->apsPredSrc[1]->uNumber == psIn->apsPredSrc[0]->uNumber + 1) &&
			    (psIn->apsPredSrc[2] == NULL || psIn->apsPredSrc[2]->uNumber == psIn->apsPredSrc[0]->uNumber + 2) &&
			    (psIn->apsPredSrc[3] == NULL || psIn->apsPredSrc[3]->uNumber == psIn->apsPredSrc[0]->uNumber + 3))));

		uUseasmPred = EncodePred(psState, 
								 psIn->apsPredSrc[0]->uNumber, 
								 GetBit(psIn->auFlag, INST_PRED_NEG),
								 GetBit(psIn->auFlag, INST_PRED_PERCHAN));

		psOut->uFlags1 |= (uUseasmPred << USEASM_OPFLAGS1_PRED_SHIFT);
	}
	else
	{
		psOut->uFlags1 |= (USEASM_PRED_NONE << USEASM_OPFLAGS1_PRED_SHIFT);
	}

	if (!GetBit(psIn->auFlag, INST_MOE))
	{
		psOut->uFlags2 |= USEASM_OPFLAGS2_PERINSTMOE;
		for (i = 0; i < (USC_MAX_MOE_OPERANDS - 1); i++)
		{
			static const USEASM_OPFLAGS2 puIncFlag[] = {USEASM_OPFLAGS2_PERINSTMOE_INCS0, 
														USEASM_OPFLAGS2_PERINSTMOE_INCS1, 
														USEASM_OPFLAGS2_PERINSTMOE_INCS2};
			if (GetBit(psIn->puSrcInc, i))
			{
				psOut->uFlags2 |= puIncFlag[i];
			}
		}
	}

	switch (psIn->eOpcode)
	{
		case IAND:
		case IOR:
		case IXOR:
		case ISHL:
		case IASR:
		case ISHR:
		{
			ConvertBitwiseInstructionToUseasm(psState, psIn, psOut);
			break;
		}
		case ISMP:
		case ISMPBIAS:
		case ISMPREPLACE:
		case ISMPGRAD:
		#if defined(OUTPUT_USPBIN)
		case ISMP_USP:
		case ISMPBIAS_USP:
		case ISMPREPLACE_USP:
		case ISMPGRAD_USP:
		case ISMP_USP_NDR:
		#endif /* defined(OUTPUT_USPBIN) */
		{
			ConvertTextureSampleToUseasm(psState, psIn, psOut);
			break;
		}
		case IRLP:
		{
			ConvertRLPToUseasm(psState, psIn, psOut);
			break;
		}
		case ISMLSI:
		{
			ConvertSMLSIToUseasm(psState, psIn, psOut);
			break;
		}
		case IIMA16:
		{
			ConvertIMA16ToUseasm(psState, psIn, psOut);
			break;
		}
		case IEFO:
		{
			ConvertEFOToUseasm(psState, psIn, psOut);
			break;
		}
		case IIMAE:
		{
			ConvertIMAEToUseasm(psState, psIn, psOut);
			break;
		}
		case ISOPWM: 
		{
			ConvertSOPWMToUseasm(psState, psIn, psOut); 
			break;
		}
		case IFPMA:
		{
			ConvertFPMAIMA8ToUseasm(psState, psIn, psOut);
			break;
		}
		case ISOP2:
		{
			ConvertSOP2ToUseasm(psState, psIn, psOut);
			break;
		}
		case ISOP3:
		{
			ConvertSOP3ToUseasm(psState, psIn, psOut);
			break;
		}
		case ILRP1:
		{
			ConvertLRP1ToUseasm(psState, psIn, psOut);
			break;
		}
		case IIDF:
		{
			ConvertIDFToUseasm(psState, psIn, psOut);
			break;
		}
		case IEMIT:
		{
			ConvertEMITToUseasm(psState, psIn, psOut);
			break;
		}
		case IWOP:
		{
			ConvertWOPToUseasm(psState, psIn, psOut);
			break;
		}
		#if defined(SUPPORT_SGX545)
		case IRNE:
		case ITRUNC_HW:
		case IFLOOR:
		{
			ConvertRoundInstToUseasm(psState, psIn, psOut);
			break;
		}
		#endif /* defined(SUPPORT_SGX545) */
		#if defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case IIMA32:
		{
			ConvertIMA32ToUseasm(psState, psIn, psOut);
			break;
		}
		#endif /* defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		#if defined(SUPPORT_SGX545)
		case IDUAL:
		{
			ConvertDualIssueToUseasm(psState, psIn, psOut);
			break;
		}
		#endif /* defined(SUPPORT_SGX545) */
		#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
		case IFNRM:
		case IFNRMF16:
		{
			ConvertNormaliseInstToUseasm(psState, psIn, psOut);
			break;
		}
		#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case IVPCKU8U8:
		case IVPCKFLTU8:
		case IVPCKFLTS8:
		case IVPCKFLTU16:
		case IVPCKFLTS16:
		case IVPCKU16U16:
		case IVPCKS16S8:
		case IVPCKU16U8:
		{
			ConvertSGX543NonVectorPack(psState, psIn, psOut);
			break;
		}
		case IVMAD:
		case IVMAD4:
		case IVMUL:
		case IVMUL3:
		case IVADD:
		case IVADD3:
		case IVFRC:
		case IVDSX_EXP:
		case IVDSY_EXP:
		case IVMIN:
		case IVMAX:
		case IVDP:
		case IVDP_GPI:
		case IVDP3_GPI:
		case IVDP_CP:
		case IVMOV_EXP:
		case IVMOVC:
		case IVMOVCU8_FLT:
		case IVRSQ:
		case IVRCP:
		case IVLOG:
		case IVEXP:
		case IVTESTMASK:
		case IVTESTBYTEMASK:
		case IVPCKU8FLT:
		case IVPCKS8FLT:
		case IVPCKC10FLT:
		case IVPCKFLTFLT:
		case IVPCKS16FLT:
		case IVPCKU16FLT:
		case IVPCKU8FLT_EXP:
		case IVPCKC10FLT_EXP:
		case IVPCKFLTFLT_EXP:
		case IVPCKS16FLT_EXP:
		case IVPCKU16FLT_EXP:
		{
			ConvertVectorSourceInst(psState, psIn, psOut);
			break;
		}
		case IVTEST:
		{
			ConvertVTESTInst(psState, psIn, psOut);
			break;
		}
		case IVDUAL:
		{
			ConvertVectorDualIssueInst(psState, psIn, psOut);
			break;
		}
		case IVPCKC10C10:
		case IVPCKFLTC10_VEC:
		case ISOPWM_VEC:
		case ISOP2_VEC:
		case ISOP3_VEC:
		case ILRP1_VEC:
		case IFPMA_VEC:
		case IFPADD8_VEC:
		case IFPSUB8_VEC:
		case IVMOVC10:
		case IVMOVCC10:
		case IVMOVCU8:
		case IFPDOT_VEC:
		case IFPTESTPRED_VEC:
		case IFPTESTMASK_VEC:
		{
			ConvertC10VectorSourceInst(psState, psIn, psOut);
			break;
		}
		case IIDXSCR:
		case IIDXSCW:
		{
			ConvertIDXSCInst(psState, psIn, psOut);
			break;
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

		case IUNPCKS16U8:
		case IUNPCKS16S8:
		case IUNPCKU16U8:
		case IUNPCKU16S8:
		case IUNPCKU16U16:
		case IUNPCKU16S16:
		case IUNPCKU16F16:
		case IPCKU16F32:
		case IUNPCKS16U16:
		case IPCKS16F32:
		case IPCKU8U8:
		case IPCKU8S8:
		case IPCKU8U16:
		case IPCKU8S16:
		case IPCKS8U8:
		case IPCKS8U16:
		case IPCKS8S16:
		case IPCKU8F16:
		case IPCKU8F32:
		case IPCKS8F32:
		case IPCKC10C10:
		case IPCKC10F32:
		case IUNPCKF16C10:
		case IUNPCKF16O8:
		case IUNPCKF16U8:
		case IUNPCKF16S8:
		case IUNPCKF16U16:
		case IUNPCKF16S16:
		case IPCKF16F16:
		case IPCKF16F32:
		case IUNPCKF32C10:
		case IUNPCKF32O8:
		case IUNPCKF32U8:
		case IUNPCKF32S8:
		case IUNPCKF32U16:
		case IUNPCKF32S16:
		case IUNPCKF32F16:
		case IPCKC10F16:
		{
			ConvertPCKInstructionToUseasm(psState, psIn, psOut);
			break;
		}
		case ICOMBC10:
		{
			ConvertCOMBC10InstructionToUseasm(psState, psIn, psOut);
			break;
		}
		case IFPDOT:
		case IFP16DOT:
		{
			ConvertIntegerDOTToUseasm(psState, psIn, psOut);
			break;
		}
		case IFDP_RPT:
		case IFDPC_RPT:
		{
			ConvertFDPRptToUseasm(psState, psIn, psOut);
			break;
		}
		case IFDDP_RPT:
		{
			ConvertFDDPToUseasm(psState, psIn, psOut);
			break;
		}
		case IMOVC:
		case IMOVC_C10:
		case IMOVC_U8:
		case IMOVC_I32:
		case IMOVC_I16:
		{
			ConvertConditionalMoveToUseasm(psState, psIn, psOut);
			break;
		}
		case ILDAB:
		case ILDAW:
		case ILDAD:
		#if defined(SUPPORT_SGX545)
		case IELDD:
		case IELDQ:
		#endif /* defined(SUPPORT_SGX545) */
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case ILDAQ:
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			ConvertMemoryLoadStoreToUseasm(psState, psIn, psOut, IMG_TRUE /* bLoad */);
			break;
		}
		case ISTAB:
		case ISTAW:
		case ISTAD:
		{
			ConvertMemoryLoadStoreToUseasm(psState, psIn, psOut, IMG_FALSE /* bLoad */);
			break;
		}
		case ILDLB:
		case ISTLB:
		case ILDLW:
		case ISTLW:
		case ILDLD:
		case ISTLD:
		case ISPILLREAD:
		case ISPILLWRITE:
		{
			ConvertLocalMemoryLocalStoreToUseasm(psState, psIn, psOut);
			break;
		}
		case IFMAD16:
		{
			ConvertFMAD16ToUseasm(psState, psIn, psOut);
			break;
		}
		case ITESTMASK:
		case ITESTPRED:
		{
			ConvertTESTToUseasm(psState, psIn, psOut);
			break;
		}
		case IFDP_RPT_TESTMASK:
		case IFDP_RPT_TESTPRED:
		{
			ConvertFDP_TESTToUseasm(psState, psIn, psOut);
			break;
		}
		case ILDTD:
		{
			ConvertLDTD(psState, psIn, psOut);
			break;
		}
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case IVMOVCU8_I32:
		case IVMOVC_I32:
		{
			ConvertVMOVCNonVecToUseasm(psState, psIn, psOut);
			break;
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		#if defined(EXECPRED)
		case ICNDST:
		case ICNDEF:
		case ICNDSM:
		case ICNDLT:
		case ICNDEND:
		case ICNDSTLOOP:
		case ICNDEFLOOP:
		{
			ConvertExecPredInstToUseasm(psState, psIn, psOut);
			break;
		}
		#endif	/* #if defined(EXECPRED) */
		case IIMA8:
		{
			ConvertFPMAIMA8ToUseasm(psState, psIn, psOut);
			break;
		}
		case IFRCP:
		case IFRSQ:
		case IFLOG:
		case IFEXP:
		#if defined(SUPPORT_SGX545)
		case IFSQRT:
		case IFSIN:
		case IFCOS:
		#endif /* defined(SUPPORT_SGX545) */
		{
			PUSE_REGISTER	psOutSrc = psOut->asArg;

			ASSERT(g_psInstDesc[psIn->eOpcode].uUseasmOpcode != (IMG_UINT32)-1);
			psOut->uOpcode = g_psInstDesc[psIn->eOpcode].uUseasmOpcode; 
			
			ConvertDest(psState, psIn, psOutSrc);
			psOutSrc++;

			CopySourcesWithFloatSourceModifier(psState, 
											   psIn, 
											   psIn->u.psFloat, 
											   psIn->uArgumentCount,
											   psOutSrc,
											   IMG_TRUE /* bCopyComponent */);

			break;
		}
		default:
		{
			PUSE_REGISTER	psOutSrc = psOut->asArg;

			ASSERT(g_psInstDesc[psIn->eOpcode].uUseasmOpcode != (IMG_UINT32)-1);
			psOut->uOpcode = g_psInstDesc[psIn->eOpcode].uUseasmOpcode; 

			if (g_psInstDesc[psIn->eOpcode].bHasDest)
			{
				ConvertDest(psState, psIn, psOutSrc);
				psOutSrc++;
			}

			if (g_psInstDesc[psIn->eOpcode].eType == INST_TYPE_FLOAT)
			{
				CopySourcesWithFloatSourceModifier(psState, 
												   psIn, 
												   psIn->u.psFloat, 
												   psIn->uArgumentCount,
												   psOutSrc,
												   IMG_FALSE /* bCopyComponent */);
			}
			else
			{
				CopySources(psState, psIn, psOutSrc);
			}
			break;
		}
	}
}

/*********************************************************************************
 Function			: EncodeInst

 Description		: Generate a HW instruction for a single intermediate one
 
 Parameters			: psUseasmContext		- Useasm context
					  eCompileTarget		- ID of the target device
					  puInst				- Where to generate the instruction
					  puBaseInst			- Base instruction for calculating inst or
					  psInst				- The intermediate instruction

 Globals Effected	: None

 Return				: the number of 32-bit words written
*********************************************************************************/
IMG_INTERNAL
IMG_UINT32 EncodeInst(PINTERMEDIATE_STATE	psState,
					  PINST					psInst,
					  IMG_PUINT32			puInst,
					  IMG_PUINT32			puBaseInst)
{
	USE_INST	asUseasmInst[2];
	IMG_UINT32	uOutSize;

	/*
		Generate a useasm instruction corresponding to the intermediate one...

		NB: We assume all internal instructions require a single Useasm
			equivalent
	*/
	asUseasmInst[0].psNext = &asUseasmInst[1];

	ConvertInstructionToUseasm(psState, psInst, asUseasmInst);

	/*
		... and assemble it.
	*/
	uOutSize = UseAssembleInstruction(psState->psTargetDesc,
									  asUseasmInst,
									  puBaseInst,
									  puInst,
									  0,
									  psState->psUseasmContext);

	#ifdef SRC_DEBUG
	if( ((psInst->uAssociatedSrcLine) < (psState->uTotalLines)) && 
		((psInst->uAssociatedSrcLine) != UNDEFINED_SOURCE_LINE) )
	{
		DecrementCostCounter(psState, psInst->uAssociatedSrcLine, 1);
		IncrementCostCounter(psState, psInst->uAssociatedSrcLine, (uOutSize/2));
	}
	#endif /* SRC_DEBUG */

	return uOutSize;
}

/*********************************************************************************
 Function			: EncodeNop

 Description		: Encode a NOP instruction
 
 Parameters			: puCode	- Location to encode the NOP.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
IMG_INTERNAL IMG_VOID EncodeNop(IMG_PUINT32 puCode)
{
	puCode[1] = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
				(EURASIA_USE1_SPECIAL_OPCAT_FLOWCTRL << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				(EURASIA_USE1_FLOWCTRL_OP2_NOP << EURASIA_USE1_FLOWCTRL_OP2_SHIFT);
	puCode[0] = 0;
}

/*****************************************************************************
 FUNCTION	: EncodeJump
    
 PURPOSE	: Encode a flow control instruction.

 PARAMETERS	: psState				- Internal compiler state
			  psUseasmContext		- Useasm context
			  eOpcode				- Intermediate opcode of the required
									  branch instruction
			  uLabel				- Destination label number
			  uPredicate			- Predicate controlling the branch
			  bPredNegate			- Whether the sense of the predicate
									  should be inverted
			  puInst				- Where to asssemble the instruction
			  puBaseInst			- Base instruction for calculating inst or
									  label offsets
			  bAssumeAlwaysSkipped	- Flag to indicate the branches should
									  always be skipped during verification
			  bSyncEnd				- Indicates whether threads need to wait
									  on this branch for synchronisation.
			  bSyncIfNotTaken		- Indicates uf threads that do not take
									  a branch must wait, as opposed to those
									  that do. NB: Not supported on all cores.
			  eExecPredBranchType	- Execution Predicate Branch Type.
			  
 RETURNS	: the number of 32-bit words written
*****************************************************************************/
IMG_INTERNAL
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
					  EXECPRED_BRANCHTYPE	eExecPredBranchType)
{
	USE_INST	sInst;
	IMG_UINT32	uUseasmPredicate;
	IMG_UINT32	uOutSize;

	#ifndef UF_TESTBENCH
	PVR_UNREFERENCED_PARAMETER(bAssumeAlwaysSkipped);
	#endif /* UF_TESTBENCH */

	#if !defined (EXECPRED)
	PVR_UNREFERENCED_PARAMETER(eExecPredBranchType);
	#endif /* #if !defined (EXECPRED) */

	uUseasmPredicate = EncodePred(psState, uPredicate, bPredNegate, IMG_FALSE);

	sInst.uFlags1 = (uUseasmPredicate << USEASM_OPFLAGS1_PRED_SHIFT);
	sInst.uFlags2 = 0;
	sInst.uFlags3 = 0;
	sInst.uTest = 0;
	sInst.psPrev = sInst.psNext = NULL;

	if (bSyncEnd)
	{
		if	(bSyncIfNotTaken)
		{
			ASSERT(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_BRANCH_EXTSYNCEND);
			sInst.uFlags3 |= USEASM_OPFLAGS3_SYNCENDNOTTAKEN;
		}
		else
		{
			sInst.uFlags1 |= USEASM_OPFLAGS1_SYNCEND;
		}
	}

	switch (eOpcode)
	{
		case IBR:
		{
			sInst.uOpcode = USEASM_OP_BR;
			#if defined(EXECPRED)
			if ((psState->uCompilerFlags2 & UF_FLAGS2_EXECPRED) && (psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_INTEGER_CONDITIONALS) &&  !(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_29643))
			{
				if(eExecPredBranchType == EXECPRED_BRANCHTYPE_ALLINSTSFALSE)
				{
					sInst.uFlags3 |= USEASM_OPFLAGS3_ALLINSTANCES;
				}
				else if(eExecPredBranchType == EXECPRED_BRANCHTYPE_ANYINSTTRUE)
				{
					sInst.uFlags3 |= USEASM_OPFLAGS3_ANYINSTANCES;
				}
			}
			#endif /* defined(EXECPRED) */
			break;
		}
		case ICALL:
		{
			sInst.uOpcode = USEASM_OP_BR;
			sInst.uFlags1 |= USEASM_OPFLAGS1_SAVELINK;
			#if defined(EXECPRED)
			if ((psState->uCompilerFlags2 & UF_FLAGS2_EXECPRED) && (psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_INTEGER_CONDITIONALS) && !(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_29643))
			{
				sInst.uFlags3 |= USEASM_OPFLAGS3_ANYINSTANCES;
			}
			#endif /* #if defined(EXECPRED) */
			break;
		}
		case ILAPC:
		{
			sInst.uOpcode = USEASM_OP_LAPC;
			break;
		}
		case ILABEL:
		{
			sInst.uOpcode = USEASM_OP_LABEL;
			break;
		}
		default: imgabort();
	}

	if (eOpcode != ILAPC)
	{
		sInst.asArg[0].uType = USEASM_REGTYPE_LABEL;
		sInst.asArg[0].uNumber = uLabel;
	}

	uOutSize = UseAssembleInstruction(psState->psTargetDesc,
									  &sInst,
									  puBaseInst,
									  puInst,
									  0,
									  psUseasmContext);

	#ifdef UF_TESTBENCH
	if (bAssumeAlwaysSkipped)
	{
		puInst[1] |= (EURASIA_USE1_BRANCH_SAVELINK << 1);
	}
	#endif /* UF_TESTBENCH */

	return uOutSize;
}

/*****************************************************************************
 FUNCTION	: EncodeLabel
    
 PURPOSE	: Encode a label instruction.

 PARAMETERS	: psUseasmContext	- Useasm context
			  psTarget			- ID and rev of the target device
			  uLabel			- Label number.
			  puInst			- Where to asssemble the instruction
			  puBaseInst		- Base instruction for calculating inst or
								  label offsets
			  
 RETURNS	: Nothing.
*****************************************************************************/
IMG_INTERNAL
IMG_UINT32 EncodeLabel(PUSEASM_CONTEXT	psUseasmContext,
					   PCSGX_CORE_DESC	psTarget,
					   IMG_UINT32		uLabel,
					   IMG_PUINT32		puInst,
					   IMG_PUINT32		puBaseInst)
{
	USE_INST sInst;

	sInst.uFlags1 = 0;
	sInst.uFlags2 = 0;
	sInst.uFlags3 = 0;
	sInst.uOpcode = USEASM_OP_LABEL;
	sInst.uTest = 0;
	sInst.psPrev = sInst.psNext = NULL;
	sInst.asArg[0].uType = USEASM_REGTYPE_LABEL;
	sInst.asArg[0].uNumber = uLabel;

	return UseAssembleInstruction(psTarget,
								  &sInst,
								  puBaseInst,
								  puInst,
								  0,
								  psUseasmContext);
}

