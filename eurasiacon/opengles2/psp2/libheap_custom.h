/*                 -*- mode: c; tab-width: 4; indent-tabs-mode: t; -*- */
/* SCE CONFIDENTIAL
 PlayStation(R)Vita Programmer Tool Runtime Library Release 03.570.011
 *
 *      Copyright (C) 2011 Sony Computer Entertainment Inc.
 *                        All Rights Reserved.
 *
 */
/*
 * 
 * 
 * 
 * 
 * 
 *
 *                        Heap Library
 *
 *                         libheap.h
 *
 *       Version        Date            Design      Log
 *  --------------------------------------------------------------------
 *       0.00           2011-01-15      kono        import from PSP library
 */

#include "services.h"
#include "img_types.h"

#ifndef _SCE_LIBHEAP_H
#define _SCE_LIBHEAP_H

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
extern "C" {
#endif	/* defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus) */


/*
 * Error number definition
 */

/**
 * @j 不正なヒープ @ej
 * @e invalid heap @ee
 */
#define SCE_HEAP_ERROR_INVALID_ID				-2142306304	/* 0x804F0000 */

/**
 * @j 不正なポインタ引数 @ej
 * @e invalid pointer argument @ee
 */
#define SCE_HEAP_ERROR_INVALID_POINTER			-2142306303	/* 0x804F0001 */



#define SCE_HEAP_AUTO_EXTEND		0x0001U

#define SCE_HEAP_OPT_MEMBLOCK_TYPE_USER		0
#define SCE_HEAP_OPT_MEMBLOCK_TYPE_USER_NC	1
#define SCE_HEAP_OPT_MEMBLOCK_TYPE_CDRAM	2

typedef struct SceHeapOptParam {
	unsigned int size;
	unsigned int memblockType;
} SceHeapOptParam;

typedef struct SceHeapAllocOptParam {
	unsigned int size;
	unsigned int alignment;
} SceHeapAllocOptParam;

typedef struct SceHeapMallinfo {
	int arena;					/*J カーネルから割り当てた総メモリサイズ  */
								/*E Total space allocated from system     */
	int ordblks;				/*J 割り当てブロックの数                  */
								/*E Number of ordinary blocks             */
	int smblks;					/*J 未使用(常に0)                         */
								/*E Unused -- always zero                 */
	int hblks;					/*J 保持ブロック数                        */
								/*E Number of FPL regions                 */
	int hblkhd;					/*J 保持ブロックの総バイト数(arenaと同じ) */
								/*E Total space allocated from FPL(arena) */
	int usmblks;				/*J 未使用(常に0)                         */
								/*E Unused -- always zero                 */
	int fsmblks;				/*J 未使用(常に0)                         */
								/*E Unused -- always zero                 */
	int uordblks;				/*J 割り当て済みメモリサイズの合計        */
								/*E Total allocated space                 */
	int fordblks;				/*J 割り当て可能なメモリサイズの合計      */
								/*E Total non in-use space                */
	int keepcost;				/*J 未使用(常に0)                         */
								/*E Unused -- always zero                 */
} SceHeapMallinfo;


/*J ヒープメモリの作成 */
/*E Create heap memory */
void *sceHeapCreateHeap(PVRSRV_DEV_DATA *psDevData, IMG_SID hDevMemContext, const char *name, unsigned int heapblocksize, int flags, const SceHeapOptParam *optParam);

/*J ヒープメモリの削除 */
/*E Delete heap memory */
int   sceHeapDeleteHeap(void *heap);

/*J ヒープメモリからメモリ確保 */
/*E Allocate memory from heap memory */
void *sceHeapAllocHeapMemory(void *heap, unsigned int nbytes);
void *sceHeapAllocHeapMemoryWithOption(void *heap, unsigned int nbytes, const SceHeapAllocOptParam *optParam);

/*J ヒープメモリへメモリ解放 */
/*E Release memory back to heap memory */
int   sceHeapFreeHeapMemory(void *heap, void *ptr);

/*J ヒープメモリからメモリ再確保 */
/*E Re-allocate memory from heap memory */
void *sceHeapReallocHeapMemory(void *heap, void *ptr, unsigned int nbytes);
void *sceHeapReallocHeapMemoryWithOption(void *heap, void *ptr, unsigned int nbytes, const SceHeapAllocOptParam *optParam);


/*J このヒープからアロケートされたメモリかどうか判定 */
/*E Check whether it is a memory allocated from this heap. */
int   sceHeapIsAllocatedHeapMemory(void *heap, void *ptr);

/*J ヒープメモリの空きサイズを取得 */
/*E Get size of empty heap memory */
int   sceHeapGetTotalFreeSize(void *heap);

/*J ヒープメモリの情報を取得 */
/*E Get heap memory usage status */
int   sceHeapGetMallinfo(void *heap, SceHeapMallinfo *pInfo);

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
}
#endif	/* defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus) */

#endif	/* _SCE_LIBHEAP_H */


/* Local variables: */
/* tab-width: 4 */
/* End: */
/* vi:set tabstop=4: */
