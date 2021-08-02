/******************************************************************************
 * Name         : icvt_mem.c
 * Title        : UNIFLEX-to-intermediate code conversion for memory related functions.
 * Created      : May 2010
 *
 * Copyright    : 2002-2010 by Imagination Technologies Limited.
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
 * Modifications:-
 * $Log: icvt_mem.c $
 *****************************************************************************/

#include "uscshrd.h"
#include "bitops.h"

static IMG_VOID ConvertLDInstruction(PINTERMEDIATE_STATE psState,
									 PCODEBLOCK psCodeBlock,
									 PUNIFLEX_INST psUFInst)
/*****************************************************************************
 FUNCTION	: ConvertLDInstruction

 PURPOSE	: Convert an input LD instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psUFInst				- Instruction to convert.

 RETURNS	: None.
*****************************************************************************/
{
	ARG sAddress;
	IMG_UINT32 uReg;
	IMG_UINT8 uFlags;
	IMG_UINT8 uPrimSize;
	IMG_UINT32 auChan[4];
	IMG_UINT32 uNumElements;
	IMG_UINT32 uNumUFReg;
	IMG_UINT32 uNumActiveChans = 0;

	/* The amount of data to load (in elements) */
	uNumElements = psUFInst->asSrc[2].uNum;
	ASSERT (uNumElements > 0);

	/* Whether the pointer has no aliases */
	uFlags = (IMG_UINT8)psUFInst->asSrc[3].uNum;

	/* Decode the mask on the destination register */
	auChan[0] = psUFInst->sDest.u.byMask & (1 << UFREG_DMASK_X_SHIFT);
	auChan[1] = psUFInst->sDest.u.byMask & (1 << UFREG_DMASK_Y_SHIFT);
	auChan[2] = psUFInst->sDest.u.byMask & (1 << UFREG_DMASK_Z_SHIFT);
	auChan[3] = psUFInst->sDest.u.byMask & (1 << UFREG_DMASK_W_SHIFT);

	/* Get the number of active channels */
	if (auChan[0] != 0) ++uNumActiveChans;
	if (auChan[1] != 0) ++uNumActiveChans;
	if (auChan[2] != 0) ++uNumActiveChans;
	if (auChan[3] != 0) ++uNumActiveChans;

	/* The number of UF registers to iterate over */
	uNumUFReg = uNumElements / uNumActiveChans;

	/* Select the stride using the format of the dest register */
	switch (psUFInst->sDest.eFormat)
	{
	case UF_REGFORMAT_F32:
	case UF_REGFORMAT_I32:
	case UF_REGFORMAT_U32:
		{
			uPrimSize = 4;
			break;
		}
	case UF_REGFORMAT_F16:
	case UF_REGFORMAT_U16:
	case UF_REGFORMAT_I16:
		{
			uPrimSize = 2;
			break;
		}
	case UF_REGFORMAT_I8_UN:
	case UF_REGFORMAT_U8_UN:
		{
			uPrimSize = 1;
			break;
		}
	default:
		{
			imgabort ();
			return;
		}
	}

	/* Get the base address register */
	{
		FLOAT_SOURCE_MODIFIER sAddrMod;
		GetSourceTypeless(psState, psCodeBlock, &psUFInst->asSrc[0], 0, &sAddress, IMG_TRUE, &sAddrMod);
	}

	for (uReg = 0; uReg < uNumUFReg; ++uReg)
	{
		ARG asDst[4];
		IMG_UINT8 uChan = 0;
		IMG_UINT8 uNumChansUsed = 0;

		/* Get the offset UF dest */
		UF_REGISTER sUFDst = psUFInst->sDest;
		sUFDst.uNum += uReg;

		/* Make sure the input temp register count is accurate for our expanded destination */
		if (sUFDst.uNum >= psState->uInputTempRegisterCount)
		{
			psState->uInputTempRegisterCount = sUFDst.uNum + 1;
		}

		for (; uNumChansUsed < uNumActiveChans; ++uChan)
		{
			PINST psUscInst;
			FLOAT_SOURCE_MODIFIER asOffMod;

			/* Check whether this channel is active */
			if (auChan[uChan] == 0)
				continue;

			/* Get the destination register of the load. */
			GetDestinationTypeless(psState, psCodeBlock, &sUFDst, uChan, &asDst[uChan]);
			asDst[uChan].eFmt = UF_REGFORMAT_UNTYPED;

			psUscInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psUscInst, ILOADMEMCONST);

			/* Destination */
			psUscInst->asDest[0] = asDst[uChan];

			/*
				Set the size of the data to be loaded.
			*/
			psUscInst->u.psLoadMemConst->uDataSize = uPrimSize;

			/* Set the base address */
			psUscInst->asArg[LOADMEMCONST_BASE_ARGINDEX] = sAddress;
			
			/* Set the dynamic offset */
			{
				if (psUFInst->asSrc[1].eType != UFREG_TYPE_IMMEDIATE)
				{
					ARG sOffset;

					/* Get the address offset of the load */
					GetSourceTypeless(psState, psCodeBlock, &psUFInst->asSrc[1], uChan, &sOffset, IMG_TRUE, &asOffMod);

					psUscInst->u.psLoadMemConst->bRelativeAddress = IMG_TRUE;
					psUscInst->asArg[LOADMEMCONST_DYNAMIC_OFFSET_ARGINDEX] = sOffset;
				}
				else
				{
					psUscInst->u.psLoadMemConst->bRelativeAddress = IMG_FALSE;
					InitInstArg(&psUscInst->asArg[LOADMEMCONST_DYNAMIC_OFFSET_ARGINDEX]);
					psUscInst->asArg[LOADMEMCONST_DYNAMIC_OFFSET_ARGINDEX].uType = USEASM_REGTYPE_IMMEDIATE;
					psUscInst->asArg[LOADMEMCONST_DYNAMIC_OFFSET_ARGINDEX].uNumber = 0;
				}
			}

			/* Set the static offset */
			{
				InitInstArg(&psUscInst->asArg[LOADMEMCONST_STATIC_OFFSET_ARGINDEX]);
				psUscInst->asArg[LOADMEMCONST_STATIC_OFFSET_ARGINDEX].uType = USEASM_REGTYPE_IMMEDIATE;
				psUscInst->asArg[LOADMEMCONST_STATIC_OFFSET_ARGINDEX].uNumber = (uReg * uNumActiveChans) + uNumChansUsed;

				if (psUFInst->asSrc[1].eType == UFREG_TYPE_IMMEDIATE)
				{
					psUscInst->asArg[LOADMEMCONST_STATIC_OFFSET_ARGINDEX].uNumber += psUFInst->asSrc[1].uNum;
				}

				psUscInst->asArg[LOADMEMCONST_STATIC_OFFSET_ARGINDEX].uNumber *= uPrimSize;
				psUscInst->asArg[LOADMEMCONST_STATIC_OFFSET_ARGINDEX].uNumber += 4;
			}

			/* Set the stride */
			{
				InitInstArg(&psUscInst->asArg[LOADMEMCONST_STRIDE_ARGINDEX]);
				psUscInst->asArg[LOADMEMCONST_STRIDE_ARGINDEX].uType = USEASM_REGTYPE_IMMEDIATE;

				if (psUscInst->u.psLoadMemConst->bRelativeAddress)
				{
					psUscInst->asArg[LOADMEMCONST_STRIDE_ARGINDEX].uNumber = uPrimSize;
				}
				else
				{
					psUscInst->asArg[LOADMEMCONST_STRIDE_ARGINDEX].uNumber = 0;
				}
			}

			/* Set the various load flags */
			psUscInst->u.psLoadMemConst->uFlags = uFlags;

			/* If there's been a memory store, we may need to set the cache bypass flag */
			if (psState->uOptimizationHint & USC_COMPILE_HINT_ABS_STORE_USED)
			{
				/**
				 * If this pointer is to constant memory, we don't need to bypass the cache.
				 */
				if ((uFlags & UF_MEM_CONST) == 0)
				{
					psUscInst->u.psLoadMemConst->bBypassCache = BypassCacheForModifiableShaderMemory (psState);
				}
			}

			AppendInst(psState, psCodeBlock, psUscInst);
			++uNumChansUsed;
		}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
		{
			ARG sVectorDst;
			PINST psUscInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psUscInst, IVMOV);

			/* Get the destination register of the pack. */
			GetDestinationF32_Vec(psState, &sUFDst, &sVectorDst);
			psUscInst->asDest[0] = sVectorDst;

			/* Set the source registers of the pack */
			psUscInst->asArg[0].uType = USC_REGTYPE_UNUSEDSOURCE;
			for (uChan = 0; uChan < 4; ++uChan)
			{
				/* Check whether this channel is active */
				if (auChan[uChan] == 0)
					continue;

				psUscInst->asArg[uChan+1] = asDst[uChan];
				psUscInst->asArg[uChan+1].eFmt = UF_REGFORMAT_F32;
			}

			psUscInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
			psUscInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F32;

			AppendInst(psState, psCodeBlock, psUscInst);
		}
#endif
	}
}

static IMG_VOID ConvertSTInstruction(PINTERMEDIATE_STATE psState,
									 PCODEBLOCK psCodeBlock,
									 PUNIFLEX_INST psUFInst)
/*****************************************************************************
 FUNCTION	: ConvertSTInstruction

 PURPOSE	: Convert an input ST instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
							psCodeBlock	- Block to add the intermediate instructions to.
							psUFInst		- Instruction to convert.

 RETURNS	: None.
*****************************************************************************/
{
	ARG sAddress;
	IOPCODE opcode;
	IMG_UINT8 uChan;
	IMG_UINT8 uFlags;
	IMG_UINT8 uPrimSize;
	IMG_UINT8 uAddrOffset;
	IMG_UINT32 uReg;
	IMG_UINT32 auChan[4];
	IMG_UINT32 uNumElements;
	IMG_UINT32 uNumUFReg;
	IMG_UINT32 uGranularity;

	/* The size of the data to store (in elements) */
	uNumElements = psUFInst->asSrc[4].uNum;
	ASSERT (uNumElements > 0);

	/* The granularity of the store (in channels per register) */
	uGranularity = psUFInst->asSrc[3].uNum;
	ASSERT (uGranularity <= 4);
	ASSERT (uGranularity > 0);

	/* Whether the pointer has no aliases */
	uFlags = (IMG_UINT8)psUFInst->asSrc[5].uNum;

	/* The number of UF registers to iterate over */
	uNumUFReg = uNumElements / uGranularity;

	/* Select the instruction using the format of the source register */
	switch (psUFInst->asSrc[0].eFormat)
	{
	case UF_REGFORMAT_F32:
	case UF_REGFORMAT_I32:
	case UF_REGFORMAT_U32:
		{
			uPrimSize = 4;
			uAddrOffset = 1;
			opcode = ISTAD;
			break;
		}
	case UF_REGFORMAT_F16:
	case UF_REGFORMAT_U16:
	case UF_REGFORMAT_I16:
		{
			uPrimSize = 2;
			uAddrOffset = 2;
			opcode = ISTAW;
			break;
		}
	case UF_REGFORMAT_I8_UN:
	case UF_REGFORMAT_U8_UN:
		{
			uPrimSize = 1;
			uAddrOffset = 4;
			opcode = ISTAB;
			break;
		}
	default:
		{
			imgabort ();
			return;
		}
	}

	/* Decode the swizzle on the source register */
	auChan[0] = (psUFInst->asSrc[0].u.uSwiz >> 0) & 3;
	auChan[1] = (psUFInst->asSrc[0].u.uSwiz >> 3) & 3;
	auChan[2] = (psUFInst->asSrc[0].u.uSwiz >> 6) & 3;
	auChan[3] = (psUFInst->asSrc[0].u.uSwiz >> 9) & 3;

	/* Get the base address register */
	{
		FLOAT_SOURCE_MODIFIER sAddrMod;
		GetSourceTypeless(psState, psCodeBlock, &psUFInst->asSrc[1], 0, &sAddress, IMG_FALSE, &sAddrMod);
	}

	for (uReg = 0; uReg < uNumUFReg; ++uReg)
	{
		for (uChan = 0; uChan < uGranularity; ++uChan)
		{
			ARG sSrcReg;
			PINST psUscInst;
			FLOAT_SOURCE_MODIFIER asArgMod[2];

			psUscInst = AllocateInst (psState, IMG_NULL);
			SetOpcode (psState, psUscInst, opcode);

			/* Get the register to the store */
			{
				UF_REGISTER sSrc = psUFInst->asSrc[0];
				sSrc.uNum += uReg;

				if (sSrc.eFormat == UF_REGFORMAT_F16)
					sSrc.eFormat = UF_REGFORMAT_F32;

				GetSourceTypeless (psState, psCodeBlock, &sSrc, auChan[uChan], &sSrcReg, IMG_TRUE, &asArgMod[0]);
			}

			/* Destination */
			psUscInst->asDest[0].uType = USC_REGTYPE_DUMMY;
			psUscInst->asDest[0].uNumber = 0;

			/* Address */
			psUscInst->asArg[0] = sAddress;

			/* Set the static offset */
			InitInstArg(&psUscInst->asArg[1]);
			psUscInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psUscInst->asArg[1].uNumber = psUFInst->asSrc[2].uNum + (uReg * 4) + uChan + psState->uMemOffsetAdjust + uAddrOffset;

			/*
				Set the units of the static offset.
			*/
			psUscInst->asArg[1].uNumber |= (uPrimSize << 16);

			/* Source register */
			psUscInst->asArg[2] = sSrcReg;

			/* Aliasing */
			psUscInst->u.psLdSt->uFlags = uFlags;

			AppendInst (psState, psCodeBlock, psUscInst);
		}
	}
}

IMG_INTERNAL 
PCODEBLOCK ConvertInstToIntermediateMem(PINTERMEDIATE_STATE psState, 
										PCODEBLOCK psCodeBlock, 
										PUNIFLEX_INST psUFInst, 
										IMG_BOOL bConditional,
										IMG_BOOL bStaticCond)
/*****************************************************************************
 FUNCTION	: ConvertInstToIntermediateMem

 PURPOSE	: Convert an input instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psUFInst				- Instruction to convert.
			  bConditional		- Whether instruction is conditionally executed
			  bStaticCond		- Whether condition is static.

 RETURNS	: None.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(bConditional);
	PVR_UNREFERENCED_PARAMETER(bStaticCond);

	switch (psUFInst->eOpCode)
	{
	case UFOP_MEMLD:
		{
			ConvertLDInstruction(psState, psCodeBlock, psUFInst);
			break;
		}
	case UFOP_MEMST:
		{
			ConvertSTInstruction(psState, psCodeBlock, psUFInst);
			psState->uOptimizationHint |= USC_COMPILE_HINT_ABS_STORE_USED;
			break;
		}
	default: imgabort();
	}

	return psCodeBlock;
}
