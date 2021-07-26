/**************************************************************************
 * Name         : lighting.h
 * Author       : BCB
 * Created      : 02/05/2003
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
 * $Log: lighting.h $
 **************************************************************************/
#ifndef _LIGHTING_
#define _LIGHTING_

#define GLES1_LIGHT_TYPE_INFINITE	0
#define GLES1_LIGHT_TYPE_POINT		1
#define GLES1_LIGHT_TYPE_SPOT		2
#define GLES1_LIGHT_TYPE_MAX			3

struct GLESLightSourceMachineRec {
  
	GLESLightSourceState *psState;
    
	/* This will be set to spot when the cut off angle != 180 */
    IMG_UINT32 ui32LightType;

    /* Unit vector VPpli pre-computed (only when light is at infinity) */
    GLEScoord sUnitVPpli;
    
	/* When possible, the normalized "h" value from the spec is pre-computed */
    GLEScoord sHHat;
    
	/* Link to next active light */
    GLESLightSourceMachine *psNext;

    /* Cosine of the spot light cutoff angle */
    IMG_FLOAT fCosCutOffAngle;

	IMG_UINT32 ui32LightNum;
};

typedef struct {

	IMG_UINT32 aui32NumEnabled[GLES1_LIGHT_TYPE_MAX];

	/* Pointer to an array of light source machines */
    GLESLightSourceMachine *psSource;

	/* List of enabled light sources */
    GLESLightSourceMachine *psSources;
	
	/* Scene color = material emissive color + material ambient color * scene ambient color
    ** Will not be used if color-material is enabled.
    ** This sum is carefully kept scaled.
    */
    GLEScolor sSceneColor;

} GLESLightMachine;

#endif /* _LIGHTING_ */

