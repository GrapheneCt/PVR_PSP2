/******************************************************************************
 * Name         : reginfo.h
 * Author       :
 * Created      : 01-25-2007
 *
 * Copyright    : 2000-2007 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited,
 *              : Home Park Estate, Kings Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * Modifications:-
 * $Log: reginfo.h $
 *****************************************************************************/

#ifndef __gl_regdefs_h_
#define __gl_regdefs_h_

/* Reg size and layout information for binding symbols (built-in constant variables) */

#define FFTNL_SIZE_TEXGENPLANE							 4
#define FFTNL_SIZE_CLIPPLANE							 4
#define FFTNL_SIZE_MATRIX_4X4							16
#define FFTNL_SIZE_MATRIX_4X3							12

#define FFTNL_OFFSETS_POINTPARAMS_SIZE					 0
#define FFTNL_OFFSETS_POINTPARAMS_MIN					 1
#define FFTNL_OFFSETS_POINTPARAMS_MAX					 2
#define FFTNL_OFFSETS_POINTPARAMS_FADETHRESH			 3
#define FFTNL_OFFSETS_POINTPARAMS_CONATTEN				 4
#define FFTNL_OFFSETS_POINTPARAMS_LINATTEN				 5
#define FFTNL_OFFSETS_POINTPARAMS_QUADATTEN				 6
#if defined(FFGEN_UNIFLEX)
#define FFTNL_SIZE_POINTPARAMS							 8
#else
#define FFTNL_SIZE_POINTPARAMS							 7
#endif
#define FFTNL_NUM_MEMBERS_POINTPARAMS					 7


#define FFTNL_OFFSETS_LIGHTMODEL_AMBIENT				 0
#define FFTNL_SIZE_LIGHTTMODEL							 4
#define FFTNL_NUM_MEMBERS_LIGHTMODEL					 1

#define FFTNL_OFFSETS_LIGHTMODELPRODUCT_SCENECOL		 0
#define FFTNL_SIZE_LIGHTTMODELPRODUCT					 4
#define FFTNL_NUM_MEMBERS_LIGHTMODELPRODUCT				 1

#define FFTNL_OFFSETS_LIGHTPRODUCT_AMBIENT				 0
#define FFTNL_OFFSETS_LIGHTPRODUCT_DIFFUSE				 4
#define FFTNL_OFFSETS_LIGHTPRODUCT_SPECULAR				 8
#define FFTNL_SIZE_LIGHTPRODUCT							12
#define FFTNL_NUM_MEMBERS_LIGHTPRODUCT					 3

#define FFTNL_OFFSETS_MATERIAL_EMISSION					 0
#define FFTNL_OFFSETS_MATERIAL_AMBIENT					 4
#define FFTNL_OFFSETS_MATERIAL_DIFFUSE					 8
#define FFTNL_OFFSETS_MATERIAL_SPECULAR					12
#define FFTNL_OFFSETS_MATERIAL_SHININESS				16
#if defined(FFGEN_UNIFLEX)
#define FFTNL_SIZE_MATERIAL								20
#else
#define FFTNL_SIZE_MATERIAL								17
#endif
#define FFTNL_NUM_MEMBERS_MATERIAL						 5


#define FFTNL_OFFSETS_LIGHTSOURCE_AMBIENT				 0
#define FFTNL_OFFSETS_LIGHTSOURCE_DIFFUSE				 4
#define FFTNL_OFFSETS_LIGHTSOURCE_SPECULAR				 8
#define FFTNL_OFFSETS_LIGHTSOURCE_POSITION				12
#define FFTNL_OFFSETS_LIGHTSOURCE_POSITION_NORMALISED	16
#define FFTNL_OFFSETS_LIGHTSOURCE_HALFVECTOR			20
#define FFTNL_OFFSETS_LIGHTSOURCE_SPOTDIRECTION			24
#define FFTNL_OFFSETS_LIGHTSOURCE_SPOTEXPONENT			27
#define FFTNL_OFFSETS_LIGHTSOURCE_CONSTANTATTENUATION	28 /* Attenuation consts are used together as a vector input to a DP3 operation, */
#define FFTNL_OFFSETS_LIGHTSOURCE_LINEARATTENUATION		29 /* so for FFGEN_UNIFLEX they must be at the beginning of a vector-4 boundary */
#define FFTNL_OFFSETS_LIGHTSOURCE_QUADRATICATTENUATION	30 /* */
#define FFTNL_OFFSETS_LIGHTSOURCE_SPOTCUTOFF			31
#define FFTNL_OFFSETS_LIGHTSOURCE_SPOTCOSCUTOFF			32

#if defined(FFGEN_UNIFLEX)
#define FFTNL_SIZE_LIGHTSOURCE							36
#else
#define FFTNL_SIZE_LIGHTSOURCE							33
#endif

#define FFTNL_NUM_MEMBERS_LIGHTSOURCE					13


/* Could use these sizes to load less data in some cases */
#define FFTNL_SIZE_LIGHTSOURCE_POINT_INFINITE				FFTNL_OFFSETS_LIGHTSOURCE_HALFVECTOR     + 4
#define FFTNL_SIZE_LIGHTSOURCE_SPOT_INFINITE				FFTNL_OFFSETS_LIGHTSOURCE_SPOTCOSCUTOFFF + 1
#define FFTNL_SIZE_LIGHTSOURCE_POINT_LOCAL					FFTNL_SIZE_LIGHTSOURCE
#define FFTNL_SIZE_LIGHTSOURCE_SPOT_LOCAL					FFTNL_SIZE_LIGHTSOURCE

#define FFTNL_NUM_TEXTURE_UNITS								8

#if defined(OGL_LINESTIPPLE)
/* For geometry shaders */
#define FFGEO_SIZE_LINEOFFSET		2
#define FFGEO_SIZE_LINEREPEAT		2
#endif						

extern const IMG_UINT32 auLightSourceMemberSizes[FFTNL_NUM_MEMBERS_LIGHTSOURCE];
extern const IMG_UINT32 auPointParamsMemberSizes[FFTNL_NUM_MEMBERS_POINTPARAMS];
extern const IMG_UINT32 auMaterialMemberSizes[FFTNL_NUM_MEMBERS_MATERIAL];
extern const IMG_UINT32 auLightProductMemberSizes[FFTNL_NUM_MEMBERS_LIGHTPRODUCT];
extern const IMG_UINT32 auLightModelMemberSizes[FFTNL_NUM_MEMBERS_LIGHTMODEL];
extern const IMG_UINT32 auLightModelProductMemberSizes[FFTNL_NUM_MEMBERS_LIGHTMODELPRODUCT];

#endif /* __gl_regdefs_h_ */

/******************************************************************************
 End of file (reginfo.h)
******************************************************************************/

