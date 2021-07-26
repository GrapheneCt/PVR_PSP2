/**************************************************************************
 * Name         : state.h
 *
 * Copyright    : 2003-2006 by Imagination Technologies Limited. All rights reserved.
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
 * $Log: state.h $
 *
 *  --- Revision Logs Removed --- 
 **************************************************************************/
#ifndef _STATE_
#define _STATE_

#define GLES1_BLENDFACTOR_ZERO					0x0
#define GLES1_BLENDFACTOR_ONE					0x1
#define GLES1_BLENDFACTOR_SRCCOLOR				0x2
#define GLES1_BLENDFACTOR_ONEMINUS_SRCCOLOR		0x3
#define GLES1_BLENDFACTOR_SRCALPHA				0x4
#define GLES1_BLENDFACTOR_ONEMINUS_SRCALPHA		0x5
#define GLES1_BLENDFACTOR_DSTALPHA				0x6
#define GLES1_BLENDFACTOR_ONEMINUS_DSTALPHA		0x7
#define GLES1_BLENDFACTOR_DSTCOLOR				0x8
#define GLES1_BLENDFACTOR_ONEMINUS_DSTCOLOR		0x9
#define GLES1_BLENDFACTOR_SRCALPHA_SATURATE		0xA

#define GLES1_BLENDFACTOR_DSTALPHA_SATURATE		0xB /* Used by HW in reverse subtract equation */

#define GLES1_BLENDFACTOR_RGBSRC_SHIFT			0
#define GLES1_BLENDFACTOR_RGBDST_SHIFT			4
#define GLES1_BLENDFACTOR_ALPHASRC_SHIFT		8
#define GLES1_BLENDFACTOR_ALPHADST_SHIFT		12

#define GLES1_BLENDFACTOR_RGBSRC_MASK			0x0000000F
#define GLES1_BLENDFACTOR_RGBDST_MASK			0x000000F0
#define GLES1_BLENDFACTOR_ALPHASRC_MASK			0x00000F00
#define GLES1_BLENDFACTOR_ALPHADST_MASK			0x0000F000

#define GLES1_BLENDFUNCTION_ZERO_ONE		((GLES1_BLENDFACTOR_ZERO << GLES1_BLENDFACTOR_RGBSRC_SHIFT)	|	\
											 (GLES1_BLENDFACTOR_ZERO << GLES1_BLENDFACTOR_ALPHASRC_SHIFT) |	\
											 (GLES1_BLENDFACTOR_ONE << GLES1_BLENDFACTOR_RGBDST_SHIFT)	|	\
											 (GLES1_BLENDFACTOR_ONE << GLES1_BLENDFACTOR_ALPHADST_SHIFT))
											
#define	GLES1_BLENDMODE_NONE					0x0
#define	GLES1_BLENDMODE_ADD						0x1
#define	GLES1_BLENDMODE_SUBTRACT				0x2
#define	GLES1_BLENDMODE_REVSUBTRACT				0x3

#define GLES1_BLENDMODE_RGB_SHIFT				0
#define GLES1_BLENDMODE_ALPHA_SHIFT				2

#define GLES1_BLENDMODE_RGB_MASK				0x3
#define GLES1_BLENDMODE_ALPHA_MASK				0xC

/************************************************************************/
/*					GLES Framebuffer Color Mask State					*/
/************************************************************************/
#if defined(SGX_FEATURE_USE_VEC34)
#define	GLES1_COLORMASK_ALPHA					0x1
#define	GLES1_COLORMASK_RED						0x2
#define	GLES1_COLORMASK_GREEN					0x4
#define	GLES1_COLORMASK_BLUE					0x8
#else
#define	GLES1_COLORMASK_ALPHA					0x1
#define	GLES1_COLORMASK_BLUE					0x2
#define	GLES1_COLORMASK_GREEN					0x4
#define	GLES1_COLORMASK_RED						0x8
#endif

#define GLES1_COLORMASK_ALL						(GLES1_COLORMASK_RED   | \
												 GLES1_COLORMASK_GREEN | \
												 GLES1_COLORMASK_BLUE  | \
												 GLES1_COLORMASK_ALPHA)


#define GLES1_CLEARFLAG_COLOR					0x1UL
#define GLES1_CLEARFLAG_DEPTH					0x2UL
#define GLES1_CLEARFLAG_STENCIL					0x4UL


typedef struct GLESPointStateRec
{
    IMG_FLOAT *pfPointSize;
    
	IMG_FLOAT fSmoothSize;
    IMG_FLOAT fAliasedSize;
	IMG_FLOAT fRequestedSize;
    
	IMG_FLOAT *pfMinPointSize;
	IMG_FLOAT *pfMaxPointSize;

	IMG_FLOAT fMinSmoothSize, fMaxSmoothSize;
	IMG_FLOAT fMinAliasedSize, fMaxAliasedSize;
    
    IMG_FLOAT afAttenuation[4];
	IMG_FLOAT fClampMin;
	IMG_FLOAT fClampMax;
	IMG_FLOAT fFade;
    IMG_BOOL  bAttenuationEnabled;

} GLESPointState;


typedef struct GLESViewportRec
{
    /*
    ** Viewport parameters from user, as integers.
    */
    IMG_INT32 i32X, i32Y;
    IMG_UINT32 ui32Width, ui32Height;

    /*
    ** Depthrange parameters from user.
    */
    IMG_FLOAT fZNear, fZFar;

    /*
    ** Internal form of viewport and depth range used to compute
    ** window coordinates from clip coordinates. These need to be in this order for VGP input
    */
    IMG_FLOAT fXCenter, fXScale;
    IMG_FLOAT fYCenter, fYScale;
    IMG_FLOAT fZCenter, fZScale;
	
	GLEScoord sFrontBackClip[2];

} GLESViewport;


typedef struct GLESLineStateRec
{
    IMG_FLOAT *pfLineWidth;
     
	IMG_FLOAT fSmoothWidth;
    IMG_FLOAT fAliasedWidth;


} GLESLineState;


typedef struct GLESPolygonStateRec
{
    GLenum eCullMode;
    GLenum eFrontFaceDirection;

    /*
    ** Polygon offset state. 
    */
#if defined(SGX_FEATURE_DEPTH_BIAS_OBJECTS)
	IMG_INT32 i32Units;
#else
	IMG_UINT32 ui32DepthBias;
#endif

	GLES1_FUINT32 factor;
	IMG_FLOAT fUnits;

} GLESPolygonState;


typedef struct GLESDepthStateRec
{
    /*
    ** Depth buffer test function and write mask
    */
    IMG_UINT32 ui32TestFunc;

    /*
    ** Value used to clear the z buffer when glClear is called.
    */
    IMG_FLOAT fClear;

} GLESDepthState;

/*
** Stencil state.  Contains all the user settable stencil state.
*/
typedef struct GLESStencilStateRec
{
	IMG_UINT32 ui32Stencil;

	IMG_UINT32 ui32StencilRef;

    IMG_UINT32 ui32Clear;


	/* True values of masks and references */
	IMG_UINT32 ui32StencilCompareMaskIn;

	IMG_UINT32 ui32StencilWriteMaskIn;

	IMG_INT32  i32StencilRefIn;

	IMG_UINT32 ui32MaxFBOStencilVal;

} GLESStencilState;




typedef struct GLESRasterStateRec
{
    /*
    ** Alpha function.  The alpha function is applied to the alpha color
    ** value and the reference value.  If it fails then the pixel is
    ** not rendered.
    */
	IMG_UINT32 ui32AlphaTestFunc;
	IMG_FLOAT  fAlphaTestRef;
	IMG_UINT32 ui32AlphaTestRef;

    /*
    ** Frame buffer blending source and destination factors
    */
    IMG_UINT32 ui32BlendFunction;

    /*
    ** Frame buffer blending equation
    */
	IMG_UINT32 ui32BlendEquation;

    /*
    ** Logic op.
    */
    IMG_UINT32 ui32LogicOp;

	/* Clear state */
	GLEScolor sClearColor;
	IMG_UINT32 ui32ClearColor;

    /*
    ** RGB write masks
    */
	IMG_UINT32	ui32ColorMask;

} GLESRasterState;


#define GLES1_HINT_PERSPECTIVE		0
#define GLES1_HINT_POINTSMOOTH		1
#define GLES1_HINT_LINESMOOTH		2
#define GLES1_HINT_POLYSMOOTH		3
#define GLES1_HINT_FOG				4

#define GLES1_HINT_GENMIPMAP			5
#define GLES1_HINT_NUMHINTS			6



/*
** Hint state.  Contains all the user controllable hint state.
*/
typedef struct GLESHintStateRec
{
    GLenum eHint[GLES1_HINT_NUMHINTS];

} GLESHintState;


typedef struct GLESTextureStateRec
{
    /* Current active texture unit (stored as an index not an enum) */
    IMG_UINT32 ui32ActiveTexture;

    /* Pointer to current active texture unit */
    GLESTextureUnitState *psActive;

    /* Current state for all texture units */
    GLESTextureUnitState asUnit[GLES1_MAX_TEXTURE_UNITS];

} GLESTextureState;


typedef struct GLESClientPixelStateRec
{
    IMG_UINT32 ui32PackAlignment;
    IMG_UINT32 ui32UnpackAlignment;

} GLESClientPixelState;

/*
** Scissor state from user.
*/
typedef struct GLESScissorRec
{
    IMG_INT32 i32ScissorX, i32ScissorY;
    IMG_UINT32 ui32ScissorWidth, ui32ScissorHeight;
	IMG_INT32 i32ClampedWidth, i32ClampedHeight;

} GLESScissor;

/*
** Multisample state
*/
typedef struct GLESMultisampleStateRec
{
	IMG_FLOAT fSampleCoverageValue;
	IMG_BOOL bSampleCoverageInvert;

} GLESMultisampleState;

/*
** Shade state
*/
typedef struct GLESShadeStateRec
{
	IMG_UINT32 ui32ShadeModel;

} GLESShadeState;


typedef struct GLESStateRec
{
    GLESHintState sHints;
    GLESScissor sScissor;
    GLESClientPixelState sClientPixel;
    GLESTextureState sTexture;

    GLESStencilState sStencil;
	GLESMultisampleState sMultisample;
    GLESPolygonState sPolygon;
    GLESRasterState sRaster;
    GLESDepthState sDepth;
    GLESViewport sViewport;
    GLESPointState sPoint;
    GLESLineState sLine;

    GLESCurrentState sCurrent;
	GLESShadeState sShade;
    GLESLightState sLight;
    GLESFogState sFog;
    GLenum eMatrixMode;
    IMG_UINT32 ui32ClientActiveTexture;


} GLESState;

#endif /* _STATE_ */
