/******************************************************************************
 * Name         : use.h
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
 * $Log: usegles.h $
 *
 *  --- Revision Logs Removed --- 
 *****************************************************************************/

#ifndef _USE_H_
#define _USE_H_

/*****************************************************************************/

#define GLES1_FRAGMENT_SECATTR_ALPHATEST_OFFSET			4
#define GLES1_FRAGMENT_SECATTR_NUM_FOG					0x00000001
#define GLES1_FRAGMENT_SECATTR_NUM_ALPHATEST			0x00000002
#define GLES1_FRAGMENT_SECATTR_NUM_RESERVED				(GLES1_FRAGMENT_SECATTR_NUM_ALPHATEST + GLES1_FRAGMENT_SECATTR_NUM_FOG)

#define GLES1_VERTEX_SECATTR_CONSTANTBASE               0x00000000
#define GLES1_VERTEX_SECATTR_INDEXABLETEMPBASE			0x00000001
#define GLES1_VERTEX_SECATTR_UNPCKS8_CONST1				0x00000002
#define GLES1_VERTEX_SECATTR_UNPCKS8_CONST2				0x00000003
#define GLES1_VERTEX_SECATTR_UNPCKS16_CONST1			0x00000004
#define GLES1_VERTEX_SECATTR_UNPCKS16_CONST2			0x00000005
#define GLES1_VERTEX_SECATTR_NUM_RESERVED				0x00000006

/*****************************************************************************/

/*
 * USE instruction encoding macros 
 */

/* General purpose */
#define GLES1_USE1_OP(X)		((EURASIA_USE1_OP_##X) << EURASIA_USE1_OP_SHIFT)

#define GLES1_USE0_DST_NUM(X)	((X) << EURASIA_USE0_DST_SHIFT)
#define GLES1_USE0_SRC0_NUM(X)	((X) << EURASIA_USE0_SRC0_SHIFT)
#define GLES1_USE0_SRC1_NUM(X)	((X) << EURASIA_USE0_SRC1_SHIFT)
#define GLES1_USE0_SRC2_NUM(X)	((X) << EURASIA_USE0_SRC2_SHIFT)

#define GLES1_USE1_DST_TYPE(X)	((EURASIA_USE1_D1STDBANK_##X) << EURASIA_USE1_D1BANK_SHIFT)
#define GLES1_USE1_SRC0_TYPE(X)	((EURASIA_USE1_S0STDBANK_##X) << EURASIA_USE1_S0BANK_SHIFT)
#define GLES1_USE0_SRC1_TYPE(X)	((EURASIA_USE0_S1STDBANK_##X) << EURASIA_USE0_S1BANK_SHIFT)
#define GLES1_USE0_SRC2_TYPE(X)	((EURASIA_USE0_S2STDBANK_##X) << EURASIA_USE0_S2BANK_SHIFT)

/* Bitwise instruction macros */
#define GLES1_USE1_BITWISEOP2(X)	((EURASIA_USE1_BITWISE_OP2_##X) << EURASIA_USE1_BITWISE_OP2_SHIFT)

/* SOP2 instruction macros */
#define GLES1_USE0_SOP2_COP(X)	  	((EURASIA_USE0_SOP2_COP_##X) << EURASIA_USE0_SOP2_COP_SHIFT)
#define GLES1_USE0_SOP2_AOP(X)		((EURASIA_USE0_SOP2_AOP_##X) << EURASIA_USE0_SOP2_AOP_SHIFT)

#define GLES1_USE1_SOP2_CSEL1(X)		((EURASIA_USE1_SOP2_CSEL1_##X) << EURASIA_USE1_SOP2_CSEL1_SHIFT)
#define GLES1_USE1_SOP2_CSEL2(X)		((EURASIA_USE1_SOP2_CSEL2_##X) << EURASIA_USE1_SOP2_CSEL2_SHIFT)
#define GLES1_USE1_SOP2_ASEL1(X)		((EURASIA_USE1_SOP2_ASEL1_##X) << EURASIA_USE1_SOP2_ASEL1_SHIFT)
#define GLES1_USE1_SOP2_ASEL2(X)		((EURASIA_USE1_SOP2_ASEL2_##X) << EURASIA_USE1_SOP2_ASEL2_SHIFT)

#define GLES1_USE1_SOP2_CSRC1_COMP	(EURASIA_USE1_SOP2_CMOD1_COMPLEMENT << EURASIA_USE1_SOP2_CMOD1_SHIFT)
#define GLES1_USE1_SOP2_CSRC2_COMP	(EURASIA_USE1_SOP2_CMOD2_COMPLEMENT << EURASIA_USE1_SOP2_CMOD2_SHIFT)
#define GLES1_USE1_SOP2_ASRC1_COMP	(EURASIA_USE1_SOP2_AMOD1_COMPLEMENT << EURASIA_USE1_SOP2_AMOD1_SHIFT)
#define GLES1_USE1_SOP2_ASRC2_COMP	(EURASIA_USE1_SOP2_AMOD2_COMPLEMENT << EURASIA_USE1_SOP2_AMOD2_SHIFT)

/* SOPWM instruction macros */
#define GLES1_USE1_SOPWM_COP(X)		((EURASIA_USE1_SOP2WM_COP_##X) << EURASIA_USE1_SOP2WM_COP_SHIFT)
#define GLES1_USE1_SOPWM_AOP(X)		((EURASIA_USE1_SOP2WM_AOP_##X) << EURASIA_USE1_SOP2WM_AOP_SHIFT)
#define GLES1_USE1_SOPWM_WRITEMASK(X)	((X) << EURASIA_USE1_SOP2WM_WRITEMASK_SHIFT)

#define GLES1_USE1_SOPWM_SEL1(X)		((EURASIA_USE1_SOP2WM_SEL1_##X) << EURASIA_USE1_SOP2WM_SEL1_SHIFT)
#define GLES1_USE1_SOPWM_SEL2(X)		((EURASIA_USE1_SOP2WM_SEL2_##X) << EURASIA_USE1_SOP2WM_SEL2_SHIFT)

#define GLES1_USE1_SOPWM_SRC1_COMP	(EURASIA_USE1_SOP2WM_MOD1_COMPLEMENT << EURASIA_USE1_SOP2WM_MOD1_SHIFT)
#define GLES1_USE1_SOPWM_SRC2_COMP	(EURASIA_USE1_SOP2WM_MOD2_COMPLEMENT << EURASIA_USE1_SOP2WM_MOD2_SHIFT)

/* USE instruction helper macros */
#define GLES1_USE_OP_AND			(GLES1_USE1_OP(ANDOR) | GLES1_USE1_BITWISEOP2(AND))
#define GLES1_USE_OP_OR			(GLES1_USE1_OP(ANDOR) | GLES1_USE1_BITWISEOP2(OR))
#define GLES1_USE_OP_XOR			(GLES1_USE1_OP(XOR)   | GLES1_USE1_BITWISEOP2(XOR))

/* GLES1_USE_SRC0() not defined as fields are split across 2 32-bit words */
#define GLES1_USE_SRC1(TYPE, NUM)	((GLES1_USE0_SRC1_TYPE(TYPE)) | (GLES1_USE0_SRC1_NUM(NUM)))
#define GLES1_USE_SRC2(TYPE, NUM)	((GLES1_USE0_SRC2_TYPE(TYPE)) | (GLES1_USE0_SRC2_NUM(NUM)))

/*****************************************************************************/

GLES1_MEMERROR SetupUSEVertexShader(GLES1Context *gc, IMG_BOOL *pbProgramChanged);
GLES1_MEMERROR SetupUSEFragmentShader(GLES1Context *gc, IMG_BOOL *pbProgramChanged);

IMG_BOOL InitSpecialUSECodeBlocks(GLES1Context *gc);
IMG_VOID FreeSpecialUSECodeBlocks(GLES1Context *gc);

IMG_DEV_VIRTADDR GetPixelEventUSEAddress(GLES1Context *gc);
IMG_DEV_VIRTADDR GetStateCopyUSEAddress(GLES1Context *gc, IMG_UINT32 ui32NumStateDWords);
GLES1_MEMERROR WriteEOTUSSECode(GLES1Context *gc, EGLPixelBEState *psPBEState, 
								IMG_DEV_VIRTADDR *puDevAddr, IMG_BOOL bPatch);

IMG_VOID GetFBBlendType(GLES1Context *gc, IMG_BOOL *pbIsBlendNone, IMG_BOOL *pbIsBlendTranslucent);

GLES1_MEMERROR WriteUSEVertexShaderMemConsts(GLES1Context *gc);

#endif /* _USE_H_ */
