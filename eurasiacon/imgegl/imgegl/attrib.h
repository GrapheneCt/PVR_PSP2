/* -*- c-file-style: "img" -*-
<module>
 * Name         : attrib.h
 * Title        : Khronos EGL Attribute Definitions
 * Author       : Marcus Shawcroft
 * Created      : 23 Nov 2003
 *
 * Copyright    : 2003 by Imagination Technologies Limited.
 *                All rights reserved.  No part of this software, either
 *                material or conceptual may be copied or distributed,
 *                transmitted, transcribed, stored in a retrieval system
 *                or translated into any human or computer language in any
 *                form by any means, electronic, mechanical, manual or
 *                other-wise, or disclosed to third parties without the
 *                express written permission of Imagination Technologies
 *                Limited, Unit 8, HomePark Industrial Estate,
 *                King's Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * Description  : Attribute Definitions
 * 
 * Platform     : ALL
 *
</module>
 */

/* Fields are:
   Attribute name.
   Default attribute value.
   Selection criteria: atleast exact mask
 */

A(EGL_BUFFER_SIZE, 0, atleast)
A(EGL_RED_SIZE, 0, atleast)
A(EGL_GREEN_SIZE, 0, atleast)
A(EGL_BLUE_SIZE, 0, atleast)
A(EGL_LUMINANCE_SIZE, 0, atleast)
A(EGL_ALPHA_SIZE, 0, atleast)
A(EGL_ALPHA_MASK_SIZE, 0, atleast)
#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE)
A(EGL_BIND_TO_TEXTURE_RGB, EGL_DONT_CARE, exact)
A(EGL_BIND_TO_TEXTURE_RGBA, EGL_DONT_CARE, exact)
#endif /* defined(EGL_EXTENSION_RENDER_TO_TEXTURE) */
A(EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER, exact)
A(EGL_CONFIG_CAVEAT, EGL_DONT_CARE, exact)
A(EGL_CONFIG_ID, EGL_DONT_CARE, exact)
A(EGL_CONFORMANT, 0, mask)
A(EGL_DEPTH_SIZE, 0, atleast)
A(EGL_LEVEL, 0, exact)
A(EGL_MATCH_NATIVE_PIXMAP, EGL_NONE, exact)
A(EGL_MAX_SWAP_INTERVAL, EGL_DONT_CARE, exact)
A(EGL_MIN_SWAP_INTERVAL, EGL_DONT_CARE, exact)
A(EGL_NATIVE_RENDERABLE, EGL_DONT_CARE, exact)
A(EGL_NATIVE_VISUAL_TYPE, EGL_DONT_CARE, exact)
A(EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT, mask)
A(EGL_SAMPLE_BUFFERS, 0, atleast)
A(EGL_SAMPLES, 0, atleast)
A(EGL_STENCIL_SIZE, 0, atleast)
A(EGL_SURFACE_TYPE, EGL_WINDOW_BIT, mask)
A(EGL_TRANSPARENT_TYPE, EGL_NONE, exact)
A(EGL_TRANSPARENT_RED_VALUE, EGL_DONT_CARE, exact)
A(EGL_TRANSPARENT_GREEN_VALUE, EGL_DONT_CARE, exact)
A(EGL_TRANSPARENT_BLUE_VALUE, EGL_DONT_CARE, exact)
#if defined(EGL_EXTENSION_DELTABUFFER_IMG)
A(EGL_DELTA_SIZE_IMG, 0, atleast)
#endif
#if defined(EGL_EXTENSION_ANDROID_RECORDABLE)
A(EGL_RECORDABLE_ANDROID, EGL_DONT_CARE, exact)
#endif /* defined(EGL_EXTENSION_ANDROID_RECORDABLE) */
