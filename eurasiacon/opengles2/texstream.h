/**************************************************************************
 * Name         : texstream.h
 * Author       : bcb
 * Created      : 16/09/2008
 *
 * Copyright    : 2007-2008 by Imagination Technologies Limited. All rights reserved.
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
 * $Date: 2011/03/11 11:51:11 $ $Revision: 1.14 $
 * $Log: texstream.h $
 **************************************************************************/

#ifndef _TEXSTREAM_
#define _TEXSTREAM_

#if defined (__cplusplus)
extern "C" {
#endif

#include "texyuv.h"

typedef struct GLES2DeviceBuffer_TAG {

	IMG_HANDLE 					hBuffer;
	PVRSRV_CLIENT_MEM_INFO 		*psBufferSurface;

	IMG_UINT32					ui32ByteStride;		/*!< Offset in bytes from one line of the surface to the next */

	IMG_UINT32 					ui32PixelWidth;		/*!< How many pixels wide is the surface */
	IMG_UINT32 					ui32PixelHeight;	/*!< How many pixels high is the surface */
	PVRSRV_PIXEL_FORMAT			ePixelFormat;		/*!< Format (e.g. rgb888) of the pixels in the surface */	
} GLES2DeviceBuffer;

typedef struct GLES2StreamDevice_TAG {

	BUFFER_INFO					sBufferInfo;
	IMG_HANDLE 					hBufferDevice;
	IMG_UINT32					ui32BufferDevice;

	GLES2DeviceBuffer			*psBuffer;

	GLES2ExternalTexState		*psExtTexState;

    IMG_UINT32                  ui32BufferRefCount; /* how many times the device' buffers have been 
													   refereced in the current context */

    IMG_BOOL                    bGhosted; /* If yes, then create a new GLES2StreamDevice structure 
											 when trying to FindBufferDevice */

	struct GLES2StreamDevice_TAG	*psNext;

} GLES2StreamDevice;


PVRSRV_ERROR FreeTexStreamState(GLES2Context *gc);
IMG_UINT32 GetNumStreamDevices(GLES2Context *gc);


#if defined (__cplusplus)
}
#endif
#endif /* _TEXSTREAM_ */

/*****************************************************************************
 End of file (texstream.h)
*****************************************************************************/

