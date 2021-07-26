/******************************************************************************
 * Name         : reg.c
 * Author       : James McCarthy
 * Created      : 16/11/2005
 *
 * Copyright    : 2005-2008 by Imagination Technologies Limited.
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
 * $Log: reg.c $
 *****************************************************************************/

#include <stdio.h>
#include <math.h>

#include "apidefs.h"
#include "codegen.h"
#include "macros.h"
#include "source.h"
#include "inst.h"
#include "reg.h"

/******************************************************************************
 * Function Name: AddRegToList
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  :  
 *****************************************************************************/
static FFGenRegList *AddRegToList(FFGenCode			*psFFGenCode,
								  FFGenRegList		*psRegList,
								  FFGenReg			*psRegToAdd,
								  IMG_BOOL			bCreateCopy,
								  IMG_UINT32		uLineNumber,
								  const IMG_CHAR	*pszFileName)
{
	FFGenRegList *psNewEntry;
	FFGenReg     *psReg;

	FFGenRegList *psList = psRegList;

	PVR_UNREFERENCED_PARAMETER(psFFGenCode);
	PVR_UNREFERENCED_PARAMETER(uLineNumber);
	PVR_UNREFERENCED_PARAMETER(pszFileName);

	if (bCreateCopy)
	{
		/* Create copy of reg */
		psReg  = FFGENMalloc2(psFFGenCode->psFFGenContext, sizeof(FFGenReg), uLineNumber, pszFileName);
		*psReg = *psRegToAdd;
	}
	else
	{
		psReg = psRegToAdd;
	}

	if (psList)
	{
		/* Go to end of list */
		while (psList->psNext)
		{
			psList = psList->psNext;
		}
	}

#if defined(OGLES1_MODULE) && defined(FFGEN_UNIFLEX)
	psReg->pui32SrcOffset = IMG_NULL;
	psReg->pui32DstOffset = IMG_NULL;
#endif /* defined(OGLES1_MODULE) && defined(FFGEN_UNIFLEX) */

	/* Create new list entry */
	psNewEntry = FFGENMalloc2(psFFGenCode->psFFGenContext, sizeof(FFGenRegList), uLineNumber, pszFileName);

	/* Add register to list entry */
	psNewEntry->psReg  = psReg;
	psNewEntry->psPrev = psList;
	psNewEntry->psNext = IMG_NULL;

	if (psList)
	{
		psList->psNext = psNewEntry;
	}
	else
	{
		psRegList = psNewEntry;
	}

	return psRegList;
}

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  :  
 *****************************************************************************/
static IMG_VOID CompressFreeList(FFGenCode *psFFGenCode)
{
	FFGenRegList *psFreeList = psFFGenCode->psFreeTempList;

	IMG_BOOL bCompressed = IMG_TRUE;

	while (bCompressed)
	{
		bCompressed = IMG_FALSE;

		/* Search through list */
		while (psFreeList)
		{
			FFGenRegList *psNext = psFreeList->psNext;

			FFGenReg *psReg     = psFreeList->psReg;

			/* Is this reg at the end of the currently allocated space? */
			if (psReg->uOffset + psReg->uSizeInDWords == psFFGenCode->uCurrentTempSize)
			{
				/* Remove it from the list and reduce the allocation size */
				psFFGenCode->uCurrentTempSize -= psReg->uSizeInDWords;

				/* Remove entry */
				if (psFreeList->psPrev)
				{
					psFreeList->psPrev->psNext = psNext;
				}

				if (psNext)
				{
					psNext->psPrev = psFreeList->psPrev;
				}

				/* If this entry is at the head of the list replace the head pointer */
				if (psFreeList == psFFGenCode->psFreeTempList)
				{
					psFFGenCode->psFreeTempList = psNext;
				}

				FFGENFree(psFFGenCode->psFFGenContext, psReg);
				FFGENFree(psFFGenCode->psFFGenContext, psFreeList);

				bCompressed = IMG_TRUE;
			}
			else if (psNext)
			{
				FFGenReg *psNextReg = psNext->psReg;

				/* Can we merge these two together? */
				if (psReg->uOffset + psReg->uSizeInDWords == psNextReg->uOffset)
				{
					psReg->uSizeInDWords += psNextReg->uSizeInDWords;


					/* remove it from the list */
					psFreeList->psNext = psNext->psNext;

					if (psFreeList->psNext)
					{
						psFreeList->psNext->psPrev = psFreeList;

					}
					/* free the next reg */
					FFGENFree(psFFGenCode->psFFGenContext, psNextReg);
					FFGENFree(psFFGenCode->psFFGenContext, psNext);

					psNext = psFreeList->psNext;

					bCompressed = IMG_TRUE;
				}
				/* Can we merge them the other way round? */
				else if (psNextReg->uOffset + psNextReg->uSizeInDWords == psReg->uOffset)
				{
					psReg->uOffset = psNextReg->uOffset;
					psReg->uSizeInDWords += psNextReg->uSizeInDWords;

					/* free the next reg */
					FFGENFree(psFFGenCode->psFFGenContext, psNextReg);

					/* remove it from the list */
					psFreeList->psNext = psNext->psNext;

					if (psFreeList->psNext)
					{
						psFreeList->psNext->psPrev = psFreeList;
					}

					FFGENFree(psFFGenCode->psFFGenContext, psNext);

					psNext = psFreeList->psNext;

					bCompressed = IMG_TRUE;
				}
				
			}

			/* Get next temp on list */
			psFreeList = psNext;
		}
		
	}
}

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  :  
 *****************************************************************************/
IMG_INTERNAL IMG_VOID ReleaseReg(FFGenCode	*psFFGenCode,
								 FFGenReg	*psReg)
{
	if (!psReg)
	{
		return;
	}

	/* Can only add temps back to a free list */
	if (psReg->eType != USEASM_REGTYPE_TEMP)
	{
		return;
	}

	/* Was this the last temp allocated? */
	if (psReg->uOffset + psReg->uSizeInDWords == psFFGenCode->uCurrentTempSize)
	{
		/*
		   Yes, then just free it and reduce the size of the temp stack
		   This reduces the chance of having awkward size holes (like a matrix) in our free temp list
		   if we're careful about the order in which we make and release large temp allocations
		*/
		psFFGenCode->uCurrentTempSize -= psReg->uSizeInDWords;

		/* free it */
		FFGENFree(psFFGenCode->psFFGenContext, psReg);
	}
	else
	{
		/* Add this temp back to the free list */
		psFFGenCode->psFreeTempList = AddRegToList(psFFGenCode, psFFGenCode->psFreeTempList, psReg, IMG_FALSE, __LINE__, __FILE__);

		CompressFreeList(psFFGenCode);
	}

#if 0
	{
		FFGenRegList *psList = psFFGenCode->psFreeTempList;

		IMG_UINT32 uTotal = 0;

		COMMENT(psFFGenCode, "Temp size = %d", psFFGenCode->uCurrentTempSize);

		while (psList)
		{
			COMMENT(psFFGenCode, "Temp reg = (%d->%d) %d", psList->psReg->uOffset, psList->psReg->uOffset + psList->psReg->uSizeInDWords, psList->psReg->uSizeInDWords);

			uTotal += psList->psReg->uSizeInDWords;
			psList = psList->psNext;
		}

		COMMENT(psFFGenCode, "Total = %d", uTotal);
	}
#endif

}

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  :  
 *****************************************************************************/
static FFGenReg *GetTemp(FFGenCode		*psFFGenCode,
						 IMG_UINT32		uSize,
						 IMG_UINT32		uLineNumber,
						 const IMG_CHAR	*pszFileName)
{
	/* Get pointer to temp list */
	FFGenRegList *psFreeList = psFFGenCode->psFreeTempList;
	FFGenReg     *psRetReg   = IMG_NULL;

	PVR_UNREFERENCED_PARAMETER(uLineNumber);
	PVR_UNREFERENCED_PARAMETER(pszFileName);

	/* Any free regs available */
	while(psFreeList)
	{
		/* Do the sizes match */
		if (psFreeList->psReg->uSizeInDWords == uSize)
		{
			/* return this register */
			psRetReg = psFreeList->psReg;

			/* remove from free list */
			if (psFreeList->psNext)
			{
				psFreeList->psNext->psPrev = psFreeList->psPrev;
			}

			if (psFreeList->psPrev)
			{
				psFreeList->psPrev->psNext =  psFreeList->psNext;
			}

			/* Are we removing the head entry? */
			if (psFreeList == psFFGenCode->psFreeTempList)
			{
				psFFGenCode->psFreeTempList = psFreeList->psNext;
			}

			/* remove this entry */
			FFGENFree(psFFGenCode->psFFGenContext, psFreeList);

			return psRetReg;
		}
		else if (psFreeList->psReg->uSizeInDWords > uSize)
		{
			/* Create a new reg */
			psRetReg = FFGENMalloc2(psFFGenCode->psFFGenContext, sizeof(FFGenReg), uLineNumber, pszFileName);

			*psRetReg = *(psFreeList->psReg);
	
			psRetReg->uSizeInDWords = uSize;

			/* Adjust current record */
			psFreeList->psReg->uSizeInDWords = psFreeList->psReg->uSizeInDWords - uSize;
			psFreeList->psReg->uOffset += uSize;

			return psRetReg;
		}


		/* Get next temp on list */
		psFreeList = psFreeList->psNext;
	}

	/* No free temp found so create a new one */
	psRetReg = FFGENMalloc2(psFFGenCode->psFFGenContext, sizeof(FFGenReg), uLineNumber, pszFileName);

	/* Setup reg */
	psRetReg->eType         = USEASM_REGTYPE_TEMP;
	psRetReg->uSizeInDWords = uSize;
	psRetReg->uOffset       = psFFGenCode->uCurrentTempSize;
	psRetReg->eBindingRegDesc  = 0;
	psRetReg->eWDFStatus    = 0;
	psRetReg->uIndex		= USEREG_INDEX_NONE;

	/* Increase size of temp allocs */
	psFFGenCode->uCurrentTempSize += uSize;

	/* Store maximum temp size */
	if (psFFGenCode->uCurrentTempSize > psFFGenCode->uTempSize)
	{
		psFFGenCode->uTempSize = psFFGenCode->uCurrentTempSize;
	}

	return psRetReg;
}

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  :  
 *****************************************************************************/
static  IMG_BOOL LoadStoreConstant(FFGenCode		*psFFGenCode,
									IMG_BOOL		bLoad,
									FFGenReg		*psConstReg,
									IMG_UINT32		uSizeInDWords,
									IMG_UINT32		uOffsetInDWords,
									FFGENIndexReg	*psIndexReg,
									FFGenReg		*psLoadDestStoreSrcReg,	/* Dest for load, src for store */
									const IMG_CHAR	*pszDesc,
									IMG_BOOL		bFCLFillLoad,
									IMG_BOOL		bIssueWDF,
									IMG_UINT32		uLineNumber,
									const IMG_CHAR	*pszFileName)
{
	FFGenReg *psTemp = psLoadDestStoreSrcReg;

	FFGenInstruction *psInst       = &(psFFGenCode->sInstruction);

	FFGenReg *psSABaseAddress      = &(psFFGenCode->sSAConstBaseAddress);
	FFGenReg *psImmediateIntReg    = &(psFFGenCode->sImmediateIntReg);
	FFGenReg *psImmediateIntReg2   = &(psFFGenCode->sImmediateIntReg2);

	IMG_UINT32 uDataSizeInDWords = uSizeInDWords;

	/* Integer because it might be negative */
	IMG_INT32 iDataOffsetInBytes  = (((psConstReg->uOffset + uOffsetInDWords) * sizeof(IMG_UINT32)) - 
									 psFFGenCode->iSAConstBaseAddressAdjust);

	IMG_INT32 iDataOffsetInDWords = iDataOffsetInBytes / sizeof(IMG_UINT32);

	if (!uSizeInDWords)
	{
		psFFGenCode->psFFGenContext->pfnPrint("LoadConstant(): No data to load\n");
	}

	if (iDataOffsetInBytes < 0)
	{
		psFFGenCode->psFFGenContext->pfnPrint("LoadConstant(): Can't handle negative values\n");
	}

	if(bLoad)
	{
		/* Setup register */
		psTemp->eBindingRegDesc = psConstReg->eBindingRegDesc;
		psTemp->eWDFStatus		= 0;
		psTemp->uIndex			= USEREG_INDEX_NONE;

		if (pszDesc)
		{
			COMMENT(psFFGenCode, "Load the %s from memory", pszDesc);
		}
	}
	else
	{
		if(pszDesc)
		{
			COMMENT(psFFGenCode, "Store the %s to memory", pszDesc);
		}
	}

	/* 
	   More optimal method that avoids the need for the address calculation 
	   Non incremental = Pre Increment with out the writeback of the address
	*/

	/* Can we do a non incremental load? */
	if (!psIndexReg                  &&
		(uDataSizeInDWords   <=  16) &&
		(iDataOffsetInDWords <  EURASIA_USE_LDST_MAX_IMM_ABSOLUTE_OFFSET)   &&
		(iDataOffsetInDWords >=   1))
	{
		/* Take off 4bytes (1 Dword) to compensate for preincrement */
		psImmediateIntReg->uOffset  = iDataOffsetInDWords - 1;

		if(bLoad)
		{	
			/* Get free DRC channel */
			psFFGenCode->sDRCReg.uOffset = GetDRC(psFFGenCode);

			/* Supply fetch count intead of repeat count */
			SET_FETCH_COUNT(uDataSizeInDWords);

			if(bFCLFillLoad)
			{
				SET_FORCE_CACHELINEFILL_LOAD();
			}

			/* ldad.f<uSizeInDwords> r[Const], [ r[addressreg], #(offset) ], DRC<DRCChannel> */
			INST4(LDAD, 
				  psTemp, 
				  psSABaseAddress, 
				  psImmediateIntReg,
				  &(psFFGenCode->sRangeReg), 
				  &(psFFGenCode->sDRCReg), 
				  IMG_NULL);

			if(bIssueWDF)
			{
				/* Issue data fence - this needs optimising */
				INST0(WDF, &(psFFGenCode->sDRCReg), IMG_NULL);

				/* Mark data channel is not outstanding anymore */
				psFFGenCode->abOutstandingDRC[psFFGenCode->sDRCReg.uOffset] = IMG_FALSE;
			}

		}
		else
		{
			IMG_UINT32 i, uBaseOffset = psImmediateIntReg->uOffset;

			psFFGenCode->sDRCReg.uOffset = GetDRC(psFFGenCode);
			
			for(i = 0; i < uDataSizeInDWords; i++)
			{
				SET_FETCH_COUNT(1);

				SET_INT_VALUE(psImmediateIntReg, uBaseOffset + i);
				SETSRCOFF(1, i);
				INST2(STAD,
					  psSABaseAddress,
					  psImmediateIntReg,
					  psTemp,
					  IMG_NULL);
			}

			/* Issue data fence */
			psImmediateIntReg->uOffset = EURASIA_USE1_IDF_PATH_ST;
			INST1(IDF, &(psFFGenCode->sDRCReg), psImmediateIntReg, IMG_NULL);

			if(bIssueWDF)
			{
				/* Issue data fence - this needs optimising */
				INST0(WDF, &(psFFGenCode->sDRCReg), IMG_NULL);

				/* Mark data channel is not outstanding anymore */
				psFFGenCode->abOutstandingDRC[psFFGenCode->sDRCReg.uOffset] = IMG_FALSE;
			}


		}
	}
	else
	{
		/* Get temp for address calc */
		FFGenReg *psAddress = GetTemp(psFFGenCode, 1, uLineNumber, pszFileName);

		/* Check reg  succeeded */
		if (!psAddress)
		{
			psFFGenCode->psFFGenContext->pfnPrint("Failed to alloc address reg\n");
			return IMG_FALSE;
		}

		
		if (psIndexReg)
		{
			IMG_UINT32 uStride;
			
			/* Work out stride */
			if (!psIndexReg->uStrideInDWords)
			{
				uStride = uSizeInDWords * sizeof(IMG_UINT32);
			}
			else
			{
				uStride = psIndexReg->uStrideInDWords * sizeof(IMG_UINT32);
			}
			
			/* Straightforward load? */
			if (uStride < 64)
			{
				/* Set up immediate value with size of item in bytes */
				psImmediateIntReg2->uOffset = uStride;
				
				/* Add in index register offset */
				if (psIndexReg->uIndexRegOffset)
				{
					SETSRCOFF(0, psIndexReg->uIndexRegOffset);
				}
				
				/* Multiply index by size of item */
				INST2(IMULU16, psAddress, psIndexReg->psReg, psImmediateIntReg2, IMG_NULL);
				
				if (psIndexReg->uIndexRegFlags)
				{
					/* Add flags to index register */
					SETSRCFLAG(0, psIndexReg->uIndexRegFlags);
				}
				
				/* iadd32 r[addressreg], r[indexReg].l/h, sa[constbaseaddress] */
				INST2(IADD32, psAddress, psAddress, psSABaseAddress, IMG_NULL);
			}
			/* Shift to get value */
			else if (IS_POWER_OF_2(uStride))
			{
				/* 
				   Calculate how much to shift value by 
				   +0.5f to get round precision errors 
				*/
				IMG_UINT32 uShift = (IMG_UINT32)(log(uStride)/log(2) + 0.5f);

				/* Set up immediate value with shift value */
				psImmediateIntReg2->uOffset = uShift;

				/* Add in index register offset */
				if (psIndexReg->uIndexRegOffset)
				{
					SETSRCOFF(0, psIndexReg->uIndexRegOffset);
				}

				/* Left shift the index register by the value */
				INST2(SHL, psAddress, psIndexReg->psReg, psImmediateIntReg2, IMG_NULL);

				if (psIndexReg->uIndexRegFlags)
				{
					/* Add flags to index register */
					SETSRCFLAG(0, psIndexReg->uIndexRegFlags);
				}

				/* iadd32 r[addressreg], r[indexReg].l/h, sa[constbaseaddress] */
				INST2(IADD32, psAddress, psAddress, psSABaseAddress, IMG_NULL);
			}
			else
			{
				psFFGenCode->psFFGenContext->pfnPrint("This load size currently not supported\n");
				return IMG_FALSE;
			}

			/* Does the offset need loading */
			if	(iDataOffsetInDWords >= EURASIA_USE_LDST_MAX_IMM_ABSOLUTE_OFFSET)
			{
				/* Get a temp to put the constant into */
				FFGenReg *psOffsetTemp = GetTemp(psFFGenCode, 1, uLineNumber, pszFileName);

				/* Check reg  succeeded */
				if (!psOffsetTemp)
				{
					psFFGenCode->psFFGenContext->pfnPrint("Failed to alloc offset reg \n");
					return IMG_FALSE;
				}

				/* Setup immediate value with data offset */
				psImmediateIntReg->uOffset  = (IMG_UINT32)iDataOffsetInBytes;

				/* Move constant off set into temp - mov r[addressreg], #constant_offset */
				INST1(LIMM, psOffsetTemp, psImmediateIntReg, IMG_NULL);

				/* Add the constant offset into the secondary base address register */
				SETSRCFLAG(0, USEASM_ARGFLAGS_LOW);

				/* iadd32 r[addressreg], r[addressreg].l, sa[constbaseaddress] */
				INST2(IADD32, psAddress, psOffsetTemp, psAddress, IMG_NULL);

				/* Free up offset reg */
				ReleaseReg(psFFGenCode, psOffsetTemp);

				/* Data offset has now been used */
				iDataOffsetInBytes  = 0;
				iDataOffsetInDWords = 0;
			}
		}
		else
		{
			/* Do we need to do the addition */
			if	(iDataOffsetInDWords < EURASIA_USE_LDST_MAX_IMM_ABSOLUTE_OFFSET)
			{
				INST1(MOV, psAddress, psSABaseAddress, IMG_NULL);
			}
			else 
			{
				/* Setup immediate value */
				psImmediateIntReg->uOffset  = (IMG_UINT32)iDataOffsetInBytes;

				/* Move constant off set into temp - mov r[addressreg], #constant_offset */
				INST1(LIMM, psAddress, psImmediateIntReg, IMG_NULL);
				
				/* Add the constant offset into the secondary base address register */
				SETSRCFLAG(0, USEASM_ARGFLAGS_LOW);
				
				/* iadd32 r[addressreg], r[addressreg].l, sa[constbaseaddress] */
				INST2(IADD32, psAddress, psAddress, psSABaseAddress, IMG_NULL);

				/* Data offset has now been used */
				iDataOffsetInBytes  = 0;
				iDataOffsetInDWords = 0;
			}
		}
	
		/* Max load count is 16 so need to break into chunks if larger */
		while (uDataSizeInDWords)
		{
			IMG_UINT32 uFetchCount; 
		
			/* Calc off set for data loads */
			IMG_UINT32 uDestOffset = uSizeInDWords - uDataSizeInDWords;

			uFetchCount = uDataSizeInDWords;

			/* Max fetch is 16 */
			if (uFetchCount > 16)
			{
				uFetchCount = 16;
			}

			/* update amount of data fetched */
			uDataSizeInDWords -= uFetchCount;
	
			SET_POST_INCREMENTAL_LOADSTORE();
			psImmediateIntReg->uOffset = iDataOffsetInDWords;
			SETDESTOFF(uDestOffset); 

			if(bLoad)
			{
				/* Get free DRC channel */
				psFFGenCode->sDRCReg.uOffset = GetDRC(psFFGenCode);

				SET_REPEAT_COUNT(uFetchCount);

				/* ldad.f<uSizeInDwords> r[Const], [ r[addressreg], #0++ ], DRC<DRCChannel> */

				if(bFCLFillLoad)
				{
					SET_FORCE_CACHELINEFILL_LOAD();
				}

				INST4(LDAD,
					  psTemp,
					  psAddress,
					  psImmediateIntReg,
					  &(psFFGenCode->sRangeReg),
					  &(psFFGenCode->sDRCReg),
					  IMG_NULL);

				if(bIssueWDF)
				{
					/* Issue data fence - this needs optimising */
					INST0(WDF, &(psFFGenCode->sDRCReg), IMG_NULL);

					/* Mark data channel is not outstanding anymore */
					psFFGenCode->abOutstandingDRC[psFFGenCode->sDRCReg.uOffset] = IMG_FALSE;
				}

			}
			else
			{
				IMG_UINT32 i;

				/* Get free DRC channel */
				psFFGenCode->sDRCReg.uOffset = GetDRC(psFFGenCode);

				for(i = 0; i < uFetchCount; i++)
				{


					SET_REPEAT_COUNT(1);

					SETSRCOFF(1, i);
					/* stad [Base, #offset++], r[] */
					INST2(STAD,
						  psSABaseAddress,
						  psImmediateIntReg,
						  psTemp,
						  IMG_NULL);


				}

				/* Issue data fence - this needs optimising */
				psImmediateIntReg->uOffset = EURASIA_USE1_IDF_PATH_ST;
				INST1(IDF, &(psFFGenCode->sDRCReg), psImmediateIntReg, IMG_NULL);

				if(bIssueWDF)
				{
					/* Issue data fence - this needs optimising */
					INST0(WDF, &(psFFGenCode->sDRCReg), IMG_NULL);

					/* Mark data channel is not outstanding anymore */
					psFFGenCode->abOutstandingDRC[psFFGenCode->sDRCReg.uOffset] = IMG_FALSE;
				}

			}
		
		}
		/* Free up address reg */
		ReleaseReg(psFFGenCode, psAddress);
	}
	

	return IMG_TRUE;
}


static FFGenReg *LoadConstant(FFGenCode			*psFFGenCode,
							  FFGenReg			*psConstReg,
							  IMG_UINT32		uSizeInDWords,
							  IMG_UINT32		uLoadOffsetInDWords,
							  FFGENIndexReg		*psIndexReg,
							  const IMG_CHAR	*pszDesc,
							  IMG_BOOL			bFCLFillLoad,
							  IMG_UINT32		uLineNumber,
							  const IMG_CHAR	*pszFileName)
{
	/* Get a temp to put the constant into */
	FFGenReg *psTemp = GetTemp(psFFGenCode, uSizeInDWords, uLineNumber, pszFileName);

	/* Check reg  succeeded */
	if (!psTemp)
	{
		psFFGenCode->psFFGenContext->pfnPrint("Failed to alloc const reg\n");
		return IMG_NULL;
	}

	LoadStoreConstant(psFFGenCode,
					  IMG_TRUE,
					  psConstReg,
					  uSizeInDWords,
					  uLoadOffsetInDWords,
					  psIndexReg,
					  psTemp,
					  pszDesc,
					  bFCLFillLoad,
					  IMG_TRUE,			/* Issue WDF immediately - get data back immediately */
					  uLineNumber,
					  pszFileName);

	return psTemp;

}

/******************************************************************************
 * Function Name: RelativeAddressingSA
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Sets up HW index register to allow relative addressing into
 *				  a secondary attribute array.
 *****************************************************************************/

static FFGenReg *RelativeAddressingSA(FFGenCode       *psFFGenCode,
									  FFGenReg        *psReg,
									  IMG_UINT32       uSize,
									  FFGENIndexReg   *psIndexReg,
									  const IMG_CHAR  *pszDesc,
									  IMG_UINT32       uLineNumber,
									  const IMG_CHAR  *pszFileName)
{

	FFGenInstruction *psInst       = &(psFFGenCode->sInstruction);
	FFGenReg *psRetReg;
	FFGenReg *psImmediateIntReg    = &(psFFGenCode->sImmediateIntReg);
	FFGenReg *psIntSrcSelReg = &psFFGenCode->sIntSrcSelReg;
	FFGenReg *psHWIndexReg = &psFFGenCode->sIndexLowReg;
	FFGenReg sIntSrc2SelReg = *psIntSrcSelReg;
	FFGenReg *psTemp;
	
	PVR_UNREFERENCED_PARAMETER(pszDesc);

	/* Get a temp to put the constant into */
	psTemp = GetTemp(psFFGenCode, 1, uLineNumber, pszFileName);

	/* Check reg  succeeded */
	if (!psTemp)
	{
		psFFGenCode->psFFGenContext->pfnPrint("Failed to alloc const reg\n");
		return IMG_NULL;
	}
	
	/* Alloc space for reg */
	psRetReg = FFGENMalloc(psFFGenCode->psFFGenContext, sizeof(FFGenReg));
	
	/* Check reg  succeeded */
	if (!psRetReg)
	{
		psFFGenCode->psFFGenContext->pfnPrint("Failed to malloc register\n");
		return IMG_NULL;
	}

	/* Set temp to base of these indexable SAs */
	psImmediateIntReg->uOffset  = psReg->uOffset;

	/* Mark this instruction for index patching - in case we move the indexable secondaries at the end */
	SET_INDEXPATCH(1);
	INST1(LIMM, psTemp, psImmediateIntReg, "mov input offset into temp");
	SET_INDEXPATCH(0);

	/* Set src selection to U32 */
	SET_INTSRCSEL(psIntSrcSelReg, USEASM_INTSRCSEL_U32);
	
	/* Set src2 selection to None */
	SET_INTSRCSEL(&sIntSrc2SelReg, USEASM_INTSRCSEL_NONE);
	
	psImmediateIntReg->uOffset  = psIndexReg->uStrideInDWords;

	/* Add in index register offset */
	if (psIndexReg->uIndexRegOffset)
	{
		SETSRCOFF(0, psIndexReg->uIndexRegOffset);
	}
	
	/* Add in index register low/high */
	if(psIndexReg->uIndexRegFlags == USEASM_ARGFLAGS_HIGH)
	{
		SETSRCFLAG(0, USEASM_ARGFLAGS_HIGH);
	}
	else
	{
		SETSRCFLAG(0, USEASM_ARGFLAGS_LOW);
	}

	/* HWIndex = SWindex.l/h * stride + temp */
	INST6(IMAE, psHWIndexReg, psIndexReg->psReg, psImmediateIntReg, psTemp, psIntSrcSelReg, &sIntSrc2SelReg, &sIntSrc2SelReg, "input index * stride + offset");

	psRetReg->eType         = USEASM_REGTYPE_SECATTR;
	psRetReg->uOffset       = 0;
	psRetReg->uSizeInDWords = uSize;
	psRetReg->eBindingRegDesc  = psReg->eBindingRegDesc;
	psRetReg->eWDFStatus    = 0;
	psRetReg->uIndex		= USEREG_INDEX_L;

	/* add to throw away list */
	psFFGenCode->psThrowAwayList = AddRegToList(psFFGenCode,
												psFFGenCode->psThrowAwayList,
												psRetReg,
												IMG_FALSE,
												uLineNumber,
												pszFileName);

	/* Free up temp reg */
	ReleaseReg(psFFGenCode, psTemp);

	return psRetReg;
}

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : This function is horrible, probably need to split the constant stuff out 
 *****************************************************************************/
IMG_INTERNAL FFGenReg *GetRegfn(FFGenCode  	  *psFFGenCode,
								UseasmRegType  eType,
								FFGenRegDesc   eBindingRegDesc,
								IMG_UINT32     uLoadOffsetInDWords,
								FFGENIndexReg *psIndexReg,
								IMG_UINT32     uSize,
								IMG_CHAR      *pszDesc,
								IMG_BOOL       bAllocSpaceOnly,
								IMG_BOOL       bIndexableSecondary,
								IMG_BOOL	   bFCLFillLoad,
								IMG_UINT32     uLineNumber,
								IMG_CHAR      *pszFileName)
{
	FFGenReg *psReg, *psRetReg = IMG_NULL;

	FFGenRegList **ppsList, *psList;

	IMG_UINT32 *puCurrentAllocSize = IMG_NULL;

	IMG_BOOL   bConstantLoadRequired = IMG_FALSE;

	if (eType != USEASM_REGTYPE_SECATTR && uLoadOffsetInDWords)
	{
		psFFGenCode->psFFGenContext->pfnPrint("GetReg: Load offsets only valid for constants (%s, %d)\n", pszFileName, uLineNumber);
		return IMG_NULL;
	}

	if (!uSize)
	{
		psFFGenCode->psFFGenContext->pfnPrint("GetReg: Size was 0 (%s, %d)\n", pszFileName, uLineNumber);
		return IMG_NULL;
	}

	switch (eType)
	{
		case USEASM_REGTYPE_OUTPUT:
			puCurrentAllocSize = &(psFFGenCode->uOutputSize);
			ppsList = &(psFFGenCode->psOutputsList);
			break;
		case USEASM_REGTYPE_PRIMATTR:
			puCurrentAllocSize = &(psFFGenCode->uInputSize);
			ppsList = &(psFFGenCode->psInputsList);
	
			if(psFFGenCode->eCodeGenFlags & FFGENCGF_INPUT_REG_SIZE_4)
			{
				if (uSize != 4)
				{
#if !defined(OGLES1_MODULE)
					/* This is benign in opengles - don't emit the error */
					psFFGenCode->psFFGenContext->pfnPrint("GetReg: Only valid input register size for opengl is 4, automatically adjusting (%s, %d)\n", 
									  pszFileName, 
									  uLineNumber);
#endif
					uSize = 4;
				}
			}

			break;
		case USEASM_REGTYPE_SECATTR:
			ppsList = &(psFFGenCode->psConstantsList);
			break;
		case USEASM_REGTYPE_TEMP:
			/* Get a free temp  */
			return GetTemp(psFFGenCode, uSize, uLineNumber, pszFileName);
		default:
			psFFGenCode->psFFGenContext->pfnPrint("GetReg: Invalid reg type (%s, %d)\n", pszFileName, uLineNumber);
			return IMG_NULL;
	}

	/* Reset list */
	psList = *ppsList;

	/* Search through input list to see if this has already been allocated */
	while (psList && !psRetReg)
	{
		psReg = psList->psReg;

		/* Get next entry */
		psList = psList->psNext;

		/* Does binding info match */
		if (psReg->eBindingRegDesc == eBindingRegDesc)
		{
			/* Can't increase intial alloc */
			if (uSize > psReg->uSizeInDWords)
			{
				psFFGenCode->psFFGenContext->pfnPrint("GetReg: Can't reget register with larger size than initial get or alloc (%s, %d)!\n", pszFileName, uLineNumber);
				return IMG_NULL;
			}

			/* Can't reserve same space twice */
			if (bAllocSpaceOnly)
			{
				psFFGenCode->psFFGenContext->pfnPrint("GetReg: Can't alloc space for the same register twice\n");
				return IMG_NULL;
			}

			/* Is this an exact match for the existing register? */
			if (psReg->uSizeInDWords == uSize)
			{
				/* 
				   If we've found a match for a constant but the contents were stored in a temp then we can't rely
				   on them still being there. Could improve this by addding some info to say if the contents of a 
				   temp still held the relavant constant info. 
				*/
				if (eType == USEASM_REGTYPE_SECATTR && psReg->eType == USEASM_REGTYPE_TEMP)
				{
					if (uLoadOffsetInDWords || psIndexReg)
					{
						psFFGenCode->psFFGenContext->pfnPrint("GetReg: If sizes match for existing constant reg then load offset and index reg should be 0\n");
						return IMG_NULL;
					}


					return LoadConstant(psFFGenCode,
										psReg,
										uSize,
										uLoadOffsetInDWords,
										psIndexReg,
										pszDesc,
										bFCLFillLoad,
										uLineNumber,
										pszFileName);
				}
				else
				{
					return psReg;
				}
			}
			else
			{
				if (eType == USEASM_REGTYPE_SECATTR)
				{
					/* Check load offset won't take it out of range */
					if (uLoadOffsetInDWords + uSize > psReg->uSizeInDWords)
					{
						psFFGenCode->psFFGenContext->pfnPrint("GetReg: Load offset + size (%d + %d = %d) exceeds size of original alloc (%d) (%s, %d)!\n", 
									 uSize,
									 uLoadOffsetInDWords,
									 uSize + uLoadOffsetInDWords,
									 psReg->uSizeInDWords,
									 pszFileName, 
									 uLineNumber);
						return IMG_NULL;
					}

					/* Do a partial load of this constant */
					if (psReg->eType == USEASM_REGTYPE_TEMP)
					{
						return LoadConstant(psFFGenCode,
											psReg,
											uSize,
											uLoadOffsetInDWords,
											psIndexReg,
											pszDesc,
											bFCLFillLoad,
											uLineNumber,
											pszFileName);
					}
					/* Create a copy of an area from the original sec attr alloc, or use indexable sec attr */
					else if (psReg->eType == USEASM_REGTYPE_SECATTR)
					{
						if (psIndexReg)
						{
#ifdef FFGEN_UNIFLEX
							if(IF_FFGENCODE_UNIFLEX)
							{
								/* We are indexing a section of the palette.
								   FFGEN_UNIFLEX uses the relative address register, so just return the whole palette. */
								return psReg;
							}
#endif
							return RelativeAddressingSA(psFFGenCode,
														psReg,
														uSize,
														psIndexReg,
														pszDesc,
														uLineNumber,
														pszFileName); 
								
						}

						/* Alloc space for reg */
						psRetReg = FFGENMalloc(psFFGenCode->psFFGenContext, sizeof(FFGenReg));

						psRetReg->eType         = USEASM_REGTYPE_SECATTR;
						psRetReg->uOffset       = psReg->uOffset + uLoadOffsetInDWords;
						psRetReg->uSizeInDWords = uSize;
						psRetReg->eBindingRegDesc  = eBindingRegDesc;
						psRetReg->eWDFStatus    = 0;
						psRetReg->uIndex		= USEREG_INDEX_NONE;

						/* add to throw away list */
						psFFGenCode->psThrowAwayList = AddRegToList(psFFGenCode,
																	psFFGenCode->psThrowAwayList,
																	psRetReg,
																	IMG_FALSE,
																	uLineNumber,
																	pszFileName);

						return psRetReg;
					}
				}
				else
				{
					psFFGenCode->psFFGenContext->pfnPrint("GetReg: Can only reget constants with a different size from original get/alloc (%s, %d)\n", 
									  pszFileName, uLineNumber);
					return IMG_NULL;
				}
			}
		}
	}

	/* Check no offset has been set */
	if (uLoadOffsetInDWords)
	{
		psFFGenCode->psFFGenContext->pfnPrint("GetReg: Load offsets not valid for 1st get/alloc (%s, %d)\n", pszFileName, uLineNumber);
		return IMG_NULL;
	}

	if (psIndexReg)
	{
		psFFGenCode->psFFGenContext->pfnPrint("GetReg: Reg space must be reserved using AllocRegSpace() before a relative load can be executed(%s, %d)\n", 
						  pszFileName, uLineNumber);
		return IMG_NULL;
	}

	/* Reset list pointer */
	psList = *ppsList;

	/* If we're trying to get a constant we need to figure out where to get it from */
	if (eType == USEASM_REGTYPE_SECATTR)
	{
		IMG_BOOL bFitInSecondaries = IMG_FALSE;

		/* Alloc space for reg */
		psRetReg = FFGENMalloc(psFFGenCode->psFFGenContext, sizeof(FFGenReg));

#ifdef FFGEN_UNIFLEX
		if(IF_FFGENCODE_UNIFLEX)
		{
			/* USC compiler deals with memory allocation, 
			   so just pretend that we always have enough space for secondary attributes */
			goto UseSecondaries;
		}
#endif
		/* Can we put this constant into the secondary attributes? */
		if(bIndexableSecondary)
		{
			if((psFFGenCode->uHighSecAttribSize + uSize + EURASIA_USE_REGISTER_NUMBER_MAX) <= psFFGenCode->uMaxSecAttribSize)
			{
				psRetReg->eType         =  USEASM_REGTYPE_SECATTR;
				psRetReg->eWDFStatus    =  0;
				psRetReg->uOffset       =  psFFGenCode->uHighSecAttribSize + EURASIA_USE_REGISTER_NUMBER_MAX;
				psRetReg->uSizeInDWords =  uSize; 
				psRetReg->eBindingRegDesc  =  eBindingRegDesc;
				psRetReg->eWDFStatus    =  0;
				psRetReg->uIndex		= USEREG_INDEX_NONE;

				/* Increase size of regs alloced  */
				psFFGenCode->uHighSecAttribSize += uSize;

				bFitInSecondaries = IMG_TRUE;

				/* add to Indexable Secondary list */
				psFFGenCode->psIndexableSecondaryList = AddRegToList(psFFGenCode,
																	psFFGenCode->psIndexableSecondaryList,
																	psRetReg,
																	IMG_FALSE,
																	uLineNumber,
																	pszFileName);

			}
			
		}
		else if (psFFGenCode->uSecAttribSize + uSize <= EURASIA_USE_REGISTER_NUMBER_MAX)
		{
#ifdef FFGEN_UNIFLEX
UseSecondaries:
			if(IF_FFGENCODE_UNIFLEX)
			{
				/*	If we are using FFGen to make UniFlex instructions, then we always start on a vector 4 boundary,
				so that vector UniFlex instructions work. Otherwise, a vector could span across a boundary. */
				if(psFFGenCode->uSecAttribSize % 4)
					psFFGenCode->uSecAttribSize += 4 - (psFFGenCode->uSecAttribSize % 4);
			}
#endif

			psRetReg->eType         =  USEASM_REGTYPE_SECATTR;
			psRetReg->eWDFStatus    =  0;
			psRetReg->uOffset       =  psFFGenCode->uSecAttribSize;
			psRetReg->uSizeInDWords =  uSize; 
			psRetReg->eBindingRegDesc  =  eBindingRegDesc;
			psRetReg->eWDFStatus    =  0;
			psRetReg->uIndex		= USEREG_INDEX_NONE;

			/* Increase size of regs alloced  */
			psFFGenCode->uSecAttribSize += uSize;
		
			bFitInSecondaries = IMG_TRUE;
		}
		
		/* Nope will have to go into the temporaries */
		if(!bFitInSecondaries)
		{
			psRetReg->eType         =  USEASM_REGTYPE_TEMP;
			psRetReg->uOffset       =  psFFGenCode->uMemoryConstantsSize;
			psRetReg->uSizeInDWords =  uSize; 
			psRetReg->eBindingRegDesc  =  eBindingRegDesc;
			psRetReg->eWDFStatus    =  0;
			psRetReg->uIndex		= USEREG_INDEX_NONE;

			/* Increase size of regs alloced  */
			psFFGenCode->uMemoryConstantsSize += uSize;

			/* This constant needs loading */
			if (!bAllocSpaceOnly)
			{
				bConstantLoadRequired = IMG_TRUE;
			}
		}
	}
	else if ((eType == USEASM_REGTYPE_OUTPUT) &&
			 (psFFGenCode->eCodeGenFlags & FFGENCGF_REDIRECT_OUTPUT_TO_INPUT))
	{
		/* redirect output reg to temp - will get copied out at end */
		psRetReg =  GetTemp(psFFGenCode, uSize, uLineNumber, pszFileName);
	
		/* Not normally set up for temps */
		psRetReg->eBindingRegDesc	=  eBindingRegDesc;
		psRetReg->eWDFStatus		=  0;
		psRetReg->uIndex			= USEREG_INDEX_NONE;

		/* Increase size of regs alloced */
		*puCurrentAllocSize += uSize;
	}
	else
	{
		/* Alloc space for reg */
		psRetReg = FFGENMalloc(psFFGenCode->psFFGenContext, sizeof(FFGenReg));

		/* Setup the register */
		psRetReg->eType         =  eType;
		psRetReg->uOffset       = *puCurrentAllocSize;
		psRetReg->uSizeInDWords =  uSize; 
		psRetReg->eBindingRegDesc  =  eBindingRegDesc;
		psRetReg->eWDFStatus    =  0;
		psRetReg->uIndex		= USEREG_INDEX_NONE;
		
		/* Increase size of regs alloced  */
		*puCurrentAllocSize += uSize;
	}
	
	/* Add this register to the relevant list */
	*ppsList = AddRegToList(psFFGenCode, psList, psRetReg, IMG_FALSE, uLineNumber, pszFileName);

	/* Do we need to perform a constant load? */
	if (bConstantLoadRequired)
	{
		return LoadConstant(psFFGenCode, 
							psRetReg, 
							uSize, 
							uLoadOffsetInDWords, 
							psIndexReg, 
							pszDesc, 
							bFCLFillLoad,
							uLineNumber, 
							pszFileName);
	}

	return psRetReg;
}


/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL StoreRegfn(FFGenCode		*psFFGenCode,
								 FFGenRegDesc	eBindingRegDesc,
								 FFGENIndexReg	*psIndexReg,
								 IMG_UINT32		uOffsetInDWords,
								 IMG_UINT32		uSizeInDWords,
								 FFGenReg		*psStoreSrc,
								 IMG_BOOL		bIssueWDFImmediately,
								 IMG_CHAR		*pszDesc,
								 IMG_UINT32		uLineNumber,
								 IMG_CHAR		*pszFileName)
{
	FFGenReg *psReg, *psRetReg = IMG_NULL;

	FFGenRegList **ppsList, *psList;
	
	if (!uSizeInDWords)
	{
		psFFGenCode->psFFGenContext->pfnPrint("StoreReg: Size was 0 (%s, %d)\n", pszFileName, uLineNumber);
		return IMG_FALSE;
	}

	/* Get constant list */
	ppsList = &(psFFGenCode->psConstantsList);

	/* Reset list */
	psList = *ppsList;

	/* Search through input list to see if this has already been allocated */
	while (psList && !psRetReg)
	{
		psReg = psList->psReg;

		/* Get next entry */
		psList = psList->psNext;

		/* Does binding info match */
		if (psReg->eBindingRegDesc == eBindingRegDesc)
		{
			/* Check whether size is out of range. */
			if(uOffsetInDWords + uSizeInDWords > psReg->uSizeInDWords)
			{
				psFFGenCode->psFFGenContext->pfnPrint("StoreReg: size + offset is greater than original alloc size(%s, %d)!\n", pszFileName, uLineNumber);
				return IMG_FALSE;
			}

			if(psReg->eType == USEASM_REGTYPE_TEMP)
			{
				LoadStoreConstant(psFFGenCode,
								  IMG_FALSE,		/* bLoad ? not, it is a store */
								  psReg,			/* psConstReg */
								  uSizeInDWords,	/* size to store */
								  uOffsetInDWords,	/* Offset */
								  psIndexReg,		/* Supplied index reg ? */
								  psStoreSrc,		/* Store from */
								  pszDesc,
								  IMG_FALSE,		/* bFCLFillLoad? not related */
								  bIssueWDFImmediately,
								  uLineNumber,
								  pszFileName);

				
				return IMG_TRUE;
			}
			else if(psReg->eType == USEASM_REGTYPE_SECATTR)
			{
				psFFGenCode->psFFGenContext->pfnPrint("StoreReg: Can not store to secondary attribute");

				return IMG_FALSE;
			}
		}
	}

	psFFGenCode->psFFGenContext->pfnPrint("StoreReg: Original register is not allocated (%s, %d)\n", pszFileName, uLineNumber);

	return IMG_FALSE;
	
}
/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  :  
 *****************************************************************************/
IMG_INTERNAL IMG_VOID DestroyRegList(FFGenContext *psFFGenContext, FFGenRegList *psList, IMG_BOOL bFreeReg)
{
	while (psList)
	{
		FFGenRegList *psNext = psList->psNext;

		if (bFreeReg)
		{
#if defined(OGLES1_MODULE) && defined(FFGEN_UNIFLEX)
			if(psList->psReg->pui32SrcOffset)
			{
				FFGENFree(psFFGenContext, psList->psReg->pui32SrcOffset);
			}

			if(psList->psReg->pui32DstOffset)
			{
				FFGENFree(psFFGenContext, psList->psReg->pui32DstOffset);
			}
#endif /* defined(OGLES1_MODULE) && defined(FFGEN_UNIFLEX) */

			/* free the register */
			FFGENFree(psFFGenContext, psList->psReg);
		}

		/* Free the entry */
		FFGENFree(psFFGenContext, psList);

		psList = psNext;
	}
}

/******************************************************************************
 End of file (reg.c)
******************************************************************************/
