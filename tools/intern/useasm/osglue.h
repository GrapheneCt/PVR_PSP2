/******************************************************************************
 * Name         : osglue.h
 *
 * Copyright    : 2011 by Imagination Technologies Limited.
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
 * $Log: osglue.h $
 **************************************************************************/
#ifndef _USEASM_OSGLUE_H_
#define _USEASM_OSGLUE_H_

/*
  Memory function wrappers
 */
#if defined(__linux__)

	#include <string.h>

	#define UseAsm_MemCopy(X,Y,Z) memcpy(X, Y, Z)
	#define UseAsm_MemSet(X,Y,Z)  memset(X, Y, Z)

#else

	#include "services.h"

	#define UseAsm_MemCopy(X,Y,Z) PVRSRVMemCopy(X, Y, Z)
	#define UseAsm_MemSet(X,Y,Z)  PVRSRVMemSet (X, Y, Z)

#endif /* defined(__linux__) || defined(_UITRON_) || defined(_WIN32) */

/*
  Memory allocation wrappers
 */
#if defined(__linux__)

	#include <stdlib.h>

	#define UseAsm_Malloc(X) malloc(X)
	#define UseAsm_Free(X)   free(X)

#else

	#include "services.h"

	#define UseAsm_Malloc(X) PVRSRVAllocUserModeMem(X)
	#define UseAsm_Free(X)   PVRSRVFreeUserModeMem(X)

#endif /* defined(__linux__) || defined(_UITRON_) || defined(_WIN32) */

#endif /* _USEASM_OSGLUE_H_ */

/******************************************************************************
 End of file osglue.h
******************************************************************************/
