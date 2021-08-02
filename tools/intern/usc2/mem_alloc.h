/******************************************************************************
 * Name         : mem_alloc.h
 * Created      : Aug 2010
 *
 * Copyright    : 2010 by Imagination Technologies Limited. All rights reserved.
 *              : No part of this software, either material or conceptual 
 *              : may be copied or distributed, transmitted, transcribed,
 *              : stored in a retrieval system or translated into any 
 *              : human or computer language in any form by any means,
 *              : electronic, mechanical, manual or other-wise, or 
 *              : disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited,
 *              : Industrial Estate, King's Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Modifications:-
 * $Log: mem_alloc.h $
 *****************************************************************************/

#ifndef MEM_ALLOC
#define MEM_ALLOC

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include "img_types.h"

#define HUGE_CHUNK_SIZE  				(512*1024)
#define SMALL_CHUNK_SIZE 				(32)
#define SMALL_CHUNKS_PER_HUGE_CHUNK 	(HUGE_CHUNK_SIZE/SMALL_CHUNK_SIZE)
#define LOG2_SMALL_CHUNK_SIZE			(5)

#define BITS_NEEDED_FOR_SMALL_CHUNKS 	(HUGE_CHUNK_SIZE/SMALL_CHUNK_SIZE)
#define BITS_PER_BYTE 					(8)
#define BITS_IN_BITFIELD_PACKET 		(BITS_PER_BYTE * sizeof(IMG_UINT32))
#define BITFIELD_PACKET_COUNT 			(BITS_NEEDED_FOR_SMALL_CHUNKS/BITS_IN_BITFIELD_PACKET)

#define TREE_MAX_WIDTH 			(16)
#define NUM_SLICES                      (TREE_MAX_WIDTH)
#define SLICE_SIZE 				(HUGE_CHUNK_SIZE / NUM_SLICES)
#define SMALL_CHUNKS_PER_SLICE 	(SLICE_SIZE / SMALL_CHUNK_SIZE)

typedef struct _SLICE_CACHE_
{
/*this holds the slice index, meant for the asMemoryPool item in the HUGE_CHUNK structure*/
  IMG_UINT32 uSliceNum;

/*the number of available small chunks in this slice*/
  IMG_UINT32 uSmallChunksLeft;

/*index of every small chunk that is still available in this slice
stores the absolute chunk index. IE, this is wrt to the psHugeChunk->asMemoryPool
NOTE: this is a conservative cache, and is not updated as chunks from this slice are freed. 
there may a few chunks in this slice that are free but are not recorded here*/
  IMG_UINT32 auFreeSmallChunksInThisSlice[SMALL_CHUNKS_PER_SLICE];
}SLICE_CACHE, *PSLICE_CACHE;





/* this structure is being defined purely to define a type,  */
/* and simplify pointer arithementic later on */
typedef struct _SMALL_CHUNK_
{
  char buffer[SMALL_CHUNK_SIZE];
}SMALL_CHUNK,*PSMALL_CHUNK;






typedef struct _HUGE_CHUNK_
{
/* the actual huge chunk  */
  SMALL_CHUNK asMemoryPool[SMALL_CHUNKS_PER_HUGE_CHUNK];

/* the bitfield to keep track of occupancy 
	popcount of a single uint gives you how many occupied chunks are there in that uint
	0 means small chunk is free, 
	1 means small chunk is occupied*/
  IMG_UINT32  auSmallChunkOccupancy[BITFIELD_PACKET_COUNT];

/* to keep track of how many small chunks have been allocated.  
 this is also used to determine wheter phase one of alocation is over.  */
  IMG_UINT32   uSmallChunksAllocatedInPhaseOne;

/* the partial tree to speed up search throught the bitfield */
  IMG_UINT32   auChunksFreeL0[1];
  IMG_UINT32   auChunksFreeL1[2];
  IMG_UINT32   auChunksFreeL2[4];
  IMG_UINT32   auChunksFreeL3[8];
  IMG_UINT32   auChunksFreeL4[16];
  PSLICE_CACHE psSliceCache;
}HUGE_CHUNK,*PHUGE_CHUNK;



#define NUM_HUGE_CHUNKS 20

typedef struct _MEM_MANAGER_
{
PHUGE_CHUNK apsMemPools[NUM_HUGE_CHUNKS];
PHUGE_CHUNK psLastHugeChunkUsed;
}MEM_MANAGER, *PMEM_MANAGER;




/*these two functions are exported outside to USC*/
IMG_PVOID FastAlloc(const IMG_PVOID);

IMG_BOOL FastFree(const IMG_PVOID, const IMG_PVOID pvPtr);



/*these two functions are used at setup creation and tear down time*/
IMG_VOID MemManagerInit(PMEM_MANAGER );

IMG_VOID MemManagerClose(IMG_PVOID );

#ifdef __cplusplus
}
#endif
#endif
