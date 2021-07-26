/******************************************************************************
 * Name         : tls.h
 *
 * Copyright    : 2003-2005 by Imagination Technologies Limited.
 * 				  All rights reserved. No part of this software, either
 * 				  material or conceptual may be copied or distributed,
 * 				  transmitted, transcribed, stored in a retrieval system or
 * 				  translated into any human or computer language in any form
 * 				  by any means, electronic, mechanical, manual or other-wise,
 * 				  or disclosed to third parties without the express written
 * 				  permission of Imagination Technologies Limited, HomePark
 * 				  Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * Modifications: 
 * $Log: tls.h $
 *
 *  --- Revision Logs Removed --- 
 ********************************************************************************/


#ifndef _tls_h_
#define _tls_h_

#include "drvegl.h"
#include "servicesext.h"
#include "metrics.h"
#include "pvr_metrics.h"
#include "srv.h"

#define EGL_MAX_NUM_DISPLAYS 10

/* dummy functions stuff */
#if (defined(SUPPORT_OPENGLES1) && defined(SUPPORT_OPENGLES2)) || defined(API_MODULES_RUNTIME_CHECKED)
typedef unsigned int    IMG_GLenum;
typedef unsigned char   IMG_GLboolean;
typedef int             IMG_GLint;
typedef int             IMG_GLsizei;
typedef void            IMG_GLvoid;
typedef unsigned int    IMG_GLuint;
typedef unsigned char   IMG_GLubyte;

typedef void * (* DUMMY_MAPBUFFER)(IMG_GLenum target, IMG_GLenum access);
typedef IMG_GLboolean (* DUMMY_UNMAPBUFFER)(IMG_GLenum target);
typedef void (* DUMMY_GETBUFFERPOINTERV)(IMG_GLenum target, IMG_GLenum pname, void **params);

typedef void *IMG_GLeglImageOES;

typedef void (* DUMMY_EGLIMAGETARGETTEXTURE2D)(IMG_GLenum target, IMG_GLeglImageOES image);
typedef void (* DUMMY_EGLIMAGETARGETRENDERBUFFERSTORAGE)(IMG_GLenum target, IMG_GLeglImageOES image);

typedef void (* DUMMY_MULTIDRAWARRAYS)(IMG_GLenum mode, IMG_GLint *first, IMG_GLsizei *count, IMG_GLsizei primcount);
typedef void (* DUMMY_MULTIDRAWELEMENTS)(IMG_GLenum mode, const IMG_GLsizei *count,	IMG_GLenum type, const IMG_GLvoid* *indices, IMG_GLsizei primcount);

typedef void (* DUMMY_GETTEXSTREAMDEVICEATTRIBUTE)(IMG_GLint device, IMG_GLenum pname, IMG_GLint *params);
typedef const IMG_GLubyte * (* DUMMY_GETTEXSTREAMDEVICENAME)(IMG_GLint device);
typedef void (* DUMMY_TEXBINDSTREAM)(IMG_GLint device, IMG_GLint deviceoffset);
typedef void (* DUMMY_TEXUNBINDSTREAM)(IMG_GLint device, IMG_GLint deviceoffset);

typedef void (* DUMMY_BINDVERTEXARRAY)(IMG_GLuint vertexarray);
typedef void (* DUMMY_DELETEVERTEXARRAY)(IMG_GLsizei n, const IMG_GLuint *vertexarrays);
typedef void (* DUMMY_GENVERTEXARRAY)(IMG_GLsizei n, const IMG_GLuint *vertexarrays);
typedef IMG_GLboolean (* DUMMY_ISVERTEXARRAY)(IMG_GLuint vertexarray);

#endif /* (defined(SUPPORT_OPENGLES1) && defined(SUPPORT_OPENGLES2)) || defined(API_MODULES_RUNTIME_CHECKED) */

typedef struct Global_TAG
{
    KEGL_DISPLAY            asDisplay[EGL_MAX_NUM_DISPLAYS];
    EGLint                  dpyCount;
    EGLint                  refCount;
    PVRSRV_MUTEX_HANDLE     hEGLGlobalResource;

#if defined(DEBUG)
    PVRSRV_MUTEX_HANDLE     hEGLMemTrackingResource;
#endif

    IMGEGLAppHints          sAppHints;
    SrvSysContext           sSysContext;

    /* These were added for API_MODULES_RUNTIME_CHECKED, but to make */
    /* code more readable, these are initialised and used also when  */
    /* it is not defined.                                            */
    EGLint                  baseES1VGConfigVariants;
    EGLint                  pbufferES1VGConfigVariants;
    EGLint                  baseES2ConfigVariants;
    EGLint                  baseOGLConfigVariants;
    EGLint                  pbufferOGLConfigVariants;
    IMG_BOOL                bApiModuleDetected[IMGEGL_CONTEXT_TYPEMAX];

#if defined(SUPPORT_OPENGLES1) || defined(API_MODULES_RUNTIME_CHECKED)
    IMG_BOOL                bHaveOGLES1Functions;
    IMG_HANDLE              hOGLES1Drv;
    IMG_OGLES1EGL_Interface spfnOGLES1;
#endif

#if defined(SUPPORT_OPENGLES2) || defined(API_MODULES_RUNTIME_CHECKED)
    IMG_BOOL                bHaveOGLES2Functions;
    IMG_HANDLE              hOGLES2Drv;
    IMG_OGLES2EGL_Interface spfnOGLES2;
#endif

#if defined(SUPPORT_OPENGL) || defined(API_MODULES_RUNTIME_CHECKED)
    IMG_BOOL                bHaveOGLFunctions;
    IMG_HANDLE              hOGLDrv;
    IMG_OGLEGL_Interface    spfnOGL;
#endif

#if defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX) || defined(API_MODULES_RUNTIME_CHECKED)
    IMG_BOOL                bHaveOVGFunctions;
    IMG_HANDLE              hOVGDrv;
    IMG_OVGEGL_Interface    spfnOVG;
#endif

#if defined(SUPPORT_OPENCL) || defined(API_MODULES_RUNTIME_CHECKED)
    IMG_BOOL                bHaveOCLFunctions;
    IMG_HANDLE              hOCLDrv;
    IMG_OCLEGL_Interface    spfnOCL;
#endif

#if (defined(SUPPORT_OPENGLES1) && defined(SUPPORT_OPENGLES2)) || defined(API_MODULES_RUNTIME_CHECKED)
    DUMMY_MAPBUFFER                         pfnMapBufferGLES1;
    DUMMY_MAPBUFFER                         pfnMapBufferGLES2;

    DUMMY_UNMAPBUFFER                       pfnUnmapBufferGLES1;
    DUMMY_UNMAPBUFFER                       pfnUnmapBufferGLES2;

    DUMMY_GETBUFFERPOINTERV                 pfnGetBufferPointervGLES1;
    DUMMY_GETBUFFERPOINTERV                 pfnGetBufferPointervGLES2;

    DUMMY_EGLIMAGETARGETTEXTURE2D           pfnEGLImageTargetTexture2DGLES1;
    DUMMY_EGLIMAGETARGETTEXTURE2D           pfnEGLImageTargetTexture2DGLES2;

    DUMMY_EGLIMAGETARGETRENDERBUFFERSTORAGE pfnEGLImageTargetRenderbufferStorageGLES1;
    DUMMY_EGLIMAGETARGETRENDERBUFFERSTORAGE pfnEGLImageTargetRenderbufferStorageGLES2;

    DUMMY_MULTIDRAWARRAYS                   pfnMultiDrawArraysGLES1;
    DUMMY_MULTIDRAWARRAYS                   pfnMultiDrawArraysGLES2;

    DUMMY_MULTIDRAWELEMENTS                 pfnMultiDrawElementsGLES1;
    DUMMY_MULTIDRAWELEMENTS                 pfnMultiDrawElementsGLES2;

    DUMMY_GETTEXSTREAMDEVICEATTRIBUTE       pfnGetTexStreamDeviceAttributeGLES1;
    DUMMY_GETTEXSTREAMDEVICEATTRIBUTE       pfnGetTexStreamDeviceAttributeGLES2;
    
    DUMMY_GETTEXSTREAMDEVICENAME            pfnGetTexStreamDeviceNameGLES1;
    DUMMY_GETTEXSTREAMDEVICENAME            pfnGetTexStreamDeviceNameGLES2;

    DUMMY_TEXBINDSTREAM                     pfnTexBindStreamGLES1;
    DUMMY_TEXBINDSTREAM                     pfnTexBindStreamGLES2;

    DUMMY_TEXUNBINDSTREAM                   pfnTexUnbindStreamGLES2;

    DUMMY_BINDVERTEXARRAY                   pfnBindVertexArrayGLES1;
    DUMMY_BINDVERTEXARRAY                   pfnBindVertexArrayGLES2;

    DUMMY_DELETEVERTEXARRAY                 pfnDeleteVertexArrayGLES1;
    DUMMY_DELETEVERTEXARRAY                 pfnDeleteVertexArrayGLES2;

    DUMMY_GENVERTEXARRAY                    pfnGenVertexArrayGLES1;
    DUMMY_GENVERTEXARRAY                    pfnGenVertexArrayGLES2;

    DUMMY_ISVERTEXARRAY                     pfnIsVertexArrayGLES1;
    DUMMY_ISVERTEXARRAY                     pfnIsVertexArrayGLES2;

#endif /* (defined(SUPPORT_OPENGLES1) && defined(SUPPORT_OPENGLES2)) || defined(API_MODULES_RUNTIME_CHECKED) */

#if defined(EGL_EXTENSION_ANDROID_BLOB_CACHE) 

	EGLSetBlobFunc							pfnSetBlob;
	EGLGetBlobFunc							pfnGetBlob;

#endif
} EGLGlobal;


struct tls_tag
{
    /* The last EGL non EGL_SUCCESS error code returned by an EGL API call */
    EGLint lastError;
    
    /*
    ** We have an array of surfaces per thread because you can have 2 different
    ** API contexts (1 OGLES and 1 OVG) current at the same time rendering
    ** to 2 different surfaces
    */
    KEGL_SURFACE *apsCurrentReadSurface[IMGEGL_NUMBER_OF_APIS];
    KEGL_SURFACE *apsCurrentDrawSurface[IMGEGL_NUMBER_OF_APIS];

    /*
    ** We have an array of contexts per thread because you can have different
    ** API contexts current (1 OGLES and/or 1 OVG and/or OPENGL) at the same time
    */
    KEGL_CONTEXT *apsCurrentContext[IMGEGL_NUMBER_OF_APIS];

    IMG_UINT32 ui32API;

    IMG_UINT32 ui32ThreadID;

    EGLGlobal *psGlobalData;

#if defined(TIMING) || defined(DEBUG)
    PVR_Temporal_Data asTimes[IMGEGL_NUM_TIMERS];

    IMG_FLOAT fCPUSpeed;
#endif /* defined(TIMING) || defined(DEBUG) */
};


TLS TLS_Open(IMG_BOOL (*init)(TLS tls));
IMG_VOID TLS_Close(IMG_VOID (*deinit)(TLS tls));

IMG_BOOL _TlsInit(TLS psTls);

#endif
