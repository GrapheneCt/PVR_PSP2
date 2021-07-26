/* -*- c-file-style: "img" -*-
<module>
 * Name         : cfg-core.h
 * Title        : EGL Config Infrastructure.
 * Author       : Marcus Shawcroft
 * Created      : 4 Nov 2003
 *
 * Copyright    : 2003-2005 by Imagination Technologies Limited.
 *                All rights reserved.  No part of this software, either
 *                material or conceptual may be copied or distributed,
 *                transmitted, transcribed, stored in a retrieval system
 *                or translated into any human or computer language in any
 *                form by any means, electronic, mechanical, manual or
 *                other-wise, or disclosed to third parties without the
 *                express written permission of Imagination Technologies
 *                Limited, Unit 8, HomePark Industrial Estate,
 *                King's Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * Description  :
 *
 * Config Infrastructure API. Provides mechanisms to represent and
 * manipulate EGL configurations.
 *
 * Configurations are created by deriving new configurations from
 * existing configurations. Attribute values are inherited from base
 * configuration to derivied configuration. A configuration may
 * overload an inherited attribute.
 *
 * Memory management is implemented by reference counting. A
 * configuration is initially created with a single reference, the
 * creator is responsible for calling the CFGC_Delete to discard
 * that reference. Memory is deallocated at the point that a
 * configuration has no references.
 *
 * Functions that derive a new configuration from an old configuration
 * *generally* take an additional reference on the base
 * configuration. HOWEVER there are exceptions to this rule for the
 * benefit of common usage patterns. The *Nl functions (Nl means no
 * link) do not increment the base configuration reference count. They
 * effectively consume the reference supplied to them.
 *  
 * Platform : ALL
 *
 * $Log: cfg_core.h $
 * .,
 *  --- Revision Logs Removed --- 
 *
 *  --- Revision Logs Removed --- 
 */

#ifndef _CFG_CORE_H_
#define _CFG_CORE_H_

#include "drvegl.h"
#include "wsegl.h"
#include "servicesext.h"

typedef struct KEGL_CONFIG_TAG
{
    IMG_UINT32	uReferenceCount;
    IMG_BOOL  	bDeallocRequired;
    IMG_UINT32	uAttribSize;
    IMG_UINT32	uAttribCount;
    EGLint	  	*pAttribArray;

    WSEGLPixelFormat       eWSEGLPixelFormat;
    PVRSRV_PIXEL_FORMAT    ePVRPixelFormat;

    struct KEGL_CONFIG_TAG *pNext;

} KEGL_CONFIG;


#if defined (__cplusplus)
extern "C" {
#endif

/*
   <function>
   FUNCTION   : CFGC_GetPixelFormat
   PURPOSE    : Return PVR pixel format of the configuration
   PARAMETERS : In:  pCfg - configuration
   RETURNS    : PVR Pixel format
   </function>
 */
PVRSRV_PIXEL_FORMAT CFGC_GetPixelFormat(KEGL_CONFIG *pCfg);

/*
   <function>
   FUNCTION   : CFGC_Create
   PURPOSE    : Create an empty configuration.
   PARAMETERS : None
   RETURNS    : Configuration.
   </function>
 */
KEGL_CONFIG *CFGC_Create(void);

/*
   <function>
   FUNCTION   : CFGC_CreateAvArray
   PURPOSE    :

   Create a configuration with attributes and values defined in a
   constant attribue value array. The created configuration maintains
   a reference to the attribute value array, but the contents of the
   array are never modified and are not deallocated by the cfg module.
   The created configuration has one reference.
   
   PARAMETERS : In: - pV :

   Attribute value array. A sequence of attribute value pairs
   terminated with an EGL_NONE attribute.
   
   RETURNS    : Configuration.
   </function>
 */

KEGL_CONFIG *CFGC_CreateAvArray(const EGLint *pV, IMG_UINT32 ui32AttribCount);

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

   RETURNS    : Configuration
   </function>
 */
KEGL_CONFIG *CFGC_ModifyAvArrayNl(KEGL_CONFIG *pBaseCfg, const EGLint *pV, IMG_UINT32 ui32AttribCount);


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
KEGL_CONFIG *CFGC_CopyNl(KEGL_CONFIG *pCfg);

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
void CFGC_Unlink(KEGL_CONFIG *pCfg);

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
EGLBoolean CFGC_SetAttrib(KEGL_CONFIG *pCfg, EGLint iAttrib, EGLint iValue);

/*
   <function>
   FUNCTION   : CFGC_GetAttrib
   PURPOSE    : Query the value of an attribute in a configuration.
   PARAMETERS : In:  pCfg - Configuration to query.
                In:  iAttrib - Attribute to query.
   RETURNS    : Queried attribute value.
   </function>
 */
EGLint CFGC_GetAttrib(const KEGL_CONFIG *pCfg, EGLint iAttrib);
EGLint CFGC_GetAttribNoAccumulate(const KEGL_CONFIG *pCfg, EGLint iAttrib);
EGLint CFGC_GetAttribAccumulate(const KEGL_CONFIG *pCfg, EGLint iAttrib);

/* Attrib count = array elements minus 1 for the EGL_NONE terminator */
#define ATTRIB_COUNT(aAvArrayName) (sizeof(aAvArrayName)/sizeof(EGLint) - 1)

#if defined (__cplusplus)
}
#endif

#endif
