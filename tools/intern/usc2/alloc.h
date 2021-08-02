/******************************************************************************
 * Name         : alloc.h
 * Title        : Eurasia USE Compiler
 * Created      : Sep 2010
 *
 * Copyright    : 2002-2007 by Imagination Technologies Limited.
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
 * Modifications:-
 * $Log: alloc.h $
 *****************************************************************************/
#ifndef __USC_ALLOC_H
#define __USC_ALLOC_H

#ifdef DEBUG
#define USC_COLLECT_ALLOC_INFO 1
#endif

/* Forward declarations of structures. */
struct tagINTERMEDIATE_STATE;

/**
 * Define a type alias
 * This is an alias for PINTERMEDIATE_STATE
 **/
typedef struct tagINTERMEDIATE_STATE* USC_DATA_STATE_PTR;

/* Forward declarations of functions. */
IMG_VOID _UscFree(USC_DATA_STATE_PTR psState, IMG_PVOID *pvBlock);

#ifdef USC_COLLECT_ALLOC_INFO
IMG_PVOID UscAllocfn(USC_DATA_STATE_PTR psState, IMG_UINT32 uSize, IMG_UINT32  uLineNumber, const IMG_CHAR *pszFileName);
#define UscAlloc(a,b) UscAllocfn(a, b, __LINE__, __FILE__)
#else
IMG_PVOID UscAllocfn(USC_DATA_STATE_PTR psState, IMG_UINT32 uSize);
#define UscAlloc(a,b) UscAllocfn(a, b)
#endif

#endif /* __USC_ALLOC_H */
