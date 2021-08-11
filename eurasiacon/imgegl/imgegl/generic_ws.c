/******************************************************************************
 * Name         : generic_ws.c
 * Title        : Generic window system functions
 * Author       : PowerVR
 * Created      : December 2003
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
 * Description  : Generic Window system functions
 *
 * Platform     : ANSI
 *
 * Version	    : $Revision: 1.91 $
 *
 * Modifications: 
 * $Log: generic_ws.c $
 *****************************************************************************/

#include "egl_internal.h"
#include "tls.h"
#include "srv.h"
#include "generic_ws.h"
#include "drvegl.h"


#if defined(API_MODULES_RUNTIME_CHECKED)
#	include <stdio.h>
#endif
#include <string.h>


#define MBX1_MINIMUM_RENDER_TARGET_ALIGNMENT	(4096)

#define PSP2_WSEGL_LIBNID						0x2D4F46F1
#define PSP2_WSEGL_GETFUNCTIONTABLEPOINTER_NID	0xB3B813C1

#define PSP2_OGLES1_LIBNID						0xF675728E
#define PSP2_OGLES1_GETSTRING_NID				0xF214B167

#define PSP2_OGLES2_LIBNID						0x626F9AB6
#define PSP2_OGLES2_GETSTRING_NID				0xF214B167

typedef struct SceKernelLibraryInfo { // size is 0x1C
	SceSize size; //!< sizeof(SceKernelLibraryInfo)
	uint16_t libver[2];
	uint32_t libnid;
	const char *libname;
	uint16_t nfunc;
	uint16_t nvar;
	uint32_t *nid_table;
	uint32_t *table_entry;
} SceKernelLibraryInfo;

int sceKernelGetLibraryInfoByNID(SceUID modid, SceUInt32 libnid, SceKernelLibraryInfo *pInfo);

static IMG_UINT32 _getFuncByNID(SceUID modid, SceUInt32 libnid, SceUInt32 funcnid)
{
	IMG_INT32 i = 0;
	IMG_INT32 ret;

	SceKernelLibraryInfo libinfo;
	sceClibMemset(&libinfo, 0, sizeof(SceKernelLibraryInfo));
	libinfo.size = sizeof(SceKernelLibraryInfo);

	ret = sceKernelGetLibraryInfoByNID(modid, libnid, &libinfo);
	if (ret != SCE_OK)
		return 0;

	while (libinfo.nid_table[i] != funcnid) {
		if (i > libinfo.nfunc)
			return 0;
		i++;
	}

	return libinfo.table_entry[i];
}


/***********************************************************************************
 Function Name      : LoadNamedWSModule
 Inputs             : pDisplay, pszWSModuleName, nativeDisplay
 Outputs            : phWSDrv
 Returns            : Handle
 Description        :
************************************************************************************/
static IMG_HANDLE LoadNamedWSModule(KEGL_DISPLAY *pDisplay, char *pszWSModuleName, NativeDisplayType nativeDisplay)
{
	IMG_HANDLE hWSDrv;
	WSEGL_FunctionTable *(*pfnWSEGL_GetFunctionTablePointer)(void);

	/*
	 * See if we can actually load the software module.
	 */
	hWSDrv = PVRSRVLoadLibrary(pszWSModuleName);

	if (!hWSDrv)
	{
		PVR_DPF((PVR_DBG_ERROR, "Couldn't load WS module %s", pszWSModuleName));
		goto Fail_WS_Load;
	}
	else
	{
		pfnWSEGL_GetFunctionTablePointer = _getFuncByNID((SceUID)hWSDrv, PSP2_WSEGL_LIBNID, PSP2_WSEGL_GETFUNCTIONTABLEPOINTER_NID);
		if (!pfnWSEGL_GetFunctionTablePointer)
		{
			PVR_DPF((PVR_DBG_ERROR, "LoadNamedWSModule: Couldn't get WSEGL_GetFunctionTablePointer fptr"));

			goto Fail_WS_Load;
		}

		/*
		 * Now call the function and actually get the table itself.
		 */
		pDisplay->pWSEGL_FT = pfnWSEGL_GetFunctionTablePointer();

		if (!pDisplay->pWSEGL_FT)
		{
			PVR_DPF ((PVR_DBG_ERROR,
				"LoadWSModule: Couldn't get function table for window system for %s",
				pszWSModuleName));

			goto Fail_WS_Load;
		}

		/*
		 * Check the version number is sensible.
		 */
		if (pDisplay->pWSEGL_FT->ui32WSEGLVersion != WSEGL_VERSION)
		{
			PVR_DPF ((PVR_DBG_ERROR,
				"LoadWSModule: Window system is wrong version %s = (%lu) -> OGLES = (%d)",
				pszWSModuleName,
				pDisplay->pWSEGL_FT->ui32WSEGLVersion,
				WSEGL_VERSION));

			goto Fail_WS_Load;
		}

		/*
		 * See if the module can validate the native display information.
		 */
		if (pDisplay->pWSEGL_FT->pfnWSEGL_IsDisplayValid(nativeDisplay) != WSEGL_SUCCESS)
		{
			PVR_DPF ((PVR_DBG_WARNING,
				"LoadWSModule: Window system module %s did not validate native display",
				pszWSModuleName));

			goto Fail_WS_Load;
		}
	}

	return hWSDrv;

Fail_WS_Load:
	/*
	 * Close library on failure.
	 */
	if (hWSDrv)
	{
		PVRSRVUnloadLibrary(hWSDrv);
	}

	pDisplay->pWSEGL_FT = IMG_NULL;

	return IMG_NULL;
}


/***********************************************************************************
 Function Name      : LoadWSModule
 Inputs             : pDisplay
 Outputs            : phWSDrv
 Returns            : Success/failure
 Description        : Load a suitable WS module.
************************************************************************************/
#if defined(__linux__)
	#define WS_DEFAULT  OPK_DEFAULT
	#define WS_NULL     OPK_FALLBACK
#elif defined(__psp2__)
	#define WS_DEFAULT  "app0:module/libpvrPSP2_WSEGL.suprx"
	#define WS_NULL     SCE_NULL
#else
				#error ("Unknown host operating system")
#endif

IMG_INTERNAL IMG_BOOL LoadWSModule(SrvSysContext *psSysContext,
								   KEGL_DISPLAY *psDisplay, 
								   IMG_HANDLE *phWSDrv, 
								   NativeDisplayType nativeDisplay,
								   IMG_CHAR *pszModuleName)
{
	IMG_CHAR *ppszWSModules[] = {0/*AppHint choice*/, WS_DEFAULT, WS_NULL};
	IMG_CHAR **ppsCurrentModule = &ppszWSModules[1];
	IMG_CHAR **ppsModuleListEnd = ppszWSModules + sizeof(ppszWSModules)/sizeof(IMG_CHAR*);

	PVR_UNREFERENCED_PARAMETER(psSysContext);

	if (pszModuleName[0]!='\0')
	{
		/* Found app hint, copy into array and start search from entry 0 */
		ppszWSModules[0] = pszModuleName;
		ppsCurrentModule = &ppszWSModules[0];
	}

	*phWSDrv = IMG_NULL;

	do
	{
		*phWSDrv = LoadNamedWSModule(psDisplay, *ppsCurrentModule, nativeDisplay);
		ppsCurrentModule++;
	}
	while((*phWSDrv == IMG_NULL) && (ppsCurrentModule < ppsModuleListEnd));

	return ((IMG_BOOL)(*phWSDrv != IMG_NULL));
}


/***********************************************************************************
 Function Name      : UnloadModule
 Inputs             : hModule
 Outputs            : None
 Returns            : Success/failure
 Description        :
************************************************************************************/
IMG_INTERNAL IMG_BOOL UnloadModule(IMG_HANDLE hModule)
{
	if (PVRSRVUnloadLibrary(hModule)==PVRSRV_OK)
	{
		return IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
}



#if defined(SUPPORT_OPENGLES1) || defined(API_MODULES_RUNTIME_CHECKED)

#if defined(PROFILE_COMMON)
	#define PROFILE_EXT "CM"
#else
	#define PROFILE_EXT "CL"
#endif

#if !defined(OGLES1_BASEPATH)
#define OGLES1_BASEPATH "app0:module/"
#endif

#if !defined(OGLES1_BASENAME)
#if defined(SUPPORT_OPENGLES1_V1) || defined(SUPPORT_OPENGLES1_V1_ONLY)
	#define OGLES1_BASENAME	"GLESv1_" PROFILE_EXT
#else /* defined(SUPPORT_OPENGLES1_V1) || defined(SUPPORT_OPENGLES1_V1_ONLY) */
	#define OGLES1_BASENAME	"GLES_" PROFILE_EXT
#endif /* defined(SUPPORT_OPENGLES1_V1) || defined(SUPPORT_OPENGLES1_V1_ONLY) */
#endif /* !defined(OGLES1_BASENAME) */

#if defined(__linux__)
	#define				OGLES1LIBNAME	OGLES1_BASEPATH "lib" OGLES1_BASENAME ".so"
#elif defined(__psp2__)
	#define				OGLES1LIBNAME	OGLES1_BASEPATH "lib" OGLES1_BASENAME ".suprx"
#else
				#error ("Unknown host operating system")
#endif

IMG_INTERNAL IMG_BOOL LoadOGLES1AndGetFunctions(EGLGlobal *psGlobalData)
{
	IMG_CHAR szAppHintPath[APPHINT_MAX_STRING_SIZE], szAppHintPathDefault[1];
	IMG_HANDLE hOGLModule;
	IMG_UINT8 *pszFunctionTable;
	IMG_UINT8 * (IMG_CALLCONV *pfnglGetString)(int name);

	szAppHintPathDefault[0] = '\0';

	if (PVRSRVGetAppHint(IMG_NULL, "GLES1", IMG_STRING_TYPE, &szAppHintPathDefault, szAppHintPath))
	{
		hOGLModule = PVRSRVLoadLibrary(szAppHintPath);
	}
	else
	{
		hOGLModule = PVRSRVLoadLibrary(OGLES1LIBNAME);
	}

	if (!hOGLModule)
	{
		PVR_DPF((PVR_DBG_ERROR, "LoadOGLES1AndGetFunctions: Couldn't load OGL module"));

		goto Fail_OGLES1_Load;
	}

	pfnglGetString = _getFuncByNID((SceUID)hOGLModule, PSP2_OGLES1_LIBNID, PSP2_OGLES1_GETSTRING_NID);
	if (!pfnglGetString)
	{
		PVR_DPF((PVR_DBG_ERROR, "LoadOGLES1AndGetFunctions: Couldn't get glGetString fptr"));

		goto Fail_OGLES1_Load;
	}

	pszFunctionTable = pfnglGetString(IMG_OGLES1_FUNCTION_TABLE);

	if (!pszFunctionTable)
	{
		PVR_DPF ((PVR_DBG_ERROR, "LoadOGLES1AndGetFunctions: Couldn't get function table"));

		goto Fail_OGLES1_Load;
	}

	PVRSRVMemCopy(&psGlobalData->spfnOGLES1, pszFunctionTable, sizeof(IMG_OGLES1EGL_Interface));

	if (psGlobalData->spfnOGLES1.ui32APIVersion != IMG_OGLES1_FUNCTION_TABLE_VERSION)
	{
		PVR_DPF ((PVR_DBG_ERROR, "LoadOGLES1AndGetFunctions: Wrong version. Got: %d, Expected %d",
				 psGlobalData->spfnOGLES1.ui32APIVersion, IMG_OGLES1_FUNCTION_TABLE_VERSION));

		goto Fail_OGLES1_Load;
	}

	psGlobalData->hOGLES1Drv			= hOGLModule;
	psGlobalData->bHaveOGLES1Functions 	= IMG_TRUE;

	return IMG_TRUE;

Fail_OGLES1_Load:
	/*
	 * Close library on failure.
	 */
	if (hOGLModule)
	{
		PVRSRVUnloadLibrary(hOGLModule);
	}

	psGlobalData->hOGLES1Drv			= IMG_NULL;
	psGlobalData->bHaveOGLES1Functions	= IMG_FALSE;

	return IMG_FALSE;
}
#endif /* defined(SUPPORT_OPENGLES1) || defined(API_MODULES_RUNTIME_CHECKED) */


#if defined(SUPPORT_OPENGLES2) || defined(API_MODULES_RUNTIME_CHECKED)

#if !defined(OGLES2_BASEPATH)
#define OGLES2_BASEPATH "app0:module/"
#endif

#if !defined(OGLES2_BASENAME)
#define OGLES2_BASENAME	"GLESv2"
#endif

#if defined(__linux__)
	#define             OGLES2LIBNAME   OGLES2_BASEPATH "lib" OGLES2_BASENAME ".so"
#elif defined(__psp2__)
	#define             OGLES2LIBNAME   OGLES2_BASEPATH "lib" OGLES2_BASENAME ".suprx"
#else
				#error ("Unknown host operating system")
#endif

IMG_INTERNAL IMG_BOOL LoadOGLES2AndGetFunctions(EGLGlobal *psGlobalData)
{
	IMG_CHAR szAppHintPath[APPHINT_MAX_STRING_SIZE], szAppHintPathDefault[1];
	IMG_HANDLE hOGLModule;
	IMG_UINT8 *pszFunctionTable;
	IMG_UINT8 * (IMG_CALLCONV *pfnglGetString)(int name);

	szAppHintPathDefault[0] = '\0';

	if (PVRSRVGetAppHint(IMG_NULL, "GLES2", IMG_STRING_TYPE, &szAppHintPathDefault, szAppHintPath))
	{
		hOGLModule = PVRSRVLoadLibrary(szAppHintPath);
	}
	else
	{
		hOGLModule = PVRSRVLoadLibrary(OGLES2LIBNAME);
	}

	if (!hOGLModule)
	{
		PVR_DPF((PVR_DBG_ERROR, "LoadOGLES2AndGetFunctions: Couldn't load OGL module"));
		
		goto Fail_OGLES2_Load;
	}

	pfnglGetString = _getFuncByNID((SceUID)hOGLModule, PSP2_OGLES2_LIBNID, PSP2_OGLES2_GETSTRING_NID);
	if (!pfnglGetString)
	{
		PVR_DPF((PVR_DBG_ERROR, "LoadOGLES2AndGetFunctions: Couldn't get glGetString fptr"));

		goto Fail_OGLES2_Load;
	}

	pszFunctionTable = pfnglGetString(IMG_OGLES2_FUNCTION_TABLE);

	if (!pszFunctionTable)
	{
		PVR_DPF ((PVR_DBG_ERROR, "LoadOGLES2AndGetFunctions: Couldn't get function table"));

		goto Fail_OGLES2_Load;
	}

	PVRSRVMemCopy(&psGlobalData->spfnOGLES2, pszFunctionTable, sizeof(IMG_OGLES2EGL_Interface));

	if (psGlobalData->spfnOGLES2.ui32APIVersion != IMG_OGLES2_FUNCTION_TABLE_VERSION)
	{
		PVR_DPF ((PVR_DBG_ERROR, "LoadOGLES2AndGetFunctions: Wrong version. Got: %d, Expected %d",
				 psGlobalData->spfnOGLES2.ui32APIVersion, IMG_OGLES2_FUNCTION_TABLE_VERSION));

		goto Fail_OGLES2_Load;
	}

	psGlobalData->hOGLES2Drv			= hOGLModule;
	psGlobalData->bHaveOGLES2Functions	= IMG_TRUE;

	return IMG_TRUE;

Fail_OGLES2_Load:
	/*
	 * Close library on failure.
	 */
	if (hOGLModule)
	{
		PVRSRVUnloadLibrary(hOGLModule);
	}

	psGlobalData->hOGLES2Drv			= IMG_NULL;
	psGlobalData->bHaveOGLES2Functions	= IMG_FALSE;

	return IMG_FALSE;
}
#endif /* defined(SUPPORT_OPENGLES2) || defined(API_MODULES_RUNTIME_CHECKED) */


#if defined(SUPPORT_OPENGL) || defined(API_MODULES_RUNTIME_CHECKED)

#if defined(__linux__)
#define OGLLIBNAME	"libPVROGL.so"
#else
				#error ("Unknown host operating system")
#endif

IMG_INTERNAL IMG_BOOL LoadOGLAndGetFunctions(EGLGlobal *psGlobalData)
{
	IMG_HANDLE hOGLModule;
	IMG_UINT8 *pszFunctionTable;
	IMG_UINT8 * (IMG_CALLCONV *pfnglGetString)(int name);

	hOGLModule = PVRSRVLoadLibrary(OGLLIBNAME);

	if (!hOGLModule)
	{
		PVR_DPF((PVR_DBG_ERROR, "LoadOGLAndGetFunctions: Couldn't load OGL module "OGLLIBNAME));

		goto Fail_OGL_Load;
	}

	if (PVRSRVGetLibFuncAddr(hOGLModule, "glGetString", (IMG_VOID **)&pfnglGetString)!=PVRSRV_OK)
	{
		PVR_DPF ((PVR_DBG_ERROR, "LoadOGLAndGetFunctions: Couldn't get address of glGetString"));

		goto Fail_OGL_Load;
	}

	pszFunctionTable = pfnglGetString(IMG_OGL_FUNCTION_TABLE);

	if (!pszFunctionTable)
	{
		PVR_DPF ((PVR_DBG_ERROR, "LoadOGLAndGetFunctions: Couldn't get function table"));

		goto Fail_OGL_Load;
	}

	PVRSRVMemCopy(&psGlobalData->spfnOGL, pszFunctionTable, sizeof(IMG_OGLEGL_Interface));

	if (psGlobalData->spfnOGL.ui32APIVersion != IMG_OGL_FUNCTION_TABLE_VERSION)
	{
		PVR_DPF ((PVR_DBG_ERROR, "LoadOGLAndGetFunctions: Wrong version. Got: %d, Expected %d",
				 psGlobalData->spfnOGL.ui32APIVersion, IMG_OGL_FUNCTION_TABLE_VERSION));

		goto Fail_OGL_Load;
	}

	psGlobalData->hOGLDrv				= hOGLModule;
	psGlobalData->bHaveOGLFunctions		= IMG_TRUE;

	return IMG_TRUE;

Fail_OGL_Load:
	/*
	 * Close library on failure.
	 */
	if (hOGLModule)
	{
		PVRSRVUnloadLibrary(hOGLModule);
	}

	psGlobalData->hOGLDrv				= IMG_NULL;
	psGlobalData->bHaveOGLFunctions		= IMG_FALSE;

	return IMG_FALSE;
}
#endif /* defined(SUPPORT_OPENGL) || defined(API_MODULES_RUNTIME_CHECKED) */

#if defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX) || defined(API_MODULES_RUNTIME_CHECKED)

#if defined(__linux__)
	#define OVGLIBNAME	"libOpenVG.so"
#else
				#error ("Unknown host operating system")
#endif

IMG_BOOL IMG_INTERNAL LoadOVGAndGetFunctions(EGLGlobal *psGlobalData)
{
	IMG_HANDLE hOVGModule;
	IMG_UINT8 *pszFunctionTable;
	IMG_UINT8 *(IMG_CALLCONV *pfnvgGetString)(int name);

	hOVGModule = PVRSRVLoadLibrary(OVGLIBNAME);

	if (!hOVGModule)
	{
		PVR_DPF((PVR_DBG_ERROR, "LoadOVGAndGetFunctions: Couldn't load OVG module "OVGLIBNAME));
		goto Fail_OVG_Load;
	}

	if (PVRSRVGetLibFuncAddr(hOVGModule, "vgGetString", (IMG_VOID**)&pfnvgGetString)!=PVRSRV_OK)
	{
		PVR_DPF ((PVR_DBG_ERROR, "LoadOVGAndGetFunctions: Couldn't get address of vgGetString"));
		goto Fail_OVG_Load;
	}

	pszFunctionTable = pfnvgGetString(IMG_OVG_FUNCTION_TABLE);

	if (!pszFunctionTable)
	{
		PVR_DPF ((PVR_DBG_ERROR, "LoadOVGAndGetFunctions: Couldn't get function table"));
		goto Fail_OVG_Load;
	}

	PVRSRVMemCopy(&psGlobalData->spfnOVG, pszFunctionTable, sizeof(IMG_OVGEGL_Interface));

	if (psGlobalData->spfnOVG.ui32APIVersion != IMG_OVG_FUNCTION_TABLE_VERSION)
	{
		PVR_DPF ((PVR_DBG_ERROR, "LoadOVGAndGetFunctions: Wrong version. Got: %d, Expected %d",
				 psGlobalData->spfnOVG.ui32APIVersion, IMG_OVG_FUNCTION_TABLE_VERSION));

		goto Fail_OVG_Load;
	}

	psGlobalData->hOVGDrv           = hOVGModule;
	psGlobalData->bHaveOVGFunctions = IMG_TRUE;

	return IMG_TRUE;

Fail_OVG_Load:
	/*
	 * Close library on failure.
	 */
	if (hOVGModule)
	{
		PVRSRVUnloadLibrary(hOVGModule);
	}

	psGlobalData->hOVGDrv           = IMG_NULL;
	psGlobalData->bHaveOVGFunctions = IMG_FALSE;

	return IMG_FALSE;
}

#endif /* defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX) || defined(API_MODULES_RUNTIME_CHECKED) */

#if defined(SUPPORT_OPENCL) || defined(API_MODULES_RUNTIME_CHECKED)

#if defined(__linux__)
	#define OCLLIBNAME	"libPVROCL.so"
#else
				#error ("Unknown host operating system")
#endif

IMG_BOOL IMG_INTERNAL LoadOCLAndGetFunctions(EGLGlobal *psGlobalData)
{
	IMG_HANDLE hOCLModule;
	IMG_UINT (IMG_CALLCONV *pfnclGetContextInfo)(void *, 
                 	 							 IMG_UINT32, 
                 	 							 IMG_UINT32, 
                 	 							 void *,
                 								 IMG_UINT32 *);

	hOCLModule = PVRSRVLoadLibrary(OCLLIBNAME);

	if (!hOCLModule)
	{
		PVR_DPF((PVR_DBG_ERROR, "LoadOCLAndGetFunctions: Couldn't load OCL module "OCLLIBNAME));
		goto Fail_OCL_Load;
	}

	if (PVRSRVGetLibFuncAddr(hOCLModule, "clGetContextInfo", (IMG_VOID**)&pfnclGetContextInfo)!=PVRSRV_OK)
	{
		PVR_DPF ((PVR_DBG_ERROR, "LoadOCLAndGetFunctions: Couldn't get address of clGetContextInfo"));
		goto Fail_OCL_Load;
	}

	pfnclGetContextInfo(NULL, IMG_OCL_FUNCTION_TABLE, 0, &psGlobalData->spfnOCL, NULL);

	if (psGlobalData->spfnOCL.ui32APIVersion != IMG_OCL_FUNCTION_TABLE_VERSION)
	{
		PVR_DPF ((PVR_DBG_ERROR, "LoadOCLAndGetFunctions: Wrong version. Got: %d, Expected %d",
				 psGlobalData->spfnOCL.ui32APIVersion, IMG_OCL_FUNCTION_TABLE_VERSION));

		goto Fail_OCL_Load;
	}

	psGlobalData->hOCLDrv           = hOCLModule;
	psGlobalData->bHaveOCLFunctions = IMG_TRUE;

	return IMG_TRUE;

Fail_OCL_Load:
	/*
	 * Close library on failure.
	 */
	if (hOCLModule)
	{
		PVRSRVUnloadLibrary(hOCLModule);
	}

	psGlobalData->hOCLDrv           = IMG_NULL;
	psGlobalData->bHaveOCLFunctions = IMG_FALSE;

	return IMG_FALSE;
}

#endif /* defined(SUPPORT_OPENCL) || defined(API_MODULES_RUNTIME_CHECKED) */

#if defined(API_MODULES_RUNTIME_CHECKED) 
#if defined(API_MODULES_RUNTIME_CHECK_BY_NAME)
IMG_INTERNAL IMG_BOOL IsOGLES1ModulePresent(void)
{
	return ENV_FileExists(OGLES1LIBNAME);
}
IMG_INTERNAL IMG_BOOL IsOGLES2ModulePresent(void)
{
	return ENV_FileExists(OGLES2LIBNAME);
}
IMG_INTERNAL IMG_BOOL IsOGLModulePresent(void)
{
	return ENV_FileExists(OGLLIBNAME);
}
IMG_INTERNAL IMG_BOOL IsOVGModulePresent(void)
{
	return ENV_FileExists(OVGLIBNAME);
}
IMG_INTERNAL IMG_BOOL IsOCLModulePresent(void)
{
	return ENV_FileExists(OCLLIBNAME);
}

#else /* defined(API_MODULES_RUNTIME_CHECK_BY_NAME) */
IMG_INTERNAL IMG_BOOL IsOGLES1ModulePresent(void)
{

	IMG_HANDLE module = PVRSRVLoadLibrary(OGLES1LIBNAME);

	if (!module)
	{
		return IMG_FALSE;
	}
	PVRSRVUnloadLibrary(module);

	return IMG_TRUE;
}

IMG_INTERNAL IMG_BOOL IsOGLES2ModulePresent(void)
{
	IMG_HANDLE module = PVRSRVLoadLibrary(OGLES2LIBNAME);

	if (!module)
	{
		return IMG_FALSE;
	}
	PVRSRVUnloadLibrary(module);

	return IMG_TRUE;
}

IMG_INTERNAL IMG_BOOL IsOGLModulePresent(void)
{
	IMG_HANDLE module = PVRSRVLoadLibrary(OGLLIBNAME);

	if (!module)
	{
		return IMG_FALSE;
	}
	PVRSRVUnloadLibrary(module);

	return IMG_TRUE;
}

IMG_INTERNAL IMG_BOOL IsOVGModulePresent(void)
{
	IMG_HANDLE module = PVRSRVLoadLibrary(OVGLIBNAME);

	if (!module)
	{
		return IMG_FALSE;
	}
	PVRSRVUnloadLibrary(module);

	return IMG_TRUE;
}

IMG_INTERNAL IMG_BOOL IsOCLModulePresent(void)
{
	IMG_HANDLE module = PVRSRVLoadLibrary(OCLLIBNAME);

	if (!module)
	{
		return IMG_FALSE;
	}
	PVRSRVUnloadLibrary(module);

	return IMG_TRUE;
}
#endif /* defined(API_MODULES_RUNTIME_CHECK_BY_NAME)*/
#endif /* defined(API_MODULES_RUNTIME_CHECKED) */

/***********************************************************************************
 Function Name      : KEGLRecreateDrawable
 Inputs             : psSurface
 Outputs            : None
 Returns            : Success/Failure
 Description        : Attempts to recreate a bad drawable
************************************************************************************/
static IMG_BOOL KEGLRecreateDrawable(KEGL_SURFACE *psSurface, 
									 IMG_BOOL      bAllowSurfaceRecreate)
{
	EGLDrawableParams   sParams;
	KEGL_DISPLAY        *psDpy;
	WSEGLError          eError;
	TLS                 psTls;
	KEGL_CONTEXT        *psContext;
	SrvSysContext		*psSysContext;
#if defined(API_MODULES_RUNTIME_CHECKED)
	EGLGlobal           *psGlobalData = ENV_GetGlobalData();

	PVR_ASSERT(psGlobalData != IMG_NULL)
#endif

	psTls = TLS_Open(_TlsInit);
	if (psTls==IMG_NULL)
	{
		return IMG_FALSE;
	}

	psDpy = psSurface->psDpy;
	psContext = psTls->apsCurrentContext[psTls->ui32API];
	psSysContext = &psTls->psGlobalData->sSysContext;

	if(psContext)
	{
		switch (psContext->eContextType)
		{
			case IMGEGL_CONTEXT_OPENGLES1:
			{
#               if defined(SUPPORT_OPENGLES1) || defined(API_MODULES_RUNTIME_CHECKED)
				{
#                   if defined(API_MODULES_RUNTIME_CHECKED)
					if(psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENGLES1])
#                   endif
					{
						/* Indicate to OGLES that its render surface pointer is invalid */
						if (psContext->hClientContext)
						{
							psTls->psGlobalData->spfnOGLES1.pfnGLESMarkRenderSurfaceAsInvalid(psContext->hClientContext);
						}
					}
				}
#               endif /* defined(SUPPORT_OPENGLES1) */

				break;
			}
			case IMGEGL_CONTEXT_OPENGLES2:
			{
#               if defined(SUPPORT_OPENGLES2) || defined(API_MODULES_RUNTIME_CHECKED)
				{
#                   if defined(API_MODULES_RUNTIME_CHECKED)
					if(psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENGLES2])
#                   endif
					{
						/* Indicate to OGLES that its render surface pointer is invalid */
						if (psContext->hClientContext)
						{
							psTls->psGlobalData->spfnOGLES2.pfnGLESMarkRenderSurfaceAsInvalid(psContext->hClientContext);
						}
					}
				}
#               endif /* defined(SUPPORT_OPENGLES2) */

				break;
			}

			case IMGEGL_CONTEXT_OPENGL:
			{
#               if defined(SUPPORT_OPENGL) || defined(API_MODULES_RUNTIME_CHECKED)
				{
#                   if defined(API_MODULES_RUNTIME_CHECKED)
					if(psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENGL])
#                   endif
					{
						/* Indicate to OGL that its render surface pointer is invalid */
						if (psContext->hClientContext)
						{
							psTls->psGlobalData->spfnOGL.pfnGLMarkRenderSurfaceAsInvalid(psContext->hClientContext);
						}
					}
				}
#               endif /* defined(SUPPORT_OPENGL) */

				break;
			}

			case IMGEGL_CONTEXT_OPENVG:
			{
#               if defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX)
				{
#                   if defined(API_MODULES_RUNTIME_CHECKED)
					if(psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENVG])
#                   endif
					{
						/*#warning is not x-platform: #warning OPENVG CHANGE NEEDED HERE*/
						/* Indicate to OVG that its render surface pointer is invalid */
						if (psContext->hClientContext)
						{
							psTls->psGlobalData->spfnOVG.pfnOVGMarkRenderSurfaceAsInvalid(psContext->hClientContext);
						}
					}
				}
#               endif /* defined(SUPPORT_OPENVG) */
				break;
			}
			default:
			{
				PVR_DPF((PVR_DBG_WARNING,"KEGLRecreateDrawable: Invalid client API"));

				return IMG_FALSE;
			}
		}
	}

	switch (psSurface->type)
	{
		case EGL_SURFACE_WINDOW:
		{
			WSEGLDrawableHandle hNewDrawable;
			WSEGLRotationAngle  eNewRotation;

			EGLThreadLockWSEGL(psDpy, psTls);

			/* Try to create new drawable,
			 * if it fails we keep the old one.
			 */
			eError = psDpy->pWSEGL_FT->pfnWSEGL_CreateWindowDrawable(psDpy->hDisplay, &psSurface->u.window.sConfig,
																	 &hNewDrawable,
																	 psSurface->u.window.native,
																	 &eNewRotation,
																	 psSysContext->psConnection);
			if (eError!=WSEGL_SUCCESS)
			{
				PVR_DPF((PVR_DBG_ERROR,"KEGLRecreateDrawable: Couldn't create a drawable"));

				EGLThreadUnlockWSEGL(psDpy, psTls);

				return IMG_FALSE;
			}

			if(psSurface->u.window.hDrawable)
			{
				eError = psSurface->psDpy->pWSEGL_FT->pfnWSEGL_DeleteDrawable(psSurface->u.window.hDrawable);
				if (eError!=WSEGL_SUCCESS)
				{
					/* Ignore the error, there is nothing we can do */
					PVR_DPF((PVR_DBG_ERROR,"KEGLRecreateDrawable: Couldn't delete a drawable"));
				}

			}

			EGLThreadUnlockWSEGL(psDpy, psTls);

			EGLThreadLock(psTls);
			/* success, assign new drawable */
			psSurface->u.window.hDrawable = hNewDrawable;
			psSurface->eRotationAngle = eNewRotation;
			EGLThreadUnlock(psTls);

			break;
		}
		default:
		{
			WSEGLDrawableHandle hNewDrawable;
			WSEGLRotationAngle  eNewRotation;

			EGLThreadLockWSEGL(psDpy, psTls);

			/* Try to create new drawable, 
			 * if it fails we keep the old one.
			 */	
			eError = psDpy->pWSEGL_FT->pfnWSEGL_CreatePixmapDrawable(psDpy->hDisplay, 
																	 &psSurface->u.pixmap.sConfig,
																	 &hNewDrawable,
																	 psSurface->u.pixmap.native,
																	 &eNewRotation);
			if (eError!=WSEGL_SUCCESS)
			{
				PVR_DPF((PVR_DBG_ERROR,"KEGLRecreateDrawable: Couldn't create a drawable"));

				EGLThreadUnlockWSEGL(psDpy, psTls);

				return IMG_FALSE;
			}

			if(psSurface->u.pixmap.hDrawable)
			{
				eError = psSurface->psDpy->pWSEGL_FT->pfnWSEGL_DeleteDrawable(psSurface->u.pixmap.hDrawable);
				if (eError!=WSEGL_SUCCESS)
				{
					/* Ignore the error, there is nothing we can do */
					PVR_DPF((PVR_DBG_ERROR,"KEGLRecreateDrawable: Couldn't delete a drawable"));
				}
			}

			EGLThreadUnlockWSEGL(psDpy, psTls);

			EGLThreadLock(psTls);
			/* success, assign new drawable */
			psSurface->u.pixmap.hDrawable = hNewDrawable;
			psSurface->eRotationAngle = eNewRotation;
			EGLThreadUnlock(psTls);

			break;
		}
	}

	if(bAllowSurfaceRecreate)
	{
		if(!KEGLGetDrawableParameters(psSurface, &sParams, IMG_FALSE))
		{
			PVR_DPF((PVR_DBG_ERROR,"KEGLRecreateDrawable: KEGLGetDrawableParameters() failed"));
			return IMG_FALSE;
		}

		if(!KEGLResizeRenderSurface(&psTls->psGlobalData->sSysContext, &sParams, psSurface->sRenderSurface.bMultiSample, IMG_FALSE, &psSurface->sRenderSurface))
		{
			PVR_DPF((PVR_DBG_ERROR,"KEGLRecreateDrawable: KEGLResizeRenderSurface() failed"));
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}


/***********************************************************************
 *
 *  FUNCTION   : KEGLGetDrawableParameters
 *  PURPOSE    : Get values for the drawable for the current context.
 *  PARAMETERS : In:  hDrawable - drawable
 *               Out: ppsParams - parameter struct to be filled out
 *               In:  bAllowDrawableRecreate - Allow recreation of drawable
 *  RETURNS    : IMG_BOOL
 *
 ***********************************************************************/
IMG_EXPORT IMG_BOOL IMG_CALLCONV KEGLGetDrawableParameters(EGLDrawableHandle hDrawable,
														   EGLDrawableParams *ppsParams,
														   IMG_BOOL bAllowSurfaceRecreate)
{
	KEGL_SURFACE *pSurface;

	if (!hDrawable)
	{
		return IMG_FALSE;
	}

	pSurface = hDrawable;
#if defined(ANDROID)
	ppsParams->bWaitForRender = IMG_FALSE;
#endif

	if (pSurface->type==EGL_SURFACE_PBUFFER)
	{
		ppsParams->ui32Width            = pSurface->u.pbuffer.ui32PixelWidth;
		ppsParams->ui32Height           = pSurface->u.pbuffer.ui32PixelHeight;
		ppsParams->ui32Stride           = pSurface->u.pbuffer.ui32ByteStride;
		ppsParams->pvLinSurfaceAddress  = pSurface->u.pbuffer.psMemInfo->pvLinAddr;
		ppsParams->ui32HWSurfaceAddress = pSurface->u.pbuffer.psMemInfo->sDevVAddr.uiAddr;
		ppsParams->ePixelFormat         = pSurface->u.pbuffer.ePixelFormat;
		ppsParams->psRenderSurface      = &pSurface->sRenderSurface;
		ppsParams->eDrawableType        = EGL_DRAWABLETYPE_PBUFFER;
		ppsParams->eRotationAngle       = PVRSRV_FLIP_Y;
		ppsParams->psSyncInfo           = pSurface->sRenderSurface.psSyncInfo;

		/* Use same surface for accumulation */
		ppsParams->eAccumPixelFormat    = ppsParams->ePixelFormat;
		ppsParams->ui32AccumStride      = ppsParams->ui32Stride;
		ppsParams->ui32AccumHWAddress   = ppsParams->ui32HWSurfaceAddress;
		ppsParams->psAccumSyncInfo      = ppsParams->psSyncInfo;

	}
	else
	{
		WSEGLDrawableParams sSourceParams, sRenderParams;
		WSEGLDrawableHandle hDrawable;
		WSEGLError eError;
		TLS pTls;

		pTls = IMGEGLGetTLSValue();

		if (pTls==IMG_NULL)
		{
			PVR_DPF((PVR_DBG_ERROR,"KEGLGetDrawableParameters: No Current thread"));
			return IMG_FALSE;
		}

		if (pSurface->type==EGL_SURFACE_WINDOW)
		{
			hDrawable = pSurface->u.window.hDrawable;
			ppsParams->eDrawableType = EGL_DRAWABLETYPE_WINDOW;
		}
		else
		{
			hDrawable = pSurface->u.pixmap.hDrawable;
			ppsParams->eDrawableType = EGL_DRAWABLETYPE_PIXMAP;
		}

		if(hDrawable == IMG_NULL)
		{
			if (!KEGLRecreateDrawable(pSurface, bAllowSurfaceRecreate))
			{
				PVR_DPF((PVR_DBG_ERROR,"KEGLGetDrawableParameters: Can't recreate drawable"));
				return IMG_FALSE;
			}

			if (pSurface->type==EGL_SURFACE_WINDOW)
			{
				hDrawable = pSurface->u.window.hDrawable;
			}
			else
			{
				hDrawable = pSurface->u.pixmap.hDrawable;
			}
		}

		EGLThreadLockWSEGL(pSurface->psDpy, pTls);
		eError = pSurface->psDpy->pWSEGL_FT->pfnWSEGL_GetDrawableParameters(hDrawable, &sSourceParams, &sRenderParams);
		EGLThreadUnlockWSEGL(pSurface->psDpy, pTls);

		if (eError!=WSEGL_SUCCESS)
		{
			if ((eError==WSEGL_BAD_NATIVE_WINDOW) || (eError==WSEGL_BAD_NATIVE_PIXMAP))
			{
				PVR_DPF((PVR_DBG_ERROR,"KEGLGetDrawableParameters: Native %s is invalid", (eError==WSEGL_BAD_NATIVE_WINDOW)?"window":"pixmap"));

				return IMG_FALSE;
			}

			/* The error code might be WSEGL_BAD_DRAWABLE or WSEGL_RETRY by
			 * now, but we'll only run the following loop more than once if
			 * WSEGL_RETRY is returned subsequently.
			 */
			do
			{
				if (!KEGLRecreateDrawable(pSurface, bAllowSurfaceRecreate))
				{
					PVR_DPF((PVR_DBG_ERROR,"KEGLGetDrawableParameters: Can't recreate drawable"));

					return IMG_FALSE;
				}	

				if (pSurface->type==EGL_SURFACE_WINDOW)
				{
					hDrawable = pSurface->u.window.hDrawable;
				}
				else
				{
					hDrawable = pSurface->u.pixmap.hDrawable;
				}

				EGLThreadLockWSEGL(pSurface->psDpy, pTls);
				eError = pSurface->psDpy->pWSEGL_FT->pfnWSEGL_GetDrawableParameters(hDrawable, &sSourceParams, &sRenderParams);
				EGLThreadUnlockWSEGL(pSurface->psDpy, pTls);
			}
			while(eError==WSEGL_RETRY);

			if (eError!=WSEGL_SUCCESS)
			{
				PVR_DPF((PVR_DBG_ERROR,"KEGLGetDrawableParameters: Can't get drawable params"));

				return IMG_FALSE;
			}
		}

		if((sRenderParams.ui32Width == 0) || (sRenderParams.ui32Height == 0) ||
			(sRenderParams.ui32Width > (IMG_UINT32) CFGC_GetAttrib(pSurface->psCfg, EGL_MAX_PBUFFER_WIDTH)) ||
			(sRenderParams.ui32Height > (IMG_UINT32)CFGC_GetAttrib(pSurface->psCfg, EGL_MAX_PBUFFER_HEIGHT)))
		{
			PVR_DPF((PVR_DBG_ERROR,"KEGLGetDrawableParameters: Out of range render param width = %lu, height = %lu",
					sRenderParams.ui32Width, sRenderParams.ui32Height));

			return IMG_FALSE;
		}

		ppsParams->ui32Width =              sRenderParams.ui32Width;
		ppsParams->ui32Height =             sRenderParams.ui32Height;
		ppsParams->ui32Stride =             sRenderParams.ui32Stride;

		/* Use Src params for accumulation */
		ppsParams->ui32AccumWidth =			sSourceParams.ui32Width;
		ppsParams->ui32AccumHeight =		sSourceParams.ui32Height;
		ppsParams->ui32AccumStride =        sSourceParams.ui32Stride;
		ppsParams->ui32AccumHWAddress =     sSourceParams.ui32HWAddress;

		if (sSourceParams.hPrivateData)
		{
#if defined(SGX_SYNCHRONOUS_TC_FLIPPING)
			ppsParams->psAccumSyncInfo = (PVRSRV_CLIENT_SYNC_INFO *)((PVRSRV_CLIENT_MEM_INFO *)(sSourceParams.hPrivateData))->hKernelMemInfo;
#else
			ppsParams->psAccumSyncInfo = ((PVRSRV_CLIENT_MEM_INFO *)(sSourceParams.hPrivateData))->psClientSyncInfo;
#endif
		}
		else
		{
			ppsParams->psAccumSyncInfo = pSurface->sRenderSurface.psSyncInfo;
		}

		/* Convert WSEGLPixelFormat to PVRSRV_PIXEL_FORMAT */
		switch(sRenderParams.ePixelFormat)
		{
			case WSEGL_PIXELFORMAT_ARGB8888:
			{
				ppsParams->ePixelFormat = PVRSRV_PIXEL_FORMAT_ARGB8888;
				ppsParams->ui32Stride <<= 2;
				break;
			}
			case WSEGL_PIXELFORMAT_ABGR8888:
			{
				ppsParams->ePixelFormat = PVRSRV_PIXEL_FORMAT_ABGR8888;
				ppsParams->ui32Stride <<= 2;
				break;
			}
			case WSEGL_PIXELFORMAT_XRGB8888:
			{
				ppsParams->ePixelFormat = PVRSRV_PIXEL_FORMAT_XRGB8888;
				ppsParams->ui32Stride <<= 2;
				break;
			}
			case WSEGL_PIXELFORMAT_XBGR8888:
			{
				ppsParams->ePixelFormat = PVRSRV_PIXEL_FORMAT_XBGR8888;
				ppsParams->ui32Stride <<= 2;
				break;
			}
			case WSEGL_PIXELFORMAT_1555:
			{
				ppsParams->ePixelFormat = PVRSRV_PIXEL_FORMAT_ARGB1555;
				ppsParams->ui32Stride <<= 1;
				break;
			}
			case WSEGL_PIXELFORMAT_4444:
			{
				ppsParams->ePixelFormat = PVRSRV_PIXEL_FORMAT_ARGB4444;
				ppsParams->ui32Stride <<= 1;
				break;
			}
			default:
			{
				ppsParams->ePixelFormat = PVRSRV_PIXEL_FORMAT_RGB565;
				ppsParams->ui32Stride <<= 1;
				break;
			}
		}

		/* Convert WSEGLPixelFormat to PVRSRV_PIXEL_FORMAT */
		switch(sSourceParams.ePixelFormat)
		{
			case WSEGL_PIXELFORMAT_ARGB8888:
			{
				ppsParams->eAccumPixelFormat = PVRSRV_PIXEL_FORMAT_ARGB8888;
				ppsParams->ui32AccumStride <<= 2;
				break;
			}
			case WSEGL_PIXELFORMAT_ABGR8888:
			{
				ppsParams->eAccumPixelFormat = PVRSRV_PIXEL_FORMAT_ABGR8888;
				ppsParams->ui32AccumStride <<= 2;
				break;
			}
			case WSEGL_PIXELFORMAT_XRGB8888:
			{
				ppsParams->eAccumPixelFormat = PVRSRV_PIXEL_FORMAT_XRGB8888;
				ppsParams->ui32AccumStride <<= 2;
				break;
			}
			case WSEGL_PIXELFORMAT_XBGR8888:
			{
				ppsParams->eAccumPixelFormat = PVRSRV_PIXEL_FORMAT_XBGR8888;
				ppsParams->ui32AccumStride <<= 2;
				break;
			}
			case WSEGL_PIXELFORMAT_1555:
			{
				ppsParams->eAccumPixelFormat = PVRSRV_PIXEL_FORMAT_ARGB1555;
				ppsParams->ui32AccumStride <<= 1;
				break;
			}
			case WSEGL_PIXELFORMAT_4444:
			{
				ppsParams->eAccumPixelFormat = PVRSRV_PIXEL_FORMAT_ARGB4444;
				ppsParams->ui32AccumStride <<= 1;
				break;
			}
			default:
			{
				ppsParams->eAccumPixelFormat = PVRSRV_PIXEL_FORMAT_RGB565;
				ppsParams->ui32AccumStride <<= 1;
				break;
			}
		}

		/* Convert WSEGLRotationAngle to GLESRotationAngle. WSEGL rotation is specified as anti-clockwise,
			PVRSRV_ROTATION is specified as clockwise. WSEGL provides the width and height of the rotated window,
			whereas client drivers require the original width and height of the window
		 */
		switch(pSurface->eRotationAngle)
		{
			case WSEGL_ROTATE_90:
			{
				ppsParams->eRotationAngle = PVRSRV_ROTATE_270;
				ppsParams->ui32Width = sRenderParams.ui32Height;
				ppsParams->ui32Height = sRenderParams.ui32Width;
				break;
			}
			case WSEGL_ROTATE_180:
			{
				ppsParams->eRotationAngle = PVRSRV_ROTATE_180;
				break;
			}
			case WSEGL_ROTATE_270:
			{
				ppsParams->eRotationAngle = PVRSRV_ROTATE_90;
				ppsParams->ui32Width = sRenderParams.ui32Height;
				ppsParams->ui32Height = sRenderParams.ui32Width;
				break;
			}
			default: /* WSEGL_ROTATE_0 */
			{
				ppsParams->eRotationAngle = PVRSRV_ROTATE_0;
				break;
			}
		}

		/* FIXME: This shouldn't be Android-specific, but we need to find
		 * a way to keep eAccumRotationAngle backwards compatible (e.g.
		 * if it is not set but pSurface->eRotationAngle != 0).
		 */
		switch(sSourceParams.eRotationAngle)
		{
			case WSEGL_ROTATE_90:
			{
				ppsParams->eAccumRotationAngle = PVRSRV_ROTATE_270;
				ppsParams->ui32AccumWidth = sSourceParams.ui32Height;
				ppsParams->ui32AccumHeight = sSourceParams.ui32Width;
				break;
			}
			case WSEGL_ROTATE_180:
			{
				ppsParams->eAccumRotationAngle = PVRSRV_ROTATE_180;
				break;
			}
			case WSEGL_ROTATE_270:
			{
				ppsParams->eAccumRotationAngle = PVRSRV_ROTATE_90;
				ppsParams->ui32AccumWidth = sSourceParams.ui32Height;
				ppsParams->ui32AccumHeight = sSourceParams.ui32Width;
				break;
			}
			default: /* WSEGL_ROTATE_0 */
			{
				ppsParams->eAccumRotationAngle = PVRSRV_ROTATE_0;
				break;
			}
		}

		ppsParams->psRenderSurface =        &pSurface->sRenderSurface;
		ppsParams->pvLinSurfaceAddress =    sRenderParams.pvLinearAddress;
		ppsParams->ui32HWSurfaceAddress =   sRenderParams.ui32HWAddress;

		if (sRenderParams.hPrivateData)
		{
			ppsParams->psSyncInfo = ((PVRSRV_CLIENT_MEM_INFO *)(sRenderParams.hPrivateData))->psClientSyncInfo;
		}
		else
		{
			ppsParams->psSyncInfo =	pSurface->sRenderSurface.psSyncInfo;
		}

#if defined(ANDROID)
		ppsParams->bWaitForRender = sRenderParams.bWaitForRender;
#endif
	}

	return IMG_TRUE;
}


/***********************************************************************
 *
 *  FUNCTION   : GWS_CreatePBufferDrawable
 *  PURPOSE    : Create a pbuffer drawable.
 *  PARAMETERS :
 *    In:  services_context - Services context.
 *    In:  pSurface - Surface.
 *    In:  pbuffer_width - Required width in pixels.
 *    In:  pbuffer_height - Required height in pixels.
 *    In:  pbuffer_largest - Allocate largest possible.
 *    In:  pixel_width - Pixel width in bytes.
 *    In:  pixel_format - Pixel format.
 *  RETURNS    :
 *    EGL_SUCCESS: pixmap conforms to config.
 *
 ***********************************************************************/

IMG_INTERNAL EGLint GWS_CreatePBufferDrawable(	SrvSysContext *psContext,
												KEGL_SURFACE *psSurface,
												EGLint pbuffer_width,
												EGLint pbuffer_height,
												EGLint pbuffer_largest,
												EGLint pixel_width,
												PVRSRV_PIXEL_FORMAT pixel_format)
{
	IMG_RESULT pvrResult;
	TLS pTls;

	pTls = IMGEGLGetTLSValue();

	if (pTls==IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,"GWS_CreatePBufferDrawable: No Current thread"));

		return EGL_NOT_INITIALIZED;
	}

	PVR_ASSERT (psSurface!=IMG_NULL);

	do
	{
		IMG_UINT32 ui32Stride;
#if defined(SUPPORT_SGX)

		pvrResult = SGXAllocatePBufferDeviceMem(psContext,
												psSurface,
												pbuffer_width,
												pbuffer_height,
												pixel_width,
												pixel_format,
												&ui32Stride);
#endif /* (SUPPORT_SGX) */

#if defined(SUPPORT_VGX)

		pvrResult = VGXAllocatePBufferDeviceMem(psContext,
												psSurface,
												pbuffer_width,
												pbuffer_height,
												pixel_width,
												pixel_format,
												&ui32Stride);
#endif /* SUPPORT_VGX */

		if (pvrResult != PVRSRV_OK)
		{
			if(pbuffer_largest)
			{
				pbuffer_width >>= 1;
				pbuffer_height >>= 1;
			}
			else
			{
				PVR_DPF((PVR_DBG_ERROR,"GWS_CreatePBufferDrawable: AllocDeviceMem failed"));
				goto fail_memalloc;
			}
		}
		else
		{
			psSurface->u.pbuffer.ui32PixelWidth = pbuffer_width;
			psSurface->u.pbuffer.ui32PixelHeight = pbuffer_height;
			psSurface->u.pbuffer.ui32ByteStride = ui32Stride;
			psSurface->u.pbuffer.ePixelFormat = pixel_format;

			return EGL_SUCCESS;	
		}
	}
	while(pbuffer_width && pbuffer_height);
	/* If we complete the loop then we can't allocate even the smallest pbuffer */

fail_memalloc:

	return EGL_BAD_ALLOC;
}


/***********************************************************************************
 *
 * Function Name      : GWS_DeletePBufferDrawable
 * Inputs             : psSurface, psContext
 * Outputs            : 
 * Returns            : 
 * Description        : Destroys a PBuffer's render surface, command queue and
 *                      device memory
 *
 ************************************************************************************/
IMG_INTERNAL IMG_VOID GWS_DeletePBufferDrawable(KEGL_SURFACE* psSurface, SrvSysContext *psSysContext)
{
	TLS pTls;

	pTls = IMGEGLGetTLSValue();

	if (pTls==IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,"GWS_DeletePBufferDrawable: No Current thread"));

		return;
	}

	if(IMGEGLFREEDEVICEMEM(&psSysContext->s3D, psSurface->u.pbuffer.psMemInfo) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"GWS_DestroyPBufferDrawable: Couldn't free device memory"));
	}
}

/******************************************************************************
 End of file (generic_ws.c)
******************************************************************************/

