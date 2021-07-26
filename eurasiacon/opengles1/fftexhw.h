/**************************************************************************
 * Name         : fftexhw.h
 *
 * Copyright    : 2006 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or other-wise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited, Home Park
 *              : Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * $Log: fftexhw.h $
 *
 *  --- Revision Logs Removed --- 
 ****************************************************************************/

#ifndef _FFTEXHW_H_
#define _FFTEXHW_H_

IMG_BOOL IntegerTextureBlendingPossible(GLES1Context		*gc,
										IMG_UINT32			ui32ExtraBlendState,
										FFTBBlendLayerDesc	*psBlendLayers,
										IMG_BOOL			*pbTexturingEnabled);


IMG_BOOL CreateIntegerFragmentProgramFromFF(GLES1Context		*gc, 
											FFTBBlendLayerDesc	*psBlendLayers,
											IMG_BOOL			 bAnyTexturingEnabled,
											IMG_BOOL			*pbTexturingEnabled, 
											IMG_UINT32			ui32ExtraBlendState,
											FFTBProgramDesc		*psProgramInfo);

#endif
