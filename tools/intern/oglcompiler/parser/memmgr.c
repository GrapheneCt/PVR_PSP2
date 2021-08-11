/**************************************************************************
 * Name         : memmgr.c
 * Author       : James McCarthy
 * Created      : 23/03/2005
 *
 * Copyright    : 2000-2004 by Imagination Technologies Limited. All rights reserved.
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
 * Platform     : ANSI
 *
 * Modifications:-
 * $Log: memmgr.c $
 **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memmgr.h"
#include "debug.h"

#ifdef USE_MEMCHK

#pragma warning ( disable : 4100 4115 4201 4209 4214 4514 )
#include <windows.h>
#pragma warning ( 4 : 4001 4100 4115 4201 4209 4214 )

extern HANDLE 	MemChkHeapCreate(DWORD flOptions, DWORD dwInitialSize, DWORD dwMaximumSize);
extern BOOL 	MemChkHeapDestroy(HANDLE hHeap);
extern PVOID 	MemChkMalloc(DWORD dwSize, DWORD dwReturnAddress);
extern PVOID 	MemChkCalloc(DWORD dwNumItems, DWORD dwItemSize, DWORD dwReturnAddress);
extern PVOID 	MemChkRealloc(PVOID pvMem, DWORD dwNewSize, DWORD dwReturnAddress);
extern void 	MemChkFree(PVOID pvMem);

#define GetReturnAddress(DEST)  __asm mov eax,ss:[ebp+4] \
								__asm mov DEST,eax


HANDLE hMemChk;

IMG_BOOL bMemChkInit = IMG_FALSE;

static void MemChkInit()
{
	hMemChk = MemChkHeapCreate(0, 0x2000, 0);
}

void MemChkDestroy()
{
	MemChkHeapDestroy(hMemChk);
}
	
	

#endif

#define MARKER 0xDEADBED5

typedef struct MemAllocRecordTAG *PMemAllocRecord;

typedef struct MemAllocTAG
{
	PMemAllocRecord  psRecord;
	IMG_UINT32         uMarker;
	IMG_BYTE         pbMem[1];
} MemAlloc;


typedef struct MemAllocRecordTAG
{
	IMG_CHAR *AllocInfo;
	IMG_UINT32  uAllocInfoSize;
	MemAlloc *psMemAlloc;
	IMG_UINT32  uSize;
	PMemAllocRecord psPrev;
	PMemAllocRecord psNext;
	IMG_UINT32 uID;
} MemAllocRecord;

typedef struct MemAllocLogTAG *PMemAllocLog;

typedef struct MemAllocLogTAG
{
	IMG_CHAR *pszAllocInfo;
	IMG_INT32  nTotalSizeInBytes;
	IMG_UINT32  uNumAllocs;
	IMG_INT32  nLargestAlloc;
	IMG_INT32  nSmallestAlloc;
	IMG_INT32  nCurrentSizeInBytes;
	IMG_INT32  nMaxSizeInBytes;
	PMemAllocLog psPrev;
	PMemAllocLog psNext;
	IMG_UINT32 uAllocID;
} MemAllocLog;

#ifdef USE_MEMMGR_ALLOC_TRACKING

/* None of this stuff is thread safe */
MemAllocLog *gpsMemAllocLog = IMG_NULL;
PMemAllocRecord gpsMemAllocRecord = IMG_NULL;
IMG_UINT32 guMemStatsNumRecords = 0;
IMG_UINT32 guMemStatsNumAllocs = 0;
IMG_UINT32 guMemStatsNumReallocs = 0;
IMG_UINT32 guMemStatsNumFrees = 0;
IMG_UINT32 guMemStatsTotalNumSearches = 0;
IMG_UINT32 guMemStatsTotalNumFinds = 0;
IMG_UINT32 guMemStatsCurrentMemAllocated = 0;
IMG_UINT32 guMemStatsMaxNumRecords = 0;
IMG_UINT32 guMemStatsMaxMemAllocated = 0;
IMG_UINT32 guMemStatsTotalMemAllocated = 0;
IMG_UINT32 guMemCheckStart = 0;
IMG_UINT32 guMemCheckEnd   = 0;

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : -
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static IMG_VOID *GetOriginalAlloc(IMG_VOID *pvMem)
{
	IMG_BYTE *pbMem = pvMem;

	/* 
	   This could cause us problems if memory for structures is not packed as anticipated 
	   Could solve by doing an initial alloc and calculating the offset 
	*/
    MemAlloc *psMemAlloc = (MemAlloc *)(pbMem - (sizeof(MemAlloc) - sizeof(IMG_UINT32)));

	if (psMemAlloc->uMarker != MARKER)
	{
		return IMG_NULL;
	}

	return (IMG_VOID *)psMemAlloc;
}


#if 0
/******************************************************************************
 * Function Name: 
 *
 * Inputs       : -
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static MemAllocRecord *SlowFindAlloc(IMG_VOID *pvMem, IMG_UINT32  uLineNumber, const IMG_CHAR *pszFileName)
{
	MemAllocRecord *psRecord = gpsMemAllocRecord;
	IMG_UINT32 uNumSearches = 0;

    MemAlloc *psMemAlloc = (MemAlloc *)GetOriginalAlloc(pvMem);
	
	guMemStatsTotalNumFinds++;

	while (psRecord)
	{
		if (psRecord->psMemAlloc == psMemAlloc)
		{
			guMemStatsTotalNumSearches += uNumSearches;
			return psRecord;
		}
		else
		{
			psRecord = psRecord->psNext;
			
			if (uNumSearches > guMemStatsNumRecords)
			{
				DEBUG_MESSAGE(("FindAlloc: Error, could not find alloc record Line: %d, File %s!\n Record list also appears to be corrupted",
							 uLineNumber, pszFileName));
				return IMG_NULL;
			}
			
			uNumSearches++;
		}
	}

	DEBUG_MESSAGE(("FindAlloc: Error, could not find alloc record Line: %d, File %s!\n",uLineNumber, pszFileName));

	return IMG_NULL;
}
#endif

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : -
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static MemAllocRecord *FindAlloc(IMG_VOID *pvMem, IMG_UINT32  uLineNumber, const IMG_CHAR *pszFileName)
{
	MemAllocRecord *psRecord;
	IMG_UINT32 uNumSearches = 0;

    MemAlloc *psMemAlloc = (MemAlloc *)GetOriginalAlloc(pvMem);
	
	if (!psMemAlloc)
	{
		DEBUG_MESSAGE(("FindAlloc: No record found - Line: %d, File %s\n",uLineNumber, pszFileName));
		return IMG_NULL;
	}

	guMemStatsTotalNumFinds++;

	psRecord = psMemAlloc->psRecord;


	uNumSearches++;
	
	if (psRecord->psMemAlloc == psMemAlloc)
	{

#if 0
		MemAllocRecord *psVerifyRecord = SlowFindAlloc(pvMem, uLineNumber, pszFileName);

		if (psRecord != psVerifyRecord)
		{
			DEBUG_MESSAGE(("FindAlloc: Fast/slow mismatch: %d, File %s!\n",uLineNumber, pszFileName));
		}

#endif

		return psRecord;
	}

	DEBUG_MESSAGE(("FindAlloc: Error, could not find alloc record Line: %d, File %s!\n",uLineNumber, pszFileName));
	
	return IMG_NULL;
}

#ifdef DUMP_LOGFILES

IMG_UINT32 uNumLogs = 0;

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : -
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static IMG_VOID LogMemoryAlloc(IMG_INT32   nSizeInBytes, 
								IMG_CHAR *pszAllocInfo)
{
	MemAllocLog *psCurrent = gpsMemAllocLog;

	/* Search for a matching record */
	while (psCurrent)
	{
		/* Does line number and file name match? */
		if ((strcmp(psCurrent->pszAllocInfo, pszAllocInfo) == 0))
		{
			psCurrent->nCurrentSizeInBytes += nSizeInBytes;

			if (psCurrent->nCurrentSizeInBytes > psCurrent->nMaxSizeInBytes)
			{
				psCurrent->nMaxSizeInBytes = psCurrent->nCurrentSizeInBytes;
			}

			/* Don't register frees */
			if (nSizeInBytes > 0)
			{
				psCurrent->nTotalSizeInBytes += nSizeInBytes;

				psCurrent->uNumAllocs++;

				if (nSizeInBytes > psCurrent->nLargestAlloc)
				{
					psCurrent->nLargestAlloc = nSizeInBytes ;
				}
				
				if (nSizeInBytes < psCurrent->nSmallestAlloc)
				{
					psCurrent->nSmallestAlloc = nSizeInBytes ;
				}
			}
		
			break;
		}
		else
		{
			psCurrent = psCurrent->psNext;
		}
	}		
		
	/* No record found create a new one */
	if (!psCurrent)
	{
		psCurrent = malloc(sizeof(MemAllocLog));
				
		psCurrent->pszAllocInfo = malloc(strlen(pszAllocInfo) + 1);

		uNumLogs++;

		if (nSizeInBytes < 0)
		{
			DEBUG_MESSAGE(("Alloc from %s was -ve %d %d\n", pszAllocInfo, nSizeInBytes));
		}
		
		strcpy(psCurrent->pszAllocInfo, pszAllocInfo);
		psCurrent->uAllocID            = uNumLogs;
		psCurrent->nTotalSizeInBytes   = nSizeInBytes;
		psCurrent->uNumAllocs          = 1;
		psCurrent->nLargestAlloc       = nSizeInBytes;
		psCurrent->nSmallestAlloc      = nSizeInBytes;
		psCurrent->nCurrentSizeInBytes = nSizeInBytes;
		psCurrent->nMaxSizeInBytes     = nSizeInBytes;
		
		if (gpsMemAllocLog)
		{
			gpsMemAllocLog->psPrev = psCurrent;
		}
		
		psCurrent->psPrev = IMG_NULL;
		psCurrent->psNext = gpsMemAllocLog;
		gpsMemAllocLog = psCurrent;
	}
}


/******************************************************************************
 * Function Name: 
 *
 * Inputs       : -
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_VOID DebugDumpMemoryAllocLog()
{
	MemAllocLog *psCurrent;

	IMG_BOOL bSwapped = IMG_TRUE;

	if (!gpsMemAllocLog)
	{
		return;
	}

	psCurrent = gpsMemAllocLog;

	DumpLogMessage(LOGFILE_MEMORY_STATS,
				   0,
				   "In Allocation Order\n\n%5s | %-65s | %10s | %13s | %13s | %17s | %17s | %17s | %17s \n\n",
				   "ID",
				   "Line  | FileName",
				   "Num Allocs",
				   "Size in bytes",
				   "Max in bytes",
				   "AVG size in bytes",
				   "Smallest in bytes",
				   "Largest  in bytes",
				   "Current  in bytes");
	
	while (psCurrent)
	{
		DumpLogMessage(LOGFILE_MEMORY_STATS,
					   0,
					   "%5d | %-65s | %10d | %13d | %13d | %17d | %17d | %17d | %17d\n",
					   psCurrent->uAllocID,
					   psCurrent->pszAllocInfo,
					   psCurrent->uNumAllocs,
					   psCurrent->nTotalSizeInBytes,
					   psCurrent->nMaxSizeInBytes,
					   psCurrent->nTotalSizeInBytes / psCurrent->uNumAllocs,
					   psCurrent->nSmallestAlloc,
					   psCurrent->nLargestAlloc,
					   psCurrent->nCurrentSizeInBytes);
		
		psCurrent = psCurrent->psNext;
	}


	/* Sort the list into largest size first */
	while (bSwapped)
	{
		psCurrent = gpsMemAllocLog;

		bSwapped = IMG_FALSE;

		
		while (psCurrent->psNext)
		{
			MemAllocLog *psNext = psCurrent->psNext;

			/* Swap entries if larger */
			if (psNext->nTotalSizeInBytes > psCurrent->nTotalSizeInBytes)
			{
				MemAllocLog *psPrev = psCurrent->psPrev;

				psCurrent->psNext = psNext->psNext;
				psCurrent->psPrev = psNext;

				if (psNext->psNext)
				{
					psNext->psNext->psPrev = psCurrent;
				}

				psNext->psNext = psCurrent;
				psNext->psPrev = psPrev;

				if (psPrev)
				{
					psPrev->psNext = psNext;
				}

				if (gpsMemAllocLog == psCurrent)
				{
					gpsMemAllocLog = psNext;
				}
				
				bSwapped = IMG_TRUE;
			}
			else
			{
				psCurrent = psCurrent->psNext;
			}
		}
	}
	
	psCurrent = gpsMemAllocLog;

	DumpLogMessage(LOGFILE_MEMORY_STATS,
				   0,
				   "\n\nIn Allocation Size Order\n\n%5s | %-65s | %10s | %13s | %13s | %17s | %17s | %17s | %17s\n\n",
				   "ID",
				   "Line  | FileName",
				   "Num Allocs",
				   "Size in bytes",
				   "Max in bytes",
				   "AVG size in bytes",
				   "Smallest in bytes",
				   "Largest  in bytes",
				   "Current  in bytes");

	while (psCurrent)
	{
		DumpLogMessage(LOGFILE_MEMORY_STATS,
					   0,
					   "%5d | %-65s | %10d | %13d | %13d | %17d | %17d | %17d | %17d\n",
					   psCurrent->uAllocID,
					   psCurrent->pszAllocInfo,
					   psCurrent->uNumAllocs,
					   psCurrent->nTotalSizeInBytes,
					   psCurrent->nMaxSizeInBytes,
					   psCurrent->nTotalSizeInBytes / psCurrent->uNumAllocs,
					   psCurrent->nSmallestAlloc,
					   psCurrent->nLargestAlloc,
					   psCurrent->nCurrentSizeInBytes);
		
		psCurrent = psCurrent->psNext;
	}

	bSwapped = IMG_TRUE;

	/* Sort the list into largest num allocs first */
	while (bSwapped)
	{
		psCurrent = gpsMemAllocLog;

		bSwapped = IMG_FALSE;

		
		while (psCurrent->psNext)
		{
			MemAllocLog *psNext = psCurrent->psNext;

			/* Swap entries if larger */
			if (psNext->uNumAllocs > psCurrent->uNumAllocs)
			{
				MemAllocLog *psPrev = psCurrent->psPrev;

				psCurrent->psNext = psNext->psNext;
				psCurrent->psPrev = psNext;

				if (psNext->psNext)
				{
					psNext->psNext->psPrev = psCurrent;
				}

				psNext->psNext = psCurrent;
				psNext->psPrev = psPrev;

				if (psPrev)
				{
					psPrev->psNext = psNext;
				}

				if (gpsMemAllocLog == psCurrent)
				{
					gpsMemAllocLog = psNext;
				}
				
				bSwapped = IMG_TRUE;
			}
			else
			{
				psCurrent = psCurrent->psNext;
			}
		}
	}
	
	psCurrent = gpsMemAllocLog;

	DumpLogMessage(LOGFILE_MEMORY_STATS,
				   0,
				   "\n\nIn Number of Allocations Order\n\n%5s | %-65s | %10s | %13s | %13s | %17s | %17s | %17s | %17s\n\n",
				   "ID",
				   "Line  | FileName",
				   "Num Allocs",
				   "Size in bytes",
				   "Max in bytes",
				   "AVG size in bytes",
				   "Smallest in bytes",
				   "Largest  in bytes",
				   "Current  in bytes");

	while (psCurrent)
	{
		MemAllocLog *psOldCurrent = psCurrent;

		DumpLogMessage(LOGFILE_MEMORY_STATS,
					   0,
					   "%5d | %-65s | %10d | %13d | %13d | %17d | %17d | %17d | %17d\n",
					   psCurrent->uAllocID,
					   psCurrent->pszAllocInfo,
					   psCurrent->uNumAllocs,
					   psCurrent->nTotalSizeInBytes,
					   psCurrent->nMaxSizeInBytes,
					   psCurrent->nTotalSizeInBytes / psCurrent->uNumAllocs,
					   psCurrent->nSmallestAlloc,
					   psCurrent->nLargestAlloc,
					   psCurrent->nCurrentSizeInBytes);
		
		psCurrent = psCurrent->psNext;
		
		free(psOldCurrent);
	}

	gpsMemAllocLog = IMG_NULL;
}

#endif // DUMP_LOGFILES

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : -
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static IMG_VOID RemoveRecord(MemAllocRecord *psRecord)
{
	MemAllocRecord *psPrev = psRecord->psPrev;
	MemAllocRecord *psNext = psRecord->psNext;
	
	if (psPrev)
	{
		if (psRecord == gpsMemAllocRecord)
		{
			DEBUG_MESSAGE(("RemoveRecord: Invalid memory record tree 1!\n"));
		}
		
		psPrev->psNext = psNext;
		
	}
	else
	{
		if (psRecord != gpsMemAllocRecord)
		{
			DEBUG_MESSAGE(("RemoveRecord: bad tree 2!\n"));
		}
		else
		{
			/* If we were removing the top record then adjust the pointer to the start of the list */
			gpsMemAllocRecord = psNext;
		}
	}
	
	if (psNext)
	{
		psNext->psPrev = psPrev;
	}
	
#ifdef USE_MEMCHK
	 MemChkFree(psRecord->AllocInfo);
	 MemChkFree(psRecord); 
#else
	free(psRecord->AllocInfo);
	free(psRecord);
#endif	
	psRecord = NULL;
	
	guMemStatsNumRecords--;
}

/******************************************************************************
 * Function Name: DebugMemAllocfn
 *
 * Inputs       : uSizeInBytes, uLineNumber, pszFileName
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : Allocates memory and creates a record of the allocation.
 *****************************************************************************/
IMG_INTERNAL IMG_VOID *DebugMemAllocfn(IMG_UINT32  uSizeInBytes, 
										IMG_UINT32  uLineNumber, 
										const IMG_CHAR *pszFileName, 
										IMG_BOOL  bInitToZero,
										IMG_VOID *pvRealloc)
{
	MemAlloc *psMemAlloc;
	MemAllocRecord *psRecord;

	/* Allocate space for mem alloc ptr */
	IMG_UINT32 uAdjustedSizeInBytes = uSizeInBytes + sizeof(MemAlloc);
	IMG_UINT32 uPreviousSizeInBytes = 0;

#ifdef USE_MEMCHK
	IMG_UINT32 uReturnAddress;
	
	if (!bMemChkInit)
	{
		MemChkInit();
		
		bMemChkInit = IMG_TRUE;
	}
#endif
	
	if(!uSizeInBytes)
	{
		return IMG_NULL;
	}

	if (pvRealloc)
	{
		psRecord = FindAlloc(pvRealloc, uLineNumber, pszFileName);

		if (!psRecord)
		{
			return IMG_NULL;
		}

		uPreviousSizeInBytes = psRecord->uSize  - sizeof(MemAlloc);

		pvRealloc = psRecord->psMemAlloc;

		RemoveRecord(psRecord);
		
#ifdef USE_MEMCHK
		GetReturnAddress(uReturnAddress);
		
		psMemAlloc = MemChkRealloc(pvRealloc, uAdjustedSizeInBytes, uReturnAddress);
#else
		psMemAlloc = realloc(pvRealloc, uAdjustedSizeInBytes);
#endif
		guMemStatsNumReallocs++;
	}
	else
	{
#ifdef USE_MEMCHK
		GetReturnAddress(uReturnAddress);

		psMemAlloc = MemChkMalloc(uAdjustedSizeInBytes, uReturnAddress);
#else
		psMemAlloc = malloc(uAdjustedSizeInBytes);
#endif
		if (bInitToZero)
		{
			memset(psMemAlloc->pbMem, 0, uSizeInBytes);
		}

		guMemStatsNumAllocs++;
	}

#ifdef USE_MEMCHK
	GetReturnAddress(uReturnAddress);
		
	psRecord = MemChkMalloc(sizeof(MemAllocRecord), uReturnAddress);
#else
	psRecord = malloc(sizeof(MemAllocRecord));
#endif

	if (psRecord)
	{
		IMG_CHAR acScratchBuffer[300];
		IMG_UINT32 uMsgLength;

		/* Add new mem alloc record to front of list */
		if (gpsMemAllocRecord)
		{
			gpsMemAllocRecord->psPrev = psRecord;
		}

		psRecord->psNext         = gpsMemAllocRecord;
		psRecord->psPrev         = NULL;
		psRecord->psMemAlloc     = psMemAlloc;
		psRecord->uSize          = uAdjustedSizeInBytes;
		psRecord->uID            = guMemStatsNumAllocs + guMemStatsNumReallocs;
		
		gpsMemAllocRecord        = psRecord;
	
		uMsgLength = sprintf(acScratchBuffer, "%5lu | %-23s",uLineNumber, pszFileName);
		psRecord->uAllocInfoSize = sizeof(IMG_CHAR) * (uMsgLength + 1);

#ifdef USE_MEMCHK
		GetReturnAddress(uReturnAddress);
		
		psRecord->AllocInfo = MemChkMalloc(psRecord->uAllocInfoSize, uReturnAddress);
#else
		psRecord->AllocInfo = malloc(psRecord->uAllocInfoSize);
#endif	
		strcpy(psRecord->AllocInfo, acScratchBuffer);			

		guMemStatsNumRecords++;

#ifdef DUMP_LOGFILES
		LogMemoryAlloc(uSizeInBytes - uPreviousSizeInBytes, psRecord->AllocInfo);
#endif
	}
	else
	{
		DEBUG_MESSAGE(("DebugMemAlloc: Error, failed to create alloc record Line: %d, File %s!\n",uLineNumber, pszFileName));
	}

	guMemStatsCurrentMemAllocated += (uAdjustedSizeInBytes - uPreviousSizeInBytes);
	guMemStatsTotalMemAllocated += uAdjustedSizeInBytes;

	if (guMemStatsCurrentMemAllocated > guMemStatsMaxMemAllocated)
	{
		guMemStatsMaxMemAllocated = guMemStatsCurrentMemAllocated;
	}

	if (guMemStatsNumRecords > guMemStatsMaxNumRecords)
	{
		guMemStatsMaxNumRecords = guMemStatsNumRecords;
	}

	psMemAlloc->psRecord = psRecord;
	psMemAlloc->uMarker  = MARKER;
	
	return &(psMemAlloc->pbMem[0]);
}

/******************************************************************************
 * Function Name: DebugMemFreefn
 *
 * Inputs       : MemToFree, uLineNumber, pszFileName
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : Frees allocated memory and removes the allocation record
 *****************************************************************************/
IMG_INTERNAL IMG_VOID DebugMemFreefn(IMG_VOID *MemToFree, IMG_UINT32 uLineNumber, const IMG_CHAR *pszFileName)
{
	MemAllocRecord *psRecord;
	
	if (!MemToFree)
	{
		return;
	}

	/* Search through our mem alloc record an remove it */
	psRecord = FindAlloc(MemToFree, uLineNumber, pszFileName);

	if (psRecord)
	{
		/* Actual memory to free is pointed to by record */
		MemToFree = psRecord->psMemAlloc;
		
		guMemStatsCurrentMemAllocated -= psRecord->uSize;
		
#ifdef DUMP_LOGFILES
		LogMemoryAlloc(-((IMG_INT32)psRecord->uSize - (IMG_INT32)sizeof(MemAlloc)), psRecord->AllocInfo);
#endif

		RemoveRecord(psRecord);

#ifdef USE_MEMCHK
		MemChkFree(MemToFree);
#else 
		free(MemToFree);
#endif
		guMemStatsNumFrees++;
	}

}

/******************************************************************************
 * Function Name: DisplayUnfreedMem
 *
 * Inputs       : -
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : Displays all currently unfreed unallocations.
 *****************************************************************************/
IMG_INTERNAL IMG_VOID DisplayUnfreedMem()
{
	MemAllocRecord *psRecord = gpsMemAllocRecord;
	IMG_UINT32 uNumSearches = 0;

	if (guMemStatsNumAllocs != guMemStatsNumFrees)
	{
		DEBUG_MESSAGE(("uNumAllocs (%d) != uNumFrees (%d) \n", guMemStatsNumAllocs, guMemStatsNumFrees));
	}


	/* Search through our mem alloc records  */
	while (psRecord)
	{
		MemAllocRecord *psNext = psRecord->psNext;
			
		DEBUG_MESSAGE(("Freeing alloc ID %u from %s, Size = %d\n", psRecord->uID, psRecord->AllocInfo, psRecord->uSize));
		
#ifdef USE_MEMCHK
		MemChkFree(psRecord->psMemAlloc);
		MemChkFree(psRecord);
#else
		free(psRecord->psMemAlloc);
		free(psRecord);
#endif
		psRecord = psNext;
			
		if (uNumSearches > guMemStatsNumRecords)
		{
			DEBUG_MESSAGE(("DisplayUnfreedMem: Table seems corrupt!\n"));
			psRecord = NULL;
		}
		
		uNumSearches++;
		
	}
}

IMG_INTERNAL IMG_VOID DebugDisplayMemoryStats()
{
	DEBUG_MESSAGE(("-- Memory Stats -- \n"));
	DEBUG_MESSAGE(("Total number of allocs                = %8d\n", guMemStatsNumAllocs));
	DEBUG_MESSAGE(("Maximum number of records             = %8d\n", guMemStatsMaxNumRecords));
	DEBUG_MESSAGE(("Total number of FindAllocs            = %8d\n", guMemStatsTotalNumFinds));
	DEBUG_MESSAGE(("Total number of Searches              = %8d\n", guMemStatsTotalNumSearches));
	DEBUG_MESSAGE(("AVG number of searches per FindAlloc  = %8d\n", guMemStatsTotalNumSearches / guMemStatsTotalNumFinds));
	DEBUG_MESSAGE(("Total memory allocated                = %8d bytes\n", guMemStatsTotalMemAllocated));
	DEBUG_MESSAGE(("Memory high watermark                 = %8d bytes\n", guMemStatsMaxMemAllocated)); 
}

#else // USE_MEMMGR_ALLOC_TRACKING

/* NULL versions of the above functions for RELEASE builds */
IMG_INTERNAL IMG_VOID DebugDisplayMemoryStats()
{
	DEBUG_MESSAGE(("-- Memory Stats -- \n"));
	DEBUG_MESSAGE(("Only available for debug builds\n"));
}

#ifdef DUMP_LOGFILES
IMG_INTERNAL IMG_VOID DebugDumpMemoryAllocLog(IMG_VOID)
{
	DumpLogMessage(LOGFILE_MEMORY_STATS, 0, "Only available for DEBUG builds\n");
}
#endif

#endif // USE_MEMMGR_ALLOC_TRACKING



/******************************************************************************
 * Function Name: DebugCreateHeap
 *
 * Inputs       : -
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL MemHeap *DebugCreateHeapfn(IMG_UINT32  uItemSizeInBytes, 
										IMG_UINT32  uNumHeapItems
#if defined(DEBUG) || defined(DUMP_LOGFILES)
										,IMG_UINT32  uLineNumber, 
										const IMG_CHAR *pszFileName
#endif
										)
{
	MemHeap *psMemHeap = DebugMemCalloc2(sizeof(MemHeap), uLineNumber, pszFileName);

	if(!psMemHeap)
	{
		return IMG_NULL;
	}

	/* Allocate a minimum size of an integer to store the free list */
	if(uItemSizeInBytes < sizeof(IMG_VOID*))
	{
		uItemSizeInBytes = sizeof(IMG_VOID*);
	}

	if(!uNumHeapItems)
	{
		uNumHeapItems = 1;
	}

	psMemHeap->uHeapItemSizeInBytes     = uItemSizeInBytes;
	psMemHeap->uHeapSizeInBytes         = uItemSizeInBytes * uNumHeapItems;
	psMemHeap->pbHeap                   = DebugMemAlloc2(psMemHeap->uHeapSizeInBytes, uLineNumber, pszFileName);

	if (!psMemHeap->pbHeap)
	{
		DebugMemFree(psMemHeap);
		return IMG_NULL;
	}

	psMemHeap->pbEndOfHeap              = &psMemHeap->pbHeap[psMemHeap->uHeapSizeInBytes];
	psMemHeap->pbCurrentWaterMark       = psMemHeap->pbHeap;
	psMemHeap->pvFreeListHead           = IMG_NULL;
	
#if defined(DUMP_LOGFILES) || defined(DEBUG)
	psMemHeap->pszHeapCreationInfo = DebugMemAlloc2(strlen(pszFileName) + 15, uLineNumber, pszFileName);
	sprintf(psMemHeap->pszHeapCreationInfo, "%u : %s", uLineNumber, pszFileName);
	psMemHeap->uHeapOverflows           = 0;
#endif

	return psMemHeap;
}


/******************************************************************************
 * Function Name: DebugAllocHeapItem
 *
 * Inputs       : -
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_VOID *DebugAllocHeapItemfn(MemHeap *psMemHeap
#if defined(STANDALONE) && defined(DEBUG) 
											, IMG_UINT32 uLineNumber, const IMG_CHAR *pszFileName
#endif
											)
{
	IMG_VOID *pvResult = psMemHeap->pvFreeListHead;

	if(psMemHeap->pvFreeListHead)
	{
		psMemHeap->pvFreeListHead = *(IMG_VOID**)psMemHeap->pvFreeListHead;
	}
	else if(psMemHeap->pbCurrentWaterMark < psMemHeap->pbEndOfHeap)
	{
		pvResult = (IMG_VOID*)psMemHeap->pbCurrentWaterMark;
		psMemHeap->pbCurrentWaterMark += psMemHeap->uHeapItemSizeInBytes;
	}
	else
	{
#ifdef DUMP_LOGFILES
		psMemHeap->uHeapOverflows++;	
#endif
		pvResult = DebugMemAlloc2(psMemHeap->uHeapItemSizeInBytes, uLineNumber, pszFileName);
	}

	return pvResult;
}

/******************************************************************************
 * Function Name: DebugFreeHeapItem
 *
 * Inputs       : -
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_VOID DebugFreeHeapItemfn(MemHeap *psMemHeap, 
										  IMG_VOID *pvItem)
{
	/* Append it to the free list */
	*(IMG_VOID**)pvItem = psMemHeap->pvFreeListHead;
	psMemHeap->pvFreeListHead = pvItem;
}

/******************************************************************************
 * Function Name: DebugDestroyHeap
 *
 * Inputs       : -
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_VOID DebugDestroyHeapfn(MemHeap *psMemHeap
#if defined(STANDALONE) && defined(DEBUG)
										 , IMG_UINT32 uLineNumber, const IMG_CHAR *pszFileName
#endif
										 )
{
	IMG_VOID *pvBlock;

	if (!psMemHeap)
	{
		return;
	}

#ifdef DUMP_LOGFILES
	DumpLogMessage(LOGFILE_MEMORY_STATS,
				   0,
				   "Heap Stats for %s\nHeap usage = %d/%d slots, Heap Overflows = %d, High watermark = %d\n\n",
				   psMemHeap->pszHeapCreationInfo,
				   (psMemHeap->pbCurrentWaterMark -psMemHeap->pbHeap)/psMemHeap->uHeapItemSizeInBytes,
				   psMemHeap->uHeapSizeInBytes/psMemHeap->uHeapItemSizeInBytes,
				   psMemHeap->uHeapOverflows,
				   psMemHeap->uHeapSizeInBytes + psMemHeap->uHeapOverflows*psMemHeap->uHeapItemSizeInBytes);

#endif

#if defined(DEBUG) || defined(DUMP_LOGFILES)
	DebugMemFree2(psMemHeap->pszHeapCreationInfo, uLineNumber, pszFileName);
#endif

	while(psMemHeap->pvFreeListHead)
	{
		pvBlock = psMemHeap->pvFreeListHead;
		psMemHeap->pvFreeListHead = *(IMG_VOID**)psMemHeap->pvFreeListHead;
		
		if(pvBlock < (IMG_VOID *)psMemHeap->pbHeap || pvBlock >= (IMG_VOID *)psMemHeap->pbEndOfHeap)
		{
			/* The block did not came from the pool, it was externally allocated */
			DebugMemFree2(pvBlock, uLineNumber, pszFileName);
		}
	}

	DebugMemFree2(psMemHeap->pbHeap, uLineNumber, pszFileName);

	/* Free the struct */
	DebugMemFree2(psMemHeap, uLineNumber, pszFileName);
}

