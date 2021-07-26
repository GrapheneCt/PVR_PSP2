/******************************************************************************
 * Name         : scopeman.h
 * Title        : Scope Management
 * Author       : Rana-Iftikhar Ahmad
 * Created      : Nov 2007
 *
 * Copyright    : 2002-2007 by Imagination Technologies Limited.
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
 * $Log: scopeman.h $
 *****************************************************************************/

#ifndef __USEASM_SCOPEMAN_H
#define __USEASM_SCOPEMAN_H

#include "sgxdefs.h"
#include "use.h"

#if !defined(IMG_UINT32_MAX)
	#define IMG_UINT32_MAX 0xFFFFFFFFUL
#endif

IMG_BOOL IsAllDigitsString(const IMG_CHAR* pszString);

IMG_VOID EnableScopeManagement(IMG_VOID);

IMG_BOOL IsScopeManagementEnabled(IMG_VOID);

IMG_VOID AddNamedRegister(const IMG_CHAR* pszName, 
						  const IMG_CHAR* pszSourceFile, 
						  IMG_UINT32 uSourceLine);

IMG_VOID AddNamedRegistersArray(const IMG_CHAR* pszName, 
								IMG_UINT32 uArrayElementsCount, 
								const IMG_CHAR* pszSourceFile, 
								IMG_UINT32 uSourceLine);

IMG_BOOL LookupNamedRegister(const IMG_CHAR* pszName, 
							 IMG_PUINT32 puNamedRegLink);

IMG_VOID RenameNamedRegister(const IMG_CHAR* pszCurrentName, 
							 const IMG_CHAR* pszCurrSourceFile, 
							 IMG_UINT32 uCurrSourceLine, 
							 const IMG_CHAR* pszNewName, 
							 const IMG_CHAR* pszNewSourceFile, 
							 IMG_UINT32 uNewSourceLine);

IMG_UINT32 GetArrayElementsCount(IMG_UINT32 uNamedRegLink);

IMG_BOOL IsArray(IMG_UINT32 uNamedRegLink);

const IMG_CHAR* GetNamedTempRegName(IMG_UINT32 uNamedRegLink);

IMG_VOID InitScopeManagementDataStructs(IMG_VOID);

IMG_UINT32 OpenScope(const IMG_CHAR* pszScopeName, 
					 IMG_BOOL bProc, 
					 IMG_BOOL bImported,
					 const IMG_CHAR* pszSourceFile, 
					 IMG_UINT32 uSourceLine);

IMG_UINT32 ReOpenLastScope(const IMG_CHAR* pszSourceFile, 
						   IMG_UINT32 uSourceLine);

IMG_VOID CloseScope(IMG_VOID);

IMG_BOOL IsCurrentScopeLinkedToModifiableLabel(
	IMG_UINT32 uLinkToModifiableLabelsTable);

IMG_BOOL LookupParentScopes(const IMG_CHAR* pszName, IMG_HVOID ppvHandle);

IMG_BOOL LookupProcScope(const IMG_CHAR* pszScopeName, 
						 const IMG_CHAR* pszSourceFile, 
						 IMG_UINT32 uSourceLine, IMG_HVOID ppvHandle);

IMG_VOID AddUndefinedNamedRegisterInScope(IMG_PVOID pvScopeHandle, 
										  const IMG_CHAR* pszName, 
										  const IMG_CHAR* pszSourceFile, 
										  IMG_UINT32 uSourceLine, 
										  IMG_PUINT32 puNamedRegLink);

IMG_VOID AddUndefinedNamedRegsArrayInScope(IMG_PVOID pvScopeHandle, 
										   const IMG_CHAR* pszName, 
										   IMG_UINT32 uArrayElementsCount, 
										   const IMG_CHAR* pszSourceFile, 
										   IMG_UINT32 uSourceLine, 
										   IMG_PUINT32 puNamedRegLink);

typedef enum tagSCOPE_TYPE
{
	EXPORT_SCOPE = 0,
	PROCEDURE_SCOPE = 1,
	BRANCH_SCOPE = 2	
} SCOPE_TYPE;

IMG_UINT32 FindScope(const IMG_CHAR* pszScopeName, 
					 SCOPE_TYPE eScopeType, 
					 const IMG_CHAR* pszSourceFile, 
					 IMG_UINT32 uSourceLine);

IMG_BOOL LookupTemporaryRegisterInScope(const IMG_VOID* pvScopeHandle, 
										const IMG_CHAR* pszName, 
										IMG_PUINT32 puNamedRegLink);

IMG_VOID VerifyThatAllScopesAreEnded(IMG_VOID);

IMG_BOOL IsCurrentScopeGlobal(IMG_VOID);

IMG_VOID AllocHwRegsToNamdTempRegsAndSetLabelRefs(PUSE_INST psInst);

IMG_UINT32 GetHwRegsAllocRangeStartForBinary(IMG_VOID);

IMG_UINT32 GetHwRegsAllocRangeEndForBinary(IMG_VOID);

IMG_PCHAR GetScopeOrLabelName(const IMG_CHAR* pszName);

IMG_VOID DeleteScopeOrLabelName(IMG_CHAR** ppszName);

IMG_VOID InitHwRegsAllocRange(IMG_VOID);

IMG_BOOL IsHwRegsAllocRangeSet(IMG_VOID);

IMG_VOID SetHwRegsAllocRange(IMG_UINT32 uRangeStart, IMG_UINT32 uRangeEnd, 
							 IMG_BOOL bOverideFileRange);

IMG_VOID SetHwRegsAllocRangeFromFile(IMG_UINT32 uRangeStart, 
									 IMG_UINT32 uRangeEnd);

#endif /* __USEASM_SCOPEMAN_H */

/******************************************************************************
 End of file (scopeman.h)
******************************************************************************/
