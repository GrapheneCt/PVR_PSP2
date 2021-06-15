/*****************************************************************************
 Name			: pds.h
 
 Title			: PDS control
 
 C Author 		: Paul Burgess
 
 Created  		: 12/04/05
 
 Copyright		: 2005-2006 by Imagination Technologies Limited. All rights reserved.
				  No part of this software, either material or conceptual may
				  be copied	or distributed, transmitted, transcribed, stored
				  in a retrieval system or translated into any human or
				  computer language in any form by any means, electronic,
				  mechanical, manual or otherwise, or disclosed to third
				  parties without the express written permission of
				  Imagination Technologies Limited, Unit 8, HomePark Industrial Estate,
				  King's Langley, Hertfordshire,
				  WD4 8LZ, U.K.
				  
 Description	: PDS control.
 
 Program Type	: 32-bit DLL
 
 Version	 	: $Revision: 1.104 $
 
 Modifications	:
 
 $Log: pds.h $
 
  --- Revision Logs Removed --- 
 
*****************************************************************************/

#ifndef _PDS_H_
#define _PDS_H_

/*****************************************************************************
 Calling conventions
*****************************************************************************/
#if (defined(__psp2__) && defined(PSP2_PRX_EXPORT))
#define PDS_CALLCONV __declspec (dllexport)
#else
#define PDS_CALLCONV IMG_INTERNAL
#endif

/*****************************************************************************
 Macro definitions
*****************************************************************************/

#if defined(SGX_FEATURE_PDS_DATA_INTERLEAVE_2DWORDS)
#define	PDS_NUM_DWORDS_PER_ROW		2
#define	PDS_NUM_DWORDS_PER_ROW_ALIGNSHIFT	1
#else
#define	PDS_NUM_DWORDS_PER_ROW		8
#define	PDS_NUM_DWORDS_PER_ROW_ALIGNSHIFT	3
#endif


#define	PDS_NUM_DMA_KICKS				3
#define	PDS_NUM_DMA_CONTROL_WORDS		2
#define	PDS_NUM_USE_TASK_CONTROL_WORDS	3
#define	PDS_NUM_VERTEX_STREAMS			16
#define	PDS_NUM_VERTEX_ELEMENTS			16
#define PVRD3D_MAXIMUM_ITERATIONS		128

#define PDS_NUM_TEXTURE_IMAGE_CHUNKS    4

#define PDS_NUM_CONST_ELEMENTS			(16*16)

/*
	Maximum number of texture reads in a PDS pixel shader program if every possible USE iteration is
	done once as well.
*/

#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)

#if (EURASIA_PDS_DOUTI_STATE_SIZE == 2)

/*
	96		Total constants
	- 3		3 words for the DOUTU command
	- 28	2 words for each iteration
	/ 7		7 words for each texture sample (2 for the DOUTI, 4 for the DOUTT and 1 for
	=9		possible wastage since the DOUTT data needs to start on a 64 bit boundary)
*/
#define PDS_MAX_TEXTURE_READS			9

#else

/*
	96		Total constants
	- 3		3 words for the DOUTU command
	- 14	1 word for each iteration
	/ 6		6 words for each texture sample (1 for the DOUTI, 4 for the DOUTT and 1 for
	=13		possible wastage since the DOUTT data needs to start on a 64 bit boundary)
*/
#define PDS_MAX_TEXTURE_READS			13

#endif

#else
/*
	96		Total constants
	- 3		3 words for the DOUTU command
	- 14	1 word for each iteration
	/ 4		4 words for each texture sample (1 for the DOUTI and 3 for the DOUTT)
	= 19
*/
#define PDS_MAX_TEXTURE_READS			19

#endif

/*
	Number of primary attributes for the pixel event program.
*/
#if defined(SGX_FEATURE_WRITEBACK_DCU)
#define PDS_PIXEVENT_NUM_PAS			1
#else
#define PDS_PIXEVENT_NUM_PAS			0
#endif

/*****************************************************************************
 Structure definitions
*****************************************************************************/

/*
	Structure representing the PDS pixel event program
	
	pui32DataSegment				- pointer to the data segment
	ui32DataSize					- size of the data segment
	aui32EOTUSETaskControl		- array of USE task control words
	aui32PTOFFUSETaskControl	- array of USE task control words
	aui32EORUSETaskControl		- array of USE task control words
	aui32DMAControl				- array of DMA control words
*/
typedef struct _PDS_PIXEL_EVENT_PROGRAM_
{
	IMG_UINT32				   *pui32DataSegment;
	IMG_UINT32					ui32DataSize;
	IMG_UINT32					aui32EOTUSETaskControl		[PDS_NUM_USE_TASK_CONTROL_WORDS];
	IMG_UINT32					aui32PTOFFUSETaskControl	[PDS_NUM_USE_TASK_CONTROL_WORDS];
	IMG_UINT32					aui32EORUSETaskControl		[PDS_NUM_USE_TASK_CONTROL_WORDS];

} PDS_PIXEL_EVENT_PROGRAM, *PPDS_PIXEL_EVENT_PROGRAM;

/*
	Structure representing the PDS pixel shader secondary attribute program
	
	pui32DataSegment				- pointer to the data segment
	ui32DataSize					- size of the data segment
	ui32NumDMAKicks				- number of DMA kicks
	aui32DMAControl				- array of DMA control words
	
	ui32DAWCount				- Number of attributes to write using DOUTAs
	ui32DAWOffset				- Value of the lowest attribute to write to
	pui32DAWData				- Date to write
*/
typedef struct _PDS_PIXEL_SHADER_SA_PROGRAM_
{
	IMG_UINT32						*pui32DataSegment;
	IMG_UINT32						ui32DataSize;
	IMG_UINT32						ui32NumDMAKicks;
	IMG_UINT32						aui32DMAControl		[PDS_NUM_DMA_KICKS * PDS_NUM_DMA_CONTROL_WORDS];
	IMG_BOOL						bKickUSE;
	IMG_BOOL						bKickUSEDummyProgram;
	IMG_BOOL						bWriteTilePosition;
	IMG_UINT32						uTilePositionAttrDest;
#if defined(SGX_FEATURE_ALPHATEST_SECONDARY)
	IMG_BOOL						bIterateZAbs;
#endif
#if defined(FIX_HW_BRN_22249)
	IMG_BOOL						bGenerateTileAddress;
	IMG_UINT32						ui32RenderBaseAddress;
#endif /* defined(FIX_HW_BRN_22249) */
	IMG_UINT32						aui32USETaskControl	[PDS_NUM_USE_TASK_CONTROL_WORDS];
	
} PDS_PIXEL_SHADER_SA_PROGRAM, *PPDS_PIXEL_SHADER_SA_PROGRAM;


/*
	Structure representing the static PDS pixel shader secondary attribute program
	
	pui32DataSegment				- pointer to the data segment
	ui32DataSize					- size of the data segment
	
	ui32DAWCount				- Number of attributes to write using DOUTAs
	ui32DAWOffset				- Value of the lowest attribute to write to
	pui32DAWData				- Date to write
*/
typedef struct _PDS_PIXEL_SHADER_STATIC_SA_PROGRAM_
{
	IMG_UINT32						*pui32DataSegment;
	IMG_UINT32						ui32DataSize;
	IMG_BOOL						bKickUSEDummyProgram;
#if defined(SGX_FEATURE_ALPHATEST_SECONDARY)
	IMG_BOOL						bIterateZAbs;
#endif
	IMG_UINT32						aui32USETaskControl[PDS_NUM_USE_TASK_CONTROL_WORDS];
	
	IMG_UINT32						ui32DAWCount;
	IMG_UINT32						ui32DAWOffset;
	IMG_UINT32						*pui32DAWData;
	
} PDS_PIXEL_SHADER_STATIC_SA_PROGRAM, *PPDS_PIXEL_SHADER_STATIC_SA_PROGRAM;



/*
	Structure representing the PDS pixel shader program
	
	pui32DataSegment				- pointer to the data segment
	ui32DataSize					- size of the data segment
	aui32USETaskControl			- array of USE task control words
	ui32NumFPUIterators			- number of FPU iterators
	aui32FPUIterators				- array of FPU iterator control words
	ui32NumTAGLayers				- number of TAG layers
	aaui32TAGLayers				- array of TAG layer control words
*/
typedef struct _PDS_PIXEL_SHADER_PROGRAM_
{
	IMG_UINT32						*pui32DataSegment;
	IMG_UINT32						ui32DataSize;
	IMG_UINT32						aui32USETaskControl	[PDS_NUM_USE_TASK_CONTROL_WORDS];
	IMG_UINT32						ui32NumFPUIterators;
#if (EURASIA_PDS_DOUTI_STATE_SIZE == 1)
	IMG_UINT32						aui32FPUIterators		[PVRD3D_MAXIMUM_ITERATIONS];
#else
	IMG_UINT32						aui32FPUIterators0		[PVRD3D_MAXIMUM_ITERATIONS];
	IMG_UINT32						aui32FPUIterators1		[PVRD3D_MAXIMUM_ITERATIONS];
#endif
	IMG_UINT32						aui32TAGLayers			[PVRD3D_MAXIMUM_ITERATIONS];
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
	IMG_UINT8						aui8LayerSize			[PVRD3D_MAXIMUM_ITERATIONS];
	IMG_UINT8						aui8FormatConv			[PVRD3D_MAXIMUM_ITERATIONS];
	IMG_BOOL						abMinPack				[PVRD3D_MAXIMUM_ITERATIONS];
#endif

} PDS_PIXEL_SHADER_PROGRAM, *PPDS_PIXEL_SHADER_PROGRAM;

/*
	Structure representing the MTE state copy program
	
	pui32DataSegment				- pointer to the data segment
	ui32DataSize					- size of the data segment
	aui32DMAControl				- array of DMA control words
	aui32USETaskControl			- array of USE task control words
*/
typedef struct _PDS_STATE_COPY_PROGRAM_
{
	IMG_UINT32						*pui32DataSegment;
	IMG_UINT32						ui32DataSize;
	IMG_UINT32						ui32NumDMAKicks;
	IMG_UINT32						aui32DMAControl		[PDS_NUM_DMA_KICKS * PDS_NUM_DMA_CONTROL_WORDS];
	IMG_UINT32						aui32USETaskControl	[PDS_NUM_USE_TASK_CONTROL_WORDS];
	
} PDS_STATE_COPY_PROGRAM, *PPDS_STATE_COPY_PROGRAM;

/*
	Structure representing the MTE Terminate state program
	
	pui32DataSegment				- pointer to the data segment
	ui32DataSize					- size of the data segment
	aui32USETaskControl			- array of USE task control words
*/
typedef struct _PDS_TERMINATE_STATE_PROGRAM_
{
	IMG_UINT32						*pui32DataSegment;
	IMG_UINT32						ui32DataSize;
	IMG_UINT32						aui32USETaskControl	[PDS_NUM_USE_TASK_CONTROL_WORDS];
	IMG_UINT32						ui32TerminateRegion;
} PDS_TERMINATE_STATE_PROGRAM;

/*
	Structure representing the PDS vertex shader secondary attribute program
	
	pui32DataSegment				- pointer to the data segment
	ui32DataSize					- size of the data segment
	ui32NumDMAKicks				- number of DMA kicks
	aui32DMAControl				- array of DMA control words
*/
typedef struct _PDS_VERTEX_SHADER_SA_PROGRAM_
{
	IMG_UINT32						*pui32DataSegment;
	IMG_UINT32						ui32DataSize;
	IMG_UINT32						ui32NumDMAKicks;
	IMG_UINT32						aui32DMAControl		[PDS_NUM_DMA_KICKS * PDS_NUM_DMA_CONTROL_WORDS];
	IMG_UINT32						aui32USETaskControl	[PDS_NUM_USE_TASK_CONTROL_WORDS];
	
} PDS_VERTEX_SHADER_SA_PROGRAM, *PPDS_VERTEX_SHADER_SA_PROGRAM;

/*
	Structure representing a PDS vertex stream element
	
	ui32Offset					- offset of the vertex stream element
	ui32Size						- size of the vertex stream element
	ui32Register					- vertex stream element register
*/
typedef struct _PDS_VERTEX_ELEMENT_
{
	IMG_UINT32						ui32Offset;
	IMG_UINT32						ui32Size;
	IMG_UINT32						ui32Register;
	
} PDS_VERTEX_ELEMENT, *PPDS_VERTEX_ELEMENT;

/*
	Structure representing a PDS vertex stream
	
	bInstanceData				- flag whether the vertex stream is indexed or instance data
	ui32Multiplier				- vertex stream frequency multiplier
	ui32Shift						- vertex stream frequency shift
	ui32Address					- vertex stream address in bytes
	ui32Stride					- vertex stream stride in bytes
	ui32NumElements				- number of vertex stream elements
	asElements					- array of vertex stream elements
*/
typedef struct _PDS_VERTEX_STREAM_
{
	IMG_BOOL						bInstanceData;
	IMG_UINT32						ui32Multiplier;
	IMG_UINT32						ui32Shift;
	IMG_UINT32						ui32Address;
	IMG_UINT32						ui32Stride;
	IMG_UINT32						ui32NumElements;
	PDS_VERTEX_ELEMENT				asElements[PDS_NUM_VERTEX_ELEMENTS];
	
} PDS_VERTEX_STREAM, *PPDS_VERTEX_STREAM;

/*
	Structure representing the PDS vertex shader program
	
	pui32DataSegment				- pointer to the data segment
	ui32DataSize					- size of the data segment
	aui32USETaskControl			- array of USE task control words
	b32BitIndices				- flag whether the vertex streams use 16-bit or 32-bit indices
	ui32NumInstances				- number of instances
	ui32NumStreams				- number of vertex streams
	asStreams					- array of vertex streams
	ui32VertexCount				- The number of vertices in the vertex streams
	bIterateVtxID				- If set, the vertex ID should be iterated
	ui32VtxIDRegister			- The register to iterate the VertexID into (if applicable)
	ui32VtxIDOffset				- Add this number on top of VertexID (when splitting draw calls)
	bIterateInstanceID			- If set, the instance ID should be iterated
	ui32InstanceIDRegister		- The register to iterate the InstanceID into (if applicable)
	ui32InstanceIDOffset		- Add this number on top of InstanceID (when splitting draw calls)
	The vertex and instance ID will both be iterated as unsigned ints
*/
typedef struct _PDS_VERTEX_SHADER_PROGRAM_
{
	IMG_UINT32						*pui32DataSegment;
	IMG_UINT32						ui32DataSize;
	IMG_UINT32						aui32USETaskControl	[PDS_NUM_USE_TASK_CONTROL_WORDS];
	IMG_BOOL						b32BitIndices;
	IMG_UINT32						ui32NumInstances;
	IMG_UINT32						ui32NumStreams;
	PDS_VERTEX_STREAM				asStreams			[PDS_NUM_VERTEX_STREAMS];
	IMG_UINT32						ui32VertexCount;
	
	IMG_BOOL						bIterateVtxID;
	IMG_UINT32						ui32VtxIDRegister;
	IMG_UINT32						ui32VtxIDOffset;
	
	IMG_BOOL						bIterateInstanceID;
	IMG_UINT32						ui32InstanceIDRegister;
	IMG_UINT32						ui32InstanceIDOffset;
} PDS_VERTEX_SHADER_PROGRAM, *PPDS_VERTEX_SHADER_PROGRAM;

/*
  Structure containing information about a program generated by PDSGenerateVertexShaderProgram()
  Can subsequently sent to PDSPatchVertexShaderProgram() to perform address and use task patching
  All offsets are in DWORDS
  
  *** Filled in by PDSGenerateVertexShaderProgram() ***
  
  aui32USETaskControlOffsets    - Offset into program where use task control should be written
  
  ui32NumStreams                - Number of stream addresses to provide
  aui32NumElements              - Number of elements in each stream
  aui32AddressOffsets           - Offset into program where stream addresses  should be written
  aui32ElementOffsets           - Stream offsets to add to addresses
  
  *** Filled in by driver before calling PDSPatchVertexShaderProgram() ***
  
  aui32StreamAddresses         - The addresses to insert into the program
  aui32USETaskControl          - The use task control words to insert into the program
  bPatchTaskControl			   - Do/Don't patch the use task control words
*/
typedef struct _PDS_VERTEX_SHADER_PROGRAM_INFO_
{
	IMG_UINT32  aui32USETaskControlOffsets[PDS_NUM_USE_TASK_CONTROL_WORDS];
	
	IMG_UINT32  ui32NumStreams;
	IMG_UINT32  aui32NumElements[PDS_NUM_VERTEX_STREAMS];
	IMG_UINT32  aui32AddressOffsets[PDS_NUM_VERTEX_STREAMS][PDS_NUM_VERTEX_ELEMENTS];
	IMG_UINT32  aui32ElementOffsets[PDS_NUM_VERTEX_STREAMS][PDS_NUM_VERTEX_ELEMENTS];
	
	IMG_UINT32  aui32StreamAddresses[PDS_NUM_VERTEX_STREAMS];
	IMG_UINT32  aui32USETaskControl[PDS_NUM_USE_TASK_CONTROL_WORDS];
	
	IMG_BOOL	bPatchTaskControl;
	
} PDS_VERTEX_SHADER_PROGRAM_INFO, *PPDS_VERTEX_SHADER_PROGRAM_INFO;

/**
 * Structure representing PDS const element to be uploaded.
 *
 * This is representative of the DOUTD interface where data is
 * transferred in n lines of m dwords.
 *
 */
typedef struct _PDS_CONST_ELEMENT_
{
	/**
	 * Device virtual address of start of source data.
	 */
	IMG_UINT32		ui32Address;
	
	/**
	 * Number of 32bit registers to copy per line.
	 * (This is effectively the size of fetch for the DMA)
	 * Valid values are 1 <= ui32Size <= EURASIA_PDS_DOUTD1_BSIZE_MAX)
	 * (Which is typically 1-16)
	 */
	IMG_UINT32		ui32Size;
	
	/**
	 * Number of times to repeat copy.
	 * (This is effectively the number of lines to fetch)
	 * Valid values are 1 <= ui32Lines <= EURASIA_PDS_DOUTD1_BLINES_MAX)
	 * (Which is typically 1-16)
	 *
	 * Total number of bytes transferred is
	 * ui32NumRegs * ui32Repeat * 4
	 */
	IMG_UINT32		ui32Lines;
	
	/**
	 * Destination register index.
	 */
	IMG_UINT32		ui32Register;
	
} PDS_CONST_ELEMENT, *PPDS_CONST_ELEMENT;

/**
 * Structure representing PDS const upload program.
 */
typedef struct _PDS_CONST_UPLOAD_PROGRAM_
{
	/**
	 * [output] Receives the address of the constants.
	 */
	IMG_UINT32			*pui32DataSegment;
	
	/**
	 * [output] Receives the PDS data size.
	 */
	IMG_UINT32			ui32DataSize;
	
	/**
	 * Data for DOUTU instruction.
	 */
	IMG_UINT32			aui32USETaskControl	[PDS_NUM_USE_TASK_CONTROL_WORDS];
	
	/**
	 * Number of valids elements as asElements.
	 */
	IMG_UINT32			ui32NumElements;
	
	/**
	 * The elements to copy.
	 */
	PDS_CONST_ELEMENT	asElements[PDS_NUM_CONST_ELEMENTS];
	
} PDS_CONST_UPLOAD_PROGRAM, *PPDS_CONST_UPLOAD_PROGRAM;

/*
	Structure representing the PDS tesselator secondary attribute program
	
	pui32DataSegment				- pointer to the data segment
	ui32DataSize					- size of the data segment
	ui32NumDMAKicks				- number of DMA kicks
	aui32DMAControl				- array of DMA control words
	aui32USETaskControl			- USE task control words
*/
typedef struct _PDS_TESSELATOR_SA_PROGRAM_
{
	IMG_UINT32						*pui32DataSegment;
	IMG_UINT32						ui32DataSize;
	IMG_UINT32						ui32NumDMAKicks;
	IMG_UINT32						aui32DMAControl		[PDS_NUM_DMA_KICKS * PDS_NUM_DMA_CONTROL_WORDS];
	IMG_UINT32						aui32USETaskControl	[PDS_NUM_USE_TASK_CONTROL_WORDS];
	
} PDS_TESSELATOR_SA_PROGRAM, *PPDS_TESSELATOR_SA_PROGRAM;

/*
	Structure representing the PDS tesselator program
	
	pui32DataSegment				- pointer to the data segment
	ui32DataSize					- size of the data segment
	aui32USETaskControl			- array of USE task control words
	b32BitIndices				- flag whether the vertex streams use 16-bit or 32-bit indices
	ui32NumInstances				- number of instances
	ui32NumStreams				- number of vertex streams
	asStreams					- array of vertex streams
	bIteratePrimID				- Iterate the primitive ID number from IR2 to the correct location
								  in the PA buffer layout
	bIterateVtxID				- Iterate the stream-output vertex ID from the top 28 bits of IR0
								  This will always be iterated into pa0, with the vertex beginning
								  at pa1 (if present)
*/
typedef struct _PDS_TESSELATOR_PROGRAM_
{
	IMG_UINT32						*pui32IndicesDataSegment;
	IMG_UINT32						*pui32VertexDataSegment;
	IMG_UINT32						*pui32DataSegment;
	#if defined(SGX545)
	IMG_UINT32						*pui32Phase2DataSegment;
	IMG_UINT32						aui32USETaskControlPhase2		[PDS_NUM_USE_TASK_CONTROL_WORDS];
	IMG_UINT32						aui32USETaskControlTess			[PDS_NUM_USE_TASK_CONTROL_WORDS];
	IMG_UINT32						ui32NumDMAKicks;
	IMG_UINT32						aui32DMAControl		[PDS_NUM_DMA_KICKS * PDS_NUM_DMA_CONTROL_WORDS];
	#else
	IMG_UINT32						aui32USETaskControlTessFirst	[PDS_NUM_USE_TASK_CONTROL_WORDS];
	IMG_UINT32						aui32USETaskControlTessOther	[PDS_NUM_USE_TASK_CONTROL_WORDS];
	#endif
	IMG_UINT32						ui32DataSize;
	IMG_UINT32						aui32USETaskControlDummy		[PDS_NUM_USE_TASK_CONTROL_WORDS];
	IMG_UINT32						aui32USETaskControlVertLoad		[PDS_NUM_USE_TASK_CONTROL_WORDS];
	IMG_BOOL						b32BitIndices;
	IMG_UINT32						ui32NumInstances;
	IMG_UINT32						ui32NumStreams;
	PDS_VERTEX_STREAM				asStreams						[PDS_NUM_VERTEX_STREAMS];
#if defined(SGX_FEATURE_PDS_EXTENDED_INPUT_REGISTERS)
	IMG_BOOL						bIteratePrimID;
#endif
#if defined(SGX_FEATURE_STREAM_OUTPUT)
	IMG_BOOL						bIterateVtxID;
#endif
} PDS_TESSELATOR_PROGRAM, *PPDS_TESSELATOR_PROGRAM;

#if defined(SGX545)
/*
	Structure representing the PDS shared comms word program for complex geometry
	
	pui32DataSegment				- pointer to the data segment
	ui32DataSize					- size of the data segment
*/
typedef struct _PDS_SCWORDCOPY_PROGRAM_
{
	IMG_UINT32						*pui32DataSegment;
	IMG_UINT32						ui32DataSize;
	IMG_UINT32						aui32USETaskControl			[PDS_NUM_USE_TASK_CONTROL_WORDS];
} PDS_SCWORDCOPY_PROGRAM, *PPDS_SCWORDCOPY_PROGRAM;

/*
	Structure representing the PDS ITP program for complex geometry
	
	pui32DataSegment				- pointer to the data segment
	ui32DataSize					- size of the data segment
	aui32USETaskControl			- array of USE task control words
*/
typedef struct _PDS_ITP_PROGRAM_
{
	IMG_UINT32						*pui32DataSegment;
	IMG_UINT32						ui32DataSize;
	IMG_UINT32						aui32USETaskControl			[PDS_NUM_USE_TASK_CONTROL_WORDS];
} PDS_ITP_PROGRAM, *PPDS_ITP_PROGRAM;

/*
	Structure representing the PDS VtxBufWritePointer program for complex geometry
	
	pui32DataSegment				- pointer to the data segment
	ui32DataSize					- size of the data segment
	aui32USETaskControl			- array of USE task control words
*/
typedef struct _PDS_VTXBUFWRITEPOINTER_PROGRAM_
{
	IMG_UINT32						*pui32DataSegment;
	IMG_UINT32						ui32DataSize;
	IMG_UINT32						aui32USETaskControl			[PDS_NUM_USE_TASK_CONTROL_WORDS];
} PDS_VTXBUFWRITEPOINTER_PROGRAM, *PPDS_VTXBUFWRITEPOINTER_PROGRAM;
#endif

/*
	Structure representing the PDS inter-pipe SD program
	
	pui32DataSegment				- pointer to the data segment
	ui32DataSize					- size of the data segment
	aui32USETaskControl			- array of USE task control words
*/
typedef struct _PDS_IPSD_PROGRAM_
{
	IMG_UINT32						*pui32DataSegment;
	IMG_UINT32						ui32DataSize;
	IMG_UINT32						aui32USETaskControl			[PDS_NUM_USE_TASK_CONTROL_WORDS];
	
} PDS_IPSD_PROGRAM, *PPDS_IPSD_PROGRAM;

/*
	Structure representing the PDS Complex Advance Flush program
	
	pui32DataSegment				- pointer to the data segment
	ui32DataSize					- size of the data segment
	aui32USETaskControl			- array of USE task control words
*/
typedef struct _PDS_COMPLEXADVANCEFLUSH_PROGRAM_
{
	IMG_UINT32						*pui32DataSegment;
	IMG_UINT32						ui32DataSize;
	IMG_UINT32						aui32USETaskControl	[PDS_NUM_USE_TASK_CONTROL_WORDS];
	
} PDS_COMPLEXADVANCEFLUSH_PROGRAM, *PPDS_COMPLEXADVANCEFLUSH_PROGRAM;

/*
	Structure for PDS program for generic kicking USE
	
	pui32DataSegment				- pointer to the data segment
	ui32DataSize					- size of the data segment
	aui32USETaskControl			- array of USE task control words
*/
typedef struct _PDS_GENERICKICKUSE_PROGRAM_
{
	IMG_UINT32						*pui32DataSegment;
	IMG_UINT32						ui32DataSize;
	IMG_UINT32						aui32USETaskControl	[PDS_NUM_USE_TASK_CONTROL_WORDS];
} PDS_GENERICKICKUSE_PROGRAM, *PPDS_GENERICKICKUSE_PROGRAM;

/*
	Structure representing the PDS VS memory-constant upload program
	
	pui32DataSegment			- pointer to the data segment
	ui32DataSize				- size of the data segment
	aui32DMAControl				- array of DMA control words
	aui32USETaskControl			- array of USE task control words
*/
typedef struct _PDS_VS_MEMCONST_UPLOAD_PROGRAM_
{
	IMG_UINT32						*pui32DataSegment;
	IMG_UINT32						ui32DataSize;
	IMG_UINT32						ui32NumDMAKicks;
	IMG_UINT32						aui32DMAControl		[PDS_NUM_DMA_KICKS * PDS_NUM_DMA_CONTROL_WORDS];
	IMG_UINT32						aui32USETaskControl	[PDS_NUM_USE_TASK_CONTROL_WORDS];
	
} PDS_VS_MEMCONST_UPLOAD_PROGRAM, *PPDS_VS_MEMCONST_UPLOAD_PROGRAM;

/*
	Structure representing the PDS
	
	uTAGControlWord0            - TAG Control word 0
	uTAGControlWord1            - TAG Control word 1
	uTAGControlWord2            - TAG Control word 2
	uTAGControlWord3            - TAG Control word 3
	ui8FormatConv				- Format conversion field of the DOUTT instruction
*/
typedef struct _PDS_TEXTURE_IMAGE_UNIT_
{
	IMG_UINT32			ui32TAGControlWord0;
	IMG_UINT32			ui32TAGControlWord1;
	IMG_UINT32			ui32TAGControlWord2;
#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
	IMG_UINT32			ui32TAGControlWord3;
#endif
} PDS_TEXTURE_IMAGE_UNIT, *PPDS_TEXTURE_IMAGE_UNIT;

/*****************************************************************************
 function declarations
*****************************************************************************/

PDS_CALLCONV IMG_UINT32 * PDSGenerateStaticVertexShaderProgram	(PPDS_VERTEX_SHADER_PROGRAM       psProgram,
																 IMG_UINT32                      *pui32Buffer,
																 PPDS_VERTEX_SHADER_PROGRAM_INFO  psPDSVertexShaderProgramInfo);
																 
PDS_CALLCONV IMG_UINT32 * PDSGeneratePixelShaderSAProgram	(PPDS_PIXEL_SHADER_SA_PROGRAM	psProgram,
															 IMG_UINT32						*pui32Buffer);
															 
PDS_CALLCONV IMG_UINT32 * PDSGenerateStaticPixelShaderSAProgram	(PPDS_PIXEL_SHADER_STATIC_SA_PROGRAM	psProgram,
															 IMG_UINT32						*pui32Buffer);
															 
PDS_CALLCONV IMG_UINT32 * PDSGeneratePixelShaderProgram		(PPDS_TEXTURE_IMAGE_UNIT      psTextureImageUnit,
															 PPDS_PIXEL_SHADER_PROGRAM		psProgram,
															 IMG_UINT32						*pui32Buffer);
															 
PDS_CALLCONV IMG_UINT32 * PDSGenerateStateCopyProgram		(PPDS_STATE_COPY_PROGRAM		psProgram,
															 IMG_UINT32						*pui32Buffer);
															 
PDS_CALLCONV IMG_UINT32 * PDSGenerateVertexShaderSAProgram	(PPDS_VERTEX_SHADER_SA_PROGRAM	psProgram,
															 IMG_UINT32					   *pui32Buffer,
															 IMG_BOOL						bShadowSAs);
															 
PDS_CALLCONV IMG_UINT32 * PDSGenerateVertexShaderProgram		(PPDS_VERTEX_SHADER_PROGRAM	psProgram,
																 IMG_UINT32						*pui32Buffer,
																 PPDS_VERTEX_SHADER_PROGRAM_INFO  psPDSVertexShaderProgramInfo);
																 
PDS_CALLCONV IMG_VOID PDSPatchVertexShaderProgram(PPDS_VERTEX_SHADER_PROGRAM_INFO  psPDSVertexShaderProgramInfo,
												  IMG_UINT32                      *pui32Buffer);
PDS_CALLCONV IMG_UINT32 * PDSGenerateConstUploadProgram
						(PPDS_CONST_UPLOAD_PROGRAM       psProgram,
						 IMG_UINT32                      *pui32Buffer);
						 
PDS_CALLCONV IMG_UINT32 * PDSGeneratePixelEventProgram		(PPDS_PIXEL_EVENT_PROGRAM		psProgram,
															 IMG_UINT32						*pui32Buffer,
															 IMG_BOOL						bMultisampleWithoutDownscale,
															 IMG_BOOL						bDownscaleWithoutMultisample,
															 IMG_UINT32						ui32MultiSampleQuality);

PDS_CALLCONV IMG_UINT32 * PDSGeneratePixelEventProgramTileXY(PPDS_PIXEL_EVENT_PROGRAM		psProgram,
															 IMG_UINT32						*pui32Buffer);

#if !defined(PDS_BUILD_OPENGLES)
#if defined(SGX545)
PDS_CALLCONV IMG_UINT32 * PDSGenerateSCWordCopyProgram	(PPDS_SCWORDCOPY_PROGRAM	psProgram,
														 IMG_UINT32 *						pui32Buffer);
														 
PDS_CALLCONV IMG_UINT32 * PDSGenerateTessPhase2Program		(PPDS_TESSELATOR_PROGRAM	psProgram,
									 IMG_UINT32						*pui32Buffer);
PDS_CALLCONV IMG_UINT32 * PDSGenerateITPProgram	(PPDS_ITP_PROGRAM	psProgram,
							 IMG_UINT32 *		pui32Buffer);
							 
PDS_CALLCONV IMG_UINT32 * PDSGenerateVtxBufWritePointerProgram	(PPDS_VTXBUFWRITEPOINTER_PROGRAM	psProgram,
							 IMG_UINT32 *		pui32Buffer);
#endif

PDS_CALLCONV IMG_UINT32 * PDSGenerateLoopbackVertexShaderProgram	(PPDS_VERTEX_SHADER_PROGRAM	psProgram,
																	 IMG_UINT32					*pui32Buffer);
																	 
PDS_CALLCONV IMG_UINT32 * PDSGenerateTessVertexShaderProgram	(PPDS_VERTEX_SHADER_PROGRAM	psProgram,
																 IMG_UINT32					ui32InputVertexStride,
																 IMG_UINT32					*pui32Buffer,
																 IMG_UINT32					ui32Address);
																 
PDS_CALLCONV IMG_UINT32 * PDSGenerateTessSAProgram	(PPDS_TESSELATOR_SA_PROGRAM	psProgram,
														 IMG_UINT32 *						pui32Buffer,
														 IMG_BOOL							bShadowSAs);
														 
PDS_CALLCONV IMG_UINT32 * PDSGenerateTessProgram			(PPDS_TESSELATOR_PROGRAM	psProgram,
															 IMG_UINT32						*pui32Buffer);
															 
PDS_CALLCONV IMG_UINT32 * PDSGenerateTessIndicesProgram		(PPDS_TESSELATOR_PROGRAM	psProgram,
															 IMG_UINT32						*pui32Buffer);
															 
PDS_CALLCONV IMG_UINT32 * PDSGenerateTessVertexProgram		(PPDS_TESSELATOR_PROGRAM	psProgram,
															 IMG_UINT32						*pui32Buffer);
															 
PDS_CALLCONV IMG_UINT32 * PDSGenerateInterPipeSDProgram	(PPDS_IPSD_PROGRAM	psProgram,
														 IMG_UINT32 *							pui32Buffer,
														 IMG_BOOL		bClearSyncFlag);
														 
														 
PDS_CALLCONV IMG_UINT32 * PDSGenerateGenericKickUSEProgram(PPDS_GENERICKICKUSE_PROGRAM	psProgram,
															IMG_UINT32				*pui32Buffer);
#if !defined(PDS_BUILD_OPENGLES)

PDS_CALLCONV IMG_UINT32 * PDSGenerateComplexAdvanceFlushProgram	(PPDS_COMPLEXADVANCEFLUSH_PROGRAM	psProgram,
																 IMG_UINT32 *		pui32Buffer);
																 
PDS_CALLCONV IMG_UINT32 * PDSGenerateVSMemConstUploadProgram	(PPDS_VS_MEMCONST_UPLOAD_PROGRAM	psProgram,
																 IMG_UINT32 *						pui32Buffer);
#endif /* #if !defined(PDS_BUILD_OPENGL) && !defined(PDS_BUILD_OPENGLES) */

#endif /* #if !defined(PDS_BUILD_OPENGLES) || defined(GLES1_EXTENSION_CARNAV) */

#if defined(PDS_BUILD_OPENGLES) || defined(PDS_BUILD_D3DM)
PDS_CALLCONV IMG_UINT32 *PDSGenerateTerminateStateProgram	(PDS_TERMINATE_STATE_PROGRAM	*psProgram,
															 IMG_UINT32						*pui32Buffer);
															 
PDS_CALLCONV IMG_VOID PDSPatchTerminateStateProgram (PDS_TERMINATE_STATE_PROGRAM *psProgram,
														IMG_UINT32 *pui32Buffer);
														
#endif

PDS_CALLCONV IMG_UINT32 PDSGetPixeventProgSize(IMG_VOID);
PDS_CALLCONV IMG_UINT32 PDSGetPixeventTileXYProgSize(IMG_VOID);

#endif /* _PDS_H_ */

/*****************************************************************************
 End of file (pds.h)
*****************************************************************************/
