/******************************************************************************
 * Name         : specialregs_vec.c
 * Title        : Pixel Shader Compiler
 * Author       : John Russell
 * Created      : Jan 2002
 *
 * Copyright    : 2002-2008 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited, Home Park
 *              : Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * Modifications:-
 * $Log: specialregs_vec.c $
 *****************************************************************************/

#undef SGX545
#if !defined(SGX543)
#define SGX543
#endif /* !defined(SGX543) */

#include "sgxusespecialbankdefs.h"

#include "specialregs.h"

IMG_INTERNAL const IMG_UINT32	g_auVecSpecialRegistersInvalidForNonBitwise[] =
	{EURASIA_USE_SPECIAL_BANK_SEQUENCENUMBER, EURASIA_USE_SPECIAL_BANK_TASKTYPE};
IMG_INTERNAL const IMG_UINT32 g_uVecSpecialRegistersInvalidForNonBitwiseCount = 
	(sizeof(g_auVecSpecialRegistersInvalidForNonBitwise) / sizeof(g_auVecSpecialRegistersInvalidForNonBitwise[0]));

