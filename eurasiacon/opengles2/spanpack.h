/******************************************************************************
 * Name         : spanpack.h
 *
 * Copyright    : 2008 by Imagination Technologies Limited.
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
 * $Log: spanpack.h $
 *****************************************************************************/
#ifndef _SPANPACK_
#define _SPANPACK_

typedef IMG_VOID (*PFNSpanPack)(const GLES2PixelSpanInfo *);


/* These are when native formats match */
IMG_VOID SpanPack16(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPack32(const GLES2PixelSpanInfo *psSpanInfo);

/* These are only for read format conversions to packed non-native HW formats */
IMG_VOID SpanPackARGB4444toRGBA4444(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackARGB1555toRGBA5551(const GLES2PixelSpanInfo *psSpanInfo);

/* ARGB8888 source */
IMG_VOID SpanPackARGB8888toABGR8888(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackARGB8888toXBGR8888(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackARGB8888toRGB565(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackARGB8888toARGB4444(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackARGB8888toARGB1555(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackARGB8888toLuminanceAlpha(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackARGB8888toLuminance(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackAXXX8888toAlpha(const GLES2PixelSpanInfo *psSpanInfo);

/* ABGR8888 source */
IMG_VOID SpanPackABGR8888toARGB8888(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackABGR8888toXBGR8888(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackABGR8888toRGB565(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackABGR8888toARGB4444(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackABGR8888toARGB1555(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackABGR8888toLuminanceAlpha(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackABGR8888toLuminance(const GLES2PixelSpanInfo *psSpanInfo);

/* ARGB4444 source */
IMG_VOID SpanPackARGB4444toABGR8888(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackARGB4444toXBGR8888(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackARGB4444toARGB8888(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackARGB4444toRGB565(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackARGB4444toARGB1555(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackARGB4444toLuminanceAlpha(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackARGB4444toLuminance(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackARGB4444toAlpha(const GLES2PixelSpanInfo *psSpanInfo);

/* ARGB1555 source */
IMG_VOID SpanPackARGB1555toABGR8888(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackARGB1555toXBGR8888(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackARGB1555toARGB8888(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackARGB1555toRGB565(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackARGB1555toARGB4444(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackARGB1555toLuminanceAlpha(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackARGB1555toLuminance(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackARGB1555toAlpha(const GLES2PixelSpanInfo *psSpanInfo);

/* RGB565 source */
IMG_VOID SpanPackRGB565toXBGR8888(const GLES2PixelSpanInfo *psSpanInfo);
IMG_VOID SpanPackRGB565toLuminance(const GLES2PixelSpanInfo *psSpanInfo);

/* XBGR8888 source */
IMG_VOID SpanPackXBGR8888to1BGR8888(const GLES2PixelSpanInfo *psSpanInfo);

/* XRGB8888 source */
IMG_VOID SpanPackXRGB8888to1BGR8888(const GLES2PixelSpanInfo *psSpanInfo);

#endif /* _SPANPACK_ */

/******************************************************************************
 End of file spanpack.h
******************************************************************************/
