/**************************************************************************
 * Name         : state.h
 *
 * Copyright    : 2005-2006 by Imagination Technologies Limited. All rights reserved.
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
 **************************************************************************/

#ifndef _STATE_
#define _STATE_


/************************************************************************/
/*					GLES2 Enable State					*/
/************************************************************************/

#define GLES2_CULLFACE							1
#define GLES2_POLYOFFSET						2
#define GLES2_SCISSOR							3
#define GLES2_ALPHABLEND						4
#define GLES2_MSALPHACOV						5
#define GLES2_MSSAMPALPHA						6
#define GLES2_MSSAMPCOV							7
#define GLES2_STENCILTEST						8
#define GLES2_DEPTHTEST							9
#define GLES2_DITHER							10


#define GLES2_CULLFACE_ENABLE					(1U << GLES2_CULLFACE)
#define GLES2_POLYOFFSET_ENABLE					(1U << GLES2_POLYOFFSET)
#define GLES2_SCISSOR_ENABLE					(1U << GLES2_SCISSOR)
#define GLES2_ALPHABLEND_ENABLE					(1U << GLES2_ALPHABLEND)
#define GLES2_MSALPHACOV_ENABLE					(1U << GLES2_MSALPHACOV)
#define GLES2_MSSAMPALPHA_ENABLE				(1U << GLES2_MSSAMPALPHA)
#define GLES2_MSSAMPCOV_ENABLE					(1U << GLES2_MSSAMPCOV)
#define GLES2_STENCILTEST_ENABLE				(1U << GLES2_STENCILTEST)
#define GLES2_DEPTHTEST_ENABLE					(1U << GLES2_DEPTHTEST)
#define GLES2_DITHER_ENABLE						(1U << GLES2_DITHER)

/************************************************************************/
/*					GLES2 Framebuffer Blending State					*/
/************************************************************************/
/* Reserve 0 to use for disabled blending */
#define	GLES2_BLENDFUNC_NONE					0x0
#define	GLES2_BLENDFUNC_ADD						0x1
#define	GLES2_BLENDFUNC_SUBTRACT				0x2
#define	GLES2_BLENDFUNC_REVSUBTRACT				0x3

#define GLES2_BLENDFUNC_RGB_SHIFT				0
#define GLES2_BLENDFUNC_ALPHA_SHIFT				2

#define GLES2_BLENDFUNC_RGB_MASK				0x3
#define GLES2_BLENDFUNC_ALPHA_MASK				0xC

#define GLES2_BLENDFACTOR_ZERO					0x0
#define GLES2_BLENDFACTOR_ONE					0x1
#define GLES2_BLENDFACTOR_SRCCOLOR				0x2
#define GLES2_BLENDFACTOR_ONEMINUS_SRCCOLOR		0x3
#define GLES2_BLENDFACTOR_SRCALPHA				0x4
#define GLES2_BLENDFACTOR_ONEMINUS_SRCALPHA		0x5
#define GLES2_BLENDFACTOR_DSTALPHA				0x6
#define GLES2_BLENDFACTOR_ONEMINUS_DSTALPHA		0x7
#define GLES2_BLENDFACTOR_DSTCOLOR				0x8
#define GLES2_BLENDFACTOR_ONEMINUS_DSTCOLOR		0x9
#define GLES2_BLENDFACTOR_SRCALPHA_SATURATE		0xA
#define GLES2_BLENDFACTOR_CONSTCOLOR			0xB
#define GLES2_BLENDFACTOR_ONEMINUS_CONSTCOLOR	0xC
#define GLES2_BLENDFACTOR_CONSTALPHA			0xD
#define GLES2_BLENDFACTOR_ONEMINUS_CONSTALPHA	0xE
#define GLES2_BLENDFACTOR_DSTALPHA_SATURATE		0xF

#define GLES2_BLENDFACTOR_RGBSRC_SHIFT			0
#define GLES2_BLENDFACTOR_RGBDST_SHIFT			4
#define GLES2_BLENDFACTOR_ALPHASRC_SHIFT		8
#define GLES2_BLENDFACTOR_ALPHADST_SHIFT		12

#define GLES2_BLENDFACTOR_RGBSRC_MASK			0x0000000F
#define GLES2_BLENDFACTOR_RGBDST_MASK			0x000000F0
#define GLES2_BLENDFACTOR_ALPHASRC_MASK			0x00000F00
#define GLES2_BLENDFACTOR_ALPHADST_MASK			0x0000F000

/************************************************************************/
/*					GLES2 Framebuffer Color Mask State					*/
/************************************************************************/
#if defined(SGX_FEATURE_USE_VEC34)
#define	GLES2_COLORMASK_ALPHA					0x1U
#define	GLES2_COLORMASK_RED						0x2U
#define	GLES2_COLORMASK_GREEN					0x4U
#define	GLES2_COLORMASK_BLUE					0x8U
#else
#define	GLES2_COLORMASK_ALPHA					0x1U
#define	GLES2_COLORMASK_BLUE					0x2U
#define	GLES2_COLORMASK_GREEN					0x4U
#define	GLES2_COLORMASK_RED						0x8U
#endif

#define GLES2_COLORMASK_ALL						0xF


#define GLES2_HINT_GENMIPMAP					0
#define GLES2_HINT_DERIVATIVE					1
#define GLES2_HINT_NUMHINTS						2

#define GLES2_CLEARFLAG_COLOR					0x1UL
#define GLES2_CLEARFLAG_DEPTH					0x2UL
#define GLES2_CLEARFLAG_STENCIL					0x4UL


/*
** Hint state.  Contains all the user controllable hint state.
*/
typedef struct {
    GLenum eHint[GLES2_HINT_NUMHINTS];
} GLES2hintState;

typedef struct GLES2rasterStateRec {

	/*
    ** Back end blend state
    */
	GLES2color sBlendColor;
	IMG_UINT32 ui32BlendEquation;
	IMG_UINT32 ui32BlendFactor;
	IMG_UINT32 ui32BlendColor;

	/*
    ** RGBA write masks.  
    */
	IMG_UINT32	ui32ColorMask;

	/* Clear state */
	GLES2color sClearColor;
	IMG_UINT32 ui32ClearColor;
}GLES2rasterState;

typedef struct GLES2polygonStateRec {

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

	GLES2_FUINT32 factor;
	IMG_FLOAT fUnits;

} GLES2polygonState;


typedef struct GLES2depthStateRec {
    /*
    ** Depth buffer test function.  The z value is compared using zFunction
    ** against the current value in the zbuffer.  If the comparison
    ** succeeds the new z value is written into the z buffer masked
    ** by the z write mask.
    */
    IMG_UINT32 ui32TestFunc;

    /*
    ** Writemask enable.  When GL_TRUE writing to the depth buffer is
    ** allowed.
    */
/*    GLboolean writeEnable; 
 * 
 *	Stored elsewhere
 */


    /*
    ** Value used to clear the z buffer when glClear is called.
    */
    IMG_FLOAT fClear;

} GLES2depthState;

typedef struct GLES2viewportRec {
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
    ** window coordinates from clip coordinates. 
    */
    IMG_FLOAT fXCenter, fXScale;
    IMG_FLOAT fYCenter, fYScale;
    IMG_FLOAT fZCenter, fZScale;
	
} GLES2viewport;

typedef struct GLES2lineStateRec {
     
	IMG_FLOAT fWidth;

} GLES2lineState;


typedef struct GLES2multisampleStateRec {
	IMG_FLOAT fSampleCoverageValue;
	IMG_BOOL bSampleCoverageInvert;
} GLES2multisampleState;

/*
** Stencil state.  Contains all the user settable stencil state.
*/
typedef struct GLES2stencilStateRec {
   
    IMG_UINT32 ui32Clear;

	IMG_UINT32 ui32FFStencil;
	IMG_UINT32 ui32BFStencil;

	IMG_UINT32 ui32FFStencilRef;
	IMG_UINT32 ui32BFStencilRef;


	/* True values of masks and references */
	IMG_UINT32 ui32FFStencilCompareMaskIn;
	IMG_UINT32 ui32BFStencilCompareMaskIn;

	IMG_UINT32 ui32FFStencilWriteMaskIn;
	IMG_UINT32 ui32BFStencilWriteMaskIn;

	IMG_INT32 i32FFStencilRefIn;
	IMG_INT32 i32BFStencilRefIn;
	
	IMG_UINT32 ui32MaxFBOStencilVal;

} GLES2stencilState;

typedef struct GLES2textureStateRec {
    /* Current active texture unit (stored as an index not an enum) */
    IMG_UINT32 ui32ActiveTexture;

    /* Pointer to current active texture unit */
    GLES2TextureUnitState *psActive;

    /* Current state for all texture units */
    GLES2TextureUnitState asUnit[GLES2_MAX_TEXTURE_UNITS];
} GLES2textureState;

typedef struct GLES2clientPixelStateRec {
    IMG_UINT32 ui32PackAlignment;
    IMG_UINT32 ui32UnpackAlignment;
} GLES2clientPixelState;

/*
** Scissor state from user.
*/
typedef struct GLES2scissorRec {
    IMG_INT32 i32ScissorX, i32ScissorY;
    IMG_UINT32 ui32ScissorWidth, ui32ScissorHeight;
	IMG_INT32 i32ClampedWidth, i32ClampedHeight;
} GLES2scissor;


/************************************************************************/
/*								GLES2 State 							*/
/************************************************************************/
typedef struct GLES2stateRec {

    GLES2hintState sHints;
    GLES2scissor sScissor;
	GLES2clientPixelState sClientPixel;
	GLES2textureState sTexture;
    GLES2stencilState sStencil;
	GLES2multisampleState sMultisample;
    GLES2polygonState sPolygon;
	GLES2rasterState sRaster;
    GLES2depthState sDepth;
    GLES2viewport sViewport;
    GLES2lineState sLine;
} GLES2state;

IMG_VOID ApplyViewport(GLES2Context *gc);
IMG_VOID ApplyDepthRange(GLES2Context *gc, IMG_FLOAT fZNear, IMG_FLOAT fZFar);

#endif /* _STATE_ */
