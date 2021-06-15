/******************************************************************************
* Name         : sgxpixfmts_types.h
* Title        : Pixel format definitions
* Author(s)    : Imagination Technologies
* Created      : 19th May 2009
*
* Copyright    : 2009 by Imagination Technologies Limited.
*                All rights reserved. No part of this software, either material
*                or conceptual may be copied or distributed, transmitted,
*                transcribed, stored in a retrieval system or translated into
*                any human or computer language in any form by any means,
*                electronic, mechanical, manual or otherwise, or disclosed
*                to third parties without the express written permission of
*                Imagination Technologies Limited, Home Park Estate,
*                Kings Langley, Hertfordshire, WD4 8LZ, U.K.
*
* Description  : For specified pixel formats, describe useful information
*
* Platform     : Generic
*
* Modifications:-
* $Log: sgxpixfmts_types.h $
******************************************************************************/

#ifndef _SGXPIXFMTS_TYPES_H_
#define _SGXPIXFMTS_TYPES_H_

#include "sgxdefs.h"

/*
	Common structure define for the texture format description tables.
*/
typedef struct _SGX_PIXEL_FORMAT_
{
	IMG_UINT32 ui32NumChunks;
	IMG_UINT32 ui32TotalBitsPerPixel;

	IMG_UINT32 aui32TAGControlWords[EURASIA_TAG_MAX_TEXTURE_CHUNKS][EURASIA_TAG_TEXTURE_STATE_SIZE];
	IMG_UINT32 ui32PBEFormat;

} SGX_PIXEL_FORMAT, * PSGX_PIXEL_FORMAT;

#endif /* _SGXPIXFMTS_TYPES_H_ */
