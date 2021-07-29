/*!
******************************************************************************
 @file   sgx_render_flip_test.c

 @brief  Render event sequence flip test app

 @Author PowerVR

 @date   04/04/07

         <b>Copyright 2003-2010 by Imagination Technologies Limited.</b>\n
         All rights reserved.  No part of this software, either
         material or conceptual may be copied or distributed,
         transmitted, transcribed, stored in a retrieval system
         or translated into any human or computer language in any
         form by any means, electronic, mechanical, manual or
         other-wise, or disclosed to third parties without the
         express written permission of Imagination Technologies
         Limited, Unit 8, HomePark Industrial Estate,
         King's Langley, Hertfordshire, WD4 8LZ, U.K.

 @Description:
        This test is based on sgx_render_test with an additional for-loop to enable
        repeated calls to SGXKickTA and PVRSRVSwapToDCBuffer - allowing flipping
        functionality and rendering to be tested.

 @Platform:
        Generic
******************************************************************************/

/******************************************************************************
Modifications :-
$Log: sgx_render_flip_test.c $

******************************************************************************/

#ifdef __psp2__
#include <kernel.h>

#define SYS_USING_INTERRUPTS

#define ALIGN(x, a)	(((x) + ((a) - 1)) & ~((a) - 1))

SCE_USER_MODULE_LIST("app0:libgpu_es4_ext.suprx");

#include "psp2_pvr_desc.h"
#include "psp2_pvr_defs.h"

IMG_SID							hPbMem = 0;

#endif

#include "sgx_unittest_utils.h"

#include "sgxpdsdefs.h"
#include "pds.h"
#include "pixevent.h"
#include "pixeventpbesetup.h"
#include "usecodegen.h"
#include "usegen.h"

#undef SGX_FEATURE_DATA_BREAKPOINTS
#undef SUPPORT_SGX_PRIORITY_SCHEDULING

/* turn off QAC long to float messages */
/* PRQA S 3796++ */

#if defined(SRFT_SUPPORT_PTHREADS)
#include "pthread.h"
#else
#if !defined(LINUX)
typedef IMG_VOID *pthread_t;
typedef IMG_VOID *pthread_attr_t;
#endif
/* Dummy implementation which calls the pthread execution function synchronously */
static IMG_INT pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                            void *(*start_routine)(void*), void *arg)
{
    PVR_UNREFERENCED_PARAMETER(attr);
    PVR_UNREFERENCED_PARAMETER(thread);
    (IMG_VOID)start_routine(arg);
    return 0;
}
/* Dummy implementation which is a no-op */
static IMG_INT pthread_join(pthread_t thread, void **value_ptr)
{
    PVR_UNREFERENCED_PARAMETER(thread);
    PVR_UNREFERENCED_PARAMETER(value_ptr);
    return 0;
}
#endif /* SRFT_SUPPORT_PTHREADS */

#define RED(a)		(((a) & (0xFF0000)) >> 16)
#define GREEN(a)	(((a) & (0x00FF00)) >> 8)
#define BLUE(a)		(((a) & (0x0000FF)) >> 0)

#if (EURASIA_PDS_DOUTD1_MAXBURST != EURASIA_PDS_DOUTD1_BSIZE_MAX)

static const IMG_BYTE g_abyFactorTable[] =
{
     1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
    16,  9, 16, 10,  7, 11, 16, 12,  5, 13,  9, 14, 16, 15, 16, 16,
    11, 16,  7, 12, 16, 16, 13, 10, 16, 14, 16, 11, 15, 16, 16, 16,
     7, 10, 16, 13, 16,  9, 11, 14, 16, 16, 16, 15, 16, 16,  9, 16,
    13, 11, 16, 16, 16, 14, 16, 12, 16, 16, 15, 16, 11, 13, 16, 16,
     9, 16, 16, 14, 16, 16, 16, 11, 16, 15, 13, 16, 16, 16, 16, 16,
    16, 14, 11, 10, 16, 16, 16, 13, 15, 16, 16, 12, 16, 11, 16, 16,
    16, 16, 16, 16, 13, 16, 16, 15, 11, 16, 16, 16, 16, 14, 16, 16,
    16, 13, 16, 12, 16, 16, 15, 16, 16, 16, 16, 14, 16, 16, 13, 16,
    16, 16, 16, 16, 16, 15, 16, 16, 16, 14, 16, 13, 16, 16, 16, 16,
    16, 16, 16, 16, 15, 16, 16, 14, 13, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 15, 16, 14, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 15, 14, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 15, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    15, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16
};

/* Table will be incorrect if these values change */
#if (EURASIA_PDS_DOUTD1_MAXBURST != 256)
#error ("EURASIA_PDS_DOUTD1_MAXBURST changed!")
#endif

#if (EURASIA_PDS_DOUTD1_BSIZE_MAX != 16)
#error ("EURASIA_PDS_DOUTD1_BSIZE_MAX changed!")
#endif

#if (EURASIA_PDS_DOUTD1_BLINES_MAX != 16)
#error ("EURASIA_PDS_DOUTD1_BLINES_MAX changed!")
#endif

#endif

/* A bad address which will cause a page fault - used for testing Hardware Recovery */
#define SRFT_PAGE_FAULT_ADDRESS 0xDEADBEE0

/* USE instruction construction macros */
#define USE0_SRC1_NUM(X)	((X) << EURASIA_USE0_SRC1_SHIFT)
#define USE0_SRC1_TYPE(X)	((EURASIA_USE0_S1STDBANK_##X) << EURASIA_USE0_S1BANK_SHIFT)
#define USE0_DST_NUM(X)		((X) << EURASIA_USE0_DST_SHIFT)
#define USE_SRC1(TYPE, NUM)	((USE0_SRC1_TYPE(TYPE)) | (USE0_SRC1_NUM(NUM)))
#define USE1_OP(X)			((EURASIA_USE1_OP_##X) << EURASIA_USE1_OP_SHIFT)
#define USE1_DST_TYPE(X)	((EURASIA_USE1_D1STDBANK_##X) << EURASIA_USE1_D1BANK_SHIFT)
#define USE1_PCK_SRCFMT(X)	((EURASIA_USE1_PCK_FMT_##X) << EURASIA_USE1_PCK_SRCF_SHIFT)

#if defined(SGX543) || defined(SGX544) || defined(SGX554)
#define SRFT_OPTEST_USE1_RMSKCNT 0
#else
#define SRFT_OPTEST_USE1_RMSKCNT 1
#endif

/* PDS code buffer alignment macros */
#define ALIGN_PDS_BUFFER_DEVV(ui32PDSBuffer) \
    (ui32PDSBuffer) = (((ui32PDSBuffer) + \
                        ((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1)) \
                        & ~((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1));
#define ALIGN_PDS_BUFFER_CPUV(pui32PDSBuffer) \
    (pui32PDSBuffer) = (IMG_UINT32*)(((IMG_UINT32)(pui32PDSBuffer) + \
                        ((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1)) \
                        & ~((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1));


/* USSE code buffer alignment macros */
#define ALIGN_USSE_BUFFER_DEVV(ui32USEBuffer) \
    (ui32USEBuffer) = (((ui32USEBuffer) + (EURASIA_PDS_DOUTU_PHASE_START_ALIGN - 1)) & ~(EURASIA_PDS_DOUTU_PHASE_START_ALIGN - 1));
#define ALIGN_USSE_BUFFER_CPUV(pui32USEBuffer) \
    (pui32USEBuffer) = (IMG_UINT32*)(((IMG_UINT32)(pui32USEBuffer) + (EURASIA_PDS_DOUTU_PHASE_START_ALIGN - 1)) \
                        & ~(EURASIA_PDS_DOUTU_PHASE_START_ALIGN - 1));

#define	CHECK_FRAME_NUM_FIRST(ui32RenderCount, ui32Frequency)	\
    ((IMG_BOOL)((ui32Frequency == 0) || ((ui32RenderCount % ui32Frequency) == 0)))
#define	CHECK_FRAME_NUM_LAST(ui32RenderCount, ui32Frequency)	\
    ((IMG_BOOL)((ui32Frequency > 0) && ((ui32RenderCount % ui32Frequency) == (ui32Frequency - 1))))


enum {RENDER_ARRAY_SIZE = 4};


typedef union IMG_FUINT32_Tag
{
    IMG_FLOAT  fVal;
    IMG_UINT32 ui32Val;

} IMG_FUINT32;


typedef	struct _SRFT_COLOUR
{
    IMG_FLOAT	fRed;
    IMG_FLOAT	fGreen;
    IMG_FLOAT	fBlue;
} SRFT_COLOUR;

const SRFT_COLOUR SRFT_RED =		{1, 0, 0};
const SRFT_COLOUR SRFT_GREEN =		{0, 1, 0};
const SRFT_COLOUR SRFT_CYAN =		{0, 1, 7.0/8};
const SRFT_COLOUR SRFT_MAGENTA =	{7.0/8, 0, 7.0/8};
const SRFT_COLOUR SRFT_YELLOW = 	{7.0/8, 1, 0};
const SRFT_COLOUR SRFT_BROWN =		{(IMG_FLOAT)0x61/256, (IMG_FLOAT)0x41/256, (IMG_FLOAT)0x21/256};
const SRFT_COLOUR SRFT_WHITE =		{1, 1, 1};


typedef	struct PVR_HWREGS_Tag
{
    /* 3D regs */
    IMG_UINT32			ui32ISPBGObjDepth;
    IMG_UINT32			ui32ISPBGObj;
    IMG_UINT32			ui32ISPIPFMisc;
    IMG_UINT32			ui32ISPPerpendicular;
    IMG_UINT32			ui32ISPCullValue;
    IMG_UINT32			ui32ISPZLoadBase;
    IMG_UINT32			ui32ISPZStoreBase;
    IMG_UINT32			ui32ISPStencilLoadBase;
    IMG_UINT32			ui32ISPStencilStoreBase;
    IMG_UINT32			ui32ZLSCtl;
    IMG_UINT32			ui32ISPValidId;
    IMG_UINT32			ui32ISPDBias;
    IMG_UINT32			ui32ISPSceneBGObj;
    IMG_UINT32			ui32EDMPixelPDSAddr;
    IMG_UINT32			ui32EDMPixelPDSDataSize;
    IMG_UINT32			ui32EDMPixelPDSInfo;
    IMG_UINT32			ui32PixelBE;

    /* TA regs */
    IMG_UINT32			ui32MTECtrl;

} PVR_HWREGS;


typedef enum
{
    SRFT_COMMANDLINE_OPTION_SWAPINTERVAL = 0,
    SRFT_COMMANDLINE_OPTION_FRAMES,
    SRFT_COMMANDLINE_OPTION_FRAME_START,
    SRFT_COMMANDLINE_OPTION_PBSIZE,
    SRFT_COMMANDLINE_OPTION_SLEEP,
    SRFT_COMMANDLINE_OPTION_KICKSLEEP,
    SRFT_COMMANDLINE_OPTION_USLEEP1,
    SRFT_COMMANDLINE_OPTION_USLEEP2,
    SRFT_COMMANDLINE_OPTION_SERIALISE,
    SRFT_COMMANDLINE_OPTION_BITMAP,
    SRFT_COMMANDLINE_OPTION_BITMAP_FILENAME,
    SRFT_COMMANDLINE_OPTION_PAUSE,
    SRFT_COMMANDLINE_OPTION_PAUSE_MESSAGE,
    SRFT_COMMANDLINE_OPTION_TRIANGLESTYLE,
    SRFT_COMMANDLINE_OPTION_TRIANGLESPERFRAME,
    SRFT_COMMANDLINE_OPTION_TRIANGLESPERFRAME2,
    SRFT_COMMANDLINE_OPTION_VERTEXCOLOUR,
    SRFT_COMMANDLINE_OPTION_VERTEXCOLOUR2,
    SRFT_COMMANDLINE_OPTION_VERTEXCOLOUR3,
    SRFT_COMMANDLINE_OPTION_BACKGROUNDCOLOUR,
    SRFT_COMMANDLINE_OPTION_NUMBUFFERS,
    SRFT_COMMANDLINE_OPTION_FRAMESPERROTATION,
    SRFT_COMMANDLINE_OPTION_NOEVENTOBJECT,
    SRFT_COMMANDLINE_OPTION_HWR,
    SRFT_COMMANDLINE_OPTION_MULTIPLE_TA_KICKS,
    SRFT_COMMANDLINE_OPTION_NOWAIT,
    SRFT_COMMANDLINE_OPTION_NOCLEANUP,
    SRFT_COMMANDLINE_OPTION_NOFLIP,
    SRFT_COMMANDLINE_OPTION_NOTRANSFER,
    SRFT_COMMANDLINE_OPTION_NORENDER,
    SRFT_COMMANDLINE_OPTION_BLIT,
    SRFT_COMMANDLINE_OPTION_BLIT2D,
    SRFT_COMMANDLINE_OPTION_BLITSOFTWARE,
    SRFT_COMMANDLINE_OPTION_MULTITHREAD,
    SRFT_COMMANDLINE_OPTION_GRIDSIZE,
    SRFT_COMMANDLINE_OPTION_GRIDPOSITION,
    SRFT_COMMANDLINE_OPTION_MEMFLAGS,
    SRFT_COMMANDLINE_OPTION_SLOWDOWN_TA,
    SRFT_COMMANDLINE_OPTION_SLOWDOWN_3D,
    SRFT_COMMANDLINE_OPTION_PARAMETERHEAPRESERVE,
    SRFT_COMMANDLINE_OPTION_PIXELSHADERHEAPRESERVE,
    SRFT_COMMANDLINE_OPTION_RENDERSPERFRAME,
    SRFT_COMMANDLINE_OPTION_STATUSFREQUENCY,
    SRFT_COMMANDLINE_OPTION_DEBUGBREAK,
    SRFT_COMMANDLINE_OPTION_CRASHTEST,
    SRFT_COMMANDLINE_OPTION_DUMPDEBUGINFO,
    SRFT_COMMANDLINE_OPTION_GETMISCINFO,
    SRFT_COMMANDLINE_OPTION_DUMPPROGRAMADDRS,
    SRFT_COMMANDLINE_OPTION_DUMPSURFACEADDRS,
    SRFT_COMMANDLINE_OPTION_VERBOSE,
    SRFT_COMMANDLINE_OPTION_OUTPUT_PREAMBLE,
    SRFT_COMMANDLINE_OPTION_RC_PRIORITY,
    SRFT_COMMANDLINE_OPTION_NO_SYNCOBJ,
    SRFT_COMMANDLINE_OPTION_READMEM,
    SRFT_COMMANDLINE_OPTION_EXTENDEDDATABREAKPOINTTEST,
    SRFT_COMMANDLINE_OPTION_PERCONTEXT_PB,
    SRFT_COMMANDLINE_OPTION_PBSIZEMAX,
    SRFT_COMMANDLINE_OPTION_2XMSAADONTRESOLVE,

} SRFT_COMMANDLINE_OPTION_VALUE;



static const SGXUT_COMMANDLINE_OPTION gasCommandLineOptions[] =
{
    {"-si",		SRFT_COMMANDLINE_OPTION_SWAPINTERVAL, "{n}\t", "Swap interval for flip chain"},
    {"-f",		SRFT_COMMANDLINE_OPTION_FRAMES, "{n}\t", "Total number of frames to render"},
    {"-fs",		SRFT_COMMANDLINE_OPTION_FRAME_START, "{n}\t", "Starting frame number"},
    {"-pb",		SRFT_COMMANDLINE_OPTION_PBSIZE, "{n}\t", "Parameter Buffer size in KiB"},
    {"-s",		SRFT_COMMANDLINE_OPTION_SLEEP, "{t}\t", "Sleep between renders in ms"},
    {"-ks",		SRFT_COMMANDLINE_OPTION_KICKSLEEP, "{t}\t", "Sleep between TA kicks in ms"},
    {"-us",		SRFT_COMMANDLINE_OPTION_USLEEP1, IMG_NULL, IMG_NULL},
    {"-us2",	SRFT_COMMANDLINE_OPTION_USLEEP2, IMG_NULL, IMG_NULL},
    {"-ser",	SRFT_COMMANDLINE_OPTION_SERIALISE, "{n}", "Serialise (wait for render) every 'n' frames"},
    {"-bmp",	SRFT_COMMANDLINE_OPTION_BITMAP, "{n}", "Dump bitmap every 'n' frames"},
    {"-bmpn",	SRFT_COMMANDLINE_OPTION_BITMAP_FILENAME, "{s}", "Prefix for Bitmap file name"},
    {"-p",		SRFT_COMMANDLINE_OPTION_PAUSE, "{n}\t", "Pause (wait for user) every 'n' frames"},
    {"-pm",		SRFT_COMMANDLINE_OPTION_PAUSE_MESSAGE, "{s}\t", "Message to display when pausing"},
    {"-ts",		SRFT_COMMANDLINE_OPTION_TRIANGLESTYLE, "{n}\t", "Triangle Style"},
    {"-tpf",	SRFT_COMMANDLINE_OPTION_TRIANGLESPERFRAME, "{n}", "Number of Triangles per frame"},
    {"-tpf2",	SRFT_COMMANDLINE_OPTION_TRIANGLESPERFRAME2, "{n}", "Number of Triangles per frame 2 (transition in time)"},
    {"-vc",		SRFT_COMMANDLINE_OPTION_VERTEXCOLOUR, "{c}\t", "Triangle Vertex Colour"},
    {"-vc2",	SRFT_COMMANDLINE_OPTION_VERTEXCOLOUR2, "{c}", "Triangle Vertex Colour 2 (colour transition in time)"},
    {"-vc3",	SRFT_COMMANDLINE_OPTION_VERTEXCOLOUR3, "{c}", "Triangle Vertex Colour 3 (colour transition in space)"},
    {"-bc",		SRFT_COMMANDLINE_OPTION_BACKGROUNDCOLOUR, "{c}\t", "Background colour"},
    {"-nb",		SRFT_COMMANDLINE_OPTION_NUMBUFFERS, IMG_NULL, IMG_NULL},
    {"-fpr",	SRFT_COMMANDLINE_OPTION_FRAMESPERROTATION, "{n}", "Number of frames per triangle rotation"},
    {"-neo",	SRFT_COMMANDLINE_OPTION_NOEVENTOBJECT, IMG_NULL, IMG_NULL},
    {"-hwr",	SRFT_COMMANDLINE_OPTION_HWR, IMG_NULL, IMG_NULL},
    {"-mtk",	SRFT_COMMANDLINE_OPTION_MULTIPLE_TA_KICKS, IMG_NULL, IMG_NULL},
    {"-nw",		SRFT_COMMANDLINE_OPTION_NOWAIT, IMG_NULL, IMG_NULL},
    {"-nc",		SRFT_COMMANDLINE_OPTION_NOCLEANUP, IMG_NULL, IMG_NULL},
    {"-nf",		SRFT_COMMANDLINE_OPTION_NOFLIP, "\t", "No flipping"},
    {"-nt",		SRFT_COMMANDLINE_OPTION_NOTRANSFER, IMG_NULL, IMG_NULL},
    {"-nr",		SRFT_COMMANDLINE_OPTION_NORENDER, IMG_NULL, IMG_NULL},
    {"-b",		SRFT_COMMANDLINE_OPTION_BLIT, "\t", "Use present blit"},
    {"-b2",		SRFT_COMMANDLINE_OPTION_BLIT2D, "\t", "Use present blit (2D core)"},
    {"-bs",		SRFT_COMMANDLINE_OPTION_BLITSOFTWARE, "\t", "Use present blit (software)"},
    {"-mt",		SRFT_COMMANDLINE_OPTION_MULTITHREAD, "{x} {y}", "Multithreaded with grid size (x, y)"},
    {"-gs",		SRFT_COMMANDLINE_OPTION_GRIDSIZE, "{x} {y}", "Grid size to determine bounding box"},
    {"-gp",		SRFT_COMMANDLINE_OPTION_GRIDPOSITION, "{x} {y}", "Grid position to determine bounding box"},
    {"-mf",		SRFT_COMMANDLINE_OPTION_MEMFLAGS, IMG_NULL, IMG_NULL},
    {"-sdt",	SRFT_COMMANDLINE_OPTION_SLOWDOWN_TA, "{n}", "Slow down factor for TA hardware processing"},
    {"-sd3",	SRFT_COMMANDLINE_OPTION_SLOWDOWN_3D, "{n}", "Slow down factor for 3D hardware processing"},
    {"-phr",	SRFT_COMMANDLINE_OPTION_PARAMETERHEAPRESERVE, IMG_NULL, IMG_NULL},
    {"-pshr",	SRFT_COMMANDLINE_OPTION_PIXELSHADERHEAPRESERVE, IMG_NULL, IMG_NULL},
    {"-rpf",	SRFT_COMMANDLINE_OPTION_RENDERSPERFRAME, IMG_NULL, IMG_NULL},
    {"-sf",		SRFT_COMMANDLINE_OPTION_STATUSFREQUENCY, "{n}\t", "Frequency of frame number output"},
    {"-db",		SRFT_COMMANDLINE_OPTION_DEBUGBREAK, IMG_NULL, IMG_NULL},
    {"-ct",		SRFT_COMMANDLINE_OPTION_CRASHTEST, IMG_NULL, IMG_NULL},
    {"-dd",		SRFT_COMMANDLINE_OPTION_DUMPDEBUGINFO, "\t", "Dump SGX debug info to kernel log and exit"},
    {"-gmi",	SRFT_COMMANDLINE_OPTION_GETMISCINFO, IMG_NULL, IMG_NULL},
    {"-dpa",	SRFT_COMMANDLINE_OPTION_DUMPPROGRAMADDRS, IMG_NULL, IMG_NULL},
    {"-dsa",	SRFT_COMMANDLINE_OPTION_DUMPSURFACEADDRS, IMG_NULL, IMG_NULL},
    {"-v",		SRFT_COMMANDLINE_OPTION_VERBOSE, "\t", "Verbose mode"},
    {"-opr",	SRFT_COMMANDLINE_OPTION_OUTPUT_PREAMBLE, IMG_NULL, IMG_NULL},
    {"-rcp",	SRFT_COMMANDLINE_OPTION_RC_PRIORITY, "{n}", "Render Context Priority"},
    {"-ns",		SRFT_COMMANDLINE_OPTION_NO_SYNCOBJ, "\t", "No Sync Object"},
    {"-rm",		SRFT_COMMANDLINE_OPTION_READMEM, "{n}\t", "Read mem in microkernel"},
    {"-edbp",	SRFT_COMMANDLINE_OPTION_EXTENDEDDATABREAKPOINTTEST, "{f}", "Perform Extended Data Breakpoint Test from file {f}"},
#if defined(SUPPORT_HYBRID_PB)
    {"-pcpb",	SRFT_COMMANDLINE_OPTION_PERCONTEXT_PB, "\t", "Use percontext PB, if not set defaults to shared PB"},
#endif
    {"-pbmax",	SRFT_COMMANDLINE_OPTION_PBSIZEMAX, "{n}\t", "Max Parameter Buffer size in KiB"},

    {"-2xmsaa",	SRFT_COMMANDLINE_OPTION_2XMSAADONTRESOLVE, "\t", "Output 2x oversampled msaa"},
};

#define SRFT_COMMANDLINE_NUMOPTIONS (sizeof(gasCommandLineOptions) / sizeof(gasCommandLineOptions[0]))

typedef struct
{
    IMG_BOOL			bVerbose;
    IMG_BOOL			bOutputPreamble;
    IMG_BOOL			bDebugBreak;
    IMG_BOOL			bDebugProgramAddrs;
    IMG_BOOL			bDebugSurfaceAddrs;
    IMG_BOOL			bCrashTest;
    IMG_BOOL			bGetMiscInfo;
    SGX_MISC_INFO_REQUEST	eMiscInfoRequest;
    IMG_UINT32			ui32TriangleStyle;
    IMG_UINT32			ui32TrianglesPerFrame1;
    IMG_UINT32			ui32TrianglesPerFrame2;
    IMG_UINT32			ui32SwapInterval;
    IMG_UINT32 			ui32MaxIterations;
    IMG_UINT32			ui32FramesPerRotation;
    IMG_UINT32			ui32NumBackBuffers;
    IMG_UINT32			ui32NumSwapChainBuffers;
    IMG_UINT32			ui32StatusFrequency;
    IMG_BOOL			bMultipleTAKicks;
    IMG_UINT32 			ui32FrameStart;
    IMG_UINT32			ui32PBSize;
    IMG_UINT32			ui32SleepTimeMS;
    IMG_UINT32			ui32KickSleepTimeMS;
    IMG_UINT32			ui32SleepTimeUS1;
    IMG_UINT32			ui32SleepTimeUS2;
    IMG_UINT32			ui32SerialiseFrequency;
    IMG_UINT32			ui32BitmapFrequency;
    IMG_CHAR			*szBitmapFileName;
    IMG_UINT32			ui32PauseFrequency;
    IMG_CHAR			*szPauseMessage;
    SRFT_COLOUR			sVertexColour1;
    SRFT_COLOUR			sVertexColour2;
    SRFT_COLOUR			sVertexColour3;
    SRFT_COLOUR			sBGColour;
    IMG_BOOL			bNoEventObject;
    IMG_UINT32			ui32TestHWRRateVDM;
    IMG_UINT32			ui32TestHWRRateVS;
    IMG_UINT32			ui32TestHWRRatePBE;
    IMG_UINT32			ui32TestHWRRatePS;
    IMG_UINT32			ui32TestHWRRateBlit;
    IMG_BOOL			bNoWait;
    IMG_BOOL			bNoCleanup;
    IMG_BOOL			bFlip;
    IMG_BOOL			bBlit;
    IMG_BOOL			b2DBlit;
    IMG_BOOL			bSoftwareBlit;
    IMG_BOOL			bNoTransfer;
    IMG_BOOL			bNoRender;
    IMG_UINT32			ui32MTGridSizeX;
    IMG_UINT32			ui32MTGridSizeY;
    IMG_UINT32			ui32MPGridSizeX;
    IMG_UINT32			ui32MPGridSizeY;
    IMG_UINT32			ui32MPGridPosX;
    IMG_UINT32			ui32MPGridPosY;
    IMG_UINT32			ui32PBEMemFlags;
    IMG_UINT32			ui32PDSVertexMemFlags;
    IMG_UINT32			ui32PDSPixelMemFlags;
    IMG_UINT32			ui32PDSPixelCodeMemFlags;
    IMG_UINT32			ui32PDSVertexCodeMemFlags;
    IMG_BOOL			bUSSEPixelWriteMem;
    IMG_UINT32			ui32USSEPixelMemFlags;
    IMG_BOOL			bUSSEVertexWriteMem;
    IMG_UINT32			ui32USSEVertexMemFlags;
    IMG_UINT32			ui32VertexUSSECodeMemFlags;
    IMG_UINT32			ui32PixelUSSECodeMemFlags;
    IMG_UINT32			ui32SlowdownFactorTA;
    IMG_UINT32			ui32SlowdownFactor3D;
    IMG_UINT32			ui323DParamsHeapReserveFactor;
    IMG_UINT32			ui32PixelShaderHeapReserve;
    IMG_UINT32			ui32RendersPerFrame;
    SGX_CONTEXT_PRIORITY		eRCPriority;
    IMG_BOOL			bNoSyncObject;
    IMG_BOOL			bReadMem;
    IMG_BOOL			bExtendedDataBreakpointTest;
    IMG_CHAR            *pszExtendedDataBreakpointTestConfigFilename;
    IMG_BOOL			bPerContextPB;
    IMG_UINT32			ui32MaxPBSize;
    IMG_BOOL			b2XMSAADontResolve;	
} SRFT_CONFIG;



/*
 *
 * EXTENDED DATA BREAKPOINT TEST STUFF
 * TODO: MOVE ME TO A SEPARATE FILE PERHAPS?
 *
 */

#define EDBP_NAMESZ 100
#define EDBP_MAXBUFFERS 100
#define EDBP_MAXBKPTS 4

typedef struct
{
    IMG_BOOL        bHardwareSupportsDataBreakpoints;
    PVRSRV_MUTEX_HANDLE hMutex;
    struct {
        IMG_BOOL    bInstalled; /* true if the hardware has been told about this bkpt */
        IMG_BOOL    bValid;     /* true if this array entry marks a breakpoint */
        IMG_CHAR	szBufferName[EDBP_NAMESZ];
        IMG_INT32   i32OffsetStart, i32OffsetEndPlusOne;
        IMG_DEV_VIRTADDR sDevVAddrStart, sDevVAddrEndPlusOne;
        IMG_BOOL    bTrapped;   /* is a trapped breakpoint */
        IMG_BOOL    bRead;      /* triggered by read */
        IMG_BOOL    bWrite;     /* triggered by write */
        IMG_UINT32  ui32DataMasterMask; /* which DMs to trigger on */
        IMG_UINT32  ui32Delayms; /* how long to wait before resuming */
        IMG_BOOL    bMustHit, bMustNotHit; /* whether the breakpoint should be hit */
        IMG_BOOL    bHit;       /* whether the breakpoint has been hit */
        IMG_BOOL    bInError;
    } asBkpt[EDBP_MAXBKPTS];
    struct {
        IMG_CHAR	szBufferName[EDBP_NAMESZ];
        IMG_DEV_VIRTADDR sDevVAddr;
        IMG_UINT32  ui32BufSz;
    } asBuffer[EDBP_MAXBUFFERS];
    IMG_UINT32      ui32NumBuffers;
    PVRSRV_DEV_DATA		*psSGXDevData;
    fnDPF           fnInfo;
    fnFIE           fnInfoFailIfError;
} EDBP_STATE_DATA;

#if defined(SRFT_SUPPORT_PTHREADS)
typedef struct
{
    EDBP_STATE_DATA   *psEdbpState;
    PVRSRV_CONNECTION *psConnection;
    pthread_t         sPThread;
    IMG_BOOL          bStop;
    IMG_UINT32        ui32WaitTimems;
#if defined (SUPPORT_SID_INTERFACE)
    IMG_SID           hOSEventObject;
#else
    IMG_HANDLE        hOSEventObject;
#endif
} EDBP_HANDLE_BKPT_THREAD_DATA;
#endif

typedef struct
{
    fnDPF					fnInfo;
    fnFIE					fnInfoFailIfError;
    PVRSRV_DEV_DATA			*psSGXDevData;
    PVRSRV_SGX_CLIENT_INFO	*psSGXInfo;
    IMG_HANDLE				hDisplayDevice;
#if defined (SUPPORT_SID_INTERFACE)
    IMG_EVENTSID			hOSGlobalEvent;
    IMG_SID					hPDSFragmentHeap;
    IMG_SID					hPDSVertexHeap;
    IMG_SID					hGeneralHeap;
    IMG_SID					hUSEVertexHeap;
    IMG_SID					hUSEFragmentHeap;
    IMG_SID					h3DParametersHeap;
    IMG_SID	 				h2DHeap;
#else
    IMG_HANDLE				hOSGlobalEvent;
    IMG_HANDLE				hPDSFragmentHeap;
    IMG_HANDLE				hPDSVertexHeap;
    IMG_HANDLE				hGeneralHeap;
    IMG_HANDLE				hUSEVertexHeap;
    IMG_HANDLE				hUSEFragmentHeap;
    IMG_HANDLE				h3DParametersHeap;
    IMG_HANDLE 				h2DHeap;
#endif
    IMG_DEV_VIRTADDR		uUSEFragmentHeapBase;
    IMG_DEV_VIRTADDR		uUSEVertexHeapBase;
    IMG_DEV_VIRTADDR		u2DHeapBase;
    PVRSRV_CLIENT_MEM_INFO	**ppsSwapBufferMemInfo;
    PVRSRV_CLIENT_MEM_INFO	*psSGXSystemBufferMemInfo;
#if defined (SUPPORT_SID_INTERFACE)
    IMG_SID					*phSwapChainBuffers;
#else
    IMG_HANDLE				*phSwapChainBuffers;
#endif
    IMG_UINT32				ui32MainRenderLineStride;
    IMG_UINT32				ui32RectangleSizeX;
    IMG_UINT32				ui32RectangleSizeY;
    IMG_UINT32				ui32BytesPerPixel;
    IMG_UINT32				ui32FBTexFormat;
    DISPLAY_DIMS			*psPrimDims;
    IMG_UINT32				ui322DSurfaceFormat;
    DISPLAY_FORMAT			*psPrimFormat;
    PDUMP_PIXEL_FORMAT		eDumpFormat;
    IMG_HANDLE				hTransferContext;
    IMG_HANDLE				hRenderContext;
#if defined (SUPPORT_SID_INTERFACE)
    IMG_SID					hDevMemContext;
#else
    IMG_HANDLE				hDevMemContext;
#endif
    PVRSRV_CONNECTION		*psConnection;
    EDBP_STATE_DATA         *psEdbpState;
} SRFT_SHARED;


typedef struct
{
    pthread_t		sPThread;
    SRFT_CONFIG		*psConfig;
    SRFT_SHARED		*psShared;
    IMG_UINT32		ui32GridPosX;
    IMG_UINT32		ui32GridPosY;
    
} SRFT_THREAD_DATA;


#if !defined(PDUMP)
/* Stub functions so real code isn't polluted with conditionals */
PVRSRV_ERROR IMG_CALLCONV PVRSRVPDumpComment(IMG_CONST PVRSRV_CONNECTION *psConnection,
                                             IMG_CONST IMG_CHAR *pszComment,
                                             IMG_BOOL bContinuous)
{
    PVR_UNREFERENCED_PARAMETER(psConnection);
    PVR_UNREFERENCED_PARAMETER(pszComment);
    PVR_UNREFERENCED_PARAMETER(bContinuous);
    return PVRSRV_OK;
}

PVRSRV_ERROR IMG_CALLCONV PVRSRVPDumpMem(IMG_CONST PVRSRV_CONNECTION *psConnection,
                                         IMG_PVOID pvAltLinAddr,
                                         PVRSRV_CLIENT_MEM_INFO *psMemInfo,
                                         IMG_UINT32 ui32Offset,
                                         IMG_UINT32 ui32Bytes,
                                         IMG_UINT32 ui32Flags)
{
    PVR_UNREFERENCED_PARAMETER(psConnection);
    PVR_UNREFERENCED_PARAMETER(pvAltLinAddr);
    PVR_UNREFERENCED_PARAMETER(psMemInfo);
    PVR_UNREFERENCED_PARAMETER(ui32Offset);
    PVR_UNREFERENCED_PARAMETER(ui32Bytes);
    PVR_UNREFERENCED_PARAMETER(ui32Flags);
    return PVRSRV_OK;
}	

PVRSRV_ERROR IMG_CALLCONV PVRSRVPDumpSetFrame(IMG_CONST PVRSRV_CONNECTION *psConnection,
                                              IMG_UINT32 ui32Frame)
{
    PVR_UNREFERENCED_PARAMETER(psConnection);
    PVR_UNREFERENCED_PARAMETER(ui32Frame);
    return PVRSRV_OK;
}
#endif /* PDUMP */


/*
 *
 * EXTENDED DATA BREAKPOINT TEST STUFF
 * TODO: MOVE ME TO A SEPARATE FILE PERHAPS?
 *
 */

static IMG_VOID edbp_ActuallySetBreakpoint(EDBP_STATE_DATA  *psEdbpState,
                                           IMG_UINT32		ui32BPIndex,
                                           IMG_DEV_VIRTADDR sDevVAddrStart,
                                           IMG_DEV_VIRTADDR sDevVAddrEndPlusOne,
                                           IMG_BOOL         bTrapped,
                                           IMG_BOOL         bRead,
                                           IMG_BOOL         bWrite,
                                           IMG_UINT32       ui32DMMask
                                           )
{
#if defined SGX_FEATURE_DATA_BREAKPOINTS
    /* Insert a data breakpoint at the given address */
    PVRSRV_ERROR		eResult;
    SGX_MISC_INFO		sSGXMiscInfo;

    psEdbpState->fnInfo("edbp_ActuallySetBreakpoint(0x%08x, 0x%08x, %d, %d, %d, %d)\n", sDevVAddrStart.uiAddr, sDevVAddrEndPlusOne.uiAddr, bTrapped, bRead, bWrite, ui32DMMask);
    psEdbpState->fnInfo("Calling SGXGetMiscInfo to set Data Breakpoint %u at 0x%08x to 0x%08x\n",
                            ui32BPIndex, sDevVAddrStart.uiAddr, sDevVAddrEndPlusOne.uiAddr-1);

    sSGXMiscInfo.eRequest = SGX_MISC_INFO_REQUEST_SET_BREAKPOINT;
    sSGXMiscInfo.uData.sSGXBreakpointInfo.bBPEnable = IMG_TRUE;
    sSGXMiscInfo.uData.sSGXBreakpointInfo.ui32BPIndex = ui32BPIndex;
    sSGXMiscInfo.uData.sSGXBreakpointInfo.sBPDevVAddr = sDevVAddrStart;
    sSGXMiscInfo.uData.sSGXBreakpointInfo.sBPDevVAddrEnd = sDevVAddrEndPlusOne;
    sSGXMiscInfo.uData.sSGXBreakpointInfo.sBPDevVAddrEnd.uiAddr -= 0x10;
    sSGXMiscInfo.uData.sSGXBreakpointInfo.bTrapped = bTrapped;
    sSGXMiscInfo.uData.sSGXBreakpointInfo.bRead = bRead;
    sSGXMiscInfo.uData.sSGXBreakpointInfo.bWrite = bWrite;
    sSGXMiscInfo.uData.sSGXBreakpointInfo.ui32DataMasterMask = ui32DMMask;
    
    eResult = SGXGetMiscInfo(psEdbpState->psSGXDevData, &sSGXMiscInfo);
    psEdbpState->fnInfoFailIfError(eResult);
#else
    PVR_UNREFERENCED_PARAMETER(ui32BPIndex);
    PVR_UNREFERENCED_PARAMETER(sDevVAddrStart);
    PVR_UNREFERENCED_PARAMETER(sDevVAddrEndPlusOne);
    PVR_UNREFERENCED_PARAMETER(bTrapped);
    PVR_UNREFERENCED_PARAMETER(bRead);
    PVR_UNREFERENCED_PARAMETER(bWrite);
    PVR_UNREFERENCED_PARAMETER(ui32DMMask);
    psEdbpState->fnInfo("Data Breakpoints not supported for this device - not setting one\n");
#endif
}

static IMG_VOID edbp_Init(EDBP_STATE_DATA  *psEdbpState, PVRSRV_DEV_DATA *psSGXDevData)
{
#if defined SGX_FEATURE_DATA_BREAKPOINTS
    psEdbpState->bHardwareSupportsDataBreakpoints = IMG_TRUE;
#else
    psEdbpState->bHardwareSupportsDataBreakpoints = IMG_FALSE;
#endif
    psEdbpState->ui32NumBuffers = 0;
    psEdbpState->asBkpt[0].bValid = IMG_FALSE;
    psEdbpState->asBkpt[1].bValid = IMG_FALSE;
    psEdbpState->asBkpt[2].bValid = IMG_FALSE;
    psEdbpState->asBkpt[3].bValid = IMG_FALSE;
    PVRSRVCreateMutex(&psEdbpState->hMutex);

    psEdbpState->psSGXDevData = psSGXDevData;

    psEdbpState->fnInfo = DPF; // actually - caller should override this? // sgxut_null_dpf
    psEdbpState->fnInfoFailIfError = sgxut_fail_if_error_quiet;
}

static IMG_VOID edbp_InstallBreakpointNow(EDBP_STATE_DATA  *psEdbpState, IMG_UINT32 ui32BkptIndex, IMG_UINT32 ui32BufferIndex)
{
    IMG_DEV_VIRTADDR sStartAddr, sEndPOAddr;

    sStartAddr = sEndPOAddr = psEdbpState->asBuffer[ui32BufferIndex].sDevVAddr;

    if (psEdbpState->asBkpt[ui32BkptIndex].i32OffsetEndPlusOne <= 0)
    {
        /* 0 means end of buffer.  -ve is rel to end.  +ve is rel to start */
        sEndPOAddr.uiAddr += psEdbpState->asBuffer[ui32BufferIndex].ui32BufSz;
    }

    if (psEdbpState->asBkpt[ui32BkptIndex].i32OffsetStart < 0)
    {
        /* 0 means start of buffer.  -ve is rel to end.  +ve is rel to start */
        sStartAddr.uiAddr += psEdbpState->asBuffer[ui32BufferIndex].ui32BufSz;
    }

    sStartAddr.uiAddr += psEdbpState->asBkpt[ui32BkptIndex].i32OffsetStart;
    sEndPOAddr.uiAddr += psEdbpState->asBkpt[ui32BkptIndex].i32OffsetEndPlusOne;

    if(sStartAddr.uiAddr&0xf)
    {
        /* Oops - need to round the address */
        sStartAddr.uiAddr += 0xf;
        sStartAddr.uiAddr &= ~0xf;
    }

    if(sEndPOAddr.uiAddr&0xf)
    {
        /* Oops - need to round the address */
        sEndPOAddr.uiAddr &= ~0xf;
    }
        
    edbp_ActuallySetBreakpoint(psEdbpState,
                               ui32BkptIndex, 
                               sStartAddr, sEndPOAddr,
                               psEdbpState->asBkpt[ui32BkptIndex].bTrapped,
                               psEdbpState->asBkpt[ui32BkptIndex].bRead,
                               psEdbpState->asBkpt[ui32BkptIndex].bWrite,
                               psEdbpState->asBkpt[ui32BkptIndex].ui32DataMasterMask);

    psEdbpState->asBkpt[ui32BkptIndex].sDevVAddrStart = sStartAddr;
    psEdbpState->asBkpt[ui32BkptIndex].sDevVAddrEndPlusOne = sEndPOAddr;
    psEdbpState->asBkpt[ui32BkptIndex].bInstalled = IMG_TRUE;
}

static IMG_VOID edbp_Buffer(EDBP_STATE_DATA  *psEdbpState, IMG_CHAR *pszBufferName, IMG_DEV_VIRTADDR sDevVAddr, IMG_UINT32 ui32BufSz)
{
    /* Attach a name to a buffer, for the purpose of setting a breakpoint */
    /* Also, install the breakpoint, if matched */

    IMG_UINT32 ui32BkptIndex;

    PVRSRVLockMutex(psEdbpState->hMutex);

    if (psEdbpState->ui32NumBuffers < EDBP_MAXBUFFERS)
    {
        strncpy(psEdbpState->asBuffer[psEdbpState->ui32NumBuffers].szBufferName, pszBufferName, EDBP_NAMESZ);
        psEdbpState->asBuffer[psEdbpState->ui32NumBuffers].sDevVAddr = sDevVAddr;
        psEdbpState->asBuffer[psEdbpState->ui32NumBuffers].ui32BufSz = ui32BufSz;

        for (ui32BkptIndex = 0; ui32BkptIndex < EDBP_MAXBKPTS; ui32BkptIndex++)
        {
            if (psEdbpState->asBkpt[ui32BkptIndex].bValid)
            {
                if (!strcmp(psEdbpState->asBkpt[ui32BkptIndex].szBufferName, pszBufferName))
                {
                    edbp_InstallBreakpointNow(psEdbpState, ui32BkptIndex, psEdbpState->ui32NumBuffers);
                }
            }
        }
        
        psEdbpState->ui32NumBuffers++;
    }

    PVRSRVUnlockMutex(psEdbpState->hMutex);
}

static IMG_VOID edbp_RegisterBreakpoint(EDBP_STATE_DATA   *psEdbpState, 
                                        IMG_UINT32        ui32Index,
                                        IMG_CHAR          *pszBufferName,
                                        IMG_INT32         i32OffsetStart,
                                        IMG_INT32         i32OffsetEndPlusOne,
                                        IMG_BOOL          bTrapped,
                                        IMG_BOOL          bRead,
                                        IMG_BOOL          bWrite,
                                        IMG_UINT32        ui32DataMasterMask,
                                        IMG_UINT32        ui32Delayms,
                                        IMG_BOOL          bMustHit,
                                        IMG_BOOL          bMustNotHit)
{
    PVRSRVLockMutex(psEdbpState->hMutex);

    psEdbpState->fnInfo("RegisterBreakpoint(..., %d, \"%s\", %d, %d, %d, %d, %d, %d, %d, %d, %d);\n",
                        ui32Index,
                        pszBufferName,
                        i32OffsetStart, i32OffsetEndPlusOne,
                        bTrapped,
                        bRead, bWrite,
                        ui32DataMasterMask,
                        ui32Delayms,
                        bMustHit, bMustNotHit);

    strncpy(psEdbpState->asBkpt[ui32Index].szBufferName, pszBufferName, EDBP_NAMESZ);
    psEdbpState->asBkpt[ui32Index].i32OffsetStart = i32OffsetStart;
    psEdbpState->asBkpt[ui32Index].i32OffsetEndPlusOne = i32OffsetEndPlusOne;
    psEdbpState->asBkpt[ui32Index].bTrapped = bTrapped;
    psEdbpState->asBkpt[ui32Index].bRead = bRead;
    psEdbpState->asBkpt[ui32Index].bWrite = bWrite;
    psEdbpState->asBkpt[ui32Index].ui32DataMasterMask = ui32DataMasterMask;
    psEdbpState->asBkpt[ui32Index].ui32Delayms = ui32Delayms;
    psEdbpState->asBkpt[ui32Index].bMustHit = bMustHit;
    psEdbpState->asBkpt[ui32Index].bMustNotHit = bMustNotHit;
    psEdbpState->asBkpt[ui32Index].bInstalled = IMG_FALSE;
    psEdbpState->asBkpt[ui32Index].bValid = IMG_TRUE;
    psEdbpState->asBkpt[ui32Index].bHit = IMG_FALSE;
    psEdbpState->asBkpt[ui32Index].bInError = IMG_FALSE;

    PVRSRVUnlockMutex(psEdbpState->hMutex);
}

static IMG_VOID edbp_ProcessLineInConfig(EDBP_STATE_DATA *psEdbpState, IMG_UINT32 ui32NumWords, IMG_CHAR *apszWords[])
{
    /* Each line in the config file must be, strictly, in order:
       -   breakpoint index {0,1,2,3}
       -   buffer name
       (the remaining entries are optional... defaults given in [], and can be <a>bbreviated)
       -   offset of start of bkpt region [0]
       -   offset of end of bkpt region [0]
       -   bkpt type: <t>rapped or <u>ntrapped [trapped]
       -   match on <r>ead/<w>rite/readwrite(<rw>) [readwrite]
       -   data master(s) to match on: {v,p,e} or a combination [vpe]
       -   milliseconds to wait before auto-resuming [0]
    */

    IMG_UINT32 ui32BkptIndex;
    IMG_CHAR *pszBufferName;
    IMG_INT32 i32OffsetStart, i32OffsetEndPlusOne;
    IMG_BOOL bTrapped, bRead, bWrite;
    IMG_UINT32 ui32DataMasterMask;
    IMG_UINT32 ui32Delayms;
    IMG_BOOL bMustHit, bMustNotHit;
    
    if(ui32NumWords<2)
    {
        /* oops */
        psEdbpState->fnInfo("WARNING: edbp: error in config file: too few words\n");
        return;
    }
    
    ui32BkptIndex = strtoul(apszWords[0], NULL, 10);
    pszBufferName = apszWords[1];
    i32OffsetStart = ui32NumWords>=3 ? strtol(apszWords[2], NULL, 0) : 0;
    i32OffsetEndPlusOne = ui32NumWords>=4 ? strtol(apszWords[3], NULL, 0) : 0;
    
    if (ui32NumWords >= 5)
    {
        if (!strcmp(apszWords[4], "t") || !strcmp(apszWords[4], "trapped"))
        {
            bTrapped = IMG_TRUE;
        }
        else if (!strcmp(apszWords[4], "u") || !strcmp(apszWords[4], "untrapped"))
        {
            bTrapped = IMG_FALSE;
        }
        else
        {
            psEdbpState->fnInfo("ERROR: edbp: error in config file: bad bkpt type: '%s'\n",  apszWords[4]);
            return;
        }
    }
    else
    {
        bTrapped = IMG_TRUE;
    }

    if (ui32NumWords >= 6)
    {
        if (!strcmp(apszWords[5], "r") || !strcmp(apszWords[5], "read"))
        {
            bRead = IMG_TRUE;
            bWrite = IMG_FALSE;
        }
        else if (!strcmp(apszWords[5], "w") || !strcmp(apszWords[5], "write"))
        {
            bRead = IMG_FALSE;
            bWrite = IMG_TRUE;
        }
        else if (!strcmp(apszWords[5], "rw") || !strcmp(apszWords[5], "readwrite"))
        {
            bRead = IMG_TRUE;
            bWrite = IMG_TRUE;
        }
        else
        {
            psEdbpState->fnInfo("ERROR: edbp: error in config file: bad access mode: '%s'\n",  apszWords[5]);
            return;
        }
    }
    else
    {
        bRead = IMG_TRUE;
        bWrite = IMG_TRUE;
    }

    if (ui32NumWords >= 7)
    {
        ui32DataMasterMask = 0;
        if (strchr(apszWords[6], 'v')) ui32DataMasterMask |= 1;
        if (strchr(apszWords[6], 'p')) ui32DataMasterMask |= 2;
        if (strchr(apszWords[6], 'e')) ui32DataMasterMask |= 4;
    }
    else
    {
        ui32DataMasterMask = 7;
    }

    if (ui32NumWords >= 8)
    {
        ui32Delayms = strtoul(apszWords[7], NULL, 10);
    }
    else
    {
        ui32Delayms = 0;
    }

    if (ui32NumWords >= 9)
    {
        if (!strcmp(apszWords[8], "hit"))
        {
            bMustHit = IMG_TRUE;
            bMustNotHit = IMG_FALSE;
        }
        else if (!strcmp(apszWords[8], "unhit"))
        {
            bMustHit = IMG_FALSE;
            bMustNotHit = IMG_TRUE;
        }
        else if (!strcmp(apszWords[8], "dontcare"))
        {
            bMustHit = IMG_FALSE;
            bMustNotHit = IMG_FALSE;
        }
        else
        {
            psEdbpState->fnInfo("ERROR: edbp: error in config file: bad expectation: '%s'\n",  apszWords[8]);
            return;
        }
    }
    else
    {
        bMustNotHit = IMG_FALSE;
        bMustHit = IMG_FALSE;
    }

    edbp_RegisterBreakpoint(psEdbpState, ui32BkptIndex, pszBufferName, i32OffsetStart, i32OffsetEndPlusOne, bTrapped, bRead, bWrite, ui32DataMasterMask, ui32Delayms, bMustHit, bMustNotHit);
}
    
static IMG_VOID edbp_ReadConfig(EDBP_STATE_DATA   *psEdbpState, 
                                IMG_CHAR          *pszFilename)
{
    FILE *hConfigFile;
    IMG_CHAR acBuf[4096];
    IMG_CHAR *apszWords[12];
    IMG_CHAR *pcCursor, *pcEol;
    IMG_UINT32 ui32NumWords;

    hConfigFile = fopen(pszFilename, "rt");
    
    if(!hConfigFile)
    {
        return;
    }

    while(fgets(acBuf, 4096, hConfigFile))
    {
        /* very simply parser.  one entry per line.
           finds "words" separated by whitespace.
           ignores anything after "#" (can be used for comment)
           or anything after Nth word.
        */

        ui32NumWords = 0;
        
        pcEol = strchr(acBuf, '#');
        if(!pcEol) pcEol = strchr(acBuf, '\r');
        if(!pcEol) pcEol = strchr(acBuf, '\n');
        if(!pcEol) pcEol = acBuf+strlen(acBuf);

        pcCursor = acBuf;

        while(pcCursor < pcEol)
        {
            /* skip over whitespace */
            while(pcCursor < pcEol && strchr(" \t\r\n", *pcCursor)) pcCursor++;

            if(pcCursor < pcEol)
            {
                apszWords[ui32NumWords++] = pcCursor;
                
                /* find end of word and null-terminate it */
                while(pcCursor < pcEol && !strchr(" \t", *pcCursor)) pcCursor++;
                *pcCursor = 0;
            }
        }
        if(ui32NumWords>0)
        {
            edbp_ProcessLineInConfig(psEdbpState, ui32NumWords, apszWords);
        }
    }			
    fclose(hConfigFile);
}

static IMG_VOID edbp_NumberedDeviceBuffer(EDBP_STATE_DATA        *psEdbpState, 
                                          IMG_CHAR               *pszBaseName, 
                                          IMG_UINT32             ui32Number,
                                          PVRSRV_CLIENT_MEM_INFO *psMemInfo)
{
    static IMG_CHAR szBuf[EDBP_NAMESZ];

    //?!?!	snprintf(szBuf, EDBP_NAMESZ, "%s%d", pszBaseName, ui32Number);
    sprintf(szBuf, "%s%d", pszBaseName, ui32Number);
    
    edbp_Buffer(psEdbpState, szBuf, psMemInfo->sDevVAddr, psMemInfo->uAllocSize);
}

static IMG_VOID edbp_NumberedDeviceBuffer3(EDBP_STATE_DATA        *psEdbpState, 
                                           IMG_CHAR               *pszBaseName, 
                                           IMG_UINT32             ui32NumberA,
                                           IMG_UINT32             ui32NumberB,
                                           IMG_UINT32             ui32NumberC,
                                           PVRSRV_CLIENT_MEM_INFO *psMemInfo)
{
    static IMG_CHAR szBuf[EDBP_NAMESZ];

    //?!?!	snprintf(szBuf, EDBP_NAMESZ, "%s%d", pszBaseName, ui32Number);
    sprintf(szBuf, "%s%d.%d.%d", pszBaseName, ui32NumberA, ui32NumberB, ui32NumberC);
    
    edbp_Buffer(psEdbpState, szBuf, psMemInfo->sDevVAddr, psMemInfo->uAllocSize);
}

static IMG_VOID edbp_HandleBkpt(EDBP_STATE_DATA *psEdbpState)
{
    /* Has a breakpoint hit?  If so, handle it. */

#if defined SGX_FEATURE_DATA_BREAKPOINTS
    {
        PVRSRV_ERROR	eResult;
        SGX_MISC_INFO	sSGXMiscInfo;
        
        psEdbpState->fnInfo("Calling SGXGetMiscInfo to poll for breakpoint\n");
        sSGXMiscInfo.eRequest = SGX_MISC_INFO_REQUEST_POLL_BREAKPOINT;
        eResult = SGXGetMiscInfo(psEdbpState->psSGXDevData, &sSGXMiscInfo);
        psEdbpState->fnInfo("SGXGetMiscInfo returned\n");
        psEdbpState->fnInfoFailIfError(eResult);
        
        if (!sSGXMiscInfo.uData.sSGXBreakpointInfo.bTrappedBP)
        {
            return;
        }
        
        psEdbpState->fnInfo("edbp: breakpoint detected.  getting mutex...\n");
        PVRSRVLockMutex(psEdbpState->hMutex);
        psEdbpState->fnInfo("edbp: got mutex...\n");
        
        /* Ideally only one thread should ever be responsible for handling
           breakpoints.  Really ideally, that thread should be in a
           separate process to this one.  However, as a compromise, we
           piggy back off the mutex protecting sEdbpState, and allow the
           owner of that mutex to assume responsibility for handling
           breakpoints.  
           
           Of course, this means we now have to re-check, having taken the
           mutex.  This is slight mis-use of the mutex, but since I've
           documented it here, I figure I'll be allowed to get away with
           it for now...  
           
           We need to take the mutex in order to update the sEdbpState
           structure in any case.
        */
        
        psEdbpState->fnInfo("Calling SGXGetMiscInfo to poll for breakpoint\n");
        sSGXMiscInfo.eRequest = SGX_MISC_INFO_REQUEST_POLL_BREAKPOINT;
        eResult = SGXGetMiscInfo(psEdbpState->psSGXDevData, &sSGXMiscInfo);
        psEdbpState->fnInfo("SGXGetMiscInfo returned (2)\n");
        psEdbpState->fnInfoFailIfError(eResult);
    
        while (sSGXMiscInfo.uData.sSGXBreakpointInfo.bTrappedBP)
        {
            IMG_UINT32 ui32Index;
            //			IMG_UINT32 ui32SeqNum, ui32LastSeqNum;
            IMG_UINT32 ui32SetRangeBegin, ui32SetRangeEnd, ui32HitRangeBegin, ui32HitRangeEnd;
            IMG_BOOL bOutOfRange;
            
            ui32Index = sSGXMiscInfo.uData.sSGXBreakpointInfo.ui32BPIndex;
            
            psEdbpState->fnInfo("%s %d Breakpoint %d trapped address 0x%08x len 0x%07x0\n",
                                sSGXMiscInfo.uData.sSGXBreakpointInfo.ui32CoreNum>99 ? "Master" : "Core",
                                sSGXMiscInfo.uData.sSGXBreakpointInfo.ui32CoreNum,
                                sSGXMiscInfo.uData.sSGXBreakpointInfo.ui32BPIndex,
                                sSGXMiscInfo.uData.sSGXBreakpointInfo.sTrappedBPDevVAddr.uiAddr,
                                sSGXMiscInfo.uData.sSGXBreakpointInfo.ui32TrappedBPBurstLength+1);
            psEdbpState->fnInfo(">>> the breakpoint was a %s by DM %d, TAG %d\n",
                                sSGXMiscInfo.uData.sSGXBreakpointInfo.bTrappedBPRead?"READ":"WRITE",
                                sSGXMiscInfo.uData.sSGXBreakpointInfo.ui32TrappedBPDataMaster,
                                sSGXMiscInfo.uData.sSGXBreakpointInfo.ui32TrappedBPTag);

            //			ui32SeqNum = sSGXBreakpointInfo.ui32SeqNum;

            
            /* Some range checking */

            ui32SetRangeBegin = psEdbpState->asBkpt[ui32Index].sDevVAddrStart.uiAddr;
            ui32SetRangeEnd = psEdbpState->asBkpt[ui32Index].sDevVAddrEndPlusOne.uiAddr;
            ui32HitRangeBegin = sSGXMiscInfo.uData.sSGXBreakpointInfo.sTrappedBPDevVAddr.uiAddr;
            ui32HitRangeEnd = sSGXMiscInfo.uData.sSGXBreakpointInfo.sTrappedBPDevVAddr.uiAddr + ((sSGXMiscInfo.uData.sSGXBreakpointInfo.ui32TrappedBPBurstLength+1)<<4);
            
            bOutOfRange = ui32HitRangeEnd <= ui32SetRangeBegin || ui32SetRangeEnd <= ui32HitRangeBegin;

            if(bOutOfRange)
            {
                psEdbpState->fnInfo("***  BREAKPOINT APPARENTLY HIT OUT OF RANGE ***\n");
                psEdbpState->fnInfo("ui32SetRangeBegin:0x%x ui32SetRangeEnd:0x%x "
                                    "ui32HitRangeBegin:0x%x ui32HitRangeEnd:0x%x\n",
                                    ui32SetRangeBegin, ui32SetRangeEnd,
                                    ui32HitRangeBegin, ui32HitRangeEnd);
                
                psEdbpState->asBkpt[ui32Index].bInError = IMG_TRUE;
            }

            /* TODO - also check whether the DM is expected, and the tag is right, and the read/write sense */

            psEdbpState->asBkpt[ui32Index].bHit = IMG_TRUE;
            
            if(psEdbpState->asBkpt[ui32Index].ui32Delayms > 0)
            {
                psEdbpState->fnInfo("Now waiting a while to let pipeline back up\n");
                sgxut_sleep_ms(psEdbpState->asBkpt[ui32Index].ui32Delayms);
            }
            
            psEdbpState->fnInfo("Resuming now\n");

            /* NB: must preserve ui32CoreNum between call to POLL and call to RESUME */
            sSGXMiscInfo.eRequest = SGX_MISC_INFO_REQUEST_RESUME_BREAKPOINT;
            eResult = SGXGetMiscInfo(psEdbpState->psSGXDevData, &sSGXMiscInfo);
            psEdbpState->fnInfo("SGXGetMiscInfo returned (3)\n");
            psEdbpState->fnInfoFailIfError(eResult);
            
            /* Check to see whether there's another breakpoint */
            sSGXMiscInfo.eRequest = SGX_MISC_INFO_REQUEST_POLL_BREAKPOINT;
            eResult = SGXGetMiscInfo(psEdbpState->psSGXDevData, &sSGXMiscInfo);
            psEdbpState->fnInfo("SGXGetMiscInfo returned (4)\n");
            psEdbpState->fnInfoFailIfError(eResult);
            
            /* Loop to handle the next one... */
            psEdbpState->fnInfo("edbp: loop to handle next bkpt\n");
        }
        
        psEdbpState->fnInfo("edbp: finished handling breakpoints, letting go of mutex\n");
        PVRSRVUnlockMutex(psEdbpState->hMutex);	
        psEdbpState->fnInfo("edbp: no longer have mutex\n");
    }
#else
    PVR_UNREFERENCED_PARAMETER(psEdbpState);
#endif
}

static IMG_VOID edbp_HandleBkptCallback(IMG_VOID *pPrivData)
{
    EDBP_STATE_DATA   *psEdbpState;

    psEdbpState = (EDBP_STATE_DATA *)pPrivData;

    edbp_HandleBkpt(psEdbpState);
}

static IMG_VOID edbp_UninstallAllBreakpoints(EDBP_STATE_DATA  *psEdbpState)
{
#if defined SGX_FEATURE_DATA_BREAKPOINTS
    /* Insert a data breakpoint at the given address */
    IMG_UINT32          ui32BPIndex;
    PVRSRV_ERROR		eResult;
    SGX_MISC_INFO		sSGXMiscInfo;

    for (ui32BPIndex = 0; ui32BPIndex < EDBP_MAXBKPTS; ui32BPIndex++)
    {
        psEdbpState->fnInfo("Calling SGXGetMiscInfo to clear Data Breakpoint %u\n",
                            ui32BPIndex);

        sSGXMiscInfo.eRequest = SGX_MISC_INFO_REQUEST_SET_BREAKPOINT;
        sSGXMiscInfo.uData.sSGXBreakpointInfo.bBPEnable = IMG_FALSE;
        sSGXMiscInfo.uData.sSGXBreakpointInfo.ui32BPIndex = ui32BPIndex;
    
        eResult = SGXGetMiscInfo(psEdbpState->psSGXDevData, &sSGXMiscInfo);
        psEdbpState->fnInfoFailIfError(eResult);
    }
#else
    PVR_UNREFERENCED_PARAMETER(psEdbpState);
#endif
}


static IMG_VOID edbp_PostCheck(EDBP_STATE_DATA *psEdbpState)
{
    IMG_UINT32 ui32BPIndex;
    IMG_UINT32 ui32NumValid;
    IMG_UINT32 ui32NumInstalled;
    IMG_UINT32 ui32NumUnexpected;
    IMG_UINT32 ui32NumInError;

    ui32NumValid = 0;
    ui32NumInstalled = 0;
    ui32NumUnexpected = 0;
    ui32NumInError = 0;

    for(ui32BPIndex = 0; ui32BPIndex < EDBP_MAXBKPTS; ui32BPIndex++)
    {
        if(psEdbpState->asBkpt[ui32BPIndex].bValid)
        {
            ui32NumValid++;

            if(psEdbpState->asBkpt[ui32BPIndex].bInstalled)
            {
                ui32NumInstalled++;
            }
            else
            {
                DPF("WARNING: Breakpoint %d was not installed - was the buffer name correct?\n", ui32BPIndex);
            }

            if(psEdbpState->asBkpt[ui32BPIndex].bHit && psEdbpState->asBkpt[ui32BPIndex].bMustNotHit)
            {
                ui32NumUnexpected++;
                DPF("ERROR: Breakpoint %d was hit but it was not supposed to be\n", ui32BPIndex);
            }

            if(!psEdbpState->asBkpt[ui32BPIndex].bHit && psEdbpState->asBkpt[ui32BPIndex].bMustHit)
            {
                ui32NumUnexpected++;
                DPF("ERROR: Breakpoint %d was not hit but it was supposed to be\n", ui32BPIndex);
            }

            if(psEdbpState->asBkpt[ui32BPIndex].bInError)
            {
                ui32NumInError++;
                DPF("ERROR: Breakpoint %d is in error\n", ui32BPIndex);
            }
        }
    }

    if(ui32NumValid > 0 && ui32NumValid != ui32NumInstalled)
    {
        DPF("WARNING: Not all breakpoints were installed\n");
    }

    if(ui32NumUnexpected)
    {
        DPF("ERROR: Some breakpoints did not behave as expected.  ** TEST FAIL ** \n");
    }

    if(ui32NumInError)
    {
        DPF("ERROR: %u breakpoints were in error.  ** TEST FAIL ** \n", ui32NumInError);
    }

    if(ui32NumValid == 0)
    {
        DPF("WARNING: No valid breakpoints\n");
    }
}

#if defined(SRFT_SUPPORT_PTHREADS)
static IMG_VOID *edbp_HandleBkptThread(IMG_VOID *pvArg)
{
    volatile EDBP_HANDLE_BKPT_THREAD_DATA *psThreadData = (EDBP_HANDLE_BKPT_THREAD_DATA *)pvArg;
    EDBP_STATE_DATA *psEdbpState = psThreadData->psEdbpState;
    IMG_UINT32 ui32WaitTimems = psThreadData->ui32WaitTimems;
#if defined (SUPPORT_SID_INTERFACE)
    IMG_EVENTSID hOSEventObject = psThreadData->hOSEventObject;
#else
    IMG_HANDLE hOSEventObject = psThreadData->hOSEventObject;
#endif
    PVRSRV_CONNECTION *psConnection = psThreadData->psConnection;
    PVRSRV_ERROR	eResult;

    ui32WaitTimems = 50;

    psEdbpState->fnInfo("edbp_HandleBkptThread start\n");
 
    while (!psThreadData->bStop)
    {
        edbp_HandleBkpt(psEdbpState);

#if defined (SUPPORT_SID_INTERFACE)
        if (hOSEventObject != 0)
#else
        if (hOSEventObject != IMG_NULL)
#endif
        {
            eResult = PVRSRVEventObjectWait(psConnection, hOSEventObject);
            if (eResult == PVRSRV_ERROR_TIMEOUT)
            {
                psEdbpState->fnInfo("edbp_HandleBkptThread: PVRSRVEventObjectWait returned PVRSRV_ERROR_TIMEOUT (retrying)\n");
                eResult = PVRSRV_OK;
            }
            sgxut_fail_if_error_quiet(eResult);
        }
        else
        {
            sgxut_sleep_ms(ui32WaitTimems);
        }
    }

    psEdbpState->fnInfo("edbp_HandleBkptThread stop\n");
    
    return IMG_NULL;
}

#if defined (SUPPORT_SID_INTERFACE)
static IMG_VOID edbp_HandleBkptThreadStart(EDBP_HANDLE_BKPT_THREAD_DATA *psThreadData, PVRSRV_CONNECTION *psConnection, EDBP_STATE_DATA *psEdbpState, IMG_EVENTSID hOSEventObject)
#else
static IMG_VOID edbp_HandleBkptThreadStart(EDBP_HANDLE_BKPT_THREAD_DATA *psThreadData, PVRSRV_CONNECTION *psConnection, EDBP_STATE_DATA *psEdbpState, IMG_HANDLE hOSEventObject)
#endif
{
    psThreadData->psConnection = psConnection;
    psThreadData->bStop = IMG_FALSE;
    psThreadData->psEdbpState = psEdbpState;
    psThreadData->ui32WaitTimems = 50;
    psThreadData->hOSEventObject = hOSEventObject;

    if (pthread_create(&psThreadData->sPThread, IMG_NULL, edbp_HandleBkptThread, psThreadData) != 0)
    {
        DPF("edbp_HandleBkptThreadStart: pthread_create failed");
    }
}

static IMG_VOID edbp_HandleBkptThreadStop(EDBP_HANDLE_BKPT_THREAD_DATA *psThreadData)
{
    //	IMG_HANDLE hOSEventObject = psThreadData->hOSEventObject;
    //	PVRSRV_CONNECTION *psConnection = psThreadData->psConnection;

    psThreadData->bStop = IMG_TRUE;

    /* TODO:  ought to signal event object to wake up the thread... */
    /* HOW? */
    //	PVRSRVEventObjectSignal(psConnection, hOSEventObject);

    if (pthread_join(psThreadData->sPThread, IMG_NULL) != 0)
    {
        DPF("edbp_HandleBkptThreadStop: pthread_join failed");
    }
}
#endif

/*********************************************************************************
 Function		:	srft_EncodeDmaBurst

 Description	:	Encode a DMA burst.

 Parameters		:	pui32DMAControl		- DMA control words
                    ui32DestOffset		- Destination offset in the attribute buffer.
                    ui32BurstSize		- Size of the burst in ui32ords.
                    ui32Address			- Source address for the burst.

 Return			:	The number of DMA transfers required.
*********************************************************************************/
static IMG_UINT32 srft_EncodeDmaBurst(IMG_UINT32 *pui32DMAControl,
                                      IMG_UINT32 ui32DestOffset,
                                      IMG_UINT32 ui32BurstSize,
                                      IMG_DEV_VIRTADDR uSrcAddress)
{
#if (EURASIA_PDS_DOUTD1_MAXBURST != EURASIA_PDS_DOUTD1_BSIZE_MAX)
    IMG_UINT32 ui32Factor, ui32Lines;
#endif

    PVR_ASSERT(ui32BurstSize > 0);
    PVR_ASSERT(ui32BurstSize <= EURASIA_PDS_DOUTD1_MAXBURST);

    /*
        Fast encoding for bursts which only require one line.
    */
    if (ui32BurstSize <= EURASIA_PDS_DOUTD1_BSIZE_MAX)
    {
        pui32DMAControl[0] = (uSrcAddress.uiAddr  << EURASIA_PDS_DOUTD0_SBASE_SHIFT);
        pui32DMAControl[1] = ((ui32BurstSize - 1) << EURASIA_PDS_DOUTD1_BSIZE_SHIFT) | /* PRQA S 3382 */ /* covered by assert */
                              (ui32DestOffset	  << EURASIA_PDS_DOUTD1_AO_SHIFT);
        return 1;
    }

#if (EURASIA_PDS_DOUTD1_MAXBURST != EURASIA_PDS_DOUTD1_BSIZE_MAX)

    #if defined(DEBUG)
    {
        static IMG_BOOL bFactorTableChecked = IMG_FALSE;

        /*
            Validate the table.
        */
        if (!bFactorTableChecked)
        {
            IMG_BYTE abyFactorTable[EURASIA_PDS_DOUTD1_MAXBURST + 1];

            IMG_UINT32 i, j;

            for (i = 1; i <= EURASIA_PDS_DOUTD1_MAXBURST; i++)
            {
                abyFactorTable[i] = EURASIA_PDS_DOUTD1_BSIZE_MAX;
                for (j = EURASIA_PDS_DOUTD1_BSIZE_MAX; j > 0; j--)
                {
                    if ((i % j) == 0 && (i / j) <= EURASIA_PDS_DOUTD1_BLINES_MAX)
                    {
                        abyFactorTable[i - 1] = (IMG_BYTE)j;

                        break;
                    }
                }
            }

            for (i = 0; i < EURASIA_PDS_DOUTD1_MAXBURST; i++)
            {
                PVR_ASSERT(g_abyFactorTable[i] == abyFactorTable[i]);
            }

            bFactorTableChecked = IMG_TRUE;
        }
    }
    #endif /* DEBUG */

    /*
        Breakdown the burst size into a dma transfer with multiple lines and one
        with a single line.
    */
    ui32Factor = g_abyFactorTable[ui32BurstSize - 1];	/* PRQA S 3689 */ /* ui32BurstSize > 0 */
    ui32Lines = ui32BurstSize / ui32Factor;

    PVR_ASSERT(ui32Lines <= EURASIA_PDS_DOUTD1_BLINES_MAX);

    pui32DMAControl[0] = (uSrcAddress.uiAddr << EURASIA_PDS_DOUTD0_SBASE_SHIFT);
    pui32DMAControl[1] = ((ui32Factor - 1)	<< EURASIA_PDS_DOUTD1_BSIZE_SHIFT)  |
                         ((ui32Lines - 1)	<< EURASIA_PDS_DOUTD1_BLINES_SHIFT) |
                         ((ui32Factor - 1)	<< EURASIA_PDS_DOUTD1_STRIDE_SHIFT) |
                         (ui32DestOffset	<< EURASIA_PDS_DOUTD1_AO_SHIFT);

    if ((ui32BurstSize % ui32Factor) != 0)
    {
        ui32DestOffset += ui32Lines * ui32Factor;
        uSrcAddress.uiAddr += ui32Lines * ui32Factor * sizeof(IMG_UINT32);

        pui32DMAControl[2] = (uSrcAddress.uiAddr					<< EURASIA_PDS_DOUTD0_SBASE_SHIFT);
        pui32DMAControl[3] = (((ui32BurstSize % ui32Factor) - 1)	<< EURASIA_PDS_DOUTD1_BSIZE_SHIFT) |
                             (ui32DestOffset						<< EURASIA_PDS_DOUTD1_AO_SHIFT);
        return 2;
    }

#endif /* #if (EURASIA_PDS_DOUTD1_MAXBURST != EURASIA_PDS_DOUTD1_BSIZE_MAX) */

    return 1;
}


/*********************************************************************************
 Function		:	srft_InsertSlowDown

 Description	:	Insert no-op instructions into a USSE program to slow it down.

 Parameters		:	ppui32USEBuffer		- pointer to USE code buffer

 Return			:	None
*********************************************************************************/
static IMG_VOID srft_InsertSlowDown(IMG_UINT32	**ppui32USEBuffer,
                                    IMG_UINT32	ui32OuterLoop,
                                    IMG_UINT32	ui32InnerLoop)
{
    IMG_UINT32	*pui32USEBuffer = *ppui32USEBuffer;

    /* mov				r1, #<outer loop> */
    *pui32USEBuffer++ = (1 << EURASIA_USE0_DST_SHIFT)											|
                        ((ui32OuterLoop << EURASIA_USE0_LIMM_IMML21_SHIFT) & ~EURASIA_USE0_LIMM_IMML21_CLRMSK);
    *pui32USEBuffer++ = (EURASIA_USE1_OP_SPECIAL			<< EURASIA_USE1_OP_SHIFT)				|
                        (EURASIA_USE1_OTHER_OP2_LIMM		<< EURASIA_USE1_OTHER_OP2_SHIFT)		|
                        (EURASIA_USE1_SPECIAL_OPCAT_OTHER	<< EURASIA_USE1_SPECIAL_OPCAT_SHIFT)	|
                        (((ui32OuterLoop >> 21) << EURASIA_USE1_LIMM_IMM2521_SHIFT) & ~EURASIA_USE1_LIMM_IMM2521_CLRMSK) |
                        (((ui32OuterLoop >> 26) << EURASIA_USE1_LIMM_IMM3126_SHIFT) & ~EURASIA_USE1_LIMM_IMM3126_CLRMSK);

    /* mov				r0, #<inner loop> */
    *pui32USEBuffer++ = (0 << EURASIA_USE0_DST_SHIFT)											|
                        ((ui32InnerLoop << EURASIA_USE0_LIMM_IMML21_SHIFT) & ~EURASIA_USE0_LIMM_IMML21_CLRMSK);
    *pui32USEBuffer++ = (EURASIA_USE1_OP_SPECIAL			<< EURASIA_USE1_OP_SHIFT)				|
                        (EURASIA_USE1_OTHER_OP2_LIMM		<< EURASIA_USE1_OTHER_OP2_SHIFT)		|
                        (EURASIA_USE1_SPECIAL_OPCAT_OTHER	<< EURASIA_USE1_SPECIAL_OPCAT_SHIFT)	|
                        (((ui32InnerLoop >> 21) << EURASIA_USE1_LIMM_IMM2521_SHIFT) & ~EURASIA_USE1_LIMM_IMM2521_CLRMSK) |
                        (((ui32InnerLoop >> 26) << EURASIA_USE1_LIMM_IMM3126_SHIFT) & ~EURASIA_USE1_LIMM_IMM3126_CLRMSK);

    /* isub16.testz	r0, p0, r0, #1
     * !p0	br -1
     */
    *pui32USEBuffer++ =  (EURASIA_USE0_S1STDBANK_TEMP << EURASIA_USE0_S1BANK_SHIFT) |
                         (EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT) |
                         (0 << EURASIA_USE0_DST_SHIFT) |
                         (0 << EURASIA_USE0_SRC1_SHIFT) |
                         (1 << EURASIA_USE0_SRC2_SHIFT) |
                         (EURASIA_USE0_TEST_ALUSEL_I16 << EURASIA_USE0_TEST_ALUSEL_SHIFT) |
                         (EURASIA_USE0_TEST_ALUOP_I16_ISUB << EURASIA_USE0_TEST_ALUOP_SHIFT) |
                         EURASIA_USE1_S2BEXT |
                         EURASIA_USE0_TEST_WBEN;

    *pui32USEBuffer++ =  (EURASIA_USE1_OP_TEST << EURASIA_USE1_OP_SHIFT) |
                         (EURASIA_USE1_EPRED_ALWAYS << EURASIA_USE1_EPRED_SHIFT) |
                         (SRFT_OPTEST_USE1_RMSKCNT << EURASIA_USE1_RMSKCNT_SHIFT) |
                         (aui32RegTypeToUSE1DestBank[USE_REGTYPE_TEMP] << EURASIA_USE1_D1BANK_SHIFT) |
                         (aui32RegTypeToUSE1DestBExt[USE_REGTYPE_TEMP]) |
                         (aui32RegTypeToUSE1Src1BExt[USE_REGTYPE_TEMP]) |
                         (aui32RegTypeToUSE1Src2BExt[USE_REGTYPE_IMMEDIATE]) |
                         (0 << EURASIA_USE1_TEST_PDST_SHIFT) |
                         (EURASIA_USE1_TEST_CHANCC_SELECT0 << EURASIA_USE1_TEST_CHANCC_SHIFT) |
                         (EURASIA_USE1_TEST_STST_NONE << EURASIA_USE1_TEST_STST_SHIFT) |
                         (EURASIA_USE1_TEST_ZTST_ZERO << EURASIA_USE1_TEST_ZTST_SHIFT) |
                         EURASIA_USE1_TEST_CRCOMB_AND;

    *pui32USEBuffer++ = 	(( ~0UL << EURASIA_USE0_BRANCH_OFFSET_SHIFT) & ~EURASIA_USE0_BRANCH_OFFSET_CLRMSK);
    *pui32USEBuffer++ = 	(EURASIA_USE1_EPRED_NOTP0 << EURASIA_USE1_EPRED_SHIFT) |
                            (EURASIA_USE1_FLOWCTRL_OP2_BR << EURASIA_USE1_FLOWCTRL_OP2_SHIFT) |
                            (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT);

    /* isub16.testz	r1, p0, r1, #1
     * p0	br -4
     */
    *pui32USEBuffer++ =  (EURASIA_USE0_S1STDBANK_TEMP << EURASIA_USE0_S1BANK_SHIFT) |
                         (EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT) |
                         (1 << EURASIA_USE0_DST_SHIFT) |
                         (1 << EURASIA_USE0_SRC1_SHIFT) |
                         (1 << EURASIA_USE0_SRC2_SHIFT) |
                         (EURASIA_USE0_TEST_ALUSEL_I16 << EURASIA_USE0_TEST_ALUSEL_SHIFT) |
                         (EURASIA_USE0_TEST_ALUOP_I16_ISUB << EURASIA_USE0_TEST_ALUOP_SHIFT) |
                         EURASIA_USE1_S2BEXT |
                         EURASIA_USE0_TEST_WBEN;

    *pui32USEBuffer++ =  (EURASIA_USE1_OP_TEST << EURASIA_USE1_OP_SHIFT) |
                         (EURASIA_USE1_EPRED_ALWAYS << EURASIA_USE1_EPRED_SHIFT) |
                         (SRFT_OPTEST_USE1_RMSKCNT << EURASIA_USE1_RMSKCNT_SHIFT) |
                         (aui32RegTypeToUSE1DestBank[USE_REGTYPE_TEMP] << EURASIA_USE1_D1BANK_SHIFT) |
                         (aui32RegTypeToUSE1DestBExt[USE_REGTYPE_TEMP]) |
                         (aui32RegTypeToUSE1Src1BExt[USE_REGTYPE_TEMP]) |
                         (aui32RegTypeToUSE1Src2BExt[USE_REGTYPE_IMMEDIATE]) |
                         (0 << EURASIA_USE1_TEST_PDST_SHIFT) |
                         (EURASIA_USE1_TEST_CHANCC_SELECT0 << EURASIA_USE1_TEST_CHANCC_SHIFT) |
                         (EURASIA_USE1_TEST_STST_NONE << EURASIA_USE1_TEST_STST_SHIFT) |
                         (EURASIA_USE1_TEST_ZTST_ZERO << EURASIA_USE1_TEST_ZTST_SHIFT) |
                         EURASIA_USE1_TEST_CRCOMB_AND;

    *pui32USEBuffer++ = 	(( ~3UL << EURASIA_USE0_BRANCH_OFFSET_SHIFT) & ~EURASIA_USE0_BRANCH_OFFSET_CLRMSK);
    *pui32USEBuffer++ = 	(EURASIA_USE1_EPRED_NOTP0 << EURASIA_USE1_EPRED_SHIFT) |
                            (EURASIA_USE1_FLOWCTRL_OP2_BR << EURASIA_USE1_FLOWCTRL_OP2_SHIFT) |
                            (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT);

    *ppui32USEBuffer = pui32USEBuffer;
}


/*********************************************************************************
 Function		:	srft_InsertMemWrite

 Description	:	Insert memory write (store) instructions into a USSE program to
                    test MMU flags.

 Parameters		:	ppui32USEBuffer		- pointer to USE code buffer
                    sDevVAddr			- address to write
                    ui32Value			- value to write

 Return			:	None
*********************************************************************************/
static IMG_VOID srft_InsertMemWrite(IMG_UINT32		**ppui32USEBuffer,
                                    IMG_DEV_VIRTADDR	*psDevVAddr,
                                    IMG_UINT32		ui32Value)
{
    IMG_UINT32	*pui32USEBuffer = *ppui32USEBuffer;

    /*
        mov r0, #(psDevVAddr->uiAddr)
    */
    *pui32USEBuffer++ = (0 << EURASIA_USE0_DST_SHIFT)											|
                        ((psDevVAddr->uiAddr << EURASIA_USE0_LIMM_IMML21_SHIFT) & ~EURASIA_USE0_LIMM_IMML21_CLRMSK);
    *pui32USEBuffer++ = (EURASIA_USE1_OP_SPECIAL			<< EURASIA_USE1_OP_SHIFT)				|
                        (EURASIA_USE1_OTHER_OP2_LIMM		<< EURASIA_USE1_OTHER_OP2_SHIFT)		|
                        (EURASIA_USE1_SPECIAL_OPCAT_OTHER	<< EURASIA_USE1_SPECIAL_OPCAT_SHIFT)	|
                        (((psDevVAddr->uiAddr >> 21) << EURASIA_USE1_LIMM_IMM2521_SHIFT) & ~EURASIA_USE1_LIMM_IMM2521_CLRMSK) |
                        (((psDevVAddr->uiAddr >> 26) << EURASIA_USE1_LIMM_IMM3126_SHIFT) & ~EURASIA_USE1_LIMM_IMM3126_CLRMSK);

    /*
        mov r1, #(ui32Value)
    */
    *pui32USEBuffer++ = (1 << EURASIA_USE0_DST_SHIFT)												|
                        ((ui32Value << EURASIA_USE0_LIMM_IMML21_SHIFT) & ~EURASIA_USE0_LIMM_IMML21_CLRMSK);
    *pui32USEBuffer++ = (EURASIA_USE1_OP_SPECIAL			<< EURASIA_USE1_OP_SHIFT)				|
                        (EURASIA_USE1_OTHER_OP2_LIMM		<< EURASIA_USE1_OTHER_OP2_SHIFT)		|
                        (EURASIA_USE1_SPECIAL_OPCAT_OTHER	<< EURASIA_USE1_SPECIAL_OPCAT_SHIFT)	|
                        (((ui32Value >> 21) << EURASIA_USE1_LIMM_IMM2521_SHIFT) & ~EURASIA_USE1_LIMM_IMM2521_CLRMSK) |
                        (((ui32Value >> 26) << EURASIA_USE1_LIMM_IMM3126_SHIFT) & ~EURASIA_USE1_LIMM_IMM3126_CLRMSK);

    /*
        stad [r0], r1
    */
    *pui32USEBuffer++ =	(EURASIA_USE0_S1EXTBANK_IMMEDIATE << EURASIA_USE0_S1BANK_SHIFT)	|
                        (1 << EURASIA_USE0_SRC2_SHIFT);
    *pui32USEBuffer++ = (EURASIA_USE1_OP_ST			<< EURASIA_USE1_OP_SHIFT)		|
                        (EURASIA_USE1_LDST_MOEEXPAND)								|
#if defined(SGX545) || defined(SGX543) || defined(SGX544) || defined(SGX554)
                        (EURASIA_USE1_LDST_DCCTL_STDBYPASSL1L2)						|
#else
                        (EURASIA_USE1_LDST_BPCACHE)									|
#endif /* SGX545 */
                        (EURASIA_USE1_S1BEXT);

    *ppui32USEBuffer = pui32USEBuffer;
}


#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
/*********************************************************************************
 Function		:	srft_InsertPhas

 Description	:	Insert a phas instruction into the USSE program.

 Parameters		:	ppui32USEBuffer		- pointer to USE code buffer
                    ui32Rate
                    ui32WaitCond

 Return			:	None
*********************************************************************************/
static IMG_VOID srft_InsertPhas(IMG_UINT32	**ppui32USEBuffer,
                                IMG_UINT32	ui32Rate,
                                IMG_UINT32	ui32WaitCond)
{
    PVR_USE_INST	sUSEInst;

    /* phas #0, #0, pixel, end, parallel */
    BuildPHASImmediate(&sUSEInst, 0, EURASIA_USE_OTHER2_PHAS_MODE_PARALLEL,
                       ui32Rate, ui32WaitCond, 0, 0x0);

    **ppui32USEBuffer = sUSEInst.ui32Word0;
    (*ppui32USEBuffer)++;
    **ppui32USEBuffer = sUSEInst.ui32Word1;
    (*ppui32USEBuffer)++;

}
#endif /* SGX_FEATURE_USE_UNLIMITED_PHASES */


static IMG_VOID srft_wait_for_render(IMG_CONST PVRSRV_CONNECTION	*psConnection,
                                     IMG_BOOL						bBlit,
                                     IMG_UINT32						ui32NumBackBuffers,
                                     PVRSRV_CLIENT_MEM_INFO			**apsBackBufferMemInfo,
                                     IMG_BOOL						bFlip,
                                     IMG_UINT32						ui32NumSwapChainBuffers,
                                     PVRSRV_CLIENT_MEM_INFO			**apsSwapBufferMemInfo,
                                     PVRSRV_CLIENT_MEM_INFO			*psSGXSystemBufferMemInfo,
#if defined (SUPPORT_SID_INTERFACE)
                                     IMG_EVENTSID					hOSEventObject,
#else
                                     IMG_HANDLE						hOSEventObject,
#endif
                                     IMG_BOOL						bVerbose,
                                     IMG_VOID                       (*pfnCallback)(IMG_VOID *),
                                     IMG_VOID                       *pvCallbackPriv)
{
    IMG_UINT32 i;

    if (bVerbose)
    {
        printf("waiting for renders to complete...\n");
    }

    if (bBlit)
    {
        for (i = 0; i < ui32NumBackBuffers; i++)
        {
            sgxut_wait_for_buffer(psConnection, apsBackBufferMemInfo[i], 200,
                                  "Back buffer", i, hOSEventObject, bVerbose,
                                  pfnCallback, pvCallbackPriv);
        }
    }

    if (bFlip)
    {
        for (i = 0; i < ui32NumSwapChainBuffers; i++)
        {
            sgxut_wait_for_buffer(psConnection, apsSwapBufferMemInfo[i], 200,
                                  "Swap Buffer", i, hOSEventObject, bVerbose,
                                  pfnCallback, pvCallbackPriv);
        }
    }
    else
    {
        sgxut_wait_for_buffer(psConnection, psSGXSystemBufferMemInfo, 200,
                              "Primary Surface", 0, hOSEventObject, bVerbose,
                              pfnCallback, pvCallbackPriv);
    }
}


static IMG_UINT32 srft_FloatToInt(IMG_FLOAT fVal)
{
    return (fVal >= 0) ? (IMG_UINT32)(fVal + 0.5) : (IMG_UINT32)(fVal - 0.5);
}


static IMG_UINT32 srft_InterpolateInt(IMG_FLOAT		fWeighting,
                                      IMG_UINT32	ui32Value1,
                                      IMG_UINT32	ui32Value2)
{
    return srft_FloatToInt((fWeighting * (IMG_FLOAT) ui32Value1) + (((IMG_FLOAT)1 - fWeighting) * (IMG_FLOAT) ui32Value2));
}

static IMG_FLOAT srft_InterpolateFloat(IMG_FLOAT	fWeighting,
                                       IMG_FLOAT	fValue1,
                                       IMG_FLOAT	fValue2)
{
    return fWeighting * fValue1 +
            ((IMG_FLOAT)1 - fWeighting) * fValue2;
}

static IMG_VOID srft_InterpolateColour(IMG_FLOAT	fWeighting,
                                       SRFT_COLOUR	*psColour1,
                                       SRFT_COLOUR	*psColour2,
                                       SRFT_COLOUR	*psResult)
{
    psResult->fRed = srft_InterpolateFloat(fWeighting, psColour1->fRed, psColour2->fRed);
    psResult->fGreen = srft_InterpolateFloat(fWeighting, psColour1->fGreen, psColour2->fGreen);
    psResult->fBlue = srft_InterpolateFloat(fWeighting, psColour1->fBlue, psColour2->fBlue);
}

static IMG_FLOAT srft_CyclePositionWeighting(IMG_UINT32	ui32CyclePosition,
                                             IMG_UINT32	ui32FramesPerRotation)
{
    IMG_FLOAT	fCyclePositionWeighting = ((IMG_FLOAT)ui32CyclePosition - ((IMG_FLOAT) ui32FramesPerRotation / 2)) / ((IMG_FLOAT) ui32FramesPerRotation / 2);
    fCyclePositionWeighting = ABS(fCyclePositionWeighting);
    return fCyclePositionWeighting;
}

static IMG_UINT32 srft_CyclePositionInterpolateInt(IMG_UINT32	ui32CyclePosition,
                                                   IMG_UINT32	ui32FramesPerRotation,
                                                   IMG_UINT32	ui32Value1,
                                                   IMG_UINT32	ui32Value2)
{
    return srft_InterpolateInt(srft_CyclePositionWeighting(ui32CyclePosition, ui32FramesPerRotation),
                               ui32Value1, ui32Value2);
}

static IMG_VOID srft_CyclePositionInterpolateColour(IMG_UINT32	ui32CyclePosition,
                                                    IMG_UINT32	ui32FramesPerRotation,
                                                    SRFT_COLOUR	*psColour1,
                                                    SRFT_COLOUR	*psColour2,
                                                    SRFT_COLOUR	*psResult)
{
    srft_InterpolateColour(srft_CyclePositionWeighting(ui32CyclePosition, ui32FramesPerRotation),
                           psColour1, psColour2, psResult);
}

static IMG_UINT32 *srft_WriteEndOfTileUSSECode(IMG_UINT32	*pui32BufferBase,
                                               IMG_UINT32	*pui32EmitWords,
                                               IMG_UINT32	ui32SideBand,
                                               IMG_BOOL		bFlushCache,
                                               IMG_BOOL		b2XMSAADontResolve)
{
#if defined(FIX_HW_BRN_31982) 
    PVR_UNREFERENCED_PARAMETER(bFlushCache);

    if (b2XMSAADontResolve)
    {
        return WriteEndOfTileUSSECode2xMSAA(pui32BufferBase, pui32EmitWords, ui32SideBand);
    }
#else
    PVR_UNREFERENCED_PARAMETER(b2XMSAADontResolve);
#endif
#if defined(SGX_FEATURE_WRITEBACK_DCU)
    #if defined(SGX_FEATURE_RENDER_TARGET_ARRAYS)
        return WriteEndOfTileUSSECode(pui32BufferBase, pui32EmitWords, ui32SideBand, IMG_NULL, bFlushCache);
    #else
        return WriteEndOfTileUSSECode(pui32BufferBase, pui32EmitWords, ui32SideBand, bFlushCache);
    #endif /* SGX_FEATURE_RENDER_TARGET_ARRAYS */
#else
    PVR_UNREFERENCED_PARAMETER(bFlushCache);

    return WriteEndOfTileUSSECode(pui32BufferBase, pui32EmitWords, ui32SideBand);
#endif
}

static IMG_VOID srft_WriteEndOfTileCode(IMG_UINT32				**ppui32USSECpuVAddr,
                                        IMG_DEV_VIRTADDR		*psUSSEDevVAddr,
                                        IMG_DEV_VIRTADDR		uUSEFragmentHeapBase,
                                        PDS_PIXEL_EVENT_PROGRAM	*psPixelEventProgram,
                                        IMG_UINT32				ui32MainRenderLineStride,
                                        IMG_UINT32				ui32SideBandWord,
                                        IMG_UINT32				ui32RenderAddress,
                                        IMG_BOOL				bTestHWRPBE,
                                        IMG_BOOL				bTestHWRPS,
                                        IMG_BOOL				bDebugProgramAddrs,
                                        IMG_BOOL				b2XMSAADontResolve,
                                        SRFT_SHARED				*psShared)
{
    IMG_UINT32			*pui32FragmentUSSEBuffer = *ppui32USSECpuVAddr;
    IMG_UINT32			aui32PBEEmitWords[STATE_PBE_DWORDS + 1] = {0};
    PBE_SURF_PARAMS 	sSurfParams;
    PBE_RENDER_PARAMS	sRenderParams;

    /**************************************************************************
    * Set PBE Emit words
    **************************************************************************/

    sSurfParams.eFormat = psShared->psPrimFormat->pixelformat;
    sSurfParams.sAddress.uiAddr = bTestHWRPBE ? SRFT_PAGE_FAULT_ADDRESS : ui32RenderAddress;
    sSurfParams.eMemLayout = IMG_MEMLAYOUT_STRIDED;
    sSurfParams.ui32LineStrideInPixels = ui32MainRenderLineStride;
    sSurfParams.eScaling = IMG_SCALING_NONE;
    sSurfParams.bEnableDithering = IMG_FALSE;
    sSurfParams.bZOnlyRender = IMG_FALSE;
    #if defined(SGX_FEATURE_PBE_GAMMACORRECTION)
    sSurfParams.bEnableGamma = IMG_FALSE;
    #endif /* SGX_FEATURE_PBE_GAMMACORRECTION */
    
    sRenderParams.eRotation = PVRSRV_ROTATE_0;
    sRenderParams.ui32MinXClip = 0;
    sRenderParams.ui32MaxXClip = psShared->ui32RectangleSizeX - 1;
    sRenderParams.ui32MinYClip = 0;
    sRenderParams.ui32MaxYClip = psShared->ui32RectangleSizeY - 1;
    sRenderParams.uSel.ui32SrcSelect = 0;
    
    WritePBEEmitState(&sSurfParams, &sRenderParams, &aui32PBEEmitWords[0]);

    /**************************************************************************
        End of tile USSE program.
    **************************************************************************/

    pui32FragmentUSSEBuffer = srft_WriteEndOfTileUSSECode(pui32FragmentUSSEBuffer,
                                                          aui32PBEEmitWords, ui32SideBandWord, IMG_FALSE, b2XMSAADontResolve);

    /**************************************************************************
        End of tile task control for PDS program.
    **************************************************************************/
    psPixelEventProgram->aui32EOTUSETaskControl[0]	= 0;
    psPixelEventProgram->aui32EOTUSETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL;
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
    psPixelEventProgram->aui32EOTUSETaskControl[0]	|= (STATE_PBE_DWORDS << EURASIA_PDS_DOUTU0_TRC_SHIFT)	|
                                                        (1	<< EURASIA_PDS_DOUTU0_TRC_SHIFT);
#if defined(SGX545)
    psPixelEventProgram->aui32EOTUSETaskControl[1]	|= EURASIA_PDS_DOUTU1_PBENABLE;
#else
    psPixelEventProgram->aui32EOTUSETaskControl[2]	= EURASIA_PDS_DOUTU2_PBENABLE;
#endif

#else
    psPixelEventProgram->aui32EOTUSETaskControl[1]	|= (STATE_PBE_DWORDS << EURASIA_PDS_DOUTU1_TRC_SHIFT)	|
                                                        (1	<< EURASIA_PDS_DOUTU1_TRC_SHIFT);
    psPixelEventProgram->aui32EOTUSETaskControl[2]	= EURASIA_PDS_DOUTU2_PBENABLE;
#endif /* SGX_FEATURE_USE_UNLIMITED_PHASES */

    if (bTestHWRPS)
    {
        /* Force a page fault to simulate a lockup - in order to test hardware recovery. */
        psUSSEDevVAddr->uiAddr = SRFT_PAGE_FAULT_ADDRESS;
    }

    SetUSEExecutionAddress(&psPixelEventProgram->aui32EOTUSETaskControl[0], 0, *psUSSEDevVAddr,
                            uUSEFragmentHeapBase, SGX_PIXSHADER_USE_CODE_BASE_INDEX);

    if (bDebugProgramAddrs)
    {
        DPF("0x%x: EOT USSE Program\n", psUSSEDevVAddr->uiAddr);
    }

    *ppui32USSECpuVAddr = pui32FragmentUSSEBuffer;
}


static IMG_VOID srft_WriteEndOfRenderCode(IMG_UINT32				**ppui32USSECpuVAddr,
                                          IMG_DEV_VIRTADDR			*psUSSEDevVAddr,
                                          IMG_DEV_VIRTADDR			uUSEFragmentHeapBase,
                                          PDS_PIXEL_EVENT_PROGRAM	*psPixelEventProgram,
                                          IMG_BOOL					bDebugProgramAddrs)
{
    IMG_UINT32	*pui32FragmentUSSEBuffer = *ppui32USSECpuVAddr;
    IMG_UINT32	ui32NumTempRegs;

    pui32FragmentUSSEBuffer = WriteEndOfRenderUSSECode(pui32FragmentUSSEBuffer);


    /**************************************************************************
        End of Render task control for PDS program.
    **************************************************************************/
    ui32NumTempRegs = USE_PIXELEVENT_EOR_NUM_TEMPS;
    psPixelEventProgram->aui32EORUSETaskControl[0] = 0;
    psPixelEventProgram->aui32EORUSETaskControl[1] = EURASIA_PDS_DOUTU1_MODE_PARALLEL;
    psPixelEventProgram->aui32EORUSETaskControl[2] = 0;
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
    psPixelEventProgram->aui32EORUSETaskControl[0] |= (ui32NumTempRegs << EURASIA_PDS_DOUTU0_TRC_SHIFT);

#if defined(SGX545)
    psPixelEventProgram->aui32EORUSETaskControl[1] |= EURASIA_PDS_DOUTU1_PBENABLE |
                                                        (1 << EURASIA_PDS_DOUTU1_ENDOFRENDER_SHIFT);
#else
    psPixelEventProgram->aui32EORUSETaskControl[2] = EURASIA_PDS_DOUTU2_PBENABLE |
                                                        (1 << EURASIA_PDS_DOUTU2_ENDOFRENDER_SHIFT);
#endif
#else
    psPixelEventProgram->aui32EORUSETaskControl[1] |= (ui32NumTempRegs << EURASIA_PDS_DOUTU1_TRC_SHIFT);
    psPixelEventProgram->aui32EORUSETaskControl[2] |= (1 << EURASIA_PDS_DOUTU2_ENDOFRENDER_SHIFT) |
                                                            EURASIA_PDS_DOUTU2_PBENABLE;
#endif /* SGX_FEATURE_USE_UNLIMITED_PHASES */

    SetUSEExecutionAddress(&psPixelEventProgram->aui32EORUSETaskControl[0], 0, *psUSSEDevVAddr,
                           uUSEFragmentHeapBase, SGX_PIXSHADER_USE_CODE_BASE_INDEX);

    if (bDebugProgramAddrs)
    {
        DPF("0x%x: EOR USSE Program\n", psUSSEDevVAddr->uiAddr);
    }

    *ppui32USSECpuVAddr = pui32FragmentUSSEBuffer;
}


static IMG_VOID srft_WritePTOFFCode(IMG_UINT32				**ppui32USSECpuVAddr,
                                    IMG_DEV_VIRTADDR		*psUSSEDevVAddr,
                                    IMG_DEV_VIRTADDR		uUSEFragmentHeapBase,
                                    PDS_PIXEL_EVENT_PROGRAM	*psPixelEventProgram,
                                    IMG_BOOL				bDebugProgramAddrs)
{
    IMG_UINT32	*pui32FragmentUSSEBuffer = *ppui32USSECpuVAddr;
    IMG_UINT32	ui32NumTempRegs;

    pui32FragmentUSSEBuffer = WritePTOffUSSECode(pui32FragmentUSSEBuffer);


    /**************************************************************************
        PTOFF task control for PDS program.
    **************************************************************************/
    ui32NumTempRegs = SGX_USE_MINTEMPREGS;
    psPixelEventProgram->aui32PTOFFUSETaskControl[0] = 0;
    psPixelEventProgram->aui32PTOFFUSETaskControl[1] = EURASIA_PDS_DOUTU1_MODE_PARALLEL;
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
    psPixelEventProgram->aui32PTOFFUSETaskControl[0] |=	(ui32NumTempRegs << EURASIA_PDS_DOUTU0_TRC_SHIFT) |
                                                    (1 << EURASIA_PDS_DOUTU0_TRC_SHIFT);
#else
    psPixelEventProgram->aui32PTOFFUSETaskControl[1] |=	(ui32NumTempRegs << EURASIA_PDS_DOUTU1_TRC_SHIFT) |
                                                    (1 << EURASIA_PDS_DOUTU1_TRC_SHIFT);
#endif /* SGX_FEATURE_USE_UNLIMITED_PHASES */
    psPixelEventProgram->aui32PTOFFUSETaskControl[2] = 0;

    SetUSEExecutionAddress(&psPixelEventProgram->aui32PTOFFUSETaskControl[0], 0, *psUSSEDevVAddr,
                            uUSEFragmentHeapBase, SGX_PIXSHADER_USE_CODE_BASE_INDEX);

    if (bDebugProgramAddrs)
    {
        DPF("0x%x: PTOFF USSE Program\n", psUSSEDevVAddr->uiAddr);
    }

    *ppui32USSECpuVAddr = pui32FragmentUSSEBuffer;
}


static IMG_VOID srft_WritePDSPixelEventCode(IMG_UINT32				**ppui32PDSCpuVAddr,
                                            IMG_DEV_VIRTADDR		*psPDSDevVAddr,
                                            PDS_PIXEL_EVENT_PROGRAM	*psPixelEventProgram,
                                            PVRSRV_SGX_CLIENT_INFO	*psSGXInfo,
                                            IMG_UINT32				*pui32StateSize,
                                            IMG_BOOL				bDebugProgramAddrs,
                                            IMG_BOOL				b2XMSAADontResolve)
{
    IMG_UINT32	ui32StateSize = PDS_PIXEVENT_NUM_PAS;

    if (b2XMSAADontResolve)
    {
#if defined(FIX_HW_BRN_31982)
        *ppui32PDSCpuVAddr = PDSGeneratePixelEventProgramTileXY(psPixelEventProgram,
                                                                *ppui32PDSCpuVAddr);
#else
        *ppui32PDSCpuVAddr = PDSGeneratePixelEventProgram(psPixelEventProgram,
                                                          *ppui32PDSCpuVAddr,
                                                          IMG_TRUE, IMG_FALSE, 2);
#endif
    }
    else
    {
        *ppui32PDSCpuVAddr = PDSGeneratePixelEventProgram(psPixelEventProgram,
                                                          *ppui32PDSCpuVAddr,
                                                          IMG_FALSE, IMG_FALSE, 0);
    }

    if (bDebugProgramAddrs)
    {
        DPF("0x%x: Pixel Event PDS Program\n\n", psPDSDevVAddr->uiAddr);
    }

    #if !defined(SGX_FEATURE_EDM_VERTEX_PDSADDR_FULL_RANGE)
    /*
        Event data master PDS address can be offset into PDS
        executable range dependant on Core revision.
    */
    psPDSDevVAddr->uiAddr -= psSGXInfo->uPDSExecBase.uiAddr;
    #else
    PVR_UNREFERENCED_PARAMETER(psSGXInfo);
    #endif

    #if defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
    ui32StateSize += USE_PIXELEVENT_NUM_TEMPS * sizeof(IMG_UINT32);
    #endif

    /*
        We must add an extra primary attribute and DMA into PA1 onwards
        as the first attribute may be required by EDM for pixel data master flushing
    */
    ui32StateSize += ((1 << EUR_CR_EVENT_PIXEL_PDS_INFO_ATTRIBUTE_SIZE_ALIGNSHIFT) - 1);

    *pui32StateSize = ui32StateSize;
}


static IMG_VOID srft_SetupHWRegs(PVR_HWREGS					*psHWRegs,
                                 IMG_DEV_VIRTADDR			sPixelEventPDSProgDevVAddr,
                                 PDS_PIXEL_EVENT_PROGRAM	*psPixelEventProgram,
                                 IMG_UINT32					ui32StateSize)
{
    IMG_FUINT32		sFPUVal;

    sFPUVal.fVal					= 1.0f;

    /* Background object depth & stencil/viewport registers */
    psHWRegs->ui32ISPBGObjDepth		= sFPUVal.ui32Val;
    psHWRegs->ui32ISPBGObj			= (1 << EUR_CR_ISP_BGOBJ_MASK_SHIFT) |
                                      (1 << EUR_CR_ISP_BGOBJ_ENABLEBGTAG_SHIFT);

    /* Ensure no pass spawn on first object (assumed to be colormasked on) */
    psHWRegs->ui32ISPIPFMisc			= (0xF << EUR_CR_ISP_IPFMISC_UPASSSTART_SHIFT);

    sFPUVal.fVal						= 1.0e-20f;
    psHWRegs->ui32ISPPerpendicular		= sFPUVal.ui32Val;
    psHWRegs->ui32ISPCullValue			= sFPUVal.ui32Val;
    psHWRegs->ui32ISPZLoadBase			= 0;
    psHWRegs->ui32ISPZStoreBase			= 0;
    psHWRegs->ui32ISPStencilLoadBase	= 0;
    psHWRegs->ui32ISPStencilStoreBase	= 0;
    psHWRegs->ui32ZLSCtl				= 0;
    psHWRegs->ui32ISPValidId			= 0;

    /* Depth bias: units adjust of -23 */
    psHWRegs->ui32ISPDBias			= 0x000000E9;

    psHWRegs->ui32ISPSceneBGObj		= 0;

    /* Store info about our generated pixel event program */
    psHWRegs->ui32EDMPixelPDSAddr		= sPixelEventPDSProgDevVAddr.uiAddr;
    psHWRegs->ui32EDMPixelPDSDataSize	= (psPixelEventProgram->ui32DataSize >> EUR_CR_EVENT_PIXEL_PDS_INFO_ATTRIBUTE_SIZE_ALIGNSHIFT);
    psHWRegs->ui32EDMPixelPDSInfo		= ((ui32StateSize >> EUR_CR_EVENT_PIXEL_PDS_INFO_ATTRIBUTE_SIZE_ALIGNSHIFT)
                                                      << EUR_CR_EVENT_PIXEL_PDS_INFO_ATTRIBUTE_SIZE_SHIFT);
#if defined(EUR_CR_EVENT_PIXEL_PDS_INFO_USE_PIPELINE_BOTH)
    psHWRegs->ui32EDMPixelPDSInfo		|= EUR_CR_EVENT_PIXEL_PDS_INFO_USE_PIPELINE_BOTH;
#endif
#if !defined(SGX545) && !defined(SGX543) && !defined(SGX544) && !defined(SGX554)
    psHWRegs->ui32EDMPixelPDSInfo		|= EUR_CR_EVENT_PIXEL_PDS_INFO_DM_PIXEL	|
                                       EUR_CR_EVENT_PIXEL_PDS_INFO_PNS_MASK;
#endif /* !SGX545 && !SGX543 && !SGX544 && !SGX554 */
    psHWRegs->ui32PixelBE				= 0;

    /* Set OpenGL mode, enable W-clamping and set a threshold of 2 for global macro-tile */
    psHWRegs->ui32MTECtrl				= (1 << EUR_CR_MTE_CTRL_WCLAMPEN_SHIFT)	|
                                      (1 << EUR_CR_MTE_CTRL_OPENGL_SHIFT)	|
                                      (2 << EUR_CR_MTE_CTRL_GLOBALMACROTILETHRESH_SHIFT);
}


static IMG_VOID srft_WriteSecondaryPixelCode(IMG_UINT32						**ppui32USSECpuVAddr,
                                             IMG_DEV_VIRTADDR				*psUSSEDevVAddr,
                                             IMG_UINT32						**ppui32PDSCpuVAddr,
                                             IMG_DEV_VIRTADDR				*psPDSDevVAddr,
                                             PDS_PIXEL_SHADER_SA_PROGRAM	*psPixelShaderSAProgram,
                                             PVRSRV_SGX_CLIENT_INFO			*psSGXInfo,
                                             IMG_DEV_VIRTADDR				uUSEFragmentHeapBase,
                                             IMG_BOOL						bDebugProgramAddrs)
{
    IMG_UINT32	*pui32FragmentUSSEBuffer = *ppui32USSECpuVAddr;
    IMG_UINT32	*pui32PDSFragmentBuffer = *ppui32PDSCpuVAddr;
    IMG_UINT32	ui32NumTempRegs;

    /*
        Write Secondary Pixel USSE program.
    */

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
    srft_InsertPhas(&pui32FragmentUSSEBuffer,
                    EURASIA_USE_OTHER2_PHAS_RATE_PIXEL,
                    EURASIA_USE_OTHER2_PHAS_WAITCOND_END);
#endif /* SGX_FEATURE_USE_UNLIMITED_PHASES */

    /* nop.end */
    *pui32FragmentUSSEBuffer++ = 0;
    *pui32FragmentUSSEBuffer++ = (EURASIA_USE1_OP_SPECIAL			<< EURASIA_USE1_OP_SHIFT)			|
                                (EURASIA_USE1_FLOWCTRL_OP2_NOP		<< EURASIA_USE1_FLOWCTRL_OP2_SHIFT)	|
                                 EURASIA_USE1_END;

    /*
        Write secondary Pixel PDS program.
    */
    psPixelShaderSAProgram->ui32NumDMAKicks = 0;
    psPixelShaderSAProgram->bKickUSE = IMG_TRUE;
    psPixelShaderSAProgram->bKickUSEDummyProgram = IMG_FALSE;

    ui32NumTempRegs = MAX(0, SGX_USE_MINTEMPREGS);
    psPixelShaderSAProgram->aui32USETaskControl[0]	= 0;
    psPixelShaderSAProgram->aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL;
    psPixelShaderSAProgram->aui32USETaskControl[2]	= 0;
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
    psPixelShaderSAProgram->aui32USETaskControl[0]	|= ui32NumTempRegs << EURASIA_PDS_DOUTU0_TRC_SHIFT;
    psPixelShaderSAProgram->aui32USETaskControl[1]	|= EURASIA_PDS_DOUTU1_SDSOFT;
#else
    psPixelShaderSAProgram->aui32USETaskControl[1]	|= ui32NumTempRegs << EURASIA_PDS_DOUTU1_TRC_SHIFT;
    psPixelShaderSAProgram->aui32USETaskControl[2]	|= EURASIA_PDS_DOUTU2_SDSOFT;
#endif /* SGX_FEATURE_USE_UNLIMITED_PHASES */

    SetUSEExecutionAddress(&psPixelShaderSAProgram->aui32USETaskControl[0], 0, *psUSSEDevVAddr,
                            uUSEFragmentHeapBase, SGX_PIXSHADER_USE_CODE_BASE_INDEX);

    pui32PDSFragmentBuffer = PDSGeneratePixelShaderSAProgram(psPixelShaderSAProgram,
                                                             pui32PDSFragmentBuffer);

    if (bDebugProgramAddrs)
    {
        DPF("0x%x: Secondary PDS Pixel Program\n", psPDSDevVAddr->uiAddr);
        DPF("0x%x: Secondary USSE Pixel Program\n", psUSSEDevVAddr->uiAddr);
    }

#if defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE)
    PVR_UNREFERENCED_PARAMETER(psSGXInfo);
#else
    /* Pixel data master PDS address must be offset into PDS Executable range */
    psPDSDevVAddr->uiAddr -= psSGXInfo->uPDSExecBase.uiAddr;
#endif

    *ppui32USSECpuVAddr = pui32FragmentUSSEBuffer;
    *ppui32PDSCpuVAddr = pui32PDSFragmentBuffer;
}


static IMG_VOID srft_SetupTextureImageUnit(PDS_TEXTURE_IMAGE_UNIT	*psTextureImageUnit,
                                           IMG_UINT32				ui32FBTexFormat,
                                           IMG_UINT32				ui32Width,
                                           IMG_UINT32				ui32Height,
                                           PVRSRV_CLIENT_MEM_INFO	*psRenderSurfMemInfo)
{
    IMG_UINT32				ui32MemLayout;

    /* Set up for simple stride based unfiltered texture */
    psTextureImageUnit->ui32TAGControlWord0 = EURASIA_PDS_DOUTT0_NOTMIPMAP			|
                                              EURASIA_PDS_DOUTT0_ANISOCTL_NONE		|
                                              EURASIA_PDS_DOUTT0_MINFILTER_POINT	|
                                              EURASIA_PDS_DOUTT0_MAGFILTER_POINT	|
                                              EURASIA_PDS_DOUTT0_UADDRMODE_CLAMP	|
                                              EURASIA_PDS_DOUTT0_VADDRMODE_CLAMP  ;

    ui32MemLayout = EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE                       |
                    ((ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)	|
                    ((ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT) ;

    /* Set up texture format and texture address to match the render target */
    psTextureImageUnit->ui32TAGControlWord1 = ui32MemLayout | ui32FBTexFormat;

    psTextureImageUnit->ui32TAGControlWord2 = (psRenderSurfMemInfo->sDevVAddr.uiAddr >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT)
                                            << EURASIA_PDS_DOUTT2_TEXADDR_SHIFT;

#if defined(SGX545)
    psTextureImageUnit->ui32TAGControlWord3 = 1 << EURASIA_PDS_DOUTT3_TAGDATASIZE_SHIFT;
#else
#if defined(SGX_FEATURE_TAG_SWIZZLE)
    if(ui32FBTexFormat == EURASIA_PDS_DOUTT1_TEXFORMAT_U8888)
    {
        psTextureImageUnit->ui32TAGControlWord3 = EURASIA_PDS_DOUTT3_SWIZ_ARGB;
    }
    else
    {
        psTextureImageUnit->ui32TAGControlWord3 = EURASIA_PDS_DOUTT3_SWIZ_BGR;
    }

#endif
#endif /* SGX545 */
}


static IMG_VOID srft_WritePrimaryPixelCode(IMG_UINT32				**ppui32USSECpuVAddr,
                                           IMG_DEV_VIRTADDR			*psUSSEDevVAddr,
                                           IMG_UINT32				**ppui32PDSCpuVAddr,
                                           IMG_DEV_VIRTADDR			*psPDSDevVAddr,
                                           PDS_PIXEL_SHADER_PROGRAM	*psPixelShaderProgram,
                                           PVRSRV_SGX_CLIENT_INFO	*psSGXInfo,
                                           IMG_DEV_VIRTADDR			uUSEFragmentHeapBase,
                                           PDS_TEXTURE_IMAGE_UNIT	*psTextureImageUnit,
                                           IMG_BOOL					bIteratorDependency,
                                           IMG_UINT32				ui32SlowdownFactor,
                                           IMG_BOOL					bUSSEPixelWriteMem,
                                           PVRSRV_CLIENT_MEM_INFO	*psUSSEPixelMemFlagsTestMemInfo,
                                           IMG_BOOL					bDebugProgramAddrs,
                                           IMG_BOOL					bGouraud,
                                           IMG_CHAR					*szType)
{
    IMG_UINT32				*pui32FragmentUSSEBuffer = *ppui32USSECpuVAddr;
    IMG_UINT32				*pui32PDSFragmentBuffer = *ppui32PDSCpuVAddr;
    IMG_UINT32				ui32NumTempRegs;

    /*
        Write USSE primary pixel program.
    */
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
    srft_InsertPhas(&pui32FragmentUSSEBuffer,
                    EURASIA_USE_OTHER2_PHAS_RATE_PIXEL,
                    EURASIA_USE_OTHER2_PHAS_WAITCOND_END);
#endif /* SGX_FEATURE_USE_UNLIMITED_PHASES */

    if (ui32SlowdownFactor > 0)
    {
        srft_InsertSlowDown(&pui32FragmentUSSEBuffer, 1, ui32SlowdownFactor);
    }

    if (bUSSEPixelWriteMem)
    {
        srft_InsertMemWrite(&pui32FragmentUSSEBuffer,
                            &psUSSEPixelMemFlagsTestMemInfo->sDevVAddr, 0);
    }


    BuildMOV((PPVR_USE_INST)pui32FragmentUSSEBuffer,
             EURASIA_USE1_EPRED_ALWAYS,
             0 /* Repeat count */,
             EURASIA_USE1_RCNTSEL | EURASIA_USE1_END,
             USE_REGTYPE_OUTPUT, 0,
             USE_REGTYPE_PRIMATTR, 0,
             0 /* Src mod */);

    pui32FragmentUSSEBuffer += USE_INST_LENGTH;

    /*
        Write PDS primary pixel program.
    */
    ui32NumTempRegs = MAX(2, SGX_USE_MINTEMPREGS);

    ui32NumTempRegs += ((1 << EURASIA_PDS_DOUTU_TRC_ALIGNSHIFT) - 1);
    ui32NumTempRegs >>= EURASIA_PDS_DOUTU_TRC_ALIGNSHIFT;

    psPixelShaderProgram->aui32USETaskControl[0]	= 0;
    psPixelShaderProgram->aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL;
    psPixelShaderProgram->aui32USETaskControl[2]	= 0;
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
    psPixelShaderProgram->aui32USETaskControl[0]	|= ui32NumTempRegs << EURASIA_PDS_DOUTU0_TRC_SHIFT;
    psPixelShaderProgram->aui32USETaskControl[1]	|= (bIteratorDependency ? EURASIA_PDS_DOUTU1_ITERATORSDEPENDENCY : 0);
    psPixelShaderProgram->aui32USETaskControl[1]	|= ((psTextureImageUnit != IMG_NULL) ? EURASIA_PDS_DOUTU1_TEXTUREDEPENDENCY : 0);
    psPixelShaderProgram->aui32USETaskControl[1]	|= EURASIA_PDS_DOUTU1_SDSOFT;
#else
    psPixelShaderProgram->aui32USETaskControl[0]	|= (bIteratorDependency ? EURASIA_PDS_DOUTU0_ITERATORSDEPENDENCY : 0);
    psPixelShaderProgram->aui32USETaskControl[0]	|= ((psTextureImageUnit != IMG_NULL) ? EURASIA_PDS_DOUTU0_TEXTUREDEPENDENCY : 0);
    psPixelShaderProgram->aui32USETaskControl[1]	|= ui32NumTempRegs << EURASIA_PDS_DOUTU1_TRC_SHIFT;
    psPixelShaderProgram->aui32USETaskControl[2]	|= EURASIA_PDS_DOUTU2_SDSOFT;
#endif /* SGX545 */

    /* Set the execution address of the clear fragment shader */
    SetUSEExecutionAddress(&psPixelShaderProgram->aui32USETaskControl[0], 0, *psUSSEDevVAddr,
                            uUSEFragmentHeapBase, SGX_PIXSHADER_USE_CODE_BASE_INDEX);

    if (bDebugProgramAddrs)
    {
        DPF("0x%x: %s Pixel PDS Program\n", psPDSDevVAddr->uiAddr, szType);
        DPF("0x%x: %s Pixel USSE Program\n", psUSSEDevVAddr->uiAddr, szType);
    }

    /* Setup the FPU iterator control words*/
    psPixelShaderProgram->ui32NumFPUIterators = 1;

    if (psTextureImageUnit != IMG_NULL)
    {
        #if defined(SGX545)
        psPixelShaderProgram->aui32FPUIterators0[0] =
        #else
        psPixelShaderProgram->aui32FPUIterators[0] =
        #endif /* SGX545 */
                                                    EURASIA_PDS_DOUTI_USEISSUE_NONE		|
                                                    EURASIA_PDS_DOUTI_TEXISSUE_TC0		|
                                                    (bGouraud ? EURASIA_PDS_DOUTI_FLATSHADE_GOURAUD : 0)	|	
                                                    EURASIA_PDS_DOUTI_TEXLASTISSUE;
        psPixelShaderProgram->aui32TAGLayers[0]	= 0;

    }
    else
    {
        #if defined(SGX545)
        psPixelShaderProgram->aui32FPUIterators0[0] =
        #else
        psPixelShaderProgram->aui32FPUIterators[0] =
        #endif /* SGX545 */
                                                    (bGouraud ? EURASIA_PDS_DOUTI_FLATSHADE_GOURAUD : 0)	|	
                                                    EURASIA_PDS_DOUTI_USEISSUE_V0							|
                                                    EURASIA_PDS_DOUTI_TEXISSUE_NONE							|
                                                    EURASIA_PDS_DOUTI_USEPERSPECTIVE						|
                                                    EURASIA_PDS_DOUTI_USELASTISSUE							|
                                                    EURASIA_PDS_DOUTI_USEDIM_4D;

        psPixelShaderProgram->aui32TAGLayers[0]	= 0xFFFFFFFF;
    }

#if defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE)
    PVR_UNREFERENCED_PARAMETER(psSGXInfo);
#else
    /* Pixel data master PDS address must be offset into PDS Executable range */
    psPDSDevVAddr->uiAddr -= psSGXInfo->uPDSExecBase.uiAddr;
#endif

    /*
        Generate the PDS pixel state program.
    */
    pui32PDSFragmentBuffer = PDSGeneratePixelShaderProgram(psTextureImageUnit,
                                                           psPixelShaderProgram,
                                                           pui32PDSFragmentBuffer);

    *ppui32USSECpuVAddr = pui32FragmentUSSEBuffer;
    *ppui32PDSCpuVAddr = pui32PDSFragmentBuffer;
}


static IMG_VOID srft_SetupPDSStateWords(IMG_UINT32						*pui32PDSState,
                                        IMG_DEV_VIRTADDR				*psPixelSecondaryPDSProgDevVAddr,
                                        PDS_PIXEL_SHADER_SA_PROGRAM		*psPixelShaderSAProgram,
                                        IMG_DEV_VIRTADDR				*psPixelPrimaryPDSProgDevVAddr,
                                        PDS_PIXEL_SHADER_PROGRAM		*psPixelShaderProgram)
{
    /*
        Write the PDS pixel shader secondary attribute base address and data size
    */
    pui32PDSState[0] =  (((psPixelSecondaryPDSProgDevVAddr->uiAddr >> EURASIA_PDS_BASEADD_ALIGNSHIFT)
                            << EURASIA_PDS_BASEADD_SHIFT) & ~EURASIA_PDS_BASEADD_CLRMSK)					|
                            (((psPixelShaderSAProgram->ui32DataSize >> EURASIA_PDS_DATASIZE_ALIGNSHIFT)
                            << EURASIA_PDS_DATASIZE_SHIFT) & ~EURASIA_PDS_DATASIZE_CLRMSK);
    /*
        Write the DMS pixel control information
    */
    /* FIXME: Will need to take into account temps reserved for task switching on SGX_FEATURE_UNIFIED_TEMPS_AND_PAS */
    pui32PDSState[1] =	EURASIA_PDS_USE_SEC_EXEC				|
                        (0 << EURASIA_PDS_SECATTRSIZE_SHIFT)	|
                        (3 << EURASIA_PDS_USETASKSIZE_SHIFT)	|
#if defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE)
                        ((psPixelSecondaryPDSProgDevVAddr->uiAddr & EURASIA_PDS_BASEADD_MSB) ? EURASIA_PDS_BASEADD_SEC_MSB : 0) |
                        ((psPixelPrimaryPDSProgDevVAddr->uiAddr & EURASIA_PDS_BASEADD_MSB) ? EURASIA_PDS_BASEADD_PRIM_MSB : 0) |
#endif
                        ((EURASIA_PDS_PDSTASKSIZE_MAX << EURASIA_PDS_PDSTASKSIZE_SHIFT)	&
                        ~EURASIA_PDS_PDSTASKSIZE_CLRMSK)		|
                        (1 << EURASIA_PDS_PIXELSIZE_SHIFT);

    /*
        Write the PDS pixel shader base address and data size
    */
    pui32PDSState[2]	= (((psPixelPrimaryPDSProgDevVAddr->uiAddr >> EURASIA_PDS_BASEADD_ALIGNSHIFT)
                            << EURASIA_PDS_BASEADD_SHIFT) & ~EURASIA_PDS_BASEADD_CLRMSK)				|
                            (((psPixelShaderProgram->ui32DataSize >> EURASIA_PDS_DATASIZE_ALIGNSHIFT)
                            << EURASIA_PDS_DATASIZE_SHIFT) & ~EURASIA_PDS_DATASIZE_CLRMSK);
}

typedef union _floatorulong
{
    IMG_FLOAT f;
    IMG_UINT32 ul;
} FLOATORULONG;

static IMG_VOID srft_WriteMTEStateToVertexDataBuffer(IMG_UINT32				**ppui32VertexDataBuffer,
                                                     IMG_UINT32				*pui32PDSState)
{
    IMG_UINT32				*pui32VertexDataBuffer = *ppui32VertexDataBuffer;
    IMG_UINT32 				ui32StateHeader;
    IMG_UINT32 				ui32ISPControlA, ui32ISPControlB;
    const FLOATORULONG		fXCenter	= {0.0f};
    const FLOATORULONG		fXScale		= {1.0f};
    const FLOATORULONG		fYCenter	= {0.0f};
    const FLOATORULONG		fYScale		= {1.0f};
    const FLOATORULONG		fZCenter	= {0.0f};
    const FLOATORULONG		fZScale		= {1.0f};
    const FLOATORULONG		fWClamp		= {0.00001f};

    /* Header word defines content of following state block */
    ui32StateHeader =	EURASIA_TACTLPRES_ISPCTLFF0		|
                        EURASIA_TACTLPRES_ISPCTLFF1		|
                        //EURASIA_TACTLPRES_ISPCTLFF2	|
                    /*	EURASIA_TACTLPRES_ISPCTLBF0		| */
                    /*	EURASIA_TACTLPRES_ISPCTLBF1		| */
                    /*	EURASIA_TACTLPRES_ISPCTLBF2		| */
                        EURASIA_TACTLPRES_PDSSTATEPTR	|
                    /*	EURASIA_TACTLPRES_RGNCLIP		| */
                        EURASIA_TACTLPRES_VIEWPORT		|
                        EURASIA_TACTLPRES_WRAP			|
                        EURASIA_TACTLPRES_OUTSELECTS	|
                        EURASIA_TACTLPRES_WCLAMP		|
                        EURASIA_TACTLPRES_MTECTRL		|
                    /*	EURASIA_TACTLPRES_TERMINATE		| */
                        EURASIA_TACTLPRES_TEXSIZE		|
                        EURASIA_TACTLPRES_TEXFLOAT		|
                        0;

    ui32ISPControlA =	EURASIA_ISPA_PASSTYPE_OPAQUE	|
                        EURASIA_ISPA_DCMPMODE_ALWAYS	|
                        EURASIA_ISPA_OBJTYPE_TRI		|
                        EURASIA_ISPA_BPRES;

    /* Set colour mask */
    ui32ISPControlB = (0xFF << EURASIA_ISPB_UPASSCTL_SHIFT);

    /* Write block header */
    *pui32VertexDataBuffer++ = ui32StateHeader;

    /* EURASIA_TACTLPRES_ISPCTLFF0 */
    *pui32VertexDataBuffer++ = ui32ISPControlA;

    /* EURASIA_TACTLPRES_ISPCTLFF1 */
    *pui32VertexDataBuffer++ = ui32ISPControlB;

    /* EURASIA_TACTLPRES_ISPCTLFF2
    *pui32VertexDataBuffer++ = ui32ISPControlC; */

    /* EURASIA_TACTLPRES_PDSSTATEPTR */
    *pui32VertexDataBuffer++ = pui32PDSState[0];
    *pui32VertexDataBuffer++ = pui32PDSState[1];
    *pui32VertexDataBuffer++ = pui32PDSState[2];

    /* EURASIA_TACTLPRES_RGNCLIP
    *pui32VertexDataBuffer++ = aui32RegionClipWord[0];
    *pui32VertexDataBuffer++ = aui32RegionClipWord[1]; */

    /* EURASIA_TACTLPRES_VIEWPORT */
    *pui32VertexDataBuffer++ = fXCenter.ul;
    *pui32VertexDataBuffer++ = fXScale.ul;
    *pui32VertexDataBuffer++ = fYCenter.ul;
    *pui32VertexDataBuffer++ = fYScale.ul;
    *pui32VertexDataBuffer++ = fZCenter.ul;
    *pui32VertexDataBuffer++ = fZScale.ul;

    /* EURASIA_TACTLPRES_WRAP */
    *pui32VertexDataBuffer++ = 0;

    /* EURASIA_TACTLPRES_OUTSELECTS */
    *pui32VertexDataBuffer++ =	EURASIA_MTE_WPRESENT				|
                                EURASIA_MTE_BASE					|
                                (8 << EURASIA_MTE_VTXSIZE_SHIFT);

    /* EURASIA_TACTLPRES_WCLAMP */
    *pui32VertexDataBuffer++ = fWClamp.ul;

    /* EURASIA_TACTLPRES_MTECTRL */
    *pui32VertexDataBuffer =	EURASIA_MTE_PRETRANSFORM	|
                                EURASIA_MTE_CULLMODE_NONE;
    #if !defined(SGX545)
    *pui32VertexDataBuffer |=	EURASIA_MTE_SHADE_GOURAUD;
    #endif
    pui32VertexDataBuffer++;

    /* EURASIA_TACTLPRES_TEXSIZE */
    #if defined(SGX545)
    *pui32VertexDataBuffer++ = EURASIA_MTE_ATTRDIM_UVST << EURASIA_MTE_ATTRDIM0_SHIFT;
    #else
    *pui32VertexDataBuffer++ = 0;
    #endif /* SGX545 */

    /* EURASIA_TACTLPRES_TEXFLOAT */
    *pui32VertexDataBuffer++ = 0;

    *ppui32VertexDataBuffer	= pui32VertexDataBuffer;
}


static IMG_VOID srft_WriteMTEStateCode(IMG_DEV_VIRTADDR			*psMTEStateBlockDevVAddr,
                                       IMG_UINT32				**ppui32USSEVertexBuffer,
                                       IMG_DEV_VIRTADDR			*psUSSEDevVAddr,
                                       IMG_UINT32				**ppui32VertexPDSBuffer,
                                       IMG_DEV_VIRTADDR			*psPDSDevVAddr,
                                       PDS_STATE_COPY_PROGRAM	*psMTEStateCopyPDSProgram,
                                       PVRSRV_SGX_CLIENT_INFO	*psSGXInfo,
                                       IMG_DEV_VIRTADDR			uUSEVertexHeapBase,
                                       IMG_UINT32				ui32MTEStateUpdateDWORDCount,
                                       IMG_BOOL					bDebugProgramAddrs)
{
    IMG_UINT32				*pui32USSEVertexBuffer = *ppui32USSEVertexBuffer;
    IMG_UINT32				*pui32VertexPDSBuffer = *ppui32VertexPDSBuffer;
    IMG_UINT32				ui32NumTempRegs;

    /**************************************************************************
    * Write MTE state copy USE program
    **************************************************************************/

    pui32USSEVertexBuffer = USEGenWriteStateEmitProgram(pui32USSEVertexBuffer,
                                                        ui32MTEStateUpdateDWORDCount,
                                                        0, IMG_FALSE);

    /**************************************************************************
    * Write MTE state copy PDS program
    **************************************************************************/

    /* Setup the parameters for the PDS program. */
    ui32NumTempRegs = SGX_USE_MINTEMPREGS;
    psMTEStateCopyPDSProgram->ui32NumDMAKicks =
            srft_EncodeDmaBurst(psMTEStateCopyPDSProgram->aui32DMAControl, 0,
                                ui32MTEStateUpdateDWORDCount, *psMTEStateBlockDevVAddr);

    psMTEStateCopyPDSProgram->aui32USETaskControl[0]	= 0;
    psMTEStateCopyPDSProgram->aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL;
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
    psMTEStateCopyPDSProgram->aui32USETaskControl[0]	|= (ui32NumTempRegs << EURASIA_PDS_DOUTU0_TRC_SHIFT);
#else
    psMTEStateCopyPDSProgram->aui32USETaskControl[0]	|= EURASIA_PDS_DOUTU0_PDSDMADEPENDENCY;
    psMTEStateCopyPDSProgram->aui32USETaskControl[1]	|= (ui32NumTempRegs << EURASIA_PDS_DOUTU1_TRC_SHIFT);
#endif /* SGX545 */
    psMTEStateCopyPDSProgram->aui32USETaskControl[2]	= 0;

    SetUSEExecutionAddress(&psMTEStateCopyPDSProgram->aui32USETaskControl[0], 0, *psUSSEDevVAddr,
                            uUSEVertexHeapBase, SGX_VTXSHADER_USE_CODE_BASE_INDEX);

    /*	Generate the PDS state copy program */
    pui32VertexPDSBuffer = PDSGenerateStateCopyProgram(psMTEStateCopyPDSProgram,
                                                       pui32VertexPDSBuffer);

    if (bDebugProgramAddrs)
    {
        DPF("\n0x%x: MTE state copy PDS Program\n", psPDSDevVAddr->uiAddr);
        DPF("0x%x: MTE state copy USSE Program\n",	psUSSEDevVAddr->uiAddr);
    }

#if !defined(SGX_FEATURE_EDM_VERTEX_PDSADDR_FULL_RANGE)
    /* Adjust PDS execution address for restricted address range */
    psPDSDevVAddr->uiAddr -= psSGXInfo->uPDSExecBase.uiAddr;
#else
    PVR_UNREFERENCED_PARAMETER(psSGXInfo);
#endif

    *ppui32USSEVertexBuffer = pui32USSEVertexBuffer;
    *ppui32VertexPDSBuffer = pui32VertexPDSBuffer;
}


static IMG_VOID srft_WriteMTEStateToVDM(IMG_UINT32				**ppui32Buffer,
                                        IMG_DEV_VIRTADDR		*psMTEStateCopyPDSProgDevVAddr,
                                        PDS_STATE_COPY_PROGRAM	*psMTEStateCopyPDSProgram,
                                        IMG_UINT32				ui32MTEStateUpdateDWORDCount)
{
    IMG_UINT32			*pui32Buffer = *ppui32Buffer;
    static IMG_UINT32	ui32CurrentOutputStateBlockUSEPipe = 0;
    IMG_UINT32			ui32NumPartitions = 1;

    #if defined(SGX545)
    ui32NumPartitions = 2;
    #endif /* SGX545 */

    #if defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
    ui32MTEStateUpdateDWORDCount += SGX_USE_MINTEMPREGS;
    #endif

    *pui32Buffer++	= (EURASIA_TAOBJTYPE_STATE                      |
                      (((psMTEStateCopyPDSProgDevVAddr->uiAddr >> EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT)
                        << EURASIA_TAPDSSTATE_BASEADDR_SHIFT) & ~EURASIA_TAPDSSTATE_BASEADDR_CLRMSK));

    *pui32Buffer++	= ((((psMTEStateCopyPDSProgram->ui32DataSize >> EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT)
                            << EURASIA_TAPDSSTATE_DATASIZE_SHIFT) & ~EURASIA_TAPDSSTATE_DATASIZE_CLRMSK)		|
                        (EURASIA_TAPDSSTATE_USEPIPE_1 + ui32CurrentOutputStateBlockUSEPipe)			|
                        (ui32NumPartitions								<< EURASIA_TAPDSSTATE_PARTITIONS_SHIFT)	|
                         EURASIA_TAPDSSTATE_SD															|
                         EURASIA_TAPDSSTATE_MTE_EMIT													|
                        (SGXUT_ALIGNFIELD(ui32MTEStateUpdateDWORDCount, (EURASIA_TAPDSSTATE_USEATTRIBUTESIZE_ALIGNSHIFT - 2)) << EURASIA_TAPDSSTATE_USEATTRIBUTESIZE_SHIFT));

    /* Switch USE pipes for the next block */
    ui32CurrentOutputStateBlockUSEPipe++;
    ui32CurrentOutputStateBlockUSEPipe %= EURASIA_TAPDSSTATE_PIPECOUNT;

    *ppui32Buffer = pui32Buffer;
}


#define SRFT_VERTEX_SIZE 7
static IMG_VOID srft_WriteVertex(IMG_FLOAT		**ppfVertexDataBuffer,
                                 IMG_FLOAT		fVertexX,
                                 IMG_FLOAT		fVertexY,
                                 IMG_FLOAT		fVertexZ,
                                 SRFT_COLOUR	*psVertexColour)
{
    IMG_FLOAT	*pfVertexDataBuffer = *ppfVertexDataBuffer;

    pfVertexDataBuffer[0] = fVertexX;
    pfVertexDataBuffer[1] = fVertexY;
    pfVertexDataBuffer[2] = fVertexZ;
    pfVertexDataBuffer[3] = psVertexColour->fRed;
    pfVertexDataBuffer[4] = psVertexColour->fGreen;
    pfVertexDataBuffer[5] = psVertexColour->fBlue;
    pfVertexDataBuffer[6] = 0; /* Alpha channel */
    pfVertexDataBuffer += SRFT_VERTEX_SIZE;

    *ppfVertexDataBuffer = pfVertexDataBuffer;
}


static IMG_VOID srft_WriteClearObjToVertexDataBuffer(IMG_UINT32		**ppui32VertexDataBuffer,
                                                     SRFT_CONFIG	*psConfig,
                                                     IMG_UINT32		ui32RectangleSizeX,
                                                     IMG_UINT32		ui32RectangleSizeY)
{
    IMG_FLOAT	*pfVertexDataBuffer = (IMG_FLOAT *)*ppui32VertexDataBuffer;

    /* First triangle that covers the Upper triangular half of the render screen */
        
    /* Vtx 1 is (0, 0, 1) */
    /* vertex colour for background - flat shade using vertex 0 as colour source */
    srft_WriteVertex(&pfVertexDataBuffer, 0.0f, 0.0f, 1.0f, &psConfig->sBGColour);

    /* Vtx 2 is (0, height , 1) */
    /* this vertex colour is ignored */
    srft_WriteVertex(&pfVertexDataBuffer, 0.0f, (IMG_FLOAT)ui32RectangleSizeY, 1.0f, &psConfig->sBGColour);
    
    /* Vtx 3 is (width, 0, 1) */
    /* this vertex colour is ignored */
    srft_WriteVertex(&pfVertexDataBuffer, (IMG_FLOAT)ui32RectangleSizeX, 0.0f, 1.0f, &psConfig->sBGColour);


    /* Second triangle that covers the lower triangular half of the render screen */

    /* Vtx 1 is (0, height, 1) */
    /* vertex colour for background - flat shade using vertex 0 as colour source */
    srft_WriteVertex(&pfVertexDataBuffer, 0.0f, (IMG_FLOAT)ui32RectangleSizeY, 1.0f, &psConfig->sBGColour);
    

    /* Vtx 2 is (width , 0, 1) */
    /* this vertex colour is ignored */
    srft_WriteVertex(&pfVertexDataBuffer, (IMG_FLOAT)ui32RectangleSizeX, 0.0f, 1.0f, &psConfig->sBGColour);

    /* Vtx 3 is (width, height, 1) */
    /* vertex colour for background - flat shade using vertex 0 as colour source */
    srft_WriteVertex(&pfVertexDataBuffer, (IMG_FLOAT)ui32RectangleSizeX, (IMG_FLOAT)ui32RectangleSizeY, 1.0f, &psConfig->sBGColour);

    *ppui32VertexDataBuffer = (IMG_UINT32 *)pfVertexDataBuffer;
    
}



static IMG_VOID srft_WriteVerticesToVertexDataBuffer(IMG_UINT32		**ppui32VertexDataBuffer,
                                                     SRFT_CONFIG	*psConfig,
                                                     IMG_UINT32		ui32RectangleSizeX,
                                                     IMG_UINT32		ui32RectangleSizeY,
                                                     IMG_UINT32		ui32CyclePosition,
                                                     IMG_UINT32		ui32NumTrianglesInFrame,
                                                     IMG_UINT32		ui32TriangleMin,
                                                     IMG_UINT32		ui32TriangleMax)
{
    IMG_FLOAT			*pfVertexDataBuffer = (IMG_FLOAT *)*ppui32VertexDataBuffer;
    SRFT_COLOUR			sMainVertexColour;
    IMG_FLOAT			fMainAngle;
    IMG_UINT32			ui32TriangleCounter;

    PVR_ASSERT(ui32TriangleMax < ui32NumTrianglesInFrame);

    /*
        Calculate the main vertex colour and angle as a linear interpolation
        based on the cycle position.
    */
    srft_CyclePositionInterpolateColour(ui32CyclePosition, psConfig->ui32FramesPerRotation,
                                        &psConfig->sVertexColour1,
                                        &psConfig->sVertexColour2,
                                        &sMainVertexColour);

    fMainAngle = (IMG_FLOAT)M_PI * 2 * (IMG_FLOAT) ui32CyclePosition / (IMG_FLOAT) psConfig->ui32FramesPerRotation;

    if ((psConfig->ui32TriangleStyle == 2) ||
        (psConfig->ui32TriangleStyle == 3))
    {
        ui32NumTrianglesInFrame = MAX(ui32NumTrianglesInFrame, 3);
    }

    for (ui32TriangleCounter = ui32TriangleMax + 1;
         ui32TriangleCounter > ui32TriangleMin;
         ui32TriangleCounter--)
    {
        IMG_FLOAT	fWeighting = ((IMG_FLOAT)ui32TriangleCounter - 1) / (IMG_FLOAT)ui32NumTrianglesInFrame;
        SRFT_COLOUR	sVertexColour;

        /*
            Calculate each vertex colour as a linear interpolation of the
            main vertex colour and VC3 based on the triangle number.
        */
        srft_InterpolateColour(fWeighting, &psConfig->sVertexColour3, &sMainVertexColour, &sVertexColour);

        if (psConfig->ui32TriangleStyle == 1)
        {
            /* Equilateral triangle rotates around axis through its centre. */

            IMG_UINT32	ui32Radius = MIN(ui32RectangleSizeX, ui32RectangleSizeY) / 2;
            IMG_FLOAT	fAngle = fMainAngle - fWeighting * (IMG_FLOAT)M_PI * 2 / 3;

            /* Vtx 1 */
            srft_WriteVertex(&pfVertexDataBuffer,
                             ((IMG_FLOAT)cos(fAngle) * (IMG_FLOAT)ui32Radius) + ((IMG_FLOAT)ui32RectangleSizeX / 2),
                             ((IMG_FLOAT)sin(fAngle) * (IMG_FLOAT)ui32Radius) + ((IMG_FLOAT)ui32RectangleSizeY / 2),
                             1.0f, &sVertexColour);

            /* Vtx 2 */
            fAngle += (IMG_FLOAT)M_PI * 2 / 3;
            srft_WriteVertex(&pfVertexDataBuffer,
                             ((IMG_FLOAT)cos(fAngle) * (IMG_FLOAT)ui32Radius) + ((IMG_FLOAT)ui32RectangleSizeX / 2),
                             ((IMG_FLOAT)sin(fAngle) * (IMG_FLOAT)ui32Radius) + ((IMG_FLOAT)ui32RectangleSizeY / 2),
                             1.0f, &sVertexColour); /* this vertex colour is ignored */

            /* Vtx 3 */
            fAngle += (IMG_FLOAT)M_PI * 2 / 3;
            srft_WriteVertex(&pfVertexDataBuffer,
                             ((IMG_FLOAT)cos(fAngle) * (IMG_FLOAT)ui32Radius) + ((IMG_FLOAT)ui32RectangleSizeX / 2),
                             ((IMG_FLOAT)sin(fAngle) * (IMG_FLOAT)ui32Radius) + ((IMG_FLOAT)ui32RectangleSizeY / 2),
                             1.0f, &sVertexColour); /* this vertex colour is ignored */
        }
        else if (psConfig->ui32TriangleStyle == 2)
        {
            /* Identical isosceles triangles meet at the centre of a rotating polygon. */
            /* Appears like a pyramid from above, rotating around its axis. */

            IMG_UINT32	ui32Radius = MIN(ui32RectangleSizeX, ui32RectangleSizeY) / 2;
            IMG_FLOAT	fAngle = fMainAngle - fWeighting * (IMG_FLOAT)M_PI * 2;

            /* Vtx 1 - centre */
            srft_WriteVertex(&pfVertexDataBuffer,
                             ((IMG_FLOAT)ui32RectangleSizeX / 2),
                             ((IMG_FLOAT)ui32RectangleSizeY / 2),
                             1.0f, &sVertexColour);

            /* Vtx 2 */
            srft_WriteVertex(&pfVertexDataBuffer,
                             ((IMG_FLOAT)cos(fAngle) * (IMG_FLOAT)ui32Radius) + ((IMG_FLOAT)ui32RectangleSizeX / 2),
                             ((IMG_FLOAT)sin(fAngle) * (IMG_FLOAT)ui32Radius) + ((IMG_FLOAT)ui32RectangleSizeY / 2),
                             1.0f, &sVertexColour); /* this vertex colour is ignored */

            /* Vtx 3 */
            fAngle += (IMG_FLOAT)M_PI * 2 / ui32NumTrianglesInFrame;
            srft_WriteVertex(&pfVertexDataBuffer,
                             ((IMG_FLOAT)cos(fAngle) * (IMG_FLOAT)ui32Radius) + ((IMG_FLOAT)ui32RectangleSizeX / 2),
                             ((IMG_FLOAT)sin(fAngle) * (IMG_FLOAT)ui32Radius) + ((IMG_FLOAT)ui32RectangleSizeY / 2),
                             1.0f, &sVertexColour); /* this vertex colour is ignored */
        }
        else if (psConfig->ui32TriangleStyle == 3)
        {
            /* Same as 2, but "inside out". */
            /* Vertex 1 traces the circumference of a circle instead of being fixed at the centre. */

            IMG_UINT32	ui32Vertex1Radius = MIN(ui32RectangleSizeX, ui32RectangleSizeY) / 2;
            IMG_FLOAT	fVertex1Angle = fMainAngle - fWeighting * (IMG_FLOAT)M_PI * 2;
            IMG_FLOAT	fInternalAngle = ((IMG_FLOAT)M_PI * 2 / ui32NumTrianglesInFrame);
            IMG_UINT32	ui32Vertex23Radius = (IMG_UINT32)((IMG_FLOAT)ui32Vertex1Radius / 2 / cos(fInternalAngle / 2));
            IMG_FLOAT	fAngle;

            /* Vtx 1 */
            srft_WriteVertex(&pfVertexDataBuffer,
                             ((IMG_FLOAT)cos(fVertex1Angle) * (IMG_FLOAT)ui32Vertex1Radius) + ((IMG_FLOAT)ui32RectangleSizeX / 2),
                             ((IMG_FLOAT)sin(fVertex1Angle) * (IMG_FLOAT)ui32Vertex1Radius) + ((IMG_FLOAT)ui32RectangleSizeY / 2),
                             1.0f, &sVertexColour);

            /* Vtx 2 */
            fAngle = fVertex1Angle - (fInternalAngle / 2);
            srft_WriteVertex(&pfVertexDataBuffer,
                             ((IMG_FLOAT)cos(fAngle) * (IMG_FLOAT)ui32Vertex23Radius) + ((IMG_FLOAT)ui32RectangleSizeX / 2),
                             ((IMG_FLOAT)sin(fAngle) * (IMG_FLOAT)ui32Vertex23Radius) + ((IMG_FLOAT)ui32RectangleSizeY / 2),
                             1.0f, &sVertexColour); /* this vertex colour is ignored */

            /* Vtx 3 */
            fAngle = fVertex1Angle + (fInternalAngle / 2);
            srft_WriteVertex(&pfVertexDataBuffer,
                             ((IMG_FLOAT)cos(fAngle) * (IMG_FLOAT)ui32Vertex23Radius) + ((IMG_FLOAT)ui32RectangleSizeX / 2),
                             ((IMG_FLOAT)sin(fAngle) * (IMG_FLOAT)ui32Vertex23Radius) + ((IMG_FLOAT)ui32RectangleSizeY / 2),
                             1.0f, &sVertexColour); /* this vertex colour is ignored */
        }
        else if (psConfig->ui32TriangleStyle == 4)
        {
            /* Non-rotating equilateral triangles orbiting the centre. */

            IMG_UINT32	ui32OrbitRadius = MIN(ui32RectangleSizeX, ui32RectangleSizeY) / 4;
            IMG_FLOAT	fOrbitAngle = fMainAngle - fWeighting * (IMG_FLOAT)M_PI * 2;
            IMG_FLOAT	fTriangleCentreX = ((IMG_FLOAT)ui32RectangleSizeX / 2) + ((IMG_FLOAT)cos(fOrbitAngle) * (IMG_FLOAT)ui32OrbitRadius);
            IMG_FLOAT	fTriangleCentreY = ((IMG_FLOAT)ui32RectangleSizeY / 2) + ((IMG_FLOAT)sin(fOrbitAngle) * (IMG_FLOAT)ui32OrbitRadius);
            IMG_UINT32	ui32TriangleRadius = ui32OrbitRadius;
            IMG_FLOAT	fVertexAngle;

            /* Vtx 1 */
            fVertexAngle = 0;
            srft_WriteVertex(&pfVertexDataBuffer,
                             fTriangleCentreX + ((IMG_FLOAT)cos(fVertexAngle) * (IMG_FLOAT)ui32TriangleRadius),
                             fTriangleCentreY + ((IMG_FLOAT)sin(fVertexAngle) * (IMG_FLOAT)ui32TriangleRadius),
                             1.0f, &sVertexColour);

            /* Vtx 2 */
            fVertexAngle += (IMG_FLOAT)M_PI * 2 / 3;
            srft_WriteVertex(&pfVertexDataBuffer,
                             fTriangleCentreX + ((IMG_FLOAT)cos(fVertexAngle) * (IMG_FLOAT)ui32TriangleRadius),
                             fTriangleCentreY + ((IMG_FLOAT)sin(fVertexAngle) * (IMG_FLOAT)ui32TriangleRadius),
                             1.0f, &sVertexColour); /* this vertex colour is ignored */

            /* Vtx 3 */
            fVertexAngle += (IMG_FLOAT)M_PI * 2 / 3;
            srft_WriteVertex(&pfVertexDataBuffer,
                             fTriangleCentreX + ((IMG_FLOAT)cos(fVertexAngle) * (IMG_FLOAT)ui32TriangleRadius),
                             fTriangleCentreY + ((IMG_FLOAT)sin(fVertexAngle) * (IMG_FLOAT)ui32TriangleRadius),
                             1.0f, &sVertexColour); /* this vertex colour is ignored */
        }
        else if (psConfig->ui32TriangleStyle == 5)
        {
            /* Equilateral triangle rotates around axis through its centre. */

            IMG_UINT32	ui32Radius = MIN(ui32RectangleSizeX, ui32RectangleSizeY) / 2;
            IMG_FLOAT	fAngle = fMainAngle - fWeighting * (IMG_FLOAT)M_PI * 2 / 3;
            sVertexColour = SRFT_RED;
            /* Vtx 1 */
            srft_WriteVertex(&pfVertexDataBuffer,
                             ((IMG_FLOAT)cos(fAngle) * (IMG_FLOAT)ui32Radius) + ((IMG_FLOAT)ui32RectangleSizeX / 2),
                             ((IMG_FLOAT)sin(fAngle) * (IMG_FLOAT)ui32Radius) + ((IMG_FLOAT)ui32RectangleSizeY / 2),
                             1.0f, &sVertexColour);

            sVertexColour = SRFT_GREEN;
            /* Vtx 2 */
            fAngle += (IMG_FLOAT)M_PI * 2 / 3;
            srft_WriteVertex(&pfVertexDataBuffer,
                             ((IMG_FLOAT)cos(fAngle) * (IMG_FLOAT)ui32Radius) + ((IMG_FLOAT)ui32RectangleSizeX / 2),
                             ((IMG_FLOAT)sin(fAngle) * (IMG_FLOAT)ui32Radius) + ((IMG_FLOAT)ui32RectangleSizeY / 2),
                             1.0f, &sVertexColour);

            sVertexColour = SRFT_YELLOW;
            /* Vtx 3 */
            fAngle += (IMG_FLOAT)M_PI * 2 / 3;
            srft_WriteVertex(&pfVertexDataBuffer,
                             ((IMG_FLOAT)cos(fAngle) * (IMG_FLOAT)ui32Radius) + ((IMG_FLOAT)ui32RectangleSizeX / 2),
                             ((IMG_FLOAT)sin(fAngle) * (IMG_FLOAT)ui32Radius) + ((IMG_FLOAT)ui32RectangleSizeY / 2),
                             1.0f, &sVertexColour);
        }
        else
        {
            DPF("ERROR: Unknown triangle style %u\n", psConfig->ui32TriangleStyle);
            SGXUT_ERROR_EXIT();
        }
    }

    *ppui32VertexDataBuffer = (IMG_UINT32 *)pfVertexDataBuffer;
}


static IMG_VOID srft_WriteIndexBuffer(IMG_UINT16	*pui16IndexBuffer,
                                      IMG_UINT32	ui32NumTriangles)
{
    /* Write the indices */
    IMG_UINT16	ui16Vertex = 0;
    IMG_UINT32	j;

    for (j = ui32NumTriangles * 3; j > 0; j--)
    {
        *pui16IndexBuffer++ = ui16Vertex;
        ui16Vertex++;
    }
}


static IMG_VOID srft_WriteVertexShaderCode(IMG_UINT32					**ppui32USSECpuVAddr,
                                           IMG_DEV_VIRTADDR				*psUSSEDevVAddr,
                                           IMG_UINT32					**ppui32PDSCpuVAddr,
                                           IMG_DEV_VIRTADDR				*psPDSDevVAddr,
                                           PDS_VERTEX_SHADER_PROGRAM	*psVertexShaderPDSProgram,
                                           PVRSRV_SGX_CLIENT_INFO		*psSGXInfo,
                                           IMG_DEV_VIRTADDR				*psVerticesDevVAddr,
                                           IMG_DEV_VIRTADDR				uUSEVertexHeapBase,
                                           IMG_UINT32					*pui32NumVertexShaderPAsInBytes,
                                           IMG_UINT32					ui32SlowdownFactor,
                                           IMG_BOOL						bTestHWRVS,
                                           IMG_BOOL						bUSSEVertexWriteMem,
                                           PVRSRV_CLIENT_MEM_INFO		*psUSSEVertexMemFlagsTestMemInfo,
                                           IMG_BOOL						bDebugProgramAddrs)
{
    IMG_UINT32		*pui32VertexUSSEBuffer = *ppui32USSECpuVAddr;
    IMG_UINT32		*pui32VertexPDSBuffer = *ppui32PDSCpuVAddr;
    IMG_UINT32		ui32NumTempRegs;
#if defined(SGX_FEATURE_VCB)
    IMG_UINT32						*pui32VertexShaderPhasInstr;
    IMG_DEV_VIRTADDR				sVertexUSEProgNextPhase;
#endif /* SGX_FEATURE_VCB */

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES) && !defined(SGX_FEATURE_VCB)
    /* With VCB, the phas instruction will be inserted later, when the next phase address is known */
    srft_InsertPhas(&pui32VertexUSSEBuffer,
                    EURASIA_USE_OTHER2_PHAS_RATE_PIXEL,
                    EURASIA_USE_OTHER2_PHAS_WAITCOND_END);
#endif /* SGX_FEATURE_USE_UNLIMITED_PHASES */


    if (ui32SlowdownFactor > 0)
    {
        srft_InsertSlowDown(&pui32VertexUSSEBuffer, 500, ui32SlowdownFactor);
    }

    if (bUSSEVertexWriteMem)
    {
        srft_InsertMemWrite(&pui32VertexUSSEBuffer,
                            &psUSSEVertexMemFlagsTestMemInfo->sDevVAddr, 0);
    }

    /* Copy the 3 vertices to the output buffer. */
    /* mov.skipinv.repeat3 o0, pa0 */
    BuildMOV ((PPVR_USE_INST)pui32VertexUSSEBuffer,
                EURASIA_USE1_EPRED_ALWAYS,
                2 /* Repeat count */,
                EURASIA_USE1_RCNTSEL,
                USE_REGTYPE_OUTPUT, 0,
                USE_REGTYPE_PRIMATTR, 0,
                0 /* Src mod */);

    pui32VertexUSSEBuffer += USE_INST_LENGTH;

    /* mov.skipinv o3, c3  (c3 is the constant 1.0) */
    BuildMOV ((PPVR_USE_INST)pui32VertexUSSEBuffer,
                EURASIA_USE1_EPRED_ALWAYS,
                0 /* Repeat count */,
                EURASIA_USE1_RCNTSEL,
                USE_REGTYPE_OUTPUT, 3,
                USE_REGTYPE_SPECIAL, EURASIA_USE_SPECIAL_CONSTANT_FLOAT1_1,
                0 /* Src mod */);

    pui32VertexUSSEBuffer += USE_INST_LENGTH;

    /* Copy the 4 colour channels to the output buffer. */
    /* mov.skipinv.repeat4 o4, pa3 */
    BuildMOV ((PPVR_USE_INST)pui32VertexUSSEBuffer,
                EURASIA_USE1_EPRED_ALWAYS,
                3 /* Repeat count */,
                EURASIA_USE1_RCNTSEL,
                USE_REGTYPE_OUTPUT, 4,
                USE_REGTYPE_PRIMATTR, 3,
                0 /* Src mod */);

    pui32VertexUSSEBuffer += USE_INST_LENGTH;


#if defined(SGX_FEATURE_VCB)
    /*
        Insert a phas instruction to tell the hardware to launch the next phase.
    */
    pui32VertexShaderPhasInstr = pui32VertexUSSEBuffer; // Address of next phase patched here below
    srft_InsertPhas(&pui32VertexUSSEBuffer,
                    EURASIA_USE_OTHER2_PHAS_RATE_SAMPLE,
                    EURASIA_USE_OTHER2_PHAS_WAITCOND_VCULL);

    // emit.vcb.vtx #0 /* incp */, #0
    *pui32VertexUSSEBuffer++ = (EURASIA_USE0_S1EXTBANK_IMMEDIATE << EURASIA_USE0_S1BANK_SHIFT)	|
                              (EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT);

    *pui32VertexUSSEBuffer++ = ((EURASIA_USE1_OP_SPECIAL				<< EURASIA_USE1_OP_SHIFT)			|
                               (EURASIA_USE1_OTHER_OP2_EMIT			<< EURASIA_USE1_OTHER_OP2_SHIFT)	|
                               (EURASIA_USE1_SPECIAL_OPCAT_OTHER	<< EURASIA_USE1_SPECIAL_OPCAT_SHIFT)|
                               (EURASIA_USE1_S1BEXT)													|
                               (EURASIA_USE1_S2BEXT)													|
                               (EURASIA_USE1_S0STDBANK_TEMP			<< EURASIA_USE1_S0BANK_SHIFT)		|
                               (0									<< EURASIA_USE1_EMIT_INCP_SHIFT)	|
                                EURASIA_USE1_END														|
                               (3									<< EURASIA_USE1_EMIT_TARGET_SHIFT)	|
#if defined(SGX545)
                               (SGX545_USE1_EMIT_VERTEX_TWOPART)										|
#endif /* SGX545 */
                               (SGX545_USE1_EMIT_VCB_EMITTYPE_VERTEX << SGX545_USE1_EMIT_VCB_EMITTYPE_SHIFT));

    sVertexUSEProgNextPhase.uiAddr = psUSSEDevVAddr->uiAddr +
                                      ((pui32VertexUSSEBuffer - *ppui32USSECpuVAddr) * sizeof(IMG_UINT32));

    ALIGN_USSE_BUFFER_DEVV(sVertexUSEProgNextPhase.uiAddr)
    ALIGN_USSE_BUFFER_CPUV(pui32VertexUSSEBuffer)

    /* Patch the phas instruction with the address of the next phase. */
    *pui32VertexShaderPhasInstr |= (((sVertexUSEProgNextPhase.uiAddr - uUSEVertexHeapBase.uiAddr) / EURASIA_USE_INSTRUCTION_SIZE)
                                    <<	EURASIA_USE0_OTHER2_PHAS_IMM_EXEADDR_SHIFT) & ~EURASIA_USE0_OTHER2_PHAS_IMM_EXEADDR_CLRMSK;

    srft_InsertPhas(&pui32VertexUSSEBuffer,
                    EURASIA_USE_OTHER2_PHAS_RATE_PIXEL,
                    EURASIA_USE_OTHER2_PHAS_WAITCOND_END);

#endif /* SGX_FEATURE_VCB */

    // emit.mtevtx.end.freep #0 /* incp */, #0
    *pui32VertexUSSEBuffer++ = (EURASIA_USE0_S1EXTBANK_IMMEDIATE << EURASIA_USE0_S1BANK_SHIFT)	|
                              (EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT)	|
                               EURASIA_USE0_EMIT_FREEP;

    *pui32VertexUSSEBuffer++ = ((EURASIA_USE1_OP_SPECIAL				<< EURASIA_USE1_OP_SHIFT)			|
                               (EURASIA_USE1_OTHER_OP2_EMIT			<< EURASIA_USE1_OTHER_OP2_SHIFT)	|
                               (EURASIA_USE1_SPECIAL_OPCAT_OTHER	<< EURASIA_USE1_SPECIAL_OPCAT_SHIFT)|
                               (EURASIA_USE1_S1BEXT)													|
                               (EURASIA_USE1_S2BEXT)													|
                               (EURASIA_USE1_S0STDBANK_TEMP			<< EURASIA_USE1_S0BANK_SHIFT)		|
                               (EURASIA_USE1_EMIT_TARGET_MTE		<< EURASIA_USE1_EMIT_TARGET_SHIFT)	|
                               (EURASIA_USE1_EMIT_MTECTRL_VERTEX	<< EURASIA_USE1_EMIT_MTECTRL_SHIFT)	|
                               (0									<< EURASIA_USE1_EMIT_INCP_SHIFT)	|
#if defined(SGX545)
                               (SGX545_USE1_EMIT_VERTEX_TWOPART)										|
#endif /* SGX545 */
                                EURASIA_USE1_END);

    /**************************************************************************
    * Set up the PDS vertex shader program
    **************************************************************************/

    /* Setup the input program structure */
    psVertexShaderPDSProgram->pui32DataSegment							= IMG_NULL;
    psVertexShaderPDSProgram->ui32DataSize								= 0;
    psVertexShaderPDSProgram->b32BitIndices								= IMG_FALSE;
    psVertexShaderPDSProgram->ui32NumInstances							= 0;
    psVertexShaderPDSProgram->ui32NumStreams							= 1;

    psVertexShaderPDSProgram->asStreams[0].ui32NumElements				= 1;
    psVertexShaderPDSProgram->asStreams[0].bInstanceData				= IMG_FALSE;
    psVertexShaderPDSProgram->asStreams[0].ui32Multiplier				= 0;
    psVertexShaderPDSProgram->asStreams[0].ui32Address					= psVerticesDevVAddr->uiAddr;
    psVertexShaderPDSProgram->asStreams[0].ui32Shift					= 0;
    psVertexShaderPDSProgram->asStreams[0].ui32Stride					= 4 * SRFT_VERTEX_SIZE;

    psVertexShaderPDSProgram->asStreams[0].asElements[0].ui32Offset		= 0;
    psVertexShaderPDSProgram->asStreams[0].asElements[0].ui32Size		= 4 * SRFT_VERTEX_SIZE;
    psVertexShaderPDSProgram->asStreams[0].asElements[0].ui32Register	= 0;

    /* Set the USE Task control words */
    /* FIXME: no temporaries used in the vertex shader */
    ui32NumTempRegs = MAX(2, SGX_USE_MINTEMPREGS);

    ui32NumTempRegs += ((1 << EURASIA_PDS_DOUTU_TRC_ALIGNSHIFT) - 1);
    ui32NumTempRegs >>= EURASIA_PDS_DOUTU_TRC_ALIGNSHIFT;

    psVertexShaderPDSProgram->aui32USETaskControl[0] = 0;
    psVertexShaderPDSProgram->aui32USETaskControl[1] = EURASIA_PDS_DOUTU1_MODE_PARALLEL;
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
    psVertexShaderPDSProgram->aui32USETaskControl[0] |= (ui32NumTempRegs << EURASIA_PDS_DOUTU0_TRC_SHIFT);
#else
    psVertexShaderPDSProgram->aui32USETaskControl[1] |= (ui32NumTempRegs << EURASIA_PDS_DOUTU1_TRC_SHIFT);
#endif /* SGX545 */
    psVertexShaderPDSProgram->aui32USETaskControl[2] = 0;

    /* Set the execution address of the vertex copy program */
    if (bTestHWRVS)
    {
        /* Force a page fault to simulate a lockup - in order to test hardware recovery. */
        psUSSEDevVAddr->uiAddr = SRFT_PAGE_FAULT_ADDRESS;
    }
    SetUSEExecutionAddress(&psVertexShaderPDSProgram->aui32USETaskControl[0], 0, *psUSSEDevVAddr,
                            uUSEVertexHeapBase, SGX_VTXSHADER_USE_CODE_BASE_INDEX);

    /* Generate the PDS Program for this shader */
    pui32VertexPDSBuffer = PDSGenerateVertexShaderProgram(psVertexShaderPDSProgram,
                                                          pui32VertexPDSBuffer, IMG_NULL);

    if (bDebugProgramAddrs)
    {
        DPF("0x%x: Vertex shader PDS Program\n", psPDSDevVAddr->uiAddr);
        DPF("0x%x: Vertex shader USSE Program\n", psUSSEDevVAddr->uiAddr);
    }

#if !defined(SGX_FEATURE_EDM_VERTEX_PDSADDR_FULL_RANGE)
    /* Adjust PDS execution address for restricted address range */
    psPDSDevVAddr->uiAddr -= psSGXInfo->uPDSExecBase.uiAddr;
#else
    PVR_UNREFERENCED_PARAMETER(psSGXInfo);
#endif

    *pui32NumVertexShaderPAsInBytes = 4 * SRFT_VERTEX_SIZE;

#if defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
    *pui32NumVertexShaderPAsInBytes += ui32NumTempRegs;
#endif

    *ppui32USSECpuVAddr = pui32VertexUSSEBuffer;
    *ppui32PDSCpuVAddr = pui32VertexPDSBuffer;
}


static IMG_VOID srft_WriteIndexListToVDM(IMG_UINT32					**ppui32Buffer,
                                         IMG_UINT32					ui32NumTriangles,
                                         IMG_UINT32					ui32NumVertexShaderPAsInBytes,
                                         IMG_DEV_VIRTADDR			*psIndicesDevVAddr,
                                         IMG_DEV_VIRTADDR			*psVertexPDSProgDevVAddr,
                                         PDS_VERTEX_SHADER_PROGRAM	*psVertexShaderPDSProgram)
{
    IMG_UINT32	*pui32Buffer = *ppui32Buffer;

    *pui32Buffer++ = EURASIA_TAOBJTYPE_INDEXLIST	|
                     EURASIA_VDM_IDXPRES2			|
                     EURASIA_VDM_IDXPRES3			|
                    #if !defined(SGX545)
                     EURASIA_VDM_IDXPRES45			|
                    #endif /* SGX545 */
                     3 * ui32NumTriangles			| /* Number of indices */
                     EURASIA_VDM_TRIS;				  /* Triangle list */

    /* Index List 1 */
    *pui32Buffer++	= (psIndicesDevVAddr->uiAddr >> EURASIA_VDM_IDXBASE16_ALIGNSHIFT) <<
                      EURASIA_VDM_IDXBASE16_SHIFT;

    /* Index List 2 */
    *pui32Buffer++	= ((IMG_UINT32)((EURASIA_VDMPDS_MAXINPUTINSTANCES_MAX - 1) << EURASIA_VDMPDS_MAXINPUTINSTANCES_SHIFT)) |
                      EURASIA_VDM_IDXSIZE_16BIT;

    /* Index List 3 */
    *pui32Buffer++ = ~EURASIA_VDM_WRAPCOUNT_CLRMSK;

    /* Index List 4 */
    *pui32Buffer++ = (7 << EURASIA_VDMPDS_PARTSIZE_SHIFT) |
                     (psVertexPDSProgDevVAddr->uiAddr >> EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) <<
                     EURASIA_VDMPDS_BASEADDR_SHIFT;

    /* Index List 5 */
    *pui32Buffer++ = (2 << EURASIA_VDMPDS_VERTEXSIZE_SHIFT)												|
                     (3 << EURASIA_VDMPDS_VTXADVANCE_SHIFT)												|
                     (SGXUT_ALIGNFIELD(ui32NumVertexShaderPAsInBytes, EURASIA_VDMPDS_USEATTRIBUTESIZE_ALIGNSHIFT) <<
                         EURASIA_VDMPDS_USEATTRIBUTESIZE_SHIFT)												|
                     (psVertexShaderPDSProgram->ui32DataSize >> EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT)
                     << EURASIA_VDMPDS_DATASIZE_SHIFT;

    *ppui32Buffer = pui32Buffer;
}


static IMG_VOID srft_WriteMTETerminateCode(IMG_UINT32					**ppui32USSECpuVAddr,
                                           IMG_DEV_VIRTADDR				*psUSSEDevVAddr,
                                           IMG_UINT32					**ppui32PDSCpuVAddr,
                                           IMG_DEV_VIRTADDR				*psPDSDevVAddr,
                                           PDS_TERMINATE_STATE_PROGRAM	*psTerminatePDSProgram,
                                           IMG_DEV_VIRTADDR				uUSEVertexHeapBase,
                                           PVRSRV_SGX_CLIENT_INFO		*psSGXInfo,
                                           IMG_UINT32					ui32MainRenderWidth,
                                           IMG_UINT32					ui32MainRenderHeight,
                                           IMG_BOOL						bDebugProgramAddrs)
{
    IMG_UINT32		*pui32VertexUSSEBuffer = *ppui32USSECpuVAddr;
    IMG_UINT32		*pui32VertexPDSBuffer = *ppui32PDSCpuVAddr;
    IMG_UINT32		ui32Right;
    IMG_UINT32		ui32Bottom;


    /*
        Move the state data from the primary attributes into the output registers.
    */
    pui32VertexUSSEBuffer = USEGenWriteStateEmitProgram (pui32VertexUSSEBuffer, 2, 0, IMG_FALSE);

    /**************************************************************************
    * Set up and write the terminate PDS program
    **************************************************************************/

    /* Snap Terminate clip rect to nearest 16 pixel units outside drawable rectangle */
    ui32Right = (ui32MainRenderWidth   +  EURASIA_TARNDCLIP_LR_ALIGNSIZE - 1) &
                                            ~(EURASIA_TARNDCLIP_LR_ALIGNSIZE - 1);
    ui32Bottom = (ui32MainRenderHeight +  EURASIA_TARNDCLIP_TB_ALIGNSIZE - 1) &
                                            ~(EURASIA_TARNDCLIP_TB_ALIGNSIZE - 1);


    /* Work out positions in 16 pixel units	*/
    ui32Right  = (ui32Right  >> EURASIA_TARNDCLIP_LR_ALIGNSHIFT) - 1;
    ui32Bottom = (ui32Bottom >> EURASIA_TARNDCLIP_TB_ALIGNSHIFT) - 1;

    /* Set up terminate region word */
    psTerminatePDSProgram->ui32TerminateRegion =	(0			<< EURASIA_TARNDCLIP_LEFT_SHIFT)	|
                                                    (ui32Right	<< EURASIA_TARNDCLIP_RIGHT_SHIFT)	|
                                                    (0			<< EURASIA_TARNDCLIP_TOP_SHIFT)		|
                                                    (ui32Bottom << EURASIA_TARNDCLIP_BOTTOM_SHIFT);


    /* Setup the parameters for the PDS program */
    psTerminatePDSProgram->aui32USETaskControl[0]	= 0;
    psTerminatePDSProgram->aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL;
    psTerminatePDSProgram->aui32USETaskControl[2]	= 0;

    SetUSEExecutionAddress(&psTerminatePDSProgram->aui32USETaskControl[0], 0, *psUSSEDevVAddr,
                            uUSEVertexHeapBase, SGX_VTXSHADER_USE_CODE_BASE_INDEX);

    pui32VertexPDSBuffer = PDSGenerateTerminateStateProgram(psTerminatePDSProgram,
                                                            pui32VertexPDSBuffer);

    if (bDebugProgramAddrs)
    {
        DPF("0x%x: Terminate PDS Program\n", psPDSDevVAddr->uiAddr);
        DPF("0x%x: Terminate USSE Program\n\n", psUSSEDevVAddr->uiAddr);
    }

    #if !defined(SGX_FEATURE_EDM_VERTEX_PDSADDR_FULL_RANGE)
    /* Adjust PDS execution address for restricted address range */
    psPDSDevVAddr->uiAddr -= psSGXInfo->uPDSExecBase.uiAddr;
    #else
    PVR_UNREFERENCED_PARAMETER(psSGXInfo);
    #endif

    *ppui32USSECpuVAddr = pui32VertexUSSEBuffer;
    *ppui32PDSCpuVAddr = pui32VertexPDSBuffer;
}


static IMG_VOID srft_WriteMTETerminateToVDM(IMG_UINT32						**ppui32Buffer,
                                            IMG_DEV_VIRTADDR				*psTerminatePDSProgDevVAddr,
                                            PDS_TERMINATE_STATE_PROGRAM	*psTerminatePDSProgram)
{
    IMG_UINT32	*pui32Buffer = *ppui32Buffer;
    IMG_UINT32	ui32NumPartitions = 1;

    #if defined(SGX545)
    ui32NumPartitions = 2;
    #endif /* SGX545 */

    /* Write the PDS state block */
    *pui32Buffer++	= (EURASIA_TAOBJTYPE_STATE                      |
                       EURASIA_TAPDSSTATE_LASTTASK					|
                        (((psTerminatePDSProgDevVAddr->uiAddr >> EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT)
                            << EURASIA_TAPDSSTATE_BASEADDR_SHIFT) & ~EURASIA_TAPDSSTATE_BASEADDR_CLRMSK));

    *pui32Buffer++	= ((((psTerminatePDSProgram->ui32DataSize >> EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT)
                            << EURASIA_TAPDSSTATE_DATASIZE_SHIFT) & ~EURASIA_TAPDSSTATE_DATASIZE_CLRMSK)	|
                        EURASIA_TAPDSSTATE_USEPIPE_1														|
                        (ui32NumPartitions << EURASIA_TAPDSSTATE_PARTITIONS_SHIFT)										|
                        EURASIA_TAPDSSTATE_SD																|
                        EURASIA_TAPDSSTATE_MTE_EMIT														|
                        (SGXUT_ALIGNFIELD( 2 /*ui32USEAttribSize*/, (EURASIA_TAPDSSTATE_USEATTRIBUTESIZE_ALIGNSHIFT - 2))
                            << EURASIA_TAPDSSTATE_USEATTRIBUTESIZE_SHIFT));

    *pui32Buffer++ = EURASIA_TAOBJTYPE_TERMINATE;

    *ppui32Buffer = pui32Buffer;
}


static IMG_VOID srft_SetupTAKick(SGX_KICKTA				*psKickTA,
                                 IMG_UINT32				*aui32HWBGObjPDSState,
                                 IMG_HANDLE				hRenderContext,
                                 PVR_HWREGS				*psHWRegs,
                                 IMG_BOOL				bFirstKick,
                                 IMG_BOOL				bKickRender,
                                 IMG_BOOL				b2XMSAADontResolve,
                                 IMG_UINT32				ui32FrameNum,
                                 PVRSRV_CLIENT_MEM_INFO	*psRenderSurfMemInfo,
                                 IMG_HANDLE				hRTDataSet,
#if defined(__psp2__)
                                 IMG_UINT32					ui32MainRenderWidth,
                                 IMG_UINT32					ui32MainRenderHeight,
#endif
                                 IMG_DEV_VIRTADDR		sTABufferCtlStreamBase,
                                 IMG_BOOL				bTestHWRVDM,
                                 IMG_PVOID 				pvKickTAPDump)
{
#if defined(PDUMP)
    PSGX_KICKTA_PDUMP psKickTAPDump = pvKickTAPDump;
#else
    PVR_UNREFERENCED_PARAMETER(pvKickTAPDump);
#endif

#if ! defined(FIX_HW_BRN_31982)
    PVR_UNREFERENCED_PARAMETER(b2XMSAADontResolve);
#endif


    /* Modify the first three dwords for PDS state for pixel shader */
    psKickTA->sKickTACommon.aui32SpecObject[0]		= aui32HWBGObjPDSState[0];
    psKickTA->sKickTACommon.aui32SpecObject[1]		= aui32HWBGObjPDSState[1];
    psKickTA->sKickTACommon.aui32SpecObject[2]		= aui32HWBGObjPDSState[2];

    psKickTA->hRenderContext	= hRenderContext;

    psKickTA->sPixEventUpdateDevAddr.uiAddr = 0;
    psKickTA->psPixEventUpdateList   = IMG_NULL;
    psKickTA->psHWBgObjPrimPDSUpdateList  = psKickTA->sKickTACommon.aui32SpecObject;

    if (psRenderSurfMemInfo != IMG_NULL)
    {
        psKickTA->ppsRenderSurfSyncInfo			= &psRenderSurfMemInfo->psClientSyncInfo;
    }
    else
    {
        psKickTA->ppsRenderSurfSyncInfo			= IMG_NULL;
    }

    psKickTA->hRTDataSet = hRTDataSet;
    psKickTA->sKickTACommon.ui32ISPBGObjDepth	= psHWRegs->ui32ISPBGObjDepth;
    psKickTA->sKickTACommon.ui32ISPBGObj		= psHWRegs->ui32ISPBGObj;
    psKickTA->sKickTACommon.ui32ISPIPFMisc		= psHWRegs->ui32ISPIPFMisc;

    psKickTA->sKickTACommon.sISPZLoadBase.uiAddr	= psHWRegs->ui32ISPZLoadBase;
    psKickTA->sKickTACommon.sISPZStoreBase.uiAddr		= psHWRegs->ui32ISPZStoreBase;
    psKickTA->sKickTACommon.sISPStencilLoadBase.uiAddr	= psHWRegs->ui32ISPStencilLoadBase;
    psKickTA->sKickTACommon.sISPStencilStoreBase.uiAddr	= psHWRegs->ui32ISPStencilStoreBase;
    
    #if !defined(SGX545)
    psKickTA->sKickTACommon.ui32ISPPerpendicular	= psHWRegs->ui32ISPPerpendicular;
    psKickTA->sKickTACommon.ui32ISPCullValue		= psHWRegs->ui32ISPCullValue;
    #endif

    psKickTA->sKickTACommon.ui32ZLSCtl				= psHWRegs->ui32ZLSCtl;
    psKickTA->sKickTACommon.ui32ISPValidId			= psHWRegs->ui32ISPValidId;
    psKickTA->sKickTACommon.ui32ISPDBias			= psHWRegs->ui32ISPDBias;
    psKickTA->sKickTACommon.ui32ISPSceneBGObj		= psHWRegs->ui32ISPSceneBGObj;
    psKickTA->sKickTACommon.ui32EDMPixelPDSAddr		= psHWRegs->ui32EDMPixelPDSAddr;
    psKickTA->sKickTACommon.ui32EDMPixelPDSDataSize	= psHWRegs->ui32EDMPixelPDSDataSize;
    psKickTA->sKickTACommon.ui32EDMPixelPDSInfo		= psHWRegs->ui32EDMPixelPDSInfo;
    psKickTA->sKickTACommon.ui32PixelBE				= psHWRegs->ui32PixelBE;
    psKickTA->sKickTACommon.ui32USELoadRedirect		= 0;
    psKickTA->sKickTACommon.ui32USEStoreRange		= 0;

    /* TARegs */
    if (bTestHWRVDM)
    {
        /* Force a page fault to simulate a lockup - in order to test hardware recovery. */
        sTABufferCtlStreamBase.uiAddr = SRFT_PAGE_FAULT_ADDRESS;
    }
    psKickTA->sKickTACommon.sTABufferCtlStreamBase	= sTABufferCtlStreamBase;
    psKickTA->sKickTACommon.ui32MTECtrl				= psHWRegs->ui32MTECtrl;

    /* No sync updates for the purpose of this test */
    psKickTA->sKickTACommon.ui32NumTAStatusVals		= 0;
    psKickTA->sKickTACommon.ui32Num3DStatusVals		= 0;
    psKickTA->sKickTACommon.ui32NumSrcSyncs			= 0;

    psKickTA->sKickTACommon.ui32KickFlags = SGX_KICKTA_FLAGS_KICKRENDER				|
                                            SGX_KICKTA_FLAGS_RESETTPC					|
                                            SGX_KICKTA_FLAGS_COLOURALPHAVALID			|
                                            /* SGX_KICKTA_FLAGS_RENDEREDMIDSCENE		| */
                                            /* SGX_KICKTA_FLAGS_GETVISRESULTS			| */
                                            /* SGX_KICKTA_FLAGS_TAUSEDAFTERRENDER		| */
                                            /* SGX_KICKTA_FLAGS_RENDEREDMIDSCENENOFREE	| */
                                            /* SGX_KICKTA_FLAGS_VISTESTTERM			| */
                                            /* SGX_KICKTA_FLAGS_MIDSCENENOFREE			| */
                                            /* SGX_KICKTA_FLAGS_ZONLYRENDER			| */
                                            /* SGX_KICKTA_FLAGS_MIDSCENETERM			| */
                                             SGX_KICKTA_FLAGS_FLUSHDATACACHE			|
                                             SGX_KICKTA_FLAGS_FLUSHPDSCACHE			|
                                             SGX_KICKTA_FLAGS_FLUSHUSECACHE;

#if defined(FIX_HW_BRN_31982)
    if (b2XMSAADontResolve)
    {
        psKickTA->sKickTACommon.ui32KickFlags |= SGX_KICKTA_FLAGS_POST_RENDER_SLC_FLUSH;
    }
#endif

    if (bFirstKick)
    {
        psKickTA->sKickTACommon.ui32KickFlags |= SGX_KICKTA_FLAGS_FIRSTTAPROD;
    }
    if (bKickRender)
    {
        psKickTA->sKickTACommon.ui32KickFlags |= SGX_KICKTA_FLAGS_TERMINATE;
    }

    psKickTA->sKickTACommon.ui32Frame = ui32FrameNum;

#if defined(__psp2__)
    psKickTA->sKickTACommon.ui32SceneWidth = ui32MainRenderWidth;
    psKickTA->sKickTACommon.ui32SceneHeight = ui32MainRenderHeight;
    psKickTA->sKickTACommon.ui32ValidRegionXMax = ui32MainRenderWidth - 1;
    psKickTA->sKickTACommon.ui32ValidRegionYMax = ui32MainRenderHeight - 1;
    psKickTA->sKickTACommon.ui16PrimitiveSplitThreshold = 1000;
#endif
    
    /* KickTA pdump */
#if defined(PDUMP)
    PVRSRVMemSet(psKickTAPDump, 0, sizeof(SGX_KICKTA_PDUMP));
    /* TODO: assign non-zero values here? */
    psKickTAPDump->ui32BufferArraySize = 0;
    psKickTAPDump->ui32PDumpBitmapSize = 0;
#endif
}


static IMG_VOID srft_PDumpBuffer(PVRSRV_CONNECTION			*psConnection,
                                 IMG_CHAR					*pszComment,
                                 PVRSRV_CLIENT_MEM_INFO		*psClientMemInfo,
                                 IMG_UINT32					ui32Offset,
                                 IMG_UINT32					ui32Size,
                                 IMG_BOOL					bContinuous)
{
    PVRSRV_ERROR	eResult;
    IMG_UINT32		ui32Flags = 0;
    
    if (bContinuous)
    {
        ui32Flags |= PVRSRV_PDUMP_FLAGS_CONTINUOUS;
    }

    PVRSRVPDumpComment(psConnection, pszComment, IMG_FALSE);
    eResult = PVRSRVPDumpMem(psConnection, IMG_NULL,
                             psClientMemInfo, ui32Offset, ui32Size, ui32Flags);

    if (eResult == PVRSRV_ERROR_PDUMP_BUFFER_FULL)
    {
        /* FIXME: services returns this error if pdump is not running. */
        eResult = PVRSRV_OK;
    }
    else if (eResult == PVRSRV_ERROR_PDUMP_NOT_ACTIVE)
    {
        /* FIXME: services occasionally returns this error - needs triage */
        eResult = PVRSRV_OK;
    }
    sgxut_fail_if_error_quiet(eResult);
}


static IMG_VOID srft_PDump3DBuffers(PVRSRV_CONNECTION		*psConnection,
                                    IMG_UINT32				ui32RenderIndex,
                                    IMG_UINT32				ui32RenderCount,
                                    IMG_UINT32				ui32FrameNum,
                                    PVRSRV_CLIENT_MEM_INFO	**apsFragmentUSSEMemInfo,
                                    IMG_UINT32				*pui32FragmentUSSEBuffer,
                                    IMG_UINT32				*pui32FragmentUSSEBufferStart,
                                    PVRSRV_CLIENT_MEM_INFO	**apsPixelPDSMemInfo,
                                    IMG_UINT32				*pui32PixelPDSBuffer,
                                    IMG_UINT32				*pui32PixelPDSBufferStart)
{
    IMG_CHAR		aui8CommentBuffer[128];

    sprintf(aui8CommentBuffer, "Setting pdump frame:%u", ui32RenderCount);
    PVRSRVPDumpComment(psConnection, aui8CommentBuffer, IMG_TRUE);
    PVRSRVPDumpSetFrame(psConnection, ui32FrameNum);

    srft_PDumpBuffer(psConnection, "Dumping USSE Pixel Buffer",
                     apsFragmentUSSEMemInfo[ui32RenderIndex], 0,
                     (IMG_UINT32)(pui32FragmentUSSEBuffer - pui32FragmentUSSEBufferStart) * sizeof(IMG_UINT32),
                     IMG_FALSE);

    srft_PDumpBuffer(psConnection, "Dumping PDS Pixel Buffer",
                     apsPixelPDSMemInfo[ui32RenderIndex], 0,
                     (IMG_UINT32)(pui32PixelPDSBuffer - pui32PixelPDSBufferStart) * sizeof(IMG_UINT32),
                     IMG_FALSE);
}


static IMG_VOID srft_PDumpTABuffers(PVRSRV_CONNECTION		*psConnection,
                                    PVRSRV_CLIENT_MEM_INFO	*psVertexUSSEMemInfo,
                                    IMG_UINT32				*pui32VertexUSSEBuffer,
                                    IMG_UINT32				*pui32VertexUSSEBufferKickStart,
                                    IMG_UINT32				*pui32VertexUSSEBufferStart,
                                    PVRSRV_CLIENT_MEM_INFO	*psVDMControlStreamMemInfo,
                                    IMG_UINT32				*pui32VDMControlStreamBuffer,
                                    IMG_UINT32				*pui32VDMControlStreamBufferKickStart,
                                    IMG_UINT32				*pui32VDMControlStreamBufferStart,
                                    PVRSRV_CLIENT_MEM_INFO	*psVertexDataMemInfo,
                                    IMG_UINT32				*pui32VertexDataBuffer,
                                    IMG_UINT32				*pui32VertexDataBufferKickStart,
                                    IMG_UINT32				*pui32VertexDataBufferStart,
                                    PVRSRV_CLIENT_MEM_INFO	*psVertexPDSMemInfo,
                                    IMG_UINT32				*pui32VertexPDSBuffer,
                                    IMG_UINT32				*pui32VertexPDSBufferKickStart,
                                    IMG_UINT32				*pui32VertexPDSBufferStart)
{
    srft_PDumpBuffer(psConnection, "Dumping USSE Vertex Buffer",
                     psVertexUSSEMemInfo,
                     (IMG_UINT32)(pui32VertexUSSEBufferKickStart - pui32VertexUSSEBufferStart) * sizeof(IMG_UINT32),
                     (IMG_UINT32)(pui32VertexUSSEBuffer - pui32VertexUSSEBufferKickStart) * sizeof(IMG_UINT32),
                     IMG_FALSE);

    srft_PDumpBuffer(psConnection, "Dumping VDM Control Stream Buffer",
                     psVDMControlStreamMemInfo,
                     (IMG_UINT32)(pui32VDMControlStreamBufferKickStart - pui32VDMControlStreamBufferStart) * sizeof(IMG_UINT32),
                     (IMG_UINT32)(pui32VDMControlStreamBuffer - pui32VDMControlStreamBufferKickStart) * sizeof(IMG_UINT32),
                     IMG_FALSE);

    srft_PDumpBuffer(psConnection, "Dumping Vertex Data Buffer",
                     psVertexDataMemInfo,
                     (IMG_UINT32)(pui32VertexDataBufferKickStart - pui32VertexDataBufferStart) * sizeof(IMG_UINT32),
                     (IMG_UINT32)(pui32VertexDataBuffer - pui32VertexDataBufferKickStart) * sizeof(IMG_UINT32),
                     IMG_FALSE);

    srft_PDumpBuffer(psConnection, "Dumping PDS Vertex Buffer",
                     psVertexPDSMemInfo,
                     (IMG_UINT32)(pui32VertexPDSBufferKickStart - pui32VertexPDSBufferStart) * sizeof(IMG_UINT32),
                     (IMG_UINT32)(pui32VertexPDSBuffer - pui32VertexPDSBufferKickStart) * sizeof(IMG_UINT32),
                     IMG_FALSE);
}


#if defined(PDUMP)
static IMG_VOID srft_DumpBitmap(PVRSRV_DEV_DATA		*psDevData,
                                IMG_UINT32			ui32RenderAddress,
#if defined (SUPPORT_SID_INTERFACE)
                                IMG_SID				hDevMemContext,
#else
                                IMG_HANDLE			hDevMemContext,
#endif
                                IMG_UINT32			ui32Width,
                                IMG_UINT32			ui32Height,
                                IMG_UINT32			ui32ByteStride,
                                PDUMP_PIXEL_FORMAT	eDumpFormat,
                                IMG_CHAR			*szFilePrefix,
                                IMG_CHAR			*szRenderType,
                                IMG_UINT32			ui32RenderCount)
{
    IMG_DEV_VIRTADDR	sRenderVAddr;
    IMG_UINT32			ui32Length;
    IMG_CHAR			aui8FileName[PVRSRV_PDUMP_MAX_FILENAME_SIZE + 10];

    ui32Length = strlen(szFilePrefix);
    if (ui32Length > PVRSRV_PDUMP_MAX_FILENAME_SIZE - 1)
    {
        DPF("Bitmap filename prefix too long\n");
        SGXUT_ERROR_EXIT();
    }
    
    sprintf(aui8FileName, "%s", szFilePrefix);
    
    if (strcmp(szFilePrefix, "outfb") != 0)
    {
        strcat(aui8FileName + strlen(aui8FileName), "_");
        strcat(aui8FileName + strlen(aui8FileName), szRenderType);
    }

    sprintf(aui8FileName + strlen(aui8FileName), "%u", ui32RenderCount);

    if (strlen(aui8FileName) > PVRSRV_PDUMP_MAX_FILENAME_SIZE - 1)
    {
        DPF("Bitmap filename too long\n");
        SGXUT_ERROR_EXIT();
    }

    sRenderVAddr.uiAddr = ui32RenderAddress;
    PVRSRVPDumpBitmap(	psDevData,
                        aui8FileName,
                        0,
                        ui32Width,
                        ui32Height,
                        ui32ByteStride,
                        sRenderVAddr,
                        hDevMemContext,
                        ui32Height * ui32ByteStride,
                        eDumpFormat,
                        PVRSRV_PDUMP_MEM_FORMAT_STRIDE,
                        0);
}
#endif /* PDUMP */


static IMG_VOID srft_DrawTriangles(SRFT_SHARED				*psShared,
                                   SRFT_CONFIG				*psConfig,
                                   PVRSRV_CLIENT_MEM_INFO	*psVertexDataMemInfo,
                                   IMG_UINT32				**ppui32VertexDataBuffer,
                                   IMG_UINT32				*pui32VertexDataBufferStart,
                                   PVRSRV_CLIENT_MEM_INFO	*psVertexPDSMemInfo,
                                   IMG_UINT32				**ppui32VertexPDSBuffer,
                                   IMG_UINT32				*pui32VertexPDSBufferStart,
                                   PVRSRV_CLIENT_MEM_INFO	*psVertexUSSEMemInfo,
                                   IMG_UINT32				**ppui32VertexUSSEBuffer,
                                   IMG_UINT32				*pui32VertexUSSEBufferStart,
                                   PVRSRV_CLIENT_MEM_INFO	*psVDMControlStreamMemInfo,
                                   IMG_UINT32				**ppui32VDMControlStreamBuffer,
                                   IMG_UINT32				*pui32VDMControlStreamBufferStart,
                                   IMG_UINT32				*pui32HWBGObjPDSState,
                                   IMG_UINT32				*pui32MainPDSState,
                                   PVR_HWREGS				*psHWRegs,
                                   PVRSRV_CLIENT_MEM_INFO	*psUSSEVertexMemFlagsTestMemInfo,
                                   PVRSRV_CLIENT_MEM_INFO	*psIndexMemInfo,
                                   PVRSRV_CLIENT_MEM_INFO	*psRenderSurfMemInfo,
                                   IMG_HANDLE				hRTDataSet,
                                   IMG_UINT32				ui32FrameNum,
                                   IMG_UINT32				ui32CyclePosition,
                                   IMG_UINT32				ui32NumTrianglesInFrame,
                                   IMG_UINT32				ui32RenderCount)
{
    PVRSRV_ERROR				eResult;
    IMG_UINT32					*pui32VertexDataBuffer = *ppui32VertexDataBuffer;
    IMG_UINT32					*pui32VertexPDSBuffer = *ppui32VertexPDSBuffer;
    IMG_UINT32					*pui32VertexUSSEBuffer = *ppui32VertexUSSEBuffer;
    IMG_UINT32					*pui32VDMControlStreamBuffer = *ppui32VDMControlStreamBuffer;
    PDS_STATE_COPY_PROGRAM		sMTEStateCopyPDSProgram  = {0};
    PDS_VERTEX_SHADER_PROGRAM	sVertexShaderPDSProgram = {0};
    PDS_TERMINATE_STATE_PROGRAM	sTerminatePDSProgram = {0};
    IMG_DEV_VIRTADDR			sUSSEProgDevVAddr;
    IMG_DEV_VIRTADDR			sMTEStateBlockDevVAddr;
    IMG_UINT32					ui32MTEStateUpdateDWORDCount;
    IMG_DEV_VIRTADDR			sMTEStateCopyPDSProgDevVAddr;
    IMG_DEV_VIRTADDR			sVDMControlStreamDevVAddr;
    IMG_DEV_VIRTADDR			sVerticesDevVAddr;
    IMG_DEV_VIRTADDR			sVertexPDSProgDevVAddr;
    IMG_UINT32					ui32NumVertexShaderPAsInBytes;
    IMG_DEV_VIRTADDR			sTerminatePDSProgDevVAddr;
    SGX_KICKTA					sKickTA;
    SGX_KICKTA_OUTPUT			sKickTAOutput;
#if defined(PDUMP)
    SGX_KICKTA_PDUMP 			sKickTAPDump;
    IMG_PVOID					pvKickTAPDump = &sKickTAPDump;
#else
    IMG_PVOID					pvKickTAPDump = IMG_NULL;
#endif
    IMG_BOOL					bNoMainRenderSyncObject = (psConfig->bNoSyncObject && !psConfig->bBlit) ? IMG_TRUE : IMG_FALSE;
    IMG_UINT32					ui32LoopCounter, ui32LoopMin, ui32LoopMax;

    memset(&sKickTA, 0, sizeof(sKickTA));

    ui32LoopMin = 0;
    ui32LoopMax = psConfig->bMultipleTAKicks ? (ui32NumTrianglesInFrame - 1) : 0;

    for (ui32LoopCounter = ui32LoopMin;
         ui32LoopCounter <= ui32LoopMax;
         ui32LoopCounter++)
    {
        IMG_UINT32	ui32FirstTriangle, ui32LastTriangle, ui32NumTrianglesInKick;
        IMG_UINT32	*pui32VertexDataBufferKickStart = pui32VertexDataBuffer;
        IMG_UINT32	*pui32VertexUSSEBufferKickStart = pui32VertexUSSEBuffer;
        IMG_UINT32	*pui32VDMControlStreamBufferKickStart = pui32VDMControlStreamBuffer;
        IMG_UINT32	*pui32VertexPDSBufferKickStart = pui32VertexPDSBuffer;

        if ((psConfig->ui32KickSleepTimeMS > 0))
        {
            sgxut_sleep_ms(psConfig->ui32KickSleepTimeMS);
        }

        if (psConfig->bMultipleTAKicks)
        {
            ui32FirstTriangle = ui32NumTrianglesInFrame - ui32LoopCounter - 1;
            ui32LastTriangle = ui32FirstTriangle;
            psShared->fnInfo("Triangle number %u\n", ui32FirstTriangle);
        }
        else
        {
            ui32FirstTriangle = 0;
            ui32LastTriangle = ui32NumTrianglesInFrame - 1;
        }
        ui32NumTrianglesInKick = ui32LastTriangle - ui32FirstTriangle + 1;

        /**************************************************************************
            Set up Macro Tiling Engine (MTE) State
        **************************************************************************/
        /*
            We must now set up MTE state with the PDS fragment
            shader and the ISP state etc that we want to embed in the display
            lists for our triangle object.

            This state is written to vertex buffer memory, and then copied to the
            MTE state using an MTE emit in the USE. The Emit USE program is written
            in Vertex USE memory and the corresponding PDS program is written in Vertex
            PDS memory.
        */
        sMTEStateBlockDevVAddr.uiAddr = psVertexDataMemInfo->sDevVAddr.uiAddr +
                                         ((IMG_UINT32)(pui32VertexDataBuffer - pui32VertexDataBufferStart) * sizeof(IMG_UINT32));

        srft_WriteMTEStateToVertexDataBuffer(&pui32VertexDataBuffer,
                                             pui32MainPDSState);

        ui32MTEStateUpdateDWORDCount = (IMG_UINT32)(pui32VertexDataBuffer - pui32VertexDataBufferKickStart);
        
        /**************************************************************************
            Generate the MTE state update programs
        **************************************************************************/
        sUSSEProgDevVAddr.uiAddr = psVertexUSSEMemInfo->sDevVAddr.uiAddr +
                                    ((IMG_UINT32)(pui32VertexUSSEBuffer - pui32VertexUSSEBufferStart) * sizeof(IMG_UINT32));

        ALIGN_USSE_BUFFER_DEVV(sUSSEProgDevVAddr.uiAddr)
        ALIGN_USSE_BUFFER_CPUV(pui32VertexUSSEBuffer)

        sMTEStateCopyPDSProgDevVAddr.uiAddr = psVertexPDSMemInfo->sDevVAddr.uiAddr +
                                              ((IMG_UINT32)(pui32VertexPDSBuffer - pui32VertexPDSBufferStart) * sizeof(IMG_UINT32));

        ALIGN_PDS_BUFFER_DEVV(sMTEStateCopyPDSProgDevVAddr.uiAddr)
        ALIGN_PDS_BUFFER_CPUV(pui32VertexPDSBuffer)

        srft_WriteMTEStateCode(&sMTEStateBlockDevVAddr,
                               &pui32VertexUSSEBuffer, &sUSSEProgDevVAddr,
                               &pui32VertexPDSBuffer, &sMTEStateCopyPDSProgDevVAddr,
                               &sMTEStateCopyPDSProgram,
                               psShared->psSGXInfo,
                               psShared->uUSEVertexHeapBase,
                               ui32MTEStateUpdateDWORDCount,
                               psConfig->bDebugProgramAddrs);

        /**************************************************************************
            Write the MTE state update to the VDM control stream
        **************************************************************************/
        sVDMControlStreamDevVAddr.uiAddr = psVDMControlStreamMemInfo->sDevVAddr.uiAddr +
                                            ((IMG_UINT32)(pui32VDMControlStreamBuffer - pui32VDMControlStreamBufferStart) * sizeof(IMG_UINT32));
        srft_WriteMTEStateToVDM(&pui32VDMControlStreamBuffer,
                                &sMTEStateCopyPDSProgDevVAddr,
                                &sMTEStateCopyPDSProgram,
                                ui32MTEStateUpdateDWORDCount);


        /**************************************************************************
            Generate the Vertices and indices for the triangles
        **************************************************************************/
        sVerticesDevVAddr.uiAddr = psVertexDataMemInfo->sDevVAddr.uiAddr +
                                    ((IMG_UINT32)(pui32VertexDataBuffer - pui32VertexDataBufferStart) * sizeof(IMG_UINT32));

        if (ui32LoopCounter == 0)
        {
            /*
                Specify two triangles to cover entire render area. This will produce a
                rectangular block which should cover the entire render area. Failure to cover the
                entire render surface produces an invalid render.
            */
            ui32NumTrianglesInKick+=2; /* The count 2 added represents the two triangles used */
            srft_WriteClearObjToVertexDataBuffer(&pui32VertexDataBuffer, psConfig,
                                                 psShared->ui32RectangleSizeX,
                                                 psShared->ui32RectangleSizeY);
        }
                     
        /*
            Draw the real triangles.
        */
        {			
            srft_WriteVerticesToVertexDataBuffer(&pui32VertexDataBuffer, psConfig,
                                                 psShared->ui32RectangleSizeX,
                                                 psShared->ui32RectangleSizeY,
                                                 ui32CyclePosition,
                                                 ui32NumTrianglesInFrame,
                                                 ui32FirstTriangle, ui32LastTriangle);
        }

        /**************************************************************************
            Generate the vertex shader program
        **************************************************************************/
        sUSSEProgDevVAddr.uiAddr = psVertexUSSEMemInfo->sDevVAddr.uiAddr +
                                    ((IMG_UINT32)(pui32VertexUSSEBuffer - pui32VertexUSSEBufferStart) * sizeof(IMG_UINT32));

        ALIGN_USSE_BUFFER_DEVV(sUSSEProgDevVAddr.uiAddr)
        ALIGN_USSE_BUFFER_CPUV(pui32VertexUSSEBuffer)

        sVertexPDSProgDevVAddr.uiAddr = psVertexPDSMemInfo->sDevVAddr.uiAddr +
                                            ((IMG_UINT32)(pui32VertexPDSBuffer - pui32VertexPDSBufferStart) * sizeof(IMG_UINT32));

        ALIGN_PDS_BUFFER_DEVV(sVertexPDSProgDevVAddr.uiAddr)
        ALIGN_PDS_BUFFER_CPUV(pui32VertexPDSBuffer)

        srft_WriteVertexShaderCode(&pui32VertexUSSEBuffer,
                                   &sUSSEProgDevVAddr,
                                   &pui32VertexPDSBuffer,
                                   &sVertexPDSProgDevVAddr,
                                   &sVertexShaderPDSProgram,
                                   psShared->psSGXInfo,
                                   &sVerticesDevVAddr,
                                   psShared->uUSEVertexHeapBase,
                                   &ui32NumVertexShaderPAsInBytes,
                                   psConfig->ui32SlowdownFactorTA,
                                   CHECK_FRAME_NUM_LAST(ui32RenderCount, psConfig->ui32TestHWRRateVS),
                                   psConfig->bUSSEVertexWriteMem,
                                   psUSSEVertexMemFlagsTestMemInfo,
                                   psConfig->bDebugProgramAddrs);

        /**************************************************************************
            Write the index list block to the VDM control stream
        **************************************************************************/
        srft_WriteIndexListToVDM(&pui32VDMControlStreamBuffer,
                                 ui32NumTrianglesInKick,
                                 ui32NumVertexShaderPAsInBytes,
                                 &psIndexMemInfo->sDevVAddr,
                                 &sVertexPDSProgDevVAddr,
                                 &sVertexShaderPDSProgram);

        /**************************************************************************
            Send other objects
        **************************************************************************/
        /* No more objects required for the purposes of this test */


        /**************************************************************************
            Generate the MTE terminate programs
        **************************************************************************/
        sUSSEProgDevVAddr.uiAddr = psVertexUSSEMemInfo->sDevVAddr.uiAddr +
                                    ((IMG_UINT32)(pui32VertexUSSEBuffer - pui32VertexUSSEBufferStart) * sizeof(IMG_UINT32));

        ALIGN_USSE_BUFFER_DEVV(sUSSEProgDevVAddr.uiAddr)
        ALIGN_USSE_BUFFER_CPUV(pui32VertexUSSEBuffer)

        sTerminatePDSProgDevVAddr.uiAddr = psVertexPDSMemInfo->sDevVAddr.uiAddr +
                                           ((IMG_UINT32)(pui32VertexPDSBuffer - pui32VertexPDSBufferStart) * sizeof(IMG_UINT32));

        ALIGN_PDS_BUFFER_DEVV(sTerminatePDSProgDevVAddr.uiAddr)
        ALIGN_PDS_BUFFER_CPUV(pui32VertexPDSBuffer)

        srft_WriteMTETerminateCode(&pui32VertexUSSEBuffer,
                                   &sUSSEProgDevVAddr,
                                   &pui32VertexPDSBuffer,
                                   &sTerminatePDSProgDevVAddr,
                                   &sTerminatePDSProgram,
                                   psShared->uUSEVertexHeapBase,
                                   psShared->psSGXInfo,
                                   psShared->ui32RectangleSizeX,
                                   psShared->ui32RectangleSizeY,
                                   psConfig->bDebugProgramAddrs);

        /**************************************************************************
            Write the MTE terminate to the VDM control stream
        **************************************************************************/
        srft_WriteMTETerminateToVDM(&pui32VDMControlStreamBuffer,
                                    &sTerminatePDSProgDevVAddr,
                                    &sTerminatePDSProgram);

        /**************************************************************************
            Make sure the TA buffers haven't been exceeded
        **************************************************************************/
        PVR_ASSERT((IMG_UINT32)((IMG_CHAR*)pui32VertexUSSEBuffer - (IMG_CHAR*)pui32VertexUSSEBufferStart) < psVertexUSSEMemInfo->uAllocSize);
        PVR_ASSERT((IMG_UINT32)((IMG_CHAR*)pui32VDMControlStreamBuffer - (IMG_CHAR*)pui32VDMControlStreamBufferStart) < psVDMControlStreamMemInfo->uAllocSize);
        PVR_ASSERT((IMG_UINT32)((IMG_CHAR*)pui32VertexDataBuffer - (IMG_CHAR*)pui32VertexDataBufferStart) < psVertexDataMemInfo->uAllocSize);
        PVR_ASSERT((IMG_UINT32)((IMG_CHAR*)pui32VertexPDSBuffer - (IMG_CHAR*)pui32VertexPDSBufferStart) < psVertexPDSMemInfo->uAllocSize);

        /**************************************************************************
            Set up the Kick TA structure
        **************************************************************************/
        srft_SetupTAKick(&sKickTA, pui32HWBGObjPDSState, psShared->hRenderContext, psHWRegs,
                         (IMG_BOOL)(ui32LoopCounter == 0),
                         (IMG_BOOL)(ui32LoopCounter == ui32LoopMax),
                         psConfig->b2XMSAADontResolve,
                         ui32FrameNum,
                         bNoMainRenderSyncObject ? IMG_NULL : psRenderSurfMemInfo, hRTDataSet,
#if defined(__psp2__)
                         psShared->ui32RectangleSizeX, psShared->ui32RectangleSizeY,
#endif
                        sVDMControlStreamDevVAddr,
                         CHECK_FRAME_NUM_LAST(ui32RenderCount, psConfig->ui32TestHWRRateVDM),
                         pvKickTAPDump);


        /**************************************************************************
            PDUMP TA data buffers
        **************************************************************************/
        srft_PDumpTABuffers(psShared->psConnection,
                            psVertexUSSEMemInfo, pui32VertexUSSEBuffer, pui32VertexUSSEBufferKickStart, pui32VertexUSSEBufferStart,
                            psVDMControlStreamMemInfo, pui32VDMControlStreamBuffer, pui32VDMControlStreamBufferKickStart, pui32VDMControlStreamBufferStart,
                            psVertexDataMemInfo, pui32VertexDataBuffer, pui32VertexDataBufferKickStart, pui32VertexDataBufferStart,
                            psVertexPDSMemInfo, pui32VertexPDSBuffer, pui32VertexPDSBufferKickStart, pui32VertexPDSBufferStart);

        /**************************************************************************
            Kick SGX
        **************************************************************************/
#ifdef __psp2__
        eResult = SGXKickTA(psShared->psSGXDevData, &sKickTA, &sKickTAOutput, IMG_NULL, IMG_NULL);
#else
        eResult = SGXKickTA(psShared->psSGXDevData, &sKickTA, &sKickTAOutput, pvKickTAPDump, IMG_NULL);
#endif
        sgxut_fail_if_error_quiet(eResult);
    }
    
    if (psConfig->bReadMem)
    {
        #if defined(SUPPORT_SGX_EDM_MEMORY_DEBUG)
        IMG_DEV_VIRTADDR	sDevVAddr;
        IMG_PVOID			pvLinAddr;

        /*
             Read memory location in microkernel
        */
        sDevVAddr = psVDMControlStreamMemInfo->sDevVAddr;
        pvLinAddr = psVDMControlStreamMemInfo->pvLinAddr;
        sgxut_read_devicemem(psShared->psSGXDevData, psShared->hDevMemContext, &sDevVAddr, pvLinAddr);
        #endif /* SUPPORT_SGX_EDM_MEMORY_DEBUG */
    }
    
    *ppui32VertexDataBuffer = pui32VertexDataBuffer;
    *ppui32VertexPDSBuffer = pui32VertexPDSBuffer;
    *ppui32VertexUSSEBuffer = pui32VertexUSSEBuffer;
    *ppui32VDMControlStreamBuffer = pui32VDMControlStreamBuffer;
}


static IMG_VOID srft_PresentBlit(SRFT_CONFIG			*psConfig,
                                 SRFT_SHARED			*psShared,
                                 PVRSRV_CLIENT_MEM_INFO	*psSrcMemInfo,
                                 IMG_UINT32				ui32SrcWidth,
                                 IMG_UINT32				ui32SrcHeight,
                                 IMG_UINT32				ui32SrcStride,
                                 PVRSRV_CLIENT_MEM_INFO	*psDestMemInfo,
                                 IMG_UINT32				ui32DstWidth,
                                 IMG_UINT32				ui32DstHeight,
                                 IMG_UINT32				ui32DstStride,
                                 IMG_BOOL				bNoDestSync,
                                 IMG_UINT32				ui32RectanglePosX,
                                 IMG_UINT32				ui32RectanglePosY,
                                 IMG_BOOL				bTestHWR)
{
    PVRSRV_ERROR			eResult;
    SGX_QUEUETRANSFER 		sQueueTransfer;
    IMG_DEV_VIRTADDR		sDstDevVAddr;

    /* Queue a blit to the display surface */
    memset(&sQueueTransfer, 0, sizeof(SGX_QUEUETRANSFER));

    sQueueTransfer.ui32NumSources = 1;
    sQueueTransfer.asSources[0].psSyncInfo = psSrcMemInfo->psClientSyncInfo;

    sQueueTransfer.ui32NumDest = 1;
    sQueueTransfer.asDests[0].psSyncInfo = bNoDestSync ? IMG_NULL : psDestMemInfo->psClientSyncInfo;

    sQueueTransfer.ui32NumStatusValues = 0;
    
    sDstDevVAddr.uiAddr = bTestHWR ? SRFT_PAGE_FAULT_ADDRESS : psDestMemInfo->sDevVAddr.uiAddr;

    if (psConfig->bDebugSurfaceAddrs)
    {
        DPF("Blit Src:0x%x Dst:0x%x\n",
            psSrcMemInfo->sDevVAddr.uiAddr, sDstDevVAddr.uiAddr);
    }
    
    #if defined (SGX_FEATURE_2D_HARDWARE) && !defined(SGX_FEATURE_PTLA)
    if (psConfig->b2DBlit)
    {
        IMG_UINT32	ui32Blit2DCtrlSizeInDwords = 0;
        IMG_UINT32	aui32Blit2DCtrlStream[25];

        {
            /* construct the control stream */

            /* src surface */
            aui32Blit2DCtrlStream[ui32Blit2DCtrlSizeInDwords++] = 0x90000000 | (psShared->ui322DSurfaceFormat << 15) | ui32SrcStride;
            aui32Blit2DCtrlStream[ui32Blit2DCtrlSizeInDwords++] = psSrcMemInfo->sDevVAddr.uiAddr - psShared->u2DHeapBase.uiAddr;
            /* src offset */
            aui32Blit2DCtrlStream[ui32Blit2DCtrlSizeInDwords++] = 0x30000000 | 0;
            /* dest surface */
            aui32Blit2DCtrlStream[ui32Blit2DCtrlSizeInDwords++] = 0xA0000000 | (psShared->ui322DSurfaceFormat << 15) | ui32DstStride;
            aui32Blit2DCtrlStream[ui32Blit2DCtrlSizeInDwords++] = sDstDevVAddr.uiAddr - psShared->u2DHeapBase.uiAddr;
            /* blt rectangle */
            aui32Blit2DCtrlStream[ui32Blit2DCtrlSizeInDwords++] = 0x80000000 | 0x001CCCC;
            aui32Blit2DCtrlStream[ui32Blit2DCtrlSizeInDwords++] = (ui32RectanglePosX << 12) | (ui32RectanglePosY << 0);
            aui32Blit2DCtrlStream[ui32Blit2DCtrlSizeInDwords++] = (ui32SrcWidth << 12) |
                                                                    (ui32SrcHeight << 0);
        }

        /* do the SRCCOPY blit via 2D */
        sQueueTransfer.eType = SGXTQ_2DHWBLIT;
        sQueueTransfer.ui32Flags = 0;
        sQueueTransfer.Details.s2DBlit.ui32CtrlSizeInDwords = ui32Blit2DCtrlSizeInDwords;
        sQueueTransfer.Details.s2DBlit.pui32CtrlStream = &aui32Blit2DCtrlStream[0];	/* PRQA S 3217 */ /* override pointer size change */

        SGX_QUEUETRANSFER_NUM_SRC_RECTS(sQueueTransfer) = 0;
        SGX_QUEUETRANSFER_NUM_DST_RECTS(sQueueTransfer) = 0;
    }
    else
    #endif /* SGX_FEATURE_2D_HARDWARE */
    {
        sQueueTransfer.eType = SGXTQ_BLIT;
        sQueueTransfer.ui32Flags = SGX_TRANSFER_DISPATCH_DISABLE_SW;
        
        if (psConfig->b2DBlit)
        {
            sQueueTransfer.ui32Flags |= SGX_TRANSFER_DISPATCH_DISABLE_3D;
        }
        else
        {
            sQueueTransfer.ui32Flags |= SGX_TRANSFER_DISPATCH_DISABLE_PTLA;
        }
        
        sQueueTransfer.Details.sBlit.eFilter = SGXTQ_FILTERTYPE_POINT;
        sQueueTransfer.Details.sBlit.eColourKey = SGXTQ_COLOURKEY_NONE;
        sQueueTransfer.Details.sBlit.ui32ColourKey = 0;
        sQueueTransfer.Details.sBlit.eRotation = SGXTQ_ROTATION_NONE;

        SGX_TQSURFACE_SET_ADDR(sQueueTransfer.asSources[0], psSrcMemInfo, 0);
        sQueueTransfer.asSources[0].ui32Width = ui32SrcWidth;
        sQueueTransfer.asSources[0].ui32Height = ui32SrcHeight;
        sQueueTransfer.asSources[0].i32StrideInBytes = (IMG_INT32)ui32SrcStride;
        sQueueTransfer.asSources[0].eFormat = psShared->psPrimFormat->pixelformat;
        sQueueTransfer.asSources[0].eMemLayout = SGXTQ_MEMLAYOUT_STRIDE;
        sQueueTransfer.asSources[0].ui32ChunkStride = 0;

        SGX_TQSURFACE_SET_ADDR(sQueueTransfer.asDests[0], psDestMemInfo, 0);
        sQueueTransfer.asDests[0].ui32Width = ui32DstWidth;
        sQueueTransfer.asDests[0].ui32Height = ui32DstHeight;
        sQueueTransfer.asDests[0].i32StrideInBytes = (IMG_INT32)ui32DstStride;
        sQueueTransfer.asDests[0].eFormat = psShared->psPrimFormat->pixelformat;
        sQueueTransfer.asDests[0].eMemLayout = SGXTQ_MEMLAYOUT_OUT_LINEAR;
        sQueueTransfer.asDests[0].ui32ChunkStride = 0;

        SGX_QUEUETRANSFER_NUM_SRC_RECTS(sQueueTransfer) = 1;
        SGX_QUEUETRANSFER_SRC_RECT(sQueueTransfer, 0).x0 = 0;
        SGX_QUEUETRANSFER_SRC_RECT(sQueueTransfer, 0).y0 = 0;
        SGX_QUEUETRANSFER_SRC_RECT(sQueueTransfer, 0).x1 = (IMG_INT32)sQueueTransfer.asSources[0].ui32Width;
        SGX_QUEUETRANSFER_SRC_RECT(sQueueTransfer, 0).y1 = (IMG_INT32)sQueueTransfer.asSources[0].ui32Height;

        SGX_QUEUETRANSFER_NUM_DST_RECTS(sQueueTransfer) = 1;
        SGX_QUEUETRANSFER_DST_RECT(sQueueTransfer, 0).x0 = (IMG_INT32)ui32RectanglePosX;
        SGX_QUEUETRANSFER_DST_RECT(sQueueTransfer, 0).y0 = (IMG_INT32)ui32RectanglePosY;
        SGX_QUEUETRANSFER_DST_RECT(sQueueTransfer, 0).x1 = SGX_QUEUETRANSFER_DST_RECT(sQueueTransfer, 0).x0 +
                                            (IMG_INT32)sQueueTransfer.asSources[0].ui32Width;
        SGX_QUEUETRANSFER_DST_RECT(sQueueTransfer, 0).y1 = SGX_QUEUETRANSFER_DST_RECT(sQueueTransfer, 0).y0 +
                                            (IMG_INT32)sQueueTransfer.asSources[0].ui32Height;
    }

    if (!psConfig->bNoTransfer)
    {
        //TODO eResult = SGXQueueTransfer(psShared->hTransferContext, &sQueueTransfer);
        sgxut_fail_if_error_quiet(eResult);
    }
}


static IMG_VOID srft_AddRenderTarget(SRFT_CONFIG		*psConfig,
                                     SRFT_SHARED		*psShared,
                                     IMG_HANDLE			hRenderContext,
                                     PVRSRV_DEV_DATA	*psSGXDevData,
                                     IMG_UINT32			ui32RectangleSizeX,
                                     IMG_UINT32			ui32RectangleSizeY,
                                     IMG_UINT32			ui32DstSurfaceWidth,
                                     IMG_HANDLE			*phRTDataSet)
{
    PVRSRV_ERROR		eResult;
    SGX_ADDRENDTARG		sAddRTInfo = {0};
#ifdef __psp2__
    SceUID				rtMemUID;
    IMG_SID				hRtMem = 0;
#endif

    sAddRTInfo.ui32Flags			= 0;
	sAddRTInfo.ui32RendersPerFrame = psConfig->ui32RendersPerFrame;
    sAddRTInfo.hRenderContext		= hRenderContext;
    sAddRTInfo.hDevCookie			= psSGXDevData->hDevCookie;
    sAddRTInfo.ui32NumPixelsX		= ui32RectangleSizeX;
    sAddRTInfo.ui32NumPixelsY		= ui32RectangleSizeY;
    sAddRTInfo.ui16MSAASamplesInX	= 1;
    sAddRTInfo.eForceScalingInX		= SGX_SCALING_NONE;
    if (psConfig->b2XMSAADontResolve)
    {
        sAddRTInfo.ui16MSAASamplesInY	= 2;
        sAddRTInfo.eForceScalingInY		= SGX_UPSCALING;
    }
    else
    {
        sAddRTInfo.ui16MSAASamplesInY	= 1;
        sAddRTInfo.eForceScalingInY		= SGX_SCALING_NONE;
    }

    {
        FLOATORULONG fBGObjUCoord;
        fBGObjUCoord.f = (IMG_FLOAT)ui32RectangleSizeX / (IMG_FLOAT) ui32DstSurfaceWidth;
        sAddRTInfo.ui32BGObjUCoord	= fBGObjUCoord.ul;
    }
    sAddRTInfo.ui16NumRTsInArray	= 1;

#ifdef __psp2__

    rtMemUID = sceKernelAllocMemBlock("SGXRenderTarget", SCE_KERNEL_MEMBLOCK_TYPE_USER_NC_RW, 0x100000, NULL);

    PVRSRVRegisterMemBlock(psSGXDevData, rtMemUID, &hRtMem, IMG_TRUE);

    sAddRTInfo.eRotation = PVRSRV_ROTATE_0;
    sAddRTInfo.ui32RendersPerQueueSwap = 3;
    sAddRTInfo.ui8MacrotileCountX = 0;
    sAddRTInfo.ui8MacrotileCountY = 0;
    sAddRTInfo.i32DataMemblockUID = rtMemUID;
    sAddRTInfo.bUseExternalUID = IMG_FALSE;
    sAddRTInfo.hMemBlockProcRef = hRtMem;
    sAddRTInfo.ui32MultisampleLocations = 0;

#endif

    psShared->fnInfo("Adding render target via SGXAddRenderTarget, %ux%u pixels\n",
                     sAddRTInfo.ui32NumPixelsX, sAddRTInfo.ui32NumPixelsY);
    eResult = SGXAddRenderTarget(psSGXDevData,
                                 &sAddRTInfo,
                                 phRTDataSet);
    psShared->fnInfoFailIfError(eResult);
}


static IMG_VOID *srft_PerThread(IMG_VOID	*pvArg)
{
    PVRSRV_ERROR						eResult;
    IMG_UINT32							i;
    SRFT_THREAD_DATA					*psThreadData = pvArg;
    SRFT_CONFIG							*psConfig = psThreadData->psConfig;
    SRFT_SHARED							*psShared = psThreadData->psShared;

    fnDPF								fnInfo = psShared->fnInfo;
    fnFIE								fnInfoFailIfError = psShared->fnInfoFailIfError;
    IMG_CHAR							aui8Preamble[100];

    IMG_UINT32							ui32StartTimeMS, ui32EndTimeMS;
    IMG_UINT32							ui32RenderIndex;
    IMG_UINT32 							ui32RenderCount;
    IMG_UINT32							ui32FrameNum;
    PVRSRV_CLIENT_MEM_INFO				*psRenderSurfMemInfo;
    IMG_UINT32							ui32BackBufferIdx = 0;
    IMG_UINT32							ui32RenderAddress;
    IMG_UINT32							ui32SwapChainIndex = 0;
    IMG_INT32							i32OldSelectBuffer = 0;
    IMG_UINT32							ui32Size;

    PVRSRV_CLIENT_MEM_INFO				*psFlushSurfMemInfo = IMG_NULL;
    PVRSRV_SYNC_TOKEN					sSyncToken;

    IMG_UINT32							ui32GridPosX = psThreadData->ui32GridPosX;
    IMG_UINT32							ui32GridPosY = psThreadData->ui32GridPosY;

    IMG_UINT32							ui32MainRenderLineStride = psShared->ui32MainRenderLineStride;
    IMG_UINT32							ui32RectanglePosX = psShared->ui32RectangleSizeX * ui32GridPosX;
    IMG_UINT32							ui32RectanglePosY = psShared->ui32RectangleSizeY * ui32GridPosY;
    IMG_UINT32							ui32BytesPerPixel = psShared->ui32BytesPerPixel;

    PVRSRV_CLIENT_MEM_INFO				**apsBackBufferMemInfo = IMG_NULL;
    IMG_HANDLE							hRTDataSet;
    IMG_UINT32							ui32SideBandWord;
    IMG_DEV_VIRTADDR					sUSSEProgDevVAddr;
    IMG_DEV_VIRTADDR					sPixelEventPDSProgDevVAddr;
    IMG_UINT32							ui32StateSize;
    PVR_HWREGS							sHWRegs;
    IMG_DEV_VIRTADDR					sHWBGObjPDSProgDevVAddr;
    IMG_DEV_VIRTADDR					sHWBGObjPixelSecondaryPDSProgDevVAddr;
    IMG_UINT32							aui32HWBGObjPDSState[3];
    IMG_DEV_VIRTADDR					sMainFragmentPDSProgDevVAddr;
    IMG_DEV_VIRTADDR					sMainPixelSecondaryPDSProgDevVAddr;
    IMG_UINT32							aui32MainPDSState[3];

    EDBP_STATE_DATA                     *psEdbpState = psShared->psEdbpState;
    IMG_VOID                            (*pfnCallbackFunc)(IMG_VOID *);
    IMG_VOID                            *pvCallbackArg;

    /**************************************************************************
    *	Buffer pointers
    **************************************************************************/
    PVRSRV_CLIENT_MEM_INFO				*psIndexMemInfo;

    PVRSRV_CLIENT_MEM_INFO				*apsFragmentPDSMemInfo[RENDER_ARRAY_SIZE];
    #define FRAGMENT_PDS_BUFFER_SIZE	(1 * 1024)
    IMG_UINT32							*pui32FragmentPDSBuffer;
    IMG_UINT32							*pui32FragmentPDSBufferStart;

    PVRSRV_CLIENT_MEM_INFO				*apsVertexPDSMemInfo[RENDER_ARRAY_SIZE];
    #define VERTEX_PDS_BUFFER_SIZE		(1 * 1024)
    IMG_UINT32							*pui32VertexPDSBuffer;
    IMG_UINT32							*pui32VertexPDSBufferStart;

    PVRSRV_CLIENT_MEM_INFO				*apsVertexDataMemInfo[RENDER_ARRAY_SIZE];
    #define VERTEX_BUFFER_SIZE			(0x10000)
    IMG_UINT32							*pui32VertexDataBuffer;
    IMG_UINT32							*pui32VertexDataBufferStart;

    PVRSRV_CLIENT_MEM_INFO				*apsVDMControlStreamMemInfo[RENDER_ARRAY_SIZE];
    #define VDM_CTRL_STREAM_BUFFER_SIZE	(0x10000)
    IMG_UINT32							*pui32VDMControlStreamBuffer;
    IMG_UINT32							*pui32VDMControlStreamBufferStart;

    PVRSRV_CLIENT_MEM_INFO				*apsVertexUSSEMemInfo[RENDER_ARRAY_SIZE];
    #define VERTEX_USE_BUFFER_SIZE		(1 * 1024)
    IMG_UINT32							*pui32VertexUSSEBuffer;
    IMG_UINT32							*pui32VertexUSSEBufferStart;

    PVRSRV_CLIENT_MEM_INFO				*apsFragmentUSSEMemInfo[RENDER_ARRAY_SIZE];
    #define FRAGMENT_USE_BUFFER_SIZE	(2 * 1024)
    IMG_UINT32							*pui32FragmentUSSEBuffer;
    IMG_UINT32							*pui32FragmentUSSEBufferStart;

    PVRSRV_CLIENT_MEM_INFO				*psUSSEPixelMemFlagsTestMemInfo = IMG_NULL;
    PVRSRV_CLIENT_MEM_INFO				*psUSSEVertexMemFlagsTestMemInfo = IMG_NULL;
    PVRSRV_CLIENT_MEM_INFO				*ps3DParamsHeapReservedMemInfo = IMG_NULL;
    PVRSRV_CLIENT_MEM_INFO				*psPixelShaderHeapReservedMemInfo = IMG_NULL;

    /**************************************************************************
    *	PDS program structures to be used in calls to PDS codegen
    **************************************************************************/
    PDS_PIXEL_EVENT_PROGRAM				sPixelEventProgram  = {0};
    PDS_TEXTURE_IMAGE_UNIT				sTextureImageUnit  = {0};
    PDS_PIXEL_SHADER_SA_PROGRAM			sPixelShaderSAProgram = {0};
    PDS_PIXEL_SHADER_PROGRAM			sHWBGOBJPixelShaderProgram  = {0};
    PDS_PIXEL_SHADER_PROGRAM			sMainPixelShaderProgram  = {0};

    if (psConfig->bOutputPreamble)
    {
        sprintf(aui8Preamble, "(%u, %u) ", ui32GridPosX, ui32GridPosY);
    }
    else
    {
        aui8Preamble[0] = 0;
    }

    /*TODO if (psConfig->bBlit)
    {*/
        /* Allocate back buffers */
        /*apsBackBufferMemInfo = malloc(psConfig->ui32NumBackBuffers * sizeof(apsBackBufferMemInfo[0]));
        if (apsBackBufferMemInfo == IMG_NULL)
        {
            fnInfoFailIfError(PVRSRV_ERROR_OUT_OF_MEMORY);
        }

        SGXUT_CHECK_MEM_ALLOC(apsBackBufferMemInfo);

        for(i = 0; i < psConfig->ui32NumBackBuffers; i++)
        {
#if defined (SUPPORT_SID_INTERFACE)
            IMG_SID hHeap = psShared->hGeneralHeap;
#else
            IMG_HANDLE hHeap = psShared->hGeneralHeap;
#endif
            
            #if !defined(SGX_FEATURE_PTLA)
            if (psConfig->b2DBlit)
            {
                hHeap = psShared->h2DHeap;
            }
            #endif*/ /* SGX_FEATURE_PTLA */
            
            /*fnInfo("Allocate back buffer %u\n", i);
            eResult = PVRSRVAllocDeviceMem(psShared->psSGXDevData,
                                           hHeap,
                                           psConfig->ui32PBEMemFlags,
                                           psShared->ui32RectangleSizeX * psShared->ui32RectangleSizeY * ui32BytesPerPixel,
                                           4096,
                                           &apsBackBufferMemInfo[i]);
            fnInfoFailIfError(eResult);
            
            edbp_NumberedDeviceBuffer3(psEdbpState, "BackBuffer", ui32GridPosX, ui32GridPosY, i, apsBackBufferMemInfo[i]);

            if (psConfig->bDebugSurfaceAddrs)
            {
                DPF("Back buffer %u - Address 0x%x Size 0x%x\n", i,
                    apsBackBufferMemInfo[i]->sDevVAddr.uiAddr,
                    apsBackBufferMemInfo[i]->uAllocSize);
            }			
        }
    }*/

    /**************************************************************************
    *	Add a new render target
    **************************************************************************/
    srft_AddRenderTarget(psConfig, psShared,
                         psShared->hRenderContext, psShared->psSGXDevData,
                         psShared->ui32RectangleSizeX, psShared->ui32RectangleSizeY,
                         ui32MainRenderLineStride, &hRTDataSet);

    /**************************************************************************
    * Allocate Device Memory Buffers
    **************************************************************************/

    /**************************************************************************
    * Allocate a static index buffer
    **************************************************************************/
    {
        IMG_UINT32 ui32NumTriangles;
        
        ui32NumTriangles = MAX(psConfig->ui32TrianglesPerFrame1, psConfig->ui32TrianglesPerFrame2);
        ui32NumTriangles+=2; // 2 triangles for the clear objectv 
        ui32Size = sizeof(IMG_UINT16) * ui32NumTriangles * 3;
        fnInfo("Allocate a static index buffer\n");
#ifdef __psp2__
        ui32Size = ALIGN(ui32Size, SCE_KERNEL_4KiB);

        eResult = PVRSRVAllocDeviceMem(psShared->psSGXDevData,
                                       psShared->hGeneralHeap,
                                       PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ | PVRSRV_HAP_NO_GPU_VIRTUAL_ON_ALLOC,
                                       ui32Size,
                                       4,
                                       hPbMem,
                                       &psIndexMemInfo);
        fnInfoFailIfError(eResult);
#else
        eResult = PVRSRVAllocDeviceMem(psShared->psSGXDevData,
                                       psShared->hGeneralHeap,
                                       PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ,
                                       ui32Size,
                                       4,
                                       &psIndexMemInfo);
#endif
        fnInfoFailIfError(eResult);

        srft_WriteIndexBuffer(psIndexMemInfo->pvLinAddr, ui32NumTriangles);

        srft_PDumpBuffer(psShared->psConnection, "Dumping Index buffer",
                         psIndexMemInfo, 0, psIndexMemInfo->uAllocSize,
                         IMG_TRUE);

        edbp_NumberedDeviceBuffer3(psEdbpState, "Index", ui32GridPosX, ui32GridPosY, 0, psIndexMemInfo);
    }

    if (psConfig->ui32PixelShaderHeapReserve > 0)
    {
        /*
            By allocating this memory first, we force the shaders to be
            allocated higher up the heap.
        */

#ifdef __psp2__
        IMG_UINT32 pixelShaderAllocSize = ALIGN(psConfig->ui32PixelShaderHeapReserve * 1024, SCE_KERNEL_4KiB);

        fnInfo("Allocate %ukB pixel shader reserve memory\n",
            pixelShaderAllocSize / 1024);

        eResult = PVRSRVAllocDeviceMem(psShared->psSGXDevData,
                                       psShared->hUSEFragmentHeap,
                                       PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ | PVRSRV_HAP_NO_GPU_VIRTUAL_ON_ALLOC,
                                       pixelShaderAllocSize,
                                       0,
                                       hPbMem,
                                       &psPixelShaderHeapReservedMemInfo);
        fnInfoFailIfError(eResult);

        /*eResult = PVRSRVMapMemoryToGpu(psShared->psSGXDevData,
                                       psShared->hDevMemContext,
                                       psShared->hUSEFragmentHeap,
                                       pixelShaderAllocSize,
                                       1UL << SGX_USE_CODE_SEGMENT_RANGE_BITS,
                                       psPixelShaderHeapReservedMemInfo->pvLinAddr,
                                       PVRSRV_MEM_READ,
                                       &psPixelShaderHeapReservedMemInfo->sDevVAddr.uiAddr);*/
#else
        fnInfo("Allocate %ukB pixel shader reserve memory\n",
            psConfig->ui32PixelShaderHeapReserve);

        eResult = PVRSRVAllocDeviceMem(psShared->psSGXDevData,
                                       psShared->hUSEFragmentHeap,
                                       PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ,
                                       psConfig->ui32PixelShaderHeapReserve * 1024,
                                       0,
                                       &psPixelShaderHeapReservedMemInfo);
#endif
        fnInfoFailIfError(eResult);
    }
    
    for (ui32RenderIndex = 0; ui32RenderIndex < RENDER_ARRAY_SIZE; ui32RenderIndex++)
    {
        IMG_UINT32	ui32Align;

        /**************************************************************************
        * Allocate a static Fragment PDS buffer
        **************************************************************************/
        ui32Size = FRAGMENT_PDS_BUFFER_SIZE;
        ui32Align = EURASIA_PDS_DATA_CACHE_LINE_SIZE;

        fnInfo("Allocate a static Fragment PDS buffer (%u)\n", ui32RenderIndex);

#ifdef __psp2__
        ui32Size = ALIGN(ui32Size, SCE_KERNEL_4KiB);

        eResult = PVRSRVAllocDeviceMem(psShared->psSGXDevData,
                                       psShared->hPDSFragmentHeap,
                                       psConfig->ui32PDSPixelCodeMemFlags,
                                       ui32Size,
                                       ui32Align,
                                       hPbMem,
                                       &apsFragmentPDSMemInfo[ui32RenderIndex]);
        fnInfoFailIfError(eResult);
#else
        eResult = PVRSRVAllocDeviceMem(psShared->psSGXDevData,
                                       psShared->hPDSFragmentHeap,
                                       psConfig->ui32PDSPixelCodeMemFlags,
                                       ui32Size,
                                       ui32Align,
                                       &apsFragmentPDSMemInfo[ui32RenderIndex]);
#endif
        fnInfoFailIfError(eResult);
        memset(apsFragmentPDSMemInfo[ui32RenderIndex]->pvLinAddr, 0, ui32Size);
        edbp_NumberedDeviceBuffer3(psEdbpState, "FragmentPDS", ui32GridPosX, ui32GridPosY, ui32RenderIndex, apsFragmentPDSMemInfo[ui32RenderIndex]);


        /**************************************************************************
        * Allocate a static Vertex PDS buffer
        **************************************************************************/
        ui32Size = FRAGMENT_PDS_BUFFER_SIZE;
        ui32Align = EURASIA_PDS_DATA_CACHE_LINE_SIZE;

        fnInfo("Allocate a static Vertex PDS buffer (%u)\n", ui32RenderIndex);
#ifdef __psp2__
        ui32Size = ALIGN(ui32Size, SCE_KERNEL_4KiB);

        eResult = PVRSRVAllocDeviceMem(psShared->psSGXDevData,
                                       psShared->hPDSVertexHeap,
                                       psConfig->ui32PDSVertexCodeMemFlags,
                                       ui32Size,
                                       ui32Align,
                                       hPbMem,
                                       &apsVertexPDSMemInfo[ui32RenderIndex]);
        fnInfoFailIfError(eResult);
#else
        eResult = PVRSRVAllocDeviceMem(psShared->psSGXDevData,
                                       psShared->hPDSVertexHeap,
                                       psConfig->ui32PDSVertexCodeMemFlags,
                                       ui32Size,
                                       ui32Align,
                                       &apsVertexPDSMemInfo[ui32RenderIndex]);
#endif
        fnInfoFailIfError(eResult);
        memset(apsVertexPDSMemInfo[ui32RenderIndex]->pvLinAddr, 0, ui32Size);
        edbp_NumberedDeviceBuffer3(psEdbpState, "VertexPDS", ui32GridPosX, ui32GridPosY, ui32RenderIndex, apsVertexPDSMemInfo[ui32RenderIndex]);


        /**************************************************************************
        * Allocate a static vertex buffer
        **************************************************************************/
        ui32Size = VERTEX_BUFFER_SIZE;
        fnInfo("Allocate a static vertex buffer (%u)\n", ui32RenderIndex);
#ifdef __psp2__
        ui32Size = ALIGN(ui32Size, SCE_KERNEL_4KiB);

        eResult = PVRSRVAllocDeviceMem(psShared->psSGXDevData,
                                       psShared->hGeneralHeap,
                                       PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ | PVRSRV_HAP_NO_GPU_VIRTUAL_ON_ALLOC,
                                       ui32Size,
                                       4,
                                       hPbMem,
                                       &apsVertexDataMemInfo[ui32RenderIndex]);
        fnInfoFailIfError(eResult);
#else
        eResult = PVRSRVAllocDeviceMem(psShared->psSGXDevData,
                                       psShared->hGeneralHeap,
                                       PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ,
                                       ui32Size,
                                       4,
                                       &apsVertexDataMemInfo[ui32RenderIndex]);
#endif
        fnInfoFailIfError(eResult);
        memset(apsVertexDataMemInfo[ui32RenderIndex]->pvLinAddr, 0, ui32Size);
        edbp_NumberedDeviceBuffer3(psEdbpState, "VertexData", ui32GridPosX, ui32GridPosY, ui32RenderIndex, apsVertexDataMemInfo[ui32RenderIndex]);

        /**************************************************************************
        * Allocate a static VDM control stream buffer
        **************************************************************************/
        ui32Size = VDM_CTRL_STREAM_BUFFER_SIZE;
        fnInfo("Allocate a static VDM control stream buffer (%u)\n", ui32RenderIndex);
#ifdef __psp2__
        ui32Size = ALIGN(ui32Size, SCE_KERNEL_4KiB);

        eResult = PVRSRVAllocDeviceMem(psShared->psSGXDevData,
                                       psShared->hGeneralHeap,
                                       PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ | PVRSRV_HAP_NO_GPU_VIRTUAL_ON_ALLOC,
                                       ui32Size,
                                       4,
                                       hPbMem,
                                       &apsVDMControlStreamMemInfo[ui32RenderIndex]);
        fnInfoFailIfError(eResult);
#else
        eResult = PVRSRVAllocDeviceMem(psShared->psSGXDevData,
                                       psShared->hGeneralHeap,
                                       PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ,
                                       ui32Size,
                                       4,
                                       &apsVDMControlStreamMemInfo[ui32RenderIndex]);
#endif
        fnInfoFailIfError(eResult);
        memset(apsVDMControlStreamMemInfo[ui32RenderIndex]->pvLinAddr, 0, ui32Size);
        edbp_NumberedDeviceBuffer3(psEdbpState, "VDMControlStream", ui32GridPosX, ui32GridPosY, ui32RenderIndex, apsVDMControlStreamMemInfo[ui32RenderIndex]);
            

        /**************************************************************************
        * Allocate a static vertex USE code buffer
        **************************************************************************/
        ui32Size = VERTEX_USE_BUFFER_SIZE;
        ui32Align = VERTEX_USE_BUFFER_SIZE;
        fnInfo("Allocate a static vertex USE code buffer (%u)\n", ui32RenderIndex);
#ifdef __psp2__
        ui32Size = ALIGN(ui32Size, SCE_KERNEL_4KiB);

        eResult = PVRSRVAllocDeviceMem(psShared->psSGXDevData,
                                       psShared->hUSEVertexHeap,
                                       psConfig->ui32VertexUSSECodeMemFlags,
                                       ui32Size,
                                       ui32Align,
                                       hPbMem,
                                       &apsVertexUSSEMemInfo[ui32RenderIndex]);
        fnInfoFailIfError(eResult);

        /*eResult = PVRSRVMapMemoryToGpu(psShared->psSGXDevData,
                                       psShared->hDevMemContext,
                                       psShared->hUSEVertexHeap,
                                       ui32Size,
                                       1UL << SGX_USE_CODE_SEGMENT_RANGE_BITS,
                                       apsVertexUSSEMemInfo[ui32RenderIndex]->pvLinAddr,
                                       PVRSRV_MEM_READ,
                                       &apsVertexUSSEMemInfo[ui32RenderIndex]->sDevVAddr.uiAddr);*/
#else
        eResult = PVRSRVAllocDeviceMem(psShared->psSGXDevData,
                                       psShared->hUSEVertexHeap,
                                       psConfig->ui32VertexUSSECodeMemFlags,
                                       ui32Size,
                                       ui32Align,
                                       &apsVertexUSSEMemInfo[ui32RenderIndex]);
#endif
        fnInfoFailIfError(eResult);
        memset(apsVertexUSSEMemInfo[ui32RenderIndex]->pvLinAddr, 0, ui32Size);
        edbp_NumberedDeviceBuffer3(psEdbpState, "VertexUSSE", ui32GridPosX, ui32GridPosY, ui32RenderIndex, apsVertexUSSEMemInfo[ui32RenderIndex]);

        /**************************************************************************
        * Allocate a static fragment USE code buffer
        **************************************************************************/
        ui32Size = FRAGMENT_USE_BUFFER_SIZE;
        ui32Align = FRAGMENT_USE_BUFFER_SIZE;
        fnInfo("Allocate a static fragment USE code buffer (%u)\n", ui32RenderIndex);
#ifdef __psp2__
        ui32Size = ALIGN(ui32Size, SCE_KERNEL_4KiB);

        eResult = PVRSRVAllocDeviceMem(psShared->psSGXDevData,
                                       psShared->hUSEFragmentHeap,
                                       psConfig->ui32PixelUSSECodeMemFlags,
                                       ui32Size,
                                       ui32Align,
                                       hPbMem,
                                       &apsFragmentUSSEMemInfo[ui32RenderIndex]);
        fnInfoFailIfError(eResult);

#else
        eResult = PVRSRVAllocDeviceMem(psShared->psSGXDevData,
                                       psShared->hUSEFragmentHeap,
                                       psConfig->ui32PixelUSSECodeMemFlags,
                                       ui32Size,
                                       ui32Align,
                                       &apsFragmentUSSEMemInfo[ui32RenderIndex]);
#endif
        fnInfoFailIfError(eResult);
        memset(apsFragmentUSSEMemInfo[ui32RenderIndex]->pvLinAddr, 0, ui32Size);
        edbp_NumberedDeviceBuffer3(psEdbpState, "FragmentUSSE", ui32GridPosX, ui32GridPosY, ui32RenderIndex, apsFragmentUSSEMemInfo[ui32RenderIndex]);
    }

    if (psConfig->ui323DParamsHeapReserveFactor > 0)
    {
        /*
            By allocating this memory first, we force the PB itself to be
            allocated higher up the heap.
        */
        fnInfo("Allocate PB reserve memory\n");
#ifdef __psp2__
        eResult = PVRSRVAllocDeviceMem(psShared->psSGXDevData,
                                       psShared->h3DParametersHeap,
                                       PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ | PVRSRV_HAP_NO_GPU_VIRTUAL_ON_ALLOC,
                                       psConfig->ui323DParamsHeapReserveFactor * 4 * psConfig->ui32PBSize,
                                       0,
                                       hPbMem,
                                       &ps3DParamsHeapReservedMemInfo);
        fnInfoFailIfError(eResult);
#else
        eResult = PVRSRVAllocDeviceMem(psShared->psSGXDevData,
                                       psShared->h3DParametersHeap,
                                       PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ,
                                       psConfig->ui323DParamsHeapReserveFactor * 4 * psConfig->ui32PBSize,
                                       0,
                                       &ps3DParamsHeapReservedMemInfo);
#endif
        fnInfoFailIfError(eResult);
        edbp_NumberedDeviceBuffer3(psEdbpState, "3DParamsHeapReserved", ui32GridPosX, ui32GridPosY, 0, ps3DParamsHeapReservedMemInfo);
    }

    /**************************************************************************
    *	Allocate memory for memory flags tests if necessary
    **************************************************************************/
#ifndef __psp2__
    if (psConfig->bUSSEPixelWriteMem)
    {
        eResult = PVRSRVAllocDeviceMem(psShared->psSGXDevData,
                                       psShared->hGeneralHeap,
                                       psConfig->ui32USSEPixelMemFlags,
                                       4, 4,
                                       &psUSSEPixelMemFlagsTestMemInfo);
        fnInfoFailIfError(eResult);
        edbp_NumberedDeviceBuffer3(psEdbpState, "USSEPixelMemFlagsTest", ui32GridPosX, ui32GridPosY, 0, psUSSEPixelMemFlagsTestMemInfo);
    }
    if (psConfig->bUSSEVertexWriteMem)
    {
        eResult = PVRSRVAllocDeviceMem(psShared->psSGXDevData,
                                       psShared->hGeneralHeap,
                                       psConfig->ui32USSEVertexMemFlags,
                                       4, 4,
                                       &psUSSEVertexMemFlagsTestMemInfo);
        fnInfoFailIfError(eResult);
        edbp_NumberedDeviceBuffer3(psEdbpState, "USSEVertexMemFlagsTest", ui32GridPosX, ui32GridPosY, 0, psUSSEVertexMemFlagsTestMemInfo);
    }
#endif

    if (psConfig->ui32PixelShaderHeapReserve > 0)
    {
        PVRSRVFreeDeviceMem(psShared->psSGXDevData, psPixelShaderHeapReservedMemInfo);
    }
    if (psConfig->ui323DParamsHeapReserveFactor > 0)
    {
        PVRSRVFreeDeviceMem(psShared->psSGXDevData, ps3DParamsHeapReservedMemInfo);
    }

    if (psConfig->bExtendedDataBreakpointTest)
    {
        /* While waiting for render complete, we poll for breakpoints
           and service them as they come up */
        pfnCallbackFunc = edbp_HandleBkptCallback;
        pvCallbackArg = (IMG_VOID *)psEdbpState;
    }
    else
    {
        /* if data breakpoint test wasn't required, then we stub out
           the callbacks to get original "wait_for_render" behaviour.
           (Strictly this is not necessary, as the callback would do
           nothing) */
        pfnCallbackFunc = IMG_NULL;
        pvCallbackArg = IMG_NULL;
    }

    sgxut_get_current_time(&ui32StartTimeMS);

    /*
        Main loop.
    */
    for (ui32RenderCount = 0, ui32RenderIndex = 0, ui32FrameNum = psConfig->ui32FrameStart;
         ui32RenderCount < psConfig->ui32MaxIterations;
         ui32RenderCount++, ui32FrameNum++)
    {
        IMG_UINT32	ui32CyclePosition = ui32RenderCount % psConfig->ui32FramesPerRotation;
        IMG_UINT32	ui32NumTrianglesInFrame;

        /*
            Sleep if required.
        */
        if (ui32RenderCount > 0)
        {
            IMG_UINT32 ui32SleepTimeUS = srft_CyclePositionInterpolateInt(
                                                ui32CyclePosition,
                                                psConfig->ui32FramesPerRotation,
                                                psConfig->ui32SleepTimeUS1,
                                                psConfig->ui32SleepTimeUS2);

            if ((ui32SleepTimeUS > 0))
            {
                sgxut_sleep_us(ui32SleepTimeUS);
            }
            if ((psConfig->ui32SleepTimeMS > 0))
            {
                sgxut_sleep_ms(psConfig->ui32SleepTimeMS);
            }
        }

        if (CHECK_FRAME_NUM_FIRST(ui32RenderCount, psConfig->ui32StatusFrequency))
        {
            DPF("%sFrame number %u\n", aui8Preamble, ui32FrameNum);
        }

        ui32NumTrianglesInFrame = srft_CyclePositionInterpolateInt(
                                    ui32CyclePosition,
                                    psConfig->ui32FramesPerRotation,
                                    psConfig->ui32TrianglesPerFrame1,
                                    psConfig->ui32TrianglesPerFrame2);

        /* Set up render address */
        if (psConfig->bBlit)
        {
            psRenderSurfMemInfo = apsBackBufferMemInfo[ui32BackBufferIdx];	/* PRQA S 505 */ /* not very good at tracking pointers is it */
            ui32RenderAddress = psRenderSurfMemInfo->sDevVAddr.uiAddr;
        }
        else
        {
            if (psConfig->bFlip)
            {
                psRenderSurfMemInfo = psShared->ppsSwapBufferMemInfo[ui32SwapChainIndex];
            }
            else
            {
                psRenderSurfMemInfo = psShared->psSGXSystemBufferMemInfo;
            }

            ui32RenderAddress = psRenderSurfMemInfo->sDevVAddr.uiAddr +
                                 (ui32MainRenderLineStride * ui32RectanglePosY + ui32RectanglePosX) * ui32BytesPerPixel;
        }

        if (!psConfig->bNoRender ||
            (psConfig->bFlip && (ui32RenderCount < psConfig->ui32NumSwapChainBuffers)) ||
            (psConfig->bBlit && (ui32RenderCount < psConfig->ui32NumBackBuffers)) ||
            (ui32RenderCount == 0))
        {
            if (psConfig->bDebugSurfaceAddrs)
            {
                DPF("Render Address: 0x%x\n", ui32RenderAddress);
            }
            
            /* Restore the buffer pointers to their start positions for each iteration */
            pui32FragmentPDSBuffer = apsFragmentPDSMemInfo[ui32RenderIndex]->pvLinAddr;
            pui32FragmentPDSBufferStart = apsFragmentPDSMemInfo[ui32RenderIndex]->pvLinAddr;

            pui32VertexPDSBuffer = apsVertexPDSMemInfo[ui32RenderIndex]->pvLinAddr;
            pui32VertexPDSBufferStart = apsVertexPDSMemInfo[ui32RenderIndex]->pvLinAddr;

            pui32VertexDataBuffer = apsVertexDataMemInfo[ui32RenderIndex]->pvLinAddr;
            pui32VertexDataBufferStart = apsVertexDataMemInfo[ui32RenderIndex]->pvLinAddr;

            pui32VDMControlStreamBuffer = apsVDMControlStreamMemInfo[ui32RenderIndex]->pvLinAddr;
            pui32VDMControlStreamBufferStart = apsVDMControlStreamMemInfo[ui32RenderIndex]->pvLinAddr;

            pui32VertexUSSEBuffer = apsVertexUSSEMemInfo[ui32RenderIndex]->pvLinAddr;
            pui32VertexUSSEBufferStart = apsVertexUSSEMemInfo[ui32RenderIndex]->pvLinAddr;

            pui32FragmentUSSEBuffer = apsFragmentUSSEMemInfo[ui32RenderIndex]->pvLinAddr;
            pui32FragmentUSSEBufferStart = apsFragmentUSSEMemInfo[ui32RenderIndex]->pvLinAddr;

            /*
                Side band word set up.

                Options - EURASIA_PIXELBE1S2_DITHER
                        - EURASIA_PIXELBE1S2_SCALE_AA
            */
#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
            ui32SideBandWord = EURASIA_PIXELBESB_TILERELATIVE;
#else
            ui32SideBandWord = EURASIA_PIXELBE2SB_TILERELATIVE;
#endif

            /**************************************************************************
                Generate End-of-tile program.
            **************************************************************************/
            sUSSEProgDevVAddr.uiAddr = apsFragmentUSSEMemInfo[ui32RenderIndex]->sDevVAddr.uiAddr +
                                        ((IMG_UINT32)(pui32FragmentUSSEBuffer - pui32FragmentUSSEBufferStart) * sizeof(IMG_UINT32));
            ALIGN_USSE_BUFFER_DEVV(sUSSEProgDevVAddr.uiAddr)
            ALIGN_USSE_BUFFER_CPUV(pui32FragmentUSSEBuffer)

            srft_WriteEndOfTileCode(&pui32FragmentUSSEBuffer, &sUSSEProgDevVAddr, psShared->uUSEFragmentHeapBase,
                                    &sPixelEventProgram,
                                    ui32MainRenderLineStride,
                                    ui32SideBandWord, ui32RenderAddress,
                                    CHECK_FRAME_NUM_LAST(ui32RenderCount, psConfig->ui32TestHWRRatePBE),
                                    CHECK_FRAME_NUM_LAST(ui32RenderCount, psConfig->ui32TestHWRRatePS),
                                    psConfig->bDebugProgramAddrs,
                                    psConfig->b2XMSAADontResolve,
                                    psShared);

            /**************************************************************************
                Generate End-of-render program.
            **************************************************************************/
            sUSSEProgDevVAddr.uiAddr = apsFragmentUSSEMemInfo[ui32RenderIndex]->sDevVAddr.uiAddr +
                                            ((IMG_UINT32)(pui32FragmentUSSEBuffer - pui32FragmentUSSEBufferStart) * sizeof(IMG_UINT32));
            ALIGN_USSE_BUFFER_DEVV(sUSSEProgDevVAddr.uiAddr)
            ALIGN_USSE_BUFFER_CPUV(pui32FragmentUSSEBuffer)

            srft_WriteEndOfRenderCode(&pui32FragmentUSSEBuffer, &sUSSEProgDevVAddr, psShared->uUSEFragmentHeapBase,
                                      &sPixelEventProgram, psConfig->bDebugProgramAddrs);

            /**************************************************************************
                Generate PTOFF program.
            **************************************************************************/
            sUSSEProgDevVAddr.uiAddr = apsFragmentUSSEMemInfo[ui32RenderIndex]->sDevVAddr.uiAddr +
                                            ((IMG_UINT32)(pui32FragmentUSSEBuffer - pui32FragmentUSSEBufferStart) * sizeof(IMG_UINT32));
            ALIGN_USSE_BUFFER_DEVV(sUSSEProgDevVAddr.uiAddr)
            ALIGN_USSE_BUFFER_CPUV(pui32FragmentUSSEBuffer)

            srft_WritePTOFFCode(&pui32FragmentUSSEBuffer, &sUSSEProgDevVAddr, psShared->uUSEFragmentHeapBase,
                                    &sPixelEventProgram, psConfig->bDebugProgramAddrs);

            /**************************************************************************
                Generate pixel event PDS program.
            **************************************************************************/
            sPixelEventPDSProgDevVAddr.uiAddr = apsFragmentPDSMemInfo[ui32RenderIndex]->sDevVAddr.uiAddr +
                                                    ((IMG_UINT32)(pui32FragmentPDSBuffer - pui32FragmentPDSBufferStart) * sizeof(IMG_UINT32));
            ALIGN_PDS_BUFFER_DEVV(sPixelEventPDSProgDevVAddr.uiAddr)
            ALIGN_PDS_BUFFER_CPUV(pui32FragmentPDSBuffer)

            srft_WritePDSPixelEventCode(&pui32FragmentPDSBuffer,
                                        &sPixelEventPDSProgDevVAddr,
                                        &sPixelEventProgram, psShared->psSGXInfo,
                                        &ui32StateSize,
                                        psConfig->bDebugProgramAddrs,
                                        psConfig->b2XMSAADontResolve);

            /**************************************************************************
                Set up hardware register values
            **************************************************************************/
            srft_SetupHWRegs(&sHWRegs, sPixelEventPDSProgDevVAddr, &sPixelEventProgram,
                             ui32StateSize);

            /**************************************************************************
                Set up texture state for the background object
            **************************************************************************/
            srft_SetupTextureImageUnit(&sTextureImageUnit,
                                       psShared->ui32FBTexFormat,
                                       ui32MainRenderLineStride,
                                       psShared->ui32RectangleSizeY,
                                       psRenderSurfMemInfo);

            /**************************************************************************
                Generate Primary Pixel program for the background object
            **************************************************************************/
            sUSSEProgDevVAddr.uiAddr = apsFragmentUSSEMemInfo[ui32RenderIndex]->sDevVAddr.uiAddr +
                                              ((IMG_UINT32)(pui32FragmentUSSEBuffer - pui32FragmentUSSEBufferStart) * sizeof(IMG_UINT32));
            ALIGN_USSE_BUFFER_DEVV(sUSSEProgDevVAddr.uiAddr)
            ALIGN_USSE_BUFFER_CPUV(pui32FragmentUSSEBuffer)

            sHWBGObjPDSProgDevVAddr.uiAddr = apsFragmentPDSMemInfo[ui32RenderIndex]->sDevVAddr.uiAddr +
                                              ((IMG_UINT32)(pui32FragmentPDSBuffer - pui32FragmentPDSBufferStart) * sizeof(IMG_UINT32));
            ALIGN_PDS_BUFFER_DEVV(sHWBGObjPDSProgDevVAddr.uiAddr)
            ALIGN_PDS_BUFFER_CPUV(pui32FragmentPDSBuffer)

            srft_WritePrimaryPixelCode(&pui32FragmentUSSEBuffer,
                                       &sUSSEProgDevVAddr,
                                       &pui32FragmentPDSBuffer,
                                       &sHWBGObjPDSProgDevVAddr,
                                       &sHWBGOBJPixelShaderProgram,
                                       psShared->psSGXInfo,
                                       psShared->uUSEFragmentHeapBase,
                                       &sTextureImageUnit,
                                       IMG_FALSE,
                                       IMG_FALSE,
                                       IMG_FALSE, IMG_NULL,
                                       psConfig->bDebugProgramAddrs,
                                       IMG_FALSE,
                                       "Background object");

            /**************************************************************************
                Generate secondary pixel program for background object
            **************************************************************************/
            sUSSEProgDevVAddr.uiAddr = apsFragmentUSSEMemInfo[ui32RenderIndex]->sDevVAddr.uiAddr +
                                            ((IMG_UINT32)(pui32FragmentUSSEBuffer - pui32FragmentUSSEBufferStart) * sizeof(IMG_UINT32));
            ALIGN_USSE_BUFFER_DEVV(sUSSEProgDevVAddr.uiAddr)
            ALIGN_USSE_BUFFER_CPUV(pui32FragmentUSSEBuffer)

            sHWBGObjPixelSecondaryPDSProgDevVAddr.uiAddr = apsFragmentPDSMemInfo[ui32RenderIndex]->sDevVAddr.uiAddr +
                                                        ((IMG_UINT32)(pui32FragmentPDSBuffer - pui32FragmentPDSBufferStart) * sizeof(IMG_UINT32));
            ALIGN_PDS_BUFFER_DEVV(sHWBGObjPixelSecondaryPDSProgDevVAddr.uiAddr)
            ALIGN_PDS_BUFFER_CPUV(pui32FragmentPDSBuffer)

            srft_WriteSecondaryPixelCode(&pui32FragmentUSSEBuffer, &sUSSEProgDevVAddr,
                                         &pui32FragmentPDSBuffer, &sHWBGObjPixelSecondaryPDSProgDevVAddr,
                                         &sPixelShaderSAProgram, psShared->psSGXInfo,
                                         psShared->uUSEFragmentHeapBase,
                                         psConfig->bDebugProgramAddrs);

            /**************************************************************************
                Store background object state for SGX kick later
            **************************************************************************/
            srft_SetupPDSStateWords(&aui32HWBGObjPDSState[0],
                                    &sHWBGObjPixelSecondaryPDSProgDevVAddr,
                                    &sPixelShaderSAProgram,
                                    &sHWBGObjPDSProgDevVAddr,
                                    &sHWBGOBJPixelShaderProgram);

            /**************************************************************************
                Send ISP enable/disable to scissor
            **************************************************************************/
            /* Not currently required */


            /**************************************************************************
                Generate Primary Pixel program
            **************************************************************************/
            sUSSEProgDevVAddr.uiAddr = apsFragmentUSSEMemInfo[ui32RenderIndex]->sDevVAddr.uiAddr +
                                            ((IMG_UINT32)(pui32FragmentUSSEBuffer - pui32FragmentUSSEBufferStart) * sizeof(IMG_UINT32));
            ALIGN_USSE_BUFFER_DEVV(sUSSEProgDevVAddr.uiAddr)
            ALIGN_USSE_BUFFER_CPUV(pui32FragmentUSSEBuffer)

            sMainFragmentPDSProgDevVAddr.uiAddr = apsFragmentPDSMemInfo[ui32RenderIndex]->sDevVAddr.uiAddr +
                                                    ((IMG_UINT32)(pui32FragmentPDSBuffer - pui32FragmentPDSBufferStart) * sizeof(IMG_UINT32));
            ALIGN_PDS_BUFFER_DEVV(sMainFragmentPDSProgDevVAddr.uiAddr)
            ALIGN_PDS_BUFFER_CPUV(pui32FragmentPDSBuffer)

            srft_WritePrimaryPixelCode(&pui32FragmentUSSEBuffer,
                                       &sUSSEProgDevVAddr,
                                       &pui32FragmentPDSBuffer,
                                       &sMainFragmentPDSProgDevVAddr,
                                       &sMainPixelShaderProgram,
                                       psShared->psSGXInfo,
                                       psShared->uUSEFragmentHeapBase,
                                       IMG_NULL,
                                       IMG_TRUE,
                                       psConfig->ui32SlowdownFactor3D,
                                       psConfig->bUSSEPixelWriteMem,
                                       psUSSEPixelMemFlagsTestMemInfo,
                                       psConfig->bDebugProgramAddrs,
                                       (IMG_BOOL)(psConfig->ui32TriangleStyle == 5),
                                       "Triangle");

            /**************************************************************************
                Generate secondary pixel program
            **************************************************************************/
            sUSSEProgDevVAddr.uiAddr = apsFragmentUSSEMemInfo[ui32RenderIndex]->sDevVAddr.uiAddr +
                                            ((IMG_UINT32)(pui32FragmentUSSEBuffer - pui32FragmentUSSEBufferStart) * sizeof(IMG_UINT32));
            ALIGN_USSE_BUFFER_DEVV(sUSSEProgDevVAddr.uiAddr)
            ALIGN_USSE_BUFFER_CPUV(pui32FragmentUSSEBuffer)

            sMainPixelSecondaryPDSProgDevVAddr.uiAddr = apsFragmentPDSMemInfo[ui32RenderIndex]->sDevVAddr.uiAddr +
                                                                    ((IMG_UINT32)(pui32FragmentPDSBuffer - pui32FragmentPDSBufferStart) * sizeof(IMG_UINT32));
            ALIGN_PDS_BUFFER_DEVV(sMainPixelSecondaryPDSProgDevVAddr.uiAddr)
            ALIGN_PDS_BUFFER_CPUV(pui32FragmentPDSBuffer)

            srft_WriteSecondaryPixelCode(&pui32FragmentUSSEBuffer, &sUSSEProgDevVAddr,
                                         &pui32FragmentPDSBuffer, &sMainPixelSecondaryPDSProgDevVAddr,
                                         &sPixelShaderSAProgram, psShared->psSGXInfo,
                                         psShared->uUSEFragmentHeapBase,
                                         psConfig->bDebugProgramAddrs);

            /**************************************************************************
                Store PDS state
            **************************************************************************/
            srft_SetupPDSStateWords(&aui32MainPDSState[0],
                                    &sMainPixelSecondaryPDSProgDevVAddr,
                                    &sPixelShaderSAProgram,
                                    &sMainFragmentPDSProgDevVAddr,
                                    &sMainPixelShaderProgram);

            /**************************************************************************
                PDUMP 3D data buffers
            **************************************************************************/
            srft_PDump3DBuffers(psShared->psConnection,
                                ui32RenderIndex, ui32RenderCount, ui32FrameNum,
                                apsFragmentUSSEMemInfo, pui32FragmentUSSEBuffer, pui32FragmentUSSEBufferStart,
                                apsFragmentPDSMemInfo, pui32FragmentPDSBuffer, pui32FragmentPDSBufferStart);

            srft_DrawTriangles(psShared, psConfig,
                               apsVertexDataMemInfo[ui32RenderIndex],
                               &pui32VertexDataBuffer, pui32VertexDataBufferStart,
                               apsVertexPDSMemInfo[ui32RenderIndex],
                               &pui32VertexPDSBuffer, pui32VertexPDSBufferStart,
                               apsVertexUSSEMemInfo[ui32RenderIndex],
                               &pui32VertexUSSEBuffer, pui32VertexUSSEBufferStart,
                               apsVDMControlStreamMemInfo[ui32RenderIndex],
                               &pui32VDMControlStreamBuffer, pui32VDMControlStreamBufferStart,
                               &aui32HWBGObjPDSState[0],
                               &aui32MainPDSState[0],
                               &sHWRegs,
                               psUSSEVertexMemFlagsTestMemInfo,
                               psIndexMemInfo,
                               psRenderSurfMemInfo,
                               hRTDataSet,
                               ui32FrameNum,
                               ui32CyclePosition, ui32NumTrianglesInFrame, ui32RenderCount);
        }

        if (CHECK_FRAME_NUM_LAST(ui32RenderCount, psConfig->ui32SerialiseFrequency) ||
            CHECK_FRAME_NUM_LAST(ui32RenderCount, psConfig->ui32BitmapFrequency))
        {
            srft_wait_for_render(psShared->psConnection,
                                 psConfig->bBlit, psConfig->ui32NumBackBuffers, apsBackBufferMemInfo,
                                 psConfig->bFlip, psConfig->ui32NumSwapChainBuffers, psShared->ppsSwapBufferMemInfo,
                                 psShared->psSGXSystemBufferMemInfo,
                                 psShared->hOSGlobalEvent, psConfig->bVerbose,
                                 pfnCallbackFunc, pvCallbackArg);
        }
        else
        {
            /* even if we're not on the last frame, we should use this opportunity to process any breakpoints that have hit so far..
               ... TODO FIXME: this should be done in a separate thread, in order to allow other threads to continue trying to render */
            if (psConfig->bExtendedDataBreakpointTest)
            {
                edbp_HandleBkpt(psEdbpState);
            }
        }
                

        if (CHECK_FRAME_NUM_LAST(ui32RenderCount, psConfig->ui32BitmapFrequency))
        {
            #if defined(PDUMP)
            srft_DumpBitmap(psShared->psSGXDevData, ui32RenderAddress,
                            psShared->hDevMemContext,
                            psShared->ui32RectangleSizeX, psShared->ui32RectangleSizeY,
                            ui32MainRenderLineStride * psShared->ui32BytesPerPixel,
                            psShared->eDumpFormat,
                            psConfig->szBitmapFileName,
                            psConfig->bBlit ? "o" :		/* off-screen */
                                psConfig->bFlip ? "f" : /* flip buffer */
                                    "r", 				/* front buffer */
                            ui32RenderCount);
            #endif /* PDUMP */
        }

        if (psConfig->bBlit)
        {
            PVRSRV_CLIENT_MEM_INFO		*psSrcMemInfo, *psDestMemInfo;

            psSrcMemInfo = psRenderSurfMemInfo;

            psDestMemInfo = psConfig->bFlip ?
                                psShared->ppsSwapBufferMemInfo[ui32SwapChainIndex] :
                                psShared->psSGXSystemBufferMemInfo;

            if (psConfig->bSoftwareBlit)
            {
                IMG_UINT32	ui32Line;
                IMG_UINT8	*pui8Dest = psDestMemInfo->pvLinAddr;
                IMG_UINT8	*pui8Src = psSrcMemInfo->pvLinAddr;

                pui8Dest += (ui32RectanglePosX + ui32RectanglePosY * psShared->psPrimDims->ui32Width) * ui32BytesPerPixel;

                for (ui32Line = 0;
                     ui32Line < psShared->ui32RectangleSizeY;
                     ui32Line++)
                {
                    memcpy(pui8Dest, pui8Src, psShared->ui32RectangleSizeX * ui32BytesPerPixel);
                    pui8Dest += psShared->psPrimDims->ui32ByteStride;
                    pui8Src += psShared->ui32RectangleSizeX * ui32BytesPerPixel;
                }
            }
            else
            {
                srft_PresentBlit(psConfig, psShared,
                                 psSrcMemInfo,
                                 psShared->ui32RectangleSizeX,
                                 psShared->ui32RectangleSizeY,
                                 psShared->ui32RectangleSizeX * ui32BytesPerPixel,
                                 psDestMemInfo,
                                 psShared->psPrimDims->ui32Width,
                                 psShared->psPrimDims->ui32Height,
                                 psShared->psPrimDims->ui32ByteStride,
                                 psConfig->bNoSyncObject,
                                 ui32RectanglePosX, ui32RectanglePosY,
                                 CHECK_FRAME_NUM_LAST(ui32RenderCount, psConfig->ui32TestHWRRateBlit));

                if (CHECK_FRAME_NUM_LAST(ui32RenderCount, psConfig->ui32SerialiseFrequency) ||
                    CHECK_FRAME_NUM_LAST(ui32RenderCount, psConfig->ui32BitmapFrequency))
                {
                    srft_wait_for_render(psShared->psConnection,
                                         IMG_FALSE, 0, IMG_NULL,
                                         IMG_FALSE, 0, IMG_NULL,
                                         psDestMemInfo,
                                         psShared->hOSGlobalEvent, psConfig->bVerbose,
                                         pfnCallbackFunc, pvCallbackArg);
                }
                else
                {
                    /* even if we're not on the last frame, we should use this opportunity to process any breakpoints that have hit so far..
                       ... TODO FIXME: this should be done in a separate thread, in order to allow other threads to continue trying to render */
                    if (psConfig->bExtendedDataBreakpointTest)
                    {
                        edbp_HandleBkpt(psEdbpState);
                    }
                }

                if (CHECK_FRAME_NUM_LAST(ui32RenderCount, psConfig->ui32BitmapFrequency))
                {
                    #if defined(PDUMP)
                    srft_DumpBitmap(psShared->psSGXDevData, psDestMemInfo->sDevVAddr.uiAddr,
                                    psShared->hDevMemContext,
                                    psShared->psPrimDims->ui32Width, psShared->psPrimDims->ui32Height,
                                    psShared->psPrimDims->ui32ByteStride,
                                    psShared->eDumpFormat,
                                    psConfig->szBitmapFileName,
                                    psConfig->b2DBlit ? "pp" : "pt", /* Present-PTLA : Present-3D */
                                    ui32RenderCount);
                    #endif /* PDUMP */
                }

                ui32BackBufferIdx++;
                if (ui32BackBufferIdx >= psConfig->ui32NumBackBuffers)
                {
                    ui32BackBufferIdx = 0;
                }
            }
        }
        
        /*
         * Snapshot the current state of the sync object on the destination
         * surface, prior to issuing a flip (if applicable), in order to flush
         * to this point before destroying the buffers upon which SGX rendering
         * is dependent.
         */
        psFlushSurfMemInfo = psConfig->bFlip ?
                                psShared->ppsSwapBufferMemInfo[ui32SwapChainIndex] :
                                psShared->psSGXSystemBufferMemInfo;

        /*TODO eResult = PVRSRVSyncOpsTakeToken(psShared->psConnection,
                                        #if defined (SUPPORT_SID_INTERFACE)
                                         psFlushSurfMemInfo->psClientSyncInfo->hKernelSyncInfo,
                                        #else
                                         psFlushSurfMemInfo->psClientSyncInfo,
                                        #endif
                                         &sSyncToken);*/
        
        if (psConfig->bFlip)
        {
            /* Flip to buffer */

            eResult = PVRSRVSwapToDCBuffer(psShared->hDisplayDevice,
                                           psShared->phSwapChainBuffers[ui32SwapChainIndex],
                                           0,
                                           IMG_NULL,
                                           psConfig->ui32SwapInterval,
#if defined (SUPPORT_SID_INTERFACE)
                                           0);
#else
                                           IMG_NULL);
#endif

            sgxut_fail_if_error_quiet(eResult);

            ui32SwapChainIndex++;
            if (ui32SwapChainIndex >= psConfig->ui32NumSwapChainBuffers)
            {
                ui32SwapChainIndex = 0;
            }

            if (CHECK_FRAME_NUM_LAST(ui32RenderCount, psConfig->ui32SerialiseFrequency) ||
                CHECK_FRAME_NUM_LAST(ui32RenderCount, psConfig->ui32BitmapFrequency))
            {
                srft_wait_for_render(psShared->psConnection,
                                     IMG_FALSE, 0, IMG_NULL,
                                     psConfig->bFlip, psConfig->ui32NumSwapChainBuffers, psShared->ppsSwapBufferMemInfo,
                                     IMG_NULL,
                                     psShared->hOSGlobalEvent, psConfig->bVerbose,
                                     pfnCallbackFunc, pvCallbackArg);
            }
            else
            {
                /* even if we're not on the last frame, we should use this opportunity to process any breakpoints that have hit so far..
                   ... TODO FIXME: this should be done in a separate thread, in order to allow other threads to continue trying to render */
                if (psConfig->bExtendedDataBreakpointTest)
                {
                    edbp_HandleBkpt(psEdbpState);
                }
            }
        }

        if (CHECK_FRAME_NUM_LAST(ui32RenderCount, psConfig->ui32PauseFrequency))
        {
            DPF("%s\n", psConfig->szPauseMessage);
            getchar();		/* PRQA S 4130 */ /* override MS macro message */
        }

        /* The next render will require a free set of buffers */
        ui32RenderIndex++;
        if (ui32RenderIndex == RENDER_ARRAY_SIZE)
        {
            ui32RenderIndex = 0;
        }
    }

    if (!psConfig->bNoWait && psFlushSurfMemInfo != IMG_NULL)
    {
        /*
         * Flush out the sync objects on the render surface(s).
         */
        sgxut_wait_for_token(psShared->psConnection,
                             psFlushSurfMemInfo,
                             &sSyncToken,
                             200,
                             "Final flush", 0,
                             psShared->hOSGlobalEvent, psConfig->bVerbose);
    }

    sgxut_get_current_time(&ui32EndTimeMS);

#if defined(PDUMP)
    PVRSRVPDumpComment(psShared->psConnection, "sgx_render_flip_test geometry complete. Advancing pdump frame for cleanup", IMG_TRUE);
    PVRSRVPDumpSetFrame(psShared->psConnection, ui32FrameNum);
#endif

    /**************************************************************************
    *	Free memory for memory flags tests if necessary
    **************************************************************************/
    if (psConfig->bUSSEPixelWriteMem)
    {
        PVRSRVFreeDeviceMem(psShared->psSGXDevData, psUSSEPixelMemFlagsTestMemInfo);
    }
    if (psConfig->bUSSEVertexWriteMem)
    {
        PVRSRVFreeDeviceMem(psShared->psSGXDevData, psUSSEVertexMemFlagsTestMemInfo);
    }

    /**************************************************************************
    * Remove render target
    **************************************************************************/
    fnInfo("Attempt to remove render target:\n");
    eResult = SGXRemoveRenderTarget(psShared->psSGXDevData,
                                    psShared->hRenderContext,
                                    hRTDataSet);
    fnInfoFailIfError(eResult);

    /**************************************************************************
    * Free all allocations
    **************************************************************************/

    PVRSRVFreeDeviceMem(psShared->psSGXDevData, psIndexMemInfo);

    for (ui32RenderIndex = 0; ui32RenderIndex < RENDER_ARRAY_SIZE; ui32RenderIndex++)
    {
        PVRSRVFreeDeviceMem(psShared->psSGXDevData, apsFragmentUSSEMemInfo[ui32RenderIndex]);
        PVRSRVFreeDeviceMem(psShared->psSGXDevData, apsVertexUSSEMemInfo[ui32RenderIndex]);
        PVRSRVFreeDeviceMem(psShared->psSGXDevData, apsVDMControlStreamMemInfo[ui32RenderIndex]);
        PVRSRVFreeDeviceMem(psShared->psSGXDevData, apsVertexDataMemInfo[ui32RenderIndex]);
        PVRSRVFreeDeviceMem(psShared->psSGXDevData, apsVertexPDSMemInfo[ui32RenderIndex]);
        PVRSRVFreeDeviceMem(psShared->psSGXDevData, apsFragmentPDSMemInfo[ui32RenderIndex]);
    }

    if (psConfig->bBlit)
    {
        /**************************************************************************
        * Free back buffers
        **************************************************************************/
        for(i = 0; i < psConfig->ui32NumBackBuffers; i++)
        {
            PVRSRVFreeDeviceMem(psShared->psSGXDevData, apsBackBufferMemInfo[i]);
        }

        free(apsBackBufferMemInfo);
    }

    /*
        Output the timing data.
    */
    {
        IMG_UINT32 ui32ElapsedTimeMS = ui32EndTimeMS - ui32StartTimeMS;

        printf("\n");
        printf("%sTotal time: %ums\n", aui8Preamble, ui32ElapsedTimeMS);
        if (psConfig->ui32MaxIterations > 0)
        {
            printf("%sMean time per frame: %uus\n", aui8Preamble,
                   1000 * ui32ElapsedTimeMS / psConfig->ui32MaxIterations);
        }
    }

    return IMG_NULL;
}


static IMG_VOID srft_ParseColour(IMG_CHAR		*szStr,
                                 SRFT_COLOUR	*psColour)
{
    IMG_UINT32	ui32ArgsMatched;
    IMG_UINT32	ui32Denominator, ui32RedNumerator, ui32GreenNumerator, ui32BlueNumerator;

    ui32ArgsMatched = sscanf(szStr, "%u/%u/%u/%u",
                             &ui32Denominator, &ui32RedNumerator, &ui32GreenNumerator, &ui32BlueNumerator);
    if (ui32ArgsMatched != 4)
    {
        DPF("Error in colour format '%s' - please use 'D/R/G/B' where "
            "D is the denimator and R,G,B are numerators\n", szStr);
        DPF("For example 4/3/1/0 for three-quarters red, one-quarter green and no blue\n");
        SGXUT_ERROR_EXIT();
    }
    psColour->fRed = (IMG_FLOAT)ui32RedNumerator / (IMG_FLOAT) ui32Denominator;
    psColour->fGreen = (IMG_FLOAT)ui32GreenNumerator / (IMG_FLOAT) ui32Denominator;
    psColour->fBlue = (IMG_FLOAT)ui32BlueNumerator / (IMG_FLOAT) ui32Denominator;
}


static IMG_VOID srft_ParseOptions(IMG_INT		argc,
                                  IMG_CHAR		**argv,
                                  SRFT_CONFIG	*psConfig)
{
    IMG_INT32	ui32ArgCounter;
    IMG_UINT32	ui32NumBuffers = 0;
    IMG_BOOL	bSleepTimeUS2Specified = IMG_FALSE;
    IMG_BOOL	bTrianglesPerFrame2Specified = IMG_FALSE;
    IMG_BOOL	bVertexColour2Specified = IMG_FALSE;
    IMG_BOOL	bVertexColour3Specified = IMG_FALSE;
    IMG_BOOL	bBGColourSpecified = IMG_FALSE;

    psConfig->bVerbose = IMG_FALSE;
    psConfig->bOutputPreamble = IMG_FALSE;
    psConfig->bDebugBreak = IMG_FALSE;
    psConfig->bDebugProgramAddrs = IMG_FALSE;
    psConfig->bDebugSurfaceAddrs = IMG_FALSE;
    psConfig->bCrashTest = IMG_FALSE;
    psConfig->bGetMiscInfo = IMG_FALSE;
    psConfig->ui32TriangleStyle = 1;
    psConfig->ui32TrianglesPerFrame1 = 4;
    psConfig->ui32SwapInterval = 1;
    psConfig->ui32MaxIterations = 1000;
    psConfig->ui32FramesPerRotation = 200;
    psConfig->ui32NumBackBuffers = 3;
    psConfig->ui32NumSwapChainBuffers = 3;
    #if defined(EMULATOR)
    psConfig->ui32StatusFrequency = 1;
    #else	
    psConfig->ui32StatusFrequency = 100;
    #endif /* EMULATOR */
    psConfig->bMultipleTAKicks = IMG_FALSE;
    psConfig->ui32FrameStart = 0;
    psConfig->ui32PBSize = 1 * 1024 * 1024; /* Default 1MB */
    psConfig->ui32SleepTimeMS = 0;
    psConfig->ui32KickSleepTimeMS = 0;
    psConfig->ui32SleepTimeUS1 = 0;
    psConfig->ui32SleepTimeUS2 = 0;
    psConfig->ui32SerialiseFrequency = 0;
    psConfig->ui32BitmapFrequency = 0;
    psConfig->szBitmapFileName = "srft"; /* Would be better to use argv[0], but note the filename size constraint. */
    psConfig->ui32PauseFrequency = 0;
    psConfig->szPauseMessage = "Press ENTER...";
    psConfig->sVertexColour1 = SRFT_RED;
    psConfig->sVertexColour2 = SRFT_WHITE;
    psConfig->sVertexColour3 = SRFT_WHITE;
    psConfig->sBGColour = SRFT_WHITE;
    psConfig->bNoEventObject = IMG_FALSE;
    psConfig->ui32TestHWRRateVDM = 0;
    psConfig->ui32TestHWRRateVS = 0;
    psConfig->ui32TestHWRRatePBE = 0;
    psConfig->ui32TestHWRRatePS = 0;
    psConfig->ui32TestHWRRateBlit = 0;
    psConfig->bNoWait = IMG_FALSE;
    psConfig->bNoCleanup = IMG_FALSE;
    psConfig->bFlip = IMG_TRUE;
    psConfig->bBlit = IMG_FALSE;
    psConfig->b2DBlit = IMG_FALSE;
    psConfig->bSoftwareBlit = IMG_FALSE;
    psConfig->bNoTransfer = IMG_FALSE;
    psConfig->bNoRender = IMG_FALSE;
    psConfig->ui32MTGridSizeX = 1;
    psConfig->ui32MTGridSizeY = 1;
    psConfig->ui32MPGridSizeX = 1;
    psConfig->ui32MPGridSizeY = 1;
    psConfig->ui32MPGridPosX = 0;
    psConfig->ui32MPGridPosY = 0;
    psConfig->ui32PBEMemFlags = PVRSRV_MEM_READ | PVRSRV_MEM_WRITE;
    psConfig->ui32PDSVertexMemFlags = PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ;
    psConfig->ui32PDSPixelMemFlags = PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ;
    psConfig->ui32PDSPixelCodeMemFlags = PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ;
    psConfig->ui32PDSVertexCodeMemFlags = PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ;
    psConfig->bUSSEPixelWriteMem = IMG_FALSE;
    psConfig->ui32USSEPixelMemFlags = 0;
    psConfig->bUSSEVertexWriteMem = IMG_FALSE;
    psConfig->ui32USSEVertexMemFlags = 0;
    psConfig->ui32VertexUSSECodeMemFlags = PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ;
    psConfig->ui32PixelUSSECodeMemFlags = PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ;
    psConfig->ui32SlowdownFactorTA = 0;
    psConfig->ui32SlowdownFactor3D = 0;
    psConfig->ui32RendersPerFrame = 0;
    psConfig->ui323DParamsHeapReserveFactor = 0;
    psConfig->ui32PixelShaderHeapReserve = 0;
    psConfig->eRCPriority = SGX_CONTEXT_PRIORITY_LOW;
    psConfig->bNoSyncObject = IMG_FALSE;
    psConfig->bReadMem = IMG_FALSE;
    psConfig->bExtendedDataBreakpointTest = IMG_FALSE;
#if (defined(SUPPORT_HYBRID_PB) || defined(SUPPORT_SHARED_PB))
    psConfig->bPerContextPB = IMG_FALSE;
#else
    psConfig->bPerContextPB = IMG_TRUE;
#endif
    psConfig->ui32MaxPBSize = 0;
    psConfig->pszExtendedDataBreakpointTestConfigFilename = IMG_NULL;
    psConfig->b2XMSAADontResolve = IMG_FALSE;

#ifdef __psp2__

    psConfig->bVerbose = IMG_TRUE;
    psConfig->bOutputPreamble = IMG_FALSE;
    psConfig->bDebugBreak = IMG_FALSE;
    psConfig->bDebugProgramAddrs = IMG_FALSE;
    psConfig->bDebugSurfaceAddrs = IMG_FALSE;
    psConfig->bCrashTest = IMG_FALSE;
    psConfig->bGetMiscInfo = IMG_FALSE;
    psConfig->ui32TriangleStyle = 1;
    psConfig->ui32TrianglesPerFrame1 = 4;
    psConfig->ui32SwapInterval = 1;
    psConfig->ui32MaxIterations = 1000;
    psConfig->ui32FramesPerRotation = 200;
    psConfig->ui32NumBackBuffers = 2;
    psConfig->ui32NumSwapChainBuffers = 2;
    psConfig->ui32StatusFrequency = 100;
    psConfig->bMultipleTAKicks = IMG_FALSE;
    psConfig->ui32FrameStart = 0;
    psConfig->ui32PBSize = 4 * 1024 * 1024;
    psConfig->ui32SleepTimeMS = 0;
    psConfig->ui32KickSleepTimeMS = 0;
    psConfig->ui32SleepTimeUS1 = 0;
    psConfig->ui32SleepTimeUS2 = 0;
    psConfig->ui32SerialiseFrequency = 0;
    psConfig->ui32BitmapFrequency = 0;
    psConfig->szBitmapFileName = "srft"; /* Would be better to use argv[0], but note the filename size constraint. */
    psConfig->ui32PauseFrequency = 0;
    psConfig->szPauseMessage = "Press ENTER...";
    psConfig->sVertexColour1 = SRFT_RED;
    psConfig->sVertexColour2 = SRFT_WHITE;
    psConfig->sVertexColour3 = SRFT_WHITE;
    psConfig->sBGColour = SRFT_WHITE;
    psConfig->bNoEventObject = IMG_FALSE;
    psConfig->ui32TestHWRRateVDM = 0;
    psConfig->ui32TestHWRRateVS = 0;
    psConfig->ui32TestHWRRatePBE = 0;
    psConfig->ui32TestHWRRatePS = 0;
    psConfig->ui32TestHWRRateBlit = 0;
    psConfig->bNoWait = IMG_FALSE;
    psConfig->bNoCleanup = IMG_FALSE;
    psConfig->bFlip = IMG_TRUE;
    psConfig->bBlit = IMG_FALSE;
    psConfig->b2DBlit = IMG_FALSE;
    psConfig->bSoftwareBlit = IMG_FALSE;
    psConfig->bNoTransfer = IMG_TRUE;
    psConfig->bNoRender = IMG_FALSE;
    psConfig->ui32MTGridSizeX = 1;
    psConfig->ui32MTGridSizeY = 1;
    psConfig->ui32MPGridSizeX = 1;
    psConfig->ui32MPGridSizeY = 1;
    psConfig->ui32MPGridPosX = 0;
    psConfig->ui32MPGridPosY = 0;
    psConfig->ui32PBEMemFlags = PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_HAP_NO_GPU_VIRTUAL_ON_ALLOC;
    psConfig->ui32PDSVertexMemFlags = PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ | PVRSRV_HAP_NO_GPU_VIRTUAL_ON_ALLOC;
    psConfig->ui32PDSPixelMemFlags = PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ | PVRSRV_HAP_NO_GPU_VIRTUAL_ON_ALLOC;
    psConfig->ui32PDSPixelCodeMemFlags = PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ | PVRSRV_HAP_NO_GPU_VIRTUAL_ON_ALLOC;
    psConfig->ui32PDSVertexCodeMemFlags = PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ | PVRSRV_HAP_NO_GPU_VIRTUAL_ON_ALLOC;
    psConfig->bUSSEPixelWriteMem = IMG_FALSE;
    psConfig->ui32USSEPixelMemFlags = 0;
    psConfig->bUSSEVertexWriteMem = IMG_FALSE;
    psConfig->ui32USSEVertexMemFlags = 0;
    psConfig->ui32VertexUSSECodeMemFlags = PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ | PVRSRV_HAP_NO_GPU_VIRTUAL_ON_ALLOC;
    psConfig->ui32PixelUSSECodeMemFlags = PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ | PVRSRV_HAP_NO_GPU_VIRTUAL_ON_ALLOC;
    psConfig->ui32SlowdownFactorTA = 0;
    psConfig->ui32SlowdownFactor3D = 0;
    psConfig->ui32RendersPerFrame = 1;
    psConfig->ui323DParamsHeapReserveFactor = 0;
    psConfig->ui32PixelShaderHeapReserve = 0;
    psConfig->eRCPriority = SGX_CONTEXT_PRIORITY_LOW;
    psConfig->bNoSyncObject = IMG_FALSE;
    psConfig->bReadMem = IMG_FALSE;
    psConfig->bExtendedDataBreakpointTest = IMG_FALSE;
#if (defined(SUPPORT_HYBRID_PB) || defined(SUPPORT_SHARED_PB))
    psConfig->bPerContextPB = IMG_FALSE;
#else
    psConfig->bPerContextPB = IMG_TRUE;
#endif
    psConfig->ui32MaxPBSize = 0;
    psConfig->pszExtendedDataBreakpointTestConfigFilename = IMG_NULL;
    psConfig->b2XMSAADontResolve = IMG_FALSE;

#endif

    for (ui32ArgCounter = 1; ui32ArgCounter < argc; ui32ArgCounter++)
    {
        SRFT_COMMANDLINE_OPTION_VALUE	eCommandLineOption;

        sgxut_parse_commandline_option(argv[0], argv[ui32ArgCounter],
                                       &gasCommandLineOptions[0],
                                       SRFT_COMMANDLINE_NUMOPTIONS,
                                       (IMG_UINT32 *)&eCommandLineOption);

        switch (eCommandLineOption)
        {

        case SRFT_COMMANDLINE_OPTION_SWAPINTERVAL:
        {
            ui32ArgCounter++;
            psConfig->ui32SwapInterval = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            DPF("Swap Interval: %u\n", psConfig->ui32SwapInterval);
            if (psConfig->ui32SwapInterval >= 25)
            {
                DPF("Swap interval should be below 25 to avoid internal timeouts\n");
                SGXUT_ERROR_EXIT();
            }
            break;
        }
        case SRFT_COMMANDLINE_OPTION_FRAMES:
        {
            ui32ArgCounter++;
            psConfig->ui32MaxIterations = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            DPF("Number of frames: %u\n", psConfig->ui32MaxIterations);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_FRAME_START:
        {
            ui32ArgCounter++;
            psConfig->ui32FrameStart = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            DPF("Starting frame: %u\n", psConfig->ui32FrameStart);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_PBSIZE:
        {
            ui32ArgCounter++;
            psConfig->ui32PBSize = 1024UL * (IMG_UINT32)atol(argv[ui32ArgCounter]);
            DPF("Parameter Buffer size: 0x%x Bytes\n", psConfig->ui32PBSize);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_SLEEP:
        {
            ui32ArgCounter++;
            psConfig->ui32SleepTimeMS = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            DPF("Sleep between renders: %ums\n", psConfig->ui32SleepTimeMS);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_KICKSLEEP:
        {
            ui32ArgCounter++;
            psConfig->ui32KickSleepTimeMS = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            DPF("Sleep between TA kicks: %ums\n", psConfig->ui32SleepTimeMS);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_USLEEP1:
        {
            ui32ArgCounter++;
            psConfig->ui32SleepTimeUS1 = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            DPF("Microsecond sleep between kicks (1): %u\n", psConfig->ui32SleepTimeUS1);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_USLEEP2:
        {
            ui32ArgCounter++;
            bSleepTimeUS2Specified = IMG_TRUE;
            psConfig->ui32SleepTimeUS2 = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            DPF("Microsecond sleep between kicks (2): %u\n", psConfig->ui32SleepTimeUS2);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_SERIALISE:
        {
            ui32ArgCounter++;
            psConfig->ui32SerialiseFrequency = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            DPF("Serialise every %u frames\n", psConfig->ui32SerialiseFrequency);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_BITMAP:
        {
            ui32ArgCounter++;
            psConfig->ui32BitmapFrequency = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            DPF("Bitmap every %u frames\n", psConfig->ui32BitmapFrequency);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_BITMAP_FILENAME:
        {
            ui32ArgCounter++;
            psConfig->szBitmapFileName = argv[ui32ArgCounter];
            DPF("(%s) <- Bitmap file name\n", psConfig->szBitmapFileName);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_PAUSE:
        {
            ui32ArgCounter++;
            psConfig->ui32PauseFrequency = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            DPF("Pause every %u frames\n", psConfig->ui32PauseFrequency);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_PAUSE_MESSAGE:
        {
            ui32ArgCounter++;
            psConfig->szPauseMessage = argv[ui32ArgCounter];
            DPF("(%s) <- Pause message\n", psConfig->szPauseMessage);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_TRIANGLESTYLE:
        {
            ui32ArgCounter++;
            psConfig->ui32TriangleStyle = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            if (psConfig->ui32TriangleStyle > 5)
            {
                DPF("Invalid triangle style: %u\n", psConfig->ui32TriangleStyle);
                SGXUT_ERROR_EXIT();
            }

            DPF("Triangle style: %u\n", psConfig->ui32TriangleStyle);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_TRIANGLESPERFRAME:
        {
            ui32ArgCounter++;
            psConfig->ui32TrianglesPerFrame1 = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            DPF("Number of triangles per frame: %u\n", psConfig->ui32TrianglesPerFrame1);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_TRIANGLESPERFRAME2:
        {
            ui32ArgCounter++;
            psConfig->ui32TrianglesPerFrame2 = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            bTrianglesPerFrame2Specified = IMG_TRUE;
            DPF("Number of triangles per frame (2): %u\n", psConfig->ui32TrianglesPerFrame2);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_VERTEXCOLOUR:
        {
            ui32ArgCounter++;
            srft_ParseColour(argv[ui32ArgCounter], &psConfig->sVertexColour1);
            DPF("Vertex Colour: R:%f G:%f B:%f\n", psConfig->sVertexColour1.fRed, psConfig->sVertexColour1.fGreen, psConfig->sVertexColour1.fBlue);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_VERTEXCOLOUR2:
        {
            ui32ArgCounter++;
            srft_ParseColour(argv[ui32ArgCounter], &psConfig->sVertexColour2);
            bVertexColour2Specified = IMG_TRUE;
            DPF("Vertex Colour (2): R:%f G:%f B:%f\n", psConfig->sVertexColour2.fRed, psConfig->sVertexColour2.fGreen, psConfig->sVertexColour2.fBlue);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_VERTEXCOLOUR3:
        {
            ui32ArgCounter++;
            srft_ParseColour(argv[ui32ArgCounter], &psConfig->sVertexColour3);
            bVertexColour3Specified = IMG_TRUE;
            DPF("Vertex Colour (3): R:%f G:%f B:%f\n", psConfig->sVertexColour3.fRed, psConfig->sVertexColour3.fGreen, psConfig->sVertexColour3.fBlue);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_BACKGROUNDCOLOUR:
        {
            ui32ArgCounter++;
            srft_ParseColour(argv[ui32ArgCounter], &psConfig->sBGColour);
            bBGColourSpecified = IMG_TRUE;
            DPF("Background Colour: R:%f G:%f B:%f\n", psConfig->sBGColour.fRed, psConfig->sBGColour.fGreen, psConfig->sBGColour.fBlue);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_NUMBUFFERS:
        {
            ui32ArgCounter++;
            ui32NumBuffers = (IMG_UINT32)strtol(argv[ui32ArgCounter], IMG_NULL, 0);
            DPF("Number of buffers: 0x%x\n", ui32NumBuffers);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_FRAMESPERROTATION:
        {
            ui32ArgCounter++;
            psConfig->ui32FramesPerRotation = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            DPF("Number of frames per triangle rotation: %u\n", psConfig->ui32FramesPerRotation);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_NOEVENTOBJECT:
        {
            psConfig->bNoEventObject = IMG_TRUE;
            DPF("Event Object: DISABLED\n");
            break;
        }
        case SRFT_COMMANDLINE_OPTION_HWR:
        {
            IMG_UINT32	*pui32TestHWRRate;
            IMG_CHAR	*szHWRType;

            ui32ArgCounter++;
            if (strcmp(argv[ui32ArgCounter], "vdm") == 0)
            {
                szHWRType = "VDM";
                pui32TestHWRRate = &psConfig->ui32TestHWRRateVDM;
            }
            else if (strcmp(argv[ui32ArgCounter], "vs") == 0)
            {
                szHWRType = "Vertex Shader";
                pui32TestHWRRate = &psConfig->ui32TestHWRRateVS;
            }
            else if (strcmp(argv[ui32ArgCounter], "ps") == 0)
            {
                szHWRType = "Pixel Shader";
                pui32TestHWRRate = &psConfig->ui32TestHWRRatePS;
            }
            else if (strcmp(argv[ui32ArgCounter], "pbe") == 0)
            {
                szHWRType = "Pixel Backend";
                pui32TestHWRRate = &psConfig->ui32TestHWRRatePBE;
            }
            else if (strcmp(argv[ui32ArgCounter], "blit") == 0)
            {
                szHWRType = "Blit";
                pui32TestHWRRate = &psConfig->ui32TestHWRRateBlit;
            }
            else
            {
                DPF("Unknown hwr option %s\n", argv[ui32ArgCounter]);
                SGXUT_ERROR_EXIT();
            }

            ui32ArgCounter++;
            *pui32TestHWRRate = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            DPF("Simulated %s lockup every %u kicks\n", szHWRType, *pui32TestHWRRate);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_MULTIPLE_TA_KICKS:
        {
            /*
                This causes all triangles to be submitted as separate TA kicks.
            */
            psConfig->bMultipleTAKicks = IMG_TRUE;
            DPF("Multiple TA kicks: ENABLED\n");
            break;
        }
        case SRFT_COMMANDLINE_OPTION_NOWAIT:
        {
            /*
                Do not wait for sync objects at the end of the test.
            */
            psConfig->bNoWait = IMG_TRUE;
            DPF("Wait for sync ops at end of test: DISABLED\n");
            break;
        }
        case SRFT_COMMANDLINE_OPTION_NOCLEANUP:
        {
            /*
                Do not clean up allocations at the end of the test.
            */
            psConfig->bNoCleanup = IMG_TRUE;
            DPF("Clean up at end of test: DISABLED\n");
            break;
        }
        case SRFT_COMMANDLINE_OPTION_NOFLIP:
        {
            psConfig->bFlip = IMG_FALSE;
            DPF("Flipping: DISABLED\n");
            break;
        }
        case SRFT_COMMANDLINE_OPTION_NOTRANSFER:
        {
            /*
                Suppress the transfer. Only expected to be used when multiple
                instances must run without any serialisation.
            */
            psConfig->bNoTransfer = IMG_TRUE;
            DPF("Transfer: DISABLED\n");
            break;
        }
        case SRFT_COMMANDLINE_OPTION_NORENDER:
        {
            /*
                Suppress the main render.
                The triangle is only drawn for the first few iterations until the buffers
                are populated.
            */
            psConfig->bNoRender = IMG_TRUE;
            DPF("Render: DISABLED\n");
            break;
        }
        case SRFT_COMMANDLINE_OPTION_BLIT:
        {
            psConfig->bBlit = IMG_TRUE;
            DPF("Blitting: ENABLED\n");
            break;
        }
        case SRFT_COMMANDLINE_OPTION_BLIT2D:
        {
#if !defined(SGX_FEATURE_2D_HARDWARE)
            DPF("2D core not supported on this platform\n");
            SGXUT_ERROR_EXIT();
#else
            psConfig->bBlit = IMG_TRUE;
            psConfig->b2DBlit = IMG_TRUE;
            DPF("Blitting (2D core): ENABLED\n");
            break;
#endif /* SGX_FEATURE_2D_HARDWARE */
        }
        case SRFT_COMMANDLINE_OPTION_BLITSOFTWARE:
        {
            psConfig->bBlit = IMG_TRUE;
            psConfig->bSoftwareBlit = IMG_TRUE;
            DPF("Blitting (software): ENABLED\n");
            break;
        }
        case SRFT_COMMANDLINE_OPTION_MULTITHREAD:
        {
            ui32ArgCounter++;
            psConfig->ui32MTGridSizeX = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            if (psConfig->ui32MTGridSizeX < 1)
            {
                DPF("Invalid grid MT size X: %u\n", psConfig->ui32MTGridSizeX);
                SGXUT_ERROR_EXIT();
            }

            ui32ArgCounter++;
            psConfig->ui32MTGridSizeY = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            if (psConfig->ui32MTGridSizeY < 1)
            {
                DPF("Invalid MT grid size Y: %u\n", psConfig->ui32MTGridSizeY);
                SGXUT_ERROR_EXIT();
            }

            DPF("Multithread Grid size: %u x %u\n",
                psConfig->ui32MTGridSizeX, psConfig->ui32MTGridSizeY);
            #if !defined(SRFT_SUPPORT_PTHREADS)
            DPF("WARNING: Pthreads not supported on this platform, running serially\n");
            #endif /* SRFT_SUPPORT_PTHREADS */
            break;
        }
        case SRFT_COMMANDLINE_OPTION_GRIDSIZE:
        {
            ui32ArgCounter++;
            psConfig->ui32MPGridSizeX = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            if (psConfig->ui32MPGridSizeX < 1)
            {
                DPF("Invalid grid size X: %u\n", psConfig->ui32MPGridSizeX);
                SGXUT_ERROR_EXIT();
            }

            ui32ArgCounter++;
            psConfig->ui32MPGridSizeY = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            if (psConfig->ui32MPGridSizeY < 1)
            {
                DPF("Invalid grid size Y: %u\n", psConfig->ui32MPGridSizeY);
                SGXUT_ERROR_EXIT();
            }

            DPF("Grid size: %u x %u\n",
                psConfig->ui32MPGridSizeX, psConfig->ui32MPGridSizeY);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_GRIDPOSITION:
        {
            ui32ArgCounter++;
            psConfig->ui32MPGridPosX = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            if (psConfig->ui32MPGridPosX >= psConfig->ui32MPGridSizeX)
            {
                DPF("Invalid grid pos X: %u\n", psConfig->ui32MPGridPosX);
                SGXUT_ERROR_EXIT();
            }

            ui32ArgCounter++;
            psConfig->ui32MPGridPosY = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            if (psConfig->ui32MPGridPosY >= psConfig->ui32MPGridSizeY)
            {
                DPF("Invalid grid pos Y: %u\n", psConfig->ui32MPGridPosY);
                SGXUT_ERROR_EXIT();
            }

            DPF("Grid pos: (%u, %u)\n",
                psConfig->ui32MPGridPosX, psConfig->ui32MPGridPosY);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_MEMFLAGS:
        {
            IMG_UINT32	*pui32MemFlags;

            /*
                Memory flags test. Read the reqestor and then the flags.
            */
            ui32ArgCounter++;
            if (strcmp(argv[ui32ArgCounter], "pbe") == 0)
            {
                /*
                    Note: only effective if using a present blit option (-b2 or -b)
                */
                pui32MemFlags = &psConfig->ui32PBEMemFlags;
                DPF("Memory flags test (PBE): ");
            }
            else if (strcmp(argv[ui32ArgCounter], "pdsp") == 0)
            {
                pui32MemFlags = &psConfig->ui32PDSPixelMemFlags;	/* PRQA S 3199 */ /* but it is used */
                DPF("Memory flags test (PDS Pixel): ");
                DPF("Not currently supported");
                SGXUT_ERROR_EXIT();
            }
            else if (strcmp(argv[ui32ArgCounter], "pdspc") == 0)
            {
                pui32MemFlags = &psConfig->ui32PDSPixelCodeMemFlags;
                DPF("Memory flags test (PDS Pixel Code): ");
            }
            else if (strcmp(argv[ui32ArgCounter], "pdsv") == 0)
            {
                pui32MemFlags = &psConfig->ui32PDSVertexMemFlags;	/* PRQA S 3199 */ /* but it is used */
                DPF("Memory flags test (PDS Vertex): ");
                DPF("Not currently supported");
                SGXUT_ERROR_EXIT();
            }
            else if (strcmp(argv[ui32ArgCounter], "pdsvc") == 0)
            {
                pui32MemFlags = &psConfig->ui32PDSVertexCodeMemFlags;
                DPF("Memory flags test (PDS Vertex Code): ");
            }
            else if (strcmp(argv[ui32ArgCounter], "up") == 0)
            {
                psConfig->bUSSEPixelWriteMem = IMG_TRUE;
                pui32MemFlags = &psConfig->ui32USSEPixelMemFlags;
                DPF("Memory flags test (USSE Pixel): ");
            }
            else if (strcmp(argv[ui32ArgCounter], "upc") == 0)
            {
                pui32MemFlags = &psConfig->ui32PixelUSSECodeMemFlags;
                DPF("Memory flags test (USSE Pixel Code): ");
            }
            else if (strcmp(argv[ui32ArgCounter], "uv") == 0)
            {
                psConfig->bUSSEVertexWriteMem = IMG_TRUE;
                pui32MemFlags = &psConfig->ui32USSEVertexMemFlags;
                DPF("Memory flags test (USSE Vertex): ");
            }
            else if (strcmp(argv[ui32ArgCounter], "uvc") == 0)
            {
                pui32MemFlags = &psConfig->ui32VertexUSSECodeMemFlags;
                DPF("Memory flags test (USSE Vertex Code): ");
            }
            else
            {
                DPF("Unknown memory flags test option %s\n", argv[ui32ArgCounter]);
                pui32MemFlags = IMG_NULL; /* avoid warning */ /* PRQA S 3199 */ /* but it is used */
                SGXUT_ERROR_EXIT();
            }

            ui32ArgCounter++;
            *pui32MemFlags = 0;
            {
                IMG_UINT32 ui32Counter;

                for (ui32Counter = 0; argv[ui32ArgCounter][ui32Counter] != 0; ui32Counter++)
                {
                    switch (argv[ui32ArgCounter][ui32Counter])
                    {
                        case '0':
                        {
                            /*
                                No-op to allow 0 to be specified on command line.
                                This isn't very useful because services treats
                                the absence of both read and write to mean allow both read and write.
                            */
                            break;
                        }
                        case 'r':
                        {
                            *pui32MemFlags |= PVRSRV_MEM_READ;
                            DPF("Read ");
                            break;
                        }
                        case 'w':
                        {
                            *pui32MemFlags |= PVRSRV_MEM_WRITE;
                            DPF("Write ");
                            break;
                        }
                        case 'e':
                        {
                            *pui32MemFlags |= PVRSRV_MEM_EDM_PROTECT;
                            DPF("EDM_PROTECT ");
                            break;
                        }
                        default:
                        {
                            DPF("Unknown memory flags test value %c\n", argv[ui32ArgCounter][ui32Counter]);
                            SGXUT_ERROR_EXIT();
                        }
                    }
                }

                DPF("\n");
            }
            break;
        }
        case SRFT_COMMANDLINE_OPTION_SLOWDOWN_TA:
        {
            ui32ArgCounter++;
            /*
                This causes a no-op loop to be added to the vertex shader.
                Useful for simulating the dynamics of other tests.
            */
            psConfig->ui32SlowdownFactorTA = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            DPF("TA Slowdown Factor: %u\n", psConfig->ui32SlowdownFactorTA);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_SLOWDOWN_3D:
        {
            ui32ArgCounter++;
            /*
                This causes a no-op loop to be added to the pixel shader.
                Useful for simulating the dynamics of other tests.
            */
            psConfig->ui32SlowdownFactor3D = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            DPF("3D Slowdown Factor: %u\n", psConfig->ui32SlowdownFactor3D);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_PARAMETERHEAPRESERVE:
        {
            ui32ArgCounter++;
            /*
                This relatively obscure option can be used to push the base of
                the PB above the heap base, which can be useful for testing DPM
                Page Table initialisation is correct, and multi-app sanity when
                using per-context PBs.
            */
            psConfig->ui323DParamsHeapReserveFactor = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            DPF("Parameters heap reserve factor: %u\n", psConfig->ui323DParamsHeapReserveFactor);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_PIXELSHADERHEAPRESERVE:
        {
            ui32ArgCounter++;
            /*
                This relatively obscure option can be used to ensure that
                the pixel shaders have addresses which are not at the base
                of the heap. This helps test the USSE program counter range.
            */
            psConfig->ui32PixelShaderHeapReserve = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            DPF("Pixel Shader heap reserve: %u kB\n", psConfig->ui32PixelShaderHeapReserve);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_RENDERSPERFRAME:
        {
            ui32ArgCounter++;
            psConfig->ui32RendersPerFrame = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            DPF("Renders per frame: %u\n", psConfig->ui32RendersPerFrame);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_STATUSFREQUENCY:
        {
            ui32ArgCounter++;
            psConfig->ui32StatusFrequency = (IMG_UINT32)atol(argv[ui32ArgCounter]);
            if (psConfig->ui32StatusFrequency < 1)
            {
                DPF("Invalid status frequency %u\n", psConfig->ui32StatusFrequency);
                SGXUT_ERROR_EXIT();
            }
            DPF("Status frequency: %u\n", psConfig->ui32StatusFrequency);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_DEBUGBREAK:
        {
            DPF("Debug Break: ENABLED\n");
            psConfig->bDebugBreak = IMG_TRUE;
            break;
        }
        case SRFT_COMMANDLINE_OPTION_CRASHTEST:
        {
            /*
                Crash the process after test by accessing invalid memory.
                Used to test driver's handling of abnormal process termination.
            */
            DPF("Crash Test: ENABLED\n");
            psConfig->bCrashTest = IMG_TRUE;
            break;
        }
        case SRFT_COMMANDLINE_OPTION_DUMPDEBUGINFO:
        {
            /*
                Ask services to dump the SGX debug info, and exit.
            */
            DPF("Dump Debug Info: ENABLED\n");
            psConfig->bGetMiscInfo = IMG_TRUE;
            psConfig->eMiscInfoRequest = SGX_MISC_INFO_DUMP_DEBUG_INFO;
            break;
        }
        case SRFT_COMMANDLINE_OPTION_GETMISCINFO:
        {
            ui32ArgCounter++;
            psConfig->bGetMiscInfo = IMG_TRUE;
            psConfig->eMiscInfoRequest = (SGX_MISC_INFO_REQUEST)atol(argv[ui32ArgCounter]);
            DPF("Misc Info Request: %u\n", psConfig->eMiscInfoRequest);
            break;
        }
        case SRFT_COMMANDLINE_OPTION_DUMPPROGRAMADDRS:
        {
            /*
                Output debug regarding SGX program addresses.
            */
            DPF("Debug Program Addresses: ENABLED\n");
            psConfig->bDebugProgramAddrs = IMG_TRUE;
            break;
        }
        case SRFT_COMMANDLINE_OPTION_DUMPSURFACEADDRS:
        {
            /*
                Output debug regarding SGX surface addresses.
            */
            DPF("Debug Surface Addresses: ENABLED\n");
            psConfig->bDebugSurfaceAddrs = IMG_TRUE;
            break;			
        }
        case SRFT_COMMANDLINE_OPTION_VERBOSE:
        {
            /*
                Verbose mode.
            */
            DPF("Verbose mode: ENABLED\n");
            psConfig->bVerbose = IMG_TRUE;
            break;
        }
        case SRFT_COMMANDLINE_OPTION_OUTPUT_PREAMBLE:
        {
            /*
                Include preamble in output.
            */
            DPF("Output preamble: ENABLED\n");
            psConfig->bOutputPreamble = IMG_TRUE;
            break;
        }
        case SRFT_COMMANDLINE_OPTION_RC_PRIORITY:
        {
#if defined(SUPPORT_SGX_LOW_LATENCY_SCHEDULING)
            ui32ArgCounter++;
            psConfig->eRCPriority = (SGX_CONTEXT_PRIORITY)atol(argv[ui32ArgCounter]);
            DPF("Render Context Priority: %u\n", (IMG_INT)psConfig->eRCPriority);
#else
            DPF("Low latency scheduling not supported on this platform\n");
            SGXUT_ERROR_EXIT();
#endif
            break;
        }
        case SRFT_COMMANDLINE_OPTION_NO_SYNCOBJ:
        {
            DPF("No SyncObject mode: ENABLED\n");
            psConfig->bNoSyncObject = IMG_TRUE;
            break;
        }
        case SRFT_COMMANDLINE_OPTION_READMEM:
        {
#if defined(SUPPORT_SGX_EDM_MEMORY_DEBUG)
            DPF("Read memory in microkernel: ENABLED\n");
            psConfig->bReadMem = IMG_TRUE;
#else
            DPF("Memory debug not supported\n");
            SGXUT_ERROR_EXIT();
#endif
            break;
        }
        case SRFT_COMMANDLINE_OPTION_EXTENDEDDATABREAKPOINTTEST:
        {
            ui32ArgCounter++;
#if defined(SGX_FEATURE_DATA_BREAKPOINTS)
            psConfig->bExtendedDataBreakpointTest = IMG_TRUE;
            psConfig->pszExtendedDataBreakpointTestConfigFilename = argv[ui32ArgCounter];
            DPF("Extended Data Breakpoint Test: ENABLED (will read configuration from '%s')\n", psConfig->pszExtendedDataBreakpointTestConfigFilename);
#else /* SGX_FEATURE_DATA_BREAKPOINTS */	
            DPF("Data Breakpoint not supported on this system\n");
            SGXUT_ERROR_EXIT();
#endif /* SGX_FEATURE_DATA_BREAKPOINTS */	
            break;
        }
        case SRFT_COMMANDLINE_OPTION_PERCONTEXT_PB:
        {
            psConfig->bPerContextPB = IMG_TRUE;
            break;
        }
        case SRFT_COMMANDLINE_OPTION_PBSIZEMAX:
        {
            ui32ArgCounter++;
            psConfig->ui32MaxPBSize = 1024UL * (IMG_UINT32)atol(argv[ui32ArgCounter]);
            DPF("Max Parameter Buffer size: 0x%x Bytes\n", psConfig->ui32MaxPBSize);
            break;

        }
        case SRFT_COMMANDLINE_OPTION_2XMSAADONTRESOLVE:
        {
            psConfig->b2XMSAADontResolve = IMG_TRUE;
            break;
        }

        default:
        {
            DPF("Unexpected error parsing command line args\n");
            SGXUT_ERROR_EXIT();
        }
        } /* swicth braces not indented*/
    }

    if (ui32NumBuffers != 0)
    {
        if (psConfig->bBlit)
        {
            psConfig->ui32NumBackBuffers = ui32NumBuffers;
        }

        if (psConfig->bFlip)
        {
            psConfig->ui32NumSwapChainBuffers = ui32NumBuffers;
        }
    }

    if (!bSleepTimeUS2Specified)
    {
        psConfig->ui32SleepTimeUS2 = psConfig->ui32SleepTimeUS1;
    }

    if (!bTrianglesPerFrame2Specified)
    {
        psConfig->ui32TrianglesPerFrame2 = psConfig->ui32TrianglesPerFrame1;
    }

    if (!bVertexColour2Specified)
    {
        psConfig->sVertexColour2 = psConfig->sVertexColour1;
    }

    if (!bBGColourSpecified)
    {
        psConfig->sBGColour = psConfig->bBlit ?
                                psConfig->b2DBlit ? SRFT_CYAN :
                                    psConfig->bSoftwareBlit ? SRFT_MAGENTA :
                                        SRFT_YELLOW
                                :	(!psConfig->bFlip ? SRFT_BROWN
                                        : SRFT_GREEN);
    }

    if (!bVertexColour3Specified)
    {
        psConfig->sVertexColour3 = psConfig->sBGColour;
    }

    if (psConfig->bFlip && (psConfig->ui32MTGridSizeX * psConfig->ui32MTGridSizeY > 1))
    {
        DPF("Please disable flipping when using multiple threads\n");
        SGXUT_ERROR_EXIT();
    }

#if !defined(SYS_USING_INTERRUPTS)
    if (psConfig->bFlip && (psConfig->ui32SwapInterval > 0))
    {
        DPF("This system does not support display interrupts, so a non-zero swap (flip) interval is not supported.\n");
        DPF("Consider using a zero swap interval using the option -si 0\n");
        SGXUT_ERROR_EXIT();
    }
    if (!psConfig->bNoEventObject)
    {
        DPF("This system does not support interrupts, so the Event Object not supported.\n");
        DPF("Consider disabling the Event Object using the option -neo\n");
        SGXUT_ERROR_EXIT();
    }
#endif /* SYS_USING_INTERRUPTS */
}


/*!
*****************************************************************************

 @Function	main

 @Description	Executable entry point.

******************************************************************************/
IMG_INT
#if !defined(LINUX)
#if !defined(__psp2__)
__cdecl
#endif
#endif
main(IMG_INT argc, IMG_CHAR **argv)
{
#ifdef __psp2__
    SceUID							driverMemUID;
    SceUID							pbMemUID;
#endif
    PVRSRV_ERROR					eResult;
    SRFT_THREAD_DATA				*asThreadData;
    SRFT_CONFIG						sConfig;
    SRFT_SHARED						sShared;
    PVRSRV_CONNECTION				*psConnection;
    PVRSRV_MISC_INFO				sMiscInfo;
    PVRSRV_DEV_DATA					sSGXDevData;
    IMG_UINT32						i;
    PVRSRV_SGX_CLIENT_INFO			sSGXInfo;
    DISPLAY_FORMAT					sPrimFormat;
    DISPLAY_DIMS					sPrimDims;
    IMG_UINT32						ui32BytesPerPixel;

    fnDPF							fnInfo = sgxut_null_dpf;
    fnFIE							fnInfoFailIfError = sgxut_fail_if_error_quiet;
    /* Swap chain variables */
#if defined (SUPPORT_SID_INTERFACE)
    IMG_SID							hSwapChain = 0;
    IMG_SID							*ahSwapChainBuffers = 0;
#else
    IMG_HANDLE						hSwapChain = IMG_NULL;
    IMG_HANDLE						*ahSwapChainBuffers = IMG_NULL;
#endif
    PVRSRV_CLIENT_MEM_INFO			**apsSwapBufferMemInfo = IMG_NULL;
    IMG_UINT32						ui32SwapChainID = 0;

    /* Present blit variables */
#if defined(SUPPORT_PVRSRV_GET_DC_SYSTEM_BUFFER)
#if defined (SUPPORT_SID_INTERFACE)
    IMG_SID							hSystemBuffer = 0;
#else
    IMG_HANDLE						hSystemBuffer = IMG_NULL;
#endif
#endif
    PVRSRV_CLIENT_MEM_INFO			*psSGXSystemBufferMemInfo = IMG_NULL;

    IMG_UINT32						ui32SharedHeapCount;
    PVRSRV_HEAP_INFO				asHeapInfo[PVRSRV_MAX_CLIENT_HEAPS];
    DISPLAY_SURF_ATTRIBUTES			sDstSurfAttrib;
    DISPLAY_SURF_ATTRIBUTES			sSrcSurfAttrib;
    IMG_UINT32						ui32Green;
    IMG_HANDLE						hDisplayDevice;
    SGX_TRANSFERCONTEXTCREATE		sCreateTransfer;
    IMG_HANDLE						hTransferContext = IMG_NULL;
    IMG_UINT32						ui322DSurfaceFormat = 0;	/* PRQA S 3197 */ /* default case */
    IMG_UINT32						ui32MainRenderLineStride;
    IMG_UINT32						ui32FBTexFormat;
    IMG_UINT32						ui32RectangleSizeX, ui32RectangleSizeY;

    /* Extended Data Breakpoint Testing */
    EDBP_STATE_DATA                      * psEdbpState;
#if defined(SRFT_SUPPORT_PTHREADS)
    EDBP_HANDLE_BKPT_THREAD_DATA    sEdbpThreadData;
#endif

    /**************************************************************************
    *	Render context and render target
    **************************************************************************/
    SGX_CREATERENDERCONTEXT			sCreateRenderContext;
    IMG_HANDLE						hRenderContext;
    PVRSRV_CLIENT_MEM_INFO			*psVisTestResultMemInfo;


    /**************************************************************************
    ***************************************************************************
    *	Main services setup and initialisation
    ***************************************************************************
    **************************************************************************/

    memset(&sShared, 0, sizeof(sShared));

    srft_ParseOptions(argc, argv, &sConfig);

    if (sConfig.bVerbose)
    {
        fnInfo = DPF;
        fnInfoFailIfError = sgxut_fail_if_error;
    }

    if (sConfig.bDebugBreak)
    {
        PVR_DBG_BREAK;
    }

    DPF("----------------------- Start -----------------------\n");

    /*
        Common SGX initialisation
    */
    sgxut_SGXInit(sConfig.bVerbose,
                  &psConnection,
                  &sSGXDevData,
                  &sSGXInfo,
                  &hDisplayDevice,
                  &sPrimFormat,
                  &sPrimDims,
                  &sShared.hDevMemContext,
                  &ui32SharedHeapCount,
                  asHeapInfo);

    if (sConfig.bGetMiscInfo)
    {
        SGX_MISC_INFO	sSGXMiscInfo;

        DPF("Calling SGXGetMiscInfo\n");

        sSGXMiscInfo.eRequest = sConfig.eMiscInfoRequest;
        eResult = SGXGetMiscInfo(&sSGXDevData, &sSGXMiscInfo);
        fnInfoFailIfError(eResult);

        goto srft_CommonExit;
    }

    #if defined(PDUMP)
    /*
        Now there is a connection, the command-line can be pdumped.
    */
    {
        IMG_INT32	i32ArgCounter;
        PVRSRVPDumpComment(psConnection, "Start of command-line options", IMG_TRUE);

        for (i32ArgCounter = 0; i32ArgCounter < argc; i32ArgCounter++)
        {
            PVRSRVPDumpComment(psConnection, argv[i32ArgCounter], IMG_TRUE);
        }

        PVRSRVPDumpComment(psConnection, "End of command-line options", IMG_TRUE);
    }
    #endif /* PDUMP */
    
        psEdbpState = malloc(sizeof(EDBP_STATE_DATA));
        if (!psEdbpState)
        {
            DPF("malloc of EDBP_STATE_DATA failed");
            SGXUT_ERROR_EXIT();
        }
    
    edbp_Init(psEdbpState, &sSGXDevData);

    if(sConfig.bExtendedDataBreakpointTest)
    {
        edbp_ReadConfig(psEdbpState, sConfig.pszExtendedDataBreakpointTestConfigFilename);
    }

    ui32RectangleSizeX = sPrimDims.ui32Width / (sConfig.ui32MPGridSizeX * sConfig.ui32MTGridSizeX);
    if (ui32RectangleSizeX < 32)
    {
        DPF("Render width must be at least 32 pixels\n");
        SGXUT_ERROR_EXIT();
    }
    /* Round down to multiple of 32 */
    ui32RectangleSizeX = (ui32RectangleSizeX / 32) * 32;
    ui32RectangleSizeY = sPrimDims.ui32Height / (sConfig.ui32MPGridSizeY * sConfig.ui32MTGridSizeY);

    /* Map the pixel format */
    switch(sPrimFormat.pixelformat)
    {
        case PVRSRV_PIXEL_FORMAT_RGB565:
        {
            ui32BytesPerPixel = 2;
            ui322DSurfaceFormat = 0x0A;
            ui32Green = 0x07E007E0;
            ui32FBTexFormat = EURASIA_PDS_DOUTT1_TEXFORMAT_R5G6B5;
#if defined(PDUMP)
            sShared.eDumpFormat = PVRSRV_PDUMP_PIXEL_FORMAT_RGB565;
#endif
            break;
        }
        case PVRSRV_PIXEL_FORMAT_ARGB8888:
        {
            ui32BytesPerPixel = 4;
            ui322DSurfaceFormat = 0x0C;
            ui32Green = 0xFF00FF00;/* full alpha, full green */
#if defined(SGX_FEATURE_TAG_SWIZZLE)
            ui32FBTexFormat = EURASIA_PDS_DOUTT1_TEXFORMAT_U8888;
#else
            ui32FBTexFormat = EURASIA_PDS_DOUTT1_TEXFORMAT_B8G8R8A8;
#endif /* SGX_FEATURE_TAG_SWIZZLE */

#if defined(PDUMP)
            sShared.eDumpFormat = PVRSRV_PDUMP_PIXEL_FORMAT_ARGB8888;
#endif
            break;
        }
        case PVRSRV_PIXEL_FORMAT_ABGR8888:
        {
            ui32BytesPerPixel = 4;
            ui322DSurfaceFormat = 0x0C;
            ui32Green = 0xFF00FF00;/* full alpha, full green */
#if defined(SGX_FEATURE_TAG_SWIZZLE)
            ui32FBTexFormat = EURASIA_PDS_DOUTT1_TEXFORMAT_U8888;
#else
            ui32FBTexFormat = EURASIA_PDS_DOUTT1_TEXFORMAT_B8G8R8A8;
#endif /* SGX_FEATURE_TAG_SWIZZLE */

#if defined(PDUMP)
            sShared.eDumpFormat = PVRSRV_PDUMP_PIXEL_FORMAT_ARGB8888;
#endif
            break;
        }
        default:
        {
            DPF("Display Class API: ERROR unsupported pixel format!\n");
            SGXUT_ERROR_EXIT();
        }
    }

    /**************************************************************************
    *	Setup heap handles for device mem context
    **************************************************************************/
    for(i=0; i < ui32SharedHeapCount; i++)
    {
        switch(HEAP_IDX(asHeapInfo[i].ui32HeapID))
        {
            case SGX_PIXELSHADER_HEAP_ID:
            {
                sShared.hUSEFragmentHeap		= asHeapInfo[i].hDevMemHeap;
                sShared.uUSEFragmentHeapBase	= asHeapInfo[i].sDevVAddrBase;
                break;
            }
            case SGX_VERTEXSHADER_HEAP_ID:
            {
                sShared.hUSEVertexHeap			= asHeapInfo[i].hDevMemHeap;
                sShared.uUSEVertexHeapBase		= asHeapInfo[i].sDevVAddrBase;
                break;
            }
            case SGX_PDSPIXEL_CODEDATA_HEAP_ID:
            {
                sShared.hPDSFragmentHeap		= asHeapInfo[i].hDevMemHeap;
                break;
            }
            case SGX_PDSVERTEX_CODEDATA_HEAP_ID:
            {
                sShared.hPDSVertexHeap			= asHeapInfo[i].hDevMemHeap;
                break;
            }
            case SGX_GENERAL_HEAP_ID:
            {
                sShared.hGeneralHeap			= asHeapInfo[i].hDevMemHeap;
                break;
            }
            case SGX_SHARED_3DPARAMETERS_HEAP_ID:
            {
                if (!sConfig.bPerContextPB)
                    sShared.h3DParametersHeap		= asHeapInfo[i].hDevMemHeap;
                break;
            }
            case SGX_PERCONTEXT_3DPARAMETERS_HEAP_ID:
            {
                if (sConfig.bPerContextPB)
                    sShared.h3DParametersHeap		= asHeapInfo[i].hDevMemHeap;
                break;
            }
#if defined(SGX_FEATURE_2D_HARDWARE)
            case SGX_2D_HEAP_ID:
            {
                sShared.h2DHeap					= asHeapInfo[i].hDevMemHeap;
                sShared.u2DHeapBase				= asHeapInfo[i].sDevVAddrBase;
                break;
            }
#endif /* SGX_FEATURE_2D_HARDWARE */
            default:
            {
                break;
            }
        }
    }

#if defined(SUPPORT_PVRSRV_GET_DC_SYSTEM_BUFFER)
    /**************************************************************************
    *	Get a handle to the primary surface in the system.
    **************************************************************************/
    fnInfo("Display Class API: get the system (primary) buffer\n");
    eResult = PVRSRVGetDCSystemBuffer(hDisplayDevice, &hSystemBuffer);
    fnInfoFailIfError(eResult);
#endif

    if (sConfig.bFlip)
    {
        /**************************************************************************
        *	Create primary swap chain
        **************************************************************************/
        sSrcSurfAttrib.pixelformat	= sPrimFormat.pixelformat;
        sSrcSurfAttrib.sDims		= sPrimDims;
        sDstSurfAttrib				= sSrcSurfAttrib;

        fnInfo("Display Class API: Create DC swap chain\n");
        eResult = PVRSRVCreateDCSwapChain(hDisplayDevice,
                                            0,
                                            &sDstSurfAttrib,
                                            &sSrcSurfAttrib,
                                            sConfig.ui32NumSwapChainBuffers,
                                            0,
                                            &ui32SwapChainID,
                                            &hSwapChain);
        if (eResult != PVRSRV_OK)
        {
            DPF("Unable to create Display Class flip chain (%s)\n", PVRSRVGetErrorString(eResult));
            DPF("Consider disabling flipping using the option -nf\n");
            SGXUT_ERROR_EXIT();
        }

        ahSwapChainBuffers = malloc(sConfig.ui32NumSwapChainBuffers * sizeof(ahSwapChainBuffers[0]));
        SGXUT_CHECK_MEM_ALLOC(ahSwapChainBuffers);

        /**************************************************************************
        *	Retrieve and map swap buffers
        **************************************************************************/
        fnInfo("Display Class API: Get DC swap buffers\n");
        eResult = PVRSRVGetDCBuffers(hDisplayDevice,
                                     hSwapChain,
                                     ahSwapChainBuffers);
        fnInfoFailIfError(eResult);

        apsSwapBufferMemInfo = malloc(sConfig.ui32NumSwapChainBuffers * sizeof(apsSwapBufferMemInfo[0]));
        if (apsSwapBufferMemInfo == IMG_NULL)
        {
            fnInfoFailIfError(PVRSRV_ERROR_OUT_OF_MEMORY);
        }

        SGXUT_CHECK_MEM_ALLOC(apsSwapBufferMemInfo);

        for(i = 0; i < sConfig.ui32NumSwapChainBuffers; i++)
        {
            IMG_UINT32 j;
            fnInfo("Display Class API: Map swap chain buffer %u\n", i);
            eResult = PVRSRVMapDeviceClassMemory(&sSGXDevData,
                                                 sShared.hDevMemContext,
                                                 ahSwapChainBuffers[i],
                                                 &apsSwapBufferMemInfo[i]);
            fnInfoFailIfError(eResult);
            if (sConfig.bDebugSurfaceAddrs)
            {
                DPF("Swap chain buffer %u - Address 0x%x Size 0x%x\n", i,
                    apsSwapBufferMemInfo[i]->sDevVAddr.uiAddr,
                    apsSwapBufferMemInfo[i]->uAllocSize);
            }			

            fnInfo("Colour swap buffer %u (size 0x%x)\n",
                i, apsSwapBufferMemInfo[i]->uAllocSize);

            /* Write a green background to each surface */
            for(j = 0; j < apsSwapBufferMemInfo[i]->uAllocSize / 4; j++)
            {
                ((IMG_UINT32 *)apsSwapBufferMemInfo[i]->pvLinAddr)[j] = ui32Green;
            }
            edbp_NumberedDeviceBuffer(psEdbpState, "SwapBuffer", i, apsSwapBufferMemInfo[i]);
        }
    }
    else
    {
#if defined(SUPPORT_PVRSRV_GET_DC_SYSTEM_BUFFER)
        /**************************************************************************
        *	Map system surface
        **************************************************************************/
        fnInfo("Display Class API: map display surface to SGX\n");
        eResult = PVRSRVMapDeviceClassMemory(&sSGXDevData,
                                             sShared.hDevMemContext,
                                             hSystemBuffer,
                                             &psSGXSystemBufferMemInfo);
        fnInfoFailIfError(eResult);
        
        if (sConfig.bDebugSurfaceAddrs)
        {
            DPF("System buffer - Address 0x%x Size 0x%x\n",
                psSGXSystemBufferMemInfo->sDevVAddr.uiAddr,
                psSGXSystemBufferMemInfo->uAllocSize);
        }			
        edbp_NumberedDeviceBuffer(psEdbpState, "SGXSystemBuffer", 0, psSGXSystemBufferMemInfo);
#else
        DPF("Using the system/primary surface is not supported on android");
#endif
    }

    /**************************************************************************
    * Get services miscellaneous info
    **************************************************************************/

#ifndef __psp2__

    fnInfo("Getting Misc Info\n");
    sMiscInfo.ui32StateRequest = PVRSRV_MISC_INFO_GLOBALEVENTOBJECT_PRESENT;
    eResult = PVRSRVGetMiscInfo(psConnection, &sMiscInfo);
    fnInfoFailIfError(eResult);
#if defined (SUPPORT_SID_INTERFACE)
    sShared.hOSGlobalEvent = sConfig.bNoEventObject ? 0 : sMiscInfo.hOSGlobalEvent;
#else
    sShared.hOSGlobalEvent = sConfig.bNoEventObject ? IMG_NULL : sMiscInfo.hOSGlobalEvent;
#endif

#else

    sShared.hOSGlobalEvent = 0;

#endif

#ifdef __psp2__

    driverMemUID = sceKernelAllocMemBlock("SGXDriver", SCE_KERNEL_MEMBLOCK_TYPE_USER_NC_RW, 0x100000, NULL);

    PVRSRVRegisterMemBlock(&sSGXDevData, driverMemUID, &hPbMem, IMG_FALSE);

    pbMemUID = sceKernelAllocMemBlock("SGXParamBuffer", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, sConfig.ui32PBSize, NULL);

    PVRSRVRegisterMemBlock(&sSGXDevData, pbMemUID, &hPbMem, IMG_FALSE);

#endif

    if (sConfig.bBlit)
    {
        ui32MainRenderLineStride = ui32RectangleSizeX;

        if (!sConfig.bSoftwareBlit)
        {
            fnInfo("Attempt to create transfer context for SGX:\n");
            sCreateTransfer.hDevMemContext = sShared.hDevMemContext;
#ifdef __psp2__
            sCreateTransfer.hMemBlockProcRef = hPbMem;
#else
            sCreateTransfer.hOSEvent = sShared.hOSGlobalEvent;
#endif
            eResult = SGXCreateTransferContext(&sSGXDevData,
                                               0,
                                               &sCreateTransfer,
                                               &hTransferContext);
            fnInfoFailIfError(eResult);
        }
    }
    else
    {
        ui32MainRenderLineStride = sPrimDims.ui32ByteStride / ui32BytesPerPixel;
    }

    /**************************************************************************
    * Create the render context
    **************************************************************************/

#ifdef __psp2__

    sCreateRenderContext.ui32Flags = 2 | 4;
    sCreateRenderContext.hDevCookie = sSGXDevData.hDevCookie;
    sCreateRenderContext.hDevMemContext = sShared.hDevMemContext;
    sCreateRenderContext.ui32PBSize = sConfig.ui32PBSize - 0x80000;
    sCreateRenderContext.ui32PBSizeLimit = sCreateRenderContext.ui32PBSize;
    sCreateRenderContext.ui32MaxSACount = 128;
    sCreateRenderContext.bPerContextPB = sConfig.bPerContextPB;
    sCreateRenderContext.hMemBlockProcRef = hPbMem;

    /*
       We don't need to test the visibility buffer, but don't pass 0 since it
       might confuse the memory manager in srvkm.
    */
    sCreateRenderContext.ui32VisTestResultBufferSize = 1;

#else

    sCreateRenderContext.ui32Flags			= 0;
    sCreateRenderContext.hDevCookie			= sSGXDevData.hDevCookie;
    sCreateRenderContext.hDevMemContext		= sShared.hDevMemContext;
    sCreateRenderContext.ui32PBSize			= sConfig.ui32PBSize;
    sCreateRenderContext.ui32PBSizeLimit	= sConfig.ui32MaxPBSize;
    sCreateRenderContext.ui32MaxSACount 	= 128;
    sCreateRenderContext.bPerContextPB 		= sConfig.bPerContextPB;

    /*
       We don't need to test the visibility buffer, but don't pass 0 since it
       might confuse the memory manager in srvkm.
    */
    sCreateRenderContext.ui32VisTestResultBufferSize = 1;
    sCreateRenderContext.hOSEvent = sShared.hOSGlobalEvent;

#endif

    fnInfo("Creating render context via SGXCreateRenderContext\n");
    eResult = SGXCreateRenderContext(&sSGXDevData,
                                     &sCreateRenderContext,
                                     &hRenderContext,
                                     &psVisTestResultMemInfo);

    fnInfoFailIfError(eResult);

#if defined(SUPPORT_SGX_PRIORITY_SCHEDULING)
    fnInfo("Attempt to set priority of context for SGX to %u:\n",(IMG_UINT32)sConfig.eRCPriority);

    eResult = SGXSetContextPriority(&sSGXDevData,
                                    &sConfig.eRCPriority,
                                    hRenderContext,
                                    hTransferContext);

    fnInfoFailIfError(eResult);

    fnInfo("Priority of transfer/render context for SGX was set to %u:\n",(IMG_UINT32)sConfig.eRCPriority);

#endif

    sShared.fnInfo						= fnInfo;
    sShared.fnInfoFailIfError			= fnInfoFailIfError;
    sShared.psSGXDevData				= &sSGXDevData;
    sShared.psSGXInfo					= &sSGXInfo;
    sShared.hDisplayDevice				= hDisplayDevice;
    sShared.ppsSwapBufferMemInfo		= apsSwapBufferMemInfo;
    sShared.psSGXSystemBufferMemInfo	= psSGXSystemBufferMemInfo;
    sShared.phSwapChainBuffers			= ahSwapChainBuffers;
    sShared.ui32RectangleSizeX			= ui32RectangleSizeX;
    sShared.ui32RectangleSizeY			= ui32RectangleSizeY;
    sShared.ui32MainRenderLineStride	= ui32MainRenderLineStride;
    sShared.ui32BytesPerPixel			= ui32BytesPerPixel;
    sShared.ui32FBTexFormat				= ui32FBTexFormat;
    sShared.psPrimDims					= &sPrimDims;
    sShared.ui322DSurfaceFormat			= ui322DSurfaceFormat;
    sShared.psPrimFormat				= &sPrimFormat;

    sShared.hTransferContext			= hTransferContext;
    sShared.hRenderContext				= hRenderContext;
    sShared.psConnection				= psConnection;

    sShared.psEdbpState                 = psEdbpState;

    asThreadData = malloc(sizeof(asThreadData[0]) * sConfig.ui32MTGridSizeX * sConfig.ui32MTGridSizeY);
    SGXUT_CHECK_MEM_ALLOC(asThreadData);

    {
        IMG_UINT32	ui32ThreadX, ui32ThreadY;

        /*
            Create the threads.
        */
        for (ui32ThreadX = 0; ui32ThreadX < sConfig.ui32MTGridSizeX; ui32ThreadX++)
        {
            for (ui32ThreadY = 0; ui32ThreadY < sConfig.ui32MTGridSizeY; ui32ThreadY++)
            {
                SRFT_THREAD_DATA	*psThreadData = &asThreadData[ui32ThreadX * sConfig.ui32MTGridSizeY + ui32ThreadY];

                psThreadData->psConfig		= &sConfig;
                psThreadData->psShared 		= &sShared;
                psThreadData->ui32GridPosX	= sConfig.ui32MPGridPosX * sConfig.ui32MTGridSizeX + ui32ThreadX;
                psThreadData->ui32GridPosY	= sConfig.ui32MPGridPosY * sConfig.ui32MTGridSizeY + ui32ThreadY;

                fnInfo("Creating thread (%u, %u)\n", psThreadData->ui32GridPosX, psThreadData->ui32GridPosY);
                if (pthread_create(&psThreadData->sPThread, IMG_NULL, srft_PerThread, psThreadData) != 0)
                {
                    fnInfoFailIfError(PVRSRV_ERROR_INVALID_PARAMS);
                }
            }
        }

        if(sConfig.bExtendedDataBreakpointTest)
        {
#if defined(SRFT_SUPPORT_PTHREADS)
            edbp_HandleBkptThreadStart(&sEdbpThreadData, psConnection, psEdbpState, sShared.hOSGlobalEvent);
#endif
        }

        /*
            Wait for threads to complete their processing.
        */
        for (ui32ThreadX = 0; ui32ThreadX < sConfig.ui32MTGridSizeX; ui32ThreadX++)
        {
            for (ui32ThreadY = 0; ui32ThreadY < sConfig.ui32MTGridSizeY; ui32ThreadY++)
            {
                SRFT_THREAD_DATA	*psThreadData = &asThreadData[ui32ThreadX * sConfig.ui32MTGridSizeY + ui32ThreadY];

                fnInfo("Joining thread (%u, %u)\n", psThreadData->ui32GridPosX, psThreadData->ui32GridPosY);
                if (pthread_join(psThreadData->sPThread, IMG_NULL) != 0)
                {
                    fnInfoFailIfError(PVRSRV_ERROR_INVALID_PARAMS);
                }
            }
        }
    }

    free(asThreadData);

    if(sConfig.bExtendedDataBreakpointTest)
    {
#if defined(SRFT_SUPPORT_PTHREADS)
        edbp_HandleBkptThreadStop(&sEdbpThreadData);
#endif

        edbp_UninstallAllBreakpoints(psEdbpState);
        /* even though we've just uninstalled the breakpoints, it is
           _just_ possible that there's still an outstanding one.  So
           we'd probably ought to just check... */
        edbp_HandleBkpt(psEdbpState);
        edbp_PostCheck(psEdbpState);
    }

    /* Cleanup psEdbpState */
    free(psEdbpState);	
    
    if (sConfig.bNoCleanup)
    {
        DPF("No cleanup was specified. Exiting.\n");
        exit(0);
    }

    /**************************************************************************
    ***************************************************************************
    *	3D services Cleanup
    ***************************************************************************
    **************************************************************************/

    /**************************************************************************
    * Destroy render context
    **************************************************************************/
    fnInfo("Attempt to destroy render context:\n");
    eResult = SGXDestroyRenderContext(&sSGXDevData,
                                      hRenderContext,
                                      psVisTestResultMemInfo,
                                      CLEANUP_WITH_POLL);
    fnInfoFailIfError(eResult);

    /**************************************************************************
    ***************************************************************************
    *	Device memory de-initialisation
    ***************************************************************************
    **************************************************************************/

    /**************************************************************************
    ***************************************************************************
    *	Display device de-initialisation
    ***************************************************************************
    **************************************************************************/

    if (sConfig.bBlit)
    {
        if (!sConfig.bSoftwareBlit)
        {
            /**************************************************************************
            * Destroy transfer context
            **************************************************************************/
            fnInfo("Destroy the transfer context:\n");
#ifdef __psp2__
            eResult = SGXDestroyTransferContext(&sSGXDevData, hTransferContext, CLEANUP_WITH_POLL);
#else
            eResult = SGXDestroyTransferContext(hTransferContext, CLEANUP_WITH_POLL);
#endif
            fnInfoFailIfError(eResult);
        }
    }

    if (sConfig.bFlip)
    {
        /**************************************************************************
        * Swap back to the sytem buffer
        **************************************************************************/
        eResult = PVRSRVSwapToDCSystem(hDisplayDevice, hSwapChain);
        sgxut_fail_if_error_quiet(eResult);

        /**************************************************************************
        * Unmap swapchain back buffers
        **************************************************************************/
        fnInfo("Display Class API: unmap swapchain display surfaces display surface from SGX\n");

        for(i = 0; i < sConfig.ui32NumSwapChainBuffers; i++)
        {
            eResult = PVRSRVUnmapDeviceClassMemory(&sSGXDevData,
                                                   apsSwapBufferMemInfo[i]);	/* PRQA S 505 */ /* not very good at tracking pointers is it */
            fnInfoFailIfError(eResult);
        }

        free(ahSwapChainBuffers);

        /**************************************************************************
        * Destroy primary swapchain
        **************************************************************************/
        fnInfo("Destroy Primary Swap Chain\n");
        eResult = PVRSRVDestroyDCSwapChain(hDisplayDevice, hSwapChain);
        fnInfoFailIfError(eResult);
    }
    else
    {
#if defined(SUPPORT_PVRSRV_GET_DC_SYSTEM_BUFFER)
        /**************************************************************************
        * Unmap primary surface
        **************************************************************************/
        fnInfo("Display Class API: unmap display surface from SGX\n");
        eResult = PVRSRVUnmapDeviceClassMemory(&sSGXDevData,
                                               psSGXSystemBufferMemInfo);
        fnInfoFailIfError(eResult);
#endif
    }

#if defined(PDUMP)
    PVRSRVPDumpComment(sShared.psConnection, "sgx_render_flip_test cleanup complete. Advancing pdump frame to unblock pdump", IMG_TRUE);
    PVRSRVPDumpSetFrame(sShared.psConnection, sConfig.ui32FrameStart + sConfig.ui32MaxIterations + 1);
#endif

    srft_CommonExit:
    /*
        Common SGX de-initialisation
    */
    sgxut_SGXDeInit(sConfig.bVerbose, psConnection, &sSGXDevData, &sSGXInfo,
                    hDisplayDevice, sShared.hDevMemContext);
    
    return 0;
}

/******************************************************************************
 End of file (sgx_render_flip_test.c)
******************************************************************************/
