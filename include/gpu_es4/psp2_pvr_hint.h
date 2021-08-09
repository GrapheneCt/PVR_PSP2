#ifndef PSP2_PVR_HINT_H
#define PSP2_PVR_HINT_H

// Universal header for PVRSRV apphint. Use this instead of services.h

typedef struct _PVRSRV_PSP2_APPHINT_
{
	/* Common hints */

	unsigned int ui32PDSFragBufferSize;
	unsigned int  ui32ParamBufferSize;
	unsigned int  ui32DriverMemorySize;
	unsigned int  ui32ExternalZBufferMode;
	unsigned int  ui32ExternalZBufferXSize;
	unsigned int  ui32ExternalZBufferYSize;
	unsigned int bDumpProfileData;
	unsigned int ui32ProfileStartFrame;
	unsigned int ui32ProfileEndFrame;
	unsigned int bDisableMetricsOutput;
	char szWindowSystem[256]; //path to libpvrPSP2_WSEGL module
	char szGLES1[256]; //path to libGLESv1_CM module
	char szGLES2[256]; //path to libGLESv2 module

	/* OGLES1 hints */

	unsigned int bFBODepthDiscard;
	unsigned int bOptimisedValidation;
	unsigned int bDisableHWTQTextureUpload;
	unsigned int bDisableHWTQNormalBlit;
	unsigned int bDisableHWTQBufferBlit;
	unsigned int bDisableHWTQMipGen;
	unsigned int bDisableHWTextureUpload;
	unsigned int ui32FlushBehaviour;
	unsigned int bEnableStaticPDSVertex;
	unsigned int bEnableStaticMTECopy;
	unsigned int bDisableStaticPDSPixelSAProgram;
	unsigned int bDisableUSEASMOPT;
	unsigned int bDumpShaders;
	unsigned int ui32DefaultVertexBufferSize;
	unsigned int ui32MaxVertexBufferSize;
	unsigned int ui32DefaultIndexBufferSize;
	unsigned int ui32DefaultPDSVertBufferSize;
	unsigned int ui32DefaultPregenPDSVertBufferSize;
	unsigned int ui32DefaultPregenMTECopyBufferSize;
	unsigned int ui32DefaultVDMBufferSize;
	unsigned int bEnableMemorySpeedTest;
	unsigned int bEnableAppTextureDependency;
	unsigned int ui32OGLES1UNCTexHeapSize;
	unsigned int ui32OGLES1CDRAMTexHeapSize;
	unsigned int bOGLES1EnableUNCAutoExtend;
	unsigned int bOGLES1EnableCDRAMAutoExtend;
	unsigned int ui32OGLES1SwTexOpThreadNum;
	unsigned int ui32OGLES1SwTexOpThreadPriority;
	unsigned int ui32OGLES1SwTexOpThreadAffinity;
	unsigned int ui32OGLES1SwTexOpMaxUltNum;
	unsigned int ui32OGLES1SwTexOpCleanupDelay;

} PVRSRV_PSP2_APPHINT;

unsigned int PVRSRVInitializeAppHint(PVRSRV_PSP2_APPHINT *psAppHint);
unsigned int PVRSRVCreateVirtualAppHint(PVRSRV_PSP2_APPHINT *psAppHint);

#endif