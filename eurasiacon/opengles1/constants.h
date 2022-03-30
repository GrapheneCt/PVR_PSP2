/**************************************************************************
 * Name         : constants.h
 *
 * Copyright    : 2003-2006 by Imagination Technologies Limited. All rights reserved.
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
 * $Log: constants.h $
 *
 *  --- Revision Logs Removed --- 
 **************************************************************************/

#ifndef _CONSTANTS_
#define _CONSTANTS_

#define GLES1_Zero							0.0f
#define GLES1_One							1.0f
#define GLES1_Half							0.5f
#define GLES1_MinusOne					   -1.0f
#define GLES1_Zerox							0
#define GLES1_Onex							(1 << 16)
#define GLES1_E								((GLfloat) 2.7182818284590452354)
#define GLES1_Pi							((GLfloat) 3.1415926535897932)
#define GLES1_DegreesToRadians				(GLES1_Pi / (GLfloat) 180.0)
#define GLES1_INV_LN_TWO       				1.4426950409f   /* 1/ln(2) */
#define GLES1_INV_SQRT_LN_TWO  				1.2011224088f   /* 1/sqrt(ln(2)) */

#define GLES1_ALIASED_POINT_SIZE_MIN		1.0f
#define GLES1_ALIASED_POINT_SIZE_MAX		32.0f
#define GLES1_SMOOTH_POINT_SIZE_MIN			1.0f
#define GLES1_SMOOTH_POINT_SIZE_MAX			1.0f
#define GLES1_ALIASED_LINE_WIDTH_MIN		1.0f
#define GLES1_ALIASED_LINE_WIDTH_MAX		16.0f
#define GLES1_SMOOTH_LINE_WIDTH_MIN			1.0f
#define GLES1_SMOOTH_LINE_WIDTH_MAX			1.0f

#define GLES1_MAX_STENCIL_VALUE				((1UL << gc->psMode->ui32StencilBits) - 1UL)

#define GLES1_MAX_LIGHTS					8

#define GLES1_MAX_MODELVIEW_STACK_DEPTH		16
#define GLES1_MAX_PROJECTION_STACK_DEPTH	2
#define GLES1_MAX_TEXTURE_STACK_DEPTH		4
#define GLES1_MAX_PROGRAM_STACK_DEPTH		2

#define GLES1_MAX_TEXTURE_UNITS				16

#if(EURASIA_TEXTURESIZE_MAX > 2048)
#define GLES1_MAX_TEXTURE_SIZE				4096UL
#define GLES1_MAX_TEXTURE_MIPMAP_LEVELS		13
#else
#define GLES1_MAX_TEXTURE_SIZE				2048UL
#define GLES1_MAX_TEXTURE_MIPMAP_LEVELS		12
#endif

#if defined(CLIENT_DRIVER_DEFAULT_WAIT_RETRIES)
#define GLES1_DEFAULT_WAIT_RETRIES CLIENT_DRIVER_DEFAULT_WAIT_RETRIES
#else
#define GLES1_DEFAULT_WAIT_RETRIES 10000
#endif

#define GLES1_MAX_SUBPIXEL_BITS				4


#define GLES1_MAX_CLIP_PLANES				6


#if defined(GLES1_EXTENSION_MATRIX_PALETTE)

	#if defined(GLES1_EXTENSION_EXTENDED_MATRIX_PALETTE)

		#define GLES1_MAX_PALETTE_MATRICES			32
		#define GLES1_MAX_VERTEX_UNITS				4

	#else /* defined(GLES1_EXTENSION_EXTENDED_MATRIX_PALETTE)*/

		#define GLES1_MAX_PALETTE_MATRICES			9
		#define GLES1_MAX_VERTEX_UNITS				3

	#endif /* defined (GLES1_EXTENSION_EXTENDED_MATRIX_PALETTE)*/

#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) )*/


#endif /* _CONSTANTS_ */
