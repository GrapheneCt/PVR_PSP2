/******************************************************************************
 * Name         : imgextensions.h
 * Created      : 7/4/2009
 *
 * Copyright    : 2009 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form by
 *              : any means, electronic, mechanical, manual or otherwise, or
 *              : disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited,
 *              : Home Park Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * $Log: imgextensions.h $
 *****************************************************************************/

#ifndef _IMGEXTENSIONS_H_
#define _IMGEXTENSIONS_H_

/* GLES1 extensions */
#define GLES1_EXTENSION_MATRIX_PALETTE
#define GLES1_EXTENSION_DRAW_TEXTURE			
#define GLES1_EXTENSION_QUERY_MATRIX			

#define GLES1_EXTENSION_TEX_ENV_CROSSBAR 		
#define GLES1_EXTENSION_TEX_MIRRORED_REPEAT 	
#define GLES1_EXTENSION_TEX_CUBE_MAP 			
#define GLES1_EXTENSION_BLEND_SUBTRACT			
#define GLES1_EXTENSION_BLEND_FUNC_SEPARATE 	
#define GLES1_EXTENSION_BLEND_EQ_SEPARATE 		
#define GLES1_EXTENSION_STENCIL_WRAP 			
#define GLES1_EXTENSION_EXTENDED_MATRIX_PALETTE 
#define GLES1_EXTENSION_FRAME_BUFFER_OBJECTS 	
#define GLES1_EXTENSION_RGB8_RGBA8 				
#define GLES1_EXTENSION_DEPTH24 				
#define GLES1_EXTENSION_STENCIL8 				

#define GLES1_EXTENSION_ETC1 					
#define GLES1_EXTENSION_MAP_BUFFER	 			
#define GLES1_EXTENSION_MULTI_DRAW_ARRAYS       
#define GLES1_EXTENSION_EGL_IMAGE				
#define GLES1_EXTENSION_REQUIRED_INTERNAL_FORMAT

#define GLES1_EXTENSION_PVRTC
#define GLES1_EXTENSION_READ_FORMAT
#define GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888
#if !defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
#define GLES1_EXTENSION_TEXTURE_STREAM
#endif

#define GLES1_EXTENSION_VERTEX_ARRAY_OBJECT

/* GLES2 extensions */
#define GLES2_EXTENSION_FLOAT_TEXTURE			
#define GLES2_EXTENSION_HALF_FLOAT_TEXTURE		
#define GLES2_EXTENSION_EGL_IMAGE				
#define GLES2_EXTENSION_MULTI_DRAW_ARRAYS       
#define GLES2_EXTENSION_NPOT					
#define GLES2_EXTENSION_TEXTURE_FORMAT_BGRA8888	
#define GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT
#define GLES2_EXTENSION_READ_FORMAT				
#define GLES2_EXTENSION_GET_PROGRAM_BINARY		
#define GLES2_EXTENSION_DEPTH_TEXTURE			
#define GLES2_EXTENSION_PACKED_DEPTH_STENCIL
#define GLES2_EXTENSION_VERTEX_ARRAY_OBJECT
#define GLES2_EXTENSION_DISCARD_FRAMEBUFFER
#if !defined(GLES2_EXTENSION_EGL_IMAGE_EXTERNAL)
#define GLES2_EXTENSION_TEXTURE_STREAM
#endif
#define GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE

/* VG extensions */
#define VG_EXTENSION_EGL_IMAGE						

/* EGL extensions */
#define EGL_EXTENSION_RENDER_TO_TEXTURE				
#define EGL_EXTENSION_KHR_IMAGE					
#define EGL_EXTENSION_KHR_GL_TEXTURE_2D_IMAGE		
#define EGL_EXTENSION_KHR_GL_TEXTURE_CUBEMAP_IMAGE	
#define EGL_EXTENSION_KHR_GL_RENDERBUFFER_IMAGE		
#define EGL_EXTENSION_KHR_VG_PARENT_IMAGE	
#define EGL_EXTENSION_KHR_FENCE_SYNC
#if defined(__psp2__)
#undef EGL_EXTENSION_KHR_FENCE_SYNC
#define EGL_EXTENSION_KHR_REUSABLE_SYNC
#endif
#define EGL_EXTENSION_IMG_EGL_HIBERNATION

/* EGL image extensions */
#if !defined(__psp2__) //TODO: Can be implemented on Vita, but not a priority right now
#define EGL_EXTENSION_NOK_IMAGE_SHARED
#endif
#define EGL_IMAGE_COMPRESSED_GLES1
#define EGL_IMAGE_COMPRESSED_GLES2

//#define GLES1_EXTENSION_CARNAV					


#endif /* _IMGEXTENSIONS_H_ */

/******************************************************************************
 End of file (imgextensions.h)
******************************************************************************/
