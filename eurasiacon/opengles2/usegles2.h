/******************************************************************************
 * Name         : usegles2.h
 * Title        : Header for USE instruction creation code
 * Author       : Ben Bowman
 * Created      : 24/06/2005
 *
 * Copyright    : 2005-2006 by Imagination Technologies Limited. All rights reserved.
 * 				  No part of this software, either material or conceptual 
 * 				  may be copied or distributed, transmitted, transcribed,
 * 				  stored in a retrieval system or translated into any 
 * 				  human or computer language in any form by any means,
 * 				  electronic, mechanical, manual or other-wise, or 
 * 				  disclosed to third parties without the express written
 * 				  permission of Imagination Technologies Limited, Unit 8, HomePark
 * 				  Industrial Estate, King's Langley, Hertfordshire,
 * 				  WD4 8LZ, U.K.
 *
 * Description  : Header for USE instruction creation code
 *
 * Platform     : ANSI
 *
 * Modifications:
 * $Log: usegles2.h $
 *****************************************************************************/

#ifndef _USEGLES2_H_
#define _USEGLES2_H_

/*****************************************************************************/
#define GLES2_USSE_CODE_HEAP_BASE_ADDR					0x02000000

#define GLES2_PIXELPACKING_MAX_CODE_SIZE_IN_DWORDS		4
#define GLES2_FBBLEND_MAX_CODE_SIZE_IN_DWORDS			10
#define GLES2_ISPFEEDBACK_MAX_CODE_SIZE_IN_DWORDS		10
#define GLES2_COLORMASK_MAX_CODE_SIZE_IN_DWORDS			2
#define GLES2_VERTEXUNPACK_MAX_CODE_SIZE_IN_DWORDS		192

#define GLES2_VISIBILITYTEST_NUM_PHASE1_INSTRUCTIONS	1

/* Important! Changes to these values should be reflected in oglcompiler/esbincompiler.c */
#define GLES2_FRAGMENT_SECATTR_CONSTANTBASE				0x00000000
#define GLES2_FRAGMENT_SECATTR_INDEXABLETEMPBASE		0x00000001
#define GLES2_FRAGMENT_SECATTR_FBBLENDCONST				0x00000002
#define GLES2_FRAGMENT_SECATTR_ISPFEEDBACK0				0x00000003
#define GLES2_FRAGMENT_SECATTR_ISPFEEDBACK1				0x00000004
#define GLES2_FRAGMENT_SECATTR_SCRATCHBASE             0x00000005
#define GLES2_FRAGMENT_SECATTR_RESERVED1               0x00000006  // Reserved for future use for compatibility with old binaries
#define GLES2_FRAGMENT_SECATTR_RESERVED2               0x00000007  // Reserved for future use for compatibility with old binaries
#define GLES2_FRAGMENT_SECATTR_RESERVED3               0x00000008  // Reserved for future use for compatibility with old binaries
#define GLES2_FRAGMENT_SECATTR_NUM_RESERVED				0x00000009

#define GLES2_VERTEX_SECATTR_CONSTANTBASE              0x00000000
#define GLES2_VERTEX_SECATTR_INDEXABLETEMPBASE			0x00000001
#define GLES2_VERTEX_SECATTR_UNPCKS8_CONST1            0x00000002
#define GLES2_VERTEX_SECATTR_UNPCKS8_CONST2            0x00000003
#define GLES2_VERTEX_SECATTR_UNPCKS16_CONST1           0x00000004
#define GLES2_VERTEX_SECATTR_UNPCKS16_CONST2           0x00000005
#define GLES2_VERTEX_SECATTR_SCRATCHBASE               0x00000006
#define GLES2_VERTEX_SECATTR_RESERVED1                 0x00000007  // Reserved for future use for compatibility with old binaries
#define GLES2_VERTEX_SECATTR_RESERVED2                 0x00000008  // Reserved for future use for compatibility with old binaries
#define GLES2_VERTEX_SECATTR_RESERVED3                 0x00000009  // Reserved for future use for compatibility with old binaries
#define GLES2_VERTEX_SECATTR_NUM_RESERVED				0x0000000A

/*****************************************************************************/

/*
 * USE instruction encoding macros 
 */

/* General purpose */
#define GLES2_USE1_OP(X)		((IMG_UINT32)(EURASIA_USE1_OP_##X) << EURASIA_USE1_OP_SHIFT)

#define GLES2_USE0_DST_NUM(X)	((X) << EURASIA_USE0_DST_SHIFT)
#define GLES2_USE0_SRC0_NUM(X)	((X) << EURASIA_USE0_SRC0_SHIFT)
#define GLES2_USE0_SRC1_NUM(X)	((X) << EURASIA_USE0_SRC1_SHIFT)
#define GLES2_USE0_SRC2_NUM(X)	((X) << EURASIA_USE0_SRC2_SHIFT)

#define GLES2_USE1_DST_TYPE(X)	((IMG_UINT32)(EURASIA_USE1_D1STDBANK_##X) << EURASIA_USE1_D1BANK_SHIFT)
#define GLES2_USE1_SRC0_TYPE(X)	((IMG_UINT32)(EURASIA_USE1_S0STDBANK_##X) << EURASIA_USE1_S0BANK_SHIFT)
#define GLES2_USE0_SRC1_TYPE(X)	((IMG_UINT32)(EURASIA_USE0_S1STDBANK_##X) << EURASIA_USE0_S1BANK_SHIFT)
#define GLES2_USE0_SRC2_TYPE(X)	((IMG_UINT32)(EURASIA_USE0_S2STDBANK_##X) << EURASIA_USE0_S2BANK_SHIFT)

/* SOP2 instruction macros */
#define GLES2_USE0_SOP2_COP(X)	  	((EURASIA_USE0_SOP2_COP_##X) << EURASIA_USE0_SOP2_COP_SHIFT)
#define GLES2_USE0_SOP2_AOP(X)		((EURASIA_USE0_SOP2_AOP_##X) << EURASIA_USE0_SOP2_AOP_SHIFT)

#define GLES2_USE1_SOP2_CSEL1(X)		((EURASIA_USE1_SOP2_CSEL1_##X) << EURASIA_USE1_SOP2_CSEL1_SHIFT)
#define GLES2_USE1_SOP2_CSEL2(X)		((EURASIA_USE1_SOP2_CSEL2_##X) << EURASIA_USE1_SOP2_CSEL2_SHIFT)
#define GLES2_USE1_SOP2_ASEL1(X)		((EURASIA_USE1_SOP2_ASEL1_##X) << EURASIA_USE1_SOP2_ASEL1_SHIFT)
#define GLES2_USE1_SOP2_ASEL2(X)		((EURASIA_USE1_SOP2_ASEL2_##X) << EURASIA_USE1_SOP2_ASEL2_SHIFT)

#define GLES2_USE1_SOP2_CSRC1_COMP	(EURASIA_USE1_SOP2_CMOD1_COMPLEMENT << EURASIA_USE1_SOP2_CMOD1_SHIFT)
#define GLES2_USE1_SOP2_CSRC2_COMP	(EURASIA_USE1_SOP2_CMOD2_COMPLEMENT << EURASIA_USE1_SOP2_CMOD2_SHIFT)
#define GLES2_USE1_SOP2_ASRC1_COMP	(EURASIA_USE1_SOP2_AMOD1_COMPLEMENT << EURASIA_USE1_SOP2_AMOD1_SHIFT)
#define GLES2_USE1_SOP2_ASRC2_COMP	(EURASIA_USE1_SOP2_AMOD2_COMPLEMENT << EURASIA_USE1_SOP2_AMOD2_SHIFT)

/* SOPWM instruction macros */
#define GLES2_USE1_SOPWM_COP(X)		((EURASIA_USE1_SOP2WM_COP_##X) << EURASIA_USE1_SOP2WM_COP_SHIFT)
#define GLES2_USE1_SOPWM_AOP(X)		((EURASIA_USE1_SOP2WM_AOP_##X) << EURASIA_USE1_SOP2WM_AOP_SHIFT)
#define GLES2_USE1_SOPWM_WRITEMASK(X)	((X) << EURASIA_USE1_SOP2WM_WRITEMASK_SHIFT)

#define GLES2_USE1_SOPWM_SEL1(X)		((EURASIA_USE1_SOP2WM_SEL1_##X) << EURASIA_USE1_SOP2WM_SEL1_SHIFT)
#define GLES2_USE1_SOPWM_SEL2(X)		((EURASIA_USE1_SOP2WM_SEL2_##X) << EURASIA_USE1_SOP2WM_SEL2_SHIFT)

#define GLES2_USE1_SOPWM_SRC1_COMP	(EURASIA_USE1_SOP2WM_MOD1_COMPLEMENT << EURASIA_USE1_SOP2WM_MOD1_SHIFT)
#define GLES2_USE1_SOPWM_SRC2_COMP	(EURASIA_USE1_SOP2WM_MOD2_COMPLEMENT << EURASIA_USE1_SOP2WM_MOD2_SHIFT)

/* GLES2_USE_SRC0() not defined as fields are split across 2 32-bit words */
#define GLES2_USE_SRC1(TYPE, NUM)	((GLES2_USE0_SRC1_TYPE(TYPE)) | (GLES2_USE0_SRC1_NUM(NUM)))
#define GLES2_USE_SRC2(TYPE, NUM)	((GLES2_USE0_SRC2_TYPE(TYPE)) | (GLES2_USE0_SRC2_NUM(NUM)))

/*****************************************************************************/
GLES2_MEMERROR SetupUSEVertexShader(GLES2Context *gc, IMG_BOOL *pbProgramChanged);
GLES2_MEMERROR SetupUSEFragmentShader(GLES2Context *gc, IMG_BOOL *pbProgramChanged);

IMG_BOOL InitSpecialUSECodeBlocks(GLES2Context *gc);
IMG_VOID FreeSpecialUSECodeBlocks(GLES2Context *gc);

IMG_DEV_VIRTADDR GetPixelEventUSEAddress(GLES2Context *gc);
IMG_DEV_VIRTADDR GetStateCopyUSEAddress(GLES2Context *gc, IMG_UINT32 ui32NumStateDWords);

GLES2_MEMERROR WriteEOTUSSECode(GLES2Context *gc, EGLPixelBEState *psPBEState, 
								IMG_DEV_VIRTADDR *puDevAddr, IMG_BOOL bPatch);

IMG_UINT32 CreateFBBlendUSECode(GLES2Context *gc,
								IMG_UINT32 ui32SrcReg,
								IMG_BOOL bColorMask,
								IMG_BOOL bNeedsISPFeedback,
								IMG_UINT32	*pui32Code,
								IMG_UINT32	*pui32NumTempRegsUsed);

IMG_VOID GetFBBlendType(GLES2Context *gc, IMG_BOOL *pbIsBlendNone, 
									 IMG_BOOL *pbIsBlendTranslucent,
									 IMG_BOOL *pbHasConstantColorBlend);

IMG_UINT32 CreateColorMaskUSECode(GLES2Context *gc,
								   IMG_BOOL bSrcInTemp,
								  IMG_UINT32 ui32SrcReg,
								  IMG_UINT32	*pui32Code);

#endif /* _USEGLES2_H_ */
