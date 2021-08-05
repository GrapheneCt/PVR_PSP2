/*******************************************************************************
 * Name         : gles1test1.c
 *
 * Copyright    : 2006 by Imagination Technologies Limited. All rights reserved.
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
 * Modifications:-
 * $Log: gles1test1.c $
 *
 *******************************************************************************/

#include <GLES/gl.h>
#include <GLES/glext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(__psp2__)

#include <kernel.h>

unsigned int sceLibcHeapSize = 16 * 1024 * 1024;

SCE_USER_MODULE_LIST("app0:libgpu_es4_ext.suprx", "app0:libIMGEGL.suprx");

#include <services.h>
#endif

#if defined(SUPPORT_ANDROID_PLATFORM)
#include "eglhelper.h"
#endif

#define TEX_SIZE 256

static int handle_events(void);

static GLubyte *texdata[TEX_SIZE*TEX_SIZE*4];

static int frameStop;

static const char *const egl_error_strings[] =
{
	"EGL_SUCCESS",
	"EGL_NOT_INITIALIZED",
	"EGL_BAD_ACCESS",
	"EGL_BAD_ALLOC",
	"EGL_BAD_ATTRIBUTE",    
	"EGL_BAD_CONFIG",
	"EGL_BAD_CONTEXT",   
	"EGL_BAD_CURRENT_SURFACE",
	"EGL_BAD_DISPLAY",
	"EGL_BAD_MATCH",
	"EGL_BAD_NATIVE_PIXMAP",
	"EGL_BAD_NATIVE_WINDOW",
	"EGL_BAD_PARAMETER",
	"EGL_BAD_SURFACE",
	"EGL_CONTEXT_LOST"
};


/***********************************************************************************
 Function Name      : handle_egl_error
 Inputs             : None
 Outputs            : None
 Returns            : None
 Description        : Displays an error message for an egl error
************************************************************************************/
static void handle_egl_error(char *name)
{
	EGLint error_code = eglGetError();

	printf ("'%s' returned egl error '%s' (0x%x)\n",
			name, egl_error_strings[error_code-EGL_SUCCESS], error_code);

	exit (1);
}


/***********************************************************************************
 Function Name      : init_texture
 Inputs             : None
 Outputs            : None
 Returns            : None
 Description        : Creates a checker-board texture
************************************************************************************/
static void init_texture(void)
{
    GLuint i,j;
    GLubyte *lpTex = texdata;

    for (j=0; j<TEX_SIZE; j++)
    {
        for (i=0; i<TEX_SIZE; i++)
        {
            if ((i ^ j) & 0x8)
            {
                lpTex[0] = lpTex[1] = lpTex[2] = 0x00; 
				/* Set full alpha */
				lpTex[3] = 0xff;
            }
            else
            {
                lpTex[0] = lpTex[1] = lpTex[2] = lpTex[3] = 0xff;
            }

            lpTex += 4;
        }
    } 
}


/***********************************************************************************
 Function Name      : init
 Inputs             : None
 Outputs            : None
 Returns            : None
 Description        : Initialises vertex and texture data
************************************************************************************/
static void init(void) 
{
	static GLfloat vertices[] =
		{-0.5f,-0.5f,  0.0f,0.5f,  0.5f,-0.5f,  -0.5f,-0.5f,  0.0f,0.5f,  0.5f,-0.5f};
    
	static GLfloat colors[]   =
		{1.0f,  0.4f,  0.4f,  1.0f,
         0.3f,  0.4f,  1.0f,  1.0f,
         0.7f,  1.0f,  0.4f,  1.0f,
         0.66f, 0.66f, 0.66f, 1.0f,
         0.33f, 0.33f, 0.33f, 1.0f,
         1.0f,  1.0f,  1.0f,  1.0f};

	static GLfloat texcoord[] = 
		{0.0f,0.0f,  1.0f,0.0f,  1.0f,1.0f,  0.0f,0.0f, 1.0f,0.0f,  1.0f,1.0f};

	glClearColor(1.0, 0.0, 0.0, 1.0); // R,G,B,A

	glShadeModel(GL_SMOOTH);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glVertexPointer  (2, GL_FLOAT, 0, vertices);
	glColorPointer   (4, GL_FLOAT, 0, colors);
	glTexCoordPointer(2, GL_FLOAT, 0, texcoord);

	init_texture();

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 TEX_SIZE, TEX_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, texdata);

	glGenerateMipmapOES(GL_TEXTURE_2D);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
}

/***********************************************************************************
 Function Name      : display
 Inputs             : None
 Outputs            : None
 Returns            : None
 Description        : Draws 2 rotating triangles
************************************************************************************/
static void display(void)
{
	static int framecount=0;

	glClear(GL_COLOR_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);

	/*
	// Triangle 1
	*/
	glLoadIdentity();

	glTranslatef(-0.5,0,0);

	glRotatef(5.0f*(GLfloat)framecount,0,1,0);

	glEnable(GL_TEXTURE_2D);

	glDrawArrays (GL_TRIANGLES, 0, 3);

	glDisable(GL_TEXTURE_2D);

	/*
	// Triangle 2
	*/
	glLoadIdentity();

	glTranslatef(0.5,0,0);

	glRotatef(-5.0f*(GLfloat)framecount*2,0,1,0); /* Twice the speed of triangle 1 */

	glDrawArrays (GL_TRIANGLES, 3, 3);

	framecount++;
}


/***********************************************************************************
 Function Name      : EglMain
 Inputs             : eglDisplay, eglWindow
 Outputs            : None
 Returns            : Error
 Description        : EGL portion of 'main' function
************************************************************************************/

#if !defined(SUPPORT_ANDROID_PLATFORM)
static
#endif
int EglMain(EGLNativeDisplayType eglDisplay, EGLNativeWindowType eglWindow)
{
	EGLDisplay dpy;
	EGLSurface surface;
	EGLContext context;
	EGLConfig configs[2];
	EGLBoolean eRetStatus;
	EGLint major, minor; 
#if !defined(SUPPORT_ANDROID_PLATFORM)
	EGLint config_count;
#if defined(__psp2__)
	EGLint cfg_attribs[] = { EGL_BUFFER_SIZE,    EGL_DONT_CARE,
							EGL_RED_SIZE,       8,
							EGL_GREEN_SIZE,     8,
							EGL_BLUE_SIZE,      8,
							EGL_ALPHA_SIZE,		8,
							EGL_NONE };
#else
	EGLint cfg_attribs[] = {EGL_BUFFER_SIZE,    EGL_DONT_CARE,
							EGL_RED_SIZE,       5,
							EGL_GREEN_SIZE,     6,
							EGL_BLUE_SIZE,      5,
							EGL_DEPTH_SIZE,     8,
							EGL_NONE};
#endif
#endif

	int i;

	dpy = eglGetDisplay(eglDisplay);

	eRetStatus = eglInitialize(dpy, &major, &minor);

	if (eRetStatus != EGL_TRUE)
	{
		handle_egl_error("eglInitialize");
	}
	
#if defined(SUPPORT_ANDROID_PLATFORM)
	configs[0] = findMatchingWindowConfig(dpy, EGL_OPENGL_ES_BIT, eglWindow);
	configs[1] = (EGLConfig)0;
#else
	eRetStatus = eglGetConfigs(dpy, configs, 2, &config_count);

	if (eRetStatus != EGL_TRUE)
	{
		handle_egl_error("eglGetConfigs");		
	}

	eRetStatus = eglChooseConfig(dpy, cfg_attribs, configs, 2, &config_count);

	if (eRetStatus != EGL_TRUE)
	{
		handle_egl_error("eglChooseConfig");
	}
#endif

	surface = eglCreateWindowSurface(dpy, configs[0], eglWindow, NULL);

	if (surface == EGL_NO_SURFACE)
	{
		handle_egl_error("eglCreateWindowSurface");
	}

	context = eglCreateContext(dpy, configs[0], EGL_NO_CONTEXT, NULL);

	if (context == EGL_NO_CONTEXT)
	{
		handle_egl_error("eglCreateContext");
	}

	eRetStatus = eglMakeCurrent(dpy, surface, surface, context);

	if (eRetStatus != EGL_TRUE)
	{
		handle_egl_error("eglMakeCurrent");		
	}
  
	eglSwapInterval(dpy, (EGLint)1);

	init();

	for(i=0; !frameStop || (i < frameStop); i++)
	{
		if(handle_events() < 0)
		{
			break;
		}

		display();

		eglSwapBuffers(dpy, surface);
   	}

	eglDestroyContext(dpy, context);

	eglDestroySurface(dpy, surface);

	eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

	eglTerminate(dpy);

	return 0;
}

/***********************************************************************************
 Function Name      : main
 Inputs             : argc, argv
 Outputs            : None
 Returns            : Error
 Description        : Executable 'main' function
************************************************************************************/


#if !defined(SUPPORT_ANDROID_PLATFORM)

/* NULLWS */

static int handle_events(void)
{
	return 0;
}

int main(int argc, char *argv[])
{
	frameStop = (argc >= 2) ? atol(argv[1]) : 1;

#if defined(__psp2__)

	PVRSRV_PSP2_APPHINT hint;
	sceClibMemset(&hint, 0, sizeof(PVRSRV_PSP2_APPHINT));

	sceClibStrncpy(hint.szGLES1, "app0:libGLESv1_CM.suprx", 256);
	sceClibStrncpy(hint.szWindowSystem, "app0:libpvrPSP2_WSEGL.suprx", 256);
	hint.bDisableMetricsOutput = IMG_TRUE;
	hint.bDumpProfileData = IMG_FALSE;
	hint.ui32ProfileStartFrame = 0;
	hint.ui32ProfileEndFrame = 0;
	hint.ui32ExternalZBufferMode = IMG_FALSE;
	hint.ui32ExternalZBufferXSize = 0;
	hint.ui32ExternalZBufferYSize = 0;
	hint.ui32ParamBufferSize = 16 * 1024 * 1024;
	hint.ui32PDSFragBufferSize = 50 * 1024;
	hint.ui32DriverMemorySize = 16 * 1024 * 1024;

	hint.bDisableHWTextureUpload = IMG_FALSE;
	hint.bDisableHWTQBufferBlit = IMG_FALSE;
	hint.bDisableHWTQMipGen = IMG_TRUE;
	hint.bDisableHWTQNormalBlit = IMG_FALSE;
	hint.bDisableHWTQTextureUpload = IMG_FALSE;
	hint.bDisableStaticPDSPixelSAProgram = IMG_FALSE;
	hint.bDisableUSEASMOPT = IMG_FALSE;
	hint.bDumpShaders = IMG_FALSE;
	hint.bEnableAppTextureDependency = IMG_FALSE;
	hint.bEnableMemorySpeedTest = IMG_TRUE;
	hint.bEnableStaticMTECopy = IMG_TRUE;
	hint.bEnableStaticPDSVertex = IMG_TRUE;
	hint.bFBODepthDiscard = IMG_TRUE;
	hint.bOptimisedValidation = IMG_TRUE;
	hint.ui32DefaultIndexBufferSize = 200 * 1024;
	hint.ui32DefaultPDSVertBufferSize = 50 * 1024;
	hint.ui32DefaultPregenMTECopyBufferSize = 50 * 1024;
	hint.ui32DefaultPregenPDSVertBufferSize = 80 * 1024;
	hint.ui32DefaultVDMBufferSize = 20 * 1024;
	hint.ui32DefaultVertexBufferSize = 200 * 1024;
	hint.ui32FlushBehaviour = 0;
	hint.ui32MaxVertexBufferSize = 800 * 1024;
	hint.ui32OGLES1UNCTexHeapSize = 4 * 1024;
	hint.ui32OGLES1CDRAMTexHeapSize = 256 * 1024;
	hint.bOGLES1EnableUNCAutoExtend = IMG_TRUE;
	hint.bOGLES1EnableCDRAMAutoExtend = IMG_TRUE;
	hint.ui32OGLES1SwTexOpThreadNum = 1;
	hint.ui32OGLES1SwTexOpThreadPriority = 70;
	hint.ui32OGLES1SwTexOpThreadAffinity = 0;
	hint.ui32OGLES1SwTexOpMaxUltNum = 16;

	PVRSRVCreateVirtualAppHint(&hint);

	frameStop = 50000;
#endif

	return EglMain(EGL_DEFAULT_DISPLAY, 0);
}

#else /* !defined(SUPPORT_ANDROID_PLATFORM) */

static int			giQuit;
static unsigned int	giWidth;
static unsigned int	giHeight;

static int handle_events(void)
{
	if(giQuit)
	{
		giQuit = 0;
		return -1;
	}

	if(giWidth != 0 && giHeight != 0)
	{
		glViewport(0, 0, giWidth, giHeight);
		giWidth  = 0;
		giHeight = 0;
	}

	return 0;
}

void ResizeWindow(unsigned int width, unsigned int height)
{
	giWidth = width;
	giHeight = height;
}

void RequestQuit(void)
{
	giQuit = 1;
}

#endif /* !defined(SUPPORT_ANDROID_PLATFORM) */


/******************************************************************************
 End of file (gles1test1.c)
******************************************************************************/
