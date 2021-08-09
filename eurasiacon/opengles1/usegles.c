/******************************************************************************
 * Name         : usegles.c
 *
 * Copyright    : 2006-2010 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited,
 *              : Home Park Estate, Kings Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * $Log: usegles.c $
 *****************************************************************************/
#include "context.h"
#include "pixevent.h"


/* Total of GLES1_MAX_MTE_STATE_DWORDS versions of the state copy code. 

	   First 16 take 2              instructions (non SGX545)
	                 3				instructions (SGX543)
                     4              instructions (SGX545)

	   The rest take 3 (round to 4) instructions (non SGX545) 
                     4              instructions (SGX543)
					 5				instructions (SGX545)
	 */
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
#if defined(SGX_FEATURE_VCB)
#if defined(FIX_HW_BRN_31988)
#define GLES1_STATECOPY_FIRST16_SIZE_IN_BYTES    (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 9), EURASIA_USE_INSTRUCTION_CACHE_LINE_SIZE))
#define GLES1_STATECOPY_REMAIN_SIZE_IN_BYTES     (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 10), EURASIA_USE_INSTRUCTION_CACHE_LINE_SIZE))
#else
#define GLES1_STATECOPY_FIRST16_SIZE_IN_BYTES    (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 5), EURASIA_PDS_DOUTU_PHASE_START_ALIGN))  /* + PHAS + WOP + EMITVCB */
#define GLES1_STATECOPY_REMAIN_SIZE_IN_BYTES     (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 6), EURASIA_PDS_DOUTU_PHASE_START_ALIGN))  /* + PHAS + WOP + EMITVCB */
#endif
#else
#if defined(FIX_HW_BRN_31988)
/* + PHAS + NOP + TST + LDAD + WDF */
#define GLES1_STATECOPY_FIRST16_SIZE_IN_BYTES    (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 7), EURASIA_USE_INSTRUCTION_CACHE_LINE_SIZE))  
#define GLES1_STATECOPY_REMAIN_SIZE_IN_BYTES     (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 8), EURASIA_USE_INSTRUCTION_CACHE_LINE_SIZE))
#else
#define GLES1_STATECOPY_FIRST16_SIZE_IN_BYTES    (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 3), EURASIA_PDS_DOUTU_PHASE_START_ALIGN))  /* + PHAS */
#define GLES1_STATECOPY_REMAIN_SIZE_IN_BYTES     (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 4), EURASIA_PDS_DOUTU_PHASE_START_ALIGN))  /* + PHAS */
#endif
#endif /* defined(SGX_FEATURE_VCB) */
#else
#define GLES1_STATECOPY_FIRST16_SIZE_IN_BYTES    (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 2), EURASIA_PDS_DOUTU_PHASE_START_ALIGN))
#define GLES1_STATECOPY_REMAIN_SIZE_IN_BYTES     (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 4), EURASIA_PDS_DOUTU_PHASE_START_ALIGN))
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */


static const IMG_UINT32 aui32StreamTypeToUseType[GLES1_STREAMTYPE_MAX] =
{
	EURASIA_USE1_PCK_FMT_S8,	/* GLES1_STREAMTYPE_BYTE */
	EURASIA_USE1_PCK_FMT_U8,	/* GLES1_STREAMTYPE_UBYTE */
	EURASIA_USE1_PCK_FMT_S16,	/* GLES1_STREAMTYPE_SHORT */
	EURASIA_USE1_PCK_FMT_U16,	/* GLES1_STREAMTYPE_USHORT */
	EURASIA_USE1_PCK_FMT_F32,	/* GLES1_STREAMTYPE_FLOAT */
	EURASIA_USE1_PCK_FMT_F16,	/* GLES1_STREAMTYPE_HALFFLOAT */
	EURASIA_USE1_PCK_FMT_U8,	/* GLES1_STREAMTYPE_FIXED */
};

/* Size of attribs in bytes, by type */
static const IMG_UINT32 aui32AttribSize[GLES1_STREAMTYPE_MAX] =
{
	1,		/* GLES1_STREAMTYPE_BYTE */
	1,		/* GLES1_STREAMTYPE_UBYTE */
	2,		/* GLES1_STREAMTYPE_SHORT */
	2,		/* GLES1_STREAMTYPE_USHORT */
	4,		/* GLES1_STREAMTYPE_FLOAT */
	2,		/* GLES1_STREAMTYPE_HALFFLOAT */
	4		/* GLES1_STREAMTYPE_FIXED */
};



/*****************************************************************************/

/*
 * Arrays that swap SRC and DST operands for 
 * the reverse-subtract blend, since the HW can only 
 * do an ordinary subtract
*/
static const IMG_UINT32 aui32RevSubtractOperandSwap[] =
{
	GLES1_BLENDFACTOR_ZERO,                 /* GLES1_BLENDFACTOR_ZERO */
	GLES1_BLENDFACTOR_ONE,	                /* GLES1_BLENDFACTOR_ONE */
	GLES1_BLENDFACTOR_DSTCOLOR,             /* GLES1_BLENDFACTOR_SRCCOLOR */
	GLES1_BLENDFACTOR_ONEMINUS_DSTCOLOR,    /* GLES1_BLENDFACTOR_ONEMINUS_SRCCOLOR */
	GLES1_BLENDFACTOR_DSTALPHA,             /* GLES1_BLENDFACTOR_SRCALPHA */
	GLES1_BLENDFACTOR_ONEMINUS_DSTALPHA,    /* GLES1_BLENDFACTOR_ONEMINUS_SRCALPHA */
	GLES1_BLENDFACTOR_SRCALPHA,             /* GLES1_BLENDFACTOR_DSTALPHA */
	GLES1_BLENDFACTOR_ONEMINUS_SRCALPHA,    /* GLES1_BLENDFACTOR_ONEMINUS_DSTALPHA */
	GLES1_BLENDFACTOR_SRCCOLOR,             /* GLES1_BLENDFACTOR_DSTCOLOR */
	GLES1_BLENDFACTOR_ONEMINUS_SRCCOLOR,    /* GLES1_BLENDFACTOR_ONEMINUS_DSTCOLOR */

	GLES1_BLENDFACTOR_DSTALPHA_SATURATE     /* GLES1_BLENDFACTOR_SRCALPHA_SATURATE */
};

typedef struct USEASMBlendFactor_TAG
{
	USEASM_INTSRCSEL eSrcSelect;

	USEASM_ARGFLAGS	 eFlags;
	
} USEASMBlendFactor;


/* Map Blend factor defines to USEASM source selects and flags */
static const USEASMBlendFactor asUSEASMColSrc[] =
{
	{USEASM_INTSRCSEL_ZERO,			(USEASM_ARGFLAGS)(0)},		/* GLES1_BLENDFACTOR_ZERO */
	{USEASM_INTSRCSEL_ZERO,			USEASM_ARGFLAGS_COMPLEMENT},/* GLES1_BLENDFACTOR_ONE */
	{USEASM_INTSRCSEL_SRC1,			(USEASM_ARGFLAGS)(0)},		/* GLES1_BLENDFACTOR_SRCCOLOR */
	{USEASM_INTSRCSEL_SRC1,			USEASM_ARGFLAGS_COMPLEMENT},/* GLES1_BLENDFACTOR_ONEMINUS_SRCCOLOR */
	{USEASM_INTSRCSEL_SRC1ALPHA, 	(USEASM_ARGFLAGS)(0)},		/* GLES1_BLENDFACTOR_SRCALPHA */
	{USEASM_INTSRCSEL_SRC1ALPHA,	USEASM_ARGFLAGS_COMPLEMENT},/* GLES1_BLENDFACTOR_ONEMINUS_SRCALPHA */
	{USEASM_INTSRCSEL_SRC2ALPHA, 	(USEASM_ARGFLAGS)(0)},		/* GLES1_BLENDFACTOR_DSTALPHA */
	{USEASM_INTSRCSEL_SRC2ALPHA,	USEASM_ARGFLAGS_COMPLEMENT},/* GLES1_BLENDFACTOR_ONEMINUS_DSTALPHA */
	{USEASM_INTSRCSEL_SRC2, 		(USEASM_ARGFLAGS)(0)},		/* GLES1_BLENDFACTOR_DSTCOLOR */
	{USEASM_INTSRCSEL_SRC2,			USEASM_ARGFLAGS_COMPLEMENT},/* GLES1_BLENDFACTOR_ONEMINUS_DSTCOLOR */
	{USEASM_INTSRCSEL_SRCALPHASAT,	(USEASM_ARGFLAGS)(0)},		/* GLES1_BLENDFACTOR_SRCALPHA_SATURATE */

	{USEASM_INTSRCSEL_SRCALPHASAT, 	USEASM_ARGFLAGS_COMPLEMENT}	/* GLES1_BLENDFACTOR_DSTALPHA_SATURATE */
};


/* Map Blend factor defines to USEASM source selects and flags */
static const USEASMBlendFactor asUSEASMAlphaSrc[] =
{
	{USEASM_INTSRCSEL_ZERO,			(USEASM_ARGFLAGS)(0)},		/* GLES1_BLENDFACTOR_ZERO */
	{USEASM_INTSRCSEL_ZERO,			USEASM_ARGFLAGS_COMPLEMENT},/* GLES1_BLENDFACTOR_ONE */
	{USEASM_INTSRCSEL_SRC1ALPHA,	(USEASM_ARGFLAGS)(0)},		/* GLES1_BLENDFACTOR_SRCCOLOR */
	{USEASM_INTSRCSEL_SRC1ALPHA,	USEASM_ARGFLAGS_COMPLEMENT},/* GLES1_BLENDFACTOR_ONEMINUS_SRCCOLOR */
	{USEASM_INTSRCSEL_SRC1ALPHA, 	(USEASM_ARGFLAGS)(0)},		/* GLES1_BLENDFACTOR_SRCALPHA */
	{USEASM_INTSRCSEL_SRC1ALPHA,	USEASM_ARGFLAGS_COMPLEMENT},/* GLES1_BLENDFACTOR_ONEMINUS_SRCALPHA */
	{USEASM_INTSRCSEL_SRC2ALPHA, 	(USEASM_ARGFLAGS)(0)},		/* GLES1_BLENDFACTOR_DSTALPHA */
	{USEASM_INTSRCSEL_SRC2ALPHA,	USEASM_ARGFLAGS_COMPLEMENT},/* GLES1_BLENDFACTOR_ONEMINUS_DSTALPHA */
	{USEASM_INTSRCSEL_SRC2ALPHA, 	(USEASM_ARGFLAGS)(0)},		/* GLES1_BLENDFACTOR_DSTCOLOR */
	{USEASM_INTSRCSEL_SRC2ALPHA,	USEASM_ARGFLAGS_COMPLEMENT},/* GLES1_BLENDFACTOR_ONEMINUS_DSTCOLOR */
	{USEASM_INTSRCSEL_ZERO,			USEASM_ARGFLAGS_COMPLEMENT},/* GLES1_BLENDFACTOR_SRCALPHA_SATURATE */

	{USEASM_INTSRCSEL_ZERO, 		USEASM_ARGFLAGS_COMPLEMENT}	/* GLES1_BLENDFACTOR_DSTALPHA_SATURATE */
};

/*****************************************************************************/


/*****************************************************************************
 Function Name	: ReplaceDestinationAlpha
 Inputs			: Blend source
 Outputs		: Modified blend source
 Description	: Helper function to replace uses of DSTA and INVDSTA
				  when the framebuffer has no alpha bits
*****************************************************************************/
static IMG_VOID ReplaceDestinationAlpha(IMG_UINT32 *pui32Src)
{
	if(*pui32Src == GLES1_BLENDFACTOR_DSTALPHA)
	{
		*pui32Src = GLES1_BLENDFACTOR_ONE;
	}

	if(*pui32Src == GLES1_BLENDFACTOR_ONEMINUS_DSTALPHA)
	{
		*pui32Src = GLES1_BLENDFACTOR_ZERO;
	}
}


/*****************************************************************************
 Function Name	: GetFBBlendType
 Inputs			: gc
 Outputs		: Is blend a noop or translucent
 Description	: -
*****************************************************************************/
IMG_INTERNAL IMG_VOID GetFBBlendType(GLES1Context *gc, 
									 IMG_BOOL *pbIsBlendNone, 
									 IMG_BOOL *pbIsBlendTranslucent)
{
	IMG_UINT32 ui32ColSrc, ui32ColDst, ui32AlphaSrc, ui32AlphaDst;

	ui32ColSrc = (gc->sState.sRaster.ui32BlendFunction & GLES1_BLENDFACTOR_RGBSRC_MASK) >> GLES1_BLENDFACTOR_RGBSRC_SHIFT;
	ui32ColDst = (gc->sState.sRaster.ui32BlendFunction & GLES1_BLENDFACTOR_RGBDST_MASK) >> GLES1_BLENDFACTOR_RGBDST_SHIFT;
	ui32AlphaSrc = (gc->sState.sRaster.ui32BlendFunction & GLES1_BLENDFACTOR_ALPHASRC_MASK) >> GLES1_BLENDFACTOR_ALPHASRC_SHIFT;
	ui32AlphaDst = (gc->sState.sRaster.ui32BlendFunction & GLES1_BLENDFACTOR_ALPHADST_MASK) >> GLES1_BLENDFACTOR_ALPHADST_SHIFT;

	if(!gc->psMode->ui32AlphaBits)
	{
		/* Framebuffer does not have alpha, so DSTA should be treated as 1 */
		ReplaceDestinationAlpha(&ui32ColSrc);
		ReplaceDestinationAlpha(&ui32ColDst);
		ReplaceDestinationAlpha(&ui32AlphaSrc);
		ReplaceDestinationAlpha(&ui32AlphaDst);
	}

	*pbIsBlendNone = IMG_FALSE;

	/* Looking for [Zero*src + One*dst] or [One*dst - Zero*src] */
	if(((gc->sState.sRaster.ui32BlendEquation & GLES1_BLENDMODE_RGB_MASK)   != (GLES1_BLENDMODE_SUBTRACT << GLES1_BLENDMODE_RGB_SHIFT)) &&
	   ((gc->sState.sRaster.ui32BlendEquation & GLES1_BLENDMODE_ALPHA_MASK) != (GLES1_BLENDMODE_SUBTRACT << GLES1_BLENDMODE_ALPHA_SHIFT)))
	{
		if((ui32ColSrc == GLES1_BLENDFACTOR_ZERO) && (ui32AlphaSrc == GLES1_BLENDFACTOR_ZERO) && 
			(ui32ColDst == GLES1_BLENDFACTOR_ONE) && (ui32AlphaDst == GLES1_BLENDFACTOR_ONE))
		{
			*pbIsBlendNone = IMG_TRUE;
		}
	}


	/* An object is translucent if the destination operands are not ZERO, OR one
	   of the source operands is a destination colour/alpha */ 
	*pbIsBlendTranslucent = ((ui32ColDst != GLES1_BLENDFACTOR_ZERO) || (ui32AlphaDst != GLES1_BLENDFACTOR_ZERO) ||
							((ui32ColSrc >= GLES1_BLENDFACTOR_DSTALPHA) &&
							(ui32ColSrc <= GLES1_BLENDFACTOR_SRCALPHA_SATURATE)) ||
							((ui32AlphaSrc >= GLES1_BLENDFACTOR_DSTALPHA) &&
							(ui32AlphaSrc <= GLES1_BLENDFACTOR_SRCALPHA_SATURATE))) ? IMG_TRUE : IMG_FALSE;

}


/***********************************************************************************
 Function Name      : CreateFBBlendUSEASMCode
 Inputs             : gc, psUSEASMInfo
 Outputs            : psUSEASMInfo
 Returns            : -
 Description        : Adds USEASM instructions for framebuffer blend
************************************************************************************/
static IMG_VOID CreateFBBlendUSEASMCode(GLES1Context *gc, GLESUSEASMInfo *psUSEASMInfo)
{
	IMG_UINT32 ui32ColSrc, ui32ColDst, ui32AlphaSrc, ui32AlphaDst, ui32ColEquation, ui32AlphaEquation;
	USE_REGISTER asArg[7];

	ui32ColSrc = (gc->sState.sRaster.ui32BlendFunction & GLES1_BLENDFACTOR_RGBSRC_MASK) >> GLES1_BLENDFACTOR_RGBSRC_SHIFT;
	ui32ColDst = (gc->sState.sRaster.ui32BlendFunction & GLES1_BLENDFACTOR_RGBDST_MASK) >> GLES1_BLENDFACTOR_RGBDST_SHIFT;
	ui32AlphaSrc = (gc->sState.sRaster.ui32BlendFunction & GLES1_BLENDFACTOR_ALPHASRC_MASK) >> GLES1_BLENDFACTOR_ALPHASRC_SHIFT;
	ui32AlphaDst = (gc->sState.sRaster.ui32BlendFunction & GLES1_BLENDFACTOR_ALPHADST_MASK) >> GLES1_BLENDFACTOR_ALPHADST_SHIFT;

	ui32ColEquation = (gc->sState.sRaster.ui32BlendEquation & GLES1_BLENDMODE_RGB_MASK) >> GLES1_BLENDMODE_RGB_SHIFT;
	ui32AlphaEquation = (gc->sState.sRaster.ui32BlendEquation & GLES1_BLENDMODE_ALPHA_MASK) >> GLES1_BLENDMODE_ALPHA_SHIFT;

	if(!gc->psMode->ui32AlphaBits)
	{
		/* Framebuffer does not have alpha, so DSTA should be treated as 1 */
		ReplaceDestinationAlpha(&ui32ColSrc);
		ReplaceDestinationAlpha(&ui32ColDst);
		ReplaceDestinationAlpha(&ui32AlphaSrc);
		ReplaceDestinationAlpha(&ui32AlphaDst);
	}


	/* Dst */
	SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_TEMP, 0);

	if(ui32ColEquation == GLES1_BLENDMODE_REVSUBTRACT)
	{
		IMG_UINT32 ui32Temp;

		ui32Temp = ui32ColSrc;

		ui32ColSrc = aui32RevSubtractOperandSwap[ui32ColDst];

		ui32ColDst = aui32RevSubtractOperandSwap[ui32Temp];

		/* Src 1 */
		SETUP_INSTRUCTION_ARG(1, 0, USEASM_REGTYPE_OUTPUT, 0);

		/* Src 2 */
		SETUP_INSTRUCTION_ARG(2, 0, USEASM_REGTYPE_TEMP, 0);

		/* Src1 modifier */
		SETUP_INSTRUCTION_ARG(3, USEASM_INTSRCSEL_NONE, USEASM_REGTYPE_INTSRCSEL, 0);

		/* Src1 colour factor */
		SETUP_INSTRUCTION_ARG(4, asUSEASMColSrc[ui32ColSrc].eSrcSelect, USEASM_REGTYPE_INTSRCSEL, asUSEASMColSrc[ui32ColSrc].eFlags);

		/* Src2 colour factor */
		SETUP_INSTRUCTION_ARG(5, asUSEASMColSrc[ui32ColDst].eSrcSelect, USEASM_REGTYPE_INTSRCSEL, asUSEASMColSrc[ui32ColDst].eFlags);

		/* Colour operation */
		SETUP_INSTRUCTION_ARG(6, USEASM_INTSRCSEL_SUB, USEASM_REGTYPE_INTSRCSEL, 0);
	}
	else 
	{
		/* Src 1 */
		SETUP_INSTRUCTION_ARG(1, 0, USEASM_REGTYPE_TEMP, 0);

		/* Src 2 */
		SETUP_INSTRUCTION_ARG(2, 0, USEASM_REGTYPE_OUTPUT, 0);

		/* Src1 modifier */
		SETUP_INSTRUCTION_ARG(3, USEASM_INTSRCSEL_NONE, USEASM_REGTYPE_INTSRCSEL, 0);

		/* Src1 colour factor */
		SETUP_INSTRUCTION_ARG(4, asUSEASMColSrc[ui32ColSrc].eSrcSelect, USEASM_REGTYPE_INTSRCSEL, asUSEASMColSrc[ui32ColSrc].eFlags);

		/* Src2 colour factor */
		SETUP_INSTRUCTION_ARG(5, asUSEASMColSrc[ui32ColDst].eSrcSelect, USEASM_REGTYPE_INTSRCSEL, asUSEASMColSrc[ui32ColDst].eFlags);

		if(ui32ColEquation==GLES1_BLENDMODE_ADD)
		{
			/* Colour operation */
			SETUP_INSTRUCTION_ARG(6, USEASM_INTSRCSEL_ADD, USEASM_REGTYPE_INTSRCSEL, 0);
		}
		else
		{
			/* Colour operation */
			SETUP_INSTRUCTION_ARG(6, USEASM_INTSRCSEL_SUB, USEASM_REGTYPE_INTSRCSEL, 0);
		}
	}

	if(gc->sPrim.sRenderState.ui32AlphaTestFlags & GLES1_ALPHA_TEST_ONLY)
	{
		AddInstruction(gc, psUSEASMInfo, USEASM_OP_SOP2, USEASM_OPFLAGS1_MAINISSUE | (USEASM_PRED_P0<<USEASM_OPFLAGS1_PRED_SHIFT), 0, 0, asArg, 7);
	}
	else
	{
		AddInstruction(gc, psUSEASMInfo, USEASM_OP_SOP2, USEASM_OPFLAGS1_MAINISSUE, 0, 0, asArg, 7);
	}


	/* Src1 modifier */
	SETUP_INSTRUCTION_ARG(0, USEASM_INTSRCSEL_NONE, USEASM_REGTYPE_INTSRCSEL, 0);

	if(ui32ColEquation == GLES1_BLENDMODE_REVSUBTRACT)
	{
		IMG_UINT32 ui32Temp;

		ui32Temp = ui32AlphaSrc;

		ui32AlphaSrc = aui32RevSubtractOperandSwap[ui32AlphaDst];

		ui32AlphaDst = aui32RevSubtractOperandSwap[ui32Temp];

		/* Src1 alpha factor */
		SETUP_INSTRUCTION_ARG(1, asUSEASMAlphaSrc[ui32AlphaSrc].eSrcSelect, USEASM_REGTYPE_INTSRCSEL, asUSEASMAlphaSrc[ui32AlphaSrc].eFlags);

		/* Src2 alpha factor */
		SETUP_INSTRUCTION_ARG(2, asUSEASMAlphaSrc[ui32AlphaDst].eSrcSelect, USEASM_REGTYPE_INTSRCSEL, asUSEASMAlphaSrc[ui32AlphaDst].eFlags);

		if(ui32AlphaEquation==GLES1_BLENDMODE_ADD)
		{
			/* Alpha operation */
			SETUP_INSTRUCTION_ARG(3, USEASM_INTSRCSEL_ADD, USEASM_REGTYPE_INTSRCSEL, 0);
		}
		else
		{
			/* Alpha operation */
			SETUP_INSTRUCTION_ARG(3, USEASM_INTSRCSEL_SUB, USEASM_REGTYPE_INTSRCSEL, 0);
		}

		if(ui32AlphaEquation == GLES1_BLENDMODE_SUBTRACT)
		{
			/* Destination modifier */
			SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_NEG, USEASM_REGTYPE_INTSRCSEL, 0);
		}
		else
		{
			/* Destination modifier */
			SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_NONE, USEASM_REGTYPE_INTSRCSEL, 0);
		}
	}
	else 
	{
		/* Src1 alpha factor */
		SETUP_INSTRUCTION_ARG(1, asUSEASMAlphaSrc[ui32AlphaSrc].eSrcSelect, USEASM_REGTYPE_INTSRCSEL, asUSEASMAlphaSrc[ui32AlphaSrc].eFlags);

		/* Src2 alpha factor */
		SETUP_INSTRUCTION_ARG(2, asUSEASMAlphaSrc[ui32AlphaDst].eSrcSelect, USEASM_REGTYPE_INTSRCSEL, asUSEASMAlphaSrc[ui32AlphaDst].eFlags);

		if(ui32AlphaEquation==GLES1_BLENDMODE_ADD)
		{
			/* Alpha operation */
			SETUP_INSTRUCTION_ARG(3, USEASM_INTSRCSEL_ADD, USEASM_REGTYPE_INTSRCSEL, 0);
		}
		else
		{
			/* Alpha operation */
			SETUP_INSTRUCTION_ARG(3, USEASM_INTSRCSEL_SUB, USEASM_REGTYPE_INTSRCSEL, 0);
		}

		if(ui32AlphaEquation == GLES1_BLENDMODE_REVSUBTRACT)
		{
			/* Destination modifier */
			SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_NEG, USEASM_REGTYPE_INTSRCSEL, 0);
		}
		else
		{
			/* Destination modifier */
			SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_NONE, USEASM_REGTYPE_INTSRCSEL, 0);
		}
	}


	AddInstruction(gc, psUSEASMInfo, USEASM_OP_ASOP2, 0, USEASM_OPFLAGS2_COISSUE, 0, asArg, 5);
}


/***********************************************************************************
 Function Name      : CreateLogicalOpsUSEASMCode
 Inputs             : gc, psUSEASMInfo
 Outputs            : psUSEASMInfo
 Returns            : -
 Description        : Adds USEASM instructions for logical ops
************************************************************************************/
static IMG_VOID CreateLogicalOpsUSEASMCode(GLES1Context *gc, GLESUSEASMInfo *psUSEASMInfo)
{
	USE_REGISTER asArg[3];
	IMG_UINT32 ui32Predicate;

	if(gc->sPrim.sRenderState.ui32AlphaTestFlags & GLES1_ALPHA_TEST_ONLY)
	{
		ui32Predicate = USEASM_PRED_P0 << USEASM_OPFLAGS1_PRED_SHIFT;
	}
	else
	{
		ui32Predicate = USEASM_PRED_NONE;
	}

	switch(gc->sState.sRaster.ui32LogicOp)
	{
		case GL_CLEAR:	/* [0,0,0,0] = d XOR d */
		{
			/* Dst */
			SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 1 */
			SETUP_INSTRUCTION_ARG(1, 0, USEASM_REGTYPE_OUTPUT, 0);

			/* Src 2 */
			SETUP_INSTRUCTION_ARG(2, 0, USEASM_REGTYPE_OUTPUT, 0);

			AddInstruction(gc, psUSEASMInfo, USEASM_OP_XOR, ui32Predicate, 0, 0, asArg, 3);

			break;
		}
		case GL_AND:		/* s AND d   */
		{
			/* Dst */
			SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 1 */
			SETUP_INSTRUCTION_ARG(1, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 2 */
			SETUP_INSTRUCTION_ARG(2, 0, USEASM_REGTYPE_OUTPUT, 0);

			AddInstruction(gc, psUSEASMInfo, USEASM_OP_AND, ui32Predicate, 0, 0, asArg, 3);

			break;
		}
		case GL_AND_REVERSE:	/* s AND ~d  */
		{
			/* Dst */
			SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 1 */
			SETUP_INSTRUCTION_ARG(1, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 2 */
			SETUP_INSTRUCTION_ARG(2, 0, USEASM_REGTYPE_OUTPUT, USEASM_ARGFLAGS_INVERT);

			AddInstruction(gc, psUSEASMInfo, USEASM_OP_AND, ui32Predicate, 0, 0, asArg, 3);

			break;
		}
		case GL_COPY:	/* s AND s   */
		{
			/* Dst */
			SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 1 */
			SETUP_INSTRUCTION_ARG(1, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 2 */
			SETUP_INSTRUCTION_ARG(2, 0, USEASM_REGTYPE_TEMP, 0);

			AddInstruction(gc, psUSEASMInfo, USEASM_OP_AND, ui32Predicate, 0, 0, asArg, 3);

			break;
		}
		case GL_AND_INVERTED:	/* d AND ~s  */
		{
			/* Dst */
			SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 1 */
			SETUP_INSTRUCTION_ARG(1, 0, USEASM_REGTYPE_OUTPUT, 0);

			/* Src 2 */
			SETUP_INSTRUCTION_ARG(2, 0, USEASM_REGTYPE_TEMP, USEASM_ARGFLAGS_INVERT);

			AddInstruction(gc, psUSEASMInfo, USEASM_OP_AND, ui32Predicate, 0, 0, asArg, 3);

			break;
		}
		case GL_NOOP:	/* No-op	*/
		{
			PVR_DPF((PVR_DBG_WARNING,"CreateLogicalOpsUSEASMCode(): LogicalOps code requested, but Op=GL_NOOP"));

			break;
		}
		case GL_XOR:		/* s XOR d	*/
		{
			/* Dst */
			SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 1 */
			SETUP_INSTRUCTION_ARG(1, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 2 */
			SETUP_INSTRUCTION_ARG(2, 0, USEASM_REGTYPE_OUTPUT, 0);

			AddInstruction(gc, psUSEASMInfo, USEASM_OP_XOR, ui32Predicate, 0, 0, asArg, 3);

			break;
		}
		case GL_OR:		/* s OR d	*/
		{
			/* Dst */
			SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 1 */
			SETUP_INSTRUCTION_ARG(1, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 2 */
			SETUP_INSTRUCTION_ARG(2, 0, USEASM_REGTYPE_OUTPUT, 0);

			AddInstruction(gc, psUSEASMInfo, USEASM_OP_OR, ui32Predicate, 0, 0, asArg, 3);

			break;
		}
		case GL_NOR:		/* ~(s OR d)	*/
		{
			/* Dst */
			SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 1 */
			SETUP_INSTRUCTION_ARG(1, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 2 */
			SETUP_INSTRUCTION_ARG(2, 0, USEASM_REGTYPE_OUTPUT, 0);

			AddInstruction(gc, psUSEASMInfo, USEASM_OP_OR, ui32Predicate, 0, 0, asArg, 3);


			/* Dst */
			SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 1 */
			SETUP_INSTRUCTION_ARG(1, EURASIA_USE_SPECIAL_CONSTANT_INT32ONE, USEASM_REGTYPE_FPCONSTANT, 0);

			/* Src 2 */
			SETUP_INSTRUCTION_ARG(2, 0, USEASM_REGTYPE_TEMP, USEASM_ARGFLAGS_INVERT);

			AddInstruction(gc, psUSEASMInfo, USEASM_OP_AND, ui32Predicate, 0, 0, asArg, 3);

			break;
		}
		case GL_EQUIV:	/* ~(s XOR d)	*/
		{
			/* Dst */
			SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 1 */
			SETUP_INSTRUCTION_ARG(1, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 2 */
			SETUP_INSTRUCTION_ARG(2, 0, USEASM_REGTYPE_OUTPUT, 0);

			AddInstruction(gc, psUSEASMInfo, USEASM_OP_XOR, ui32Predicate, 0, 0, asArg, 3);


			/* Dst */
			SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 1 */
			SETUP_INSTRUCTION_ARG(1, EURASIA_USE_SPECIAL_CONSTANT_INT32ONE, USEASM_REGTYPE_FPCONSTANT, 0);

			/* Src 2 */
			SETUP_INSTRUCTION_ARG(2, 0, USEASM_REGTYPE_TEMP, USEASM_ARGFLAGS_INVERT);

			AddInstruction(gc, psUSEASMInfo, USEASM_OP_AND, ui32Predicate, 0, 0, asArg, 3);

			break;
		}
		case GL_INVERT:	/* ~d = 1 AND ~d */
		{
			/* Dst */
			SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 1 */
			SETUP_INSTRUCTION_ARG(1, EURASIA_USE_SPECIAL_CONSTANT_INT32ONE, USEASM_REGTYPE_FPCONSTANT, 0);

			/* Src 2 */
			SETUP_INSTRUCTION_ARG(2, 0, USEASM_REGTYPE_OUTPUT, USEASM_ARGFLAGS_INVERT);

			AddInstruction(gc, psUSEASMInfo, USEASM_OP_AND, ui32Predicate, 0, 0, asArg, 3);

			break;
		}
		case GL_OR_REVERSE:	/* s OR ~d */
		{
			/* Dst */
			SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 1 */
			SETUP_INSTRUCTION_ARG(1, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 2 */
			SETUP_INSTRUCTION_ARG(2, 0, USEASM_REGTYPE_OUTPUT, USEASM_ARGFLAGS_INVERT);

			AddInstruction(gc, psUSEASMInfo, USEASM_OP_OR, ui32Predicate, 0, 0, asArg, 3);

			break;
		}
		case GL_COPY_INVERTED:	/* 1 AND ~s	*/
		{
			/* Dst */
			SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 1 */
			SETUP_INSTRUCTION_ARG(1, EURASIA_USE_SPECIAL_CONSTANT_INT32ONE, USEASM_REGTYPE_FPCONSTANT, 0);

			/* Src 2 */
			SETUP_INSTRUCTION_ARG(2, 0, USEASM_REGTYPE_TEMP, USEASM_ARGFLAGS_INVERT);

			AddInstruction(gc, psUSEASMInfo, USEASM_OP_AND, ui32Predicate, 0, 0, asArg, 3);

			break;
		}
		case GL_OR_INVERTED:	/* d OR ~s	*/
		{
			/* Dst */
			SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 1 */
			SETUP_INSTRUCTION_ARG(1, 0, USEASM_REGTYPE_OUTPUT, 0);

			/* Src 2 */
			SETUP_INSTRUCTION_ARG(2, 0, USEASM_REGTYPE_TEMP, USEASM_ARGFLAGS_INVERT);

			AddInstruction(gc, psUSEASMInfo, USEASM_OP_OR, ui32Predicate, 0, 0, asArg, 3);

			break;
		}
		case GL_NAND:	/* ~(s AND d)	*/
		{
			/* Dst */
			SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 1 */
			SETUP_INSTRUCTION_ARG(1, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 2 */
			SETUP_INSTRUCTION_ARG(2, 0, USEASM_REGTYPE_OUTPUT, 0);

			AddInstruction(gc, psUSEASMInfo, USEASM_OP_AND, ui32Predicate, 0, 0, asArg, 3);


			/* Dst */
			SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 1 */
			SETUP_INSTRUCTION_ARG(1, EURASIA_USE_SPECIAL_CONSTANT_INT32ONE, USEASM_REGTYPE_FPCONSTANT, 0);

			/* Src 2 */
			SETUP_INSTRUCTION_ARG(2, 0, USEASM_REGTYPE_TEMP, USEASM_ARGFLAGS_INVERT);

			AddInstruction(gc, psUSEASMInfo, USEASM_OP_AND, ui32Predicate, 0, 0, asArg, 3);

			break;
		}
		case GL_SET:		/* d OR ~d	*/
		{
			/* Dst */
			SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 1 */
			SETUP_INSTRUCTION_ARG(1, 0, USEASM_REGTYPE_OUTPUT, 0);

			/* Src 2 */
			SETUP_INSTRUCTION_ARG(2, 0, USEASM_REGTYPE_OUTPUT, USEASM_ARGFLAGS_INVERT);

			AddInstruction(gc, psUSEASMInfo, USEASM_OP_OR, ui32Predicate, 0, 0, asArg, 3);

			break;
		}
	}
}



/***********************************************************************************
 Function Name      : CreateColorMaskUSEASMCode
 Inputs             : gc, psUSEASMInfo
 Outputs            : psUSEASMInfo
 Returns            : -
 Description        : Adds USEASM instructions for color mask
************************************************************************************/
static IMG_VOID CreateColorMaskUSEASMCode(GLES1Context *gc, GLESUSEASMInfo *psUSEASMInfo)
{
	USE_REGISTER asArg[7];
	IMG_UINT32 ui32ColourMask;

	ui32ColourMask = 0;

	if(gc->sState.sRaster.ui32ColorMask & GLES1_COLORMASK_ALPHA)
	{
		ui32ColourMask |= 0x8;
	}
	if(gc->sState.sRaster.ui32ColorMask & GLES1_COLORMASK_RED)
	{
#if defined(SGX_FEATURE_USE_VEC34)
		ui32ColourMask |= 0x1;
#else
		ui32ColourMask |= 0x4;
#endif
	}
	if(gc->sState.sRaster.ui32ColorMask & GLES1_COLORMASK_GREEN)
	{
		ui32ColourMask |= 0x2;
	}
	if(gc->sState.sRaster.ui32ColorMask & GLES1_COLORMASK_BLUE)
	{
#if defined(SGX_FEATURE_USE_VEC34)
		ui32ColourMask |= 0x4;
#else
		ui32ColourMask |= 0x1;
#endif
	}

	/* Dst */
	SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_OUTPUT, USEASM_ARGFLAGS_BYTEMSK_PRESENT | (ui32ColourMask<<USEASM_ARGFLAGS_BYTEMSK_SHIFT));

	/* Src1 */
	SETUP_INSTRUCTION_ARG(1, 0, USEASM_REGTYPE_TEMP, 0);

	/* Src2 */
	SETUP_INSTRUCTION_ARG(2, 0, USEASM_REGTYPE_TEMP, 0);

	/* Modifier 1 */
	SETUP_INSTRUCTION_ARG(3, USEASM_INTSRCSEL_ZERO, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Modifier 2 */
	SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_ZERO, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Colour operation */
	SETUP_INSTRUCTION_ARG(5, USEASM_INTSRCSEL_MAX, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Alpha operation */
	SETUP_INSTRUCTION_ARG(6, USEASM_INTSRCSEL_MAX, USEASM_REGTYPE_INTSRCSEL, 0);

	if(gc->sPrim.sRenderState.ui32AlphaTestFlags & GLES1_ALPHA_TEST_ONLY)
	{
		AddInstruction(gc, psUSEASMInfo, USEASM_OP_SOP2WM, USEASM_OPFLAGS1_END | (USEASM_PRED_P0<<USEASM_OPFLAGS1_PRED_SHIFT), 0, 0, asArg, 7);
	}
	else
	{
		AddInstruction(gc, psUSEASMInfo, USEASM_OP_SOP2WM, USEASM_OPFLAGS1_END, 0, 0, asArg, 7);
	}
}


/***********************************************************************************
 Function Name      : CreateFogUSEASMCode
 Inputs             : gc, psUSEASMInfo,	ui32FogPAReg, bDestIsOutput
 Outputs            : psUSEASMInfo
 Returns            : -
 Description        : Adds USEASM instructions for fog
************************************************************************************/
static IMG_VOID CreateFogUSEASMCode(GLES1Context	 *gc,
									GLESUSEASMInfo   *psUSEASMInfo,
									IMG_UINT32       ui32FogPAReg)
{
	USE_REGISTER asArg[7];
	IMG_UINT32 ui32SAOffset = 0;

	/*
	 * Pack floating-point fog factor into a U8 
	 * PCK	Temp1.xyzw, FogPAReg F32->U8, 1.0f F32->U8
	*/

#if defined(SGX_FEATURE_USE_VEC34)
	/* Dst */
	SETUP_INSTRUCTION_ARG(0, 1, USEASM_REGTYPE_TEMP, 0);
	
	if(ui32FogPAReg & 1)
	{
		/* Src 1 */
		SETUP_INSTRUCTION_ARG(1, ui32FogPAReg - 1, USEASM_REGTYPE_PRIMATTR, 0);

		/* Src 2 */
		SETUP_INSTRUCTION_ARG(2, ui32FogPAReg - 1, USEASM_REGTYPE_PRIMATTR, 0);

		/* Swizzle */
		SETUP_INSTRUCTION_ARG(3, USEASM_SWIZZLE(Y,Y,Y,Y), USEASM_REGTYPE_SWIZZLE, 0);
	}
	else
	{
		/* Src 1 */
		SETUP_INSTRUCTION_ARG(1, ui32FogPAReg, USEASM_REGTYPE_PRIMATTR, 0);

		/* Src 2 */
		SETUP_INSTRUCTION_ARG(2, ui32FogPAReg, USEASM_REGTYPE_PRIMATTR, 0);

		/* Swizzle */
		SETUP_INSTRUCTION_ARG(3, USEASM_SWIZZLE(X,X,X,X), USEASM_REGTYPE_SWIZZLE, 0);

	}

	AddInstruction(gc, psUSEASMInfo, USEASM_OP_VPCKU8F32, (0xF<<USEASM_OPFLAGS1_MASK_SHIFT), USEASM_OPFLAGS2_SCALE, 0, asArg, 4);

#else /* defined(SGX_FEATURE_USE_VEC34) */

	/* Dst */
	SETUP_INSTRUCTION_ARG(0, 1, USEASM_REGTYPE_TEMP, (0xF<<USEASM_ARGFLAGS_BYTEMSK_SHIFT));

	/* Src 1 */
	SETUP_INSTRUCTION_ARG(1, ui32FogPAReg, USEASM_REGTYPE_PRIMATTR, 0);

	/* Src 2 */
	SETUP_INSTRUCTION_ARG(2, ui32FogPAReg, USEASM_REGTYPE_PRIMATTR, 0);

	/* Rounding */
	SETUP_INSTRUCTION_ARG(3, USEASM_INTSRCSEL_ROUNDNEAREST, USEASM_REGTYPE_INTSRCSEL, 0);

	AddInstruction(gc, psUSEASMInfo, USEASM_OP_PCKU8F32, 0, USEASM_OPFLAGS2_SCALE, 0, asArg, 4);

#endif /* defined(SGX_FEATURE_USE_VEC34) */

	/* 
	* Fog blend:
	* 
	* RGB = FogFactor * Fragment.rgb + (1-FogFactor) * FogColor.rgb
	* A   = Fragment.a
	* 
	* SOP3 in Interpolate1 mode does:
	* 
	* Temp0.rgb = (1-Src0.a) * CSEL10 + Src0.a * AMOD(CSEL11)
	* Temp0.a	 = CMOD1(CSEL1) * Src1.a AOP CMOD2(CSEL2) * Src2.a
	* 
	* Setup to do:
	* 
	* Temp0.rgb = (1-Src0.a) * Src1.rgb + Src0.a * NONE(Src2.rgb)
	* Temp0.a	 = NONE(ZERO) * Src1.a + COMPLEMENT(ZERO) * Src2.a
	* 
	* With: Src0.a = fog factor = Temp1
	*       Src1 = fog colour = Secondary attribute register
	*       Src2 = fragment colour = Temp0
	*/
	/* Dst */
	SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_TEMP, 0);


	/* Src0 */
	SETUP_INSTRUCTION_ARG(1, 1, USEASM_REGTYPE_TEMP, 0);

	/* Src1 */
	if(gc->sPrim.sRenderState.ui32AlphaTestFlags)
	{
#if defined(SGX_FEATURE_ALPHATEST_SECONDARY_PERPRIMITIVE)
		if(gc->sPrim.sRenderState.ui32AlphaTestFlags & GLES1_ALPHA_TEST_FEEDBACK)
		{
			ui32SAOffset += GLES1_FRAGMENT_SECATTR_ALPHATEST_OFFSET;
		}
#endif
		
		/* Adjust secondary attribute offset for alpha test attributes */
		ui32SAOffset += GLES1_FRAGMENT_SECATTR_NUM_ALPHATEST;
	}

	SETUP_INSTRUCTION_ARG(2, ui32SAOffset, USEASM_REGTYPE_SECATTR, 0);

	/* Src2 */
	SETUP_INSTRUCTION_ARG(3, 0, USEASM_REGTYPE_TEMP, 0);

	SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_SRC1, USEASM_REGTYPE_INTSRCSEL, 0);
	SETUP_INSTRUCTION_ARG(5, USEASM_INTSRCSEL_SRC2, USEASM_REGTYPE_INTSRCSEL, 0);
	SETUP_INSTRUCTION_ARG(6, USEASM_INTSRCSEL_SRC0ALPHA, USEASM_REGTYPE_INTSRCSEL, 0);

	if(gc->sPrim.sRenderState.ui32AlphaTestFlags & GLES1_ALPHA_TEST_ONLY)
	{
		AddInstruction(gc, psUSEASMInfo, USEASM_OP_LRP1, USEASM_OPFLAGS1_MAINISSUE | (USEASM_PRED_P0<<USEASM_OPFLAGS1_PRED_SHIFT), 0, 0, asArg, 7);
	}
	else
	{
		AddInstruction(gc, psUSEASMInfo, USEASM_OP_LRP1, USEASM_OPFLAGS1_MAINISSUE, 0, 0, asArg, 7);
	}


	SETUP_INSTRUCTION_ARG(0, USEASM_INTSRCSEL_ZERO, USEASM_REGTYPE_INTSRCSEL, 0);
	SETUP_INSTRUCTION_ARG(1, USEASM_INTSRCSEL_ONE, USEASM_REGTYPE_INTSRCSEL, 0);
	SETUP_INSTRUCTION_ARG(2, USEASM_INTSRCSEL_ADD, USEASM_REGTYPE_INTSRCSEL, 0);

	AddInstruction(gc, psUSEASMInfo, USEASM_OP_ASOP, 0, USEASM_OPFLAGS2_COISSUE, 0, asArg, 3);
}


/***********************************************************************************
 Function Name      : WriteEOTUSSECode
 Inputs             : gc
 Outputs            : 
 Returns            : Device address
 Description        : Writes end of tile USSE code variant appropriate for current 
					  state. This may patch preexisting state if there has been a 
					  change between StartFrame and the first ScheduleTA
************************************************************************************/
IMG_INTERNAL GLES1_MEMERROR WriteEOTUSSECode(GLES1Context *gc, EGLPixelBEState *psPBEState, 
											 IMG_DEV_VIRTADDR *puDevAddr,   IMG_BOOL bPatch)
{
	IMG_UINT32 *pui32Buffer, *pui32BufferBase;
	IMG_UINT32 ui32TileCodeSize;


	if(bPatch)
	{
		pui32BufferBase = psPBEState->pui32PixelEventUSSE;

		GLES1_ASSERT(pui32BufferBase);
	}
	else
	{
		ui32TileCodeSize = USE_PIXELEVENT_EOT_BYTES;
		ui32TileCodeSize = (ui32TileCodeSize + (EURASIA_PDS_DOUTU_PHASE_START_ALIGN - 1)) & ~(EURASIA_PDS_DOUTU_PHASE_START_ALIGN - 1);

		pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers, ui32TileCodeSize >> 2, CBUF_TYPE_USSE_FRAG_BUFFER, IMG_FALSE);
		
		if(!pui32BufferBase)
		{
			return GLES1_3D_BUFFER_ERROR;
		}

		/* Record pixel event position so we can patch at first kick if necessary */
		psPBEState->pui32PixelEventUSSE = pui32BufferBase;
	}

	pui32Buffer = WriteEndOfTileUSSECode(pui32BufferBase, 
										 psPBEState->aui32EmitWords, 
										 psPBEState->ui32SidebandWord
#if defined(SGX_FEATURE_RENDER_TARGET_ARRAYS)
										 , IMG_NULL
#endif
#if defined(SGX_FEATURE_WRITEBACK_DCU)
										 , IMG_FALSE
#endif
										 );
	
	GLES1_INC_COUNT(GLES1_TIMER_USSE_FRAG_DATA_COUNT, (pui32Buffer - pui32BufferBase));

	*puDevAddr = CBUF_GetBufferDeviceAddress(gc->apsBuffers, pui32BufferBase, CBUF_TYPE_USSE_FRAG_BUFFER);

	if(bPatch)
	{
		/* Reset pixel event position until next StartFrame */
		psPBEState->pui32PixelEventUSSE = IMG_NULL;
	}
	else
	{
		CBUF_UpdateBufferPos(gc->apsBuffers, (IMG_UINT32)(pui32Buffer - pui32BufferBase), CBUF_TYPE_USSE_FRAG_BUFFER);
	}
	
	return GLES1_NO_ERROR;
}

/***********************************************************************************
 Function Name      : InitPixelEventUSECodeBlocks
 Inputs             : gc
 Outputs            : 
 Returns            : Success
 Description        : Initialises any USSE code blocks required for pixel events
************************************************************************************/
static IMG_BOOL InitPixelEventUSECodeBlocks(GLES1Context *gc)
{
	IMG_UINT32 *pui32USEFragmentBuffer;

	gc->sPrim.psPixelEventPTOFFCodeBlock = 
		UCH_CodeHeapAllocate(gc->psSharedState->psUSEFragmentCodeHeap, USE_PIXELEVENT_PTOFF_BYTES, gc->psSysContext->hPerProcRef);

	GLES1_INC_COUNT(GLES1_TIMER_USECODEHEAP_FRAG_COUNT, USE_PIXELEVENT_PTOFF_BYTES >> 2);

	if(!gc->sPrim.psPixelEventPTOFFCodeBlock)
	{
		PVR_DPF((PVR_DBG_FATAL,"Couldn't allocate USE pixel event PTOFF code"));
		return IMG_FALSE;
	}

	pui32USEFragmentBuffer = gc->sPrim.psPixelEventPTOFFCodeBlock->pui32LinAddress;

	WritePTOffUSSECode(pui32USEFragmentBuffer);

	gc->sPrim.psPixelEventEORCodeBlock = 
		UCH_CodeHeapAllocate(gc->psSharedState->psUSEFragmentCodeHeap, USE_PIXELEVENT_EOR_BYTES, gc->psSysContext->hPerProcRef);

	GLES1_INC_COUNT(GLES1_TIMER_USECODEHEAP_FRAG_COUNT, USE_PIXELEVENT_EOR_BYTES >> 2);

	if(!gc->sPrim.psPixelEventEORCodeBlock)
	{
		UCH_CodeHeapFree(gc->sPrim.psPixelEventPTOFFCodeBlock);
		PVR_DPF((PVR_DBG_FATAL,"Couldn't allocate USE pixel event End of Render code"));
		return IMG_FALSE;
	}

	pui32USEFragmentBuffer = gc->sPrim.psPixelEventEORCodeBlock->pui32LinAddress;

	WriteEndOfRenderUSSECode(pui32USEFragmentBuffer);


	return IMG_TRUE;
}

/***********************************************************************************
 Function Name      : FreePixelEventUSECodeBlocks
 Inputs             : gc
 Outputs            : 
 Returns            : -
 Description        : Frees all USSE code blocks required for pixel events
************************************************************************************/
static IMG_VOID FreePixelEventUSECodeBlocks(GLES1Context *gc)
{
	UCH_CodeHeapFree(gc->sPrim.psPixelEventEORCodeBlock);
	UCH_CodeHeapFree(gc->sPrim.psPixelEventPTOFFCodeBlock);
}

/***********************************************************************************
 Function Name      : GetStateCopyUSEAddress
 Inputs             : gc, ui32NumStateDWords
 Outputs            : 
 Returns            : Device address
 Description        : Gets device address of state copy USSE code variant appropriate
					  for number of DWords to copy
************************************************************************************/
IMG_INTERNAL IMG_DEV_VIRTADDR GetStateCopyUSEAddress(GLES1Context *gc, IMG_UINT32 ui32NumStateDWords)
{
	IMG_DEV_VIRTADDR uDevAddr;

	uDevAddr = gc->sPrim.psStateCopyCodeBlock->sCodeAddress;

	/* Total of GLES1_MAX_MTE_STATE_DWORDS versions of the state copy code. First 16 take 2 instructions, the rest
	 * take 3 instructions (round to 4 for addressing granularity)
	 */
	if(ui32NumStateDWords <= EURASIA_USE_MAXIMUM_REPEAT)
	{
		uDevAddr.uiAddr += ((ui32NumStateDWords - 1) * GLES1_STATECOPY_FIRST16_SIZE_IN_BYTES);
	}
	else
	{
		uDevAddr.uiAddr += (EURASIA_USE_MAXIMUM_REPEAT * GLES1_STATECOPY_FIRST16_SIZE_IN_BYTES);
		uDevAddr.uiAddr += ((ui32NumStateDWords - EURASIA_USE_MAXIMUM_REPEAT - 1) * GLES1_STATECOPY_REMAIN_SIZE_IN_BYTES);
	}

	return uDevAddr;
}

/***********************************************************************************
 Function Name      : InitStateCopyUSECodeBlocks
 Inputs             : gc
 Outputs            : 
 Returns            : Success
 Description        : Initialises any USSE code blocks required for state copy
************************************************************************************/
static IMG_BOOL InitStateCopyUSECodeBlocks(GLES1Context *gc)
{
	IMG_UINT32 *pui32BufferBase, *pui32Buffer;
	IMG_UINT32 i, ui32CodeSize;
	IMG_UINT32 ui32First16Offset, ui32RemainOffset;

	ui32CodeSize  = GLES1_STATECOPY_FIRST16_SIZE_IN_BYTES * EURASIA_USE_MAXIMUM_REPEAT;

	ui32CodeSize += GLES1_STATECOPY_REMAIN_SIZE_IN_BYTES  * (GLES1_MAX_MTE_STATE_DWORDS - EURASIA_USE_MAXIMUM_REPEAT) ;

	ui32First16Offset = GLES1_STATECOPY_FIRST16_SIZE_IN_BYTES >> 2;
	ui32RemainOffset  = GLES1_STATECOPY_REMAIN_SIZE_IN_BYTES  >> 2;
	
	gc->sPrim.psStateCopyCodeBlock = 
		UCH_CodeHeapAllocate(gc->psSharedState->psUSEVertexCodeHeap, ui32CodeSize, gc->psSysContext->hPerProcRef);
	
	GLES1_INC_COUNT(GLES1_TIMER_USECODEHEAP_VERT_COUNT, ui32CodeSize >> 2);

	if(!gc->sPrim.psStateCopyCodeBlock)
	{
		PVR_DPF((PVR_DBG_FATAL,"Couldn't allocate USE state copy code"));
		return IMG_FALSE;
	}
	pui32Buffer = pui32BufferBase = gc->sPrim.psStateCopyCodeBlock->pui32LinAddress;

	/*
		Generate the USE state copy program. (for the first 16 copy programs)
	*/
	for (i = 0; i < EURASIA_USE_MAXIMUM_REPEAT; i++)
	{
	    /* Reset pui32BufferBase */

	    pui32BufferBase = pui32Buffer;

		pui32Buffer = USEGenWriteStateEmitProgram (pui32BufferBase, i + 1, 0, IMG_FALSE);	/* PRQA S 3198 */ /* pui32Buffer is used in the following GLES1_ASSERT. */

		GLES1_ASSERT((IMG_UINT32)(pui32Buffer - pui32BufferBase) <= ui32First16Offset);

		/* Update pui32Buffer */
		pui32Buffer = pui32BufferBase + ui32First16Offset;
	}

	/*
		Generate the USE state copy program. (for the remaining copy programs)
	*/
	for (i = EURASIA_USE_MAXIMUM_REPEAT; i < GLES1_MAX_MTE_STATE_DWORDS; i++)
	{
	    /* Reset pui32BufferBase */

	    pui32BufferBase = pui32Buffer;

		pui32Buffer = USEGenWriteStateEmitProgram (pui32BufferBase, i + 1, 0, IMG_FALSE);	/* PRQA S 3198 */ /* pui32Buffer is used in the following GLES1_ASSERT. */

		GLES1_ASSERT((IMG_UINT32)(pui32Buffer - pui32BufferBase) <= ui32RemainOffset);

		/* Ensure each variant is aligned to 2 instruction boundary */
		pui32Buffer = pui32BufferBase + ui32RemainOffset;

	}

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : FreeStateCopyUSECodeBlocks
 Inputs             : gc
 Outputs            : 
 Returns            : -
 Description        : Frees all USSE code blocks required for state copy
************************************************************************************/
static IMG_VOID FreeStateCopyUSECodeBlocks(GLES1Context *gc)
{
	UCH_CodeHeapFree(gc->sPrim.psStateCopyCodeBlock);
}


/*****************************************************************************
 Function Name	: InitSpecialUSECodeBlocks
 Inputs			: gc
 Outputs		: -
 Returns		: Success
 Description	: Allocates and initialises specially generated use code
*****************************************************************************/

IMG_INTERNAL IMG_BOOL InitSpecialUSECodeBlocks(GLES1Context *gc)
{
	if(!InitAccumUSECodeBlocks(gc))
	{
		return IMG_FALSE;
	}

	if(!InitClearUSECodeBlocks(gc))
	{
		return IMG_FALSE;
	}

	if(!InitScissorUSECodeBlocks(gc))
	{
		return IMG_FALSE;
	}
	
	if(!InitPixelEventUSECodeBlocks(gc))
	{
		return IMG_FALSE;
	}

	if(!InitStateCopyUSECodeBlocks(gc))
	{
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Function Name	: FreeSpecialUSECodeBlocks
 Inputs			: gc
 Outputs		: -
 Returns		: -
 Description	: Frees specially generated use code
*****************************************************************************/
IMG_INTERNAL IMG_VOID FreeSpecialUSECodeBlocks(GLES1Context *gc)
{
	FreeAccumUSECodeBlocks(gc);

	FreeClearUSECodeBlocks(gc);

	FreeScissorUSECodeBlocks(gc);
	
	FreePixelEventUSECodeBlocks(gc);

	FreeStateCopyUSECodeBlocks(gc);

}


/*****************************************************************************
 Function Name	: CreateVertexUnpackUSECode
 Inputs			: gc, psVertexShader, ui32MainCodeSizeInBytes
 Outputs		: ppui32Instruction
 Returns		: Mem Error
 Description	: Generates USE code to perform vertex attribute unpacking.
                For example, unpacks vertex position in fixed format to floating point.
*****************************************************************************/
static GLES1_MEMERROR CreateVertexUnpackUSECode(GLES1Context *gc, GLES1ShaderVariant *psVertexVariant,
												IMG_UINT32 ui32MainCodeSizeInBytes, IMG_BOOL bUSEPerInstanceMode,
												IMG_UINT32 **ppui32Instruction)
{
	IMG_UINT32 i, ui32NumInstructions, ui32NumTemps, ui32CodeSizeInBytes, ui32AlignSize;
	IMG_UINT32 *pui32Code;
	IMG_UINT32 j;
#if defined(SGX_FEATURE_USE_VEC34) || defined(FIX_HW_BRN_31988)
	IMG_BOOL bNeedNOPForEndFlag = IMG_FALSE;
#endif

	ui32NumInstructions = 0;
	ui32NumTemps = 0;

	psVertexVariant->ui32PhaseCount = 0;

	/* First check for number of instructions */
	if (gc->sPrim.eCurrentPrimitiveType!=GLES1_PRIMTYPE_DRAWTEXTURE)
	{
	    psVertexVariant->u.sVertex.ui32NumItemsPerVertex = gc->sVAOMachine.ui32NumItemsPerVertex;

		for (i = 0; i < psVertexVariant->u.sVertex.ui32NumItemsPerVertex; i++)
		{
			VSInputReg *pasVSInputRegisters  = &gc->sProgram.psCurrentVertexShader->asVSInputRegisters[0];
			GLES1AttribArrayPointerMachine *psAttribPointer;
			IMG_UINT32 ui32CurrentAttrib, ui32Size, ui32Type;
			IMG_BOOL bNormalized;

			psAttribPointer = gc->sVAOMachine.apsPackedAttrib[i];
	
			ui32CurrentAttrib = (IMG_UINT32)(psAttribPointer - &(gc->sVAOMachine.asAttribPointer[0]));

			ui32Size = (psAttribPointer->ui32CopyStreamTypeSize >> GLES1_STREAMSIZE_SHIFT);
			ui32Type = (psAttribPointer->ui32CopyStreamTypeSize & GLES1_STREAMTYPE_MASK);
			bNormalized = (psAttribPointer->ui32CopyStreamTypeSize & GLES1_STREAMNORM_BIT) != 0 ? IMG_TRUE : IMG_FALSE;


			ui32NumInstructions += (pasVSInputRegisters[ui32CurrentAttrib].ui32Size - ui32Size);

			GLES1_ASSERT((pasVSInputRegisters[ui32CurrentAttrib].ui32Size >= ui32Size));

			switch(ui32Type)
			{
				case GLES1_STREAMTYPE_UBYTE:
				case GLES1_STREAMTYPE_USHORT:
				case GLES1_STREAMTYPE_HALFFLOAT:
				{
					/* Simple case: use a single unpck instruction (with or without scaling) */
					ui32NumInstructions += ui32Size;

					break;
				}
				case GLES1_STREAMTYPE_BYTE:
				case GLES1_STREAMTYPE_SHORT:
				{
					if(bNormalized)
					{
						/* Software workaround for HW BRN 22509:
						   rather than using one unpck.scale instruction, we use unpck followed by fmad
						*/
						ui32NumInstructions += 2*ui32Size;
#if defined(SGX_FEATURE_USE_VEC34)
						bNeedNOPForEndFlag = IMG_TRUE;
#endif
					}
					else
					{
						/* Use a simple unpck instruction */
						ui32NumInstructions += ui32Size;
					}

					break;
				}
				case GLES1_STREAMTYPE_FIXED:
				{
					/* Fixed 16.16 -> FLoat unpacking requires 3 instructions and 1 temp */
					ui32NumInstructions += (3 * ui32Size);
					ui32NumTemps = 1;
#if defined(SGX_FEATURE_USE_VEC34)
					bNeedNOPForEndFlag = IMG_TRUE;
#endif

					break;
				}
				case GLES1_STREAMTYPE_FLOAT:
				{
					/* do nothing */
					break;
				}
				default:
				{
					PVR_DPF((PVR_DBG_FATAL,"CreateVertexUnpackUSECode: Invalid case in switch statement"));	
				}
			}

			/* Setup variant check structures for next time */
			psVertexVariant->u.sVertex.aui32StreamTypeSize[i] = psAttribPointer->ui32CopyStreamTypeSize;
		}
	}
	else
	{
		IMG_UINT32 ui32StreamTypeSize;

		ui32StreamTypeSize = GLES1_STREAMTYPE_FLOAT | (4 << GLES1_STREAMSIZE_SHIFT);

		/* Colour */
		psVertexVariant->u.sVertex.aui32StreamTypeSize[0] = ui32StreamTypeSize;

		/* Position */
		psVertexVariant->u.sVertex.aui32StreamTypeSize[1] = ui32StreamTypeSize;

		psVertexVariant->u.sVertex.ui32NumItemsPerVertex = 2;

		/* Texture coordinates */
		for (i=0; i<gc->ui32NumImageUnitsActive; i++)
		{
			psVertexVariant->u.sVertex.aui32StreamTypeSize[i+2] = ui32StreamTypeSize;

			psVertexVariant->u.sVertex.ui32NumItemsPerVertex++;
		}
	}

#if defined(FIX_HW_BRN_31988)
	if(!ui32NumInstructions)
	{
		bNeedNOPForEndFlag = IMG_TRUE;
	}
	ui32NumInstructions += 4;
#endif

#if defined(SGX_FEATURE_USE_VEC34)
	if(bNeedNOPForEndFlag)
	{
		ui32NumInstructions++;
	}
#endif

	ui32CodeSizeInBytes = ui32NumInstructions * EURASIA_USE_INSTRUCTION_SIZE;

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)

	if (ui32CodeSizeInBytes)
	{
	    /* Count PHAS instruction */
	    ui32NumInstructions++;

		ui32CodeSizeInBytes += EURASIA_USE_INSTRUCTION_SIZE;
	} 
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */


	/* Align to exe address boundary */
	ui32AlignSize = EURASIA_PDS_DOUTU_PHASE_START_ALIGN - 1;

	ui32CodeSizeInBytes = (ui32CodeSizeInBytes + ui32AlignSize) & ~ui32AlignSize;

	GLES1_TIME_START(GLES1_TIMER_USECODEHEAP_VERT_TIME);
	
	/* Allocate a single block for unpack and main shader code */
	psVertexVariant->psCodeBlock = UCH_CodeHeapAllocate(gc->psSharedState->psUSEVertexCodeHeap, 
													ui32CodeSizeInBytes + ui32MainCodeSizeInBytes, gc->psSysContext->hPerProcRef);

	GLES1_TIME_STOP(GLES1_TIMER_USECODEHEAP_VERT_TIME);
	
	if(!psVertexVariant->psCodeBlock)
	{
		/* Destroy all vertex variants (will kick and wait for TA) */
		DestroyVertexVariants(gc); 

		GLES1_TIME_START(GLES1_TIMER_USECODEHEAP_VERT_TIME);
		
		/* Allocate a single block for unpack and main shader code */
		psVertexVariant->psCodeBlock = UCH_CodeHeapAllocate(gc->psSharedState->psUSEVertexCodeHeap, 
														ui32CodeSizeInBytes + ui32MainCodeSizeInBytes, gc->psSysContext->hPerProcRef);

		GLES1_TIME_STOP(GLES1_TIMER_USECODEHEAP_VERT_TIME);

		if(!psVertexVariant->psCodeBlock)
		{
			return GLES1_TA_USECODE_ERROR;
		}
	}

	GLES1_INC_COUNT(GLES1_TIMER_USECODEHEAP_VERT_COUNT, (ui32CodeSizeInBytes + ui32MainCodeSizeInBytes)>>2);

	if(ui32CodeSizeInBytes)
	{
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
		IMG_UINT32 ui32NextPhaseMode, ui32NextPhaseAddress;
#endif
		/* Setup 1st execution address */ 
		psVertexVariant->sStartAddress[0] = psVertexVariant->psCodeBlock->sCodeAddress;
		pui32Code = psVertexVariant->psCodeBlock->pui32LinAddress;

		/* Setup 2nd execution address */ 
		psVertexVariant->sStartAddress[1].uiAddr = psVertexVariant->psCodeBlock->sCodeAddress.uiAddr + ui32CodeSizeInBytes;

		*ppui32Instruction = (IMG_UINT32 *)((IMG_UINTPTR_T)psVertexVariant->psCodeBlock->pui32LinAddress + ui32CodeSizeInBytes);

		psVertexVariant->ui32MaxTempRegs = MAX(psVertexVariant->ui32MaxTempRegs, ui32NumTemps);
		
		psVertexVariant->ui32PhaseCount++;

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)

#if defined(SGX_FEATURE_VCB)
		/* Setup 3rd execution address */ 
		psVertexVariant->sStartAddress[2].uiAddr = ( psVertexVariant->psCodeBlock->sCodeAddress.uiAddr 
													 + ui32CodeSizeInBytes
													 + ui32MainCodeSizeInBytes
													 - (2 * EURASIA_USE_INSTRUCTION_SIZE) );
#endif /* defined(SGX_FEATURE_VCB) */

		if(bUSEPerInstanceMode)
		{
			ui32NextPhaseMode = EURASIA_USE_OTHER2_PHAS_MODE_PERINSTANCE;
		}
		else
		{
			ui32NextPhaseMode = EURASIA_USE_OTHER2_PHAS_MODE_PARALLEL;
		}

		ui32NumTemps = ALIGNCOUNTINBLOCKS(psVertexVariant->ui32MaxTempRegs, EURASIA_USE_OTHER2_PHAS_TCOUNT_ALIGNSHIFT);
	
		ui32NextPhaseAddress = GetUSEPhaseAddress(psVertexVariant->sStartAddress[1],
												gc->psSysContext->uUSEVertexHeapBase, 
												SGX_VTXSHADER_USE_CODE_BASE_INDEX);
		
		BuildPHASImmediate((PPVR_USE_INST)	pui32Code,
											0, /* ui32End */
											ui32NextPhaseMode,
											EURASIA_USE_OTHER2_PHAS_RATE_PIXEL,
											EURASIA_USE_OTHER2_PHAS_WAITCOND_NONE,
											ui32NumTemps,
											ui32NextPhaseAddress);
									
		ui32NumInstructions = 1;
#else 
		ui32NumInstructions = 0;
		PVR_UNREFERENCED_PARAMETER(bUSEPerInstanceMode);
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */		

	}
	else
	{
		psVertexVariant->sStartAddress[0].uiAddr = psVertexVariant->psCodeBlock->sCodeAddress.uiAddr;
		*ppui32Instruction = psVertexVariant->psCodeBlock->pui32LinAddress;

#if defined(SGX_FEATURE_VCB)
		/* Setup VCB execution address */ 
		psVertexVariant->sStartAddress[2].uiAddr = ( psVertexVariant->psCodeBlock->sCodeAddress.uiAddr 
													 + ui32MainCodeSizeInBytes
													 - (2 * EURASIA_USE_INSTRUCTION_SIZE) );
#endif

		/* ui32NumInstructions = 0; */

		return GLES1_NO_ERROR;
	}

#if defined(FIX_HW_BRN_31988)
	USEGenWriteBRN31988Fragment(&pui32Code[ui32NumInstructions * 2]);

	ui32NumInstructions +=4;

#endif

	if (gc->sPrim.eCurrentPrimitiveType!=GLES1_PRIMTYPE_DRAWTEXTURE)
	{
		/* Setup copies for all of the streams */
		for (i = 0; i < psVertexVariant->u.sVertex.ui32NumItemsPerVertex; i++)
		{
			GLES1AttribArrayPointerMachine *psAttribPointer;
			IMG_UINT32 ui32CurrentAttrib, ui32Size, ui32Type, ui32ComponentSize;
			IMG_BOOL bNormalized, bWorkAroundBRN22509;
			VSInputReg *pasVSInputRegisters  = &gc->sProgram.psCurrentVertexShader->asVSInputRegisters[0];
			IMG_UINT32 ui32PrimaryAttrReg, ui32RequiredSize;

			psAttribPointer = gc->sVAOMachine.apsPackedAttrib[i];
			ui32CurrentAttrib = (IMG_UINT32)(psAttribPointer - &(gc->sVAOMachine.asAttribPointer[0]));

			ui32Size = (psAttribPointer->ui32CopyStreamTypeSize >> GLES1_STREAMSIZE_SHIFT);
			ui32Type = (psAttribPointer->ui32CopyStreamTypeSize & GLES1_STREAMTYPE_MASK);
			ui32ComponentSize = aui32AttribSize[ui32Type];
			bNormalized = (psAttribPointer->ui32CopyStreamTypeSize & GLES1_STREAMNORM_BIT) != 0 ? IMG_TRUE : IMG_FALSE;
			bWorkAroundBRN22509 = (bNormalized &&
								((ui32Type==GLES1_STREAMTYPE_BYTE) || (ui32Type==GLES1_STREAMTYPE_SHORT))) ? IMG_TRUE : IMG_FALSE;

			ui32PrimaryAttrReg = pasVSInputRegisters[ui32CurrentAttrib].ui32PrimaryAttribute;
			ui32RequiredSize   = pasVSInputRegisters[ui32CurrentAttrib].ui32Size;

			switch(ui32Type)
			{
				case GLES1_STREAMTYPE_FLOAT:
				{
					break;
				}
				/* If the stream is not float type - it needs to be unpacked to float */
				case GLES1_STREAMTYPE_BYTE:
				case GLES1_STREAMTYPE_SHORT:
				case GLES1_STREAMTYPE_UBYTE:
				case GLES1_STREAMTYPE_USHORT:
				case GLES1_STREAMTYPE_HALFFLOAT:
				{
					/* Unpack the src registers */
					for (j = 0; j < ui32Size; j++)
					{
						/* Unpack in reverse so that the source data isn't overwritten until the end */
						IMG_UINT32 k = ui32Size - 1 - j;
						IMG_UINT32 ui32SrcRegister = ui32PrimaryAttrReg + ((k * ui32ComponentSize) >> 2);
						IMG_UINT32 ui32SrcComponent = (k * ui32ComponentSize) % 4;


						/* unpckf32?? pa[paReg + i], pa[paReg].i */
						BuildUNPCKF32((PPVR_USE_INST) &pui32Code[ui32NumInstructions * 2],
										EURASIA_USE1_EPRED_ALWAYS, 0,
										EURASIA_USE1_RCNTSEL,
										aui32StreamTypeToUseType[ui32Type],
										((bNormalized && !bWorkAroundBRN22509)? EURASIA_USE0_PCK_SCALE : 0),
										USE_REGTYPE_PRIMATTR,
										ui32PrimaryAttrReg + k,
										USE_REGTYPE_PRIMATTR,
										ui32SrcRegister, 
										ui32SrcComponent);

						ui32NumInstructions++;

						if(bWorkAroundBRN22509)
						{
							/* Both unpckf32s8.scale and unpckf32s16.scale perform normalization as D3D requires
							rather than as OpenGL. Instead we do by hand the normalization with a multiply-add.
							See table "Table 2.9: Component conversions" of the OpenGL 2.0 spec.
						*/
							IMG_UINT32 ui32MADSrc1, ui32MADSrc2;

							if(ui32Type == GLES1_STREAMTYPE_BYTE)
							{
								ui32MADSrc1 = GLES1_VERTEX_SECATTR_UNPCKS8_CONST1;
								ui32MADSrc2 = GLES1_VERTEX_SECATTR_UNPCKS8_CONST2;
							}
							else
							{
								GLES1_ASSERT(ui32Type == GLES1_STREAMTYPE_SHORT);
								ui32MADSrc1 = GLES1_VERTEX_SECATTR_UNPCKS16_CONST1;
								ui32MADSrc2 = GLES1_VERTEX_SECATTR_UNPCKS16_CONST2;
							}

							/* fmad.skipinv pa[k], pa[k], sa[const1], sa[const2] */
							/* that is, pa[k] = pa[k]*sa[const1] + sa[const2] */
							BuildMAD((PPVR_USE_INST) &pui32Code[ui32NumInstructions * 2],
													EURASIA_USE1_EPRED_ALWAYS,
													0,
													EURASIA_USE1_RCNTSEL,
													USE_REGTYPE_PRIMATTR,
													ui32PrimaryAttrReg + k,
													USE_REGTYPE_PRIMATTR,
													ui32PrimaryAttrReg + k,
													EURASIA_USE_SRCMOD_NONE,
													USE_REGTYPE_SECATTR,
													ui32MADSrc1,
													EURASIA_USE_SRCMOD_NONE,
													USE_REGTYPE_SECATTR,
													ui32MADSrc2,
													EURASIA_USE_SRCMOD_NONE);

							ui32NumInstructions++;
						}
					}

					break;
				}
				case GLES1_STREAMTYPE_FIXED:
				{
					/* Unpack the src registers */
					for (j = 0; j < ui32Size; j++)
					{
						/* Unpack in reverse so that the source data isn't overwritten until the end */
						IMG_UINT32 k = ui32Size - 1 - j;
						/* Unpack high 16 bits first */
						IMG_UINT32 ui32SrcRegister = ui32PrimaryAttrReg + k;
						IMG_UINT32 ui32SrcComponent = 2;

						/* unpckf32s8 tmp0, pa[paReg].i */
						BuildUNPCKF32((PPVR_USE_INST) &pui32Code[ui32NumInstructions * 2],
										EURASIA_USE1_EPRED_ALWAYS, 0,
										EURASIA_USE1_RCNTSEL,
										EURASIA_USE1_PCK_FMT_S16,
										0,
										USE_REGTYPE_TEMP,
										0,
										USE_REGTYPE_PRIMATTR,
										ui32SrcRegister, 
										ui32SrcComponent);

						ui32NumInstructions++;

						/* Now unpack low 16 bits */
						ui32SrcComponent = 0;

						/* unpckf32u8 pa[paReg + i], pa[paReg].i */
						BuildUNPCKF32((PPVR_USE_INST) &pui32Code[ui32NumInstructions * 2],
										EURASIA_USE1_EPRED_ALWAYS, 0,
										EURASIA_USE1_RCNTSEL,
										EURASIA_USE1_PCK_FMT_U16,
										0,
										USE_REGTYPE_PRIMATTR,
										ui32PrimaryAttrReg + k,
										USE_REGTYPE_PRIMATTR,
										ui32SrcRegister, 
										ui32SrcComponent);

						ui32NumInstructions++;

						/* mad pa[paReg + i], pa[paReg + i].i, 1/65536, tmp0 */
						BuildMAD((PPVR_USE_INST) &pui32Code[ui32NumInstructions * 2],
												EURASIA_USE1_EPRED_ALWAYS,
												0,
												EURASIA_USE1_RCNTSEL,
												USE_REGTYPE_PRIMATTR,
												ui32PrimaryAttrReg + k,
												USE_REGTYPE_PRIMATTR,
												ui32SrcRegister,
												EURASIA_USE_SRCMOD_NONE,
												USE_REGTYPE_SPECIAL,
												EURASIA_USE_SPECIAL_CONSTANT_FLOAT1OVER65536,
												EURASIA_USE_SRCMOD_NONE,
												USE_REGTYPE_TEMP,
												0,
												EURASIA_USE_SRCMOD_NONE);

						ui32NumInstructions++;
					}
					break;
				}
				default:
				{
					PVR_DPF((PVR_DBG_ERROR,"CreateVertexUnpackUSECode: Unknown stream-type"));

					break;
				}
			}

			/* Pad the remaining elements with 0's and 1's */
			for (j=ui32Size; j < ui32RequiredSize; j++)
			{	
				static const IMG_FLOAT afVals[] = {0.0f, 0.0f, 0.0f, 1.0f};
				GLES1_FUINT32 uTemp;      

				if(ui32CurrentAttrib==AP_WEIGHTARRAY)
				{
					/* 
					** If the matrix index array is bigger than the weight
					** array then pad the weight array with 0's
					*/
					uTemp.fVal = 0.0f;
				}
				else
				{
					uTemp.fVal = afVals[j];
				}

				/* limm pa[paReg + i], #1.0f or #0.0f */
				BuildLIMM((PPVR_USE_INST)&pui32Code[ui32NumInstructions * 2],
										EURASIA_USE1_EPRED_ALWAYS,
										0,
										USE_REGTYPE_PRIMATTR,
										ui32PrimaryAttrReg + j,
										uTemp.ui32Val);
				ui32NumInstructions++;
			}
		}
	}
	

#if defined(SGX_FEATURE_USE_VEC34)
	if(bNeedNOPForEndFlag)
	{
		BuildNOP((PPVR_USE_INST) &pui32Code[ui32NumInstructions * 2], 0, IMG_FALSE);
		ui32NumInstructions++;
	}
#endif

	/* Terminate this block of code */
	pui32Code[(ui32NumInstructions * 2) - 1] |= EURASIA_USE1_END;	/* PRQA S 3382 */ /* No wraparound in this context. */

	return GLES1_NO_ERROR;
}

/****************************************************************
 * Function Name  	: SetupUSEVertexShader
 * Returns        	: Success/Failure
 * Globals Used    	: 
 * Description    	: 
 ****************************************************************/
IMG_INTERNAL GLES1_MEMERROR SetupUSEVertexShader(GLES1Context *gc, IMG_BOOL *pbProgramChanged)
{
	GLES1Shader *psVertexShader = gc->sProgram.psCurrentVertexShader;
	GLES1ShaderVariant *psVertexVariant = psVertexShader->psShaderVariant;
	FFGEN_PROGRAM_DETAILS *psHWCode = psVertexShader->psFFGENProgramDetails;
	IMG_UINT32 i, ui32InstSizeInBytes, ui32CodeSizeInBytes;
	IMG_UINT32 *pui32Instruction;
	GLES1_MEMERROR eError;
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
#if defined(SGX_FEATURE_VCB)
	IMG_UINT32 ui32AlignSize, ui32NextPhaseAddress, ui32NextPhaseTemps;
#endif /* defined(SGX_FEATURE_VCB) */
	IMG_UINT32 *pui32InstructionBase;
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

	GLES1VertexArrayObjectMachine *psVAOMachine = &(gc->sVAOMachine);

	GLES1_ASSERT(VAO(gc));


	if(gc->sPrim.eCurrentPrimitiveType!=GLES1_PRIMTYPE_DRAWTEXTURE)
	{
		while(psVertexVariant)
		{
		    if(psVertexVariant->u.sVertex.ui32NumItemsPerVertex == psVAOMachine->ui32NumItemsPerVertex)
			{
				IMG_BOOL bMatch = IMG_TRUE;
				i = 0;

				while((i < psVertexVariant->u.sVertex.ui32NumItemsPerVertex) && bMatch)
				{
				    GLES1AttribArrayPointerMachine *psAttribPointer = psVAOMachine->apsPackedAttrib[i];

					if(psVertexVariant->u.sVertex.aui32StreamTypeSize[i] != psAttribPointer->ui32CopyStreamTypeSize)
					{
						bMatch = IMG_FALSE;
					}
					else
					{
						i++;
					}
					
				}

				if(bMatch)
				{
					break;
				}
			}

			/* Check the next vertex variant */
			psVertexVariant = psVertexVariant->psNext;
		}
	}
	else
	{
		IMG_UINT32 ui32NumItemsPerVertex;

		ui32NumItemsPerVertex = 2 + gc->ui32NumImageUnitsActive;

		while(psVertexVariant)
		{
			if(psVertexVariant->u.sVertex.ui32NumItemsPerVertex == ui32NumItemsPerVertex)
			{
				IMG_BOOL bMatch = IMG_TRUE;
				i = 0;

				while((i < psVertexVariant->u.sVertex.ui32NumItemsPerVertex) && bMatch)
				{
					if(psVertexVariant->u.sVertex.aui32StreamTypeSize[i] != (GLES1_STREAMTYPE_FLOAT | (4 << GLES1_STREAMSIZE_SHIFT)))
					{
						bMatch = IMG_FALSE;
					}
					else
					{
						i++;
					}
				}

				if(bMatch)
				{
					break;
				}
			}

			psVertexVariant = psVertexVariant->psNext;
		}
	}

	if(!psVertexVariant)
	{
		psVertexVariant = GLES1Calloc(gc, sizeof(GLES1ShaderVariant));
		
		if(!psVertexVariant)
		{
			return GLES1_HOST_MEM_ERROR;
		}


		ui32InstSizeInBytes = psHWCode->ui32InstructionCount * EURASIA_USE_INSTRUCTION_SIZE;

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
#if defined(SGX_FEATURE_VCB)

		/* + 1 PHAS + 1 EMITVCB */
		ui32CodeSizeInBytes = ui32InstSizeInBytes + (2 * EURASIA_USE_INSTRUCTION_SIZE);

		/*Align to exe address boundary for the current phase */
		ui32AlignSize = EURASIA_PDS_DOUTU_PHASE_START_ALIGN - 1;
		ui32CodeSizeInBytes = (ui32CodeSizeInBytes + ui32AlignSize) & ~ui32AlignSize;

		/* + 1 PHAS + 1 EMITVERTEX */
		ui32CodeSizeInBytes += 2 * EURASIA_USE_INSTRUCTION_SIZE;

#else /* defined(SGX_FEATURE_VCB) */

		/* + 1 PHAS + 1 EMITVERTEX */
		ui32CodeSizeInBytes = ui32InstSizeInBytes + (2 * EURASIA_USE_INSTRUCTION_SIZE);

#endif /* defined(SGX_FEATURE_VCB) */

#else /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

#if defined(FFGEN_UNIFLEX)
		ui32CodeSizeInBytes = ui32InstSizeInBytes + (1*EURASIA_USE_INSTRUCTION_SIZE);
#else
		ui32CodeSizeInBytes = ui32InstSizeInBytes;
#endif /* defined(FFGEN_UNIFLEX) */

#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */		

		
		/* Update temporary register count */
		psVertexVariant->ui32MaxTempRegs = MAX(psVertexVariant->ui32MaxTempRegs, psHWCode->ui32TemporaryRegisterCount);
		psVertexVariant->ui32MaxTempRegs = MAX(psVertexVariant->ui32MaxTempRegs, SGX_USE_MINTEMPREGS);

		eError = CreateVertexUnpackUSECode(gc, psVertexVariant, ui32CodeSizeInBytes, psHWCode->bUSEPerInstanceMode ? IMG_TRUE : IMG_FALSE, &pui32Instruction);

		if(eError != GLES1_NO_ERROR)
		{
			GLES1Free(IMG_NULL, psVertexVariant);

			return eError;
		}

		/* Insert PHAS instruction before copy HWCode to USEVertexShaderHeap */

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)

		pui32InstructionBase = pui32Instruction;

#if defined(SGX_FEATURE_VCB)


		ui32NextPhaseAddress = GetUSEPhaseAddress(psVertexVariant->sStartAddress[2],
												gc->psSysContext->uUSEVertexHeapBase, 
												SGX_VTXSHADER_USE_CODE_BASE_INDEX);

		ui32NextPhaseTemps = ALIGNCOUNTINBLOCKS(psVertexVariant->ui32MaxTempRegs, EURASIA_USE_OTHER2_PHAS_TCOUNT_ALIGNSHIFT);
		
		/* Insert PHAS */
		BuildPHASImmediate((PPVR_USE_INST)	pui32InstructionBase,
											0, /* ui32End */
											EURASIA_USE_OTHER2_PHAS_MODE_PARALLEL,
											EURASIA_USE_OTHER2_PHAS_RATE_PIXEL,
											EURASIA_USE_OTHER2_PHAS_WAITCOND_VCULL,
											ui32NextPhaseTemps,
											ui32NextPhaseAddress);
							
		pui32Instruction += USE_INST_LENGTH;

#else

		BuildPHASLastPhase ((PPVR_USE_INST) pui32InstructionBase, 0);
		pui32Instruction += USE_INST_LENGTH;
		
#endif /* defined(SGX_FEATURE_VCB) */
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */		


		GLES1MemCopy(pui32Instruction, psHWCode->pui32Instructions, ui32InstSizeInBytes);
	
		/* Emit VCB/MTE after loading HWCode to USEVertexShaderHeap */

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)

		pui32Instruction += (ui32InstSizeInBytes >> 2);
		
#if defined(SGX_FEATURE_VCB)

		/* EMIT VCB */
		pui32Instruction[0] =	(EURASIA_USE0_S1EXTBANK_IMMEDIATE << EURASIA_USE0_S1BANK_SHIFT) |
								(EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT) |
								(0								  << EURASIA_USE0_SRC2_SHIFT);

		pui32Instruction[1] = 
				((EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT)                            |
				 (EURASIA_USE1_OTHER_OP2_EMIT << EURASIA_USE1_OTHER_OP2_SHIFT)                 |
				 (EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT)        |
				 (EURASIA_USE1_S1BEXT)                                                         |  
				 (EURASIA_USE1_S2BEXT)                                                         |  
				 (EURASIA_USE1_S0STDBANK_TEMP << EURASIA_USE1_S0BANK_SHIFT)                    | 				 
				 (SGX545_USE1_EMIT_TARGET_VCB << EURASIA_USE1_EMIT_TARGET_SHIFT)               |
				 (SGX545_USE1_EMIT_VCB_EMITTYPE_VERTEX << SGX545_USE1_EMIT_VCB_EMITTYPE_SHIFT) |
				 (SGX545_USE1_EMIT_VERTEX_TWOPART)                                             |
				 (0 << EURASIA_USE1_EMIT_INCP_SHIFT)                                           |
				 EURASIA_USE1_END)                                                             ;		
		

		psVertexVariant->ui32PhaseCount++;

		pui32Instruction = (IMG_UINT32 *)( (IMG_UINT8 *)psVertexVariant->psCodeBlock->pui32LinAddress 
										   + (psVertexVariant->sStartAddress[2].uiAddr - psVertexVariant->sStartAddress[0].uiAddr) );

		pui32InstructionBase = pui32Instruction;

		BuildPHASLastPhase ((PPVR_USE_INST) pui32InstructionBase, 0);
		pui32Instruction += USE_INST_LENGTH;


#endif /* defined(SGX_FEATURE_VCB) */

		/* EMIT MTE */
		pui32Instruction[0] = (EURASIA_USE0_S1EXTBANK_IMMEDIATE << EURASIA_USE0_S1BANK_SHIFT)  |
							   (EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT) |
							   (0								 << EURASIA_USE0_SRC2_SHIFT)   |
								EURASIA_USE0_EMIT_FREEP;

		pui32Instruction[1] = 
				((EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT)                            |
				 (EURASIA_USE1_OTHER_OP2_EMIT << EURASIA_USE1_OTHER_OP2_SHIFT)                 |
				 (EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT)        |
				 (EURASIA_USE1_S1BEXT)                                                         |  
				 (EURASIA_USE1_S2BEXT)                                                         |  
				 (EURASIA_USE1_S0STDBANK_TEMP << EURASIA_USE1_S0BANK_SHIFT)                    | 				 
				 (EURASIA_USE1_EMIT_TARGET_MTE << EURASIA_USE1_EMIT_TARGET_SHIFT)              |
				 (EURASIA_USE1_EMIT_MTECTRL_VERTEX << EURASIA_USE1_EMIT_MTECTRL_SHIFT)	       |
#if defined(SGX_FEATURE_VCB)
				 (SGX545_USE1_EMIT_VERTEX_TWOPART)                                             |
#endif
				 (0 << EURASIA_USE1_EMIT_INCP_SHIFT)                                           |
				 EURASIA_USE1_END);
		
#else /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */		

#if defined(FFGEN_UNIFLEX)

		pui32Instruction = (IMG_UINT32 *) ((IMG_UINTPTR_T)pui32Instruction + ui32InstSizeInBytes);

		{
			/* Add EMITVTX instruction */
			pui32Instruction[0] =	(EURASIA_USE0_S1EXTBANK_IMMEDIATE << EURASIA_USE0_S1BANK_SHIFT) |
									(EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT) |
									(0								  << EURASIA_USE0_SRC2_SHIFT)	|
									EURASIA_USE0_EMIT_FREEP;

			pui32Instruction[1] = 
					((EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
					(EURASIA_USE1_OTHER_OP2_EMIT << EURASIA_USE1_OTHER_OP2_SHIFT) |
					(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
					(EURASIA_USE1_S1BEXT) |  
					(EURASIA_USE1_S2BEXT) |  
					(EURASIA_USE1_S0STDBANK_TEMP << EURASIA_USE1_S0BANK_SHIFT) | 				 
					(EURASIA_USE1_EMIT_TARGET_MTE << EURASIA_USE1_EMIT_TARGET_SHIFT) |
					(EURASIA_USE1_EMIT_MTECTRL_VERTEX << EURASIA_USE1_EMIT_MTECTRL_SHIFT) | 
					(0 << EURASIA_USE1_EMIT_INCP_SHIFT) |
					EURASIA_USE1_END);
		}


#endif /* defined(FFGEN_UNIFLEX) */

#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */		


		psVertexVariant->ui32PhaseCount++;

		/* Setup use mode of operation */
		if(psHWCode->bUSEPerInstanceMode)
		{
			psVertexShader->ui32USEMode = EURASIA_PDS_DOUTU1_MODE_PERINSTANCE;
		}
		else
		{
			psVertexShader->ui32USEMode = EURASIA_PDS_DOUTU1_MODE_PARALLEL;
		}

		/* Add to variant list */
		psVertexVariant->psNext = psVertexShader->psShaderVariant;
		psVertexShader->psShaderVariant = psVertexVariant;
	
		/* And back link it to the shader */
		psVertexVariant->psShader = psVertexShader;
	}

	/* Set current variant */
	if(gc->sProgram.psCurrentVertexVariant!=psVertexVariant)
	{
		gc->sProgram.psCurrentVertexVariant = psVertexVariant;

		*pbProgramChanged = IMG_TRUE;
	}
	else
	{
		*pbProgramChanged = IMG_FALSE;
	}

	return GLES1_NO_ERROR;
}


/*****************************************************************************
 Function Name	: SetupIteratorsAndTextureLookups
 Inputs			: gc, ui32NumNonDependentTextureLoads, 
				  psTextureLoads, bIteratePlaneCoefficients 
 Outputs		: puPlaneCoeffPAReg
 Returns		: -
 Description	: Sets up iterators and non-dependent texture loads for fragment shader
*****************************************************************************/
static IMG_VOID SetupIteratorsAndTextureLookups(GLES1Context 		 *gc,
												IMG_UINT32 			  ui32NumNonDependentTextureLoads,
												GLES1_TEXTURE_LOAD   *psTextureLoads,
												IMG_UINT32			 *pui32FogPAReg,
												IMG_UINT32 			 *pui32PlaneCoeffPAReg
#if defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728)
												,IMG_UINT32 			 *pui32PositionPAReg
#endif /* defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728) */
												)
{
#if defined(SGX_FEATURE_CEM_S_USES_PROJ)
	PDS_TEXTURE_IMAGE_UNIT *psTextureImageUnit = gc->sPrim.sTextureState.asTextureImageUnits;
#endif
	GLES1ShaderVariant *psFragmentVariant = gc->sProgram.psCurrentFragmentVariant;
	GLES1PDSInfo *psPDSInfo  = &psFragmentVariant->u.sFragment.sPDSInfo;
	IMG_UINT32  i;
	IMG_UINT32 *pui32TSPParamFetchInterface;
	IMG_UINT32 *pui32LayerControl           = &psPDSInfo->aui32LayerControl[0];

	IMG_UINT32 ui32DMSInfoTextureSize, ui32DMSInfoIterationSize;
	IMG_UINT32 ui32LastTexIssue, ui32LastUSEIssue, ui32DependencyControl;

	/* Clear existing hardware state. */
	ui32DMSInfoTextureSize = 0;
	ui32DMSInfoIterationSize = 0;
	ui32DependencyControl = 0;

#if defined(SGX_FEATURE_ALPHATEST_SECONDARY_PERPRIMITIVE) || defined(SGX_FEATURE_ALPHATEST_AUTO_COEFF)
	PVR_UNREFERENCED_PARAMETER(pui32PlaneCoeffPAReg);
#endif /* defined(SGX_FEATURE_ALPHATEST_SECONDARY_PERPRIMITIVE) || defined(SGX_FEATURE_ALPHATEST_AUTO_COEFF) */

	/* Clear coordinate masks. */
#if defined(SGX545)
	pui32TSPParamFetchInterface = &psPDSInfo->aui32TSPParamFetchInterface0[0];
#else 
	pui32TSPParamFetchInterface = &psPDSInfo->aui32TSPParamFetchInterface[0];
#endif /* defined(SGX545) */

	ui32LastTexIssue = ui32LastUSEIssue = 0xFFFFFFFF;

	for (i=0; i<ui32NumNonDependentTextureLoads; i++)
	{
		IMG_UINT32 ui32TextureUnit	  = psTextureLoads[i].ui32Texture;
		IMG_UINT32 ui32InputCoordUnit = psTextureLoads[i].eCoordinate;

#if defined(SGX545)
		psPDSInfo->aui32TSPParamFetchInterface1[i] = 0;
#endif

		/* Is this iteration command for the USE or the TAG? */
		if (ui32TextureUnit == GLES1_TEXTURE_NONE)
		{
#if defined(FIX_HW_BRN_25211)
			if(gc->sAppHints.bUseC10Colours)
			{
				/* Setup the data we want to iterate - the values in uCoordUnit 
				   match the EURASIA_PDS_DOUTI_USEISSUE_* defines. */
				pui32TSPParamFetchInterface[i] = ((ui32InputCoordUnit + (EURASIA_PDS_DOUTI_TEXISSUE_TC0 >> EURASIA_PDS_DOUTI_TEXISSUE_SHIFT)) << EURASIA_PDS_DOUTI_USEISSUE_SHIFT) |
												EURASIA_PDS_DOUTI_TEXISSUE_NONE;

				/* Always perspective correct iteration to the USE */
				pui32TSPParamFetchInterface[i] |= EURASIA_PDS_DOUTI_USEPERSPECTIVE;

				/* No wrapping */
				pui32TSPParamFetchInterface[i] |= (0 << EURASIA_PDS_DOUTI_USEWRAP_SHIFT);

				/* How many primary attribute registers does this load consume?	*/

				pui32TSPParamFetchInterface[i] |= (psTextureLoads[i].ui32CoordinateDimension - 1) << EURASIA_PDS_DOUTI_USEDIM_SHIFT;

				ui32DMSInfoIterationSize += 2;

				pui32TSPParamFetchInterface[i] |= EURASIA_PDS_DOUTI_USECOLFLOAT;

#if defined(SGX_FEATURE_EXTENDED_USE_ALU)
				/*  Format of Iterated data sent to the USE.

					The USE_FORMAT is only honoured when
					the USE_ISSUE is a floating point/integer colour or a texture coordinate set
					ie Fog and position data are always sent as F32.  When the USE_FORMAT is "00",
					in order to remain backwards compatible with SGX535, the iterators output in
					INT8 format only when USE_FLOATCOL is 0 and the USE_ISSUE is base or highlight
					colour. F32's are produced in all other cases.
				*/

				pui32TSPParamFetchInterface[i] |= EURASIA_PDS_DOUTI_USEFORMAT_INT10;
#endif
				/* Record this as the last USE issue */
				ui32LastUSEIssue = i;

				pui32LayerControl[i] = (IMG_UINT32)-1;
			}
			else
#endif /* defined(FIX_HW_BRN_25211) */
			{
				/* How many primary attribute registers does this load consume?	*/
				GLES1_ASSERT((ui32InputCoordUnit==GLES1_TEXTURE_COORDINATE_V0) || 
							(ui32InputCoordUnit==GLES1_TEXTURE_COORDINATE_V1));

				GLES1_ASSERT(psTextureLoads[i].eFormat == GLES1_TEXLOAD_FORMAT_U8);

				/* Setup the data we want to iterate - the values in uCoordUnit 
				   match the EURASIA_PDS_DOUTI_USEISSUE_* defines. */
				if (ui32InputCoordUnit==GLES1_TEXTURE_COORDINATE_V0)
				{
					pui32TSPParamFetchInterface[i] = EURASIA_PDS_DOUTI_USEISSUE_V0   |
				                                     EURASIA_PDS_DOUTI_TEXISSUE_NONE ;
				}
				else
				{
					pui32TSPParamFetchInterface[i] = EURASIA_PDS_DOUTI_USEISSUE_V1   |
				                                     EURASIA_PDS_DOUTI_TEXISSUE_NONE ;
				}

				/* Always perspective correct iteration to the USE */
				pui32TSPParamFetchInterface[i] |= EURASIA_PDS_DOUTI_USEPERSPECTIVE;

				/* No wrapping */
				pui32TSPParamFetchInterface[i] |= (0 << EURASIA_PDS_DOUTI_USEWRAP_SHIFT);

				/* Integer colour is 4x8-bit packed into a 32-bit word */
				ui32DMSInfoIterationSize += 1;
				pui32TSPParamFetchInterface[i] |= EURASIA_PDS_DOUTI_USEDIM_4D;				

#if defined(SGX_FEATURE_EXTENDED_USE_ALU)
				/*  Format of Iterated data sent to the USE.

					The USE_FORMAT is only honoured when
					the USE_ISSUE is a floating point/integer colour or a texture coordinate set
					ie Fog and position data are always sent as F32.  When the USE_FORMAT is "00",
					in order to remain backwards compatible with SGX535, the iterators output in
					INT8 format only when USE_FLOATCOL is 0 and the USE_ISSUE is base or highlight
					colour. F32's are produced in all other cases.
				*/
				pui32TSPParamFetchInterface[i] |= EURASIA_PDS_DOUTI_USEFORMAT_INT8;
#endif
				/* Record this as the last USE issue */
				ui32LastUSEIssue = i;

				pui32LayerControl[i] = 0xFFFFFFFF;
			}
		}
		else
		{
			IMG_BOOL bSpriteOverride = IMG_FALSE;

			if (gc->sPrim.eCurrentPrimitiveType == GLES1_PRIMTYPE_SPRITE)
			{
				if (ui32InputCoordUnit < GLES1_MAX_TEXTURE_UNITS && gc->sState.sTexture.asUnit[ui32InputCoordUnit].sEnv.bPointSpriteReplace)
				{
					ui32InputCoordUnit = 0;
					bSpriteOverride = IMG_TRUE;
				}
				else
				{
					ui32InputCoordUnit++;
				}
			}

			/* Setup the data we want to iterate - the values in uCoordUnit 
			   match the EURASIA_PDS_DOUTI_TEXISSUE_* defines */
			pui32TSPParamFetchInterface[i] = (((ui32InputCoordUnit + (EURASIA_PDS_DOUTI_TEXISSUE_TC0 >> EURASIA_PDS_DOUTI_TEXISSUE_SHIFT))
											   << EURASIA_PDS_DOUTI_TEXISSUE_SHIFT) 
											  |   EURASIA_PDS_DOUTI_USEISSUE_NONE );

			/* No wrapping */
			pui32TSPParamFetchInterface[i] |= (0 << EURASIA_PDS_DOUTI_TEXWRAP_SHIFT);

			ui32DMSInfoTextureSize++;

#if defined(SGX_FEATURE_CEM_S_USES_PROJ)
			/*
				Force 3rd coordinate to be iterated for CEM. 
			*/
			if((psTextureImageUnit[ui32TextureUnit].ui32TAGControlWord1 & ~EURASIA_PDS_DOUTT1_TEXTYPE_CLRMSK) ==
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
				EURASIA_PDS_DOUTT1_TEXTYPE_CEM)
#else
				EURASIA_PDS_DOUTT1_TEXTYPE_ARB_CEM)
#endif
			{
				pui32TSPParamFetchInterface[i] &= EURASIA_PDS_DOUTI_TEXPROJ_CLRMSK;
				pui32TSPParamFetchInterface[i] |= EURASIA_PDS_DOUTI_TEXPROJ_S;
			}
			else
#endif	/* #if defined(SGX_FEATURE_CEM_S_USES_PROJ) */		
			/* Do we want projection or perspective correct iteration? */
			if (psTextureLoads[i].bProjected && !bSpriteOverride)
			{
				pui32TSPParamFetchInterface[i] |= EURASIA_PDS_DOUTI_TEXPROJ_T;
			}
			else
			{
				pui32TSPParamFetchInterface[i] |= EURASIA_PDS_DOUTI_TEXPROJ_RHW;
			}

#if defined(SGX_FEATURE_PERLAYER_POINTCOORD)
			if (bSpriteOverride)
			{
#if defined(SGX545)
				psPDSInfo->aui32TSPParamFetchInterface1[i] |= EURASIA_PDS_DOUTI1_TPOINTSPRITE_FORCE;
#else
				pui32TSPParamFetchInterface[i] |= EURASIA_PDS_DOUTI_TEX_POINTSPRITE_FORCED;
#endif
			}

#endif

			/* Record this as the last texture issue */
			ui32LastTexIssue = i;

			pui32LayerControl[i] = ui32TextureUnit;

			psPDSInfo->ui32NonDependentImageUnits |= (1UL << ui32TextureUnit);
		}

		/*
		   Colours are affected by colour-interpolation, texcoords are
		   always gouraud shaded
		*/
		if((gc->sState.sShade.ui32ShadeModel == EURASIA_MTE_SHADE_VERTEX2) && 
		   (ui32TextureUnit == GLES1_TEXTURE_NONE))
		{
			pui32TSPParamFetchInterface[i] |= EURASIA_PDS_DOUTI_FLATSHADE_VTX2;
		}
		else
		{
			pui32TSPParamFetchInterface[i] |= EURASIA_PDS_DOUTI_FLATSHADE_GOURAUD;
		}
	}
	
	if (gc->ui32RasterEnables & GLES1_RS_FOG_ENABLE)
	{
#if defined(SGX545)
		IMG_UINT32 ui32FogTexCoord = gc->ui32NumImageUnitsActive + (EURASIA_PDS_DOUTI_USEISSUE_TC0 >> EURASIA_PDS_DOUTI_USEISSUE_SHIFT);
#endif

		pui32TSPParamFetchInterface[i] |=	EURASIA_PDS_DOUTI_TEXISSUE_NONE							|
#if defined(SGX545)
											(ui32FogTexCoord << EURASIA_PDS_DOUTI_USEISSUE_SHIFT)	|
#else /* defined(SGX545) */
											EURASIA_PDS_DOUTI_USEISSUE_FOG							|
#endif /* defined(SGX545) */
											EURASIA_PDS_DOUTI_FLATSHADE_GOURAUD						|
											EURASIA_PDS_DOUTI_USEPERSPECTIVE						|
											EURASIA_PDS_DOUTI_USEDIM_1D;

		pui32LayerControl[i] = 0xFFFFFFFF;

		/* Record where in the primary attribute registers the fog 
		   will be iterated */
		*pui32FogPAReg = ui32DMSInfoIterationSize + ui32DMSInfoTextureSize;

		/* Iterating 1 value */
		ui32DMSInfoIterationSize += 1;

		/* Record this as the last USE issue */
		ui32LastUSEIssue = i;

		i++;
	}

#if defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728)
    if(gc->sPrim.sRenderState.ui32AlphaTestFlags & GLES1_ALPHA_TEST_BRNFIX_DEPTHF)
    {
#if defined(FIX_HW_BRN_29625)
		*pui32PositionPAReg = 0;
#else
		pui32TSPParamFetchInterface[i] |= EURASIA_PDS_DOUTI_USEISSUE_POSITION	|
										  EURASIA_PDS_DOUTI_TEXISSUE_NONE		|
										  EURASIA_PDS_DOUTI_FLATSHADE_GOURAUD	|
										  EURASIA_PDS_DOUTI_USEPERSPECTIVE		|
										  EURASIA_PDS_DOUTI_USEDIM_3D;

		pui32LayerControl[i] = 0xFFFFFFFF;

		/* Record where in the primary attribute registers the position
		   will be iterated */
		*pui32PositionPAReg = ui32DMSInfoIterationSize + ui32DMSInfoTextureSize;

		/* Iterating 3 values */
		ui32DMSInfoIterationSize += 3;

		/* Record this as the last USE issue */
		ui32LastUSEIssue = i;

		i++;
#endif /* defined(FIX_HW_BRN_29625) */
    }
#endif /* defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728) */

#if !defined(SGX_FEATURE_ALPHATEST_SECONDARY_PERPRIMITIVE) && !defined(SGX_FEATURE_ALPHATEST_AUTO_COEFF)

	/* If plane coefficients are required, they must be the last 
	   iteration command sent to the USE */
	if(gc->sPrim.sRenderState.ui32AlphaTestFlags & GLES1_ALPHA_TEST_FEEDBACK)
	{
		pui32TSPParamFetchInterface[i] |=	EURASIA_PDS_DOUTI_USEISSUE_ZABS		|
											EURASIA_PDS_DOUTI_TEXISSUE_NONE		|
											EURASIA_PDS_DOUTI_FLATSHADE_GOURAUD	|
											EURASIA_PDS_DOUTI_USEPERSPECTIVE	|
											EURASIA_PDS_DOUTI_USEDIM_3D;

		pui32LayerControl[i] = 0xFFFFFFFF;

		/* Record where in the primary attribute registers the plane coefficients 
		   will be iterated */
		*pui32PlaneCoeffPAReg = ui32DMSInfoIterationSize + ui32DMSInfoTextureSize;

		/* Iterating 3 values */
		ui32DMSInfoIterationSize += 3;

#if defined(FIX_HW_BRN_31547)
		if(gc->psRenderSurface->bMultiSample)
		{
			/* Dimension = 4 */
			pui32TSPParamFetchInterface[i] &= EURASIA_PDS_DOUTI_USEDIM_CLRMSK;
			pui32TSPParamFetchInterface[i] |= EURASIA_PDS_DOUTI_USEDIM_4D;

			/* Iterating 4 values */
			ui32DMSInfoIterationSize++;

			/* Skip past per-primitive sample position */
			*pui32PlaneCoeffPAReg++;
		}
#endif /* defined(FIX_HW_BRN_31547) */

		/* Record this as the last USE issue */
		ui32LastUSEIssue = i;

		i++;
	}
#endif


	psPDSInfo->ui32IterationCount = i;

	/*
		Include any primary attribute registers used by the pixel shader in the allocation.
	*/
	if ((ui32DMSInfoTextureSize + ui32DMSInfoIterationSize) < psFragmentVariant->ui32USEPrimAttribCount)
	{
		ui32DMSInfoIterationSize = psFragmentVariant->ui32USEPrimAttribCount - ui32DMSInfoTextureSize;
	}

	if (i > GLES1_MAX_DOUTI)
	{
		PVR_DPF((PVR_DBG_ERROR,"SetupIteratorsAndTextureLookups: exceeded maximum number of iteration commands (SW limit)"));
	}

	if((ui32DMSInfoIterationSize + ui32DMSInfoTextureSize) > 64)
	{
		PVR_DPF((PVR_DBG_ERROR,"SetupIteratorsAndTextureLookups: exceeded maximum number of HW iterated components"));
	}

	/*
		Set the bits that mark the last USE/texture issues
	*/
	if (ui32LastUSEIssue != 0xFFFFFFFF)
	{
		GLES1_ASSERT(psPDSInfo->ui32IterationCount > 0);
		pui32TSPParamFetchInterface[ui32LastUSEIssue] |= EURASIA_PDS_DOUTI_USELASTISSUE;
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
		ui32DependencyControl |= EURASIA_PDS_DOUTU1_ITERATORSDEPENDENCY;
#else 
		ui32DependencyControl |= EURASIA_PDS_DOUTU0_ITERATORSDEPENDENCY;
#endif
	}
	if (ui32LastTexIssue != 0xFFFFFFFF)
	{
		GLES1_ASSERT(psPDSInfo->ui32IterationCount > 0);
		pui32TSPParamFetchInterface[ui32LastTexIssue] |= EURASIA_PDS_DOUTI_TEXLASTISSUE;
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
		ui32DependencyControl |= EURASIA_PDS_DOUTU1_TEXTUREDEPENDENCY;
#else 
		ui32DependencyControl |= EURASIA_PDS_DOUTU0_TEXTUREDEPENDENCY;
#endif
	}

	psPDSInfo->ui32DMSInfoTextureSize	= ui32DMSInfoTextureSize;
	psPDSInfo->ui32DMSInfoIterationSize = ui32DMSInfoIterationSize;
	psPDSInfo->ui32DependencyControl	= ui32DependencyControl;
}


/****************************************************************
 * Function Name  	: SetupUSEFragmentShader
 * Returns        	: Success/Failure
 * Globals Used    	: 
 * Description    	: 
 ****************************************************************/
IMG_INTERNAL GLES1_MEMERROR SetupUSEFragmentShader(GLES1Context *gc, IMG_BOOL *pbProgramChanged)
{
	GLES1Shader *psFragmentShader = gc->sProgram.psCurrentFragmentShader;
	GLESCompiledRenderState *psRenderState = &gc->sPrim.sRenderState;
	GLES1ShaderVariant *psFragmentVariant = psFragmentShader->psShaderVariant;
	FFGEN_PROGRAM_DETAILS *psHWCode = psFragmentShader->psFFGENProgramDetails;
	IMG_UINT32 ui32BlendEquation, i;
	IMG_BOOL bMSAATrans = IMG_FALSE;

	if(gc->psMode->ui32AntiAliasMode && (((gc->sPrim.sRenderState.ui32ISPControlWordA & ~EURASIA_ISPA_PASSTYPE_CLRMSK) == EURASIA_ISPA_PASSTYPE_TRANS) || 
										 ((gc->sPrim.sRenderState.ui32ISPControlWordA & ~EURASIA_ISPA_PASSTYPE_CLRMSK) == EURASIA_ISPA_PASSTYPE_TRANSPT)))
	{
		if(gc->psRenderSurface->bFirstKick)
		{
			/* 
			 * We can't tell whether or not the render surface multisample state will change at KickTA time, 
			 * so set bMSAATrans in case the state changes to MSAA on 
			 */
			bMSAATrans = IMG_TRUE;
		}
		else
		{
			/* First TA kick done, so render surface multisample state can't change */
			if(gc->psRenderSurface->bMultiSample)
			{
				bMSAATrans = IMG_TRUE;
			}
		}
	}

	if(gc->ui32RasterEnables & GLES1_RS_ALPHABLEND_ENABLE)
	{
		ui32BlendEquation = gc->sState.sRaster.ui32BlendEquation;
	}
	else
	{
		ui32BlendEquation =	(GLES1_BLENDMODE_NONE << GLES1_BLENDMODE_RGB_SHIFT) | 
							(GLES1_BLENDMODE_NONE << GLES1_BLENDMODE_ALPHA_SHIFT);
	}

	while(psFragmentVariant)
	{
		if((psFragmentVariant->u.sFragment.ui32RasterEnables == (gc->ui32RasterEnables & (GLES1_RS_LOGICOP_ENABLE | GLES1_RS_FOG_ENABLE))) &&
		   (psFragmentVariant->u.sFragment.ui32ColorMask == gc->sState.sRaster.ui32ColorMask) &&
		   (psFragmentVariant->u.sFragment.ui32LogicOp == gc->sState.sRaster.ui32LogicOp) &&
		   (psFragmentVariant->u.sFragment.ui32BlendFunction == gc->sState.sRaster.ui32BlendFunction) &&
		   (psFragmentVariant->u.sFragment.ui32ShadeModel == gc->sState.sShade.ui32ShadeModel) &&
		   (psFragmentVariant->u.sFragment.ui32BlendEquation == ui32BlendEquation) &&
		   (psFragmentVariant->u.sFragment.ui32AlphaTestFlags == gc->sPrim.sRenderState.ui32AlphaTestFlags) &&
		   (psFragmentVariant->u.sFragment.bPointSprite == (gc->sPrim.eCurrentPrimitiveType == GLES1_PRIMTYPE_SPRITE)) &&
		   (psFragmentVariant->u.sFragment.bMSAATrans == bMSAATrans)
			)
		{
			/* If raster state matches, check point sprite state */
			IMG_BOOL bMatch = IMG_TRUE;

			if(gc->ui32NumImageUnitsActive)
			{
				for(i=0; i<GLES1_MAX_TEXTURE_UNITS; i++)
				{
					if(gc->ui32ImageUnitEnables & (1UL << i))
					{
						if(psFragmentVariant->u.sFragment.abPointSpriteReplace[i] != gc->sState.sTexture.asUnit[i].sEnv.bPointSpriteReplace)
						{
							/* If we have failed the search - break to next variant */
							bMatch = IMG_FALSE;

							break;
						}
					}
				}
			}

			/* If we have found a match, don't look any further */
			if(bMatch)
			{
				break;
			}
		}

		psFragmentVariant = psFragmentVariant->psNext;
	}

	if(!psFragmentVariant)
	{
		IMG_UINT32 ui32PlaneCoeffPAReg, ui32FogPAReg;
		IMG_UINT32 ui32CodeSizeInBytes = 0;
		IMG_UINT32 ui32NumTemps;
		GLESUSEASMInfo *psUSEASMInfo, sPhase0USEASMInfo = {0}, sPhase1USEASMInfo = {0};
		IMG_BOOL bSkipFollowingCode;
		IMG_UINT32 ui32Phase0CodeSizeInBytes, ui32Phase1CodeSizeInBytes, ui32Phase1CodeOffsetInBytes;
		USE_INST *psModifiedLastInstruction;
		IMG_UINT32 ui32MaxPrimaryAttributeReg;
		GLES1_MEMERROR eError;
#if defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728)
		IMG_UINT32 ui32PositionPAReg;
#endif /* defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728) */

		psFragmentVariant = GLES1Calloc(gc, sizeof(GLES1ShaderVariant));

		if(!psFragmentVariant)
		{
			return GLES1_HOST_MEM_ERROR;
		}

		psFragmentVariant->u.sFragment.ui32RasterEnables = gc->ui32RasterEnables & (GLES1_RS_LOGICOP_ENABLE | GLES1_RS_FOG_ENABLE);
		psFragmentVariant->u.sFragment.ui32ColorMask = gc->sState.sRaster.ui32ColorMask;
		psFragmentVariant->u.sFragment.ui32LogicOp = gc->sState.sRaster.ui32LogicOp;
		psFragmentVariant->u.sFragment.ui32BlendFunction = gc->sState.sRaster.ui32BlendFunction;
		psFragmentVariant->u.sFragment.ui32BlendEquation = ui32BlendEquation;
		psFragmentVariant->u.sFragment.ui32ShadeModel = gc->sState.sShade.ui32ShadeModel;
		psFragmentVariant->u.sFragment.ui32NumImageUnitsActive = gc->ui32NumImageUnitsActive;
		psFragmentVariant->u.sFragment.ui32AlphaTestFlags = gc->sPrim.sRenderState.ui32AlphaTestFlags;
		psFragmentVariant->u.sFragment.bPointSprite = (gc->sPrim.eCurrentPrimitiveType == GLES1_PRIMTYPE_SPRITE) ? IMG_TRUE : IMG_FALSE;
		psFragmentVariant->u.sFragment.bMSAATrans = bMSAATrans;
		if(gc->ui32NumImageUnitsActive)
		{
			for(i=0; i<GLES1_MAX_TEXTURE_UNITS; i++)
			{
				if(gc->ui32ImageUnitEnables & (1UL << i))
				{
					psFragmentVariant->u.sFragment.abPointSpriteReplace[i] = gc->sState.sTexture.asUnit[i].sEnv.bPointSpriteReplace;
				}
			}
		}

		/* Set current variant */
		gc->sProgram.psCurrentFragmentVariant = psFragmentVariant;
               
		psFragmentVariant->ui32USEPrimAttribCount = psHWCode->ui32PrimaryAttributeCount;

		ui32MaxPrimaryAttributeReg = psHWCode->ui32PrimaryAttributeCount - 1;

		/* Remove build warnings */
		ui32PlaneCoeffPAReg = 0;
		ui32FogPAReg = 0;

		/* 
			Generate pixel shader iterators and texture look ups 
			If a discard/ISP feedback is enabled we need to iterate 
			the triangle plane coefficients 
		*/
		SetupIteratorsAndTextureLookups(gc,
										psFragmentShader->u.sFFTB.psFFTBProgramDesc->ui32NumNonDependentTextureLoads,
										psFragmentShader->u.sFFTB.psFFTBProgramDesc->psNonDependentTextureLoads,
										&ui32FogPAReg,
										&ui32PlaneCoeffPAReg
#if defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728)
										,&ui32PositionPAReg
#endif /* defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728) */
										);

		psFragmentVariant->ui32MaxTempRegs = psHWCode->ui32TemporaryRegisterCount;

		psFragmentVariant->ui32PhaseCount=1;

		bSkipFollowingCode = IMG_FALSE;


		/*****************************************************************
		          Setup Phase 0 instructions in Phase0USEASMInfo
		******************************************************************/

		/* Copy FFTBProgram USE instructions into Phase0 */
		/* In the case of UNLIMITED_PHASES, a single PHAS is inserted in DuplicateUSEASMInstructionList() */

		if (psFragmentShader->u.sFFTB.psFFTBProgramDesc->sUSEASMInfo.ui32NumMainUSEASMInstructions > 0)
		{
			DuplicateUSEASMInstructionList(gc, &psFragmentShader->u.sFFTB.psFFTBProgramDesc->sUSEASMInfo, &sPhase0USEASMInfo);
		}

		/* Add more USE instructions into Phase0 */
		psUSEASMInfo = &sPhase0USEASMInfo;

		if(psRenderState->ui32AlphaTestFlags & GLES1_ALPHA_TEST_FEEDBACK)
		{
			USE_REGISTER asArg[11];
			IMG_UINT32 ui32SAOffset = 0;
			IMG_UINT32 ui32EndFlag = USEASM_OPFLAGS1_END;
#if !defined(SGX_FEATURE_ALPHATEST_AUTO_COEFF)
#if !defined(SGX_FEATURE_ALPHATEST_COEFREORDER)
			IMG_UINT32 ui32CoeffA;
#endif


			/* Transfer plane co-efficients with PCOEFF.
			This instruction runs over two cycles and accesses the sources in
			the following way:
#if defined(SGX_FEATURE_ALPHATEST_COEFREORDER)
			Iteration One:	Src0 = Coeff A, 	Src1 = Don't care,	Src2 = Don't care
			Iteration Two:	Src0 = Coeff B,		Src1 = Coeff C,		Src2 = VisTest register control
#else
			Iteration One:	Src0 = Don't care, 	Src1 = Coeff B,		Src2 = Don't care
			Iteration Two:	Src0 = Coeff A,		Src1 = Coeff C,		Src2 = VisTest register control
#endif
			Due to MOE increments, the sources are setup to access the correct data in a particular iteration. */

#if defined(SGX_FEATURE_ALPHATEST_COEFREORDER)
			/* PCOEFF */

#if defined(SGX_FEATURE_ALPHATEST_SECONDARY_PERPRIMITIVE)
			/* Src 0 */
			SETUP_INSTRUCTION_ARG(0, ui32PlaneCoeffPAReg, USEASM_REGTYPE_SECATTR, 0);

			/* Src 1 */
			SETUP_INSTRUCTION_ARG(1, ui32PlaneCoeffPAReg+1, USEASM_REGTYPE_SECATTR, 0);

			ui32SAOffset += GLES1_FRAGMENT_SECATTR_ALPHATEST_OFFSET;
#else
			/* Src 0 */
			SETUP_INSTRUCTION_ARG(0, ui32PlaneCoeffPAReg, USEASM_REGTYPE_PRIMATTR, 0);

			/* Src 1 */
			SETUP_INSTRUCTION_ARG(1, ui32PlaneCoeffPAReg+1, USEASM_REGTYPE_PRIMATTR, 0);
#endif

#else /* SGX_FEATURE_ALPHATEST_COEFREORDER */

			if(ui32PlaneCoeffPAReg == 0)
			{
				ui32CoeffA  = 0;

				SETUP_INSTRUCTION_ARG(0, 1, USEASM_REGTYPE_IMMEDIATE, 0);
				SETUP_INSTRUCTION_ARG(1, 0, USEASM_REGTYPE_IMMEDIATE, 0);
				SETUP_INSTRUCTION_ARG(2, 1, USEASM_REGTYPE_IMMEDIATE, 0);
				SETUP_INSTRUCTION_ARG(3, 1, USEASM_REGTYPE_IMMEDIATE, 0);
				SETUP_INSTRUCTION_ARG(4, 0, USEASM_REGTYPE_IMMEDIATE, 0);
				SETUP_INSTRUCTION_ARG(5, 0, USEASM_REGTYPE_IMMEDIATE, 0);
				SETUP_INSTRUCTION_ARG(6, 0, USEASM_REGTYPE_IMMEDIATE, 0);
				SETUP_INSTRUCTION_ARG(7, 0, USEASM_REGTYPE_IMMEDIATE, 0);
				SETUP_INSTRUCTION_ARG(8, 0, USEASM_REGTYPE_IMMEDIATE, 0);
				SETUP_INSTRUCTION_ARG(9, 0, USEASM_REGTYPE_IMMEDIATE, 0);
				SETUP_INSTRUCTION_ARG(10, 0, USEASM_REGTYPE_IMMEDIATE, 0);
					
				AddInstruction(gc, psUSEASMInfo, USEASM_OP_SMLSI, 0, 0, 0, asArg, 11);
			}
			else
			{
				ui32CoeffA = ui32PlaneCoeffPAReg - 1;
			}

			/* PCOEFF */

#if defined(SGX_FEATURE_ALPHATEST_SECONDARY_PERPRIMITIVE)
			/* Src 0 */
			SETUP_INSTRUCTION_ARG(0, ui32CoeffA, USEASM_REGTYPE_SECATTR, 0);

			/* Src 1 */
			SETUP_INSTRUCTION_ARG(1, ui32PlaneCoeffPAReg+1, USEASM_REGTYPE_SECATTR, 0);

			ui32SAOffset += GLES1_FRAGMENT_SECATTR_ALPHATEST_OFFSET;
#else
			/* Src 0 */
			SETUP_INSTRUCTION_ARG(0, ui32CoeffA, USEASM_REGTYPE_PRIMATTR, 0);

			/* Src 1 */
			SETUP_INSTRUCTION_ARG(1, ui32PlaneCoeffPAReg+1, USEASM_REGTYPE_PRIMATTR, 0);
#endif

#endif /* SGX_FEATURE_ALPHATEST_COEFREORDER */

			/* Src 2 */
			SETUP_INSTRUCTION_ARG(2, EURASIA_USE_SPECIAL_CONSTANT_ZERO1, USEASM_REGTYPE_FPCONSTANT, 0);

#if defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728)
			if((gc->sPrim.sRenderState.ui32AlphaTestFlags & GLES1_ALPHA_TEST_BRNFIX_DEPTHF) == 0)
#endif /* defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728) */
			{
				AddInstruction(gc, psUSEASMInfo, USEASM_OP_PCOEFF, 0, 0, 0, asArg, 3);
			}	
#endif /* !defined(SGX_FEATURE_ALPHATEST_AUTO_COEFF) */


			/* ATST8 - writes the colour result to PA0.
			   Temporaries are not preserved between phases */
		
			/* Dst */
			SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_PRIMATTR, 0);

			/* Predicate */
#if defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728)
			if(psRenderState->ui32AlphaTestFlags & GLES1_ALPHA_TEST_BRNFIX_DEPTHF)
			{
				SETUP_INSTRUCTION_ARG(1, 1, USEASM_REGTYPE_PREDICATE, 0);
			}
			else
#endif /* defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728) */
			{
				SETUP_INSTRUCTION_ARG(1, 0, USEASM_REGTYPE_PREDICATE, USEASM_ARGFLAGS_DISABLEWB);
			}

			/* Src 0 */
			SETUP_INSTRUCTION_ARG(2, 0, USEASM_REGTYPE_TEMP, 0);

			/* Src 1 */
			SETUP_INSTRUCTION_ARG(3, ui32SAOffset + 0, USEASM_REGTYPE_SECATTR, 0);

			/* Src 2 */
			SETUP_INSTRUCTION_ARG(4, ui32SAOffset + 1, USEASM_REGTYPE_SECATTR, 0);

			/* [TWOSIDED,Mode] */
			SETUP_INSTRUCTION_ARG(5, USEASM_INTSRCSEL_NONE, USEASM_REGTYPE_INTSRCSEL, 0);

			/* Feedback */
#if defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728)
			if(psRenderState->ui32AlphaTestFlags & GLES1_ALPHA_TEST_BRNFIX_DEPTHF)
			{
				SETUP_INSTRUCTION_ARG(6, USEASM_INTSRCSEL_NOFEEDBACK, USEASM_REGTYPE_INTSRCSEL, 0);
			}
			else
#endif /* defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728) */
			{
				SETUP_INSTRUCTION_ARG(6, USEASM_INTSRCSEL_FEEDBACK, USEASM_REGTYPE_INTSRCSEL, 0);
			}


#if defined(FIX_HW_BRN_33668)
			ui32EndFlag = 0;
#endif

#if defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728)
			if(psRenderState->ui32AlphaTestFlags & GLES1_ALPHA_TEST_BRNFIX_DEPTHF)
			{
				ui32EndFlag = 0;
			}
#endif

			AddInstruction(gc, psUSEASMInfo, USEASM_OP_ATST8, ui32EndFlag, 0, 0, asArg, 7);

			/* 3 more primaries */
			ui32MaxPrimaryAttributeReg += 3;


#if defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728)
			if(psRenderState->ui32AlphaTestFlags & GLES1_ALPHA_TEST_BRNFIX_DEPTHF)
			{
				/* Src 0 */
				SETUP_INSTRUCTION_ARG(0, ui32PositionPAReg+2, USEASM_REGTYPE_PRIMATTR, 0);

				/* Src 1 */
				SETUP_INSTRUCTION_ARG(1, ui32SAOffset+0, USEASM_REGTYPE_SECATTR, 0);

				/* Src 2 */
				SETUP_INSTRUCTION_ARG(2, ui32SAOffset+1, USEASM_REGTYPE_SECATTR, 0);

				/* [TWOSIDED,Mode] */
				SETUP_INSTRUCTION_ARG(3, USEASM_INTSRCSEL_NONE, USEASM_REGTYPE_INTSRCSEL, 0);

				/* Feedback */
				SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_FEEDBACK, USEASM_REGTYPE_INTSRCSEL, 0);

				AddInstruction(gc, psUSEASMInfo, USEASM_OP_DEPTHF, USEASM_OPFLAGS1_END | (USEASM_PRED_P1<<USEASM_OPFLAGS1_PRED_SHIFT), 0, 0, asArg, 5);

				/* 3 more primaries */
				ui32MaxPrimaryAttributeReg += 3;
			}
#endif /* defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728) */

#if defined(FIX_HW_BRN_33668)
			AddInstruction(gc, psUSEASMInfo, USEASM_OP_PADDING, USEASM_OPFLAGS1_END, 0, 0, asArg, 0);
#endif /* FIX_HW_BRN_33668 */

			/*****************************************************************
		          Setup Phase 1 instructions in Phase1USEASMInfo
			******************************************************************/

			/* Any following code goes into the next phase */
			psUSEASMInfo = &sPhase1USEASMInfo;

			psFragmentVariant->ui32PhaseCount++;

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
			/* Insert 2nd PHAS instruction */
			/* Arg[0] : the execution address of the next phase */
			SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_IMMEDIATE, 0);

			/* Arg[1] : the number of temporary registers to allocate for the next phase */
			SETUP_INSTRUCTION_ARG(1, 0, USEASM_REGTYPE_IMMEDIATE, 0);

			/* Arg[2]: the execution rate for the next phase */
			SETUP_INSTRUCTION_ARG(2, USEASM_INTSRCSEL_PIXEL, USEASM_REGTYPE_INTSRCSEL, 0);

			/* Arg[3]: the wait condition for the start of the next phase */
			SETUP_INSTRUCTION_ARG(3, USEASM_INTSRCSEL_END, USEASM_REGTYPE_INTSRCSEL, 0);

			/* Arg[4]: the execution mode for the next phase */
			SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_PARALLEL, USEASM_REGTYPE_INTSRCSEL, 0);

			/* Add instruction */
			AddInstruction(gc, psUSEASMInfo, USEASM_OP_PHASIMM, 0, 0, 0, asArg, 5);

#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */


			if(psRenderState->ui32AlphaTestFlags & GLES1_ALPHA_TEST_WAS_TWD)
			{
				/* Was TWD, so just NOP in second phase */
				AddInstruction(gc, psUSEASMInfo, USEASM_OP_PADDING, 0, 0, 0, IMG_NULL, 0);
			}
			else
			{
				/* Restore colour from primary attribute register.
				This instruction is to be executed in phase 1, see usage of
				define GLES1_VISIBILITYTEST_NUM_PHASE1_INSTRUCTIONS.

				MOV Temp0, PrimaryAttribute0 */

				/* Dst */
				SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_TEMP, 0);

				/* Src */
				SETUP_INSTRUCTION_ARG(1, 0, USEASM_REGTYPE_PRIMATTR, 0);

				AddInstruction(gc, psUSEASMInfo, USEASM_OP_MOV, 0, 0, 0, asArg, 2);
			}
		}
		else if(psRenderState->ui32AlphaTestFlags & GLES1_ALPHA_TEST_ONLY)
		{
			USE_REGISTER asArg[7];

			/* ATST8 - writes the colour result to Temp0, Sets predicate p0, disables ISP feedback */

			if((!psRenderState->bNeedLogicOpsCode && !psRenderState->bNeedFBBlendCode && !psRenderState->bNeedColorMaskCode) &&
				!(((gc->ui32RasterEnables & GLES1_RS_FOG_ENABLE) != 0) && ((psRenderState->ui32AlphaTestFlags & GLES1_ALPHA_TEST_WAS_TWD) == 0)))
			{

				/* If this will be the last instruction - don't set predicate, write result to OUTPUT reg, set END for instruction */

				/* Dst */
				SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_OUTPUT, 0);

				/* Predicate */
				SETUP_INSTRUCTION_ARG(1, 0, USEASM_REGTYPE_PREDICATE,  USEASM_ARGFLAGS_DISABLEWB);

				/* Src 0 */
				SETUP_INSTRUCTION_ARG(2, 0, USEASM_REGTYPE_TEMP, 0);

				/* Src 1 */
				SETUP_INSTRUCTION_ARG(3, 0, USEASM_REGTYPE_SECATTR, 0);

				/* Src 2 */
				SETUP_INSTRUCTION_ARG(4, 1, USEASM_REGTYPE_SECATTR, 0);

				/* [TWOSIDED,Mode] */
				SETUP_INSTRUCTION_ARG(5, USEASM_INTSRCSEL_NONE, USEASM_REGTYPE_INTSRCSEL, 0);

				/* Feedback */
				SETUP_INSTRUCTION_ARG(6, USEASM_INTSRCSEL_NOFEEDBACK, USEASM_REGTYPE_INTSRCSEL, 0);

				AddInstruction(gc, psUSEASMInfo, USEASM_OP_ATST8, USEASM_OPFLAGS1_END, 0, 0, asArg, 7);

				bSkipFollowingCode = IMG_TRUE;
			}
			else
			{
				/* Dst */
				SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_TEMP, 0);

				/* Predicate */
				SETUP_INSTRUCTION_ARG(1, 0, USEASM_REGTYPE_PREDICATE,  0);

				/* Src 0 */
				SETUP_INSTRUCTION_ARG(2, 0, USEASM_REGTYPE_TEMP, 0);

				/* Src 1 */
				SETUP_INSTRUCTION_ARG(3, 0, USEASM_REGTYPE_SECATTR, 0);

				/* Src 2 */
				SETUP_INSTRUCTION_ARG(4, 1, USEASM_REGTYPE_SECATTR, 0);

				/* [TWOSIDED,Mode] */
				SETUP_INSTRUCTION_ARG(5, USEASM_INTSRCSEL_NONE, USEASM_REGTYPE_INTSRCSEL, 0);

				/* Feedback */
				SETUP_INSTRUCTION_ARG(6, USEASM_INTSRCSEL_NOFEEDBACK, USEASM_REGTYPE_INTSRCSEL, 0);

				AddInstruction(gc, psUSEASMInfo, USEASM_OP_ATST8, 0, 0, 0, asArg, 7);
			}
		}

		ui32NumTemps = 1;

		if(!bSkipFollowingCode)
		{
			if(((gc->ui32RasterEnables & GLES1_RS_FOG_ENABLE) != 0) && ((psRenderState->ui32AlphaTestFlags & GLES1_ALPHA_TEST_WAS_TWD) == 0))
			{
				CreateFogUSEASMCode(gc, psUSEASMInfo, ui32FogPAReg);

				ui32NumTemps++;

				ui32MaxPrimaryAttributeReg++;
			}

			if(psRenderState->bNeedLogicOpsCode)
			{
				CreateLogicalOpsUSEASMCode(gc, psUSEASMInfo);
			}
			else if(psRenderState->bNeedFBBlendCode)
			{
				CreateFBBlendUSEASMCode(gc, psUSEASMInfo);
			}

			if(psRenderState->bNeedColorMaskCode)
			{
				CreateColorMaskUSEASMCode(gc, psUSEASMInfo);
			}
			else
			{
				/* Change the main issue, not a coissue */
				if(psUSEASMInfo->psLastUSEASMInstruction->uFlags2 & USEASM_OPFLAGS2_COISSUE)
				{
					psModifiedLastInstruction = psUSEASMInfo->psLastUSEASMInstruction->psPrev;
				}
				else
				{
					psModifiedLastInstruction = psUSEASMInfo->psLastUSEASMInstruction;
				}

				psModifiedLastInstruction->asArg[0].uType = USEASM_REGTYPE_OUTPUT;

					psModifiedLastInstruction->uFlags1 |= USEASM_OPFLAGS1_END;

			}	
		}


		sPhase0USEASMInfo.ui32MaxPrimaryNumber = ui32MaxPrimaryAttributeReg;
		sPhase0USEASMInfo.ui32MaxTempNumber = MAX(psFragmentVariant->ui32MaxTempRegs, ui32NumTemps)-1;

		eError = AssembleUSEASMInstructions(gc, &sPhase0USEASMInfo);

		if(eError != GLES1_NO_ERROR)
		{
			FreeUSEASMInstructionList(gc, &sPhase0USEASMInfo);

			return eError;
		}

		ui32Phase0CodeSizeInBytes = sPhase0USEASMInfo.ui32NumHWInstructions * EURASIA_USE_INSTRUCTION_SIZE;


		ui32CodeSizeInBytes += ui32Phase0CodeSizeInBytes;

		/* Assemble phase 1, if necessary */
		if(sPhase1USEASMInfo.ui32NumMainUSEASMInstructions)
		{
			sPhase1USEASMInfo.ui32MaxPrimaryNumber = ui32MaxPrimaryAttributeReg;
			sPhase1USEASMInfo.ui32MaxTempNumber = MAX(psFragmentVariant->ui32MaxTempRegs, ui32NumTemps)-1;

			eError = AssembleUSEASMInstructions(gc, &sPhase1USEASMInfo);

			if(eError != GLES1_NO_ERROR)
			{
				FreeUSEASMInstructionList(gc, &sPhase0USEASMInfo);
				FreeUSEASMInstructionList(gc, &sPhase1USEASMInfo);

				return eError;
			}

			ui32Phase1CodeSizeInBytes = sPhase1USEASMInfo.ui32NumHWInstructions * EURASIA_USE_INSTRUCTION_SIZE;

			/* Align the start address of phase 1 */
			ui32CodeSizeInBytes += (EURASIA_PDS_DOUTU_PHASE_START_ALIGN - 1);
			ui32CodeSizeInBytes &= ~(EURASIA_PDS_DOUTU_PHASE_START_ALIGN - 1);

			ui32Phase1CodeOffsetInBytes = ui32CodeSizeInBytes;

			ui32CodeSizeInBytes += ui32Phase1CodeSizeInBytes;
		}
		else
		{
			ui32Phase1CodeSizeInBytes = 0;

			/* Remove build warnings */
			ui32Phase1CodeOffsetInBytes = 0;
		}


		GLES1_TIME_START(GLES1_TIMER_USECODEHEAP_FRAG_TIME);

		psFragmentVariant->psCodeBlock = UCH_CodeHeapAllocate(gc->psSharedState->psUSEFragmentCodeHeap, ui32CodeSizeInBytes, gc->psSysContext->hPerProcRef);

		GLES1_TIME_STOP(GLES1_TIMER_USECODEHEAP_FRAG_TIME);
		

		if(!psFragmentVariant->psCodeBlock)
		{
			/* Destroy unused fragment shaders */
			KRM_ReclaimUnneededResources(gc, &gc->psSharedState->sUSEShaderVariantKRM);
			
			GLES1_TIME_START(GLES1_TIMER_USECODEHEAP_FRAG_TIME);

			psFragmentVariant->psCodeBlock = UCH_CodeHeapAllocate(gc->psSharedState->psUSEFragmentCodeHeap, ui32CodeSizeInBytes, gc->psSysContext->hPerProcRef);
		
			GLES1_TIME_STOP(GLES1_TIMER_USECODEHEAP_FRAG_TIME);
			
			if(!psFragmentVariant->psCodeBlock)
			{
				GLES1Free(IMG_NULL, psFragmentVariant);

				return GLES1_3D_USECODE_ERROR;
			}
		}

		GLES1_INC_COUNT(GLES1_TIMER_USECODEHEAP_FRAG_COUNT, ui32CodeSizeInBytes>>2);

		/* Setup Max number of temporary registers */
		psFragmentVariant->ui32MaxTempRegs = MAX(psFragmentVariant->ui32MaxTempRegs, ui32NumTemps);

#if defined(SGX_FEATURE_USE_VEC34)
		/* Align temp counts to nearest pair */
		psFragmentVariant->ui32MaxTempRegs = ALIGNCOUNT(psFragmentVariant->ui32MaxTempRegs, 2);
#endif

#if defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
		/* Need to allocate 4 times as many temps if running at sample rate */
		if(bMSAATrans)
		{
			psFragmentVariant->ui32MaxTempRegs <<= 2;
		}
#endif

		/* Setup use mode of operation */
		if(psHWCode->bUSEPerInstanceMode)
		{
			psFragmentShader->ui32USEMode = EURASIA_PDS_DOUTU1_MODE_PERINSTANCE;
		}
		else
		{
			psFragmentShader->ui32USEMode = EURASIA_PDS_DOUTU1_MODE_PARALLEL;
		}

		/* Setup phase 0 */

		psFragmentVariant->sStartAddress[0].uiAddr = psFragmentVariant->psCodeBlock->sCodeAddress.uiAddr;

		GLES1MemCopy(psFragmentVariant->psCodeBlock->pui32LinAddress, sPhase0USEASMInfo.pui32HWInstructions, ui32Phase0CodeSizeInBytes);

		/* Delete instructions and the temporary HW code buffer */
		FreeUSEASMInstructionList(gc, &sPhase0USEASMInfo);

		/* Setup phase 1 */

		if(ui32Phase1CodeSizeInBytes)
		{
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
			IMG_UINT32 ui32ExeAddr;
#if !defined(FIX_HW_BRN_29019)
		    IMG_UINT32 *pui32USECode = psFragmentVariant->psCodeBlock->pui32LinAddress;
#else /* !defined(FIX_HW_BRN_29019) */
			/* PHAS is 2nd instruction in shader */
		    IMG_UINT32 *pui32USECode = psFragmentVariant->psCodeBlock->pui32LinAddress + USE_INST_LENGTH;

			/* Need at least 1 PA for the dummy MOV */
			if(!psFragmentVariant->ui32USEPrimAttribCount)
			{
				psFragmentVariant->ui32USEPrimAttribCount++;
			}
#endif /* !defined(FIX_HW_BRN_29019) */

#endif

			psFragmentVariant->sStartAddress[1].uiAddr = psFragmentVariant->psCodeBlock->sCodeAddress.uiAddr + ui32Phase1CodeOffsetInBytes;

			GLES1MemCopy((IMG_UINT8 *)psFragmentVariant->psCodeBlock->pui32LinAddress + ui32Phase1CodeOffsetInBytes, sPhase1USEASMInfo.pui32HWInstructions, ui32Phase1CodeSizeInBytes);

			/* Delete instructions and the temporary HW code buffer */
			FreeUSEASMInstructionList(gc, &sPhase1USEASMInfo);

			/* Reset PHAS instruction in phase 0 */
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)

			/* the execution address of the next phase */
			ui32ExeAddr = GetUSEPhaseAddress(psFragmentVariant->sStartAddress[1],
												gc->psSysContext->uUSEFragmentHeapBase, 
												SGX_PIXSHADER_USE_CODE_BASE_INDEX);
		
			/* Insert PHAS for Phase0 */
			BuildPHASImmediate((PPVR_USE_INST)	pui32USECode,
								0, /* ui32End */
								(psHWCode->bUSEPerInstanceMode ? EURASIA_USE_OTHER2_PHAS_MODE_PERINSTANCE : EURASIA_USE_OTHER2_PHAS_MODE_PARALLEL),
								(bMSAATrans ? EURASIA_USE_OTHER2_PHAS_RATE_SELECTIVE : EURASIA_USE_OTHER2_PHAS_RATE_PIXEL),
								EURASIA_USE_OTHER2_PHAS_WAITCOND_PT,
								psFragmentVariant->ui32MaxTempRegs,
								ui32ExeAddr);

			/* Get code address for Phase1 */
			pui32USECode = psFragmentVariant->psCodeBlock->pui32LinAddress + ui32Phase1CodeOffsetInBytes;

			/* Insert PHAS for Phas1 */
			BuildPHASLastPhase ((PPVR_USE_INST) pui32USECode, 0);

#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */
		}

		/* Add to variant list */
		psFragmentVariant->psNext = psFragmentShader->psShaderVariant;
		psFragmentShader->psShaderVariant = psFragmentVariant;
		
		/* And back link it to the shader */
		psFragmentVariant->psShader = psFragmentShader;

		*pbProgramChanged = IMG_TRUE;
	}
	else
	{
		/* Set current variant */
		if(gc->sProgram.psCurrentFragmentVariant!=psFragmentVariant)
		{
			gc->sProgram.psCurrentFragmentVariant = psFragmentVariant;

			*pbProgramChanged = IMG_TRUE;
		}
		else
		{
			*pbProgramChanged = IMG_FALSE;
		}

	}
	

	return GLES1_NO_ERROR;
}


/*********************************************************************************
 Function	 : WriteUSEVertexShaderMemConsts

 Description : Writes out the memory constants for the current vertex shader
			   USE code

 Parameters	 : psContext			- Current context

 Return		 : BOOL					- IMG_TRUE on success. IMG_FALSE otherwise.
*********************************************************************************/
IMG_INTERNAL GLES1_MEMERROR WriteUSEVertexShaderMemConsts(GLES1Context *gc)
{
	IMG_UINT32 *pui32Buffer;
	GLES1Shader *psShader;
	FFGEN_PROGRAM_DETAILS *psHWCode;
	IMG_UINT32 ui32SizeOfConstantsInDWords;

	psShader = gc->sProgram.psCurrentVertexShader;

	psHWCode = psShader->psFFGENProgramDetails;

	ui32SizeOfConstantsInDWords = psHWCode->ui32MemoryConstantCount;

	if(!ui32SizeOfConstantsInDWords)
	{
		/* Nothing to do */

		return GLES1_NO_ERROR;
	}

	/*
		Get buffer space for all the SA-register constants
	*/
	pui32Buffer = CBUF_GetBufferSpace(gc->apsBuffers, ui32SizeOfConstantsInDWords, CBUF_TYPE_PDS_VERT_BUFFER, IMG_FALSE);

	if(!pui32Buffer)
	{
		return GLES1_TA_BUFFER_ERROR;
	}

	GLES1MemCopy(&pui32Buffer[0],
				 &psShader->pfConstantData[psHWCode->ui32SecondaryAttributeCount+GLES1_VERTEX_SECATTR_NUM_RESERVED],
				 ui32SizeOfConstantsInDWords<<2);

	/* Update buffer position */
	CBUF_UpdateBufferPos(gc->apsBuffers, ui32SizeOfConstantsInDWords, CBUF_TYPE_PDS_VERT_BUFFER);

	GLES1_INC_COUNT(GLES1_TIMER_PDS_VERT_DATA_COUNT, ui32SizeOfConstantsInDWords);

	/*
		Record the device address of the memory constants and their size
	*/
	psShader->uUSEConstsDataBaseAddress  = CBUF_GetBufferDeviceAddress(gc->apsBuffers, pui32Buffer, CBUF_TYPE_PDS_VERT_BUFFER); 
	psShader->ui32USEConstsDataSizeinDWords = ui32SizeOfConstantsInDWords;

	return GLES1_NO_ERROR;
}
/******************************************************************************
 End of file (usegles.c)
******************************************************************************/
