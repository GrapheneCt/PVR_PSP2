/******************************************************************************
 * Name         : use.c
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
 * $Log: use.c $
 * by <<= 2.
 *  --- Revision Logs Removed --- 
 *****************************************************************************/

#include "context.h"
#include "pixevent.h"
#include "use.h"

#define GLES2_PIXELEVENT_EOT_NO_DITHER			0
#define GLES2_PIXELEVENT_EOT_DITHER				1
#define GLES2_PIXELEVENT_EOT_NO_DITHER_MSAA		2
#define GLES2_PIXELEVENT_EOT_DITHER_MSAA		3
#define GLES2_PIXELEVENT_EOT_NUM_VARIANTS		4


/* Total of GLES2_MAX_MTE_STATE_DWORDS versions of the state copy code. 

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
#define GLES2_STATECOPY_FIRST16_SIZE_IN_BYTES    (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 9), EURASIA_USE_INSTRUCTION_CACHE_LINE_SIZE))  
#define GLES2_STATECOPY_REMAIN_SIZE_IN_BYTES     (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 10), EURASIA_USE_INSTRUCTION_CACHE_LINE_SIZE)) 
#else
#define GLES2_STATECOPY_FIRST16_SIZE_IN_BYTES    (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 5), EURASIA_PDS_DOUTU_PHASE_START_ALIGN))  /* + PHAS + WOP + EMITVCB */
#define GLES2_STATECOPY_REMAIN_SIZE_IN_BYTES     (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 6), EURASIA_PDS_DOUTU_PHASE_START_ALIGN))  /* + PHAS + WOP + EMITVCB */
#endif

#else
#if defined(FIX_HW_BRN_31988)
/* + PHAS + NOP + TST + LDAD + WDF */
#define GLES2_STATECOPY_FIRST16_SIZE_IN_BYTES    (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 7), EURASIA_USE_INSTRUCTION_CACHE_LINE_SIZE))  
#define GLES2_STATECOPY_REMAIN_SIZE_IN_BYTES     (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 8), EURASIA_USE_INSTRUCTION_CACHE_LINE_SIZE))
#else
#define GLES2_STATECOPY_FIRST16_SIZE_IN_BYTES    (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 3), EURASIA_PDS_DOUTU_PHASE_START_ALIGN))  /* + PHAS */
#define GLES2_STATECOPY_REMAIN_SIZE_IN_BYTES     (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 4), EURASIA_PDS_DOUTU_PHASE_START_ALIGN))  /* + PHAS */
#endif
#endif /* defined(SGX_FEATURE_VCB) */
#else
#define GLES2_STATECOPY_FIRST16_SIZE_IN_BYTES    (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 2), EURASIA_PDS_DOUTU_PHASE_START_ALIGN))
#define GLES2_STATECOPY_REMAIN_SIZE_IN_BYTES     (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 4), EURASIA_PDS_DOUTU_PHASE_START_ALIGN))
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */


static const IMG_UINT32 aui32StreamTypeToUseType[GLES2_STREAMTYPE_MAX] = {
	EURASIA_USE1_PCK_FMT_S8,	/* GLES2_STREAMTYPE_BYTE    */
	EURASIA_USE1_PCK_FMT_U8,	/* GLES2_STREAMTYPE_UBYTE    */
	EURASIA_USE1_PCK_FMT_S16,	/* GLES2_STREAMTYPE_SHORT  */
	EURASIA_USE1_PCK_FMT_U16,	/* GLES2_STREAMTYPE_USHORT  */
	EURASIA_USE1_PCK_FMT_F32,	/* GLES2_STREAMTYPE_FLOAT  */
	EURASIA_USE1_PCK_FMT_F16,	/* GLES2_STREAMTYPE_HALFFLOAT */
	EURASIA_USE1_PCK_FMT_U8,	/* GLES2_STREAMTYPE_FIXED  */
};

/* Size of attribs in bytes, by type */
static const IMG_UINT32 aui32AttribSize[GLES2_STREAMTYPE_MAX] =
{
	1,		/* GLES2_STREAMTYPE_BYTE */
	1,		/* GLES2_STREAMTYPE_UBYTE */
	2,		/* GLES2_STREAMTYPE_SHORT */
	2,		/* GLES2_STREAMTYPE_USHORT */
	4,		/* GLES2_STREAMTYPE_FLOAT */
	2,		/* GLES2_STREAMTYPE_HALFFLOAT */
	4		/* GLES2_STREAMTYPE_FIXED */
};

/*****************************************************************************/

/* 
 * Arrays that map Blend factor/equation defines 
 * to Eurasia SOP2 instruction fields 
 */
static const IMG_UINT32 aui32SOP2ColBlendOp[] = {
	0,						  /* GLES2_BLENDFUNC_NONE */ 
	GLES2_USE0_SOP2_COP(ADD), /* GLES2_BLENDFUNC_ADD */ 
	GLES2_USE0_SOP2_COP(SUB), /* GLES2_BLENDFUNC_SUBTRACT */ 
	GLES2_USE0_SOP2_COP(SUB), /* GLES2_BLENDFUNC_REVSUBTRACT */ };

static const IMG_UINT32 aui32SOP2AlphaBlendOp[] = {
	0,						  /* GLES2_BLENDFUNC_NONE */ 
	GLES2_USE0_SOP2_AOP(ADD), /* GLES2_BLENDFUNC_ADD */ 
	GLES2_USE0_SOP2_AOP(SUB), /* GLES2_BLENDFUNC_SUBTRACT */ 
	GLES2_USE0_SOP2_AOP(SUB), /* GLES2_BLENDFUNC_REVSUBTRACT */ };

/*
 * Arrays that flip between src and dst in blend factor defines
 */
static const IMG_UINT32 aui32SOP2FlipSel[] = {
	GLES2_BLENDFACTOR_ZERO,                 /* GLES2_BLENDFACTOR_ZERO */
	GLES2_BLENDFACTOR_ONE,	                /* GLES2_BLENDFACTOR_ONE */
	GLES2_BLENDFACTOR_DSTCOLOR,             /* GLES2_BLENDFACTOR_SRCCOLOR */
	GLES2_BLENDFACTOR_ONEMINUS_DSTCOLOR,    /* GLES2_BLENDFACTOR_ONEMINUS_SRCCOLOR */
	GLES2_BLENDFACTOR_DSTALPHA,             /* GLES2_BLENDFACTOR_SRCALPHA */
	GLES2_BLENDFACTOR_ONEMINUS_DSTALPHA,    /* GLES2_BLENDFACTOR_ONEMINUS_SRCALPHA */
	GLES2_BLENDFACTOR_SRCALPHA,             /* GLES2_BLENDFACTOR_DSTALPHA */
	GLES2_BLENDFACTOR_ONEMINUS_SRCALPHA,    /* GLES2_BLENDFACTOR_ONEMINUS_DSTALPHA */
	GLES2_BLENDFACTOR_SRCCOLOR,             /* GLES2_BLENDFACTOR_DSTCOLOR */
	GLES2_BLENDFACTOR_ONEMINUS_SRCCOLOR,    /* GLES2_BLENDFACTOR_ONEMINUS_DSTCOLOR */
	GLES2_BLENDFACTOR_DSTALPHA_SATURATE,    /* GLES2_BLENDFACTOR_SRCALPHA_SATURATE */
	GLES2_BLENDFACTOR_CONSTCOLOR,           /* GLES2_BLENDFACTOR_CONSTCOLOR */
	GLES2_BLENDFACTOR_ONEMINUS_CONSTCOLOR,  /* GLES2_BLENDFACTOR_ONEMINUS_CONSTCOLOR */
	GLES2_BLENDFACTOR_CONSTALPHA,           /* GLES2_BLENDFACTOR_CONSTALPHA */
	GLES2_BLENDFACTOR_ONEMINUS_CONSTALPHA,  /* GLES2_BLENDFACTOR_ONEMINUS_CONSTALPHA */
	GLES2_BLENDFACTOR_SRCALPHA_SATURATE,    /* GLES2_BLENDFACTOR_DSTALPHA_SATURATE */
};

/* Map Blend factor defines to color dest Eurasia source 2 color selects */
static const IMG_UINT32 aui32SOP2ColSrc1Sel[] = {
	GLES2_USE1_SOP2_CSEL1(ZERO),										/* GLES2_BLENDFACTOR_ZERO */
	GLES2_USE1_SOP2_CSEL1(ZERO)			| GLES2_USE1_SOP2_CSRC1_COMP,	/* GLES2_BLENDFACTOR_ONE */
	GLES2_USE1_SOP2_CSEL1(SRC1),										/* GLES2_BLENDFACTOR_SRCCOLOR */
	GLES2_USE1_SOP2_CSEL1(SRC1)			| GLES2_USE1_SOP2_CSRC1_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_SRCCOLOR */
	GLES2_USE1_SOP2_CSEL1(SRC1ALPHA),									/* GLES2_BLENDFACTOR_SRCALPHA */
	GLES2_USE1_SOP2_CSEL1(SRC1ALPHA)	| GLES2_USE1_SOP2_CSRC1_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_SRCALPHA */
	GLES2_USE1_SOP2_CSEL1(SRC2ALPHA),									/* GLES2_BLENDFACTOR_DSTALPHA */
	GLES2_USE1_SOP2_CSEL1(SRC2ALPHA)	| GLES2_USE1_SOP2_CSRC1_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_DSTALPHA */
	GLES2_USE1_SOP2_CSEL1(SRC2),										/* GLES2_BLENDFACTOR_DSTCOLOR */
	GLES2_USE1_SOP2_CSEL1(SRC2)			| GLES2_USE1_SOP2_CSRC1_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_DSTCOLOR */
	GLES2_USE1_SOP2_CSEL1(MINSRC1A1MSRC2A),								/* GLES2_BLENDFACTOR_SRCALPHA_SATURATE */
	GLES2_USE1_SOP2_CSEL1(ZERO),		/* Cannot be encoded by SOP2 ->	   GLES2_BLENDFACTOR_CONSTCOLOR */
	GLES2_USE1_SOP2_CSEL1(ZERO),		/* Cannot be encoded by SOP2 ->	   GLES2_BLENDFACTOR_ONEMINUS_CONSTCOLOR */
	GLES2_USE1_SOP2_CSEL1(ZERO),		/* Cannot be encoded by SOP2 ->    GLES2_BLENDFACTOR_CONSTALPHA */
	GLES2_USE1_SOP2_CSEL1(ZERO),			/* Cannot be encoded by SOP2 ->    GLES2_BLENDFACTOR_ONEMINUS_CONSTALPHA */ 
	GLES2_USE1_SOP2_CSEL1(MINSRC1A1MSRC2A) | GLES2_USE1_SOP2_CSRC1_COMP /* GLES2_BLENDFACTOR_DSTALPHA_SATURATE */
};

static const IMG_UINT32 aui32SOP2ColSrc2Sel[] = {
	GLES2_USE1_SOP2_CSEL2(ZERO),										/* GLES2_BLENDFACTOR_ZERO */
	GLES2_USE1_SOP2_CSEL2(ZERO)			| GLES2_USE1_SOP2_CSRC2_COMP,	/* GLES2_BLENDFACTOR_ONE */
	GLES2_USE1_SOP2_CSEL2(SRC1),										/* GLES2_BLENDFACTOR_SRCCOLOR */
	GLES2_USE1_SOP2_CSEL2(SRC1)			| GLES2_USE1_SOP2_CSRC2_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_SRCCOLOR */
	GLES2_USE1_SOP2_CSEL2(SRC1ALPHA),									/* GLES2_BLENDFACTOR_SRCALPHA */
	GLES2_USE1_SOP2_CSEL2(SRC1ALPHA)	| GLES2_USE1_SOP2_CSRC2_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_SRCALPHA */
	GLES2_USE1_SOP2_CSEL2(SRC2ALPHA),									/* GLES2_BLENDFACTOR_DSTALPHA */
	GLES2_USE1_SOP2_CSEL2(SRC2ALPHA)	| GLES2_USE1_SOP2_CSRC2_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_DSTALPHA */
	GLES2_USE1_SOP2_CSEL2(SRC2),										/* GLES2_BLENDFACTOR_DSTCOLOR */
	GLES2_USE1_SOP2_CSEL2(SRC2)			| GLES2_USE1_SOP2_CSRC2_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_DSTCOLOR */
	GLES2_USE1_SOP2_CSEL2(MINSRC1A1MSRC2A),								/* GLES2_BLENDFACTOR_SRCALPHA_SATURATE */
	GLES2_USE1_SOP2_CSEL2(ZERO),		/* Cannot be encoded by SOP2 ->    GLES2_BLENDFACTOR_CONSTCOLOR */
	GLES2_USE1_SOP2_CSEL2(ZERO),		/* Cannot be encoded by SOP2 ->    GLES2_BLENDFACTOR_ONEMINUS_CONSTCOLOR */
	GLES2_USE1_SOP2_CSEL2(ZERO),		/* Cannot be encoded by SOP2 ->    GLES2_BLENDFACTOR_CONSTALPHA */
	GLES2_USE1_SOP2_CSEL2(ZERO),		/* Cannot be encoded by SOP2 ->    GLES2_BLENDFACTOR_ONEMINUS_CONSTALPHA */
	GLES2_USE1_SOP2_CSEL2(MINSRC1A1MSRC2A) | GLES2_USE1_SOP2_CSRC2_COMP /* GLES2_BLENDFACTOR_DSTALPHA_SATURATE */
};

static const IMG_UINT32 aui32SOP2AlphaSrc1Sel[] = {
	GLES2_USE1_SOP2_ASEL1(ZERO),										/* GLES2_BLENDFACTOR_ZERO */
	GLES2_USE1_SOP2_ASEL1(ZERO)			| GLES2_USE1_SOP2_ASRC1_COMP,	/* GLES2_BLENDFACTOR_ONE */
	GLES2_USE1_SOP2_ASEL1(SRC1ALPHA),									/* GLES2_BLENDFACTOR_SRCCOLOR */
	GLES2_USE1_SOP2_ASEL1(SRC1ALPHA)	| GLES2_USE1_SOP2_ASRC1_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_SRCCOLOR */
	GLES2_USE1_SOP2_ASEL1(SRC1ALPHA),									/* GLES2_BLENDFACTOR_SRCALPHA */
	GLES2_USE1_SOP2_ASEL1(SRC1ALPHA)	| GLES2_USE1_SOP2_ASRC1_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_SRCALPHA */
	GLES2_USE1_SOP2_ASEL1(SRC2ALPHA),									/* GLES2_BLENDFACTOR_DSTALPHA */
	GLES2_USE1_SOP2_ASEL1(SRC2ALPHA)	| GLES2_USE1_SOP2_ASRC1_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_DSTALPHA */
	GLES2_USE1_SOP2_ASEL1(SRC2ALPHA),									/* GLES2_BLENDFACTOR_DSTCOLOR */
	GLES2_USE1_SOP2_ASEL1(SRC2ALPHA)	| GLES2_USE1_SOP2_ASRC1_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_DSTCOLOR */
	GLES2_USE1_SOP2_ASEL1(ZERO)			| GLES2_USE1_SOP2_ASRC1_COMP,	/* GLES2_BLENDFACTOR_SRCALPHA_SATURATE */ 
	GLES2_USE1_SOP2_ASEL1(ZERO),		/* Cannot be encoded by SOP2 ->    GLES2_BLENDFACTOR_CONSTCOLOR */
	GLES2_USE1_SOP2_ASEL1(ZERO),		/* Cannot be encoded by SOP2 ->    GLES2_BLENDFACTOR_ONEMINUS_CONSTCOLOR */
	GLES2_USE1_SOP2_ASEL1(ZERO),		/* Cannot be encoded by SOP2 ->    GLES2_BLENDFACTOR_CONSTALPHA */
	GLES2_USE1_SOP2_ASEL1(ZERO),		/* Cannot be encoded by SOP2 ->    GLES2_BLENDFACTOR_ONEMINUS_CONSTALPHA */
	GLES2_USE1_SOP2_ASEL1(ZERO)			| GLES2_USE1_SOP2_ASRC1_COMP,	/* GLES2_BLENDFACTOR_DSTALPHA_SATURATE */
};

static const IMG_UINT32 aui32SOP2AlphaSrc2Sel[] = {
	GLES2_USE1_SOP2_ASEL2(ZERO),										/* GLES2_BLENDFACTOR_ZERO */
	GLES2_USE1_SOP2_ASEL2(ZERO)			| GLES2_USE1_SOP2_ASRC2_COMP,	/* GLES2_BLENDFACTOR_ONE */
	GLES2_USE1_SOP2_ASEL2(SRC1ALPHA),									/* GLES2_BLENDFACTOR_SRCCOLOR */
	GLES2_USE1_SOP2_ASEL2(SRC1ALPHA)	| GLES2_USE1_SOP2_ASRC2_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_SRCCOLOR */
	GLES2_USE1_SOP2_ASEL2(SRC1ALPHA),									/* GLES2_BLENDFACTOR_SRCALPHA */
	GLES2_USE1_SOP2_ASEL2(SRC1ALPHA)	| GLES2_USE1_SOP2_ASRC2_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_SRCALPHA */
	GLES2_USE1_SOP2_ASEL2(SRC2ALPHA),									/* GLES2_BLENDFACTOR_DSTALPHA */
	GLES2_USE1_SOP2_ASEL2(SRC2ALPHA)	| GLES2_USE1_SOP2_ASRC2_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_DSTALPHA */
	GLES2_USE1_SOP2_ASEL2(SRC2ALPHA),									/* GLES2_BLENDFACTOR_DSTCOLOR */
	GLES2_USE1_SOP2_ASEL2(SRC2ALPHA)	| GLES2_USE1_SOP2_ASRC2_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_DSTCOLOR */
	GLES2_USE1_SOP2_ASEL2(ZERO)			| GLES2_USE1_SOP2_ASRC2_COMP,	/* GLES2_BLENDFACTOR_SRCALPHA_SATURATE */
	GLES2_USE1_SOP2_ASEL2(ZERO),		/* Cannot be encoded by SOP2 ->    GLES2_BLENDFACTOR_CONSTCOLOR */
	GLES2_USE1_SOP2_ASEL2(ZERO),		/* Cannot be encoded by SOP2 ->    GLES2_BLENDFACTOR_ONEMINUS_CONSTCOLOR */
	GLES2_USE1_SOP2_ASEL2(ZERO),		/* Cannot be encoded by SOP2 ->    GLES2_BLENDFACTOR_CONSTALPHA */
	GLES2_USE1_SOP2_ASEL2(ZERO),		/* Cannot be encoded by SOP2 ->    GLES2_BLENDFACTOR_ONEMINUS_CONSTALPHA */
	GLES2_USE1_SOP2_ASEL2(ZERO)			| GLES2_USE1_SOP2_ASRC2_COMP,	/* GLES2_BLENDFACTOR_DSTALPHA_SATURATE */
};

/*****************************************************************************/

/* 
 * Array that maps Blend factor defines
 * to Eurasia SOP2WM register types and numbers
 */
static const IMG_UINT32 aui32SOPWMSrc1[2][16] = {
{	
	GLES2_USE_SRC1(PRIMATTR, 0),										/* GLES2_BLENDFACTOR_ZERO */
	GLES2_USE_SRC1(PRIMATTR, 0),										/* GLES2_BLENDFACTOR_ONE */
	GLES2_USE_SRC1(PRIMATTR, 0),										/* GLES2_BLENDFACTOR_SRCCOLOR */
	GLES2_USE_SRC1(PRIMATTR, 0),										/* GLES2_BLENDFACTOR_ONEMINUS_SRCCOLOR */
	GLES2_USE_SRC1(PRIMATTR, 0),										/* GLES2_BLENDFACTOR_SRCALPHA */
	GLES2_USE_SRC1(PRIMATTR, 0),										/* GLES2_BLENDFACTOR_ONEMINUS_SRCALPHA */
	GLES2_USE_SRC1(OUTPUT, 0),											/* GLES2_BLENDFACTOR_DSTALPHA */
	GLES2_USE_SRC1(OUTPUT, 0),											/* GLES2_BLENDFACTOR_ONEMINUS_DSTALPHA */
	GLES2_USE_SRC1(OUTPUT, 0),											/* GLES2_BLENDFACTOR_DSTCOLOR */
	GLES2_USE_SRC1(OUTPUT, 0),											/* GLES2_BLENDFACTOR_ONEMINUS_DSTCOLOR */
	GLES2_USE_SRC1(OUTPUT, 0),											/* GLES2_BLENDFACTOR_SRCALPHASAT */
	GLES2_USE_SRC1(SECATTR, GLES2_FRAGMENT_SECATTR_FBBLENDCONST),		/* GLES2_BLENDFACTOR_CONSTCOLOR */
	GLES2_USE_SRC1(SECATTR, GLES2_FRAGMENT_SECATTR_FBBLENDCONST),		/* GLES2_BLENDFACTOR_ONEMINUS_CONSTCOLOR */
	GLES2_USE_SRC1(SECATTR, GLES2_FRAGMENT_SECATTR_FBBLENDCONST),		/* GLES2_BLENDFACTOR_CONSTALPHA */
	GLES2_USE_SRC1(SECATTR, GLES2_FRAGMENT_SECATTR_FBBLENDCONST),		/* GLES2_BLENDFACTOR_ONEMINUS_CONSTALPHA */
	GLES2_USE_SRC1(OUTPUT, 0),                                          /* GLES2_BLENDFACTOR_DSTALPHA_SATURATE */
},
{
	GLES2_USE_SRC1(TEMP, 0),											/* GLES2_BLENDFACTOR_ZERO */
	GLES2_USE_SRC1(TEMP, 0),											/* GLES2_BLENDFACTOR_ONE */
	GLES2_USE_SRC1(TEMP, 0),											/* GLES2_BLENDFACTOR_SRCCOLOR */
	GLES2_USE_SRC1(TEMP, 0),											/* GLES2_BLENDFACTOR_ONEMINUS_SRCCOLOR */
	GLES2_USE_SRC1(TEMP, 0),											/* GLES2_BLENDFACTOR_SRCALPHA */
	GLES2_USE_SRC1(TEMP, 0),											/* GLES2_BLENDFACTOR_ONEMINUS_SRCALPHA */
	GLES2_USE_SRC1(OUTPUT, 0),											/* GLES2_BLENDFACTOR_DSTALPHA */
	GLES2_USE_SRC1(OUTPUT, 0),											/* GLES2_BLENDFACTOR_ONEMINUS_DSTALPHA */
	GLES2_USE_SRC1(OUTPUT, 0),											/* GLES2_BLENDFACTOR_DSTCOLOR */
	GLES2_USE_SRC1(OUTPUT, 0),											/* GLES2_BLENDFACTOR_ONEMINUS_DSTCOLOR */
	GLES2_USE_SRC1(OUTPUT, 0),											/* GLES2_BLENDFACTOR_SRCALPHASAT */
	GLES2_USE_SRC1(SECATTR, GLES2_FRAGMENT_SECATTR_FBBLENDCONST),		/* GLES2_BLENDFACTOR_CONSTCOLOR */
	GLES2_USE_SRC1(SECATTR, GLES2_FRAGMENT_SECATTR_FBBLENDCONST),		/* GLES2_BLENDFACTOR_ONEMINUS_CONSTCOLOR */
	GLES2_USE_SRC1(SECATTR, GLES2_FRAGMENT_SECATTR_FBBLENDCONST),		/* GLES2_BLENDFACTOR_CONSTALPHA */
	GLES2_USE_SRC1(SECATTR, GLES2_FRAGMENT_SECATTR_FBBLENDCONST),		/* GLES2_BLENDFACTOR_ONEMINUS_CONSTALPHA */
	GLES2_USE_SRC1(OUTPUT, 0),                                          /* GLES2_BLENDFACTOR_DSTALPHA_SATURATE */
}
};

static const IMG_UINT32 aui32SOPWMSrc2[] = {
	GLES2_USE_SRC2(PRIMATTR, 0),										/* GLES2_BLENDFACTOR_ZERO */
	GLES2_USE_SRC2(PRIMATTR, 0),										/* GLES2_BLENDFACTOR_ONE */
	GLES2_USE_SRC2(PRIMATTR, 0),										/* GLES2_BLENDFACTOR_SRCCOLOR */
	GLES2_USE_SRC2(PRIMATTR, 0),										/* GLES2_BLENDFACTOR_ONEMINUS_SRCCOLOR */
	GLES2_USE_SRC2(PRIMATTR, 0),										/* GLES2_BLENDFACTOR_SRCALPHA */
	GLES2_USE_SRC2(PRIMATTR, 0),										/* GLES2_BLENDFACTOR_ONEMINUS_SRCALPHA */
	GLES2_USE_SRC2(OUTPUT, 0),											/* GLES2_BLENDFACTOR_DSTALPHA */
	GLES2_USE_SRC2(OUTPUT, 0),											/* GLES2_BLENDFACTOR_ONEMINUS_DSTALPHA */
	GLES2_USE_SRC2(OUTPUT, 0),											/* GLES2_BLENDFACTOR_DSTCOLOR */
	GLES2_USE_SRC2(OUTPUT, 0),											/* GLES2_BLENDFACTOR_ONEMINUS_DSTCOLOR */
	GLES2_USE_SRC2(OUTPUT, 0),											/* GLES2_BLENDFACTOR_SRCALPHASAT */
	GLES2_USE_SRC2(SECATTR, GLES2_FRAGMENT_SECATTR_FBBLENDCONST),		/* GLES2_BLENDFACTOR_CONSTCOLOR */
	GLES2_USE_SRC2(SECATTR, GLES2_FRAGMENT_SECATTR_FBBLENDCONST),		/* GLES2_BLENDFACTOR_ONEMINUS_CONSTCOLOR */
	GLES2_USE_SRC2(SECATTR, GLES2_FRAGMENT_SECATTR_FBBLENDCONST),		/* GLES2_BLENDFACTOR_CONSTALPHA */
	GLES2_USE_SRC2(SECATTR, GLES2_FRAGMENT_SECATTR_FBBLENDCONST),		/* GLES2_BLENDFACTOR_ONEMINUS_CONSTALPHA */ 
	GLES2_USE_SRC2(OUTPUT, 0),                                          /* GLES2_BLENDFACTOR_DSTALPHA_SATURATE */
};

/* 
 * Array that maps Blend factor defines
 * to Eurasia SOP2WM source selects
 */
static const IMG_UINT32 aui32SOPWMSrc1Sel[] = {
	GLES2_USE1_SOPWM_SEL1(ZERO),										/* GLES2_BLENDFACTOR_ZERO */
	GLES2_USE1_SOPWM_SEL1(ZERO)			| GLES2_USE1_SOPWM_SRC1_COMP,	/* GLES2_BLENDFACTOR_ONE */
	GLES2_USE1_SOPWM_SEL1(SRC1),										/* GLES2_BLENDFACTOR_SRCCOLOR */
	GLES2_USE1_SOPWM_SEL1(SRC1)			| GLES2_USE1_SOPWM_SRC1_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_SRCCOLOR */
	GLES2_USE1_SOPWM_SEL1(SRC1ALPHA),									/* GLES2_BLENDFACTOR_SRCALPHA */
	GLES2_USE1_SOPWM_SEL1(SRC1ALPHA)	| GLES2_USE1_SOPWM_SRC1_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_SRCALPHA */
	GLES2_USE1_SOPWM_SEL1(SRC2ALPHA),									/* GLES2_BLENDFACTOR_DSTALPHA */
	GLES2_USE1_SOPWM_SEL1(SRC2ALPHA)	| GLES2_USE1_SOPWM_SRC1_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_DSTALPHA */
	GLES2_USE1_SOPWM_SEL1(SRC2),										/* GLES2_BLENDFACTOR_DSTCOLOR */
	GLES2_USE1_SOPWM_SEL1(SRC2)			| GLES2_USE1_SOPWM_SRC1_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_DSTCOLOR */
	GLES2_USE1_SOPWM_SEL1(MINSRC1A1MSRC2A),								/* GLES2_BLENDFACTOR_SRCALPHASAT */
	GLES2_USE1_SOPWM_SEL1(SRC2),										/* GLES2_BLENDFACTOR_CONSTCOLOR */
	GLES2_USE1_SOPWM_SEL1(SRC2)			| GLES2_USE1_SOPWM_SRC1_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_CONSTCOLOR */
	GLES2_USE1_SOPWM_SEL1(SRC2ALPHA),									/* GLES2_BLENDFACTOR_CONSTALPHA */
	GLES2_USE1_SOPWM_SEL1(SRC2ALPHA)	| GLES2_USE1_SOPWM_SRC1_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_CONSTALPHA */
	GLES2_USE1_SOPWM_SEL1(MINSRC1A1MSRC2A) | GLES2_USE1_SOPWM_SRC1_COMP /* GLES2_BLENDFACTOR_DSTALPHA_SATURATE */
};

static const IMG_UINT32 aui32SOPWMSrc2Sel[] = {
	GLES2_USE1_SOPWM_SEL2(ZERO),										/* GLES2_BLENDFACTOR_ZERO */
	GLES2_USE1_SOPWM_SEL2(ZERO)			| GLES2_USE1_SOPWM_SRC2_COMP,	/* GLES2_BLENDFACTOR_ONE */
	GLES2_USE1_SOPWM_SEL2(SRC1),										/* GLES2_BLENDFACTOR_SRCCOLOR */
	GLES2_USE1_SOPWM_SEL2(SRC1)			| GLES2_USE1_SOPWM_SRC2_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_SRCCOLOR */
	GLES2_USE1_SOPWM_SEL2(SRC1ALPHA),									/* GLES2_BLENDFACTOR_SRCALPHA */
	GLES2_USE1_SOPWM_SEL2(SRC1ALPHA)	| GLES2_USE1_SOPWM_SRC2_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_SRCALPHA */
	GLES2_USE1_SOPWM_SEL2(SRC2ALPHA),									/* GLES2_BLENDFACTOR_DSTALPHA */
	GLES2_USE1_SOPWM_SEL2(SRC2ALPHA)	| GLES2_USE1_SOPWM_SRC2_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_DSTALPHA */
	GLES2_USE1_SOPWM_SEL2(SRC2),										/* GLES2_BLENDFACTOR_DSTCOLOR */
	GLES2_USE1_SOPWM_SEL2(SRC2)			| GLES2_USE1_SOPWM_SRC2_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_DSTCOLOR */
	GLES2_USE1_SOPWM_SEL2(MINSRC1A1MSRC2A),								/* GLES2_BLENDFACTOR_SRCALPHASAT */
	GLES2_USE1_SOPWM_SEL2(SRC1),										/* GLES2_BLENDFACTOR_CONSTCOLOR */
	GLES2_USE1_SOPWM_SEL2(SRC1)			| GLES2_USE1_SOPWM_SRC2_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_CONSTCOLOR */
	GLES2_USE1_SOPWM_SEL2(SRC1ALPHA),									/* GLES2_BLENDFACTOR_CONSTALPHA */
	GLES2_USE1_SOPWM_SEL2(SRC1ALPHA)	| GLES2_USE1_SOPWM_SRC2_COMP,	/* GLES2_BLENDFACTOR_ONEMINUS_CONSTALPHA */
	GLES2_USE1_SOPWM_SEL2(MINSRC1A1MSRC2A)| GLES2_USE1_SOPWM_SRC2_COMP  /* GLES2_BLENDFACTOR_DSTALPHA_SATURATE */
};


#if defined(DEBUG)
	IMG_INTERNAL IMG_VOID DumpVertexShader(GLES2Context *gc, GLES2USEShaderVariant *psVertexVariant, IMG_UINT32 ui32CodeSizeInUSEInsts);
	IMG_INTERNAL IMG_VOID DumpFragmentShader(GLES2Context *gc, GLES2USEShaderVariant *psFragmentVariant, IMG_UINT32 ui32CodeSizeInUSEInsts);
#endif /* defined(DEBUG) */

/*****************************************************************************
 Function Name	: EncodeTwoSourceBlend
 Inputs			: Blend op, sources, Src 2 field for SOP2 instruction
 Outputs		: USE code in memory pointed to by pui32Code
 Returns		: Number of bytes for USE instructions
 Description	: Generates USE code to perform framebuffer blending.
				  using SOP2 instruction.
				  Requires Output0 to contain destination and PA0 to contain 
				  source, both in 8888 format. Stores result in PA0.
*****************************************************************************/
static IMG_UINT32 EncodeTwoSourceBlend(IMG_UINT32 ui32ColBlendEquation,
									   IMG_UINT32 ui32AlphaBlendEquation,
									   IMG_UINT32 ui32ColSrc,
									   IMG_UINT32 ui32ColDst,
									   IMG_UINT32 ui32AlphaSrc,
									   IMG_UINT32 ui32AlphaDst,
									   IMG_UINT32 ui32Src1,
									   IMG_UINT32 ui32Src2,
									   IMG_UINT32 ui32Dst,
									   IMG_UINT32 *pui32Code)
{
	IMG_UINT32 ui32OutputColSrc, ui32OutputColDst, ui32OutputAlphaSrc, ui32OutputAlphaDst;
	IMG_UINT32 ui32Swap;
	IMG_BOOL bNegateAlphaResult = IMG_FALSE;

	/* Reverse source and dest arguments for reverse subtract B-A since 
	   HW only does A-B */
	if(ui32ColBlendEquation == GLES2_BLENDFUNC_REVSUBTRACT)
	{
		IMG_UINT32 ui32Src1Bank, ui32Src1RegNum, ui32Src1BankExtend;
		IMG_UINT32 ui32Src2Bank, ui32Src2RegNum, ui32Src2BankExtend;

		/* Flip src and dst color factors */
		ui32Swap = ui32ColSrc;
		ui32ColSrc = aui32SOP2FlipSel[ui32ColDst];
		ui32ColDst = aui32SOP2FlipSel[ui32Swap];

		/* Flip src and dst alpha factors */
		ui32Swap = ui32AlphaSrc;
		ui32AlphaSrc = aui32SOP2FlipSel[ui32AlphaDst];
		ui32AlphaDst = aui32SOP2FlipSel[ui32Swap];

		/* Flip src and dst themselves */
		ui32Src1Bank   = (ui32Src1 & ~EURASIA_USE0_S1BANK_CLRMSK) >> EURASIA_USE0_S1BANK_SHIFT;
		ui32Src1RegNum = (ui32Src1 & ~EURASIA_USE0_SRC1_CLRMSK) >> EURASIA_USE0_SRC1_SHIFT;
		ui32Src1BankExtend = (ui32Src1 & EURASIA_USE1_S1BEXT);

		ui32Src2Bank   = (ui32Src2 & ~EURASIA_USE0_S2BANK_CLRMSK) >> EURASIA_USE0_S2BANK_SHIFT;
		ui32Src2RegNum = (ui32Src2 & ~EURASIA_USE0_SRC2_CLRMSK) >> EURASIA_USE0_SRC2_SHIFT;
		ui32Src2BankExtend = (ui32Src2 & EURASIA_USE1_S2BEXT);

		ui32Src1 = (ui32Src2Bank << EURASIA_USE0_S1BANK_SHIFT) | 
					(ui32Src2RegNum << EURASIA_USE0_SRC1_SHIFT) |
					(ui32Src2BankExtend ? EURASIA_USE1_S1BEXT : 0);

		ui32Src2 = (ui32Src1Bank << EURASIA_USE0_S2BANK_SHIFT) | 
					(ui32Src1RegNum << EURASIA_USE0_SRC2_SHIFT) |
					(ui32Src1BankExtend ? EURASIA_USE1_S2BEXT : 0);
		
		/* If alpha equation is simply SUBTRACT,
		   negate the alpha result: -(B-A) = A-B */
		if(ui32AlphaBlendEquation == GLES2_BLENDFUNC_SUBTRACT)
		{
			bNegateAlphaResult = IMG_TRUE;
		}
	}
	else if(ui32AlphaBlendEquation == GLES2_BLENDFUNC_REVSUBTRACT)
	{
		/* Colour equation is not REVERSE SUBTRACT,
		   but alpha equation is, so negate alpha 
		   result -(A-B) = B-A */
		bNegateAlphaResult = IMG_TRUE;
	}

	/* Setup instruction basics */
	pui32Code[0] = GLES2_USE0_DST_NUM(0)      |
				   ui32Src1	                  |
				   ui32Src2					  |
				   (bNegateAlphaResult ? ((IMG_UINT32)EURASIA_USE0_SOP2_ADSTMOD_NEGATE << EURASIA_USE0_SOP2_ADSTMOD_SHIFT) : 0);
	
	pui32Code[1] = GLES2_USE1_OP(SOP2)        |
				   EURASIA_USE1_SKIPINV       |
				   ui32Dst;

	/* Setup colour and alpha ops */
	pui32Code[0] |= aui32SOP2ColBlendOp[ui32ColBlendEquation] | aui32SOP2AlphaBlendOp[ui32AlphaBlendEquation];

	/* Setup color source */
	ui32OutputColSrc = aui32SOP2ColSrc1Sel[ui32ColSrc];

	/* Setup color dest */
	ui32OutputColDst = aui32SOP2ColSrc2Sel[ui32ColDst];

	/* Setup alpha source */
	ui32OutputAlphaSrc = aui32SOP2AlphaSrc1Sel[ui32AlphaSrc];

	/* Setup alpha dest */
	ui32OutputAlphaDst = aui32SOP2AlphaSrc2Sel[ui32AlphaDst];

	pui32Code[1] |= ui32OutputColSrc | ui32OutputColDst | ui32OutputAlphaSrc | ui32OutputAlphaDst;

	return 8;
}

/*****************************************************************************
 Function Name	: EncodeThreeSourceBlend
 Inputs			: Blend op, sources
 Outputs		: USE code in memory pointed to by pui32Code
 Returns		: Number of bytes for USE instructions
 Description	: Generates USE code to perform framebuffer blending.
				  using mostly SOPWM instructions.
				  Requires Output0 to contain destination and PA0 to contain 
				  source, both in 8888 format. Stores result in PA0.
*****************************************************************************/
static IMG_UINT32 EncodeThreeSourceBlend(IMG_UINT32 ui32ColBlendEquation,
										IMG_UINT32 ui32AlphaBlendEquation,
										IMG_UINT32 ui32ColSrc,
										IMG_UINT32 ui32ColDst,
										IMG_UINT32 ui32AlphaSrc,
										IMG_UINT32 ui32AlphaDst,
										IMG_UINT32 ui32Dst,
										IMG_UINT32 ui32SrcReg,
										IMG_BOOL bSrcIsTemp,
										IMG_UINT32 *pui32Code)
{
	IMG_UINT32 ui32TmpSrc;
	IMG_UINT32 ui32Src1 = bSrcIsTemp ? GLES2_USE_SRC1(TEMP, ui32SrcReg) : GLES2_USE_SRC1(PRIMATTR, ui32SrcReg);
	IMG_UINT32 ui32Mask = bSrcIsTemp ? GLES2_USE1_SOPWM_WRITEMASK(EURASIA_USE1_SOP2WM_WRITEMASK_RGB) :
										GLES2_USE1_SOPWM_WRITEMASK(EURASIA_USE1_SOP2WM_WRITEMASK_RGBA);

	/* Encode the destination colour blend: Temp1.rgba = {Src.rgb|BlendColor.rgb} * Zero + Dest.rgb * ui32ColDst */
	/* NOTE: we use a dest mask of rgba even though the alpha will be overwritten in the next instruction, so that */
	/* we can get matching outfiles */
	ui32TmpSrc = aui32SOPWMSrc1[bSrcIsTemp ? 1 : 0][ui32ColDst];

	/* Use correct reg number for true input src */
	if((ui32TmpSrc == GLES2_USE_SRC1(TEMP, 0)) || (ui32TmpSrc == GLES2_USE_SRC1(PRIMATTR, 0)))
	{
		ui32TmpSrc |= GLES2_USE0_SRC1_NUM(ui32SrcReg);
	}

	pui32Code[0] = GLES2_USE0_DST_NUM(1)											|
				   ui32TmpSrc									                    |
	               GLES2_USE_SRC2(OUTPUT, 0);

	pui32Code[1] = GLES2_USE1_OP(SOPWM)                                             |
	               EURASIA_USE1_SKIPINV                                             |
	               GLES2_USE1_SOPWM_COP(ADD)                                        |
	               GLES2_USE1_SOPWM_AOP(ADD)                                        |
	               aui32SOPWMSrc1Sel[GLES2_BLENDFACTOR_ZERO]                        |
	               aui32SOPWMSrc2Sel[ui32ColDst]                                    |
	               GLES2_USE1_SOPWM_WRITEMASK(EURASIA_USE1_SOP2WM_WRITEMASK_RGBA)   |
	               GLES2_USE1_DST_TYPE(TEMP);

	/* Encode the destination alpha blend: Temp1.a = {Src.a|BlendColor.a} * Zero + Dest.a * ui32AlphaDst */
	ui32TmpSrc = aui32SOPWMSrc1[bSrcIsTemp ? 1 : 0][ui32AlphaDst];

	/* Use correct reg number for true input src */
	if((ui32TmpSrc == GLES2_USE_SRC1(TEMP, 0)) || (ui32TmpSrc == GLES2_USE_SRC1(PRIMATTR, 0)))
	{
		ui32TmpSrc |= GLES2_USE0_SRC1_NUM(ui32SrcReg);
	}

	pui32Code[2] = GLES2_USE0_DST_NUM(1)											|
				   ui32TmpSrc									                    |
	               GLES2_USE_SRC2(OUTPUT, 0);

	pui32Code[3] = GLES2_USE1_OP(SOPWM)                                             |
	               EURASIA_USE1_SKIPINV                                             |
	               GLES2_USE1_SOPWM_COP(ADD)                                        |
	               GLES2_USE1_SOPWM_AOP(ADD)                                        |
	               aui32SOPWMSrc1Sel[GLES2_BLENDFACTOR_ZERO]                        |
	               aui32SOPWMSrc2Sel[ui32AlphaDst]                                  |
	               GLES2_USE1_SOPWM_WRITEMASK(EURASIA_USE1_SOP2WM_WRITEMASK_A)      |
	               GLES2_USE1_DST_TYPE(TEMP);

	/* Encode the Source colour blend: Temp0.rgba = Src.rgb * ui32ColSrc + {Dst.rgb|BlendColor.rgb} * Zero      */
	/* NOTE: if the source color is in Temp0, it will be overwritten. Do not expect it to have the original value */

	ui32TmpSrc = aui32SOPWMSrc2[ui32ColSrc];

	/* Use correct reg number for true input src */
	if((ui32TmpSrc == GLES2_USE_SRC2(TEMP, 0)) || (ui32TmpSrc == GLES2_USE_SRC2(PRIMATTR, 0)))
	{
		ui32TmpSrc |= GLES2_USE0_SRC2_NUM(ui32SrcReg);
	}

	pui32Code[4] = GLES2_USE0_DST_NUM(0)                                            |
					ui32Src1														|
					ui32TmpSrc;

	pui32Code[5] = GLES2_USE1_OP(SOPWM)                                             |
					EURASIA_USE1_SKIPINV                                            |
					GLES2_USE1_SOPWM_COP(ADD)                                       |
					GLES2_USE1_SOPWM_AOP(ADD)                                       |
					aui32SOPWMSrc1Sel[ui32ColSrc]                                   |
					aui32SOPWMSrc2Sel[GLES2_BLENDFACTOR_ZERO]                       |
					ui32Mask														|
					GLES2_USE1_DST_TYPE(TEMP);

	/* Encode the Source alpha blend: Temp0.a = Src.a * ui32AlphaSrc + {Dst.a|BlendColor.a} * Zero */
	ui32TmpSrc = aui32SOPWMSrc2[ui32AlphaSrc];

	/* Use correct reg number for true input src */
	if((ui32TmpSrc == GLES2_USE_SRC2(TEMP, 0)) || (ui32TmpSrc == GLES2_USE_SRC2(PRIMATTR, 0)))
	{
		ui32TmpSrc |= GLES2_USE0_SRC2_NUM(ui32SrcReg);
	}

	pui32Code[6] = GLES2_USE0_DST_NUM(0)                                            |
	               ui32Src1															|
	               ui32TmpSrc;

	pui32Code[7] = GLES2_USE1_OP(SOPWM)                                             |
	               EURASIA_USE1_SKIPINV                                             |
	               GLES2_USE1_SOPWM_COP(ADD)                                        |
	               GLES2_USE1_SOPWM_AOP(ADD)                                        |
	               aui32SOPWMSrc1Sel[ui32AlphaSrc]                                  |
	               aui32SOPWMSrc2Sel[GLES2_BLENDFACTOR_ZERO]                        |
	               GLES2_USE1_SOPWM_WRITEMASK(EURASIA_USE1_SOP2WM_WRITEMASK_A)      |
	               GLES2_USE1_DST_TYPE(TEMP);

	/* Now encode the requested blend equation, using Temp1 for Src2 */
	return 32 + EncodeTwoSourceBlend(ui32ColBlendEquation,
									ui32AlphaBlendEquation,
									GLES2_BLENDFACTOR_ONE,
									GLES2_BLENDFACTOR_ONE,
									GLES2_BLENDFACTOR_ONE,
									GLES2_BLENDFACTOR_ONE,
									GLES2_USE_SRC1(TEMP, 0),
									GLES2_USE_SRC2(TEMP, 1),
									ui32Dst,
									&pui32Code[8]);
}

/***********************************************************************************
 Function Name      : IsBlendNone
 Inputs             : ui32BlendFactor, ui32BlendEquation
 Outputs            : -
 Returns            : Is a blend not a blend!
 Description        : UTILITY: Checks triviality of a blend
************************************************************************************/
static IMG_BOOL IsBlendNone(IMG_UINT32 ui32BlendFactor, IMG_UINT32 ui32BlendEquation)
{
	/* Looking for [Zero*src + One*dst] or [One*dst - Zero*src] */
	if(((ui32BlendEquation & GLES2_BLENDFUNC_RGB_MASK) == (GLES2_BLENDFUNC_SUBTRACT << GLES2_BLENDFUNC_RGB_SHIFT)) ||
		((ui32BlendEquation & GLES2_BLENDFUNC_ALPHA_MASK) == (GLES2_BLENDFUNC_SUBTRACT << GLES2_BLENDFUNC_ALPHA_SHIFT)))
	{
		return IMG_FALSE;
	}

	if(ui32BlendFactor == (	(GLES2_BLENDFACTOR_ZERO << GLES2_BLENDFACTOR_RGBSRC_SHIFT)	|
							(GLES2_BLENDFACTOR_ZERO << GLES2_BLENDFACTOR_ALPHASRC_SHIFT)|
							(GLES2_BLENDFACTOR_ONE << GLES2_BLENDFACTOR_RGBDST_SHIFT)	|
							(GLES2_BLENDFACTOR_ONE << GLES2_BLENDFACTOR_ALPHADST_SHIFT)))
	{
		return IMG_TRUE;
	}

	return IMG_FALSE;
}

/***********************************************************************************
 Function Name      : IsBlendTranslucent
 Inputs             : ui32ColSrc, ui32ColDst, ui32AlphaSrc, ui32AlphaDst
 Outputs            : -
 Returns            : Is a blend translucent
 Description        : UTILITY: Checks translucency of a blend
************************************************************************************/

static IMG_BOOL IsBlendTranslucent(IMG_UINT32 ui32ColSrc, IMG_UINT32 ui32ColDst, 
								   IMG_UINT32 ui32AlphaSrc, IMG_UINT32 ui32AlphaDst)
{
	IMG_BOOL bTranslucent;

	/* An object is translucent if the destination operands are not ZERO, OR one
	   of the source operands is a destination colour/alpha */ 
	
	bTranslucent =  ((ui32ColDst != GLES2_BLENDFACTOR_ZERO) || (ui32AlphaDst != GLES2_BLENDFACTOR_ZERO) ||
					((ui32ColSrc >= GLES2_BLENDFACTOR_DSTALPHA) &&
					(ui32ColSrc <= GLES2_BLENDFACTOR_SRCALPHA_SATURATE)) ||
					((ui32AlphaSrc >= GLES2_BLENDFACTOR_DSTALPHA) &&
					(ui32AlphaSrc <= GLES2_BLENDFACTOR_SRCALPHA_SATURATE))) ? IMG_TRUE : IMG_FALSE;

	return bTranslucent;
}


/*****************************************************************************
 Function Name	: ReplaceDestinationAlpha
 Inputs			: Blend source
 Outputs		: Modified blend source
 Description	: Helper function to replace uses of DSTA and INVDSTA
				  when the framebuffer has no alpha bits
*****************************************************************************/
static IMG_VOID ReplaceDestinationAlpha(IMG_UINT32 *pui32Src)
{
	if(*pui32Src == GLES2_BLENDFACTOR_DSTALPHA)
	{
		*pui32Src = GLES2_BLENDFACTOR_ONE;
	}

	if(*pui32Src == GLES2_BLENDFACTOR_ONEMINUS_DSTALPHA)
	{
		*pui32Src = GLES2_BLENDFACTOR_ZERO;
	}
}

/*****************************************************************************
 Function Name	: GetFBBlendType
 Inputs			: gc
 Outputs		: Is blend a noop, translucent or uses a constant color
 Description	: -
*****************************************************************************/
IMG_INTERNAL IMG_VOID GetFBBlendType(GLES2Context *gc, IMG_BOOL *pbIsBlendNone, 
									 IMG_BOOL *pbIsBlendTranslucent,
									 IMG_BOOL *pbHasConstantColorBlend)
{
	IMG_UINT32 ui32ColSrc, ui32ColDst, ui32AlphaSrc, ui32AlphaDst;

	ui32ColSrc = (gc->sState.sRaster.ui32BlendFactor & GLES2_BLENDFACTOR_RGBSRC_MASK) >> GLES2_BLENDFACTOR_RGBSRC_SHIFT;
	ui32ColDst = (gc->sState.sRaster.ui32BlendFactor & GLES2_BLENDFACTOR_RGBDST_MASK) >> GLES2_BLENDFACTOR_RGBDST_SHIFT;
	ui32AlphaSrc = (gc->sState.sRaster.ui32BlendFactor & GLES2_BLENDFACTOR_ALPHASRC_MASK) >> GLES2_BLENDFACTOR_ALPHASRC_SHIFT;
	ui32AlphaDst = (gc->sState.sRaster.ui32BlendFactor & GLES2_BLENDFACTOR_ALPHADST_MASK) >> GLES2_BLENDFACTOR_ALPHADST_SHIFT;

	if(!gc->psMode->ui32AlphaBits)
	{
		/* Framebuffer does not have alpha, so DSTA should be treated as 1 */
		ReplaceDestinationAlpha(&ui32ColSrc);
		ReplaceDestinationAlpha(&ui32ColDst);
		ReplaceDestinationAlpha(&ui32AlphaSrc);
		ReplaceDestinationAlpha(&ui32AlphaDst);
	}

	*pbIsBlendNone = IsBlendNone(gc->sState.sRaster.ui32BlendFactor, gc->sState.sRaster.ui32BlendEquation);
	*pbIsBlendTranslucent = IsBlendTranslucent(ui32ColSrc, ui32ColDst, ui32AlphaSrc, ui32AlphaDst);
	*pbHasConstantColorBlend = IMG_FALSE;

		/* Check for blends that use the constant blend colour */
	if(((ui32ColSrc >= GLES2_BLENDFACTOR_CONSTCOLOR) && (ui32ColSrc <= GLES2_BLENDFACTOR_ONEMINUS_CONSTALPHA))	 ||
	   ((ui32ColDst >= GLES2_BLENDFACTOR_CONSTCOLOR) && (ui32ColDst <= GLES2_BLENDFACTOR_ONEMINUS_CONSTALPHA))	 ||
	   ((ui32AlphaSrc >= GLES2_BLENDFACTOR_CONSTCOLOR) && (ui32AlphaSrc <= GLES2_BLENDFACTOR_ONEMINUS_CONSTALPHA)) ||
	   ((ui32AlphaDst >= GLES2_BLENDFACTOR_CONSTCOLOR) && (ui32AlphaDst <= GLES2_BLENDFACTOR_ONEMINUS_CONSTALPHA)))
	{
		*pbHasConstantColorBlend = IMG_TRUE;
	}
}


/*****************************************************************************
 Function Name	: CreateFBBlendUSECode
 Inputs			: Blend op, sources, colour, 
 Outputs		: USE code in memory pointed to by pui32Code,
				  number of temporary registers used.
 Returns		: Number of bytes for USE instructions
 Description	: Generates USE code to perform framebuffer blending.
				  Requires Output0 to contain destination and Temp0 to contain 
				  source, both in 8888 format.
 Limitations	: Does not handle blends that reference constant colours when
				  the equation is MIN or MAX
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 CreateFBBlendUSECode(GLES2Context *gc,
											 IMG_UINT32 ui32SrcReg,
											 IMG_BOOL bColorMask,
											 IMG_BOOL bNeedsISPFeedback,
											 IMG_UINT32 *pui32Code,
											 IMG_UINT32 *pui32NumTempRegsUsed)
{
	IMG_BOOL bConstantColorBlend = IMG_FALSE;
	IMG_BOOL bSrcIsTemp = IMG_FALSE;
	IMG_UINT32 ui32ColSrc, ui32ColDst, ui32AlphaSrc, ui32AlphaDst, ui32ColEquation, ui32AlphaEquation;
	IMG_UINT32 ui32Dst;
	IMG_BOOL bIsBlendNone, bIsBlendTranslucent;

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	PVR_UNREFERENCED_PARAMETER(bNeedsISPFeedback);
#endif


	ui32ColSrc = (gc->sState.sRaster.ui32BlendFactor & GLES2_BLENDFACTOR_RGBSRC_MASK) >> GLES2_BLENDFACTOR_RGBSRC_SHIFT;
	ui32ColDst = (gc->sState.sRaster.ui32BlendFactor & GLES2_BLENDFACTOR_RGBDST_MASK) >> GLES2_BLENDFACTOR_RGBDST_SHIFT;
	ui32AlphaSrc = (gc->sState.sRaster.ui32BlendFactor & GLES2_BLENDFACTOR_ALPHASRC_MASK) >> GLES2_BLENDFACTOR_ALPHASRC_SHIFT;
	ui32AlphaDst = (gc->sState.sRaster.ui32BlendFactor & GLES2_BLENDFACTOR_ALPHADST_MASK) >> GLES2_BLENDFACTOR_ALPHADST_SHIFT;

	ui32ColEquation = (gc->sState.sRaster.ui32BlendEquation & GLES2_BLENDFUNC_RGB_MASK) >> GLES2_BLENDFUNC_RGB_SHIFT;
	ui32AlphaEquation = (gc->sState.sRaster.ui32BlendEquation & GLES2_BLENDFUNC_ALPHA_MASK) >> GLES2_BLENDFUNC_ALPHA_SHIFT;

	if(!gc->psMode->ui32AlphaBits)
	{
		/* Framebuffer does not have alpha, so DSTA should be treated as 1 */
		ReplaceDestinationAlpha(&ui32ColSrc);
		ReplaceDestinationAlpha(&ui32ColDst);
		ReplaceDestinationAlpha(&ui32AlphaSrc);
		ReplaceDestinationAlpha(&ui32AlphaDst);
	}

	GetFBBlendType(gc, &bIsBlendNone, &bIsBlendTranslucent, &bConstantColorBlend);

#if !defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	/* If this is a pure translucent object, the source is in a temp. If it is a translucent punchthrough object,
	   the ATST8 instruction has moved it to a PA.
	 */
	if(gc->psMode->ui32AntiAliasMode && !bNeedsISPFeedback && !bIsBlendNone && (bIsBlendTranslucent || bColorMask))
	{
		bSrcIsTemp = IMG_TRUE;
	}
#endif
	
	if(bColorMask)
	{
		ui32Dst = GLES2_USE1_DST_TYPE(TEMP);
		*pui32NumTempRegsUsed = 1;
	}
	else
	{
		ui32Dst = GLES2_USE1_DST_TYPE(OUTPUT);
		*pui32NumTempRegsUsed = 0;
	}


	if(!bConstantColorBlend)
	{
		return EncodeTwoSourceBlend(ui32ColEquation,
									ui32AlphaEquation,
									ui32ColSrc,
									ui32ColDst,
									ui32AlphaSrc,
									ui32AlphaDst,
									bSrcIsTemp ? GLES2_USE_SRC1(TEMP, ui32SrcReg) : GLES2_USE_SRC1(PRIMATTR, ui32SrcReg),
									GLES2_USE_SRC2(OUTPUT, 0),
									ui32Dst,
									pui32Code);
	}
	else
	{
		/* Blend uses constant colour, so there are three sources: 
		   source, dest and constant.
		   The generated code requires the the use of Temp0&1 */
		*pui32NumTempRegsUsed = 2;

		return EncodeThreeSourceBlend(ui32ColEquation,
									  ui32AlphaEquation,
									  ui32ColSrc,
									  ui32ColDst,
									  ui32AlphaSrc,
									  ui32AlphaDst,
									  ui32Dst,
									  ui32SrcReg,
									  bSrcIsTemp,
									  pui32Code);
	}
}


/*****************************************************************************
 Function Name	: CreateColorMaskUSECode
 Inputs			: gc, ui32SrcReg
 Outputs		: USE code in memory pointed to by puCode
 Returns		: Number of bytes for USE instructions
 Description	: Generates USE code to perform color masking
				  and writes data to output buffer
				  Requires Output0 to contain destination and Temp(n) to contain 
				  source, both in 8888 format.
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 CreateColorMaskUSECode(GLES2Context *gc,
											   IMG_BOOL bSrcInTemp,
											   IMG_UINT32 ui32SrcReg,
											   IMG_UINT32 *puCode)
{
	IMG_UINT32 ui32ColorMask = gc->sState.sRaster.ui32ColorMask;

	if(ui32ColorMask == GLES2_COLORMASK_ALL)
	{
		return 0;
	}

	/* Some color channels are disabled. 
		Use the sum-of-products with mask instruction, which operates on
		8-bit data. Use MAX operation with duplicate sources:
		SOPWM.colormask o0, MAX(t0.rgba, t0.rgba) 
		If MSAA trans, then input to colormask is in temp, not PA
	 */
	if(bSrcInTemp)
	{
		puCode[0] = GLES2_USE0_DST_NUM(0)						|
					GLES2_USE_SRC1(TEMP, ui32SrcReg)			|
					GLES2_USE_SRC2(TEMP, ui32SrcReg);
	}
	else
	{
		puCode[0] = GLES2_USE0_DST_NUM(0)						|
					GLES2_USE_SRC1(PRIMATTR, ui32SrcReg)		|
					GLES2_USE_SRC2(PRIMATTR, ui32SrcReg);
	}

	puCode[1] = GLES2_USE1_OP(SOPWM)						|
				EURASIA_USE1_SKIPINV						|
				GLES2_USE1_SOPWM_COP(MAX)					|
				GLES2_USE1_SOPWM_AOP(MAX)					|
				GLES2_USE1_SOPWM_WRITEMASK(ui32ColorMask)	|
				GLES2_USE1_DST_TYPE(OUTPUT);

	return 8;
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
IMG_INTERNAL GLES2_MEMERROR WriteEOTUSSECode(GLES2Context *gc, EGLPixelBEState *psPBEState, 
											 IMG_DEV_VIRTADDR *puDevAddr, IMG_BOOL bPatch)
{
    IMG_UINT32 *pui32Buffer, *pui32BufferBase;


	if(bPatch)
	{
		pui32BufferBase = psPBEState->pui32PixelEventUSSE;

		GLES_ASSERT(pui32BufferBase);
	}
	else
	{
		pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers, USE_PIXELEVENT_EOT_BYTES >> 2, CBUF_TYPE_USSE_FRAG_BUFFER, IMG_FALSE);
		
		if(!pui32BufferBase)
		{
			return GLES2_3D_BUFFER_ERROR;
		}

		/* Record pixel event position so we can patch at first kick if necessary */
		psPBEState->pui32PixelEventUSSE = pui32BufferBase;
	}


#if defined(FIX_HW_BRN_26922)
	if(gc->psRenderSurface->eColourSpace==PVRSRV_COLOURSPACE_FORMAT_UNKNOWN)
	{
		pui32Buffer = WriteEndOfTileUSSECode(pui32BufferBase, 
											 psPBEState->aui32EmitWords, 
											 psPBEState->ui32SidebandWord);
	}
	else
#endif /* defined(FIX_HW_BRN_26922) */
	{
		pui32Buffer = WriteEndOfTileUSSECode(pui32BufferBase, 
											 psPBEState->aui32EmitWords, 
											 psPBEState->ui32SidebandWord
#if defined(SGX_FEATURE_RENDER_TARGET_ARRAYS)
											 , IMG_NULL
#endif
#if defined(SGX_FEATURE_WRITEBACK_DCU)
											 ,IMG_FALSE
#endif
											);
	}
	
	GLES2_INC_COUNT(GLES2_TIMER_USSE_FRAG_DATA_COUNT, (pui32Buffer - pui32BufferBase));

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

	return GLES2_NO_ERROR;
}


/***********************************************************************************
 Function Name      : InitPixelEventUSECodeBlocks
 Inputs             : gc
 Outputs            : 
 Returns            : Success
 Description        : Initialises any USSE code blocks required for pixel events
************************************************************************************/
static IMG_BOOL InitPixelEventUSECodeBlocks(GLES2Context *gc)
{
    IMG_UINT32 *pui32USEFragmentBuffer;

	gc->sPrim.psPixelEventPTOFFCodeBlock = 
		UCH_CodeHeapAllocate(gc->psSharedState->psUSEFragmentCodeHeap, USE_PIXELEVENT_PTOFF_BYTES, gc->psSysContext->hPerProcRef);
		
	GLES2_INC_COUNT(GLES2_TIMER_USECODEHEAP_FRAG_COUNT, USE_PIXELEVENT_PTOFF_BYTES >> 2);

	if(!gc->sPrim.psPixelEventPTOFFCodeBlock)
	{
		PVR_DPF((PVR_DBG_FATAL,"Couldn't allocate USE pixel event PTOFF code"));
		return IMG_FALSE;
	}
	
	pui32USEFragmentBuffer = gc->sPrim.psPixelEventPTOFFCodeBlock->pui32LinAddress;

	WritePTOffUSSECode(pui32USEFragmentBuffer);

	gc->sPrim.psPixelEventEORCodeBlock = 
		UCH_CodeHeapAllocate(gc->psSharedState->psUSEFragmentCodeHeap, USE_PIXELEVENT_EOR_BYTES, gc->psSysContext->hPerProcRef);
		
	GLES2_INC_COUNT(GLES2_TIMER_USECODEHEAP_FRAG_COUNT, USE_PIXELEVENT_EOR_BYTES >> 2);

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
static IMG_VOID FreePixelEventUSECodeBlocks(GLES2Context *gc)
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
IMG_INTERNAL IMG_DEV_VIRTADDR GetStateCopyUSEAddress(GLES2Context *gc, IMG_UINT32 ui32NumStateDWords)
{
	IMG_DEV_VIRTADDR uDevAddr;

	uDevAddr = gc->sPrim.psStateCopyCodeBlock->sCodeAddress;
	
	/* Total of GLES2_MAX_MTE_STATE_DWORDS versions of the state copy code. First 16 take a different number of instructions
     * than the rest.
	 */

	if(ui32NumStateDWords <= EURASIA_USE_MAXIMUM_REPEAT)
	{
		uDevAddr.uiAddr += ((ui32NumStateDWords - 1) * GLES2_STATECOPY_FIRST16_SIZE_IN_BYTES);
	}
	else
	{
		uDevAddr.uiAddr += (EURASIA_USE_MAXIMUM_REPEAT * GLES2_STATECOPY_FIRST16_SIZE_IN_BYTES);
		uDevAddr.uiAddr += ((ui32NumStateDWords - EURASIA_USE_MAXIMUM_REPEAT - 1) * GLES2_STATECOPY_REMAIN_SIZE_IN_BYTES);
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
static IMG_BOOL InitStateCopyUSECodeBlocks(GLES2Context *gc)
{
	IMG_UINT32 *pui32BufferBase, *pui32Buffer;
	IMG_UINT32 i, ui32CodeSize;
	IMG_UINT32 ui32First16Offset, ui32RemainOffset;


	ui32CodeSize  = GLES2_STATECOPY_FIRST16_SIZE_IN_BYTES * EURASIA_USE_MAXIMUM_REPEAT;

	ui32CodeSize += GLES2_STATECOPY_REMAIN_SIZE_IN_BYTES  * (GLES2_MAX_MTE_STATE_DWORDS - EURASIA_USE_MAXIMUM_REPEAT) ;

	ui32First16Offset = GLES2_STATECOPY_FIRST16_SIZE_IN_BYTES >> 2;
	ui32RemainOffset  = GLES2_STATECOPY_REMAIN_SIZE_IN_BYTES  >> 2;

	gc->sPrim.psStateCopyCodeBlock = 
		UCH_CodeHeapAllocate(gc->psSharedState->psUSEVertexCodeHeap, ui32CodeSize, gc->psSysContext->hPerProcRef);
	
	GLES2_INC_COUNT(GLES2_TIMER_USECODEHEAP_VERT_COUNT, ui32CodeSize >> 2);

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

		pui32Buffer = USEGenWriteStateEmitProgram (pui32BufferBase, i + 1, 0, IMG_FALSE);	/* PRQA S 3198 */ /* pui32Buffer is used in the following GLES_ASSERT. */

		GLES_ASSERT((IMG_UINT32)(pui32Buffer - pui32BufferBase) <= ui32First16Offset);

		pui32Buffer = pui32BufferBase + ui32First16Offset;
	}

	/*
		Generate the USE state copy program. (for the remaining copy programs)
	*/
	for (i = EURASIA_USE_MAXIMUM_REPEAT; i < GLES2_MAX_MTE_STATE_DWORDS; i++)
	{
	    pui32BufferBase = pui32Buffer;

		pui32Buffer = USEGenWriteStateEmitProgram (pui32BufferBase, i + 1, 0, IMG_FALSE);	/* PRQA S 3198 */ /* pui32Buffer is used in the following GLES_ASSERT. */

		GLES_ASSERT((IMG_UINT32)(pui32Buffer - pui32BufferBase) <= ui32RemainOffset);

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
static IMG_VOID FreeStateCopyUSECodeBlocks(GLES2Context *gc)
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

IMG_INTERNAL IMG_BOOL InitSpecialUSECodeBlocks(GLES2Context *gc)
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

IMG_INTERNAL IMG_VOID FreeSpecialUSECodeBlocks(GLES2Context *gc)
{
	FreeAccumUSECodeBlocks(gc);
	
	FreeClearUSECodeBlocks(gc);
	
	FreeScissorUSECodeBlocks(gc);

	FreePixelEventUSECodeBlocks(gc);

	FreeStateCopyUSECodeBlocks(gc);
}


/*****************************************************************************
 Function Name	: CreateVertexUnpackUSECode
 Inputs			: gc, psVertexVariant, ui32MainCodeSizeInBytes
 Outputs		: psVertexVariant->asUSEPhaseInfo[0]
 Returns		: Mem Error
 Description	: Generates USE code to perform vertex attribute unpacking.
                For example, unpacks vertex position in fixed format to floating point.
*****************************************************************************/
static GLES2_MEMERROR CreateVertexUnpackUSECode(GLES2Context *gc, GLES2USEShaderVariant *psVertexVariant, 
												IMG_UINT32 ui32MainCodeSizeInBytes, IMG_UINT32 **ppui32Instruction)
{
	IMG_UINT32 i, ui32NumInstructions = 0, ui32NumTemps = 0, ui32CodeSizeInBytes, ui32AlignSize;
	IMG_UINT32 *pui32Code;
	IMG_INT32 j;
	IMG_UINT32 ui32PrimaryAttrReg;
	IMG_UINT32 ui32InputPresentMask;
	IMG_BOOL bNeedNOPForEndFlag = IMG_FALSE;
	GLES2VertexArrayObjectMachine *psVAOMachine = &(gc->sVAOMachine);
	IMG_UINT32 ui32RepeatCount;
	IMG_UINT32 ui32NumOutputRegs = (gc->sProgram.psCurrentProgram->ui32OutputSelects & ~EURASIA_MTE_VTXSIZE_CLRMSK) >>
									EURASIA_MTE_VTXSIZE_SHIFT;
	
	/* Assert VAO is not NULL */
	GLES_ASSERT(VAO(gc));

	psVertexVariant->ui32PhaseCount = 0;

	psVertexVariant->u.sVertex.ui32NumItemsPerVertex = psVAOMachine->ui32NumItemsPerVertex;

	if(gc->sAppHints.bInitialiseVSOutputs)
	{
		ui32NumInstructions += ui32NumOutputRegs;	
	}


	/* 
	   First check for number of instructions 
	*/

	for (i = 0; i < psVertexVariant->u.sVertex.ui32NumItemsPerVertex; i++)
	{
	    GLES2AttribArrayPointerMachine *psAttribPointer;
		IMG_UINT32 ui32CurrentAttrib, ui32Size, ui32Type;
		IMG_BOOL bNormalized;

		psAttribPointer = psVAOMachine->apsPackedAttrib[i];
		ui32CurrentAttrib = (IMG_UINT32)(psAttribPointer - &(psVAOMachine->asAttribPointer[0]));
		ui32Size = (psAttribPointer->ui32CopyStreamTypeSize >> GLES2_STREAMSIZE_SHIFT);
		ui32Type = (psAttribPointer->ui32CopyStreamTypeSize & GLES2_STREAMTYPE_MASK);
		bNormalized = (psAttribPointer->ui32CopyStreamTypeSize & GLES2_STREAMNORM_BIT) ? IMG_TRUE : IMG_FALSE;

		/* Treat register as Vec4 */
		ui32PrimaryAttrReg = gc->sProgram.psCurrentProgram->aui32InputRegMappings[ui32CurrentAttrib] << 2;

		/* Pad the remaining elements with 0's and 1's */
		for (j = 3; j >= (IMG_INT32)ui32Size; j--)
		{
			IMG_UINT32 ui32VSInputsUsedBlock = ui32PrimaryAttrReg/32;
			ui32InputPresentMask = 1U << (ui32PrimaryAttrReg%32 + (IMG_UINT32)j);

			if(psVertexVariant->psPatchedShader->auVSInputsUsed[ui32VSInputsUsedBlock] & ui32InputPresentMask)
			{
				ui32NumInstructions++;
			}
		}

		switch(ui32Type)
		{
			case GLES2_STREAMTYPE_UBYTE:
			case GLES2_STREAMTYPE_USHORT:
			case GLES2_STREAMTYPE_HALFFLOAT:
			{
				for (j = (IMG_INT32)(ui32Size - 1); j >= 0; j--)
				{
					IMG_UINT32 ui32VSInputsUsedBlock = ui32PrimaryAttrReg/32;
					ui32InputPresentMask = 1U << (ui32PrimaryAttrReg%32 + (IMG_UINT32)j);

					/* Simple case: use a single unpck instruction (with or without scaling) */
					if(psVertexVariant->psPatchedShader->auVSInputsUsed[ui32VSInputsUsedBlock] & ui32InputPresentMask)
					{
						ui32NumInstructions++;
					}
				}
				break;
			}
			case GLES2_STREAMTYPE_BYTE:
			case GLES2_STREAMTYPE_SHORT:
			{
				for (j = (IMG_INT32)(ui32Size - 1); j >= 0; j--)
				{
					IMG_UINT32 ui32VSInputsUsedBlock = ui32PrimaryAttrReg/32;
					ui32InputPresentMask = 1U << (ui32PrimaryAttrReg%32 + (IMG_UINT32)j);

					if(psVertexVariant->psPatchedShader->auVSInputsUsed[ui32VSInputsUsedBlock] & ui32InputPresentMask)
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
					}
				}

				break;
			}
			case GLES2_STREAMTYPE_FIXED:
			{
				for (j = (IMG_INT32)(ui32Size - 1); j >= 0; j--)
				{
					IMG_UINT32 ui32VSInputsUsedBlock = ui32PrimaryAttrReg/32;
					ui32InputPresentMask = 1U << (ui32PrimaryAttrReg%32 + (IMG_UINT32)j);

					/* Fixed 16.16 -> Float unpacking requires 3 instructions and 1 temp */
					if(psVertexVariant->psPatchedShader->auVSInputsUsed[ui32VSInputsUsedBlock] & ui32InputPresentMask)
					{
						ui32NumInstructions += 3;
						ui32NumTemps = 1;
#if defined(SGX_FEATURE_USE_VEC34)
						bNeedNOPForEndFlag = IMG_TRUE;
#endif
					}
				}
				break;
			}
			case GLES2_STREAMTYPE_FLOAT:
				/* do nothing */
				break;

			default:
				PVR_DPF((PVR_DBG_FATAL,"CreateVertexUnpackUSECode: Invalid case in switch statement"));	
		}

		/* Setup variant check structures for next time */
		psVertexVariant->u.sVertex.aui32StreamTypeSize[i] = psAttribPointer->ui32CopyStreamTypeSize;
	}

#if defined(FIX_HW_BRN_31988)
	if(!ui32NumInstructions)
	{
		bNeedNOPForEndFlag = IMG_TRUE;
	}
	ui32NumInstructions += 4;
#endif

	/* bNeedNOPForEndFlag may be ture if defined(SGX_FEATURE_USE_VEC34). */
	/* PRQA S 3359,3201 ++ */
	if(bNeedNOPForEndFlag)
	{
		ui32NumInstructions++;
	}


	/* PRQA S 3359,3201 -- */

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

	GLES2_TIME_START(GLES2_TIMER_USECODEHEAP_VERT_TIME);
	
	/* Allocate a single block for unpack and main shader code */
	psVertexVariant->psCodeBlock = UCH_CodeHeapAllocate(gc->psSharedState->psUSEVertexCodeHeap, 
													ui32CodeSizeInBytes + ui32MainCodeSizeInBytes,
													gc->psSysContext->hPerProcRef);

	GLES2_TIME_STOP(GLES2_TIMER_USECODEHEAP_VERT_TIME);
	
	if(!psVertexVariant->psCodeBlock)
	{
		/* Destroy all vertex variants (will kick and wait for TA) */
		NamesArrayMapFunction(gc, gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_PROGRAM], DestroyVertexVariants, IMG_NULL);

		GLES2_TIME_START(GLES2_TIMER_USECODEHEAP_VERT_TIME);
		
		/* Allocate a single block for unpack and main shader code */
		psVertexVariant->psCodeBlock = UCH_CodeHeapAllocate(gc->psSharedState->psUSEVertexCodeHeap, 
														ui32CodeSizeInBytes + ui32MainCodeSizeInBytes,
														gc->psSysContext->hPerProcRef);

		GLES2_TIME_STOP(GLES2_TIMER_USECODEHEAP_VERT_TIME);

		if(!psVertexVariant->psCodeBlock)
		{
			return GLES2_TA_USECODE_ERROR;
		}
	}
	
	GLES2_INC_COUNT(GLES2_TIMER_USECODEHEAP_VERT_COUNT, (ui32CodeSizeInBytes + ui32MainCodeSizeInBytes) >> 2);

	if(ui32CodeSizeInBytes)
	{
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
		IMG_UINT32 ui32NextPhaseMode, ui32NextPhaseAddress;
#endif

		/* Setup 1st phase execution address */ 
		psVertexVariant->sStartAddress[0] = psVertexVariant->psCodeBlock->sCodeAddress;
		pui32Code = psVertexVariant->psCodeBlock->pui32LinAddress;

		/* Setup 2nd phase execution address */ 
		psVertexVariant->sStartAddress[1].uiAddr = ( psVertexVariant->psCodeBlock->sCodeAddress.uiAddr +
													 ui32CodeSizeInBytes +
													 (psVertexVariant->psPatchedShader->uProgStartInstIdx * EURASIA_USE_INSTRUCTION_SIZE) );

		*ppui32Instruction = (IMG_UINT32 *)( (IMG_UINTPTR_T)psVertexVariant->psCodeBlock->pui32LinAddress + 
											 ui32CodeSizeInBytes );

		psVertexVariant->ui32MaxTempRegs = MAX(psVertexVariant->ui32MaxTempRegs, ui32NumTemps);

		/* Increase ui32PhaseCount */
		psVertexVariant->ui32PhaseCount++;

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)

		if(psVertexVariant->psPatchedShader->uFlags & UNIFLEX_HW_FLAGS_PER_INSTANCE_MODE)
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

#if defined(SGX_FEATURE_VCB)
		/* Setup 3rd phase execution address */
		psVertexVariant->sStartAddress[2].uiAddr = ( psVertexVariant->psCodeBlock->sCodeAddress.uiAddr +
													 ui32CodeSizeInBytes + 
													 ui32MainCodeSizeInBytes - 
													 2 * EURASIA_USE_INSTRUCTION_SIZE);	/* - PHAS - EMITVTX */	
#endif /* defined(SGX_FEATURE_VCB) */

		ui32NumInstructions = 1;
#else /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */		
		ui32NumInstructions = 0;
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */		
	}
	else
	{
		/* Setup 1st phase execution address */
	    psVertexVariant->sStartAddress[0].uiAddr = ( psVertexVariant->psCodeBlock->sCodeAddress.uiAddr + 
													 (psVertexVariant->psPatchedShader->uProgStartInstIdx * EURASIA_USE_INSTRUCTION_SIZE) );

		*ppui32Instruction = (IMG_UINT32 *)( (IMG_UINTPTR_T)psVertexVariant->psCodeBlock->pui32LinAddress );
		
#if defined(SGX_FEATURE_VCB)
		/* Setup 2nd phase execution address */
		psVertexVariant->sStartAddress[1].uiAddr = ( psVertexVariant->psCodeBlock->sCodeAddress.uiAddr +
													 ui32MainCodeSizeInBytes -
													 2 * EURASIA_USE_INSTRUCTION_SIZE);	/* - PHAS - EMITVTX */
#endif /* defined(SGX_FEATURE_VCB) */

		return GLES2_NO_ERROR;
	}

#if defined(FIX_HW_BRN_31988)
	USEGenWriteBRN31988Fragment(&pui32Code[ui32NumInstructions * 2]);

	ui32NumInstructions +=4;

#endif

	if(gc->sAppHints.bInitialiseVSOutputs)
	{
		do
		{
			if(ui32NumOutputRegs > (EURASIA_USE1_RCOUNT_MAX + 1))
			{
				ui32RepeatCount = EURASIA_USE1_RCOUNT_MAX;
			}
			else
			{
				ui32RepeatCount = ui32NumOutputRegs-1;
			}

			/* Do a MOV outputN, Immediate0 with a repeat count */
			BuildMOV ((PPVR_USE_INST) &pui32Code[ui32NumInstructions * 2],
						EURASIA_USE1_EPRED_ALWAYS,
						ui32RepeatCount,
						EURASIA_USE1_RCNTSEL,
						USE_REGTYPE_OUTPUT, 0,
						USE_REGTYPE_IMMEDIATE, 0,
						0 /* Src mod */);

			ui32NumInstructions ++;
			ui32NumOutputRegs -= ui32RepeatCount+1;

		} while(ui32NumOutputRegs != 0);
	}

	/* Setup copies for all of the streams */
	for (i = 0; i < psVertexVariant->u.sVertex.ui32NumItemsPerVertex; i++)
	{
	    GLES2AttribArrayPointerMachine *psAttribPointer;
		IMG_UINT32 ui32CurrentAttrib, ui32Size, ui32Type, ui32ComponentSize;
		IMG_BOOL bNormalized, bWorkAroundBRN22509;
	    IMG_UINT32 ui32MADSrc1, ui32MADSrc2;

		psAttribPointer     = psVAOMachine->apsPackedAttrib[i];
		ui32CurrentAttrib   = (IMG_UINT32)(psAttribPointer - &(psVAOMachine->asAttribPointer[0]));
		ui32Size            = (psAttribPointer->ui32CopyStreamTypeSize >> GLES2_STREAMSIZE_SHIFT);
		ui32Type            = (psAttribPointer->ui32CopyStreamTypeSize & GLES2_STREAMTYPE_MASK);
		ui32ComponentSize   = aui32AttribSize[ui32Type];
		bNormalized         = (psAttribPointer->ui32CopyStreamTypeSize & GLES2_STREAMNORM_BIT) ? IMG_TRUE : IMG_FALSE;
		bWorkAroundBRN22509 = ( bNormalized && 
								((ui32Type == GLES2_STREAMTYPE_BYTE) || 
								(ui32Type == GLES2_STREAMTYPE_SHORT)) ) ? IMG_TRUE : IMG_FALSE;

		/* Treat register as Vec4 */
		ui32PrimaryAttrReg = gc->sProgram.psCurrentProgram->aui32InputRegMappings[ui32CurrentAttrib] << 2;

		/* Pad the remaining elements with 0's and 1's */
		for (j = 3; j >= (IMG_INT32)ui32Size; j--)
		{
			static const IMG_FLOAT afVals[] = {0.0f, 0.0f, 0.0f, 1.0f};
			GLES2_FUINT32 uTemp;
			IMG_UINT32 ui32VSInputsUsedBlock = ui32PrimaryAttrReg/32;	
			ui32InputPresentMask = 1U << (ui32PrimaryAttrReg%32 + (IMG_UINT32)j);

			if(psVertexVariant->psPatchedShader->auVSInputsUsed[ui32VSInputsUsedBlock] & ui32InputPresentMask)
			{
				uTemp.fVal = afVals[j];

				/* limm pa[paReg + i], #1.0f or #0.0f */
				BuildLIMM((PPVR_USE_INST)&pui32Code[ui32NumInstructions * 2],
										EURASIA_USE1_EPRED_ALWAYS,
										0,
										USE_REGTYPE_PRIMATTR,
										ui32PrimaryAttrReg + (IMG_UINT32)j,
										uTemp.ui32Val);

				ui32NumInstructions++;
			}
		}

		switch(ui32Type)
		{
			case GLES2_STREAMTYPE_FLOAT:
				break;
			/* If the stream is not float type - it needs to be unpacked to float */
			case GLES2_STREAMTYPE_BYTE:
			case GLES2_STREAMTYPE_SHORT:
			case GLES2_STREAMTYPE_UBYTE:
			case GLES2_STREAMTYPE_USHORT:
			case GLES2_STREAMTYPE_HALFFLOAT:
			{
				/* Unpack the src registers */
				for (j = (IMG_INT32)(ui32Size - 1); j >= 0; j--)
				{
					IMG_UINT32 ui32SrcRegister = ui32PrimaryAttrReg + (((IMG_UINT32)j * ui32ComponentSize) >> 2);
					IMG_UINT32 ui32SrcComponent = ((IMG_UINT32)j * ui32ComponentSize) % 4;
					IMG_UINT32 ui32VSInputsUsedBlock = ui32PrimaryAttrReg/32;
					
					ui32InputPresentMask = 1U << (ui32PrimaryAttrReg%32 + (IMG_UINT32)j);

					if(psVertexVariant->psPatchedShader->auVSInputsUsed[ui32VSInputsUsedBlock] & ui32InputPresentMask)
					{

						/* unpckf32XX pa[paReg + i], pa[paReg].i */
						BuildUNPCKF32((PPVR_USE_INST) &pui32Code[ui32NumInstructions * 2],
										EURASIA_USE1_EPRED_ALWAYS, 0,
										EURASIA_USE1_RCNTSEL,
										aui32StreamTypeToUseType[ui32Type],
										((bNormalized && !bWorkAroundBRN22509)? EURASIA_USE0_PCK_SCALE : 0),
										USE_REGTYPE_PRIMATTR,
										ui32PrimaryAttrReg + (IMG_UINT32)j,
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

							if(ui32Type == GLES2_STREAMTYPE_BYTE)
							{
								ui32MADSrc1 = GLES2_VERTEX_SECATTR_UNPCKS8_CONST1;
								ui32MADSrc2 = GLES2_VERTEX_SECATTR_UNPCKS8_CONST2;
							}
							else
							{
								GLES_ASSERT(ui32Type == GLES2_STREAMTYPE_SHORT);
								ui32MADSrc1 = GLES2_VERTEX_SECATTR_UNPCKS16_CONST1;
								ui32MADSrc2 = GLES2_VERTEX_SECATTR_UNPCKS16_CONST2;
							}

							/* fmad.skipinv pa[j], pa[j], sa[const1], sa[const2] */
							/* that is, pa[j] = pa[j]*sa[const1] + sa[const2] */
							BuildMAD((PPVR_USE_INST) &pui32Code[ui32NumInstructions * 2],
													EURASIA_USE1_EPRED_ALWAYS,
													0,
													EURASIA_USE1_RCNTSEL,
													USE_REGTYPE_PRIMATTR,
													ui32PrimaryAttrReg + (IMG_UINT32)j,
													USE_REGTYPE_PRIMATTR,
													ui32PrimaryAttrReg + (IMG_UINT32)j,
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
				}

				break;
			}
			case GLES2_STREAMTYPE_FIXED:
			{
				/* Unpack the src registers */
				for (j = (IMG_INT32)(ui32Size - 1); j >= 0; j--)
				{
					/* Unpack high 16 bits first */
					IMG_UINT32 ui32SrcRegister = ui32PrimaryAttrReg + (IMG_UINT32)j;
					IMG_UINT32 ui32SrcComponent = 2;
					IMG_UINT32 ui32VSInputsUsedBlock = ui32PrimaryAttrReg/32;
					
					ui32InputPresentMask = 1U << (ui32PrimaryAttrReg%32 + (IMG_UINT32)j);

					if(psVertexVariant->psPatchedShader->auVSInputsUsed[ui32VSInputsUsedBlock] & ui32InputPresentMask)
					{
						/* unpckf32s16 tmp0, pa[paReg].i */
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

						/* unpckf32u16 pa[paReg + i], pa[paReg].i */
						BuildUNPCKF32((PPVR_USE_INST) &pui32Code[ui32NumInstructions * 2],
										EURASIA_USE1_EPRED_ALWAYS, 0,
										EURASIA_USE1_RCNTSEL,
										EURASIA_USE1_PCK_FMT_U16,
										0,
										USE_REGTYPE_PRIMATTR,
										ui32PrimaryAttrReg + (IMG_UINT32)j,
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
												ui32PrimaryAttrReg + (IMG_UINT32)j,
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
				}
				break;
			}
			default:
				PVR_DPF((PVR_DBG_ERROR,"Unknown stream-type"));
				break;
		}
	}

	/* bNeedNOPForEndFlag may be ture if defined(SGX_FEATURE_USE_VEC34). */
	/* PRQA S 3359,3201 ++ */
	if(bNeedNOPForEndFlag)
	{
		BuildNOP((PPVR_USE_INST) &pui32Code[ui32NumInstructions * 2], 0, IMG_FALSE);
		ui32NumInstructions++;
	}
	/* PRQA S 3359,3201 -- */

	/* Terminate this block of code */
	if (ui32NumInstructions)
	{
		pui32Code[(ui32NumInstructions * 2) - 1] |= EURASIA_USE1_END;
	}

	return GLES2_NO_ERROR;
}


/*****************************************************************************
 Function Name	: SetupUSESecondaryUploadTask
 Inputs			: gc              - pointer to the context
                psPatchedShader - shader where the secondary attr. upload task is taken from
                bIsVertexShader - IMG_TRUE if the patched shader is a vertex one. IMG_FALSE otherwise
 Outputs		: psSharedState->psSecondaryUploadTask - If it doesn't exist, it is created. Else, its refcount is incremented.
 Returns		: Pointer to the newly created secondary upload task or IMG_NULL if something went wrong
 Description	: If the patched shader has a USE secondary upload task but it has not been copied to the shader
                shared state, this function copies it. If it has already been copied before, the refcount is incremented.
                The need for refcounting is that these tasks are shared between the ShaderSharedState, USE variants and USE ghosts.
*****************************************************************************/
static GLES2_MEMERROR SetupUSESecondaryUploadTask(GLES2Context *gc, const USP_HW_SHADER *psPatchedShader,
																 GLES2SharedShaderState *psSharedState, IMG_BOOL bIsVertexShader)
{
	UCH_UseCodeBlock            *psSecondaryUseCode;
	IMG_UINT32                   ui32SAProgramSize, *pui32UseCode, *pui32UseCodeBase;
	GLES2USESecondaryUploadTask  *psSecondaryUploadTask;
	UCH_UseCodeHeap             *psUSEHeap = bIsVertexShader? gc->psSharedState->psUSEVertexCodeHeap : gc->psSharedState->psUSEFragmentCodeHeap;

	/* This function shouldn't be called if there is no secondary USE program */
	GLES_ASSERT(psPatchedShader && psPatchedShader->uSAUpdateInstCount);

	PVRSRVLockMutex(gc->psSharedState->hPrimaryLock);

	if(psSharedState->psSecondaryUploadTask)
	{
		/* There's already a secondary upload task. Just increment the refcount.
		   XX: Is the double locking required?
		*/
		USESecondaryUploadTaskAddRef(gc, psSharedState->psSecondaryUploadTask);
		PVRSRVUnlockMutex(gc->psSharedState->hPrimaryLock);

		return GLES2_NO_ERROR;
	}

	psSecondaryUploadTask = GLES2Calloc(gc, sizeof(GLES2USESecondaryUploadTask));

	if(!psSecondaryUploadTask)
	{
		PVR_DPF((PVR_DBG_ERROR, "SetupUSESecondaryUploadTask: Out of host memory.\n"));
		return GLES2_HOST_MEM_ERROR;
	}


	ui32SAProgramSize = psPatchedShader->uSAUpdateInstCount * EURASIA_USE_INSTRUCTION_SIZE;

	if(psPatchedShader->uFlags & USP_HWSHADER_FLAGS_SAPROG_LABEL_AT_END)
	{
		ui32SAProgramSize += EURASIA_USE_INSTRUCTION_SIZE;
	}

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES) 
	/* Add the space of single PHAS */
	ui32SAProgramSize += EURASIA_USE_INSTRUCTION_SIZE;
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

	GLES2_TIME_START(bIsVertexShader? GLES2_TIMER_USECODEHEAP_VERT_TIME : GLES2_TIMER_USECODEHEAP_FRAG_TIME);

	psSecondaryUseCode = UCH_CodeHeapAllocate(psUSEHeap, ui32SAProgramSize, gc->psSysContext->hPerProcRef);

	GLES2_TIME_STOP(bIsVertexShader? GLES2_TIMER_USECODEHEAP_VERT_TIME  : GLES2_TIMER_USECODEHEAP_FRAG_TIME);

	if(!psSecondaryUseCode)
	{
		if(bIsVertexShader)
		{
			/* Destroy all vertex variants (will kick and wait for TA) */
			NamesArrayMapFunction(gc, gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_PROGRAM], DestroyVertexVariants, IMG_NULL);
		}
		else
		{
			/* Destroy unused fragment shaders */
			KRM_ReclaimUnneededResources(gc, &gc->psSharedState->sUSEShaderVariantKRM);
		}

		GLES2_TIME_START(bIsVertexShader? GLES2_TIMER_USECODEHEAP_VERT_TIME : GLES2_TIMER_USECODEHEAP_FRAG_TIME);

		psSecondaryUseCode = UCH_CodeHeapAllocate(psUSEHeap, ui32SAProgramSize, gc->psSysContext->hPerProcRef);

		GLES2_TIME_STOP(bIsVertexShader? GLES2_TIMER_USECODEHEAP_VERT_TIME  : GLES2_TIMER_USECODEHEAP_FRAG_TIME);

		if(!psSecondaryUseCode)
		{
			PVR_DPF((PVR_DBG_ERROR,"SetupUSESecondaryUploadTask: Out of USE memory!"));
			GLES2Free(IMG_NULL, psSecondaryUploadTask);
			return bIsVertexShader? GLES2_TA_USECODE_ERROR : GLES2_3D_USECODE_ERROR;
		}
	}

	GLES2_INC_COUNT(bIsVertexShader? GLES2_TIMER_USECODEHEAP_VERT_COUNT : GLES2_TIMER_USECODEHEAP_FRAG_COUNT, ui32SAProgramSize >> 2);

	pui32UseCode = pui32UseCodeBase = (IMG_UINT32 *)psSecondaryUseCode->pui32LinAddress; 

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES) 
	BuildPHASLastPhase ((PPVR_USE_INST) pui32UseCodeBase, 0);

	pui32UseCode += USE_INST_LENGTH;
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */
	
	GLES2MemCopy(pui32UseCode, 
				psPatchedShader->puSAUpdateInsts,
				psPatchedShader->uSAUpdateInstCount * EURASIA_USE_INSTRUCTION_SIZE);

	pui32UseCode = (IMG_UINT32 *)((IMG_UINTPTR_T)pui32UseCode + (psPatchedShader->uSAUpdateInstCount * EURASIA_USE_INSTRUCTION_SIZE));

	if(psPatchedShader->uFlags & USP_HWSHADER_FLAGS_SAPROG_LABEL_AT_END)
	{
		BuildNOP((PPVR_USE_INST)pui32UseCode, 0, IMG_FALSE);
		pui32UseCode += USE_INST_LENGTH;
	}

	/* Terminate this block of code (sneaky negative array index) */
	pui32UseCode[-1] |= EURASIA_USE1_END;

	/*
	 * The initial refcount is two because when the task is created it is immediately referenced by
	 * the SharedShaderState and by the USE variant that triggered the creation.
	 */
	psSecondaryUploadTask->ui32RefCount = 2;
	psSecondaryUploadTask->psSecondaryCodeBlock = psSecondaryUseCode;

	psSharedState->psSecondaryUploadTask = psSecondaryUploadTask;
	
	PVRSRVUnlockMutex(gc->psSharedState->hPrimaryLock);

	return GLES2_NO_ERROR;
}

/****************************************************************
 * Function Name  	: SetupUSEVertexShader
 * Returns        	: Error code
 * Globals Used    	: pbProgramChanged
 * Description    	: 
 ****************************************************************/
IMG_INTERNAL GLES2_MEMERROR SetupUSEVertexShader(GLES2Context *gc, IMG_BOOL *pbProgramChanged)
{
	GLES2CompiledTextureState *psVertexTextureState = &gc->sPrim.sVertexTextureState;
	GLES2ProgramShader *psVertexShader = &gc->sProgram.psCurrentProgram->sVertex;
	GLES2USEShaderVariant *psVertexVariant = psVertexShader->psVariant;
	USP_HW_SHADER *psPatchedShader;
	IMG_UINT32 ui32CodeSizeInBytes, *pui32Instruction, *pui32InstructionBase;
	IMG_UINT32 ui32AlignSize;
	IMG_BOOL bMatch;
	IMG_UINT32 ui32ImageUnitEnables = psVertexTextureState->ui32ImageUnitEnables;
	IMG_UINT16 i;
	GLES2_MEMERROR eError = GLES2_NO_ERROR;
#if defined(SGX_FEATURE_VCB)
	IMG_UINT32 ui32NextPhaseMode, ui32NextPhaseAddress;
#endif
	GLES2VertexArrayObjectMachine *psVAOMachine = &(gc->sVAOMachine);

#if defined(DEBUG)
	psPatchedShader = IMG_NULL;
#endif

	GLES_ASSERT(VAO(gc));


	while(psVertexVariant)
	{
	    if(psVertexVariant->u.sVertex.ui32NumItemsPerVertex == psVAOMachine->ui32NumItemsPerVertex)
		{
			bMatch = IMG_TRUE;
			  
			i = 0;
			  
			/* Check vertex streams match */
			while((i < psVertexVariant->u.sVertex.ui32NumItemsPerVertex) && bMatch)
			{
				GLES2AttribArrayPointerMachine *psAttribPointer = psVAOMachine->apsPackedAttrib[i];

				if(psVertexVariant->u.sVertex.aui32StreamTypeSize[i] != psAttribPointer->ui32CopyStreamTypeSize)
				{
					bMatch = IMG_FALSE;
				}
				else
				{
					i++;
				}
			}

			/* If vertex streams match, check texture formats/enables */
			if(bMatch && psVertexVariant->u.sVertex.ui32ImageUnitEnables == ui32ImageUnitEnables)
			{
				for(i=0; i < GLES2_MAX_TEXTURE_UNITS; i++)
				{
					if(ui32ImageUnitEnables & (1U << i))
					{
						if(psVertexVariant->u.sVertex.apsTexFormat[i] != psVertexTextureState->apsTexFormat[i])
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

		/* Check the next vertex variant */
		psVertexVariant = psVertexVariant->psNext;
	}

	if(!psVertexVariant)
	{
		psVertexVariant = GLES2Calloc(gc, sizeof(GLES2USEShaderVariant));

		if(!psVertexVariant)
		{
			return GLES2_HOST_MEM_ERROR;
		}

		psVertexVariant->psProgramShader = psVertexShader;

		/* Add 1 instruction (PHAS) space at the beginning of uProgStartInstIdx (UniPatchShader) */
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
		PVRUniPatchSetPreambleInstCount(gc->sProgram.pvUniPatchContext, 1);
#endif

		/* Setup state to for variant */
		psVertexVariant->u.sVertex.ui32ImageUnitEnables = ui32ImageUnitEnables;

		if(ui32ImageUnitEnables)
		{
			for(i=0; i < GLES2_MAX_TEXTURE_UNITS; i++)
			{
				if(ui32ImageUnitEnables & (1U << i))
				{
					psVertexVariant->u.sVertex.apsTexFormat[i] = psVertexTextureState->apsTexFormat[i];
					
					PVRUniPatchSetTextureFormat(gc->sProgram.pvUniPatchContext,
												i,
												(USP_TEX_FORMAT *)((IMG_UINTPTR_T)(&psVertexTextureState->apsTexFormat[i]->sTexFormat)),
												IMG_FALSE,
												IMG_FALSE);
				}
			}
		}

		psPatchedShader = PVRUniPatchFinaliseShader(gc->sProgram.pvUniPatchContext, 
													psVertexShader->psSharedState->pvUniPatchShader);
		
		if(!psPatchedShader)
		{
			PVR_DPF((PVR_DBG_FATAL,"SetupUSEVertexShader: Unipatch failed to finalise the shader"));
			SetError(gc, GL_OUT_OF_MEMORY);
			GLES2Free(IMG_NULL, psVertexVariant);
			return GLES2_GENERAL_MEM_ERROR;
		}

		/* Create the secondary code block as necessary */
		if(psPatchedShader->uSAUpdateInstCount)
		{
			eError = SetupUSESecondaryUploadTask(gc, psPatchedShader, psVertexShader->psSharedState, IMG_TRUE);

			if(eError != GLES2_NO_ERROR)
			{
				PVRUniPatchDestroyHWShader(gc->sProgram.pvUniPatchContext, psPatchedShader);
				GLES2Free(IMG_NULL, psVertexVariant);
				return eError;
			}
		}

		psVertexVariant->psPatchedShader = psPatchedShader;
		psVertexVariant->psSecondaryUploadTask = psVertexShader->psSharedState->psSecondaryUploadTask;

		/* Add 1 instruction for EMITVTX */
		ui32CodeSizeInBytes = (psPatchedShader->uInstCount + 1) * EURASIA_USE_INSTRUCTION_SIZE;


		/*Align to exe address boundary for the current phase */
		ui32AlignSize = EURASIA_PDS_DOUTU_PHASE_START_ALIGN - 1;

		ui32CodeSizeInBytes = (ui32CodeSizeInBytes + ui32AlignSize) & ~ui32AlignSize;

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
#if defined(SGX_FEATURE_VCB)
		/* Add 1 EMITVCB, 1 PHAS */
		ui32CodeSizeInBytes += EURASIA_USE_INSTRUCTION_SIZE * 2;
#endif 
#endif 
		psVertexVariant->ui32MaxTempRegs = psPatchedShader->uTempRegCount;

		eError = CreateVertexUnpackUSECode(gc, psVertexVariant, ui32CodeSizeInBytes, &pui32InstructionBase);

		if(eError != GLES2_NO_ERROR)
		{
			USESecondaryUploadTaskDelRef(gc, psVertexVariant->psSecondaryUploadTask);
			PVRUniPatchDestroyHWShader(gc->sProgram.pvUniPatchContext, psPatchedShader);
			GLES2Free(IMG_NULL, psVertexVariant);
			return eError;
		}
			
		pui32Instruction = pui32InstructionBase;
	
		/* Copy code from patched shader */
		GLES2MemCopy((IMG_UINT8 *)pui32Instruction, 
					 psPatchedShader->puInsts, 
					 psPatchedShader->uInstCount * EURASIA_USE_INSTRUCTION_SIZE);


		/* Increase ui32PhaseCount */
		psVertexVariant->ui32PhaseCount++;		


#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)

		/* Setup execution address of this phase */
		pui32Instruction = ( pui32InstructionBase +
							 ((psVertexVariant->psPatchedShader->uProgStartInstIdx * EURASIA_USE_INSTRUCTION_SIZE) >> 2) );

#if defined(SGX_FEATURE_VCB)

		if(psPatchedShader->uFlags & UNIFLEX_HW_FLAGS_PER_INSTANCE_MODE)
		{
			ui32NextPhaseMode = EURASIA_USE_OTHER2_PHAS_MODE_PERINSTANCE;
		}
		else
		{
			ui32NextPhaseMode = EURASIA_USE_OTHER2_PHAS_MODE_PARALLEL;
		}

		/* Setup HW address for next phase instruction */
		ui32NextPhaseAddress = GetUSEPhaseAddress(psVertexVariant->sStartAddress[psVertexVariant->ui32PhaseCount],
												  gc->psSysContext->uUSEVertexHeapBase, 
												  SGX_VTXSHADER_USE_CODE_BASE_INDEX);

		BuildPHASImmediate((PPVR_USE_INST)	pui32Instruction,
											0, /* ui32End */
											ui32NextPhaseMode,
											EURASIA_USE_OTHER2_PHAS_RATE_PIXEL,
											EURASIA_USE_OTHER2_PHAS_WAITCOND_VCULL,
											0, /* ui32NumTemps */
											ui32NextPhaseAddress);
	
		pui32Instruction = (IMG_UINT32 *)( (IMG_UINTPTR_T)pui32InstructionBase + 
										   (psPatchedShader->uInstCount * EURASIA_USE_INSTRUCTION_SIZE) );
		
		/***** Add EMITVCB instruction *****/
		USEGenWriteEndPreCullVtxShaderFragment(pui32Instruction);
		
		/* Increase ui32PhaseCount */
		psVertexVariant->ui32PhaseCount++;		

		/* Update current instruction address to the start of next phase */
		pui32Instruction = ( pui32InstructionBase + 
							 ((ui32CodeSizeInBytes - EURASIA_USE_INSTRUCTION_SIZE * 2) >> 2) );

		/***** Insert PHAS instruction *****/
		BuildPHASLastPhase ((PPVR_USE_INST) pui32Instruction, 0);
		pui32Instruction += USE_INST_LENGTH;
#else
		/* Replace NOP (the first instruction) with PHAS NONE */
		BuildPHASLastPhase((PPVR_USE_INST) pui32Instruction, 0);

		pui32Instruction = (IMG_UINT32 *)( (IMG_UINT8 *)pui32InstructionBase + 
										   (psPatchedShader->uInstCount * EURASIA_USE_INSTRUCTION_SIZE) );
#endif

#else /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

		pui32Instruction = (IMG_UINT32 *)( (IMG_UINTPTR_T)pui32InstructionBase + 
										   (psPatchedShader->uInstCount * EURASIA_USE_INSTRUCTION_SIZE) );
	
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

		/* Add EMITMTE instruction */
		USEGenWriteEndVtxShaderFragment(pui32Instruction);

		/* Setup use mode of operation */
		if(psPatchedShader->uFlags & UNIFLEX_HW_FLAGS_PER_INSTANCE_MODE)
		{
			psVertexShader->ui32USEMode = EURASIA_PDS_DOUTU1_MODE_PERINSTANCE;
		}
		else
		{
			psVertexShader->ui32USEMode = EURASIA_PDS_DOUTU1_MODE_PARALLEL;
		}

		/* Add to variant list */
		psVertexVariant->psNext = psVertexShader->psVariant;
		psVertexShader->psVariant = psVertexVariant;
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

#if defined(DEBUG)
	if(gc->pShaderAnalysisHandle && psPatchedShader)
	{
		DumpVertexShader(gc, psVertexVariant, (psVertexVariant->psCodeBlock->ui32Size >>3)-1);
	}
#endif /* defined(DEBUG) */

	return eError;
}


/*****************************************************************************
 Function Name	: SetupIteratorsAndTextureLookups
 Inputs			: gc, ui32NumNonDependentTextureLoads, 
				  psTextureLoads, bIteratePlaneCoefficients 
 Outputs		: puPlaneCoeffPAReg
 Returns		: -
 Description	: Sets up iterators and non-dependent texture loads for fragment shader
*****************************************************************************/
static IMG_VOID SetupIteratorsAndTextureLookups(GLES2Context *gc,
												IMG_UINT32 ui32NumNonDependentTextureLoads,
												USP_HW_PSINPUT_LOAD *psTextureLoads,
												USP_TEX_CTR_WORDS *psTextureControlWords,
#if defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728)
												IMG_BOOL bIteratePosition,
												IMG_UINT32 *pui32PositionPAReg,
#endif /* defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728) */
												IMG_BOOL bIteratePlaneCoefficients,
												IMG_UINT32 *pui32PlaneCoeffPAReg)
{
	GLES2USEShaderVariant *psFragmentVariant = gc->sProgram.psCurrentFragmentVariant;
	GLES2PDSInfo *psPDSInfo  = &psFragmentVariant->u.sFragment.sPDSInfo;
	IMG_UINT32 ui32TextureUnit;

#if defined(SGX_FEATURE_CEM_S_USES_PROJ)
	IMG_UINT32 ui32ChunkNumber;
#endif

	IMG_UINT32  i;
	IMG_UINT32 *pui32TSPParamFetchInterface = 
#if defined(SGX545)
	                                          &psPDSInfo->aui32TSPParamFetchInterface0[0];
#else /* defined(SGX545) */
	                                          &psPDSInfo->aui32TSPParamFetchInterface[0];
#endif /* defined(SGX545) */
	IMG_UINT32 *pui32LayerControl           = &psPDSInfo->aui32LayerControl[0];
	IMG_UINT32 ui32LastTexIssue, ui32LastUSEIssue, ui32DependencyControl;
	IMG_UINT32 ui32PACount = 0;

	ui32DependencyControl = 0;


	/* ui32LastTexIssue = ui32LastUSEIssue = (IMG_UINT32)-1; */
	ui32LastTexIssue = ui32LastUSEIssue = 0xFFFFFFFFU;

	for (i=0; i<ui32NumNonDependentTextureLoads; i++)
	{
		IMG_UINT32 ui32IteratorNumber  = psTextureLoads[i].eCoord;
		IMG_UINT32 ui32InputCoordUnit = 0;

		/* Is this iteration command for the USE or the TAG? */
		if (psTextureLoads[i].eType == USP_HW_PSINPUT_TYPE_ITERATION)
		{
			/* Find which vertex shader output corresponding this input */
			if(ui32IteratorNumber == USP_HW_PSINPUT_COORD_POS)
			{
				ui32InputCoordUnit =  EURASIA_PDS_DOUTI_USEISSUE_POSITION;
			}
			else if(ui32IteratorNumber == USP_HW_PSINPUT_COORD_V0)
			{
				ui32InputCoordUnit =  EURASIA_PDS_DOUTI_USEISSUE_V0;
			}
			else if (ui32IteratorNumber < NUM_TC_REGISTERS)
			{
				ui32InputCoordUnit = (gc->sProgram.psCurrentProgram->aui32IteratorPosition[ui32IteratorNumber] + 
					(EURASIA_PDS_DOUTI_USEISSUE_TC0 >> EURASIA_PDS_DOUTI_USEISSUE_SHIFT)) << EURASIA_PDS_DOUTI_USEISSUE_SHIFT;
			}


		    pui32TSPParamFetchInterface[i] = (ui32InputCoordUnit & ~EURASIA_PDS_DOUTI_USEISSUE_CLRMSK) | EURASIA_PDS_DOUTI_TEXISSUE_NONE;

			/* Always perspective correct iteration to the USE */
			pui32TSPParamFetchInterface[i] |= EURASIA_PDS_DOUTI_USEPERSPECTIVE;

			/* No wrapping */
			pui32TSPParamFetchInterface[i] |= (0 << EURASIA_PDS_DOUTI_USEWRAP_SHIFT);

			/* How many primary attribute registers does this load consume?	*/
			if( (ui32IteratorNumber == USP_HW_PSINPUT_COORD_V0) ||
				(ui32IteratorNumber == USP_HW_PSINPUT_COORD_V1) )
			{
			    pui32TSPParamFetchInterface[i] |= EURASIA_PDS_DOUTI_USEDIM_4D;

				if(psTextureLoads[i].eFormat == USP_HW_PSINPUT_FMT_F32 ||
					psTextureLoads[i].eFormat == USP_HW_PSINPUT_FMT_F16)
				{
					pui32TSPParamFetchInterface[i] |= EURASIA_PDS_DOUTI_USECOLFLOAT;
				}
			}
			else
			{
			    pui32TSPParamFetchInterface[i] |= (psTextureLoads[i].uCoordDim - 1U) << EURASIA_PDS_DOUTI_USEDIM_SHIFT;
			}

			/*  Format of Iterated data sent to the USE.

				The USE_FORMAT is only honoured when
				the USE_ISSUE is a floating point/integer colour or a texture coordinate set
				ie Fog and position data are always sent as F32.  When the USE_FORMAT is "00",
				in order to remain backwards compatible with SGX535, the iterators output in
				INT8 format only when USE_FLOATCOL is 0 and the USE_ISSUE is base or highlight
				colour. F32's are produced in all other cases.
			*/

			switch(psTextureLoads[i].eFormat)
			{
				case USP_HW_PSINPUT_FMT_F32:
				case USP_HW_PSINPUT_FMT_U8:
					pui32TSPParamFetchInterface[i] |= EURASIA_PDS_DOUTI_USEFORMAT_INT8;
					break;
				case USP_HW_PSINPUT_FMT_F16:
					pui32TSPParamFetchInterface[i] |= EURASIA_PDS_DOUTI_USEFORMAT_F16;
					break;
				case USP_HW_PSINPUT_FMT_C10:
					pui32TSPParamFetchInterface[i] |= EURASIA_PDS_DOUTI_USEFORMAT_INT10;
					break;
				default:
					PVR_DPF((PVR_DBG_ERROR,"SetupIteratorsAndTextureLookups: Unknown USP_HW_PSINPUT_FMT!\n"));
					break;
			}

#if defined(SGX_FEATURE_PERLAYER_POINTCOORD)
			if (gc->sProgram.psCurrentProgram->sFragment.psActiveSpecials[GLES2_SPECIAL_POINTCOORD] && 
				(gc->sProgram.psCurrentProgram->aui32IteratorPosition[ui32IteratorNumber] == 0))
			{
#if defined(SGX545)
				psPDSInfo->aui32TSPParamFetchInterface1[i] |= EURASIA_PDS_DOUTI1_UPOINTSPRITE_FORCE;
#else
				pui32TSPParamFetchInterface[i] |= EURASIA_PDS_DOUTI_USE_POINTSPRITE_FORCED;
#endif
			}
#endif

			/* Record this as the last USE issue */
			ui32LastUSEIssue = i;

			pui32LayerControl[i] = 0xFFFFFFFFU;
		}
		else
		{
			ui32TextureUnit	= psTextureControlWords[psTextureLoads[i].uTexCtrWrdIdx].uTextureIdx;

#if defined(SGX_FEATURE_CEM_S_USES_PROJ)
			ui32ChunkNumber = psTextureControlWords[psTextureLoads[i].uTexCtrWrdIdx].uChunkIdx + 
								(ui32TextureUnit * PDS_NUM_TEXTURE_IMAGE_CHUNKS);
#endif

			if (ui32IteratorNumber < NUM_TC_REGISTERS)
			{
				ui32InputCoordUnit = (gc->sProgram.psCurrentProgram->aui32IteratorPosition[ui32IteratorNumber] + 
						(EURASIA_PDS_DOUTI_TEXISSUE_TC0 >> EURASIA_PDS_DOUTI_TEXISSUE_SHIFT)) << EURASIA_PDS_DOUTI_TEXISSUE_SHIFT;
			}

		    pui32TSPParamFetchInterface[i] = (ui32InputCoordUnit & ~EURASIA_PDS_DOUTI_TEXISSUE_CLRMSK) | EURASIA_PDS_DOUTI_USEISSUE_NONE;

			/* No wrapping */
			pui32TSPParamFetchInterface[i] |= (0 << EURASIA_PDS_DOUTI_TEXWRAP_SHIFT);

#if defined(SGX_FEATURE_CEM_S_USES_PROJ)
			/*
				Force 3rd coordinate to be iterated for CEM.
			*/
			
			if((gc->sPrim.sFragmentTextureState.aui32TAGControlWord[ui32ChunkNumber][1] & ~EURASIA_PDS_DOUTT1_TEXTYPE_CLRMSK) ==
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
			if (psTextureLoads[i].uFlags & USP_HW_PSINPUT_FLAG_PROJECTED)
			{
				pui32TSPParamFetchInterface[i] |= EURASIA_PDS_DOUTI_TEXPROJ_T;
			}
			else
			{
				pui32TSPParamFetchInterface[i] |= EURASIA_PDS_DOUTI_TEXPROJ_RHW;
			}


#if defined(SGX_FEATURE_PERLAYER_POINTCOORD)
			if (gc->sProgram.psCurrentProgram->sFragment.psActiveSpecials[GLES2_SPECIAL_POINTCOORD] && 
				(gc->sProgram.psCurrentProgram->aui32IteratorPosition[ui32IteratorNumber] == 0))
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

			pui32LayerControl[i] = psTextureLoads[i].uTexCtrWrdIdx;

			psPDSInfo->ui32NonDependentImageUnits |= (1U << ui32TextureUnit);
		}

		ui32PACount += psTextureLoads[i].uDataSize;

		pui32TSPParamFetchInterface[i] |= EURASIA_PDS_DOUTI_FLATSHADE_GOURAUD;
	}

#if defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728)
    if(bIteratePosition)
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


		pui32LayerControl[i] = 0xFFFFFFFFU;

		GLES_ASSERT(ui32PACount <= psFragmentVariant->ui32USEPrimAttribCount);

		*pui32PositionPAReg = ui32PACount;

		/* We use another 3 PAs if iterating position */
		psFragmentVariant->ui32USEPrimAttribCount += 3;

		/* Record this as the last USE issue */
		ui32LastUSEIssue = i;

		i++;
#endif /* defined(FIX_HW_BRN_29625) */
    }
#endif /* defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728) */

#if defined(SGX_FEATURE_ALPHATEST_AUTO_COEFF)
	PVR_UNREFERENCED_PARAMETER(bIteratePlaneCoefficients);
	PVR_UNREFERENCED_PARAMETER(pui32PlaneCoeffPAReg);
#else
	/* If plane coefficients are required, they must be the last 
	   iteration command sent to the USE */
	if (bIteratePlaneCoefficients)
	{
		pui32TSPParamFetchInterface[i] |=	EURASIA_PDS_DOUTI_USEISSUE_ZABS		|
											EURASIA_PDS_DOUTI_TEXISSUE_NONE		|
											EURASIA_PDS_DOUTI_FLATSHADE_GOURAUD	|
											EURASIA_PDS_DOUTI_USEPERSPECTIVE	|
											EURASIA_PDS_DOUTI_USEDIM_3D;

		pui32LayerControl[i] = 0xFFFFFFFFU;

		GLES_ASSERT(ui32PACount <= psFragmentVariant->ui32USEPrimAttribCount);

		*pui32PlaneCoeffPAReg = ui32PACount;

		/* We use another 3 PAs if iterating plane coefficients */
		psFragmentVariant->ui32USEPrimAttribCount += 3;

#if defined(FIX_HW_BRN_31547)
		if(gc->psMode->ui32AntiAliasMode)
		{
			/* Dimension = 4 */
			pui32TSPParamFetchInterface[i] &= EURASIA_PDS_DOUTI_USEDIM_CLRMSK;
			pui32TSPParamFetchInterface[i] |= EURASIA_PDS_DOUTI_USEDIM_4D;

			/* Iterating 4 values */
			psFragmentVariant->ui32USEPrimAttribCount++;

			/* Skip past per-primitive sample position */
			*pui32PlaneCoeffPAReg = ui32PACount + 1;
		}
#endif /* defined(FIX_HW_BRN_31547) */

		/* Record this as the last USE issue */
		ui32LastUSEIssue = i;

		i++;
	}
#endif

	psPDSInfo->ui32IterationCount = i;

	if (i > GLES2_MAX_DOUTI)
	{
		PVR_DPF((PVR_DBG_ERROR,"SetupIteratorsAndTextureLookups: exceeded maximum number of iteration commands (SW limit)"));
	}

	if(psFragmentVariant->ui32USEPrimAttribCount > 64)
	{
		PVR_DPF((PVR_DBG_ERROR,"SetupIteratorsAndTextureLookups: exceeded maximum number of HW iterated components"));
	}

	/*
		Set the bits that mark the last USE/texture issues
	*/
	if (ui32LastUSEIssue != 0xFFFFFFFFU/*(IMG_UINT32)-1*/)
	{
		GLES_ASSERT(psPDSInfo->ui32IterationCount > 0);
		pui32TSPParamFetchInterface[ui32LastUSEIssue] |= EURASIA_PDS_DOUTI_USELASTISSUE;
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
		ui32DependencyControl |= EURASIA_PDS_DOUTU1_ITERATORSDEPENDENCY;
#else
		ui32DependencyControl |= EURASIA_PDS_DOUTU0_ITERATORSDEPENDENCY;
#endif
	}
	if (ui32LastTexIssue != 0xFFFFFFFFU)
	{
		GLES_ASSERT(psPDSInfo->ui32IterationCount > 0);
		pui32TSPParamFetchInterface[ui32LastTexIssue] |= EURASIA_PDS_DOUTI_TEXLASTISSUE;
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
		ui32DependencyControl |= EURASIA_PDS_DOUTU1_TEXTUREDEPENDENCY;
#else 
		ui32DependencyControl |= EURASIA_PDS_DOUTU0_TEXTUREDEPENDENCY;
#endif
	}

	psPDSInfo->ui32DependencyControl	= ui32DependencyControl;
}


#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
/********************************* Shader layout **************************************
 *
 *                               Returned from Unipatch
 *                          |----------------------------------|
 *							v	                               v If LAE
 * Option 1a : No Discard   +--------------+----+--------------+-------+
 *		LAE = Label At End	| Shader (fns) |PHAS| Shader (main)|NOP.end|
 *                          +--------------+----+--------------+-------+
 *							               ^				   
 *							               Phase 0 Start	   
 *
 *                               Returned from Unipatch
 *                          |----------------------------------|
 *							v	                               v  
 * Option 1b : No Discard   +--------------+----+--------------+-----+
 *			   Blending   	| Shader (fns) |PHAS| Shader (main)|Blend|
 *                          +--------------+----+--------------+-----+
 *							               ^				            
 *							               Phase 0 Start	            
 *
 *                               Returned from Unipatch
 *                          |----------------------------------|
 *							v	                               v If LAE  
 * Option 1c : No Discard   +--------------+----+--------------+-------+----------+
 *			  MSAA Blending | Shader (fns) |PHAS| Shader (main)|NOP.end|PHAS+Blend|
 *      LAE = Label At End  +--------------+----+--------------+-------+----------+
 *							               ^				           ^
 *							               Phase 0 Start	           Phase 1 Start
 *
 *
 *                                         Returned from Unipatch
 *                          |------------------------------------------------|
 *							v	                                             v
 * Option 2a : Discard      +--------------+--------+--------------+-----+---+----+
 *			  No Split  	| Shader (fns) |PHAS/PCF| Shader (main)|ATST8|MOV|PHAS|
 *            PCF=PCOEFF    +--------------+--------+--------------+-----+---+----+
 *                                         ^						     ^
 *										   Phase 0 Start				 Phase 1 Start
 *
 *                                         Returned from Unipatch
 *                          |--------------------------------------------|
 *							v	                                         v
 * Option 2b : Discard      +--------------+--------+--------------+-----+----------+
 *		       No Split  	| Shader (fns) |PHAS/PCF| Shader (main)|ATST8|Blend+PHAS|
 *       (MSAA) Blending	+--------------+--------+--------------+-----+----------+
 *		       PCF=PCOEFF                  ^					         ^
 *										   Phase 0 Start				 Phase 1 Start (Selective rate)
 *
 *
 *                                              Returned from Unipatch
 *                          |-------------------------------------------------------------|
 *							v	                                                          v			
 * Option 3a :Discard       +--------------+--------+---------------+-----+---------------+----+
 *			  Split  	    | Shader (fns) |PHAS/PCF| Shader Part 1 |ATST8| Shader Part 2 |PHAS|
 *            PCF=PCOEFF    +--------------+--------+---------------+-----+---------------+----+
 *							               ^						      ^                          
 *										   Phase 0 Start			      Phase 1 Start		       
 *
 *
 *                                              Returned from Unipatch
 *                          |-------------------------------------------------------------|
 *							v	                                                          v	
 * Option 3b :Discard       +--------------+--------+---------------+-----+---------------+----------+
 *			  Split  	    | Shader (fns) |PHAS/PCF| Shader Part 1 |ATST8| Shader Part 2 |Blend+PHAS|
 *            Blending      +--------------+--------+---------------+-----+---------------+----------+
 *			  PCF=PCOEFF			       ^						      ^                          
 *										   Phase 0 Start			      Phase 1 Start		       
 *
 *
 *                                              Returned from Unipatch
 *                          |-------------------------------------------------------------|
 *							v	                                                          v		
 * Option 3c :Discard       +--------------+--------+---------------+-----+---------------+----+------ ---+
 *			  Split  	    | Shader (fns) |PHAS/PCF| Shader Part 1 |ATST8| Shader Part 2 |PHAS|PHAS+Blend|
 *            MSAA Blending +--------------+--------+---------------+-----+---------------+----+----------+
 *		      PCF=PCOEFF				   ^						      ^                    ^
 *										   Phase 0 Start			      Phase 1 Start		   Phase 2 Start (Selective rate)
 *
 ****************************************************************************************/

/****************************************************************
 * Function Name  	: SetupUSEFragmentShader
 * Returns        	: Error code
 * Outputs			: pbProgramChanged
 * Globals Used    	: 
 * Description    	: 
 ****************************************************************/


IMG_INTERNAL GLES2_MEMERROR SetupUSEFragmentShader(GLES2Context *gc, IMG_BOOL *pbProgramChanged)
{
	GLES2CompiledRenderState *psRenderState = &gc->sPrim.sRenderState;
	GLES2CompiledTextureState *psFragmentTextureState = &gc->sPrim.sFragmentTextureState;
	GLES2ProgramShader *psFragmentShader = &gc->sProgram.psCurrentProgram->sFragment;
	GLES2USEShaderVariant *psFragmentVariant = psFragmentShader->psVariant;
	USP_HW_SHADER *psPatchedShader;
	IMG_UINT32 ui32PlaneCoeffPAReg = 0;
	IMG_UINT32 aui32FBBlendUSECode[GLES2_FBBLEND_MAX_CODE_SIZE_IN_DWORDS];
	IMG_UINT32 aui32ColorMaskUSECode[GLES2_COLORMASK_MAX_CODE_SIZE_IN_DWORDS];
	IMG_UINT32 ui32FragmentShaderCodeSizeInBytes;
	IMG_BOOL bColorMaskCode = IMG_FALSE;
	IMG_UINT32 ui32FBBlendUSECodeSizeInBytes;
	IMG_UINT32 ui32ColorMaskUSECodeSizeInBytes;
	IMG_UINT32 *pui32USECode;
	IMG_UINT32 ui32NumTemps = 0, ui32NumBlendTemps = 0;
	IMG_UINT32 ui32BlendEquation;
	IMG_UINT32 ui32ImageUnitEnables = psFragmentTextureState->ui32ImageUnitEnables;
	IMG_BOOL bMatch;
	IMG_BOOL bSeparateBlendPhase = IMG_FALSE;
	IMG_UINT16 i;
	IMG_UINT32 ui32PreambleCount = 0;
	GLES2_MEMERROR eError = GLES2_NO_ERROR;
	IMG_UINT32 ui32ExeAddr, ui32NextPhaseMode;
	IMG_UINT32 ui32IncrementalCodeSizeInUSEInsts;
#if defined(DEBUG)
	IMG_BOOL bNewShader = IMG_FALSE;
#endif

	if(gc->ui32Enables & GLES2_ALPHABLEND_ENABLE)
	{
		ui32BlendEquation = gc->sState.sRaster.ui32BlendEquation;
	}
	else
	{
		ui32BlendEquation =	(GLES2_BLENDFUNC_NONE << GLES2_BLENDFUNC_RGB_SHIFT) | 
							(GLES2_BLENDFUNC_NONE << GLES2_BLENDFUNC_ALPHA_SHIFT);
	}
	
	if(gc->sState.sRaster.ui32ColorMask && (gc->sState.sRaster.ui32ColorMask != GLES2_COLORMASK_ALL))
	{
		bColorMaskCode = IMG_TRUE;
	}

	if(gc->psMode->ui32AntiAliasMode && (ui32BlendEquation || bColorMaskCode))
	{
		bSeparateBlendPhase = IMG_TRUE;
	}

	while(psFragmentVariant)
	{
		if((psFragmentVariant->u.sFragment.ui32ColorMask == gc->sState.sRaster.ui32ColorMask) && 
			(psFragmentVariant->u.sFragment.ui32BlendEquation == ui32BlendEquation) && 
			(psFragmentVariant->u.sFragment.ui32BlendFactor == gc->sState.sRaster.ui32BlendFactor) && 
			(psFragmentVariant->u.sFragment.bSeparateBlendPhase == bSeparateBlendPhase) && 
			(psFragmentVariant->u.sFragment.ui32ImageUnitEnables == ui32ImageUnitEnables))
		{
			/* If raster state matches, check texture formats */
			bMatch = IMG_TRUE;

			for(i=0; i < GLES2_MAX_TEXTURE_UNITS; i++)
			{
				if(ui32ImageUnitEnables & (1 << i))
				{
					if(psFragmentVariant->u.sFragment.apsTexFormat[i] != psFragmentTextureState->apsTexFormat[i])
					{
						/* If we have failed the search - break to next variant */
						bMatch = IMG_FALSE;
						break;
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
		IMG_UINT32 ui32CodeSizeInBytes;
		IMG_UINT32 *pui32USECodeBase;
#if defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728)
		IMG_UINT32 ui32PositionPAReg;
#endif /* defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728) */

		psFragmentVariant = GLES2Calloc(gc, sizeof(GLES2USEShaderVariant));

		if(!psFragmentVariant)
		{
			return GLES2_HOST_MEM_ERROR;
		}

		/* Setup state to for variant */
		psFragmentVariant->u.sFragment.ui32BlendEquation = ui32BlendEquation;
		psFragmentVariant->u.sFragment.ui32BlendFactor = gc->sState.sRaster.ui32BlendFactor;
		psFragmentVariant->u.sFragment.ui32ColorMask = gc->sState.sRaster.ui32ColorMask;
		psFragmentVariant->u.sFragment.bSeparateBlendPhase = bSeparateBlendPhase; 
		psFragmentVariant->u.sFragment.ui32ImageUnitEnables = ui32ImageUnitEnables;

		if(ui32ImageUnitEnables)
		{
			for(i=0; i < GLES2_MAX_TEXTURE_UNITS; i++)
			{
				if(ui32ImageUnitEnables & (1 << i))
				{
					psFragmentVariant->u.sFragment.apsTexFormat[i] = psFragmentTextureState->apsTexFormat[i];
					
					PVRUniPatchSetTextureFormat(gc->sProgram.pvUniPatchContext,
												i,
												(USP_TEX_FORMAT *)&psFragmentTextureState->apsTexFormat[i]->sTexFormat,
												IMG_FALSE,
												IMG_FALSE);
				}
			}
		}

		/* Add 1 instruction (PHAS) space at the beginning of uProgStartInstIdx (UniPatchShader) */
		ui32PreambleCount++;
	
#if !defined(SGX_FEATURE_ALPHATEST_AUTO_COEFF)
		if(psRenderState->ui32AlphaTestFlags & GLES2_ALPHA_TEST_DISCARD)
		{
			/* Add instruction (pcoeff) space at the beginning of uProgStartInstIdx (UniPatchShader) */
			ui32PreambleCount++;
		}
#endif

#if defined(FIX_HW_BRN_29019)
		if((psRenderState->ui32AlphaTestFlags & GLES2_ALPHA_TEST_DISCARD)==0)
		{
			if(bSeparateBlendPhase)
			{
				/* Add instruction (MOV) space at the beginning of uProgStartInstIdx (UniPatchShader) */
				ui32PreambleCount++;
			}
		}
#endif /* defined(FIX_HW_BRN_29019) */


		/* If we are going to add on any code, don't bother patching */
		if(ui32BlendEquation || bColorMaskCode)
		{
			PVRUniPatchSetOutputLocation(gc->sProgram.pvUniPatchContext, USP_OUTPUT_REGTYPE_DEFAULT);
		}
		else
		{
			PVRUniPatchSetOutputLocation(gc->sProgram.pvUniPatchContext, USP_OUTPUT_REGTYPE_OUTPUT);
		}

		PVRUniPatchSetPreambleInstCount(gc->sProgram.pvUniPatchContext, ui32PreambleCount);

		psPatchedShader = PVRUniPatchFinaliseShader(gc->sProgram.pvUniPatchContext, 
													psFragmentShader->psSharedState->pvUniPatchShader);

		if(!psPatchedShader)
		{
			PVR_DPF((PVR_DBG_FATAL,"SetupUSEFragmentShader: Unipatch failed to finalise the shader"));
			SetError(gc, GL_OUT_OF_MEMORY);
			GLES2Free(IMG_NULL, psFragmentVariant);
			return GLES2_GENERAL_MEM_ERROR;
		}

		/* Create the secondary code block as necessary */
		if(psPatchedShader->uSAUpdateInstCount)
		{
			eError = SetupUSESecondaryUploadTask(gc, psPatchedShader, psFragmentShader->psSharedState, IMG_FALSE);

			if(eError != GLES2_NO_ERROR)
			{
				PVRUniPatchDestroyHWShader(gc->sProgram.pvUniPatchContext, psPatchedShader);
				GLES2Free(IMG_NULL, psFragmentVariant);
				return eError;
			}
		}

		psFragmentVariant->psPatchedShader = psPatchedShader;
		psFragmentVariant->psProgramShader = psFragmentShader;
		psFragmentVariant->ui32USEPrimAttribCount = psPatchedShader->uPARegCount;
		psFragmentVariant->psSecondaryUploadTask = psFragmentShader->psSharedState->psSecondaryUploadTask;

		/* Set current variant */
		gc->sProgram.psCurrentFragmentVariant = psFragmentVariant;

		ui32FragmentShaderCodeSizeInBytes = psPatchedShader->uInstCount * EURASIA_USE_INSTRUCTION_SIZE;

		ui32CodeSizeInBytes = ui32FragmentShaderCodeSizeInBytes;

		/* 
			Generate pixel shader iterators and texture look ups 
			If a discard/ISP feedback is enabled we need to iterate 
			the triangle plane coefficients 
		*/

		SetupIteratorsAndTextureLookups(gc,
										psPatchedShader->uPSInputCount,
										psPatchedShader->psPSInputLoads,
										psPatchedShader->psTextCtrWords,
#if defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728)
										(psRenderState->ui32AlphaTestFlags & GLES1_ALPHA_TEST_BRNFIX_DEPTHF) ? IMG_TRUE : IMG_FALSE,
										&ui32PositionPAReg,
#endif /* defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728) */
										psRenderState->ui32AlphaTestFlags ? IMG_TRUE : IMG_FALSE,
										&ui32PlaneCoeffPAReg);

		psFragmentVariant->ui32PhaseCount = 0;

		ui32FBBlendUSECodeSizeInBytes = 0;
		ui32ColorMaskUSECodeSizeInBytes = 0;

		/* Create the FB blending code if required */
		if(ui32BlendEquation)
		{
		    ui32FBBlendUSECodeSizeInBytes = CreateFBBlendUSECode(gc,
																 psPatchedShader->uPSResultRegNum,
																 bColorMaskCode,
																 psRenderState->ui32AlphaTestFlags ? IMG_TRUE : IMG_FALSE,
																 aui32FBBlendUSECode,
																 &ui32NumBlendTemps);
		}
	
		/* Create the colormask code if required */
		if(bColorMaskCode)
		{
			/* Blending will output to reg 0, otherwise use shader output reg number */
			ui32ColorMaskUSECodeSizeInBytes = CreateColorMaskUSECode(gc, 
																	ui32BlendEquation ? IMG_TRUE : IMG_FALSE,
																	ui32BlendEquation ? 0 : psPatchedShader->uPSResultRegNum,
																	 aui32ColorMaskUSECode);
		}

		ui32CodeSizeInBytes += (ui32FBBlendUSECodeSizeInBytes + ui32ColorMaskUSECodeSizeInBytes);

		psFragmentVariant->ui32PhaseCount++;

		if(psRenderState->ui32AlphaTestFlags & GLES2_ALPHA_TEST_DISCARD)
		{
			/* Adding a PHAS for second phase */
			ui32CodeSizeInBytes += EURASIA_USE_INSTRUCTION_SIZE;
			psFragmentVariant->ui32PhaseCount++;
	
			if(bSeparateBlendPhase && (psPatchedShader->uFlags & USP_HWSHADER_FLAGS_SPLITALPHACALC))
			{
				/* Adding a PHAS for third phase */
				ui32CodeSizeInBytes += EURASIA_USE_INSTRUCTION_SIZE;
				psFragmentVariant->ui32PhaseCount++;
			}
		}
		else
		{
			if(psPatchedShader->uFlags & USP_HWSHADER_FLAGS_LABEL_AT_END)
			{
				if(bSeparateBlendPhase || (!ui32BlendEquation && !bColorMaskCode))
				{
					/* Adding a nop due to last shader instruction not accepting end flag */
					ui32CodeSizeInBytes += EURASIA_USE_INSTRUCTION_SIZE;
				}
			}

			if(bSeparateBlendPhase)
			{
				/* Adding a PHAS for second phase */
				ui32CodeSizeInBytes += EURASIA_USE_INSTRUCTION_SIZE;
				psFragmentVariant->ui32PhaseCount++;
			}
		}
		
		GLES2_TIME_START(GLES2_TIMER_USECODEHEAP_FRAG_TIME);

		psFragmentVariant->psCodeBlock = UCH_CodeHeapAllocate(gc->psSharedState->psUSEFragmentCodeHeap, ui32CodeSizeInBytes, gc->psSysContext->hPerProcRef);
	
		GLES2_TIME_STOP(GLES2_TIMER_USECODEHEAP_FRAG_TIME);

		if(!psFragmentVariant->psCodeBlock)
		{
			/* Destroy unused fragment shaders and retry */
			KRM_ReclaimUnneededResources(gc, &gc->psSharedState->sUSEShaderVariantKRM);
			
			GLES2_TIME_START(GLES2_TIMER_USECODEHEAP_FRAG_TIME);

			psFragmentVariant->psCodeBlock = UCH_CodeHeapAllocate(gc->psSharedState->psUSEFragmentCodeHeap, ui32CodeSizeInBytes, gc->psSysContext->hPerProcRef);
		
			GLES2_TIME_STOP(GLES2_TIMER_USECODEHEAP_FRAG_TIME);
			
			if(!psFragmentVariant->psCodeBlock)
			{
				gc->sProgram.psCurrentFragmentVariant = IMG_NULL;
				USESecondaryUploadTaskDelRef(gc, psFragmentVariant->psSecondaryUploadTask);
				PVRUniPatchDestroyHWShader(gc->sProgram.pvUniPatchContext, psPatchedShader);
				GLES2Free(IMG_NULL, psFragmentVariant);
				return GLES2_3D_USECODE_ERROR;
			}
		}

		GLES2_INC_COUNT(GLES2_TIMER_USECODEHEAP_FRAG_COUNT, ui32CodeSizeInBytes >> 2);

		psFragmentVariant->sStartAddress[0].uiAddr = psFragmentVariant->psCodeBlock->sCodeAddress.uiAddr + 
												(psPatchedShader->uProgStartInstIdx * EURASIA_USE_INSTRUCTION_SIZE);
		
		psFragmentVariant->ui32MaxTempRegs = psPatchedShader->uTempRegCount;
		pui32USECode = psFragmentVariant->psCodeBlock->pui32LinAddress;
			
		/* First copy fragment shader code */
		GLES2MemCopy(pui32USECode, 
					 psPatchedShader->puInsts, 
					 ui32FragmentShaderCodeSizeInBytes);

		
		pui32USECodeBase = pui32USECode;
		ui32IncrementalCodeSizeInUSEInsts = (ui32CodeSizeInBytes >> 3);

		if(psPatchedShader->uFlags & UNIFLEX_HW_FLAGS_PER_INSTANCE_MODE)
		{
			ui32NextPhaseMode = EURASIA_USE_OTHER2_PHAS_MODE_PERINSTANCE;
			psFragmentShader->ui32USEMode = EURASIA_PDS_DOUTU1_MODE_PERINSTANCE;
		}
		else
		{
			ui32NextPhaseMode = EURASIA_USE_OTHER2_PHAS_MODE_PARALLEL;
			psFragmentShader->ui32USEMode = EURASIA_PDS_DOUTU1_MODE_PARALLEL;
		}

		/* Next copy pcoeff instruction as necessary */
		if(psRenderState->ui32AlphaTestFlags & GLES2_ALPHA_TEST_DISCARD)
		{
			IMG_UINT32 ui32NextPhaseRate;

			psFragmentVariant->sStartAddress[1].uiAddr = psFragmentVariant->psCodeBlock->sCodeAddress.uiAddr + 
												(psPatchedShader->uPTPhase1StartInstIdx * EURASIA_USE_INSTRUCTION_SIZE);

			/* Preamble code has been reserved at start of main code */
			pui32USECode = (IMG_UINT32 *)((IMG_UINT8 *)psFragmentVariant->psCodeBlock->pui32LinAddress + 
										(psPatchedShader->uProgStartInstIdx * EURASIA_USE_INSTRUCTION_SIZE));


			ui32ExeAddr = GetUSEPhaseAddress(psFragmentVariant->sStartAddress[1],
											 gc->psSysContext->uUSEFragmentHeapBase, 
											SGX_PIXSHADER_USE_CODE_BASE_INDEX);

			/* Only 2 phases with MSAA - so next one needs to be selective */ 
			if(bSeparateBlendPhase && (psFragmentVariant->ui32PhaseCount == 2))
			{
				ui32NextPhaseRate = EURASIA_USE_OTHER2_PHAS_RATE_SELECTIVE;
				ui32NumTemps = ALIGNCOUNTINBLOCKS(ui32NumBlendTemps, EURASIA_USE_OTHER2_PHAS_TCOUNT_ALIGNSHIFT);
			}
			else
			{
				ui32NextPhaseRate = EURASIA_USE_OTHER2_PHAS_RATE_PIXEL;
				ui32NumTemps = ALIGNCOUNTINBLOCKS(psPatchedShader->uTempRegCount, EURASIA_USE_OTHER2_PHAS_TCOUNT_ALIGNSHIFT);
			}

#if !defined(FIX_HW_BRN_29019)

			BuildPHASImmediate((PPVR_USE_INST)	pui32USECode,
												0, /* ui32End */
												ui32NextPhaseMode,
												ui32NextPhaseRate,
												EURASIA_USE_OTHER2_PHAS_WAITCOND_PT,
												ui32NumTemps,
												ui32ExeAddr);
			
			pui32USECode += USE_INST_LENGTH;

#if !defined(SGX_FEATURE_ALPHATEST_AUTO_COEFF)
#if defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728)
			if(psRenderState->ui32AlphaTestFlags & GLES1_ALPHA_TEST_BRNFIX_DEPTHF)
			{
				if(!psFragmentVariant->ui32USEPrimAttribCount)
				{
					psFragmentVariant->ui32USEPrimAttribCount++;
				}

				pui32USECode[0] = (EURASIA_USE0_S1STDBANK_PRIMATTR << EURASIA_USE0_S1BANK_SHIFT) |
									 (EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT) |
									 (0 << EURASIA_USE0_DST_SHIFT) |
									 (0 << EURASIA_USE0_SRC1_SHIFT);
				
				pui32USECode[1] = (EURASIA_USE1_OP_MOVC << EURASIA_USE1_OP_SHIFT) |
									 (EURASIA_USE1_MOVC_TSTDTYPE_UNCOND << EURASIA_USE1_MOVC_TSTDTYPE_SHIFT) |
									 (EURASIA_USE1_EPRED_ALWAYS << EURASIA_USE1_EPRED_SHIFT) |
									 (0) |	/* DestBExt */
									 (0) | /* Src1BExt */
									 EURASIA_USE1_S2BEXT |
									 (0 << EURASIA_USE1_RMSKCNT_SHIFT) |
									 (0 << EURASIA_USE1_SRC1MOD_SHIFT) |
									 (EURASIA_USE1_D1STDBANK_PRIMATTR << EURASIA_USE1_D1BANK_SHIFT) |
									 EURASIA_USE1_RCNTSEL |
									 EURASIA_USE1_SKIPINV;
			}
			else
#endif /* defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728) */
			{
				/* PCOEFF */
				pui32USECode[0] = GLES2_USE_SRC1(PRIMATTR, ui32PlaneCoeffPAReg+1)					|
								GLES2_USE_SRC2(SECATTR, GLES2_FRAGMENT_SECATTR_ISPFEEDBACK1)	|
								GLES2_USE0_SRC0_NUM(ui32PlaneCoeffPAReg);
			
				pui32USECode[1] = GLES2_USE1_OP(SPECIAL)																|
								(EURASIA_USE1_VISTEST_OP2_PTCTRL << EURASIA_USE1_OTHER_OP2_SHIFT)					|
								(EURASIA_USE1_SPECIAL_OPCAT_VISTEST << EURASIA_USE1_SPECIAL_OPCAT_SHIFT)			|
								(EURASIA_USE1_VISTEST_PTCTRL_TYPE_PLANE << EURASIA_USE1_VISTEST_PTCTRL_TYPE_SHIFT)	|
								(EURASIA_USE1_S0STDBANK_PRIMATTR << EURASIA_USE1_S0BANK_SHIFT);
			}

#endif /* !SGX_FEATURE_ALPHATEST_AUTO_COEFF */

#else /* !defined(FIX_HW_BRN_29019) */

#if defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728)
			if(psRenderState->ui32AlphaTestFlags & GLES1_ALPHA_TEST_BRNFIX_DEPTHF)
			{
				if(!psFragmentVariant->ui32USEPrimAttribCount)
				{
					psFragmentVariant->ui32USEPrimAttribCount++;
				}

				pui32USECode[0] = (EURASIA_USE0_S1STDBANK_PRIMATTR << EURASIA_USE0_S1BANK_SHIFT) |
									 (EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT) |
									 (0 << EURASIA_USE0_DST_SHIFT) |
									 (0 << EURASIA_USE0_SRC1_SHIFT);
				
				pui32USECode[1] = (EURASIA_USE1_OP_MOVC << EURASIA_USE1_OP_SHIFT) |
									 (EURASIA_USE1_MOVC_TSTDTYPE_UNCOND << EURASIA_USE1_MOVC_TSTDTYPE_SHIFT) |
									 (EURASIA_USE1_EPRED_ALWAYS << EURASIA_USE1_EPRED_SHIFT) |
									 (0) |	/* DestBExt */
									 (0) | /* Src1BExt */
									 EURASIA_USE1_S2BEXT |
									 (0 << EURASIA_USE1_RMSKCNT_SHIFT) |
									 (0 << EURASIA_USE1_SRC1MOD_SHIFT) |
									 (EURASIA_USE1_D1STDBANK_PRIMATTR << EURASIA_USE1_D1BANK_SHIFT) |
									 EURASIA_USE1_RCNTSEL |
									 EURASIA_USE1_SKIPINV;
			}
			else
#endif /* defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728) */
			{
				/* PCOEFF */
				pui32USECode[0] = GLES2_USE_SRC1(PRIMATTR, ui32PlaneCoeffPAReg+1)					|
								  GLES2_USE_SRC2(SECATTR, GLES2_FRAGMENT_SECATTR_ISPFEEDBACK1)	|
								  GLES2_USE0_SRC0_NUM(ui32PlaneCoeffPAReg);
			
				pui32USECode[1] = GLES2_USE1_OP(SPECIAL)																|
								 (EURASIA_USE1_VISTEST_OP2_PTCTRL << EURASIA_USE1_OTHER_OP2_SHIFT)					|
								 (EURASIA_USE1_SPECIAL_OPCAT_VISTEST << EURASIA_USE1_SPECIAL_OPCAT_SHIFT)			|
								 (EURASIA_USE1_VISTEST_PTCTRL_TYPE_PLANE << EURASIA_USE1_VISTEST_PTCTRL_TYPE_SHIFT)	|
								 (EURASIA_USE1_S0STDBANK_PRIMATTR << EURASIA_USE1_S0BANK_SHIFT);
			}

			pui32USECode += USE_INST_LENGTH;

			BuildPHASImmediate((PPVR_USE_INST)	pui32USECode,
												0, /* ui32End */
												ui32NextPhaseMode,
												ui32NextPhaseRate,
												EURASIA_USE_OTHER2_PHAS_WAITCOND_PT,
												ui32NumTemps,
												ui32ExeAddr);

#endif /* !defined(FIX_HW_BRN_29019) */

			/* Next fill in ISP feedback code */
			pui32USECode = (IMG_UINT32 *)((IMG_UINT8 *)psFragmentVariant->psCodeBlock->pui32LinAddress + 
											(psPatchedShader->uPTPhase0EndInstIdx * EURASIA_USE_INSTRUCTION_SIZE));
			
#if defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728)
			if(psRenderState->ui32AlphaTestFlags & GLES1_ALPHA_TEST_BRNFIX_DEPTHF)
			{
				BuildDEPTHF((PPVR_USE_INST)	&pui32USECode,
							EURASIA_USE1_SPRED_P1,
							EURASIA_USE1_END,
		                    USE_REGTYPE_PRIMATTR,
							ui32PositionPAReg+2,
							USE_REGTYPE_SECATTR,
							GLES2_FRAGMENT_SECATTR_ISPFEEDBACK0,
							USE_REGTYPE_SECATTR,
							GLES2_FRAGMENT_SECATTR_ISPFEEDBACK1);
			}
			else
#endif /* defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728) */
			{
				IMG_UINT32 ui32EndFlag = EURASIA_USE1_END;

#if defined(FIX_HW_BRN_33668)
				ui32EndFlag = 0;
#endif

				/* ATST8 - writes the colour result to uPSResultRegNum. Temporaries are not preserved between phases */
				pui32USECode[0] = GLES2_USE_SRC1(SECATTR, GLES2_FRAGMENT_SECATTR_ISPFEEDBACK0)			|
										GLES2_USE_SRC2(SECATTR, GLES2_FRAGMENT_SECATTR_ISPFEEDBACK1)	|
										GLES2_USE0_SRC0_NUM(psPatchedShader->uPSResultRegNum)			|
										GLES2_USE0_DST_NUM(psPatchedShader->uPSResultRegNum);

				/* Texkill result is in predicate P1. Mark end of first phase */
				pui32USECode[1] = GLES2_USE1_OP(SPECIAL)													|
								(EURASIA_USE1_VISTEST_OP2_ATST8 << EURASIA_USE1_OTHER_OP2_SHIFT)			|
								(EURASIA_USE1_SPECIAL_OPCAT_VISTEST << EURASIA_USE1_SPECIAL_OPCAT_SHIFT)	|
								(EURASIA_USE1_S0STDBANK_PRIMATTR << EURASIA_USE1_S0BANK_SHIFT)				|
								GLES2_USE1_DST_TYPE(PRIMATTR)												|
								EURASIA_USE1_SPRED_P1 << EURASIA_USE1_VISTEST_ATST8_SPRED_SHIFT				|
								ui32EndFlag;

#if defined(FIX_HW_BRN_33668)
				pui32USECode += USE_INST_LENGTH;

				BuildNOP((PPVR_USE_INST)pui32USECode, EURASIA_USE1_END, IMG_FALSE);
#endif
			}	

			if(psFragmentVariant->ui32PhaseCount == 3)
			{
				/* Jump to the end of phase 1 code to insert next PHAS */
				pui32USECode = (IMG_UINT32 *)((IMG_UINT8 *)psFragmentVariant->psCodeBlock->pui32LinAddress + ui32FragmentShaderCodeSizeInBytes);
			
				/* Setup address for 2nd PHAS to point to */
				psFragmentVariant->sStartAddress[2].uiAddr = psFragmentVariant->psCodeBlock->sCodeAddress.uiAddr + 
															 ui32FragmentShaderCodeSizeInBytes + EURASIA_USE_INSTRUCTION_SIZE;
				
				ui32NumTemps = ALIGNCOUNTINBLOCKS(ui32NumBlendTemps, EURASIA_USE_OTHER2_PHAS_TCOUNT_ALIGNSHIFT);

				ui32ExeAddr = GetUSEPhaseAddress(psFragmentVariant->sStartAddress[2],
												gc->psSysContext->uUSEFragmentHeapBase, 
												SGX_PIXSHADER_USE_CODE_BASE_INDEX);

				/* Second PHAS points to third PHAS */
				BuildPHASImmediate((PPVR_USE_INST)	pui32USECode,
													EURASIA_USE1_OTHER2_PHAS_END,
													EURASIA_USE_OTHER2_PHAS_MODE_PARALLEL,
													EURASIA_USE_OTHER2_PHAS_RATE_SELECTIVE,
													EURASIA_USE_OTHER2_PHAS_WAITCOND_NONE,
													ui32NumTemps,
													ui32ExeAddr);

				pui32USECode += USE_INST_LENGTH;
			
				BuildPHASLastPhase ((PPVR_USE_INST) pui32USECode, 0);

				/* Setup blend code insertion point */
				pui32USECode += USE_INST_LENGTH;
			}
			else
			{
				/* Jump to the end of code to insert final PHAS */
				pui32USECode = (IMG_UINT32 *)( (IMG_UINT8 *)psFragmentVariant->psCodeBlock->pui32LinAddress +
											ui32CodeSizeInBytes - EURASIA_USE_INSTRUCTION_SIZE);
				
				BuildPHASLastPhase ((PPVR_USE_INST) pui32USECode, EURASIA_USE1_OTHER2_PHAS_END);

				/* Setup blend code insertion point */
				pui32USECode = (IMG_UINT32 *)( (IMG_UINT8 *)psFragmentVariant->psCodeBlock->pui32LinAddress + ui32FragmentShaderCodeSizeInBytes);

			}
		
		}
		else
		{
			IMG_UINT32 ui32PhasEnd = 0;

			/* If there is no shader code, PHAS is last instruction */
			if(psPatchedShader->uInstCount == ui32PreambleCount)
			{
				ui32PhasEnd = EURASIA_USE1_OTHER2_PHAS_END;
			}

			if(bSeparateBlendPhase || (!ui32BlendEquation && !bColorMaskCode))
			{
				pui32USECode = (IMG_UINT32 *)((IMG_UINT8 *)psFragmentVariant->psCodeBlock->pui32LinAddress + ui32FragmentShaderCodeSizeInBytes);
				
				if(psPatchedShader->uFlags & USP_HWSHADER_FLAGS_LABEL_AT_END)
				{
					/* Adding a nop due to last shader instruction not accepting end flag */
					BuildNOP((PPVR_USE_INST)pui32USECode, EURASIA_USE1_END, IMG_FALSE);
				}
				else if(ui32PhasEnd == 0)
				{
					/* Terminate this block of code (as long as there is actually some code to terminate and not just a PHAS) */
					pui32USECode[-1] |= EURASIA_USE1_END;
				}
			}

			/* Space for PHAS instruction has been reserved at start of main code */
			pui32USECode = (IMG_UINT32 *)((IMG_UINT8 *)psFragmentVariant->psCodeBlock->pui32LinAddress + 
										  (psPatchedShader->uProgStartInstIdx * EURASIA_USE_INSTRUCTION_SIZE));

			/* Insert PHAS instruction(s) */
			if(bSeparateBlendPhase)
			{
				/* Setup address for 1st PHAS to point to */
				psFragmentVariant->sStartAddress[1].uiAddr = psFragmentVariant->psCodeBlock->sCodeAddress.uiAddr + ui32FragmentShaderCodeSizeInBytes;
				
				if(psPatchedShader->uFlags & USP_HWSHADER_FLAGS_LABEL_AT_END)
				{
					psFragmentVariant->sStartAddress[1].uiAddr += EURASIA_USE_INSTRUCTION_SIZE;
				}

				ui32NumTemps = ALIGNCOUNTINBLOCKS(ui32NumBlendTemps, EURASIA_USE_OTHER2_PHAS_TCOUNT_ALIGNSHIFT);

				ui32ExeAddr = GetUSEPhaseAddress(psFragmentVariant->sStartAddress[1],
												gc->psSysContext->uUSEFragmentHeapBase, 
												SGX_PIXSHADER_USE_CODE_BASE_INDEX);

#if defined(FIX_HW_BRN_29019)

				if(!psFragmentVariant->ui32USEPrimAttribCount)
				{
					psFragmentVariant->ui32USEPrimAttribCount++;
				}

				pui32USECode[0] = (EURASIA_USE0_S1STDBANK_PRIMATTR << EURASIA_USE0_S1BANK_SHIFT) |
									 (EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT) |
									 (0 << EURASIA_USE0_DST_SHIFT) |
									 (0 << EURASIA_USE0_SRC1_SHIFT);
				
				pui32USECode[1] = (EURASIA_USE1_OP_MOVC << EURASIA_USE1_OP_SHIFT) |
									 (EURASIA_USE1_MOVC_TSTDTYPE_UNCOND << EURASIA_USE1_MOVC_TSTDTYPE_SHIFT) |
									 (EURASIA_USE1_EPRED_ALWAYS << EURASIA_USE1_EPRED_SHIFT) |
									 (0) |	/* DestBExt */
									 (0) | /* Src1BExt */
									 EURASIA_USE1_S2BEXT |
									 (0 << EURASIA_USE1_RMSKCNT_SHIFT) |
									 (0 << EURASIA_USE1_SRC1MOD_SHIFT) |
									 (EURASIA_USE1_D1STDBANK_PRIMATTR << EURASIA_USE1_D1BANK_SHIFT) |
									 EURASIA_USE1_RCNTSEL |
									 EURASIA_USE1_SKIPINV;

				pui32USECode += USE_INST_LENGTH;
				
#endif /* defined(FIX_HW_BRN_29019) */

				/* First PHAS points to second PHAS */
				BuildPHASImmediate((PPVR_USE_INST)	pui32USECode,
													ui32PhasEnd,
													ui32NextPhaseMode,
													EURASIA_USE_OTHER2_PHAS_RATE_SELECTIVE,
													EURASIA_USE_OTHER2_PHAS_WAITCOND_NONE,
													ui32NumTemps,
													ui32ExeAddr);

				/* Second PHAS is last */
				pui32USECode = (IMG_UINT32 *)((IMG_UINT8 *)psFragmentVariant->psCodeBlock->pui32LinAddress + 
														(psFragmentVariant->sStartAddress[1].uiAddr - psFragmentVariant->psCodeBlock->sCodeAddress.uiAddr));
			
				BuildPHASLastPhase ((PPVR_USE_INST) pui32USECode, 0);

				/* Setup blend code insertion point */
				pui32USECode += USE_INST_LENGTH;
			}
			else
			{
				BuildPHASLastPhase ((PPVR_USE_INST) pui32USECode, 0);
			
				/* Setup blend code insertion point */
				pui32USECode = (IMG_UINT32 *)( (IMG_UINT8 *)psFragmentVariant->psCodeBlock->pui32LinAddress + ui32FragmentShaderCodeSizeInBytes);
			}
		}

		/* Now for blend code */
		if(ui32BlendEquation || bColorMaskCode)
		{
			/* Next copy framebuffer blending code as necessary */
			if(ui32FBBlendUSECodeSizeInBytes)
			{
				GLES2MemCopy(pui32USECode, aui32FBBlendUSECode, ui32FBBlendUSECodeSizeInBytes);
	
				pui32USECode += (ui32FBBlendUSECodeSizeInBytes >> 2);
			}

			/* Next copy color mask as necessary */
			if(ui32ColorMaskUSECodeSizeInBytes)
			{
				GLES2MemCopy(pui32USECode, aui32ColorMaskUSECode, ui32ColorMaskUSECodeSizeInBytes);
	
				pui32USECode += (ui32ColorMaskUSECodeSizeInBytes >> 2);
			}
		
			/* Terminate this block of code (sneaky negative array index). Other code paths have already been terminated */
			if(psFragmentVariant->ui32PhaseCount == 3 || ((psRenderState->ui32AlphaTestFlags & GLES2_ALPHA_TEST_DISCARD) == 0))
			{
				pui32USECode[-1] |= EURASIA_USE1_END;
			}
		}

#if defined(SGX_FEATURE_USE_VEC34)
		/* Align temp counts to nearest pair */
		psFragmentVariant->ui32MaxTempRegs = ALIGNCOUNT(psFragmentVariant->ui32MaxTempRegs, 2);
		ui32NumBlendTemps = ALIGNCOUNT(ui32NumBlendTemps, 2);
#endif

#if defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
		/* Need to allocate 4 times as many temps if running at sample rate */
		if(bSeparateBlendPhase)
		{
			ui32NumBlendTemps <<= 2;
		}
#endif

		psFragmentVariant->ui32MaxTempRegs = MAX(psFragmentVariant->ui32MaxTempRegs,  ui32NumBlendTemps);
	
		/* Add to variant list */
		psFragmentVariant->psNext = psFragmentShader->psVariant;
		psFragmentShader->psVariant = psFragmentVariant;

		*pbProgramChanged = IMG_TRUE;

#if defined(DEBUG)
		bNewShader=IMG_TRUE;
#endif
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

		psPatchedShader = psFragmentVariant->psPatchedShader;

	}
	
#if defined(DEBUG)
	if(gc->sAppHints.bDumpShaderAnalysis && bNewShader)
	{
		DumpFragmentShader(gc,
						   psFragmentVariant,
						   ui32IncrementalCodeSizeInUSEInsts);
	}
#endif /* defined(DEBUG) */

	return GLES2_NO_ERROR;
}

#else /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */


/********************************* Shader layout **************************************
 *
 *                               Returned from Unipatch
 *                          |------------------------------|
 *							v	                           v Optional  
 * Option 1 : No Discard:   +--------------+---------------+----------+
 *					     	| Shader (fns) | Shader (main) |NOP/Blend |
 *                          +--------------+---------------+----------+
 *							               ^ 
 *							               Phase 0 Start
 *
 *
 *                                         Returned from Unipatch
 *                          |--------------------------------------------------|
 *							v	                                               v
 * Option 2 : Discard:      +--------------+---+--------------+-----+-----+-----------+
 *			  No Split  	| Shader (fns) |PCF| Shader (main)|ATST8| Pad | MOV/Blend |
 *            PCF=PCOEFF    +--------------+---+--------------+-----+-----+-----------+
 *                                         ^						      ^
 *										   Phase 0 Start			      Phase 1 start
 *
 *
 *                                              Returned from Unipatch
 *                          |--------------------------------------------------------|
 *							v	                                                     v Optional
 * Option 3 : Discard:      +--------------+---+---------------+-----+---------------+---------+
 *			  Split  	    | Shader (fns) |PCF| Shader Part 1 |ATST8| Shader Part 2 |NOP/Blend|
 *            PCF=PCOEFF    +--------------+---+---------------+-----+---------------+---------+
 *							               ^						 ^
 *										   Phase 0 Start			 Phase 1 start
 *
 ****************************************************************************************/

/****************************************************************
 * Function Name  	: SetupUSEFragmentShader
 * Returns        	: Error code
 * Outputs			: pbProgramChanged
 * Globals Used    	: 
 * Description    	: 
 ****************************************************************/
IMG_INTERNAL GLES2_MEMERROR SetupUSEFragmentShader(GLES2Context *gc, IMG_BOOL *pbProgramChanged)
{
	GLES2CompiledRenderState *psRenderState = &gc->sPrim.sRenderState;
	GLES2CompiledTextureState *psFragmentTextureState = &gc->sPrim.sFragmentTextureState;
	GLES2ProgramShader *psFragmentShader = &gc->sProgram.psCurrentProgram->sFragment;
	GLES2USEShaderVariant *psFragmentVariant = psFragmentShader->psVariant;
	USP_HW_SHADER *psPatchedShader;
	IMG_UINT32 ui32PlaneCoeffPAReg = 0;
	IMG_UINT32 aui32FBBlendUSECode[GLES2_FBBLEND_MAX_CODE_SIZE_IN_DWORDS];
	IMG_UINT32 aui32ColorMaskUSECode[GLES2_COLORMASK_MAX_CODE_SIZE_IN_DWORDS];
	IMG_UINT32 ui32FragmentShaderCodeSizeInBytes;
	IMG_BOOL bColorMaskCode = IMG_FALSE;
	IMG_UINT32 ui32FBBlendUSECodeSizeInBytes;
	IMG_UINT32 ui32ColorMaskUSECodeSizeInBytes;
	IMG_UINT32 *pui32USECode;
	IMG_UINT32 ui32NumTemps = 0;
	IMG_UINT32 ui32BlendEquation;
	IMG_UINT32 ui32ImageUnitEnables = psFragmentTextureState->ui32ImageUnitEnables;
	IMG_BOOL bMatch;
	IMG_UINT16 i;
	IMG_BOOL bReadOnlyPAs = IMG_FALSE;
	IMG_UINT32 ui32PreambleCount = 0;
	GLES2_MEMERROR eError;
	IMG_UINT32 ui32IncrementalCodeSizeInUSEInsts;
#if defined(DEBUG)
	IMG_BOOL bNewShader = IMG_FALSE;
#endif

	if(gc->ui32Enables & GLES2_ALPHABLEND_ENABLE)
	{
		ui32BlendEquation = gc->sState.sRaster.ui32BlendEquation;
	}
	else
	{
		ui32BlendEquation =	(GLES2_BLENDFUNC_NONE << GLES2_BLENDFUNC_RGB_SHIFT) | 
							(GLES2_BLENDFUNC_NONE << GLES2_BLENDFUNC_ALPHA_SHIFT);
	}
	
	if(gc->sState.sRaster.ui32ColorMask && (gc->sState.sRaster.ui32ColorMask != GLES2_COLORMASK_ALL))
	{
		bColorMaskCode = IMG_TRUE;
	}

	/* Translucent MSAA cannot write to PAs (as they shared with all subsamples).
	   Although the first phase of TRANSPT can write to PAs (because it runs at pixel rate), the second cannot, so disallow this optimisation 
	 */
	if(gc->psMode->ui32AntiAliasMode && (((psRenderState->ui32ISPControlWordA & ~EURASIA_ISPA_PASSTYPE_CLRMSK) == EURASIA_ISPA_PASSTYPE_TRANS) ||
										((psRenderState->ui32ISPControlWordA & ~EURASIA_ISPA_PASSTYPE_CLRMSK) == EURASIA_ISPA_PASSTYPE_TRANSPT)))
	{
		bReadOnlyPAs = IMG_TRUE;
	}

	while(psFragmentVariant)
	{
		if((psFragmentVariant->u.sFragment.ui32ColorMask == gc->sState.sRaster.ui32ColorMask) && 
			(psFragmentVariant->u.sFragment.ui32BlendEquation == ui32BlendEquation) && 
			(psFragmentVariant->u.sFragment.ui32BlendFactor == gc->sState.sRaster.ui32BlendFactor) && 
			(psFragmentVariant->u.sFragment.bReadOnlyPAs == bReadOnlyPAs) && 
#if defined(FIX_HW_BRN_25077)
			(psFragmentVariant->u.sFragment.ui32AlphaTestFlags == psRenderState->ui32AlphaTestFlags) &&
#endif
			(psFragmentVariant->u.sFragment.ui32ImageUnitEnables == ui32ImageUnitEnables))
		{
			/* If raster state matches, check texture formats */
			bMatch = IMG_TRUE;

			for(i=0; i < GLES2_MAX_TEXTURE_UNITS; i++)
			{
				if(ui32ImageUnitEnables & (1U << i))
				{
					if(psFragmentVariant->u.sFragment.apsTexFormat[i] != psFragmentTextureState->apsTexFormat[i])
					{
						/* If we have failed the search - break to next variant */
						bMatch = IMG_FALSE;
						break;
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
		IMG_UINT32 ui32CodeSizeInBytes;
		IMG_UINT32 *pui32USECodeBase;

		psFragmentVariant = GLES2Calloc(gc, sizeof(GLES2USEShaderVariant));

		if(!psFragmentVariant)
		{
			return GLES2_HOST_MEM_ERROR;
		}

		/* Setup state to for variant */
		psFragmentVariant->u.sFragment.ui32BlendEquation = ui32BlendEquation;
		psFragmentVariant->u.sFragment.ui32BlendFactor = gc->sState.sRaster.ui32BlendFactor;
		psFragmentVariant->u.sFragment.ui32ColorMask = gc->sState.sRaster.ui32ColorMask;
		psFragmentVariant->u.sFragment.ui32ImageUnitEnables = ui32ImageUnitEnables;
		psFragmentVariant->u.sFragment.bReadOnlyPAs = bReadOnlyPAs;

#if defined(FIX_HW_BRN_25077)
		psFragmentVariant->u.sFragment.ui32AlphaTestFlags = psRenderState->ui32AlphaTestFlags;
#endif

		if(ui32ImageUnitEnables)
		{
			for(i=0; i < GLES2_MAX_TEXTURE_UNITS; i++)
			{
				if(ui32ImageUnitEnables & (1U << i))
				{
					psFragmentVariant->u.sFragment.apsTexFormat[i] = psFragmentTextureState->apsTexFormat[i];
					
					PVRUniPatchSetTextureFormat(gc->sProgram.pvUniPatchContext,
												i,
												(USP_TEX_FORMAT *)((IMG_UINTPTR_T)(&psFragmentTextureState->apsTexFormat[i]->sTexFormat)),
												IMG_FALSE,
												IMG_FALSE);
				}
			}
		}

		if(psRenderState->ui32AlphaTestFlags & GLES2_ALPHA_TEST_DISCARD)
		{
#if defined(SGX_FEATURE_ALPHATEST_COEFREORDER)
			/* Add instruction (pcoeff) space at the beginning of uProgStartInstIdx (UniPatchShader) */
			ui32PreambleCount++;
#else
			/* Add 2 instruction (smlsi/nop + pcoeff) space at the beginning of uProgStartInstIdx (UniPatchShader) */
			ui32PreambleCount += 2;
#endif
		}

#if defined(FIX_HW_BRN_25077)
		if(psRenderState->ui32AlphaTestFlags & GLES2_ALPHA_TEST_BRN25077)
		{
#if defined(SGX_FEATURE_ALPHATEST_COEFREORDER)
			/* Add space for pcoeff and atst8, and don't set dest to output as we will have a second phase */
			PVRUniPatchSetOutputLocation(gc->sProgram.pvUniPatchContext, USP_OUTPUT_REGTYPE_DEFAULT);
			
			ui32PreambleCount = 2;
#else
			/* Add space for smlsi/nop, pcoeff and atst8, and don't set dest to output as we will have a second phase */
			PVRUniPatchSetOutputLocation(gc->sProgram.pvUniPatchContext, USP_OUTPUT_REGTYPE_DEFAULT);
			
			ui32PreambleCount = 3;
#endif
		}
		else
#endif
		/* If we are going to add on any code, don't bother patching */
		if(ui32BlendEquation || bColorMaskCode)
		{
			PVRUniPatchSetOutputLocation(gc->sProgram.pvUniPatchContext, USP_OUTPUT_REGTYPE_DEFAULT);
		}
		else
		{
			PVRUniPatchSetOutputLocation(gc->sProgram.pvUniPatchContext, USP_OUTPUT_REGTYPE_OUTPUT);
		}

		PVRUniPatchSetPreambleInstCount(gc->sProgram.pvUniPatchContext, ui32PreambleCount);

		/* Use MSAA version (ie read only PAs) if this object is translucent or translucent pt */
		if(bReadOnlyPAs)
		{
			psPatchedShader = PVRUniPatchFinaliseShader(gc->sProgram.pvUniPatchContext, 
													psFragmentShader->psSharedState->pvUniPatchShaderMSAATrans);
		}
		else
		{
			psPatchedShader = PVRUniPatchFinaliseShader(gc->sProgram.pvUniPatchContext, 
													psFragmentShader->psSharedState->pvUniPatchShader);
		}

		if(!psPatchedShader)
		{
			PVR_DPF((PVR_DBG_FATAL,"SetupUSEFragmentShader: Unipatch failed to finalise the shader"));
			SetError(gc, GL_OUT_OF_MEMORY);
			GLES2Free(IMG_NULL, psFragmentVariant);
			return GLES2_GENERAL_MEM_ERROR;
		}

		/* Create the secondary code block as necessary */
		if(psPatchedShader->uSAUpdateInstCount)
		{
			eError = SetupUSESecondaryUploadTask(gc, psPatchedShader, psFragmentShader->psSharedState, IMG_FALSE);

			if(eError != GLES2_NO_ERROR)
			{
				PVRUniPatchDestroyHWShader(gc->sProgram.pvUniPatchContext, psPatchedShader);
				GLES2Free(IMG_NULL, psFragmentVariant);
				return eError;
			}
		}

		psFragmentVariant->psPatchedShader = psPatchedShader;
		psFragmentVariant->psProgramShader = psFragmentShader;
		psFragmentVariant->ui32USEPrimAttribCount = psPatchedShader->uPARegCount;
		psFragmentVariant->psSecondaryUploadTask = psFragmentShader->psSharedState->psSecondaryUploadTask;

		/* Set current variant */
		gc->sProgram.psCurrentFragmentVariant = psFragmentVariant;

		ui32FragmentShaderCodeSizeInBytes = psPatchedShader->uInstCount * EURASIA_USE_INSTRUCTION_SIZE;

		ui32CodeSizeInBytes = ui32FragmentShaderCodeSizeInBytes;

#if defined(FIX_HW_BRN_25077)
		if(psRenderState->ui32AlphaTestFlags & GLES2_ALPHA_TEST_BRN25077)
		{
			IMG_UINT32 ui32AlignSize = EURASIA_PDS_DOUTU_PHASE_START_ALIGN - 1;
		
			/* Align second phase correctly */
			ui32CodeSizeInBytes = (ui32CodeSizeInBytes + ui32AlignSize) & ~ui32AlignSize;
		
			if(psRenderState->ui32AlphaTestFlags & GLES2_ALPHA_TEST_WAS_TWD)
			{
				/* Adding a nop */
				ui32CodeSizeInBytes += EURASIA_USE_INSTRUCTION_SIZE;
				
				/* Disable other back end code */
				ui32BlendEquation = 0;
				bColorMaskCode = IMG_FALSE;
			}
			else if(!ui32BlendEquation && !bColorMaskCode)
			{
				/* Adding a mov */
				ui32CodeSizeInBytes += EURASIA_USE_INSTRUCTION_SIZE;
			}
		}
#endif /* FIX_HW_BRN_25077 */

		/* 
			Generate pixel shader iterators and texture look ups 
			If a discard/ISP feedback is enabled we need to iterate 
			the triangle plane coefficients 
		*/

		SetupIteratorsAndTextureLookups(gc,
										psPatchedShader->uPSInputCount,
										psPatchedShader->psPSInputLoads,
										psPatchedShader->psTextCtrWords,
										psRenderState->ui32AlphaTestFlags ? IMG_TRUE : IMG_FALSE,
										&ui32PlaneCoeffPAReg);

		psFragmentVariant->ui32PhaseCount = 0;

		ui32FBBlendUSECodeSizeInBytes = 0;
		ui32ColorMaskUSECodeSizeInBytes = 0;

		/* Create the FB blending code if required */
		if(ui32BlendEquation)
		{
		    ui32FBBlendUSECodeSizeInBytes = CreateFBBlendUSECode(gc,
																 psPatchedShader->uPSResultRegNum,
																 bColorMaskCode,
																 psRenderState->ui32AlphaTestFlags ? IMG_TRUE : IMG_FALSE,
																 aui32FBBlendUSECode,
																 &ui32NumTemps);
		}
	
		/* Create the colormask code if required */
		if(bColorMaskCode)
		{
			/* Blending will output to reg 0, otherwise use shader output reg number */
			ui32ColorMaskUSECodeSizeInBytes = CreateColorMaskUSECode(gc, 
																	(ui32BlendEquation || bReadOnlyPAs) ? IMG_TRUE : IMG_FALSE,
																	ui32BlendEquation ? 0 : psPatchedShader->uPSResultRegNum,
																	 aui32ColorMaskUSECode);
		}

		ui32CodeSizeInBytes += (ui32FBBlendUSECodeSizeInBytes + ui32ColorMaskUSECodeSizeInBytes);

		if(((psPatchedShader->uFlags & USP_HWSHADER_FLAGS_LABEL_AT_END) != 0) && !ui32BlendEquation && !bColorMaskCode)
		{
			/* Adding a nop at the end */
			ui32CodeSizeInBytes += EURASIA_USE_INSTRUCTION_SIZE;
		}
		
		GLES2_TIME_START(GLES2_TIMER_USECODEHEAP_FRAG_TIME);

		psFragmentVariant->psCodeBlock = UCH_CodeHeapAllocate(gc->psSharedState->psUSEFragmentCodeHeap, ui32CodeSizeInBytes);
	
		GLES2_TIME_STOP(GLES2_TIMER_USECODEHEAP_FRAG_TIME);

		if(!psFragmentVariant->psCodeBlock)
		{
			/* Destroy unused fragment shaders and retry */
			KRM_ReclaimUnneededResources(gc, &gc->psSharedState->sUSEShaderVariantKRM);
			
			GLES2_TIME_START(GLES2_TIMER_USECODEHEAP_FRAG_TIME);

			psFragmentVariant->psCodeBlock = UCH_CodeHeapAllocate(gc->psSharedState->psUSEFragmentCodeHeap, ui32CodeSizeInBytes);
		
			GLES2_TIME_STOP(GLES2_TIMER_USECODEHEAP_FRAG_TIME);
			
			if(!psFragmentVariant->psCodeBlock)
			{
				gc->sProgram.psCurrentFragmentVariant = IMG_NULL;
				USESecondaryUploadTaskDelRef(gc, psFragmentVariant->psSecondaryUploadTask);
				PVRUniPatchDestroyHWShader(gc->sProgram.pvUniPatchContext, psPatchedShader);
				GLES2Free(IMG_NULL, psFragmentVariant);
				return GLES2_3D_USECODE_ERROR;
			}
		}

		GLES2_INC_COUNT(GLES2_TIMER_USECODEHEAP_FRAG_COUNT, ui32CodeSizeInBytes >> 2);

		psFragmentVariant->sStartAddress[0].uiAddr = psFragmentVariant->psCodeBlock->sCodeAddress.uiAddr + 
												(psPatchedShader->uProgStartInstIdx * EURASIA_USE_INSTRUCTION_SIZE);
		
		pui32USECode = psFragmentVariant->psCodeBlock->pui32LinAddress;
	
		psFragmentVariant->ui32MaxTempRegs = MAX(psPatchedShader->uTempRegCount, ui32NumTemps);

#if defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
		/* Need to allocate 4 times as many temps if running at sample rate */
		if(bReadOnlyPAs)
		{
			psFragmentVariant->ui32MaxTempRegs <<= 2;
		}
#endif
		psFragmentVariant->ui32PhaseCount++;
		
		/* First copy fragment shader code */
		GLES2MemCopy(pui32USECode, 
					 psPatchedShader->puInsts, 
					 ui32FragmentShaderCodeSizeInBytes);

		
		pui32USECodeBase = pui32USECode;
		ui32IncrementalCodeSizeInUSEInsts = (ui32FragmentShaderCodeSizeInBytes >> 3);

		/* Next copy pcoeff instruction as necessary */
		if(psRenderState->ui32AlphaTestFlags & GLES2_ALPHA_TEST_DISCARD)
		{
#if !defined(SGX_FEATURE_ALPHATEST_COEFREORDER)
			IMG_UINT32 ui32CoeffA = ui32PlaneCoeffPAReg;
#endif

			psFragmentVariant->sStartAddress[1].uiAddr = psFragmentVariant->psCodeBlock->sCodeAddress.uiAddr + 
												(psPatchedShader->uPTPhase1StartInstIdx * EURASIA_USE_INSTRUCTION_SIZE);
			psFragmentVariant->ui32PhaseCount++;

			/* Preamble code has been reserved at start of main code */
			pui32USECode = (IMG_UINT32 *)((IMG_UINTPTR_T)psFragmentVariant->psCodeBlock->pui32LinAddress + 
										(psPatchedShader->uProgStartInstIdx * EURASIA_USE_INSTRUCTION_SIZE));


#if defined(SGX_FEATURE_ALPHATEST_COEFREORDER)
			/* PCOEFF */
			pui32USECode[0] = GLES2_USE_SRC1(PRIMATTR, ui32PlaneCoeffPAReg+1)					|
							GLES2_USE_SRC2(SECATTR, GLES2_FRAGMENT_SECATTR_ISPFEEDBACK1)	|
							GLES2_USE0_SRC0_NUM(ui32PlaneCoeffPAReg);
		
			pui32USECode[1] = GLES2_USE1_OP(SPECIAL)																|
							(EURASIA_USE1_VISTEST_OP2_PTCTRL << EURASIA_USE1_OTHER_OP2_SHIFT)					|
							(EURASIA_USE1_SPECIAL_OPCAT_VISTEST << EURASIA_USE1_SPECIAL_OPCAT_SHIFT)			|
							(EURASIA_USE1_VISTEST_PTCTRL_TYPE_PLANE << EURASIA_USE1_VISTEST_PTCTRL_TYPE_SHIFT)	|
							(EURASIA_USE1_S0STDBANK_PRIMATTR << EURASIA_USE1_S0BANK_SHIFT);

#else
			if(ui32CoeffA == 0)
			{
				BuildSMLSI((PPVR_USE_INST) pui32USECode,
							0, 0, 0, 0, /*< Limits */
							1, 0, 1, 1); /*< Increments */
			}
			else
			{
				BuildNOP((PPVR_USE_INST) pui32USECode, 0, IMG_FALSE);
				ui32CoeffA -= 1;
			}

			/* PCOEFF */
			pui32USECode[2] = GLES2_USE_SRC1(PRIMATTR, ui32PlaneCoeffPAReg+1)					|
							GLES2_USE_SRC2(SECATTR, GLES2_FRAGMENT_SECATTR_ISPFEEDBACK1)	|
							GLES2_USE0_SRC0_NUM(ui32CoeffA);

			pui32USECode[3] = GLES2_USE1_OP(SPECIAL)																|
							(EURASIA_USE1_VISTEST_OP2_PTCTRL << EURASIA_USE1_OTHER_OP2_SHIFT)					|
							(EURASIA_USE1_SPECIAL_OPCAT_VISTEST << EURASIA_USE1_SPECIAL_OPCAT_SHIFT)			|
							(EURASIA_USE1_VISTEST_PTCTRL_TYPE_PLANE << EURASIA_USE1_VISTEST_PTCTRL_TYPE_SHIFT)	|
							(EURASIA_USE1_S0STDBANK_PRIMATTR << EURASIA_USE1_S0BANK_SHIFT);
#endif /* SGX_FEATURE_ALPHATEST_COEFREORDER */

			/* Next fill in ISP feedback code */
			pui32USECode = (IMG_UINT32 *)((IMG_UINTPTR_T)psFragmentVariant->psCodeBlock->pui32LinAddress + 
											(psPatchedShader->uPTPhase0EndInstIdx * EURASIA_USE_INSTRUCTION_SIZE));
			
			/* ATST8 - writes the colour result to uPSResultRegNum. Temporaries are not preserved between phases */
			pui32USECode[0] = GLES2_USE_SRC1(SECATTR, GLES2_FRAGMENT_SECATTR_ISPFEEDBACK0)	|
									GLES2_USE_SRC2(SECATTR, GLES2_FRAGMENT_SECATTR_ISPFEEDBACK1)	|
									GLES2_USE0_SRC0_NUM(psPatchedShader->uPSResultRegNum)											|
									GLES2_USE0_DST_NUM(psPatchedShader->uPSResultRegNum);

			/* Texkill result is in predicate P1. Mark end of first phase */
			pui32USECode[1] = GLES2_USE1_OP(SPECIAL)												|
							((IMG_UINT32)EURASIA_USE1_VISTEST_OP2_ATST8 << EURASIA_USE1_OTHER_OP2_SHIFT)			|
							((IMG_UINT32)EURASIA_USE1_SPECIAL_OPCAT_VISTEST << EURASIA_USE1_SPECIAL_OPCAT_SHIFT)	|
							(bReadOnlyPAs ? ((IMG_UINT32)EURASIA_USE1_S0STDBANK_TEMP << EURASIA_USE1_S0BANK_SHIFT) : 
										((IMG_UINT32)EURASIA_USE1_S0STDBANK_PRIMATTR << EURASIA_USE1_S0BANK_SHIFT))	|
							GLES2_USE1_DST_TYPE(PRIMATTR)												|
							(IMG_UINT32)EURASIA_USE1_SPRED_P1 << EURASIA_USE1_VISTEST_ATST8_SPRED_SHIFT				|
							EURASIA_USE1_END;

/* PRQA S 3332 1 */ /* Override QAC suggestion and use this macro. */
		}

		/* Jump to end of code */
		pui32USECode = (IMG_UINT32 *)((IMG_UINTPTR_T)psFragmentVariant->psCodeBlock->pui32LinAddress + ui32FragmentShaderCodeSizeInBytes);

#if defined(FIX_HW_BRN_25077)
		if(psRenderState->ui32AlphaTestFlags & GLES2_ALPHA_TEST_BRN25077)
		{
			IMG_UINT32 ui32Phase2Offset;
			IMG_UINT32 ui32AlignSize = EURASIA_PDS_DOUTU_PHASE_START_ALIGN - 1;
			IMG_UINT32 ui32CoeffA = ui32PlaneCoeffPAReg;

			/* Preamble code has been reserved at start of main code */
			pui32USECode = (IMG_UINT32 *)((IMG_UINTPTR_T)psFragmentVariant->psCodeBlock->pui32LinAddress + 
										(psPatchedShader->uProgStartInstIdx * EURASIA_USE_INSTRUCTION_SIZE));


			if(ui32CoeffA == 0)
			{
				BuildSMLSI((PPVR_USE_INST) pui32USECode,
							0, 0, 0, 0, /*< Limits */
							1, 0, 1, 1); /*< Increments */
			}
			else
			{
				BuildNOP((PPVR_USE_INST) pui32USECode, 0, IMG_FALSE);
				ui32CoeffA -= 1;
			}


			/* PCOEFF */
			pui32USECode[2] = GLES2_USE_SRC1(PRIMATTR, ui32PlaneCoeffPAReg+1)					|
							GLES2_USE_SRC2(SECATTR, GLES2_FRAGMENT_SECATTR_ISPFEEDBACK1)	|
							GLES2_USE0_SRC0_NUM(ui32CoeffA);

			pui32USECode[3] = GLES2_USE1_OP(SPECIAL)																|
							(EURASIA_USE1_VISTEST_OP2_PTCTRL << EURASIA_USE1_OTHER_OP2_SHIFT)					|
							(EURASIA_USE1_SPECIAL_OPCAT_VISTEST << EURASIA_USE1_SPECIAL_OPCAT_SHIFT)			|
							(EURASIA_USE1_VISTEST_PTCTRL_TYPE_PLANE << EURASIA_USE1_VISTEST_PTCTRL_TYPE_SHIFT)	|
							(EURASIA_USE1_S0STDBANK_PRIMATTR << EURASIA_USE1_S0BANK_SHIFT);
		
			/* ATST8 - writes the colour result to uPSResultRegNum. Temporaries are not preserved between phases */
			pui32USECode[4] = GLES2_USE_SRC1(SECATTR, GLES2_FRAGMENT_SECATTR_ISPFEEDBACK0)	|
									GLES2_USE_SRC2(SECATTR, GLES2_FRAGMENT_SECATTR_ISPFEEDBACK1)	|
									GLES2_USE0_SRC0_NUM(psPatchedShader->uPSResultRegNum)											|
									GLES2_USE0_DST_NUM(psPatchedShader->uPSResultRegNum);

			pui32USECode[5] = GLES2_USE1_OP(SPECIAL)												|
							(EURASIA_USE1_VISTEST_OP2_ATST8 << EURASIA_USE1_OTHER_OP2_SHIFT)			|
							(EURASIA_USE1_SPECIAL_OPCAT_VISTEST << EURASIA_USE1_SPECIAL_OPCAT_SHIFT)	|
							(EURASIA_USE1_S0STDBANK_PRIMATTR << EURASIA_USE1_S0BANK_SHIFT)				|
							GLES2_USE1_DST_TYPE(PRIMATTR)												|
							(EURASIA_USE1_SPRED_ALWAYS << EURASIA_USE1_VISTEST_ATST8_SPRED_SHIFT);
	
			pui32USECode = (IMG_UINT32 *)((IMG_UINTPTR_T)psFragmentVariant->psCodeBlock->pui32LinAddress + ui32FragmentShaderCodeSizeInBytes);

			/* Terminate 1st phase */
			pui32USECode[-1] |= EURASIA_USE1_END;

			/* Align second phase correctly */
			ui32Phase2Offset = (ui32FragmentShaderCodeSizeInBytes + ui32AlignSize) & ~ui32AlignSize;

			pui32USECode = (IMG_UINT32 *)((IMG_UINTPTR_T)psFragmentVariant->psCodeBlock->pui32LinAddress + ui32Phase2Offset);

			psFragmentVariant->sStartAddress[1].uiAddr = psFragmentVariant->psCodeBlock->sCodeAddress.uiAddr + ui32Phase2Offset;
			psFragmentVariant->ui32PhaseCount++;

		}
#endif
	
		if(ui32BlendEquation || bColorMaskCode)
		{
			IMG_BOOL bComplexShader;
			/* combiner tries to collapse insts only if there is single shader output */
			bComplexShader = psPatchedShader->uFlags & USP_HWSHADER_FLAGS_SIMPLE_PS ? IMG_FALSE : IMG_TRUE;

			/* Next copy framebuffer blending code as necessary */
			if(ui32FBBlendUSECodeSizeInBytes)
			{
#if defined(FIX_HW_BRN_25077)
				if(psRenderState->ui32AlphaTestFlags & GLES2_ALPHA_TEST_BRN25077)
				{
					GLES2MemCopy(pui32USECode, aui32FBBlendUSECode, ui32FBBlendUSECodeSizeInBytes);
					
					pui32USECode += (ui32FBBlendUSECodeSizeInBytes >> 2);
				}
				else
#endif
				{
					IMG_UINT32 ui32OldSizeInUSEInsts = ui32IncrementalCodeSizeInUSEInsts;

					ui32IncrementalCodeSizeInUSEInsts = PVRCombineBlendCodeWithShader(pui32USECodeBase,
																					ui32IncrementalCodeSizeInUSEInsts,
																					pui32USECodeBase,
																					(ui32FBBlendUSECodeSizeInBytes >> 3),
																					aui32FBBlendUSECode,									     
																					bComplexShader,
																					IMG_FALSE);
					
					pui32USECode += (ui32IncrementalCodeSizeInUSEInsts - ui32OldSizeInUSEInsts) << 1;
				}
			}

			/* Next copy color mask as necessary */
			if(ui32ColorMaskUSECodeSizeInBytes)
			{
#if defined(FIX_HW_BRN_25077)
				if(psRenderState->ui32AlphaTestFlags & GLES2_ALPHA_TEST_BRN25077)
				{
					GLES2MemCopy(pui32USECode, aui32ColorMaskUSECode, ui32ColorMaskUSECodeSizeInBytes);
					
					pui32USECode += (ui32ColorMaskUSECodeSizeInBytes >> 2);
				}
				else
#endif
				{
					IMG_UINT32 ui32OldSizeInUSEInsts = ui32IncrementalCodeSizeInUSEInsts;

					ui32IncrementalCodeSizeInUSEInsts = PVRCombineBlendCodeWithShader(pui32USECodeBase,
																					ui32IncrementalCodeSizeInUSEInsts,
																					pui32USECodeBase,
																					(ui32ColorMaskUSECodeSizeInBytes >> 3),
																					aui32ColorMaskUSECode,									     
																					bComplexShader,
																					IMG_FALSE);
		
					pui32USECode += (ui32IncrementalCodeSizeInUSEInsts - ui32OldSizeInUSEInsts) << 1;
				}
			}
		}
#if defined(FIX_HW_BRN_25077)
		else if(psRenderState->ui32AlphaTestFlags & GLES2_ALPHA_TEST_WAS_TWD)
		{
			BuildNOP((PPVR_USE_INST)pui32USECode, 0, IMG_FALSE);
			pui32USECode += USE_INST_LENGTH;

		}
		else if(psRenderState->ui32AlphaTestFlags & GLES2_ALPHA_TEST_BRN25077)
		{
			/* mov o0, paX (x == PSResultRegNum) */
			BuildMOV ((PPVR_USE_INST) pui32USECode,
						EURASIA_USE1_EPRED_ALWAYS,
						0,
						EURASIA_USE1_RCNTSEL,
						USE_REGTYPE_OUTPUT, 0,
						USE_REGTYPE_PRIMATTR, psPatchedShader->uPSResultRegNum,
						0 /* Src mod */);

			pui32USECode += USE_INST_LENGTH;
		}
#endif
		else if(psPatchedShader->uFlags & USP_HWSHADER_FLAGS_LABEL_AT_END)
		{
			BuildNOP((PPVR_USE_INST)pui32USECode, 0, IMG_FALSE);
			pui32USECode += USE_INST_LENGTH;
		}

		/* Terminate this block of code (sneaky negative array index) */
		pui32USECode[-1] |= EURASIA_USE1_END;
		
		/* Add to variant list */
		psFragmentVariant->psNext = psFragmentShader->psVariant;
		psFragmentShader->psVariant = psFragmentVariant;

		*pbProgramChanged = IMG_TRUE;

#if defined(DEBUG)
		bNewShader=IMG_TRUE;
#endif
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

		psPatchedShader = psFragmentVariant->psPatchedShader;

	}
	
	/* Setup use mode of operation */
	if(psPatchedShader->uFlags & UNIFLEX_HW_FLAGS_PER_INSTANCE_MODE)
	{
		psFragmentShader->ui32USEMode = EURASIA_PDS_DOUTU1_MODE_PERINSTANCE;
	}
	else
	{
		psFragmentShader->ui32USEMode = EURASIA_PDS_DOUTU1_MODE_PARALLEL;
	}

#if defined(DEBUG)
	if(gc->sAppHints.bDumpShaderAnalysis && bNewShader)
	{
		DumpFragmentShader(gc,
						   psFragmentVariant,
						   ui32IncrementalCodeSizeInUSEInsts);
	}
#endif /* defined(DEBUG) */
	return GLES2_NO_ERROR;
}

#endif /* SGX_FEATURE_USE_UNLIMITED_PHASES */

#if defined(DEBUG)
/****************************************************************
 * Function Name  	: DumpVertexShader
 * Returns        	: 
 * Outputs			: 
 * Globals Used    	: 
 * Description    	: 
 ****************************************************************/
IMG_INTERNAL IMG_VOID DumpVertexShader(GLES2Context *gc, 
									   GLES2USEShaderVariant *psVertexVariant,
	   								   IMG_UINT32 ui32CodeSizeInUSEInsts)
{
	IMG_UINT32 uInstCount;
	IMG_PUINT32 puInstructions;
	IMG_UINT16 i, j, uInstCountDigits;
	IMG_UINT32 ui32DeviceVirtualAddress;

	//uInstCount = psVertexVariant->psCodeBlock->ui32Size >> 3;
	uInstCount = ui32CodeSizeInUSEInsts;
	puInstructions = psVertexVariant->psCodeBlock->pui32LinAddress;

	for (i = 10, uInstCountDigits = 1; i < uInstCount; i *= 10, uInstCountDigits++);

	ui32DeviceVirtualAddress = psVertexVariant->psCodeBlock->sCodeAddress.uiAddr;
	fprintf(gc->pShaderAnalysisHandle, "-------------------------------------\n\n");
	fprintf(gc->pShaderAnalysisHandle, "Device Virtual Address for code : 0x%X\n", ui32DeviceVirtualAddress);
	fprintf(gc->pShaderAnalysisHandle, "Starting Addresses:\n");

	for (i=0; i < psVertexVariant->ui32PhaseCount; i++)
	{
		fprintf(gc->pShaderAnalysisHandle, "Phase %d: 0x%X\n", i, psVertexVariant->sStartAddress[i].uiAddr);
	}

#if defined(GLES2_EXTENSION_GET_PROGRAM_BINARY)
	if (gc->sProgram.psCurrentProgram->bLoadFromBinary)
	{
		fprintf(gc->pShaderAnalysisHandle, "Vertex Shader (N/A) Disassembly.  (No shader name as program is loaded from binary.)\n");
	}
	else
#endif
	{
		fprintf(gc->pShaderAnalysisHandle, "Vertex Shader (%u) Disassembly:\n", gc->sProgram.psCurrentProgram->psVertexShader->sNamedItem.ui32Name);
	}

	fprintf(gc->pShaderAnalysisHandle, "-------------------------------------\n\n");

	for (i = 0; i < uInstCount; i++, puInstructions+=2, ui32DeviceVirtualAddress+=8)
	{
		IMG_UINT32 uInst0 = puInstructions[0];
		IMG_UINT32 uInst1 = puInstructions[1];
		IMG_CHAR pszInst[256];

		/* Phase markers */
		for (j=0; j < psVertexVariant->ui32PhaseCount; j++)
		{
			if (psVertexVariant->sStartAddress[j].uiAddr==ui32DeviceVirtualAddress)
			{
				fprintf(gc->pShaderAnalysisHandle, "---------- Phase %u Start ----------\n", j);
			}
		}

		/* Line number */
		fprintf(gc->pShaderAnalysisHandle, "%*d: ", uInstCountDigits, i);

		/* Device Virtual Address */
		fprintf(gc->pShaderAnalysisHandle, "0x%.8X ", ui32DeviceVirtualAddress);

		/* Instruction */
		fprintf(gc->pShaderAnalysisHandle, "0x%.8X%.8X  ", uInst1, uInst0);
		UseDisassembleInstruction(UseAsmGetCoreDesc(&psVertexVariant->psPatchedShader->sTargetDev), uInst0, uInst1, pszInst);
		fprintf(gc->pShaderAnalysisHandle, "%s\n", pszInst);

	}

	fprintf(gc->pShaderAnalysisHandle, "-------------------------------------\n\n");

	if (psVertexVariant->psPatchedShader->uSAUpdateInstCount)
	{
#if defined(GLES2_EXTENSION_GET_PROGRAM_BINARY)
		if (gc->sProgram.psCurrentProgram->bLoadFromBinary)
		{
			fprintf(gc->pShaderAnalysisHandle, "Secondary Vertex Shader (N/A) Disassembly.  (No shader name as program is loaded from binary.)\n");
		}
		else
#endif
		{
			fprintf(gc->pShaderAnalysisHandle, "Secondary Vertex Shader (%u) Disassembly:\n", gc->sProgram.psCurrentProgram->psVertexShader->sNamedItem.ui32Name);
		}

		fprintf(gc->pShaderAnalysisHandle, "-----------------------------------------------\n");

		uInstCount = psVertexVariant->psPatchedShader->uSAUpdateInstCount + 1;
		puInstructions = psVertexVariant->psSecondaryUploadTask->psSecondaryCodeBlock->pui32LinAddress;
		ui32DeviceVirtualAddress = psVertexVariant->psSecondaryUploadTask->psSecondaryCodeBlock->sCodeAddress.uiAddr;
		/* Start print disassembly */
		for (i = 0; i < uInstCount; i++, puInstructions+=2, ui32DeviceVirtualAddress+=8)
		{
			IMG_UINT32 uInst0 = puInstructions[0];
			IMG_UINT32 uInst1 = puInstructions[1];
			IMG_CHAR pszInst[256];

			/* Line number */
			fprintf(gc->pShaderAnalysisHandle, "%3d: ", i);

			/* Device Virtual Address */
			fprintf(gc->pShaderAnalysisHandle, "0x%.8X ", ui32DeviceVirtualAddress);

			/* Instruction */
			fprintf(gc->pShaderAnalysisHandle, "0x%.8X%.8X  ", uInst1, uInst0);
			UseDisassembleInstruction(UseAsmGetCoreDesc(&psVertexVariant->psPatchedShader->sTargetDev), uInst0, uInst1, pszInst);
			fprintf(gc->pShaderAnalysisHandle, "%s\n", pszInst);

		}
		fprintf(gc->pShaderAnalysisHandle, "-----------------------------------------------\n");

	}
}
/****************************************************************
* Function Name  	: DumpFragmentShader
* Returns        	: 
* Outputs			: 
* Globals Used    	: 
* Description    	: 
****************************************************************/
IMG_INTERNAL IMG_VOID DumpFragmentShader(GLES2Context *gc, 
									     GLES2USEShaderVariant *psFragmentVariant, 
	   								     IMG_UINT32 ui32CodeSizeInUSEInsts)
{
	GLES2CompiledRenderState *psRenderState = &gc->sPrim.sRenderState;
	IMG_UINT32 uInstCount;
	IMG_PUINT32 puInstructions;
	IMG_UINT16 i, j, uInstCountDigits;
	IMG_UINT32 ui32DeviceVirtualAddress;

#if defined(GLES2_EXTENSION_GET_PROGRAM_BINARY)
	if (gc->sProgram.psCurrentProgram->bLoadFromBinary)
	{
		fprintf(gc->pShaderAnalysisHandle, "Fragment Shader (N/A) Disassembly.  (No shader name as program is loaded from binary.)\n");
	}
	else
#endif
	{
		fprintf(gc->pShaderAnalysisHandle, "Fragment Shader (%u) Disassembly:\n", gc->sProgram.psCurrentProgram->psFragmentShader->sNamedItem.ui32Name);
	}
	fprintf(gc->pShaderAnalysisHandle, "-------------------------------------\n");

	//uInstCount = psFragmentVariant->psCodeBlock->ui32Size >> 3; /* This count is sometomes 1 too big - dont use */
	uInstCount = ui32CodeSizeInUSEInsts;
	puInstructions = psFragmentVariant->psCodeBlock->pui32LinAddress;
	for (i = 10, uInstCountDigits = 1; i < uInstCount; i *= 10, uInstCountDigits++);

	if(gc->ui32Enables & GLES2_ALPHABLEND_ENABLE)
	{
		fprintf(gc->pShaderAnalysisHandle, "Aplha Blend Enabled:\n");
		for (i=0;i<2;i++)
		{
			IMG_UINT32 equation;
			switch (i)
			{
				case 0:
					fprintf(gc->pShaderAnalysisHandle, "      RGB Function:  ");
					equation = (gc->sState.sRaster.ui32BlendEquation & GLES2_BLENDFUNC_RGB_MASK) >> GLES2_BLENDFUNC_RGB_SHIFT;
					break;
				case 1:
					fprintf(gc->pShaderAnalysisHandle, "    Alpha Function:  ");
					equation = (gc->sState.sRaster.ui32BlendEquation & GLES2_BLENDFUNC_ALPHA_MASK) >> GLES2_BLENDFUNC_ALPHA_SHIFT;
					break;
			}
			switch (equation)
			{
				case GLES2_BLENDFUNC_NONE:
					fprintf(gc->pShaderAnalysisHandle, "GLES2_BLENDFUNC_NONE\n");
					break;
				case GLES2_BLENDFUNC_ADD:
					fprintf(gc->pShaderAnalysisHandle, "GLES2_BLENDFUNC_ADD\n");
					break;
				case GLES2_BLENDFUNC_SUBTRACT:
					fprintf(gc->pShaderAnalysisHandle, "GLES2_BLENDFUNC_SUBTRACT\n");
					break;
				case GLES2_BLENDFUNC_REVSUBTRACT:
					fprintf(gc->pShaderAnalysisHandle, "GLES2_BLENDFUNC_REVSUBTRACT\n");
					break;
				default:
					fprintf(gc->pShaderAnalysisHandle, "<unknown blend function>\n");
			}
		}
		for (i=0;i<4;i++)
		{
			IMG_UINT32 factormask;
			IMG_UINT32 factorshift;
			switch (i)
			{
				case 0:
					factormask = GLES2_BLENDFACTOR_RGBSRC_MASK;
					factorshift = GLES2_BLENDFACTOR_RGBSRC_SHIFT;
					fprintf(gc->pShaderAnalysisHandle, "    RGB Src Factor:  ");
					break;
				case 1:
					factormask = GLES2_BLENDFACTOR_RGBDST_MASK;
					factorshift = GLES2_BLENDFACTOR_RGBDST_SHIFT;
					fprintf(gc->pShaderAnalysisHandle, "    RGB Dst Factor:  ");
					break;
				case 2:
					factormask = GLES2_BLENDFACTOR_ALPHASRC_MASK;
					factorshift = GLES2_BLENDFACTOR_ALPHASRC_SHIFT;
					fprintf(gc->pShaderAnalysisHandle, "  Alpha Src Factor:  ");
					break;
				case 3:
					factormask = GLES2_BLENDFACTOR_ALPHADST_MASK;
					factorshift = GLES2_BLENDFACTOR_ALPHADST_SHIFT;
					fprintf(gc->pShaderAnalysisHandle, "  Alpha Dst Factor:  ");
					break;
			}

			switch ( (gc->sState.sRaster.ui32BlendFactor & factormask ) >> factorshift )
			{
				case GLES2_BLENDFACTOR_ZERO:
						fprintf(gc->pShaderAnalysisHandle, "GLES2_BLENDFACTOR_ZERO\n");
						break;
				case GLES2_BLENDFACTOR_ONE:
						fprintf(gc->pShaderAnalysisHandle, "GLES2_BLENDFACTOR_ONE\n");
						break;
				case GLES2_BLENDFACTOR_SRCCOLOR:
						fprintf(gc->pShaderAnalysisHandle, "GLES2_BLENDFACTOR_SRCCOLOR\n");
						break;
				case GLES2_BLENDFACTOR_ONEMINUS_SRCCOLOR:
						fprintf(gc->pShaderAnalysisHandle, "GLES2_BLENDFACTOR_ONEMINUS_SRCCOLOR\n");
						break;
				case GLES2_BLENDFACTOR_SRCALPHA:
						fprintf(gc->pShaderAnalysisHandle, "GLES2_BLENDFACTOR_SRCALPHA\n");
						break;
				case GLES2_BLENDFACTOR_ONEMINUS_SRCALPHA:
						fprintf(gc->pShaderAnalysisHandle, "GLES2_BLENDFACTOR_ONEMINUS_SRCALPHA\n");
						break;
				case GLES2_BLENDFACTOR_DSTALPHA:
						fprintf(gc->pShaderAnalysisHandle, "GLES2_BLENDFACTOR_DSTALPHA\n");
						break;
				case GLES2_BLENDFACTOR_ONEMINUS_DSTALPHA:
						fprintf(gc->pShaderAnalysisHandle, "GLES2_BLENDFACTOR_ONEMINUS_DSTALPHA\n");
						break;
				case GLES2_BLENDFACTOR_DSTCOLOR:
						fprintf(gc->pShaderAnalysisHandle, "GLES2_BLENDFACTOR_DSTCOLOR\n");
						break;
				case GLES2_BLENDFACTOR_ONEMINUS_DSTCOLOR:
						fprintf(gc->pShaderAnalysisHandle, "GLES2_BLENDFACTOR_ONEMINUS_DSTCOLOR\n");
						break;
				case GLES2_BLENDFACTOR_SRCALPHA_SATURATE:
						fprintf(gc->pShaderAnalysisHandle, "GLES2_BLENDFACTOR_SRCALPHA_SATURATE\n");
						break;
				case GLES2_BLENDFACTOR_CONSTCOLOR:
						fprintf(gc->pShaderAnalysisHandle, "GLES2_BLENDFACTOR_CONSTCOLOR\n");
						break;
				case GLES2_BLENDFACTOR_ONEMINUS_CONSTCOLOR:
						fprintf(gc->pShaderAnalysisHandle, "GLES2_BLENDFACTOR_ONEMINUS_CONSTCOLOR\n");
						break;
				case GLES2_BLENDFACTOR_CONSTALPHA:
						fprintf(gc->pShaderAnalysisHandle, "GLES2_BLENDFACTOR_CONSTALPHA\n");
						break;
				case GLES2_BLENDFACTOR_ONEMINUS_CONSTALPHA:
						fprintf(gc->pShaderAnalysisHandle, "GLES2_BLENDFACTOR_ONEMINUS_CONSTALPHA\n");
						break;
				case GLES2_BLENDFACTOR_DSTALPHA_SATURATE:
						fprintf(gc->pShaderAnalysisHandle, "GLES2_BLENDFACTOR_DSTALPHA_SATURATE\n");
						break;
				default:
					fprintf(gc->pShaderAnalysisHandle, "<unknown blend factor>\n");
			}
		}
	}

	if (gc->sState.sRaster.ui32ColorMask)
	{
		fprintf(gc->pShaderAnalysisHandle, "Color Mask : ");
		switch (gc->sState.sRaster.ui32ColorMask)
		{
			case GLES2_COLORMASK_ALPHA:
				fprintf(gc->pShaderAnalysisHandle, "GLES2_COLORMASK_ALPHA\n");
				break;
			case GLES2_COLORMASK_BLUE:
				fprintf(gc->pShaderAnalysisHandle, "GLES2_COLORMASK_BLUE\n");
				break;
			case GLES2_COLORMASK_GREEN:
				fprintf(gc->pShaderAnalysisHandle, "GLES2_COLORMASK_GREEN\n");
				break;
			case GLES2_COLORMASK_RED:
				fprintf(gc->pShaderAnalysisHandle, "GLES2_COLORMASK_RED\n");
				break;
			case GLES2_COLORMASK_ALL:
				fprintf(gc->pShaderAnalysisHandle, "GLES2_COLORMASK_ALL\n");
				break;
			default:
				fprintf(gc->pShaderAnalysisHandle, "<unknown color mask>\n");
		}
	}

	fprintf(gc->pShaderAnalysisHandle, "Pass Type:  ");
	switch (psRenderState->ui32ISPControlWordA & ~EURASIA_ISPA_PASSTYPE_CLRMSK)
	{
		case EURASIA_ISPA_PASSTYPE_OPAQUE:
			fprintf(gc->pShaderAnalysisHandle, "EURASIA_ISPA_PASSTYPE_OPAQUE\n");
			break;
		case EURASIA_ISPA_PASSTYPE_TRANS:
			fprintf(gc->pShaderAnalysisHandle, "EURASIA_ISPA_PASSTYPE_TRANS\n");
			break;
		case EURASIA_ISPA_PASSTYPE_TRANSPT:
			fprintf(gc->pShaderAnalysisHandle, "EURASIA_ISPA_PASSTYPE_TRANSPT\n");
			break;
		case EURASIA_ISPA_PASSTYPE_VIEWPORT:
			fprintf(gc->pShaderAnalysisHandle, "EURASIA_ISPA_PASSTYPE_VIEWPORT\n");
			break;
		case EURASIA_ISPA_PASSTYPE_FASTPT:
			fprintf(gc->pShaderAnalysisHandle, "EURASIA_ISPA_PASSTYPE_FASTPT\n");
			break;
		case EURASIA_ISPA_PASSTYPE_DEPTHFEEDBACK:
			fprintf(gc->pShaderAnalysisHandle, "EURASIA_ISPA_PASSTYPE_DEPTHFEEDBACK\n");
			break;
		default:
			fprintf(gc->pShaderAnalysisHandle, "<unknown pass type>\n");
	}

	ui32DeviceVirtualAddress = psFragmentVariant->psCodeBlock->sCodeAddress.uiAddr;
	fprintf(gc->pShaderAnalysisHandle, "\n");
	fprintf(gc->pShaderAnalysisHandle, "Device Virtual Address for code : 0x%X\n", ui32DeviceVirtualAddress);
	fprintf(gc->pShaderAnalysisHandle, "Starting Addresses:\n");
	for (i=0; i < psFragmentVariant->ui32PhaseCount; i++)
	{
		fprintf(gc->pShaderAnalysisHandle, "Phase %d: 0x%X\n", i, psFragmentVariant->sStartAddress[i].uiAddr);
	}
	fprintf(gc->pShaderAnalysisHandle, "-------------------------------------\n");

	/* Start print disassembly */
	for (i = 0; i < uInstCount; i++, puInstructions+=2, ui32DeviceVirtualAddress+=8)
	{
		IMG_UINT32 uInst0 = puInstructions[0];
		IMG_UINT32 uInst1 = puInstructions[1];
		IMG_CHAR pszInst[256];

		/* Phase markers */
		for (j=0; j < psFragmentVariant->ui32PhaseCount; j++)
		{
			if (psFragmentVariant->sStartAddress[j].uiAddr==ui32DeviceVirtualAddress)
			{
				fprintf(gc->pShaderAnalysisHandle, "---------- Phase %u Start ----------\n", j);
			}
		}

		/* Line number */
		fprintf(gc->pShaderAnalysisHandle, "%*d: ", uInstCountDigits, i);

		/* Device Virtual Address */
		fprintf(gc->pShaderAnalysisHandle, "0x%.8X ", ui32DeviceVirtualAddress);

		/* Instruction */
		fprintf(gc->pShaderAnalysisHandle, "0x%.8X%.8X  ", uInst1, uInst0);
		UseDisassembleInstruction(UseAsmGetCoreDesc(&psFragmentVariant->psPatchedShader->sTargetDev), uInst0, uInst1, pszInst);
		fprintf(gc->pShaderAnalysisHandle, "%s\n", pszInst);

	}
	fprintf(gc->pShaderAnalysisHandle, "-------------------------------------\n\n");


	if (psFragmentVariant->psPatchedShader->uSAUpdateInstCount)
	{
#if defined(GLES2_EXTENSION_GET_PROGRAM_BINARY)
		if (gc->sProgram.psCurrentProgram->bLoadFromBinary)
		{
			fprintf(gc->pShaderAnalysisHandle, "Secondary Fragment Shader (N/A) Disassembly.  (No shader name as program is loaded from binary.)\n");
		}
		else
#endif
		{
			fprintf(gc->pShaderAnalysisHandle, "Secondary Fragment Shader (%u) Disassembly:\n", gc->sProgram.psCurrentProgram->psFragmentShader->sNamedItem.ui32Name);
		}

		fprintf(gc->pShaderAnalysisHandle, "-----------------------------------------------\n");

		uInstCount = psFragmentVariant->psPatchedShader->uSAUpdateInstCount;
		puInstructions = psFragmentVariant->psSecondaryUploadTask->psSecondaryCodeBlock->pui32LinAddress;
		ui32DeviceVirtualAddress = psFragmentVariant->psSecondaryUploadTask->psSecondaryCodeBlock->sCodeAddress.uiAddr;
		/* Start print disassembly */
		for (i = 0; i < uInstCount; i++, puInstructions+=2, ui32DeviceVirtualAddress+=8)
		{
			IMG_UINT32 uInst0 = puInstructions[0];
			IMG_UINT32 uInst1 = puInstructions[1];
			IMG_CHAR pszInst[256];

			/* Line number */
			fprintf(gc->pShaderAnalysisHandle, "%3d: ", i);

			/* Device Virtual Address */
			fprintf(gc->pShaderAnalysisHandle, "0x%.8X ", ui32DeviceVirtualAddress);

			/* Instruction */
			fprintf(gc->pShaderAnalysisHandle, "0x%.8X%.8X  ", uInst1, uInst0);
			UseDisassembleInstruction(UseAsmGetCoreDesc(&psFragmentVariant->psPatchedShader->sTargetDev), uInst0, uInst1, pszInst);
			fprintf(gc->pShaderAnalysisHandle, "%s\n", pszInst);
		}
		fprintf(gc->pShaderAnalysisHandle, "-----------------------------------------------\n");
	}

}
#endif /* defined DEBUG */

/******************************************************************************
 End of file (use.c)
******************************************************************************/

