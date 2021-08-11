/******************************************************************************
 * Name         : gles2test1.c
 *
 * Copyright    : 2006-2007 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any  human or computer language in any form
 *              : by any means, electronic, mechanical, manual or otherwise, 
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited, 
 *              : Home Park Estate, Kings Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * $Log: gles2test1.c $
 *****************************************************************************/

#if defined(__psp2__)

#include <kernel.h>

unsigned int sceLibcHeapSize = 16 * 1024 * 1024;

SCE_USER_MODULE_LIST("app0:libgpu_es4_ext.suprx", "app0:libIMGEGL.suprx");

#include <services.h>
#endif

#include <GLES2/gl2.h>
#include <EGL/egl.h>

#if defined(SUPPORT_ANDROID_PLATFORM)
#include "eglhelper.h"
#endif

#ifndef GL_ES_VERSION_2_0
#error ("wrong header file")
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "maths.h"

#define TEX_SIZE 32
#define WINDOW_WIDTH 200
#define WINDOW_HEIGHT 200

#if defined(SUPPORT_ANDROID_PLATFORM)
#define LOG_TAG "GLES2Test1"
#include <cutils/log.h>
#ifndef ALOGI
#define ALOGI LOGI
#endif
#ifndef ALOGE
#define ALOGE LOGE
#endif
#define INFO  ALOGI
#define ERROR ALOGE
#else /* defined(SUPPORT_ANDROID_PLATFORM) */
#define INFO  printf
#define ERROR printf
#endif


static int handle_events(void);

//#define PBUFFER 1

static int frameStop;

static int mvp_pos[2];
static int hProgramHandle[2];
static int attriblocation[2];

static GLfloat projection[4][4] = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
static GLfloat modelview[4][4] = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
static GLfloat mvp[4][4] = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};

#define FLOAT_TO_FIXED(x)	(long)((x) * 65536.0f)

static const char * const error_strings [] =
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

static void handle_egl_error (char *name)
{
	EGLint error_code = eglGetError ();
	ERROR("'%s' returned egl error '%s' (0x%x)\n",
		   name, error_strings[error_code-EGL_SUCCESS], error_code);
	exit (1);
}

#define FILEPATH ""

static char const *const apszProgramFileName[] = 
{
	FILEPATH "glsltest1_vertshader.txt",
	FILEPATH "glsltest1_fragshaderA.txt",
	FILEPATH "glsltest1_fragshaderB.txt"
};

#define NFILES (int)(sizeof(apszProgramFileName)/sizeof(apszProgramFileName[0]))


#ifdef FILES_EMBEDDED

#include "shaders.h"

static char const * const apszFiles[NFILES]=
{
	glsltest1_vertshader,
	glsltest1_fragshaderA,
	glsltest1_fragshaderB
};

static EGLBoolean file_load(int i, char **pcData, int *piLen)
{
	if (i < 0 || i >= NFILES || !pcData || !piLen)
		return GL_FALSE;

	*pcData = (char *)apszFiles[i];
	*piLen = strlen(apszFiles[i]);
	return GL_TRUE;
}

static void file_unload(char *pData) {}

#else /* FILES_EMBEDDED */

static EGLBoolean file_load(int i, char **pcData, int *piLen)
{
	FILE *fpShader;
	int iLen,iGot;

	if (i < 0 || i >= NFILES || !pcData || !piLen)
		return GL_FALSE;

	/* open the shader file */
	fpShader = fopen(apszProgramFileName[i], "r");

	/* Check open suceeded */
	if(!fpShader)
	{
		ERROR("Error: Failed to open shader file '%s'!\n",
			  apszProgramFileName[i]);
		return GL_FALSE;
	}

	/* To get size of file, seek to end, ftell, then rewind */
	fseek(fpShader, 0, SEEK_END);
	iLen = ftell(fpShader);
	fseek(fpShader, 0, SEEK_SET);

	*pcData = (char *)malloc(iLen + 1);

	if(*pcData == NULL)
	{
		ERROR("Error: Failed to allocate %d bytes for program '%s'!\n",
			  iLen + 1, apszProgramFileName[i]);
		fclose(fpShader);
		return GL_FALSE;
	}

	/* Read the file into the buffer */
	iGot = fread(*pcData, 1, iLen, fpShader);
	if (iGot != iLen)
	{
		// Might be ASCII vs Binary
		ERROR("Warning: Only read %u bytes of %d from '%s'!\n",
			  iGot, iLen, apszProgramFileName[i]);
	}

	/* Close the file */
	fclose(fpShader);

	/* Terminate the string */
	(*pcData)[iGot] = '\0';
	*piLen = iGot;
	return GL_TRUE;
}

static void file_unload(char *pData)
{
	free(pData);
}

#endif /* FILES_EMBEDDED */

static int init(void) 
{
	static GLfixed vertices[] =
	   {FLOAT_TO_FIXED(-0.5),FLOAT_TO_FIXED(-0.5),
		FLOAT_TO_FIXED(0),FLOAT_TO_FIXED(0.5),
		FLOAT_TO_FIXED(0.5),FLOAT_TO_FIXED(-0.5),
		FLOAT_TO_FIXED(-0.5),FLOAT_TO_FIXED(-0.5),
		FLOAT_TO_FIXED(0),FLOAT_TO_FIXED(0.5),
		FLOAT_TO_FIXED(0.5),FLOAT_TO_FIXED(-0.5)};

	static GLfloat colors[] = {0.66f, 0.66f, 0.66f,1.0f,
							  0.33f, 0.33f, 0.33f,1.0f,
							  1.0f, 1.0f, 1.0f,1.0f,
							  1.0f, 0.4f, 0.4f,1.0f,
							  0.3f, 0.4f, 1.0f,1.0f,
							  0.7f, 1.0f, 0.4f,1.0f};

	static GLfloat texcoord[] = {0.0, 0.0,
							  1.0, 0.0,
							  1.0, 1.0,
							  0.0, 0.0,
							  1.0, 0.0,
							  1.0, 1.0};

	GLubyte texdata[TEX_SIZE*TEX_SIZE*4];
	GLubyte *lpTex = texdata;
	GLuint i,j;
	char pszInfoLog[1024];
	int  nShaderStatus, nInfoLogLength;
	int hShaderHandle[3];
	int basetexture_pos;

	glClearColor (0.5, 0.0, 0.0, 1.0);
		
	for (j=0; j<TEX_SIZE; j++) {
		for (i=0; i<TEX_SIZE; i++) {
			if ((i ^ j) & 0x8) {
				lpTex[0] = lpTex[1] = lpTex[2] = 0x00; 
				/* Set full alpha */
				lpTex[3] = 0xff;
			} else {
				lpTex[0] = lpTex[1] = lpTex[2] = lpTex[3] = 0xFF;
			}
			lpTex += 4;
		}
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEX_SIZE, TEX_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, texdata);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	/* Create a program object */
	hShaderHandle[0] = glCreateShader(GL_VERTEX_SHADER);
	hShaderHandle[1] = glCreateShader(GL_FRAGMENT_SHADER);
	hShaderHandle[2] = glCreateShader(GL_FRAGMENT_SHADER);
	hProgramHandle[0] = glCreateProgram();
	hProgramHandle[1] = glCreateProgram();

	for(i=0; i<NFILES ; i++)
	{
		char *pszProgramString;
		int  nProgramLength;

		if (file_load(i,&pszProgramString,&nProgramLength)==GL_FALSE)
			return GL_FALSE;

		sprintf(pszInfoLog, "Compiling program '%s', %d bytes long\n", apszProgramFileName[i], nProgramLength);
		INFO("%s", pszInfoLog);

		/* Supply shader source */
		glShaderSource(hShaderHandle[i], 1, (const char **)&pszProgramString, &nProgramLength); 

		/* Free the program string */
		file_unload(pszProgramString);

		/* Compile the shader */
		glCompileShader(hShaderHandle[i]);

		/* Check it compiled OK */
		glGetShaderiv(hShaderHandle[i], GL_COMPILE_STATUS, &nShaderStatus); 

		if (nShaderStatus != GL_TRUE)
		{
			ERROR("Error: Failed to compile GLSL shader\n");
			glGetShaderInfoLog(hShaderHandle[i], 1024, &nInfoLogLength, pszInfoLog);
			INFO("%s", pszInfoLog);
			return GL_FALSE;
		}
		
	}
	/* Attach the shader to the programs */
	glAttachShader(hProgramHandle[0], hShaderHandle[0]);
	glAttachShader(hProgramHandle[0], hShaderHandle[1]);

	glAttachShader(hProgramHandle[1], hShaderHandle[0]);
	glAttachShader(hProgramHandle[1], hShaderHandle[2]);

	for(i=0; i < 2; i++)
	{

		glBindAttribLocation(hProgramHandle[i], 0, "position");
		glBindAttribLocation(hProgramHandle[i], 1, "inputcolor");

		/* Link the program */
		glLinkProgram(hProgramHandle[i]);

		/* Check it linked OK */
		glGetProgramiv(hProgramHandle[i], GL_LINK_STATUS, &nShaderStatus); 

		if (nShaderStatus != GL_TRUE)
		{
			ERROR("Error: Failed to link GLSL program\n");
			glGetProgramInfoLog(hProgramHandle[i], 1024, &nInfoLogLength, pszInfoLog);
			INFO("%s", pszInfoLog);
			return GL_FALSE;
		}

		glValidateProgram(hProgramHandle[i]);

		glGetProgramiv(hProgramHandle[i], GL_VALIDATE_STATUS, &nShaderStatus); 

		if (nShaderStatus != GL_TRUE)
		{
			ERROR("Error: Failed to validate GLSL program\n");
			glGetProgramInfoLog(hProgramHandle[i], 1024, &nInfoLogLength, pszInfoLog);
			INFO("%s", pszInfoLog);
			return GL_FALSE;
		}

		mvp_pos[i] = glGetUniformLocation(hProgramHandle[i], "mvp");
		basetexture_pos = glGetUniformLocation(hProgramHandle[i], "basetexture");

		glUseProgram(hProgramHandle[i]);

		glUniform1i(basetexture_pos, 0);
	}

	j = glGetError();

	if(j != GL_NO_ERROR)
	{
		ERROR("GL ERROR = %x", j);
		return GL_FALSE;
	}

	glEnableVertexAttribArray (0);
	glEnableVertexAttribArray (1);

	glVertexAttribPointer(0, 2, GL_FIXED, 0, 0, vertices);

	glVertexAttribPointer(1, 4, GL_FLOAT, 0, 0, colors);

	attriblocation[0] = glGetAttribLocation(hProgramHandle[0], "inputtexcoord");
	attriblocation[1] = glGetAttribLocation(hProgramHandle[1], "inputtexcoord");
	glVertexAttribPointer(attriblocation[1], 2, GL_FLOAT, 0, 0, texcoord);

	myIdentity(projection);
	myPersp(projection,  60.0f,	1.0f, 0.1f,	100);

	return GL_TRUE;
}


static void display(void)
{
	static int framecount=0;

	glClear (GL_COLOR_BUFFER_BIT);


	/* No texturing */
	glDisableVertexAttribArray (attriblocation[0]);
	glUseProgram(hProgramHandle[0]);


	myIdentity(modelview);

	myTranslate(modelview, -0.5, 0, -2.0f);

	myRotate(modelview, 0.0f, 1.0f, 0.0f, 5.0f * framecount);

	myMultMatrix(mvp, modelview, projection);

	glUniformMatrix4fv(mvp_pos[0], 1, GL_FALSE, &mvp[0][0]);

	glDrawArrays (GL_TRIANGLES, 0, 3);

	/* Texturing */
	glEnableVertexAttribArray (attriblocation[1]);
	glUseProgram(hProgramHandle[1]);

	myIdentity(modelview);

	myTranslate(modelview, 0.5, 0, -2.0);

	myRotate(modelview, 0, 1, 0, -5.0f * framecount);

	myMultMatrix(mvp, modelview, projection);

	glUniformMatrix4fv(mvp_pos[1], 1, GL_FALSE, &mvp[0][0]);

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
	EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
#if !defined(SUPPORT_ANDROID_PLATFORM)
	EGLint config_count;
#if defined(__psp2__)
	    EGLint cfg_attribs[] = {EGL_BUFFER_SIZE,    EGL_DONT_CARE,
							EGL_DEPTH_SIZE,		16,
							EGL_RED_SIZE,       8,
							EGL_GREEN_SIZE,     8,
							EGL_BLUE_SIZE,      8,
#if PBUFFER
							EGL_SURFACE_TYPE,   EGL_PBUFFER_BIT,
#endif
							EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
							EGL_NONE};
#else
    EGLint cfg_attribs[] = {EGL_BUFFER_SIZE,    EGL_DONT_CARE,
							EGL_DEPTH_SIZE,		16,
							EGL_RED_SIZE,       5,
							EGL_GREEN_SIZE,     6,
							EGL_BLUE_SIZE,      5,
#if PBUFFER
							EGL_SURFACE_TYPE,   EGL_PBUFFER_BIT,
#endif
							EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
							EGL_NONE};
#endif
#endif
	int i;
#if PBUFFER
	GLubyte *readbuf = NULL;
#if 0
	GLubyte filename[20];
	FILE *file = NULL;
	int x,y;
#endif
#endif

	INFO("--------------------- started ---------------------\n");

	dpy = eglGetDisplay(eglDisplay);

	eRetStatus = eglInitialize(dpy, &major, &minor);
	if( eRetStatus != EGL_TRUE )
		handle_egl_error("eglInitialize");

#if defined(SUPPORT_ANDROID_PLATFORM)
	configs[0] = findMatchingWindowConfig(dpy, EGL_OPENGL_ES2_BIT, eglWindow);
	configs[1] = (EGLConfig)0;
#else
	eRetStatus = eglGetConfigs (dpy, configs, 2, &config_count);
	if( eRetStatus != EGL_TRUE )
		handle_egl_error ("eglGetConfigs");

	eRetStatus = eglChooseConfig (dpy, cfg_attribs, configs, 2, &config_count);
	if( eRetStatus != EGL_TRUE  || !config_count)
		handle_egl_error ("eglChooseConfig");
#endif

#if PBUFFER
	{
		EGLint pbuf[] = {EGL_WIDTH, WINDOW_WIDTH, EGL_HEIGHT, WINDOW_HEIGHT, EGL_NONE};

		surface = eglCreatePbufferSurface (dpy, configs[0], pbuf);
		if (surface == EGL_NO_SURFACE)
		{
			handle_egl_error ("eglCreatePbufferSurface");
		}
	}
	readbuf = malloc(WINDOW_WIDTH*WINDOW_HEIGHT*4);

#else

	surface = eglCreateWindowSurface(dpy, configs[0], eglWindow, NULL);
	if (surface == EGL_NO_SURFACE)
	{
		handle_egl_error ("eglCreateWindowSurface");
	}
#endif

	eRetStatus = eglBindAPI(EGL_OPENGL_ES_API);
	if (eRetStatus != EGL_TRUE)
	{
		handle_egl_error ("eglBindAPI");
	}

	context = eglCreateContext (dpy, configs[0], EGL_NO_CONTEXT, context_attribs);
	if (context == EGL_NO_CONTEXT)
	{
		handle_egl_error ("eglCreateContext");
	}

	eRetStatus = eglMakeCurrent (dpy, surface, surface, context);
	if( eRetStatus != EGL_TRUE )
		handle_egl_error ("eglMakeCurrent");

	if(init() == GL_FALSE)
		goto term;

	for(i=0; !frameStop || (i < frameStop); i++)
	{
		if(handle_events() < 0)
		{
			break;
		}

		display();
#if PBUFFER
		glReadPixels(0,0,WINDOW_WIDTH, WINDOW_HEIGHT,GL_RGBA,GL_UNSIGNED_BYTE, readbuf);
#if 0
		sprintf(filename, "readpix%d.ppm",i);
		
		/* Bitmap should be written in binary mode. */
		file = fopen(filename, "wb");

		if(file)
		{
			fprintf(file, "P6\n%d %d\n255 ", WINDOW_WIDTH, WINDOW_HEIGHT); 
			
			
			for (y=WINDOW_HEIGHT-1; y >= 0; y--)
			{
				for (x=0; x < WINDOW_WIDTH; x++)
				{
					GLubyte r,g,b;

					r = readbuf[(y*WINDOW_WIDTH + x) * 4];
					g = readbuf[((y*WINDOW_WIDTH + x) * 4) + 1];
					b = readbuf[((y*WINDOW_WIDTH + x) * 4) + 2];

					fwrite(&r, 1, 1, file); 
					fwrite(&g, 1, 1, file); 
					fwrite(&b, 1, 1, file); 
				}
			}
			
			fclose(file); 
		}
#endif
#else
		eglSwapBuffers (dpy, surface);
#endif
	}

term:
	eglDestroyContext (dpy, context);
	eglDestroySurface (dpy, surface);
	eglMakeCurrent (dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglTerminate (dpy);

	INFO("--------------------- finished ---------------------\n");
	return 0;
}


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

	PVRSRVInitializeAppHint(&hint);
	PVRSRVCreateVirtualAppHint(&hint);

	frameStop = 50000;
#endif

	return EglMain(EGL_DEFAULT_DISPLAY, 0);
}

#else /* !defined(SUPPORT_ANDROID_PLATFORM) */

static int			giQuit;
static unsigned int	guiWidth;
static unsigned int	guiHeight;

static int handle_events(void)
{
	if(giQuit)
	{
		giQuit = 0;
		return -1;
	}

	if(guiWidth != 0 && guiHeight != 0)
	{
		glViewport(0, 0, guiWidth, guiHeight);
		guiWidth  = 0;
		guiHeight = 0;
	}

	return 0;
}

void ResizeWindow(unsigned int width, unsigned int height)
{
	guiWidth = width;
	guiHeight = height;
}

void RequestQuit(void)
{
	giQuit = 1;
}

#endif /* !defined(SUPPORT_ANDROID_PLATFORM) */

