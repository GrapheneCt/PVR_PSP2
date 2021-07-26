/******************************************************************************
 * Name         : coreutils.h
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
 * $Log: coreutils.h $
 *****************************************************************************/

#ifndef __USEASM_COREUTILS_H
#define __USEASM_COREUTILS_H

SGX_CORE_ID_TYPE GetDefaultCore(IMG_VOID);
IMG_BOOL CheckCore(SGX_CORE_ID_TYPE	eCompileTarget);
SGX_CORE_ID_TYPE ParseCoreSpecifier(IMG_PCHAR pszTarget);
IMG_UINT32 ParseRevisionSpecifier(IMG_PCHAR pszTarget);

#endif /* __USEASM_COREUTILS_H */
