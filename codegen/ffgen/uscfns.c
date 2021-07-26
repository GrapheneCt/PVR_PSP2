/******************************************************************************
 * Name         : uscfns.c
 * Author       : James McCarthy
 * Created      : 9/12/2005
 *
 * Copyright    : 2005-2007 by Imagination Technologies Limited.
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
 * Modifications:-
 * $Log: uscfns.c $
 *****************************************************************************/

#include <stdio.h>
#include <stdarg.h>

#include "uscfns.h"

/*****************************************************************************
 FUNCTION	: AssemblerError

 PURPOSE	: Called by the assembler to report a problem encoding an instruction.

 PARAMETERS	: psInst		- The problematic instruction
			  pszFmt, ...	- Description of the problem encountered.

 RETURNS	: None.
*****************************************************************************/
IMG_INTERNAL IMG_VOID IMG_CALLCONV AssemblerError(IMG_PVOID pvContext, PUSE_INST psInst, IMG_CHAR *pszFmt, ...)
{
#if defined(STANDALONE) || (defined(OGLES1_MODULE) && defined(DEBUG))
	va_list ap;
	IMG_CHAR pszTemp[256];
	FFGenUseasmState *psUseasmState = ( FFGenUseasmState *)pvContext;
	IMG_UINT32 uInstrNum = 0;
	PUSE_INST psTempInst = psUseasmState->psBaseInstr;

	while(psTempInst)
	{
		if(psTempInst == psInst)
		{
			break;
		}
		psTempInst = psTempInst->psNext;
		uInstrNum++;
	}
	
	va_start(ap, pszFmt);
	vsprintf(pszTemp, pszFmt, ap);
	printf("Assember Error:\n");
	printf("INSTR No %u: %s, %s\n", uInstrNum, UseGetNameOpcode(psInst->uOpcode), pszTemp);
	va_end(ap);
#else
	PVR_UNREFERENCED_PARAMETER(pvContext); 
	PVR_UNREFERENCED_PARAMETER(psInst); 
	PVR_UNREFERENCED_PARAMETER(pszFmt);
#endif
}


/*****************************************************************************
 FUNCTION	: UseAssemblerSetLabelAddress

 PURPOSE	: Set the address for a label.

 PARAMETERS	: pvContext			- Context.
			  dwLabel			- Id of the label
			  dwAddress			- The address to set.

 RETURNS	: Nothing.
*****************************************************************************/
IMG_INTERNAL IMG_VOID IMG_CALLCONV UseAssemblerSetLabelAddress(IMG_PVOID pvContext, IMG_UINT32 uLabel, IMG_UINT32 uAddress)
{
	FFGenUseasmState *psUseasmState = ( FFGenUseasmState *)pvContext;

	psUseasmState->piLabelArray[uLabel] = uAddress;

	/* Original line 
	((IMG_PUINT32)pvContext)[uLabel] = uAddress;
	*/
}


/*****************************************************************************
 FUNCTION	: UseAssemblerGetLabelName

 PURPOSE	: Get the user-friendly name for a label.

 PARAMETERS	: pvContext			- Context.
			  dwLabel			- Id of the label

 RETURNS	: Pointer to the name.
*****************************************************************************/
IMG_INTERNAL IMG_PCHAR IMG_CALLCONV UseAssemblerGetLabelName(IMG_PVOID pvContext, IMG_UINT32 uLabel)
{
	PVR_UNREFERENCED_PARAMETER(pvContext);
	PVR_UNREFERENCED_PARAMETER(uLabel);

	return IMG_NULL;
}


/*****************************************************************************
 FUNCTION	: UseAssemblerGetLabelAddress

 PURPOSE	: Get the address for a label.

 PARAMETERS	: pvContext			- Context
			  dwLabel			- Id of the label

 RETURNS	: The address.
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 IMG_CALLCONV UseAssemblerGetLabelAddress(IMG_PVOID pvContext, IMG_UINT32 uLabel)
{
	FFGenUseasmState *psUseasmState = ( FFGenUseasmState *)pvContext;

	return psUseasmState->piLabelArray[uLabel];

	/* Original line:
	return ((IMG_PUINT32)pvContext)[uLabel];
	*/
}



/*****************************************************************************
 FUNCTION	: UseasmRealloc

 PURPOSE	:

 PARAMETERS	:

 RETURNS	:
*****************************************************************************/
IMG_INTERNAL IMG_PVOID IMG_CALLCONV UseasmRealloc(IMG_VOID *pvContext, IMG_VOID *pvOldBuf, IMG_UINT32 uNewSize, IMG_UINT32 uOldSize)
{
	FFGenUseasmState *psUseasmState = ( FFGenUseasmState *)pvContext;

	PVR_UNREFERENCED_PARAMETER(uOldSize);
	PVR_UNREFERENCED_PARAMETER(psUseasmState);

/*
	Not sure what memory function we should use
*/
	return FFGENRealloc(psUseasmState->psFFGenContext, pvOldBuf, uNewSize);
}

/******************************************************************************
 End of file (uscfns.c)
******************************************************************************/
