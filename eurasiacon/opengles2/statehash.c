/**************************************************************************
 * Name         : statehash.c
 *
 * Copyright    : 2006-2008 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or other-wise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited, Home Park
 *              : Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * $Log: statehash.c $
 **************************************************************************/

#include "context.h"

typedef  unsigned long  int  ub4;   /* unsigned 4-byte quantities */
/* typedef  unsigned       char ub1; */

#define HASH_SIZE(N) ((ub4)1<<(N))
#define	HASH_MASK(N) (HASH_SIZE(N)-1)


/***********************************************************************************
 Function Name      : HashTableCreate
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
IMG_INTERNAL IMG_BOOL HashTableCreate(GLES2Context *gc, HashTable *psHashTable,
									  IMG_UINT32 ui32Log2TableSize, IMG_UINT32 ui32MaxNumEntries,
									  PFNDestroyHashItem pfnDestroyItemFunc)
{
	psHashTable->ui32NumEntries = 0;
	psHashTable->ui32NumHashValues = 0;
	psHashTable->ui32PeakNumEntries = 0;
	psHashTable->ui32PeakNumHashValues = 0;
	psHashTable->ui32TableSize = HASH_SIZE(ui32Log2TableSize);
	psHashTable->ui32HashValueMask = HASH_MASK(ui32Log2TableSize);
	psHashTable->ui32MaxNumEntries = ui32MaxNumEntries;
	psHashTable->pfnDestroyItemFunc = pfnDestroyItemFunc;

	psHashTable->psTable = (HashEntry **)GLES2Calloc(gc, psHashTable->ui32TableSize * sizeof(HashEntry *));

	
	PVR_UNREFERENCED_PARAMETER(gc);
	/* 
		Some systems are ignoring the gc parameter in GLES1Calloc, and 
		issue warnings
	*/

	if(!psHashTable->psTable)
	{
		PVR_DPF((PVR_DBG_ERROR,"Hash table alloc failed"));
		return IMG_FALSE;
	}
	
	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : HashTableDestroy
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
IMG_INTERNAL IMG_VOID HashTableDestroy(GLES2Context *gc, HashTable *psHashTable)
{
	IMG_UINT32 i;
	HashEntry *psHashChain, *psNextInChain;

	/* Go through entire table */
	for(i=0; i<psHashTable->ui32TableSize; i++)
	{
		psHashChain = psHashTable->psTable[i];
		
		/* For each item in the chain, call the destroy function
		   and free the HashEntry */
		while(psHashChain)
		{
			psNextInChain = psHashChain->psNext;

			(psHashTable->pfnDestroyItemFunc)(gc, psHashChain->ui32Item);

			if(psHashChain->pui32HashKey)
			{
				GLES2Free(IMG_NULL, psHashChain->pui32HashKey);
			}
			GLES2Free(IMG_NULL, psHashChain);

			psHashChain = psNextInChain;
		}
	}

	/* Finally free the table of HashEntry pointers */
	GLES2Free(IMG_NULL, psHashTable->psTable);
}


/***********************************************************************************
 Function Name      : HashFunc
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
IMG_INTERNAL HashValue HashFunc(IMG_UINT32 *pui32HashKey, IMG_UINT32 ui32KeySizeInDwords, IMG_UINT32 ui32InitialValue)
{
	IMG_UINT32 h;
	IMG_UINT32 i;

	for(h=ui32InitialValue, i=0; i<ui32KeySizeInDwords; ++i) 
	{ 
		h += (pui32HashKey)[i]; 
		h += (h<<10); 
		h ^= (h>>6); 
	} 
	
	h += (h<<3); 
	h ^= (h>>11); 
	h += (h<<15); 

	return (HashValue)h;
}


/***********************************************************************************
 Function Name      : HashTableSearch
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : Search hash table 'psHashTable' for the hash value 'tHashValue'.
   					  If found, returns IMG_TRUE and sets 'pui32Item' to the stored item. If 
					  not found, returns IMG_FALSE.
************************************************************************************/
IMG_INTERNAL IMG_BOOL HashTableSearch(GLES2Context *gc,
									  HashTable	  *psHashTable,
									  HashValue	  tHashValue, 
									  IMG_UINT32  *pui32HashKey,
									  IMG_UINT32  ui32HashKeySizeInDWords,	
									  IMG_UINT32  *pui32Item)
{
	IMG_UINT32	ui32TableIndex;
	HashEntry  *psHashChain;
	IMG_BOOL	bFound = IMG_FALSE;

	ui32TableIndex = tHashValue & psHashTable->ui32HashValueMask;

	psHashChain = psHashTable->psTable[ui32TableIndex];

	while(psHashChain != IMG_NULL && !bFound)
	{
		if(psHashChain->tHashValue == tHashValue)
		{
			IMG_UINT32 i;

			if(ui32HashKeySizeInDWords == psHashChain->ui32HashKeySizeInDWords)
			{
				IMG_UINT32 ui32HashKeyComparison = 0;

				for(i=0; i<ui32HashKeySizeInDWords; i++)
				{
					ui32HashKeyComparison |= pui32HashKey[i] ^ psHashChain->pui32HashKey[i];
				}

				if(ui32HashKeyComparison == 0)
				{
					bFound = IMG_TRUE;

					*pui32Item = psHashChain->ui32Item;

					psHashChain->ui32LastFrameHashed = gc->ui32FrameNum;
				}
			}
		}

		psHashChain = psHashChain->psNext;
	}
	
	return bFound;
}


/***********************************************************************************
 Function Name      : HashTableInsert
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : Insert 'ui32Item' into the hash table 'psHashTable',
					  using the hash value 'tHashValue'
************************************************************************************/
IMG_INTERNAL IMG_VOID HashTableInsert(GLES2Context *gc,
									  HashTable	  *psHashTable,
									  HashValue	  tHashValue,
									  IMG_UINT32  *pui32HashKey,
									  IMG_UINT32  ui32HashKeySizeInDWords,
									  IMG_UINT32  ui32Item)
{
	IMG_UINT32	ui32TableIndex;
	HashEntry  *psHashChain, *psNewHashEntry;

	/* Find the hash chain to attach this entry to */
	ui32TableIndex = tHashValue & psHashTable->ui32HashValueMask;
	psHashChain = psHashTable->psTable[ui32TableIndex];

	/* Create the new entry */
	psNewHashEntry = (HashEntry *)GLES2Malloc(gc, sizeof(HashEntry));

	psNewHashEntry->tHashValue = tHashValue;
	psNewHashEntry->pui32HashKey = pui32HashKey;
	psNewHashEntry->ui32HashKeySizeInDWords = ui32HashKeySizeInDWords;
	
	psNewHashEntry->ui32Item = ui32Item;

	psNewHashEntry->ui32LastFrameHashed = gc->ui32FrameNum;
	psNewHashEntry->psNext = psHashChain;

	/* Insert at head of hash chain */
	psHashTable->psTable[ui32TableIndex] = psNewHashEntry;

	psHashTable->ui32NumEntries++;

	/* If the number of entries in the table has exceeded the maximum,
	   then we must delete an entry */
	while(psHashTable->ui32NumEntries > psHashTable->ui32MaxNumEntries)
	{
		HashEntry *psOldestEntry;
		IMG_UINT32 ui32OldestEntryFrame;

		while(psHashChain == IMG_NULL)
		{
			/* There were no entries in this hash chain before we added
	 		   so find the next non-empty chain */
			ui32TableIndex = (ui32TableIndex + 1) & psHashTable->ui32HashValueMask;

			psHashChain = psHashTable->psTable[ui32TableIndex];
		}
		
		/* Search through the hash chain, looking for the oldest 
		   item in the chain. */
		psOldestEntry = psHashTable->psTable[ui32TableIndex];

		ui32OldestEntryFrame = psOldestEntry->ui32LastFrameHashed;
			
		psHashChain = psOldestEntry->psNext;

		while(psHashChain)
		{
			if(psHashChain->ui32LastFrameHashed < ui32OldestEntryFrame)
			{
				psOldestEntry = psHashChain;

				ui32OldestEntryFrame = psHashChain->ui32LastFrameHashed;
			}

			psHashChain = psHashChain->psNext;
		}

		if(psOldestEntry != psNewHashEntry) /* We don't want to delete the new entry! */
		{
			IMG_UINT32 ui32Unused;
	
			HashTableDelete(gc, psHashTable, psOldestEntry->tHashValue,
							psOldestEntry->pui32HashKey, psOldestEntry->ui32HashKeySizeInDWords, &ui32Unused);
		}
		else
		{
			psHashChain = IMG_NULL; /* Loop round and find another chain to search */
		}
	}
}


/***********************************************************************************
 Function Name      : HashTableDelete
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : Delete the hash entry with the hash value 'tHashValue' from
					  the table 'psHashTable'
************************************************************************************/
IMG_INTERNAL IMG_BOOL HashTableDelete(GLES2Context *gc, HashTable *psHashTable, HashValue tHashValue,  
									  IMG_UINT32 *pui32HashKey, IMG_UINT32 ui32HashKeySizeInDWords,
									  IMG_UINT32 *pui32Item)
{
	IMG_UINT32	ui32TableIndex;
	HashEntry  *psHashChain, *psPrevHashEntry;
	IMG_BOOL	bFound = IMG_FALSE;

	ui32TableIndex = tHashValue & psHashTable->ui32HashValueMask;
	psHashChain = psHashTable->psTable[ui32TableIndex];
	psPrevHashEntry = psHashChain;

	while(psHashChain != IMG_NULL && !bFound)
	{
		if(psHashChain->tHashValue == tHashValue)
		{
			IMG_UINT32 i;
			IMG_UINT32 ui32HashKeyComparison = 0;

			if(ui32HashKeySizeInDWords == psHashChain->ui32HashKeySizeInDWords)
			{
				for(i=0; i<ui32HashKeySizeInDWords; i++)
				{
					ui32HashKeyComparison |= pui32HashKey[i] ^ psHashChain->pui32HashKey[i];
				}

				if(ui32HashKeyComparison == 0)
				{
					bFound = IMG_TRUE;

					/* Remove item from hash chain */
					if(psHashTable->psTable[ui32TableIndex] == psHashChain)
					{
						psHashTable->psTable[ui32TableIndex] = psHashChain->psNext;
					}
					else
					{
						psPrevHashEntry->psNext = psHashChain->psNext;
					}

					/* Return the hash item - useful in some circumstances */
					*pui32Item = psHashChain->ui32Item;

					/* Call the destroy function for this item */
					(psHashTable->pfnDestroyItemFunc)(gc, psHashChain->ui32Item);

					if(psHashChain->pui32HashKey)
					{
						GLES2Free(IMG_NULL, psHashChain->pui32HashKey);
					}

					GLES2Free(IMG_NULL, psHashChain);

					psHashTable->ui32NumEntries--;
				}
				else
				{
					psPrevHashEntry = psHashChain;
					psHashChain = psHashChain->psNext;
				}
			}
		}
		else
		{
			psPrevHashEntry = psHashChain;
			psHashChain = psHashChain->psNext;
		}
	}
	
	return bFound;
}

