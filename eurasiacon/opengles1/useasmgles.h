/******************************************************************************
 * Name         : useasmgles.h
 *
 * Copyright    : 2008 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited, Home Park
 *              : Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * Description  : USEASM interfacing functions
 *
 * Platform     : ANSI
 *
 * $Log: useasmgles.h $
 *
 *****************************************************************************/



typedef struct GLESUSEASMInfoRec
{
	/* USEASM instructions (USEASM input) */
	IMG_UINT32	ui32NumMainUSEASMInstructions;
	USE_INST	*psFirstUSEASMInstruction;
	USE_INST	*psLastUSEASMInstruction;

	/* For USEASMOPT */
	IMG_UINT32 ui32MaxPrimaryNumber;
	IMG_UINT32 ui32MaxTempNumber;


	/* HW instructions  (USEASM output) */
	IMG_UINT32	ui32NumHWInstructions;
	IMG_UINT32	*pui32HWInstructions;

} GLESUSEASMInfo;


#define SETUP_INSTRUCTION_ARG(A, B, C, D) \
	asArg[A].uNumber       = B; \
	asArg[A].uType         = C; \
	asArg[A].uFlags        = D; \
	asArg[A].uIndex        = USEREG_INDEX_NONE; \
	asArg[A].uNamedRegLink = 0;



IMG_VOID AddInstruction(GLES1Context   *gc,
						GLESUSEASMInfo *psUSEASMInfo,
					    USEASM_OPCODE  uOpcode,
					    IMG_UINT32     uFlags1,
					    IMG_UINT32     uFlags2,
					    IMG_UINT32     uTest,
					    USE_REGISTER   *psArgs,
					    IMG_UINT32     ui32NumArgs);

IMG_VOID FreeUSEASMInstructionList(GLES1Context *gc, GLESUSEASMInfo *psUSEASMInfo);

IMG_VOID DuplicateUSEASMInstructionList(GLES1Context *gc, GLESUSEASMInfo *psSrcUSEASMInfo, GLESUSEASMInfo *psDstUSEASMInfo);

GLES1_MEMERROR AssembleUSEASMInstructions(GLES1Context *gc, GLESUSEASMInfo *psUSEASMInfo);
