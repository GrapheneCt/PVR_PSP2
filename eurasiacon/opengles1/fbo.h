/**************************************************************************
 * Name         : fbo.h
 *
 * Copyright    : 2006 by Imagination Technologies Limited. 
 *                All rights reserved.
 *                No part of this software, either material or conceptual 
 *                may be copied or distributed, transmitted, transcribed,
 *                stored in a retrieval system or translated into any 
 *                human or computer language in any form by any means,
 *                electronic, mechanical, manual or other-wise, or 
 *                disclosed to third parties without the express written
 *                permission of:
 *                        Imagination Technologies Limited, 
 *                        HomePark Industrial Estate,
 *                        Kings Langley, 
 *                        Hertfordshire,
 *                        WD4 8LZ, U.K.
 *
 * Description  : Header file framebuffer_object
 *
 * Platform     : ANSI
 *
 * $Log: fbo.h $
 *
 *  --- Revision Logs Removed --- 
 **************************************************************************/

#ifndef _FBO_
#define _FBO_


/*
 * Framebuffer attachable image.
 * The first variable of renderbuffers and mipmap levels _must_ be of this type.
 * See OES_framebuffer_objects extension.
 *
 * DESIGN NOTE: Mipmap levels are not objects in OpenGL but in our implementation they do
 *              have a name due to being FrameBufferAttachable. This redundancy was the lesser of two evils:
 *              either we made mipmaps have a name or renderbuffers would have to inherit from both NamedItem and
 *              FrameBufferAttachable which would be a pain to implement in C.
 *
 *              Hence, the class diagram looks like this:
 *
 *
 *                             +------------+
 *                             |  NamedItem |
 *                             +------------+
 *                                    ^
 *                                    |
 *                   +----------------+-----------------------------+
 *                   |                |                             |
 *                   |         +------+------+        * +-----------+-----------+     +---------------+
 *                   |         | FrameBuffer |--------->| FrameBufferAttachable |---->| RenderSurface |
 *                   |         +-------------+          +-----------------------+     +---------------+
 *                   |                                              ^
 *                   |                                              |
 *                   |                             +----------------+--------+
 *                   |                             |                         |
 *              +----+-----+  < belongs  *  +------+------+          +-------+------+
 *              | Texture  |<>--------------| MipMapLevel |          | RenderBuffer |
 *              +----------+                +-------------+          +--------------+
 *
 */
typedef struct GLES1FrameBufferAttachableRec
{	
	/* This struct must be the first variable */
	GLES1NamedItem   sNamedItem;
	
    /*
	 * If sRenderSurface.hRenderTarget is 0, no render surface has been allocated.
	 * See GetFrameBufferCompleteness(), glTexImage2D() and glRenderbufferStorageOES()
	 */
	EGLRenderSurface *psRenderSurface;

	/* If the texture behind the fbo has been ghosted the meminfo may be destroyed */
	IMG_BOOL			bGhosted;

	/* Can be either GL_TEXTURE for a mipmap level or GL_RENDERBUFFER_OES for a renderbuffer */
    GLenum           eAttachmentType;

} GLES1FrameBufferAttachable;

/* Forward declaration */
struct GLESMipMapLevelRec;

/*
 * Renderbuffer object.
 */
typedef struct GLESRenderBufferRec
{
	/* This struct must be the first variable */
	GLES1FrameBufferAttachable sFBAttachable;
   
	GLenum          eRequestedFormat;
	IMG_UINT32      ui32Width;
	IMG_UINT32      ui32Height;
	IMG_UINT8       ui8RedSize;
	IMG_UINT8       ui8GreenSize;
	IMG_UINT8       ui8BlueSize;
	IMG_UINT8       ui8AlphaSize;
	IMG_UINT8       ui8DepthSize;
	IMG_UINT8       ui8StencilSize;

	/* Can we load from this buffer? Only relevant for depth/stencil */
	IMG_BOOL        bInitialised; 

	/* Allocated memory in bytes. This attrib is derived from the previous */
	IMG_UINT32      ui32AllocatedBytes;
	PVRSRV_CLIENT_MEM_INFO *psMemInfo;

#if defined(GLES1_EXTENSION_EGL_IMAGE)
	EGLImage *psEGLImageSource;
	EGLImage *psEGLImageTarget;
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */

} GLESRenderBuffer;

#define GLES1_COLOR_ATTACHMENT		0
#define GLES1_DEPTH_ATTACHMENT		1
#define GLES1_STENCIL_ATTACHMENT	2
#define GLES1_MAX_ATTACHMENTS		3


/*
 * Framebuffer object.
 */
typedef struct GLESFrameBufferRec
{
 	/* This struct must be the first variable */
	GLES1NamedItem     sNamedItem;

	GLenum             eStatus;

	EGLcontextMode     sMode;
	EGLDrawableParams  sDrawParams;
	EGLDrawableParams  sReadParams;

	/*
	 * If any given pointer in this array is NULL it means no
	 * image is attached to that attachment point.
	 */
	GLES1FrameBufferAttachable *apsAttachment[GLES1_MAX_ATTACHMENTS];

} GLESFrameBuffer;


typedef struct GLESFrameBufferObjectMachineRec 
{
	GLESFrameBuffer  *psActiveFrameBuffer; 
 	GLESRenderBuffer *psActiveRenderBuffer; 

	GLESFrameBuffer  sDefaultFrameBuffer;

} GLESFrameBufferObjectMachine;


IMG_VOID SetupRenderBufferNameArray(GLES1NamesArray *psNamesArray);
IMG_VOID SetupFrameBufferObjectNameArray(GLES1NamesArray *psNamesArray);

IMG_VOID FreeFrameBufferState(GLES1Context *gc);
IMG_BOOL CreateFrameBufferState(GLES1Context *gc, EGLcontextMode *psMode);
IMG_VOID RemoveFrameBufferAttachment(GLES1Context *gc, IMG_BOOL bIsRenderBuffer, IMG_UINT32 ui32Name);
GLenum   GetFrameBufferCompleteness(GLES1Context *gc);
IMG_VOID ChangeDrawableParams(GLES1Context *gc, GLESFrameBuffer *psFrameBuffer, 
								EGLDrawableParams *psReadParams, EGLDrawableParams *psDrawParams);

IMG_BOOL IsColorAttachmentTwiddled(GLES1Context *gc, GLESFrameBuffer *psFrameBuffer);
IMG_MEMLAYOUT GetColorAttachmentMemFormat(GLES1Context *gc, GLESFrameBuffer *psFrameBuffer);

IMG_VOID DumpTextureRenderTarget(GLES1Context *gc, struct GLESMipMapLevelRec *psLevel);

/* This is called by either glTexImage2D or glRenderBufferStorage */
IMG_VOID FBOAttachableHasBeenModified(GLES1Context *gc, GLES1FrameBufferAttachable *psAttachment);

IMG_VOID DestroyFBOAttachableRenderSurface(GLES1Context *gc, GLES1FrameBufferAttachable *psAttachment);
IMG_BOOL FlushAttachableIfNeeded(GLES1Context *gc, GLES1FrameBufferAttachable *psAttachment, IMG_UINT32 ui32KickFlags);
IMG_BOOL FlushRenderSurface(GLES1Context *gc, EGLRenderSurface *psRenderSurface, IMG_UINT32 ui32KickFlags);
IMG_BOOL FlushAllUnflushedFBO(GLES1Context *gc, IMG_BOOL bWaitForHW);

#if defined(GLES1_EXTENSION_EGL_IMAGE)
IMG_BOOL SetupRenderbufferFromEGLImage(GLES1Context *gc, GLESRenderBuffer *psRenderBuffer);
IMG_VOID ReleaseImageFromRenderbuffer(GLESRenderBuffer *psRenderBuffer);
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */

#endif /* _FBO_ */
