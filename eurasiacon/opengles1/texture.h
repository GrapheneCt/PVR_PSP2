/******************************************************************************
 * Name         : texture.h
 *
 * Copyright    : 2003-2008 by Imagination Technologies Limited.
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
 * $Log: texture.h $
 *****************************************************************************/

#ifndef _TEXTURE_
#define _TEXTURE_

#include "names.h"

#define GLES1_TEXTURE_LATENCY				2

#define GLES1_TEX_INCONSISTENT				0
#define GLES1_TEX_CONSISTENT				1
#define GLES1_TEX_UNKNOWN					2
#define GLES1_TEX_DUMMY						3


#define GLES1_ALPHA_TEX_INDEX				0
#define GLES1_LUMINANCE_TEX_INDEX			1
#define GLES1_LUMINANCE_ALPHA_TEX_INDEX		2
#define GLES1_RGB_TEX_INDEX					3
#define GLES1_RGBA_TEX_INDEX				4

#define GLES1_NUM_TEX_FORMATS				5

#define GLES1_MODULATE_INDEX				0
#define GLES1_DECAL_INDEX					1
#define GLES1_BLEND_INDEX					2
#define GLES1_REPLACE_INDEX					3
#define GLES1_ADD_INDEX						4
#define GLES1_NUM_TEX_BLENDS				5

#define GLES1_REAP_ALL_ACTIVE				1
#define GLES1_FORCE_FREE_MEM				2


#define	GLES1_COMBINE_INDEX					5

#define GLES1_COMBINE_COLOR					0
#define GLES1_COMBINE_ALPHA					1
#define GLES1_NUM_COMBINERS					2

#define GLES1_COMBINE_MAX_SRC				3

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)

	#define GLES1_TEXTURE_TARGET_2D				0
	#define GLES1_TEXTURE_TARGET_CEM			1
#if defined(GLES1_EXTENSION_TEXTURE_STREAM) || defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
	#define GLES1_TEXTURE_TARGET_STREAM			2
	#define GLES1_TEXTURE_TARGET_MAX			3
#else /* defined(GLES1_EXTENSION_TEXTURE_STREAM) */
	#define GLES1_TEXTURE_TARGET_MAX			2
#endif /* defined(GLES1_EXTENSION_TEXTURE_STREAM) || defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)*/

#define GLES1_TEXTURE_CEM_FACE_POSX				0
	#define GLES1_TEXTURE_CEM_FACE_NEGX			1
	#define GLES1_TEXTURE_CEM_FACE_POSY			2
	#define GLES1_TEXTURE_CEM_FACE_NEGY			3
	#define GLES1_TEXTURE_CEM_FACE_POSZ			4
	#define GLES1_TEXTURE_CEM_FACE_NEGZ			5

	#define GLES1_TEXTURE_CEM_FACE_MAX			6

#else /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */

	#define GLES1_TEXTURE_TARGET_2D				0

#if defined(GLES1_EXTENSION_TEXTURE_STREAM)|| defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
	#define GLES1_TEXTURE_TARGET_STREAM			1
	#define GLES1_TEXTURE_TARGET_MAX			2
#else /* defined(GLES1_EXTENSION_TEXTURE_STREAM) || defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)*/
	#define GLES1_TEXTURE_TARGET_MAX			1
#endif /* defined(GLES1_EXTENSION_TEXTURE_STREAM) || defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)*/

#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */

#define GLES1_LOADED_LEVEL					((IMG_VOID *)0xFFFFFFFF)


#define GLES1_MIPMAP						0x00000001
#define GLES1_COMPRESSED					0x00000002


typedef struct GLESTextureFormatRec
{
	IMG_UINT32 ui32TotalBytesPerTexel;
	IMG_UINT32 ui32BaseFormatIndex;

	PVRSRV_PIXEL_FORMAT ePixelFormat;

} GLESTextureFormat;

/*
** Per Mipmap Level state.
*/
typedef struct GLESMipMapLevelRec
{
	/* This struct must be the first variable */
	GLES1FrameBufferAttachable sFBAttachable;

	/* These are the texels for this mipmap level */
	IMG_UINT8 *pui8Buffer;

	/* Image dimensions */
	IMG_UINT32 ui32Width, ui32Height, ui32ImageSize;

	/* log2 of width & height */
	IMG_UINT32 ui32WidthLog2, ui32HeightLog2;

	/* Requested internal format */
	GLenum eRequestedFormat;

	/* Actual texel format */ 
	const GLESTextureFormat *psTexFormat;

	/* Texture this mipmap belongs to */
	struct GLESTextureRec *psTex;

	/* The level of this mipmap in the parent texture */
	IMG_UINT32 ui32Level;

} GLESMipMapLevel;



/* 
** Sub texture info used for HWTQ functions.
*/
typedef struct GLESSubTextureInfoRec
{
    IMG_UINT32     ui32SubTexXoffset;
    IMG_UINT32     ui32SubTexYoffset;

    IMG_UINT32     ui32SubTexWidth;
    IMG_UINT32     ui32SubTexHeight;

    IMG_UINT8     *pui8SubTexBuffer;

} GLESSubTextureInfo;




/*
** State set with glTexParameter
*/

typedef struct GLESTextureParamStateRec
{
	/* Hardware control words */
	IMG_UINT32 ui32StateWord0;
	IMG_UINT32 ui32StateWord1;
	IMG_UINT32 ui32StateWord2;
#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
	IMG_UINT32 ui32StateWord3;
#endif /* defined(SGX545) */    

	/* Minification and magnification filters */
	IMG_UINT32 ui32MinFilter;
	IMG_UINT32 ui32MagFilter;

    IMG_BOOL   bAutoMipGen;
	IMG_BOOL   bGenerateMipmap;

	/* Crop rectangle for DrawTexture extension */
	IMG_INT32 i32CropRectU;
	IMG_INT32 i32CropRectV;
	IMG_INT32 i32CropRectW;
	IMG_INT32 i32CropRectH;

} GLESTextureParamState;


/* Representation of texture_env_combine/texture_env_crossbar state */

/* Op defines */
#define	GLES1_COMBINEOP_REPLACE		0x00000000UL
#define GLES1_COMBINEOP_MODULATE	0x00000001UL
#define GLES1_COMBINEOP_ADD			0x00000002UL
#define GLES1_COMBINEOP_ADDSIGNED	0x00000003UL
#define GLES1_COMBINEOP_INTERPOLATE	0x00000004UL
#define GLES1_COMBINEOP_SUBTRACT	0x00000005UL
#define GLES1_COMBINEOP_DOT3_RGB	0x00000006UL
#define GLES1_COMBINEOP_DOT3_RGBA	0x00000007UL

#define GLES1_COMBINEOP_SCALE_ONE	0x00000000UL
#define GLES1_COMBINEOP_SCALE_TWO	0x00000010UL
#define GLES1_COMBINEOP_SCALE_FOUR	0x00000020UL

#define GLES1_COMBINE_COLOROP_SHIFT		0
#define GLES1_COMBINE_COLOROP_CLRMSK	0xFFFFFFF0UL
#define GLES1_COMBINE_COLORSCALE_SHIFT	0
#define GLES1_COMBINE_COLORSCALE_CLRMSK	0xFFFFFF0FUL

#define GLES1_COMBINE_ALPHAOP_SHIFT		8
#define GLES1_COMBINE_ALPHAOP_CLRMSK	0xFFFFF0FFUL
#define GLES1_COMBINE_ALPHASCALE_SHIFT	8
#define GLES1_COMBINE_ALPHASCALE_CLRMSK	0xFFFF0FFFUL


#define GLES1_COMBINE_SET_COLOROP(OP, COLOROP) \
	(((OP) & GLES1_COMBINE_COLOROP_CLRMSK) | (COLOROP))

#define GLES1_COMBINE_GET_COLOROP(OP) \
	((OP) & ~GLES1_COMBINE_COLOROP_CLRMSK)

#define GLES1_COMBINE_SET_COLORSCALE(OP, COLORSCALE) \
	(((OP) & GLES1_COMBINE_COLORSCALE_CLRMSK) | (COLORSCALE))

#define GLES1_COMBINE_GET_COLORSCALE(OP) \
	((OP) & ~GLES1_COMBINE_COLORSCALE_CLRMSK)


#define GLES1_COMBINE_SET_ALPHAOP(OP, ALPHAOP) \
	(((OP) & GLES1_COMBINE_ALPHAOP_CLRMSK) | ((ALPHAOP) << GLES1_COMBINE_ALPHAOP_SHIFT))

#define GLES1_COMBINE_GET_ALPHAOP(OP) \
	(((OP) & ~GLES1_COMBINE_ALPHAOP_CLRMSK) >> GLES1_COMBINE_ALPHAOP_SHIFT)

#define GLES1_COMBINE_SET_ALPHASCALE(OP, ALPHASCALE) \
	(((OP) & GLES1_COMBINE_ALPHASCALE_CLRMSK) | ((ALPHASCALE) << GLES1_COMBINE_ALPHASCALE_SHIFT))

#define GLES1_COMBINE_GET_ALPHASCALE(OP) \
	(((OP) & ~GLES1_COMBINE_ALPHASCALE_CLRMSK) >> GLES1_COMBINE_ALPHASCALE_SHIFT)

/* Colour sources */

/* Colour source defines */
#define GLES1_COMBINECSRC_PRIMARY			0x00000000UL
#define GLES1_COMBINECSRC_PREVIOUS			0x00000001UL
#define GLES1_COMBINECSRC_TEXTURE			0x00000002UL
#define GLES1_COMBINECSRC_CONSTANT			0x00000003UL

#define GLES1_COMBINECSRC_OPERANDCOLOR		0x00000000UL
#define GLES1_COMBINECSRC_OPERANDALPHA		0x00000004UL
#define GLES1_COMBINECSRC_OPERANDONEMINUS	0x00000008UL
#define GLES1_COMBINECSRC_CROSSBAR			0x00000010UL

#define GLES1_COMBINECSRC_SRC_SHIFT				0
#define GLES1_COMBINECSRC_SRC_MSK				0x00000003UL
#define GLES1_COMBINECSRC_OPERAND_SHIFT			0
#define GLES1_COMBINECSRC_OPERAND_MSK			0x0000000CUL
#define GLES1_COMBINECSRC_CROSSBAR_MSK			0x000000F0UL
#define GLES1_COMBINECSRC_CROSSBAR_UNIT_SHIFT	5
#define GLES1_COMBINECSRC_SRC_SIZE				8

/* Sources */
#define GLES1_COMBINE_SET_COLSRC(SOURCES, NUM, SRC) \
	(((SOURCES) & ~(GLES1_COMBINECSRC_SRC_MSK << (NUM * GLES1_COMBINECSRC_SRC_SIZE))) |	\
							  (SRC) << (NUM * GLES1_COMBINECSRC_SRC_SIZE))

#define GLES1_COMBINE_GET_COLSRC(SOURCES, NUM) \
	(((SOURCES) >> (NUM * GLES1_COMBINECSRC_SRC_SIZE)) & GLES1_COMBINECSRC_SRC_MSK)

/* Operands */
#define GLES1_COMBINE_SET_COLOPERAND(SOURCES, NUM, OPERAND) \
	(((SOURCES) & ~(GLES1_COMBINECSRC_OPERAND_MSK << (NUM * GLES1_COMBINECSRC_SRC_SIZE))) |	\
							  (OPERAND) << (NUM * GLES1_COMBINECSRC_SRC_SIZE))

#define GLES1_COMBINE_GET_COLOPERAND(SOURCES, NUM) \
	(((SOURCES) >> (NUM * GLES1_COMBINECSRC_SRC_SIZE)) & GLES1_COMBINECSRC_OPERAND_MSK)

/* Crossbar unit number */
#define GLES1_COMBINE_SET_COLCROSSBAR(SOURCES, NUM, UNIT) \
	(((SOURCES) & ~(GLES1_COMBINECSRC_CROSSBAR_MSK << (NUM * GLES1_COMBINECSRC_SRC_SIZE))) |	\
							  (((UNIT) << GLES1_COMBINECSRC_CROSSBAR_UNIT_SHIFT) | GLES1_COMBINECSRC_CROSSBAR )<< (NUM * GLES1_COMBINECSRC_SRC_SIZE))

#define GLES1_COMBINE_GET_COLCROSSBAR(SOURCES, NUM) \
	(((SOURCES) >> (NUM * GLES1_COMBINECSRC_SRC_SIZE)) & GLES1_COMBINECSRC_CROSSBAR_MSK)

#define GLES1_COMBINE_CLEAR_COLCROSSBAR(SOURCES, NUM) \
	((SOURCES) & ~(GLES1_COMBINECSRC_CROSSBAR_MSK << (NUM * GLES1_COMBINECSRC_SRC_SIZE)))

/* Alpha sources */

/* Alpha source defines */
#define	GLES1_COMBINEASRC_PRIMARY 			0x00000000UL
#define GLES1_COMBINEASRC_PREVIOUS 			0x00000001UL
#define GLES1_COMBINEASRC_TEXTURE			0x00000002UL
#define GLES1_COMBINEASRC_CONSTANT			0x00000003UL

#define GLES1_COMBINEASRC_OPERANDONEMINUS	0x00000008UL
#define GLES1_COMBINEASRC_CROSSBAR			0x00000010UL

#define GLES1_COMBINEASRC_SRC_SHIFT				0
#define GLES1_COMBINEASRC_SRC_MSK				0x00000003UL
#define GLES1_COMBINEASRC_OPERAND_SHIFT			0
#define GLES1_COMBINEASRC_OPERAND_MSK			0x00000008UL
#define GLES1_COMBINEASRC_CROSSBAR_MSK			0x000000F0UL
#define GLES1_COMBINEASRC_CROSSBAR_UNIT_SHIFT	5
#define GLES1_COMBINEASRC_SRC_SIZE				8

/* Sources */
#define GLES1_COMBINE_SET_ALPHASRC(SOURCES, NUM, SRC) \
	(((SOURCES) & ~(GLES1_COMBINEASRC_SRC_MSK << (NUM * GLES1_COMBINEASRC_SRC_SIZE))) |	\
								(SRC) << (NUM * GLES1_COMBINEASRC_SRC_SIZE))

#define GLES1_COMBINE_GET_ALPHASRC(SOURCES, NUM) \
	(((SOURCES) >> (NUM * GLES1_COMBINEASRC_SRC_SIZE)) & GLES1_COMBINEASRC_SRC_MSK)


/* Operands */
#define GLES1_COMBINE_SET_ALPHAOPERAND(SOURCES, NUM, OPERAND) \
	(((SOURCES) & ~(GLES1_COMBINEASRC_OPERAND_MSK << (NUM * GLES1_COMBINEASRC_SRC_SIZE))) |	\
							  (OPERAND) << (NUM * GLES1_COMBINEASRC_SRC_SIZE))

#define GLES1_COMBINE_GET_ALPHAOPERAND(SOURCES, NUM) \
	(((SOURCES) >> (NUM * GLES1_COMBINEASRC_SRC_SIZE)) & GLES1_COMBINEASRC_OPERAND_MSK)


/* Crossbar unit number */
#define GLES1_COMBINE_SET_ALPHACROSSBAR(SOURCES, NUM, UNIT) \
	(((SOURCES) & ~(GLES1_COMBINEASRC_CROSSBAR_MSK << (NUM * GLES1_COMBINEASRC_SRC_SIZE))) |	\
								(((UNIT) << GLES1_COMBINEASRC_CROSSBAR_UNIT_SHIFT) | GLES1_COMBINEASRC_CROSSBAR) << (NUM * GLES1_COMBINEASRC_SRC_SIZE))

#define GLES1_COMBINE_GET_ALPHACROSSBAR(SOURCES, NUM) \
	(((SOURCES) >> (NUM * GLES1_COMBINEASRC_SRC_SIZE)) & GLES1_COMBINEASRC_CROSSBAR_MSK)

#define GLES1_COMBINE_CLEAR_ALPHACROSSBAR(SOURCES, NUM) \
	((SOURCES) & ~(GLES1_COMBINEASRC_CROSSBAR_MSK << (NUM * GLES1_COMBINEASRC_SRC_SIZE)))


/*
** State set with glTexEnv
*/
typedef struct GLESTextureEnvStateRec
{
	/* environment "blend" function */
	IMG_UINT32 ui32Mode;

	/* environment color - stored in HW format*/
	IMG_UINT32 ui32Color;

	/* environment color */
	GLEScolor sColor;

	/* Point sprite texture coord */
	IMG_BOOL bPointSpriteReplace;

	/* Texture_env_combine stuff */
	IMG_UINT32 ui32Op;
	IMG_UINT32 ui32ColorSrcs;
	IMG_UINT32 ui32AlphaSrcs;

} GLESTextureEnvState;

typedef struct GLESTextureUnitStateRec
{
	/* Per texture object binding state (points to bound texture object) */
	GLESTextureParamState *psTexture[GLES1_TEXTURE_TARGET_MAX];

	/* Per texture environment binding state */
	GLESTextureEnvState sEnv;

	GLenum eTexGenMode;


} GLESTextureUnitState;

/*
** Per Texture Object state.
*/
struct GLESTextureRec
{
	/* This struct must be the first variable */
	GLES1NamedItem sNamedItem;
	
	/* Textures are frame resources */
	KRMResource sResource;

	/* Param state for this texture object */
	GLESTextureParamState sState;

	/* Target that this texture was created on */
	IMG_UINT32 ui32TextureTarget;

	/* Level information */
	IMG_UINT32 ui32NumLevels;
	GLESMipMapLevel *psMipLevel;

	/* Number of mipmap levels that are render targets */
	IMG_UINT32 ui32NumRenderTargets;

	/* Assuming psMemInfo is non-null, this flag indicates whether the device memory is up-to-date.
	   In other words, if psMemInfo is non-null and this flag is false, it means the host data has
	   been updated, but the new data has not yet been loaded into the video memory texture.
	   With this technique, multiple modifications to the host data (e.g., a series
	   the MIP levels updates) can be batched up and loaded into video memory in all together.
	   With this technique, multiple modifications to the host data (e.g. a series
	   of MIP levels updates) can be batched up and loaded into video memory in all together.
	*/
	IMG_BOOL bResidence;

	/* Whether this texture has ever been ghosted */
	IMG_BOOL bHasEverBeenGhosted;
	
	/*
	 * levelsConsistent is updated everytime an level in the texture 
	 * changes size and summarises whether the texture can be sensibly
	 * MIP mapped.
	 */
	IMG_UINT32 ui32LevelsConsistent; 

	const GLESTextureFormat *psFormat;
	IMG_UINT32 ui32HWFlags;


	PVRSRV_CLIENT_MEM_INFO *psMemInfo;

	IMG_VOID (*pfnReadBackData)(IMG_VOID *pvDest, const IMG_VOID *pvSrc, 
								IMG_UINT32 ui32Log2Width, IMG_UINT32 ui32Log2Height, 
								IMG_UINT32 ui32Width, IMG_UINT32 ui32Height, 
								IMG_UINT32 ui32DstStride);

	IMG_VOID (*pfnTextureTwiddle)(IMG_VOID *pvDest, const IMG_VOID	*pvSrc, 
								  IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
								  IMG_UINT32 ui32StrideIn);

#if defined(PDUMP)
	/* The i-th bit marks whether the i-th mipmap level must be dumped.
	 * In a C.E.M. it marks the i-th level of every face.
	 */
	IMG_UINT32 ui32LevelsToDump;
#endif

#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE)
	IMG_HANDLE hPBuffer;
#endif /* defined(EGL_EXTENSION_RENDER_TO_TEXTURE) */

#if defined(GLES1_EXTENSION_EGL_IMAGE)
	EGLImage *psEGLImageSource;
	EGLImage *psEGLImageTarget;
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */

#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
	GLES1StreamDevice *psBufferDevice;
	IMG_UINT32 ui32BufferOffset;
#endif
#if defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
	GLES1ExternalTexState *psExtTexState;
#endif /* GLES1_EXTENSION_EGL_IMAGE_EXTERNAL */
};


typedef struct GLESGhostRec
{
	/* Ghosts are frame resources */
	KRMResource sResource;

	PVRSRV_CLIENT_MEM_INFO *psMemInfo;
	IMG_UINT32 ui32Size;
#if defined(GLES1_EXTENSION_EGL_IMAGE)
	IMG_VOID *hImage;
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */
#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE)
	IMG_VOID *hPBuffer;
#endif /* defined(EGL_EXTENSION_RENDER_TO_TEXTURE) */
#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
	IMG_VOID *hBufferDevice;
#endif /* defined(GLES1_EXTENSION_TEXTURE_STREAM) */

#if defined PDUMP
	/* Whether this ghost has been dumped or not */
	IMG_BOOL bDumped;
#endif

} GLESGhost;


/*
 * Records state of textures; whether they're idle, active, ghosted etc.
 */
typedef struct GLESTextureManagerRec
{
	IMG_UINT32	ui32GhostMem;
	
	/* Frame resource manager for textures */
	KRMKickResourceManager sKRM;

	/* Dummy textures used when some of the textures are inconsistent or could not be made resident */
	PVRSRV_CLIENT_MEM_INFO *psWhiteDummyTexture;

} GLES1TextureManager;

typedef IMG_VOID (*PFNReadSpan)(const GLESPixelSpanInfo *);
typedef IMG_VOID (*PFNCopyPaletteSpan)(IMG_VOID *, const IMG_VOID *, IMG_UINT32, const IMG_VOID *);

typedef IMG_VOID (*PFNCopyTextureData)(IMG_VOID *, const IMG_VOID *, IMG_UINT32, 
				       IMG_UINT32, IMG_UINT32, GLESMipMapLevel *, 
				       IMG_BOOL);


/*
** Per Context rendering state.
*/
typedef struct GLESTextureMachineRec
{
	/* Texture object for the default texture */
	GLESTexture *psDefaultTexture[GLES1_TEXTURE_TARGET_MAX];

	/* Currently bound texture object */
	GLESTexture *apsBoundTexture[GLES1_MAX_TEXTURE_UNITS][GLES1_TEXTURE_TARGET_MAX];

	/* Currently enabled texture target based on enables */
	IMG_UINT32	aui32CurrentTarget[GLES1_MAX_TEXTURE_UNITS];
	
	/* Currently enabled texture format based on enables */
	const GLESTextureFormat *apsCurrentFormat[GLES1_MAX_TEXTURE_UNITS];

	IMG_BOOL	bSpriteMode;

	/* Early initialization function for texture manager */
	IMG_VOID (*pfnInitTextureManager)(GLES1Context *gc);

	/* Default copy functions for RGB/A UB data */
	const GLESTextureFormat *psDefaultRGBFormat;
	const GLESTextureFormat *psDefaultRGBAFormat;

	/* Create a texture with the given name and target */
} GLESTextureMachine;

IMG_VOID TranslateLevel(GLES1Context *gc, GLESTexture *psTex, IMG_UINT32 ui32Face, IMG_UINT32 ui32Lod);

IMG_UINT32 GetMipMapOffset(IMG_UINT32 ui32MapLevel, IMG_UINT32 ui32TopUSize, IMG_UINT32 ui32TopVSize);
IMG_UINT32 GetCompressedMipMapOffset(IMG_UINT32 ui32MapLevel, IMG_UINT32 ui32TopUSize,
									 IMG_UINT32 ui32TopVSize, IMG_BOOL bIs2Bpp);
IMG_BOOL BindTexture(GLES1Context *gc, IMG_UINT32 ui32Unit, IMG_UINT32 ui32Target, IMG_UINT32 ui32Texture);
GLESTexture * CreateTexture(GLES1Context *gc, IMG_UINT32 ui32Name, IMG_UINT32 ui32Target);

IMG_UINT8 * TextureCreateLevel(GLES1Context *gc, GLESTexture *psTex, IMG_UINT32 ui32Level, 
							   GLenum eInternalFormat, const GLESTextureFormat *psTexFormat,
							   IMG_UINT32 ui32Width, IMG_UINT32 ui32Height);

IMG_VOID ReadBackTextureData(GLES1Context *gc, GLESTexture *psTex, IMG_UINT32 ui32Face, IMG_UINT32 ui32Lod, IMG_VOID *pvBuffer);
IMG_VOID TextureRemoveResident(GLES1Context *gc, GLESTexture *psTex);
IMG_VOID SetupTexNameArray(GLES1NamesArray *psNamesArray);
IMG_VOID SetupTwiddleFns(GLESTexture *psTex);

IMG_BOOL TextureMakeResident(GLES1Context *gc, GLESTexture *psTex);
IMG_UINT32 IsTextureConsistent(GLES1Context *gc, GLESTexture *psTex, IMG_BOOL bCheckForRenderingLoop);

IMG_BOOL CreateTextureState(GLES1Context *gc);
IMG_BOOL SetupTextureState(GLES1Context *gc);
IMG_BOOL FreeTextureState(GLES1Context *gc);

IMG_VOID CopyTextureData(GLES1Context           *gc, 
						 GLESTexture            *psDstTex, 
						 IMG_UINT32              ui32DstOffsetInBytes,
						 PVRSRV_CLIENT_MEM_INFO *psSrcInfo,
						 IMG_UINT32              ui32SrcOffsetInBytes,
						 IMG_UINT32              ui32SizeInBytes);
						 

GLES1TextureManager *CreateTextureManager(GLES1Context *gc);
IMG_VOID ReleaseTextureManager(GLES1Context *gc, GLES1TextureManager *psTexMgr);
IMG_BOOL TexMgrGhostTexture(GLES1Context *gc, GLESTexture *psTex);

IMG_BOOL MakeTextureMipmapLevels(GLES1Context *gc, GLESTexture *psTex, IMG_UINT32 ui32Face);
IMG_BOOL HardwareMakeTextureMipmapLevels(GLES1Context *gc, GLESTexture *psTex, IMG_UINT32 ui32TexTarget);

#if defined PDUMP
IMG_VOID PDumpTexture(GLES1Context *gc, GLESTexture *psTex);
#endif

IMG_BOOL CreateTextureMemory(GLES1Context *gc, GLESTexture *psTex);
IMG_VOID SetupTextureBlendWords(GLES1Context *gc);
IMG_BOOL ValidateTextureUnits(GLES1Context *gc);
IMG_VOID TexEnvCombine(GLES1Context *gc, GLenum pname, GLenum e);
IMG_VOID InitTexCombineState(GLES1Context *gc);
IMG_UINT32 CalcCombineControlWord(GLESTextureEnvState *psEnvState, IMG_BOOL *pbHasFactorCol, IMG_BOOL *pbIsDot3);
IMG_VOID ConvertFromFactorColor(GLEScolor *psColor, IMG_UINT32 ui32Color);
IMG_VOID ReleasePbufferFromTexture(GLES1Context *gc, GLESTexture *psTex);

IMG_BOOL SetupTextureRenderTargetControlWords(GLES1Context *gc, GLESTexture *psTex);

IMG_BOOL UnloadInconsistentTexture(GLES1Context *gc, GLESTexture *psTex);



IMG_BOOL PrepareHWTQTextureUpload(GLES1Context       *gc, 
								  GLESTexture        *psTex,
								  IMG_UINT32          ui32OffsetInBytes,
								  GLESMipMapLevel    *psMipLevel, 
								  GLESSubTextureInfo *psSubTexInfo,
								  PFNCopyTextureData  pfnCopyTextureData,
								  IMG_UINT32          ui32AppRowSize,
								  IMG_UINT8          *pui8AppPixels,
								  SGX_QUEUETRANSFER  *psQueueTransfer); 

IMG_BOOL HWTQTextureUpload(GLES1Context *gc,
						   GLESTexture *psTex, 
						   SGX_QUEUETRANSFER *psQueueTransfer);


IMG_BOOL PrepareHWTQTextureNormalBlit(GLES1Context       *gc, 
									  GLESTexture        *psDstTex,
									  IMG_UINT32          ui32DstOffsetInBytes,
									  GLESMipMapLevel    *psDstMipLevel,
									  GLESSubTextureInfo *psDstSubTexInfo,
									  EGLDrawableParams  *psSrcReadParams,
									  GLESSubTextureInfo *psSrcSubTexInfo,
									  SGX_QUEUETRANSFER  *psQueueTransfer);

IMG_BOOL HWTQTextureNormalBlit(GLES1Context      *gc,
							   GLESTexture       *psDstTex, 
							   EGLDrawableParams *psSrcReadParams,
							   SGX_QUEUETRANSFER *psQueueTransfer);


#if defined(GLES1_EXTENSION_PVRTC)
IMG_VOID CopyTexturePVRTC(IMG_UINT32 *pui32Dest, const IMG_UINT32 *pui32Src,
			  IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
			  IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
			  IMG_BOOL bCopySubTex);
#endif /* defined(GLES1_EXTENSION_PVRTC) */

#if defined(GLES1_EXTENSION_ETC1)
IMG_VOID CopyTextureETC1(IMG_UINT32 *pui32Dest, IMG_UINT32 *pui32Src,
			 IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
			 IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
			 IMG_BOOL bCopySubTex);
#endif /* defined(GLES1_EXTENSION_ETC1) */

#if defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888)
IMG_VOID CopyTexture8888BGRAto8888RGBA(IMG_UINT32 *pui32Dest, IMG_UINT32 *pui32Src,
				       IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
				       IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
				       IMG_BOOL bCopySubTex);
#endif /* defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888) */

IMG_VOID CopyTexture32Bits(IMG_UINT32 *pui32Dest, IMG_UINT32 *pui32Src,
			   IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
			   IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
			   IMG_BOOL bCopySubTex);

IMG_VOID CopyTexture16Bits(IMG_UINT16 *pui16Dest, IMG_UINT16 *pui16Src,
			   IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
			   IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
			   IMG_BOOL bCopySubTex);

IMG_VOID CopyTexture8Bits(IMG_UINT8 *pui8Dest, IMG_UINT8 *pui8Src,
			  IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
			  IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
			  IMG_BOOL bCopySubTex);

IMG_VOID CopyTexture5551(IMG_UINT16 *pui16Dest, IMG_UINT16 *pui16Src,
			 IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
			 IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
			 IMG_BOOL bCopySubTex);

IMG_VOID CopyTexture4444(IMG_UINT16 *pui16Dest, IMG_UINT16 *pui16Src,
			 IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
			 IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
			 IMG_BOOL bCopySubTex);

IMG_VOID CopyTexture888X(IMG_UINT8 *pui8Dest, IMG_UINT8 *pui8Src,
			 IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
			 IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
			 IMG_BOOL bCopySubTex);

IMG_VOID CopyTexture888to565(IMG_UINT16 *pui16Dest, IMG_UINT8 *pui8Src,
			     IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
			     IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
			     IMG_BOOL bCopySubTex);

IMG_VOID CopyTexture565toRGBX8888(IMG_UINT8 *pui8Dest, IMG_UINT16 *pui16Src,
				  IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
				  IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
				  IMG_BOOL bCopySubTex);

IMG_VOID CopyTexture5551to4444(IMG_UINT16 *pui16Dest, IMG_UINT16 *pui16Src, 
			       IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
			       IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
			       IMG_BOOL bCopySubTex);

#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE)
IMG_VOID CopyTexture5551toBGRA8888(IMG_UINT8 *pui8Dest, IMG_UINT16 *pui16Src,
				   IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
				   IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
				   IMG_BOOL bCopySubTex);
#endif /* defined(EGL_EXTENSION_RENDER_TO_TEXTURE) */

IMG_VOID CopyTexture5551toRGBA8888(IMG_UINT8 *pui8Dest, IMG_UINT16 *pui16Src,
				   IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
				   IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
				   IMG_BOOL bCopySubTex);

IMG_VOID CopyTexture4444to5551(IMG_UINT16 *pui16Dest, IMG_UINT16 *pui16Src,
			       IMG_UINT32 ui32Width,  IMG_UINT32 ui32Height,
			       IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
			       IMG_BOOL bCopySubTex);

#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE)
IMG_VOID CopyTexture4444toBGRA8888(IMG_UINT8 *pui8Dest, IMG_UINT16 *pui16Src,
				   IMG_UINT32 ui32Width,  IMG_UINT32 ui32Height,
				   IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
				   IMG_BOOL bCopySubTex);
#endif /* defined(EGL_EXTENSION_RENDER_TO_TEXTURE) */

IMG_VOID CopyTexture4444toRGBA8888(IMG_UINT8 *pui8Dest, IMG_UINT16 *pui16Src, 
				   IMG_UINT32 ui32Width,  IMG_UINT32 ui32Height,
				   IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
				   IMG_BOOL bCopySubTex);

IMG_VOID CopyTextureRGBA8888to5551(IMG_UINT16 *pui16Dest, IMG_UINT8 *pui8Src,
				   IMG_UINT32 ui32Width,  IMG_UINT32 ui32Height,
				   IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
				   IMG_BOOL bCopySubTex);

#if defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888)
IMG_VOID CopyTextureBGRA8888to5551(IMG_UINT16 *pui16Dest, IMG_UINT8 *pui8Src,
				   IMG_UINT32 ui32Width,  IMG_UINT32 ui32Height,
				   IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
				   IMG_BOOL bCopySubTex);
#endif /* defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888) */

IMG_VOID CopyTextureRGBA8888to4444(IMG_UINT16 *pui16Dest, IMG_UINT8 *pui8Src,
				   IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
				   IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
				   IMG_BOOL bCopySubTex);

#if defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888)
IMG_VOID CopyTextureBGRA8888to4444(IMG_UINT16 *pui16Dest, IMG_UINT8 *pui8Src,
				   IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
				   IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
				   IMG_BOOL bCopySubTex);
#endif /* defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888) */




#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE)
IMG_BOOL GLESBindTexImage(EGLContextHandle hContext, EGLDrawableHandle hSurface, EGLDrawableHandle *hTexture);
IMG_VOID GLESReleaseTexImage(EGLContextHandle hContext, EGLDrawableHandle hSurface, EGLDrawableHandle *hTexture);
IMG_BOOL TextureCreatePBufferLevel(GLES1Context *gc, GLESTexture *psTex);
#endif /* defined(EGL_EXTENSION_RENDER_TO_TEXTURE) */

#if defined(GLES1_EXTENSION_EGL_IMAGE)
IMG_BOOL TextureCreateImageLevel(GLES1Context *gc, GLESTexture *psTex);
IMG_VOID ReleaseImageFromTexture(GLES1Context *gc, GLESTexture *psTex);
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */

#endif /* _TEXTURE_ */

/******************************************************************************
 End of file (texture.h)
******************************************************************************/
