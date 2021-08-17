/******************************************************************************
 * Name         : context.h
 *
 * Copyright    : 2003-2007 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited, Home Park
 *              : Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * $Log: context.h $
 *****************************************************************************/

#ifndef _CONTEXT_
#define _CONTEXT_

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <math.h>

#include "psp2/libheap_custom.h"

#include "imgextensions.h"

#include "services.h"

#include "ogles_types.h"

#include "drvgl.h"
#include "drvglext.h"
#include "buffers.h"
#include "eglapi.h"

#include "sgxdefs.h"

#include "constants.h"
#include "names.h"
#include "kickresource.h"
#include "bufobj.h"
#include "fbo.h"
#include "pds.h"
#if defined(GLES1_EXTENSION_TEXTURE_STREAM) || defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
#include "texyuv.h"
#endif /* defined(GLES1_EXTENSION_TEXTURE_STREAM) || defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL) */
#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
#include "texstream.h"
#endif /* defined(GLES1_EXTENSION_TEXTURE_STREAM) */
#include "texture.h"
#include "tnlstate.h"
#include "lighting.h"
#include "state.h"
#include "usegles.h"
#include "codeheap.h"
#include "statehash.h"
#include "validate.h"
#include "metrics.h"
#include "profile.h"
#include "misc.h"
#include "osglue.h"
#include "pvr_debug.h"
#include "pvr_metrics.h"
#include "pdump.h"
#include "ffgen.h"
#include "fftnlgles.h"
#include "useasmgles.h"
#include "fftex.h"
#include "shader.h"
#include "fftexhw.h"
#include "gles1errata.h"
#include "usecodegen.h"
#include "usegen.h"
#include "sgxpixfmts.h"
#include "vertexarrobj.h"


#define GLES1_SCHEDULE_HW_LAST_IN_SCENE         0x00000001
#define GLES1_SCHEDULE_HW_WAIT_FOR_TA           0x00000002
#define GLES1_SCHEDULE_HW_WAIT_FOR_3D           0x00000004
#define GLES1_SCHEDULE_HW_DISCARD_SCENE         0x00000008
#define GLES1_SCHEDULE_HW_BBOX_RENDER			0x00000010
#define GLES1_SCHEDULE_HW_POST_BBOX_RENDER		0x00000020
#define GLES1_SCHEDULE_HW_IGNORE_FLUSHLIST		0x00000040
#define GLES1_SCHEDULE_HW_MIDSCENE_RENDER		0x00000080


/**********************************************************
 * State that affects rasterisation, can be handled by HW
 * and can be properly controlled per-object
 **********************************************************/
#define GLES1_RS_ALPHABLEND				0
#define GLES1_RS_ALPHATEST				1
#define GLES1_RS_LOGICOP				2
#define GLES1_RS_STENCILTEST			3
#define GLES1_RS_2DTEXTURE0				4
#define GLES1_RS_2DTEXTURE1				5
#define GLES1_RS_2DTEXTURE2				6
#define GLES1_RS_2DTEXTURE3				7
#define GLES1_RS_DEPTHTEST				8
#define GLES1_RS_POLYOFFSET				9
#define GLES1_RS_FOG					10
#define GLES1_RS_LINESMOOTH				11
#define GLES1_RS_POINTSMOOTH			12

#define GLES1_RS_CEMTEXTURE0			13
#define GLES1_RS_CEMTEXTURE1			14
#define GLES1_RS_CEMTEXTURE2			15
#define GLES1_RS_CEMTEXTURE3			16

#define GLES1_RS_GENTEXTURE0			17
#define GLES1_RS_GENTEXTURE1			18
#define GLES1_RS_GENTEXTURE2			19
#define GLES1_RS_GENTEXTURE3			20

#define GLES1_RS_TEXTURE0_STREAM		21
#define GLES1_RS_TEXTURE1_STREAM		22
#define GLES1_RS_TEXTURE2_STREAM		23
#define GLES1_RS_TEXTURE3_STREAM		24

	#define GLES1_NUMBER_RASTER_STATES		25

#define GLES1_RS_ALPHABLEND_ENABLE		(1UL << GLES1_RS_ALPHABLEND)
#define GLES1_RS_ALPHATEST_ENABLE		(1UL << GLES1_RS_ALPHATEST)
#define GLES1_RS_LOGICOP_ENABLE			(1UL << GLES1_RS_LOGICOP)
#define GLES1_RS_STENCILTEST_ENABLE		(1UL << GLES1_RS_STENCILTEST)
#define GLES1_RS_2DTEXTURE0_ENABLE		(1UL << GLES1_RS_2DTEXTURE0)
#define GLES1_RS_2DTEXTURE1_ENABLE		(1UL << GLES1_RS_2DTEXTURE1)
#define GLES1_RS_2DTEXTURE2_ENABLE		(1UL << GLES1_RS_2DTEXTURE2)
#define GLES1_RS_2DTEXTURE3_ENABLE		(1UL << GLES1_RS_2DTEXTURE3)
#define GLES1_RS_DEPTHTEST_ENABLE		(1UL << GLES1_RS_DEPTHTEST)
#define GLES1_RS_POLYOFFSET_ENABLE		(1UL << GLES1_RS_POLYOFFSET)
#define GLES1_RS_FOG_ENABLE				(1UL << GLES1_RS_FOG)
#define GLES1_RS_LINESMOOTH_ENABLE		(1UL << GLES1_RS_LINESMOOTH)
#define GLES1_RS_POINTSMOOTH_ENABLE		(1UL << GLES1_RS_POINTSMOOTH)

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
#define GLES1_RS_CEMTEXTURE0_ENABLE		(1UL << GLES1_RS_CEMTEXTURE0)
#define GLES1_RS_CEMTEXTURE1_ENABLE		(1UL << GLES1_RS_CEMTEXTURE1)
#define GLES1_RS_CEMTEXTURE2_ENABLE		(1UL << GLES1_RS_CEMTEXTURE2)
#define GLES1_RS_CENTEXTURE3_ENABLE		(1UL << GLES1_RS_CEMTEXTURE3)
#define GLES1_RS_GENTEXTURE0_ENABLE		(1UL << GLES1_RS_GENTEXTURE0)
#define GLES1_RS_GENTEXTURE1_ENABLE		(1UL << GLES1_RS_GENTEXTURE1)
#define GLES1_RS_GENTEXTURE2_ENABLE		(1UL << GLES1_RS_GENTEXTURE2)
#define GLES1_RS_GENTEXTURE3_ENABLE		(1UL << GLES1_RS_GENTEXTURE3)
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */

#if defined(GLES1_EXTENSION_TEXTURE_STREAM) || defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
#define GLES1_RS_TEXTURE0_STREAM_ENABLE	(1UL << GLES1_RS_TEXTURE0_STREAM)
#define GLES1_RS_TEXTURE1_STREAM_ENABLE	(1UL << GLES1_RS_TEXTURE1_STREAM)
#define GLES1_RS_TEXTURE2_STREAM_ENABLE	(1UL << GLES1_RS_TEXTURE2_STREAM)
#define GLES1_RS_TEXTURE3_STREAM_ENABLE	(1UL << GLES1_RS_TEXTURE3_STREAM)
#endif /* defined(GLES1_EXTENSION_TEXTURE_STREAM)  || defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)*/


/**********************************************************
 * State that affects rasterisation, can be handled by HW
 * but cannot be properly controlled per-object ...
 * so is handled per frame or with special objects
 **********************************************************/
#define GLES1_FS_DITHER					0
#define GLES1_FS_MULTISAMPLE			1
#define GLES1_FS_SCISSOR				2

#define GLES1_NUMBER_FRAME_STATES		3

#define GLES1_FS_DITHER_ENABLE			(1UL << GLES1_FS_DITHER)
#define GLES1_FS_MULTISAMPLE_ENABLE		(1UL << GLES1_FS_MULTISAMPLE)
#define GLES1_FS_SCISSOR_ENABLE			(1UL << GLES1_FS_SCISSOR)


/**********************************************************
 * Raster state that can't be handled in hardware, but
 * will be tracked atleast
 **********************************************************/
#define GLES1_IG_MSALPHACOV				0
#define GLES1_IG_MSSAMPALPHA			1
#define GLES1_IG_MSSAMPCOV				2

#define GLES1_NUMBER_IGNORE_STATES      3

#define GLES1_IG_MSALPHACOV_ENABLE 		(1UL << GLES1_IG_MSALPHACOV)
#define GLES1_IG_MSSAMPALPHA_ENABLE		(1UL << GLES1_IG_MSSAMPALPHA)
#define GLES1_IG_MSSAMPCOV_ENABLE		(1UL << GLES1_IG_MSSAMPCOV)


/**********************************************************
 * State that affects transform and lighting
 **********************************************************/
#define GLES1_TL_LIGHT0					0
#define GLES1_TL_LIGHT1					1
#define GLES1_TL_LIGHT2					2
#define GLES1_TL_LIGHT3					3
#define GLES1_TL_LIGHT4					4
#define GLES1_TL_LIGHT5					5
#define GLES1_TL_LIGHT6					6
#define GLES1_TL_LIGHT7					7
#define GLES1_TL_LIGHTING				8
#define GLES1_TL_RESCALE				9
#define GLES1_TL_COLORMAT				10
#define GLES1_TL_NORMALIZE				11
#define GLES1_TL_CULLFACE				12
#define GLES1_TL_CLIP_PLANE0			13
#define GLES1_TL_CLIP_PLANE1			14
#define GLES1_TL_CLIP_PLANE2			15
#define GLES1_TL_CLIP_PLANE3			16
#define GLES1_TL_CLIP_PLANE4			17
#define GLES1_TL_CLIP_PLANE5			18
#define GLES1_TL_POINTSPRITE			19
#define GLES1_TL_MATRIXPALETTE			20

#define GLES1_NUMBER_TNL_STATES			21

#define GLES1_TL_LIGHT0_ENABLE			(1UL << GLES1_TL_LIGHT0)
#define GLES1_TL_LIGHT1_ENABLE			(1UL << GLES1_TL_LIGHT1)
#define GLES1_TL_LIGHT2_ENABLE			(1UL << GLES1_TL_LIGHT2)
#define GLES1_TL_LIGHT3_ENABLE			(1UL << GLES1_TL_LIGHT3)
#define GLES1_TL_LIGHT4_ENABLE			(1UL << GLES1_TL_LIGHT4)
#define GLES1_TL_LIGHT5_ENABLE			(1UL << GLES1_TL_LIGHT5)
#define GLES1_TL_LIGHT6_ENABLE			(1UL << GLES1_TL_LIGHT6)
#define GLES1_TL_LIGHT7_ENABLE			(1UL << GLES1_TL_LIGHT7)
#define GLES1_TL_LIGHTING_ENABLE		(1UL << GLES1_TL_LIGHTING)
#define GLES1_TL_RESCALE_ENABLE			(1UL << GLES1_TL_RESCALE)
#define GLES1_TL_COLORMAT_ENABLE		(1UL << GLES1_TL_COLORMAT)
#define GLES1_TL_NORMALIZE_ENABLE		(1UL << GLES1_TL_NORMALIZE)
#define GLES1_TL_CULLFACE_ENABLE		(1UL << GLES1_TL_CULLFACE)
#define GLES1_TL_CLIP_PLANE0_ENABLE		(1UL << GLES1_TL_CLIP_PLANE0)
#define GLES1_TL_CLIP_PLANE1_ENABLE		(1UL << GLES1_TL_CLIP_PLANE1)
#define GLES1_TL_CLIP_PLANE2_ENABLE		(1UL << GLES1_TL_CLIP_PLANE2)
#define GLES1_TL_CLIP_PLANE3_ENABLE		(1UL << GLES1_TL_CLIP_PLANE3)
#define GLES1_TL_CLIP_PLANE4_ENABLE		(1UL << GLES1_TL_CLIP_PLANE4)
#define GLES1_TL_CLIP_PLANE5_ENABLE		(1UL << GLES1_TL_CLIP_PLANE5)
#define GLES1_TL_POINTSPRITE_ENABLE		(1UL << GLES1_TL_POINTSPRITE)
#define GLES1_TL_MATRIXPALETTE_ENABLE	(1UL << GLES1_TL_MATRIXPALETTE)

#define GLES1_TL_LIGHTS_ENABLE	 (	GLES1_TL_LIGHT0_ENABLE | GLES1_TL_LIGHT1_ENABLE | \
									GLES1_TL_LIGHT2_ENABLE | GLES1_TL_LIGHT3_ENABLE | \
									GLES1_TL_LIGHT4_ENABLE | GLES1_TL_LIGHT5_ENABLE | \
									GLES1_TL_LIGHT6_ENABLE | GLES1_TL_LIGHT7_ENABLE)

#define GLES1_TL_CLIP_PLANE0_SHIFT       GLES1_TL_CLIP_PLANE0

#define GLES1_TL_CLIP_PLANES_ENABLE (GLES1_TL_CLIP_PLANE0_ENABLE | GLES1_TL_CLIP_PLANE1_ENABLE | \
									GLES1_TL_CLIP_PLANE2_ENABLE | GLES1_TL_CLIP_PLANE3_ENABLE | \
									GLES1_TL_CLIP_PLANE4_ENABLE | GLES1_TL_CLIP_PLANE5_ENABLE)



/*****************************************************************************/
#define GLES1_DIRTY_VGP_CONSTANT(gc, constant)

/*
** Polygon machine state.  Contains all the polygon specific state,
** as well as procedure pointers used during rendering operations.
*/
typedef struct GLESPolygonMachineRec {

	/*
	** Lookup table that returns the face (0=front, 1=back) when indexed
	** by a flag which is zero for CW and 1 for CCW.  If FrontFace is CW:
	** 	face[0] = 0
	** 	face[1] = 1
	** else
	** 	face[0] = 1
	** 	face[1] = 0
	*/
	IMG_UINT32 aui32Face[2];

	/*
	** Culling flag.  0 when culling the front face, 1 when culling the
	** back face and 2 when not culling.
	*/
	IMG_UINT32 ui32CullFlag;

} GLESPolygonMachine;

/* defines for above ui32CullFlag */
#define GLES1_CULLFACE_FRONTFACE	0
#define GLES1_CULLFACE_BACKFACE		1

#define GLES1_CULLFLAG_FRONT		GLES1_CULLFACE_FRONTFACE
#define GLES1_CULLFLAG_BACK			GLES1_CULLFACE_BACKFACE
#define GLES1_CULLFLAG_DONT			2
#define GLES1_CULLFLAG_BOTH			3

/* Indices for aui32Face[] array above */
#define GLES1_WINDING_CW			0
#define GLES1_WINDING_CCW			1


typedef struct GLES1SurfaceFlushListTAG
{
	EGLRenderSurface *psRenderSurface;
	GLESTexture *psTex;
	GLES1Context *gc;

	struct GLES1SurfaceFlushListTAG *psNext;

}GLES1SurfaceFlushList;

/******************************************************************************/

typedef struct GLES1ContextSharedState_TAG
{
	/* Reference count. Update only within a critical section. Init with one, free when one. */
	IMG_UINT32           ui32RefCount;

	/* Keeps track of what textures are attached to any given surface */
	GLES1TextureManager  *psTextureManager;

	/* Keeps track of what PDS code variants are attached to any given surface */
	KRMKickResourceManager sPDSVariantKRM;

	/* Keeps track of what USSE code variants are attached to any given surface */
	KRMKickResourceManager sUSEShaderVariantKRM;

	/* Keeps track of which buffer objects are attached to any given context */
	KRMKickResourceManager sBufferObjectKRM;

	/* Dictionaries of GL objects addressed by name. */
	GLES1NamesArray      *apsNamesArray[GLES1_MAX_SHAREABLE_NAMETYPE]; 

	/* Memory heap for vertex and fragment code blocks */
	UCH_UseCodeHeap     *psUSEVertexCodeHeap;
	UCH_UseCodeHeap     *psUSEFragmentCodeHeap;
	UCH_UseCodeHeap     *psPDSFragmentCodeHeap;
	UCH_UseCodeHeap     *psPDSVertexCodeHeap;
	
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
	
	GLES1SurfaceFlushList *psFlushList;
	PVRSRV_MUTEX_HANDLE hFlushListLock;

#ifdef PDUMP
	IMG_BOOL bMustDumpSequentialStaticIndices;
	IMG_BOOL bMustDumpLineStripStaticIndices;
#endif

} GLES1ContextSharedState;


/*****************************************************************************/

struct GLES1Context_TAG
{
	PVRSRV_DEV_DATA *ps3DDevData;
	SrvSysContext 	*psSysContext;
	SGX_KICKTA 		sKickTA;
	IMG_BOOL 		bSPMOutOfMemEvent;

	KRMStatusUpdate sKRMTAStatusUpdate;

	GLES1ProgramMachine sProgram;
		
	IMG_UINT32 ui32NumImageUnitsActive;
	IMG_UINT32 ui32ImageUnitEnables;	/* Bit field indicating which image units are enabled */
	IMG_UINT32 ui32TexImageUnitsEnabled[GLES1_MAX_TEXTURE_UNITS];

	IMG_UINT32 ui32RasterEnables;
	IMG_UINT32 ui32TnLEnables;
	IMG_UINT32 ui32FrameEnables;
	IMG_UINT32 ui32IgnoredEnables;
	/************************************************************************/

	/*
	** All of the current user controllable state is resident here.
	*/
	GLESState sState;

	/************************************************************************/

	/*
	** Most recent error code, or GL_NO_ERROR if no error has occurred
	** since the last glGetError.
	*/
	IMG_INT32 i32Error;


	/************************************************************************/

	/*
	** Mask words of dirty bits
	*/
	IMG_UINT32 ui32DirtyMask;
	IMG_UINT32 ui32EmitMask;


	/*	Function pointers that are mode dependent */
	GLESProcs sProcs; 

	/* Machine structures defining software rendering "machine" state */
	GLESLightMachine sLight;
	GLESPolygonMachine sPolygon;
	GLESTextureMachine sTexture;
	GLES1TransformMachine sTransform;

    GLES1VertexArrayObjectMachine sVAOMachine;
	/* Keeps track of which VAOs are attached to any given context */
	KRMKickResourceManager sVAOKRM;

	GLESBufferObjectMachine sBufferObject;
	GLESFrameBufferObjectMachine sFrameBuffer;

	GLESPrimitiveMachine sPrim;

#if defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT)
	/* Dictionaries of GL objects addressed by name. */
	GLES1NamesArray *apsNamesArray[GLES1_MAX_UNSHAREABLE_NAMETYPE]; 
#endif /* defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT) */

	/*
	** Mode information that describes the kind of buffers and rendering
	** modes that this context manages.
	*/
	EGLcontextMode *psMode;

	EGLDrawableParams *psDrawParams;
	EGLDrawableParams *psReadParams;

	EGLRenderSurface  *psRenderSurface;
	EGLDrawableHandle  hEGLSurface;
	
	IMG_BOOL bFullScreenViewport;
	IMG_BOOL bFullScreenScissor;

	IMG_BOOL bDrawMaskInvalid;

	IMG_CHAR *pszExtensions;

	IMG_BOOL bHasBeenCurrent;

#if defined(GLES1_EXTENSION_EGL_IMAGE)
	IMG_UINT32 ui32NumEGLImageTextureTargetsBound;
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */

	GLESAppHints		sAppHints;

#if defined(TIMING) || defined(DEBUG)

	IMG_UINT32 		ui32RedundantStatesRaster[GLES1_NUMBER_RASTER_STATES];
	IMG_UINT32 		ui32RedundantStatesTnL[GLES1_NUMBER_TNL_STATES];
	IMG_UINT32 		ui32RedundantStatesFrame[GLES1_NUMBER_FRAME_STATES];
	IMG_UINT32 		ui32RedundantStatesIgnored[GLES1_NUMBER_IGNORE_STATES];
	IMG_UINT32 		ui32ValidStatesRaster[GLES1_NUMBER_RASTER_STATES];
	IMG_UINT32 		ui32ValidStatesTnL[GLES1_NUMBER_TNL_STATES];
	IMG_UINT32 		ui32ValidStatesFrame[GLES1_NUMBER_FRAME_STATES];
	IMG_UINT32 		ui32ValidStatesIgnored[GLES1_NUMBER_IGNORE_STATES];

	StateMetricData	*psStateMetricData;

	IMG_UINT32 		ui32ValidEnables[GLES1_NUMBER_PROFILE_ENABLES];
	IMG_UINT32 		ui32RedundantEnables[GLES1_NUMBER_PROFILE_ENABLES];

	IMG_UINT32 		ui32VBOMemCurrent;
	IMG_UINT32		ui32VBOHighWaterMark;
	IMG_UINT32		ui32VBONamesGenerated;

	DrawCallData	sDrawArraysCalls[2];
	DrawCallData	sDrawElementsCalls[4];

	IMG_UINT32		ui32PrevTime;

	PVR_Temporal_Data	asTimes[GLES1_NUM_TIMERS];

	IMG_FLOAT		fCPUSpeed;

	IMG_VOID  *pvFileInfo;
#endif /* defined(TIMING) || defined(DEBUG) */


	IMG_UINT32	*pui32IndexData;
	IMG_VOID	*pvVertexData;

	IMG_UINT32	ui32VertexSize;
	IMG_UINT32	ui32VertexRCSize;
	IMG_UINT32	ui32VertexAlignSize;

	CircularBuffer *apsBuffers[CBUF_NUM_BUFFERS];

	IMG_UINT32 ui32FrameNum;


#if defined(SGX_FEATURE_SW_VDM_CONTEXT_SWITCH)
	SmallKickTAData sSmallKickTA;
#endif

#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
	/* linked-list of buffer devices */
	GLES1StreamDevice *psBufferDevice;
#endif
	
#if defined(PDUMP)
	IMG_BOOL   bPrevFrameDumped;
#endif

	/* State that is shared among all contexts */
	GLES1ContextSharedState *psSharedState;

	/* PSP2-specific */

	IMG_PVOID pvUNCHeap;
	IMG_PVOID pvCDRAMHeap;
	IMG_PVOID pvUltRuntime;
	IMG_UINT32 ui32AsyncTexOpNum;
	IMG_BOOL bSwTexOpFin;
	SceUID hSwTexOpThrd;
};


#define GLES1_ASSERT(n) PVR_ASSERT(n)

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))


#define __GLES1_SET_CONTEXT(gc) \
	OGL_SetTLSValue((IMG_VOID *)(gc));

#define __GLES1_GET_CONTEXT() \
	GLES1Context *gc = (GLES1Context *)OGL_GetTLSValue(); \
	if(!gc) return

#define __GLES1_GET_CONTEXT_RETURN(RetVal) \
	GLES1Context *gc = (GLES1Context *)OGL_GetTLSValue(); \
	if(!gc) return RetVal;

/* pixelops.c */
IMG_BOOL SetupReadPixelsSpanInfo(GLES1Context *gc, GLESPixelSpanInfo *psSpanInfo, IMG_INT32 i32X, IMG_INT32 i32Y, 
								IMG_UINT32 ui32Width, IMG_UINT32 ui32Height, GLenum format, GLenum eType, 
								IMG_BOOL bUsePackAlignment, EGLDrawableParams *psReadParams);
IMG_BOOL ClipReadPixels(GLESPixelSpanInfo *psSpanInfo, EGLDrawableParams *psReadParams);

IMG_VOID *GetStridedSurfaceData(GLES1Context *gc, EGLDrawableParams *psReadParams, GLESPixelSpanInfo *psSpanInfo);

/* tnl */
IMG_VOID Materialfv(GLES1Context *gc, GLenum ui32Face, GLenum ePname, const IMG_FLOAT *pfParams);

/* validate.c */
IMG_VOID ValidateTextureState(GLES1Context *gc);
IMG_VOID ValidatePointLineSize(GLES1Context *gc);

/* sgxif.c */
IMG_BOOL PrepareToDraw(GLES1Context *gc, IMG_UINT32 *pui32ClearFlags, IMG_BOOL bTakeLock);
IMG_EGLERROR ScheduleTA(GLES1Context *gc, EGLRenderSurface *psRenderSurface, IMG_UINT32 ui32KickFlags);
IMG_VOID KickLimit_ScheduleTA(IMG_VOID *pvContext, IMG_BOOL bLastInScene);
IMG_VOID KickUnFlushed_ScheduleTA(IMG_VOID *pvContext, IMG_VOID *pvRenderSurface);
IMG_VOID WaitForTA(GLES1Context *gc);

/* accum.c */
GLES1_MEMERROR SetupBGObject(GLES1Context *gc, IMG_BOOL bIsAccumulate, IMG_UINT32 *pui32PDSState);

/* scissor.c */
IMG_VOID CalcRegionClip(GLES1Context *gc, EGLRect *psRegion, IMG_UINT32 *pui32RegionClip);
GLES1_MEMERROR SendDrawMaskForClear(GLES1Context *gc);
GLES1_MEMERROR SendDrawMaskForPrimitive(GLES1Context *gc);
GLES1_MEMERROR SendDrawMaskRect(GLES1Context *gc, EGLRect *psRect, IMG_BOOL bIsEnable);

/* drawvarray.c */
IMG_VOID AttachAllUsedResourcesToCurrentSurface(GLES1Context *gc);

/* eglimage.c */
#if defined(EGL_EXTENSION_KHR_IMAGE)
IMG_EGLERROR GLESGetImageSource(EGLContextHandle hContext, IMG_UINT32 ui32Source, IMG_UINT32 ui32Name, IMG_UINT32 ui32Level, EGLImage *psEGLImage);
#endif



#if defined(DEBUG)

#define GLES1ALLOCDEVICEMEM(psDevData, hDevMemHeap, ui32Attribs, ui32Size, ui32Alignment, ppsMemInfo) \
         KEGLAllocDeviceMemTrack(gc->psSysContext, __FILE__, __LINE__, psDevData, hDevMemHeap, ui32Attribs, ui32Size, ui32Alignment, ppsMemInfo)

#define GLES1FREEDEVICEMEM(psDevData, psMemInfo) \
         KEGLFreeDeviceMemTrack(gc->psSysContext, __FILE__, __LINE__, psDevData, psMemInfo)


#else /* defined(DEBUG) */

#define GLES1ALLOCDEVICEMEM(psDevData, hDevMemHeap, ui32Attribs, ui32Size, ui32Alignment, ppsMemInfo) \
         KEGLAllocDeviceMemPsp2(gc->psSysContext, psDevData, hDevMemHeap, ui32Attribs, ui32Size, ui32Alignment, ppsMemInfo)

#define GLES1FREEDEVICEMEM(psDevData, psMemInfo) \
         KEGLFreeDeviceMemPsp2(gc->psSysContext, psDevData, psMemInfo)

#endif /* defined(DEBUG) */


#define GLES1MallocHeapUNC(X,Y)	(IMG_VOID*)sceHeapAllocHeapMemory(X->pvUNCHeap, Y)
__inline IMG_VOID *GLES1MemalignHeapUNC(GLES1Context *gc, unsigned int size, unsigned int alignment)
{
	SceHeapAllocOptParam opt;
	opt.size = sizeof(SceHeapAllocOptParam);
	opt.alignment = alignment;
	return sceHeapAllocHeapMemoryWithOption(gc->pvUNCHeap, size, &opt);
}
__inline IMG_VOID *GLES1CallocHeapUNC(GLES1Context *gc, unsigned int size)
{
	IMG_VOID *ret = sceHeapAllocHeapMemory(gc->pvUNCHeap, size);
	sceClibMemset(ret, 0, size);
	return ret;
}
#define GLES1ReallocHeapUNC(X,Y,Z)	(IMG_VOID*)sceHeapReallocHeapMemory(X->pvUNCHeap, Y, Z)
#define GLES1FreeAsync(X,Y)					   texOpAsyncAddForCleanup(X, Y)
__inline IMG_VOID GLES1Free(GLES1Context *gc, void *mem)
{

#if defined(DEBUG)
	SceKernelMemBlockInfo sMemInfo;
	sMemInfo.size = sizeof(SceKernelMemBlockInfo);
	sMemInfo.type = 0;
	sceKernelGetMemBlockInfoByAddr(mem, &sMemInfo);
	if (sMemInfo.type != SCE_KERNEL_MEMBLOCK_TYPE_USER_RW && !gc)
	{
		PVR_DPF((PVR_DBG_WARNING, "GLES1Free called on uncached memory, but gc is NULL!"));
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


#define GLES1MallocHeapCDRAM(X,Y)	(IMG_VOID*)sceHeapAllocHeapMemory(X->pvCDRAMHeap, Y)
__inline IMG_VOID *GLES1MemalignHeapCDRAM(GLES1Context *gc, unsigned int size, unsigned int alignment)
{
	SceHeapAllocOptParam opt;
	opt.size = sizeof(SceHeapAllocOptParam);
	opt.alignment = alignment;
	return sceHeapAllocHeapMemoryWithOption(gc->pvCDRAMHeap, size, &opt);
}
__inline IMG_VOID *GLES1CallocHeapCDRAM(GLES1Context *gc, unsigned int size)
{
	IMG_VOID *ret = sceHeapAllocHeapMemory(gc->pvCDRAMHeap, size);
	sceClibMemset(ret, 0, size);
	return ret;
}
#define GLES1ReallocHeapCDRAM(X,Y,Z)	(IMG_VOID*)sceHeapReallocHeapMemory(X->pvCDRAMHeap, Y, Z)


__inline PVRSRV_ERROR GLES1ALLOCDEVICEMEM_HEAP(GLES1Context *gc, IMG_UINT32 ui32Attribs, IMG_UINT32 ui32Size, IMG_UINT32 ui32Alignment, PVRSRV_CLIENT_MEM_INFO **ppsMemInfo)
{
	PVRSRV_CLIENT_MEM_INFO *psMemInfo;
	IMG_PVOID mem;

	if (ui32Attribs & PVRSRV_MAP_GC_MMU)
		mem = GLES1MemalignHeapCDRAM(gc, ui32Size, ui32Alignment);
	else
		mem = GLES1MemalignHeapUNC(gc, ui32Size, ui32Alignment);

	if (!mem)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	psMemInfo = GLES1Calloc(gc, sizeof(PVRSRV_CLIENT_MEM_INFO));
	if (!psMemInfo)
	{
		GLES1Free(gc, mem);
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

__inline PVRSRV_ERROR GLES1FREEDEVICEMEM_HEAP(GLES1Context *gc, PVRSRV_CLIENT_MEM_INFO *psMemInfo)
{
	if (psMemInfo->psClientSyncInfo)
	{
		PVRSRVFreeSyncInfo(gc->ps3DDevData, psMemInfo->psClientSyncInfo);
	}

	GLES1Free(gc, psMemInfo->pvLinAddr);
	GLES1Free(IMG_NULL, psMemInfo);

	return PVRSRV_OK;
}


#endif /* _CONTEXT_ */

/**************************************************************************
 End of file (context.h)
**************************************************************************/


