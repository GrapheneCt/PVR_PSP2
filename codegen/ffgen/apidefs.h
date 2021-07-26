/**************************************************************************
 * Name         : apidefs.h
 * Author       : jml
 * Created      : 16/08/2006
 *
 * Copyright    : 2000-2006 by Imagination Technologies Limited. All rights reserved.
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
 * Modifications:-
 * $Log: apidefs.h $
 **************************************************************************/

#ifndef __api_defs_h_
#define __api_defs_h_

#if defined(STANDALONE)

#include "memmgr.h"
#include "debug.h"

#define FFGENMalloc(context, a)			DebugMemAlloc(a)
#define FFGENCalloc(context, a)			DebugMemCalloc(a)
#define FFGENRealloc(context, a, b)		DebugMemRealloc(a, b) 
#define FFGENMalloc2(context, a, b, c)	DebugMemAlloc2(a, b, c)
#define FFGENFree(context,  a)			DebugMemFree(a); context

#else

#define FFGENMalloc(context, a)			(context->pfnMalloc)(context->hClientHandle, a)
#define FFGENCalloc(context, a)			(context->pfnCalloc)(context->hClientHandle, a)
#define FFGENRealloc(context, a, b)		(context->pfnRealloc)(context->hClientHandle, a, b)
#define FFGENMalloc2(context, a, b, c)	(context->pfnMalloc)(context->hClientHandle, a)
#define FFGENFree(context, a)			(context->pfnFree)(context->hClientHandle, a)

#endif /* STANDALONE */


#endif /* __api_defs_h_ */

/**************************************************************************
 End of file (apidefs.h)
**************************************************************************/

