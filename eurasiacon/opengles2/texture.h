/******************************************************************************
 * Name         : texture.h
 * Author       : BCB
 * Created      : 19/12/2005
 *
 * Copyright    : 2005-2007 by Imagination Technologies Limited.
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

#define GLES2_TEX_INCONSISTENT				0
#define GLES2_TEX_CONSISTENT				1
#define GLES2_TEX_UNKNOWN					2
#define GLES2_TEX_DUMMY						3


#define GLES2_ALPHA_TEX_INDEX				0
#define GLES2_LUMINANCE_TEX_INDEX			1
#define GLES2_LUMINANCE_ALPHA_TEX_INDEX		2
#define GLES2_RGB_TEX_INDEX					3
#define GLES2_RGBA_TEX_INDEX				4

#if defined(GLES2_EXTENSION_DEPTH_TEXTURE)
#define GLES2_DEPTH_TEX_INDEX				5
#endif /* defined(GLES2_EXTENSION_DEPTH_TEXTURE) */

#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
#define GLES2_DEPTH_STENCIL_TEX_INDEX		6
#endif /* defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */

#define GLES2_TEXTURE_TARGET_2D				0
#define GLES2_TEXTURE_TARGET_CEM			1
#define GLES2_TEXTURE_TARGET_STREAM			2
#define GLES2_TEXTURE_TARGET_MAX			3

#define GLES2_TEXTURE_CEM_FACE_POSX			0
#define GLES2_TEXTURE_CEM_FACE_NEGX			1
#define GLES2_TEXTURE_CEM_FACE_POSY			2
#define GLES2_TEXTURE_CEM_FACE_NEGY			3
#define GLES2_TEXTURE_CEM_FACE_POSZ			4
#define GLES2_TEXTURE_CEM_FACE_NEGZ			5

#define GLES2_TEXTURE_CEM_FACE_MAX			6

#define GLES2_LOADED_LEVEL					((IMG_VOID *)0xFFFFFFFF)

#define GLES2_GLSL_PERM_TEXTURE_UNIT		GLES2_MAX_TEXTURE_UNITS
#define GLES2_GLSL_GRAD_TEXTURE_UNIT		(GLES2_MAX_TEXTURE_UNITS + 1)

#define GLES2_IS_PERM_TEXTURE_UNIT(a)		(a == GLES2_GLSL_PERM_TEXTURE_UNIT)
#define GLES2_IS_GRAD_TEXTURE_UNIT(a)		(a == GLES2_GLSL_GRAD_TEXTURE_UNIT)

#define GLES2_MIPMAP						0x00000001
#define GLES2_NONPOW2						0x00000002
#define GLES2_COMPRESSED					0x00000004
#define GLES2_MULTICHUNK					0x00000008
#define GLES2_FLOAT							0x00000010

/* Forward declaration */
struct GLES2TextureRec;



/*
** Per Mipmap Level state.
*/
typedef struct GLES2MipMapLevelRec
{
	/* This struct must be the first variable                     */
	GLES2FrameBufferAttachable sFBAttachable;

	/* These are the texels for this mipmap level                 */
	IMG_UINT8                  *pui8Buffer;

	/* Image dimensions                                           */
	IMG_UINT32                 ui32Width, ui32Height, ui32ImageSize;

	/* log2 of width & height                                     */
	IMG_UINT32                 ui32WidthLog2, ui32HeightLog2;

	/* Requested internal format                                  */
	GLenum                     eRequestedFormat;

	/* Actual texel format                                        */ 
	const GLES2TextureFormat  *psTexFormat;

	/* Texture this mipmap belongs to                             */
	struct GLES2TextureRec    *psTex;

	/* The level of this mipmap in the parent texture             */
	IMG_UINT32                 ui32Level;

} GLES2MipMapLevel;



/* 
** Sub texture info used for HWTQ functions.
*/
typedef struct GLES2SubTextureInfoRec
{
    IMG_UINT32         ui32SubTexXoffset;
    IMG_UINT32         ui32SubTexYoffset;

    IMG_UINT32         ui32SubTexWidth;
    IMG_UINT32         ui32SubTexHeight;

    IMG_UINT8         *pui8SubTexBuffer;

} GLES2SubTextureInfo;



/*
** State set with glTexParameter
*/
typedef struct GLES2TextureParamStateRec
{
	/* Hardware control words */
	IMG_UINT32 ui32StateWord0;
	IMG_UINT32 aui32StateWord1[GLES2_MAX_CHUNKS];
	IMG_UINT32 aui32StateWord2[GLES2_MAX_CHUNKS];

	/* Minification and magnification filters */
	IMG_UINT32 ui32MinFilter;
	IMG_UINT32 ui32MagFilter;

    IMG_BOOL   bAutoMipGen;

} GLES2TextureParamState;


typedef struct GLES2TextureUnitStateRec
{
	/* Per texture object binding state */
	GLES2TextureParamState *psTexture[GLES2_TEXTURE_TARGET_MAX];

} GLES2TextureUnitState;


/*
** Per Texture Object state.
*/
typedef struct GLES2TextureRec
{
	/* This struct must be the first variable */
	GLES2NamedItem           sNamedItem;

	/* Textures are frame resources */
	KRMResource       		 sResource;

	/* State for this texture object */
	GLES2TextureParamState   sState;

	/* Target that this texture was created on */
	IMG_UINT32               ui32TextureTarget;

	/* Level information */
	IMG_UINT32               ui32NumLevels;
	GLES2MipMapLevel         *psMipLevel;

	/* Number of mipmap levels that are render targets */
	IMG_UINT32               ui32NumRenderTargets;

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
	 * levelsConsistent is updated everytime a level in the texture
	 * changes size and summarises whether the texture can be sensibly
	 * MIP mapped.
	 */
	IMG_UINT32                ui32LevelsConsistent; 

	const GLES2TextureFormat *psFormat;
	IMG_UINT32                ui32HWFlags;
	IMG_UINT32                ui32ChunkSize;


	PVRSRV_CLIENT_MEM_INFO   *psMemInfo;

	IMG_VOID (*pfnReadBackData)(IMG_VOID *pvDest, const IMG_VOID *pvSrc,
								IMG_UINT32 ui32Log2Width, IMG_UINT32 ui32Log2Height,
								IMG_UINT32 ui32Width, IMG_UINT32 ui32Height, 
								IMG_UINT32 ui32DstStride);

#if defined(PDUMP)
	/* The i-th bit marks whether the i-th mipmap level must be dumped.
	 * In a C.E.M. it marks the i-th level of every face.
	 */
	IMG_UINT32 ui32LevelsToDump;
#endif

#if defined(GLES2_EXTENSION_EGL_IMAGE)
	EGLImage *psEGLImageSource;
	EGLImage *psEGLImageTarget;
#if defined(GLES2_EXTENSION_EGL_IMAGE_EXTERNAL)
	GLES2ExternalTexState *psExtTexState;
#endif /* defined(GLES2_EXTENSION_EGL_IMGE_EXTERNAL) */
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */

#if defined(GLES2_EXTENSION_TEXTURE_STREAM)
	GLES2StreamDevice *psBufferDevice;
	IMG_UINT32 ui32BufferOffset;
#endif /* defined(GLES2_EXTENSION_TEXTURE_STREAM) */


} GLES2Texture;


/*
 * Ghosted texture.
 */
typedef struct GLES2GhostRec
{
	/* Ghosts are frame resources */
	KRMResource sResource;

	PVRSRV_CLIENT_MEM_INFO *psMemInfo;
	IMG_UINT32             ui32Size;
#if defined(GLES2_EXTENSION_EGL_IMAGE)
	IMG_VOID *hImage;
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */
#if defined(GLES2_EXTENSION_TEXTURE_STREAM)
    IMG_VOID *hBufferDevice;
#endif /* defined(GLES2_EXTENSION_TEXTURE_STREAM) */
#if defined PDUMP
	/* Whether this ghost has been dumped or not */
	IMG_BOOL bDumped;
#endif

} GLES2Ghost;


/*
 * Records state of textures; whether they're idle, active, ghosted etc.
 */
typedef struct GLES2TextureManagerRec
{
	IMG_UINT32 ui32GhostMem;

	/* Frame resource manager for textures */
	KRMKickResourceManager sKRM;

	/* Dummy textures used when some of the textures are inconsistent or could not be made resident */
	PVRSRV_CLIENT_MEM_INFO *psWhiteDummyTexture;
	PVRSRV_CLIENT_MEM_INFO *psBlackDummyTexture;

} GLES2TextureManager;

typedef IMG_VOID (*PFNCopyTextureData)(IMG_VOID *, const IMG_VOID *, IMG_UINT32,
				       IMG_UINT32, IMG_UINT32, GLES2MipMapLevel *,
				       IMG_BOOL);


/*
** Per Context rendering state.
*/
typedef struct GLES2TextureMachineRec
{
	/* Texture object for the default texture */
	GLES2Texture        *psDefaultTexture[GLES2_TEXTURE_TARGET_MAX];

	/* Currently bound texture object */
	GLES2Texture        *apsBoundTexture[GLES2_MAX_TEXTURE_UNITS][GLES2_TEXTURE_TARGET_MAX];

	IMG_BOOL            abTexUnitValid[GLES2_MAX_TEXTURE_UNITS];

	/* Early initialization function for texture manager */
	IMG_VOID (*pfnInitTextureManager)(GLES2Context *gc);

} GLES2TextureMachine;


typedef struct GLES2CompiledTextureState_TAG
{
	/* Bit field indicating which image units are enabled */
	IMG_UINT32 ui32ImageUnitEnables;

	/* An array which contains information when we setup HW state. */
	IMG_UINT32 aui32TAGControlWord[GLES2_MAX_TEXTURE_UNITS * PDS_NUM_TEXTURE_IMAGE_CHUNKS][EURASIA_TAG_TEXTURE_STATE_SIZE];
	IMG_UINT32 aui32ChunkCount[GLES2_MAX_TEXTURE_UNITS];

	PDS_TEXTURE_IMAGE_UNIT *psTextureImageChunks;
	IMG_UINT32 ui32NumCompiledImageUnits;

	/* An an input when patching to final HW code */
	const GLES2TextureFormat *apsTexFormat[GLES2_MAX_TEXTURE_UNITS];

	/* Whether any of the active textures has ever been ghosted */
	IMG_BOOL bSomeTexturesWereGhosted;

/* PRQA S 3332 1 */ /* Override QAC suggestion and use this macro. */

} GLES2CompiledTextureState;

IMG_VOID TextureUpload(GLES2Texture *psTex, GLES2MipMapLevel *psMipLevel, IMG_UINT32 ui32OffsetInBytes, GLES2TextureFormat *psTexFmt,
	IMG_UINT32 ui32Face, IMG_UINT32 ui32Lod, IMG_UINT32 ui32TopUsize, IMG_UINT32 ui32TopVsize);

IMG_VOID TranslateLevel(GLES2Context *gc, GLES2Texture *psTex, IMG_UINT32 ui32Face, IMG_UINT32 ui32Lod);
IMG_UINT32 GetMipMapOffset(IMG_UINT32 ui32MapLevel, IMG_UINT32 ui32TopUSize, IMG_UINT32 ui32TopVSize);
IMG_UINT32 GetCompressedMipMapOffset(IMG_UINT32 ui32MapLevel, IMG_UINT32 ui32TopUSize,
									 IMG_UINT32 ui32TopVSize, IMG_BOOL bIs2Bpp);

IMG_UINT32 GetNPOTMipMapOffset(IMG_UINT32 ui32MapLevel, GLES2Texture *psTex);

#define MIN_POW2_MIPLEVEL_SIZE 8

IMG_BOOL BindTexture(GLES2Context *gc, IMG_UINT32 ui32Unit, IMG_UINT32 ui32Target, IMG_UINT32 ui32Texture);
GLES2Texture * CreateTexture(GLES2Context *gc, IMG_UINT32 ui32Name, IMG_UINT32 ui32Target);
IMG_VOID SetupDefaultTexFormat(GLES2Context *gc);
IMG_UINT8 * TextureCreateLevel(GLES2Context *gc, GLES2Texture *psTex, IMG_UINT32 ui32Level, GLenum eInternalFormat, 
							   const GLES2TextureFormat *psTexFormat, IMG_UINT32 ui32Width, IMG_UINT32 ui32Height);

IMG_VOID ReadBackTextureData(GLES2Context *gc, GLES2Texture *psTex, IMG_UINT32 ui32Face, IMG_UINT32 ui32Lod, IMG_VOID *pvBuffer);
IMG_VOID ReadBackTiledData(IMG_VOID *pvDest, const IMG_VOID *pvSrc,
						   IMG_UINT32 ui32Width, IMG_UINT32 ui32Height, 
						   const GLES2Texture *psTex);
IMG_VOID TextureRemoveResident(GLES2Context *gc, GLES2Texture *psTex);
typedef IMG_VOID (*PFNReadSpan)(const GLES2PixelSpanInfo *);

IMG_BOOL TextureMakeResident(GLES2Context *gc, GLES2Texture *psTex);
IMG_UINT32 IsTextureConsistent(GLES2Context *gc, GLES2Texture *psTex, IMG_UINT32 ui32OverloadTexLayout, IMG_BOOL bCheckForRenderingLoop);


IMG_BOOL SetupReadPixelsSpanInfo(GLES2Context *gc, GLES2PixelSpanInfo *psSpanInfo,
								 IMG_INT32 i32X, IMG_INT32 i32Y, IMG_UINT32 ui32Width,
								 IMG_UINT32 ui32Height, GLenum format, GLenum type,
								 IMG_BOOL bUsePackAlignment, EGLDrawableParams *psReadParams);
IMG_BOOL ClipReadPixels(GLES2PixelSpanInfo *psSpanInfo, EGLDrawableParams *psReadParams);

IMG_VOID *GetStridedSurfaceData(GLES2Context *gc, EGLDrawableParams *psReadParams, GLES2PixelSpanInfo *psSpanInfo);


IMG_VOID SetupTexNameArray(GLES2NamesArray *psNamesArray);
IMG_VOID SetupTwiddleFns(GLES2Texture *psTex);

IMG_BOOL CreateTextureState(GLES2Context *gc);
IMG_BOOL FreeTextureState(GLES2Context *gc);
IMG_VOID SetupTextureState(GLES2Context *gc);

IMG_VOID CopyTextureData(GLES2Context *gc, 
						 GLES2Texture           *psDstTex,  
						 IMG_UINT32              ui32DstOffsetInBytes,
						 PVRSRV_CLIENT_MEM_INFO *psSrcInfo,
						 IMG_UINT32              ui32SrcOffsetInBytes,
						 IMG_UINT32              ui32SizeInBytes);

GLES2TextureManager * CreateTextureManager(GLES2Context *gc);
IMG_VOID ReleaseTextureManager(GLES2Context *gc, GLES2TextureManager *psTexMgr);

IMG_VOID MakeTextureMipmapLevelsSoftware(GLES2Context *gc, GLES2Texture *psTex, IMG_UINT32 ui32Face, IMG_UINT32 ui32MaxFace, IMG_BOOL bIsNonPow2);
IMG_BOOL MakeTextureMipmapLevels(GLES2Context *gc, GLES2Texture *psTex, IMG_UINT32 ui32TexTarget);

IMG_BOOL TexMgrGhostTexture(GLES2Context *gc, GLES2Texture *psTex);

IMG_BOOL SetupTextureRenderTargetControlWords(GLES2Context *gc, GLES2Texture *psTex);


#if defined PDUMP
IMG_VOID PDumpTexture(GLES2Context *gc, GLES2Texture *psTex);
#endif /* PDUMP */

#if defined(GLES2_EXTENSION_EGL_IMAGE)
IMG_BOOL TextureCreateImageLevel(GLES2Context *gc, GLES2Texture *psTex);
IMG_VOID ReleaseImageFromTexture(GLES2Context *gc, GLES2Texture *psTex);
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */

IMG_BOOL CreateTextureMemory(GLES2Context *gc, GLES2Texture *psTex);


IMG_BOOL UnloadInconsistentTexture(GLES2Context *gc, GLES2Texture *psTex);


IMG_BOOL PrepareHWTQTextureUpload(GLES2Context        *gc, 
								  GLES2Texture        *psTex,
								  IMG_UINT32           ui32OffsetInBytes,
								  GLES2MipMapLevel    *psMipLevel, 
								  GLES2SubTextureInfo *psSubTexInfo,
								  PFNCopyTextureData   pfnCopyTextureData,
								  IMG_UINT32           ui32AppRowSize,
								  const IMG_UINT8     *pui8AppPixels,
								  SGX_QUEUETRANSFER   *psQueueTransfer); 

IMG_BOOL HWTQTextureUpload(GLES2Context *gc,
						   GLES2Texture *psTex, 
						   SGX_QUEUETRANSFER *psQueueTransfer);


IMG_BOOL PrepareHWTQTextureNormalBlit(GLES2Context        *gc, 
									  GLES2Texture        *psDstTex,
									  IMG_UINT32           ui32DstOffsetInBytes,
									  GLES2MipMapLevel    *psDstMipLevel,
									  GLES2SubTextureInfo *psDstSubTexInfo,
									  EGLDrawableParams   *psSrcReadParams,
									  GLES2SubTextureInfo *psSrcSubTexInfo,
									  SGX_QUEUETRANSFER   *psQueueTransfer);

IMG_BOOL HWTQTextureNormalBlit(GLES2Context      *gc,
							   GLES2Texture      *psDstTex, 
							   EGLDrawableParams *psSrcReadParams,
							   SGX_QUEUETRANSFER *psQueueTransfer);


#if defined(GLES2_EXTENSION_FLOAT_TEXTURE)
IMG_VOID CopyTextureFloatRGBA(IMG_UINT32 *pui32Dest, const IMG_UINT32 *pui32Src,
			      IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
			      IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
			      IMG_BOOL bCopySubTex);

IMG_VOID CopyTextureFloatRGB(IMG_UINT32 *pui32Dest, const IMG_UINT32 *pui32Src,
			     IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
			     IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
			     IMG_BOOL bCopySubTex);

IMG_VOID CopyTextureFloatLA(IMG_UINT32 *pui32Dest, const IMG_UINT32 *pui32Src,
			    IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
			    IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
			    IMG_BOOL bCopySubTex);
#endif /*GLES2_EXTENSION_FLOAT_TEXTURE */ 

#if defined(GLES2_EXTENSION_HALF_FLOAT_TEXTURE)
IMG_VOID CopyTextureHalfFloatRGBA(IMG_UINT16 *pui16Dest, const IMG_UINT16 *pui16Src,
				  IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
				  IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
				  IMG_BOOL bCopySubTex);

IMG_VOID CopyTextureHalfFloatRGB(IMG_UINT16 *pui16Dest, const IMG_UINT16 *pui16Src, 
				 IMG_UINT32 ui32Width, IMG_UINT32 ui32Height, 
				 IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
				 IMG_BOOL bCopySubTex);
#endif /* GLES2_EXTENSION_HALF_FLOAT_TEXTURE */

#if defined(GLES2_EXTENSION_TEXTURE_FORMAT_BGRA8888)
IMG_VOID CopyTextureBGRA8888toRGBA8888(IMG_UINT32 *pui32Dest, const IMG_UINT32 *pui32Src,
										IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
										IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
										IMG_BOOL bCopySubTex);

IMG_VOID CopyTextureBGRA8888to5551(IMG_UINT16 *pui16Dest, const IMG_UINT8 *pui8Src,
												IMG_UINT32 ui32Width,  IMG_UINT32 ui32Height,
												IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
												IMG_BOOL bCopySubTex);

IMG_VOID CopyTextureBGRA8888to4444(IMG_UINT16 *pui16Dest, const IMG_UINT8 *pui8Src,
												IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
												IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
												IMG_BOOL bCopySubTex);

#endif /* GLES2_EXTENSION_TEXTURE_FORMAT_BGRA8888 */


IMG_VOID CopyTexture32Bits(IMG_UINT32 *pui32Dest, const IMG_UINT32 *pui32Src,
			   IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
			   IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
			   IMG_BOOL bCopySubTex);

IMG_VOID CopyTexture16Bits(IMG_UINT16 *pui16Dest, const IMG_UINT16 *pui16Src,
			   IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
			   IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
			   IMG_BOOL bCopySubTex);

IMG_VOID CopyTexture8Bits(IMG_UINT8 *pui8Dest, const IMG_UINT8 *pui8Src,
			  IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
			  IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
			  IMG_BOOL bCopySubTex);

IMG_VOID CopyTexture5551(IMG_UINT16 *pui16Dest, const IMG_UINT16 *pui16Src,
			 IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
			 IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
			 IMG_BOOL bCopySubTex);

IMG_VOID CopyTexture4444(IMG_UINT16 *pui16Dest, const IMG_UINT16 *pui16Src,
			 IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
			 IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
			 IMG_BOOL bCopySubTex);

IMG_VOID CopyTexture888X(IMG_UINT8 *pui8Dest, const IMG_UINT8 *pui8Src,
			 IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
			 IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
			 IMG_BOOL bCopySubTex);

IMG_VOID CopyTexture888to565(IMG_UINT16 *pui16Dest, const IMG_UINT8 *pui8Src,
			     IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
			     IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
			     IMG_BOOL bCopySubTex);

IMG_VOID CopyTexture565toRGBX8888(IMG_UINT8 *pui8Dest, const IMG_UINT16 *pui16Src,
				  IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
				  IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
				  IMG_BOOL bCopySubTex);

IMG_VOID CopyTexture5551to4444(IMG_UINT16 *pui16Dest, const IMG_UINT16 *pui16Src, 
			       IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
			       IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
			       IMG_BOOL bCopySubTex);

IMG_VOID CopyTexture5551toBGRA8888(IMG_UINT8 *pui8Dest, const IMG_UINT16 *pui16Src,
				   IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
				   IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
				   IMG_BOOL bCopySubTex);

IMG_VOID CopyTexture5551toRGBA8888(IMG_UINT8 *pui8Dest, const IMG_UINT16 *pui16Src,
				   IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
				   IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
				   IMG_BOOL bCopySubTex);

IMG_VOID CopyTexture4444to5551(IMG_UINT16 *pui16Dest, const IMG_UINT16 *pui16Src,
			       IMG_UINT32 ui32Width,  IMG_UINT32 ui32Height,
			       IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
			       IMG_BOOL bCopySubTex);

IMG_VOID CopyTexture4444toRGBA8888(IMG_UINT8 *pui8Dest, const IMG_UINT16 *pui16Src, 
				   IMG_UINT32 ui32Width,  IMG_UINT32 ui32Height,
				   IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
				   IMG_BOOL bCopySubTex);

IMG_VOID CopyTextureRGBA8888to5551(IMG_UINT16 *pui16Dest, const IMG_UINT8 *pui8Src,
				   IMG_UINT32 ui32Width,  IMG_UINT32 ui32Height,
				   IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
				   IMG_BOOL bCopySubTex);

IMG_VOID CopyTextureRGBA8888to4444(IMG_UINT16 *pui16Dest, const IMG_UINT8 *pui8Src,
				   IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
				   IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
				   IMG_BOOL bCopySubTex);



#endif /* _TEXTURE_ */

/******************************************************************************
 End of file (texture.h)
******************************************************************************/

