/******************************************************************************
 * Name         : cfg.h
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
 * $Log: cfg.h $
 * ./
 *  --- Revision Logs Removed --- 
 *
 *  --- Revision Logs Removed --- 
 ********************************************************************************/

#ifndef _CFG_H_
#define _CFG_H_

#include "drvegl.h"


#if defined (__cplusplus)
extern "C" {
#endif

/*
   <function>
   FUNCTION   : CFG_Initialize
   PURPOSE    : Initializes configuration information (number of supported config variants) to EGL global data
   </function>
 */
void CFG_Initialise(void);

/*
   <function>
   FUNCTION   : CFG_Variants
   PURPOSE    :

   Determine the number of configurations supported for a specified
   display.  This function queries the window system interface to
   determine the number of configurations variations that can be
   generated from the base configurations defined by the generic EGL
   layer.
   
   PARAMETERS : In:  pDpy - Display.
   RETURNS    : Number of configurations.
   </function>
 */
EGLint CFG_Variants(KEGL_DISPLAY *pDpy);

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

   PARAMETERS : In:  pDpy - Display
                In:  eglCfg - Configuration required.
                Out: ppCfg - Receives the generated configuration.
   RETURNS : EGL_SUCCESS - Success
             Other EGL error code - failure.
</function>
 */
EGLint CFG_GenerateVariant(KEGL_DISPLAY *pDpy, KEGL_CONFIG_INDEX eglCfg, KEGL_CONFIG **ppCfg);

/*
   <function>
   FUNCTION   : CFG_Match
   PURPOSE    : Determine if a candidate config meets the selection criteria
                provided in a requested config.
   PARAMETERS : In: requested_cfg - The requested configuration.
                In: candidate - The candidate configuration.
   RETURNS    : EGL_TRUE - candiate matches the selection criteria
                EGL_FALSE - candiate does not match the selection criteria
   </function>
 */
EGLBoolean CFG_Match(const KEGL_CONFIG *requested_cfg, const KEGL_CONFIG *candidate);

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

   PARAMETERS : In: pAttribList - attribute list
                In: nAttribCount - number of attribs (identifiers + values)
   RETURNS    : requested configuration or NULL.
   </function>
 */
KEGL_CONFIG *CFG_PrepareConfigFilter(const EGLint *pAttribList, EGLint nAttribCount);

/*
   <function>
   FUNCTION   : CFG_CompatibleConfigs
   PURPOSE    :

   Test if two configs are compatible, based on definition of
   compatible from k-egl specification section 2.2. A context and a
   *surface* are considered compatible if they:
   
     - have colour and ancillary buffers of the same depth
     - were created with respect to the same display.

   PARAMETERS : In:  pCa - config
                In:  pCb - config
                In:  bOpenVGCheck - Should we only check ancillary buffers related to OpenVG
   RETURNS    : EGL_TRUE - Compatible.
                EGL_FALSE - Not compatible.
   </function>
 */
EGLBoolean CFG_CompatibleConfigs(const KEGL_CONFIG *pCa, const KEGL_CONFIG *pCb, IMG_BOOL bOpenVGCheck);

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
IMG_INT CFG_Compare(void *pA, void *pB, void *pState);

#if defined (__cplusplus)
};
#endif

#endif

/******************************************************************************
 End of file (cfg.h)
******************************************************************************/

