/*!****************************************************************************
@File           pvr2dmem.c

@Title          PVR2D library memory related functions

@Author         Imagination Technologies

@Date           14/12/2006

@Copyright      Copyright 2006-2007 by Imagination Technologies Limited.
                All rights reserved. No part of this software, either material
                or conceptual may be copied or distributed, transmitted,
                transcribed, stored in a retrieval system or translated into
                any human or computer language in any form by any means,
                electronic, mechanical, manual or otherwise, or disclosed
                to third parties without the express written permission of
                Imagination Technologies Limited, Home Park Estate,
                Kings Langley, Hertfordshire, WD4 8LZ, U.K.

@Platform       Generic

@Description    PVR2D library memory function code

@DoxygenVer

******************************************************************************/

/******************************************************************************
Modifications :-
$Log: pvr2dmem.c $
******************************************************************************/

#include "img_defs.h"
#include "services.h"
#include "pvr2d.h"
#include "pvr2dint.h"

/* MS define is signed */
#undef PAGE_SIZE
#define PAGE_SIZE	4096U

/******************************************************************************
 @Function	PVR2DMemAlloc

 @Input    hContext        : pvr2d context handle

 @Input    ulBytes         : size of surface

 @Input    ulAlign         : desired byte aligment for pixmap memory

 @Input    ulFlags         : alloc flags

 @Modified ppsMemInfo      : PVR2DMEMINFO reference populated with new
                             hardware-allocated memory info

 @Return   error           : error code

 @Description              : Allocate a hardware memory for surface
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DMemAlloc (PVR2DCONTEXTHANDLE	hContext,
						  PVR2D_ULONG			ulBytes,
						  PVR2D_ULONG 			ulAlign,
						  PVR2D_ULONG 			ulFlags,
						  PVR2DMEMINFO 			**ppsMemInfoOut)
{
	PVRSRV_CLIENT_MEM_INFO *psMemInfo;
	PVR2DCONTEXT *psContext = (PVR2DCONTEXT *)hContext;
	PVR2D_BUFFER *psBuffer;
	IMG_UINT32 ui32Attribs;
	PVRSRV_ERROR srvErr;

	if (!hContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid context"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	if (!ulBytes || !ppsMemInfoOut)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	// Check for illegal flags
	if (ulFlags & ~(PVR2D_MEM_CACHED|PVR2D_MEM_WRITECOMBINE|PVR2D_GPU_PAGEABLE))
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - illegal flags"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	psBuffer = PVR2DCalloc(psContext, sizeof(PVR2D_BUFFER));
	if (!psBuffer)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - could not allocate host mem"));
		return PVR2DERROR_MEMORY_UNAVAILABLE;
	}

	ui32Attribs = PVRSRV_MEM_READ | PVRSRV_MEM_WRITE; // Add compulsary flags;

	if (ulFlags & PVR2D_MEM_CACHED)
	{
		ui32Attribs |= PVRSRV_MEM_CACHED;
	}
	if (ulFlags & PVR2D_MEM_WRITECOMBINE)
	{
		ui32Attribs |= PVRSRV_MEM_WRITECOMBINE;
	}
	if (ulFlags & PVR2D_GPU_PAGEABLE)
	{
		ui32Attribs |= PVRSRV_HAP_GPU_PAGEABLE;
	}

	// Alignment must be a power of 2
	if ((ulAlign != 0) && ((ulAlign & (ulAlign - 1)) != 0))
	{
		return PVR2DERROR_INVALID_PARAMETER;
	}

	if (ulFlags & (PVR2D_MEM_CACHED|PVR2D_MEM_WRITECOMBINE))
	{
		// If mem flags are specified then size/alignment must be a multiple of the SGX MMU hardware page size
		ulBytes = (ulBytes + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1);
		ulAlign = (ulAlign + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1);
	}

	srvErr = PVRSRVAllocDeviceMem(&psContext->sDevData,
							psContext->h2DHeap,
							ui32Attribs,
							ulBytes, ulAlign, &psMemInfo);

	if (srvErr != PVRSRV_OK)
	{
		PVR2DFree(psContext, psBuffer);
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - could not allocate device memory!"));
		return PVR2DERROR_IOCTL_ERROR;
	}

#if defined(PDUMP)
	PVRSRVPDumpCommentf(psContext->psServices, IMG_FALSE,
				"PVR2DMemAlloc : Size=0x%08X Alignment=0x%08X DevVAddr=0x%08X",
				ulBytes, ulAlign, psMemInfo->sDevVAddr.uiAddr);
#endif

	psBuffer->eType = PVR2D_MEMINFO_ALLOCATED;
	PVR2DMEMINFO_INITIALISE(&psBuffer->sMemInfo, psMemInfo);
	*ppsMemInfoOut = (PVR2DMEMINFO *)psBuffer;
	return PVR2D_OK;
}


/******************************************************************************
 @Function	PVR2DMemExport

 @Input		hContext        : pvr2d context handle

 @Input		ulFlags         :

 @Input		psMemInfo		: PVR2DMEMINFO

 @Output	*phMemInfo		: PVR2D_HANDLE handle to pass to other processes
							  as input to the PVR2DMemMap function

 @Return  	error           : error code

 @Description				: Exports the allocation so it can be mapped to the
 								same device in other processes
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DMemExport (PVR2DCONTEXTHANDLE	hContext,
						  PVR2D_ULONG 			ulFlags,
						  PVR2DMEMINFO 			*psMemInfo,
#if defined (SUPPORT_SID_INTERFACE)
						  PVR2D_SID				*phMemInfo)
#else
						  PVR2D_HANDLE			*phMemInfo)
#endif
{
	PVR2DCONTEXT *psContext = (PVR2DCONTEXT *)hContext;
	PVRSRV_CLIENT_MEM_INFO *psClientMemInfo;

	PVR_UNREFERENCED_PARAMETER(ulFlags);

	if (!hContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid context"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	if (!psMemInfo || !phMemInfo)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	psClientMemInfo = CLIENT_MEM_INFO_FROM_PVR2DMEMINFO(psMemInfo);

	if (!psClientMemInfo)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	if(PVRSRVExportDeviceMem(&psContext->sDevData,
							 psClientMemInfo,
							 phMemInfo) != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - could not export device memory!"));
		return PVR2DERROR_IOCTL_ERROR;
	}

	return PVR2D_OK;
}


/******************************************************************************
 @Function	PVR2DMemWrap

 @Input    hContext        : pvr2d context handle

 @Input    pMem			   : User Mode CPU Virtual address pointing to memory to wrap

 @Input    ulBytes		   : Size of memory to wrap

 @Input    plPageAddress   : List of system (bus) addresses of pages to wrap

 @Modified ppsMemInfo      : PVR2DMEMINFO reference populated with new
                             hardware-wrapped memory info

 @Return   error           : error code

 @Description              : Wrap memory for hardware for surface
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DMemWrap(PVR2DCONTEXTHANDLE hContext,
						PVR2D_VOID  *pMem,
						PVR2D_ULONG ulFlags,
						PVR2D_ULONG ulBytes,
						PVR2D_ULONG *plPageAddress,
						PVR2DMEMINFO **ppsMemInfo)
{
	PVRSRV_CLIENT_MEM_INFO *psMemInfo;
	PVR2DCONTEXT *psContext = (PVR2DCONTEXT *)hContext;
	PVR2D_BUFFER *psBuffer;
	IMG_UINT32 ui32Mem = (IMG_UINT32)pMem;
	IMG_UINT32 ui32Offset = 0, ui32PageTableEntries;
	IMG_SYS_PHYADDR *psPageTable = (IMG_SYS_PHYADDR *)0;
	IMG_BOOL bPhysContig = (ulFlags & PVR2D_WRAPFLAG_CONTIGUOUS) ? IMG_TRUE : IMG_FALSE;
	IMG_UINT32 i;

	if (!hContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid context"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	if (!ulBytes || !pMem || !ppsMemInfo)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	if (plPageAddress)
	{
		/* client passed in a list of physical pages */
		ui32Offset = (ui32Mem & PVR2D_MMU_PAGE_MASK);

		ui32PageTableEntries =
			bPhysContig ?  1 :
		   (ui32Offset + ulBytes + PVR2D_MMU_PAGE_MASK) >> PVR2D_MMU_PAGE_SHIFT;

		psPageTable = PVR2DCalloc(psContext, ui32PageTableEntries * sizeof(IMG_SYS_PHYADDR));
		if (!psPageTable)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - could not allocate host mem"));
			return PVR2DERROR_MEMORY_UNAVAILABLE;
		}

		for (i = 0; i < ui32PageTableEntries; i++)
		{
			psPageTable[i].uiAddr = plPageAddress[i];
	    }
	}

	psBuffer = PVR2DCalloc(psContext, sizeof(PVR2D_BUFFER));
	if (!psBuffer)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - could not allocate host mem"));
		PVR2DFree(psContext, psPageTable);
		return PVR2DERROR_MEMORY_UNAVAILABLE;
	}

	if (PVRSRVWrapExtMemory(&psContext->sDevData,
							psContext->hDevMemContext,
							ulBytes,
							ui32Offset,
							bPhysContig,
							psPageTable,
							pMem,
							0,
							&psMemInfo)	!= PVRSRV_OK)
	{
		if (psPageTable)
		{
			PVR2DFree(psContext, psPageTable);
		}
		PVR2DFree(psContext, psBuffer);

		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - could not wrap external memory!"));
		return PVR2DERROR_MEMORY_UNAVAILABLE;
	}

	if (psPageTable)
	{
		PVR2DFree(psContext, psPageTable);
	}
	psBuffer->eType = PVR2D_MEMINFO_WRAPPED;
	PVR2DMEMINFO_INITIALISE(&psBuffer->sMemInfo, psMemInfo);

	/*
	 * Set the user mode address in the PVR2DMEMINFO, in case someone
	 * tries accessing the wrapped memory via this route.
	 */
	psBuffer->sMemInfo.pBase = pMem;

	*ppsMemInfo = (PVR2DMEMINFO *)psBuffer;

	return PVR2D_OK;
}

/******************************************************************************
 @Function	PVR2DMemMap

 @Input		hContext		: pvr2d context handle

 @Input		ulFlags			: flags

 @Input		hMemHandle		: private mapping information for the buffer to map,
							  obtained from PVR2DMemExport

 @Output	ppsDstMem		: DST PVR2DMEMINFO reference populated with new
							  mapped memory info

 @Return   error			: error code

 @Description				: maps memory from one PVR2DMEMINFO buffer to
 							  another PVR2DMEMINFO buffer. Buffers may be in
 							  different processes.  Only hMemHandle needs
 							  to be passed for the original buffer
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DMemMap(PVR2DCONTEXTHANDLE hContext,
						PVR2D_ULONG ulFlags,
#if defined (SUPPORT_SID_INTERFACE)
						PVR2D_SID    hMemHandle,
#else
						PVR2D_HANDLE hMemHandle,
#endif
						PVR2DMEMINFO **ppsDstMem)
{
	PVRSRV_ERROR eResult;
	PVR2DCONTEXT *psContext;
	PVRSRV_CLIENT_MEM_INFO *psDstMemInfo;
	PVR2D_BUFFER *psDstBuffer;

	PVR_UNREFERENCED_PARAMETER(ulFlags);

	if (!hContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid context"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	/* check the parameters */
	if (!hMemHandle || !ppsDstMem)
	{
		PVR2D_DPF((PVR_DBG_ERROR,"PVR2DMemMap error: invalid parameters"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	/* setup the context */
	psContext = (PVR2DCONTEXT *)hContext;

	/*
		the memory that is being mapped could have been allocated
		from the current process or a different process.
		In either case, the only significant input information for
		the buffer to map is the hMemHandle exported by PVR2DMemExport.
	*/

	/*
		construct a client meminfo for the original allocation
	*/

	/* allocate memory for the meminfo */
	psDstBuffer = PVR2DCalloc(psContext, sizeof(PVR2D_BUFFER));
	if (!psDstBuffer)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - could not allocate host mem"));
		return PVR2DERROR_MEMORY_UNAVAILABLE;
	}

	/* map the meminfo */
	eResult = PVRSRVMapDeviceMemory(&psContext->sDevData,
									hMemHandle,
									psContext->hGeneralMappingHeap,
									&psDstMemInfo);
	if (eResult != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DMemMap error: mapping of meminfo failed"));
		PVR2DFree(psContext, psDstBuffer);
		return PVR2DERROR_MAPPING_FAILED;
	}

	/* set up the PVR2D mem info */
	psDstBuffer->eType = PVR2D_MEMINFO_MAPPED;
	PVR2DMEMINFO_INITIALISE(&psDstBuffer->sMemInfo, psDstMemInfo);

	/* return the new meminfo */
	*ppsDstMem = &psDstBuffer->sMemInfo;

	return PVR2D_OK;
}


/******************************************************************************
 @Function	PVR2DMemFree

 @Input    hContext        : pvr2d context handle

 @Input    psMem           : PVR2DMEMINFO reference

 @Return   error           : error code

 @Description              : Free hardware memory used for surface
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DMemFree (PVR2DCONTEXTHANDLE hContext, PVR2DMEMINFO *psMem)
{
	PVRSRV_CLIENT_MEM_INFO *psClientMemInfo;
	PVR2DCONTEXT *psContext = (PVR2DCONTEXT *)hContext;
	PVR2D_BUFFER *psBuffer;
	PVR2DERROR eResult;

	if (!hContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid context"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	if(!psMem)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	psClientMemInfo = CLIENT_MEM_INFO_FROM_PVR2DMEMINFO(psMem);

	if (!psClientMemInfo)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	psBuffer = (PVR2D_BUFFER *)psMem;

	if (psBuffer->eType == PVR2D_MEMINFO_DISPLAY_OWNED)
	{
		// Don't need to wait for display surface blts to complete
		// .. because we dont want to free the display surface
		return PVR2D_OK;
	}
	
	// Wait for outstanding read/write ops to complete before freeing the memory
	eResult = PVR2DQueryBlitsComplete(hContext, psMem, 1);
	
	if (eResult != PVR2D_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DMemFree -> PVR2DQueryBlitsComplete failed"));
		return eResult;
	}
	
	switch(psBuffer->eType)
	{
		case PVR2D_MEMINFO_ALLOCATED:
			if(PVRSRVFreeDeviceMem(&psContext->sDevData, psClientMemInfo) != PVRSRV_OK)
			{
				PVR2D_DPF((PVR_DBG_ERROR, "PVR2DMemFree error - could not free device memory!"));
				return PVR2DERROR_IOCTL_ERROR;
			}
			
#if defined(PDUMP)
			PVRSRVPDumpCommentf( psContext->psServices, IMG_FALSE,
									"PVR2DMemFree : DevVAddr=0x%08X",
									psClientMemInfo->sDevVAddr.uiAddr);
#endif

			PVR2DFree(psContext, psMem);
			break;

		case PVR2D_MEMINFO_WRAPPED:
			if(PVRSRVUnwrapExtMemory(&psContext->sDevData, psClientMemInfo) != PVRSRV_OK)
			{
				PVR2D_DPF((PVR_DBG_ERROR, "PVR2DMemFree error - could not unwrap external memory!"));
				return PVR2DERROR_IOCTL_ERROR;
			}

			PVR2DFree(psContext, psMem);
			break;

		case PVR2D_MEMINFO_MAPPED:
			if(PVRSRVUnmapDeviceMemory(&psContext->sDevData, psClientMemInfo) != PVRSRV_OK)
			{
				PVR2D_DPF((PVR_DBG_ERROR, "PVR2DMemFree error: unmap of meminfo failed"));
				return PVR2DERROR_MAPPING_FAILED;
			}
			PVR2DFree(psContext, psMem);
			break;

		default:
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DMemFree error - invalid mem type!"));
			return PVR2DERROR_INVALID_PARAMETER;
	}

	return PVR2D_OK;
}

/******************************************************************************
 @Function	PVR2DRemapToDev

 @Input    hContext        : pvr2d context handle

 @Input    psMem           : PVR2DMEMINFO reference

 @Return   error           : error code

 @Description              : Remap buffer to GPU virtual address space
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DRemapToDev(PVR2DCONTEXTHANDLE hContext, PVR2DMEMINFO *psMem)
{
	PVRSRV_CLIENT_MEM_INFO *psClientMemInfo;
	PVR2DCONTEXT *psContext = (PVR2DCONTEXT *)hContext;

	if (!hContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DRemapToDev - invalid context"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	if (!psMem)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DRemapToDev - invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	psClientMemInfo = CLIENT_MEM_INFO_FROM_PVR2DMEMINFO(psMem);

	if (!psClientMemInfo)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DRemapToDev - invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	if (PVRSRVRemapToDev(&psContext->sDevData, psClientMemInfo) != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DRemapToDev error - could not unmap from device!"));
		return PVR2DERROR_IOCTL_ERROR;
	}

	/* device virtual address might have changed */
	psMem->ui32DevAddr = psClientMemInfo->sDevVAddr.uiAddr;

	return PVR2D_OK;
}

/******************************************************************************
 @Function	PVR2DUnmapFromDev

 @Input    hContext        : pvr2d context handle

 @Input    psMem           : PVR2DMEMINFO reference

 @Return   error           : error code

 @Description              : Unmap buffer from GPU virtual address space
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DUnmapFromDev(PVR2DCONTEXTHANDLE hContext, PVR2DMEMINFO *psMem)
{
	PVRSRV_CLIENT_MEM_INFO *psClientMemInfo;
	PVR2DCONTEXT *psContext = (PVR2DCONTEXT *)hContext;
	PVR2DERROR eResult;

	if (!hContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DUnmapFromDev error - invalid context"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	if (!psMem)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DUnmapFromDev error - invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	psClientMemInfo = CLIENT_MEM_INFO_FROM_PVR2DMEMINFO(psMem);

	if (!psClientMemInfo)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DUnmapFromDev error - invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	// Wait for outstanding read/write ops to complete before unmapping the memory
	eResult = PVR2DQueryBlitsComplete(hContext, psMem, 1);

	if (eResult != PVR2D_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DUnmapFromDev error - PVR2DQueryBlitsComplete failed"));
		return eResult;
	}

	if (PVRSRVUnmapFromDev(&psContext->sDevData, psClientMemInfo) != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DUnmapFromDev error - could not unmap from device!"));
		return PVR2DERROR_IOCTL_ERROR;
	}

	return PVR2D_OK;
}

/******************************************************************************
 @Function	PVR2DModifyPendingOps

 @Input    hContext				: pvr2d context handle

 @Output   phSyncModObj			: pointer to receive mod object handle

 @Input    psMemInfo			: PVR2DMEMINFO reference

 @Input	   bIsWriteOp			: Flag denoting if the write ops (PVR2D_TRUE) or the 
								  read ops (PVR2D_FALSE) should be modified. 

 @Input	   pulReadOpsPending	: (Optional) pointer to receive pending read ops value 
								  before this op was registered

 @Input	   pulWriteOpsPending	: (Optional) pointer to receive pending write ops value 
								  before this op was registered

 @Return   error				: error code

 @Description					: Adds a pending read or write op to a mem info and returns
								  a tracking handle for the operation. EACH CALL TO THIS API 
								  MUST BE PAIRED WITH A SUBSEQUENT CALL TO PVR2DModifyCompleteOps
								  OR ALL OPERATIONS TO OR FROM THIS SURFACE WILL BE PERMANENTLY
								  STALLED.
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DModifyPendingOps(const PVR2DCONTEXTHANDLE hContext,
#if defined (SUPPORT_SID_INTERFACE)
								 PVR2D_SID    *phSyncModObj,
#else
								 PVR2D_HANDLE *phSyncModObj,
#endif
								 PVR2DMEMINFO *psMemInfo,
								 PVR2D_BOOL  bIsWriteOp,
								 PVR2D_ULONG *pulReadOpsPending,
								 PVR2D_ULONG *pulWriteOpsPending)
{
	PVRSRV_CLIENT_MEM_INFO	*psClientMemInfo;
	PVR2DCONTEXT			*psContext			= (PVR2DCONTEXT *)hContext;

#if defined (SUPPORT_SID_INTERFACE)
	 IMG_SID				hSyncInfoModObj;
#else
	 IMG_HANDLE				hSyncInfoModObj;
#endif

	if (!hContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid context"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	if(!psMemInfo)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	if(!phSyncModObj)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	psClientMemInfo = CLIENT_MEM_INFO_FROM_PVR2DMEMINFO(psMemInfo);

	if (!psClientMemInfo)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	if(PVRSRVCreateSyncInfoModObj(psContext->psServices, &hSyncInfoModObj) != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - PVRSRVCreateSyncInfoModObj Failed!"));
		return PVR2DERROR_GENERIC_ERROR;
	}

	if(PVRSRVModifyPendingSyncOps(psContext->psServices, 
								  hSyncInfoModObj,
								  psClientMemInfo->psClientSyncInfo,
								  bIsWriteOp ? PVRSRV_MODIFYSYNCOPS_FLAGS_WO_INC : 
											   PVRSRV_MODIFYSYNCOPS_FLAGS_RO_INC,
								  (IMG_UINT32*) pulReadOpsPending,
								  (IMG_UINT32*) pulWriteOpsPending) != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - PVRSRVModifyPendingSyncOps Failed!"));
		PVRSRVDestroySyncInfoModObj(psContext->psServices, hSyncInfoModObj);
		return PVR2DERROR_GENERIC_ERROR;
	}

#if defined (SUPPORT_SID_INTERFACE)
	 *phSyncModObj = (PVR2D_SID)	hSyncInfoModObj;
#else
	 *phSyncModObj = (PVR2D_HANDLE)	hSyncInfoModObj;
#endif

	return PVR2D_OK;
}

/******************************************************************************
 @Function	PVR2DModifyCompleteOps

 @Input    hContext				: pvr2d context handle

 @Output   hSyncModObj			: The sync mod object handle

 @Return   error				: error code

 @Description					: Marks a previously created pending operation
								  as complete. The given sync mod object handle 
								  will be invalid after this call and can be 
								  discarded.
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DModifyCompleteOps(const PVR2DCONTEXTHANDLE hContext,
#if defined (SUPPORT_SID_INTERFACE)
								  PVR2D_SID    hSyncModObj)
#else
								  PVR2D_HANDLE hSyncModObj)
#endif
{
	PVR2DCONTEXT *psContext	= (PVR2DCONTEXT *)hContext;
	PVRSRV_ERROR eError;

	if (!hContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid context"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	if(!hSyncModObj)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	eError = PVRSRVModifyCompleteSyncOps(psContext->psServices, 
#if defined (SUPPORT_SID_INTERFACE)
										(IMG_SID)  hSyncModObj);
#else
										(IMG_HANDLE) hSyncModObj);
#endif		

	switch(eError)
	{
		case PVRSRV_OK: 
		{
			return PVR2D_OK;
		}
		case PVRSRV_ERROR_HANDLE_INDEX_OUT_OF_RANGE:
		case PVRSRV_ERROR_HANDLE_NOT_ALLOCATED:
		case PVRSRV_ERROR_HANDLE_TYPE_MISMATCH:
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid handle"));
			return PVR2DERROR_INVALID_PARAMETER;
		}
		default:
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - unknown error"));
			return PVR2DERROR_GENERIC_ERROR;
		}
	}
}
/******************************************************************************
 @Function	PVR2DFlushToSyncModObj

 @Input    hContext				: pvr2d context handle

 @Output   hSyncModObj			: The sync mod object handle

 @Input	   bWait				: Flag denoting if the implementation should wait
								  for the dependencies to be up to date (PVR2D_TRUE) or 
								  perform a single test and return (PVR2D_FALSE).

 @Return   error				: Return code - PVR2D_OK if dependencies for the Sync mod 
								  object are up to date or PVR2DERROR_BLT_NOTCOMPLETE if 
								  the dependencies are not up to date.

 @Description					: Checks the dependencies are up to date for a previously 
								  created Sync mod object.
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DFlushToSyncModObj(const PVR2DCONTEXTHANDLE hContext,
#if defined (SUPPORT_SID_INTERFACE)
								  PVR2D_SID  hSyncModObj,
#else
								  PVR2D_HANDLE hSyncModObj,
#endif
								  PVR2D_BOOL bWait)
{
	PVR2DCONTEXT *psContext	= (PVR2DCONTEXT *)hContext;
	PVRSRV_ERROR eError;

	if (!hContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid context"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	if(!hSyncModObj)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}
	
	/* Wait not implemented in the bridge, so wait here if requested */
	while((eError = PVRSRVSyncOpsFlushToModObj(psContext->psServices,
#if defined (SUPPORT_SID_INTERFACE)
									(IMG_SID)  hSyncModObj,
#else
									(IMG_HANDLE) hSyncModObj,
#endif
									 IMG_FALSE)) == PVRSRV_ERROR_RETRY)
	{
		if(!bWait)
		{
			break;
		}

		if(PVRSRVEventObjectWait(psContext->psServices, psContext->sMiscInfo.hOSGlobalEvent) == PVRSRV_ERROR_TIMEOUT)
		{
#if defined (SUPPORT_SGX)
			/* Sometimes SGX needs a little encouragement */
			if(SGXScheduleProcessQueues(&psContext->sDevData) != PVRSRV_OK)
			{
				PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - SGXScheduleProcessQueues failed"));
			}
#endif
		}

	}

	switch(eError)
	{
		case PVRSRV_OK: 
		{
			return PVR2D_OK;
		}
		case PVRSRV_ERROR_RETRY: 
		{
			return PVR2DERROR_BLT_NOTCOMPLETE;
		}		
		case PVRSRV_ERROR_HANDLE_INDEX_OUT_OF_RANGE:
		case PVRSRV_ERROR_HANDLE_NOT_ALLOCATED:
		case PVRSRV_ERROR_HANDLE_TYPE_MISMATCH:
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid handle"));
			return PVR2DERROR_INVALID_PARAMETER;
		}
		default:
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - unknown error"));
			return PVR2DERROR_GENERIC_ERROR;
		}
	}
}

/******************************************************************************
 @Function	PVR2DTakeSyncToken

 @Input    hContext				: pvr2d context handle

 @Input    psMemInfo			: PVR2DMEMINFO reference

 @Output   phSyncToken			: Pointer a sync snapshot token handle

 @Input	   pulReadOpsPending	: (Optional) pointer to receive current pending read ops value 

 @Input	   pulWriteOpsPending	: (Optional) pointer to receive current pending write ops value 

 @Return   error				: error code

 @Description					: Snapshots the currently pending operations for the 
								  given memory and returns a referencing handle to the 
								  caller. This function does not modify the pending 
								  operations for the given memory. THIS CALL MUST 
								  BE PAIRED WITH A SUBSEQUENT CALL TO PVR2DReleaseSyncToken
								  TO PREVENT MEMORY LEAKS.

******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DTakeSyncToken(const PVR2DCONTEXTHANDLE hContext,
							  PVR2DMEMINFO *psMemInfo,
							  PVR2D_HANDLE *phSyncToken,
							  PVR2D_ULONG *pulReadOpsPending,
							  PVR2D_ULONG *pulWriteOpsPending)
{
	PVRSRV_CLIENT_MEM_INFO	*psClientMemInfo;
	PVR2DCONTEXT			*psContext			= (PVR2DCONTEXT *)hContext;
	PVRSRV_SYNC_TOKEN		*psToken;

	if (!hContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid context"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	if(!psMemInfo)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	if(!phSyncToken)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	psClientMemInfo = CLIENT_MEM_INFO_FROM_PVR2DMEMINFO(psMemInfo);

	if (!psClientMemInfo)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}
	
	if((psToken = PVR2DCalloc(psContext, sizeof(PVRSRV_SYNC_TOKEN))) == IMG_NULL)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - could not allocate host mem"));
		return PVR2DERROR_MEMORY_UNAVAILABLE;
	}

	if(PVRSRVSyncOpsTakeToken(psContext->psServices,
#if defined (SUPPORT_SID_INTERFACE)
							  psClientMemInfo->psClientSyncInfo->hKernelSyncInfo,
#else
							  psClientMemInfo->psClientSyncInfo,
#endif
							  psToken) != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid params"));
		PVR2DFree(psContext, psToken);
		return PVR2DERROR_INVALID_PARAMETER;
	}

	*phSyncToken = (PVR2D_HANDLE) psToken;

	if(pulReadOpsPending)
	{
		*pulReadOpsPending = psToken->sPrivate.ui32ReadOpsPendingSnapshot;
	} 	

	if(pulWriteOpsPending)
	{
		*pulWriteOpsPending = psToken->sPrivate.ui32WriteOpsPendingSnapshot;
	}

	return PVR2D_OK;
}

/******************************************************************************
 @Function	PVR2DReleaseSyncToken

 @Input    hContext				: pvr2d context handle

 @Input    hSyncToken			: Sync snapshot token handle

 @Return   error				: error code

 @Description					: Releases resources acquired when token was taken.

******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DReleaseSyncToken(const PVR2DCONTEXTHANDLE hContext,
								 PVR2D_HANDLE hSyncToken)
{
	PVR2DCONTEXT			*psContext	= (PVR2DCONTEXT *)hContext;
	PVRSRV_SYNC_TOKEN		*psToken	= (PVRSRV_SYNC_TOKEN*) hSyncToken;

	if (!psContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid context"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	if(!psToken)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	PVR2DFree(psContext, psToken);

	return PVR2D_OK;
}

/******************************************************************************
 @Function	PVR2DFlushToSyncToken

 @Input    hContext				: pvr2d context handle

 @Input    psMemInfo			: PVR2DMEMINFO reference

 @Input    hSyncToken			: The sync snapshot token handle

 @Input	   bWait				: Flag denoting if the implementation should wait
								  for the dependencies to be up to date (PVR2D_TRUE) or 
								  perform a single test and return  (PVR2D_FALSE).

 @Return   error				: Return code - PVR2D_OK if dependencies for the Sync mod 
								  object are up to date or PVR2DERROR_BLT_NOTCOMPLETE if 
								  the dependencies are not up to date.

 @Description					: Checks the dependencies are up to date for a previously 
								  created sync snapshot token.
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DFlushToSyncToken(const PVR2DCONTEXTHANDLE hContext,
								 PVR2DMEMINFO *psMemInfo,
								 PVR2D_HANDLE hSyncToken,
								 PVR2D_BOOL bWait)
{
	PVRSRV_ERROR			eError;
	PVRSRV_CLIENT_MEM_INFO	*psClientMemInfo;
	PVR2DCONTEXT			*psContext			= (PVR2DCONTEXT *)hContext;
	PVRSRV_SYNC_TOKEN		*psToken			= (PVRSRV_SYNC_TOKEN*) hSyncToken;

	if (!hContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid context"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	if(!psMemInfo)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	if(!hSyncToken)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	psClientMemInfo = CLIENT_MEM_INFO_FROM_PVR2DMEMINFO(psMemInfo);

	if (!psClientMemInfo)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}
	
	/* Wait not implemented in the bridge, so wait here if requested */
	while((eError = PVRSRVSyncOpsFlushToToken(psContext->psServices,
#if defined (SUPPORT_SID_INTERFACE)
											  psClientMemInfo->psClientSyncInfo->hKernelSyncInfo,
#else
											  psClientMemInfo->psClientSyncInfo,
#endif
											  psToken,
											  IMG_FALSE)) == PVRSRV_ERROR_RETRY)
	{
		if(!bWait)
		{
			break;
		}

		if(PVRSRVEventObjectWait(psContext->psServices, psContext->sMiscInfo.hOSGlobalEvent) == PVRSRV_ERROR_TIMEOUT)
		{
#if defined (SUPPORT_SGX)
			/* Sometimes SGX needs a little encouragement */
			if(SGXScheduleProcessQueues(&psContext->sDevData) != PVRSRV_OK)
			{
				PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - SGXScheduleProcessQueues failed"));
			}
#endif
		}
	}

	switch(eError)
	{
		case PVRSRV_OK: 
		{
			return PVR2D_OK;
		}
		case PVRSRV_ERROR_RETRY: 
		{
			return PVR2DERROR_BLT_NOTCOMPLETE;
		}		
		case PVRSRV_ERROR_HANDLE_INDEX_OUT_OF_RANGE:
		case PVRSRV_ERROR_HANDLE_NOT_ALLOCATED:
		case PVRSRV_ERROR_HANDLE_TYPE_MISMATCH:
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid handle"));
			return PVR2DERROR_INVALID_PARAMETER;
		}
		default:
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - unknown error"));
			return PVR2DERROR_GENERIC_ERROR;
		}
	}
}

/******************************************************************************
 @Function	PVR2DWaitForNextHardwareEvent

 @Input    hContext				: pvr2d context handle

 @Return   error				: Return code - PVR2D_OK if an event occured or 
								  PVR2DERROR_BLT_NOTCOMPLETE if the call timed out.

 @Description					: Waits for the next hardware event signal
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DWaitForNextHardwareEvent(const PVR2DCONTEXTHANDLE hContext)
{		
	PVR2DCONTEXT *psContext	= (PVR2DCONTEXT *)hContext;
	PVRSRV_ERROR eError;
	
	if (!hContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - invalid context"));
		return PVR2DERROR_INVALID_CONTEXT;
	}
	
	eError = PVRSRVEventObjectWait(psContext->psServices, psContext->sMiscInfo.hOSGlobalEvent);

	switch(eError)
	{
		case PVRSRV_OK: 
		{
			return PVR2D_OK;
		}
		case PVRSRV_ERROR_TIMEOUT: 
		{
			return PVR2DERROR_BLT_NOTCOMPLETE;
		}		
		default:
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2D error - unknown error"));
			return PVR2DERROR_GENERIC_ERROR;
		}
	}
}
/******************************************************************************
 End of file (pvr2dmem.c)
******************************************************************************/

