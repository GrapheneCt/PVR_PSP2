/******************************************************************************
 * Name         : symtab.c
 * Author       : James McCarthy
 * Created      : 25/11/2003
 *
 * Copyright    : 2003-2007 by Imagination Technologies Limited.
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
 * $Log: symtab.c $
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symtab.h"
#include "debug.h"

/******************************************************************************
 * Function Name: InitSymbolTableManager
 *
 * Inputs       : -
 * Outputs      : -
 * Returns      : A symbol table context
 * Globals Used : -
 *
 * Description  : Creates a symbol table context.
 *****************************************************************************/
IMG_INTERNAL SymbolTableContext *InitSymbolTableManager()
{
	SymbolTableContext *psSymbolTableContext = DebugMemAlloc(sizeof(SymbolTableContext));

	if(psSymbolTableContext)
	{
		psSymbolTableContext->uSymbolTableListSize = 0;
		psSymbolTableContext->ppsSymbolTables      = IMG_NULL;	
	}
	else
	{
		DEBUG_MESSAGE(("InitSymbolTableManager: Failed to allocate memory for symbol table context"));
	}

	return psSymbolTableContext;
}

/******************************************************************************
 * Function Name: DestroySymbolTableManager
 *
 * Inputs       : -
 * Outputs      : -
 * Returns      : A symbol table context
 * Globals Used : -
 *
 * Description  : Destroys a symbol table context.
 *****************************************************************************/
IMG_INTERNAL IMG_VOID DestroySymbolTableManager(SymbolTableContext *psSymbolTableContext)
{
	if (psSymbolTableContext)
	{
		IMG_UINT32 i;

		/* Warn about any still attached tables */
		for (i = 0; i < psSymbolTableContext->uSymbolTableListSize; i++)
		{
			if (psSymbolTableContext->ppsSymbolTables[i])
			{
				DEBUG_MESSAGE(("DestroySymbolTableManager: Symbol table '%s' (%d out of %d) still seems to be attached",
							 psSymbolTableContext->ppsSymbolTables[i]->acDesc,
							 i, psSymbolTableContext->uSymbolTableListSize));
			}
		}

		if (psSymbolTableContext->ppsSymbolTables)
		{
			DebugMemFree(psSymbolTableContext->ppsSymbolTables);
		}

		DebugMemFree(psSymbolTableContext);
	}
}

/******************************************************************************
 * Function Name: AttachSymbolTableToContext
 *
 * Inputs       : Symbol table
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : Creates a unique ID used to identify symbols from the supplied table
 *****************************************************************************/
static IMG_BOOL AttachSymbolTableToContext(SymbolTableContext *psSymbolTableContext,
											SymTable           *psSymbolTable)
{
	IMG_UINT32 i;

	SymTable **ppsSymbolTableList = psSymbolTableContext->ppsSymbolTables;

	/* Check there is enough space for the unique symbol ID */
	if (psSymbolTableContext->uSymbolTableListSize >= psSymbolTable->uMaxNumUniqueSymbolIDs)
	{
		DEBUG_MESSAGE(("AttachSymbolTableToContext: Not enough space to create unique ID, add more bits to increase size"));
		return IMG_FALSE;
	}

	/* Check that any new symbol tables support the same ID size as any existing tables - cannot mix and match within a context */
	for (i = 0; i < psSymbolTableContext->uSymbolTableListSize; i++)
	{
		if (ppsSymbolTableList[i])
		{
			if (psSymbolTableContext->ppsSymbolTables[i]->uMaxNumBitsForSymbolTableIDs != psSymbolTable->uMaxNumBitsForSymbolTableIDs)
			{
				DEBUG_MESSAGE(("AttachSymbolTableToContext: All symbol tables added to context must have matching number of bits allocated for symbol table IDS"));
				return IMG_FALSE;
			}
			else
			{
				break;
			}
		}
	}

	/* Look for a gap */
	for (i = 0; i < psSymbolTableContext->uSymbolTableListSize; i++)
	{
		/* If a gap is found insert the symbol table */
		if (ppsSymbolTableList[i] == IMG_NULL)
		{
			/* Create a unique ID (can't be 0) */
			psSymbolTable->uUniqueSymbolTableID = ((i + 1) << psSymbolTable->uSymbolIDShift);

			ppsSymbolTableList[i] = psSymbolTable;

			return IMG_TRUE;
		}
	}
	
	/* Didn't find a gap so tack it on the end */

	/* Create a unique ID (can't be 0) */
	psSymbolTable->uUniqueSymbolTableID = ((psSymbolTableContext->uSymbolTableListSize + 1) << psSymbolTable->uSymbolIDShift);

	/* realloc memory for new pointer */
	psSymbolTableContext->ppsSymbolTables = DebugMemRealloc(psSymbolTableContext->ppsSymbolTables, 
															sizeof(SymTable *) * (psSymbolTableContext->uSymbolTableListSize + 1));
	
	/* Add symbol table */
	psSymbolTableContext->ppsSymbolTables[psSymbolTableContext->uSymbolTableListSize] = psSymbolTable;

	psSymbolTableContext->uSymbolTableListSize++;


	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: RemoveSymbolTableFromManager
 *
 * Inputs       : Symbol table
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : Removes a symbol table from the symbol table manager
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL RemoveSymbolTableFromManager(SymbolTableContext *psSymbolTableContext,
												  SymTable           *psSymbolTable)
{
	IMG_UINT32 i;

	/* Find the entry and set it to NULL  */
	for (i = 0; i < psSymbolTableContext->uSymbolTableListSize; i++)
	{
		if (psSymbolTableContext->ppsSymbolTables[i] == psSymbolTable)
		{
			psSymbolTableContext->ppsSymbolTables[i] = IMG_NULL;
		}
	}

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: CreateSymTable
 *
 * Inputs       : uNumEntries
 * Outputs      : -
 * Returns      : A symbol table
 * Globals Used : -
 *
 * Description  : Creates a symbol table.
 *****************************************************************************/
IMG_INTERNAL SymTable *CreateSymTable(SymbolTableContext *psSymbolTableContext,
										IMG_CHAR           *pszDesc,
										IMG_UINT32          uNumEntries, 
										IMG_UINT32          uMaxNumBitsForSymbolTableIDs,
										SymTable           *psSecondarySymbolTable)
{
	SymTable *psSymTable;

	IMG_UINT32 uNumBitsForUniqueID;
	
	if (psSecondarySymbolTable)
	{
		if (psSecondarySymbolTable->psSecondarySymbolTable)
		{
			DEBUG_MESSAGE(("CreateSymTable: Multiple levels of symbol tables not supported."));
			return IMG_NULL;
		}
	}

	psSymTable = DebugMemAlloc(sizeof(SymTable));

	if (!psSymTable)
	{
		DEBUG_MESSAGE(("CreateSymTable: Failed to allocate memory for symbol table"));
		return NULL;
	}

	/* Copy the description */
	strncpy(psSymTable->acDesc, pszDesc, SYMBOL_TABLE_DESCRIPTION_LENGTH - 1);

	psSymTable->psEntries = DebugMemAlloc(sizeof(SymTableEntry) * uNumEntries);

	if (!psSymTable->psEntries)
	{
		DebugMemFree(psSymTable);

		DEBUG_MESSAGE(("CreateSymTable: Failed to allocate memory for symbol table entries"));
		return NULL;
	}

	/* Calculate number of bits available for the symbol IDs */
	if (uMaxNumBitsForSymbolTableIDs > 32)
	{
		/* Clamp to 32 bits */
		uMaxNumBitsForSymbolTableIDs = 32;
	}

	/* ~1 bit for ID for every 5 bits of entry num */
	uNumBitsForUniqueID = uMaxNumBitsForSymbolTableIDs / 6;

	psSymTable->uSymbolIDShift         = uMaxNumBitsForSymbolTableIDs - uNumBitsForUniqueID;
	psSymTable->uMaxTableSize          = (1U<<(uMaxNumBitsForSymbolTableIDs - uNumBitsForUniqueID)) - 1;	// was pow(2.0,...)-1
	psSymTable->uMaxNumUniqueSymbolIDs = (1U<<(uNumBitsForUniqueID)) - 1;	// was pow(2.0,...)-1
	psSymTable->uSymbolIDMask          = psSymTable->uMaxTableSize;

	psSymTable->uMaxNumEntries               = uNumEntries;
	psSymTable->uMaxNumBitsForSymbolTableIDs = uMaxNumBitsForSymbolTableIDs;
	psSymTable->uNumEntries                  = 0;
	psSymTable->uCurrentScopeLevel           = 0;
	psSymTable->uGetNextSymbolCounter        = 0;
	psSymTable->uGetNextSymbolScopeLevel     = 0;
	psSymTable->psSecondarySymbolTable       = psSecondarySymbolTable;

	if (!AttachSymbolTableToContext(psSymbolTableContext, psSymTable))
	{
		DEBUG_MESSAGE(("CreateSymTable: Failed to attach symbol table to context"));
		return NULL;
	}

	return psSymTable;
}

/******************************************************************************
 * Function Name: DestroySymTable
 *
 * Inputs       : -
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : Frees resources allocated by CreateSymTable
 *****************************************************************************/
IMG_INTERNAL IMG_VOID DestroySymTable(SymTable *psSymTable)
{
	IMG_UINT32 i;
	SymTableEntry *psSymTableEntry;

	for (i = 0; i < psSymTable->uNumEntries; i++)
	{
		psSymTableEntry = &psSymTable->psEntries[i];

		DebugMemFree(psSymTableEntry->pszString);

		if (psSymTableEntry->pfnSymbolDeconstructor)
		{
			psSymTableEntry->pfnSymbolDeconstructor(psSymTableEntry->pvData);
		}
		else if (psSymTableEntry->pvData)
		{
			DEBUG_MESSAGE(("DestroySymTable: No deconstructor function available to free data with"));
		}
	}

	DebugMemFree (psSymTable->psEntries);
	DebugMemFree (psSymTable);
}

/******************************************************************************
 * Function Name: FindSymbolInTable
 *
 * Inputs       : psSymTable
                  pszSymbolName - The name of the symbol to look for.
                  bCurrentScopeOnly - Whether to look only in the current scope or in any parent.
                  bSearchSecondary  - 
 * Outputs      : puSymbolID -  the symbol's ID if found
 * Returns      : IMG_TRUE if the symbol was found and its ID written into puSymbolID. IMG_FALSE otherwise.
 * Globals Used : -
 *
 * Description  : Searches through the symbol table for an entry that matches 
                  the supplied name. If found it stores the ID of the symbol in puSymbolID
 *****************************************************************************/
static IMG_BOOL FindSymbolInTable(SymTable *psSymTable,
									IMG_CHAR *pszSymbolName,
									IMG_UINT32 *puSymbolID,
									IMG_BOOL  bCurrentScopeOnly,
									IMG_BOOL  bSearchSecondary)
{
	IMG_INT32 i;

	SymTable *psCurrentSymTable = psSymTable;

	while (psCurrentSymTable)
	{
		IMG_UINT32 uMinScopeLevel = psCurrentSymTable->uCurrentScopeLevel;
		
		for (i = (IMG_INT32)(psCurrentSymTable->uNumEntries - 1); i >= 0; i--)
		{
			/* Don't check symbols that have been removed or scope modifiers */
			if (psCurrentSymTable->psEntries[i].uRefCount)
			{
				if (psCurrentSymTable->psEntries[i].bScopeModifier)
				{
					/* Have we droppped down a scope level? */
					if (psCurrentSymTable->psEntries[i-1].uScopeLevel < uMinScopeLevel)
					{
						/* Are we searching just the current scope? */
						if (bCurrentScopeOnly)
						{
							return IMG_FALSE;
						}
						else
						{
							/* Check symbols below the current level but never above it */
							uMinScopeLevel = psCurrentSymTable->psEntries[i-1].uScopeLevel;
						}
					}
				}
				else
				{
					/* Only match entries to the current scope level */
					if (psCurrentSymTable->psEntries[i].uScopeLevel == uMinScopeLevel) 
					{
						if (strcmp(pszSymbolName, psCurrentSymTable->psEntries[i].pszString) == 0)
						{
							if (puSymbolID)
							{
								*puSymbolID = i | psCurrentSymTable->uUniqueSymbolTableID;
							}
							
							return IMG_TRUE;
						}
					}
				}
			}
		}

		if (bSearchSecondary)
		{
			psCurrentSymTable = psCurrentSymTable->psSecondarySymbolTable;
		}
		else
		{
			psCurrentSymTable = IMG_NULL;
		}
	}

	return IMG_FALSE;
}

/******************************************************************************
 * Function Name: FindSymbol
 *
 * Inputs       : psSymTable, pSymbolName
 * Outputs      : puSymbolIDNode type specific data
 * Returns      : Success / Failure
 * Globals Used : -
 *
 * Description  :
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL FindSymbol(SymTable *psSymTable,
								IMG_CHAR *pszSymbolName,
								IMG_UINT32 *puSymbolID,
								IMG_BOOL  bCurrentScopeOnly)
{
	return FindSymbolInTable(psSymTable,
							 pszSymbolName,
							 puSymbolID,
							 bCurrentScopeOnly,
							 IMG_TRUE);
}




/******************************************************************************
 * Function Name: GetSymbolTableEntry
 *
 * Inputs       : -
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static SymTableEntry *GetSymbolTableEntry(SymTable *psSymTable, IMG_UINT32 uSymbolID)
{
	IMG_UINT32  uSymbolIndex        = uSymbolID & psSymTable->uSymbolIDMask;
	IMG_UINT32  uSymbolTableID      = uSymbolID & ~(psSymTable->uSymbolIDMask);
	SymTable   *psSecondarySymTable = psSymTable->psSecondarySymbolTable;

	SymTableEntry *psSymTableEntry;

	SymTable *psCurrentSymTable;

	/* Check symbol ID is valid */
	if (uSymbolTableID == psSymTable->uUniqueSymbolTableID)
	{
		psCurrentSymTable = psSymTable;
	}
	else if (psSecondarySymTable && (uSymbolTableID == psSymTable->psSecondarySymbolTable->uUniqueSymbolTableID))
	{
		psCurrentSymTable = psSecondarySymTable;
	}
	else
	{
		DEBUG_MESSAGE(("GetSymbolTableEntry: Invalid symbol ID (%08X)\n",uSymbolID));
		return IMG_NULL;
	}

	/* Check it's in range */
	if (uSymbolIndex > psCurrentSymTable->uNumEntries)
	{
		DEBUG_MESSAGE(("CheckSymbolID: Symbol ID out of range (%d/%d)\n",uSymbolIndex, psCurrentSymTable->uNumEntries));
		return IMG_NULL;
	}

	/* Get the entry */
	psSymTableEntry = &(psCurrentSymTable->psEntries[uSymbolIndex]);

	/* Check if it exists */
	if (!psSymTableEntry->uSymbolID)
	{
		DEBUG_MESSAGE(("GetSymbolTableEntry: Symbol already removed\n"));
		return IMG_NULL;
	}

	return psSymTableEntry;
}

/******************************************************************************
 * Function Name: GetSymbolData
 *
 * Inputs       : psSymTable, uSymbolID
 * Outputs      : -
 * Returns      : Data associated with symbol
 * Globals Used : -
 *
 * Description  : Returns data assocated with the entry in the symbol table
 *****************************************************************************/
IMG_INTERNAL IMG_VOID *GetSymbolDatafn(IMG_UINT32 uLineNumber,
									   IMG_CHAR *pszFileName,
									   SymTable *psSymTable,
									   IMG_UINT32 uSymbolID)
{
	SymTableEntry *psSymTableEntry = GetSymbolTableEntry(psSymTable, uSymbolID);

	PVR_UNREFERENCED_PARAMETER(uLineNumber);
	PVR_UNREFERENCED_PARAMETER(pszFileName);

	if (!psSymTableEntry)
	{
		DEBUG_MESSAGE(("GetSymbolData: Failed to get symbol table entry (%08X) (%s line:%d)\n", uSymbolID, pszFileName, uLineNumber));
		return IMG_NULL;
	}

	return psSymTableEntry->pvData;
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
static IMG_BOOL IncreaseSymbolReferenceCount(SymTable *psSymTable, IMG_UINT32  uSymbolID)

{
	SymTableEntry *psSymTableEntry = GetSymbolTableEntry(psSymTable, uSymbolID);

	if (!psSymTableEntry)
	{
		DEBUG_MESSAGE(("IncreaseSymbolReferenceCount: Failed to get symbol table entry"));
		return IMG_FALSE;
	}

#if defined(DEBUG) && defined(COMPACT_MEMORY_MODEL)

	if (psSymTableEntry->uRefCount > SYMBOL_TABLE_REF_COUNT)
	{		
		DEBUG_MESSAGE(("uRefCount to large to fit inside compacted symbol structure\n Try building compiler without COMPACT_MEMORY_MODEL=1"));
		return IMG_FALSE;
	}

#endif


	/* Increase reference count */
	psSymTableEntry->uRefCount++;

#ifdef DUMP_LOGFILES
	DumpLogMessage(LOGFILE_SYMBOLTABLE,
				   psSymTable->uCurrentScopeLevel,
				   "(%d - %d) Increasing reference count of '%s' to %d, ID = %08X\n", 
				   psSymTable->uNumEntries,
				   psSymTable->uCurrentScopeLevel,
				   psSymTableEntry->pszString,
				   psSymTableEntry->uRefCount,
				   uSymbolID);
#endif
	
	return IMG_TRUE;
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
static IMG_BOOL CheckTableSize(SymTable *psSymTable)
{
	if (psSymTable->uNumEntries >= psSymTable->uMaxTableSize)
	{
		DEBUG_MESSAGE(("Maximum table size reached no more entries allowed"));
		return IMG_FALSE;
	}

	/* If we've run out of space we need to resize the table */
	if (psSymTable->uNumEntries >= psSymTable->uMaxNumEntries)
	{

#ifdef COMPACT_MEMORY_MODEL
		IMG_UINT32	uNewNumEntries = psSymTable->uMaxNumEntries + 100;
#else
		IMG_UINT32	uNewNumEntries = psSymTable->uMaxNumEntries * 2;
#endif

		if (uNewNumEntries > psSymTable->uMaxTableSize)
		{
			uNewNumEntries = psSymTable->uMaxTableSize;
		}

		psSymTable->psEntries = DebugMemRealloc(psSymTable->psEntries, uNewNumEntries * sizeof(SymTableEntry));

		if (!psSymTable->psEntries)
		{
			DEBUG_MESSAGE(("Failed to resize memory for symbol table entries"));
			return IMG_FALSE;
		}

		psSymTable->uMaxNumEntries = uNewNumEntries;
	}

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: AddSymbolToTable
 *
 * Inputs       : psSymTable, pSymbolName, data
 * Outputs      : -
 * Returns      : Sucess / Failure
 * Globals Used : -
 *
 * Description  : Adds and entry to the symbol table    
 *****************************************************************************/
static IMG_BOOL AddSymbolToTable(SymTable                  *psSymTable,
								IMG_CHAR                  *pszSymbolName,
								IMG_VOID                  *data, 
								IMG_UINT32                   uDataSizeInBytes,
								IMG_BOOL                   bAllowDuplicates,
								IMG_BOOL                   bScopeModifier,
								IMG_UINT32                  *puSymbolID,
								PFN_SYMBOL_DECONSTRUCTOR   pfnSymbolDeconstructor)
{
	SymTableEntry *psSymTableEntry;

	/* If we've run out of space we need to resize the table */
	if (!CheckTableSize(psSymTable))
	{
		return IMG_FALSE;
	}

	/* Check symbol will fit inside compacted structue */
#if defined(DEBUG) && defined(COMPACT_MEMORY_MODEL)

	if (uDataSizeInBytes > SYMBOL_TABLE_MAX_DATA_SIZE_IN_BYTES)
	{
		DEBUG_MESSAGE(("uDataSizeInBytes to large to fit inside compacted symbol structure\n Try building compiler without COMPACT_MEMORY_MODEL=1"));
		return IMG_FALSE;
	}

	if (psSymTable->uCurrentScopeLevel > SYMBOL_TABLE_MAX_SCOPE_LEVELS)
	{
		DEBUG_MESSAGE(("uScopeLevel to large to fit inside compacted symbol structure\n Try building compiler without COMPACT_MEMORY_MODEL=1"));
		return IMG_FALSE;
	}

#endif


	if (!bScopeModifier)
	{
		/* Don't search the secondary table, if an item exists there this one will replace it */
		if (FindSymbolInTable(psSymTable, pszSymbolName, puSymbolID, IMG_TRUE, IMG_FALSE))
		{
			if (bAllowDuplicates)
			{

				psSymTableEntry = GetSymbolTableEntry(psSymTable, *puSymbolID);

				if (uDataSizeInBytes != psSymTableEntry->uDataSizeInBytes)
				{
					DEBUG_MESSAGE(("Symbol with duplicate name has different sized data"));
					return IMG_FALSE;
				}

				/* Increase the reference count of this symbol */
				IncreaseSymbolReferenceCount(psSymTable, *puSymbolID);

				/* Call the free function on the provided data */
				pfnSymbolDeconstructor(data);

				return IMG_TRUE;
			}
			else
			{
				DEBUG_MESSAGE(("Symbol already exists with name '%s'", pszSymbolName));
				return IMG_FALSE;
			}
		}
	}

	psSymTableEntry = &psSymTable->psEntries[psSymTable->uNumEntries];

	psSymTableEntry->pszString = DebugMemAlloc(sizeof(IMG_CHAR) * (strlen(pszSymbolName) + 1));

	/* Check memory alloc for name succeeded */
	if (!psSymTableEntry->pszString)
	{
			DEBUG_MESSAGE(("Failed to alloc memory for symbol name"));
			return IMG_FALSE;
	}

	/* Add name of entry */
	strcpy(psSymTableEntry->pszString, pszSymbolName);

	/* Store pointer to data */
	psSymTableEntry->pvData =  data;

	/* Store size of data */
	psSymTableEntry->uDataSizeInBytes = uDataSizeInBytes;

	/* Store the scope level at which the entry was added */
	psSymTableEntry->uScopeLevel = psSymTable->uCurrentScopeLevel;

	/* Initialise the reference count */
	psSymTableEntry->uRefCount   = 1;

	/* Create a symbol ID */
	psSymTableEntry->uSymbolID = (psSymTable->uNumEntries | psSymTable->uUniqueSymbolTableID);

	/* Not a scope modifier */
	psSymTableEntry->bScopeModifier = bScopeModifier;

	/* Store the deconstructor function */
	psSymTableEntry->pfnSymbolDeconstructor = pfnSymbolDeconstructor;

	if (puSymbolID)
	{
		/* Copy this symbol ID back */
		*puSymbolID = psSymTableEntry->uSymbolID;
	}

#ifdef DUMP_LOGFILES
	/* Dump a message to the log file */
	DumpLogMessage(LOGFILE_SYMBOLTABLE,
				   psSymTable->uCurrentScopeLevel,
				   "(%d - %d) Adding '%s' to the symbol table, ID = %08X\n",
				   psSymTable->uNumEntries,
				   psSymTable->uCurrentScopeLevel,
				   psSymTableEntry->pszString,
				   psSymTableEntry->uSymbolID);
#endif

	psSymTable->uNumEntries++;

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: AddSymbol
 *
 * Inputs       : psSymTable, pSymbolName, data
 * Outputs      : -
 * Returns      : Sucess / Failure
 * Globals Used : -
 *
 * Description  : Adds and entry to the symbol table    
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL AddSymbol(SymTable                 *psSymTable,
								IMG_CHAR                 *pszSymbolName,
								IMG_VOID                 *data,
								IMG_UINT32                  uDataSizeInBytes,
								IMG_BOOL                  bAllowDuplicates,
								IMG_UINT32                 *puSymbolID,
								PFN_SYMBOL_DECONSTRUCTOR  pfnSymbolDeconstructor)
{
	return AddSymbolToTable(psSymTable,
							pszSymbolName,
							data,
							uDataSizeInBytes,
							bAllowDuplicates,
							IMG_FALSE,
							puSymbolID,
							pfnSymbolDeconstructor);
}


/******************************************************************************
 * Function Name: RemoveSymbol
 *
 * Inputs       : -
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL RemoveSymbol(SymTable *psSymTable, IMG_UINT32 uSymbolID)
{
	SymTableEntry *psSymTableEntry = GetSymbolTableEntry(psSymTable, uSymbolID);

	if (!psSymTableEntry)
	{
		DEBUG_MESSAGE(("RemoveSymbol: Failed to get symbol table entry"));
		return IMG_FALSE;
	}

	/* Reduce reference count */
	psSymTableEntry->uRefCount--;

	if (!psSymTableEntry->uRefCount)
	{

#ifdef DUMP_LOGFILES
		DumpLogMessage(LOGFILE_SYMBOLTABLE,
					   0,
					   "Removing '%s' from the symbol table, ID = %08X\n",
					   psSymTableEntry->pszString,
					   uSymbolID);
#endif

		/* Mark symbol as destroyed */
		psSymTableEntry->uSymbolID = 0;
	}
	else
	{
#ifdef DUMP_LOGFILES
		DumpLogMessage(LOGFILE_SYMBOLTABLE,
					   0,
					   "Decreasing reference count of '%s' to %d, ID = %08X\n",
					   psSymTableEntry->pszString,
					   psSymTableEntry->uRefCount,
					   uSymbolID);
#endif
	}

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: GetSymbolName
 *
 * Inputs       : psSymTable, uSymbolID
 * Outputs      : -
 * Returns      : Name associated with symbol
 * Globals Used : -
 *
 * Description  : Returns name assocated with the entry in the symbol table    
 *****************************************************************************/
IMG_INTERNAL IMG_CHAR *GetSymbolNamefn(IMG_UINT32 uLineNumber,
									   IMG_CHAR *pszFileName,
									   SymTable *psSymTable,
									   IMG_UINT32 uSymbolID)
{
	SymTableEntry *psSymTableEntry = GetSymbolTableEntry(psSymTable, uSymbolID);

	PVR_UNREFERENCED_PARAMETER(uLineNumber);
	PVR_UNREFERENCED_PARAMETER(pszFileName);

	if (!psSymTableEntry)
	{
		DEBUG_MESSAGE(("GetSymbolName: Failed to get symbol table entry (%s line:%d)\n", pszFileName, uLineNumber));
		return IMG_NULL;
	}

	return psSymTableEntry->pszString;
}

/******************************************************************************
 * Function Name: IncreaseScopeLevel
 *
 * Inputs       : psSymTable
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : Increases the current scope level  
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL IncreaseScopeLevel(SymTable *psSymTable)
{
	char acString[50];

	psSymTable->uCurrentScopeLevel++;

	sprintf(acString, "@---- ScopeModifer %03u ----@", psSymTable->uCurrentScopeLevel);

	return AddSymbolToTable(psSymTable,
							acString,
							IMG_NULL,
							0,
							IMG_TRUE,
							IMG_TRUE,
							IMG_NULL,
							IMG_NULL);

}

/******************************************************************************
 * Function Name: DecreaseScopeLevel
 *
 * Inputs       : psSymTable
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : Decreases the current scope level
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL DecreaseScopeLevel(SymTable *psSymTable)
{
	if (psSymTable->uCurrentScopeLevel == 0)
	{
		DEBUG_MESSAGE(("DecreaseScopeLevel: Error cannot decrease scope level any further"));
		return IMG_FALSE;
	}
	else
	{
		char acString[50];

		psSymTable->uCurrentScopeLevel--;

		sprintf(acString, "@---- ScopeModifer %03u ----@", psSymTable->uCurrentScopeLevel);

		return AddSymbolToTable(psSymTable,
								acString,
								IMG_NULL,
								0,
								IMG_TRUE,
								IMG_TRUE,
								IMG_NULL,
								IMG_NULL);
	}


}

/******************************************************************************
 * Function Name: GetScopeLevel
 *
 * Inputs       : psSymTable
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  :     
 *****************************************************************************/
IMG_INTERNAL IMG_UINT32 GetScopeLevel(SymTable *psSymTable)
{
	return psSymTable->uCurrentScopeLevel;
}

/******************************************************************************
 * Function Name: GetScopeLevel
 *
 * Inputs       : psSymTable
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL GetSymbolScopeLevelfn(IMG_UINT32 uLineNumber,
											IMG_CHAR *pszFileName,
											SymTable *psSymTable,
											IMG_UINT32 uSymbolID,
											IMG_UINT32 *puScopeLevel)
{
	SymTableEntry *psSymTableEntry = GetSymbolTableEntry(psSymTable, uSymbolID);

	PVR_UNREFERENCED_PARAMETER(uLineNumber);
	PVR_UNREFERENCED_PARAMETER(pszFileName);

	if (!psSymTableEntry)
	{
		DEBUG_MESSAGE(("GetSymbolScopeLevel: Failed to get symbol table entry (%08X) (%s line:%u)", uSymbolID, pszFileName, uLineNumber));
		return IMG_FALSE;
	}

	*puScopeLevel = psSymTableEntry->uScopeLevel;

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: 
 *
 * Inputs       : psSymTable
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  :     
 *****************************************************************************/
IMG_INTERNAL IMG_VOID ResetGetNextSymbolCounter(SymTable *psSymTable)
{
	psSymTable->uGetNextSymbolCounter = 0;
	psSymTable->uGetNextSymbolScopeLevel = 0;
}

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : psSymTable
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_UINT32 GetNextSymbol(SymTable *psSymTable, IMG_BOOL bSkipScopeChanges)
{
	IMG_UINT32 i;

	for (i = psSymTable->uGetNextSymbolCounter; i < psSymTable->uNumEntries; i++)
	{
		SymTableEntry *psSymTableEntry = &(psSymTable->psEntries[i]);

		/* Increase counter */
		psSymTable->uGetNextSymbolCounter++;

		/* Scope modifiers aren't valid symbols to return */
		if (!psSymTableEntry->bScopeModifier)
		{
			/* If we're not worried about scope level changes then return the next symbol on the list */
			if (bSkipScopeChanges)
			{
				/* Only return entries that match the current scope level */
				if (psSymTableEntry->uScopeLevel == psSymTable->uGetNextSymbolScopeLevel)
				{
					return psSymTableEntry->uSymbolID;
				}
			}
			else
			{
				/* Update the current scope level */
				psSymTable->uGetNextSymbolScopeLevel = psSymTableEntry->uScopeLevel;
				
				return psSymTableEntry->uSymbolID;
			}
		}
	}

	return 0;
}

/******************************************************************************
 End of file (symtab.c)
******************************************************************************/
