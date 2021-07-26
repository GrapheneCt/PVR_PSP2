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
 * $Log: dmscalc.h $
 **************************************************************************/

IMG_VOID CalculatePixelDMSInfo(PVRSRV_SGX_CLIENT_INFO *psSGXInfo,
								IMG_UINT32 ui32PrimaryAllocation,
								IMG_UINT32 ui32TempAllocation,
								IMG_UINT32 ui32SecondaryAllocation,
								IMG_UINT32 ui32ConcurrentSampleCount,
								IMG_UINT32 ui32ExtraOutputBuffCount,
								IMG_UINT32 *pui32DMSInfo,
								IMG_UINT32 *pui32NumInstances);


IMG_VOID CalculateVertexDMSInfo(PVRSRV_SGX_CLIENT_INFO *psSGXInfo, 
								IMG_UINT32 ui32PrimaryAllocation, 
								IMG_UINT32 ui32TempAllocation, 
								IMG_UINT32 ui32SecondaryAllocation, 
								IMG_UINT32 ui32OutputVertexSize,
								IMG_UINT32 *pui32DMSIndexList2,
								IMG_UINT32 *pui32DMSIndexList4,
								IMG_UINT32 *pui32DMSIndexList5);

#define PVR_ATTRIBUTES_SUBDIVISION		1
#define PVR_TEMPORARIES_SUBDIVISION		2

#if defined(EURASIA_PDS_RESERVED_PIXEL_CHUNKS)
#define PVR_SECONDARY_SUBDIVISION_FOR_PIXELS		2
#else
#define PVR_SECONDARY_SUBDIVISION_FOR_PIXELS		3
#endif
#define PVR_SECONDARY_SUBDIVISION_FOR_VERTICES		3
#define PVR_ATTRIBUTES_DM_SUBDIVISION	4

#define PVR_BLOCK_SIZE_IN_PIXELS		4

#if(EURASIA_USE_NUM_UNIFIED_REGISTERS > 2048)
#define PVR_MAX_VS_SECONDARIES			512
#else
#define PVR_MAX_VS_SECONDARIES			300
#endif

#define PVR_MAX_PS_SECONDARIES			128

