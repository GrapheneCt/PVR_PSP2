/******************************************************************************
 * Name         : function_table.c
 *
 * Copyright    : 2007-2008 by Imagination Technologies Limited.
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
 * $Log: function_table.c $
 ********************************************************************************/

#include "egl_internal.h"
#include "function_table.h"
#include "tls.h"


#define EGL_COMPARE_AND_RETURN(func) \
	if(!strcmp(procname, #func))	 \
	{                 				 \
		return IMG_TRUE;		     \
	}

#define EGL_COMPARE_AND_RETURN_OPTION(func, func1) \
	if(!strcmp(procname, #func) || !strcmp(procname, #func1))	\
	{                 				 							\
		return IMG_TRUE;		     							\
	}


#define EGL_COMPARE_AND_RETURN_DUMMY(func)	\
	if(!strcmp(procname, #func))	        \
	{                 				        \
		return (IMG_GL_PROC)&Dummy##func;	\
	}

#define EGL_COMPARE_AND_RETURN_DUMMY_OPTION(string, string1, func)	\
	if(!strcmp(procname, #string) || !strcmp(procname, #string1))   \
	{                 												\
		return (IMG_GL_PROC)&Dummy##func;							\
	}


#if (defined(SUPPORT_OPENGLES1) && defined(SUPPORT_OPENGLES2)) || defined(API_MODULES_RUNTIME_CHECKED)
/************************************
  Dummy function section 
(i.e., extension functions sharing the same name in both GLES1 and GLES2)
*************************************/


/***********************************************************************************
 Function Name      : DummyglMapBufferOES
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static void * IMG_CALLCONV DummyglMapBufferOES(IMG_GLenum target, IMG_GLenum access)
{
	TLS psTls;	

	psTls = IMGEGLGetTLSValue();

	if (psTls==IMG_NULL)
	{
		return IMG_NULL;
	}

	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES1)
	{
		if(!psTls->psGlobalData->pfnMapBufferGLES1)
		{
			psTls->psGlobalData->pfnMapBufferGLES1 = (DUMMY_MAPBUFFER) psTls->psGlobalData->spfnOGLES1.pfnGLESGetProcAddress("glMapBufferOES");

			if(!psTls->psGlobalData->pfnMapBufferGLES1)
			{
				return IMG_NULL;
			}
		}
	   
		return psTls->psGlobalData->pfnMapBufferGLES1(target, access);		
	}
	
	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES2)
	{
		if(!psTls->psGlobalData->pfnMapBufferGLES2)
		{
			psTls->psGlobalData->pfnMapBufferGLES2 = (DUMMY_MAPBUFFER) psTls->psGlobalData->spfnOGLES2.pfnGLESGetProcAddress("glMapBufferOES");

			if(!psTls->psGlobalData->pfnMapBufferGLES2)
			{
				return IMG_NULL;
			}
		}
		
		return psTls->psGlobalData->pfnMapBufferGLES2(target, access);
	}
	
	/* no context or incorrect context for the extension */
	return IMG_NULL;
}


/***********************************************************************************
 Function Name      : DummyglUnmapBufferOES
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static IMG_GLboolean IMG_CALLCONV DummyglUnmapBufferOES(IMG_GLenum target)
{
	TLS psTls;	
	
	psTls = IMGEGLGetTLSValue(); 

	if (psTls==IMG_NULL)
	{
		return IMG_FALSE;
	}

	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES1)
	{
		if(!psTls->psGlobalData->pfnUnmapBufferGLES1)
		{
			psTls->psGlobalData->pfnUnmapBufferGLES1 = (DUMMY_UNMAPBUFFER) psTls->psGlobalData->spfnOGLES1.pfnGLESGetProcAddress("glUnmapBufferOES");

			if(!psTls->psGlobalData->pfnUnmapBufferGLES1)
			{
				return IMG_FALSE;
			}
		}
	   
		return psTls->psGlobalData->pfnUnmapBufferGLES1(target);		
	}
	
	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES2)
	{
		if(!psTls->psGlobalData->pfnUnmapBufferGLES2)
		{
			psTls->psGlobalData->pfnUnmapBufferGLES2 = (DUMMY_UNMAPBUFFER) psTls->psGlobalData->spfnOGLES2.pfnGLESGetProcAddress("glUnmapBufferOES");

			if(!psTls->psGlobalData->pfnUnmapBufferGLES2)
			{
				return IMG_FALSE;
			}
		}
		
		return psTls->psGlobalData->pfnUnmapBufferGLES2(target);
	}
	
	/* no context or incorrect context for the extension */
	return IMG_FALSE;
}


/***********************************************************************************
 Function Name      : DummyglGetBufferPointervOES
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static void IMG_CALLCONV DummyglGetBufferPointervOES(IMG_GLenum target, IMG_GLenum pname, void **params)
{
	TLS psTls;	
	
	psTls = IMGEGLGetTLSValue(); 

	if (psTls==IMG_NULL)
	{
		return;
	}

	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES1)
	{
		if(!psTls->psGlobalData->pfnGetBufferPointervGLES1)
		{
			psTls->psGlobalData->pfnGetBufferPointervGLES1 = (DUMMY_GETBUFFERPOINTERV) psTls->psGlobalData->spfnOGLES1.pfnGLESGetProcAddress("glGetBufferPointervOES");

			if(!psTls->psGlobalData->pfnGetBufferPointervGLES1)
			{
				return;
			}
		}
	   
		psTls->psGlobalData->pfnGetBufferPointervGLES1(target, pname, params);

		return;
	}
	
	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES2)
	{
		if(!psTls->psGlobalData->pfnGetBufferPointervGLES2)
		{
			psTls->psGlobalData->pfnGetBufferPointervGLES2 = (DUMMY_GETBUFFERPOINTERV) psTls->psGlobalData->spfnOGLES2.pfnGLESGetProcAddress("glGetBufferPointervOES");

			if(!psTls->psGlobalData->pfnGetBufferPointervGLES2)
			{
				return;
			}
		}
	   
		psTls->psGlobalData->pfnGetBufferPointervGLES2(target, pname, params);

		return;
	}
	
	/* no context or incorrect context for the extension */
	return;
}


/***********************************************************************************
 Function Name      : DummyglEGLImageTargetTexture2DOES
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static void IMG_CALLCONV DummyglEGLImageTargetTexture2DOES(IMG_GLenum target, IMG_GLeglImageOES image)
{
	TLS psTls;	
	
	psTls = IMGEGLGetTLSValue(); 

	if (psTls==IMG_NULL)
	{
		return;
	}

	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES1)
	{
		if(!psTls->psGlobalData->pfnEGLImageTargetTexture2DGLES1)
		{
			psTls->psGlobalData->pfnEGLImageTargetTexture2DGLES1 = (DUMMY_EGLIMAGETARGETTEXTURE2D) psTls->psGlobalData->spfnOGLES1.pfnGLESGetProcAddress("glEGLImageTargetTexture2DOES");

			if(!psTls->psGlobalData->pfnEGLImageTargetTexture2DGLES1)
			{
				return;
			}
		}
	   
		psTls->psGlobalData->pfnEGLImageTargetTexture2DGLES1(target, image);

		return;
	}
	
	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES2)
	{
		if(!psTls->psGlobalData->pfnEGLImageTargetTexture2DGLES2)
		{
			psTls->psGlobalData->pfnEGLImageTargetTexture2DGLES2 = (DUMMY_EGLIMAGETARGETTEXTURE2D) psTls->psGlobalData->spfnOGLES2.pfnGLESGetProcAddress("glEGLImageTargetTexture2DOES");

			if(!psTls->psGlobalData->pfnEGLImageTargetTexture2DGLES2)
			{
				return;
			}
		}
	   
		psTls->psGlobalData->pfnEGLImageTargetTexture2DGLES2(target, image);

		return;
	}
	
	/* no context or incorrect context for the extension */
	return;
}	


/***********************************************************************************
 Function Name      : DummyglEGLImageTargetRenderbufferStorageOES
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static void IMG_CALLCONV DummyglEGLImageTargetRenderbufferStorageOES(IMG_GLenum target, IMG_GLeglImageOES image)
{
	TLS psTls;	
	
	psTls = IMGEGLGetTLSValue(); 

	if (psTls==IMG_NULL)
	{
		return;
	}

	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES1)
	{
		if(!psTls->psGlobalData->pfnEGLImageTargetRenderbufferStorageGLES1)
		{
			psTls->psGlobalData->pfnEGLImageTargetRenderbufferStorageGLES1 = (DUMMY_EGLIMAGETARGETRENDERBUFFERSTORAGE) psTls->psGlobalData->spfnOGLES1.pfnGLESGetProcAddress("glEGLImageTargetRenderbufferStorageOES");

			if(!psTls->psGlobalData->pfnEGLImageTargetRenderbufferStorageGLES1)
			{
				return;
			}
		}
	   
		psTls->psGlobalData->pfnEGLImageTargetRenderbufferStorageGLES1(target, image);

		return;
	}
	
	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES2)
	{
		if(!psTls->psGlobalData->pfnEGLImageTargetRenderbufferStorageGLES2)
		{
			psTls->psGlobalData->pfnEGLImageTargetRenderbufferStorageGLES2 = (DUMMY_EGLIMAGETARGETRENDERBUFFERSTORAGE) psTls->psGlobalData->spfnOGLES2.pfnGLESGetProcAddress("glEGLImageTargetRenderbufferStorageOES");

			if(!psTls->psGlobalData->pfnEGLImageTargetRenderbufferStorageGLES2)
			{
				return;
			}
		}
	   
		psTls->psGlobalData->pfnEGLImageTargetRenderbufferStorageGLES2(target, image);

		return;
	}
	
	/* no context or incorrect context for the extension */
	return;
}	


/***********************************************************************************
 Function Name      : DummyglMultiDrawArrays
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static void IMG_CALLCONV DummyglMultiDrawArrays(IMG_GLenum mode, IMG_GLint *first, IMG_GLsizei *count, IMG_GLsizei primcount)
{
	TLS psTls;	
	
	psTls = IMGEGLGetTLSValue(); 

	if (psTls==IMG_NULL)
	{
		return;
	}

	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES1)
	{
		if(!psTls->psGlobalData->pfnMultiDrawArraysGLES1)
		{
			psTls->psGlobalData->pfnMultiDrawArraysGLES1 = (DUMMY_MULTIDRAWARRAYS) psTls->psGlobalData->spfnOGLES1.pfnGLESGetProcAddress("glMultiDrawArrays");

			if(!psTls->psGlobalData->pfnMultiDrawArraysGLES1)
			{
				return;
			}
		}
	   
		psTls->psGlobalData->pfnMultiDrawArraysGLES1(mode, first, count, primcount);

		return;
	}
	
	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES2)
	{
		if(!psTls->psGlobalData->pfnMultiDrawArraysGLES2)
		{
			psTls->psGlobalData->pfnMultiDrawArraysGLES2 = (DUMMY_MULTIDRAWARRAYS) psTls->psGlobalData->spfnOGLES2.pfnGLESGetProcAddress("glMultiDrawArrays");

			if(!psTls->psGlobalData->pfnMultiDrawArraysGLES2)
			{
				return;
			}
		}
	   
		psTls->psGlobalData->pfnMultiDrawArraysGLES2(mode, first, count, primcount);

		return;
	}
	
	/* no context or incorrect context for the extension */
	return;
}	


/***********************************************************************************
 Function Name      : DummyglMultiDrawElements
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static void IMG_CALLCONV DummyglMultiDrawElements(IMG_GLenum mode, const IMG_GLsizei *count, IMG_GLenum type, const IMG_GLvoid* *indices, IMG_GLsizei primcount)
{
	TLS psTls;	
	
	psTls = IMGEGLGetTLSValue(); 

	if (psTls==IMG_NULL)
	{
		return;
	}

	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES1)
	{
		if(!psTls->psGlobalData->pfnMultiDrawElementsGLES1)
		{
			psTls->psGlobalData->pfnMultiDrawElementsGLES1 = (DUMMY_MULTIDRAWELEMENTS) psTls->psGlobalData->spfnOGLES1.pfnGLESGetProcAddress("glMultiDrawElements");

			if(!psTls->psGlobalData->pfnMultiDrawElementsGLES1)
			{
				return;
			}
		}
	   
		psTls->psGlobalData->pfnMultiDrawElementsGLES1(mode, count, type, indices, primcount);

		return;
	}
	
	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES2)
	{
		if(!psTls->psGlobalData->pfnMultiDrawElementsGLES2)
		{
			psTls->psGlobalData->pfnMultiDrawElementsGLES2 = (DUMMY_MULTIDRAWELEMENTS) psTls->psGlobalData->spfnOGLES2.pfnGLESGetProcAddress("glMultiDrawElements");

			if(!psTls->psGlobalData->pfnMultiDrawElementsGLES2)
			{
				return;
			}
		}
	   
		psTls->psGlobalData->pfnMultiDrawElementsGLES2(mode, count, type, indices, primcount);

		return;
	}
	
	/* no context or incorrect context for the extension */
	return;
}	

#if defined(GLES1_EXTENSION_TEXTURE_STREAM) && defined(GLES2_EXTENSION_TEXTURE_STREAM)

/***********************************************************************************
 Function Name      : DummyglGetTexStreamDeviceAttributeivIMG
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static void IMG_CALLCONV DummyglGetTexStreamDeviceAttributeivIMG(IMG_GLint device, IMG_GLenum pname, IMG_GLint *params)
{
	TLS psTls;	

	psTls = IMGEGLGetTLSValue();

	if (psTls==IMG_NULL)
	{
		return;
	}

	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES1)
	{
		if(!psTls->psGlobalData->pfnGetTexStreamDeviceAttributeGLES1)
		{
			psTls->psGlobalData->pfnGetTexStreamDeviceAttributeGLES1 = 
				(DUMMY_GETTEXSTREAMDEVICEATTRIBUTE) psTls->psGlobalData->spfnOGLES1.pfnGLESGetProcAddress("glGetTexStreamDeviceAttributeivIMG");

			if(!psTls->psGlobalData->pfnGetTexStreamDeviceAttributeGLES1)
			{
				return;
			}
		}
	   
		psTls->psGlobalData->pfnGetTexStreamDeviceAttributeGLES1(device, pname, params);		
		return;
	}
	
	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES2)
	{
		if(!psTls->psGlobalData->pfnGetTexStreamDeviceAttributeGLES2)
		{
			psTls->psGlobalData->pfnGetTexStreamDeviceAttributeGLES2 = 
				(DUMMY_GETTEXSTREAMDEVICEATTRIBUTE) psTls->psGlobalData->spfnOGLES2.pfnGLESGetProcAddress("glGetTexStreamDeviceAttributeivIMG");

			if(!psTls->psGlobalData->pfnGetTexStreamDeviceAttributeGLES2)
			{
				return;
			}
		}
		
		psTls->psGlobalData->pfnGetTexStreamDeviceAttributeGLES2(device, pname, params);
		return;
	}
	
	/* no context or incorrect context for the extension */
	return;
}

/***********************************************************************************
 Function Name      : DummyglGetTexStreamDeviceNameIMG
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/

static const IMG_GLubyte * IMG_CALLCONV DummyglGetTexStreamDeviceNameIMG(IMG_GLint device)
{
	TLS psTls;	

	psTls = IMGEGLGetTLSValue();

	if (psTls==IMG_NULL)
	{
		return IMG_NULL;
	}

	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES1)
	{
		if(!psTls->psGlobalData->pfnGetTexStreamDeviceNameGLES1)
		{
			psTls->psGlobalData->pfnGetTexStreamDeviceNameGLES1 = 
				(DUMMY_GETTEXSTREAMDEVICENAME) psTls->psGlobalData->spfnOGLES1.pfnGLESGetProcAddress("glGetTexStreamDeviceNameIMG");

			if(!psTls->psGlobalData->pfnGetTexStreamDeviceNameGLES1)
			{
				return IMG_NULL;
			}
		}
	   
		return psTls->psGlobalData->pfnGetTexStreamDeviceNameGLES1(device);		
	}
	
	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES2)
	{
		if(!psTls->psGlobalData->pfnGetTexStreamDeviceNameGLES2)
		{
			psTls->psGlobalData->pfnGetTexStreamDeviceNameGLES2 = 
				(DUMMY_GETTEXSTREAMDEVICENAME) psTls->psGlobalData->spfnOGLES2.pfnGLESGetProcAddress("glGetTexStreamDeviceNameIMG");

			if(!psTls->psGlobalData->pfnGetTexStreamDeviceNameGLES2)
			{
				return IMG_NULL;
			}
		}
		
		return psTls->psGlobalData->pfnGetTexStreamDeviceNameGLES2(device);
	}
	
	/* no context or incorrect context for the extension */
	return IMG_NULL;
}

/***********************************************************************************
 Function Name      : DummyglTexBindStreamIMG
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static void IMG_CALLCONV DummyglTexBindStreamIMG(IMG_GLint device, IMG_GLint deviceoffset)
{
	TLS psTls;	

	psTls = IMGEGLGetTLSValue();

	if (psTls==IMG_NULL)
	{
		return;
	}

	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES1)
	{
		if(!psTls->psGlobalData->pfnTexBindStreamGLES1)
		{
			psTls->psGlobalData->pfnTexBindStreamGLES1 = 
				(DUMMY_TEXBINDSTREAM) psTls->psGlobalData->spfnOGLES1.pfnGLESGetProcAddress("glTexBindStreamIMG");

			if(!psTls->psGlobalData->pfnTexBindStreamGLES1)
			{
				return;
			}
		}
	   
		psTls->psGlobalData->pfnTexBindStreamGLES1(device, deviceoffset);		
		return;
	}
	
	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES2)
	{
		if(!psTls->psGlobalData->pfnTexBindStreamGLES2)
		{
			psTls->psGlobalData->pfnTexBindStreamGLES2 = 
				(DUMMY_TEXBINDSTREAM) psTls->psGlobalData->spfnOGLES2.pfnGLESGetProcAddress("glTexBindStreamIMG");

			if(!psTls->psGlobalData->pfnTexBindStreamGLES2)
			{
				return;
			}
		}
		
		psTls->psGlobalData->pfnTexBindStreamGLES2(device, deviceoffset);
		return;
	}
	
	/* no context or incorrect context for the extension */
	return;
}

#endif /* defined(GLES1_EXTENSION_TEXTURE_STREAM) && defined(GLES2_EXTENSION_TEXTURE_STREAM) */



/***********************************************************************************
 Function Name      : DummyglBindVertexArrayOES
 Inputs             : vertexarray
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static void IMG_CALLCONV DummyglBindVertexArrayOES(IMG_GLuint vertexarray)
{
	TLS psTls;	
	
	psTls = IMGEGLGetTLSValue(); 

	if (psTls==IMG_NULL)
	{
		return;
	}

	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES1)
	{
		if(!psTls->psGlobalData->pfnBindVertexArrayGLES1)
		{
			psTls->psGlobalData->pfnBindVertexArrayGLES1 = (DUMMY_BINDVERTEXARRAY) psTls->psGlobalData->spfnOGLES1.pfnGLESGetProcAddress("glBindVertexArrayOES");

			if(!psTls->psGlobalData->pfnBindVertexArrayGLES1)
			{
				return;
			}
		}
	   
		psTls->psGlobalData->pfnBindVertexArrayGLES1(vertexarray);

		return;
	}
	
	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES2)
	{
		if(!psTls->psGlobalData->pfnBindVertexArrayGLES2)
		{
			psTls->psGlobalData->pfnBindVertexArrayGLES2 = (DUMMY_BINDVERTEXARRAY) psTls->psGlobalData->spfnOGLES2.pfnGLESGetProcAddress("glBindVertexArrayOES");

			if(!psTls->psGlobalData->pfnBindVertexArrayGLES2)
			{
				return;
			}
		}
	   
		psTls->psGlobalData->pfnBindVertexArrayGLES2(vertexarray);

		return;
	}

	/* no context or incorrect context for the extension */
	return;
}	


/***********************************************************************************
 Function Name      : DummyglDeleteVertexArraysOES
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static void IMG_CALLCONV DummyglDeleteVertexArraysOES(IMG_GLsizei n, const IMG_GLuint *vertexarrays)
{
	TLS psTls;	
	
	psTls = IMGEGLGetTLSValue(); 

	if (psTls==IMG_NULL)
	{
		return;
	}

	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES1)
	{
		if(!psTls->psGlobalData->pfnDeleteVertexArrayGLES1)
		{
			psTls->psGlobalData->pfnDeleteVertexArrayGLES1 = (DUMMY_DELETEVERTEXARRAY) psTls->psGlobalData->spfnOGLES1.pfnGLESGetProcAddress("glDeleteVertexArraysOES");

			if(!psTls->psGlobalData->pfnDeleteVertexArrayGLES1)
			{
				return;
			}
		}
	   
		psTls->psGlobalData->pfnDeleteVertexArrayGLES1(n, vertexarrays);

		return;
	}

	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES2)
	{
		if(!psTls->psGlobalData->pfnDeleteVertexArrayGLES2)
		{
			psTls->psGlobalData->pfnDeleteVertexArrayGLES2 = (DUMMY_DELETEVERTEXARRAY) psTls->psGlobalData->spfnOGLES2.pfnGLESGetProcAddress("glDeleteVertexArraysOES");

			if(!psTls->psGlobalData->pfnDeleteVertexArrayGLES2)
			{
				return;
			}
		}
	   
		psTls->psGlobalData->pfnDeleteVertexArrayGLES2(n, vertexarrays);

		return;
	}

	/* no context or incorrect context for the extension */
	return;
}	


/***********************************************************************************
 Function Name      : DummyglGenVertexArraysOES
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static void IMG_CALLCONV DummyglGenVertexArraysOES(IMG_GLsizei n, IMG_GLuint *vertexarrays)
{
	TLS psTls;	
	
	psTls = IMGEGLGetTLSValue(); 

	if (psTls==IMG_NULL)
	{
		return;
	}

	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES1)
	{
		if(!psTls->psGlobalData->pfnGenVertexArrayGLES1)
		{
			psTls->psGlobalData->pfnGenVertexArrayGLES1 = (DUMMY_GENVERTEXARRAY) psTls->psGlobalData->spfnOGLES1.pfnGLESGetProcAddress("glGenVertexArraysOES");

			if(!psTls->psGlobalData->pfnGenVertexArrayGLES1)
			{
				return;
			}
		}
	   
		psTls->psGlobalData->pfnGenVertexArrayGLES1(n, vertexarrays);

		return;
	}

	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES2)
	{
		if(!psTls->psGlobalData->pfnGenVertexArrayGLES2)
		{
			psTls->psGlobalData->pfnGenVertexArrayGLES2 = (DUMMY_GENVERTEXARRAY) psTls->psGlobalData->spfnOGLES2.pfnGLESGetProcAddress("glGenVertexArraysOES");

			if(!psTls->psGlobalData->pfnGenVertexArrayGLES2)
			{
				return;
			}
		}
	   
		psTls->psGlobalData->pfnGenVertexArrayGLES2(n, vertexarrays);

		return;
	}

	/* no context or incorrect context for the extension */
	return;
}	


/***********************************************************************************
 Function Name      : DummyglIsVertexArrayOES
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static IMG_GLboolean IMG_CALLCONV DummyglIsVertexArrayOES(IMG_GLuint vertexarray)
{
	TLS psTls;	
	
	psTls = IMGEGLGetTLSValue(); 

	if (psTls==IMG_NULL)
	{
		return IMG_FALSE;
	}

	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES1)
	{
		if(!psTls->psGlobalData->pfnIsVertexArrayGLES1)
		{
			psTls->psGlobalData->pfnIsVertexArrayGLES1 = (DUMMY_ISVERTEXARRAY) psTls->psGlobalData->spfnOGLES1.pfnGLESGetProcAddress("glIsVertexArrayOES");

			if(!psTls->psGlobalData->pfnIsVertexArrayGLES1)
			{
				return IMG_FALSE;
			}
		}
	   
		return psTls->psGlobalData->pfnIsVertexArrayGLES1(vertexarray);
	}

	if((psTls->apsCurrentContext[psTls->ui32API])->eContextType  == IMGEGL_CONTEXT_OPENGLES2)
	{
		if(!psTls->psGlobalData->pfnIsVertexArrayGLES2)
		{
			psTls->psGlobalData->pfnIsVertexArrayGLES2 = (DUMMY_ISVERTEXARRAY) psTls->psGlobalData->spfnOGLES2.pfnGLESGetProcAddress("glIsVertexArrayOES");

			if(!psTls->psGlobalData->pfnIsVertexArrayGLES2)
			{
				return IMG_FALSE;
			}
		}
	   
		return psTls->psGlobalData->pfnIsVertexArrayGLES2(vertexarray);
	}
	
	/* no context or incorrect context for the extension */
	return IMG_FALSE;
}



/************************************
  End of dummy function section
*************************************/
#endif /* (defined(SUPPORT_OPENGLES1) && defined(SUPPORT_OPENGLES2)) || defined(API_MODULES_RUNTIME_CHECKED) */



#if defined(SUPPORT_OPENGLES1) || defined(API_MODULES_RUNTIME_CHECKED)
/***********************************************************************************
 Function Name      : IMGSearchGLES1Extension
 Inputs             : procname
 Outputs            : -
 Returns            : boolean
************************************************************************************/
static IMG_BOOL IMGSearchGLES1Extension(const char *procname)
{

/***********************************************************
					Required extensions
 ***********************************************************/

	EGL_COMPARE_AND_RETURN(glPointSizePointerOES)


/***********************************************************
					Optional extensions
 ***********************************************************/

#if defined(GLES1_EXTENSION_MATRIX_PALETTE)

	EGL_COMPARE_AND_RETURN(glCurrentPaletteMatrixOES)
	EGL_COMPARE_AND_RETURN(glLoadPaletteFromModelViewMatrixOES)
	EGL_COMPARE_AND_RETURN(glMatrixIndexPointerOES)
	EGL_COMPARE_AND_RETURN(glWeightPointerOES)

#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */


#if defined(GLES1_EXTENSION_DRAW_TEXTURE)

	EGL_COMPARE_AND_RETURN(glDrawTexsOES)
	EGL_COMPARE_AND_RETURN(glDrawTexiOES)

#if defined(PROFILE_COMMON)

	EGL_COMPARE_AND_RETURN(glDrawTexfOES)

#endif /* PROFILE_COMMON */

	EGL_COMPARE_AND_RETURN(glDrawTexxOES)
	EGL_COMPARE_AND_RETURN(glDrawTexsvOES)
	EGL_COMPARE_AND_RETURN(glDrawTexivOES)

#if defined(PROFILE_COMMON)

	EGL_COMPARE_AND_RETURN(glDrawTexfvOES)

#endif /* PROFILE_COMMON */

	EGL_COMPARE_AND_RETURN(glDrawTexxvOES)

#endif /* defined(GLES1_EXTENSION_DRAW_TEXTURE) */


#if defined(GLES1_EXTENSION_QUERY_MATRIX)

	EGL_COMPARE_AND_RETURN(glQueryMatrixxOES)

#endif /* defined(GLES1_EXTENSION_QUERY_MATRIX) */


/***********************************************************
					Extension pack
 ***********************************************************/

#if defined(GLES1_EXTENSION_BLEND_SUBTRACT)

	EGL_COMPARE_AND_RETURN(glBlendEquationOES)

#endif /* defined(GLES1_EXTENSION_BLEND_SUBTRACT) */


#if defined(GLES1_EXTENSION_BLEND_EQ_SEPARATE)

	EGL_COMPARE_AND_RETURN(glBlendEquationSeparateOES)

#endif /* defined(GLES1_EXTENSION_BLEND_EQ_SEPARATE) */


#if defined(GLES1_EXTENSION_BLEND_FUNC_SEPARATE)

	EGL_COMPARE_AND_RETURN(glBlendFuncSeparateOES)

#endif /* defined(GLES1_EXTENSION_BLEND_FUNC_SEPARATE) */


#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)

	EGL_COMPARE_AND_RETURN(glTexGeniOES)
	EGL_COMPARE_AND_RETURN(glTexGenivOES)

#if defined(PROFILE_COMMON)
	EGL_COMPARE_AND_RETURN(glTexGenfOES)
	EGL_COMPARE_AND_RETURN(glTexGenfvOES)
#endif /* PROFILE_COMMON */

	EGL_COMPARE_AND_RETURN(glTexGenxOES)
	EGL_COMPARE_AND_RETURN(glTexGenxvOES)
	EGL_COMPARE_AND_RETURN(glGetTexGenivOES)

#if defined(PROFILE_COMMON)
	EGL_COMPARE_AND_RETURN(glGetTexGenfvOES)
#endif /* PROFILE_COMMON */

	EGL_COMPARE_AND_RETURN(glGetTexGenxvOES)

#else /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */


#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */

#if defined(GLES1_EXTENSION_FRAME_BUFFER_OBJECTS)

	EGL_COMPARE_AND_RETURN(glIsRenderbufferOES)
	EGL_COMPARE_AND_RETURN(glBindRenderbufferOES)
	EGL_COMPARE_AND_RETURN(glDeleteRenderbuffersOES)
	EGL_COMPARE_AND_RETURN(glGenRenderbuffersOES)
	EGL_COMPARE_AND_RETURN(glRenderbufferStorageOES)
	EGL_COMPARE_AND_RETURN(glGetRenderbufferParameterivOES)
	EGL_COMPARE_AND_RETURN(glIsFramebufferOES)
	EGL_COMPARE_AND_RETURN(glBindFramebufferOES)
	EGL_COMPARE_AND_RETURN(glDeleteFramebuffersOES)
	EGL_COMPARE_AND_RETURN(glGenFramebuffersOES)
	EGL_COMPARE_AND_RETURN(glCheckFramebufferStatusOES)
	EGL_COMPARE_AND_RETURN(glFramebufferTexture2DOES)
	EGL_COMPARE_AND_RETURN(glFramebufferRenderbufferOES)
	EGL_COMPARE_AND_RETURN(glGetFramebufferAttachmentParameterivOES)
	EGL_COMPARE_AND_RETURN(glGenerateMipmapOES)

#endif /* defined(GLES1_EXTENSION_FRAME_BUFFER_OBJECTS) */


/***********************************************************
					IMG extensions
 ***********************************************************/

#if defined(GLES1_EXTENSION_TEXTURE_STREAM)

	EGL_COMPARE_AND_RETURN(glGetTexStreamDeviceAttributeivIMG)
	EGL_COMPARE_AND_RETURN(glGetTexStreamDeviceNameIMG)
	EGL_COMPARE_AND_RETURN(glTexBindStreamIMG)

#endif /* defined(GLES1_EXTENSION_TEXTURE_STREAM) */

#if defined(GLES1_EXTENSION_MAP_BUFFER)	

	EGL_COMPARE_AND_RETURN(glGetBufferPointervOES)
	EGL_COMPARE_AND_RETURN(glMapBufferOES)
	EGL_COMPARE_AND_RETURN(glUnmapBufferOES)

#endif /* defined(GLES1_EXTENSION_MAP_BUFFER) */

#if defined(GLES1_EXTENSION_MULTI_DRAW_ARRAYS)

	EGL_COMPARE_AND_RETURN_OPTION(glMultiDrawArrays, glMultiDrawArraysEXT)	
	EGL_COMPARE_AND_RETURN_OPTION(glMultiDrawElements, glMultiDrawElementsEXT)	

#endif 

#if defined(GLES1_EXTENSION_EGL_IMAGE)
	EGL_COMPARE_AND_RETURN(glEGLImageTargetTexture2DOES)
	EGL_COMPARE_AND_RETURN(glEGLImageTargetRenderbufferStorageOES)
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */


#if defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT)
	EGL_COMPARE_AND_RETURN(glBindVertexArrayOES)  
	EGL_COMPARE_AND_RETURN(glDeleteVertexArraysOES)  
	EGL_COMPARE_AND_RETURN(glGenVertexArraysOES)  
	EGL_COMPARE_AND_RETURN(glIsVertexArrayOES)  
#endif /* defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT) */	

	return IMG_FALSE;
}
#endif /* defined(SUPPORT_OPENGLES1) || defined(API_MODULES_RUNTIME_CHECKED) */


#if defined(SUPPORT_OPENGLES2) || defined(API_MODULES_RUNTIME_CHECKED)
/***********************************************************************************
 Function Name      : GLES2GetProcAddress
 Inputs             : procname
 Outputs            : -
 Returns            : boolean
************************************************************************************/
static IMG_BOOL IMGSearchGLES2Extension(const char *procname)
{

	EGL_COMPARE_AND_RETURN(glMapBufferOES)
	EGL_COMPARE_AND_RETURN(glUnmapBufferOES)
	EGL_COMPARE_AND_RETURN(glGetBufferPointervOES)

#if defined(GLES2_EXTENSION_EGL_IMAGE)

	EGL_COMPARE_AND_RETURN(glEGLImageTargetTexture2DOES)
	EGL_COMPARE_AND_RETURN(glEGLImageTargetRenderbufferStorageOES)

#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */

#if defined(GLES2_EXTENSION_MULTI_DRAW_ARRAYS)

	EGL_COMPARE_AND_RETURN_OPTION(glMultiDrawArrays, glMultiDrawArraysEXT)	
	EGL_COMPARE_AND_RETURN_OPTION(glMultiDrawElements, glMultiDrawElementsEXT)	

#endif /* defined(GLES2_EXTENSION_MULTI_DRAW_ARRAYS) */

#if defined(GLES2_EXTENSION_TEXTURE_STREAM)

	EGL_COMPARE_AND_RETURN(glGetTexStreamDeviceAttributeivIMG)
	EGL_COMPARE_AND_RETURN(glGetTexStreamDeviceNameIMG)
	EGL_COMPARE_AND_RETURN(glTexBindStreamIMG)

#endif /* #if defined(GLES2_EXTENSION_TEXTURE_STREAM) */

#if defined(GLES2_EXTENSION_GET_PROGRAM_BINARY)
	EGL_COMPARE_AND_RETURN(glGetProgramBinaryOES)
	EGL_COMPARE_AND_RETURN(glProgramBinaryOES)
#endif /* defined(GLES2_EXTENSION_GET_PROGRAM_BINARY) */

#if defined(GLES2_EXTENSION_VERTEX_ARRAY_OBJECT)
	EGL_COMPARE_AND_RETURN(glBindVertexArrayOES)  
	EGL_COMPARE_AND_RETURN(glDeleteVertexArraysOES)  
	EGL_COMPARE_AND_RETURN(glGenVertexArraysOES)  
	EGL_COMPARE_AND_RETURN(glIsVertexArrayOES)  
#endif /* defined(GLES2_EXTENSION_VERTEX_ARRAY_OBJECT) */	

#if defined(GLES2_EXTENSION_DISCARD_FRAMEBUFFER)
	EGL_COMPARE_AND_RETURN(glDiscardFramebufferEXT)
#endif /* defined(GLES2_EXTENSION_DISCARD_FRAMEBUFFER) */

#if defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
	EGL_COMPARE_AND_RETURN(glRenderbufferStorageMultisampleIMG)
	EGL_COMPARE_AND_RETURN(glFramebufferTexture2DMultisampleIMG)
#endif /* defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE) */

	return IMG_FALSE;
}	
#endif /* defined(SUPPORT_OPENGLES2) || defined(API_MODULES_RUNTIME_CHECKED) */



#if defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX) || defined(API_MODULES_RUNTIME_CHECKED)
/***********************************************************************************
 Function Name      : IMGSearchOVGExtension
 Inputs             : procname
 Outputs            : -
 Returns            : boolean
************************************************************************************/
static IMG_BOOL IMGSearchOVGExtension(const char *procname)
{
	EGL_COMPARE_AND_RETURN(vgCreateEGLImageTargetKHR)
	EGL_COMPARE_AND_RETURN(vgValidateVGX)

	return IMG_FALSE;
}	
#endif /* defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX) || defined(API_MODULES_RUNTIME_CHECKED) */
 

#if (defined(SUPPORT_OPENGLES1) && defined(SUPPORT_OPENGLES2)) || defined(API_MODULES_RUNTIME_CHECKED)
/***********************************************************************
 *
 *  FUNCTION   : IMGGetDummyFunction
 *  PURPOSE    : Retrieve dummy functions.
 *  PARAMETERS : In:  procname - Extension function name.
 *  RETURNS    : Extension function or IMG_NULL.
 *
 **********************************************************************/
IMG_INTERNAL IMG_GL_PROC IMGGetDummyFunction(const char *procname)
{
	/*
	  We use dummy functions only if there is a GLES1/GLES2 name clash
	*/
	EGL_COMPARE_AND_RETURN_DUMMY(glMapBufferOES)
	EGL_COMPARE_AND_RETURN_DUMMY(glUnmapBufferOES)
	EGL_COMPARE_AND_RETURN_DUMMY(glGetBufferPointervOES)

	EGL_COMPARE_AND_RETURN_DUMMY(glEGLImageTargetTexture2DOES)
	EGL_COMPARE_AND_RETURN_DUMMY(glEGLImageTargetRenderbufferStorageOES)

#if defined(GLES1_EXTENSION_TEXTURE_STREAM) && defined(GLES2_EXTENSION_TEXTURE_STREAM)
	EGL_COMPARE_AND_RETURN_DUMMY(glGetTexStreamDeviceAttributeivIMG)
	EGL_COMPARE_AND_RETURN_DUMMY(glGetTexStreamDeviceNameIMG)
	EGL_COMPARE_AND_RETURN_DUMMY(glTexBindStreamIMG)
#endif

	EGL_COMPARE_AND_RETURN_DUMMY_OPTION(glMultiDrawArrays, glMultiDrawArraysEXT, glMultiDrawArrays)
	EGL_COMPARE_AND_RETURN_DUMMY_OPTION(glMultiDrawElements, glMultiDrawElementsEXT, glMultiDrawElements)

#if defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT) && defined(GLES2_EXTENSION_VERTEX_ARRAY_OBJECT)
	EGL_COMPARE_AND_RETURN_DUMMY(glBindVertexArrayOES)
	EGL_COMPARE_AND_RETURN_DUMMY(glDeleteVertexArraysOES)
	EGL_COMPARE_AND_RETURN_DUMMY(glGenVertexArraysOES)
	EGL_COMPARE_AND_RETURN_DUMMY(glIsVertexArrayOES)
#endif

	return IMG_NULL;
}
#endif /* (defined(SUPPORT_OPENGLES1) && defined(SUPPORT_OPENGLES2)) || defined(API_MODULES_RUNTIME_CHECKED) */

/***********************************************************************************
 Function Name      : IMGFindLibraryToLoad
 Inputs             : procname
 Outputs            : -
 Returns            : boolean
 Description        : Returns the library name of the named function
************************************************************************************/
IMG_INTERNAL IMG_UINT32 IMGFindLibraryToLoad(const char *procname)
{
#if defined(API_MODULES_RUNTIME_CHECKED)
	EGLGlobal *psGlobalData = ENV_GetGlobalData();

	IMG_BOOL bIsGLES1 = IMG_FALSE;
	IMG_BOOL bIsGLES2 = IMG_FALSE;
	IMG_BOOL bIsOVG   = IMG_FALSE;

	PVR_ASSERT(psGlobalData!=IMG_NULL)

	if(psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENGLES1])
	{
		bIsGLES1 = IMGSearchGLES1Extension(procname);
	}
	if(psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENGLES2])
	{
		bIsGLES2 = IMGSearchGLES2Extension(procname);
	}
	if(psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENVG])
	{
		bIsOVG = IMGSearchOVGExtension(procname);
	}

	if(bIsGLES1 && bIsGLES2)
	{
		return IMGEGL_NAME_CLASH;
	}

	if(bIsGLES1)
	{
		return IMGEGL_LOAD_GLES1;
	}
	if(bIsGLES2)
	{
		return IMGEGL_LOAD_GLES2;
	}
	if(bIsOVG)
	{
		return IMGEGL_LOAD_OPENVG;
	}
	return IMGEGL_NO_LIBRARY;

#else /* defined(API_MODULES_RUNTIME_CHECKED) */

#if defined(SUPPORT_OPENGLES1)
	IMG_BOOL bIsGLES1;
#endif

#if defined(SUPPORT_OPENGLES2)
	IMG_BOOL bIsGLES2;
#endif

#if defined(SUPPORT_OPENVG)
	IMG_BOOL bIsOVG;
#endif
	
#if !(defined(SUPPORT_OPENGLES1) || defined(SUPPORT_OPENGLES2) \
	 || defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX))
PVR_UNREFERENCED_PARAMETER(procname);
#endif


#if defined(SUPPORT_OPENGLES1)
	bIsGLES1 = IMG_FALSE;
	bIsGLES1 = IMGSearchGLES1Extension(procname);
#endif

#if defined(SUPPORT_OPENGLES2)
	bIsGLES2 = IMG_FALSE;
	bIsGLES2 = IMGSearchGLES2Extension(procname);
#endif

#if defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX)
	bIsOVG = IMGSearchOVGExtension(procname);
#endif

#if (defined(SUPPORT_OPENGLES1) && defined(SUPPORT_OPENGLES2))
	/*
	  Check for a name clash; if there is one, use a dummy function
	 */
	if(bIsGLES1 && bIsGLES2)
	{
		return IMGEGL_NAME_CLASH;
	}
#endif

#if defined(SUPPORT_OPENGLES1)
	if(bIsGLES1)
	{
		return IMGEGL_LOAD_GLES1;
	}
#endif

#if defined(SUPPORT_OPENGLES2)
	if(bIsGLES2)
	{
		return IMGEGL_LOAD_GLES2;
	}
#endif

#if defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX)
	if(bIsOVG)
	{
		return IMGEGL_LOAD_OPENVG;
	}
#endif
	return IMGEGL_NO_LIBRARY;
#endif /* defined(API_MODULES_RUNTIME_CHECKED) */
}

/******************************************************************************
 End of file (function_table.c)
******************************************************************************/
