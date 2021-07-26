/*************************************************************************
 * Name         : ctree.c
 * Title        : USE assembler
 * Author       : David Welch
 * Created      : Dec 2005
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
 *
 * Modifications:-
 * $Log: ctree.c $
 **************************************************************************/

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>

#include "img_types.h"
#include "useasm.h"
#include "ctree.h"
#include "osglue.h"

#if defined(LINUX)
#define min(a,b) a>b?b:a
#define max(a,b) a>b?a:b
#endif 


enum STORAGE
{
	STORAGE_UNKNOWN,
	STORAGE_TYPEDEF,
	STORAGE_EXTERN,
	STORAGE_STATIC,
	STORAGE_AUTO,
	STORAGE_REGISTER,
};

enum TYPECLASS
{
	TYPECLASS_UNKNOWN,
	TYPECLASS_BASE,
	TYPECLASS_POINTER,
	TYPECLASS_TYPEDEF,
	TYPECLASS_UNION,
	TYPECLASS_STRUCT,
	TYPECLASS_ENUM,
	TYPECLASS_FIXEDARRAY,
	TYPECLASS_VARIABLEARRAY,
	TYPECLASS_FUNCTION,
	TYPECLASS_COUNT,
};

enum ATOMICTYPE
{
	ATOMICTYPE_UNKNOWN,
	ATOMICTYPE_VOID,
	ATOMICTYPE_UNSIGNEDCHAR,
	ATOMICTYPE_SIGNEDCHAR,
	ATOMICTYPE_UNSIGNEDSHORT,
	ATOMICTYPE_SIGNEDSHORT,
	ATOMICTYPE_UNSIGNEDINT,
	ATOMICTYPE_SIGNEDINT,
	ATOMICTYPE_UNSIGNEDLONG,
	ATOMICTYPE_SIGNEDLONG,
	ATOMICTYPE_FLOAT,
	ATOMICTYPE_DOUBLE,
	ATOMICTYPE_LONGDOUBLE,
	ATOMICTYPE_UNSIGNEDLONGLONG,
	ATOMICTYPE_SIGNEDLONGLONG,
	ATOMICTYPE_GCC_BUILTIN_VA_LIST,
	ATOMICTYPE_BOOL,
	ATOMICTYPE_COUNT,
};

struct _CTTYPE;

typedef struct _CTSTRUCTMEMBER
{
	struct _CTTYPE*		psType;
	IMG_PCHAR			pszName;
	IMG_UINT32			uBitFieldSize;
	IMG_UINT32			uByteOffset;
} CTSTRUCTMEMBER;

typedef struct _CTENUMMEMBER
{
	IMG_PCHAR			pszName;
	IMG_INT32			iValue;
} CTENUMMEMBER;

typedef struct _CTTYPE
{
	enum TYPECLASS		eTypeClass;

	YYLTYPE				sLocation;

	IMG_BOOL			bAnonymousType;

	IMG_UINT32			uTypeNum;

	IMG_PCHAR			pszName;

	enum ATOMICTYPE		eAtomicType;

	IMG_BOOL			bVolatile;
	IMG_BOOL			bRestrict;
	IMG_BOOL			bConst;

	struct _CTTYPE*		psSubType;

	IMG_UINT32			uStructMemberCount;
	CTSTRUCTMEMBER*		psStructMembers;
	IMG_UINT32			uStructSize;
	IMG_UINT32			uStructAlign;
	IMG_BOOL			bStructVariableLength;
	IMG_BOOL			bCompletedStructure;

	IMG_UINT32			uEnumeratorCount;
	CTENUMMEMBER*		psEnumerators;

	IMG_UINT32			uArraySize;

	struct _CTTYPE*		psNext;
	struct _CTTYPE*		psHashNext;
} CTTYPE, *PCTTYPE;

static IMG_VOID CTreeError(YYLTYPE sLocation, IMG_PCHAR pszError, ...) IMG_FORMAT_PRINTF(2, 3);

static enum STORAGE GetStorageClass(YZDECLSPECIFIER* psDeclSpecifierList);
static PCTTYPE GetType(YZDECLSPECIFIER* psDeclSpecifierList);
static PCTTYPE GetDeclaratorType(YZDECLARATOR* psDeclarator, PCTTYPE psAtomicType);

#define TYPE_HASH_TABLE_SIZE		(1023)

typedef struct
{
	PCTTYPE		asHead[TYPE_HASH_TABLE_SIZE];
} TYPECLASS_HASH_TABLE, *PTYPECLASS_HASH_TABLE;

#define ENUMERATOR_HASH_TABLE_SIZE	(4095)

typedef struct _ENUMERATOR
{
	PCTTYPE				psType;
	IMG_UINT32			uIndex;
	struct _ENUMERATOR*	psHashNext;
} ENUMERATOR, *PENUMERATOR;

typedef struct
{
	PENUMERATOR	asHead[ENUMERATOR_HASH_TABLE_SIZE];
} ENUMERATOR_HASH_TABLE, PENUMERATOR_HASH_TABLE;

static ENUMERATOR_HASH_TABLE	g_sEnumeratorHashTable;

static IMG_UINT32		g_uTypeCount = 0;

#if defined (POINTERSIZE)
static IMG_UINT32		g_uPointerSize = POINTERSIZE;
#else
static IMG_UINT32		g_uPointerSize = 4;
#endif

static PCTTYPE				g_psTypeListHead = NULL;
static PCTTYPE				g_psTypeListTail = NULL;
static TYPECLASS_HASH_TABLE	g_asTypeClassHashTable[TYPECLASS_COUNT];

static IMG_UINT32		g_pdwAtomicTypesSize[] = { 0 /* ATOMICTYPE_UNKNOWN */,
												 0 /* ATOMICTYPE_VOID */,
											     1 /* ATOMICTYPE_UNSIGNEDCHAR */,
											     1 /* ATOMICTYPE_SIGNEDCHAR */,
											     2 /* ATOMICTYPE_UNSIGNEDSHORT */,
											     2 /* ATOMICTYPE_SIGNEDSHORT */,
											     4 /* ATOMICTYPE_UNSIGNEDINT */,
											     4 /* ATOMICTYPE_SIGNEDINT */,
											     4 /* ATOMICTYPE_UNSIGNEDLONG */,
											     4 /* ATOMICTYPE_SIGNEDLONG */,
											     4 /* ATOMICTYPE_FLOAT */,
											     8 /* ATOMICTYPE_DOUBLE */,
											     8 /* ATOMICTYPE_LONGDOUBLE */,
											     8 /* ATOMICTYPE_UNSIGNEDLONGLONG */,
											     8 /* ATOMICTYPE_SIGNEDLONGLONG */,
												 0 /* ATOMICTYPE_GCC_BUILTIN_VALIST */,
												 1 /* ATOMICTYPE_BOOL */};

static PCTTYPE LookupAtomicType(enum ATOMICTYPE eAtomicType);

static IMG_UINT32 StringToHash(IMG_PCHAR pszString)
{
	IMG_UINT32 uHash;

	uHash = 0;
	while (*pszString)
	{
		uHash += *pszString++;
	}

	return uHash;
}

static IMG_BOOL IsStruct(PCTTYPE psType)
{
	switch (psType->eTypeClass)
	{
		case TYPECLASS_TYPEDEF:
		{
			return IsStruct(psType->psSubType);		// PRQA S 3670
		}
		case TYPECLASS_STRUCT:
		{
			return IMG_TRUE;
		}
		default:
		{
			return IMG_FALSE;
		}
	}
}

static IMG_BOOL IsUnion(PCTTYPE psType)
{
	switch (psType->eTypeClass)
	{
		case TYPECLASS_TYPEDEF:
		{
			return IsUnion(psType->psSubType);		// PRQA S 3670
		}
		case TYPECLASS_UNION:
		{
			return IMG_TRUE;
		}
		default:
		{
			return IMG_FALSE;
		}
	}
}

static IMG_BOOL IsVariableLengthArray(PCTTYPE psType)
{
	switch (psType->eTypeClass)
	{
		case TYPECLASS_TYPEDEF:
		{
			return IsVariableLengthArray(psType->psSubType);	// PRQA S 3670
		}
		case TYPECLASS_FIXEDARRAY:
		{
			return IsVariableLengthArray(psType->psSubType);	// PRQA S 3670
		}
		case TYPECLASS_VARIABLEARRAY:
		{
			return IMG_TRUE;
		}
		default:	
		{
			return IMG_FALSE;
		}
	}
}

static IMG_BOOL IsCompleteType(PCTTYPE psType)
{
	switch (psType->eTypeClass)
	{
		case TYPECLASS_BASE:
		{
			assert(psType->eAtomicType != ATOMICTYPE_UNKNOWN);
			if (psType->eAtomicType != ATOMICTYPE_VOID)
			{
				return IMG_TRUE;
			}
			else
			{
				return IMG_FALSE;
			}
		}
		case TYPECLASS_POINTER:
		{
			return IMG_TRUE;
		}
		case TYPECLASS_TYPEDEF:
		{
			return IsCompleteType(psType->psSubType);		// PRQA S 3670
		}
		case TYPECLASS_UNION:
		case TYPECLASS_STRUCT:
		{
			return psType->bCompletedStructure;
		}
		case TYPECLASS_ENUM:
		{
			return (IMG_BOOL)(psType->uEnumeratorCount != 0);
		}
		case TYPECLASS_FIXEDARRAY:
		{
			return IMG_TRUE;
		}
		case TYPECLASS_VARIABLEARRAY:
		{
			return IMG_TRUE;
		}
		case TYPECLASS_FUNCTION:
		{
			return IMG_TRUE;
		}
		default:
		{
			IMG_ABORT();
		}
	}
}

static IMG_UINT32 GetTypeSize(PCTTYPE psType)
{
	switch (psType->eTypeClass)
	{
		case TYPECLASS_BASE:
		{
			assert(psType->eAtomicType != ATOMICTYPE_VOID && psType->eAtomicType != ATOMICTYPE_UNKNOWN);
			assert(psType->eAtomicType < ATOMICTYPE_COUNT);
			return g_pdwAtomicTypesSize[(int)(psType->eAtomicType)];
		}
		case TYPECLASS_POINTER:
		{
			return g_uPointerSize;
		}
		case TYPECLASS_TYPEDEF:
		{
			return GetTypeSize(psType->psSubType);		// PRQA S 3670
		}
		case TYPECLASS_UNION:
		case TYPECLASS_STRUCT:
		{
			return psType->uStructSize;
		}
		case TYPECLASS_ENUM:
		{
			return GetTypeSize(psType->psSubType);		// PRQA S 3670
		}
		case TYPECLASS_FIXEDARRAY:
		{
			return GetTypeSize(psType->psSubType) * psType->uArraySize;		// PRQA S 3670
		}
		case TYPECLASS_VARIABLEARRAY:
		{
			return 0;
		}
		default:
		{
			IMG_ABORT();
		}
	}
}

static PCTTYPE ResolveTypedef(PCTTYPE psType)
{
	while (psType->eTypeClass == TYPECLASS_TYPEDEF)
	{
		psType = psType->psSubType;
	}
	return psType;
}

static IMG_BOOL EquivalentTypes(PCTTYPE psType1, PCTTYPE psType2)
{
	psType1 = ResolveTypedef(psType1);
	psType2 = ResolveTypedef(psType2);

	if (psType1->eTypeClass != psType2->eTypeClass)
	{
		return IMG_FALSE;
	}

	switch (psType1->eTypeClass)
	{
		case TYPECLASS_BASE:
		{
			if (psType1->eAtomicType != psType2->eAtomicType)
			{
				return IMG_FALSE;
			}
			return IMG_TRUE;
		}
		case TYPECLASS_POINTER:
		{
			if (psType1->bVolatile != psType2->bVolatile ||
				psType1->bRestrict != psType2->bRestrict ||
				psType1->bConst != psType2->bConst ||
				!EquivalentTypes(psType1->psSubType, psType2->psSubType))
			{
				return IMG_FALSE;
			}
			return IMG_TRUE;
		}
		case TYPECLASS_UNION:
		case TYPECLASS_STRUCT:
		case TYPECLASS_ENUM:
		{
			if (psType1 != psType2)
			{
				return IMG_FALSE;
			}
			return IMG_TRUE;
		}
		case TYPECLASS_FIXEDARRAY:
		{
			if (psType1->uArraySize != psType2->uArraySize)
			{
				return IMG_FALSE;
			}
			return EquivalentTypes(psType1->psSubType, psType2->psSubType);
		}
		case TYPECLASS_VARIABLEARRAY:
		{
			return EquivalentTypes(psType1->psSubType, psType2->psSubType);
		}
		default: IMG_ABORT();
	}
}

static IMG_UINT32 GetTypeAlign(PCTTYPE psType)
{
	switch (psType->eTypeClass)
	{
		case TYPECLASS_BASE:
		{
			return GetTypeSize(psType);
		}
		case TYPECLASS_POINTER:
		{
			return g_uPointerSize;
		}
		case TYPECLASS_TYPEDEF:
		{
			return GetTypeAlign(psType->psSubType);		// PRQA S 3670
		}
		case TYPECLASS_UNION:
		case TYPECLASS_STRUCT:
		{
			return psType->uStructAlign;
		}
		case TYPECLASS_ENUM:
		{
			return GetTypeAlign(psType->psSubType);		// PRQA S 3670
		}
		case TYPECLASS_FIXEDARRAY:
		case TYPECLASS_VARIABLEARRAY:
		{
			return GetTypeAlign(psType->psSubType);		// PRQA S 3670
		}
		default:
		{
			IMG_ABORT();
		}
	}
}

static PCTTYPE MakeCTType(IMG_VOID)
{
	PCTTYPE	psType;

	psType = UseAsm_Malloc(sizeof(CTTYPE));
	psType->uTypeNum = g_uTypeCount++;
	psType->eTypeClass = TYPECLASS_UNKNOWN;
	psType->eAtomicType = ATOMICTYPE_UNKNOWN;
	psType->psSubType = NULL;
	psType->uEnumeratorCount = 0;
	psType->psEnumerators = NULL;
	psType->uStructMemberCount = 0;
	psType->psStructMembers = NULL;
	psType->psNext = NULL;
	psType->bConst = IMG_FALSE;
	psType->bRestrict = IMG_FALSE;
	psType->bVolatile = IMG_FALSE;
	psType->pszName = NULL;
	psType->bAnonymousType = IMG_FALSE;
	psType->bStructVariableLength = IMG_FALSE;
	psType->bCompletedStructure = IMG_FALSE;
	psType->uStructAlign = 0;

	return psType;
}

static IMG_VOID AddEnumeratorToHashTable(PCTTYPE	psType,
										 IMG_UINT32	uEnum)
/*****************************************************************************
 FUNCTION	: AddEnumeratorToHashTable

 PURPOSE	: Insert a newly declared enumeration member into the hash table.

 PARAMETERS	: psType			- Type of the enumeration.
			  uEnum				- Index of the enumeration member.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PENUMERATOR	psEnum;
	IMG_UINT32	uHash;

	psEnum = UseAsm_Malloc(sizeof(*psEnum));
	psEnum->psType = psType;
	psEnum->uIndex = uEnum;

	/*
		Compute the hash value of the enumeration member.
	*/
	uHash = StringToHash(psType->psEnumerators[uEnum].pszName) % ENUMERATOR_HASH_TABLE_SIZE;

	/*
		Insert it into the hash table.
	*/
	psEnum->psHashNext = g_sEnumeratorHashTable.asHead[uHash];
	g_sEnumeratorHashTable.asHead[uHash] = psEnum;
}

static IMG_BOOL LookupEnumerator(IMG_PCHAR pszName, PCTTYPE* ppsType, IMG_PUINT32 puEnumerator)
{
	IMG_UINT32	uHash;
	IMG_BOOL	bFound;
	PENUMERATOR	psHashEnum;

	uHash = StringToHash(pszName) % ENUMERATOR_HASH_TABLE_SIZE;

	bFound = IMG_FALSE;
	for (psHashEnum = g_sEnumeratorHashTable.asHead[uHash]; psHashEnum != NULL; psHashEnum = psHashEnum->psHashNext)
	{
		if (strcmp(psHashEnum->psType->psEnumerators[psHashEnum->uIndex].pszName, pszName) == 0)
		{
			bFound = IMG_TRUE;
			*ppsType = psHashEnum->psType;
			*puEnumerator = psHashEnum->uIndex;
			break;
		}
	}
	return bFound;
}

static IMG_VOID AddToGlobalTypeList(PCTTYPE	psType)
{
	if (g_psTypeListHead == NULL)
	{
		g_psTypeListHead = g_psTypeListTail = psType;
		return;
	}

	g_psTypeListTail->psNext = psType;
	g_psTypeListTail = psType;

	if (!psType->bAnonymousType)
	{
		IMG_UINT32				uHash;
		PTYPECLASS_HASH_TABLE	psTable;

		uHash = StringToHash(psType->pszName) % TYPE_HASH_TABLE_SIZE;

		assert(psType->eTypeClass < TYPECLASS_COUNT);
		psTable = &g_asTypeClassHashTable[psType->eTypeClass];

		psType->psHashNext = psTable->asHead[uHash];
		psTable->asHead[uHash] = psType;
	}
}

static PCTTYPE FindType(enum TYPECLASS eTypeClass, IMG_PCHAR pszName)
{
	IMG_UINT32				uHash;
	PTYPECLASS_HASH_TABLE	psTable;
	PCTTYPE					psHashType;

	uHash = StringToHash(pszName) % TYPE_HASH_TABLE_SIZE;

	assert(eTypeClass < TYPECLASS_COUNT);
	psTable = &g_asTypeClassHashTable[eTypeClass];

	for (psHashType = psTable->asHead[uHash]; psHashType != NULL; psHashType = psHashType->psHashNext)
	{
		assert(psHashType->eTypeClass == eTypeClass);
		assert(!psHashType->bAnonymousType);
		if (strcmp(psHashType->pszName, pszName) == 0)
		{
			break;
		}
	}

	return psHashType;
}

static IMG_BOOL IsConstIntegerExpression(YZEXPRESSION* psExpression, IMG_PINT32 piValue)
{
	switch (psExpression->uOperator)
	{
		case YZEXPRESSION_IDENTIFIER:
		{
			PCTTYPE psEnumType;
			IMG_UINT32 uEnumerator;
		
			if (LookupEnumerator(psExpression->pszString, &psEnumType, &uEnumerator))
			{
				*piValue = psEnumType->psEnumerators[uEnumerator].iValue;
				return IMG_TRUE;
			}

			return IMG_FALSE;
		}
		case YZEXPRESSION_CAST:
		{
			return IsConstIntegerExpression(psExpression->psSubExpr1, piValue);
		}
		case YZEXPRESSION_INTEGER_NUMBER:	
		{
			*piValue = (IMG_INT32)psExpression->uNumber;
			return IMG_TRUE;
		}
		case YZEXPRESSION_CONDITIONAL:
		{
			IMG_INT32	iGuardValue;
			if (!IsConstIntegerExpression(psExpression->psSubExpr1, &iGuardValue))
			{
				return IMG_FALSE;
			}
			if (iGuardValue)
			{
				return IsConstIntegerExpression(psExpression->psSubExpr2, piValue);
			}
			else
			{
				return IsConstIntegerExpression(psExpression->psSubExpr3, piValue);
			}
		}
		case YZEXPRESSION_POSITIVE:
		case YZEXPRESSION_NEGATE:
		case YZEXPRESSION_BITWISENOT:
		{
			IMG_INT32 iValue;
			if (!IsConstIntegerExpression(psExpression->psSubExpr1, &iValue))	// PRQA S 3670
			{
				return IMG_FALSE;
			}
			switch (psExpression->uOperator)
			{
				case YZEXPRESSION_POSITIVE:			*piValue = iValue; break;
				case YZEXPRESSION_NEGATE:			*piValue = -iValue; break;
				case YZEXPRESSION_BITWISENOT:		*piValue = (IMG_INT32)~(IMG_UINT32)iValue; break;
			}
			return IMG_TRUE;
		}
		case YZEXPRESSION_ADD:
		case YZEXPRESSION_SUB:
		case YZEXPRESSION_SHIFTLEFT:
		case YZEXPRESSION_SHIFTRIGHT:
		case YZEXPRESSION_BITWISEAND:
		case YZEXPRESSION_BITWISEOR:
		case YZEXPRESSION_BITWISEXOR:
		case YZEXPRESSION_MULTIPLY:
		case YZEXPRESSION_DIVIDE:
		case YZEXPRESSION_MODULUS:
		case YZEXPRESSION_LT:
		case YZEXPRESSION_GT:
		case YZEXPRESSION_LTE:
		case YZEXPRESSION_GTE:
		case YZEXPRESSION_EQ:
		case YZEXPRESSION_NEQ:
		{
			IMG_INT32 iValue1, iValue2;
			if (!IsConstIntegerExpression(psExpression->psSubExpr1, &iValue1) ||		// PRQA S 3670 2
				!IsConstIntegerExpression(psExpression->psSubExpr2, &iValue2))
			{
				return IMG_FALSE;
			}
			switch (psExpression->uOperator)
			{
				case YZEXPRESSION_ADD:				*piValue = iValue1 + iValue2; break;
				case YZEXPRESSION_SUB:				*piValue = iValue1 - iValue2; break;
				case YZEXPRESSION_SHIFTLEFT:		*piValue = iValue1 << iValue2; break;
				case YZEXPRESSION_SHIFTRIGHT:		*piValue = iValue1 >> iValue2; break;
				case YZEXPRESSION_BITWISEAND:		*piValue = (IMG_INT32)((IMG_UINT32)iValue1 & (IMG_UINT32)iValue2); break;
				case YZEXPRESSION_BITWISEOR:		*piValue = (IMG_INT32)((IMG_UINT32)iValue1 | (IMG_UINT32)iValue2); break;
				case YZEXPRESSION_BITWISEXOR:		*piValue = (IMG_INT32)((IMG_UINT32)iValue1 ^ (IMG_UINT32)iValue2); break;
				case YZEXPRESSION_MULTIPLY:			*piValue = iValue1 * iValue2; break;
				case YZEXPRESSION_DIVIDE:			*piValue = iValue1 / iValue2; break;
				case YZEXPRESSION_MODULUS:			*piValue = iValue1 % iValue2; break;
				case YZEXPRESSION_LT:				*piValue = iValue1 < iValue2; break;
				case YZEXPRESSION_GT:				*piValue = iValue1 > iValue2; break;
				case YZEXPRESSION_LTE:				*piValue = iValue1 <= iValue2; break;
				case YZEXPRESSION_GTE:				*piValue = iValue1 >= iValue2; break;
				case YZEXPRESSION_EQ:				*piValue = iValue1 == iValue2; break;
				case YZEXPRESSION_NEQ:				*piValue = iValue1 != iValue2; break;
			}
			return IMG_TRUE;
		}
		case YZEXPRESSION_SIZEOFTYPE:
		{
			PYZTYPENAME	psTypename = psExpression->psSizeofTypeName;
			PCTTYPE		psType;

			psType = GetType(psTypename->psDeclSpecifierList);
			psType = GetDeclaratorType(psTypename->psDeclarator, psType);

			if (IsVariableLengthArray(psType))
			{
				CTreeError(psExpression->sLocation, "Applying sizeof to a variable length array");
				return IMG_FALSE;
			}

			*piValue = GetTypeSize(psType);
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static IMG_VOID CTreeError(YYLTYPE sLocation, IMG_PCHAR pszError, ...)
{
	va_list ap;

	va_start(ap, pszError);
	fprintf(stderr, "%s(%u): error: ", sLocation.pszFilename, sLocation.uLine);
	vfprintf(stderr, pszError, ap);
	fprintf(stderr, "\n");
	va_end(ap);

	g_bCCodeError = IMG_TRUE;
}

static IMG_PCHAR GetDeclaratorName(YZDECLARATOR* psDeclarator)
{
	switch (psDeclarator->uType)
	{
		case YZDECLARATOR_IDENTIFIER:
		{
			return strdup(psDeclarator->pszIdentifier);
		}
		default:
		{
			return GetDeclaratorName(psDeclarator->psSubDeclarator);	// PRQA S 3670
		}
	}
}

static IMG_VOID GetTypeQualifiers(PCTTYPE psType, YZDECLSPECIFIER* psTypeQualifierList)
{
	YZDECLSPECIFIER*	psTypeQualifier;

	for (psTypeQualifier = psTypeQualifierList; psTypeQualifier != NULL; psTypeQualifier = psTypeQualifier->psNext)
	{
		switch (psTypeQualifier->uType)
		{
			case YZDECLSPECIFIER_CONST:
			{
				psType->bConst = IMG_TRUE;
				break;
			}
			case YZDECLSPECIFIER_RESTRICT:
			{
				psType->bRestrict = IMG_TRUE;
				break;
			}
			case YZDECLSPECIFIER_VOLATILE:
			{
				psType->bVolatile = IMG_TRUE;
				break;
			}
			case YZDECLSPECIFIER_MSVC_PTR32:
			case YZDECLSPECIFIER_MSVC_PTR64:
			case YZDECLSPECIFIER_CDECL:
			case YZDECLSPECIFIER_STDCALL:
			{
				/*
					Ignore these: Microsoft extensions which don't affect us.
				*/
				break;
			}
			default: IMG_ABORT();
		}
	}
}

static PCTTYPE MakeArrayType(YZDECLARATOR* psDeclarator, PCTTYPE psAtomicType)
{
	PCTTYPE		psType;

	psType = MakeCTType();
	psType->sLocation = psDeclarator->sLocation;
	psType->bAnonymousType = IMG_TRUE;
	psType->psSubType = psAtomicType;

	if (psDeclarator->uType == YZDECLARATOR_VARIABLELENGTHARRAY)
	{
		psType->eTypeClass = TYPECLASS_VARIABLEARRAY;
	}
	else
	{
		IMG_INT32	iArraySize;

		assert(psDeclarator->uType == YZDECLARATOR_FIXEDLENGTHARRAY);

		psType->eTypeClass = TYPECLASS_FIXEDARRAY;
		if (!IsConstIntegerExpression(psDeclarator->psArraySize, &iArraySize))
		{
			CTreeError(psDeclarator->psArraySize->sLocation, "Array size is non-constant");
		}
		if (iArraySize < 0)
		{
			CTreeError(psDeclarator->psArraySize->sLocation, "Array size is negative");
		}
		psType->uArraySize = (IMG_UINT32)iArraySize;	// PRQA S 291
	}
	AddToGlobalTypeList(psType);

	return psType;
}

static PCTTYPE GetDeclaratorType(YZDECLARATOR* psDeclarator, PCTTYPE psAtomicType)
{
	PCTTYPE psType;

	if (psDeclarator == NULL)
	{
		return psAtomicType;
	}

	if (psDeclarator->psPointer != NULL)
	{
		PCTTYPE psPtrType;
		YZPOINTER* psPointer;

		/*
			The pointer list is in left-to-right order - preceed down the list and for each
			entry make a new type "pointer to base_type" then make that the new base type.
		*/
		for (psPointer = psDeclarator->psPointer; psPointer != NULL; psPointer = psPointer->psNext)
		{
			psPtrType = MakeCTType();
			psPtrType->sLocation = psDeclarator->psPointer->sLocation;
			psPtrType->eTypeClass = TYPECLASS_POINTER;
			psPtrType->psSubType = psAtomicType;
			psPtrType->bAnonymousType = IMG_TRUE;
			GetTypeQualifiers(psPtrType, psPointer->psQualifierList);
			AddToGlobalTypeList(psPtrType);

			psAtomicType = psPtrType;
		}
	}

	switch (psDeclarator->uType)
	{
		case YZDECLARATOR_ABSTRACTPOINTER:
		{
			return GetDeclaratorType(psDeclarator->psSubDeclarator, psAtomicType);
		}
		case YZDECLARATOR_FUNCTION:
		{
			/*
				Make a new type representing a function with the existing base type and
				its return type.
			*/
			psType = MakeCTType();
			psType->sLocation = psDeclarator->sLocation;
			psType->eTypeClass = TYPECLASS_FUNCTION;
			psType->bAnonymousType = IMG_TRUE;
			psType->psSubType = psAtomicType;
			AddToGlobalTypeList(psType);

			return GetDeclaratorType(psDeclarator->psSubDeclarator, psType);	// PRQA S 3670
		}
		case YZDECLARATOR_VARIABLELENGTHARRAY:
		case YZDECLARATOR_FIXEDLENGTHARRAY:
		{
			PCTTYPE		psArrayType;

			if (psAtomicType->eTypeClass == TYPECLASS_VARIABLEARRAY)
			{
				CTreeError(psDeclarator->psArraySize->sLocation, 
					"Only the leftmost dimension of an array can be variable length");
			}

			/*
				Make a type representing the array.
			*/
			psArrayType = MakeArrayType(psDeclarator, psAtomicType);
	
			/*
				Continue down the list using the array type as the new base type.
			*/
			return GetDeclaratorType(psDeclarator->psSubDeclarator, psArrayType);	// PRQA S 3670
		}
		case YZDECLARATOR_IDENTIFIER:
		{
			/*
				Reached the end of the list so use the created type.
			*/
			return psAtomicType;
		}
		case YZDECLARATOR_BRACKETED:
		{
			return GetDeclaratorType(psDeclarator->psSubDeclarator, psAtomicType);	// PRQA S 3670
		}
		default: IMG_ABORT();
	}
}

static IMG_VOID ParseEnum(YZENUMERATOR*				psEnumeratorList,
						  PCTTYPE					psEnumeratorType)
{
	YZENUMERATOR*	psEnumerator;
	IMG_UINT32		uEnum;
	IMG_INT32		iNextValue = 0;

	psEnumeratorType->uEnumeratorCount = 0;
	for (psEnumerator = psEnumeratorList; psEnumerator != NULL; psEnumerator = psEnumerator->psNext)
	{
		psEnumeratorType->uEnumeratorCount++;
	}

	psEnumeratorType->psEnumerators = calloc(sizeof(CTENUMMEMBER), psEnumeratorType->uEnumeratorCount);

	for (psEnumerator = psEnumeratorList, uEnum = 0; psEnumerator != NULL; psEnumerator = psEnumerator->psNext, uEnum++)
	{
		psEnumeratorType->psEnumerators[uEnum].pszName = psEnumerator->pszIdentifier;
		if (psEnumerator->psValue != NULL)
		{
			if (!IsConstIntegerExpression(psEnumerator->psValue, &iNextValue))
			{
				CTreeError(psEnumerator->psValue->sLocation, "Enumerator initialization isn't constant");
			}
		}
		psEnumeratorType->psEnumerators[uEnum].iValue = iNextValue;
		iNextValue++;

		AddEnumeratorToHashTable(psEnumeratorType, uEnum);
	}

	psEnumeratorType->psSubType = LookupAtomicType(ATOMICTYPE_UNSIGNEDINT);
}

static IMG_UINT32 RoundUp(IMG_UINT32 uI, IMG_UINT32 uRound)
/*****************************************************************************
 FUNCTION	: RoundUp

 PURPOSE	: Rounds one integer up to the next multiple of another.

 PARAMETERS	: uI		- The value to round.
			  uRound	- The multiple to round up to.

 RETURNS	: The rounded value.
*****************************************************************************/
{
	if ((uI % uRound) != 0)
	{
		return uI + uRound - (uI % uRound);
	}
	else
	{
		return uI;
	}
}

static IMG_VOID ResolveStruct(PCTTYPE		psType,
							  IMG_UINT32	uAlignmentOverride)
/*****************************************************************************
 FUNCTION	: ResolveStruct

 PURPOSE	: Calculate the size and alignment of a structure type.

 PARAMETERS	: psType				- Structure to resolve.
			  uAlignmentOverride	- Any alignment override (set with
									"pragma pack") that was active when the
									structure was declared.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uMember;
	IMG_UINT32	uCurrentOffset;

	/*
		Set the default structure alignmnet.
	*/
	psType->uStructAlign = 1;

	uCurrentOffset = 0;
	for (uMember = 0; uMember < psType->uStructMemberCount; uMember++)
	{
		IMG_UINT32		uAlign, uSize;
		PCTTYPE			psStructMemberType = psType->psStructMembers[uMember].psType;

		if (IsVariableLengthArray(psStructMemberType) && uMember < (psType->uStructMemberCount - 1))
		{
			CTreeError(psType->sLocation, "A variable length array must be the last member in the structure");
		}
		else
		{
			psType->bStructVariableLength = IMG_TRUE;
		}

		if (!IsCompleteType(psStructMemberType))
		{
			CTreeError(psType->sLocation, "Structure contains a member with an incomplete type");
		}

		/*
			Get the size and alignment of the type of this structure member.
		*/
		uAlign = GetTypeAlign(psStructMemberType);
		uSize = GetTypeSize(psStructMemberType);

		/*
			Override the alignment with any currently set "pragma pack" value.
		*/
		uAlign = min(uAlign, uAlignmentOverride);

		/*
			Round up the offset of this structure member to the next multipler of
			the alignment.
		*/
		uCurrentOffset = RoundUp(uCurrentOffset, uAlign);
		
		/*
			Set the alignment of the structure to the maximum of the alignment of
			the structure members.
		*/
		psType->uStructAlign = max(psType->uStructAlign, uAlign);

		/*
			Store the offset of this structure member from the start of the structure.
		*/
		psType->psStructMembers[uMember].uByteOffset = uCurrentOffset;

		uCurrentOffset += uSize;
	}

	/*
		Round the size of the structure up to the next multiple of the structure
		alignment.
	*/
	psType->uStructSize = RoundUp(uCurrentOffset, psType->uStructAlign);
}

static IMG_VOID ResolveUnion(PCTTYPE		psType)
/*****************************************************************************
 FUNCTION	: ResolveUnion

 PURPOSE	: Calculate the size and alignment of a union type.

 PARAMETERS	: psType				- Union to resolve.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32				uMember;
	
	psType->uStructSize = 0;
	psType->uStructAlign = 0;

	for (uMember = 0; uMember < psType->uStructMemberCount; uMember++)
	{
		IMG_UINT32		uAlign, uSize;

		if (!IsCompleteType(psType->psStructMembers[uMember].psType))
		{
			CTreeError(psType->sLocation, "Union contains a member with an incomplete type");
		}

		uAlign = GetTypeAlign(psType->psStructMembers[uMember].psType);
		uSize = GetTypeSize(psType->psStructMembers[uMember].psType);

		psType->uStructSize = max(psType->uStructSize, uSize);
		psType->uStructAlign = max(psType->uStructAlign, uAlign);
	}

	/*
		Round the size of the union up to the next multiple of the uion
		alignment.
	*/
	psType->uStructSize = RoundUp(psType->uStructSize, psType->uStructAlign);
}

static CTSTRUCTMEMBER* FindStructMember(IMG_UINT32 uStructMemberCount, CTSTRUCTMEMBER* psStructMembers, IMG_PCHAR pszName)
/*****************************************************************************
 FUNCTION	: FindStructMember

 PURPOSE	: Look for a member of a structure or union with the specified name.

 PARAMETERS	: uStructMemberCount		- Number of members of the structure.
			  psStructMembers			- Array with an entry for each member of
										the structure.
			  pszName					- Name of the member to find.

 RETURNS	: The structure describing the member if it was found or NULL.
*****************************************************************************/
{
	IMG_UINT32	uMember;

	for (uMember = 0; uMember < uStructMemberCount; uMember++)
	{
		if (psStructMembers[uMember].pszName == NULL)
		{
			CTSTRUCTMEMBER*	psMember;

			/*
				For an anonymous structure member check for a match in the subtype.
			*/
			if (IsStruct(psStructMembers[uMember].psType) || IsUnion(psStructMembers[uMember].psType))
			{
				//PRQA S 3670 3
				psMember = FindStructMember(psStructMembers[uMember].psType->uStructMemberCount,
											psStructMembers[uMember].psType->psStructMembers,
											pszName);
				if (psMember != NULL)
				{
					return psMember;
				}
			}
		}
		else
		{
			if (strcmp(psStructMembers[uMember].pszName, pszName) == 0)
			{
				return &psStructMembers[uMember];
			}
		}
	}

	return NULL;
}

static IMG_VOID CheckNameCollision(YYLTYPE sLocation, IMG_UINT32 uStructMemberCount, CTSTRUCTMEMBER* psMembers, PCTTYPE psType)
/*****************************************************************************
 FUNCTION	: CheckNameCollision

 PURPOSE	: Check if any members of a structure which is the type of an
			  anonymous member of another structure have the same names
			  as members of the containing structure.

 PARAMETERS	: sLocation			- Location of the anonymous structure.
			  uStructMemberCount	
								- Count of members of the anonymous structure.
			  psMembers			- Pointers to the members of the anonymous
								structure.
			  psType			- Type of the containing structure.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uMember;

	for (uMember = 0; uMember < psType->uStructMemberCount; uMember++)
	{
		if (psType->psStructMembers[uMember].pszName == NULL)
		{
			/*
				Check for collisions with the members of another anonymous
				structure or union.
			*/
			if (IsStruct(psType->psStructMembers[uMember].psType) ||
				IsUnion(psType->psStructMembers[uMember].psType))
			{
				CheckNameCollision(sLocation, uStructMemberCount, psMembers, psType->psStructMembers[uMember].psType); //PRQA S 3670
			}
		}
		else
		{
			/*
				Check if this member of the containing structure has the same name as a member of 
				the anonymous structure.
			*/
			if (FindStructMember(uStructMemberCount, psMembers, psType->psStructMembers[uMember].pszName) != NULL)
			{
				CTreeError(sLocation, "Two struct/union members with the same name");
			}
		}
	}
}

static IMG_BOOL RedeclFixup(YZDECLSPECIFIER* psDeclSpecifierList, IMG_PCHAR* ppszName)
{
	YZDECLSPECIFIER*	psDeclSpecifier;
	YZDECLSPECIFIER*	psPrevDeclSpecifier = NULL;

	if (psDeclSpecifierList == NULL || psDeclSpecifierList->psNext == NULL)
	{
		return IMG_FALSE;
	}

	for (psDeclSpecifier = psDeclSpecifierList; ; psDeclSpecifier = psDeclSpecifier->psNext)
	{
		if (psDeclSpecifier->psNext == NULL)
		{
			break;
		}
		psPrevDeclSpecifier = psDeclSpecifier;
	}

	if (psDeclSpecifier->uType == YZDECLSPECIFIER_TYPEDEF_NAME)
	{
		*ppszName = psDeclSpecifier->pszIdentifier;
		psPrevDeclSpecifier->psNext = NULL;
		UseAsm_Free(psDeclSpecifier);
		return IMG_TRUE;
	}

	return IMG_FALSE;
}


static IMG_VOID ParseStructOrUnion(YZSTRUCTDECLARATION*		psStructDeclarationList, 
								   IMG_PUINT32				puStructMemberCount, 
								   CTSTRUCTMEMBER**			ppsStructMembers)
{
	YZSTRUCTDECLARATION*	psStructDeclaration;
	IMG_UINT32				uMember;

	/*
		Count the number of structure members.
	*/
	(*puStructMemberCount) = 0;
	for (psStructDeclaration = psStructDeclarationList; psStructDeclaration != NULL; psStructDeclaration = psStructDeclaration->psNext)
	{
		YZSTRUCTDECLARATOR*		psStructDeclarator;
		if (psStructDeclaration->psDeclaratorList == NULL)
		{
			(*puStructMemberCount)++;
		}
		else
		{
			for (psStructDeclarator = psStructDeclaration->psDeclaratorList; psStructDeclarator != NULL; psStructDeclarator = psStructDeclarator->psNext)
			{
				(*puStructMemberCount)++;
			}
		}
	}

	(*ppsStructMembers) = UseAsm_Malloc(sizeof(CTSTRUCTMEMBER) * (*puStructMemberCount));

	for (psStructDeclaration = psStructDeclarationList, uMember = 0; psStructDeclaration != NULL; psStructDeclaration = psStructDeclaration->psNext)
	{
		YZSTRUCTDECLARATOR*		psStructDeclarator;
		PCTTYPE					psAtomicType;
		enum STORAGE			eStorageClass;

		eStorageClass = GetStorageClass(psStructDeclaration->psSpecifierList);
		if (eStorageClass != STORAGE_UNKNOWN)
		{
			CTreeError(psStructDeclaration->sLocation, "Storage classes aren't valid for structure or union members");
		}

		if (psStructDeclaration->psDeclaratorList == NULL)
		{
			IMG_PCHAR	pszName;

			/*
				If the structure member 
			*/
			if (RedeclFixup(psStructDeclaration->psSpecifierList, &pszName))
			{
				psStructDeclaration->psDeclaratorList = CTreeMakeStructDeclarator(psStructDeclaration->sLocation);
				psStructDeclaration->psDeclaratorList->bBitField = IMG_FALSE;
				psStructDeclaration->psDeclaratorList->psBitFieldSize = NULL;
				psStructDeclaration->psDeclaratorList->psDeclarator = CTreeMakeDeclarator(psStructDeclaration->sLocation);
				psStructDeclaration->psDeclaratorList->psDeclarator->uType = YZDECLARATOR_IDENTIFIER;
				psStructDeclaration->psDeclaratorList->psDeclarator->pszIdentifier = pszName;
			}
		}

		psAtomicType = GetType(psStructDeclaration->psSpecifierList);

		if (psStructDeclaration->psDeclaratorList == NULL)
		{
			(*ppsStructMembers)[uMember].pszName = NULL;
			(*ppsStructMembers)[uMember].psType = psAtomicType;
			(*ppsStructMembers)[uMember].uBitFieldSize = USE_UNDEF;
			if (!IsUnion(psAtomicType) && !IsStruct(psAtomicType))
			{
				CTreeError(psStructDeclaration->sLocation, "Only structure members which are structures or union can be anonymous");
			}
			else
			{
				CheckNameCollision(psStructDeclaration->sLocation, uMember, *ppsStructMembers, psAtomicType);
			}
			uMember++;
		}
		else
		{
			for (psStructDeclarator = psStructDeclaration->psDeclaratorList; psStructDeclarator != NULL; psStructDeclarator = psStructDeclarator->psNext, uMember++)
			{
				if (psStructDeclarator->psDeclarator == NULL)
				{
					(*ppsStructMembers)[uMember].pszName = NULL;
				}
				else
				{
					(*ppsStructMembers)[uMember].pszName = GetDeclaratorName(psStructDeclarator->psDeclarator);
				}
				if (psStructDeclarator->psDeclarator == NULL)
				{
					(*ppsStructMembers)[uMember].psType = psAtomicType;
				}
				else
				{
					(*ppsStructMembers)[uMember].psType = GetDeclaratorType(psStructDeclarator->psDeclarator, psAtomicType);
				}
				if ((*ppsStructMembers)[uMember].pszName == NULL && !psStructDeclarator->bBitField)
				{
					CTreeError(psStructDeclarator->sLocation, "Only structure members which are bit fields can be anonymous");
				}
				if (psStructDeclarator->bBitField)
				{
					IMG_INT32	iBitFieldSize;
					if (!IsConstIntegerExpression(psStructDeclarator->psBitFieldSize, &iBitFieldSize))
					{
						CTreeError(psStructDeclarator->psBitFieldSize->sLocation, "Bit field width is non-constant");
					}
					if (iBitFieldSize < 0)
					{
						CTreeError(psStructDeclarator->psBitFieldSize->sLocation, "Bit field width is negative");
					}
					(*ppsStructMembers)[uMember].uBitFieldSize = (IMG_UINT32)iBitFieldSize;	//PRQA S 291
				}
				else
				{
					(*ppsStructMembers)[uMember].uBitFieldSize = USE_UNDEF;
				}
	
				if ((*ppsStructMembers)[uMember].pszName != NULL)
				{
					if (FindStructMember(uMember, *ppsStructMembers, (*ppsStructMembers)[uMember].pszName) != NULL)
					{
						CTreeError(psStructDeclarator->sLocation, "Two structure members with the same name");
					}
				}
			}
		}
	}
}

static const enum ATOMICTYPE g_aeAtomicTypeToSignedVariant[ATOMICTYPE_COUNT] = 
{
	ATOMICTYPE_UNKNOWN,				/* ATOMICTYPE_UNKNOWN */
	ATOMICTYPE_UNKNOWN,				/* ATOMICTYPE_VOID */
	ATOMICTYPE_SIGNEDCHAR,			/* ATOMICTYPE_UNSIGNEDCHAR */
	ATOMICTYPE_SIGNEDCHAR,			/* ATOMICTYPE_SIGNEDCHAR */
	ATOMICTYPE_SIGNEDSHORT,			/* ATOMICTYPE_UNSIGNEDSHORT */
	ATOMICTYPE_SIGNEDSHORT,			/* ATOMICTYPE_SIGNEDSHORT */
	ATOMICTYPE_SIGNEDINT,			/* ATOMICTYPE_UNSIGNEDINT */
	ATOMICTYPE_SIGNEDINT,			/* ATOMICTYPE_SIGNEDINT */
	ATOMICTYPE_SIGNEDLONG,			/* ATOMICTYPE_UNSIGNEDLONG */
	ATOMICTYPE_SIGNEDLONG,			/* ATOMICTYPE_SIGNEDLONG */
	ATOMICTYPE_UNKNOWN,				/* ATOMICTYPE_FLOAT */
	ATOMICTYPE_UNKNOWN,				/* ATOMICTYPE_DOUBLE */
	ATOMICTYPE_UNKNOWN,				/* ATOMICTYPE_LONGDOUBLE */
	ATOMICTYPE_SIGNEDLONGLONG,		/* ATOMICTYPE_UNSIGNEDLONGLONG */
	ATOMICTYPE_SIGNEDLONGLONG,		/* ATOMICTYPE_SIGNEDLONGLONG */
	ATOMICTYPE_UNKNOWN,				/* ATOMICTYPE_GCC_BUILTIN_VA_LIST */
	ATOMICTYPE_UNKNOWN,				/* ATOMICTYPE_BOOL */
};

static const enum ATOMICTYPE g_aeAtomicTypeToLongVariant[ATOMICTYPE_COUNT] = 
{
	ATOMICTYPE_UNKNOWN,				/* ATOMICTYPE_UNKNOWN */
	ATOMICTYPE_UNKNOWN,				/* ATOMICTYPE_VOID */
	ATOMICTYPE_UNKNOWN,				/* ATOMICTYPE_UNSIGNEDCHAR */
	ATOMICTYPE_UNKNOWN,				/* ATOMICTYPE_SIGNEDCHAR */
	ATOMICTYPE_UNKNOWN,				/* ATOMICTYPE_UNSIGNEDSHORT */
	ATOMICTYPE_UNKNOWN,				/* ATOMICTYPE_SIGNEDSHORT */
	ATOMICTYPE_UNSIGNEDLONG,		/* ATOMICTYPE_UNSIGNEDINT */
	ATOMICTYPE_SIGNEDLONG,			/* ATOMICTYPE_SIGNEDINT */
	ATOMICTYPE_UNSIGNEDLONGLONG,	/* ATOMICTYPE_UNSIGNEDLONG */
	ATOMICTYPE_SIGNEDLONGLONG,		/* ATOMICTYPE_SIGNEDLONG */
	ATOMICTYPE_UNKNOWN,				/* ATOMICTYPE_FLOAT */
	ATOMICTYPE_LONGDOUBLE,			/* ATOMICTYPE_DOUBLE */
	ATOMICTYPE_UNKNOWN,				/* ATOMICTYPE_LONGDOUBLE */
	ATOMICTYPE_UNKNOWN,				/* ATOMICTYPE_UNSIGNEDLONGLONG */
	ATOMICTYPE_UNKNOWN,				/* ATOMICTYPE_SIGNEDLONGLONG */
	ATOMICTYPE_UNKNOWN,				/* ATOMICTYPE_GCC_BUILTIN_VA_LIST */
	ATOMICTYPE_UNKNOWN,				/* ATOMICTYPE_BOOL */
};

static const enum ATOMICTYPE g_aeAtomicTypeToShortVariant[ATOMICTYPE_COUNT] = 
{
	ATOMICTYPE_UNKNOWN,			/* ATOMICTYPE_UNKNOWN */
	ATOMICTYPE_UNKNOWN,			/* ATOMICTYPE_VOID */
	ATOMICTYPE_UNKNOWN,			/* ATOMICTYPE_UNSIGNEDCHAR */
	ATOMICTYPE_UNKNOWN,			/* ATOMICTYPE_SIGNEDCHAR */
	ATOMICTYPE_UNKNOWN,			/* ATOMICTYPE_UNSIGNEDSHORT */
	ATOMICTYPE_UNKNOWN,			/* ATOMICTYPE_SIGNEDSHORT */
	ATOMICTYPE_UNSIGNEDSHORT,	/* ATOMICTYPE_UNSIGNEDINT */
	ATOMICTYPE_SIGNEDSHORT,		/* ATOMICTYPE_SIGNEDINT */
	ATOMICTYPE_UNKNOWN,			/* ATOMICTYPE_UNSIGNEDLONG */
	ATOMICTYPE_UNKNOWN,			/* ATOMICTYPE_SIGNEDLONG */
	ATOMICTYPE_UNKNOWN,			/* ATOMICTYPE_FLOAT */
	ATOMICTYPE_UNKNOWN,			/* ATOMICTYPE_DOUBLE */
	ATOMICTYPE_UNKNOWN,			/* ATOMICTYPE_LONGDOUBLE */
	ATOMICTYPE_UNKNOWN,			/* ATOMICTYPE_UNSIGNEDLONGLONG */
	ATOMICTYPE_UNKNOWN,			/* ATOMICTYPE_SIGNEDLONGLONG */
	ATOMICTYPE_UNKNOWN,			/* ATOMICTYPE_GCC_BUILTIN_VA_LIST */
	ATOMICTYPE_UNKNOWN,			/* BOOL */
};

static const IMG_BOOL g_abAtomicTypeIsInteger[ATOMICTYPE_COUNT] = 
{
	IMG_FALSE,			/* ATOMICTYPE_UNKNOWN */
	IMG_FALSE,			/* ATOMICTYPE_VOID */
	IMG_TRUE,			/* ATOMICTYPE_UNSIGNEDCHAR */
	IMG_TRUE,			/* ATOMICTYPE_SIGNEDCHAR */
	IMG_TRUE,			/* ATOMICTYPE_UNSIGNEDSHORT */
	IMG_TRUE,			/* ATOMICTYPE_SIGNEDSHORT */
	IMG_TRUE,			/* ATOMICTYPE_UNSIGNEDINT */
	IMG_TRUE,			/* ATOMICTYPE_SIGNEDINT */
	IMG_TRUE,			/* ATOMICTYPE_UNSIGNEDLONG */
	IMG_TRUE,			/* ATOMICTYPE_SIGNEDLONG */
	IMG_FALSE,			/* ATOMICTYPE_FLOAT */
	IMG_FALSE,			/* ATOMICTYPE_DOUBLE */
	IMG_FALSE,			/* ATOMICTYPE_LONGDOUBLE */
	IMG_TRUE,			/* ATOMICTYPE_UNSIGNEDLONGLONG */
	IMG_TRUE,			/* ATOMICTYPE_SIGNEDLONGLONG */
	IMG_FALSE,			/* ATOMICTYPE_GCC_BUILTIN_VA_LIST */
	IMG_FALSE,			/* ATOMICTYPE_BOOL */
};

typedef struct
{
	enum ATOMICTYPE	eAtomicType;
	IMG_UINT32		uSignedCount;
	IMG_UINT32		uUnsignedCount;
	IMG_UINT32		uLongCount;
	IMG_UINT32		uShortCount;
	IMG_BOOL		bMSVC_Warn64;
	IMG_BOOL		bMSVC_ExplicitType;
	IMG_BOOL		bNonAtomicType;
} ATOMICTYPE_INFO, *PATOMICTYPE_INFO;

static enum ATOMICTYPE DeclaratorToBaseType(YZDECLSPECIFIER* psDeclSpecifier, IMG_PBOOL pbMSVCExplicitType)
{
	switch (psDeclSpecifier->uType)
	{
		case YZDECLSPECIFIER_VOID:					*pbMSVCExplicitType = IMG_FALSE; return ATOMICTYPE_VOID;
		case YZDECLSPECIFIER_CHAR:					*pbMSVCExplicitType = IMG_FALSE; return ATOMICTYPE_UNSIGNEDCHAR;
		case YZDECLSPECIFIER_INT:					*pbMSVCExplicitType = IMG_FALSE; return ATOMICTYPE_UNSIGNEDINT;
		case YZDECLSPECIFIER_FLOAT:					*pbMSVCExplicitType = IMG_FALSE; return ATOMICTYPE_FLOAT;
		case YZDECLSPECIFIER_DOUBLE:				*pbMSVCExplicitType = IMG_FALSE; return ATOMICTYPE_DOUBLE;
		case YZDECLSPECIFIER_GCC_BUILTIN_VA_LIST:	*pbMSVCExplicitType = IMG_FALSE; return ATOMICTYPE_GCC_BUILTIN_VA_LIST;
		case YZDECLSPECIFIER_MSVC_INT8:				*pbMSVCExplicitType = IMG_TRUE; return ATOMICTYPE_UNSIGNEDCHAR;
		case YZDECLSPECIFIER_MSVC_INT16:			*pbMSVCExplicitType = IMG_TRUE; return ATOMICTYPE_UNSIGNEDSHORT;
		case YZDECLSPECIFIER_MSVC_INT32:			*pbMSVCExplicitType = IMG_TRUE; return ATOMICTYPE_UNSIGNEDINT;
		case YZDECLSPECIFIER_MSVC_INT64:			*pbMSVCExplicitType = IMG_TRUE; return ATOMICTYPE_UNSIGNEDLONGLONG;
		case YZDECLSPECIFIER_BOOL:					*pbMSVCExplicitType = IMG_FALSE; return ATOMICTYPE_BOOL;
		default: IMG_ABORT();
	}
}

static IMG_BOOL GetBaseType(YZDECLSPECIFIER* psDeclSpecifierList, PATOMICTYPE_INFO psInfo)
{
	YZDECLSPECIFIER*	psDeclSpecifier;

	psInfo->eAtomicType = ATOMICTYPE_UNKNOWN;
	psInfo->uSignedCount = 0;
	psInfo->uUnsignedCount = 0;
	psInfo->uLongCount = 0;
	psInfo->uShortCount = 0;
	psInfo->bMSVC_Warn64 = IMG_FALSE;
	psInfo->bMSVC_ExplicitType = IMG_FALSE;
	psInfo->bNonAtomicType = IMG_FALSE;

	for (psDeclSpecifier = psDeclSpecifierList; psDeclSpecifier != NULL; psDeclSpecifier = psDeclSpecifier->psNext)
	{
		switch (psDeclSpecifier->uType)
		{
			case YZDECLSPECIFIER_VOID:
			case YZDECLSPECIFIER_CHAR:
			case YZDECLSPECIFIER_INT:
			case YZDECLSPECIFIER_FLOAT:
			case YZDECLSPECIFIER_DOUBLE:
			case YZDECLSPECIFIER_MSVC_INT8:
			case YZDECLSPECIFIER_MSVC_INT16:
			case YZDECLSPECIFIER_MSVC_INT32:
			case YZDECLSPECIFIER_MSVC_INT64:
			case YZDECLSPECIFIER_GCC_BUILTIN_VA_LIST:
			case YZDECLSPECIFIER_BOOL:
			{
				enum ATOMICTYPE	eAtomicType;

				eAtomicType = DeclaratorToBaseType(psDeclSpecifier, &psInfo->bMSVC_ExplicitType);
				if (psInfo->eAtomicType != ATOMICTYPE_UNKNOWN || psInfo->bNonAtomicType)
				{
					CTreeError(psDeclSpecifier->sLocation, "Different base types");
					return IMG_FALSE;
				}

				psInfo->eAtomicType = eAtomicType;
				break;
			}

			case YZDECLSPECIFIER_SHORT:
			{
				psInfo->uShortCount++;
				break;
			}
			case YZDECLSPECIFIER_LONG:
			{
				psInfo->uLongCount++;
				break;
			}
			case YZDECLSPECIFIER_SIGNED:
			{
				psInfo->uSignedCount++;
				break;
			}
			case YZDECLSPECIFIER_UNSIGNED:
			{
				psInfo->uUnsignedCount++;
				break;
			}
			case YZDECLSPECIFIER_MSVC__W64:
			{
				psInfo->bMSVC_Warn64 = IMG_TRUE;
				break;
			}

			case YZDECLSPECIFIER_TYPEDEF_NAME:
			case YZDECLSPECIFIER_STRUCT:
			case YZDECLSPECIFIER_ENUM:
			{
				if (psInfo->eAtomicType != ATOMICTYPE_UNKNOWN)
				{
					CTreeError(psDeclSpecifier->sLocation, "Different base types");
				}
				psInfo->bNonAtomicType = IMG_TRUE;
				break;
			}
		}
	}

	return IMG_TRUE;
}

static IMG_BOOL GetAtomicType(YZDECLSPECIFIER* psDeclSpecifierList, enum ATOMICTYPE * peAtomicType)
{
	ATOMICTYPE_INFO		sInfo;
	IMG_UINT32			uLongIdx;

	/*
		Set the default return value.
	*/
	*peAtomicType = ATOMICTYPE_UNKNOWN;

	if (!GetBaseType(psDeclSpecifierList, &sInfo))
	{
		return IMG_FALSE;
	}

	if (sInfo.eAtomicType == ATOMICTYPE_UNKNOWN)
	{
		/*
			If there is no base type but a long, short, signed or unsigned modifier then
			make the base type int.
		*/
		if (
				sInfo.uLongCount > 0 || 
				sInfo.uShortCount > 0 || 
				sInfo.uSignedCount > 0 ||
				sInfo.uUnsignedCount > 0
			)
		{
			sInfo.eAtomicType = ATOMICTYPE_UNSIGNEDINT;
		}
	}

	/*
		Check for both 'signed' and 'unsigned' modifiers in the same declaration.
	*/
	if (sInfo.uSignedCount > 0 && sInfo.uUnsignedCount > 0)
	{
		CTreeError(psDeclSpecifierList->sLocation, "Both signed and unsigned specified in declaration");
		return IMG_FALSE;
	}

	/*
		Check for the signed or unsigned modifiers specified multiple times in the declaration.
	*/
	if (sInfo.uSignedCount > 1)
	{
		CTreeError(psDeclSpecifierList->sLocation, "Duplicate signed");
		return IMG_FALSE;
	}
	if (sInfo.uUnsignedCount > 1)
	{
		CTreeError(psDeclSpecifierList->sLocation, "Duplicate unsigned");
		return IMG_FALSE;
	}

	/*
		Check for using the signed or unsigned modifier with a non-integer type.
	*/
	if (	
			(
				sInfo.uSignedCount > 0 ||
				sInfo.uUnsignedCount > 0
			) &&
			(
				!g_abAtomicTypeIsInteger[sInfo.eAtomicType] ||
				sInfo.bNonAtomicType
		    )
	   )
	{
		if (sInfo.uSignedCount > 1)
		{
			CTreeError(psDeclSpecifierList->sLocation, "The signed modifier isn't supported with this base type");
			return IMG_FALSE;
		}
		else
		{
			CTreeError(psDeclSpecifierList->sLocation, "The unsigned modifier isn't supported with this base type");
			return IMG_FALSE;
		}
	}

	/*
		Check for the short modifier specified multiple times in the declaration.
	*/
	if (sInfo.uShortCount > 1)
	{
		CTreeError(psDeclSpecifierList->sLocation, "Duplicate short");
		return IMG_FALSE;
	}

	/*
		Check for both short and long specified in the declaration.
	*/
	if (sInfo.uLongCount > 0 && sInfo.uShortCount > 0)
	{
		CTreeError(psDeclSpecifierList->sLocation, "Both long and short specified in declaration");
		return IMG_FALSE;
	}

	/*
		Check for short or long modifiers used with unsupported base types.
	*/
	if (sInfo.uShortCount > 0)
	{
		if (	
				g_aeAtomicTypeToShortVariant[sInfo.eAtomicType] == ATOMICTYPE_UNKNOWN ||
				sInfo.bNonAtomicType ||
				sInfo.bMSVC_ExplicitType
		   )
		{
			CTreeError(psDeclSpecifierList->sLocation, "The short modifier can only be applied to int");
			return IMG_FALSE;
		}
	}
	if (sInfo.uLongCount > 0)
	{
		if (
				g_aeAtomicTypeToLongVariant[sInfo.eAtomicType] == ATOMICTYPE_UNKNOWN ||
				sInfo.bNonAtomicType ||
				sInfo.bMSVC_ExplicitType
		   )
		{
			CTreeError(psDeclSpecifierList->sLocation, "The long modifier can only be applied to int and double");
			return IMG_FALSE;
		}	
		else
		{
			if (sInfo.eAtomicType == ATOMICTYPE_UNSIGNEDINT)
			{
				if (sInfo.uLongCount > 2)
				{
					CTreeError(psDeclSpecifierList->sLocation, "The largest integer type supported is 64 bits");
					return IMG_FALSE;
				}
			}
			else
			{
				if (sInfo.uLongCount > 1)
				{
					CTreeError(psDeclSpecifierList->sLocation, "Only long doubles are supported");
					return IMG_FALSE;
				}
			}
		}
	}

	/*
		If the declaration includes signed modifiers then map the base type to
		a signed variant. The default is unsigned.
	*/
	if (sInfo.uSignedCount > 0)
	{
		if (g_aeAtomicTypeToSignedVariant[sInfo.eAtomicType] != ATOMICTYPE_UNKNOWN)
		{
			sInfo.eAtomicType = g_aeAtomicTypeToSignedVariant[sInfo.eAtomicType];
		}
	}

	/*
		If the declaration includes long modifiers then map the base type to a
		longer variant.
	*/
	for (uLongIdx = 0; uLongIdx < sInfo.uLongCount; uLongIdx++)
	{
		if (g_aeAtomicTypeToLongVariant[sInfo.eAtomicType] != ATOMICTYPE_UNKNOWN)
		{
			sInfo.eAtomicType = g_aeAtomicTypeToLongVariant[sInfo.eAtomicType];
		}
	}

	/*
		If the declaration includes a short modifier then map the base type to a
		short variant.
	*/
	if (sInfo.uShortCount > 0)
	{
		if (g_aeAtomicTypeToShortVariant[sInfo.eAtomicType] != ATOMICTYPE_UNKNOWN)
		{
			sInfo.eAtomicType = g_aeAtomicTypeToShortVariant[sInfo.eAtomicType];
		}
	}

	*peAtomicType = sInfo.eAtomicType;
	return IMG_TRUE;
}

static PCTTYPE LookupAtomicType(enum ATOMICTYPE eAtomicType)
{
	PCTTYPE psType;

	for (psType = g_psTypeListHead; psType != NULL; psType = psType->psNext)
	{
		if (psType->eTypeClass == TYPECLASS_BASE && psType->eAtomicType == eAtomicType)
		{
			return psType;
		}
	}

	IMG_ABORT();
}

static PCTTYPE GetType(YZDECLSPECIFIER* psDeclSpecifierList)
{
	enum ATOMICTYPE		eAtomicType;
	YZDECLSPECIFIER*	psDeclSpecifier;
	PCTTYPE				psType;

	if (!GetAtomicType(psDeclSpecifierList, &eAtomicType))
	{
		return LookupAtomicType(ATOMICTYPE_UNSIGNEDINT);
	}

	if (eAtomicType != ATOMICTYPE_UNKNOWN)
	{
		return LookupAtomicType(eAtomicType);
	}
	
	psType = MakeCTType();
	psType->sLocation = psDeclSpecifierList->sLocation;
	psType->eTypeClass = TYPECLASS_UNKNOWN;

	for (psDeclSpecifier = psDeclSpecifierList; psDeclSpecifier != NULL; psDeclSpecifier = psDeclSpecifier->psNext)
	{
		switch (psDeclSpecifier->uType)
		{
			case YZDECLSPECIFIER_STRUCT:
			{
				IMG_BOOL bNewType = IMG_TRUE;
				if (psType->eTypeClass != TYPECLASS_UNKNOWN)
				{
					CTreeError(psDeclSpecifierList->sLocation, "Different base types");
				}
				if (psDeclSpecifier->psStruct->bStruct)
				{
					psType->eTypeClass = TYPECLASS_STRUCT;
				}
				else
				{
					psType->eTypeClass = TYPECLASS_UNION;
				}
				if (psDeclSpecifier->psStruct->pszName != NULL)
				{
					PCTTYPE psPreviousType;

					psPreviousType = FindType(psType->eTypeClass, psDeclSpecifier->psStruct->pszName);
					if (psPreviousType != NULL)
					{
						if (psDeclSpecifier->psStruct->psMemberList != NULL &&
							psPreviousType->uStructMemberCount != 0)
						{
							CTreeError(psDeclSpecifierList->sLocation, "structure defined twice");
						}
						UseAsm_Free(psType);
						psType = psPreviousType;
						bNewType = IMG_FALSE;
					}
				}
				if (bNewType)
				{
					psType->pszName = psDeclSpecifier->psStruct->pszName;
					if (psType->pszName == NULL)
					{
						psType->bAnonymousType = IMG_TRUE;
					}
					AddToGlobalTypeList(psType);	
				}
				if (psDeclSpecifier->psStruct->psMemberList != NULL)
				{
					ParseStructOrUnion(psDeclSpecifier->psStruct->psMemberList, 
									   &psType->uStructMemberCount,
									   &psType->psStructMembers);
					if (psType->eTypeClass == TYPECLASS_STRUCT)
					{
						ResolveStruct(psType, psDeclSpecifier->psStruct->uAlignOverride);
					}
					else
					{
						ResolveUnion(psType);
					}
					psType->bCompletedStructure = IMG_TRUE;
				}
				break;
			}
			case YZDECLSPECIFIER_ENUM:
			{
				IMG_BOOL bNewType = IMG_TRUE;
				if (psType->eTypeClass != TYPECLASS_UNKNOWN)
				{
					CTreeError(psDeclSpecifierList->sLocation, "Different base types");
				}
				psType->eTypeClass = TYPECLASS_ENUM;
				if (psDeclSpecifier->psEnum->pszIdentifier != NULL)
				{
					PCTTYPE psPreviousType;

					psPreviousType = FindType(TYPECLASS_ENUM, psDeclSpecifier->psEnum->pszIdentifier);
					if (psPreviousType != NULL)
					{
						if (!psDeclSpecifier->psEnum->bPartial &&
							psPreviousType->uEnumeratorCount != 0)
						{
							CTreeError(psDeclSpecifierList->sLocation, "enumeration defined twice");
						}
						UseAsm_Free(psType);
						psType = psPreviousType;
						bNewType = IMG_FALSE;
					}
				}
				if (bNewType)
				{
					psType->pszName = psDeclSpecifier->psEnum->pszIdentifier;
					if (psType->pszName == NULL)
					{
						psType->bAnonymousType = IMG_TRUE;
					}
					AddToGlobalTypeList(psType);
				}
				if (!psDeclSpecifier->psEnum->bPartial)
				{
					ParseEnum(psDeclSpecifier->psEnum->psEnumeratorList, psType);
				}
				break;
			}
			case YZDECLSPECIFIER_TYPEDEF_NAME:
			{
				PCTTYPE	psTypedefType = FindType(TYPECLASS_TYPEDEF, psDeclSpecifier->pszIdentifier);

				psTypedefType = ResolveTypedef(psTypedefType);
				if (psType->eTypeClass != TYPECLASS_UNKNOWN)
				{
					if (psTypedefType != psType)
					{
						CTreeError(psDeclSpecifierList->sLocation, "Different base types");
					}
				}
				else
				{
					UseAsm_Free(psType);
					psType = psTypedefType;
				}
				break;
			}
		}
	}

	if (psType->eTypeClass == TYPECLASS_UNKNOWN)
	{
		UseAsm_Free(psType);
		return NULL;
	}

	return psType;
}

static enum STORAGE GetStorageClass(YZDECLSPECIFIER* psDeclSpecifierList)
{
	YZDECLSPECIFIER*	psDeclSpecifier;
	enum STORAGE		eStorage = STORAGE_UNKNOWN; 

	for (psDeclSpecifier = psDeclSpecifierList; psDeclSpecifier != NULL; psDeclSpecifier = psDeclSpecifier->psNext)
	{
		switch (psDeclSpecifier->uType)
		{
			case YZDECLSPECIFIER_TYPEDEF:
			{
				if (eStorage != STORAGE_UNKNOWN)
				{
					CTreeError(psDeclSpecifierList->sLocation, "Declaration has more than one storage class");
				}	
				eStorage = STORAGE_TYPEDEF;
				break;
			}
			case YZDECLSPECIFIER_EXTERN:
			{
				if (eStorage != STORAGE_UNKNOWN)
				{
					CTreeError(psDeclSpecifierList->sLocation, "Declaration has more than one storage class");
				}	
				eStorage = STORAGE_EXTERN;
				break;
			}
			case YZDECLSPECIFIER_STATIC:
			{
				if (eStorage != STORAGE_UNKNOWN)
				{
					CTreeError(psDeclSpecifierList->sLocation, "Declaration has more than one storage class");
				}	
				eStorage = STORAGE_STATIC;
				break;
			}
			case YZDECLSPECIFIER_AUTO:
			{
				if (eStorage != STORAGE_UNKNOWN)
				{
					CTreeError(psDeclSpecifierList->sLocation, "Declaration has more than one storage class");
				}	
				eStorage = STORAGE_AUTO;
				break;
			}
			case YZDECLSPECIFIER_REGISTER:
			{
				if (eStorage != STORAGE_UNKNOWN)
				{
					CTreeError(psDeclSpecifierList->sLocation, "Declaration has more than one storage class");
				}	
				eStorage = STORAGE_REGISTER;
				break;
			}
		}
	}

	return eStorage;
}

static IMG_VOID ParseDecl(YZDECLARATION* psDeclaration)
{
	enum STORAGE		eStorage;
	PCTTYPE				psType;
	YZINITDECLARATOR*	psInitDeclarator;

	if (psDeclaration->psInitDeclaratorList == NULL)
	{
		IMG_PCHAR pszName;

		if (RedeclFixup(psDeclaration->psDeclSpecifierList, &pszName))
		{
			psDeclaration->psInitDeclaratorList = CTreeMakeInitDeclarator(psDeclaration->sLocation);

			psDeclaration->psInitDeclaratorList->psDeclarator = CTreeMakeDeclarator(psDeclaration->sLocation);
			psDeclaration->psInitDeclaratorList->psDeclarator->uType = YZDECLARATOR_IDENTIFIER;
			psDeclaration->psInitDeclaratorList->psDeclarator->pszIdentifier = pszName;
			psDeclaration->psInitDeclaratorList->psInitializer = NULL;
		}
	}

	eStorage = GetStorageClass(psDeclaration->psDeclSpecifierList);

	psType = GetType(psDeclaration->psDeclSpecifierList);

	for (psInitDeclarator = psDeclaration->psInitDeclaratorList; psInitDeclarator != NULL; psInitDeclarator = psInitDeclarator->psNext)
	{
		if (eStorage == STORAGE_TYPEDEF)
		{
			PCTTYPE		psTypedefType;
			PCTTYPE		psExistingType;

			if (psInitDeclarator->psInitializer != NULL)
			{
				CTreeError(psInitDeclarator->psInitializer->sLocation, "A typedef can't be initialized");
			}
	
			psTypedefType = MakeCTType();
			psTypedefType->sLocation = psInitDeclarator->sLocation;
			psTypedefType->eTypeClass = TYPECLASS_TYPEDEF;
			psTypedefType->bAnonymousType = IMG_FALSE;
			psTypedefType->pszName = GetDeclaratorName(psInitDeclarator->psDeclarator);
			psTypedefType->psSubType = GetDeclaratorType(psInitDeclarator->psDeclarator, psType);

			if ((psExistingType = FindType(TYPECLASS_TYPEDEF, psTypedefType->pszName)) != NULL)
			{
				if (!EquivalentTypes(psExistingType->psSubType, psTypedefType->psSubType))
				{
					CTreeError(psInitDeclarator->psDeclarator->sLocation, "Duplicate typedef declaration");
					CTreeError(psExistingType->sLocation, "This is the location of the previous definition");
				}
			}
			else
			{
				AddToGlobalTypeList(psTypedefType);
			}
		}
	}
}

IMG_VOID CTreeAddExternDecl(YZDECLARATION* psDeclaration)
{
	ParseDecl(psDeclaration);
}

YZDECLARATION* CTreeMakeDeclaration(YYLTYPE sLocation)
{
	YZDECLARATION*	psDeclaration;
	
	psDeclaration = UseAsm_Malloc(sizeof(YZDECLARATION));

	psDeclaration->psDeclSpecifierList = NULL;
	psDeclaration->psInitDeclaratorList = NULL;
	psDeclaration->psNext = NULL;
	psDeclaration->sLocation = sLocation;

	return psDeclaration;
}

YZDECLSPECIFIER* CTreeAddToDeclSpecifierList(YZDECLSPECIFIER* psList, YZDECLSPECIFIER* psItem)
{
	YZDECLSPECIFIER*	psTemp;
	
	for (psTemp = psList; psTemp->psNext != NULL; psTemp = psTemp->psNext);
	psTemp->psNext = psItem;

	return psTemp;
}

YZINITDECLARATOR* CTreeAddToInitDeclaratorList(YZINITDECLARATOR* psList, YZINITDECLARATOR* psItem)
{
	YZINITDECLARATOR*	psTemp;

	if (psList == NULL)
	{
		return psItem;
	}
	
	for (psTemp = psList; psTemp->psNext != NULL; psTemp = psTemp->psNext);
	psTemp->psNext = psItem;

	return psList;
}

YZINITDECLARATOR* CTreeMakeInitDeclarator(YYLTYPE sLocation)
{
	YZINITDECLARATOR*	psInitDeclarator;
	
	psInitDeclarator = UseAsm_Malloc(sizeof(YZINITDECLARATOR));

	psInitDeclarator->psDeclarator = NULL;
	psInitDeclarator->psNext = NULL;
	psInitDeclarator->psInitializer = NULL;
	psInitDeclarator->sLocation = sLocation;

	return psInitDeclarator;
}

YZDECLSPECIFIER* CTreeMakeDeclSpecifier(YYLTYPE sLocation)
{
	YZDECLSPECIFIER*	psDeclSpecifier;

	psDeclSpecifier = UseAsm_Malloc(sizeof(YZDECLSPECIFIER));

	psDeclSpecifier->psEnum = NULL;
	psDeclSpecifier->psNext = NULL;
	psDeclSpecifier->psStruct = NULL;
	psDeclSpecifier->pszIdentifier = NULL;
	psDeclSpecifier->sLocation = sLocation;
	
	return psDeclSpecifier;
}

YZSTRUCT* CTreeMakeStruct(YYLTYPE sLocation)
{
	YZSTRUCT*			psStruct;

	psStruct = UseAsm_Malloc(sizeof(YZSTRUCT));

	psStruct->bStruct = IMG_FALSE;
	psStruct->psMemberList = NULL;
	psStruct->pszName = NULL;
	psStruct->sLocation = sLocation;

	return psStruct;
}

YZSTRUCTDECLARATION* CTreeAddToStructDeclarationList(YZSTRUCTDECLARATION* psList, YZSTRUCTDECLARATION* psItem)
{
	YZSTRUCTDECLARATION*	psTemp;
	
	for (psTemp = psList; psTemp->psNext != NULL; psTemp = psTemp->psNext);
	psTemp->psNext = psItem;

	return psList;
}

YZSTRUCTDECLARATION* CTreeMakeStructDeclaration(YYLTYPE sLocation)
{
	YZSTRUCTDECLARATION*	psStructDeclaration;

	psStructDeclaration = UseAsm_Malloc(sizeof(YZSTRUCTDECLARATION));

	psStructDeclaration->psDeclaratorList = NULL;
	psStructDeclaration->psNext = NULL;
	psStructDeclaration->psSpecifierList = NULL;
	psStructDeclaration->sLocation = sLocation;

	return psStructDeclaration;
}

YZSTRUCTDECLARATOR* CTreeAddToStructDeclaratorList(YZSTRUCTDECLARATOR* psList, YZSTRUCTDECLARATOR* psItem)
{
	YZSTRUCTDECLARATOR*	psTemp;
	
	for (psTemp = psList; psTemp->psNext != NULL; psTemp = psTemp->psNext);
	psTemp->psNext = psItem;

	return psList;
}

YZSTRUCTDECLARATOR* CTreeMakeStructDeclarator(YYLTYPE sLocation)
{
	YZSTRUCTDECLARATOR*		psStructDeclarator;

	psStructDeclarator = UseAsm_Malloc(sizeof(YZSTRUCTDECLARATOR));

	psStructDeclarator->bBitField = IMG_FALSE;
	psStructDeclarator->psDeclarator = NULL;
	psStructDeclarator->psBitFieldSize = NULL;
	psStructDeclarator->psNext = NULL;
	psStructDeclarator->sLocation = sLocation;

	return psStructDeclarator;
}

YZENUM* CTreeMakeEnum(YYLTYPE sLocation)
{
	YZENUM*	psEnum;

	psEnum = UseAsm_Malloc(sizeof(YZENUM));

	psEnum->bPartial = IMG_FALSE;
	psEnum->psEnumeratorList = NULL;
	psEnum->pszIdentifier = NULL;
	psEnum->sLocation = sLocation;

	return psEnum;
}

YZENUMERATOR* CTreeAddToEnumeratorList(YZENUMERATOR* psList, YZENUMERATOR* psItem)
{
	YZENUMERATOR*	psTemp;
	
	for (psTemp = psList; psTemp->psNext != NULL; psTemp = psTemp->psNext);
	psTemp->psNext = psItem;

	return psList;
}

YZENUMERATOR* CTreeMakeEnumerator(YYLTYPE sLocation)
{
	YZENUMERATOR*	psEnumerator;

	psEnumerator = UseAsm_Malloc(sizeof(YZENUMERATOR));

	psEnumerator->pszIdentifier = NULL;
	psEnumerator->psValue = NULL;
	psEnumerator->psNext = NULL;
	psEnumerator->sLocation = sLocation;

	return psEnumerator;
}

YZDECLARATOR* CTreeMakeDeclarator(YYLTYPE sLocation)
{
	YZDECLARATOR*	psDeclarator;

	psDeclarator = UseAsm_Malloc(sizeof(YZDECLARATOR));

	psDeclarator->psNext = NULL;
	psDeclarator->psPointer = NULL;
	psDeclarator->psSubDeclarator = NULL;
	psDeclarator->pszIdentifier = NULL;
	psDeclarator->psArraySize = NULL;
	psDeclarator->sLocation = sLocation;
	
	return psDeclarator;
}

YZPOINTER* CTreeMakePointer(YYLTYPE sLocation)
{
	YZPOINTER*	psPointer;

	psPointer = UseAsm_Malloc(sizeof(YZPOINTER));

	psPointer->psNext = NULL;
	psPointer->psQualifierList = NULL;
	psPointer->sLocation = sLocation;

	return psPointer;
}

YZTYPENAME* CTreeMakeTypeName(YYLTYPE				sLocation, 
							  YZDECLSPECIFIER*		psDeclSpecifierList, 
							  YZDECLARATOR*			psDeclarator)
{
	YZTYPENAME*	psTypename;

	psTypename = UseAsm_Malloc(sizeof(YZTYPENAME));

	psTypename->psDeclSpecifierList = psDeclSpecifierList;
	psTypename->psDeclarator = psDeclarator;
	psTypename->sLocation = sLocation;

	return psTypename;
}

IMG_BOOL CTreeIsTypedefName(IMG_PCHAR pszName)
{
	if (FindType(TYPECLASS_TYPEDEF, pszName) != NULL)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

void CollapseStringCodes(IMG_PCHAR pszCodes)
{
	IMG_PCHAR	pszIn, pszOut;

	pszIn = pszOut = pszCodes;
	while (*pszIn != '\0')
	{
		if (*pszIn == '\\')
		{
			pszIn++;
			*pszOut++ = *pszIn++;
		}
		else
		{
			*pszOut++ = *pszIn++;
		}
	}
	*pszOut = '\0';
}

static IMG_VOID LexerError(IMG_PCHAR pszFmt, ...) IMG_FORMAT_PRINTF(1, 2);

static IMG_VOID LexerError(IMG_PCHAR pszFmt, ...)
{
	va_list va;

	va_start(va, pszFmt);
	fprintf(stderr, "%s(%u): error: ", g_pszYzSourceFile, g_uYzSourceLine);
	vfprintf(stderr, pszFmt, va);
	fprintf(stderr, ".\n");
	va_end(va);

	g_bCCodeError = IMG_TRUE;
}

static IMG_PCHAR NextHashToken(IMG_PCHAR* ppszStr)
{
	IMG_PCHAR	pszStr = (*ppszStr);
	IMG_PCHAR	pszTokenStart;
	IMG_PCHAR	pszToken;
	IMG_BOOL	bInsideDoubleQuote;
	IMG_UINT32	uTokenLength;

	while (isspace(*pszStr))
	{
		pszStr++;
	}
	if (*pszStr == '\0')
	{
		return NULL;
	}
	
	if (!isalnum(*pszStr) && (*pszStr) != '"')
	{
		pszToken = UseAsm_Malloc(2);
		pszToken[0] = *pszStr;
		pszToken[1] = '\0';

		(*ppszStr) = pszStr + 1;

		return pszToken;
	}
	
	pszTokenStart = pszStr;
	bInsideDoubleQuote = IMG_FALSE;
	for (;;)
	{
		IMG_CHAR	cC;
		
		cC = *pszStr;
		if (cC == '\0')
		{
			break;
		}
		if (cC == '"')
		{
			bInsideDoubleQuote = !bInsideDoubleQuote;
		}
		if (!isalnum(cC) && !bInsideDoubleQuote)
		{
			break;
		}
		pszStr++;
	}
	(*ppszStr) = pszStr;
	
	uTokenLength = pszStr - pszTokenStart;
	pszToken = UseAsm_Malloc(uTokenLength + 1);
	UseAsm_MemCopy(pszToken, pszTokenStart, uTokenLength);
	pszToken[uTokenLength] = '\0';
	return pszToken;
}

static struct
{
	IMG_PCHAR	pszIdentifier;
	IMG_UINT32	uAlign;
}* g_psAlignStack = NULL;
static IMG_UINT32 g_uAlignStackTop = 0;

static IMG_VOID PackPragmaPushOrPop(IMG_BOOL bPush, IMG_PCHAR pszIdentifier)
{
	if (bPush)
	{
		/*
			Grow the packing stack.
		*/
		g_psAlignStack = realloc(g_psAlignStack, (g_uAlignStackTop + 1) * sizeof(g_psAlignStack[0]));
		if (g_psAlignStack == NULL)
		{
			fprintf(stderr, "Out of memory.\n");
			exit(1);
		}

		/*
			Create a new entry at the top of the packing stack containing the current alignment.
		*/
		if (pszIdentifier != NULL)
		{
			g_psAlignStack[g_uAlignStackTop].pszIdentifier = strdup(pszIdentifier);
			if (g_psAlignStack[g_uAlignStackTop].pszIdentifier == NULL)
			{
				fprintf(stderr, "Out of memory.\n");
				exit(1);
			}
		}
		else
		{
			g_psAlignStack[g_uAlignStackTop].pszIdentifier = NULL;
		}
		g_psAlignStack[g_uAlignStackTop].uAlign = g_uStructAlignOverride;
		g_uAlignStackTop++;
	}
	else
	{
		IMG_INT32	iLastIdxToPop;
		IMG_INT32	iIdx;

		if (pszIdentifier != NULL)
		{
			/*
				Look for an entry on the packing stack with the same identifier.
			*/
			for (iIdx = g_uAlignStackTop - 1; iIdx >= 0; iIdx--)
			{
				if (
						g_psAlignStack[iIdx].pszIdentifier != NULL &&
						strcmp(g_psAlignStack[iIdx].pszIdentifier, pszIdentifier) == 0
				   )
				{
					break;
				}
			}

			/*
				If the identifier doesn't appear on the stack then ignore the pop.
			*/
			if (iIdx < 0)
			{
				return;
			}
			iLastIdxToPop = iIdx;
		}
		else
		{
			/*
				Ignore popping from an empty stack.
			*/
			if (g_uAlignStackTop == 0)
			{
				return;
			}
			iLastIdxToPop = g_uAlignStackTop - 1;
		}


		for (iIdx = g_uAlignStackTop - 1; iIdx >= iLastIdxToPop; iIdx--)
		{
			/*
				Free any identifier associated with this stack entry.
			*/
			if (g_psAlignStack[iIdx].pszIdentifier != NULL)
			{
				UseAsm_Free(g_psAlignStack[iIdx].pszIdentifier);
			}

			/*
				Set the current structure alignment from the popped entry.
			*/
			g_uStructAlignOverride = g_psAlignStack[iIdx].uAlign;
		}

		g_uAlignStackTop = iLastIdxToPop;
		if (g_uAlignStackTop == 0)
		{
			g_psAlignStack = NULL;
		}
		else
		{
			g_psAlignStack = realloc(g_psAlignStack, sizeof(g_psAlignStack[0]) * g_uAlignStackTop);
		}
	}
}

static IMG_PCHAR ParsePackPragma_PushOrPop(IMG_BOOL bPush, IMG_PCHAR* ppszDirective)
{
	IMG_PCHAR	pszTok;

	pszTok = NextHashToken(ppszDirective);
	if (pszTok == NULL)
	{
		LexerError("Invalid #pragma pack: expected ')'");
		return NULL;
	}
	else if (strcmp(pszTok, ",") == 0)
	{
		pszTok = NextHashToken(ppszDirective);
		if (pszTok == NULL)
		{
			LexerError("Invalid #pragma pack: expected identifier or number");
			return NULL;
		}
		if (isalpha(*pszTok))
		{
			PackPragmaPushOrPop(bPush, pszTok);
			UseAsm_Free(pszTok);
				
			pszTok = NextHashToken(ppszDirective);
			if (pszTok != NULL)
			{
				if (strcmp(pszTok, ")") == 0)
				{
					UseAsm_Free(pszTok);
					return NULL;
				}
				else if (strcmp(pszTok, ",") == 0)
				{	
					pszTok = NextHashToken(ppszDirective);
					if (pszTok == NULL)
					{
						LexerError("Invalid #pragma pack: expected digit");
						return NULL;	
					}

					return pszTok;
				}
			}
			
			if (pszTok != NULL)
			{
				UseAsm_Free(pszTok);
			}
			LexerError("Invalid #pragma pack: expected ',' or ')'");
			return NULL;
		}
		else
		{
			PackPragmaPushOrPop(bPush, NULL /* pszIdentifier */);
			
			return pszTok;
		}
	}
	else if (strcmp(pszTok, ")") == 0)
	{
		UseAsm_Free(pszTok);
		PackPragmaPushOrPop(bPush, NULL /* pszIdentifier */);
		return NULL;
	}
	else
	{
		UseAsm_Free(pszTok);
		LexerError("Invalid #pragma pack: expected ')'");
		return NULL;
	}
}

static IMG_VOID ParsePackPragma(IMG_PCHAR pszDirective)
{
	IMG_PCHAR	pszTok;
	IMG_BOOL	bPop = IMG_FALSE;
	IMG_BOOL	bPush = IMG_FALSE;
	IMG_UINT32	uAlign;
	
	/*
		The first part of the pack pragma should be an '('.
	*/
	pszTok = NextHashToken(&pszDirective);
	if (pszTok == NULL || strcmp(pszTok, "(") != 0)
	{
		if (pszTok != NULL)
		{
			UseAsm_Free(pszTok);
		}
		LexerError("Invalid #pragma pack: expected '('");
		return;
	}
	UseAsm_Free(pszTok);
	
	pszTok = NextHashToken(&pszDirective);
	if (pszTok == NULL)
	{
		LexerError("Invalid #pragma pack: expected ')'");
		return;
	}
	
	if (strcmp(pszTok, ")") == 0)
	{
		/*
			An empty pragma ("pack ()") - set the structure alignment to the default.
		*/
		UseAsm_Free(pszTok);
		g_uStructAlignOverride = g_uDefaultStructAlignOverride;
		return;
	}
	else if (strcmp(pszTok, "push") == 0)
	{
		bPush = IMG_TRUE;
	}
	else if (strcmp(pszTok, "pop") == 0)
	{
		bPop = IMG_TRUE;
	}
	
	if (bPush || bPop)
	{
		UseAsm_Free(pszTok);

		/*
			Parse a push or pop command.
		*/
		if ((pszTok = ParsePackPragma_PushOrPop(bPush, &pszDirective)) == NULL)
		{
			return;
		}
	}
	
	if (!isdigit(*pszTok))
	{
		UseAsm_Free(pszTok);
		LexerError("Invalid #pragma pack: expected number, 'push' or 'pop'");
		return;
	}
	
	/*
		Get the alignment to set.
	*/
	uAlign = atoi(pszTok);
	UseAsm_Free(pszTok);
	if (uAlign != 1 && uAlign != 2 && uAlign != 4 && uAlign != 8 && uAlign != 16)
	{
		LexerError("Invalid alignment %d", uAlign);
	}
	else
	{
		g_uStructAlignOverride = uAlign;
	}
	
	/*
		Check the pack command is terminated by a ')'.
	*/
	pszTok = NextHashToken(&pszDirective);
	if (pszTok == NULL || strcmp(pszTok, ")") != 0)
	{
		LexerError("Invalid #pragma pack: expected ')'");
	}
	UseAsm_Free(pszTok);
}

IMG_INTERNAL
IMG_VOID ParseHashLine(IMG_PCHAR pszLine, IMG_UINT32 uLength)
{
	IMG_PCHAR pszTemp;
	IMG_PCHAR pszCommand;

	pszTemp = UseAsm_Malloc(uLength + 1);
	UseAsm_MemCopy(pszTemp, pszLine, uLength);
	pszTemp[uLength] = '\0';
	pszLine = pszTemp;

	pszCommand = NextHashToken(&pszLine);
	assert(strcmp(pszCommand, "#") == 0);

	pszCommand = NextHashToken(&pszLine);
	if (pszCommand == NULL)
	{
		/*
			A hash character by itself on a line. Just ignore.
		*/
	}
	else if (strcmp(pszCommand, "pragma") == 0)
	{
		IMG_PCHAR	pszPragmaCommand;
		
		/*
			Get the pragma command from the next token.
		*/
		UseAsm_Free(pszCommand);
		pszPragmaCommand = NextHashToken(&pszLine);
		if (pszPragmaCommand != NULL)
		{
			/*
				Check for a command we know.
			*/
			if (strcmp(pszPragmaCommand, "pack") == 0)
			{
				ParsePackPragma(pszLine);
			}

			/*
				Otherwise just ignore unknown commands.
			*/
			UseAsm_Free(pszPragmaCommand);
		}
	}
	else if (strcmp(pszCommand, "line") == 0 || (isdigit(*pszCommand)))
	{
		IMG_PCHAR pszEnd;

		/*
			Check for a command of the form:

				#line LINENUMBER FILENAME
			or
				#LINENUMBER FILENAME
		*/

		if (strcmp(pszCommand, "line") == 0)
		{
			UseAsm_Free(pszCommand);
			pszCommand = NextHashToken(&pszLine);
		}

		/*
			Store the line number in the file we are currently parsing (as recorded by
			the preprocessor).
		*/
		g_uYzSourceLine = atoi(pszCommand) - 1;

		UseAsm_Free(pszCommand);
		pszCommand = NextHashToken(&pszLine);

		/*
			Strip any enclosing double quotes from around the filename.
		*/
		if (*pszCommand == '\"')
		{
			pszCommand++;
		}
		if ((pszEnd = strchr(pszCommand, '\"')) != NULL)
		{
			*pszEnd = 0;
		}

		/*
			Store the name of the file we are currently parsing (as recorded by the
			preprocessor).
		*/
		g_pszYzSourceFile = pszCommand;
		CollapseStringCodes(g_pszYzSourceFile);
	}
	else
	{
		LexerError("invalid preprocessor command '%s'", pszCommand);
		UseAsm_Free(pszCommand);
	}

	UseAsm_Free(pszTemp);

	g_uYzSourceLine++;
}


IMG_VOID CTreeInitialize(IMG_VOID)
{
	static enum ATOMICTYPE	peAtomicTypes[] = {ATOMICTYPE_VOID,
											 ATOMICTYPE_UNSIGNEDCHAR,
											 ATOMICTYPE_SIGNEDCHAR,
											 ATOMICTYPE_UNSIGNEDSHORT,
											 ATOMICTYPE_SIGNEDSHORT,
											 ATOMICTYPE_UNSIGNEDINT,
   											 ATOMICTYPE_SIGNEDINT,
											 ATOMICTYPE_UNSIGNEDLONG,
											 ATOMICTYPE_SIGNEDLONG,
											 ATOMICTYPE_FLOAT,
											 ATOMICTYPE_DOUBLE,
											 ATOMICTYPE_LONGDOUBLE,
											 ATOMICTYPE_UNSIGNEDLONGLONG,
											 ATOMICTYPE_SIGNEDLONGLONG,
											 ATOMICTYPE_GCC_BUILTIN_VA_LIST,
											 ATOMICTYPE_BOOL};
	static IMG_PCHAR		psAtomicTypesName[] = {"void",
												 "unsigned char",
												 "signed char",
												 "unsigned short",
												 "signed short",
												 "unsigned int",
												 "signed int",
												 "unsigned long",
												 "signed long",
												 "float",
												 "double",
												 "long double",
												 "unsigned long long",
												 "signed long long",
												 "__builtin_va_list",
												 "_Bool"};
	IMG_UINT32 i;

	for (i = 0; i < (sizeof(peAtomicTypes) / sizeof(peAtomicTypes[0])); i++)
	{
		PCTTYPE	psType;

		psType = MakeCTType();
		psType->eTypeClass = TYPECLASS_BASE;
		psType->eAtomicType = peAtomicTypes[i];
		psType->bAnonymousType = IMG_FALSE;
		psType->pszName = psAtomicTypesName[i];
		AddToGlobalTypeList(psType);
	}

	for (i = 0; i < TYPECLASS_COUNT; i++)
	{
		UseAsm_MemSet(g_asTypeClassHashTable, 0, sizeof(g_asTypeClassHashTable));
	}
	UseAsm_MemSet(&g_sEnumeratorHashTable, 0, sizeof(g_sEnumeratorHashTable));
}

static IMG_VOID DumpTypeName(PCTTYPE psType)
{
	switch (psType->eTypeClass)
	{
		case TYPECLASS_BASE:
		case TYPECLASS_FIXEDARRAY:
		case TYPECLASS_VARIABLEARRAY:
		case TYPECLASS_POINTER:
		case TYPECLASS_FUNCTION:
		case TYPECLASS_TYPEDEF:
		{
			break;
		}
		case TYPECLASS_UNION:
		{
			printf("union ");
			break;
		}
		case TYPECLASS_STRUCT:
		{
			printf("struct ");
			break;
		}
		case TYPECLASS_ENUM:
		{
			printf("enum ");
			break;
		}
		default:
		{
			IMG_ABORT();
		}
	}
	
	if (psType->bAnonymousType)
	{
		printf("__anontype%u", psType->uTypeNum);
	}
	else
	{
		printf("%s", psType->pszName);	
	}
}

static IMG_VOID DumpType(PCTTYPE psType)
{
	switch (psType->eTypeClass)
	{
		case TYPECLASS_BASE:
		{
			printf("%s", psType->pszName);
			break;
		}
		case TYPECLASS_POINTER:
		{
			DumpTypeName(psType);
			printf(": %s%spointer to ", 
				psType->bVolatile ? "volatile " : "",
				psType->bConst ? "const " : "");
			DumpTypeName(psType->psSubType);
			break;
		}
		case TYPECLASS_TYPEDEF:
		{
			printf("typedef ");
			DumpTypeName(psType->psSubType);
			printf(" %s", psType->pszName);
			break;
		}
		case TYPECLASS_UNION:
		case TYPECLASS_STRUCT:
		{
			IMG_UINT32	uMember;
			DumpTypeName(psType);

			if (!psType->bCompletedStructure)
			{
				printf(": incomplete");
				break;
			}

			printf("\n{\n");
			for (uMember = 0; uMember < psType->uStructMemberCount; uMember++)
			{
				printf("\t");
				DumpTypeName(psType->psStructMembers[uMember].psType);
				if (psType->psStructMembers[uMember].pszName != NULL)
				{
					printf("\t%s", psType->psStructMembers[uMember].pszName);
				}
				else
				{
					printf("\t<anonymous>");
				}
				if (psType->psStructMembers[uMember].uBitFieldSize != USE_UNDEF)
				{
					printf(" : %u", psType->psStructMembers[uMember].uBitFieldSize);
				}
				printf(";\n");
			}
			printf("}");
			break;
		}
		case TYPECLASS_ENUM:
		{
			IMG_UINT32	uEnum;
			DumpTypeName(psType);
			printf("\n{\n");
			for (uEnum = 0; uEnum < psType->uEnumeratorCount; uEnum++)
			{
				printf("\t%s = %d\n", psType->psEnumerators[uEnum].pszName, psType->psEnumerators[uEnum].iValue);
			}
			printf("\n}");
			break;
		}
		case TYPECLASS_FIXEDARRAY:
		{
			DumpTypeName(psType);
			printf(": array of ");
			DumpTypeName(psType->psSubType);
			printf(" (length %u)", psType->uArraySize);
			break;
		}
		case TYPECLASS_VARIABLEARRAY:
		{
			DumpTypeName(psType);
			printf(": array of ");
			DumpTypeName(psType->psSubType);
			printf(" (variable length)");
			break;
		}
		case TYPECLASS_FUNCTION:
		{
			DumpTypeName(psType);
			printf(": function returning ");
			DumpTypeName(psType->psSubType);
			printf(" with parameters (\?\?)");
			break;
		}
		default: IMG_ABORT();
	}
}

IMG_VOID DumpGlobalTypes(IMG_VOID)
{
	PCTTYPE	psType;
	
	for (psType = g_psTypeListHead; psType != NULL; psType = psType->psNext)
	{
		DumpType(psType);
		printf("\n");
	}
}

YZEXPRESSION* CTreeMakeExpression(IMG_UINT32 uOperator, YYLTYPE sLocation)
{
	YZEXPRESSION*	psExpression;

	psExpression = UseAsm_Malloc(sizeof(YZEXPRESSION));
	psExpression->uOperator = uOperator;
	psExpression->psNext = NULL;
	psExpression->psSubExpr1 = NULL;
	psExpression->psSubExpr2 = NULL;
	psExpression->psSubExpr3 = NULL;
	psExpression->pszString = NULL;
	psExpression->uNumber = 0;
	psExpression->sLocation = sLocation;

	return psExpression;
}

YZEXPRESSION* CTreeAddToExpressionList(YZEXPRESSION* psList, YZEXPRESSION* psItem)
{
	YZEXPRESSION* psTemp;

	if (psList == NULL)
	{
		return psItem;
	}

	for (psTemp = psList; psTemp->psNext != NULL; psTemp = psTemp->psNext);

	psTemp->psNext = psItem;

	return psList;
}

static IMG_VOID StripSpace(IMG_PCHAR pszString)
{
	IMG_PCHAR pszIn = pszString, pszOut = pszString;

	while (*pszIn != '\0')
	{
		if (!isspace((int)*pszIn))
		{
			*pszOut++ = *pszIn;
		}
		pszIn++;
	}
	*pszOut = '\0';
}

static IMG_BOOL IsIdentifierChar(IMG_CHAR cC)
{
	return (IMG_BOOL)(isalnum((int)cC) || cC == '_');
}

IMG_VOID ResolveNameToType(IMG_PCHAR pszTypeMemberName, IMG_UINT32 uTypeMemberNameLength, IMG_PUINT32 puOffset, IMG_PUINT32 puSize, IMG_PCHAR pszErrorFilename, IMG_UINT32 uErrorLine)
{
	PCTTYPE psAtomicType = NULL;
	enum TYPECLASS eTypeClass;
	static struct
	{
		enum TYPECLASS	ePrefixClass;
		IMG_PCHAR		pszPrefixName;
	} psPrefixes[] = {{TYPECLASS_STRUCT, "struct"}, {TYPECLASS_UNION, "union"}};
	IMG_PCHAR pszTypeName;
	IMG_PCHAR pszDot;
	IMG_BOOL bEnd = IMG_FALSE;
	IMG_BOOL bArrayAccess ;
	IMG_UINT32 i;
	YYLTYPE sLocation;
	IMG_PCHAR pszTemp;

	sLocation.pszFilename = pszErrorFilename;
	sLocation.uLine = uErrorLine;

	if (puOffset != NULL)
	{
		*puOffset = 0;
	}
	if (puSize != NULL)
	{
		*puSize = 0;
	}

	pszTemp = UseAsm_Malloc(uTypeMemberNameLength + 1);
	UseAsm_MemCopy(pszTemp, pszTypeMemberName, uTypeMemberNameLength);
	pszTemp[uTypeMemberNameLength] = '\0';
	pszTypeMemberName = pszTemp;

	eTypeClass = TYPECLASS_TYPEDEF;	
	pszTypeName = pszTypeMemberName;
	for (i = 0; i < (sizeof(psPrefixes) / sizeof(psPrefixes[0])); i++)
	{
		IMG_UINT32 uPrefixLength = strlen(psPrefixes[i].pszPrefixName);
		if (strncmp(pszTypeMemberName, psPrefixes[i].pszPrefixName, uPrefixLength) == 0 && 
			isspace((int)pszTypeMemberName[uPrefixLength]))
		{
			pszTypeName = pszTypeMemberName + uPrefixLength;
			while (isspace((int)*pszTypeName))
			{
				pszTypeName++;
			}

			eTypeClass = psPrefixes[i].ePrefixClass;
			break;
		}
	}

	StripSpace(pszTypeName);

	pszDot = pszTypeName;

	for (;;)
	{
		while (IsIdentifierChar(*pszDot))
		{
			pszDot++;
		}
		if (*pszDot != '.' && *pszDot != '\0' && *pszDot != '[')
		{
			CTreeError(sLocation, "Unknown character '%c'.", *pszDot);
			return;
		}
		if (*pszDot == '[')
		{
			bArrayAccess = IMG_TRUE;
		}
		else
		{
			bArrayAccess = IMG_FALSE;
		}
		if (*pszDot == '\0')
		{
			bEnd = IMG_TRUE;
		}
		*pszDot++ = '\0';

		if (psAtomicType == NULL)
		{
			if (puSize == NULL && bEnd)
			{
				CTreeError(sLocation, "The parameter to the OFFSET operator must specify a structure member.");
				return;
			}

			psAtomicType = FindType(eTypeClass, pszTypeName);
			if (psAtomicType == NULL)
			{
				CTreeError(sLocation, "Type %s is unknown.", pszTypeName);
				return;
			}
		}
		else
		{
			CTSTRUCTMEMBER* psStructMember;
			PCTTYPE psStructType;

			psStructType = psAtomicType;
			while (psStructType->eTypeClass == TYPECLASS_TYPEDEF)
			{
				psStructType = psStructType->psSubType;
			}
			if (psStructType->eTypeClass != TYPECLASS_STRUCT &&
				psStructType->eTypeClass != TYPECLASS_UNION)
			{
				CTreeError(sLocation, "Type %s is not a structure or a union.", psAtomicType->pszName);
				return;
			}

			psStructMember = FindStructMember(psStructType->uStructMemberCount, psStructType->psStructMembers, pszTypeName);
			if (psStructMember == NULL)
			{
				CTreeError(sLocation, "No such structure member %s in type %s.", pszTypeName, psAtomicType->pszName);
				return;
			}
			if (puOffset != NULL)
			{
				*puOffset += psStructMember->uByteOffset;
			}
			psAtomicType = psStructMember->psType;
		}

		pszTypeName = pszDot;

		while (bArrayAccess)
		{
			IMG_UINT32		uArrayIndex;		
			PCTTYPE			psArrayType;

			psArrayType = psAtomicType;
			while (psArrayType->eTypeClass == TYPECLASS_TYPEDEF)
			{
				psArrayType = psArrayType->psSubType;
			}

			if (psArrayType->eTypeClass != TYPECLASS_FIXEDARRAY &&
				psArrayType->eTypeClass != TYPECLASS_VARIABLEARRAY)
			{
				CTreeError(sLocation, "Type %s isn't an array.", psAtomicType->pszName);
				return;
			}

			uArrayIndex = strtoul(pszTypeName, &pszTypeName, 10);
			if (*pszTypeName != ']')
			{
				CTreeError(sLocation, "Unknown character '%c'.", *pszTypeName);
				return;
			}
			pszTypeName++;

			if (puOffset != NULL)
			{
				*puOffset += uArrayIndex * GetTypeSize(psArrayType->psSubType);
			}

			/*
				Move to the type of array elements.
			*/
			psAtomicType = psArrayType->psSubType;

			if (*pszTypeName == '\0')
			{
				/*
					Reached the complete end of the expression.
				*/
				bEnd = IMG_TRUE;
				break;
			}
			else if (*pszTypeName == '[')
			{
				/*
					Repeat the loop to parse an index into the next dimension of the array.
				*/
				pszTypeName++;
				bArrayAccess = IMG_TRUE;
			}
			else
			{
				if (*pszTypeName != '.')
				{
					CTreeError(sLocation, "Unknown character '%c'.", *pszTypeName);
					return;
				}
				pszTypeName++;
				bArrayAccess = IMG_FALSE;
				pszDot = pszTypeName;
			}
		}

		if (bEnd)
		{
			break;
		}
	}

	if (puSize != NULL)
	{
		*puSize = GetTypeSize(psAtomicType);
	}
}

