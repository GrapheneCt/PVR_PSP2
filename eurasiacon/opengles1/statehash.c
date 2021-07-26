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
 *
 *  --- Revision Logs Removed --- 
 **************************************************************************/

#include "context.h"
#include <string.h>

typedef  unsigned long  int  ub4;   /* unsigned 4-byte quantities */

#define HASH_SIZE(N) ((ub4)1<<(N))
#define	HASH_MASK(N) (HASH_SIZE(N)-1)

static IMG_VOID HashTableDeleteUnsafe(GLES1Context *gc, HashTable *psHashTable, HashEntry  *psHashEntry);

/***********************************************************************************
 Function Name      : HashTableCreate
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
IMG_INTERNAL IMG_BOOL HashTableCreate(GLES1Context *gc, HashTable *psHashTable,
									  IMG_UINT32 ui32Log2TableSize, IMG_UINT32 ui32MaxNumEntries, 
									  PFNDestroyHashItem pfnDestroyItemFunc
#ifdef HASHTABLE_DEBUG
									  , const IMG_CHAR *	pszHashTableName
#endif
									  )
{
#ifdef HASHTABLE_DEBUG
	psHashTable->pszName = pszHashTableName;
#endif
	psHashTable->ui32NumEntries = 0;
	psHashTable->ui32NumHashValues = 0;
	psHashTable->ui32PeakNumEntries = 0;
	psHashTable->ui32PeakNumHashValues = 0;
	psHashTable->ui32TableSize = HASH_SIZE(ui32Log2TableSize);
	psHashTable->ui32HashValueMask = HASH_MASK(ui32Log2TableSize);
	psHashTable->ui32MaxNumEntries = ui32MaxNumEntries;
	psHashTable->pfnDestroyItemFunc = pfnDestroyItemFunc;

	psHashTable->psTable = (HashEntry **)GLES1Calloc(gc, psHashTable->ui32TableSize * sizeof(HashEntry *));

	psHashTable->sInsertInfo.ui32DeleteEntryHashChain = 0;
	psHashTable->sInsertInfo.psEntryToDelete = IMG_NULL;
	psHashTable->sInsertInfo.psEntryAheadDelete = IMG_NULL;
	psHashTable->sInsertInfo.bChecked = IMG_FALSE;
	
	/* 
		Some systems are ignoring the gc parameter in GLES1Calloc, and 
		issue warnings
	*/
	PVR_UNREFERENCED_PARAMETER(gc);

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
IMG_INTERNAL IMG_VOID HashTableDestroy(GLES1Context *gc, HashTable *psHashTable)
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

			HashTableDeleteUnsafe(gc, psHashTable, psHashChain);

			psHashChain = psNextInChain;
		}
	}

	/* Finally free the table of HashEntry pointers */
	GLES1Free(gc, psHashTable->psTable);
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

#if (defined(DEBUG) || defined(LINUX))

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

#else

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

#endif

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
IMG_INTERNAL IMG_BOOL HashTableSearch(GLES1Context *gc,
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
 Function Name      : HashTableDeleteUnsafe
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : Delete the hash entry psHashEntry.  This function is only 
                      used internally in this file.
************************************************************************************/
static IMG_VOID HashTableDeleteUnsafe(GLES1Context *gc, HashTable *psHashTable, HashEntry  *psHashEntry)
{
	KRMKickResourceManager * psMgr;
	KRMResource * psResource;

	/* Remove from the KRM list */
	if (psHashTable->pfnDestroyItemFunc == DestroyHashedPDSVariant)
	{
		psMgr = &(gc->psSharedState->sPDSVariantKRM);
		psResource = &(((GLES1PDSCodeVariant *)(psHashEntry->ui32Item))->sResource);
		
		KRM_RemoveResourceFromAllLists(psMgr, psResource);
	}

	/* Call the destroy function for this item */
	(psHashTable->pfnDestroyItemFunc)(gc, psHashEntry->ui32Item);

	if(psHashEntry->pui32HashKey)
	{
		GLES1Free(gc, psHashEntry->pui32HashKey);
	}

	GLES1Free(gc, psHashEntry);

	psHashTable->ui32NumEntries--;
}

/***********************************************************************************
 Function Name      : CheckIsEntryInUse
 Inputs             : gc, psHashTable, psHashEntry
 Outputs            : 
 Returns            : 
 Description        : Check if the psHashEntry is currently in use and thus cannot 
                      be deleted from the hash table.
************************************************************************************/
static IMG_BOOL CheckIsEntryInUse(GLES1Context *gc, HashTable *psHashTable, HashEntry  *psHashEntry)
{
	KRMKickResourceManager * psMgr;
	KRMResource * psResource;
	
	if (psHashTable->pfnDestroyItemFunc == DestroyHashedPDSVariant)
	{
		psMgr = &(gc->psSharedState->sPDSVariantKRM);
		psResource = &(((GLES1PDSCodeVariant *)(psHashEntry->ui32Item))->sResource);
		
		return KRM_IsResourceNeeded((const KRMKickResourceManager *)psMgr, (const KRMResource *)psResource);
	}

	/* If cannot judge, just return true to mark it in use */
	return IMG_TRUE;
}

/***********************************************************************************
 Function Name      : ValidateHashTableInsert
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : Validate if a new entry can be inserted into the 
                      hash table 'psHashTable', using the hash value 'tHashValu.
					  This function must be called before calling HashTableInsert.
************************************************************************************/
IMG_INTERNAL IMG_BOOL ValidateHashTableInsert(GLES1Context *gc,
											  HashTable	  *psHashTable,
											  HashValue	  tHashValue)
{
	IMG_UINT32	ui32TableIndex, i, ui32OldestEntryFrame;
	HashEntry  *psHashChain;

	/* Ensure this function has been called before calling HashTableInsert. */
	psHashTable->sInsertInfo.bChecked = IMG_TRUE;

	/* If the hash table is not full, safely return true. */
	if (psHashTable->ui32NumEntries < psHashTable->ui32MaxNumEntries)
	{
		psHashTable->sInsertInfo.ui32DeleteEntryHashChain = 0;
		psHashTable->sInsertInfo.psEntryAheadDelete = psHashTable->sInsertInfo.psEntryToDelete = IMG_NULL;

#ifdef HASHTABLE_DEBUG
		PVR_DPF((PVR_DBG_WARNING,"Validate Insert for hash table %s, OK as not full (cur: %d / max: %d)", \
			psHashTable->pszName, psHashTable->ui32NumEntries, psHashTable->ui32MaxNumEntries));
#endif

		return IMG_TRUE;
	}

	/* If the hash table is full, then an old entry needs to be deleted with this new entry inserted.
	   However, it is necessary to judge that this old entry is not in use before it can be moved.
	 */

	/* Find the least used entry which is also not in use from the current hash table, 
	   start from the hash chain this new item is going to be inserted.
	 */
	ui32TableIndex = tHashValue & psHashTable->ui32HashValueMask;
	psHashChain = psHashTable->psTable[ui32TableIndex];

	i = 1;
	while ( (psHashChain == NULL) && (i < psHashTable->ui32TableSize) )
	{
		/* There were no entries in this hash chain before we added
	 	   so find the next non-empty chain
		 */
		ui32TableIndex = (ui32TableIndex + 1) & psHashTable->ui32HashValueMask;

		psHashChain = psHashTable->psTable[ui32TableIndex];

		i += 1;
	}

	/* Search through the hash chain, looking for the oldest 
	   item in the chain. */
	if (psHashChain == NULL)
	{
		psHashTable->sInsertInfo.ui32DeleteEntryHashChain = 0;
		psHashTable->sInsertInfo.psEntryAheadDelete = psHashTable->sInsertInfo.psEntryToDelete = IMG_NULL;
		return IMG_FALSE;
	}

	psHashTable->sInsertInfo.ui32DeleteEntryHashChain = ui32TableIndex;
	psHashTable->sInsertInfo.psEntryToDelete = psHashChain;
	ui32OldestEntryFrame = psHashTable->sInsertInfo.psEntryToDelete->ui32LastFrameHashed;
	psHashTable->sInsertInfo.psEntryAheadDelete = IMG_NULL;
	
	while(psHashChain->psNext != NULL)
	{
		if (psHashChain->psNext->ui32LastFrameHashed <= ui32OldestEntryFrame)
		{
			psHashTable->sInsertInfo.psEntryToDelete = psHashChain->psNext;
			psHashTable->sInsertInfo.psEntryAheadDelete = psHashChain;
		}

		psHashChain = psHashChain->psNext;
	}

	/* Check if this item is currently in use */
	if (!CheckIsEntryInUse(gc, psHashTable, psHashTable->sInsertInfo.psEntryToDelete))
	{
		/* item is not in use */
#ifdef HASHTABLE_DEBUG
		PVR_DPF((PVR_DBG_WARNING,"Validate Insert for hash table %s, OK as one entry can be freed (cur: %d / max: %d)", \
			psHashTable->pszName, psHashTable->ui32NumEntries, psHashTable->ui32MaxNumEntries));
#endif
		return IMG_TRUE;
	}
	else
	{
		/* NEED THIS ???
		   Search through the hash table to see if any entry can be removed
		 */
		psHashTable->sInsertInfo.ui32DeleteEntryHashChain = 0;
		psHashTable->sInsertInfo.psEntryAheadDelete = psHashTable->sInsertInfo.psEntryToDelete = IMG_NULL;

#ifdef HASHTABLE_DEBUG
		PVR_DPF((PVR_DBG_WARNING,"Validate Insert for hash table %s, FAIL as no entry can be freed (cur: %d / max: %d)", \
			psHashTable->pszName, psHashTable->ui32NumEntries, psHashTable->ui32MaxNumEntries));
#endif
		return IMG_FALSE;
	}
}

/***********************************************************************************
 Function Name      : HashTableInsert
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : Insert 'ui32Item' into the hash table 'psHashTable',
					  using the hash value 'tHashValue'
************************************************************************************/
IMG_INTERNAL IMG_VOID HashTableInsert(GLES1Context *gc,
									  HashTable	  *psHashTable,
									  HashValue	  tHashValue,
									  IMG_UINT32  *pui32HashKey,
									  IMG_UINT32  ui32HashKeySizeInDWords,
									  IMG_UINT32  ui32Item)
{
	IMG_UINT32	ui32TableIndex;
	HashEntry  *psNewHashEntry;

	/* Check if the ValidateHashTableInsert() has been called before calling this function. */
	//GLES1_ASSERT(psHashTable->sInsertInfo.bChecked == IMG_TRUE);

	psHashTable->sInsertInfo.bChecked = IMG_FALSE;

	/* Find the hash chain to attach this entry to */
	ui32TableIndex = tHashValue & psHashTable->ui32HashValueMask;

	/* Create the new entry */
	psNewHashEntry = (HashEntry *)GLES1Malloc(gc, sizeof(HashEntry));

	psNewHashEntry->tHashValue = tHashValue;
	psNewHashEntry->pui32HashKey = pui32HashKey;
	psNewHashEntry->ui32HashKeySizeInDWords = ui32HashKeySizeInDWords;
	
	psNewHashEntry->ui32Item = ui32Item;

	psNewHashEntry->ui32LastFrameHashed = gc->ui32FrameNum;

	/* Delete an item based on the returned information from ValidateHashTableInsert(). */
	if (psHashTable->sInsertInfo.psEntryToDelete == IMG_NULL)
	{
		/* No entry needs to be deleted. */
	}
	else
	{
		if (psHashTable->sInsertInfo.psEntryAheadDelete == IMG_NULL)
		{
			/* The first entry in the chain needs to be deleted */
			psHashTable->psTable[psHashTable->sInsertInfo.ui32DeleteEntryHashChain] = psHashTable->sInsertInfo.psEntryToDelete->psNext;
		}
		else
		{
			psHashTable->sInsertInfo.psEntryAheadDelete->psNext = psHashTable->sInsertInfo.psEntryToDelete->psNext;
		}

		/* Delete this entry */
		HashTableDeleteUnsafe(gc, psHashTable, psHashTable->sInsertInfo.psEntryToDelete);
	}

	/* Insert at head of hash chain */
	psNewHashEntry->psNext = psHashTable->psTable[ui32TableIndex];
	psHashTable->psTable[ui32TableIndex] = psNewHashEntry;

	psHashTable->ui32NumEntries++;

	psHashTable->sInsertInfo.ui32DeleteEntryHashChain = 0;
	psHashTable->sInsertInfo.psEntryAheadDelete = psHashTable->sInsertInfo.psEntryToDelete = IMG_NULL;

#ifdef HASHTABLE_DEBUG
	PVR_DPF((PVR_DBG_WARNING,"\t=> Insert into hash table %s (cur: %d / max: %d)", psHashTable->pszName, psHashTable->ui32NumEntries, psHashTable->ui32MaxNumEntries));
#endif
}


/***********************************************************************************
 Function Name      : HashTableDelete
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : Delete the hash entry with the hash value 'tHashValue' from
					  the table 'psHashTable'
************************************************************************************/
IMG_INTERNAL IMG_BOOL HashTableDelete(GLES1Context *gc, HashTable *psHashTable, HashValue tHashValue,  
									  IMG_UINT32 *pui32HashKey, IMG_UINT32 ui32HashKeySizeInDWords,
									  IMG_UINT32 *pui32Item)
{
	IMG_UINT32	ui32TableIndex;
	HashEntry  *psHashChain, *psPrevHashEntry;
	IMG_BOOL	bFound = IMG_FALSE;

#ifdef HASHTABLE_DEBUG
	IMG_CHAR szInfo[512];
#endif

	ui32TableIndex = tHashValue & psHashTable->ui32HashValueMask;
	psHashChain = psHashTable->psTable[ui32TableIndex];
	psPrevHashEntry = psHashChain;

#ifdef HASHTABLE_DEBUG
	sprintf(szInfo, "Delete entry in hash table %s", psHashTable->pszName);
#endif

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
					/* Check if this item is currently in use */
					if (CheckIsEntryInUse(gc, psHashTable, psHashChain))
					{
						/* cannot delete this item */
#ifdef HASHTABLE_DEBUG
						strcat(szInfo, "FAIL as this entry is in use");
#endif
						(*pui32Item) = 0;
						return IMG_FALSE;
					}

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

					/* Delete this entry */
					HashTableDeleteUnsafe(gc, psHashTable, psHashChain);

#ifdef HASHTABLE_DEBUG
					strcat(szInfo, "deleted");
#endif
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
	
#ifdef HASHTABLE_DEBUG
	if (!bFound)
	{
		strcat(szInfo, "no such entry");
	}

	PVR_DPF((PVR_DBG_WARNING, "%s (cur: %d / max: %d)", szInfo, psHashTable->ui32NumEntries, psHashTable->ui32MaxNumEntries));
#endif

	return bFound;
}

