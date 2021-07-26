/*************************************************************************/ /*!
@File           eglapi.h
@Title          imgegl / KEGL API for drivers
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    imgegl / KEGL API for drivers
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef _EGLAPI_H_
#define _EGLAPI_H_

#include "drvegl.h"
#include "drveglext.h"
#include "services.h"
#if defined(SUPPORT_SGX)
#include "sgxapi.h"
#endif
#if defined(SUPPORT_VGX)
#include "vgxapi.h"
#endif
#include "servicesext.h"
#include "srvcontext.h"
#if defined(SUPPORT_SGX)
#include "pds.h"
#include "pixevent.h"
#include "buffers.h"
#include "kickresource.h"
#endif
#include "common_tls.h"
#include "imgextensions.h"


#if defined (__cplusplus)
extern "C" {
#endif

#if defined(SUPPORT_VGX)
#define EGL_MAX_SRC_SYNCS VGX_MAX_SRC_SYNCS
#endif

#if defined(SUPPORT_SGX)
#define EGL_MAX_SRC_SYNCS SGX_MAX_SRC_SYNCS
#endif

#if defined(SUPPORT_VGX)
/**************************************************************
 *
 *  External VGX Stencil Buffer buffer control apphint values
 *
 **************************************************************/
#define EXTERNAL_VGX_STENCIL_BUFFER_MODE_ALLOC_ONDEMAND			0
#define EXTERNAL_VGX_STENCIL_BUFFER_MODE_ALLOC_NOAA_UPFRONT		1
#define EXTERNAL_VGX_STENCIL_BUFFER_MODE_ALLOC_4X_UPFRONT		2
#define EXTERNAL_VGX_STENCIL_BUFFER_MODE_ALLOC_8X_UPFRONT		3


#define EXTERNAL_VGX_STENCIL_MODE_DEFAULT						EXTERNAL_VGX_STENCIL_BUFFER_MODE_ALLOC_ONDEMAND

#endif

/**************************************************************
 *
 *         External Z buffer control apphint values
 *
 **************************************************************/
#define EXTERNAL_ZBUFFER_MODE_ALLOC_ONDEMAND_USED_ALWAYS		0
#define EXTERNAL_ZBUFFER_MODE_ALLOC_ONDEMAND_USED_ASNEEDED		1

#define EXTERNAL_ZBUFFER_MODE_ALLOC_UPFRONT_USED_ALWAYS			2
#define EXTERNAL_ZBUFFER_MODE_ALLOC_UPFRONT_USED_ASNEEDED		3

#define EXTERNAL_ZBUFFER_MODE_ALLOC_NEVER_USED_NEVER			4


#define EXTERNAL_ZBUFFER_MODE_DEFAULT							EXTERNAL_ZBUFFER_MODE_ALLOC_ONDEMAND_USED_ASNEEDED




/**************************************************************
 *
 *                       Generic
 *
 **************************************************************/
typedef IMG_VOID (IMG_CALLCONV *GPA_PROC)(IMG_VOID);

/**************************************************************
 *
 *                       EGL --> OpenGL
 *
 **************************************************************/
#define IMG_ANTIALIAS_NONE		0x00000000
#define IMG_ANTIALIAS_2x1		0x00000001
#define IMG_ANTIALIAS_2x2		0x00000002

#define IMG_MIN_RENDER_TEXTURE_LEVEL		4
#define IMG_MAX_TEXTURE_MIPMAP_LEVELS		11

typedef IMG_VOID* EGLDrawableHandle;
typedef IMG_VOID* EGLContextHandle;
typedef IMG_VOID* EGLRenderSurfaceHandle;

typedef struct EGLRect_TAG
{
	IMG_INT32  i32X;
	IMG_INT32  i32Y;
	IMG_UINT32 ui32Width;
	IMG_UINT32 ui32Height;

} EGLRect;

typedef enum
{
	EGL_DRAWABLETYPE_WINDOW	 = 0,
	EGL_DRAWABLETYPE_PIXMAP	 = 1,
	EGL_DRAWABLETYPE_PBUFFER = 2

} EGLDrawableType;

#if defined(SUPPORT_SGX)

typedef struct EGLPixelBEState_TAG
{
	/*
		Pixel BE sideband word
	*/
	IMG_UINT32			ui32SidebandWord;

	/* Emit word index
			0			1st Emit Src0           (PIXELBE1S0)
			1			1st Emit Src1           (PIXELBE1S1)
			2			2nd Emit Src0           (PIXELBE2S0)
			3			2nd Emit Src1           (PIXELBE2S1)
            4           2nd Emit Src2           (PIXELBE2S2)  // if defined (SGX_FEATURE_PIXELBE_32K_LINESTRIDE)
	*/
	IMG_UINT32			aui32EmitWords[STATE_PBE_DWORDS];

	PVRSRV_HWREG		sEventPixelExec;
	PVRSRV_HWREG		sEventPixelData;
	PVRSRV_HWREG		sEventPixelInfo;

	IMG_BOOL			bDither;

	IMG_UINT32			*pui32PixelEventPDS;
	IMG_UINT32			*pui32PixelEventUSSE;

} EGLPixelBEState;

#endif

/* Moved from the context */
#define GLES_SET_REGISTER(REG, ADDRESS, VALUE)	\
	REG.ui32RegAddr = (ADDRESS);				\
	REG.ui32RegVal = (VALUE);

typedef struct GLESTARegisters_TAG
{
	PVRSRV_HWREG	sMTEControl;

} GLESTARegisters;

#define GLES_COUNT_TAREG (sizeof(GLESTARegisters) / sizeof(PVRSRV_HWREG))

typedef struct GLES3DRegisters_TAG
{
	PVRSRV_HWREG	sPixelBE;
	PVRSRV_HWREG	sISPIPFMisc;
#if defined (SGX545)
    PVRSRV_HWREG    sISPFPUCtrl;
#else /* defined (SGX545) */
	PVRSRV_HWREG	sISPPerpendicular;
	PVRSRV_HWREG	sISPCullValue;
#endif /* defined (SGX545) */
	PVRSRV_HWREG	sISPDepthBias;
#if defined(SGX_FEATURE_DEPTH_BIAS_OBJECTS)
	PVRSRV_HWREG	sISPDepthBias1;
#endif
	PVRSRV_HWREG	sISPZLSControl;
	PVRSRV_HWREG	sISPZLoadBase;
	PVRSRV_HWREG	sISPZStoreBase;
	PVRSRV_HWREG	sISPStencilLoadBase;
	PVRSRV_HWREG	sISPStencilStoreBase;
	PVRSRV_HWREG	sISPBackgroundObject;
	PVRSRV_HWREG	sISPBackgroundObjectDepth;
	PVRSRV_HWREG	sEDMPixelPDSExec;
	PVRSRV_HWREG	sEDMPixelPDSData;
	PVRSRV_HWREG	sEDMPixelPDSInfo;

} GLES3DRegisters;

#define GLES_COUNT_3DREG (sizeof(GLES3DRegisters) / sizeof(PVRSRV_HWREG))
/* End moved from the context */


#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
#if defined(SGX_FEATURE_VCB)
#if defined(FIX_HW_BRN_31988)
#define USE_TERMINATE_DWORDS		(9 * EURASIA_USE_INSTRUCTION_SIZE/sizeof(IMG_UINT32))  /* + PHAS + WOP + EMITVCB*/
#else
#define USE_TERMINATE_DWORDS		(5 * EURASIA_USE_INSTRUCTION_SIZE/sizeof(IMG_UINT32))  /* + PHAS + WOP + EMITVCB*/
#endif
#else
#if defined(FIX_HW_BRN_31988)
#define USE_TERMINATE_DWORDS		(7 * EURASIA_USE_INSTRUCTION_SIZE/sizeof(IMG_UINT32))  /* + PHAS + NOP + TST + LDAD + WDF */
#else
#define USE_TERMINATE_DWORDS		(3 * EURASIA_USE_INSTRUCTION_SIZE/sizeof(IMG_UINT32))  /* + PHAS */
#endif
#endif
#else
#define USE_TERMINATE_DWORDS		(2 * EURASIA_USE_INSTRUCTION_SIZE/sizeof(IMG_UINT32))
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

#define MTE_STATE_TERMINATE_DWORDS	2


#if defined(FIX_HW_BRN_25339) || defined(FIX_HW_BRN_27330)
#define PDS_TERMINATE_DWORDS		(16 + 5) /* 16 constants, 5 instructions */
#else
#if defined(FIX_HW_BRN_31988)
#define PDS_TERMINATE_DWORDS		(12 + 6) /* 12 constants, 6 instructions */
#else
#define PDS_TERMINATE_DWORDS		(12 + 4) /* 12 constants, 4 instructions */
#endif
#endif

#if defined(FIX_HW_BRN_25339) || defined(FIX_HW_BRN_27330)
#define PDS_TERMINATE_SAUPDATE_DWORDS			11 /* 8 constants, 3 instructions */
#else
#define PDS_TERMINATE_SAUPDATE_DWORDS			14 /* 12 constants, 2 instructions */
#endif
#define MAX_DWORDS_PER_PDS_STATE_BLOCK			2
#define MAX_DWORDS_PER_INDEX_LIST_BLOCK			6
#define MAX_DWORDS_PER_TERMINATE_BLOCK			1


typedef struct EGLTerminateState_TAG
{
	PVRSRV_CLIENT_MEM_INFO *psTerminateUSEMemInfo;
	PVRSRV_CLIENT_MEM_INFO *psTerminatePDSMemInfo;
	PVRSRV_CLIENT_MEM_INFO *psSAUpdatePDSMemInfo;

	IMG_UINT32 ui32PDSDataSize;

	IMG_UINT32 ui32TerminateRegion;
	IMG_DEV_VIRTADDR uPDSCodeAddress;
	IMG_DEV_VIRTADDR uSAUpdateCodeAddress;
	IMG_UINT32 ui32SAUpdatePDSDataSize;

} EGLTerminateState;


#define EGL_RENDER_TARGET_NOAA_INDEX	0
#define EGL_RENDER_TARGET_AA_INDEX		1
#define EGL_RENDER_TARGET_MAX_INDEX		2

#if defined(SGX_FEATURE_DEPTH_BIAS_OBJECTS)

#define EGL_RENDER_DEPTHBIAS_UNUSED				0
#define EGL_RENDER_DEPTHBIAS_REGISTER_USED		1
#define EGL_RENDER_DEPTHBIAS_OBJECT_USED		2

#endif

struct EGLDrawableParamsTAG;

typedef struct EGLRenderSurfaceTAG
{
	IMG_BOOL			bFirstKick;

	IMG_HANDLE			ahRenderTarget[EGL_RENDER_TARGET_MAX_INDEX];

	/*
	** Sync info to use for this surface.  It defaults to the render surface sync info,
	** but can be changed by the driver to point to e.g. the sync info for an EGL Image
	*/
	PVRSRV_CLIENT_SYNC_INFO	*psSyncInfo;

	/* Sync info for render surface */
	PVRSRV_CLIENT_SYNC_INFO	*psRenderSurfaceSyncInfo;

#if defined(SUPPORT_SGX)
	/* Status value for renders */
	KRMStatusUpdate		sRenderStatusUpdate;
#endif

	PVRSRV_CLIENT_MEM_INFO 	*psZSBufferMemInfo;

	SceUID					hZSBufferMemBlockUID;

	/* Last render was an overflow so load ZS */
	IMG_BOOL bNeedZSLoadAfterOverflowRender;

#if defined(SUPPORT_VGX)
	PVRSRV_CLIENT_MEM_INFO 	*psMaskClipBufferMemInfo;
#endif


#if defined(SUPPORT_SGX)
	CircularBuffer		sPDSBuffer;
	CircularBuffer		sUSSEBuffer;
	EGLPixelBEState		sPixelBEState;
	IMG_UINT32			aui32HWBGObjPDSState[3];
	EGLTerminateState	sTerm;
	IMG_UINT32			ui32TerminateRegion;
	IMG_UINT32			aui32RegionClipWord[2];
	GLESTARegisters     sTARegs;
	GLES3DRegisters     s3DRegs;
#endif
	IMG_BOOL			bMultiSample;
	IMG_BOOL			bAllocTwoRT;
	IMG_BOOL 			bInFrame;
	IMG_BOOL 			bInExternalFrame;
	IMG_BOOL			bPrimitivesSinceLastTA;
	IMG_BOOL			bLastDrawMaskFullScreenEnable;
	EGLRect				sLastDrawMask;

	PVRSRV_MUTEX_HANDLE hMutex;

#if defined(SGX_FEATURE_DEPTH_BIAS_OBJECTS)
	IMG_UINT32			ui32DepthBiasState;
	IMG_INT32			i32LastUnitsInObject;
	IMG_FLOAT			fLastFactorInObject;
#endif

#if defined(EUR_CR_ISP_TAGCTRL_TRIANGLE_FRAGMENT_PIXELS_MASK)
	IMG_UINT32			ui32TriangleSplitPixelThreshold;
#endif

	EGLDrawableHandle	hEGLSurface;
	IMG_BOOL			bDepthStencilBits;

	PVRSRV_ALPHA_FORMAT			eAlphaFormat;
	PVRSRV_COLOURSPACE_FORMAT	eColourSpace;

	IMG_BOOL			bIsTwiddledSurface;

#if defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX) || defined(API_MODULES_RUNTIME_CHECKED)
	EGLClientBuffer				hVGClientBuffer;
#endif

#if defined(SUPPORT_OPENGLES1) || defined(SUPPORT_OPENGLES2) || defined(API_MODULES_RUNTIME_CHECKED)
	IMG_UINT32			ui32FBOAttachmentCount;
#endif

	IMG_UINT32					ui32NumSrcSyncs;
	PVRSRV_CLIENT_SYNC_INFO		*apsSrcSurfSyncInfo[EGL_MAX_SRC_SYNCS];

#if defined(EGL_EXTENSION_IMG_EGL_HIBERNATION)
	/* Information that needs to be stored here during hibernation */
	IMG_BOOL					bHadZSBuffer;
	PVRSRV_ROTATION 			eRotationAngle;
	IMG_UINT32 					ui32Width;
	IMG_UINT32 					ui32Height;
	/* Next surface on all displays */
	struct EGLRenderSurfaceTAG	*psNextSurfaceAll;
#endif
} EGLRenderSurface;


typedef struct EGLDrawableParamsTAG
{
	PVRSRV_ROTATION 		eRotationAngle;
	IMG_UINT32 				ui32Width;
	IMG_UINT32 				ui32Height;

	PVRSRV_PIXEL_FORMAT 	ePixelFormat;
	PVRSRV_CLIENT_SYNC_INFO *psSyncInfo;

	IMG_UINT32 				ui32Stride;
	IMG_VOID   				*pvLinSurfaceAddress;
	IMG_UINT32 				ui32HWSurfaceAddress;

	EGLRenderSurface		*psRenderSurface;

	EGLDrawableType 		eDrawableType;

	PVRSRV_ROTATION			eAccumRotationAngle;
	IMG_UINT32				ui32AccumWidth;
	IMG_UINT32				ui32AccumHeight;

	PVRSRV_PIXEL_FORMAT 	eAccumPixelFormat;
	PVRSRV_CLIENT_SYNC_INFO *psAccumSyncInfo;

	IMG_UINT32 				ui32AccumStride;
	IMG_UINT32 				ui32AccumHWAddress;

#if defined(ANDROID)
	IMG_BOOL				bWaitForRender;
#endif

} EGLDrawableParams;


typedef struct EGLcontextModeRec
{
	IMG_UINT32 ui32AntiAliasMode;

    IMG_UINT32 ui32RedBits;		/* bits per comp */
    IMG_UINT32 ui32GreenBits;
    IMG_UINT32 ui32BlueBits;
    IMG_UINT32 ui32AlphaBits;

    IMG_UINT32 ui32ColorBits;	/* total bits for rgb */

    IMG_UINT32 ui32DepthBits;

	IMG_UINT32 ui32StencilBits;


#if defined(SUPPORT_SGX_LOW_LATENCY_SCHEDULING)
	SGX_CONTEXT_PRIORITY eContextPriority;
#endif
	IMG_UINT32 ui32MaxViewportX;
	IMG_UINT32 ui32MaxViewportY;

} EGLcontextMode;


#if defined(EGL_EXTENSION_KHR_IMAGE)

/*
// YUV format flags
*/
#define EGLIMAGE_FLAGS_YUV_CONFORMANT_RANGE (0 << 0)
#define EGLIMAGE_FLAGS_YUV_FULL_RANGE       (1 << 0)
#define EGLIMAGE_FLAGS_YUV_BT601            (0 << 1)
#define EGLIMAGE_FLAGS_YUV_BT709            (1 << 1)
#define EGLIMAGE_FLAGS_COMPOSITION_SYNC     (1 << 2)

typedef struct EGLImageRec
{
	IMG_UINT32 				ui32Width;
	IMG_UINT32 				ui32Height;

	PVRSRV_PIXEL_FORMAT 	ePixelFormat;

	/* Used for distinction between compressed RGB/RGBA variants */
	IMG_BOOL				bCompressedRGBOnly;
	
	IMG_UINT32 				ui32Stride;
	IMG_VOID   				*pvLinSurfaceAddress;
	IMG_UINT32 				ui32HWSurfaceAddress;
#if defined(SUPPORT_NV12_FROM_2_HWADDRS)
	IMG_UINT32				ui32HWSurfaceAddress2;
#endif

	PVRSRV_CLIENT_MEM_INFO	*psMemInfo;

	IMG_VOID				*hImage;

	/* Original parameters of source */
	IMG_UINT32				ui32Target;
	IMG_UINT32				ui32Buffer;
	IMG_UINT32				ui32Level;
	IMG_BOOL				bTwiddled;

	IMG_UINT32				ui32Flags;
} EGLImage;

#endif /* defined(EGL_EXTENSION_KHR_IMAGE) */

typedef enum IMG_EGLERROR_TAG
{
	IMG_EGL_NO_ERROR			  = 0,
	IMG_EGL_GENERIC_ERROR		  = 1,
	IMG_EGL_SCENE_LOST			  = 2,
	IMG_EGL_MEMORY_INVALID_ERROR  = 3,
	IMG_EGL_BAD_ACCESS			  = 4,

#if defined(EGL_EXTENSION_KHR_IMAGE)
	IMG_EGL_BAD_PARAMETER		  = 5,
	IMG_EGL_BAD_MATCH			  = 6,
	IMG_EGL_OUT_OF_MEMORY		  = 7,
#endif /* defined(EGL_EXTENSION_KHR_IMAGE) */

} IMG_EGLERROR;


typedef struct IMG_OGLES1EGL_Interface_TAG
{
	IMG_UINT32	ui32APIVersion;

	GPA_PROC (*pfnGLESGetProcAddress)(const IMG_CHAR *procname);

	IMG_BOOL (*pfnGLESCreateGC)(SrvSysContext *, EGLContextHandle *, EGLcontextMode *, EGLContextHandle);
	IMG_BOOL (*pfnGLESDestroyGC)(EGLContextHandle);
	IMG_EGLERROR (*pfnGLESMakeCurrentGC)(EGLRenderSurface *, EGLRenderSurface *, EGLContextHandle);
	IMG_VOID (*pfnGLESMakeUnCurrentGC)(IMG_VOID);
	IMG_EGLERROR (*pfnGLESFlushBuffersGC)(EGLContextHandle, EGLRenderSurface *, IMG_BOOL, IMG_BOOL, IMG_BOOL);

#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE)
	IMG_BOOL (*pfnGLESBindTexImage)(EGLContextHandle, EGLDrawableHandle, EGLDrawableHandle*);
	IMG_VOID (*pfnGLESReleaseTexImage)(EGLContextHandle, EGLDrawableHandle, EGLDrawableHandle*);
#endif /* defined(EGL_EXTENSION_RENDER_TO_TEXTURE) */

#if defined(EGL_EXTENSION_KHR_IMAGE)
	IMG_EGLERROR (*pfnGLESGetImageSource)(EGLContextHandle, IMG_UINT32, IMG_UINT32, IMG_UINT32, EGLImage*);
#endif /* defined(EGL_EXTENSION_KHR_IMAGE) */

	IMG_VOID (*pfnGLESMarkRenderSurfaceAsInvalid)(EGLContextHandle);

#if defined(ANDROID)
	IMG_EGLERROR (*pfnGLESFlushBuffersGCNoContext)(IMG_BOOL, IMG_BOOL);
#endif

} IMG_OGLES1EGL_Interface;


/* According to http://www.opengl.org/registry/api/enum.spec,
 *
 *    If an extension is experimental, allocate temporary enum values in the
 *    range 0x6000-0x8000 during development work.
 *
 * So by using enums in that range we ensure that they are not going to clash
 * with any standard GL enum ever.
 */
#define IMG_OGLES1_FUNCTION_TABLE			0x6500
#define IMG_OGLES1_FUNCTION_TABLE_VERSION	3




typedef struct IMG_OGLES2EGL_Interface_TAG
{
	IMG_UINT32	ui32APIVersion;

	GPA_PROC (*pfnGLESGetProcAddress)(const IMG_CHAR *procname);

	IMG_BOOL (*pfnGLESCreateGC)(SrvSysContext *, EGLContextHandle *, EGLcontextMode *, EGLContextHandle);
	IMG_BOOL (*pfnGLESDestroyGC)(EGLContextHandle);
	IMG_EGLERROR (*pfnGLESMakeCurrentGC)(EGLRenderSurface *, EGLRenderSurface *, EGLContextHandle);
	IMG_VOID (*pfnGLESMakeUnCurrentGC)(IMG_VOID);
	IMG_EGLERROR (*pfnGLESFlushBuffersGC)(EGLContextHandle, EGLRenderSurface *, IMG_BOOL, IMG_BOOL, IMG_BOOL);

#if defined(EGL_EXTENSION_KHR_IMAGE)
	IMG_EGLERROR (*pfnGLESGetImageSource)(EGLContextHandle, IMG_UINT32, IMG_UINT32, IMG_UINT32, EGLImage*);
#endif /* defined(EGL_EXTENSION_KHR_IMAGE) */

	IMG_VOID (*pfnGLESMarkRenderSurfaceAsInvalid)(EGLContextHandle);

} IMG_OGLES2EGL_Interface;


/* According to http://www.opengl.org/registry/api/enum.spec,
 *
 *    If an extension is experimental, allocate temporary enum values in the
 *    range 0x6000-0x8000 during development work.
 *
 * So by using enums in that range we ensure that they are not going to clash
 * with any standard GL enum ever.
 */
#define IMG_OGLES2_FUNCTION_TABLE		  0x7500
#define IMG_OGLES2_FUNCTION_TABLE_VERSION 2



/**************************************************************
 *
 *                       EGL -->  OpenGL
 *
 **************************************************************/

#if defined(SUPPORT_OPENGL) || defined(API_MODULES_RUNTIME_CHECKED)

#if defined(LINUX) || defined(__psp2__)
#define IMG_EGLOGL_CALLCONV
#else
	#define IMG_EGLOGL_CALLCONV __stdcall
#endif

typedef struct IMG_OGLEGL_Interface_TAG
{
	IMG_UINT32	ui32APIVersion;

	GPA_PROC (IMG_EGLOGL_CALLCONV *pfnGLGetProcAddress)(const IMG_CHAR *procname);

	IMG_BOOL (IMG_EGLOGL_CALLCONV *pfnGLCreateGC)(SrvSysContext *, EGLContextHandle *, EGLcontextMode *, EGLContextHandle);
	IMG_BOOL (IMG_EGLOGL_CALLCONV *pfnGLDestroyGC)(EGLContextHandle);
	IMG_EGLERROR (IMG_EGLOGL_CALLCONV *pfnGLMakeCurrentGC)(EGLRenderSurface *, EGLRenderSurface *, EGLContextHandle);
	IMG_VOID (IMG_EGLOGL_CALLCONV *pfnGLMakeUnCurrentGC)(IMG_VOID);
	IMG_EGLERROR (IMG_EGLOGL_CALLCONV *pfnGLFlushBuffersGC)(EGLContextHandle, IMG_BOOL, IMG_BOOL);

#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE)
	IMG_BOOL (IMG_EGLOGL_CALLCONV *pfnGLBindTexImage)(EGLContextHandle, EGLDrawableHandle, EGLDrawableHandle* );
	IMG_VOID (IMG_EGLOGL_CALLCONV *pfnGLReleaseTexImage)(EGLContextHandle, EGLDrawableHandle,EGLDrawableHandle* );
#endif

	IMG_VOID (IMG_EGLOGL_CALLCONV *pfnGLMarkRenderSurfaceAsInvalid)(EGLContextHandle);

	IMG_BOOL (IMG_EGLOGL_CALLCONV *pfnGLFreeResources)(SrvSysContext *);

} IMG_OGLEGL_Interface;


/* According to http://www.opengl.org/registry/api/enum.spec,
 *
 *    If an extension is experimental, allocate temporary enum values in the
 *    range 0x6000-0x8000 during development work.
 *
 * So by using enums in that range we ensure that they are not going to clash
 * with any standard GL enum ever.
 */
#define IMG_OGL_FUNCTION_TABLE		   0x7800
#define IMG_OGL_FUNCTION_TABLE_VERSION 1

#endif


#if defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX) || defined(API_MODULES_RUNTIME_CHECKED)
/**************************************************************
 *
 *                       EGL -->  OpenVG
 *
 **************************************************************/
typedef struct IMG_OVGEGL_Interface_TAG
{
	IMG_UINT32	ui32APIVersion;

	GPA_PROC (*pfnOVGGetProcAddress)(const IMG_CHAR *procname);

	IMG_BOOL (*pfnOVGCreateGC)(SrvSysContext*, EGLContextHandle*, EGLcontextMode*, EGLContextHandle);
	IMG_BOOL (*pfnOVGDestroyGC)(EGLContextHandle);
	IMG_EGLERROR (*pfnOVGMakeCurrentGC)(EGLRenderSurface *, EGLRenderSurface *, EGLContextHandle);
	IMG_VOID (*pfnOVGMakeUnCurrentGC)(IMG_VOID);
	IMG_EGLERROR (*pfnOVGFlushBuffersGC)(EGLContextHandle, IMG_BOOL, IMG_BOOL);/* TODO: need surface handle? */
#if defined(EGL_EXTENSION_KHR_IMAGE)
	IMG_EGLERROR (*pfnVGGetImageSource)(EGLContextHandle, IMG_UINT32, IMG_UINT32, EGLImage*);
#endif /* defined(EGL_EXTENSION_KHR_IMAGE) */

	EGLint (*pfnOVGWrapImageAsRenderSurface)(EGLContextHandle		,
										   	EGLRenderSurface		*,
										   	EGLClientBuffer			*,
										   	IMG_UINT32				,
										   	IMG_UINT32				,
										   	IMG_UINT32				,
										   	IMG_UINT32				,
											IMG_UINT32				*,
											IMG_UINT32				*,
											IMG_UINT32				*,
											PVRSRV_PIXEL_FORMAT		*,
											PVRSRV_CLIENT_MEM_INFO	**psMemInfo);


	IMG_VOID (*pfnOVGUnWrapImage)(EGLClientBuffer);

    IMG_VOID (*pfnOVGMarkRenderSurfaceAsInvalid) (EGLContextHandle);
} IMG_OVGEGL_Interface;

#define IMG_OVG_FUNCTION_TABLE		 0x6200

#define IMG_OVG_FUNCTION_TABLE_VERSION 2

#endif /* defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX) || defined(API_MODULES_RUNTIME_CHECKED) */


#if defined(SUPPORT_OPENCL) || defined(API_MODULES_RUNTIME_CHECKED)
/**************************************************************
 *
 *                       EGL -->  OpenCL
 *
 **************************************************************/
typedef struct IMG_OCLEGL_Interface_TAG
{
	IMG_UINT32	ui32APIVersion;

#if defined(EGL_EXTENSION_KHR_IMAGE)
	IMG_EGLERROR (*pfnCLGetImageSource)(EGLContextHandle, IMG_UINT32, void *, EGLImage*);
#endif /* defined(EGL_EXTENSION_KHR_IMAGE) */

} IMG_OCLEGL_Interface;

#define IMG_OCL_FUNCTION_TABLE		 0x6200

#define IMG_OCL_FUNCTION_TABLE_VERSION 1

#endif /* defined(SUPPORT_OPENCL) || defined(API_MODULES_RUNTIME_CHECKED) */

/**************************************************************
 *
 *                       OpenGL/OpenVG --> EGL
 *
 **************************************************************/
typedef enum EGLAPIType_TAG
{
	EGL_API_OPENGLES1	=	0,
	EGL_API_OPENGLES2	=	1,
	EGL_API_OPENGLVG	=	2,
	EGL_API_DONTCARE	=	3,

}EGLAPIType;


IMG_IMPORT IMG_VOID IMG_CALLCONV KEGLSurfaceBind(IMG_HANDLE hSurface);

IMG_IMPORT IMG_VOID IMG_CALLCONV KEGLSurfaceUnbind(SrvSysContext *pvrsrv, IMG_HANDLE hSurface);

IMG_IMPORT IMG_BOOL IMG_CALLCONV KEGLCreateRenderSurface(SrvSysContext *psSysContext,
														EGLDrawableParams *psParams,
														IMG_BOOL bMultisample,
														IMG_BOOL bAllocTwoRT,
														IMG_BOOL bCreateZSBuffer,
														EGLRenderSurface *psSurface);

IMG_IMPORT IMG_BOOL IMG_CALLCONV KEGLDestroyRenderSurface(SrvSysContext *psSysContext,
															EGLRenderSurface *psSurface);

IMG_IMPORT IMG_BOOL IMG_CALLCONV KEGLResizeRenderSurface(SrvSysContext *psSysContext,
														 EGLDrawableParams *psParams,
														 IMG_BOOL bMultisample,
 														 IMG_BOOL bCreateZSBuffer,
														 EGLRenderSurface *psSurface);

IMG_IMPORT IMG_BOOL IMG_CALLCONV KEGLGetDrawableParameters(EGLDrawableHandle hDrawable,
												  		   EGLDrawableParams *ppsParams,
												  		   IMG_BOOL bAllowSurfaceRecreate);


#if defined(EGL_EXTENSION_KHR_IMAGE)

IMG_IMPORT IMG_VOID IMG_CALLCONV KEGLBindImage(IMG_VOID *hImage);
IMG_IMPORT IMG_VOID IMG_CALLCONV KEGLUnbindImage(IMG_VOID *hImage);

IMG_IMPORT IMG_BOOL IMG_CALLCONV KEGLGetImageSource(void *hEGLImage, EGLImage **ppsImage);

#endif /* defined(EGL_EXTENSION_KHR_IMAGE) */


#if defined(EGL_EXTENSION_ANDROID_BLOB_CACHE) 
IMG_IMPORT IMG_VOID IMG_CALLCONV KEGLSetBlob(const IMG_VOID* pvKey, IMG_UINT32 ui32KeySize, const IMG_VOID * pvBlob, IMG_UINT32 ui32BlobSize);
IMG_IMPORT IMG_UINT32 IMG_CALLCONV KEGLGetBlob(const IMG_VOID* pvKey, IMG_UINT32 ui32KeySize, IMG_VOID * pvBlob, IMG_UINT32 ui32BlobSize);
#endif

#if defined(EGL_EXTENSION_IMG_EGL_HIBERNATION)
IMG_IMPORT IMG_BOOL KEGLHibernateRenderSurface(SrvSysContext	*psSysContext,
											   EGLRenderSurface	*psSurface);
IMG_IMPORT IMG_BOOL KEGLAwakeRenderSurface(SrvSysContext		*psSysContext,
										   EGLRenderSurface		*psSurface);
#endif

#if defined(DEBUG)

IMG_IMPORT PVRSRV_ERROR IMG_CALLCONV KEGLAllocDeviceMemTrack(SrvSysContext *psSysContext,
															 const IMG_CHAR *pszFile, IMG_UINT32 ui32Line,
															 PVRSRV_DEV_DATA *psDevData, IMG_HANDLE hDevMemHeap,
															 IMG_UINT32 ui32Attribs, IMG_UINT32 ui32Size,
															 IMG_UINT32 ui32Alignment, PVRSRV_CLIENT_MEM_INFO **ppsMemInfo);

IMG_IMPORT PVRSRV_ERROR IMG_CALLCONV KEGLFreeDeviceMemTrack(SrvSysContext *psSysContext, const IMG_CHAR *pszFile,
															IMG_UINT32 ui32Line, PVRSRV_DEV_DATA	*psDevData,
															PVRSRV_CLIENT_MEM_INFO *psMemInfo);

#endif /* defined(DEBUG) */

#if defined (__cplusplus)
}
#endif

#endif /* _EGLAPI_H_ */

/******************************************************************************
 End of file (eglapi.h)
******************************************************************************/
