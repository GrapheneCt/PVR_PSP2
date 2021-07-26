/******************************************************************************
 * Name         : fftexhw.c
 *
 * Copyright    : 2006-2008 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited, Home Park
 *              : Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * Description  : Converts fixed-function texture blending into 
 *				  HW shader instructions
 *
 * Platform     : ANSI
 *
 * $Log: fftexhw.c $
 *
 *  --- Revision Logs Removed --- 
 *****************************************************************************/

#include "context.h"


/***********************************************************************************/

/* Register types */
typedef enum 
{
	HW_REGTYPE_TEMP		 = 0,
	HW_REGTYPE_PRIMATTR  = 1,
	HW_REGTYPE_SECATTR	 = 2,
	HW_REGTYPE_OUTPUT	 = 3,
	HW_REGTYPE_SPECIAL = 4

} HWRegType;

/* Source type: RGB or ALPHA */
typedef enum
{
	HW_SRCTYPE_RGB = 0,
	HW_SRCTYPE_A   = 1,

} HWSrcType;

/* Source modifier */
typedef enum 
{
	HW_SRCMOD_NONE		 = 0,
	HW_SRCMOD_COMPLEMENT = 1,

} HWSrcMod;

/* Source negate for IMA/FPMA instruction */
typedef enum 
{
	HW_IMA_SRCNEG_NONE	 = 0,
	HW_IMA_SRCNEG_NEGATE = 1,

} HWIMASrcNeg;

/* Source selects for IMA/FPMA instruction */
typedef enum 
{
	HW_IMA_SRCSEL_RGB	= 0,
	HW_IMA_SRCSEL_A		= 1,

} HWIMASrcSel;

typedef struct
{
	HWRegType	eType;
	IMG_UINT32  ui32Num;
	HWSrcMod	eSrcMod;

	HWIMASrcNeg	eSrcNeg;

	union
	{
		HWIMASrcSel		IMA;
	
	} eSrcSel;

} HWReg;

/* HW temporary register numbers */
#define HW_TEMPREG_MAX					32
#define HW_TEMPREG_INVALID				0xFFFFFFFF

/* HW input register numbers */
#define HW_INPUTREG_COLOUR0				0
#define HW_INPUTREG_TEXTURE0			1
#define HW_INPUTREG_TEXTURE1			2
#define HW_INPUTREG_TEXTURE2			3
#define HW_INPUTREG_TEXTURE3			4
#define HW_INPUTREG_TEXTURE0_PLANE1		5
#define HW_INPUTREG_TEXTURE1_PLANE1		6
#define HW_INPUTREG_TEXTURE2_PLANE1		7
#define HW_INPUTREG_TEXTURE3_PLANE1		8
#define HW_INPUTREG_TEXTURE0_PLANE2		9
#define HW_INPUTREG_TEXTURE1_PLANE2		10
#define HW_INPUTREG_TEXTURE2_PLANE2		11
#define HW_INPUTREG_TEXTURE3_PLANE2		12
#define HW_INPUTREG_COLOUR1				13
#define HW_INPUTREG_MAX					14	/* Number of HW input registers */

/* HW output register numbers */
#define HW_OUTPUTREG_COLOUR				0


/* Bits for sources used, to calculate number of unique sources */
#define SW_SRC_PRIMARY	0x00000001UL
#define	SW_SRC_PREVIOUS	0x00000002UL
#define	SW_SRC_CONSTANT	0x00000004UL
#define	SW_SRC_TEXTURE0	0x00000008UL
#define	SW_SRC_TEXTURE1	0x00000010UL
#define	SW_SRC_TEXTURE2	0x00000020UL
#define	SW_SRC_TEXTURE3	0x00000040UL
#define	SW_SRC_TEXTURE4	0x00000080UL
#define	SW_SRC_TEXTURE5	0x00000100UL
#define	SW_SRC_TEXTURE6	0x00000200UL
#define	SW_SRC_TEXTURE7	0x0000040UL



/* Structure for describing the sources used in a blend layer */
typedef struct 
{
	/* RGB sources */
	IMG_UINT32		ui32NumColorSources;
	IMG_UINT32		ui32NumUniqueColorSources;
	IMG_UINT32		ui32ColorSourcesUsed;
	
	/* Alpha sources */
	IMG_UINT32		ui32NumAlphaSources;
	IMG_UINT32		ui32NumUniqueAlphaSources;
	IMG_UINT32		ui32AlphaSourcesUsed;

	/* Combined source info */
	IMG_UINT32		ui32SourcesUsed;
	IMG_UINT32		ui32NumUniqueSources;

	IMG_BOOL		bColorUsesComplement;
	IMG_BOOL		bAlphaUsesComplement;
	IMG_BOOL		bColorUsesAlpha;
	IMG_BOOL		bTwoSubtractsDifferentSourceOrder;

} SWSourceInfo;

/*
typedef enum
{
	SOP2_SRCTYPE_PRIMARY	= 0,
	SOP2_SRCTYPE_PREVIOUS	= 1,
	SOP2_SRCTYPE_CONSTANT	= 2,
	SOP2_SRCTYPE_TEXTURE0	= 3,
	SOP2_SRCTYPE_TEXTURE1	= 4,
	SOP2_SRCTYPE_TEXTURE2	= 5,
	SOP2_SRCTYPE_TEXTURE3	= 6,
	SOP2_SRCTYPE_TEXTURE4	= 7,
	SOP2_SRCTYPE_TEXTURE5	= 8,
	SOP2_SRCTYPE_TEXTURE6	= 9,
	SOP2_SRCTYPE_TEXTURE7	= 10,
	
} SOP2SourceType;
*/

#define SOP2_SRCTYPE_PRIMARY		0
#define SOP2_SRCTYPE_PREVIOUS		1
#define SOP2_SRCTYPE_CONSTANT		2
#define SOP2_SRCTYPE_TEXTURE0		3
#define SOP2_SRCTYPE_TEXTURE1		4
#define SOP2_SRCTYPE_TEXTURE2		5
#define SOP2_SRCTYPE_TEXTURE3		6
#define SOP2_SRCTYPE_TEXTURE4		7
#define SOP2_SRCTYPE_TEXTURE5		8
#define SOP2_SRCTYPE_TEXTURE6		9
#define SOP2_SRCTYPE_TEXTURE7		10

#define SOP2_SRCTYPE_COUNT (SOP2_SRCTYPE_TEXTURE7 + 1)

/* Keep these values such that Src1 = 1, Src2 = 2 */
/*
typedef enum
{
	SOP2_SRC_NONE			= 0,
	SOP2_SRC1				= 1,
	SOP2_SRC2				= 2,

} SOP2SourceAssignments;
*/

/* Keep these values such that Src1 = 1, Src2 = 2 */
#define SOP2_SRC_NONE		0
#define SOP2_SRC1			1
#define SOP2_SRC2			2

/* A structure used to keep track of the contents of a SOP2 source 
   (i.e. Src1 == TEXTURE0) */
typedef struct 
{
	IMG_UINT32				eCurrentSrc;							/* The current source (SOP2_SRC1 or SOP2_SRC2) */
	IMG_UINT32				aeSrcAssignments[SOP2_SRCTYPE_COUNT];	/* Should be indexed by SOP2SourceTypes */

} SOP2Sources;

/* A collection of useful information needed when
   creating a program. */
typedef struct
{
	/* For allocating temporary registers */
	IMG_UINT32				ui32TempRegsUsed;		/* Bit-field */
	IMG_UINT32				ui32NumTempRegsUsed;

	/* Allocated register numbers for important data */
	IMG_UINT32				ui32CurrentRGBARegNum;
	IMG_UINT32				ui32IntermediateRGBRegNum;
	IMG_UINT32				ui32IntermediateAlphaRegNum;
	IMG_UINT32				ui32FPMASpecialConstantRegNum;

	/* Input registers */
	IMG_UINT32				aui32InputRegMappings[HW_INPUTREG_MAX];

	/* Input registers moved into temps */
	IMG_UINT32				aui32InputsPlacedInTemps[HW_INPUTREG_MAX];


	/* State needed to compile program */
	IMG_UINT32				ui32ExtraBlendState;
	FFTBProgramDesc			*psFFTBProgramDesc;

	/* USEASM instructions for fragment shader */
	GLESUSEASMInfo			sUSEASMInfo;

} ProgramContext;



/***********************************************************************************/

/* Map SOP2SourceAssignments to source numbers suitable for SetupHWSource() */
static const IMG_UINT32 aui32RemapSOP2SourceAssignmentToSrcNum[] = {0, 1, 2};

/* Array to be indexed by __GLSGXTexCombineOp, to remap sources 
   into the correct order needed by FPMA instruction.

   For example, FPMA does Src0 + Src1 * Src2, so a two-source MUL needs 
   its arguments in Src1 and Src2 */
static const IMG_UINT32 aui32FPMASrcRemap[10][3] = {{0, 1, 2}, /* GLES1_COMBINEOP_REPLACE */
												 {1, 2, 0}, /* GLES1_COMBINEOP_MODULATE */
												 {0, 1, 2}, /* GLES1_COMBINEOP_ADD */
												 {0, 1, 2}, /* GLES1_COMBINEOP_ADDSIGNED */
												 {0, 1, 2}, /* GLES1_COMBINEOP_INTERPOLATE */
												 {1, 0, 2}, /* GLES1_COMBINEOP_SUBTRACT */
												 {0, 1, 2}, /* GLES1_COMBINEOP_ARB_DOT3_RGB */
												 {0, 1, 2}, /* GLES1_COMBINEOP_ARB_DOT3_RGBA */
												 {0, 1, 2}, /* GLES1_COMBINEOP_EXT_DOT3_RGB */
												 {0, 1, 2}};/* GLES1_COMBINEOP_EXT_DOT3_RGBA */

/*****************************************************************************/

static const UseasmRegType aeRegTypeRemap[] = {USEASM_REGTYPE_TEMP, 
									  		   USEASM_REGTYPE_PRIMATTR,
									  		   USEASM_REGTYPE_SECATTR,
									  		   USEASM_REGTYPE_OUTPUT,
									  		   USEASM_REGTYPE_MAXIMUM};


/*****************************************************************************/

/* Function prototypes */

static IMG_VOID IntegerCopy(GLES1Context *gc,
						ProgramContext	*psProgramContext,
						HWRegType	 eDestType,
						IMG_UINT32	 ui32DestNum,
						HWRegType	 eSrcType,
						IMG_UINT32	 ui32SrcNum);

static IMG_VOID IntegerADDSIGNEDSOP2(GLES1Context *gc,
									HWReg			*psDest,
									 HWReg			*psSources,
									 ProgramContext	*psProgramContext);

static IMG_VOID EncodeFPMAReplace(GLES1Context *gc,
						HWReg				*psDest,
							  HWReg				*psSource,
							  HWSrcType			 eSrcType,
							  ProgramContext	*psProgramContext);

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)

static IMG_VOID InsertPHAS(GLES1Context *gc, ProgramContext	*psProgramContext)
{
	USE_REGISTER asArg[5];

#if defined(FIX_HW_BRN_29019)
	if(gc->sPrim.sRenderState.ui32AlphaTestFlags & GLES1_ALPHA_TEST_FEEDBACK)
	{
		/* Dst */
		SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_PRIMATTR, 0);

		/* Src */
		SETUP_INSTRUCTION_ARG(1, 0, USEASM_REGTYPE_PRIMATTR, 0);

		/* Add instruction */
		AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_MOV, 0, 0, 0, asArg, 2);
	}
#endif /* defined(FIX_HW_BRN_29019) */

	/* Arg[0] : the execution address of the next phase */
	SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_IMMEDIATE, 0);

	/* Arg[1] : the number of temporary registers to allocate for the next phase */
	SETUP_INSTRUCTION_ARG(1, 0, USEASM_REGTYPE_IMMEDIATE, 0);

	/* Arg[2]: the execution rate for the next phase */
	SETUP_INSTRUCTION_ARG(2, USEASM_INTSRCSEL_PIXEL, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Arg[3]: the wait condition for the start of the next phase */
	SETUP_INSTRUCTION_ARG(3, USEASM_INTSRCSEL_END, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Arg[4]: the execution mode for the next phase */
	SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_PARALLEL, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Add instruction */
	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_PHASIMM, 0, 0, 0, asArg, 5);
}

#endif


/*****************************************************************************
 Function Name      : AllocateTempReg
 Description        : Allocates a temporary register
******************************************************************************/

static IMG_UINT32 AllocateTempReg(ProgramContext *psProgramContext)
{
	IMG_UINT32 i = 0;
	IMG_UINT32 ui32AllocatedRegNum;

	/* Search for a free temporary register, indicated
	   by a bit NOT being set in ui32TempRegsUsed */
	while(psProgramContext->ui32TempRegsUsed & (1UL << i))
	{
		i++;

		if(i == HW_TEMPREG_MAX)
		{
			break;
		}
	}

	if(i == HW_TEMPREG_MAX)
	{
		PVR_DPF((PVR_DBG_ERROR,"AllocateTempReg(): failed to allocate register - defaulting to Temp0"));
		ui32AllocatedRegNum = 0;
	}
	else
	{
		/* Set the bit to mark this register as allocated */
		psProgramContext->ui32TempRegsUsed |= (1UL << i);
		ui32AllocatedRegNum = i;

		if(psProgramContext->ui32NumTempRegsUsed < (i + 1))
		{
			psProgramContext->ui32NumTempRegsUsed = i + 1;
		}
	}

	return ui32AllocatedRegNum;
}

/*****************************************************************************
 Function Name      : DeallocateTempReg
 Description        : Deallocates a temporary register
******************************************************************************/
static IMG_VOID DeallocateTempReg(ProgramContext	*psProgramContext,
								  IMG_UINT32		ui32RegNum)
{
	if(ui32RegNum == HW_TEMPREG_INVALID)
	{
		return;
	}

	if(ui32RegNum > HW_TEMPREG_MAX)
	{
		PVR_DPF((PVR_DBG_ERROR,"DeallocateTempReg(): invalid register number: %d", ui32RegNum));
		return;
	}

	if((psProgramContext->ui32TempRegsUsed & (1UL << ui32RegNum)) == 0)
	{
		PVR_DPF((PVR_DBG_ERROR,"DeallocateTempReg(): register number %d not allocated", ui32RegNum));
		return;
	}
	
	psProgramContext->ui32TempRegsUsed &= ~(1UL << ui32RegNum);
}


/***********************************************************************************
 Function Name      : SetupColourSourceRegTypeNum
 Description        : 
************************************************************************************/
static void SetupColourSourceRegTypeNum(IMG_UINT32	 ui32ColorSrcs,
										IMG_UINT32				 ui32SrcNum,
										IMG_UINT32				 ui32SrcLayerNum, 
										IMG_UINT32				 ui32EnabledLayerNum,
										IMG_UINT32				 uCurrentRGBRegister,
										ProgramContext			*psProgramContext,
										HWReg					*psSrc)
{
	switch(GLES1_COMBINE_GET_COLSRC(ui32ColorSrcs, ui32SrcNum))
	{
		case GLES1_COMBINECSRC_PRIMARY:
		{
			if(psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR0]!=HW_TEMPREG_INVALID)
			{
				psSrc->eType = HW_REGTYPE_TEMP;
				psSrc->ui32Num = psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR0];
			}
			else
			{
				psSrc->eType = HW_REGTYPE_PRIMATTR;
				psSrc->ui32Num = psProgramContext->aui32InputRegMappings[HW_INPUTREG_COLOUR0];
			}

			break;
		}
		case GLES1_COMBINECSRC_PREVIOUS:
		{
			/* On texture layer 0, PREVIOUS becomes PRIMARY */
			if(ui32EnabledLayerNum == 0)
			{
				if(psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR0]!=HW_TEMPREG_INVALID)
				{
					psSrc->eType = HW_REGTYPE_TEMP;
					psSrc->ui32Num = psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR0];
				}
				else
				{
					psSrc->eType = HW_REGTYPE_PRIMATTR;
					psSrc->ui32Num = psProgramContext->aui32InputRegMappings[HW_INPUTREG_COLOUR0];
				}
			}
			else
			{
				psSrc->eType = HW_REGTYPE_TEMP;
				psSrc->ui32Num = uCurrentRGBRegister;
			}

			break;
		}
		case GLES1_COMBINECSRC_TEXTURE:
		{
			IMG_UINT32 uTexUnit;

#if defined(GLES1_EXTENSION_TEX_ENV_CROSSBAR)
			/* Is a unit number encoded in the source? (for crossbar) */
			if(GLES1_COMBINE_GET_COLCROSSBAR(ui32ColorSrcs, ui32SrcNum) & GLES1_COMBINECSRC_CROSSBAR)
			{
				uTexUnit = (IMG_UINT32) GLES1_COMBINE_GET_COLCROSSBAR(ui32ColorSrcs, ui32SrcNum) >> GLES1_COMBINECSRC_CROSSBAR_UNIT_SHIFT;
			}
			else
#endif /* defined(GLES1_EXTENSION_TEX_ENV_CROSSBAR) */
			{
				uTexUnit = ui32SrcLayerNum;
			}
			
			if(psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_TEXTURE0 + uTexUnit]!=HW_TEMPREG_INVALID)
			{
				psSrc->eType = HW_REGTYPE_TEMP;
				psSrc->ui32Num = psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_TEXTURE0 + uTexUnit];
			}
			else
			{
				psSrc->eType = HW_REGTYPE_PRIMATTR;
				psSrc->ui32Num = psProgramContext->aui32InputRegMappings[HW_INPUTREG_TEXTURE0 + uTexUnit];
			}

			break;
		}
		case GLES1_COMBINECSRC_CONSTANT:
		{
			IMG_UINT32 uConstOffset;

			uConstOffset = AddFFTextureBinding(psProgramContext->psFFTBProgramDesc,
											   FFTB_BINDING_FACTOR_SCALAR,
											   ui32SrcLayerNum);

			psSrc->eType = HW_REGTYPE_SECATTR;
			psSrc->ui32Num = uConstOffset;

			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR,"SetupColourSourceRegTypeNum: Invalid colour source"));
			break;
		}
	}
}

/***********************************************************************************
 Function Name      : SetupAlphaSourceRegTypeNum
 Description        : 
************************************************************************************/
static void SetupAlphaSourceRegTypeNum(IMG_UINT32				ui32AlphaSrcs,
									   IMG_UINT32				ui32SrcNum,
									   IMG_UINT32				ui32SrcLayerNum, 
									   IMG_UINT32				ui32EnabledLayerNum,
									   IMG_UINT32				ui32CurrentARegister,
									   ProgramContext			*psProgramContext,
									   HWReg					*psSrc)
{
	switch(GLES1_COMBINE_GET_ALPHASRC(ui32AlphaSrcs, ui32SrcNum))
	{
		case GLES1_COMBINEASRC_PRIMARY:
		{
			if(psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR0]!=HW_TEMPREG_INVALID)
			{
				psSrc->eType = HW_REGTYPE_TEMP;
				psSrc->ui32Num = psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR0];
			}
			else
			{
				psSrc->eType = HW_REGTYPE_PRIMATTR;
				psSrc->ui32Num = psProgramContext->aui32InputRegMappings[HW_INPUTREG_COLOUR0];
			}

			break;
		}
		case GLES1_COMBINEASRC_PREVIOUS:
		{
			if(ui32EnabledLayerNum == 0)
			{
				if(psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR0]!=HW_TEMPREG_INVALID)
				{
					psSrc->eType = HW_REGTYPE_TEMP;
					psSrc->ui32Num = psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR0];
				}
				else
				{
					psSrc->eType = HW_REGTYPE_PRIMATTR;
					psSrc->ui32Num = psProgramContext->aui32InputRegMappings[HW_INPUTREG_COLOUR0];
				}
			}
			else
			{
				psSrc->eType = HW_REGTYPE_TEMP;
				psSrc->ui32Num = ui32CurrentARegister;
			}

			break;
		}
		case GLES1_COMBINEASRC_TEXTURE:
		{
			IMG_UINT32 uTexUnit;

#if defined(GLES1_EXTENSION_TEX_ENV_CROSSBAR)
			/* Is a unit number encoded in the source? (for crossbar) */
			if(GLES1_COMBINE_GET_ALPHACROSSBAR(ui32AlphaSrcs, ui32SrcNum) & GLES1_COMBINEASRC_CROSSBAR)
			{
				uTexUnit = (IMG_UINT32)GLES1_COMBINE_GET_ALPHACROSSBAR(ui32AlphaSrcs, ui32SrcNum) >> GLES1_COMBINEASRC_CROSSBAR_UNIT_SHIFT;
			}
			else
#endif /* defined(GLES1_EXTENSION_TEX_ENV_CROSSBAR) */
			{
				uTexUnit = ui32SrcLayerNum;
			}

			if(psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_TEXTURE0 + uTexUnit]!=HW_TEMPREG_INVALID)
			{
				psSrc->eType = HW_REGTYPE_TEMP;
				psSrc->ui32Num = psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_TEXTURE0 + uTexUnit];
			}
			else
			{
				psSrc->eType = HW_REGTYPE_PRIMATTR;
				psSrc->ui32Num = psProgramContext->aui32InputRegMappings[HW_INPUTREG_TEXTURE0 + uTexUnit];
			}
			
			break;
		}
		case GLES1_COMBINEASRC_CONSTANT:
		{
			IMG_UINT32 uConstOffset;

			uConstOffset = AddFFTextureBinding(psProgramContext->psFFTBProgramDesc,
											   FFTB_BINDING_FACTOR_SCALAR,
											   ui32SrcLayerNum);

			psSrc->eType = HW_REGTYPE_SECATTR;
			psSrc->ui32Num = uConstOffset;

			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR,"SetupAlphaSourceRegTypeNum: Invalid alpha source"));
			break;
		}
	}
}

/***********************************************************************************
 Function Name      : EncodeDOT3
 Description        : 
************************************************************************************/
static IMG_VOID EncodeDOT3(GLES1Context *gc,
						   HWReg		  *psDest,
						   HWReg		  *psSources,
						   IMG_BOOL		   bReplicateToAlpha,
						   ProgramContext *psProgramContext)
{
	IMG_UINT32 ui32TempRegNum[2] = {HW_TEMPREG_INVALID, HW_TEMPREG_INVALID};
	USE_REGISTER asArg[5];
	IMG_UINT32 i;

	PVR_UNREFERENCED_PARAMETER(bReplicateToAlpha);

	for(i=0; i<2; i++)
	{
		/* DOT3 cannot access alpha component, so move into a temporary
		   register */
		if(psSources[i].eSrcSel.IMA == HW_IMA_SRCSEL_A)
		{
			HWReg sDest;

			ui32TempRegNum[i] = AllocateTempReg(psProgramContext);

			sDest.eType = HW_REGTYPE_TEMP;
			sDest.ui32Num = ui32TempRegNum[i];

			EncodeFPMAReplace(gc,
							  &sDest,
							  &psSources[i],
							  HW_SRCTYPE_RGB,
							  psProgramContext);

			/* Update source to refer to result of above instruction,
			   with no source select or modifiers */
			psSources[i].eType = HW_REGTYPE_TEMP;
			psSources[i].ui32Num = ui32TempRegNum[i];
			psSources[i].eSrcMod = HW_SRCMOD_NONE;
			psSources[i].eSrcSel.IMA = HW_IMA_SRCSEL_RGB;
		}
	}

	/* Dst */
	SETUP_INSTRUCTION_ARG(0, psDest->ui32Num, aeRegTypeRemap[psDest->eType], 0);

	/*  */
	SETUP_INSTRUCTION_ARG(1, USEASM_INTSRCSEL_CX4, USEASM_REGTYPE_INTSRCSEL, 0);

	/*  */
	SETUP_INSTRUCTION_ARG(2, USEASM_INTSRCSEL_AX4, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Src1 */
	if(psSources[0].eSrcMod==HW_SRCMOD_COMPLEMENT)
	{
		SETUP_INSTRUCTION_ARG(3, psSources[0].ui32Num, aeRegTypeRemap[psSources[0].eType], USEASM_ARGFLAGS_COMPLEMENT);
	}
	else
	{
		SETUP_INSTRUCTION_ARG(3, psSources[0].ui32Num, aeRegTypeRemap[psSources[0].eType], 0);
	}

	/* Src2 */
	if(psSources[1].eSrcMod==HW_SRCMOD_COMPLEMENT)
	{
		SETUP_INSTRUCTION_ARG(4, psSources[1].ui32Num, aeRegTypeRemap[psSources[1].eType], USEASM_ARGFLAGS_COMPLEMENT);
	}
	else
	{
		SETUP_INSTRUCTION_ARG(4, psSources[1].ui32Num, aeRegTypeRemap[psSources[1].eType], 0);
	}

	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_U8DOT3OFF, 0, 0, 0, asArg, 5);


	/* Deallocate registers if required */
	DeallocateTempReg(psProgramContext, ui32TempRegNum[0]);
	DeallocateTempReg(psProgramContext, ui32TempRegNum[1]);

}

/***********************************************************************************
 Function Name      : LoadFPMASpecialConstant
 Description        : Loads special constant (0x00FFFFFF) used to perform basic
					  arithmetic operations using the FPMA instruction
************************************************************************************/
static IMG_VOID LoadFPMASpecialConstant(GLES1Context *gc, ProgramContext *psProgramContext)
{
	/* Load special constant, if needed */
	if(psProgramContext->ui32FPMASpecialConstantRegNum == HW_TEMPREG_INVALID)
	{
		IMG_UINT32 uConstOffset;

		/* Add binding for secondary attribute immediate value */
		uConstOffset = AddFFTextureBinding(psProgramContext->psFFTBProgramDesc,
										   FFTB_BINDING_IMMEDIATE_SCALAR,
										   0x00FFFFFF);

		/* Allocate a temporary register to store the special constant */
		psProgramContext->ui32FPMASpecialConstantRegNum = AllocateTempReg(psProgramContext);

		/* Copy 0x00FFFFFF from a secondary attribute
		   to a temporary, needed for use in FPMA instructions */
		IntegerCopy(gc,
					psProgramContext,
					HW_REGTYPE_TEMP,
					psProgramContext->ui32FPMASpecialConstantRegNum,
					HW_REGTYPE_SECATTR,
					uConstOffset);
	}
}

/***********************************************************************************
 Function Name      : EncodeFPMA
 Description        : Encodes an FPMA instruction using the supplied 
					  sources and destination
************************************************************************************/
static IMG_VOID EncodeFPMA(GLES1Context *gc,
						   HWReg		   *psDest,
						   HWReg		   *psSources,
						   HWSrcType		eSrcType,
						   ProgramContext  *psProgramContext)
{
	USE_REGISTER asArg[10];

	/* Dst */
	SETUP_INSTRUCTION_ARG(0, psDest->ui32Num, aeRegTypeRemap[psDest->eType], 0);

	/* Src 0 */
	if(psSources[0].eSrcNeg == HW_IMA_SRCNEG_NEGATE)
	{
		SETUP_INSTRUCTION_ARG(1, psSources[0].ui32Num, aeRegTypeRemap[psSources[0].eType], USEASM_ARGFLAGS_NEGATE);
	}
	else
	{
		SETUP_INSTRUCTION_ARG(1, psSources[0].ui32Num, aeRegTypeRemap[psSources[0].eType], 0);
	}

	/* Src 1 */
	SETUP_INSTRUCTION_ARG(2, psSources[1].ui32Num, aeRegTypeRemap[psSources[1].eType], 0);

	/* Src 2 */
	SETUP_INSTRUCTION_ARG(3, psSources[2].ui32Num, aeRegTypeRemap[psSources[2].eType], 0);

	/* Colour modifier 0 */
	if(eSrcType==HW_SRCTYPE_RGB && psSources[0].eSrcSel.IMA==HW_IMA_SRCSEL_A)
	{
		if(psSources[0].eSrcMod==HW_SRCMOD_COMPLEMENT)
		{
			SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_SRC0ALPHA, USEASM_REGTYPE_INTSRCSEL, USEASM_ARGFLAGS_COMPLEMENT);
		}
		else
		{
			SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_SRC0ALPHA, USEASM_REGTYPE_INTSRCSEL, 0);
		}
	}
	else
	{
		if(psSources[0].eSrcMod==HW_SRCMOD_COMPLEMENT)
		{
			SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_SRC0, USEASM_REGTYPE_INTSRCSEL, USEASM_ARGFLAGS_COMPLEMENT);
		}
		else
		{
			SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_SRC0, USEASM_REGTYPE_INTSRCSEL, 0);
		}
	}

	/* Colour modifier 1 */
	if(eSrcType==HW_SRCTYPE_RGB && psSources[1].eSrcSel.IMA==HW_IMA_SRCSEL_A)
	{
		if(psSources[1].eSrcMod==HW_SRCMOD_COMPLEMENT)
		{
			SETUP_INSTRUCTION_ARG(5, USEASM_INTSRCSEL_SRC1ALPHA, USEASM_REGTYPE_INTSRCSEL, USEASM_ARGFLAGS_COMPLEMENT);
		}
		else
		{
			SETUP_INSTRUCTION_ARG(5, USEASM_INTSRCSEL_SRC1ALPHA, USEASM_REGTYPE_INTSRCSEL, 0);
		}
	}
	else
	{
		if(psSources[1].eSrcMod==HW_SRCMOD_COMPLEMENT)
		{
			SETUP_INSTRUCTION_ARG(5, USEASM_INTSRCSEL_SRC1, USEASM_REGTYPE_INTSRCSEL, USEASM_ARGFLAGS_COMPLEMENT);
		}
		else
		{
			SETUP_INSTRUCTION_ARG(5, USEASM_INTSRCSEL_SRC1, USEASM_REGTYPE_INTSRCSEL, 0);
		}
	}

	/* Colour modifier 2 */
	if(eSrcType==HW_SRCTYPE_RGB && psSources[2].eSrcSel.IMA==HW_IMA_SRCSEL_A)
	{
		if(psSources[2].eSrcMod==HW_SRCMOD_COMPLEMENT)
		{
			SETUP_INSTRUCTION_ARG(6, USEASM_INTSRCSEL_SRC2ALPHA, USEASM_REGTYPE_INTSRCSEL, USEASM_ARGFLAGS_COMPLEMENT);
		}
		else
		{
			SETUP_INSTRUCTION_ARG(6, USEASM_INTSRCSEL_SRC2ALPHA, USEASM_REGTYPE_INTSRCSEL, 0);
		}
	}
	else
	{
		if(psSources[2].eSrcMod==HW_SRCMOD_COMPLEMENT)
		{
			SETUP_INSTRUCTION_ARG(6, USEASM_INTSRCSEL_SRC2, USEASM_REGTYPE_INTSRCSEL, USEASM_ARGFLAGS_COMPLEMENT);
		}
		else
		{
			SETUP_INSTRUCTION_ARG(6, USEASM_INTSRCSEL_SRC2, USEASM_REGTYPE_INTSRCSEL, 0);
		}
	}

	/* Alpha modifier 0 */
	if(psSources[0].eSrcMod==HW_SRCMOD_COMPLEMENT)
	{
		SETUP_INSTRUCTION_ARG(7, USEASM_INTSRCSEL_SRC0ALPHA, USEASM_REGTYPE_INTSRCSEL, USEASM_ARGFLAGS_COMPLEMENT);
	}
	else
	{
		SETUP_INSTRUCTION_ARG(7, USEASM_INTSRCSEL_SRC0ALPHA, USEASM_REGTYPE_INTSRCSEL, 0);
	}

	/* Alpha modifier 1 */
	if(psSources[1].eSrcMod==HW_SRCMOD_COMPLEMENT)
	{
		SETUP_INSTRUCTION_ARG(8, USEASM_INTSRCSEL_SRC1ALPHA, USEASM_REGTYPE_INTSRCSEL, USEASM_ARGFLAGS_COMPLEMENT);
	}
	else
	{
		SETUP_INSTRUCTION_ARG(8, USEASM_INTSRCSEL_SRC1ALPHA, USEASM_REGTYPE_INTSRCSEL, 0);
	}

	/* Alpha modifier 2 */
	if(psSources[2].eSrcMod==HW_SRCMOD_COMPLEMENT)
	{
		SETUP_INSTRUCTION_ARG(9, USEASM_INTSRCSEL_SRC2ALPHA, USEASM_REGTYPE_INTSRCSEL, USEASM_ARGFLAGS_COMPLEMENT);
	}
	else
	{
		SETUP_INSTRUCTION_ARG(9, USEASM_INTSRCSEL_SRC2ALPHA, USEASM_REGTYPE_INTSRCSEL, 0);
	}

#if 0
	/* 
	   Force generation of exactly the same instructions as the old fftexhw code.
	   Useful for debugging...
	*/
	if(eSrcType==HW_SRCTYPE_RGB)
	{
		SETUP_INSTRUCTION_ARG(7, USEASM_INTSRCSEL_SRC0ALPHA, USEASM_REGTYPE_INTSRCSEL, 0);
		SETUP_INSTRUCTION_ARG(8, USEASM_INTSRCSEL_SRC1ALPHA, USEASM_REGTYPE_INTSRCSEL, 0);
		SETUP_INSTRUCTION_ARG(9, USEASM_INTSRCSEL_SRC2ALPHA, USEASM_REGTYPE_INTSRCSEL, 0);
	}
	else
	{
		SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_SRC0, USEASM_REGTYPE_INTSRCSEL, 0);
		SETUP_INSTRUCTION_ARG(5, USEASM_INTSRCSEL_SRC1, USEASM_REGTYPE_INTSRCSEL, 0);
		SETUP_INSTRUCTION_ARG(6, USEASM_INTSRCSEL_SRC2, USEASM_REGTYPE_INTSRCSEL, 0);
	}
#endif

	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_FPMA, 0, USEASM_OPFLAGS2_SAT, 0, asArg, 10);
}

/***********************************************************************************
 Function Name      : EncodeFPMAReplace
 Description        : Helper function to encode a REPLACE operation using
					  FPMA
************************************************************************************/
static IMG_VOID EncodeFPMAReplace(GLES1Context *gc,
								  HWReg				*psDest,
								  HWReg				*psSource,
								  HWSrcType			 eSrcType,
								  ProgramContext	*psProgramContext)
{
	HWReg asSources[3];

	/* Load special constant, if needed */
	LoadFPMASpecialConstant(gc, psProgramContext);

	/* FPMA does Src0 + Src1 * Src2.
	   For replace we do 0 + Src1 * 1.
	   By not using Src0, we can use a secondary attribute in Src1 */

	if(eSrcType == HW_SRCTYPE_RGB)
	{
		asSources[0].eType = HW_REGTYPE_TEMP;
		asSources[0].ui32Num = psProgramContext->ui32FPMASpecialConstantRegNum,
		asSources[0].eSrcMod = HW_SRCMOD_COMPLEMENT;
		asSources[0].eSrcNeg = HW_IMA_SRCNEG_NONE;
		asSources[0].eSrcSel.IMA = HW_IMA_SRCSEL_RGB;

		asSources[1] = *psSource;

		asSources[2].eType = HW_REGTYPE_TEMP;
		asSources[2].ui32Num = psProgramContext->ui32FPMASpecialConstantRegNum;
		asSources[2].eSrcMod = HW_SRCMOD_NONE;
		asSources[2].eSrcNeg = HW_IMA_SRCNEG_NONE;
		asSources[2].eSrcSel.IMA = HW_IMA_SRCSEL_RGB;
	}
	else
	{
		asSources[0].eType = HW_REGTYPE_TEMP;
		asSources[0].ui32Num = psProgramContext->ui32FPMASpecialConstantRegNum,
		asSources[0].eSrcMod = HW_SRCMOD_NONE;
		asSources[0].eSrcNeg = HW_IMA_SRCNEG_NONE;
		asSources[0].eSrcSel.IMA = HW_IMA_SRCSEL_A;

		asSources[1] = *psSource;

		asSources[2].eType = HW_REGTYPE_TEMP;
		asSources[2].ui32Num = psProgramContext->ui32FPMASpecialConstantRegNum;
		asSources[2].eSrcMod = HW_SRCMOD_COMPLEMENT;
		asSources[2].eSrcNeg = HW_IMA_SRCNEG_NONE;
		asSources[2].eSrcSel.IMA = HW_IMA_SRCSEL_A;
	}

	EncodeFPMA(gc,
			   psDest,
			   asSources,
			   eSrcType,
			   psProgramContext);
}

/***********************************************************************************
 Function Name      : EncodeGeneralColorBlend
 Description        : Encodes a layer of texture blending using the fixed-point
					  multiply-add instruction (FPMA) for the colour channel
************************************************************************************/
static IMG_VOID EncodeGeneralColorBlend(GLES1Context *gc,
										IMG_UINT32			 ui32SrcLayerNum,
										IMG_UINT32			 ui32EnabledLayerNum,
										FFTBBlendLayerDesc	*psBlendLayer,
										ProgramContext		*psProgramContext)
{
	IMG_UINT32 i;
	/* Variables to store the layer info in a convenient form */
	IMG_UINT32 ui32ColOp;
	IMG_UINT32 ui32NumColSrcs;
	IMG_UINT32 ui32TempRegNum = HW_TEMPREG_INVALID;
	HWReg sColDest;
	HWReg asColSrc[3], *psSrc;

	IMG_BOOL bDOT3ReplicateToAlpha = IMG_FALSE;
	IMG_BOOL bDOT3Blend = IMG_FALSE;

	/* Setup colour destination register */
	sColDest.ui32Num = psProgramContext->ui32IntermediateRGBRegNum;
	sColDest.eType = HW_REGTYPE_TEMP;
	sColDest.eSrcMod = HW_SRCMOD_NONE;
	sColDest.eSrcNeg = HW_IMA_SRCNEG_NONE;
	sColDest.eSrcSel.IMA = HW_IMA_SRCSEL_RGB;

	ui32ColOp = GLES1_COMBINE_GET_COLOROP(psBlendLayer->ui32Op);

	switch (ui32ColOp)
	{
		case GLES1_COMBINEOP_REPLACE:
		{
			ui32NumColSrcs = 1;

			break;
		}
		case GLES1_COMBINEOP_MODULATE:
		case GLES1_COMBINEOP_ADD:
		case GLES1_COMBINEOP_SUBTRACT:
		{
			ui32NumColSrcs	= 2;

			break;
		}
		case GLES1_COMBINEOP_DOT3_RGB:
		case GLES1_COMBINEOP_DOT3_RGBA:
		{
			ui32NumColSrcs	= 2;
			bDOT3Blend = IMG_TRUE;

			break;
		}
		case GLES1_COMBINEOP_INTERPOLATE:
		{
			ui32NumColSrcs	= 3;

			break;
		}
		case GLES1_COMBINEOP_ADDSIGNED:
		default:
		{
			PVR_DPF((PVR_DBG_ERROR,"EncodeGeneralColorBlend(): unsupported mode, shouldn't happen! Defaulting to REPLACE"));

			ui32ColOp = GLES1_COMBINEOP_REPLACE;

			ui32NumColSrcs = 1;
		}
	}

	if(!bDOT3Blend)
	{
		/* Load special constant, if needed */
		LoadFPMASpecialConstant(gc, psProgramContext);
	}

	for(i=0; i<ui32NumColSrcs; i++)
	{
		psSrc = &asColSrc[aui32FPMASrcRemap[ui32ColOp][i]];

		/* No source negation */
		psSrc->eSrcNeg = HW_IMA_SRCNEG_NONE;

		/* Set up the source register type and number */
		SetupColourSourceRegTypeNum(psBlendLayer->ui32ColorSrcs,
									i,
									ui32SrcLayerNum,
									ui32EnabledLayerNum,
									psProgramContext->ui32CurrentRGBARegNum,
									psProgramContext,
									psSrc);

		/* Secondary attributes cannot be accessed in 
		   src0 by FPMA, so move into a temp */
		if(!bDOT3Blend &&
		   (aui32FPMASrcRemap[ui32ColOp][i] == 0) &&
		   (psSrc->eType == HW_REGTYPE_SECATTR))
		{
			HWReg sDest;

			ui32TempRegNum = AllocateTempReg(psProgramContext);
		
			sDest.eType = HW_REGTYPE_TEMP;
			sDest.ui32Num = ui32TempRegNum;

			IntegerCopy(gc,
						psProgramContext, 
						HW_REGTYPE_TEMP,
						ui32TempRegNum,
						psSrc->eType,
						psSrc->ui32Num);

			psSrc->eType = sDest.eType;
			psSrc->ui32Num = sDest.ui32Num;
		}

		/* LSB indicates that source should be complemented (1-Src) */
		if(GLES1_COMBINE_GET_COLOPERAND(psBlendLayer->ui32ColorSrcs, i) & GLES1_COMBINECSRC_OPERANDONEMINUS)
		{
			psSrc->eSrcMod = HW_SRCMOD_COMPLEMENT;
		}
		else
		{
			psSrc->eSrcMod = HW_SRCMOD_NONE;
		}

		/* Alpha swizzle */
		if(GLES1_COMBINE_GET_COLOPERAND(psBlendLayer->ui32ColorSrcs, i) & GLES1_COMBINECSRC_OPERANDALPHA)
		{
			/* Source select alpha */
			psSrc->eSrcSel.IMA = HW_IMA_SRCSEL_A;
		}
		else
		{
			/* Source select RGB */
			psSrc->eSrcSel.IMA = HW_IMA_SRCSEL_RGB;
		}
	}

	/*
		Instruction generation
		----------------------
	*/

	/* Colour blend */
	switch (ui32ColOp)
	{
		/* REPLACE				= S0 */
		case GLES1_COMBINEOP_REPLACE:
		{
			/* FPMA does Src0 + Src1 * Src2, so for replace
			   we need Src0 + 0 * 0.
			   Get 0 by complementing FPMASpecialConstant.rgb */

			for(i=1; i<3; i++)
			{
				asColSrc[i].eType = HW_REGTYPE_TEMP;
				asColSrc[i].ui32Num = psProgramContext->ui32FPMASpecialConstantRegNum;
				asColSrc[i].eSrcMod = HW_SRCMOD_COMPLEMENT;
				asColSrc[i].eSrcNeg = HW_IMA_SRCNEG_NONE;
				asColSrc[i].eSrcSel.IMA = HW_IMA_SRCSEL_RGB;
			}
			
			break;
		}

		/* MODULATE				= S0.S1 */
		case GLES1_COMBINEOP_MODULATE:
		{
			/* FPMA does Src0 + Src1 * Src2, so for modulate
			   we need 0 + Src1 * Src2.
			   Get 0 by complementing FPMASpecialConstant.rgb */

			asColSrc[0].eType = HW_REGTYPE_TEMP;
			asColSrc[0].ui32Num = psProgramContext->ui32FPMASpecialConstantRegNum;
			asColSrc[0].eSrcMod = HW_SRCMOD_COMPLEMENT;
			asColSrc[0].eSrcNeg = HW_IMA_SRCNEG_NONE;
			asColSrc[0].eSrcSel.IMA = HW_IMA_SRCSEL_RGB;

			break;
		}

		/*	ADD 				=  S0 + S1 */
		case GLES1_COMBINEOP_ADD:
		{
			/* FPMA does Src0 + Src1 * Src2, so for add
			   we need Src0 + Src1 * 1.
			   Get 1 from FPMASpecialConstant.rgb */

			asColSrc[2].eType = HW_REGTYPE_TEMP;
			asColSrc[2].ui32Num = psProgramContext->ui32FPMASpecialConstantRegNum;
			asColSrc[2].eSrcMod = HW_SRCMOD_NONE;
			asColSrc[2].eSrcNeg = HW_IMA_SRCNEG_NONE;
			asColSrc[2].eSrcSel.IMA = HW_IMA_SRCSEL_RGB;

			break;
		}

		/*	SUBTRACT			=  S0 - S1 */
		case GLES1_COMBINEOP_SUBTRACT:
		{
			/* FPMA does Src0 + Src1 * Src2, so for subtract
			   we need -Src1 + Src0 * 1.
			   Get 1 from FPMASpecialConstant.rgb */

			asColSrc[2].eType = HW_REGTYPE_TEMP;
			asColSrc[2].ui32Num = psProgramContext->ui32FPMASpecialConstantRegNum;
			asColSrc[2].eSrcMod = HW_SRCMOD_NONE;
			asColSrc[2].eSrcNeg = HW_IMA_SRCNEG_NONE;
			asColSrc[2].eSrcSel.IMA = HW_IMA_SRCSEL_RGB;

			/* Negate source 0 */
			asColSrc[0].eSrcNeg = HW_IMA_SRCNEG_NEGATE;

			break;
		}

		/*	DOT3_RGB			= (S0.dot.S1), Replicate to RGB channels
			DOT3_RGBA			= (S0.dot.S1), Replicate to RGBA channels */
		case GLES1_COMBINEOP_DOT3_RGBA:
		case GLES1_COMBINEOP_DOT3_RGB:
		{
			/* DOT3 is encoded with a specific DOT3 instruction */

			if(ui32ColOp == GLES1_COMBINEOP_DOT3_RGBA)
			{
				bDOT3ReplicateToAlpha = IMG_TRUE;

				/* Change destination to current RGBA register, since
				   DOT3 RGBA writes a complete colour and alpha */
				sColDest.ui32Num = psProgramContext->ui32CurrentRGBARegNum;
			}

			break;
		}

		/*	INTERPOLATE		=  S0.S2 + (1 - S2).S1 */
		case GLES1_COMBINEOP_INTERPOLATE:
		{
			HWReg sInterpColDest;
			HWReg asInterpColSrc[3];
			IMG_UINT32 uInterpTempRegNum = AllocateTempReg(psProgramContext);

			/* Interpolate is implemented in two instructions:
			   Temp   = S0.S2;
			   Result = Temp + (1 - S2).S1 */

			/* Multiply (see code above for MODULATE) */
			asInterpColSrc[0].eType = HW_REGTYPE_TEMP;
			asInterpColSrc[0].ui32Num = psProgramContext->ui32FPMASpecialConstantRegNum;
			asInterpColSrc[0].eSrcMod = HW_SRCMOD_COMPLEMENT;
			asInterpColSrc[0].eSrcNeg = HW_IMA_SRCNEG_NONE;
			asInterpColSrc[0].eSrcSel.IMA = HW_IMA_SRCSEL_RGB;

			/* Copy source 0 and 2 into correct source locations */
			asInterpColSrc[1] = asColSrc[0];
			asInterpColSrc[2] = asColSrc[2];

			/* Modify destination to be a spare temporary */
			sInterpColDest = sColDest;
			sInterpColDest.ui32Num = uInterpTempRegNum;

			EncodeFPMA(gc,
					   &sInterpColDest,
					   asInterpColSrc,
					   HW_SRCTYPE_RGB,
					   psProgramContext);

			/* Multiply-Add */

			/* Use result from previous instruction */
			asColSrc[0].eType = HW_REGTYPE_TEMP;
			asColSrc[0].ui32Num = uInterpTempRegNum;
			asColSrc[0].eSrcMod = HW_SRCMOD_NONE;
			asColSrc[0].eSrcNeg = HW_IMA_SRCNEG_NONE;
			asColSrc[0].eSrcSel.IMA = HW_IMA_SRCSEL_RGB;

			/* Complement source 2 */
			if(asColSrc[2].eSrcMod == HW_SRCMOD_COMPLEMENT)
			{
				asColSrc[2].eSrcMod = HW_SRCMOD_NONE;
			}
			else
			{
				asColSrc[2].eSrcMod = HW_SRCMOD_COMPLEMENT;
			}

			/* Deallocate register now we are finished with it */
			DeallocateTempReg(psProgramContext, uInterpTempRegNum);

			break;
		}

		default:
		{
			PVR_DPF((PVR_DBG_WARNING,"EncodeGeneralColorBlend(): Invalid colour op!"));
		}
	}

	if((ui32ColOp != GLES1_COMBINEOP_DOT3_RGBA) &&
	   (ui32ColOp != GLES1_COMBINEOP_DOT3_RGB))
	{
		/* Encode the FPMA instruction for the colour blend */
		EncodeFPMA(gc,
				   &sColDest,
				   asColSrc,
				   HW_SRCTYPE_RGB,
				   psProgramContext);
	}
	else
	{
		/* Encode the DOT3 instruction */
		EncodeDOT3(gc,
				   &sColDest,
				   asColSrc, 
				   bDOT3ReplicateToAlpha,
				   psProgramContext);
	}

	/* Deallocate temporary register */
	DeallocateTempReg(psProgramContext, ui32TempRegNum);
}

/***********************************************************************************
 Function Name      : EncodeGeneralAlphaBlend
 Description        : Encodes a layer of texture blending using the fixed-point
					  multiply-add instruction (FPMA)
************************************************************************************/
static IMG_VOID EncodeGeneralAlphaBlend(GLES1Context *gc,
										IMG_UINT32			 ui32SrcLayerNum, 
										IMG_UINT32			 ui32EnabledLayerNum,
										FFTBBlendLayerDesc	*psBlendLayer,
										ProgramContext		*psProgramContext)
{
	IMG_UINT32 i;

	/* Variables to store the layer info in a convenient form */
	IMG_UINT32 ui32AlphaOp;

	IMG_UINT32 ui32NumAlphaSrcs;
	IMG_UINT32 ui32TempRegNum = HW_TEMPREG_INVALID;
	HWReg sAlphaDest;
	HWReg asAlphaSrc[3], *psSrc;

	/* Load special constant, if needed */
	LoadFPMASpecialConstant(gc, psProgramContext);


	/* Setup alpha destination register */
	sAlphaDest.ui32Num = psProgramContext->ui32IntermediateAlphaRegNum;
	sAlphaDest.eType = HW_REGTYPE_TEMP;
	sAlphaDest.eSrcMod = HW_SRCMOD_NONE;
	sAlphaDest.eSrcSel.IMA = HW_IMA_SRCSEL_A;

	ui32AlphaOp = GLES1_COMBINE_GET_ALPHAOP(psBlendLayer->ui32Op);

	switch (ui32AlphaOp)
	{
		case GLES1_COMBINEOP_REPLACE:
		{
			ui32NumAlphaSrcs = 1;

			break;
		}
		case GLES1_COMBINEOP_MODULATE:
		case GLES1_COMBINEOP_ADD:
		case GLES1_COMBINEOP_SUBTRACT:
		{
			ui32NumAlphaSrcs = 2;

			break;
		}
		case GLES1_COMBINEOP_INTERPOLATE:
		{
			ui32NumAlphaSrcs = 3;

			break;
		}
		case GLES1_COMBINEOP_ADDSIGNED:
		default:
		{
			PVR_DPF((PVR_DBG_ERROR,"EncodeGeneralAlphaBlend(): unsupported mode, shouldn't happen! Defaulting to REPLACE"));

			ui32AlphaOp = GLES1_COMBINEOP_REPLACE;

			ui32NumAlphaSrcs = 1;
		}
	}

	/* As above, for alpha */
	for(i=0; i<ui32NumAlphaSrcs; i++)
	{
		psSrc = &asAlphaSrc[aui32FPMASrcRemap[ui32AlphaOp][i]];

		psSrc->eSrcNeg = HW_IMA_SRCNEG_NONE;

		SetupAlphaSourceRegTypeNum(psBlendLayer->ui32AlphaSrcs,
								   i,
								   ui32SrcLayerNum,
								   ui32EnabledLayerNum,
								   psProgramContext->ui32CurrentRGBARegNum,
								   psProgramContext,
								   psSrc);
	
		/* Secondary attributes cannot be accessed in 
		   src0 by FPMA, so move into a temp */
		if((aui32FPMASrcRemap[ui32AlphaOp][i] == 0) &&
		   (psSrc->eType == HW_REGTYPE_SECATTR))
		{
			HWReg sDest;

			ui32TempRegNum = AllocateTempReg(psProgramContext);

			sDest.eType = HW_REGTYPE_TEMP;
			sDest.ui32Num = ui32TempRegNum;

			IntegerCopy(gc,
						psProgramContext, 
						HW_REGTYPE_TEMP,
						ui32TempRegNum,
						psSrc->eType,
						psSrc->ui32Num);

			psSrc->eType = sDest.eType;
			psSrc->ui32Num = sDest.ui32Num;
		}

		if(GLES1_COMBINE_GET_ALPHAOPERAND(psBlendLayer->ui32AlphaSrcs, i) & GLES1_COMBINEASRC_OPERANDONEMINUS)
		{
			psSrc->eSrcMod = HW_SRCMOD_COMPLEMENT;
		}
		else
		{
			psSrc->eSrcMod = HW_SRCMOD_NONE;
		}
	}

	/* 
		Instruction generation
		----------------------
	*/

	/* Alpha blend */
	switch (ui32AlphaOp)
	{
		/* REPLACE			=  S0		*/
		case GLES1_COMBINEOP_REPLACE:
		{
			/* FPMA does Src0 + Src1 * Src2, so for replace
			   we need Src0 + 0 * 0.
			   Get 0 from FPMASpecialConstant.a */

			for(i=1; i<3; i++)
			{
				asAlphaSrc[i].eType = HW_REGTYPE_TEMP;
				asAlphaSrc[i].ui32Num = psProgramContext->ui32FPMASpecialConstantRegNum;
				asAlphaSrc[i].eSrcMod = HW_SRCMOD_NONE;
				asAlphaSrc[i].eSrcNeg = HW_IMA_SRCNEG_NONE;
				asAlphaSrc[i].eSrcSel.IMA = HW_IMA_SRCSEL_A;
			}
						
			break;
		}

		/* MODULATE			=  S0.S1	*/
		case GLES1_COMBINEOP_MODULATE:
		{
			/* FPMA does Src0 + Src1 * Src2, so for modulate
			   we need 0 + Src1 * Src2.
			   Get 0 from FPMASpecialConstant.a */

			asAlphaSrc[0].eType = HW_REGTYPE_TEMP;
			asAlphaSrc[0].ui32Num = psProgramContext->ui32FPMASpecialConstantRegNum;
			asAlphaSrc[0].eSrcMod = HW_SRCMOD_NONE;
			asAlphaSrc[0].eSrcNeg = HW_IMA_SRCNEG_NONE;
			asAlphaSrc[0].eSrcSel.IMA = HW_IMA_SRCSEL_A;

			break;
		}

		/*	ADD 			=  S0 + S1	*/
		case GLES1_COMBINEOP_ADD:
		{
			/* FPMA does Src0 + Src1 * Src2, so for add
			   we need Src0 + Src1 * 1-0.
			   Get 1 by complementing FPMASpecialConstant.a */
			
			asAlphaSrc[2].eType = HW_REGTYPE_TEMP;
			asAlphaSrc[2].ui32Num = psProgramContext->ui32FPMASpecialConstantRegNum;
			asAlphaSrc[2].eSrcMod = HW_SRCMOD_COMPLEMENT;
			asAlphaSrc[2].eSrcNeg = HW_IMA_SRCNEG_NONE;
			asAlphaSrc[2].eSrcSel.IMA = HW_IMA_SRCSEL_A;

			break;
		}

		/*	SUBTRACT		=  S0 - S1 */
		case GLES1_COMBINEOP_SUBTRACT:
		{
			/* FPMA does Src0 + Src1 * Src2, so for subtract
			   we need -Src1 + Src0 * 1-0.
			   Get 1 by complementing FPMASpecialConstant.a */
			
			asAlphaSrc[2].eType = HW_REGTYPE_TEMP;
			asAlphaSrc[2].ui32Num = psProgramContext->ui32FPMASpecialConstantRegNum;
			asAlphaSrc[2].eSrcMod = HW_SRCMOD_COMPLEMENT;
			asAlphaSrc[2].eSrcNeg = HW_IMA_SRCNEG_NONE;
			asAlphaSrc[2].eSrcSel.IMA = HW_IMA_SRCSEL_A;

			/* Negate source 0 */
			asAlphaSrc[0].eSrcNeg = HW_IMA_SRCNEG_NEGATE;

			break;
		}

		/*	INTERPOLATE		=  S0.S2 + (1 - S2).S1 */
		case GLES1_COMBINEOP_INTERPOLATE:
		{
			HWReg sInterpAlphaDest;
			HWReg asInterpAlphaSrc[3];
			IMG_UINT32 uInterpTempRegNum = AllocateTempReg(psProgramContext);

			/* Interpolate is implemented in two instructions:
			   Temp   = S0.S2;
			   Result = Temp + (1 - S2).S1 */

			/* Multiply (see code above for MODULATE) */
			asInterpAlphaSrc[0].eType = HW_REGTYPE_TEMP;
			asInterpAlphaSrc[0].ui32Num = psProgramContext->ui32FPMASpecialConstantRegNum;
			asInterpAlphaSrc[0].eSrcMod = HW_SRCMOD_NONE;
			asInterpAlphaSrc[0].eSrcNeg = HW_IMA_SRCNEG_NONE;
			asInterpAlphaSrc[0].eSrcSel.IMA = HW_IMA_SRCSEL_A;

			/* Copy source 0 and 2 into correct source locations */
			asInterpAlphaSrc[1] = asAlphaSrc[0];
			asInterpAlphaSrc[2] = asAlphaSrc[2];

			/* Modify destination to be a spare temporary */
			sInterpAlphaDest = sAlphaDest;
			sInterpAlphaDest.ui32Num = uInterpTempRegNum;

			EncodeFPMA(gc,
					   &sInterpAlphaDest,
					   asInterpAlphaSrc,
					   HW_SRCTYPE_A,
					   psProgramContext);
			
			/* Multiply-Add */

			/* Use result from previous instruction */
			asAlphaSrc[0].eType = HW_REGTYPE_TEMP;
			asAlphaSrc[0].ui32Num = uInterpTempRegNum;
			asAlphaSrc[0].eSrcMod = HW_SRCMOD_NONE;
			asAlphaSrc[0].eSrcNeg = HW_IMA_SRCNEG_NONE;
			asAlphaSrc[0].eSrcSel.IMA = HW_IMA_SRCSEL_A;

			/* Complement source 2 */
			if(asAlphaSrc[2].eSrcMod == HW_SRCMOD_COMPLEMENT)
			{
				asAlphaSrc[2].eSrcMod = HW_SRCMOD_NONE;
			}
			else
			{
				asAlphaSrc[2].eSrcMod = HW_SRCMOD_COMPLEMENT;
			}

			/* Deallocate register now we are finished with it */
			DeallocateTempReg(psProgramContext, uInterpTempRegNum);

			break;
		}

		default:
		{
			PVR_DPF((PVR_DBG_WARNING,"EncodeGeneralAlphaBlend(): Invalid alpha op!"));
		}
	}

	/* Encode the FPMA instruction for the alpha blend */
	EncodeFPMA(gc,
			   &sAlphaDest,
			   asAlphaSrc,
			   HW_SRCTYPE_A,
			   psProgramContext);

	/* Deallocate temporary register */
	DeallocateTempReg(psProgramContext, ui32TempRegNum);
}

/***********************************************************************************
 Function Name      : SetSOP2SourceAssignment
 Description        :
************************************************************************************/
#ifdef INLINE_IS_PRAGMA
#pragma inline(SetSOP2SourceAssignment)
#endif
static INLINE void SetSOP2SourceAssignment(SOP2Sources		*psSOP2Sources,
										   IMG_UINT32		eSOP2SourceType)
{
	psSOP2Sources->aeSrcAssignments[eSOP2SourceType] = psSOP2Sources->eCurrentSrc;
}

/***********************************************************************************
 Function Name      : GetSOP2SourceAssignment
 Description        :
************************************************************************************/
#ifdef INLINE_IS_PRAGMA
#pragma inline(GetSOP2SourceAssignment)
#endif
static INLINE IMG_UINT32 GetSOP2SourceAssignment(SOP2Sources	*psSOP2Sources,
												 IMG_UINT32		eSOP2SourceType)
{
	return psSOP2Sources->aeSrcAssignments[eSOP2SourceType];
}

/***********************************************************************************
 Function Name      : GetSOP2SourceTypeFromColorSrc
 Description        :
************************************************************************************/
#ifdef INLINE_IS_PRAGMA
#pragma inline(GetSOP2SourceTypeFromColorSrc)
#endif
static INLINE IMG_UINT32 GetSOP2SourceTypeFromColorSrc(IMG_UINT32					ui32ColorSrcs,
															 IMG_UINT32					uColorSrcNum,
															 IMG_UINT32					ui32EnabledLayerNum,
															 IMG_UINT32					ui32SrcLayerNum)
{
	IMG_UINT32 ui32Src = GLES1_COMBINE_GET_COLSRC(ui32ColorSrcs, uColorSrcNum);
	
	static const IMG_UINT32	eMapColSrcToSourceType[] = {SOP2_SRCTYPE_PRIMARY,
															SOP2_SRCTYPE_PREVIOUS,
															SOP2_SRCTYPE_TEXTURE0,
															SOP2_SRCTYPE_CONSTANT};

	IMG_UINT32 eSOP2SrcType;

	if(ui32Src == GLES1_COMBINECSRC_TEXTURE)
	{
		IMG_UINT32 uTexUnit;

#if defined(GLES1_EXTENSION_TEX_ENV_CROSSBAR)
		/* Is a unit number encoded in the source? (for crossbar) */
		if(GLES1_COMBINE_GET_COLCROSSBAR(ui32ColorSrcs, uColorSrcNum) & GLES1_COMBINECSRC_CROSSBAR)
		{
			uTexUnit = GLES1_COMBINE_GET_COLCROSSBAR(ui32ColorSrcs, uColorSrcNum) >> GLES1_COMBINECSRC_CROSSBAR_UNIT_SHIFT;
		}
		else
#endif /* defined(GLES1_EXTENSION_TEX_ENV_CROSSBAR) */
		{
			uTexUnit = ui32SrcLayerNum;
		}

		eSOP2SrcType = SOP2_SRCTYPE_TEXTURE0 + uTexUnit;
	}
	else if ((ui32Src==GLES1_COMBINECSRC_PREVIOUS) && (ui32EnabledLayerNum==0))
	{
		eSOP2SrcType = SOP2_SRCTYPE_PRIMARY;
	}
	else
	{
		eSOP2SrcType = eMapColSrcToSourceType[ui32Src];
	}

	return eSOP2SrcType;
}

/***********************************************************************************
 Function Name      : GetSOP2SourceTypeFromAlphaSrc
 Description        : 
************************************************************************************/
#ifdef INLINE_IS_PRAGMA
#pragma inline(GetSOP2SourceTypeFromAlphaSrc)
#endif
static INLINE IMG_UINT32 GetSOP2SourceTypeFromAlphaSrc(IMG_UINT32	ui32AlphaSrcs,
															IMG_UINT32					uAlphaSrcNum,
															IMG_UINT32					ui32EnabledLayerNum,
															IMG_UINT32					ui32SrcLayerNum)
{
	IMG_UINT32 ui32Src = GLES1_COMBINE_GET_ALPHASRC(ui32AlphaSrcs, uAlphaSrcNum);
	static const IMG_UINT32	eMapAlphaSrcToSourceType[] = {SOP2_SRCTYPE_PRIMARY,
															  SOP2_SRCTYPE_PREVIOUS,
															  SOP2_SRCTYPE_TEXTURE0,
															  SOP2_SRCTYPE_CONSTANT};
	IMG_UINT32 eSOP2SrcType;

	if(ui32Src == GLES1_COMBINEASRC_TEXTURE)
	{
		IMG_UINT32 uTexUnit;

#if defined(GLES1_EXTENSION_TEX_ENV_CROSSBAR)
		/* Is a unit number encoded in the source? (for crossbar) */
		if(GLES1_COMBINE_GET_ALPHACROSSBAR(ui32AlphaSrcs, uAlphaSrcNum) & GLES1_COMBINEASRC_CROSSBAR)
		{
			uTexUnit = GLES1_COMBINE_GET_ALPHACROSSBAR(ui32AlphaSrcs, uAlphaSrcNum) >> GLES1_COMBINEASRC_CROSSBAR_UNIT_SHIFT;
		}
		else
#endif /* defined(GLES1_EXTENSION_TEX_ENV_CROSSBAR) */
		{
			uTexUnit = ui32SrcLayerNum;
		}

		eSOP2SrcType = SOP2_SRCTYPE_TEXTURE0 + uTexUnit;
	}
	else if ((ui32Src==GLES1_COMBINEASRC_PREVIOUS) && (ui32EnabledLayerNum==0))
	{
		eSOP2SrcType = SOP2_SRCTYPE_PRIMARY;
	}
	else
	{
		eSOP2SrcType = eMapAlphaSrcToSourceType[ui32Src];
	}

	return eSOP2SrcType;
}

/***********************************************************************************
 Function Name      : SetupSOP2ColorSource
 Description        : 
************************************************************************************/
static void SetupSOP2ColorSource(IMG_UINT32				ui32ColorSrcs,
								 IMG_UINT32				uColorSrcNum,
								 IMG_UINT32				ui32SrcLayerNum,
								 IMG_UINT32				ui32EnabledLayerNum,
								 SOP2Sources			*psSOP2Sources,
								 ProgramContext			*psProgramContext,
								 USE_REGISTER			*asArg)
{
	IMG_UINT32	eSOP2SrcType;
	HWReg			sColSrc;

	/* Set up the source register type and number */
	SetupColourSourceRegTypeNum(ui32ColorSrcs,
								uColorSrcNum,
								ui32SrcLayerNum,
								ui32EnabledLayerNum,
								psProgramContext->ui32CurrentRGBARegNum,
								psProgramContext,
								&sColSrc);

	eSOP2SrcType = GetSOP2SourceTypeFromColorSrc(ui32ColorSrcs, uColorSrcNum, ui32EnabledLayerNum, ui32SrcLayerNum);

	/* If this source has not been assigned, assign it */
	if(GetSOP2SourceAssignment(psSOP2Sources, eSOP2SrcType) == SOP2_SRC_NONE)
	{
		SetSOP2SourceAssignment(psSOP2Sources, eSOP2SrcType);

		GLES1_ASSERT(aui32RemapSOP2SourceAssignmentToSrcNum[psSOP2Sources->eCurrentSrc]);

		SETUP_INSTRUCTION_ARG(aui32RemapSOP2SourceAssignmentToSrcNum[psSOP2Sources->eCurrentSrc], sColSrc.ui32Num, aeRegTypeRemap[sColSrc.eType], 0);

		psSOP2Sources->eCurrentSrc++;
	}
}

/***********************************************************************************
 Function Name      : SetupSOP2AlphaSource
 Description        : 
************************************************************************************/
static void SetupSOP2AlphaSource(IMG_UINT32					ui32AlphaSrcs,
								 IMG_UINT32					uAlphaSrcNum,
								 IMG_UINT32					ui32SrcLayerNum,
								 IMG_UINT32					ui32EnabledLayerNum,
								 SOP2Sources				*psSOP2Sources,
								 ProgramContext				*psProgramContext,
								 USE_REGISTER				*asArg)
{
	IMG_UINT32		 	  eSOP2SrcType;
	HWReg					  sAlphaSrc;

	/* Set up the source register type and number */
	SetupAlphaSourceRegTypeNum(ui32AlphaSrcs,
							   uAlphaSrcNum,
							   ui32SrcLayerNum,
							   ui32EnabledLayerNum,
							   psProgramContext->ui32CurrentRGBARegNum,
							   psProgramContext,
							   &sAlphaSrc);

	eSOP2SrcType = GetSOP2SourceTypeFromAlphaSrc(ui32AlphaSrcs, uAlphaSrcNum, ui32EnabledLayerNum, ui32SrcLayerNum);

	/* If this source has not been assigned, assign it */
	if(GetSOP2SourceAssignment(psSOP2Sources, eSOP2SrcType) == SOP2_SRC_NONE)
	{
		SetSOP2SourceAssignment(psSOP2Sources, eSOP2SrcType);

		GLES1_ASSERT(aui32RemapSOP2SourceAssignmentToSrcNum[psSOP2Sources->eCurrentSrc]);

		SETUP_INSTRUCTION_ARG(aui32RemapSOP2SourceAssignmentToSrcNum[psSOP2Sources->eCurrentSrc], sAlphaSrc.ui32Num, aeRegTypeRemap[sAlphaSrc.eType], 0);

		psSOP2Sources->eCurrentSrc++;
	}
}

/***********************************************************************************
 Function Name      : SetupSourcesForSOP2
 Description        : Decides the order in which the sources should be assigned for
					  the SOP2 instruction
************************************************************************************/
static IMG_VOID SetupSourcesForSOP2(FFTBBlendLayerDesc	*psBlendLayer,
								IMG_UINT32			 ui32SrcLayerNum,
								IMG_UINT32			 ui32EnabledLayerNum,
								SOP2Sources			*psSOP2Sources,
								ProgramContext		*psProgramContext,
								USE_REGISTER		*asArg,
								SWSourceInfo		*psSourceInfo)
{
	IMG_UINT32 i;
	IMG_UINT32 ui32AlphaOp, ui32ColOp;
	IMG_BOOL bUseColorSrcs = IMG_FALSE;
	IMG_BOOL bUseAlphaSrcs = IMG_FALSE;

	ui32ColOp	 = GLES1_COMBINE_GET_COLOROP(psBlendLayer->ui32Op);
	ui32AlphaOp = GLES1_COMBINE_GET_ALPHAOP(psBlendLayer->ui32Op);

	/* If either colour or alpha operation is subtract, respect the order 
	   of the sources needed by subtract, since it is a non-commutative 
	   operation.
	   NOTE: if both ops are subtract, IntegerSetupTextureBlend() will
	   have ensured they use the same sources, so it doesn't matter whether
	   colour or alpha is chosen to assign the sources */
	if(ui32ColOp == GLES1_COMBINEOP_SUBTRACT)
	{
		bUseColorSrcs = IMG_TRUE;

		if(psSourceInfo->ui32NumUniqueColorSources == 1)
		{
			bUseAlphaSrcs = IMG_TRUE;
		}
	}
	else if(ui32AlphaOp == GLES1_COMBINEOP_SUBTRACT)
	{
		bUseAlphaSrcs = IMG_TRUE;

		if(psSourceInfo->ui32NumUniqueAlphaSources == 1)
		{
			bUseColorSrcs = IMG_TRUE;
		}
	}
	else
	{
		/* If 2 unique sources are used in one channel, 
		   use that channel to assign sources */
		if(psSourceInfo->ui32NumUniqueColorSources == 2)
		{
			bUseColorSrcs = IMG_TRUE;
		}
		else if(psSourceInfo->ui32NumUniqueAlphaSources == 2)
		{
			bUseAlphaSrcs = IMG_TRUE;
		}
		else
		{
			/* 1 source each for colour and alpha */
			bUseColorSrcs = IMG_TRUE;
			bUseAlphaSrcs = IMG_TRUE;
		}
	}

	if(bUseColorSrcs)
	{
		for(i=0; i<psSourceInfo->ui32NumColorSources; i++)
		{
			SetupSOP2ColorSource(psBlendLayer->ui32ColorSrcs,
								 i,
								 ui32SrcLayerNum,
								 ui32EnabledLayerNum,
								 psSOP2Sources,
								 psProgramContext,
								 asArg);
		}
	}

	if(bUseAlphaSrcs)
	{
		for(i=0; i<psSourceInfo->ui32NumAlphaSources; i++)
		{
			SetupSOP2AlphaSource(psBlendLayer->ui32AlphaSrcs,
								 i,
								 ui32SrcLayerNum,
								 ui32EnabledLayerNum,
								 psSOP2Sources,
								 psProgramContext,
								 asArg);
		}
	}

	if(psSourceInfo->ui32NumUniqueSources == 1)
	{
		/* If only one source was used, use special constant 0 for source 2.
		   (Avoids HW-sim outfile mistmatches when using an uninitialised register) */
		SETUP_INSTRUCTION_ARG(2, EURASIA_USE_SPECIAL_CONSTANT_ZERO, USEASM_REGTYPE_FPCONSTANT, 0);
	}
}

/***********************************************************************************
 Function Name      : EncodeRestrictedBlend
 Description        : Encodes a layer of texture blending using the 
					  sum-of-products two-source instruction (SOP2)
************************************************************************************/
static IMG_VOID EncodeRestrictedBlend(GLES1Context *gc,
									  IMG_UINT32		 uDestRegNum,
									  IMG_UINT32		 ui32SrcLayerNum,
									  IMG_UINT32		 ui32EnabledLayerNum,
									  FFTBBlendLayerDesc *psBlendLayer,
									  SWSourceInfo		 *psSourceInfo,
									  ProgramContext	 *psProgramContext)
{
	IMG_UINT32	i;

	/* Variables to store the layer info in a convenient form */
	IMG_UINT32 ui32AlphaOp, ui32ColOp;

	SOP2Sources	sSOP2Sources;

	USE_REGISTER asArg[7];

	/* Dst */
	SETUP_INSTRUCTION_ARG(0, uDestRegNum, USEASM_REGTYPE_TEMP, 0);


	sSOP2Sources.eCurrentSrc = SOP2_SRC1;

	for(i=0; i<SOP2_SRCTYPE_COUNT; i++)
	{
		sSOP2Sources.aeSrcAssignments[i] = SOP2_SRC_NONE;
	}

	/* Setup the sources for the SOP2 instruction,
	   assigning them to Src1 and Src2 */
	SetupSourcesForSOP2(psBlendLayer,
						ui32SrcLayerNum,
						ui32EnabledLayerNum,
						&sSOP2Sources,
						psProgramContext,
						asArg,
						psSourceInfo);

	/* Src1 modifier */
	SETUP_INSTRUCTION_ARG(3, USEASM_INTSRCSEL_NONE, USEASM_REGTYPE_INTSRCSEL, 0);


	ui32ColOp	 = GLES1_COMBINE_GET_COLOROP(psBlendLayer->ui32Op);
	ui32AlphaOp = GLES1_COMBINE_GET_ALPHAOP(psBlendLayer->ui32Op);

	/*
		Instruction generation
		----------------------

		SOP2 does: RGB = CMod1[CSel1] * Src1Mod[Src1.rgb] COP CMod2[CSel2] * Src2.rgb
				   A   = AMod1[ASel1] * ASrc1Mod[Src1.a]  AOP AMod2[ASel2] * Src2.a

		Complement[Zero] = 1-0 = 1 is used to choose the parts of the equation that are needed.
		None[Zero] = 0 is used to zero-out the unwanted parts of the equation.
	*/

	/* Colour blend */
	switch (ui32ColOp)
	{
		/* REPLACE				= S0 */
		case GLES1_COMBINEOP_REPLACE:
		{
			IMG_UINT32 eSOP2SrcType;

			eSOP2SrcType = GetSOP2SourceTypeFromColorSrc(psBlendLayer->ui32ColorSrcs, 0, ui32EnabledLayerNum, ui32SrcLayerNum);

			/* If the source for the replace was assigned to source 2, the modifiers must 
			   be swapped to choose the correct source */
			if(sSOP2Sources.aeSrcAssignments[eSOP2SrcType] == SOP2_SRC1)
			{
				/* Src1 colour factor */
				SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_ONE, USEASM_REGTYPE_INTSRCSEL, 0);

				/* Src2 colour factor */
				SETUP_INSTRUCTION_ARG(5, USEASM_INTSRCSEL_ZERO, USEASM_REGTYPE_INTSRCSEL, 0);
			}
			else
			{
				/* Src1 colour factor */
				SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_ZERO, USEASM_REGTYPE_INTSRCSEL, 0);

				/* Src2 colour factor */
				SETUP_INSTRUCTION_ARG(5, USEASM_INTSRCSEL_ONE, USEASM_REGTYPE_INTSRCSEL, 0);
			}

			/* Colour operation */
			SETUP_INSTRUCTION_ARG(6, USEASM_INTSRCSEL_ADD, USEASM_REGTYPE_INTSRCSEL, 0);

			break;
		}

		/* MODULATE				= S0.S1 */
		case GLES1_COMBINEOP_MODULATE:
		{
			/* SOP2 can also multiply result by 2, but only when Src2 is used for CSEL2
			   (e.g. a multiply operation) */
			if(GLES1_COMBINE_GET_COLORSCALE(psBlendLayer->ui32Op) == GLES1_COMBINEOP_SCALE_TWO)
			{
				/* Src1 colour factor */
				SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_SRC2SCALE, USEASM_REGTYPE_INTSRCSEL, 0);

				/* Remove scaling from blend layer, so it isn't done again */
				psBlendLayer->ui32Op = GLES1_COMBINE_SET_COLORSCALE(psBlendLayer->ui32Op, GLES1_COMBINEOP_SCALE_ONE);
			}
			else
			{
				/* Src1 colour factor */
				SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_SRC2, USEASM_REGTYPE_INTSRCSEL, 0);
			}

			/* Src2 colour factor */
			SETUP_INSTRUCTION_ARG(5, USEASM_INTSRCSEL_ZERO, USEASM_REGTYPE_INTSRCSEL, 0);


			/* Colour operation */
			SETUP_INSTRUCTION_ARG(6, USEASM_INTSRCSEL_ADD, USEASM_REGTYPE_INTSRCSEL, 0);

			break;
		}

		/*	ADD 				=  S0 + S1 */
		case GLES1_COMBINEOP_ADD:
		{
			/* Src1 colour factor */
			SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_ONE, USEASM_REGTYPE_INTSRCSEL, 0);

			/* Src2 colour factor */
			SETUP_INSTRUCTION_ARG(5, USEASM_INTSRCSEL_ONE, USEASM_REGTYPE_INTSRCSEL, 0);

			/* Colour operation */
			SETUP_INSTRUCTION_ARG(6, USEASM_INTSRCSEL_ADD, USEASM_REGTYPE_INTSRCSEL, 0);

			break;
		}

		/*	ADD SIGNED			=  S0 + S1 - 0.5 */
		case GLES1_COMBINEOP_ADDSIGNED:
		{
			/* Src1 colour factor */
			SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_ONE, USEASM_REGTYPE_INTSRCSEL, 0);

			/* Src2 colour factor */
			SETUP_INSTRUCTION_ARG(5, USEASM_INTSRCSEL_ZEROS, USEASM_REGTYPE_INTSRCSEL, USEASM_ARGFLAGS_COMPLEMENT);

			/* Colour operation */
			SETUP_INSTRUCTION_ARG(6, USEASM_INTSRCSEL_ADD, USEASM_REGTYPE_INTSRCSEL, 0);

			break;
		}

		/*	SUBTRACT			=  S0 - S1 */
		case GLES1_COMBINEOP_SUBTRACT:
		{
			/* Src1 colour factor */
			SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_ONE, USEASM_REGTYPE_INTSRCSEL, 0);

			/* Src2 colour factor */
			SETUP_INSTRUCTION_ARG(5, USEASM_INTSRCSEL_ONE, USEASM_REGTYPE_INTSRCSEL, 0);

			/* Colour operation */
			SETUP_INSTRUCTION_ARG(6, USEASM_INTSRCSEL_SUB, USEASM_REGTYPE_INTSRCSEL, 0);

			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_WARNING,"EncodeRestrictedBlend(): Invalid colour op!"));
		}
	}
	
	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_SOP2, USEASM_OPFLAGS1_MAINISSUE, 0, 0, asArg, 7);


	/* Src1 modifier */
	SETUP_INSTRUCTION_ARG(0, USEASM_INTSRCSEL_NONE, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Alpha blend */
	switch (ui32AlphaOp)
	{
		/* REPLACE			=  S0		*/
		case GLES1_COMBINEOP_REPLACE:
		{
			IMG_UINT32 eSOP2SrcType;

			eSOP2SrcType = GetSOP2SourceTypeFromAlphaSrc(psBlendLayer->ui32AlphaSrcs, 0, ui32EnabledLayerNum, ui32SrcLayerNum);

			if(sSOP2Sources.aeSrcAssignments[eSOP2SrcType] == SOP2_SRC1)
			{
				/* Src1 alpha factor */
				SETUP_INSTRUCTION_ARG(1, USEASM_INTSRCSEL_ONE, USEASM_REGTYPE_INTSRCSEL, 0);

				/* Src2 alpha factor */
				SETUP_INSTRUCTION_ARG(2, USEASM_INTSRCSEL_ZERO, USEASM_REGTYPE_INTSRCSEL, 0);
			}
			else
			{
				/* Src1 alpha factor */
				SETUP_INSTRUCTION_ARG(1, USEASM_INTSRCSEL_ZERO, USEASM_REGTYPE_INTSRCSEL, 0);

				/* Src2 alpha factor */
				SETUP_INSTRUCTION_ARG(2, USEASM_INTSRCSEL_ONE, USEASM_REGTYPE_INTSRCSEL, 0);
			}

			/* Alpha operation */
			SETUP_INSTRUCTION_ARG(3, USEASM_INTSRCSEL_ADD, USEASM_REGTYPE_INTSRCSEL, 0);

			break;
		}

		/* MODULATE			=  S0.S1	*/
		case GLES1_COMBINEOP_MODULATE:
		{
			if(GLES1_COMBINE_GET_ALPHASCALE(psBlendLayer->ui32Op) == GLES1_COMBINEOP_SCALE_TWO)
			{
				/* Src1 alpha factor */
				SETUP_INSTRUCTION_ARG(1, USEASM_INTSRCSEL_SRC2SCALE, USEASM_REGTYPE_INTSRCSEL, 0);
				
				psBlendLayer->ui32Op = GLES1_COMBINE_SET_ALPHASCALE(psBlendLayer->ui32Op, GLES1_COMBINEOP_SCALE_ONE);
			}
			else
			{
				/* Src1 alpha factor */
				SETUP_INSTRUCTION_ARG(1, USEASM_INTSRCSEL_SRC2ALPHA, USEASM_REGTYPE_INTSRCSEL, 0);
			}

			/* Src2 alpha factor */
			SETUP_INSTRUCTION_ARG(2, USEASM_INTSRCSEL_ZERO, USEASM_REGTYPE_INTSRCSEL, 0);

			/* Alpha operation */
			SETUP_INSTRUCTION_ARG(3, USEASM_INTSRCSEL_ADD, USEASM_REGTYPE_INTSRCSEL, 0);

			break;
		}

		/*	ADD 			=  S0 + S1	*/
		case GLES1_COMBINEOP_ADD:
		{
			/* Src1 alpha factor */
			SETUP_INSTRUCTION_ARG(1, USEASM_INTSRCSEL_ONE, USEASM_REGTYPE_INTSRCSEL, 0);

			/* Src2 alpha factor */
			SETUP_INSTRUCTION_ARG(2, USEASM_INTSRCSEL_ONE, USEASM_REGTYPE_INTSRCSEL, 0);

			/* Alpha operation */
			SETUP_INSTRUCTION_ARG(3, USEASM_INTSRCSEL_ADD, USEASM_REGTYPE_INTSRCSEL, 0);

			break;
		}

		/*	ADD SIGNED		=  S0 + S1 - 0.5 */
		case GLES1_COMBINEOP_ADDSIGNED:
		{
			/* Src1 alpha factor */
			SETUP_INSTRUCTION_ARG(1, USEASM_INTSRCSEL_ONE, USEASM_REGTYPE_INTSRCSEL, 0);

			/* Src2 alpha factor */
			SETUP_INSTRUCTION_ARG(2, USEASM_INTSRCSEL_ZEROS, USEASM_REGTYPE_INTSRCSEL, USEASM_ARGFLAGS_COMPLEMENT);

			/* Alpha operation */
			SETUP_INSTRUCTION_ARG(3, USEASM_INTSRCSEL_ADD, USEASM_REGTYPE_INTSRCSEL, 0);

			break;
		}

		/*	SUBTRACT		=  S0 - S1 */
		case GLES1_COMBINEOP_SUBTRACT:
		{
			/* Src1 alpha factor */
			SETUP_INSTRUCTION_ARG(1, USEASM_INTSRCSEL_ONE, USEASM_REGTYPE_INTSRCSEL, 0);

			/* Src2 alpha factor */
			SETUP_INSTRUCTION_ARG(2, USEASM_INTSRCSEL_ONE, USEASM_REGTYPE_INTSRCSEL, 0);

			/* Alpha operation */
			SETUP_INSTRUCTION_ARG(3, USEASM_INTSRCSEL_SUB, USEASM_REGTYPE_INTSRCSEL, 0);

			break;
		}

		default:
		{
			PVR_DPF((PVR_DBG_WARNING,"EncodeRestrictedBlend(): Invalid alpha op!"));
		}
	}

	/* Destination modifier */
	SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_NONE, USEASM_REGTYPE_INTSRCSEL, 0);

	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_ASOP2, 0, USEASM_OPFLAGS2_COISSUE, 0, asArg, 5);
}

/***********************************************************************************
 Function Name      : EncodeGeneralADDSIGNEDBlend
 Description        : 
************************************************************************************/
static IMG_VOID EncodeGeneralADDSIGNEDBlend(GLES1Context *gc,
											IMG_UINT32			ui32SrcLayerNum, 
											IMG_UINT32			ui32EnabledLayerNum,
											FFTBBlendLayerDesc *psBlendLayer,
											ProgramContext	   *psProgramContext)
{
	HWReg		sDest;
	HWReg		asSources[2];
	IMG_UINT32	ui32TempRegNum[2] = {HW_TEMPREG_INVALID, HW_TEMPREG_INVALID};
	IMG_UINT32	i;

	if(GLES1_COMBINE_GET_COLOROP(psBlendLayer->ui32Op) == GLES1_COMBINEOP_ADDSIGNED)
	{
		/* Setup structures describing sources */
		for(i=0; i<2; i++)
		{
			SetupColourSourceRegTypeNum(psBlendLayer->ui32ColorSrcs,
										i,
										ui32SrcLayerNum,
										ui32EnabledLayerNum,
										psProgramContext->ui32CurrentRGBARegNum,
										psProgramContext,
										&asSources[i]);

			asSources[i].eSrcNeg = HW_IMA_SRCNEG_NONE;

			/* LSB indicates that source should be complemented (1-Src) */
			if(GLES1_COMBINE_GET_COLOPERAND(psBlendLayer->ui32ColorSrcs, i) & GLES1_COMBINECSRC_OPERANDONEMINUS)
			{
				asSources[i].eSrcMod = HW_SRCMOD_COMPLEMENT;
			}
			else
			{
				asSources[i].eSrcMod = HW_SRCMOD_NONE;
			}
			
			/* Alpha swizzle */
			if(GLES1_COMBINE_GET_COLOPERAND(psBlendLayer->ui32ColorSrcs, i) & GLES1_COMBINECSRC_OPERANDALPHA)
			{
				/* Source select alpha */
				asSources[i].eSrcSel.IMA = HW_IMA_SRCSEL_A;
			}
			else
			{
				/* Source select RGB */
				asSources[i].eSrcSel.IMA = HW_IMA_SRCSEL_RGB;
			}

			/* SOP2 cannot handle complement or alpha replicate.
			   If these modifiers are used, implement them with FPMA */
			if((asSources[i].eSrcMod != HW_SRCMOD_NONE) ||
			   (asSources[i].eSrcSel.IMA != HW_IMA_SRCSEL_RGB))
			{
				/* Allocate temporary register numbers */
				ui32TempRegNum[i] = AllocateTempReg(psProgramContext);

				sDest.eType = HW_REGTYPE_TEMP;
				sDest.ui32Num = ui32TempRegNum[i];

				EncodeFPMAReplace(gc,
								  &sDest,
								  &asSources[i],
								  HW_SRCTYPE_RGB,
								  psProgramContext);

				/* Update source to refer to result of above instruction */
				asSources[i].eType = HW_REGTYPE_TEMP;
				asSources[i].ui32Num = ui32TempRegNum[i];
				asSources[i].eSrcMod = HW_SRCMOD_NONE;
				asSources[i].eSrcNeg = HW_IMA_SRCNEG_NONE;
				asSources[i].eSrcSel.IMA = HW_IMA_SRCSEL_RGB;
			}
		}

		/* Setup structure describing destination */
		sDest.eType = HW_REGTYPE_TEMP;
		sDest.ui32Num = psProgramContext->ui32IntermediateRGBRegNum;

		/* Implement ADDSIGNED using SOP2 */
		IntegerADDSIGNEDSOP2(gc,
							 &sDest,
							 asSources,
							 psProgramContext);
	}
	else
	{
		/* Not ADDSIGNED, so implement with FPMA */
		EncodeGeneralColorBlend(gc,
								ui32SrcLayerNum,
								ui32EnabledLayerNum,
								psBlendLayer,
								psProgramContext);
	}

	if(GLES1_COMBINE_GET_COLOROP(psBlendLayer->ui32Op) != GLES1_COMBINEOP_DOT3_RGBA)
	{
		if(GLES1_COMBINE_GET_ALPHAOP(psBlendLayer->ui32Op) == GLES1_COMBINEOP_ADDSIGNED)
		{
			/* Setup structures describing sources */
			for(i=0; i<2; i++)
			{
				SetupAlphaSourceRegTypeNum(psBlendLayer->ui32AlphaSrcs,
										   i,
										   ui32SrcLayerNum,
										   ui32EnabledLayerNum,
										   psProgramContext->ui32CurrentRGBARegNum,
										   psProgramContext,
										   &asSources[i]);
				
				asSources[i].eSrcNeg = HW_IMA_SRCNEG_NONE;

				if(GLES1_COMBINE_GET_ALPHAOPERAND(psBlendLayer->ui32AlphaSrcs, i) & GLES1_COMBINEASRC_OPERANDONEMINUS)
				{
					asSources[i].eSrcMod = HW_SRCMOD_COMPLEMENT;
				}
				else
				{
					asSources[i].eSrcMod = HW_SRCMOD_NONE;
				}
				
				asSources[i].eSrcSel.IMA = HW_IMA_SRCSEL_A;

				if(asSources[i].eSrcMod != HW_SRCMOD_NONE)
				{
					/* Allocate temporary register numbers, if not already done */
					if(ui32TempRegNum[i] == HW_TEMPREG_INVALID)
					{
						ui32TempRegNum[i] = AllocateTempReg(psProgramContext);
					}

					sDest.eType = HW_REGTYPE_TEMP;
					sDest.ui32Num = ui32TempRegNum[i];

					EncodeFPMAReplace(gc,
									  &sDest,
									  &asSources[i],
									  HW_SRCTYPE_A,
									  psProgramContext);

					/* Update source to refer to result of above instruction */
					asSources[i].eType = HW_REGTYPE_TEMP;
					asSources[i].ui32Num = ui32TempRegNum[i];
					asSources[i].eSrcMod = HW_SRCMOD_NONE;
					asSources[i].eSrcNeg = HW_IMA_SRCNEG_NONE;
					asSources[i].eSrcSel.IMA = HW_IMA_SRCSEL_A;
				}
			}

			/* Setup structure describing destination */
			sDest.eType = HW_REGTYPE_TEMP;
			sDest.ui32Num = psProgramContext->ui32IntermediateAlphaRegNum;

			/* Implement ADDSIGNED using SOP2 */
			IntegerADDSIGNEDSOP2(gc,
								 &sDest,
								 asSources,
								 psProgramContext);;
		}
		else
		{
			/* Not ADDSIGNED, so implement with FPMA */
			EncodeGeneralAlphaBlend(gc,
									ui32SrcLayerNum,
									ui32EnabledLayerNum,
									psBlendLayer,
									psProgramContext);
		}
	}

	/* Free any allocated registers */
	DeallocateTempReg(psProgramContext, ui32TempRegNum[0]);
	DeallocateTempReg(psProgramContext, ui32TempRegNum[1]);
}


/***********************************************************************************
 Function Name      : IntegerCopy
 Description        : 
************************************************************************************/
static IMG_VOID IntegerCopy(GLES1Context *gc,
							ProgramContext	*psProgramContext,
							HWRegType	 eDestType,
							IMG_UINT32	 ui32DestNum,
							HWRegType	 eSrcType,
							IMG_UINT32	 ui32SrcNum)
{
	USE_REGISTER asArg[2];
		
	/* Dst */
	SETUP_INSTRUCTION_ARG(0, ui32DestNum, aeRegTypeRemap[eDestType], 0);

	/* Src */
	SETUP_INSTRUCTION_ARG(1, ui32SrcNum, aeRegTypeRemap[eSrcType], 0);

	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_MOV, 0, 0, 0, asArg, 2);
}


#if defined(FIX_HW_BRN_25211)

/***********************************************************************************
 Function Name      : SetupBaseColour
 Description        : F16 -> U8
************************************************************************************/
static IMG_VOID SetupBaseColour(GLES1Context *gc, ProgramContext *psProgramContext)
{
	USE_REGISTER asArg[7];
		
	/* 
	   SOP2 does: RGB = CMod1[CSel1] * Src1Mod[Src1.rgb] COP CMod2[CSel2] * Src2.rgb
				  A   = AMod1[ASel1] * ASrc1Mod[Src1.a]  AOP AMod2[ASel2] * Src2.a 

	   To combine colour and alpha we do:

				   RGB = (1 * Src1.rgb) + (0 * Src2.rgb)
				   A   = (0 * Src1.a)   + (1 * Src2.a)
	*/
	GLES1_ASSERT(psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR0]==HW_TEMPREG_INVALID)

	psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR0] = AllocateTempReg(psProgramContext);

	/* Dst */
	SETUP_INSTRUCTION_ARG(0, psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR0], USEASM_REGTYPE_TEMP, 0);

	/* Src 1 */
	SETUP_INSTRUCTION_ARG(1, psProgramContext->aui32InputRegMappings[HW_INPUTREG_COLOUR0], USEASM_REGTYPE_PRIMATTR, USEASM_ARGFLAGS_FMTC10);

	/* Src 2 */
	SETUP_INSTRUCTION_ARG(2, psProgramContext->aui32InputRegMappings[HW_INPUTREG_COLOUR0]+1, USEASM_REGTYPE_PRIMATTR, USEASM_ARGFLAGS_FMTC10);

	/* Src1 modifier */
	SETUP_INSTRUCTION_ARG(3, USEASM_INTSRCSEL_NONE, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Src1 colour factor */
	SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_ONE, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Src2 colour factor */
	SETUP_INSTRUCTION_ARG(5, USEASM_INTSRCSEL_ZERO, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Colour operation */
	SETUP_INSTRUCTION_ARG(6, USEASM_INTSRCSEL_ADD, USEASM_REGTYPE_INTSRCSEL, 0);

	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_SOP2, USEASM_OPFLAGS1_MAINISSUE, USEASM_OPFLAGS2_FORMATSELECT, 0, asArg, 7);


	/* Src1 modifier */
	SETUP_INSTRUCTION_ARG(0, USEASM_INTSRCSEL_NONE, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Src1 alpha factor */
	SETUP_INSTRUCTION_ARG(1, USEASM_INTSRCSEL_ZERO, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Src2 alpha factor */
	SETUP_INSTRUCTION_ARG(2, USEASM_INTSRCSEL_ONE, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Alpha operation */
	SETUP_INSTRUCTION_ARG(3, USEASM_INTSRCSEL_ADD, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Destination modifier */
	SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_NONE, USEASM_REGTYPE_INTSRCSEL, 0);

	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_ASOP2, 0, USEASM_OPFLAGS2_COISSUE, 0, asArg, 5);
}	

#endif /* defined(FIX_HW_BRN_25211) */


/***********************************************************************************
 Function Name      : SetupTwoSidedColour
 Description        : Overwrites colour0 with colour1
************************************************************************************/
static IMG_VOID SetupTwoSidedColour(GLES1Context *gc, ProgramContext *psProgramContext)
{
	USE_REGISTER asArg[7];

#if defined(FIX_HW_BRN_25211)
	if(gc->sAppHints.bUseC10Colours)
	{
		/* 
		   SOP2 does: RGB = CMod1[CSel1] * Src1Mod[Src1.rgb] COP CMod2[CSel2] * Src2.rgb
					  A   = AMod1[ASel1] * ASrc1Mod[Src1.a]  AOP AMod2[ASel2] * Src2.a 

		   To combine colour and alpha we do:

					   RGB = (1 * Src1.rgb) + (0 * Src2.rgb)
					   A   = (0 * Src1.a)   + (1 * Src2.a)
		*/
		GLES1_ASSERT(psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR0]!=HW_TEMPREG_INVALID)
		GLES1_ASSERT(psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR1]==HW_TEMPREG_INVALID)

		psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR1] = AllocateTempReg(psProgramContext);

		/* Dst */
		SETUP_INSTRUCTION_ARG(0, psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR1], USEASM_REGTYPE_TEMP, 0);

		/* Src 1 */
		SETUP_INSTRUCTION_ARG(1, psProgramContext->aui32InputRegMappings[HW_INPUTREG_COLOUR1], USEASM_REGTYPE_PRIMATTR, USEASM_ARGFLAGS_FMTC10);

		/* Src 2 */
		SETUP_INSTRUCTION_ARG(2, psProgramContext->aui32InputRegMappings[HW_INPUTREG_COLOUR1]+1, USEASM_REGTYPE_PRIMATTR, USEASM_ARGFLAGS_FMTC10);

		/* Src1 modifier */
		SETUP_INSTRUCTION_ARG(3, USEASM_INTSRCSEL_NONE, USEASM_REGTYPE_INTSRCSEL, 0);

		/* Src1 colour factor */
		SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_ONE, USEASM_REGTYPE_INTSRCSEL, 0);

		/* Src2 colour factor */
		SETUP_INSTRUCTION_ARG(5, USEASM_INTSRCSEL_ZERO, USEASM_REGTYPE_INTSRCSEL, 0);

		/* Colour operation */
		SETUP_INSTRUCTION_ARG(6, USEASM_INTSRCSEL_ADD, USEASM_REGTYPE_INTSRCSEL, 0);

		AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_SOP2, USEASM_OPFLAGS1_MAINISSUE, USEASM_OPFLAGS2_FORMATSELECT, 0, asArg, 7);


		/* Src1 modifier */
		SETUP_INSTRUCTION_ARG(0, USEASM_INTSRCSEL_NONE, USEASM_REGTYPE_INTSRCSEL, 0);

		/* Src1 alpha factor */
		SETUP_INSTRUCTION_ARG(1, USEASM_INTSRCSEL_ZERO, USEASM_REGTYPE_INTSRCSEL, 0);

		/* Src2 alpha factor */
		SETUP_INSTRUCTION_ARG(2, USEASM_INTSRCSEL_ONE, USEASM_REGTYPE_INTSRCSEL, 0);

		/* Alpha operation */
		SETUP_INSTRUCTION_ARG(3, USEASM_INTSRCSEL_ADD, USEASM_REGTYPE_INTSRCSEL, 0);

		/* Destination modifier */
		SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_NONE, USEASM_REGTYPE_INTSRCSEL, 0);

		AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_ASOP2, 0, USEASM_OPFLAGS2_COISSUE, 0, asArg, 5);
	}
	else
#endif /* defined(FIX_HW_BRN_25211) */
	{
		GLES1_ASSERT(psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR0]==HW_TEMPREG_INVALID)
		GLES1_ASSERT(psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR1]==HW_TEMPREG_INVALID)

		psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR0] = AllocateTempReg(psProgramContext);

		/* Dst */
		SETUP_INSTRUCTION_ARG(0, psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR0], USEASM_REGTYPE_TEMP, 0);

		/* Src */
		SETUP_INSTRUCTION_ARG(1, psProgramContext->aui32InputRegMappings[HW_INPUTREG_COLOUR0], USEASM_REGTYPE_PRIMATTR, 0);

		AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_MOV, 0, 0, 0, asArg, 2);
	}


	/* Dst */
	SETUP_INSTRUCTION_ARG(0, 0, USEASM_REGTYPE_TEMP, USEASM_ARGFLAGS_DISABLEWB);

	/* Src1 */
	SETUP_INSTRUCTION_ARG(1, 0, USEASM_REGTYPE_PREDICATE, 0);

	/* Src2 */
	SETUP_INSTRUCTION_ARG(2, 16, USEASM_REGTYPE_GLOBAL, 0);

	/* Src3 */
	SETUP_INSTRUCTION_ARG(3, 1, USEASM_REGTYPE_IMMEDIATE, 0);

	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_AND, USEASM_OPFLAGS1_TESTENABLE | (0x1<<USEASM_OPFLAGS1_MASK_SHIFT), 0, 
					(USEASM_TEST_CHANSEL_C0<<USEASM_TEST_CHANSEL_SHIFT) | 
					 USEASM_TEST_CRCOMB_AND | 
					(USEASM_TEST_ZERO_NONZERO<<USEASM_TEST_ZERO_SHIFT) | 
					(USEASM_TEST_SIGN_TRUE<<USEASM_TEST_SIGN_SHIFT),
					 asArg, 4);

	if(psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR0]==HW_TEMPREG_INVALID)
	{
		psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR0] = AllocateTempReg(psProgramContext);
	}

	/* Dst */
	SETUP_INSTRUCTION_ARG(0, psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR0], USEASM_REGTYPE_TEMP, 0);

	if(psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR1]!=HW_TEMPREG_INVALID)
	{
		/* Src */
		SETUP_INSTRUCTION_ARG(1, psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR1], USEASM_REGTYPE_TEMP, 0);
	}
	else
	{
		/* Src */
		SETUP_INSTRUCTION_ARG(1, psProgramContext->aui32InputRegMappings[HW_INPUTREG_COLOUR1], USEASM_REGTYPE_PRIMATTR, 0);
	}

	if(psProgramContext->ui32ExtraBlendState & FFTB_BLENDSTATE_FRONTFACE_CW)
	{
		AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_MOV, (USEASM_PRED_P0<<USEASM_OPFLAGS1_PRED_SHIFT), 0, 0, asArg, 2);
	}
	else
	{
		AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_MOV, (USEASM_PRED_NEGP0<<USEASM_OPFLAGS1_PRED_SHIFT), 0, 0, asArg, 2);
	}
}


/***********************************************************************************
 Function Name      : IntegerCombineColorAndAlpha
 Description        : Uses SOP2 to combine colour and alpha from two source 
					  registers into one destination register.
************************************************************************************/
static IMG_VOID IntegerCombineColorAndAlpha(GLES1Context *gc,
											HWRegType		 eDestType,
											IMG_UINT32		 ui32DestNum,
											HWRegType		 eSrcColType,
											IMG_UINT32		 uSrcColNum,
											HWRegType		 eSrcAlphaType,
											IMG_UINT32		 uSrcAlphaNum,
											ProgramContext	*psProgramContext)
{
	USE_REGISTER asArg[7];
		
	/* 
	   SOP2 does: RGB = CMod1[CSel1] * Src1Mod[Src1.rgb] COP CMod2[CSel2] * Src2.rgb
				  A   = AMod1[ASel1] * ASrc1Mod[Src1.a]  AOP AMod2[ASel2] * Src2.a 

	   To combine colour and alpha we do:

				   RGB = (1 * Src1.rgb) + (0 * Src2.rgb)
				   A   = (0 * Src1.a)   + (1 * Src2.a)
	*/

	/* Dst */
	SETUP_INSTRUCTION_ARG(0, ui32DestNum, aeRegTypeRemap[eDestType], 0);

	/* Src 1 */
	SETUP_INSTRUCTION_ARG(1, uSrcColNum, aeRegTypeRemap[eSrcColType], 0);

	/* Src 2 */
	SETUP_INSTRUCTION_ARG(2, uSrcAlphaNum, aeRegTypeRemap[eSrcAlphaType], 0);

	/* Src1 modifier */
	SETUP_INSTRUCTION_ARG(3, USEASM_INTSRCSEL_NONE, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Src1 colour factor */
	SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_ONE, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Src2 colour factor */
	SETUP_INSTRUCTION_ARG(5, USEASM_INTSRCSEL_ZERO, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Colour operation */
	SETUP_INSTRUCTION_ARG(6, USEASM_INTSRCSEL_ADD, USEASM_REGTYPE_INTSRCSEL, 0);

	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_SOP2, USEASM_OPFLAGS1_MAINISSUE, 0, 0, asArg, 7);


	/* Src1 modifier */
	SETUP_INSTRUCTION_ARG(0, USEASM_INTSRCSEL_NONE, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Src1 alpha factor */
	SETUP_INSTRUCTION_ARG(1, USEASM_INTSRCSEL_ZERO, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Src2 alpha factor */
	SETUP_INSTRUCTION_ARG(2, USEASM_INTSRCSEL_ONE, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Alpha operation */
	SETUP_INSTRUCTION_ARG(3, USEASM_INTSRCSEL_ADD, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Destination modifier */
	SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_NONE, USEASM_REGTYPE_INTSRCSEL, 0);

	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_ASOP2, 0, USEASM_OPFLAGS2_COISSUE, 0, asArg, 5);
}


/***********************************************************************************
 Function Name      : IntegerScaleColorAndAlpha
 Description        : Applies scaling to color and alpha using IMA8.
					  IMA8 MUST be used (not FPMA or SOP2) since we need to use
					  values >1.
************************************************************************************/
static IMG_VOID IntegerScaleColorAndAlpha(GLES1Context *gc,
										  FFTBBlendLayerDesc *psBlendLayer,
										  IMG_UINT32		  ui32DestNum,
										  HWRegType			  eSrcType,
										  IMG_UINT32		  ui32SrcNum,
										  ProgramContext	 *psProgramContext)
{
	IMG_UINT32  ui32ScalingValue, ui32ScalingRegNum;
	USE_REGISTER asArg[10];

	switch(GLES1_COMBINE_GET_COLORSCALE(psBlendLayer->ui32Op))
	{
		case GLES1_COMBINEOP_SCALE_ONE:
		{
			if(GLES1_COMBINE_GET_ALPHASCALE(psBlendLayer->ui32Op) == GLES1_COMBINEOP_SCALE_ONE)
			{
				/* No scaling required, so return */
				return;
			}

			ui32ScalingValue = 0x00010101;

			break;
		}
		case GLES1_COMBINEOP_SCALE_TWO:
		{
			ui32ScalingValue = 0x00020202;

			break;
		}
		case GLES1_COMBINEOP_SCALE_FOUR:
		{
			ui32ScalingValue = 0x00040404;

			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR,"IntegerScaleColorAndAlpha: Invalid colour scale"));

			return;
		}
	}

	switch(GLES1_COMBINE_GET_ALPHASCALE(psBlendLayer->ui32Op))
	{
		case GLES1_COMBINEOP_SCALE_ONE:
		{
			ui32ScalingValue |= 0x01000000;

			break;
		}
		case GLES1_COMBINEOP_SCALE_TWO:
		{
			ui32ScalingValue |= 0x02000000;

			break;
		}
		case GLES1_COMBINEOP_SCALE_FOUR:
		{
			ui32ScalingValue |= 0x04000000;

			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR,"IntegerScaleColorAndAlpha: Invalid alpha scale"));

			return;
		}
	}

	/* Add binding for secondary attribute immediate value */
	ui32ScalingRegNum = AddFFTextureBinding(psProgramContext->psFFTBProgramDesc,
										 FFTB_BINDING_IMMEDIATE_SCALAR,
										 ui32ScalingValue);

	/* Load special constant, if needed */
	LoadFPMASpecialConstant(gc, psProgramContext);


	/* Encode IMA8 to do: CURRENT_RGBA = 0 + CURRENT_RGBA * Scaling */

	/* Dst */
	SETUP_INSTRUCTION_ARG(0, ui32DestNum, USEASM_REGTYPE_TEMP, 0);

	/* Src 0 */
	SETUP_INSTRUCTION_ARG(1, psProgramContext->ui32FPMASpecialConstantRegNum, USEASM_REGTYPE_TEMP, 0);

	/* Src 1 */
	SETUP_INSTRUCTION_ARG(2, ui32SrcNum, aeRegTypeRemap[eSrcType], 0);

	/* Src 2 */
	SETUP_INSTRUCTION_ARG(3, ui32ScalingRegNum, USEASM_REGTYPE_SECATTR, 0);

	/* Colour modifier 0 */
	SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_SRC0, USEASM_REGTYPE_INTSRCSEL, USEASM_ARGFLAGS_COMPLEMENT);

	/* Colour modifier 1 */
	SETUP_INSTRUCTION_ARG(5, USEASM_INTSRCSEL_SRC1, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Colour modifier 2 */
	SETUP_INSTRUCTION_ARG(6, USEASM_INTSRCSEL_SRC2, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Alpha modifier 0 */
	SETUP_INSTRUCTION_ARG(7, USEASM_INTSRCSEL_SRC0ALPHA, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Alpha modifier 1 */
	SETUP_INSTRUCTION_ARG(8, USEASM_INTSRCSEL_SRC1ALPHA, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Alpha modifier 2 */
	SETUP_INSTRUCTION_ARG(9, USEASM_INTSRCSEL_SRC2ALPHA, USEASM_REGTYPE_INTSRCSEL, 0);

	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_IMA8, 0, USEASM_OPFLAGS2_SAT, 0, asArg, 10);
}

/***********************************************************************************
 Function Name      : IntegerADDSIGNEDSOP2
 Description        : Uses SOP2 to perform an ADDSIGNED. Used in the 
					  general ADDSIGNED path
************************************************************************************/
static IMG_VOID IntegerADDSIGNEDSOP2(GLES1Context *gc,
									HWReg			*psDest,
								 	HWReg			*psSources,
								 	ProgramContext	*psProgramContext)
{
	USE_REGISTER asArg[7];

	/* 
	   SOP2 does: RGB = CMod1[CSel1] * Src1Mod[Src1.rgb] COP CMod2[CSel2] * Src2.rgb
				  A   = AMod1[ASel1] * ASrc1Mod[Src1.a]  AOP AMod2[ASel2] * Src2.a 

	   To get ADDSIGNED we do:

				   RGB = 1-[0] * None[Src1.rgb] + 1-[0] * Src2.rgb - 0.5
				   A   = 1-[0] * None[Src1.a]   + 1-[0] * Src2.a - 0.5
	*/

	/* Dst */
	SETUP_INSTRUCTION_ARG(0, psDest->ui32Num, aeRegTypeRemap[psDest->eType], 0);

	/* Src 1 */
	SETUP_INSTRUCTION_ARG(1, psSources[0].ui32Num, aeRegTypeRemap[psSources[0].eType], 0);

	/* Src 2 */
	SETUP_INSTRUCTION_ARG(2, psSources[1].ui32Num, aeRegTypeRemap[psSources[1].eType], 0);

	/* Src1 modifier */
	SETUP_INSTRUCTION_ARG(3, USEASM_INTSRCSEL_NONE, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Src1 colour factor */
	SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_ONE, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Src2 colour factor */
	SETUP_INSTRUCTION_ARG(5, USEASM_INTSRCSEL_ZEROS, USEASM_REGTYPE_INTSRCSEL, USEASM_ARGFLAGS_COMPLEMENT);

	/* Colour operation */
	SETUP_INSTRUCTION_ARG(6, USEASM_INTSRCSEL_ADD, USEASM_REGTYPE_INTSRCSEL, 0);

	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_SOP2, USEASM_OPFLAGS1_MAINISSUE, 0, 0, asArg, 7);


	/* Src1 modifier */
	SETUP_INSTRUCTION_ARG(0, USEASM_INTSRCSEL_NONE, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Src1 alpha factor */
	SETUP_INSTRUCTION_ARG(1, USEASM_INTSRCSEL_ONE, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Src2 alpha factor */
	SETUP_INSTRUCTION_ARG(2, USEASM_INTSRCSEL_ZEROS, USEASM_REGTYPE_INTSRCSEL, USEASM_ARGFLAGS_COMPLEMENT);

	/* Alpha operation */
	SETUP_INSTRUCTION_ARG(3, USEASM_INTSRCSEL_ADD, USEASM_REGTYPE_INTSRCSEL, 0);

	/* Destination modifier */
	SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_NONE, USEASM_REGTYPE_INTSRCSEL, 0);

	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_ASOP2, 0, USEASM_OPFLAGS2_COISSUE, 0, asArg, 5);
}


/***********************************************************************************
 Function Name      : IntegerConvertTextureFormat
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static IMG_VOID IntegerConvertTextureFormat(GLES1Context *gc,
											IMG_UINT32		 ui32SrcLayerNum,
											FFTBBlendLayerDesc	*psBlendLayer,
											ProgramContext	*psProgramContext)
{
	IMG_UINT32 ui32TexInputRegNum = psProgramContext->aui32InputRegMappings[HW_INPUTREG_TEXTURE0 + ui32SrcLayerNum];
	USE_REGISTER asArg[3];

#if !defined(SGX_FEATURE_TAG_LUMINANCE_ALPHA)
	if(psBlendLayer->ui32TexLoadInfo & FFTB_TEXLOAD_LUMINANCE8)
	{
		/* U8 HW texture format, replicated to all channels to give LLLL.
		   Need 1LLL (alpha=1), so OR in 0xFF000000 (0xFF shifted by 24) */

		/* Dst */
		SETUP_INSTRUCTION_ARG(0, ui32TexInputRegNum, USEASM_REGTYPE_PRIMATTR, 0);

		/* Src 1 */
		SETUP_INSTRUCTION_ARG(1, ui32TexInputRegNum, USEASM_REGTYPE_PRIMATTR, 0);

		/* Src 2 */
		SETUP_INSTRUCTION_ARG(2, 0xFF000000, USEASM_REGTYPE_IMMEDIATE, 0);

		AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_OR, 0, 0, 0, asArg, 3);
		return;
	}
	
	if(psBlendLayer->ui32TexLoadInfo & FFTB_TEXLOAD_ALPHA8)
	{
		/* U8 HW texture format, replicated to all channels to give AAAA.
		   Need A000, so AND with 0xFF000000 (0xFF shifted by 24) */
			
		/* Dst */
		SETUP_INSTRUCTION_ARG(0, ui32TexInputRegNum, USEASM_REGTYPE_PRIMATTR, 0);

		/* Src 1 */
		SETUP_INSTRUCTION_ARG(1, ui32TexInputRegNum, USEASM_REGTYPE_PRIMATTR, 0);

		/* Src 2 */
		SETUP_INSTRUCTION_ARG(2, 0xFF000000, USEASM_REGTYPE_IMMEDIATE, 0);

		AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_AND, 0, 0, 0, asArg, 3);

		return;
	}
#endif /* !defined(SGX_FEATURE_TAG_LUMINANCE_ALPHA) */
#if !defined(SGX_FEATURE_TAG_LUMINANCE_ALPHA) || defined(FIX_HW_BRN_29373)
	if(psBlendLayer->ui32TexLoadInfo & FFTB_TEXLOAD_LUMINANCEALPHA8)
	{
#if defined(FIX_HW_BRN_29373)
		/* U8U8 HW texture format, replicate to give ALAL. */
		/* Dst */
		SETUP_INSTRUCTION_ARG(0, ui32TexInputRegNum, USEASM_REGTYPE_PRIMATTR, (0xC<<USEASM_ARGFLAGS_BYTEMSK_SHIFT));

		/* Src 1 */
		SETUP_INSTRUCTION_ARG(1, ui32TexInputRegNum, USEASM_REGTYPE_PRIMATTR, (0 << USEASM_ARGFLAGS_COMP_SHIFT));

		/* Src 2 */
		SETUP_INSTRUCTION_ARG(2, ui32TexInputRegNum, USEASM_REGTYPE_PRIMATTR, (1 << USEASM_ARGFLAGS_COMP_SHIFT));

		AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_UNPCKU8U8, 0, 0, 0, asArg, 3);
#endif

		/* U8U8 HW texture format, replicated to give ALAL.
		   Copy L into second byte to get ALLL. */

		/* Dst */
		SETUP_INSTRUCTION_ARG(0, ui32TexInputRegNum, USEASM_REGTYPE_PRIMATTR, (0x3<<USEASM_ARGFLAGS_BYTEMSK_SHIFT));

		/* Src 1 */
		SETUP_INSTRUCTION_ARG(1, ui32TexInputRegNum, USEASM_REGTYPE_PRIMATTR, 0);

		/* Src 2 */
		SETUP_INSTRUCTION_ARG(2, ui32TexInputRegNum, USEASM_REGTYPE_PRIMATTR, 0);

		AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_UNPCKU8U8, 0, 0, 0, asArg, 3);

		return;
	}
#endif /* !defined(SGX_FEATURE_TAG_LUMINANCE_ALPHA) || defined(FIX_HW_BRN_29373) */
#if defined(GLES1_EXTENSION_TEXTURE_STREAM) || defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
	if(psBlendLayer->ui32TexLoadInfo & (FFTB_TEXLOAD_YUVCSC|FFTB_TEXLOAD_YUVCSC_420_2PLANE|FFTB_TEXLOAD_YUVCSC_420_3PLANE))
	{
		/* Need alpha of 1 */
			
		/* Dst */
		SETUP_INSTRUCTION_ARG(0, ui32TexInputRegNum, USEASM_REGTYPE_PRIMATTR, 0);

		/* Src 1 */
		SETUP_INSTRUCTION_ARG(1, ui32TexInputRegNum, USEASM_REGTYPE_PRIMATTR, 0);

		/* Src 2 */
		SETUP_INSTRUCTION_ARG(2, 0xFF000000, USEASM_REGTYPE_IMMEDIATE, 0);

		AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_OR, 0, 0, 0, asArg, 3);
		
		return;
	}
#endif /* GLES1_EXTENSION_TEXTURE_STREAM || GLES1_EXTENSION_EGL_IMAGE_EXTERNAL */
	/* Other texture formats don't need manipulation */
}

/***********************************************************************************
 Function Name      : CalculateSourceInformation
 Description        : Calculates information about sources used by a texture
					  blend layer, used to determine the best method of encoding
					  the blend.
************************************************************************************/
static IMG_VOID CalculateSourceInformation(GLES1Context		  *gc,
										   ProgramContext	  *psProgramContext,
										   FFTBBlendLayerDesc *psBlendLayerBase,
										   IMG_UINT32		  ui32BlendLayerIndex,
										   IMG_UINT32		  ui32SrcLayerNum,
										   IMG_UINT32		  ui32EnabledLayerNum,
										   SWSourceInfo		  *psSourceInfo)
{
	/* Variables to store the layer info in a convenient form */
	IMG_UINT32 ui32AlphaOp, ui32ColOp;
	FFTBBlendLayerDesc *psBlendLayer;
	IMG_UINT32 i, uTexUnit = 0;
	IMG_UINT32 ui32SourcesUsed;

	psBlendLayer = &psBlendLayerBase[ui32BlendLayerIndex];

	ui32ColOp	 = GLES1_COMBINE_GET_COLOROP(psBlendLayer->ui32Op);
	ui32AlphaOp = GLES1_COMBINE_GET_ALPHAOP(psBlendLayer->ui32Op);

	switch (ui32ColOp)
	{
		case GLES1_COMBINEOP_REPLACE:
		{
			psSourceInfo->ui32NumColorSources = 1;

			break;
		}
		case GLES1_COMBINEOP_MODULATE:
		case GLES1_COMBINEOP_ADD:
		case GLES1_COMBINEOP_ADDSIGNED:
		case GLES1_COMBINEOP_SUBTRACT:
		case GLES1_COMBINEOP_DOT3_RGB:
		case GLES1_COMBINEOP_DOT3_RGBA:
		{
			psSourceInfo->ui32NumColorSources	= 2;

			break;
		}
		case GLES1_COMBINEOP_INTERPOLATE:
		{
			psSourceInfo->ui32NumColorSources	= 3;

			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR,"CalculateSourceInformation(): unsupported mode, shouldn't happen!"));

			psSourceInfo->ui32NumColorSources = 1;
		}
	}
	
	switch (ui32AlphaOp)
	{
		case GLES1_COMBINEOP_REPLACE:
		{
			psSourceInfo->ui32NumAlphaSources = 1;

			break;
		}
		case GLES1_COMBINEOP_MODULATE:
		case GLES1_COMBINEOP_ADD:
		case GLES1_COMBINEOP_ADDSIGNED:
		case GLES1_COMBINEOP_SUBTRACT:
		{
			psSourceInfo->ui32NumAlphaSources = 2;

			break;
		}
		case GLES1_COMBINEOP_INTERPOLATE:
		{
			psSourceInfo->ui32NumAlphaSources = 3;

			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR,"CalculateSourceInformation(): unsupported mode, shouldn't happen!"));

			psSourceInfo->ui32NumAlphaSources = 1;
		}
	}

	for(i=0; i<psSourceInfo->ui32NumColorSources; i++)
	{
		IMG_BOOL bTexture, bAlphaOperand;

		bTexture      = IMG_FALSE;
		bAlphaOperand = IMG_FALSE;

		/* Set up the source register type and number */
		switch(GLES1_COMBINE_GET_COLSRC(psBlendLayer->ui32ColorSrcs, i))
		{
			case GLES1_COMBINECSRC_PRIMARY:
			{
				psSourceInfo->ui32ColorSourcesUsed |= SW_SRC_PRIMARY;

				break;
			}
			case GLES1_COMBINECSRC_PREVIOUS:
			{
				/* On texture layer 0, PREVIOUS becomes PRIMARY */
				if(ui32EnabledLayerNum == 0)
				{
					psSourceInfo->ui32ColorSourcesUsed |= SW_SRC_PRIMARY;
				}
				else
				{
					psSourceInfo->ui32ColorSourcesUsed |= SW_SRC_PREVIOUS;
				}

				break;
			}
			case GLES1_COMBINECSRC_TEXTURE:
			{
#if defined(GLES1_EXTENSION_TEX_ENV_CROSSBAR)
				/* Is a unit number encoded in the source? (for crossbar) */
				if(GLES1_COMBINE_GET_COLCROSSBAR(psBlendLayer->ui32ColorSrcs, i) & GLES1_COMBINECSRC_CROSSBAR)
				{
					uTexUnit = GLES1_COMBINE_GET_COLCROSSBAR(psBlendLayer->ui32ColorSrcs, i) >> GLES1_COMBINECSRC_CROSSBAR_UNIT_SHIFT;
				}
				else
#endif /* defined(GLES1_EXTENSION_TEX_ENV_CROSSBAR) */
				{
					uTexUnit = ui32SrcLayerNum;
				}

				psSourceInfo->ui32ColorSourcesUsed |= SW_SRC_TEXTURE0 << uTexUnit;

				bTexture = IMG_TRUE;

				break;
			}
			case GLES1_COMBINECSRC_CONSTANT:
			{
				psSourceInfo->ui32ColorSourcesUsed |= SW_SRC_CONSTANT;

				break;
			}
			default:
			{
				PVR_DPF((PVR_DBG_ERROR,"CalculateSourceInformation: Invalid colour source"));
				break;
			}
		}

		/* LSB indicates that source should be complemented (1-Src) */
		if(GLES1_COMBINE_GET_COLOPERAND(psBlendLayer->ui32ColorSrcs, i) & GLES1_COMBINECSRC_OPERANDONEMINUS)
		{
			psSourceInfo->bColorUsesComplement = IMG_TRUE;
		}
	
		/* Alpha swizzle */
		if(GLES1_COMBINE_GET_COLOPERAND(psBlendLayer->ui32ColorSrcs, i) & GLES1_COMBINECSRC_OPERANDALPHA)
		{
			psSourceInfo->bColorUsesAlpha = IMG_TRUE;

			bAlphaOperand = IMG_TRUE;
		}

		if(bTexture)
		{
			IMG_UINT32 ui32LayerIndex = 0, j;
			FFTBBlendLayerDesc *psReferencedBlendLayer;

			for(j=0; j<gc->ui32NumImageUnitsActive; j++)
			{
				if(gc->ui32TexImageUnitsEnabled[j] == uTexUnit)
				{
					ui32LayerIndex = j;

					break;
				}
			}

			psReferencedBlendLayer = &psBlendLayerBase[ui32LayerIndex];

			if((bAlphaOperand && ((psReferencedBlendLayer->ui32Unpack & FFTB_TEXUNPACK_ALPHA) != 0)) ||
				(!bAlphaOperand && ((psReferencedBlendLayer->ui32Unpack & FFTB_TEXUNPACK_COLOR) != 0)))
			{
				IntegerConvertTextureFormat(gc, uTexUnit, psReferencedBlendLayer, psProgramContext);

				psReferencedBlendLayer->ui32Unpack = 0;
			}
		}
	}

	/* As above, for alpha */
	for(i=0; i<psSourceInfo->ui32NumAlphaSources; i++)
	{
		IMG_BOOL bTexture;

		bTexture = IMG_FALSE;

		switch(GLES1_COMBINE_GET_ALPHASRC(psBlendLayer->ui32AlphaSrcs, i))
		{
			case GLES1_COMBINEASRC_PRIMARY:
			{
				psSourceInfo->ui32AlphaSourcesUsed |= SW_SRC_PRIMARY;

				break;
			}
			case GLES1_COMBINEASRC_PREVIOUS:
			{
				
				if(ui32EnabledLayerNum == 0)
				{
					psSourceInfo->ui32AlphaSourcesUsed |= SW_SRC_PRIMARY;
				}
				else
				{
					psSourceInfo->ui32AlphaSourcesUsed |= SW_SRC_PREVIOUS;
				}

				break;
			}
			case GLES1_COMBINEASRC_TEXTURE:
			{
#if defined(GLES1_EXTENSION_TEX_ENV_CROSSBAR)
				/* Is a unit number encoded in the source? (for crossbar) */
				if(GLES1_COMBINE_GET_ALPHACROSSBAR(psBlendLayer->ui32AlphaSrcs, i) & GLES1_COMBINEASRC_CROSSBAR)
				{
					uTexUnit = GLES1_COMBINE_GET_ALPHACROSSBAR(psBlendLayer->ui32AlphaSrcs, i) >> GLES1_COMBINEASRC_CROSSBAR_UNIT_SHIFT;
				}
				else
#endif /* defined(GLES1_EXTENSION_TEX_ENV_CROSSBAR) */
				{
					uTexUnit = ui32SrcLayerNum;
				}

				psSourceInfo->ui32AlphaSourcesUsed |= SW_SRC_TEXTURE0 << uTexUnit;

				bTexture = IMG_TRUE;

				break;
			}
			case GLES1_COMBINEASRC_CONSTANT:
			{
				psSourceInfo->ui32AlphaSourcesUsed |= SW_SRC_CONSTANT;

				break;
			}
			default:
			{
				PVR_DPF((PVR_DBG_ERROR,"CalculateSourceInformation: Invalid alpha source"));
				break;
			}
		}
		
		if(GLES1_COMBINE_GET_ALPHAOPERAND(psBlendLayer->ui32AlphaSrcs, i) & GLES1_COMBINEASRC_OPERANDONEMINUS)
		{
			psSourceInfo->bAlphaUsesComplement = IMG_TRUE;
		}

		if(bTexture)
		{
			IMG_UINT32 ui32LayerIndex = 0, j;
			FFTBBlendLayerDesc *psReferencedBlendLayer;

			for(j=0; j<gc->ui32NumImageUnitsActive; j++)
			{
				if(gc->ui32TexImageUnitsEnabled[j] == uTexUnit)
				{
					ui32LayerIndex = j;

					break;
				}
			}

			psReferencedBlendLayer = &psBlendLayerBase[ui32LayerIndex];

			if(psReferencedBlendLayer->ui32Unpack & FFTB_TEXUNPACK_ALPHA)
			{
				IntegerConvertTextureFormat(gc, uTexUnit, psReferencedBlendLayer, psProgramContext);

				psReferencedBlendLayer->ui32Unpack = 0;
			}	
		}
	}

	/* Count the number of different sources used */
	ui32SourcesUsed = psSourceInfo->ui32ColorSourcesUsed;

	while(ui32SourcesUsed)
	{
		if(ui32SourcesUsed & 1)
		{
			psSourceInfo->ui32NumUniqueColorSources++;
		}
		
		ui32SourcesUsed >>= 1;
	}

	ui32SourcesUsed = psSourceInfo->ui32AlphaSourcesUsed;

	while(ui32SourcesUsed)
	{
		if(ui32SourcesUsed & 1)
		{
			psSourceInfo->ui32NumUniqueAlphaSources++;
		}
		
		ui32SourcesUsed >>= 1;
	}

	psSourceInfo->ui32SourcesUsed = (psSourceInfo->ui32ColorSourcesUsed | psSourceInfo->ui32AlphaSourcesUsed);

	ui32SourcesUsed = psSourceInfo->ui32SourcesUsed;

	while(ui32SourcesUsed)
	{
		if(ui32SourcesUsed & 1)
		{
			psSourceInfo->ui32NumUniqueSources++;
		}
		
		ui32SourcesUsed >>= 1;
	}

	/* Check for two subtract operations that use only two unique sources,
	   but use them in different orders. SOP2 cannot handle this */
	if((ui32ColOp == GLES1_COMBINEOP_SUBTRACT) &&
	   (ui32AlphaOp == GLES1_COMBINEOP_SUBTRACT) && 
	   (psSourceInfo->ui32NumUniqueSources == 2))
	{
		for(i=0; i<2; i++)
		{
			if(GLES1_COMBINE_GET_COLSRC(psBlendLayer->ui32ColorSrcs, i) != 
			   GLES1_COMBINE_GET_ALPHASRC(psBlendLayer->ui32AlphaSrcs, i))
			{
				psSourceInfo->bTwoSubtractsDifferentSourceOrder = IMG_TRUE;
			}
		}
	}
}



#if defined(GLES1_EXTENSION_TEXTURE_STREAM) || defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)

/***********************************************************************************
 Function Name      : EncodeYUVColourSpaceConversion
 Description        : Use FIRH to colour space convert XYUV to RGB
************************************************************************************/
static IMG_VOID EncodeYUVColourSpaceConversion(GLES1Context *gc,
												ProgramContext	*psProgramContext,
												IMG_UINT32	 ui32Unit)
{
	USE_REGISTER asArg[9];
	IMG_UINT32 ui32TempReg, ui32RegNum;

	ui32RegNum = psProgramContext->aui32InputRegMappings[HW_INPUTREG_TEXTURE0 + ui32Unit];

	ui32TempReg = AllocateTempReg(psProgramContext);

	/* Don't try to work out if previous instructions can take nosched, just insert NOP.nosched */
	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_PADDING, USEASM_OPFLAGS1_NOSCHED, 0, 0, IMG_NULL, 0);

	/* Second NOP guarantees that noscheds work even if previous instructions are optimised out */
	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_PADDING, USEASM_OPFLAGS1_NOSCHED, 0, 0, IMG_NULL, 0);

	/*
	 * firh.skipinv(.nosched) rY, paX, paX, paX, u8, msc, fcs0, #3, #2 
	 * where Y is ui32TempReg and X is ui32SrcNum.
	 */
	SETUP_INSTRUCTION_ARG(0, ui32TempReg, USEASM_REGTYPE_TEMP, 0);
	SETUP_INSTRUCTION_ARG(1, ui32RegNum, USEASM_REGTYPE_PRIMATTR, 0);
	SETUP_INSTRUCTION_ARG(2, ui32RegNum, USEASM_REGTYPE_PRIMATTR, 0);
	SETUP_INSTRUCTION_ARG(3, ui32RegNum, USEASM_REGTYPE_PRIMATTR, 0);
	SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_U8, USEASM_REGTYPE_INTSRCSEL, 0);
	SETUP_INSTRUCTION_ARG(5, EURASIA_USE1_FIRH_EDGEMODE_MIRRORSINGLE, USEASM_REGTYPE_IMMEDIATE, 0);
	SETUP_INSTRUCTION_ARG(6, 0, USEASM_REGTYPE_FILTERCOEFF, 0);
	SETUP_INSTRUCTION_ARG(7, 3, USEASM_REGTYPE_IMMEDIATE, 0);
	SETUP_INSTRUCTION_ARG(8, 2, USEASM_REGTYPE_IMMEDIATE, 0);

#if defined(SGX_FEATURE_USE_NO_INSTRUCTION_PAIRING)
	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_FIRH, USEASM_OPFLAGS1_NOSCHED, 0, 0, asArg, 9);
#else
	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_FIRH, 0, 0, 0, asArg, 9);
#endif

	/*
	 * firh.skipinv rY, paX, paX, paX, u8, msc, fcs1, #3, #1 
	 * where Y is ui32TempReg and X is ui32SrcNum.
	 */
	SETUP_INSTRUCTION_ARG(0, ui32TempReg, USEASM_REGTYPE_TEMP, 0);
	SETUP_INSTRUCTION_ARG(1, ui32RegNum, USEASM_REGTYPE_PRIMATTR, 0);
	SETUP_INSTRUCTION_ARG(2, ui32RegNum, USEASM_REGTYPE_PRIMATTR, 0);
	SETUP_INSTRUCTION_ARG(3, ui32RegNum, USEASM_REGTYPE_PRIMATTR, 0);
	SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_U8, USEASM_REGTYPE_INTSRCSEL, 0);
	SETUP_INSTRUCTION_ARG(5, EURASIA_USE1_FIRH_EDGEMODE_MIRRORSINGLE, USEASM_REGTYPE_IMMEDIATE, 0);
	SETUP_INSTRUCTION_ARG(6, 1, USEASM_REGTYPE_FILTERCOEFF, 0);
	SETUP_INSTRUCTION_ARG(7, 3, USEASM_REGTYPE_IMMEDIATE, 0);
	SETUP_INSTRUCTION_ARG(8, 1, USEASM_REGTYPE_IMMEDIATE, 0);

	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_FIRH, 0, 0, 0, asArg, 9);


	/*
	 * firh.skipinv rY, paX, paX, paX, u8, msc, fcs2, #5, #0 
	 * where Y is ui32TempReg and X is ui32SrcNum.
	 */
	GLES1_ASSERT(psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_TEXTURE0 + ui32Unit]==HW_TEMPREG_INVALID);

	psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_TEXTURE0 + ui32Unit] = AllocateTempReg(psProgramContext);

	SETUP_INSTRUCTION_ARG(0, psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_TEXTURE0 + ui32Unit], USEASM_REGTYPE_TEMP, 0);

	SETUP_INSTRUCTION_ARG(1, ui32RegNum, USEASM_REGTYPE_PRIMATTR, 0);
	SETUP_INSTRUCTION_ARG(2, ui32RegNum, USEASM_REGTYPE_PRIMATTR, 0);
	SETUP_INSTRUCTION_ARG(3, ui32RegNum, USEASM_REGTYPE_PRIMATTR, 0);
	SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_U8, USEASM_REGTYPE_INTSRCSEL, 0);
	SETUP_INSTRUCTION_ARG(5, EURASIA_USE1_FIRH_EDGEMODE_MIRRORSINGLE, USEASM_REGTYPE_IMMEDIATE, 0);
	SETUP_INSTRUCTION_ARG(6, 2, USEASM_REGTYPE_FILTERCOEFF, 0);
	SETUP_INSTRUCTION_ARG(7, 5, USEASM_REGTYPE_IMMEDIATE, 0);
	SETUP_INSTRUCTION_ARG(8, 0, USEASM_REGTYPE_IMMEDIATE, 0);

	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_FIRH, 0, 0, 0, asArg, 9);

	DeallocateTempReg(psProgramContext, ui32TempReg);
}


/***********************************************************************************
 Function Name      : EncodeYUV420ColourSpaceConversion
 Description        : Use FIRH to colour space convert Y, VU to RGB
************************************************************************************/
static IMG_VOID EncodeYUV420ColourSpaceConversion(GLES1Context *gc,
												ProgramContext	*psProgramContext,
												IMG_UINT32	 ui32Unit,
												IMG_BOOL b2Planar)
{
	IMG_UINT32 ui32TempReg;
	USE_REGISTER asArg[9];
	IMG_UINT32 ui32YUVReg0, ui32YUVReg1, ui32YUVReg2;
	IMG_UINT32 ui32FilterOffset;

	ui32TempReg = AllocateTempReg(psProgramContext);

	/* Don't try to work out if previous instructions can take nosched, just insert NOP.nosched */
	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_PADDING, USEASM_OPFLAGS1_NOSCHED, 0, 0, IMG_NULL, 0);

	/* Second NOP guarantees that noscheds work even if previous instructions are optimised out */
	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_PADDING, USEASM_OPFLAGS1_NOSCHED, 0, 0, IMG_NULL, 0);

	if(b2Planar)
	{
		ui32YUVReg0 = psProgramContext->aui32InputRegMappings[HW_INPUTREG_TEXTURE0 + ui32Unit] + 1; /* UV plane */
		ui32YUVReg1 = psProgramContext->aui32InputRegMappings[HW_INPUTREG_TEXTURE0 + ui32Unit] + 1;	/* UV plane */ 
		ui32YUVReg2 = psProgramContext->aui32InputRegMappings[HW_INPUTREG_TEXTURE0 + ui32Unit];		/* Y Plane */
		ui32FilterOffset = 7;
	}
	else
	{
		ui32YUVReg0 = psProgramContext->aui32InputRegMappings[HW_INPUTREG_TEXTURE0 + ui32Unit] + 2;	/* U Plane */
		ui32YUVReg1 = psProgramContext->aui32InputRegMappings[HW_INPUTREG_TEXTURE0 + ui32Unit] + 1;	/* V Plane */ 
		ui32YUVReg2 = psProgramContext->aui32InputRegMappings[HW_INPUTREG_TEXTURE0 + ui32Unit];		/* Y Plane */
		ui32FilterOffset = 6;
	}

	/*
	 * firh.skipinv(.nosched) rY, ui32YUVReg0, ui32YUVReg1, ui32YUVReg2, u8, msc, fcs0, #ui32FilterOffset, #2 
	 * where Y is ui32TempReg and X is ui32SrcNum.
	 */
	SETUP_INSTRUCTION_ARG(0, ui32TempReg, USEASM_REGTYPE_TEMP, 0);
	SETUP_INSTRUCTION_ARG(1, ui32YUVReg0, USEASM_REGTYPE_PRIMATTR, 0);
	SETUP_INSTRUCTION_ARG(2, ui32YUVReg1, USEASM_REGTYPE_PRIMATTR, 0);
	SETUP_INSTRUCTION_ARG(3, ui32YUVReg2, USEASM_REGTYPE_PRIMATTR, 0);
	SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_U8, USEASM_REGTYPE_INTSRCSEL, 0);
	SETUP_INSTRUCTION_ARG(5, EURASIA_USE1_FIRH_EDGEMODE_MIRRORSINGLE, USEASM_REGTYPE_IMMEDIATE, 0);
	SETUP_INSTRUCTION_ARG(6, 0, USEASM_REGTYPE_FILTERCOEFF, 0);
	SETUP_INSTRUCTION_ARG(7, ui32FilterOffset, USEASM_REGTYPE_IMMEDIATE, 0);
	SETUP_INSTRUCTION_ARG(8, 2, USEASM_REGTYPE_IMMEDIATE, 0);

#if defined(SGX_FEATURE_USE_NO_INSTRUCTION_PAIRING)
	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_FIRH, USEASM_OPFLAGS1_NOSCHED, 0, 0, asArg, 9);
#else
	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_FIRH, 0, 0, 0, asArg, 9);
#endif

	/*
	 * firh.skipinv rY, ui32YUVReg0, ui32YUVReg1, ui32YUVReg2, u8, msc, fcs1, #ui32FilterOffset, #1 
	 * where Y is ui32TempReg and X is ui32SrcNum.
	 */
	SETUP_INSTRUCTION_ARG(0, ui32TempReg, USEASM_REGTYPE_TEMP, 0);
	SETUP_INSTRUCTION_ARG(1, ui32YUVReg0, USEASM_REGTYPE_PRIMATTR, 0);
	SETUP_INSTRUCTION_ARG(2, ui32YUVReg1, USEASM_REGTYPE_PRIMATTR, 0);
	SETUP_INSTRUCTION_ARG(3, ui32YUVReg2, USEASM_REGTYPE_PRIMATTR, 0);
	SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_U8, USEASM_REGTYPE_INTSRCSEL, 0);
	SETUP_INSTRUCTION_ARG(5, EURASIA_USE1_FIRH_EDGEMODE_MIRRORSINGLE, USEASM_REGTYPE_IMMEDIATE, 0);
	SETUP_INSTRUCTION_ARG(6, 1, USEASM_REGTYPE_FILTERCOEFF, 0);
	SETUP_INSTRUCTION_ARG(7, ui32FilterOffset, USEASM_REGTYPE_IMMEDIATE, 0);
	SETUP_INSTRUCTION_ARG(8, 1, USEASM_REGTYPE_IMMEDIATE, 0);

	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_FIRH, 0, 0, 0, asArg, 9);


	/*
	 * firh.skipinv rY, ui32YUVReg0, ui32YUVReg1, ui32YUVReg2, u8, msc, fcs2, #ui32FilterOffset, #0 
	 * where Y is ui32TempReg and X is ui32SrcNum.
	 */
	GLES1_ASSERT(psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_TEXTURE0 + ui32Unit]==HW_TEMPREG_INVALID);

	psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_TEXTURE0 + ui32Unit] = AllocateTempReg(psProgramContext);

	SETUP_INSTRUCTION_ARG(0, psProgramContext->aui32InputsPlacedInTemps[HW_INPUTREG_TEXTURE0 + ui32Unit], USEASM_REGTYPE_TEMP, 0);

	SETUP_INSTRUCTION_ARG(1, ui32YUVReg0, USEASM_REGTYPE_PRIMATTR, 0);
	SETUP_INSTRUCTION_ARG(2, ui32YUVReg1, USEASM_REGTYPE_PRIMATTR, 0);
	SETUP_INSTRUCTION_ARG(3, ui32YUVReg2, USEASM_REGTYPE_PRIMATTR, 0);
	SETUP_INSTRUCTION_ARG(4, USEASM_INTSRCSEL_U8, USEASM_REGTYPE_INTSRCSEL, 0);
	SETUP_INSTRUCTION_ARG(5, EURASIA_USE1_FIRH_EDGEMODE_MIRRORSINGLE, USEASM_REGTYPE_IMMEDIATE, 0);
	SETUP_INSTRUCTION_ARG(6, 2, USEASM_REGTYPE_FILTERCOEFF, 0);
	SETUP_INSTRUCTION_ARG(7, ui32FilterOffset, USEASM_REGTYPE_IMMEDIATE, 0);
	SETUP_INSTRUCTION_ARG(8, 0, USEASM_REGTYPE_IMMEDIATE, 0);

	AddInstruction(gc, &psProgramContext->sUSEASMInfo, USEASM_OP_FIRH, 0, 0, 0, asArg, 9);

	DeallocateTempReg(psProgramContext, ui32TempReg);
}

#endif /* GLES1_EXTENSION_TEXTURE_STREAM || GLES1_EXTENSION_EGL_IMAGE_EXTERNAL */


/***********************************************************************************
 Function Name      : PickRestrictedBlend
 Inputs             : sSourceInfo, ui32ColOp, ui32AlphaOp
 Outputs            : True/False
 Returns            : 
 Description        : 
************************************************************************************/
static IMG_BOOL PickRestrictedBlend(SWSourceInfo *psSourceInfo, IMG_UINT32 ui32ColOp, IMG_UINT32 ui32AlphaOp)
{
	if((psSourceInfo->ui32NumUniqueSources <= 2) &&
	   !psSourceInfo->bColorUsesComplement && 
	   !psSourceInfo->bAlphaUsesComplement && 
	   !psSourceInfo->bColorUsesAlpha &&
	   !psSourceInfo->bTwoSubtractsDifferentSourceOrder &&
	   (ui32ColOp != GLES1_COMBINEOP_DOT3_RGBA) &&
	   (ui32ColOp != GLES1_COMBINEOP_DOT3_RGB)  &&
	   (ui32ColOp != GLES1_COMBINEOP_INTERPOLATE) && 
	   (ui32AlphaOp != GLES1_COMBINEOP_INTERPOLATE) &&
/*
	FUTURE - OPTIMISATION
	---------------------
	This path can handle a few ADDSIGNED variants, but not all.  E.g. ADDSIGNED for both colour and alpha both, using
	the same two sources, is possible, but it can't handle things like if colour and alpha have 1 unique source each.
	The restricted blend path produces less instructions than the general ADDSIGNED path so it should be used whenever
	possible, rather than punting all ADDSIGNED blends into the ADDSIGNED path as happens with the test conditions above
*/
	   (ui32ColOp != GLES1_COMBINEOP_ADDSIGNED) && 
	   (ui32AlphaOp != GLES1_COMBINEOP_ADDSIGNED) &&
/*
	FUTURE - OPTIMISATION
	---------------------
	This path currently can't handle 2 source operations with 1 unique source, e.g. SUBTRACT TEX, TEX
*/
	   !(psSourceInfo->ui32NumUniqueColorSources==1 && psSourceInfo->ui32NumColorSources>1) &&
	   !(psSourceInfo->ui32NumUniqueAlphaSources==1 && psSourceInfo->ui32NumAlphaSources>1)
	   )
	{
		return IMG_TRUE;
	}

	return IMG_FALSE;
}


/***********************************************************************************
 Function Name      : IntegerSetupTextureBlend
 Description        : 
************************************************************************************/
static IMG_VOID IntegerSetupTextureBlend(GLES1Context *gc,
									IMG_UINT32			 ui32SrcLayerNum, 
									 IMG_UINT32			 ui32EnabledLayerNum,
									 FFTBBlendLayerDesc	*psBlendLayerBase,
									 IMG_UINT32 		ui32BlendLayerIndex,
									 ProgramContext		*psProgramContext)
{
    SWSourceInfo	sSourceInfo = {0};
	IMG_BOOL		bCombineColorAndAlpha;
	IMG_UINT32		ui32AlphaOp, ui32ColOp;
	FFTBBlendLayerDesc	*psBlendLayer;

	psBlendLayer = &psBlendLayerBase[ui32BlendLayerIndex];

	ui32ColOp   = GLES1_COMBINE_GET_COLOROP(psBlendLayer->ui32Op);
	ui32AlphaOp = GLES1_COMBINE_GET_ALPHAOP(psBlendLayer->ui32Op);

	/* Calculate information on the sources used by this blend layer */
	CalculateSourceInformation(gc,
							   psProgramContext,
							   psBlendLayerBase,
							   ui32BlendLayerIndex,
							   ui32SrcLayerNum,
							   ui32EnabledLayerNum,
							   &sSourceInfo);

	/* Make a decision on how to implement this blend layer based
	   on information about the sources and operations */
	if(PickRestrictedBlend(&sSourceInfo, ui32ColOp, ui32AlphaOp))
	{
		/* SOP2 path is restricted in its capabilities, but can encode
		   simple colour and alpha blends in one instruction */	
		EncodeRestrictedBlend(gc,
							  psProgramContext->ui32CurrentRGBARegNum,
							  ui32SrcLayerNum,
							  ui32EnabledLayerNum,
							  psBlendLayer,
							  &sSourceInfo,
							  psProgramContext);

		bCombineColorAndAlpha = IMG_FALSE;
	}
	else if((ui32ColOp == GLES1_COMBINEOP_ADDSIGNED) ||
			(ui32AlphaOp == GLES1_COMBINEOP_ADDSIGNED))
	{
		/* ADDSIGNED can ONLY be implemented using SOP2, but this instruction 
		   has limitations. This path handles those limitations when an ADDSIGNED
		   operation is used. */
		if(psProgramContext->ui32IntermediateRGBRegNum == HW_TEMPREG_INVALID)
		{
			psProgramContext->ui32IntermediateRGBRegNum = AllocateTempReg(psProgramContext);
		}

		if(psProgramContext->ui32IntermediateAlphaRegNum == HW_TEMPREG_INVALID)
		{
			psProgramContext->ui32IntermediateAlphaRegNum = AllocateTempReg(psProgramContext);
		}

		EncodeGeneralADDSIGNEDBlend(gc,
									ui32SrcLayerNum,
									ui32EnabledLayerNum,
									psBlendLayer,
									psProgramContext);

		if(ui32ColOp == GLES1_COMBINEOP_DOT3_RGBA)
		{
			bCombineColorAndAlpha = IMG_FALSE;
		}
		else
		{
			bCombineColorAndAlpha = IMG_TRUE;
		}
	}
	else
	{
		/* FPMA path is more capable, since it can support more than two sources,
		   complement etc., but requires two instructions per blend, plus one extra
		   instruction to pack the two output values (RGB and A) into a single 
		   register. */
		
		if(psProgramContext->ui32IntermediateRGBRegNum == HW_TEMPREG_INVALID)
		{
			psProgramContext->ui32IntermediateRGBRegNum = AllocateTempReg(psProgramContext);
		}

		EncodeGeneralColorBlend(gc,
								ui32SrcLayerNum,
							    ui32EnabledLayerNum,
							    psBlendLayer,
							    psProgramContext);

		/* DOT3_RGBA writes to the alpha, so don't do another alpha op */
		if((ui32ColOp == GLES1_COMBINEOP_DOT3_RGBA))
		{
			/* DOT3 RGBA has written current RGBA (including alpha channel), 
			   so don't combine colour and alpha from intermediate registers */
			bCombineColorAndAlpha = IMG_FALSE;
		}
		else
		{
			if(psProgramContext->ui32IntermediateAlphaRegNum == HW_TEMPREG_INVALID)
			{
				psProgramContext->ui32IntermediateAlphaRegNum = AllocateTempReg(psProgramContext);
			}

			EncodeGeneralAlphaBlend(gc,
									ui32SrcLayerNum,
									ui32EnabledLayerNum,
									psBlendLayer,
									psProgramContext);

			bCombineColorAndAlpha = IMG_TRUE;
		}
	}

	if(bCombineColorAndAlpha)
	{
		/* Combine the two separate registers that contain colour 
		   and alpha into one register */
		IntegerCombineColorAndAlpha(gc,
									HW_REGTYPE_TEMP,
									psProgramContext->ui32CurrentRGBARegNum,
									HW_REGTYPE_TEMP,
									psProgramContext->ui32IntermediateRGBRegNum,
									HW_REGTYPE_TEMP,
									psProgramContext->ui32IntermediateAlphaRegNum,
									psProgramContext);
	}

	/* Apply and required scaling */
	IntegerScaleColorAndAlpha(gc,
							  psBlendLayer,
							  psProgramContext->ui32CurrentRGBARegNum,
							  HW_REGTYPE_TEMP,
							  psProgramContext->ui32CurrentRGBARegNum,
							  psProgramContext);
}




/***********************************************************************************
 Function Name      : CreateIntegerFragmentProgramFromFF
 Description        : 
 Outputs			: 'nFragmentProgramName' in psFFTBProgramDesc is assigned the
					  name of the created SGL program
************************************************************************************/
IMG_INTERNAL IMG_BOOL CreateIntegerFragmentProgramFromFF(GLES1Context			*gc, 
														 FFTBBlendLayerDesc		*psBlendLayers,
														 IMG_BOOL				 bAnyTexturingEnabled,
														 IMG_BOOL				*pbTexturingEnabled, 
														 IMG_UINT32				ui32ExtraBlendState,
														 FFTBProgramDesc		*psFFTBProgramDesc)
{
	FFGEN_PROGRAM_DETAILS *psFFGENProgramDetails;
	ProgramContext	sProgramContext;
	GLES1_TEXTURE_LOAD *psTexLoad;
	IMG_UINT32		ui32TexLoadIndex = 0;
	IMG_UINT32		ui32InputReg = 0;
	IMG_UINT32		i, ui32SecAttribOffset, ui32EnabledUnitNum = 0;

	/* Initialise some values in the program context */
	sProgramContext.ui32TempRegsUsed = 0;
	sProgramContext.ui32NumTempRegsUsed = 0;

	/* Result will be stored in ui32CurrentRGBARegNum, and we expect the result
	   in Temp0, so allocate a register immediately */
	sProgramContext.ui32CurrentRGBARegNum			= AllocateTempReg(&sProgramContext);
	sProgramContext.ui32IntermediateRGBRegNum		= HW_TEMPREG_INVALID;
	sProgramContext.ui32IntermediateAlphaRegNum		= HW_TEMPREG_INVALID;
	sProgramContext.ui32FPMASpecialConstantRegNum	= HW_TEMPREG_INVALID;

	for(i=0;i<HW_INPUTREG_MAX;i++)
	{
		sProgramContext.aui32InputsPlacedInTemps[i] = HW_TEMPREG_INVALID;
	}

	sProgramContext.ui32ExtraBlendState  = ui32ExtraBlendState;
	sProgramContext.psFFTBProgramDesc = psFFTBProgramDesc;

	psFFGENProgramDetails = GLES1Calloc(gc, sizeof(FFGEN_PROGRAM_DETAILS));

	if(!psFFGENProgramDetails)
	{
		PVR_DPF((PVR_DBG_ERROR,"CreateIntegerFragmentProgramFromFF(): Failed to allocate memory for psFFGENProgramDetails"));
		return IMG_FALSE;
	}

	/* USEASM instructions  (USEASM input) */
	sProgramContext.sUSEASMInfo.ui32NumMainUSEASMInstructions = 0;
	sProgramContext.sUSEASMInfo.psFirstUSEASMInstruction      = IMG_NULL;
	sProgramContext.sUSEASMInfo.psLastUSEASMInstruction       = IMG_NULL;

	/* HW instructions  (USEASM output) */
	sProgramContext.sUSEASMInfo.ui32NumHWInstructions = 0;
	sProgramContext.sUSEASMInfo.pui32HWInstructions   = IMG_NULL;


	psFFTBProgramDesc->ui32NumBindings = 0;

	ui32SecAttribOffset = 0;

	/* Fog colour */
	if(ui32ExtraBlendState & FFTB_BLENDSTATE_FOG)
	{
		ui32SecAttribOffset++;
	}

	/* Alpha test*/
	if(ui32ExtraBlendState & FFTB_BLENDSTATE_ALPHATEST)
	{
		ui32SecAttribOffset += GLES1_FRAGMENT_SECATTR_NUM_ALPHATEST;

#if defined(SGX_FEATURE_ALPHATEST_SECONDARY_PERPRIMITIVE)
		if(ui32ExtraBlendState & FFTB_BLENDSTATE_ALPHATEST_FEEDBACK)
		{
			ui32SecAttribOffset += GLES1_FRAGMENT_SECATTR_ALPHATEST_OFFSET;
		}
#endif
	}

	psFFTBProgramDesc->ui32CurrentConstantOffset = ui32SecAttribOffset;

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	InsertPHAS(gc, &sProgramContext);
#endif

#if defined(FIX_HW_BRN_25211)
	if(gc->sAppHints.bUseC10Colours)
	{
		if(ui32ExtraBlendState & FFTB_BLENDSTATE_NEEDS_BASE_COLOUR)
		{
			/* Iterate the base colour  */
			psTexLoad = &psFFTBProgramDesc->psNonDependentTextureLoads[ui32TexLoadIndex++];
			psTexLoad->ui32Texture = GLES1_TEXTURE_NONE;
			psTexLoad->eCoordinate = gc->ui32NumImageUnitsActive;
			psTexLoad->ui32CoordinateDimension = 4;
			psTexLoad->eFormat = GLES1_TEXLOAD_FORMAT_C10;

			sProgramContext.aui32InputRegMappings[HW_INPUTREG_COLOUR0] = ui32InputReg;

			ui32InputReg += 2;

			/* Unpack base colour */
			SetupBaseColour(gc, &sProgramContext);
		}

		if(ui32ExtraBlendState & FFTB_BLENDSTATE_TWOSIDED_LIGHTING)
		{
			/* Iterate back face  */
			psTexLoad = &psFFTBProgramDesc->psNonDependentTextureLoads[ui32TexLoadIndex++];
			psTexLoad->ui32Texture = GLES1_TEXTURE_NONE;
			psTexLoad->eCoordinate = gc->ui32NumImageUnitsActive + 1;
			psTexLoad->ui32CoordinateDimension = 4;
			psTexLoad->eFormat = GLES1_TEXLOAD_FORMAT_C10;

			sProgramContext.aui32InputRegMappings[HW_INPUTREG_COLOUR1] = ui32InputReg;

			ui32InputReg += 2;

			/* Overwrite the colour0 with the back face colour */
			SetupTwoSidedColour(gc, &sProgramContext);
		}
	}
	else
#endif /* defined(FIX_HW_BRN_25211) */
	{
		if(ui32ExtraBlendState & FFTB_BLENDSTATE_NEEDS_BASE_COLOUR)
		{
			/* Iterate the base colour  */
			psTexLoad = &psFFTBProgramDesc->psNonDependentTextureLoads[ui32TexLoadIndex++];
			psTexLoad->ui32Texture = GLES1_TEXTURE_NONE;
			psTexLoad->eCoordinate = GLES1_TEXTURE_COORDINATE_V0;
			psTexLoad->ui32CoordinateDimension = 4;
			psTexLoad->eFormat = GLES1_TEXLOAD_FORMAT_U8;

			sProgramContext.aui32InputRegMappings[HW_INPUTREG_COLOUR0] = ui32InputReg++;
		}

		if(ui32ExtraBlendState & FFTB_BLENDSTATE_TWOSIDED_LIGHTING)
		{
			/* Iterate back face colour  */
			psTexLoad = &psFFTBProgramDesc->psNonDependentTextureLoads[ui32TexLoadIndex++];
			psTexLoad->ui32Texture = GLES1_TEXTURE_NONE;
			psTexLoad->eCoordinate = GLES1_TEXTURE_COORDINATE_V1;
			psTexLoad->ui32CoordinateDimension = 4;
			psTexLoad->eFormat = GLES1_TEXLOAD_FORMAT_U8;

			sProgramContext.aui32InputRegMappings[HW_INPUTREG_COLOUR1] = ui32InputReg++;

			/* Overwrite the colour0 with the back face colour */
			SetupTwoSidedColour(gc, &sProgramContext);
		}
	}

	if(bAnyTexturingEnabled && gc->ui32NumImageUnitsActive)
	{
		/* Textured blend state */
		for(i=0; i<gc->ui32NumImageUnitsActive; i++)
		{
			IMG_UINT32 unit = gc->ui32TexImageUnitsEnabled[i];

			if(((psBlendLayers[i].ui32TexLoadInfo & FFTB_TEXLOAD_NEEDTEX) != 0) || ((ui32ExtraBlendState & FFTB_BLENDSTATE_NEEDS_ALL_LAYERS) != 0))
			{
				psTexLoad = &psFFTBProgramDesc->psNonDependentTextureLoads[ui32TexLoadIndex++];
				psTexLoad->ui32Texture = i;

				psTexLoad->eCoordinate = (IMG_UINT32)i;
				psTexLoad->eFormat = GLES1_TEXLOAD_FORMAT_F32;

				sProgramContext.aui32InputRegMappings[HW_INPUTREG_TEXTURE0 + unit] = ui32InputReg++;

				if(psBlendLayers[i].ui32TexLoadInfo & FFTB_TEXLOAD_2D)
				{
					psTexLoad->ui32CoordinateDimension = 2;
				}
				else if(psBlendLayers[i].ui32TexLoadInfo & FFTB_TEXLOAD_CEM)
				{
					psTexLoad->ui32CoordinateDimension = 3;
				}

				if(psBlendLayers[i].ui32TexLoadInfo & FFTB_TEXLOAD_PROJECTED)
				{
					psTexLoad->bProjected = IMG_TRUE;
				}
				else
				{
					psTexLoad->bProjected = IMG_FALSE;
				}

#if defined(GLES1_EXTENSION_TEXTURE_STREAM) || defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)

				if(psBlendLayers[i].ui32TexLoadInfo & FFTB_TEXLOAD_YUVCSC)
				{
					/* HW returns XYUV */
					EncodeYUVColourSpaceConversion(gc, &sProgramContext, unit);
				}
				else if(psBlendLayers[i].ui32TexLoadInfo & FFTB_TEXLOAD_YUVCSC_420_2PLANE)
				{
					GLES1_TEXTURE_LOAD *psYUVTexLoad = &psFFTBProgramDesc->psNonDependentTextureLoads[ui32TexLoadIndex++];

					/* Copy plane 0 load instructions */
					*psYUVTexLoad = *psTexLoad;

					/* Extra planes use image units interleaved with normal texture units */
					psYUVTexLoad->ui32Texture++;
					
					sProgramContext.aui32InputRegMappings[HW_INPUTREG_TEXTURE0_PLANE1 + unit] = ui32InputReg++;

					/* HW returns Y8, VU88 */
					EncodeYUV420ColourSpaceConversion(gc, &sProgramContext, unit, IMG_TRUE);
				}
				else if(psBlendLayers[i].ui32TexLoadInfo & FFTB_TEXLOAD_YUVCSC_420_3PLANE)
				{
					GLES1_TEXTURE_LOAD *psYUVTexLoad = &psFFTBProgramDesc->psNonDependentTextureLoads[ui32TexLoadIndex++];

					/* Copy plane 0 load instructions */
					*psYUVTexLoad = *psTexLoad;

					/* Extra planes use image units interleaved with normal texture units */
					psYUVTexLoad->ui32Texture++;
					
					sProgramContext.aui32InputRegMappings[HW_INPUTREG_TEXTURE0_PLANE1 + unit] = ui32InputReg++;

					psYUVTexLoad = &psFFTBProgramDesc->psNonDependentTextureLoads[ui32TexLoadIndex++];

					/* Copy plane 0 load instructions */
					*psYUVTexLoad = *psTexLoad;

					/* Extra planes use image units interleaved with normal texture units */
					psYUVTexLoad->ui32Texture+=2;
					
					sProgramContext.aui32InputRegMappings[HW_INPUTREG_TEXTURE0_PLANE2 + unit] = ui32InputReg++;

					/* HW returns Y8, V8, U8 */
					EncodeYUV420ColourSpaceConversion(gc, &sProgramContext, unit, IMG_FALSE);
				}
#endif /* GLES1_EXTENSION_TEXTURE_STREAM || GLES1_EXTENSION_EGL_IMAGE_EXTERNAL */
			}
		}

		for(i=0; i<gc->ui32NumImageUnitsActive; i++)
		{
			IMG_UINT32 unit = gc->ui32TexImageUnitsEnabled[i];
			
			if(pbTexturingEnabled[i])
			{
				IntegerSetupTextureBlend(gc,
										 unit,
										 ui32EnabledUnitNum,
										 psBlendLayers,
										 i,
										 &sProgramContext);

				ui32EnabledUnitNum++;
			}
		}
	}
	else
	{
		/* Non-textured blend state, copy colour to Temp0 */
		if(sProgramContext.aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR0]!=HW_TEMPREG_INVALID)
		{
			IntegerCopy(gc,
						&sProgramContext,
						HW_REGTYPE_TEMP,
						HW_OUTPUTREG_COLOUR,
						HW_REGTYPE_TEMP,
						sProgramContext.aui32InputsPlacedInTemps[HW_INPUTREG_COLOUR0]);
		}
		else
		{
			IntegerCopy(gc,
						&sProgramContext,
						HW_REGTYPE_TEMP,
						HW_OUTPUTREG_COLOUR,
						HW_REGTYPE_PRIMATTR,
						sProgramContext.aui32InputRegMappings[HW_INPUTREG_COLOUR0]);
		}
	}


	psFFTBProgramDesc->sUSEASMInfo = sProgramContext.sUSEASMInfo;

	psFFGENProgramDetails->pui32Instructions 			= sProgramContext.sUSEASMInfo.pui32HWInstructions;
	psFFGENProgramDetails->ui32InstructionCount			= sProgramContext.sUSEASMInfo.ui32NumHWInstructions;
	psFFGENProgramDetails->ui32TemporaryRegisterCount	= sProgramContext.ui32NumTempRegsUsed;
	psFFGENProgramDetails->ui32PrimaryAttributeCount	= ui32TexLoadIndex;
	psFFGENProgramDetails->bUSEPerInstanceMode			= IMG_FALSE;

	psFFTBProgramDesc->ui32NumNonDependentTextureLoads		= ui32TexLoadIndex;

#if defined(FIX_HW_BRN_25211)
	if(gc->sAppHints.bUseC10Colours)
	{
		if(ui32ExtraBlendState & FFTB_BLENDSTATE_NEEDS_BASE_COLOUR)
		{
			/* C10 uses 2 PAs, so we need to increase by 1 */
			psFFGENProgramDetails->ui32PrimaryAttributeCount++;
		}

		if(ui32ExtraBlendState & FFTB_BLENDSTATE_TWOSIDED_LIGHTING)
		{
			/* C10 uses 2 PAs, so we need to increase by 1 */
			psFFGENProgramDetails->ui32PrimaryAttributeCount++;
		}
	}
#endif /* defined(FIX_HW_BRN_25211) */

	/* Setup constants */
	psFFGENProgramDetails->ui32SecondaryAttributeCount = sProgramContext.psFFTBProgramDesc->ui32CurrentConstantOffset-ui32SecAttribOffset;

	psFFTBProgramDesc->psFFGENProgramDetails = psFFGENProgramDetails;

	return IMG_TRUE;
}

/******************************************************************************
 End of file (fftexhw.c)
******************************************************************************/

