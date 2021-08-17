/******************************************************************************
 * Name         : context.h
 *
 * Copyright    : 2005-2006 by Imagination Technologies Limited.
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
 * $Log: context.h $
 *****************************************************************************/

#ifndef _CONTEXT_
#define _CONTEXT_

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h> /* For the macro offsetof() */
#include <math.h>

#include "psp2/libheap_custom.h"

#include "imgextensions.h"

#include "services.h"

#include "ogles2_types.h"

#include "drvgl2.h"

/* To ensure that the prototypes for the extensions are declared */
#define GL_GLEXT_PROTOTYPES 1
#include "drvgl2ext.h"
#include "buffers.h"
#include "eglapi.h"

#include "sgxdefs.h"

/* To get the right structs in glsl.h */
#define GLSL_ES 1

#include "constants.h"
#include "names.h"
#include "kickresource.h"
#include "bufobj.h"
#include "attrib.h"
#include "fbo.h"
#include "glsl.h"
#include "usp.h"
#include "texformat.h"
#include "pds.h"
#include "texyuv.h"
#include "texstream.h"
#include "texture.h"
#include "state.h"
#include "glsl2uf.h"
#include "uniform.h"
#include "codeheap.h"
#include "statehash.h"
#include "esbinshader.h"
#include "shader.h"
#include "usegles2.h"
#include "validate.h"
#include "metrics.h"
#include "profile.h"
#include "misc.h"
#include "osglue.h"
#include "pvr_debug.h"
#include "pvr_metrics.h"
#include "pdump.h"
#include "combiner.h"
#include "usecodegen.h"
#include "usegen.h"
#include "vertexarrobj.h"



#define GLES2_SCHEDULE_HW_LAST_IN_SCENE		0x00000001
#define GLES2_SCHEDULE_HW_WAIT_FOR_TA		0x00000002
#define GLES2_SCHEDULE_HW_WAIT_FOR_3D		0x00000004
#define GLES2_SCHEDULE_HW_DISCARD_SCENE		0x00000008
#define GLES2_SCHEDULE_HW_BBOX_RENDER		0x00000010
#define GLES2_SCHEDULE_HW_POST_BBOX_RENDER	0x00000020
#define GLES2_SCHEDULE_HW_IGNORE_FLUSHLIST	0x00000040
#define GLES2_SCHEDULE_HW_MIDSCENE_RENDER	0x00000080


typedef struct GLES2SurfaceFlushListTAG
{
	EGLRenderSurface *psRenderSurface;
	GLES2Texture *psTex;
	GLES2Context *gc;
	struct GLES2SurfaceFlushListTAG *psNext;

}GLES2SurfaceFlushList;

/******************************************************************************
	GLES2 Context Shared Data
******************************************************************************/
typedef struct GLES2ContextSharedStateTAG
{
	/* Reference count. Update only within a critical section. Init with one, free when one. */
	IMG_UINT32           ui32RefCount;

	/* Keeps track of what textures are attached to any given surface */
	GLES2TextureManager  *psTextureManager;

	/* Keeps track of what USSE code variants are attached to any given surface */
	KRMKickResourceManager sUSEShaderVariantKRM;

	/* Keeps track of which buffer objects are attached to any given context */
	KRMKickResourceManager sBufferObjectKRM;

	/* Dictionaries of GL objects addressed by name. */
	GLES2NamesArray      *apsNamesArray[GLES2_MAX_SHAREABLE_NAMETYPE]; 

	/* Memory heap for vertex and fragment code blocks */
	UCH_UseCodeHeap     *psUSEVertexCodeHeap;
	UCH_UseCodeHeap     *psUSEFragmentCodeHeap;
	UCH_UseCodeHeap     *psPDSFragmentCodeHeap;

	/* Locks shared between all contexts.
	 * The refcount, memory heaps and name arrays use the primary lock while the
	 * texture manager and USSE code manager uses the secondary to avoid deadlocks.
	 * If an operation will require both locks, it must take
	 * the primary lock first and the secondary lock later (to avoid deadlocks).
	 * The tertiary lock prevents simultaneous attempts to upload the same texture
	 */
	PVRSRV_MUTEX_HANDLE     hPrimaryLock, hSecondaryLock, hTertiaryLock;

	/* Meminfo for drawarray static index buffers */
	PVRSRV_CLIENT_MEM_INFO *psSequentialStaticIndicesMemInfo;
	PVRSRV_CLIENT_MEM_INFO *psLineStripStaticIndicesMemInfo;
	
	GLES2SurfaceFlushList *psFlushList;
	PVRSRV_MUTEX_HANDLE hFlushListLock;

#ifdef PDUMP
	IMG_BOOL bMustDumpSequentialStaticIndices;
	IMG_BOOL bMustDumpLineStripStaticIndices;
#endif

} GLES2ContextSharedState;


/******************************************************************************
	GLES2 Context
******************************************************************************/

struct GLES2Context_TAG
{
	IMG_UINT32 ui32Enables;

	IMG_UINT32 ui32DirtyState;
	IMG_UINT32 ui32EmitMask;

	GLES2state sState;

	PVRSRV_DEV_DATA *ps3DDevData;
	SrvSysContext *psSysContext;
	SGX_KICKTA sKickTA;
	IMG_BOOL 		bSPMOutOfMemEvent;

	KRMStatusUpdate sKRMTAStatusUpdate;

	GLES2ProgramMachine sProgram;
	GLES2BufferObjectMachine sBufferObject;
	GLES2FrameBufferObjectMachine sFrameBuffer;
	GLES2TextureMachine sTexture;
  
	GLES2PrimitiveMachine sPrim;

    GLES2VertexArrayObjectMachine sVAOMachine;
	/* Keeps track of which VAOs are attached to any given context */
	KRMKickResourceManager sVAOKRM;

#if defined(GLES2_EXTENSION_VERTEX_ARRAY_OBJECT)
	/* Dictionaries of GL objects addressed by name. */
	GLES2NamesArray *apsNamesArray[GLES2_MAX_UNSHAREABLE_NAMETYPE]; 
#endif /* defined(GLES2_EXTENSION_VERTEX_ARRAY_OBJECT) */

	/*
	** Mode information that describes the kind of buffers and rendering
	** modes that this context is currently using.
	*/

	EGLcontextMode *psMode;

	EGLDrawableParams *psDrawParams;
	EGLDrawableParams *psReadParams;

	EGLRenderSurface *psRenderSurface;
	EGLDrawableHandle	hEGLSurface;

	IMG_BOOL bFullScreenViewport;
	IMG_BOOL bFullScreenScissor;

	IMG_BOOL bDrawMaskInvalid;

	IMG_CHAR *pszExtensions;

	IMG_BOOL bHasBeenCurrent;

#if defined(GLES2_EXTENSION_EGL_IMAGE)
	IMG_UINT32 ui32NumEGLImageTextureTargetsBound;
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */

	IMG_INT32 i32Error;

#if defined(TIMING) || defined(DEBUG)

	IMG_VOID  *pvFileInfo;

	StateMetricData	*psStateMetricData;

	IMG_UINT32 ui32ValidEnables[GLES2_NUMBER_PROFILE_ENABLES];
	IMG_UINT32 ui32RedundantEnables[GLES2_NUMBER_PROFILE_ENABLES];

	IMG_UINT32 		ui32VBOMemCurrent;
	IMG_UINT32		ui32VBOHighWaterMark;
	IMG_UINT32		ui32VBONamesGenerated;

	DrawCallData	sDrawArraysCalls[2];
	DrawCallData	sDrawElementsCalls[4];

	IMG_UINT32		ui32PrevTime;

	PVR_Temporal_Data	asTimes[GLES2_NUM_TIMERS]; 

	IMG_FLOAT		fCPUSpeed;

#endif /* defined(TIMING) || defined(DEBUG) */

#if defined(DEBUG)
	FILE *pShaderAnalysisHandle;
#endif

	IMG_UINT32	*pui32IndexData;
	IMG_VOID	*pvVertexData;

	IMG_UINT32	ui32VertexSize;
	IMG_UINT32	ui32VertexRCSize;
	IMG_UINT32	ui32VertexAlignSize;

	CircularBuffer *apsBuffers[CBUF_NUM_BUFFERS];

	IMG_UINT32 ui32FrameNum;

	GLES2SurfaceFlushList *psFlushList;
	IMG_UINT32 ui32NumUnflushedSurfaces;

#if defined(SGX_FEATURE_SW_VDM_CONTEXT_SWITCH)
	SmallKickTAData sSmallKickTA;
#endif

	/* linked-list of buffer devices */
	GLES2StreamDevice *psBufferDevice;

#if defined(PDUMP)
	IMG_BOOL   bPrevFrameDumped;
#endif

	GLESAppHints sAppHints;
	
	/* Used when app hint bDumpShaders is set */
	IMG_UINT32 ui32ShaderCount;	

	/* State that is shared among all contexts */
	GLES2ContextSharedState *psSharedState;

	/* PSP2-specific */

	IMG_PVOID pvUNCHeap;
	IMG_PVOID pvCDRAMHeap;
	IMG_PVOID pvUltRuntime;
	IMG_UINT32 ui32AsyncTexOpNum;
	IMG_BOOL bSwTexOpFin;
	SceUID hSwTexOpThrd;

}; /* The typedef is in ogles2_types.h */


#define GLES_ASSERT(expr) PVR_ASSERT(expr)

#define MIN(a,b) ((a)<(b)?(a):(b))

#define MAX(a,b) ((a)>(b)?(a):(b))

/******************************************************************************
	TLS Macros
******************************************************************************/
#define __GLES2_SET_CONTEXT(gc) \
	OGLES2_SetTLSValue((IMG_VOID *)(gc));

#define __GLES2_GET_CONTEXT() \
	GLES2Context *gc = (GLES2Context *)OGLES2_GetTLSValue(); \
	if(!gc) return;

#define __GLES2_GET_CONTEXT_RETURN(RetVal) \
	GLES2Context *gc = (GLES2Context *)OGLES2_GetTLSValue(); \
	if(!gc) return RetVal;

GLES2_MEMERROR SetupBGObject(GLES2Context *gc, IMG_BOOL bIsAccumulate, IMG_UINT32 *pui32PDSState);
IMG_BOOL PrepareToDraw(GLES2Context *gc, IMG_UINT32 *pui32ClearFlags, IMG_BOOL bTakeLock);
IMG_EGLERROR ScheduleTA(GLES2Context *gc, EGLRenderSurface *psRenderSurface, IMG_UINT32 ui32KickFlags);
IMG_VOID KickLimit_ScheduleTA(IMG_VOID *pvContext, IMG_BOOL bLastInScene);
IMG_VOID KickUnFlushed_ScheduleTA(IMG_VOID *pvContext, IMG_VOID *pvRenderSurface);

IMG_VOID CalcRegionClip(GLES2Context *gc, EGLRect *psRegion, IMG_UINT32 *pui32RegionClip);
GLES2_MEMERROR SendDrawMaskForClear(GLES2Context *gc);
GLES2_MEMERROR SendDrawMaskForPrimitive(GLES2Context *gc);
GLES2_MEMERROR SendDrawMaskRect(GLES2Context *gc, EGLRect *psRect, IMG_BOOL bIsEnable);

IMG_VOID WaitForTA(GLES2Context *gc);

#if defined(EGL_EXTENSION_KHR_IMAGE)
extern IMG_EGLERROR GLESGetImageSource(EGLContextHandle hContext, IMG_UINT32 ui32Source, IMG_UINT32 ui32Name, IMG_UINT32 ui32Level, EGLImage *psEGLImage);
#endif




#if defined(DEBUG)

#define GLES2ALLOCDEVICEMEM(psDevData, hDevMemHeap, ui32Attribs, ui32Size, ui32Alignment, ppsMemInfo) \
         KEGLAllocDeviceMemTrack(gc->psSysContext, __FILE__, __LINE__, psDevData, hDevMemHeap, ui32Attribs, ui32Size, ui32Alignment, ppsMemInfo)

#define GLES2FREEDEVICEMEM(psDevData, psMemInfo) \
         KEGLFreeDeviceMemTrack(gc->psSysContext, __FILE__, __LINE__, psDevData, psMemInfo)


#else /* defined(DEBUG) */

#define GLES2ALLOCDEVICEMEM(psDevData, hDevMemHeap, ui32Attribs, ui32Size, ui32Alignment, ppsMemInfo) \
         KEGLAllocDeviceMemPsp2(gc->psSysContext, psDevData, hDevMemHeap, ui32Attribs, ui32Size, ui32Alignment, ppsMemInfo)

#define GLES2FREEDEVICEMEM(psDevData, psMemInfo) \
         KEGLFreeDeviceMemPsp2(gc->psSysContext, psDevData, psMemInfo)

#endif /* defined(DEBUG) */


#define GLES2MallocHeapUNC(X,Y)	(IMG_VOID*)sceHeapAllocHeapMemory(X->pvUNCHeap, Y)
__inline IMG_VOID *GLES2MemalignHeapUNC(GLES2Context *gc, unsigned int size, unsigned int alignment)
{
	SceHeapAllocOptParam opt;
	opt.size = sizeof(SceHeapAllocOptParam);
	opt.alignment = alignment;
	return sceHeapAllocHeapMemoryWithOption(gc->pvUNCHeap, size, &opt);
}
__inline IMG_VOID *GLES2CallocHeapUNC(GLES2Context *gc, unsigned int size)
{
	IMG_VOID *ret = sceHeapAllocHeapMemory(gc->pvUNCHeap, size);
	sceClibMemset(ret, 0, size);
	return ret;
}
#define GLES2ReallocHeapUNC(X,Y,Z)	(IMG_VOID*)sceHeapReallocHeapMemory(X->pvUNCHeap, Y, Z)
#define GLES2FreeAsync(X,Y)					   texOpAsyncAddForCleanup(X, Y)
__inline IMG_VOID GLES2Free(GLES2Context *gc, void *mem)
{

#if defined(DEBUG)
	SceKernelMemBlockInfo sMemInfo;
	sMemInfo.size = sizeof(SceKernelMemBlockInfo);
	sMemInfo.type = SCE_KERNEL_MEMBLOCK_TYPE_USER_RW;
	IMG_INT32 ret = sceKernelGetMemBlockInfoByAddr(mem, &sMemInfo);
	if (ret == 0 && sMemInfo.type != SCE_KERNEL_MEMBLOCK_TYPE_USER_RW && !gc)
	{
		PVR_DPF((PVR_DBG_ERROR, "GLES2Free called on uncached memory, but gc is NULL!"));
		abort();
	}
#endif

	if (gc)
	{
		if (sceHeapFreeHeapMemory(gc->pvUNCHeap, mem) == SCE_HEAP_ERROR_INVALID_POINTER)
		{
			if (sceHeapFreeHeapMemory(gc->pvCDRAMHeap, mem) == SCE_HEAP_ERROR_INVALID_POINTER)
			{
				free(mem);
			}
		}
	}
	else
	{
		free(mem);
	}
}


#define GLES2MallocHeapCDRAM(X,Y)	(IMG_VOID*)sceHeapAllocHeapMemory(X->pvCDRAMHeap, Y)
__inline IMG_VOID *GLES2MemalignHeapCDRAM(GLES2Context *gc, unsigned int size, unsigned int alignment)
{
	SceHeapAllocOptParam opt;
	opt.size = sizeof(SceHeapAllocOptParam);
	opt.alignment = alignment;
	return sceHeapAllocHeapMemoryWithOption(gc->pvCDRAMHeap, size, &opt);
}
__inline IMG_VOID *GLES2CallocHeapCDRAM(GLES2Context *gc, unsigned int size)
{
	IMG_VOID *ret = sceHeapAllocHeapMemory(gc->pvCDRAMHeap, size);
	sceClibMemset(ret, 0, size);
	return ret;
}
#define GLES2ReallocHeapCDRAM(X,Y,Z)	(IMG_VOID*)sceHeapReallocHeapMemory(X->pvCDRAMHeap, Y, Z)


__inline PVRSRV_ERROR GLES2ALLOCDEVICEMEM_HEAP(GLES2Context *gc, IMG_UINT32 ui32Attribs, IMG_UINT32 ui32Size, IMG_UINT32 ui32Alignment, PVRSRV_CLIENT_MEM_INFO **ppsMemInfo)
{
	PVRSRV_CLIENT_MEM_INFO *psMemInfo;
	IMG_PVOID mem;

	if (ui32Attribs & PVRSRV_MAP_GC_MMU)
		mem = GLES2MemalignHeapCDRAM(gc, ui32Size, ui32Alignment);
	else
		mem = GLES2MemalignHeapUNC(gc, ui32Size, ui32Alignment);

	if (!mem)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	psMemInfo = GLES2Calloc(gc, sizeof(PVRSRV_CLIENT_MEM_INFO));

	if (!psMemInfo)
	{
		GLES2Free(gc, mem);
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	psMemInfo->pvLinAddr = mem;

	if (!(ui32Attribs & PVRSRV_MEM_NO_SYNCOBJ))
	{
		PVRSRVAllocSyncInfo(gc->ps3DDevData, &psMemInfo->psClientSyncInfo);
	}
	else
	{
		psMemInfo->psClientSyncInfo = IMG_NULL;
	}

	psMemInfo->hKernelMemInfo = 0;
	psMemInfo->psNext = IMG_NULL;
	psMemInfo->sDevVAddr.uiAddr = psMemInfo->pvLinAddr;
	psMemInfo->uAllocSize = ui32Size;
	psMemInfo->ui32Flags = ui32Attribs;

	*ppsMemInfo = psMemInfo;

	return PVRSRV_OK;
}

__inline PVRSRV_ERROR GLES2FREEDEVICEMEM_HEAP(GLES2Context *gc, PVRSRV_CLIENT_MEM_INFO *psMemInfo)
{
	if (psMemInfo->psClientSyncInfo)
	{
		PVRSRVFreeSyncInfo(gc->ps3DDevData, psMemInfo->psClientSyncInfo);
	}

	GLES2Free(gc, psMemInfo->pvLinAddr);
	GLES2Free(IMG_NULL, psMemInfo);

	return PVRSRV_OK;
}


#endif /* _CONTEXT_ */

/******************************************************************************
 End of file (context.h)
******************************************************************************/

