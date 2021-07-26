/******************************************************************************
 * Name         : cfg.c
 * Title        : EGL Config Support
 * Author       : Marcus Shawcroft
 * Created      : 4 Nov 2003
 *
 * Copyright    : 2003-2008 by Imagination Technologies Limited.
 *                All rights reserved. No part of this software, either
 *                material or conceptual may be copied or distributed,
 *                transmitted, transcribed, stored in a retrieval system or
 *                translated into any human or computer language in any form
 *                by any means, electronic, mechanical, manual or otherwise,
 *                or disclosed to third parties without the express written
 *                permission of Imagination Technologies Limited, Home
 *                Park Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * Description  : Generation and manipulation of EGL configurations.
 *
 * Platform : ALL
 *
 * $Date: 2011/07/20 15:20:31 $ $Revision: 1.66 $
 * $Log: cfg.c $
 * ./
 *  --- Revision Logs Removed --- 
 *
 *  --- Revision Logs Removed --- 
 *************************************************************************/

#include "egl_internal.h"
#include "drvegl.h"
#include "drveglext.h"
#include "srv.h"
#include "cfg.h"
#include "tls.h"


/* Number of configuration variants supported by the generic EGL */
#if defined(API_MODULES_RUNTIME_CHECKED)

		#define BASE_ES1_VG_CONFIG_VARIANTS 4

	#define PBUFFER_ES1_VG_CONFIG_VARIANTS  4

	#define ES1_VG_CONFORMANT       EGL_OPENGL_ES_BIT|EGL_OPENVG_BIT
	#define ES1_VG_RENDERABLE_TYPE  EGL_OPENGL_ES_BIT|EGL_OPENVG_BIT

#else

#   if defined(SUPPORT_OPENGLES1)

			#define BASE_ES1_VG_CONFIG_VARIANTS 4

		#define PBUFFER_ES1_VG_CONFIG_VARIANTS  4

		#if defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX)

			#define ES1_VG_CONFORMANT       EGL_OPENGL_ES_BIT|EGL_OPENVG_BIT
			#define ES1_VG_RENDERABLE_TYPE  EGL_OPENGL_ES_BIT|EGL_OPENVG_BIT

		#else /* defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX)*/

			#define ES1_VG_CONFORMANT       EGL_OPENGL_ES_BIT
			#define ES1_VG_RENDERABLE_TYPE  EGL_OPENGL_ES_BIT

		#endif /* defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX)*/

	#else /* defined(SUPPORT_OPENGLES1) */

		#if defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX)

			#define BASE_ES1_VG_CONFIG_VARIANTS     1
			#define PBUFFER_ES1_VG_CONFIG_VARIANTS  4

			#define ES1_VG_CONFORMANT       EGL_OPENVG_BIT
			#define ES1_VG_RENDERABLE_TYPE  EGL_OPENVG_BIT

		#endif /* defined(SUPPORT_OPENVG)  || defined(SUPPORT_OPENVGX)*/

	#endif /* defined(SUPPORT_OPENGLES1) */

#endif /* defined(API_MODULES_RUNTIME_CHECKED) */


/*  Configuration variation attributes  */ 
#if defined(SUPPORT_OPENGLES1) || defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX) || defined(API_MODULES_RUNTIME_CHECKED)

static const EGLint _ES1VGNoDepthStencilVariant[] = 
{ 
	EGL_DEPTH_SIZE,             0,
	EGL_STENCIL_SIZE,           0,

	EGL_NONE
};

static const EGLint _ES1VGMultiSampleVariant[] = 
{ 
	EGL_SAMPLE_BUFFERS,         1,
	EGL_SAMPLES,                4,

	EGL_NONE
};

static const EGLint _ES1VG16BitDepthVariant[] =
{
        EGL_DEPTH_SIZE,             16,
        EGL_STENCIL_SIZE,           0,

        EGL_NONE
};


static const EGLint _ES1VGPBufferVariant0[] = 
{ 
	EGL_BUFFER_SIZE,            32,
	EGL_ALPHA_SIZE,             8,
	EGL_RED_SIZE,               8,
	EGL_GREEN_SIZE,             8,
	EGL_BLUE_SIZE,              8,
	EGL_BIND_TO_TEXTURE_RGBA,   1,
	EGL_CONFORMANT,             ES1_VG_CONFORMANT,
	EGL_RENDERABLE_TYPE,        ES1_VG_RENDERABLE_TYPE,

	EGL_NONE
};

static const EGLint _ES1VGPBufferVariant1[] = 
{ 
	EGL_CONFORMANT,             ES1_VG_CONFORMANT,
	EGL_RENDERABLE_TYPE,        ES1_VG_RENDERABLE_TYPE,

	EGL_NONE
};

static const EGLint _ES1VGPBufferVariant2[] = 
{ 
	EGL_BUFFER_SIZE,            16,
	EGL_ALPHA_SIZE,             4,
	EGL_RED_SIZE,               4,
	EGL_GREEN_SIZE,             4,
	EGL_BLUE_SIZE,              4,
	EGL_BIND_TO_TEXTURE_RGBA,   1,
	EGL_CONFORMANT,             ES1_VG_CONFORMANT,
	EGL_RENDERABLE_TYPE,        ES1_VG_RENDERABLE_TYPE,

	EGL_NONE
};

static const EGLint _ES1VGPBufferVariant3[] = 
{ 
	EGL_BUFFER_SIZE,            16,
	EGL_ALPHA_SIZE,             1,
	EGL_RED_SIZE,               5,
	EGL_GREEN_SIZE,             5,
	EGL_BLUE_SIZE,              5,
	EGL_BIND_TO_TEXTURE_RGBA,   1,
	EGL_CONFORMANT,             ES1_VG_CONFORMANT,
	EGL_RENDERABLE_TYPE,        ES1_VG_RENDERABLE_TYPE,

	EGL_NONE
};

/* 
 * This array defines the base configuration attribute values for ES and/or VG. The
 * base configurations is passed to the WS which creates delta configurations from the base. 
 */
static const EGLint aBaseES1VGAttribs[] =
{
	EGL_BUFFER_SIZE,                0,      /* ws must define */
	EGL_ALPHA_SIZE,                 0,      /* ws must define */
	EGL_RED_SIZE,                   0,      /* ws must define */
	EGL_GREEN_SIZE,                 0,      /* ws must define */
	EGL_BLUE_SIZE,                  0,      /* ws must define */
	EGL_DEPTH_SIZE,                 0,
	EGL_STENCIL_SIZE,               0,
	EGL_LUMINANCE_SIZE,             0,
	EGL_ALPHA_MASK_SIZE,            0,
	EGL_CONFIG_CAVEAT,              EGL_NONE,
	EGL_CONFIG_ID,                  0,
	EGL_LEVEL,                      0,
#if(EURASIA_TEXTURESIZE_MAX > 2048)
	EGL_MAX_PBUFFER_WIDTH,          4096,
	EGL_MAX_PBUFFER_HEIGHT,         4096,
	EGL_MAX_PBUFFER_PIXELS,         4096*4096,
#else
	EGL_MAX_PBUFFER_WIDTH,          2048,
	EGL_MAX_PBUFFER_HEIGHT,         2048,
	EGL_MAX_PBUFFER_PIXELS,         2048*2048,
#endif
	EGL_NATIVE_RENDERABLE,          EGL_FALSE,
	EGL_NATIVE_VISUAL_ID,           0,
	EGL_NATIVE_VISUAL_TYPE,         EGL_NONE,
	EGL_SAMPLES,                    0,
	EGL_SAMPLE_BUFFERS,             0,
	EGL_SURFACE_TYPE,               EGL_PBUFFER_BIT,
	EGL_TRANSPARENT_TYPE,           EGL_NONE,
	EGL_TRANSPARENT_RED_VALUE,      0,
	EGL_TRANSPARENT_GREEN_VALUE,    0,
	EGL_TRANSPARENT_BLUE_VALUE,     0,
	EGL_MIN_SWAP_INTERVAL,          1,      /* ws must define */
	EGL_MAX_SWAP_INTERVAL,          1,      /* ws must define */
	EGL_COLOR_BUFFER_TYPE,          EGL_RGB_BUFFER,
	EGL_MATCH_NATIVE_PIXMAP,        EGL_NONE,
#if defined(EGL_EXTENSION_ANDROID_RECORDABLE)
	EGL_RECORDABLE_ANDROID,			EGL_TRUE,
#endif /* defined(EGL_EXTENSION_ANDROID_RECORDABLE) */
	EGL_NONE
};

static const EGLint aBaseES1VGAttribs_OpenGLES1[] =
{
	EGL_DEPTH_SIZE,                 24,
	EGL_STENCIL_SIZE,               8,
	EGL_BIND_TO_TEXTURE_RGB,        0,      /* ws must define */
	EGL_BIND_TO_TEXTURE_RGBA,       0,      /* ws must define */
	EGL_CONFORMANT,                 EGL_OPENGL_ES_BIT,
	EGL_RENDERABLE_TYPE,            EGL_OPENGL_ES_BIT,

	EGL_NONE
};

static const EGLint aBaseES1VGAttribs_OpenVG[] =
{
	EGL_ALPHA_MASK_SIZE,            8,
	EGL_SURFACE_TYPE,               EGL_PBUFFER_BIT | EGL_VG_COLORSPACE_LINEAR_BIT | EGL_VG_ALPHA_FORMAT_PRE_BIT,
	EGL_CONFORMANT,                 EGL_OPENVG_BIT,
	EGL_RENDERABLE_TYPE,            EGL_OPENVG_BIT,

	EGL_NONE
};

/* This array defines the base configuration attribute values for
 * pbuffers only configs for ES. 
 */

static const EGLint asPBufferBaseES1VGAttribs[] =
{
	EGL_BUFFER_SIZE,                16,
	EGL_ALPHA_SIZE,                 0,
	EGL_RED_SIZE,                   5,
	EGL_GREEN_SIZE,                 6,
	EGL_BLUE_SIZE,                  5,
	EGL_DEPTH_SIZE,                 0,
	EGL_STENCIL_SIZE,               0,
	EGL_LUMINANCE_SIZE,             0,
	EGL_ALPHA_MASK_SIZE,            0,
	EGL_CONFIG_CAVEAT,              EGL_NONE,
	EGL_CONFIG_ID,                  0,
	EGL_LEVEL,                      0,
#if(EURASIA_TEXTURESIZE_MAX > 2048)
	EGL_MAX_PBUFFER_WIDTH,          4096,
	EGL_MAX_PBUFFER_HEIGHT,         4096,
	EGL_MAX_PBUFFER_PIXELS,         4096*4096,
#else
	EGL_MAX_PBUFFER_WIDTH,          2048,
	EGL_MAX_PBUFFER_HEIGHT,         2048,
	EGL_MAX_PBUFFER_PIXELS,         2048*2048,
#endif
	EGL_NATIVE_RENDERABLE,          EGL_FALSE,
	EGL_NATIVE_VISUAL_ID,           0,
	EGL_NATIVE_VISUAL_TYPE,         EGL_NONE,
	EGL_SAMPLES,                    0,
	EGL_SAMPLE_BUFFERS,             0,
	EGL_SURFACE_TYPE,               EGL_PBUFFER_BIT,
	EGL_TRANSPARENT_TYPE,           EGL_NONE,
	EGL_TRANSPARENT_RED_VALUE,      0,
	EGL_TRANSPARENT_GREEN_VALUE,    0,
	EGL_TRANSPARENT_BLUE_VALUE,     0,
	EGL_MIN_SWAP_INTERVAL,          1, 
	EGL_MAX_SWAP_INTERVAL,          1, 
	EGL_COLOR_BUFFER_TYPE,          EGL_RGB_BUFFER,
	EGL_MATCH_NATIVE_PIXMAP,        EGL_NONE,
	EGL_CONFORMANT,                 0,
	EGL_RENDERABLE_TYPE,            0,
#if defined(EGL_EXTENSION_ANDROID_RECORDABLE)
	EGL_RECORDABLE_ANDROID,			EGL_FALSE,
#endif /* defined(EGL_EXTENSION_ANDROID_RECORDABLE) */

	EGL_NONE
};

static const EGLint asPBufferBaseES1VGAttribs_OpenGLES1[] =
{
	EGL_DEPTH_SIZE,                 24,
	EGL_STENCIL_SIZE,               8,
	EGL_BIND_TO_TEXTURE_RGB,        1, 
	EGL_BIND_TO_TEXTURE_RGBA,       0, 

	EGL_NONE
};

static const EGLint asPBufferBaseES1VGAttribs_OpenVG[] =
{
	EGL_ALPHA_MASK_SIZE,            8,
	EGL_SURFACE_TYPE,               EGL_PBUFFER_BIT | EGL_VG_COLORSPACE_LINEAR_BIT | EGL_VG_ALPHA_FORMAT_PRE_BIT,

	EGL_NONE
};

#endif /* defined(SUPPORT_OPENGLES1) || defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX) */


/*  Configuration variation attributes  */ 
#if defined(SUPPORT_OPENGLES2) || defined(API_MODULES_RUNTIME_CHECKED)

#define BASE_ES2_CONFIG_VARIANTS 3

static const EGLint _ES2PBufferVariant[] = 
{ 
	EGL_SURFACE_TYPE,               EGL_PBUFFER_BIT ,
#if defined(EGL_EXTENSION_ANDROID_RECORDABLE)
	EGL_RECORDABLE_ANDROID,			EGL_FALSE,
#endif /* defined(EGL_EXTENSION_ANDROID_RECORDABLE) */

	EGL_NONE
};

static const EGLint _ES2NoDepthStencilVariant[] = 
{ 
	EGL_DEPTH_SIZE,                 0,
	EGL_STENCIL_SIZE,               0,

	EGL_NONE
};

static const EGLint _ES2MultiSampleVariant[] = 
{ 
	EGL_SAMPLE_BUFFERS,             1,
	EGL_SAMPLES,                    4,

	EGL_NONE
};

/* 
 * GLES2 base configuration
 */
static const EGLint aBaseES2Attribs[] =
{
	EGL_BUFFER_SIZE,                0,        /* ws must define */
	EGL_ALPHA_SIZE,                 0,        /* ws must define */
	EGL_RED_SIZE,                   0,        /* ws must define */
	EGL_GREEN_SIZE,                 0,        /* ws must define */
	EGL_BLUE_SIZE,                  0,        /* ws must define */
	EGL_DEPTH_SIZE,                 24,
	EGL_STENCIL_SIZE,               8,
	EGL_LUMINANCE_SIZE,             0,
	EGL_ALPHA_MASK_SIZE,            0,
	EGL_CONFIG_CAVEAT,              EGL_NONE,
	EGL_CONFIG_ID,                  0,
	EGL_LEVEL,                      0,

#if(EURASIA_TEXTURESIZE_MAX > 2048)
	EGL_MAX_PBUFFER_WIDTH,          4096,
	EGL_MAX_PBUFFER_HEIGHT,         4096,
	EGL_MAX_PBUFFER_PIXELS,         4096*4096,
#else
	EGL_MAX_PBUFFER_WIDTH,          2048,
	EGL_MAX_PBUFFER_HEIGHT,         2048,
	EGL_MAX_PBUFFER_PIXELS,         2048*2048,
#endif
	EGL_NATIVE_RENDERABLE,          EGL_FALSE,
	EGL_NATIVE_VISUAL_ID,           0,
	EGL_NATIVE_VISUAL_TYPE,         EGL_NONE,
	EGL_SAMPLES,                    0,
	EGL_SAMPLE_BUFFERS,             0,
	EGL_SURFACE_TYPE,               0,
	EGL_TRANSPARENT_TYPE,           EGL_NONE,
	EGL_TRANSPARENT_RED_VALUE,      0,
	EGL_TRANSPARENT_GREEN_VALUE,    0,
	EGL_TRANSPARENT_BLUE_VALUE,     0,
	EGL_CONFORMANT,                 EGL_OPENGL_ES2_BIT,
	EGL_RENDERABLE_TYPE,            EGL_OPENGL_ES2_BIT,
	EGL_MIN_SWAP_INTERVAL,          1,        /* ws must define */
	EGL_MAX_SWAP_INTERVAL,          1,        /* ws must define */
	EGL_BIND_TO_TEXTURE_RGB,        0,      
	EGL_BIND_TO_TEXTURE_RGBA,       0,     
	EGL_COLOR_BUFFER_TYPE,          EGL_RGB_BUFFER,
	EGL_MATCH_NATIVE_PIXMAP,        EGL_NONE,
#if defined(EGL_EXTENSION_ANDROID_RECORDABLE)
	EGL_RECORDABLE_ANDROID,			EGL_TRUE,
#endif /* defined(EGL_EXTENSION_ANDROID_RECORDABLE) */

	EGL_NONE
};

static const EGLint aBaseES2Attribs_OpenVG[] =
{
	EGL_ALPHA_MASK_SIZE,            8,
	EGL_SURFACE_TYPE,               EGL_VG_COLORSPACE_LINEAR_BIT | EGL_VG_ALPHA_FORMAT_PRE_BIT,
	EGL_CONFORMANT,                 EGL_OPENGL_ES2_BIT | EGL_OPENVG_BIT,
	EGL_RENDERABLE_TYPE,            EGL_OPENGL_ES2_BIT | EGL_OPENVG_BIT,

	EGL_NONE
};

#endif /* defined(SUPPORT_OPENGLES2) || defined(API_MODULES_RUNTIME_CHECKED)) */



static const enum 
{
	atleast,
	exact,
	mask
} aAttribMatchCriteria[] =
{
#define A(N,D,C) C,
#include "attrib.h"
#undef A
};

static const EGLint aAttribDflts[] =
{
#define A(N,D,C) N, D,
#include "attrib.h"
#undef A
EGL_NONE
};

/*
	Lookup table used by:
	CFG_CompatibleConfigs: the whole list
	_CompareColourBits: Just the first 4, the colours
*/
static const EGLint aAttribCheckList[] =
{
	EGL_ALPHA_SIZE,
	EGL_RED_SIZE,
	EGL_GREEN_SIZE,
	EGL_BLUE_SIZE,
	EGL_SAMPLE_BUFFERS,
	EGL_SAMPLES,
	EGL_DEPTH_SIZE,
	EGL_STENCIL_SIZE

};

#define ATTRIB_CHECKLIST_SIZE_FULL          (sizeof(aAttribCheckList)/sizeof(EGLint))
#define ATTRIB_CHECKLIST_SIZE_ONLY_COLOURS  4
#define ATTRIB_CHECKLIST_SIZE_VG            6


/*
   <function>
   FUNCTION   : _CaveatOrder
   PURPOSE    : Determine a sort order priority for a caveat.
   PARAMETERS : In:  caveat -
   RETURNS    : Sort priority.
   </function>
 */
static EGLint _CaveatOrder(EGLint caveat)
{
	switch (caveat)
	{
		case EGL_NONE:
		{
			return 0;
		}
		case EGL_SLOW_CONFIG:
		{
			return 1;
		}
		case EGL_NON_CONFORMANT_CONFIG:
		{
			return 2;
		}
	}

	return 0;
}


/*
   <function>
   FUNCTION   : _CompareColourBits
   PURPOSE    :

   Find the difference in the colour bits of two configs.
   Colour bits include RGBA. Only colours which are not specified as
   DON'T CARE or 0 are included in the sum.

   PARAMETERS : In:  pCfgA - configuration to sum colour bits from.
				In:  pCfgB - configuration to sum colour bits from.
				In:  pRequestedCfg - requested config values.
   RETURNS    : Difference in significant colour bits.
   </function>
 */
static EGLint _CompareColourBits(const KEGL_CONFIG *pCfgA,
								 const KEGL_CONFIG *pCfgB,
								 const KEGL_CONFIG *pRequestedCfg)
{
	EGLint *pnAttrib;
	EGLint *pnAttibEnd;

	EGLint iTotalA, iTotalB;
	iTotalA = 0;
	iTotalB = 0;

	pnAttrib = (EGLint *)aAttribCheckList;
	pnAttibEnd = pnAttrib + ATTRIB_CHECKLIST_SIZE_ONLY_COLOURS;

	do
	{
		EGLint v = CFGC_GetAttrib(pRequestedCfg, *pnAttrib);
   
		if ((v == EGL_DONT_CARE) || (v == 0))
		{
			/* Don't need to query the attribute */
		}
		else
{
			iTotalA += CFGC_GetAttrib(pCfgA, *pnAttrib);
			iTotalB += CFGC_GetAttrib(pCfgB, *pnAttrib);
		}
		pnAttrib++;
	}
	while (pnAttrib < pnAttibEnd);

	return (iTotalB - iTotalA);
}

/* Functions to build OpenGLES 1 and/or OpenVG base and pbuffer configurations */ 
#if defined(SUPPORT_OPENGLES1) || defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX) || defined(API_MODULES_RUNTIME_CHECKED)
/*
   <function>
   FUNCTION   : _BuildBaseES1VGCfg_CommonBase
   PURPOSE    :

   Construct an APIaware starting point for a base ES1 and/or VGconfiguration
   based on aBaseES1VGAttribs[] and variations. APIawareness is based on either
   untime APImodule checking or SUPPORT_* defines.

   RETURNS    : Configuration or IMG_NULL.
   </function>
 */
static KEGL_CONFIG *_BuildBaseES1VGCfg_CommonBase(IMG_UINT32 ui32RequiredApi)
{
	EGLGlobal   *psGlobalData   = ENV_GetGlobalData();
	KEGL_CONFIG *psCfg          = IMG_NULL;

	if(!psGlobalData)
	{
		return IMG_NULL;
	}

#if defined(API_MODULES_RUNTIME_CHECKED)

	/* Either OpenGL ES 1 or OpenVG is always required */ 
	if(
		!psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENGLES1] &&
		!psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENVG]
	)
	{
		return IMG_NULL;
	}

	/* Check we have a specific required API, if any */
	if(
		(ui32RequiredApi != IMGEGL_CONTEXT_TYPEMAX) &&
		!psGlobalData->bApiModuleDetected[ui32RequiredApi]
	)
	{
		return IMG_NULL;
	}

	psCfg = CFGC_CreateAvArray(aBaseES1VGAttribs, ATTRIB_COUNT(aBaseES1VGAttribs));

	if(psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENGLES1])
	{
		psCfg = CFGC_ModifyAvArrayNl(psCfg, aBaseES1VGAttribs_OpenGLES1,    ATTRIB_COUNT(aBaseES1VGAttribs_OpenGLES1));
	}
	if(psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENVG])
	{
		psCfg = CFGC_ModifyAvArrayNl(psCfg, aBaseES1VGAttribs_OpenVG,       ATTRIB_COUNT(aBaseES1VGAttribs_OpenVG));
	}

#else /* defined(API_MODULES_RUNTIME_CHECKED) */

	(void)(ui32RequiredApi);

	psCfg = CFGC_CreateAvArray(aBaseES1VGAttribs, ATTRIB_COUNT(aBaseES1VGAttribs));

#   if defined(SUPPORT_OPENGLES1)
	psCfg = CFGC_ModifyAvArrayNl(psCfg, aBaseES1VGAttribs_OpenGLES1,    ATTRIB_COUNT(aBaseES1VGAttribs_OpenGLES1));
#   endif

#   if defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX)
	psCfg = CFGC_ModifyAvArrayNl(psCfg, aBaseES1VGAttribs_OpenVG,       ATTRIB_COUNT(aBaseES1VGAttribs_OpenVG));
#   endif

#endif /* defined(API_MODULES_RUNTIME_CHECKED) */

	return psCfg;
}

/*
   <function>
   FUNCTION   : _BuildBaseES1VGCfg
   PURPOSE    :

   Construct a specified base configuration. The variant supplied will be
   in the range 0..(N-1) where N is BASE_ES1_CONFIG_VARIANTS.
   
   PARAMETERS : In:  iEglVariant - Base configuration variant.
   RETURNS    : Configuration or IMG_NULL.
   </function>
 */
static KEGL_CONFIG *_BuildBaseES1VGCfg(EGLint iEglVariant)
{
	KEGL_CONFIG *psCfg = IMG_NULL;

	switch (iEglVariant)
	{
		case 0:
		{
			/* ES1VG */
			psCfg = _BuildBaseES1VGCfg_CommonBase(IMGEGL_CONTEXT_TYPEMAX);
			break;
		}
		case 1:
		{
			/* ES1VG, no depth */
			psCfg = _BuildBaseES1VGCfg_CommonBase(IMGEGL_CONTEXT_TYPEMAX);
			psCfg = CFGC_ModifyAvArrayNl(psCfg, _ES1VGNoDepthStencilVariant, ATTRIB_COUNT(_ES1VGNoDepthStencilVariant));
			
			break;
		}
		case 2:
		{
			/* ES1VG, multisampling  */
			psCfg = _BuildBaseES1VGCfg_CommonBase(IMGEGL_CONTEXT_TYPEMAX);
			psCfg = CFGC_ModifyAvArrayNl(psCfg, _ES1VGMultiSampleVariant, ATTRIB_COUNT(_ES1VGMultiSampleVariant));
			break;
		}
                case 3:
                {
                        /* ES1VG, 16 bit depth  */
                        psCfg = _BuildBaseES1VGCfg_CommonBase(IMGEGL_CONTEXT_TYPEMAX);
                        psCfg = CFGC_ModifyAvArrayNl(psCfg, _ES1VG16BitDepthVariant, ATTRIB_COUNT(_ES1VG16BitDepthVariant));
                        break;
                }
		default:
		{
			PVR_DPF((PVR_DBG_ERROR,"Unknown base ES1 config variant"));
			break;
		}

	}

	return psCfg;
}

static KEGL_CONFIG *_BuildPBufferBaseES1VGCfg_CommonBase(void)
{
	EGLGlobal   *psGlobalData   = ENV_GetGlobalData();
	KEGL_CONFIG *psCfg          = IMG_NULL;

	if(!psGlobalData)
	{
		return IMG_NULL;
	}

#if defined(API_MODULES_RUNTIME_CHECKED)

	/* Either OpenGL ES 1 or OpenVG is always required */ 
	if(
		!psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENGLES1] &&
		!psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENVG]
	)
	{
		return IMG_NULL;
	}

	psCfg = CFGC_CreateAvArray(asPBufferBaseES1VGAttribs, ATTRIB_COUNT(asPBufferBaseES1VGAttribs));

	if(psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENGLES1])
	{
		psCfg = CFGC_ModifyAvArrayNl(psCfg, asPBufferBaseES1VGAttribs_OpenGLES1,    ATTRIB_COUNT(asPBufferBaseES1VGAttribs_OpenGLES1));
	}

	if(psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENVG])
	{
		psCfg = CFGC_ModifyAvArrayNl(psCfg, asPBufferBaseES1VGAttribs_OpenVG,       ATTRIB_COUNT(asPBufferBaseES1VGAttribs_OpenVG));
	}

#else /* defined(API_MODULES_RUNTIME_CHECKED) */

	psCfg = CFGC_CreateAvArray(asPBufferBaseES1VGAttribs, ATTRIB_COUNT(asPBufferBaseES1VGAttribs));

#   if defined(SUPPORT_OPENGLES1)
	psCfg = CFGC_ModifyAvArrayNl(psCfg, asPBufferBaseES1VGAttribs_OpenGLES1,    ATTRIB_COUNT(asPBufferBaseES1VGAttribs_OpenGLES1));
#   endif

#   if defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX)
	psCfg = CFGC_ModifyAvArrayNl(psCfg, asPBufferBaseES1VGAttribs_OpenVG,       ATTRIB_COUNT(asPBufferBaseES1VGAttribs_OpenVG));
#   endif

#endif /* defined(API_MODULES_RUNTIME_CHECKED) */

	return psCfg;
}


/*
   <function>
   FUNCTION   : _BuildPBufferBaseES1VGCfg
   PURPOSE    :

   Construct a specified base configuration. The variant supplied will be
   in the range 0..(N-1) where N is PBUFFER_ES1_CONFIG_VARIANTS.
   
   PARAMETERS : In:  iEglVariant - Base configuration variant.
   RETURNS    : Configuration or IMG_NULL.
   </function>
 */
static KEGL_CONFIG *_BuildPBufferBaseES1VGCfg(EGLint iEglVariant)
{
	KEGL_CONFIG *psCfg = IMG_NULL;

	switch (iEglVariant)
	{
		case 0:
		{
			/* 8888, ES, VG, Pbuffer */
			psCfg = _BuildPBufferBaseES1VGCfg_CommonBase();
			psCfg = CFGC_ModifyAvArrayNl(psCfg, _ES1VGPBufferVariant0, ATTRIB_COUNT(_ES1VGPBufferVariant0));
			if(psCfg != IMG_NULL)
			{
				psCfg->eWSEGLPixelFormat = WSEGL_PIXELFORMAT_ARGB8888;
				psCfg->ePVRPixelFormat = PVRSRV_PIXEL_FORMAT_ARGB8888;
			}

			break;
		}
		case 1:
		{
			/* 565, ES, VG, Pbuffer */
			psCfg = _BuildPBufferBaseES1VGCfg_CommonBase();
			psCfg = CFGC_ModifyAvArrayNl(psCfg, _ES1VGPBufferVariant1, ATTRIB_COUNT(_ES1VGPBufferVariant1));
			if(psCfg != IMG_NULL)
			{
				psCfg->eWSEGLPixelFormat = WSEGL_PIXELFORMAT_RGB565;
				psCfg->ePVRPixelFormat = PVRSRV_PIXEL_FORMAT_RGB565;
			}

			break;
		}
		case 2:
		{
			/* 4444, ES, VG, Pbuffer */
			psCfg = _BuildPBufferBaseES1VGCfg_CommonBase();
			psCfg = CFGC_ModifyAvArrayNl(psCfg, _ES1VGPBufferVariant2, ATTRIB_COUNT(_ES1VGPBufferVariant2));
			if(psCfg != IMG_NULL)
			{
				psCfg->eWSEGLPixelFormat = WSEGL_PIXELFORMAT_ARGB4444;
				psCfg->ePVRPixelFormat = PVRSRV_PIXEL_FORMAT_ARGB4444;
			}

			break;
		}
		case 3:
		{
			/* 1555, ES, VG, Pbuffer */
			psCfg = _BuildPBufferBaseES1VGCfg_CommonBase();
			psCfg = CFGC_ModifyAvArrayNl(psCfg, _ES1VGPBufferVariant3, ATTRIB_COUNT(_ES1VGPBufferVariant3));
			if(psCfg != IMG_NULL)
			{
				psCfg->eWSEGLPixelFormat = WSEGL_PIXELFORMAT_ARGB1555;
				psCfg->ePVRPixelFormat = PVRSRV_PIXEL_FORMAT_ARGB1555;
			}

			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR,"Unknown pbuffer ES config variant"));
			
			break;
		}

	}
  
	return psCfg;
}

#endif /* defined(SUPPORT_OPENGLES1) || defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX) || defined(API_MODULES_RUNTIME_CHECKED) */


/* Functions to build OpenGLES 2 base configurations */ 
#if defined(SUPPORT_OPENGLES2) || defined(API_MODULES_RUNTIME_CHECKED)

static KEGL_CONFIG *__BuildBaseES2Cfg_CommonBase(void)
{
	EGLGlobal   *psGlobalData   = ENV_GetGlobalData();
	KEGL_CONFIG *psCfg          = IMG_NULL;

	if(!psGlobalData)
	{
		return IMG_NULL;
	}

#if defined(API_MODULES_RUNTIME_CHECKED)

	/*  ES2 is needed in all runtime configurations  */ 
	if(!psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENGLES2])
	{
		return IMG_NULL;
	}

	psCfg = CFGC_CreateAvArray(aBaseES2Attribs, ATTRIB_COUNT(aBaseES2Attribs));

	if(psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENVG])
	{
		psCfg = CFGC_ModifyAvArrayNl(psCfg, aBaseES2Attribs_OpenVG,       ATTRIB_COUNT(aBaseES2Attribs_OpenVG));
	}

#else /* defined(API_MODULES_RUNTIME_CHECKED) */

	psCfg = CFGC_CreateAvArray(aBaseES2Attribs, ATTRIB_COUNT(aBaseES2Attribs));

#   if defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX)
	psCfg = CFGC_ModifyAvArrayNl(psCfg, aBaseES2Attribs_OpenVG,       ATTRIB_COUNT(aBaseES2Attribs_OpenVG));
#   endif

#endif /* defined(API_MODULES_RUNTIME_CHECKED) */

	return psCfg;
}


/*
   <function>
   FUNCTION   : _BuildBaseES2Cfg
   PURPOSE    :

   Construct a specified base configuration. The variant supplied will be
   in the range 0..(N-1) where N is BASE_ES2_CONFIG_VARIANTS.
   
   PARAMETERS : In:  iEglVariant - Base configuration variant.
   RETURNS    : Configuration or IMG_NULL.
   </function>
 */
static KEGL_CONFIG *_BuildBaseES2Cfg(EGLint iEglVariant)
{
	KEGL_CONFIG *psCfg = IMG_NULL;

	switch (iEglVariant)
	{
		case 0:
		{
			/* ES2 */
			psCfg = __BuildBaseES2Cfg_CommonBase();
			psCfg = CFGC_ModifyAvArrayNl(psCfg, _ES2PBufferVariant, ATTRIB_COUNT(_ES2PBufferVariant));
			break;
		}
		case 1:
		{
			/* ES2, no depth */
			psCfg = __BuildBaseES2Cfg_CommonBase();
			psCfg = CFGC_ModifyAvArrayNl(psCfg, _ES2NoDepthStencilVariant, ATTRIB_COUNT(_ES2NoDepthStencilVariant));
			psCfg = CFGC_ModifyAvArrayNl(psCfg, _ES2PBufferVariant, ATTRIB_COUNT(_ES2PBufferVariant));
			
			break;
		}
		case 2:
		{
			/* ES2, multisampling  */
			psCfg = __BuildBaseES2Cfg_CommonBase();
			psCfg = CFGC_ModifyAvArrayNl(psCfg, _ES2MultiSampleVariant, ATTRIB_COUNT(_ES2MultiSampleVariant));
			
			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR,"Unknown base ES2 config variant"));
			break;
		}

	}

	return psCfg;
}

#endif /* defined(SUPPORT_OPENGLES2) ||defined(API_MODULES_RUNTIME_CHECKED) */


/* Functions to build OpenGL base and pbuffer configurations */ 
#if defined(SUPPORT_OPENGL) || defined(API_MODULES_RUNTIME_CHECKED)
/* EGL OGL Configs */

#   define BASE_OGL_CONFIG_VARIANTS     3
#   define PBUFFER_OGL_CONFIG_VARIANTS  3

static const EGLint _OGLNoDepthStencilVariant[] = 
{ 
	EGL_DEPTH_SIZE,     0,
	EGL_STENCIL_SIZE,   0,
	EGL_NONE
};

static const EGLint _OGLMultiSampleVariant[] = 
{ 
	EGL_SAMPLE_BUFFERS, 1,
	EGL_SAMPLES,        4,

	EGL_NONE
};


static const EGLint _OGLPBufferVariant1[] = 
{ 
	EGL_BUFFER_SIZE,            16,
	EGL_ALPHA_SIZE,             4,
	EGL_RED_SIZE,               4,
	EGL_GREEN_SIZE,             4,
	EGL_BLUE_SIZE,              4,
	EGL_BIND_TO_TEXTURE_RGBA,   1,
	EGL_NONE
};

static const EGLint _OGLPBufferVariant2[] = 
{ 
	EGL_BUFFER_SIZE,            32,
	EGL_ALPHA_SIZE,             8,
	EGL_RED_SIZE,               8,
	EGL_GREEN_SIZE,             8,
	EGL_BLUE_SIZE,              8,
	EGL_BIND_TO_TEXTURE_RGBA,   1,
	EGL_NONE
};



/* This array defines the base configuration attribute values for OGL. The
 * base configurations is passed to the WS which creates delta
 * configurations from the base. 
 *
 * OGL base configuration
 */
static const EGLint aBaseOGLAttribs[] =
{
	EGL_BUFFER_SIZE,                0,      /* ws must define */
	EGL_ALPHA_SIZE,                 0,      /* ws must define */
	EGL_RED_SIZE,                   0,      /* ws must define */
	EGL_GREEN_SIZE,                 0,      /* ws must define */
	EGL_BLUE_SIZE,                  0,      /* ws must define */
	EGL_DEPTH_SIZE,                 24,
	EGL_STENCIL_SIZE,               8,
	EGL_LUMINANCE_SIZE,             0,
	EGL_ALPHA_MASK_SIZE,            0,
	EGL_CONFIG_CAVEAT,              EGL_NONE,
	EGL_CONFIG_ID,                  0,
	EGL_LEVEL,                      0,
#if(EURASIA_TEXTURESIZE_MAX > 2048)
	EGL_MAX_PBUFFER_WIDTH,          4096,
	EGL_MAX_PBUFFER_HEIGHT,         4096,
	EGL_MAX_PBUFFER_PIXELS,         4096*4096,
#else
	EGL_MAX_PBUFFER_WIDTH,          2048,
	EGL_MAX_PBUFFER_HEIGHT,         2048,
	EGL_MAX_PBUFFER_PIXELS,         2048*2048,
#endif
	EGL_NATIVE_RENDERABLE,          EGL_FALSE,
	EGL_NATIVE_VISUAL_ID,           0,
	EGL_NATIVE_VISUAL_TYPE,         EGL_NONE,
	EGL_SAMPLES,                    0,
	EGL_SAMPLE_BUFFERS,             0,
	EGL_SURFACE_TYPE,               EGL_PBUFFER_BIT,
	EGL_TRANSPARENT_TYPE,           EGL_NONE,
	EGL_TRANSPARENT_RED_VALUE,      0,
	EGL_TRANSPARENT_GREEN_VALUE,    0,
	EGL_TRANSPARENT_BLUE_VALUE,     0,
	EGL_CONFORMANT,                 EGL_OPENGL_BIT,
	EGL_RENDERABLE_TYPE,            EGL_OPENGL_BIT,
	EGL_MIN_SWAP_INTERVAL,          1,      /* ws must define */
	EGL_MAX_SWAP_INTERVAL,          1,      /* ws must define */
	EGL_BIND_TO_TEXTURE_RGB,        0,      /* ws must define */
	EGL_BIND_TO_TEXTURE_RGBA,       0,      /* ws must define */
	EGL_COLOR_BUFFER_TYPE,          EGL_RGB_BUFFER,
	EGL_MATCH_NATIVE_PIXMAP,        EGL_NONE,

	EGL_NONE
};


/* This array defines the base configuration attribute values for
 * pbuffers only configs for OGL. 
 */

static const EGLint asPBufferBaseOGLAttribs[] =
{
	EGL_BUFFER_SIZE,                16,
	EGL_ALPHA_SIZE,                 0,
	EGL_RED_SIZE,                   5,
	EGL_GREEN_SIZE,                 6,
	EGL_BLUE_SIZE,                  5,
	EGL_LUMINANCE_SIZE,             0,
	EGL_ALPHA_MASK_SIZE,            0,
	EGL_DEPTH_SIZE,                 24,
	EGL_STENCIL_SIZE,               8,
	EGL_CONFIG_CAVEAT,              EGL_NONE,
	EGL_CONFIG_ID,                  0,
	EGL_LEVEL,                      0,
#if(EURASIA_TEXTURESIZE_MAX > 2048)
	EGL_MAX_PBUFFER_WIDTH,          4096,
	EGL_MAX_PBUFFER_HEIGHT,         4096,
	EGL_MAX_PBUFFER_PIXELS,         4096*4096,
#else
	EGL_MAX_PBUFFER_WIDTH,          2048,
	EGL_MAX_PBUFFER_HEIGHT,         2048,
	EGL_MAX_PBUFFER_PIXELS,         2048*2048,
#endif
	EGL_NATIVE_RENDERABLE,          EGL_FALSE,
	EGL_NATIVE_VISUAL_ID,           0,
	EGL_NATIVE_VISUAL_TYPE,         EGL_NONE,
	EGL_SAMPLES,                    0,
	EGL_SAMPLE_BUFFERS,             0,
	EGL_SURFACE_TYPE,               EGL_PBUFFER_BIT,
	EGL_TRANSPARENT_TYPE,           EGL_NONE,
	EGL_TRANSPARENT_RED_VALUE,      0,
	EGL_TRANSPARENT_GREEN_VALUE,    0,
	EGL_TRANSPARENT_BLUE_VALUE,     0,
	EGL_MIN_SWAP_INTERVAL,          1, 
	EGL_MAX_SWAP_INTERVAL,          1, 
	EGL_BIND_TO_TEXTURE_RGB,        1, 
	EGL_BIND_TO_TEXTURE_RGBA,       0, 
	EGL_COLOR_BUFFER_TYPE,          EGL_RGB_BUFFER,
	EGL_MATCH_NATIVE_PIXMAP,        EGL_NONE,
	EGL_CONFORMANT,                 EGL_OPENGL_BIT,
	EGL_RENDERABLE_TYPE,            EGL_OPENGL_BIT,

	EGL_NONE
};

/*
   <function>
   FUNCTION   : _BuildBaseOGLCfg
   PURPOSE    :

   Construct a specified base configuration. The variant supplied will be
   in the range 0..(N-1) where N is BASE_OGL_CONFIG_VARIANTS.
   
   PARAMETERS : In:  iEglVariant - Base configuration variant.
   RETURNS    : Configuration or IMG_NULL.
   </function>
 */
static KEGL_CONFIG *_BuildBaseOGLCfg(EGLint iEglVariant)
{
	KEGL_CONFIG *psCfg = IMG_NULL;

	switch (iEglVariant)
	{
		case 0:
		{
			/* OGL (with depth) */
			psCfg = CFGC_CreateAvArray(aBaseOGLAttribs, ATTRIB_COUNT(aBaseOGLAttribs));
			break;
		}
		case 1:
		{
			/* OGL, no depth */
			psCfg = CFGC_CreateAvArray(aBaseOGLAttribs, ATTRIB_COUNT(aBaseOGLAttribs));
			psCfg = CFGC_ModifyAvArrayNl(psCfg, _OGLNoDepthStencilVariant, ATTRIB_COUNT(_OGLNoDepthStencilVariant));
			
			break;
		}
		case 2:
		{
			/* OGL, multisampling */
			psCfg = CFGC_CreateAvArray(aBaseOGLAttribs, ATTRIB_COUNT(aBaseOGLAttribs));
			psCfg = CFGC_ModifyAvArrayNl(psCfg, _OGLMultiSampleVariant, ATTRIB_COUNT(_OGLMultiSampleVariant));
			
			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR,"Unknown base OGL config variant"));
			break;
		}

	}

	return psCfg;
}

/*
   <function>
   FUNCTION   : _BuildPBufferBaseOGLCfg
   PURPOSE    :

   Construct a specified base configuration. The variant supplied will be
   in the range 0..(N-1) where N is PBUFFER_OGL_CONFIG_VARIANTS.
   
   PARAMETERS : In:  iEglVariant - Base configuration variant.
   RETURNS    : Configuration or IMG_NULL.
   </function>
 */
static KEGL_CONFIG *_BuildPBufferBaseOGLCfg(EGLint iEglVariant)
{
	KEGL_CONFIG *psCfg = IMG_NULL;

	switch (iEglVariant)
	{
		case 0:
		{
			/* 8888, OGL, Pbuffer */
			psCfg = CFGC_CreateAvArray(asPBufferBaseOGLAttribs, ATTRIB_COUNT(asPBufferBaseOGLAttribs));
			psCfg = CFGC_ModifyAvArrayNl(psCfg, _OGLPBufferVariant2, ATTRIB_COUNT(_OGLPBufferVariant2));

			break;
		}
		case 1:
		{
			/* 565, OGL, Pbuffer */
			psCfg = CFGC_CreateAvArray(asPBufferBaseOGLAttribs, ATTRIB_COUNT(asPBufferBaseOGLAttribs));

			break;
		}
		case 2:
		{
			/* 4444, OGL, Pbuffer */
			psCfg = CFGC_CreateAvArray(asPBufferBaseOGLAttribs, ATTRIB_COUNT(asPBufferBaseOGLAttribs));
			psCfg = CFGC_ModifyAvArrayNl(psCfg, _OGLPBufferVariant1, ATTRIB_COUNT(_OGLPBufferVariant1));

			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR,"Unknown pbuffer OGL config variant"));
			break;
		}

	}
  
	return psCfg;
}

#endif /* if defined(SUPPORT_OPENGL)||defined(API_MODULES_RUNTIME_CHECKED) */

#if defined(API_MODULES_RUNTIME_CHECKED)

IMG_INTERNAL void CFG_Initialise(void)
{
	EGLGlobal *psGlobalData = ENV_GetGlobalData();

	PVR_ASSERT(psGlobalData!=IMG_NULL)

	psGlobalData->baseES1VGConfigVariants    = 0;
	psGlobalData->pbufferES1VGConfigVariants = 0;
	psGlobalData->baseES2ConfigVariants      = 0;
	psGlobalData->baseOGLConfigVariants      = 0;
	psGlobalData->pbufferOGLConfigVariants   = 0;

	if(psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENGLES1])
	{
		psGlobalData->baseES1VGConfigVariants    = 4;
		psGlobalData->pbufferES1VGConfigVariants = 4;
	}
	else if(psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENVG])
	{
		psGlobalData->baseES1VGConfigVariants    = 1;
		psGlobalData->pbufferES1VGConfigVariants = 4;
	}

	if(psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENGLES2])
	{
		psGlobalData->baseES2ConfigVariants = 3;
	}

	if(psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENGL])
	{
		psGlobalData->baseOGLConfigVariants    = 6;
		psGlobalData->pbufferOGLConfigVariants = 6;
	}
}

#else /* defined(API_MODULES_RUNTIME_CHECKED) */

IMG_INTERNAL void CFG_Initialise(void)
{
	EGLGlobal *psGlobalData = ENV_GetGlobalData();

	PVR_ASSERT(psGlobalData!=IMG_NULL)

	psGlobalData->baseES1VGConfigVariants    = 0;
	psGlobalData->pbufferES1VGConfigVariants = 0;
	psGlobalData->baseES2ConfigVariants      = 0;
	psGlobalData->baseOGLConfigVariants      = 0;
	psGlobalData->pbufferOGLConfigVariants   = 0;

#if defined(SUPPORT_OPENGLES1) || defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX)
	psGlobalData->baseES1VGConfigVariants    = BASE_ES1_VG_CONFIG_VARIANTS;
	psGlobalData->pbufferES1VGConfigVariants = PBUFFER_ES1_VG_CONFIG_VARIANTS;
#endif

#if defined(SUPPORT_OPENGLES2)
	psGlobalData->baseES2ConfigVariants      = BASE_ES2_CONFIG_VARIANTS;
#endif

#if defined(SUPPORT_OPENGL)
	psGlobalData->baseOGLConfigVariants      = BASE_OGL_CONFIG_VARIANTS;
	psGlobalData->pbufferOGLConfigVariants   = PBUFFER_OGL_CONFIG_VARIANTS;
#endif

}

#endif /* defined(API_MODULES_RUNTIME_CHECKED) */


/*
   <function>
   FUNCTION   : CFG_variants
   PURPOSE    :

   Determine the number of configurations supported for a specified
   display.  This function queries the window system interface to
   determine the number of configurations variations that can be
   generated from the base configurations defined by the generic EGL
   layer.
   
   PARAMETERS : In:  psDpy - Display.
   RETURNS    : Number of configurations.
   </function>
 */
IMG_INTERNAL EGLint  CFG_Variants(KEGL_DISPLAY *psDpy)
{
	EGLGlobal   *psGlobalData   = ENV_GetGlobalData();
	EGLint      returnValue     = 0;
	
	if(!psGlobalData)
	{
		return 0;
	}

	returnValue += (psDpy->ui32NumConfigs * psGlobalData->baseES1VGConfigVariants) + psGlobalData->pbufferES1VGConfigVariants;
	returnValue += (psDpy->ui32NumConfigs * psGlobalData->baseES2ConfigVariants);
	returnValue += (psDpy->ui32NumConfigs * psGlobalData->baseOGLConfigVariants) + psGlobalData->pbufferOGLConfigVariants;

	return (returnValue);
}


static KEGL_CONFIG *DeriveCfg(KEGL_DISPLAY *psDisplay, KEGL_CONFIG *psConfig, EGLint variant)
{
	PVRSRV_PIXEL_FORMAT pf;
	WSEGLConfig *psWSConfig;
	EGLint wsSurfaceType = 0, surfaceType;
	EGLint iRenderableType;
	EGLint iConformantBits;

	if ((IMG_UINT32)(variant+1)>psDisplay->ui32NumConfigs)
	{
		return (KEGL_CONFIG *)0;
	}
	
	if (!psDisplay->psConfigs)
	{
		return (KEGL_CONFIG *)0;
	}

	psWSConfig = &psDisplay->psConfigs[variant];
	
	switch(psWSConfig->ePixelFormat)
	{
		case WSEGL_PIXELFORMAT_ARGB8888:
		{
			pf = PVRSRV_PIXEL_FORMAT_ARGB8888;

			break;
		}
		case WSEGL_PIXELFORMAT_ABGR8888:
		{
			pf = PVRSRV_PIXEL_FORMAT_ABGR8888;

			break;
		}
		case WSEGL_PIXELFORMAT_XRGB8888:
		{
			pf = PVRSRV_PIXEL_FORMAT_XRGB8888;

			break;
		}
		case WSEGL_PIXELFORMAT_XBGR8888:
		{
			pf = PVRSRV_PIXEL_FORMAT_XBGR8888;

			break;
		}
		case WSEGL_PIXELFORMAT_1555:
		{
			pf = PVRSRV_PIXEL_FORMAT_ARGB1555;

			break;
		}
		case WSEGL_PIXELFORMAT_4444:
		{
			pf = PVRSRV_PIXEL_FORMAT_ARGB4444;

			break;
		}
		default:
		{
			pf = PVRSRV_PIXEL_FORMAT_RGB565;

			break;
		}
	}

	if(psWSConfig->ui32DrawableType & WSEGL_DRAWABLE_WINDOW)
	{
		wsSurfaceType |= EGL_WINDOW_BIT;
	}

	if(psWSConfig->ui32DrawableType & WSEGL_DRAWABLE_PIXMAP)
	{
		wsSurfaceType |= EGL_PIXMAP_BIT;
	}

	psConfig = CFGC_CopyNl(psConfig);
	if(psConfig == IMG_NULL)
	{
		return (KEGL_CONFIG *)0;
	}

	psConfig->eWSEGLPixelFormat = psWSConfig->ePixelFormat;
	psConfig->ePVRPixelFormat = pf;

	if(!CFGC_SetAttrib(psConfig, EGL_BUFFER_SIZE,           gasSRVPixelFormat[pf].iWidth))
	{
		CFGC_Unlink(psConfig);
		return (KEGL_CONFIG *)0;
	}
	if(!CFGC_SetAttrib(psConfig, EGL_ALPHA_SIZE,            gasSRVPixelFormat[pf].iAlpha))
	{
		CFGC_Unlink(psConfig);
		return (KEGL_CONFIG *)0;
	}
	if(!CFGC_SetAttrib(psConfig, EGL_RED_SIZE,              gasSRVPixelFormat[pf].iRed))
	{
		CFGC_Unlink(psConfig);
		return (KEGL_CONFIG *)0;
	}
	if(!CFGC_SetAttrib(psConfig, EGL_GREEN_SIZE,            gasSRVPixelFormat[pf].iGreen))
	{
		CFGC_Unlink(psConfig);
		return (KEGL_CONFIG *)0;
	}
	if(!CFGC_SetAttrib(psConfig, EGL_BLUE_SIZE,             gasSRVPixelFormat[pf].iBlue))
	{
		CFGC_Unlink(psConfig);
		return (KEGL_CONFIG *)0;
	}
	if(!CFGC_SetAttrib(psConfig, EGL_LEVEL,                 psWSConfig->ulFrameBufferLevel))
	{
		CFGC_Unlink(psConfig);
		return (KEGL_CONFIG *)0;
	}
	if(!CFGC_SetAttrib(psConfig, EGL_NATIVE_VISUAL_ID,      psWSConfig->ulNativeVisualID))
	{
		CFGC_Unlink(psConfig);
		return (KEGL_CONFIG *)0;
	}
	if(!CFGC_SetAttrib(psConfig, EGL_NATIVE_VISUAL_TYPE,    (EGLint)psWSConfig->hNativeVisual))
	{
		CFGC_Unlink(psConfig);
		return (KEGL_CONFIG *)0;
	}

	if(psWSConfig->ulNativeRenderable)
	{
		if(!CFGC_SetAttrib (psConfig, EGL_NATIVE_RENDERABLE, EGL_TRUE))
		{
			CFGC_Unlink(psConfig);
			return (KEGL_CONFIG *)0;
		}
	}

	if(psWSConfig->eTransparentType == WSEGL_COLOR_KEY)
	{
		IMG_UINT32 ui32Red   = (psWSConfig->ulTransparentColor & 0x00FF0000) >> 16;
		IMG_UINT32 ui32Green = (psWSConfig->ulTransparentColor & 0x0000FF00) >> 8;
		IMG_UINT32 ui32Blue  = (psWSConfig->ulTransparentColor & 0x000000FF);
		
		if(!CFGC_SetAttrib(psConfig, EGL_TRANSPARENT_TYPE, EGL_TRANSPARENT_RGB))
		{
			CFGC_Unlink(psConfig);
			return (KEGL_CONFIG *)0;
		}
		if(!CFGC_SetAttrib(psConfig, EGL_TRANSPARENT_RED_VALUE, ui32Red))
		{
			CFGC_Unlink(psConfig);
			return (KEGL_CONFIG *)0;
		}
		if(!CFGC_SetAttrib(psConfig, EGL_TRANSPARENT_GREEN_VALUE, ui32Green))
		{
			CFGC_Unlink(psConfig);
			return (KEGL_CONFIG *)0;
		}
		if(!CFGC_SetAttrib(psConfig, EGL_TRANSPARENT_BLUE_VALUE, ui32Blue))
		{
			CFGC_Unlink(psConfig);
			return (KEGL_CONFIG *)0;
		}
	}

	surfaceType = CFGC_GetAttrib(psConfig, EGL_SURFACE_TYPE);

	surfaceType |= wsSurfaceType;

	if(!CFGC_SetAttrib(psConfig, EGL_SURFACE_TYPE, surfaceType))
	{
		CFGC_Unlink(psConfig);
		return (KEGL_CONFIG *)0;
	}

	/* Set min/max swap interval that the display supports. */
	if(!CFGC_SetAttrib(psConfig, EGL_MIN_SWAP_INTERVAL, psDisplay->ui32MinSwapInterval))
	{
		CFGC_Unlink(psConfig);
		return (KEGL_CONFIG *)0;
	}
	if(!CFGC_SetAttrib(psConfig, EGL_MAX_SWAP_INTERVAL, psDisplay->ui32MaxSwapInterval))
	{
		CFGC_Unlink(psConfig);
		return (KEGL_CONFIG *)0;
	}

#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE)
	if(surfaceType & EGL_PBUFFER_BIT)
	{	
		/* Always allow RGB/RGBA pbuffers to be bound as rgb textures */ 
		if(!CFGC_SetAttrib(psConfig, EGL_BIND_TO_TEXTURE_RGB, IMG_TRUE))
		{
			CFGC_Unlink(psConfig);
			return (KEGL_CONFIG *)0;
		}

		if(gasSRVPixelFormat[pf].iAlpha)
		{
			if(!CFGC_SetAttrib(psConfig, EGL_BIND_TO_TEXTURE_RGBA, IMG_TRUE))
			{
				CFGC_Unlink(psConfig);
				return (KEGL_CONFIG *)0;
			}
		}
	}
#endif /* defined(EGL_EXTENSION_RENDER_TO_TEXTURE) */

	iRenderableType = CFGC_GetAttrib(psConfig, EGL_RENDERABLE_TYPE);

	/* Accumulate the complete list of conformant API's in this config */
	iConformantBits = CFGC_GetAttribAccumulate(psConfig, EGL_CONFORMANT);

	/* OpenVG is not conformant for MSAA configs */
	if ((iRenderableType & EGL_OPENVG_BIT) != 0 &&
		CFGC_GetAttrib(psConfig, EGL_SAMPLE_BUFFERS) != 0)
	{
		iConformantBits &= ~EGL_OPENVG_BIT;
	}

	/* Set the final conformant bits into the derived config */
	CFGC_SetAttrib(psConfig, EGL_CONFORMANT, iConformantBits);

	return psConfig;
}


/*
<function>
   FUNCTION   : CFG_GenerateVariant
   PURPOSE    :

   Generate a specific configuration from the configuration
   handle. The generic EGL component determines a generic EGL base
   configuration and a window system interface configuration variant
   from the EGLConfig supplied. The appropriate generic EGL base
   configuration is generated and passed to the window system
   interface to construct the required window system variant.

   PARAMETERS : In:  psDpy - Display
				In:  eglCfg - Configuration required.
				Out: ppCfg - Receives the generated configuration.
   RETURNS : EGL_SUCCESS - Success
			 Other EGL error code - failure.
</function>
 */
IMG_INTERNAL EGLint  CFG_GenerateVariant(
	KEGL_DISPLAY        *psDpy, 
	KEGL_CONFIG_INDEX   EglCfg, 
	KEGL_CONFIG         **ppCfg
)
{
	KEGL_CONFIG         *pDerivedCfg = (KEGL_CONFIG *)0;
	KEGL_CONFIG         *pCfgCopy    = (KEGL_CONFIG *)0;
	IMG_INT32           i32MaxCrtClientVariants;
	KEGL_CONFIG_INDEX   EglCfgRel; /* Config. number translated into current client config space */
	IMG_BOOL            bHandledConfig;

	EGLGlobal           *psGlobalData = IMG_NULL;

	PVR_ASSERT(psDpy!=IMG_NULL);
	PVR_ASSERT(ppCfg!=IMG_NULL);

	if ((EglCfg <= 0) || (EglCfg > CFG_Variants(psDpy)))
	{
		return EGL_BAD_CONFIG;
	}

	psGlobalData = ENV_GetGlobalData();
	if(!psGlobalData)
	{
		return EGL_BAD_ALLOC;
	}

	EglCfgRel = EglCfg-1;
	bHandledConfig = IMG_FALSE;

#if defined(SUPPORT_OPENGLES1) || defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX) || defined(API_MODULES_RUNTIME_CHECKED)

	i32MaxCrtClientVariants = (psDpy->ui32NumConfigs * psGlobalData->baseES1VGConfigVariants) + psGlobalData->pbufferES1VGConfigVariants;
	if(
		!bHandledConfig && 
		psGlobalData->baseES1VGConfigVariants && 
		(EglCfgRel < i32MaxCrtClientVariants)
	)
	{
		EGLint iEglVariant = EglCfgRel % psGlobalData->baseES1VGConfigVariants;
		EGLint iWsVariant  = EglCfgRel / psGlobalData->baseES1VGConfigVariants;
		
		if(iWsVariant < (EGLint)psDpy->ui32NumConfigs)
		{
			pDerivedCfg = DeriveCfg(psDpy, _BuildBaseES1VGCfg(iEglVariant), iWsVariant);
			
			if(!pDerivedCfg)
			{
				return EGL_BAD_ALLOC;
			}
			*ppCfg = pDerivedCfg;
		}
		else
		{
			EGLint iPBufferVariant;

			iPBufferVariant = EglCfgRel - (psDpy->ui32NumConfigs * psGlobalData->baseES1VGConfigVariants);
			
			*ppCfg = _BuildPBufferBaseES1VGCfg(iPBufferVariant);
		}

		bHandledConfig = IMG_TRUE;
	}
	else
	{
		/* Rebase (offset) EGLCfgRel for the following clients */
		EglCfgRel -= i32MaxCrtClientVariants;
	}
#endif /* defined(SUPPORT_OPENGLES1) || defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX) || defined(API_MODULES_RUNTIME_CHECKED) */


#if defined(SUPPORT_OPENGLES2) || defined(API_MODULES_RUNTIME_CHECKED)

	i32MaxCrtClientVariants = psDpy->ui32NumConfigs * psGlobalData->baseES2ConfigVariants;
	if(
		!bHandledConfig && 
		psGlobalData->baseES2ConfigVariants && 
		(EglCfgRel < i32MaxCrtClientVariants)
	)
	{
		EGLint iEglVariant = EglCfgRel % psGlobalData->baseES2ConfigVariants;
		EGLint iWsVariant  = EglCfgRel / psGlobalData->baseES2ConfigVariants;
		
		pDerivedCfg = DeriveCfg(psDpy, _BuildBaseES2Cfg(iEglVariant), iWsVariant);
			
		if(!pDerivedCfg)
		{
			return EGL_BAD_ALLOC;
		}
		*ppCfg = pDerivedCfg;
		bHandledConfig = IMG_TRUE;
	}
	else
	{
		/* Rebase (offset) EGLCfgRel for the following clients */
		EglCfgRel -= i32MaxCrtClientVariants;
	}
#endif /* defined(SUPPORT_OPENGLES2) */


#if defined(SUPPORT_OPENGL) || defined(API_MODULES_RUNTIME_CHECKED)

	i32MaxCrtClientVariants = (psDpy->ui32NumConfigs * psGlobalData->baseOGLConfigVariants) + psGlobalData->pbufferOGLConfigVariants;

	if (
		!bHandledConfig && 
		psGlobalData->baseOGLConfigVariants &&
		(EglCfgRel < i32MaxCrtClientVariants)
	)
	{
		EGLint iEglVariant = EglCfgRel % psGlobalData->baseOGLConfigVariants;
		EGLint iWsVariant  = EglCfgRel / psGlobalData->baseOGLConfigVariants;
		
		if(iWsVariant < (EGLint)psDpy->ui32NumConfigs)
		{
			pDerivedCfg = DeriveCfg(psDpy, _BuildBaseOGLCfg(iEglVariant), iWsVariant);
			if(!pDerivedCfg)
			{
				return EGL_BAD_ALLOC;
			}
			*ppCfg = pDerivedCfg;
		}
		else
		{
			EGLint iPBufferVariant;

			iPBufferVariant = EglCfgRel - (psDpy->ui32NumConfigs * psGlobalData->baseOGLConfigVariants);
				
			*ppCfg = _BuildPBufferBaseOGLCfg(iPBufferVariant);
		}

		bHandledConfig = IMG_TRUE;	
	}
	else
	{
		/* Rebase (offset) EGLCfgRel for the following clients */
		EglCfgRel -= i32MaxCrtClientVariants;
	}
#endif /* defined(SUPPORT_OPENGL) || defined(API_MODULES_RUNTIME_CHECKED) */


	pCfgCopy = CFGC_CopyNl(*ppCfg);
	if(!pCfgCopy)
	{
		return EGL_BAD_ALLOC;
	}

	*ppCfg = pCfgCopy;

	if (!CFGC_SetAttrib(*ppCfg, EGL_CONFIG_ID, EglCfg))
	{
		CFGC_Unlink(*ppCfg);

		return EGL_BAD_ALLOC;
	}
  
	return EGL_SUCCESS;
}


/*
   <function>
   FUNCTION   : CFG_Match
   PURPOSE    : Determine if a candidate config meets the selection criteria
				provided in a requested config.
   PARAMETERS : In: pRequestedCfg - The requested configuration.
				In: pCandidate - The candidate configuration.
   RETURNS    : EGL_TRUE - candiate matches the selection criteria
				EGL_FALSE - candiate does not match the selection criteria
   </function>
 */
IMG_INTERNAL EGLBoolean CFG_Match(const KEGL_CONFIG *pRequestedCfg, const KEGL_CONFIG *pCandidate)
{
	EGLint *pnAttrib;
	EGLint *pnAttibEnd;
	IMG_UINT32 *pui32AttribCriteria;

	pui32AttribCriteria = (IMG_UINT32 *)aAttribMatchCriteria;
	pnAttrib = (EGLint *)aAttribDflts;

	pnAttibEnd = pnAttrib + ATTRIB_COUNT(aAttribDflts);

	/* The last entry should be the EGL_NONE list terminator */
	PVR_ASSERT(*pnAttibEnd == EGL_NONE);

	do
	{
		EGLint iRequestValue;

		/* Do not accumulate bitfields in the *requested* cfg, only the candidate cfg */
		iRequestValue = CFGC_GetAttribNoAccumulate(pRequestedCfg, *pnAttrib);

		if (iRequestValue != EGL_DONT_CARE)
		{
			EGLint iCandidateValue;

			iCandidateValue = CFGC_GetAttrib(pCandidate, *pnAttrib);
	  
			switch (*pui32AttribCriteria)
			{
				case atleast:
				{
					if (iCandidateValue < iRequestValue)
					{
						return EGL_FALSE;
					}

					break;
				}
				case exact:
				{
					if (iCandidateValue != iRequestValue)
					{
						return EGL_FALSE;
					}

					break;
				}
				case mask:
				{
					if ((iCandidateValue & iRequestValue) != iRequestValue)
					{
						return EGL_FALSE;
					}

					break;
				}
				default:
				{
					PVR_DPF((PVR_DBG_ERROR,"CFG_Match: Unknown comparison %d",*pui32AttribCriteria));
					
					/* PVR_ASSERT(0) might generate a warning for constant expression */
					PVR_ASSERT(pui32AttribCriteria != pui32AttribCriteria);
				}
			}
		}

		pnAttrib+=2;			/* Next attrib, skipping over default value */
		pui32AttribCriteria++;	/* Next comparison method */
	}
	while(pnAttrib < pnAttibEnd);

	return EGL_TRUE;
}


/*
   <function>
   FUNCTION   : CFG_PrepareConfigFilter
   PURPOSE    :

   Prepare a requested configuration. Creates a configuration with
   default values assigned to attributes as defined in the egl
   specification.  Attribute values provided in an attribute list are
   then inserted into the configuration overwriting the default
   values. Finally requested attribute values which cannot be modified
   by the attribute list are fixed up.  
   
   PARAMETERS : pAttribList - attribute list
			  : nAttribCount - attribute count
   RETURNS    : requested configuration or IMG_NULL.
   </function>
 */
IMG_INTERNAL KEGL_CONFIG * CFG_PrepareConfigFilter(const EGLint *pAttribList, EGLint nAttribCount)
{
	KEGL_CONFIG *pDfltCfg;
	KEGL_CONFIG *pCopyCfg;
	KEGL_CONFIG *pRqstCfg;
	
	pDfltCfg = CFGC_CreateAvArray(aAttribDflts, ATTRIB_COUNT(aAttribDflts));

	if(pAttribList == IMG_NULL)
	{
		pCopyCfg = pDfltCfg;
	}
	else
	{
		pCopyCfg = CFGC_ModifyAvArrayNl(pDfltCfg, pAttribList, nAttribCount);
	}

	pRqstCfg = CFGC_CopyNl(pCopyCfg);

	if (pRqstCfg==IMG_NULL)
	{
		return IMG_NULL;
	}

	/* Fixup the entries to enforce the specification. */
	if (!CFGC_SetAttrib(pRqstCfg, EGL_MAX_PBUFFER_WIDTH, EGL_DONT_CARE))
	{
		goto failed;
	}

	if (!CFGC_SetAttrib(pRqstCfg, EGL_MAX_PBUFFER_HEIGHT, EGL_DONT_CARE))
	{
		goto failed;
	}

	if (!CFGC_SetAttrib(pRqstCfg, EGL_MAX_PBUFFER_PIXELS, EGL_DONT_CARE))
	{
		goto failed;
	}

	if (!CFGC_SetAttrib(pRqstCfg, EGL_NATIVE_VISUAL_ID, EGL_DONT_CARE))
	{
		goto failed;
	}

	if ((CFGC_GetAttrib(pRqstCfg, EGL_SURFACE_TYPE) & EGL_WINDOW_BIT)==0)
	{
		if (!CFGC_SetAttrib(pRqstCfg, EGL_NATIVE_VISUAL_TYPE, EGL_DONT_CARE))
		{
			goto failed;
		}
	}

	if (CFGC_GetAttrib(pRqstCfg, EGL_TRANSPARENT_TYPE)==EGL_NONE)
	{
		if (!CFGC_SetAttrib(pRqstCfg,EGL_TRANSPARENT_RED_VALUE, EGL_DONT_CARE))
		{
			goto failed;
		}

		if (!CFGC_SetAttrib(pRqstCfg, EGL_TRANSPARENT_GREEN_VALUE,	EGL_DONT_CARE))
		{
			goto failed;
		}

		if (!CFGC_SetAttrib(pRqstCfg, EGL_TRANSPARENT_BLUE_VALUE, EGL_DONT_CARE))
		{
			goto failed;
		}
	}

	/* If config ID has been specified in the attrib list, 
	   everything else becomes don't care. 
	*/
	if(CFGC_GetAttrib(pCopyCfg, EGL_CONFIG_ID)!=EGL_DONT_CARE)
	{
		EGLint *pnAttrib;
		EGLint *pnAttibEnd;

		pnAttrib = (EGLint *)aAttribDflts;
		pnAttibEnd = pnAttrib + ATTRIB_COUNT(aAttribDflts);

		/* The last entry should be the EGL_NONE list terminator */
		PVR_ASSERT(*pnAttibEnd == EGL_NONE);

		do
		{
			/* 
				The k-egl specification states that EGL_LEVEL may not
				be set to don't care
			*/
			if(*pnAttrib == EGL_LEVEL)
			{
				/* Pass in the default value for EGL_LEVEL */
				if (!CFGC_SetAttrib(pRqstCfg, EGL_LEVEL, *(pnAttrib + 1)))
				{
					goto failed;
				}
			}
			else if(*pnAttrib != EGL_CONFIG_ID)
			{
				if (!CFGC_SetAttrib(pRqstCfg, *pnAttrib, EGL_DONT_CARE))
				{
					goto failed;
				}
			}
	
			pnAttrib+=2; /* Next attrib, skipping over default value */

		}
		while(pnAttrib < pnAttibEnd);
	}

	return pRqstCfg;

failed:

	CFGC_Unlink(pRqstCfg);

	return IMG_NULL;
}


/*
   <function>
   FUNCTION   : CFG_CompatibleConfigs
   PURPOSE    :

   Test if two configs are compatible, based on definition of
   compatible from k-egl specification section 2.2. A context and a
   *surface* are considered compatible if they:
   
	 - have colour and ancillary buffers of the same depth
	 - were created with respect to the same display.

   PARAMETERS : In:  pCfgA - config
				In:  pCfgB - config
				In:  bOpenVGCheck - Should we only check ancillary buffers related to OpenVG
   RETURNS    : EGL_TRUE - Compatible.
				EGL_FALSE - Not compatible.
   </function>
 */
IMG_INTERNAL EGLBoolean CFG_CompatibleConfigs(const KEGL_CONFIG *pCfgA, const KEGL_CONFIG *pCfgB, IMG_BOOL bOpenVGCheck)
{
	EGLint *pnAttrib;
	EGLint *pnAttibEnd;

	PVR_ASSERT(pCfgA!=IMG_NULL);
	PVR_ASSERT(pCfgB!=IMG_NULL);
  
	/* Point to start and end of array, pnAttrib becomes our current pointer */
	pnAttrib = (EGLint *)aAttribCheckList;

	if(bOpenVGCheck)
	{
		pnAttibEnd = pnAttrib + ATTRIB_CHECKLIST_SIZE_VG;
	}
	else
	{
		pnAttibEnd = pnAttrib + ATTRIB_CHECKLIST_SIZE_FULL;
	}
	

	do
	{
		EGLint nTempAttrib = *pnAttrib;

		if (CFGC_GetAttrib(pCfgA, nTempAttrib) != CFGC_GetAttrib(pCfgB, nTempAttrib))
		{
			return EGL_FALSE;
		}
	
		pnAttrib++;
	}
	while(pnAttrib < pnAttibEnd);

	return EGL_TRUE;
}


/*
   <function>
   FUNCTION   : CFG_Compare
   PURPOSE    : Determine a sort order between two configurations. This
				function is used as a callback to qsort_s.
   PARAMETERS : In:  pA - 1st configuration
				In:  pB - 2nd configuration
				In:  pState - the values array used to track the selection
					 criteria applied to each configuration.
   RETURNS    : 
	 0 :: a == b
	<0 :: a < b
	>0 :: a > b
   </function>
 */
IMG_INTERNAL IMG_INT  CFG_Compare(void *pA, void *pB, void *pState)
{
	const KEGL_CONFIG *pCfgA = *(KEGL_CONFIG **)pA;
	const KEGL_CONFIG *pCfgB = *(KEGL_CONFIG **)pB;
	const KEGL_CONFIG *pRqstCfg = pState;
	EGLint nColourResult;
	EGLint nTempAttribA;
	EGLint nTempAttribB;

	nTempAttribA = CFGC_GetAttrib(pCfgA, EGL_CONFIG_CAVEAT);
	nTempAttribB = CFGC_GetAttrib(pCfgB, EGL_CONFIG_CAVEAT);
  
	if (nTempAttribA != nTempAttribB)
	{
		return (_CaveatOrder(nTempAttribA) - _CaveatOrder(nTempAttribB));
	}

	nColourResult = _CompareColourBits(pCfgA, pCfgB, pRqstCfg);

	if (nColourResult)
	{
		return nColourResult;
	}
  
	nTempAttribA = CFGC_GetAttrib(pCfgA, EGL_BUFFER_SIZE);
	nTempAttribB = CFGC_GetAttrib(pCfgB, EGL_BUFFER_SIZE);
	if (nTempAttribA != nTempAttribB)
	{
		return (nTempAttribA - nTempAttribB);
	}
  
	nTempAttribA = CFGC_GetAttrib(pCfgA, EGL_SAMPLE_BUFFERS);
	nTempAttribB = CFGC_GetAttrib(pCfgB, EGL_SAMPLE_BUFFERS);
	if (nTempAttribA != nTempAttribB)
	{
		return (nTempAttribA - nTempAttribB);
	}

	nTempAttribA = CFGC_GetAttrib(pCfgA, EGL_SAMPLES);
	nTempAttribB = CFGC_GetAttrib(pCfgB, EGL_SAMPLES);
	if (nTempAttribA != nTempAttribB)
	{
		return (nTempAttribA - nTempAttribB);
	}

	nTempAttribA = CFGC_GetAttrib(pCfgA, EGL_DEPTH_SIZE);
	nTempAttribB = CFGC_GetAttrib(pCfgB, EGL_DEPTH_SIZE);
	if (nTempAttribA != nTempAttribB)
	{
		return (nTempAttribA - nTempAttribB);
	}

	nTempAttribA = CFGC_GetAttrib(pCfgA, EGL_STENCIL_SIZE);
	nTempAttribB = CFGC_GetAttrib(pCfgB, EGL_STENCIL_SIZE);
	if (nTempAttribA != nTempAttribB)
	{
		return (nTempAttribA - nTempAttribB);
	}


	nTempAttribA = CFGC_GetAttrib(pCfgA, EGL_CONFIG_ID);
	nTempAttribB = CFGC_GetAttrib(pCfgB, EGL_CONFIG_ID);

	return (nTempAttribA - nTempAttribB);
}

/******************************************************************************
 End of file (cfg.c)
******************************************************************************/
