/******************************************************************************
 * Name         : usc.c
 * Title        : Common code for building/manipulating and compiling from CFGs
 * Created      : April 2005
 *
 * Copyright    : 2002-2008 by Imagination Technologies Limited.
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
 * $Log: usc.c $
 * ,.
 *  --- Revision Logs Removed --- -iftikhar.ahmad
 *****************************************************************************/

/*
	Hide warnings about
	unused parameters, unused variables, unreferenced functions, non-ansi
*/
#include "uscshrd.h"
#include "reggroup.h"
#include "bitops.h"
#include "usedef.h"
#include <limits.h>


/*
	How many bits are set in a nibble.
*/
IMG_INTERNAL
const IMG_UINT32	g_auSetBitCount[] =  {	0	/* 0 */, 
											1	/* 1 */, 
											1	/* 2 */, 
											2	/* 3 */, 
											1	/* 4 */, 
											2	/* 5 */, 
											2	/* 6 */, 
											3	/* 7 */, 
											1	/* 8 */, 
											2	/* 9 */,
											2	/* 10 */, 
											3	/* 11 */, 
											2	/* 12 */, 
											3	/* 13 */, 
											3	/* 14 */, 
											4	/* 15 */};

/*
	Maximum bit set in a nibble.
*/
IMG_INTERNAL
const IMG_UINT32	g_auMaxBitCount[] =  {	USC_UNDEF	/* 0 */, 
											0			/* 1 */, 
											1			/* 2 */, 
											1			/* 3 */, 
											2			/* 4 */, 
											2			/* 5 */, 
											2			/* 6 */, 
											2			/* 7 */, 
											3			/* 8 */, 
											3			/* 9 */,
											3			/* 10 */, 
											3			/* 11 */, 
											3			/* 12 */, 
											3			/* 13 */, 
											3			/* 14 */, 
											3			/* 15 */};

/*
	First bit set in a nibble.
*/
IMG_INTERNAL
const IMG_UINT32	g_auFirstSetBit[] =  {	USC_UNDEF	/* 0 */, 
											0			/* 1 */, 
											1			/* 2 */, 
											0			/* 3 */, 
											2			/* 4 */, 
											0			/* 5 */, 
											1			/* 6 */, 
											0			/* 7 */, 
											3			/* 8 */, 
											0			/* 9 */,
											1			/* 10 */, 
											0			/* 11 */, 
											2			/* 12 */, 
											0			/* 13 */, 
											1			/* 14 */, 
											0			/* 15 */};

/*
	Is only one bit set in a nibble.
*/
IMG_INTERNAL
const IMG_BOOL g_abSingleBitSet[] = {IMG_FALSE, IMG_TRUE, IMG_TRUE, IMG_FALSE, IMG_TRUE,
									 IMG_FALSE, IMG_FALSE, IMG_FALSE, IMG_TRUE, IMG_FALSE,
									 IMG_FALSE, IMG_FALSE, IMG_FALSE, IMG_FALSE, IMG_FALSE,
									 IMG_FALSE};

/*
	Swap red and blue for channels in a vec4 register. 
*/
IMG_INTERNAL const IMG_UINT32 g_puSwapRedAndBlueChan[] = {2, 1, 0, 3, 4, 5, 6, 7};

/*
	Swap red and blue for a mask for updating a vec4 register.
*/
IMG_INTERNAL const IMG_UINT32 g_puSwapRedAndBlueMask[] = {0, 4, 2, 6, 1, 5, 3, 7, 8, 12, 10, 14, 9, 13, 11, 15};

/* Maps a 4-bit mask to the single component it writes. */
IMG_INTERNAL const IMG_INT32	g_aiSingleComponent[] = 
		{-1 /*  0 */,  0 /*  1 */,  1 /*  2 */, -1 /*  3 */,
		  2 /*  4 */, -1 /*  5 */, -1 /*  6 */, -1 /*  7 */,
		  3 /*  8 */, -1 /*  9 */, -1 /* 10 */, -1 /* 11 */,
		 -1 /* 12 */, -1 /* 13 */, -1 /* 14 */, -1 /* 15 */};

/*
	Decompose a nibble mask into masks containing just the first set bit,
	just the second set bit, etc.
*/
IMG_INTERNAL
const IMG_UINT32 g_auSetBitMask[][4] = 
{
	{0x0, 0x0, 0x0, 0x0},	/* 0000 */
	{0x1, 0x0, 0x0, 0x0},	/* 0001 */
	{0x2, 0x0, 0x0, 0x0},	/* 0010 */
	{0x1, 0x2, 0x0, 0x0},	/* 0011 */
	{0x4, 0x0, 0x0, 0x0},	/* 0100 */
	{0x1, 0x4, 0x0, 0x0},	/* 0101 */
	{0x2, 0x4, 0x0, 0x0},	/* 0110 */
	{0x1, 0x2, 0x4, 0x0},	/* 0111 */
	{0x8, 0x0, 0x0, 0x0},	/* 1000 */
	{0x1, 0x8, 0x0, 0x0},	/* 1001 */
	{0x2, 0x8, 0x0, 0x0},	/* 1010 */
	{0x1, 0x2, 0x8, 0x0},	/* 1011 */
	{0x4, 0x8, 0x0, 0x0},	/* 1100 */
	{0x1, 0x4, 0x8, 0x0},	/* 1101 */
	{0x2, 0x4, 0x8, 0x0},	/* 1110 */
	{0x1, 0x2, 0x4, 0x8},	/* 1111 */
};

/*
	Convert a comparison operator to its inverse.
*/
IMG_INTERNAL
UFREG_COMPOP const g_puInvertCompOp[] = { UFREG_COMPOP_INVALID,	/* Invalid */
										  UFREG_COMPOP_LE,		/* UFREG_COMPOP_GT */
										  UFREG_COMPOP_NE,		/* UFREG_COMPOP_EQ */
										  UFREG_COMPOP_LT,		/* UFREG_COMPOP_GE */
										  UFREG_COMPOP_GE,		/* UFREG_COMPOP_LT */
										  UFREG_COMPOP_EQ,		/* UFREG_COMPOP_NE */
										  UFREG_COMPOP_GT,		/* UFREG_COMPOP_LE */};

/*
	What's the new comparison operator if both sides are negated.
*/
IMG_INTERNAL
UFREG_COMPOP const g_puNegateCompOp[] = { UFREG_COMPOP_INVALID,	/* Invalid */
										  UFREG_COMPOP_LT,		/* UFREG_COMPOP_GT */
										  UFREG_COMPOP_EQ,		/* UFREG_COMPOP_EQ */
										  UFREG_COMPOP_LE,		/* UFREG_COMPOP_GE */
										  UFREG_COMPOP_GT,		/* UFREG_COMPOP_LT */
										  UFREG_COMPOP_NE,		/* UFREG_COMPOP_NE */
										  UFREG_COMPOP_GE,		/* UFREG_COMPOP_LE */};

/*************************
 *  Function Definitions *
 *************************/
#if defined(DEBUG)
static 
IMG_VOID DebugVerifyInstGroup(PINTERMEDIATE_STATE psState)
{
	IMG_UINT32 i, j, k, auGroupCount[32] = {0};
	IMG_UINT32 auGroupFlgShared[32][32] = {{0}};
	
	/* Calculate individual counts for groups */
	for(i = 0; i < IOPCODE_MAX; i++)
	{
		for(j = 0; j<32; j++)
		{
			if(g_psInstDesc[i].uOptimizationGroup & (1U<<j))
			{
				auGroupCount[j]++;
				for(k = 0; k < 32; k++)
				{
					if(g_psInstDesc[i].uOptimizationGroup & (1U<<k))
					{
						auGroupFlgShared[j][k]++;
					}
				}
			}
		}
	}
	
	for(j=0; j<32; j++)
	{
		for(k = j+1; k < 32; k++)
		{
			if(auGroupCount[j])
			{
				if(auGroupCount[k] == auGroupCount[j] && 
					auGroupCount[k] == auGroupFlgShared[j][k])
				{
					DBG_PRINTF((DBG_ERROR, "USC_OPT_GROUP: SAME FLAGS (%08x):(%08x)", (1<<j), (1<<k)));
				}
			}
		}
	}
}
#else
	#define DebugVerifyInstGroup(x) 
#endif

/*****************************************************************************
 FUNCTION	: UscAbort

 PURPOSE	: USC Error handler routine

 PARAMETERS	: pvState		- Compiler state.
			  uError		- Error code indicating the error condition
			  pcErrorStr	- Optional string to print (DEBUG builds only,
							  ignored if NULL)
			  pszFile		- File where UscAbort was called
			  uLine			- Line number where UscAbort was called

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL void UscAbort(IMG_PVOID	pvState,
						   IMG_UINT32	uError,
						   IMG_PCHAR	pcErrorStr,
						   IMG_PCHAR	pszFile,
						   IMG_UINT32	uLine)
{
	const PINTERMEDIATE_STATE psState = (PINTERMEDIATE_STATE)pvState;


	/*
		If the internal compiler-state isn't available, then all we can
		do is abort

		NB: During verification phase (i.e. post compilation) psState will be
			NULL.
	*/
	if	(!psState)
	{
		abort();
	}

	#if defined(DEBUG)
	/*
		Report what has gone wrong (and where)
	*/
	{
		IMG_PCHAR	pcError;
		IMG_PCHAR	pcFileName = NULL;

		/*
			Map the return code to a string
		*/
		switch (uError)
		{
			case UF_OK:
			{
				pcError = "UF_OK";
				break;
			}
			case UF_ERR_INVALID_OPCODE:
			{
				pcError = "UF_ERR_INVALID_OPCODE";
				break;
			}
			case UF_ERR_INVALID_DST_REG:
			{
				pcError = "UF_ERR_INVALID_DST_REG";
				break;
			}
			case UF_ERR_INVALID_SRC_REG:
			{
				pcError = "UF_ERR_INVALID_SRC_REG";
				break;
			}
			case UF_ERR_INVALID_DST_MOD:
			{
				pcError = "UF_ERR_INVALID_DST_MOD";
				break;
			}
			case UF_ERR_INVALID_SRC_MOD:
			{
				pcError = "UF_ERR_INVALID_SRC_MOD";
				break;
			}
			case UF_ERR_TOO_MANY_INSTS:
			{
				pcError = "UF_ERR_TOO_MANY_INSTS";
				break;
			}
			case UF_ERR_GENERIC:
			{
				pcError = "UF_ERR_GENERIC";
				break;
			}
			case UF_ERR_INTERNAL:
			{
				pcError = "UF_ERR_INTERNAL";
				break;
			}
			case UF_ERR_NO_MEMORY:
			{
				pcError = "UF_ERR_NO_MEMORY";
				break;
			}
			default:
			{
				pcError = "Unknown Error";
				uError	= UF_ERR_INTERNAL;
				break;
			}
		}

		if (pszFile)
		{
			pcFileName = pszFile + strlen(pszFile);
			while (pcFileName != pszFile)
			{
				if	((*pcFileName == '\\') || (*pcFileName == '/'))
				{
					pcFileName++;
					break;	
				}
				pcFileName--;	
			}
		}
		psState->pfnPrint("\n*** USC_ABORT: %s(%u): %s: %s ***\n", 
						  ((pcFileName != NULL) ? pcFileName : ""), 
						  uLine, 
						  pcError,
						  ((pcErrorStr != NULL) ? pcErrorStr : ""));
	}
	#else	/* #if defined(DEBUG) */
	/*
		Only used when printing an error message
	*/
	PVR_UNREFERENCED_PARAMETER(pcErrorStr);
	PVR_UNREFERENCED_PARAMETER(pszFile);
	PVR_UNREFERENCED_PARAMETER(uLine);
	#endif	/* #if defined(DEBUG) */

	/*
		Jump to the memory cleanup routine at the start of CompileUniflex, if
		a jongjump handler has been setup.
	*/
	if	(psState->bExceptionReturnValid)
	{
		longjmp(psState->sExceptionReturn, uError);
	}
	else
	{
		abort();
	}
}

/*****************************************************************************
 FUNCTION	: OldAllocfn

 PURPOSE	: Allocates an internal memory block.
				It is the slow path for allocations, used only for large 
				(> SMALL_CHUNK_SIZE) allocations, which are comparatively rare. 

 PARAMETERS	: psState		- Compiler state.
			  uSize			- The size of the block to allocate.

 RETURNS	: The allocated block.
*****************************************************************************/
static
#ifdef USC_COLLECT_ALLOC_INFO
IMG_PVOID OldAllocfn(USC_DATA_STATE_PTR psState, IMG_UINT32 uSize, IMG_UINT32  uLineNumber, const IMG_CHAR *pszFileName)
#else
IMG_PVOID OldAllocfn(PINTERMEDIATE_STATE psState, const IMG_UINT32 uSize)
#endif
{
	IMG_PVOID			pvBlock;
	PUSC_ALLOC_HEADER	psHeader;

	pvBlock = psState->pfnAlloc(sizeof(USC_ALLOC_HEADER) + uSize);
	if (pvBlock == NULL)
	{
		/* Doesn't return. */
		longjmp(psState->sExceptionReturn, UF_ERR_NO_MEMORY);
	}
	psHeader = (PUSC_ALLOC_HEADER)pvBlock;
	#ifdef DEBUG
	psHeader->uSize = uSize;
	psHeader->uAllocNum = psState->uAllocCount++;
	#endif /* DEBUG */

	if (psState->psAllocationListHead != NULL)
	{
		psState->psAllocationListHead->psPrev = psHeader;
	}
	psHeader->psNext = psState->psAllocationListHead;
	psHeader->psPrev = NULL;
	psState->psAllocationListHead = psHeader;

	#ifdef USC_COLLECT_ALLOC_INFO	
	sprintf(psHeader->acAllocInfo, "%5u | %-70.70s",uLineNumber, pszFileName);
	#endif
	
	return (IMG_PVOID)(psHeader + 1);
}

IMG_INTERNAL
#ifdef USC_COLLECT_ALLOC_INFO
IMG_PVOID UscAllocfn(USC_DATA_STATE_PTR psState, IMG_UINT32 uSize, IMG_UINT32  uLineNumber, const IMG_CHAR *pszFileName)
#else
IMG_PVOID UscAllocfn(PINTERMEDIATE_STATE psState, IMG_UINT32 uSize)
#endif
/*****************************************************************************
 FUNCTION	: UscAllocfn

 PURPOSE	: Allocates an internal memory block.

 PARAMETERS	: psState		- Compiler state.
			  uSize			- The size of the block to allocate.

 RETURNS	: The allocated block.
*****************************************************************************/
{
	IMG_PVOID pvBlock;

	if (uSize == 0)
	{
		return NULL;
	}

#if defined(FAST_CHUNK_ALLOC)

	if( uSize > SMALL_CHUNK_SIZE )
	{
		#ifdef USC_COLLECT_ALLOC_INFO
		pvBlock = OldAllocfn( psState,  uSize,   uLineNumber,  pszFileName);
		#else
		pvBlock = OldAllocfn( psState, uSize);
		#endif /* USC_COLLECT_ALLOC_INFO */
	}
	else
	{
		pvBlock = FastAlloc(psState);
		if (pvBlock == NULL)
			{
				
				/* Doesn't return. */
				longjmp(psState->sExceptionReturn, UF_ERR_NO_MEMORY);
			}
	}

#else

	#ifdef USC_COLLECT_ALLOC_INFO
	pvBlock = OldAllocfn( psState, uSize,   uLineNumber,  pszFileName);
	#else
	pvBlock = OldAllocfn( psState, uSize);
	#endif /* USC_COLLECT_ALLOC_INFO */

#endif /*FAST_CHUNK_ALLOC*/


#ifdef DEBUG

	psState->uMemoryUsed += uSize;
	psState->uMemoryUsedHWM = max(psState->uMemoryUsedHWM, psState->uMemoryUsed);

#endif /* DEBUG */

	return pvBlock;
}

/*****************************************************************************

 FUNCTION	: _OldFree

 PURPOSE	: Frees an internal memory block.
				This function is the slow path for de-allocations. 
				Meant only for memory blocks that were allocated using the OldAllocfn


 PARAMETERS	: psState		- Compiler state.
			  pvBlock		- The block to free.

 RETURNS	: Nothing.
*****************************************************************************/
static
IMG_VOID _OldFree(PINTERMEDIATE_STATE psState, IMG_PVOID *pvBlock)
{
	PUSC_ALLOC_HEADER	psHeader;

	if (*pvBlock == NULL)
	{
		return;
	}

	psHeader = (PUSC_ALLOC_HEADER)(*pvBlock) - 1;

	#ifdef DEBUG
	{
		/* Empty statement to make setting a break point easier */
		IMG_UINT32 windbg = psHeader->uAllocNum;
		if (windbg != windbg){ windbg = psHeader->uAllocNum; }
	}
	#endif /* DEBUG */

	if (psHeader->psPrev == NULL)
	{
		psState->psAllocationListHead = psHeader->psNext;
	}
	else
	{
		psHeader->psPrev->psNext = psHeader->psNext;
	}
	if (psHeader->psNext != NULL)
	{
		psHeader->psNext->psPrev = psHeader->psPrev;
	}
	
	#ifdef DEBUG
	ASSERT(psState->uMemoryUsed >= psHeader->uSize);
	psState->uMemoryUsed -= psHeader->uSize;
	#endif /* DEBUG */
	psState->pfnFree(psHeader);
}


IMG_INTERNAL
IMG_VOID _UscFree(PINTERMEDIATE_STATE psState, IMG_PVOID *pvBlock)
/*****************************************************************************

 FUNCTION	: _UscFree

 PURPOSE	: Frees an internal memory block.


 PARAMETERS	: psState		- Compiler state.
			  pvBlock		- The block to free.

 RETURNS	: Nothing.
*****************************************************************************/
{

#if defined(FAST_CHUNK_ALLOC)
	if(FastFree(psState, *pvBlock) == IMG_FALSE )
#endif
	{
		_OldFree(psState, pvBlock);
	}

/*null the pointer */
	*pvBlock = NULL;
}


#ifdef USC_COLLECT_ALLOC_INFO
static
IMG_VOID DisplayUnfreedAllocs(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: DisplayUnfreedAllocs

 PURPOSE	: Utility function to print out details of any memory leaks

 PARAMETERS	: psState		- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psState->psAllocationListHead)
	{
		PUSC_ALLOC_HEADER psHeader = psState->psAllocationListHead;
	
		psState->pfnPrint("Unfreed allocs detected:");
	
		while (psHeader)
		{
		        psState->pfnPrint("%s", psHeader->acAllocInfo);
			psHeader = psHeader->psNext;
		}
	}
}
#else
#define DisplayUnfreedAllocs(a) 
#endif

IMG_INTERNAL
PUNIFLEX_INST AllocInputInst(	PINTERMEDIATE_STATE		psState,
								PINPUT_PROGRAM			psProg,
								PUNIFLEX_INST			psOrigInst)
/*****************************************************************************
 FUNCTION	: AllocInputInst

 PURPOSE	: Allocate a new input instruction and add it to the end of the 
			  program.

 PARAMETERS	: psState		- The compiler state.
			  psProg		- The structure representing the program.
			  psOrigInst	- The input instruction to get source line from

 RETURNS	: The new instruction.
*****************************************************************************/
{
	PUNIFLEX_INST	psNewInst;

	#if !defined(SRC_DEBUG)
	PVR_UNREFERENCED_PARAMETER(psOrigInst);
	#endif /* !defined(SRC_DEBUG) */

	psNewInst = UscAlloc(psState, sizeof(UNIFLEX_INST));

	#if defined(SRC_DEBUG)
	if(psOrigInst)
	{
		/*
			Copy source line number from given input instruction
		*/
		psNewInst->uSrcLine = psOrigInst->uSrcLine;
	}
	else
	{
		psNewInst->uSrcLine = UNDEFINED_SOURCE_LINE;
	}
	#endif /* defined(SRC_DEBUG) */

	psNewInst->psBLink = psProg->psTail;
	psNewInst->psILink = NULL;

	if (psProg->psHead == NULL)
	{
		psProg->psHead = psProg->psTail = psNewInst;
	}
	else
	{
		psProg->psTail->psILink = psNewInst;
		psProg->psTail = psNewInst;
	}
	
	return psNewInst;
}

IMG_INTERNAL
IMG_VOID CopyInputInst(PUNIFLEX_INST	psDest, PUNIFLEX_INST	psSrc)
/*****************************************************************************
 FUNCTION	: CopyInputInst

 PURPOSE	: Copy an input instruction.

 PARAMETERS	: psDest			- The destination for the copy.
			  psSrc				- The source for the copy.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uSrc;

	psDest->eOpCode = psSrc->eOpCode;
	psDest->sDest = psSrc->sDest;
	psDest->sDest2 = psSrc->sDest2;
	psDest->uPredicate = psSrc->uPredicate;

#ifdef SRC_DEBUG
	psDest->uSrcLine = psSrc->uSrcLine;
#endif /* SRC_DEBUG */

	for (uSrc = 0; uSrc < g_asInputInstDesc[psSrc->eOpCode].uNumSrcArgs; uSrc++)
	{
		psDest->asSrc[uSrc] = psSrc->asSrc[uSrc];
	}
}

IMG_INTERNAL
IMG_VOID FreeInputProg(PINTERMEDIATE_STATE psState, PINPUT_PROGRAM *ppsInputProg)
{
	PUNIFLEX_INST psTempInst, psInst = (*ppsInputProg)->psHead;
	while(psInst)
	{
		psTempInst = psInst;
		psInst = psInst->psILink;
		UscFree(psState, psTempInst);
	}
	UscFree(psState, (*ppsInputProg));
	*ppsInputProg = IMG_NULL;
}

#if defined(DEBUG) || defined(USER_PERFORMANCE_STATS_ONLY)
/*****************************************************************************
 FUNCTION	: CheckReg

 PURPOSE	: Check that a register is well formed.

 PARAMETERS	: psState	- Internal compilation state
			  psReg		- The registers to check

 RETURNS	: True or false.
*****************************************************************************/
static
IMG_BOOL CheckReg(PINTERMEDIATE_STATE psState, PARG psReg)
{
	PVR_UNREFERENCED_PARAMETER(psState);

	/* Check the type */
	if (psReg->uType <= USC_REGTYPE_MAXIMUM)
	{
		return IMG_TRUE;
	}

	return IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: CheckInst

 PURPOSE	: Check that an instruction is well formed.

 PARAMETERS	: psState	- Internal compilation state
			  psInst	- The instruction to check

 RETURNS	: True or false.
*****************************************************************************/
static
IMG_BOOL CheckInst(PINTERMEDIATE_STATE psState, PINST psInst)
{
	IMG_UINT32 uCtr;
	IMG_UINT32 uArgCount;
	IMG_BOOL bInstIsGood;

	if (psInst == NULL)
	{
		return IMG_TRUE;
	}
	else if (psInst->eOpcode == ICALL)
	{
		bInstIsGood = (psInst->u.psCall->psTarget && psInst->u.psCall->psTarget->uLabel != UINT_MAX)
			? IMG_TRUE : IMG_FALSE;
		ASSERT (bInstIsGood);
		return bInstIsGood;
	}

	/* Check arguments */
	uArgCount = psInst->uArgumentCount;

	for (uCtr = 0; uCtr < psInst->uDestCount; uCtr++)
	{
		bInstIsGood = CheckReg(psState, &psInst->asDest[uCtr]);
		ASSERT(bInstIsGood);
		if (!bInstIsGood)
		{
			return IMG_FALSE;
		}
	}
	for (uCtr = 0; uCtr < uArgCount; uCtr ++)
	{
		bInstIsGood = CheckReg(psState, &psInst->asArg[uCtr]);

		ASSERT(bInstIsGood);
		if (!bInstIsGood)
		{
			return IMG_FALSE;
		}
	}
	

	return IMG_TRUE;
}

typedef struct _CHECK_BLOCK_DATA
{
	BLOCK_SORT_FUNC pfnSort;
	IMG_BOOL bHandlesCalls;
	IMG_PBOOL pbOk;
	IMG_UINT32 uCallCount;
} CHECK_BLOCK_DATA, *PCHECK_BLOCK_DATA;

static
IMG_VOID CheckBlockBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvUserData)
/*****************************************************************************
 FUNCTION	: CheckBlock

 PURPOSE	: BLOCK_PROC to check that a  block is correctly formed.

 PARAMETERS	: psState	- Internal compilation state
			  psBlock	- Block to examine

 OUTPUT		: pvStatus - IMG_PBOOL set to IMG_TRUE iff block is well-formed;
						 set to IMG_FALSE otherwise.
					
 NOTE		: in DEBUG builds, when the program is not well-formed, aborts
			  rather than setting pvStatus to false.

 RETURNS	: None; but sets pvStatus, as above.
*****************************************************************************/
{
	PCHECK_BLOCK_DATA psCheckBlockData = (PCHECK_BLOCK_DATA)(pvUserData);
	IMG_PBOOL pbStatus = psCheckBlockData->pbOk;
	IMG_BOOL bExitOk;
	PINST psInst;
	IMG_UINT32 uSucc;
	IMG_UINT32 uCallCount;
	ASSERT(psBlock);
	
	uCallCount = 0;
	for (psInst = psBlock->psBody; *pbStatus && psInst; psInst = psInst->psNext)
	{
		IMG_BOOL bInstIsGood;
		
		bInstIsGood = CheckInst(psState, psInst);
		ASSERT(bInstIsGood);
		if (psInst->eOpcode == ICALL)
		{
			uCallCount++;
		}
		*pbStatus = (*pbStatus && bInstIsGood) ? IMG_TRUE : IMG_FALSE;
	}
	ASSERT(psBlock->uCallCount == uCallCount);
	psCheckBlockData->uCallCount += uCallCount;
	if (psBlock == psBlock->psOwner->psExit)
	{
		bExitOk = (psBlock->eType == CBTYPE_EXIT) ? IMG_TRUE : IMG_FALSE;
	}
	else
	{
		bExitOk = (psBlock->eType != CBTYPE_EXIT) ? IMG_TRUE : IMG_FALSE;
	}

	ASSERT (bExitOk);
	*pbStatus = (*pbStatus && bExitOk) ? IMG_TRUE : IMG_FALSE;
	
	//check number of successors is correct for block type.
	if ((psBlock->eType == CBTYPE_SWITCH && psBlock->uNumSuccs < 2) ||
		(psBlock->eType == CBTYPE_COND && psBlock->uNumSuccs != 2) ||
		(psBlock->eType == CBTYPE_UNCOND && psBlock->uNumSuccs != 1) ||
		(psBlock->eType == CBTYPE_EXIT && psBlock->uNumSuccs != 0) ||
		(psBlock->eType == CBTYPE_CONTINUE && (psBlock->uNumSuccs != 1 || psBlock->psLoopHeader != psBlock->asSuccs[0].psDest)) ||
		(psBlock->eType == CBTYPE_UNDEFINED /* shouldn't exist post-construction! */))
	{
		//incorrect number of successors.
#if defined(DEBUG)
		imgabort();
#else
		*pbStatus = IMG_FALSE;
#endif
	}

	//ok, an extra check for CFG. Are predecessors consistent?
	if (psState->uFlags & USC_FLAGS_INTERMEDIATE_CODE_GENERATED)
	{
		IMG_UINT32 uPred;
		ASSERT (psBlock->uNumPreds ? psBlock->asPreds != NULL
								   : psBlock->asPreds == NULL);

		for (uPred = 0; uPred < psBlock->uNumPreds; uPred++)
		{
			PCODEBLOCK_EDGE	psEdge = &psBlock->asPreds[uPred];
			PCODEBLOCK		psDest;

			psDest = psEdge->psDest;
			ASSERT(psEdge->uDestIdx < psDest->uNumSuccs);
			ASSERT(psDest->asSuccs[psEdge->uDestIdx].psDest == psBlock);
			ASSERT(psDest->asSuccs[psEdge->uDestIdx].uDestIdx == uPred);
		}

		// Are successors consistent?
		for (uSucc = 0; uSucc < psBlock->uNumSuccs; uSucc++)
		{
			PCODEBLOCK_EDGE	psEdge = &psBlock->asSuccs[uSucc];
			PCODEBLOCK		psDest;

			psDest = psEdge->psDest;
			ASSERT(psEdge->uDestIdx < psDest->uNumPreds);
			ASSERT(psDest->asPreds[psEdge->uDestIdx].psDest == psBlock);
			ASSERT(psDest->asPreds[psEdge->uDestIdx].uDestIdx == uSucc);
		}
	}
	if(psBlock->uFlags & USC_CODEBLOCK_CFG)
	{
		DoOnCfgBasicBlocks(psState, psBlock->u.sUncond.psCfg, psCheckBlockData->pfnSort, CheckBlockBP, psCheckBlockData->bHandlesCalls, pvUserData);
	}
}

/*****************************************************************************
 FUNCTION	: CheckProgram

 PURPOSE	: Check that the program (main, functions, SA updater) is well formed.

 PARAMETERS	: psState	- Internal compilation state

 RETURNS	: IMG_TRUE if the program is well-formed; IMG_FALSE otherwise

 NOTE		: in DEBUG builds, when the program is not well-formed, aborts 
			  rather than returning false.
******************************************************************************/
IMG_INTERNAL
IMG_BOOL CheckProgram(PINTERMEDIATE_STATE psState)
{
	IMG_BOOL bOk = IMG_TRUE;
	PFUNC psFunc;

	/*
		Check all flow control blocks are well-formed.
	*/
	for (psFunc = psState->psFnInnermost; psFunc; psFunc = psFunc->psFnNestOuter)
	{
		CHECK_BLOCK_DATA sCheckBlockData;
		sCheckBlockData.pfnSort = ANY_ORDER;
		sCheckBlockData.bHandlesCalls = IMG_TRUE;		
		sCheckBlockData.pbOk = &bOk;
		sCheckBlockData.uCallCount = 0;
		DoOnCfgBasicBlocks(psState, &psFunc->sCfg, sCheckBlockData.pfnSort, CheckBlockBP, sCheckBlockData.bHandlesCalls, &sCheckBlockData);
		ASSERT(sCheckBlockData.uCallCount == psFunc->uCallCount);
	}

	#if defined(UF_TESTBENCH)
	/*
		Check USE-DEF chains are up to date.
	*/
	VerifyUseDef(psState);
	#endif /* defined(UF_TESTBENCH) */

	return bOk;
}
#endif /* DEBUG */

IMG_INTERNAL IMG_UINT32 GetNextRegisterCount(PINTERMEDIATE_STATE	psState,
											 IMG_UINT32				uCount)
/********************************************************************************
 Function			: GetNextRegisterCount

 Description		: Get a set of new temporary register numbers.
 
 Parameters			: psState				- Compiler state.
					  uCount				- Count of registers to get.

 Globals Effected	: None

 Return				: The first register number in the set. The other numbers
					  are consecutive.
*********************************************************************************/
{
	IMG_UINT32	uIdx;
	IMG_UINT32	uTemp;

	uTemp = GetNextRegister(psState);
	for (uIdx = 1; uIdx < uCount; uIdx++)
	{
		GetNextRegister(psState);
	}
	return uTemp;
}

static
IMG_UINT32 BaseGetNextRegister(PINTERMEDIATE_STATE psState, IMG_UINT32 uType, PVREGISTER* ppsVReg)
/*********************************************************************************
 Function			: BaseGetNextRegister

 Description		: Get a new temporary register number.
 
 Parameters			: psState				- Compiler state.

 Globals Effected	: None

 Return				: The register number.
*********************************************************************************/
{
	IMG_UINT32	uRegNum;
	USC_PARRAY	psVRegArray;

	if (uType == USEASM_REGTYPE_TEMP)
	{
		uRegNum = psState->uNumRegisters++;
		psVRegArray = psState->psTempVReg;
	}
	else
	{
		ASSERT(uType == USEASM_REGTYPE_PREDICATE);
		uRegNum = psState->uNumPredicates++;
		psVRegArray = psState->psPredVReg;
	}

	if (psVRegArray != NULL)
	{
		PVREGISTER	psVReg;

		psVReg = UscAlloc(psState, sizeof(*psVReg));
		psVReg->psGroup = NULL;
		psVReg->psUseDefChain = NULL;
		psVReg->psSecAttr = NULL;
		ArraySet(psState, psVRegArray, uRegNum, psVReg);

		if (ppsVReg != NULL)
		{
			*ppsVReg = psVReg;
		}
	}
	else
	{
		if (ppsVReg != NULL)
		{
			*ppsVReg = NULL;
		}
	}

	return uRegNum;
}

IMG_INTERNAL IMG_UINT32 GetNextPredicateRegister(PINTERMEDIATE_STATE psState)
/*********************************************************************************
 Function			: GetNextPredicateRegister

 Description		: Get a new predicate register number.
 
 Parameters			: psState				- Compiler state.

 Globals Effected	: None

 Return				: The register number.
*********************************************************************************/
{
	return BaseGetNextRegister(psState, USEASM_REGTYPE_PREDICATE, NULL /* ppsVReg */);
}

IMG_INTERNAL
IMG_VOID MakeNewTempArg(PINTERMEDIATE_STATE psState, UF_REGFORMAT eFmt, PARG psArg)
/*********************************************************************************
 Function			: MakeNewTempArg

 Description		: Create an argument structure refering to a previously unused
					  temporary.
 
 Parameters			: psState		- Compiler state.
					  eFmt			- Format of the temporary.
					  psArg			- Argument structure to initialize.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	InitInstArg(psArg);
	psArg->uType = USEASM_REGTYPE_TEMP;
	psArg->uNumber = BaseGetNextRegister(psState, USEASM_REGTYPE_TEMP, &psArg->psRegister);
	psArg->eFmt = eFmt;
}

IMG_INTERNAL
IMG_VOID MakeNewPredicateArg(PINTERMEDIATE_STATE psState, PARG psArg)
/*********************************************************************************
 Function			: MakeNewPredicateArg

 Description		: Create an argument structure refering to a previously unused
					  predicate.
 
 Parameters			: psState		- Compiler state.
					  psArg			- Argument structure to initialize.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	InitInstArg(psArg);
	psArg->uType = USEASM_REGTYPE_PREDICATE;
	psArg->uNumber = BaseGetNextRegister(psState, USEASM_REGTYPE_PREDICATE, &psArg->psRegister);
}

IMG_INTERNAL IMG_UINT32 GetNextRegister(PINTERMEDIATE_STATE psState)
/*********************************************************************************
 Function			: GetNextRegister

 Description		: Get a new temporary register number.
 
 Parameters			: psState				- Compiler state.

 Globals Effected	: None

 Return				: The register number.
*********************************************************************************/
{
	return BaseGetNextRegister(psState, USEASM_REGTYPE_TEMP, NULL /* ppsVReg */);
}

IMG_INTERNAL
IMG_VOID ClearFeedbackSplit(PINTERMEDIATE_STATE psState)
/*********************************************************************************
 Function			: ClearFeedbackSplit

 Description		: Flag that the program is no longer split into pre- and 
					  post-feedback fragments.
 
 Parameters			: psState				- Compiler state.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	psState->uFlags &= ~USC_FLAGS_SPLITFEEDBACKCALC;
	psState->psPreFeedbackBlock = NULL;
	psState->psPreFeedbackDriverEpilogBlock = NULL;
	#if defined(OUTPUT_USPBIN)
	psState->bResultWrittenInPhase0 = IMG_TRUE;
	#endif /* defined(OUTPUT_USPBIN) */
}

/******************************************************************************
 Routines for modifying CFGs/BLOCKs
******************************************************************************/

static IMG_VOID RemoveFromPredecessors(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_UINT32 uPredIdxToRemove)
/******************************************************************************
 FUNCTION		: RemoveFromPredecessors

 DESCRIPTION	: Removes a block from the predecessor list of another block

 PARAMETERS		: psState			- Compiler Intermediate State
				  psBlock			- Block to modify.
				  uPredIdxToRemove	- Entry in the predecessor array to remove.

 RETURNS		: Nothing

 NOTE			: removes only a single occurrence of psPred in psSucc's list.
******************************************************************************/
{
	ASSERT(uPredIdxToRemove < psBlock->uNumPreds);

	if (psBlock->asPreds)
	{
		/*
			Remove element, resize array.
		*/
		PCODEBLOCK_EDGE		asPredsNew;
		IMG_UINT32			uDestPredIdx;
		IMG_UINT32			uSrcPredIdx;

		asPredsNew = UscAlloc(psState, (psBlock->uNumPreds - 1) * sizeof(asPredsNew[0]));

		uDestPredIdx = 0;
		for (uSrcPredIdx = 0; uSrcPredIdx < psBlock->uNumPreds; uSrcPredIdx++)
		{
			if (uSrcPredIdx != uPredIdxToRemove)
			{
				PCODEBLOCK_EDGE	psEdge = &psBlock->asPreds[uSrcPredIdx];
				PCODEBLOCK		psEdgeDest;

				asPredsNew[uDestPredIdx] = *psEdge;

				psEdgeDest = psEdge->psDest;

				ASSERT(psEdge->uDestIdx < psEdgeDest->uNumSuccs);
				ASSERT(psEdgeDest->asSuccs[psEdge->uDestIdx].psDest == psBlock);
				ASSERT(psEdgeDest->asSuccs[psEdge->uDestIdx].uDestIdx == uSrcPredIdx);
				psEdgeDest->asSuccs[psEdge->uDestIdx].uDestIdx = uDestPredIdx;

				uDestPredIdx++;
			}
		}

		UscFree(psState, psBlock->asPreds);
		psBlock->asPreds = asPredsNew;
	}
	else
	{
		/*
			During generation of intermediate code, only, predecessor arrays do
			*not* have to be maintained, i.e. they may be NULL instead.
		*/
		ASSERT ((psState->uFlags & USC_FLAGS_INTERMEDIATE_CODE_GENERATED)==0);
	}
	psBlock->uNumPreds--;
}

static IMG_UINT32 AddAsPredecessor(PINTERMEDIATE_STATE psState, PCODEBLOCK psPred, PCODEBLOCK psSucc, IMG_UINT32 uSuccIdx)
/******************************************************************************
 FUNCTION		: AddAsPredecessor

 DESCRIPTION	: Adds a codeblock to the predecessor list of another

 PARAMETERS		: psState	- Compiler intermediate state
				  psPred	- Predecessor block
				  psSucc	- Successor block (i.e. to whose list the first
							  block should be added)
				  uSuccIdx	- Index of successor block in the predecessor block's
							successor array.

 RETURNS		: The index of the newly entry in the successor block's predecessor array.
******************************************************************************/
{
	/*
		During generation of intermediate code, only, predecessor arrays do
		*not* have to be maintained, i.e. they may be NULL instead. Take
		advantage of this to avoid resizing unnecessarily.
	*/
	if ((psState->uFlags & USC_FLAGS_INTERMEDIATE_CODE_GENERATED) != 0 || psSucc->asPreds)
	{

		IncreaseArraySizeInPlace(psState,
					(psSucc->uNumPreds + 0) * sizeof(psSucc->asPreds[0]),
					(psSucc->uNumPreds + 1) * sizeof(psSucc->asPreds[0]),
					(IMG_PVOID*)&psSucc->asPreds);
		/*
			success of ResizeArray includes freeing old array
			and (subsequently) overwriting the apsPreds pointer
		*/
		psSucc->asPreds[psSucc->uNumPreds].psDest = psPred;
		psSucc->asPreds[psSucc->uNumPreds].uDestIdx = uSuccIdx;
	}
	psSucc->uNumPreds++;
	return psSucc->uNumPreds - 1;
}

static
IMG_VOID SetSingleSuccessor(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PCODEBLOCK psSucc, IMG_UINT32 uPredIdx)
/******************************************************************************
 FUNCTION		: SetSingleSuccessor

 DESCRIPTION	: Set the successor array of a flow control block to point
				  to a single block.

 PARAMETERS		: psState			- Compiler intermediate state
				  psBlock			- Block to set the successor for.
				  psSucc			- Successor block.
				  uPredIdx			- Index of the edge pointing to the block
									in the successor's array of predecessors.

  RETURNS		: Nothing
******************************************************************************/
{
	if (psBlock->uNumSuccs != 1)
	{
		ResizeArray(psState,
					psBlock->asSuccs,
					sizeof(psBlock->asSuccs[0]) * psBlock->uNumSuccs,
					sizeof(psBlock->asSuccs[0]) * 1,
					(IMG_PVOID*)&psBlock->asSuccs);
		psBlock->uNumSuccs = 1;	
	}
	psBlock->asSuccs[0].psDest = psSucc;
	psBlock->asSuccs[0].uDestIdx = uPredIdx;
}

static
IMG_VOID InitializeUnconditionalBlock(PCODEBLOCK psBlock)
/******************************************************************************
 FUNCTION		: InitializeUnconditionalBlock

 DESCRIPTION	: Initialize the satellite data for a block with only one
				  successor.

 PARAMETERS		: psBlock	- Block to initialize.

 RETURNS		: Nothing.
******************************************************************************/
{
	psBlock->eType = CBTYPE_UNCOND;
	psBlock->u.sUncond.bSyncEnd = IMG_FALSE; //would prefer 'undefined'
}

static
IMG_VOID ClearBlockState(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
/******************************************************************************
 FUNCTION		: ClearBlockState

 DESCRIPTION	: Clear the satellite data for a block.

 PARAMETERS		: psState	- Compiler state.
				  psBlock	- Block to initialize.

 RETURNS		: Nothing.
******************************************************************************/
{
	if (psBlock->eType == CBTYPE_COND)
	{
		UseDefDropUse(psState, &psBlock->u.sCond.sPredSrcUse);
	}
	else if (psBlock->eType == CBTYPE_SWITCH)
	{
		//leave psArg untouched - caller's responsibility
		UscFree(psState, psBlock->u.sSwitch.pbSyncEnd);
		UscFree(psState, psBlock->u.sSwitch.auCaseValues);
	}

	#if defined(EXECPRED)
	if (!(psState->uCompilerFlags2 & UF_FLAGS2_EXECPRED) || !(psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_INTEGER_CONDITIONALS) || (psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_29643))
	{
	#endif /* #if defined(EXECPRED) */
		//unconditional blocks are static, so decrement if we're removing one
		if (psBlock->eType == CBTYPE_COND || psBlock->eType == CBTYPE_SWITCH)
		{
			if (!psBlock->bStatic) 
			{
				ASSERT(psState->uNumDynamicBranches > 0);
				psState->uNumDynamicBranches--;
			}
		}
	#if defined(EXECPRED)
	}
	#endif /* #if defined(EXECPRED) */
}

static
IMG_VOID FreePredecessors(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
/******************************************************************************
 FUNCTION		: FreePredecessors

 DESCRIPTION	: Free the predecessor array for a block.

 PARAMETERS		: psState	- Compiler state.
				  psBlock	- Block to modify.

 RETURNS		: Nothing.
******************************************************************************/
{
	UscFree(psState, psBlock->asPreds);
	psBlock->asPreds = NULL;
	psBlock->uNumPreds = 0;
}

static
IMG_VOID FreeSuccessors(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
/******************************************************************************
 FUNCTION		: FreeSuccessors

 DESCRIPTION	: Free the successors array for a block.

 PARAMETERS		: psState	- Compiler state.
				  psBlock	- Block to modify.

 RETURNS		: Nothing.
******************************************************************************/
{
	UscFree(psState, psBlock->asSuccs);
	psBlock->asSuccs = NULL;
	psBlock->uNumSuccs = 0;
}

IMG_INTERNAL
IMG_VOID ClearSuccessors(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
/******************************************************************************
 FUNCTION		: ClearSuccessors

 DESCRIPTION	: Remove edge to the block from the predecessor array of all
				  successors and free the successors array.

 PARAMETERS		: psState	- Compiler state.
				  psBlock	- Block to modify.

 RETURNS		: Nothing.
******************************************************************************/
{
	IMG_UINT32	uSucc;

	for (uSucc = 0; uSucc < psBlock->uNumSuccs; uSucc++)
	{
		PCODEBLOCK_EDGE	psEdge = &psBlock->asSuccs[uSucc];

		DropPredecessorFromDeltaInstructions(psState, psEdge->psDest, psEdge->uDestIdx);
		RemoveFromPredecessors(psState, psEdge->psDest, psEdge->uDestIdx);
	}
	FreeSuccessors(psState, psBlock);
}

IMG_INTERNAL
IMG_VOID SetBlockSwitch(PINTERMEDIATE_STATE psState,
						PCODEBLOCK psBlock,
						IMG_UINT32 uNumSuccs,
						PCODEBLOCK *apsSuccs,
						PARG psArg,
						IMG_BOOL bDefault,
						IMG_PUINT32 auCaseValues)
/******************************************************************************
 FUNCTION		: SetBlockSwitch

 DESCRIPTION	: Makes a codeblock into a switch block (i.e. which chooses
				  amongst its successors according to an integer value in an
				  arbitrary register - whether by lookup table, subsequent
				  translation into a series of conditionals, or whatever) by
				  setting all relevant fields (pbSyncEnd is initialised to all
				  FALSE). This overwrites any previous block type, tho the
				  predecessor lists of all (new and old) successors are updated

 PARAMETERS		: psState		- Compiler intermediate state
				  psBlock		- Block to make into a switch
				  uNumSuccs		- Number of successors (specifically, cases)
				  apsSuccs		- Successor of each case (see <auCaseValues>;
								  should have exactly uNumSuccs elements)
				  psArg			- Register whose value selects case
				  bDefault		- Whether the final successor (in <apsSuccs>)
								  applies if no case value matches psArg (if
								  false, auCaseValues must be an exhaustive
								  enumeration of all possible values of reg)
				  auCaseValues	- array of case values; if at runtime the value
								  of psArg equals auCaseValues[i], then the
								  chosen successor will be apsSuccs[i]. Thus,
								  if bDefault==FALSE, should have exactly
								  uNumSuccs elements; if bDefault==TRUE, then
								  one less, such that the final element of
								  apsSuccs will be selected if psArg's value
								  != auCaseValues[i] for any 0<i<uNumSuccs-1.

 RETURNS		: Nothing.

 NOTE			: for memory management purposes, a block is considered to own
				  its successor array, as well as syncend/auCaseValue arrays
				  (so previously-existing such will be freed), but NOT the
				  target of its psArg field if it's already a SWITCH.
******************************************************************************/
{
	IMG_UINT32 uSucc;
	
	/*
		Clear any existing block state.
	*/
	ClearBlockState(psState, psBlock);

	/*
		Clear any existing block successors.
	*/
	ClearSuccessors(psState, psBlock);

	psBlock->eType = CBTYPE_SWITCH;
	
	psState->uOptimizationHint |= USC_COMPILE_HINT_SWITCH_USED;
	
	psBlock->uNumSuccs = uNumSuccs;
	psBlock->asSuccs = UscAlloc(psState, sizeof(psBlock->asSuccs[0]) * uNumSuccs);
	psBlock->bStatic = IMG_FALSE;
	if (!psBlock->bStatic) psState->uNumDynamicBranches++;
	psBlock->u.sSwitch.pbSyncEnd = UscAlloc(psState, sizeof(IMG_BOOL) * uNumSuccs);
	memset(psBlock->u.sSwitch.pbSyncEnd, 0, sizeof(IMG_BOOL) * uNumSuccs);
	psBlock->u.sSwitch.psArg = psArg;
	psBlock->u.sSwitch.auCaseValues = auCaseValues;
	psBlock->u.sSwitch.bDefault = bDefault;

	UseDefResetArgument(&psBlock->u.sSwitch.sArgUse, USE_TYPE_SWITCH, USEDEF_TYPE_UNDEF, USC_UNDEF /* uLocation */, psBlock);

	//Add as predecessor of all successors.
	for (uSucc = 0; uSucc < uNumSuccs; uSucc++)
	{
		psBlock->asSuccs[uSucc].psDest = apsSuccs[uSucc];
		psBlock->asSuccs[uSucc].uDestIdx = AddAsPredecessor(psState, psBlock, apsSuccs[uSucc], uSucc);
	}
	psBlock->psOwner->bBlockStructureChanged = IMG_TRUE;
}

static
IMG_VOID UpdateSuccessor(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_UINT32 uNewSucc, IMG_UINT32 uOldSucc)
/******************************************************************************
 FUNCTION		: UpdateSuccessor

 DESCRIPTION	: Update the successor when moving an edge in the successor
				  array of a block.

 PARAMETERS		: psState			- Compiler state.
				  psBlock			- Block being modified.
				  uNewSucc			- New position in the successor array.
				  uOldSucc			- Old position in the successor array.

 RETURNS		: Nothing.
******************************************************************************/
{
	PCODEBLOCK_EDGE	psEdge = &psBlock->asSuccs[uNewSucc];
	PCODEBLOCK		psEdgeDest = psEdge->psDest;

	ASSERT(psEdge->uDestIdx < psEdgeDest->uNumPreds);
	ASSERT(psEdgeDest->asPreds[psEdge->uDestIdx].psDest == psBlock);
	ASSERT(psEdgeDest->asPreds[psEdge->uDestIdx].uDestIdx == uOldSucc);
	psEdgeDest->asPreds[psEdge->uDestIdx].uDestIdx = uNewSucc;
}

IMG_INTERNAL
IMG_VOID ExchangeConditionalSuccessors(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
/******************************************************************************
 FUNCTION		: ExchangeConditionalSuccessors

 DESCRIPTION	: Exchange the successors to a conditional block so if the predicate
				  is true we branch to the old false successor and vice-versa.

 PARAMETERS		: psBlock	- Block to initialize.

 RETURNS		: Nothing.
******************************************************************************/
{
	CODEBLOCK_EDGE		sTemp;

	ASSERT(psBlock->eType == CBTYPE_COND);
	ASSERT(psBlock->uNumSuccs == 2);

	sTemp = psBlock->asSuccs[0];

	psBlock->asSuccs[0] = psBlock->asSuccs[1];
	UpdateSuccessor(psState, psBlock, 0 /* uNewSucc */, 1 /* uOldSucc */);

	psBlock->asSuccs[1] = sTemp;
	UpdateSuccessor(psState, psBlock, 1 /* uNewSucc */, 0 /* uOldSucc */);
}

IMG_INTERNAL
IMG_VOID SetBlockConditional(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_UINT32 uPredSrc, PCODEBLOCK psTrueSucc, PCODEBLOCK psFalseSucc, IMG_BOOL bStatic)
/******************************************************************************
 FUNCTION		: SetBlockConditional

 DESCRIPTION	: Makes a codeblock into a conditional (i.e. which chooses
				  amongst its successors according to a boolean in a predicate
				  register) by setting all relevant fields (initially, syncend
				  is all FALSE). This overwrites any previous block type, tho
				  predecessor lists of all (new and old) successors are updated

 PARAMETERS		: psState		- Compiler intermediate state
				  psBlock		- Block to make into a conditional
				  uPredSrc		- Index of predicate reg controlling selection
				  psTrueSucc	- Block to execute if predicate reg stores TRUE
				  psFalseSucc	- Block to execute if predicate reg is FALSE
				  bStatic		- Whether the same successor is guaranteed to
								  be chosen for all pixels.

 RETURNS		: Nothing.

 NOTE			: for memory management purposes, a block is considered to own
				  its successor array, as well as syncend/auCaseValue arrays
				  (so previously-existing such will be freed), but NOT the
				  target of its psArg field if it's previously a SWITCH.
******************************************************************************/
{
	//quickie: prevent pointless conditionals
	if (psTrueSucc == psFalseSucc)
	{
		SetBlockUnconditional(psState, psBlock, psTrueSucc);
		return;
	}

	/*
		Clear any existing block state.
	*/
	ClearBlockState(psState, psBlock);

	/*
		Clear any existing block successors.
	*/
	ClearSuccessors(psState, psBlock);

	/*
		Add the true and false choices as the successors of the block.
	*/
	psBlock->eType = CBTYPE_COND;
	psBlock->asSuccs = UscAlloc(psState, 2 * sizeof(psBlock->asSuccs[0]));
	psBlock->uNumSuccs = 2;
	psBlock->asSuccs[0].psDest = psTrueSucc;
	psBlock->asSuccs[0].uDestIdx = AddAsPredecessor(psState, psBlock, psTrueSucc, 0 /* uSuccIdx */);
	psBlock->asSuccs[1].psDest = psFalseSucc;
	psBlock->asSuccs[1].uDestIdx = AddAsPredecessor(psState, psBlock, psFalseSucc, 1 /* uSuccIdx */);

	UseDefReset(&psBlock->u.sCond.sPredSrcUse, USE_TYPE_COND, USC_UNDEF /* uLocation */, psBlock);
		
	ASSERT (uPredSrc != USC_PREDREG_NONE);
	psBlock->u.sCond.sPredSrc.uType = USC_REGTYPE_UNUSEDSOURCE;
	SetConditionalBlockPredicate(psState, psBlock, uPredSrc);
	psBlock->bStatic = bStatic;
	#if defined(EXECPRED)
	if (!(psState->uCompilerFlags2 & UF_FLAGS2_EXECPRED) || !(psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_INTEGER_CONDITIONALS) || (psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_29643))
	{
	#endif /* #if defined(EXECPRED) */
		if (!psBlock->bStatic) psState->uNumDynamicBranches++;
	#if defined(EXECPRED)
	}
	#endif /* if defined(EXECPRED) */
	//would prefer to set to 'undefined'...
	psBlock->u.sCond.uSyncEndBitMask = 0;
}

IMG_INTERNAL
IMG_VOID SetBlockUnconditional(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PCODEBLOCK psSucc)
/******************************************************************************
 FUNCTION		: SetBlockUnconditional

 DESCRIPTION	: Makes a codeblock unconditional (i.e. control always flows to
				  the same single successor) by setting all relevant fields
				  (initially, syncend is FALSE). This overwrites any previous
				  block type, tho predecessor lists of all successors (new and
				  old) are updated.

 PARAMETERS		: psState		- Compiler intermediate state
				  psBlock		- Block to make unconditional
				  psSucc		- The unique successor block chosen in all cases

 RETURNS		: Nothing.

 NOTE			: for memory management purposes, a block is considered to own
				  its successor array, as well as syncend/auCaseValue arrays
				  (so previously-existing such will be freed), but NOT the
				  target of its psArg field if it's previously a SWITCH.
******************************************************************************/
{
	/*
		Clear any existing block successors.
	*/
	ClearSuccessors(psState, psBlock);

	/*
		Clear any state for the existing block.
	*/
	ClearBlockState(psState, psBlock);

	SetSingleSuccessor(psState, psBlock, psSucc, AddAsPredecessor(psState, psBlock, psSucc, 0 /* uSuccIdx */));

	InitializeUnconditionalBlock(psBlock);

	psBlock->psOwner->bBlockStructureChanged = IMG_TRUE;
}

#if defined(EXECPRED)
IMG_INTERNAL
IMG_VOID SetBlockConditionalExecPred(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PCODEBLOCK psTrueSucc, PCODEBLOCK psFalseSucc, IMG_BOOL bStatic)
/******************************************************************************
 FUNCTION		: SetBlockConditionalExecPred

 DESCRIPTION	: Makes a codeblock into a conditional (i.e. which chooses
				  amongst its successors according to a boolean in a Execution 
				  Predicate register) by setting all relevant fields (initially, 
				  syncend is all FALSE). This overwrites any previous block type, 
				  the predecessor lists of all (new and old) successors are 
				  updated

 PARAMETERS		: psState		- Compiler intermediate state
				  psBlock		- Block to make into a conditional				  
				  psTrueSucc	- Block to execute if execution predicate reg stores TRUE
				  psFalseSucc	- Block to execute if execution predicate reg is FALSE
				  bStatic		- Whether the same successor is guaranteed to
								  be chosen for all pixels.

 RETURNS		: Nothing.

 NOTE			: for memory management purposes, a block is considered to own
				  its successor array, as well as syncend/auCaseValue arrays
				  (so previously-existing such will be freed), but NOT the
				  target of its psArg field if it's previously a SWITCH.
******************************************************************************/
{
	//quickie: prevent pointless conditionals
	if (psTrueSucc == psFalseSucc)
	{
		SetBlockUnconditional(psState, psBlock, psTrueSucc);
		return;
	}

	/*
		Clear any existing block state.
	*/
	ClearBlockState(psState, psBlock);

	/*
		Clear any existing block successors.
	*/
	ClearSuccessors(psState, psBlock);

	/*
		Add the true and false choices as the successors of the block.
	*/
	psBlock->eType = CBTYPE_COND;
	psBlock->asSuccs = UscAlloc(psState, 2 * sizeof(psBlock->asSuccs[0]));
	psBlock->uNumSuccs = 2;
	psBlock->asSuccs[0].psDest = psTrueSucc;
	psBlock->asSuccs[0].uDestIdx = AddAsPredecessor(psState, psBlock, psTrueSucc, 0 /* uSuccIdx */);
	psBlock->asSuccs[1].psDest = psFalseSucc;
	psBlock->asSuccs[1].uDestIdx = AddAsPredecessor(psState, psBlock, psFalseSucc, 1 /* uSuccIdx */);
	psBlock->u.sCond.sPredSrc.uType = USC_REGTYPE_EXECPRED;
	psBlock->bStatic = bStatic;	
	//would prefer to set to 'undefined'...
	psBlock->u.sCond.uSyncEndBitMask = 0;
}
#endif /* #if defined(EXECPRED) */

IMG_INTERNAL
IMG_VOID MergeIdenticalSuccessors(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_UINT32 uSuccToRetain)
/******************************************************************************
 FUNCTION		: MergeIdenticalSuccessors

 DESCRIPTION	: Changes a conditional block with both successors pointing to the
				  same block to an unconditional block.

 PARAMETERS		: psState		- Compiler state.
				  psBlock		- Block to modify.
				  uSuccToRetain	- The successor edge whose entry in the successor's
								predecessor array is to be kept.

 RETURNS		: Nothing.
******************************************************************************/
{
	PCODEBLOCK		psSucc;
	IMG_UINT32		auPredIdx[2];
	IMG_UINT32		uSucc;
	IMG_UINT32		uPredToRemove;
	IMG_UINT32		uPredToRetain;

	ASSERT(psBlock->eType == CBTYPE_COND);
	ASSERT(psBlock->uNumSuccs == 2);
	ASSERT(psBlock->asSuccs[0].psDest == psBlock->asSuccs[1].psDest);
	psSucc = psBlock->asSuccs[0].psDest;

	for (uSucc = 0; uSucc < 2; uSucc++)
	{
		PCODEBLOCK_EDGE	psEdge;

		psEdge = &psBlock->asSuccs[uSucc];

		ASSERT(psEdge->uDestIdx < psSucc->uNumPreds);
		ASSERT(psSucc->asPreds[psEdge->uDestIdx].psDest == psBlock);
		ASSERT(psSucc->asPreds[psEdge->uDestIdx].uDestIdx == uSucc);
		auPredIdx[uSucc] = psEdge->uDestIdx;
	}

	/*
		Shrink the successor's array of predecessors so there is only one entry pointing
		to the block.
	*/
	uPredToRemove = auPredIdx[1 - uSuccToRetain];
	uPredToRetain = auPredIdx[uSuccToRetain];
	ASSERT(psSucc->asPreds[uPredToRetain].uDestIdx == uSuccToRetain);
	RemoveFromPredecessors(psState, psSucc, uPredToRemove);
	if (uPredToRetain > uPredToRemove)
	{
		uPredToRetain--;
	}

	/*
		Clear satellite data for a conditional block.
	*/
	ClearBlockState(psState, psBlock);

	/*
		Set the succcessor as the block's single successor.
	*/
	ASSERT(psSucc->asPreds[uPredToRetain].psDest == psBlock);
	psSucc->asPreds[uPredToRetain].uDestIdx = 0;
	SetSingleSuccessor(psState, psBlock, psSucc, uPredToRetain);

	/*
		Initialize satellite data for an unconditional block.
	*/
	InitializeUnconditionalBlock(psBlock);

	psBlock->psOwner->bBlockStructureChanged = IMG_TRUE;
}

IMG_INTERNAL
IMG_VOID DropPredecessorFromDeltaInstruction(PINTERMEDIATE_STATE psState, PINST psDeltaInst, IMG_UINT32 uPredToRemove)
/******************************************************************************
 FUNCTION		: DropPredecessorFromDeltaInstruction

 DESCRIPTION	: Modify a delta function to remove the argument
				  corresponding to a predecessor.

 PARAMETERS		: psState		- Compiler state.
				  psDeltaInst	- Instruction to modify.
				  uPredToRemove	- Predecessor which is to be removed.

 RETURNS		: Nothing.
******************************************************************************/
{
	IMG_UINT32	uArgIdx;

	/*
		Shuffle the arguments to the delta function down to remove the source corresponding
		to the removed predecessor.
	*/
	for (uArgIdx = uPredToRemove; uArgIdx < (psDeltaInst->uArgumentCount - 1); uArgIdx++)
	{
		MoveSrc(psState, psDeltaInst, uArgIdx, psDeltaInst, uArgIdx + 1);
	}
	if (psDeltaInst->uArgumentCount == 2)
	{
		/*
			Convert a delta function with one source to a simple move.
		*/
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (psDeltaInst->u.psDelta->bVector)
		{
			IMG_UINT32 uChan;

			SetOpcode(psState, psDeltaInst, IVMOV);
			psDeltaInst->auDestMask[0] = psDeltaInst->auLiveChansInDest[0];
			psDeltaInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
			psDeltaInst->u.psVec->aeSrcFmt[0] = psDeltaInst->asDest[0].eFmt;

			for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
			{
				SetSrcUnused(psState, psDeltaInst, 1 + uChan);
			}
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		if (psDeltaInst->asDest[0].uType == USEASM_REGTYPE_PREDICATE)
		{
			SetOpcode(psState, psDeltaInst, IMOVPRED);
		}
		else
		{
			SetOpcode(psState, psDeltaInst, IMOV);
		}
	}
	else
	{
		SetArgumentCount(psState, psDeltaInst, psDeltaInst->uArgumentCount - 1);
	}
}

IMG_INTERNAL
IMG_VOID DropPredecessorFromDeltaInstructions(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_UINT32 uPredToRemove)
/******************************************************************************
 FUNCTION		: DropPredecessorFromDeltaInstructions

 DESCRIPTION	: Modify delta functions in a block to remove the argument
				  corresponding to a predecessor.

 PARAMETERS		: psState		- Compiler state.
				  psBlock		- Block to modify.
				  uPredToRemove	- Predecessor which is to be removed.

 RETURNS		: Nothing.
******************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;
	PUSC_LIST_ENTRY	psNextListEntry;

	/*
		For all delta functions in the successor block.
	*/
	for (psListEntry = psBlock->sDeltaInstList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PDELTA_PARAMS	psDeltaParams;
		PINST			psDeltaInst;

		psNextListEntry = psListEntry->psNext;
		psDeltaParams = IMG_CONTAINING_RECORD(psListEntry, PDELTA_PARAMS, sListEntry);
		psDeltaInst = psDeltaParams->psInst;
		ASSERT(psDeltaInst->eOpcode == IDELTA);
		ASSERT(psDeltaInst->psBlock == psBlock);
		ASSERT(psDeltaInst->uArgumentCount == psBlock->uNumPreds);

		DropPredecessorFromDeltaInstruction(psState, psDeltaInst, uPredToRemove);
	}
}

IMG_INTERNAL
IMG_VOID SwitchConditionalBlockToUnconditional(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_BOOL bCondSrc)
/******************************************************************************
 FUNCTION		: SwitchConditionalBlockToUnconditional

 DESCRIPTION	: Changes a conditional block with a compile time known result to
				  an unconditional block.

 PARAMETERS		: psState	- Compiler state.
				  psBlock	- Block to modify.
				  bCondSrc	- Compile-time known value of the conditional source.

 RETURNS		: Nothing.
******************************************************************************/
{
	PCODEBLOCK		psDroppedSucc;
	PCODEBLOCK		psUncondSucc;
	IMG_UINT32		uPredToRemove;
	IMG_UINT32		uPredToRetain;
	IMG_UINT32		uSuccToDrop;
	IMG_UINT32		uSuccToRetain;

	ASSERT(psBlock->eType == CBTYPE_COND);
	ASSERT(psBlock->uNumSuccs == 2);

	uSuccToDrop = bCondSrc ? 1U : 0U;
	uSuccToRetain = 1 - uSuccToDrop;

	psDroppedSucc = psBlock->asSuccs[uSuccToDrop].psDest;
	uPredToRemove = psBlock->asSuccs[uSuccToDrop].uDestIdx;

	psUncondSucc = psBlock->asSuccs[uSuccToRetain].psDest;
	uPredToRetain = psBlock->asSuccs[uSuccToRetain].uDestIdx;

	/*
		Reshuffle the dropped successor's predecessors array to remove the reference to the
		conditional block.
	*/
	DropPredecessorFromDeltaInstructions(psState, psDroppedSucc, uPredToRemove);
	RemoveFromPredecessors(psState, psDroppedSucc, uPredToRemove);
	if (psUncondSucc == psDroppedSucc && uPredToRetain > uPredToRemove)
	{
		uPredToRetain--;
	}

	/*
		Clear existing state for the conditional block.
	*/
	ClearBlockState(psState, psBlock);

	/*
		Update the conditional block's successor array to contain just the always chosen successor.
	*/
	ASSERT(uPredToRetain < psUncondSucc->uNumPreds);
	ASSERT(psUncondSucc->asPreds[uPredToRetain].psDest == psBlock);
	ASSERT(psUncondSucc->asPreds[uPredToRetain].uDestIdx == uSuccToRetain);
	psUncondSucc->asPreds[uPredToRetain].uDestIdx = 0;
	SetSingleSuccessor(psState, psBlock, psUncondSucc, uPredToRetain);

	/*
		Initialize the new state for the conditional block.
	*/
	InitializeUnconditionalBlock(psBlock);

	psBlock->psOwner->bBlockStructureChanged = IMG_TRUE;
}

IMG_INTERNAL
IMG_VOID SetSyncEndOnSuccessor(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_UINT32 uSuccIdx)
/******************************************************************************
 FUNCTION		: SetSyncEndOnSuccessor

 DESCRIPTION	: Set the sync-end flag on the edge between a block and one of
				  its successor.

 PARAMETERS		: psState	- Compiler intermediate state
				  psBlock	- Block to set sync-end on the edge to a successor.
				  uSuccIdx	- Index of the successor to set sync-end on the edge to.
******************************************************************************/
{
	ASSERT(uSuccIdx < psBlock->uNumSuccs);
	switch (psBlock->eType)
	{
		case CBTYPE_SWITCH:
			psBlock->u.sSwitch.pbSyncEnd[uSuccIdx] = IMG_TRUE;
			break;
		case CBTYPE_COND:
		{
			psBlock->u.sCond.uSyncEndBitMask |= 1U << uSuccIdx;
			break;
		}
		case CBTYPE_UNCOND:
		{
			psBlock->u.sUncond.bSyncEnd = IMG_TRUE;
			break;
		}
		default:
			imgabort();
	}
}

IMG_INTERNAL
PCODEBLOCK AddEdgeViaEmptyBlock(PINTERMEDIATE_STATE psState, PCODEBLOCK psTo, IMG_UINT32 uPredIdx)
/******************************************************************************
 FUNCTION		: AddEdgeViaEmptyBlock

 DESCRIPTION	: Replace the edge to a block by an edge via a new block.

 PARAMETERS		: psState	- Compiler intermediate state
				  psTo		- Block to modify.
				  uPredIdx	- Index of the edge to the block.

  RETURNS		: A pointer to the new block.
******************************************************************************/
{
	PCODEBLOCK	psNewBlock;
	PCODEBLOCK	psOldPred;
	IMG_UINT32	uOldPredIdx;

	/*
		Get the old predecessor via the edge.
	*/
	ASSERT(uPredIdx < psTo->uNumPreds);
	psOldPred = psTo->asPreds[uPredIdx].psDest;
	uOldPredIdx = psTo->asPreds[uPredIdx].uDestIdx;	
	/*
		Create a new block.
	*/
	psNewBlock = AllocateBlock(psState, psTo->psOwner);
	InitializeUnconditionalBlock(psNewBlock);

	/*
		Set old predecessor as the predecessor of the new block.
	*/
	psNewBlock->uNumPreds = 1;
	psNewBlock->asPreds = UscAlloc(psState, sizeof(psNewBlock->asPreds[0]));
	psNewBlock->asPreds[0] = psTo->asPreds[uPredIdx];

	/*
		Set TO as the successor of the new block.
	*/
	SetSingleSuccessor(psState, psNewBlock, psTo, uPredIdx);

	/*
		Set the new block as the successor of the old predecessor.
	*/
	ASSERT(uOldPredIdx < psOldPred->uNumSuccs);
	ASSERT(psOldPred->asSuccs[uOldPredIdx].psDest == psTo);
	ASSERT(psOldPred->asSuccs[uOldPredIdx].uDestIdx == uPredIdx);
	psOldPred->asSuccs[uOldPredIdx].psDest = psNewBlock;
	psOldPred->asSuccs[uOldPredIdx].uDestIdx = 0;

	/*
		Replace the old predecessor with the new block as a predecessor of TO.
	*/
	psTo->asPreds[uPredIdx].psDest = psNewBlock;
	psTo->asPreds[uPredIdx].uDestIdx = 0;
	return psNewBlock;	
}

IMG_INTERNAL
IMG_VOID RedirectEdgesFromPredecessors(PINTERMEDIATE_STATE psState, 
									   PCODEBLOCK psFrom, 
									   PCODEBLOCK psTo,
									   IMG_BOOL bSyncEnd)
/******************************************************************************
 FUNCTION		: RedirectEdgesFromPredecessors

 DESCRIPTION	:

 PARAMETERS		: psState	- Compiler intermediate state
				  psFrom	- Old successor: remove edges from its predecessors
				  psTo		- New successor: add edges to this block
				  bSyncEnd	- Set sync-end on the edges to the new successor.

  RETURNS		: Nothing.
******************************************************************************/
{
	IMG_UINT32	uPred;
	
	ASSERT ((psState->uFlags & USC_FLAGS_INTERMEDIATE_CODE_GENERATED) != 0);

	/*
		Grow the new successor's array of predecessors.
	*/
	IncreaseArraySizeInPlace(psState,
				psTo->uNumPreds * sizeof(psTo->asPreds[0]),
				(psTo->uNumPreds + psFrom->uNumPreds) * sizeof(psTo->asPreds[0]),
				(IMG_PVOID*)&psTo->asPreds);

	/*
		Add the old successor's predecessors to the new successor's predecessor
		array.
	*/
	for (uPred = 0; uPred < psFrom->uNumPreds; uPred++)
	{
		PCODEBLOCK_EDGE	psPredEdge = &psFrom->asPreds[uPred];
		PCODEBLOCK_EDGE	psSuccEdge;
		IMG_UINT32		uNewPred = psTo->uNumPreds + uPred;

		ASSERT(psPredEdge->uDestIdx < psPredEdge->psDest->uNumSuccs);
		psSuccEdge = &psPredEdge->psDest->asSuccs[psPredEdge->uDestIdx];

		ASSERT(psSuccEdge->psDest == psFrom);
		ASSERT(psSuccEdge->uDestIdx == uPred);
		psSuccEdge->psDest = psTo;
		psSuccEdge->uDestIdx = uNewPred;

		psTo->asPreds[uNewPred] = *psPredEdge;

		if (bSyncEnd)
		{
			SetSyncEndOnSuccessor(psState, psPredEdge->psDest, psPredEdge->uDestIdx);
		}
	}

	/*
		If the old successor is the entrypoint of the function then update the entrypoint.
	*/
	if (psFrom->psOwner->psEntry == psFrom)
	{
		psFrom->psOwner->psEntry = psTo;
	}

	psTo->uNumPreds += psFrom->uNumPreds;

	/*
		Clear FROM's predecessor array.
	*/
	UscFree(psState, psFrom->asPreds);
	psFrom->uNumPreds = 0;
	psFrom->asPreds = NULL;

	/*
		We must re-sort blocks in the current function.
	*/
	psFrom->psOwner->pfnCurrentSortOrder = NULL;
}

IMG_INTERNAL
IMG_BOOL UnconditionallyFollows(PINTERMEDIATE_STATE	psState, PCODEBLOCK psBlock1, PCODEBLOCK psBlock2)
/*****************************************************************************
 FUNCTION	: UnconditionallyFollows

 PURPOSE    : Test whether one block will, directly or indirectly, be unconditionally
			  followed by another. Return TRUE if they are the same block.

 INPUT		: psState		- Internal compiler state
			  psBlock1		- Block to start in.
			  psBlock2		- Block to target.

 RETURNS	: IMG_TRUE or IMG_FALSE.
*****************************************************************************/
{
	if (psBlock1 == psBlock2)
	{
		return IMG_TRUE;
	}

	if (psBlock1->eType != CBTYPE_UNCOND)
	{
		return IMG_FALSE;
	}

	ASSERT(psBlock1->uNumSuccs == 1);

	return UnconditionallyFollows(psState, psBlock1->asSuccs[0].psDest, psBlock2);
}

IMG_INTERNAL
IMG_VOID ReplaceEdge(PINTERMEDIATE_STATE psState, PCODEBLOCK psSource, IMG_UINT32 uSucc, PCODEBLOCK psDest)
/******************************************************************************
 FUNCTION		: ReplaceEdge

 DESCRIPTION	: Replace an edge via a block with a direct edge.

 PARAMETERS		: psState	- Compiler state.
				  psSource	- Edge source.
				  uSucc		- Edge to modify.
				  psDest	- New edge destination.

 RETURNS		: Nothing.
******************************************************************************/
{
	IMG_UINT32	uPred;
	PCODEBLOCK	psOldSucc;
	IMG_BOOL	bFreedBlock;

	ASSERT (psSource->psOwner == psDest->psOwner);

	ASSERT(uSucc < psSource->uNumSuccs);
	psOldSucc = psSource->asSuccs[uSucc].psDest;

	ASSERT(psOldSucc->eType == CBTYPE_UNCOND);
	ASSERT(psOldSucc->uNumSuccs == 1);
	ASSERT(UnconditionallyFollows(psState, psOldSucc, psDest));
	ASSERT(psOldSucc->uNumPreds == 1);
	ASSERT(psOldSucc->asPreds[0].psDest == psSource);

	while (psOldSucc->asSuccs[0].psDest != psDest)
	{
		PCODEBLOCK psTemp = psOldSucc->asSuccs[0].psDest;

		FreePredecessors(psState, psOldSucc);
		FreeSuccessors(psState, psOldSucc);
		bFreedBlock = FreeBlock(psState, psOldSucc);
		ASSERT(bFreedBlock);

		psOldSucc = psTemp;
	}
	uPred = psOldSucc->asSuccs[0].uDestIdx;

	/*
		Update the successor/predecessor arrays.
	*/
	psSource->asSuccs[uSucc].psDest = psDest;
	psSource->asSuccs[uSucc].uDestIdx = uPred;
	ASSERT(uPred < psDest->uNumPreds);
	psDest->asPreds[uPred].psDest = psSource;
	psDest->asPreds[uPred].uDestIdx = uSucc;

	/*	
		Clear the predecessor/successor arrays of the old successor.
	*/
	FreePredecessors(psState, psOldSucc);
	FreeSuccessors(psState, psOldSucc);
	bFreedBlock = FreeBlock(psState, psOldSucc);
	ASSERT(bFreedBlock);

	psSource->psOwner->bBlockStructureChanged = IMG_TRUE;
}

IMG_INTERNAL
IMG_VOID RedirectEdge(PINTERMEDIATE_STATE psState, PCODEBLOCK psSource, IMG_UINT32 uSucc, PCODEBLOCK psDest)
{
	ASSERT (uSucc < psSource->uNumSuccs);
	ASSERT (psSource->psOwner == psDest->psOwner);
	RemoveFromPredecessors(psState, psSource->asSuccs[uSucc].psDest, psSource->asSuccs[uSucc].uDestIdx);
	psSource->asSuccs[uSucc].psDest = psDest;
	psSource->asSuccs[uSucc].uDestIdx = AddAsPredecessor(psState, psSource, psDest, uSucc);
	psSource->psOwner->bBlockStructureChanged = IMG_TRUE;
}

IMG_INTERNAL 
PCODEBLOCK AllocateBlock(PINTERMEDIATE_STATE psState, PCFG psCfg)
/*****************************************************************************
 FUNCTION	: AllocateCodeBlock

 PURPOSE	: Allocate a code block

 PARAMETERS	: psState	- Compiler state.
			  psCfg 	- CFG block will be part of
						  (uNumBlocks updated, but apsAllBlocks assumed null)
	
 RETURNS	: The new block. Initially, psOwner and uIdx will be valid,
				and uNumPreds = 0, uNumSuccs = -1.
*****************************************************************************/
{
	PCODEBLOCK psResult = (PCODEBLOCK)UscAlloc(psState, sizeof(CODEBLOCK));

	memset(psResult, 0, sizeof(*psResult));

	psResult->eType = CBTYPE_UNDEFINED;
	//psResult->uNumSuccs = 0; //memset

	//psResult->uNumPreds = 0; //memset

	psResult->uGlobalIdx = psState->uGlobalBlockCount++;
	AppendToList(&psState->sBlockList, &psResult->sBlockListEntry);

	AttachBlockToCfg(psState, psResult, psCfg);
	InitRegLiveSet( &psResult->sRegistersLiveOut );

	return psResult;
}

static IMG_VOID FreeInsts(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
//Free instructions...
{
	PINST psInst, psInstNext;
	
	/* Simple code, as follows, used to be in DropCodeBlock...*/
	//for (psInst = psBlock->psHead; psInst; )
	//{
	//	PINST psNext = psInst->psNext;
	//	FreeInst(psInst);
	//	psInst = psNext;
	//}
	
	/*
		...with the following more elaborate version reserved for ReleaseState (only).
		No idea why, so we're doing the thorough version here.
	*/
	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInstNext)
	{
		PINST psGroupNext;

		psInstNext = psInst->psNext;
		
		RemoveInst(psState, psBlock, psInst);
		for (; psInst != NULL; psInst = psGroupNext)
		{
			psGroupNext = psInst->psGroupNext;
			FreeInst(psState, psInst);
		}
	}

	ASSERT(psBlock->psBody == NULL);
	ASSERT(psBlock->psBodyTail == NULL);
	ASSERT(psBlock->uInstCount == 0);
}

static
IMG_VOID FreeBlockState(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
/******************************************************************************
 Function			: FreeBlockState

 Description		: Releases state for a basic block without updating the CFG.

 Return				: Nothing.
******************************************************************************/
{
	IMG_UINT32 uIRegIdx;

	if (psBlock->asPreds) UscFree(psState, psBlock->asPreds);
	if (psBlock->apsDomChildren) UscFree(psState, psBlock->apsDomChildren);
	if (psBlock->asSuccs) UscFree(psState, psBlock->asSuccs);
	if (psBlock->psSavedDepState) FreeDGraphState(psState, &psBlock->psSavedDepState);
	
	/*
		Free state used to chose the successor.
	*/
	if (psBlock->eType == CBTYPE_SWITCH)
	{
		UseDefDropUse(psState, &psBlock->u.sSwitch.sArgUse.sUseDef);
		UscFree(psState, psBlock->u.sSwitch.pbSyncEnd);
		UscFree(psState, psBlock->u.sSwitch.psArg);
		UscFree(psState, psBlock->u.sSwitch.auCaseValues);
	}
	else if (psBlock->eType == CBTYPE_COND)
	{
		UseDefDropUse(psState, &psBlock->u.sCond.sPredSrcUse);
	}
	
	/*
		Free all instructions in the block.
	*/
	FreeInsts(psState, psBlock);

	/*
		Drop USE-DEF information about internal registers in the block.
	*/
	for (uIRegIdx = 0; uIRegIdx < (sizeof(psBlock->apsIRegUseDef) / sizeof(psBlock->apsIRegUseDef[0])); uIRegIdx++)
	{
		if (psBlock->apsIRegUseDef[uIRegIdx] != NULL)
		{
			UseDefDelete(psState, psBlock->apsIRegUseDef[uIRegIdx]);
		}
	}

	/*
		Remove from the global list of flow control blocks.
	*/
	RemoveFromList(&psState->sBlockList, &psBlock->sBlockListEntry);

	psBlock->uIdx = UINT_MAX;
	psBlock->psOwner = NULL;
	
	ClearRegLiveSet(psState, &psBlock->sRegistersLiveOut);
	UscFree(psState, psBlock);
}

IMG_INTERNAL
IMG_VOID DetachBlockFromCfg(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PCFG psCfg)
{
	/*
		Remove the pointer to this block from the cfg.
	*/
	psCfg->apsAllBlocks[psBlock->uIdx] = psCfg->apsAllBlocks[ psCfg->uNumBlocks - 1];
	psCfg->apsAllBlocks[psBlock->uIdx]->uIdx = psBlock->uIdx;

	if (psState->uFlags & USC_FLAGS_INTERMEDIATE_CODE_GENERATED)
	{
		//apsAllBlocks is, and shall stay, an exact fit

		ResizeArray(psState,
					psCfg->apsAllBlocks,
					psCfg->uNumBlocks * sizeof(PCODEBLOCK),
					(psCfg->uNumBlocks - 1) * sizeof(PCODEBLOCK),
					(IMG_PVOID*)&psCfg->apsAllBlocks);
	}
	else
	{
		/*
			During intermediate code conversion.
			Thus, we allow apsAllBlocks to become too big; it'll be shrunk to fit later.
			(Note that if uNumBlocks drops to a power of 2, and AllocateBlock is then called,
			it'll deduce that apsAllBlocks needs growing, even when it doesn't. But this still
			seems cheaper than shrinking it, and then growing it *again*!)
		*/
		if (psCfg->uNumBlocks == 1)
		{
			UscFree(psState, psCfg->apsAllBlocks);
			psCfg->apsAllBlocks = NULL;
		}
	}

	psCfg->uNumBlocks--;
	psCfg->bBlockStructureChanged = IMG_TRUE;
}

IMG_INTERNAL
IMG_VOID AttachBlockToCfg(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PCFG psCfg)
{
	psBlock->uIdx = psCfg->uNumBlocks;
	switch (psState->uFlags & USC_FLAGS_INTERMEDIATE_CODE_GENERATED)
	{
		IMG_UINT32 uNewSize;
		case 0: //generating intermediate code
			if (psCfg->apsAllBlocks == NULL)
			{
				psCfg->apsAllBlocks = UscAlloc(psState, sizeof(PCODEBLOCK));
				break;
			}
			else if (psCfg->uNumBlocks == (psCfg->uNumBlocks & -((IMG_INT32)psCfg->uNumBlocks)))
			{
				//uNumBlocks has reached a power of 2, so apsAllBlocks full.
				uNewSize = psCfg->uNumBlocks * 2;
				//fallthrough to resize
			}
			else
			{
				//uNumBlocks not a power of two, so still space in apsAllBlocks
				break;
		default: //intermediate code generated; apsAllBlocks is, and shall stay, an exact fit.
				//note nothing falls through into this case, because of it's positioning in the 'else' clause after a 'break'
				uNewSize = psCfg->uNumBlocks + 1;
				//however, it falls through into the ResizeArray call just fine :-)
			}
		//last part of switch - all (&only) cases in which apsAllBlocks need resizing end up here.

		ResizeArray(psState,
					psCfg->apsAllBlocks,
					psCfg->uNumBlocks * sizeof(PCODEBLOCK),
					uNewSize * sizeof(PCODEBLOCK),
					(IMG_PVOID*)&psCfg->apsAllBlocks);
	}
	psBlock->psOwner = psCfg;
	psCfg->apsAllBlocks[psCfg->uNumBlocks] = psBlock;
	psCfg->uNumBlocks++;
	
	psCfg->bBlockStructureChanged = IMG_TRUE;
}

static
IMG_VOID FreeCfg(PINTERMEDIATE_STATE psState, CFG **ppsCfg)
{
	PCFG psCfg = *ppsCfg;
	IMG_UINT32 uI;
	psCfg->psEntry = psCfg->psExit = NULL;	
	for (uI=0; uI < psCfg->uNumBlocks; uI++)
	{
		PCODEBLOCK psBlock = psCfg->apsAllBlocks[uI];
		if(psBlock != NULL)
		{
			FreeBlockState(psState, psBlock);
		}
	}
	UscFree(psState, psCfg->apsAllBlocks);
	UscFree(psState, psCfg);
	*ppsCfg = NULL;
}

static
IMG_VOID MoveSubCfgInToCfg(PINTERMEDIATE_STATE psState, PCFG psCfg, PCODEBLOCK psParentBlock, PCFG psSubCfg)
{
	ASSERT(psState->uFlags & USC_FLAGS_INTERMEDIATE_CODE_GENERATED);
	{
		IMG_UINT32 uNewSize;
		uNewSize = psCfg->uNumBlocks + psSubCfg->uNumBlocks;
		ResizeArray(psState,
					psCfg->apsAllBlocks,
					psCfg->uNumBlocks * sizeof(PCODEBLOCK),
					uNewSize * sizeof(PCODEBLOCK),
					(IMG_PVOID*)&psCfg->apsAllBlocks);
	}
	{
		IMG_UINT32 uSubCfgBlkArrayIdx;
		for (uSubCfgBlkArrayIdx = 0; uSubCfgBlkArrayIdx < psSubCfg->uNumBlocks; uSubCfgBlkArrayIdx++)
		{
			psCfg->apsAllBlocks[psCfg->uNumBlocks + uSubCfgBlkArrayIdx] = psSubCfg->apsAllBlocks[uSubCfgBlkArrayIdx];
			(psCfg->apsAllBlocks[psCfg->uNumBlocks + uSubCfgBlkArrayIdx]->uIdx) += psCfg->uNumBlocks;
			psCfg->apsAllBlocks[psCfg->uNumBlocks + uSubCfgBlkArrayIdx]->psOwner = psCfg;
			psSubCfg->apsAllBlocks[uSubCfgBlkArrayIdx] = NULL;
		}
	}
	psCfg->uNumBlocks+= psSubCfg->uNumBlocks;
	RedirectEdgesFromPredecessors(psState, psParentBlock, psSubCfg->psEntry, IMG_FALSE);
	RedirectEdgesFromPredecessors(psState, psSubCfg->psExit, psParentBlock, IMG_FALSE);
	ClearSuccessors(psState, psSubCfg->psExit);
	FreeBlock(psState, psSubCfg->psExit);
	FreeCfg(psState, &psSubCfg);		
	psCfg->bBlockStructureChanged = IMG_TRUE;
}

IMG_INTERNAL
IMG_VOID AttachSubCfgs(PINTERMEDIATE_STATE psState, PCFG psCfg)
{
	IMG_UINT32 uBlockArrayIdx;
	IMG_UINT32 uNumBlocks = psCfg->uNumBlocks;
	for (uBlockArrayIdx = 0; uBlockArrayIdx < uNumBlocks; uBlockArrayIdx++)
	{
		if (psCfg->apsAllBlocks[uBlockArrayIdx]->uFlags & USC_CODEBLOCK_CFG)
		{
			AttachSubCfgs(psState, psCfg->apsAllBlocks[uBlockArrayIdx]->u.sUncond.psCfg);
			MoveSubCfgInToCfg(psState, psCfg, psCfg->apsAllBlocks[uBlockArrayIdx], psCfg->apsAllBlocks[uBlockArrayIdx]->u.sUncond.psCfg);
			psCfg->apsAllBlocks[uBlockArrayIdx]->uFlags &= ~USC_CODEBLOCK_CFG;
		}
	}
}

IMG_INTERNAL 
IMG_BOOL FreeBlock(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
/******************************************************************************
 Function			: FreeBlock

 Description		: Removes a basic block from the CFG, including from
					  its successors, but *not* from its predecessors (see below).
					  Also frees the block's instructions, and updates invariants for
					  indices / apsAllBlocks.

Return				: Whether deletion was successful; deletion will FAIL if the
							block has predecessors, or is its CFGs Entry or Exit node.
******************************************************************************/
{
	PCFG psCfg = psBlock->psOwner;
	IMG_UINT32 i;
	
	if (psBlock == NULL) return IMG_TRUE;
	
	ASSERT (psBlock->uIdx < psCfg->uNumBlocks);

	ASSERT (psCfg->apsAllBlocks[psBlock->uIdx] == psBlock);

	/*
		Check if there are still links to this block in the CFG.
	*/
	if (psBlock->uNumPreds > 0) return IMG_FALSE;
	if (psCfg->psEntry == psBlock || psCfg->psExit == psBlock) return IMG_FALSE;

	for (i = 0; i < psBlock->uNumSuccs; i++)
	{
		/*
			Remove the source argument corresponding to the edge from this block in all delta instructions
			in the successor.
		*/
		DropPredecessorFromDeltaInstructions(psState, psBlock->asSuccs[i].psDest, psBlock->asSuccs[i].uDestIdx);

		/*
			Drop the back-link to this block from the successor.
		*/
		RemoveFromPredecessors(psState, psBlock->asSuccs[i].psDest, psBlock->asSuccs[i].uDestIdx);
	}
	
	DetachBlockFromCfg(psState, psBlock, psCfg);
	
	FreeBlockState(psState, psBlock);
	
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL IsCall(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
/******************************************************************************
 FUNCTION		: IsCall

 DESCRIPTION	: Tells whether the specified block is a call to a function.

 PARAMETERS		: psState	- Current compilation context
				  psBlock	- Block to examine

 RETURNS		: TRUE if it's a call block, i.e. contains a single ICALL instr
				  FALSE otherwise (in which case should contain no ICALL insts)

 NOTE			: Also gathers together invariants about call blocks - they are
				  unconditional, unpredicated, and contain no other instrs;
				  DEBUG_CALLS builds also check other blocks contain no ICALLs.
******************************************************************************/
{
	if (psBlock->psBody == NULL ||
		psBlock->psBody->eOpcode != ICALL)
	{
		//not a CALL block.
#ifdef DEBUG_CALLS
		//(Optionally!) check there are no ICALLs anywhere in it...
		PINST psInst;
		for (psInst = psBlock->psBody; psInst; psInst = psInst->psNext)
		{
			ASSERT (psInst->eOpcode != ICALL);
		}
#endif /*DEBUG_CALLS*/
		return IMG_FALSE;
	}
	//is a call block. Check well-formed.
	ASSERT (psBlock->psBody == psBlock->psBodyTail);
	ASSERT (NoPredicate(psState, psBlock->psBody));
	ASSERT (psBlock->eType == CBTYPE_UNCOND);
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL IsNonMergable(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
/******************************************************************************
 FUNCTION		: IsNonMergable

 DESCRIPTION	: Tells whether the specified block non mergable to other 
				  blocks.

 PARAMETERS		: psState	- Current compilation context
				  psBlock	- Block to examine

 RETURNS		: TRUE if it's a call block, i.e. contains a single ICALL instr
				  FALSE otherwise (in which case should contain no ICALL insts)

 NOTE			: Also gathers together invariants about call blocks - they are
				  unconditional, unpredicated, and contain no other instrs;
				  DEBUG_CALLS builds also check other blocks contain no ICALLs.
******************************************************************************/
{
	if(IsCall(psState, psBlock))
	{
		return IMG_TRUE;
	}
	#if !defined(EXECPRED)
	return IMG_FALSE;
	#else /* #if !defined(EXECPRED) */
	if 
	(
		psBlock->psBody == NULL ||
		(!(g_psInstDesc[psBlock->psBody->eOpcode].uFlags2 & DESC_FLAGS2_NONMERGABLE))
	)
	{
		//not a non mergable block.
		#ifdef DEBUG
		//(Optionally!) check there are no non mergable insts. anywhere in it...
		PINST psInst;
		for (psInst = psBlock->psBody; psInst; psInst = psInst->psNext)
		{
			ASSERT (!(g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_NONMERGABLE));			
		}
		#endif
		return IMG_FALSE;
	}
	return IMG_TRUE;
	#endif /* #if !defined(EXECPRED) */
}

IMG_INTERNAL
IMG_BOOL MergeBasicBlocks(PINTERMEDIATE_STATE psState, PFUNC psFunc)
/******************************************************************************
 FUNCTION	: MergeBasicBlocks

 PURPOSE    : Apply a number of standard optimizations on BB structure
              including: combine all consecutively executed basic-blocks
			  (single-successor with no other predecessors); delete
			  unreachable blocks; optimize branches over empty blocks;
			  etc.

 PARAMETERS	: psState	- Compiler state.
			  psFunc	- Function/CFG to optimize.

 RETURNS	: Whether any blocks were merged (does not include deleting unreachables)

 NOTE		: If after merging the function is found to be empty, it is deleted
			  along with all CALLs to it. Thus psFunc may be invalid on return.

              This is very expensive if called before
			  FinaliseIntermediateCode, as blocks may not have
			  predecessor arrays. Also note, FinaliseIntermediateCode
			  calls MergeBB on all CFGs itself.
******************************************************************************/
{
	IMG_BOOL bMerged = IMG_FALSE;
	IMG_BOOL bFreed;

#ifdef DEBUG
	ASSERT (psState->uBlockIterationsInProgress == 0);
#endif /* DEBUG */

	if (!psFunc->sCfg.bBlockStructureChanged && !psFunc->sCfg.bEmptiedBlocks) return IMG_FALSE;
	do
	{
		IMG_UINT32 uExamine = 0;
		psFunc->sCfg.bBlockStructureChanged = IMG_FALSE;
		psFunc->sCfg.bEmptiedBlocks = IMG_FALSE;
		while (uExamine < psFunc->sCfg.uNumBlocks)
		{
			PCODEBLOCK	psBlock = psFunc->sCfg.apsAllBlocks[uExamine];
			IMG_BOOL	bUnreachableBlock;

			ASSERT (psBlock->psOwner->psFunc == psFunc);

			bUnreachableBlock = IMG_FALSE;
			if (psFunc->sCfg.psEntry != psBlock)
			{
				if (psBlock->uNumPreds == 0 || (psBlock->uNumPreds == 1 && psBlock->asPreds[0].psDest == psBlock))
				{
					bUnreachableBlock = IMG_TRUE;
				}
			}

			if (bUnreachableBlock)
			{
				/*
					This block can never be reached so it should be deleted
				*/
				if (psBlock->uNumPreds > 0)
				{
					ClearSuccessors(psState, psBlock);
				}
				if (psFunc->sCfg.psExit == psBlock)
				{
					//can't delete; so empty it instead.
					FreeInsts(psState, psBlock);
					uExamine++; //skip over it
				}
				else
				{
					//Freeing block includes removing it from apsPreds of all succs
#if defined(DEBUG)
					bFreed =
#endif
					FreeBlock(psState, psBlock);
#if defined(DEBUG)
					ASSERT (bFreed);
#endif
					//bMerged = IMG_TRUE; //ACL that's what MergeBB's spec says...!
					//leave uExamine - another block will be moved into uIdx
				}
				continue;
			}

			if (psBlock->eType == CBTYPE_COND)
			{
				ASSERT(psBlock->uNumSuccs == 2);
				if (psBlock->asSuccs[0].psDest == psBlock->asSuccs[1].psDest && 
					IsListEmpty(&psBlock->asSuccs[0].psDest->sDeltaInstList))
				{
					MergeIdenticalSuccessors(psState, psBlock, 0 /* uSuccToRetain */);
				}
			}

			switch (psBlock->eType)
			{
				case CBTYPE_COND:
					/*
						there is a possible case here: if the block is
						empty, could make any of its predecessors
						which were unconditional bypass it (by copying
						its successor-choosing code back to
						them). However, this does not seem of much
						benefit (the eventual branch code might even
						be duplicated!), nor does it seem very likely
						to occur (as the predicate would have to have
						been precalculated)...
					*/
				default:
					break;
				case CBTYPE_UNCOND:
				{
					/* The unique successor */
					PCODEBLOCK psSucc = psBlock->asSuccs[0].psDest;

					/*
						Unconditional block pointing to itself (halt state)
						Continue to next block
					*/
					if(psSucc == psBlock)
					{
						uExamine++;
						continue;
					}

					if (psBlock == psState->psPreFeedbackBlock)
					{
						break;
					}
					if (psBlock == psState->psPreFeedbackDriverEpilogBlock)
					{
						break;
					}
					if (psBlock == psState->psPreSplitBlock)
					{
						break;
					}
					#if defined (EXECPRED)
					if	(
							(	
								(psBlock->psBody != NULL) && (psBlock->psBody->eOpcode == ICNDST)
							)
							&&
							(
								(psSucc->psBody != NULL) && (psSucc->psBody->eOpcode == ICNDEF)
							)
						)
					{
						PINST psTempInst = psSucc->psBody;
						RemoveInst(psState, psSucc, psSucc->psBody);
						FreeInst(psState, psTempInst);
						psTempInst = psBlock->psBody;
						RemoveInst(psState, psBlock, psBlock->psBody);
						AppendInst(psState, psSucc, psTempInst);						
						if(psSucc->psBody->asArg[2].uNumber == 0)						
						{
							psSucc->psBody->asArg[2].uNumber = 1;
						}
						else
						{
							psSucc->psBody->asArg[2].uNumber = 0;
						}
						psBlock->psBody = NULL;						
					}
					else
					#endif /* #if defined (EXECPRED) */
					{
						if (psBlock->psBody || psBlock->bAddSyncAtStart)
						{
							PINST		psInst, psNextInst;
							PINST		psOldFirstInst;

							/*
								if successor has no other predecessors, can copy
								code into said successor; otherwise, can't, so fail
							*/
							if (psFunc->sCfg.psEntry == psSucc || psSucc->uNumPreds > 1) break;

							ASSERT (psBlock->u.sUncond.bSyncEnd == IMG_FALSE);
							if (IsNonMergable(psState, psBlock) || IsNonMergable(psState, psSucc))
							{
								//merging would break invariant re. CALLs or EXECPRED, so fail
								break; //out of switch
							}

							/*
								Copy instructions.
							*/
							psOldFirstInst = psSucc->psBody;
							for (psInst = psBlock->psBody; psInst != NULL; psInst = psNextInst)
							{
								psNextInst = psInst->psNext;

								RemoveInst(psState, psBlock, psInst);
								InsertInstBefore(psState, psSucc, psInst, psOldFirstInst);
							}

							ASSERT (psBlock->psExtPostDom == psSucc->psExtPostDom); //prob. both NULL
						}
						else
						{
							if (!IsListEmpty(&psSucc->sDeltaInstList))
							{
								break;
							}
						}
					}

					//copy other characteristics
					if (psBlock->bDomSync)
					{
						psSucc->bDomSync = IMG_TRUE;
						if (psBlock->bDomSyncEnd) psSucc->bDomSyncEnd = IMG_TRUE;
					}
					if (psBlock->bAddSyncAtStart) psSucc->bAddSyncAtStart = IMG_TRUE;

					//body of psBlock should now be empty even if wasn't before
					ASSERT (psBlock->psBody == NULL);
					//ok - branch optimizations, avoid going through psBlock and go straight to psSucc
					bMerged = IMG_TRUE;
					/*
						note that psSucc has at least one predecessor: psBlock.
						Thus, during FinaliseIntermediateCode, even tho
						USC_FLAGS_INTERMEDIATE_CODE_GENERATED is not set,
						psSucc->apsPreds will be non-null, and thus will be
						updated & resized by AddAsPredecessor etc., and so will
						remain correct throughout the following call.
					*/
					RemoveFromPredecessors(psState, psSucc, psBlock->asSuccs[0].uDestIdx);
					FreeSuccessors(psState, psBlock);
					RedirectEdgesFromPredecessors(psState, 
												  psBlock, 
												  psSucc, 
												  psBlock->u.sUncond.bSyncEnd); //includes setting cfg psEntry

					//edge psBlock->psSucc is removed in FreeBlock, below.

					//tidy up. 
					bFreed = FreeBlock(psState, psBlock);
					ASSERT (bFreed);

					/*
						FreeBlock'll have moved another block into psBlock's
						old uIdx. We'll examine that next...
					*/
					continue;
				} //end of case unconditional (the interesting one!)
			} //end of switch
			//break arrives here. psBlock survived; examine next.
			uExamine++;
		}
		/*
			if any blocks were dropped, predecessor etc. info of *other* blocks
			may have changed, potentially enabling new/further optimizations.
			Hence, repeat.
		*/
	} while (psFunc->sCfg.bBlockStructureChanged || psFunc->sCfg.bEmptiedBlocks);

	//Check if there's anything left in a function...
	if (psFunc->sCfg.uNumBlocks == 1 && psFunc->sCfg.psEntry->psBody == NULL)
	{
		//Nope, it's empty
		PINST psInst, psNextInst;
		IMG_PCHAR pchDesc = psFunc->pchEntryPointDesc;
		
		//the function must be reachable somehow:
		ASSERT (psFunc->psCallSiteHead || pchDesc);

		//but it's a no-op regardless, so CALLs are pointless - remove them!
		for (psInst = psFunc->psCallSiteHead; psInst; psInst = psNextInst)
		{
			psNextInst = psInst->u.psCall->psCallSiteNext;
			RemoveInst(psState, psInst->u.psCall->psBlock, psInst);
			FreeInst(psState, psInst);
		}
		//FreeInst will delete FN unless externally visible...
		if (pchDesc == NULL) return IMG_TRUE; //...so avoid resizing Free'd FN
	}
	
	psFunc->sCfg.pfnCurrentSortOrder = NULL; //remove cached sort order
	/*
		TODO - consider separately caching whether current dom. tree is valid?
		Preliminary investigation suggests not worth it as CalcDoms is so fast.
	*/
	CalcDoms(psState, &(psFunc->sCfg));
	return bMerged;
}

IMG_INTERNAL
IMG_VOID MergeAllBasicBlocks(PINTERMEDIATE_STATE psState)
/******************************************************************************
 FUNCTION		: MergeAllBasicBlocks

 DESCRIPTION	: Calls MergeBasicBlocks on every function (inc main + SA prog)

 PARAMETERS		: psState	- Compiler intermediate state; contains all funcs!

 RETURNS		: Nothing
******************************************************************************/
{
	PFUNC psFunc, psNextFunc;
	//Innermost first, to cope with functions being deleted
	for (psFunc = psState->psFnInnermost; psFunc; psFunc = psNextFunc)
	{
		psNextFunc = psFunc->psFnNestOuter;
		MergeBasicBlocks(psState, psFunc);
	}
}

static IMG_VOID FillPredecessorArraysBP(PINTERMEDIATE_STATE	psState,
										PCODEBLOCK			psBlock,
										IMG_PVOID			pvNull)
/******************************************************************************
 FUNCTION		: FillPredecessorArraysBP

 DESCRIPTION	: BLOCK_PROC to setup predecessor arrays after input conversion
				  (part of FinaliseIntermediateCode)

 PARAMETERS		: psState	- Compiler intermediate state
				  psBlock	- block to process
				  pvNull	- IMG_PVOID to fit callback signature; unused

 RETURNS		: Nothing
******************************************************************************/
{
	IMG_UINT32 uSucc;
	PVR_UNREFERENCED_PARAMETER(pvNull);

	//psBlock has a valid array of successors - so add as a predecessor to each
	for (uSucc = 0; uSucc < psBlock->uNumSuccs; uSucc++)
	{
		PCODEBLOCK psSucc = psBlock->asSuccs[uSucc].psDest;

		if (psSucc->asPreds == NULL)
		{
			//psBlock is first predecessor to try adding - so make the array
			ASSERT (psSucc->uNumPreds); //we are a predecessor!
			psSucc->asPreds = UscAlloc(psState,
									   psSucc->uNumPreds * sizeof(psSucc->asPreds[0]));
			psSucc->uNumPreds = 0; //used as 'next index into apsPreds to fill'
		}
		psSucc->asPreds[psSucc->uNumPreds].psDest = psBlock;
		psSucc->asPreds[psSucc->uNumPreds].uDestIdx = uSucc;
		psBlock->asSuccs[uSucc].uDestIdx = psSucc->uNumPreds;
		psSucc->uNumPreds++;
	}
}

static
IMG_VOID SetupMasksAndPredicatesBP(PINTERMEDIATE_STATE	psState,
								   PCODEBLOCK			psBlock,
								   IMG_PVOID			pvNULL)
{
	PINST	psInst;

	PVR_UNREFERENCED_PARAMETER(pvNULL);

	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		IMG_UINT32	uDestIdx;

		for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
		{
			if (
					psInst->apsOldDest[uDestIdx] == NULL && 
					(
						!NoPredicate(psState, psInst) || 
						psInst->auDestMask[uDestIdx] != USC_ALL_CHAN_MASK
					)
				)
			{
				SetPartiallyWrittenDest(psState, psInst, uDestIdx, &psInst->asDest[uDestIdx]);
			}
		}
	}
}

/******************************************************************************
 FUNCTION		: FinaliseIntermediateCode

 DESCRIPTION	: Call this when intermediate code has been generated (from
				  whatever input). It patches up several invariants on the
				  intermediate code which we relax during generation (as we
				  hypothesize that maintaining these during generation will be
				  very expensive), specifically:

				  (1) apsPreds arrays of CODEBLOCKs: during generation, we
				  maintain uNumPreds only, and leave these arrays NULL, to
				  avoid repeated resizing. Thus, this procedure fills these in
				  (backwards, from each block's successors).

				  (2) It also calls MergeBasicBlocks, as this is very expensive
				  during generation, but becomes much cheaper when predecessor
				  info has been computed, and further, has the effect of resizing
				  the apsAllBlocks arrays to be an exact fit (during generation,
				  these are allocated out to (some) sufficient power of 2, again
				  to reduce resizing).

				  (3) It also sorts the functions (in the psState->psFnInnermost
				  list) into nesting order (according to their uNestingLevel, which
				  must be set before this is called); during generation, said list
				  need not be kept sorted.

 PARAMETERS		: psState		- Compiler state (contains converted program)

 RETURNS		: Nothing
******************************************************************************/
IMG_INTERNAL IMG_VOID FinaliseIntermediateCode(PINTERMEDIATE_STATE psState)
{
	ASSERT ((psState->uFlags & USC_FLAGS_INTERMEDIATE_CODE_GENERATED) == 0);

	//1. sort functions into nesting order
	if (psState->uMaxLabel)
	{
		//1a. make array
		PFUNC *apsFuncs = UscAlloc(psState, (psState->uMaxLabel + 1)* sizeof(PFUNC)), *pPtr = apsFuncs;
		PFUNC psTemp;
		for (psTemp = psState->psFnOutermost; psTemp; psTemp = psTemp->psFnNestInner)
			*(pPtr++) = psTemp;
		*pPtr = NULL; //sentinel

		//1b. repeatedly find outermost remaining function...
		psState->psFnInnermost = NULL;
		for (pPtr = apsFuncs; *pPtr; )
		{
			PFUNC psOuter = *pPtr, *ppsSearch;
			for (ppsSearch = ++pPtr; *ppsSearch; ppsSearch++)
			{
				if ((*ppsSearch)->uNestingLevel > psOuter->uNestingLevel || psOuter == psState->psSecAttrProg)
				{
					psTemp = psOuter;
					psOuter = *ppsSearch;
					*ppsSearch = psTemp;
				}
			}
			
			//...and add to psState-> list
			psOuter->psFnNestOuter = psState->psFnInnermost;
			
			if (psState->psFnInnermost)
			{
				psState->psFnInnermost->psFnNestInner = psOuter;
			}
			else
			{
				psState->psFnOutermost = psOuter;
			}
			psState->psFnInnermost = psOuter;
		}
		psState->psFnInnermost->psFnNestInner = NULL;
		
		UscFree(psState, apsFuncs);
#ifdef DEBUG
		{
			//check sort order and list consistency.
			PFUNC psFunc = psState->psFnOutermost;
			ASSERT (psFunc->psFnNestOuter == NULL);
			for (; psFunc->psFnNestInner; psFunc = psFunc->psFnNestInner)
			{
				ASSERT (psFunc->psFnNestInner->uNestingLevel <= psFunc->uNestingLevel);
				ASSERT (psFunc->psFnNestInner->psFnNestOuter == psFunc);
			}
		}
#endif /*DEBUG*/
		/* 
			Function inlining is now done as part of DCE, as removing
			dead CALLs tends to make our heuristic do more inlining.
		*/
	}

	/*
		Delete never called functions.
	*/
	{
		PFUNC	psFunc, psFnNestOuter;

		for (psFunc = psState->psFnInnermost; psFunc != NULL; psFunc = psFnNestOuter)
		{
			psFnNestOuter = psFunc->psFnNestOuter;
			if (psFunc->psCallSiteHead == NULL && psFunc->pchEntryPointDesc == NULL)
			{
				FreeFunction(psState, psFunc);
			}
		}
	}
	
	//patch up apsArrays...
	DoOnAllBasicBlocks(psState, ANY_ORDER, FillPredecessorArraysBP,
					   IMG_TRUE/*CALLs*/, NULL);

	/*
		Cheap to MergeBBs now that predecessors have been computed.
		This also deletes any empty functions and any calls to them,
		and resizes apsAllBlocks to fit (once only, as
		USC_FLAGS_INTERMEDIATE_CODE_GENERATED is not yet set)
	*/
	psState->uFlags |= USC_FLAGS_INTERMEDIATE_CODE_GENERATED;
	MergeAllBasicBlocks(psState);
	if(((psState->uFlags2 & USC_FLAGS2_SPLITCALC) != 0) && (psState->psPreSplitBlock != NULL))
	{
		if (!Dominates(psState, psState->psPreSplitBlock, psState->psMainProg->sCfg.psExit))
		{
			USC_ERROR(UF_ERR_INVALID_PROG_STRUCT, "Can not split program at the specified point");
		}
	}

	/*
	*/
	DoOnAllBasicBlocks(psState, ANY_ORDER, SetupMasksAndPredicatesBP, IMG_FALSE /* bHandleCalls */, NULL /* pvUserData */);
}

/*****************************************************************************/
/*************************** Traversal Routines... ***************************/
/*****************************************************************************/

/* Note closure should compute value at beginning of BB, given values at beginning of successors;
 * for Live Registers, we expect to save in the BB structure the value at the *end* of the block
 * (i.e. the merge of the successors, but before the body of the BB is considered).
 */


IMG_INTERNAL
IMG_VOID InitializeBlockWorkList(PBLOCK_WORK_LIST psList)
{
	psList->psHead = psList->psTail = NULL;
}

IMG_INTERNAL
IMG_BOOL IsBlockWorkListEmpty(PBLOCK_WORK_LIST psList)
{
	return psList->psHead == NULL ? IMG_TRUE : IMG_FALSE;
}

IMG_INTERNAL
IMG_BOOL IsBlockInBlockWorkList(PBLOCK_WORK_LIST psList, PCODEBLOCK psBlock)
{
	if (psBlock->psWorkListNext == NULL && psBlock != psList->psTail)
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_VOID PrependToBlockWorkList(PBLOCK_WORK_LIST psList, PCODEBLOCK psBlockToAdd)
{
	if (IsBlockInBlockWorkList(psList, psBlockToAdd))
	{
		return;
	}

	psBlockToAdd->psWorkListNext = psList->psHead;
	psList->psHead = psBlockToAdd;
	if (psList->psTail == NULL)
	{
		psList->psTail = psBlockToAdd;
	}
}

IMG_INTERNAL
IMG_VOID AppendToBlockWorkList(PBLOCK_WORK_LIST psList, PCODEBLOCK psBlockToAdd)
{
	if (IsBlockInBlockWorkList(psList, psBlockToAdd))
	{
		return;
	}

	psBlockToAdd->psWorkListNext = NULL;
	if (psList->psTail != NULL)
	{
		psList->psTail->psWorkListNext = psBlockToAdd;
	}
	else
	{
		psList->psHead = psBlockToAdd;
	}
	psList->psTail = psBlockToAdd;
}

IMG_INTERNAL
IMG_VOID AppendPredecessorsToBlockWorkList(PBLOCK_WORK_LIST psList, PCODEBLOCK psBlock)
{
	IMG_UINT32	uPredIdx;

	for (uPredIdx = 0; uPredIdx < psBlock->uNumPreds; uPredIdx++)
	{
		PCODEBLOCK	psPred = psBlock->asPreds[uPredIdx].psDest;

		AppendToBlockWorkList(psList, psPred);
	}
}

IMG_INTERNAL
PCODEBLOCK RemoveBlockWorkListHead(PINTERMEDIATE_STATE psState, PBLOCK_WORK_LIST psList)
{
	PCODEBLOCK	psRemovedBlock;

	if (psList->psHead == NULL)
	{
		ASSERT(psList->psTail == NULL);
		return NULL;
	}

	psRemovedBlock = psList->psHead;
	psList->psHead = psRemovedBlock->psWorkListNext;
	if (psList->psTail == psRemovedBlock)
	{
		ASSERT(psList->psHead == NULL);
		psList->psTail = NULL;
	}
	psRemovedBlock->psWorkListNext = NULL;
	return psRemovedBlock;
}

IMG_INTERNAL IMG_VOID DoDataflow(PINTERMEDIATE_STATE psState, PFUNC psFunc,
								IMG_BOOL bForwards,
								IMG_UINT32 uSize, IMG_PVOID pvWorking,
								DATAFLOW_FN pfClosure, IMG_PVOID pvUserData)
/******************************************************************************
 Function		: DoDataflow
 
 Description	: Performs a dataflow analysis on a CFG until convergence.

 Parameters		: psFunc	- Flowgraph on which to iterate analysis
				  bForwards	- Direction of dataflow (see pfClosure).
				  uSize		- size in memory of a dataflow value
				  pvWorking	- points to area of memory, of size at least uSize
							  *per block*, used for working values; the caller
							  may fill with block-specific initial values.
				  pfClosure	- Function to compute dataflow value for a block,
							  given corresponding values at adjacent blocks.
							  Specifically:
								if bForwards, computes value at END of block,
								  given values at ENDs of PREDECESSORS; returns
								  whether to add SUCCESSORS to worklist.
								if !bForwards, computes value at block START,
								  given values at STARTs of SUCCESSORS; returns
								  whether to add PREDECESSORS to worklist.
				  pvUserData - Additional data to pass to pfClosure

 Returns		: None, but final values for each block are left in pvWorking.
******************************************************************************/
{
	IMG_UINT32 i, uInputArraySize = 5;
	IMG_PVOID *apvInputs = UscAlloc(psState, uInputArraySize * sizeof(IMG_PVOID));
	PCODEBLOCK psWorkListHead = NULL, psWorkListTail = NULL;

	/*
		set up work list. Since the order in which we consider blocks does not
		affect correctness, we just use *whatever order the blocks happen to be
		sorted in*; however, since said ordering may affect performance, we
		take the chance of reversing it for backwards analyses.
	*/
	for (i = 0; i < psFunc->sCfg.uNumBlocks; i++)
	{
		IMG_UINT32 uIdx = (bForwards) ? i : (psFunc->sCfg.uNumBlocks - 1 - i);
		PCODEBLOCK psAddToList = psFunc->sCfg.apsAllBlocks[ uIdx ];
		if (psWorkListTail)
		{
			psWorkListTail->psWorkListNext = psAddToList;
		}
		else
		{
			psWorkListHead = psAddToList;
		}
		psWorkListTail = psAddToList;
		psAddToList->psWorkListNext = NULL;
	}
	
	//ok - just keep going while there are blocks to process.
	while (psWorkListHead)
	{
		PCODEBLOCK psProcess = psWorkListHead;
		PCODEBLOCK_EDGE apsAdj = bForwards ? psProcess->asPreds : psProcess->asSuccs;
		IMG_UINT32 uNumAdj = bForwards ? psProcess->uNumPreds : psProcess->uNumSuccs;
		
		/* Remove from WorkList */
		psWorkListHead = psWorkListHead->psWorkListNext;
		psProcess->psWorkListNext = NULL;
		if (psWorkListHead == NULL)
		{
			ASSERT (psWorkListTail == psProcess);
			psWorkListTail = NULL;
		}
		
		/* Set up array of pointers to data for predecessors/successors */
		if (uNumAdj > uInputArraySize)
		{
			UscFree(psState, apvInputs);
			uInputArraySize = uNumAdj;
			apvInputs = UscAlloc(psState, uInputArraySize * sizeof(IMG_PVOID));
		}
		
		while (uNumAdj-- > 0) //NOTE DECREMENT
		{
			//use pointer arithmetic within working array, based on uSize.
			IMG_UINTPTR_T uAddr = ((IMG_UINTPTR_T)pvWorking) 
				+ (apsAdj[uNumAdj].psDest->uIdx * uSize);
			apvInputs[uNumAdj] = (IMG_PVOID)(uAddr);
		}
		
		// Right - parameters setup. Invoke closure... 
		if (pfClosure(psState,
				  psProcess,
				  (IMG_PVOID)( ((IMG_UINTPTR_T)pvWorking) + psProcess->uIdx*uSize),
				  apvInputs,
				  pvUserData))
		{
			/*
				Closure says to continue (i.e. different value produced)
				- so add all dependents to worklist
			*/
			i=(bForwards) ? psProcess->uNumSuccs : psProcess->uNumPreds;
			while (i-- > 0) //NOTE DECREMENT
			{
				PCODEBLOCK psAdd;

				if (bForwards)
				{
					psAdd = psProcess->asSuccs[i].psDest;
				}
				else
				{
					psAdd = psProcess->asPreds[i].psDest;
				}
				if (psAdd->psWorkListNext || psAdd == psWorkListTail) continue; //already in worklist
				if (psWorkListTail)
				{
					ASSERT (psWorkListTail->psWorkListNext == NULL);
					psWorkListTail->psWorkListNext = psAdd;
				}
				else
				{
					ASSERT (psWorkListHead == NULL);
					psWorkListHead = psAdd;
				}
				psWorkListTail = psAdd;
			}
		}
	}
	ASSERT (psWorkListTail == NULL);

	UscFree(psState, apvInputs);
}

IMG_INTERNAL
IMG_VOID DoOnCfgBasicBlocks(PINTERMEDIATE_STATE		psState,
							PCFG					psCfg,
							BLOCK_SORT_FUNC			pfnSort,
							BLOCK_PROC				pfClosure,
							IMG_BOOL				bHandlesCalls,
							IMG_PVOID				pvUserData)
/******************************************************************************
 Function		: DoOnCfgBasicBlocks

 Description	: Apply a simple BLOCK_PROC callback exactly once to each block
				  in a CFG, in a specified order (or any)

 Parameters		: psState		- Current compilation context
				  pfnSort		- callback to sort the function's blocks into
								  the required order; NULL if any order is ok.
				  pfClosure		- callback to apply to each block after sorting
				  bHandlesCalls - whether the callback should be passed blocks
								  representing function calls (see IsCall)
				  pUserData		- additional data to pass to pfClosure.

 RETURNS		: Nothing

 NOTE			: if pfClosure is NULL but pfnSort is provided, _just_ sorts.
******************************************************************************/
{
	IMG_UINT32 uBlock;
	if (psCfg->psEntry == NULL)
	{
		ASSERT (psCfg->uNumBlocks == 0);
		return;
	}
	
	//1. is an order required?
	if (pfnSort)
	{
		//sort lazily only if blocks are not already in desired order
		if (psCfg->pfnCurrentSortOrder != pfnSort)
		{
			pfnSort(psState, psCfg);
			//cache the function according to which the blocks are now sorted
			psCfg->pfnCurrentSortOrder = pfnSort;
		}
		if (pfClosure == NULL) return;
	}
	
	ASSERT (pfClosure);

#ifdef DEBUG
	psState->uBlockIterationsInProgress++;
#endif

	//2. Process blocks...
	for (uBlock = 0; uBlock < psCfg->uNumBlocks; uBlock++)
	{
		PCODEBLOCK psBlock = psCfg->apsAllBlocks[uBlock];		
		if (bHandlesCalls || !IsCall(psState, psBlock))
		{
			pfClosure(psState, psBlock, pvUserData);
		}
	}
#ifdef DEBUG
	ASSERT (psState->uBlockIterationsInProgress > 0);
	psState->uBlockIterationsInProgress--;
#endif
}

IMG_INTERNAL
IMG_VOID DoOnAllBasicBlocks(PINTERMEDIATE_STATE psState,
							BLOCK_SORT_FUNC pfnSort,
							BLOCK_PROC pfClosure,
							IMG_BOOL bHandlesCalls,
							IMG_PVOID pvUserData)
/******************************************************************************
 Function		: DoOnAllBasicBlocks

 Description	: Applies a function to all basic blocks in the entire program
				  (main proc, secondary update, and all functions)
	
 Parameters		: pfSortFunc	- order to process blocks within each function
				  pfClosure		- callback to apply to each block
				  bHandlesCalls - whether the callback should be passed blocks
								  representing procedure calls (see IsCall)
				  pvUserData	- additional data to pass to pfClosure.
******************************************************************************/
{
	PFUNC psFunc, psNextFunc;
	
#if defined(SUPPORT_ICODE_SERIALIZATION)
	/*
		Tracking recursion level to avoid intermediate code load during state use
	*/
	psState->uInsideDoOnAllBasicBlocks++;
#endif
	
	for (psFunc = psState->psFnInnermost; psFunc; psFunc = psNextFunc)
	{
		psNextFunc = psFunc ->psFnNestOuter;
		DoOnCfgBasicBlocks(	psState,
							&(psFunc->sCfg),
							pfnSort,
							pfClosure,
							bHandlesCalls,
							pvUserData);
	}
	
#if defined(SUPPORT_ICODE_SERIALIZATION)
	psState->uInsideDoOnAllBasicBlocks--;
#endif
}

static IMG_VOID DebugPrintSortRecursive(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
/******************************************************************************
 FUNCTION		: DebugPrintSortRecursive

 DESCRIPTION	: Internal function, part of DebugPrintSF (below)

 PARAMETERS		: psState	- Current compilation context
				  psBlock	- Node in CFG to recursively traverse

 RETURNS		: None
******************************************************************************/
{
	/*
		Essential idea is to allocate indices to nodes in "reverse postorder":
		the reverse (so high indices first) of the order in which a recursive
		depth-first traversal *finishes* processing each node. However, we
		introduce a couple extra complications to get nicer numbering of (some)
		loops and backedges - specifically, if we encounter a block which we are
		already in the middle of traversing, rather than returning immediately,
		we continue traversing through the block's successors from wherever the
		enclosing call has got to, but only assign an index when the outermost
		call on the block (i.e. the first one to encounter it) finishes. Hence,
		record this here.
	*/
	IMG_BOOL bFirstVisit;

	if ( ((IMG_INT32)psBlock->uIdx) > 0) return; //visited, finished.
	/*
		Is an enclosing call to SortBlocks (not) in progress on this node?
		(If so, node is a loop header, and we've reached it down a backedge)
	*/
	bFirstVisit = (psBlock->uIdx == 0) ? IMG_TRUE : IMG_FALSE;
	/*
		Iterate through children (successors). We store the index of the next
		child to process _negated_ in uIdx, to indicate that iteration is still
		progressing, and thus this node has not yet been assigned a proper idx.
		This allows any nested call to SortBlocks on this node (w/ bFirstVisit) to resume
		iteration where we left off; this ensures that successors on paths to the exit,
		should *finish* traversal (and be assigned high indices) before those on loop paths (which will be assigned lower indices, closer to
		the loop header).
	*/
	while (	((IMG_UINT32)-((IMG_INT32)psBlock->uIdx)) < psBlock->uNumSuccs)
	{
		PCODEBLOCK psSucc = psBlock->asSuccs[ -((IMG_INT32)psBlock->uIdx) ].psDest;
		psBlock->uIdx--;
		DebugPrintSortRecursive(psState, psSucc);
	}
	// all successors have now been visited, or are in progress.
	if (bFirstVisit)
	{
		//all successors finished (and allocated indices). (Quick check!)
#if defined(DEBUG)
		{
			IMG_UINT32 i;
			for (i=0; i<psBlock->uNumSuccs; i++)
			{
				ASSERT (psBlock->asSuccs[i].psDest==psBlock || psBlock->asSuccs[i].psDest->uIdx > 0);
			}
		}
#endif
		//thus, can now allocate index for this block
		psBlock->psOwner->uNumBlocks--; //obtain next index (in decreasing order)
		psBlock->uIdx = psBlock->psOwner->uNumBlocks;
		ASSERT ((psBlock==psBlock->psOwner->psEntry
					? psBlock->uIdx == 0
					: psBlock->uIdx > 0)
				||  /*unreachable exit->all bets off*/
					(psBlock->psOwner->psExit->uNumPreds == 0) );

		ASSERT(psBlock->psOwner->apsAllBlocks[psBlock->uIdx] == NULL);
		psBlock->psOwner->apsAllBlocks[psBlock->uIdx] = psBlock;
	}
	/*
		else, index will be assigned by outermost call on this node, i.e.
		when recursive calls to all successors have returned.
	*/
}

IMG_INTERNAL
IMG_VOID DebugPrintSF(PINTERMEDIATE_STATE psState, PCFG psCfg)
/******************************************************************************
 FUNCTION		: DebugPrintSF

 DESCRIPTION	: BLOCK_SORT_FUNC which sorts the blocks into a variation of
				  reverse postorder. Specifically, whereas reverse postorder is
				  just the reverse order to that in which depth-first-traversal
				  finishes on each node, we try to group loop headers together
				  with their loop bodies, "inside" any blocks not on paths back
				  to the header. However, this isn't 100% reliable on all cases
				  (i.e. on complex control flow - nested loops/ifs, etc.).

 PARAMETERS		: psState	- Compiler intermediate state
				  psCfg		- CFG whose blocks should be sorted

 RETURNS		: None, but sorts psFunc->apsAllBlocks & updates block ->uIdx's

 NOTE			: temporarily (ab)uses a number of structure members to store
				  traversal status, so usual invariants will not hold while
				  sorting is in progress.

 NOTE			: This is the ordering used to print out a program for debugging
				  purposes (so we aim for it to correspond to an intuitive idea
				  of "forwards"); ATM it is also used as the order for (a) laying
				  out the blocks in the HW code, and (b) register allocation by
				  linear-scan (depending on options). Hence the IMG_INTERNAL.
				  When we do these other things by other/better means, this can
				  become static in debug.c - TODO.
******************************************************************************/
{
	IMG_UINT32 uBlock;
	//clear indices - we use uIdx as traversal status, 0 = not yet seen
	for (uBlock = 0; uBlock < psCfg->uNumBlocks; uBlock++)
	{
		psCfg->apsAllBlocks[uBlock]->uIdx = 0;
		psCfg->apsAllBlocks[uBlock] = NULL;
	}
	
	/*
		Preset the index for the exit block (the code which lays out the final hardware program 
		relies on this block having the highest index.)
	*/
	if (psCfg->psEntry != psCfg->psExit)
	{
		ASSERT(psCfg->psExit->psOwner == psCfg);
		ASSERT(psCfg->uNumBlocks > 0);

		psCfg->uNumBlocks--;
		psCfg->psExit->uIdx = psCfg->uNumBlocks;
		psCfg->apsAllBlocks[psCfg->uNumBlocks] = psCfg->psExit;
	}

	DebugPrintSortRecursive(psState, psCfg->psEntry);

	ASSERT ((psCfg->uNumBlocks == 0 //used by SortBlocks as "last used uIdx"...
				&& psCfg->psEntry->uIdx == 0)
			|| psCfg->psExit->uNumPreds==0 /*no path to exit, all bets off*/);
	psCfg->uNumBlocks = uBlock; //...so restore proper value!
}

#ifdef SRC_DEBUG
IMG_INTERNAL 
IMG_VOID IncrementCostCounter(PINTERMEDIATE_STATE psState, 
							  IMG_UINT32 uSrcLine, IMG_UINT32 uIncrementValue)
/*****************************************************************************
 FUNCTION	: IncrementCostCounter

 PURPOSE	: Increments the specified entry in the source line cycle counter 
			  table.

 PARAMETERS	: psState	- Compiler state. (carries the table)
			  uSrcLine	- Number of entry in table to increment.
			  uIncrementValue	- Value to be icremented by.

 RETURNS	: None.
*****************************************************************************/
{
	psState->puSrcLineCost[uSrcLine] += uIncrementValue;
}

IMG_INTERNAL 
IMG_VOID DecrementCostCounter(PINTERMEDIATE_STATE psState, 
							  IMG_UINT32 uSrcLine, IMG_UINT32 uDecrementValue)
/*****************************************************************************
 FUNCTION	: DecrementCostCounter

 PURPOSE	: Decrements the specified entry in the source line cycle counter 
			  table.

 PARAMETERS	: psState	- Compiler state. (carries the table)
			  uSrcLine	- Number of entry in table to decrement.
			  uDecrementValue	- Value to be decremented by.

 RETURNS	: None.
*****************************************************************************/
{
	psState->puSrcLineCost[uSrcLine] -= uDecrementValue;
}
#endif /* SRC_DEBUG */

IMG_INTERNAL
IMG_VOID InlineFunction(PINTERMEDIATE_STATE psState, PINST psCallInst)
/******************************************************************************
 FUNCTION	: InlineFunction

 PURPOSE	: Replaces a call to a function by inlining the function body
			  (If other calls remain, the body is copied; if not, the body
			  is moved from the function structure which is then freed.)

 PARAMETERS	: psState		- Current compilation context
			  psCallInst	- Call instruction to replace (freed on completion)

 RETURNS	: None
******************************************************************************/
{
	PCODEBLOCK psCallBlock = psCallInst->u.psCall->psBlock;
	PFUNC psFunc = psCallInst->u.psCall->psTarget;
	PCFG psInto = psCallBlock->psOwner;
	PCODEBLOCK psFirst, psLast; //of inlined function body.
	PFUNC_INOUT_ARRAY psOutputs = &psFunc->sOut;
	IMG_UINT32 i;

	ASSERT (psFunc->sCfg.uNumBlocks); //non-empty
	ASSERT (psState->uFlags & USC_FLAGS_INTERMEDIATE_CODE_GENERATED);
	
	//for now, we'll resize the array twice - once to add the body, then again to remove the call block

	IncreaseArraySizeInPlace(psState,
				psCallBlock->psOwner->uNumBlocks * sizeof(PCODEBLOCK),
				(psCallBlock->psOwner->uNumBlocks + psFunc->sCfg.uNumBlocks) * sizeof(PCODEBLOCK),
				(IMG_PVOID*)&psCallBlock->psOwner->apsAllBlocks);

	/*
		All calls in the inlined function are now moved to the function containing the call site.
	*/
	psInto->psFunc->uCallCount += psFunc->uCallCount;

	//are there other calls to the function?
	if (psFunc->psCallSiteHead == psCallInst && psCallInst->u.psCall->psCallSiteNext == NULL)
	{
		//only call. move blocks from function, and drop function.
		for (i = 0; i < psFunc->sCfg.uNumBlocks; i++)
		{
			psFunc->sCfg.apsAllBlocks[i]->psOwner = psInto;
			psInto->apsAllBlocks[psInto->uNumBlocks] = psFunc->sCfg.apsAllBlocks[i];
			psInto->apsAllBlocks[psInto->uNumBlocks]->uIdx = psInto->uNumBlocks;
			psInto->uNumBlocks++; //increment
		}
		psFirst = psFunc->sCfg.psEntry; psLast = psFunc->sCfg.psExit;
		UscFree(psState, psFunc->sCfg.apsAllBlocks);
		psFunc->sCfg.apsAllBlocks = NULL;
		psFunc->sCfg.uNumBlocks = 0;
		psFunc->uCallCount = 0;
		//Function will be freed along with call instruction (below)
	}
	else
	{
		//copy the blocks forming the body of the function
		for (i = 0; i < psFunc->sCfg.uNumBlocks; i++)
		{
			PCODEBLOCK psNewBlock = UscAlloc(psState, sizeof(CODEBLOCK));
			PINST psInst, psPrevNew = NULL, *ppsNextPtr = &psNewBlock->psBody;
			*psNewBlock = *psFunc->sCfg.apsAllBlocks[i]; //copies everything
			psNewBlock->psOwner = psInto; //so overwrite
			//apsDomChildren, psLoopHeader, psI{,Post}Dom computed later
			for (psInst = psNewBlock->psBody; psInst; psInst = psInst->psNext)
			{
				//copy instruction & update previous psNext to point to copy
				*ppsNextPtr = AllocateInst(psState, psInst);
				*(*ppsNextPtr) = *psInst;
				//update psPrev to point to copy of previous inst, not original
				(*ppsNextPtr)->psPrev = psPrevNew;
				//move on
				psPrevNew = *ppsNextPtr;
				ppsNextPtr = & ((*ppsNextPtr)->psNext);
			}
			psInto->apsAllBlocks[psInto->uNumBlocks + i] = psNewBlock;
			psNewBlock->uIdx = psInto->uNumBlocks + i;
			//succs/preds not copied yet!
		}
		//copy predecessor and successor info
		for (i = 0; i < psFunc->sCfg.uNumBlocks; i++)
		{
			IMG_UINT32 uElem, /* which - preds or succs? */uDir;
			/* target block */
			PCODEBLOCK psBlock = psInto->apsAllBlocks[psInto->uNumBlocks + i];
			/* arrays of predecessors/successors... */
			PCODEBLOCK_EDGE /*...to copy from*/ asSrc, /*...to overwrite*/ *pasDest;
			for (uDir = 0; uDir < 2; uDir++)
			{
				/*
					Initially, the new blocks have *the same* pred/succ arrays
					as those in the function from which they were copied (i.e.
					containing pointers to blocks in the old function)
				*/
				if (uDir == 0)
				{
					uElem = psBlock->uNumSuccs;
					pasDest = &psBlock->asSuccs;
				}
				else
				{
					uElem = psBlock->uNumPreds;
					pasDest = &psBlock->asPreds;
				}
				//hence, grab the current array
				asSrc = *pasDest;
				//then make a new one specific to the block in the new function
				*pasDest = UscAlloc(psState, uElem * sizeof(CODEBLOCK_EDGE));
				while ( uElem-- ) //post-decrement
				{
					ASSERT(asSrc[uElem].psDest->psOwner == &(psFunc->sCfg));
					(*pasDest)[uElem].psDest = psInto->apsAllBlocks[psInto->uNumBlocks + asSrc[uElem].psDest->uIdx];
				}
			}
		}
		psFirst = psInto->apsAllBlocks[psInto->uNumBlocks + psFunc->sCfg.psEntry->uIdx];
		psLast = psInto->apsAllBlocks[psInto->uNumBlocks + psFunc->sCfg.psExit->uIdx];
		psInto->uNumBlocks += psFunc->sCfg.uNumBlocks;
	}

	/* if the function has outputs then need to insert moves from the output to the calls output */
	if (psOutputs->uCount > 0)
	{
		IMG_UINT32 uOutputIdx;

		for (uOutputIdx = 0; uOutputIdx < psOutputs->uCount; uOutputIdx++)
		{
			PFUNC_INOUT	psOutput = &psOutputs->asArray[uOutputIdx];
			PINST psMovInst;

			if (psCallInst->asDest[uOutputIdx].uNumber != psOutput->uNumber ||
				psCallInst->asDest[uOutputIdx].uType != psOutput->uType)
			{
				psMovInst = AllocateInst(psState, IMG_NULL);
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				if (psOutput->bVector)
				{
					SetOpcode(psState, psMovInst, IVMOV);
				}
				else
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
				{
					SetOpcode(psState, psMovInst, IMOV);
				}

				SetSrc(psState, psMovInst, 0, psOutput->uType, psOutput->uNumber, psOutput->eFmt);
				
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				if (psOutput->bVector)
				{
					psMovInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
					psMovInst->u.psVec->aeSrcFmt[0] = psOutput->eFmt;
				}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

				MoveDest(psState, psMovInst, 0, psCallInst, 0);

				psMovInst->auDestMask[0] = psOutput->uChanMask;

				AppendInst(psState, psLast, psMovInst);
			}
		}
	}

	ASSERT (psLast->eType == CBTYPE_EXIT);

	//excise the call block, and integrate the function body into its place
	RedirectEdgesFromPredecessors(psState, psCallBlock, psFirst, IMG_FALSE /* bSyncEnd */);
	
	psLast->eType = psCallBlock->eType;
	psLast->asSuccs = psCallBlock->asSuccs;
	psLast->uNumSuccs = psCallBlock->uNumSuccs;
	psLast->psIPostDom = psCallBlock->psIPostDom;
	
	{
		PCODEBLOCK psBlock;
		
		//nothing dominates the entry node (to the function)...
		ASSERT (psFirst->psIDom == NULL);

		for (psBlock = psLast; psBlock; psBlock = psBlock->psIDom)
		{
			/*
				Blocks dominating the function's exit, can't have any
				postdominators outside their dom. tree (as there are no nodes
				after the exit to be outside their dom. tree!).
			*/
			ASSERT (psBlock->psExtPostDom == NULL);
			psBlock->psExtPostDom = psCallBlock->psExtPostDom;
		}
		//...until it's inlined!
		psFirst->psIDom = psCallBlock->psIDom;
	}
	
	for (i = 0; i < psLast->uNumSuccs; i++)
	{
		PCODEBLOCK_EDGE	psSuccEdge = &psLast->asSuccs[i];
		PCODEBLOCK		psEdgeDest = psSuccEdge->psDest;

		ASSERT(psSuccEdge->uDestIdx < psEdgeDest->uNumPreds);
		ASSERT(psEdgeDest->asPreds[psSuccEdge->uDestIdx].psDest == psCallBlock);
		ASSERT(psEdgeDest->asPreds[psSuccEdge->uDestIdx].uDestIdx == i);
		psEdgeDest->asPreds[psSuccEdge->uDestIdx].psDest = psLast;
	}

	psCallBlock->asSuccs = NULL; //array has been transferred to psLast
	psCallBlock->uNumSuccs = 0;

	psCallBlock->eType = CBTYPE_UNDEFINED;
	FreeBlock(psState, psCallBlock);
}

USC_EXPORT
IMG_VOID IMG_CALLCONV PVRUniFlexDestroyContext(IMG_PVOID		pvContext)
/*****************************************************************************
 FUNCTION	: PVRUniFlexDestroyContext

 PURPOSE	: Called by the driver to destroy a context.

 PARAMETERS	: pvContext		- The context to destroy.

 RETURNS	: None.
*****************************************************************************/
{
	PINTERMEDIATE_STATE psState = (PINTERMEDIATE_STATE)pvContext;
#ifdef UF_TESTBENCH
	if (psState->puVerifLoopHdrs)
	{
		psState->pfnFree(psState->puVerifLoopHdrs);
	}
#endif

#if defined (FAST_CHUNK_ALLOC)
	MemManagerClose(psState);
#if defined (DEBUG)
	DBG_PRINTF((DBG_MESSAGE,"Total memory used = %u\n",psState->uMemoryUsedHWM));
#endif	
#endif


	psState->pfnFree(psState);
}

IMG_INTERNAL 
IMG_VOID InsertInstAfter(PINTERMEDIATE_STATE psState,
						  PCODEBLOCK psBlock, 
						  PINST psInstToInsert, 
						  PINST psInstToInsertAfter)
/*****************************************************************************
 FUNCTION	: InsertInstBefore

 PURPOSE	: Insert an instruction at a specified point in a basic block.

 PARAMETERS	: psState				- Internal compilation state
			  psCodeBlock			- Block to insert the instruction in.
			  psInstToInsert		- Instruction to insert.
			  psInstToInsertAfter	- Instruction that will be followed by the inserted one
									  or NULL to insert at the start of the block.

 RETURNS	: None.
*****************************************************************************/
{
	if (psInstToInsertAfter == NULL)
	{
		InsertInstBefore(psState, psBlock, psInstToInsert, psBlock->psBody);
	}
	else
	{
		InsertInstBefore(psState, psBlock, psInstToInsert, psInstToInsertAfter->psNext);
	}
}

IMG_INTERNAL
IMG_VOID ClearBlockInsts(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
/*****************************************************************************
 FUNCTION	: ClearBlockInsts

 PURPOSE	: Remove all the instructions from a block.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to remove instructions from.

 RETURNS	: Nothing.
*****************************************************************************/
{
	while (psBlock->psBody != NULL)
	{
		RemoveInst(psState, psBlock, psBlock->psBody);
	}
}

static
IMG_VOID InsertInstIntoBlock(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PINST psInstToInsert)
/*****************************************************************************
 FUNCTION	: InsertInstIntoBlock

 PURPOSE	: Update the instruction state when inserting an instruction into
			  a block.

 PARAMETERS	: psState				- Internal compilation state
			  psBlock				- Block to insert the instruction in.
			  psInstToInsert		- Instruction to insert.

 RETURNS	: None.
*****************************************************************************/
{
	PINST		psNextInst;

	/*
		Set the back pointer from the instruction to the block containing it.
	*/
	ASSERT(psInstToInsert->psBlock == NULL);
	psInstToInsert->psBlock = psBlock;

	/*
		Set the index of the instruction in the block.
	*/
	if (psInstToInsert->psPrev != NULL)
	{
		psInstToInsert->uBlockIndex = psInstToInsert->psPrev->uBlockIndex + 1;
	}
	else
	{
		psInstToInsert->uBlockIndex = 0;
	}

	/*
		Update the index of all following instructions.
	*/
	for (psNextInst = psInstToInsert->psNext; psNextInst != NULL; psNextInst = psNextInst->psNext)
	{
		psNextInst->uBlockIndex++;
	}

	/*
		Update the sorting of USE-DEF chains when inserting an instruction into a block.
	*/
	UseDefModifyInstructionBlock(psState, psInstToInsert, NULL /* psOldBlock */);
	
	/*
		Update the count of instructions in the block.
	*/
	psBlock->uInstCount += 1;

	if (psInstToInsert->eOpcode == IDELTA)
	{
		AppendToList(&psBlock->sDeltaInstList, &psInstToInsert->u.psDelta->sListEntry);
	}
	else
	if (psInstToInsert->eOpcode == ICALL)
	{
		psInstToInsert->u.psCall->psBlock = psBlock;
		psBlock->uCallCount++;
		psBlock->psOwner->psFunc->uCallCount++;
	}
	
	/* Update block flags to indicate which instruction groups it possesses */
	psBlock->uInstGroupCreated |= g_psInstDesc[psInstToInsert->eOpcode].uOptimizationGroup;
	psBlock->uFlags |= USC_CODEBLOCK_NEED_DEP_RECALC;
}

IMG_INTERNAL 
IMG_VOID InsertInstBefore(PINTERMEDIATE_STATE psState,
						  PCODEBLOCK psBlock, 
						  PINST psInstToInsert, 
						  PINST psInstToInsertBefore)
/*****************************************************************************
 FUNCTION	: InsertInstBefore

 PURPOSE	: Insert an instruction at a specified point in a basic block.

 PARAMETERS	: psState				- Internal compilation state
			  psCodeBlock			- Block to insert the instruction in.
			  psInstToInsert		- Instruction to insert.
			  psInstToInsertBefore	- Instruction that will follow the inserted one
									  or NULL to insert at the end of the block.

 RETURNS	: None.
*****************************************************************************/
{

#ifdef DEBUG
	CheckInst(psState, psInstToInsert);
#endif

	if (psInstToInsertBefore == NULL)
	{
		AppendInst(psState, psBlock, psInstToInsert);
		return;
	}
	if (IsNonMergable(psState, psBlock))
	{
		//can't insert into a call block or non mergable block; must prepend a block (!)
		PCODEBLOCK psPrelude = AllocateBlock(psState, psBlock->psOwner);
		ASSERT (psInstToInsertBefore == psBlock->psBody);
		RedirectEdgesFromPredecessors(psState, psBlock, psPrelude, IMG_FALSE /* bSyncEnd */);
		SetBlockUnconditional(psState, psPrelude, psBlock);
		AppendInst(psState, psPrelude, psInstToInsert);
		return;
	}
		
	psInstToInsert->psNext = psInstToInsertBefore;

	psInstToInsert->psPrev = psInstToInsertBefore->psPrev;
	if (psInstToInsertBefore->psPrev == NULL)
	{
		psBlock->psBody = psInstToInsert;
	}
	else
	{
		psInstToInsertBefore->psPrev->psNext = psInstToInsert;
	}
	psInstToInsertBefore->psPrev = psInstToInsert;

	InsertInstIntoBlock(psState, psBlock, psInstToInsert);
}

IMG_INTERNAL 
IMG_VOID AppendInst(PINTERMEDIATE_STATE psState, 
					PCODEBLOCK psBlock, 
					PINST psInstToInsert)
/*****************************************************************************
 FUNCTION	: AppendInst

 PURPOSE	: Insert an instruction at the end of a basic block.

 PARAMETERS	: psState			- Internal compilation state
			  psCodeBlock		- Block to which to append instruction.
			  psInstToInsert	- Instruction to insert.

 RETURNS	: None.
*****************************************************************************/
{
#ifdef DEBUG
	CheckInst(psState, psInstToInsert);
#endif

	if (IsNonMergable(psState, psBlock))
	{
		//can't insert into a call block - must insert a new block after it		
		if(IsCall(psState, psBlock))
		{
			PCODEBLOCK psAfter = AllocateBlock(psState, psBlock->psOwner);
			ASSERT (psBlock->eType == CBTYPE_UNCOND); //it's a CALL...maybe relax this sometime!?
			SetBlockUnconditional(psState, psAfter, psBlock->asSuccs[0].psDest);
			SetBlockUnconditional(psState, psBlock, psAfter);
			psBlock = psAfter;
		}
		else
		{
			/*
				Assumes that there will be only one instruction in non mergable block.
			*/
			PCODEBLOCK psNewBlock;
			PINST psNonMergableInst = psBlock->psBody;
			RemoveInst(psState, psBlock, psNonMergableInst);
			psNewBlock = AddUnconditionalPredecessor(psState, psBlock);
			AppendInst(psState, psNewBlock, psNonMergableInst);
		}		
		//and carry on
	}

	psInstToInsert->psNext = NULL;

	psInstToInsert->psPrev = psBlock->psBodyTail;

	*(psBlock->psBodyTail ? & psBlock->psBodyTail->psNext : &psBlock->psBody) = psInstToInsert;
	
	psBlock->psBodyTail = psInstToInsert;

	InsertInstIntoBlock(psState, psBlock, psInstToInsert);
}

IMG_INTERNAL
IMG_VOID SetAsGroupChild(PINTERMEDIATE_STATE psState, PINST psGroupParent, PINST psLastGroupChild, PINST psNewGroupChild)
/*****************************************************************************
 FUNCTION	: SetAsGroupChild

 PURPOSE	: Add an instruction to an MOE repeat.

 PARAMETERS	: psState			- Compiler state.
			  psGroupParent		- Instruction which represents the MOE repeat.
			  psLastGroupChild	- The last instruction which was added to the
								MOE repeat or the parent instruction if this
								the first instruction to be repeated.
			  psNewGroupChild	- The instruction to add to the MOE repeat.

 RETURNS	: None.
*****************************************************************************/
{
	PCODEBLOCK	psChildBlock = psNewGroupChild->psBlock;

	/*
		Remove the instruction from the flow control block.
	*/
	RemoveInst(psState, psChildBlock, psNewGroupChild);

	psLastGroupChild->psGroupNext = psNewGroupChild;
	psNewGroupChild->psGroupParent = psGroupParent;
	psNewGroupChild->uGroupChildIndex = psLastGroupChild->uGroupChildIndex + 1;

	UseDefModifyInstructionBlock(psState, psNewGroupChild, NULL /* psOldBlock */);
}

IMG_INTERNAL 
IMG_VOID RemoveInst(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PINST psInstToRemove)
/*****************************************************************************
 FUNCTION	: RemoveInst

 PURPOSE	: Remove an instruction from a basic block.

 PARAMETERS	: psBlock			- Block from which to remove the instruction.
			  psInstToRemove	- Instruction to remove.

 RETURNS	: None.
*****************************************************************************/
{
	PINST	psNextInst;
	psBlock->uFlags |= USC_CODEBLOCK_NEED_DEP_RECALC;

	ASSERT(psInstToRemove->psBlock == psBlock);
	psInstToRemove->psBlock = NULL;

	/*
		Update the sorting of USE-DEF chains when removing an instruction from a block.
	*/
	UseDefModifyInstructionBlock(psState, psInstToRemove, psBlock);

	if (psInstToRemove->eOpcode == IDELTA)
	{
		RemoveFromList(&psBlock->sDeltaInstList, &psInstToRemove->u.psDelta->sListEntry);
	}

	for (psNextInst = psInstToRemove->psNext; psNextInst != NULL; psNextInst = psNextInst->psNext)
	{
		psNextInst->uBlockIndex--;
	}
	psInstToRemove->uBlockIndex = USC_UNDEF;

	if (psBlock->psBody == psInstToRemove)
	{
		psBlock->psBody = psInstToRemove->psNext;
	}
	else
	{
		psInstToRemove->psPrev->psNext = psInstToRemove->psNext;
	}
	if (psBlock->psBodyTail == psInstToRemove)
	{
		psBlock->psBodyTail = psInstToRemove->psPrev;
	}
	else
	{
		psInstToRemove->psNext->psPrev = psInstToRemove->psPrev;
	}
	
	psInstToRemove->psPrev = psInstToRemove->psNext = NULL;

	/*hmmm. old code follows - not quite sure why it's like this...*/
//#ifdef DEBUG
	//if (!psCodeBlock->u.sBasic.uInstCount > 0)
		//abort();
//#endif /* def DEBUG */
	
	//if (psCodeBlock->u.sBasic.uInstCount > 0)
		//psCodeBlock->u.sBasic.uInstCount -= 1;
	/* Instead, we'll do... */

	ASSERT (psBlock->uInstCount > 0);
	psBlock->uInstCount--;
	if (psBlock->uInstCount == 0) psBlock->psOwner->bEmptiedBlocks = IMG_TRUE;

	if (psInstToRemove->eOpcode == ICALL)
	{
		ASSERT(psBlock->uCallCount > 0);
		psBlock->uCallCount--;

		ASSERT(psBlock->psOwner->psFunc->uCallCount > 0);
		psBlock->psOwner->psFunc->uCallCount--;

		psInstToRemove->u.psCall->psBlock = NULL;
	}
}

IMG_INTERNAL 
IMG_UINT32 MaskToSwiz(IMG_UINT32 uMask)
/*****************************************************************************
 FUNCTION	: MaskToSwiz

 PURPOSE	: Create a mask for comparing swizzles from a channel mask.

 PARAMETERS	: dwMask			- Channel mask

 RETURNS	: Mask.
*****************************************************************************/
{
	IMG_UINT32 uSCMask = 0;
	IMG_UINT32 i;
	for (i = 0; i < 4; i++)
	{
		if (uMask & (1U << i))
		{
			uSCMask |= (0x7U << (i * 3));
		}
	}
	return uSCMask;
}

IMG_INTERNAL 
TEST_TYPE ConvertCompOpToIntermediate(PINTERMEDIATE_STATE psState, UFREG_COMPOP eCompOp)
/*****************************************************************************
 FUNCTION	: ConvertCompOpToIntermediatet

 PURPOSE	: Convert a comparison operator to a test.

 PARAMETERS	: psState	- Internal compiler state
			  eCompOp	- Operator.

 RETURNS	: The intermediate test type.
*****************************************************************************/
{
	const TEST_TYPE g_aeCompOpToTest[] = 
	{
		TEST_TYPE_INVALID,	/* UFREG_COMPOP_INVALID */
		TEST_TYPE_GT_ZERO,	/* UFREG_COMPOP_GT */
		TEST_TYPE_EQ_ZERO,	/* UFREG_COMPOP_EQ */
		TEST_TYPE_GTE_ZERO,	/* UFREG_COMPOP_GE */
		TEST_TYPE_LT_ZERO,	/* UFREG_COMPOP_LT */
		TEST_TYPE_NEQ_ZERO,	/* UFREG_COMPOP_NE */
		TEST_TYPE_LTE_ZERO,	/* UFREG_COMPOP_LE */
		TEST_TYPE_INVALID,	/* UFREG_COMPOP_NEVER */
		TEST_TYPE_INVALID,	/* UFREG_COMPOP_ALWAYS */
	};

	ASSERT(eCompOp < (sizeof(g_aeCompOpToTest) / sizeof(g_aeCompOpToTest[0])));
	return g_aeCompOpToTest[eCompOp];
}

IMG_INTERNAL IMG_VOID CompOpToTest(PINTERMEDIATE_STATE psState,
								   UFREG_COMPOP uCompOp,
								   PTEST_DETAILS psTest)
/*****************************************************************************
 FUNCTION	: CompOpToTest

 PURPOSE	: Convert a comparison operator to a test.

 PARAMETERS	: psState	- Internal compiler state
			  uCompOp	- Operator.
			  psTest	- Initialized with the corresponding test.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psTest->eChanSel = USEASM_TEST_CHANSEL_C0;
	psTest->eMaskType = USEASM_TEST_MASK_NONE;
	psTest->eType = ConvertCompOpToIntermediate(psState, uCompOp);
}

IMG_INTERNAL 
IMG_VOID GetInputPredicate(PINTERMEDIATE_STATE psState, IMG_PUINT32 puPredSrc, IMG_PBOOL pbPredNegate, IMG_UINT32 uPredicate, IMG_UINT32 uChan)
/*****************************************************************************
 FUNCTION	: GetInputPredicate

 PURPOSE	: Converts an input predicate to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Code block to insert the instructions in.
			  uPredicate		- Input predicate.
			  uChan				- Channel required.

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uPredIdx;
	IMG_UINT32	uScalarPredSrc;

	/* Detect if no predication is needed */
	if ((uPredicate & UF_PRED_COMP_MASK) == UF_PRED_NONE)
	{
		*puPredSrc = USC_PREDREG_NONE;
		*pbPredNegate = IMG_FALSE;
		return;
	}

	/* Handle the invert modifier */
	if (uPredicate & UF_PRED_NEGFLAG)
	{
		*pbPredNegate = IMG_TRUE;
	}
	else
	{
		*pbPredNegate = IMG_FALSE;
	}
	uPredicate ^= UF_PRED_NEGFLAG;

	/* Extract the index of the predicate register to use */
	uPredIdx = (uPredicate & UF_PRED_IDX_MASK) >> UF_PRED_IDX_SHIFT;

	/* Form a scalar predicate index */
	ASSERT(uPredIdx < psState->uInputPredicateRegisterCount);
	uScalarPredSrc = USC_PREDREG_P0X + (uPredIdx * CHANNELS_PER_INPUT_REGISTER);

	switch (uPredicate & UF_PRED_COMP_MASK)
	{
		case UF_PRED_XYZW:
		{
			uScalarPredSrc += uChan;
			break;
		}

		case UF_PRED_X:
		case UF_PRED_Y:
		case UF_PRED_Z:
		case UF_PRED_W:
		{
			IMG_UINT32 uPredComp;

			uPredComp = ((uPredicate & UF_PRED_COMP_MASK) - UF_PRED_X) >> UF_PRED_COMP_SHIFT; 

			uScalarPredSrc += uPredComp;
			break;
		}

		default:
		{
			break;
		}
	}

	*puPredSrc = uScalarPredSrc;
}

IMG_INTERNAL PFUNC AllocFunction(PINTERMEDIATE_STATE psState, IMG_PCHAR pchEntryPointDesc)
/******************************************************************************
 FUNCTION		: AllocFunction
  
 DESCRIPTION	: Makes a new FUNC structure, and adds it as the outermost such
				  in the nesting-order list in psState (->psFnOutermost, etc.)

 PARAMETERS		: psState			- Current compilation context
				  pchEntryPointDesc	- Name by which function is externally
									  visible (NULL if internal to program).
******************************************************************************/
{
	PFUNC psFunc = UscAlloc(psState, sizeof(FUNC));

	psFunc->sCfg.psFunc = psFunc;
	psFunc->uNestingLevel = 0;
	InitRegLiveSet(&psFunc->sCallStartRegistersLive);
	psFunc->psCallSiteHead = NULL;
	psFunc->pchEntryPointDesc = pchEntryPointDesc;
	psFunc->uLabel = psState->uMaxLabel++;

	//add to list of all functions
	psFunc->psFnNestOuter = psState->psFnInnermost;
	psState->psFnInnermost = psFunc;
	*((psFunc->psFnNestOuter) ? &psFunc->psFnNestOuter->psFnNestInner : &psState->psFnOutermost) = psFunc;
	psFunc->psFnNestInner = NULL;

	InitCfg(psState, &(psFunc->sCfg), psFunc);
	/*psFunc->sCfg.psExit = AllocateBlock(psState, &(psFunc->sCfg));
	psFunc->sCfg.psExit->eType = CBTYPE_EXIT;
	psFunc->sCfg.psExit->uNumSuccs = 0;
	psFunc->sCfg.psExit->asSuccs = NULL;*/

	psFunc->uD3DLoopIndex = USC_UNDEF;

	psFunc->sIn.uCount = 0;
	psFunc->sIn.asArray = NULL;
	psFunc->sIn.asArrayUseDef = NULL;

	psFunc->sOut.uCount = 0;
	psFunc->sOut.asArray = NULL;
	psFunc->sOut.asArrayUseDef = NULL;

	psFunc->uCallCount = 0;

	psFunc->uUnlinkedJumpCount = 0;
	psFunc->asUnlinkedJumps = NULL;
	
	psFunc->sCfg.psEntry = psFunc->sCfg.psExit;
#if defined(TRACK_REDUNDANT_PCONVERSION)
	psFunc->uWorstPathCycleEstimate = 0;
	psFunc->uPathTotal = 0;
	psFunc->fAverageCycleCount = 0.0f;
#endif	
	return psFunc;
}

IMG_INTERNAL
IMG_BOOL FreeFunction(PINTERMEDIATE_STATE psState, PFUNC psFunc)
/******************************************************************************
 FUNCTION		: FreeFunction

 DESCRIPTION	: Frees a FUNC structure, including all blocks in its contained
				  flowgraph; includes removing it from psState's nesting list.

 PARAMETERS		: psState	- Compiler intermediate state
				  psFunc	- pointer to structure to free

 NOTE			: The function will not be freed if calls to it exist (i.e. in
				  the function's psCallSiteHead member)

 RETURNS		: TRUE if the function was freed; FALSE if not (if calls exist)
******************************************************************************/
{
	PFUNC *ppsListPtr;
	IMG_UINT32 i;
	
	if (psFunc->psCallSiteHead) return IMG_FALSE;
	
	//remove from nesting list
	ppsListPtr = (psFunc->psFnNestOuter ? &psFunc->psFnNestOuter->psFnNestInner : &psState->psFnOutermost);
	ASSERT (*ppsListPtr == psFunc);
	*ppsListPtr = psFunc->psFnNestInner;
	ppsListPtr = (psFunc->psFnNestInner ? &psFunc->psFnNestInner->psFnNestOuter : &psState->psFnInnermost);
	ASSERT (*ppsListPtr == psFunc);
	*ppsListPtr = psFunc->psFnNestOuter;
	
	if (psState->psSecAttrProg == psFunc) psState->psSecAttrProg = NULL;
	else if (psState->psMainProg == psFunc) psState->psMainProg = NULL; //that'll surely cause trouble...

	//Free structures
	psFunc->sCfg.psEntry = psFunc->sCfg.psExit = NULL;
	for (i=0; i < psFunc->sCfg.uNumBlocks; i++)
	{
		PCODEBLOCK psBlock = psFunc->sCfg.apsAllBlocks[i];

		FreeBlockState(psState, psBlock);
	}

	ASSERT(psFunc->uCallCount == 0);

	UscFree(psState, psFunc->sCfg.apsAllBlocks);
	ClearRegLiveSet(psState, &psFunc->sCallStartRegistersLive);
	for (i = 0; i < psFunc->sIn.uCount; i++)
	{
		UseDefDropFuncInput(psState, psFunc, i);
	}
	UscFree(psState, psFunc->sIn.asArray);
	UscFree(psState, psFunc->sIn.asArrayUseDef);
	for (i = 0; i < psFunc->sOut.uCount; i++)
	{
		UseDefDropFuncOutput(psState, psFunc, i);
	}
	UscFree(psState, psFunc->sOut.asArray);
	UscFree(psState, psFunc->sOut.asArrayUseDef);
	psFunc->sCfg.psFunc = NULL;
	UscFree(psState, psFunc);

	return IMG_TRUE;
}

IMG_INTERNAL
IMG_UINT32 GetRemapLocationCount(PINTERMEDIATE_STATE	psState,
								 PREGISTER_REMAP		psRemap,
								 IMG_UINT32				uInputNum,
								 IMG_UINT32				uCount)
/*****************************************************************************
 FUNCTION	: GetRemapLocationCount
    
 PURPOSE	: Get the translation for a temporary register from a remapping
			  table; creating a translation if it doesn't already exist.

 PARAMETERS	: psState		- Compiler state.
			  psRemap		- Remapping table.
			  uInputNum		- Temporary register number to remap.

 RETURNS	: The start of the replacement temporary register numbers.
*****************************************************************************/
{
	ASSERT(uInputNum < psRemap->uRemapCount);

	/*
		If mapping doesn't exist for this temporary register then create one.
	*/
	if (psRemap->auRemap[uInputNum] == USC_UNDEF)
	{
		psRemap->auRemap[uInputNum] = GetNextRegisterCount(psState, uCount);
	}

	return psRemap->auRemap[uInputNum];
}

IMG_INTERNAL
IMG_UINT32 GetRemapLocation(PINTERMEDIATE_STATE	psState,
						    PREGISTER_REMAP		psRemap,
						    IMG_UINT32			uInputNum)
/*****************************************************************************
 FUNCTION	: GetRemapLocation
    
 PURPOSE	: Get the translation for a temporary register from a remapping
			  table; creating a translation if it doesn't already exist.

 PARAMETERS	: psState		- Compiler state.
			  psRemap		- Remapping table.
			  uInputNum		- Temporary register number to remap.

 RETURNS	: The remapped temporary register number.
*****************************************************************************/
{
	return GetRemapLocationCount(psState, psRemap, uInputNum, 1 /* uCount */);
}

IMG_INTERNAL
IMG_VOID InitializeRemapTable(PINTERMEDIATE_STATE psState, PREGISTER_REMAP psRemap)
/*****************************************************************************
 FUNCTION	: InitializeRemapTable
    
 PURPOSE	: Create a remapping table.

 PARAMETERS	: psState		- Compiler state.
			  psRemap		- Remapping table.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psRemap->uRemapCount = psState->uNumRegisters;
	psRemap->auRemap = UscAlloc(psState, sizeof(psRemap->auRemap[0]) * psRemap->uRemapCount);
	memset(psRemap->auRemap, 0xFF, sizeof(psRemap->auRemap[0]) * psRemap->uRemapCount);
}

IMG_INTERNAL
IMG_VOID DeinitializeRemapTable(PINTERMEDIATE_STATE psState, PREGISTER_REMAP psRemap)
/*****************************************************************************
 FUNCTION	: DeinitializeRemapTable
    
 PURPOSE	: Free a remapping table.

 PARAMETERS	: psState		- Compiler state.
			  psRemap		- Remapping table.

 RETURNS	: Nothing.
*****************************************************************************/
{
	UscFree(psState, psRemap->auRemap);
	psRemap->auRemap = NULL;
}

static
IMG_VOID ExpandCallArguments(PINTERMEDIATE_STATE				psState,
							 PREGISTER_REMAP					psRemap,
							 IMG_BOOL							bDest,
							 PINST								psCallInst,
							 IMG_UINT32							uNewArgCount,
							 PFUNC_INOUT_ARRAY					psOldParams,
							 IMG_UINT32							uFirstC10PairChanMask)
/*****************************************************************************
 FUNCTION	: ExpandCallArguments
    
 PURPOSE	: Expand a CALL instruction's source or destination array
			  when replacing vector registers by scalar registers.

 PARAMETERS	: psState			- Compiler state.
			  psRemap			- Table remapping for vector register numbers
								to scalar register numbers.
			  bDest				- If TRUE expand destinations.
								  If FALSE expand sources.
			  psCallInst		- Instruction to expand.
			  uNewArgCount		- Number of arguments after expansion.
			  psOldParams		- Function inputs/outputs before expansion
								of vector registers.
			  uFirstC10PairChanMask
								- The mask of channels which include some bits
								from the first register of the pair of 32-bit
								registers holding a C10 format vec4.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_INT32	iNewArg, iArg;
	PARG		asArgs = bDest ? psCallInst->asDest : psCallInst->asArg;

	iNewArg = (IMG_INT32)uNewArgCount - 1;
	for (iArg = (IMG_INT32)psOldParams->uCount - 1; iArg >= 0; iArg--)
	{
		PFUNC_INOUT	psParam = &psOldParams->asArray[iArg];

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (psParam->bVector)
		{
			iNewArg = ExpandVectorCallArgument(psState,
											   psRemap,
											   psCallInst,
											   bDest,
											   asArgs,
											   psParam,
											   iArg,
											   iNewArg);
		}
		else 
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		if (psParam->eFmt == UF_REGFORMAT_C10)
		{
			if ((psParam->uChanMask & USC_ALPHA_CHAN_MASK) != 0)
			{
				PARG		psRGBArg = &asArgs[iArg];
				IMG_UINT32	uAlphaRegNum;

				if (psRGBArg->uType == USEASM_REGTYPE_IMMEDIATE)
				{
					uAlphaRegNum = psRGBArg->uNumber;
				}
				else
				{
					ASSERT(psRGBArg->uType == USEASM_REGTYPE_TEMP);

					uAlphaRegNum = GetRemapLocation(psState, psRemap, psRGBArg->uNumber);
				}

				/*
					Create a new argument for the ALPHA channels of the C10
					vector.
				*/
				SetArgument(psState, 
							psCallInst, 
							bDest,
							(IMG_UINT32)iNewArg, 
							psRGBArg->uType,
							uAlphaRegNum,
							psRGBArg->eFmt);
				if (bDest)
				{
					psCallInst->auLiveChansInDest[iNewArg] = psCallInst->auLiveChansInDest[iArg] & USC_ALPHA_CHAN_MASK;
				}

				ASSERT(iNewArg >= 0);
				iNewArg--;
			}

			if ((psParam->uChanMask & uFirstC10PairChanMask) != 0)
			{
				/*
					Copy the argument holding the RGB channels of the C10 vector.
				*/
				MoveArgument(psState, bDest, psCallInst, (IMG_UINT32)iNewArg, psCallInst, (IMG_UINT32)iArg);

				if (bDest)
				{
					psCallInst->auLiveChansInDest[iNewArg] = 
						psCallInst->auLiveChansInDest[iArg] & uFirstC10PairChanMask;
				}

				ASSERT(iNewArg >= 0);
				iNewArg--;
			}
		}
		else
		{
			/*
				Just copy an argument corresponding to a non-vector and non-C10
				arguments.
			*/
			MoveArgument(psState, bDest, psCallInst, (IMG_UINT32)iNewArg, psCallInst, (IMG_UINT32)iArg);
			if (bDest)
			{
				psCallInst->auLiveChansInDest[iNewArg] = psCallInst->auLiveChansInDest[iArg];
			}

			ASSERT(iNewArg >= 0);
			iNewArg--;
		}
	}
	ASSERT(iNewArg == -1);
}

static
IMG_VOID ExpandCallInstruction(PINTERMEDIATE_STATE				psState, 
							   PREGISTER_REMAP					psRemap,
							   PINST							psCallInst,
							   PFUNC_INOUT_ARRAY				psOldIn,
							   PFUNC_INOUT_ARRAY				psOldOut,
							   IMG_UINT32						uFirstC10PairChanMask)
/*****************************************************************************
 FUNCTION	: ExpandCallInstruction
    
 PURPOSE	: Expand a CALL instruction when replacing vector registers by scalar 
			  registers.

 PARAMETERS	: psState			- Compiler state.
			  psRemap			- Table remapping for vector register numbers
								to scalar register numbers.
			  psCallInst		- Instruction to expand.
			  psOldIn			- Function inputs before expansion of
								scalar registers.
			  psOldOut			- Function outputs before expansion of
								scalar registers.
			  uFirstC10PairChanMask
								- The mask of channels which include some bits
								from the first register of the pair of 32-bit
								registers holding a C10 format vec4.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PFUNC	psTarget = psCallInst->u.psCall->psTarget;

	/*
		Grow the array of instruction destinations.
	*/
	ASSERT(psCallInst->uDestCount == psOldOut->uCount);
	SetDestCount(psState, psCallInst, psTarget->sOut.uCount);

	/*
		Replace vector registers by scalar registers in the CALL instruction's
		destination array.
	*/
	ExpandCallArguments(psState,
						psRemap,
						IMG_TRUE /* bDest */,
						psCallInst,
						psTarget->sOut.uCount,
						psOldOut,
						uFirstC10PairChanMask);

	/*
		Grow the array of instruction sources.
	*/
	ASSERT(psCallInst->uArgumentCount == psOldIn->uCount);
	SetArgumentCount(psState, psCallInst, psTarget->sIn.uCount);

	/*
		Replace vector registers by scalar registers in the CALL instruction's
		source array.
	*/
	ExpandCallArguments(psState,
						psRemap,
						IMG_FALSE /* bDest */,
						psCallInst,
						psTarget->sIn.uCount,
						psOldIn,
						uFirstC10PairChanMask);
}


static
IMG_UINT32 ExpandFunctionParameterArray(PINTERMEDIATE_STATE				psState,
										PREGISTER_REMAP					psRemap,
										PFUNC_INOUT						asNewParams,
										PFUNC_INOUT_ARRAY				psOldParams,
										IMG_UINT32						uFirstC10PairChanMask)
/*****************************************************************************
 FUNCTION	: ExpandFunctionParameterArray
    
 PURPOSE	: Expand a function's array of inputs or outputs when replacing 
			  vector registers by scalar registers.

 PARAMETERS	: psState			- Compiler state.
			  psRemap			- Table remapping for vector register numbers
								to scalar register numbers.
			  asNewParams		- If non-NULL: entries are filled out with
								details of the expanded inputs or outputs.
			  psOldParams		- Input or outputs before expansion.
			  uFirstC10PairChanMask
								- The mask of channels which include some bits
								from the first register of the pair of 32-bit
								registers holding a C10 format vec4.

 RETURNS	: The new count of inputs or outputs.
*****************************************************************************/
{
	IMG_UINT32	uArg;
	IMG_UINT32	uNewArgCount;

	uNewArgCount = 0;
	for (uArg = 0; uArg < psOldParams->uCount; uArg++)
	{
		PFUNC_INOUT	psOldParam = &psOldParams->asArray[uArg];

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (psOldParam->bVector)
		{
			uNewArgCount = ExpandFunctionVectorParameter(psState, psRemap, psOldParam, asNewParams, uNewArgCount);
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		if (psOldParam->eFmt == UF_REGFORMAT_C10)
		{
			if ((psOldParam->uChanMask & USC_ALPHA_CHAN_MASK) != 0)
			{
				if (asNewParams != NULL)
				{
					asNewParams[uNewArgCount].uType = psOldParam->uType;
					asNewParams[uNewArgCount].uNumber = GetRemapLocation(psState, psRemap, psOldParam->uNumber);
					asNewParams[uNewArgCount].eFmt = psOldParam->eFmt;
					asNewParams[uNewArgCount].uChanMask = USC_RED_CHAN_MASK;
					#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
					asNewParams[uNewArgCount].bVector = IMG_FALSE;
					#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
				}
				uNewArgCount++;
			}

			if ((psOldParam->uChanMask & uFirstC10PairChanMask) != 0)
			{
				if (asNewParams != NULL)
				{
					asNewParams[uNewArgCount] = *psOldParam;
				}
				uNewArgCount++;
			}
		}
		else
		{
			if (asNewParams != NULL)
			{
				asNewParams[uNewArgCount] = *psOldParam;
			}
			uNewArgCount++;
		}
	}
	return uNewArgCount;
}

static
IMG_VOID ExpandFunctionParameters(PINTERMEDIATE_STATE				psState,
								  PREGISTER_REMAP					psRemap,
								  PFUNC								psFunc,
								  IMG_BOOL							bInput,
								  PFUNC_INOUT_ARRAY					psOldParams,
								  PFUNC_INOUT_ARRAY					psNewParams,
								  IMG_UINT32						uFirstC10PairChanMask)
/*****************************************************************************
 FUNCTION	: ExpandFunctionParameters
    
 PURPOSE	: Expand a function's array of inputs or outputs when replacing 
			  vector registers by scalar registers.

 PARAMETERS	: psState			- Compiler state.
			  psRemap			- Table remapping for vector register numbers
								to scalar register numbers.
			  psFunc			- Function to modify.
			  bInput			- TRUE to modify inputs.
								  FALSE to modify outputs.
			  psOldParams		- Input or outputs before expansion.
			  psNewParams		- On return: details of the expanded inputs
								or outputs.
			  uFirstC10PairChanMask
								- The mask of channels which include some bits
								from the first register of the pair of 32-bit
								registers holding a C10 format vec4.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uParamIdx;

	/*
		Count the new number of input or outputs.
	*/
	psNewParams->uCount = 
		ExpandFunctionParameterArray(psState, psRemap, NULL /* asNewParams */, psOldParams, uFirstC10PairChanMask);

	/*
		Drop USE-DEF information for the old function inputs/outputs.
	*/
	for (uParamIdx = 0; uParamIdx < psOldParams->uCount; uParamIdx++)
	{
		if (bInput)
		{
			UseDefDropFuncInput(psState, psFunc, uParamIdx);
		}
		else
		{
			UseDefDropFuncOutput(psState, psFunc, uParamIdx);
		}
	}

	/*
		Create the new array of inputs or outputs.
	*/
	psNewParams->asArray = UscAlloc(psState, sizeof(psNewParams->asArray[0]) * psNewParams->uCount);
	ExpandFunctionParameterArray(psState, psRemap, psNewParams->asArray, psOldParams, uFirstC10PairChanMask);

	/*
		Create USE-DEF information for the new function inputs/outputs.
	*/
	psNewParams->asArrayUseDef = UscAlloc(psState, sizeof(psNewParams->asArrayUseDef[0]) * psNewParams->uCount);
	for (uParamIdx = 0; uParamIdx < psNewParams->uCount; uParamIdx++)
	{
		if (bInput)
		{
			UseDefReset(&psNewParams->asArrayUseDef[uParamIdx], DEF_TYPE_FUNCINPUT, uParamIdx, psFunc);
			UseDefAddFuncInput(psState, psFunc, uParamIdx);
		}
		else
		{
			UseDefReset(&psNewParams->asArrayUseDef[uParamIdx], USE_TYPE_FUNCOUTPUT, uParamIdx, psFunc);
			UseDefAddFuncOutput(psState, psFunc, uParamIdx);
		}
	}
}

static
IMG_VOID FreeFunctionInOut(PINTERMEDIATE_STATE psState, PFUNC_INOUT_ARRAY psInOut)
/*****************************************************************************
 FUNCTION	: FreeFunctionInOut
    
 PURPOSE	: Free information about funciton input or outputs.

 PARAMETERS	: psState			- Compiler state.
			  psInOut			- Array of inputs/output to free.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psInOut->asArray != NULL)
	{
		UscFree(psState, psInOut->asArray);
		psInOut->asArray = NULL;

		UscFree(psState, psInOut->asArrayUseDef);
		psInOut->asArrayUseDef = NULL;
	}
}
static
IMG_VOID ExpandFunction(PINTERMEDIATE_STATE				psState,
						PREGISTER_REMAP					psRemap,
						PFUNC							psFunc, 
						IMG_UINT32						uFirstC10PairChanMask)
/*****************************************************************************
 FUNCTION	: ExpandFunction
    
 PURPOSE	: Expand a function's array of inputs and outputs when replacing 
			  vector registers by scalar registers.

 PARAMETERS	: psState			- Compiler state.
			  psRemap			- Remapping table from vector register numbers
								to scalar register numbers.
			  psFunc			- Function to expand.
			  uFirstC10PairChanMask
								- The mask of channels which include some bits
								from the first register of the pair of 32-bit
								registers holding a C10 format vec4.

 RETURNS	: Nothing.
*****************************************************************************/
{
	FUNC_INOUT_ARRAY	sOldIn;
	FUNC_INOUT_ARRAY	sOldOut;
	PINST				psCallInst;

	/*
		Save the pre-expanded input and outputs for use when expanding calls to this
		function.
	*/
	sOldIn = psFunc->sIn;
	sOldOut = psFunc->sOut;

	/*
		Expand the function inputs.
	*/
	ExpandFunctionParameters(psState, psRemap, psFunc, IMG_TRUE /* bInput */, &sOldIn, &psFunc->sIn, uFirstC10PairChanMask);

	/*
		Expand the function outputs.
	*/
	ExpandFunctionParameters(psState, psRemap, psFunc, IMG_FALSE /* bInput */, &sOldOut, &psFunc->sOut, uFirstC10PairChanMask);

	/*
		Expand the sources/destinations of calls to this function.
	*/
	for (psCallInst = psFunc->psCallSiteHead; psCallInst != NULL; psCallInst = psCallInst->u.psCall->psCallSiteNext)
	{
		ExpandCallInstruction(psState, psRemap, psCallInst, &sOldIn, &sOldOut, uFirstC10PairChanMask);
	}

	/*
		Free information about the pre-expanded function inputs/outputs.
	*/
	FreeFunctionInOut(psState, &sOldIn);
	FreeFunctionInOut(psState, &sOldOut);
}

IMG_INTERNAL
IMG_VOID ExpandFunctions(PINTERMEDIATE_STATE psState, PREGISTER_REMAP psRemap, IMG_UINT32 uFirstC10PairChanMask)
/*****************************************************************************
 FUNCTION	: ExpandFunctions
    
 PURPOSE	: Expand all functions arrays of inputs and outputs when replacing 
			  vector registers by scalar registers.

 PARAMETERS	: psState			- Compiler state.
			  psRemap			- Remapping table from vector register numbers
								to scalar register numbers.
			uFirstC10PairChanMask
								- The mask of channels which include some bits
								from the first register of the pair of 32-bit
								registers holding a C10 format vec4.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PFUNC	psFunc;

	for (psFunc = psState->psFnInnermost; psFunc != NULL; psFunc = psFunc->psFnNestOuter)
	{
		ExpandFunction(psState, psRemap, psFunc, uFirstC10PairChanMask);
	}
}

static IMG_VOID IntegerOptimizeBP(PINTERMEDIATE_STATE		psState,
									 PCODEBLOCK					psBlock,
									 IMG_PVOID				pvNull)
/*********************************************************************************
 FUNCTION			: IntegerOptimizeBP

 DESCRIPTION		: Optimize integer instruction in a basic block.

 PARAMETERS			: psState	- The intermediate state.
					  psBlock	- The block to optimize.
					  pvNull	- IMG_PVOID to fit callback signature; unused

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_BOOL	bChanges;
	PVR_UNREFERENCED_PARAMETER(pvNull);
	do
	{
		IMG_BOOL	bEliminateMoves;

		bChanges = IntegerOptimize(psState, psBlock, &bEliminateMoves);
		TESTONLY_PRINT_INTERMEDIATE("Integer optimizations", "int_opt", IMG_FALSE);

		if (bEliminateMoves)
		{
			EliminateMovesBP(psState, psBlock);
			TESTONLY_PRINT_INTERMEDIATE("Eliminate moves", "elim_mov", IMG_FALSE);
		}
	} while (bChanges);
}

static IMG_VOID AddFixForBRN21752BP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNull)
/*****************************************************************************
 FUNCTION	: AddFixForBRN21752

 PURPOSE	: BLOCK_PROC, modifies a single basic block to fix BRN21752

 PARAMETERS	: psState	- Compiler state.
			  psBlock	- Block to search for instrs suffering from BRN21752
			  pvNull	- IMG_PVOID to fit callback signature; unused

 RETURNS	: None
*****************************************************************************/
{
	/*
		Look for instructions that suffer from BRN21752 in all basic-blocks
	*/
	PINST psInst;
	PVR_UNREFERENCED_PARAMETER(pvNull);

	for (psInst = psBlock->psBody; psInst; psInst = psInst->psNext)
	{
		/*
			BRN21752 applies to pack/unpack instructions where:
				(destination = unified store) AND
				(destination format = C10 XOR source format = C10) AND
				(destination mask /= .X111
		*/
		if (
				IsInstAffectedByBRN21752(psInst) &&
				((psInst->auDestMask[0] & 0x7) != 0x7)
		   )
		{
			IMG_UINT32		uTempRegNum;
			UF_REGFORMAT	eSrcRegFmt;
			PINST			psPackInst;

			/*
				If all live channels the destination are written by the
				instruction, we might be able to change the instruction so
				that it can write all components.
			*/
			if	((psInst->auLiveChansInDest[0] & ~psInst->auDestMask[0]) == 0)
			{
				switch (psInst->eOpcode)
				{
					case IUNPCKF16C10:
					{
						ASSERT(psInst->auDestMask[0] == 3 || psInst->auDestMask[0] == 12 || psInst->auDestMask[0] == 15);

						/* 
							As only one F16 channel is being written in the
							destination, changing the mask to 0xF (both F16 
							channels written) will cause the data from the 
							first source to be packed into the first channel
							of the dest (lower 16-bits), and the second 
							source to be packed to the upper 16-bits. 

							Thus, we need to swap the two sources if the 
							first source is currently expected to go to the
							upper 16-bits of the dest.
						*/
						if (psInst->auDestMask[0] & 12)
						{
							SwapPCKSources(psState, psInst);
						}
						psInst->auDestMask[0] = USC_DESTMASK_FULL;

						continue;
					}

					case IPCKC10F32:
					case IPCKC10F16:
					{
						/*
							Forcing the write-mask to 0xF will cause the
							data from the first source to be packed into
							channels 0 and 2 of the destination register,
							and the second source to be packed into
							channels 1 and 3.

							Thus, we need to swap the two sources if the
							first is currently expected to go to channels
							1 or 3.

							We cannot change the mask if the instruction
							currently packs the two sources to channels
							0 and 2, or 1 and 3 (mask is 0x5 or 0xA).
						*/
						if (psInst->auDestMask[0] == 1 ||  
							psInst->auDestMask[0] == 3 ||
							psInst->auDestMask[0] == 4 || 
							psInst->auDestMask[0] == 9 ||
							psInst->auDestMask[0] == 12 ||
							EqualPCKArgs(psState, psInst))
						{
							psInst->auDestMask[0] = USC_DESTMASK_FULL;
							continue;
						}
						else if (psInst->auDestMask[0] == 6 ||
								 psInst->auDestMask[0] == 8 || 
								 psInst->auDestMask[0] == 2)
						{
							SwapPCKSources(psState, psInst);

							psInst->auDestMask[0] = USC_DESTMASK_FULL;
							continue;
						}
						break;
					}

					default:
					{
						imgabort();
					}
				}
			}

			uTempRegNum	= psState->uNumRegisters++;

			/*
				Insert an additional pack instruction to perform 
				the masked write of the original destination, without
				any format conversion.

				NB: The original pack will be modified to have a full
					(0xF) dest-mask. So the two source components that
					get packed by it will always end up in the first
					2 channels of the temporary destination (in order).
			*/
			psPackInst = AllocateInst(psState, IMG_NULL);//ACL psInst??

			if	(psInst->eOpcode == IUNPCKF16C10)
			{
				SetOpcode(psState, psPackInst, IPCKF16F16);
				eSrcRegFmt = UF_REGFORMAT_F16;
			}
			else
			{
				SetOpcode(psState, psPackInst, IPCKC10C10);
				eSrcRegFmt = UF_REGFORMAT_C10;
			}

			CopyPredicate(psState, psPackInst, psInst);

			psPackInst->u.psPck->bScale = IMG_FALSE;

			psPackInst->asDest[0]			= psInst->asDest[0];
			psPackInst->auDestMask[0]			= psInst->auDestMask[0];
			psPackInst->auLiveChansInDest[0]	= psInst->auLiveChansInDest[0];

			psPackInst->asArg[0].uType		= USEASM_REGTYPE_TEMP;
			psPackInst->asArg[0].uNumber	= uTempRegNum;
			SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, 0 /* uComponent */);
			psPackInst->asArg[0].eFmt		= eSrcRegFmt;

			psPackInst->asArg[1].uType		= USEASM_REGTYPE_TEMP;
			psPackInst->asArg[1].uNumber	= uTempRegNum;
			SetPCKComponent(psState, psPackInst, 1U /* uArgIdx */, (eSrcRegFmt == UF_REGFORMAT_F16) ? 2U : 1U);
			psPackInst->asArg[1].eFmt		= eSrcRegFmt;

			InsertInstBefore(psState, psBlock, psPackInst, psInst->psNext);

			/*
				Change the original pack instruction to perform the
				format conversion into the spare temporary register, 
				switching to a full-mask instead.
			*/
			psInst->asDest[0].uType		= USEASM_REGTYPE_TEMP;
			psInst->asDest[0].uNumber	= uTempRegNum;
			psInst->auDestMask[0]			= 0xF;
			psInst->auLiveChansInDest[0]	= (eSrcRegFmt == UF_REGFORMAT_F16) ? 0xFU : 0x3U;
		}
	}
}

/*********************************************************************************
 Function			: InitialiseIndexableTemps

 Description		: Initialise data structures for indexable temporaries.
 
 Parameters			: psState			- The current compilation context

 Return				: Nothing.
*********************************************************************************/
IMG_INTERNAL
IMG_VOID InitialiseIndexableTemps(PINTERMEDIATE_STATE psState)
{
	const IMG_UINT32 uNumTempArrays = psState->uIndexableTempArrayCount;

	psState->apsTempVecArray = UscAlloc(psState, 
										(uNumTempArrays * sizeof(PUSC_VEC_ARRAY_DATA)));
	memset(psState->apsTempVecArray, 0, uNumTempArrays * sizeof(PUSC_VEC_ARRAY_DATA));

	psState->uIndexableTempArraySize = 0;
}

static IMG_VOID ReleaseVaildShaderOutputsData(PINTERMEDIATE_STATE psState)
/*********************************************************************************
 Function			: ReleaseVaildShaderOutputsData

 Description		: Releases memory used by Valid shader outputs specification
					  data.

 Parameters			: psState			- The current compilation context

 Return				: Nothing.
*********************************************************************************/
{
	if((psState->psSAOffsets->eShaderType == USC_SHADERTYPE_VERTEX) || (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_GEOMETRY))
	{
		if(psState->sValidShaderOutPutRanges.uRangesCount > 0)
		{
			UscFree(psState, psState->sValidShaderOutPutRanges.psRanges);
			UscFree(psState, psState->psPackedValidOutPutRanges);
		}
	}
}

static
IMG_VOID InitVertexShaderState(PINTERMEDIATE_STATE	psState)
/*********************************************************************************
 Function			: InitVertexShaderState

 Description		: Initialize parts of the compiler state specific to
					  vertex/geometry shaders.

 Parameters			: psState			- The current compilation context

 Return				: Nothing.
*********************************************************************************/
{
	PVERTEXSHADER_STATE	psVS;

	psVS = UscAlloc(psState, sizeof(*psVS));
	psState->sShader.psVS = psVS;

	psVS->uVSInputPARegCount = 0;
	psVS->auVSInputPARegUsage = NULL;

	psVS->uVerticesBaseInternArrayIdx = USC_UNDEF;

	psVS->uVertexShaderNumOutputs = 0;
}

static
IMG_VOID InitPixelShaderState(PINTERMEDIATE_STATE	psState)
/*********************************************************************************
 Function			: InitPixelShaderState

 Description		: Initialize parts of the compiler state specific to
					  pixel shaders.

 Parameters			: psState			- The current compilation context

 Return				: Nothing.
*********************************************************************************/
{
	PPIXELSHADER_STATE	psPS;
	IMG_UINT32			uSurf;

	psPS = UscAlloc(psState, sizeof(*psPS));
	psState->sShader.psPS = psPS;

	/*
		Reset information about pixel shader inputs.
	*/
	psPS->uNrPixelShaderInputs = 0;
	InitializeList(&psPS->sPixelShaderInputs);
	#if defined(OUTPUT_USPBIN)
	memset(psPS->apsF32TextureCoordinateInputs, 0, sizeof(psPS->apsF32TextureCoordinateInputs));
	#endif /* defined(OUTPUT_USPBIN) */

	psPS->uInputOrderPackedPSOutputMask = 0;
	psPS->uHwOrderPackedPSOutputMask = 0;
	psPS->psFixedHwPARegReg = NULL;
	psPS->uAlphaOutputOffset = 0;
	psPS->psTexkillOutput = NULL;
	psPS->psDepthOutput = NULL;
	psPS->psOMaskOutput = NULL;

	psPS->uColOutputCount = 0;

	/* Initiallly no emits in the program. */
	psPS->uEmitsPresent = 0;
	psPS->uPostSplitRate = 1;

	psPS->eZFormat = UF_REGFORMAT_UNTYPED;
	psPS->eOMaskFormat = UF_REGFORMAT_UNTYPED;
	for (uSurf = 0; uSurf < UNIFLEX_MAX_OUT_SURFACES; uSurf++)
	{
		psPS->aeColourResultFormat[uSurf] = UF_REGFORMAT_UNTYPED;
	}
}

IMG_INTERNAL
IMG_UINT32 SearchRangeList(PUNIFLEX_RANGES_LIST	psRangesList, IMG_UINT32 uNum, PUNIFLEX_RANGE* ppsFoundRange)
/*********************************************************************************
 Function		: SearchRangeList

  Description	: Search a ranges list for a range containing a specified point.

 Parameters		: psRangesList	- Search of ranges to search.
				  uNum			- Point to search for.
				  ppsFoundRange	- Returns the range containing the point if it
								exists.

 Return			: The index of the range containing the point or USC_UNDEF.
*********************************************************************************/
{
	IMG_UINT32	uArrayIdx;

	*ppsFoundRange = NULL;
	for (uArrayIdx = 0; uArrayIdx < psRangesList->uRangesCount; uArrayIdx++)
	{
		PUNIFLEX_RANGE	psRange = &psRangesList->psRanges[uArrayIdx];

		if (uNum >= psRange->uRangeStart && uNum < psRange->uRangeEnd)
		{
			*ppsFoundRange = psRange;
			return uArrayIdx;
		}
	}
	return USC_UNDEF;
}

static
IMG_BOOL IsInvariantShader(PINTERMEDIATE_STATE psState)
/*********************************************************************************
 Function		: IsInvariantShader

  Description	: Checks for a shader which declares some outputs as invariant.

 Parameters		: psState			- The compilation context.

 Return			: TRUE if some outputs are invariant.
*********************************************************************************/
{
	IMG_UINT32	uDwordIdx;

	if ((psState->uCompilerFlags & UF_GLSL) == 0)
	{
		return IMG_FALSE;
	}

	if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL)
	{
		if (GetBit(psState->psSAOffsets->puInvariantShaderOutputs, CHANNELS_PER_INPUT_REGISTER * UFREG_OUTPUT_Z))
		{
			return IMG_TRUE;
		}
		if (GetBit(psState->psSAOffsets->puInvariantShaderOutputs, CHANNELS_PER_INPUT_REGISTER * UFREG_OUTPUT_OMASK))
		{
			return IMG_TRUE;
		}
	}
	for (uDwordIdx = 0; uDwordIdx < USC_SHADER_OUTPUT_MASK_DWORD_COUNT; uDwordIdx++)
	{
		IMG_UINT32	uInvariantMask;

		uInvariantMask = psState->psSAOffsets->puValidShaderOutputs[uDwordIdx];
		uInvariantMask &= psState->psSAOffsets->puInvariantShaderOutputs[uDwordIdx];
		if (uDwordIdx == (USC_SHADER_OUTPUT_MASK_DWORD_COUNT - 1))
		{
			IMG_UINT32	uBitsInLastDword;

			uBitsInLastDword = USC_MAX_SHADER_OUTPUTS % BITS_PER_UINT;
			uInvariantMask &= ((1U << uBitsInLastDword) - 1);
		}
		if (uInvariantMask != 0)
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

/*********************************************************************************
 Function		: InitState

  Description	: Initialises the compiler intermediate state with various
				  parameters etc. Call before creating uniflex input program.

 Parameters		: psState			- The compilation context to initialise
				  uFlags			- Compiler flags.
				  uFlags2			- Compiler flags 2.
				  psConstants		- Local constants used in the program.
				  psProgramParameters
									- Driver compilation options.

 Return			: Compilation status.
*********************************************************************************/
static
IMG_VOID InitState(PINTERMEDIATE_STATE			psState,
				   IMG_UINT32					uFlags,
				   IMG_UINT32					uFlags2,
				   PUNIFLEX_CONSTDEF			psConstants,
				   PUNIFLEX_PROGRAM_PARAMETERS	psProgramParameters)
{
	#if defined(OUTPUT_USCHW)
	IMG_UINT32	uSamplerIdx;
	#endif /* defined(OUTPUT_USCHW) */
	IMG_UINT32	uI;
	IMG_UINT32	uOpIdx;

	psState->uFlags	= 0;
	psState->uFlags2 = 0;
	psState->uCompilerFlags = uFlags;
	psState->uCompilerFlags2 = uFlags2;
	psState->psConstants = psConstants;	
	psState->uNumRegisters = 0;
	psState->uOptimizationHint = 0;

	/*
		Reset the count of created instructions.
	*/
	psState->uGlobalInstructionCount = 0;

	/*
		Reset the count of created blocks.
	*/
	psState->uGlobalBlockCount = 0;

	#if defined(INITIALISE_REGS_ON_FIRST_WRITE)
	#if defined(INITIALISE_REGS_ON_FIRST_WRITE_CMDLINE_OPTION)
	if (uFlags & UF_INITREGSONFIRSTWRITE)
	{
		psState->uFlags |= USC_FLAGS_INITIALISEREGSONFIRSTWRITE;
	}
	#else
	psState->uFlags |= USC_FLAGS_INITIALISEREGSONFIRSTWRITE;
	#endif /* defined(INITIALISE_REGS_ON_FIRST_WRITE_CMDLINE_OPTION) */
	#endif /* defined(INITIALISE_REGS_ON_FIRST_WRITE) */

	psState->uNumPredicates	= 0;

	/*
		Reset information about each constant buffer.
	*/
	for(uI=0; uI<UF_CONSTBUFFERID_COUNT; uI++)	
	{
		PCONSTANT_BUFFER			psConstBuf = &psState->asConstantBuffer[uI];
		PUNIFLEX_CONSTBUFFERDESC	psDesc = &psProgramParameters->asConstBuffDesc[uI];

		psConstBuf->uRemappedCount = 0;
		psConstBuf->psRemappedMap = IMG_NULL;
		psConstBuf->psRemappedFormat = IMG_NULL;
		InitInstArg(&psConstBuf->sBase);
		if (psDesc->eConstBuffLocation == UF_CONSTBUFFERLOCATION_MEMORY_ONLY && psDesc->uBaseAddressSAReg != USC_UNDEF)
		{
			psConstBuf->sBase.uType = USEASM_REGTYPE_SECATTR;
			psConstBuf->sBase.uNumber = psDesc->uBaseAddressSAReg;
		}
		psConstBuf->bInUse = IMG_FALSE;
		InitializeList(&psConstBuf->sLoadConstList);
	}	

	psState->psSAOffsets = psProgramParameters;
	psState->psTargetDesc = UseAsmGetCoreDesc(&psProgramParameters->sTarget);
	psState->psTargetFeatures = psState->psTargetDesc->psFeatures;
	psState->psTargetBugs = &psState->psTargetDesc->sBugs;
	psState->uTexStateSize = psState->psTargetFeatures->ui32TextureStateSize;

	/* Determine number of cores to be utilised by this shader */
	if (psState->uCompilerFlags & UF_RUNTIMECORECOUNT)
	{
		psState->uMPCoreCount = psState->psSAOffsets->uMPCoreCount;
	}
	else if (psProgramParameters->eShaderType == USC_SHADERTYPE_PIXEL)
	{
		psState->uMPCoreCount = USC_MP_CORE_COUNT_PIXEL; 
	}
	else if (psProgramParameters->eShaderType == USC_SHADERTYPE_VERTEX)
	{
		psState->uMPCoreCount = USC_MP_CORE_COUNT_VERTEX; 
	}
	/* Compute or Geometry get the max core count */
	else
	{
		psState->uMPCoreCount = USC_MP_CORE_COUNT_MAX;
	}
		
	/*
		Calculate the maximum number of simultaneous instances of a USE program across the
		whole system for the current core.
	*/
	psState->uMaxSimultaneousInstances = 
		EURASIA_MAX_USE_INSTANCES * psState->psTargetFeatures->ui32NumUSEPipes * psState->uMPCoreCount;

	/*
		Calculate the amount of storage in the hardware's GPI register bank.
	*/
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		psState->uGPISizeInScalarRegs = 
			psState->psTargetFeatures->ui32NumInternalRegisters * SGXVEC_INTERNAL_REGISTER_WIDTH_IN_DWORDS;
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		psState->uGPISizeInScalarRegs = psState->psTargetFeatures->ui32NumInternalRegisters;
	}
	ASSERT(psState->uGPISizeInScalarRegs <= MAXIMUM_GPI_SIZE_IN_SCALAR_REGS);

	psState->uMainProgInstCount = 0;
	psState->uMainProgLabelCount = 0;
	psState->uMainProgStart = UINT_MAX;
	psState->uMainProgFeedbackPhase0End = UINT_MAX;
	psState->psMainProgFeedbackPhase0EndInst = IMG_NULL;
	psState->uMainProgFeedbackPhase1Start = UINT_MAX;
	psState->uMainProgSplitPhase1Start = UINT_MAX;
	psState->uSAProgInstCount = 0;
	psState->psUseasmContext = NULL;
	psState->psPreFeedbackBlock = NULL;
	psState->psPreFeedbackDriverEpilogBlock = NULL;
	psState->psPreSplitBlock = NULL;

	/*
		Reset information about which texture samplers the driver should disable
		anisotropic filtering on.
	*/
	psState->auNonAnisoTexStages = NULL;

	#if defined(OUTPUT_USCHW)
	/*
		Initially no samplers have selected an unpacked format.
	*/
	psState->auTextureUnpackFormatSelectedMask = CallocBitArray(psState, psState->psSAOffsets->uTextureCount);
	
	/*
		Set default values for texture sampler unpacking formats.
	*/
	psState->asTextureUnpackFormat = 
		UscAlloc(psState, psState->psSAOffsets->uTextureCount * sizeof(psState->asTextureUnpackFormat[0]));
	for (uSamplerIdx = 0; uSamplerIdx < psState->psSAOffsets->uTextureCount; uSamplerIdx++)
	{
		psState->asTextureUnpackFormat[uSamplerIdx].eFormat = UNIFLEX_TEXTURE_UNPACK_FORMAT_NATIVE;
		psState->asTextureUnpackFormat[uSamplerIdx].bNormalise = IMG_FALSE;
	}
	#endif /* defined(OUTPUT_USCHW) */

	psState->uIndexableTempArraySize = 0;
	psState->apsVecArrayReg = NULL;
	psState->uNumVecArrayRegs = 0;
	psState->uAvailArrayRegs = USC_REGARRAY_LIMIT;
	
	/*
		Reset information about shader outputs.
	*/
	InitializeList(&psState->sFixedRegList);
	psState->uGlobalFixedRegCount = 0;

#ifdef DEBUG
	psState->uBlockIterationsInProgress = 0;
#endif

	/*
		Reset the list of functions.
	*/
	psState->psFnOutermost = psState->psFnInnermost = NULL;
	psState->uMaxLabel = 0;

	/*
		Initialize the list of flow control blocks.
	*/
	InitializeList(&psState->sBlockList);

#ifdef DEBUG_TIME
	USC_CLOCK_SET(psState->sTimeStart);
#endif 

#ifdef UF_TESTBENCH
	psState->uNumVerifLoopHdrs = 0;
	psState->puVerifLoopHdrs = NULL;
#endif

	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_EXTENDED_LOAD)
		|| (psState->uCompilerFlags & UF_OPENCL)
		)
	{
		psState->uMemOffsetAdjust = 0;
	}
	else
	{
		psState->uMemOffsetAdjust = 1;
	}

	psState->uNumDynamicBranches = 0;
	if ((psState->uCompilerFlags & UF_MULTIPLECONSTANTSBUFFERS) == 0)
	{
		psState->uNumOfConstsBuffsAvailable = 1;
		psState->uStaticConstsBuffer = UF_CONSTBUFFERID_LEGACY;
		psState->uTextureStateConstsBuffer = UF_CONSTBUFFERID_LEGACY;
	}
	else
	{
		psState->uNumOfConstsBuffsAvailable = UF_CONSTBUFFERID_COUNT;
		psState->uStaticConstsBuffer = UF_CONSTBUFFERID_STATIC_CONSTS;
		psState->uTextureStateConstsBuffer = UF_CONSTBUFFERID_TEX_STATE;
	}

	/*
		Reset information about secondary attribute usage.
	*/
	InitializeSAProg(psState);

	/*
		Check for a shader where any of the outputs are declared as invariant.
	*/
	psState->bInvariantShader =	IsInvariantShader(psState);

	/*
		Make a copy of the list of indexable temporary array sizes.
	*/
	psState->uIndexableTempArrayCount = psState->psSAOffsets->uIndexableTempArrayCount;
	psState->psIndexableTempArraySizes = 
		UscAlloc(psState, sizeof(psState->psIndexableTempArraySizes[0]) * psState->uIndexableTempArrayCount);
	memcpy(psState->psIndexableTempArraySizes,
		   psState->psSAOffsets->psIndexableTempArraySizes,
		   sizeof(psState->psIndexableTempArraySizes[0]) * psState->uIndexableTempArrayCount);


	/*
		Reset the lists of instructions with each opcode.
	*/
	for (uOpIdx = 0; uOpIdx < IOPCODE_MAX; uOpIdx++)
	{
		SafeListInitialize(&psState->asOpcodeLists[uOpIdx]);
	}

	/*
		Reset shader type specific variables.
	*/
	switch (psState->psSAOffsets->eShaderType)
	{
		case USC_SHADERTYPE_PIXEL: 
		{
			InitPixelShaderState(psState); 
			break;
		}
		case USC_SHADERTYPE_VERTEX: 
		case USC_SHADERTYPE_GEOMETRY:
		{
			InitVertexShaderState(psState); 
			break;
		}
		default: break;
	}	

	psState->uMaxInstMovement = psState->psSAOffsets->uMaxInstMovement;

	/*
		Set default values for branch flattening code
	*/
	psState->uMaxConditionalALUInstsToFlatten = psProgramParameters->uMaxALUInstsToFlatten;
	psState->ePredicationLevel = psProgramParameters->ePredicationLevel;
	
	/*
		Copy the optimization level parameter from the input.
	*/
	if (psState->uCompilerFlags & UF_RESTRICTOPTIMIZATIONS)
	{
		psState->uOptimizationLevel = psState->psSAOffsets->uOptimizationLevel;
	}
	else
	{
		psState->uOptimizationLevel = USC_OPTIMIZATIONLEVEL_MAXIMUM;
	}

	/*
		Clear pointers to information about virtual registers. These will be set up in
		ssa.c.
	*/
	psState->psTempVReg = NULL;
	psState->psPredVReg = NULL;	

	/*
		Reset the list of registers which are potentially unused.
	*/
	InitializeList(&psState->sDroppedUsesTempList);

	/*
		Clear state relating to groups of intermediate registers which require consecutive
		hardware register numbers.
	*/
	psState->psGroupState = NULL;
	
	InitializeList(&psState->sIndexUseTempList);
	InitializeList(&psState->sC10TempList);
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	InitializeList(&psState->sVectorTempList);
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	#if defined(OUTPUT_USPBIN)
	psState->uNextSmpID = 0;
	#endif /* defined(OUTPUT_UPBIN) */
	
	#if defined(EXECPRED)
	psState->uExecPredTemp = USC_UNDEF;
	#endif /* #if defined(EXECPRED) */
}

static
IMG_VOID ReleaseVertexShaderState(PINTERMEDIATE_STATE psState)
/*********************************************************************************
 Function		: ReleaseVertexShaderState

 Description	: Releases all dynamically allocated memory specific to
				  vertex/geometry shaders.

 Parameters		: psState	- The current compiler context

 Return			: none
*********************************************************************************/
{
	PVERTEXSHADER_STATE	psVS;

	psVS = psState->sShader.psVS;

	/* Primary attribute usage */
	UscFree(psState, psVS->auVSInputPARegUsage);

	UscFree(psState, psState->sShader.psVS);
}

static
IMG_VOID ReleasePixelShaderState(PINTERMEDIATE_STATE psState)
/*********************************************************************************
 Function		: ReleasePixelShaderState

 Description	: Releases all dynamically allocated memory specific to
				  pixel shaders.

 Parameters		: psState	- The current compiler context

 Return			: none
*********************************************************************************/
{
	PPIXELSHADER_STATE	psPS = psState->sShader.psPS;
	IMG_UINT32			uArrayIdx;

	/*
		Free information about pixel shader inputs.
	*/
	{
		PUSC_LIST_ENTRY	psOldHead;

		while ((psOldHead = RemoveListHead(&psState->sShader.psPS->sPixelShaderInputs)) != NULL)
		{
			PPIXELSHADER_INPUT	psPSInput = IMG_CONTAINING_RECORD(psOldHead, PPIXELSHADER_INPUT, sListEntry);

			UscFree(psState, psPSInput);
		}
	}

	/*
		Free information about dynamically indexable pixel shader inputs.
	*/
	for (uArrayIdx = 0; uArrayIdx < psPS->uTextureCoordinateArrayCount; uArrayIdx++)
	{
		UscFree(psState, psPS->asTextureCoordinateArrays[uArrayIdx].apsIterations);
	}
	UscFree(psState, psPS->asTextureCoordinateArrays);

	/*
		Free pixel shader state structure.
	*/
	UscFree(psState, psPS);
}

IMG_INTERNAL
IMG_VOID ReleaseFixedReg(PINTERMEDIATE_STATE psState, PFIXED_REG_DATA psFixedReg)
/*********************************************************************************
 Function		: ReleaseFixedReg

 Description	: Release storage used for a fixed registers.

 Parameters		: psState			- The current compiler context
				  psFixedReg		- Fixed register to free.

 Return			: none
*********************************************************************************/
{
	IMG_UINT32	uRegIdx;

	if (psFixedReg->bPrimary)
	{
		RemoveFromList(&psState->sFixedRegList, &psFixedReg->sListEntry);
	}
	else
	{
		RemoveFromList(&psState->sSAProg.sFixedRegList, &psFixedReg->sListEntry);
	}

	if (psFixedReg->puUsedChans != NULL)
	{
		UscFree(psState, psFixedReg->puUsedChans);
	}

	#if defined(OUTPUT_USPBIN)
	if (psFixedReg->uNumAltPRegs > 0)
	{
		UscFree(psState, psFixedReg->asAltPRegs);
	}
	#endif /* defined(OUTPUT_USPBIN) */

	if (psFixedReg->auMask != NULL)
	{
		UscFree(psState, psFixedReg->auMask);
	}
	
	if (psFixedReg->aeUsedForFeedback != NULL)
	{
		UscFree(psState, psFixedReg->aeUsedForFeedback);
	}

	for (uRegIdx = 0; uRegIdx < psFixedReg->uConsecutiveRegsCount; uRegIdx++)
	{
		if (psFixedReg->bLiveAtShaderEnd)
		{
			UseDefDropFixedRegUse(psState, psFixedReg, uRegIdx);
		}
		else
		{
			UseDefDropFixedRegDef(psState, psFixedReg, uRegIdx);
		}
	}
	UscFree(psState, psFixedReg->asVRegUseDef);
	UscFree(psState, psFixedReg->auVRegNum);
	UscFree(psState, psFixedReg->aeVRegFmt);
	UscFree(psState, psFixedReg);
}

static
IMG_VOID ReleaseFixedRegList(PINTERMEDIATE_STATE psState, PUSC_LIST psFixedRegList)
/*********************************************************************************
 Function		: ReleaseFixedRegList

 Description	: Release storage used for a list of fixed registers.

 Parameters		: psState			- The current compiler context
				  psFixedRegList	- Fixed register list to free.

 Return			: none
*********************************************************************************/
{
	while (psFixedRegList->psHead != NULL)
	{
		PFIXED_REG_DATA	psFixedReg = IMG_CONTAINING_RECORD(psFixedRegList->psHead, PFIXED_REG_DATA, sListEntry);

		ReleaseFixedReg(psState, psFixedReg);
	}
}

/*********************************************************************************
 Function		: ReleaseState

 Description	: Releases all dynamically allocated memory associated with 
				  the instructions and the compiler state.

 Parameters		: psState	- The current compiler context

 Return			: none
*********************************************************************************/
IMG_INTERNAL
IMG_VOID ReleaseState(PINTERMEDIATE_STATE psState)
{
	IMG_UINT32	uIdx, uNumOfConstsBuffsAvailable;
	
	/* 
	 * Free intermediate instructions and code blocks. 
	 */
#ifdef SRC_DEBUG
	psState->uFlags |= USC_FLAGS_REMOVING_INTERMEDIATE_CODE;
#endif /* SRC_DEBUG */
	
	/* 
	 * Label dependent data 
	 */
	while (psState->psFnOutermost)
		FreeFunction(psState, psState->psFnOutermost);
	
	/* 
	 * Compiler state data
	 */
	
	if ((psState->uCompilerFlags & UF_MULTIPLECONSTANTSBUFFERS) == 0)
	{
		uNumOfConstsBuffsAvailable = 1;
	}
	else
	{
		uNumOfConstsBuffsAvailable = UF_CONSTBUFFERID_COUNT;
	}
	
	/* Remapped constants */
	for (uIdx = 0; uIdx < uNumOfConstsBuffsAvailable; uIdx++)
	{
		PCONSTANT_BUFFER	psConstBuf = &psState->asConstantBuffer[uIdx];

		FreeArray(psState, &psConstBuf->psRemappedMap);
		FreeArray(psState, &psConstBuf->psRemappedFormat);
	}

	/*
		Destroy information about uses/defines of virtual registers.
	*/
	UseDefDeinitialize(psState);

	/* Indexable temp arrays */
	{
		const IMG_UINT32 uSize = psState->uIndexableTempArrayCount;

		for (uIdx = 0; uIdx < uSize; uIdx ++)
		{
			UscFree(psState, psState->apsTempVecArray[uIdx]);
		}
		UscFree(psState, psState->apsTempVecArray);

		for (uIdx = 0; uIdx < psState->uNumVecArrayRegs; uIdx ++)
		{
			UscFree(psState, psState->apsVecArrayReg[uIdx]);
		}
		UscFree(psState, psState->apsVecArrayReg);
	}

	/*
		Free storage for in-register constants.
	*/
	{
		PUSC_LIST_ENTRY	psListEntry;

		while ((psListEntry = RemoveListHead(&psState->sSAProg.sInRegisterConstantList)) != NULL)
		{
			PINREGISTER_CONST	psConst = IMG_CONTAINING_RECORD(psListEntry, PINREGISTER_CONST, sListEntry);

			UscFree(psState, psConst);
		}
		psState->sSAProg.uInRegisterConstantCount = 0;
	}

	/*
		Free information about secondary attributes available for allocation.
	*/
	if (psState->sSAProg.asAllocRegions != NULL)
	{
		UscFree(psState, psState->sSAProg.asAllocRegions);
	}

	/*
		Free storage for information about indexed secondary
		attributes.
	*/
	{
		PUSC_LIST_ENTRY	psListEntry;

		while ((psListEntry = RemoveListHead(&psState->sSAProg.sInRegisterConstantRangeList)) != NULL)
		{
			PCONSTANT_INREGISTER_RANGE	psRange;
			
			psRange = IMG_CONTAINING_RECORD(psListEntry, PCONSTANT_INREGISTER_RANGE, sListEntry);
			UscFree(psState, psRange);
		}
	}

	/* 
		Free storage for fixed hardware register registers data. 
	*/
	ReleaseFixedRegList(psState, &psState->sFixedRegList);
	
	/*
		Free shader specific state.
	*/
	switch (psState->psSAOffsets->eShaderType)
	{
		case USC_SHADERTYPE_PIXEL: 
		{
			ReleasePixelShaderState(psState); 
			break;
		}
		case USC_SHADERTYPE_VERTEX: 
		case USC_SHADERTYPE_GEOMETRY:
		{
			ReleaseVertexShaderState(psState); 
			break;
		}
		default:
		{
			break;
		}
	}

	ReleaseVaildShaderOutputsData(psState);

	/*
		Free secondary update program results structure.
	*/
	{
		PUSC_LIST_ENTRY	psListEntry;

		while ((psListEntry = RemoveListHead(&psState->sSAProg.sResultsList)) != NULL)
		{
			PSAPROG_RESULT	psResult = IMG_CONTAINING_RECORD(psListEntry, PSAPROG_RESULT, sListEntry);

			UscFree(psState, psResult);
		}

		ReleaseFixedRegList(psState, &psState->sSAProg.sFixedRegList);
	}

	#if defined(OUTPUT_USCHW)
	/*
		Free information about texture unpacking.
	*/
	UscFree(psState, psState->auTextureUnpackFormatSelectedMask);
	UscFree(psState, psState->asTextureUnpackFormat);
	#endif /* defined(OUTPUT_USCHW) */

	/*
		Free information about which texture samplers the driver should disable
		anisotropic filtering on.
	*/
	if (psState->auNonAnisoTexStages != NULL)
	{
		UscFree(psState, psState->auNonAnisoTexStages);
		psState->auNonAnisoTexStages = NULL;
	}

	/*
		Destroy information about virtual registers.
	*/
	ReleaseVRegisterInfo(psState);

	/*
		Destroy the list of indexable temporary array sizes.
	*/
	UscFree(psState, psState->psIndexableTempArraySizes);
	psState->psIndexableTempArraySizes = NULL;

	#ifdef SRC_DEBUG
	UscFree(psState, psState->puSrcLineCost);
	#endif /* SRC_DEBUG */
}

static
PCODEBLOCK GenerateSwitchTest(PINTERMEDIATE_STATE	psState,
							  PCODEBLOCK			psSwitch,
							  IMG_UINT32			uCase,
							  IMG_PUINT32			puPredTemp)
/*********************************************************************************
 Function		: GenerateSwitchTest

 Description	: Generate intermediate instructions for testing whether a switch
				  argument matches a particular case.

 Parameters		: psState	- The current compiler context
				  psSwitch	- The block which selects its successor using a switch.
				  uCase		- The index of the case to generate code for.

 Return			: A basic block containing the instructions.
*********************************************************************************/
{
	PCODEBLOCK	psTestBlock;
	IMG_UINT32	uCaseVal;
	PINST		psXorInst;

	psTestBlock = AllocateBlock(psState, psSwitch->psOwner);

	/*
		if there's no default, we'll never use auCaseValues[uNumSuccs - 1],
		even though it exists (apsSuccs[uNumSuccs - 1] is always used as
		the successor for the final test block - set after the loop)
	*/
	ASSERT(uCase < psSwitch->uNumSuccs);
	uCaseVal = psSwitch->u.sSwitch.auCaseValues[uCase];

	/*
		Allocate a new predicate register for the result of the test.
	*/
	*puPredTemp = GetNextPredicateRegister(psState);

	/*
		pTEMP = BITWISE_XOR(SWITCH_ARG, CASE_VAL) == 0 
				(SWITCH_ARG == CASE_VAL)
	*/
	psXorInst = AllocateInst(psState, NULL/* TODO */);
	SetOpcodeAndDestCount(psState, psXorInst, ITESTPRED, 1 /* uDestCount */);
	psXorInst->u.psTest->eAluOpcode = IXOR;
	MakePredicateDest(psState, psXorInst, 0 /* uDestIdx */, *puPredTemp);
	CompOpToTest(psState, UFREG_COMPOP_EQ, &psXorInst->u.psTest->sTest);
	SetSrcFromArg(psState, psXorInst, 0 /* uSrcIdx */, psSwitch->u.sSwitch.psArg);
	AppendInst(psState, psTestBlock, psXorInst);
	LoadImmediate(psState, psXorInst, 1 /* uArgIdx */, uCaseVal);

	/*
		Flag the new predicate register as live out of the block containing
		the test.
	*/
	SetRegisterLiveMask(psState, 
						&psTestBlock->sRegistersLiveOut, 
						USEASM_REGTYPE_PREDICATE, 
						*puPredTemp, 
						0 /* uArrayOffset */, 
						USC_DESTMASK_FULL);

	return psTestBlock;
}

IMG_INTERNAL
IMG_VOID ConvertSwitchBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psSwitch, IMG_PVOID pbChanged)
/*********************************************************************************
 Function		: ConvertSwitchBP

 Description	: Convert a block which selects its successor using a switch to a
				  sequence of conditional blocks.

 Parameters		: psState	- The current compiler context
				  psSwitch	- The block which selects its successor using a switch.
				  pbChanged	- Set to TRUE on return if changes were made to the
							block.

 Return			: Nothing.
*********************************************************************************/
{
	IMG_UINT32 uCase;
	PCODEBLOCK psPrevious, psPrevSucc;
	IMG_UINT32 uPrevPredNum;
	PCODEBLOCK psFirstNewSucc;

	/*
		Convert from:

			----------
			|        |
			| SWITCH |
			|        |
			---------
			|  |  | |
			/  |  | \
		   x   y  z  DEFAULT

	   To:

			----------
			|        |
			| SWITCH |     
			|        |
			----------
			    |
				|
			----------
			|  TEST  |
			----------
			|       |
			/       \
		   x		--------
					| TEST |
					--------
					|      |
					/      \
				   y	   --------
						   | TEST |
						   --------
						   |      |
						   /      \
					      z       DEFAULT
	*/

	if (psSwitch->eType != CBTYPE_SWITCH) return;

	if (pbChanged) *((IMG_PBOOL)pbChanged) = IMG_TRUE;
	
	psPrevious = NULL;
	psPrevSucc = NULL;
	psFirstNewSucc = NULL;
	uPrevPredNum = USC_UNDEF;
	for (uCase = 0; uCase < psSwitch->uNumSuccs; uCase++)
	{
		PCODEBLOCK psNewSucc;
		IMG_UINT32 uPredNum;

		if (psSwitch->u.sSwitch.bDefault &&
			uCase != (psSwitch->uNumSuccs - 1) &&
			psSwitch->asSuccs[uCase].psDest == psSwitch->asSuccs[psSwitch->uNumSuccs - 1].psDest)
		{
			//case has same target as default - ignore
			continue;
		}
		
		if (uCase < (psSwitch->uNumSuccs - 1))
		{
			/*
				Generate code to test the switch value.
			*/
			psNewSucc = GenerateSwitchTest(psState, psSwitch, uCase, &uPredNum);
		}
		else
		{
			/*
				For the default case go directly to the default case
				body.
			*/
			psNewSucc = psSwitch->asSuccs[psSwitch->uNumSuccs - 1].psDest;
			uPredNum = USC_UNDEF;
		}

		if (psPrevious)
		{
			/*
				Set the previous block's successor as either the previous
				case body or this block.
			*/
			ASSERT (uCase > 0);
			SetBlockConditional(psState,
								psPrevious,
								uPrevPredNum,
								psPrevSucc,
								psNewSucc,
								psSwitch->bStatic);
		}
		else
		{
			/*
				To avoid freeing the structures associated with the switch just 
				store a pointer to the first block. Later we'll set it as the
				unconditional successor of the old switch.
			*/
			psFirstNewSucc = psNewSucc;
		}
		psPrevious = psNewSucc;
		psPrevSucc = psSwitch->asSuccs[uCase].psDest;
		uPrevPredNum = uPredNum;
	}

	/*
		Free memory for the structure representing the register used to choose the
		switch case.
	*/
	UseDefDropUse(psState, &psSwitch->u.sSwitch.sArgUse.sUseDef);
	UscFree(psState, psSwitch->u.sSwitch.psArg);
	
	/*
		Set the block which checks for the first case as the unconditional successor of
		the old switch. The two blocks will be merged by MergeBasicBlocks later.
	*/
	ASSERT(psFirstNewSucc != NULL);
	SetBlockUnconditional(psState, psSwitch, psFirstNewSucc);
}

/*
  SORT_DATA: Data for sorting arrays by accesses.
  uAccesses : Number of accesss
  uArrayNum : Array number
  uNumRegs  : Number of registers needed
*/
typedef struct _SORT_DATA_
{
	IMG_UINT32 uAccesses;
	IMG_UINT32 uArrayNum;
	IMG_UINT32 uNumRegs;
	ARRAY_TYPE eArrayType;
} SORT_DATA, *PSORT_DATA;

static
IMG_INT32 CmpArraySetupData(IMG_VOID* pvLeft, IMG_VOID* pvRight)
/*****************************************************************************
 FUNCTION	: CmpArraySetupData

 PURPOSE	: Order array setup data elements by accesses.

 PARAMETERS	: pvLeft    - Left element
              pvRight   - Right element

 RETURNS	: < 0         if pvLeft is busier than pvRight
              > 0         if pvRight is busier than pvLeft
              == 0        if pvLeft and pvRight are equally busy.
*****************************************************************************/
{
	PSORT_DATA psLeft, psRight;

	psLeft = (PSORT_DATA)pvLeft;
	psRight = (PSORT_DATA)pvRight;
	
	if((psLeft->eArrayType == ARRAY_TYPE_VERTICES_BASE) && (psRight->eArrayType != ARRAY_TYPE_VERTICES_BASE))
	{
		return (-1);
	}
	else if((psLeft->eArrayType != ARRAY_TYPE_VERTICES_BASE) && (psRight->eArrayType == ARRAY_TYPE_VERTICES_BASE))
	{
		return (1);
	}
	else
	{
		if (psLeft->uAccesses > psRight->uAccesses)
			return (-1);
		if (psLeft->uAccesses < psRight->uAccesses)
			return (1);
	}
	return 0;
}

IMG_INTERNAL
IMG_UINT32 AddNewRegisterArray(PINTERMEDIATE_STATE	psState,
							   ARRAY_TYPE			eArrayType,
							   IMG_UINT32			uArrayNum,
							   IMG_UINT32			uChannelsPerDword,
							   IMG_UINT32			uNumRegs)
{
	PUSC_VEC_ARRAY_REG	psVecReg;
	IMG_UINT32			uVecIdx;

	/* Get the index of the next slot */
	uVecIdx = psState->uNumVecArrayRegs ++;

	/* Increase the size of the array. */
	IncreaseArraySizeInPlace(psState,
				(psState->uNumVecArrayRegs - 1) * sizeof(PUSC_VEC_ARRAY_REG),
				(psState->uNumVecArrayRegs + 0) * sizeof(PUSC_VEC_ARRAY_REG),
				(IMG_PVOID*)&psState->apsVecArrayReg);

	/* Put array into registers */
	psVecReg = UscAlloc(psState, sizeof(USC_VEC_ARRAY_REG));
	psVecReg->uArrayNum = uArrayNum;
	psVecReg->uRegs = uNumRegs;
	psVecReg->eArrayType = eArrayType;
	psVecReg->uRegType = USEASM_REGTYPE_TEMP;
	psVecReg->uChannelsPerDword = uChannelsPerDword;
	psVecReg->psUseDef = NULL;
	psVecReg->psBaseUseDef = NULL;
	psVecReg->psGroup = NULL;

	/* Take registers */
	psVecReg->uBaseReg = GetNextRegisterCount(psState, psVecReg->uRegs);

	/* All done, store the array information */
	psState->apsVecArrayReg[uVecIdx] = psVecReg;

	return uVecIdx;
}

static IMG_VOID SetupLoadStoreArray(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: SetupLoadStoreArray

 PURPOSE	: Setup indexable temp arrays for load/store expansion

 PARAMETERS	: psState		- Compiler state.

 RETURNS	: Nothing

 NOTES		: Decides which arrays to put into registers. The current test 
			  is simple minded: the busiest arrays using less than 
			  USC_REGARRAY_LIMIT are put into registers. When the number 
			  of register arrays is USC_MAX_NUM_REGARRAYS, no more arrays are 
			  put into registers.
*****************************************************************************/
{
	const IMG_UINT32 uNumArrays = psState->uIndexableTempArrayCount;
	PUSC_VEC_ARRAY_DATA psCurr;
	IMG_UINT32 uNumRegs;
	IMG_UINT32 uCtr, uCurrIdx;
	IMG_UINT32 uNumRegArrays;
	IMG_UINT32 uChansPerReg = 0;
	IMG_PVOID *apsArrayData;

	/* Check for space */
	if (psState->uAvailArrayRegs < 1)
	{
		/* No more registers available for arrays */
		return;
	}

	/* 
	   Check for busyness 

	   These checks assume that only one array will be ever be in registers.
	*/
	apsArrayData = UscAlloc(psState, sizeof(IMG_PVOID) * uNumArrays);
	memset(apsArrayData, 0, sizeof(IMG_PVOID) * uNumArrays);

	psCurr = NULL; 
	uCurrIdx = 0;
	for (uCtr = 0; uCtr < uNumArrays; uCtr ++)
	{
		const IMG_UINT32 uMaxSize = psState->uAvailArrayRegs;
		PUSC_VEC_ARRAY_DATA psData;
		IMG_UINT32 uBusyness;

		psData = psState->apsTempVecArray[uCtr];
		if (psData == NULL)
		{
			/* No array here */
			continue;
		}
		if (psData->uLoads == 0)
		{
			/* No loads, drop the array */
			UscFree(psState, psState->apsTempVecArray[uCtr]);
			ASSERT(psState->apsTempVecArray[uCtr] == NULL);
			continue;
		}
		if (!psData->bStatic)
		{
			/*
			  Array must be statically indexed (to handle spilling in
			  the register array).
			*/
			continue;
		}
		uBusyness = (psData->uLoads + psData->uStores);

		/* Final test: how many registers are needed */
		psCurr = psData;
		if (psCurr->eFmt == UF_REGFORMAT_F32 ||
			psCurr->eFmt == UF_REGFORMAT_U32 ||
			psCurr->eFmt == UF_REGFORMAT_I32 ||
			psCurr->eFmt == UF_REGFORMAT_U16 ||
			psCurr->eFmt == UF_REGFORMAT_I16 ||
			psCurr->eFmt == UF_REGFORMAT_U8_UN ||
			psCurr->eFmt == UF_REGFORMAT_I8_UN)
		{
			uChansPerReg = 1;
		}
		else if (psCurr->eFmt == UF_REGFORMAT_F16)
		{
			uChansPerReg = 2;
		}
		else if (psCurr->eFmt == UF_REGFORMAT_C10)
		{
			uChansPerReg = 2;
		}
		else
		{
			imgabort();		// Only f32, f16 or c10 is supported
		}
		
		uNumRegs = ((psData->uVecs * CHANNELS_PER_INPUT_REGISTER) / uChansPerReg);
		if (uNumRegs > uMaxSize)
		{
			
			/* Array takes too many registers */
			continue;			
		}

		/* This array is a candidate for placing in registers */
		{
			PSORT_DATA psElem = UscAlloc(psState, sizeof(SORT_DATA));

			psElem->uAccesses = uBusyness;
			psElem->uArrayNum = uCtr;
			psElem->uNumRegs = uNumRegs;

			apsArrayData[uCurrIdx] = (IMG_PVOID)psElem;
			uCurrIdx += 1;
		}
	}

	/* Sort array of candidates by accesses */
	InsertSort(psState, CmpArraySetupData, uCurrIdx, apsArrayData);

	/* Iterate through the candidates, putting each into memory */
	uNumRegArrays = 0;
	for (uCtr = 0; uCtr < uCurrIdx; uCtr ++)
	{
		PUSC_VEC_ARRAY_DATA psData;
		IMG_UINT32 uArrayNum;
		IMG_UINT32 uVecIdx;
		PSORT_DATA psElem;

		if (apsArrayData[uCtr] == NULL)
		{
			/* Nothing to do */
			continue;
		}

		/* Get the array number and data */
		psElem = (PSORT_DATA)apsArrayData[uCtr];
		uArrayNum = psElem->uArrayNum;
		uNumRegs = psElem->uNumRegs;
		psData = psState->apsTempVecArray[uArrayNum];
		
		/* Test for size */
		if (psState->uAvailArrayRegs < uNumRegs)
		{
			/* Array is too large */
			continue;
		}

		if (psData->bStatic)
		{
			/*
				Allocate a range of registers (not dynamically indexable).
			*/
			psData->uBaseTempNum = GetNextRegisterCount(psState, uNumRegs);
			psData->uRegArray = USC_UNDEF;
		}
		else
		{
			/*
				Add a new set of registers which can be dynamically indexed.
			*/
			uVecIdx = AddNewRegisterArray(psState, 
										  ARRAY_TYPE_NORMAL, 
										  uArrayNum, 
										  LONG_SIZE /* uChannelsPerDword */,
										  uNumRegs);

			/* Update the array record */
			psData->uRegArray = uVecIdx;
		}
		psData->bInRegs = IMG_TRUE;

		/* Reduce the size of the space required for indexable temporaries in memory. */
		psState->uIndexableTempArraySize -= psData->uSize;

		/* Reduce number of registers available for arrays */
		psState->uAvailArrayRegs -= uNumRegs;
		/* Increment the count of register arrays */
		uNumRegArrays += 1;

		/* Test limits */
		if (psState->uAvailArrayRegs < 2)
		{
			/* No more space for arrays */
			break;
		}
		ASSERT(uNumRegArrays <= uNumArrays);
		/* Provide a way of limiting the number of register arrays */
		if (!(uNumRegArrays < USC_MAX_NUM_REGARRAYS))
		{
			break;
		}
	}

	/* Clean up */
	for (uCtr = 0; uCtr < uCurrIdx; uCtr++)
	{
		UscFree(psState, apsArrayData[uCtr]);
	}
	UscFree(psState, apsArrayData);
}

static
IMG_VOID EmitLdStRegIndex(PINTERMEDIATE_STATE psState,
						  PCODEBLOCK psCodeBlock,
						  PARG psDynReg,
						  PARG psBaseReg,
						  IMG_UINT32 uChansPerReg,
						  PINST psInsertPoint,
						  IMG_PUINT32 puPredReg)
/*****************************************************************************
 FUNCTION	: EmitLdStIndex

 PURPOSE	: Emit code to set up an index to a component in a register array

 PARAMETERS	: psState		- Compiler state.
			  psCodeBlock	- Block to insert the code into.
			  psDynReg		- Dynamic offset register
			  psBaseReg		- Base registser
			  uChansPerReg - Number of input channel stored in each hw register
			  psInsertPoint - Insertion point in the code block
			  puPredReg		- Predicate selecting channel 0 or 2

 RETURNS	: Predicate register in psPredReg (set to immediate 0 if no predicate).
			  psPredReg is true iff psDynReg is 
*****************************************************************************/
{
	/* 
	   Emitted code:

	   ChansPerReg = ChansPerReg(Fmt)

	   if (ChansPerReg != 1)
	   {
	     and.tt&z p, DynReg, #1		// test for even
		 shr sDynReg, DynReg, #1
	   }

	   imae TempReg, DynReg, 1, &BaseReg

	   p == odd(DynReg)
	   DynReg = TempReg
	*/
	IMG_UINT32 uPredReg;
	PINST psInst;
	IMG_UINT32 uTempReg;

	/* Take a temp register */
	uTempReg = psState->uNumRegisters++;

	if (uChansPerReg != 1)	// More than one component in each register
	{
		ASSERT(uChansPerReg == 2);
		/* 
		   PreReg = odd(DynReg)
		   
		   and.tt&nz p, DynReg, #1
		*/
		uPredReg = psState->uNumPredicates ++;
		
		psInst = AllocateInst(psState, psInsertPoint);

		SetOpcodeAndDestCount(psState, psInst, ITESTPRED, 1 /* uDestCount */);
		psInst->u.psTest->eAluOpcode = IAND;

		MakePredicateArgument(&psInst->asDest[0], uPredReg);
		CompOpToTest(psState, UFREG_COMPOP_NE, &psInst->u.psTest->sTest);

		psInst->asArg[0] = *psDynReg;

		psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psInst->asArg[1].uNumber = 1;

		InsertInstBefore(psState, psCodeBlock, psInst, psInsertPoint);

		/*  shr DynReg, DynReg, #1 */
		psInst = AllocateInst(psState, psInsertPoint);
		
		SetOpcode(psState, psInst, ISHR);
		psInst->asDest[0] = *psDynReg;

		psInst->asArg[BITWISE_SHIFTEND_SOURCE] = *psDynReg;

		psInst->asArg[BITWISE_SHIFT_SOURCE].uType = USEASM_REGTYPE_IMMEDIATE;
		psInst->asArg[BITWISE_SHIFT_SOURCE].uNumber = 1;

		InsertInstBefore(psState, psCodeBlock, psInst, psInsertPoint);	
	}
	else
	{
		uPredReg = USC_PREDREG_NONE;
	}

	/* imae TempReg, DynReg, ChansPerReg, &BaseReg */
	{
		psInst = AllocateInst(psState, psInsertPoint);

		SetOpcode(psState, psInst, IIMAE);
		psInst->asDest[0] = *psDynReg;
		psInst->asDest[0].uNumber = uTempReg;

		psInst->asArg[0] = *psDynReg;

		psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psInst->asArg[1].uNumber = uChansPerReg;

		psInst->asArg[2] = *psBaseReg;

		InsertInstBefore(psState, psCodeBlock, psInst, psInsertPoint);
	}

	/* Update the index register */
	psDynReg->uNumber = uTempReg;
	/* Store the predicate register */
	if (puPredReg != NULL)
	{
		*puPredReg = uPredReg;
	}
}

static
IMG_VOID EmitLdStRegMove(PINTERMEDIATE_STATE psState,
						 PCODEBLOCK psCodeBlock,
						 PINST psInsertPoint,
						 UF_REGFORMAT eFmt,
						 IMG_BOOL bLoad,
						 IMG_BOOL bSecond,
						 IMG_UINT32 uRegChan,
						 IMG_UINT32 uPredReg,
						 PARG psDestReg,
						 PARG psDataReg)
/*****************************************************************************
 FUNCTION	: EmitLdStRegMove

 PURPOSE	: Emit code to move data between a register and a register array

 PARAMETERS	: psState		- Compiler state.
			  psCodeBlock	- Block to insert the code into.
			  psInsertPoint - Insertion point in the code block
			  eFmt			- Register format
			  bLoad			- Whether a Load or store
			  bSecond		- Whether the first or second of a pair of moves
			  uRegChan		- Register channel being moved
			  uPredReg		- Predicate register
			  psDestReg		- Destination of the data
			  psDataReg		- Source of the data

 RETURNS	: Nothing
*****************************************************************************/
{
	PINST psInst;
	PINST psMove1, psMove2;
	PARG psMovSrc = NULL;
	IMG_UINT32 uExtraOffset;
	IMG_UINT32 uStoreComp = USC_DESTMASK_FULL, uRegComp = USC_DESTMASK_FULL;
	IMG_BOOL bOddStoreOffset = ((uRegChan & 1) == 0) ? IMG_FALSE : IMG_TRUE;
	IMG_BOOL bInstIsPack;

	/* Set up the instruction */
	psInst = AllocateInst(psState, psInsertPoint);
	psMove1 = NULL;
	psMove2 = NULL;

	psDestReg->eFmt = eFmt;
	uExtraOffset = 0;
	bInstIsPack = IMG_FALSE;
	if (eFmt == UF_REGFORMAT_F32)
	{
		SetOpcode(psState, psInst, IMOV);
		psMovSrc = &psInst->asArg[0];
	}
	else if (eFmt == UF_REGFORMAT_F16)
	{
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
    	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
		{
			SetOpcode(psState, psInst, IUNPCKU16U16);
		}
		else
    	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			SetOpcode(psState, psInst, IPCKF16F16);
		}
		bInstIsPack = IMG_TRUE;
		psMovSrc = &psInst->asArg[0];

		if (bLoad)
		{
			if (!bSecond)
			{
				if (bOddStoreOffset)
				{
					uRegComp = ((1U << 3) | (1U << 2));
					uStoreComp = 2;
				}
				else  /* !bOddStoreOffset */
				{
					uRegComp = ((1U << 1) | (1U << 0));
					uStoreComp = 0;
				}
			}
			else /* bSecond */
			{
				if (bOddStoreOffset)
				{
					uRegComp = (1U << 3) | (1U << 2);
					uStoreComp = 0;
					uExtraOffset = 1;
				}
				else
				{
					uRegComp = (1U << 1) | (1U << 0);
					uStoreComp = 2;
				}
			}
		}
		else /* !bLoad */
		{
			if (!bSecond)
			{
				if (bOddStoreOffset)
				{
					uStoreComp = (1U << 3) | (1U << 2);
					uRegComp = 2;
				}
				else /* !bOddStoreOffset */
				{
					uStoreComp = (1U << 1) | (1U << 0);
					uRegComp = 0;
				}
			}
			else /* bSecond */
			{
				if (bOddStoreOffset)
				{
					uStoreComp = (1U << 1) | (1U << 0);
					uRegComp =  2 ;
					uExtraOffset = 1;
				}
				else /* !bOddStoreOffset */
				{
					uStoreComp = (1U << 3) | (1U << 2);
					uRegComp = 0;
				}
			}
		}

		/* Set up remaining arguments */
		psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psInst->asArg[1].uNumber = 0;
	}
	else if (eFmt == UF_REGFORMAT_C10)
	{
		SetOpcode(psState, psInst, IPCKC10C10);
		bInstIsPack = IMG_TRUE;

		if (bLoad)
		{
			bInstIsPack = IMG_TRUE;

			uRegComp = (1U << uRegChan);
			if (!bSecond)
			{
				uStoreComp = bOddStoreOffset ? 2U : 0U;
			}
			else /* bSecond */
			{
				if (bOddStoreOffset)
				{
					uStoreComp = 0;
					uExtraOffset = 1;
				}
				else
				{
					uStoreComp = 2;
				}
			}
				
			psDestReg->eFmt = UF_REGFORMAT_C10;
			psDataReg->eFmt = UF_REGFORMAT_C10;
			uRegComp = g_puSwapRedAndBlueMask[uRegComp];
		}
		else /* !bLoad */
		{
			uRegComp = uRegChan;
			if (!bSecond)
			{
				if (bOddStoreOffset)
					uStoreComp = (1U << 3) | (1U << 2);
				else
					uStoreComp = (1U << 1) | (1U << 0);
			}
			else /* bSecond */
			{
				if (bOddStoreOffset)
				{
					uStoreComp = (1U << 1) | (1U << 0);
					uExtraOffset = 1;
				}
				else
				{
					uStoreComp = (1U << 3) | (1U << 2);
				}
			}

			psDestReg->eFmt = UF_REGFORMAT_C10;
			psDataReg->eFmt = UF_REGFORMAT_C10;
			uRegComp = g_puSwapRedAndBlueChan[uRegComp];
		}

		/* Set up arguments */
		psMovSrc = &psInst->asArg[0];
		psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psInst->asArg[1].uNumber = 0;
	}
	else
	{
		imgabort();
	}

	/* Setup predication, negated on the first instruction of a pair */
	SetPredicate(psState, psInst, uPredReg, bSecond /* bPredNegate */);

	/* Set the destination and source of the move instruction */
	psInst->asDest[0] = *psDestReg;
	(*psMovSrc) = *psDataReg;

	/* Set the components and add the extra offset */	
	if (eFmt != UF_REGFORMAT_F32)
	{
		if (bLoad)
		{
			SetPCKComponent(psState, psInst, 0 /* uArgIdx */, uStoreComp);
			psInst->auDestMask[0] = uRegComp;
			if (uRegComp != USC_ALL_CHAN_MASK)
			{
				SetPartiallyWrittenDest(psState, psInst, 0 /* uDestIdx */, psDestReg);
			}

			if (psMovSrc->uType == USC_REGTYPE_REGARRAY)
			{
				psMovSrc->uArrayOffset += uExtraOffset;
			}
		}
		else
		{
			psInst->auDestMask[0] = uStoreComp;
			SetPCKComponent(psState, psInst, 0 /* uArgIdx */, uRegComp);

			if(psInst->asDest[0].uType == USC_REGTYPE_REGARRAY)
			{
				psInst->asDest[0].uArrayOffset += uExtraOffset;
			}
		}
	}

	/*
	  Packs to indexed destination have restrction on the supported masks.
	  Moves with a temporary variable may be needed.
	 */
	if (bInstIsPack &&
		uStoreComp != 0 && uStoreComp != USC_DESTMASK_FULL &&
		psDestReg->uIndexType != USC_REGTYPE_NOINDEX)
	{
		IMG_UINT32 uTmpRegNum;
		PARG psTmpReg;

		/*
		  Need to add moves:

		  pck.yyxx dst[i], src1, src2
		  -->
		  mov tmp, dst[i]
		  pck.yyxx, tmp, src1, src2
		  mov dst[i], tmp
		*/

		/* Temp. register number */
		uTmpRegNum = psState->uNumRegisters++;

		/* First mov instruction */
		psMove1 = FormMove(psState, NULL, &psInst->asDest[0], psInsertPoint);

		CopyPredicate(psState, psMove1, psInst);

		psMove1->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psMove1->asDest[0].uNumber = uTmpRegNum;

		/* Store the temporary register */
		psTmpReg = &psMove1->asDest[0];

		/* Second mov instruction */
		psMove2 = FormMove(psState, &psInst->asDest[0], psTmpReg, psInsertPoint);

		CopyPredicate(psState, psMove2, psInst);

		/* Change the instruction's destination */
		psInst->asDest[0].uType = psTmpReg->uType;
		psInst->asDest[0].uNumber = psTmpReg->uNumber;
		/* Clear the index */
		psInst->asDest[0].uIndexType = USC_REGTYPE_NOINDEX;
		psInst->asDest[0].uIndexNumber = USC_UNDEF;
		psInst->asDest[0].uIndexArrayOffset = USC_UNDEF;
	}

	/* Add all instructions to the code block */
	if (psMove1 != NULL)
	{
		InsertInstBefore(psState, psCodeBlock, psMove1, psInsertPoint);
	}
	InsertInstBefore(psState, psCodeBlock, psInst, psInsertPoint);
	if (psMove2 != NULL)
	{
		InsertInstBefore(psState, psCodeBlock, psMove2, psInsertPoint);
	}
}

static
IMG_VOID SetFirstArrayElement(PUSC_VEC_ARRAY_DATA	psVecArrayData, 
							  UF_REGFORMAT			eFmt, 
							  PARG					psArrayElement)
/*****************************************************************************
 FUNCTION	: SetFirstArrayElement

 PURPOSE	: Initialize an argument representing the first element in an array.

 PARAMETERS	: psVecArrayData	- Input indexable temporary array.
			  eFmt				- Format of the data in the array.
			  psArrayElement	- Points to the argument structure to initialize.

 RETURNS	: Nothing.
*****************************************************************************/
{
	InitInstArg(psArrayElement);
	if (psVecArrayData->bStatic)
	{
		/* For static access, go straight to the temporary */
		psArrayElement->uType = USEASM_REGTYPE_TEMP;
		psArrayElement->uNumber = psVecArrayData->uBaseTempNum;
		psArrayElement->eFmt = eFmt;			// Format of the array
	}
	else
	{
		psArrayElement->uType = USC_REGTYPE_REGARRAY;
		psArrayElement->uNumber = psVecArrayData->uRegArray;	// Number of the array
		psArrayElement->uArrayOffset = 0;
		psArrayElement->eFmt = eFmt;
	}
}

static
IMG_BOOL ExpandRegLdStInst(PINTERMEDIATE_STATE psState,
						   PCODEBLOCK psCodeBlock,
						   PINST psLdStInst)
/*****************************************************************************
 FUNCTION	: ExpandRegLdStInst

 PURPOSE	: Expand an load or store instruction for an array in registers

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to insert the code into.
			  psInst			- Instruction to expand
			  bExpandDynamic	- Whether to expand dynamic access

 RETURNS	: IMG_FALSE on no change, IMG_TRUE if instruction was expanded
*****************************************************************************/
{
	const IMG_UINT32 uMaxIdxOffset = EURASIA_USE_INDEX_MAXIMUM_OFFSET - 1;	// Maximum offset from an index register
	const IMG_UINT32 uFirstStSrc = LDSTARR_STORE_DATA_ARGSTART;		// Index of first data source in a store instruction
	IMG_UINT32 uIndexReg;
	UF_REGFORMAT eFmt;
	IMG_UINT32 uStaticIdx;				// Register number in the array
	IMG_UINT32 uRegChanOffset;		// Offset (in registers) from channel 0 to required channel
	IMG_UINT32 uStoreChanOffset;	// Offset (in registers) from channel 0 to required channel
	IMG_UINT32 uChanPerReg;			// Number of channels in the data format
	IMG_UINT32 uArrayNum;			// Number of the array
	IMG_UINT32 uRegArrayNum;		// Number of the register array
	IMG_UINT32 uRegChan;
	IMG_UINT32 uDstIdx;			// Index of destintation registers
	ARG sIndexReg;					// Index register
	IMG_UINT32 uChanPerStore;		// Number of channels per store register
	IMG_UINT32 uChanMask;			// Mask of channels to read/write
	IMG_UINT32 uIdxOffset;			// Offset in registers already added to the index
	IMG_UINT32 uPredReg = USC_PREDREG_NONE;
	PUSC_VEC_ARRAY_REG psVecArrayReg;
	PUSC_VEC_ARRAY_DATA psVecArrayData;
	IMG_BOOL bHasDynOffset;
	IMG_BOOL bDynArray;
	IMG_BOOL bLoad;

	/*
	  LDARR dst, Rn[idx + off]
	  -->
	  add idx, idx, base(Rn)
	  mov dst, r[idx + off]

	  
	  STARR Rn[idx + off], src
	  -->
	  add idx, idx, base(Rn)
	  mov r[idx + off], src
	*/

	if (psLdStInst->eOpcode >= ILDARRF32 && 
		psLdStInst->eOpcode <= ILDARRC10)
	{
		bLoad = IMG_TRUE;
	}
	else if (psLdStInst->eOpcode >= ISTARRF32 &&
			 psLdStInst->eOpcode <= ISTARRC10)
	{
		bLoad = IMG_FALSE;
	}
	else
	{
		/* Not a load/store instruction so nothing to do*/
		return IMG_FALSE;
	}

	/* Get the format from Src0 */
	eFmt = psLdStInst->u.psLdStArray->eFmt;

	/* Get the array number from the instruction */
	uArrayNum = psLdStInst->u.psLdStArray->uArrayNum;
	psVecArrayData = psState->apsTempVecArray[uArrayNum];

	/* Get the register array number from the instruction */
	uRegArrayNum = psVecArrayData->uRegArray;

	/* Get the vector array register */
	if (psVecArrayData->bStatic)
	{
		psVecArrayReg = NULL;
	}
	else
	{
		ASSERT(uRegArrayNum < psState->uNumVecArrayRegs);
		psVecArrayReg = psState->apsVecArrayReg[uRegArrayNum];
	}

	/* Get the index register from Src1 */
	sIndexReg = psLdStInst->asArg[LDSTARR_DYNAMIC_OFFSET_ARGINDEX];

	/* Test for static array */
	bDynArray = (!psVecArrayData->bStatic) ? IMG_TRUE : IMG_FALSE;
	/* Test for dynamic offset */
	bHasDynOffset = ((bDynArray) &&  
					 (sIndexReg.uType != USEASM_REGTYPE_IMMEDIATE)) ? IMG_TRUE : IMG_FALSE;
	if (bHasDynOffset)
	{
		uIndexReg = sIndexReg.uNumber;
	}
	else
	{
		/* Set the index register to none */
		uIndexReg = USC_UNDEF;
	}

	/* Get the channel mask from the destination mask */
	uChanMask = psLdStInst->u.psLdStArray->uLdStMask;
	if (uChanMask == 0)
	{
		/* Nothing to do */
		return IMG_TRUE;
	}

	if (psState->uCompilerFlags & UF_OPENCL)
	{
		/* All formats all take the F32 path */
		if (eFmt == UF_REGFORMAT_U32 ||
			eFmt == UF_REGFORMAT_I32 ||
			eFmt == UF_REGFORMAT_F16 ||
			eFmt == UF_REGFORMAT_U16 ||
			eFmt == UF_REGFORMAT_I16 ||
			eFmt == UF_REGFORMAT_U8_UN ||
			eFmt == UF_REGFORMAT_I8_UN)
		{
			eFmt = UF_REGFORMAT_F32;
		}
		else if (eFmt != UF_REGFORMAT_F32)
		{
			/* Only f/u/i32, f/u/i16 and u/i8 formats are supported */
			imgabort();
		}
	}

	/* 
	   Set the data size (in bytes) and channels per register.
	   (C10 is stored as F16 data.)
	*/
	if (eFmt == UF_REGFORMAT_F32)
	{
		uChanPerReg = 1;
		uChanPerStore = 1;
	}
	else if (eFmt == UF_REGFORMAT_F16)
	{
		uChanPerReg = 2;    // Number of channels in normal register
		uChanPerStore = 2;  // Number of channels in array element		
	}
	else if (eFmt == UF_REGFORMAT_C10)
	{
		uChanPerReg = 4;    // Number of channels in normal register
		uChanPerStore = 2;  // Number of channels in array element
		uChanMask = g_puSwapRedAndBlueMask[uChanMask];
	}
	else
	{
		/* Only f32, f16 and c10 formats are supported */
		imgabort();
	}

	/* Get the static offset from the instruction. */
	uStaticIdx = psLdStInst->u.psLdStArray->uArrayOffset * CHANNELS_PER_INPUT_REGISTER;
	uStaticIdx = uStaticIdx / uChanPerStore;

	/* 	
	   Got the address of the first required component, now read/write each
	   specified channel.
	*/
	uIdxOffset = uMaxIdxOffset + 1;
	uDstIdx = 0;	// Index of destination registers
	for (uRegChan = 0; uRegChan < CHANNELS_PER_INPUT_REGISTER; uRegChan ++)
	{
		IMG_UINT32	uAddr;
		ARG			sDestReg;
		ARG			sDataReg;

		if ((uChanMask & (1U << uRegChan)) == 0)
		{
			/* The component is not active */
			continue;
		}

		/* Calculate the channel offset */
		uRegChanOffset = (uRegChan / uChanPerReg);                  // Register
		uStoreChanOffset = (uRegChan / uChanPerStore) + uStaticIdx; // Array element

		/* Set the address of the component */
		if (bDynArray)
		{
			IMG_BOOL bSetupIdxReg = IMG_FALSE;
			IMG_UINT32 uStoreOffset = 0;

			/* Setup the index register, if necessary */
			if (bHasDynOffset && 
				uIdxOffset > uMaxIdxOffset)
			{
				bSetupIdxReg = IMG_TRUE;
				uStoreOffset = 0;
			} 
			else if (uStoreChanOffset > uMaxIdxOffset)
			{
				bSetupIdxReg = IMG_TRUE;
				uStoreOffset = uStoreChanOffset - uIdxOffset;
			}
			
			/* Set up the index register */
			if (bSetupIdxReg)
			{
				ARG sBaseReg;

				InitInstArg(&sBaseReg);
				sBaseReg.uType = USC_REGTYPE_ARRAYBASE;	
				sBaseReg.uNumber = uRegArrayNum;
				sBaseReg.uArrayOffset = uStoreOffset; 

				ASSERT(sBaseReg.uArrayOffset < psVecArrayReg->uRegs);

				uPredReg = USC_PREDREG_NONE;

				EmitLdStRegIndex(psState, psCodeBlock, &sIndexReg, &sBaseReg,
								 uChanPerStore, psLdStInst, &uPredReg);

				/* Get the new index register */
				uIndexReg = sIndexReg.uNumber;

				/* Add just the channel offset */
				uIdxOffset = uStoreOffset;
				uStoreChanOffset = uStoreChanOffset - uIdxOffset;
			}

			uAddr = uStoreChanOffset;
		}
		else
		{
			uAddr = uStoreChanOffset;
		}

		/* Load each component from the data to the dest register. */
		if (bLoad)
		{
			/* Set up the destination */
			sDestReg = psLdStInst->asDest[0];
			sDestReg.uNumber += (uDstIdx / uChanPerReg);
			uDstIdx += 1;

			if (bDynArray)
			{
				sDataReg.uArrayOffset = uAddr;
				sDataReg.uIndexType = USEASM_REGTYPE_TEMP;	// Dynamic index into the array
				sDataReg.uIndexNumber = uIndexReg;		
				sDataReg.uIndexArrayOffset = 0;
				sDataReg.uIndexStrideInBytes = LONG_SIZE;

				ASSERT(sDataReg.uArrayOffset < psVecArrayReg->uRegs);
			}
			else
			{
				SetFirstArrayElement(psVecArrayData, eFmt, &sDataReg);
				sDataReg.uNumber += uAddr;
			}
		}
		else
		{
			/* For a store, set the value register to the register to read from */
			ASSERT((uFirstStSrc + uRegChanOffset) < psLdStInst->uArgumentCount);
			sDataReg = psLdStInst->asArg[uFirstStSrc + uRegChanOffset];

			if (bDynArray)
			{
				sDestReg.uArrayOffset = uAddr;
				sDestReg.uIndexType = USEASM_REGTYPE_TEMP;	// Dynamic index into the array
				sDestReg.uIndexNumber = uIndexReg;
				sDestReg.uIndexArrayOffset = 0;
				sDestReg.uIndexStrideInBytes = LONG_SIZE;

				ASSERT(sDestReg.uArrayOffset < psVecArrayReg->uRegs);
			}
			else
			{
				SetFirstArrayElement(psVecArrayData, eFmt, &sDestReg);
				sDestReg.uNumber += uAddr;
			}
		}

		/* Transfer the data between array and register using either a move or a pack. */
		{
			/* 
			 * Main instruction (Even offset) 
			 */
			EmitLdStRegMove(psState, psCodeBlock, psLdStInst,
							eFmt, bLoad, IMG_FALSE,
							uRegChan, uPredReg,
							&sDestReg, &sDataReg);

			/* Test whether a second instruction is needed */
			if ((!bHasDynOffset) ||
				uChanPerStore == 1)
			{
				continue;
			}

			/* 
			 * Second instruction (Odd offset) 
			 */
			EmitLdStRegMove(psState, psCodeBlock, psLdStInst,
							eFmt, bLoad, IMG_TRUE,
							uRegChan, uPredReg,
							&sDestReg, &sDataReg);
		}
	}

	/* Signal change */
	return IMG_TRUE;
}

static
IMG_VOID GetLocalAddressingMemoryOffset(PINTERMEDIATE_STATE			psState,
										PCODEBLOCK					psCodeBlock,
										PINST						psInsertPoint,
										PARG						psOffsetArg,
										IMG_UINT32					uBaseAddr,
										IMG_BOOL					bHasDynOffset,
										IMG_UINT32					uDynOffsetReg,
										IMG_UINT32					uCompDataSize)
/*****************************************************************************
 FUNCTION	: GetLocalAddressingMemoryOffset

 PURPOSE	: Generate instructions to set a register to the offset parameter
			  to a load/store instruction using local addressing.

 PARAMETERS	: psState		- Compiler state.
			  psCodeBlock	- Block to insert the code into.
			  psInsertPoint	- Point to insert the instruction before.
			  psOffsetArg	- Set up with the register containing the offset.
			  uBaseAddr		- Static offset (in bytes)
			  bHasDynOffset	- TRUE if the offset should include a dynamic part.
			  uDynOffsetReg	- Register number that contains the dynamic part of
							the offset.
			  uCompDataSize	- Scaling factor for the dynamic offset.

 RETURNS	: Nothing
*****************************************************************************/
{
	IMG_UINT32	uOffset;

	/* Store the address to read/write */
	uOffset = uBaseAddr;
	/* Handle strides */
	if ((psState->uCompilerFlags & UF_FIXEDTEMPARRAYSTRIDE) == 0)
	{
		uOffset |= (psState->uIndexableTempArraySize << 16);
	}
	else
	{
		uOffset |= (1U << 16);
	}

	/*
		Create an argument representing the static offset into memory.
	*/
	InitInstArg(psOffsetArg);
	psOffsetArg->uType = USEASM_REGTYPE_IMMEDIATE;
	psOffsetArg->uNumber = uOffset;

	/* 
	   2) Add the dynamic address offset, scaling to take account of the data size. 

	   imae r0, r1, CompDataSize, r0
	   where r1 is the register with the dynamic offset
	*/
	if (bHasDynOffset)
	{
		PINST		psAddInst;
		IMG_UINT32	uDynAddrReg = psState->uNumRegisters++;

		psAddInst = AllocateInst(psState, psInsertPoint);
		
		SetOpcode(psState, psAddInst, IIMAE);
		psAddInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psAddInst->asDest[0].uNumber = uDynAddrReg;

		psAddInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
		psAddInst->asArg[0].uNumber = uDynOffsetReg;

		psAddInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psAddInst->asArg[1].uNumber = uCompDataSize;

		psAddInst->asArg[2] = *psOffsetArg;

		InsertInstBefore(psState, psCodeBlock, psAddInst, psInsertPoint);

		psOffsetArg->uType = USEASM_REGTYPE_TEMP;
		psOffsetArg->uNumber = uDynAddrReg;
	}
}

static
IMG_BOOL ExpandLoadStoreInst(PINTERMEDIATE_STATE psState,
							 PCODEBLOCK psCodeBlock,
							 PINST psLdStInst)
/*****************************************************************************
 FUNCTION	: ExpandLoadStoreInst

 PURPOSE	: Expand a load or store instruction 
 
 PARAMETERS	: psState		- Compiler state.
			  psCodeBlock	- Block to insert the code into.
			  psInst		- Instruction to expand

 RETURNS	: IMG_FALSE on no change, IMG_TRUE if instruction was expanded
*****************************************************************************/
{
	const IMG_UINT32 uFirstStSrc = LDSTARR_STORE_DATA_ARGSTART;		// Index of first data source in a store instruction
	IMG_UINT32 i;
	UF_REGFORMAT eFmt;
	IMG_UINT32 uArrayNum;
	PINST psInst, psInsertPoint;
	PINST psLoadInst;
	IMG_UINT32 uCompDataSize = 0;		// Size of each component (in bytes).
	IMG_UINT32 uBaseAddr;				// Base address of the array to read/write
	IMG_UINT32 uStaticIdx;					// Register number in the array
	IMG_UINT32 uRegOffset;				// Offset relative to the base address
	IMG_UINT32 uDynOffsetReg = 0;	// Number of the register with the dynamic offset
	IMG_UINT32 uChanOffset;			// Offset (in registers) from channel 0 to required channel
	IMG_UINT32 uChansPerDataReg;	// Number of channels in the data format
	IMG_UINT32 uSrcChan;
	IMG_BOOL bHasDynOffset;
	IMG_BOOL bDynArray;             // Some access is dynamic
	ARG sDataReg;					// Register for value read from/written to memory
	ARG sTempReg;
	PARG psValReg = NULL;
	IMG_UINT32 uChanMask;			// Mask of channels to read/write
	IMG_BOOL bLoad;
	IMG_UINT32 uFirstChan;


	if (psLdStInst->eOpcode >= ILDARRF32 && 
		psLdStInst->eOpcode <= ILDARRC10)
	{
		bLoad = IMG_TRUE;
	}
	else if (psLdStInst->eOpcode >= ISTARRF32 &&
			 psLdStInst->eOpcode <= ISTARRC10)
	{
		bLoad = IMG_FALSE;
	}
	else
	{
		/* Not a load/store instruction so nothing to do*/
		return IMG_FALSE;
	}

	/* Get the format from the instruction */
	eFmt = psLdStInst->u.psLdStArray->eFmt;

	/* Get the array from the instruction */
	uArrayNum = psLdStInst->u.psLdStArray->uArrayNum;

	/* Get the register offset from the instruction */
	uStaticIdx = psLdStInst->u.psLdStArray->uArrayOffset;

	/* Get the channel mask from the destination mask */
	uChanMask = psLdStInst->u.psLdStArray->uLdStMask;
	if (eFmt == UF_REGFORMAT_C10)
	{
		uChanMask = ConvertInputWriteMaskToIntermediate(psState, uChanMask);
	}

	/* Set the insertion point */
	psInsertPoint = psLdStInst;

	/* Test whether the array is in registers or in memory */
	if (psState->apsTempVecArray[uArrayNum] != NULL)
	{
		if (psState->apsTempVecArray[uArrayNum]->bInRegs)
		{
			/* Registers are in memory */
			return IMG_FALSE;
		}
	}

	/* 
	   Set the data size (in bytes) and channels per register.
	   (C10 is stored as F16 data.)
	*/
	if (psState->uCompilerFlags & UF_OPENCL)
	{
		if (eFmt == UF_REGFORMAT_F32 ||
			eFmt == UF_REGFORMAT_U32 ||
			eFmt == UF_REGFORMAT_I32)
		{
			uCompDataSize = LONG_SIZE;
		}
		else if (eFmt == UF_REGFORMAT_F16 ||
				 eFmt == UF_REGFORMAT_U16 ||
				 eFmt == UF_REGFORMAT_I16)
		{
			uCompDataSize = WORD_SIZE;
		}
		else if (eFmt == UF_REGFORMAT_U8_UN ||
				 eFmt == UF_REGFORMAT_I8_UN)
		{
			uCompDataSize = BYTE_SIZE;
		}
		else
		{
			/* Only f/u/i32, f/u/i16 and u/i8 formats are supported */
			imgabort();
		}

		uChansPerDataReg = 1;
	}
	else if (eFmt == UF_REGFORMAT_F32)
	{
		uChansPerDataReg = 1;
		uCompDataSize = LONG_SIZE;
	}
	else if (eFmt == UF_REGFORMAT_F16)
	{
		uChansPerDataReg = 2;
		uCompDataSize = WORD_SIZE;
	}
	else if (eFmt == UF_REGFORMAT_C10)
	{
		uChansPerDataReg = 4;
		uCompDataSize = WORD_SIZE;
	}
	else
	{
		/* Only f32, f16, c10 and u8 formats are supported */
		imgabort();
	}

	/* Find the first channel */
	ASSERT(uChanMask != 0);
	for (uSrcChan = 0; uSrcChan < CHANNELS_PER_INPUT_REGISTER; uSrcChan ++)
	{ 
		if ((uChanMask & (1U << uSrcChan)) != 0)
		{
			/* Found the first channel */
			break;
		}
	}
	if (uSrcChan >= CHANNELS_PER_INPUT_REGISTER)
	{
		/* Got an empty mask */
		ASSERT(uSrcChan < CHANNELS_PER_INPUT_REGISTER);
		return IMG_FALSE;
	}

	/* 
	   Find the array this temp is in and its base address
	*/
	uBaseAddr = 0;
	{
		PUSC_VEC_ARRAY_DATA psVecData;  // Data for this array

		for (i = 0; i < uArrayNum; i++)
		{
			psVecData = psState->apsTempVecArray[i];
			/* If this array is used and not in temporaries: add it to the base address */
			if (psVecData == NULL)
			{
				continue;
			}
			if (psVecData->uLoads > 0 && !psVecData->bInRegs)
			{
				uBaseAddr += psVecData->uSize;
			}
		}
		ASSERT(i < psState->uIndexableTempArrayCount);		
		psVecData = psState->apsTempVecArray[i];

		/* Test for a write to an array that is never read */
		if (psVecData == NULL)
		{
			/* Load is never used */
			return IMG_TRUE;
		}

		/* Record whether access is static */
		bDynArray = (!psVecData->bStatic) ? IMG_TRUE : IMG_FALSE;
	}

	/* 
	   Calculate the offset in bytes of the register to be read/written
	   and the active channel in the register.

	   RegAddr = uArrayBase + uRegOffset + uChanOffset
	   where
	     uRegOffset = (uStaticIdx * CHANNELS_PER_INPUT_REGISTER) * uCompDataSize
		 uChanOffset = (uSrcChan / uChansPerReg) * uCompDataSize
	*/
	/* Work out the offset from the base to the register storing the first specified component */
	uRegOffset = (uStaticIdx * CHANNELS_PER_INPUT_REGISTER) + uSrcChan;
	/* Calculate the address of the required channel */
	uBaseAddr += (uRegOffset * uCompDataSize);

	/* Get the dynamic offset register, if any */
	if (!bDynArray)
	{
        /* All accesses to array elements are static */
		bHasDynOffset = IMG_FALSE;
	}
	else if (psLdStInst->asArg[LDSTARR_DYNAMIC_OFFSET_ARGINDEX].uType != USEASM_REGTYPE_IMMEDIATE)
	{
		bHasDynOffset = IMG_TRUE;
		uDynOffsetReg = psLdStInst->asArg[LDSTARR_DYNAMIC_OFFSET_ARGINDEX].uNumber;
	}
	else
	{
		bHasDynOffset = IMG_FALSE;
	}

	/* Get temporary registers for the data */
	if (bLoad)
	{
		sDataReg = psLdStInst->asDest[0];
	}
	else
	{
		/* For a store, the data register is the first source operand */
		sDataReg = psLdStInst->asArg[uFirstStSrc];
	}
	/* 
	   For non-f32, load/store to a temporary variable
	*/
	if (eFmt != UF_REGFORMAT_F32)
	{
		InitInstArg(&sTempReg);
		sTempReg.uType = USEASM_REGTYPE_TEMP;
		sTempReg.uNumber = psState->uNumRegisters++;
		sTempReg.eFmt = eFmt;
	}
	else
	{
		sTempReg = sDataReg;
	}

	/* Set-up the strides */
	if (psState->uCompilerFlags & UF_FIXEDTEMPARRAYSTRIDE)
	{
		PINST psAddInst, psShiftInst, psOrInst;

		/*
		  Concatenate the instance number in the slot and
		  the instance number in the task.
		*/
		psShiftInst = AllocateInst(psState, psLdStInst);

		SetOpcode(psState, psShiftInst, ISHL);
		psShiftInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psShiftInst->asDest[0].uNumber = USC_TEMPREG_TEMPADDRESS;
		psShiftInst->asArg[BITWISE_SHIFTEND_SOURCE].uType = USEASM_REGTYPE_GLOBAL;
		psShiftInst->asArg[BITWISE_SHIFTEND_SOURCE].uNumber = EURASIA_USE_SPECIAL_BANK_INSTNRINTASK;
		psShiftInst->asArg[BITWISE_SHIFT_SOURCE].uType = USEASM_REGTYPE_IMMEDIATE;
		psShiftInst->asArg[BITWISE_SHIFT_SOURCE].uNumber = 2;
		InsertInstBefore(psState, psCodeBlock, psShiftInst, psInsertPoint);

		psOrInst = AllocateInst(psState, psLdStInst);

		SetOpcode(psState, psOrInst, IOR);
		psOrInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psOrInst->asDest[0].uNumber = USC_TEMPREG_TEMPADDRESS;
		psOrInst->asArg[0].uType = USEASM_REGTYPE_GLOBAL;
		psOrInst->asArg[0].uNumber = EURASIA_USE_SPECIAL_BANK_INSTNRINSLOT;
		psOrInst->asArg[1].uType = USEASM_REGTYPE_TEMP;
		psOrInst->asArg[1].uNumber = USC_TEMPREG_TEMPADDRESS;
		InsertInstBefore(psState, psCodeBlock, psOrInst, psInsertPoint);

		/*
		  Multiply the instance number by the instance stride.
		*/
		psShiftInst = AllocateInst(psState, psLdStInst);

		SetOpcode(psState, psShiftInst, ISHL);
		psShiftInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psShiftInst->asDest[0].uNumber = USC_TEMPREG_TEMPADDRESS;
		psShiftInst->asArg[BITWISE_SHIFTEND_SOURCE].uType = USEASM_REGTYPE_TEMP;
		psShiftInst->asArg[BITWISE_SHIFTEND_SOURCE].uNumber = USC_TEMPREG_TEMPADDRESS;
		psShiftInst->asArg[BITWISE_SHIFT_SOURCE].uType = USEASM_REGTYPE_IMMEDIATE;
		psShiftInst->asArg[BITWISE_SHIFT_SOURCE].uNumber = USC_FIXED_TEMP_ARRAY_SHIFT;
		InsertInstBefore(psState, psCodeBlock, psShiftInst, psInsertPoint);

		/*
		  Add the offset onto the base address of the array. The offset has the lower
		  16-bits clear so we can use a 16-bit add.
		*/
		psAddInst = AllocateInst(psState, psLdStInst);

		SetOpcode(psState, psAddInst, IIMA16);
		psAddInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psAddInst->asDest[0].uNumber = USC_TEMPREG_TEMPADDRESS;
		psAddInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
		psAddInst->asArg[0].uNumber = USC_TEMPREG_TEMPADDRESS;
		psAddInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psAddInst->asArg[1].uNumber = 1;
		psAddInst->asArg[2].uType = USEASM_REGTYPE_SECATTR;
		psAddInst->asArg[2].uNumber = psState->psSAOffsets->uIndexableTempBase;
		InsertInstBefore(psState, psCodeBlock, psShiftInst, psInsertPoint);
	}

	/* 	
	   Got the base address, now read/write each specified channel.
	*/
	uFirstChan = uSrcChan;
	for ( ; uSrcChan < CHANNELS_PER_INPUT_REGISTER; uSrcChan ++)
	{
		if ((uChanMask & (1U << uSrcChan)) == 0)
		{
			/* The component is not active */
			continue;
		}

		/* Calculate the channel offset */
		uChanOffset = uSrcChan / uChansPerDataReg;

		/* Set the data register. (Loads always use the destination.) */
		if (!bLoad)
		{
			/* For a store, set the value register to the register to read from */
			ASSERT((uFirstStSrc + uChanOffset) < psLdStInst->uArgumentCount);
			sDataReg = psLdStInst->asArg[uFirstStSrc + uChanOffset];
		}
	
		/* 
		   For a store with c10 or f16, put the data into the the lower 16 bits of the
		   temporary register
		*/
		if (bLoad)
		{
			/* Use a temporary if the component has to be moved from the low bits */
			if ((eFmt == UF_REGFORMAT_F16 && (uSrcChan == 1 || uSrcChan == 3)) ||
				(eFmt == UF_REGFORMAT_C10 && (uSrcChan != 0)))
			{
				psValReg = &sTempReg;
			}
			else
			{
				psValReg = &sDataReg;
			}
		}
		else
		{
			IMG_BOOL bUnPack = IMG_FALSE;
			IOPCODE eOpcode = IINVALID;
			IMG_UINT32 uReadChan = uSrcChan % uChansPerDataReg;
			IMG_UINT32 uPackMask = USC_UNDEF;

			if (eFmt == UF_REGFORMAT_C10)
			{
				/* 
				   Shift the component to the lower 10 bits.
				*/
				bUnPack = IMG_TRUE;
				eOpcode = IPCKC10C10;
				uReadChan = ConvertInputChannelToIntermediate(psState, uReadChan);
				uPackMask = USC_X_CHAN_MASK;
			}
			else if (eFmt == UF_REGFORMAT_F16 &&
					 (uSrcChan == 1 || uSrcChan == 3))		// F16 component in upper 16 bits
			{
				/* 
				   Shift the component to the lower 16 bits
				   pckf16f16 uValReg.1100, uDataReg, #0
				*/
				bUnPack = IMG_TRUE;
				#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
    			if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
				{
					eOpcode = IUNPCKU16U16;
				}
				else
    			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
				{
					eOpcode = IPCKF16F16;
				}
				uReadChan = uReadChan * 2;	// Scale up for f16 channels 
				uPackMask = USC_XY_CHAN_MASK;
			}
			else if (eFmt == UF_REGFORMAT_F32)
			{
				sTempReg = sDataReg;
			}

			/* Emit the instruction */
			if(bUnPack)
			{
				psInst = AllocateInst(psState, psLdStInst);
				
				SetOpcode(psState, psInst, eOpcode);
		
				psInst->asDest[0] = sTempReg;
				psInst->auDestMask[0] = uPackMask;

				psInst->asArg[0] = sDataReg;
				SetPCKComponent(psState, psInst, 0 /* uArgIdx */, uReadChan);

				psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
				psInst->asArg[1].uNumber = 0;

				InsertInstBefore(psState, psCodeBlock, psInst, psInsertPoint);

				psValReg = &sTempReg;
			}
			else
			{
				psValReg = &sDataReg;
			}
		}


		/*
		  Do the load/store.

		  Emit  ld r0, base, r0
		  or st r0, base, r0, StoreSrc
		  where
		  base is the base address of the register banks
		  StoreSrc is the data to be stored (a temporary register)
		*/
		psLoadInst = AllocateInst(psState, psLdStInst);
		
		if (psState->uCompilerFlags & UF_FIXEDTEMPARRAYSTRIDE)
		{
			IOPCODE eOpcode = IINVALID;
			switch (eFmt)
			{
				case UF_REGFORMAT_U8_UN:
					eOpcode = bLoad ? ILDAB : ISTAB;
					break;
				case UF_REGFORMAT_U16:
				case UF_REGFORMAT_F16:
				case UF_REGFORMAT_C10:
					eOpcode = bLoad ? ILDAW : ISTAW; 
					break;
				case UF_REGFORMAT_F32:
					eOpcode = bLoad ? ILDAD : ISTAD; 
					break;
				default:
					imgabort();
			}
			SetOpcode(psState, psLoadInst, eOpcode); 	
		}
		else
		{
			IOPCODE eOpcode = IINVALID;
			switch (eFmt)
			{
				case UF_REGFORMAT_I8_UN:
				case UF_REGFORMAT_U8_UN:
					eOpcode = bLoad ? ILDLB : ISTLB;
					break;
				case UF_REGFORMAT_I16:
				case UF_REGFORMAT_U16:
				case UF_REGFORMAT_F16:
				case UF_REGFORMAT_C10:
					eOpcode = bLoad ? ILDLW : ISTLW; 
					break;
				case UF_REGFORMAT_I32:
				case UF_REGFORMAT_U32:
				case UF_REGFORMAT_F32:
					eOpcode = bLoad ? ILDLD : ISTLD;
					break;
				default:
					imgabort();
			}
			SetOpcode(psState, psLoadInst, eOpcode);
		}
		psState->uOptimizationHint |= USC_COMPILE_HINT_LOCAL_LOADSTORE_USED;
		psLoadInst->u.psLdSt->bBypassCache = BypassCacheForModifiableShaderMemory(psState);
		if (bLoad)
		{
			psLoadInst->asDest[0] = *psValReg;
		}
		else
		{
			psLoadInst->asDest[0].uType = USC_REGTYPE_DUMMY;
			psLoadInst->asDest[0].uNumber = 0;
		}
		if (psState->uCompilerFlags & UF_FIXEDTEMPARRAYSTRIDE)
		{
			psLoadInst->asArg[MEMLOADSTORE_BASE_ARGINDEX].uType = USEASM_REGTYPE_TEMP;
			psLoadInst->asArg[MEMLOADSTORE_BASE_ARGINDEX].uNumber = USC_TEMPREG_TEMPADDRESS;
		}
		else
		{
			psLoadInst->asArg[MEMLOADSTORE_BASE_ARGINDEX].uType = USEASM_REGTYPE_SECATTR;
			psLoadInst->asArg[MEMLOADSTORE_BASE_ARGINDEX].uNumber = psState->psSAOffsets->uIndexableTempBase;
		}
		/*
			Get the value for the offset (SRC1) parameter to the load/store instruction.
		*/
		GetLocalAddressingMemoryOffset(psState,
									   psCodeBlock,
									   psInsertPoint,
									   &psLoadInst->asArg[MEMLOADSTORE_OFFSET_ARGINDEX],
									   uBaseAddr + (uSrcChan - uFirstChan) * uCompDataSize,
									   bHasDynOffset,
									   uDynOffsetReg,
									   uCompDataSize * psLdStInst->u.psLdStArray->uRelativeStrideInComponents);
		if (!bLoad)
		{
			psLoadInst->asArg[MEMSTORE_DATA_ARGINDEX] = *psValReg;
		}
		else
		{
			psLoadInst->asArg[MEMLOAD_RANGE_ARGINDEX].uType = USEASM_REGTYPE_IMMEDIATE;
			psLoadInst->asArg[MEMLOAD_RANGE_ARGINDEX].uNumber = 0;
			psLoadInst->asArg[MEMLOAD_DRC_ARGINDEX].uType = USEASM_REGTYPE_DRC;
			psLoadInst->asArg[MEMLOAD_DRC_ARGINDEX].uNumber = 0;
		}
		InsertInstBefore(psState, psCodeBlock, psLoadInst, psInsertPoint);

		/* 
		   For c10 or f16 load, move the data into the correct component
		*/
		if (bLoad)
		{
			IMG_BOOL bPack = IMG_FALSE;
			IMG_UINT32 uDestMask = 0;
			IOPCODE eOpcode = IINVALID;

			if(eFmt == UF_REGFORMAT_C10)
			{
				/* 
				   Pack the component from the low 10 bits of the
				   load result into the expected channel.
				*/
				bPack = IMG_TRUE;
				eOpcode = IPCKC10C10;
				uDestMask = ConvertInputWriteMaskToIntermediate(psState, 1U << uSrcChan);
			}
			else if (eFmt == UF_REGFORMAT_F16 &&
					 (uSrcChan == 1 || uSrcChan == 3))
			{
				/* Put the component in the channel expected by the conversion functions */
				bPack = IMG_TRUE;

				#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
    			if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
				{
					eOpcode = IUNPCKU16U16;
				}
				else
    			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
				{
					eOpcode = IPCKF16F16;
				}
				uDestMask = (1U << 3) | (1U << 2);
			}

			/* Emit the pack */
			if (bPack)
			{
				psInst = AllocateInst(psState, psLdStInst);
				
				SetOpcode(psState, psInst, eOpcode);
		
				psInst->asDest[0] = sDataReg;
				psInst->asDest[0].eFmt = eFmt;
				psInst->auDestMask[0] = uDestMask;
				if (uDestMask != USC_ALL_CHAN_MASK)
				{
					SetPartiallyWrittenDest(psState, psInst, 0 /* uDestIdx */, &sDataReg);
				}

				psInst->asArg[0] = *psValReg;
				psInst->asArg[0].eFmt = eFmt;

				psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
				psInst->asArg[1].uNumber = 0;

				InsertInstBefore(psState, psCodeBlock, psInst, psInsertPoint);
			}
		}
	}

	/* Signal change */
	return IMG_TRUE;
}

static IMG_VOID ExpandMemLoadStoreBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNull)
/*****************************************************************************
 FUNCTION	: ExpandMemLoadStoreBP

 PURPOSE	: BLOCK_PROC to expand load or store instructions in a block
			  (except, does not expand reg arrays)

 PARAMETERS	: psState	- Compiler state.
			  psBlock	- Block within which to expand instructions
			  pvNull	- IMG_PVOID to fit callback signature; unused

 RETURNS	: None
*****************************************************************************/
{
	PINST psInst, psNextInst;
	
	PVR_UNREFERENCED_PARAMETER(pvNull);
	
	for (psInst = psBlock->psBody; psInst; psInst = psNextInst)
	{
		psNextInst = psInst->psNext;
		
		if (psInst->eOpcode >= ILDARRF32 &&
			psInst->eOpcode <= ISTARRC10)
		{
			if(ExpandLoadStoreInst(psState, psBlock, psInst))
			{
				RemoveInst(psState, psBlock, psInst);
				FreeInst(psState, psInst);
			}
		}
	}
}

static IMG_VOID ExpandLoadStoreBP(PINTERMEDIATE_STATE psState,
							  PCODEBLOCK psBlock,
							  IMG_PVOID pvNull)
/*****************************************************************************
 FUNCTION	: ExpandLoadStoreBP

 PURPOSE	: BLOCK_PROC to expand load or store instructions in a block
			  (expands both memory and register arrays)

 PARAMETERS	: psState	- Compiler state.
			  psBlock	- Block within which to expand instructions
			  pvNull	- IMG_PVOID to fit callback signature; unused

 RETURNS	: None
*****************************************************************************/
{
	PINST psInst, psNextInst;
	IMG_BOOL bBlockChange = IMG_FALSE;
	PVR_UNREFERENCED_PARAMETER(pvNull);
	
	for (psInst = psBlock->psBody; psInst; psInst = psNextInst)
	{
		psNextInst = psInst->psNext;
		
		if (!(psInst->eOpcode >= ILDARRF32 && psInst->eOpcode <= ISTARRC10))
			continue;

		if (ExpandLoadStoreInst(psState, psBlock, psInst) ||
			/* 
			   No change but instruction is an array load/store, try
			   expanding for register arrays.
			*/
			ExpandRegLdStInst(psState, psBlock, psInst))
		{
			/* Remove original instruction */
			RemoveInst(psState, psBlock, psInst);
			FreeInst(psState, psInst);

			bBlockChange = IMG_TRUE;
		}
	}
	if (bBlockChange)
	{
		PeepholeOptimizeBlock(psState, psBlock);
	}
}

static IMG_VOID EliminateNewMovesBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNull)
/******************************************************************************
 FUNCTION		: EliminateNewMovesBP
 
 DESCRIPTION	: Eliminate moves only from blocks with the NEED_GLOBAL_MOVE_ELIM
				  flag set. (Set by IntegerOptimize on blocks which it thinks
				  might need global, rather than local, move elimination - see
				  CompileIntermediate).

 PARAMETERS		: psState	- Compiler intermediate state
				  psBlock	- block from which to eliminate moves IFF flag set
				  pvNull	- IMG_PVOID to fit callback signature; unused

 RETURNS		: None
******************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(pvNull);
	if (psBlock->uFlags & NEED_GLOBAL_MOVE_ELIM)
	{
		EliminateMovesBP(psState, psBlock);
		psBlock->uFlags &= ~NEED_GLOBAL_MOVE_ELIM;
	}
}

IMG_INTERNAL 
PDGRAPH_STATE ComputeBlockDependencyGraph(  PINTERMEDIATE_STATE psState,
											PCODEBLOCK psBlock, 
											IMG_BOOL bIgnoreDesched)
/*********************************************************************************
 Function			: ComputeBlockDependencyGraph

 Description		: Calculate a dependency graph for a block.
 
 Parameters			: psState		Compile state
					  psBlock		To compute dependency graph for
					  bIgnoreDesched

 Globals Effected	: None

 Return				: Dependency graph
*********************************************************************************/
{
	ASSERT(psBlock->psDepState == NULL);

	if( ((psBlock->uFlags & USC_CODEBLOCK_NEED_DEP_RECALC) == 0) &&
		bIgnoreDesched == psBlock->bIgnoreDesched &&
	    psBlock->psSavedDepState)
	{
		/* Reuse saved dep graph state */
		psBlock->psDepState = psBlock->psSavedDepState;
		psBlock->psSavedDepState = IMG_NULL;
		#if defined(DEBUG)
		{
			/* Verifing cached revision for correctness */
			PDGRAPH_STATE psTempGraph;
			
			psTempGraph = ComputeDependencyGraph(psState, psBlock, bIgnoreDesched);
			
			/* 
				If this assert failed, USC_CODEBLOCK_NEED_DEP_RECALC should be cleared before 
				Graph recompution, as cached revision is invalid.
			*/
			ASSERT(GraphMatch(psState, psTempGraph->psDepGraph, psBlock->psDepState->psDepGraph));
			
			if(psTempGraph)
			{
				/* Free temporary allocated graph */
				FreeDGraphState(psState, &psTempGraph);
			}
		}
		#endif
		return psBlock->psDepState;
	}
	else
	{
		/* Scrap saved dependency graph */
		if(psBlock->psSavedDepState)
		{
			FreeDGraphState(psState, &psBlock->psSavedDepState);
		}
	}
	
	ASSERT(psBlock->psSavedDepState == IMG_NULL);
	
	psBlock->uFlags &= ~USC_CODEBLOCK_NEED_DEP_RECALC;
	
	psBlock->bIgnoreDesched	= bIgnoreDesched;
	psBlock->psDepState		= ComputeDependencyGraph(psState, psBlock, bIgnoreDesched);
	
	return psBlock->psDepState;
}

IMG_INTERNAL
IMG_VOID FreeBlockDGraphState(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
{
	PVR_UNREFERENCED_PARAMETER(psState);
	
	psBlock->psSavedDepState = psBlock->psDepState;
	psBlock->psDepState = IMG_NULL;
}

IMG_INTERNAL IMG_VOID CompileIntermediate(PINTERMEDIATE_STATE psState)
/*********************************************************************************
 FUNCTION			: CompileIntermediate

 DESCRIPTION		: Compiles the intermediate code (i.e. after conversion from
					  uniflex input code or GPGPU). to make it ready for output
					  generation. Encompasses rest of the optimizations not specific
					  to an input format, inc. register allocation & finalization.
 
 PARAMETERS			: psState - Compiler state, *containing intermediate code*

 RETURNS			: Nothing
*********************************************************************************/
{
	ASSERT (psState->bExceptionReturnValid);

#ifdef DEBUG_TIME
	USC_CLOCK_SET(psState->sTimeStart);
#endif

#if defined(DEBUG)
	ASSERT(CheckProgram(psState));
#endif

#if 0 
    /* Local Constant Propagation disabled for debugging */
	{
		DBG_PRINTF((DBG_TEST_DATA, __SECTION_TITLE__ "Local Constant Propagation" __SECTION_TITLE__));
		LocalConstProp(psState);
		TESTONLY(PrintIntermediate(psState, "local_const_prop", IMG_FALSE, IMG_FALSE));
	}
#endif

	if(psState->uFlags & USC_FLAGS_INDEXABLETEMPS_USED)
	{
		/*
		* Expand array load/store instructions for array in memory.
		*/
		DBG_PRINTF((DBG_TEST_DATA, __SECTION_TITLE__ "Expand Array Load/Store (Memory)" __SECTION_TITLE__));
		SetupLoadStoreArray(psState);
		DoOnAllBasicBlocks(psState, ANY_ORDER, ExpandMemLoadStoreBP, IMG_FALSE, NULL);
		TESTONLY(PrintIntermediate(psState, "expand_mem", IMG_TRUE));
	
		/* 
		* Expand array load/store instructions with dynamic access to registers
		* (must be done before assining index registers)
		*/
		DBG_PRINTF((DBG_TEST_DATA, __SECTION_TITLE__ "Expand Array Load/Store (Registers)" __SECTION_TITLE__));
		DoOnAllBasicBlocks(psState, ANY_ORDER, ExpandLoadStoreBP, IMG_FALSE, NULL);
		TESTONLY(PrintIntermediate(psState, "expand_reg", IMG_TRUE));
	}

	DBG_PRINTF((DBG_TEST_DATA, __SECTION_TITLE__ "Dead code removal" __SECTION_TITLE__));
	DeadCodeElimination(psState, IMG_TRUE);
	TESTONLY(PrintIntermediate(psState, "dce_prepack", IMG_FALSE));

	METRICS_START(psState, VALUE_NUMBERING);
	ValueNumbering(psState);
	METRICS_FINISH(psState, VALUE_NUMBERING);

	METRICS_START(psState,  FLATTEN_CONDITIONALS_AND_ELIMINATE_MOVES);
	DBG_PRINTF((DBG_TEST_DATA, __SECTION_TITLE__ "Eliminate moves" __SECTION_TITLE__));
	EliminateMovesAndFuncs(psState);
	ArithmeticSimplification(psState);
	TESTONLY(PrintIntermediate(psState, "elim_mov", IMG_TRUE));

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		OptimiseConstLoads(psState);
	}
	#endif //defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	
	FinaliseTextureSamples(psState);
	TESTONLY_PRINT_INTERMEDIATE("Finalise Texture Samples", "fin_smp", IMG_FALSE);

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		SimplifySwizzlesOnConsts(psState);
		if (FoldConstants(psState))
		{
			DBG_PRINTF((DBG_TEST_DATA, __SECTION_TITLE__ "Eliminate moves" __SECTION_TITLE__));
			EliminateMoves(psState);
			TESTONLY(PrintIntermediate(psState, "elim_mov_after_fold_consts", IMG_TRUE));

			DBG_PRINTF((DBG_TEST_DATA, __SECTION_TITLE__ "Dead code removal" __SECTION_TITLE__));
			DeadCodeElimination(psState, IMG_TRUE);
			TESTONLY(PrintIntermediate(psState, "dce_prepack_after_fold_consts", IMG_FALSE));
		}
	}
	#endif //defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)

	DBG_PRINTF((DBG_TEST_DATA, __SECTION_TITLE__ "Constant Register Packing" __SECTION_TITLE__));
	PackConstantRegisters(psState);
	TESTONLY(PrintIntermediate(psState, "const_pack", IMG_FALSE));
	
	#if defined(OUTPUT_USPBIN)
	if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
	{
		if (!psState->bInvariantShader)
		{
			if(psState->uOptimizationHint & USC_COMPILE_HINT_OPTIMISE_USP_SMP)
			{
				DBG_PRINTF((DBG_TEST_DATA, __SECTION_TITLE__ "USP-sample optimisation" __SECTION_TITLE__));
				DoOnAllBasicBlocks(psState, ANY_ORDER, OptimiseUSPSamplesBP, IMG_FALSE, NULL);
				TESTONLY(PrintIntermediate(psState, "usp_sample", IMG_FALSE));
			}
		}
	}
	#endif /* defined(OUTPUT_USPBIN) */
	#if defined(OUTPUT_USPBIN) && defined(OUTPUT_USCHW)
	else
	#endif /* defined(OUTPUT_USPBIN) && defined(OUTPUT_USCHW) */
	#if defined(OUTPUT_USCHW)
	{
		if (!psState->bInvariantShader &&
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_TAG_UNPACK_RESULT))
		{
			ReduceSampleResultPrecision(psState);
			
			TESTONLY_PRINT_INTERMEDIATE("Reduce TAG unpack precision", "reduce_prec", IMG_TRUE);
		}
	}
	#endif /* defined(OUTPUT_USCHW) */

    if(psState->uOptimizationLevel > 0)
    {
    	DoOnAllBasicBlocks(psState, ANY_ORDER, EliminateF16MovesBP, IMG_FALSE, NULL);
		ArithmeticSimplification(psState);
		
    	TESTONLY_PRINT_INTERMEDIATE("Eliminate F16 moves", "elim_f16mov", IMG_TRUE);
    }
	METRICS_FINISH(psState, FLATTEN_CONDITIONALS_AND_ELIMINATE_MOVES);

	METRICS_START(psState, INTEGER_OPTIMIZATIONS);
	if (psState->uFlags & USC_FLAGS_INTEGERUSED)
	{
		if ((psState->uFlags & USC_FLAGS_INTEGERINLASTBLOCKONLY) == 0)
		{
			DoOnAllBasicBlocks(psState, ANY_ORDER, IntegerOptimizeBP, IMG_FALSE, NULL);
			/*
				Perform a second whole-program pass of move elimination,
				targetted only to blocks containing new moves which might be
				eliminated by *global* move elimination. (We might be able to
				eliminate more of these moves now, than when we were performing
				move elimination partway through integer optimization. (?!?!))
			*/
			DoOnAllBasicBlocks(psState, ANY_ORDER, EliminateNewMovesBP, IMG_FALSE, NULL);
		}
		else
		{
			IntegerOptimizeBP(psState, psState->psMainProg->sCfg.psExit, NULL);
			/*
				no point in doing extra move elim now, as nothing has changed
				globally (outside that block)
			*/
			psState->psMainProg->sCfg.psExit->uFlags &= ~NEED_GLOBAL_MOVE_ELIM;
		}
	}
	METRICS_FINISH(psState, INTEGER_OPTIMIZATIONS);

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		/*
			Normalise vector variables so the channels used in the vector
			start from X.
		*/
		NormaliseVectorLengths(psState);
		
		/*
			Replace scalar MOV instructions with vector VMOVs to allow more optimisations.
		*/
		ReplaceMOVsWithVMOVs(psState);

		/*
			Combine two VMOV instructions into one.
		*/
		if (CombineVMOVs(psState))
		{
			EliminateMoves(psState);
		}
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	ConvertPredicatedMovesToMovc(psState);

    if (psState->uOptimizationLevel > 0)
    {
	    METRICS_START(psState, MERGE_IDENTICAL_INSTRUCTIONS);
	    DoOnAllBasicBlocks(psState, ANY_ORDER, MergeIdenticalInstructionsBP, IMG_FALSE, NULL);
    	TESTONLY(PrintIntermediate(psState, "identical_instrs", IMG_FALSE));
    }

	#if defined(OUTPUT_USPBIN)
	if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
	{
		if(psState->uOptimizationHint & USC_COMPILE_HINT_OPTIMISE_USP_SMP)
		{
			DBG_PRINTF((DBG_TEST_DATA, __SECTION_TITLE__ "USP-sample optimisation" __SECTION_TITLE__));
			DoOnAllBasicBlocks(psState, ANY_ORDER, OptimiseUSPSamplesBP, IMG_FALSE, NULL);
			TESTONLY(PrintIntermediate(psState, "usp_sample2", IMG_FALSE));
		}
	}
	#endif /* defined(OUTPUT_USPBIN) */

	/*
		Remove redundant MIN and MAX instructions.
	*/
	if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL)
	{
		DBG_PRINTF((DBG_TEST_DATA, __SECTION_TITLE__ "Remove Unnecessary Saturations" __SECTION_TITLE__));
		DoOnAllBasicBlocks(psState, ANY_ORDER, RemoveUnnecessarySaturationsBP, IMG_FALSE, NULL);
		TESTONLY(PrintIntermediate(psState, "remove_sat", IMG_TRUE));
	}

	if (psState->uCompilerFlags & UF_EXTRACTCONSTANTCALCS)
	{
    	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		RegroupVMULComputations(psState);
		TESTONLY_PRINT_INTERMEDIATE("Regroup VMUL Computations", "regroup_VMUL", IMG_FALSE);
    	#endif //defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)

		DBG_PRINTF((DBG_TEST_DATA, __SECTION_TITLE__ "Extract constant calculations" __SECTION_TITLE__));
		ExtractConstantCalculations(psState);
		TESTONLY(PrintIntermediate(psState, "extract_const", IMG_FALSE));
	}
	
	METRICS_FINISH(psState, MERGE_IDENTICAL_INSTRUCTIONS);

   	METRICS_START(psState, INSTRUCTION_SELECTION);
	
	if (FlattenProgramConditionals(psState))
	{
		UseDefDropUnusedTemps(psState);

		EliminateMoves(psState);

		MergeAllBasicBlocks(psState);

		TESTONLY_PRINT_INTERMEDIATE("Flatten Conditionals", "flat_cond", IMG_TRUE);
	}

	OptimizePredicateCombines(psState);
	
	RemovePredicates(psState);

	EliminateMoves(psState);

    if (psState->uOptimizationLevel > 0)
    {
    	DoOnAllBasicBlocks(psState, ANY_ORDER, GenerateNonEfoInstructionsBP, IMG_FALSE, NULL);
		MergeAllBasicBlocks(psState);
    
    	TESTONLY_PRINT_INTERMEDIATE("Instruction selection (Other)", "isel_other", IMG_TRUE);
    
    	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
    	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) == 0)
    	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
    	{
    		DoOnAllBasicBlocks(psState, ANY_ORDER, GenerateEfoInstructionsBP, IMG_FALSE, NULL);
    		TESTONLY_PRINT_INTERMEDIATE("Instruction selection (EFO)", "isel_efo", IMG_TRUE);
    	}
    }
	
	ExpandConditionalTextureSamples(psState);
	TESTONLY_PRINT_INTERMEDIATE("Expand Conditional Texture Samples", "fin_smp", IMG_FALSE);
	
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	ReplaceUnusedArguments(psState);

	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		DoOnAllBasicBlocks(psState, ANY_ORDER, MergeIdenticalInstructionsBP, IMG_FALSE, NULL);
		TESTONLY_PRINT_INTERMEDIATE("Merge Identical Instructions", "merge_ident", IMG_FALSE);

		CombineChannels(psState);

		CombineDependantIdenticalInsts(psState);

		/* 
			Check for cases where a VADD followed by a VMOV which makes a
		    new vector from the VADD result and one of the VADD sources can
		    be optimized.
		*/
		CombineVecAddsMoves(psState);
		
		/*
			Combine instructions on different elements of the same vector register.
		*/
		Vectorise(psState);

		/*
			Where possible represent truth values in temporary registers as bytes
			rather than floats.
		*/
		ReducePredicateValuesBitWidth(psState);
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	ScheduleForPredRegPressure(psState);

	/*
		Expand some instructions which aren't supported directly by the hardware but which were kept
		in a compact form to make it easiest to apply optimizations involving them.
	*/
	ExpandUnsupportedInstructions(psState);
	
	/*
		Reorder instructions to avoid stalls waiting for data from memory.
	*/
    if (psState->uOptimizationLevel > 0)
    {
		BuildFetchInstructions(psState);
    	TESTONLY_PRINT_INTERMEDIATE("Build Fetch Instructions", "build_fetch", IMG_TRUE);
    }
	if (psState->uOptimizationLevel > 0)
    {
		DoOnAllBasicBlocks(psState, ANY_ORDER, ReorderHighLatncyInstsBP, IMG_FALSE, NULL);
    	TESTONLY_PRINT_INTERMEDIATE("High Latency Instruction reordering", "reorder", IMG_TRUE);
	}

	/*
		Generate final hardware instructions for loads from memory.
	*/
	FinaliseMemoryLoads(psState);
	TESTONLY_PRINT_INTERMEDIATE("Finalise memory loads", "fin_memload", IMG_TRUE);

	/*
		Optimise and finalise memory store instructions.
	*/
	if ((psState->uOptimizationLevel > 0) && (psState->uCompilerFlags & UF_OPENCL))
	{
		DoOnAllBasicBlocks(psState, ANY_ORDER, FinaliseMemoryStoresBP, IMG_FALSE, NULL);
		TESTONLY_PRINT_INTERMEDIATE("Finalise memory stores", "fin_memstore", IMG_TRUE);
	}

	if (psState->uCompilerFlags & UF_SPLITFEEDBACK)
	{
		IMG_BOOL	bTrySplit;

		bTrySplit = IMG_FALSE;
		
		{
			if(psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL)
			{
				if (
						(psState->psSAOffsets->ePackDestFormat == UF_REGFORMAT_U8) &&
						(psState->uCompilerFlags & UF_MSAATRANS) == 0
				   )
				{
					bTrySplit = IMG_TRUE;
				}
			}
			#if defined(SUPPORT_SGX545)
			else if (
						(psState->psSAOffsets->eShaderType == USC_SHADERTYPE_VERTEX) &&
						(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_VCB) != 0
					)
			{
				bTrySplit = IMG_TRUE;
			}			
			#endif /* defined(SUPPORT_SGX545) */
		}

		if (bTrySplit)
		{
			SplitFeedback(psState);
			TESTONLY_PRINT_INTERMEDIATE("Split feedback", "split_feedback", IMG_FALSE);
		}
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		/*
			Fix vector instructions with unsupported source modifiers.
		*/
		FixVectorSourceModifiers(psState);

		/*	
			Fix instructions which use invalid source swizzles.
		*/
		FixVectorSwizzles(psState);

		/*
			Where vector instructions use large immediate values try to replace them
			by hardware constants.
		*/
		ReplaceImmediatesByVecConstants(psState);

		/*
			Fix instructions which use destination write-masking at a granularity
			not supported by the hardware.
		*/
		FixUnsupportedVectorMasks(psState);
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	
	DoOnAllBasicBlocks(psState, ANY_ORDER, MergeIdenticalInstructionsBP, IMG_FALSE, NULL);
	TESTONLY_PRINT_INTERMEDIATE("Merge Identical Instructions", "merge_ident", IMG_FALSE);

	METRICS_FINISH(psState, INSTRUCTION_SELECTION);

#if defined(IREGALLOC_LSCAN) || defined(REGALLOC_LSCAN)
	{
		PFUNC psFunc;
		for (psFunc = psState->psFnOutermost; psFunc; psFunc = psFunc->psFnNestInner)
			ComputeLoopNestingTree(psState, psFunc->sCfg.psEntry);
	}
#endif

	METRICS_START(psState, C10_REGISTER_ALLOCATION);
	/* Assign internal registers */
	DBG_PRINTF((DBG_TEST_DATA, __SECTION_TITLE__ "Internal Register Allocation" __SECTION_TITLE__));
	AssignInternalRegisters(psState);
	TESTONLY(PrintIntermediate(psState, "iregalloc", IMG_TRUE));

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		/*
			Fix vector instructions which use per-channel, negated predicates.
		*/
		FixNegatedPerChannelPredicates(psState);
	}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	METRICS_FINISH(psState, C10_REGISTER_ALLOCATION);

	METRICS_START(psState, REGISTER_ALLOCATION);
	if	(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_21752)
	{
		DBG_PRINTF((DBG_TEST_DATA, __SECTION_TITLE__ "Fix for BRN21752" __SECTION_TITLE__));
		DoOnAllBasicBlocks(psState, ANY_ORDER, AddFixForBRN21752BP, IMG_FALSE, NULL);
		TESTONLY(PrintIntermediate(psState, "fix21752", IMG_FALSE));

		psState->uFlags |= USC_FLAGS_APPLIEDBRN21752_FIX;
	}
	
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		TESTONLY(DBG_PRINTF((DBG_MESSAGE, "------- Vector dual issued instruction formation --------\n")));
		GenerateVectorDualIssue(psState);
	}
#endif

	#if defined(OUTPUT_USPBIN)
	if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
	{
		DBG_PRINTF((DBG_TEST_DATA, __SECTION_TITLE__ "After Alternate Results Insertion" __SECTION_TITLE__));
		InsertAlternateResults(psState);		
		TESTONLY(PrintIntermediate(psState, "altOutPuts", IMG_FALSE));
	}
	#endif /* defined(OUTPUT_USPBIN) */

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		LowerVectorRegisters(psState);
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Set up information about the hardware registers available for
		allocation.
	*/
	CalculateHardwareRegisterLimits(psState);

	/*
		Remove unused pixel shader iterations.
	*/
	if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL)
	{
		RemoveUnusedPixelShaderInputs(psState);
	}

	/*
		Set up information about groups of registers which require consecutive hardware register
		numbers.
	*/
	SetupRegisterGroups(psState);

	/*
		Assign hardware register numbers to variables containing uniform data.
	*/
	AllocateSecondaryAttributes(psState);
	TESTONLY_PRINT_INTERMEDIATE("Assign secondary attribute registers", "assign_sa", IMG_TRUE);

	if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL)
	{
		/*
			Assign hardware register numbers to variables which are the results of iterations
			or non-dependent texture samples.
		*/
		AllocatePixelShaderIterationRegisters(psState);
		TESTONLY_PRINT_INTERMEDIATE("Assign pixel shader iteration registers", "assign_psinput", IMG_TRUE);
	}

	/*
		Fix instructions which use invalid source register banks.
	*/
	FixInvalidSourceBanks(psState);
	TESTONLY_PRINT_INTERMEDIATE("Fix invalid source banks", "fix_invalid_banks", IMG_FALSE);

	/*
		Assign hardware index registers.
	*/
	METRICS_START(psState, ASSIGN_INDEX_REGISTERS);
	psState->uFlags |= USC_FLAGS_POSTINDEXREGALLOC;
	AssignHardwareIndexRegisters(psState);
	TESTONLY_PRINT_INTERMEDIATE("Assign index registers", "assign_index", IMG_FALSE);
	METRICS_FINISH(psState, ASSIGN_INDEX_REGISTERS);

#if defined(SUPPORT_SGX545)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_DUAL_ISSUE)
	{
		if(GenerateDualIssue(psState))
		{
			TESTONLY_PRINT_INTERMEDIATE("Instruction selection (Dual Issue)", "isel_dual", IMG_TRUE);
			
			MergeAllBasicBlocks(psState);
			TESTONLY_PRINT_INTERMEDIATE("Merge Blocks (After Dual Issue)", "merge_blocks", IMG_FALSE);

			EliminateMovesFromGPI(psState);
		}
	}
#endif /* defined(SUPPORT_SGX545) */

	/* Register allocation */
	AssignRegisters(psState);
	TESTONLY(PrintIntermediate(psState, "regalloc", IMG_TRUE));
	METRICS_FINISH(psState, REGISTER_ALLOCATION);

	/*
		Release information about groups of registers requiring consecutive hardware
		register numbers.
	*/
	ReleaseRegisterGroups(psState);

	/*
		occasionally register allocation empties a block (and hence, potentially
		a function) because of coalescing source and destination to a move. At any
		rate it seems a good idea to get the block structure nailed down before
		finalization. Hence:
	*/
	MergeAllBasicBlocks(psState);
	
	METRICS_START(psState, FINALISE_SHADER);
	FinaliseShader(psState);
	METRICS_FINISH(psState, FINALISE_SHADER);
}

#ifdef SRC_DEBUG
IMG_INTERNAL
IMG_VOID CreateSourceCycleCountTable(PINTERMEDIATE_STATE			psState,
									 PUNIFLEX_SOURCE_CYCLE_COUNT	psTableSourceCycle)
/*****************************************************************************
 FUNCTION	: CreateSourceCycleCountTable

 PURPOSE	: Creates the source cycle counter table to be used by 
			  GLSL compiler.

 PARAMETERS	: psState	- Compiler state. (carries the source table)
			  psTableSourceCycle - Table that will be generated for 
								   GLSL compiler. This table will carries 
								   entries for only those source lines that 
								   have consumed any hardware cycles.

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32 i, j;
	IMG_UINT32 uTotalCostingLines = 1;
	IMG_UINT32 uTotalCycleCount;

	/* Get cycle count of usc generated instructions */
	uTotalCycleCount = psState->puSrcLineCost[(psState->uTotalLines)];

	for(i=1; i<(psState->uTotalLines); i++)
	{
		if((psState->puSrcLineCost[i])>0)
		{
			uTotalCostingLines++;

			/* Sum up the total cycle cost */
			uTotalCycleCount += (psState->puSrcLineCost[i]);
		}
	}

	/* Get memory for source line cycle cost table. */
	psTableSourceCycle->psSourceLineCost = 
		(PUNIFLEX_SOURCE_LINE_COST) psState->pfnAlloc(sizeof(UNIFLEX_SOURCE_LINE_COST) * 
		uTotalCostingLines);

	if(psTableSourceCycle->psSourceLineCost == NULL)
	{
		USC_ERROR(UF_ERR_NO_MEMORY, 
			"Failed to alloc space for source line cycle count table");
	}
	
	j=1;
	for(i=1; i<(psState->uTotalLines); i++)
	{
		if((psState->puSrcLineCost[i])>0)
		{
			/* Set the source line number. */
			psTableSourceCycle->psSourceLineCost[j].uSrcLine = i;
			
			/* Set the cycle cost for this source line number. */
			psTableSourceCycle->psSourceLineCost[j].uCost = 
				psState->puSrcLineCost[i];
			
			j++;
		}
	}

	/* Set cycle count of usc generated instructions. */
	psTableSourceCycle->psSourceLineCost[0].uCost = 
				psState->puSrcLineCost[(psState->uTotalLines)];
	
	/* Set source line number for usc generated instructions. */
	psTableSourceCycle->psSourceLineCost[0].uSrcLine = 
				(psState->uTotalLines);

	/* Set the total source lines that have consumed any cycles */
	psTableSourceCycle->uTableCount = uTotalCostingLines;
	
	/* Set the total cycles consumed by given program */
	psTableSourceCycle->uTotalCycleCount = uTotalCycleCount;
}
#endif /* SRC_DEBUG */

/*********************************************************************************
 Function			: UscMakeState

 Description		: Make a minimal usc compiler state

 Parameters			: pfnAlloc    - Memory allocation function
                      pfnFree     - Memory free

 Notes              : Sets fields pfnAlloc, pfnFree and pfnPrint
                      Initialises memory allocation routines.

 Return				: A pointer to the created state
*********************************************************************************/
IMG_INTERNAL
PINTERMEDIATE_STATE UscMakeState(USC_ALLOCFN pfnAlloc,
								 USC_FREEFN pfnFree,
								 USC_PRINTFN pfnPrint)
{
	PINTERMEDIATE_STATE	psState;

	psState = pfnAlloc(sizeof(*psState));
	if (psState == NULL)
	{
		return NULL;
	}
	memset(psState,0,sizeof(*psState));

	psState->pfnAlloc = pfnAlloc;
	psState->pfnFree = pfnFree;
	psState->pfnPrint = pfnPrint;

	psState->pfnPDump = NULL;
	psState->pvPDumpFnDrvParam = NULL;
	psState->pvMetricsFnDrvParam = NULL;
	psState->pfnStart = NULL;
	psState->pfnFinish = NULL;
	psState->psAllocationListHead = NULL;
	#ifdef DEBUG
	psState->uMemoryUsedHWM = 0;
	psState->uMemoryUsed = 0;
	psState->uAllocCount = 0;
	#endif /* DEBUG */

	#ifdef SRC_DEBUG
	psState->uCurSrcLine = UNDEFINED_SOURCE_LINE;
	psState->puSrcLineCost = NULL;
	psState->uTotalLines = UNDEFINED_SOURCE_LINE;
	psState->uTotalCost = 0;
	psState->uTotalSAUpdateCost = 0;
	#endif /* SRC_DEBUG */

	return psState;
}

/*********************************************************************************
 Function			: PVRUniFlexCreateContext

 Description		: Create a context for the universal shader compiler.

 Parameters			: None.

 Globals Effected	: None

 Return				: A pointer to the created context (opaque type).
*********************************************************************************/
USC_EXPORT
IMG_PVOID IMG_CALLCONV PVRUniFlexCreateContext(USC_ALLOCFN	pfnAlloc,
											   USC_FREEFN	pfnFree,
											   USC_PRINTFN	pfnPrint,
											   USC_PDUMPFN	pfnPDump,
											   IMG_PVOID	pvPDumpFnDrvParam,
											   USC_STARTFN  pfnStart,
											   USC_FINISHFN pfnFinish,
											   IMG_PVOID    pvMetricsFnDrvParam)
{
	PINTERMEDIATE_STATE	psState;
	
	psState = pfnAlloc(sizeof(*psState));
	if (psState == NULL)
	{
		return NULL;
	}

	psState->pfnAlloc = pfnAlloc;
	psState->pfnFree = pfnFree;
	psState->pfnPrint = pfnPrint;
	psState->pfnPDump = pfnPDump;
	psState->pvPDumpFnDrvParam = pvPDumpFnDrvParam;
	psState->pvMetricsFnDrvParam = pvMetricsFnDrvParam;
	psState->pfnStart = pfnStart;
	psState->pfnFinish = pfnFinish;
	psState->psAllocationListHead = NULL;
	#ifdef DEBUG
	psState->uMemoryUsedHWM = 0;
	psState->uMemoryUsed = 0;
	psState->uAllocCount = 0;
	#endif /* DEBUG */

	#ifdef SRC_DEBUG
	psState->uCurSrcLine = UNDEFINED_SOURCE_LINE;
	psState->puSrcLineCost = NULL;
	psState->uTotalLines = UNDEFINED_SOURCE_LINE;
	psState->uTotalCost = 0;
	psState->uTotalSAUpdateCost = 0;
	#endif /* SRC_DEBUG */

	#if defined (FAST_CHUNK_ALLOC)
	MemManagerInit(&(psState->sMemManager));
	#endif

	return (IMG_PVOID)psState;
}

/*********************************************************************************
 Function		: SetErrorHandler

 Description	: Enable or disable the error handler

 Parameters		: psState    - Compiler state
                  bEnable    - Enable (IMG_TRUE) or disable (IMG_FALSE) the
                               error handler
 Return			: Nothing
*********************************************************************************/
static
IMG_VOID SetErrorHandler(PINTERMEDIATE_STATE psState,
						 IMG_BOOL bEnable)
{
	psState->bExceptionReturnValid = bEnable;
}

/*********************************************************************************
 Function		: CleanUpOnError

 Description	: Clean up after a compiler error

 Parameters		: psState    - Compiler state

 Return			: Nothing
*********************************************************************************/
static
IMG_VOID CleanUpOnError(PINTERMEDIATE_STATE psState)
{
	/*
	  Free all allocated blocks.
	*/
	while (psState->psAllocationListHead)
	{
		PUSC_ALLOC_HEADER	psBlock;

		psBlock = psState->psAllocationListHead;
		psState->psAllocationListHead = psBlock->psNext;

		psState->pfnFree(psBlock);
	}
#ifdef DEBUG
	psState->uMemoryUsed = 0;
#endif /* DEBUG */
}

/**********************************************************************
 *
 * Common functions for configurations that take Uniflex input.
 *
 **********************************************************************/

#if defined(INPUT_UNIFLEX)

#if defined(UF_TESTBENCH)
static
IMG_VOID MkVerifHelper(PINTERMEDIATE_STATE psState, PUNIFLEX_VERIF_HELPER psVerifHelper)
/*********************************************************************************
 Function		: MkVerifHelper

 Description	: Copy information in the UNIFLEX_VERIF_HELPER to return to the
				  test bench.

 Parameters		: psState    - Compiler state
				  psVerifHelper
							 - Returns information about the compiled program to
							 assist testing of the compiler.

 Return			: Nothing.
*********************************************************************************/
{
	IMG_UINT32	uNumArrays;
	IMG_UINT32	uArrayIdx;

	VerifierPreCompileInit(psVerifHelper);
	/*
		Allocate memory to describe the storage of indexable temporary
		arrays.
	*/
	uNumArrays = psState->uIndexableTempArrayCount;
	psVerifHelper->psIndexableTemps = 
		psState->pfnAlloc(sizeof(psVerifHelper->psIndexableTemps[0]) * uNumArrays);

	/*
		Initialize each element to say the array is in memory.
	*/
	for (uArrayIdx = 0; uArrayIdx < uNumArrays; uArrayIdx++)
	{
		PTEMP_ARRAY_LOCATION	psLoc;

		psLoc = &psVerifHelper->psIndexableTemps[uArrayIdx];
		psLoc->bInRegisters = IMG_FALSE;
		psLoc->uStart = psLoc->uEnd = UINT_MAX;
	}
	/*
		For each array in registers copy information about its location.
	*/
	for (uArrayIdx = 0; uArrayIdx < psState->uNumVecArrayRegs; uArrayIdx++)
	{
		PTEMP_ARRAY_LOCATION	psLoc;
		PUSC_VEC_ARRAY_REG		psVecArrayReg = psState->apsVecArrayReg[uArrayIdx];

		if (psVecArrayReg != NULL && psVecArrayReg->eArrayType == ARRAY_TYPE_NORMAL)
		{
			psLoc = &psVerifHelper->psIndexableTemps[psVecArrayReg->uArrayNum];
			psLoc->bInRegisters = IMG_TRUE;
			psLoc->uStart = psVecArrayReg->uBaseReg;
			psLoc->uEnd = psVecArrayReg->uBaseReg + psVecArrayReg->uRegs;
		}
	}

	/*
		Copy the compiler flag (including any modifications for precision conversion).
	*/
	psVerifHelper->uCompilerFlags = psState->uCompilerFlags;
	/*
		Copy target core info
	*/
	psVerifHelper->psTargetDesc = psState->psTargetDesc;
	psVerifHelper->psTargetFeatures = psState->psTargetFeatures;
	psVerifHelper->psTargetBugs = psState->psTargetBugs;
}
#endif /* UF_TESTBENCH */

/*********************************************************************************
 Function		: CompileUniflex

 Description	: Convert and optimise a uniflex program to an internal representation
                  suitable for lowering to output code.

 Parameters		: psState    - Compiler state
				  psVerifHelper
							 - Returns information about the compiled program to
							 assist testing of the compiler.
                  psSWProc   - Uniflex input program

 Return			: Nothing.
*********************************************************************************/
static
IMG_VOID CompileUniflex(PINTERMEDIATE_STATE		psState, 
						#if defined(UF_TESTBENCH)
						PUNIFLEX_VERIF_HELPER	psVerifHelper,
						#endif /* defined(UF_TESTBENCH) */
						PUNIFLEX_INST			psSWProc)
{
#if defined(TRACK_REDUNDANT_PCONVERSION)
	PINTERMEDIATE_STATE	psSavedState;
	USC_PCONVERT_SETTINGS_FLAGS		eEnforcementTestSettings;
	IMG_UINT32			uCompileForUSPBin;
	PINPUT_PROGRAM		psOverrideProg = IMG_NULL;
	PUNIFLEX_INST		psInputProg;
	IMG_UINT32			auPrecisionConversionPaths[2] = {0,0};
#endif
		
	#if defined(OUTPUT_USPBIN) && !defined(OUTPUT_USCHW)
	ASSERT(psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN);
	#endif /* defined(OUTPUT_USPBIN) && !defined(OUTPUT_USCHW) */

	#if defined(OUTPUT_USCHW) && !defined(OUTPUT_USPBIN)
	ASSERT((psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) == 0);
	#endif /* defined(OUTPUT_USCHW) && !defined(OUTPUT_USPBIN) */
	
	#if defined(DEBUG)
	DebugVerifyInstGroup(psState);
	#endif
	

	#if defined(TRACK_REDUNDANT_PCONVERSION)
	/* Allocating alternate state */
	psSavedState = psState->pfnAlloc(sizeof(INTERMEDIATE_STATE));
	
	if(psState->uCompilerFlags & (UF_FORCE_C10_TO_F16_PRECISION | UF_FORCE_F32_PRECISION))
	{
		psOverrideProg = OverridePrecisions(psState, psSWProc);
		if(psOverrideProg)
		{
			psInputProg = psOverrideProg->psHead;
		}
		else
		{
			/* Forward old program */
			psInputProg = psSWProc;
		}
		psState->uCompilerFlags &= ~UF_FORCE_PRECISION_CONVERT_ENABLE;
	}
	else
	{
		if(!psState->bInvariantShader)
		{
			if(psState->uCompilerFlags & UF_FORCE_PRECISION_CONVERT_ENABLE)
			{
				/* Manually set flag indicates pconvert to disable functionality */
				psState->uCompilerFlags &= ~UF_FORCE_PRECISION_CONVERT_ENABLE;
				
				DBG_PRINTF((DBG_WARNING, "Precision convertion disabled, found manual disable flag."));
			}
			/* Don't do a precision conversion redundancy check for OpenCL */
			else if ((psState->uCompilerFlags & UF_OPENCL) == 0)
			{
				/* Enable precision conversion redundancy check as default */
				psState->uCompilerFlags |= UF_FORCE_PRECISION_CONVERT_ENABLE;
			}
		}
		else
		{
			DBG_PRINTF((DBG_PCONVERT_DATA, "PCONVERT %s - Precision conversion skipped for invariant shader", USC_INPUT_FILENAME));
		}
		psInputProg = psSWProc;
	}
	
	uCompileForUSPBin = (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN);
	
	/* Convert the uniflex program to intermediate code	*/
	METRICS_START(psState, INTERMEDIATE_CODE_GENERATION);
	ConvertToIntermediate(psInputProg, psState);
	TESTONLY_PRINT_INTERMEDIATE("Converted input", "conv_input", IMG_FALSE);
	#else /*defined(TRACK_REDUNDANT_PCONVERSION)*/
	/* Convert the uniflex program to intermediate code	*/
	METRICS_START(psState, INTERMEDIATE_CODE_GENERATION);
	ConvertToIntermediate(psSWProc, psState);
	TESTONLY_PRINT_INTERMEDIATE("Converted input", "conv_input", IMG_FALSE);
	#endif /*defined(TRACK_REDUNDANT_PCONVERSION)*/

#ifdef SRC_DEBUG
	psState->uCurSrcLine = (IMG_UINT32)UNDEFINED_SOURCE_LINE;
#endif /* SRC_DEBUG */
	FinaliseIntermediateCode(psState);
	TESTONLY_PRINT_INTERMEDIATE("Merged input", "merged_input", IMG_FALSE);
	METRICS_FINISH(psState, INTERMEDIATE_CODE_GENERATION);

#if defined(TRACK_REDUNDANT_PCONVERSION)
	CompileIntermediate(psState);
	if((psState->uOptimizationLevel > 0) && (psState->uCompilerFlags & UF_FORCE_PRECISION_CONVERT_ENABLE))
	{
		/* Search for template Instruction patterns for redundant coversions */
		DoOnAllBlocksForFunction(psState, psState->psMainProg, TrackRedundantFormatConversions, IMG_NULL);
		
		/* Compute the precision upconvert enforcing flags */
		eEnforcementTestSettings = ConsolidateFormatConversionPatterns(psState, psState->psMainProg, auPrecisionConversionPaths);
	}
	else
	{
		eEnforcementTestSettings = USC_DISABLE_ALL_TESTS;
	}
	
	if(((IMG_UINT32)eEnforcementTestSettings & USC_DISABLE_INSTCYCLE_TEST) == 0)
	{
		psOverrideProg = OverridePrecisions(psState, psInputProg);
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		/* Try one more setting of f16 to f32 if c10 registers are absent */
		if(!psOverrideProg)
		{
			/* Try default settings */
			TryDefaultPrecisionEnforcingSetting(psState, USC_PCONVERT_PRIMARY);
			psOverrideProg = OverridePrecisions(psState, psInputProg);
			if(!psOverrideProg)
			{
				/* Try secondary precision conversion setting */
				TryDefaultPrecisionEnforcingSetting(psState, USC_PCONVERT_SECONDARY);
				psOverrideProg = OverridePrecisions(psState, psInputProg);
			}
		}
#endif
		if(psOverrideProg)
		{
			IMG_BOOL bResult;
			
			/* Clone state */
			memcpy(psSavedState, psState, sizeof(INTERMEDIATE_STATE));
			
			/* Truncate allocation list it will be now part of new compile state */
			psSavedState->psAllocationListHead = IMG_NULL;
			
			InitState(psState, psState->uCompilerFlags, psState->uCompilerFlags2, psState->psConstants,psState->psSAOffsets);
			
			/* Append saved USC_FLAGS_COMPILE_FOR_USPBIN flag */
			psState->uFlags |= uCompileForUSPBin;
			
			ConvertToIntermediate(psOverrideProg->psHead, psState);
			
			/* Free the overridden input program as not needed anymore */
			FreeInputProg(psState, &psOverrideProg);

			FinaliseIntermediateCode(psState);
			
			/* Compile intermediate represention */
			CompileIntermediate(psState);
			
			/* Do decide which version is better */
			bResult = PConvertCompareCostings(psState, psSavedState, psState, eEnforcementTestSettings);
			if(bResult)
			{
				DBG_PRINTF((DBG_PCONVERT_DATA, "PCONVERT: Precision enforced %s", USC_INPUT_FILENAME));
				#if defined(DEBUG)
				PrintInput(psState, 
							";------ Raw Input after precision override -------\r\n", 
							";------ End Raw Input after precision override ----\r\n", 
							psSWProc,
							psState->psConstants,
							psState->uCompilerFlags,
							psState->uCompilerFlags2,
							psState->psSAOffsets,
							(psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) ? IMG_TRUE : IMG_FALSE
						       #ifdef USER_PERFORMANCE_STATS_ONLY
						       ,(IMG_CHAR*)NULL
						       #endif
						       );
				#endif
				TESTONLY_PRINT_INTERMEDIATE("Converted input after recompilation", "conv_input_recompile", IMG_FALSE);
			
				/* Release the descarded state */
				ReleaseState(psSavedState);
				
				/* Print out any memory leaks */
				DisplayUnfreedAllocs(psSavedState);
			}
			else
			{
				/* Release precision enforcing compile state */
				ReleaseState(psState);
				
				/* Restore orignal state by memory copy */
				memcpy(psState, psSavedState, sizeof(INTERMEDIATE_STATE));
				psState->uCompilerFlags &= 
					~(UF_FORCE_F16_TO_F32_PRECISION | UF_FORCE_C10_TO_F16_PRECISION | UF_FORCE_C10_TO_F32_PRECISION);
			}
		}
		else
		{
			DBG_PRINTF((DBG_PCONVERT_DATA, "PCONVERT: (%s) Nothing found to override", USC_INPUT_FILENAME));
		}
	}
	else
	{
		if(psOverrideProg)
		{
			/* Free the overridden input program */
			FreeInputProg(psState, &psOverrideProg);
		}
		DBG_PRINTF((DBG_PCONVERT_DATA, "PCONVERT: (%s) No redundant path found", USC_INPUT_FILENAME));
		psState->uCompilerFlags &= 
			~(UF_FORCE_F16_TO_F32_PRECISION | UF_FORCE_C10_TO_F16_PRECISION | UF_FORCE_C10_TO_F32_PRECISION);
	}
	
	psState->pfnFree(psSavedState);
#else
	/* Optimise and lower the intermediate code to output code */
	CompileIntermediate(psState);
#endif

	#ifdef UF_TESTBENCH	
	/*
		Return extra information about compilation to assist compiler
		testing.
	*/
	MkVerifHelper(psState, psVerifHelper);
	#endif /* defined(UF_TESTBENCH) */
	
}
#endif /* defined(INPUT_UNIFLEX) */

/**********************************************************************
 *
 * Standard build: Uniflex input, HW output
 *
 **********************************************************************/

#if defined(OUTPUT_USCHW)
/*********************************************************************************
 Function		: PVRUniFlexCompileToHw

 Description	: External API to generate a HW executeable program (with
				  associated meta-data), suitable for runtime compilation
				  within a driver.

 Parameters		: pvContext			- Compiler context.				  
				  psSWProc			- Program to compile.
				  pdwTexDimensions	- Array of texture dimensions.
				  psConstants		- Local constants used in the program.
				  psSAOffsets		- Layout of the secondary attribute
									  registers.
				  dwProjectedCoordinateMask
									- Mask of the texture coordinates which are 
									  projectecd.
				  psTextures		- Formats of the textures.
				  dwGammaStages		- Texture stages which have gamma enabled.
				  dwPreambleCount	- Space to reserve at the start of the
									  code.
				  psHw				- Structure that returns the compiled
									  program.
 Return			: Compilation status.
*********************************************************************************/
USC_EXPORT
IMG_UINT32
IMG_CALLCONV PVRUniFlexCompileToHw(IMG_PVOID					pvContext,
								   PUNIFLEX_INST				psSWProc,
								   PUNIFLEX_CONSTDEF			psConstants,
								   PUNIFLEX_PROGRAM_PARAMETERS	psProgramParameters,
								   #if defined(UF_TESTBENCH)
								   PUNIFLEX_VERIF_HELPER		psVerifHelper,
								   #endif /* defined(UF_TESTBENCH) */
								   PUNIFLEX_HW					psHw)
{
	PINTERMEDIATE_STATE	psState;
	IMG_UINT32			uErr;
	jmp_buf mark;

	psState	= (PINTERMEDIATE_STATE)pvContext;
	uErr	= UF_ERR_INTERNAL; 
	
	METRICS_START(psState, TOTAL_COMPILE_TIME);

	/* Start the error handler */
	if ((uErr = setjmp(mark)) != 0U)
	{
		/*
			Free any allocations with UscAlloc.
		*/
		CleanUpOnError(psState);

		/*
			Free any allocations which we're going to return
			to the driver.
		*/
		CleanupUniflexHw(psState, psHw);

		METRICS_FINISH(psState, TOTAL_COMPILE_TIME);
		
		return uErr;
	}
	else
	{
		memcpy(&psState->sExceptionReturn, &mark, sizeof(mark));
	}
	
	
	/* Enable the error handler */
	SetErrorHandler(psState, IMG_TRUE);

	/* 
		Clear arrays we allocate in the result structure so we know whether we need to free them.
	*/
	psHw->psNonDependentTextureLoads = NULL;
	psHw->auNonAnisoTextureStages = NULL;
	psHw->asTextureUnpackFormat = NULL;

	/*
		Decode the input shader and parameters
	*/
	if	((psProgramParameters->uFlags & UF_QUIET) == 0)
	{
		#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
		PrintInput(psState, 
				   ";------ Raw Input -------\r\n", 
				   ";------ End Raw Input ----\r\n", 
				   psSWProc,
				   psConstants,
				   psProgramParameters->uFlags,
				   psProgramParameters->uFlags2,
				   psProgramParameters,
				   IMG_FALSE /* bCompileForUSP */
				   #ifdef USER_PERFORMANCE_STATS_ONLY
				   ,(IMG_CHAR*)"pliczek.txt"
				   #endif
			);
		#endif /* defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
	}

	/*
		Initialise the compiler state
	*/
	InitState(psState,
			  psProgramParameters->uFlags,
			  psProgramParameters->uFlags2,
			  psConstants,
			  psProgramParameters);

	/* Compile the program */
	CompileUniflex(psState, 
				   #if defined(UF_TESTBENCH)
				   psVerifHelper,
				   #endif /* defined(UF_TESTBENCH) */
				   psSWProc);
	
	#ifdef SRC_DEBUG
	psState->uTotalSAUpdateCost = 0;
	if (psState->psSecAttrProg)
	{
		PINST psInst;
		//this'll only work for single-BB SA Programs...
		ASSERT (psState->psSecAttrProg->sCfg.psEntry == psState->psSecAttrProg->sCfg.psExit);
		for (psInst = psState->psSecAttrProg->sCfg.psEntry->psBody; psInst; psInst = psInst->psNext)
		{
			DecrementCostCounter(psState, (psInst->uAssociatedSrcLine), 1);
			(psState->uTotalSAUpdateCost)++;
		}
	}
	ASSERT((psState->uTotalSAUpdateCost) == (psState->uSAProgInstCount));
	#endif /* SRC_DEBUG */

	/*
		Generate a HW program from the intermediate code.
	*/
	uErr = CompileToHw(psState, psHw);

	#ifdef SRC_DEBUG
	/* Dump instructions with source line and cycle counts */
	if	((psProgramParameters->uFlags & UF_QUIET) == 0)
	{
		#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
		PrintSourceLineCost(psState, 
				   psSWProc);
		#endif /* defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
	}
	#endif /* SRC_DEBUG */

	/* 
		Free compiler data and instructions
	*/
	ReleaseState(psState);
	
	/* Print out any memory leaks */
	DisplayUnfreedAllocs(psState);

	#if defined(UF_TESTBENCH) && defined(DEBUG)
	psVerifHelper->uMemoryUsedHWM = psState->uMemoryUsedHWM;
	psVerifHelper->psHw = psHw;
	#endif /* defined(UF_TESTBENCH) && defined(DEBUG) */

	ASSERT(psState->psAllocationListHead == NULL);
	
	/* Disable the error handler */
	SetErrorHandler(psState, IMG_FALSE);
	
	METRICS_FINISH(psState, TOTAL_COMPILE_TIME);

	return uErr;
}
#endif /* #if defined(OUTPUT_USCHW) */

#if defined(OUTPUT_USPBIN)
/**********************************************************************
 *
 * USP Build: Uniflex input, usp output
 *
 **********************************************************************/

/*********************************************************************************
 Function		: PVRUniFlexCompileToUspBin

 Description	: External API to Generate USP compatible pre-compiled binary
				  shader data, suitable for offline compilation and subsequent
				  runtime finalisation by the USP.

 Parameters		: pvContext			- Compiler context.
				  uFlags			- Compiler flags.
				  psSWProc			- Program to compile.
				  pdwTexDimensions	- Array of texture dimensions.
				  psConstants		- Local constants used in the program.
				  psSAOffsets		- Layout of the secondary attribute
									  registers.
				  dwProjectedCoordinateMask
									- Mask of the texture coordinates which are 
									  projectecd.
				  psTextures		- Formats of the textures.
				  dwGammaStages		- Texture stages which have gamma enabled.
				  dwPreambleCount	- Space to reserve at the start of the code.
				  psHw				- Structure that returns the compiled
									  program.

 Return			: Compilation status.
*********************************************************************************/
USC_EXPORT
IMG_UINT32
IMG_CALLCONV PVRUniFlexCompileToUspBin(IMG_PVOID					pvContext,
									   IMG_UINT32					*pui32Flags,
									   PUNIFLEX_INST				psSWProc,
									   PUNIFLEX_CONSTDEF			psConstants,
									   PUNIFLEX_PROGRAM_PARAMETERS	psProgramParameters,
									   #if defined(UF_TESTBENCH)
									   PUNIFLEX_VERIF_HELPER		psVerifHelper,
									   #endif /* defined(UF_TESTBENCH) */
									   PUSP_PC_SHADER*				ppsPCShader
#ifdef SRC_DEBUG
									   , PUNIFLEX_SOURCE_CYCLE_COUNT		psTableSourceCycle
#endif /* SRC_DEBUG */
	)
{
	PINTERMEDIATE_STATE	psState;
	PUSP_PC_SHADER		psPCShader;
	IMG_UINT32			uErr;
	IMG_UINT32			uFlags;
	IMG_UINT32			uFlags2;

	psState		= (PINTERMEDIATE_STATE)pvContext;
	psPCShader	= IMG_NULL;
	uErr		= UF_ERR_INTERNAL;
	uFlags = psProgramParameters->uFlags;
	uFlags2 = psProgramParameters->uFlags2;

	METRICS_START(psState, TOTAL_COMPILE_TIME);

	/* Start the error handler */
	if ((uErr = setjmp(psState->sExceptionReturn)) != 0U)
	{
		/*
			Free any allocations with UscAlloc.
		*/
		CleanUpOnError(psState);

		/*
			Free any allocations which we're going to return
			to the driver.
		*/
		if (psPCShader != NULL)
		{
			psState->pfnFree(psPCShader);
			psPCShader = NULL;
		}

		METRICS_FINISH(psState, TOTAL_COMPILE_TIME);
		
		return uErr;
	}
	
	/* Enable the error handler */
	SetErrorHandler(psState, IMG_TRUE);

	/* Dump the input. */
	if	((uFlags & UF_QUIET) == 0)
	{
		#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
		PrintInput(psState, 
				   ";------ Raw Input -------\r\n", 
				   ";------ End Raw Input ----\r\n", 
				   psSWProc,
				   psConstants,
				   uFlags,
				   uFlags2,
				   psProgramParameters,
				   IMG_TRUE /* bCompileForUSP */
				   #ifdef USER_PERFORMANCE_STATS_ONLY
				   ,(IMG_CHAR*)"pliczek.txt"
				   #endif
				   );
		#endif /* defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
	}

	/*
		Initialise the compiler state
	*/
	InitState(psState,
			  uFlags,
			  uFlags2,
			  psConstants,
			  psProgramParameters);

	/* Set compiling for OUTPUT_USPBIN */

	psState->uFlags |= USC_FLAGS_COMPILE_FOR_USPBIN;

	/*
		Compile the input program to intermediate code
	*/
	CompileUniflex(psState, 
				   #if defined(UF_TESTBENCH)
				   psVerifHelper,
				   #endif /* defined(UF_TESTBENCH) */
				   psSWProc);

	/*
		Generate USP-compitible pre-compiled binary shader-data from the
		intermediate code
	*/
	CreateUspBinOutput(psState, &psPCShader);

	#ifdef SRC_DEBUG
	if(uErr == UF_OK)
	{
		/*
			Table creation is required or not. 
		*/
		if(psTableSourceCycle != IMG_NULL)
		{
			CreateSourceCycleCountTable(psState, psTableSourceCycle);
		}
	}
	#endif /* SRC_DEBUG */

	#ifdef SRC_DEBUG
	/* Dump instructions with source line and cycle counts */
	if	((uFlags & UF_QUIET) == 0)
	{
		#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
		PrintSourceLineCost(psState, 
				   psSWProc);
		#endif /* defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) */
	}
	#endif /* SRC_DEBUG */

	/* 
	   Free compiler data and instructions
	 */
	ReleaseState(psState);

#if defined(SUPPORT_ICODE_SERIALIZATION)
	if(psState->psAllocationListHead)
	{
		/*
			Debug facility isn't intened for release build, Belatedly removing 
			allocations to continue compilation further.
		*/
		DBG_PRINTF((DBG_WARNING, "Belatedly removed unfreed memory allocations."));
		CleanUpOnError(psState);
	}
#else
	/* Print out any memory leaks */
	DisplayUnfreedAllocs(psState);
#endif
	
	ASSERT(psState->psAllocationListHead == NULL);
	
	/* Disable the error handler */
	SetErrorHandler(psState, IMG_FALSE);

	*pui32Flags = 0;
	
	if (psState->uFlags & USC_FLAGS_TEXKILL_PRESENT)
	{
		*pui32Flags |= UNIFLEX_HW_FLAGS_TEXKILL_USED;
	}

	*ppsPCShader = psPCShader;
	
	METRICS_FINISH(psState, TOTAL_COMPILE_TIME);
	
	return uErr;
}

/*********************************************************************************
 Function			: PVRUniFlexDestroyUspBin

 Description		: External API to destroy previously generated USP binary-
					  shader data, as created using PVRUniFlexCompileToUspBin.

 Parameters			: pvContext		- Compiler context.
					  psPCShader	- The PC shader data to destroy

 Return				: Compilation status.
*********************************************************************************/
USC_EXPORT
IMG_VOID IMG_CALLCONV PVRUniFlexDestroyUspBin(IMG_PVOID			pvContext,
											  PUSP_PC_SHADER	psPCShader)
{
	PINTERMEDIATE_STATE	psState;

	psState = (PINTERMEDIATE_STATE)pvContext;

	DestroyUspBinOutput(psState, psPCShader);
}
#endif /* defined(OUTPUT_USPBIN) */

IMG_INTERNAL
IMG_VOID InitCfg(PINTERMEDIATE_STATE psState, PCFG psCfg, PFUNC psFunc)
{
	psCfg->uNumBlocks = 0;
	psCfg->pfnCurrentSortOrder = NULL;
	psCfg->apsAllBlocks = NULL; //set by AllocateBlock)
	psCfg->psExit = AllocateBlock(psState, psCfg);
	psCfg->psExit->eType = CBTYPE_EXIT;
	psCfg->psExit->uNumSuccs = 0;
	psCfg->psExit->asSuccs = NULL;
	psCfg->psFunc = psFunc;
}

IMG_INTERNAL
PCFG AllocateCfg(PINTERMEDIATE_STATE psState, PFUNC psFunc)
{
	PCFG psCfg = UscAlloc(psState, sizeof(CFG));
	InitCfg(psState, psCfg, psFunc);
	return psCfg;
}

IMG_INTERNAL
PEDGE_INFO AppendEdgeInfoToList(PINTERMEDIATE_STATE psState, PUSC_LIST psEdgeInfoLst)
{
	PEDGE_INFO_LISTENTRY psEdgeInfoLstEntry;
	psEdgeInfoLstEntry = UscAlloc(psState, sizeof(*psEdgeInfoLstEntry));
	AppendToList(psEdgeInfoLst, &psEdgeInfoLstEntry->sListEntry);
	psEdgeInfoLstEntry->sEdgeInfo.psEdgeParentBlock = NULL;	
	return &(psEdgeInfoLstEntry->sEdgeInfo);
}

IMG_INTERNAL
IMG_BOOL IsEdgeInfoPresentInList(PUSC_LIST psEdgeInfoLst, PCODEBLOCK psEdgeParentBlock, IMG_UINT32 uEdgeSuccIdx)
{	
	PUSC_LIST_ENTRY	psListEntry;	
	PEDGE_INFO_LISTENTRY psEdgeInfoLstEntry;
	for (psListEntry = psEdgeInfoLst->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		psEdgeInfoLstEntry = IMG_CONTAINING_RECORD(psListEntry, PEDGE_INFO_LISTENTRY, sListEntry);
		if	(
				psEdgeInfoLstEntry->sEdgeInfo.psEdgeParentBlock == psEdgeParentBlock
				&&
				psEdgeInfoLstEntry->sEdgeInfo.uEdgeSuccIdx == uEdgeSuccIdx
			)
		{		
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_VOID FreeEdgeInfoList(PINTERMEDIATE_STATE psState, PUSC_LIST psEdgeInfoLst)
{
	PUSC_LIST_ENTRY	psListEntry;
	while ((psListEntry = RemoveListHead(psEdgeInfoLst)) != NULL)
	{
		PEDGE_INFO_LISTENTRY psEdgeInfoLstEntry;
		psEdgeInfoLstEntry = IMG_CONTAINING_RECORD(psListEntry, PEDGE_INFO_LISTENTRY, sListEntry);
		UscFree(psState, psEdgeInfoLstEntry);
	}
}
/******************************************************************************
 End of file (usc.c)
******************************************************************************/
