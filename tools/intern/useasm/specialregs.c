/******************************************************************************
 * Name         : specialregs.c
 * Title        : Pixel Shader Compiler
 * Author       : John Russell
 * Created      : Jan 2002
 *
 * Copyright    : 2002-2010 by Imagination Technologies Limited.
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
 * $Log: specialregs.c $
 *****************************************************************************/

#undef SGX543
#undef SGX544
#undef SGX554

#include "sgxusespecialbankdefs.h"

#include "specialregs.h"

IMG_INTERNAL const IMG_UINT32 g_auNonVecSpecialRegistersInvalidForNonBitwise[] =
	{EURASIA_USE_SPECIAL_BANK_SEQUENCENUMBER, EURASIA_USE_SPECIAL_BANK_EDGEMASKSELECT, EURASIA_USE_SPECIAL_BANK_TASKTYPE};
IMG_INTERNAL const IMG_UINT32 g_uNonVecSpecialRegistersInvalidForNonBitwiseCount = 
	(sizeof(g_auNonVecSpecialRegistersInvalidForNonBitwise) / sizeof(g_auNonVecSpecialRegistersInvalidForNonBitwise[0]));

