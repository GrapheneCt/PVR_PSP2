/******************************************************************************
 * Name         : validate.h
 *
 * Copyright    : 2006-2008 by Imagination Technologies Limited.
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
 * $Log: validate.h $
 *****************************************************************************/

#ifndef _VALIDATE_
#define _VALIDATE_



/******************************************************************************
	Validation/dirty flags
******************************************************************************/
#define GLES1_DIRTYFLAG_RENDERSTATE				0x00000001UL
#define GLES1_DIRTYFLAG_ATTRIB_STREAM			0x00000002UL
#define GLES1_DIRTYFLAG_ATTRIB_POINTER			0x00000004UL
#define GLES1_DIRTYFLAG_VERTPROG_CONSTANTS		0x00000008UL
#define GLES1_DIRTYFLAG_FRAGPROG_CONSTANTS		0x00000010UL
#define GLES1_DIRTYFLAG_TEXTURE_STATE			0x00000020UL
#define GLES1_DIRTYFLAG_VP_STATE				0x00000040UL
#define GLES1_DIRTYFLAG_FP_STATE				0x00000080UL
#define GLES1_DIRTYFLAG_VERTEX_PROGRAM			0x00000100UL
#define GLES1_DIRTYFLAG_FRAGMENT_PROGRAM		0x00000200UL
#define GLES1_DIRTYFLAG_POINT_LINE_WIDTH		0x00000400UL
#define GLES1_DIRTYFLAG_DMS_INFO				0x00000800UL
#define GLES1_DIRTYFLAG_CLIENT_STATE			0x00001000UL
#define GLES1_DIRTYFLAG_PRIM_INDEXBITS			0x00002000UL


#define GLES1_DIRTYFLAG_VAO_BINDING             0x00100000UL
#define GLES1_DIRTYFLAG_VAO_CLIENT_STATE	    0x00200000UL
#define GLES1_DIRTYFLAG_VAO_ATTRIB_STREAM       0x00400000UL
#define GLES1_DIRTYFLAG_VAO_ATTRIB_POINTER      0x00800000UL
#define GLES1_DIRTYFLAG_VAO_ELEMENT_BUFFER      0x01000000UL
#define GLES1_DIRTYFLAG_VAO_ALL                 0x01F00000UL


#define GLES1_DIRTYFLAG_ALL					    0x01F03FFFUL


/******************************************************************************
	Emit flags
******************************************************************************/
/*
#define GLES1_EMITSTATE_PDS_FRAGMENT_PROGRAM			0x00000001UL
#define GLES1_EMITSTATE_PDS_FRAGMENT_SECONDARY_PROGRAM	0x00000002UL
*/
#define GLES1_EMITSTATE_PDS_FRAGMENT_STATE				0x00000004UL
#define GLES1_EMITSTATE_PDS_FRAGMENT_SECONDARY_STATE	0x00000008UL

/*
#define GLES1_EMITSTATE_PDS_VERTEX_PROGRAM				0x00000010UL
#define GLES1_EMITSTATE_PDS_VERTEX_SECONDARY_PROGRAM	0x00000020UL
*/
#define GLES1_EMITSTATE_PDS_VERTEX_SECONDARY_STATE		0x00000040UL

#define GLES1_EMITSTATE_MTE_STATE_REGION_CLIP			0x00000080UL
#define GLES1_EMITSTATE_MTE_STATE_VIEWPORT				0x00000100UL
#define GLES1_EMITSTATE_MTE_STATE_TAMISC				0x00000200UL
#define GLES1_EMITSTATE_MTE_STATE_CONTROL				0x00000400UL
#define GLES1_EMITSTATE_MTE_STATE_ISP					0x00000800UL
#define GLES1_EMITSTATE_MTE_STATE_OUTPUTSELECTS			0x00001000UL
#define GLES1_EMITSTATE_STATEUPDATE						0x00002000UL

#define GLES1_EMITSTATE_ALL								0x00003FFFUL


/*****************************************************************************/
/*****************************************************************************/


#define GLES1_EXTRACT(Data, Field)	((Data & ~EURASIA_ ##Field ##_CLRMSK) >> EURASIA_ ##Field ##_SHIFT)
#define ALIGNCOUNT(X, Y)	(((X) + ((Y) - 1)) & ~((Y) - 1))
#define ALIGNCOUNTINBLOCKS(Size, Log2OfBlockSsize)	(((Size) + ((1UL << (Log2OfBlockSsize)) - 1)) >> (Log2OfBlockSsize))

#define EURASIA_PDS_SECATTRSIZE_ALIGNSHIFTINREGISTERS			(EURASIA_PDS_SECATTRSIZE_ALIGNSHIFT - 2)
#define EURASIA_VDMPDS_USEATTRIBUTESIZE_ALIGNSHIFTINREGISTERS	(EURASIA_VDMPDS_USEATTRIBUTESIZE_ALIGNSHIFT - 2)
#define EURASIA_VDMPDS_USEATTRIBUTESIZE_ALIGNINREGISTERS		(1UL << EURASIA_VDMPDS_USEATTRIBUTESIZE_ALIGNSHIFTINREGISTERS)

#define EURASIA_TAPDSSTATE_USEATTRIBUTESIZE_ALIGNSHIFTINREGISTERS (EURASIA_TAPDSSTATE_USEATTRIBUTESIZE_ALIGNSHIFT - 2)

#define GLES1_MAX_MTE_STATE_DWORDS						(23 + 1) /* +1 adding header dword */

#define EURASIA_NUM_PDS_VERTEX_PROGRAM_CONSTANTS		4
#if defined(FIX_HW_BRN_25339) || defined(FIX_HW_BRN_27330)
#define EURASIA_NUM_PDS_STATE_PROGRAM_DWORDS			(16 + 5)
#define	EURASIA_NUM_PDS_VERTEX_PROGRAM_INSTRUCTIONS		22
#else
#define EURASIA_NUM_PDS_STATE_PROGRAM_DWORDS			(12 + 4)
#define	EURASIA_NUM_PDS_VERTEX_PROGRAM_INSTRUCTIONS		19
#endif
#define EURASIA_NUM_PDS_VERTEX_PROGRAM_DWORDS			(GLES1_MAX_NUMBER_OF_VERTEX_ATTRIBS * (EURASIA_NUM_PDS_VERTEX_PROGRAM_CONSTANTS + EURASIA_NUM_PDS_VERTEX_PROGRAM_INSTRUCTIONS) + 1 + 1)

#define GLES1_STATIC_PDS_NUMBER_OF_STREAMS  8

#define PDS_AUXILIARY_PROG_SIZE			(PDS_AUXILIARYVERTEX_SIZE >> 2)
#define ALIGNED_PDS_AUXILIARY_PROG_SIZE (PDS_AUXILIARY_PROG_SIZE + (EURASIA_VDMPDS_BASEADDR_CACHEALIGN - PDS_AUXILIARY_PROG_SIZE % EURASIA_VDMPDS_BASEADDR_CACHEALIGN))

#define GLES1_MTE_COPY_PROG_SIZE		 (PDS_MTESTATECOPY_SIZE >> 2)
#define GLES1_ALIGNED_MTE_COPY_PROG_SIZE (GLES1_MTE_COPY_PROG_SIZE + (EURASIA_VDMPDS_BASEADDR_CACHEALIGN - GLES1_MTE_COPY_PROG_SIZE % EURASIA_VDMPDS_BASEADDR_CACHEALIGN))    

/*
** 2 alpha test + 1 fog + 1 special texturing constant + NUM_TEX_UNITS * (1 env colour + 1 scaling value)
*/

#define MAX_PIXEL_SA_COUNT (2 + 1 + 1 + GLES1_MAX_TEXTURE_UNITS * (1 + 1))

/* Granularity in number of registers of DMS attribute allocation */
#define GLES1_BLOCK_SIZE_IN_PIXELS		4

#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
/* Allow for 2 units per stream (2 planar) */
#define GLES1_MAX_TEXTURE_IMAGE_UNITS	(GLES1_MAX_TEXTURE_UNITS * 2)
#else
#define GLES1_MAX_TEXTURE_IMAGE_UNITS	GLES1_MAX_TEXTURE_UNITS
#endif

#if defined(SGX_FEATURE_SW_VDM_CONTEXT_SWITCH)
typedef struct SmallKickTAData_TAG
{
	/* Number of indices and number of primitives sent this TA kick */
	IMG_UINT32	ui32NumIndicesThisTA;
	IMG_UINT32	ui32NumPrimitivesThisTA;

	/* Number of indices and number of primitives sent this frame */
	IMG_UINT32	ui32NumIndicesThisFrame;
	IMG_UINT32	ui32NumPrimitivesThisFrame;

	/* MGS Calculated kick threshold */
	IMG_UINT32	ui32KickThreshold;

} SmallKickTAData;
#endif

typedef struct PDSFragmentSAInfoHashData_TAG
{
	IMG_UINT32 ui32AttribCount;
	IMG_UINT32 ui32SAOffset;
	IMG_UINT32 aui32AttribData[MAX_PIXEL_SA_COUNT];

} PDSFragmentSAInfoHashData;

typedef struct PDSFragmentSAInfo_TAG
{
	IMG_DEV_VIRTADDR uPDSUSEShaderSABaseAddr;
	IMG_UINT32		 ui32FragmentPDSSecAttribDataSize;

	UCH_UseCodeBlock *psCodeBlock;

} PDSFragmentSAInfo;


struct GLES1PDSCodeVariant_TAG
{
	/* PDS variant are TA-kick resources */
	KRMResource sResource;

#ifdef PDUMP
	/* Has this block been pdumped since it was last changed. */
	IMG_BOOL bDumped;
#endif /* PDUMP */

	UCH_UseCodeBlock	*psCodeBlock;

	/* Size of the data memory. */
	IMG_UINT32 ui32DataSize;

	IMG_UINT32 *pui32HashCompare;
	IMG_UINT32 ui32HashCompareSizeInDWords;

	HashValue tHashValue;
	/* USE variant that this PDS variant was compiled from (if one exists). */
	GLES1ShaderVariant *psUSEVariant;

	/* Next variant in the list. */
	struct GLES1PDSCodeVariant_TAG *psNext;
};

typedef struct GLES1PDSVertexCodeVariant_TAG
{
	/* Pointer to PDS vertex program heap */
	UCH_UseCodeBlock	*psCodeBlock;

	/* Size of the data memory and program address. */
	IMG_UINT32 ui32DataSize;
	IMG_DEV_VIRTADDR uProgramAddress;

	/* stride */
	IMG_UINT32 aui32Stride[GLES1_MAX_ATTRIBS_ARRAY];

	IMG_BOOL b32BitPDSIndices;

	/* Next variant in the list. */
	struct GLES1PDSVertexCodeVariant_TAG *psNext;

}GLES1PDSVertexCodeVariant;

typedef enum GLES1PrimType_TAG
{
	GLES1_PRIMTYPE_POINT			= GL_POINTS,
	GLES1_PRIMTYPE_LINE				= GL_LINES,
	GLES1_PRIMTYPE_LINE_LOOP		= GL_LINE_LOOP,
	GLES1_PRIMTYPE_LINE_STRIP		= GL_LINE_STRIP,
	GLES1_PRIMTYPE_TRIANGLE			= GL_TRIANGLES,
	GLES1_PRIMTYPE_TRIANGLE_STRIP	= GL_TRIANGLE_STRIP,
	GLES1_PRIMTYPE_TRIANGLE_FAN		= GL_TRIANGLE_FAN,
	GLES1_PRIMTYPE_SPRITE			= 7,
	GLES1_PRIMTYPE_DRAWTEXTURE		= 8,

} GLES1PrimType;

#define GLES1_PRIMTYPE_MAX			(GLES1_PRIMTYPE_DRAWTEXTURE + 1)

typedef struct GLES1PDSVertexState_TAG
{
	/* Is this stream merged with the next */
	IMG_BOOL   bMerged[GLES1_MAX_ATTRIBS_ARRAY];

	PDS_VERTEX_SHADER_PROGRAM_INFO sProgramInfo;
	IMG_UINT32 aui32LastPDSProgram[EURASIA_NUM_PDS_VERTEX_PROGRAM_DWORDS];
	IMG_UINT32 ui32ProgramSize;

} GLES1PDSVertexState;


#define GLES1_ALPHA_TEST_ONLY			(1 << 0)
#define GLES1_ALPHA_TEST_FEEDBACK		(1 << 1)
#define GLES1_ALPHA_TEST_WAS_TWD		(1 << 2)

#if defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728)
#define GLES1_ALPHA_TEST_BRNFIX_DEPTHF	(1 << 3)
#endif /* defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728) */


typedef struct GLESCompiledRenderState_TAG
{
	/* ISP State */
	IMG_UINT32	ui32ISPControlWordA;				/* Stencil masks, stencil operations */
	IMG_UINT32	ui32ISPControlWordB;				/* Stencil ref, object type (tri, line etc.), Tag & Depth write 
												   	   disables, depth compare mode, pass type */
	IMG_UINT32	ui32ISPControlWordC;				/* Visibility test register, user pass-spawn, Depth bias 
												   	   units & factor, PS register/block size control, valid id */

    /* Second set of ISP control, for 2-sided stencil etc */
    IMG_UINT32      ui32BFISPControlWordA;
    IMG_UINT32      ui32BFISPControlWordB;
    IMG_UINT32      ui32BFISPControlWordC;

	/* MTE control word */
	IMG_UINT32			ui32MTEControl;

	/* Logical ops */
	IMG_BOOL			bNeedLogicOpsCode;
	IMG_BOOL			bNeedFBBlendCode;
	IMG_BOOL			bNeedColorMaskCode;

	/* Alphatest  */
	IMG_UINT32			ui32AlphaTestFlags;
	IMG_UINT32			ui32ISPFeedback0;	/* State sent back to ISP  */
	IMG_UINT32			ui32ISPFeedback1;	/* from USE for alpha test. */ 

} GLESCompiledRenderState;


typedef struct GLESShaderTextureState_TAG
{
	/* An array which contains information when we setup HW state. */
	PDS_TEXTURE_IMAGE_UNIT asTextureImageUnits[GLES1_MAX_TEXTURE_IMAGE_UNITS];	

	/* Whether any of the active textures has ever been ghosted */
	IMG_BOOL bSomeTexturesWereGhosted;
	
} GLESShaderTextureState;

typedef struct GLESPrimitiveMachine_TAG
{
	GLESCompiledRenderState		sRenderState;
	GLESShaderTextureState		sTextureState;
	
	GLES1PDSVertexState			*psPDSVertexState;

	UCH_UseCodeBlock			*psHWBGCodeBlock;
	UCH_UseCodeBlock			*psClearVertexCodeBlock;
	UCH_UseCodeBlock			*psClearFragmentCodeBlock;
	UCH_UseCodeBlock			*psAccumVertexCodeBlock;
	UCH_UseCodeBlock			*psScissorVertexCodeBlock;
	UCH_UseCodeBlock			*psDummyPixelSecondaryPDSCode;
	UCH_UseCodeBlock			*psPixelEventPTOFFCodeBlock;
	UCH_UseCodeBlock			*psPixelEventEORCodeBlock;
	UCH_UseCodeBlock			*psStateCopyCodeBlock;

	IMG_VOID *pvDrawTextureAddr;		/* Vertex data for draw texture primitive */

	IMG_BOOL bPrevious32BitPDSIndices;

	GLES1PrimType	 eCurrentPrimitiveType;
	GLES1PrimType	 ePreviousPrimitiveType;

	IMG_UINT32		 ui32CurrentTexCoordSelects;

	IMG_UINT32		 ui32CurrentOutputStateBlockUSEPipe;

	IMG_UINT32		 ui32DummyPDSPixelSecondaryDataSize;

	/*
		Virtual base address and data size of the PDS pixel state program
	*/
	IMG_DEV_VIRTADDR uOutputStatePDSBaseAddress;
	IMG_UINT32		 ui32OutputStatePDSDataSize;
	
	IMG_UINT32		 ui32OutputStateUSEAttribSize;

#if defined(GLES1_EXTENSION_MATRIX_PALETTE)
	IMG_UINT32		 ui32MaxMatrixPaletteIndex;
#endif

	/*
		Virtual base address and data size of the PDS primary program
	*/
	IMG_DEV_VIRTADDR	uVertexPDSBaseAddress;
	IMG_UINT32			ui32VertexPDSDataSize;

	IMG_DEV_VIRTADDR	uFragmentPDSBaseAddress;
	IMG_UINT32			ui32FragmentPDSDataSize;

	/*
		Virtual base address and data size of the PDS Program that loads the USE shader secondary attribute program
	*/
	IMG_DEV_VIRTADDR	uVertexPDSSecAttribBaseAddress;
	IMG_UINT32			ui32VertexPDSSecAttribDataSize;

	IMG_DEV_VIRTADDR	uFragmentPDSSecAttribBaseAddress;
	IMG_UINT32			ui32FragmentPDSSecAttribDataSize;


} GLESPrimitiveMachine;


IMG_VOID DestroyHashedPDSFragSA(GLES1Context *gc, IMG_UINT32 ui32Item);

GLES1_MEMERROR ValidateState(GLES1Context *gc);

GLES1_MEMERROR GLES1EmitState(GLES1Context *gc, IMG_UINT32 ui32NumIndices, IMG_DEV_VIRTADDR uIndexAddress, IMG_UINT32 ui32IndexOffset);

IMG_UINT32 EncodeDmaBurst(IMG_UINT32 *	pui32DMAControl,
							IMG_UINT32	ui32DestOffset,
							IMG_UINT32	ui32BurstSize,
							IMG_DEV_VIRTADDR uSrcAddress);

GLES1_MEMERROR PatchPregenMTEStateCopyPrograms(GLES1Context	    *gc,
											   IMG_UINT32       ui32StateSizeInDWords,
											   IMG_DEV_VIRTADDR	uStateDataHWAddress);

GLES1_MEMERROR WriteMTEStateCopyPrograms(GLES1Context		*gc,
										IMG_UINT32			ui32StateSizeInDWords,
										IMG_DEV_VIRTADDR	uStateDataHWAddress);

GLES1_MEMERROR OutputTerminateState(GLES1Context *gc, EGLRenderSurface *psRenderSurface, IMG_UINT32 ui32KickFlags);
GLES1_MEMERROR SetupPDSVertexPregenBuffer(GLES1Context *gc);
GLES1_MEMERROR SetupMTEPregenBuffer(GLES1Context *gc);
GLES1_MEMERROR SetupPixelEventProgram(GLES1Context *gc, EGLPixelBEState *psPixelBEState, IMG_BOOL bPatch);
GLES1_MEMERROR SetupStateUpdate(GLES1Context *gc, IMG_BOOL bMTEStateUpdate);
GLES1_MEMERROR SendAccumulateObject(GLES1Context *gc, IMG_BOOL bClearDepth, IMG_FLOAT fDepth);

IMG_BOOL InitAccumUSECodeBlocks(GLES1Context *gc);
IMG_BOOL InitClearUSECodeBlocks(GLES1Context *gc);
IMG_BOOL InitErrataUSECodeBlocks(GLES1Context *gc);
IMG_BOOL InitScissorUSECodeBlocks(GLES1Context *gc);

IMG_VOID FreeAccumUSECodeBlocks(GLES1Context *gc);
IMG_VOID FreeClearUSECodeBlocks(GLES1Context *gc);
IMG_VOID FreeErrataUSECodeBlocks(GLES1Context *gc);
IMG_VOID FreeScissorUSECodeBlocks(GLES1Context *gc);

GLES1_MEMERROR SendClearPrims(GLES1Context *gc,
							  IMG_UINT32 ui32ClearFlags,
							  IMG_BOOL bFullScreen,
							  IMG_FLOAT fDepth);

GLES1_MEMERROR SendDepthBiasPrims(GLES1Context *gc);

IMG_VOID AttachAllUsedBOsAndVAOToCurrentKick(GLES1Context *gc);

IMG_VOID ReclaimPDSVariantMemKRM(IMG_VOID *pvContext, KRMResource *psResource);
IMG_VOID DestroyPDSVariantGhostKRM(IMG_VOID *pvContext, KRMResource *psResource);

#endif /* _VALIDATE_ */
/******************************************************************************
 End of file (validate.h)
******************************************************************************/
