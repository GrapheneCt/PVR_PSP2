/*************************************************************************
 * Name         : usetab.h
 * Title        : Useful USE lookup tables (copied from use.c)
 * Author       : James McCarthy
 * Created      : 28/04/2006
 *
 * Copyright    : 2006-2010 by Imagination Technologies Limited. All rights reserved.
 *              : No part of this software, either material or conceptual 
 *              : may be copied or distributed, transmitted, transcribed,
 *              : stored in a retrieval system or translated into any 
 *              : human or computer language in any form by any means,
 *              : electronic, mechanical, manual or other-wise, or 
 *              : disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited, Unit 8, HomePark
 *              : Industrial Estate, King's Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 *
 * Modifications:-
 * $Log: usetab.h $
 **************************************************************************/

#ifndef USETAB_H
#define USETAB_H

#define USEASM_NUM_HW_SPECIAL_CONSTS (EURASIA_USE_SPECIAL_CONSTANT_FLOAT1_4 + 1)

extern const IMG_PCHAR  pszTypeToString[];
extern const IMG_FLOAT  g_pfHardwareConstants[USEASM_NUM_HW_SPECIAL_CONSTS];

#if defined(SGX543) || defined(SGX544) || defined(SGX554) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
typedef struct _USETAB_SPECIAL_CONST
{
	IMG_UINT32	auValue[SGXVEC_USE_SPECIAL_CONSTANT_WIDTH];
	IMG_BOOL	bReserved;
} USETAB_SPECIAL_CONST, *PUSETAB_SPECIAL_CONST;

typedef USETAB_SPECIAL_CONST const* PCUSETAB_SPECIAL_CONST;

extern const USETAB_SPECIAL_CONST g_asVecHardwareConstants[SGXVEC_USE_SPECIAL_NUM_FLOAT_CONSTANTS];

typedef enum 
{
	DUALISSUEVECTOR_SRCSLOT_GPI0,
	DUALISSUEVECTOR_SRCSLOT_GPI1,
	DUALISSUEVECTOR_SRCSLOT_GPI2,
	DUALISSUEVECTOR_SRCSLOT_UNIFIEDSTORE,
	DUALISSUEVECTOR_SRCSLOT_UNDEF,
} DUALISSUEVECTOR_SRCSLOT, *PDUALISSUEVECTOR_SRCSLOT;

typedef DUALISSUEVECTOR_SRCSLOT const* PCDUALISSUEVECTOR_SRCSLOT;

/*
	Entries point to arrays mapping from input instruction sources to encoding slots for the dual-issue
	vector instruction.
*/
extern const PCDUALISSUEVECTOR_SRCSLOT g_aapeDualIssueVectorPrimaryMap[SGXVEC_USE_DVEC_OP1_MAXIMUM_SRC_COUNT]
                                                                      [SGXVEC_USE0_DVEC_SRCCFG_COUNT];
extern const PCDUALISSUEVECTOR_SRCSLOT g_aaapeDualIssueVectorSecondaryMap[SGXVEC_USE_DVEC_OP1_MAXIMUM_SRC_COUNT]
                                                                         [SGXVEC_USE_DVEC_OP2_MAXIMUM_SRC_COUNT]
																		 [SGXVEC_USE0_DVEC_SRCCFG_COUNT];

#endif /* defined(SGX543) || defined(SGX544) || defined(SGX554) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

#endif /* USETAB_H */
