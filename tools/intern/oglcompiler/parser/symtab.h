/**************************************************************************
 * Name         : symtab.h
 * Author       : James McCarthy
 * Created      : 25/11/2003
 *
 * Copyright    : 2000-2003 by Imagination Technologies Limited. All rights reserved.
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
 **************************************************************************/

#ifndef __gl_symtab_h_
#define __gl_symtab_h_

#include "img_types.h"



typedef IMG_VOID (*PFN_SYMBOL_DECONSTRUCTOR)(IMG_VOID *);

#ifdef COMPACT_MEMORY_MODEL

/* Numbers must match bitfield definitions below */
#define SYMBOL_TABLE_MAX_SCOPE_LEVELS       ((1 << 7)  - 1)
#define SYMBOL_TABLE_REF_COUNT              ((1 << 9)  - 1) 
#define SYMBOL_TABLE_MAX_DATA_SIZE_IN_BYTES ((1 << 15) - 1)

typedef struct SymTableEntry_TAG
{
	IMG_CHAR                 *pszString;
	IMG_UINT32                uSymbolID;
	IMG_UINT32                bScopeModifier     : 1;
	IMG_UINT32                uScopeLevel        : 7; 
	IMG_UINT32                uRefCount          : 9;
	IMG_UINT32                uDataSizeInBytes   : 15;
	IMG_VOID                 *pvData;
	PFN_SYMBOL_DECONSTRUCTOR  pfnSymbolDeconstructor;
} SymTableEntry;

#else

typedef struct SymTableEntry_TAG
{
	IMG_CHAR                 *pszString;
	IMG_UINT32                uSymbolID;
	IMG_UINT32                bScopeModifier;
	IMG_UINT32                uScopeLevel;
	IMG_UINT32                uRefCount;
	IMG_UINT32                uDataSizeInBytes;
	IMG_VOID                 *pvData;
	PFN_SYMBOL_DECONSTRUCTOR  pfnSymbolDeconstructor;
} SymTableEntry;

#endif // COMPACT_MEMORY_MODEL

#define SYMBOL_TABLE_DESCRIPTION_LENGTH 20 

typedef struct SymTable_TAG
{
	IMG_CHAR             acDesc[SYMBOL_TABLE_DESCRIPTION_LENGTH];
	IMG_UINT32           uNumEntries;
	IMG_UINT32           uMaxNumEntries;
	IMG_UINT32           uMaxNumBitsForSymbolTableIDs;
	IMG_UINT32           uSymbolIDShift;
	IMG_UINT32           uSymbolIDMask;
	IMG_UINT32           uMaxTableSize;
	IMG_UINT32           uMaxNumUniqueSymbolIDs;
	IMG_UINT32           uCurrentScopeLevel;
	IMG_UINT32           uUniqueSymbolTableID;
	IMG_UINT32           uGetNextSymbolCounter;
	IMG_UINT32           uGetNextSymbolScopeLevel;
	struct SymTable_TAG *psSecondarySymbolTable;
	SymTableEntry       *psEntries;
} SymTable;

typedef struct SymbolTableContextTAG
{
	IMG_UINT32 uSymbolTableListSize;
	SymTable **ppsSymbolTables;
} SymbolTableContext;


SymbolTableContext *InitSymbolTableManager(IMG_VOID);

void DestroySymbolTableManager(SymbolTableContext *psSymbolTableContext);

IMG_BOOL RemoveSymbolTableFromManager(SymbolTableContext *psSymbolTableContext,
									  SymTable           *psSymbolTable);

SymTable *CreateSymTable(SymbolTableContext *psSymbolTableContext, 
						 IMG_CHAR           *pszDesc,
						 IMG_UINT32          uNumEntries, 
						 IMG_UINT32          uMaxNumBitsForSymbolTableIDs,
						 SymTable           *psSecondarySymbolTable);


IMG_VOID DestroySymTable(SymTable *psSymTable);

IMG_BOOL FindSymbol(SymTable *psSymTable, 
					IMG_CHAR *pszSymbolName, 
					IMG_UINT32 *puSymbolID,
					IMG_BOOL  bCurrentScopeOnly);

IMG_BOOL AddSymbol(SymTable                 *psSymTable, 
				   IMG_CHAR                 *pszSymbolName, 
				   IMG_VOID                 *data, 
				   IMG_UINT32                uDataSizeInBytes,
				   IMG_BOOL                  bAllowDuplicates,
				   IMG_UINT32               *puSymbolID,
				   PFN_SYMBOL_DECONSTRUCTOR  pfnSymbolDeconstructor);

IMG_BOOL IncreaseScopeLevel(SymTable *psSymTable);

IMG_BOOL DecreaseScopeLevel(SymTable *psSymTable);

IMG_VOID *GetSymbolDatafn(IMG_UINT32 uLineNumber, IMG_CHAR *pszFileName, SymTable *psSymTable, IMG_UINT32 uSymbolID);

IMG_CHAR *GetSymbolNamefn(IMG_UINT32 uLineNumber, IMG_CHAR *pszFileName, SymTable *psSymTable, IMG_UINT32 uSymbolID);

IMG_BOOL RemoveSymbol(SymTable *psSymTable, IMG_UINT32 uSymbolID);

IMG_UINT32 GetScopeLevel(SymTable *psSymTable);

IMG_VOID ResetGetNextSymbolCounter(SymTable *psSymTable);

IMG_UINT32 GetNextSymbol(SymTable *psSymTable, IMG_BOOL bSkipScopeChanges);

IMG_BOOL  GetSymbolScopeLevelfn(IMG_UINT32 uLineNumber, IMG_CHAR *pszFileName, SymTable *psSymTable, IMG_UINT32 uSymbolID, IMG_UINT32 *puScopeLevel);

#define GetSymbolData(a, b) GetSymbolDatafn(__LINE__, __FILE__, a, b) 

#define GetSymbolName(a, b) GetSymbolNamefn(__LINE__, __FILE__, a, b) 

#define GetSymbolScopeLevel(a, b, c) GetSymbolScopeLevelfn(__LINE__, __FILE__, a, b, c) 


#endif /* __gl_symtab_h_ */
