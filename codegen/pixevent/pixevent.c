/******************************************************************************
 * Name         : pixevent.c
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
 * $Log: pixevent.c $
 *****************************************************************************/
#include "img_defs.h"
#include "sgxdefs.h"
#include "pixevent.h"
#include "usecodegen.h"

#if !defined(__psp2__)
/***********************************************************************************
 Function Name      : WritePTOffUSSECode
 Inputs             : pui32Buffer, pui32EmitWords, ui32SideBand
 Outputs            : pui32Buffer
 Returns            : pui32Buffer
 Description        : Writes PTOff USSE code. 
************************************************************************************/
IMG_INTERNAL IMG_UINT32 * WritePTOffUSSECode(IMG_UINT32 *pui32BufferBase)
{
	IMG_UINT32 *pui32Buffer;

	pui32Buffer = pui32BufferBase;;

#if defined(SGX543) || defined(SGX544) || defined(SGX554)
	/* PTOFF instruction has been removed */ 
	/* 
	   phas #0, #0, pixel, end, parallel: Insert Single Phas instruction 
	*/	
	BuildPHASLastPhase((PPVR_USE_INST)pui32Buffer, EURASIA_USE1_OTHER2_PHAS_END);
	pui32Buffer += 2;

#else
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	/* 
	   phas #0, #0, pixel, end, parallel: Insert Single Phas instruction 
	*/	
	BuildPHASLastPhase((PPVR_USE_INST)pui32Buffer, 0 /* ui32End */);
	pui32Buffer += 2;
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

	/*
		Set up the instruction which will do the end of pass event.
	*/
	*pui32Buffer++ = 0;

	*pui32Buffer++ = ((EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
					(EURASIA_USE1_VISTEST_OP2_PTCTRL << EURASIA_USE1_OTHER_OP2_SHIFT) |
					(EURASIA_USE1_SPECIAL_OPCAT_VISTEST << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
					EURASIA_USE1_END |
					(EURASIA_USE1_VISTEST_PTCTRL_TYPE_PTOFF << EURASIA_USE1_VISTEST_PTCTRL_TYPE_SHIFT));
#endif /* 543 */

	return pui32Buffer;
}
#endif

/***********************************************************************************
 Function Name      : WriteEndOfRenderUSSECode
 Inputs             : pui32Buffer, pui32EmitWords, ui32SideBand
 Outputs            : pui32Buffer
 Returns            : pui32Buffer
 Description        : Writes end of pass USSE code. 
************************************************************************************/
#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)

#if !defined(__psp2__)

IMG_INTERNAL IMG_UINT32 * WriteEndOfRenderUSSECode(IMG_UINT32 *pui32BufferBase)
{
	IMG_UINT32 *pui32Buffer;
	IMG_UINT32 ui32SideBandWord = EURASIA_PIXELBESB_TILERELATIVE;

	pui32Buffer = pui32BufferBase;;

	/* 
	   phas #0, #0, pixel, end, parallel: Insert Single Phas instruction 
	*/	
	BuildPHASLastPhase((PPVR_USE_INST)pui32Buffer, 0 /* ui32End */);
	pui32Buffer += 2;

	/* mov r0, #0 */
	BuildLIMM((PPVR_USE_INST)pui32Buffer,
				EURASIA_USE1_EPRED_ALWAYS,
				0 /* ui32Flags */,
				USE_REGTYPE_TEMP,
				0,
				0);
	
	pui32Buffer += 2;

	/* mov r1, #0 */
	BuildLIMM((PPVR_USE_INST)pui32Buffer,
				EURASIA_USE1_EPRED_ALWAYS,
				0 /* ui32Flags */,
				USE_REGTYPE_TEMP,
				1,
				0);
	
	pui32Buffer += 2;

	/* emitpix.end #0  r0, #0, r0, #EURASIA_PIXELBESB_TILERELATIVE */
	*pui32Buffer++ = (EURASIA_USE0_S1EXTBANK_IMMEDIATE		<< EURASIA_USE0_S1BANK_SHIFT)					|
					EURASIA_USE0_EMIT_FREEP																	|
					(((ui32SideBandWord >> 0)				<< EURASIA_USE0_EMIT_SIDEBAND_197_192_SHIFT) 
															& ~EURASIA_USE0_EMIT_SIDEBAND_197_192_CLRMSK)	|
					(0										<< EURASIA_USE0_SRC0_SHIFT)						|
					(0										<< EURASIA_USE0_SRC1_SHIFT);

	*pui32Buffer++ = (EURASIA_USE1_OP_SPECIAL				<< EURASIA_USE1_OP_SHIFT)							|
						(EURASIA_USE1_OTHER_OP2_EMIT			<< EURASIA_USE1_OTHER_OP2_SHIFT)				|	
						(((ui32SideBandWord >> 12)				<< EURASIA_USE1_EMIT_SIDEBAND_205_204_SHIFT) 
																& ~EURASIA_USE1_EMIT_SIDEBAND_205_204_CLRMSK)	|
						(EURASIA_USE1_SPECIAL_OPCAT_OTHER		<< EURASIA_USE1_SPECIAL_OPCAT_SHIFT)			|	
							EURASIA_USE1_END																	|
							EURASIA_USE1_S1BEXT																	|
						(EURASIA_USE1_EMIT_TARGET_PIXELBE		<< EURASIA_USE1_EMIT_TARGET_SHIFT)				|
						(((ui32SideBandWord >> 6)				<< EURASIA_USE1_EMIT_SIDEBAND_203_198_SHIFT) 
																& ~EURASIA_USE1_EMIT_SIDEBAND_203_198_CLRMSK)	|
						(EURASIA_USE1_S0STDBANK_TEMP			<< EURASIA_USE1_S0BANK_SHIFT)					|
						(0										<< EURASIA_USE1_EMIT_INCP_SHIFT);

	return pui32Buffer;
}
#endif

#else /* SGX_FEATURE_UNIFIED_STORE_64BITS */

IMG_INTERNAL IMG_UINT32 * WriteEndOfRenderUSSECode(IMG_UINT32 *pui32BufferBase)
{
	IMG_UINT32 *pui32Buffer;
	IMG_UINT32 ui32SideBandWord = EURASIA_PIXELBE2SB_TILERELATIVE;

	pui32Buffer = pui32BufferBase;;

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	/* 
	   phas #0, #0, pixel, end, parallel: Insert Single Phas instruction 
	*/	
	BuildPHASLastPhase((PPVR_USE_INST)pui32Buffer, 0 /* ui32End */);
	pui32Buffer += 2;
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

	/* mov r0, #0 */
	BuildLIMM((PPVR_USE_INST)pui32Buffer,
				EURASIA_USE1_EPRED_ALWAYS,
				0 /* ui32Flags */,
				USE_REGTYPE_TEMP,
				0,
				0);
	
	pui32Buffer += 2;

	/* emitpix.end #0  r0, #0, r0, #EURASIA_PIXELBE2SB_TILERELATIVE */
	*pui32Buffer++ = (EURASIA_USE0_S1EXTBANK_IMMEDIATE		<< EURASIA_USE0_S1BANK_SHIFT)					|
					EURASIA_USE0_EMIT_FREEP																	|
					(((ui32SideBandWord >> 0)				<< EURASIA_USE0_EMIT_SIDEBAND_101_96_SHIFT) 
															& ~EURASIA_USE0_EMIT_SIDEBAND_101_96_CLRMSK)	|
					(0										<< EURASIA_USE0_SRC0_SHIFT)						|
					(0										<< EURASIA_USE0_SRC1_SHIFT);

	*pui32Buffer++ = (EURASIA_USE1_OP_SPECIAL				<< EURASIA_USE1_OP_SHIFT)							|
						(EURASIA_USE1_OTHER_OP2_EMIT			<< EURASIA_USE1_OTHER_OP2_SHIFT)				|	
						(((ui32SideBandWord >> 12)				<< EURASIA_USE1_EMIT_SIDEBAND_109_108_SHIFT) 
																& ~EURASIA_USE1_EMIT_SIDEBAND_109_108_CLRMSK)	|
						(EURASIA_USE1_SPECIAL_OPCAT_OTHER		<< EURASIA_USE1_SPECIAL_OPCAT_SHIFT)			|	
							EURASIA_USE1_END																	|
							EURASIA_USE1_S1BEXT																	|
						(EURASIA_USE1_EMIT_TARGET_PIXELBE		<< EURASIA_USE1_EMIT_TARGET_SHIFT)				|
						(((ui32SideBandWord >> 6)				<< EURASIA_USE1_EMIT_SIDEBAND_107_102_SHIFT) 
																& ~EURASIA_USE1_EMIT_SIDEBAND_107_102_CLRMSK)	|
						(EURASIA_USE1_S0STDBANK_TEMP			<< EURASIA_USE1_S0BANK_SHIFT)					|
						(0										<< EURASIA_USE1_EMIT_INCP_SHIFT);

	return pui32Buffer;
}

#endif /* SGX_FEATURE_UNIFIED_STORE_64BITS */

#if defined(SGX545)
/***********************************************************************************
 Function Name      : WriteEndOfTileUSSECodeMultiEmit
 Inputs             : pui32Buffer, pui32EmitWords, ui32EmitCount
 Outputs            : pui32Buffer
 Returns            : pui32Buffer
 Description        : Writes end of tile USSE code. 
************************************************************************************/
IMG_INTERNAL IMG_PUINT32 WriteEndOfTileUSSECodeMultiEmit(IMG_UINT32 *pui32BufferBase,
														 IMG_UINT32 pui32EmitWords[][STATE_PBE_DWORDS + 1], 
														 IMG_UINT32 ui32EmitCount
														 #if defined(SGX545)
														 ,IMG_BOOL bFlushCache
														 #endif
														 )
{
	IMG_INT32	i32EmitIdx;
	IMG_PUINT32	pui32Buffer;

	pui32Buffer = pui32BufferBase;

	/*
		No subsequent phase after this one.
	*/
	BuildPHASLastPhase((PPVR_USE_INST)pui32Buffer,
					   0 /* ui32End */);
	pui32Buffer += 2;
	
	#if defined(SGX545)
	if(bFlushCache)
	{
		/*
			If this is the last tile in the render then flush the cache..
		*/
		BuildALUTST((PPVR_USE_INST)pui32Buffer,
					EURASIA_USE1_EPRED_ALWAYS,
					1,
					EURASIA_USE1_TEST_CRCOMB_AND,
					USE_REGTYPE_TEMP,
					0, // r0
					0,
					USE_REGTYPE_PRIMATTR,
					0, // pa0
					USE_REGTYPE_IMMEDIATE,
					EURASIA_PDS_IR1_PIX_EVENT_ENDRENDER,
					0,
					EURASIA_USE1_TEST_STST_NONE,
					EURASIA_USE1_TEST_ZTST_ZERO,
					EURASIA_USE1_TEST_CHANCC_SELECT0,
					EURASIA_USE0_TEST_ALUSEL_BITWISE,
					EURASIA_USE0_TEST_ALUOP_BITWISE_AND);
		pui32Buffer+=2;
		
		BuildBR((PPVR_USE_INST)pui32Buffer, EURASIA_USE1_EPRED_P0, 2, 0);
		pui32Buffer+=2;
		
		BuildCFI((PPVR_USE_INST)pui32Buffer,
			   SGX545_USE1_OTHER2_CFI_GLOBAL | SGX545_USE1_OTHER2_CFI_FLUSH | SGX545_USE1_OTHER2_CFI_INVALIDATE,
			   0, // mode?
			   USE_REGTYPE_IMMEDIATE,
			   SGX545_USE_OTHER2_CFI_DATAMASTER_PIXEL, // data master
			   USE_REGTYPE_IMMEDIATE,
			   0, // address (don't care, we're doing a global flush)
			   SGX545_USE1_OTHER2_CFI_LEVEL_MAX);
		pui32Buffer+=2;
	}
	#endif

	/*
		Do the emits in reverse order of partition offset so the EMIT that frees the
		partitions has an INCP of 0.
	*/
	for (i32EmitIdx = ui32EmitCount - 1; i32EmitIdx >= 0; i32EmitIdx--)
	{
		IMG_PUINT32	pui32StateWords = pui32EmitWords[i32EmitIdx];
		IMG_UINT32	ui32Sideband = pui32StateWords[STATE_PBE_DWORDS];
		IMG_BOOL	bFreep;
		IMG_UINT32	ui32INCP = i32EmitIdx * 2;
		IMG_UINT32	ui32Flags;
		IMG_UINT32	ui32StateWordIdx;

		/*
			Free partitions only on the last emit.
		*/
		if (i32EmitIdx == 0)
		{
			bFreep = IMG_TRUE;
			ui32Flags = EURASIA_USE1_END;
		}
		else
		{
			bFreep = IMG_FALSE;
			ui32Flags = 0;
		}

		/*
			Load the state data for the emit into the first N temporary registers.
		*/
		for (ui32StateWordIdx = 0; ui32StateWordIdx < STATE_PBE_DWORDS; ui32StateWordIdx++)
		{
			IMG_UINT32	ui32StateWord;

			ui32StateWord = pui32StateWords[ui32StateWordIdx];
			if (i32EmitIdx > 0 && ui32StateWordIdx == 1)
			{
				/*
					Don't advance the TILE X/Y fifo until the last emit.
				*/
				ui32StateWord |= EURASIA_PIXELBE1S1_NOADVANCE;
			}

			BuildLIMM((PPVR_USE_INST)pui32Buffer,
					  EURASIA_USE1_EPRED_ALWAYS,
					  0 /* ui32Flags */,
					  USE_REGTYPE_TEMP,
					  ui32StateWordIdx,
					  ui32StateWord);
			pui32Buffer += 2;
		}

		/*
			First emit.
		*/
		_BuildEMITPIX((PPVR_USE_INST)pui32Buffer,	
					  EURASIA_USE1_EPRED_ALWAYS,
					  0 /* ui32Flags */,
					  USE_REGTYPE_TEMP,			/* r0 */
					  0,
					  USE_REGTYPE_TEMP,			/* r1 */
					  1,
					  USE_REGTYPE_IMMEDIATE,	/* #0 */
					  0,
					  ui32INCP,
					  bFreep,
					  EURASIA_PIXELBE1SB_TWOEMITS /* ui32SideBand */);
		pui32Buffer += 2;

		/*
			Second emit.
		*/
		_BuildEMITPIX((PPVR_USE_INST)pui32Buffer,	
					  EURASIA_USE1_EPRED_ALWAYS,
					  ui32Flags /* ui32Flags */,
					  USE_REGTYPE_TEMP,			/* r2 */
					  2,
					  USE_REGTYPE_TEMP,			/* r3 */
					  3,
					  #if defined(SGX_FEATURE_PIXELBE_32K_LINESTRIDE)
					  USE_REGTYPE_TEMP,			/* r4 */
					  4,
					  #else /* defined(SGX_FEATURE_PIXELBE_32K_LINESTRIDE) */
					  USE_REGTYPE_IMMEDIATE,	/* #0 */
					  0,
					  #endif /* defined(SGX_FEATURE_PIXELBE_32K_LINESTRIDE) */
					  ui32INCP,
					  bFreep,
					  ui32Sideband /* ui32SideBand */);
		pui32Buffer += 2;
	}

	return pui32Buffer;
}
#endif /* defined(SGX545) */

/***********************************************************************************
 Function Name      : WriteEndOfTileUSSECode
 Inputs             : pui32Buffer, pui32EmitWords, ui32SideBand
 Outputs            : pui32Buffer, pui32StateWordLimmOffset
 Returns            : pui32Buffer
 Description        : Writes end of tile USSE code. 
************************************************************************************/
#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)

#if defined(FIX_HW_BRN_31982)
IMG_INTERNAL IMG_UINT32 * WriteEndOfTileUSSECode2xMSAA(IMG_UINT32 *pui32BufferBase,
												 IMG_UINT32 *pui32EmitWords, 
												 IMG_UINT32 ui32SideBand)
{
	IMG_UINT32			* pui32Buffer;
	IMG_UINT32			ui32LineStride;
	IMG_UINT32			* pui32LoopBase;
	IMG_UINT32			i;
	IMG_DEV_VIRTADDR	sFBAddr;
	
	pui32Buffer = pui32BufferBase;

	/* We can do 4bpp strided non clipped renders */
	if ((pui32EmitWords[0] & ~EURASIA_PIXELBES0LO_MEMLAYOUT_CLRMSK) != EURASIA_PIXELBES0LO_MEMLAYOUT_LINEAR)
	{
		PVR_DPF((PVR_DBG_ERROR, "WriteEndOfTileUSSECode2xMSAA : memory layout not supported"));
	}

	/* 
	   phas #0, #0, pixel, end, parallel: Insert Single Phas instruction 
	*/		
	BuildPHASLastPhase((PPVR_USE_INST)pui32Buffer, 0 /* ui32End */);
	pui32Buffer += 2;

	/*
	 * Get the DevVAddr from the PBE state
	 */
	sFBAddr.uiAddr = ((pui32EmitWords[1]) >> EURASIA_PIXELBES0HI_FBADDR_SHIFT) << EURASIA_PIXELBE_FBADDR_ALIGNSHIFT;

	/* mov r0, #fbAddr */
	BuildLIMM((PPVR_USE_INST)pui32Buffer,
			EURASIA_USE1_EPRED_ALWAYS,
			0 /* ui32Flags */,
			USE_REGTYPE_TEMP,
			0,
			sFBAddr.uiAddr);
	pui32Buffer += 2;

	/* mov r3, #stride*/
	ui32LineStride = ((pui32EmitWords[2]) & ~EURASIA_PIXELBES1LO_LINESTRIDE_CLRMSK) >> EURASIA_PIXELBES1LO_LINESTRIDE_SHIFT; 
	ui32LineStride = 2 * (ui32LineStride + 1);

	BuildLIMM((PPVR_USE_INST)pui32Buffer,
			EURASIA_USE1_EPRED_ALWAYS,
			0 /* ui32Flags */,
			USE_REGTYPE_TEMP,
			3,
			ui32LineStride);
	pui32Buffer += 2;

	/* 
	 * offset the tile
	 */
	/* mov r4, #128*/
	BuildLIMM((PPVR_USE_INST)pui32Buffer,
			EURASIA_USE1_EPRED_ALWAYS,
			0 /* ui32Flags */,
			USE_REGTYPE_TEMP,
			4,
			128);
	pui32Buffer += 2;

	/* get tiley values 0, 1, 2..*/
	/* shr  r1, pa0, #8*/
	BuildSHRIMM((PPVR_USE_INST)pui32Buffer,
			EURASIA_USE1_EPRED_ALWAYS,
			0,
			0,
			USE_REGTYPE_TEMP,
			1,
			USE_REGTYPE_PRIMATTR,
			0,
			8,
			0);
	pui32Buffer += 2;

	/* mask the tiley*/
	/* and  r1, r1, #ff*/
	BuildANDIMM((PPVR_USE_INST)pui32Buffer,
			EURASIA_USE1_EPRED_ALWAYS,
			0,
			0,
			USE_REGTYPE_TEMP,
			1,
			USE_REGTYPE_TEMP,
			1,
			0xff,
			0);
	pui32Buffer += 2;

	/* imae r2, r1.low, r4, #0, u32*/
	BuildIMAE((PPVR_USE_INST)pui32Buffer,
			EURASIA_USE1_EPRED_ALWAYS,
			0,
			(EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT),
			USE_REGTYPE_TEMP,
			2,
			USE_REGTYPE_TEMP,
			1,
			0,
			USE_REGTYPE_TEMP,
			4,
			0,
			USE_REGTYPE_IMMEDIATE,
			0,
			0);
	pui32Buffer += 2;

	/* imae r0, r2.low, r3.low, r0, u32*/
	BuildIMAE((PPVR_USE_INST)pui32Buffer,
			EURASIA_USE1_EPRED_ALWAYS,
			0,
			(EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT),
			USE_REGTYPE_TEMP,
			0,
			USE_REGTYPE_TEMP,
			2,
			0,
			USE_REGTYPE_TEMP,
			3,
			0,
			USE_REGTYPE_TEMP,
			0,
			0);
	pui32Buffer += 2;

	/* get tilex values 0, 2, 4..*/
	/* and  r1, pa0, #ff*/ 
	BuildANDIMM((PPVR_USE_INST)pui32Buffer,
			EURASIA_USE1_EPRED_ALWAYS,
			0,
			0,
			USE_REGTYPE_TEMP,
			1,
			USE_REGTYPE_PRIMATTR,
			0,
			0xff,
			0);
	pui32Buffer += 2;

	/* imae r0, r1.low, #64, r0, u32*/
	BuildIMAE((PPVR_USE_INST)pui32Buffer,
			EURASIA_USE1_EPRED_ALWAYS,
			0,
			(EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT),
			USE_REGTYPE_TEMP,
			0,
			USE_REGTYPE_TEMP,
			1,
			0,
			USE_REGTYPE_IMMEDIATE,
			64,
			0,
			USE_REGTYPE_TEMP,
			0,
			0);
	pui32Buffer += 2;

	/*
	 * offset the pipe
	 */
	/* mov  r2, #64*/
	BuildLIMM((PPVR_USE_INST)pui32Buffer,
			EURASIA_USE1_EPRED_ALWAYS,
			0 /* ui32Flags */,
			USE_REGTYPE_TEMP,
			2,
			64);
	pui32Buffer += 2;

	/* shr  r1, g17, #1*/
	*pui32Buffer++ =
		(IMG_UINT32)((0x81 << 0) | (0x28 << 8) | (0x20 << 16) | (0x60 << 24));
	*pui32Buffer++ =
		(IMG_UINT32)((0x00 << 0) | (0x00 << 8) | (0x83 << 16) | (0x68 << 24));

	/* imae r1, r1.low, r2.low, #0, u32*/
	BuildIMAE((PPVR_USE_INST)pui32Buffer,
			EURASIA_USE1_EPRED_ALWAYS,
			0,
			(EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT),
			USE_REGTYPE_TEMP,
			1,
			USE_REGTYPE_TEMP,
			1,
			0,
			USE_REGTYPE_TEMP,
			2,
			0,
			USE_REGTYPE_IMMEDIATE,
			0,
			0);
	pui32Buffer += 2;

	/* imae r0, r3.low, r1, r0, u32*/
	BuildIMAE((PPVR_USE_INST)pui32Buffer,
			EURASIA_USE1_EPRED_ALWAYS,
			0,
			(EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT),
			USE_REGTYPE_TEMP,
			0,
			USE_REGTYPE_TEMP,
			3,
			0,
			USE_REGTYPE_TEMP,
			1,
			0,
			USE_REGTYPE_TEMP,
			0,
			0);
	pui32Buffer += 2;

	/* and  r1, g17, #1*/
	*pui32Buffer++ =
		(IMG_UINT32)((0x81 << 0) | (0x28 << 8) | (0x20 << 16) | (0x60 << 24));
	*pui32Buffer++ =
		(IMG_UINT32)((0x00 << 0) | (0x00 << 8) | (0x83 << 16) | (0x50 << 24));

	/* imae r0, r2.low, r1.low, r0, u32*/
	BuildIMAE((PPVR_USE_INST)pui32Buffer,
			EURASIA_USE1_EPRED_ALWAYS,
			0,
			(EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT),
			USE_REGTYPE_TEMP,
			0,
			USE_REGTYPE_TEMP,
			2,
			0,
			USE_REGTYPE_TEMP,
			1,
			0,
			USE_REGTYPE_TEMP,
			0,
			0);
	pui32Buffer += 2;

	/* get the fbaddr offset per line
	 * (stride * bpp - 64 byte to compensate the advance on previous line)
	 * mov r3, #(ui32LineStride << 2) - 64)
	 */ 
	BuildLIMM((PPVR_USE_INST)pui32Buffer,
			EURASIA_USE1_EPRED_ALWAYS,
			0 /* ui32Flags */,
			USE_REGTYPE_TEMP,
			3,
			(ui32LineStride << 2) - 64);
	pui32Buffer += 2;

	/*
	 * per line loop counter (2 lines per loops) 
	 */
	/* mov  r1, #0*/
	BuildLIMM((PPVR_USE_INST)pui32Buffer,
			EURASIA_USE1_EPRED_ALWAYS,
			0 /* ui32Flags */,
			USE_REGTYPE_TEMP,
			1,
			0);
	pui32Buffer += 2;
	
	/* { loop */
	pui32LoopBase = pui32Buffer;

	/* and r4, r1, #1*/
	*pui32Buffer++ =
		(IMG_UINT32)((0x81 << 0) | (0x00 << 8) | (0x80 << 16) | (0x20 << 24));
	*pui32Buffer++ =
		(IMG_UINT32)((0x00 << 0) | (0x00 << 8) | (0x81 << 16) | (0x50 << 24));

	/* shl r2, r1, #4*/
	*pui32Buffer++ =
		(IMG_UINT32)((0x84 << 0) | (0x00 << 8) | (0x40 << 16) | (0x20 << 24));
	*pui32Buffer++ =
		(IMG_UINT32)((0x00 << 0) | (0x00 << 8) | (0x81 << 16) | (0x60 << 24));

	/* and r2, r2, #30*/
	*pui32Buffer++ =
		(IMG_UINT32)((0x60 << 0) | (0x01 << 8) | (0x40 << 16) | (0x20 << 24));
	*pui32Buffer++ =
		(IMG_UINT32)((0x00 << 0) | (0x00 << 8) | (0x81 << 16) | (0x50 << 24));

	/* or  r2, r4, r2*/
	*pui32Buffer++ =
		(IMG_UINT32)((0x02 << 0) | (0x02 << 8) | (0x40 << 16) | (0x00 << 24));
	*pui32Buffer++ =
		(IMG_UINT32)((0x08 << 0) | (0x00 << 8) | (0x80 << 16) | (0x50 << 24));

	/* mov.nosched i.l, r2*/
	*pui32Buffer++ =
		(IMG_UINT32)((0x00 << 0) | (0x01 << 8) | (0x20 << 16) | (0x20 << 24));
	*pui32Buffer++ =
		(IMG_UINT32)((0x0A << 0) | (0x00 << 8) | (0xC9 << 16) | (0x50 << 24));

	/* even line - 4 samples at a time*/
	for (i = 0; i < 4; i++) 
	{
		IMG_UINT32 j;

		if (i != 0)
		{
			/* advance the index */
			/* imae r4, r2, #1, #((i & 1) << 4 | (i & 2) << 2)*/
			BuildIMAE((PPVR_USE_INST)pui32Buffer,
					EURASIA_USE1_EPRED_ALWAYS,
					0,
					(EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT),
					USE_REGTYPE_TEMP,
					4,
					USE_REGTYPE_TEMP,
					2,
					0,
					USE_REGTYPE_IMMEDIATE,
					1,
					0,
					USE_REGTYPE_IMMEDIATE,
					((i & 1) << 4 | (i & 2) << 2),
					0);
			pui32Buffer += 2;

			/* mov.nosched i.l, r4*/
			*pui32Buffer++ =
				(IMG_UINT32)((0x00 << 0) | (0x02 << 8) | (0x20 << 16) | (0x20 << 24));
			*pui32Buffer++ =
				(IMG_UINT32)((0x0A << 0) | (0x00 << 8) | (0xC9 << 16) | (0x50 << 24));
		}

		for (j = 0; j < 2; j++)
		{
			/* vmov.flt.nosched i0.xyzw, o[i.l + #(j * 4 + 2)], swizzle(xxxx)*/
			*pui32Buffer++ =
				(IMG_UINT32)(((j * 2 + 1) << 7) | (0x04 << 8) | (0xF0 << 16) | (0x0F << 24));
			*pui32Buffer++ =
				(IMG_UINT32)((0x00 << 0) | (0x0D << 8) | (0x82 << 16) | (0x38 << 24));

			/* vmov.flt i0.x, o[i.l + #(j * 4)], swizzle(xxxx)*/
			*pui32Buffer++ =
				(IMG_UINT32)(((j * 2) << 7) | (0x04 << 8) | (0xF0 << 16) | (0x01 << 24));
			*pui32Buffer++ =
				(IMG_UINT32)((0x00 << 0) | (0x05 << 8) | (0x82 << 16) | (0x38 << 24));

			if (j == 1)
			{
				/* staq.f1 [r0, #0++], i0 */
				*pui32Buffer++ =
					(IMG_UINT32)((0x7C << 0) | (0x00 << 8) | (0x00 << 16) | (0x80 << 24));
				*pui32Buffer++ =
					(IMG_UINT32)((0x30 << 0) | (0x02 << 8) | (0x82 << 16) | (0xF0 << 24));
			}
			else
			{
				/* staq.f1.nosched [r0, #0++], i0 */
				*pui32Buffer++ =
					(IMG_UINT32)((0x7C << 0) | (0x00 << 8) | (0x00 << 16) | (0x80 << 24));
				*pui32Buffer++ =
					(IMG_UINT32)((0x30 << 0) | (0x02 << 8) | (0xC2 << 16) | (0xF0 << 24));
			}
		}
	}

	/* mov i.l, r2 - reload the index as odd lines use the same index
	 * but they select the high 32 bit*/
	*pui32Buffer++ =
		(IMG_UINT32)((0x00 << 0) | (0x01 << 8) | (0x20 << 16) | (0x20 << 24));
	*pui32Buffer++ =
		(IMG_UINT32)((0x0A << 0) | (0x00 << 8) | (0x89 << 16) | (0x50 << 24));

	/* add on a stride*/
	/* imae.nosched r0, r3.low, #1, r0, u32*/
	*pui32Buffer++ =
		(IMG_UINT32)((0x80 << 0) | (0xC0 << 8) | (0x00 << 16) | (0x80 << 24));
	*pui32Buffer++ =
		(IMG_UINT32)((0x80 << 0) | (0x00 << 8) | (0xC2 << 16) | (0xA8 << 24));

	/* odd line - 4 samples at a time*/
	for (i = 0; i < 4; i++) 
	{
		IMG_UINT32 j;

		if (i != 0)
		{
			/* advance the index */ 
			/* imae r4, r2, #1, #((i & 1) << 4 | (i & 2) << 2)*/
			BuildIMAE((PPVR_USE_INST)pui32Buffer,
					EURASIA_USE1_EPRED_ALWAYS,
					0,
					(EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT),
					USE_REGTYPE_TEMP,
					4,
					USE_REGTYPE_TEMP,
					2,
					0,
					USE_REGTYPE_IMMEDIATE,
					1,
					0,
					USE_REGTYPE_IMMEDIATE,
					((i & 1) << 4 | (i & 2) << 2),
					0);
			pui32Buffer += 2;

			/* mov.nosched i.l, r4*/
			*pui32Buffer++ =
				(IMG_UINT32)((0x00 << 0) | (0x02 << 8) | (0x20 << 16) | (0x20 << 24));
			*pui32Buffer++ =
				(IMG_UINT32)((0x0A << 0) | (0x00 << 8) | (0xC9 << 16) | (0x50 << 24));
		}

		for (j = 0; j < 2; j++)
		{
			/* vmov.flt.nosched i0.xyzw, o[i.l + #(j * 4 + 2)], swizzle(yyyy)*/
			*pui32Buffer++ =
				(IMG_UINT32)(((j * 2 + 1) << 7) | (0x04 << 8) | (0xF0 << 16) | (0x0F << 24));
			*pui32Buffer++ =
				(IMG_UINT32)((0x08 << 0) | (0x0D << 8) | (0x82 << 16) | (0x38 << 24));

			/* vmov.flt i0.x, o[i.l + #(j * 4)], swizzle(yyyy)*/
			*pui32Buffer++ =
				(IMG_UINT32)(((j * 2) << 7) | (0x04 << 8) | (0xF0 << 16) | (0x01 << 24));
			*pui32Buffer++ =
				(IMG_UINT32)((0x08 << 0) | (0x05 << 8) | (0x82 << 16) | (0x38 << 24));

			if (j == 1)
			{
				/* staq.f1 [r0, #0++], i0 */
				*pui32Buffer++ =
					(IMG_UINT32)((0x7C << 0) | (0x00 << 8) | (0x00 << 16) | (0x80 << 24));
				*pui32Buffer++ =
					(IMG_UINT32)((0x30 << 0) | (0x02 << 8) | (0x82 << 16) | (0xF0 << 24));
			}
			else
			{
				/* staq.f1.nosched [r0, #0++], i0 */
				*pui32Buffer++ =
					(IMG_UINT32)((0x7C << 0) | (0x00 << 8) | (0x00 << 16) | (0x80 << 24));
				*pui32Buffer++ =
					(IMG_UINT32)((0x30 << 0) | (0x02 << 8) | (0xC2 << 16) | (0xF0 << 24));
			}
		}
	}

	/* add on a stride */
	/* imae r0, r3.low, #1, r0, u32*/
	*pui32Buffer++ =
		(IMG_UINT32)((0x80 << 0) | (0xC0 << 8) | (0x00 << 16) | (0x80 << 24));
	*pui32Buffer++ =
		(IMG_UINT32)((0x80 << 0) | (0x00 << 8) | (0x82 << 16) | (0xA8 << 24));

	/* imae r1, r1.low, #1, #1, u32*/
	BuildIMAE((PPVR_USE_INST)pui32Buffer,
			EURASIA_USE1_EPRED_ALWAYS,
			0,
			(EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT),
			USE_REGTYPE_TEMP,
			1,
			USE_REGTYPE_TEMP,
			1,
			0,
			USE_REGTYPE_IMMEDIATE,
			1,
			0,
			USE_REGTYPE_IMMEDIATE,
			1,
			0);
	pui32Buffer += 2;

	/* xor.testnz p0, r1, #8*/
	*pui32Buffer++ =
		(IMG_UINT32)((0x88 << 0) | (0x80 << 8) | (0x0C << 16) | (0x20 << 24));
	*pui32Buffer++ =
		(IMG_UINT32)((0x81 << 0) | (0x02 << 8) | (0x89 << 16) | (0x48 << 24));

	/* p0 br loop */
	BuildBR((PPVR_USE_INST)pui32Buffer,
			EURASIA_USE1_EPRED_P0,
			(pui32LoopBase - pui32Buffer) / 2,
			0);
	pui32Buffer += 2;

	/* idf drc0, st*/
	*pui32Buffer++ =
		(IMG_UINT32)((0x00 << 0) | (0x00 << 8) | (0x00 << 16) | (0x00 << 24));
	*pui32Buffer++ =
		(IMG_UINT32)((0x00 << 0) | (0x00 << 8) | (0xA0 << 16) | (0xF8 << 24));

	/* wdf drc0*/
	*pui32Buffer++ =
		(IMG_UINT32)((0x00 << 0) | (0x00 << 8) | (0x00 << 16) | (0x00 << 24));
	*pui32Buffer++ =
		(IMG_UINT32)((0x00 << 0) | (0x00 << 8) | (0x20 << 16) | (0xF9 << 24));

	/* no pixels sent to the PBE */
	*pui32EmitWords	&= EURASIA_PIXELBES0LO_COUNT_CLRMSK;

	for (i = 0; i < STATE_PBE_DWORDS; i++)
	{
		/* LIMM r[i], emitword[i] */
		BuildLIMM((PPVR_USE_INST)pui32Buffer,
					EURASIA_USE1_EPRED_ALWAYS,
					0 /* ui32Flags */,
					USE_REGTYPE_TEMP,
					i,
					pui32EmitWords[i]);
		
		pui32Buffer += 2;
	}

	/*
		Set up the instruction which will do the tile emit.
	*/
	*pui32Buffer++ = ((EURASIA_USE0_S1STDBANK_TEMP << EURASIA_USE0_S1BANK_SHIFT) |
					((ui32SideBand << EURASIA_USE0_EMIT_SIDEBAND_197_192_SHIFT) & ~EURASIA_USE0_EMIT_SIDEBAND_197_192_CLRMSK) |
					EURASIA_USE0_EMIT_FREEP |
					(2 << EURASIA_USE0_SRC2_SHIFT) |
					(1 << EURASIA_USE0_SRC1_SHIFT) |
					(0 << EURASIA_USE0_SRC0_SHIFT));

	*pui32Buffer++ = ((EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
					(EURASIA_USE1_OTHER_OP2_EMIT << EURASIA_USE1_OTHER_OP2_SHIFT) |
					(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
					(EURASIA_USE1_EMIT_TARGET_PIXELBE << EURASIA_USE1_EMIT_TARGET_SHIFT) |
					(((ui32SideBand >> 12) << EURASIA_USE1_EMIT_SIDEBAND_205_204_SHIFT) & ~EURASIA_USE1_EMIT_SIDEBAND_205_204_CLRMSK) |
					(((ui32SideBand >> 6) << EURASIA_USE1_EMIT_SIDEBAND_203_198_SHIFT) & ~EURASIA_USE1_EMIT_SIDEBAND_203_198_CLRMSK) |
					(EURASIA_USE1_S0STDBANK_TEMP << EURASIA_USE1_S0BANK_SHIFT) |
					(0 << EURASIA_USE1_EMIT_INCP_SHIFT));

	/* Terminate this block of code (sneaky negative array index) */
	pui32Buffer[-1] |= EURASIA_USE1_END;

	return pui32Buffer;
}

#endif /* FIX_HW_BRN_31982 */

#if !defined(__psp2__)
IMG_INTERNAL IMG_UINT32 * WriteEndOfTileUSSECode(IMG_UINT32 *pui32BufferBase,
												 IMG_UINT32 *pui32EmitWords, 
												 IMG_UINT32 ui32SideBand)
{
	IMG_UINT32 *pui32Buffer;
	IMG_UINT32 i;
	
	pui32Buffer = pui32BufferBase;

	/* 
	   phas #0, #0, pixel, end, parallel: Insert Single Phas instruction 
	*/		
	BuildPHASLastPhase((PPVR_USE_INST)pui32Buffer, 0 /* ui32End */);
	pui32Buffer += 2;
	
	for(i=0; i < STATE_PBE_DWORDS; i++)
	{
		/* LIMM r[i], emitword[i] */
		BuildLIMM((PPVR_USE_INST)pui32Buffer,
					EURASIA_USE1_EPRED_ALWAYS,
					0 /* ui32Flags */,
					USE_REGTYPE_TEMP,
					i,
					pui32EmitWords[i]);
		
		pui32Buffer += 2;
	}

	/*
		Set up the instruction which will do the tile emit.
	*/

	*pui32Buffer++ = ((EURASIA_USE0_S1STDBANK_TEMP << EURASIA_USE0_S1BANK_SHIFT) |
					((ui32SideBand << EURASIA_USE0_EMIT_SIDEBAND_197_192_SHIFT) & ~EURASIA_USE0_EMIT_SIDEBAND_197_192_CLRMSK) |
					EURASIA_USE0_EMIT_FREEP |
					(2 << EURASIA_USE0_SRC2_SHIFT) |
					(1 << EURASIA_USE0_SRC1_SHIFT) |
					(0 << EURASIA_USE0_SRC0_SHIFT));

	*pui32Buffer++ = ((EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
					(EURASIA_USE1_OTHER_OP2_EMIT << EURASIA_USE1_OTHER_OP2_SHIFT) |
					(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
					(EURASIA_USE1_EMIT_TARGET_PIXELBE << EURASIA_USE1_EMIT_TARGET_SHIFT) |
					(((ui32SideBand >> 12) << EURASIA_USE1_EMIT_SIDEBAND_205_204_SHIFT) & ~EURASIA_USE1_EMIT_SIDEBAND_205_204_CLRMSK) |
					(((ui32SideBand >> 6) << EURASIA_USE1_EMIT_SIDEBAND_203_198_SHIFT) & ~EURASIA_USE1_EMIT_SIDEBAND_203_198_CLRMSK) |
					(EURASIA_USE1_S0STDBANK_TEMP << EURASIA_USE1_S0BANK_SHIFT) |
					(0 << EURASIA_USE1_EMIT_INCP_SHIFT));

	/* Terminate this block of code (sneaky negative array index) */
	pui32Buffer[-1] |= EURASIA_USE1_END;

	return pui32Buffer;
}
#endif

#else /* SGX_FEATURE_UNIFIED_STORE_64BITS */

IMG_INTERNAL IMG_UINT32 * WriteEndOfTileUSSECode(IMG_UINT32 *pui32BufferBase,
												 IMG_UINT32 *pui32EmitWords, 
												 IMG_UINT32 ui32SideBand
												 #if defined(SGX_FEATURE_RENDER_TARGET_ARRAYS)
												 ,IMG_PUINT32 *ppui32StateWordLimmOffset
												 #endif
												 #if defined(SGX_FEATURE_WRITEBACK_DCU)
												 ,IMG_BOOL bFlushCache
												 #endif
												 )
{
	IMG_UINT32 *pui32Buffer;
	IMG_UINT32 i, ui32EmitFlag;
	
	pui32Buffer = pui32BufferBase;

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES) 
	/* 
	   phas #0, #0, pixel, end, parallel: Insert Single Phas instruction 
	*/		
	BuildPHASLastPhase((PPVR_USE_INST)pui32Buffer, 0 /* ui32End */);
	pui32Buffer += 2;
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

	#if defined(SGX_FEATURE_WRITEBACK_DCU)
	if(bFlushCache)
	{
		/*
			If this is the last tile in the render then flush the cache..
		*/
		BuildALUTST((PPVR_USE_INST)pui32Buffer,
					EURASIA_USE1_EPRED_ALWAYS,
					1,
					EURASIA_USE1_TEST_CRCOMB_AND,
					USE_REGTYPE_TEMP,
					0, // r0
					0,
					USE_REGTYPE_PRIMATTR,
					0, // pa0
					USE_REGTYPE_IMMEDIATE,
					EURASIA_PDS_IR1_PIX_EVENT_ENDRENDER,
					0,
					EURASIA_USE1_TEST_STST_NONE,
					EURASIA_USE1_TEST_ZTST_ZERO,
					EURASIA_USE1_TEST_CHANCC_SELECT0,
					EURASIA_USE0_TEST_ALUSEL_BITWISE,
					EURASIA_USE0_TEST_ALUOP_BITWISE_AND);
		pui32Buffer+=2;
		
		BuildBR((PPVR_USE_INST)pui32Buffer, EURASIA_USE1_EPRED_P0, 2, 0);
		pui32Buffer+=2;
		
		BuildCFI((PPVR_USE_INST)pui32Buffer,
			   SGX545_USE1_OTHER2_CFI_GLOBAL | SGX545_USE1_OTHER2_CFI_FLUSH | SGX545_USE1_OTHER2_CFI_INVALIDATE,
			   0, // mode?
			   USE_REGTYPE_IMMEDIATE,
			   SGX545_USE_OTHER2_CFI_DATAMASTER_PIXEL, // data master
			   USE_REGTYPE_IMMEDIATE,
			   0, // address (don't care, we're doing a global flush)
			   SGX545_USE1_OTHER2_CFI_LEVEL_MAX);
		pui32Buffer+=2;
	}
	#endif


#if (defined(FIX_HW_BRN_23194) || defined(FIX_HW_BRN_23460) || defined(FIX_HW_BRN_23949)) 

#if !defined(SUPPORT_SGX_COMPLEX_GEOMETRY)
	#define	USE_PIXELEVENT_GLOBAL_SYNC_REGISTER		EURASIA_USE_SPECIAL_BANK_G0
#else
	#define	USE_PIXELEVENT_GLOBAL_SYNC_REGISTER		EURASIA_USE_SPECIAL_BANK_G1

	/* 
		ldr.skipinv 		r0, #EUR_CR_USE_G1 >> 2, drc  
	*/
	*pui32Buffer++ = (EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT) |
					  (0 << EURASIA_USE0_DST_SHIFT) |
					  (((EUR_CR_USE_G1 >> (2 + EURASIA_USE0_LDRSTR_SRC2EXT_INTERNALSHIFT)) << EURASIA_USE0_LDRSTR_SRC2EXT_SHIFT) & ~EURASIA_USE0_LDRSTR_SRC2EXT_CLRMSK) | 
					  (((EUR_CR_USE_G1 >> 2) << EURASIA_USE0_SRC2_SHIFT) & ~EURASIA_USE0_SRC2_CLRMSK);

	*pui32Buffer++ = ((EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) | 
					  (EURASIA_USE1_OTHER_OP2_LDRSTR << EURASIA_USE1_OTHER_OP2_SHIFT) |
					  EURASIA_USE1_SKIPINV |
					  (EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) | 
					  EURASIA_USE1_LDRSTR_DSEL_LOAD |
					  EURASIA_USE1_S2BEXT |
					  EURASIA_USE1_LDST_DBANK_TEMP |
					  (0 << EURASIA_USE1_LDST_DRCSEL_SHIFT));

	
	/* 
		wdf				drc0
	 */
	*pui32Buffer++ = 0x00000000;
	
	*pui32Buffer++ = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
					 (EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
					 (EURASIA_USE1_OTHER_OP2_WDF << EURASIA_USE1_OTHER_OP2_SHIFT) |
					 (0 << EURASIA_USE1_IDFWDF_DRCSEL_SHIFT);
#endif

	/*
		xor.testnz.skipinv !r0, p0, g0 (g1 for SUPPORT_SGX_COMPLEX_GEOMETRY), g17
	*/

	*pui32Buffer++ = (EURASIA_USE0_S1EXTBANK_SPECIAL << EURASIA_USE0_S1BANK_SHIFT) |
					(EURASIA_USE0_S2EXTBANK_SPECIAL << EURASIA_USE0_S2BANK_SHIFT) | 
					(0 << EURASIA_USE0_DST_SHIFT) |
					(EURASIA_USE0_TEST_ALUSEL_BITWISE << EURASIA_USE0_TEST_ALUSEL_SHIFT) |
					(EURASIA_USE0_TEST_ALUOP_BITWISE_XOR << EURASIA_USE0_TEST_ALUOP_SHIFT) |
					((USE_PIXELEVENT_GLOBAL_SYNC_REGISTER | EURASIA_USE_SPECIAL_INTERNALDATA) << EURASIA_USE0_SRC1_SHIFT) |
					((EURASIA_USE_SPECIAL_BANK_PIPENUMBER | EURASIA_USE_SPECIAL_INTERNALDATA) << EURASIA_USE0_SRC2_SHIFT);

	*pui32Buffer++ = (EURASIA_USE1_OP_TEST << EURASIA_USE1_OP_SHIFT) |
					EURASIA_USE1_SKIPINV |
					EURASIA_USE1_S1BEXT |
					EURASIA_USE1_S2BEXT | 
					(1 << EURASIA_USE1_TEST_PREDMASK_SHIFT) |
					(EURASIA_USE1_TEST_ZTST_NOTZERO << EURASIA_USE1_TEST_ZTST_SHIFT) |
					(EURASIA_USE1_TEST_CHANCC_SELECT0 << EURASIA_USE1_TEST_CHANCC_SHIFT) |
					EURASIA_USE1_TEST_CRCOMB_AND |
					(0 << EURASIA_USE1_TEST_PDST_SHIFT) | 
					(EURASIA_USE1_D1STDBANK_TEMP << EURASIA_USE1_D1BANK_SHIFT);

	/*
		p0 br -#0x00000001
	*/

	*pui32Buffer++ = ((-1 << EURASIA_USE0_BRANCH_OFFSET_SHIFT) & ~EURASIA_USE0_BRANCH_OFFSET_CLRMSK);

	*pui32Buffer++ = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
						(EURASIA_USE1_EPRED_P0 << EURASIA_USE1_EPRED_SHIFT) |
						(EURASIA_USE1_SPECIAL_OPCAT_FLOWCTRL << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
						(EURASIA_USE1_FLOWCTRL_OP2_BR << EURASIA_USE1_FLOWCTRL_OP2_SHIFT);

#endif /* (defined(FIX_HW_BRN_23194) || defined(FIX_HW_BRN_23460) || defined(FIX_HW_BRN_23949)) */
	
#if defined(SGX_FEATURE_RENDER_TARGET_ARRAYS)
	/*
		Recored where the start of the state word LIMM instructions is so that
		the driver can tell the uKernel where to put the updated frame buffer
		address when rendering to an array of render targets.
	*/
	if(ppui32StateWordLimmOffset != IMG_NULL)
	{
		*ppui32StateWordLimmOffset = pui32Buffer;
	}
#endif
	
	for(i=0; i < STATE_PBE_DWORDS; i++)
	{
		/* LIMM r[i], emitword[i] */
		*pui32Buffer++ = (i << EURASIA_USE0_DST_SHIFT) |
			(((pui32EmitWords[i] >> 0) << EURASIA_USE0_LIMM_IMML21_SHIFT) & (~EURASIA_USE0_LIMM_IMML21_CLRMSK));

		*pui32Buffer++ = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
							(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
							(EURASIA_USE1_OTHER_OP2_LIMM << EURASIA_USE1_OTHER_OP2_SHIFT) |
							(EURASIA_USE1_EPRED_ALWAYS << EURASIA_USE1_LIMM_EPRED_SHIFT) |
							(EURASIA_USE1_D1STDBANK_TEMP << EURASIA_USE1_D1BANK_SHIFT) |
							(((pui32EmitWords[i] >> 21) << EURASIA_USE1_LIMM_IMM2521_SHIFT) & (~EURASIA_USE1_LIMM_IMM2521_CLRMSK)) |
							(((pui32EmitWords[i] >> 26) << EURASIA_USE1_LIMM_IMM3126_SHIFT) & (~EURASIA_USE1_LIMM_IMM3126_CLRMSK)) |
							EURASIA_USE1_SKIPINV;
	}

	/*
		Set up the instruction which will do the first tile emit.
	*/
	ui32EmitFlag = EURASIA_PIXELBE1SB_TWOEMITS;
	*pui32Buffer++ = ((EURASIA_USE0_S1STDBANK_TEMP << EURASIA_USE0_S1BANK_SHIFT) |
					(ui32EmitFlag << EURASIA_USE0_EMIT_SIDEBAND_101_96_SHIFT) | 
#if !defined(FIX_HW_BRN_23460) && !defined(FIX_HW_BRN_23949)
					EURASIA_USE0_EMIT_FREEP |
#endif
					(0 << EURASIA_USE0_SRC0_SHIFT) |
					(1 << EURASIA_USE0_SRC1_SHIFT));

	*pui32Buffer++ = ((EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
					(EURASIA_USE1_OTHER_OP2_EMIT << EURASIA_USE1_OTHER_OP2_SHIFT) |
					(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
					(EURASIA_USE1_EMIT_TARGET_PIXELBE << EURASIA_USE1_EMIT_TARGET_SHIFT) |
					(((ui32EmitFlag >> 12) << EURASIA_USE1_EMIT_SIDEBAND_109_108_SHIFT) & ~EURASIA_USE1_EMIT_SIDEBAND_109_108_CLRMSK) |
					(((ui32EmitFlag >> 6) << EURASIA_USE1_EMIT_SIDEBAND_107_102_SHIFT) & ~EURASIA_USE1_EMIT_SIDEBAND_107_102_CLRMSK) |
					(EURASIA_USE1_S0STDBANK_TEMP << EURASIA_USE1_S0BANK_SHIFT) |
					(0 << EURASIA_USE1_EMIT_INCP_SHIFT));	

	/*
		Set up the instruction which will do the second tile emit.
	*/
	*pui32Buffer++ = ((EURASIA_USE0_S1STDBANK_TEMP << EURASIA_USE0_S1BANK_SHIFT) |
					((ui32SideBand << EURASIA_USE0_EMIT_SIDEBAND_101_96_SHIFT) & ~EURASIA_USE0_EMIT_SIDEBAND_101_96_CLRMSK) |
#if !defined(FIX_HW_BRN_23460) && !defined(FIX_HW_BRN_23949)
					EURASIA_USE0_EMIT_FREEP |
#endif
#if defined(SGX_FEATURE_PIXELBE_32K_LINESTRIDE)
					(4 << EURASIA_USE0_SRC2_SHIFT) |
#endif
					(3 << EURASIA_USE0_SRC1_SHIFT) |
					(2 << EURASIA_USE0_SRC0_SHIFT));

	*pui32Buffer++ = ((EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
					(EURASIA_USE1_OTHER_OP2_EMIT << EURASIA_USE1_OTHER_OP2_SHIFT) |
					(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
					(EURASIA_USE1_EMIT_TARGET_PIXELBE << EURASIA_USE1_EMIT_TARGET_SHIFT) |
					(((ui32SideBand >> 12) << EURASIA_USE1_EMIT_SIDEBAND_109_108_SHIFT) & ~EURASIA_USE1_EMIT_SIDEBAND_109_108_CLRMSK) |
					(((ui32SideBand >> 6) << EURASIA_USE1_EMIT_SIDEBAND_107_102_SHIFT) & ~EURASIA_USE1_EMIT_SIDEBAND_107_102_CLRMSK) |
					(EURASIA_USE1_S0STDBANK_TEMP << EURASIA_USE1_S0BANK_SHIFT) |
					(0 << EURASIA_USE1_EMIT_INCP_SHIFT));

#if defined(FIX_HW_BRN_23460) || defined(FIX_HW_BRN_23949)
	/*
		mov tmp5, #0
	*/
	*pui32Buffer++ = ((EURASIA_USE0_S1EXTBANK_SPECIAL << EURASIA_USE0_S1BANK_SHIFT)	|
					(EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT)			|
					(STATE_PBE_DWORDS << EURASIA_USE0_DST_SHIFT)			|
					(EURASIA_USE_SPECIAL_CONSTANT_ZERO1 << EURASIA_USE0_SRC1_SHIFT));
	
	*pui32Buffer++ = ((EURASIA_USE1_OP_MOVC << EURASIA_USE1_OP_SHIFT) |
					(EURASIA_USE1_MOVC_TSTDTYPE_UNCOND << EURASIA_USE1_MOVC_TSTDTYPE_SHIFT) |
					EURASIA_USE1_RCNTSEL |
					EURASIA_USE1_S2BEXT  |
					EURASIA_USE1_S1BEXT  |
					(EURASIA_USE1_D1STDBANK_TEMP << EURASIA_USE1_D1BANK_SHIFT));

	/*
		Set up the instruction which will do the dummy tile emit.
	*/
	*pui32Buffer++ = ((EURASIA_USE0_S1STDBANK_TEMP << EURASIA_USE0_S1BANK_SHIFT) |
					EURASIA_USE0_EMIT_FREEP |
					(STATE_PBE_DWORDS << EURASIA_USE0_SRC0_SHIFT) |
					(0 << EURASIA_USE0_SRC1_SHIFT));

	*pui32Buffer++ = ((EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
					(EURASIA_USE1_OTHER_OP2_EMIT << EURASIA_USE1_OTHER_OP2_SHIFT) |
					(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
					(EURASIA_USE1_EMIT_TARGET_PIXELBE << EURASIA_USE1_EMIT_TARGET_SHIFT) |
					(EURASIA_USE1_S0STDBANK_TEMP << EURASIA_USE1_S0BANK_SHIFT) |
					(0 << EURASIA_USE1_EMIT_INCP_SHIFT));	

#endif /* defined(FIX_HW_BRN_23460) || defined(FIX_HW_BRN_23949) */

#if (defined(FIX_HW_BRN_23194) || defined(FIX_HW_BRN_23460) || defined(FIX_HW_BRN_23949))

#if !defined(SUPPORT_SGX_COMPLEX_GEOMETRY)
	/*
		xor.end.skipinv g0, g0, #0x00000001
	*/
	*pui32Buffer++ = (EURASIA_USE0_S1EXTBANK_SPECIAL << EURASIA_USE0_S1BANK_SHIFT) |
					(EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT) |
					((EURASIA_USE_SPECIAL_BANK_G0 | EURASIA_USE_SPECIAL_INTERNALDATA) << EURASIA_USE0_DST_SHIFT) |
					((EURASIA_USE_SPECIAL_BANK_G0 | EURASIA_USE_SPECIAL_INTERNALDATA) << EURASIA_USE0_SRC1_SHIFT) |
					(1 << EURASIA_USE0_SRC2_SHIFT);

	*pui32Buffer++ = (EURASIA_USE1_OP_XOR << EURASIA_USE1_OP_SHIFT) |
						EURASIA_USE1_SKIPINV |
						EURASIA_USE1_DBEXT |
						EURASIA_USE1_S1BEXT |
						EURASIA_USE1_S2BEXT | 
						(1 << EURASIA_USE1_RMSKCNT_SHIFT) |
						(EURASIA_USE1_D1EXTBANK_SPECIAL << EURASIA_USE1_D1BANK_SHIFT);

#else
	/*
		xor.skipinv r0, g1, #0x00000001
	*/
	*pui32Buffer++ = (EURASIA_USE0_S1EXTBANK_SPECIAL << EURASIA_USE0_S1BANK_SHIFT) |
					 (EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT) |
					 (0 << EURASIA_USE0_DST_SHIFT) |
					 ((EURASIA_USE_SPECIAL_BANK_G1 | EURASIA_USE_SPECIAL_INTERNALDATA) << EURASIA_USE0_SRC1_SHIFT) |
					 (1<< EURASIA_USE0_SRC2_SHIFT);

	*pui32Buffer++ = (EURASIA_USE1_OP_XOR << EURASIA_USE1_OP_SHIFT) |
					  EURASIA_USE1_SKIPINV |
					  EURASIA_USE1_S1BEXT |
					  EURASIA_USE1_S2BEXT | 
					  (1 << EURASIA_USE1_RMSKCNT_SHIFT) |
					  (EURASIA_USE1_D1STDBANK_TEMP << EURASIA_USE1_D1BANK_SHIFT);

	/* str.skipinv 		#EUR_CR_USE_G1 >> 2, r0  */
	*pui32Buffer++ = (EURASIA_USE0_S1STDBANK_TEMP << EURASIA_USE0_S1BANK_SHIFT) |
					  (EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT) |
					  (0 << EURASIA_USE0_SRC1_SHIFT) |
   					  (((EUR_CR_USE_G1 >> (2 + EURASIA_USE0_LDRSTR_SRC2EXT_INTERNALSHIFT)) << EURASIA_USE0_LDRSTR_SRC2EXT_SHIFT) & ~EURASIA_USE0_LDRSTR_SRC2EXT_CLRMSK) | 
					  (((EUR_CR_USE_G1 >> 2) << EURASIA_USE0_SRC2_SHIFT) & ~EURASIA_USE0_SRC2_CLRMSK);


	*pui32Buffer++ = ((EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) | 
					  (EURASIA_USE1_OTHER_OP2_LDRSTR << EURASIA_USE1_OTHER_OP2_SHIFT) |
					  EURASIA_USE1_SKIPINV |
					  (EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) | 
					  EURASIA_USE1_LDRSTR_DSEL_STORE |
					  EURASIA_USE1_S2BEXT);

	/* 
		mov.skipinv		r0, r0
		(Needed because STR cannot have .end applied)
	*/

	*pui32Buffer++ = ((EURASIA_USE0_S1STDBANK_TEMP << EURASIA_USE0_S1BANK_SHIFT)		|
					  (EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT)	|
					  (0 << EURASIA_USE0_DST_SHIFT)			|
					  (0 << EURASIA_USE0_SRC1_SHIFT));
	
	*pui32Buffer++ = ((EURASIA_USE1_OP_MOVC << EURASIA_USE1_OP_SHIFT) |
					  (EURASIA_USE1_MOVC_TSTDTYPE_UNCOND << EURASIA_USE1_MOVC_TSTDTYPE_SHIFT) |
					  EURASIA_USE1_S2BEXT  |
					  EURASIA_USE1_SKIPINV |
					  (1 << EURASIA_USE1_RMSKCNT_SHIFT) |
					  (EURASIA_USE1_D1STDBANK_TEMP << EURASIA_USE1_D1BANK_SHIFT));

#endif /* !defined(SUPPORT_SGX_COMPLEX_GEOMETRY) */
	
#endif /* (defined(FIX_HW_BRN_23194) || defined(FIX_HW_BRN_23460) || defined(FIX_HW_BRN_23949)) */

	/* Terminate this block of code (sneaky negative array index) */
	pui32Buffer[-1] |= EURASIA_USE1_END;

	return pui32Buffer;
}

#endif /* SGX_FEATURE_UNIFIED_STORE_64BITS */


#if defined(FIX_HW_BRN_22249)

/***********************************************************************************
 Function Name      : WriteEndOfTileUSSECode2KStride
 Inputs             : pui32Buffer, pui32EmitWords, ui32SideBand
 Outputs            : pui32Buffer
 Returns            : pui32Buffer
 Description        : Writes end of tile USSE code when stride is 2K
************************************************************************************/
IMG_INTERNAL IMG_UINT32 * WriteEndOfTileUSSECode2KStride(IMG_UINT32 *pui32BufferBase,
						IMG_UINT32 *pui32EmitWords, 
						IMG_UINT32 ui32SideBand)
{
	IMG_UINT32 *pui32Buffer;
	IMG_UINT32 i, ui32EmitFlag, ui32Stride, ui32OffsetAdd;

	pui32Buffer = pui32BufferBase;

	/*
		xor.testnz.skipinv !r0, p0, g0, g17
	*/

	*pui32Buffer++ = (EURASIA_USE0_S1EXTBANK_SPECIAL << EURASIA_USE0_S1BANK_SHIFT) |
					(EURASIA_USE0_S2EXTBANK_SPECIAL << EURASIA_USE0_S2BANK_SHIFT) | 
					(0 << EURASIA_USE0_DST_SHIFT) |
					(EURASIA_USE0_TEST_ALUSEL_BITWISE << EURASIA_USE0_TEST_ALUSEL_SHIFT) |
					(EURASIA_USE0_TEST_ALUOP_BITWISE_XOR << EURASIA_USE0_TEST_ALUOP_SHIFT) |
					((EURASIA_USE_SPECIAL_BANK_G0 | EURASIA_USE_SPECIAL_INTERNALDATA) << EURASIA_USE0_SRC1_SHIFT) |
					((EURASIA_USE_SPECIAL_BANK_PIPENUMBER | EURASIA_USE_SPECIAL_INTERNALDATA) << EURASIA_USE0_SRC2_SHIFT);

	*pui32Buffer++ = (EURASIA_USE1_OP_TEST << EURASIA_USE1_OP_SHIFT) |
					EURASIA_USE1_SKIPINV |
					EURASIA_USE1_S1BEXT |
					EURASIA_USE1_S2BEXT | 
					(1 << EURASIA_USE1_TEST_PREDMASK_SHIFT) |
					(EURASIA_USE1_TEST_ZTST_NOTZERO << EURASIA_USE1_TEST_ZTST_SHIFT) |
					(EURASIA_USE1_TEST_CHANCC_SELECT0 << EURASIA_USE1_TEST_CHANCC_SHIFT) |
					EURASIA_USE1_TEST_CRCOMB_AND |
					(0 << EURASIA_USE1_TEST_PDST_SHIFT) | 
					(EURASIA_USE1_D1STDBANK_TEMP << EURASIA_USE1_D1BANK_SHIFT);

	/*
		p0 br -#0x00000001
	*/

	*pui32Buffer++ = ((-1 << EURASIA_USE0_BRANCH_OFFSET_SHIFT) & ~EURASIA_USE0_BRANCH_OFFSET_CLRMSK);

	*pui32Buffer++ = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
						(EURASIA_USE1_EPRED_P0 << EURASIA_USE1_EPRED_SHIFT) |
						(EURASIA_USE1_SPECIAL_OPCAT_FLOWCTRL << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
						(EURASIA_USE1_FLOWCTRL_OP2_BR << EURASIA_USE1_FLOWCTRL_OP2_SHIFT);


	for(i=0; i < 3; i++)
	{
		/* LIMM r[i], emitword[i] */
		*pui32Buffer++ = (i << EURASIA_USE0_DST_SHIFT) |
			(((pui32EmitWords[i] >> 0) << EURASIA_USE0_LIMM_IMML21_SHIFT) & (~EURASIA_USE0_LIMM_IMML21_CLRMSK));

		*pui32Buffer++ = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
							(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
							(EURASIA_USE1_OTHER_OP2_LIMM << EURASIA_USE1_OTHER_OP2_SHIFT) |
							(EURASIA_USE1_EPRED_ALWAYS << EURASIA_USE1_LIMM_EPRED_SHIFT) |
							(EURASIA_USE1_D1STDBANK_TEMP << EURASIA_USE1_D1BANK_SHIFT) |
							(((pui32EmitWords[i] >> 21) << EURASIA_USE1_LIMM_IMM2521_SHIFT) & (~EURASIA_USE1_LIMM_IMM2521_CLRMSK)) |
							(((pui32EmitWords[i] >> 26) << EURASIA_USE1_LIMM_IMM3126_SHIFT) & (~EURASIA_USE1_LIMM_IMM3126_CLRMSK)) |
							EURASIA_USE1_SKIPINV;
	}

	/* mov	r3, sa0 (tile base address, calculated in PDS) */
	*pui32Buffer++ = ((EURASIA_USE0_S1STDBANK_SECATTR << EURASIA_USE0_S1BANK_SHIFT)	|
					(EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT)			|
					(3 << EURASIA_USE0_DST_SHIFT)			|
					(0 << EURASIA_USE0_SRC1_SHIFT));
	
	*pui32Buffer++ = ((EURASIA_USE1_OP_MOVC << EURASIA_USE1_OP_SHIFT) |
					(EURASIA_USE1_MOVC_TSTDTYPE_UNCOND << EURASIA_USE1_MOVC_TSTDTYPE_SHIFT) |
					EURASIA_USE1_SKIPINV |
					EURASIA_USE1_RCNTSEL |
					EURASIA_USE1_S2BEXT  |
					(EURASIA_USE1_D1STDBANK_TEMP << EURASIA_USE1_D1BANK_SHIFT));

	/*
		mov r4, #0
	*/
	*pui32Buffer++ = ((EURASIA_USE0_S1EXTBANK_SPECIAL << EURASIA_USE0_S1BANK_SHIFT)	|
					(EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT)			|
					(4 << EURASIA_USE0_DST_SHIFT)			|
					(EURASIA_USE_SPECIAL_CONSTANT_ZERO1 << EURASIA_USE0_SRC1_SHIFT));
	
	*pui32Buffer++ = ((EURASIA_USE1_OP_MOVC << EURASIA_USE1_OP_SHIFT) |
					(EURASIA_USE1_MOVC_TSTDTYPE_UNCOND << EURASIA_USE1_MOVC_TSTDTYPE_SHIFT) |
					EURASIA_USE1_SKIPINV |
					EURASIA_USE1_RCNTSEL |
					EURASIA_USE1_S2BEXT  |
					EURASIA_USE1_S1BEXT  |
					(EURASIA_USE1_D1STDBANK_TEMP << EURASIA_USE1_D1BANK_SHIFT));

	/*
		mov r5, #4096 (2K stride * 2bpp)
	*/
	ui32Stride = 2048 * 2;

	*pui32Buffer++ = (5 << EURASIA_USE0_DST_SHIFT) |
		(((ui32Stride >> 0) << EURASIA_USE0_LIMM_IMML21_SHIFT) & (~EURASIA_USE0_LIMM_IMML21_CLRMSK));

	*pui32Buffer++ = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
						(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
						(EURASIA_USE1_OTHER_OP2_LIMM << EURASIA_USE1_OTHER_OP2_SHIFT) |
						(EURASIA_USE1_EPRED_ALWAYS << EURASIA_USE1_LIMM_EPRED_SHIFT) |
						(EURASIA_USE1_D1STDBANK_TEMP << EURASIA_USE1_D1BANK_SHIFT) |
						(((ui32Stride >> 21) << EURASIA_USE1_LIMM_IMM2521_SHIFT) & (~EURASIA_USE1_LIMM_IMM2521_CLRMSK)) |
						(((ui32Stride >> 26) << EURASIA_USE1_LIMM_IMM3126_SHIFT) & (~EURASIA_USE1_LIMM_IMM3126_CLRMSK)) |
						EURASIA_USE1_SKIPINV;

	/*
		Set up the instruction which will do the first tile emit.
		emitpix1 #0 , r0, r1, r0, sideband
		Line 0
	*/
	ui32EmitFlag = EURASIA_PIXELBE1SB_TWOEMITS;
	*pui32Buffer++ = ((EURASIA_USE0_S1STDBANK_TEMP << EURASIA_USE0_S1BANK_SHIFT) |
					(ui32EmitFlag << EURASIA_USE0_EMIT_SIDEBAND_101_96_SHIFT) | 
					(0 << EURASIA_USE0_SRC0_SHIFT) |
					(1 << EURASIA_USE0_SRC1_SHIFT));

	*pui32Buffer++ = ((EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
					(EURASIA_USE1_OTHER_OP2_EMIT << EURASIA_USE1_OTHER_OP2_SHIFT) |
					(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
					(EURASIA_USE1_EMIT_TARGET_PIXELBE << EURASIA_USE1_EMIT_TARGET_SHIFT) |
					(((ui32EmitFlag >> 12) << EURASIA_USE1_EMIT_SIDEBAND_109_108_SHIFT) & ~EURASIA_USE1_EMIT_SIDEBAND_109_108_CLRMSK) |
					(((ui32EmitFlag >> 6) << EURASIA_USE1_EMIT_SIDEBAND_107_102_SHIFT) & ~EURASIA_USE1_EMIT_SIDEBAND_107_102_CLRMSK) |
					(EURASIA_USE1_S0STDBANK_TEMP << EURASIA_USE1_S0BANK_SHIFT) |
					(0 << EURASIA_USE1_EMIT_INCP_SHIFT));	
	/*
		Set up the instruction which will do the second tile emit.
		emitpix2 #0 , r2, r3, r0, sideband
		Line 0
	*/
	*pui32Buffer++ = ((EURASIA_USE0_S1STDBANK_TEMP << EURASIA_USE0_S1BANK_SHIFT) |
					((ui32SideBand << EURASIA_USE0_EMIT_SIDEBAND_101_96_SHIFT) & ~EURASIA_USE0_EMIT_SIDEBAND_101_96_CLRMSK) |
					(2 << EURASIA_USE0_SRC0_SHIFT) |
					(3 << EURASIA_USE0_SRC1_SHIFT));

	*pui32Buffer++ = ((EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
					(EURASIA_USE1_OTHER_OP2_EMIT << EURASIA_USE1_OTHER_OP2_SHIFT) |
					(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
					(EURASIA_USE1_EMIT_TARGET_PIXELBE << EURASIA_USE1_EMIT_TARGET_SHIFT) |
					(((ui32SideBand >> 12) << EURASIA_USE1_EMIT_SIDEBAND_109_108_SHIFT) & ~EURASIA_USE1_EMIT_SIDEBAND_109_108_CLRMSK) |
					(((ui32SideBand >> 6) << EURASIA_USE1_EMIT_SIDEBAND_107_102_SHIFT) & ~EURASIA_USE1_EMIT_SIDEBAND_107_102_CLRMSK) |
					(EURASIA_USE1_S0STDBANK_TEMP << EURASIA_USE1_S0BANK_SHIFT) |
					(0 << EURASIA_USE1_EMIT_INCP_SHIFT));

	/*
		xor.skipinv g0, g0, #0x00000001
	*/
	*pui32Buffer++ = (EURASIA_USE0_S1EXTBANK_SPECIAL << EURASIA_USE0_S1BANK_SHIFT) |
					(EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT) |
					((EURASIA_USE_SPECIAL_BANK_G0| EURASIA_USE_SPECIAL_INTERNALDATA) << EURASIA_USE0_DST_SHIFT) |
					((EURASIA_USE_SPECIAL_BANK_G0 | EURASIA_USE_SPECIAL_INTERNALDATA) << EURASIA_USE0_SRC1_SHIFT) |
					(1 << EURASIA_USE0_SRC2_SHIFT);

	*pui32Buffer++ = (EURASIA_USE1_OP_XOR << EURASIA_USE1_OP_SHIFT) |
						EURASIA_USE1_SKIPINV |
						EURASIA_USE1_DBEXT |
						EURASIA_USE1_S1BEXT |
						EURASIA_USE1_S2BEXT | 
						(1 << EURASIA_USE1_RMSKCNT_SHIFT) |
						(EURASIA_USE1_D1EXTBANK_SPECIAL << EURASIA_USE1_D1BANK_SHIFT);

	/* Lines 1 to 15 */
	for(i=1; i < 16; i++)
	{
		ui32OffsetAdd = ((i%2) ? 2 : 14);
		
		/*
			iaddu32		r3, r5.low, r3 
			imae
		*/
		*pui32Buffer++ = (EURASIA_USE0_S1EXTBANK_IMMEDIATE << EURASIA_USE0_S1BANK_SHIFT) |
						(EURASIA_USE0_S2STDBANK_TEMP << EURASIA_USE0_S2BANK_SHIFT) |
						(3 << EURASIA_USE0_DST_SHIFT) |
						(5 << EURASIA_USE0_SRC0_SHIFT) |
						(1 << EURASIA_USE0_SRC1_SHIFT) |
						(3 << EURASIA_USE0_SRC2_SHIFT);

		*pui32Buffer++ = (EURASIA_USE1_OP_IMAE << EURASIA_USE1_OP_SHIFT) |
						EURASIA_USE1_IMAE_SRC0H_SELECTLOW |
						EURASIA_USE1_IMAE_SRC1H_SELECTLOW |
						(EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT) |
						EURASIA_USE1_SKIPINV |
						EURASIA_USE1_S1BEXT |
						(EURASIA_USE1_S0STDBANK_TEMP << EURASIA_USE1_S0BANK_SHIFT);

		/*
			iaddu32		r4, r4.low, #ui32OffsetAdd
			imae
		*/
		*pui32Buffer++ = (EURASIA_USE0_S1EXTBANK_IMMEDIATE << EURASIA_USE0_S1BANK_SHIFT) |
						(EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT) |
						(4 << EURASIA_USE0_DST_SHIFT) |
						(4 << EURASIA_USE0_SRC0_SHIFT) |
						(1 << EURASIA_USE0_SRC1_SHIFT) |
						(ui32OffsetAdd << EURASIA_USE0_SRC2_SHIFT);

		*pui32Buffer++ = (EURASIA_USE1_OP_IMAE << EURASIA_USE1_OP_SHIFT) |
						EURASIA_USE1_IMAE_SRC0H_SELECTLOW |
						EURASIA_USE1_IMAE_SRC1H_SELECTLOW |
						(EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT) |
						EURASIA_USE1_SKIPINV |
						EURASIA_USE1_S1BEXT |
						EURASIA_USE1_S2BEXT |
						(EURASIA_USE1_S0STDBANK_TEMP << EURASIA_USE1_S0BANK_SHIFT);
		/*
			or			r6, r2, r4
		*/
		*pui32Buffer++ = (EURASIA_USE0_S1STDBANK_TEMP << EURASIA_USE0_S1BANK_SHIFT) |
						(EURASIA_USE0_S2STDBANK_TEMP << EURASIA_USE0_S2BANK_SHIFT) |
						(6 << EURASIA_USE0_DST_SHIFT) |
						(2 << EURASIA_USE0_SRC1_SHIFT) |
						(4 << EURASIA_USE0_SRC2_SHIFT);

		*pui32Buffer++ = (EURASIA_USE1_OP_ANDOR << EURASIA_USE1_OP_SHIFT) |
						EURASIA_USE1_RCNTSEL |
						EURASIA_USE1_SKIPINV |
						(EURASIA_USE1_BITWISE_OP2_OR << EURASIA_USE1_BITWISE_OP2_SHIFT) |
						(EURASIA_USE1_S0STDBANK_TEMP << EURASIA_USE1_S0BANK_SHIFT);

		/*
			xor.testnz.skipinv !r0, p0, g0, g17
		*/

		*pui32Buffer++ = (EURASIA_USE0_S1EXTBANK_SPECIAL << EURASIA_USE0_S1BANK_SHIFT) |
						(EURASIA_USE0_S2EXTBANK_SPECIAL << EURASIA_USE0_S2BANK_SHIFT) | 
						(0 << EURASIA_USE0_DST_SHIFT) |
						(EURASIA_USE0_TEST_ALUSEL_BITWISE << EURASIA_USE0_TEST_ALUSEL_SHIFT) |
						(EURASIA_USE0_TEST_ALUOP_BITWISE_XOR << EURASIA_USE0_TEST_ALUOP_SHIFT) |
						((EURASIA_USE_SPECIAL_BANK_G0 | EURASIA_USE_SPECIAL_INTERNALDATA) << EURASIA_USE0_SRC1_SHIFT) |
						((EURASIA_USE_SPECIAL_BANK_PIPENUMBER | EURASIA_USE_SPECIAL_INTERNALDATA) << EURASIA_USE0_SRC2_SHIFT);

		*pui32Buffer++ = (EURASIA_USE1_OP_TEST << EURASIA_USE1_OP_SHIFT) |
						EURASIA_USE1_SKIPINV |
						EURASIA_USE1_S1BEXT |
						EURASIA_USE1_S2BEXT | 
						(1 << EURASIA_USE1_TEST_PREDMASK_SHIFT) |
						(EURASIA_USE1_TEST_ZTST_NOTZERO << EURASIA_USE1_TEST_ZTST_SHIFT) |
						(EURASIA_USE1_TEST_CHANCC_SELECT0 << EURASIA_USE1_TEST_CHANCC_SHIFT) |
						EURASIA_USE1_TEST_CRCOMB_AND |
						(0 << EURASIA_USE1_TEST_PDST_SHIFT) | 
						(EURASIA_USE1_D1STDBANK_TEMP << EURASIA_USE1_D1BANK_SHIFT);

		/*
			p0 br -#0x00000001
		*/

		*pui32Buffer++ = ((-1 << EURASIA_USE0_BRANCH_OFFSET_SHIFT) & ~EURASIA_USE0_BRANCH_OFFSET_CLRMSK);

		*pui32Buffer++ = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
							(EURASIA_USE1_EPRED_P0 << EURASIA_USE1_EPRED_SHIFT) |
							(EURASIA_USE1_SPECIAL_OPCAT_FLOWCTRL << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
							(EURASIA_USE1_FLOWCTRL_OP2_BR << EURASIA_USE1_FLOWCTRL_OP2_SHIFT);

		/*
			Set up the instruction which will do the first tile emit.
			emitpix1 #0 , r0, r1, r0, sideband
			Line i
		*/
		ui32EmitFlag = EURASIA_PIXELBE1SB_TWOEMITS;
		*pui32Buffer++ = ((EURASIA_USE0_S1STDBANK_TEMP << EURASIA_USE0_S1BANK_SHIFT) |
						(ui32EmitFlag << EURASIA_USE0_EMIT_SIDEBAND_101_96_SHIFT) | 
						(0 << EURASIA_USE0_SRC0_SHIFT) |
						(1 << EURASIA_USE0_SRC1_SHIFT));

		*pui32Buffer++ = ((EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
						(EURASIA_USE1_OTHER_OP2_EMIT << EURASIA_USE1_OTHER_OP2_SHIFT) |
						(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
						(EURASIA_USE1_EMIT_TARGET_PIXELBE << EURASIA_USE1_EMIT_TARGET_SHIFT) |
						(((ui32EmitFlag >> 12) << EURASIA_USE1_EMIT_SIDEBAND_109_108_SHIFT) & ~EURASIA_USE1_EMIT_SIDEBAND_109_108_CLRMSK) |
						(((ui32EmitFlag >> 6) << EURASIA_USE1_EMIT_SIDEBAND_107_102_SHIFT) & ~EURASIA_USE1_EMIT_SIDEBAND_107_102_CLRMSK) |
						(EURASIA_USE1_S0STDBANK_TEMP << EURASIA_USE1_S0BANK_SHIFT) |
						(0 << EURASIA_USE1_EMIT_INCP_SHIFT));	
	
		/*
			Set up the instruction which will do the second tile emit.
			emitpix2 #0 , r6, r3, r0, sideband
			Line i
		*/
		*pui32Buffer++ = ((EURASIA_USE0_S1STDBANK_TEMP << EURASIA_USE0_S1BANK_SHIFT) |
						((ui32SideBand << EURASIA_USE0_EMIT_SIDEBAND_101_96_SHIFT) & ~EURASIA_USE0_EMIT_SIDEBAND_101_96_CLRMSK) |
						(6 << EURASIA_USE0_SRC0_SHIFT) |
						(3 << EURASIA_USE0_SRC1_SHIFT));

		*pui32Buffer++ = ((EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
						(EURASIA_USE1_OTHER_OP2_EMIT << EURASIA_USE1_OTHER_OP2_SHIFT) |
						(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
						(EURASIA_USE1_EMIT_TARGET_PIXELBE << EURASIA_USE1_EMIT_TARGET_SHIFT) |
						(((ui32SideBand >> 12) << EURASIA_USE1_EMIT_SIDEBAND_109_108_SHIFT) & ~EURASIA_USE1_EMIT_SIDEBAND_109_108_CLRMSK) |
						(((ui32SideBand >> 6) << EURASIA_USE1_EMIT_SIDEBAND_107_102_SHIFT) & ~EURASIA_USE1_EMIT_SIDEBAND_107_102_CLRMSK) |
						(EURASIA_USE1_S0STDBANK_TEMP << EURASIA_USE1_S0BANK_SHIFT) |
						(0 << EURASIA_USE1_EMIT_INCP_SHIFT));

		/*
			xor.end.skipinv g0, g0, #0x00000001
		*/
		*pui32Buffer++ = (EURASIA_USE0_S1EXTBANK_SPECIAL << EURASIA_USE0_S1BANK_SHIFT) |
						(EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT) |
						((EURASIA_USE_SPECIAL_BANK_G0| EURASIA_USE_SPECIAL_INTERNALDATA) << EURASIA_USE0_DST_SHIFT) |
						((EURASIA_USE_SPECIAL_BANK_G0 | EURASIA_USE_SPECIAL_INTERNALDATA) << EURASIA_USE0_SRC1_SHIFT) |
						(1 << EURASIA_USE0_SRC2_SHIFT);

		*pui32Buffer++ = (EURASIA_USE1_OP_XOR << EURASIA_USE1_OP_SHIFT) |
							EURASIA_USE1_SKIPINV |
							EURASIA_USE1_DBEXT |
							EURASIA_USE1_S1BEXT |
							EURASIA_USE1_S2BEXT | 
							(1 << EURASIA_USE1_RMSKCNT_SHIFT) |
							(EURASIA_USE1_D1EXTBANK_SPECIAL << EURASIA_USE1_D1BANK_SHIFT);
	}

	/*
		Set up the instruction which will do the dummy tile emit.
	*/
	*pui32Buffer++ = ((EURASIA_USE0_S1STDBANK_TEMP << EURASIA_USE0_S1BANK_SHIFT) |
					EURASIA_USE0_EMIT_FREEP |
					(4 << EURASIA_USE0_SRC0_SHIFT) |
					(0 << EURASIA_USE0_SRC1_SHIFT));

	*pui32Buffer++ = ((EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
					(EURASIA_USE1_OTHER_OP2_EMIT << EURASIA_USE1_OTHER_OP2_SHIFT) |
					(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
					(EURASIA_USE1_EMIT_TARGET_PIXELBE << EURASIA_USE1_EMIT_TARGET_SHIFT) |
					(EURASIA_USE1_S0STDBANK_TEMP << EURASIA_USE1_S0BANK_SHIFT) |
					(0 << EURASIA_USE1_EMIT_INCP_SHIFT));	

	/* Terminate this block of code (sneaky negative array index) */
	pui32Buffer[-1] |= EURASIA_USE1_END;

	return pui32Buffer;
}

#endif /* defined(FIX_HW_BRN_22249) */

/******************************************************************************
 End of file (pixevent.c)
******************************************************************************/

