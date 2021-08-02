/*************************************************************************
 * Name         : efo.c
 * Title        : EFO Instructions!
 * Created      : Sept 2005
 *
 * Copyright    : 2002-2006 by Imagination Technologies Limited. All rights reserved.
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
 * Modifications:-
 * $Log: efo.c $
 **************************************************************************/

#include "uscshrd.h"
#include "bitops.h"
#include "usedef.h"
#include "reggroup.h"
#include <limits.h>

/* 
   EFO
 */

#if defined(EFO_TEST)
#define EFO_TESTONLY(X) X
#else /* defined(EFO_TEST) */
#define EFO_TESTONLY(X)
#endif /* defined(EFO_TEST) */

/*
	Range of internal registers which can be written implicitly (not through the unified store
	destination) by EFO instructions.
*/
#define NUM_INTERNAL_EFO_REGISTERS			(2)

typedef struct _EFO_GROUP_DATA
{
	/*
		Temporary storage.
	*/
	IMG_BOOL			bDependent;
	/*
		Number of temporary registers referenced by the EFO group.
	*/
	IMG_UINT32			uArgCount;
	/*
		First EFO instruction in the EFO group.
	*/
	PINST				psHead;
	/*
		Last EFO instruction in the EFO group.
	*/
	PINST				psTail;
	/*
		Is this a use of the internal register created before EFO generation.
	*/
	IMG_BOOL			bExistingGroup;
	/*
		Count of the instructions in the group.
	*/
	IMG_UINT32			uInstCount;
	/*
		Bit vector of descheduling instructions which this group is
		dependent on.
	*/
	IMG_PUINT32			auDeschedDependencies;
	/*
		Count of the descheduling instructions which this group is
		dependent on.
	*/
	IMG_UINT32			uDeschedDependencyCount;
	/*
		Count of the other groups this group is dependent on.
	*/
	IMG_UINT32			uGroupDependencyCount;
	/*
		In OutputInstructionsInGroupOrder:
			Count of the satisfied group dependencies.
	*/
	IMG_UINT32			uGroupSatisfiedDependencyCount;
} EFO_GROUP_DATA, *PEFO_GROUP_DATA;

typedef struct
{
	/*
		Current basic block.
	*/
	PCODEBLOCK		psCodeBlock;
	/* 
		Matrix of the dependencies between EFO groups. 
	*/
	IMG_PUINT32		aauEfoDependencyGraph;
	/* 
		Matrix of the dependencies between EFO groups closed under transitivity. 
	*/
	IMG_PUINT32		aauClosedEfoDependencyGraph;
	/*
		EFO group data.
	*/
	PEFO_GROUP_DATA	asEfoGroup;
	/*
		Count of the EFO groups created so far for this block.
	*/
	IMG_UINT32		uEfoGroupCount;
	/*
		Are we targeting the new EFO features?
	*/
	IMG_BOOL		bNewEfoFeature;
	/*
		Head of the list of descheduling instructions.
	*/
	PINST			psDeschedInstListHead;
	/*
		Tail of the list of descheduling instructions.
	*/
	PINST			psDeschedInstListTail;
} EFOGEN_STATE, *PEFOGEN_STATE;

static IMG_BOOL CanUseEfoSrc(PINTERMEDIATE_STATE	psState, 
							 PCODEBLOCK				psBlock, 
							 IMG_UINT32				uArg, 
							 PINST					psSrcInst,
							 IMG_UINT32				uSrcInstArgIdx);
static
IMG_BOOL CanConvertSingleInstToEfo(PINTERMEDIATE_STATE	psState,
								   PINST				psInst,
								   IMG_UINT32			uIReg);
static
IMG_VOID ConvertSingleInstToEfo(PINTERMEDIATE_STATE	psState,
								PCODEBLOCK			psBlock,
								PINST				psInst,
								PINST				psPrevEfoInst,
								IMG_UINT32			uPrevEfoDestIdx,
								IMG_UINT32			uIReg,
								EFO_SRC				eWBSrc);
typedef IMG_BOOL (*PFN_EFO_BUILDER)(PINTERMEDIATE_STATE		psState,
									PEFOGEN_STATE			psEfoState,
									PINST					psInstA, 
								    PINST					psInstB, 
									IMG_UINT32				uASrcInI0, 
									IMG_UINT32				uASrcInI1,
									IMG_UINT32				uBSrcInI0, 
 									IMG_UINT32				uBSrcInI1, 
									IMG_UINT32				uBSrcFromADest, 
									PINST					psEfoInst,
									IMG_PBOOL				pbExtraSub);

typedef struct
{
	IMG_UINT32		puASrcInIReg[2];
	IMG_UINT32		puASrcInNegIReg[2];
	IMG_UINT32		puBSrcInIReg[2];
	IMG_UINT32		puBSrcInNegIReg[2];
} IREG_STATUS, *PIREG_STATUS;

typedef struct
{
	EFO_SRC			eM0Src0, eM0Src1;
	EFO_SRC			eM1Src0, eM1Src1;

	EFO_SRC			eA0Src0, eA0Src1;
	EFO_SRC			eA1Src0, eA1Src1;

	EFO_SRC			eI0Src, eI1Src;

	EFO_SRC			eDSrc;
} EFO_PARAMS_TEMPLATE, *PEFO_PARAMS_TEMPLATE;
typedef EFO_PARAMS_TEMPLATE const* PCEFO_PARAMS_TEMPLATE;

typedef struct
{
	/*
		In the templates ABC are the 3 sources to instruction A and EFG are
		the 3 sources to instruction B.
	*/

	/*
		A list of pair of sources that must point to the same register and have
		the same modifiers.
	*/
	IMG_PCHAR						pszSameSources;
	/*
		For each source to the EFO the corresponding source to use from two instructions
		being combined; '1' to use the hardware constant 1 or '_' to leave undefined.
	*/
	IMG_PCHAR						pszEfoSources;
	/*
		List of the sources which must be able to be replaced by I0. If a source name is prefixed by '-'
		then if the source is negated the negate flag is set on the A0 right-hand input. If a source
		name isn't prefixed by '-' then a negated source doesn't match.
	*/
	IMG_PCHAR						pszI0Sources;
	/*
		List of the sources which must be able to be replaced by I1. If a source name is prefixed by '-'
		then if the source is negated the negate flag is set on the A1 left-hand input. If a source
		name isn't prefixed by '-' then a negated source doesn't match.
	*/
	IMG_PCHAR						pszI1Sources;
	/*
		EFO parameters for this template.
	*/
	const EFO_PARAMS_TEMPLATE*		psEfoParams;
	/*	
		If FALSE then I0 is the result of instruction A; I1 is the result of instruction B.
		If TRUE then I0 is the result of instruction B; I1 is the result of instruction A.
	*/
	IMG_BOOL						bInstBIsFirst;
	/*
		If TRUE then the EFO destination is used for the result of one of the instructions
		and I0 for the result of the other.
	*/
	IMG_BOOL						bWriteI0Only;
	/*
		If TRUE then the new EFO instruction uses internal registers written by an earlier EFO.
	*/
	IMG_BOOL						bIRegSources;
} EFO_TEMPLATE;

/* MUL/MUL EFO parameters. */
static const EFO_PARAMS_TEMPLATE sS0S1S0S2_M0M1 = {SRC0, SRC1, SRC0, SRC2, I0, I0, I0, I0, M0, M1, I0};
static const EFO_PARAMS_TEMPLATE sS1S2S0S0_M0M1 = {SRC1, SRC2, SRC0, SRC0, I0, I0, I0, I0, M0, M1, I0};
static const EFO_PARAMS_TEMPLATE sS1I0S0I1_M0M1 = {SRC1, I0,   SRC0, I1,   I0, I0, I0, I0, M0, M1, I0};

/* ADD/ADD EFO parameters. */
static const EFO_PARAMS_TEMPLATE sS0S1S2S0_A0A1 = {I0,   I0,   I0, I0, SRC0, SRC1, SRC2, SRC0, A0, A1, I0};
static const EFO_PARAMS_TEMPLATE sS0S1_M0S2I1I0	= {SRC0, SRC1, I0, I0, M0,   SRC2, I1,   I0,   A0, A1, I0};

/* ADD/MUL EFO parameters. */
static const EFO_PARAMS_TEMPLATE sS0S2_S0S1_A0M1 = {I0,   I0,   SRC0, SRC2, SRC0, SRC1, I0, I0, A0, M1, I0};
static const EFO_PARAMS_TEMPLATE sS1S2_I1I0_M0   = {SRC1, SRC2, I0,   I0,   I0,   I0,   I1, I0, M0, I0, A1};

/* MAD/MAD EFO parameters. */
static const EFO_PARAMS_TEMPLATE sS1S2S0S0_M0I0_I1M1 = {SRC1, SRC2, SRC0, SRC0, M0, I0, I1, M1, A0, A1, I0};
static const EFO_PARAMS_TEMPLATE sS0S1S0S2_M0I0_I1M1 = {SRC0, SRC1, SRC0, SRC2, M0, I0, I1, M1, A0, A1, I0};
static const EFO_PARAMS_TEMPLATE sS1I0S0I1_M0I0_I1M1 = {SRC1, I0,   SRC0, I1,   M0, I0, I1, M1, A0, A1, I0};

/* MAD/ADD EFO parameters. */
static const EFO_PARAMS_TEMPLATE sS0S1_M0S2_I1I0 = {SRC0, SRC1, I0, I0, M0, SRC2, I1, I0, A0, A1, I0};

/* MAD/MUL EFO parameters. */
static const EFO_PARAMS_TEMPLATE sS0S1S0S2_M0S2	  = {SRC0, SRC1, SRC0, SRC2, M0, SRC2, I0, I0, A0, M1, I0};
static const EFO_PARAMS_TEMPLATE sS1I0S0I1_M0S2	  = {SRC1, I0,   SRC0, I1,   M0, SRC2, I0, I0, A0, M1, I0};
static const EFO_PARAMS_TEMPLATE sS0S1S0S2_M0I0	  = {SRC0, SRC1, SRC0, SRC2, M0, I0,   I0, I0, A0, M1, I0};
static const EFO_PARAMS_TEMPLATE sS0S1S0S0_M0S2   = {SRC0, SRC1, SRC0, SRC0, M0, SRC2, I0, I0, A0, M1, I0};
static const EFO_PARAMS_TEMPLATE sS0S1_M0S2_M0_A0 = {SRC0, SRC1, I0,   I0,   M0, SRC2, I0, I0, M0, I0, A0}; 
static const EFO_PARAMS_TEMPLATE sS0S1S0S2_I1M1   = {SRC0, SRC1, SRC0, SRC2, I0, I0,   I1, M1, M0, I0, A1};

/* MSA/ADD EFO parameters. */
static const EFO_PARAMS_TEMPLATE sS1S2S0S0_M0M1_I0I1 = {SRC1, SRC2, SRC0, SRC0, M0, M1, I1, I0, A0, A1, I0};

	/*																bInstBIsFirst	bWriteI0Only	bIRegSources*/
static const EFO_TEMPLATE psMulMulTemplate_535[] =
{
	/* M0=S1*I0; M1=S0*I1; I0=M0; I1=M1 */
	{	"",		"EA_",	"B",	"F",	&sS1I0S0I1_M0M1,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"FA_",	"B",	"E",	&sS1I0S0I1_M0M1,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"EB_",	"A",	"F",	&sS1I0S0I1_M0M1,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"FB_",	"A",	"E",	&sS1I0S0I1_M0M1,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"AE_",	"F",	"B",	&sS1I0S0I1_M0M1,			IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"AF_",	"E",	"B",	&sS1I0S0I1_M0M1,			IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"BE_",	"F",	"A",	&sS1I0S0I1_M0M1,			IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"BF_",	"E",	"A",	&sS1I0S0I1_M0M1,			IMG_TRUE,		IMG_FALSE,		IMG_TRUE},

	/* M0=S0*S1; M1=S0*S2; I0=M0; I1=M1 */
	{	"AE",	"ABF",	"",		"",		&sS0S1S0S2_M0M1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
	{	"AF",	"ABE",	"",		"",		&sS0S1S0S2_M0M1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
	{	"BE",	"BAF",	"",		"",		&sS0S1S0S2_M0M1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
	{	"BF",	"BAE",	"",		"",		&sS0S1S0S2_M0M1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},

	/* M0=S1*S2; M1=S0*S0; I0=M0; I1=M1 */
	{	"AB",	"AEF",	"",		"",		&sS1S2S0S0_M0M1,			IMG_TRUE,		IMG_FALSE,		IMG_FALSE},
	{	"EF",	"EAB",	"",		"",		&sS1S2S0S0_M0M1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
};

	/*																bInstBIsFirst	bWriteI0Only	bIRegSources*/
static const EFO_TEMPLATE psAddAddTemplate_535[] =
{
	/* A0=S0+S1; A1=S2+S0; I0=A0; I1=A1 */
	{	"AE",	"ABF",	"",		"",		&sS0S1S2S0_A0A1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
	{	"AF",	"ABE",	"",		"",		&sS0S1S2S0_A0A1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
	{	"BE",	"BAF",	"",		"",		&sS0S1S2S0_A0A1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
	{	"BF",	"BAE",	"",		"",		&sS0S1S2S0_A0A1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},

	/* M0=S0*1(S1); A0=M0+S2; A1=I1+I0 */
	{	"",		"A1B",	"E",	"-F",	&sS0S1_M0S2I1I0,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"B1A",	"E",	"-F",	&sS0S1_M0S2I1I0,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"A1B",	"F",	"-E",	&sS0S1_M0S2I1I0,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"B1A",	"F",	"-E",	&sS0S1_M0S2I1I0,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"E1F",	"A",	"-B",	&sS0S1_M0S2I1I0,			IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"F1E",	"A",	"-B",	&sS0S1_M0S2I1I0,			IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"E1F",	"B",	"-A",	&sS0S1_M0S2I1I0,			IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"F1E",	"B",	"-A",	&sS0S1_M0S2I1I0,			IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
};

	/*																bInstBIsFirst	bWriteI0Only	bIRegSources*/
static const EFO_TEMPLATE psAddMulTemplate_535[] =
{
	/* M1=S0*S2; A0=S0+S1; I0=A0; I1=M1 */
	{	"AE",	"ABF",	"",		"",		&sS0S2_S0S1_A0M1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
	{	"AF",	"ABE",	"",		"",		&sS0S2_S0S1_A0M1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
	{	"BE",	"BAF",	"",		"",		&sS0S2_S0S1_A0M1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
	{	"BF",	"BAE",	"",		"",		&sS0S2_S0S1_A0M1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},

	/* M0=S1*S2, A0=I1+I0; I0=M0; D=A0 */
	{	"",		"_EF",	"A",	"-B",	&sS1S2_I1I0_M0,				IMG_FALSE,		IMG_TRUE,		IMG_TRUE},
	{	"",		"_EF",	"B",	"-A",	&sS1S2_I1I0_M0,				IMG_FALSE,		IMG_TRUE,		IMG_TRUE},
};

	/*																bInstBIsFirst	bWriteI0Only	bIRegSources*/
static const EFO_TEMPLATE psMadMadTemplate_535[] =
{
	/* M0=S1*S2; M1=S0*S0; A0=M0+I0; A1=I1+M1; I0=A0; I1=A1 */
	{	"AB",	"AEF",	"G",	"-C",	&sS1S2S0S0_M0I0_I1M1,		IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"EF",	"EAB",	"C",	"-G",	&sS1S2S0S0_M0I0_I1M1,		IMG_FALSE,		IMG_FALSE,		IMG_TRUE},

	/* M0=S0*S1; M1=S0*S2; A0=M0+I0; A1=I1+M1; I0=A0; I1=A1 */
	{	"AE",	"ABF",	"C",	"-G",	&sS0S1S0S2_M0I0_I1M1,		IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"AF",	"ABE",	"C",	"-G",	&sS0S1S0S2_M0I0_I1M1,		IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"AE",	"AFB",	"G",	"-C",	&sS0S1S0S2_M0I0_I1M1,		IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"AF",	"AEB",	"G",	"-C",	&sS0S1S0S2_M0I0_I1M1,		IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"BE",	"BAF",	"C",	"-G",	&sS0S1S0S2_M0I0_I1M1,		IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"BF",	"BAE",	"C",	"-G",	&sS0S1S0S2_M0I0_I1M1,		IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"BE",	"BFA",	"G",	"-C",	&sS0S1S0S2_M0I0_I1M1,		IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"BF",	"BEA",	"G",	"-C",	&sS0S1S0S2_M0I0_I1M1,		IMG_TRUE,		IMG_FALSE,		IMG_TRUE},

	/* M0=S1*I0; M1=S0*I1; A0=M0+I0; A1=I1+M1; I0=A0; I1=A1 */
	{	"",		"EA_",	"BC",	"F-G",	&sS1I0S0I1_M0I0_I1M1,		IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"EB_",	"AC",	"F-G",	&sS1I0S0I1_M0I0_I1M1,		IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"FA_",	"BC",	"E-G",	&sS1I0S0I1_M0I0_I1M1,		IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"FB_",	"AC",	"E-G",	&sS1I0S0I1_M0I0_I1M1,		IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"AE_",	"FG",	"B-C",	&sS1I0S0I1_M0I0_I1M1,		IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"BE_",	"FG",	"A-C",	&sS1I0S0I1_M0I0_I1M1,		IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"AF_",	"EG",	"B-C",	&sS1I0S0I1_M0I0_I1M1,		IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"BF_",	"EG",	"A-C",	&sS1I0S0I1_M0I0_I1M1,		IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
};

	/*																bInstBIsFirst	bWriteI0Only	bIRegSources*/
static const EFO_TEMPLATE psMadAddTemplate_535[] = 
{
	/* M0=S0*S1; A0=M0+S2; A1=I1+I0 */
	{	"",		"ABC",	"E",	"-F",	&sS0S1_M0S2_I1I0,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"BAC",	"E",	"-F",	&sS0S1_M0S2_I1I0,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"ABC",	"F",	"-E",	&sS0S1_M0S2_I1I0,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"BAC",	"F",	"-E",	&sS0S1_M0S2_I1I0,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
};

	/*																bInstBIsFirst	bWriteI0Only	bIRegSources*/
static const EFO_TEMPLATE psMadMulTemplate_535[] =
{
	/* M0=S0*S1; M1=S0*S2; A0=M0+S2 */
	{	"AECF",	"ABC",	"",		"",		&sS0S1S0S2_M0S2,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
	{	"BECF",	"BAC",	"",		"",		&sS0S1S0S2_M0S2,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},	

	/* M0=S1*I0; M1=S0*I1; A0=M0+S2 */
	{	"",		"EAC",	"B",	"F",	&sS1I0S0I1_M0S2,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"EBC",	"A",	"F",	&sS1I0S0I1_M0S2,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"FAC",	"B",	"E",	&sS1I0S0I1_M0S2,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"FBC",	"A",	"E",	&sS1I0S0I1_M0S2,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},

	/* M0=S0*S1; M1=S0*S2; A0=M0+I0 */
	{	"AE",	"ABF",	"C",	"",		&sS0S1S0S2_M0I0,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"AF",	"ABE",	"C",	"",		&sS0S1S0S2_M0I0,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"BE",	"BAF",	"C",	"",		&sS0S1S0S2_M0I0,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"BF",	"BAE",	"C",	"",		&sS0S1S0S2_M0I0,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},

	/* M0=S0*S1; M1=S0*S2; A1=I1+M1 */
	{	"AE",	"AFB",	"",		"-C",	&sS0S1S0S2_I1M1,			IMG_FALSE,		IMG_TRUE,		IMG_TRUE},
	{	"AF",	"AEB",	"",		"-C",	&sS0S1S0S2_I1M1,			IMG_FALSE,		IMG_TRUE,		IMG_TRUE},
	{	"BE",	"BFA",	"",		"-C",	&sS0S1S0S2_I1M1,			IMG_FALSE,		IMG_TRUE,		IMG_TRUE},
	{	"BF",	"BEA",	"",		"-C",	&sS0S1S0S2_I1M1,			IMG_FALSE,		IMG_TRUE,		IMG_TRUE},

	/* M0=S0*S1; A0=M0+S2; I0=M0; DSRC=A0 */
	{	"AEBF",	"ABC",	"",		"",		&sS0S1_M0S2_M0_A0,			IMG_FALSE,		IMG_TRUE,		IMG_FALSE},
	{	"AFBE",	"ABC",	"",		"",		&sS0S1_M0S2_M0_A0,			IMG_FALSE,		IMG_TRUE,		IMG_FALSE},
	{	"AEBF",	"BAC",	"",		"",		&sS0S1_M0S2_M0_A0,			IMG_FALSE,		IMG_TRUE,		IMG_FALSE},
	{	"AFBE",	"BAC",	"",		"",		&sS0S1_M0S2_M0_A0,			IMG_FALSE,		IMG_TRUE,		IMG_FALSE},

	/* M0=S0*S1; M1=S0*S0; A0=M0+S2; I0=A0; I1=M1 */
	{	"AEEF",	"EBC",	"",		"",		&sS0S1S0S0_M0S2,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
	{	"BEEF",	"EAC",	"",		"",		&sS0S1S0S0_M0S2,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
};

	/*																bInstBIsFirst	bWriteI0Only	bIRegSources*/
static const EFO_TEMPLATE psMsaAddTemplate_535[] =
{
	/* M0=S1*S2; M1=S0*S0; A0=M0+M1; A1=I1+I0 */
	{	"",	"ABC",		"E",	"-F",	&sS1S2S0S0_M0M1_I0I1,		IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",	"ABC",		"F",	"-E",	&sS1S2S0S0_M0M1_I0I1,		IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
};

#if defined(SUPPORT_SGX545)
/* 545 MUL/MUL EFO parameters. */
static const EFO_PARAMS_TEMPLATE sS0S2S1S2_M0M1 = {SRC0, SRC2, SRC1, SRC2, I0, I0, I0, I0, M0, M1};
static const EFO_PARAMS_TEMPLATE sS1S0S2S2_M0M1 = {SRC1, SRC0, SRC2, SRC2, I0, I0, I0, I0, M0, M1};
static const EFO_PARAMS_TEMPLATE sS1I0S2I1_M0M1 = {SRC1, I0,   SRC2, I1,   I0, I0, I0, I0, M0, M1};

/* 545 ADD/ADD EFO parameters. */
static const EFO_PARAMS_TEMPLATE sS0S2S1S2_A0A1          = {I0,   I0,   I0,   I0,   SRC0, SRC2, SRC1, SRC2, A0, A1};
static const EFO_PARAMS_TEMPLATE sS0S2S1S2_M0I0I1M1_A0A1 = {SRC0, SRC2, SRC1, SRC2, M0,   I0,   I1,   M1,   A0, A1};
static const EFO_PARAMS_TEMPLATE sS2S1_M0S0I1I0_A0A1     = {SRC2, SRC1, I0,   I0,   M0,   SRC0, I1,   I0,   A0, A1};

/* 545 ADD/MUL EFO parameters. */
static const EFO_PARAMS_TEMPLATE sS1S2S0S2_A0M1   = {I0,   I0,   SRC1, SRC2, SRC0, SRC2, I0, I0, A0, M1};
static const EFO_PARAMS_TEMPLATE sS2S1_I1I0_M0_A1 = {SRC2, SRC1, I0,   I0,   I0,   I0,   I1, I0, M0, I0, A1};

/* 545 MAD/MAD EFO parameters. */
static const EFO_PARAMS_TEMPLATE sS1S0S2S2_M0I0_I1M1 = {SRC1, SRC0, SRC2, SRC2, M0, I0, I1, M1, A0, A1};
static const EFO_PARAMS_TEMPLATE sS0S2S1S2_M0I0_I1M1 = {SRC0, SRC2, SRC1, SRC2, M0, I0, I1, M1, A0, A1};
static const EFO_PARAMS_TEMPLATE sS1I0S2I1_M0I0_I1M1 = {SRC1, I0,   SRC2, I1,   M0, I0, I1, M1, A0, A1};

/* 545 MAD/ADD EFO parameters. */
static const EFO_PARAMS_TEMPLATE sS2S1_M0S0_I1I0 = {SRC2, SRC1, I0, I0,  M0, SRC0, I1, I0, A0, A1};

/* 545 MAD/MUL EFO parameters. */
static const EFO_PARAMS_TEMPLATE sS0S2S1S2_M0S0   = {SRC0, SRC2, SRC1, SRC2, M0, SRC0, I0, I0, A0, M1};
static const EFO_PARAMS_TEMPLATE sS1I0S2I1_M0S0   = {SRC1, I0,   SRC2, I1,   M0, SRC0, I0, I0, A0, M1};
static const EFO_PARAMS_TEMPLATE sS0S2S1S2_M0I0   = {SRC0, SRC2, SRC1, SRC2, M0, I0,   I0, I0, A0, M1};
static const EFO_PARAMS_TEMPLATE sS0S2S1S2_I1M1   = {SRC0, SRC2, SRC1, SRC2, I0, I0,   I1, M1, M0, I0, A1};
static const EFO_PARAMS_TEMPLATE sS2S1_M0S0_M0_A0 = {SRC2, SRC1, I0,   I0,   M0, SRC0, I0, I0, M0, I0, A0};
static const EFO_PARAMS_TEMPLATE sS2S1S2S2_M0S0   = {SRC2, SRC1, SRC2, SRC2, M0, SRC0, I0, I0, A0, M1};

/* 545 MSA/ADD EFO parameters. */
static const EFO_PARAMS_TEMPLATE sS1S0S2S2_M0M1_I0I1 = {SRC1, SRC0, SRC2, SRC2, M0, M1, I1, I0, A0, A1};

	/*																bInstBIsFirst	bWriteI0Only	bIRegSources*/
static const EFO_TEMPLATE psMulMulTemplate_545[] =
{
	/* M0=S0*S2; M1=S1*S2; I0=M0; I1=M1 */
	{	"AE",	"BFA",	"",		"",		&sS0S2S1S2_M0M1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
	{	"AF",	"BEA",	"",		"",		&sS0S2S1S2_M0M1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
	{	"BE",	"AFB",	"",		"",		&sS0S2S1S2_M0M1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
	{	"BF",	"AEB",	"",		"",		&sS0S2S1S2_M0M1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},

	/* M0=S1*S0; M1=S2*S2; I0=M0; I1=M1 */
	{	"AB",	"EFA",	"",		"",		&sS1S0S2S2_M0M1,			IMG_TRUE,		IMG_FALSE,		IMG_FALSE},
	{	"EF",	"ABE",	"",		"",		&sS1S0S2S2_M0M1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},

	/* M0=S1*I0; M1=S2*I1; I0=M0; I1=M1 */
	{	"",		"_AE",	"B",	"F",	&sS1I0S2I1_M0M1,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"_AF",	"B",	"E",	&sS1I0S2I1_M0M1,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"_BE",	"A",	"F",	&sS1I0S2I1_M0M1,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"_BF",	"A",	"E",	&sS1I0S2I1_M0M1,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"_EA",	"F",	"B",	&sS1I0S2I1_M0M1,			IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"_FA",	"E",	"B",	&sS1I0S2I1_M0M1,			IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"_EB",	"F",	"A",	&sS1I0S2I1_M0M1,			IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"_FB",	"E",	"A",	&sS1I0S2I1_M0M1,			IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
};

static const EFO_TEMPLATE psAddAddTemplate_545[] =
{
	/* A0=S0+S2; A1=S1+S2; I0=A0; I1=A1 */
	{	"-AE",	"BFE",	"",		"",		&sS0S2S1S2_A0A1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
	{	"-AF",	"BEF",	"",		"",		&sS0S2S1S2_A0A1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
	{	"-BE",	"AFE",	"",		"",		&sS0S2S1S2_A0A1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
	{	"-BF",	"AEF",	"",		"",		&sS0S2S1S2_A0A1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},

	/* M0=S0*1; M1=S1*1; A0=M0+I0; A1=I1+M1; I0=A0; I1=A1 */
	{	"",		"AE1",	"-B",	"-F",	&sS0S2S1S2_M0I0I1M1_A0A1,	IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"AF1",	"-B",	"-E",	&sS0S2S1S2_M0I0I1M1_A0A1,	IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"BE1",	"-A",	"-F",	&sS0S2S1S2_M0I0I1M1_A0A1,	IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"BF1",	"-A",	"-E",	&sS0S2S1S2_M0I0I1M1_A0A1,	IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"EA1",	"-F",	"-B",	&sS0S2S1S2_M0I0I1M1_A0A1,	IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"FA1",	"-E",	"-B",	&sS0S2S1S2_M0I0I1M1_A0A1,	IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"EB1",	"-F",	"-A",	&sS0S2S1S2_M0I0I1M1_A0A1,	IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"FB1",	"-E",	"-A",	&sS0S2S1S2_M0I0I1M1_A0A1,	IMG_TRUE,		IMG_FALSE,		IMG_TRUE},

	/* M0=1*S1; A0=M0+S0; A1=I1+I0 */
	{	"",		"AB1",	"E",	"-F",	&sS2S1_M0S0I1I0_A0A1,		IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"AB1",	"F",	"-E",	&sS2S1_M0S0I1I0_A0A1,		IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"EF1",	"A",	"-B",	&sS2S1_M0S0I1I0_A0A1,		IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"EF1",	"B",	"-A",	&sS2S1_M0S0I1I0_A0A1,		IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
};

static const EFO_TEMPLATE psAddMulTemplate_545[] =
{
	/* M1=S1*S2; A0=S0+S2; I0=A0; I1=M1 */
	{	"-AE",	"BFE",	"",		"",		&sS1S2S0S2_A0M1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
	{	"-AF",	"BEF",	"",		"",		&sS1S2S0S2_A0M1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
	{	"-BE",	"AFE",	"",		"",		&sS1S2S0S2_A0M1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
	{	"-BF",	"AEF",	"",		"",		&sS1S2S0S2_A0M1,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},

	/* M0=S2*S1; A1=I1+I0; I0=M0; D=A1 */
	{	"",		"_EF",	"A",	"-B",	&sS2S1_I1I0_M0_A1,			IMG_FALSE,		IMG_TRUE,		IMG_TRUE},
	{	"",		"_EF",	"B",	"-A",	&sS2S1_I1I0_M0_A1,			IMG_FALSE,		IMG_TRUE,		IMG_TRUE},
};

static const EFO_TEMPLATE psMadMadTemplate_545[] =
{
	/* M0=S1*S0; M1=S2*S2; A0=M0+I0; A1=I1+M1; I0=A0; I1=A1 */
	{	"AB",	"EFA",	"-G",	"-C",	&sS1S0S2S2_M0I0_I1M1,		IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"EF",	"ABE",	"-C",	"-G",	&sS1S0S2S2_M0I0_I1M1,		IMG_FALSE,		IMG_FALSE,		IMG_TRUE},

	/* M0=S0*S2; M1=S1*S2; A0=M0+I0; A1=I1+M1; I0=A0; I1=A1 */
	{	"AE",	"BFA",	"-C",	"-G",	&sS0S2S1S2_M0I0_I1M1,		IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"AF",	"BEA",	"-C",	"-G",	&sS0S2S1S2_M0I0_I1M1,		IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"AE",	"FBA",	"-G",	"-C",	&sS0S2S1S2_M0I0_I1M1,		IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"AF",	"EBA",	"-G",	"-C",	&sS0S2S1S2_M0I0_I1M1,		IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"BE",	"AFB",	"-C",	"-G",	&sS0S2S1S2_M0I0_I1M1,		IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"BF",	"AEB",	"-C",	"-G",	&sS0S2S1S2_M0I0_I1M1,		IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"BE",	"FAB",	"-G",	"-C",	&sS0S2S1S2_M0I0_I1M1,		IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"BF",	"EAB",	"-G",	"-C",	&sS0S2S1S2_M0I0_I1M1,		IMG_TRUE,		IMG_FALSE,		IMG_TRUE},

	/* M0=S1*I0; M1=S2*I1; A0=M0+I0; A1=I1+M1; I0=A0; I1=A1 */
	{	"",		"_AE",	"B-C",	"F-G",	&sS1I0S2I1_M0I0_I1M1,		IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"_BE",	"A-C",	"F-G",	&sS1I0S2I1_M0I0_I1M1,		IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"_AF",	"B-C",	"E-G",	&sS1I0S2I1_M0I0_I1M1,		IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"_BF",	"A-C",	"E-G",	&sS1I0S2I1_M0I0_I1M1,		IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"_EA",	"F-G",	"B-C",	&sS1I0S2I1_M0I0_I1M1,		IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"_EB",	"F-G",	"A-C",	&sS1I0S2I1_M0I0_I1M1,		IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"_FA",	"E-G",	"B-C",	&sS1I0S2I1_M0I0_I1M1,		IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"_FB",	"E-G",	"A-C",	&sS1I0S2I1_M0I0_I1M1,		IMG_TRUE,		IMG_FALSE,		IMG_TRUE},
};

static const EFO_TEMPLATE psMadAddTemplate_545[] = 
{
	/* M0=S2*S1; A0=M0+S0; A1=I1+I0 */
	{	"",		"CAB",	"E",	"-F",	&sS2S1_M0S0_I1I0,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"CBA",	"E",	"-F",	&sS2S1_M0S0_I1I0,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"CAB",	"F",	"-E",	&sS2S1_M0S0_I1I0,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"CBA",	"F",	"-E",	&sS2S1_M0S0_I1I0,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
};

static const EFO_TEMPLATE psMadMulTemplate_545[] =
{
	/* M0=S0*S2; M1=S1*S2; A0=M0+S0; I0=A0; I1=M1 */
	{	"AE-BC", "BFA",	"",		"",		&sS0S2S1S2_M0S0,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
	{	"BE-AC", "AFB",	"",		"",		&sS0S2S1S2_M0S0,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},	
	{	"AF-BC", "BEA",	"",		"",		&sS0S2S1S2_M0S0,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
	{	"BF-AC", "AEB",	"",		"",		&sS0S2S1S2_M0S0,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},	

	/* M0=S1*I0; M1=S2*I1; A0=M0+S0; I0=A0; I1=M1 */
	{	"",		"CAE",	"B",	"F",	&sS1I0S2I1_M0S0,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"CBE",	"A",	"F",	&sS1I0S2I1_M0S0,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"CAF",	"B",	"E",	&sS1I0S2I1_M0S0,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",		"CBF",	"A",	"E",	&sS1I0S2I1_M0S0,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},

	/* M0=S0*S2; M1=S1*S2; A0=M0+I0 */
	{	"AE",	"BFA",	"-C",	"",		&sS0S2S1S2_M0I0,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"AF",	"BEA",	"-C",	"",		&sS0S2S1S2_M0I0,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"BE",	"AFB",	"-C",	"",		&sS0S2S1S2_M0I0,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"BF",	"AEB",	"-C",	"",		&sS0S2S1S2_M0I0,			IMG_FALSE,		IMG_FALSE,		IMG_TRUE},

	/* M0=S0*S2; M1=S1*S2; A1=I1+M1; I0=M0; DSRC=A1*/
	{	"AE",	"FBA",	"",	  "-C",		&sS0S2S1S2_I1M1,			IMG_FALSE,		IMG_TRUE,		IMG_TRUE},
	{	"AF",	"EBA",	"",	  "-C",		&sS0S2S1S2_I1M1,			IMG_FALSE,		IMG_TRUE,		IMG_TRUE},
	{	"BE",	"FAB",	"",	  "-C",		&sS0S2S1S2_I1M1,			IMG_FALSE,		IMG_TRUE,		IMG_TRUE},
	{	"BF",	"EAB",	"",	  "-C",		&sS0S2S1S2_I1M1,			IMG_FALSE,		IMG_TRUE,		IMG_TRUE},

	/* M0=S2*S1; A0=M0+S0; I0=M0; DSRC=A0 */
	{	"AEBF",	"CAB",	"",		"",		&sS2S1_M0S0_M0_A0,			IMG_FALSE,		IMG_TRUE,		IMG_FALSE},
	{	"AEBF",	"CBA",	"",		"",		&sS2S1_M0S0_M0_A0,			IMG_FALSE,		IMG_TRUE,		IMG_FALSE},
	{	"AFBE",	"CBA",	"",		"",		&sS2S1_M0S0_M0_A0,			IMG_FALSE,		IMG_TRUE,		IMG_FALSE},
	{	"AFBE",	"CAB",	"",		"",		&sS2S1_M0S0_M0_A0,			IMG_FALSE,		IMG_TRUE,		IMG_FALSE},

	/* M0=S2*S1; M1=S2*S2; A0=M0+S0; I0=A0; I1=M1 */
	{	"AEEF",	"CBE",	"",		"",		&sS2S1S2S2_M0S0,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
	{	"BEEF",	"CAE",	"",		"",		&sS2S1S2S2_M0S0,			IMG_FALSE,		IMG_FALSE,		IMG_FALSE},
};

static const EFO_TEMPLATE psMsaAddTemplate_545[] =
{
	/* M0=S1*S0; M1=S2*S2; A0=M0+M1; A1=I1+I0 */
	{	"",	"BCA",		"E",	"-F",	&sS1S0S2S2_M0M1_I0I1,		IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
	{	"",	"BCA",		"F",	"-E",	&sS1S0S2S2_M0M1_I0I1,		IMG_FALSE,		IMG_FALSE,		IMG_TRUE},
};
#endif /* defined(SUPPORT_SGX545) */

static IMG_VOID CopyEfoSource(PINTERMEDIATE_STATE	psState,
							  PINST					psDestInst, 
							  IMG_UINT32			uDestArgIdx, 
							  PINST					psSrcInst, 
							  IMG_UINT32			uSrcArgIdx)
/*****************************************************************************
 FUNCTION	: MoveEfoSource
    
 PURPOSE	: Copy a source argument and associated source modifiers between
			  two EFO instructions.

 PARAMETERS	: psState			- Compiler state.
			  psDestInst		- Destination instruction.
			  uDestArgIdx		- Destination argument index.
			  psSrcInst			- Source instruction.
			  uSrcArgIdx		- Source argument index.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(psDestInst->eOpcode == IEFO);
	ASSERT(psSrcInst->eOpcode == IEFO);

	psDestInst->u.psEfo->sFloat.asSrcMod[uDestArgIdx] = psSrcInst->u.psEfo->sFloat.asSrcMod[uSrcArgIdx];
	CopySrc(psState, psDestInst, uDestArgIdx, psSrcInst, uSrcArgIdx);
}


static IMG_VOID MoveEfoSource(PINTERMEDIATE_STATE	psState,
							  PINST					psDestInst, 
							  IMG_UINT32			uDestArgIdx, 
							  PINST					psSrcInst, 
							  IMG_UINT32			uSrcArgIdx)
/*****************************************************************************
 FUNCTION	: MoveEfoSource
    
 PURPOSE	: Move a source argument and associated source modifiers between
			  two EFO instructions.

 PARAMETERS	: psState			- Compiler state.
			  psDestInst		- Destination instruction.
			  uDestArgIdx		- Destination argument index.
			  psSrcInst			- Source instruction.
			  uSrcArgIdx		- Source argument index.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(psDestInst->eOpcode == IEFO);
	ASSERT(psSrcInst->eOpcode == IEFO);

	psDestInst->u.psEfo->sFloat.asSrcMod[uDestArgIdx] = psSrcInst->u.psEfo->sFloat.asSrcMod[uSrcArgIdx];
	MoveSrc(psState, psDestInst, uDestArgIdx, psSrcInst, uSrcArgIdx);
}

static IMG_VOID CopyEfoArgToFloat(PINTERMEDIATE_STATE	psState,
								  PINST					psDestInst, 
								  IMG_UINT32			uDestArgIdx, 
								  PINST					psEfoInst, 
								  IMG_UINT32			uSrcArgIdx)
/*****************************************************************************
 FUNCTION	: CopyEfoArgToFloat
    
 PURPOSE	: Copy a source argument and associated source modifiers from an
			  EFO instruction to a floating point instruction.

 PARAMETERS	: psState			- Compiler state.
			  psDestInst		- Destination instruction.
			  uDestArgIdx		- Destination argument index.
			  psEfoInst			- Source instruction.
			  uSrcArgIdx		- Source argument index.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(psEfoInst->eOpcode == IEFO);
	ASSERT(g_psInstDesc[psDestInst->eOpcode].eType == INST_TYPE_FLOAT);

	psDestInst->u.psFloat->asSrcMod[uDestArgIdx] = psEfoInst->u.psEfo->sFloat.asSrcMod[uSrcArgIdx];
	CopySrc(psState, psDestInst, uDestArgIdx, psEfoInst, uSrcArgIdx);
}

static IMG_VOID MoveFloatArgToEfo(PINTERMEDIATE_STATE	psState,
								  PINST					psEfoInst, 
								  IMG_UINT32			uDestArgIdx, 
								  PINST					psSrcInst, 
								  IMG_UINT32			uSrcArgIdx)
/*****************************************************************************
 FUNCTION	: MoveFloatArgToEfo
    
 PURPOSE	: Copy a source argument and associated source modifiers from a
			  floating point instruction to an EFO instruction.

 PARAMETERS	: psState			- Compiler state.
			  psEfoInst			- Destination instruction.
			  uDestArgIdx		- Destination argument index.
			  psSrcInst			- Source instruction.
			  uSrcArgIdx		- Source argument index.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PFLOAT_SOURCE_MODIFIER	psDestMod;

	ASSERT(psEfoInst->eOpcode == IEFO);
	psDestMod = &psEfoInst->u.psEfo->sFloat.asSrcMod[uDestArgIdx];
	*psDestMod = *GetFloatMod(psState, psSrcInst, uSrcArgIdx);

	MoveSrc(psState, psEfoInst, uDestArgIdx, psSrcInst, uSrcArgIdx);

	/*
		Record (approximately) whether any EFO instructions have
		F16 sources.
	*/
	if (psEfoInst->asArg[uDestArgIdx].eFmt == UF_REGFORMAT_F16)
	{
		psState->uFlags |= USC_FLAGS_EFOS_HAVE_F16_SOURCES;
	}
}

static IMG_BOOL GetDependency(PEFOGEN_STATE		psEfoState,
							  IMG_UINT32		uSrc,
							  IMG_UINT32		uDest)
/*****************************************************************************
 FUNCTION	: GetDependency
    
 PURPOSE	: Check for a non-transitive dependency between two EFO groups.

 PARAMETERS	: psEfoState		- EFO state.
			  uSrc, uDest		- Groups to check.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	return (GetBit(psEfoState->aauEfoDependencyGraph + uSrc * UINTS_TO_SPAN_BITS(psEfoState->uEfoGroupCount), uDest)) ? IMG_TRUE : IMG_FALSE;
}

static IMG_VOID SetDependency(PEFOGEN_STATE	psEfoState,
							  IMG_UINT32	uSrc,
							  IMG_UINT32	uDest,
							  IMG_UINT32	uBit)
/*****************************************************************************
 FUNCTION	: SetDependency
    
 PURPOSE	: Set the non-transitive dependency between two EFO groups.

 PARAMETERS	: psEfoState		- EFO state.
			  uSrc, uDest		- Groups to set for.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_PUINT32	auGraphRow = psEfoState->aauEfoDependencyGraph + uSrc * UINTS_TO_SPAN_BITS(psEfoState->uEfoGroupCount);

	if (uBit)
	{
		if (!GetBit(auGraphRow, uDest))
		{
			psEfoState->asEfoGroup[uSrc].uGroupDependencyCount++;
			SetBit(auGraphRow, uDest, 1);
		}
	}
	else
	{
		if (GetBit(auGraphRow, uDest))
		{
			psEfoState->asEfoGroup[uSrc].uGroupDependencyCount--;
			SetBit(auGraphRow, uDest, 0);
		}
	}
}

static IMG_BOOL GetClosedDependency(PEFOGEN_STATE	psEfoState,
									IMG_UINT32		uSrc,
									IMG_UINT32		uDest)
/*****************************************************************************
 FUNCTION	: GetClosedDependency
    
 PURPOSE	: Check for a transitive dependency between two EFO groups.

 PARAMETERS	: psEfoState		- EFO state.
			  uSrc, uDest		- Groups to check.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	return (GetBit(psEfoState->aauClosedEfoDependencyGraph + uSrc * UINTS_TO_SPAN_BITS(psEfoState->uEfoGroupCount), uDest)) ? IMG_TRUE : IMG_FALSE;
}

static IMG_VOID SetClosedDependency(PEFOGEN_STATE	psEfoState,
									IMG_UINT32		uSrc,
									IMG_UINT32		uDest,
									IMG_UINT32		uBit)
/*****************************************************************************
 FUNCTION	: SetClosedDependency
    
 PURPOSE	: Set the transitive dependency between two EFO groups.

 PARAMETERS	: psEfoState		- EFO state.
			  uSrc, uDest		- Groups to set for.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	SetBit(psEfoState->aauClosedEfoDependencyGraph + uSrc * UINTS_TO_SPAN_BITS(psEfoState->uEfoGroupCount), uDest, uBit);
}

static IMG_VOID ClearDependencies(PEFOGEN_STATE	psEfoState,
								  IMG_UINT32	uNode)
/*****************************************************************************
 FUNCTION	: ClearDependencies
    
 PURPOSE	: Clear the dependency structures for an EFO group.

 PARAMETERS	: psEfoState		- EFO state.
			  uNode				- Group to clear for.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uGraphStride = UINTS_TO_SPAN_BITS(psEfoState->uEfoGroupCount);

	memset(psEfoState->aauEfoDependencyGraph + uNode * uGraphStride,
		   0,
		   uGraphStride * sizeof(IMG_UINT32));
	memset(psEfoState->aauClosedEfoDependencyGraph + uNode * uGraphStride,
		   0,
		   uGraphStride * sizeof(IMG_UINT32));
}

static IMG_VOID SetupIRegDests(PINTERMEDIATE_STATE	psState,
							   PINST				psEfoInst,
							   EFO_SRC				eI0Src,
							   EFO_SRC				eI1Src)
/*****************************************************************************
 FUNCTION	: SetupIRegDests
    
 PURPOSE	: Sets up the EFO destination representing writes to I0 and I1.

 PARAMETERS	: psState			- Compiler state.
			  psEfoInst			- EFO instruction.
			  eI0Src			- ALU output written to I0 or EFO_SRC_UNDEF to
								disable the write.
			  eI1Src			- ALU output written to I1 or EFO_SRC_UNDEF to
								disable the write.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (eI0Src != EFO_SRC_UNDEF)
	{
		psEfoInst->u.psEfo->bWriteI0 = IMG_TRUE;
		SetDest(psState, psEfoInst, EFO_I0_DEST, USEASM_REGTYPE_FPINTERNAL, 0 /* uNumber */, UF_REGFORMAT_F32);
	}
	else
	{
		psEfoInst->u.psEfo->bWriteI0 = IMG_FALSE;
	}
	psEfoInst->u.psEfo->eI0Src = eI0Src;

	if (eI1Src != EFO_SRC_UNDEF)
	{
		psEfoInst->u.psEfo->bWriteI1 = IMG_TRUE;
		SetDest(psState, psEfoInst, EFO_I1_DEST, USEASM_REGTYPE_FPINTERNAL, 1 /* uNumber */, UF_REGFORMAT_F32);
	}
	else
	{
		psEfoInst->u.psEfo->bWriteI1 = IMG_FALSE;
	}
	psEfoInst->u.psEfo->eI1Src = eI1Src;
}

static IMG_BOOL CheckEfoIReg(PINTERMEDIATE_STATE psState,
							 IMG_CHAR cArg, 
							 PIREG_STATUS psIRegStatus, 
							 IMG_UINT32 uIReg, 
							 IMG_BOOL bNegate, 
							 IMG_BOOL bSwap,
							 IMG_PBOOL pbNegateResult)
/*****************************************************************************
 FUNCTION	: CheckEfoIReg
    
 PURPOSE	: Checks if an argument to one of two instructions can be replaced
			  by an internal register.

 PARAMETERS	: cArg				- Argument to check.
			  psIRegStatus		- Which arguments are replacable by internal
								registers.
			  uIReg				- Internal register to check.
			  bNegate			- Can the argument be negated?
			  bSwap				- If the sense of the two instruction should
								be swapped.
			  pbNegateResult	- Set to TRUE on return if the argument was
								negated.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32 uMask = 0;
	IMG_PUINT32 puSrcInIReg, puSrcInNegIReg;

	#ifdef DEBUG
	/* psState used to handle failed ASSERT on release build only */
	PVR_UNREFERENCED_PARAMETER(psState);
	#endif

	if (bSwap)
	{
		if (cArg == 'A' || cArg == 'B' || cArg == 'C')
		{
			cArg = (IMG_CHAR)('E' + (cArg - 'A'));
		}
		else
		{
			ASSERT(cArg == 'E' || cArg == 'F' || cArg == 'G');
			cArg = (IMG_CHAR)('A' + (cArg - 'E'));
		}
	}

	switch (cArg)
	{
		case 'A':
		case 'E': uMask = 1; break;
		case 'B':
		case 'F': uMask = 2; break;
		case 'C':
		case 'G': uMask = 4; break;
		default: imgabort();
	}

	if (cArg == 'A' || cArg == 'B' || cArg == 'C')
	{
		puSrcInIReg = psIRegStatus->puASrcInIReg;
		puSrcInNegIReg = psIRegStatus->puASrcInNegIReg;
	}
	else
	{
		puSrcInIReg = psIRegStatus->puBSrcInIReg;
		puSrcInNegIReg = psIRegStatus->puBSrcInNegIReg;
	}

	if (bNegate)
	{
		if (puSrcInNegIReg[uIReg] & uMask)
		{
			*pbNegateResult = IMG_TRUE;
			return IMG_TRUE; 
		}
		else
		{
			*pbNegateResult = IMG_FALSE;
		}
	}

	return (puSrcInIReg[uIReg] & uMask) ? IMG_TRUE : IMG_FALSE;
}

static IMG_VOID GetArg(PINTERMEDIATE_STATE psState,
					   PINST psInstA, 
					   PINST psInstB, 
					   IMG_CHAR cArg,
					   PINST* ppsArgInst,
					   IMG_PUINT32 puArgInstSrcIdx)
/*****************************************************************************
 FUNCTION	: GetArg
    
 PURPOSE	: Gets a pointer to an argument from one of two instruction.

 PARAMETERS	: psState		- Internal compiler state
			  psInstA		- Instructions.
			  psInstB
			  cArg			- Argument to get.
			  ppsArgMod		- Returns a pointer to the source modifier
							on the argument.
			  
 RETURNS	: A pointer to the argument.
*****************************************************************************/
{
	switch (cArg)
	{
		case 'A':
		{
			*ppsArgInst = psInstA;
			*puArgInstSrcIdx = 0;
			break;
		}
		case 'B':
		{
			*ppsArgInst = psInstA;
			*puArgInstSrcIdx = 1;
			break;
		}
		case 'C':
		{
			*ppsArgInst = psInstA;
			*puArgInstSrcIdx = 2;
			break;
		}
		case 'E':
		{
			*ppsArgInst = psInstB;
			*puArgInstSrcIdx = 0;
			break;
		}
		case 'F':
		{
			*ppsArgInst = psInstB;
			*puArgInstSrcIdx = 1;
			break;
		}
		case 'G':
		{
			*ppsArgInst = psInstB;
			*puArgInstSrcIdx = 2;
			break;
		}
		default: imgabort();
	}
}

static IMG_VOID SetupEfoStageData(PINTERMEDIATE_STATE psState, 
								  PEFOGEN_STATE psEfoState,
								  PINST psInst)
/*****************************************************************************
 FUNCTION	: SetupEfoStageData
    
 PURPOSE	: Sets up the per-instruction data used in the EFO generated step.

 PARAMETERS	: psState		- Compiler state
			  psEfoState	- Module state.
			  psInst		- Instruction to set up.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	psInst->sStageData.psEfoData = UscAlloc(psState, sizeof(EFO_STAGEDATA));

	psInst->sStageData.psEfoData->uEfoGroupId = USC_UNDEF;
	psInst->sStageData.psEfoData->psPrevWriter = NULL;
	psInst->sStageData.psEfoData->psNextWriter = NULL;
	psInst->sStageData.psEfoData->psFirstReader = NULL;
	psInst->sStageData.psEfoData->psNextReader = NULL;
	psInst->sStageData.psEfoData->bSelfContained = IMG_FALSE;
	psInst->sStageData.psEfoData->abIRegUsed[0] = IMG_FALSE;
	psInst->sStageData.psEfoData->abIRegUsed[1] = IMG_FALSE;

	/*
		Create a linked list of the descheduling instructions in the block.
	*/
	if (IsDeschedulingPoint(psState, psInst))
	{
		if (psEfoState->psDeschedInstListTail != NULL)
		{
			psEfoState->psDeschedInstListTail->sStageData.psEfoData->psNextDeschedInst =
				psInst;
		}
		else
		{
			psEfoState->psDeschedInstListHead = psInst;
		}
		psEfoState->psDeschedInstListTail = psInst;
		psInst->sStageData.psEfoData->psNextDeschedInst = NULL;
	}
}

static PINST CreateEfoInst(PINTERMEDIATE_STATE psState, 
						   PCODEBLOCK psBlock,
						   IMG_UINT32 uInstA, 
						   IMG_UINT32 uInstB, 
						   IMG_PUINT32 puEfoInst,
						   PINST psSrcLineInst)
/*****************************************************************************
 FUNCTION	: CreateEfoInst
    
 PURPOSE	: Creates a new EFO instruction and adds it to the dependency graph
			  structures.

 PARAMETERS	: psState					- Compiler state.
			  psBlock					- Basic block to add the EFO to.
			  uInstA, uInstB			- Instructions which are being combined
										  into the EFO.
			  puEfoInst					- Returns the number of the EFO instruction.
			  psSrcLineInst             - Source instruction for which this 
			                              instruction is generated.
			  
 RETURNS	: A pointer to the new instruction.
*****************************************************************************/
{
	IMG_UINT32		uEfoInst;
	PINST			psEfoInst;
	PDGRAPH_STATE	psDepState = psBlock->psDepState;
	ASSERT(psBlock->psDepState != NULL);

	psEfoInst = AllocateInst(psState, psSrcLineInst);

	SetOpcodeAndDestCount(psState, psEfoInst, IEFO, EFO_DEST_COUNT);

	uEfoInst = AddNewInstToDGraph(psDepState, psEfoInst);

	EFO_TESTONLY(printf("\tCreating EFO %d from %d, %d\n", uEfoInst, uInstA, uInstB));

	SetupEfoStageData(psState, NULL, psEfoInst);

	/*
		Give the EFO the same dependencies as the two old instructions.
	*/
	MergeInstructions(psDepState, uEfoInst, uInstA, IMG_TRUE);
	MergeInstructions(psDepState, uEfoInst, uInstB, IMG_TRUE);

	/*
		Insert the EFO at the end of the block.
	*/
	InsertInstBefore(psState, psBlock, psEfoInst, NULL);

	*puEfoInst = uEfoInst;
	return psEfoInst;
}

static IMG_BOOL CanUseHardwareConstant(PINTERMEDIATE_STATE	psState,
									   IMG_UINT32			uArg)
/*****************************************************************************
 FUNCTION	: CanUseHardwareConstant
    
 PURPOSE	: Checks if its possible to use a hardware constant in an EFO
			  source (either directly or by using a secondary attribute with
			  the same value).

 PARAMETERS	: psState					- Compiler state.
			  uArg						- Argument number.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (
			/*
				545 EFOs can use hardware constants directly in source 2.
			*/
			(
				(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NEW_EFO_OPTIONS) != 0 &&
				uArg == 2
			) ||
			/*
				Check that there are secondary attributes available to replace hardware constants.
			*/
			(psState->sSAProg.uConstSecAttrCount + 6) <= psState->sSAProg.uInRegisterConstantLimit
	   )
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_BOOL CheckEfoTemplate(PINTERMEDIATE_STATE psState, 
								 const EFO_TEMPLATE* psTemplate, 
								 PINST psInstA, 
								 PINST psInstB, 
								 PIREG_STATUS psIRegStatus,
								 IMG_BOOL bSwap,
								 PINST psEfoInst)
/*****************************************************************************
 FUNCTION	: CheckEfoTemplate
    
 PURPOSE	: Check if an EFO can be made from a pair of instructions using
			  a specific template.

 PARAMETERS	: psState					- Compiler state.
			  psTemplate				- Template to check.
			  psInstA, psInstB			- Instructions to check.
			  uASrcInI0					- Bit mask of the arguments to instruction A
			  uASrcInI1					  that could be replaced by an internal register.
			  uBSrcInI0					- Bit mask of the arguments to instruction B
			  uBSrcInI1					  that could be replaced by an internal register.
			  psEfoInst					- If non-NULL then the instruction it points to
										  is replaced by the EFO.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_PCHAR	pszSameSources, pszIReg;
	IMG_UINT32	uArg;
	IMG_BOOL	bA0RightNegate = IMG_FALSE;
	IMG_BOOL	bA1LeftNegate = IMG_FALSE;
	
	/*
		Check each pair of sources are the same register with the same modifiers.
	*/
	for (pszSameSources = psTemplate->pszSameSources; *pszSameSources != '\0'; pszSameSources += 2)
	{
		PINST			psSame1Inst;
		IMG_UINT32		uSame1SrcIdx;
		PINST			psSame2Inst;
		IMG_UINT32		uSame2SrcIdx;
		IMG_BOOL		bNegate;
		IMG_BOOL		bDifferentNegate;

		bNegate = IMG_FALSE;
		if (*pszSameSources == '-')
		{
			bNegate = IMG_TRUE;
			pszSameSources++;
		}

		GetArg(psState, psInstA, psInstB, pszSameSources[0], &psSame1Inst, &uSame1SrcIdx);
		GetArg(psState, psInstA, psInstB, pszSameSources[1], &psSame2Inst, &uSame2SrcIdx);

		if (bNegate)
		{
			if (!EqualFloatSrcsIgnoreNegate(psState, psSame1Inst, uSame1SrcIdx, psSame2Inst, uSame2SrcIdx, &bDifferentNegate))
			{
				ASSERT(psEfoInst == NULL);
				return IMG_FALSE;
			}
			/*	
				If the negation modifiers differ then negate the A0 right hand source.
			*/
			bA0RightNegate = bDifferentNegate;
		}
		else
		{
			if (!EqualFloatSrcs(psState, psSame1Inst, uSame1SrcIdx, psSame2Inst, uSame2SrcIdx))
			{
				ASSERT(psEfoInst == NULL);
				return IMG_FALSE;
			}
		}
	}
	/*
		Check the sources from the two instructions fit with the bank restrictions on an EFO
		instruction.
	*/
	for (uArg = 0; uArg < 3; uArg++)
	{
		IMG_CHAR				cEfoSrc = psTemplate->pszEfoSources[uArg];
		if (cEfoSrc == '_')
		{
			if (psEfoInst != NULL)
			{
				psEfoInst->asArg[uArg].uType = USEASM_REGTYPE_IMMEDIATE;
				psEfoInst->asArg[uArg].uNumber = 0;
			}
		}
		else if (cEfoSrc == '1')
		{
			if (!CanUseHardwareConstant(psState, uArg))
			{
				ASSERT(psEfoInst == NULL);
				return IMG_FALSE;
			}
			if (psEfoInst != NULL)
			{
				psEfoInst->asArg[uArg].uType = USEASM_REGTYPE_FPCONSTANT;
				psEfoInst->asArg[uArg].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
			}
		}
		else
		{
			PINST		psArgInst;
			IMG_UINT32	uArgInstSrcIdx;

			GetArg(psState, psInstA, psInstB, cEfoSrc, &psArgInst, &uArgInstSrcIdx);
			ASSERT(psInstA->psBlock == psInstB->psBlock);
			if (!CanUseEfoSrc(psState, psInstA->psBlock, uArg, psArgInst, uArgInstSrcIdx))
			{
				ASSERT(psEfoInst == NULL);
				return IMG_FALSE;
			}
			if (psEfoInst != NULL)
			{
				MoveFloatArgToEfo(psState, psEfoInst, uArg, psArgInst, uArgInstSrcIdx);
			}
		}
	}
	/*
		Check that the right sources can be replaced by internal registers.
	*/
	for (pszIReg = psTemplate->pszI0Sources; *pszIReg != '\0'; pszIReg++)
	{
		IMG_BOOL	bCouldNegate = IMG_FALSE;
		if (*pszIReg == '-')
		{
			bCouldNegate = IMG_TRUE;
			pszIReg++;
		}
		if (!CheckEfoIReg(psState, *pszIReg, psIRegStatus, 0, bCouldNegate, bSwap, &bA0RightNegate))
		{
			ASSERT(psEfoInst == NULL);
			return IMG_FALSE;
		}
	}
	for (pszIReg = psTemplate->pszI1Sources; *pszIReg != '\0'; pszIReg++)
	{
		IMG_BOOL	bCouldNegate = IMG_FALSE;
		if (*pszIReg == '-')
		{
			bCouldNegate = IMG_TRUE;
			pszIReg++;
		}
		if (!CheckEfoIReg(psState, *pszIReg, psIRegStatus, 1, bCouldNegate, bSwap, &bA1LeftNegate))
		{
			ASSERT(psEfoInst == NULL);
			return IMG_FALSE;
		}
	}
	if (psEfoInst != NULL)
	{
		/*
			Copy the EFO parameters from the template.
		*/
		ASSERT(psEfoInst->u.psEfo != NULL);

		psEfoInst->u.psEfo->eM0Src0 = psTemplate->psEfoParams->eM0Src0;
		psEfoInst->u.psEfo->eM0Src1 = psTemplate->psEfoParams->eM0Src1;
		psEfoInst->u.psEfo->eM1Src0 = psTemplate->psEfoParams->eM1Src0;
		psEfoInst->u.psEfo->eM1Src1 = psTemplate->psEfoParams->eM1Src1;

		psEfoInst->u.psEfo->eA0Src0 = psTemplate->psEfoParams->eA0Src0;
		psEfoInst->u.psEfo->eA0Src1 = psTemplate->psEfoParams->eA0Src1;
		psEfoInst->u.psEfo->eA1Src0 = psTemplate->psEfoParams->eA1Src0;
		psEfoInst->u.psEfo->eA1Src1 = psTemplate->psEfoParams->eA1Src1;

		psEfoInst->u.psEfo->bA0RightNeg = bA0RightNegate;
		psEfoInst->u.psEfo->bA1LeftNeg = bA1LeftNegate;

		if (psTemplate->bWriteI0Only)
		{
			/*
				Write one of the instruction destinations using
				the EFO dest and the other through the i0.
			*/
			SetupIRegDests(psState,
						   psEfoInst, 
						   psTemplate->psEfoParams->eI0Src,
						   EFO_SRC_UNDEF);
			
			psEfoInst->u.psEfo->bIgnoreDest = IMG_FALSE;
			psEfoInst->u.psEfo->eDSrc = psTemplate->psEfoParams->eDSrc;

			if (psTemplate->bInstBIsFirst)
			{
				MoveDest(psState, psEfoInst, EFO_USFROMI0_DEST, psInstA, 0 /* uMoveFromDestIdx */);
				MoveDest(psState, psEfoInst, EFO_US_DEST, psInstB, 0 /* uMoveFromDestIdx */);
			}
			else
			{
				MoveDest(psState, psEfoInst, EFO_USFROMI0_DEST, psInstB, 0 /* uMoveFromDestIdx */);
				MoveDest(psState, psEfoInst, EFO_US_DEST, psInstA, 0 /* uMoveFromDestIdx */);
			}
		}
		else
		{
			/*
				Set up the EFO to write the destinations of the
				two instructions.
			*/
			SetupIRegDests(psState,
						   psEfoInst, 
						   psTemplate->psEfoParams->eI0Src,
						   psTemplate->psEfoParams->eI1Src);
			
			psEfoInst->u.psEfo->bIgnoreDest = IMG_TRUE;
	
			if (psTemplate->bInstBIsFirst)
			{
				MoveDest(psState, psEfoInst, EFO_USFROMI0_DEST, psInstB, 0 /* uMoveFromDestIdx */);
				MoveDest(psState, psEfoInst, EFO_USFROMI1_DEST, psInstA, 0 /* uMoveFromDestIdx */);
			}
			else
			{
				MoveDest(psState, psEfoInst, EFO_USFROMI0_DEST, psInstA, 0 /* uMoveFromDestIdx */);
				MoveDest(psState, psEfoInst, EFO_USFROMI1_DEST, psInstB, 0 /* uMoveFromDestIdx */);
			}
		}
	}

	return IMG_TRUE;
}

static IMG_BOOL CheckEfoTemplateTable(PINTERMEDIATE_STATE psState, 
									  const EFO_TEMPLATE* psTemplateTable, 
									  IMG_UINT32 uNumTemplateTableEntries,
									  PINST psInstA, 
									  PINST psInstB, 
									  PIREG_STATUS psIRegStatus,
									  IMG_BOOL bSwap,
									  const EFO_TEMPLATE** ppsSelectedEntry)
/*****************************************************************************
 FUNCTION	: CheckEfoTemplateTable
    
 PURPOSE	: Check if an EFO can be made from a pair of instructions using
			  any of the templates in a table.

 PARAMETERS	: psState					- Compiler state.
			  psTemplateTable			- Pointer to the table of EFO templates.
			  uNumTemplateTableEntries	- Number of entries in the table.
			  psInstA, psInstB			- Instructions to check.
			  uASrcInI0					- Bit mask of the arguments to instruction A
			  uASrcInI1					  that could be replaced by an internal register.
			  uBSrcInI0					- Bit mask of the arguments to instruction B
			  uBSrcInI1					  that could be replaced by an internal register.
			  ppsSelectedEntry			- If an EFO can be made then returns the
										  template to use.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uEntry;

	for (uEntry = 0; uEntry < uNumTemplateTableEntries; uEntry++)
	{
		const EFO_TEMPLATE* psEntry = &psTemplateTable[uEntry];
		if (CheckEfoTemplate(psState, 
							 psEntry, 
							 psInstA, 
							 psInstB, 
							 psIRegStatus, 
							 bSwap,
							 NULL))
		{
			*ppsSelectedEntry = psEntry;
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL IsValidEFOSourceModifier(PINTERMEDIATE_STATE psState, PINST psSrcInst, IMG_UINT32 uSrcInstArgIdx)
/*****************************************************************************
 FUNCTION	: IsValidEFOSourceModifier
    
 PURPOSE	: Check if the source modifier on an existing instruction source would
			  be valid if moved to an EFO instruction.

 PARAMETERS	: psState			- Compiler state.
			  psSrcInst			- Existing instruction.
			  uSrcInstArgIdx	- Existing instruction source.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	#if defined(SUPPORT_SGX545)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NEW_EFO_OPTIONS) != 0)
	{
		/*
			For SGX545 no absolute source modifiers on EFO.
		*/
		if (IsAbsolute(psState, psSrcInst, uSrcInstArgIdx))
		{
			return IMG_FALSE;
		}
	}
	#else /* defined(SUPPORT_SGX545) */
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psSrcInst);
	PVR_UNREFERENCED_PARAMETER(uSrcInstArgIdx);
	#endif /* defined(SUPPORT_SGX545) */
	return IMG_TRUE;
}

static IMG_BOOL CanUseEfoSrc(PINTERMEDIATE_STATE	psState, 
							 PCODEBLOCK				psBlock,
							 IMG_UINT32				uArg, 
							 PINST					psSrcInst,
							 IMG_UINT32				uSrcInstArgIdx)
/*****************************************************************************
 FUNCTION	: CanUseEfoSrc
    
 PURPOSE	: Check if a register can be used as an argument to an EFO instruction.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Flow control block containing the EFO instruction.
			  uArg				- Argument number.
			  psArg				- Register.
			  psArgMod			- Register source modifier.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32				uArgType;
	PARG					psArg = &psSrcInst->asArg[uSrcInstArgIdx];

	if (IsDriverLoadedSecAttr(psState, psArg))
	{
		if (psBlock->psOwner->psFunc == psState->psSecAttrProg)
		{
			uArgType = USEASM_REGTYPE_PRIMATTR;
		}
		else
		{
			uArgType = USEASM_REGTYPE_SECATTR;
		}
	}
	else
	{
		uArgType = psArg->uType;
	}

	#if defined(SUPPORT_SGX545)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NEW_EFO_OPTIONS)
	{
		if (uArg == 0 || uArg == 1)
		{
			if (uArgType != USEASM_REGTYPE_TEMP &&
				uArgType != USEASM_REGTYPE_OUTPUT &&
				uArgType != USEASM_REGTYPE_PRIMATTR &&
				uArgType != USEASM_REGTYPE_SECATTR &&
				uArgType != USEASM_REGTYPE_FPCONSTANT &&
				uArgType != USEASM_REGTYPE_FPINTERNAL)
			{
				return IMG_FALSE;
			}
			if (psArg->uIndexType != USC_REGTYPE_NOINDEX)
			{
				return IMG_FALSE;
			}
		}
	}
	else
	#endif /* defined(SUPPORT_SGX545) */
	{
		if (uArg == 0)
		{
			if (
					uArgType != USEASM_REGTYPE_TEMP &&
					uArgType != USEASM_REGTYPE_PRIMATTR &&
					uArgType != USEASM_REGTYPE_FPINTERNAL
			   )
			{
				return IMG_FALSE;
			}
		}
		else
		{
			if (
					uArgType != USEASM_REGTYPE_TEMP &&
					uArgType != USEASM_REGTYPE_PRIMATTR &&
					uArgType != USEASM_REGTYPE_OUTPUT &&
					uArgType != USEASM_REGTYPE_SECATTR &&
					uArgType != USEASM_REGTYPE_FPCONSTANT &&
					uArgType != USEASM_REGTYPE_FPINTERNAL
			   )
			{
				return IMG_FALSE;
			}
		}
		if (psArg->uIndexType != USC_REGTYPE_NOINDEX)
		{
			return IMG_FALSE;
		}

		if (psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_21697)
		{
			if (uArg == 0 && psArg->eFmt == UF_REGFORMAT_F16)
			{
				return IMG_FALSE;
			}
		}
	}

	if (!IsValidEFOSourceModifier(psState, psSrcInst, uSrcInstArgIdx))
	{
		return IMG_FALSE;
	}

	if (
			uArgType == USEASM_REGTYPE_FPCONSTANT &&
			!CanUseHardwareConstant(psState, uArg)	
	   )
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

static IMG_VOID UpdateClosedEfoDependencyGraph(PEFOGEN_STATE psEfoState, 
											   IMG_UINT32 uTo, 
											   IMG_UINT32 uFrom)
/*****************************************************************************
 FUNCTION	: UpdateClosedDependencyGraph
    
 PURPOSE	: Update the closed dependency graph when a new dependency is added.

 PARAMETERS	: psDepState		- Module state.
			  uTo				- Dependency destination.
			  uFrom				- Dependency source.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uEfoGroup, uDepend;
	IMG_UINT32	uGraphStride = UINTS_TO_SPAN_BITS(psEfoState->uEfoGroupCount);

	/*
		There is now a direct dependency between TO and FROM.
	*/
	SetClosedDependency(psEfoState, uTo, uFrom, 1);

	/*
		Any dependencies of FROM are now dependencies of TO together.
	*/
	for (uDepend = 0; uDepend < uGraphStride; uDepend++)
	{
		psEfoState->aauClosedEfoDependencyGraph[uTo * uGraphStride + uDepend] |= 
			psEfoState->aauClosedEfoDependencyGraph[uFrom * uGraphStride + uDepend];
	}

	/*
		Any group EFOGROUP which depends on TO now depends on FROM as well.
	*/
	for (uEfoGroup = 0; uEfoGroup < psEfoState->uEfoGroupCount; uEfoGroup++)
	{
		if (GetClosedDependency(psEfoState, uEfoGroup, uTo))
		{
			SetClosedDependency(psEfoState, uEfoGroup, uFrom, 1);
			for (uDepend = 0; uDepend < uGraphStride; uDepend++)
			{
				psEfoState->aauClosedEfoDependencyGraph[uEfoGroup * uGraphStride + uDepend] |= 
					psEfoState->aauClosedEfoDependencyGraph[uFrom * uGraphStride + uDepend];
			}
		}
	}
}

static IMG_VOID ReplaceInEfoReaderList(PINST psEfoInst, PINST psReaderInst, PINST psNewInst)
/*****************************************************************************
 FUNCTION	: RemoveFromEfoReaderList
    
 PURPOSE	: Replace an instruction in an EFO readers list.

 PARAMETERS	: psEfoInst		- The EFO instruction.
			  psReaderInst	- The reader instruction.
			  psNewInst		- The instruction to replace by.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psInst;

	psNewInst->sStageData.psEfoData->psNextReader = psReaderInst->sStageData.psEfoData->psNextReader;

	if (psEfoInst->sStageData.psEfoData->psFirstReader == psReaderInst)
	{
		psEfoInst->sStageData.psEfoData->psFirstReader = psNewInst;
		return;
	}
	for (psInst = psEfoInst->sStageData.psEfoData->psFirstReader; ; psInst = psInst->sStageData.psEfoData->psNextReader)
	{
		if (psInst->sStageData.psEfoData->psNextReader == psReaderInst)
		{
			psInst->sStageData.psEfoData->psNextReader = psNewInst;
		}
	}
}

static IMG_VOID AddToEfoReaderList(PINST psEfoInst, PINST psReaderInst)
/*****************************************************************************
 FUNCTION	: AddToEfoReaderList
    
 PURPOSE	: Add an instruction to the list of EFO readers.

 PARAMETERS	: psEfoInst		- The EFO instruction.
			  psReaderInst	- The reader instruction.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psInst;
	psReaderInst->sStageData.psEfoData->psNextReader = NULL;
	if (psEfoInst->sStageData.psEfoData->psFirstReader == NULL)
	{
		psEfoInst->sStageData.psEfoData->psFirstReader = psReaderInst;
		return;
	}
	for (psInst = psEfoInst->sStageData.psEfoData->psFirstReader; ; psInst = psInst->sStageData.psEfoData->psNextReader)
	{
		if (psInst == psReaderInst)
		{
			return;
		}
		if (psInst->sStageData.psEfoData->psNextReader == NULL)
		{
			psInst->sStageData.psEfoData->psNextReader = psReaderInst;
			return;
		}
	}
}

static IMG_BOOL CanUseStandardDest(PARG psArg)
/*****************************************************************************
 FUNCTION	: CanUseStandardDest
    
 PURPOSE	: Check if an argument could be used as a destination for an
			  instruction without extended banks.

 PARAMETERS	: 
			  
 RETURNS	: IMG_TRUE or IMG_FALSE.
*****************************************************************************/
{
	switch (psArg->uType)
	{
		case USEASM_REGTYPE_TEMP: return IMG_TRUE;
		case USEASM_REGTYPE_OUTPUT: return IMG_TRUE;
		case USEASM_REGTYPE_PRIMATTR: return IMG_TRUE;
		case USC_REGTYPE_REGARRAY:
		{
			if (psArg->uIndexType != USC_REGTYPE_NOINDEX)
			{
				return IMG_FALSE;
			}
			return IMG_TRUE;
		}
		default: return IMG_FALSE;
	}
}

/*****************************************************************************
 FUNCTION	: ReplaceHardwareConstants
    
 PURPOSE	: Replaces hardware constants in the sources to an instruction by
			  secondary attributes..

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction to replace the sources to.
			  
 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ReplaceHardwareConstants(PINTERMEDIATE_STATE	psState, 
										 PINST					psInst)
{
	IMG_UINT32	uArg;

	for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
	{
		#if defined(SUPPORT_SGX545)
		if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NEW_EFO_OPTIONS) && uArg == 2)
		{
			continue;
		}
		#endif /* defined(SUPPORT_SGX545) */
		if (psInst->asArg[uArg].uType == USEASM_REGTYPE_FPCONSTANT)
		{
			IMG_UINT32	uSecAttrType, uSecAttrNum;

			ReplaceHardwareConstBySecAttr(psState, 
										  psInst->asArg[uArg].uNumber, 
										  IMG_FALSE, 
										  &uSecAttrType, 
										  &uSecAttrNum);

			SetSrc(psState, psInst, uArg, uSecAttrType, uSecAttrNum, psInst->asArg[uArg].eFmt);
		}
	}
}

static IMG_BOOL MakeEfo_MulMad_SharedSrc(PINTERMEDIATE_STATE	psState,
										 PEFOGEN_STATE			psEfoState,
										 PINST					psInstA, 
										 PINST					psInstB, 
										 IMG_UINT32				uASrcInI0, 
										 IMG_UINT32				uASrcInI1,
										 IMG_UINT32				uBSrcInI0, 
										 IMG_UINT32				uBSrcInI1, 
										 IMG_UINT32				uBSrcFromADest, 
										 PINST					psEfoInst,
										 IMG_PBOOL				pbExtraSub)
/*****************************************************************************
 FUNCTION	: MakeEfo_MulMad_SharedSrc
    
 PURPOSE	: Make an EFO from a MUL/MAD with one shared source.

 PARAMETERS	: psState		- Compiler state.
			  psInstA		- Instructions to combine.
			  psInstB
			  uASrcInI0		- Instruction sources which could come from
			  uASrcInI1		  internal registers.
			  uBSrcInI0
			  uBSrcInI1
			  uBSrcFromADest - Instruction B sources which come from the
							   result of instruction A.
			  psEfoInst		 - If non-NULL then set up with the result of
							   combining the instruction.
							   If NULL just check if combining is possible.
			  pbExtraSub	 - On return set to TRUE if the caller should use
							 I0-I1 not I0+I1 for the extra add.
			  
 RETURNS	: IMG_TRUE or IMG_FALSE.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(pbExtraSub);

	/* Check for M0=SRC0*SRC1; M1=SRC0*SRC2; A0=M0+M1 */
	if (psInstA->eOpcode == IFMUL && 
		psInstB->eOpcode == IFMAD && 
		uBSrcFromADest == 4 &&
		(uASrcInI0 | uASrcInI1) == 0 &&
		(uBSrcInI0 | uBSrcInI1) == 0)
	{
		IMG_UINT32	uAArg, uBArg;

		for (uAArg = 0; uAArg < 2; uAArg++)
		{
			for (uBArg = 0; uBArg < 2; uBArg++)
			{
				if (EqualFloatSrcs(psState, psInstA, uAArg, psInstB, uBArg))
				{
					IMG_BOOL	bValidBanks = IMG_FALSE;

					if (psEfoState->bNewEfoFeature)
					{
						if (CanUseEfoSrc(psState, psEfoState->psCodeBlock, 2, psInstA, uAArg) &&
							CanUseEfoSrc(psState, psEfoState->psCodeBlock, 0, psInstA, 1 - uAArg) &&
							CanUseEfoSrc(psState, psEfoState->psCodeBlock, 1, psInstB, 1 - uBArg))
						{
							bValidBanks = IMG_TRUE;
						}
					}
					else
					{
						if (CanUseEfoSrc(psState, psEfoState->psCodeBlock, 0, psInstA, uAArg) &&
							CanUseEfoSrc(psState, psEfoState->psCodeBlock, 1, psInstA, 1 - uAArg) &&
							CanUseEfoSrc(psState, psEfoState->psCodeBlock, 2, psInstB, 1 - uBArg))
						{
							bValidBanks = IMG_TRUE;
						}
					}

					if (bValidBanks)
					{
						if (psEfoInst != NULL)
						{
							psEfoInst->u.psEfo->bIgnoreDest = IMG_FALSE;
							MoveDest(psState, psEfoInst, EFO_US_DEST /* uMoveToDestIdx */, psInstB, 0 /* uMoveFromDestIdx */);

							psEfoInst->sStageData.psEfoData->bSelfContained = IMG_TRUE;
	
							psEfoInst->u.psEfo->eDSrc = A0;
	
							psEfoInst->u.psEfo->eA0Src0 = M0;
							psEfoInst->u.psEfo->eA0Src1 = M1;
	
							if (psEfoState->bNewEfoFeature)
							{
								MoveFloatArgToEfo(psState, psEfoInst, 2 /* uDestArgIdx */, psInstA, uAArg /* uSrcArgIdx */);
								MoveFloatArgToEfo(psState, psEfoInst, 0 /* uDestArgIdx */, psInstA, 1 - uAArg /* uSrcArgIdx */);
								MoveFloatArgToEfo(psState, psEfoInst, 1 /* uDestArgIdx */, psInstB, 1 - uBArg /* uSrcArgIdx */);

								if (psInstB->u.psFloat->asSrcMod[2].bNegate)
								{
									InvertNegateModifier(psState, psEfoInst, 0 /* uArgIdx */);
								}

								psEfoInst->u.psEfo->eM0Src0 = SRC0;
								psEfoInst->u.psEfo->eM0Src1 = SRC2;
								psEfoInst->u.psEfo->eM1Src0 = SRC1;
								psEfoInst->u.psEfo->eM1Src1 = SRC2;
							}
							else
							{
								MoveFloatArgToEfo(psState, psEfoInst, 0 /* uDestArgIdx */, psInstA, uAArg /* uSrcArgIdx */);
								MoveFloatArgToEfo(psState, psEfoInst, 1 /* uDestArgIdx */, psInstA, 1 - uAArg /* uSrcArgIdx */);
								MoveFloatArgToEfo(psState, psEfoInst, 2 /* uDestArgIdx */, psInstB, 1 - uBArg /* uSrcArgIdx */);

								if (psInstB->u.psFloat->asSrcMod[2].bNegate)
								{
									InvertNegateModifier(psState, psEfoInst, 1 /* uArgIdx */);
								}

								psEfoInst->u.psEfo->eM0Src0 = SRC0;
								psEfoInst->u.psEfo->eM0Src1 = SRC1;
								psEfoInst->u.psEfo->eM1Src0 = SRC0;
								psEfoInst->u.psEfo->eM1Src1 = SRC2;
							}
						}	
						return IMG_TRUE;
					}
				}
			}
		}
	}
	ASSERT(psEfoInst == NULL);
	return IMG_FALSE;
}

static IMG_BOOL MakeEfo_MulMad_Square(PINTERMEDIATE_STATE	psState,
									  PEFOGEN_STATE			psEfoState,
									  PINST					psInstA, 
									  PINST					psInstB, 
									  IMG_UINT32			uASrcInI0, 
									  IMG_UINT32			uASrcInI1,
									  IMG_UINT32			uBSrcInI0, 
									  IMG_UINT32			uBSrcInI1, 
									  IMG_UINT32			uBSrcFromADest, 
									  PINST					psEfoInst,
									  IMG_PBOOL				pbExtraSub)
/*****************************************************************************
 FUNCTION	: MakeEfo_MulMad_Square
    
 PURPOSE	: Make an EFO from a MUL/MAD with one squared source.

 PARAMETERS	: psState		- Compiler state.
			  psInstA		- Instructions to combine.
			  psInstB
			  uASrcInI0		- Instruction sources which could come from
			  uASrcInI1		  internal registers.
			  uBSrcInI0
			  uBSrcInI1
			  uBSrcFromADest - Instruction B sources which come from the
							   result of instruction A.
			  psEfoInst		 - If non-NULL then set up with the result of
							   combining the instruction.
							   If NULL just check if combining is possible.
			  pbExtraSub	 - On return set to TRUE if the caller should use
							 I0-I1 not I0+I1 for the extra add.
			  
 RETURNS	: IMG_TRUE or IMG_FALSE.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(pbExtraSub);

	/* Check for M0=SRC1*SRC2; M1=SRC0*SRC0; A0=M0+M1 */
	if (psInstA->eOpcode == IFMUL && 
		psInstB->eOpcode == IFMAD && 
		uBSrcFromADest == 4 &&
		(uASrcInI0 | uASrcInI1) == 0 &&
		(uBSrcInI0 | uBSrcInI1) == 0)
	{
		IMG_BOOL				bA0RightNeg = IMG_FALSE;
		IMG_BOOL				bNegateOtherArg1 = IMG_FALSE;
		PINST					psSqrArgInst;
		PINST					psOtherInst;
		IMG_BOOL				bValidBanks;

		if (EqualFloatSrcs(psState, psInstA, 0 /* uInst1ArgIdx */, psInstA, 1 /* uInst2ArgIdx */))
		{
			psSqrArgInst = psInstA;
			psOtherInst = psInstB;

			if (psInstB->u.psFloat->asSrcMod[2].bNegate)
			{
				if (!psEfoState->bNewEfoFeature)
				{
					return IMG_FALSE;
				}
				bA0RightNeg = IMG_TRUE;
			}
		}
		else if (EqualFloatSrcs(psState, psInstB, 0 /* uInst1ArgIdx */, psInstB, 1 /* uInst2ArgIdx */))
		{
			psSqrArgInst = psInstB;
			psOtherInst = psInstA;

			if (psInstB->u.psFloat->asSrcMod[2].bNegate)
			{
				bNegateOtherArg1 = IMG_TRUE;
			}
		}
		else
		{
			return IMG_FALSE;
		}

		bValidBanks = IMG_FALSE;
		if (psEfoState->bNewEfoFeature)
		{
			if (CanUseEfoSrc(psState, psEfoState->psCodeBlock, 2, psSqrArgInst, 0 /* uSrcInstArgIdx */) &&
				CanUseEfoSrc(psState, psEfoState->psCodeBlock, 0, psOtherInst, 0 /* uSrcInstArgIdx */) &&
				CanUseEfoSrc(psState, psEfoState->psCodeBlock, 1, psOtherInst, 1 /* uSrcInstArgIdx */))
			{
				bValidBanks = IMG_TRUE;
			}
		}
		else
		{
			if (CanUseEfoSrc(psState, psEfoState->psCodeBlock, 0, psSqrArgInst, 0 /* uSrcInstArgIdx */) &&
				CanUseEfoSrc(psState, psEfoState->psCodeBlock, 1, psOtherInst, 0 /* uSrcInstArgIdx */) &&
				CanUseEfoSrc(psState, psEfoState->psCodeBlock, 2, psOtherInst, 1 /* uSrcInstArgIdx */))
			{
				bValidBanks = IMG_TRUE;
			}
		}

		if (bValidBanks)
		{
			if (psEfoInst != NULL)
			{
				IMG_UINT32	uOtherArg1;

				psEfoInst->u.psEfo->bIgnoreDest = IMG_FALSE;
				MoveDest(psState, psEfoInst, EFO_US_DEST /* uMoveToDestIdx */, psInstB, 0 /* uMoveFromDestIdx */);

				psEfoInst->sStageData.psEfoData->bSelfContained = IMG_TRUE;
	
				psEfoInst->u.psEfo->eDSrc = A0;
	
				/*
					Result = M0 + M1
				*/
				psEfoInst->u.psEfo->eA0Src0 = M0;
				psEfoInst->u.psEfo->eA0Src1 = M1;

				if (psEfoState->bNewEfoFeature)
				{
					MoveFloatArgToEfo(psState, psEfoInst, 2 /* uDestArgIdx */, psSqrArgInst, 0 /* uSrcArgIdx */);
					MoveFloatArgToEfo(psState, psEfoInst, 0 /* uDestArgIdx */, psOtherInst, 1 /* uSrcArgIdx */);
					uOtherArg1 = 0;
					MoveFloatArgToEfo(psState, psEfoInst, 1 /* uDestArgIdx */, psOtherInst, 2 /* uSrcArgIdx */);

					psEfoInst->u.psEfo->bA0RightNeg = bA0RightNeg;

					/*
						M0 = SRC1 * SRC2
						M1 = SRC2 * SRC2
					*/
					psEfoInst->u.psEfo->eM0Src0 = SRC1;
					psEfoInst->u.psEfo->eM0Src1 = SRC0;
					psEfoInst->u.psEfo->eM1Src0 = SRC2;
					psEfoInst->u.psEfo->eM1Src1 = SRC2;
				}
				else
				{
					MoveFloatArgToEfo(psState, psEfoInst, 0 /* uDestArgIdx */, psSqrArgInst, 0 /* uSrcArgIdx */);
					MoveFloatArgToEfo(psState, psEfoInst, 1 /* uDestArgIdx */, psOtherInst, 1 /* uSrcArgIdx */);
					uOtherArg1 = 1;
					MoveFloatArgToEfo(psState, psEfoInst, 2 /* uDestArgIdx */, psOtherInst, 2 /* uSrcArgIdx */);

					/*
						M0 = SRC1 * SRC2
						M1 = SRC0 * SRC0
					*/
					psEfoInst->u.psEfo->eM0Src0 = SRC1;
					psEfoInst->u.psEfo->eM0Src1 = SRC2;
					psEfoInst->u.psEfo->eM1Src0 = SRC0;
					psEfoInst->u.psEfo->eM1Src1 = SRC0;
				}

				if (bNegateOtherArg1)
				{
					InvertNegateModifier(psState, psEfoInst, uOtherArg1);
				}
			}	
			return IMG_TRUE;
		}
	}
	ASSERT(psEfoInst == NULL);
	return IMG_FALSE;
}

static IMG_BOOL MakeEfo_MulMad_IRegSrcs(PINTERMEDIATE_STATE		psState,
										PEFOGEN_STATE			psEfoState,
										PINST					psInstA, 
										PINST					psInstB, 
										IMG_UINT32				uASrcInI0, 
										IMG_UINT32				uASrcInI1,
										IMG_UINT32				uBSrcInI0, 
										IMG_UINT32				uBSrcInI1, 
										IMG_UINT32				uBSrcFromADest, 
										PINST					psEfoInst,
										IMG_PBOOL				pbExtraSub)
/*****************************************************************************
 FUNCTION	: MakeEfo_MulMad_IRegSrcs
    
 PURPOSE	: Make an EFO from a MUL/MAD where some sources come from internal
			  registers.

 PARAMETERS	: psState		- Compiler state.
			  psInstA		- Instructions to combine.
			  psInstB
			  uASrcInI0		- Instruction sources which could come from
			  uASrcInI1		  internal registers.
			  uBSrcInI0
			  uBSrcInI1
			  uBSrcFromADest - Instruction B sources which come from the
							   result of instruction A.
			  psEfoInst		 - If non-NULL then set up with the result of
							   combining the instruction.
							   If NULL just check if combining is possible.
			  pbExtraSub	 - On return set to TRUE if the caller should use
							 I0-I1 not I0+I1 for the extra add.
			  
 RETURNS	: IMG_TRUE or IMG_FALSE.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(pbExtraSub);

	/* Check for M0=SRC0*I0; M1=SRC1*I1; A0=M0+M1 */
	if (psInstA->eOpcode == IFMUL && psInstB->eOpcode == IFMAD && uBSrcFromADest == 4)
	{
		IMG_UINT32	uAArg, uBArg;

		for (uAArg = 0; uAArg < 2; uAArg++)
		{
			for (uBArg = 0; uBArg < 2; uBArg++)
			{
				PINST		psI0MulInst = NULL;
				IMG_UINT32	uI0MulInstArg = USC_UNDEF;
				PINST		psI1MulInst = NULL;
				IMG_UINT32	uI1MulInstArg = USC_UNDEF;
				EFO_SRC		eAIReg = EFO_SRC_UNDEF;

				if (uASrcInI0 == (IMG_UINT32)(1 << uAArg) && uBSrcInI1 == (IMG_UINT32)(1 << uBArg))
				{
					psI0MulInst = psInstA;
					uI0MulInstArg = 1 - uAArg;

					psI1MulInst = psInstB;
					uI1MulInstArg = 1 - uBArg;

					eAIReg = I0;
				}
				if (uASrcInI1 == (IMG_UINT32)(1 << uAArg) && uBSrcInI0 == (IMG_UINT32)(1 << uBArg))
				{
					psI0MulInst = psInstB;
					uI0MulInstArg = 1 - uBArg;

					psI1MulInst = psInstA;
					uI1MulInstArg = 1 - uAArg;

					eAIReg = I1;
				}

				if (psI0MulInst != NULL && psI1MulInst != NULL)
				{
					IMG_BOOL	bValidBanks = IMG_FALSE;

					if (psEfoState->bNewEfoFeature)
					{
						if (CanUseEfoSrc(psState, psEfoState->psCodeBlock, 1, psI0MulInst, uI0MulInstArg) && 
							CanUseEfoSrc(psState, psEfoState->psCodeBlock, 2, psI1MulInst, uI1MulInstArg))
						{
							bValidBanks = IMG_TRUE;
						}
					}
					else
					{
						if (CanUseEfoSrc(psState, psEfoState->psCodeBlock, 1, psI0MulInst, uI0MulInstArg) && 
							CanUseEfoSrc(psState, psEfoState->psCodeBlock, 0, psI1MulInst, uI1MulInstArg))
						{
							bValidBanks = IMG_TRUE;
						}
					}

					if (bValidBanks)
					{
						if (psEfoInst != NULL)
						{
							IMG_UINT32	uAIRegMultiplicand;

							psEfoInst->u.psEfo->bIgnoreDest = IMG_FALSE;
							MoveDest(psState, psEfoInst, EFO_US_DEST /* uMoveToDestIdx */, psInstB, 0 /* uMoveFromDestIdx */);
	
							psEfoInst->u.psEfo->eDSrc = A0;
	
							psEfoInst->u.psEfo->eA0Src0 = M0;
							psEfoInst->u.psEfo->eA0Src1 = M1;

							if (psEfoState->bNewEfoFeature)
							{
								MoveFloatArgToEfo(psState, psEfoInst, 1 /* uDestArgIdx */, psI0MulInst, uI0MulInstArg);
								MoveFloatArgToEfo(psState, psEfoInst, 2 /* uDestArgIdx */, psI1MulInst, uI1MulInstArg);
								psEfoInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
								psEfoInst->asArg[0].uNumber = 0;

								uAIRegMultiplicand = (eAIReg == I0) ? 1U : 2U;

								psEfoInst->u.psEfo->eM0Src0 = SRC1;
								psEfoInst->u.psEfo->eM0Src1 = I0;
								psEfoInst->u.psEfo->eM1Src0 = SRC2;
								psEfoInst->u.psEfo->eM1Src1 = I1;
							}
							else
							{
								MoveFloatArgToEfo(psState, psEfoInst, 1 /* uDestArgIdx */, psI0MulInst, uI0MulInstArg);
								MoveFloatArgToEfo(psState, psEfoInst, 0 /* uDestArgIdx */, psI1MulInst, uI1MulInstArg);
								psEfoInst->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
								psEfoInst->asArg[2].uNumber = 0;

								uAIRegMultiplicand = (eAIReg == I0) ? 1U : 0U;

								psEfoInst->u.psEfo->eM0Src0 = SRC1;
								psEfoInst->u.psEfo->eM0Src1 = I0;
								psEfoInst->u.psEfo->eM1Src0 = SRC0;
								psEfoInst->u.psEfo->eM1Src1 = I1;
							}

							if (IsNegated(psState, psInstB, 2 /* uArgIdx */))
							{
								InvertNegateModifier(psState, psEfoInst, uAIRegMultiplicand);
							}
						}

						return IMG_TRUE;
					}
				}
			}
		}
	}

	ASSERT(psEfoInst == NULL);
	return IMG_FALSE;
}

static IMG_BOOL MakeEfo_MadMad_Lrp(PINTERMEDIATE_STATE	psState,
								   PEFOGEN_STATE		psEfoState,
								   PINST				psInstA, 
								   PINST				psInstB, 
								   IMG_UINT32			uASrcInI0, 
								   IMG_UINT32			uASrcInI1,
								   IMG_UINT32			uBSrcInI0, 
								   IMG_UINT32			uBSrcInI1, 
								   IMG_UINT32			uBSrcFromADest, 
								   PINST				psEfoInst,
								   IMG_PBOOL			pbExtraSub)
/*****************************************************************************
 FUNCTION	: MakeEfo_MadMad_Lrp
    
 PURPOSE	: Make an EFO from a MAD/MAD doing a linear interpolate calculation.

 PARAMETERS	: psState		- Compiler state.
			  psInstA		- Instructions to combine.
			  psInstB
			  uASrcInI0		- Instruction sources which could come from
			  uASrcInI1		  internal registers.
			  uBSrcInI0
			  uBSrcInI1
			  uBSrcFromADest - Instruction B sources which come from the
							   result of instruction A.
			  psEfoInst		 - If non-NULL then set up with the result of
							   combining the instruction.
							   If NULL just check if combining is possible.
			  pbExtraSub	 - On return set to TRUE if the caller should use
							 I0-I1 not I0+I1 for the extra add.
			  
 RETURNS	: IMG_TRUE or IMG_FALSE.
*****************************************************************************/
{
	/* Check for I0=A0=SRC0*SRC1+SRC2 I1=M1=SRC0*SRC2 D=I0+I1. */
	if (psInstA->eOpcode == IFMAD &&
		psInstB->eOpcode == IFMAD &&
		uBSrcFromADest == 4 &&
		!psInstB->u.psFloat->asSrcMod[2].bNegate &&
		(uASrcInI0 | uASrcInI1) == 0 &&
		(uBSrcInI0 | uBSrcInI1) == 0)
	{
		IMG_UINT32	uBCandidate;
		IMG_UINT32	uACommonSrc;
		IMG_BOOL	bExtraSub = IMG_FALSE;
		IMG_BOOL	bDifferentCNegate;
		IMG_BOOL	bDifferentCommonSrcNegate;

		/*
			Look for two instructions of the form

				MAD		T, A, B, C
				MAD		D, A, C, T
		*/
		if (EqualFloatSrcsIgnoreNegate(psState, 
									   psInstA, 
									   2 /* uInst1SrcIdx */, 
									   psInstB, 
									   0 /* uInst2SrcIdx */, 
									   &bDifferentCNegate))
		{
			uBCandidate = 1;
		}
		else if (EqualFloatSrcsIgnoreNegate(psState,
											psInstA, 
											2 /* uInst1SrcIdx */, 
											psInstB, 
											1 /* uInst2SrcIdx */, 
											&bDifferentCNegate))
		{
			uBCandidate = 0;
		}
		else
		{
			ASSERT(psEfoInst == NULL);
			return IMG_FALSE;
		}

		if (EqualFloatSrcsIgnoreNegate(psState,
									   psInstA, 
									   0 /* uInst1SrcIdx */, 
									   psInstB, 
									   uBCandidate /* uInst2SrcIdx */, 
									   &bDifferentCommonSrcNegate))
		{
			uACommonSrc = 0;
		}
		else if (EqualFloatSrcsIgnoreNegate(psState,
											psInstA, 
											1 /* uInst1SrcIdx */, 
											psInstB, 
											uBCandidate /* uInst2SrcIdx */, 
											&bDifferentCommonSrcNegate))
		{
			uACommonSrc = 1;
		}
		else
		{
			ASSERT(psEfoInst == NULL);
			return IMG_FALSE;
		}

		/*
			We're going to convert to an EFO which calculates
				I0 = A * B + C
				I1 = A * C

			The negate flag on C in the second MAD disappears so check if
			we need to negate I1 when doing I0+I1.
		*/
		if (bDifferentCNegate)
		{
			bExtraSub = IMG_TRUE;
		}
		if (bDifferentCommonSrcNegate)
		{
			bExtraSub = (!bExtraSub) ? IMG_TRUE : IMG_FALSE;
		}

		/*
			Check bank restrictions.
		*/
		if (psEfoState->bNewEfoFeature)
		{
			if (!CanUseEfoSrc(psState, psEfoState->psCodeBlock, 2, psInstA, uACommonSrc) ||
				!CanUseEfoSrc(psState, psEfoState->psCodeBlock, 1, psInstA, 1 - uACommonSrc) ||
				!CanUseEfoSrc(psState, psEfoState->psCodeBlock, 0, psInstA, 2))
			{
				ASSERT(psEfoInst == NULL);
				return IMG_FALSE;
			}
		}
		else
		{
			if (!CanUseEfoSrc(psState, psEfoState->psCodeBlock, 0, psInstA, uACommonSrc) ||
				!CanUseEfoSrc(psState, psEfoState->psCodeBlock, 1, psInstA, 1 - uACommonSrc) ||
				!CanUseEfoSrc(psState, psEfoState->psCodeBlock, 2, psInstA, 2))
			{
				ASSERT(psEfoInst == NULL);
				return IMG_FALSE;
			}	 
		}

		if (psEfoInst != NULL)
		{
			SetOpcodeAndDestCount(psState, psEfoInst, IEFO, EFO_DEST_COUNT);

			psEfoInst->u.psEfo->bIgnoreDest = IMG_TRUE;
			SetDest(psState, psEfoInst, EFO_US_DEST, USEASM_REGTYPE_TEMP, GetNextRegister(psState), UF_REGFORMAT_F32);

			if (psEfoState->bNewEfoFeature)
			{
				MoveFloatArgToEfo(psState, psEfoInst, 2, psInstA, uACommonSrc);
				MoveFloatArgToEfo(psState, psEfoInst, 1, psInstA, 1 - uACommonSrc);
				MoveFloatArgToEfo(psState, psEfoInst, 0, psInstA, 2);

				psEfoInst->u.psEfo->eM0Src0 = SRC0;
				psEfoInst->u.psEfo->eM0Src1 = SRC2;
				psEfoInst->u.psEfo->eM1Src0 = SRC1;
				psEfoInst->u.psEfo->eM1Src1 = SRC2;
				psEfoInst->u.psEfo->eA0Src0 = M0;
				psEfoInst->u.psEfo->eA0Src1 = SRC0;

				/*
					The 545 EFO does
						
						  I0 = A*C + C
						  I1 = A*B

					I0 can't be negated in the following ADD so instead negate C as a
					source and then again (to cancel) in the A0 adder.
				*/
				if (bExtraSub)
				{
					InvertNegateModifier(psState, psEfoInst, 0 /* uArgIdx */);

					psEfoInst->u.psEfo->bA0RightNeg = IMG_TRUE;
				}
			}
			else
			{
				MoveFloatArgToEfo(psState, psEfoInst, 0, psInstA, uACommonSrc);
				MoveFloatArgToEfo(psState, psEfoInst, 1, psInstA, 1 - uACommonSrc);
				MoveFloatArgToEfo(psState, psEfoInst, 2, psInstA, 2);

				psEfoInst->u.psEfo->eM0Src0 = SRC0;
				psEfoInst->u.psEfo->eM0Src1 = SRC1;
				psEfoInst->u.psEfo->eM1Src0 = SRC0;
				psEfoInst->u.psEfo->eM1Src1 = SRC2;
				psEfoInst->u.psEfo->eA0Src0 = M0;
				psEfoInst->u.psEfo->eA0Src1 = SRC2;

				*pbExtraSub = bExtraSub;
			}

			psEfoInst->u.psEfo->eA1Src0 = I1;
			psEfoInst->u.psEfo->eA1Src1 = I0;	

			/*
				Write:
					I0 = A0
					I1 = M1
			*/
			SetupIRegDests(psState, psEfoInst, A0, M1);
		}

		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_BOOL MakeEfo_EfoMad_Lrp_535(PINTERMEDIATE_STATE	psState,
									   PEFOGEN_STATE		psEfoState,
									   PINST				psInstA, 
									   PINST				psInstB, 
									   IMG_UINT32			uASrcInI0, 
									   IMG_UINT32			uASrcInI1,
									   IMG_UINT32			uBSrcInI0, 
									   IMG_UINT32			uBSrcInI1, 
									   IMG_UINT32			uBSrcFromADest, 
									   PINST				psEfoInst,
									   IMG_PBOOL			pbExtraSub)
/*****************************************************************************
 FUNCTION	: MakeEfo_MadMad_Lrp_535
    
 PURPOSE	: Make an EFO from a EFO/MAD doing a linear interpolate calculation.

 PARAMETERS	: psState		- Compiler state.
			  psInstA		- Instructions to combine.
			  psInstB
			  uASrcInI0		- Instruction sources which could come from
			  uASrcInI1		  internal registers.
			  uBSrcInI0
			  uBSrcInI1
			  uBSrcFromADest - Instruction B sources which come from the
							   result of instruction A.
			  psEfoInst		 - If non-NULL then set up with the result of
							   combining the instruction.
							   If NULL just check if combining is possible.
			  pbExtraSub	 - On return set to TRUE if the caller should use
							 I0-I1 not I0+I1 for the extra add.
			  
 RETURNS	: IMG_TRUE or IMG_FALSE.
*****************************************************************************/
{
	/* 
	  I0 = A0 = (a * b) + c, A1 = I1 + I0; I0 = A0, I1 = A1; fmad
	 */
	if (!psEfoState->bNewEfoFeature &&
		psInstA->eOpcode == IEFO &&
		psInstB->eOpcode == IFMAD &&
		uBSrcFromADest == 4 &&
		!psInstB->u.psFloat->asSrcMod[2].bNegate &&
		(uASrcInI0 | uASrcInI1) == 0 &&
		(uBSrcInI0 | uBSrcInI1) == 0 &&
		psInstA->u.psEfo->eM0Src0 == SRC0 && psInstA->u.psEfo->eM0Src1 == SRC1 &&
		psInstA->u.psEfo->eA0Src0 == M0 && psInstA->u.psEfo->eA0Src1 == SRC2 &&
		psInstA->u.psEfo->eA1Src0 == I1 && psInstA->u.psEfo->eA1Src1 == I0 &&
		psInstA->u.psEfo->bIgnoreDest &&
		psInstA->u.psEfo->eI0Src == A0 &&
		psInstA->u.psEfo->eI1Src == A1 &&
		psInstA->u.psEfo->bWriteI0 &&
		psInstA->u.psEfo->bWriteI1)
	{
		IMG_UINT32	uBCandidate;
		IMG_UINT32	uACommonSrc;
		IMG_BOOL	bBDifferentNegate;
		IMG_BOOL	bCDifferentNegate;

		if (EqualFloatSrcsIgnoreNegate(psState,
									   psInstA, 
									   2 /* uInst1SrcIdx */, 
									   psInstB, 
									   0 /* uInst2SrcIdx */, 
									   &bCDifferentNegate))
		{
			uBCandidate = 1;
		}
		else if (EqualFloatSrcsIgnoreNegate(psState,
											psInstA, 
											2 /* uInst1SrcIdx */, 
											psInstB, 
											1 /* uInst2SrcIdx */, 
											&bCDifferentNegate))
		{
			uBCandidate = 0;
		}
		else
		{
			ASSERT(psEfoInst == NULL);
			return IMG_FALSE;
		}

		if (EqualFloatSrcsIgnoreNegate(psState,
									   psInstA, 
									   0 /* uInst1SrcIdx */, 
									   psInstB, 
									   uBCandidate /* uInst2SrcIdx */, 
									   &bBDifferentNegate))
		{
			uACommonSrc = 0;
		}
		else if (EqualFloatSrcsIgnoreNegate(psState,
										   psInstA, 
										   1 /* uInst1SrcIdx */, 
										   psInstB, 
										   uBCandidate /* uInst2SrcIdx */, 
										   &bBDifferentNegate))
		{
			uACommonSrc = 1;
		}
		else
		{
			ASSERT(psEfoInst == NULL);
			return IMG_FALSE;
		}

		if (!CanUseEfoSrc(psState, psEfoState->psCodeBlock, 0, psInstA, uACommonSrc) ||
			!CanUseEfoSrc(psState, psEfoState->psCodeBlock, 1, psInstA, 1 - uACommonSrc) ||
			!CanUseEfoSrc(psState, psEfoState->psCodeBlock, 2, psInstA, 2))
		{
			ASSERT(psEfoInst == NULL);
			return IMG_FALSE;
		}

		if (psEfoInst != NULL)
		{
			/*
				We	have T = A*B + C
						 D = C*B + T

				The new instruction will only have one instance of C as an argument so check
				if the negate flags on the two existing instances differ.
			*/
			if (bCDifferentNegate)
			{
				*pbExtraSub = IMG_TRUE;
			}
			/*
				Similarly for the two instances of B.
			*/
			if (bBDifferentNegate)
			{
				*pbExtraSub = (!(*pbExtraSub)) ? IMG_TRUE : IMG_FALSE;
			}

			MoveFloatArgToEfo(psState, psEfoInst, 0 /* uDestArgIdx */, psInstA, uACommonSrc /* uSrcArgIdx */);
			MoveFloatArgToEfo(psState, psEfoInst, 1 /* uDestArgIdx */, psInstA, 1 - uACommonSrc /* uSrcArgIdx */);
			MoveFloatArgToEfo(psState, psEfoInst, 2 /* uDestArgIdx */, psInstA, 2 /* uSrcArgIdx */);

			psEfoInst->u.psEfo->eM0Src0 = SRC0;
			psEfoInst->u.psEfo->eM0Src1 = SRC1;
			psEfoInst->u.psEfo->eM1Src0 = SRC0;
			psEfoInst->u.psEfo->eM1Src1 = SRC2;

			psEfoInst->u.psEfo->eA0Src0 = M0;
			psEfoInst->u.psEfo->eA0Src1 = SRC2;
			psEfoInst->u.psEfo->eA1Src0 = I1;
			psEfoInst->u.psEfo->eA1Src1 = I0;

			psEfoInst->u.psEfo->bA0RightNeg = psInstA->u.psEfo->bA0RightNeg;
			psEfoInst->u.psEfo->bA1LeftNeg = psInstA->u.psEfo->bA1LeftNeg;
	
			psEfoInst->u.psEfo->bIgnoreDest = IMG_FALSE;
			psEfoInst->u.psEfo->eDSrc = A1;
			MoveDest(psState, 
					 psEfoInst, 
					 EFO_US_DEST /* uMoveToDestIdx */, 
					 psInstA, 
					 EFO_USFROMI1_DEST /* uMoveFromDestIdx */);

			/*
				Write:
					I0 = A0
					I1 = M1
			*/
			SetupIRegDests(psState, psEfoInst, A0, M1);
		}

		return IMG_TRUE;
	}
	ASSERT(psEfoInst == NULL);
	return IMG_FALSE;
}

static IMG_BOOL MakeEfo_EfoMad_Lrp_545(PINTERMEDIATE_STATE	psState,
									   PEFOGEN_STATE		psEfoState,
									   PINST				psInstA, 
									   PINST				psInstB, 
									   IMG_UINT32			uASrcInI0, 
									   IMG_UINT32			uASrcInI1,
									   IMG_UINT32			uBSrcInI0, 
									   IMG_UINT32			uBSrcInI1, 
									   IMG_UINT32			uBSrcFromADest, 
									   PINST				psEfoInst,
									   IMG_PBOOL			pbExtraSub)
/*****************************************************************************
 FUNCTION	: MakeEfo_MadMad_Lrp_545
    
 PURPOSE	: Make an EFO from a EFO/MAD doing a linear interpolate calculation.

 PARAMETERS	: psState		- Compiler state.
			  psInstA		- Instructions to combine.
			  psInstB
			  uASrcInI0		- Instruction sources which could come from
			  uASrcInI1		  internal registers.
			  uBSrcInI0
			  uBSrcInI1
			  uBSrcFromADest - Instruction B sources which come from the
							   result of instruction A.
			  psEfoInst		 - If non-NULL then set up with the result of
							   combining the instruction.
							   If NULL just check if combining is possible.
			  pbExtraSub	 - On return set to TRUE if the caller should use
							 I0-I1 not I0+I1 for the extra add.
			  
 RETURNS	: IMG_TRUE or IMG_FALSE.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(pbExtraSub);

	/*
		Check for:
		
		an EFO which does 
			A0 = SRC2*SRC1+SRC0 /
			A1 = I0+I1.
			I0DEST = I0 = A0
			I1DEST = I1 = A1
		a MAD which does
			DEST = A * B + I0DEST
	*/
	if (psEfoState->bNewEfoFeature &&
		psInstA->eOpcode == IEFO &&
		psInstB->eOpcode == IFMAD &&
		uBSrcFromADest == 4 &&
		!psInstB->u.psFloat->asSrcMod[2].bNegate &&
		(uASrcInI0 | uASrcInI1) == 0 &&	(uBSrcInI0 | uBSrcInI1) == 0 &&
		psInstA->u.psEfo->eM0Src0 == SRC2 && psInstA->u.psEfo->eM0Src1 == SRC1 &&
		psInstA->u.psEfo->eA0Src0 == M0 && psInstA->u.psEfo->eA0Src1 == SRC0 &&
		psInstA->u.psEfo->eA1Src0 == I1 && psInstA->u.psEfo->eA1Src1 == I0 &&
		!psInstA->u.psEfo->bA0RightNeg &&
		psInstA->u.psEfo->bIgnoreDest &&
		psInstA->u.psEfo->eI0Src == A0 &&
		psInstA->u.psEfo->eI1Src == A1 &&
		psInstA->u.psEfo->bWriteI0 &&
		psInstA->u.psEfo->bWriteI1)
	{
		IMG_UINT32	uBCandidate;
		IMG_UINT32	uACommonSrc, uAOtherSrc;
		IMG_BOOL	bBDifferentNegate;
		IMG_BOOL	bCDifferentNegate;

		/*
			Check that one of the first two arguments to the MAD is the
			same as the first argument to the EFO
		*/
		if (EqualFloatSrcsIgnoreNegate(psState,
									   psInstA, 
									   0 /* uInst1SrcIdx */, 
									   psInstB, 
									   0 /* uInst2SrcIdx */, 
									   &bCDifferentNegate))
		{
			uBCandidate = 1;
		}
		else if (EqualFloatSrcsIgnoreNegate(psState,
											psInstA, 
											0 /* uInst1SrcIdx */, 
											psInstB, 
											1 /* uInst2SrcIdx */, 
											&bCDifferentNegate))
		{
			uBCandidate = 0;
		}
		else
		{
			ASSERT(psEfoInst == NULL);
			return IMG_FALSE;
		}

		/*
			Check the other argument to the MAD is the same as one of the
			last two arguments to the EFO.
		*/
		if (EqualFloatSrcsIgnoreNegate(psState,
									   psInstA, 
									   2 /* uInst1SrcIdx */, 
									   psInstB, 
									   uBCandidate /* uInst2SrcIdx */, 
									   &bBDifferentNegate))
		{
			uACommonSrc = 2;
			uAOtherSrc = 1;
		}
		else if (EqualFloatSrcsIgnoreNegate(psState,
										    psInstA, 
											1 /* uInst1SrcIdx */, 
											psInstB, 
											uBCandidate /* uInst2SrcIdx */, 
											&bBDifferentNegate))
		{
			uACommonSrc = 1;
			uAOtherSrc = 2;
		}
		else
		{
			ASSERT(psEfoInst == NULL);
			return IMG_FALSE;
		}

		/*
			Check the arguments fit with the EFO source bank restrictions.
		*/
		if (!CanUseEfoSrc(psState, psEfoState->psCodeBlock, 2, psInstA, uACommonSrc) ||
			!CanUseEfoSrc(psState, psEfoState->psCodeBlock, 1, psInstA, uAOtherSrc) ||
			!CanUseEfoSrc(psState, psEfoState->psCodeBlock, 0, psInstA, 0))
		{
			ASSERT(psEfoInst == NULL);
			return IMG_FALSE;
		}

		if (psEfoInst != NULL)
		{
			MoveFloatArgToEfo(psState, psEfoInst, 2 /* uDestArgIdx */, psInstB, uBCandidate /* uSrcArgIdx */);
			MoveFloatArgToEfo(psState, psEfoInst, 1 /* uDestArgIdx */, psInstA, uAOtherSrc /* uSrcArgIdx */);
			MoveFloatArgToEfo(psState, psEfoInst, 0 /* uDestArgIdx */, psInstB, 1 - uBCandidate /* uSrcArgIdx */);

			/*
				We	have T = A*B + C
						 D = C*B + T

				The new instruction will only have one instance of C as an argument so check
				if the negate flags on the two existing instances differ.
			*/
			if (bCDifferentNegate)
			{
				psEfoInst->u.psEfo->bA0RightNeg = IMG_TRUE;
			}
			/*
				Similarly if the two existing instances of B have different negate flags.
			*/
			if (bBDifferentNegate)
			{
				*pbExtraSub = (!(*pbExtraSub)) ? IMG_TRUE : IMG_FALSE;
			}

			/*
				M0 = SRC0 * SRC2
				M1 = SRC1 * SRC2
			*/
			psEfoInst->u.psEfo->eM0Src0 = SRC0;
			psEfoInst->u.psEfo->eM0Src1 = SRC2;
			psEfoInst->u.psEfo->eM1Src0 = SRC1;
			psEfoInst->u.psEfo->eM1Src1 = SRC2;

			/*
				A0 = M0 + SRC0
				A1 = I1 + I0
			*/
			psEfoInst->u.psEfo->eA0Src0 = M0;
			psEfoInst->u.psEfo->eA0Src1 = SRC0;
			psEfoInst->u.psEfo->eA1Src0 = I1;
			psEfoInst->u.psEfo->eA1Src1 = I0;

			psEfoInst->u.psEfo->bA1LeftNeg = psInstA->u.psEfo->bA1LeftNeg;
	
			psEfoInst->u.psEfo->bIgnoreDest = IMG_FALSE;
			psEfoInst->u.psEfo->eDSrc = A1;
			MoveDest(psState, 
					 psEfoInst, 
					 EFO_US_DEST /* uMoveToDestIdx */, 
					 psInstA, 
					 EFO_USFROMI1_DEST /* uMoveFromDestIdx */);

			/*
				I0 = A0
				I1 = M1

				Another instruction will do MAD_DEST = I0 + I1
			*/
			SetupIRegDests(psState, psEfoInst, A0, M1);
		}
		return IMG_TRUE;
	}

	return IMG_FALSE;
}

static IMG_BOOL CanCombineDependentsIntoEfo(PINTERMEDIATE_STATE		psState,
											PEFOGEN_STATE			psEfoState,
											PINST					psInstA, 
											PINST					psInstB, 
											PIREG_STATUS			psIRegStatus,
											IMG_UINT32				uBSrcFromADest, 
											IMG_PBOOL				pbExtraAdd,
											PFN_EFO_BUILDER*		ppfnSelectedBuilder)
/*****************************************************************************
 FUNCTION	: CanCombineDependentsIntoEfo
    
 PURPOSE	: Checks if two instructions can be combined into an IEFO instruction
			  where some of the sources of the second instruction are the result
			  of the first.

 PARAMETERS	: 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32 uEntry;
	static const struct
	{
		PFN_EFO_BUILDER	pfnBuilder;
		IMG_BOOL		bExtraAdd;
	} sEfoBuilders[] = 
	{
		{MakeEfo_MulMad_SharedSrc, IMG_FALSE},
		{MakeEfo_MulMad_Square, IMG_FALSE},
		{MakeEfo_MulMad_IRegSrcs, IMG_FALSE},
		{MakeEfo_MadMad_Lrp, IMG_TRUE},
		{MakeEfo_EfoMad_Lrp_535, IMG_TRUE},
		{MakeEfo_EfoMad_Lrp_545, IMG_TRUE},
	};

	ASSERT(uBSrcFromADest);
	ASSERT((uBSrcFromADest & (psIRegStatus->puBSrcInIReg[0] | psIRegStatus->puBSrcInIReg[1])) == 0);

	/* Check if a pair of dependent instructions can be combined. */
	for (uEntry = 0; uEntry < (sizeof(sEfoBuilders) / sizeof(sEfoBuilders[0])); uEntry++)
	{
		if (sEfoBuilders[uEntry].pfnBuilder(psState,
											psEfoState,
											psInstA,
											psInstB,
											psIRegStatus->puASrcInIReg[0],
											psIRegStatus->puASrcInIReg[1],
											psIRegStatus->puBSrcInIReg[0],
											psIRegStatus->puBSrcInIReg[1],
											uBSrcFromADest,
											NULL,
											NULL))
		{
			*pbExtraAdd = sEfoBuilders[uEntry].bExtraAdd;
			*ppfnSelectedBuilder = sEfoBuilders[uEntry].pfnBuilder;
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL CanCombineIntoEfo(PINTERMEDIATE_STATE psState,
								  PINST psInstA, 
								  PINST psInstB, 
								  PIREG_STATUS psIRegStatus,
								  const EFO_TEMPLATE** ppsSelectedEntry)
/*****************************************************************************
 FUNCTION	: CanCombineIntoEfo
    
 PURPOSE	: Checks if two instructions can be combined into an IEFO instruction.

 PARAMETERS	: 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	/* Check that the sources can fit into one of the options for the efo instruction. */
	#if defined(SUPPORT_SGX545)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NEW_EFO_OPTIONS)
	{
		if (psInstA->eOpcode == IFMUL)
		{ 
			if(psInstB->eOpcode == IFMUL)
			{
				return CheckEfoTemplateTable(psState, 
										 psMulMulTemplate_545, 
										 sizeof(psMulMulTemplate_545) / sizeof(psMulMulTemplate_545[0]),
										 psInstA, 
										 psInstB, 
										 psIRegStatus,
										 IMG_FALSE,
										 ppsSelectedEntry);
			}
			if(psInstB->eOpcode == IFMAD)
			{
				return CheckEfoTemplateTable(psState, 
										 psMadMulTemplate_545, 
										 sizeof(psMadMulTemplate_545) / sizeof(psMadMulTemplate_545[0]),
										 psInstB, 
										 psInstA, 
										 psIRegStatus, 
										 IMG_TRUE,
										 ppsSelectedEntry);
			}
			if(psInstB->eOpcode == IFADD)
			{	
				return CheckEfoTemplateTable(psState, 
										 psAddMulTemplate_545, 
										 sizeof(psAddMulTemplate_545) / sizeof(psAddMulTemplate_545[0]),
										 psInstB, 
										 psInstA, 
										 psIRegStatus, 
										 IMG_TRUE,
										 ppsSelectedEntry);
			}
		}
		if (psInstA->eOpcode == IFADD)
		{
			if (psInstB->eOpcode == IFADD)
			{	
				return CheckEfoTemplateTable(psState, 
										 psAddAddTemplate_545, 
										 sizeof(psAddAddTemplate_545) / sizeof(psAddAddTemplate_545[0]),
										 psInstA, 
										 psInstB, 
										 psIRegStatus, 
										 IMG_FALSE,
										 ppsSelectedEntry);
			}
			if (psInstB->eOpcode == IFMUL)
			{
				return CheckEfoTemplateTable(psState, 
										 psAddMulTemplate_545, 
										 sizeof(psAddMulTemplate_545) / sizeof(psAddMulTemplate_545[0]),
										 psInstA, 
										 psInstB, 
										 psIRegStatus, 
										 IMG_FALSE,
										 ppsSelectedEntry);
			}
			if (psInstB->eOpcode == IFMAD)
			{
				return CheckEfoTemplateTable(psState, 
										 psMadAddTemplate_545, 
										 sizeof(psMadAddTemplate_545) / sizeof(psMadAddTemplate_545[0]),
										 psInstB, 
										 psInstA, 
										 psIRegStatus, 
										 IMG_TRUE,
										 ppsSelectedEntry);
			}
		}
		if (psInstA->eOpcode == IFMAD)
		{
			if (psInstB->eOpcode == IFADD)
			{
				return CheckEfoTemplateTable(psState, 
										 psMadAddTemplate_545, 
										 sizeof(psMadAddTemplate_545) / sizeof(psMadAddTemplate_545[0]),
										 psInstA, 
										 psInstB, 
										 psIRegStatus, 
										 IMG_FALSE,
										 ppsSelectedEntry);
			}
			if (psInstB->eOpcode == IFMUL)
			{
				return CheckEfoTemplateTable(psState, 
										 psMadMulTemplate_545, 
										 sizeof(psMadMulTemplate_545) / sizeof(psMadMulTemplate_545[0]),
										 psInstA, 
										 psInstB, 
										 psIRegStatus, 
										 IMG_FALSE,
										 ppsSelectedEntry);
			}
			if (psInstB->eOpcode == IFMAD)
			{
				return CheckEfoTemplateTable(psState, 
										 psMadMadTemplate_545, 
										 sizeof(psMadMadTemplate_545) / sizeof(psMadMadTemplate_545[0]),
										 psInstA, 
										 psInstB, 
										 psIRegStatus, 
										 IMG_FALSE,
										 ppsSelectedEntry);
			}
		}
		if (psInstA->eOpcode == IFMSA && psInstB->eOpcode == IFADD)
		{
			return CheckEfoTemplateTable(psState, 
										 psMsaAddTemplate_545, 
										 sizeof(psMsaAddTemplate_545) / sizeof(psMsaAddTemplate_545[0]),
										 psInstA, 
										 psInstB, 
										 psIRegStatus, 
										 IMG_FALSE,
										 ppsSelectedEntry);
		}
		if (psInstA->eOpcode == IFADD && psInstB->eOpcode == IFMSA)
		{
			return CheckEfoTemplateTable(psState, 
										 psMsaAddTemplate_545, 
										 sizeof(psMsaAddTemplate_545) / sizeof(psMsaAddTemplate_545[0]),
										 psInstB, 
										 psInstA, 
										 psIRegStatus, 
										 IMG_TRUE,
										 ppsSelectedEntry);
		}
	}
	else
	#endif /* defined(SUPPORT_SGX545) */
	{
		if (psInstA->eOpcode == IFMUL)
		{ 
			if(psInstB->eOpcode == IFMUL)
				return CheckEfoTemplateTable(psState, 
										 psMulMulTemplate_535, 
										 sizeof(psMulMulTemplate_535) / sizeof(psMulMulTemplate_535[0]),
										 psInstA, 
										 psInstB, 
										 psIRegStatus,
										 IMG_FALSE,
										 ppsSelectedEntry);
	
			if(psInstB->eOpcode == IFMAD)
				return CheckEfoTemplateTable(psState, 
										 psMadMulTemplate_535, 
										 sizeof(psMadMulTemplate_535) / sizeof(psMadMulTemplate_535[0]),
										 psInstB, 
										 psInstA, 
										 psIRegStatus, 
										 IMG_TRUE,
										 ppsSelectedEntry);
			if(psInstB->eOpcode == IFADD)
				return CheckEfoTemplateTable(psState, 
										 psAddMulTemplate_535, 
										 sizeof(psAddMulTemplate_535) / sizeof(psAddMulTemplate_535[0]),
										 psInstB, 
										 psInstA, 
										 psIRegStatus, 
										 IMG_TRUE,
										 ppsSelectedEntry);
		}
		if (psInstA->eOpcode == IFADD)
		{
			 if (psInstB->eOpcode == IFADD)
				return CheckEfoTemplateTable(psState, 
										 psAddAddTemplate_535, 
										 sizeof(psAddAddTemplate_535) / sizeof(psAddAddTemplate_535[0]),
										 psInstA, 
										 psInstB, 
										 psIRegStatus, 
										 IMG_FALSE,
										 ppsSelectedEntry);

			if (psInstB->eOpcode == IFMUL)
				return CheckEfoTemplateTable(psState, 
										 psAddMulTemplate_535, 
										 sizeof(psAddMulTemplate_535) / sizeof(psAddMulTemplate_535[0]),
										 psInstA, 
										 psInstB, 
										 psIRegStatus, 
										 IMG_FALSE,
										 ppsSelectedEntry);

			if (psInstB->eOpcode == IFMAD)
				return CheckEfoTemplateTable(psState, 
										 psMadAddTemplate_535, 
										 sizeof(psMadAddTemplate_535) / sizeof(psMadAddTemplate_535[0]),
										 psInstB, 
										 psInstA, 
										 psIRegStatus, 
										 IMG_TRUE,
										 ppsSelectedEntry);
		}
		if (psInstA->eOpcode == IFMAD)
		{
			if (psInstB->eOpcode == IFADD)
				return CheckEfoTemplateTable(psState, 
										 psMadAddTemplate_535, 
										 sizeof(psMadAddTemplate_535) / sizeof(psMadAddTemplate_535[0]),
										 psInstA, 
										 psInstB, 
										 psIRegStatus, 
										 IMG_FALSE,
										 ppsSelectedEntry);
	
			if (psInstB->eOpcode == IFMUL)
				return CheckEfoTemplateTable(psState, 
										 psMadMulTemplate_535, 
										 sizeof(psMadMulTemplate_535) / sizeof(psMadMulTemplate_535[0]),
										 psInstA, 
										 psInstB, 
										 psIRegStatus, 
										 IMG_FALSE,
										 ppsSelectedEntry);

			if (psInstB->eOpcode == IFMAD)
				return CheckEfoTemplateTable(psState, 
										 psMadMadTemplate_535, 
										 sizeof(psMadMadTemplate_535) / sizeof(psMadMadTemplate_535[0]),
										 psInstA, 
										 psInstB, 
										 psIRegStatus, 
										 IMG_FALSE,
										 ppsSelectedEntry);
		}
		if (psInstA->eOpcode == IFMSA && psInstB->eOpcode == IFADD)
		{
			return CheckEfoTemplateTable(psState, 
										 psMsaAddTemplate_535, 
										 sizeof(psMsaAddTemplate_535) / sizeof(psMsaAddTemplate_535[0]),
										 psInstA, 
										 psInstB, 
										 psIRegStatus, 
										 IMG_FALSE,
										 ppsSelectedEntry);
		}
		if (psInstA->eOpcode == IFADD && psInstB->eOpcode == IFMSA)
		{
			return CheckEfoTemplateTable(psState, 
										 psMsaAddTemplate_535, 
										 sizeof(psMsaAddTemplate_535) / sizeof(psMsaAddTemplate_535[0]),
										 psInstB, 
										 psInstA, 
										 psIRegStatus, 
										 IMG_TRUE,
										 ppsSelectedEntry);
		}
	}
	return IMG_FALSE;
}

IMG_INTERNAL 
IMG_VOID SetupEfoUsage(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: SetupEfoUsage
    
 PURPOSE	: Set up the information about which argument to EFO are used.

 PARAMETERS	: 
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PEFO_PARAMETERS psEfo = psInst->u.psEfo;
	IMG_BOOL abSrcUsed[EFO_UNIFIEDSTORE_SOURCE_COUNT];
	IMG_UINT32 uSrc;

	#ifdef DEBUG
	/* psState used to handle failed ASSERT on release build only */
	PVR_UNREFERENCED_PARAMETER(psState);
	#endif

	psEfo->bA0Used = IMG_FALSE;
	psEfo->bA1Used = IMG_FALSE;
	psEfo->bM0Used = IMG_FALSE;
	psEfo->bM1Used = IMG_FALSE;
	psEfo->bI0Used = IMG_FALSE;
	psEfo->bI1Used = IMG_FALSE;
	memset(abSrcUsed, 0, sizeof(abSrcUsed));

	if (psEfo->bWriteI0)
	{
		switch (psEfo->eI0Src)
		{
			case A0: psEfo->bA0Used = IMG_TRUE; break;
			case A1: psEfo->bA1Used = IMG_TRUE; break;
			case M0: psEfo->bM0Used = IMG_TRUE; break;
			default: imgabort();
		}
	}
	if (psEfo->bWriteI1)
	{
		switch (psEfo->eI1Src)
		{
			case A0: psEfo->bA0Used = IMG_TRUE; break;
			case A1: psEfo->bA1Used = IMG_TRUE; break;
			case M1: psEfo->bM1Used = IMG_TRUE; break;
			default: imgabort();
		}
	}
	if (!psInst->u.psEfo->bIgnoreDest)
	{
		switch (psEfo->eDSrc)
		{
			case I0: psEfo->bI0Used = IMG_TRUE; break;
			case I1: psEfo->bI1Used = IMG_TRUE; break;
			case A0: psEfo->bA0Used = IMG_TRUE; break;
			case A1: psEfo->bA1Used = IMG_TRUE; break;
			default: imgabort();
		}
	}
	if (psEfo->bA0Used)
	{
		switch (psEfo->eA0Src0)
		{
			case M0: psEfo->bM0Used = IMG_TRUE; break;
			case SRC0: abSrcUsed[0] = IMG_TRUE; break;
			default: imgabort();
		}
		switch (psEfo->eA0Src1)
		{
			case M1: psEfo->bM1Used = IMG_TRUE; break;
			case SRC2: abSrcUsed[2] = IMG_TRUE; break;
			case I0: psEfo->bI0Used = IMG_TRUE; break;
			case SRC1: abSrcUsed[1] = IMG_TRUE; break;
			/* New EFO */
			case SRC0: abSrcUsed[0] = IMG_TRUE; break;
			default: imgabort();
		}
	}
	if (psEfo->bA1Used)
	{
		switch (psEfo->eA1Src0)
		{
			case I1: psEfo->bI1Used = IMG_TRUE; break;
			case SRC2: abSrcUsed[2] = IMG_TRUE; break;
			/* New EFO */
			case SRC1: abSrcUsed[1] = IMG_TRUE; break;
			default: imgabort();
		}
		switch (psEfo->eA1Src1)
		{
			case I0: psEfo->bI0Used = IMG_TRUE; break;
			case M1: psEfo->bM1Used = IMG_TRUE; break;
			case SRC0: abSrcUsed[0] = IMG_TRUE; break;
			/* New EFO */
			case SRC2: abSrcUsed[2] = IMG_TRUE; break;
			default: imgabort();
		}
	}
	if (psEfo->bM0Used)
	{
		switch (psEfo->eM0Src0)
		{
			case SRC0: abSrcUsed[0] = IMG_TRUE; break;
			case SRC1: abSrcUsed[1] = IMG_TRUE; break;
			/* New EFO */
			case SRC2: abSrcUsed[2] = IMG_TRUE; break;
			default: imgabort();
		}
		switch (psEfo->eM0Src1)
		{
			case SRC0: abSrcUsed[0] = IMG_TRUE; break;
			case SRC1: abSrcUsed[1] = IMG_TRUE; break;
			case SRC2: abSrcUsed[2] = IMG_TRUE; break;
			case I0: psEfo->bI0Used = IMG_TRUE; break;
			default: imgabort();
		}
	}
	if (psEfo->bM1Used)
	{
		ASSERT(psEfo->eM1Src0 == SRC0 || 
			   (psEfo->eM1Src0 == SRC1 && psEfo->eM1Src1 == SRC2) ||
			   (psEfo->eM1Src0 == SRC2 && psEfo->eM1Src1 == SRC2) ||
			   (psEfo->eM1Src0 == SRC2 && psEfo->eM1Src1 == I1));

		switch (psEfo->eM1Src0)
		{
			case SRC0: abSrcUsed[0] = IMG_TRUE; break;
			/* New EFO */
			case SRC1: abSrcUsed[1] = IMG_TRUE; break;
			case SRC2: abSrcUsed[2] = IMG_TRUE; break;
			default: imgabort();
		}

		switch (psEfo->eM1Src1)
		{
			case SRC2: abSrcUsed[2] = IMG_TRUE; break;
			case SRC0: abSrcUsed[0] = IMG_TRUE; break;
			case I1: psEfo->bI1Used = IMG_TRUE; break;
			/* New EFO */
			case SRC1: abSrcUsed[1] = IMG_TRUE; break;
			default: imgabort();
		}
	}

	/*
		Clear any unreferenced sources.
	*/
	for (uSrc = 0; uSrc < EFO_UNIFIEDSTORE_SOURCE_COUNT; uSrc++)
	{
		if (!abSrcUsed[uSrc])
		{
			SetSrcUnused(psState, psInst, uSrc);

			psEfo->sFloat.asSrcMod[uSrc].bNegate = IMG_FALSE;
			psEfo->sFloat.asSrcMod[uSrc].bAbsolute = IMG_FALSE;
			psEfo->sFloat.asSrcMod[uSrc].uComponent = 0;
		}
	}
	/*
		Set up additional sources representing implicitly
		read internal registers.
	*/
	InitInstArg(&psInst->asArg[EFO_I0_SRC]);
	if (psEfo->bI0Used)
	{
		SetSrc(psState, psInst, EFO_I0_SRC, USEASM_REGTYPE_FPINTERNAL, 0 /* uNumber */, UF_REGFORMAT_F32);
	}
	else
	{
		SetSrcUnused(psState, psInst, EFO_I0_SRC);
	}
	InitInstArg(&psInst->asArg[EFO_I1_SRC]);
	if (psEfo->bI1Used)
	{
		SetSrc(psState, psInst, EFO_I1_SRC, USEASM_REGTYPE_FPINTERNAL, 1 /* uNumber */, UF_REGFORMAT_F32);
	}
	else
	{
		SetSrcUnused(psState, psInst, EFO_I1_SRC);
	}
}

static IMG_VOID CombineIntoEfo(PINTERMEDIATE_STATE psState, 
							   const EFO_TEMPLATE* psSelectedEntry,
							   PINST psInstA, 
							   PINST psInstB, 
						       PIREG_STATUS psIRegStatus,  
						       PINST psEfoInst)
/*****************************************************************************
 FUNCTION	: CombineIntoEfo
    
 PURPOSE	: Combine two instructions into an EFO instruction.

 PARAMETERS	: 
			  
 RETURNS	: A pointer to the new instruction which is one psInstA or psInstB.
*****************************************************************************/
{
	if ((psInstA->eOpcode == IFADD && psInstB->eOpcode == IFMAD) ||
		(psInstA->eOpcode == IFMUL && psInstB->eOpcode == IFMAD) ||
		(psInstA->eOpcode == IFMUL && psInstB->eOpcode == IFADD) ||
		(psInstA->eOpcode == IFADD && psInstB->eOpcode == IFMSA))
	{
		CheckEfoTemplate(psState, psSelectedEntry, psInstB, psInstA, psIRegStatus, IMG_TRUE, psEfoInst);
	}
	else
	{
		CheckEfoTemplate(psState, psSelectedEntry, psInstA, psInstB, psIRegStatus, IMG_FALSE, psEfoInst);
	}
}

static IMG_BOOL IsInstDependentOnGroup(PINTERMEDIATE_STATE	psState,
									   PEFOGEN_STATE		psEfoState,
									   IMG_UINT32			uInst,
									   IMG_UINT32			uGroupId)
/*****************************************************************************
 FUNCTION	: IsInstDependentOnGroup
   
 PURPOSE	: Check if a specifed instruction is dependent on any instruction in 
			  an EFO group.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  uInst				- Index of the instruction to check.
			  uEfoGroupId		- Index of the EFO group to check.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PEFO_GROUP_DATA	psEfoGroup = &psEfoState->asEfoGroup[uGroupId];
	PDGRAPH_STATE	psDepState = psEfoState->psCodeBlock->psDepState;
	ASSERT(psEfoState->psCodeBlock->psDepState != NULL);

	/*
		Check for an empty group.
	*/
	if (psEfoGroup->uInstCount == 0)
	{
		return IMG_FALSE;
	}

	if (psEfoGroup->bExistingGroup)
	{
		PINST	psSrcInst;

		/*
			For each instruction in the group check if it is a dependency of
			the supplied instruction.
		*/
		for (psSrcInst = psEfoGroup->psHead; 
			 psSrcInst != NULL; 
			 psSrcInst = psSrcInst->sStageData.psEfoData->psNextWriter)
		{
			if (GraphGet(psState, psDepState->psClosedDepGraph, uInst, psSrcInst->uId))
			{
				return IMG_TRUE;
			}
		}
	}
	else
	{
		IMG_UINT32	uGroupHead = psEfoGroup->psHead->uId;

		/*
			For a group created by us every instruction in the group will be dependent
			on the first instruction so (since the graph is closed under transitivity) checking
			if an instruction is dependent on any instruction in the group only requires checking
			the first instruction. e.g.

				H => G so G => I is equivalent to H => I
		*/
		if (GraphGet(psState, psDepState->psClosedDepGraph, uInst, uGroupHead))
		{
			return IMG_TRUE;
		}
	}

	return IMG_FALSE;
}

static IMG_BOOL IsGroupDependentOnInst(PINTERMEDIATE_STATE	psState, 
									   PEFOGEN_STATE		psEfoState,
									   IMG_UINT32			uInst, 
									   IMG_UINT32			uGroupId)
/*****************************************************************************
 FUNCTION	: IsGroupDependentOnInst
   
 PURPOSE	: Check if any instruction in an EFO group is dependent on a specified
			  instruction.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  uInst				- Index of the instruction to check.
			  uEfoGroupId		- Index of the group to check.
			
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PEFO_GROUP_DATA	psEfoGroup = &psEfoState->asEfoGroup[uGroupId];
	PDGRAPH_STATE	psDepState = psEfoState->psCodeBlock->psDepState;
	ASSERT(psEfoState->psCodeBlock->psDepState != NULL);

	/*
		Check for an empty group.
	*/
	if (psEfoGroup->uInstCount == 0)
	{
		return IMG_FALSE;
	}

	if (psEfoGroup->bExistingGroup)
	{
		PINST	psSrcInst;

		/*
			Check for each instruction in the group if it is dependent on the supplied
			instruction.
		*/
		for (psSrcInst = psEfoGroup->psHead; 
			 psSrcInst != NULL; 
			 psSrcInst = psSrcInst->sStageData.psEfoData->psNextWriter)
		{
			if (GraphGet(psState, psDepState->psClosedDepGraph, psSrcInst->uId, uInst))
			{
				return IMG_TRUE;
			}
		}
	}
	else
	{
		if (psEfoGroup->psTail != NULL)
		{
			IMG_UINT32	uGroupTail = psEfoGroup->psTail->uId;
			PINST		psTailReader;

			/*
				For a group created by us every instruction in the group will be a dependency of the 
				last instruction so (since the graph is closed under transitivity) checking for
				a dependency of the group only requires checking the last instruction. e.g.

					G => T so I => G is equivalent to I => T
			*/
			if (GraphGet(psState, psDepState->psClosedDepGraph, uGroupTail, uInst))
			{
				return IMG_TRUE;
			}	
			for (psTailReader = psEfoGroup->psTail->sStageData.psEfoData->psFirstReader;
				 psTailReader != NULL;
				 psTailReader = psTailReader->sStageData.psEfoData->psNextReader)
			{
				if (GraphGet(psState, psDepState->psClosedDepGraph, psTailReader->uId, uInst))
				{
					return IMG_TRUE;
				}
			}
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL IsDescheduleBetweenInstAndInsts(PINTERMEDIATE_STATE		psState,
												PEFOGEN_STATE			psEfoState,
												IMG_UINT32				uInst,
												IMG_UINT32				uDest1,
												IMG_UINT32				uDest2)
/*****************************************************************************
 FUNCTION	: IsDescheduleBetweenInstAndInsts
    
 PURPOSE	: Check if there is an instruction causing a deschedule which is
			  dependent on one instruction and a dependency of a pair of instructions.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
		      uInst				- The single instruction to check.
			  uDest1			- The pair of instructions to check.
			  uDest2
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PINST			psDeschedInst;
	PDGRAPH_STATE	psDepState = psEfoState->psCodeBlock->psDepState;
	ASSERT(psEfoState->psCodeBlock->psDepState != NULL);

	for (psDeschedInst = psEfoState->psDeschedInstListHead;
		 psDeschedInst != NULL;
		 psDeschedInst = psDeschedInst->sStageData.psEfoData->psNextDeschedInst)
	{
		IMG_UINT32	uDeschedInst = psDeschedInst->uId;

		/*
			Check if the descheduling instruction is dependent on the first instruction.
		*/
		if (GraphGet(psState, psDepState->psClosedDepGraph, uDeschedInst, uInst))
		{
			/*
				Check if either of the pair of instructions is dependent on the 
				descheduling instruction.
			*/
			if (GraphGet(psState, psDepState->psClosedDepGraph, uDest1, uDeschedInst) ||
				GraphGet(psState, psDepState->psClosedDepGraph, uDest2, uDeschedInst))
			{
				return IMG_TRUE;
			}
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL IsDescheduleBetweenInstAndGroup(PINTERMEDIATE_STATE		psState,
												PEFOGEN_STATE			psEfoState,
												IMG_UINT32				uInst,
												IMG_UINT32				uEfoGroupId)
/*****************************************************************************
 FUNCTION	: IsDescheduleBetweenInstAndInsts
    
 PURPOSE	: Check if there is an instruction causing a deschedule which is
			  dependent on one instruction and a dependency of an EFO group

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
		      uInst				- The single instruction to check.
			  uEfoGroup			- The group to check.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PINST			psDeschedInst;
	PDGRAPH_STATE	psDepState = psEfoState->psCodeBlock->psDepState;
	ASSERT(psEfoState->psCodeBlock->psDepState != NULL);

	for (psDeschedInst = psEfoState->psDeschedInstListHead;
		 psDeschedInst != NULL;
		 psDeschedInst = psDeschedInst->sStageData.psEfoData->psNextDeschedInst)
	{
		IMG_UINT32	uDeschedInst = psDeschedInst->uId;

		/*
			Check if the descheduling instruction is dependent on the first instruction.
		*/
		if (GraphGet(psState, psDepState->psClosedDepGraph, uDeschedInst, uInst))
		{
			/*
				Check if the group is dependent on the descheduling instruction.
			*/
			if (IsGroupDependentOnInst(psState, psEfoState, uDeschedInst, uEfoGroupId))
			{
				return IMG_TRUE;
			}
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL IsDescheduleBetweenGroupAndInsts(PINTERMEDIATE_STATE	psState, 
												 PEFOGEN_STATE			psEfoState,
												 IMG_UINT32				uEfoGroupId,
												 IMG_UINT32				uOtherInst,
												 IMG_UINT32				uDest1, 
												 IMG_UINT32				uDest2)
/*****************************************************************************
 FUNCTION	: IsDescheduleBetweenGroupAndInst
    
 PURPOSE	: Check if there is an instruction causing a deschedule which is
			  dependent on an EFO group and a dependency of either a single
			  instruction or a pair of instructions.

 PARAMETERS	: psState			- Compiler state.
		      psEfoState		- Module state.
			  uEfoGroupId		- Index of the EFO group to check.
			  uOtherInst		- If not -1 the index of an instruction to
								treat as through it were part of the EFO group.
			  uDest1, uDest2	- Index of the instructions to check.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	
	PINST			psDeschedInst;
	PDGRAPH_STATE	psDepState = psEfoState->psCodeBlock->psDepState;
	ASSERT(psEfoState->psCodeBlock->psDepState != NULL);

	for (psDeschedInst = psEfoState->psDeschedInstListHead;
		 psDeschedInst != NULL;
		 psDeschedInst = psDeschedInst->sStageData.psEfoData->psNextDeschedInst)
	{
		IMG_UINT32	uDeschedInst = psDeschedInst->uId;

		/*
			Check for an indirect dependency through a descheduling point..
		*/
		if (
				IsInstDependentOnGroup(psState, psEfoState, uDeschedInst, uEfoGroupId) ||
				(
					uOtherInst != USC_UNDEF &&
					GraphGet(psState, psDepState->psClosedDepGraph, uDeschedInst, uOtherInst)
				)
		   )
		{
			if (
					GraphGet(psState, psDepState->psClosedDepGraph, uDest1, uDeschedInst) ||
					(
						uDest2 != USC_UNDEF && 
						GraphGet(psState, psDepState->psClosedDepGraph, uDest2, uDeschedInst)
					)
			   )
			{
				return IMG_TRUE;
			}
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL IsGroupDependentOnGroup(PINTERMEDIATE_STATE	psState, 
										PEFOGEN_STATE		psEfoState,
										IMG_UINT32			uEfoGroup1, 
										IMG_UINT32			uEfoGroup2)
/*****************************************************************************
 FUNCTION	: IsGroupDependentOnGroup
    
 PURPOSE	: Check if any instruction in the second group is a dependency of 
			  any instruction in the first group.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  uEfoGroup1		- Groups to check.
			  uEfoGroup2
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PEFO_GROUP_DATA	psEfoGroup1 = &psEfoState->asEfoGroup[uEfoGroup1];
	PEFO_GROUP_DATA psEfoGroup2 = &psEfoState->asEfoGroup[uEfoGroup2];
	PDGRAPH_STATE	psDepState = psEfoState->psCodeBlock->psDepState;
	ASSERT(psEfoState->psCodeBlock->psDepState != NULL);

	if (psEfoGroup1->bExistingGroup || psEfoGroup2->bExistingGroup)
	{
		PINST	psSrcInst;

		/*
			Check for each instructions in the group.
		*/
		for (psSrcInst = psEfoGroup1->psHead; 
			 psSrcInst != NULL; 
			 psSrcInst = psSrcInst->sStageData.psEfoData->psNextWriter)
		{
			if (IsGroupDependentOnInst(psState, psEfoState, psSrcInst->uId, uEfoGroup2))
			{
				return IMG_TRUE;
			}
		}
		return IMG_FALSE;
	}
	else
	{
		/*
			Check for an empty group.
		*/
		if (psEfoGroup1->psHead == NULL || psEfoGroup2->psHead == NULL)
		{
			return IMG_FALSE;
		}
		else
		{
			PINST	psEfoGroup1Head = psEfoGroup1->psHead;
			PINST	psEfoGroup2Tail = psEfoGroup2->psTail;
			PINST	psTailReader;

			/*
				Every instruction in the group is dependent on the first instruction and a dependency of
				the last instruction so

					H => F
					G => T
					F => G is equivalent to H => T
			*/
			if (GraphGet(psState, psDepState->psClosedDepGraph, psEfoGroup2Tail->uId, psEfoGroup1Head->uId))
			{
				return IMG_TRUE;
			}
			for (psTailReader = psEfoGroup2Tail->sStageData.psEfoData->psFirstReader;
				 psTailReader != NULL;
				 psTailReader = psTailReader->sStageData.psEfoData->psNextReader)
			{
				if (GraphGet(psState, psDepState->psClosedDepGraph, psTailReader->uId, psEfoGroup1Head->uId))
				{
					return IMG_TRUE;
				}
			}
			return IMG_FALSE;
		}
	}
}

static IMG_BOOL CheckEfoGroupOrder(PINTERMEDIATE_STATE psState, 
								   PEFOGEN_STATE psEfoState, IMG_UINT32 uEfoGroup, 
								   IMG_UINT32 uEfoInst1, IMG_UINT32 uEfoInst2)
/*****************************************************************************
 FUNCTION	: CheckEfoGroupOrder
    
 PURPOSE	: Checks if a dependency through an internal register can be inserted
			  between two instructions.

 PARAMETERS	: psState			- Compiler state.
			  uEfoGroupId		- EFO group id of the dependency source.
			  uDest1, uDest2	- Two instructions that will be combined into the
								  the destination for the dependency.
			  
 RETURNS	: TRUE if the dependency can be inserted.
*****************************************************************************/
{
	IMG_UINT32	uOtherEfoGroup;

	/*
		Check for another group:
		1) With a dependency on the first group
		2) With either of the instructions to be combined having a dependency on the other group.
	*/
	for (uOtherEfoGroup = 0; uOtherEfoGroup < psEfoState->uEfoGroupCount; uOtherEfoGroup++)
	{
		if (uOtherEfoGroup != uEfoGroup && IsGroupDependentOnGroup(psState, psEfoState, uEfoGroup, uOtherEfoGroup))
		{
			if (
					IsInstDependentOnGroup(psState, psEfoState, uEfoInst1, uOtherEfoGroup) ||
					(	
						uEfoInst2 != USC_UNDEF &&
						IsInstDependentOnGroup(psState, psEfoState, uEfoInst2, uOtherEfoGroup)
					)
			   )
			{
				return IMG_FALSE;
			}
		}
	}
	return IMG_TRUE;
}

static IMG_BOOL CheckEfoGroupDependency(PINTERMEDIATE_STATE	psState,
										PEFOGEN_STATE		psEfoState,
										IMG_UINT32			uEfoInst1, 
										IMG_UINT32			uEfoInst2,
										IMG_UINT32			uEfoGroupId)
/*****************************************************************************
 FUNCTION	: CheckEfoGroupDependency
   
 PURPOSE	: Check if combining two instructions would result in a dependency
			  between two EFO groups which already have a dependency in the
			  other direction.

 PARAMETERS	: psState			- Compiler state.
			  pEfoState			- Module state.
			  uEfoInst1			- The two instructions we are trying to combine
			  uEfoInst2			into an EFO.
			  uEfoGroupId		- Either USC_UNDEF or the EFO group to which the
								two instructions will be added.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uSrcEfoGroup, uDestEfoGroup;
	IMG_BOOL		bAnyDependent;

	/*
		Precompute which groups are dependent on uEfoInst1 or uEfoInst2.
	*/
	bAnyDependent = IMG_FALSE;
	for (uDestEfoGroup = 0; uDestEfoGroup < psEfoState->uEfoGroupCount; uDestEfoGroup++)
	{
		psEfoState->asEfoGroup[uDestEfoGroup].bDependent = IMG_FALSE;
		if (
				IsGroupDependentOnInst(psState, psEfoState, uEfoInst1, uDestEfoGroup) ||
				IsGroupDependentOnInst(psState, psEfoState, uEfoInst2, uDestEfoGroup) ||
				(
					uEfoGroupId != USC_UNDEF &&
					uEfoGroupId != uDestEfoGroup &&
					IsGroupDependentOnGroup(psState, psEfoState, uEfoGroupId, uDestEfoGroup)
				)
		   )
		{
			psEfoState->asEfoGroup[uDestEfoGroup].bDependent = IMG_TRUE;
			bAnyDependent = IMG_TRUE;
		}
	}
	if (!bAnyDependent)
	{
		return IMG_TRUE;
	}

	/*
		Looking for:

			SRCGROUP => INST1+INST2 => DESTGROUP => SRCGROUP
	*/
	for (uSrcEfoGroup = 0; uSrcEfoGroup < psEfoState->uEfoGroupCount; uSrcEfoGroup++)
	{
		if (
				IsInstDependentOnGroup(psState, psEfoState, uEfoInst1, uSrcEfoGroup) ||
				IsInstDependentOnGroup(psState, psEfoState, uEfoInst2, uSrcEfoGroup) ||
				(
					uEfoGroupId != USC_UNDEF &&
					uEfoGroupId != uDestEfoGroup &&
					IsGroupDependentOnGroup(psState, psEfoState, uSrcEfoGroup, uEfoGroupId)
				)
			)
		{
			for (uDestEfoGroup = 0; uDestEfoGroup < psEfoState->uEfoGroupCount; uDestEfoGroup++)
			{
				if (uDestEfoGroup != uSrcEfoGroup)
				{
					if (psEfoState->asEfoGroup[uDestEfoGroup].bDependent)
					{
						if (IsGroupDependentOnGroup(psState, psEfoState, uDestEfoGroup, uSrcEfoGroup))
						{
							return IMG_FALSE;
						}
					}
				}
			}
		}
	}
	return IMG_TRUE;
}

static IMG_BOOL WouldBeInterveningIRegWrite(PINTERMEDIATE_STATE psState,
											PEFOGEN_STATE		psEfoState,
											IMG_UINT32			uEfoInst1, 
											IMG_UINT32			uEfoInst2)
/*****************************************************************************
 FUNCTION	: WouldBeInterveningIRegWrite
    
 PURPOSE	: Checks if combing two instructions into an EFO would overwrite another
			  use of the internal register.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  uEfoInst1,		- The two instructions to be combined into
			  uEfoInst2			  an EFO.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32		uEfoGroupIdx;

	/*
		Check for a pair of EFO instructions in the same group (therefore the
		internal registers are live between them) with either or both of the
		pairs of instructions we are trying to combine dependent on the
		first EFO and a dependency of the second EFO.
	*/
	for (uEfoGroupIdx = 0; uEfoGroupIdx < psEfoState->uEfoGroupCount; uEfoGroupIdx++)
	{
		if (
				(
					IsInstDependentOnGroup(psState, psEfoState, uEfoInst1, uEfoGroupIdx) ||
					IsInstDependentOnGroup(psState, psEfoState, uEfoInst2, uEfoGroupIdx)
				) &&
				(
					IsGroupDependentOnInst(psState, psEfoState, uEfoInst1, uEfoGroupIdx) ||
					IsGroupDependentOnInst(psState, psEfoState, uEfoInst2, uEfoGroupIdx)
				)
			)
		{
			return IMG_TRUE;
		}
	}

	return IMG_FALSE;
}

static IMG_BOOL IsGroupDependency(PEFOGEN_STATE psEfoState,
								  IMG_UINT32 uEfoSrcGroup, 
								  IMG_UINT32 uEfoDestGroup)
/*****************************************************************************
 FUNCTION	: IsGroupDependency
    
 PURPOSE	: Checks for a dependency between two efo groups.

 PARAMETERS	: psEfoState		- EFO generation state
			  uEfoSrcGroup		- Source for the dependency.
			  uEfoDestGroup		- Destination for the dependency.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (GetClosedDependency(psEfoState, uEfoDestGroup, uEfoSrcGroup))
	{
		return IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
}

static IMG_BOOL IsInterveningIRegWriteForRead(PINTERMEDIATE_STATE psState, 
											  PEFOGEN_STATE psEfoState, 
											  PINST psSrcEfoInst,
											  IMG_UINT32 uOtherInst,
											  IMG_UINT32 uDest)
/*****************************************************************************
 FUNCTION	: IsInterveningIRegWriteForRead
    
 PURPOSE	: Checks if there is a write to an internal register on a dependency path
			  between an internal register write and an internal register read.

 PARAMETERS	: psState			- Compiler state.
			  uSrcEfoGroupId	- Group that contains the internal register write.
			  uOtherInst		- If not -1 an instruction to consider as part of the
								group.
			  uDest				- Internal register read.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32		uEfoGroup;
	IMG_UINT32		uSrcEfoGroupId = psSrcEfoInst->sStageData.psEfoData->uEfoGroupId;
	PINST			psEfoInst;
	PDGRAPH_STATE	psDepState = psEfoState->psCodeBlock->psDepState;
	ASSERT(psEfoState->psCodeBlock->psDepState != NULL);

	/*
		Check for another EFO group with a dependency onto the instruction we are adding to
		the group and a dependency from the group.
	*/
	for (uEfoGroup = 0; uEfoGroup < psEfoState->uEfoGroupCount; uEfoGroup++)
	{
		if (uEfoGroup != uSrcEfoGroupId && 
			IsInstDependentOnGroup(psState, psEfoState, uDest, uEfoGroup))
		{
			if (GetClosedDependency(psEfoState, uEfoGroup, uSrcEfoGroupId))
			{
				return IMG_TRUE;
			}
			if (uOtherInst != USC_UNDEF &&
				IsGroupDependentOnInst(psState, psEfoState, uOtherInst, uEfoGroup))
			{
				return IMG_TRUE;
			}
		}
	}

	/*
		Check for a later EFO within the group with a dependency onto the
		instruction we are adding.
	*/
	for (psEfoInst = psSrcEfoInst->sStageData.psEfoData->psNextWriter;
		 psEfoInst != NULL; 
		 psEfoInst = psEfoInst->sStageData.psEfoData->psNextWriter)
	{
		if (GraphGet(psState, psDepState->psClosedDepGraph, uDest, psEfoInst->uId))
		{
			return IMG_TRUE;
		}
	}

	return IMG_FALSE;
}


static IMG_BOOL IsDescheduleBetweenGroups(PINTERMEDIATE_STATE	psState,
										  PEFOGEN_STATE			psEfoState,
										  IMG_UINT32			uEfoGroupId1, 
										  IMG_UINT32			uEfoGroupId2,
										  IMG_UINT32			uOtherInst)
/*****************************************************************************
 FUNCTION	: IsDescheduleBetweenGroups
    
 PURPOSE	: Check if there is an instruction causing a deschedule which is
			  dependent on one EFO group and a dependency of a second.

 PARAMETERS	: psState			- Compiler state.
		      psEfoState		- Module state.
			  uEfoGroupId1		- Indexes of the EFO group to check.
			  uEfoGroupId2
			  uOtherInst		- If not -1 then an instruction to consider at
								through it was part of the first group.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PINST			psDeschedInst;
	PDGRAPH_STATE	psDepState = psEfoState->psCodeBlock->psDepState;
	ASSERT(psEfoState->psCodeBlock->psDepState != NULL);

	for (psDeschedInst = psEfoState->psDeschedInstListHead;
		 psDeschedInst != NULL;
		 psDeschedInst = psDeschedInst->sStageData.psEfoData->psNextDeschedInst)
	{
		IMG_UINT32	uDeschedInst = psDeschedInst->uId;

		/*
			Check for a descheduling point which is dependent on the first group and a
			dependency of the second group.
		*/
		if (IsGroupDependentOnInst(psState, psEfoState, uDeschedInst, uEfoGroupId2))
		{
			if (IsInstDependentOnGroup(psState, psEfoState, uDeschedInst, uEfoGroupId1))
			{
				return IMG_TRUE;
			}
			if (uOtherInst != USC_UNDEF)
			{
				if (GraphGet(psState, psDepState->psClosedDepGraph, uDeschedInst, uOtherInst))
				{
					return IMG_TRUE;
				}
			}
		}
	}
	return IMG_FALSE;
}

static IMG_UINT32 CountArgs(PINST psInst, IMG_UINT32 uSrcToIReg, 
							IMG_UINT32 uPrevArgCount, PARG* ppsPrevArgs)
/*****************************************************************************
 FUNCTION	: CountArgs
    
 PURPOSE	: Counts the number of unique arguments to an instruction.

 PARAMETERS	: psInst		- The instruction to count arguments on.
			  uSrcInIReg	- Bitmask of the argument that can be replaced by
							internal register.
			  uPrevArgCount	- The count of unique arguments to previous instructions.
			  ppsPrevArgs	- Array of pointers to the unique arguments to previous
							instructions.
								  
 RETURNS	: The new count of unique arguments.
*****************************************************************************/
{
	IMG_UINT32	uArg, uPrevArg;

	for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
	{
		if (uSrcToIReg & (1U << uArg))
		{
			continue;
		}
		for (uPrevArg = 0; uPrevArg < uPrevArgCount; uPrevArg++)
		{
			if (EqualArgs(&psInst->asArg[uArg], ppsPrevArgs[uPrevArg]))
			{
				break;
			}
		}
		if (uPrevArg == uPrevArgCount)
		{
			ppsPrevArgs[uPrevArg] = &psInst->asArg[uArg];
			uPrevArgCount++;
		}
	}
	return uPrevArgCount;
}

static IMG_BOOL CheckEfoGroupDependencyForMad(PINTERMEDIATE_STATE		psState,
											  PEFOGEN_STATE				psEfoState,
											  IMG_UINT32				uInstA,
											  IMG_UINT32				uInstB,
										      IMG_UINT32				uMadInst)
/*****************************************************************************
 FUNCTION	: CheckEfoGroupDependencyForMad
    
 PURPOSE	: Check if using a particular MAD instruction to do part of an
			  EFO calculation would stop us from ordering EFO groups
			  correctly.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  uInstA			- The two instructions to be combined into
			  uInstB			  an EFO.
			  uMadInst			- The instruction which will do the I0+I1 part
								of the calculation.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uSrcEfoGroupIdx;
	IMG_UINT32	uDestEfoGroupIdx;

	for (uSrcEfoGroupIdx = 0; uSrcEfoGroupIdx < psEfoState->uEfoGroupCount; uSrcEfoGroupIdx++)
	{
		/*
			Check for a group SRC which has a dependency on either of the instruction we are going
			to combine into an EFO.
		*/
		if (IsInstDependentOnGroup(psState, psEfoState, uInstA, uSrcEfoGroupIdx) ||
			IsInstDependentOnGroup(psState, psEfoState, uInstB, uSrcEfoGroupIdx))
		{
			for (uDestEfoGroupIdx = 0; uDestEfoGroupIdx < psEfoState->uEfoGroupCount; uDestEfoGroupIdx++)
			{
				if (uDestEfoGroupIdx != uSrcEfoGroupIdx)
				{
					/*
						Check for another group DEST which is dependent on the MAD instruction.
					*/
					if (IsGroupDependentOnInst(psState, psEfoState, uMadInst, uDestEfoGroupIdx))
					{
						/*
							Check for the SRC group dependent on the DEST group. If the MAD instruction
							is made dependent on the EFO then there will be a circular dependency.
						*/
						if (IsGroupDependentOnGroup(psState, psEfoState, uDestEfoGroupIdx, uSrcEfoGroupIdx))
						{
							return IMG_FALSE;
						}
					}
				}
			}
		}
	}
	return IMG_TRUE;
}

static PINST FindMadForEfoWriteBack(PINTERMEDIATE_STATE	psState,
										 PEFOGEN_STATE	psEfoState,
										 PCODEBLOCK		psBlock,
										 IMG_UINT32		uEfoInst,
										 IMG_UINT32		uOtherInst,
										 IMG_UINT32		uExistingGroupId)
/*****************************************************************************
 FUNCTION	: FindMadForEfoWriteBack
    
 PURPOSE	: Find a MAD instruction which can be replaced by an EFO which also
			  does I1=I0+I1 using the internal registers written by another EFO.

 PARAMETERS	: psState			- Compiler state.
			  uEfoInst			- The two instructions that might form the
			  uOtherInst		other EFO. The first one might already be an EFO.
			  uExistingGroupId	- The existing EFO group the new instructions will
								be added to.
			  
 RETURNS	: A pointer to the found MAD instruction or NULL if no suitable MAD
			  could be found.
*****************************************************************************/
{
	PINST			psMadInst;
	PDGRAPH_STATE	psDepState = psEfoState->psCodeBlock->psDepState;

	for (psMadInst = psBlock->psBody; psMadInst != NULL; psMadInst = psMadInst->psNext)
	{
		IMG_UINT32	uMadInst = psMadInst->uId;

		
		/*
			Look for an instruction of the right type, without a dependency on
			the EFO.
		*/
		if (uMadInst != uEfoInst &&
			psMadInst->eOpcode == IFMAD &&
			NoPredicate(psState, psMadInst) &&
			psMadInst->asDest[0].uIndexType == USC_REGTYPE_NOINDEX &&
			psMadInst->sStageData.psEfoData->uEfoGroupId == USC_UNDEF &&
			!GraphGet(psState, psDepState->psClosedDepGraph, uMadInst, uEfoInst) &&
			!GraphGet(psState, psDepState->psClosedDepGraph, uEfoInst, uMadInst) &&
			!GraphGet(psState, psDepState->psClosedDepGraph, uMadInst, uOtherInst) &&
			!GraphGet(psState, psDepState->psClosedDepGraph, uOtherInst, uMadInst))
		{
			IMG_UINT32	uMulSrc0, uMulSrc1, uAddSrc;

			/*
				Check bank restrictions on the instruction.
			*/
			if (psEfoState->bNewEfoFeature)
			{
				uMulSrc0 = 1;
				uMulSrc1 = 2;
				uAddSrc = 0;
			}
			else
			{
				uMulSrc0 = 0;
				uMulSrc1 = 1;
				uAddSrc = 2;
			}
			if (!(
					CanUseEfoSrc(psState, psEfoState->psCodeBlock, uMulSrc0, psMadInst, 0) &&
					CanUseEfoSrc(psState, psEfoState->psCodeBlock, uMulSrc1, psMadInst, 1) &&
					CanUseEfoSrc(psState, psEfoState->psCodeBlock, uAddSrc, psMadInst, 2)
				 ) &&
				!(
					CanUseEfoSrc(psState, psEfoState->psCodeBlock, uMulSrc1, psMadInst, 0) &&
					CanUseEfoSrc(psState, psEfoState->psCodeBlock, uMulSrc0, psMadInst, 1) &&
					CanUseEfoSrc(psState, psEfoState->psCodeBlock, uAddSrc, psMadInst, 2)
				 )
			   )
			{
				continue;
			}

			/*
				Check that the path EFO->MAD where the internal registers are live
				wouldn't interfere with another path where the internal registers
				are also live.
			*/
			if (WouldBeInterveningIRegWrite(psState, psEfoState, uEfoInst, uMadInst))
			{
				continue;
			}
			if (!CheckEfoGroupDependencyForMad(psState, psEfoState, uEfoInst, uOtherInst, uMadInst))
			{
				continue;
			}
			/*
				Check that the EFO groups can still be ordered correctly.
			*/
			if (uExistingGroupId != USC_UNDEF)
			{
				if (!CheckEfoGroupOrder(psState, psEfoState, uExistingGroupId, 
										uMadInst, USC_UNDEF))
				{
					continue;
				}

				/*
					Check if the MAD instruction is dependent on a descheduling instruction which is itself
					dependent on one of the instructions in the EFO group. In that case we would be forced
					the descheduling instruction to be inserted at a point where the internal registers are
					live.
				*/
				if (IsDescheduleBetweenGroupAndInsts(psState, 
													 psEfoState, 
													 uExistingGroupId, 
													 USC_UNDEF, 
													 uMadInst, 
													 USC_UNDEF))
				{
					continue;
				}
			}
			
			return psMadInst;
		}
	}
	return NULL;
}

static IMG_VOID ConvertFloatInstToEfo(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: ConvertFloatInstToEfo
    
 PURPOSE	: Switch an instruction from a scalar floating point operation
			  to an EFO; preserving the floating point source modifiers.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to convert.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	FLOAT_PARAMS	sFloat;

	sFloat = *psInst->u.psFloat;
	SetOpcodeAndDestCount(psState, psInst, IEFO, EFO_DEST_COUNT);
	psInst->u.psEfo->sFloat = sFloat;
}

static IMG_VOID BuildExtraAddInst(PINTERMEDIATE_STATE	psState, 
								  PEFOGEN_STATE			psEfoState,
								  PINST					psExtraAddInst, 
								  PINST					psInstB)
/*****************************************************************************
 FUNCTION	: BuildExtraAddInst
    
 PURPOSE	: Convert a MAD instruction to an EFO which does both the original MAD
			  calculation and the final part of a linear interpolate.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- The block containing the instructions
			  psExtraAddInst	- Instruction to be converted.
			  psInstB			- Second part of the interpolate.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	ARG	sExtraAddDest = psExtraAddInst->asDest[0];
	
	ConvertFloatInstToEfo(psState, psExtraAddInst);

	/* M0 = (a * b), M1 = (a * c); A0 = M0 + c, A1 = I1 + I0 */
	if (psEfoState->bNewEfoFeature)
	{
		/* a -> src2, b -> src1, c -> src0 */
		SwapInstSources(psState, psExtraAddInst, 0 /* uSrc1 */, 2 /* uSrc2 */);

		/*
			Swap the arguments to the multiply part of the 
			calculation to avoid bank restrictions.
		*/
		if (!CanUseEfoSrc(psState, psEfoState->psCodeBlock, 1, psExtraAddInst, 1))
		{
			ASSERT(CanUseEfoSrc(psState, psEfoState->psCodeBlock, 1, psExtraAddInst, 2));

			SwapInstSources(psState, psExtraAddInst, 1 /* uSrc1 */, 2 /* uSrc2 */);
		}

		/*
			M0=S2*S1; A0=M0+SRC0; A1=I1+I0
		*/
		psExtraAddInst->u.psEfo->eM0Src0 = SRC2;  // a
		psExtraAddInst->u.psEfo->eM0Src1 = SRC1;  // b
		psExtraAddInst->u.psEfo->eM1Src1 = SRC2;  // a
		psExtraAddInst->u.psEfo->eM1Src0 = SRC2;  // a
		psExtraAddInst->u.psEfo->eA0Src0 = M0;
		psExtraAddInst->u.psEfo->eA0Src1 = SRC0;  // c
		psExtraAddInst->u.psEfo->eA1Src0 = I1;
		psExtraAddInst->u.psEfo->eA1Src1 = I0;
	}
	else
	{
		/*
			Swap the arguments to the multiply part of the 
			calculation to avoid bank restrictions.
		*/
		if (!CanUseEfoSrc(psState, psEfoState->psCodeBlock, 0, psExtraAddInst, 0))
		{
			ASSERT(CanUseEfoSrc(psState, psEfoState->psCodeBlock, 0, psExtraAddInst, 1));

			SwapInstSources(psState, psExtraAddInst, 0 /* uSrc1 */, 1 /* uSrc2 */);
		}

		/*
			M0=S0*S1; A0=M0+SRC2; A1=I1+I0
		*/
		psExtraAddInst->u.psEfo->eM0Src0 = SRC0;
		psExtraAddInst->u.psEfo->eM0Src1 = SRC1;
		psExtraAddInst->u.psEfo->eM1Src0 = SRC0;
		psExtraAddInst->u.psEfo->eM1Src1 = SRC2;
		psExtraAddInst->u.psEfo->eA0Src0 = M0;
		psExtraAddInst->u.psEfo->eA0Src1 = SRC2;
		psExtraAddInst->u.psEfo->eA1Src0 = I1;
		psExtraAddInst->u.psEfo->eA1Src1 = I0;
	}

	psExtraAddInst->u.psEfo->eDSrc = A0;

	/*
		I0 = A0 (=original MAD calculation)
		I1 = A1 (=LRP result)
	*/
	SetupIRegDests(psState, psExtraAddInst, A0, A1);
	
	/*
		MAD destination = I0
	*/
	SetDestFromArg(psState, psExtraAddInst, EFO_USFROMI0_DEST, &sExtraAddDest);

	/*
		LRP destination = I1
	*/
	MoveDest(psState, psExtraAddInst, EFO_USFROMI1_DEST, psInstB, 0 /* uDestIdx */);

	/*
		Point the EFO destination to a dummy register.
	*/
	psExtraAddInst->u.psEfo->bIgnoreDest = IMG_TRUE;
	SetDest(psState, psExtraAddInst, EFO_US_DEST, USEASM_REGTYPE_TEMP, GetNextRegister(psState), UF_REGFORMAT_F32);

	SetupEfoUsage(psState, psExtraAddInst);

	/*
		Replace hardware constants in the EFO by secondary attributes.
	*/
	ReplaceHardwareConstants(psState, psExtraAddInst);
}

static IMG_BOOL IsIRegDestOverwrite(PINTERMEDIATE_STATE	psState,
									PEFOGEN_STATE		psEfoState,
									PINST				psEfoInst,
									IMG_UINT32			uInstA,
									IMG_UINT32			uInstB)
/*****************************************************************************
 FUNCTION	: IsIRegDestOverwrite
    
 PURPOSE	: Check for an instruction which overwrites either of the EFO
			  I0DEST/I1DEST destinations.

 PARAMETERS	: psState			- Compiler state.
			  psEfoInst			- Index of the source EFO instruction.
			  uInstA			- Instructions using the result of the EFO
			  uInstB			instructions.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32				uDepInst;
	PDGRAPH_STATE			psDepState = psEfoState->psCodeBlock->psDepState;
	IMG_UINT32				uEfoInst = psEfoInst->uId;
	PADJACENCY_LIST			psList;
	ADJACENCY_LIST_ITERATOR	sIter;
	
	psList = (PADJACENCY_LIST)ArrayGet(psState, psDepState->psDepList, uEfoInst);

	for (uDepInst = FirstAdjacent(psList, &sIter); !IsLastAdjacent(&sIter); uDepInst = NextAdjacent(&sIter))
	{
		if (
				GraphGet(psState, psDepState->psClosedDepGraph, uInstA, uDepInst) ||
				GraphGet(psState, psDepState->psClosedDepGraph, uInstB, uDepInst)
		   )
		{
			IMG_UINT32	uIRegIdx;
			PINST		psDepInst = (PINST)ArrayGet(psState, psDepState->psInstructions, uDepInst);

			for (uIRegIdx = 0; uIRegIdx < 2; uIRegIdx++)
			{
				PARG	psUSDest = &psEfoInst->asDest[EFO_USFROMI0_DEST + uIRegIdx];

				if (GetChannelsWrittenInArg(psDepInst, psUSDest, NULL))
				{
					return IMG_TRUE;
				}
			}
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL CheckSrcToIReg(PINTERMEDIATE_STATE	psState, 
							   PEFOGEN_STATE		psEfoState,
							   IMG_UINT32			uEfoGroupIdx,
							   IMG_UINT32			uInstA,
							   PINST				psInstA,
							   IMG_UINT32			uInstB,
							   PINST				psInstB,
							   PIREG_STATUS			psIRegStatus,
							   IMG_PUINT32			puEfoInst)
/*****************************************************************************
 FUNCTION	: CheckSrcToIReg
    
 PURPOSE	: Checks which sources to a pair of instructions could be replaced by
			  internal registers.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  uEfoGroupIdx		- Index of the group writing the internal registers.
			  uInstA, uInstB	- Pair of instructions to check.
			  psInstA, psInstB
			  psIRegStatus		- Returns masks of the sources which could be
								  replaced by each internal register.
			  puEfoInst			- Returns the number of the EFO instruction
								  which writes the internal registers.
			  
 RETURNS	: TRUE if some sources could be replaced by internal registers.

 NOTES		: Checks which sources registers have the same contents as an
			  internal register and so can be replaced by that internal
			  register. 
*****************************************************************************/
{
	IMG_UINT32		uArg;
	IMG_BOOL		bFound = IMG_FALSE; 
	PDGRAPH_STATE	psDepState = psEfoState->psCodeBlock->psDepState;
	PINST			psEfoInst = psEfoState->asEfoGroup[uEfoGroupIdx].psTail;
	IMG_UINT32		uEfoInst = psEfoInst->uId;
	IMG_BOOL		abIRegDestValid[2];
	PARG			apsIRegDest[2];
	IMG_UINT32		uInst;
	IMG_UINT32		auInstSrcsToIRegs[2];
	IMG_UINT32		uIRegIdx;

	ASSERT(psEfoState->psCodeBlock->psDepState != NULL);

	(*puEfoInst) = USC_UNDEF;

	/*
		Ignore uses of the internal registers already present before we started
		generating EFOs.
	*/
	if (psEfoState->asEfoGroup[uEfoGroupIdx].bExistingGroup)
	{
		return IMG_FALSE;
	}

	ASSERT(psEfoInst != NULL);
	ASSERT(psEfoInst->eOpcode == IEFO);

	if (uEfoInst == uInstA || uEfoInst == uInstB)
	{
		return IMG_FALSE;
	}

	/*
		Check at least one of the instructions is dependent on the EFO
		instruction. Otherwise they can't possibly use the results 
		the EFO writes.
	*/
	if (!GraphGet(psState, psDepState->psDepGraph, uInstA, uEfoInst) &&
		!GraphGet(psState, psDepState->psDepGraph, uInstB, uEfoInst))
	{
		return IMG_FALSE;
	}

	/*
		Check there wouldn't be a descheduling instruction between the EFO and the
		combined A+B instruction.
	*/
	if (IsDescheduleBetweenGroupAndInsts(psState, 
										 psEfoState, 
										 psEfoInst->sStageData.psEfoData->uEfoGroupId, 
										 USC_UNDEF, 
										 uInstA, 
										 uInstB))
	{
		return IMG_FALSE;
	}

	/*
		Check there wouldn't be another use of the internal registers between the
		EFO and the combined A+B instruction.
	*/
	if (!CheckEfoGroupOrder(psState, psEfoState, uEfoGroupIdx, uInstA, uInstB))
	{
		return IMG_FALSE;
	}

	/*
		Check for overwriting the EFO results between the EFO and A or B
	*/
	if (IsIRegDestOverwrite(psState, psEfoState, psEfoInst, uInstA, uInstB))
	{
		return IMG_FALSE;
	}
	
	for (uIRegIdx = 0; uIRegIdx < 2; uIRegIdx++)
	{
		apsIRegDest[uIRegIdx] = &psEfoInst->asDest[EFO_USFROMI0_DEST + uIRegIdx];
		if (apsIRegDest[uIRegIdx]->uType != USC_REGTYPE_UNUSEDDEST)
		{
			abIRegDestValid[uIRegIdx] = IMG_TRUE;
		}
		else
		{
			abIRegDestValid[uIRegIdx] = IMG_FALSE;
		}
	}

	for (uInst = 0; uInst < 2; uInst++)
	{
		IMG_PUINT32 puSrcToIReg = (uInst == 0) ? psIRegStatus->puASrcInIReg : psIRegStatus->puBSrcInIReg;
		IMG_PUINT32 puSrcToNegIReg = (uInst == 0) ? psIRegStatus->puASrcInNegIReg : psIRegStatus->puBSrcInNegIReg;
		PINST		psInst;
		IMG_UINT32	uDestIdx;
				
		psInst = (uInst == 0) ? psInstA : psInstB;

		/*
			Check if an arguments to the instruction comes from the results
			of the EFO.
		*/
		for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
		{
			PARG					psArg = &psInst->asArg[uArg];
			PFLOAT_SOURCE_MODIFIER	psMod = &psInst->u.psFloat->asSrcMod[uArg];

			/*
				No absolute modifiers on implicit EFO sources.
			*/
			if (psMod->bAbsolute)
			{
				continue;
			}

			/*
				Check for each of the EFO destinations whether this instruction
				argument is the same register.
			*/
			for (uDestIdx = 0; uDestIdx < 2; uDestIdx++)
			{
				PARG	psDest;

				if (!abIRegDestValid[uDestIdx])
				{
					continue;
				}

				psDest = apsIRegDest[uDestIdx];
				if (psArg->uType == psDest->uType &&
					psArg->uNumber == psDest->uNumber)
				{
					if (!psMod->bNegate)
					{
						puSrcToIReg[uDestIdx] |= (1U << uArg);
					}
					else
					{
						puSrcToNegIReg[uDestIdx] |= (1U << uArg);
					}
					bFound = IMG_TRUE;
				}
			}
		}
		auInstSrcsToIRegs[uInst] = puSrcToIReg[0] | puSrcToIReg[1];
		auInstSrcsToIRegs[uInst] |= puSrcToNegIReg[0] | puSrcToNegIReg[1];

		/*
			Check for the instruction overwriting some of the EFO results.
		*/
		for (uDestIdx = 0; uDestIdx < 2; uDestIdx++)
		{
			if (GetChannelsWrittenInArg(psInst, apsIRegDest[uDestIdx], NULL) != 0)
			{
				abIRegDestValid[uDestIdx] = IMG_FALSE;
			}
		}
	}
	if (bFound)
	{
		IMG_UINT32	uArgCount;
		PARG		ppsArgs[USC_MAXIMUM_NONCALL_ARG_COUNT * 2];

		ASSERT(psInstA->uArgumentCount <= USC_MAXIMUM_NONCALL_ARG_COUNT);
		ASSERT(psInstB->uArgumentCount <= USC_MAXIMUM_NONCALL_ARG_COUNT);

		/*
			If these instructions in combination with the EFO have at most
			three arguments then try to combine them into another EFO.
		*/
		uArgCount = CountArgs(psInstA, 
							  auInstSrcsToIRegs[0], 
							  0, 
							  ppsArgs);
		uArgCount = CountArgs(psInstB, 
							  auInstSrcsToIRegs[1], 
							  uArgCount, 
							  ppsArgs);

		if (uArgCount <= 3)
		{
			*puEfoInst = uEfoInst;
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static IMG_VOID AddIRegDependency(PINTERMEDIATE_STATE psState,
								  PEFOGEN_STATE psEfoState,
								  PINST psEfoInst, 
								  IMG_UINT32 uNewInst)
/*****************************************************************************
 FUNCTION	: AddIRegDependency
    
 PURPOSE	: Adds an dependency between two instructions using the internal register.

 PARAMETERS	: psState			- Compiler state.
			  psEfoInst			- Source instruction.
			  uNewInst			- Destination instruction.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uEfoGroupId = psEfoInst->sStageData.psEfoData->uEfoGroupId;
	IMG_UINT32		uOtherEfoGroupId;
	PINST			psNextEfoInst;

	/*
		For any other EFO group which the new instruction is dependent on:
			The group we adding the new instruction to is now dependent on the other group.
	*/
	for (uOtherEfoGroupId = 0; uOtherEfoGroupId < psEfoState->uEfoGroupCount; uOtherEfoGroupId++)
	{
		if (
				uOtherEfoGroupId != uEfoGroupId &&
				!GetDependency(psEfoState, uEfoGroupId, uOtherEfoGroupId) &&
				IsInstDependentOnGroup(psState, psEfoState, uNewInst, uOtherEfoGroupId)
		   )
		{
			ASSERT(!GetClosedDependency(psEfoState, uOtherEfoGroupId, uEfoGroupId));
			SetDependency(psEfoState, uEfoGroupId, uOtherEfoGroupId, 1);
			UpdateClosedEfoDependencyGraph(psEfoState, 
										   uEfoGroupId, 
										   uOtherEfoGroupId);
		}
	}

	/*
		Add a dependency from all EFO instructions later on in the group on
		the new reader of the internal registers.
	*/
	for (psNextEfoInst = psEfoInst->sStageData.psEfoData->psNextWriter;
		 psNextEfoInst != NULL; 
		 psNextEfoInst = psNextEfoInst->sStageData.psEfoData->psNextWriter)
	{
		IMG_UINT32	uNextEfoInst = psNextEfoInst->uId;

		AddClosedDependency(psState, psEfoState->psCodeBlock->psDepState, uNewInst, uNextEfoInst);
	}
}

/*****************************************************************************
 FUNCTION	: MergeEfoGroup
    
 PURPOSE	: Merge two EFO groups.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- EFO generation stage state.
			  psEfoInst			- Tail of the first group to merge.
			  uEfoInstGroupId	- ID of the first group to merge.
			  psOtherEfoInst	- Head of the second group to merge
			  uOtherEfoInstGroupId
								- ID of the second group to merge.
			  
 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID MergeEfoGroup(PINTERMEDIATE_STATE	psState,
							  PCODEBLOCK			psBlock,
							  PEFOGEN_STATE			psEfoState,
							  PINST					psEfoInst,
							  IMG_UINT32			uEfoInstGroupId,
							  PINST					psOtherEfoInst,
							  IMG_UINT32			uOtherEfoInstGroupId)
{
	PINST			psInst;
	IMG_UINT32		uDepEfoGroupId;

	ASSERT(psEfoInst->sStageData.psEfoData->psNextWriter == NULL);
	ASSERT(psOtherEfoInst->sStageData.psEfoData->psPrevWriter == NULL);
	ASSERT(psEfoState->asEfoGroup[uEfoInstGroupId].psTail == psEfoInst);
	ASSERT(psEfoState->asEfoGroup[uOtherEfoInstGroupId].psHead == psOtherEfoInst);

	/*
		Join the linked list of EFO instructions together.
	*/
	psEfoInst->sStageData.psEfoData->psNextWriter = psOtherEfoInst;
	psOtherEfoInst->sStageData.psEfoData->psPrevWriter = psEfoInst;
	psEfoState->asEfoGroup[uEfoInstGroupId].psTail = psEfoState->asEfoGroup[uOtherEfoInstGroupId].psTail;
	psEfoState->asEfoGroup[uOtherEfoInstGroupId].psHead = NULL;
	psEfoState->asEfoGroup[uOtherEfoInstGroupId].psTail = NULL;

	/*
		Combine the dependencies for both groups.
	*/
	ASSERT(!GetDependency(psEfoState, uEfoInstGroupId, uOtherEfoInstGroupId));
	for (uDepEfoGroupId = 0; 
		 uDepEfoGroupId < psEfoState->uEfoGroupCount; 
		 uDepEfoGroupId++)
	{
		if (uDepEfoGroupId != uOtherEfoInstGroupId &&
			uDepEfoGroupId != uEfoInstGroupId)
		{
			/*
				If OTHER has a dependency on DEP
			*/
			if (GetDependency(psEfoState, uOtherEfoInstGroupId, uDepEfoGroupId))
			{
				/*
					Make the merged group dependent on DEP.
				*/
				SetDependency(psEfoState, uEfoInstGroupId, uDepEfoGroupId, 1);
				UpdateClosedEfoDependencyGraph(psEfoState, uEfoInstGroupId, uDepEfoGroupId);
			}
			/*
				If DEP has a dependency on OTHER
			*/
			if (GetDependency(psEfoState, uDepEfoGroupId, uOtherEfoInstGroupId))
			{
				/*
					Make DEP dependent on the merged group.
				*/
				SetDependency(psEfoState, uDepEfoGroupId, uEfoInstGroupId, 1);
				UpdateClosedEfoDependencyGraph(psEfoState, uDepEfoGroupId, 	uEfoInstGroupId);
				/*
					Clear the dependency on OTHER.
				*/
				SetDependency(psEfoState, uDepEfoGroupId, uOtherEfoInstGroupId, 0);
				SetClosedDependency(psEfoState, uDepEfoGroupId, uOtherEfoInstGroupId, 0);
			}
		}
	}
	ClearDependencies(psEfoState, uOtherEfoInstGroupId);

	/*
		Rename the group ids for the instructions in the other group.
	*/
	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		if (psInst->sStageData.psEfoData->uEfoGroupId == uOtherEfoInstGroupId)
		{
			psInst->sStageData.psEfoData->uEfoGroupId = uEfoInstGroupId;
		}
	}

	/*
		Keep track of how many registers are used in the group.
	*/
	psEfoState->asEfoGroup[uEfoInstGroupId].uArgCount += psEfoState->asEfoGroup[uOtherEfoInstGroupId].uArgCount;
	psEfoState->asEfoGroup[uOtherEfoInstGroupId].uArgCount = USC_UNDEF;

	/*
		Sum the instruction counts for both groups.
	*/
	psEfoState->asEfoGroup[uEfoInstGroupId].uInstCount += psEfoState->asEfoGroup[uOtherEfoInstGroupId].uInstCount;
	psEfoState->asEfoGroup[uOtherEfoInstGroupId].uInstCount = 0;
}

static IMG_BOOL IsInterveningGroup(PINTERMEDIATE_STATE psState,
								   PEFOGEN_STATE psEfoState, 
								   IMG_UINT32 uEfoSrcGroup, 
								   IMG_UINT32 uEfoDestGroup,
								   IMG_UINT32 uOtherInst)
/*****************************************************************************
 FUNCTION	: IsInterveningGroup
    
 PURPOSE	: Checks if there is an indirect dependency between two EFO groups.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  uEfoSrcGroup		- First group.
			  uEfoDestGroup		- Second group.
			  uOtherInst		- If not -1 then an instruction to consider
								as though it were part of the first group.
			  
 RETURNS	: TRUE if an intervening group exists.
*****************************************************************************/
{
	IMG_UINT32	uEfoGroup;

	for (uEfoGroup = 0; uEfoGroup < psEfoState->uEfoGroupCount; uEfoGroup++)
	{
		if (uEfoGroup != uEfoSrcGroup &&
			uEfoGroup != uEfoDestGroup)
		{
			/*
				Check if the second group is dependent on this group.
			*/
			if (GetClosedDependency(psEfoState, uEfoDestGroup, uEfoGroup))
			{
				/*
					Check if this group is dependent on the first group.
				*/
				if (GetClosedDependency(psEfoState, uEfoGroup, uEfoSrcGroup))
				{
					return IMG_TRUE;
				}
				/*
					Check if this group is dependent on the other instruction.
				*/
				if (uOtherInst != USC_UNDEF)
				{
					if (IsGroupDependentOnInst(psState, psEfoState, uOtherInst, uEfoGroup))
					{
						return IMG_TRUE;
					}
				}
			}
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL IsEfoWriterInGroup(PEFOGEN_STATE	psEfoState,
								   IMG_UINT32		uEfoGroup,
								   PINST			psInst)
/*****************************************************************************
 FUNCTION	: IsEfoWriterInGroup
    
 PURPOSE	: Checks if an instruction is one of the internal register writers
			  in a particular group.

 PARAMETERS	: psEfoState		- Module state.
			  uEfoGroup			- Index of the group to check.
			  psInst			- Pointer to the instruction to check.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PINST	psEfoWriter;

	for (psEfoWriter = psEfoState->asEfoGroup[uEfoGroup].psHead;
		 psEfoWriter != NULL;
		 psEfoWriter = psEfoWriter->sStageData.psEfoData->psNextWriter)
	{
		if (psEfoWriter == psInst)
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}


static IMG_BOOL IsEfoOrDeschedDependent(PINTERMEDIATE_STATE		psState,
										PEFOGEN_STATE			psEfoState,
										PARG					psDest,
										PINST*					ppsEfoDependencyInst,
										PINST*					ppsDeschedDependencyInst)
/*****************************************************************************
 FUNCTION	: IsEfoOrDeschedDependent
    
 PURPOSE	: Checks if an instruction has a dependency through one of its unified
			  store destinations on another EFO or a descheduling instruction.

 PARAMETERS	: psState			- Compiler state.
			  psDest			- Unified store destination to check.
			  ppsEfoDependencyInst
								- Returns the dependent EFO if it exists; otherwise NULL.
			  ppsDeschedDependencyInst
								- Returns the dependent descheduling instruction if it
								exists; otherwise NULL.
			  
 RETURNS	: FALSE if there are two or more EFO or descheduling dependents.
*****************************************************************************/
{	
	PUSEDEF_CHAIN		psDestUseDef;
	PUSC_LIST_ENTRY		psListEntry;

	/*
		Check for an empty destination.
	*/
	if (psDest == NULL)
	{
		return IMG_TRUE;
	}

	psDestUseDef = UseDefGet(psState, psDest->uType, psDest->uNumber);

	/*
		Check for a destination without USE-DEF information available.
	*/
	if (psDestUseDef == NULL)
	{
		return IMG_FALSE;
	}

	/*
		For all instructions dependent on the instructions.
	*/
	for (psListEntry = psDestUseDef->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		PINST	psDepInst;

		if (psUse->eType != USE_TYPE_SRC)
		{
			continue;
		}
		psDepInst = psUse->u.psInst;
		if (psDepInst->psBlock != psEfoState->psCodeBlock)
		{
			continue;
		}
	
		if (
				psDepInst->sStageData.psEfoData->uEfoGroupId != USC_UNDEF &&
				IsEfoWriterInGroup(psEfoState, psDepInst->sStageData.psEfoData->uEfoGroupId, psDepInst)
		    )
		{
			/*
				Fail if more than one EFO or a descheduling instruction uses the register.
			*/
			if ((*ppsEfoDependencyInst != NULL && *ppsEfoDependencyInst != psDepInst) || *ppsDeschedDependencyInst != NULL)
			{
				return IMG_FALSE;
			}
			*ppsEfoDependencyInst = psDepInst;
		}
		/*
			Fail if the internal registers are lost before the instruction.
		*/
		if (IsDeschedBeforeInst(psState, psDepInst))
		{
			return IMG_FALSE;
		}
		if (IsDeschedAfterInst(psDepInst))
		{
			/*
				Fail if more than one descheduling instruction or EFO uses the register.
			*/
			if (*ppsEfoDependencyInst != NULL || (*ppsDeschedDependencyInst != NULL && *ppsDeschedDependencyInst != psDepInst))
			{
				return IMG_FALSE;
			}
			*ppsDeschedDependencyInst = psDepInst;
		}
	}
	return IMG_TRUE;
}

static IMG_BOOL CheckWriteAfterReadDependency(PINTERMEDIATE_STATE	psState,
											  PEFOGEN_STATE			psEfoState,
											  PINST					psEfoInst,
											  IMG_UINT32			uDestInst,
											  IMG_BOOL				bAllowDirect)
/*****************************************************************************
 FUNCTION	: CheckWriteAfterReadDependency
    
 PURPOSE	: Checks if it is possible to insert a write-after-read dependency when
			  doing an internal register writeback using another efo instruction.

 PARAMETERS	: psState			- Compiler state.
			  uEfoInst			- Instruction which writes the internal register.
			  uDestInst			- Instruction which uses the result of the writeback.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PDGRAPH_STATE	psDepState = psEfoState->psCodeBlock->psDepState;
	PINST			psDepInst;

	/*
		We are going to add a dependency onto the destination instruction from
		another EFO instruction so we must ensure that this doesn't add a
		dependency onto an instruction which uses the internal registers
		written by the first EFO from the other EFO e.g.

		(A) I[0] = ...; R[n] = ...
		(B) R[m] = ..., R[n], ...
		(C) ...., I0, R[m], ...

		If another EFO is used to do the write to R[n] then B (and indirectly C) will
		be dependent on the other EFO but the other EFO will overwrite the internal
		register result from A used by C.
	*/
	for (psDepInst = psEfoInst->sStageData.psEfoData->psFirstReader; 
		 psDepInst != NULL; 
		 psDepInst = psDepInst->sStageData.psEfoData->psNextReader)
	{
		IMG_UINT32	uDepInst = psDepInst->uId;

		ASSERT(psDepInst->sStageData.psEfoData->uEfoGroupId == psEfoInst->sStageData.psEfoData->uEfoGroupId);

		if (GraphGet(psState, psDepState->psClosedDepGraph, uDepInst, uDestInst))
		{
			return IMG_FALSE;
		}
		if (!bAllowDirect && uDepInst == uDestInst)
		{
			return IMG_FALSE;
		}
	}
	return IMG_TRUE;
}


static IMG_BOOL CheckForDeschedReaders(PINTERMEDIATE_STATE	psState,
									   PINST				psEfoInst)
/*****************************************************************************
 FUNCTION	: CheckForDeschedReaders
    
 PURPOSE	: Check if any of the instructions which read internal registers
			  written by an EFO are descheduling instruction.

 PARAMETERS	: psState			- Compiler state.
			  psEfoInst			- Instruction which writes the internal register.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PINST			psDepInst;

	for (psDepInst = psEfoInst->sStageData.psEfoData->psFirstReader;
		 psDepInst != NULL; 
		 psDepInst = psDepInst->sStageData.psEfoData->psNextReader)
	{
		ASSERT(psDepInst->sStageData.psEfoData->uEfoGroupId == psEfoInst->sStageData.psEfoData->uEfoGroupId);

		ASSERT(!IsDeschedBeforeInst(psState, psDepInst));
		if (IsDeschedAfterInst(psDepInst))
		{
			return IMG_FALSE;
		}
	}
	return IMG_TRUE;
}

typedef struct _IREG_SOURCE_REPLACE
{
	IMG_UINT32	uD1UsedMask;
	IMG_UINT32	uD2UsedMask;
	IMG_BOOL	bValid;
} IREG_SOURCE_REPLACE, *PIREG_SOURCE_REPLACE;

static IMG_VOID CheckIRegReplaceGroupCB(PINTERMEDIATE_STATE	psState,
										PINST				psInst,
										IMG_BOOL			bDest,
										IMG_UINT32			uRegSetStart,
										IMG_UINT32			uRegSetSize,
										HWREG_ALIGNMENT		eGroupAlign,
										IMG_PVOID			pvReplace)
/*****************************************************************************
 FUNCTION	: CheckIRegReplaceGroupCB
    
 PURPOSE	: Where some of the sources to an instruction require consecutive
			  hardware register numbers check that if the sources are replaced
			  by internal registers the register number order will still be
			  correct.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Current instruction.
			  bDest				- TRUE for a group of destinations.
								  FALSE for a group of sources.
			  uRegSetStart		- Start of the group of arguments.
			  uRegSetSize		- Count of arguments in the group.
			  eGroupAlign		- Required alignment for the hardware register
								number assigned to the first argument in the group.
			  pvReplace			- Details of the replacement.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PIREG_SOURCE_REPLACE	psReplace = (PIREG_SOURCE_REPLACE)pvReplace;
	IMG_UINT32				uRegSetMask;

	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst);
	PVR_UNREFERENCED_PARAMETER(eGroupAlign);

	ASSERT(!bDest);

	uRegSetMask = ((1U <<uRegSetSize) - 1) << uRegSetStart;

	/*
		Are we trying to replace any arguments in the set.
	*/
	if ((uRegSetMask & (psReplace->uD1UsedMask | psReplace->uD2UsedMask)) != 0)
	{
		/*
			We must be able to completely replace the existing arguments in the set.
		*/
		if (uRegSetSize > 2)
		{
			psReplace->bValid = IMG_FALSE;
			return;
		}
		if (uRegSetSize == 2)
		{
			/*
				Check that we can replace the first argument in the set by i0.
			*/
			if (((psReplace->uD1UsedMask & uRegSetMask) >> uRegSetStart) != 1)
			{
				psReplace->bValid = IMG_FALSE;
				return;
			}
			/*
				Check that we can replace the second argument in the set by i1.
			*/
			if (((psReplace->uD2UsedMask & uRegSetMask) >> uRegSetStart) != 2)
			{
				psReplace->bValid = IMG_FALSE;
				return;
			}
		}
	}
}

static IMG_BOOL GetNextInstUse(PCODEBLOCK		psBlock,
							   PUSC_LIST_ENTRY	psListEntry,
							   PINST*			ppsInst)
/*****************************************************************************
 FUNCTION	: GetNextInstUse
    
 PURPOSE	: Get the next instruction using a temporary register as a source.

 PARAMETERS	: psBlock			- Basic block.
			  psListEntry		- Next entry in the temporary register's USE
								list.
			  ppsInst			- Returns the instruction.
	
 RETURNS	: FALSE if the next instruction is in a different basic block.
*****************************************************************************/
{
	PUSEDEF	psUse;

	if (psListEntry == NULL)
	{
		*ppsInst = NULL;
		return IMG_TRUE;
	}

	psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

	/*
		Fail if the temporary register is used other than as a source.
	*/
	if (psUse->eType != USE_TYPE_SRC)
	{
		return IMG_FALSE;
	}

	/*
		Fail if the temporary register is used in a different block.
	*/
	if (psUse->u.psInst->psBlock != psBlock)
	{
		return IMG_FALSE;
	}
	if (psUse->u.psInst->eOpcode == IDELTA || psUse->u.psInst->eOpcode == ICALL)
	{
		return IMG_FALSE;
	}

	*ppsInst = psUse->u.psInst;
	return IMG_TRUE;
}

static IMG_UINT32 GetInstUseMask(PINST				psInst, 
								 PUSC_LIST_ENTRY*	ppsListEntry)
/*****************************************************************************
 FUNCTION	: GetInstUseMask
    
 PURPOSE	: Get the mask of sources to an instruction which are a particular
			  temporary register.

 PARAMETERS	: psInst		- Instruction to get uses of.
			  ppsListEntry	- On input: points to the first entry in the
							  USE list to check.
						      On output: updated to point to the first entry
							  in a different instruction.
	
 RETURNS	: The mask of instruction sources 
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;
	IMG_UINT32		uSrcMask;

	psListEntry = *ppsListEntry;

	uSrcMask = 0;

	while (psListEntry != NULL)
	{
		PUSEDEF	psUse;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

		/*
			Stop if the temporary register is used other than as a source.
		*/
		if (psUse->eType != USE_TYPE_SRC)
		{
			break;
		}
		
		/*
			Stop once we've found all the uses in a particular instruction.
		*/
		if (psInst != psUse->u.psInst)
		{
			break;
		}

		uSrcMask |= (1U << psUse->uLocation);

		psListEntry = psListEntry->psNext;
	}
	
	(*ppsListEntry) = psListEntry;

	return uSrcMask;
}

static IMG_BOOL NextEfoDestUses(PCODEBLOCK				psBlock, 
								PUSC_LIST_ENTRY*		ppsListEntry1,
								PUSC_LIST_ENTRY*		ppsListEntry2,
								PINST*					ppsUseInst,
								IMG_PUINT32				puD1SrcMask,
								IMG_PUINT32				puD2SrcMask)
/*****************************************************************************
 FUNCTION	: NextEfoDestUses
    
 PURPOSE	: Get the next instruction using either of a pair of temporary
			  registers.

 PARAMETERS	: psBlock			- Current basic block.
			  ppsListEntry1		- Current entry in the USE-DEF chain
			  ppsListEntry2		for each temporary register.
			  ppsUseInst		- Returns the instruction using the register.
			  puD1SrcMask		- Returns the mask of instruction sources which
								are the first temporary register.
			  puD2SrcMask		- Returns the mask of instruction sources which
								are the second temporary register.
			  
 RETURNS	: FALSE if either temporary register is used outside of the current
			  block or other than as a source.
*****************************************************************************/
{
	PINST	psD1Inst, psD2Inst;
	PINST	psUseInst;

	/*
		Get the next instruction from each USE-DEF chain.
	*/
	if (!GetNextInstUse(psBlock, *ppsListEntry1, &psD1Inst))
	{
		return IMG_FALSE;
	}
	if (!GetNextInstUse(psBlock, *ppsListEntry2, &psD2Inst))
	{
		return IMG_FALSE;
	}

	/*
		Process the earliest instruction.
	*/
	if (psD1Inst != NULL && (psD2Inst == NULL || psD1Inst->uBlockIndex < psD2Inst->uBlockIndex))
	{
		psUseInst = psD1Inst;
	}
	else
	{
		psUseInst = psD2Inst;
	}

	/*
		Get the mask of sources in the selected instruction which are one of the two temporary
		registers.
	*/
	*puD1SrcMask = *puD2SrcMask = 0;
	if (psD1Inst == psUseInst)
	{
		*puD1SrcMask = GetInstUseMask(psUseInst, ppsListEntry1);
	}
	if (psD2Inst == psUseInst)
	{
		*puD2SrcMask = GetInstUseMask(psUseInst, ppsListEntry2);
	}

	*ppsUseInst = psUseInst;
	return IMG_TRUE;
}

static IMG_BOOL CanReplaceIRegMove(PINTERMEDIATE_STATE	psState,
								   PEFOGEN_STATE		psEfoState, 
								   PCODEBLOCK			psBlock,
								   PINST				psEfoInst,
								   IMG_UINT32			uIRegWriterInst,
								   PARG					psDest1,
								   PARG					psDest2,
								   PINST*				ppsEfoDependencyInst,
								   PINST*				ppsDeschedDependencyInst)
/*****************************************************************************
 FUNCTION	: CanReplaceIRegMove
    
 PURPOSE	: Checks if a temporary register can be replaced by an internal register.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  psBlock			- Block containing the EFO instruction.
			  psEfoInst			- Instruction which writes the internal register.
			  psDest1			- Temporary register to replace.
			  psDest2			- Second temporary register to replace. This argument can be
								NULL to replace only one register.
			  ppsEfoDependencyInst		- Returns information to pass to ReplaceIRegMove.
			  ppsDeschedDependencyInst
			  
 RETURNS	: TRUE if the replacement can be done.
*****************************************************************************/
{

	PDGRAPH_STATE			psDepState = psEfoState->psCodeBlock->psDepState;
	IMG_UINT32				uEfoInst = psEfoInst->uId;
	IMG_UINT32				uEfoDependencyInst;
	PINST					psEfoDependencyInst;
	PINST					psDeschedDependencyInst;
	IMG_UINT32				uOtherInst;
	IMG_UINT32				uEfoGroupId = psEfoInst->sStageData.psEfoData->uEfoGroupId;
	PUSEDEF_CHAIN			psDest1UseDef;
	PUSEDEF_CHAIN			psDest2UseDef;
	PUSC_LIST_ENTRY			psDest1ListEntry;
	PUSC_LIST_ENTRY			psDest2ListEntry;

	/*
		If we are considering whether another instruction could be converted to an EFO to
		an internal register writeback then we need to treat that instruction as part of the
		EFO group.
	*/
	if (uIRegWriterInst == uEfoInst)
	{
		uOtherInst = USC_UNDEF;
	}
	else
	{
		uOtherInst = uIRegWriterInst;
	}

	uEfoDependencyInst = USC_UNDEF;

	/*
		Get USE-DEF information for both destinations.
	*/
	psDest1UseDef = NULL;
	if (psDest1 != NULL)
	{
		psDest1UseDef = UseDefGet(psState, psDest1->uType, psDest1->uNumber);
		if (psDest1UseDef == NULL)
		{
			return IMG_FALSE;
		}
	}
	psDest2UseDef = NULL;
	if (psDest2 != NULL)
	{
		psDest2UseDef = UseDefGet(psState, psDest2->uType, psDest2->uNumber);
		if (psDest2UseDef == NULL)
		{
			return IMG_FALSE;
		}
	}

	/*
		Check if there is another EFO instruction or descheduling instruction which uses the result from this EFO.
	*/
	*ppsEfoDependencyInst = NULL;
	*ppsDeschedDependencyInst = NULL;
	if (!IsEfoOrDeschedDependent(psState, 
								 psEfoState, 
								 psDest1,
								 ppsEfoDependencyInst, 
								 ppsDeschedDependencyInst))
	{
		return IMG_FALSE;
	}
	if (!IsEfoOrDeschedDependent(psState, 
								 psEfoState, 
								 psDest2,
								 ppsEfoDependencyInst, 
								 ppsDeschedDependencyInst))
	{
		return IMG_FALSE;
	}
	psEfoDependencyInst = *ppsEfoDependencyInst;
	psDeschedDependencyInst = *ppsDeschedDependencyInst;

	if (psEfoDependencyInst != NULL)
	{
		IMG_UINT32	uEfoDependencyGroupId;

		uEfoDependencyInst = psEfoDependencyInst->uId;
		uEfoDependencyGroupId = psEfoDependencyInst->sStageData.psEfoData->uEfoGroupId;

		/*
			If there a third EFO which uses internal register results from the first
			then the second can't use them..
		*/
		if (psEfoInst->sStageData.psEfoData->psNextWriter != NULL &&
			psEfoInst->sStageData.psEfoData->psNextWriter != psEfoDependencyInst)
		{
			return IMG_FALSE;
		}
		/*
			Similarly if the EFO dependency uses results in the internal registers from
			a different, previous EFO.
		*/
		if (psEfoDependencyInst->sStageData.psEfoData->psPrevWriter != NULL &&
			psEfoDependencyInst->sStageData.psEfoData->psPrevWriter != psEfoInst)
		{
			return IMG_FALSE;
		}
		/*
			Check for a different group of EFOs which would have to be scheduled
			between this EFO and the dependent EFO.
		*/
		ASSERT(uEfoGroupId == uEfoDependencyGroupId || 
			   uOtherInst != USC_UNDEF || 
			   GetClosedDependency(psEfoState, uEfoDependencyGroupId, uEfoGroupId));
		if (IsInterveningGroup(psState, psEfoState, uEfoGroupId, uEfoDependencyGroupId, uOtherInst))
		{
			return IMG_FALSE;
		}
		/*
			Check for descheduling instruction in between the two groups.
		*/
		if (IsDescheduleBetweenGroups(psState, psEfoState, uEfoGroupId, uEfoDependencyGroupId, uOtherInst))
		{
			return IMG_FALSE;
		}
		/*
			Check that all the instructions that are dependent through an internal register
			on the first EFO aren't also dependent on the second EFO.
		*/
		if (!CheckWriteAfterReadDependency(psState, psEfoState, psEfoInst, uEfoDependencyInst, IMG_TRUE))
		{
			return IMG_FALSE;
		}
		/*
			Check if any of the instructions which read the internal registers
			written by the first EFO are descheduling instructions. These instructions
			need to be before the second EFO (since it will overwrite the internal
			registers) but they themselves cause the values in the internal
			registers to be lost.
		*/
		if (!CheckForDeschedReaders(psState, psEfoInst))
		{
			return IMG_FALSE;
		}
	}
	else if (psDeschedDependencyInst != NULL)
	{
		IMG_UINT32	uDeschedDependencyInst;
		uDeschedDependencyInst = psDeschedDependencyInst->uId;

		/*
			Check if the results that the EFO instruction writes to internal registers are used by
			another EFO. The second EFO will overwrite the internal registers so the descheduling
			instruction can't go after it but the descheduling instruction will cause the contents
			of the internal registers to be lost so it can't go before the EFO either.
		*/
		if (psEfoInst->sStageData.psEfoData->psNextWriter != NULL)
		{
			return IMG_FALSE;
		}
		/*
			Check that all the instructions which have a dependency through an internal register on
			the EFO can be made dependencies of the descheduling instruction.
		*/
		if (!CheckWriteAfterReadDependency(psState, psEfoState, psEfoInst, uDeschedDependencyInst, IMG_TRUE))
		{
			return IMG_FALSE;
		}
		/*
			Check there isn't already a descheduling instruction with an internal register source written by
			this EFO.
		*/
		if (!CheckForDeschedReaders(psState, psEfoInst))
		{
			return IMG_FALSE;
		}
	}

	/*	
		Get the start of the list of uses for the first destination.
	*/
	psDest1ListEntry = NULL;
	if (psDest1UseDef != NULL)
	{
		psDest1ListEntry = psDest1UseDef->sList.psHead;
	}

	/*
		Get the start of the list of uses for the second destination.
	*/
	psDest2ListEntry = NULL;
	if (psDest2UseDef != NULL)
	{
		psDest2ListEntry = psDest2UseDef->sList.psHead;
	}

	for (;;)
	{
		PINST				psDepInst;
		IMG_UINT32			uDepInst;
		IMG_UINT32			uUsedMask;
		IREG_SOURCE_REPLACE	sReplace;
		IMG_BOOL			bSwapped = IMG_FALSE;
		IMG_UINT32			uArg;

		/*
			Skip the definitions of the two temporary registers.
		*/
		if (psDest1UseDef != NULL && psDest1ListEntry == &psDest1UseDef->psDef->sListEntry)
		{
			psDest1ListEntry = psDest1ListEntry->psNext;
		}
		if (psDest2UseDef != NULL && psDest2ListEntry == &psDest2UseDef->psDef->sListEntry)
		{
			psDest2ListEntry = psDest2ListEntry->psNext;
		}

		if (psDest1ListEntry == NULL && psDest2ListEntry == NULL)
		{
			break;
		}

		/*
			Check which sources need to be replaced.
		*/
		if (!NextEfoDestUses(psBlock, 
							 &psDest1ListEntry,
							 &psDest2ListEntry, 
							 &psDepInst, 
							 &sReplace.uD1UsedMask,
							 &sReplace.uD2UsedMask))
		{
			return IMG_FALSE;
		}
		uUsedMask = sReplace.uD1UsedMask | sReplace.uD2UsedMask;
		uDepInst = psDepInst->uId;

		/*
			Check for an instruction which can't have an internal register as a source.
		*/
		ASSERT(!IsDeschedBeforeInst(psState, psDepInst));
		ASSERT(!IsDeschedAfterInst(psDepInst) || psDepInst == psDeschedDependencyInst);
		/*
			Check if we can get around the restrictions on source 0 by
			swapped the first two sources.
		*/
		if (
				/* Not used in both sources. */
				(uUsedMask & 3) == 1 &&
				/* An internal register can't be used in source 0 */
				!CanUseSrc(psState, psDepInst, 0, USEASM_REGTYPE_FPINTERNAL, USC_REGTYPE_NOINDEX) &&
				/* An internal register can be used in source 1 */
				CanUseSrc(psState, psDepInst, 1, USEASM_REGTYPE_FPINTERNAL, USC_REGTYPE_NOINDEX) &&
				/* The first two sources commute */
				InstSource01Commute(psState, psDepInst) &&
				/* Can move a source modifier to source 0 */
				CanHaveSourceModifier(psState, psDepInst, 0, IsNegated(psState, psDepInst, 1), IsAbsolute(psState, psDepInst, 1)) &&
				/* Can use the existing source 1 in source 0 */
				CanUseSrc(psState, psDepInst, 0, psDepInst->asArg[1].uType, psDepInst->asArg[1].uIndexType)
		   )
		{
			bSwapped = IMG_TRUE;
		}
		/*
			Check for trying to use an internal register in a source that 
			doesn't support it.
		*/
		if (!bSwapped)
		{
			for (uArg = 0; uArg < psDepInst->uArgumentCount; uArg++)
			{
				if (uUsedMask & (1U << uArg))
				{
					if (!CanUseSrc(psState, psDepInst, uArg, USEASM_REGTYPE_FPINTERNAL, USC_REGTYPE_NOINDEX))
					{
						return IMG_FALSE;
					}
				}
			}
		}
		/*
			Check for an instruction which requires some of its arguments to have consecutive register numbers.
		*/
		sReplace.bValid = IMG_TRUE;
		ProcessSourceRegisterGroups(psState, psDepInst, CheckIRegReplaceGroupCB, (IMG_PVOID)&sReplace);
		if (!sReplace.bValid)
		{
			return IMG_FALSE;
		}
		/*
			Is a dependency through an internal register legal between the two instructions.
		*/
		if (IsDescheduleBetweenGroupAndInsts(psState, 
											 psEfoState, 
											 psEfoInst->sStageData.psEfoData->uEfoGroupId, 
											 uOtherInst,
											 uDepInst, 
											 USC_UNDEF))
		{
			return IMG_FALSE;
		}
		if (IsInterveningIRegWriteForRead(psState, psEfoState, psEfoInst, uOtherInst, uDepInst))
		{
			return IMG_FALSE;
		}
		/*
			Check the instruction either belongs to no EFO group or the same EFO group as
			the current EFO.
		*/
		if (psDepInst->sStageData.psEfoData->uEfoGroupId != USC_UNDEF && 
			psDepInst->sStageData.psEfoData->uEfoGroupId != psEfoInst->sStageData.psEfoData->uEfoGroupId &&
			uDepInst != uEfoDependencyInst)
		{
			return IMG_FALSE;
		}
		/*
			We must be able to make an EFO instruction which uses the internal registers a dependency
			of all the instructions which also use the internal registers since the EFO will
			overwrite them.
		*/
		if (psEfoDependencyInst != NULL)
		{
			if (GraphGet(psState, psDepState->psClosedDepGraph, uDepInst, uEfoDependencyInst))
			{
				return IMG_FALSE;
			}
		}
	}
	return IMG_TRUE;
}

static IMG_VOID AddWriteAfterReadDependency(PINTERMEDIATE_STATE	psState,
											PEFOGEN_STATE		psEfoState,
											PINST				psEfoInst,
											IMG_UINT32			uOtherEfoInst)
/*****************************************************************************
 FUNCTION	: AddWriteAfterReadDependency
    
 PURPOSE	: Adds write after read dependencies when using another EFO instruction.
			  to do a writeback from an internal register.

 PARAMETERS	: psState			- Compiler state.
			  psEfoInst			- Instruction which writes the internal register.
			  uOtherEfoInst		- EFO instruction which does a writeback
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST			psDepInst;

	for (psDepInst = psEfoInst->sStageData.psEfoData->psFirstReader;
		 psDepInst != NULL; 
		 psDepInst = psDepInst->sStageData.psEfoData->psNextReader)
	{
		IMG_UINT32	uDepInst = psDepInst->uId;

		ASSERT(psDepInst->sStageData.psEfoData->uEfoGroupId == psEfoInst->sStageData.psEfoData->uEfoGroupId);

		if (uDepInst != uOtherEfoInst)
		{
			AddClosedDependency(psState, psEfoState->psCodeBlock->psDepState, uDepInst, uOtherEfoInst);
		}
	}
}

static IMG_VOID ReplaceByInternalRegister(PINTERMEDIATE_STATE	psState,
										  PEFOGEN_STATE			psEfoState,
										  PARG					psDest,
										  IMG_UINT32			uIRegNum,
										  PINST					psEfoInst,
										  PINST					psEfoDependencyInst)
/*****************************************************************************
 FUNCTION	: ReplaceByInternalRegister
    
 PURPOSE	: Replace all uses of a temporary register by an internal register.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- EFO generation state.
			  psDest			- Temporary register to replace.
			  uIRegNum			- Internal register to replace by.
			  psEfoInst			- Instruction writing the internal register.
			  psEfoDependencyInst
								- EFO instruction reading the temporary register.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psDestUseDef;
	PUSC_LIST_ENTRY	psListEntry;
	PUSC_LIST_ENTRY	psNextListEntry;

	/*
		Get USE/DEF information for the register to be replaced.
	*/
	psDestUseDef = UseDefGet(psState, psDest->uType, psDest->uNumber);
	ASSERT(psDestUseDef != NULL);

	for (psListEntry = psDestUseDef->sList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PUSEDEF		psUse;
		PINST		psDepInst;
		IMG_UINT32	uDepInst;
		IMG_UINT32	uArg;

		psNextListEntry = psListEntry->psNext;
		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

		if (psUse == psDestUseDef->psDef)
		{
			continue;
		}

		ASSERT(psUse->eType == USE_TYPE_SRC);

		psDepInst = psUse->u.psInst;
		uDepInst = psDepInst->uId;
		uArg = psUse->uLocation;

		/*
			Replace the argument.
		*/
		SetSrc(psState, psDepInst, uArg, USEASM_REGTYPE_FPINTERNAL, uIRegNum, UF_REGFORMAT_F32);
		
		/*
			Check if we can get around the limitations on source 0 by swapping the arguments.
		*/
		if (!CanUseSrc(psState, psDepInst, uArg, psDepInst->asArg[uArg].uType, USC_REGTYPE_NOINDEX))
		{
			ASSERT(uArg == 0);
			ASSERT(InstSource01Commute(psState, psDepInst));
			ASSERT(!(psDest != NULL && psDepInst->asArg[1].uType == psDest->uType && psDepInst->asArg[1].uNumber == psDest->uNumber));
			ASSERT(CanUseSrc(psState, psDepInst, 0, psDepInst->asArg[1].uType, psDepInst->asArg[1].uIndexType));
			ASSERT(CanUseSrc(psState, psDepInst, 1, psDepInst->asArg[uArg].uType, USC_REGTYPE_NOINDEX));

			CommuteSrc01(psState, psDepInst);
		}

		/*
			Add the instruction to the group linked through the internal registers.
		*/
		if (psDepInst != psEfoDependencyInst)
		{
			IMG_UINT32	uEfoGroupId = psEfoInst->sStageData.psEfoData->uEfoGroupId;

			if (psDepInst->sStageData.psEfoData->uEfoGroupId != uEfoGroupId)
			{
				ASSERT(psDepInst->sStageData.psEfoData->uEfoGroupId == USC_UNDEF);
				psDepInst->sStageData.psEfoData->uEfoGroupId = uEfoGroupId;
				AddIRegDependency(psState, psEfoState, psEfoInst, uDepInst);
				AddToEfoReaderList(psEfoInst, psDepInst);

				psEfoState->asEfoGroup[uEfoGroupId].uInstCount++;
			}
		}
	}
}

static IMG_VOID ReplaceIRegMove(PINTERMEDIATE_STATE psState, 
								PCODEBLOCK			psBlock,
								PEFOGEN_STATE		psEfoState,
								PINST				psEfoInst,
								IMG_UINT32			uIRegWriterInst,
								PARG				psDest1, 
								PARG				psDest2,
								IMG_UINT32			uIRegNum,
								PINST				psEfoDependencyInst,
								PINST				psDeschedDependencyInst)
/*****************************************************************************
 FUNCTION	: ReplaceIRegMove
    
 PURPOSE	: Replace all uses of a temporary register by an internal register.

 PARAMETERS	: psState			- Compiler state.
			  uEfoInst			- Instruction which writes the internal register.
			  psDest			- Temporary register to replace.
			  uIRegNum			- Internal register to replace.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PDGRAPH_STATE			psDepState = psBlock->psDepState;
	IMG_UINT32				uEfoDependencyInst, uDeschedDependencyInst;

	if (psEfoDependencyInst != NULL)
	{
		IMG_UINT32	uEfoInstGroupId;
		IMG_UINT32	uEfoDependencyInstGroupId;

		ASSERT(psDeschedDependencyInst == NULL);

		uEfoDependencyInst = psEfoDependencyInst->uId;

		uEfoInstGroupId = psEfoInst->sStageData.psEfoData->uEfoGroupId;
		uEfoDependencyInstGroupId = psEfoDependencyInst->sStageData.psEfoData->uEfoGroupId;

		ASSERT(GraphGet(psState, psDepState->psDepGraph, uEfoDependencyInst, uIRegWriterInst));

		/*
			Add a dependency onto any instruction which uses internal registers
			written by the first EFO from the other EFO.
		*/
		AddWriteAfterReadDependency(psState, psEfoState, psEfoInst, uEfoDependencyInst);

		if (uEfoInstGroupId != uEfoDependencyInstGroupId)
		{
			MergeEfoGroup(psState, psBlock, psEfoState, psEfoInst, uEfoInstGroupId, psEfoDependencyInst, uEfoDependencyInstGroupId);
		}

		/*
			Add the second EFO instruction to list of readers of internal registers written by the first.
		*/
		AddToEfoReaderList(psEfoInst, psEfoDependencyInst);
	}
	else if (psDeschedDependencyInst != NULL)
	{
		/*
			Add a dependency onto any instruction which uses internal registers
			written by the first EFO from the descheduling instruction.
		*/
		uDeschedDependencyInst = psDeschedDependencyInst->uId;
		AddWriteAfterReadDependency(psState, psEfoState, psEfoInst, uDeschedDependencyInst);
	}

	/*
		Replace all occurances of each destination by the corresponding internal register.
	*/
	if (psDest1 != NULL)
	{
		ReplaceByInternalRegister(psState, psEfoState, psDest1, uIRegNum + 0, psEfoInst, psEfoDependencyInst);
	}
	if (psDest2 != NULL)
	{
		ReplaceByInternalRegister(psState, psEfoState, psDest2, uIRegNum + 1, psEfoInst, psEfoDependencyInst);
	}
}

static IMG_BOOL CanWriteDestUsingEfo(PINTERMEDIATE_STATE	psState,
									 PEFOGEN_STATE			psEfoState,
									 PINST					psEfoInst,
									 PARG					psDest,
									 IMG_UINT32				uOtherEfoInst,
									 IMG_BOOL				bOtherInstOverwritesBothIRegs)
/*****************************************************************************
 FUNCTION	: CanWriteDestUsingEfo
    
 PURPOSE	: Checks if an EFO instruction can be used to do a move from an	
			  internal register to a temporary.

 PARAMETERS	: psState			- Compiler state.
			  uEfoInst			- Instruction which writes the internal register.
			  psDest			- Temporary register.
			  uOtherEfoInst		- Instruction to check.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32				uDepInst;
	PDGRAPH_STATE			psDepState = psEfoState->psCodeBlock->psDepState;
	IMG_UINT32				uEfoInst = psEfoInst->uId;
	PADJACENCY_LIST			psList;
	ADJACENCY_LIST_ITERATOR	sIter;

	ASSERT(psEfoState->psCodeBlock->psDepState != NULL);

	psList = (PADJACENCY_LIST)ArrayGet(psState, psDepState->psDepList, uEfoInst);
	for (uDepInst = FirstAdjacent(psList, &sIter); !IsLastAdjacent(&sIter); uDepInst = NextAdjacent(&sIter))
	{
		PINST		psDepInst;
		IMG_UINT32	uUsedMask;
		IMG_UINT32	uArg;
		IMG_UINT32	uDestIdx;

		psDepInst = ArrayGet(psState, psDepState->psInstructions, uDepInst);
		
		/*
			Check if this instruction overwrites the destination.
		*/
		for (uDestIdx = 0; uDestIdx < psDepInst->uDestCount; uDestIdx++)
		{
			if (psDepInst->asDest[uDestIdx].uType == psDest->uType &&
				psDepInst->asDest[uDestIdx].uNumber == psDest->uNumber)
			{
				return IMG_FALSE;
			}
		}

		/*
			Check if this instruction uses the temporary register destination.
		*/
		uUsedMask = 0;
		for (uArg = 0; uArg < psDepInst->uArgumentCount; uArg++)
		{
			if (psDepInst->asArg[uArg].uType == psDest->uType &&
				psDepInst->asArg[uArg].uNumber == psDest->uNumber)
			{
				uUsedMask |= (1U << uArg);
				break;
			}
		}

		if (uUsedMask != 0)
		{
			/*
				If the other EFO instruction uses the register as a source then check we can
				replace the source by the corresponding internal register.
			*/
			if (uOtherEfoInst == uDepInst)
			{
				for (uArg = 0; uArg < psDepInst->uArgumentCount; uArg++)
				{
					if (
						(uUsedMask & (1U << uArg)) != 0 &&
						!CanUseSrc(psState, psDepInst, uArg, USEASM_REGTYPE_FPINTERNAL, USC_REGTYPE_NOINDEX)
						)
					{
						return IMG_FALSE;
					}
				}
				continue;
			}
		}

		/*
			Check if an instruction which uses the temporary register is a dependency of the other EFO.
		*/
		if (GraphGet(psState, psDepState->psClosedDepGraph, uOtherEfoInst, uDepInst))
		{
			return IMG_FALSE;
		}
		if (bOtherInstOverwritesBothIRegs)
		{
			/*
				If there are any instructions which read the internal registers written by the
				first EFO check they don't have a dependency on this instruction. If they
				do then if we use the second EFO to do a writeback they will be indirectly
				dependent on the second EFO but the second EFO will overwrite the internal
				registers written by the first.
			*/
			if (!CheckWriteAfterReadDependency(psState, psEfoState, psEfoInst, uDepInst, IMG_FALSE))
			{
				return IMG_FALSE;
			}
		}
	}
	return IMG_TRUE;
}

static IMG_VOID AddDepsForWriteDestUsingEfo(PINTERMEDIATE_STATE		psState,
											PEFOGEN_STATE			psEfoState,
											IMG_UINT32				uEfoInst,
											PARG					psDest,
											IMG_UINT32				uOtherEfoInst)
/*****************************************************************************
 FUNCTION	: AddDepsForWriteDestUsingEfo
    
 PURPOSE	: Add additional dependencies when a move from an internal register
			  is done using another EFO instruction..

 PARAMETERS	: psState			- Compiler state.
			  uEfoInst			- EFO instruction which writes the internal	
								  register.
			  psDest			- Register to write.
			  uOtherEfoInst		- EFO instruction which does the write from
								  the internal register.
			  
 RETURNS	: TRUE if the write could be done.
*****************************************************************************/
{
	IMG_UINT32				uDepInst;
	PDGRAPH_STATE			psDepState = psEfoState->psCodeBlock->psDepState;
	PADJACENCY_LIST			psList = (PADJACENCY_LIST)ArrayGet(psState, psDepState->psDepList, uEfoInst);
	ADJACENCY_LIST_ITERATOR	sIter;

	for (uDepInst = FirstAdjacent(psList, &sIter); !IsLastAdjacent(&sIter); uDepInst = NextAdjacent(&sIter))
	{
		PINST		psDepInst = ArrayGet(psState, psDepState->psInstructions, uDepInst);
		IMG_BOOL	bUsed = IMG_FALSE;
		IMG_UINT32	uArg;

		if (uDepInst == uEfoInst)
		{
			continue;
		}

		/*
			Check for an instruction which uses the destination register.
		*/
		for (uArg = 0; uArg < psDepInst->uArgumentCount; uArg++)
		{
			if (psDepInst->asArg[uArg].uType == psDest->uType &&
				psDepInst->asArg[uArg].uNumber == psDest->uNumber)
			{
				bUsed = IMG_TRUE;
				break;
			}
		}
		if (!bUsed)
		{
			continue;
		}
		if (uDepInst == uOtherEfoInst)
		{
			continue;
		}
		ASSERT(!GraphGet(psState, psDepState->psClosedDepGraph, uOtherEfoInst, uDepInst));
		/*
			Add a dependency onto the EFO instruction which now writes the register.
		*/
		AddClosedDependency(psState, psEfoState->psCodeBlock->psDepState, uOtherEfoInst, uDepInst);
	}
}

static IMG_BOOL WriteDestUsingEfo(PINTERMEDIATE_STATE psState, 
								  PEFOGEN_STATE psEfoState, 
								  PCODEBLOCK psBlock,
								  PINST psEfoInst, 
								  IMG_UINT32 uEfoDestIdx,
								  PINST psOtherEfoInst, 
								  IMG_UINT32 uIRegNum)
/*****************************************************************************
 FUNCTION	: WriteDestUsingEfo
    
 PURPOSE	: Try to do a write from an internal register using EFO
			  instruction.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  uEfoInst			- EFO instruction which writes the internal	
								  register.
			  uEfoDestIdx		- EFO destination to write to.
			  uIRegNum			- Internal register to write from.
			  
 RETURNS	: TRUE if the write could be done.
*****************************************************************************/
{
	PDGRAPH_STATE	psDepState = psEfoState->psCodeBlock->psDepState;
	IMG_UINT32		uEfoInst = psEfoInst->uId;
	PARG			psEfoDest = &psEfoInst->asDest[uEfoDestIdx];
	PARG			psNewEfoDest;
	IMG_UINT32		uOtherEfoInst = psOtherEfoInst->uId;
	IMG_UINT32		uEfoInstGroupId = psEfoInst->sStageData.psEfoData->uEfoGroupId;
	IMG_UINT32		uOtherEfoInstGroupId = psOtherEfoInst->sStageData.psEfoData->uEfoGroupId;
	IMG_UINT32		uNewArgCount;
	IMG_UINT32		uArg;

	ASSERT(uOtherEfoInst != uEfoInst);
	ASSERT(!GraphGet(psState, psDepState->psClosedDepGraph, uEfoInst, uOtherEfoInst));

	/*
		Count how many arguments will be used by the combined group.
	*/
	uNewArgCount = psEfoState->asEfoGroup[uEfoInstGroupId].uArgCount;
	if (uOtherEfoInstGroupId != uEfoInstGroupId)
	{
		uNewArgCount += psEfoState->asEfoGroup[uOtherEfoInstGroupId].uArgCount;
	}

	if (psOtherEfoInst->eOpcode != IEFO)
	{
		/*
			If the first internal register writer in the other group isn't an EFO check
			we can change it to one.
		*/
		ASSERT(psOtherEfoInst->uDestCount == 1);
		ASSERT(psOtherEfoInst->asDest[0].uType == USEASM_REGTYPE_FPINTERNAL);

		if (psOtherEfoInst->asDest[0].uNumber != uIRegNum)
		{
			return IMG_FALSE;
		}
		if (!CanConvertSingleInstToEfo(psState, psOtherEfoInst, psOtherEfoInst->asDest[0].uNumber))
		{
			return IMG_FALSE;
		}
	}
	else
	{
		/*
			The later EFO instruction isn't already using its destination.
		*/
		if (!psOtherEfoInst->u.psEfo->bIgnoreDest)
		{
			return IMG_FALSE;
		}
	}
	/*
		There isn't a descheduling instruction between the two EFOs.
	*/
	if (IsDescheduleBetweenGroups(psState, 
								  psEfoState,
								  uEfoInstGroupId, 
								  uOtherEfoInstGroupId,
								  USC_UNDEF))
	{
		return IMG_FALSE;
	}
	/*
		Check if any of the instructions which read the internal registers
		written by the first EFO are descheduling instructions. These instructions
		need to be before the second EFO (since it will overwrite the internal
		registers) but they themselves cause the values in the internal
		registers to be lost.
	*/
	if (!CheckForDeschedReaders(psState, psEfoInst))
	{
		return IMG_FALSE;
	}
	/*
		It's possible to make the instructions which use the EFO
		destination dependent on the later EFO.
	*/
	if (!CanWriteDestUsingEfo(psState, psEfoState, psEfoInst, psEfoDest, uOtherEfoInst, IMG_TRUE))
	{
		return IMG_FALSE;
	}
	/*
		We aren't creating a group which uses too many temporary
		registers (this creates problems for the register allocator)
	*/
	if (uNewArgCount > (EURASIA_USE_TEMPORARY_BANK_SIZE / 2))
	{
		return IMG_FALSE;
	}

	EFO_TESTONLY(printf("\tUsing other instruction %d's writeback for %d.\n", psOtherEfoInst->uId, psEfoInst->asDest[uEfoDestIdx].uNumber));

	if (psOtherEfoInst->eOpcode != IEFO)
	{
		/*
			Convert the other instruction to an EFO.
		*/
		ConvertSingleInstToEfo(psState, 
							   psBlock, 
							   psOtherEfoInst, 
							   psEfoInst, 
							   uEfoDestIdx, 
							   uIRegNum, 
							   (EFO_SRC)(I0 + uIRegNum));
	}
	else
	{
		/*
			Set up the other EFO instruction to write the register.
		*/
		MoveDest(psState, psOtherEfoInst, EFO_US_DEST, psEfoInst, uEfoDestIdx);
		psOtherEfoInst->u.psEfo->bIgnoreDest = IMG_FALSE;
		psOtherEfoInst->u.psEfo->eDSrc = (EFO_SRC)(I0 + uIRegNum);	
		SetupEfoUsage(psState, psOtherEfoInst);	
	}

	psNewEfoDest = &psOtherEfoInst->asDest[EFO_US_DEST];

	/*
		Replace the other EFO instruction's sources which use the register.
	*/
	for (uArg = 0; uArg < psOtherEfoInst->uArgumentCount; uArg++)
	{
		if (psOtherEfoInst->asArg[uArg].uType == psNewEfoDest->uType &&
			psOtherEfoInst->asArg[uArg].uNumber == psNewEfoDest->uNumber)
		{
			ASSERT(CanUseSrc(psState, psOtherEfoInst, uArg, USEASM_REGTYPE_FPINTERNAL, USC_REGTYPE_NOINDEX));

			SetSrc(psState, psOtherEfoInst, uArg, USEASM_REGTYPE_FPINTERNAL, uIRegNum, UF_REGFORMAT_F32);
		}
	}

	/*
		Add a dependency between the two EFO instructions.
	*/
	AddClosedDependency(psState, psEfoState->psCodeBlock->psDepState, uEfoInst, uOtherEfoInst);

	/*
		Add a dependency from all the instructions that use the register
		onto the EFO instruction which does the writeback.
	*/
	AddDepsForWriteDestUsingEfo(psState, psEfoState, uEfoInst, psNewEfoDest, uOtherEfoInst);

	/*
		Add a dependency onto any instruction which uses internal registers
		written by the first EFO from the other EFO.
	*/
	AddWriteAfterReadDependency(psState, psEfoState, psEfoInst, uOtherEfoInst);

	/*
		The two EFO instructions now have a dependency through an internal
		register so combine their groups.
	*/
	if (uEfoInstGroupId != uOtherEfoInstGroupId)
	{
		MergeEfoGroup(psState, psBlock, psEfoState, psEfoInst, uEfoInstGroupId, psOtherEfoInst, uOtherEfoInstGroupId);
	}

	return IMG_TRUE;
}

static IMG_BOOL WriteDestUsingAnotherEfo(PINTERMEDIATE_STATE psState, 
										 PEFOGEN_STATE psEfoState, 
										 PCODEBLOCK psBlock,
										 PINST psEfoInst, 
										 IMG_UINT32 uEfoDestIdx,
										 IMG_UINT32 uIRegNum)
/*****************************************************************************
 FUNCTION	: WriteDestUsingAnotherEfo
    
 PURPOSE	: Try to do a write from an internal register using another EFO
			  instruction.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  psEfoInst			- EFO instruction which writes the internal	
								  register.
			  uEfoDestIdx		- EFO instruction destination to write to.
			  uIRegNum			- Internal register to write from.
			  
 RETURNS	: TRUE if the write could be done.
*****************************************************************************/
{
	if (psEfoInst->sStageData.psEfoData->psNextWriter != NULL)
	{
		return WriteDestUsingEfo(psState, 
								 psEfoState, 
								 psBlock,
								 psEfoInst,
								 uEfoDestIdx, 
								 psEfoInst->sStageData.psEfoData->psNextWriter, 
								 uIRegNum);
	}
	else
	{
		IMG_UINT32	uEfoGroup;
		for (uEfoGroup = 0; uEfoGroup < psEfoState->uEfoGroupCount; uEfoGroup++)
		{
			PINST		psOtherEfoInst = psEfoState->asEfoGroup[uEfoGroup].psHead;

			if (uEfoGroup == psEfoInst->sStageData.psEfoData->uEfoGroupId)
			{
				continue;
			}
			if (psOtherEfoInst == NULL)
			{
				continue;
			}
			/*
				Check if this group is a dependency of the group which contains the EFO whose
				result we are trying to writeback.
			*/
			if (GetClosedDependency(psEfoState, psEfoInst->sStageData.psEfoData->uEfoGroupId, uEfoGroup))
			{
				continue;
			}
			/*
				Check for another group which is dependent on the group which containing the EFO
				whose result we are trying to writeback and a dependency of this group.
			*/
			if (IsInterveningGroup(psState, psEfoState, psEfoInst->sStageData.psEfoData->uEfoGroupId, uEfoGroup, USC_UNDEF))
			{
				continue;
			}

			if (WriteDestUsingEfo(psState, 
								  psEfoState,
								  psBlock,
								  psEfoInst, 
								  uEfoDestIdx, 
								  psOtherEfoInst, 
								  uIRegNum))
			{
				return IMG_TRUE;
			}
		}
		return IMG_FALSE;
	}
}

static IMG_BOOL CheckPackDestDependency(PARG psSrc, PINST psDest)
/*****************************************************************************
 FUNCTION	: CheckPackDestDependency
    
 PURPOSE	: Check for a dependency between a pack instruction and a register 
			  written by an earlier instruction.

 PARAMETERS	: psSrc				- Written register.
			  psDest			- Pack instruction.
			  
 RETURNS	: TRUE if a dependency exists.
*****************************************************************************/
{
	IMG_UINT32 uArg;
	if (psDest->uPredCount != 0)
	{
		IMG_UINT32 uPred;
		for (uPred = 0; uPred < psDest->uPredCount; ++uPred)
		{
			if (psDest->apsPredSrc[uPred] != NULL)
			{
				if (psSrc->uType == psDest->apsPredSrc[uPred]->uType &&
					psSrc->uNumber == psDest->apsPredSrc[uPred]->uNumber)
				{
					return IMG_TRUE;
				}
			}
		}
	}
	for (uArg = 0; uArg < psDest->uArgumentCount; uArg++)
	{
		if (psSrc->uType == psDest->asArg[uArg].uType &&
			psSrc->uNumber == psDest->asArg[uArg].uNumber)
		{
			return IMG_TRUE;
		}
		if (OverwritesIndexDest(psSrc, &psDest->asArg[uArg]))
		{
			return IMG_TRUE;
		}
	}
	if (psSrc->uType == psDest->asDest[0].uType &&
		psSrc->uNumber == psDest->asDest[0].uNumber)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_BOOL CheckPackDependency(PINST psSrc, PINST psDest)
/*****************************************************************************
 FUNCTION	: CheckPackDependency
    
 PURPOSE	: Check for a dependency between a pack instruction and an earlier instruction.

 PARAMETERS	: psSrc				- Earlier instruction.
			  psDest			- Pack instruction.
			  
 RETURNS	: TRUE if a dependency exists.
*****************************************************************************/
{
	IMG_UINT32	uDestIdx;

	for (uDestIdx = 0; uDestIdx < psSrc->uDestCount; uDestIdx++)
	{
		PARG	psSrcWrite = &psSrc->asDest[uDestIdx];

		if (CheckPackDestDependency(psSrcWrite, psDest))
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static IMG_VOID UpdatePackMask(PINTERMEDIATE_STATE	psState,
							   PINST				psPackInst,
							   IMG_UINT32			uALocation,
							   IMG_UINT32			uAMask,
							   IMG_UINT32			uBMask)
/*****************************************************************************
 FUNCTION	: UpdatePackMask
    
 PURPOSE	: Updates PCK instruction's destination mask when exchanging the
			  sources between two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psPackInst		- PCK instruction to modify.
			  uALocation		- Index of source A to the PCK instruction.
			  uAMask			- Mask of the channel in the destination to copy
								the converted value of source A to.
			  uBMask			- Mask of the channel in the destatinion to copy
								the converted value of the other source to.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uFirstSrcMask, uSecondSrcMask;

	if (uALocation == 0)
	{
		uFirstSrcMask = uAMask;
		uSecondSrcMask = uBMask;
	}
	else
	{
		uFirstSrcMask = uBMask;
		uSecondSrcMask = uAMask;
	}

	if (uFirstSrcMask > uSecondSrcMask)
	{
		SwapInstSources(psState, psPackInst, 0 /* uSrc1Idx */, 1 /* uSrc2Idx */);
	}
	psPackInst->auDestMask[0] = uAMask | uBMask;
}

typedef struct _PACK_REORDER_DATA
{
	PINST		psInst;
	IMG_UINT32	uMatchSrc;
	IMG_UINT32	uMatchMask;
	IMG_UINT32	uNonMatchMask;
} PACK_REORDER_DATA, *PPACK_REORDER_DATA;

static IMG_VOID TryReorderPacks(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PINST psEfoInst)
/*****************************************************************************
 FUNCTION	: TryReorderPacks
    
 PURPOSE	: Try swapping sources between pack instructions to get the best
			  chance of removing moves from internal registers..

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Basic block.
			  psEfoInst			- Instruction which writes the internal registers.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PDGRAPH_STATE		psDepState = psBlock->psDepState;
	IMG_UINT32			uInst;
	PACK_REORDER_DATA	asPacks[2];
	PPACK_REORDER_DATA	psFirstPack;
	PPACK_REORDER_DATA	psSecondPack;
	PARG				psFirstPackDest;
	PINST				psFirstPackUseInst;
	USEDEF_TYPE			eFirstPackUseType;
	IMG_UINT32			uFirstPackUseLocation;
	ARG					sTempArg;

	/*
		Check for exactly two instructions which are directly dependent on the efo.
	*/
	for (uInst = 0; uInst < 2; uInst++)
	{
		PARG			psUSDest = &psEfoInst->asDest[EFO_USFROMI0_DEST + uInst];
		PINST			psUseInst;
		USEDEF_TYPE		eUseType;
		IMG_UINT32		uUseIdx;

		/*
			Check there is only a single use of the EFO result.
		*/
		if (!UseDefGetSingleUse(psState, psUSDest, &psUseInst, &eUseType, &uUseIdx))
		{
			return;
		}

		/*
			Check the use is as a source argument in the current block.
		*/
		if (eUseType != USE_TYPE_SRC || psUseInst->psBlock != psBlock)
		{
			return;
		}

		/*
			Get the instruction where the EFO result is used.
		*/
		asPacks[uInst].psInst = psUseInst;

		/*
			Get the index of the source where the EFO result is used.
		*/
		asPacks[uInst].uMatchSrc = uUseIdx;
	}
	
	/*
		Check that each instruction is a pack and each uses one internal
		register result from the efo.
	*/
	if (asPacks[0].psInst == asPacks[1].psInst)
	{
		return;
	}
	for (uInst = 0; uInst < 2; uInst++)
	{
		PPACK_REORDER_DATA psPack = &asPacks[uInst];
		PINST psInst = psPack->psInst;
		PARG psOtherSrc;
		PARG psOldDest;

		if (psInst->eOpcode != IPCKU8F32 || 
			g_auSetBitCount[psInst->auDestMask[0]] != 2 || 
			!NoPredicate(psState, psInst))
		{
			return;
		}

		/*
			Get the channel in the destination which is the result of the source which
			contains the result of the EFO.
		*/
		ASSERT(psPack->uMatchSrc < PCK_SOURCE_COUNT);
		asPacks[uInst].uMatchMask = g_auSetBitMask[psInst->auDestMask[0]][psPack->uMatchSrc];
		asPacks[uInst].uNonMatchMask = psInst->auDestMask[0] & ~psPack->uMatchMask;

		/*
			Check if this PCK is already dependent on an EFO.
		*/
		psOtherSrc = &psInst->asArg[1 - psPack->uMatchSrc];
		if (psOtherSrc->uType == USEASM_REGTYPE_FPINTERNAL)
		{
			return;
		}

		psOldDest = psInst->apsOldDest[0];
		if (psOldDest != NULL && psOldDest->uType == USEASM_REGTYPE_FPINTERNAL)
		{
			return;
		}
	}

	/*
		Check the result of the first pack is used only as the partially written destination
		in the second pack.
	*/
	if (asPacks[0].psInst->uId < asPacks[1].psInst->uId)
	{
		psFirstPack = &asPacks[0];
		psSecondPack = &asPacks[1];
	}
	else
	{
		psFirstPack = &asPacks[1];
		psSecondPack = &asPacks[0];
	}

	ASSERT(psFirstPack->psInst->uDestCount == 1);
	psFirstPackDest = &psFirstPack->psInst->asDest[0];
	if (!UseDefGetSingleUse(psState, psFirstPackDest, &psFirstPackUseInst, &eFirstPackUseType, &uFirstPackUseLocation))
	{
		return;
	}
	if (eFirstPackUseType != USE_TYPE_OLDDEST || psFirstPackUseInst != psSecondPack->psInst)
	{
		return;
	}
	ASSERT(uFirstPackUseLocation == 0);

	/*
		Check the two packs are writing mutually exclusive masks of channels.
	*/
	if ((psFirstPack->psInst->auDestMask[0] & psSecondPack->psInst->auDestMask[0]) != 0)
	{
		return;
	}

	/*
		Swap the source which is the result of the EFO from the second pack to the first.
	*/
	sTempArg = psSecondPack->psInst->asArg[psSecondPack->uMatchSrc];
	MoveSrc(psState, psSecondPack->psInst, psSecondPack->uMatchSrc, psFirstPack->psInst, 1 - psFirstPack->uMatchSrc);
	SetSrcFromArg(psState, psFirstPack->psInst, 1 - psFirstPack->uMatchSrc, &sTempArg);

	/*
		Swap the channels in the destination masks of the two instructions corresponding to the
		swapped sources.
	*/
	UpdatePackMask(psState, 
				   psFirstPack->psInst, 
				   psFirstPack->uMatchSrc, 
				   psFirstPack->uMatchMask,
				   psSecondPack->uMatchMask);
	UpdatePackMask(psState, 
				   psSecondPack->psInst, 
				   1 - psSecondPack->uMatchSrc, 
				   psSecondPack->uNonMatchMask, 
				   psFirstPack->uNonMatchMask);

	/*
		Reorder destinations so the PCK instruction with internal register sources can be
		placed earlier in the block e.g.

			SECONDPACK	TEMP, A, B
			FIRSTPACK	DEST[=TEMP], I0, I1
		->
			FIRSTPACK	TEMP, I0, I1
			SECONDPACK	DEST[=TEMP], A, B
	*/
	if (psFirstPack->psInst->uId > psSecondPack->psInst->uId)
	{
		ARG			sTempDest;

		sTempDest = psFirstPack->psInst->asDest[0];

		MoveDest(psState, 
				 psFirstPack->psInst, 
				 0 /* uDestIdx */, 
				 psSecondPack->psInst, 
				 0 /* uDestIdx */);
		CopyPartiallyWrittenDest(psState, 
								 psFirstPack->psInst, 
								 0 /* uMoveToDestIdx */, 
								 psSecondPack->psInst, 
								 0 /* uMoveFromDestIdx */);

		SetDestFromArg(psState, psSecondPack->psInst, 0 /* uDestIdx */, &sTempDest);
		SetPartiallyWrittenDest(psState, psSecondPack->psInst, 0 /* uDestIdx */, &psFirstPack->psInst->asDest[0]);

		RemoveDependency(psDepState, psSecondPack->psInst->uId, psFirstPack->psInst);
	}
	
	/*
		Rearrange the dependencies between the two instructions.
	*/
	for (uInst = 0; uInst < 2; uInst++)
	{
		IMG_UINT32	uDepInst;
		IMG_UINT32	uPackInst = asPacks[uInst].psInst->uId;
		PINST		psPackInst = asPacks[uInst].psInst;

		ASSERT(ArrayGet(psState, psDepState->psSatDepCount, uPackInst) == 0);

		/*
			For each instruction recheck whether the PACK has a dependency on it.
		*/
		for (uDepInst = 0; uDepInst < psDepState->uBlockInstructionCount; uDepInst++)
		{
			PINST		psDepInst = ArrayGet(psState, psDepState->psInstructions, uDepInst);
			IMG_BOOL	bDependent;

			if (psDepInst == NULL)
			{
				continue;
			}

			if (psDepInst == asPacks[0].psInst || psDepInst == asPacks[1].psInst)
			{
				bDependent = IMG_FALSE;			
				if (psPackInst == psSecondPack->psInst && psDepInst == psFirstPack->psInst)
				{
					bDependent = IMG_TRUE;
				}
			}
			else
			{
				bDependent = CheckPackDependency(psDepInst, psPackInst);
			}

			if (bDependent)
			{
				AddDependency(psDepState, uDepInst, uPackInst);
			}
			else
			{
				RemoveDependency(psDepState, uDepInst, psPackInst);
			}
		}
	}

	/*
		Recompute the version of the dependency graph closed under transitivity.
	*/
	ComputeClosedDependencyGraph(psDepState, IMG_TRUE);
}

static IMG_BOOL IsDestDependency(PINST psInst, PARG psDest)
/*****************************************************************************
 FUNCTION	: IsDestDependency
    
 PURPOSE	: Check for a dependency from the destination of an instruction
			  onto another instruction.

 PARAMETERS	: psInst		- Instruction to check for a dependency
			  psDest		- Destination register.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uArg;
	IMG_UINT32	uDestIdx;

	for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
	{
		if (psInst->asArg[uArg].uType == psDest->uType &&
			psInst->asArg[uArg].uNumber == psDest->uNumber)
		{
			return IMG_TRUE;
		}
	}
	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		if (psInst->asDest[uDestIdx].uType == psDest->uType &&
			psInst->asDest[uDestIdx].uNumber == psDest->uNumber)
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL IsInterveningInst(PINTERMEDIATE_STATE	psState,
								  PEFOGEN_STATE			psEfoState,
								  IMG_UINT32			uInstA,
								  IMG_UINT32			uInstB)
/*****************************************************************************
 FUNCTION	: IsInterveningInst
    
 PURPOSE	: Is there any indirect dependency between two instruction.

 PARAMETERS	: psState			- Compiler state.
			  uInstA, uInstB	- Instructions to check.
			  
 RETURNS	: TRUE if there is an indirect dependency.
*****************************************************************************/
{
	IMG_UINT32		uDepInst;
	PDGRAPH_STATE	psDepState = psEfoState->psCodeBlock->psDepState;
	ASSERT(psEfoState->psCodeBlock->psDepState != NULL);

	for (uDepInst = 0; uDepInst < psDepState->uBlockInstructionCount; uDepInst++)
	{
		if (uDepInst == uInstA || uDepInst == uInstB)
		{
			continue;
		}
		if (GraphGet(psState, psDepState->psClosedDepGraph, uInstA, uDepInst) &&
			GraphGet(psState, psDepState->psClosedDepGraph, uDepInst, uInstB))
		{
			return IMG_TRUE;
		}
		if (GraphGet(psState, psDepState->psClosedDepGraph, uInstB, uDepInst) &&
			GraphGet(psState, psDepState->psClosedDepGraph, uDepInst, uInstA))
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static IMG_UINT32 CreateNewEfoGroup(PINTERMEDIATE_STATE			psState,
									PEFOGEN_STATE				psEfoState,
									IMG_BOOL					bPreEfoGroup)
/*****************************************************************************
 FUNCTION	: CreateNewEfoGroup
    
 PURPOSE	: Create a new group of instructions linked through the internal
			  registers.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  bPreEfoGroup		- Is this a group created before this module.
			  
 RETURNS	: The id of the group.
*****************************************************************************/
{
	IMG_UINT32	uEfoGroupId;

	IncreaseArraySizeInPlace(psState,
							 psEfoState->uEfoGroupCount * sizeof(psEfoState->asEfoGroup[0]),
							 (psEfoState->uEfoGroupCount + 1) * sizeof(psEfoState->asEfoGroup[0]),
							 (IMG_PVOID*)&psEfoState->asEfoGroup);

	uEfoGroupId = psEfoState->uEfoGroupCount++;
	psEfoState->asEfoGroup[uEfoGroupId].uArgCount = 0;
	psEfoState->asEfoGroup[uEfoGroupId].psHead = NULL;
	psEfoState->asEfoGroup[uEfoGroupId].psTail = NULL;
	psEfoState->asEfoGroup[uEfoGroupId].bExistingGroup = bPreEfoGroup;
	psEfoState->asEfoGroup[uEfoGroupId].uInstCount = 0;
	psEfoState->asEfoGroup[uEfoGroupId].auDeschedDependencies = NULL;

	return uEfoGroupId;
}

static IMG_VOID AddToEfoWriterList(PINTERMEDIATE_STATE			psState,
								   PEFOGEN_STATE				psEfoState,
								   IMG_UINT32					uEfoGroupId,
								   PINST						psInst)
/*****************************************************************************
 FUNCTION	: AddToEfoWriterList
    
 PURPOSE	: Add an instruction to the list of instructions in a group
			  which write the internal registers.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  uEfoGroupId		- The group to add to.
			  psInst			- The instruction to add.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psOldTail;

	if (psEfoState->asEfoGroup[uEfoGroupId].psTail != NULL &&
		!psEfoState->asEfoGroup[uEfoGroupId].bExistingGroup)
	{
		PDGRAPH_STATE	psDepState = psEfoState->psCodeBlock->psDepState;
	
		ASSERT(GraphGet(psState, psDepState->psClosedDepGraph, psInst->uId, psEfoState->asEfoGroup[uEfoGroupId].psTail->uId));
		ASSERT(GraphGet(psState, psDepState->psClosedDepGraph, psInst->uId, psEfoState->asEfoGroup[uEfoGroupId].psHead->uId));
	}

	psOldTail = psEfoState->asEfoGroup[uEfoGroupId].psTail;
	if (psOldTail != NULL)
	{
		PINST	psTailReader;

		/*
			Make the new writer dependent on all the instructions reading the internal
			registers written by the previous writer.
		*/
		for (psTailReader = psOldTail->sStageData.psEfoData->psFirstReader;
			 psTailReader != NULL;
			 psTailReader = psTailReader->sStageData.psEfoData->psNextReader)
		{
			if (psTailReader != psInst)
			{
				AddClosedDependency(psState, psEfoState->psCodeBlock->psDepState, psTailReader->uId, psInst->uId);
			}
		}

		psOldTail->sStageData.psEfoData->psNextWriter = psInst;
	}
	else
	{
		ASSERT(psEfoState->asEfoGroup[uEfoGroupId].psHead == NULL);
		psEfoState->asEfoGroup[uEfoGroupId].psHead = psInst;
	}
	psInst->sStageData.psEfoData->psPrevWriter = psOldTail;

	psEfoState->asEfoGroup[uEfoGroupId].psTail = psInst;
}

static IMG_VOID AddToMiddleEfoWriterList(PINTERMEDIATE_STATE		psState,
										 PEFOGEN_STATE				psEfoState,
										 IMG_UINT32					uEfoGroupId,
										 PINST						psInst,
										 PINST						psNextInst)
/*****************************************************************************
 FUNCTION	:  AddToMiddleEfoWriterList
    
 PURPOSE	: Add an instruction to the list of instructions in a group
			  which write the internal registers.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  uEfoGroupId		- The group to add to.
			  psInst			- The instruction to add.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		Set the next pointer for the previous instruction.
	*/
	if (psNextInst->sStageData.psEfoData->psPrevWriter == NULL)
	{
		ASSERT(psEfoState->asEfoGroup[uEfoGroupId].psHead == psNextInst);
		psEfoState->asEfoGroup[uEfoGroupId].psHead = psInst;
	}
	else
	{
		psNextInst->sStageData.psEfoData->psPrevWriter->sStageData.psEfoData->psNextWriter = psInst;
	}

	/*
		Set the previous and next pointer for the new instruction.
	*/
	psInst->sStageData.psEfoData->psPrevWriter = psNextInst->sStageData.psEfoData->psPrevWriter;
	psInst->sStageData.psEfoData->psNextWriter = psNextInst;

	/*
		Set the previous pointer for the next instruction.
	*/
	psNextInst->sStageData.psEfoData->psPrevWriter = psInst;
}

static IMG_VOID DropInst(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: DropInst
    
 PURPOSE	: Remove an instruction from its block and free it.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to drop.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	UscFree(psState, psInst->sStageData.psEfoData);
	RemoveInst(psState, psInst->psBlock, psInst);
	FreeInst(psState, psInst);
}

static IMG_BOOL TryCombineDependentInstructions(PINTERMEDIATE_STATE		psState,
												PEFOGEN_STATE			psEfoState,
												PCODEBLOCK				psBlock,
												IMG_UINT32				uInstA,
												PINST					psInstA,
												IMG_UINT32				uInstB,
												PINST					psInstB,
												IMG_UINT32				uBSrcFromADest,
												PIREG_STATUS			psIRegStatus,
												IMG_UINT32				uEfoInst)
/*****************************************************************************
 FUNCTION	: TryCombineDependentInstructions
    
 PURPOSE	: Try to combine a pair of dependent instructions.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Stage state.
			  psBlock			- Basic block.
			  uInstA, psInstA	- The two instructions to combine.
			  uInstB, psInstB
			  uBSrcFromADest	- Mask of the arguments to the second instruction which
								are the same as the result of the first instruction.
			  psIRegStatus		- Which are arguments to either instructions can be
								replaced by internal registers.
			  uEfoInst			- If any of the arguments to either instructions can
								be replaced by internal registers the EFO instructions
								which writes the internal registers.
			  
 RETURNS	: TRUE if the instructions could be combined.
*****************************************************************************/
{
	PDGRAPH_STATE	psDepState = psBlock->psDepState;
	PINST			psNewInst;
	IMG_UINT32		uNewInst, uArg;
	IMG_BOOL		bExtraAdd = IMG_FALSE;
	IMG_BOOL		bExtraSub = IMG_FALSE;
	IMG_UINT32		uExtraAddInst = USC_UNDEF;
	PINST			psExtraAddInst = IMG_NULL;
	PFN_EFO_BUILDER	pfnBuilder;

	/*
		Check if we can combine the two instructions into one efo.
	*/
	if (!CanCombineDependentsIntoEfo(psState, 
									 psEfoState,
									 psInstA, 
									 psInstB, 
									 psIRegStatus, 
									 uBSrcFromADest, 
									 &bExtraAdd,
									 &pfnBuilder))
	{	
		return IMG_FALSE;
	}

	/*
		Look for another MAD instruction which can do the I0+I1/I0-I1
		part of the calculation.
	*/
	if (bExtraAdd)
	{
		psExtraAddInst = FindMadForEfoWriteBack(psState, psEfoState, psBlock, uInstA, uInstB, psInstA->sStageData.psEfoData->uEfoGroupId);
		if (psExtraAddInst == NULL)
		{
			return IMG_FALSE;
		}
		uExtraAddInst = psExtraAddInst->uId;
	
		/*
			Convert the other MAD instruction to an EFO.
		*/
		BuildExtraAddInst(psState, psEfoState, psExtraAddInst, psInstB);
	}
	
	/*
		Actually do the combine.
	*/
	psNewInst = CreateEfoInst(psState, psBlock, uInstA, uInstB, &uNewInst, psInstA);

	pfnBuilder(psState,
			   psEfoState,
			   psInstA,
			   psInstB,
			   psIRegStatus->puASrcInIReg[0],
			   psIRegStatus->puASrcInIReg[1],
			   psIRegStatus->puBSrcInIReg[0],
			   psIRegStatus->puBSrcInIReg[1],
			   uBSrcFromADest,
			   psNewInst,
			   &bExtraSub);
	SetupEfoUsage(psState, psNewInst);

	/*
		Replace hardware constants in the EFO by secondary attributes.
	*/
	ReplaceHardwareConstants(psState, psNewInst);
	
	if (bExtraSub)
	{
		psExtraAddInst->u.psEfo->bA1LeftNeg = IMG_TRUE;
	}

	/*
		If one of the instructions we are combining was an EFO then
		replace it in the EFO structures by the new instruction.
	*/
	if (psInstA->eOpcode == IEFO)
	{
		PINST		psPrevWriter;
		IMG_UINT32	uEfoGroupId;

		/*
			Replace InstA by the new EFO instruction in the
			list of EFO writers in the group.
		*/
		psPrevWriter = psInstA->sStageData.psEfoData->psPrevWriter;
		ASSERT(psPrevWriter->sStageData.psEfoData->psNextWriter == psInstA);
		psPrevWriter->sStageData.psEfoData->psNextWriter = psNewInst;

		ReplaceInEfoReaderList(psPrevWriter, psInstA, psNewInst);
	
		uEfoGroupId = psInstA->sStageData.psEfoData->uEfoGroupId;
		ASSERT(psEfoState->asEfoGroup[uEfoGroupId].psTail == psInstA);
		psEfoState->asEfoGroup[uEfoGroupId].psTail = psNewInst;

		psEfoState->asEfoGroup[uEfoGroupId].uInstCount--;
	}

	/*
		Add the new EFO instruction to the EFO group.
	*/
	if (uExtraAddInst != USC_UNDEF)
	{
		IMG_UINT32				uDepInst;
		IMG_UINT32				uEfoGroupId;
		ADJACENCY_LIST_ITERATOR	sIter;
		PADJACENCY_LIST			psList;
	
		if (psNewInst->u.psEfo->bI0Used || psNewInst->u.psEfo->bI1Used)
		{
			uEfoGroupId = psInstA->sStageData.psEfoData->uEfoGroupId;
			psNewInst->sStageData.psEfoData->uEfoGroupId = uEfoGroupId;
		}
		else
		{
			uEfoGroupId = CreateNewEfoGroup(psState, psEfoState, IMG_FALSE);
			AddToEfoWriterList(psState, psEfoState, uEfoGroupId, psNewInst);
	
			ASSERT(psNewInst->sStageData.psEfoData->uEfoGroupId == USC_UNDEF);
			psNewInst->sStageData.psEfoData->uEfoGroupId = uEfoGroupId;
			for (uArg = 0; uArg < EFO_UNIFIEDSTORE_SOURCE_COUNT; uArg++)
			{
				if (psNewInst->asArg[uArg].uType != USC_REGTYPE_UNUSEDSOURCE)
				{
					psEfoState->asEfoGroup[uEfoGroupId].uArgCount++;
				}
			}
		}

		/*
			The EFO instruction is now part of the group.
		*/
		psEfoState->asEfoGroup[uEfoGroupId].uInstCount++;

		/*		
			The MAD instruction which does I0+I1 now has a dependency on the EFO.
		*/
		AddClosedDependency(psState, psEfoState->psCodeBlock->psDepState, uNewInst, uExtraAddInst);
	
		/*
			Anything that had a dependency on the EFO now has a dependency on the MAD
			instruction.
		*/
		psList = (PADJACENCY_LIST)ArrayGet(psState, psDepState->psDepList, uNewInst);
		for (uDepInst = FirstAdjacent(psList, &sIter); !IsLastAdjacent(&sIter); uDepInst = NextAdjacent(&sIter))
		{
			if (uDepInst != uExtraAddInst && !GraphGet(psState, psDepState->psDepGraph, uDepInst, uExtraAddInst))
			{
				PINST psDepInst = ArrayGet(psState, psDepState->psInstructions, uDepInst);
				
				if (psDepInst != NULL && 
					IsDestDependency(psDepInst, &psExtraAddInst->asDest[EFO_USFROMI1_DEST]))
				{
					AddDependency(psEfoState->psCodeBlock->psDepState, uExtraAddInst, uDepInst);
					AddClosedDependency(psState, psEfoState->psCodeBlock->psDepState, uExtraAddInst, uDepInst);
				}
			}
		}

		/*
			Add the MAD instruction to the EFO group.
		*/
		AddToEfoWriterList(psState, psEfoState, uEfoGroupId, psExtraAddInst);

		/*
			Add the MAD instruction to the list of instruction reading internal
			registers written by the new EFO.
		*/
		AddToEfoReaderList(psNewInst, psExtraAddInst);

		/*
			Flag that the internal registers written by the early EFO are
			now used.
		*/
		if (psExtraAddInst->u.psEfo->bI0Used)
		{
			psNewInst->sStageData.psEfoData->abIRegUsed[0] = IMG_TRUE;
		}
		if (psExtraAddInst->u.psEfo->bI1Used)
		{
			psNewInst->sStageData.psEfoData->abIRegUsed[1] = IMG_TRUE;
		}
	
		/*
			Count (approximately) how many registers used now used by the
			EFO group.
		*/
		ASSERT(psExtraAddInst->sStageData.psEfoData->uEfoGroupId == USC_UNDEF);
		psExtraAddInst->sStageData.psEfoData->uEfoGroupId = uEfoGroupId;
		for (uArg = 0; uArg < EFO_UNIFIEDSTORE_SOURCE_COUNT; uArg++)
		{
			if (psExtraAddInst->asArg[uArg].uType != USC_REGTYPE_UNUSEDSOURCE)
			{
				psEfoState->asEfoGroup[uEfoGroupId].uArgCount++;
			}	
		}
		psEfoState->asEfoGroup[uEfoGroupId].uInstCount++;
	}
	else if (psNewInst->u.psEfo->bI0Used || psNewInst->u.psEfo->bI1Used)
	{
		PINST psEfoInst = ArrayGet(psState, psDepState->psInstructions, uEfoInst);
		IMG_UINT32	uEfoGroupId;

		ASSERT(!psNewInst->u.psEfo->bWriteI0 && 
			   !psNewInst->u.psEfo->bWriteI1 && 
			   !psNewInst->u.psEfo->bIgnoreDest);
		ASSERT(uExtraAddInst == USC_UNDEF);

		uEfoGroupId = psEfoInst->sStageData.psEfoData->uEfoGroupId;		

		/*	
			Add to the list of instructions writing internal registers in
			the group.
		*/
		if (psNewInst->u.psEfo->bWriteI0 || psNewInst->u.psEfo->bWriteI1)
		{
			ASSERT(psEfoState->asEfoGroup[uEfoGroupId].psTail == psEfoInst);
			AddToEfoWriterList(psState, psEfoState, uEfoGroupId, psNewInst);
		}

		/*
			Add to the list of instructions reading internal registers from
			the previous EFO.
		*/
		AddToEfoReaderList(psEfoInst, psNewInst);

		/*
			Flag that the internal registers written by the earlier EFO are
			now used.
		*/
		if (psNewInst->u.psEfo->bI0Used)
		{
			psEfoInst->sStageData.psEfoData->abIRegUsed[0] = IMG_TRUE;
		}
		if (psNewInst->u.psEfo->bI1Used)
		{
			psEfoInst->sStageData.psEfoData->abIRegUsed[1] = IMG_TRUE;
		}
	
		/*
			Flag the new instruction as part of the same group as the EFO
			which writes the internal registers it uses.
		*/
		ASSERT(psNewInst->sStageData.psEfoData->uEfoGroupId == USC_UNDEF);
		psNewInst->sStageData.psEfoData->uEfoGroupId = uEfoGroupId;

		/*
			Increase the count of registers used in the EFO group.	
		*/
		for (uArg = 0; uArg < EFO_UNIFIEDSTORE_SOURCE_COUNT; uArg++)
		{
			if (psNewInst->asArg[uArg].uType != USC_REGTYPE_UNUSEDSOURCE)
			{
				psEfoState->asEfoGroup[uEfoGroupId].uArgCount++;
			}
		}

		/*
			The EFO instruction is now part of the group.
		*/
		psEfoState->asEfoGroup[uEfoGroupId].uInstCount++;
	}
	else
	{
		ASSERT(!psNewInst->u.psEfo->bWriteI0 && !psNewInst->u.psEfo->bWriteI1);
	}

	/*	
		Free the old instructions.
	*/
	DropInst(psState, psInstA);
	DropInst(psState, psInstB);

	return IMG_TRUE;
}

static IMG_BOOL GenerateEfosFromDependentInstructions(PINTERMEDIATE_STATE psState, 
													  PEFOGEN_STATE psEfoState, 
													  PCODEBLOCK psBlock,
													  PINST psInstA,
													  PINST* ppsNextInstA)
/*****************************************************************************
 FUNCTION	: GenerateEfosFromDependentInstructions
    
 PURPOSE	: Try to create EFO instructions from pair of dependent instructions.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  psBlock			- Basic block.
			  psInstA			- First instruction of the pair to try and combine.
			  
 RETURNS	: Nothing
*****************************************************************************/
{
	IMG_UINT32				uInstA = psInstA->uId;
	IMG_UINT32				uInstB;
	PARG					psAResult;
	PINST					psInstB;
	IMG_UINT32				uBSrcFromADest;
	IMG_UINT32				uStartGroup;
	IMG_UINT32				uEfoGroupIdx;
	USEDEF_TYPE				eAResultUseType;
	IMG_UINT32				uAResultUseLocation;

	/*
		Check some basic conditions on the first instruction.
	*/
	if (
			!(
				psInstA->eOpcode == IFMUL || 
				psInstA->eOpcode == IFADD || 
				psInstA->eOpcode == IFMAD || 
				psInstA->eOpcode == IEFO
			 )
	   )
	{
		return IMG_FALSE;
	}
	if (psInstA->asDest[0].uIndexType != USC_REGTYPE_NOINDEX)
	{
		return IMG_FALSE;
	}
	/*
		For an EFO check that either it doesn't write any internal registers or
		it is the last instruction in its group to write internal registers.
	*/
	if (
			psInstA->eOpcode == IEFO && 
			(
				psInstA->sStageData.psEfoData->uEfoGroupId == USC_UNDEF ||
				psEfoState->asEfoGroup[psInstA->sStageData.psEfoData->uEfoGroupId].psTail != psInstA
			)
	    )
	{
		return IMG_FALSE;
	}

	/*
		Which result of instruction A are we checking for.
	*/
	if (psInstA->eOpcode == IEFO)
	{
		psAResult = &psInstA->asDest[EFO_USFROMI0_DEST];
	}
	else
	{
		ASSERT(psInstA->uDestCount == 1);

		psAResult = &psInstA->asDest[0];
	}

	/*
		There is only one use of A's result.
	*/
	if (!UseDefGetSingleUse(psState, psAResult, &psInstB, &eAResultUseType, &uAResultUseLocation))
	{
		return IMG_FALSE;
	}

	/*
		Check A's result is used in a source.
	*/
	if (eAResultUseType != USE_TYPE_SRC)
	{
		return IMG_FALSE;
	}

	/*
		Check A's result is used in the current block.
	*/
	if (psInstB->psBlock != psEfoState->psCodeBlock)
	{
		return IMG_FALSE;
	}

	/*
		Check for an unsupported source modifier.
	*/
	if (IsAbsolute(psState, psInstB, uAResultUseLocation))
	{
		return IMG_FALSE;
	}

	uBSrcFromADest = (1U << uAResultUseLocation);
	uInstB = psInstB->uId;

	/*
		Simple checks to see if instruction B could be combined into an EFO.
	*/
	if (psInstB->uDestCount != 1 ||
		!NoPredicate(psState, psInstB) ||
		!(psInstB->eOpcode == IFMUL || 
		  psInstB->eOpcode == IFADD || 	
		  psInstB->eOpcode == IFMAD) ||
		psInstB->sStageData.psEfoData->uEfoGroupId != USC_UNDEF)
	{
		return IMG_FALSE;
	}
	if (psInstB->asDest[0].uIndexType != USC_REGTYPE_NOINDEX)
	{
		return IMG_FALSE;
	}

	/*
		Check the two instructions can be merged.
	*/
	if (IsInterveningInst(psState, psEfoState, uInstA, uInstB))
	{
		return IMG_FALSE;
	}

	if (psInstA->sStageData.psEfoData->uEfoGroupId != USC_UNDEF)
	{
		/*
			Check for another group G with
			(a) G dependent on the group InstA belongs to
			(b) InstB dependent on G
		*/
		if (!CheckEfoGroupOrder(psState, psEfoState, psInstA->sStageData.psEfoData->uEfoGroupId, uInstA, uInstB))
		{
			return IMG_FALSE;
		}
		/*
			Don't check for using internal registers results from other EFO instructions.
		*/
		uStartGroup = psEfoState->uEfoGroupCount;
	}
	else
	{
		uStartGroup = 0;
	}

	for (uEfoGroupIdx = uStartGroup; uEfoGroupIdx <= psEfoState->uEfoGroupCount; uEfoGroupIdx++)
	{
		IREG_STATUS	sIRegStatus;
		IMG_UINT32	uEfoInst;
		PINST		psNextInstB;

		memset(&sIRegStatus, 0, sizeof(sIRegStatus));

		if (uEfoGroupIdx == psEfoState->uEfoGroupCount)
		{
			/*
				Try just the instructions without any sources replaced by
				internal registers.
			*/
			uEfoInst = USC_UNDEF;
		}
		else
		{
			/*
				Check which sources could be replaced by internal registers.
			*/
			if (!CheckSrcToIReg(psState, 
							    psEfoState,
								uEfoGroupIdx,
							    uInstA, 
								psInstA,
							    uInstB, 
								psInstB,
							    &sIRegStatus,
							    &uEfoInst))
			{
				continue;
			}
		}

		/*
			Check if there is an EFO which can do the calculations of both instructions.
		*/
		psNextInstB = psInstB->psNext;
		if (TryCombineDependentInstructions(psState, 
											psEfoState,
											psBlock,
											uInstA,
											psInstA,
											uInstB,
											psInstB,
											uBSrcFromADest,
											&sIRegStatus,
											uEfoInst))
		{
			if (*ppsNextInstA == psInstB)
			{
				*ppsNextInstA = psNextInstB;
			}

			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL IsInvalidSrc0ForUnwindEfo(PINTERMEDIATE_STATE	psState,
										  PCODEBLOCK			psBlock,
										  PINST					psEfoInst,
										  EFO_SRC				eSrc)
/*****************************************************************************
 FUNCTION	: IsInvalidSrc0ForUnwindEfo
    
 PURPOSE	: Checks if an EFO argument would be valid in source 0 when the
			  EFO is unwound.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Flow control block that would contain the
								unwound instructions.
			  psEfoInst			- EFO instruction.
			  eSrc				- EFO source to check.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (eSrc == I0 || eSrc == I1)
	{
		return IMG_FALSE;
	}
	else
	{
		if (!CanUseSource0(psState, psBlock->psOwner->psFunc, &psEfoInst->asArg[eSrc - SRC0]))
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL UnwindEfoToEfo(PINTERMEDIATE_STATE	psState, 
							   PEFOGEN_STATE		psEfoState,
							   PCODEBLOCK			psBlock, 
							   PINST				psEfoInst,
							   IMG_BOOL				bCheckOnly)
/*****************************************************************************
 FUNCTION	: UnwindEfoToEfo
    
 PURPOSE	: Convert an EFO instruction back into seperate instructions where
			  the EFO writes both internal registers and unified store registers.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  psBlock			- Basic block containing the EFO.
			  psEfoInst			- EFO instruction.
			  bCheckOnly		- If TRUE just check if unwinding is possible.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32		uZeroArgType;
	IMG_UINT32		uZeroArgNum;
	IMG_UINT32		uInst;
	PEFO_PARAMETERS	psEfo = psEfoInst->u.psEfo;
	IMG_BOOL		abInstUsesOtherIReg[2];

	/*
		Check for a special case of EFO where we can write the same value
		to both a unified store register and an internal register.
	*/
	if (
			!(
				psEfo->eI0Src == M0 &&
				psEfo->eI1Src == M1
			 )
	   )
	{
		return IMG_FALSE;
	}
	if (!psEfoState->bNewEfoFeature)
	{
		if (
				!(
					psEfo->eM0Src0 == SRC0 &&
					psEfo->eM0Src1 == SRC1 &&
					psEfo->eM1Src0 == SRC0 &&
					psEfo->eM1Src1 == SRC2
				 )
		   )	
		{
			return IMG_FALSE;
		}
	}
	else
	{	
		if (
				!(
					psEfo->eM0Src0 == SRC0 &&
					psEfo->eM0Src1 == SRC2 &&
					psEfo->eM1Src0 == SRC1 &&
					psEfo->eM1Src1 == SRC2
				 )
		   )	
		{
			return IMG_FALSE;
		}
	}

	/*	
		Check if either of the new instructions we are going to
		create reference the internal register written by the
		other.
	*/
	for (uInst = 0; uInst < 2; uInst++)
	{
		IMG_UINT32	auArg[2];
		IMG_UINT32	uArg;

		if (psEfoState->bNewEfoFeature)
		{
			auArg[0] = 2U;
			auArg[1] = (uInst == 0) ? 0U : 1U;
		}
		else
		{
			auArg[0] = 0U;
			auArg[1] = (uInst == 0) ? 1U : 2U;
		}

		abInstUsesOtherIReg[uInst] = IMG_FALSE;
		for (uArg = 0; uArg < 2; uArg++)
		{
			PARG	psArg = &psEfoInst->asArg[auArg[uArg]];

			if (psArg->uType == USEASM_REGTYPE_FPINTERNAL && psArg->uNumber == (1 - uInst))
			{
				abInstUsesOtherIReg[uInst] = IMG_TRUE;
				break;
			}
		}
	}
	if (abInstUsesOtherIReg[0] && abInstUsesOtherIReg[1])
	{
		return IMG_FALSE;
	}

	/*
		Check that we can add a secondary attribute containing 0.
	*/
	if (!ReplaceHardwareConstBySecAttr(psState, EURASIA_USE_SPECIAL_CONSTANT_ZERO, IMG_FALSE, NULL, NULL))
	{
		return IMG_FALSE;
	}
	if (bCheckOnly)
	{
		return IMG_TRUE;
	}
	ReplaceHardwareConstBySecAttr(psState, 
								  EURASIA_USE_SPECIAL_CONSTANT_ZERO, 
								  IMG_FALSE, 
								  &uZeroArgType, 
								  &uZeroArgNum);

	for (uInst = 0; uInst < 2; uInst++)
	{
		PINST		psNewInst;

		psNewInst = AllocateInst(psState, psEfoInst);

		SetOpcodeAndDestCount(psState, psNewInst, IEFO, EFO_DEST_COUNT);
		SetupEfoStageData(psState, NULL, psNewInst);
		MoveDest(psState, psNewInst, 0 /* uCopyToIdx */, psEfoInst, EFO_USFROMI0_DEST + uInst /* uCopyFromIdx */);

		/*
			Write the same value to both the destination and the internal register.
		*/
		psNewInst->u.psEfo->eDSrc = A0;
		if (uInst == 0)
		{
			SetupIRegDests(psState, psNewInst, A0, EFO_SRC_UNDEF);
		}
		else
		{
			SetupIRegDests(psState, psNewInst, EFO_SRC_UNDEF, A0);
		}
		if (!psEfoState->bNewEfoFeature)
		{
			/*
				A0 = M0 + SRC2(=0)
				M0 = SRC0 * SRC1
			*/
			psNewInst->u.psEfo->eA0Src0 = M0;
			psNewInst->u.psEfo->eA0Src1 = SRC2;
			psNewInst->u.psEfo->eM0Src0 = SRC0;
			psNewInst->u.psEfo->eM0Src1 = SRC1;

			/*	
				Copy the sources from the old EFO.
			*/
			CopyEfoSource(psState, psNewInst, 0 /* uDestArgIdx */, psEfoInst, 0 /* uSrcArgIdx */);
			CopyEfoSource(psState, psNewInst, 1 /* uDestArgIdx */, psEfoInst, 1 + uInst /* uSrcArgIdx */);

			/*
				Set up SRC2 to be a secondary attribute containing 0.
			*/
			SetSrc(psState, psNewInst, 2 /* uSrcIdx */, uZeroArgType, uZeroArgNum, UF_REGFORMAT_F32);
		}
		else
		{
			/*
				A0 = M0 + SRC0(=0)
				M0 = SRC2 * SRC1
			*/
			psNewInst->u.psEfo->eA0Src0 = M0;
			psNewInst->u.psEfo->eA0Src1 = SRC0;
			psNewInst->u.psEfo->eM0Src0 = SRC2;
			psNewInst->u.psEfo->eM0Src1 = SRC1;
	
			/*	
				Copy the sources from the old EFO.
			*/
			CopyEfoSource(psState, psNewInst, 2 /* uDestArgIdx */, psEfoInst, 2 /* uSrcArgIdx */);
			CopyEfoSource(psState, psNewInst, 1 /* uDestArgIdx */, psEfoInst, uInst /* uSrcArgIdx */);

			/*
				Set up SRC0 to be a secondary attribute containing 0.
			*/
			SetSrc(psState, psNewInst, 0 /* uSrcIdx */, uZeroArgType, uZeroArgNum, UF_REGFORMAT_F32);
		}
		SetupEfoUsage(psState, psNewInst);

		/*
			If the second instruction references i0 before it was written by the
			first instruction then insert it before the first instruction.
		*/
		if (uInst == 1 && abInstUsesOtherIReg[1])
		{
			InsertInstBefore(psState, psBlock, psNewInst, psBlock->psBodyTail);
		}
		else
		{
			InsertInstBefore(psState, psBlock, psNewInst, NULL);
		}
	}

	DropInst(psState, psEfoInst);

	return IMG_TRUE;
}

static IMG_BOOL UnwindEfo(PINTERMEDIATE_STATE	psState, 
						  PEFOGEN_STATE			psEfoState,
						  PCODEBLOCK			psBlock, 
						  IMG_UINT32			uEfoInst,
						  PINST					psEfoInst, 
						  IMG_BOOL				bUpdateGraph,
						  IMG_BOOL				bCheckOnly)
/*****************************************************************************
 FUNCTION	: UnwindEfo
    
 PURPOSE	: Convert an EFO instruction back into seperate instructions.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Basic block containing the EFO.
			  psEfoInst			- EFO instruction.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PEFO_STAGEDATA	psEfoData = psEfoInst->sStageData.psEfoData;
	PEFO_PARAMETERS	psEfo = psEfoInst->u.psEfo;
	IMG_UINT32		uInst;
	IMG_UINT32		uArg;
	EFO_SRC			aaeArg[2][3];
	IMG_BOOL		abNegateArg[2][3];
	IOPCODE			aeOpcode[2];
	IMG_UINT32		auNewInst[2];
	PARG			psI0Dest = &psEfoInst->asDest[EFO_USFROMI0_DEST];
	PARG			psI1Dest = &psEfoInst->asDest[EFO_USFROMI1_DEST];
	PDGRAPH_STATE	psDepState = psEfoState->psCodeBlock->psDepState;
	ASSERT(psEfoState->psCodeBlock->psDepState != NULL);

	ASSERT(psEfo->bIgnoreDest);

	/*
		Unwinding this EFO would require two instructions.
	*/
	if (psEfo->eI0Src == A0 && 
		psEfo->eA0Src0 == M0 &&
		psEfo->eA0Src1 == M1)
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}

	/*
		Check for needing to write to both a unified store register
		and an internal register.
	*/
	if (psEfoData->abIRegUsed[0] || psEfoData->abIRegUsed[1])
	{
		ASSERT(!bUpdateGraph);
		if (!UnwindEfoToEfo(psState, psEfoState, psBlock, psEfoInst, bCheckOnly))
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
		return IMG_TRUE;
	}

	ASSERT(psI0Dest->uType != USC_REGTYPE_UNUSEDDEST);
	ASSERT(psI1Dest->uType != USC_REGTYPE_UNUSEDDEST);

	for (uInst = 0; uInst < 2; uInst++)
	{
		EFO_SRC	eISrc;

		if (uInst == 0)
		{
			eISrc = psEfo->eI0Src;
		}
		else
		{
			eISrc = psEfo->eI1Src;
		}

		if (eISrc == M0 || eISrc == M1)
		{
			EFO_SRC eMSrc0 = (eISrc == M0) ? psEfo->eM0Src0 : psEfo->eM1Src0;
			EFO_SRC eMSrc1 = (eISrc == M0) ? psEfo->eM0Src1 : psEfo->eM1Src1;

			aeOpcode[uInst] = IFMUL;

			aaeArg[uInst][0] = eMSrc0;
			abNegateArg[uInst][0] = IMG_FALSE;
			aaeArg[uInst][1] = eMSrc1;
			abNegateArg[uInst][1] = IMG_FALSE;
		}
		else
		{
			EFO_SRC eASrc0;
			EFO_SRC eASrc1;
			IMG_BOOL bNegateALeft, bNegateARight;

			if (eISrc == A0)
			{
				eASrc0 = psEfo->eA0Src0;
				eASrc1 = psEfo->eA0Src1;

				bNegateALeft = IMG_FALSE;
				bNegateARight = psEfo->bA0RightNeg;
			}
			else
			{
				eASrc0 = psEfo->eA1Src0;
				eASrc1 = psEfo->eA1Src1;

				bNegateALeft = psEfo->bA1LeftNeg;
				bNegateARight = IMG_FALSE;
			}

			if (eASrc0 != M0 && eASrc0 != M1 && eASrc1 != M0 && eASrc1 != M1)
			{
				aeOpcode[uInst] = IFADD;

				aaeArg[uInst][0] = eASrc0;
				abNegateArg[uInst][0] = bNegateALeft;
				aaeArg[uInst][1] = eASrc1;
				abNegateArg[uInst][1] = bNegateARight;
			}
			else
			{
				IMG_BOOL	bNegateMSrc;
				EFO_SRC		eMSrc0;
				EFO_SRC		eMSrc1;

				aeOpcode[uInst] = IFMAD;

				if (eASrc0 == M0 || eASrc0 == M0)
				{
					eMSrc0 = psEfo->eM0Src0;
					eMSrc1 = psEfo->eM0Src1;
				}
				else
				{
					eMSrc0 = psEfo->eM1Src0;
					eMSrc1 = psEfo->eM1Src1;
				}

				aaeArg[uInst][0] = eMSrc0;
				aaeArg[uInst][1] = eMSrc1;

				if (eASrc0 == M0 || eASrc0 == M1)
				{
					ASSERT(eASrc1 != M0 && eASrc1 != M1);

					aaeArg[uInst][2] = eASrc1;
					abNegateArg[uInst][2] = bNegateARight;
					bNegateMSrc = bNegateALeft;
				}
				else
				{
					aaeArg[uInst][2] = eASrc0;
					abNegateArg[uInst][2] = bNegateALeft;
					bNegateMSrc = bNegateARight;	
				}

				/*
					If the result of the multiplier is negated then negate one of
					the multiplier sources.
				*/
				abNegateArg[uInst][0] = IMG_FALSE;
				if (bNegateMSrc)
				{
					abNegateArg[uInst][1] = IMG_TRUE;
				}
				else
				{
					abNegateArg[uInst][1] = IMG_FALSE;
				}
			}
		}

		/*
			If internal registers aren't supported in src0 then check if we can
			swap the sources so the EFO can still be unwound.
		*/
		if (IsInvalidSrc0ForUnwindEfo(psState, psBlock, psEfoInst, aaeArg[uInst][0]))
		{
			EFO_SRC	eTemp;
			IMG_BOOL bTemp;

			if (IsInvalidSrc0ForUnwindEfo(psState, psBlock, psEfoInst, aaeArg[uInst][1]))
			{
				ASSERT(bCheckOnly);
				return IMG_FALSE;
			}

			eTemp = aaeArg[uInst][0];
			aaeArg[uInst][0] = aaeArg[uInst][1];
			aaeArg[uInst][1] = eTemp;

			bTemp = abNegateArg[uInst][0];
			abNegateArg[uInst][0] = abNegateArg[uInst][1];
			abNegateArg[uInst][1] = bTemp;
		}
	}

	if (bCheckOnly)
	{
		return IMG_TRUE;
	}

	for (uInst = 0; uInst < 2; uInst++)
	{
		PINST		psNewInst;
		IMG_BOOL	bUsesIReg = IMG_FALSE;

		psNewInst = AllocateInst(psState, psEfoInst);

		SetOpcode(psState, psNewInst, aeOpcode[uInst]);
		SetupEfoStageData(psState, NULL, psNewInst);
		MoveDest(psState, psNewInst, 0 /* uCopyToIdx */, psEfoInst, EFO_USFROMI0_DEST + uInst /* uCopyFromIdx */);
		for (uArg = 0; uArg < psNewInst->uArgumentCount; uArg++)
		{
			EFO_SRC	eSrc = aaeArg[uInst][uArg];
			switch (eSrc)
			{
				case SRC0:
				case SRC1:
				case SRC2:
				{
					CopyEfoArgToFloat(psState, psNewInst, uArg, psEfoInst, (IMG_UINT32)(eSrc - SRC0));
					if (psNewInst->asArg[uArg].uType == USEASM_REGTYPE_FPINTERNAL)
					{
						bUsesIReg = IMG_TRUE;
					}
					break;
				}
				case I0:
				case I1:
				{
					SetSrc(psState, psNewInst, uArg, USEASM_REGTYPE_FPINTERNAL, (IMG_UINT32)(eSrc - I0), UF_REGFORMAT_F32);
					bUsesIReg = IMG_TRUE;
					break;
				}
				default:
				{
					imgabort();
					break;
				}
			}
			if (abNegateArg[uInst][uArg])
			{
				InvertNegateModifier(psState, psNewInst, uArg);
			}
		}
		InsertInstBefore(psState, psBlock, psNewInst, NULL);

		if (bUpdateGraph)
		{
			/*
				Add the new instruction to the dependency graph.
			*/
			auNewInst[uInst] = psDepState->uBlockInstructionCount++;
			ArraySet(psDepState->psState, psDepState->psInstructions, 
					 auNewInst[uInst], psNewInst);
			psNewInst->uId = auNewInst[uInst];

			/*
				If the new instruction still uses internal registers then add it
				as a reader of the previous EFO's results.
			*/
			if (bUsesIReg)
			{
				PINST		psPrevWriter = psEfoInst->sStageData.psEfoData->psPrevWriter;
				IMG_UINT32	uEfoGroupId = psPrevWriter->sStageData.psEfoData->uEfoGroupId;

				ASSERT(psPrevWriter != NULL);

				psNewInst->sStageData.psEfoData->uEfoGroupId = uEfoGroupId;
				AddToEfoReaderList(psPrevWriter, psNewInst);

				psEfoState->asEfoGroup[uEfoGroupId].uInstCount++;
			}
		}
	}

	if (bUpdateGraph)
	{
		IMG_UINT32	uEfoGroupId = psEfoInst->sStageData.psEfoData->uEfoGroupId;

		if (psEfoInst->sStageData.psEfoData->psPrevWriter != NULL)
		{
			PINST	psPrevWriter = psEfoInst->sStageData.psEfoData->psPrevWriter;
			PINST*	ppsPrevReader;
			PINST	psReader;

			/*
				Remove the EFO instruction from the previous EFO instruction's list
				of readers of the result it writes to internal register.
			*/
			psReader = psPrevWriter->sStageData.psEfoData->psFirstReader;
			ppsPrevReader = &psPrevWriter->sStageData.psEfoData->psFirstReader;

			while (psReader != NULL)
			{
				if (psReader == psEfoInst)
				{
					*ppsPrevReader = psEfoInst->sStageData.psEfoData->psNextReader;
					break;
				}

				ppsPrevReader = &psReader->sStageData.psEfoData->psNextReader;
				psReader = psReader->sStageData.psEfoData->psNextReader;
			}
			ASSERT(psReader != NULL);

			ASSERT(psPrevWriter->sStageData.psEfoData->psNextWriter == psEfoInst);
			psPrevWriter->sStageData.psEfoData->psNextWriter = NULL;
		}

		/*
			Remove the EFO from its group.
		*/
		psEfoState->asEfoGroup[uEfoGroupId].uInstCount--;
		ASSERT(psEfoInst->sStageData.psEfoData->psNextWriter == NULL);
		ASSERT(psEfoState->asEfoGroup[uEfoGroupId].psTail == psEfoInst);
		psEfoState->asEfoGroup[uEfoGroupId].psTail = psEfoInst->sStageData.psEfoData->psPrevWriter;
		if (psEfoState->asEfoGroup[uEfoGroupId].psHead == psEfoInst)
		{
			IMG_UINT32	uOtherGroupId;

			ASSERT(psEfoState->asEfoGroup[uEfoGroupId].psTail == NULL);
			ASSERT(psEfoState->asEfoGroup[uEfoGroupId].uInstCount == 0);
			psEfoState->asEfoGroup[uEfoGroupId].psHead = NULL;

			/*
				Clear any dependencies other groups have on this one.
			*/
			for (uOtherGroupId = 0; uOtherGroupId < psEfoState->uEfoGroupCount; uOtherGroupId++)
			{
				if (uOtherGroupId != uEfoGroupId)
				{
					SetDependency(psEfoState, uOtherGroupId, uEfoGroupId, 0);
				}
			}

			/*
				If the group is empty then clear any dependencies.
			*/
			ClearDependencies(psEfoState, uEfoGroupId);
		}

		/*
			Replace the EFO in the dependency graph structures.
		*/
		SplitInstructions(psDepState, auNewInst, uEfoInst);
	}

	DropInst(psState, psEfoInst);

	return IMG_TRUE;
}

static IMG_BOOL TryEfoCombineWithEfo(PINTERMEDIATE_STATE	psState,
									 PEFOGEN_STATE			psEfoState,
									 PCODEBLOCK				psBlock,
									 IMG_UINT32				uInstA,
									 PINST					psInstA,
									 IMG_UINT32				uInstB,
									 PINST					psInstB,
									 PIREG_STATUS			psIRegStatus,
									 IMG_UINT32				uEfoInst)
/*****************************************************************************
 FUNCTION	: TryEfoCombineWithEfo
    
 PURPOSE	: Try to combine two instructions into an EFO given a previous
			  EFO which writes some of their arguments.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Basic block to which contains the two instructions.
			  uInstA			- First instruction.
			  psInstA
			  uInstB			- Second instruction.
			  psInstB	
			  psIRegStatus		- The arguments which can be replaced by internal
								registers.
			  uEfoInst			- The previous EFO.
			  
 RETURNS	: TRUE if the two instructions were combined.
*****************************************************************************/
{
	PINST				psNewInst;
	IMG_UINT32			uNewInst, uEfoGroupId, uArg, uSrcEfoGroupId;
	const EFO_TEMPLATE*	psEfoForm = NULL;
	PDGRAPH_STATE	psDepState = psEfoState->psCodeBlock->psDepState;
	ASSERT(psEfoState->psCodeBlock->psDepState != NULL);

	/*
		Check if we can combine the two instructions into one efo.
	*/
	if (!CanCombineIntoEfo(psState, 
						   psInstA, 
						   psInstB, 
						   psIRegStatus,
						   &psEfoForm))
	{
		return IMG_FALSE;
	}

	/*
		Check we aren't going to overwrite another use of the internal registers.
	*/
	if (WouldBeInterveningIRegWrite(psState, psEfoState, uInstA, uInstB))
	{
		return IMG_FALSE;
	}
	if (psEfoForm->bIRegSources)
	{
		PINST 	psEfoInst = ArrayGet(psState, psDepState->psInstructions, uEfoInst);

		uSrcEfoGroupId = psEfoInst->sStageData.psEfoData->uEfoGroupId;
	}
	else
	{
		uSrcEfoGroupId = USC_UNDEF;
	}
	if (!CheckEfoGroupDependency(psState, psEfoState, uInstA, uInstB, uSrcEfoGroupId))
	{
		return IMG_FALSE;
	}

	/*
		Actually do the combine.
	*/
	psNewInst = CreateEfoInst(psState, psBlock, uInstA, uInstB, &uNewInst, psInstA);

	CombineIntoEfo(psState, 
				   psEfoForm, 
				   psInstA, 
				   psInstB, 
				   psIRegStatus, 
				   psNewInst);
	SetupEfoUsage(psState, psNewInst);

	/*
		Create a dummy destination.
	*/
	if (psNewInst->u.psEfo->bIgnoreDest)
	{
		SetDest(psState, psNewInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, GetNextRegister(psState), UF_REGFORMAT_F32);
	}

	/*
		Drop old instructions.
	*/
	DropInst(psState, psInstB);
	DropInst(psState, psInstA);

	/*
		Add the new EFO instruction to the EFO group.
	*/
	if (psNewInst->u.psEfo->bI0Used || psNewInst->u.psEfo->bI1Used)
	{
		PINST 	psEfoInst = ArrayGet(psState, psDepState->psInstructions, uEfoInst);

		uEfoGroupId = psEfoInst->sStageData.psEfoData->uEfoGroupId;		
		ASSERT(psEfoState->asEfoGroup[uEfoGroupId].psTail == psEfoInst);

		AddToEfoReaderList(psEfoInst, psNewInst);

		if (psNewInst->u.psEfo->bI0Used)
		{
			psEfoInst->sStageData.psEfoData->abIRegUsed[0] = IMG_TRUE;
		}
		if (psNewInst->u.psEfo->bI1Used)
		{
			psEfoInst->sStageData.psEfoData->abIRegUsed[1] = IMG_TRUE;
		}
	}
	else
	{
		uEfoGroupId = CreateNewEfoGroup(psState, psEfoState, IMG_FALSE);
	}
	AddToEfoWriterList(psState, psEfoState, uEfoGroupId, psNewInst);
	ASSERT(psNewInst->sStageData.psEfoData->uEfoGroupId == USC_UNDEF);
	psNewInst->sStageData.psEfoData->uEfoGroupId = uEfoGroupId;
	for (uArg = 0; uArg < EFO_UNIFIEDSTORE_SOURCE_COUNT; uArg++)
	{
		if (psNewInst->asArg[uArg].uType != USC_REGTYPE_UNUSEDSOURCE)
		{
			psEfoState->asEfoGroup[uEfoGroupId].uArgCount++;
		}
	}

	psEfoState->asEfoGroup[uEfoGroupId].uInstCount++;

	/*
		Replace hardware constants in the EFO by secondary attributes.
	*/
	ReplaceHardwareConstants(psState, psNewInst);

	return IMG_TRUE;
}

static IMG_BOOL TryEfoCombine(PINTERMEDIATE_STATE	psState,
							  PEFOGEN_STATE			psEfoState,
							  PCODEBLOCK			psBlock,
							  PINST					psInstA,
							  PINST					psInstB)
/*****************************************************************************
 FUNCTION	: TryEfoCombine
    
 PURPOSE	: Try to combine two instructions into an EFO.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Stage state.
			  psBlock			- Basic block to which contains the two instructions.
			  psInstA			- First instruction.
			  psInstB			- Second instruction.
			  
 RETURNS	: TRUE if the two instructions were combined.
*****************************************************************************/
{
	IMG_UINT32		uEfoInst;
	PDGRAPH_STATE	psDepState = psEfoState->psCodeBlock->psDepState;
	IMG_UINT32		uEfoGroupIdx;
	IMG_UINT32		uInstA = psInstA->uId;
	IMG_UINT32		uInstB = psInstB->uId;
	
	ASSERT(psEfoState->psCodeBlock->psDepState != NULL);

	/*
		Check some basic conditions on the second instruction and that neither
		instructions is dependent on the other.
	*/
	if (psInstB->sStageData.psEfoData->uEfoGroupId != USC_UNDEF ||
		GraphGet(psState, psDepState->psClosedDepGraph, uInstA, uInstB) ||
		GraphGet(psState, psDepState->psClosedDepGraph, uInstB, uInstA) ||
		psInstB->uDestCount != 1 ||
		!NoPredicate(psState, psInstB) ||
		!(TestInstructionGroup(psInstB->eOpcode, USC_OPT_GROUP_EFO_FORMATION)))
	{
		return IMG_FALSE;
	}
	if (psInstB->asDest[0].uIndexType != USC_REGTYPE_NOINDEX)
	{
		return IMG_FALSE;
	}

	for (uEfoGroupIdx = 0; uEfoGroupIdx <= psEfoState->uEfoGroupCount; uEfoGroupIdx++)
	{
		IREG_STATUS		sIRegStatus;

		memset(&sIRegStatus, 0, sizeof(sIRegStatus));

		if (uEfoGroupIdx == psEfoState->uEfoGroupCount)
		{
			/*
				Try just the instructions without any sources replaced by
				internal registers.
			*/
			uEfoInst = USC_UNDEF;
		}
		else
		{
			/*
				Check which sources could be replaced by internal registers written by
				an instruction in the current group.
			*/	
			if (!CheckSrcToIReg(psState, 
								psEfoState,
								uEfoGroupIdx,
							    uInstA,
								psInstA,
								uInstB, 
								psInstB,
							    &sIRegStatus,
								&uEfoInst))
			{
				continue;
			}
		}

		/*
			Try combining the two instructions.
		*/
		if (TryEfoCombineWithEfo(psState, psEfoState, psBlock, uInstA, psInstA, uInstB, psInstB, &sIRegStatus, uEfoInst))
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL ConvertM0ToA0(PINTERMEDIATE_STATE	psState, 
							  PEFOGEN_STATE			psEfoState,
							  PINST					psEfoInst, 
							  EFO_SRC*				peIRegSrc)
/*****************************************************************************
 FUNCTION	: ConvertM0ToA0
    
 PURPOSE	: Try and convert an EFO writing M0/M1 to one writing A0/M1.

 PARAMETERS	: psState			- Compiler state.
			  psEfoInst			- EFO instruction.
			  peIRegSrc			- Updated with the new internal register source.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (
			psEfoInst->u.psEfo->eI0Src == M0 && psEfoInst->u.psEfo->eI1Src == M1 &&
			ReplaceHardwareConstBySecAttr(psState, EURASIA_USE_SPECIAL_CONSTANT_ZERO, IMG_FALSE, NULL, NULL) &&
			(
				(
					!psEfoState->bNewEfoFeature &&
					psEfoInst->u.psEfo->eM0Src0 == SRC1 &&
					psEfoInst->u.psEfo->eM0Src1 == I0 &&
					psEfoInst->u.psEfo->eM1Src0 == SRC0 &&
					psEfoInst->u.psEfo->eM1Src1 == I1
				) ||
				(
					psEfoState->bNewEfoFeature &&
					psEfoInst->u.psEfo->eM0Src0 == SRC1 &&
					psEfoInst->u.psEfo->eM0Src1 == I0 &&
					psEfoInst->u.psEfo->eM1Src0 == SRC2 &&
					psEfoInst->u.psEfo->eM1Src1 == I1
				)
			)
		)
	{
		IMG_UINT32	uZeroArgType;
		IMG_UINT32	uZeroArgNum;

		/*
			Add a new secondary attribute with the value 0.
		*/
		ReplaceHardwareConstBySecAttr(psState, 
									  EURASIA_USE_SPECIAL_CONSTANT_ZERO, 
									  IMG_FALSE, 
									  &uZeroArgType,
									  &uZeroArgNum);

		/*
			Add zero to the M0 result so we can use the A0 writeback.
		*/
		if (psEfoState->bNewEfoFeature)
		{
			psEfoInst->u.psEfo->eA0Src0 = M0;
			psEfoInst->u.psEfo->eA0Src1 = SRC0;

			SetSrc(psState, psEfoInst, 0 /* uSrcIdx */, uZeroArgType, uZeroArgNum, UF_REGFORMAT_F32);
		}
		else
		{
			psEfoInst->u.psEfo->eA0Src0 = M0;
			psEfoInst->u.psEfo->eA0Src1 = SRC2;

			SetSrc(psState, psEfoInst, 2 /* uSrcIdx */, uZeroArgType, uZeroArgNum, UF_REGFORMAT_F32);
		}

		psEfoInst->u.psEfo->eI0Src = A0;
		*peIRegSrc = A0;

		SetupEfoUsage(psState, psEfoInst);

		return IMG_TRUE;
	}

	return IMG_FALSE;
}

#define IREG_MASK		((1 << NUM_INTERNAL_EFO_REGISTERS) - 1)

static IMG_VOID CreateExistingIRegGroups(PINTERMEDIATE_STATE		psState,
										 PCODEBLOCK					psBlock,
										 PEFOGEN_STATE				psEfoState)
/*****************************************************************************
 FUNCTION	: CreateExistingIRegGroups
    
 PURPOSE	: Get information about existing uses of the internal registers in
			  a block.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Basic block.
			  psEfoState		- Module state.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY		apsListEntry[NUM_INTERNAL_EFO_REGISTERS];
	IMG_UINT32			uIRegIdx;
	IMG_UINT32			uIRegsLive;
	IMG_UINT32			uPreEfoGroupId;
	PINST				psLastWriter;

	/*
		Store the first reference to each internal register in the current block.
	*/
	for (uIRegIdx = 0; uIRegIdx < NUM_INTERNAL_EFO_REGISTERS; uIRegIdx++)
	{
		if (psBlock->apsIRegUseDef[uIRegIdx] != NULL)
		{
			apsListEntry[uIRegIdx] = psBlock->apsIRegUseDef[uIRegIdx]->sList.psHead;
		}
		else
		{
			apsListEntry[uIRegIdx] = NULL;
		}
	}

	uIRegsLive = 0;
	uPreEfoGroupId = USC_UNDEF;
	psLastWriter = NULL;
	for (;;)
	{
		PINST		psCurrentInst;
		IMG_UINT32	uIRegsWritten;
		IMG_UINT32	uIRegsRead;
		IMG_UINT32	uIRegsUsed;

		/*
			Find the earliest remaining reference to an internal register in the block.
		*/
		psCurrentInst = NULL;
		for (uIRegIdx = 0; uIRegIdx < NUM_INTERNAL_EFO_REGISTERS; uIRegIdx++)
		{
			PUSEDEF		psIRegUseDef;

			if (apsListEntry[uIRegIdx] == NULL)
			{
				continue;
			}

			psIRegUseDef = IMG_CONTAINING_RECORD(apsListEntry[uIRegIdx], PUSEDEF, sListEntry);
			ASSERT(psIRegUseDef->eType == DEF_TYPE_INST || 
				   psIRegUseDef->eType == USE_TYPE_SRC || 
				   psIRegUseDef->eType == USE_TYPE_OLDDEST);

			if (psCurrentInst == NULL || psCurrentInst->uBlockIndex > psIRegUseDef->u.psInst->uBlockIndex)
			{
				psCurrentInst = psIRegUseDef->u.psInst;
			}
		}

		/*
			No more references to internal registers.
		*/
		if (psCurrentInst == NULL)
		{
			break;
		}

		/*
			Find which internal registers are read or written by the current instruction.
		*/
		uIRegsWritten = 0;
		uIRegsRead = 0;
		for (uIRegIdx = 0; uIRegIdx < NUM_INTERNAL_EFO_REGISTERS; uIRegIdx++)
		{
			while (apsListEntry[uIRegIdx] != NULL)
			{
				PUSEDEF		psIRegUseDef;

				psIRegUseDef = IMG_CONTAINING_RECORD(apsListEntry[uIRegIdx], PUSEDEF, sListEntry);
				ASSERT(psIRegUseDef->eType == DEF_TYPE_INST || 
					   psIRegUseDef->eType == USE_TYPE_SRC || 
					   psIRegUseDef->eType == USE_TYPE_OLDDEST);

				if (psIRegUseDef->u.psInst != psCurrentInst)
				{
					break;
				}

				if (psIRegUseDef->eType == DEF_TYPE_INST)
				{
					uIRegsWritten |= (1U << uIRegIdx);
				}
				else
				{
					uIRegsRead |= (1U << uIRegIdx);
				}

				apsListEntry[uIRegIdx] = apsListEntry[uIRegIdx]->psNext;
			}
		}
		uIRegsUsed = uIRegsRead | uIRegsWritten;

		if ((uIRegsLive & IREG_MASK) == 0 && uIRegsWritten != 0)
		{
			/*
				This is a new use of the internal registers so create a 
				new group.
			*/
			uPreEfoGroupId = CreateNewEfoGroup(psState, psEfoState, IMG_TRUE);
		}
		else if (uIRegsUsed != 0)
		{
			ASSERT(uPreEfoGroupId != USC_UNDEF);
			ASSERT(psLastWriter != NULL);

			/*
				Add to the reader list of the last instruction.
			*/
			AddToEfoReaderList(psLastWriter, psCurrentInst);
		}

		/*
			Add to the internal register group.
		*/
		if (uIRegsUsed != 0)
		{
			AddToEfoWriterList(psState, psEfoState, uPreEfoGroupId, psCurrentInst); 

			ASSERT(psCurrentInst->sStageData.psEfoData->uEfoGroupId == USC_UNDEF);
			psCurrentInst->sStageData.psEfoData->uEfoGroupId = uPreEfoGroupId;
	
			psEfoState->asEfoGroup[uPreEfoGroupId].uInstCount++;
		
			psLastWriter = psCurrentInst;
		}

		/*
			Update the mask of internal registers currently live.
		*/
		uIRegsLive |= uIRegsWritten;
		for (uIRegIdx = 0; uIRegIdx < NUM_INTERNAL_EFO_REGISTERS; uIRegIdx++)
		{
			if (!UseDefIsNextReferenceAsUse(psState, apsListEntry[uIRegIdx]))
			{
				uIRegsLive &= ~(1U << uIRegIdx);
			}
		}
	}
}

static IMG_VOID FinishDeschedInst(PINTERMEDIATE_STATE	psState,
								  PEFOGEN_STATE			psEfoState,
								  IMG_UINT32			uDeschedInst)
/*****************************************************************************
 FUNCTION	: FinishDeschedInst
    
 PURPOSE	: Update the state after selecting a descheduling instruction as
			  the next one in the block.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  uDeschedInst		- Index of the descheduling instruction.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uGroup;

	for (uGroup = 0; uGroup < psEfoState->uEfoGroupCount; uGroup++)
	{
		if (GetBit(psEfoState->asEfoGroup[uGroup].auDeschedDependencies, uDeschedInst))
		{
			ASSERT(psEfoState->asEfoGroup[uGroup].uDeschedDependencyCount > 0);
			psEfoState->asEfoGroup[uGroup].uDeschedDependencyCount--;
		}
	}
}

static IMG_VOID FinishGroup(PINTERMEDIATE_STATE		psState,
							PEFOGEN_STATE			psEfoState,
							IMG_UINT32				uFinishedGroup)
/*****************************************************************************
 FUNCTION	: FinishGroupInst
    
 PURPOSE	: Update the state after selecting a group of internal register
			  using instructions as the next ones in the block.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  uFinishedGroup	
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uOtherGroup;

	for (uOtherGroup = 0; uOtherGroup < psEfoState->uEfoGroupCount; uOtherGroup++)
	{
		if (GetDependency(psEfoState, uOtherGroup, uFinishedGroup))
		{
			PEFO_GROUP_DATA	psOtherGroupData = &psEfoState->asEfoGroup[uOtherGroup];

			ASSERT(psOtherGroupData->uGroupSatisfiedDependencyCount < psOtherGroupData->uGroupDependencyCount);
			psOtherGroupData->uGroupSatisfiedDependencyCount++;
		}
	}
	psEfoState->asEfoGroup[uFinishedGroup].uGroupSatisfiedDependencyCount = USC_UNDEF;
}

static IMG_BOOL CanStartGroup(PINTERMEDIATE_STATE		psState,
							  PEFOGEN_STATE				psEfoState,
							  IMG_UINT32				uEfoGroupId,
							  IMG_BOOL					bIgnoreDesched)
/*****************************************************************************
 FUNCTION	: CanStartGroup
    
 PURPOSE	: Check if the conditions for selecting an instruction which
			  writes the internal registers are satisfied.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  uEfoGroupId		- Group to check for.
			  bIgnoreDesched	- If TRUE ignore restrictions on placing
								descheduling instructions where the
								internal registers are live.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PEFO_GROUP_DATA	psGroupData = &psEfoState->asEfoGroup[uEfoGroupId];

	PVR_UNREFERENCED_PARAMETER(psState);
	#if defined(DEBUG)
	ASSERT(psGroupData->uGroupSatisfiedDependencyCount != USC_UNDEF);
	{
		IMG_UINT32	uGroupIdx;
		IMG_UINT32	uCount;
		IMG_UINT32	uSatisfiedCount;

		uCount = 0;
		uSatisfiedCount = 0;
		for (uGroupIdx = 0; uGroupIdx < psEfoState->uEfoGroupCount; uGroupIdx++)
		{
			PEFO_GROUP_DATA	psOtherGroupData = &psEfoState->asEfoGroup[uGroupIdx];

			if (GetDependency(psEfoState, uEfoGroupId, uGroupIdx))
			{
				uCount++;
				if (psOtherGroupData->uGroupSatisfiedDependencyCount == USC_UNDEF)
				{
					uSatisfiedCount++;
				}
			}
		}
		ASSERT(uCount == psEfoState->asEfoGroup[uEfoGroupId].uGroupDependencyCount);
		ASSERT(uSatisfiedCount == psEfoState->asEfoGroup[uEfoGroupId].uGroupSatisfiedDependencyCount);
	}
	#endif /* defined(DEBUG) */

	/*
		Don't start a group until all other groups which contain instructions that are needed
		by instructions in the group have been output.
	*/
	if (psGroupData->uGroupSatisfiedDependencyCount < psGroupData->uGroupDependencyCount)
	{
		return IMG_FALSE;
	}
	/*
		Don't start a group until all descheduling instructions which are needed by instructions
		in the group have been output.
	*/
	if (!bIgnoreDesched && psEfoState->asEfoGroup[uEfoGroupId].uDeschedDependencyCount > 0)
	{
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

static IMG_VOID ExpandInternalRegistersMoves(PINTERMEDIATE_STATE	psState,
											 PEFOGEN_STATE			psEfoState,
											 PCODEBLOCK				psBlock,
											 PINST					psInst)
/*****************************************************************************
 FUNCTION	: ExpandInternalRegistersMove
    
 PURPOSE	: Expand the implicit moves to unified store registers from 
			  internal registers associated with an EFO instruction.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  psBlock			- Block containing the instruction.
			  psInst			- Instruction.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uDest;
	PARG		apsIRegDest[2];

	apsIRegDest[0] = &psInst->asDest[EFO_USFROMI0_DEST];
	apsIRegDest[1] = &psInst->asDest[EFO_USFROMI1_DEST];

	/*
		Check for an EFO we could split back up into its constituent
		instructions to avoid the moves from the internal registers
		into the original destinations.
	*/
	if (apsIRegDest[0]->uType != USC_REGTYPE_UNUSEDDEST &&
		apsIRegDest[1]->uType != USC_REGTYPE_UNUSEDDEST &&
		psInst->u.psEfo->bIgnoreDest &&
		UnwindEfo(psState, psEfoState, psBlock, USC_UNDEF, psInst, IMG_FALSE, IMG_TRUE))
	{
		UnwindEfo(psState, psEfoState, psBlock, USC_UNDEF, psInst, IMG_FALSE, IMG_FALSE);
		return;
	}
	
	/*	
		Generate:
			MOV		DEST, iN
	*/
	for (uDest = 0; uDest < 2; uDest++)
	{
		PARG	psEfoDest = apsIRegDest[uDest];

		if (psEfoDest->uType != USC_REGTYPE_UNUSEDDEST)
		{
			PINST	psMoveInst;

			psMoveInst = AllocateInst(psState, psInst);

			SetOpcode(psState, psMoveInst, IMOV);
			MoveDest(psState, psMoveInst, 0 /* uCopyToIdx */, psInst, EFO_USFROMI0_DEST + uDest /* uCopyFromIdx */);
			SetSrc(psState, psMoveInst, 0 /* uSrcIdx */, USEASM_REGTYPE_FPINTERNAL, uDest, UF_REGFORMAT_F32);

			InsertInstBefore(psState, psBlock, psMoveInst, NULL);

			/*
				Flag this destination as unwritten.
			*/
			SetDestUnused(psState, psInst, EFO_USFROMI0_DEST + uDest);
		}
	}
}

static IMG_VOID FinishGroupDependencyGraph(PINTERMEDIATE_STATE psState, PEFOGEN_STATE psEfoState, IMG_BOOL bIgnoreDesched)
/*****************************************************************************
 FUNCTION	: FinishGroupDependencyGraph
    
 PURPOSE	: Free memory allocated by SetupGroupDependencyGraph which isn't
			  needed after OutputInstructionsInGroupOrder finishes.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  bIgnoreDesched	- If TRUE ignore restrictions on placing
								descheduling instructions where the internal
								registers are live.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (!bIgnoreDesched)
	{
		IMG_UINT32	uEfoGroup;

		for (uEfoGroup = 0; uEfoGroup < psEfoState->uEfoGroupCount; uEfoGroup++)
		{
			UscFree(psState, psEfoState->asEfoGroup[uEfoGroup].auDeschedDependencies);
		}
	}
}

static IMG_VOID SetupDeschedDependencyState(PINTERMEDIATE_STATE	psState, PEFOGEN_STATE psEfoState)
/*****************************************************************************
 FUNCTION	: SetupDeschedDependencyState
    
 PURPOSE	: Set up information about which EFO groups depend on descheduling
			  instructions.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PDGRAPH_STATE	psDepState = psEfoState->psCodeBlock->psDepState;
	PINST			psDeschedInst;
	IMG_UINT32		uEfoGroup;

	/*
		Allocate storage for recording the dependencies on descheduling instructions for
		each group.
	*/
	for (uEfoGroup = 0; uEfoGroup < psEfoState->uEfoGroupCount; uEfoGroup++)
	{
		psEfoState->asEfoGroup[uEfoGroup].auDeschedDependencies =
			UscAlloc(psState, UINTS_TO_SPAN_BITS(psDepState->uBlockInstructionCount) * sizeof(IMG_UINT32));
		memset(psEfoState->asEfoGroup[uEfoGroup].auDeschedDependencies,
			   0,
			   UINTS_TO_SPAN_BITS(psDepState->uBlockInstructionCount) * sizeof(IMG_UINT32));

		psEfoState->asEfoGroup[uEfoGroup].uDeschedDependencyCount = 0;
	}

	/*
		For each instruction which causes a deschedule set the corresponding entry in the
		bit array for each group which depends on the descheduling instruction.
	*/
	for (psDeschedInst = psEfoState->psDeschedInstListHead;
		 psDeschedInst != NULL;
		 psDeschedInst = psDeschedInst->sStageData.psEfoData->psNextDeschedInst)
	{
		IMG_UINT32	uDeschedInst = psDeschedInst->uId;

		for (uEfoGroup = 0; uEfoGroup < psEfoState->uEfoGroupCount; uEfoGroup++)
		{
			if (IsGroupDependentOnInst(psState, psEfoState, uDeschedInst, uEfoGroup))
			{
				ASSERT(!IsInstDependentOnGroup(psState, psEfoState, uDeschedInst, uEfoGroup));

				SetBit(psEfoState->asEfoGroup[uEfoGroup].auDeschedDependencies,
					   uDeschedInst,
					   1);
				psEfoState->asEfoGroup[uEfoGroup].uDeschedDependencyCount++;
			}
		}
	}
}

static IMG_VOID SetupGroupDependencyGraph(PINTERMEDIATE_STATE psState, PEFOGEN_STATE psEfoState)
/*****************************************************************************
 FUNCTION	: SetupGroupDependencyGraph
    
 PURPOSE	: Set up information about which EFO groups depend on each other.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uEfoGroup1, uEfoGroup2;
	IMG_UINT32	uGraphStride = UINTS_TO_SPAN_BITS(psEfoState->uEfoGroupCount);

	/*
		Allocate storage.
	*/
	psEfoState->aauEfoDependencyGraph = 
			UscAlloc(psState,
					 uGraphStride * psEfoState->uEfoGroupCount * sizeof(IMG_UINT32));
	memset(psEfoState->aauEfoDependencyGraph, 
		   0, 
		   uGraphStride * psEfoState->uEfoGroupCount * sizeof(IMG_UINT32));

	psEfoState->aauClosedEfoDependencyGraph = 
			UscAlloc(psState,
					 uGraphStride * psEfoState->uEfoGroupCount * sizeof(IMG_UINT32));
	memset(psEfoState->aauClosedEfoDependencyGraph, 
		   0, 
		   uGraphStride * psEfoState->uEfoGroupCount * sizeof(IMG_UINT32));

	/*
		Get the constraints on the execution order of groups of EFO instructions.
	*/
	for (uEfoGroup1 = 0; uEfoGroup1 < psEfoState->uEfoGroupCount; uEfoGroup1++)
	{
		psEfoState->asEfoGroup[uEfoGroup1].uGroupDependencyCount = 0;
	}
	for (uEfoGroup1 = 0; uEfoGroup1 < psEfoState->uEfoGroupCount; uEfoGroup1++)
	{
		for (uEfoGroup2 = 0; uEfoGroup2 < psEfoState->uEfoGroupCount; uEfoGroup2++)
		{
			if (uEfoGroup1 != uEfoGroup2 && IsGroupDependentOnGroup(psState, psEfoState, uEfoGroup1, uEfoGroup2))
			{
				SetDependency(psEfoState, uEfoGroup2, uEfoGroup1, 1); 
			}
		}
	}

	/*
		Generate the closed under transitivity EFO dependency graph.
	*/
	for (uEfoGroup1 = 0; uEfoGroup1 < psEfoState->uEfoGroupCount; uEfoGroup1++)
	{
		/* Start with the non-transistive dependencies. */
		memcpy(psEfoState->aauClosedEfoDependencyGraph + uEfoGroup1 * uGraphStride,
			   psEfoState->aauEfoDependencyGraph + uEfoGroup1 * uGraphStride,
			   uGraphStride * sizeof(IMG_UINT32));
	}
	for (uEfoGroup1 = 0; uEfoGroup1 < psEfoState->uEfoGroupCount; uEfoGroup1++)
	{
		/* Add in the closed dependencies for dependencies of this group. */
		for (uEfoGroup2 = 0; uEfoGroup2 < psEfoState->uEfoGroupCount; uEfoGroup2++)
		{
			if (GetDependency(psEfoState, uEfoGroup1, uEfoGroup2))
			{
				IMG_UINT32	uDepend;
				for (uDepend = 0; 
					 uDepend < UINTS_TO_SPAN_BITS(psEfoState->uEfoGroupCount); 
					 uDepend++)
				{
					psEfoState->aauClosedEfoDependencyGraph[uEfoGroup1 * uGraphStride + uDepend] |= 
						psEfoState->aauClosedEfoDependencyGraph[uEfoGroup2 * uGraphStride + uDepend];
				}
			}
		}
	}
}

static IMG_VOID OutputInstructionsInGroupOrder(PINTERMEDIATE_STATE	psState,
											   PEFOGEN_STATE		psEfoState,
											   PCODEBLOCK			psBlock,
											   IMG_BOOL				bIgnoreDesched,
											   IMG_BOOL				bExpandIRegMoves)
/*****************************************************************************
 FUNCTION	: OutputInstructionsInGroupOrder
    
 PURPOSE	: .

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  psBlock			- Block.
			  bIgnoreDesched	- If TRUE ignore restrictions on placing
								descheduling instructions where the internal
								registers are live.
			  bExpandIRegsMove	- If TRUE expand any implicit moves associated
								with EFO instructions.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PDGRAPH_STATE	psDepState = psEfoState->psCodeBlock->psDepState;
	IMG_UINT32		uCurrentGroup;
	IMG_UINT32		uCountInCurrentGroup;
	IMG_UINT32		uGroupIdx;

	/*
		Clear dependency information.
	*/
	for (uGroupIdx = 0; uGroupIdx < psEfoState->uEfoGroupCount; uGroupIdx++)
	{
		psEfoState->asEfoGroup[uGroupIdx].uGroupSatisfiedDependencyCount = 0;
	}

	/*
		Set up information about which groups are dependent on descheduling
		instructions.
	*/
	if (!bIgnoreDesched)
	{
		SetupDeschedDependencyState(psState, psEfoState);
	}

	uCurrentGroup = USC_UNDEF;
	uCountInCurrentGroup = 0;
	ClearBlockInsts(psState, psBlock);
	while (psDepState->uAvailInstCount > 0)
	{
		IMG_UINT32	uInst;
		PINST		psInst;

		uInst = USC_UNDEF;
		for (psInst = GetNextAvailable(psDepState, NULL); 
			 psInst != NULL; 
			 psInst = GetNextAvailable(psDepState, psInst))
		{
			uInst = psInst->uId;

			if (uCurrentGroup != USC_UNDEF)
			{
				/*
					Don't select any instructions from a different group.
				*/
				if (psInst->sStageData.psEfoData->uEfoGroupId != USC_UNDEF &&
					psInst->sStageData.psEfoData->uEfoGroupId != uCurrentGroup)
				{
					continue;
				}
				if (!bIgnoreDesched)
				{
					/*
						Don't select a descheduling instruction.
					*/
					if (
							IsDeschedBeforeInst(psState, psInst) || 
							(
								psInst->sStageData.psEfoData->uEfoGroupId == USC_UNDEF &&
								IsDeschedAfterInst(psInst)
							)
					   )
					{
						continue;
					}
					/*
						Make a descheduling instruction in a group the last in the group.
					*/
					if (IsDeschedAfterInst(psInst) && uCountInCurrentGroup != 1)
					{
						continue;
					}
				}
			}
			else
			{
				/*
					Don't output an instruction which starts a group until all the descheduling
					instructions and other groups that the group depends on have been output.
				*/
				if (psInst->sStageData.psEfoData->uEfoGroupId != USC_UNDEF &&
					!CanStartGroup(psState, psEfoState, psInst->sStageData.psEfoData->uEfoGroupId, bIgnoreDesched))
				{
					continue;
				}
			}
			break;
		}

		ASSERT(uInst != USC_UNDEF);	 
		RemoveInstruction(psDepState, psInst);
		InsertInstBefore(psState, psBlock, psInst, NULL);

		if (psInst->sStageData.psEfoData->uEfoGroupId != USC_UNDEF)
		{
			IMG_UINT32	uEfoGroupId = psInst->sStageData.psEfoData->uEfoGroupId;

			/*
				Keep track of how many instructions remain in the current group.
			*/
			if (uCurrentGroup == USC_UNDEF)
			{
				uCountInCurrentGroup = psEfoState->asEfoGroup[uEfoGroupId].uInstCount;
				uCurrentGroup = uEfoGroupId;
			}
			ASSERT(uCountInCurrentGroup > 0);
			uCountInCurrentGroup--;

			if (uCountInCurrentGroup == 0)
			{
				ASSERT(uCurrentGroup != USC_UNDEF);
				FinishGroup(psState, psEfoState, uCurrentGroup);
				uCurrentGroup = USC_UNDEF;
			}
		}

		if (!bIgnoreDesched && IsDeschedulingPoint(psState, psInst))
		{
			FinishDeschedInst(psState, psEfoState, uInst);
		}

		/*
			For any internal register moves that remain convert them to
			seperate instructions.
		*/
		if (psInst->eOpcode == IEFO && bExpandIRegMoves)
		{
			ExpandInternalRegistersMoves(psState, psEfoState, psBlock, psInst);
		}
	}

	/*
		Release temporary storage.
	*/
	FinishGroupDependencyGraph(psState, psEfoState, bIgnoreDesched);
}

static
IMG_VOID ConvertSingleInstToEfo(PINTERMEDIATE_STATE	psState,
								PCODEBLOCK			psBlock,
								PINST				psInst,
								PINST				psPrevEfoInst,
								IMG_UINT32			uPrevEfoDestIdx,
								IMG_UINT32			uIReg,
								EFO_SRC				eWBSrc)
/*****************************************************************************
 FUNCTION	: ConvertSingleInstToEfo
    
 PURPOSE	: Convert an instruction to an EFO writing its existing result to a 
			  specified internal register and writing the value of another internal
			  register to a unified store destination.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block that contains the instruction.
			  psInst			- Instruction to convert.
			  psPrevEfoInst		- Unified store destination.
			  uPrevEfoDestIdx
			  uIReg				- Internal register to write.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	EFO_SRC	eISrc;
	IOPCODE eOldOpcode = psInst->eOpcode;

	/*
		Convert to an EFO.
	*/
	ConvertFloatInstToEfo(psState, psInst);

	/*
		Set up the result to write into the internal register.
	*/
	if (eOldOpcode != IFMUL)
	{
		eISrc = A0;
	}
	else
	{
		if (uIReg == 0)
		{
			eISrc = M0;
		}
		else
		{
			eISrc = M1;
		}
	}
	if (uIReg == 0)
	{
		SetupIRegDests(psState, psInst, eISrc, EFO_SRC_UNDEF);
	}
	else
	{
		SetupIRegDests(psState, psInst, EFO_SRC_UNDEF, eISrc);
	}

	switch (eOldOpcode)
	{
		case IFADD:
		{
			#if defined(SUPPORT_SGX545)
			if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NEW_EFO_OPTIONS)
			{
				MoveEfoSource(psState, psInst, 2 /* uDestArgIdx */, psInst, 1 /* uArgArgIdx */);

				/*
					A0 = SRC0 + SRC2
				*/
				psInst->u.psEfo->eA0Src0 = SRC0;
				psInst->u.psEfo->eA0Src1 = SRC2;

				/*
					Swap the multiplier sources to avoid
					source bank restrictions.
				*/
				if (!CanUseEfoSrc(psState, psBlock, 0, psInst, 0))
				{
					SwapInstSources(psState, psInst, 0 /* uSrc1 */, 2 /* uSrc2 */);
				}
			}
			else
			#endif /* defined(SUPPORT_SGX545) */
			{
				/*
					A0 = SRC0 + SRC1
				*/
				psInst->u.psEfo->eA0Src0 = SRC0;
				psInst->u.psEfo->eA0Src1 = SRC1;

				/*
					Swap the multiplier sources to avoid
					source bank restrictions.
				*/
				if (!CanUseEfoSrc(psState, psBlock, 0, psInst, 0))
				{
					SwapInstSources(psState, psInst, 0 /* uSrc1 */, 1 /* uSrc2 */);
				}
			}
			break;
		}
		case IFMUL:
		{
			#if defined(SUPPORT_SGX545)
			if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NEW_EFO_OPTIONS)
			{
				if (CanUseEfoSrc(psState, psBlock, 2 /* uArg */, psInst, 1 /* uSrcArgIdx */) &&
					CanUseEfoSrc(psState, psBlock, 1 /* uArg */, psInst, 0 /* uSrcArgIdx */))
				{
					MoveEfoSource(psState, psInst, 2 /* uDestArgIdx */, psInst, 1 /* uSrcArgIdx */);
					MoveEfoSource(psState, psInst, 1 /* uDestArgIdx */, psInst, 0 /* uSrcArgIdx */);
				}
				else
				{
					MoveEfoSource(psState, psInst, 2 /* uDestArgIdx */, psInst, 0 /* uSrcArgIdx */);
				}

				if (uIReg == 0)
				{
					/*
						M0 = SRC2 * SRC1
					*/
					psInst->u.psEfo->eM0Src0 = SRC2;
					psInst->u.psEfo->eM0Src1 = SRC1;
				}
				else
				{
					/*
						M1 = SRC1 * SRC2
					*/
					psInst->u.psEfo->eM1Src0 = SRC1;
					psInst->u.psEfo->eM1Src1 = SRC2;
				}
			}
			else
			#endif /* defined(SUPPORT_SGX545) */
			{
				if (uIReg == 0)
				{
					/*
						M0 = SRC1 * SRC2
					*/
					if (CanUseEfoSrc(psState, psBlock, 2 /* uArg */, psInst, 1 /* uSrcArgIdx */) &&
						CanUseEfoSrc(psState, psBlock, 1 /* uArg */, psInst, 0 /* uSrcArgIdx */))
					{
						MoveEfoSource(psState, psInst, 2 /* uDestArgIdx */, psInst, 1 /* uArgArgIdx */);
						MoveEfoSource(psState, psInst, 1 /* uDestArgIdx */, psInst, 0 /* uArgArgIdx */);
					}
					else
					{
						MoveEfoSource(psState, psInst, 2 /* uDestArgIdx */, psInst, 0 /* uArgArgIdx */);
					}	

					psInst->u.psEfo->eM0Src0 = SRC1;
					psInst->u.psEfo->eM0Src1 = SRC2;
				}
				else
				{
					/*
						M1 = SRC0 * SRC2
					*/
					if (CanUseEfoSrc(psState, psBlock, 2 /* uArg */, psInst, 1 /* uSrcArgIdx */) &&
						CanUseEfoSrc(psState, psBlock, 0 /* uArg */, psInst, 0 /* uSrcArgIdx */))
					{
						MoveEfoSource(psState, psInst, 2 /* uDestArgIdx */, psInst, 1 /* uArgArgIdx */);
					}
					else
					{
						MoveEfoSource(psState, psInst, 2 /* uDestArgIdx */, psInst, 0 /* uArgArgIdx */);
						MoveEfoSource(psState, psInst, 0 /* uDestArgIdx */, psInst, 1 /* uArgArgIdx */);
					}
	
					psInst->u.psEfo->eM1Src0 = SRC0;
					psInst->u.psEfo->eM1Src1 = SRC2;
				}
			}
			break;
		}
		case IFMAD:
		{
			#if defined(SUPPORT_SGX545)
			if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NEW_EFO_OPTIONS)
			{
				SwapInstSources(psState, psInst, 0 /* uSrc1 */, 2 /* uSrc2 */);

				/*
					M0 = SRC2 * SRC1
					A0 = M0 + SRC0
				*/
				psInst->u.psEfo->eM0Src0 = SRC2;
				psInst->u.psEfo->eM0Src1 = SRC1;
				psInst->u.psEfo->eA0Src0 = M0;
				psInst->u.psEfo->eA0Src1 = SRC0;

				/*
					Swap the multiplier sources to avoid
					source bank restrictions.
				*/
				if (!CanUseEfoSrc(psState, psBlock, 1, psInst, 1))
				{
					SwapInstSources(psState, psInst, 1 /* uSrc1 */, 2 /* uSrc2 */);
				}
			}
			else
			#endif /* defined(SUPPORT_SGX545) */
			{
				/*
					M0 = SRC0 * SRC1
					A0 = M0 + SRC2
				*/
				psInst->u.psEfo->eM0Src0 = SRC0;
				psInst->u.psEfo->eM0Src1 = SRC1;
				psInst->u.psEfo->eA0Src0 = M0;
				psInst->u.psEfo->eA0Src1 = SRC2;

				/*
					Swap the multiplier sources to avoid
					source bank restrictions.
				*/
				if (!CanUseEfoSrc(psState, psBlock, 0, psInst, 0))
				{
					SwapInstSources(psState, psInst, 0 /* uSrc1 */, 1 /* uSrc2 */);
				}
			}
			break;
		} 
		case IFMSA:
		{
			#if defined(SUPPORT_SGX545)
			if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NEW_EFO_OPTIONS)
			{
				/*	
					Swap from 
						MSA		DEST, A, B, C	(DEST = A * A + B * C)
					->
						EFO		DEST, B, C, A
				*/
				SwapInstSources(psState, psInst, 0 /* uSrc1 */, 2 /* uSrc2 */);

				/*
					M0 = SRC1 * SRC0
					M1 = SRC2 * SRC2
				*/
				psInst->u.psEfo->eM0Src0 = SRC1;
				psInst->u.psEfo->eM0Src1 = SRC0;
				psInst->u.psEfo->eM1Src0 = SRC2;
				psInst->u.psEfo->eM1Src1 = SRC2;

				/*
					Swap the sources to the second multiply to avoid
					source bank restrictions.
				*/
				if (!CanUseEfoSrc(psState, psBlock, 0, psInst, 0))
				{
					ASSERT(CanUseEfoSrc(psState, psBlock, 0, psInst, 1));
					ASSERT(CanUseEfoSrc(psState, psBlock, 1, psInst, 0));

					SwapInstSources(psState, psInst, 0 /* uSrc1 */, 1 /* uSrc2 */);
				}
			}
			else
			#endif /* defined(SUPPORT_SGX545) */
			{
				/*
					M0 = SRC0 * SRC0
					M1 = SRC1 * SRC2
				*/
				psInst->u.psEfo->eM0Src0 = SRC1;
				psInst->u.psEfo->eM0Src1 = SRC2;
				psInst->u.psEfo->eM1Src0 = SRC0;
				psInst->u.psEfo->eM1Src1 = SRC0;

				/*
					Swap the sources to the second multiply to avoid
					source bank restrictions.
				*/
				if (!CanUseEfoSrc(psState, psBlock, 1, psInst, 1))
				{
					ASSERT(CanUseEfoSrc(psState, psBlock, 1, psInst, 2));
					ASSERT(CanUseEfoSrc(psState, psBlock, 2, psInst, 1));

					SwapInstSources(psState, psInst, 0 /* uSrc1 */, 1 /* uSrc2 */);
				}
			}
			/*
				A0 = M0 + M1
			*/
			psInst->u.psEfo->eA0Src0 = M0;
			psInst->u.psEfo->eA0Src1 = M1;
			break;
		} 
		default: imgabort();
	}

	/*
		Set up the writeback of the previous value in
		the internal register.
	*/
	psInst->u.psEfo->eDSrc = eWBSrc;
	MoveDest(psState, psInst, EFO_US_DEST, psPrevEfoInst, uPrevEfoDestIdx);

	/*
		Store EFO usage information.
	*/
	SetupEfoUsage(psState, psInst);

	/*
		Replace hardware constant arguments to the new EFO by
		secondary attributes.
	*/
	ReplaceHardwareConstants(psState, psInst);
}

static
IMG_BOOL IsValidEFOSourceArgument(PINTERMEDIATE_STATE	psState, 
								  IMG_UINT32			uEfoSrc, 
								  PINST					psSrcInst, 
								  IMG_UINT32			uOrigSrc,
								  IMG_PUINT32			puReservedSecAttrCount)
/*****************************************************************************
 FUNCTION	: IsValidEFOSourceArgument
    
 PURPOSE	: Check if a source to an existing instruction would be valid as
			  a source to an EFO instruction.

 PARAMETERS	: psState			- Compiler state.
			  uEfoSrc			- EFO source argument to check for.
			  psSrcInst			- Original instruction.
			  uOrigSrc			- Original instruction source.
			  puReservedSecAttrCount
								- Number of secondary attributes reserved to
								replace constant sources.
			  
 RETURNS	: TRUE if the source is valid.
*****************************************************************************/
{
	IMG_UINT32	uHwType;
	PARG		psOrigSrc;
	IMG_BOOL	bInvalidBank;

	ASSERT(uOrigSrc < psSrcInst->uArgumentCount);
	psOrigSrc = &psSrcInst->asArg[uOrigSrc];

	uHwType = GetPrecolouredRegisterType(psState, psSrcInst, psOrigSrc->uType, psOrigSrc->uNumber);

	bInvalidBank = IMG_FALSE;
	if (!IsValidEFOSource(psState, NULL /* psInst */, uEfoSrc, uHwType, psOrigSrc->uIndexType))
	{
		bInvalidBank = IMG_TRUE;
	}
	if (uHwType == USEASM_REGTYPE_IMMEDIATE && psOrigSrc->uNumber > EURASIA_USE_MAXIMUM_IMMEDIATE)
	{
		bInvalidBank = IMG_TRUE;
	}

	if (bInvalidBank && (uHwType == USEASM_REGTYPE_FPCONSTANT || uHwType == USEASM_REGTYPE_IMMEDIATE))
	{
		/*
			Check that there are secondary attributes available to replace hardware constants.
		*/
		if ((psState->sSAProg.uConstSecAttrCount + (*puReservedSecAttrCount) + 1) <= psState->sSAProg.uInRegisterConstantLimit)
		{
			(*puReservedSecAttrCount)++;
			bInvalidBank = IMG_FALSE;
		}
	}

	if (bInvalidBank)
	{
		return IMG_FALSE;
	}

	if (!IsValidEFOSourceModifier(psState, psSrcInst, uOrigSrc))
	{
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

static
IMG_BOOL IsValidEFOInst(PINTERMEDIATE_STATE		psState, 
						PINST					psSrcInst, 
						PCEFO_PARAMS_TEMPLATE	psTemplate, 
						IMG_UINT32 const*		auSrcOrder)
/*****************************************************************************
 FUNCTION	: IsValidEFOInst
    
 PURPOSE	: If possible replace an instruction by an equivalent EFO
			  instruction.

 PARAMETERS	: psState			- Compiler state.
			  psSrcInst			- Instruction to convert.
			  psTemplate		- ALU setup for the new instruction.
			  auSrcOrder		- Mapping from the order of the sources to
								the original instruction to the order of the
								sources of the replacement EFO instruction.
			  
 RETURNS	: TRUE if the instruction was replaced.
*****************************************************************************/
{
	PINST		psEfoInst;
	IMG_UINT32	uSrc;
	IMG_UINT32	uReservedSecAttrCount;

	/*
		Check the EFO instruction supports the register banks and modifiers of
		all the sources to the original instruction 
	*/
	uReservedSecAttrCount = 0;
	for (uSrc = 0; uSrc < EFO_UNIFIEDSTORE_SOURCE_COUNT; uSrc++)
	{
		IMG_UINT32	uOrigSrc = auSrcOrder[uSrc];

		if (uOrigSrc == USC_UNDEF)
		{
			continue;
		}
		if (!IsValidEFOSourceArgument(psState, uSrc, psSrcInst, uOrigSrc, &uReservedSecAttrCount))
		{
			return IMG_FALSE;
		}
	}

	/*
		Create a replacement EFO instruction.
	*/
	psEfoInst = AllocateInst(psState, psSrcInst);

	SetOpcodeAndDestCount(psState, psEfoInst, IEFO, EFO_DEST_COUNT);

	/*
		Copy details of the ALU setup for the EFO instruction.
	*/
	psEfoInst->u.psEfo->eM0Src0 = psTemplate->eM0Src0;
	psEfoInst->u.psEfo->eM0Src1 = psTemplate->eM0Src1;
	psEfoInst->u.psEfo->eM1Src0 = psTemplate->eM1Src0;
	psEfoInst->u.psEfo->eM1Src1 = psTemplate->eM1Src1;

	psEfoInst->u.psEfo->eA0Src0 = psTemplate->eA0Src0;
	psEfoInst->u.psEfo->eA0Src1 = psTemplate->eA0Src1;
	psEfoInst->u.psEfo->eA1Src0 = psTemplate->eA1Src0;
	psEfoInst->u.psEfo->eA1Src1 = psTemplate->eA1Src1;

	psEfoInst->u.psEfo->bIgnoreDest = IMG_FALSE;
	psEfoInst->u.psEfo->eDSrc = psTemplate->eDSrc;

	/*
		Mark the EFO instruction has not writing any internal registers.
	*/
	SetupIRegDests(psState, psEfoInst, EFO_SRC_UNDEF, EFO_SRC_UNDEF);

	/*
		Copy the destination of the original instruction to the EFO instruction.
	*/
	MoveDest(psState, psEfoInst, EFO_US_DEST, psSrcInst, 0 /* uMoveFromDestIdx */);
	CopyPartiallyWrittenDest(psState, psEfoInst, EFO_US_DEST, psSrcInst, 0 /* uMoveFromDestIdx */);
	psEfoInst->auLiveChansInDest[EFO_US_DEST] = psSrcInst->auLiveChansInDest[EFO_US_DEST];

	/*
		Copy the predicate of the original instruction to the EFO instruction.
	*/
	CopyPredicate(psState, psEfoInst, psSrcInst);

	/*
		Copy the sources of the original instruction to the EFO instruction.
	*/
	for (uSrc = 0; uSrc < EFO_UNIFIEDSTORE_SOURCE_COUNT; uSrc++)
	{
		IMG_UINT32	uOrigSrc = auSrcOrder[uSrc];

		if (uOrigSrc == USC_UNDEF)
		{
			SetSrcUnused(psState, psEfoInst, uSrc);
		}
		else
		{
			MoveFloatArgToEfo(psState, psEfoInst, uSrc, psSrcInst, uOrigSrc);
		}
	}

	SetupEfoUsage(psState, psEfoInst);

	/*
		Replace large immediate sources by secondary attributes.
	*/
	for (uSrc = 0; uSrc < EFO_UNIFIEDSTORE_SOURCE_COUNT; uSrc++)
	{
		PARG	psSrc = &psEfoInst->asArg[uSrc];

		if (psSrc->uType == USEASM_REGTYPE_IMMEDIATE && psSrc->uNumber > EURASIA_USE_MAXIMUM_IMMEDIATE)
		{
			IMG_BOOL	bRet;
			IMG_UINT32	uSecAttrType;
			IMG_UINT32	uSecAttrNum;

			bRet = AddStaticSecAttr(psState, psSrc->uNumber, &uSecAttrType, &uSecAttrNum);
			ASSERT(bRet);
			SetSrc(psState, psEfoInst, uSrc, uSecAttrType, uSecAttrNum, psSrc->eFmt);
		}
	}

	/*
		Replace hardware constant sources to the EFO by secondary attributes.
	*/
	ReplaceHardwareConstants(psState, psEfoInst);

	/*
		Insert the EFO instruction before the original instruction.
	*/
	InsertInstBefore(psState, psSrcInst->psBlock, psEfoInst, psSrcInst);

	/*
		Free the original instruction.
	*/
	RemoveInst(psState, psSrcInst->psBlock, psSrcInst);
	FreeInst(psState, psSrcInst);

	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL ConvertInstToEfo(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: ConvertInstToEfo
    
 PURPOSE	: If possible replace an instruction by an equivalent EFO
			  instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  
 RETURNS	: TRUE if the instruction was replaced.
*****************************************************************************/
{
	switch (psInst->eOpcode)
	{
		case IFADD:
		{
			static const EFO_PARAMS_TEMPLATE	sAddTemplate = 
			{
				EFO_SRC_UNDEF,	/* eM0Src0 */ 
				EFO_SRC_UNDEF,	/* eM0Src1 */
				EFO_SRC_UNDEF,	/* eM1Src0 */
				EFO_SRC_UNDEF,	/* eM1Src1 */
				EFO_SRC_UNDEF,	/* eA0Src0 */
				EFO_SRC_UNDEF,	/* eA0Src1 */
				SRC1,			/* eA1Src0 */
				SRC2,			/* eA1Src1 */
				EFO_SRC_UNDEF,	/* eI0Src */
				EFO_SRC_UNDEF,	/* eI1Src */
				A1,				/* eDSrc */
			};
			static const IMG_UINT32 au_01[] = {USC_UNDEF, 0, 1};
			static const IMG_UINT32 au_10[] = {USC_UNDEF, 1, 0};

			if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NEW_EFO_OPTIONS) == 0)
			{
				return IMG_FALSE;
			}

			/*
					FADD	DEST, A, B
				->
					EFO		DEST=A0, A0=SRC1+SRC2, UNDEF, A, B
			*/
			/*
				Try with the ADD sources in either order.
			*/
			if (IsValidEFOInst(psState, psInst, &sAddTemplate, au_01))
			{
				return IMG_TRUE;
			}
			if (IsValidEFOInst(psState, psInst, &sAddTemplate, au_10))
			{
				return IMG_TRUE;
			}
			break;
		}
		case IFMAD:
		{
			#if defined(SUPPORT_SGX545)
			if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NEW_EFO_OPTIONS) != 0)
			{
				static const EFO_PARAMS_TEMPLATE	sMadTemplate_545 = 
				{
					SRC2,			/* eM0Src0 */ 
					SRC1,			/* eM0Src1 */
					EFO_SRC_UNDEF,	/* eM1Src0 */
					EFO_SRC_UNDEF,	/* eM1Src1 */
					M0,				/* eA0Src0 */
					SRC0,			/* eA0Src1 */
					EFO_SRC_UNDEF,	/* eA1Src0 */
					EFO_SRC_UNDEF,	/* eA1Src1 */
					EFO_SRC_UNDEF,	/* eI0Src */
					EFO_SRC_UNDEF,	/* eI1Src */
					A0,				/* eDSrc */
				};
				static const IMG_UINT32 au201[] = {2, 0, 1};
				static const IMG_UINT32 au210[] = {2, 1, 0};

				/*
						FMAD	DEST, A, B, C
					->
						EFO		DEST=A0, A0=M0+SRC0, M0=SRC2*SRC1 
				*/
				if (IsValidEFOInst(psState, psInst, &sMadTemplate_545, au201))
				{
					return IMG_TRUE;
				}
				if (IsValidEFOInst(psState, psInst, &sMadTemplate_545, au210))
				{
					return IMG_TRUE;
				}
			}
			else
			#endif /* defined(SUPPORT_SGX545) */
			{
				static const EFO_PARAMS_TEMPLATE	sMadTemplate_535 = 
				{
					SRC1,			/* eM0Src0 */ 
					SRC2,			/* eM0Src1 */
					EFO_SRC_UNDEF,	/* eM1Src0 */
					EFO_SRC_UNDEF,	/* eM1Src1 */
					M0,				/* eA0Src0 */
					I0,				/* eA0Src1 */
					EFO_SRC_UNDEF,	/* eA1Src0 */
					EFO_SRC_UNDEF,	/* eA1Src1 */
					EFO_SRC_UNDEF,	/* eI0Src */
					EFO_SRC_UNDEF,	/* eI1Src */
					A0,				/* eDSrc */
				};
				static const IMG_UINT32 au_01[] = {USC_UNDEF, 0, 1};
				static const IMG_UINT32 au_10[] = {USC_UNDEF, 1, 0};
				PARG psSrc2;

				psSrc2 = &psInst->asArg[2];
				if (
						psSrc2->uType == USEASM_REGTYPE_FPINTERNAL && 
						psSrc2->uNumber == 0 &&
						!HasNegateOrAbsoluteModifier(psState, psInst, 2 /* uArgIdx */)
					)
				{
					/*
							FMAD	DEST, A, B, I0
						->
							EFO		DEST=A0, A0=M0+I0, M0=SRC1*SRC2
					*/
					if (IsValidEFOInst(psState, psInst, &sMadTemplate_535, au_01))
					{
						return IMG_TRUE;
					}
					if (IsValidEFOInst(psState, psInst, &sMadTemplate_535, au_10))
					{
						return IMG_TRUE;
					}
				}
			}
			break;
		}
		default:
			break;
	}
	return IMG_FALSE;
}

static
IMG_BOOL CanConvertSingleInstToEfo(PINTERMEDIATE_STATE	psState,
								   PINST				psInst,
								   IMG_UINT32			uIReg)
/*****************************************************************************
 FUNCTION	: CanConvertSingleInstToEfo
    
 PURPOSE	: Check if an instruction could be converted to an EFO writing
			  its existing result to a specified internal register.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  uIReg				- Internal register to write.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	switch (psInst->eOpcode)
	{
		case IFADD:
		{
			IMG_UINT32	uAddArg0, uAddArg1;

			#if defined(SUPPORT_SGX545)
			if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NEW_EFO_OPTIONS)
			{
				uAddArg0 = 0;
				uAddArg1 = 2;
			}
			else
			#endif /* defined(SUPPORT_SGX545) */
			{
				uAddArg0 = 0;
				uAddArg1 = 1;
			}

			if (
					!(
						CanUseEfoSrc(psState, psInst->psBlock, uAddArg0, psInst, 0) &&
						CanUseEfoSrc(psState, psInst->psBlock, uAddArg1, psInst, 1)
					 ) &&
					 !(
						CanUseEfoSrc(psState, psInst->psBlock, uAddArg1, psInst, 0) &&
						CanUseEfoSrc(psState, psInst->psBlock, uAddArg0, psInst, 1)
					 )
				)
			{
				return IMG_FALSE;
			}
			return IMG_TRUE;
		}
		case IFMUL:
		{
			IMG_UINT32	uMulArg0;
			IMG_UINT32	uMulArg1;

			#if defined(SUPPORT_SGX545)
			if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NEW_EFO_OPTIONS)
			{
				uMulArg0 = 1;
				uMulArg1 = 2;
			}
			else
			#endif /* defined(SUPPORT_SGX545) */
			{
				if (uIReg == 0)
				{
					uMulArg0 = 1;
					uMulArg1 = 2;
				}
				else
				{
					uMulArg0 = 0;
					uMulArg1 = 2;
				}
			}

			if (CanUseEfoSrc(psState, psInst->psBlock, uMulArg0, psInst, 0 /* uSrcInstArgIdx */) && 
				CanUseEfoSrc(psState, psInst->psBlock, uMulArg1, psInst, 1 /* uSrcInstArgIdx */))
			{
				return IMG_TRUE;
			}
			else if (CanUseEfoSrc(psState, psInst->psBlock, uMulArg0, psInst, 1 /* uSrcInstArgIdx */) && 
					 CanUseEfoSrc(psState, psInst->psBlock, uMulArg1, psInst, 0 /* uSrcInstArgIdx */))
			{
				return IMG_TRUE;
			}
			else
			{
				return IMG_FALSE;
			}
		}
		case IFMAD:
		{
			IMG_UINT32	uAddArg, uMulArg0, uMulArg1;

			#if defined(SUPPORT_SGX545)
			if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NEW_EFO_OPTIONS)
			{
				uAddArg = 0;
				uMulArg0 = 1;
				uMulArg1 = 2;
			}
			else
			#endif /* defined(SUPPORT_SGX545) */
			{
				uAddArg = 2;
				uMulArg0 = 0;
				uMulArg1 = 1;
			}

			if (!CanUseEfoSrc(psState, psInst->psBlock, uAddArg, psInst, 2))
			{
				return IMG_FALSE;
			}
			if (
					!(
						CanUseEfoSrc(psState, psInst->psBlock, uMulArg0, psInst, 0) &&
						CanUseEfoSrc(psState, psInst->psBlock, uMulArg1, psInst, 1)
					 ) &&
				    !(
						CanUseEfoSrc(psState, psInst->psBlock, uMulArg1, psInst, 0) &&
						CanUseEfoSrc(psState, psInst->psBlock, uMulArg0, psInst, 1)
					 )
			   )
			{
				return IMG_FALSE;
			}
			return IMG_TRUE;
		} 
		case IFMSA:
		{
			IMG_UINT32	uSqrArg, uMulArg0, uMulArg1;

			#if defined(SUPPORT_SGX545)
			if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NEW_EFO_OPTIONS)
			{
				uSqrArg = 2;
				uMulArg0 = 0;
				uMulArg1 = 1;
			}
			else
			#endif /* defined(SUPPORT_SGX545) */
			{
				uSqrArg = 0;
				uMulArg0 = 1;
				uMulArg1 = 2;
			}

			if (!CanUseEfoSrc(psState, psInst->psBlock, uSqrArg, psInst, 0))
			{
				return IMG_FALSE;
			}
			if (
					!(
						CanUseEfoSrc(psState, psInst->psBlock, uMulArg0, psInst, 1) &&
						CanUseEfoSrc(psState, psInst->psBlock, uMulArg1, psInst, 2)
					 ) &&
				    !(
						CanUseEfoSrc(psState, psInst->psBlock, uMulArg1, psInst, 1) &&
						CanUseEfoSrc(psState, psInst->psBlock, uMulArg0, psInst, 2)
					 )
			   )
			{
				return IMG_FALSE;
			}
			return IMG_TRUE;
		}
		default:
		{
			return IMG_FALSE;
		}
	}
}

static
IMG_BOOL CheckGroupOrderForNonEfo(PINTERMEDIATE_STATE	psState,
								  PEFOGEN_STATE			psEfoState,
								  IMG_UINT32			uEfoGroupIdx,
								  IMG_UINT32			uInstIdx)
/*****************************************************************************
 FUNCTION	: CheckGroupOrderForNonEfo
    
 PURPOSE	: Check for another EFO group dependent on the specified
			  instruction and a dependency of the specified group.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  uEfoGroupIdx		- Index of the EFO group to check.
			  uInstIdx			- Index of the instruction to check.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uOtherEfoGroupIdx;

	for (uOtherEfoGroupIdx = 0; uOtherEfoGroupIdx < psEfoState->uEfoGroupCount; uOtherEfoGroupIdx++)
	{
		if (uOtherEfoGroupIdx != uEfoGroupIdx)
		{
			if (IsGroupDependentOnInst(psState, psEfoState, uInstIdx, uOtherEfoGroupIdx) &&
				GetClosedDependency(psEfoState, uEfoGroupIdx, uOtherEfoGroupIdx))
			{
				return IMG_FALSE;
			}
			if (IsInstDependentOnGroup(psState, psEfoState, uInstIdx, uOtherEfoGroupIdx) &&
				GetClosedDependency(psEfoState, uOtherEfoGroupIdx, uEfoGroupIdx))
			{
				return IMG_FALSE;
			}
		}
	}
	return IMG_TRUE;
}

static
IMG_BOOL WriteDestUsingNonEfo(PINTERMEDIATE_STATE	psState,
							  PCODEBLOCK			psBlock,
							  PEFOGEN_STATE			psEfoState,
							  PINST					psEfoInst,
							  IMG_UINT32			uEfoInstDestIdx,
							  IMG_UINT32			uIReg)
/*****************************************************************************
 FUNCTION	: WriteDestUsingNonEfo
    
 PURPOSE	: To do a writeback from an internal register look for a single
			  instruction which can be converted to an EFO which does
			  the writeback and writes its own result to an internal register.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Basic block containing the EFO.
			  psEfoState		- Module state.
			  psEfoInst			- EFO instruction.
			  uEfoInstDestIdx	- Index of the destination to write back to.
			  uIReg				- Internal register to write back from.
			  
 RETURNS	: TRUE if an instruction was found.
*****************************************************************************/
{
	IMG_UINT32		uInst;
	PDGRAPH_STATE	psDepState = psBlock->psDepState;
	IMG_UINT32		uEfoInst = psEfoInst->uId;
	IMG_UINT32		uEfoGroupId = psEfoInst->sStageData.psEfoData->uEfoGroupId;
	PARG			psEfoDest = &psEfoInst->asDest[uEfoInstDestIdx];

	/*
		Check no instruction is using the internal register as a source.
	*/
	if (psEfoInst->sStageData.psEfoData->abIRegUsed[uIReg])
	{
		return IMG_FALSE;
	}

	for (uInst = 0; uInst < psDepState->uBlockInstructionCount; uInst++)
	{
		PINST		psInst = ArrayGet(psState, psDepState->psInstructions, uInst);
		ARG			sInstDest;
		IMG_UINT32	uArg;
		PINST		psEfoDependencyInst, psDeschedDependencyInst;
		IMG_UINT32	uOtherEfoGroupId;
		PARG		psNewEfoDest;

		if (psInst == NULL)
		{
			continue;
		}

		/*
			Check the EFO isn't dependent on the instruction's result.
		*/
		if (GraphGet(psState, psDepState->psClosedDepGraph, uEfoInst, uInst))
		{
			continue;
		}
		
		/*
			Check for a opcode and arguments we can convert to an EFO.
		*/
		if (!CanConvertSingleInstToEfo(psState, psInst, uIReg))
		{
			continue;
		}
		if (!NoPredicate(psState, psInst))
		{
			continue;
		}
		
		/*
			Check for a destination we might be able to replace by an internal register.
		*/
		if (psInst->asDest[0].uIndexType != USC_REGTYPE_NOINDEX)
		{
			continue;
		}
		if (ReferencedOutsideBlock(psState, psBlock, &psInst->asDest[0]))
		{
			continue;
		}

		/*
			Is there a descheduling instruction dependent on this instruction and a dependency
			of an instruction in the EFO group.
		*/
		if (IsDescheduleBetweenInstAndGroup(psState, psEfoState, uInst, psEfoInst->sStageData.psEfoData->uEfoGroupId))
		{
			continue;
		}

		/*
			Check there are no descheduling instructions between the EFO and the instruction.
		*/
		if (IsDescheduleBetweenGroupAndInsts(psState, 
											 psEfoState, 
											 psEfoInst->sStageData.psEfoData->uEfoGroupId, 
											 USC_UNDEF, 
											 uInst, 
											 USC_UNDEF))
		{
			continue;
		}

		/*
			Is another instruction writing an internal register dependent on the EFO and a 
			dependency of this instruction?
		*/
		if (IsInterveningIRegWriteForRead(psState, psEfoState, psEfoInst, USC_UNDEF, uInst))
		{
			continue;
		}

		/*
			Is there another EFO group dependent on the instruction and a dependency of
			the group the EFO belongs to?
		*/
		if (!CheckGroupOrderForNonEfo(psState, psEfoState, uEfoGroupId, uInst))
		{
			continue;
		}

		/*
			Check the instruction either belongs to no EFO group or the same EFO group as
			the current EFO.
		*/
		if (psInst->sStageData.psEfoData->uEfoGroupId != USC_UNDEF && 
			psInst->sStageData.psEfoData->uEfoGroupId != uEfoGroupId)
		{
			continue;
		}

		/*
			Check all the instructions which use the unified store destination can be
			made dependent on this instruction.
		*/
		if (!CanWriteDestUsingEfo(psState, psEfoState, psEfoInst, psEfoDest, uInst, IMG_FALSE))
		{
			continue;
		}

		/*
			Check the result of the instruction can be replaced everywhere by an internal register.
		*/
		ASSERT(psInst->uDestCount == 1);
		if (!CanReplaceIRegMove(psState,
								psEfoState,
								psBlock,
								psEfoInst,
								uInst,
								&psInst->asDest[0],
								NULL,
								&psEfoDependencyInst,
								&psDeschedDependencyInst))
		{
			continue;
		}

		/*
			Convert the instruction to an EFO calculating the same result but also writing
			back the previous EFO result from an internal register.
		*/
		EFO_TESTONLY(printf("\tUsing other instruction %d's writeback for %d.\n", psInst->uId, psEfoInst->asDest[uEfoInstDestIdx].uNumber));
		sInstDest = psInst->asDest[0];
		ConvertSingleInstToEfo(psState, psBlock, psInst, psEfoInst, uEfoInstDestIdx, uIReg, (EFO_SRC)(I0 + uIReg));
		psNewEfoDest = &psInst->asDest[EFO_US_DEST];

		/*
			Replace the instruction's sources which use the unified store destination.
		*/
		for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
		{
			if (psInst->asArg[uArg].uType == psNewEfoDest->uType &&
				psInst->asArg[uArg].uNumber == psNewEfoDest->uNumber)
			{
				ASSERT(CanUseSrc(psState, psInst, uArg, USEASM_REGTYPE_FPINTERNAL, USC_REGTYPE_NOINDEX));
				SetSrc(psState, psInst, uArg, USEASM_REGTYPE_FPINTERNAL, uIReg, UF_REGFORMAT_F32);
			}
		}

		/*
			Make the instruction dependent on the EFO.
		*/
		AddClosedDependency(psState, psEfoState->psCodeBlock->psDepState, uEfoInst, uInst);

		/*
			Update the dependencies between EFO groups.
		*/
		for (uOtherEfoGroupId = 0; uOtherEfoGroupId < psEfoState->uEfoGroupCount; uOtherEfoGroupId++)
		{
			if (uOtherEfoGroupId != uEfoGroupId &&
				!IsGroupDependency(psEfoState, uEfoGroupId, uOtherEfoGroupId) &&
				IsGroupDependentOnInst(psState, psEfoState, uInst, uOtherEfoGroupId))
			{
				ASSERT(!IsGroupDependency(psEfoState, uOtherEfoGroupId, uEfoGroupId));
				SetDependency(psEfoState, uOtherEfoGroupId, uEfoGroupId, 1);
				UpdateClosedEfoDependencyGraph(psEfoState, 
											   uOtherEfoGroupId,
											   uEfoGroupId);
			}
		}

		/*
			Make all the other instructions which use the unified store destination dependent on the 
			instruction.
		*/
		AddDepsForWriteDestUsingEfo(psState, psEfoState, uEfoInst, psNewEfoDest, uInst);

		/*
			Add the instruction to the list of instructions reading the internal
			registers of the first EFO.
		*/
		if (psInst->sStageData.psEfoData->uEfoGroupId != uEfoGroupId)
		{
			ASSERT(psInst->sStageData.psEfoData->uEfoGroupId == USC_UNDEF);
			psInst->sStageData.psEfoData->uEfoGroupId = uEfoGroupId;
			AddIRegDependency(psState, psEfoState, psEfoInst, uInst);
			AddToEfoReaderList(psEfoInst, psInst);

			psEfoState->asEfoGroup[uEfoGroupId].uInstCount++;
		}

		/*
			Replace the result of the instruction by an internal register.
		*/
		ReplaceIRegMove(psState,
						psBlock,
						psEfoState,
						psEfoInst,
						uInst,
						&sInstDest,
						NULL,
						uIReg,
						psEfoDependencyInst,
						psDeschedDependencyInst);

		return IMG_TRUE;
	}

	return IMG_FALSE;
}

static IMG_BOOL CanReplaceSourceByIReg(PINTERMEDIATE_STATE	psState,
									   PEFOGEN_STATE		psEfoState,
									   PCODEBLOCK			psBlock,
									   IMG_UINT32			uInst,
									   PINST				psInst,
									   IMG_UINT32			uOtherInst,
									   PINST				psOtherInst,
									   PARG					psSource,
									   IMG_PUINT32			puWriterInst,
									   PINST*				ppsWriterInst)
/*****************************************************************************
 FUNCTION	: CanReplaceSourceByIReg
    
 PURPOSE	: Check if a source to an instruction could be replaced by an
			  internal register by changing the instruction which writes
			  the source.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  psBlock			- Basic block containing the instruction.
			  uInst
			  uOtherInst
			  psSource			- The source to replace.
			  puWriterInst		- On success returns the instruction which
			  ppsWriterInst		  writes the source.
			  
 RETURNS	: TRUE if replacement is possible.
*****************************************************************************/
{
	IMG_UINT32		uWriterInst;
	PINST			psWriterInst;
	PUSEDEF_CHAIN	psSourceUseDef;
	PUSEDEF			psSourceDef;
	IMG_UINT32		uEfoGroupIdx;

	/*
		Check the source is suitable for replacement.
	*/
	if (psSource->uType != USEASM_REGTYPE_TEMP)
	{
		return IMG_FALSE;
	}
	if (psSource->uIndexType != USC_REGTYPE_NOINDEX)
	{
		return IMG_FALSE;
	}
	if (psSource->eFmt == UF_REGFORMAT_F16)
	{
		return IMG_FALSE;
	}
	ASSERT(psSource->eFmt == UF_REGFORMAT_F32);

	psSourceUseDef = UseDefGet(psState, psSource->uType, psSource->uNumber);

	/*
		Check it's only used in this block.
	*/
	if (UseDefIsReferencedOutsideBlock(psState, psSourceUseDef, psBlock))
	{
		return IMG_FALSE;
	}

	/*
		Get the instruction which writes the source.
	*/
	ASSERT(psSourceUseDef->psDef != NULL);
	psSourceDef = psSourceUseDef->psDef;
	ASSERT(psSourceDef->eType == DEF_TYPE_INST);
	psWriterInst = psSourceDef->u.psInst;

	ASSERT(psWriterInst->psBlock == psBlock);
	uWriterInst = psWriterInst->uId;

	ASSERT(uWriterInst != uInst);

	/*
		Check if the writer is the same as the other instruction we are trying
		to combine into an EFO.
	*/
	if (uWriterInst == uOtherInst)
	{
		return IMG_FALSE;
	}

	/*
		Check for a conditional or partial write.
	*/
	if (!NoPredicate(psState, psWriterInst))
	{
		return IMG_FALSE;
	}
	ASSERT(psSourceDef->uLocation < psWriterInst->uDestCount);
	if (psWriterInst->auDestMask[psSourceDef->uLocation] != USC_DESTMASK_FULL)
	{
		return IMG_FALSE;
	}

	/*
		Check if we could replace the destination by an internal register.
	*/
	if (psWriterInst->uDestCount > 1)
	{
		return IMG_FALSE;
	}
	if (psWriterInst->eOpcode == IEFO)
	{
		return IMG_FALSE;
	}
	if (!CanUseDest(psState, psWriterInst, psSourceDef->uLocation, USEASM_REGTYPE_FPINTERNAL, USC_REGTYPE_NOINDEX))
	{
		return IMG_FALSE;
	}
	if (IsDeschedulingPoint(psState, psWriterInst))
	{
		return IMG_FALSE;
	}

	/*
		Check that the source is only used in one instruction.
	*/
	if (UseDefGetSingleSourceUse(psState, psInst, psSource) == USC_UNDEF)
	{
		return IMG_FALSE;
	}

	/*
		Check if there is a descheduling instruction between the instruction writing
		the source and the instructions we are going to combine into an EFO.
	*/
	if (IsDescheduleBetweenInstAndInsts(psState, psEfoState, uWriterInst, uInst, uOtherInst))
	{
		return IMG_FALSE;
	}

	/*
		Check if there is another use of internal registers between the writer
		instructions and the instructions we are going to combine into an EFO.
	*/
	for (uEfoGroupIdx = 0; uEfoGroupIdx < psEfoState->uEfoGroupCount; uEfoGroupIdx++)
	{
		if (IsGroupDependentOnInst(psState, psEfoState, uWriterInst, uEfoGroupIdx))
		{
			if (IsInstDependentOnGroup(psState, psEfoState, uInst, uEfoGroupIdx) ||
				IsInstDependentOnGroup(psState, psEfoState, uOtherInst, uEfoGroupIdx))
			{
				return IMG_FALSE;
			}
			ASSERT(psInst->sStageData.psEfoData->uEfoGroupId == USC_UNDEF);
			if (psOtherInst->sStageData.psEfoData->uEfoGroupId != USC_UNDEF)
			{
				if (IsGroupDependentOnGroup(psState, psEfoState, psOtherInst->sStageData.psEfoData->uEfoGroupId, uEfoGroupIdx))
				{
					return IMG_FALSE;
				}
			}
		}
	}

	/*
		Return the instruction which writes the source.
	*/
	*ppsWriterInst = psWriterInst;
	*puWriterInst = uWriterInst;

	return IMG_TRUE;
}

static IMG_VOID ReplaceDestByIReg(PINTERMEDIATE_STATE	psState,
								  PEFOGEN_STATE			psEfoState,
								  PINST					psInst,
								  IMG_UINT32			uIReg,
								  IMG_UINT32			uEfoGroupId,
								  PINST					psNextWriter)
/*****************************************************************************
 FUNCTION	: ReplaceDestByIReg
    
 PURPOSE	: Change an instruction to write to an internal register.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  psInst			- Instruction to change.
			  uIReg				- Internal register to write.
			  uEfoGroupId		- Internal register group to add the
								instruction to.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		Update the instruction's destination.
	*/
	SetDest(psState, psInst, 0 /* uDestIdx */, USEASM_REGTYPE_FPINTERNAL, uIReg, UF_REGFORMAT_F32);

	/*
		Add the instruction to the list of internal register writers.
	*/
	if (psNextWriter == NULL)
	{
		AddToEfoWriterList(psState, psEfoState, uEfoGroupId, psInst);
	}
	else
	{
		AddToMiddleEfoWriterList(psState, psEfoState, uEfoGroupId, psInst, psNextWriter);
	}

	ASSERT(psInst->sStageData.psEfoData->uEfoGroupId == USC_UNDEF);
	psInst->sStageData.psEfoData->uEfoGroupId = uEfoGroupId;
}

/*****************************************************************************
 FUNCTION	: SetupEfoInstExtraIRegs
    
 PURPOSE	: Setup an EFO instruction when we are combining two other
			  instruction while changing some earlier instructions to
			  write internal registers.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  psInstA			- Combined instructions.
			  psInstB
			  uAIReg
			  psEfoInst			- EFO instruction.
			  uAWriterInst		- Instruction writing a source to A which
			  psAWriterInst		is going to be changed to write an internal
								register.
			  uBWriterInst		- Instruction writing a source to B which
			  psBWriterInst		is going to be changed to write an internal
								register.
			  
 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID SetupEfoInstExtraIRegs(PINTERMEDIATE_STATE	psState,
									   PEFOGEN_STATE		psEfoState,
									   PINST				psInstA,
									   PINST				psInstB,
									   IMG_UINT32			uAIReg,
									   PINST				psEfoInst,
									   IMG_UINT32			uAWriterInst,
									   PINST				psAWriterInst,
									   IMG_UINT32			uBWriterInst,
									   PINST				psBWriterInst)
{
	IMG_UINT32	uNewEfoGroupId;
	PINST		psNextWriter;

	ASSERT(psInstA->sStageData.psEfoData->uEfoGroupId == USC_UNDEF);

	if (psInstB->sStageData.psEfoData->uEfoGroupId == USC_UNDEF)
	{
		/*
			Create a new EFO group to represent the combined instructions and the
			instructions we are changing to write internal registers.
		*/
		uNewEfoGroupId = CreateNewEfoGroup(psState, psEfoState, IMG_FALSE);

		/*
			Add new EFO writers at the end of the group.
		*/
		psNextWriter = NULL;
	}
	else
	{
		/*
			Add to the EFO group INSTB already belongs to.
		*/
		uNewEfoGroupId = psInstB->sStageData.psEfoData->uEfoGroupId;
		psNextWriter = psInstB;
	}
	/*
		Increase the instruction count for each of instructions
		we are changing to write an internal register.
	*/
	if (psBWriterInst != NULL)
	{
		psEfoState->asEfoGroup[uNewEfoGroupId].uInstCount += 2;
	}
	else
	{
		psEfoState->asEfoGroup[uNewEfoGroupId].uInstCount += 1;
	}
	/*
		Increase the instruction count for the new EFO.
	*/
	if (psInstB->sStageData.psEfoData->uEfoGroupId == USC_UNDEF)
	{
		psEfoState->asEfoGroup[uNewEfoGroupId].uInstCount++;
	}

	/*
		Create a dummy destination.
	*/
	if (psEfoInst->u.psEfo->bIgnoreDest)
	{
		SetDest(psState, psEfoInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, GetNextRegister(psState), UF_REGFORMAT_F32);
	}

	SetupEfoUsage(psState, psEfoInst);

	/*
		Replace hardware constant sources to the EFO
		by secondary attributes.
	*/
	ReplaceHardwareConstants(psState, psEfoInst);

	/*
		Add a dependency between the writer instructions.
	*/
	if (psBWriterInst != NULL)
	{
		ASSERT(!GraphGet(psState, psEfoState->psCodeBlock->psDepState->psClosedDepGraph, uAWriterInst, uBWriterInst));
		AddClosedDependency(psState, psEfoState->psCodeBlock->psDepState, uAWriterInst, uBWriterInst);
	}

	/*
		Change the instructions writing the other sources to write internal registers instead.
	*/
	ReplaceDestByIReg(psState, psEfoState, psAWriterInst, uAIReg, uNewEfoGroupId, psNextWriter);
	if (psBWriterInst != NULL)
	{
		ReplaceDestByIReg(psState, psEfoState, psBWriterInst, 1 - uAIReg, uNewEfoGroupId, psNextWriter);
		AddToEfoReaderList(psAWriterInst, psBWriterInst);
		AddToEfoReaderList(psBWriterInst, psEfoInst);
	}
	else
	{
		AddToEfoReaderList(psAWriterInst, psEfoInst);
	}

	/*
		Add the EFO for the combined instructions to the group as well.
	*/
	if (psNextWriter == NULL)
	{
		AddToEfoWriterList(psState, psEfoState, uNewEfoGroupId, psEfoInst);
	}
	else
	{
		/*
			Copy the PREVIOUS/NEXT pointer from the existing instruction.
		*/
		psEfoInst->sStageData.psEfoData->psPrevWriter = psInstB->sStageData.psEfoData->psPrevWriter;
		psEfoInst->sStageData.psEfoData->psNextWriter = psInstB->sStageData.psEfoData->psNextWriter;

		/*
			Replace the NEXT pointer in the instruction before the existing instruction.
		*/
		ASSERT(psInstB->sStageData.psEfoData->psPrevWriter->sStageData.psEfoData->psNextWriter == psInstB);
		psInstB->sStageData.psEfoData->psPrevWriter->sStageData.psEfoData->psNextWriter = psEfoInst;

		/*
			Replace the PREVIOUS pointer in the instruction after the existing instruction.
		*/
		ASSERT(psInstB->sStageData.psEfoData->psNextWriter->sStageData.psEfoData->psPrevWriter == psInstB);
		psInstB->sStageData.psEfoData->psNextWriter->sStageData.psEfoData->psPrevWriter = psEfoInst;
	}
	psEfoInst->sStageData.psEfoData->uEfoGroupId = uNewEfoGroupId;

	/*
		Drop old instructions.
	*/
	DropInst(psState, psInstB);
	DropInst(psState, psInstA);
}

/*****************************************************************************
 FUNCTION	: TryEfoCombineExtraIRegsMULMUL
    
 PURPOSE	: Try to combine two MUL instructions into a single EFO by changing
			  some earlier instructions to write to the internal registers.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  psBlock			- The block containing the two instructions.
			  uInstA			- The first instruction in the pair.
			  psInstA
			  uInstB			- The second instruction in the pair.
			  psInstB
			  
 RETURNS	: TRUE if the two instructions were combined into an EFO.
*****************************************************************************/
static IMG_BOOL TryEfoCombineExtraIRegsMULMUL(PINTERMEDIATE_STATE	psState,
											  PEFOGEN_STATE			psEfoState,
											  PCODEBLOCK			psBlock,
											  IMG_UINT32			uInstA,
											  PINST					psInstA,
											  IMG_UINT32			uInstB,
											  PINST					psInstB)
{
	IMG_UINT32	uIdx;
	IMG_UINT32	uI0Src, uI1Src;

	/*
		Which sources from the EFO are multiplied by the internal registers.
	*/
	if (psEfoState->bNewEfoFeature)
	{
		uI0Src = 1;
		uI1Src = 2;
	}
	else
	{
		uI0Src = 1;
		uI1Src = 0;
	}

	if (SameRegister(&psInstA->asArg[0], &psInstA->asArg[1]) ||
		SameRegister(&psInstB->asArg[0], &psInstB->asArg[1]))
	{
		return IMG_FALSE;
	}

	for (uIdx = 0; uIdx < 4; uIdx++)
	{
		IMG_UINT32	uAArg, uBArg;
		IMG_UINT32	uAIReg;
		IMG_UINT32	uAWriterInst, uBWriterInst;
		PINST		psAWriterInst, psBWriterInst;
		PINST		psEfoInst;
		IMG_UINT32	uEfoInst;

		switch (uIdx)
		{
			case 0: uAArg = 0; uBArg = 0; break;
			case 1: uAArg = 0; uBArg = 1; break;
			case 2: uAArg = 1; uBArg = 0; break;
			case 3: uAArg = 1; uBArg = 1; break;
			default: imgabort();
		}
				
		/*
			Check the two sources we aren't replacing by internal registers
			can be sources to an EFO.
		*/
		if (CanUseEfoSrc(psState, psEfoState->psCodeBlock, uI1Src, psInstA, 1 - uAArg) &&
			CanUseEfoSrc(psState, psEfoState->psCodeBlock, uI0Src, psInstB, 1 - uBArg))
		{
			uAIReg = 1;
		}
		else if (CanUseEfoSrc(psState, psEfoState->psCodeBlock, uI0Src, psInstA, 1 - uAArg) &&
				 CanUseEfoSrc(psState, psEfoState->psCodeBlock, uI1Src, psInstB, 1 - uBArg))
		{
			uAIReg = 0;
		}
		else
		{
			continue;
		}

		/*
			No source modifiers for arguments that come from the internal
			registers.
		*/
		if (HasNegateOrAbsoluteModifier(psState, psInstA, uAArg) ||
			HasNegateOrAbsoluteModifier(psState, psInstB, uBArg))
		{
			continue;
		}

		/*
			Check the other two sources can be replaced by internal registers.
		*/
		if (!CanReplaceSourceByIReg(psState, 
									psEfoState, 
									psBlock, 
									uInstA, 
									psInstA,
									uInstB, 
									psInstB,
									&psInstA->asArg[uAArg], 
									&uAWriterInst,
									&psAWriterInst))
		{
			continue;
		}
		if (!CanReplaceSourceByIReg(psState, 
									psEfoState, 
									psBlock, 
									uInstB, 
									psInstA,
									uInstA, 
									psInstB,
									&psInstB->asArg[uBArg], 
									&uBWriterInst,
									&psBWriterInst))
		{
			continue;
		}
		if (GraphGet(psState, psEfoState->psCodeBlock->psDepState->psClosedDepGraph, uAWriterInst, uBWriterInst))
		{
			continue;
		}

		/*
			Check that combining the two instructions would interfere with 
			other uses of the internal registers.
		*/
		if (WouldBeInterveningIRegWrite(psState, psEfoState, uInstA, uInstB))
		{
			continue;
		}
		if (!CheckEfoGroupDependency(psState, psEfoState, uInstA, uInstB, USC_UNDEF /* uEfoGroupId */))
		{
			continue;
		}
		
		/*
			Create an EFO instruction.
		*/
		psEfoInst = CreateEfoInst(psState, psBlock, uInstA, uInstB, &uEfoInst, psInstA);

		/*
			SGX535:
				M0 = SRC1 * I0
				M1 = SRC0 * I1
			SGX545:
				M0 = SRC1 * I0
				M1 = SRC2 * I1
		*/
		psEfoInst->u.psEfo->eM0Src0 = (EFO_SRC)(SRC0 + uI0Src);
		psEfoInst->u.psEfo->eM0Src1 = I0;
		psEfoInst->u.psEfo->eM1Src0 = (EFO_SRC)(SRC0 + uI1Src);
		psEfoInst->u.psEfo->eM1Src1 = I1;

		/*
			I0 = M0 / I1 = M1
		*/
		SetupIRegDests(psState, psEfoInst, M0, M1);

		/*
			Copy the sources we aren't replacing by internal registers to the EFO.
		*/
		if (uAIReg == 0)
		{
			MoveFloatArgToEfo(psState, psEfoInst, uI0Src, psInstA, 1 - uAArg);
			MoveFloatArgToEfo(psState, psEfoInst, uI1Src, psInstB, 1 - uBArg);
		}
		else
		{
			MoveFloatArgToEfo(psState, psEfoInst, uI0Src, psInstB, 1 - uBArg);
			MoveFloatArgToEfo(psState, psEfoInst, uI1Src, psInstA, 1 - uAArg);
		}

		/*
			Write the EFO results to the destinations of the two instructions we
			are combining.
		*/
		if (uAIReg == 0)
		{
			MoveDest(psState, psEfoInst, EFO_USFROMI0_DEST, psInstA, 0 /* uMoveFromDestIdx */);
			MoveDest(psState, psEfoInst, EFO_USFROMI1_DEST, psInstB, 0 /* uMoveFromDestIdx */);
		}
		else
		{
			MoveDest(psState, psEfoInst, EFO_USFROMI0_DEST, psInstB, 0 /* uMoveFromDestIdx */);
			MoveDest(psState, psEfoInst, EFO_USFROMI1_DEST, psInstA, 0 /* uMoveFromDestIdx */);
		}

		/*
			Not using the destination yet.
		*/
		psEfoInst->u.psEfo->bIgnoreDest = IMG_TRUE;

		/*
			Set up the rest of the EFO state.
		*/
		SetupEfoInstExtraIRegs(psState,
							   psEfoState,
							   psInstA,
							   psInstB,
							   uAIReg,
							   psEfoInst,
							   uAWriterInst,
							   psAWriterInst,
							   uBWriterInst,
							   psBWriterInst);

		return IMG_TRUE;
	}
	return IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: TryEfoCombineExtraIRegsMADMUL
    
 PURPOSE	: Try to combine a MAD and MUL instruction into a single EFO by changing
			  some earlier instructions to write to the internal registers.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  psBlock			- The block containing the two instructions.
			  uInstA			- The first instruction in the pair.
			  psInstA
			  uInstB			- The second instruction in the pair.
			  psInstB
			  
 RETURNS	: TRUE if the two instructions were combined into an EFO.
*****************************************************************************/
static IMG_BOOL TryEfoCombineExtraIRegsMADMUL(PINTERMEDIATE_STATE	psState,
											  PEFOGEN_STATE			psEfoState,
											  PCODEBLOCK			psBlock,
											  IMG_UINT32			uInstA,
											  PINST					psInstA,
											  IMG_UINT32			uInstB,
											  PINST					psInstB)
{
	IMG_UINT32	uAArgIdx, uBArgIdx;
	IMG_UINT32	uCommonSrc, uI0Multiply, uI1Multiply;
	IMG_BOOL	bCanWriteI0, bCanWriteI1;

	/*
		If INSTB is already writing an internal register check if we can
		change INSTB to write it using one of the implicit EFO
		destination.
	*/
	ASSERT(psInstA->asDest[0].uType != USEASM_REGTYPE_FPINTERNAL);
	if (psInstB->asDest[0].uType == USEASM_REGTYPE_FPINTERNAL &&
		psInstB->asDest[0].uNumber >= NUM_INTERNAL_EFO_REGISTERS)
	{
		return IMG_FALSE;
	}

	if (psEfoState->bNewEfoFeature)
	{
		uCommonSrc = 2;
		uI0Multiply = 0;
		uI1Multiply = 1;
	}
	else
	{
		uCommonSrc = 0;
		uI0Multiply = 1;
		uI1Multiply = 2;
	}

	bCanWriteI0 = bCanWriteI1 = IMG_TRUE;
	/*
		Is the MUL instruction already writing to an internal register?
	*/
	if (psInstB->asDest[0].uType == USEASM_REGTYPE_FPINTERNAL)
	{
		if (psInstB->sStageData.psEfoData->psPrevWriter != NULL)
		{
			return IMG_FALSE;
		}
		/*
			The new EFO instruction must still write the MUL result to
			the same internal register.
		*/
		if (psInstB->asDest[0].uNumber == 0)
		{
			bCanWriteI0 = IMG_FALSE;
		}
		if (psInstB->asDest[0].uNumber == 1)
		{
			bCanWriteI1 = IMG_FALSE;
		}
	}

	for (uAArgIdx = 0; uAArgIdx < 2; uAArgIdx++)
	{
		for (uBArgIdx = 0; uBArgIdx < 2; uBArgIdx++)
		{
			PINST		psEfoInst;
			IMG_UINT32	uAWriterInst;
			PINST		psAWriterInst;
			IMG_UINT32	uEfoInst;
			IMG_UINT32	uArgIdx;
			IMG_UINT32	uAIReg;
			IMG_BOOL	bA0RightNegate = IMG_FALSE;
			IMG_BOOL	bNegateAWriterResult = IMG_FALSE;
			IMG_BOOL	bCanWriteI0Here;

			/*
				Check for a common argument between the two instructions.
			*/
			if (!EqualFloatSrcs(psState, psInstA, uAArgIdx, psInstB, uBArgIdx))
			{
				continue;
			}

			/*
				Check the common argument can be a source to an EFO.
			*/
			if (!CanUseEfoSrc(psState, psEfoState->psCodeBlock, uCommonSrc, psInstA, uAArgIdx))
			{
				continue;
			}

			/*
				Check the other two sources can be replaced by internal registers.
			*/
			if (IsAbsolute(psState, psInstA, 2 /* uArgIdx */))
			{
				continue;
			}
			if (!CanReplaceSourceByIReg(psState, 
										psEfoState, 
										psBlock, 
										uInstA, 
										psInstA,
										uInstB, 
										psInstB,
										&psInstA->asArg[2], 
										&uAWriterInst,
										&psAWriterInst))
			{
				continue;
			}

			/*
				If the source we are replacing by an internal register has a negate modifier
				then we can only use i0 when either

				(a) We have the new EFO features which allow a negate modifier on an implicit
				i0 source
				(b) We can change the instruction which writes the internal register to 
				negate its result.
			*/
			bCanWriteI0Here = bCanWriteI0;
			if (IsNegated(psState, psInstA, 2 /* uArgIdx */))
			{
				if (
						!(
							psEfoState->bNewEfoFeature ||
							psAWriterInst->eOpcode == IFMUL
						)
				   )
				{
					bCanWriteI0Here = IMG_FALSE;
				}
			}

			/*
				Check the other two arguments can be sources to an EFO.	
			*/
			if (
					bCanWriteI0Here &&
					CanUseEfoSrc(psState, psEfoState->psCodeBlock, uI0Multiply, psInstA, 1 - uAArgIdx) &&
					CanUseEfoSrc(psState, psEfoState->psCodeBlock, uI1Multiply, psInstB, 1 - uBArgIdx)
			   )
			{
				uAIReg = 0;
			}
			else if (
						bCanWriteI1 &&
						CanUseEfoSrc(psState, psEfoState->psCodeBlock, uI1Multiply, psInstA, 1 - uAArgIdx) &&
						CanUseEfoSrc(psState, psEfoState->psCodeBlock, uI0Multiply, psInstB, 1 - uBArgIdx)
					)
			{
				uAIReg = 1;
			}
			else
			{
				continue;
			}

			if (uAIReg == 0)
			{
				if (psInstA->u.psFloat->asSrcMod[2].bNegate)
				{
					if (psEfoState->bNewEfoFeature)
					{
						/*
							Apply the negate modifier to the source we are replacing
							by an internal register using the EFO A0 right negate flag.
						*/
						bA0RightNegate = IMG_TRUE;
					}
					else
					{
						/*
							Apply the negate modifier to the source we are replacing
							by changing the negate modifiers on the instruction which
							writes it.
						*/
						bNegateAWriterResult = IMG_TRUE;
					}
				}
			}

			/*
				Check that combining the two instructions would interfere with 
				other uses of the internal registers.
			*/
			if (WouldBeInterveningIRegWrite(psState, psEfoState, uInstA, uInstB))
			{
				continue;
			}
			if (!CheckEfoGroupDependency(psState, psEfoState, uInstA, uInstB, psInstB->sStageData.psEfoData->uEfoGroupId))
			{
				continue;
			}
			
			/*
				Create an EFO instruction.
			*/
			psEfoInst = CreateEfoInst(psState, psBlock, uInstA, uInstB, &uEfoInst, psInstA);

			/*
				SGX535:
					M0 = SRC0 * SRC1
					M1 = SRC0 * SRC2
				SGX545:
					M0 = SRC0 * SRC2
					M1 = SRC1 * SRC2
			*/
			if (psEfoState->bNewEfoFeature)
			{
				psEfoInst->u.psEfo->eM0Src0 = SRC0;
				psEfoInst->u.psEfo->eM0Src1 = SRC2;
				psEfoInst->u.psEfo->eM1Src0 = SRC1;
				psEfoInst->u.psEfo->eM1Src1 = SRC2;
			}
			else
			{
				psEfoInst->u.psEfo->eM0Src0 = SRC0;
				psEfoInst->u.psEfo->eM0Src1 = SRC1;
				psEfoInst->u.psEfo->eM1Src0 = SRC0;
				psEfoInst->u.psEfo->eM1Src1 = SRC2;
			}

			if (uAIReg == 0)
			{
				/*
					A0 = M0 + I0
				*/
				psEfoInst->u.psEfo->eA0Src0 = M0;
				psEfoInst->u.psEfo->eA0Src1 = I0;
			}
			else
			{
				/*
					A1 = I1 + M1
				*/
				psEfoInst->u.psEfo->eA1Src0 = I1;
				psEfoInst->u.psEfo->eA1Src1 = M1;
			}

			if (uAIReg == 0)
			{
				/*
					I0 = A0 / I1 = M1
				*/
				SetupIRegDests(psState, psEfoInst, A0, M1);
			}
			else
			{
				/*
					I0 = M0
				*/
				SetupIRegDests(psState, psEfoInst, M0, EFO_SRC_UNDEF);
			}

			/*
				Copy source modifiers on the internal registers.
			*/
			if (uAIReg == 0)
			{
				psEfoInst->u.psEfo->bA0RightNeg = bA0RightNegate;
			}
			else
			{
				psEfoInst->u.psEfo->bA1LeftNeg = IsNegated(psState, psInstA, 2 /* uArgIdx */);
			}
			ASSERT(psEfoState->bNewEfoFeature || !psEfoInst->u.psEfo->bA0RightNeg);

			/*
				Copy the non-common sources which are replaced by internal
				registers to the EFO.
			*/
			if (uAIReg == 0)
			{
				MoveFloatArgToEfo(psState, psEfoInst, uI0Multiply, psInstA, 1 - uAArgIdx);
				MoveFloatArgToEfo(psState, psEfoInst, uI1Multiply, psInstB, 1 - uBArgIdx);
			}
			else
			{
				MoveFloatArgToEfo(psState, psEfoInst, uI0Multiply, psInstB, 1 - uBArgIdx);
				MoveFloatArgToEfo(psState, psEfoInst, uI1Multiply, psInstA, 1 - uAArgIdx);
			}

			/*
				Copy the common source to the EFO.
			*/
			MoveFloatArgToEfo(psState, psEfoInst, uCommonSrc, psInstA, uAArgIdx);

			/*
				Replace other sources that are the same as the sources we
				replaced by internal registers.
			*/
			for (uArgIdx = 0; uArgIdx < 3; uArgIdx++)
			{
				PARG	psEfoSrc = &psEfoInst->asArg[uArgIdx];

				if (SameRegister(psEfoSrc, &psInstA->asArg[2]))
				{
					SetSrc(psState, psEfoInst, uArgIdx, USEASM_REGTYPE_FPINTERNAL, uAIReg, UF_REGFORMAT_F32);
				}
			}

			/*
				Write the EFO results to the destinations of the two instructions we
				are combining.
			*/
			if (uAIReg == 0)
			{
				MoveDest(psState, psEfoInst, EFO_USFROMI0_DEST, psInstA, 0 /* uMoveFromDestIdx */);

				if (psInstB->asDest[0].uType != USEASM_REGTYPE_FPINTERNAL)
				{
					MoveDest(psState, psEfoInst, EFO_USFROMI1_DEST, psInstB, 0 /* uMoveFromDestIdx */);
				}
				else
				{
					ASSERT(psInstB->asDest[0].uNumber == 1);
				}

				psEfoInst->u.psEfo->bIgnoreDest = IMG_TRUE;
			}
			else
			{
				if (psInstB->asDest[0].uType != USEASM_REGTYPE_FPINTERNAL)
				{
					MoveDest(psState, psEfoInst, EFO_USFROMI0_DEST, psInstB, 0 /* uMoveFromDestIdx */);
				}
				else
				{
					ASSERT(psInstB->asDest[0].uNumber == 0);
				}

				psEfoInst->u.psEfo->bIgnoreDest = IMG_FALSE;
				psEfoInst->u.psEfo->eDSrc = A1;
				MoveDest(psState, psEfoInst, EFO_US_DEST, psInstA, 0 /* uMoveFromDestIdx */);
			}

			/*
				Change the instruction which writes the internal register to negate
				its result.
			*/
			if (bNegateAWriterResult)
			{
				InvertNegateModifier(psState, psAWriterInst, 0 /* uArgIdx */);
			}

			/*
				Set up the rest of the EFO state.
			*/
			SetupEfoInstExtraIRegs(psState,
								   psEfoState,
								   psInstA,
								   psInstB,
								   uAIReg,
								   psEfoInst,
								   uAWriterInst,
								   psAWriterInst,
								   UINT_MAX,
								   NULL);

			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: TryEfoCombineExtraIRegsMADMAD
    
 PURPOSE	: Try to combine two MAD instructions into a single EFO by changing
			  some earlier instructions to write to the internal registers.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  psBlock			- The block containing the two instructions.
			  uInstA			- The first instruction in the pair.
			  psInstA
			  uInstB			- The second instruction in the pair.
			  psInstB
			  
 RETURNS	: TRUE if the two instructions were combined into an EFO.
*****************************************************************************/
static IMG_BOOL TryEfoCombineExtraIRegsMADMAD(PINTERMEDIATE_STATE	psState,
											  PEFOGEN_STATE			psEfoState,
											  PCODEBLOCK			psBlock,
											  IMG_UINT32			uInstA,
											  PINST					psInstA,
											  IMG_UINT32			uInstB,
											  PINST					psInstB)
{
	IMG_UINT32	uAArgIdx, uBArgIdx;
	IMG_UINT32	uCommonSrc, uI0Multiply, uI1Multiply;

	if (psEfoState->bNewEfoFeature)
	{
		uCommonSrc = 2;
		uI0Multiply = 0;
		uI1Multiply = 1;
	}
	else
	{
		uCommonSrc = 0;
		uI0Multiply = 1;
		uI1Multiply = 2;
	}

	for (uAArgIdx = 0; uAArgIdx < 2; uAArgIdx++)
	{
		for (uBArgIdx = 0; uBArgIdx < 2; uBArgIdx++)
		{
			IMG_UINT32	uAIReg;
			PINST		psEfoInst;
			IMG_UINT32	uAWriterInst, uBWriterInst;
			PINST		psAWriterInst, psBWriterInst;
			IMG_UINT32	uEfoInst;
			IMG_UINT32	uArgIdx;

			/*
				Check for a common argument between the two instructions.
			*/
			if (!EqualFloatSrcs(psState, psInstA, uAArgIdx, psInstB, uBArgIdx))
			{
				continue;
			}

			/*
				Check the common argument can be a source to an EFO.
			*/
			if (!CanUseEfoSrc(psState, psEfoState->psCodeBlock, uCommonSrc, psInstA, uAArgIdx))
			{
				continue;
			}

			/*
				Check the other two arguments can be sources to an EFO.	
			*/
			if (
					/* Without the new EFO features negate is only available on I1 */
					(
						psEfoState->bNewEfoFeature ||
						!psInstA->u.psFloat->asSrcMod[2].bNegate
					) &&
					CanUseEfoSrc(psState, psEfoState->psCodeBlock, uI0Multiply, psInstA, 1 - uAArgIdx) &&
					CanUseEfoSrc(psState, psEfoState->psCodeBlock, uI1Multiply, psInstB, 1 - uBArgIdx)
			   )
			{
				uAIReg = 0;
			}
			else if (
						/* Without the new EFO features negate is only available on I1 */
						(
							psEfoState->bNewEfoFeature ||
							!psInstA->u.psFloat->asSrcMod[2].bNegate
						) &&
						CanUseEfoSrc(psState, psEfoState->psCodeBlock, uI1Multiply, psInstA, 1 - uAArgIdx) &&
						CanUseEfoSrc(psState, psEfoState->psCodeBlock, uI0Multiply, psInstB, 1 - uBArgIdx)
					)
			{
				uAIReg = 1;
			}
			else
			{
				continue;
			}

			/*
				No absolute source modifiers on sources replaced by internal registers.
			*/
			if (IsAbsolute(psState, psInstA, 2 /* uArgIdx */) || IsAbsolute(psState, psInstB, 2 /* uArgIdx */))
			{
				continue;
			}

			/*
				Check the other two sources can be replaced by internal registers.
			*/
			if (!CanReplaceSourceByIReg(psState, 
										psEfoState, 
										psBlock, 
										uInstA, 
										psInstA,
										uInstB, 
										psInstB,
										&psInstA->asArg[2], 
										&uAWriterInst,
										&psAWriterInst))
			{
				continue;
			}
			if (!CanReplaceSourceByIReg(psState, 
										psEfoState, 
										psBlock, 
										uInstB, 
										psInstB,
										uInstA, 
										psInstA,
										&psInstB->asArg[2], 
										&uBWriterInst,
										&psBWriterInst))
			{
				continue;
			}
			if (GraphGet(psState, psEfoState->psCodeBlock->psDepState->psClosedDepGraph, uAWriterInst, uBWriterInst))
			{
				continue;
			}

			/*
				Check that combining the two instructions would interfere with 
				other uses of the internal registers.
			*/
			if (WouldBeInterveningIRegWrite(psState, psEfoState, uInstA, uInstB))
			{
				continue;
			}
			if (!CheckEfoGroupDependency(psState, psEfoState, uInstA, uInstB, USC_UNDEF /* uEfoGroupId */))
			{
				continue;
			}
			
			/*
				Create an EFO instruction.
			*/
			psEfoInst = CreateEfoInst(psState, psBlock, uInstA, uInstB, &uEfoInst, psInstA);

			/*
				SGX535:
					M0 = SRC0 * SRC1
					M1 = SRC0 * SRC2
				SGX545:
					M0 = SRC0 * SRC2
					M1 = SRC1 * SRC2
			*/
			if (psEfoState->bNewEfoFeature)
			{
				psEfoInst->u.psEfo->eM0Src0 = SRC0;
				psEfoInst->u.psEfo->eM0Src1 = SRC2;
				psEfoInst->u.psEfo->eM1Src0 = SRC1;
				psEfoInst->u.psEfo->eM1Src1 = SRC2;
			}
			else
			{
				psEfoInst->u.psEfo->eM0Src0 = SRC0;
				psEfoInst->u.psEfo->eM0Src1 = SRC1;
				psEfoInst->u.psEfo->eM1Src0 = SRC0;
				psEfoInst->u.psEfo->eM1Src1 = SRC2;
			}

			/*
				A0 = M0 + I0
				A1 = I1 + M1
			*/
			psEfoInst->u.psEfo->eA0Src0 = M0;
			psEfoInst->u.psEfo->eA0Src1 = I0;
			psEfoInst->u.psEfo->eA1Src0 = I1;
			psEfoInst->u.psEfo->eA1Src1 = M1;

			/*
				I0 = A0 / I1 = A1
			*/
			SetupIRegDests(psState, psEfoInst, A0, A1);

			/*
				Copy source modifiers on the internal registers.
			*/
			if (uAIReg == 0)
			{
				psEfoInst->u.psEfo->bA1LeftNeg = psInstB->u.psFloat->asSrcMod[2].bNegate;
				psEfoInst->u.psEfo->bA0RightNeg = psInstA->u.psFloat->asSrcMod[2].bNegate;
			}
			else
			{
				psEfoInst->u.psEfo->bA1LeftNeg = psInstA->u.psFloat->asSrcMod[2].bNegate;
				psEfoInst->u.psEfo->bA0RightNeg = psInstB->u.psFloat->asSrcMod[2].bNegate;
			}
			ASSERT(psEfoState->bNewEfoFeature || !psEfoInst->u.psEfo->bA0RightNeg);

			/*
				Copy the non-common sources which are replaced by internal
				registers to the EFO.
			*/
			if (uAIReg == 0)
			{
				MoveFloatArgToEfo(psState, psEfoInst, uI0Multiply, psInstA, 1 - uAArgIdx);
				MoveFloatArgToEfo(psState, psEfoInst, uI1Multiply, psInstB, 1 - uBArgIdx);
			}
			else
			{
				MoveFloatArgToEfo(psState, psEfoInst, uI0Multiply, psInstB, 1 - uBArgIdx);
				MoveFloatArgToEfo(psState, psEfoInst, uI1Multiply, psInstA, 1 - uAArgIdx);
			}

			/*
				Copy the common source to the EFO.
			*/
			MoveFloatArgToEfo(psState, psEfoInst, uCommonSrc, psInstA, uAArgIdx);

			/*
				Replace other sources that are the same as the sources we
				replaced by internal registers.
			*/
			for (uArgIdx = 0; uArgIdx < EFO_UNIFIEDSTORE_SOURCE_COUNT; uArgIdx++)
			{
				PARG	psEfoSrc = &psEfoInst->asArg[uArgIdx];

				if (SameRegister(psEfoSrc, &psInstA->asArg[2]))
				{
					SetSrc(psState, psEfoInst, uArgIdx, USEASM_REGTYPE_FPINTERNAL, uAIReg, UF_REGFORMAT_F32);
				}
				if (SameRegister(psEfoSrc, &psInstB->asArg[2]))
				{
					SetSrc(psState, psEfoInst, uArgIdx, USEASM_REGTYPE_FPINTERNAL, 1 - uAIReg, UF_REGFORMAT_F32);
				}
			}

			/*
				Write the EFO results to the destinations of the two instructions we
				are combining.
			*/
			if (uAIReg == 0)
			{
				MoveDest(psState, psEfoInst, EFO_USFROMI0_DEST, psInstA, 0 /* uMoveFromDestIdx */);
				MoveDest(psState, psEfoInst, EFO_USFROMI1_DEST, psInstB, 0 /* uMoveFromDestIdx */);
			}
			else
			{
				MoveDest(psState, psEfoInst, EFO_USFROMI0_DEST, psInstB, 0 /* uMoveFromDestIdx */);
				MoveDest(psState, psEfoInst, EFO_USFROMI1_DEST, psInstA, 0 /* uMoveFromDestIdx */);
			}

			/*
				Not using the destination yet.
			*/
			psEfoInst->u.psEfo->bIgnoreDest = IMG_TRUE;

			/*
				Set up the rest of the EFO state.
			*/
			SetupEfoInstExtraIRegs(psState,
								   psEfoState,
								   psInstA,
								   psInstB,
								   uAIReg,
								   psEfoInst,
								   uAWriterInst,
								   psAWriterInst,
								   uBWriterInst,
								   psBWriterInst);

			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL TryEfoCombineExtraIRegs(PINTERMEDIATE_STATE	psState,
										PEFOGEN_STATE		psEfoState,
										PCODEBLOCK			psBlock,
										PINST				psInstA,
										PINST				psInstB)
/*****************************************************************************
 FUNCTION	: TryEfoCombineExtraIRegs
    
 PURPOSE	: Try to combine two instructions into a single EFO by changing
			  some earlier instructions to write to the internal registers.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  psBlock			- The block containing the two instructions.
			  psInstA			- The first instruction in the pair.
			  psInstB			- The second instruction in the pair.
			  
 RETURNS	: TRUE if the two instructions were combined into an EFO.
*****************************************************************************/
{
	IMG_UINT32	uInstA = psInstA->uId;
	IMG_UINT32	uInstB = psInstB->uId;

	/*
		Check the two instructions are unpredicated multiplies and neither is
		dependent on the other.
	*/
	if (!NoPredicate(psState, psInstA) ||
		!NoPredicate(psState, psInstB) ||
		psInstB->uDestCount != 1 ||
		GraphGet(psState, psBlock->psDepState->psClosedDepGraph, uInstA, uInstB) ||
		GraphGet(psState, psBlock->psDepState->psClosedDepGraph, uInstB, uInstA))
	{
		return IMG_FALSE;
	}
	if (psInstA->asDest[0].uIndexType != USC_REGTYPE_NOINDEX)
	{
		return IMG_FALSE;
	}
	if (psInstB->asDest[0].uIndexType != USC_REGTYPE_NOINDEX)
	{
		return IMG_FALSE;
	}

	/*
		Check for two MULs.
	*/
	if (psInstA->eOpcode == IFMUL &&
		psInstA->sStageData.psEfoData->uEfoGroupId == USC_UNDEF &&
		psInstB->eOpcode == IFMUL &&
		psInstB->sStageData.psEfoData->uEfoGroupId == USC_UNDEF)
	{
		if (TryEfoCombineExtraIRegsMULMUL(psState, psEfoState, psBlock, uInstA, psInstA, uInstB, psInstB))
		{
			return IMG_TRUE;
		}
	}

	/*
		Check for two MADs with a common source 
	*/
	if (psInstA->eOpcode == IFMAD &&
		psInstA->sStageData.psEfoData->uEfoGroupId == USC_UNDEF &&
		psInstB->eOpcode == IFMAD &&
		psInstB->sStageData.psEfoData->uEfoGroupId == USC_UNDEF)
	{
		if (TryEfoCombineExtraIRegsMADMAD(psState, psEfoState, psBlock, uInstA, psInstA, uInstB, psInstB))
		{
			return IMG_TRUE;
		}
	}

	/*
		Check for a MAD and a MUL with a common source.
	*/
	if (psInstA->eOpcode == IFMAD &&
		psInstA->sStageData.psEfoData->uEfoGroupId == USC_UNDEF &&
		psInstB->eOpcode == IFMUL)
	{
		if (TryEfoCombineExtraIRegsMADMUL(psState, psEfoState, psBlock, uInstA, psInstA, uInstB, psInstB))
		{
			return IMG_TRUE;
		}
	}
	if (psInstA->eOpcode == IFMUL &&
		psInstB->eOpcode == IFMAD &&
		psInstB->sStageData.psEfoData->uEfoGroupId == USC_UNDEF)
	{
		if (TryEfoCombineExtraIRegsMADMUL(psState, psEfoState, psBlock, uInstB, psInstB, uInstA, psInstA))
		{
			return IMG_TRUE;
		}
	}

	return IMG_FALSE;
}

static IMG_BOOL GenerateEfoFromCombinedInstructionsExtraIRegs(PINTERMEDIATE_STATE	psState,
															  PEFOGEN_STATE			psEfoState,
															  PCODEBLOCK			psBlock,
															  PINST					psInstA,
															  PINST*				ppsNextInstA)
/*****************************************************************************
 FUNCTION	: GenerateEfoFromCombinedInstructionsExtraIRegs
    
 PURPOSE	: Try to find other instructions to combine with a specified
			  instruction into an EFO by changing some previous instructions
			  to write to internal registers.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  psBlock			- Basic block containing the instruction.
			  psInstA			- First part of the pair of instructions to combine.
			  
 RETURNS	: TRUE if a new EFO was generated.
*****************************************************************************/
{
	PINST			psInstB;

	/*
		Check some basic conditions on the first instruction.
	*/
	if (psInstA->uDestCount != 1 || !NoPredicate(psState, psInstA))
	{
		return IMG_FALSE;
	}
	if (psInstA->asDest[0].uIndexType != USC_REGTYPE_NOINDEX)
	{
		return IMG_FALSE;
	}
	
	/*
		Check for combining with all the remaining instructions in the block.
	*/
	for (psInstB = psInstA->psNext; psInstB != NULL; psInstB = psInstB->psNext)
	{
		PINST	psNextInstB = psInstB->psNext;

		if (psState->uMaxInstMovement != USC_MAXINSTMOVEMENT_NOLIMIT &&
			psInstB->uBlockIndex - psInstA->uBlockIndex > psState->uMaxInstMovement)
		{
			return IMG_FALSE;
		}

		if (TryEfoCombineExtraIRegs(psState, psEfoState, psBlock, psInstA, psInstB))
		{
			if (*ppsNextInstA == psInstB)
			{
				*ppsNextInstA = psNextInstB;
			}
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL GenerateEfoFromCombinedInstructions(PINTERMEDIATE_STATE	psState,
													PEFOGEN_STATE		psEfoState,
													PCODEBLOCK			psBlock,
													PINST				psInstA,
													PINST*				ppsNextInstA)
/*****************************************************************************
 FUNCTION	: GenerateEfoFromCombinedInstructions
    
 PURPOSE	: Try to find other instructions to combine with a specified
			  instruction into an EFO.

 PARAMETERS	: psState			- Compiler state.
			  psEfoState		- Module state.
			  psBlock			- Basic block containing the instruction.
			  psInstA			- First part of the pair of instructions to combine.
			  
 RETURNS	: TRUE if a new EFO was generated.
*****************************************************************************/
{
	PINST	psInstB;

	/*
		Check some basic conditions on the first instruction.
	*/
	if (psInstA->sStageData.psEfoData->uEfoGroupId != USC_UNDEF ||
		psInstA->uDestCount != 1 || 
		!NoPredicate(psState, psInstA) ||
		!(TestInstructionGroup(psInstA->eOpcode, USC_OPT_GROUP_EFO_FORMATION)))
	{
		return IMG_FALSE;
	}
	if (psInstA->asDest[0].uIndexType != USC_REGTYPE_NOINDEX)
	{
		return IMG_FALSE;
	}

	/*
		Check for combining with all the remaining instructions in the block.
	*/
	for (psInstB = psInstA->psNext; psInstB != NULL; psInstB = psInstB->psNext)
	{
		PINST	psNextInstB = psInstB->psNext;
		
		if (psState->uMaxInstMovement != USC_MAXINSTMOVEMENT_NOLIMIT &&
			psInstB->uBlockIndex - psInstA->uBlockIndex > psState->uMaxInstMovement)
		{
			return IMG_FALSE;
		}

		if (TryEfoCombine(psState, psEfoState, psBlock, psInstA, psInstB))
		{
			if (*ppsNextInstA == psInstB)
			{
				*ppsNextInstA = psNextInstB;
			}

			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static
IMG_BOOL CreateEfoInstructions(PINTERMEDIATE_STATE	psState,
							   PCODEBLOCK			psBlock,
							   PEFOGEN_STATE		psEfoState)
/*****************************************************************************
 FUNCTION	: CreateEfoInstructions
    
 PURPOSE	: Initial stage of creating EFO instructions from pairs of other
			  instructions.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Basic block to optimize.
			  psEfoState		- Module state.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_BOOL	bChanges;
	IMG_BOOL	bEfoInstructionsCreated;

	bEfoInstructionsCreated = IMG_FALSE;
#ifdef USC_OFFLINE
	do
#endif
	{
		PINST	psInstA;
		PINST	psNextInstA = IMG_NULL;

		bChanges = IMG_FALSE;
		for (psInstA = psBlock->psBody; psInstA != NULL; psInstA = psNextInstA)
		{
			psNextInstA = psInstA->psNext;

			/*
				Try to create an EFO from two independent instructions.
			*/
			if (GenerateEfoFromCombinedInstructions(psState, psEfoState, psBlock, psInstA, &psNextInstA))
			{
				bChanges = IMG_TRUE;
				bEfoInstructionsCreated = IMG_TRUE;
				continue;
			}

			/*
				Try to create an EFO from two instructions one of which uses the result of the other.
			*/
			if (GenerateEfosFromDependentInstructions(psState, psEfoState, psBlock, psInstA, &psNextInstA))
			{
				bChanges = IMG_TRUE;
				bEfoInstructionsCreated = IMG_TRUE;
			}
			
		}
#ifndef JIT
		/* Only do a single pass for JIT mode */
		for (psInstA = psBlock->psBody; psInstA != NULL; psInstA = psNextInstA)
#endif /* JIT */
		{
			/*
				Try to create an EFO from instruction by changing previous
				instructions to write to the internal register.
			*/
			if (GenerateEfoFromCombinedInstructionsExtraIRegs(psState, psEfoState,
															  psBlock, 
															  psInstA, &psNextInstA))
			{
				bChanges = IMG_TRUE;
				bEfoInstructionsCreated = IMG_TRUE;
			}
		}
	}
#ifdef USC_OFFLINE
	while (bChanges);
#endif

	return bEfoInstructionsCreated;
}

IMG_INTERNAL 
IMG_VOID GenerateEfoInstructionsBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNull)
/*****************************************************************************
 FUNCTION	: GenerateEfoInstructionsBP
    
 PURPOSE	: Try to optimize a basic block by combining basic arithmetic 
			  instructions into efo instructions.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Basic block to optimize.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST			psInstA;
	PINST			psEfoInst;
	PINST			psNextEfoInst;
	PEFOGEN_STATE	psEfoState;
	PDGRAPH_STATE	psDepState;

	PVR_UNREFERENCED_PARAMETER (pvNull);
	
	ASSERT(psBlock->psDepState == NULL);
// 	psBlock->psDepState = NewDGraphState(psState);
	

	psEfoState = UscAlloc(psState, sizeof(EFOGEN_STATE));
	psEfoState->uEfoGroupCount = 0;
	psEfoState->aauEfoDependencyGraph = NULL;
	psEfoState->aauClosedEfoDependencyGraph = NULL;
	psEfoState->asEfoGroup = NULL;
	psEfoState->psDeschedInstListHead = NULL;
	psEfoState->psDeschedInstListTail = NULL;
	psEfoState->psCodeBlock = psBlock;

	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NEW_EFO_OPTIONS)
		psEfoState->bNewEfoFeature = IMG_TRUE;
	else
		psEfoState->bNewEfoFeature = IMG_FALSE;

	psDepState = ComputeBlockDependencyGraph(psState, psBlock, IMG_TRUE);
	
	//TESTONLY(DotPrintDepGraph(psDepState, "pre-efo", IMG_FALSE));
	ComputeClosedDependencyGraph(psDepState, IMG_FALSE);

	/*
		Initialize the per-instruction data used in the EFO stage.
	*/
	for (psInstA = psBlock->psBody; psInstA != NULL; psInstA = psInstA->psNext)
	{
		//if(TestInstructionGroup(psInstA->eOpcode, USC_OPT_GROUP_EFO_FORMATION))
			SetupEfoStageData(psState, psEfoState, psInstA);
	}

	/*
		Set up information about existing uses of the internal registers.
	*/
	CreateExistingIRegGroups(psState, psBlock, psEfoState);

	/*
		Try to create EFOs.
	*/
	if (!CreateEfoInstructions(psState, psBlock, psEfoState))
	{
		/*
			No EFO instructions generated so return early.
		*/
		goto cleanup;
	}

	//TESTONLY(DotPrintDepGraph(psDepState, "mid-efo", IMG_FALSE));

	/*
		Set up information about which EFO groups depend on each other.
	*/
	SetupGroupDependencyGraph(psState, psEfoState);
	#if defined(EFO_TEST)
	TESTONLY_PRINT_INTERMEDIATE("EFO instructions before internal register moves removal", "efo_movelim", IMG_FALSE, IMG_FALSE);
	#endif /* defined(EFO_TEST) */
		
	for (psEfoInst = psBlock->psBody; psEfoInst != NULL; psEfoInst = psNextEfoInst)
	{
		IMG_UINT32	uEfoInst = psEfoInst->uId;
		IMG_UINT32	uIReg;
		PARG		psDest1;
		PARG		psDest2;

		psNextEfoInst = psEfoInst->psNext;

		if (psEfoInst->eOpcode != IEFO)
		{
			continue;
		}

		/*
			Get the unified store store destinations we are trying to write from the internal
			registers into.
		*/
		psDest1 = &psEfoInst->asDest[EFO_USFROMI0_DEST];
		psDest2 = &psEfoInst->asDest[EFO_USFROMI1_DEST];

		if (psDest1->uType != USC_REGTYPE_UNUSEDDEST && psDest2->uType != USC_REGTYPE_UNUSEDDEST)
		{
			/*
				If we have a case like this where a PCKU8F32 gets its sources from
				two different EFOs e.g.

					EFO	r0 = i0, r1 = i1
					EFO r2 = i0, r3 = i1
					PCKU8F32 o0.bytemask0011, r0, r2
					PCKU8F32 o0.bytemask1100, r1, r3

				then try and swap the pack sources so we get

					EFO	r0 = i0, r1 = i1
					EFO r2 = i0, r3 = i1
					PCKU8F32 o0.bytemask0101, r0, r1
					PCKU8F32 o0.bytemask1010, r2, r3
			*/
			TryReorderPacks(psState, psBlock, psEfoInst);
		}

		/*
			Try replacing both fake EFO destinations simultaneously.
		*/
		if (psDest1->uType != USC_REGTYPE_UNUSEDDEST && psDest2->uType != USC_REGTYPE_UNUSEDDEST)
		{
			PINST	psEfoDependencyInst;
			PINST	psDeschedDependencyInst;

			if (CanReplaceIRegMove(psState, 
								   psEfoState, 
								   psBlock,
								   psEfoInst,
								   uEfoInst,
								   psDest1, 
								   psDest2, 
								   &psEfoDependencyInst, 
								   &psDeschedDependencyInst))
			{
				EFO_TESTONLY(printf("\tReplacing (%d, %d) by internal registers.\n", psDest1->uNumber, psDest2->uNumber));

				ReplaceIRegMove(psState, 
								psBlock,
								psEfoState, 
								psEfoInst, 
								uEfoInst, 
								psDest1, 
								psDest2, 
								0, 
								psEfoDependencyInst, 
								psDeschedDependencyInst);

				SetDestUnused(psState, psEfoInst, EFO_USFROMI0_DEST);
				SetDestUnused(psState, psEfoInst, EFO_USFROMI1_DEST);
			}
		}

		/*
			Up to this point we've assumed that an EFO can write two non-internal registers. Remove
			this assumption trying not to introduce extra MOV instructions.
		*/
		for (uIReg = 0; uIReg < 2; uIReg++)
		{
			PARG psDest;
			EFO_SRC eISrc;
			PINST psEfoDependencyInst;
			PINST psDeschedDependencyInst;

			if (uIReg == 0)
			{
				psDest = psDest1;
				eISrc = psEfoInst->u.psEfo->eI0Src;
			}
			else
			{
				psDest = psDest2;
				eISrc = psEfoInst->u.psEfo->eI1Src;
			}

			if (psDest->uType == USC_REGTYPE_UNUSEDDEST)
			{
				continue;
			}

			/*
				Try and replace uses of the fake EFO destination by the corresponding internal register.
			*/
			if (CanReplaceIRegMove(psState, 
								   psEfoState,
								   psBlock,
 								   psEfoInst, 
								   uEfoInst, 
								   psDest, 
								   NULL, 
								   &psEfoDependencyInst, 
								   &psDeschedDependencyInst))
			{
				EFO_TESTONLY(printf("\tReplacing %d by internal registers.\n", psDest->uNumber));

				ReplaceIRegMove(psState, 
								psBlock,
								psEfoState, 
								psEfoInst,
								uEfoInst,
								psDest,
								NULL, 
								uIReg, 
								psEfoDependencyInst, 
								psDeschedDependencyInst);
					
				/*
					Flag the destination as now unwritten.
				*/
				SetDestUnused(psState, psEfoInst, EFO_USFROMI0_DEST + uIReg);
			}
			/*
				Try and write the fake destination directly from the EFO.
			*/
			else if (
						psEfoInst->u.psEfo->bIgnoreDest && 
						CanUseStandardDest(psDest) &&
						(
							(eISrc == A0 || eISrc == A1) ||
							(uIReg == 0 && ConvertM0ToA0(psState, psEfoState, psEfoInst, &eISrc))
						)
					)
			{
				EFO_TESTONLY(printf("\tUsing %d's writeback for %d.\n", psEfoInst->uId, psDest->uNumber));

				psEfoInst->u.psEfo->bIgnoreDest = IMG_FALSE;
				psEfoInst->u.psEfo->eDSrc = eISrc;
				MoveDest(psState, psEfoInst, 0 /* uMoveToDestIdx */, psEfoInst, EFO_USFROMI0_DEST + uIReg);
				SetDestUnused(psState, psEfoInst, EFO_USFROMI0_DEST + uIReg);

				/*
					Remove the write to an internal register if it was only copied into the unified
					store destination.
				*/
				if (!psEfoInst->sStageData.psEfoData->abIRegUsed[uIReg])
				{
					SetDestUnused(psState, psEfoInst, EFO_I0_DEST + uIReg);
					if (uIReg == 0)
					{
						psEfoInst->u.psEfo->bWriteI0 = IMG_FALSE;
					}
					else
					{
						psEfoInst->u.psEfo->bWriteI1 = IMG_FALSE;
					}
					SetupEfoUsage(psState, psEfoInst);
				}
			}
		}

		/*
			Before using an unrelated EFO to do a writeback check if it would be better just to
			remove the EFO altogether. We aren't saving any cycles if a MOV from the other
			internal register is still required and it blocks another EFO from using the
			same writeback.
		*/
		if (
				psDest1->uType != USC_REGTYPE_UNUSEDDEST &&
				psDest2->uType != USC_REGTYPE_UNUSEDDEST &&
				psEfoInst->u.psEfo->bIgnoreDest &&
				!psEfoInst->sStageData.psEfoData->abIRegUsed[0] &&
				!psEfoInst->sStageData.psEfoData->abIRegUsed[1] &&
				UnwindEfo(psState, psEfoState, psBlock, uEfoInst, psEfoInst, IMG_TRUE, IMG_TRUE) &&
				psEfoInst->sStageData.psEfoData->psNextWriter == NULL &&
				psEfoInst->sStageData.psEfoData->psFirstReader == NULL
		   )
		{
			EFO_TESTONLY(printf("\tUnwinding %d.\n", psEfoInst->uId));
			UnwindEfo(psState, psEfoState, psBlock, uEfoInst, psEfoInst, IMG_TRUE, IMG_FALSE);
		}
		else
		{
			IMG_UINT32	uIRegL;

			for (uIRegL = 0; uIRegL < 2; uIRegL++)
			{	
				IMG_UINT32 uDestIdx = EFO_USFROMI0_DEST + uIRegL;
				PARG psDest = &psEfoInst->asDest[uDestIdx];

				if (psDest->uType != USC_REGTYPE_UNUSEDDEST && CanUseStandardDest(psDest))
				{
					if (
							/*
								Look for another EFO which can write back a result from an internal register.
							*/
							WriteDestUsingAnotherEfo(psState, psEfoState, psBlock, psEfoInst, uDestIdx, uIRegL) ||
							/*
								Look for another instruction which can be replaced an EFO and used to
								do the writeback.
							*/
							WriteDestUsingNonEfo(psState, psBlock, psEfoState, psEfoInst, uDestIdx, uIRegL)
						)
					{
						/* Mark the destination as now not written. */
						SetDestUnused(psState, psEfoInst, EFO_USFROMI0_DEST + uIRegL);
					}
				}
			}
		}
	}

	/* 
		Reconstruct the block taking into account any new dependencies introduced when
		removing internal register moves.
	*/
	OutputInstructionsInGroupOrder(psState, psEfoState, psBlock, IMG_FALSE, IMG_TRUE);

	/* Remove unused internal register moves. */
	#if defined(EFO_TEST)
	TESTONLY_PRINT_INTERMEDIATE("EFO instructions after internal register moves removal", "efo_movelim", IMG_FALSE, IMG_FALSE);
	#endif /* defined(EFO_TEST) */
	//TESTONLY(DotPrintDepGraph(psDepState, "post-efo", IMG_FALSE));

cleanup:
	/* Release memory */
	UscFree(psState, psEfoState->asEfoGroup);
	UscFree(psState, psEfoState->aauEfoDependencyGraph);
	UscFree(psState, psEfoState->aauClosedEfoDependencyGraph);
	UscFree(psState, psEfoState);
	FreeBlockDGraphState(psState, psBlock);

	/*
		Free per-instruction data.
	*/
	{
		PINST	psInst;

		for (psInst = psBlock->psBody; psInst; psInst = psInst->psNext)
		{
			UscFree(psState, psInst->sStageData.psEfoData);
			psInst->sStageData.psEfoData = NULL;
		}
	}
}

/* EOF */
