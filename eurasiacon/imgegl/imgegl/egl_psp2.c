/**************************************************************************
 * Name         : egl_linux.c
 * Created      : 07/08/2003
 *
 * Copyright    : 2003-2005 by Imagination Technologies Limited. All rights reserved.
 *              : No part of this software, either material or conceptual
 *              : may be copied or distributed, transmitted, transcribed,
 *              : stored in a retrieval system or translated into any
 *              : human or computer language in any form by any means,
 *              : electronic, mechanical, manual or other-wise, or
 *              : disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited, Unit 8, HomePark
 *              : Industrial Estate, King's Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * $Date: 2008/12/19 12:36:27 $ $Revision: 1.6 $
 * $Log: egl_linux.c $
 *
 *  --- Revision Logs Removed --- 
 **************************************************************************/


#include "egl_internal.h"
#include "tls.h"


static IMG_VOID (*pfGlobalDataDeinitCallback)(IMG_VOID *);

static EGLGlobal sGlobalData;

static EGLGlobal *psGlobalData;


/***********************************************************************************
 Function Name      : ENV_CreateGlobalData
 Inputs             : ui32Size - The amount to allocate
 Outputs            : -
 Returns            : Allocated and zeroed global data value
 Description        : Creates the global data
************************************************************************************/
IMG_INTERNAL IMG_VOID *ENV_CreateGlobalData(IMG_UINT32 ui32Size, IMG_VOID (*pfDeinitCallback)(IMG_VOID *))
{
#if defined(DEBUG)
	PVR_ASSERT(ui32Size == sizeof(EGLGlobal));
#else
	PVR_UNREFERENCED_PARAMETER(ui32Size);
#endif

	pfGlobalDataDeinitCallback = pfDeinitCallback;

	psGlobalData = &sGlobalData;

	return (IMG_VOID *)psGlobalData;
}


/***********************************************************************************
 Function Name      : ENV_DestroyGlobalData
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : Destroys the global data
************************************************************************************/
IMG_INTERNAL IMG_VOID ENV_DestroyGlobalData(IMG_VOID)
{
	if (pfGlobalDataDeinitCallback)
	{
		pfGlobalDataDeinitCallback((IMG_VOID *)&sGlobalData);
	}
}


/***********************************************************************************
 Function Name      : ENV_GetGlobalData
 Inputs             : -
 Outputs            : -
 Returns            : Global data value
 Description        : Gets a value from global data
************************************************************************************/
IMG_INTERNAL IMG_VOID *ENV_GetGlobalData(IMG_VOID)
{
	return (IMG_VOID *)psGlobalData;
}


/***********************************************************************************
 Function Name      : egl_init
 Inputs             : None
 Outputs            : None
 Returns            : None
 Description        : Library initialisation routine
************************************************************************************/
static void egl_init(void)
{
	PVR_DPF((PVR_DBG_MESSAGE,"egl_init: EGL init"));
}


/***********************************************************************************
 Function Name      : egl_fini
 Inputs             : None
 Outputs            : None
 Returns            : None
 Description        : Library shutdown routine
************************************************************************************/
static void egl_fini(void)
{
	PVR_DPF((PVR_DBG_MESSAGE,"egl_fini: EGL fini"));

	ENV_DestroyGlobalData();
}

int __module_stop(SceSize argc, const void *args)
{
	egl_fini();
	return SCE_KERNEL_STOP_SUCCESS;
}

int __module_exit()
{
	return SCE_KERNEL_STOP_SUCCESS;
}

int __module_start(SceSize argc, void *args)
{
	egl_init();
	return SCE_KERNEL_START_SUCCESS;
}


