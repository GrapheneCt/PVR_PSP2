/*!****************************************************************************
@File           sgxutils_client.c

@Title          Device specific utility routines

@Author         Imagination Technologies

@Date           03/02/06

@Copyright      Copyright 2006-2009 by Imagination Technologies Limited.
                All rights reserved. No part of this software, either material
                or conceptual may be copied or distributed, transmitted,
                transcribed, stored in a retrieval system or translated into
                any human or computer language in any form by any means,
                electronic, mechanical, manual or otherwise, or disclosed
                to third parties without the express written permission of
                Imagination Technologies Limited, Home Park Estate,
                Kings Langley, Hertfordshire, WD4 8LZ, U.K.

@Platform       Generic

@Description    Device specific functions

@DoxygenVer

******************************************************************************/

/******************************************************************************
Modifications :-
$Log: sgxutils_client.c $

******************************************************************************/
#include <stddef.h>

#include "img_defs.h"
#include "services.h"
#include "sgxapi.h"
#include "sgxinfo.h"
#include "sgxinfo_client.h"
#include "sgxutils_client.h"
#include "pvr_debug.h"
#include "pdump_um.h"
#include "sysinfo.h"
#include "sgxtransfer_client.h"
#include "osfunc_client.h"
#include "sgx_bridge_um.h"
#include "sgx_mkif_client.h"

/*****************************************************************************
 FUNCTION	: CreateCCB

 PURPOSE	: Creates a circular buffer to be used for TA CCB commands

 PARAMETERS	:

 RETURNS	: PVRSRV_ERROR
*****************************************************************************/
IMG_INTERNAL PVRSRV_ERROR CreateCCB(PVRSRV_DEV_DATA *psDevData,
							    IMG_UINT32			ui32CCBSize,
							    IMG_UINT32			ui32AllocGran,	/* PRQA S 3195 */ /* always overwritten in code */
							    IMG_UINT32			ui32OverrunSize,
#if defined (SUPPORT_SID_INTERFACE)
							    IMG_SID				hDevMemHeap,
#else
							    IMG_HANDLE			hDevMemHeap,
#endif
							    SGX_CLIENT_CCB	**ppsCCB)
{
	SGX_CLIENT_CCB				*psCCB;
	PVRSRV_ERROR				eError;

	eError = PVRSRV_OK;

	psCCB =  PVRSRVAllocUserModeMem(sizeof(SGX_CLIENT_CCB));

	if(!psCCB)
	{
		PVR_DPF((PVR_DBG_ERROR,"ERROR - Failed to alloc host mem for TA CCB!"));
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto ErrorExit;
	}

	/* Setup CCB struct */
	psCCB->psCCBClientMemInfo = IMG_NULL;
	psCCB->psCCBCtlClientMemInfo = IMG_NULL;
	psCCB->pui32CCBLinAddr = IMG_NULL;
	psCCB->pui32WriteOffset = IMG_NULL;
	psCCB->pui32ReadOffset = IMG_NULL;

	#ifdef PDUMP
	psCCB->ui32CCBDumpWOff = 0;
	#endif

	/* Try to reduce the number of page breaks. To do this, see if the size is 
		less than a page if so round it up to power of 2, if greater alloc on a page */
	if ( ui32CCBSize < 0x1000UL )
	{
		IMG_UINT32	i, ui32PowOfTwo;

		ui32PowOfTwo = 0x1000UL;

		for (i = 12; i > 0; i--)
		{
			if (ui32CCBSize & ui32PowOfTwo)
			{
				break;
			}
	
			ui32PowOfTwo >>= 1;
		}
	
		if (ui32CCBSize & (ui32PowOfTwo - 1))
		{
			ui32PowOfTwo <<= 1;
		}
	
		ui32AllocGran = ui32PowOfTwo;
	}
	else
	{
		ui32AllocGran = 0x1000UL;
	}

	/* Try and allocate the actual CCB memory (must overallocate due to simplified rounding code) */
	PDUMPCOMMENT(psDevData->psConnection, "Allocate Client CCB", IMG_TRUE);
	if (PVRSRVAllocDeviceMem(	psDevData,
								hDevMemHeap,
								PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MEM_EDM_PROTECT | PVRSRV_MEM_NO_SYNCOBJ | PVRSRV_MEM_CACHE_CONSISTENT,
								ui32CCBSize + ui32OverrunSize,
								ui32AllocGran,
								&psCCB->psCCBClientMemInfo) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"ERROR - Failed to alloc mem for CCB!"));

		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto ErrorExit;
	}

	psCCB->pui32CCBLinAddr = psCCB->psCCBClientMemInfo->pvLinAddr;
	psCCB->sCCBDevAddr = psCCB->psCCBClientMemInfo->sDevVAddr;
	psCCB->ui32Size = ui32CCBSize;
	psCCB->ui32AllocGran = ui32AllocGran;

	/* Try and allocate CCB control struct */
	PDUMPCOMMENT(psDevData->psConnection, "Allocate Client CCB Control", IMG_TRUE);
	if (PVRSRVAllocDeviceMem(	psDevData,
								hDevMemHeap,
								PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MEM_EDM_PROTECT | PVRSRV_MEM_NO_SYNCOBJ | PVRSRV_MEM_CACHE_CONSISTENT,
								sizeof(PVRSRV_SGX_CCB_CTL),
								32,
								&psCCB->psCCBCtlClientMemInfo) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"ERROR - Failed to alloc mem for CCB control struct!"));

		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto ErrorExit;
	}
	/* Initialise read/write offset */
    PVRSRVMemSet(psCCB->psCCBCtlClientMemInfo->pvLinAddr, 0, sizeof(PVRSRV_SGX_CCB_CTL));

// PRQA S 3305 10
// PRQA S 5120 9
	/* Pull out pointers to read/write offsets */
	psCCB->pui32WriteOffset =
        (IMG_UINT32 *)(((IMG_UINT8*)psCCB->psCCBCtlClientMemInfo->pvLinAddr)
        + offsetof(PVRSRV_SGX_CCB_CTL, ui32WriteOffset));

	psCCB->pui32ReadOffset = 
        (IMG_UINT32 *)(((IMG_UINT8*)psCCB->psCCBCtlClientMemInfo->pvLinAddr)
        + offsetof(PVRSRV_SGX_CCB_CTL, ui32ReadOffset));

	PDUMPMEM(psDevData->psConnection,
			 IMG_NULL,
			 psCCB->psCCBCtlClientMemInfo,
			 0,
			 sizeof(PVRSRV_SGX_CCB_CTL),
			 PDUMP_FLAGS_CONTINUOUS);

	/* Save reference to newly created CCB */
	*ppsCCB = psCCB;

	return eError;

ErrorExit:

	/* Get rid of anything allocated */

	if (psCCB)
	{
		if (psCCB->psCCBClientMemInfo)
		{
			PVRSRVFreeDeviceMem(psDevData, psCCB->psCCBClientMemInfo);
		}

		if (psCCB->psCCBCtlClientMemInfo)
		{
			PVRSRVFreeDeviceMem(psDevData, psCCB->psCCBCtlClientMemInfo);
		}

		PVRSRVFreeUserModeMem(psCCB);
	}

	return eError;
}

/*****************************************************************************
 FUNCTION	: DestroyCCB

 PURPOSE	:

 PARAMETERS	:

 RETURNS	: None
*****************************************************************************/
IMG_INTERNAL IMG_VOID DestroyCCB(PVRSRV_DEV_DATA *psDevData,
								SGX_CLIENT_CCB *psCCB)
{
	PVRSRVFreeDeviceMem(psDevData, psCCB->psCCBClientMemInfo);

	PVRSRVFreeDeviceMem(psDevData, psCCB->psCCBCtlClientMemInfo);

	if (psCCB)
	{
        PVRSRVFreeUserModeMem(psCCB);
	}
}

/******************************************************************************
 FUNCTION	: SGXAcquireCCB

 PURPOSE	: Attempts to obtain access to whatever...

 PARAMETERS	: psCCB		- the CCB
			  ui32CmdSize	- How much space is required

 RETURNS	: Address of space if available, IMG_NULL otherwise
******************************************************************************/
#if defined (SUPPORT_SID_INTERFACE)
IMG_INTERNAL IMG_PVOID SGXAcquireCCB(PVRSRV_DEV_DATA *psDevData, SGX_CLIENT_CCB *psCCB, IMG_UINT32 ui32CmdSize, IMG_EVENTSID hOSEvent)
#else
IMG_INTERNAL IMG_PVOID SGXAcquireCCB(PVRSRV_DEV_DATA *psDevData, SGX_CLIENT_CCB *psCCB, IMG_UINT32 ui32CmdSize, IMG_HANDLE hOSEvent)
#endif
{
	IMG_UINT32 uiStart;

	if(GET_CCB_SPACE(*psCCB->pui32WriteOffset, *psCCB->pui32ReadOffset, psCCB->ui32Size) > ui32CmdSize)
	{
		return (IMG_PVOID)((IMG_UINTPTR_T)psCCB->psCCBClientMemInfo->pvLinAddr + *psCCB->pui32WriteOffset);
	}

	uiStart = PVRSRVClockus();

	do
	{
		if (hOSEvent)
		{
			if(PVRSRVEventObjectWait(psDevData->psConnection,
										hOSEvent) != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_MESSAGE, "SGXAcquireCCB: PVRSRVEventObjectWait failed"));
			}
		}
		else
		{
			PVRSRVWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
		}

		if(GET_CCB_SPACE(*psCCB->pui32WriteOffset, *psCCB->pui32ReadOffset, psCCB->ui32Size) > ui32CmdSize)
		{
			return (IMG_PVOID)((IMG_UINTPTR_T)psCCB->psCCBClientMemInfo->pvLinAddr + *psCCB->pui32WriteOffset);
		}

	} while ((PVRSRVClockus() - uiStart) < MAX_HW_TIME_US);

	/* Time out on waiting for CCB space */
	return IMG_NULL;
}

/*****************************************************************************
FUNCTION	: SGXSetContextPriority

PURPOSE	: sets the priority of render and transfer contexts

PARAMETERS	: psDevData, ePriority, hRenderContext, hTransferContext

RETURNS	: PVRSRV_ERROR
 *****************************************************************************/
IMG_EXPORT PVRSRV_ERROR IMG_CALLCONV 
SGXSetContextPriority(PVRSRV_DEV_DATA *psDevData,
						SGX_CONTEXT_PRIORITY *pePriority,
						IMG_HANDLE hRenderContext,
						IMG_HANDLE hTransferContext)
{
#if defined(SUPPORT_SGX_PRIORITY_SCHEDULING)
	IMG_UINT32 ui32Priority;
	SGX_CONTEXT_PRIORITY ePriority;
    PVRSRV_ERROR eError;

	if(psDevData == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,"SGXSetContextPriority: psDevData is NULL"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	if(pePriority == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,"SGXSetContextPriority: pePriority is NULL"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	if((hRenderContext == IMG_NULL)
	&& (hTransferContext == IMG_NULL))
	{
		PVR_DPF((PVR_DBG_ERROR,"SGXSetContextPriority: both render and transfer contexts are NULL"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	
	ePriority = *pePriority;

	/* process must have 'privilege' to use raised priority levels */
	/* PRQA S 3415 3 */ /* ignore side effects warning */
	if ((ePriority != SGX_CONTEXT_PRIORITY_LOW) &&
		(ePriority != SGX_CONTEXT_PRIORITY_MEDIUM) &&
		!OSIsProcessPrivileged())
	{
		PVR_DPF((PVR_DBG_MESSAGE,
			"SGXSetContextPriority: Priority level request failed because process is not privileged"));
		PVR_DPF((PVR_DBG_MESSAGE, 
			"						Defaulting to medium priority"));
		ePriority = SGX_CONTEXT_PRIORITY_MEDIUM;
	}
	
	switch(ePriority)
	{
		case SGX_CONTEXT_PRIORITY_LOW:
			ui32Priority = 2;
			break;
		case SGX_CONTEXT_PRIORITY_MEDIUM:
			ui32Priority = 1;
			break;
		case SGX_CONTEXT_PRIORITY_HIGH:
			ui32Priority = 0;
			break;
		default:
			PVR_DPF((PVR_DBG_ERROR,"SGXSetContextPriority: Invalid priority level specified - %d!", (IMG_UINT32)ePriority));
			return PVRSRV_ERROR_INVALID_PARAMS;
	}
	
	
	/* set render context priority */
	if(hRenderContext != IMG_NULL)
	{
		SGX_RENDERCONTEXT *psRenderContext = (SGX_RENDERCONTEXT *)hRenderContext;
	
		/* 
			If this context has submitted operations or the priority has already been set, then priority cannot be changed.
		*/
		if(!psRenderContext->bPrioritySet && !psRenderContext->bKickSubmitted)
		{
            eError = SGXSetRenderContextPriority(
                            psDevData,
                            psRenderContext->hHWRenderContext,
                            ui32Priority,
                            offsetof(SGXMKIF_HWRENDERCONTEXT, sCommon.ui32Priority));

            if (eError != PVRSRV_OK)
            {
                PVR_DPF((PVR_DBG_ERROR, "SGXSetContextPriority: failed to set Render Context Priority"));
            }
            else
            {
                psRenderContext->bPrioritySet = IMG_TRUE;
                psRenderContext->ePriority = ePriority;
            }
		}
		else
		{
            ePriority = psRenderContext->ePriority;
		}
	}

	/* set transfer context priority */
	if(hTransferContext != IMG_NULL)
	{
		SGXTQ_CLIENT_TRANSFER_CONTEXT *psTransferContext 
        = (SGXTQ_CLIENT_TRANSFER_CONTEXT *)hTransferContext;

		/* 
			If this context has submitted operations or the priority has already been set, then priority cannot be changed.
		*/
		if(!psTransferContext->bPrioritySet && !psTransferContext->bKickSubmitted)
		{
            eError = SGXSetTransferContextPriority(
                            psDevData,
                            psTransferContext->hHWTransferContext,
                            ui32Priority,
                            offsetof(SGXMKIF_HWTRANSFERCONTEXT, sCommon.ui32Priority));

            if (eError != PVRSRV_OK)
            {
                PVR_DPF((PVR_DBG_ERROR, "SGXSetContextPriority: failed to set Transfer Context Priority"));
            }
            else
            {
                psTransferContext->bPrioritySet = IMG_TRUE;
                psTransferContext->ePriority = ePriority;
            }
		}
		else
		{
            ePriority = psTransferContext->ePriority;
		}
	}

	*pePriority = ePriority;

	return PVRSRV_OK;
#else /* defined(SUPPORT_SGX_PRIORITY_SCHEDULING) */

	PVR_UNREFERENCED_PARAMETER(psDevData);
	PVR_UNREFERENCED_PARAMETER(pePriority);
	PVR_UNREFERENCED_PARAMETER(hRenderContext);
	PVR_UNREFERENCED_PARAMETER(hTransferContext);

	return PVRSRV_ERROR_NOT_SUPPORTED; 
#endif /* defined(SUPPORT_SGX_PRIORITY_SCHEDULING) */
}

/******************************************************************************
 End of file (sgxutils_client.c)
******************************************************************************/
