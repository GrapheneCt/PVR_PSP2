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
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "ParamBufferSize", 16))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32ParamBufferSize;
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
			bFound = IMG_TRUE;
		}
		else if (!sceClibStrncasecmp(pszHintName, "ui32ExternalZBufferYSize", 25))
		{
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32ExternalZBufferYSize;
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
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32DumpProfileData;
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
			*(IMG_UINT32 *)pvReturn = s_appHint.ui32DisableMetricsOutput;
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
				strcpy(uReturn.pcData, uDefault.pcData);

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