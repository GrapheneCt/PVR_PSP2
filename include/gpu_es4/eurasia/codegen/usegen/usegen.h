/***************************************************************************//**
 * @file           usegen.h
 *
 * @brief          Helper functions to build various pieces of common USE code
 *
 * @details        
 *
 * @author         Donald Scorgie
 *
 * @date           15/10/2008
 *
 * @Version        $Revision: 1.55 $
 *
 * @Copyright      Copyright 2008 by Imagination Technologies Limited.
 *                All rights reserved. No part of this software, either material
 *                or conceptual may be copied or distributed, transmitted,
 *                transcribed, stored in a retrieval system or translated into
 *                any human or computer language in any form by any means,
 *                electronic, mechanical, manual or otherwise, or disclosed
 *                to third parties without the express written permission of
 *                Imagination Technologies Limited, Home Park Estate,
 *                Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 ******************************************************************************/

/******************************************************************************
Modifications :-

$Log: usegen.h $

******************************************************************************/

#ifndef _usegen_h_
#define _usegen_h_

#include "img_types.h"
#include "img_3dtypes.h"
#include "servicesext.h"
#include "pixevent.h"

/*
 * About USEGen
 *
 * USEGen is a series of useful USE programs that are common to different
 * drivers.
 *
 * In some cases, the USEGen function will only write a fragment of code
 * and the driver must supply additional code to complete the program.
 * An example of this is the "emit state" program.  In this case,
 * USEGen will NOT write the actual emit (as this will depend on what the
 * program is used for), or any preamble (PHAS instructions).
 *
 * When USEgen writes a complete program, the function name should
 * end in Program (i.e. USEGenWriteClearProgram).  When USEgen
 * only writes a fragment, it should end in Fragment (i.e.
 * USEGenWriteStateFragment).
 *
 * The size of the USEGen result can be specified in two ways.
 * If the fragment is of fixed length, there will be a define
 * that specifies the size in bytes of the program.
 *
 * If the fragment size is dependent on inputs, there will
 * be a separate calculation program which should return
 * the required size (in bytes).
 *
 * All (non-calculating) functions should take (at least) a pointer to the
 * (linear) address to write to.  It is the responsibility of the caller
 * to ensure enough memory has been allocated for the function
 *
 * All (non-calculating) functions should also return a pointer past the end of
 * their written block.
 *
 * For functions relating to vertex processing and with chips supporting
 * VCB culling, the main program will always refer to the post-cull phase
 * while a separate function will be declared for the pre-cull section.
 */

/*
 * USEGen optional features:
 * To stop inclusion of unnecessary code for those not wanting it,
 * some USEGen functions are behind USEGEN_SUPPORT_ defines.  In
 * addition, these are separated into different source files.
 *
 * Currently, these are:
 * USEGEN_SUPPORT_BLENDING (For blending fragments)
 * USEGEN_SUPPORT_PACKUNPACK (For format conversion fragments)
 *
 * To enable these, the relevant define must be added, and the source
 * file added to the build system.
 */

/******************************************************************************
 * Useful defines
 ******************************************************************************/
/**
 * Convert USE instruction counts to bytes
 */
#define USE_INST_TO_BYTES(x) ((x) * EURASIA_USE_INSTRUCTION_SIZE)

/**
 * Convert USE instruction counts to UINT32s
 */
#define USE_INST_TO_UINT32(x) ((x) * EURASIA_USE_INSTRUCTION_SIZE / sizeof(IMG_UINT32))

/**
 * Convert number of bytes into USE instruction count
 * Well, it might come in handy some day
 */
#define USE_BYTES_TO_INSTS(x) (((x) / EURASIA_USE_INSTRUCTION_SIZE))

/**
 * Number of UINT32s that make up a USE instruction
 */
#define USE_INST_LENGTH (2)

/******************************************************************************
 * Structure and enum definitions
 ******************************************************************************/

/**
 * The "Bank" of registers a particular register can be found in.
 */
typedef enum _USEGEN_BANK_TYPE_
{
	USEGEN_REG_BANK_PRIM, /**< Primary Attributes */
	USEGEN_REG_BANK_TEMP, /**< Temporaries */
	USEGEN_REG_BANK_OUTPUT, /**< Output */
} USEGEN_BANK_TYPE;

/**
 * The precision for the blend program.
 */
typedef enum _USEGEN_BLEND_PRECISION_
{
	USEGEN_BLEND_PRECISION_F32 = 0, /**< 32bit floating point (the default fallback) */
	USEGEN_BLEND_PRECISION_F16, /**< 16bit floating point (see USEGenF16BlendingAvail) */
	USEGEN_BLEND_PRECISION_INT, /**< 8bit integer (see USEGenIntBlendingAvail) */
} USEGEN_BLEND_PRECISION;

/**
 * Program describing a blend operation
 */
typedef struct _USEGEN_BLEND_PROG_
{
	IMG_UINT32 ui32SrcReg; /**< The register number containing the
							*   result of the pixel shader program */
	USEGEN_BANK_TYPE eSrcBank; /**< The bank to find the source in */

	IMG_UINT32 ui32DstReg; /**< The register number containing the
							*   current surface value */
	USEGEN_BANK_TYPE eDestBank; /**< Bank to find the dest in */

	IMG_BLEND eSrcBlend; /**< The selected blend type for the Src */
	IMG_BLEND eSrcAlphaBlend; /**< The selected blend type for the Src
								  *   alpha */
	IMG_BLENDOP eOp; /**< The operation to perform for colour */
	IMG_BLENDOP eAlphaOp; /**< The operation to perform for alpha */

	IMG_BLEND eDstBlend; /**< The selected blend type for the Dst */
	IMG_BLEND eDstAlphaBlend; /**< The selected blend type for the Dst
								  *   alpha */

	IMG_UINT32 ui32WriteMask; /**< The write mask for the output.
							   *   Comprising 1 bit per channel (up to 4
							   *   channels)
							   *   bit 0=Red, 1=Green, 2=Blue, 3=Alpha
							   */
	IMG_FLOAT afBlendFactors[4]; /**<  Floating point factors for the
								   *    IMG_BLEND_FACTOR operation */
								   
	IMG_BOOL bUseDstAlpha; /**< If false, should not use Dst alpha channel data */

	USEGEN_BLEND_PRECISION eBlendPrecision;	/**< Selects the precision of the blend.
										 *	 Both the sources and the destination
										 *	 MUST be presented at the selected
										 *	 precision and the relevant pre-check
										 *	 functions (eg: USEGenIntBlendingAvail)
										 *	 MUST be called prior to generating
										 *	 the blend program.
										 */

	/* Outputs */
	IMG_UINT32 ui32TempCount; /** Number of temporaries required for the
							   *  program */
} USEGEN_BLEND_PROG, *PUSEGEN_BLEND_PROG;


/**
 * Potentially required constants for a MOV_MASK emulation
 */
#define USEGEN_MOVMSK_EMU_CONST0 (0x3E4C0000)
#define USEGEN_MOVMSK_EMU_CONST1 (0x3E500000)
#define USEGEN_MOVMSK_EMU_CONST2 (0x3E480000)

/**
 * Constants used when doing gamma correction using secondary attributes
 */
#define USEGEN_GAMMA_CONST0 (0x3b4d2e1c)
#define USEGEN_GAMMA_CONST1 (0x414eb852)
#define USEGEN_GAMMA_CONST2 (0x3ed55555)
#define USEGEN_GAMMA_CONST3 (0xbd6147ae)

/**
 * Values we can ask to load into SA registers
 */
typedef enum _USEGEN_SA_LOAD_TYPE_
{
	USEGEN_SA_LOAD_FOGEND, /**< FogEnd */
	USEGEN_SA_LOAD_INV_FOGDIFF, /**< 1 / (FogEnd - FogStart) */
	USEGEN_SA_LOAD_DENSITY, /**< Fog density */
	USEGEN_SA_COLOUR_INT, /**< The fog colour should be loaded as
						   *   8 bit integers in 1 register
						   *   in the format A8R8G8B8 */
	USEGEN_SA_COLOUR_R, /**< Load the red channel of the fog colour as
						 *   a float  */
	USEGEN_SA_COLOUR_G, /**< Load the green channel of the fog colour as
						 *   a float  */
	USEGEN_SA_COLOUR_B, /**< Load the blue channel of the fog colour as
						 *   a float  */
} USEGEN_SA_LOAD_TYPE;

/**
 * When asking for an SA load, we supply the SA register number
 * and the load type
 */
typedef struct _USEGEN_SA_LOAD_
{
	IMG_UINT32 ui32SARegister;
	USEGEN_SA_LOAD_TYPE eType;
} USEGEN_SA_LOAD;

/**
 * Program describing a special "fog" blending operation
 * A fog operation will modify the pixel shader results in-place
 */
typedef struct _USEGEN_FOG_PROG_
{
	IMG_UINT32 ui32SrcReg; /**< The register number containing the
							*   result of the pixel shader program */
	USEGEN_BANK_TYPE eSrcBank; /**< The bank to find the source in */

	IMG_BOOL bInt; /**< If set, this indicates the blending should be
					*   performed as an 8-bit integer per channel
					*   operation
					*/

	IMG_UINT32 ui32SecAttrBase; /**< Base secondary attribute to use
								 *   for any constants which need
								 *   loading */
	IMG_UINT32 ui32FogCoord; /**< Primary attribute register to find the
							  *   required attribute for the fog index
							  *   base.  If eMode == IMG_FOGMODE_NONE,
							  *   this will be a single value of the
							  *   previously calculated fog index
							  *   For any other type, this will be
							  *   the 4-component iterated position
							  */
	IMG_BOOL bFogCoordIsInt; /**< If set, the above ui32FogCoord has been
							  *   iterated as an integer (i.e. the value
							  *   comes from a colour value not a fog
							  *   iteration)
							  */

	IMG_BOOL bAffine; /**< If set, indicates an affine projection is used
					   *   (WNear == 1.0 && WFar == 1.0)
					   *   In this case, the fog index will be based on
					   *   the Z coordinate of the position instead of the
					   *   W component
					   */
	IMG_BOOL bStartIsEnd; /**< If set, the fog start and end values are
						   *   identical
						   */

	IMG_FOGMODE eMode; /**< The requested mode to perform the Fog blending
						*   with
						*/
	/* Outputs */
	IMG_UINT32 ui32TempCount; /**< Number of temporaries required for the
							   *  program */
	USEGEN_SA_LOAD asLoads[6]; /**< Details of any attribute values we need
								*   loaded into SA registers
								*/
	IMG_UINT32 ui32NumLoads; /**< Number of loads to perform */
} USEGEN_FOG_PROG, *PUSEGEN_FOG_PROG;

/**
 * Program describing ISP Feedback
 */
typedef struct _USEGEN_ISP_FEEDBACK_PROG_
{
	IMG_UINT32 		ui32PredReg; 	/**< The predicate register to base 
									 *   the texkill on
									 */
	IMG_UINT32 		ui32StartPA;	/**< The base PA (+2 more) to use for the PCOEFF.
									 *
									 *   e.g, ui32StartPA is 20, then this func
									 *   builds "PCOEFF PA20, PA21, PA22". You usually
									 *   want this to be:
									 *   UNIFLEX_HW.uPrimaryAttributeCount - 3 or so
									 *
									 *   see also #USEGEN_WRITE_ISP_FEEDBACK_FRAGMENT_BYTES
									 *   see also #USEGEN_WRITE_ISP_FEEDBACK_FRAGMENT_INST
									 *   see also #USEGEN_WRITE_ISP_FEEDBACK_PAREG_COUNT
									 */
	IMG_UINT32 		ui32OutputPA;	/**< The PRIMATTR register uniflex puts the
									 *   result of the pixel shader into
									 *   used as the value to be alphatested
									 */
	IMG_UINT32 		ui32TempBase;	/**< First available temp for use in 
									 *   this fragment
									 */
	IMG_COMPFUNC 	eDepthCmp;		/**< The depth comparison mode
									 */
	IMG_COMPFUNC 	eAlphaCmp;		/**< The alphatest comparison mode
									 */
	IMG_UINT8		ui8AlphaRef;	/**< The alphatest reference value that pixel
									 *   shader results are compared against
									 */

	IMG_BOOL		bDepthEnable;	/**< Depth testing is enabled */

	IMG_BOOL		bDWriteDis;		/**< The Depth Write Disable state
									 */

	IMG_BOOL		bStencilFFEnable;	/**< The Stencil Front Face Enable state */

	IMG_BOOL		bStencilBFEnable;	/**< The Stencil Back Face Enable state */

	IMG_BOOL		bFloatToAlpha;	/**< Pack the f32 alpha component of a pixel
									 *   shader result so it can be alpha tested
									 *
									 *   (spread across four registers in the
									 *   manner described in USEGenWritePackFragment)
									 */
	IMG_BOOL		bAlphaToCoverage; /**< Use Alpha to Coverage section */

	IMG_BOOL		bSampleMask;	/**< Load additional mask from software */

	IMG_BYTE		bySampleMask;	/**< Sample mask to be used when writing pixels */

	IMG_BOOL		bOMask;			/**< Shader outputs mask */

	USEGEN_BANK_TYPE	eMaskBank;	/**< USE bank storing the output mask */

	IMG_UINT32		ui32MaskReg;	/**< the position of the mask in the bank */

	IMG_BOOL 		bDepthWriteEnable;	/**< Whether we should do a depth write
										 */
	IMG_BOOL 		bTexkillEnable;	/**< Whether we should perform the texkill
									 *   operation
									 */
	IMG_BOOL 		bAlphaTest;		/**< Whether we should perform an alpha
									 *   test
									 */

	IMG_COMPFUNC	eStencilFFCmp;	/**< Stencil comparison mode (front face)
									 */

	IMG_STENCILOP	eStPassFFOp;	/**<  Operation to perform on stencil pass
									 *    (front face)
									 */

	IMG_STENCILOP	eStFailFFOp;	/**<  Operation to perform on stencil
									 *    failure (front face)
									 */

	IMG_STENCILOP	eStZFailFFOp;	/**< Operation to perform on Z failing
									 *   (front face)
									 */
	IMG_COMPFUNC	eStencilBFCmp;	/**< Stencil comparison mode (back face)
									 */

	IMG_STENCILOP	eStPassBFOp;	/**<  Operation to perform on stencil pass
									 *    (back face)
									 */

	IMG_STENCILOP	eStFailBFOp;	/**<  Operation to perform on stencil
									 *    failure (back face)
									 */

	IMG_STENCILOP	eStZFailBFOp;	/**< Operation to perform on Z failing
									 *   (back face)
									 */

	IMG_UINT8		ui8StencilRef;	/**< Reference stencil value
									 */
	IMG_UINT8		ui8StencilReadMask;	/**< Stencil read mask value
										 */
	IMG_UINT8		ui8StencilWriteMask;/**< Stencil write mask value
										 */

#if defined (FIX_HW_BRN_31547)
	IMG_BOOL		bZABS4D;		/**< BRN31547 makes ZABS iterate 4 values
									 * for MSAA renders.
									 */
#endif

	/* outputs */
	IMG_UINT32		ui32MaxTempUsed; /**< The index of the maximum temp used,
									  *   is equal to the ui32TempBase plus
									  *   any temps used in writing this program
									  */

	IMG_UINT32		ui32Offset;		/**< Offset to second part of the feedback
									 * program. The first part only contains
									 * enough instructions to perform a PCOEFF
									 */
} USEGEN_ISP_FEEDBACK_PROG, *PUSEGEN_ISP_FEEDBACK_PROG;

/**
 * Details about an integer register
 */
typedef struct _USEGEN_INT_REG_
{
	IMG_UINT32 ui32PASrc; /**< The base PA for the register */
	IMG_UINT32 ui32Mask; /**< The mask of elements in the register */
} USEGEN_INT_REG, *PUSEGEN_INT_REG;

/**
 * Program describing integer register storing
 */
typedef struct _USEGEN_INT_REG_STORE_PROG_
{
	IMG_BOOL bGeometryShader; /**< This fragment is for a geometry shader */
	IMG_UINT32 ui32NumRegs; /**< The number of registers in psRegs */
	PUSEGEN_INT_REG psRegs; /**< Details of each integer register.
							 *   The order registers appear should be the same
							 *   in the load and store programs. */

	IMG_UINT32 ui32VertexIDReg; /**< The vertex ID PA */
	IMG_UINT32 ui32IntRegVertexSize; /**< The size of the integer registers
									  *   in each vertex */

	IMG_DEV_VIRTADDR uBaseAddress; /**< Base address of the memory integer
									*   registers are written to */
	IMG_UINT32 ui32BaseAddressSA; /**< SA containing base address. Used for
								   *   vertex shaders. */

	IMG_BOOL bNonCGInstancing; /**< The program should take into account
								*   instancing (ignored for GS). */
	IMG_UINT32 ui32InstanceIDReg; /**< The instance ID PA */
	IMG_UINT32 ui32IdxCount; /**< The number of vertices drawn per instance */
} USEGEN_INT_REG_STORE_PROG, *PUSEGEN_INT_REG_STORE_PROG;

/**
 * Program describing integer register loading
 */
typedef struct _USEGEN_INT_REG_LOAD_PROG_
{
	IMG_UINT32 ui32NumRegs; /**< The number of registers in psRegs */
	PUSEGEN_INT_REG psRegs; /**< Details of each integer register.
							 *   The order registers appear should be the same
							 *   in the load and store programs. */
} USEGEN_INT_REG_LOAD_PROG, *PUSEGEN_INT_REG_LOAD_PROG;

/******************************************************************************
 * Static Program Sizes
 ******************************************************************************/

/**
 * The number of instructions of "preamble" -
 * instructions that must be in every USE program
 * (but not necessarily fragments)
 *
 * In chips supporting unlimited phases, this is the
 * PHAS instruction.
 */
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
#define USEGEN_PREAMBLE_COUNT 1
#else
#define USEGEN_PREAMBLE_COUNT 0
#endif


/******************************************************************************
 * Fragment sizes
 ******************************************************************************/

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
/**
 * Size of the phase fragment
 */
#define USEGEN_PHASE_FRAGMENT_BYTES (USE_INST_TO_BYTES (1))
#else
/**
 * With limited phases, the phase fragment doesn't
 * write anything
 */
#define USEGEN_PHASE_FRAGMENT_BYTES (0)
#endif

/**
 * Size of the Dummy End Fragment
 */
#define USEGEN_DUMMY_END_FRAGMENT_BYTES (USE_INST_TO_BYTES (1))

#if defined(SGX_FEATURE_VCB)
/**
 * Size of the End of Pre-Cull Vertex Shader Fragment
 */
#define USEGEN_END_PRECULL_VERTEX_SHADER_FRAGMENT_BYTES (USE_INST_TO_BYTES (1))
#endif

/**
 * Size of the End of Vertex Shader Fragment
 * On chips supporting VCB, this will write the Post-VCB phase
 * finish of the vertex shader
 */
#define USEGEN_END_VERTEX_SHADER_FRAGMENT_BYTES (USE_INST_TO_BYTES (1))

/**
 * Size of the End of Vertex Shader Fragment when guard band clipping
 * is enabled.
 * On chips supporting VCB, this will write the Post-VCB phase
 * finish of the vertex shader
 */
#define USEGEN_END_VERTEX_SHADER_GB_FRAGMENT_BYTES (USE_INST_TO_BYTES (11))

/**
 * Size of the ISP Feedback/Texkil stage. For Uniflex uFeedbackInstCount.
 **/
#define USEGEN_WRITE_ISP_FEEDBACK_FRAGMENT_BYTES (USE_INST_TO_BYTES(17)+USEGEN_DUMMY_END_FRAGMENT_BYTES)
#define USEGEN_WRITE_ISP_FEEDBACK_FRAGMENT_INST (17+USE_BYTES_TO_INSTS(USEGEN_DUMMY_END_FRAGMENT_BYTES))

/**
 * The number of PA registers that UseGenWriteISPFeedbackFragment needs.
 * This is mostly needed by Uniflex and it's uExtraPARegisters
 **/
#define USEGEN_WRITE_ISP_FEEDBACK_PAREG_COUNT (3)

#if defined(SGX545)
/**
 * Size of the End of Geometry Shader Fragment
 */
#define USEGEN_GEOMETRY_SHADER_END_FRAGMENT_BYTES			\
	(USE_INST_TO_BYTES (10))

#else
#define USEGEN_GEOMETRY_SHADER_END_FRAGMENT_BYTES			\
	(USE_INST_TO_BYTES (0))

#endif


/* todo: This is horrible */
#if EURASIA_PDS_IR0_PDM_TILEY_SHIFT == 9
#if EURASIA_PERISP_NUM_USE_PIPES_IN_X == 1 && EURASIA_PERISP_NUM_USE_PIPES_IN_Y > 1
#define USEGEN_PIXFROMTILE_FRAG_BYTES (USE_INST_TO_BYTES(28 + USEGEN_PREAMBLE_COUNT))
#else /* EURASIA_PERISP_NUM_USE_PIPES_IN_X == 1 && EURASIA_PERISP_NUM_USE_PIPES_IN_Y > 1 */
#define USEGEN_PIXFROMTILE_FRAG_BYTES (USE_INST_TO_BYTES(25 + USEGEN_PREAMBLE_COUNT))
#endif /* EURASIA_PERISP_NUM_USE_PIPES_IN_X == 1 && EURASIA_PERISP_NUM_USE_PIPES_IN_Y > 1 */
#else /* EURASIA_PDS_IR0_PDM_TILEY_SHIFT == 9 */
#if EURASIA_PERISP_NUM_USE_PIPES_IN_X == 2 && EURASIA_PERISP_NUM_USE_PIPES_IN_Y == 1
#define USEGEN_PIXFROMTILE_FRAG_BYTES (USE_INST_TO_BYTES(23 + USEGEN_PREAMBLE_COUNT))
#else /* EURASIA_PERISP_NUM_USE_PIPES_IN_X == 2 && EURASIA_PERISP_NUM_USE_PIPES_IN_Y == 1 */
#define USEGEN_PIXFROMTILE_FRAG_BYTES (USE_INST_TO_BYTES(27 + USEGEN_PREAMBLE_COUNT))
#endif /* EURASIA_PERISP_NUM_USE_PIPES_IN_X == 2 && EURASIA_PERISP_NUM_USE_PIPES_IN_Y == 1 */
#endif /* EURASIA_PDS_IR0_PDM_TILEY_SHIFT == 9 */

#define USEGEN_PIXDIRECT_FRAGMENT_BYTES (USE_INST_TO_BYTES(3))


/*
 * About the twiddle table
 *
 * The most efficent way to deal with twiddled ressources is to create a twiddle
 * table and read the offsets from it through the secondaries attributes. It contains
 * the twiddled pixel's offset from the start of the tile.
 *
 * The table is divided in two sections: 
 *  - The first section is filled based on the size of the region.
 *  - The second section is filled based on the maximum render size and the width of the region.
 */

/*
 * Twiddle table sizes.
 * These refer to the two sections when filling the twiddle table.
 */
#define USEGEN_TWIDDLE_TABLE_SIZE1 (EURASIA_ISPREGION_SIZEY * EURASIA_ISPREGION_SIZEX * 2)
#define USEGEN_TWIDDLE_TABLE_SIZE2 (((max(EURASIA_RENDERSIZE_MAXX, EURASIA_RENDERSIZE_MAXY) / EURASIA_ISPREGION_SIZEX) * 2))
/*
 * About the hybrid twiddle table.
 *
 * For hybrid twiddling, a look up table is required per tile size; 16*8, 8*4, 4*2 and 2*1. For convenience, they are 
 * all stored in one block. The appropriate one is then called by specifying the offset from the start of the array.
 */

/*
 * Hybrid twiddle table size and address offset.
 */
/* Total size of the twiddle tables is ((16 * 8) + (8 * 4) + (4 * 2) + (2 * 1)) * sizeof(IMG_UINT16). */
#define USEGEN_HYBRID_TWIDDLE_TABLE_SIZE (170 * sizeof(IMG_UINT16))
/* 16*8 is the first entry in the array. */
#define USEGEN_HYBRID_TWIDDLING_SIZE_16_OFFSET	(0)
/* Then 8*4, offset is 16 * 8 * sizeof(IMG_UINT16). */
#define USEGEN_HYBRID_TWIDDLING_SIZE_8_OFFSET	(128 * sizeof(IMG_UINT16))
/* Then 4*2, offset is obtained by adding the offset of the previous table in the array plus its size. */
#define USEGEN_HYBRID_TWIDDLING_SIZE_4_OFFSET	(USEGEN_HYBRID_TWIDDLING_SIZE_8_OFFSET + 32 * sizeof(IMG_UINT16))
/* Then 2*1, offset is obtained by adding the offset of the previous table in the array plus its size. */
#define USEGEN_HYBRID_TWIDDLING_SIZE_2_OFFSET	(USEGEN_HYBRID_TWIDDLING_SIZE_4_OFFSET + 8 * sizeof(IMG_UINT16))

/*
 * Shift required to go from a twiddled tile/USE address to a pixel address.
 */
#if EURASIA_PERISP_NUM_USE_PIPES_IN_X == 2 && EURASIA_PERISP_NUM_USE_PIPES_IN_Y == 1
/* -1 for two pipes striped in X so the pipe number is the bottom bit. */
#define USEGEN_TWIDDLED_ADDRESS_TILE_SHIFT	(EURASIA_PERISP_TILE_SHIFTX + EURASIA_PERISP_TILE_SHIFTY - 1)
#elif EURASIA_PERISP_NUM_USE_PIPES_IN_X == 1 && EURASIA_PERISP_NUM_USE_PIPES_IN_Y == 1
#define USEGEN_TWIDDLED_ADDRESS_TILE_SHIFT	(EURASIA_PERISP_TILE_SHIFTX + EURASIA_PERISP_TILE_SHIFTY)
#elif EURASIA_PERISP_NUM_USE_PIPES_IN_X == 1 && EURASIA_PERISP_NUM_USE_PIPES_IN_Y == 2
/* -2 for two pipes striped in Y so the pipe number is the bottom bit, then a gap of one, then the tile address. */
#define USEGEN_TWIDDLED_ADDRESS_TILE_SHIFT	(EURASIA_PERISP_TILE_SHIFTX + EURASIA_PERISP_TILE_SHIFTY - 2)
#elif EURASIA_PERISP_NUM_USE_PIPES_IN_X == 2 && EURASIA_PERISP_NUM_USE_PIPES_IN_Y == 2
/* -2 for four pipes with the pipe X and the pipe Y in the bottom two bits. */
#define USEGEN_TWIDDLED_ADDRESS_TILE_SHIFT	(EURASIA_PERISP_TILE_SHIFTX + EURASIA_PERISP_TILE_SHIFTY - 2)
#else
/* FIXME: This pipeline configuration is not supported for twiddled MRTs */
#define USEGEN_TWIDDLED_ADDRESS_TILE_SHIFT	0
#endif

#if !defined(SGX_FEATURE_PIXELBE_32K_LINESTRIDE)
#define EMIT2_SIDEBAND	4
#else
#define EMIT2_SIDEBAND	5
#endif
#define EMIT_ADDRESS (3)

#if defined(SGX545)
#define USEGEN_EOT_LOAD_FRAG_MAX_BYTES (USE_INST_TO_BYTES(44))
#else
#define USEGEN_EOT_LOAD_FRAG_MAX_BYTES (USE_INST_TO_BYTES(68))
#endif

#if (defined(FIX_HW_BRN_23194) || defined(FIX_HW_BRN_23460) || defined(FIX_HW_BRN_23949))
#define USEGEN_EOT_EMIT_FRAG_MAX_BYTES (USE_INST_TO_BYTES(3 + STATE_PBE_DWORDS + 2 + 7))
#else
#define USEGEN_EOT_EMIT_FRAG_MAX_BYTES (USE_INST_TO_BYTES(3 + STATE_PBE_DWORDS + 2 + 2))
#endif
#define USEGEN_EOT_RTA_SETUP_MAX_BYTES (USE_INST_TO_BYTES(2))

#define USEGEN_DEPENDANT_READ_MAX_BYTES (USE_INST_TO_BYTES(10))

/*
 * Unpack Fragment Sizes
 * These unpack the specified type to 4 F32 registers
 * (padding any undefined destinations to 1.0f)
 * This is useful when using vertex shaders
 */
#define USEGEN_UNPACK_FRAG_MAX_BYTES (USE_INST_TO_BYTES (27))

#define USEGEN_PACK_FRAG_MAX_BYTES (USE_INST_TO_BYTES(31))

#define USEGEN_PACK_MAX_TEMPS_USED 5

/* number of temps used by the integer register storing and loading programs */
#define USEGEN_INT_REG_STORE_TEMPS_USED 2
#define USEGEN_INT_REG_LOAD_TEMPS_USED 1

/******************************************************************************
 * Program sizes
 ******************************************************************************/

/**
 * Size of the Clear Pixel Shader Program
 */
#define USEGEN_CLEAR_PIX_PROGRAM_BYTES (USE_INST_TO_BYTES (1+USEGEN_PREAMBLE_COUNT))

#if defined(SGX_FEATURE_VCB)
/**
 * Size of the Pre-VCB Clear Vertex Shader
 */
#define USEGEN_CLEAR_PRECULL_VTX_PROGRAM_BYTES \
	(USE_INST_TO_BYTES (2+USEGEN_PREAMBLE_COUNT))
#endif

/**
 * Size of the Clear Vertex Shader
 * On chips supporting VCB, this will write the Post-VCB phase
 * of the clear
 */
#if defined(SGX_FEATURE_VCB)
#define USEGEN_CLEAR_VTX_PROGRAM_BYTES (USE_INST_TO_BYTES (1+USEGEN_PREAMBLE_COUNT))
#else
#define USEGEN_CLEAR_VTX_PROGRAM_BYTES (USE_INST_TO_BYTES (2+USEGEN_PREAMBLE_COUNT))
#endif

/*
 * Size of the maximum gamma fragment.  It is guaranteed
 * the gamma fragment generation will never use more than
 * this size
 */
#define USEGEN_GAMMA_FRAGMENT_MAX_BYTES \
	(USE_INST_TO_BYTES (38))

/*
 * Size of the maximum blending program.  It is guaranteed
 * the blending function will never use more than this
 * size.
 */
#define USEGEN_BLEND_PROGRAM_MAX_BYTES \
	(USE_INST_TO_BYTES (14 + USEGEN_PREAMBLE_COUNT))

/*
 * Size of the maximum fog fragment.  It is guaranteed
 * the fog fragment generation will never use more than
 * this size
 */
#define USEGEN_FOG_FRAGMENT_MAX_BYTES \
	(USE_INST_TO_BYTES (11 + USEGEN_PREAMBLE_COUNT))

/*
 * These programs have only been written for SGX545 currently
 * They are related to Complex Geometry
 * @todo Other versions need written?
 */
#if defined(SGX545)
/**
 * Size of the Complex Geometry ITP USE Program
 */
#define USEGEN_CG_ITP_PROGRAM_BYTES (USE_INST_TO_BYTES (2+USEGEN_PREAMBLE_COUNT))

/**
 * Complex Geometry Phase 1 Emit USE Program Size
 */
#define USEGEN_CG_PHASE1_STATE_UPDATE_PROGRAM_BYTES \
	(USE_INST_TO_BYTES (4 + USEGEN_PREAMBLE_COUNT))

/**
 * Complex Geometry Vertex Buffer Write Pointer USE Program Size
 */
#define USEGEN_CG_VTX_WRITE_PTR_PROGRAM_BYTES \
	(USE_INST_TO_BYTES (5 + USEGEN_PREAMBLE_COUNT))

/**
 * Size of the Complex Geomentry Vertex DMA task
 * On chips supporting VCB, this will write the
 * post-Cull phase
 */
#define USEGEN_CG_VTX_DMA_PROGRAM_BYTES \
	(USE_INST_TO_BYTES (13+USEGEN_PREAMBLE_COUNT))

#define USEGEN_RTA_TERMINATE_STATE_PROG_BYTES \
	(USE_INST_TO_BYTES (6 + USEGEN_PREAMBLE_COUNT))

#else

/**
 * Size of the Complex Geometry ITP USE Program
 */
#define USEGEN_CG_ITP_PROGRAM_BYTES (0)

/**
 * Complex Geometry Phase 1 Emit USE Program Size
 */
#define USEGEN_CG_PHASE1_STATE_UPDATE_PROGRAM_BYTES \
	(0)

/**
 * Complex Geometry Vertex Buffer Write Pointer USE Program Size
 */
#define USEGEN_CG_VTX_WRITE_PTR_PROGRAM_BYTES \
	(0)

/**
 * Size of the Complex Geomentry Vertex DMA task
 * On chips supporting VCB, this will write the
 * post-Cull phase
 */
#define USEGEN_CG_VTX_DMA_PROGRAM_BYTES \
	(0)

#define USEGEN_RTA_TERMINATE_STATE_PROG_BYTES \
	(0)
#endif

#if defined(SGX545)
/**
 * Maximum size of a "special" geometry shader program
 */
#define USEGEN_SPECIAL_GS_PROGRAM_BYTES	\
	(USE_INST_TO_BYTES (USEGEN_PREAMBLE_COUNT + 20 + 50))

#else
#define USEGEN_SPECIAL_GS_PROGRAM_BYTES			\
	(USE_INST_TO_BYTES (0))
#endif

#define USEGEN_RTA_GS_SPECIAL_SECONDARIES (3)


/******************************************************************************
 * Calculation Functions
 ******************************************************************************/
IMG_INTERNAL IMG_UINT32 USEGenCalculateStateSize (IMG_UINT32 ui32NumWrites);

#if defined(SGX_FEATURE_VCB)
IMG_UINT32 USEGenCalculateVtxDMAPreCullSize (IMG_UINT32 ui32VtxSize);
#endif

IMG_INTERNAL IMG_UINT32 USEGenCalculateIntRegStoreSize(IMG_BOOL bGeometryShader,
										  IMG_BOOL bNonCGInstancing,
										  IMG_UINT32 ui32Elements);
IMG_INTERNAL IMG_UINT32 USEGenCalculateIntRegLoadSize(IMG_UINT32 ui32IntRegs);

/******************************************************************************
 * Fragment Generation Functions
 ******************************************************************************/
IMG_INTERNAL IMG_PUINT32 USEGenWriteDummyEndFragment (IMG_PUINT32 pui32Base);

IMG_INTERNAL IMG_PUINT32 USEGenSetUSEEndFlag (IMG_PUINT32 pui32Base);

#if defined(USEGEN_SUPPORT_PACKUNPACK)
IMG_PUINT32 USEGenWriteGammaFragment (IMG_PUINT32 pui32Base,
									 IMG_UINT32 ui32Reg,
									 USEGEN_BANK_TYPE eBank,
									 IMG_PUINT32 pui32TempsUsed,
									 IMG_UINT32 ui32ChannelMask,
									 IMG_BOOL bUseSec,
									 IMG_UINT32 ui32SecBase);

IMG_PUINT32 USEGenWriteDeGammaFragment (IMG_PUINT32 pui32Base,
									 IMG_UINT32 ui32Reg,
									 USEGEN_BANK_TYPE eBank,
									 IMG_PUINT32 pui32TempsUsed,
									 IMG_UINT32 ui32ChannelMask);

IMG_PUINT32 USEGenWriteUnpackRawFragment (IMG_PUINT32 pui32Base,
									   IMG_UINT32 ui32OrigReg,
									   USEGEN_BANK_TYPE eOrigBank,
									   IMG_UINT32 ui32DestReg,
									   USEGEN_BANK_TYPE eDestBank,
									   PVRSRV_PIXEL_FORMAT eOrigFmt,
									   IMG_BYTE byUsedMask,
									   IMG_UINT32 ui32TempBase,
									   IMG_PUINT32 pui32TempsUsed);

IMG_PUINT32 USEGenWriteUnpackFragment (IMG_PUINT32 pui32Base,
											IMG_UINT32 ui32OrigReg,
											USEGEN_BANK_TYPE eOrigBank,
											IMG_UINT32 ui32DestReg,
											USEGEN_BANK_TYPE eDestBank,
											PVRSRV_PIXEL_FORMAT eOrigFmt,
											IMG_BYTE byUsedMask,
											IMG_UINT32 ui32TempBase,
											IMG_PUINT32 pui32TempsUsed);

IMG_PUINT32 USEGenWritePackFragment (IMG_PUINT32 pui32Base,
									 PVRSRV_PIXEL_FORMAT eFormat,
									 IMG_UINT32 ui32Chunk,
									 IMG_UINT32 ui32OrigReg,
									 USEGEN_BANK_TYPE eOrigBank,
									 IMG_UINT32 ui32DestReg,
									 USEGEN_BANK_TYPE eDestBank,
									 IMG_UINT32 ui32TempBase,
									 IMG_PUINT32 pui32TempsUsed);
IMG_PUINT32 USEGenLoad4Vec(IMG_PUINT32 pui32Base,
						   USEGEN_BANK_TYPE eBank,
						   IMG_UINT32 ui32Reg,
						   IMG_BYTE byMask,
						   IMG_UINT32 ui32Value0,
						   IMG_UINT32 ui32Value1,
						   IMG_UINT32 ui32Value2,
						   IMG_UINT32 ui32Value3);
IMG_PUINT32 USEGenLoadNVec(IMG_PUINT32 pui32Base,
						   USEGEN_BANK_TYPE eBank,
						   IMG_UINT32 ui32Reg,
						   IMG_UINT32 ui32Mask,
						   IMG_PUINT32 pui32Values);

#if defined(SGX_FEATURE_PBE_GAMMACORRECTION)
IMG_PUINT32 USEGenWritePackForPBEGammaFragment (IMG_PUINT32 pui32Base,
												IMG_UINT32 ui32OrigReg,
												USEGEN_BANK_TYPE eOrigBank,
												IMG_UINT32 ui32DestReg,
												USEGEN_BANK_TYPE eDestBank);
#endif /* defined(SGX_FEATURE_PBE_GAMMACORRECTION) */
#endif /* defined(USEGEN_SUPPORT_PACKUNPACK) */

#if defined(SGX_FEATURE_VCB)
IMG_PUINT32 USEGenWriteEndPreCullVtxShaderFragment (IMG_PUINT32 pui32Base);
#endif

IMG_INTERNAL IMG_PUINT32 USEGenWriteEndVtxShaderFragment (IMG_PUINT32 pui32Base);

IMG_INTERNAL IMG_PUINT32 USEGenWriteEndVtxShaderGBFragment(IMG_PUINT32 pui32Base,
											  IMG_UINT32 ui32SecondaryGBBase);

IMG_INTERNAL IMG_PUINT32 USEGenWriteEndGeomShaderFragment (IMG_PUINT32 pui32Base,
											  IMG_UINT32 ui32NumPartitions,
											  IMG_BOOL bDoublePartition);
#if defined(FIX_HW_BRN_31988)
IMG_INTERNAL IMG_PUINT32 USEGenWriteBRN31988Fragment (IMG_PUINT32 pui32Base);

#endif

IMG_INTERNAL IMG_PUINT32 USEGenWriteStateEmitFragment (IMG_PUINT32 pui32Base,
										  IMG_UINT32 ui32NumWrites,
										  IMG_UINT32 ui32Start);

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)

typedef enum _USEGEN_PHASE_DEPENDANCY_
{
	USEGEN_NO_DEPENDANT, /**< The next phase can happen immediately */
	USEGEN_VCB_DEPENDANT, /**< Next phase can only execute after the VCB cull */
	USEGEN_PT_DEPENDANT /**< Next phase can only execute after phunchthrough is
						 *   tested */
} USEGEN_PHASE_DEPENDANCY;

IMG_INTERNAL IMG_PUINT32 USEGenWritePhaseFragment (IMG_PUINT32 pui32Base,
									  IMG_UINT32 ui32NextPhase,
									  IMG_UINT32 ui32NumTemps,
									  USEGEN_PHASE_DEPENDANCY eDependency,
									  IMG_BOOL bPerInstance,
									  IMG_BOOL bPerSample,
									  IMG_BOOL bLastPhase,
									  IMG_BOOL bEnd);

#endif

IMG_INTERNAL IMG_PUINT32 USEGenWriteISPFeedbackFragment(IMG_PUINT32 				 pui32BufferBase,
										   PUSEGEN_ISP_FEEDBACK_PROG psProg);

IMG_INTERNAL IMG_PUINT32 USEGenWritePixPosFromTileSecondaryFragment(IMG_PUINT32	pui32Base,
													   IMG_UINT32	ui32TileSA,
													   IMG_UINT32	ui32AddressSA,
													   IMG_UINT32	ui32StrideSA,
													   IMG_BOOL 	bTwiddled,
													   IMG_UINT32	ui32TTAddr,
													   IMG_UINT32	ui32TileSize,
													   IMG_UINT32	ui32ChunkByteSize,
													   IMG_INT32	i32Stride,
													   IMG_UINT32 	ui32Width,
													   IMG_UINT32 	ui32Height,
													   IMG_BOOL 	bWriteSetup,
													   IMG_BOOL 	bEnableRTAs,
													   IMG_UINT32 	ui32ArrayStride);

IMG_INTERNAL IMG_PUINT32 USEGenPixDirectFragment(IMG_PUINT32 pui32Base,
									IMG_UINT32 ui32AddressSA,
									IMG_UINT32 ui32StrideSA,
									IMG_UINT32 ui32DataSize,
									USEGEN_BANK_TYPE ePixBank,
									IMG_UINT32 ui32PixelReg,
									IMG_BOOL bTwiddled,
									IMG_UINT32 ui32TTAddr,
									IMG_UINT32 ui32TTStride,
									IMG_BOOL bLoad,
									IMG_BOOL bIssueWDF,
									IMG_BOOL bUsePredication,
									IMG_UINT32 ui32TwiddleTmpRegNum);

IMG_INTERNAL IMG_PUINT32 USEGenIntRegStoreFragment(PUSEGEN_INT_REG_STORE_PROG psProg,
									  IMG_PUINT32 pui32Buffer);
IMG_INTERNAL IMG_PUINT32 USEGenIntRegLoadFragment(PUSEGEN_INT_REG_LOAD_PROG psProg,
									 IMG_PUINT32 pui32Buffer);

IMG_INTERNAL IMG_PUINT32 USEGenWriteTileBufferSecProgram(IMG_PUINT32 pui32Base,
											IMG_UINT32 ui32FirstSA,
											IMG_UINT32 ui32NumBuffers,
											IMG_PUINT32 pui32TempCount);

IMG_INTERNAL IMG_PUINT32 USEGenTileBufferAccess(IMG_PUINT32 pui32Base,
								   IMG_UINT32 ui32TileSA,
								   IMG_UINT32 ui32DataSize,
								   IMG_UINT32 ui32DataReg,
								   USEGEN_BANK_TYPE eDataBank,
								   IMG_BOOL bLoad,
								   IMG_BOOL bIssueWDF);

typedef enum _USEGEN_SPECIALOBJ_TYPE_
{
	USEGEN_CLEAR, 
	USEGEN_CLEAR_PACKEDCOL, 
	USEGEN_ACCUM, 
	USEGEN_SCISSOR
} USEGEN_SPECIALOBJ_TYPE;


IMG_INTERNAL IMG_PUINT32 USEGenWriteSpecialObjVtxProgram (IMG_PUINT32 pui32Base,
											 USEGEN_SPECIALOBJ_TYPE eProgramType,
											IMG_DEV_VIRTADDR uBaseAddress, 
											IMG_DEV_VIRTADDR uCodeHeapBase, 
											IMG_UINT32 ui32CodeHeapBaseIndex);


IMG_INTERNAL IMG_PUINT32 USEGenWriteStateEmitProgram (IMG_PUINT32 pui32Base,
										 IMG_UINT32 ui32NumWrites, 
										 IMG_UINT32 ui32Start,
										 IMG_BOOL bComplex);

/******************************************************************************
 * Complete Program Generation Functions
 ******************************************************************************/
#if defined(SGX_FEATURE_VCB)
IMG_PUINT32 USEGenWritePreCullClearVtxProgram (IMG_PUINT32 pui32Base,
											   IMG_UINT32 ui32PostCullAddress,
											   IMG_UINT32 ui32NumOutputs);
#endif

IMG_INTERNAL IMG_PUINT32 USEGenWriteClearVtxProgram (IMG_PUINT32 pui32Base, IMG_UINT32 ui32NumOutputs);

#if defined(FIX_HW_BRN_29838)
	IMG_PUINT32 USEGenWriteClearPixelProgramF16(IMG_PUINT32 pui32Base);
#endif

IMG_INTERNAL IMG_PUINT32 USEGenWriteClearPixelProgram (IMG_PUINT32 pui32Base, IMG_BOOL bUseSecondary);
IMG_INTERNAL IMG_PUINT32 USEGenWriteAccum2PProgram (IMG_PUINT32 pui32Base);

IMG_INTERNAL IMG_PUINT32 USEGenWriteSpecialRTAGSProgram (IMG_PUINT32 pui32Base,
											IMG_UINT32 ui32VtxSize,
											IMG_PUINT32 pui32TempsUsed);

IMG_INTERNAL IMG_PUINT32 USEGenWriteCGPhase1StateEmitProgram (IMG_PUINT32 pui32Base,
												 IMG_UINT32 ui32State0,
												 IMG_UINT32 ui32State1,
												 IMG_UINT32 ui32State2,
												 IMG_UINT32 ui32State3);

IMG_INTERNAL IMG_PUINT32 USEGenWriteCGITPProgram (IMG_PUINT32 pui32Base);

IMG_INTERNAL IMG_PUINT32 USEGenWriteCGVtxBufferPtrProgram (IMG_PUINT32 pui32Base);

#if defined(SGX_FEATURE_VCB)
IMG_PUINT32 USEGenWritePreCullVtxDMAProgram (IMG_PUINT32 pui32Base,
											 IMG_UINT32 ui32VtxSize,
											 IMG_BOOL bViewportIdxPresent,
											 IMG_UINT32 ui32PostCullAddress,
											 IMG_BOOL bIterateVertexID);
#endif

IMG_INTERNAL IMG_PUINT32 USEGenWriteVtxDMAProgram (IMG_PUINT32 pui32Base,
									  IMG_UINT32 ui32VtxSize,
									  IMG_BOOL bViewportIdxPresent,
									  IMG_UINT32 ui32NumVpt,
									  IMG_BOOL bVPGuadBandClipping,
									  IMG_UINT32 ui32SecondaryGBBase,
									  IMG_UINT32 ui32ClipFirstPA,
									  IMG_UINT32 ui32ClipCount,
									  PUSEGEN_INT_REG_STORE_PROG psIntRegProg);


IMG_INTERNAL IMG_PUINT32 USEGenWriteRTATerminateStateProgram (IMG_PUINT32 pui32Base,
												 IMG_UINT32 ui32TAClipWord,
												 IMG_UINT32 ui32RTNumber,
												 IMG_BOOL bFinalRT);

IMG_INTERNAL IMG_PUINT32 USEGenWriteBGDependentLoad(IMG_PUINT32 pui32Base,
									   IMG_UINT32 ui32PrimOffset,
									   IMG_UINT32 ui32SecOffset,
									   USEGEN_BANK_TYPE eResultBank,
									   IMG_UINT32 ui32ResultOffset,
									   IMG_BOOL bRTAPresent,
									   IMG_UINT32 ui32ElementStride,
									   IMG_BOOL bFirstCall,
									   IMG_BOOL bIssueWDF,
									   IMG_PUINT32 pui32Temps);

#if defined (SGX_FEATURE_HYBRID_TWIDDLING)
IMG_PUINT32 USEGenWriteHybridTwiddleAddress(IMG_PUINT32		psUSEInst,
											IMG_UINT32		ui32TileSize,
											IMG_UINT32		ui32ChunkByteSize,
											IMG_INT32		i32Stride,
											IMG_UINT32		ui32SurfaceAddrSAReg);
#else  /* defined (SGX_FEATURE_HYBRID_TWIDDLING) */
IMG_INTERNAL IMG_PUINT32 USEGenWriteTwiddleAddress(IMG_PUINT32	psUSEInst,
						   			  IMG_UINT32	ui32Width,
								 	  IMG_UINT32	ui32Height,
									  IMG_UINT32	ui32SurfaceAddrSAReg,
									  IMG_UINT32	ui32SurfaceStrideSAReg,
									  IMG_UINT32	ui32TTAddr,
									  IMG_BOOL		bWriteSetup);
#endif  /* defined (SGX_FEATURE_HYBRID_TWIDDLING) */

#if defined(USEGEN_SUPPORT_BLENDING)
IMG_BOOL USEGenIntBlendingAvail (PUSEGEN_BLEND_PROG psProg);
IMG_BOOL USEGenF16BlendingAvail (PUSEGEN_BLEND_PROG psProg);

IMG_PUINT32 USEGenWriteBlendingProgram (IMG_PUINT32 pui32Base,
										PUSEGEN_BLEND_PROG psProg,
										IMG_BOOL bWritePhase);
IMG_PUINT32 USEGenWriteMergeFragment (IMG_PUINT32 pui32Base,
									  USEGEN_BANK_TYPE eSrcBank,
									  IMG_UINT32 ui32SrcReg,
									  USEGEN_BANK_TYPE eDestBank,
									  IMG_UINT32 ui32DestReg,
									  IMG_BYTE byMask,
									  IMG_BOOL bWritePhase);
IMG_PUINT32 USEGenWriteFogFragment (IMG_PUINT32 pui32Base,
									PUSEGEN_FOG_PROG psProg,
									IMG_BOOL bWritePhase);

/* EOT stuff */
IMG_PUINT32 USEGenEOTSetupForRTAs(IMG_PUINT32 pui32Base,
								  IMG_PUINT32 pui32TempBase);

IMG_PUINT32 USEGenEOTEmitFragment(IMG_PUINT32 pui32Base,
								  IMG_UINT32 ui32PartitionOffset,
								  IMG_BOOL bRTAPresent,
								  IMG_UINT32 ui32RTAElementStride,
								  IMG_UINT32 ui32TempOffset,
								  IMG_PUINT32 aui32EmitWords,
								  IMG_BOOL bFlushCache,
								  IMG_BOOL bEnd);

IMG_PUINT32 USEGenEOTLoadFragment(IMG_PUINT32 pui32Base,
								  IMG_BOOL bFirstLoad,
								  IMG_BOOL bRTAPresent,
								  IMG_DEV_VIRTADDR sTileBufferAddress,
								  IMG_UINT32 ui32DataSize,
								  IMG_UINT32 ui32PartitionOffset,
								  IMG_BOOL bLastWasDoublePartition,
								  IMG_PUINT32 pui32StaticTempBase,
								  IMG_PUINT32 pui32Temps);

#endif


#endif /* define _usegen_h_ */
