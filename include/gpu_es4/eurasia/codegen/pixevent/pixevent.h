/******************************************************************************
 * Name         : pixevent.h
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
 * $Log: pixevent.h $
 *****************************************************************************/
#ifndef _PIXEVENT_H_
#define _PIXEVENT_H_

#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
	#define STATE_PBE_DWORDS	6
#else
#if defined(SGX_FEATURE_PIXELBE_32K_LINESTRIDE)
	#define STATE_PBE_DWORDS	5
#else
	#define STATE_PBE_DWORDS	4
#endif
#endif

#if defined(FIX_HW_BRN_31982)
#define USE_PIXELEVENT_EOT_2XMSAA_BYTES			(EURASIA_USE_INSTRUCTION_SIZE * 98)
#endif

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)

#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
#define USE_PIXELEVENT_EOT_BYTES				(EURASIA_USE_INSTRUCTION_SIZE * (2 + STATE_PBE_DWORDS))
#else
#define USE_PIXELEVENT_EOT_BYTES				(EURASIA_USE_INSTRUCTION_SIZE * (3 + 3 + STATE_PBE_DWORDS))
#endif
#define USE_PIXELEVENT_PBESTATE_BYTE_OFFSET		(EURASIA_USE_INSTRUCTION_SIZE * 1)
#define USE_PIXELEVENT_SIDEBAND_BYTE_OFFSET		(EURASIA_USE_INSTRUCTION_SIZE * (STATE_PBE_DWORDS + 2))

#else /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

#if (defined(FIX_HW_BRN_23194) || defined(FIX_HW_BRN_23460) || defined(FIX_HW_BRN_23949))

#if !defined(SUPPORT_SGX_COMPLEX_GEOMETRY)

#if defined(FIX_HW_BRN_23460) || defined(FIX_HW_BRN_23949)
#define USE_PIXELEVENT_EOT_BYTES				(EURASIA_USE_INSTRUCTION_SIZE * (7 + STATE_PBE_DWORDS))
#else
#define USE_PIXELEVENT_EOT_BYTES				(EURASIA_USE_INSTRUCTION_SIZE * (5 + STATE_PBE_DWORDS))
#endif

#define USE_PIXELEVENT_PBESTATE_BYTE_OFFSET		(EURASIA_USE_INSTRUCTION_SIZE * 2)	
#define USE_PIXELEVENT_SIDEBAND_BYTE_OFFSET		(EURASIA_USE_INSTRUCTION_SIZE * 7)

#else

#if defined(FIX_HW_BRN_23460) || defined(FIX_HW_BRN_23949)
#define USE_PIXELEVENT_EOT_BYTES				(EURASIA_USE_INSTRUCTION_SIZE * (11 + STATE_PBE_DWORDS))
#else
#define USE_PIXELEVENT_EOT_BYTES				(EURASIA_USE_INSTRUCTION_SIZE * (9 + STATE_PBE_DWORDS))
#endif

#define USE_PIXELEVENT_PBESTATE_BYTE_OFFSET		(EURASIA_USE_INSTRUCTION_SIZE * 4)	
#define USE_PIXELEVENT_SIDEBAND_BYTE_OFFSET		(EURASIA_USE_INSTRUCTION_SIZE * 9)

#endif /* !defined(SUPPORT_SGX_COMPLEX_GEOMETRY) */

#else

#if defined(FIX_HW_BRN_23460) || defined(FIX_HW_BRN_23949)
#define USE_PIXELEVENT_EOT_BYTES				(EURASIA_USE_INSTRUCTION_SIZE * (4 + STATE_PBE_DWORDS))
#else
#define USE_PIXELEVENT_EOT_BYTES				(EURASIA_USE_INSTRUCTION_SIZE * (2 + STATE_PBE_DWORDS))
#endif

#define USE_PIXELEVENT_PBESTATE_BYTE_OFFSET		(EURASIA_USE_INSTRUCTION_SIZE * 0)	
#define USE_PIXELEVENT_SIDEBAND_BYTE_OFFSET		(EURASIA_USE_INSTRUCTION_SIZE * (STATE_PBE_DWORDS + 1))


#endif /* (defined(FIX_HW_BRN_23194) || defined(FIX_HW_BRN_23460) || defined(FIX_HW_BRN_23949)) */

#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */


#if defined(FIX_HW_BRN_23460) || defined(FIX_HW_BRN_23949)
#define USE_PIXELEVENT_NUM_TEMPS				(STATE_PBE_DWORDS + 1)
#else 
#define USE_PIXELEVENT_NUM_TEMPS				(STATE_PBE_DWORDS)
#endif 

#if defined(FIX_HW_BRN_22249)

#define USE_PIXELEVENT2K_EOT_BYTES					(EURASIA_USE_INSTRUCTION_SIZE * 132)

#define USE_PIXELEVENT2K_PBESTATE_BYTE_OFFSET		(EURASIA_USE_INSTRUCTION_SIZE * 2)	
#define USE_PIXELEVENT2K_SIDEBAND_BYTE_OFFSET		(EURASIA_USE_INSTRUCTION_SIZE * 9)

#define USE_PIXELEVENT2K_NUM_TEMPS					7

#endif /* defined(FIX_HW_BRN_22249) */

#define USE_PIXELEVENT_EOR_NUM_TEMPS				1

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)

#if defined(SGX543) || defined(SGX544) || defined(SGX554)
#define USE_PIXELEVENT_PTOFF_BYTES				(EURASIA_USE_INSTRUCTION_SIZE)
#else
#define USE_PIXELEVENT_PTOFF_BYTES				(EURASIA_USE_INSTRUCTION_SIZE * 2)
#endif

#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
#define USE_PIXELEVENT_EOR_BYTES				(EURASIA_USE_INSTRUCTION_SIZE * 4)
#else
#define USE_PIXELEVENT_EOR_BYTES				(EURASIA_USE_INSTRUCTION_SIZE * 3)
#endif
#else
#define USE_PIXELEVENT_PTOFF_BYTES				(EURASIA_USE_INSTRUCTION_SIZE)
#define USE_PIXELEVENT_EOR_BYTES				(EURASIA_USE_INSTRUCTION_SIZE * 2)
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */



IMG_INTERNAL IMG_UINT32 * WriteEndOfTileUSSECode(
				IMG_UINT32 *pui32BufferBase,
				IMG_UINT32 *pui32EmitWords,
				IMG_UINT32 ui32SideBand
				#if defined(SGX_FEATURE_RENDER_TARGET_ARRAYS)
				,IMG_PUINT32 *ppui32StateWordLimmOffset
				#endif
				#if defined(SGX545)
				,IMG_BOOL bFlushCache
				#endif
			);

#if defined(SGX545)
#define USE_PIXELEVENT_EOT_MULTIEMIT_BYTES(NUM_EMITS)	(EURASIA_USE_INSTRUCTION_SIZE * (1 + 3 + NUM_EMITS * (STATE_PBE_DWORDS + 2))) 

IMG_PUINT32 WriteEndOfTileUSSECodeMultiEmit(IMG_UINT32 *pui32BufferBase,
											IMG_UINT32 pui32EmitWords[][STATE_PBE_DWORDS + 1], 
											IMG_UINT32 ui32EmitCount
											#if defined(SGX545)
											,IMG_BOOL bFlushCache
											#endif
											);
#endif /* defined(SGX545) */

IMG_INTERNAL IMG_UINT32 * WriteEndOfTileUSSECode2KStride(IMG_UINT32 *pui32BufferBase, IMG_UINT32 *pui32EmitWords, IMG_UINT32 ui32SideBand);

#if defined(FIX_HW_BRN_31982)
IMG_INTERNAL IMG_UINT32 * WriteEndOfTileUSSECode2xMSAA(IMG_UINT32 *pui32BufferBase, IMG_UINT32 *pui32EmitWords, IMG_UINT32 ui32SideBand);
#endif
IMG_INTERNAL IMG_UINT32 * WritePTOffUSSECode(IMG_UINT32 *pui32BufferBase);

IMG_INTERNAL IMG_UINT32 * WriteEndOfRenderUSSECode(IMG_UINT32 *pui32BufferBase);


#endif /* _PIXEVENT_H_ */
