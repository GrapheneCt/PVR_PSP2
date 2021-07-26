/*************************************************************************
 * Name         : combiner.h
 * Title        : Pixel Shader Compiler Text Interface
 * Author       : John Russell
 * Created      : Jan 2002
 *
 * Copyright    : 2002-2006 by Imagination Technologies Limited. All rights reserved.
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
 *
 * Modifications:-
 * $Log: combiner.h $
 **************************************************************************/

#include "img_types.h"

IMG_UINT32 PVRCombineBlendCodeWithShader(IMG_PUINT32			pui32Output,
									     IMG_UINT32				ui32ShaderCodeCount,
									     IMG_PUINT32			pui32ShaderCode,
									     IMG_UINT32				ui32BlendCodeCount,
									     IMG_PUINT32			pui32BlendCode,
									     IMG_BOOL				bMultipleShaderOutputs,
										 IMG_BOOL				bC10FormatControlOn);
