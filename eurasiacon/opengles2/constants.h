/**************************************************************************
 * Name         : constants.h
 * Author       : BCB
 * Created      : 08/12/2005
 *
 * Copyright    : 2005-2006 by Imagination Technologies Limited. All rights reserved.
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
 * $Date: 2011/02/10 15:11:08 $ $Revision: 1.28 $
 * $Log: constants.h $
 * 
 *  --- Revision Logs Removed --- 
 **************************************************************************/

#ifndef _CONSTANTS_
#define _CONSTANTS_

#define GLES2_Zero							0.0f
#define GLES2_One							1.0f
#define GLES2_Half							0.5f

#define GLES2_ALIASED_LINE_WIDTH_MIN		1
#define GLES2_ALIASED_LINE_WIDTH_MAX		16

#define GLES2_POINT_SPRITE_SIZE_MIN			0.0f
#define GLES2_POINT_SPRITE_SIZE_MAX			EURASIA_MAX_POINT_SIZE

#define GLES2_MAX_STENCIL_VALUE				((1UL << gc->psMode->ui32StencilBits) - 1UL)

#if(EURASIA_TEXTURESIZE_MAX > 2048)
#define GLES2_MAX_TEXTURE_SIZE				4096UL
#define GLES2_MAX_CEM_TEXTURE_SIZE			4096UL
#define GLES2_MAX_TEXTURE_MIPMAP_LEVELS		13U
#else
#define GLES2_MAX_TEXTURE_SIZE				2048UL
#define GLES2_MAX_CEM_TEXTURE_SIZE			2048UL
#define GLES2_MAX_TEXTURE_MIPMAP_LEVELS		12U
#endif

#define GLES2_MIN_TEXTURE_LEVEL				0

#if defined(GLES2_TEST_LIMITS)
#define GLES2_MAX_TEXTURE_UNITS				16
#define GLES2_MAX_VERTEX_TEXTURE_UNITS		16
#define GLES2_MAX_VERTEX_UNIFORM_VECTORS	512
#define GLES2_MAX_FRAGMENT_UNIFORM_VECTORS	64
#else
#define GLES2_MAX_TEXTURE_UNITS				8
#define GLES2_MAX_VERTEX_TEXTURE_UNITS		8
#define GLES2_MAX_VERTEX_UNIFORM_VECTORS	128
#define GLES2_MAX_FRAGMENT_UNIFORM_VECTORS	64
#endif


#define GLES2_MAX_VERTICES					65536
#define GLES2_MAX_INDICES					65536

#define GLES2_MAX_SUBPIXEL_BITS				4

#define GLES2_MAX_DRAW_BUFFERS             1
#if defined(FIX_HW_BRN_25339) || defined(FIX_HW_BRN_27330)
/* Reduce attrib count due to PDS constant limit for streams */
#define GLES2_MAX_VERTEX_ATTRIBS			8
#else
#define GLES2_MAX_VERTEX_ATTRIBS			16
#endif
#define GLES2_MAX_VARYING_VECTORS			8

#if defined(CLIENT_DRIVER_DEFAULT_WAIT_RETRIES)
#define GLES2_DEFAULT_WAIT_RETRIES CLIENT_DRIVER_DEFAULT_WAIT_RETRIES
#else
#define GLES2_DEFAULT_WAIT_RETRIES 10000
#endif

#if defined(__linux__)
	#define GLES2_GLSL_COMPILER_MODULE_NAME    "lib" GLSL_COMPILER_BASENAME ".so"
#elif defined(__psp2__)
	#define GLES2_GLSL_COMPILER_MODULE_NAME
#else
				#error ("Unknown host operating system")

#endif


#endif /* _CONSTANTS_ */
