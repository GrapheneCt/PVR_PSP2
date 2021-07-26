/******************************************************************************
 * Name         : macros.h
 * Author       : James McCarthy
 * Created      : 16/11/2005
 *
 * Copyright    : 2000-2006 by Imagination Technologies Limited.
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
 * Modifications:-
 * $Log: macros.h $
 *****************************************************************************/

#ifndef __gl_macros_h_
#define __gl_macros_h_

#include <string.h>
#include "ffgen.h"


#define IS_POWER_OF_2(a) (((a - 1) & a) == 0) 

#if defined(STANDALONE)
#define COPYDESC(a,b) strncpy(a, b, MAX_DESC_SIZE - 1); a[MAX_DESC_SIZE - 1] = '\0';
#else
#define COPYDESC(a,b)
#endif

#if defined(STANDALONE) || defined(DEBUG)

#define INCREASE_INDENT() psFFGenCode->uIndent++;
#define DECREASE_INDENT() psFFGenCode->uIndent--;

#else

#define INCREASE_INDENT() 
#define DECREASE_INDENT()

#endif

#define INST(InstName, Comment) \
	psInst->eOpcode    = USEASM_OP_##InstName; \
	psInst->uNumRegs   = 0; \
	psInst->pszComment = Comment; \
	EncodeInstruction(psFFGenCode, psInst);

#define INST0(InstName, DestReg, Comment) \
	psInst->eOpcode    = USEASM_OP_##InstName; \
	psInst->ppsRegs[0] = DestReg; \
	psInst->uNumRegs   = 1; \
	psInst->pszComment = Comment; \
	COPYDESC(psInst->ppsRegs[0]->acDesc, #DestReg); \
	EncodeInstruction(psFFGenCode, psInst);

#define INST1(InstName, DestReg, SrcReg1, Comment) \
	psInst->eOpcode    = USEASM_OP_##InstName; \
	psInst->ppsRegs[0] = DestReg; \
	psInst->ppsRegs[1] = SrcReg1; \
	psInst->uNumRegs   = 2; \
	psInst->pszComment = Comment; \
	COPYDESC(psInst->ppsRegs[0]->acDesc, #DestReg); \
	COPYDESC(psInst->ppsRegs[1]->acDesc, #SrcReg1); \
	EncodeInstruction(psFFGenCode, psInst);

#define INST2(InstName, DestReg, SrcReg1, SrcReg2, Comment) \
	psInst->eOpcode    = USEASM_OP_##InstName; \
	psInst->ppsRegs[0] = DestReg; \
	psInst->ppsRegs[1] = SrcReg1; \
	psInst->ppsRegs[2] = SrcReg2; \
	psInst->uNumRegs   = 3; \
	psInst->pszComment = Comment; \
	COPYDESC(psInst->ppsRegs[0]->acDesc, #DestReg); \
	COPYDESC(psInst->ppsRegs[1]->acDesc, #SrcReg1); \
	COPYDESC(psInst->ppsRegs[2]->acDesc, #SrcReg2); \
	EncodeInstruction(psFFGenCode, psInst);

#define INST3(InstName, DestReg, SrcReg1, SrcReg2, SrcReg3, Comment) \
	psInst->eOpcode    = USEASM_OP_##InstName; \
	psInst->ppsRegs[0] = DestReg; \
	psInst->ppsRegs[1] = SrcReg1; \
	psInst->ppsRegs[2] = SrcReg2; \
	psInst->ppsRegs[3] = SrcReg3; \
	psInst->uNumRegs   = 4; \
	psInst->pszComment = Comment; \
	COPYDESC(psInst->ppsRegs[0]->acDesc, #DestReg); \
	COPYDESC(psInst->ppsRegs[1]->acDesc, #SrcReg1); \
	COPYDESC(psInst->ppsRegs[2]->acDesc, #SrcReg2); \
	COPYDESC(psInst->ppsRegs[3]->acDesc, #SrcReg3); \
	EncodeInstruction(psFFGenCode, psInst);

#define INST4(InstName, DestReg, SrcReg1, SrcReg2, SrcReg3, SrcReg4, Comment) \
	psInst->eOpcode    = USEASM_OP_##InstName; \
	psInst->ppsRegs[0] = DestReg; \
	psInst->ppsRegs[1] = SrcReg1; \
	psInst->ppsRegs[2] = SrcReg2; \
	psInst->ppsRegs[3] = SrcReg3; \
	psInst->ppsRegs[4] = SrcReg4; \
	psInst->uNumRegs   = 5; \
	psInst->pszComment = Comment; \
	COPYDESC(psInst->ppsRegs[0]->acDesc, #DestReg); \
	COPYDESC(psInst->ppsRegs[1]->acDesc, #SrcReg1); \
	COPYDESC(psInst->ppsRegs[2]->acDesc, #SrcReg2); \
	COPYDESC(psInst->ppsRegs[3]->acDesc, #SrcReg3); \
	COPYDESC(psInst->ppsRegs[4]->acDesc, #SrcReg4); \
	EncodeInstruction(psFFGenCode, psInst);

#define INST5(InstName, DestReg, SrcReg1, SrcReg2, SrcReg3, SrcReg4, SrcReg5, Comment) \
	psInst->eOpcode    = USEASM_OP_##InstName; \
	psInst->ppsRegs[0] = DestReg; \
	psInst->ppsRegs[1] = SrcReg1; \
	psInst->ppsRegs[2] = SrcReg2; \
	psInst->ppsRegs[3] = SrcReg3; \
	psInst->ppsRegs[4] = SrcReg4; \
	psInst->ppsRegs[5] = SrcReg5; \
	psInst->uNumRegs   = 6; \
	psInst->pszComment = Comment; \
	COPYDESC(psInst->ppsRegs[0]->acDesc, #DestReg); \
	COPYDESC(psInst->ppsRegs[1]->acDesc, #SrcReg1); \
	COPYDESC(psInst->ppsRegs[2]->acDesc, #SrcReg2); \
	COPYDESC(psInst->ppsRegs[3]->acDesc, #SrcReg3); \
	COPYDESC(psInst->ppsRegs[4]->acDesc, #SrcReg4); \
	COPYDESC(psInst->ppsRegs[5]->acDesc, #SrcReg5); \
	EncodeInstruction(psFFGenCode, psInst);

#define INST6(InstName, DestReg, SrcReg1, SrcReg2, SrcReg3, SrcReg4, SrcReg5, SrcReg6, Comment) \
	psInst->eOpcode    = USEASM_OP_##InstName; \
	psInst->ppsRegs[0] = DestReg; \
	psInst->ppsRegs[1] = SrcReg1; \
	psInst->ppsRegs[2] = SrcReg2; \
	psInst->ppsRegs[3] = SrcReg3; \
	psInst->ppsRegs[4] = SrcReg4; \
	psInst->ppsRegs[5] = SrcReg5; \
	psInst->ppsRegs[6] = SrcReg6; \
	psInst->uNumRegs   = 7; \
	psInst->pszComment = Comment; \
	COPYDESC(psInst->ppsRegs[0]->acDesc, #DestReg); \
	COPYDESC(psInst->ppsRegs[1]->acDesc, #SrcReg1); \
	COPYDESC(psInst->ppsRegs[2]->acDesc, #SrcReg2); \
	COPYDESC(psInst->ppsRegs[3]->acDesc, #SrcReg3); \
	COPYDESC(psInst->ppsRegs[4]->acDesc, #SrcReg4); \
	COPYDESC(psInst->ppsRegs[5]->acDesc, #SrcReg5); \
	COPYDESC(psInst->ppsRegs[6]->acDesc, #SrcReg6); \
	EncodeInstruction(psFFGenCode, psInst);


#define SETDESTOFF(DestRegOff) \
	psInst->aiRegOffsets[0]   = DestRegOff; \
	psInst->uUseRegOffset    |= (1 << 0);

#define SETSRCOFF(SrcReg, SrcRegOff) \
	psInst->aiRegOffsets[SrcReg + 1] = SrcRegOff; \
	psInst->uUseRegOffset            |= (1 << (SrcReg + 1));

#define UNSETSRCOFF(SrcReg) \
	psInst->aiRegOffsets[SrcReg + 1] = 0; \
	psInst->uUseRegOffset            &= ~(1 << (SrcReg + 1));

#define GETSRCOFF(Inst, SrcReg) \
	(Inst->aiRegOffsets[SrcReg + 1])

#define SETDESTFLAG(Flag) \
	psInst->auRegFlags[0] |= Flag;

#define SETSRCFLAG(SrcReg, Flag) \
	psInst->auRegFlags[SrcReg + 1] |= Flag;

#define SETDESTMASK(uMask) \
	psInst->auRegFlags[0] &= USEASM_ARGFLAGS_BYTEMSK_CLRMSK; \
	psInst->auRegFlags[0] |= ((uMask & 0xF) << USEASM_ARGFLAGS_BYTEMSK_SHIFT);

#define SETSRCMASK(SrcReg, uMask) \
	psInst->auRegFlags[SrcReg + 1] &= USEASM_ARGFLAGS_BYTEMSK_CLRMSK; \
	psInst->auRegFlags[SrcReg + 1] |= ((uMask & 0xF) << USEASM_ARGFLAGS_BYTEMSK_SHIFT);

#define SETSRCCOMP(SrcReg, uComponent) \
	psInst->auRegFlags[SrcReg + 1] &= USEASM_ARGFLAGS_COMP_CLRMSK; \
	psInst->auRegFlags[SrcReg + 1] |= (uComponent << USEASM_ARGFLAGS_COMP_SHIFT);

#define USEINTERNALREG(uInternalReg) \
	psInst->uExtraInfo &= EURASIA_USE1_EFO_DSRC_CLRMSK; \
	psInst->uExtraInfo |= (uInternalReg) << EURASIA_USE1_EFO_DSRC_SHIFT;

#define SET_INDEXPATCH(Val) \
	psInst->bIndexPatch = ((Val) ? IMG_TRUE : IMG_FALSE);


#define INIT_REG(reg, type, bindingInfo, offset, size) \
	reg->eType         = type;   \
	reg->uSizeInDWords = size;   \
	reg->eWDFStatus    = 0;      \
	reg->uOffset       = offset; \
	reg->uIndex		   = USEREG_INDEX_NONE;

#define SET_FLOAT_ZERO(reg) (reg)->uOffset = EURASIA_USE_SPECIAL_CONSTANT_ZERO; 
#define SET_FLOAT_ONE(reg)  (reg)->uOffset = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
#define SET_FLOAT_HALF(reg) (reg)->uOffset = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1OVER2;
#define SET_FLOAT_TWO(reg)  (reg)->uOffset = EURASIA_USE_SPECIAL_CONSTANT_FLOAT2;
#define SET_FLOAT_VALUE(reg, hwconstant) reg->uOffset = hwconstant;

#define SET_INT_ZERO(reg)   (reg)->uOffset = 0;
#define SET_INT_ONE(reg)   (reg)->uOffset = 1;
#define SET_INT_VALUE(reg, intvalue) (reg)->uOffset = intvalue;
#define SET_INTSRCSEL(reg, value) (reg)->uOffset = value
#define SET_GLOBALREG_ZERO(reg) (reg)->uOffset = 0
#define SET_GLOBALREG_SEQUENCENUMBER(reg)  (reg)->uOffset = EURASIA_USE_SPECIAL_BANK_SEQUENCENUMBER

#define DP3_ADJUST(a) (a - 2)
#define DP4_ADJUST(a) (a - 3)

#define SMLSI(uInc1, uInc2, uInc3, uInc4, Comment)		\
	{													\
		IMG_UINT i;										\
														\
		psInst->eOpcode          = USEASM_OP_SMLSI;		\
		psFFGenCode->sImmediateIntReg.uOffset = 0;		\
		for (i = 0; i < USE_MAX_ARGUMENTS-1; i++)		\
		{												\
		     SETSRCOFF(i, 0);							\
		     psInst->ppsRegs[i] = &(psFFGenCode->sImmediateIntReg); \
		}												\
		psInst->ppsRegs[i] = &(psFFGenCode->sImmediateIntReg); \
		SETDESTOFF(uInc1);								\
		SETSRCOFF(0, uInc2);							\
		SETSRCOFF(1, uInc3);							\
		SETSRCOFF(2, uInc4);							\
		psInst->uNumRegs         = USE_MAX_ARGUMENTS;	\
		psInst->pszComment       = Comment;				\
		EncodeInstruction(psFFGenCode, psInst);			\
	}

#define SMBO(uBOffset0, uBOffset1, uBOffset2, uBOffset3, Comment)	\
	{																\
		IMG_UINT32 i;												\
																	\
		psInst->eOpcode = USEASM_OP_SMBO;							\
		psFFGenCode->sImmediateIntReg.uOffset = 0;					\
		for(i = 0; i < UseGetOpcodeArgumentCount(USEASM_OP_SMBO); i++)	\
		{																\
			psInst->ppsRegs[i] = &(psFFGenCode->sImmediateIntReg);		\
		}																\
		SETDESTOFF(uBOffset0);											\
		SETSRCOFF(0, uBOffset1);										\
		SETSRCOFF(1, uBOffset2);										\
		SETSRCOFF(2, uBOffset3);										\
		psInst->uNumRegs   = UseGetOpcodeArgumentCount(USEASM_OP_SMBO);	\
		psInst->pszComment = Comment;									\
		EncodeInstruction(psFFGenCode, psInst);							\
	}

#define BR(uLabel)										\
	psFFGenCode->sLabelReg.uOffset = uLabel;			\
	INST0(BR, &psFFGenCode->sLabelReg, IMG_NULL);	

#define BRL(uLabel)										\
	psFFGenCode->sLabelReg.uOffset = uLabel;			\
	psInst->uFlags1 |= USEASM_OPFLAGS1_SAVELINK;		\
	INST0(BR, &psFFGenCode->sLabelReg, IMG_NULL);	

#define LABEL(uLabel)									\
	psFFGenCode->sLabelReg.uOffset = uLabel;			\
	INST0(LABEL, &psFFGenCode->sLabelReg, IMG_NULL);	\

/*********** NEW ONES *****************/

#define SET_TESTPRED_POSITIVE() \
		psInst->uFlags1 |= USEASM_OPFLAGS1_TESTENABLE;                              \
		psInst->uTest   |= ((USEASM_TEST_SIGN_POSITIVE << USEASM_TEST_SIGN_SHIFT) | \
							(USEASM_TEST_ZERO_NONZERO << USEASM_TEST_ZERO_SHIFT)  | \
							(USEASM_TEST_CRCOMB_AND));

#define SET_TESTPRED_NEGATIVE() \
		psInst->uFlags1 |= USEASM_OPFLAGS1_TESTENABLE;                              \
		psInst->uTest   |= ((USEASM_TEST_SIGN_NEGATIVE << USEASM_TEST_SIGN_SHIFT) | \
							(USEASM_TEST_ZERO_NONZERO << USEASM_TEST_ZERO_SHIFT)  | \
							(USEASM_TEST_CRCOMB_AND));

#define SET_TESTPRED_NEGATIVE_OR_ZERO() \
		psInst->uFlags1 |= USEASM_OPFLAGS1_TESTENABLE;                              \
		psInst->uTest   |= ((USEASM_TEST_SIGN_NEGATIVE << USEASM_TEST_SIGN_SHIFT) | \
							(USEASM_TEST_ZERO_ZERO << USEASM_TEST_ZERO_SHIFT));

#define SET_TESTPRED_POSITIVE_OR_ZERO() \
		psInst->uFlags1 |= USEASM_OPFLAGS1_TESTENABLE;                              \
		psInst->uTest   |= ((USEASM_TEST_SIGN_POSITIVE << USEASM_TEST_SIGN_SHIFT) | \
							(USEASM_TEST_ZERO_ZERO << USEASM_TEST_ZERO_SHIFT));

#define SET_TESTPRED_NONZERO() \
		psInst->uFlags1 |= USEASM_OPFLAGS1_TESTENABLE;                              \
		psInst->uTest   |= ((USEASM_TEST_ZERO_NONZERO << USEASM_TEST_ZERO_SHIFT)  | \
							(USEASM_TEST_CRCOMB_AND));

#define SET_TESTPRED_ZERO() \
		psInst->uFlags1 |= USEASM_OPFLAGS1_TESTENABLE;                              \
		psInst->uTest   |= ((USEASM_TEST_ZERO_ZERO << USEASM_TEST_ZERO_SHIFT)  | \
							(USEASM_TEST_CRCOMB_AND));

#define SET_TEST_CHANNEL(channel) \
		psInst->uTest   |= ((USEASM_TEST_CHANSEL_C0 + channel) <<  USEASM_TEST_CHANSEL_SHIFT);

#define SET_REPEAT_COUNT(uRepeatCount) \
        psInst->uFlags1 &= USEASM_OPFLAGS1_REPEAT_CLRMSK; \
        psInst->uFlags1 |= uRepeatCount << USEASM_OPFLAGS1_REPEAT_SHIFT;

#define GET_REPEAT_COUNT(psInst) ((psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT)

#define SET_FETCH_COUNT(uFetchCount)	\
		psInst->uFlags1 |= USEASM_OPFLAGS1_FETCHENABLE;		\
		SET_REPEAT_COUNT(uFetchCount);

#define SET_POST_INCREMENTAL_LOADSTORE() \
		psInst->uFlags1 |= USEASM_OPFLAGS1_FETCHENABLE;   \
		psInst->uFlags1 |= USEASM_OPFLAGS1_POSTINCREMENT;

#define SET_PRE_INCREMENTAL_LOADSTORE() \
		psInst->uFlags1 |= USEASM_OPFLAGS1_FETCHENABLE;   \
		psInst->uFlags1 |= USEASM_OPFLAGS1_PREINCREMENT;

#define SET_FORCE_CACHELINEFILL_LOAD()	\
		psInst->uFlags2 |= USEASM_OPFLAGS2_FORCELINEFILL;

#define USE_PRED(psPredReg) \
		psFFGenCode->eCodeGenFlags |= FFGENCGF_DYNAMIC_FLOW_CONTROL; \
		psInst->uFlags1 &= USEASM_OPFLAGS1_PRED_CLRMSK; \
		psInst->uFlags1 |= (USEASM_PRED_P0 + psPredReg->uOffset) << USEASM_OPFLAGS1_PRED_SHIFT;

#define USE_PRED23(psPredReg) \
		psFFGenCode->eCodeGenFlags |= FFGENCGF_DYNAMIC_FLOW_CONTROL; \
		psInst->uFlags1 &= USEASM_OPFLAGS1_PRED_CLRMSK; \
		psInst->uFlags1 |= (USEASM_PRED_P2 + psPredReg->uOffset - 2) << USEASM_OPFLAGS1_PRED_SHIFT;


#define USE_NEGPRED(psPredReg) \
		psFFGenCode->eCodeGenFlags |= FFGENCGF_DYNAMIC_FLOW_CONTROL; \
		psInst->uFlags1 &= USEASM_OPFLAGS1_PRED_CLRMSK; \
		psInst->uFlags1 |= (USEASM_PRED_NEGP0 + psPredReg->uOffset) << USEASM_OPFLAGS1_PRED_SHIFT;

#define GET_PRED(psInst) ((psInst->uFlags1 & ~USEASM_OPFLAGS1_PRED_CLRMSK) >> USEASM_OPFLAGS1_PRED_SHIFT)

#define SET_NOSCHED() \
		psInst->uFlags1 |= USEASM_OPFLAGS1_NOSCHED;

#define SET_FREEP() \
		psInst->uFlags3 |= USEASM_OPFLAGS3_FREEP;

#define SET_END() \
		psInst->uFlags1 |= USEASM_OPFLAGS1_END;

#endif //  __gl_macros_h_

/******************************************************************************
 End of file (macros.h)
******************************************************************************/


