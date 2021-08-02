 /*****************************************************************************
 Name			: USP_INSTBLOCK.H
 
 Title			: Header for usp_instblock.c
 
 C Author 		: Joe Molleson
 
 Created  		: 02/01/2002
 
 Copyright      : 2002-2006 by Imagination Technologies Limited. All rights reserved.
                : No part of this software, either material or conceptual 
                : may be copied or distributed, transmitted, transcribed,
                : stored in a retrieval system or translated into any 
                : human or computer language in any form by any means,
                : electronic, mechanical, manual or other-wise, or 
                : disclosed to third parties without the express written
                : permission of Imagination Technologies Limited, Unit 8, HomePark
                : Industrial Estate, King's Langley, Hertfordshire,
                : WD4 8LZ, U.K.

 Description 	: Definitions and prototypes for usp_instblock.c
   
 Program Type	: 32-bit DLL

 Version	 	: $Revision: 1.7 $

 Modifications	:

 $Log: usp_instblock.h $
*****************************************************************************/
#ifndef _USP_INSTBLOCK_H_
#define _USP_INSTBLOCK_H_

/*
	Initialise the instrucitons in a block, ready for finalising
*/
IMG_VOID USPInstBlockReset(PUSP_INSTBLOCK psInstBlock);

/*
	Insert a HW instruction into a block
*/
IMG_BOOL USPInstBlockInsertHWInst(PUSP_INSTBLOCK	psInstBlock,
								  PUSP_INST			psInsertPos,
								  USP_OPCODE		eOpcode,
								  PHW_INST			psHWInst,
								  IMG_UINT32		uInstDescFlags,
								  PUSP_CONTEXT		psContext,
								  PUSP_INST*		ppsInst);

/*
	Append a pre-compiled HW instrucion to an instruction-block
*/
IMG_BOOL USPInstBlockAddPCInst(PUSP_INSTBLOCK	psInstBlock,
							   PHW_INST			psPCInst,
							   IMG_UINT32		uInstDescFlags,
							   PUSP_CONTEXT		psContext,
							   PUSP_INST*		ppsInst);

/*
	Remove a USP-instruction from a block
*/
IMG_BOOL USPInstBlockRemoveInst(PUSP_INSTBLOCK	psInstBlock,
								PUSP_INST		psInst);

/*
	Finalise the HW instructions to be generated for a block
*/
IMG_BOOL USPInstBlockFinaliseInsts(PUSP_INSTBLOCK	psBlock,
								   PUSP_CONTEXT		psContext);

/*
	Create a USP instruction block
*/
PUSP_INSTBLOCK USPInstBlockCreate(PUSP_CONTEXT	psContext,
								  PUSP_SHADER	psShader,
								  IMG_UINT32	uMaxInstCount,
								  IMG_UINT32	uOrgInstCount,
								  IMG_BOOL		bAddResultRefs,
								  PUSP_MOESTATE	psMOEState);

/*
	Destroy a USP instruction block
*/
IMG_VOID USPInstBlockDestroy(PUSP_INSTBLOCK	psInstBlock,
							 PUSP_CONTEXT	psContext);

#endif	/* #ifndef _USP_INSTBLOCK_H_ */
/*****************************************************************************
 End of file (USP_INSTBLOCK.H)
*****************************************************************************/
