 /*****************************************************************************
 Name			: HWINST.H
 
 Title			: Interface for HWINST.C
 
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

 Description 	: Definitions and prototypes for structures and functions
				  from HWINST.C, for encodeing and decode HW instructions
   
 Program Type	: 32-bit DLL

 Version	 	: $Revision: 1.56 $

 Modifications	:

 $Log: hwinst.h $
*****************************************************************************/
#ifndef _HWINST_H_
#define _HWINST_H_

/*
	Structure representing a USSE (HW) instruction
*/
struct _HW_INST_
{
	IMG_UINT32	uWord0;
	IMG_UINT32	uWord1;
};

/*
	Decoded HW instruciton opcodes (and significant instruction variants)
*/
typedef enum _USP_OPCODE_
{
	USP_OPCODE_INVALID		= 0,
	#if !defined(SGX_FEATURE_USE_VEC34)
	USP_OPCODE_MAD			= 1,
	USP_OPCODE_ADM			= 2,
	USP_OPCODE_MSA			= 3,
	USP_OPCODE_FRC			= 4,
	USP_OPCODE_RCP			= 5,
	USP_OPCODE_RSQ			= 6,
	USP_OPCODE_LOG			= 7,
	USP_OPCODE_EXP			= 8,
	USP_OPCODE_DP			= 9,
	USP_OPCODE_DDP			= 10,
	USP_OPCODE_DDPC			= 11,
	USP_OPCODE_MIN			= 12,
	USP_OPCODE_MAX			= 13,
	USP_OPCODE_DSX			= 14,
	USP_OPCODE_DSY			= 15,
	USP_OPCODE_MOVC			= 16,
	USP_OPCODE_FMAD16		= 17,
	USP_OPCODE_EFO			= 18,
	#endif /* !defined(SGX_FEATURE_USE_VEC34) */
	USP_OPCODE_PCKUNPCK		= 19,
	USP_OPCODE_TEST			= 20,
	USP_OPCODE_AND			= 21,
	USP_OPCODE_OR			= 22,
	USP_OPCODE_XOR			= 23,
	USP_OPCODE_SHL			= 24,
	USP_OPCODE_ROL			= 25,
	USP_OPCODE_SHR			= 26,
	USP_OPCODE_ASR			= 27,
	USP_OPCODE_RLP			= 28,
	USP_OPCODE_TESTMASK		= 29,
	USP_OPCODE_SOP2			= 30,
	USP_OPCODE_SOP3			= 31,
	USP_OPCODE_SOPWM		= 32,
	USP_OPCODE_IMA8			= 33,
	USP_OPCODE_IMA16		= 34,
	USP_OPCODE_IMAE			= 35,
	USP_OPCODE_ADIF			= 36,
	USP_OPCODE_BILIN		= 37,
	USP_OPCODE_FIRV			= 38,
	USP_OPCODE_FIRH			= 39,
	USP_OPCODE_DOT3			= 40,
	USP_OPCODE_DOT4			= 41,
	USP_OPCODE_FPMA			= 42,
	USP_OPCODE_SMP			= 43,
	USP_OPCODE_SMPBIAS		= 44,
	USP_OPCODE_SMPREPLACE	= 45,
	USP_OPCODE_SMPGRAD		= 46,
	USP_OPCODE_LD			= 47,
	USP_OPCODE_ST			= 48,
	USP_OPCODE_BA			= 49,
	USP_OPCODE_BR			= 50,
	USP_OPCODE_LAPC			= 51,
	USP_OPCODE_SETL			= 52,
	USP_OPCODE_SAVL			= 53,
	USP_OPCODE_NOP			= 54,
	USP_OPCODE_SMOA			= 55,
	USP_OPCODE_SMR			= 56,
	USP_OPCODE_SMLSI		= 57,
	USP_OPCODE_SMBO			= 58,
	USP_OPCODE_IMO			= 59,
	USP_OPCODE_SETFC		= 60,
	USP_OPCODE_IDF			= 61,
	USP_OPCODE_WDF			= 62,
	USP_OPCODE_SETM			= 63,
	USP_OPCODE_EMIT			= 64,
	USP_OPCODE_LIMM			= 65,
	USP_OPCODE_LOCK			= 66,
	USP_OPCODE_RELEASE		= 67,
	USP_OPCODE_LDR			= 68,
	USP_OPCODE_STR			= 69,
	USP_OPCODE_WOP			= 70,
	USP_OPCODE_PCOEFF		= 71,
	USP_OPCODE_PTOFF		= 72,
	USP_OPCODE_ATST8		= 73,
	USP_OPCODE_DEPTHF		= 74,
	#if defined(SGX_FEATURE_USE_NORMALISE)
	USP_OPCODE_FNRM			= 75,
	#endif /* #ifdef SGX_FEATURE_USE_NORMALISE */
	#if defined(SGX545)
	USP_OPCODE_DUAL         = 76,
	#endif /* defined(SGX545) */
	#if defined(SGX_FEATURE_USE_VEC34)
	USP_OPCODE_VMAD			= 77,
	USP_OPCODE_VMUL			= 78,
	USP_OPCODE_VADD			= 79,
	USP_OPCODE_VFRC			= 80,
	USP_OPCODE_VDSX			= 81,
	USP_OPCODE_VDSY			= 82,
	USP_OPCODE_VMIN			= 83,
	USP_OPCODE_VMAX			= 84,
	USP_OPCODE_VDP			= 85,
	USP_OPCODE_VDP3			= 86,
	USP_OPCODE_VDP4			= 87,
	USP_OPCODE_VMAD3		= 88,
	USP_OPCODE_VMAD4		= 89,
	USP_OPCODE_VDUAL		= 90,
	USP_OPCODE_VRCP			= 91,
	USP_OPCODE_VRSQ			= 92,
	USP_OPCODE_VLOG			= 93,
	USP_OPCODE_VEXP			= 94,
	USP_OPCODE_VMOV			= 95,
	#endif /* defined(SGX_FEATURE_USE_VEC34) */
	USP_OPCODE_PREAMBLE		= 96,
	#if defined(SGX_FEATURE_USE_IDXSC)
	USP_OPCODE_IDXSCR		= 97,
	USP_OPCODE_IDXSCW		= 98,
	#endif /* defined(SGX_FEATURE_USE_IDXSC) */
	#if defined(SGX_FEATURE_USE_32BIT_INT_MAD)
	USP_OPCODE_IMA32		= 99,
	#endif /* defined(SGX_FEATURE_USE_32BIT_INT_MAD) */
}USP_OPCODE, *PUSP_OPCODE;

/*
	Decoded register-types for HW operands
*/
typedef enum _USP_REGTYPE_
{
	USP_REGTYPE_TEMP		= 0,
	USP_REGTYPE_PA			= 1,
	USP_REGTYPE_SA			= 2,
	USP_REGTYPE_OUTPUT		= 3,
	USP_REGTYPE_INTERNAL	= 4,
	USP_REGTYPE_INDEX		= 5,
	USP_REGTYPE_SPECIAL		= 6,
	USP_REGTYPE_IMM			= 7,
}USP_REGTYPE, *PUSP_REGTYPE;

/*
	Decoded register data-formats (i.e. the format the HW will read/write)
*/
typedef enum _USP_REGFMT_
{
	USP_REGFMT_UNKNOWN	= 0,
	USP_REGFMT_F32		= 1,
	USP_REGFMT_F16		= 2,
	USP_REGFMT_C10		= 3,
	USP_REGFMT_U8		= 4,	
	USP_REGFMT_U16		= 5,
	USP_REGFMT_I16		= 6,
	USP_REGFMT_U32		= 7,
	USP_REGFMT_I32		= 8
} USP_REGFMT, *PUSP_REGFMT;

/*
	Decoded dynamic-indexing options for an instruction operand
*/
typedef enum _USP_DYNIDX_
{
	USP_DYNIDX_NONE	= 0,
	USP_DYNIDX_IL	= 1,
	USP_DYNIDX_IH	= 2
}USP_DYNIDX, *PUSP_DYNIDX;

/*
	Data describing a generic operand used by an instruction
*/
struct _USP_REG_
{
	USP_REGTYPE	eType;
	IMG_UINT32	uNum;
	USP_REGFMT	eFmt;
	IMG_UINT32	uComp;
	USP_DYNIDX	eDynIdx;
};

/*
	Per-operand format selection control options
*/
typedef enum _USP_FMTCTL_
{
	USP_FMTCTL_NONE			= 0,	
	USP_FMTCTL_F32_OR_F16	= 1,
	USP_FMTCTL_U8_OR_C10	= 2
}USP_FMTCTL, *PUSP_FMTCTL;

/*
	Source/dest data formats for the PCKUNPCK instruction
*/
typedef enum _USP_PCKUNPCK_FMT_
{
	USP_PCKUNPCK_FMT_INVALID	= 0,
	USP_PCKUNPCK_FMT_U8			= 1,
	USP_PCKUNPCK_FMT_S8			= 2,
	USP_PCKUNPCK_FMT_O8			= 3,
	USP_PCKUNPCK_FMT_U16		= 4,
	USP_PCKUNPCK_FMT_S16		= 5,
	USP_PCKUNPCK_FMT_F16		= 6,
	USP_PCKUNPCK_FMT_F32		= 7,
	USP_PCKUNPCK_FMT_C10		= 8
}USP_PCKUNPCK_FMT, *PUSP_PCKUNPCK_FMT;

typedef enum _USP_MOVC_TESTDTYPE_
{
	USP_MOVC_TESTDTYPE_INVALID	= 0,
	USP_MOVC_TESTDTYPE_NONE		= 1,
	USP_MOVC_TESTDTYPE_INT8		= 2,
	USP_MOVC_TESTDTYPE_INT16	= 3,
	USP_MOVC_TESTDTYPE_INT32	= 4,
	USP_MOVC_TESTDTYPE_FLOAT32	= 5,
	USP_MOVC_TESTDTYPE_INT10	= 6
}USP_MOVC_TESTDTYPE, *PUSP_MOVC_TESTDTYPE;

typedef enum _USP_REPEAT_MODE_
{
	USP_REPEAT_MODE_NONE	= 0,
	USP_REPEAT_MODE_MASK	= 1,
	USP_REPEAT_MODE_REPEAT	= 2,
	USP_REPEAT_MODE_FETCH	= 3
}USP_REPEAT_MODE, *PUSP_REPEAT_MODE;
 
/*
	Instruction operand indices/flags
*/
#define USP_OPERANDIDX_DST		(0)
#define USP_OPERANDIDX_SRC0		(1)
#define USP_OPERANDIDX_SRC1		(2)
#define USP_OPERANDIDX_SRC2		(3)
#define USP_OPERANDIDX_MAX		(3)

#define USP_OPERANDFLAG_DST		(1 << USP_OPERANDIDX_DST)
#define USP_OPERANDFLAG_SRC0	(1 << USP_OPERANDIDX_SRC0)
#define USP_OPERANDFLAG_SRC1	(1 << USP_OPERANDIDX_SRC1)
#define USP_OPERANDFLAG_SRC2	(1 << USP_OPERANDIDX_SRC2)

/*
	Maximum repeat/fetch count for LD instructions
*/
#define HW_INST_LD_MAX_REPEAT_COUNT		((~EURASIA_USE1_RMSKCNT_CLRMSK) >> EURASIA_USE1_RMSKCNT_SHIFT)

/*
	Maximum repeat count for EFO instructions
*/
#define HW_INST_EFO_MAX_REPEAT_COUNT	((~EURASIA_USE1_EFO_RCOUNT_CLRMSK) >> EURASIA_USE1_EFO_RCOUNT_SHIFT)

/*
	Decode the opcode for a HW instruction
*/
IMG_BOOL HWInstGetOpcode(PHW_INST psHWInst, PUSP_OPCODE peOpcode);

/*
	Determine which operands will be used by a HW instruction
	(see HW_OPERANDFLAG_xxx)
*/
IMG_BOOL HWInstGetOperandsUsed(USP_OPCODE	eOpcode,
							   PHW_INST		psHWInst,
							   IMG_PUINT32	puOperandsUsed);

/*
	Determine whether per-operand control over data-format is enabled for
	an instruction
*/
IMG_BOOL HWInstGetPerOperandFmtCtl(PUSP_MOESTATE	psMOEState,
								   USP_OPCODE		eOpcode,
								   PHW_INST			psHWInst,
								   PUSP_FMTCTL		peFmtCtl);

/*
	Indicates whether a HW instruction supports extended source banks
*/
IMG_BOOL HWInstCanUseExtSrc0Banks(USP_OPCODE eOpcode);

/*
	Check whether a specific register-type is an extended source 0 bank
*/
IMG_BOOL HWInstIsExtSrc0Bank(USP_REGTYPE eRegType);

/*
	Indicates whether an instruction effects the MOE state
*/
IMG_BOOL HWInstIsMOEControlInst(USP_OPCODE eOpcode);

/*
	Update MOE state to reflect a MOE-control instruction
*/
IMG_BOOL HWInstUpdateMOEState(USP_OPCODE	eOpcode,
							  PHW_INST		psHWInst,
							  PUSP_MOESTATE	psMOEState);

/*
	Encode the destination register bank and number used by a HW instruction
*/
IMG_BOOL HWInstEncodeDestBankAndNum(USP_FMTCTL	eFmtCtl,
							  		USP_OPCODE	eOpcode,
									PHW_INST	psHWInst,
									PUSP_REG	psReg);

/*
	Decode the destination register bank and number of a HW instruction
*/
IMG_BOOL HWInstDecodeDestBankAndNum(USP_FMTCTL	eFmtCtl,
									USP_OPCODE	eOpcode,
							  		PHW_INST	psHWInst,
							  		PUSP_REG	psReg);

/*
	Encode the source 0 register bank and number used by a HW inst instruction
*/
IMG_BOOL HWInstEncodeSrc0BankAndNum(USP_FMTCTL	eFmtCtl,
									USP_OPCODE	eOpcode,
									IMG_BOOL	bCanUseExtBanks,
							  		PHW_INST	psHWInst,
									PUSP_REG	psReg);

/*
	Decode the source 0 register bank and number used by a HW instruction
*/
IMG_BOOL HWInstDecodeSrc0BankAndNum(USP_FMTCTL	eFmtCtl,
								  	IMG_BOOL	bCanUseExtBanks,
							  		PHW_INST	psHWInst,
							  		PUSP_REG	psReg);

/*
	Encode the source 1 register bank and number used by a HW instruction
*/
IMG_BOOL HWInstEncodeSrc1BankAndNum(USP_FMTCTL	eFmtCtl,
									USP_OPCODE	eOpcode,
							  		PHW_INST	psHWInst,
									PUSP_REG	psReg);

/*
	Decode the source 1 register bank and number from a HW instruction
*/
IMG_BOOL HWInstDecodeSrc1BankAndNum(USP_FMTCTL	eFmtCtl,
							  		PHW_INST	psHWInst,
							  		PUSP_REG	psReg);

/*
	Encode the source 2 register bank and number used by a HW instruction
*/
IMG_BOOL HWInstEncodeSrc2BankAndNum(USP_FMTCTL	eFmtCtl,
									USP_OPCODE	eOpcode,
							  		PHW_INST	psHWInst,
									PUSP_REG	psReg);

/*
	Decode the source 2 register bank and number from a HW instruction
*/
IMG_BOOL HWInstDecodeSrc2BankAndNum(USP_FMTCTL	eFmtCtl,
							  		PHW_INST	psHWInst,
							  		PUSP_REG	psReg);

/*
	Decode the register banck and number for an operand from a HH instruction
*/
IMG_BOOL HWInstDecodeOperandBankAndNum(PUSP_MOESTATE	psMOEState,
						  			   USP_OPCODE		eOpcode,
						  			   PHW_INST			psHWInst,
						  			   IMG_UINT32		uOperandIdx,
						  			   PUSP_REG			psReg);

/*
	Decode the register bnck and number for an operand from a texture sample instruction
*/
IMG_BOOL HWInstDecodeSMPOperandBankAndNum(PUSP_MOESTATE	psMOEState,
										  USP_OPCODE	eOpcode,
										  PHW_INST		psHWInst,
										  IMG_UINT32	uOperandIdx,
										  PUSP_REG		psReg);

/*
	Encode the register banck and number for an operand from a HW instruction
*/
IMG_BOOL HWInstEncodeOperandBankAndNum(PUSP_MOESTATE	psMOEState,
						  			   USP_OPCODE		eOpcode,
						  			   PHW_INST			psHWInst,
						  			   IMG_UINT32		uOperandIdx,
						  			   PUSP_REG			psReg);

/*
	Encode the offset for a relative branch
*/
IMG_BOOL HWInstSetBranchOffset(PHW_INST	psHWInst, IMG_INT32 iOffset);

/*
	Encode the address for an absolute branch
*/
IMG_BOOL HWInstSetBranchAddr(PHW_INST psHWInst, IMG_INT32 iAddr);

/*
	Determine whether a HW instruction supports the NoSched flag
*/
IMG_BOOL HWInstSupportsNoSched(USP_OPCODE eOpcode);

/*
	Set the NoSched flag on a HW instruction
*/
IMG_BOOL HWInstSetNoSched(USP_OPCODE	eOpcode,
						  PHW_INST		psHWInst,
						  IMG_BOOL		bNoSchedState);

/*
	Determine whether a HW instruction needs the sync-start flag set on the
	preceeding instruction
*/
IMG_BOOL HWInstNeedsSyncStartBefore(USP_OPCODE eOpcode, PHW_INST psHWInst);

/*
	Determine whether a HW instruction supports the SyncStart flag
*/
IMG_BOOL HWInstSupportsSyncStart(USP_OPCODE eOpcode);

/*
	Get the state of the SyncStart flag for an instruction
*/
IMG_BOOL HWInstGetSyncStart(USP_OPCODE	eOpcode,
							PHW_INST	psHWInst,
							IMG_BOOL*	pbSyncStartState);

/*
	Set the state of the SyncStart flag on an instruction
*/
IMG_BOOL HWInstSetSyncStart(USP_OPCODE	eOpcode,
							PHW_INST	psHWInst,
							IMG_BOOL	bSyncStartState);

/*
	Determine whether a HW instruction supports the SyncEnd flag
*/
IMG_BOOL HWInstSupportsSyncEnd(USP_OPCODE eOpcode);

/*
	Get the state of the SyncEnd flag for an instruction
*/
IMG_BOOL HWInstGetSyncEnd(USP_OPCODE	eOpcode,
						  PHW_INST		psHWInst,
						  IMG_BOOL*		pbSyncEndState);

/*
	Determine whether a HW instruciton forces a deschedule
*/
IMG_BOOL HWInstForcesDeschedule(USP_OPCODE eOpcode, PHW_INST psHWInst);

/*
	Check for an illegal combination of instructions
*/
IMG_BOOL HWInstIsIllegalInstPair(USP_OPCODE eOpcode1, PHW_INST psHWInst1,
								 USP_OPCODE eOpcode2, PHW_INST psHWInst2);

/*
	Test whether a spceific HW instruction supports the END flag
*/
IMG_BOOL HWInstSupportsEnd(USP_OPCODE eOpcode);

/*
	Encode a NOP HW instruction
*/
IMG_UINT32 HWInstEncodeNOPInst(PHW_INST psHWInst);

/*
	Encode a bitwise AND instruction
*/
IMG_UINT32 HWInstEncodeANDInst(PHW_INST		psHWInst,
							   IMG_UINT32	uRepeatCount,
							   IMG_BOOL		bUsePred,
							   IMG_BOOL		bNegatePred,
							   IMG_UINT32	uPredNum,
							   IMG_BOOL		bSkipInv,
							   PUSP_REG		psDestReg,
							   PUSP_REG		psSrc1Reg,
							   PUSP_REG		psSrc2Reg);

/*
	Encode a LIMM instruction
*/
IMG_INTERNAL IMG_UINT32 HWInstEncodeLIMMInst(PHW_INST		psHWInst,
											 IMG_UINT32		uRepeatCount,
											 PUSP_REG		psDestReg,
											 IMG_UINT32		uValue);

/*
	Encode a TEST instruction
*/
IMG_INTERNAL IMG_UINT32 HWInstEncodeTESTInst(PHW_INST		psHWInst,
											IMG_UINT32		uRepeatCount,
											IMG_UINT32		dwFlags,
											PUSP_REG		psDest,
											PUSP_REG		psSrc0,
											PUSP_REG		psSrc1,
											IMG_UINT32		dwPDST,
											IMG_UINT32		dwSTST,
											IMG_UINT32		dwZTST,
											IMG_UINT32		dwCHANCC,
											IMG_UINT32		dwALUSEL,
											IMG_UINT32		dwALUOP);

/*
	Encode a conditional MOV instruction
*/
IMG_INTERNAL IMG_UINT32 HWInstEncodeMOVCInst(PHW_INST	psHWInst,
											 IMG_UINT32	uRepeatCount,
											 IMG_UINT32 uPred,
											 IMG_BOOL	bTestDst,
											 IMG_BOOL	bSkipInv,
											 PUSP_REG	psDestReg,
											 PUSP_REG	psSrc0Reg,
											 PUSP_REG	psSrc1Reg,
											 PUSP_REG	psSrc2Reg);

/*
	Encode a bitwise OR instruction
*/
IMG_INTERNAL IMG_UINT32 HWInstEncodeORInst(PHW_INST		psHWInst,
										   IMG_UINT32	uRepeatCount,
										   IMG_BOOL		bSkipInv,
										   PUSP_REG		psDestReg,
										   PUSP_REG		psSrc1Reg,
										   PUSP_REG		psSrc2Reg);

/*
	Encode a bitwise XOR instruction
*/
IMG_INTERNAL IMG_UINT32 HWInstEncodeXORInst(PHW_INST		psHWInst,
											IMG_UINT32		uRepeatCount,
											IMG_UINT32		uPred,
											IMG_BOOL		bSkipInv,
											PUSP_REG		psDestReg,
											PUSP_REG		psSrc1Reg,
											PUSP_REG		psSrc2Reg);

/*
	Encode a SHL instruction
*/
IMG_INTERNAL IMG_UINT32 HWInstEncodeSHLInst(PHW_INST		psHWInst,
											IMG_UINT32		uRepeatCount,
											IMG_BOOL		bSkipInv,
											PUSP_REG		psDestReg,
											PUSP_REG		psSrc1Reg,
											PUSP_REG		psSrc2Reg);

/*
	Encode a SHR instruction
*/
IMG_INTERNAL IMG_UINT32 HWInstEncodeSHRInst(PHW_INST		psHWInst,
											IMG_UINT32		uRepeatCount,
											IMG_BOOL		bSkipInv,
											PUSP_REG		psDestReg,
											PUSP_REG		psSrc1Reg,
											PUSP_REG		psSrc2Reg);

/*
	Encode an IMA16 instruction
*/
IMG_INTERNAL IMG_UINT32 HWInstEncodeIMA16Inst(PHW_INST		psHWInst,
										 	  IMG_UINT32	uRepeatCount,
											  PUSP_REG		psDestReg,
											  PUSP_REG		psSrc0Reg,
											  PUSP_REG		psSrc1Reg,
											  PUSP_REG		psSrc2Reg);

/*
	Encode an IMAE instruction
*/
IMG_INTERNAL IMG_UINT32 HWInstEncodeIMAEInst(PHW_INST		psHWInst,
										 	 IMG_UINT32		uRepeatCount,
											 PUSP_REG		psDestReg,
											 PUSP_REG		psSrc0Reg,
											 PUSP_REG		psSrc1Reg,
											 PUSP_REG		psSrc2Reg);

/*
	Encode an STAD instruction
*/
IMG_INTERNAL IMG_UINT32 HWInstEncodeSTInst(PHW_INST		psHWInst,
										   IMG_BOOL		bSkipInv,
										   IMG_UINT32	uRepeatCount,
										   PUSP_REG		psBaseAddrReg,
										   PUSP_REG		psAddrOffReg,
										   PUSP_REG		psDataReg);

#if !defined(SGX_FEATURE_USE_VEC34)
IMG_INTERNAL IMG_UINT32 HWInstEncodeFRCInst(PHW_INST   psHWInst,
                                            IMG_BOOL   bSkipInv,
                                            IMG_UINT32 uRepeatCount,
                                            PUSP_REG   psDestReg,
                                            PUSP_REG   psSrc1Reg,
                                            PUSP_REG   psSrc2Reg);

IMG_INTERNAL IMG_UINT32 HWInstEncodeFMADInst(PHW_INST   psHWInst,
                                             IMG_BOOL   bSkipInv,
                                             IMG_UINT32 uRepeatCount,
                                             PUSP_REG   psDestReg,
                                             PUSP_REG   psSrc0Reg,
                                             PUSP_REG   psSrc1Reg,
                                             PUSP_REG   psSrc2Reg);
#else
IMG_INTERNAL IMG_UINT32 HWInstEncodeVMADInst(PHW_INST   psHWInst,
                                             IMG_BOOL   bSkipInv,
                                             IMG_UINT32 uRepeatCount,
                                             PUSP_REG   psDestReg,
											 IMG_UINT32 ui32WMask,
                                             PUSP_REG   psSrc0Reg,
											 IMG_UINT32	ui32Src0Swiz,
                                             PUSP_REG   psSrc1Reg,
											 IMG_UINT32 ui32Src1Swiz,
                                             PUSP_REG   psSrc2Reg,
											 IMG_UINT32 ui32Src2Swiz);

IMG_INTERNAL IMG_UINT32 HWInstEncodeVFRCInst(PHW_INST   psHWInst,
                                             IMG_BOOL   bSkipInv,
                                             IMG_UINT32 uRepeatCount,
                                             PUSP_REG   psDestReg,
                                             PUSP_REG   psSrc1Reg,
											 IMG_UINT32 ui32Src1Swiz,
                                             PUSP_REG   psSrc2Reg,
											 IMG_UINT32 ui32Src2Swiz);
#endif /* !defined(SGX_FEATURE_USE_VEC34) */

IMG_INTERNAL IMG_UINT32 HWInstEncodeIDFInst(PHW_INST psHWInst);

/*
	Encode an unconditional MOV instruction
*/
IMG_UINT32 HWInstEncodeMOVInst(PHW_INST		psHWInst,
							   IMG_UINT32	uRepeatCount,
							   IMG_BOOL		bSkipInv,
							   PUSP_REG		psDestReg,
							   PUSP_REG		psSrc1Reg);

/*
	Decode the test data-type used by a MOVC instruction
*/
IMG_BOOL HWInstDecodeMOVInstTestDataType(PHW_INST				psHWInst,
										 PUSP_MOVC_TESTDTYPE	peTestType);

/*
	Decode the repeat options for an instruction
*/
IMG_BOOL HWInstDecodeRepeat(PHW_INST			psHWInst,
							USP_OPCODE			eOpcode,
							PUSP_REPEAT_MODE	peMode,
							IMG_PUINT32			puRepeat);

/*
	Encode an SMLSI HW-instruction
*/
IMG_UINT32 HWInstEncodeSMLSIInst(PHW_INST		psHWInst,
							     IMG_PBOOL		pbUseSwiz,
							     IMG_PINT32		piInc,
							     IMG_PUINT32	puSwiz,
							     IMG_PUINT32	puLimit);

/*
	Encode an SMBO HW-instruction
*/
IMG_UINT32 HWInstEncodeSMBOInst(PHW_INST psHWInst, IMG_PUINT32 puBaseOffsets);

/*
	Encode an SETFC HW-instruction
*/
IMG_UINT32 HWInstEncodeSETFCInst(PHW_INST	psHWInst,
							     IMG_BOOL	bColFmtCtl,
							     IMG_BOOL	bEFOFmtCtl);

/*
	Encode a PCKUNPCK instruction
*/
IMG_UINT32 HWInstEncodePCKUNPCKInst(PHW_INST			psHWInst,
							        IMG_BOOL			bSkipInv,
				  					USP_PCKUNPCK_FMT	eDstFmt,
				  					USP_PCKUNPCK_FMT	eSrcFmt,
							        IMG_BOOL			bScaleEnable,
							        IMG_UINT32			uWriteMask,
							        PUSP_REG			psDestReg,
									IMG_UINT32			uSrc1Select,
									IMG_UINT32			uSrc2Select,
								    PUSP_REG			psSrc1Reg,
								    PUSP_REG			psSrc2Reg);

/*
	Test whether a HW instruction supports partial writes
*/
IMG_BOOL HWInstSupportsWriteMask(USP_OPCODE eOpcode);

/*
	Decode the write-mask for a PCKUNPCK instruciton
*/
IMG_UINT32 HWInstDecodePCKUNPCKInstWriteMask(PHW_INST psHWInst);

/*
	Decode the write-mask for a SOPWM instruciton
*/
IMG_UINT32 HWInstDecodeSOPWMInstWriteMask(PHW_INST psHWInst);

/*
	Decode the write-mask for an instruciton that supports partial-writes to
	registers
*/
IMG_BOOL HWInstDecodeWriteMask(USP_OPCODE	eOpcode,
							   PHW_INST		psHWInst,
							   IMG_PUINT32	puWriteMask);

/*
	Encode the write-mask for a PCKUNPCK instruciton
*/
IMG_BOOL HWInstEncodePCKUNPCKInstWriteMask(PHW_INST		psHWInst,
										   IMG_UINT32	uWriteMask,
										   IMG_BOOL		bForceMask);

/*
	Encode the write-mask for a SOPWM instruciton
*/
IMG_BOOL HWInstEncodeSOPWMInstWriteMask(PHW_INST	psHWInst,
										IMG_UINT32	uWriteMask);

/*
	Encode the write-mask for an instruciton that supports partial-writes to
	registers
*/
IMG_BOOL HWInstEncodeWriteMask(USP_OPCODE	eOpcode,
							   PHW_INST		psHWInst,
							   IMG_UINT32	uWriteMask);

/*
	Force an instruction that performs a partial-write to completely initialise
	the destination (i.e. use a full write-mask)
*/
IMG_BOOL HWInstForceFullWriteMask(USP_OPCODE	eOpcode, 
								  PHW_INST		psHWInst,
								  IMG_PBOOL		pbHasFullMask);

/*
	Decode the DRC number used by a SMP instruction
*/
IMG_UINT32 HWInstDecodeSMPInstDRCNum(PHW_INST psHWInst);

/*
	HWInstEncodeWDFInst
*/
IMG_UINT32 HWInstEncodeWDFInst(PHW_INST		psHWInst,
							   IMG_UINT32	uDRCNum);

/*
	Encode a SOPWM instruction
*/
IMG_UINT32 HWInstEncodeSOPWMInst(PUSP_MOESTATE	psMOEState,
								 PHW_INST		psHWInst,
								 IMG_BOOL		bSkipInv,
								 IMG_UINT32		uHWCop,
								 IMG_UINT32		uHWAop,
								 IMG_UINT32		uHWMod1,
								 IMG_UINT32		uHWSel1,
								 IMG_UINT32		uHWMod2,
								 IMG_UINT32		uHWSel2,
							  	 IMG_UINT32		uWriteMask,
							  	 PUSP_REG		psDestReg,
								 PUSP_REG		psSrc1Reg,
								 PUSP_REG		psSrc2Reg);

/*
	Encode a SOP2 instruction
*/
IMG_UINT32 HWInstEncodeSOP2Inst(PUSP_MOESTATE	psMOEState,
								PHW_INST		psHWInst,
								IMG_BOOL		bSkipInv,
								IMG_UINT32		uHWCOp,
								IMG_UINT32		uHWCMod1,
								IMG_UINT32		uHWCSel1,
								IMG_UINT32		uHWCMod2,
								IMG_UINT32		uHWCSel2,
								IMG_UINT32		uHWAOp,
								IMG_UINT32		uHWAMod1,
								IMG_UINT32		uHWASel1,
								IMG_UINT32		uHWAMod2,
								IMG_UINT32		uHWASel2,
							  	PUSP_REG		psDestReg,
								PUSP_REG		psSrc1Reg,
								PUSP_REG		psSrc2Reg);

/*
	Decode the data-format for the texture-coordinates used by a SMP
	instruction
*/
USP_REGFMT HWInstDecodeSMPInstCoordFmt(PHW_INST psHWInst);

/*
	Decode the dimensionality of the texture-coordinates used by a SMP 
	instruction
*/
IMG_UINT32 HWInstDecodeSMPInstCoordDim(PHW_INST psHWInst);

/*
	Decode the source format for a pack/unpack HW instruction
*/
USP_PCKUNPCK_FMT HWInstDecodePCKUNPCKInstSrcFormat(PHW_INST psHWInst);

/*
	Decode the destination format for a pack/unpack HW instruction
*/
USP_PCKUNPCK_FMT HWInstDecodePCKUNPCKInstDstFormat(PHW_INST psHWInst);

/*
	Encode a basic LD instruction (absolute addressing or 32-bit data)
*/
IMG_UINT32 HWInstEncodeLDInst(PHW_INST		psHWInst,
							  IMG_BOOL		bSkipInv,
							  IMG_UINT32	uFetchCount,
							  PUSP_REG		psDestReg,
							  PUSP_REG		psBaseAddrReg,
							  PUSP_REG		psAddrOffReg,
							  IMG_UINT32	uDRCNum);

/*
	Encode the fetch/repeat count for a LD instruction
*/
IMG_BOOL HWInstEncodeLDInstFetchCount(PHW_INST	psHWInst, 
									  IMG_UINT32	uFetchCount);

/*
	Encode an basic FIRH instruction (unpredicated, unrepeated)
*/

typedef enum _USP_SRC_FORMAT_
{
	USP_SRC_FORMAT_U8	= 0,
	USP_SRC_FORMAT_S8	= 1	
}USP_SRC_FORMAT, *PUSP_SRC_FORMAT;

IMG_UINT32 HWInstEncodeFIRHInst(PHW_INST		psHWInst,
								IMG_BOOL		bSkipInv,
								USP_SRC_FORMAT	eSrcFormat,
								IMG_UINT32		uCoeffSel,
								IMG_INT32		iSOff,
								IMG_UINT32		uPOff,
								PUSP_REG		psDestReg,
								PUSP_REG		psSrc0Reg,
								PUSP_REG		psSrc1Reg,
								PUSP_REG		psSrc2Reg);

#if defined(SGX_FEATURE_USE_VEC34)
/* 
	Encodes the unpacking format for SMP instruction on SGX543
*/
IMG_INTERNAL IMG_BOOL HWInstEncodeSMPInstUnPackingFormat(PHW_INST	psHWInst, 
														 USP_REGFMT	eUnPckFmt);
/* 
	Encodes vector unpacking instruction on SGX543
*/
IMG_INTERNAL IMG_UINT32 HWInstEncodeVectorPCKUNPCKInst(PHW_INST			psHWInst, 
													   IMG_BOOL			bSkipInv, 
													   USP_PCKUNPCK_FMT	eDstFmt, 
													   USP_PCKUNPCK_FMT	eSrcFmt, 
													   IMG_BOOL			bScaleEnable, 
													   IMG_UINT32		uWriteMask, 
													   PUSP_REG			psDestReg, 
													   IMG_BOOL			bUseSrcSelect, 
													   IMG_PUINT32		puSrcSelect, 
													   IMG_UINT32		uSrc1Select, 
													   IMG_UINT32		uSrc2Select, 
													   PUSP_REG			psSrc1Reg, 
													   PUSP_REG			psSrc2Reg);
#endif /* defined(SGX_FEATURE_USE_VEC34) */

IMG_INTERNAL IMG_BOOL HWInstEncodeSMPInstCoordDim(PHW_INST psHWInst, 
												  IMG_UINT32 uCoordDim);
#if defined(SGX_FEATURE_USE_32BIT_INT_MAD)
IMG_INTERNAL IMG_BOOL HWInstEncodeIMA32Inst(PHW_INST		psHWInst,
										 	IMG_BOOL		bSkipInv,
										 	PUSP_REG		psDestReg,
											PUSP_REG		psSrc0Reg,
											PUSP_REG		psSrc1Reg,
											PUSP_REG		psSrc2Reg);
#endif /* defined(SGX_FEATURE_USE_32BIT_INT_MAD) */

#endif	/* #ifndef _HWINST_H_ */
/*****************************************************************************
 End of file (HWINST.H)
*****************************************************************************/
