/*************************************************************************/ /*!
@Title          Read application specific hints in Linux.
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/

/* PRQA S 3332 3 */ /* ignore warning about using undefined macros */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>


#include "psp2_pvr_defs.h"

#include "pvr_debug.h"
#include "img_defs.h"
#include "services.h"

typedef struct _PVR_APPHINT_STATE_
{
	IMG_MODULE_ID eModuleID;

	const IMG_CHAR *pszAppName;

} PVR_APPHINT_STATE;

static PVRSRV_PSP2_APPHINT s_appHint;
static IMG_BOOL s_appHintCreated = IMG_FALSE;


/******************************************************************************
 Function Name      : PVRSRVCreateAppHintState
 Inputs             : eModuleID, *pszAppName
 Outputs            : *ppvState
 Returns            : none
 Description        : Create app hint state
******************************************************************************/
IMG_EXPORT IMG_VOID PVRSRVCreateAppHintState(IMG_MODULE_ID eModuleID,
										   const IMG_CHAR *pszAppName,
										   IMG_VOID **ppvState)
{
	PVR_APPHINT_STATE* psHintState;
	
	psHintState = PVRSRVAllocUserModeMem(sizeof(PVR_APPHINT_STATE));

	if(!psHintState)
	{
		PVR_DPF((PVR_DBG_ERROR, "OSCreateAppHintState: Failed"));	
	}
	else
	{
		psHintState->eModuleID = eModuleID;
		psHintState->pszAppName = pszAppName;
	}

	*ppvState = (IMG_VOID*)psHintState;
}


/******************************************************************************
 Function Name      : PVRSRVFreeAppHintState
 Inputs             : eModuleID, *pvState
 Outputs            : none
 Returns            : none
 Description        : Free the app hint state, if it was created
******************************************************************************/
IMG_EXPORT IMG_VOID PVRSRVFreeAppHintState(IMG_MODULE_ID eModuleID,
										 IMG_VOID *pvHintState)
{
	PVR_UNREFERENCED_PARAMETER(eModuleID);	/* PRQA S 3358 */ /* override enum warning */

	if (pvHintState)
	{
		PVR_APPHINT_STATE* psHintState = (PVR_APPHINT_STATE*)pvHintState;

		PVRSRVFreeUserModeMem(psHintState);
	}
}


/******************************************************************************
 Function Name      : PVRSRVGetAppHint
 Inputs             : *pvHintState, *pszHintName, eDataType, *pvDefault
 Outputs            : *pvReturn
 Returns            : Boolean - True if hint read, False if used default
 Description        : Return the value of this hint from state or use default
******************************************************************************/
IMG_EXPORT IMG_BOOL PVRSRVGetAppHint(IMG_VOID		*pvHintState,
								   const IMG_CHAR	*pszHintName,
								   IMG_DATA_TYPE	eDataType,
								   const IMG_VOID	*pvDefault,
								   IMG_VOID		*pvReturn)
{
	PVR_UNREFERENCED_PARAMETER(pvHintState);

	IMG_BOOL bFound = IMG_FALSE;
	typedef union
	{
		IMG_VOID    *pvData;
		IMG_UINT32  *pui32Data;
		IMG_CHAR    *pcData;
		IMG_INT32   *pi32Data;
	} PTRXLATE;

	typedef union
	{
		const IMG_VOID    *pvData;
		const IMG_UINT32  *pui32Data;
		const IMG_CHAR    *pcData;
		const IMG_INT32   *pi32Data;
	} CONSTPTRXLATE;

	CONSTPTRXLATE uDefault;
	PTRXLATE uReturn;

	/* Debug sanity check - should be called with known strings */
	PVR_ASSERT(pszHintName);

	if (s_appHintCreated)
	{
		if (!sceClibStrncasecmp(pszHintName, "PDSFragBufferSize", 18))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32PDSFragBufferSize;
			if (*(IMG_UINT32 *)pvReturn != 0)
				bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "ParamBufferSize", 16))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32ParamBufferSize;
			if (*(IMG_UINT32 *)pvReturn != 0)
				bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "DriverMemorySize", 17))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32DriverMemorySize;
			if (*(IMG_UINT32 *)pvReturn != 0)
				bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "ExternalZBufferMode", 20))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32ExternalZBufferMode;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "ui32ExternalZBufferXSize", 25))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32ExternalZBufferXSize;
			if (*(IMG_UINT32 *)pvReturn != 0)
				bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "ui32ExternalZBufferYSize", 25))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32ExternalZBufferYSize;
			if (*(IMG_UINT32 *)pvReturn != 0)
				bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "WindowSystem", 13))
		{
			sceClibStrncpy((IMG_CHAR *)pvReturn, s_appHint.szWindowSystem, APPHINT_MAX_STRING_SIZE);
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "GLES1", 6))
		{
			sceClibStrncpy((IMG_CHAR *)pvReturn, s_appHint.szGLES1, APPHINT_MAX_STRING_SIZE);
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "GLES2", 6))
		{
			sceClibStrncpy((IMG_CHAR *)pvReturn, s_appHint.szGLES2, APPHINT_MAX_STRING_SIZE);
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "DumpProfileData", 16))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bDumpProfileData;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "ProfileStartFrame", 18))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32ProfileStartFrame;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "ProfileEndFrame", 16))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32ProfileEndFrame;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "DisableMetricsOutput", 21))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bDisableMetricsOutput;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "FBODepthDiscard", 16))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bFBODepthDiscard;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "OptimisedValidation", 20))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bOptimisedValidation;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "DisableHWTQTextureUpload", 25))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bDisableHWTQTextureUpload;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "DisableHWTQNormalBlit", 22))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bDisableHWTQNormalBlit;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "DisableHWTQBufferBlit", 22))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bDisableHWTQBufferBlit;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "DisableHWTQMipGen", 18))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bDisableHWTQMipGen;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "DisableHWTQMipGen", 18))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bDisableHWTQMipGen;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "DisableHWTextureUpload", 23))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bDisableHWTextureUpload;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "FlushBehaviour", 15))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32FlushBehaviour;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "EnableStaticPDSVertex", 22))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bEnableStaticPDSVertex;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "EnableStaticMTECopy", 20))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bEnableStaticMTECopy;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "DisableStaticPDSPixelSAProgram", 31))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bDisableStaticPDSPixelSAProgram;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "DisableUSEASMOPT", 17))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bDisableUSEASMOPT;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "DumpShaders", 12))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bDumpShaders;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "DefaultVertexBufferSize", 24))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32DefaultVertexBufferSize;
			if (*(IMG_UINT32 *)pvReturn != 0)
				bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "MaxVertexBufferSize", 20))
		{
		*(IMG_UINT32 *)pvReturn = s_appHint.ui32MaxVertexBufferSize;
			if (*(IMG_UINT32 *)pvReturn != 0)
				bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "DefaultIndexBufferSize", 23))
		{
		*(IMG_UINT32 *)pvReturn = s_appHint.ui32DefaultIndexBufferSize;
			if (*(IMG_UINT32 *)pvReturn != 0)
				bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "DefaultPDSVertBufferSize", 25))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32DefaultPDSVertBufferSize;
			if (*(IMG_UINT32 *)pvReturn != 0)
				bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "DefaultPregenPDSVertBufferSize", 31))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32DefaultPregenPDSVertBufferSize;
			if (*(IMG_UINT32 *)pvReturn != 0)
				bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "DefaultPregenMTECopyBufferSize", 31))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32DefaultPregenMTECopyBufferSize;
			if (*(IMG_UINT32 *)pvReturn != 0)
				bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "DefaultVDMBufferSize", 21))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32DefaultVDMBufferSize;
			if (*(IMG_UINT32 *)pvReturn != 0)
				bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "EnableMemorySpeedTest", 22))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bEnableMemorySpeedTest;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "EnableAppTextureDependency", 27))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bEnableAppTextureDependency;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "UNCTexHeapSize", 21))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32UNCTexHeapSize;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "CDRAMTexHeapSize", 23))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32CDRAMTexHeapSize;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "EnableUNCAutoExtend", 26))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bEnableUNCAutoExtend;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "EnableCDRAMAutoExtend", 28))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bEnableCDRAMAutoExtend;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "SwTexOpThreadNum", 23))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32SwTexOpThreadNum;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "SwTexOpThreadPriority", 28))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32SwTexOpThreadPriority;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "SwTexOpThreadAffinity", 28))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32SwTexOpThreadAffinity;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "SwTexOpMaxUltNum", 23))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32SwTexOpMaxUltNum;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "SwTexOpCleanupDelay", 26))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32SwTexOpCleanupDelay;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "AdjustShaderPrecision", 22))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32AdjustShaderPrecision;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "DumpCompilerLogFiles", 21))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bDumpCompilerLogFiles;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "StrictBinaryVersionComparison", 30))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bStrictBinaryVersionComparison;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "PolygonUnitsMultiplier", 23))
		{
			*(IMG_FLOAT *)pvReturn = s_appHint.fPolygonUnitsMultiplier;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "PolygonFactorMultiplier", 24))
		{
			*(IMG_FLOAT *)pvReturn = s_appHint.fPolygonFactorMultiplier;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "DumpUSPOutput", 14))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bDumpUSPOutput;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "DumpShaderAnalysis", 19))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bDumpShaderAnalysis;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "TrackUSCMemory", 15))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bTrackUSCMemory;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "OverloadTexLayout", 18))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32OverloadTexLayout;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "InitialiseVSOutputs", 20))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bInitialiseVSOutputs;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "TriangleSplitPixelThreshold", 28))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32TriangleSplitPixelThreshold;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "DynamicSplitCalc", 17))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bDynamicSplitCalc;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "AllowTrilinearNPOT", 19))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bAllowTrilinearNPOT;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "EnableVaryingPrecisionOpt", 26))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bEnableVaryingPrecisionOpt;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "DisableAsyncTextureOp", 22))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.bDisableAsyncTextureOp;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "MaxDrawCallsPerCore", 20))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32MaxDrawCallsPerCore;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "PrimitiveSplitThreshold", 24))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32PrimitiveSplitThreshold;
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "GLSLEnabledWarnings", 20))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32GLSLEnabledWarnings;
			bFound = IMG_TRUE;
		}
	}

	if (!bFound)
	{
		uDefault.pvData = pvDefault;
		uReturn.pvData = pvReturn;

		switch(eDataType)
		{
			case IMG_UINT_TYPE:
			{
				*uReturn.pui32Data = *uDefault.pui32Data;

				break;
			}
			case IMG_STRING_TYPE:
			{
				sceClibStrncpy(uReturn.pcData, uDefault.pcData, APPHINT_MAX_STRING_SIZE);

				break;
			}
			case IMG_INT_TYPE:
			default:
			{
				*uReturn.pi32Data = *uDefault.pi32Data;

				break;
			}
		}
	}

	return bFound;
}

/******************************************************************************
 Function Name      : PVRSRVCreateVirtualAppHint
 Inputs             : *psAppHint
 Outputs            :
 Returns            : Boolean - True if hint struct created, False if error
 Description        : Create virtual app hint file
******************************************************************************/
IMG_EXPORT IMG_BOOL PVRSRVCreateVirtualAppHint(PVRSRV_PSP2_APPHINT *psAppHint)
{
	if (!psAppHint)
		return IMG_FALSE;

	sceClibMemcpy(&s_appHint, psAppHint, sizeof(PVRSRV_PSP2_APPHINT));
	s_appHintCreated = IMG_TRUE;

	return IMG_TRUE;
}

/******************************************************************************
 Function Name      : PVRSRVInitializeAppHint
 Inputs             : *psAppHint
 Outputs            :
 Returns            : Boolean - True if hint struct created, False if error
 Description        : Sets default values for apphint struct
******************************************************************************/
IMG_EXPORT IMG_BOOL PVRSRVInitializeAppHint(PVRSRV_PSP2_APPHINT *psAppHint)
{
	if (!psAppHint)
		return IMG_FALSE;

	sceClibMemset(psAppHint, 0, sizeof(PVRSRV_PSP2_APPHINT));

	sceClibStrncpy(psAppHint->szGLES1, "app0:module/libGLESv1_CM.suprx", 256);
	sceClibStrncpy(psAppHint->szGLES2, "app0:module/libGLESv2.suprx", 256);
	sceClibStrncpy(psAppHint->szWindowSystem, "app0:module/libpvrPSP2_WSEGL.suprx", 256);
	psAppHint->bDisableMetricsOutput = IMG_TRUE;
	psAppHint->bDumpProfileData = IMG_FALSE;
	psAppHint->ui32ProfileStartFrame = 0;
	psAppHint->ui32ProfileEndFrame = 0;
	psAppHint->ui32ExternalZBufferMode = IMG_FALSE;
	psAppHint->ui32ExternalZBufferXSize = 0;
	psAppHint->ui32ExternalZBufferYSize = 0;
	psAppHint->ui32ParamBufferSize = 16 * 1024 * 1024;
	psAppHint->ui32PDSFragBufferSize = 50 * 1024;
	psAppHint->ui32DriverMemorySize = 4 * 1024 * 1024;

	psAppHint->bDisableHWTextureUpload = IMG_FALSE;
	psAppHint->bDisableHWTQBufferBlit = IMG_FALSE;
	psAppHint->bDisableHWTQMipGen = IMG_TRUE;
	psAppHint->bDisableHWTQNormalBlit = IMG_FALSE;
	psAppHint->bDisableHWTQTextureUpload = IMG_FALSE;
	psAppHint->bDisableStaticPDSPixelSAProgram = IMG_FALSE;
	psAppHint->bDisableUSEASMOPT = IMG_FALSE;
	psAppHint->bDumpShaders = IMG_FALSE;
	psAppHint->bEnableAppTextureDependency = IMG_FALSE;
	psAppHint->bEnableMemorySpeedTest = IMG_TRUE;
	psAppHint->bEnableStaticMTECopy = IMG_TRUE;
	psAppHint->bEnableStaticPDSVertex = IMG_TRUE;
	psAppHint->bFBODepthDiscard = IMG_TRUE;
	psAppHint->bOptimisedValidation = IMG_TRUE;
	psAppHint->ui32DefaultIndexBufferSize = 200 * 1024;
	psAppHint->ui32DefaultPDSVertBufferSize = 50 * 1024;
	psAppHint->ui32DefaultPregenMTECopyBufferSize = 50 * 1024;
	psAppHint->ui32DefaultPregenPDSVertBufferSize = 80 * 1024;
	psAppHint->ui32DefaultVDMBufferSize = 20 * 1024;
	psAppHint->ui32DefaultVertexBufferSize = 200 * 1024;
	psAppHint->ui32FlushBehaviour = 0;
	psAppHint->ui32MaxVertexBufferSize = 800 * 1024;
	psAppHint->ui32UNCTexHeapSize = 4 * 1024;
	psAppHint->ui32CDRAMTexHeapSize = 256 * 1024;
	psAppHint->bEnableUNCAutoExtend = IMG_TRUE;
	psAppHint->bEnableCDRAMAutoExtend = IMG_TRUE;
	psAppHint->ui32SwTexOpThreadNum = 1;
	psAppHint->ui32SwTexOpThreadPriority = 70;
	psAppHint->ui32SwTexOpThreadAffinity = 0;
	psAppHint->ui32SwTexOpMaxUltNum = 256;
	psAppHint->ui32SwTexOpCleanupDelay = 1000000;
	psAppHint->bDisableAsyncTextureOp = IMG_FALSE;
	psAppHint->ui32PrimitiveSplitThreshold = 1000;
	psAppHint->ui32MaxDrawCallsPerCore = 0;

	psAppHint->ui32AdjustShaderPrecision = 0;
	psAppHint->bDumpCompilerLogFiles = 0;
	psAppHint->bStrictBinaryVersionComparison = 1;
	psAppHint->fPolygonUnitsMultiplier = 1.0f;
	psAppHint->fPolygonFactorMultiplier = 1.0f;
	psAppHint->bDumpUSPOutput = 0;
	psAppHint->bDumpShaderAnalysis = 0;
	psAppHint->bTrackUSCMemory = 1;
	psAppHint->ui32OverloadTexLayout = 0;
	psAppHint->bInitialiseVSOutputs = 0;
	psAppHint->ui32TriangleSplitPixelThreshold = EURASIA_DEFAULT_TAG_TRIANGLE_SPLIT;
	psAppHint->bDynamicSplitCalc = 1;
	psAppHint->bAllowTrilinearNPOT = 1;
	psAppHint->bEnableVaryingPrecisionOpt = 1;
	psAppHint->ui32GLSLEnabledWarnings = 0x7FFFFFFF;

	return IMG_TRUE;
}