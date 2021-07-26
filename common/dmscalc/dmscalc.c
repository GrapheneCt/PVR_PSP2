/******************************************************************************
 * Name         : dmscalc.c
 *
 * Copyright    : 2006-2009 by Imagination Technologies Limited.
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
 * $Log: dmscalc.c $
 **************************************************************************/

#include "sgxdefs.h"
#include "sgxapi.h"
#include "pvr_debug.h"
#include "dmscalc.h"

#ifndef ALIGNCOUNT
#define ALIGNCOUNT(X, Y)	(((X) + ((Y) - 1U)) & ~((Y) - 1U))
#endif

#ifndef ALIGNCOUNTINBLOCKS
#define ALIGNCOUNTINBLOCKS(Size, Log2OfBlockSsize)	(((Size) + ((1UL << (Log2OfBlockSsize)) - 1)) >> (Log2OfBlockSsize))
#endif

#ifndef MIN
#define MIN(a,b) 			(((a) < (b)) ? (a) : (b))
#endif

#define EURASIA_VDMPDS_USEATTRIBUTESIZE_ALIGNSHIFTINREGISTERS	(EURASIA_VDMPDS_USEATTRIBUTESIZE_ALIGNSHIFT - 2)
#define EURASIA_VDMPDS_USEATTRIBUTESIZE_ALIGNINREGISTERS		(1UL << EURASIA_VDMPDS_USEATTRIBUTESIZE_ALIGNSHIFTINREGISTERS)

#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
#define EURASIA_UNIFIED_STORE_BANK_STRIDE 32
#else
#define EURASIA_UNIFIED_STORE_BANK_STRIDE 16
#endif


IMG_INTERNAL IMG_VOID CalculatePixelDMSInfo(PVRSRV_SGX_CLIENT_INFO *psSGXInfo,
											IMG_UINT32 ui32PrimaryAllocation,
											IMG_UINT32 ui32TempAllocation,
											IMG_UINT32 ui32SecondaryAllocation,
											IMG_UINT32 ui32ConcurrentSampleCount,
											IMG_UINT32 ui32ExtraOutputBuffCount,
											IMG_UINT32 *pui32DMSInfo,
											IMG_UINT32 *pui32NumInstances)
{
	IMG_UINT32 ui32DMSInfo = *pui32DMSInfo;
	IMG_UINT32 ui32MaxUSETaskSize, ui32MaxUSETaskSizeInChunks, ui32MaxUSETaskSizeInRegs;
	IMG_UINT32 ui32MaxPDSTaskSize, ui32MaxPDSTaskSizeInChunks, ui32MaxPDSTaskSizeInBlocks;
	IMG_UINT32 ui32PixelSize, ui32PAPixelSizeInRegs, ui32AttributesSizeInChunks, ui32PASizeInChunks;
	IMG_UINT32 ui32SAPixelSizeInChunks, ui32SASizeInChunks;
	IMG_UINT32 ui32InstanceCount = 0;
#if !defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
	IMG_UINT32 ui32AvailableTemps;
#endif
#if !defined(EURASIA_PDS_RESERVED_PIXEL_CHUNKS)
	IMG_UINT32 ui32SAVertexSizeInChunks;
#endif
	IMG_UINT32 ui32SAChunks;
	IMG_UINT32 ui32PixelSizeAlignSize = EURASIA_PDS_PIXELSIZE_ALIGNSIZE;

#if defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
	ui32MaxUSETaskSize = EURASIA_PDS_USETASKSIZE_MAXSIZE + 1;

	ui32PixelSize = ui32PrimaryAllocation + ui32TempAllocation;
#else /* defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS) */
	/*
		Calculate the maximum number of 2x2 blocks in a USE task according to the number of temporaries
	*/
	ui32AvailableTemps = psSGXInfo->ui32NumUSETemporaryRegisters / PVR_TEMPORARIES_SUBDIVISION;

	if (ui32TempAllocation > 0)
	{
		IMG_UINT32 ui32TempChunkSize = ALIGNCOUNT(ui32TempAllocation * PVR_BLOCK_SIZE_IN_PIXELS, EURASIA_PDS_CHUNK_SIZE);

		if (ui32AvailableTemps < ui32TempChunkSize)
		{
			ui32MaxUSETaskSize = 1;
		}
		else
		{
			ui32MaxUSETaskSize = MIN(ui32AvailableTemps / ui32TempChunkSize, EURASIA_PDS_USETASKSIZE_MAXSIZE + 1);
		}
	}
	else
	{
		ui32MaxUSETaskSize = EURASIA_PDS_USETASKSIZE_MAXSIZE + 1;
	}

	PVR_ASSERT(ui32MaxUSETaskSize > 0);

	ui32PixelSize = ui32PrimaryAllocation;

#endif /* #if defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS) */

	/*
		Multiply the pixel size by the amount of simultaneously allocated samples..
	*/
	PVR_ASSERT(ui32ConcurrentSampleCount > 0);
	ui32PixelSize *= ui32ConcurrentSampleCount;
	ui32PixelSizeAlignSize *= ui32ConcurrentSampleCount;

	/*
		Determine the primary attribute pixel size in registers.
		Round to PDS alloc granularity, then round to nearest 4, due to USSE stride granularity.
	*/
	ui32PixelSize = ALIGNCOUNT(ui32PixelSize, ui32PixelSizeAlignSize);
	ui32PAPixelSizeInRegs = ALIGNCOUNT(ui32PixelSize, 4);

	if (ui32PAPixelSizeInRegs > 0)
	{
		/*
			Reduce the maximum number of 2x2 blocks in a USE task according to the number of primary and secondary attributes
		*/

		/*
			Determine the number of chunks required for the secondary attributes
		*/
		ui32SAPixelSizeInChunks	= (ui32SecondaryAllocation + (EURASIA_PDS_CHUNK_SIZE - 1)) / EURASIA_PDS_CHUNK_SIZE;

#if defined(EURASIA_PDS_RESERVED_PIXEL_CHUNKS)
		ui32SASizeInChunks = ui32SAPixelSizeInChunks;

		/*
			Determine the number of chunks avaiable for the primary attributes
		*/
		ui32AttributesSizeInChunks	= EURASIA_PDS_RESERVED_PIXEL_CHUNKS;
#else
		ui32SAVertexSizeInChunks = (PVR_MAX_VS_SECONDARIES + (EURASIA_PDS_CHUNK_SIZE - 1)) / EURASIA_PDS_CHUNK_SIZE;
		ui32SASizeInChunks = ui32SAPixelSizeInChunks + ui32SAVertexSizeInChunks;

		/*
			Determine the number of chunks avaiable for the primary attributes
		*/
		ui32AttributesSizeInChunks	= psSGXInfo->ui32NumUSEAttributeRegisters / EURASIA_PDS_CHUNK_SIZE;
#endif
		
		/*
			Reserve space for the extra output buffers..
		*/
		ui32AttributesSizeInChunks -= ui32ExtraOutputBuffCount * EURASIA_OUTPUT_PARTITION_SIZE;
		
		/*
			Reserve space for the SAs..
		*/
		ui32PASizeInChunks			= ui32AttributesSizeInChunks - ui32SASizeInChunks;
		
		ui32InstanceCount			= (ui32PASizeInChunks * EURASIA_PDS_CHUNK_SIZE) / (PVR_BLOCK_SIZE_IN_PIXELS * ui32PAPixelSizeInRegs);

		/*
			Determine the maximum-sized USE task assuming that the primary and secondary attributes split the available space into subdivisions
		*/
		ui32MaxUSETaskSizeInChunks	= (ui32PASizeInChunks + (PVR_SECONDARY_SUBDIVISION_FOR_PIXELS - 1)) / PVR_SECONDARY_SUBDIVISION_FOR_PIXELS;

		/*
			Limit the maximum-sized USE task according to the number of primary attributes in the largest subdivision
		*/
		if (ui32MaxUSETaskSize * PVR_BLOCK_SIZE_IN_PIXELS * ui32PAPixelSizeInRegs > ui32MaxUSETaskSizeInChunks * EURASIA_PDS_CHUNK_SIZE)
		{
			ui32MaxUSETaskSize = (ui32MaxUSETaskSizeInChunks * EURASIA_PDS_CHUNK_SIZE) / (PVR_BLOCK_SIZE_IN_PIXELS * ui32PAPixelSizeInRegs);
		}

		/*
			Calculate the maximum number of 2x2 blocks in a PDS task according to the number of primary and secondary attributes
		*/

		/*
			Determine the number of chunks in the maximum-sized USE task
		*/
		ui32MaxUSETaskSizeInRegs = ui32MaxUSETaskSize * PVR_BLOCK_SIZE_IN_PIXELS * ui32PAPixelSizeInRegs;
		ui32MaxUSETaskSizeInChunks = (ui32MaxUSETaskSizeInRegs + (EURASIA_PDS_CHUNK_SIZE - 1)) / EURASIA_PDS_CHUNK_SIZE;

		/*
			Allow for the alignment gaps at the ends of two out of the three subdivisions
		*/
		ui32MaxPDSTaskSizeInChunks = ui32PASizeInChunks - ((PVR_SECONDARY_SUBDIVISION_FOR_PIXELS - 1) * (ui32MaxUSETaskSizeInChunks - 1));

		/*
			Determine the number of maximum-sized USE tasks
		*/
		ui32MaxPDSTaskSizeInBlocks = (ui32MaxPDSTaskSizeInChunks / ui32MaxUSETaskSizeInChunks) * ui32MaxUSETaskSize;

		/*
			Add the one smaller-sized USE task
		*/
		ui32MaxPDSTaskSizeInBlocks += 
			((ui32MaxPDSTaskSizeInChunks % ui32MaxUSETaskSizeInChunks) * EURASIA_PDS_CHUNK_SIZE) / (PVR_BLOCK_SIZE_IN_PIXELS * ui32PAPixelSizeInRegs);

		/*
			Limit the maximum number of 2x2 blocks in a PDS task to consume less than a 
			proportion of the primary attribute space (currently a quarter). Don't go below 1 2x2 block.
			*/
		PVR_ASSERT(ui32MaxPDSTaskSizeInBlocks > 0);
		
		if((ui32MaxPDSTaskSizeInBlocks * PVR_BLOCK_SIZE_IN_PIXELS * ui32PAPixelSizeInRegs) > 
			((ui32PASizeInChunks * EURASIA_PDS_CHUNK_SIZE) / PVR_ATTRIBUTES_DM_SUBDIVISION))
		{
			if(((ui32PASizeInChunks * EURASIA_PDS_CHUNK_SIZE) / PVR_ATTRIBUTES_DM_SUBDIVISION) > (PVR_BLOCK_SIZE_IN_PIXELS * ui32PAPixelSizeInRegs))
			{
				ui32MaxPDSTaskSizeInBlocks = 
					((ui32PASizeInChunks * EURASIA_PDS_CHUNK_SIZE) / PVR_ATTRIBUTES_DM_SUBDIVISION) / (PVR_BLOCK_SIZE_IN_PIXELS * ui32PAPixelSizeInRegs);
			}
			else
			{
				ui32MaxPDSTaskSizeInBlocks = 1;
			}
		}

		/*
			Limit the size of the PDS task
		*/
		if (ui32MaxPDSTaskSizeInBlocks > EURASIA_PDS_PDSTASKSIZE_MAX)
		{
			ui32MaxPDSTaskSizeInBlocks = EURASIA_PDS_PDSTASKSIZE_MAX;
		}

		ui32MaxPDSTaskSize = ui32MaxPDSTaskSizeInBlocks;
	}
	else
	{
		ui32MaxPDSTaskSize = EURASIA_PDS_PDSTASKSIZE_MAX;
	}
	
	PVR_ASSERT(ui32MaxUSETaskSize > 0);
	PVR_ASSERT(ui32MaxPDSTaskSize > 0);

	/*
		Check that the PDS task size and USE task size have the same ratio.
	*/
	if (ui32MaxPDSTaskSize > EURASIA_PDS_USESIZETOPDSSIZE_MAX_RATIO * ui32MaxUSETaskSize)
	{
		ui32MaxPDSTaskSize = EURASIA_PDS_USESIZETOPDSSIZE_MAX_RATIO * ui32MaxUSETaskSize;
	}

	/*
	 	EURASIA_PDS_SECATTRSIZE_ALIGNSHIFT is in bytes, so we need to align
	 	correctly
	 */
	ui32SAChunks = ALIGNCOUNTINBLOCKS(ui32SecondaryAllocation, (EURASIA_PDS_SECATTRSIZE_ALIGNSHIFT-2));

	/*
		Set up the fields in the pixel shader data master information words.
	*/
	ui32DMSInfo &= EURASIA_PDS_PDSTASKSIZE_CLRMSK &
		EURASIA_PDS_USETASKSIZE_CLRMSK &
		EURASIA_PDS_PIXELSIZE_CLRMSK &
		EURASIA_PDS_SECATTRSIZE_CLRMSK;
	ui32DMSInfo |= ((ui32MaxPDSTaskSize << EURASIA_PDS_PDSTASKSIZE_SHIFT) & ~EURASIA_PDS_PDSTASKSIZE_CLRMSK) | 
		((ui32MaxUSETaskSize - 1) << EURASIA_PDS_USETASKSIZE_SHIFT) |
		(ui32SAChunks << EURASIA_PDS_SECATTRSIZE_SHIFT) |
		((ui32PixelSize >> EURASIA_PDS_PIXELSIZE_ALIGNSHIFT) << EURASIA_PDS_PIXELSIZE_SHIFT);
	
	*pui32DMSInfo = ui32DMSInfo;
	*pui32NumInstances = ui32InstanceCount;
}

#if defined(FIX_HW_BRN_25161)

static IMG_VOID CalcVertexAdvance(	IMG_UINT32 *pui32OutputVerticesPerPartition,
									IMG_UINT32 *pui32OutputVertexAdvance)
{
	IMG_UINT32 ui32NumVertices = 0, ui32VertexAdvance = 0;
#if 0
	IMG_BOOL bEquationSatisfied = IMG_FALSE;

	/* Adjust number of vertices per partition and vertex advance until the 
	   following equation is satisfied:
	
	   (128 mod (cam_size + vertex_per_partition * advance_granauality) ) == 0 */

	for(ui32NumVertices = *pui32OutputVerticesPerPartition; ui32NumVertices > 0; ui32NumVertices--)
	{
		for(ui32VertexAdvance=1; ui32VertexAdvance<=MIN(ui32NumVertices, 4); ui32VertexAdvance++)
		{
			IMG_UINT32 x;

			x = MIN(EURASIA_VDMPDS_PARTSIZE_MAX, ui32NumVertices * 4)  + ui32NumVertices * ui32VertexAdvance;
			
			/* Check if condition is satisfied */
			if((128 % x) == 0)
			{
				bEquationSatisfied = IMG_TRUE;
				break;
			}
		}

		if(bEquationSatisfied)
		{
			break;
		}
	}

	if (!bEquationSatisfied)
	{
		ui32NumVertices = 1;
		ui32VertexAdvance = 1;
	}

#else
	/* We are using a lookup, since the number of solutions is actually very small */
	switch(*pui32OutputVerticesPerPartition)
	{
		case 16:
		case 15:
		case 14:
		case 13:
		case 12:
		case 11:
		case 10:
		{
			ui32NumVertices = 10;
			ui32VertexAdvance = 2;
			break;
		}
		case 9:
		case 8:
		case 7:
		case 6:
		case 5:
		case 4:
		{
			ui32NumVertices = 4;
			ui32VertexAdvance = 1;
			break;
		}
		case 3:
		case 2:
		case 1:
		{
			ui32NumVertices = 1;
			ui32VertexAdvance = 1;
			break;
		}
		default:
			//PVR_DPF((PVR_DBG_ERROR,"CalcVertexAdvance: invalid number of vertices per partition: %lu",ui32NumVertices));
			break;
	}
#endif

	*pui32OutputVerticesPerPartition = ui32NumVertices;
	*pui32OutputVertexAdvance = ui32VertexAdvance-1;
}

#endif

/*****************************************************************************
 Function Name	: GetMaxInstancesInAttributes
 Inputs			: gc						- pointer to the context
				  psShader					- pointer to current shader
				  ui32InputVertexSizeInRegisters	- Attributes required for each instance.
 Outputs		: none
 Returns		: none
 Description	: The maximum number of instances.
*****************************************************************************/
static IMG_UINT32 GetMaxInstancesInAttributes(IMG_UINT32 ui32NumAttribs, IMG_UINT32 ui32InputVertexSizeInRegisters, IMG_UINT32 ui32SecondaryAllocation)
{
	IMG_UINT32 ui32SecondaryAttributeSizeInRegisters;
	IMG_UINT32 ui32PrimaryAttributeAllocation;
	IMG_UINT32 ui32MaxVerticesInPrimaryAttributes;

	/*
		Round the vertex size up for the units of the field in the index list command.
	*/
	ui32InputVertexSizeInRegisters = ALIGNCOUNT(ui32InputVertexSizeInRegisters, EURASIA_VDMPDS_USEATTRIBUTESIZE_ALIGNINREGISTERS);

#if defined(EURASIA_PDS_RESERVED_PIXEL_CHUNKS)
	/*
		For vertices the HW reserves some more to make sure that pixel tasks can run...
	*/
	ui32NumAttribs -= (EURASIA_PDS_RESERVED_PIXEL_CHUNKS * EURASIA_PDS_CHUNK_SIZE);
#endif

	/*
		Round the secondary allocation up to the allocation granularity.
	*/
	ui32SecondaryAttributeSizeInRegisters = ALIGNCOUNT(ui32SecondaryAllocation, EURASIA_PDS_CHUNK_SIZE);

	/*
		Include the maximum pixel secondary allocation rounded up to the allocation granularity.
	*/
	ui32SecondaryAttributeSizeInRegisters += ALIGNCOUNT(PVR_MAX_PS_SECONDARIES, EURASIA_PDS_CHUNK_SIZE);


	/*
		Calculate how much space remains for primary allocations.
	*/
	ui32PrimaryAttributeAllocation = ui32NumAttribs - ui32SecondaryAttributeSizeInRegisters;

	/*
		The pixel and vertex secondary attributes might lie in the middle of the overall attribute space so divide by three
		to take into account fragmentation.
	*/
	ui32PrimaryAttributeAllocation	= (ui32PrimaryAttributeAllocation + (PVR_SECONDARY_SUBDIVISION_FOR_VERTICES - 1)) / PVR_SECONDARY_SUBDIVISION_FOR_VERTICES;

	/*
		Calculate how many instances we can fit into the available attributes.
	*/
	ui32MaxVerticesInPrimaryAttributes = ui32PrimaryAttributeAllocation / ui32InputVertexSizeInRegisters;

	return MIN(ui32MaxVerticesInPrimaryAttributes, EURASIA_VDMPDS_MAXINPUTINSTANCES_MAX);
}

IMG_INTERNAL IMG_VOID CalculateVertexDMSInfo(PVRSRV_SGX_CLIENT_INFO *psSGXInfo, 
											 IMG_UINT32 ui32PrimaryAllocation, 
											 IMG_UINT32 ui32TempAllocation, 
											 IMG_UINT32 ui32SecondaryAllocation, 
											 IMG_UINT32 ui32OutputVertexSize,
											 IMG_UINT32 *pui32DMSIndexList2,
											 IMG_UINT32 *pui32DMSIndexList4,
											 IMG_UINT32 *pui32DMSIndexList5)
{
	IMG_UINT32 ui32MaxVerticesInOutputPartition, ui32VertexAdvance;
	IMG_UINT32 ui32InputVertexSizeInDWORDs, ui32MaxInputInstances;
	IMG_UINT32 ui32MaxVerticesInPrimaryAttributes, ui32OutputVertexSizeIn128BitWORDs;
#if !defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
	IMG_UINT32 ui32MaxVerticesInTemporaries, ui32NumTemps;
#endif
#if defined(SGX_FEATURE_VCB)
	IMG_UINT32 ui32Temp;
#endif

#if defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)

	/*
		Determine the maximum number of vertices in the primary attributes
	*/
	ui32InputVertexSizeInDWORDs = ui32PrimaryAllocation + ui32TempAllocation;

	/* Increase the PA+Temp allocation by the alignment amount if the allocation is a multiple of 1/2 the bank stride */
	if((ALIGNCOUNT(ui32InputVertexSizeInDWORDs, EURASIA_VDMPDS_USEATTRIBUTESIZE_ALIGNINREGISTERS) % (EURASIA_UNIFIED_STORE_BANK_STRIDE>>1)) == 0)
	{
		ui32InputVertexSizeInDWORDs += EURASIA_VDMPDS_USEATTRIBUTESIZE_ALIGNINREGISTERS;
	}

	ui32MaxVerticesInPrimaryAttributes = GetMaxInstancesInAttributes(psSGXInfo->ui32NumUSEAttributeRegisters, ui32InputVertexSizeInDWORDs, ui32SecondaryAllocation);

	/*
		Determine the maximum number of input instances
	*/
	ui32MaxInputInstances = ui32MaxVerticesInPrimaryAttributes;

#else

	/*
		Determine the maximum number of vertices in the primary attributes
	*/
	ui32InputVertexSizeInDWORDs = ui32PrimaryAllocation;
	ui32MaxVerticesInPrimaryAttributes = GetMaxInstancesInAttributes(psSGXInfo->ui32NumUSEAttributeRegisters, ui32InputVertexSizeInDWORDs, ui32SecondaryAllocation);
	
	/*
		Determine the maximum number of vertices in the temporaries
	*/
	ui32NumTemps = (psSGXInfo->ui32NumUSETemporaryRegisters / PVR_TEMPORARIES_SUBDIVISION);

	if (ui32TempAllocation * EURASIA_VDMPDS_MAXINPUTINSTANCES_MAX <= ui32NumTemps)
	{
		ui32MaxVerticesInTemporaries = EURASIA_VDMPDS_MAXINPUTINSTANCES_MAX;
	}
	else
	{
		ui32MaxVerticesInTemporaries = ui32NumTemps / ui32TempAllocation;
	}
	/*
		Determine the maximum number of input instances
	*/
	if (ui32MaxVerticesInPrimaryAttributes < ui32MaxVerticesInTemporaries)
	{
		ui32MaxInputInstances = ui32MaxVerticesInPrimaryAttributes;
	}
	else
	{
		ui32MaxInputInstances = ui32MaxVerticesInTemporaries;
	}
#endif /* #if defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS) */

	
	/*
		Determine the maximum number of vertices in output partition
	*/

	ui32OutputVertexSize = ALIGNCOUNT(ui32OutputVertexSize, EURASIA_VDMPDS_VERTEXSIZE_ALIGN);

	ui32OutputVertexSizeIn128BitWORDs = ui32OutputVertexSize >> EURASIA_VDMPDS_VERTEXSIZE_ALIGNSHIFT;


#if defined(SGX_FEATURE_VCB)

	/* Simple geometry processing always uses double partitions */
	if (ui32OutputVertexSize * EURASIA_VDMPDS_PARTSIZE_MAX <= (EURASIA_OUTPUT_PARTITION_SIZE*2))
	{
		ui32MaxVerticesInOutputPartition = EURASIA_VDMPDS_PARTSIZE_MAX;
	}
	else
	{
		ui32MaxVerticesInOutputPartition = (EURASIA_OUTPUT_PARTITION_SIZE*2) / ui32OutputVertexSize;
	}

	/* Extra calculations to take into account the VCB */
	ui32Temp = ALIGNCOUNT(ui32InputVertexSizeInDWORDs,  EURASIA_VDMPDS_USEATTRIBUTESIZE_ALIGNINREGISTERS);
	ui32Temp = ALIGNCOUNT(ui32Temp * ui32MaxInputInstances, EURASIA_PDS_CHUNK_SIZE-1);

	ui32SecondaryAllocation = ALIGNCOUNT(ui32SecondaryAllocation, EURASIA_PDS_CHUNK_SIZE);

	ui32MaxVerticesInOutputPartition = MIN(ui32MaxVerticesInOutputPartition, 
										   (((psSGXInfo->ui32NumUSEAttributeRegisters 
											  - ui32SecondaryAllocation)
											 - (ui32Temp - EURASIA_PDS_CHUNK_SIZE))
											* ui32MaxInputInstances)
										   / (ui32Temp * 2));

#else /* defined(SGX_FEATURE_VCB) */

	if (ui32OutputVertexSize * EURASIA_VDMPDS_PARTSIZE_MAX <= EURASIA_OUTPUT_PARTITION_SIZE)
	{
		ui32MaxVerticesInOutputPartition = EURASIA_VDMPDS_PARTSIZE_MAX;
	}
	else
	{
		ui32MaxVerticesInOutputPartition = EURASIA_OUTPUT_PARTITION_SIZE / ui32OutputVertexSize;
	}

#endif /* defined(SGX_FEATURE_VCB) */

#if defined(FIX_HW_BRN_28249)
 	/*
		Reduce number of vertices per partition before calculating the vertex fifo advance
	*/
	if(ui32MaxVerticesInOutputPartition > ui32MaxInputInstances)
	{
		ui32MaxVerticesInOutputPartition = ui32MaxInputInstances;
	}
#endif
	
	/*
		Determine the vertex fifo advance.
	*/
#if defined(FIX_HW_BRN_25161)
	CalcVertexAdvance(&ui32MaxVerticesInOutputPartition, &ui32VertexAdvance);
#else
	ui32VertexAdvance = MIN(ui32MaxVerticesInOutputPartition - 1, 3);
#endif


	*pui32DMSIndexList2 = (ui32MaxInputInstances - 1) << EURASIA_VDMPDS_MAXINPUTINSTANCES_SHIFT;

	/* Index List 4 */	
	*pui32DMSIndexList4	= (ui32MaxVerticesInOutputPartition - 1) << EURASIA_VDMPDS_PARTSIZE_SHIFT;

	/* Index List 5 */
	*pui32DMSIndexList5	= 
		(ui32OutputVertexSizeIn128BitWORDs << EURASIA_VDMPDS_VERTEXSIZE_SHIFT)				|
		(ui32VertexAdvance << EURASIA_VDMPDS_VTXADVANCE_SHIFT)								|
		(ALIGNCOUNTINBLOCKS((ui32InputVertexSizeInDWORDs << 2), EURASIA_VDMPDS_USEATTRIBUTESIZE_ALIGNSHIFT) << EURASIA_VDMPDS_USEATTRIBUTESIZE_SHIFT);
}
