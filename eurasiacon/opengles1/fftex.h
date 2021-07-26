/**************************************************************************
 * Name         : fftex.h
 *
 * Copyright    : 2006 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or other-wise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited, Home Park
 *              : Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * $Log: fftex.h $
 *
 *  --- Revision Logs Removed --- 
 **************************************************************************/

#ifndef _FFTEX_H_
#define _FFTEX_H_

/****************************************************************************/


/* Defines for the fragment shader temporary registers used for FF texture blending */
#define FFTB_BLENDREG_CURRENT				0
#define FFTB_BLENDREG_TWOSIDED_PRIMARY		1

#define FFTB_BLENDREG_TEMP0					4
#define FFTB_BLENDREG_TEMP1					5
#define FFTB_BLENDREG_TEMP2					6
#define FFTB_BLENDREG_TEMP3					7

#define FFTB_BLENDREG_TEXTURE0				8
#define FFTB_BLENDREG_TEXTURE1				9
#define FFTB_BLENDREG_TEXTURE2				10
#define FFTB_BLENDREG_TEXTURE3				11
#define FFTB_BLENDREG_TEXTURE4				12
#define FFTB_BLENDREG_TEXTURE5				13
#define FFTB_BLENDREG_TEXTURE6				14
#define FFTB_BLENDREG_TEXTURE7				15
#define FFTB_BLENDREG_TEXTURE8				16
#define FFTB_BLENDREG_TEXTURE9				17
#define FFTB_BLENDREG_TEXTURE10				18

#define FFTB_MAX_TEXTURE_BINDINGS			 (3 * GLES1_MAX_TEXTURE_UNITS) 


/****************************************************************************/

/* Extra information needed to correctly construct fragment shaders */
#define	FFTB_BLENDSTATE_TWOSIDED_LIGHTING	0x00000001UL
#define FFTB_BLENDSTATE_NEEDS_BASE_COLOUR	0x00000002UL
#define	FFTB_BLENDSTATE_FRONTFACE_CW		0x00000004UL
#define FFTB_BLENDSTATE_NEEDS_ALL_LAYERS	0x00000008UL
#define FFTB_BLENDSTATE_ALPHATEST			0x00000010UL
#define FFTB_BLENDSTATE_ALPHATEST_FEEDBACK	0x00000020UL
#define FFTB_BLENDSTATE_FOG					0x00000040UL

typedef enum _FFTBBindingType_
{
	FFTB_BINDING_FACTOR_SCALAR		= 1,
	FFTB_BINDING_IMMEDIATE_SCALAR	= 2,

} FFTBBindingType;


typedef struct
{
	FFTBBindingType	eType;				/* Type of binding */
	IMG_UINT32		ui32Value;			/* Value for this binding, meaning is dependent on type */
	IMG_UINT32		ui32ConstantOffset;	/* Constant offset assigned to this binding */

} FFTBBindingInfo;


/* ************************************************************************************************************* */
/* ************************************************************************************************************* */
/* ************************************************************************************************************* */
/* ************************************************************************************************************* */
/* ************************************************************************************************************* */
/* ************************************************************************************************************* */
/* ************************************************************************************************************* */

/*
typedef enum
{
	GLES1_TEXTURE_COORDINATE_0		= 0,
	GLES1_TEXTURE_COORDINATE_1		= 1,
	GLES1_TEXTURE_COORDINATE_2		= 2,
	GLES1_TEXTURE_COORDINATE_3		= 3,
	GLES1_TEXTURE_COORDINATE_4		= 4,
	GLES1_TEXTURE_COORDINATE_5		= 5,
	GLES1_TEXTURE_COORDINATE_6		= 6,
	GLES1_TEXTURE_COORDINATE_7		= 7,
	GLES1_TEXTURE_COORDINATE_8		= 8,
	GLES1_TEXTURE_COORDINATE_9		= 9,
	GLES1_TEXTURE_COORDINATE_V0		= 10,
	GLES1_TEXTURE_COORDINATE_V1		= 11,
	GLES1_TEXTURE_COORDINATE_FOG		= 12,
	GLES1_TEXTURE_COORDINATE_POSITION	= 13,

} GLES1_TEXTURE_COORDINATE;
*/

#define GLES1_TEXTURE_COORDINATE_0				0
#define GLES1_TEXTURE_COORDINATE_1				1
#define GLES1_TEXTURE_COORDINATE_2				2
#define GLES1_TEXTURE_COORDINATE_3				3
#define GLES1_TEXTURE_COORDINATE_4				4
#define GLES1_TEXTURE_COORDINATE_5				5
#define GLES1_TEXTURE_COORDINATE_6				6
#define GLES1_TEXTURE_COORDINATE_7				7
#define GLES1_TEXTURE_COORDINATE_8				8
#define GLES1_TEXTURE_COORDINATE_9				9
#define GLES1_TEXTURE_COORDINATE_V0				10
#define GLES1_TEXTURE_COORDINATE_V1				11
#define GLES1_TEXTURE_COORDINATE_FOG			12
#define GLES1_TEXTURE_COORDINATE_POSITION		13

/*
	Iterated data formats, to indicate how input data iterated by the
	HW should be packed for use by the compiled program
*/
typedef enum
{
	GLES1_TEXLOAD_FORMAT_F32 = 0,

#if defined(FIX_HW_BRN_25211)
	GLES1_TEXLOAD_FORMAT_C10 = 2,
#endif /* defined(FIX_HW_BRN_25211) */

	GLES1_TEXLOAD_FORMAT_U8	 = 3		/* supercedes bIntegerColour */

} GLES1_TEXLOAD_FORMAT;

/*
	Data describing the texture or vertex data to be sampled or iterated
	by the HW to form input data for the compiled program
*/
typedef struct
{
	IMG_UINT32					ui32Texture;
	IMG_UINT32					eCoordinate;
	IMG_UINT32					ui32CoordinateDimension;
	IMG_BOOL					bProjected;
	GLES1_TEXLOAD_FORMAT		eFormat;

} GLES1_TEXTURE_LOAD;

/*
	Index used to indicate an iterated rather than pre-sampled 
	GLES1_TEXTURE_LOAD entry.
*/
#define GLES1_TEXTURE_NONE	(17)


/* ************************************************************************************************************* */
/* ************************************************************************************************************* */
/* ************************************************************************************************************* */
/* ************************************************************************************************************* */
/* ************************************************************************************************************* */
/* ************************************************************************************************************* */
/* ************************************************************************************************************* */

#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
/* 3 for each texture layer (could be 3 planar YUV) + 1 for 2-sided lighting */
#define GLES1_MAX_NON_DEPENDENT_LOADS	((GLES1_MAX_TEXTURE_UNITS * 3) + 1)
#else
/* 1 for each texture layer + 1 for 2-sided lighting */
#define GLES1_MAX_NON_DEPENDENT_LOADS	(GLES1_MAX_TEXTURE_UNITS + 1)
#endif

typedef struct FFTBProgramDesc_TAG
{
	/* Number of non-dependent texture loads and coordinate iterations required for the program */
	IMG_UINT32			ui32NumNonDependentTextureLoads;

	/* Details about each texture load/coordinate iteration  */
	GLES1_TEXTURE_LOAD	psNonDependentTextureLoads[GLES1_MAX_NON_DEPENDENT_LOADS];

	FFGEN_PROGRAM_DETAILS *psFFGENProgramDetails;

	GLESUSEASMInfo		sUSEASMInfo;

	IMG_UINT32			ui32NumBindings;

	FFTBBindingInfo		asBindings[FFTB_MAX_TEXTURE_BINDINGS]; /* 3 constant per unit (factor colour) */

	IMG_UINT32			ui32CurrentConstantOffset;

} FFTBProgramDesc;


/* The kind of texture that is being sampled and if 
   projection is required, as well as some format information  */
#define FFTB_TEXLOAD_2D					0x00000001	/* 2D texture */
#define FFTB_TEXLOAD_CEM				0x00000004	/* CEM texture */
#define FFTB_TEXLOAD_PROJECTED			0x00000008	/* Projected look-up */
#define FFTB_TEXLOAD_ALPHA8				0x00000010	/* 8-bit alpha texture */
#define FFTB_TEXLOAD_LUMINANCE8			0x00000020	/* 8-bit luminance texture */
#define FFTB_TEXLOAD_LUMINANCEALPHA8	0x00000040	/* 8-bit per component luminance-alpha texture */
#if defined(GLES1_EXTENSION_TEXTURE_STREAM) || defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
#define FFTB_TEXLOAD_YUVCSC				0x00000080	/* Native HW YUV 422, XYUV */
#define FFTB_TEXLOAD_YUVCSC_420_2PLANE	0x00000100	/* 2 Planar YUV 420, eg NV12 */
#define FFTB_TEXLOAD_YUVCSC_420_3PLANE	0x00000200	/* 3 Planar YUV 420, eg YV12, I420 */
#endif
#define FFTB_TEXLOAD_NEEDTEX			0x00000400	/* Needs a texture load */


#define FFTB_TEXUNPACK_ALPHA			0x00000001 /* Needs unpacking for alpha */
#define FFTB_TEXUNPACK_COLOR			0x00000002 /* Needs unpacking for color */

/* Description of a fixed-function texture blending layer */
typedef struct
{
	/* Texture blending operations, sources and colours */
	IMG_UINT32 ui32Op;
	IMG_UINT32 ui32ColorSrcs;
	IMG_UINT32 ui32AlphaSrcs;

	/* Texture load info */
	IMG_UINT32 ui32TexLoadInfo;

	/* Do we require an unpack instruction if the colour/alpha is referenced? */
	IMG_UINT32 ui32Unpack;

} FFTBBlendLayerDesc;


typedef enum
{
	FFTB_UNIFLEX_INSTRUCTIONS	= 0x00000001,
	FFTB_HW_INSTRUCTIONS		= 0x00000002,

} FFTBInstructionType;


/****************************************************************************/

IMG_VOID ValidateFFTextureConstants(GLES1Context *gc);

GLES1_MEMERROR ValidateFFTextureBlending(GLES1Context *gc);

IMG_VOID DestroyHashedBlendState(GLES1Context *gc, IMG_UINT32 ui32Item);

IMG_UINT32 AddFFTextureBinding(FFTBProgramDesc *psFFTBProgramDesc, FFTBBindingType eBindingType, IMG_UINT32 ui32BindingValue);

#endif /* _FFTEX_H_ */

