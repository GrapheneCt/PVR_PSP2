/******************************************************************************
 * Name         : uscfns.h
 * Author       : James McCarthy
 * Created      : 07/11/2005
 *
 * Copyright    : 2000-2006 by Imagination Technologies Limited.
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
 * $Log: uscfns.h $
 *****************************************************************************/

#ifndef __gl_uscfns_h_
#define __gl_uscfns_h_

#include "apidefs.h"
#include "codegen.h"

typedef struct FFGenUseasmStateTAG
{
	/* Num labels is based on number of lights */
	IMG_INT32		*piLabelArray;

	USE_INST		*psBaseInstr;
	/* Client handle for memory functions */
	FFGenContext	*psFFGenContext;

}FFGenUseasmState;


IMG_INTERNAL IMG_VOID IMG_CALLCONV AssemblerError(IMG_PVOID pvContext, PUSE_INST psInst, IMG_CHAR *pszFmt, ...) IMG_FORMAT_PRINTF(3, 4);
IMG_INTERNAL IMG_VOID IMG_CALLCONV UseAssemblerSetLabelAddress(IMG_PVOID pvContext, IMG_UINT32 uLabel, IMG_UINT32 uAddress);
IMG_INTERNAL IMG_PCHAR IMG_CALLCONV UseAssemblerGetLabelName(IMG_PVOID pvContext, IMG_UINT32 uLabel);
IMG_INTERNAL IMG_UINT32 IMG_CALLCONV UseAssemblerGetLabelAddress(IMG_PVOID pvContext, IMG_UINT32 uLabel);
IMG_INTERNAL IMG_PVOID IMG_CALLCONV UseasmRealloc(IMG_VOID *pvContext, IMG_VOID *pvOldBuf, IMG_UINT32 uNewSize, IMG_UINT32 uOldSize);

#endif /* __gl_uscfns_h_ */

/******************************************************************************
 End of file (ffgen.h)
******************************************************************************/

