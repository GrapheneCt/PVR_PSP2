/*****************************************************************************
 Name           : USEDEF.H
 
 Title          : USC USE/DEF information.
 
 C Author       : David Welch
 
 Created        : 21/11/2008
 
 Copyright      : 2002-2007 by Imagination Technologies Ltd.
                  All rights reserved. No part of this software, either
                  material or conceptual may be copied or distributed,
                  transmitted, transcribed, stored in a retrieval system or
                  translated into any human or computer language in any form
                  by any means, electronic, mechanical, manual or otherwise,
                  or disclosed to third parties without the express written
                  permission of Imagination Technologies Ltd,
                  Home Park Estate, Kings Langley, Hertfordshire,
                  WD4 8LZ, U.K.

 Program Type   : 32-bit DLL

 Version        : $Revision: 1.20 $

 Modifications  :

 $Log: usedef.h $ 
*****************************************************************************/

#ifndef __USC_USEDEF_H_
#define __USC_USEDEF_H_

typedef enum _USEDEF_CONTAINER_TYPE
{
	USEDEF_CONTAINER_TYPE_UNDEF,
	USEDEF_CONTAINER_TYPE_INST,
	USEDEF_CONTAINER_TYPE_BLOCK,
	USEDEF_CONTAINER_TYPE_FIXEDREG,
	USEDEF_CONTAINER_TYPE_FUNCTION,
} USEDEF_CONTAINER_TYPE, *PUSEDEF_CONTAINER_USEDEF;

typedef enum _USEDEF_TYPE
{
	USEDEF_TYPE_UNDEF,

	/* Used as the old value of a partially/conditionally updated destination. */
	USE_TYPE_OLDDEST,
	/* Used as an index in the old value of a partially/conditionally updated destination. */
	USE_TYPE_OLDDESTIDX,
	/* Used as an index in an instruction destination argument. */
	USE_TYPE_DESTIDX,
	/* Used as an instruction source argument. */
	USE_TYPE_SRC,
	/* Used as an index in an instruction source argument. */
	USE_TYPE_SRCIDX,
	/* Used as the predicate to control execution of an instruction. */
	USE_TYPE_PREDICATE,
	/* Used in the driver epilog. */
	USE_TYPE_FIXEDREG,
	/* Used as a function result. */
	USE_TYPE_FUNCOUTPUT,
	/* Used in a CBTYPE_SWITCH flow control block. */
	USE_TYPE_SWITCH,
	/* Used in a CBTYPE_COND flow control block. */
	USE_TYPE_COND,

	/* Defined by an instruction. */
	DEF_TYPE_INST,
	/* Defined by the driver prolog. */
	DEF_TYPE_FIXEDREG,
	/* Function input. */
	DEF_TYPE_FUNCINPUT,

	/* Range of USE/DEF types corresponding to uses. */
	USE_TYPE_FIRST = USE_TYPE_OLDDEST,
	USE_TYPE_LAST = USE_TYPE_COND,

	/* Range of USE/DEF types for defines. */
	DEF_TYPE_FIRST = DEF_TYPE_INST,
	DEF_TYPE_LAST = DEF_TYPE_FUNCINPUT,

	/* Range of USE types for uses in instruction. */
	USE_TYPE_FIRSTINSTUSE = USE_TYPE_OLDDEST,
	USE_TYPE_LASTINSTUSE = USE_TYPE_PREDICATE,
} USEDEF_TYPE, *PUSEDEF_TYPE;


#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
typedef enum _USEDEF_ISVECTOR
{
	USEDEF_ISVECTOR_UNKNOWN,
	USEDEF_ISVECTOR_YES,
	USEDEF_ISVECTOR_NO,
	USEDEF_ISVECTOR_NOTRECORDED,
} USEDEF_ISVECTOR, *PUSEDEF_ISVECTOR;
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

typedef struct _USEDEF
{
	/*
		Location of the use or definition.
	*/
	union
	{
		/* 
			DEF_TYPE_INST
			USE_TYPE_SRC
			USE_TYPE_SRCIDX
			USE_TYPE_DESTIDX
			USE_TYPE_OLDDEST
			USE_TYPE_OLDDESTIDX
			USE_TYPE_PREDICATE 
		*/
		PINST			psInst;	
		/* DEF_TYPE_FIXEDREG/USE_TYPE_FIXEDREG */
		PFIXED_REG_DATA	psFixedReg;
		/* USE_TYPE_COND/USE_TYPE_SWITCH */
		PCODEBLOCK		psBlock;
		/* DEF_TYPE_FUNCINPUT/USE_TYPE_FUNC_OUTPUT */
		PFUNC			psFunc;
		IMG_PVOID		pvData;
	} u;
	/* Type of the use or definition. */
	USEDEF_TYPE				eType;
	/* Index of the source or destination or offset into the fixed register data structure. */
	IMG_UINT32				uLocation;
	/* Entry in the list of uses associated with a particular variable. */
	USC_LIST_ENTRY			sListEntry;
	/* Points back to the variable of which this is a use or definition. */
	struct _USEDEF_CHAIN*	psUseDefChain;
} USEDEF, *PUSEDEF;

typedef struct _USEDEF_CHAIN
{
	/* Identifies the variable. */
	IMG_UINT32		uType;
	IMG_UINT32		uNumber;
	UF_REGFORMAT	eFmt;
	/*
		If the variable is an internal register then the block where all the uses/defines occur.
	*/
	PCODEBLOCK		psBlock;
	/* For a variable in SSA form: points to the (unique) definition. */
	PUSEDEF			psDef;
	/* Count of associated defines and uses. */
	IMG_UINT32		uUseDefCount;
	/* List of associated defines and uses. */
	USC_LIST		sList;
	/* Entry in the list of variables where a use has been dropped. */
	USC_LIST_ENTRY	sDroppedUsesTempListEntry;
	/* Count of uses of this variable as an index. */
	IMG_UINT32		uIndexUseCount;
	/* Entry in the list of variables used as indices. */
	USC_LIST_ENTRY	sIndexUseTempListEntry;
	/* Entry in the list of variables of C10 format. */
	USC_LIST_ENTRY	sC10TempListEntry;
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	USEDEF_ISVECTOR	eIsVector;
	/* Entry in the list of vector-sized registers. */
	USC_LIST_ENTRY	sVectorTempListEntry;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
} USEDEF_CHAIN, *PUSEDEF_CHAIN;

typedef struct _ARGUMENT_USEDEF
{
	/*
		Information about the register itself.
	*/
	USEDEF			sUseDef;
	/*
		Information about any register used as an index.
	*/
	USEDEF			sIndexUseDef;
} ARGUMENT_USEDEF, *PARGUMENT_USEDEF;

typedef struct _IREG_STATE
{
	/*
		Mask of the internal registers live before the instruction.
	*/
	IMG_UINT32	uLiveMask;

	/*
		Mask of the internal registers where the defining instruction
		wrote them with C10 format data.
	*/
	IMG_UINT32	uC10Mask;

	/*
		Mask of the internal registers where the defining instruction had
		the SKIPINVALID flag set.
	*/
	IMG_UINT32	uSkipInvalidMask;
} IREG_STATE, *PIREG_STATE;

typedef struct _IREGLIVENESS_ITERATOR
{
	/*
		Mask of internal registers live before the current instruction.
	*/
	IMG_UINT32		uIRegLiveMask;
	/*
		Mask of internal registers which are live and have been written
		with C10 format data.
	*/
	IMG_UINT32		uIRegC10Mask;
	/*
		Mask of internal registers which are live and have been written
		by instructions with the skipinvalid flag set.
	*/
	IMG_UINT32		uIRegSkipInvalidMask;
	/*
		For each internal register: points to the first use/define in an
		instruction either at or after the current instruction.
	*/
	PUSC_LIST_ENTRY	apsUseDefEntries[MAXIMUM_GPI_SIZE_IN_SCALAR_REGS];
} IREGLIVENESS_ITERATOR, *PIREGLIVENESS_ITERATOR;

FORCE_INLINE
IMG_VOID UseDefReset(PUSEDEF psUseDef, USEDEF_TYPE eType, IMG_UINT32 uLocation, IMG_PVOID pvData)
/*****************************************************************************
 FUNCTION	: UseDefReset
    
 PURPOSE	: Initialize a use or define record.

 PARAMETERS	: psUseDef			- Structure to initialize.
			  eType				- Type of use/define.
			  uLocation			- Type dependent information about the use/define.
			  pvData
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	psUseDef->eType = eType;
	psUseDef->uLocation = uLocation;
	psUseDef->u.pvData = pvData;
	ClearListEntry(&psUseDef->sListEntry);
	psUseDef->psUseDefChain = NULL;
}

FORCE_INLINE
IMG_VOID UseDefResetArgument(PARGUMENT_USEDEF	psArgUseDef, 
							 USEDEF_TYPE		eType, 
							 USEDEF_TYPE		eIndexType, 
							 IMG_UINT32			uLocation,
							 IMG_PVOID			pvData)
/*****************************************************************************
 FUNCTION	: UseDefResetArgument
    
 PURPOSE	: Initialize a use or define record for a source or destination argument.

 PARAMETERS	: psUseDef			- Structure to initialize.
			  eType				- Type of use/define.
			  eIndexType		- Type of use/define for any index used at the argument.
			  uLocation			- Type dependent information about the use/define.
			  pvData
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	UseDefReset(&psArgUseDef->sUseDef, eType, uLocation, pvData);
	UseDefReset(&psArgUseDef->sIndexUseDef, eIndexType, uLocation, pvData);
}

#if defined(UF_TESTBENCH)
IMG_VOID VerifyUseDef(PINTERMEDIATE_STATE psState);
#endif /* defined(UF_TESTBENCH) */
PUSEDEF_CHAIN UseDefGet(PINTERMEDIATE_STATE psState, IMG_UINT32 uType, IMG_UINT32 uNumber);
IMG_VOID UseDefAddUse(PINTERMEDIATE_STATE	psState,
					  IMG_UINT32			uType,
					  IMG_UINT32			uNumber,
					  PUSEDEF				psUseToAdd);
IMG_VOID UseDefDropUse(PINTERMEDIATE_STATE	psState, 
					   PUSEDEF				psUse);
IMG_VOID UseDefAddDef(PINTERMEDIATE_STATE	psState,
					  IMG_UINT32			uType,
					  IMG_UINT32			uNumber,
					  PUSEDEF				psDef);
IMG_VOID UseDefReplaceDef(PINTERMEDIATE_STATE	psState, 
						  IMG_UINT32			uType, 
						  IMG_UINT32			uNumber,
						  PUSEDEF				psFromDef,
						  PUSEDEF				psToDef);
IMG_VOID UseDefDropArgUses(PINTERMEDIATE_STATE	psState,
						   PARGUMENT_USEDEF		psArgUse);
IMG_VOID UseDefDeinitialize(PINTERMEDIATE_STATE psState);
IMG_VOID UseDefFreeAll(PINTERMEDIATE_STATE psState, IMG_UINT32 uType);
IMG_VOID UseDefDropFixedRegDef(PINTERMEDIATE_STATE	psState, 
							   PFIXED_REG_DATA		psFixedReg,
							   IMG_UINT32			uRegIdx);
IMG_VOID UseDefAddFixedRegDef(PINTERMEDIATE_STATE	psState,
							  PFIXED_REG_DATA		psFixedReg,
							  IMG_UINT32			uRegIdx);
IMG_VOID UseDefSubstUse(PINTERMEDIATE_STATE psState, 
						PUSEDEF_CHAIN		psUseDefToReplace, 
						PUSEDEF				psUseToReplace, 
						PARG				psReplacement);
IMG_VOID UseDefBaseSubstUse(PINTERMEDIATE_STATE psState, 
							PUSEDEF_CHAIN		psUseDefToReplace, 
							PUSEDEF				psUseToReplace, 
							PARG				psReplacement,
							IMG_BOOL			bDontUpdateFmt);
IMG_BOOL UseDefSubstituteIntermediateRegister(PINTERMEDIATE_STATE	psState,
											  PARG					psDest,
											  PARG					psSrc,
											  #if defined(SRC_DEBUG)
											  IMG_UINT32			uSrcLineToSubst,
											  #endif /* defined(SRC_DEBUG) */
											  IMG_BOOL				bExcludePrimaryEpilog,
											  IMG_BOOL				bExcludeSecondaryEpilog,
											  IMG_BOOL				bCheckOnly);
IMG_BOOL UseDefIsReferencedOutsideBlock(PINTERMEDIATE_STATE	psState,
										PUSEDEF_CHAIN		psUseDef,
										PCODEBLOCK			psBlock);
IMG_VOID UseDefInitialize(PINTERMEDIATE_STATE psState);
IMG_VOID UseDefDropFixedRegUse(PINTERMEDIATE_STATE	psState, 
							   PFIXED_REG_DATA		psFixedReg,
							   IMG_UINT32			uRegIdx);
IMG_VOID UseDefAddFixedRegUse(PINTERMEDIATE_STATE	psState,
							  PFIXED_REG_DATA		psFixedReg,
							  IMG_UINT32			uRegIdx);
IMG_BOOL UseDefIsDef(PUSEDEF psUseDef);
IMG_BOOL UseDefIsUse(PUSEDEF psUseDef);
IMG_BOOL UseDefIsPredicateSourceOnly(PINTERMEDIATE_STATE psState, PINST psInst, PARG psDest);
IMG_BOOL UseDefGetSingleUse(PINTERMEDIATE_STATE psState, 
						    PARG psDest,
							PINST* ppsUseInst,
							PUSEDEF_TYPE peUseType,
							IMG_PUINT32 puUseSrcIdx);
IMG_UINT32 UseDefGetSingleSourceUse(PINTERMEDIATE_STATE psState, PINST psInst, PARG psDest);
IMG_BOOL UseDefIsSingleSourceUse(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSrcIdx, PARG psDest);
IMG_BOOL UseDefIsSingleSourceRegisterUse(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSrcIdx);
IMG_UINT32 UseDefGetSourceUseMask(PINTERMEDIATE_STATE psState, PINST psInst, PARG psDest);
IMG_VOID UseDefDelete(PINTERMEDIATE_STATE psState, PUSEDEF_CHAIN psUseDef);
IMG_VOID UseDefDropDest(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uDestIdx);
PINST UseDefGetDefInst(PINTERMEDIATE_STATE psState, IMG_UINT32 uType, IMG_UINT32 uNumber, IMG_PUINT32 puDestIdx);
PINST UseDefGetDefInstFromChain(PUSEDEF_CHAIN psUseDef, IMG_PUINT32 puDestIdx);
PFUNC UseDefGetUseLocation(PUSEDEF psUse);
IMG_VOID UseDefSetFmt(PINTERMEDIATE_STATE psState, IMG_UINT32 uTempNum, UF_REGFORMAT eFmt);
IMG_VOID UseDefSetArrayFmt(PINTERMEDIATE_STATE psState, IMG_UINT32 uArrayNum, UF_REGFORMAT eFmt);
IMG_VOID UseDefDropUnusedTemps(PINTERMEDIATE_STATE psState);
IMG_VOID UseDefDropFuncInput(PINTERMEDIATE_STATE psState, PFUNC psFunc, IMG_UINT32 uInput);
IMG_VOID UseDefDropFuncOutput(PINTERMEDIATE_STATE psState, PFUNC psFunc, IMG_UINT32 uOutput);
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
IMG_BOOL IsVectorDest(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uDestIdx);
IMG_BOOL IsVectorSource(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSrc);
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
IMG_VOID UseDefAddFuncOutput(PINTERMEDIATE_STATE psState, PFUNC psFunc, IMG_UINT32 uOutput);
IMG_VOID UseDefMoveFuncInput(PINTERMEDIATE_STATE psState, PFUNC psFunc, IMG_UINT32 uToIdx, IMG_UINT32 uFromIdx);
IMG_VOID UseDefAddFuncInput(PINTERMEDIATE_STATE psState, PFUNC psFunc, IMG_UINT32 uInput);
PARG UseDefGetInstUseLocation(PINTERMEDIATE_STATE	psState,
							  PUSEDEF				psUse,
							  PUSEDEF*				ppsIndexUse);
IMG_UINT32 UseDefGetRegisterSourceUseMask(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uType, IMG_UINT32 uNumber);
IMG_BOOL UseDefIsPartialDestOnly(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uDestIdx, PARG psDest);
IMG_VOID UseDefAddArgUses(PINTERMEDIATE_STATE	psState,
						  PCARG					psArg,
						  PARGUMENT_USEDEF		psArgUseDef);
IMG_VOID UseDefReplaceArgumentUses(PINTERMEDIATE_STATE	psState, 
								   PARG					psToArg, 
								   PARGUMENT_USEDEF		psToUseRecord,
								   PARGUMENT_USEDEF		psFromUseRecord);
IMG_BOOL UseDefGetSingleRegisterUse(PINTERMEDIATE_STATE psState, 
									PVREGISTER psRegister,
									IMG_PVOID* ppvUse,
									PUSEDEF_TYPE peUseType,
									IMG_PUINT32 puUseSrcIdx);
IMG_VOID UseDefReplaceDest(PINTERMEDIATE_STATE	psState,
						   PINST				psMoveToInst,
						   IMG_UINT32			uMoveToDestIdx,
						   PINST				psMoveFromInst,
						   IMG_UINT32			uMoveFromDestIdx);
IMG_BOOL UseDefIsSSARegisterType(PINTERMEDIATE_STATE psState, IMG_UINT32 uType);
PARGUMENT_USEDEF UseDefResizeArgArray(PINTERMEDIATE_STATE	psState, 
									  PARGUMENT_USEDEF		asOldUseDef, 
									  IMG_UINT32			uOldCount, 
									  IMG_UINT32			uNewCount);
PUSEDEF UseDefResizeArray(PINTERMEDIATE_STATE	psState, 
						  PUSEDEF				asOldUseDef, 
						  IMG_UINT32			uOldCount, 
						  IMG_UINT32			uNewCount);
IMG_VOID UseDefModifyInstructionBlock(PINTERMEDIATE_STATE psState, PINST psInst, PCODEBLOCK psOldBlock);
IMG_BOOL UseDefIsNextReferenceAsUse(PINTERMEDIATE_STATE psState, PUSC_LIST_ENTRY psListEntry);
IMG_VOID UseDefGetIRegStateBeforeInst(PINTERMEDIATE_STATE psState, PINST psInst, PIREG_STATE psIRegState);
IMG_UINT32 UseDefGetIRegsLiveBeforeInst(PINTERMEDIATE_STATE psState, PINST psInst);
IMG_BOOL UseDefIsIRegLiveInInternal(PINTERMEDIATE_STATE psState, 
									IMG_UINT32			uIRegIdx, 
									PINST				psIntervalStart, 
									PINST				psIntervalEnd);

IMG_VOID UseDefIterateIRegLiveness_Initialize(PINTERMEDIATE_STATE		psState, 
											  PCODEBLOCK				psBlock, 
											  PIREGLIVENESS_ITERATOR	psIterator);
PINST UseDefGetLastUse(PINTERMEDIATE_STATE psState, IMG_UINT32 uType, IMG_UINT32 uNumber);
IMG_VOID UseDefIterateIRegLiveness_InitializeAtPoint(PINTERMEDIATE_STATE	psState, 
													 PINST					psPoint, 
													 PIREGLIVENESS_ITERATOR	psIterator);
IMG_VOID UseDefIterateIRegLiveness_Next(PINTERMEDIATE_STATE		psState, 
										PIREGLIVENESS_ITERATOR	psIterator,
										PINST					psCurrentInst);
IMG_VOID UseDefGetIRegLivenessSpanOverInterval(PINTERMEDIATE_STATE	psState,
											   PINST				psStartInst,
											   PINST				psEndInst, 
											   IMG_UINT32			uIReg,
											   PINST 				*ppsLastDefForSpan,
											   PINST				*ppsLastRefForSpan);
PINST UseDefGetInst(PUSEDEF psUseOrDef);
IMG_UINT32 GetUseChanMask(PINTERMEDIATE_STATE	psState,
						  PUSEDEF				psUse);
IMG_BOOL UseDefForAllUsesOfSet(PINTERMEDIATE_STATE	psState,
							   IMG_UINT32			uSetCount,
							   PUSEDEF_CHAIN		apsSet[],
							   IMG_BOOL				(*pfProcess)(PINTERMEDIATE_STATE, PINST, PCSOURCE_VECTOR, IMG_PVOID),
							   IMG_PVOID			pvUserData);
IMG_INT32 CmpUse(PUSC_LIST_ENTRY psListEntry1, PUSC_LIST_ENTRY psListEntry2);
IMG_UINT32 UseDefGetUsedChanMask(PINTERMEDIATE_STATE psState, PUSEDEF_CHAIN psUseDef);

FORCE_INLINE
IMG_UINT32 UseDefIterateIRegLiveness_Current(PIREGLIVENESS_ITERATOR	psIterator)
/*****************************************************************************
 FUNCTION	: UseDefIterateIRegLiveness_Current
    
 PURPOSE	: Returns the mask of internal registers live before the current
			  instruction.

 PARAMETERS	: psIterator		- Iterator state.

 RETURNS	: The mask of live registers.
*****************************************************************************/
{
	return psIterator->uIRegLiveMask;
}

FORCE_INLINE
IMG_UINT32 UseDefIterateIRegLiveness_GetC10Mask(PIREGLIVENESS_ITERATOR	psIterator)
/*****************************************************************************
 FUNCTION	: UseDefIterateIRegLiveness_GetC10Mask
    
 PURPOSE	: Returns the mask of internal registers live before the current
			  instruction which have been written with C10 format data.

 PARAMETERS	: psIterator		- Iterator state.

 RETURNS	: The mask of C10 format registers.
*****************************************************************************/
{
	return psIterator->uIRegC10Mask;
}

FORCE_INLINE
IMG_UINT32 UseDefIterateIRegLiveness_GetSkipInvalidMask(PIREGLIVENESS_ITERATOR	psIterator)
/*****************************************************************************
 FUNCTION	: UseDefIterateIRegLiveness_GetSkipInvalidMask
    
 PURPOSE	: Returns the mask of internal registers live before the current
			  instruction which have been written by instruction with
			  the SKIPINVALID flag set.

 PARAMETERS	: psIterator		- Iterator state.

 RETURNS	: The mask of SKIPINVALID registers.
*****************************************************************************/
{
	return psIterator->uIRegSkipInvalidMask;
}

FORCE_INLINE
IMG_BOOL UseDefIsInstUseDef(PUSEDEF psUseDef)
/*****************************************************************************
 FUNCTION	: UseDefIsInstUseDef
    
 PURPOSE	: Checks if a use or define record is related to an intermediate
			  instruction.

 PARAMETERS	: psUseDef			- Use or define record to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (
			(
				psUseDef->eType >= USE_TYPE_FIRSTINSTUSE && 
				psUseDef->eType <= USE_TYPE_LASTINSTUSE
			) || 
			psUseDef->eType == DEF_TYPE_INST
	   )
	{
		return IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
}

struct _USEDEF_SETUSES_ITERATOR;
typedef struct _USEDEF_SETUSES_ITERATOR* PUSEDEF_SETUSES_ITERATOR;

PUSEDEF_SETUSES_ITERATOR UseDefSetUsesIterator_Initialize(PINTERMEDIATE_STATE		psState, 
														  IMG_UINT32				uSetCount, 
														  PUSEDEF_CHAIN				apsSet[]);
IMG_BOOL UseDefSetUsesIterator_Continue(PUSEDEF_SETUSES_ITERATOR psIter);
IMG_VOID UseDefSetUsesIterator_Next(PUSEDEF_SETUSES_ITERATOR psIter);
IMG_VOID UseDefSetUsesIterator_Finalise(PINTERMEDIATE_STATE psState, PUSEDEF_SETUSES_ITERATOR psIter);
IMG_PVOID UseDefSetUsesIterator_CurrentContainer(PUSEDEF_SETUSES_ITERATOR psIter);
USEDEF_CONTAINER_TYPE UseDefSetUsesIterator_CurrentContainerType(PUSEDEF_SETUSES_ITERATOR psIter);

IMG_BOOL UseDefSetUsesIterator_Member_Continue(PUSEDEF_SETUSES_ITERATOR psIter, IMG_UINT32 uMember);
IMG_VOID UseDefSetUsesIterator_Member_Next(PUSEDEF_SETUSES_ITERATOR psIter, IMG_UINT32 uMember);
PUSEDEF UseDefSetUsesIterator_Member_CurrentUse(PUSEDEF_SETUSES_ITERATOR psIter, IMG_UINT32 uMember);

#endif /* __USC_USEDEF_H_ */

/* EOF */
