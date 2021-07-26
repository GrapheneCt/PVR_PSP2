
/**************************************************************************
 * Name         : cfg-core.c
 * Author       : Marcus Shawcroft
 * Created      : 04/11/2003
 *
 * Copyright    : 2003-2005 by Imagination Technologies Limited. All rights reserved.
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
 * $Date: 2011/02/14 15:26:34 $ $Revision: 1.13 $
 * $Log: cfg_core.c $
 *
 *  --- Revision Logs Removed --- 
 **************************************************************************/

#include "egl_internal.h"
#include "cfg_core.h"
#include "drvegl.h"


#define CFGC_GROW 8

#if defined (DEBUG)
static IMG_UINT32 _CountAttribs(const EGLint *pV)
{
	IMG_UINT32 ui32AttribCount;

	for (ui32AttribCount=0; pV[ui32AttribCount] != EGL_NONE; ui32AttribCount+=2);

	return ui32AttribCount;
}
#endif /* DEBUG */

/*
   <function>
   FUNCTION   : CFGC_Create
   PURPOSE    : Create an empty configuration.
   PARAMETERS : None
   RETURNS    : Configuration.
   </function>
 */
IMG_INTERNAL KEGL_CONFIG *CFGC_Create(void)
{
	KEGL_CONFIG *pCfg;

	pCfg = EGLCalloc(sizeof(*pCfg));

	if (pCfg!=IMG_NULL)
	{
		pCfg->uReferenceCount	= 1;
		pCfg->bDeallocRequired	= IMG_TRUE;

		PVR_ASSERT(pCfg->uAttribSize	== 0);
		PVR_ASSERT(pCfg->uAttribCount	== 0);
		PVR_ASSERT(pCfg->pAttribArray	== IMG_NULL);
		PVR_ASSERT(pCfg->pNext			== IMG_NULL);
		PVR_ASSERT(pCfg->eWSEGLPixelFormat == WSEGL_PIXELFORMAT_RGB565); /* 0 */
		PVR_ASSERT(pCfg->ePVRPixelFormat   == PVRSRV_PIXEL_FORMAT_UNKNOWN); /* 0 */
	}

	return pCfg;
}


/*
   <function>
   FUNCTION   : CFGC_CreateAvArray
   PURPOSE    :

   Create a configuration with attributes and values defined in a
   constant attribue value array. The created configuration maintains
   a reference to the attribute value array, but the contents of the
   array are never modified and are not deallocated by the cfg module.
   The created configuration has one reference.
   
   PARAMETERS : In: pV -

   Attribute value array. A sequence of attribute value pairs
   terminated with an EGL_NONE attribute.
                In:  ui32AttribCount - number of attribs & values
   
   RETURNS    : Configuration.
   </function>
 */
IMG_INTERNAL KEGL_CONFIG *CFGC_CreateAvArray(const EGLint *pV, IMG_UINT32 ui32AttribCount)
{
	KEGL_CONFIG *pCfg;

	pCfg = CFGC_Create ();

	if (pCfg!=IMG_NULL)
	{
		pCfg->bDeallocRequired = IMG_FALSE;

		pCfg->uAttribCount = ui32AttribCount;

		pCfg->pAttribArray = (EGLint *)pV;

#if defined(DEBUG)
		PVR_ASSERT(_CountAttribs(pV) == ui32AttribCount);
#endif
	}

	return pCfg;
}


/*
   <function>
   FUNCTION   : CFGC_ModifyAvArrayNl
   PURPOSE    :

   Derive a new configuration from an old configuration redefining the
   attribute values specified in an attribute value array. The
   original configuration is not modified. The attribute value array
   is never modified and is not deallocated by the cfg module.
   The created configuration has one reference. An extra reference is
   not taken on the base configuration.
   
   PARAMETERS : In:  pBaseCfg - Configuration to derive from.
                In:  pV -

   Attribute value array. A sequence of attribute value pairs
   terminated with an EGL_NONE attribute.
                In:  ui32AttribCount - number of attribs & values

   RETURNS    : Configuration
   </function>
 */
IMG_INTERNAL KEGL_CONFIG *CFGC_ModifyAvArrayNl(KEGL_CONFIG *pBaseCfg,
											   const EGLint *pV,
											   IMG_UINT32 ui32AttribCount)
{
	KEGL_CONFIG *pCfg;

	if (pBaseCfg==IMG_NULL)
	{
		return IMG_NULL;
	}

	pCfg = CFGC_Create ();

	if (pCfg == IMG_NULL)
	{
		CFGC_Unlink (pBaseCfg);

		return IMG_NULL;
	}
  
	pCfg->pNext = pBaseCfg;

	pCfg->bDeallocRequired = IMG_FALSE;

	pCfg->uAttribCount = ui32AttribCount;

	pCfg->pAttribArray = (EGLint *) pV;

#if defined(DEBUG)
	PVR_ASSERT(_CountAttribs(pV) == ui32AttribCount);
#endif

	return pCfg;
}


/*
   <function>
   FUNCTION   : CFGC_CopyNl
   PURPOSE    : Create a copy of a configuration. The new configuration is
                derived from the base configuration but does not initially
                override any attribute values.
   PARAMETERS : In:  pCfg - configuration
   RETURNS    : New configuration.
   </function>
 */
IMG_INTERNAL KEGL_CONFIG *CFGC_CopyNl(KEGL_CONFIG *pCfg)
{
	KEGL_CONFIG *pCfgCopy;

	if (pCfg==IMG_NULL)
	{
		return IMG_NULL;
	}

	pCfgCopy = CFGC_Create ();

	if (pCfgCopy==IMG_NULL)
	{
		return IMG_NULL;
	}

	pCfgCopy->pNext = pCfg;

	return pCfgCopy;
}


/*
   <function>
   FUNCTION   : CFGC_Unlink
   PURPOSE    :

   Unlink a configuration. Decremements the configurations reference
   count. Once the reference count hits zero the configuration is
   deleted and the base configuration if any is unlinked recursively.
   
   PARAMETERS : In:  pCfg - Configuration to unlink.
   RETURNS    : None
   </function>
 */
IMG_INTERNAL void CFGC_Unlink(KEGL_CONFIG *pCfg)
{
	if (pCfg!=IMG_NULL)
	{
		if (--pCfg->uReferenceCount == 0)
		{
			CFGC_Unlink (pCfg->pNext);

			if (pCfg->pAttribArray!=IMG_NULL && pCfg->bDeallocRequired)
			{
				EGLFree(pCfg->pAttribArray);
			}

			EGLFree(pCfg);
		}
	}
}


/*
   <function>
   FUNCTION   : CFGC_SetAttrib
   PURPOSE    :

   Change the value of an attribute in a configuration. This will also
   modify the value of the attribute in all configurations derived
   from the specified configuration that do not redefine the attribute
   explicitly.

   PARAMETERS : In:  pCfg - Configuration to modify.
                In:  iAttrib - Attribute to change.
                In:  iValue - New attribute value.
   RETURNS    : EGL_TRUE - Success.
                EGL_FALSE - Failure
   </function>
 */
IMG_INTERNAL EGLBoolean CFGC_SetAttrib(KEGL_CONFIG *pCfg, EGLint iAttrib, EGLint iValue)
{
	EGLint *pnAttrib;
	IMG_UINT32 ui32AttribCount;

	if ((pCfg==IMG_NULL) || !pCfg->bDeallocRequired)
	{
		return EGL_FALSE;
	}

	/* Read the count, it stays constant but the compiler doesn't know that */
	ui32AttribCount = pCfg->uAttribCount;

	if (pCfg->uAttribSize <= ui32AttribCount + 2)
	{
		EGLint *av;
		IMG_UINT32 ui32Size = pCfg->uAttribSize + CFGC_GROW;

		av = EGLRealloc(pCfg->pAttribArray, sizeof(*av) * ui32Size);

		if (av==IMG_NULL)
		{
			return EGL_FALSE;
		}

		pCfg->pAttribArray = av;
		pCfg->uAttribSize = ui32Size;
	}
  
	/* Pointer to the end of the attrib array */
	pnAttrib = pCfg->pAttribArray + ui32AttribCount;

	/* Append this attribute and its value... */
	pnAttrib[0] = iAttrib;
	pnAttrib[1] = iValue;

	/* ...and update the count */
	pCfg->uAttribCount = ui32AttribCount + 2;

	return EGL_TRUE;
}


/*
   <function>
   FUNCTION   : CFGC_GetPixelFormat
   PURPOSE    : Return PVR pixel format of the configuration
   PARAMETERS : In:  pCfg - configuration
   RETURNS    : PVR Pixel format
   </function>
 */
IMG_INTERNAL PVRSRV_PIXEL_FORMAT CFGC_GetPixelFormat(KEGL_CONFIG *pCfg)
{
	while(pCfg)
	{
		if(pCfg->ePVRPixelFormat != PVRSRV_PIXEL_FORMAT_UNKNOWN)
		{
			return pCfg->ePVRPixelFormat;
		}
		pCfg = pCfg->pNext;
	}
	return PVRSRV_PIXEL_FORMAT_UNKNOWN;
}


/*
   <function>
   FUNCTION   : CFGC_GetAttrib
   PURPOSE    : Query the value of an attribute in a configuration.
   PARAMETERS : In:  pCfg - Configuration to query.
				In:  iAttrib - Attribute to query.
   RETURNS    : Queried attribute value.
   </function>
 */
IMG_INTERNAL EGLint CFGC_GetAttrib(const KEGL_CONFIG *pCfg, EGLint iAttrib)
{
	EGLint iReturnValue = 0;
	EGLint bFoundAttrib = EGL_FALSE;

	while(pCfg)
	{
		EGLint* pArrayWalker = pCfg->pAttribArray;
		EGLint* pArrayEnd = pArrayWalker + pCfg->uAttribCount;
		/* uAttribCount is the size of the array */

		while(pArrayWalker < pArrayEnd)
		{
			if (*pArrayWalker == iAttrib)
			{
				/* 
					If iAttrib is a bit field (e.g., EGL RENDERABLE TYPE, EGL SURFACE TYPE),
					accumulate it.
					Ideally, this would check a attribute type rather than a fixed list of 
					"mask" type attribs 
				*/

				switch(iAttrib)
				{
					case EGL_RENDERABLE_TYPE:
					case EGL_SURFACE_TYPE:
					{
						iReturnValue |= *(pArrayWalker + 1);
						bFoundAttrib = EGL_TRUE;
						break;
					}
					
					default:
					{
						return *(pArrayWalker + 1);
					}
				}
			}
			pArrayWalker += 2;
		}

		pCfg=pCfg->pNext;
	}
	
	if (bFoundAttrib)
	{
		return iReturnValue;
	}
	
	return EGL_DONT_CARE;
}


/*
   <function>
   FUNCTION   : CFGC_GetAttrib
   PURPOSE    : Query the value of an attribute in a configuration.
   PARAMETERS : In:  pCfg - Configuration to query.
                In:  iAttrib - Attribute to query.
   RETURNS    : Queried attribute value.
   </function>
 */
IMG_INTERNAL EGLint CFGC_GetAttribNoAccumulate(const KEGL_CONFIG *pCfg, EGLint iAttrib)
{
	while(pCfg)
	{
		EGLint* pArrayWalker = pCfg->pAttribArray;
		EGLint* pArrayEnd = pArrayWalker + pCfg->uAttribCount;
		/* uAttribCount is the size of the array */

		while(pArrayWalker < pArrayEnd)
		{
			if (*pArrayWalker == iAttrib)
			{
				return *(pArrayWalker + 1);
			}
			pArrayWalker += 2;
		}

		pCfg=pCfg->pNext;
	}
	   
	return EGL_DONT_CARE;
}


/*
   <function>
   FUNCTION   : CFGC_GetAttribAccumulate
   PURPOSE    : Query the value of an attribute in a configuration.
   PARAMETERS : In:  pCfg - Configuration to query.
				In:  iAttrib - Attribute to query.
   RETURNS    : Queried attribute value.
   </function>
 */
IMG_INTERNAL EGLint CFGC_GetAttribAccumulate(const KEGL_CONFIG *pCfg, EGLint iAttrib)
{
	EGLint iReturnValue = 0;
	EGLint bFoundAttrib = EGL_FALSE;

	while(pCfg)
	{
		EGLint* pArrayWalker = pCfg->pAttribArray;
		EGLint* pArrayEnd = pArrayWalker + pCfg->uAttribCount;
		/* uAttribCount is the size of the array */

		while(pArrayWalker < pArrayEnd)
		{
			if (*pArrayWalker == iAttrib)
			{
				iReturnValue |= *(pArrayWalker + 1);
				bFoundAttrib = EGL_TRUE;
				break;
			}
			pArrayWalker += 2;
		}

		pCfg=pCfg->pNext;
	}
	
	if (bFoundAttrib)
	{
		return iReturnValue;
	}
	
	return EGL_DONT_CARE;
}

/******************************************************************************
 End of file (cfg_core.c)
******************************************************************************/


