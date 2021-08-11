/******************************************************************************
 * Name         : gles2errata.c
 *
 * Copyright    : 2006-2007 by Imagination Technologies Limited.
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
 * $Log: gles2errata.c $
 **************************************************************************/

#include "context.h"
#include "sgxpdsdefs.h"
#include "usegles2.h"
#include "gles2errata.h"

#if defined(FIX_HW_BRN_26922)
/***********************************************************************************
 Function Name      : AllocateBRN26922Mem
 Inputs             : gc
 Outputs            : 
 Returns            : Success
 Description        : Allocated memory required for errata
************************************************************************************/
IMG_INTERNAL IMG_BOOL AllocateBRN26922Mem(GLES2Context *gc)
{
	if(GLES2ALLOCDEVICEMEM(gc->ps3DDevData, 
							gc->psSysContext->hGeneralHeap, 
							PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MEM_NO_SYNCOBJ,
							2,
							(1 << EURASIA_PIXELBE_FBADDR_ALIGNSHIFT),
							&gc->sPrim.sBRN26922State.psBRN26922MemInfo) != PVRSRV_OK)

	{
		PVR_DPF((PVR_DBG_FATAL,"AllocateBRN26922Mem: Failed to allocate memory for BRN26922 fix"));

		return IMG_FALSE;
	}

#if defined(PDUMP)
	gc->sPrim.sBRN26922State.bDump = IMG_TRUE;
#endif		

	return IMG_TRUE;
}

/***********************************************************************************
 Function Name      : FreeBRN26922Mem
 Inputs             : gc
 Outputs            : 
 Returns            : 
 Description        : Frees memory required for errata
************************************************************************************/
IMG_INTERNAL IMG_VOID FreeBRN26922Mem(GLES2Context *gc)
{
	GLES2FREEDEVICEMEM(gc->ps3DDevData, gc->sPrim.sBRN26922State.psBRN26922MemInfo);
}

#endif /* defined(FIX_HW_BRN_26922) */
/******************************************************************************
 End of file (gles2errata.c)
******************************************************************************/

