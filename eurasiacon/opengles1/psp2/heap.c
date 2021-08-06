/*                 -*- mode: c; tab-width: 4; indent-tabs-mode: t; -*- */
/* SCE CONFIDENTIAL
 PlayStation(R)Vita Programmer Tool Runtime Library Release 03.570.011
 *
 *      Copyright (C) 2012 Sony Computer Entertainment Inc.
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
 *                           heap.c
 *
 *       Version        Date            Design      Log
 *  --------------------------------------------------------------------
 *       0.00           2011-01-15      kono        import from PSP library
 *       0.01           2011-09-21      kono        fix    sceHeapIsAllocatedHeapMemory()
 *       0.02           2011-10-04      kono        fix    lwmutex leakage
 *       0.03           2012-01-25      kono        fix    sceHeapDeleteHeap()
 *       0.04           2012-03-07      kono        fix    sceHeapReallocHeapMemoryWithOption()
 */

#include <string.h>

#include <kernel.h>
#include <sceerror.h>
#include "libheap_custom.h"
#include "heaplib_internal.h"
#include "services.h"
#include "img_types.h"
#include "psp2_pvr_defs.h"

#define ALIGN(x, a)	(((x) + ((a) - 1)) & ~((a) - 1))

//J プロトタイプ宣言
//E prototype declaration
int _sceHeapQueryBlockInfo(void *heap, void *ptr, unsigned int *puiSize, int *piBlockIndex, SceHeapMspaceLink **msplink);
int _sceHeapQueryBlockIndex(void *heap, void *ptr);
int _sceHeapQueryAllocatedSize(void *heap, void *ptr);


//J ヒープメモリの作成
//E Create heap memory
void	*sceHeapCreateHeap(PVRSRV_DEV_DATA *psDevData, IMG_SID hDevMemContext, const char *name, unsigned int heapblocksize, int flags, const SceHeapOptParam *optParam)
{
	SceHeapWorkInternal	*head;
	SceHeapMspaceLink	*hp;
	SceUID uid;
	unsigned int memblockType = SCE_HEAP_OPT_MEMBLOCK_TYPE_USER;
	void *p;
	int res;
	int i;

	if (optParam) {
		memblockType = optParam->memblockType;
	}

	//J サイズ0および負数はエラーにします。
	if (((int)heapblocksize) <= 0) {
		return (SCE_NULL);
	}

	//J SCE_HEAP_AUTO_EXTEND以外のビットはエラーにします
	if (flags & ~SCE_HEAP_AUTO_EXTEND) {
		return (SCE_NULL);
	}

	//J バイト単位まで切り上げ
	switch (memblockType) {
	case SCE_HEAP_OPT_MEMBLOCK_TYPE_USER:
		heapblocksize = ALIGN(heapblocksize, 4096);
		uid = sceKernelAllocMemBlock(name, SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, heapblocksize, SCE_NULL);
		break;
	case SCE_HEAP_OPT_MEMBLOCK_TYPE_USER_NC:
		heapblocksize = ALIGN(heapblocksize, 4096);
		uid = sceKernelAllocMemBlock(name, SCE_KERNEL_MEMBLOCK_TYPE_USER_NC_RW, heapblocksize, SCE_NULL);
		break;
	case SCE_HEAP_OPT_MEMBLOCK_TYPE_CDRAM:
		heapblocksize = ALIGN(heapblocksize, 256 * 1024);
		uid = sceKernelAllocMemBlock(name, SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, heapblocksize, SCE_NULL);
		break;
	}

	if (uid < 0) {
		return (SCE_NULL);
	}
	res = sceKernelGetMemBlockBase(uid, &p);
	if (res < 0) {
		sceKernelFreeMemBlock(uid);
		return (SCE_NULL);
	}

	res = PVRSRVMapMemoryToGpu(
		psDevData,
		hDevMemContext,
		0,
		heapblocksize,
		0,
		p,
		PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MEM_USER_SUPPLIED_DEVVADDR,
		IMG_NULL);
	if (res != PVRSRV_OK) {
		sceKernelFreeMemBlock(uid);
		return (SCE_NULL);
	}

	head        = (SceHeapWorkInternal *)p;
	head->bsize = (flags & SCE_HEAP_AUTO_EXTEND) ? heapblocksize : 0;
	for(i=0; name[i]!=0 && i<SCE_UID_NAMELEN; i++) {
		head->name[i] = name[i];
	}
	head->name[i] = 0x00;
#if USE_HEAPINFO
	head->info.arena    = heapblocksize;	//J カーネルから割り当てた総メモリサイズ
	head->info.ordblks  = 0;				//J 割り当て済み標準ブロックの数
	head->info.hblks    = 1;				//J カーネルから割り当てた保持FPL数
#endif	/* USE_HEAPINFO */

	//J 排他制御用の軽量ミューテックスを生成
	res = sceKernelCreateLwMutex(&head->lwmtx, name, SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE | SCE_KERNEL_LW_MUTEX_ATTR_TH_FIFO, 0, SCE_NULL);
	if (res < 0) {
		PVRSRVUnmapMemoryFromGpu(psDevData, p, 0, IMG_FALSE);
		sceKernelFreeMemBlock(uid);
		return (SCE_NULL);
	}

	hp = &head->prim;
	hp->next = hp;
	hp->prev = hp;
	hp->uid  = uid;
	hp->size = heapblocksize - sizeof(SceHeapWorkInternal);
	hp->msp  = sceClibMspaceCreate((hp + 1), hp->size);

	head->magic = (SceUIntPtr)(head + 1);

	head->psDevData = psDevData;
	head->hDevMemContext = hDevMemContext;
	head->memblockType = memblockType;

	return (head);
}


//J ヒープメモリの削除
//E Delete heap memory
int sceHeapDeleteHeap(void *heap)
{
	SceHeapWorkInternal	*head;
	SceHeapMspaceLink	*hp, *next;
	void *tmpBase;
	int res;

	head = (SceHeapWorkInternal *)heap;

	if (head == SCE_NULL) {
		return (SCE_HEAP_ERROR_INVALID_ID);
	}
	if (head->magic != (SceUIntPtr)(head + 1)) {
		return (SCE_HEAP_ERROR_INVALID_ID);
	}

	//J 排他制御用の軽量ミューテックスをロック
	res = sceKernelLockLwMutex(&head->lwmtx, 1, SCE_NULL);
	if (res < 0) {
		return (res);
	}

	head->magic = 0;
	sceKernelUnlockLwMutex(&head->lwmtx, 1);

	sceKernelDeleteLwMutex(&head->lwmtx);

	for (hp = head->prim.next; ; hp = next) {
		next = hp->next;
		sceClibMspaceDestroy(hp->msp);
		sceKernelGetMemBlockBase(hp->uid, &tmpBase);
		PVRSRVUnmapMemoryFromGpu(head->psDevData, tmpBase, 0, IMG_FALSE);
		sceKernelFreeMemBlock(hp->uid);
		if (hp == &(head->prim)) {
			break;
		}
	}

	return (SCE_OK);
}


//J ヒープメモリからメモリ確保
//E Allocate memory from heap memory
void	*sceHeapAllocHeapMemoryWithOption(void *heap, unsigned int nbytes, const SceHeapAllocOptParam *optParam)
{
	SceHeapWorkInternal	*head;
	SceHeapMspaceLink	*hp;
	SceSize	alignment;
	int		res;
	void	*result;

	head = (SceHeapWorkInternal *)heap;

#if defined(DEBUG)
	sceClibPrintf("sceHeapAllocHeapMemoryWithOption:\n\nHeap type 0x%X\nAllocation size: 0x%X\n\n", head->memblockType, nbytes);
#endif

	if (head == SCE_NULL) {
		return (SCE_NULL);
	}
	if (head->magic != (SceUIntPtr)(head + 1)) {
		return (SCE_NULL);
	}

	//J optParam引数があったときalignment指定を取得
	if (optParam != SCE_NULL) {
		if (optParam->size != sizeof(SceHeapAllocOptParam)) {
			return (SCE_NULL);
		}

		//J alignmentの値の妥当性チェック
		alignment = optParam->alignment;
		if (alignment == 0 || alignment > 4096 || (alignment % sizeof(int) != 0) || (((alignment - 1) & alignment) != 0)) {
			return (SCE_NULL);
		}
	} else {
		alignment = 0;
	}

	//J 排他制御用の軽量ミューテックスをロック
	res = sceKernelLockLwMutex(&head->lwmtx, 1, SCE_NULL);
	if (res < 0) {
		return (SCE_NULL);
	}

	result = SCE_NULL;
	for (hp = head->prim.next; ; hp = hp->next) {
		if (alignment != 0) {
			result = sceClibMspaceMemalign(hp->msp, alignment, nbytes);
		} else {
			result = sceClibMspaceMalloc(hp->msp, nbytes);
		}
		if (result != SCE_NULL) {
#if USE_HEAPINFO
			//J 割り当て済み標準ブロックの数をインクリメント
			head->info.ordblks++;
#endif	/* USE_HEAPINFO */
			sceKernelUnlockLwMutex(&head->lwmtx, 1);
			return (result);
		}
		if (hp == &(head->prim)) {
			break;
		}
	}
	if (head->bsize > 3) {
		//J SCE_HEAP_AUTO_EXTENDを意味している
		SceUID uid;
		void *p;
		unsigned int hsize = (((head->bsize) >> 12) << 12);
		unsigned int nbytes2;

		if (alignment != 0) {
			//J alignment指定あり
			SceUInt remainder = 0;
			SceUInt extrasize = 0;

			nbytes2 = ((nbytes + 7) >> 3) << 3;
			remainder = SCE_HEAP_OFFSET_TO_VALID_HEAP & (alignment - 1);
			if (remainder != 0) {
				extrasize = alignment - remainder;
			}
			if (nbytes2 + extrasize > hsize - SCE_HEAP_MSPACE_LINK_OVERHEAD) {
				hsize = nbytes2 + extrasize + SCE_HEAP_MSPACE_LINK_OVERHEAD;

				//J 4096バイト単位まで切り上げ
				hsize = ((hsize + 4095) >> 12) << 12;
			}
		} else {
			//J alignment指定なし
			if (nbytes > hsize - SCE_HEAP_MSPACE_LINK_OVERHEAD) {
				nbytes2 = ((nbytes + 7) >> 3) << 3;
				hsize = nbytes2 + SCE_HEAP_MSPACE_LINK_OVERHEAD;

				//J 4096バイト単位まで切り上げ
				hsize = ((hsize + 4095) >> 12) << 12;
			}
		}

		switch (head->memblockType) {
		case SCE_HEAP_OPT_MEMBLOCK_TYPE_USER:
			uid = sceKernelAllocMemBlock(head->name, SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, hsize, SCE_NULL);
			break;
		case SCE_HEAP_OPT_MEMBLOCK_TYPE_USER_NC:
			uid = sceKernelAllocMemBlock(head->name, SCE_KERNEL_MEMBLOCK_TYPE_USER_NC_RW, hsize, SCE_NULL);
			break;
		case SCE_HEAP_OPT_MEMBLOCK_TYPE_CDRAM:
			hsize = ALIGN(hsize, 256 * 1024);
			uid = sceKernelAllocMemBlock(head->name, SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, hsize, SCE_NULL);
			break;
		}

		if (uid < 0) {
			sceKernelUnlockLwMutex(&head->lwmtx, 1);
			return (SCE_NULL);
		}
		res = sceKernelGetMemBlockBase(uid, &p);
		if (res < 0) {
			sceKernelFreeMemBlock(uid);
			sceKernelUnlockLwMutex(&head->lwmtx, 1);
			return (SCE_NULL);
		}

		res = PVRSRVMapMemoryToGpu(
			head->psDevData,
			head->hDevMemContext,
			0,
			hsize,
			0,
			p,
			PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MEM_USER_SUPPLIED_DEVVADDR,
			IMG_NULL);

		if (res != PVRSRV_OK) {
			sceKernelFreeMemBlock(uid);
			sceKernelUnlockLwMutex(&head->lwmtx, 1);
			return (SCE_NULL);
		}

#if USE_HEAPINFO
		//J 保持ブロック数とサイズを増やす
		head->info.hblks++;				//J 保持ブロック数(mmap)
		head->info.arena += hsize;		//J 保持ブロックのサイズ
#endif	/* USE_HEAPINFO */

		hp = (SceHeapMspaceLink *)p;

		if (hp != SCE_NULL) {
			hp->uid  = uid;
			hp->size = hsize - sizeof(SceHeapMspaceLink);
			hp->msp  = sceClibMspaceCreate((hp + 1), hp->size);

			//J 双方向リンクリストに追加します。
			//E insert to double-linked linst
			hp->next = head->prim.next;
			hp->prev = head->prim.next->prev;
			head->prim.next->prev = hp;
			hp->prev->next        = hp;

			if (alignment != 0) {
				result = sceClibMspaceMemalign(hp->msp, alignment, nbytes);
			} else {
				result = sceClibMspaceMalloc(hp->msp, nbytes);
			}
		}
	}
#if USE_HEAPINFO
	//J 割り当て済み標準ブロックの数をインクリメント
	head->info.ordblks++;
#endif	/* USE_HEAPINFO */
	sceKernelUnlockLwMutex(&head->lwmtx, 1);

#if defined(DEBUG)
	res = PVRSRVCheckMappedMemory(
		head->psDevData,
		head->hDevMemContext,
		result,
		nbytes,
		PVRSRV_MEM_READ | PVRSRV_MEM_WRITE
		);

	sceClibPrintf("sceHeapAllocHeapMemoryWithOption check res: %d\n", res);
	sceClibPrintf("base: 0x%X\n", result);
	sceClibPrintf("size: 0x%X\n", nbytes);
#endif

	return (result);
}


//J ヒープメモリからメモリ確保
//E Allocate memory from heap memory
void *sceHeapAllocHeapMemory(void *heap, unsigned int nbytes)
{
	return (sceHeapAllocHeapMemoryWithOption(heap, nbytes, SCE_NULL));
}


//J ヒープメモリへメモリ解放
//E Release memory back to heap memory
int	sceHeapFreeHeapMemory(void *heap, void *ptr)
{
	SceHeapWorkInternal	*head;
	SceHeapMspaceLink	*hp;
	int res;
	void *tmpBase;

	head = (SceHeapWorkInternal *)heap;

	if (head == SCE_NULL) {
		return (SCE_HEAP_ERROR_INVALID_ID);
	}
	if (head->magic != (SceUIntPtr)(head + 1)) {
		return (SCE_HEAP_ERROR_INVALID_ID);
	}

	//J 排他制御用の軽量ミューテックスをロック
	res = sceKernelLockLwMutex(&head->lwmtx, 1, SCE_NULL);
	if (res < 0) {
		return (res);
	}

	if (ptr == SCE_NULL) {
		sceKernelUnlockLwMutex(&head->lwmtx, 1);
		return (0);
	}
	for (hp = head->prim.next; ; hp = hp->next) {
		if (_sceHeapIsPointerInBound(hp, ptr)) {
			sceClibMspaceFree(hp->msp, ptr);

			if (hp != &head->prim && sceClibMspaceIsHeapEmpty(hp->msp)) {
				//J 双方向リンクリストから抜きます
				//E unlink from double-liked list
				hp->next->prev = hp->prev;
				hp->prev->next = hp->next;

#if USE_HEAPINFO
				//J 保持ブロック数とサイズを減らす
				head->info.hblks--;
				switch (head->memblockType) {
				case SCE_HEAP_OPT_MEMBLOCK_TYPE_USER:
				case SCE_HEAP_OPT_MEMBLOCK_TYPE_USER_NC:
					head->info.arena -= ALIGN(hp->size, 4096);
					break;
				case SCE_HEAP_OPT_MEMBLOCK_TYPE_CDRAM:
					head->info.arena -= ALIGN(hp->size, 256 * 1024);
					break;
				}
#endif	/* USE_HEAPINFO */
				sceClibMspaceDestroy(hp->msp);
				sceKernelGetMemBlockBase(hp->uid, &tmpBase);
				PVRSRVUnmapMemoryFromGpu(head->psDevData, tmpBase, 0, IMG_FALSE);
				sceKernelFreeMemBlock(hp->uid);
			}
#if USE_HEAPINFO
			//J 割り当て済み標準ブロックの数をデクリメント
			head->info.ordblks--;
#endif	/* USE_HEAPINFO */
			sceKernelUnlockLwMutex(&head->lwmtx, 1);
			return (0);
		}
		if (hp == &(head->prim)) {
			break;
		}
	}
	//J 不正なメモリブロックアドレスを指定された場合
	sceKernelUnlockLwMutex(&head->lwmtx, 1);
	return (SCE_HEAP_ERROR_INVALID_POINTER);
}


//J ヒープメモリからメモリ再確保
//E Re-allocate memory from heap memory
void *sceHeapReallocHeapMemoryWithOption(void *heap, void *ptr, unsigned int nbytes, const SceHeapAllocOptParam *optParam)
{
	SceHeapWorkInternal	*head;
	SceHeapMspaceLink	*hp;
	void *newptr;
	SceSize alignment;
	int res;
	unsigned int uiSize;

	if (ptr == SCE_NULL) {
		return (sceHeapAllocHeapMemoryWithOption(heap, nbytes, optParam));
	}
	if (nbytes == 0) {
		sceHeapFreeHeapMemory(heap, ptr);
		return (SCE_NULL);
	}

	head = (SceHeapWorkInternal *)heap;

	if (head == SCE_NULL) {
		return (SCE_NULL);
	}
	if (head->magic != (SceUIntPtr)(head + 1)) {
		return (SCE_NULL);
	}

	//J optParam引数があったときalignment指定を取得
	if (optParam != SCE_NULL) {
		if (optParam->size != sizeof(SceHeapAllocOptParam)) {
			return (SCE_NULL);
		}

		//J alignmentの値の妥当性チェック
		alignment = optParam->alignment;
		if (alignment == 0 || alignment > 4096 || (alignment % sizeof(int) != 0) || (((alignment - 1) & alignment) != 0)) {
			return (SCE_NULL);
		}
	} else {
		alignment = 0;
	}

	//J 排他制御用の軽量ミューテックスをロック
	res = sceKernelLockLwMutex(&head->lwmtx, 1, SCE_NULL);
	if (res < 0) {
		return (SCE_NULL);
	}

	//J 指定されたブロックのアロケートサイズおよび所属lowheapを検索
	res = _sceHeapQueryBlockInfo(heap, ptr, &uiSize, SCE_NULL, &hp);
	if (res < 0) {
		sceKernelUnlockLwMutex(&head->lwmtx, 1);
		return (SCE_NULL);
	}
	if (uiSize == 0) {
		sceKernelUnlockLwMutex(&head->lwmtx, 1);
		return (SCE_NULL);
	}

	//J 現在のmspace上でリアロケートできるかチェック
	if (alignment == 0) {
		newptr = sceClibMspaceRealloc(hp->msp, ptr, nbytes);
	} else {
		newptr = sceClibMspaceReallocalign(hp->msp, ptr, nbytes, alignment);
	}
	if (newptr != SCE_NULL) {
		//J メモリ確保成功しました
		sceKernelUnlockLwMutex(&head->lwmtx, 1);
		return (newptr);
	}

	//J 新しいバッファを確保します
	newptr = sceHeapAllocHeapMemoryWithOption(heap, nbytes, optParam);
	if (newptr == SCE_NULL) {
		//J メモリ確保失敗しました
		sceKernelUnlockLwMutex(&head->lwmtx, 1);
		return (SCE_NULL);
	}
	sceClibMemcpy(newptr, ptr, uiSize);
	res = sceHeapFreeHeapMemory(heap, ptr);

	sceKernelUnlockLwMutex(&head->lwmtx, 1);
	return (newptr);
}


//J ヒープメモリからメモリ再確保
//E Re-allocate memory from heap memory
void *sceHeapReallocHeapMemory(void *heap, void *ptr, unsigned int nbytes)
{
	return (sceHeapReallocHeapMemoryWithOption(heap, ptr, nbytes, SCE_NULL));
}



int	_sceHeapQueryBlockInfo(void *heap, void *ptr, unsigned int *puiSize, int *piBlockIndex, SceHeapMspaceLink **msplink)
{
	SceHeapWorkInternal	*head;
	SceHeapMspaceLink	*hp;
	int res;
	int cnt;

	head = (SceHeapWorkInternal *)heap;

	if (head == SCE_NULL) {
		return (SCE_HEAP_ERROR_INVALID_ID);
	}
	if (head->magic != (SceUIntPtr)(head + 1)) {
		return (SCE_HEAP_ERROR_INVALID_ID);
	}

	//J 排他制御用の軽量ミューテックスをロック
	res = sceKernelLockLwMutex(&head->lwmtx, 1, SCE_NULL);
	if (res < 0) {
		return (res);
	}

	cnt = 0;
	for (hp = head->prim.prev; ; hp = hp->prev) {
		if (ptr != SCE_NULL) {
			if (_sceHeapIsPointerInBound(hp, ptr)) {
				unsigned int sz;

				sz = sceClibMspaceMallocUsableSize(ptr);
				sceKernelUnlockLwMutex(&head->lwmtx, 1);

				if (puiSize != SCE_NULL) {
					*puiSize = sz;
				}
				if (msplink != SCE_NULL) {
					*msplink = hp;
				}
				if (piBlockIndex != SCE_NULL) {
					if (hp == &(head->prim)) {
						cnt = 0;
					} else {
						cnt++;
					}
					*piBlockIndex = cnt;
				}
				return (1);
			}
		}
		cnt++;
		if (hp == &(head->prim)) {
			break;
		}
	}
	if (piBlockIndex != SCE_NULL) {
		*piBlockIndex = cnt;
	}
	if (ptr == SCE_NULL) {
		sceKernelUnlockLwMutex(&head->lwmtx, 1);
		return (0);
	}
	//J 不正なメモリブロックアドレスを指定された場合
	//J すでに縮小済み開放済みヒープだった場合も含む
	sceKernelUnlockLwMutex(&head->lwmtx, 1);
	return (SCE_HEAP_ERROR_INVALID_ID);
}


//J このヒープからアロケートされたメモリかどうか判定
//E Check whether it is a memory allocated from this heap.
int	sceHeapIsAllocatedHeapMemory(void *heap, void *ptr)
{
	int res;

	res = _sceHeapQueryAllocatedSize(heap, ptr);
	if (res > 0) {
		res = 1;
	}
	return (res);
}


//J このアロケートされたメモリのサイズを取得
int _sceHeapQueryAllocatedSize(void *heap, void *ptr)
{
	int res;
	unsigned int uiSize;

	if (ptr == SCE_NULL) {
		return (SCE_HEAP_ERROR_INVALID_POINTER);
	}
	res = _sceHeapQueryBlockInfo(heap, ptr, &uiSize, SCE_NULL, SCE_NULL);
	if (res < 0) {
		return (res);
	}
	res = (int)uiSize;
	return (res);
}


//J 何回目のヒープ拡張によって確保されたメモリか取得
int	_sceHeapQueryBlockIndex(void *heap, void *ptr)
{
	int res;
	int iTotalBlocks;

	res = _sceHeapQueryBlockInfo(heap, ptr, SCE_NULL, &iTotalBlocks, SCE_NULL);
	if (res < 0) {
		return (res);
	}
	res = iTotalBlocks;
	return res;
}


//J ヒープメモリの空きサイズを取得
//E Get size of empty heap memory
int	sceHeapGetTotalFreeSize(void *heap)
{
	SceHeapWorkInternal	*head;
	SceHeapMspaceLink	*hp;
	int		size;
	int	res;

	head = (SceHeapWorkInternal *)heap;

	if (head == SCE_NULL) {
		return (SCE_HEAP_ERROR_INVALID_ID);
	}
	if (head->magic != (SceUIntPtr)(head + 1)) {
		return (SCE_HEAP_ERROR_INVALID_ID);
	}

	//J 排他制御用の軽量ミューテックスをロック
	res = sceKernelLockLwMutex(&head->lwmtx, 1, SCE_NULL);
	if (res < 0) {
		return (res);
	}

	size = 0;
	for (hp = head->prim.next; ; hp = hp->next) {
		struct SceClibMspaceStats mspstat;

		sceClibMspaceMallocStats(hp->msp, &mspstat);
		size += mspstat.currentSystemSize - mspstat.currentInUseSize;
		if (hp == &(head->prim)) {
			break;
		}
	}
	sceKernelUnlockLwMutex(&head->lwmtx, 1);
	return (size);
}


#if USE_HEAPINFO

//J ヒープメモリの情報を取得
int	sceHeapGetMallinfo(void *heap, SceHeapMallinfo *pInfo)
{
	SceHeapWorkInternal	*head;
	int fordblks;
	int res;

	head = (SceHeapWorkInternal *)heap;

	if (head == SCE_NULL) {
		return (SCE_HEAP_ERROR_INVALID_ID);
	}
	if (head->magic != (SceUIntPtr)(head + 1)) {
		return (SCE_HEAP_ERROR_INVALID_ID);
	}

	//J 排他制御用の軽量ミューテックスをロック
	res = sceKernelLockLwMutex(&head->lwmtx, 1, SCE_NULL);
	if (res < 0) {
		return (res);
	}

	fordblks = sceHeapGetTotalFreeSize(heap);
	if (fordblks < 0) {
		sceKernelUnlockLwMutex(&head->lwmtx, 1);
		return (fordblks);
	}

	pInfo->arena    = head->info.arena;
	pInfo->ordblks  = head->info.ordblks;
	pInfo->smblks   = 0;
	pInfo->hblks    = head->info.hblks;
	pInfo->hblkhd   = head->info.arena;
	pInfo->usmblks  = 0;
	pInfo->fsmblks  = 0;
	pInfo->uordblks = head->info.arena - fordblks;
	pInfo->fordblks = fordblks;
	pInfo->keepcost = 0;

	sceKernelUnlockLwMutex(&head->lwmtx, 1);
	return (0);
}

#endif	/* USE_HEAPINFO */


/* Local variables: */
/* tab-width: 4 */
/* End: */
/* vi:set tabstop=4: */
