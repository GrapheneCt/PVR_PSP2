/**************************************************************************
 * Name         : pdump.c
 *
 * Copyright    : 2003-2006 by Imagination Technologies Limited. All rights reserved.
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
 * $Log: pdump.c $
 *
 *  --- Revision Logs Removed --- 
 **************************************************************************/
#include <string.h>
#include <stdarg.h>
#include "context.h"

/* Exclude from non-PDUMP builds */
#ifdef PDUMP

/***********************************************************************************
 Function Name      : PDumpString
 Inputs             : gc, pszString
 Outputs            : -
 Returns            : Success
 Description        : Writes a string to the script file
************************************************************************************/
IMG_INTERNAL IMG_BOOL PDumpString(GLES1Context *gc, IMG_CHAR *pszFormat, ...)
{
	IMG_CHAR szScript[256];

	va_list argList;
	va_start(argList, pszFormat);
	vsprintf(szScript, pszFormat, argList);
	va_end(argList);

	if(PVRSRVPDumpComment(gc->ps3DDevData->psConnection, szScript, IMG_FALSE) != PVRSRV_OK)
	{
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

/***********************************************************************************
 Function Name      : PDumpStringContinuous
 Inputs             : gc, pszString
 Outputs            : -
 Returns            : Success
 Description        : Writes a string to the script file
************************************************************************************/
IMG_INTERNAL IMG_BOOL PDumpStringContinuous(GLES1Context *gc, IMG_CHAR *pszFormat, ...)
{
	IMG_CHAR szScript[256];

	va_list argList;
	va_start(argList, pszFormat);
	vsprintf(szScript, pszFormat, argList);
	va_end(argList);

	if(PVRSRVPDumpComment(gc->ps3DDevData->psConnection, szScript, IMG_TRUE) != PVRSRV_OK)
	{
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

/***********************************************************************************
 Function Name      : PDumpReg
 Inputs             : gc, ui32Addr, ui32Data
 Outputs            : -
 Returns            : Success
 Description        : Writes a register to the script file
************************************************************************************/
IMG_INTERNAL IMG_BOOL PDumpReg(GLES1Context *gc, IMG_UINT32 ui32Addr, IMG_UINT32 ui32Data)
{
	if(PVRSRVPDumpReg(gc->ps3DDevData, "SGXREG", ui32Addr, ui32Data, 0) != PVRSRV_OK)
	{	
		return IMG_FALSE;
	}
	
	return IMG_TRUE;
}

/***********************************************************************************
 Function Name      : PDumpPoll
 Inputs             : gc, ui32Addr, ui32Data
 Outputs            : -
 Returns            : Success
 Description        : Writes a register poll to the script file
************************************************************************************/
IMG_INTERNAL IMG_BOOL PDumpPoll(GLES1Context *gc, IMG_UINT32 ui32Addr, IMG_UINT32 ui32Data)
{
	if(PVRSRVPDumpRegPol(gc->ps3DDevData, "SGXREG", ui32Addr, ui32Data, ui32Data) != PVRSRV_OK)
	{	
		return IMG_FALSE;
	}
	
	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : PDumpSync
 Inputs             : gc, ui32Addr, ui32Data
 Outputs            : -
 Returns            : Success
 Description        : Writes a sync to the script file
************************************************************************************/
IMG_INTERNAL IMG_BOOL PDumpSync(GLES1Context *gc, IMG_VOID *pvAltLinAddr, PVRSRV_CLIENT_SYNC_INFO *psSyncInfo, IMG_UINT32 ui32Offset, IMG_UINT32 ui32Bytes)
{
	if(PVRSRVPDumpSync(gc->ps3DDevData->psConnection, pvAltLinAddr, psSyncInfo, ui32Offset, ui32Bytes) != PVRSRV_OK)
	{	
		return IMG_FALSE;
	}
	
	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : PDumpMem
 Inputs             : gc, psMemInfo, ui32Offset, ui32Size
 Outputs            : -
 Returns            : Success
 Description        : Writes memory to the tex file and a load command to the 
					  script file.
************************************************************************************/
IMG_INTERNAL IMG_BOOL PDumpMem(GLES1Context *gc, PVRSRV_CLIENT_MEM_INFO *psMemInfo, 
							   IMG_UINT32 ui32Offset, IMG_UINT32 ui32Size)
{
	if(PVRSRVPDumpMem(gc->ps3DDevData->psConnection, IMG_NULL, 
						psMemInfo, ui32Offset, ui32Size, 0) != PVRSRV_OK)
	{	
		return IMG_FALSE;
	}
	
	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : PDumpSetFrame
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
                      
************************************************************************************/
IMG_INTERNAL IMG_VOID PDumpSetFrame(GLES1Context *gc)
{
	PVRSRVPDumpSetFrame(gc->ps3DDevData->psConnection, gc->ui32FrameNum);
}


/***********************************************************************************
 Function Name      : PDumpIsCapturing
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
                      
************************************************************************************/
IMG_INTERNAL IMG_BOOL PDumpIsCapturing(GLES1Context *gc)
{
	IMG_BOOL bIsCapturing;

	PVRSRVPDumpIsCapturing(gc->ps3DDevData->psConnection, &bIsCapturing);

	return bIsCapturing;
}

#endif /* PDUMP */

