/******************************************************************************
 * Name         : useasm.h
 * Title        : Pixel Shadel Compiler
 * Author       : John Russell
 * Created      : Jan 2002
 *
 * Copyright    : 2002-2006 by Imagination Technologies Limited.
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
 * $Log: specialregs.h $
 *****************************************************************************/

#ifndef __USEASM_SPECIALREGS_H
#define __USEASM_SPECIALREGS_H

#include "img_types.h"

extern const IMG_UINT32 g_auNonVecSpecialRegistersInvalidForNonBitwise[];
extern const IMG_UINT32 g_uNonVecSpecialRegistersInvalidForNonBitwiseCount;
extern const IMG_UINT32 g_auVecSpecialRegistersInvalidForNonBitwise[];
extern const IMG_UINT32 g_uVecSpecialRegistersInvalidForNonBitwiseCount;

#endif /* defined(__USEASM_SPECIALREGS_H) */

