/**************************************************************************
 * Name         : validate.h
 *
 * Copyright    : 2006 by Imagination Technologies Limited. All rights reserved.
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
 * $Log: validate.h $
 **************************************************************************/

#ifndef _VALIDATE_
#define _VALIDATE_



/**************************************************************************/
/*						 Validation/dirty flags							  */
/**************************************************************************/

#define GLES2_DIRTYFLAG_RENDERSTATE			0x00000001U
#define GLES2_DIRTYFLAG_PRIM_INDEXBITS		0x00000002U
#define GLES2_DIRTYFLAG_VERTPROG_CONSTANTS	0x00000004U
#define GLES2_DIRTYFLAG_FRAGPROG_CONSTANTS	0x00000008U
#define GLES2_DIRTYFLAG_TEXTURE_STATE		0x00000010U
#define GLES2_DIRTYFLAG_FP_TEXCTRLWORDS		0x00000020U
#define GLES2_DIRTYFLAG_VP_TEXCTRLWORDS		0x00000040U
#define GLES2_DIRTYFLAG_VP_STATE			0x00000080U
#define GLES2_DIRTYFLAG_FP_STATE			0x00000100U
#define GLES2_DIRTYFLAG_VERTEX_PROGRAM		0x00000200U
#define GLES2_DIRTYFLAG_FRAGMENT_PROGRAM	0x00000400U
#define GLES2_DIRTYFLAG_POINT_LINE_WIDTH	0x00000800U

#define GLES2_DIRTYFLAG_VAO_BINDING         0x00001000U
#define GLES2_DIRTYFLAG_VAO_CLIENT_STATE	0x00002000U
#define GLES2_DIRTYFLAG_VAO_ATTRIB_STREAM   0x00004000U
#define GLES2_DIRTYFLAG_VAO_ATTRIB_POINTER  0x00008000U
#define GLES2_DIRTYFLAG_VAO_ELEMENT_BUFFER  0x00010000U
#define GLES2_DIRTYFLAG_VAO_ALL             0x0001F000U

#define GLES2_DIRTYFLAG_DMS_INFO			0x00020000

#define GLES2_DIRTYFLAG_ALL					0x0003FFFF

/* PRQA S 3332 1 */ /* Override QAC suggestion and use this macro. */

/******************************************************************************
	Emit flags
******************************************************************************/
/*
#define GLES2_EMITSTATE_PDS_FRAGMENT_PROGRAM			0x00000001
#define GLES2_EMITSTATE_PDS_FRAGMENT_SECONDARY_PROGRAM	0x00000002
*/
#define GLES2_EMITSTATE_PDS_FRAGMENT_STATE				0x00000004U
#define GLES2_EMITSTATE_PDS_FRAGMENT_SECONDARY_STATE	0x00000008U

/*
#define GLES2_EMITSTATE_PDS_VERTEX_PROGRAM				0x00000010
#define GLES2_EMITSTATE_PDS_VERTEX_SECONDARY_PROGRAM	0x00000020
*/
#define GLES2_EMITSTATE_PDS_VERTEX_SECONDARY_STATE		0x00000040U

#define GLES2_EMITSTATE_MTE_STATE_REGION_CLIP			0x00000080U
#define GLES2_EMITSTATE_MTE_STATE_VIEWPORT				0x00000100U
#define GLES2_EMITSTATE_MTE_STATE_TAMISC				0x00000200U
#define GLES2_EMITSTATE_MTE_STATE_CONTROL				0x00000400U
#define GLES2_EMITSTATE_MTE_STATE_ISP					0x00000800U
#define GLES2_EMITSTATE_MTE_STATE_OUTPUTSELECTS			0x00001000U
#define GLES2_EMITSTATE_STATEUPDATE						0x00002000U

#define GLES2_EMITSTATE_ALL								0x00003FFFU


/**************************************************************************/
/**************************************************************************/



#define GLES2_EXTRACT(Data, Field)	((Data & ~EURASIA_ ##Field ##_CLRMSK) >> EURASIA_ ##Field ##_SHIFT)
#define ALIGNCOUNT(X, Y)	(((X) + ((Y) - 1)) & ~((Y) - 1))
#define ALIGNCOUNTINBLOCKS(Size, Log2OfBlockSsize)	(((Size) + ((1UL << (Log2OfBlockSsize)) - 1)) >> (Log2OfBlockSsize))


#define EURASIA_PDS_SECATTRSIZE_ALIGNSHIFTINREGISTERS			(EURASIA_PDS_SECATTRSIZE_ALIGNSHIFT - 2)
#define EURASIA_VDMPDS_USEATTRIBUTESIZE_ALIGNSHIFTINREGISTERS	(EURASIA_VDMPDS_USEATTRIBUTESIZE_ALIGNSHIFT - 2)
#define EURASIA_VDMPDS_USEATTRIBUTESIZE_ALIGNINREGISTERS		(1 << EURASIA_VDMPDS_USEATTRIBUTESIZE_ALIGNSHIFTINREGISTERS)

#define EURASIA_TAPDSSTATE_USEATTRIBUTESIZE_ALIGNSHIFTINREGISTERS (EURASIA_TAPDSSTATE_USEATTRIBUTESIZE_ALIGNSHIFT - 2)

#define GLES2_MAX_MTE_STATE_DWORDS						(23 + 1) /* +1 adding header dword */
#define	EURASIA_NUM_PDS_VERTEX_PROGRAM_CONSTANTS		4
#if defined(FIX_HW_BRN_25339) || defined(FIX_HW_BRN_27330)
#define	EURASIA_NUM_PDS_VERTEX_PROGRAM_INSTRUCTIONS		22
#define EURASIA_NUM_PDS_STATE_PROGRAM_DWORDS			(16 + 5)
#else
#define	EURASIA_NUM_PDS_VERTEX_PROGRAM_INSTRUCTIONS		19
#define EURASIA_NUM_PDS_STATE_PROGRAM_DWORDS			(12 + 4)
#endif
#define	EURASIA_NUM_PDS_VERTEX_PROGRAM_DWORDS			(GLES2_MAX_VERTEX_ATTRIBS * (EURASIA_NUM_PDS_VERTEX_PROGRAM_CONSTANTS + EURASIA_NUM_PDS_VERTEX_PROGRAM_INSTRUCTIONS) + 1 + 1)

#define GLES2_MTE_COPY_PROG_SIZE		 (PDS_MTESTATECOPY_SIZE >> 2)
#define GLES2_ALIGNED_MTE_COPY_PROG_SIZE (GLES2_MTE_COPY_PROG_SIZE + (EURASIA_VDMPDS_BASEADDR_CACHEALIGN - GLES2_MTE_COPY_PROG_SIZE % EURASIA_VDMPDS_BASEADDR_CACHEALIGN))    

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

#define GLES2_PRIMTYPE_POINT			0
#define GLES2_PRIMTYPE_LINE				1
#define GLES2_PRIMTYPE_LINE_LOOP		2 
#define GLES2_PRIMTYPE_LINE_STRIP		3 
#define GLES2_PRIMTYPE_TRIANGLE			4 
#define GLES2_PRIMTYPE_TRIANGLE_STRIP	5 
#define GLES2_PRIMTYPE_TRIANGLE_FAN		6 
#define	GLES2_PRIMTYPE_SPRITE			7
#define GLES2_PRIMTYPE_MAX				8 

typedef struct GLES2PDSVertexState_TAG
{
	/* Is this stream merged with the next */
	IMG_BOOL   bMerged[GLES2_MAX_VERTEX_ATTRIBS];

	PDS_VERTEX_SHADER_PROGRAM_INFO sProgramInfo;
	IMG_UINT32 aui32LastPDSProgram[EURASIA_NUM_PDS_VERTEX_PROGRAM_DWORDS];
	IMG_UINT32 ui32ProgramSize;
}GLES2PDSVertexState;


#define GLES2_ALPHA_TEST_DISCARD		(1U << 0)
#define GLES2_ALPHA_TEST_WAS_TWD		(1U << 1)

#if defined(FIX_HW_BRN_25077)
#define GLES2_ALPHA_TEST_BRN25077		(1U << 2)
#endif /* defined(FIX_HW_BRN_25077) */

#if defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728)
#define GLES1_ALPHA_TEST_BRNFIX_DEPTHF	(1U << 3)
#endif /* defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728) */

typedef struct GLES2CompiledRenderState_TAG
{

	/* ISP State */
	IMG_UINT32	ui32ISPControlWordA;				/* Stencil masks, stencil operations */
	IMG_UINT32	ui32ISPControlWordB;				/* Stencil ref, object type (tri, line etc.), Tag & Depth write 
												   disables, depth compare mode, pass type */
	IMG_UINT32	ui32ISPControlWordC;				/* Visibility test register, user pass-spawn, Depth bias 
												   units & factor, PS register/block size control, valid id */

	/* Second set of ISP control, for 2-sided stencil etc */
	IMG_UINT32	ui32BFISPControlWordA;
	IMG_UINT32	ui32BFISPControlWordB;
	IMG_UINT32	ui32BFISPControlWordC;

	/********************************
	 *      New Eurasia state       *
	 ********************************/
	IMG_UINT32			ui32MTEControl;

	IMG_BOOL			bHasConstantColorBlend;

	/* Discard feedback test */
	IMG_UINT32			ui32AlphaTestFlags;
	IMG_UINT32			ui32ISPFeedback0;	/* Visibility test state sent back to ISP  */
	IMG_UINT32			ui32ISPFeedback1;	/* from USE for discard. */ 
	
} GLES2CompiledRenderState;


#if defined(FIX_HW_BRN_26922)
typedef struct GLES2BRN26922State_TAG
{
#if defined(PDUMP)
	IMG_BOOL 					bDump;
#endif 	
	PVRSRV_CLIENT_MEM_INFO		*psBRN26922MemInfo;

} GLES2BRN26922State;
#endif

typedef struct GLES2PrimitiveMachine_TAG
{
	GLES2CompiledRenderState	sRenderState;
	GLES2CompiledTextureState	sVertexTextureState;
	GLES2CompiledTextureState	sFragmentTextureState;
	
	GLES2PDSVertexState			*psPDSVertexState;

	UCH_UseCodeBlock			*psHWBGCodeBlock;
#if defined(FIX_HW_BRN_26922)
	GLES2BRN26922State			sBRN26922State;
#endif 

	UCH_UseCodeBlock			*psClearVertexCodeBlock;
	UCH_UseCodeBlock			*psClearFragmentCodeBlock;
	UCH_UseCodeBlock			*psAccumVertexCodeBlock;
	UCH_UseCodeBlock			*psScissorVertexCodeBlock;
	UCH_UseCodeBlock			*psDummyPixelSecondaryPDSCode;
	UCH_UseCodeBlock			*psPixelEventPTOFFCodeBlock;
	UCH_UseCodeBlock			*psPixelEventEORCodeBlock;
	UCH_UseCodeBlock			*psStateCopyCodeBlock;


#if defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728)
	IMG_UINT32		 eCurrentPrimitiveType;
#endif /* defined(FIX_HW_BRN_29546) */

	/* Info for previous primitive */
	IMG_UINT32		 ePreviousPrimitiveType;

	IMG_BOOL		 bPrevious32BitPDSIndices;
	
	IMG_UINT32		 ui32CurrentOutputStateBlockUSEPipe;

	IMG_UINT32		 ui32DummyPDSPixelSecondaryDataSize;
	/*
		Virtual base address and data size of the PDS pixel state program
	*/
	IMG_DEV_VIRTADDR uOutputStatePDSBaseAddress;
	IMG_UINT32 ui32OutputStatePDSDataSize;
	
	IMG_UINT32 ui32OutputStateUSEAttribSize;


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
	IMG_UINT32          ui32VertexPDSSecAttribDataSize;

	IMG_DEV_VIRTADDR	uFragmentPDSSecAttribBaseAddress;
	IMG_UINT32          ui32FragmentPDSSecAttribDataSize;

} GLES2PrimitiveMachine;


GLES2_MEMERROR ValidateState(GLES2Context *gc);

GLES2_MEMERROR GLES2EmitState(GLES2Context *gc, IMG_UINT32 ePrimitiveType, IMG_BOOL b32BitIndices,
							IMG_UINT32 ui32NumIndices, IMG_DEV_VIRTADDR uIndexAddress, IMG_UINT32 ui32IndexOffset);

IMG_UINT32 EncodeDmaBurst(IMG_UINT32 *	pui32DMAControl,
							IMG_UINT32	ui32DestOffset,
							IMG_UINT32	ui32BurstSize,
							IMG_DEV_VIRTADDR uSrcAddress);

GLES2_MEMERROR PatchPregenMTEStateCopyPrograms(GLES2Context	*gc,
											   IMG_UINT32 ui32StateSizeInDWords,
											   IMG_DEV_VIRTADDR	uStateDataHWAddress);

GLES2_MEMERROR WriteMTEStateCopyPrograms	(GLES2Context		*gc,
											IMG_UINT32			ui32StateSizeInDWords,
											IMG_DEV_VIRTADDR	uStateDataHWAddress);

GLES2_MEMERROR OutputTerminateState(GLES2Context *gc, EGLRenderSurface *psRenderSurface, IMG_UINT32 ui32KickFlags);
GLES2_MEMERROR SetupPixelEventProgram(GLES2Context *gc, EGLPixelBEState *psPixelBEState, IMG_BOOL bPatch);
GLES2_MEMERROR SetupStateUpdate(GLES2Context *gc, IMG_BOOL bMTEStateUpdate);
GLES2_MEMERROR SetupMTEPregenBuffer(GLES2Context *gc);
GLES2_MEMERROR SendAccumulateObject(GLES2Context *gc, IMG_BOOL bClearDepth, IMG_FLOAT fDepth);
IMG_BOOL InitAccumUSECodeBlocks(GLES2Context *gc);
IMG_BOOL InitClearUSECodeBlocks(GLES2Context *gc);
IMG_BOOL InitErrataUSECodeBlocks(GLES2Context *gc);
IMG_BOOL InitScissorUSECodeBlocks(GLES2Context *gc);

IMG_VOID FreeAccumUSECodeBlocks(GLES2Context *gc);
IMG_VOID FreeClearUSECodeBlocks(GLES2Context *gc);
IMG_VOID FreeErrataUSECodeBlocks(GLES2Context *gc);
IMG_VOID FreeScissorUSECodeBlocks(GLES2Context *gc);

GLES2_MEMERROR SendClearPrims(GLES2Context *gc, 
							  IMG_UINT32 ui32ClearFlags, 
							  IMG_BOOL bFullScreen,
							  IMG_FLOAT fDepth);

GLES2_MEMERROR SendDepthBiasPrims(GLES2Context *gc);

IMG_VOID AttachAllUsedBOsAndVAOToCurrentKick(GLES2Context *gc);


#endif /* _VALIDATE_ */
