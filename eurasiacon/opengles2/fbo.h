/**************************************************************************
 * Name         : fbo.h
 * Author       : 
 * Created      : 
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
 * Modifications:-
 * $Log: fbo.h $
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
typedef struct GLES2FrameBufferAttachableRec
{	
	/* This struct must be the first variable */
	GLES2NamedItem   sNamedItem;
	
    /*
	 * If sRenderSurface.hRenderTarget is 0, no render surface has been allocated.
	 * See GetFrameBufferCompleteness(), glTexImage2D() and glRenderbufferStorageOES()
	 */
	EGLRenderSurface *psRenderSurface;

	/* If the texture behind the fbo has been ghosted the meminfo may be destroyed */
	IMG_BOOL			bGhosted;

	/* Can be either GL_TEXTURE for a mipmap level or GL_RENDERBUFFER_OES for a renderbuffer */
    GLenum           eAttachmentType;

#if defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
	/* Indicates whether this attachment was created with an anti-aliased surface */
	IMG_UINT32       ui32Samples;
#endif


} GLES2FrameBufferAttachable;

/* Forward declaration */
struct GLES2MipMapLevelRec;

/*
 * Renderbuffer object.
 */
typedef struct GLES2RenderBufferRec
{
	/* This struct must be the first variable */
	GLES2FrameBufferAttachable sFBAttachable;
   
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

#if defined(GLES2_EXTENSION_EGL_IMAGE)
	EGLImage *psEGLImageSource;
	EGLImage *psEGLImageTarget;
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */

#if defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
	/* for renderbuffers the number of samples is stored here rather than 
	 * in the attachable record as it is created as multi-sampled before
	 * it is attached to a framebuffer. */
	IMG_UINT32			ui32Samples;
#endif

} GLES2RenderBuffer;

#define GLES2_COLOR_ATTACHMENT		0
#define GLES2_DEPTH_ATTACHMENT		1
#define GLES2_STENCIL_ATTACHMENT	2
#define GLES2_MAX_ATTACHMENTS		3


/*
 * Framebuffer object.
 */
typedef struct GLES2FrameBufferRec
{
 	/* This struct must be the first variable */
	GLES2NamedItem     sNamedItem;

	GLenum             eStatus;

	EGLcontextMode     sMode;
	EGLDrawableParams  sDrawParams;
	EGLDrawableParams  sReadParams;

	/*
	 * If any given pointer in this array is NULL it means no
	 * image is attached to that attachment point.
	 */
	GLES2FrameBufferAttachable *apsAttachment[GLES2_MAX_ATTACHMENTS];

} GLES2FrameBuffer;


typedef struct GLES2FrameBufferObjectMachineRec 
{
	GLES2FrameBuffer  *psActiveFrameBuffer; 
 	GLES2RenderBuffer *psActiveRenderBuffer; 

	GLES2FrameBuffer  sDefaultFrameBuffer;

} GLES2FrameBufferObjectMachine;


IMG_VOID SetupRenderBufferNameArray(GLES2NamesArray *psNamesArray);
IMG_VOID SetupFrameBufferObjectNameArray(GLES2NamesArray *psNamesArray);

IMG_VOID FreeFrameBufferState(GLES2Context *gc);
IMG_BOOL CreateFrameBufferState(GLES2Context *gc, EGLcontextMode *psMode);
IMG_VOID RemoveFrameBufferAttachment(GLES2Context *gc, IMG_BOOL bIsRenderBuffer, IMG_UINT32 ui32Name);
GLenum   GetFrameBufferCompleteness(GLES2Context *gc);
IMG_VOID ChangeDrawableParams(GLES2Context *gc, GLES2FrameBuffer *psFrameBuffer, 
								EGLDrawableParams *psReadParams, EGLDrawableParams *psDrawParams);

IMG_MEMLAYOUT GetColorAttachmentMemFormat(GLES2Context *gc, GLES2FrameBuffer *psFrameBuffer);
IMG_VOID DumpTextureRenderTarget(GLES2Context *gc, struct GLES2MipMapLevelRec *psLevel);
 
/* This is called by either glTexImage2D or glRenderBufferStorage */
IMG_VOID FBOAttachableHasBeenModified(GLES2Context *gc, GLES2FrameBufferAttachable *psAttachment);

IMG_VOID DestroyFBOAttachableRenderSurface(GLES2Context *gc, GLES2FrameBufferAttachable *psAttachment);
IMG_BOOL FlushAttachableIfNeeded(GLES2Context *gc, GLES2FrameBufferAttachable *psAttachment, IMG_UINT32 ui32KickFlags);
IMG_BOOL FlushRenderSurface(GLES2Context *gc, EGLRenderSurface *psRenderSurface, IMG_UINT32 ui32KickFlags);
IMG_BOOL FlushAllUnflushedFBO(GLES2Context *gc, IMG_BOOL bWaitForHW);

#if defined(GLES2_EXTENSION_EGL_IMAGE)
IMG_BOOL SetupRenderbufferFromEGLImage(GLES2Context *gc, GLES2RenderBuffer *psRenderBuffer);
IMG_VOID ReleaseImageFromRenderbuffer(GLES2Context *gc, GLES2RenderBuffer *psRenderBuffer);
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */

#endif /* _FBO_ */
