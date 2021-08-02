/**********************************************************************
 * Name         : regalloc.c
 * Title        : Allocation of all other (non-predicate, non-internal) registers
 * Created      : April 2005
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
 *
 * Modifications:-
 * $Log: regalloc.c $
 * 
 * Added support for Results Register patching of Unpacked Result Shader.
 * Fixed bug where instruction result referencing flags were set even for unpatchable shader results.
 *  --- Revision Logs Removed --- 
 **************************************************************************/

#include "uscshrd.h"
#include "reggroup.h"
#include "bitops.h"
#include "usedef.h"
#include "execpred.h"
#include <limits.h>

/****************************************
 * FILE START
 ****************************************/

/********************************************************************************
 *
 *	Data Types and Macros
 *
 ********************************************************************************/

/* USC_NUM_SAMPLE_SRCS: Maximum number of sources for a sample. */
#define USC_NUM_SMP_SRCS (SMP_MAX_GRAD_SIZE + SMP_MAX_COORD_SIZE)

/* USC_NUM_SAMPLE_DSTS: Maximum number of destinations for a sample. */
#define USC_NUM_SMP_DSTS (SMP_MAX_GRAD_SIZE + SMP_MAX_COORD_SIZE)

typedef enum
{
	FUNCGROUP_MAIN,
	FUNCGROUP_SECONDARY,
	FUNCGROUP_MAX,
	FUNCGROUP_UNDEF
} FUNCGROUP;

typedef struct _SPILL_STATE
{
	/*
		Either NULL or points to a LIMM instruction in the secondary update program which
		loads the size of the spill area into a temporary.
	*/
	PINST		psSpillAreaSizeLoader;

	/*
		TRUE if spill LD or ST instructions can use immediate values for the offset into
		memory.
	*/
	IMG_BOOL	bSpillUseImmediateOffsets;

	/*
		Count of the secondary attributes used to hold memory offsets into the spill area.
	*/
	IMG_UINT32	uSpillAreaOffsetsSACount;

	/*
		Details of the secondary attributes used to hold memory offsets into the spill area.
	*/
	struct
	{
		/* Points to the structure describing the data to load into the secondary attribute. */
		PINREGISTER_CONST	psConst;
	} *asSpillAreaOffsetsSANums;

	/*
		List of the STLD/LDLD instructions inserted for spills to memory.
	*/
	USC_LIST	sSpillInstList;
} SPILL_STATE, *PSPILL_STATE;

struct _REGISTER_STATE_;

/*
 *	State used during register allocation.
 */

/* Start of the node numbers for primary attribute registers. */
#define REGALLOC_PRIMATTR_START						(0)

/*
	REGISTER_GROUP_DATA: Data for grouping registers with respect to the
	instruction and operand they appear in. Each instruction opcode has a
	REGISTER_GROUP_DATA.
*/
typedef struct _REGISTER_GROUP_DATA
{
	/*
		Information from previous instance of the instruction, used to filter
		out impossible groupings.
	*/
	PINST psInst;

	/* The previous register */
	PARG psPrevReg[USC_MAX_MOE_OPERANDS];
	/* The previous node for each operand (UINT_MAX is the undef value) */
	IMG_UINT32 auPrevNode[USC_MAX_MOE_OPERANDS];
	/* The size of the group built so far */
	IMG_UINT32 uGroup;
} REGISTER_GROUP_DATA, *PREGISTER_GROUP_DATA;

/* The default value for REGISTER_GROUP_DATA */
#define USC_REGISTER_GROUP_DATA_DEFAULT_VALUE							\
	{ NULL,																\
	  {NULL, NULL, NULL, NULL},											\
  	  {UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX}, \
	  0}

/*
	Types of hardware registers which are assigned by the register allocator.
*/
typedef enum _COLOUR_TYPE
{
	/* Primary attributes. */
	COLOUR_TYPE_PRIMATTR,
	/* Output registers. */
	COLOUR_TYPE_OUTPUT,
	/* GPI registers. */
	COLOUR_TYPE_GPI,
	/* Temporary registers. */
	COLOUR_TYPE_TEMP,
	COLOUR_TYPE_COUNT,

	/*
		Set for USP alternate shader outputs when we have decided not to use them.
	*/
	COLOUR_TYPE_DUMMY,

	/*
		Used for an uninitialized colour.
	*/
	COLOUR_TYPE_UNDEF,
} COLOUR_TYPE, *PCOLOUR_TYPE;

typedef struct _COLOUR
{
	COLOUR_TYPE		eType;
	IMG_UINT32		uNum;
} COLOUR, *PCOLOUR;

/* REGALLOC_DATA: Data common to all register allocator  */
typedef struct _REGALLOC_DATA_
{
	/*
	 * Compiler state
	 */
	PINTERMEDIATE_STATE psState;

	/*
		Function group we are allocating registers for.
	*/
	FUNCGROUP		eFuncGroup;

	/*
	 * Available hardware registers
	 */
	IMG_UINT32		auAvailRegsPerType[COLOUR_TYPE_COUNT];
	IMG_UINT32		uTotalAvailRegs;

	IMG_UINT32		uNrRegisters;      // Number of variables
	IMG_UINT32		uOutputStart;
	IMG_UINT32		uGPIStart;
	IMG_UINT32		uTempStart;

	/*
		Fixed VRegs: Virtual registers that must be placed in specific
		colours.
	*/
	PUSC_LIST				psFixedVRegList;

	/*
	 * Per-operand register grouping data
	 */
    REGISTER_GROUP_DATA		asRegGroupData[IOPCODE_MAX];

	/*
		Largest range of consecutive hardware registers.
	*/
	IMG_UINT32				uMaximumRangeLength;
} REGALLOC_DATA, *PREGALLOC_DATA;

#if defined(REGALLOC_GCOL)
/***************************************************************************
 *
 * Graph Colouring Types and Macros
 *
 ***************************************************************************/

#define MAXIMUM_RANGE_COUNT		(4)

typedef struct _RANGE
{
	/*
		First colour in the range.
	*/
	IMG_UINT32	uStart;
	/*
		Length of the range.
	*/
	IMG_UINT32	uLength;
} RANGE, *PRANGE;

typedef struct _COLOUR_RANGE_
{
	/*
		Hardware register type for the range.
	*/
	COLOUR_TYPE	eType;
	/*
		Range for each possible start alignment and for odd and
		even group lengths.
	*/
	RANGE		asRanges[HWREG_ALIGNMENT_COUNT][2];
	/*
		Flag (from BANK_FLAG_x) which is set if a node can be assigned
		a colour from this range.
	*/
	IMG_UINT32	uBankFlag;
} COLOUR_RANGE, *PCOLOUR_RANGE;

typedef struct _COLOUR_RANGE_LIST_
{
	/*
		Count of ranges.
	*/
	IMG_UINT32		uRangeCount;

	/*
		Information about each range.
	*/
	COLOUR_RANGE	asRanges[MAXIMUM_RANGE_COUNT];
} COLOUR_RANGE_LIST, *PCOLOUR_RANGE_LIST;

/*
	Flags for NODE_DATA.uBankFlags. A flag is set if a node can be
	assigned to the corresponding hardware register type.
*/
#define BANK_FLAG_TEMP		(1 << COLOUR_TYPE_TEMP)
#define BANK_FLAG_PRIMATTR	(1 << COLOUR_TYPE_PRIMATTR)
#define BANK_FLAG_OUTPUT	(1 << COLOUR_TYPE_OUTPUT)
#define BANK_FLAG_GPI		(1 << COLOUR_TYPE_GPI)
#define BANK_FLAG_ALL		(BANK_FLAG_TEMP | BANK_FLAG_PRIMATTR | BANK_FLAG_OUTPUT | BANK_FLAG_GPI)

typedef enum _NODE_FLAG
{
	/* Node read/write access markers  */
	NODE_FLAG_SPILLABLE,
	NODE_FLAG_REFERENCED,
	/* Set if the node couldn't be given a colour. */
	NODE_FLAG_UNCOLOURED,
	/* Set if the node is to be replaced by memory read/writes at each reference. */
	NODE_FLAG_SPILL,
	/* Set if the node is live at the point where program execution is suspended for feedback to the ISP. */
	NODE_FLAG_ALPHALIVE,
	/* Set if the node is live at the point where program execution is changing from pixel rate to sample rate. */
	NODE_FLAG_SPLITLIVE,
	/* Set if the node is used by the USP for temporary data when expanding a texture sample. */
	NODE_FLAG_USPTEMP,
	/* Set if the node is C10 format. */
	NODE_FLAG_C10,
	/* Set if the node is in Post Split phase */
	NODE_FLAG_POSTSPLITPHASE,
	/* Set when processing the main program if the node is a secondary attribute. */
	NODE_FLAG_ISSECATTR,
	NODE_FLAG_COUNT,
} NODE_FLAG, *PNODE_FLAG;

typedef struct _NODE_DATA
{
	IMG_UINT32			auFlags[UINTS_TO_SPAN_BITS(NODE_FLAG_COUNT)];
	/* Nodes spill addresses */
	IMG_UINT32			uAddress;
	/*
		Count of hardware registers assigned to this node. All normal
		intermediate registers have only one but shader outputs which can be
		renamed by the USP have more than one.
	*/
	IMG_UINT32			uColourCount;
	/*
		Hardware registers assigned to this node.
	*/
	PCOLOUR				asColours;
	/*
		Storage for puColours for a node assigned only a single colour.
	*/
	COLOUR				sSingleColour;
	/* Colour reserved for this node (regalloc.c:ColourNode) */
	COLOUR				sReservedColour;
	IMG_UINT32			uDegree;
	/*
		Entry in the list of nodes which can't be coloured/are to be spilled.
	*/
	USC_LIST_ENTRY		sSpillListEntry;
	/*
		Mask of flags for the hardware register banks which are available for this node.
	*/
	IMG_UINT32			uBankFlags;
} NODE_DATA, *PNODE_DATA;

/* RAGCOL_STATE: State for graph colouring allocator */
typedef struct _RAGCOL_STATE_
{
	/* Reg Alloc data */
	REGALLOC_DATA			sRAData;

	/* Per-node information. */
	PNODE_DATA				asNodes;

	/* Interference graph: X is connected to Y if X and Y must be assigned different hardware registers. */
	PINTFGRAPH				psIntfGraph;

	/*
	  Bit vector. The bit corresponding to a node is set
	  if the node was introduced when spilling another node.
	*/
    USC_VECTOR				sNodesUsedInSpills;

	/*
		List of nodes which couldn't be coloured.
	*/
	USC_LIST				sUncolouredList;

	/*
		List of nodes which are to be replaced by memory read/writes at each reference.
	*/
	USC_LIST				sSpillList;

	/*
		Points to the head of the list of instructions we have changed
		from MOV to PCKU8U8 or PCKC10C10.
	*/
	PUSC_LIST				psMOVToPCKList;

	/*
		Points to information about spilling variables to memory.
	*/
	PSPILL_STATE			psSpillState;

	/*
		Number of temporary registers used after register allocation.
	*/
	IMG_UINT32				uTemporaryRegisterCount;

	/*
		Number of temporary registers used after register allocation 
		for insturctions in post split point.
	*/
	IMG_UINT32				uTemporaryRegisterCountPostSplit;

	/*
		Number of primary attributes used after register allocation.
	*/
	IMG_UINT32				uPrimaryAttributeCount;

	/*
		Ranges of colours to use for general (not-fixed) variables.
	*/
	COLOUR_RANGE_LIST		sGeneralRange;

	/*
		Ranges of colours to use for general variables in the post-split
		program.
	*/
	COLOUR_RANGE_LIST		sPostSplitGeneralRange;

	/*
		Ranges of colours to use for variables which can be coloured
		to primary attributes only.
	*/
	COLOUR_RANGE_LIST		sPAOnlyRange;

	/*
		If TRUE primary attribute registers can be written. Otherwise
		they can't be assigned to any variable which is an instruction
		destination.
	*/
	IMG_BOOL				bNeverWritePrimaryAttributes;

	/*
		Number of registers used by the program.
	*/
	IMG_UINT32				uNumUsedRegisters;
} RAGCOL_STATE, *PRAGCOL_STATE;

/*
	Flag set on an instruction when it was changed from MOV to either
	PCKU8U8 or PCKC10C10.
*/
#define INST_CHANGEDMOVTOPCK				INST_LOCAL0

#endif /* defined(REGALLOC_GCOL) */

#if defined(REGALLOC_LSCAN)
/***************************************************************************
 *
 * Linear Scan Types and Macros
 *
 ***************************************************************************/

/* IREG_SORT: Record of spill locations for internal registers */
typedef struct _IREG_STORE_
{
	IMG_UINT32 uRGBNode;
	IMG_UINT32 uAlphaNode;
	IMG_UINT32 uAlphaChan;
} IREG_STORE;

/* RALSCAN_STATE: State for linear scan register allocator */
typedef struct _RALSCAN_STATE_
{
	/* Reg Alloc data */
	REGALLOC_DATA sRAData;

	/* Register information */
    IMG_UINT32      uRegAllocStart;    // First register node available for use

	/* Total number of available registers (excluding those needed for memory addressing) */
	IMG_UINT32 uNumHwRegs;

	/* apsRegInterval: Array of register intervals  (lists connected by psRegNext/psRegPrev) */
	PUSC_REG_INTERVAL *apsRegInterval;

	/* Array of intervals assigned to colours (lists connected by psProcNext/psProcPrev) */
	PUSC_REG_INTERVAL *apsColour;

	/* psIntervals: List of intervals sorted by start point (connected by psProcNext/psProcPrev) */
	PUSC_REG_INTERVAL psIntervals;

	/* List of intervals for fixed colour registers (in order,
	 * connected by psProcNext/psProcPrev) */
	PUSC_REG_INTERVAL psFixed;

	/* Node has a fixed colour */
	IMG_PUINT32 auIsFixed;
	/* Fixed colour of a node */
	IMG_PUINT32 auFixedColour;

	/* Node is written (needed to detect bogus temp. regs.) */
	IMG_PUINT32 auIsWritten;

	/*
	  Split feed back index (temp registers can't be live across this point).
	  uSplitFeedBackIdx == USC_OPEN_END if no split feedback.
	*/
	IMG_UINT32 uSplitFeedBackIdx;

	/*
	  Split index (temp registers can't be live across this point).
	  uSplitIdx == USC_OPEN_END if no split.
	*/
	IMG_UINT32 uSplitIdx;

    /* Nodes which are output registers */
	IMG_PUINT32 auIsOutput;

	/* Flags indicating whether an spill memory location is set for a node */
	IMG_PUINT32 auNodeHasLoc;
	/* Spill memory location allocated set for a node */
	IMG_PUINT32 auNodeMemLoc;
	/* Register number to use for a spill address register */
	IMG_UINT32 uAddrReg;
	/* Number of spill address registers needed */
	IMG_UINT32 uNumAddrRegisters;

	/* Whether memory was accessed for a spill */
	IMG_BOOL bSpillToMem;

	/* Whether memory was accessed for a spill before an instruction */
	IMG_BOOL bSpillToMemPre;
	/* Whether memory was accessed for a spill after an instruction */
	IMG_BOOL bSpillToMemPost;

	/* Instructions which set up hw registers */
	PINST *apsRegInst;

	/* Index of next colour to use for a spill */
	IMG_UINT32 uNextSpillColour;

	/* Array of intervals assigned to hwregs (lists connected by psProcNext/psProcPrev) */
	PUSC_REG_INTERVAL *apsHwReg;

	/* Number of iregisters used */
	IMG_UINT32 uNumIRegsUsed;

	/* Number of registers needed to store iregisters */
	IMG_UINT32 uNumIRegStores;

	/* Mask of IRegisters saved and written */
	IMG_UINT32 uIRegSaveMask;
	/* Mask of IRegisters loaded since last save */
	IMG_UINT32 uIRegLoadMask;
	/* Mask of IRegisters written but not saved */
	IMG_UINT32 uIRegWriteMask;

	/* Mask of IRegisters used for c10 data */
	IMG_UINT32 uIRegC10Mask;

	/* Where to store iregisters */
	IREG_STORE* apsIRegStore;

	/* First node storing iregisters */
	IMG_UINT32 uIRegNodeReg;

	/* IRegsters live in the program */
	IMG_UINT32 uProgIRegsLive;

	/* Index of last instruction to access a node */
	IMG_PUINT32 auLastNodeUse;

	/* IRegister spill registers */
	IMG_PUINT32 auIRegSpill;
	IMG_UINT32 uIRegSpillReg;

	/* Whether allocation data must be re-initialised */
	IMG_BOOL bRestartRegState;

	/* Number of iterations needed to initialise the register
	 * allocoator (always <2) */
	IMG_UINT32 uIteration;

#ifdef DEBUG
	IMG_UINT32 uMaxInstIdx;  // Maximum instruction index in the program.
#endif /* defined(DEBUG) */
} RALSCAN_STATE, *PRALSCAN_STATE;

/* USC_CALLBACK_DATA: Data passed to liveness call back */
typedef struct _USC_CALLBACK_DATA_
{
	PRALSCAN_STATE psRegState;      // Record of the allocator state
	IMG_UINT32 uInstIdx;            // Instruction index (last instruction has index 0)
	PUSC_REG_INTERVAL psCurrent;    // List of intervals still to complete
	IMG_UINT32 uIRegUse;            // IRegisters in use
	IMG_BOOL bConditional;			// Current block is conditionally executed
	IMG_UINT32 uLoopDepth;			// Loop nesting depth of current block
	IMG_BOOL bInFunction;           // In function body
	IMG_UINT32 uInstWriteMask;      // Write mask of instruction being processed
	IMG_UINT32 uSplitFeedBackIdx;   // Index of last instruction of first part of split feedback
	IMG_UINT32 uSplitIdx;			// Index of last instruction of first part of split
} USC_CALLBACK_DATA, *PUSC_CALLBACK_DATA;

/* RA_REG_LIMITS: Register limits for the register allocator */
typedef struct _RA_REG_LIMITS_
{
	IMG_UINT32 uNumVariables;    // Number of vregs to allocate
    IMG_UINT32 uNumHWRegs;       // Total number of hw registers
    IMG_UINT32 uNumPARegs;       // Number of PA registers
	IMG_UINT32 uNumOutputRegs;   // Number of output registers

	IMG_UINT32 uOutputStart;     // First output register to use
	IMG_UINT32 uTempStart;       // First temp register to use

	IMG_UINT32 uMaxPAIndex;      // Maximum index into PA registers
	IMG_UINT32 uMaxOutputIndex;  // Maximum index into output registers
} RA_REG_LIMITS;
#endif /* defined(REGALLOC_LSCAN) */

/***************************************************************************
 *
 * Common Code
 *
 ***************************************************************************/

#if defined(REGALLOC_GCOL) // Temporary, until lscan allocator is updated
static IMG_BOOL RegIsNode(PREGALLOC_DATA psRAData, PARG psReg);

static
COLOUR ArgumentToColour(PREGALLOC_DATA psRAData, PARG psArg)
/*****************************************************************************
 FUNCTION	: ArgumentToColour

 PURPOSE	: Converts an argument structure to the corresponding colour.

 PARAMETERS	: psRAData			- Register allocator state.
			  psArg				- Argument structure.

 RETURNS	: The corresponding colour.
*****************************************************************************/
{
	PINTERMEDIATE_STATE psState = psRAData->psState;
	COLOUR				sColour;

	switch (psArg->uType)
	{
		case USEASM_REGTYPE_TEMP: 
		{
			sColour.eType = COLOUR_TYPE_TEMP; 
			break;
		}
		case USEASM_REGTYPE_OUTPUT: 
		{
			sColour.eType = COLOUR_TYPE_OUTPUT; 
			break;
		}
		case USEASM_REGTYPE_FPINTERNAL: 
		{
			sColour.eType = COLOUR_TYPE_GPI; 
			break;
		}
		case USEASM_REGTYPE_PRIMATTR:
		case USEASM_REGTYPE_SECATTR:
		{
			sColour.eType = COLOUR_TYPE_PRIMATTR;
			break;
		}
		default: imgabort();
	}
	sColour.uNum = psArg->uNumber;
	return sColour;
}

static
IMG_VOID ColourToRegister(PREGALLOC_DATA psRAData,
						  PCOLOUR psColour,
						  IMG_PUINT32 puColourType,
						  IMG_PUINT32 puColourNum)
/*****************************************************************************
 FUNCTION	: ColourToRegister

 PURPOSE	: Converts a colour to a register type and number.

 PARAMETERS	: psRAData			- Register allocator state.
			  psColour			- Colour to convert.
			  puColourType		- Type corresponding to the colour.
			  puColourNum		- Register number corresponding to the colour.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINTERMEDIATE_STATE psState = psRAData->psState;

	switch (psColour->eType)
	{
		case COLOUR_TYPE_PRIMATTR:
		{
			*puColourType = USEASM_REGTYPE_PRIMATTR;
			*puColourNum = psColour->uNum;
			break;
		}
		case COLOUR_TYPE_OUTPUT:
		{
			*puColourType = USEASM_REGTYPE_OUTPUT;
			*puColourNum = psColour->uNum;
			break;
		}
		case COLOUR_TYPE_GPI:
		{
			*puColourType = USEASM_REGTYPE_FPINTERNAL;
			*puColourNum = psColour->uNum;
			break;
		}
		case COLOUR_TYPE_TEMP:
		{
			*puColourType = USEASM_REGTYPE_TEMP;
			*puColourNum = psColour->uNum;
			break;
		}
		case COLOUR_TYPE_DUMMY:
		{
			*puColourType = USC_REGTYPE_UNUSEDDEST;
			*puColourNum = 0;	
			break;
		}
		default: imgabort();
	}
}

static
IMG_BOOL IsNonTemporaryNode(PREGALLOC_DATA psRAData, IMG_UINT32 uNode)
/*****************************************************************************
 FUNCTION	: IsNonTemporaryNode

 PURPOSE	: Check if a node corresponds to a non-temporary register.

 PARAMETERS	: psRAData			- Register allocator state.
			  uNode				- Node to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (uNode < psRAData->uTempStart)
	{
		return IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
}

#endif /* REGALLOC_GCOL */ // Temporary, until lscan allocator is updated

static
IMG_VOID DoOnAllFuncGroupBasicBlocks(PINTERMEDIATE_STATE psState,
									 BLOCK_SORT_FUNC pfnSort,
									 BLOCK_PROC pfClosure,
									 IMG_BOOL bHandlesCalls,
									 IMG_PVOID pvUserData,
									 FUNCGROUP eFuncGroup)
/*****************************************************************************
 FUNCTION	: DoOnAllFuncGroupBasicBlocks

 PURPOSE	: Apply a callback to all basic blocks in a function group.

 PARAMETERS	:

 RETURNS	: Nothing.
*****************************************************************************/
{
	PFUNC psFunc;
	if (eFuncGroup == FUNCGROUP_SECONDARY)
	{
		DoOnCfgBasicBlocks(psState,
						   &(psState->psSecAttrProg->sCfg),
						   pfnSort,
						   pfClosure,
						   bHandlesCalls,
						   pvUserData);
	}
	else
	{
		for (psFunc = psState->psFnInnermost; psFunc; psFunc = psFunc->psFnNestOuter)
		{
			if (psFunc == psState->psSecAttrProg)
			{
				continue;
			}
			DoOnCfgBasicBlocks(psState,
							   &(psFunc->sCfg),
							   pfnSort,
							   pfClosure,
							   bHandlesCalls,
							   pvUserData);
		}
	}
}

static
IMG_VOID InitRAData(PINTERMEDIATE_STATE psState,
					PREGALLOC_DATA psRAData,
					IMG_UINT32 uNrRegisters)
/*****************************************************************************
 FUNCTION	: InitRAData

 PURPOSE	: Initialise register allocation data

 PARAMETERS	: psState	   - Compiler state.
			  psRAData     - Points to the register allocation state to initialise
              uNrRegisters - Number of nodes

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psRAData == NULL)
		return;

	psRAData->psState = psState;
	psRAData->uNrRegisters = uNrRegisters;
}

static PREGISTER_GROUP GetRegisterGroup(PREGALLOC_DATA psRAData, IMG_UINT32 uNode)
{
	PINTERMEDIATE_STATE	psState = psRAData->psState;

	ASSERT(uNode >= psRAData->uTempStart);
	ASSERT(uNode < psRAData->uNrRegisters);
	return FindRegisterGroup(psState, uNode - psRAData->uTempStart);
}

static PREGISTER_GROUP MakeRegisterGroup(PREGALLOC_DATA psRAData, IMG_UINT32 uNode)
{
	PINTERMEDIATE_STATE	psState = psRAData->psState;

	ASSERT(uNode >= psRAData->uTempStart);
	ASSERT(uNode < psRAData->uNrRegisters);
	return AddRegisterGroup(psState, uNode - psRAData->uTempStart);
}

static IMG_VOID NodeToRegister(PREGALLOC_DATA psRAData,
							   IMG_UINT32 uNode,
							   IMG_PUINT32 puType,
							   IMG_PUINT32 puNumber)
/*****************************************************************************
 FUNCTION	: NodeToRegister

 PURPOSE	: Convert a node number to a register type and number.

 PARAMETERS	: psRegState		- Register allocator state.
			  uNode				- Node number.
			  puType			- Returns the register type.
			  puNumber			- Returns the register number.

 RETURNS	: The node number.
*****************************************************************************/
{
	PINTERMEDIATE_STATE psState = psRAData->psState;
	ASSERT(uNode < psRAData->uNrRegisters);

	if (uNode < psRAData->uOutputStart)
	{
		*puType = USEASM_REGTYPE_PRIMATTR;
		if (puNumber != NULL)
		{
			*puNumber = uNode - REGALLOC_PRIMATTR_START;
		}
	}
	else if (uNode < psRAData->uGPIStart)
	{
		*puType = USEASM_REGTYPE_OUTPUT;
		if (puNumber != NULL)
		{
			*puNumber = uNode - psRAData->uOutputStart;
		}
	}
	else if (uNode < psRAData->uTempStart)
	{
		*puType = USEASM_REGTYPE_FPINTERNAL;
		if (puNumber != NULL)
		{
			*puNumber = uNode - psRAData->uGPIStart;
		}
	}
	else
	{
		*puType = USEASM_REGTYPE_TEMP;
		if (puNumber != NULL)
		{
			*puNumber = uNode - psRAData->uTempStart;
		}
	}
}

static IMG_UINT32 RegisterToNode(PREGALLOC_DATA psRAData,
								 IMG_UINT32 uRegisterType,
								 IMG_UINT32 uRegisterNum)
/*****************************************************************************
 FUNCTION	: RegisterToNode

 PURPOSE	: Converts between a register type and number and a node number.

 PARAMETERS	: psRAData			- Register allocator state.
			  uRegisterType		- Register.
			  uRegisterNum

 RETURNS	: The corresponding node.
*****************************************************************************/
{
	PINTERMEDIATE_STATE psState = psRAData->psState;
	switch (uRegisterType)
	{
		case USEASM_REGTYPE_TEMP:
			return psRAData->uTempStart + uRegisterNum;
		case USEASM_REGTYPE_OUTPUT:
			return psRAData->uOutputStart + uRegisterNum;
		case USEASM_REGTYPE_PRIMATTR:
			return REGALLOC_PRIMATTR_START + uRegisterNum;
		case USEASM_REGTYPE_FPINTERNAL:
			return psRAData->uGPIStart + uRegisterNum;
		default:
			imgabort();
	}
}

static IMG_UINT32 ArgumentToNode(PREGALLOC_DATA psRAData, PARG psArg)
/*****************************************************************************
 FUNCTION	: ArgumentToNode

 PURPOSE	: Convert between a source/dest argument and a node number.

 PARAMETERS	: psRegState		- Register allocator state.
			  psArg				- Argument to convert.

 RETURNS	: The node number.
*****************************************************************************/
{
	PINTERMEDIATE_STATE psState = psRAData->psState;
	if (psArg->uType == USEASM_REGTYPE_TEMP)
	{
		ASSERT(psArg->uNumber < psRAData->uNrRegisters);
		return RegisterToNode(psRAData, USEASM_REGTYPE_TEMP, psArg->uNumber);
	}
	else if (psArg->uType == USC_REGTYPE_REGARRAY)
	{
		PUSC_VEC_ARRAY_REG psVecArrayReg = psState->apsVecArrayReg[psArg->uNumber];

		ASSERT (psVecArrayReg != NULL);
		return RegisterToNode(psRAData, USEASM_REGTYPE_TEMP,
							  psVecArrayReg->uBaseReg + psArg->uArrayOffset);
	}
	else
	{
		return RegisterToNode(psRAData, psArg->uType, psArg->uNumber);
	}
}

static IMG_VOID GetFixedRegister(PINTERMEDIATE_STATE	psState,
							     PREGALLOC_DATA			psRAData,
								 IMG_UINT32				uNode,
								 PFIXED_REG_DATA*		ppsFixedReg,
								 IMG_PUINT32			puFixedRegOffset)
/*****************************************************************************
 FUNCTION	: GetFixedRegister

 PURPOSE	: Gets the fixed register corresponding to a  virtual register.

 PARAMETERS	: psState		- Compiler state.
			  psRAData		- Register allocator state.
			  uNode			- Node to get the fixed register for.
			  ppsFixedReg	- Returns the fixed register.
			  puFixedRegOffset
							- Returns the offset of the virtual register within
							the set of virtual register described by the 
							fixed register.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psRAData->psFixedVRegList != NULL)
	{
		PUSC_LIST_ENTRY	psListEntry;

		for (psListEntry = psRAData->psFixedVRegList->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
		{
			PFIXED_REG_DATA psFixedReg = IMG_CONTAINING_RECORD(psListEntry, PFIXED_REG_DATA, sListEntry);

			if (psFixedReg->uVRegType == USEASM_REGTYPE_TEMP)
			{
				IMG_UINT32		uRegIdx;

				for (uRegIdx = 0; uRegIdx < psFixedReg->uConsecutiveRegsCount; uRegIdx++)
				{
					if (RegisterToNode(psRAData, psFixedReg->uVRegType, psFixedReg->auVRegNum[uRegIdx]) == uNode)
					{
						*ppsFixedReg = psFixedReg;
						if (puFixedRegOffset != NULL)
						{
							*puFixedRegOffset = uRegIdx;
						}
						return;
					}
				}
			}
		}
	}
	imgabort();
}

static 
IMG_VOID GetFixedColour(PINTERMEDIATE_STATE	psState,
						PREGALLOC_DATA		psRAData,
						IMG_UINT32			uNode,
						IMG_PUINT32			puColourType,
						IMG_PUINT32			puColourNum)
/*****************************************************************************
 FUNCTION	: GetFixedColour

 PURPOSE	: Gets the fixed colour to be assigned to a virtual register.

 PARAMETERS	: psState		- Compiler state.
			  psRAData		- Register allocator state.
			  uNode			- Node to get the fixed colour for.
			  puColourType	- Returns the register bank of the fixed colour.
			  puColourNum	- Returns the register number of the fixed colour.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PFIXED_REG_DATA	psFixedReg;
	IMG_UINT32		uFixedRegOffset;

	GetFixedRegister(psState, psRAData, uNode, &psFixedReg, &uFixedRegOffset);

	*puColourType = psFixedReg->sPReg.uType;
	if (psFixedReg->sPReg.uNumber != USC_UNDEF)
	{
		*puColourNum = psFixedReg->sPReg.uNumber + uFixedRegOffset;
	}
	else
	{
		*puColourNum = USC_UNDEF;
	}
}



static
IMG_BOOL IsAfter(PREGISTER_GROUP		psNode,
				 PREGISTER_GROUP		psCheck)
/*****************************************************************************
 FUNCTION	: IsAfter

 PURPOSE	: Checks if one node needs to be given a higher register number
			  than another.

 PARAMETERS	: psNode		- Node with the lower register number.
			  psCheck		- Node with the higher register number.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	for (; psNode != NULL; psNode = psNode->psNext)
	{
		if (psNode == psCheck)
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

IMG_INTERNAL IMG_UINT32 GetIRegsWrittenMask(PINTERMEDIATE_STATE	psState,
											PINST				psInst,
											IMG_BOOL			bPreMoe)
/*****************************************************************************
 FUNCTION	: GetIRegsWrittenMask

 PURPOSE	: Get the mask of internal registers written by an instruction.

 PARAMETERS	: psState		- Internal compiler state
			  psInst		- Instruction to get the mask for.
			  bPreMoe		- Use the pre-moe register numbers.

 RETURNS	: The mask of internal registers written.
*****************************************************************************/
{
	IMG_UINT32	uIRegsWrittenMask;
	IMG_UINT32	uDestIdx;

	PVR_UNREFERENCED_PARAMETER(psState);

	uIRegsWrittenMask = 0;
	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		PARG	psDest = &psInst->asDest[uDestIdx];

		if (
				psDest->uType == USEASM_REGTYPE_FPINTERNAL && 
				(psInst->auDestMask[uDestIdx] & psInst->auLiveChansInDest[uDestIdx]) != 0
		   )
		{
			IMG_UINT32	uRegNumber;

			if (bPreMoe)
			{
				uRegNumber = psDest->uNumberPreMoe;
			}
			else
			{
				uRegNumber = psDest->uNumber;
			}

			uIRegsWrittenMask |= (1U << uRegNumber);
		}
	}
	return uIRegsWrittenMask;
}

IMG_INTERNAL IMG_UINT32 GetIRegsReadMask(PINTERMEDIATE_STATE	psState,
										 PINST					psInst,
										 IMG_BOOL				bPreMoe)
/*****************************************************************************
 FUNCTION	: GetIRegsReadMask

 PURPOSE	: Get the mask of internal registers read by an instruction.

 PARAMETERS	: psState		- Internal compiler state
			  psInst		- Instruction to get the mask for.
			  bPreMoe		- Use the pre-moe register numbers.

 RETURNS	: The mask of internal registers read.
*****************************************************************************/
{
	IMG_UINT32	uArg;
	IMG_UINT32	uIRegsReadMask;

	PVR_UNREFERENCED_PARAMETER(psState);

	uIRegsReadMask = 0;
	for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
	{
		if (psInst->asArg[uArg].uType == USEASM_REGTYPE_FPINTERNAL)
		{
			if (GetLiveChansInArg(psState, psInst, uArg) != 0)
			{
				IMG_UINT32	uRegNumber;

				if (bPreMoe)
					uRegNumber = psInst->asArg[uArg].uNumberPreMoe;
				else
					uRegNumber = psInst->asArg[uArg].uNumber;

				uIRegsReadMask |= (1U << uRegNumber);
			}
		}
	}
	return uIRegsReadMask;
}

static PINST AdjustBaseAddress(PINTERMEDIATE_STATE psState,
							   IMG_UINT32 uSecAttr)
/*****************************************************************************
 FUNCTION	: AdjustBaseAddress

 PURPOSE	: Generate code to adjust the base address of a secondary attribute
			  for the pipe number.

 PARAMETERS	: psState			- Compiler state.
`			  uSecAttr			- Secondary attribute to adjust.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psLimmInst, psImaInst;
	PINST		psSecAttrFirstInst = psState->psSecAttrProg->sCfg.psEntry->psBody;
	IMG_UINT32	uStrideTemp;
	ARG			sPipeNumber;
	PCODEBLOCK	psInsertBlock;

	/*
		We append instructions to the first block of the SA Updater, so they're always executed.
	*/
	psInsertBlock = psState->psSecAttrProg->sCfg.psEntry;

	if (psState->uMPCoreCount > 1)
	{
		IMG_UINT32	uPipeCountTemp;
		PINST		psImaeInst;

		/*
			For multicore systems concatanate the core number with the pipe number.
		*/

		MakeNewTempArg(psState, UF_REGFORMAT_F32, &sPipeNumber);

		/*
			Get a temporary to hold the number of pipelines.
		*/
		uPipeCountTemp = GetNextRegister(psState);
		
		/*
			Load the count of pipelines per-core into a temporary.
		*/
		psLimmInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psLimmInst, ILIMM);
		SetBit(psLimmInst->auFlag, INST_SKIPINV, 1);
		SetDest(psState, psLimmInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uPipeCountTemp, UF_REGFORMAT_F32);
		psLimmInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
		psLimmInst->asArg[0].uNumber = psState->psTargetFeatures->ui32NumUSEPipes;
		InsertInstBefore(psState, psInsertBlock, psLimmInst, psSecAttrFirstInst);

		/*
			Calculate:
				PIPE_NUMBER = PIPE_COUNT_PER_CORE * CORE_NUMBER + PIPE_NUMBER
		*/
		psImaeInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psImaeInst, IIMAE);
		SetBit(psImaeInst->auFlag, INST_SKIPINV, 1);
		SetDestFromArg(psState, psImaeInst, 0 /* uDestIdx */, &sPipeNumber);
		SetSrc(psState, psImaeInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uPipeCountTemp, UF_REGFORMAT_F32);
		psImaeInst->asArg[1].uType = USEASM_REGTYPE_GLOBAL;
		/*
			For cores affected by BRN28033 the hardware doesn't provide any
			way to read the core number directly so services stores it into
			the G1 register.
		*/
		if (psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_28033)
		{
			psImaeInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_BANK_G1;
		}
		else
		{
			psImaeInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_BANK_CORENUMBER;
		}
		psImaeInst->asArg[2].uType = USEASM_REGTYPE_GLOBAL;
		psImaeInst->asArg[2].uNumber = EURASIA_USE_SPECIAL_BANK_PIPENUMBER;
		InsertInstBefore(psState, psInsertBlock, psImaeInst, psSecAttrFirstInst);
	}
	else
	{
		InitInstArg(&sPipeNumber);
		sPipeNumber.uType = USEASM_REGTYPE_GLOBAL;
		sPipeNumber.uNumber = EURASIA_USE_SPECIAL_BANK_PIPENUMBER;
	}

	/* Assign a temporary for the per-pipe stride. */
	uStrideTemp = GetNextRegister(psState);

	/* Load the stride per pipe into a temporary. */
	psLimmInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psLimmInst, ILIMM);
	SetBit(psLimmInst->auFlag, INST_SKIPINV, 1);
	SetDest(psState, psLimmInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uStrideTemp, UF_REGFORMAT_F32);
	psLimmInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
	psLimmInst->asArg[0].uNumber = USC_UNDEF;
	InsertInstBefore(psState, psInsertBlock, psLimmInst, psSecAttrFirstInst);

	/* Add the offset of the start of the scratch area for this pipe onto the scratch base. */
	psImaInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psImaInst, IIMAE);
	SetBit(psImaInst->auFlag, INST_SKIPINV, 1);
	psImaInst->asDest[0].uType = USEASM_REGTYPE_PRIMATTR;
	psImaInst->asDest[0].uNumber = uSecAttr;
	SetSrc(psState, psImaInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uStrideTemp, UF_REGFORMAT_F32);
	SetSrcFromArg(psState, psImaInst, 1 /* uSrcIdx */, &sPipeNumber);
	psImaInst->asArg[2].uType = USEASM_REGTYPE_PRIMATTR;
	psImaInst->asArg[2].uNumber = uSecAttr;
	InsertInstBefore(psState, psInsertBlock, psImaInst, psSecAttrFirstInst);

	/*
		Make the secondary attribute pointing to the scratch area base live out of
		the secondary update program.
	*/
	SetRegisterLiveMask(psState,
						&psState->psSecAttrProg->sCfg.psExit->sRegistersLiveOut,
						USEASM_REGTYPE_PRIMATTR,
						uSecAttr,
						0 /* uArrayOffset */,
						USC_DESTMASK_FULL);

	return psLimmInst;
}

/********************
 * Start of setup spill area main block
 ********************/
static
IMG_VOID SetupSpillAreaBP(PINTERMEDIATE_STATE psState,
						  PCODEBLOCK psBlock,
						  IMG_PVOID pvUseImmOffset)
/*****************************************************************************
 FUNCTION	  : SetupSpillAreaBP

 PURPOSE	  : BLOCK_PROC to Setup the spill area access

 PARAMETERS	  : psState			- Compiler state.
                pvUseImmOffset - an IMG_BOOL (not pointer!), should we use
						an immediate offset for STLD/LDLD instructions?

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psInst;

#if defined (REGALLOC_LSCAN)
	IMG_UINT32 uLastRegType = USC_UNDEF;
	IMG_UINT32 uLastRegNum = USC_UNDEF;
	IMG_UINT32 uLastAddress = USC_UNDEF;
#endif /* defined REGALLOC_LSCAN */

	IMG_BOOL bUseImmOffset = (pvUseImmOffset) ? IMG_TRUE : IMG_FALSE;

	/* Add the spill area stride to spill addresses */
	for (psInst = psBlock->psBody; psInst; psInst = psInst->psNext)
	{
		PARG		psOffsetArg;
		IMG_UINT32	uMemoryOffset;

		if (!(psInst->eOpcode == ISPILLREAD || psInst->eOpcode == ISPILLWRITE))
		{
			continue;
		}

		ASSERT(GetBit(psInst->auFlag, INST_SPILL) == 1);

		psOffsetArg = &psInst->asArg[MEMSPILL_OFFSET_ARGINDEX];
		if (psOffsetArg->uType != USEASM_REGTYPE_IMMEDIATE)
		{
			continue;
		}

		/*
			The memory offset is interpreted as:
				SRC1[31..16] * INSTANCE_NUMBER + SRC1[15..0]
		*/
		uMemoryOffset = (psOffsetArg->uNumber * sizeof(IMG_UINT32)) << EURASIA_USE_LDST_NONIMM_LOCAL_OFFSET_SHIFT;
		uMemoryOffset |= (psState->uSpillAreaSize * sizeof(IMG_UINT32)) << EURASIA_USE_LDST_NONIMM_LOCAL_STRIDE_SHIFT;

		if (bUseImmOffset)
		{
			psOffsetArg->uNumber = uMemoryOffset;
		}
		else
		{
			/*
			  Try to get a secondary attribute containing the offset.
			*/
			if (AddStaticSecAttr(psState, uMemoryOffset, NULL, NULL))
			{
				IMG_UINT32	uSecAttrType, uSecAttrNum;

				AddStaticSecAttr(psState, uMemoryOffset, &uSecAttrType, &uSecAttrNum);

				psOffsetArg->uType = uSecAttrType;
				psOffsetArg->uNumber = uSecAttrNum;
			}
			else
			{
				IMG_UINT32 uAddrType = psInst->asTemp[0].eType;
				IMG_UINT32 uAddrNum = psInst->asTemp[0].uNumber;
				IMG_BOOL bNewAddr = IMG_TRUE;

#if defined(REGALLOC_LSCAN)
				if ((uLastRegType == uAddrType) &&
					(uLastRegNum == uAddrNum) &&
					(uLastAddress == uMemoryOffset))
				{
					bNewAddr = IMG_FALSE;
#if defined(UF_TESTBENCH) && defined(REGALLOC_GCOL)
					if (g_uOnlineOption == 0)
					{
						bNewAddr = IMG_TRUE;
					}
#endif /* defined(UF_TESTBENCH) && defined(REGALLOC_GCOL) */
				}
#endif /* defined(REGALLOC_LSCAN) */

				if (bNewAddr)
				{
					PINST		psLimmInst;

#if defined (REGALLOC_LSCAN)
					uLastRegType = uAddrType;
					uLastRegNum = uAddrNum;
					uLastAddress = uMemoryOffset;
#endif /* defined REGALLOC_LSCAN */
					psLimmInst = AllocateInst(psState, psInst);

					/*
					  Create a LIMM instruction to load this value.
					*/
					SetOpcode(psState, psLimmInst, ILIMM);

					/*
						Copy the SKIPINVALID flag from the memory load/store instruction.
					*/
					SetBit(psLimmInst->auFlag, INST_SKIPINV, GetBit(psInst->auFlag, INST_SKIPINV));

					psLimmInst->asDest[0].uType = uAddrType;
					psLimmInst->asDest[0].uNumber = uAddrNum;

					psLimmInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
					psLimmInst->asArg[0].uNumber = uMemoryOffset;

					InsertInstBefore(psState, psBlock, psLimmInst, psInst);
				}

				InitInstArg(psOffsetArg);
				psOffsetArg->uType = uAddrType;
				psOffsetArg->uNumber = uAddrNum;
			}
		}
	}
}

static
IMG_VOID SetupSpillArea(PINTERMEDIATE_STATE psState, PSPILL_STATE psSpillState)
/*****************************************************************************
 FUNCTION	  : SetupSpillArea

 PURPOSE	  : Setup the spill area access

 PARAMETERS	  : psState			- Compiler state.
                puSecAttrTemp   -

 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		Can we use an immediate offset with the STLD/LDLD instructions?
	*/
	if (psSpillState->bSpillUseImmediateOffsets)
	{
		ASSERT(psState->uSpillAreaSize <= EURASIA_USE_LDST_MAX_IMM_LOCAL_STRIDE);
		ASSERT(!(psState->psTargetBugs->ui32Flags & (SGX_BUG_FLAGS_FIX_HW_BRN_24895 | SGX_BUG_FLAGS_FIX_HW_BRN_30795)));

		/*
			Round the stride up to the next multiple of the granularity.
		*/
		psState->uSpillAreaSize =
			(psState->uSpillAreaSize + EURASIA_USE_LDST_IMM_LOCAL_STRIDE_GRAN - 1);
		psState->uSpillAreaSize &= ~(EURASIA_USE_LDST_IMM_LOCAL_STRIDE_GRAN - 1U);
	}

	/*
		Where secondary attributes have been reserved to hold offsets into the
		spill area set their final values.
	*/
	if (psSpillState->uSpillAreaOffsetsSACount > 0)
	{
		IMG_UINT32	uMemIdx;

		ASSERT(!psSpillState->bSpillUseImmediateOffsets);

		for (uMemIdx = 0; uMemIdx < psSpillState->uSpillAreaOffsetsSACount; uMemIdx++)
		{
			IMG_UINT32		uMemoryOffset;
			PINREGISTER_CONST	psConst;

			uMemoryOffset = uMemIdx * sizeof(IMG_UINT32);
			uMemoryOffset |= (psState->uSpillAreaSize * sizeof(IMG_UINT32)) << 16;

			psConst = psSpillState->asSpillAreaOffsetsSANums[uMemIdx].psConst;

			ReplaceSpillAreaOffsetByStatic(psState, psConst, uMemoryOffset);
		}

		UscFree(psState, psSpillState->asSpillAreaOffsetsSANums);
	}

	/*
		Update all instructions loading/storing spill data with the final
		spill area size.
	*/
	DoOnAllBasicBlocks(psState, ANY_ORDER,
					   SetupSpillAreaBP,
					   IMG_FALSE, (IMG_PVOID)(IMG_UINTPTR_T)psSpillState->bSpillUseImmediateOffsets);

	/*
		If we added instructions to the secondary update program to 
		add the offset within the spill area for instances on the
		current pipeline then update the instruction loading the
		per-pipeline stride now we know the final size of the spill area.
	*/
	if (psSpillState->psSpillAreaSizeLoader != NULL)
	{
		PINST	psInst;

		psInst = psSpillState->psSpillAreaSizeLoader;
		ASSERT(psInst->eOpcode == ILIMM);
		ASSERT(psInst->asArg[0].uType == USEASM_REGTYPE_IMMEDIATE);
		ASSERT(psInst->asArg[0].uNumber == USC_UNDEF);
		psInst->asArg[0].uNumber = psState->uSpillAreaSize * sizeof(IMG_UINT32) * EURASIA_MAX_USE_INSTANCES;

		psSpillState->psSpillAreaSizeLoader = NULL;
	}
}
/********************
 * End of setup spill area main block
 ********************/

static
IMG_BOOL RegIsNode(PREGALLOC_DATA psRAData, PARG psReg)
/*****************************************************************************
 FUNCTION   : RegIsNode

 PURPOSE    : Test whether a register is a node (subject to register allocation).

 PARAMETERS : psRAData       - Register allocator state
              psReg            - Regsiter to test

 RETURNS    : IMG_TRUE iff psReg is a node.
*****************************************************************************/
{
    /* Temporary are nodes */
    if (psReg->uType == USEASM_REGTYPE_TEMP || psReg->uType == USC_REGTYPE_REGARRAY)
	{
		PREGISTER_GROUP	psGroup;

		if (psReg->uType == USEASM_REGTYPE_TEMP)
		{
			psGroup = psReg->psRegister->psGroup;
		}
		else
		{
			psGroup = psRAData->psState->apsVecArrayReg[psReg->uNumber]->psGroup;
		}

		if (psRAData->eFuncGroup == FUNCGROUP_MAIN && IsPrecolouredToSecondaryAttribute(psGroup))
		{
			return IMG_FALSE;
		}
		return IMG_TRUE;
    }

    /* Primary attributes less than the declared number are nodes */
    if (psReg->uType == USEASM_REGTYPE_PRIMATTR &&
        psReg->uNumber < psRAData->auAvailRegsPerType[COLOUR_TYPE_PRIMATTR])
    {
        return IMG_TRUE;
    }

	/* Output registers are node */
	if (psReg->uType == USEASM_REGTYPE_OUTPUT &&
		psReg->uNumber < psRAData->auAvailRegsPerType[COLOUR_TYPE_OUTPUT])
	{
		return IMG_TRUE;
	}

    return IMG_FALSE;
}

#if defined(UF_TESTBENCH)
/*****************************************************************************
 FUNCTION	  : InitMapData

 PURPOSE	  : Initialise register map data

 PARAMETERS	  : psMapData  - Register map data

 RETURNS	  : Nothing.
*****************************************************************************/
IMG_INTERNAL
IMG_VOID InitMapData(PUSC_REGMAP_DATA psMapData)
{
	if (psMapData == NULL)
		return;

	memset(psMapData, 0, sizeof(USC_REGMAP_DATA));
}

#if !(defined(PCONVERT_HASH_COMPUTE) && defined(__linux__))
/*****************************************************************************
 FUNCTION	  : PrintNodeLScan

 PURPOSE	  : Print a node of the linear scan allocator

 PARAMETERS	  : psState       - Compiler state.
                psMapData  - Register map data
                uNode         - Node to print

 RETURNS	  : Nothing.
*****************************************************************************/
static
IMG_VOID PrintNodeLScan(PUSC_REGMAP_DATA psMapData,
                        IMG_UINT32 uNode,
						IMG_PCHAR *ppszBuffer)
{
	IMG_UINT32 uRegType = USC_UNDEF;
    IMG_UINT32 uRegNum = USC_UNDEF;
	IMG_UINT32 uCtr = 0;
	IMG_PCHAR pszBuffer = NULL;
	IMG_PCHAR pszName = NULL;
	pszBuffer = *ppszBuffer;

	if (psMapData->pfnNodeToReg == NULL)
	{
		pszName = "node";
	}
	else if (psMapData->pfnNodeToReg(psMapData, uNode, &uRegType, &uRegNum))
	{
		pszName = g_pszRegType[uRegType];
	}
	else
	{
		pszName = "node";
	}

	uCtr = sprintf(*ppszBuffer, "%s%u", pszName, uRegNum);
	(*ppszBuffer) += uCtr;
}

/*****************************************************************************
 FUNCTION	  : PrintIntervalList

 PURPOSE	  : Print the assignments of a list of intervals

 PARAMETERS	  : psState       - Compiler state.
                psMapData     - Register map data
                psInterval    - Interval to print
                bPrintNode    - Whether to print the node number

 RETURNS	  : Nothing.
*****************************************************************************/
static
IMG_VOID PrintInterval(PUSC_REGMAP_DATA psMapData,
                       PUSC_REG_INTERVAL psInterval,
					   IMG_BOOL bPrintNode,
					   IMG_PCHAR *ppszBuffer)
{
	IMG_UINT32 uCtr = 0;

	if (psInterval == NULL)
		return;

	/* Node */
	if (bPrintNode)
	{
		PrintNodeLScan(psMapData, psInterval->uNode, ppszBuffer);
		uCtr = sprintf((*ppszBuffer), ": ");
		(*ppszBuffer) += uCtr;
	}

	/* Limits */
	if (psInterval->uStart == USC_OPEN_START)
		uCtr = sprintf((*ppszBuffer), "(open, ");
	else
		uCtr = sprintf((*ppszBuffer), "(%u, ", psInterval->uStart);
	(*ppszBuffer) += uCtr;

	if (psInterval->uEnd == USC_OPEN_END)
		uCtr = sprintf((*ppszBuffer), "open]");
	else
		uCtr = sprintf((*ppszBuffer), "%u]", psInterval->uEnd);
	(*ppszBuffer) += uCtr;

	/* Print mapping */
	uCtr = sprintf((*ppszBuffer), " -> ");
	(*ppszBuffer) += uCtr;
	if (psInterval->bSpill)
	{
		uCtr = sprintf(*ppszBuffer, "spill");
		(*ppszBuffer) += uCtr;
	}
	else
	{
		PrintNodeLScan(psMapData, psInterval->uColour, ppszBuffer);
	}

}

static
IMG_VOID PrintIntervalList(PINTERMEDIATE_STATE psState,
						   PUSC_REGMAP_DATA psMapData,
                           PUSC_REG_INTERVAL psList)
/*****************************************************************************
 FUNCTION	  : PrintIntervalList

 PURPOSE	  : Print the assignments of a list of intervals

 PARAMETERS	  : psState			  - Compiler state.
                psMapData         - Register map data
                uNode             - Node being printed
                psList            - Interval list to print

 RETURNS	  : Nothing.
*****************************************************************************/
{
	IMG_PCHAR pszOutput = NULL;
    if (psList == NULL)
        return;

	/* Print the intervals */
	pszOutput = UscAlloc(psState, sizeof(IMG_CHAR) * 100);
	for ( ; psList != NULL; psList = psList->psRegNext)
	{
		IMG_PCHAR pszBuffer;

		pszBuffer = pszOutput;
		memset(pszBuffer, 0, sizeof(IMG_CHAR) * 100);

		PrintInterval(psMapData, psList, IMG_TRUE, &pszBuffer);
		if (pszOutput[0] != 0)
		{
			DBG_PRINTF((DBG_MESSAGE, "%s;", pszOutput));
		}
	}
	UscFree(psState, pszOutput);
}

IMG_INTERNAL
IMG_VOID PrintNodeColours(PINTERMEDIATE_STATE psState,
						  PUSC_REGMAP_DATA psMapData,
                          IMG_UINT32 uNumNodes,
                          PUSC_REG_INTERVAL *apsInterval)
/*****************************************************************************
 FUNCTION	  : PrintNodeColours

 PURPOSE	  : Print the assignment of colours to nodes

 PARAMETERS	  : psState			  - Compiler state.
                psMapData         - Register map data
                uNumNodes         - Number of nodes
                apsInterval       - Interval array

 RETURNS	  : Nothing.
*****************************************************************************/
{
    IMG_UINT32 uCtr;

	DBG_PRINTF((DBG_MESSAGE, "Register Mapping"));
	DBG_PRINTF((DBG_MESSAGE, "----------------"));
    for (uCtr = 0; uCtr < uNumNodes; uCtr ++)
    {
        PrintIntervalList(psState, psMapData, apsInterval[uCtr]);
    }
	DBG_PRINTF((DBG_MESSAGE, "------------------------------"));
}
#endif /* !(defined(PCONVERT_HASH_COMPUTE) && defined(__linux__)) */
#endif /* defined(UF_TESTBENCH) */

#if 0
static
PUSC_LIST OutputRegs(PINTERMEDIATE_STATE psState,
					 IMG_UINT32 uColOutputCount)
/*****************************************************************************
 FUNCTION	  : OutputRegs

 PURPOSE	  : Make a list of output register types and numbers

 PARAMETERS	  : psState			    - Compiler state.
                uColOutputCount     - Number of outputs

 NOTES        : Returns a list of pairs. Both list and pairs must be
                free'd.

 RETURNS	  : A list of the register types and numbers used for outputs
*****************************************************************************/
{
	PUSC_PAIR psPair = NULL;
	PUSC_LIST psList = NULL;
	USC_REGTYPE uRegType;
	IMG_UINT32 uRegNum;
	IMG_UINT32 i;

	uRegType = USEASM_REGTYPE_TEMP;
	if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL)
	{
		/* Pixel shader */
		for (i = 0; i < uColOutputCount; i++)
		{
			uRegNum = i;

			psPair = PairMake(psState, (IMG_PVOID)uRegType, (IMG_PVOID)uRegNum);
			psList = ListCons(psState, psPair, psList);
		}

		if ((psState->uFlags & USC_FLAGS_DEPTHFEEDBACKPRESENT))
		{
			uRegNum = psState->uDepthOutputNum;

			psPair = PairMake(psState, (IMG_PVOID)uRegType, (IMG_PVOID)uRegNum);
			psList = ListCons(psState, psPair, psList);
		}
	}
	else
	{
		if(psState->psSAOffsets->eShaderType == USC_SHADERTYPE_VERTEX)
		{
		/* Vertex shader */
		if (psState->uFlags & USC_FLAGS_VSCLIPPOS_USED)
		{
			for (i = 0; i < 4; i++)
			{
				uRegNum = i;
				psPair = PairMake(psState, (IMG_PVOID)uRegType, (IMG_PVOID)uRegNum);
				psList = ListCons(psState, psPair, psList);
			}
		}
		if (psState->uCompilerFlags & UF_REDIRECTVSOUTPUTS)
		{
			uRegType = USEASM_REGTYPE_PRIMATTR;
			for (i = 0; i <= psState->uMaximumVSOutput; i++)
			{
				uRegNum = i;
				psPair = PairMake(psState, (IMG_PVOID)uRegType, (IMG_PVOID)uRegNum);
				psList = ListCons(psState, psPair, psList);
			}
		}
	}
	}

	return psList;
}
#endif

#if defined(REGALLOC_GCOL)
/**************************************************************************
 *
 * Graph Colouring Allocator
 *
 **************************************************************************/
static IMG_VOID FreeRAGColState(PINTERMEDIATE_STATE psState,
								PRAGCOL_STATE *ppsRegState,
								IMG_BOOL bFreeState)
/*****************************************************************************
 FUNCTION	: FreeRAGColState

 PURPOSE	: Free register allocation state.

 PARAMETERS	: psState			- Compiler state.
			  ppsRegState		- Points to the register allocation state to free.
			  bFreeState		- If FALSE just free the array that vary in size
								depending on the number of registers.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PRAGCOL_STATE	psRegState = *ppsRegState;
	IMG_UINT32		uIdx;

	for (uIdx = 0; uIdx < psRegState->sRAData.uNrRegisters; uIdx++)
	{
		PNODE_DATA	psNodeData = &psRegState->asNodes[uIdx];

		if (psNodeData->uColourCount > 1)
		{
			UscFree(psState, psNodeData->asColours);
			psNodeData->asColours = NULL;
		}
	}

	IntfGraphDelete(psState, psRegState->psIntfGraph);
	psRegState->psIntfGraph = NULL;

	UscFree(psState, psRegState->asNodes);

	if (bFreeState)
    {
        ClearVector(psState, &psRegState->sNodesUsedInSpills);
        UscFree(psState, *ppsRegState);
    }
}

static IMG_VOID AllocateRAGColState(PINTERMEDIATE_STATE psState,
									IMG_UINT32 uNrRegisters,
									PRAGCOL_STATE* ppsRegState)
/*****************************************************************************
 FUNCTION	: AllocateRAGCOLState

 PURPOSE	: Allocate register allocation data structures.

 PARAMETERS	: psState			- Compiler state.
			  uNrRegisters		- Number of registers that need to be allocated.
			  ppsRegState		- Initialised with the allocated structure.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PRAGCOL_STATE psRegState;

	/* Allocate the register state */
	if ((*ppsRegState) == NULL)
	{
		*ppsRegState = UscAlloc(psState, sizeof(RAGCOL_STATE));
		psRegState = *ppsRegState;

		InitVector(&psRegState->sNodesUsedInSpills, USC_MIN_VECTOR_CHUNK, IMG_FALSE);
	}
	else
	{
		FreeRAGColState(psState, ppsRegState, IMG_FALSE);
		psRegState = *ppsRegState;
	}
	InitRAData(psState, &psRegState->sRAData, uNrRegisters);

	psRegState->sRAData.psState = psState;
	psRegState->sRAData.uNrRegisters = uNrRegisters;

	usc_alloc_num(psState, psRegState->asNodes, uNrRegisters);
}

static IMG_VOID CheckGPIFlags(PINTERMEDIATE_STATE	psState,
							  PRAGCOL_STATE			psRegState,
							  FUNCGROUP				eFuncGroup)
/*****************************************************************************
 FUNCTION   : CheckGPIFlags

 PURPOSE    : Check the GPI flag on every temp register and unset it if a GPI
			  cannot be allocated for it.

 PARAMETERS : psState           - Compiler state.
              psRegState		- Register state
              eFuncGroup		- The function type to allocate

 RETURNS    : Nothing.
*****************************************************************************/
{
	PFUNC			pProg;
	IMG_UINT32		uNode;

	if (eFuncGroup == FUNCGROUP_MAIN)
	{
		pProg = psState->psMainProg;
	}
	else
	{
		pProg = psState->psSecAttrProg;
	}

	for (uNode = psRegState->sRAData.uTempStart; uNode < psRegState->sRAData.uNrRegisters; ++uNode)
	{
		PINST			psDefineInst, psLastUseInst;
		IMG_UINT32		uTemp = uNode - psRegState->sRAData.uTempStart;

		/* 
			Avoid allocating GPIs to the first instruction a block, because a NOP would have to be inserted
			before it to put the required "no shed" flag on.
		*/
		if (!CanReplaceTempWithAnyGPI(psState, uTemp, &psDefineInst, &psLastUseInst, IMG_TRUE) ||
			psDefineInst == psDefineInst->psBlock->psBody)
		{
			PNODE_DATA psNode = &psRegState->asNodes[uNode];
			psNode->uBankFlags &= ~BANK_FLAG_GPI;
		}
	}
}

static IMG_VOID CopyColourRange(PINTERMEDIATE_STATE	psState,
								PRANGE				psRange, 
								IMG_INT32			iStart, 
								IMG_INT32			iEnd, 
								IMG_UINT32			uAlignOffset)
/*****************************************************************************
 FUNCTION	: CopyColourRange

 PURPOSE	: 

 PARAMETERS	: psState					- Compiler state.
			  psRange
			  iStart
			  iEnd
			  uAlignOffset

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uStart;
	IMG_UINT32	uEnd;

	ASSERT(iStart >= 0);
	if (iEnd < iStart)
	{
		psRange->uStart = USC_UNDEF;
		psRange->uLength = 0;
		return;
	}

	uStart = (IMG_UINT32)iStart;
	ASSERT((uStart % 2) == uAlignOffset);
	psRange->uStart = (uStart - uAlignOffset) >> 1;

	uEnd = (IMG_UINT32)iEnd;
	ASSERT((uEnd % 2) == uAlignOffset);
	uEnd = (uEnd - uAlignOffset) >> 1;
	psRange->uLength = uEnd - psRange->uStart + 1;
}

static IMG_VOID AlignColourRange(PINTERMEDIATE_STATE	psState,
								 RANGE					asRange[],
								 IMG_UINT32				uStart, 
								 IMG_UINT32				uLength, 
								 IMG_UINT32				uAlignOffset)
/*****************************************************************************
 FUNCTION	: AlignColourRange

 PURPOSE	: Adjusts the start/length of a range so only aligned points are
			  inside it.

 PARAMETERS	: psState					- Compiler state.
			  asRange					- 
			  uStart					- Start of the range.
			  iLength					- Length of the range.
			  uAlignOffset

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_INT32	iEnd;
	IMG_INT32	iStart;
	IMG_INT32	iAlignedStart;
	IMG_INT32	iAlignedEnd;
	IMG_INT32	iEvenLength_End;
	IMG_INT32	iAlignOffset = (IMG_INT32)uAlignOffset;

	iStart = (IMG_INT32)uStart;
	iEnd = (IMG_INT32)(uStart + uLength - 1);

	iAlignedStart = iStart;
	if ((iStart % 2) != iAlignOffset)
	{
		iAlignedStart++;
	}

	iAlignedEnd = iEnd;
	if ((iEnd % 2) != iAlignOffset)
	{
		iAlignedEnd--;
	}

	CopyColourRange(psState, &asRange[1], iAlignedStart, iAlignedEnd, uAlignOffset);

	if ((iAlignedEnd + 1) <= iEnd)
	{
		iEvenLength_End = iAlignedEnd;
	}
	else
	{
		iEvenLength_End = iAlignedEnd - 2;
	}
	CopyColourRange(psState, &asRange[0], iAlignedStart, iEvenLength_End, uAlignOffset);
}

static IMG_VOID SetColourRange(PINTERMEDIATE_STATE	psState,
							   PCOLOUR_RANGE		psRange, 
							   COLOUR_TYPE			eType, 
							   IMG_UINT32			uStart, 
							   IMG_UINT32			uLength)
/*****************************************************************************
 FUNCTION	: SetColourRange

 PURPOSE	: Set the parameters for a range of colours.

 PARAMETERS	: psState			- Compiler state.
			  psRange			- Range to initialize.
			  eType				- Colour type.
			  uStart			- Start of the range.
			  uLength			- Length of the range.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	i;

	psRange->eType = eType;
	psRange->uBankFlag = 1 << eType;

	for (i = 0; i < 2; i++)
	{
		psRange->asRanges[HWREG_ALIGNMENT_NONE][i].uStart = uStart;
		psRange->asRanges[HWREG_ALIGNMENT_NONE][i].uLength = uLength;
	}

	AlignColourRange(psState, psRange->asRanges[HWREG_ALIGNMENT_EVEN], uStart, uLength, 0 /* uAlignOffset */);
	AlignColourRange(psState, psRange->asRanges[HWREG_ALIGNMENT_ODD], uStart, uLength, 1 /* uAlignOffset */);
}

static IMG_VOID ResetRAGColState(PINTERMEDIATE_STATE psState,
                                 PRAGCOL_STATE* ppsRegState,
								 FUNCGROUP eFuncGroup)
/*****************************************************************************
 FUNCTION   : ResetRegisterState

 PURPOSE    : Reset register allocation data structures.

 PARAMETERS : psState           - Compiler state.
              ppsRegState       - Initialised with the allocated structure.
			  eFuncGroup		- The function type to allocate

 RETURNS    : Nothing.
*****************************************************************************/
{
    PRAGCOL_STATE psRegState = *ppsRegState;
	PREGALLOC_DATA psRAData;
    IMG_UINT32 uNode;
    IMG_UINT32 uNumAvailRegs;
    IMG_UINT32 uNumAvailPARegs;
	IMG_UINT32 uNumAvailOutputRegs;
	IMG_UINT32 uNumAvailGPIRegs;
	IMG_BOOL bFirstPass;
	IMG_UINT32 uNrRegisters;
	IMG_UINT32 uNumAvailTemps;
	IMG_UINT32 uNumAvailTempsPostSplit = 0;
	IMG_UINT32 uNumAvailRegsPostSplit;

	uNumAvailGPIRegs = psState->psTargetFeatures->ui32NumInternalRegisters;
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		uNumAvailGPIRegs *= SCALAR_REGISTERS_PER_F32_VECTOR;
	}
	#endif //defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)

	/*
		Set up information about the number of registers available.
	*/
	if (eFuncGroup == FUNCGROUP_MAIN)
	{
		uNumAvailPARegs = psState->sHWRegs.uNumPrimaryAttributes;
		uNumAvailOutputRegs = psState->sHWRegs.uNumOutputRegisters;
		uNumAvailTemps = psState->psSAOffsets->uNumAvailableTemporaries;
		if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL)
		{
			uNumAvailTempsPostSplit = psState->psSAOffsets->uNumAvailableTemporaries / psState->sShader.psPS->uPostSplitRate;
		}
	}
	else
	{
		uNumAvailOutputRegs = 0;
		uNumAvailPARegs = psState->psSAOffsets->uInRegisterConstantOffset;
		uNumAvailPARegs += psState->psSAOffsets->uInRegisterConstantLimit;
		
		#if defined(OUTPUT_USPBIN)
		if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
		{
			/*
				Keep secondary attributes reserved for the USP to use for texture state
				as reserved.
			*/
			ASSERT(uNumAvailPARegs >= psState->sSAProg.uNumSAsReservedForTextureState);
			uNumAvailPARegs -= psState->sSAProg.uNumSAsReservedForTextureState;
		}
		#endif /* defined(OUTPUT_USPBIN) */

		if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_UNIFIED_TEMPS_AND_PAS)
		{
			/*
				For cores with this feature temporary registers can't be allocated for the
				secondary update program.
			*/
			uNumAvailTemps = 0;
			uNumAvailTempsPostSplit = 0;
		}
		else
		{
			uNumAvailTemps = psState->psSAOffsets->uNumAvailableTemporaries;
			if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL)
			{
				uNumAvailTempsPostSplit = psState->psSAOffsets->uNumAvailableTemporaries / psState->sShader.psPS->uPostSplitRate;
			}
		}
	}

    /* Set some basic values */
    uNumAvailRegs = REGALLOC_PRIMATTR_START;
	uNumAvailRegs += uNumAvailPARegs;
	uNumAvailRegs += uNumAvailOutputRegs;
	uNumAvailRegs += uNumAvailGPIRegs;
	uNumAvailRegs += uNumAvailTemps;

	uNumAvailRegsPostSplit = REGALLOC_PRIMATTR_START + uNumAvailPARegs + uNumAvailOutputRegs + uNumAvailGPIRegs + uNumAvailTempsPostSplit;

	uNrRegisters = REGALLOC_PRIMATTR_START;
	uNrRegisters += uNumAvailPARegs;
	uNrRegisters += uNumAvailOutputRegs;
	uNrRegisters += uNumAvailGPIRegs;
	uNrRegisters += psState->uNumRegisters;

	/* Reallocate the state on the first pass or if the number of registers has changed */
    if (psRegState == NULL ||
		uNrRegisters != psRegState->sRAData.uNrRegisters)
	{
        AllocateRAGColState(psState, uNrRegisters, ppsRegState);
        psRegState = *ppsRegState;

		bFirstPass = IMG_TRUE;
	}
	else
	{
		bFirstPass = IMG_FALSE;
	}
	psRAData = &(psRegState->sRAData);

    /* Initialise values */
	psRAData->eFuncGroup = eFuncGroup;
	psRAData->uTotalAvailRegs = uNumAvailRegs;
    psRAData->auAvailRegsPerType[COLOUR_TYPE_PRIMATTR] = uNumAvailPARegs;
	psRAData->auAvailRegsPerType[COLOUR_TYPE_OUTPUT] = uNumAvailOutputRegs;
	psRAData->auAvailRegsPerType[COLOUR_TYPE_GPI] = uNumAvailGPIRegs;
	psRAData->auAvailRegsPerType[COLOUR_TYPE_TEMP] = uNumAvailTemps;

	psRAData->uOutputStart = REGALLOC_PRIMATTR_START + uNumAvailPARegs;
	psRAData->uGPIStart = (REGALLOC_PRIMATTR_START +
						   uNumAvailPARegs +
						   uNumAvailOutputRegs);
	psRAData->uTempStart = (REGALLOC_PRIMATTR_START +
							uNumAvailPARegs +
							uNumAvailOutputRegs +
							uNumAvailGPIRegs);
	psRAData->uMaximumRangeLength = max(max(uNumAvailPARegs, uNumAvailOutputRegs), uNumAvailTemps);

	/*
		Initialize information about registers which are shader results.
	*/
	if (eFuncGroup == FUNCGROUP_MAIN)
	{
		psRAData->psFixedVRegList = &psState->sFixedRegList;
	}
	else
	{
		psRAData->psFixedVRegList = &psState->sSAProg.sFixedRegList;
	}

	if (!bFirstPass)
	{
		IntfGraphDelete(psState, psRegState->psIntfGraph);
	}
	psRegState->psIntfGraph = IntfGraphCreate(psState, psRAData->uNrRegisters);

	for (uNode = 0; uNode < psRAData->uNrRegisters; uNode++)
	{
		PNODE_DATA	psNode = &psRegState->asNodes[uNode];

		if (!bFirstPass)
		{
			if (psNode->uColourCount > 1)
			{
				UscFree(psState, psNode->asColours);
				psNode->asColours = NULL;
			}
		}

		psNode->uColourCount = 1;
		psNode->asColours = &psNode->sSingleColour;
		psNode->asColours[0].eType = COLOUR_TYPE_UNDEF;
		psNode->sReservedColour.eType = COLOUR_TYPE_UNDEF;
		psNode->uAddress = (IMG_UINT32)-1;
		memset(psNode->auFlags, 0, sizeof(psNode->auFlags));
		psNode->uBankFlags = BANK_FLAG_ALL;
	}

	/*
		Flag intermediate registers which represent secondary attributes.
	*/
	if (eFuncGroup == FUNCGROUP_MAIN)
	{
		IMG_UINT32	uTemp;

		for (uTemp = 0; uTemp < psState->uNumRegisters; uTemp++)
		{
			PUSEDEF_CHAIN	psTemp;

			psTemp = UseDefGet(psState, USEASM_REGTYPE_TEMP, uTemp);
			if (psTemp != NULL && IsUniform(psState, psTemp))
			{
				IMG_UINT32	uNode;
				PNODE_DATA	psNode;

				uNode = RegisterToNode(psRAData, USEASM_REGTYPE_TEMP, uTemp);
				psNode = &psRegState->asNodes[uNode];
				SetBit(psNode->auFlags, NODE_FLAG_ISSECATTR, 1);
			}
		}
	}
	
	/*
		Flag variables that need fixed hardware registers as precoloured.
	*/
	if (psRAData->psFixedVRegList != NULL)
	{
		PUSC_LIST_ENTRY	psListEntry;

		for (psListEntry = psRAData->psFixedVRegList->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
		{
			PFIXED_REG_DATA psFixedReg = IMG_CONTAINING_RECORD(psListEntry, PFIXED_REG_DATA, sListEntry);

			if (psFixedReg->uVRegType == USEASM_REGTYPE_TEMP)
			{
				IMG_UINT32	uConsecutiveRegIdx;

				for (uConsecutiveRegIdx = 0; 
					 uConsecutiveRegIdx < psFixedReg->uConsecutiveRegsCount; 
					 uConsecutiveRegIdx++)
				{
					IMG_UINT32	uShaderOutputNode;
					PNODE_DATA	psShaderOutput;
					
					uShaderOutputNode = RegisterToNode(psRAData, psFixedReg->uVRegType, psFixedReg->auVRegNum[uConsecutiveRegIdx]);
					psShaderOutput = &psRegState->asNodes[uShaderOutputNode];

					SetBit(psShaderOutput->auFlags, NODE_FLAG_REFERENCED, 1);

					#if defined(OUTPUT_USPBIN)
					/*
						For nodes which can be given multiple colours increase the size of the array
						holding the assigned colours.
					*/
					if (psFixedReg->uNumAltPRegs > 0)
					{
						IMG_UINT32	uColourIdx;

						ASSERT(psShaderOutput->uColourCount == 1);
						psShaderOutput->uColourCount = 1 + psFixedReg->uNumAltPRegs;
						psShaderOutput->asColours = 
							UscAlloc(psState, sizeof(psShaderOutput->asColours[0]) * psShaderOutput->uColourCount);
						for (uColourIdx = 0; uColourIdx < psShaderOutput->uColourCount; uColourIdx++)
						{
							psShaderOutput->asColours[uColourIdx].eType = COLOUR_TYPE_UNDEF;
						}
					}
					#endif /* defined(OUTPUT_USPBIN) */

					/*
						Set the hardware register banks available for shader outputs so we don't
						try to coalesce them with other intermediate registers which can't be
						coloured to the right fixed hardware register bank.
					*/
					if (psFixedReg->sPReg.uType == USEASM_REGTYPE_PRIMATTR ||
						psFixedReg->sPReg.uType == USEASM_REGTYPE_SECATTR)
					{
						psShaderOutput->uBankFlags = BANK_FLAG_PRIMATTR;
					}
					else if (psFixedReg->sPReg.uType == USEASM_REGTYPE_OUTPUT)
					{
						psShaderOutput->uBankFlags = BANK_FLAG_OUTPUT;
					}
					else
					{
						ASSERT(psFixedReg->sPReg.uType == USEASM_REGTYPE_TEMP);
						psShaderOutput->uBankFlags = BANK_FLAG_TEMP | BANK_FLAG_GPI;
					}
				}
			}
		}
	}

	/*
		When the MSAA translucency flag is set primary attributes can't be
		written in the main program.
	*/
	psRegState->bNeverWritePrimaryAttributes = IMG_FALSE;
	if (eFuncGroup == FUNCGROUP_MAIN && (psState->uCompilerFlags & UF_MSAATRANS) != 0)
	{
		psRegState->bNeverWritePrimaryAttributes = IMG_TRUE;
	}

	/*
		Set up ranges of colours.
	*/
	if (
			eFuncGroup == FUNCGROUP_MAIN || 
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_UNIFIED_TEMPS_AND_PAS) != 0
	   )
	{
		/*
			For general variables use all registers.
		*/
		psRegState->sGeneralRange.uRangeCount = 4;
		SetColourRange(psState, 
						&psRegState->sGeneralRange.asRanges[0], 
						COLOUR_TYPE_GPI, 
						0 /* uStart */, 
						uNumAvailGPIRegs);
		SetColourRange(psState, 
						&psRegState->sGeneralRange.asRanges[1], 
						COLOUR_TYPE_PRIMATTR, 
						0 /* uStart */, 
						uNumAvailPARegs);
		SetColourRange(psState, 
						&psRegState->sGeneralRange.asRanges[2], 
						COLOUR_TYPE_OUTPUT, 
						0 /* uStart */, 
						uNumAvailOutputRegs);
		SetColourRange(psState, 
						&psRegState->sGeneralRange.asRanges[3], 
						COLOUR_TYPE_TEMP, 
						0 /* uStart */, 
						uNumAvailTemps);

		/*
			For the sample rate program reduce the number of temporary registers available.
		*/
		psRegState->sPostSplitGeneralRange = psRegState->sGeneralRange;
		SetColourRange(psState,
					   &psRegState->sPostSplitGeneralRange.asRanges[2], 
					   COLOUR_TYPE_TEMP, 
					   0 /* uStart */, 
					   uNumAvailTempsPostSplit);
	}
	else
	{
		IMG_UINT32	uSAStartOffset = psState->psSAOffsets->uInRegisterConstantOffset;
		IMG_UINT32	uLoadedSAEnd;

		/*
			For general variables three possible ranges. We want to use temporary registers in preference
			to new secondary attributes (those not already loaded with constant data by the driver) since
			temporary registers are allocated only while the secondary update program is running whereas
			new secondary attributes will remain allocated until all the associated primary tasks
			finish.
		*/
		psRegState->sGeneralRange.uRangeCount = 3;

		/*
			First all secondary attributes which are loaded with constants by the driver.
		*/
		uLoadedSAEnd = uSAStartOffset + psState->sSAProg.uConstSecAttrCount;
		SetColourRange(psState,
					   &psRegState->sGeneralRange.asRanges[0], 
					   COLOUR_TYPE_PRIMATTR, 
					   0 /* uStart */, 
					   uLoadedSAEnd);

		ASSERT(uLoadedSAEnd <= uNumAvailPARegs);

		/*
			Next temporary registers.
		*/
		SetColourRange(psState,
					   &psRegState->sGeneralRange.asRanges[1], 
					   COLOUR_TYPE_TEMP, 
					   0 /* uStart */, 
				       psState->psSAOffsets->uNumAvailableTemporaries);
		
		/*
			Finally secondary attributes not loaded with constants.
		*/
		SetColourRange(psState,
					   &psRegState->sGeneralRange.asRanges[2], 
					   COLOUR_TYPE_PRIMATTR, 
					   uLoadedSAEnd, 
					   uNumAvailPARegs - uLoadedSAEnd);

		/*
			Splitting into pixel rate/sample rate fragments applies to the main program only.
		*/
		psRegState->sPostSplitGeneralRange.uRangeCount = 0;
	}

	/*
		Range of colours to use for variables which can be coloured to primary attributes only.
	*/
	psRegState->sPAOnlyRange.uRangeCount = 1;
	SetColourRange(psState,
				   &psRegState->sPAOnlyRange.asRanges[0], 
				   COLOUR_TYPE_PRIMATTR, 
				   0 /* uStart */, 
				   uNumAvailPARegs);
}

static
IMG_UINT32 GetSpillTempRegister(PINTERMEDIATE_STATE psState, PRAGCOL_STATE psRegState)
/*****************************************************************************
 FUNCTION   : GetSpillTempRegister

 PURPOSE    : Allocate a temporary register for use in a spill instruction.

 PARAMETERS : psState       - Compiler state.
			  psRegState	- Register allocator state.

 RETURNS    : The temporary register number.
*****************************************************************************/
{
	IMG_UINT32	uTempNum;

	uTempNum = GetNextRegister(psState);
	VectorSet(psState, &psRegState->sNodesUsedInSpills, uTempNum, 1);
	return uTempNum;
}

static
IMG_VOID SetSpillAddress(PINTERMEDIATE_STATE	psState,
						 PRAGCOL_STATE			psRegState,
						 PINST					psSpillInst,
						 IMG_UINT32				uSrcIdx,
						 IMG_UINT32				uSpillAddress)
/*****************************************************************************
 FUNCTION   : SetSpillAddress

 PURPOSE    : Set up the argument to a LD or ST instruction which contains the
			  offset into the spill area to load or store.

 PARAMETERS : psState       - Compiler state.
              psRegState	- Module state.
			  psSpillInst	- LD or ST instruction.
			  psArg			- Argument to setup.
			  uSpillAddress	- Offset into the spill area to load or store.

 RETURNS    : Nothing.
*****************************************************************************/
{
	PSPILL_STATE	psSpillState = psRegState->psSpillState;
	IMG_UINT32		uOffsetArgType, uOffsetArgNumber;

	if (uSpillAddress < psSpillState->uSpillAreaOffsetsSACount)
	{
		/*
			Use a secondary attribute.
		*/
		GetDriverLoadedConstantLocation(psState, 
										psSpillState->asSpillAreaOffsetsSANums[uSpillAddress].psConst,
										&uOffsetArgType,
										&uOffsetArgNumber);
	}
	else
	{
		uOffsetArgType = USEASM_REGTYPE_IMMEDIATE;
		/* Set the offset into the spill memory in dwords. */
		uOffsetArgNumber = uSpillAddress;

		if (!psRegState->psSpillState->bSpillUseImmediateOffsets)
		{
			PSHORT_REG	psReservedTemp;

			/*
				Reserve a temporary register here. After register allocation we'll add a LIMM
				instruction to load the memory offset into the reserved register then use the
				reserved register as the offset argument to this instruction.
			*/
			psSpillInst->uTempCount = 1;
			psSpillInst->asTemp = UscAlloc(psState, sizeof(psSpillInst->asTemp[0]));
			psReservedTemp = psSpillInst->asTemp;
			psReservedTemp->eType = USEASM_REGTYPE_TEMP;
			psReservedTemp->uNumber = GetSpillTempRegister(psState, psRegState);
		}
	}

	SetSrc(psState, psSpillInst, uSrcIdx, uOffsetArgType, uOffsetArgNumber, UF_REGFORMAT_F32);
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_VOID SaveInternalRegister_Vec(PINTERMEDIATE_STATE	psState, 
								  PCODEBLOCK			psBlock,
								  PINST					psInsertBeforeInst, 
								  IMG_UINT32			uIRegNum,
								  IMG_UINT32			uChanCount,
								  IMG_UINT32			auChansToSave[],
								  IMG_BOOL				abSkipInvalidChan[],
								  IMG_UINT32			auTempNums[])
/*****************************************************************************
 FUNCTION   : SaveInternalRegister_Vec

 PURPOSE    : Copy from an internal register into a temporary register.

 PARAMETERS : psState			- Compiler state.
			  psBlock			- Flow control block to insert the new instruction into.
              psInsertBeforeInst
								- Instruction to insert the new instruction before.
			  uIRegNum			- Internal register to copy from.
			  uChanCount		- Count of channels to save (maximum of two).
			  auChansToSave		- Channels to save from the internal register.
			  abSkipInvalidChan	- For each channel: FALSE if that channel is used for
								gradient calculations.
			  auTempNums		- Temporary register to save each channel into.

 RETURNS    : Nothing.
*****************************************************************************/
{
	PINST		psNewInst;
	IMG_UINT32	uRegIdx;
	IMG_UINT32	uSwizzle;

	psNewInst = AllocateInst(psState, psInsertBeforeInst);
	
	SetOpcodeAndDestCount(psState, psNewInst, IVMOV, uChanCount);

	/*
		If any channel is used later for gradient calculations then don't set SKIPINVALID on
		the save instruction.
	*/
	SetBit(psNewInst->auFlag, INST_SKIPINV, 1);
	for (uRegIdx = 0; uRegIdx < uChanCount; uRegIdx++)
	{
		if (!abSkipInvalidChan[uRegIdx])
		{
			SetBit(psNewInst->auFlag, INST_SKIPINV, 0);
		}
	}

	/*
		Create an array of destinations from the temporary registers to save into.
	*/
	for (uRegIdx = 0; uRegIdx < uChanCount; uRegIdx++)
	{
		SetDest(psState, psNewInst, uRegIdx, USEASM_REGTYPE_TEMP, auTempNums[uRegIdx], UF_REGFORMAT_F32);	
		psNewInst->auDestMask[uRegIdx] = USC_ALL_CHAN_MASK;
	}

	psNewInst->asArg[0].uType = USC_REGTYPE_UNUSEDSOURCE;
	psNewInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F32;
	
	/*
		Create an array of sources from the channels of the internal register.
	*/
	for (uRegIdx = 0; uRegIdx < 4; uRegIdx++)
	{
		psNewInst->asArg[1 + uRegIdx].uType = USEASM_REGTYPE_FPINTERNAL;
		psNewInst->asArg[1 + uRegIdx].uNumber = uIRegNum * SGXVEC_INTERNAL_REGISTER_WIDTH_IN_DWORDS + uRegIdx;				
	}

	/*
		Create a source swizzle to select the internal register channels to save.
	*/
	uSwizzle = USEASM_SWIZZLE(X, Y, Z, W);
	for (uRegIdx = 0; uRegIdx < uChanCount; uRegIdx++)
	{
		uSwizzle = SetChan(uSwizzle, uRegIdx, USEASM_SWIZZLE_SEL_X + auChansToSave[uRegIdx]);
	}
	psNewInst->u.psVec->auSwizzle[0] = uSwizzle;

	InsertInstBefore(psState, psBlock, psNewInst, psInsertBeforeInst);

	/*
		Mark the save destinations as requiring consecutive hardware register numbers.
	*/
	MakeGroup(psState, psNewInst->asDest, uChanCount, HWREG_ALIGNMENT_EVEN);
}

static
IMG_VOID RestoreInternalRegister_Vec(PINTERMEDIATE_STATE	psState, 	 
									 PCODEBLOCK				psBlock,
									 PINST					psSrcLineInst,
									 PINST					psInsertBeforeInst, 
									 IMG_UINT32				uIRegNum,
									 IMG_UINT32				uRestoreMask,
									 IMG_UINT32				uRestoreChanCount,
									 IMG_BOOL				bSkipInvalid,
									 IMG_UINT32				auTempNums[])
/*****************************************************************************
 FUNCTION   : RestoreInternalRegister_Vec

 PURPOSE    : Copy from a temporary register into an internal register.

 PARAMETERS : psState			- Compiler state.
			  psBlock			- Flow control block to insert the new instruction into.
			  psSrcLineInst		- Instruction to copy the associated input src line from.
              psInsertBeforeInst
								- Instruction to insert the new instruction before.
			  uIRegNum			- Internal register to copy to.
			  uRestoreMask		- Mask of channels to restore.
			  uRestoreChanCount	- Count of set bits in the mask.
			  bSkipInvalid		- TRUE if none of channels are required for gradient
								calculations.
			  auTempNums		- Temporary register to restore each channel from.

 RETURNS    : Nothing.
*****************************************************************************/
{
	IMG_UINT32	uRegIdx;
	PINST		psNewInst;
	IMG_UINT32	uChan;
	IMG_UINT32	uSwizzle;
	
	psNewInst = AllocateInst(psState, psSrcLineInst);
	InsertInstBefore(psState, psBlock, psNewInst, psInsertBeforeInst);
	SetOpcodeAndDestCount(psState, psNewInst, IVMOV, SGXVEC_INTERNAL_REGISTER_WIDTH_IN_DWORDS);

	/*
		If none of the channels are used later on for gradient calculations then the restore
		instruction can be skipped for invalid pixels.
	*/
	if (bSkipInvalid)
	{
		SetBit(psNewInst->auFlag, INST_SKIPINV, 1);
	}

	/*
		Create the array of instruction destinations for all the channels of the
		internal register.
	*/
	for (uRegIdx = 0; uRegIdx < 4; uRegIdx++)
	{
		SetDest(psState, 
			    psNewInst, 
				uRegIdx, 
				USEASM_REGTYPE_FPINTERNAL, 
				(uIRegNum * SGXVEC_INTERNAL_REGISTER_WIDTH_IN_DWORDS) + uRegIdx,
				UF_REGFORMAT_F32);

		if ((uRestoreMask & (1U << uRegIdx)) != 0)
		{	
			psNewInst->auLiveChansInDest[uRegIdx] = USC_ALL_CHAN_MASK;
			psNewInst->auDestMask[uRegIdx] = USC_ALL_CHAN_MASK;
		}
		else
		{
			psNewInst->auLiveChansInDest[uRegIdx] = 0;
			psNewInst->auDestMask[uRegIdx] = 0;
		}
	}

	psNewInst->asArg[0].uType = USC_REGTYPE_UNUSEDSOURCE;
	
	/*
		Create an array of sources with all the temporary registers to restore from.
	*/
	for (uRegIdx = 0; uRegIdx < uRestoreChanCount; uRegIdx++)
	{
		SetSrc(psState, psNewInst, 1 + uRegIdx, USEASM_REGTYPE_TEMP, auTempNums[uRegIdx], UF_REGFORMAT_F32);
	}
	for (; uRegIdx < SGXVEC_INTERNAL_REGISTER_WIDTH_IN_DWORDS; uRegIdx++)
	{
		SetSrcUnused(psState, psNewInst, 1 + uRegIdx);
	}

	/*
		Create a source swizzle to map from the source registers (which are in consecutive channels) to
		the (non-consecutive) destination registers.
	*/
	uChan = 0;
	uSwizzle = USEASM_SWIZZLE(X, Y, Z, W);
	for (uRegIdx = 0; uRegIdx < SGXVEC_INTERNAL_REGISTER_WIDTH_IN_DWORDS; uRegIdx++)
	{
		if ((uRestoreMask & (1U << uRegIdx)) != 0)
		{
			uSwizzle = SetChan(uSwizzle, uRegIdx, USEASM_SWIZZLE_SEL_X + uChan);
			uChan++;
		}
	}
	psNewInst->u.psVec->auSwizzle[0] = uSwizzle;
	
	psNewInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F32;

	/*
		Mark the restore sources as requiring consecutive hardware register numbers.
	*/
	MakeGroup(psState, psNewInst->asArg + 1, uRestoreChanCount, HWREG_ALIGNMENT_EVEN);
}

static
IMG_VOID SpillInternalRegisters_Vec(PINTERMEDIATE_STATE	psState,
									PRAGCOL_STATE		psRegState,
									PIREG_STATE			psIRegState,
									PCODEBLOCK			psBlock,
								    PINST				psSaveInst,
								    PINST				psRestoreInst)
/*****************************************************************************
 FUNCTION   : SpillInternalRegisters_Vec

 PURPOSE    : Save and restore the contents of internal registers around a
			  a series of memory load/store instructions.

 PARAMETERS : psState       - Compiler state.
			  psRegState	- Register allocator state.
			  psBlock		- Flow control block to insert the instructions into.
			  psIRegState	- Details of the internal registers which are live
							before the memory load/store.
              psSaveInst	- Instruction to save the internal registers before.
			  psRestoreInst	- Instruction to restore the internal registers after.

 RETURNS    : Nothing.
*****************************************************************************/
{
	IMG_UINT32	uIRegNum;
	PINST		psRestoreInsertPoint;

	psRestoreInsertPoint = psRestoreInst->psNext;

	ASSERT(psIRegState->uC10Mask == 0);

	for (uIRegNum = 0; uIRegNum < psState->psTargetFeatures->ui32NumInternalRegisters; uIRegNum++)
	{
		IMG_UINT32	uLiveMask;
		IMG_UINT32	uSkipInvalidMask;
		IMG_UINT32	auLiveChans[SGXVEC_INTERNAL_REGISTER_WIDTH_IN_DWORDS];
		IMG_BOOL	abChanSkipInvalid[SGXVEC_INTERNAL_REGISTER_WIDTH_IN_DWORDS];
		IMG_UINT32	auTempNums[SGXVEC_INTERNAL_REGISTER_WIDTH_IN_DWORDS];
		IMG_UINT32	uLiveChanCount;
		IMG_UINT32	uChan;
		IMG_BOOL	bSkipInvalid;

		/*
			Get the mask of channels live in the internal register at this point.
		*/
		uLiveMask = psIRegState->uLiveMask >> (uIRegNum * SGXVEC_INTERNAL_REGISTER_WIDTH_IN_DWORDS);
		uLiveMask &= ((1U << SGXVEC_INTERNAL_REGISTER_WIDTH_IN_DWORDS) - 1);

		if (uLiveMask == 0)
		{
			continue;
		}

		/*
			Get the mask of channels in the internal register not used later on for gradient calculations.
		*/
		uSkipInvalidMask = psIRegState->uSkipInvalidMask >> (uIRegNum * SGXVEC_INTERNAL_REGISTER_WIDTH_IN_DWORDS);
		uSkipInvalidMask &= ((1U << SGXVEC_INTERNAL_REGISTER_WIDTH_IN_DWORDS) - 1);

		/*
			Convert the mask of live channels to a list.
		*/
		uLiveChanCount = 0;
		for (uChan = 0; uChan < SGXVEC_INTERNAL_REGISTER_WIDTH_IN_DWORDS; uChan++)
		{
			if ((uLiveMask & (1U << uChan)) != 0)
			{
				auLiveChans[uLiveChanCount] = uChan;

				/*
					Record if this channel is not used later for gradient gradients.
				*/
				abChanSkipInvalid[uLiveChanCount] = IMG_FALSE;
				if (uSkipInvalidMask & (1U << uChan))
				{
					abChanSkipInvalid[uLiveChanCount] = IMG_TRUE;
				}

				uLiveChanCount++;
			}
		}

		/*
			Save channels from the internal register in batches of two.
		*/
		for (uChan = 0; uChan < uLiveChanCount; uChan += 2)
		{
			IMG_UINT32	uInstChanCount;
			IMG_UINT32	uRegIdx;

			if ((uChan + 1) < uLiveChanCount)
			{
				uInstChanCount = 2;
			}
			else
			{
				uInstChanCount = 1;
			}

			/*
				Allocate temporary registers to save the channels into.
			*/
			for (uRegIdx = 0; uRegIdx < uInstChanCount; uRegIdx++)
			{
				auTempNums[uChan + uRegIdx] = GetSpillTempRegister(psState, psRegState);
			}

			SaveInternalRegister_Vec(psState,
									 psBlock,
									 psSaveInst,
									 uIRegNum,
									 uInstChanCount,
									 auLiveChans + uChan,
									 abChanSkipInvalid + uChan,
									 auTempNums + uChan);
		}

		/*
			Check if all the channels aren't used later on for gradient calculations.
		*/
		bSkipInvalid = IMG_FALSE;
		if ((uSkipInvalidMask & uLiveMask) == uLiveMask)
		{
			bSkipInvalid = IMG_TRUE;
		}

		/*
			Restore all the channels into the internal register.
		*/
		RestoreInternalRegister_Vec(psState,
									psBlock,
									psRestoreInst,
									psRestoreInsertPoint,
									uIRegNum,
									uLiveMask,
									uLiveChanCount,
									bSkipInvalid,
									auTempNums);
	}
}

#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static
PINST MakeInternalRegisterCopyInst(PINTERMEDIATE_STATE psState, PINST psSrcLineInst, UF_REGFORMAT eCopyFormat)
/*****************************************************************************
 FUNCTION   : MakeInternalRegisterCopyInst

 PURPOSE    : Create a suitable instruction for copying data to or from an
			  internal register.

 PARAMETERS : psState       - Compiler state.
			  psSrcLineInst	- Instruction to take the source line to associate
							with the new instruction from.
			  eCopyFormat	- Format of the data in the internal register.

 RETURNS    : A pointer to the created instruction.
*****************************************************************************/
{
	PINST	psInst;

	psInst = AllocateInst(psState, psSrcLineInst);

	if (eCopyFormat == UF_REGFORMAT_C10)
	{
		SetOpcode(psState, psInst, ISOPWM);

		/*
			RESULT = SRC1 * ONE + SRC2 * ZERO
		*/
		psInst->u.psSopWm->uSel1 = USEASM_INTSRCSEL_ZERO;
		psInst->u.psSopWm->bComplementSel1 = IMG_TRUE;
		psInst->u.psSopWm->uSel2 = USEASM_INTSRCSEL_ZERO;
		psInst->u.psSopWm->bComplementSel2 = IMG_FALSE;
		psInst->u.psSopWm->uCop = USEASM_INTSRCSEL_ADD;
		psInst->u.psSopWm->uAop = USEASM_INTSRCSEL_ADD;

		/*
			Set the second (unreferenced) argument to a safe value.
		*/
		InitInstArg(&psInst->asArg[1]);
		psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psInst->asArg[1].uNumber = 0;
	}
	else
	{
		SetOpcode(psState, psInst, IMOV);
	}
			
	return psInst;
}

static
IMG_VOID SpillInternalRegister(PINTERMEDIATE_STATE	psState,
							   PRAGCOL_STATE		psRegState,
							   PIREG_STATE			psIRegState,
							   PCODEBLOCK			psBlock,
							   PINST				psSaveInsertPoint,
							   PINST				psRestoreInsertPoint,
							   IMG_UINT32			uIRegNum,
							   UF_REGFORMAT			eIRegFormat)
/*****************************************************************************
 FUNCTION   : SpillInternalRegister

 PURPOSE    : Insert instructions to copy from an internal register to a
			  temporary register then back again.

 PARAMETERS : psState       - Compiler state.
			  psRegState	- Register allocator state.
			  psIRegState	- State of the internal registers.
			  psBlock		- Block to add the instructions to.
			  psSaveInsertPoint
							- Instruction to insert the copy from the internal
							register before.
			  psRestoreInsertPoint
							- Instruction to insert the copy to the internal
							register before.
			  uIRegNum		- Internal register to copy.
			  eIRegFormat	- Format of the data in the internal register.

 RETURNS    : Nothing.
*****************************************************************************/
{
	IMG_UINT32	uIRegSrc;
	PINST		psSaveInst;
	PINST		psRestoreInst;

	uIRegSrc = GetSpillTempRegister(psState, psRegState);

	psSaveInst = MakeInternalRegisterCopyInst(psState, psSaveInsertPoint, eIRegFormat);
	SetBit(psSaveInst->auFlag, INST_SKIPINV, GetBit(&psIRegState->uSkipInvalidMask, uIRegNum));
	SetDest(psState, psSaveInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uIRegSrc, eIRegFormat);
	SetSrc(psState, psSaveInst, 0 /* uSrcIdx */, USEASM_REGTYPE_FPINTERNAL, uIRegNum, eIRegFormat);
	InsertInstBefore(psState, psBlock, psSaveInst, psSaveInsertPoint);

	psRestoreInst = MakeInternalRegisterCopyInst(psState, psSaveInsertPoint, eIRegFormat);
	SetBit(psRestoreInst->auFlag, INST_SKIPINV, GetBit(&psIRegState->uSkipInvalidMask, uIRegNum));
	SetDest(psState, psRestoreInst, 0 /* uDestIdx */, USEASM_REGTYPE_FPINTERNAL, uIRegNum, eIRegFormat);
	SetSrc(psState, psRestoreInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uIRegSrc, eIRegFormat);
	InsertInstBefore(psState, psBlock, psRestoreInst, psRestoreInsertPoint);
}

static
IMG_VOID SaveInternalRegisterWChannel(PINTERMEDIATE_STATE	psState,
									  PIREG_STATE			psIRegState,
									  PCODEBLOCK			psBlock,
									  PINST					psInsertBeforeInst,
									  IMG_UINT32			uDestTempNum,
									  IMG_UINT32			uDestChan,
									  IMG_UINT32			uIRegNum)
/*****************************************************************************
 FUNCTION   : SaveInternalRegisterWChannel

 PURPOSE    : Copy a C10 channel from the W channel of an internal register to
			  a channel in a temporary register.

 PARAMETERS : psState       - Compiler state.
			  psIRegState	- State of the internal registers.
			  psBlock		- Block to add the instructions to.
			  psInsertBeforeInst
							- Instruction to insert the new instruction before.
			  uDestTempNum	- Temporary register to copy to.
			  uDestChan		- Channel to copy to.
			  uIRegNum		- Internal register to copy from.

 RETURNS    : Nothing.
*****************************************************************************/
{
	PINST	psNewInst;

	psNewInst = AllocateInst(psState, psInsertBeforeInst);

	SetOpcode(psState, psNewInst, IPCKC10C10);

	SetBit(psNewInst->auFlag, INST_SKIPINV, GetBit(&psIRegState->uSkipInvalidMask, uIRegNum));

	SetDest(psState, psNewInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uDestTempNum, UF_REGFORMAT_C10);
	psNewInst->auDestMask[0] = (1U << uDestChan);

	SetSrc(psState, psNewInst, 0 /* uSrcIdx */, USEASM_REGTYPE_FPINTERNAL, uIRegNum, UF_REGFORMAT_C10);
	SetPCKComponent(psState, psNewInst, 0 /* uArgIdx */, USC_W_CHAN);

	psNewInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
	psNewInst->asArg[1].uNumber = 0;

	InsertInstBefore(psState, psBlock, psNewInst, psInsertBeforeInst);
}

static
IMG_VOID RestoreInternalRegisterWChannel(PINTERMEDIATE_STATE	psState,
										 PIREG_STATE			psIRegState,
										 PCODEBLOCK				psBlock,
										 PINST					psSrcLineInst,
										 PINST					psInsertBeforeInst,
										 IMG_UINT32				uSrcTempNum,
										 IMG_UINT32				uSrcChan,
										 IMG_UINT32				uIRegNum)
/*****************************************************************************
 FUNCTION   : RestoreInternalRegisterWChannel

 PURPOSE    : Copy a C10 channel from a temporary register to the W channel
			  of an internal register.

 PARAMETERS : psState       - Compiler state.
			  psIRegState	- State of the internal registers.
			  psBlock		- Block to add the instructions to.
			  psSrcLineInst	- Instruction to copy the associated input src line from.
			  psInsertBeforeInst
							- Instruction to insert the new instruction before.
			  uSrcTempNum	- Temporary register to copy from.
			  uSrcChan		- Channel to copy from.
			  uIRegNum		- Internal register to copy to.

 RETURNS    : Nothing.
*****************************************************************************/
{
	PINST	psNewInst;

	psNewInst = AllocateInst(psState, psSrcLineInst);

	SetOpcode(psState, psNewInst, IPCKC10C10);

	SetBit(psNewInst->auFlag, INST_SKIPINV, GetBit(&psIRegState->uSkipInvalidMask, uIRegNum));

	SetDest(psState, psNewInst, 0 /* uDestIdx */, USEASM_REGTYPE_FPINTERNAL, uIRegNum, UF_REGFORMAT_C10);
	SetPartialDest(psState, psNewInst, 0 /* uDestIdx */, USEASM_REGTYPE_FPINTERNAL, uIRegNum, UF_REGFORMAT_C10);
	psNewInst->auDestMask[0] = USC_W_CHAN_MASK;

	SetSrc(psState, psNewInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uSrcTempNum, UF_REGFORMAT_C10);
	SetPCKComponent(psState, psNewInst, 0 /* uArgIdx */, uSrcChan);

	psNewInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
	psNewInst->asArg[1].uNumber = 0;

	InsertInstBefore(psState, psBlock, psNewInst, psInsertBeforeInst);
}

static
IMG_VOID SpillInternalRegisters_NonVec(PINTERMEDIATE_STATE	psState,
									   PRAGCOL_STATE	psRegState,
									   PIREG_STATE			psIRegState,
									   PCODEBLOCK			psBlock,
									   PINST				psSaveInst,
									   PINST				psRestoreInst)
/*****************************************************************************
 FUNCTION   : SpillInternalRegisters_NonVec

 PURPOSE    : Save and restore the contents of internal registers around a
			  a series of memory load/store instructions.

 PARAMETERS : psState       - Compiler state.
			  psRegState	- Register allocator state.
			  psIRegState	- Details of the internal registers which are live
							before the memory load/store.
			  psBlock		- Block to add the instructions to.
              psSaveInst	- Instruction to save the internal registers before.
			  psRestoreInst	- Instruction to restore the internal registers after.

 RETURNS    : Nothing.
*****************************************************************************/
{
	IMG_UINT32	uIRegNum;
	IMG_UINT32	uChan;
	IMG_UINT32	uAlphaChanReg;
	const IMG_UINT32 uChansPerReg = 3;
	PINST		psRestoreInsertPoint;

	uChan = uChansPerReg;
	uAlphaChanReg = USC_UNDEF;
	psRestoreInsertPoint = psRestoreInst->psNext;
	for (uIRegNum = 0; uIRegNum < psState->uGPISizeInScalarRegs; uIRegNum++)
	{
		if (psIRegState->uLiveMask & (1U << uIRegNum))
		{
			UF_REGFORMAT	eIRegFormat;

			if ((psIRegState->uC10Mask & (1U << uIRegNum)) != 0)
			{
				eIRegFormat = UF_REGFORMAT_C10;
			}
			else
			{
				eIRegFormat = UF_REGFORMAT_F32;
			}

			SpillInternalRegister(psState, 
								  psRegState,
								  psIRegState,
								  psBlock,
								  psSaveInst, 
								  psRestoreInsertPoint, 
								  uIRegNum, 
								  eIRegFormat);

			if (eIRegFormat == UF_REGFORMAT_C10)
			{
				if (!(uChan < uChansPerReg))
				{
					uAlphaChanReg = GetSpillTempRegister(psState, psRegState);
					uChan = 0;
				}
					
				SaveInternalRegisterWChannel(psState, 
											 psIRegState, 
											 psBlock, 
											 psSaveInst,
											 uAlphaChanReg,
											 uChan, 
											 uIRegNum);
				RestoreInternalRegisterWChannel(psState, 
												psIRegState, 
												psBlock, 
												psRestoreInst,
												psRestoreInsertPoint, 
												uAlphaChanReg, 
												uChan, 
												uIRegNum);

				uChan++;
			}
		}
	}
}

static
IMG_VOID SpillInternalRegisters(PINTERMEDIATE_STATE	psState,
								PRAGCOL_STATE		psRegState,
								PCODEBLOCK			psBlock,
								PINST				psSaveInst,
								PINST				psRestoreInst)
/*****************************************************************************
 FUNCTION   : SpillInternalRegisters

 PURPOSE    : Save and restore the contents of internal registers around a
			  a series of memory load/store instructions.

 PARAMETERS : psState       - Compiler state.
			  psRegstate	- Register allocator state.
			  psBlock		- Block to add the instructions to.
              psSaveInst	- Instruction to save the internal registers before.
			  psRestoreInst	- Instruction to restore the internal registers after.

 RETURNS    : Nothing.
*****************************************************************************/
{
	IREG_STATE	sIRegState;

	/*
		Get a vector of the internal registers live at this point and some information
		about the instructions writing each one.
	*/
	UseDefGetIRegStateBeforeInst(psState, psSaveInst, &sIRegState);

	/*
		Save and restore the internal registers around the memory access.
    */
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		SpillInternalRegisters_Vec(psState, psRegState, &sIRegState, psBlock, psSaveInst, psRestoreInst);
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		SpillInternalRegisters_NonVec(psState, psRegState, &sIRegState, psBlock, psSaveInst, psRestoreInst);
	}
}

static
IMG_VOID InsertSpill(PINTERMEDIATE_STATE psState,
					 PRAGCOL_STATE		 psRegState,
					 PCODEBLOCK			 psCodeBlock,
					 PINST				 psSpillInst,
					 PINST				 psInsertBeforeInst,
					 IMG_UINT32			 uDestSrc,
					 IMG_BOOL			 bLoad,
					 UF_REGFORMAT		 eDataFmt,
					 IMG_UINT32			 uSpillAddress,
					 IMG_UINT32			 uLiveChansInDest,
					 PINST*				 ppsNewInst)
/*****************************************************************************
 FUNCTION   : InsertSpill

 PURPOSE    : Insert a load or a save of a register around an instruction.

 PARAMETERS : psState             - Compiler state.
			  psCodeBlock         - Block which contains the instruction.
			  psSpillInst         - Instruction to spill for.
			  psInsertBeforeInst  - Instruction insertion reference point.
			  uDestSrc            - New register for the loaded/stored value.
			  bLoad               - TRUE if the register needs to be loaded.
			  eDataFmt            - Format of the data to load/store.
			  uSpillAddress		  - Location in the scratch area to store to/load from.
			  uLiveChansInDest	  - For a destination the mask of channels that were
								    live in the spilled register.
			  ppsNewInst		  - Returns the newly created instruction.

 RETURNS    : Nothing.
*****************************************************************************/
{
    PINST psNewInst;
   
    /*
		Do the load or store.
    */
	*ppsNewInst = psNewInst = AllocateInst(psState, psInsertBeforeInst);

    if (bLoad)
    {
        SetOpcode(psState, psNewInst, ISPILLREAD);
		SetDest(psState, psNewInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uDestSrc, eDataFmt);
    }
    else
    {
        SetOpcode(psState, psNewInst, ISPILLWRITE);
        psNewInst->asDest[0].uType = USC_REGTYPE_DUMMY;
        psNewInst->asDest[0].uNumber = 0;

		psNewInst->u.psMemSpill->uLiveChans = uLiveChansInDest;
    }
	psState->uOptimizationHint |= USC_COMPILE_HINT_LOCAL_LOADSTORE_USED;

	psNewInst->u.psMemSpill->bBypassCache = BypassCacheForModifiableShaderMemory(psState);
    SetBit(psNewInst->auFlag, INST_SPILL, 1);
	if (RequiresGradients(psSpillInst) == IMG_TRUE)
	{
		SetBit(psNewInst->auFlag, INST_SKIPINV, 0);
	}
	else
	{
		/* Inherit skipinv flag from cause */
		SetBit(psNewInst->auFlag, INST_SKIPINV, GetBit(psSpillInst->auFlag, INST_SKIPINV));
	}

	AppendToList(&psRegState->psSpillState->sSpillInstList, &psNewInst->sAvailableListEntry);

    psNewInst->asArg[MEMSPILL_BASE_ARGINDEX].uType = USEASM_REGTYPE_SECATTR;
    psNewInst->asArg[MEMSPILL_BASE_ARGINDEX].uNumber = psState->psSAOffsets->uScratchBase;

	SetSpillAddress(psState, 
					psRegState, 
					psNewInst, 
					MEMSPILL_OFFSET_ARGINDEX, 
					uSpillAddress);

    if (bLoad)
    {
        psNewInst->asArg[MEMSPILLREAD_RANGE_ARGINDEX].uType = USEASM_REGTYPE_IMMEDIATE;
        psNewInst->asArg[MEMSPILLREAD_RANGE_ARGINDEX].uNumber = 0;
        psNewInst->asArg[MEMSPILLREAD_DRC_ARGINDEX].uType = USEASM_REGTYPE_DRC;
        psNewInst->asArg[MEMSPILLREAD_DRC_ARGINDEX].uNumber = 0;
    }
    else
    {
		SetSrc(psState, psNewInst, MEMSPILLWRITE_DATA_ARGINDEX, USEASM_REGTYPE_TEMP, uDestSrc, eDataFmt); 
    }

    InsertInstBefore(psState, psCodeBlock, psNewInst, psInsertBeforeInst);
}

static
IMG_BOOL AddRegToInstGroup(PINTERMEDIATE_STATE psState,
                           PRAGCOL_STATE psRegState,
                           PINST psInst,
						   IMG_UINT32 uMoeArg,
                           IMG_UINT32 uInstArg,
                           PREGISTER_GROUP_DATA psGData,
                           IMG_PUINT32 puNode)
/*****************************************************************************
 FUNCTION   : AddRegToInstGroup

 PURPOSE    : Add a register to the group formed for the operand of an instruction.

 PARAMETERS : psState           - The compiler state
              psRegState        - Register information
              psInst            - Instruction to group for.
              uArg              - Operand to to group (0: dest, 1..3: src0..src2)
              psGData           - Register grouping data.

 OUTPUT     : puNode            - The node added to a group (-1 if no node was
                                  added to a group).

 RETURNS    : IMG_TRUE if the register was added to, or already in, a group.
*****************************************************************************/
{
    IMG_UINT32 uNode = UINT_MAX;
	PREGISTER_GROUP psNodeGroup;
    IMG_UINT32 uPrevNode = UINT_MAX;
    PARG psArg;
    PARG psPrevArg;

    /* Sanity checks. */
    ASSERT(psState != NULL);
    ASSERT(psRegState != NULL);
    ASSERT(psInst != NULL);
    ASSERT(uMoeArg < USC_MAX_MOE_OPERANDS);
    ASSERT(uInstArg < (psInst->uArgumentCount + 1));

    /* zero out the return values */
    (*puNode) = UINT_MAX;

	/* Skip an unwritten destination. */
	if (uInstArg == 0 && psInst->uDestCount == 0)
	{
		return IMG_TRUE;
	}

    /* Get the register */
    if (uInstArg == 0)
        psArg = &psInst->asDest[0];
    else
        psArg = &psInst->asArg[uInstArg - 1];

	if (RegIsNode(&psRegState->sRAData, psArg))
	{
		/* Get the node in the register graph */
		uNode = ArgumentToNode(&psRegState->sRAData, psArg);
	}

    /* Previous operand in current position */
    psPrevArg = psGData->psPrevReg[uMoeArg];

    /* Don't do anything if this operand is the same as the previous operand */
    if (psPrevArg == psArg)
    {
        return IMG_TRUE;
    }

    /* Compare new register with previous register (tests from groupinst.c:CheckArg) */
    if (psPrevArg != NULL)
    {
		IMG_BOOL	bPrevNodePrecoloured, bNodePrecoloured;

        if(psPrevArg->uType != psArg->uType ||
           psPrevArg->uIndexType != psArg->uIndexType ||
		   psPrevArg->uIndexNumber != psArg->uIndexNumber ||
           psPrevArg->eFmt != psArg->eFmt)
        {
            return IMG_FALSE;
        }

		if (uInstArg > 0)
		{
			PFLOAT_SOURCE_MODIFIER	psMod, psPrevMod;
			IMG_UINT32				uComponent, uPrevComponent;

			psMod = GetFloatMod(psState, psInst, uInstArg - 1);
			psPrevMod = GetFloatMod(psState, psGData->psInst, uInstArg - 1);
			if (psMod != NULL)
			{
				ASSERT(psPrevMod != NULL);
				if (psMod->bNegate != psPrevMod->bNegate ||
					psMod->bAbsolute != psPrevMod->bAbsolute)
				{	
					return IMG_FALSE;
				}
			}

			uComponent = GetComponentSelect(psState, psInst, uInstArg - 1);
			uPrevComponent = GetComponentSelect(psState, psGData->psInst, uInstArg - 1);
			if (uComponent != uPrevComponent)
			{
				return IMG_FALSE;
			}
		}

		/* Test for operands which can't be grouped */
        if (!RegIsNode(&psRegState->sRAData, psArg) || !RegIsNode(&psRegState->sRAData, psPrevArg))
        {
            /* Continue group if register numbers are the same */
            return (psArg->uNumber == psPrevArg->uNumber) ? IMG_TRUE : IMG_FALSE;
        }

        /* Node number of previous register */
        uPrevNode = ArgumentToNode(&psRegState->sRAData, psPrevArg);

        /* Don't do anything if this node is the same as the previous node */
        if (uNode == uPrevNode)
        {
            return IMG_TRUE;
        }

		bPrevNodePrecoloured = IsPrecolouredNode(GetRegisterGroup(&psRegState->sRAData, uPrevNode));
		bNodePrecoloured = IsPrecolouredNode(GetRegisterGroup(&psRegState->sRAData, uNode));
		if (bPrevNodePrecoloured || bNodePrecoloured)
		{
			IMG_UINT32	uPrevNodeColourType, uPrevNodeColourNum;
			IMG_UINT32	uNodeColourType, uNodeColourNum;

			if (!(bPrevNodePrecoloured && bNodePrecoloured))
			{
				return IMG_FALSE;
			}

			GetFixedColour(psState, &psRegState->sRAData, uPrevNode, &uPrevNodeColourType, &uPrevNodeColourNum);
			GetFixedColour(psState, &psRegState->sRAData, uNode, &uNodeColourType, &uNodeColourNum);

			if (uPrevNodeColourType != uNodeColourType)
			{
				return IMG_FALSE;
			}
		}
    }
    else
    {
        /* Test for operands which can be grouped */
        if (!RegIsNode(&psRegState->sRAData, psArg))
        {
            return IMG_TRUE;
        }
    }

    /*  Don't add to a group if already in a group or if precoloured. */
	psNodeGroup = GetRegisterGroup(&psRegState->sRAData, uNode);
    if (IsPrecolouredNode(psNodeGroup))
    {
        /* Continue the group, but don't do anything with this node */
        return IMG_TRUE;
    }
    if (IsNodeInGroup(psNodeGroup))
    {
        /* Continue the group, but don't do anything with this node */
        return IMG_TRUE;
    }
	if (GetNodeAlignment(psNodeGroup) != HWREG_ALIGNMENT_NONE)
	{
		/* Continue the group, but don't do anything with this node */
		return IMG_TRUE;
	}

    /* Decide whether to make a new group or add to the old group */
    if (uPrevNode != UINT_MAX)
    {
		PREGISTER_GROUP	psPrevNodeGroup = GetRegisterGroup(&psRegState->sRAData, uPrevNode);

		if (!IsPrecolouredNode(psPrevNodeGroup))
		{
			ASSERT(uPrevNode != uNode);

			/* Add to the existing group */
			if (psPrevNodeGroup == NULL)
			{
				psPrevNodeGroup = MakeRegisterGroup(&psRegState->sRAData, uPrevNode);
			}
			if (psNodeGroup == NULL)
			{
				psNodeGroup = MakeRegisterGroup(&psRegState->sRAData, uNode);
			}
			AppendToGroup(psState, psPrevNodeGroup, psNodeGroup, IMG_TRUE);
			/* Record node number */
			(*puNode) = uNode;
			/* Signal grouping */
			return IMG_TRUE;
		}
    }

    /* Record node number */
    (*puNode) = uNode;
    /* Signal grouping */
    return IMG_TRUE;
}

static IMG_BOOL CompatibleInstructionModes(PINTERMEDIATE_STATE		psState,
										   PREGISTER_GROUP_DATA		psGData, 
										   PINST					psInst, 
										   PINST					psPrevInst)
/*****************************************************************************
 FUNCTION   : CompatibleInstructionModes

 PURPOSE    : Do a rough check for whether two instructions could be part of the
			  same MOE repeat.

 PARAMETERS : psState		- Compiler state.
			  psGData		- Information about the MOE repeat.
			  psInst,		- Instructions to check.
			  psPrevInst

 RETURNS    : TRUE or FALSE.
*****************************************************************************/
{
	if (GetBit(psInst->auFlag, INST_SKIPINV) != GetBit(psPrevInst->auFlag, INST_SKIPINV))
	{
		return IMG_FALSE;
	}
	if (!EqualPredicates(psInst, psPrevInst))
	{
		return IMG_FALSE;
	}
	if (psInst->uDestCount != psPrevInst->uDestCount)
	{
		return IMG_FALSE;
	}
	if (psInst->uDestCount > 0 && psInst->auDestMask[0] != psPrevInst->auDestMask[0])
	{
		return IMG_FALSE;
	}
	if (g_psInstDesc[psInst->eOpcode].eType == INST_TYPE_TEST)
	{
		ASSERT(g_psInstDesc[psPrevInst->eOpcode].eType == INST_TYPE_TEST);
		if (CompareInstParametersTypeTEST(psState, psInst, psPrevInst) != 0)
		{
			return IMG_FALSE;
		}
	}
	else if (g_psInstDesc[psInst->eOpcode].eType == INST_TYPE_MOVC)
	{
		if (CompareInstParametersTypeMOVC(psState, psInst, psPrevInst) != 0)
		{
			return IMG_FALSE;
		}
	}
	if (psGData->uGroup + 1> USC_MAX_INCREMENT_INC)
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

static
IMG_VOID RegisterGrouping(PINTERMEDIATE_STATE psState,
                                 PRAGCOL_STATE psRegState,
								 PDGRAPH_STATE	psDepState,
                                 PINST psInst)
/*****************************************************************************
 FUNCTION   : RegisterGrouping

 PURPOSE    : Try to group registers into contiguous block based on the
              instruction they appear in

 PARAMETERS : psState           - The compiler state
              psRegState        - The state of the registers
			  psDepState		- The dependency graph state
              psInst            - The instruction to work on

 RETURNS    : Nothing.

 NOTES      : Forms register groups by adding registers to the end of a group.
              Registers groups are always optional so mandatory groups will always
              precede the groups formed by this function.
*****************************************************************************/
{
    const IMG_UINT32 uArgCount = psInst->uArgumentCount;
    IMG_UINT32 i;
    REGISTER_GROUP_DATA sGroupData;
    PREGISTER_GROUP_DATA psGData;
    IMG_UINT32 uNode;
    PINST psPrevInst;
    IMG_BOOL bGroupContinues = IMG_TRUE;
    IMG_UINT32 auRegNode[USC_MAX_MOE_OPERANDS] =
        {UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX};
    PARG asNewArg[USC_MAX_MOE_OPERANDS] = {NULL, NULL, NULL, NULL};
    REGISTER_GROUP_DATA sTemp = USC_REGISTER_GROUP_DATA_DEFAULT_VALUE;

    /* Sanity checks. */
    ASSERT(psState != NULL);
    ASSERT(psRegState != NULL);
    ASSERT(psInst != NULL);

    /* Things that are not handled */
	if (!CanRepeatInst(psState, psInst) || psInst->eOpcode == ILDAD)
	{
		return;
	}

    /* Get the register grouping data */
    memcpy(&sGroupData,
           &psRegState->sRAData.asRegGroupData[psInst->eOpcode],
           sizeof(sGroupData));
    psGData = &sGroupData;
    psPrevInst = psGData->psInst;

    /* Check previous instruction */
    if (psGData->uGroup > 0)       /* Group has at least one member */
    {
		ASSERT(psPrevInst != NULL);

		if (!CompatibleInstructionModes(psState, psGData, psInst, psPrevInst))
		{
			/*
               Instruction is not compatible with previous instance, so end
               the existing group.
            */
            bGroupContinues = IMG_FALSE;
		}
		else if (GraphGet(psState, psDepState->psClosedDepGraph, psInst->uId, psPrevInst->uId))
		{
			/* instructions have a dependency so can't be grouped */
            bGroupContinues = IMG_FALSE;
		}
    }

    /* Add destination to per instruction group */
    uNode = UINT_MAX;
    if(bGroupContinues &&
       AddRegToInstGroup(psState, psRegState, psInst, 0, 0, psGData, &uNode))
    {
        auRegNode[0] = uNode;
    }
    else
    {
        bGroupContinues = IMG_FALSE;
    }
    /* Set the destination in the group data */
    asNewArg[0] = &psInst->asDest[0];

    /* Add MOE-able sources to per instruction group */
    for (i = 1; i < USC_MAX_MOE_OPERANDS; i++)
    {
		IMG_UINT32	uInstArg;

        uInstArg = MoeArgToInstArg(psState, psInst, i - 1);
		if (
				uInstArg == DESC_ARGREMAP_DONTCARE || 
				uInstArg == DESC_ARGREMAP_INVALID || 
				(uInstArg & DESC_ARGREMAP_ALIASMASK) != 0
		   )
		{
			continue;
		}
        if (!(uInstArg < uArgCount))
            continue;

        uNode = UINT_MAX;
        if(bGroupContinues &&
           AddRegToInstGroup(psState, psRegState, psInst, i, uInstArg + 1, psGData,
                             &uNode))
        {
            auRegNode[i] = uNode;
        }
        else
        {
            bGroupContinues = IMG_FALSE;
        }
        /* Set the stored register in the group data */
        asNewArg[i] = &psInst->asArg[uInstArg];
    }

    /*
       If the grouping fails, remove any registers from the group that
       may have been put in and start a new group.
    */
    if (!bGroupContinues)
    {
        for(i = 0; i < USC_MAX_MOE_OPERANDS; i++)
        {
            if (auRegNode[i] != UINT_MAX)
            {
				PREGISTER_GROUP	psRegNodeGroup = GetRegisterGroup(&psRegState->sRAData, auRegNode[i]);

				if (psRegNodeGroup != NULL)
				{
					RemoveFromGroup(psState, psRegNodeGroup);
				}
                auRegNode[i] = UINT_MAX;
            }

        }

        /* If the group ends and only has two iterations then dismantle the group completely */
        if (sGroupData.uGroup <= 2)
        {
            for(i = 0; i < USC_MAX_MOE_OPERANDS; i++)
            {
                if (sGroupData.auPrevNode[i] != UINT_MAX)
                {
					PREGISTER_GROUP	psPrevNodeGroup = GetRegisterGroup(&psRegState->sRAData, sGroupData.auPrevNode[i]);

					if (psPrevNodeGroup != NULL)
					{
						RemoveFromGroup(psState, psPrevNodeGroup);
					}
                }
            }
        }

        /* Start a new group */
        memcpy(psGData, &sTemp, sizeof(psGData));
    }

    /* Update the grouping data with the current instruction */
    psGData->psInst = psInst;
    psGData->uGroup += 1;

    /* Copy the arguments */
    memcpy(&sGroupData.psPrevReg, &asNewArg, sizeof(sGroupData.psPrevReg));
    /* Copy the nodes */
    memcpy(&sGroupData.auPrevNode, &auRegNode, sizeof(sGroupData.auPrevNode));

    /* Store the new grouping data */
    memcpy(&psRegState->sRAData.asRegGroupData[psInst->eOpcode],
           &sGroupData,
           sizeof(psRegState->sRAData.asRegGroupData[psInst->eOpcode]));
}

static
IMG_VOID FormOptRegGroupBP(PINTERMEDIATE_STATE psState,
						   PCODEBLOCK psBlock,
						   IMG_PVOID pvRegState)
/*****************************************************************************
 FUNCTION   : FormOptRegGroupBP

 PURPOSE    : BLOCK_PROC to Form optional register groups

 PARAMETERS : psState           - Compiler state.
              pvRegState        - PREGISTER_STATE of Register allocator state

 RETURNS    : Nothing.
*****************************************************************************/
{
	PRAGCOL_STATE psRegState = (PRAGCOL_STATE)pvRegState;
    PINST psInst;
    IMG_UINT32 i;
	PDGRAPH_STATE psDepState;

	if (psBlock->psOwner->psFunc == psState->psSecAttrProg)
	{
		return;
	}
	/* Clear the register grouping data */
	for (i = 0; i < IOPCODE_MAX; i++)
	{
		REGISTER_GROUP_DATA sRGData = USC_REGISTER_GROUP_DATA_DEFAULT_VALUE;
		memcpy(&(psRegState->sRAData.asRegGroupData[i]),
			   &sRGData,
			   sizeof(psRegState->sRAData.asRegGroupData[i]));
	}

	/* find all the instruction this one depends on.  can't have MOE
	on dependent instructions.  TODO, you can, but only if the second
	instruction reads directly from the first, not via another inst */
	psBlock->uFlags |= USC_CODEBLOCK_NEED_DEP_RECALC;
	psDepState = ComputeBlockDependencyGraph(psState, psBlock, IMG_FALSE);
	ComputeClosedDependencyGraph(psDepState, IMG_FALSE /* bUnorderedDeps */);

	psBlock->uFlags |= USC_CODEBLOCK_NEED_DEP_RECALC;

	for (psInst = psBlock->psBody; psInst; psInst = psInst->psNext)
	{
		RegisterGrouping(psState, psRegState, psDepState, psInst);
	}

	/* Release memory */
	FreeBlockDGraphState(psState, psBlock);
}

static
IMG_VOID SetNodeColour(PRAGCOL_STATE psRegState,
                       IMG_UINT32 uNode,
					   IMG_UINT32 uColourIdx,
                       PCOLOUR psColour)
/*****************************************************************************
 FUNCTION   : SetNodeColour

 PURPOSE    : Set the colour of a node

 PARAMETERS : psRegState       - Register allocator state
              uNode            - The node to colour
			  uColourIdx	   - For a node with multiple colours: the index
							   of the colour to set.
              psColour          - The new colour.

 RETURNS    : Nothing.
*****************************************************************************/
{
	PINTERMEDIATE_STATE	psState = psRegState->sRAData.psState;
	PNODE_DATA			psNode = &psRegState->asNodes[uNode];

	ASSERT(uColourIdx < psNode->uColourCount);
	psNode->asColours[uColourIdx] = *psColour;
	if (uColourIdx == (psNode->uColourCount - 1))
	{
		IntfGraphInsert(psState, psRegState->psIntfGraph, uNode);
	}
}

static
IMG_VOID ReserveNodeColour(PRAGCOL_STATE psRegState,
                           IMG_UINT32 uNode,
                           PCOLOUR psColour)
/*****************************************************************************
 FUNCTION   : ReserveNodeColour

 PURPOSE    : Reserve a  colour for a node

 PARAMETERS : psRegState       - Register allocator state
              uNode            - The node to colour
              psColour         - The colour to reserve.

 RETURNS    : Nothing.
*****************************************************************************/
{
	psRegState->asNodes[uNode].sReservedColour = *psColour;
}


typedef struct _REGSTACK
{
	IMG_UINT32	uSize;
	IMG_PUINT32	auStack;
} REGSTACK, *PREGSTACK;

static IMG_BOOL SimplifyGraph(PRAGCOL_STATE psRegState, PNODE_DATA* apsRegistersSortedByDegree, IMG_BOOL bSpill, PREGSTACK psRegStack)
/*****************************************************************************
 FUNCTION   : SimplifyGraph

 PURPOSE    : Simplify the register interference graph.

 PARAMETERS : bSpill    - if IMG_FALSE then remove nodes we should be able to colour;
                          if IMG_TRUE then remove nodes we might not be able to colour.

 RETURNS    : IMG_TRUE if a node could be removed; IMG_FALSE otherwise.
*****************************************************************************/
{
    IMG_UINT32 i;
	IMG_UINT32 uAvailRegs = psRegState->sRAData.uTotalAvailRegs;

    for (i = 0; i < psRegState->uNumUsedRegisters; i++)
    {
		PNODE_DATA	psReg = apsRegistersSortedByDegree[i];
		IMG_UINT32	uReg = (IMG_UINT32)(psReg - psRegState->asNodes);
        IMG_UINT32	uDegree = IntfGraphGetVertexDegree(psRegState->psIntfGraph, uReg);

        if (uDegree > 0 &&
            ((!bSpill && uDegree < uAvailRegs) ||
             (bSpill && uDegree >= uAvailRegs)))
        {
            psRegStack->auStack[psRegStack->uSize++] = uReg;
            IntfGraphRemove(psRegState->sRAData.psState, psRegState->psIntfGraph, uReg);
            return IMG_TRUE;
        }
    }
    return IMG_FALSE;
}

typedef struct _LIVE_SET
{
	/*
		Live of the currently live intermediate registers.
	*/
	PSPARSE_SET		psLiveList;
	/*
		Mask of channels live in each intermediate register.
	*/
	IMG_PUINT32		auLiveChanMask;
} LIVE_SET, *PLIVE_SET;

static IMG_VOID UpdateInterferenceGraph(PRAGCOL_STATE	psRegState, 
										PLIVE_SET		psLiveSet,
										IMG_UINT32		uReg, 
										IMG_UINT32		uRegLiveChanMask,
										IMG_UINT32		uSkipNode)
/*****************************************************************************
 FUNCTION	: UpdateInterferenceGraph

 PURPOSE	: Make an intermediate register interfere with all currently live
			  intermediate registers.

 PARAMETERS	: psRegState			- Register allocator state.
			  psLiveSet				- Information about the currently live
									intermediate registers.
			  uReg					- Intermediate register to add interference
									relations for.
			  uRegLiveChanMask		- Mask of channels live in the intermediate
									register.
			  uSkipNode				- Don't add interference relations for this
									intermediate register.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_BOOL			bRegIsC10;
	PINTERMEDIATE_STATE	psState = psRegState->sRAData.psState;
	SPARSE_SET_ITERATOR	sIter;

	ASSERT(uReg < psRegState->sRAData.uNrRegisters);

	/* Make the newly live register interference with all those currently live. */
	bRegIsC10 = GetBit(psRegState->asNodes[uReg].auFlags, NODE_FLAG_C10);

	for (SparseSetIteratorInitialize(psLiveSet->psLiveList, &sIter); 
		 SparseSetIteratorContinue(&sIter); 
		 SparseSetIteratorNext(&sIter))
	{
		IMG_UINT32	uOtherReg;
		IMG_UINT32	uOtherRegLiveChanMask;

		uOtherReg = SparseSetIteratorCurrent(&sIter);
		uOtherRegLiveChanMask = GetRegMask(psLiveSet->auLiveChanMask, uOtherReg);

		if (uOtherReg == uSkipNode)
		{
			uOtherRegLiveChanMask &= ~uRegLiveChanMask;
		}

		if (uOtherRegLiveChanMask != 0)
		{
			IMG_BOOL	bOtherRegC10;

			bOtherRegC10 = GetBit(psRegState->asNodes[uOtherReg].auFlags, NODE_FLAG_C10);

			/*
				Make the two registers interfere if they use overlapping channels
				or one is C10 and the other not (since channels in C10 registers
				and non-C10 registers are different sizes).
			*/
			if ((uRegLiveChanMask & uOtherRegLiveChanMask) != 0 || bRegIsC10 != bOtherRegC10)
			{
				IntfGraphAddEdge(psState, psRegState->psIntfGraph, uReg, uOtherReg);
			}
		}
	}
}

IMG_INTERNAL
IMG_VOID ReplaceRegisterLiveOutSetsBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvContext)
/*****************************************************************************
 FUNCTION	: ReplaceRegisterInsts

 PURPOSE	: Replace all references to a register in the stored set of
			  registers live out of a flow control block.

 PARAMETERS	: psState				- Compiler state.
              psBlock				- Flow control block to modify.
			  pvContext				- Information about the register to be
									replaced.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PREPLACE_REGISTER_LIVEOUT_SETS	psContext = (PREPLACE_REGISTER_LIVEOUT_SETS)pvContext;
	IMG_UINT32						uLiveOutMask;

	uLiveOutMask = GetRegisterLiveMask(psState,
									   &psBlock->sRegistersLiveOut,
									   psContext->uRenameFromRegType,
									   psContext->uRenameFromRegNum,
									   0 /* uArrayOffset */);
	if (uLiveOutMask != 0)
	{
		IncreaseRegisterLiveMask(psState,
								 &psBlock->sRegistersLiveOut,
								 psContext->uRenameToRegType,
								 psContext->uRenameToRegNum,
								 0 /* uArrayOffset */,
								 uLiveOutMask);

		/*
			Clear information about the renamed-from register.
		*/
		SetRegisterLiveMask(psState,
						    &psBlock->sRegistersLiveOut,
							psContext->uRenameFromRegType,
							psContext->uRenameFromRegNum,
							0 /* uArrayOffset */,
							0 /* uMask */);
	}
}

#if defined(OUTPUT_USPBIN)
static
IMG_VOID MarkRegisterAsShaderResult(PINTERMEDIATE_STATE psState, PUSEDEF_CHAIN psRegister)
/*****************************************************************************
 FUNCTION	: MarkRegisterAsShaderResult

 PURPOSE	: Mark all instruction reading/writing a register as reading/writing a
			  shader result.

 PARAMETERS	: psState		- Compiler state.
			  psRegister	- Register to mark references to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;

	for (psListEntry = psRegister->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUseDef;

		psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		switch (psUseDef->eType)
		{
			case DEF_TYPE_INST:
			{
				PINST		psUseInst = psUseDef->u.psInst;
				MarkShaderResultWrite(psState, psUseInst);
				if (psUseInst->uDestCount > 1)
				{
					psState->uFlags |= USC_FLAGS_RESULT_REFS_NOT_PATCHABLE;
				}
				break;
			}
			case USE_TYPE_SRC:
			{
				PINST		psUseInst = psUseDef->u.psInst;
				IMG_UINT32	uHWOperandsUsed = GetHWOperandsUsedForArg(psUseInst, psUseDef->uLocation + 1);
				psUseInst->uShaderResultHWOperands |= uHWOperandsUsed;
				break;
			}
			default:
			{
				break;
			}
		}
	}
}	
#endif /* defined(OUTPUT_USPBIN) */

static
IMG_UINT32 GetNewLiveChansInDest(PINTERMEDIATE_STATE psState, PUSEDEF_CHAIN psTemp, PINST psPoint)
/*****************************************************************************
 FUNCTION	: GetNewLiveChansInDest

 PURPOSE	: Get the mask of channels live in a temporary at a particular point
			  in the program.

 PARAMETERS	: psState		- Compiler state.
			  psTemp		- Temporary register to get the live channels for.
			  psPoint		- The returned mask is live channels at the point
							where this instruction writes its destinations.

 RETURNS	: The mask of live channels.
*****************************************************************************/
{
	IMG_UINT32		uLiveChans;
	PUSC_LIST_ENTRY	psListEntry, psFirstUseAfterPoint, psLastUseAfterPoint;

	/*
		Get the mask of channels in the temporary which are live into the block which contains the specified instruction.
	*/
	ASSERT(psTemp->uType == USEASM_REGTYPE_TEMP);
	uLiveChans = 
		GetRegisterLiveMask(psState, 
							&psPoint->psBlock->sRegistersLiveOut, 
							USEASM_REGTYPE_TEMP, 
							psTemp->uNumber, 
							0 /* uArrayOffset */);

	/*
		Search through the temporary list of uses/defines for the first one in an instruction after the specified
		instruction.
	*/
	for (psListEntry = psTemp->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUseDef;
		PINST	psUseDefInst;

		psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		psUseDefInst = UseDefGetInst(psUseDef);
		if (psUseDefInst != NULL && 
			psUseDefInst->psBlock == psPoint->psBlock && 
			psUseDefInst->uBlockIndex > psPoint->uBlockIndex)
		{
			break;
		}
	}

	/*
		Check for no uses/defines of the temporary after the specified instruction.
	*/
	if (psListEntry == NULL)
	{
		return uLiveChans;
	}

	/*
		Find the last use/define of the temporary in the same block as the specified instruction.
	*/
	psFirstUseAfterPoint = psListEntry;
	psLastUseAfterPoint = psListEntry;
	for (; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUseDef;
		PINST	psUseDefInst;

		psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		psUseDefInst = UseDefGetInst(psUseDef);
		if (psUseDefInst == NULL || psUseDefInst->psBlock != psPoint->psBlock)
		{
			break;
		}
		psLastUseAfterPoint = psListEntry;
	}

	/*
		Process all the references to the temporary between the specified instruction and the end of the block.
	*/
	psListEntry = psLastUseAfterPoint;
	for (;;)
	{
		PUSEDEF	psUseDef;

		psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		if (psUseDef->eType == DEF_TYPE_INST)
		{
			uLiveChans = 0;
		}
		else
		{
			uLiveChans |= GetUseChanMask(psState, psUseDef);
		}

		if (psListEntry == psFirstUseAfterPoint)
		{
			break;
		}
		psListEntry = psListEntry->psPrev;
	}

	return uLiveChans;
}

static
IMG_VOID UpdateLiveChansInDestAfterCoalesce(PINTERMEDIATE_STATE	psState, 
											PUSEDEF_CHAIN		psFirstRegister, 
											PUSEDEF_CHAIN		psSecondRegister,
											PARG				psCombinedRegister)
/*****************************************************************************
 FUNCTION	: UpdateLiveChansInDestAfterCoalesce

 PURPOSE	: Update the stored masks of channels live in instruction defining
			  one register as it is coalesced with another register.

 PARAMETERS	: psState				- Compiler state.
			  psFirstRegister		- Register to update defining instructions for.
			  psSecondRegister		- Register which is to be coalesced with the
									first register.
			  psCombinedRegister	- Destination register for the coalesce.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;

	/*
		For all of the instructions which define the first register.
	*/
	for (psListEntry = psFirstRegister->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUseDef;

		psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		if (psUseDef->eType == DEF_TYPE_INST)
		{
			PINST		psDefInst = psUseDef->u.psInst;
			IMG_UINT32	uDestIdx = psUseDef->uLocation;
			IMG_UINT32	uExtraLiveChansInDest;

			/*
				Get the mask of channels which are live in the second register at this instruction.
			*/
			ASSERT(uDestIdx < psDefInst->uDestCount);
			uExtraLiveChansInDest = GetNewLiveChansInDest(psState, psSecondRegister, psDefInst);
			if (uExtraLiveChansInDest > 0)
			{
				/*
					Update the channels live in this instruction destination to the channels live in the
					combined register.
				*/
				psDefInst->auLiveChansInDest[uDestIdx] |= uExtraLiveChansInDest;

				/*
					If there are now extra channels live in the combined register at this point then mark
					that the instruction needs to preserve their values.
				*/
				if (psDefInst->apsOldDest[psUseDef->uLocation] == NULL)
				{	
					SetPartiallyWrittenDest(psState, psDefInst, psUseDef->uLocation, psCombinedRegister);
				}
			}	
		}
	}
}

static
IMG_VOID ReplaceRegisterInsts(PINTERMEDIATE_STATE psState, 
							  PRAGCOL_STATE	psRegState,
							  IMG_UINT32 uRenameFromTempNum, 
							  IMG_UINT32 uRenameToTempNum,
							  IMG_BOOL bToRegShaderResult,
							  IMG_BOOL bFromRegShaderResult)
/*****************************************************************************
 FUNCTION	: ReplaceRegisterInsts

 PURPOSE	: Replace all references to a temp register in instructions.

 PARAMETERS	: psState				- Compiler state.
			  psRegState			- Register allocator state.
              uRenameFromTempNum	- Temporary register to be replaced.
			  uRenameToTempNum		- Temporary register to replace by.
			  bToRegShaderResult	- TRUE if the register to replace by is a 
									shader result.
			  bFromRegShaderResult	- TRUE if the register to be replaced is a
									shader result.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF_CHAIN					psRenameFromUseDef;
	PVREGISTER						psRenameToVReg;
	PUSEDEF_CHAIN					psRenameToUseDef;
	PUSC_LIST_ENTRY					psListEntry;
	PUSC_LIST_ENTRY					psNextListEntry;
	REPLACE_REGISTER_LIVEOUT_SETS	sContext;
	ARG								sRenameToArg;

	psRenameFromUseDef = UseDefGet(psState, USEASM_REGTYPE_TEMP, uRenameFromTempNum);
	psRenameToVReg = GetVRegister(psState, USEASM_REGTYPE_TEMP, uRenameToTempNum);
	psRenameToUseDef = psRenameToVReg->psUseDefChain;

	#if defined(OUTPUT_USPBIN)
	/*
		If either of the registers are shader results then mark all of the instructions
		referencing the other register as reading/writing a shader result.
	*/
	if ((psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) != 0)
	{
		if (bToRegShaderResult)
		{
			MarkRegisterAsShaderResult(psState, psRenameFromUseDef);
		}
		if (bFromRegShaderResult)
		{
			MarkRegisterAsShaderResult(psState, psRenameToUseDef);
		}
	}
	#else /* defined(OUTPUT_USPBIN) */
	PVR_UNREFERENCED_PARAMETER(bToRegShaderResult);
	PVR_UNREFERENCED_PARAMETER(bFromRegShaderResult);
	#endif /* defined(OUTPUT_USPBIN) */

	/*
		Make an argument structure for the renamed to register.
	*/
	InitInstArg(&sRenameToArg);
	sRenameToArg.uType = USEASM_REGTYPE_TEMP;
	sRenameToArg.uNumber = uRenameToTempNum;
	sRenameToArg.psRegister = psRenameToVReg;
	sRenameToArg.eFmt = UF_REGFORMAT_UNTYPED;

	/*
		Update the stored live channels at each instruction defining both of the coalesced registers.
	*/
	UpdateLiveChansInDestAfterCoalesce(psState, psRenameFromUseDef, psRenameToUseDef, &sRenameToArg);
	UpdateLiveChansInDestAfterCoalesce(psState, psRenameToUseDef, psRenameFromUseDef, &sRenameToArg);

	/*
		For all the references to the renamed-from register replace them by the renamed-to register.
	*/
	for (psListEntry = psRenameFromUseDef->sList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PUSEDEF	psUseDef;

		psNextListEntry = psListEntry->psNext;
		psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

		UseDefBaseSubstUse(psState, psRenameFromUseDef, psUseDef, &sRenameToArg, IMG_TRUE /* bDontUpdateFmt */);
	}

	/*
		Rename the register in the live out sets for all blocks.
	*/
	sContext.uRenameFromRegType = USEASM_REGTYPE_TEMP;
	sContext.uRenameFromRegNum = uRenameFromTempNum;
	sContext.uRenameToRegType = USEASM_REGTYPE_TEMP;
	sContext.uRenameToRegNum = uRenameToTempNum;
	DoOnAllFuncGroupBasicBlocks(psState,
								ANY_ORDER,
								ReplaceRegisterLiveOutSetsBP,
								IMG_FALSE,
								&sContext,
								psRegState->sRAData.eFuncGroup);
}

static
IMG_VOID CoalesceRegister(PINTERMEDIATE_STATE	psState,
						  PRAGCOL_STATE		    psRegState,
						  IMG_UINT32			uRenameToNode,
						  IMG_UINT32			uRenameFromNode,
						  IMG_BOOL				bToNodeShaderResult,
						  IMG_BOOL				bFromNodeShaderResult)
/*****************************************************************************
 FUNCTION	: CoalesceRegister

 PURPOSE	: Replace all uses of one register by another.

 PARAMETERS	: psState			- Compiler state.
			  psRegState		- Register allocator state.
			  uRenameToNode		-
			  uRenameFromNode	-

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32				uRenameFromType;
	IMG_UINT32				uRenameFromRegNum;
	IMG_UINT32				uRenameToType;
	IMG_UINT32				uRenameToRegNum;
	PREGALLOC_DATA			psRAData = &psRegState->sRAData;

	/*
		Go from node numbers to register numbers.
	*/
	NodeToRegister(psRAData, uRenameToNode, &uRenameToType, &uRenameToRegNum);
	ASSERT(uRenameToType == USEASM_REGTYPE_TEMP);

	NodeToRegister(psRAData, uRenameFromNode, &uRenameFromType, &uRenameFromRegNum);
	/* Shouldn't try to rename registers with a fixed colour. */
	ASSERT(uRenameFromType == USEASM_REGTYPE_TEMP);

	/*
		Merge the edges for both nodes.
	*/
	IntfGraphMergeVertices(psState, psRegState->psIntfGraph, uRenameToNode, uRenameFromNode);

	/*
		Rename references to the register in all instructions.
	*/
	ReplaceRegisterInsts(psState, 
						 psRegState, 
						 uRenameFromRegNum, 
						 uRenameToRegNum, 
						 bToNodeShaderResult, 
						 bFromNodeShaderResult);
}

static IMG_VOID MakeInterfereWithAllPAs(PRAGCOL_STATE	psRegState,
									    IMG_UINT32		uNode)
/*****************************************************************************
 FUNCTION	: MakeInterfereWithAllPAs

 PURPOSE	: Add edges between a node and all nodes represting primary
			  attributes.

 PARAMETERS	: psRegState		- Register allocator state.
			  uNode				- Node to add edges for.

 RETURNS	: Nothing.

 NOTES      :
*****************************************************************************/
{
	psRegState->asNodes[uNode].uBankFlags &= ~BANK_FLAG_PRIMATTR;
}

static IMG_VOID MakeInterfereWithAllOutputRegs(PRAGCOL_STATE	psRegState,
											   IMG_UINT32		uNode)
/*****************************************************************************
 FUNCTION	: MakeInterfereWithAllOutputRegs

 PURPOSE	: Add edges between a node and all nodes represting output
			  registers.

 PARAMETERS	: psRegState		- Register allocator state.
			  uNode				- Node to add edges for.

 RETURNS	: Nothing.

 NOTES      :
*****************************************************************************/
{
	psRegState->asNodes[uNode].uBankFlags &= ~BANK_FLAG_OUTPUT;
}

typedef struct _DEF_RANGE
{
	/*
		First node in the range potentially defined by this instruction destination.
	*/
	IMG_UINT32	uStart;
	/*
		First node after the range defined.
	*/
	IMG_UINT32	uEnd;
	/*
		TRUE if the destination is dynamically indexed.
	*/
	IMG_BOOL	bIndexed;
	/*
		Mask of channels written by this destination.
	*/
	IMG_UINT32	uMask;
	/*
		Either USC_UNDEF or the node copied into this destination.
	*/
	IMG_UINT32	uCopyNode;
} DEF_RANGE, *PDEF_RANGE;

typedef struct _DEF_LIST
{
	/*
		List of ranges potentially defined by the instruction.
	*/
	DEF_RANGE	asList[USC_MAX_NONCALL_DEST_COUNT];
	/*
		Count of ranges.
	*/
	IMG_UINT32	uCount;
} DEF_LIST, *PDEF_LIST;

static IMG_VOID AddEdgeForAllDefs(PRAGCOL_STATE psRegState, PDEF_LIST psDefList, IMG_UINT32 uNode)
/*****************************************************************************
 FUNCTION	: AddEdgeForAllDefs

 PURPOSE	: Add an edge to an interference graph between a specified node and
			  all the intermediate registers defined by an instruction.

 PARAMETERS	: psRegState			- Register allocator state.
			  psDefList				- List of registers defined by the instruction.
			  uNode					- Node to add edges for.

 RETURNS	: Nothing.

 NOTES      :
*****************************************************************************/
{
	IMG_UINT32	uOtherDestIdx;

	for (uOtherDestIdx = 0; uOtherDestIdx < psDefList->uCount; uOtherDestIdx++)
	{
		IMG_UINT32	uOtherNode;
		PDEF_RANGE	psRange = &psDefList->asList[uOtherDestIdx];

		for (uOtherNode = psRange->uStart; uOtherNode < psRange->uEnd; uOtherNode++)
		{ 
			if (uNode != uOtherNode)
			{
				IntfGraphAddEdge(psRegState->sRAData.psState, psRegState->psIntfGraph, uNode, uOtherNode);
			}
		}
	}
}

static IMG_VOID ConstructInterferenceGraphForImmTemps(PINTERMEDIATE_STATE	psState,
													  PRAGCOL_STATE			psRegState,
													  PLIVE_SET				psLiveSet,
													  PINST					psInst,
													  IMG_BOOL				bInstBlockPostSplit)
/*****************************************************************************
 FUNCTION	: ConstructInterferenceGraphForImmTemps

 PURPOSE	: Add all the register interference relations for immediate temporaries.

 PARAMETERS	: psState			- Compiler state.
			  psRegState		- Register allocator state.
			  psInst			- Instruction.
			  bInstBlockPostSplit
								- Whether block in which instruction is present is
								present post split point or not.

 RETURNS	: Nothing.

 NOTES      :
*****************************************************************************/
{
	IMG_UINT32	uTempIdx;
	IMG_UINT32	auNewNodes[USC_MAXIMUM_CONSECUTIVE_REGISTER_SET_LENGTH];
	IMG_UINT32	uNewNodeCount;

	uNewNodeCount = 0;
	for (uTempIdx = 0; uTempIdx < psInst->uTempCount; uTempIdx++)
	{
		PSHORT_REG	psTemp = &psInst->asTemp[uTempIdx];

		if (psTemp->eType == USEASM_REGTYPE_TEMP)
		{
			IMG_UINT32	uPrevNodeIdx;
			IMG_UINT32	uTempNode;

			uTempNode = RegisterToNode(&psRegState->sRAData, psTemp->eType, psTemp->uNumber);
			SetBit(psRegState->asNodes[uTempNode].auFlags, NODE_FLAG_REFERENCED, 1);

			/*
				Make the immediate node interfere with all currently live registers.
			*/
			UpdateInterferenceGraph(psRegState, psLiveSet, uTempNode, USC_DESTMASK_FULL, USC_UNDEF /* uSkipNode */);

			/*
				And all other immediate nodes for this instruction.
			*/
			for (uPrevNodeIdx = 0; uPrevNodeIdx < uNewNodeCount; uPrevNodeIdx++)
			{
				IntfGraphAddEdge(psState, psRegState->psIntfGraph, auNewNodes[uPrevNodeIdx], uTempNode);
			}

			/* Never assign an immediate temporary to an output register. */
			MakeInterfereWithAllOutputRegs(psRegState, uTempNode);

			/*
				We'll (eventually) need to make the temporary the destination of an instruction so
				don't allow it to be a primary attribute if primary attributes can't be written.
			*/
			if (psRegState->bNeverWritePrimaryAttributes || bInstBlockPostSplit)
			{
				MakeInterfereWithAllPAs(psRegState, uTempNode);
			}

			ASSERT(uNewNodeCount < (sizeof(auNewNodes) / sizeof(auNewNodes[0])));
			auNewNodes[uNewNodeCount++] = uTempNode;
		}
	}
}

static IMG_VOID IndexedArgumentToNodeRange(PREGALLOC_DATA   psRAData,
										   PARG				psArg,
										   IMG_PUINT32		puRangeStart,
										   IMG_PUINT32		puRangeEnd)
/*****************************************************************************
 FUNCTION	: IndexedArgumentToNodeRange

 PURPOSE	: Get the range of nodes potentially read/written by an
			  indexed source/destination.

 PARAMETERS	: psRAData          - Register allocator data.
			  psArg				- Argument to convert.
			  puRangeStart		- Returns the first node in the range.
			  puRangeEnd		- Returns the last node after the range.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINTERMEDIATE_STATE psState = psRAData->psState;
	PUSC_VEC_ARRAY_REG	psVecArrayReg;

	ASSERT(psArg->uIndexType != USC_REGTYPE_NOINDEX);
	ASSERT(psArg->uType == USC_REGTYPE_REGARRAY);

	psVecArrayReg = psState->apsVecArrayReg[psArg->uNumber];
	ASSERT (psVecArrayReg != NULL);

	*puRangeStart = RegisterToNode(psRAData,
								   USEASM_REGTYPE_TEMP,
								   psVecArrayReg->uBaseReg);
	*puRangeEnd = RegisterToNode(psRAData,
								 USEASM_REGTYPE_TEMP,
								 psVecArrayReg->uBaseReg + psVecArrayReg->uRegs);
}

static IMG_VOID FlagC10NodesArg(PINTERMEDIATE_STATE		psState,
								PRAGCOL_STATE			psRegState,
								PARG					psArg)
{
	const PREGALLOC_DATA psRAData = &psRegState->sRAData;

	if (!RegIsNode(psRAData, psArg))
	{
		return;
	}
	if (psArg->eFmt != UF_REGFORMAT_C10)
	{
		return;
	}

	/*
		Flag registers which are C10 format.
	*/
	if (psArg->uIndexType != USC_REGTYPE_NOINDEX)
	{
		IMG_UINT32	uSrcNode, uSrcNodeRangeStart, uSrcNodeRangeEnd;

		IndexedArgumentToNodeRange(psRAData, psArg, &uSrcNodeRangeStart, &uSrcNodeRangeEnd);

		ASSERT(uSrcNodeRangeEnd <= psRAData->uNrRegisters);
		for (uSrcNode = uSrcNodeRangeStart; uSrcNode < uSrcNodeRangeEnd; uSrcNode++)
		{
			/*
				Flag registers which are C10 format.
			*/
			SetBit(psRegState->asNodes[uSrcNode].auFlags, NODE_FLAG_C10, IMG_TRUE);
		}
	}
	else
	{
		IMG_UINT32 uSrcNode = ArgumentToNode(psRAData, psArg);

		ASSERT(uSrcNode < psRAData->uNrRegisters);
		SetBit(psRegState->asNodes[uSrcNode].auFlags, NODE_FLAG_C10, IMG_TRUE);
	}
}

static IMG_VOID FlagC10NodesInst(PINTERMEDIATE_STATE	psState,
								 PRAGCOL_STATE			psRegState,
								 PINST					psInst)
/*****************************************************************************
 FUNCTION	: FlagC10NodesInst

 PURPOSE	: Flag registers used in an instruction which are C10 format.

 PARAMETERS	: psState			- Compiler state.
			  psRegState		- Module state.
			  psInst			- Instruction.

 RETURNS	: Nothing.

 NOTES      :
*****************************************************************************/
{
	IMG_UINT32		uDestIdx;
	IMG_UINT32		uArgIdx;

	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		PARG		psDest = &psInst->asDest[uDestIdx];
		PARG		psOldDest = psInst->apsOldDest[uDestIdx];
		
		FlagC10NodesArg(psState, psRegState, psDest);
		if (psOldDest != NULL)
		{
			FlagC10NodesArg(psState, psRegState, psOldDest);
		}
	}

	/* Process the instruction source arguments. */
	for (uArgIdx = 0; uArgIdx < psInst->uArgumentCount; uArgIdx++)
	{
		PARG	psArg = &psInst->asArg[uArgIdx];

		FlagC10NodesArg(psState, psRegState, psArg);
	}
}

static IMG_VOID FlagC10NodesBP(PINTERMEDIATE_STATE	psState,
							   PCODEBLOCK			psBlock,
							   IMG_PVOID			pvRegState)
/*****************************************************************************
 FUNCTION	: FlagC10NodesBP

 PURPOSE	: Flag registers used in a block which are C10 format.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			-
			  pvRegState		- Module state.

 RETURNS	: Nothing.

 NOTES      :
*****************************************************************************/
{
	PRAGCOL_STATE	psRegState = (PRAGCOL_STATE)pvRegState;
	PINST			psInst;

	/*
		For each instruction in the block flag C10 registers used.
	*/
	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		FlagC10NodesInst(psState, psRegState, psInst);
	}
}

static IMG_VOID LiveSetAdd(PLIVE_SET psLiveSet, IMG_UINT32 uNode, IMG_UINT32 uMask)
/*****************************************************************************
 FUNCTION	: LiveSetAdd

 PURPOSE	: Add a new node to the set of currently live registers.

 PARAMETERS	: psLiveSet		- Set of currently live registetrs.
			  uNode			- Node to add.
			  uMask			- Mask of channels live in the node.

 RETURNS	: Nothing.

 NOTES      :
*****************************************************************************/
{
	if (uMask != 0)
	{
		IMG_UINT32	uExistingLiveChanMask;

		if (!SparseSetIsMember(psLiveSet->psLiveList, uNode))
		{
			SparseSetAddMember(psLiveSet->psLiveList, uNode);
			SetRegMask(psLiveSet->auLiveChanMask, uNode, 0);
		}

		uExistingLiveChanMask = GetRegMask(psLiveSet->auLiveChanMask, uNode);
		uExistingLiveChanMask |= uMask;
		SetRegMask(psLiveSet->auLiveChanMask, uNode,uExistingLiveChanMask);
	}
}

static IMG_VOID UpdateInterferenceGraphForArgument(PRAGCOL_STATE		psRegState,
												   PLIVE_SET			psLiveSet,
												   PINST				psInst,
												   IMG_UINT32			uSrcNode,
												   IMG_BOOL				bOutputBankInvalid,
												   IMG_UINT32			uLiveMask,
												   PDEF_LIST			psDefList)
/*****************************************************************************
 FUNCTION	: UpdateInterferenceGraphForArgument

 PURPOSE	: Update the interference graph for an instruction argument.

 PARAMETERS	: psRegState			- Module state
			  psLiveSet				- Bit array of the registers currently
									live.
			  psInst				- Instruction.
			  uSrcNode				- Instruction source.
			  bOutputBankInvalid	- If TRUE then the hardware output register
									bank isn't supported by this instruction
									source.
			  uLiveMask				- Mask of channels used from the
									instruction source.
			  psDefList				- List of the intermediate registers
									defined by the instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINTERMEDIATE_STATE	psState = psRegState->sRAData.psState;

	ASSERT(uSrcNode < psRegState->sRAData.uNrRegisters);

	/* Add the instruction argument to the list of currently live registers. */
	LiveSetAdd(psLiveSet, uSrcNode, uLiveMask);

	/*
		Flag this register is used so it is a candidate for spilling.
	*/
	SetBit(psRegState->asNodes[uSrcNode].auFlags, NODE_FLAG_SPILLABLE, IMG_TRUE);
	SetBit(psRegState->asNodes[uSrcNode].auFlags, NODE_FLAG_REFERENCED, IMG_TRUE);

	/*
		If the output register bank can't be used in this argument of
		the instruction then make the source interfere with all
		output registers.
	*/
	if (bOutputBankInvalid)
	{
		MakeInterfereWithAllOutputRegs(psRegState, uSrcNode);
	}

	/*
		We can't allocate the source and destination of a
		dsx/dsy to the same register since the source will
		effectively be used after the destination is written.
	*/
	if (g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_DESTANDSRCOVERLAP)
	{
		AddEdgeForAllDefs(psRegState, psDefList, uSrcNode);
	}
}

static IMG_VOID UpdateInterferenceGraphForSrc(PRAGCOL_STATE			psRegState,
											  PINST					psInst,
											  PARG					psArg,
											  IMG_BOOL				bOutputBankInvalid,
											  IMG_UINT32			uLiveChansInArg,
											  PLIVE_SET				psLiveSet,
											  PDEF_LIST				psDefList)
/*****************************************************************************
 FUNCTION	: UpdateInterferenceGraphForSrc

 PURPOSE	: Add all the register interference relations for an instruction
			  source argument.

 PARAMETERS	: psRegState			- Register allocator state.
			  psArg					- Source argument.
			  bOutputBankInvalid	- TRUE if this source argument supports the 
									hardware output register bank.
			  uLiveChansInArg		- Mask of channels used from the source argument.
			  psLiveSet				- Set of registers live before the instruction executes.
			  psDefList				- List of registers defined by the instruction.

 RETURNS	: Nothing.

 NOTES      :
*****************************************************************************/
{
	const PREGALLOC_DATA psRAData = &psRegState->sRAData;

	if (psArg->uIndexType != USC_REGTYPE_NOINDEX)
	{
		IMG_UINT32	uSrcNode, uSrcNodeRangeStart, uSrcNodeRangeEnd;

		IndexedArgumentToNodeRange(psRAData, psArg, &uSrcNodeRangeStart, &uSrcNodeRangeEnd);

		for (uSrcNode = uSrcNodeRangeStart; uSrcNode < uSrcNodeRangeEnd; uSrcNode++)
		{
			UpdateInterferenceGraphForArgument(psRegState,
											   psLiveSet,
											   psInst,
											   uSrcNode,
											   bOutputBankInvalid,
											   uLiveChansInArg,
											   psDefList);
		}
	}
	else
	{
		IMG_UINT32 uSrcNode = ArgumentToNode(psRAData, psArg);

		UpdateInterferenceGraphForArgument(psRegState,
										   psLiveSet,
										   psInst,
										   uSrcNode,
										   bOutputBankInvalid,
										   uLiveChansInArg,
										   psDefList);
	}
}

static IMG_UINT32 GetCopySourceArgument(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uDest)
/*****************************************************************************
 FUNCTION	: GetCopySourceArgument

 PURPOSE	: Get the source argument which is copied into a specified destination by
			  an instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction.
			  uDest			- Destination.

 RETURNS	: Either the index of the source argument or USC_UNDEF if the
			  destination isn't a copy of a source argument.

 NOTES      :
*****************************************************************************/
{
	if (!NoPredicate(psState, psInst))
	{
		return USC_UNDEF;
	}

	/*
		MOV	DEST = SRC0
	*/
	if (psInst->eOpcode == IMOV)
	{
		ASSERT(uDest == 0);
		return 0;
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psInst->eOpcode == IVMOV || psInst->eOpcode == IVPCKFLTFLT_EXP)
	{
		/*
			Check for a move instruction performing a format conversion.
		*/
		if (psInst->u.psVec->aeSrcFmt[0] != psInst->asDest[uDest].eFmt)
		{
			return USC_UNDEF;
		}

		/*
			Check for a move instruction with a source modifier.
		*/
		if (psInst->u.psVec->asSrcMod[0].bNegate || psInst->u.psVec->asSrcMod[0].bAbs)
		{
			return USC_UNDEF;
		}
		return GetVectorCopySourceArgument(psState, psInst, uDest);
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	return USC_UNDEF;
}

static IMG_VOID GetDefList(PINTERMEDIATE_STATE psState, PREGALLOC_DATA psRAData, PINST psInst, PDEF_LIST psDefList)
/*****************************************************************************
 FUNCTION	: GetDefList

 PURPOSE	: Get the list of intermediate registers potentially defined by an instrction.

 PARAMETERS	: psState		- Compiler state.
			  psRAData		- Register allocator state.
			  psInst		- Instruction.
			  psDefList		- Returns the list of intermediate registers.

 RETURNS	: Nothing.

 NOTES      :
*****************************************************************************/
{
	IMG_UINT32	uDestIdx;

	psDefList->uCount = 0;

	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		PARG		psDest = &psInst->asDest[uDestIdx];
		IMG_UINT32	uDestNodeRangeStart, uDestNodeRangeEnd;
		IMG_BOOL	bIndexed;
		IMG_UINT32	uCopyNode;

		/*
			Skip if this isn't a register handled by the register allocator.
		*/
		if (!RegIsNode(psRAData, psDest))
		{
			continue;
		}

		/*
			Skip an unwritten register.
		*/
		if (psInst->auDestMask[uDestIdx] == 0)
		{
			continue;
		}

		uCopyNode = USC_UNDEF;
		if (psDest->uIndexType != USC_REGTYPE_NOINDEX)
		{
			/*
				Get the range of variables potentially written by this destination.
			*/
			IndexedArgumentToNodeRange(psRAData, psDest, &uDestNodeRangeStart, &uDestNodeRangeEnd);
			bIndexed = IMG_TRUE;
		}
		else
		{
			IMG_UINT32	uCopySourceArg;

			uDestNodeRangeStart = ArgumentToNode(psRAData, psDest);
			uDestNodeRangeEnd = uDestNodeRangeStart + 1;
			bIndexed = IMG_FALSE;

			/*
				Check if this destination is a copy of one of the source arguments.
			*/
			uCopySourceArg = GetCopySourceArgument(psState, psInst, uDestIdx);
			if (uCopySourceArg != USC_UNDEF)
			{
				PARG	psCopySource;

				ASSERT(uCopySourceArg < psInst->uArgumentCount);
				psCopySource = &psInst->asArg[uCopySourceArg];
				if (RegIsNode(psRAData, psCopySource) && psCopySource->uIndexType == USC_REGTYPE_NOINDEX)
				{
					/*
						Don't add an edge to the interference graph between this
						destination and the source it is a copy of.
					*/
					uCopyNode = ArgumentToNode(psRAData, psCopySource);
				}
			}
		}

		psDefList->asList[psDefList->uCount].uStart = uDestNodeRangeStart;
		psDefList->asList[psDefList->uCount].uEnd = uDestNodeRangeEnd;
		psDefList->asList[psDefList->uCount].uMask = psInst->auDestMask[uDestIdx];
		psDefList->asList[psDefList->uCount].bIndexed = bIndexed;
		psDefList->asList[psDefList->uCount].uCopyNode = uCopyNode;
		psDefList->uCount++;
	}
}

static IMG_VOID AddDestRestrictions(PINTERMEDIATE_STATE psState, 
									PRAGCOL_STATE		psRegState,
									PINST				psInst, 
									IMG_UINT32			uDestIdx,
									IMG_UINT32			uDestNode,
									IMG_BOOL			bInstBlockPostSplit)
/*****************************************************************************
 FUNCTION	: AddDestRestrictions

 PURPOSE	: Add restrictions for the possible hardware registers which can be
			  assigned to an intermediate register used as an instruction destination.

 PARAMETERS	: psState				- Compiler state.
			  psRegState			- Register allocator state.
			  psInst				- Instruction.
			  uDestIdx				- Instruction destination.
			  uDestNode				- Register defined by the instruction.
			  bInstBlockPostSplit	- TRUE if this instruction executes at sample rate.

 RETURNS	: Nothing.

 NOTES      :
*****************************************************************************/
{
	IMG_BOOL		bUseOutputs;
	PREGALLOC_DATA	psRAData = &psRegState->sRAData;

	ASSERT(uDestNode < psRAData->uNrRegisters);

	/*
		If the output register bank can't be used in the destination of
		this instruction then make the destination interfere with all
		output registers.
	*/
	bUseOutputs = IMG_TRUE;
	/* Check for restrictions on which register banks can be used with this instruction. */
	if (!CanUseDest(psState, psInst, uDestIdx, USEASM_REGTYPE_OUTPUT, USC_REGTYPE_NOINDEX))
	{
		bUseOutputs = IMG_FALSE;
	}
	/* In pixel shaders instructions which write the output registers must have the SKIPINVALID flag set. */ 
	if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL && !GetBit(psInst->auFlag, INST_SKIPINV))
	{
		bUseOutputs = IMG_FALSE;
	}
	if (!bUseOutputs)
	{
		MakeInterfereWithAllOutputRegs(psRegState, uDestNode);
	}

	/*
		If the MSAATRANS flag is set on input then primary attributes
		can never be the destination of an instruction.
	*/
	if (psRegState->bNeverWritePrimaryAttributes || bInstBlockPostSplit)
	{
		MakeInterfereWithAllPAs(psRegState, uDestNode);
	}

	/*
		Flag this node as used so it is a potential candidate for spilling.
	*/
	SetBit(psRegState->asNodes[uDestNode].auFlags, NODE_FLAG_SPILLABLE, IMG_TRUE);
	SetBit(psRegState->asNodes[uDestNode].auFlags, NODE_FLAG_REFERENCED, IMG_TRUE);

	/*
		Mark nodes which are used in the sample rate part of the program. We need to restrict
		the possible range of hardware temporary registers for them.
	*/
	if (bInstBlockPostSplit)
	{
		SetBit(psRegState->asNodes[uDestNode].auFlags, NODE_FLAG_POSTSPLITPHASE, IMG_TRUE);
	}
}

static IMG_VOID ConstructInterferenceGraphForDestinations(PINTERMEDIATE_STATE	psState, 
														  PRAGCOL_STATE			psRegState,
														  PINST					psInst, 
														  PDEF_LIST				psDefList,
														  PLIVE_SET				psLiveSet,
														  IMG_BOOL				bInstBlockPostSplit)
/*****************************************************************************
 FUNCTION	: ConstructInterferenceGraphForDestinations

 PURPOSE	: Add all the register interference relations for the set of registers
			  defined by an instruction to the graph.

 PARAMETERS	: psState				- Compiler state.
			  psRegState			- Register allocator state.
			  psInst				- Instruction.
			  psDefList				- List of registers defined by the instruction.
			  psLiveSet				- Set of registers live before the instruction executes.
			  bInstBlockPostSplit	- TRUE if this instruction executes at sample rate.

 RETURNS	: Nothing.

 NOTES      :
*****************************************************************************/
{
	IMG_UINT32	uDestIdx;

	for (uDestIdx = 0; uDestIdx < psDefList->uCount; uDestIdx++)
	{
		IMG_UINT32	uNode;

		for (uNode = psDefList->asList[uDestIdx].uStart; uNode < psDefList->asList[uDestIdx].uEnd; uNode++)
		{
			/*
				Add restrictions from the instruction on the possible hardware registers which
				can be assigned to the instruction destinations.
			*/
			AddDestRestrictions(psState, psRegState, psInst, uDestIdx, uNode, bInstBlockPostSplit);

			/*
				Make the defined intermediate register interfere with all currently live registers.
			*/
			UpdateInterferenceGraph(psRegState, 
									psLiveSet, 
									uNode, 
									psDefList->asList[uDestIdx].uMask,
									psDefList->asList[uDestIdx].uCopyNode);

			/*
				Make the defined intermediate register interfere with all other intermediate
				register defined in the same instruction.
			*/
			AddEdgeForAllDefs(psRegState, psDefList, uNode);
		}
	}

	/*
		Remove all defined intermediate registers from the live set.
	*/
	for (uDestIdx = 0; uDestIdx < psDefList->uCount; uDestIdx++)
	{
		if (!psDefList->asList[uDestIdx].bIndexed)
		{
			IMG_UINT32	uNode;

			for (uNode = psDefList->asList[uDestIdx].uStart; uNode < psDefList->asList[uDestIdx].uEnd; uNode++)
			{
				SparseSetDeleteMember(psLiveSet->psLiveList, uNode);
			}
		}
	}
}

#if defined(OUTPUT_USPBIN)
static IMG_VOID AddUSPTempsToInterferenceGraph(PRAGCOL_STATE		psRegState,
											   PLIVE_SET			psLiveSet,
											   PARG					psBaseTempReg,
											   IMG_UINT32			uTempCount,
											   PDEF_LIST			psDefList,
											   IMG_BOOL				bInstBlockPostSplit)
/*****************************************************************************
 FUNCTION	: AddUSPTempsToInterferenceGraph

 PURPOSE	: Add all the register interference relations in an instruction to the graph.

 PARAMETERS	: psRegState			- Register allocator state.
			  psLiveSet				- Set of registers live before the instruction executes.
			  psBaseTempReg			- First register reserved for the USP.
			  uTempCount			- Count of registers reserved the USP.
			  psDefList				- List of registers defined by the instruction.
			  bInstBlockPostSplit	- TRUE if this instruction executes at sample rate.

 RETURNS	: Nothing.

 NOTES      :
*****************************************************************************/
{
	IMG_UINT32	uTempIdx;

	for (uTempIdx = 0; uTempIdx < uTempCount; uTempIdx++)
	{
		IMG_UINT32	uNode;

		/*
			Get the node corresponding to this temporary.
		*/
		uNode = ArgumentToNode(&psRegState->sRAData, psBaseTempReg) + uTempIdx;

		/*
			Flag this node as a temporary used for the USP.
		*/
		SetBit(psRegState->asNodes[uNode].auFlags, NODE_FLAG_USPTEMP, IMG_TRUE);

		/*
			Flag this node needing to be assigned a hardware register.
		*/
		SetBit(psRegState->asNodes[uNode].auFlags, NODE_FLAG_REFERENCED, IMG_TRUE);

		/*
			Make the node interfere with all the registers live
			before the instruction.
		*/
		UpdateInterferenceGraph(psRegState, psLiveSet, uNode, USC_DESTMASK_FULL, USC_UNDEF /* uSkipNode */);

		/*
			Never assign a USP temporary to an output register.
		*/
		MakeInterfereWithAllOutputRegs(psRegState, uNode);

		/*
			Don't assign USP temporary to primary attributes if we can't write
			to them.
		*/
		if (psRegState->bNeverWritePrimaryAttributes || bInstBlockPostSplit)
		{
			MakeInterfereWithAllPAs(psRegState, uNode);
		}

		/*
			Also make the node interfere with all the destination
			registers.
		*/
		AddEdgeForAllDefs(psRegState, psDefList, uNode);
	}
}
#endif /* defined(OUTPUT_USPBIN) */

static IMG_VOID ConstructInterferenceGraphForInst(PINTERMEDIATE_STATE	psState,
												  PRAGCOL_STATE			psRegState,
												  PINST					psInst,
												  PLIVE_SET				psLiveSet,
												  IMG_BOOL				bInstBlockPostSplit)
/*****************************************************************************
 FUNCTION	: ConstructInterferenceGraphForInst

 PURPOSE	: Add all the register interference relations in an instruction to the graph.

 PARAMETERS	: psState			- Compiler state.
			  psRegState		- Register allocator state.
			  psInst			- The instruction to add relations for.
			  psLiveSet			- The existing set of live registers.
			  bInstBlockPostSplit
								- Whether block in which instruction is present is 
								  post split point or not.

 RETURNS	: Nothing.

 NOTES      :
*****************************************************************************/
{		
	IMG_UINT32		uArgIdx;
	IMG_UINT32		uArgCount = psInst->uArgumentCount;
	DEF_LIST		sDefList;
	IMG_UINT32		uDestIdx;

	/*
	   Update interference graph with this instruction.
	*/

	/*
		Get a list of intermediate registers defined by this instruction.
	*/
	GetDefList(psState, &psRegState->sRAData, psInst, &sDefList);

	/*
		Update the interference graph for all the intermediate registers defined by this
		instruction.
	*/
	ConstructInterferenceGraphForDestinations(psState, psRegState, psInst, &sDefList, psLiveSet, bInstBlockPostSplit);

	/* Process the old values of partially/conditionally written destinations. */
	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		PARG	psOldDest = psInst->apsOldDest[uDestIdx];

		if (psOldDest != NULL && RegIsNode(&psRegState->sRAData, psOldDest))
		{
			UpdateInterferenceGraphForSrc(psRegState,
										  psInst,
										  psOldDest,
										  IMG_FALSE /* bOutputBankInvalid */,
										  GetPreservedChansInPartialDest(psState, psInst, uDestIdx),
										  psLiveSet,
										  &sDefList);
		}
	}

	/* Process the instruction source arguments. */
	for (uArgIdx = 0; uArgIdx < uArgCount; uArgIdx++)
	{
		PARG	psArg = &psInst->asArg[uArgIdx];

		if (RegIsNode(&psRegState->sRAData, psArg))
		{
			IMG_UINT32	uLiveChansInArg;
			IMG_BOOL	bOutputBankInvalid;

			uLiveChansInArg = GetLiveChansInArg(psState, psInst, uArgIdx);

			bOutputBankInvalid = IMG_FALSE;
			if (!CanUseSrc(psState, psInst, uArgIdx, USEASM_REGTYPE_OUTPUT, USC_REGTYPE_NOINDEX))
			{	
				bOutputBankInvalid = IMG_TRUE;
			}

			UpdateInterferenceGraphForSrc(psRegState,
										  psInst,
										  psArg,
										  bOutputBankInvalid,
										  uLiveChansInArg,
										  psLiveSet,
										  &sDefList);
		}
	}

	#if defined(OUTPUT_USPBIN)
	if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
	{
		/*
			Add to the interference graph the extra temporaries used when the
			USP expands the texture sample.
		*/
		if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_USPTEXTURESAMPLE)
		{
			IMG_UINT32	uTempCount;
			IMG_UINT32	uTempIdx;

			uTempCount = GetUSPPerSampleTemporaryCount(psState, psInst->u.psSmp->uTextureStage, psInst);
			AddUSPTempsToInterferenceGraph(psRegState,
										   psLiveSet,
										   &psInst->u.psSmp->sUSPSample.sTempReg,
										   uTempCount,
										   &sDefList,
										   bInstBlockPostSplit);

			for (uTempIdx = 0; uTempIdx < UF_MAX_CHUNKS_PER_TEXTURE; uTempIdx++)
			{
				IMG_UINT32	uNode;

				/*
				  Get the node corresponding to this temporary.
				*/
				uNode = ArgumentToNode(&(psRegState->sRAData), &psInst->asDest[uTempIdx]);

				/*
				  Flag this node as a temporary used for the USP.
				*/
				SetBit(psRegState->asNodes[uNode].auFlags, NODE_FLAG_USPTEMP, IMG_TRUE);

				/*
					Flag this node needing to be assigned a hardware register.
				*/
				SetBit(psRegState->asNodes[uNode].auFlags, NODE_FLAG_REFERENCED, IMG_TRUE);
								
				/*
				  Make the node interfere with all the registers live
				  before the instruction.
				*/
				UpdateInterferenceGraph(psRegState, psLiveSet, uNode, USC_DESTMASK_FULL, USC_UNDEF /* uSkipNode */);				

				/*
					Never assign a USP temporary to an output register.
				*/
				MakeInterfereWithAllOutputRegs(psRegState, uNode);

				/*
					Don't assign USP temporary to primary attributes if we can't write
					to them.
				*/
				if (psRegState->bNeverWritePrimaryAttributes || bInstBlockPostSplit)
				{
					MakeInterfereWithAllPAs(psRegState, uNode);
				}
			}
		}
		/*
			Add to the interference graph the extra temporaries used when the
			USP expands the texture sample unpack.
		*/
		else if (psInst->eOpcode == ISMPUNPACK_USP)
		{
			IMG_UINT32	uTempIdx;

			AddUSPTempsToInterferenceGraph(psRegState,
										   psLiveSet,
										   &psInst->u.psSmpUnpack->sTempReg,
										   GetUSPPerSampleUnpackTemporaryCount(),
										   &sDefList,
										   bInstBlockPostSplit);

			for (uTempIdx = 0; uTempIdx < UF_MAX_CHUNKS_PER_TEXTURE; uTempIdx++)
			{
				IMG_UINT32	uNode;

				/*
				  Get the node corresponding to this temporary.
				*/
				uNode = ArgumentToNode(&(psRegState->sRAData), &psInst->asArg[uTempIdx]);

				/*
				  Flag this node as a temporary used for the USP.
				*/
				SetBit(psRegState->asNodes[uNode].auFlags, NODE_FLAG_USPTEMP, IMG_TRUE);

				/*
					Flag this node needing to be assigned a hardware register.
				*/
				SetBit(psRegState->asNodes[uNode].auFlags, NODE_FLAG_REFERENCED, IMG_TRUE);

				/*
					Never assign a USP temporary to an output register.
				*/
				MakeInterfereWithAllOutputRegs(psRegState, uNode);
			}			
		}
		/*
			Add to the interference graph the extra temporaries used when the
			USP expands an texture write instruction
		*/
		else if (psInst->eOpcode == ITEXWRITE)
		{
			AddUSPTempsToInterferenceGraph(psRegState,
										   psLiveSet,
										   &psInst->u.psTexWrite->sTempReg,
										   GetUSPTextureWriteTemporaryCount(),
										   &sDefList,
										   bInstBlockPostSplit);
		}	
	}
	#endif /* defined(OUTPUT_USPBIN) */

	/*
	  For temporaries that might be needed for large immediates value make them interfere with
	  all registers that are live just before the instruction.
	*/
	ConstructInterferenceGraphForImmTemps(psState, psRegState, psLiveSet, psInst, bInstBlockPostSplit);
}

typedef struct _INTFGRAPH_CONTEXT
{
	/*
		Storage for the set of currently live registers.
	*/
	LIVE_SET		sLiveSet;
	/*
		Register allocator state.
	*/
	PRAGCOL_STATE	psRegState;
} INTFGRAPH_CONTEXT, *PINTFGRAPH_CONTEXT;

static IMG_VOID SetFlagForLiveRegisters(PRAGCOL_STATE psRegState, PLIVE_SET psLiveSet, IMG_UINT32 uFlag)
/*****************************************************************************
 FUNCTION	: SetFlagForLiveRegisters

 PURPOSE	: Set a flag in the PNODE_DATA structure for all currently live
			  registers.

 PARAMETERS	: psRegState	- Register allocator state.
			  psLiveSet		- Set of currently live registers.
			  uFlag			- Flag to set.

 RETURNS	: Nothing.
*****************************************************************************/
{
	SPARSE_SET_ITERATOR	sIter;

	for (SparseSetIteratorInitialize(psLiveSet->psLiveList, &sIter);
		 SparseSetIteratorContinue(&sIter);
		 SparseSetIteratorNext(&sIter))
	{
		IMG_UINT32	uReg = SparseSetIteratorCurrent(&sIter);
		SetBit(psRegState->asNodes[uReg].auFlags, uFlag, IMG_TRUE);
	}
}

static IMG_VOID VectorToLiveset(PINTERMEDIATE_STATE	psState,
								PRAGCOL_STATE		psRegState, 
								PLIVE_SET			psDest, 
								USC_PVECTOR			psSrc, 
								IMG_UINT32			uSrcOffset)
/*****************************************************************************
 FUNCTION	: VectorToLiveset

 PURPOSE	: Add all the registers in a bit-vector to a set of live registers.

 PARAMETERS	: psState			- Compiler state.
			  psRegState		- Register allocator state.
			  psDest			- Live set to copy to.
			  psSrc				- Vector to copy from.
			  uSrcOffset		- Offset to add onto register numbers from the
								vector.

 RETURNS	: Nothing.
*****************************************************************************/
{
	VECTOR_ITERATOR	sIter;

	for (VectorIteratorInitialize(psState, psSrc, VECTOR_LENGTH, &sIter); 
		 VectorIteratorContinue(&sIter); 
		 VectorIteratorNext(&sIter))
	{
		IMG_UINT32	uRegNum;
		IMG_UINT32	uNode;

		uRegNum = VectorIteratorCurrentPosition(&sIter) / VECTOR_LENGTH;
		uNode = uSrcOffset + uRegNum;
		
		if (!GetBit(psRegState->asNodes[uNode].auFlags, NODE_FLAG_ISSECATTR))
		{
			IMG_UINT32	uMask;

			uMask = VectorIteratorCurrentMask(&sIter);
			LiveSetAdd(psDest, uNode, uMask);
		}
	}
}

static IMG_VOID ConstructInterferenceGraphBP(PINTERMEDIATE_STATE	psState,
											 PCODEBLOCK				psCodeBlock,
											 IMG_PVOID				pvContext)
/*****************************************************************************
 FUNCTION	: ConstructInterferenceGraphForBlock

 PURPOSE	: Add all the register interference relations in a block to the graph.

 PARAMETERS	: psState			- Compiler state.
			  psRegState		- Register allocator state.
			  psCodeBlock		- Block to add to the graph.
=
 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST psInst;
	PINTFGRAPH_CONTEXT psContext = (PINTFGRAPH_CONTEXT)pvContext;
	PRAGCOL_STATE psRegState = psContext->psRegState;
	PLIVE_SET psLiveSet = &psContext->sLiveSet;

	/*
		Clear the set of currently live registers.
	*/
	SparseSetEmpty(psLiveSet->psLiveList);

	/* Load in the saved set of registers live at the end of the block. */
	VectorToLiveset(psState,
					psRegState, 
					psLiveSet, 
					&psCodeBlock->sRegistersLiveOut.sPrimAttr, 
					REGALLOC_PRIMATTR_START);
	VectorToLiveset(psState,
					psRegState, 
					psLiveSet, 
					&psCodeBlock->sRegistersLiveOut.sOutput, 
					psRegState->sRAData.uOutputStart);
	VectorToLiveset(psState,
					psRegState, 
					psLiveSet, 
					&psCodeBlock->sRegistersLiveOut.sTemp, 
					psRegState->sRAData.uTempStart);

	/* Save the registers live at the point where shader execution is suspended for feedback. */
	if (
			(psState->uFlags & USC_FLAGS_SPLITFEEDBACKCALC) != 0 &&
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_UNIFIED_TEMPS_AND_PAS) == 0 &&
			psState->psPreFeedbackDriverEpilogBlock == psCodeBlock
		)
	{
		SetFlagForLiveRegisters(psRegState, psLiveSet, NODE_FLAG_ALPHALIVE);
	}

	/* Save the registers live at the point where shader execution is suspended for split. */
	if (			
			(
				(psState->uFlags2 & USC_FLAGS2_SPLITCALC) != 0 &&
				psState->psPreSplitBlock == psCodeBlock
			)
		)
	{
		SetFlagForLiveRegisters(psRegState, psLiveSet, NODE_FLAG_SPLITLIVE);
	}
	{
		IMG_BOOL bPostSplitBlock;
		bPostSplitBlock = IsBlockDominatedByPreSplitBlock(psState, psCodeBlock);
		/*
		   Move backwards through the block constructing an interference graph.
		*/
		for (psInst = psCodeBlock->psBodyTail; psInst; psInst = psInst->psPrev)
		{
			ConstructInterferenceGraphForInst(psState,
											  psRegState,
											  psInst,
											  psLiveSet,
											  bPostSplitBlock);
		}
	}
}

static IMG_VOID ClearConflictingReservations(PRAGCOL_STATE	psRegState,
											 IMG_UINT32		uNode,
											 PCOLOUR		psColour)
/*****************************************************************************
 FUNCTION	: ClearConflictingReservations

 PURPOSE	: If a node has been coloured to a colour already reserved by other
			  nodes then clear the other node's reservation.

 PARAMETERS	: psRegState		- Module state.
			  uNode				- Node to clear reservations for.
			  psColour			- Colour assigned to the node.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PADJACENCY_LIST	psNodeList = IntfGraphGetVertexAdjacent(psRegState->psIntfGraph, uNode);
	ADJACENCY_LIST_ITERATOR sIter;
	IMG_UINT32 uOtherNode;

	for (uOtherNode = FirstAdjacent(psNodeList, &sIter); !IsLastAdjacent(&sIter); uOtherNode = NextAdjacent(&sIter))
	{
		PNODE_DATA	psOtherNode = &psRegState->asNodes[uOtherNode];

		/*
			Check if another node has already reserved this colour.
		*/
		if (psOtherNode->sReservedColour.eType == psColour->eType &&
			psOtherNode->sReservedColour.uNum == psColour->uNum)
		{	
			/*
				Clear the reservation.
			*/
			psOtherNode->sReservedColour.eType = COLOUR_TYPE_UNDEF;
			psOtherNode->sReservedColour.uNum = USC_UNDEF;
		}
	}
}

/*
	Information about the colours available for an intermediate register.
*/
typedef struct _COLOUR_STATUS
{
	/*
		Mask of the banks available.
	*/
	IMG_UINT32	uBankFlags;
	/*
		For each bank: a bit vector of the register numbers available in that bank.
	*/
	IMG_PUINT32	pauMask[COLOUR_TYPE_COUNT];
} COLOUR_STATUS, *PCOLOUR_STATUS;

static IMG_VOID ClearBitInColourVector(PCOLOUR_STATUS psStatus, PCOLOUR psColour, IMG_UINT32 uOffset, IMG_UINT32 uAlign)
/*****************************************************************************
 FUNCTION	: ClearBitInColourVector

 PURPOSE	: Remove a colour from a set of available colours.

 PARAMETERS	: psStatus			- Set of available colours.
			  psColour			- Colour to remove.
			  uOffset			- 
			  uAlign			-

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psColour->eType >= COLOUR_TYPE_COUNT)
	{
		return;
	}
	if (psColour->uNum >= uOffset)
	{
		IMG_UINT32	uColourNum;
		uColourNum = psColour->uNum - uOffset;
		if ((uColourNum & ((1 << uAlign) - 1)) == 0)
		{
			SetBit(psStatus->pauMask[psColour->eType], uColourNum >> uAlign, 0);
		}
	}
}

static
IMG_UINT32 GetMOEForGPI(PINTERMEDIATE_STATE	psState, PINST psInst, IMG_BOOL bDest, IMG_UINT32 uArgIdx)
/*****************************************************************************
 FUNCTION	: GetMOEForGPI

 PURPOSE	: Gets the units of MOE base/increments used by an instruction, if
			  the given argument or destination was a GPI.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction to check.
			  bDest		- Set if the argument is a destination.
			  uArgIdx	- Instruction argument to check.

 RETURNS	: The units in bytes.
*****************************************************************************/
{
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)

	IMG_UINT32 uFlagToCheck;
	/*
		For cores with the vector instruction set 128-bit MOE units for internal registers
		regardless of the type of instruction they are used with.
	*/
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		return DQWORD_SIZE_LOG2;
	}

	/*
		64-bit units for source arguments to SMP on cores with the vector instructions. 
	*/
	if (	
			!bDest &&
			(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_DEPENDENTTEXTURESAMPLE) != 0 &&
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0
		)
	{
		return QWORD_SIZE_LOG2;
	}

	/*
		Special case for vector dual-issue instruction: if the result written to the unified store
		destination is scalar then it has dword units.
	*/
	if (
			psInst->eOpcode == IVDUAL && 
			bDest && 
			uArgIdx == VDUAL_DESTSLOT_UNIFIEDSTORE && 
			!IsVDUALUnifiedStoreVectorResult(psInst)
		)
	{
		return LONG_SIZE_LOG2;
	}

	/*
		Check the instruction description. 
	*/
	if (bDest)
	{
		uFlagToCheck = DESC_FLAGS2_DEST64BITMOEUNITS;
	}
	else
	{
		uFlagToCheck = DESC_FLAGS2_SOURCES64BITMOEUNITS;
	}
	if ((g_psInstDesc[psInst->eOpcode].uFlags2 & uFlagToCheck) != 0)
	{
		return QWORD_SIZE_LOG2;
	}
#else
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst);
	PVR_UNREFERENCED_PARAMETER(bDest);
	PVR_UNREFERENCED_PARAMETER(uArgIdx);
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Otherwise default to 32-bit units.
	*/
	return LONG_SIZE_LOG2;
}

static
IMG_BOOL MOEAllowsGPIEverywhere(PINTERMEDIATE_STATE	psState,
								IMG_UINT32			uTempRegNum, 
								IMG_UINT32			uGPIRegNum)
/*****************************************************************************
 FUNCTION	: MOEAllowsGPIEverywhere

 PURPOSE	: Check whether a given temp register can be replaced by a given GPI
			  everywhere, given the allowed register numbers of an instruction
			  are restricted by the units of MOE base/increments used by the instruction.

 PARAMETERS	: psState		- Compiler state.
			  uTempRegNum	- Temporary register to replace.
			  uGPIRegNum	- The GPI to replace the temporary register with.

 RETURNS	: TRUE if possible, FALSE otherwise.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psUseDef;
	PUSC_LIST_ENTRY	psListEntry;
	PINST			psDefInst;
	PCODEBLOCK		psDefineBlock;
	UF_REGFORMAT	eDefFmt;
	IMG_UINT32		uMOEUnitsInBytesLog2;
	
	psUseDef = UseDefGet(psState, USEASM_REGTYPE_TEMP, uTempRegNum);
	
	/*
		If this temporary is not used return FALSE.
		If this temporary is defined in the driver prolog then it can't be replaced by an
		internal register.
	*/
	ASSERT(psUseDef != NULL && psUseDef->psDef != NULL && psUseDef->psDef->eType == DEF_TYPE_INST);
	
	psDefInst = psUseDef->psDef->u.psInst;
	psDefineBlock = psDefInst->psBlock;
	eDefFmt = psDefInst->asDest[psUseDef->psDef->uLocation].eFmt;
	
	if (psUseDef->psDef->uLocation == 0)
	{
		uMOEUnitsInBytesLog2 = GetMOEForGPI(psState, psDefInst, IMG_TRUE, 0);

		if (uMOEUnitsInBytesLog2 > LONG_SIZE_LOG2)
		{
			IMG_UINT32	uAdjustLog2 = uMOEUnitsInBytesLog2 - LONG_SIZE_LOG2;

			if ((uGPIRegNum % (1U << uAdjustLog2)) != 0)
			{
				return IMG_FALSE;
			}
		}
	}

	ASSERT(g_psInstDesc[psDefInst->eOpcode].eType != INST_TYPE_PCK ||
		psDefInst->auDestMask[psUseDef->psDef->uLocation] == USC_ALL_CHAN_MASK);

	ASSERT(eDefFmt != UF_REGFORMAT_C10 && eDefFmt != UF_REGFORMAT_U8);

	for (psListEntry = psUseDef->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF		psUse;
		PINST		psReader;
		IMG_UINT32	uReaderArg;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

		if (psUse == psUseDef->psDef)
		{
			continue;
		}

		ASSERT(psUse->eType == USE_TYPE_SRC);

		psReader = psUse->u.psInst;
		uReaderArg = psUse->uLocation;

		ASSERT(psReader->psBlock == psDefineBlock);
		
		uMOEUnitsInBytesLog2 = GetMOEForGPI(psState, psReader, IMG_FALSE, uReaderArg);

		if (uMOEUnitsInBytesLog2 > LONG_SIZE_LOG2)
		{
			IMG_UINT32	uAdjustLog2 = uMOEUnitsInBytesLog2 - LONG_SIZE_LOG2;

			if ((uGPIRegNum % (1U << uAdjustLog2)) != 0)
			{
				return IMG_FALSE;
			}
		}
	}

	return IMG_TRUE;
}

static IMG_BOOL UsedAsSrcInF16FmtControlInst(PINTERMEDIATE_STATE	psState,
											 IMG_UINT32				uRegNum)
/*****************************************************************************
 FUNCTION	: UsedAsSrcInF16FmtControlInst

 PURPOSE	: Check whether a given temporary register is used anywhere as a source
			  to an instruction which uses F16 format control.

 PARAMETERS	: psState	- Compiler state.
			  uRegNum	- The temp register to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psUseDef;
	PUSC_LIST_ENTRY	psListEntry;
	PINST			psDefInst;
	
	psUseDef = UseDefGet(psState, USEASM_REGTYPE_TEMP, uRegNum);
	
	ASSERT(psUseDef != NULL && psUseDef->psDef != NULL && psUseDef->psDef->eType == DEF_TYPE_INST);
	
	psDefInst = psUseDef->psDef->u.psInst;

	ASSERT(g_psInstDesc[psDefInst->eOpcode].eType != INST_TYPE_PCK ||
		   psDefInst->auDestMask[psUseDef->psDef->uLocation] == USC_ALL_CHAN_MASK);

	ASSERT(psDefInst->asDest[psUseDef->psDef->uLocation].eFmt != UF_REGFORMAT_C10 && 
		   psDefInst->asDest[psUseDef->psDef->uLocation].eFmt != UF_REGFORMAT_U8);

	for (psListEntry = psUseDef->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF		psUse;
		PINST		psReader;
		IMG_UINT32	uReaderArg;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

		if (psUse->eType != USE_TYPE_SRC)
		{
			continue;
		}

		psReader = psUse->u.psInst;
		uReaderArg = psUse->uLocation;

		ASSERT(psReader->psBlock == psDefInst->psBlock);

		/*
			Check for instructions which don't support an internal register as an argument.
		*/
		if (psReader->asArg[uReaderArg].eFmt == UF_REGFORMAT_F16 &&
			InstUsesF16FmtControl(psReader))
		{
			return IMG_TRUE;
		}
	}

	return IMG_FALSE;
}

static IMG_VOID BaseGetAvailableColourVector(PRAGCOL_STATE	psRegState, 
											 PCOLOUR_STATUS psStatus,
											 IMG_UINT32		uNode,
											 IMG_UINT32		uOffset,
											 IMG_UINT32		uAlign,
											 IMG_BOOL		bIncludeReservations)
/*****************************************************************************
 FUNCTION	: BaseGetAvailableColourVector

 PURPOSE	: Remove colours which can't be allocated to a register from a set.

 PARAMETERS	: psRegState		- Module state.
			  psStatus			- Set of available colours.
			  uNode				- Intermediate register.
			  uOffset			- 
			  uAlign			- 
			  bIncludeReservations
								- Remove colours from the bit vector which are 
								reserved by registers which are part of an optional group.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PADJACENCY_LIST			psNodeList = IntfGraphGetVertexAdjacent(psRegState->psIntfGraph, uNode);
	IMG_UINT32				uOtherNode;
	ADJACENCY_LIST_ITERATOR sIter;
	PINTERMEDIATE_STATE		psState = psRegState->sRAData.psState;

	/*
		Remove register banks not available for this node.
	*/
	psStatus->uBankFlags &= psRegState->asNodes[uNode].uBankFlags;

	for (uOtherNode = FirstAdjacent(psNodeList, &sIter); !IsLastAdjacent(&sIter); uOtherNode = NextAdjacent(&sIter))
	{
		PNODE_DATA	psOtherNode = &psRegState->asNodes[uOtherNode];
		IMG_UINT32	uColourIdx;

		/*
			Check if another node has already reserved this colour.
		*/
		if (bIncludeReservations && psOtherNode->sReservedColour.eType != COLOUR_TYPE_UNDEF)
		{
			ClearBitInColourVector(psStatus, &psOtherNode->sReservedColour, uOffset, uAlign);
			continue;
		}

		/*
			Skip nodes we haven't coloured yet.
		*/
		if (IntfGraphIsVertexRemoved(psRegState->psIntfGraph, uOtherNode))
		{
			continue;
		}

		/*
			Fail if another node has already been given the same colour.
		*/
		for (uColourIdx = 0; uColourIdx < psOtherNode->uColourCount; uColourIdx++)
		{
			ClearBitInColourVector(psStatus, &psOtherNode->asColours[uColourIdx], uOffset, uAlign);
		}
	}
	
	/* Check the register doesn't interfere with already allocated GPIs */
	if ((psStatus->uBankFlags & BANK_FLAG_GPI) != 0)
	{
		IMG_UINT32 uGPI;
		for (uGPI = 0; uGPI < psRegState->sRAData.auAvailRegsPerType[COLOUR_TYPE_GPI]; uGPI++)
		{
			IMG_UINT32	uGPIColour, uRegNum, uRegType;
			PINST		psDefineInst, psLastUseInst;

			uGPIColour = psRegState->sRAData.uGPIStart + uGPI;

			NodeToRegister(&psRegState->sRAData, uNode, &uRegType, &uRegNum);

			psDefineInst = UseDefGetDefInst(psState, uRegType, uRegNum, NULL);
			psLastUseInst = UseDefGetLastUse(psState, uRegType, uRegNum);

			ASSERT(CanReplaceTempWithAnyGPI(psState, uRegNum, NULL, NULL, IMG_TRUE));
			
			/*
				Using i2 or i3 if UsedAsSrcInF16FmtControlInst returns true would require extra
				SMBO instructions, so it's not worth it.
				Also might not be able to use i2 if GPIs are encoded as large temp registers.
			*/

			if (!MOEAllowsGPIEverywhere(psState, uRegNum, uGPI) ||
				UseDefIsIRegLiveInInternal(psState, uGPI, psDefineInst, psLastUseInst) ||
				(uGPI > 1 && UsedAsSrcInF16FmtControlInst(psState, uRegNum)))
			{
				COLOUR sColour;
				sColour.eType = COLOUR_TYPE_GPI; 
				sColour.uNum = uGPI;

				ClearBitInColourVector(psStatus, &sColour, uOffset, uAlign);
			}
		}
	}
}

static IMG_VOID FreeColourStatus(PINTERMEDIATE_STATE psState, PCOLOUR_STATUS psStatus)
/*****************************************************************************
 FUNCTION	: FreeColourStatus

 PURPOSE	: Free storage for the set of colours available for an intermediate
			  register.

 PARAMETERS	: psState			- Compiler state.
			  psStatus			- Set to free.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uColour;

	UscFree(psState, psStatus->pauMask[0]);
	for (uColour = 0; uColour < COLOUR_TYPE_COUNT; uColour++)
	{
		psStatus->pauMask[uColour] = NULL;
	}
}

static IMG_VOID AllocColourStatus(PINTERMEDIATE_STATE psState, PRAGCOL_STATE psRegState, PCOLOUR_STATUS psStatus)
/*****************************************************************************
 FUNCTION	: AllocColourStatus

 PURPOSE	: Allocate storage for the set of colours available for an intermediate
			  register.

 PARAMETERS	: psState			- Compiler state.
			  psRegState		- Register allocator status.
			  psStatus			- Initialized with the allocated storage.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uNumUintsToAlloc;
	IMG_PUINT32	auMask;
	IMG_UINT32	uColour;

	/*	
		Count the size of the bit vector for each register bank.
	*/
	uNumUintsToAlloc = 0;
	for (uColour = 0; uColour < COLOUR_TYPE_COUNT; uColour++)
	{
		uNumUintsToAlloc += UINTS_TO_SPAN_BITS(psRegState->sRAData.auAvailRegsPerType[uColour]);
	}

	/*
		Allocate one heap block for all register banks.
	*/
	auMask = UscAlloc(psState, uNumUintsToAlloc * sizeof(IMG_UINT32));

	/*
		Set up the pointers for each register bank.
	*/
	for (uColour = 0; uColour < COLOUR_TYPE_COUNT; uColour++)
	{
		psStatus->pauMask[uColour] = auMask;
		auMask += UINTS_TO_SPAN_BITS(psRegState->sRAData.auAvailRegsPerType[uColour]);
	}
}

static IMG_VOID ResetColourStatus(PRAGCOL_STATE	psRegState, PCOLOUR_STATUS psStatus)
/*****************************************************************************
 FUNCTION	: ResetColourStatus

 PURPOSE	: Reset a set of available colours so all colours are available.

 PARAMETERS	: psRegState		- Register allocator status.
			  psStatus			- Set of available colours.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uColour;

	psStatus->uBankFlags = BANK_FLAG_ALL;
	for (uColour = 0; uColour < COLOUR_TYPE_COUNT; uColour++)
	{
		memset(psStatus->pauMask[uColour], 
			   0xFF, 
			   UINTS_TO_SPAN_BITS(psRegState->sRAData.auAvailRegsPerType[uColour]) * sizeof(IMG_UINT32));
	}
}

static IMG_VOID GetAvailableColourVector(PRAGCOL_STATE	psRegState, 
										 IMG_UINT32		uNode,
										 PCOLOUR_STATUS psStatus, 
										 IMG_BOOL		bIncludeReservations)
/*****************************************************************************
 FUNCTION	: GetAvailableColourVector

 PURPOSE	: Return a bit vector of the colours which can be allocated by an
			  intermediate register.

 PARAMETERS	: psRegState		- Module state.
			  uNode				- Intermediate register to get the valid colours for.
			  psStatus			- Returns the set of available colours.
			  bIncludeReservations
								- Remove colours from the bit vector which are 
								reserved by registers which are part of an optional group.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ResetColourStatus(psRegState, psStatus);
	BaseGetAvailableColourVector(psRegState, psStatus, uNode, 0 /* uOffset */, 0 /* uAlign */, bIncludeReservations);
}

static IMG_BOOL GetAvailableColour(PCOLOUR_STATUS		psAvailableColours,
								   PCOLOUR_RANGE_LIST	psColourRanges,
								   IMG_UINT32			uLength,
								   HWREG_ALIGNMENT		eStartAlign,
								   IMG_UINT32			uLengthAlign,
								   PCOLOUR				psColour)
/*****************************************************************************
 FUNCTION	: GetAvailableColour

 PURPOSE	: Select the first available colours from a set of the colours
			  available for an intermediate register.

 PARAMETERS	: psAvailableColours	- Set of available colours.
			  psColourRanges		- Ranges of colours to consider.
			  uLength				- 
			  eStartAlign			- 
			  uLengthAlign			- 
			  psColour				- Returns the selected colour.

 RETURNS	: TRUE if a colour was available.
*****************************************************************************/
{
	IMG_UINT32			uRangeIdx;

	for (uRangeIdx = 0; uRangeIdx < psColourRanges->uRangeCount; uRangeIdx++)
	{
		PCOLOUR_RANGE	psColourRange = &psColourRanges->asRanges[uRangeIdx];
		PRANGE			psRange = &psColourRange->asRanges[eStartAlign][uLengthAlign];
		IMG_UINT32		uColour;
		IMG_UINT32		uRangeStart = psRange->uStart;
		IMG_UINT32		uRangeLength = psRange->uLength;

		/*
			Check if the bank restrictions on the instructions using this intermediate
			register mean it can't be assigned to any colour in this range.
		*/
		if ((psAvailableColours->uBankFlags & psColourRange->uBankFlag) == 0)
		{
			continue;
		}

		/*
			For groups of registers: don't consider hardware register numbers for the first
			register in the group where the hardware register number for the last register
			in the group would be greater than the maximum valid.
		*/
		if (uRangeLength < uLength)
		{
			continue;
		}
		uRangeLength = uRangeLength - (uLength - 1);

		/*
			Look for a set bit.
		*/
		uColour = FindFirstSetBitInRange(psAvailableColours->pauMask[psColourRange->eType], uRangeStart, uRangeLength);
		if (uColour == USC_UNDEF)
		{
			continue;
		}

		psColour->eType = psColourRange->eType;
		psColour->uNum = uColour;
		return IMG_TRUE;
	}

	return IMG_FALSE;
}

static IMG_BOOL GetColourForGroup(PINTERMEDIATE_STATE	psState,
								  PRAGCOL_STATE			psRegState,
								  PREGISTER_GROUP		psBaseGroup, 
								  IMG_UINT32			uLength,
								  PCOLOUR_STATUS		psStatus, 
								  IMG_BOOL				bIncludeReservations, 
								  PCOLOUR_RANGE_LIST	psColourRangeList,
								  PCOLOUR				psBaseColour)
/*****************************************************************************
 FUNCTION	: GetColourForGroup

 PURPOSE	: Find a range of colours available for a group of intermediate
			  registers which require consecutive hardware registers.

 PARAMETERS	: psState				- Compiler state.
			  psRegState			- Module state.
			  psBaseGroup			- Start of the group.
			  uLength				- Number of registers in the group.
			  psStatus				- Temporary storage.
			  bIncludeReservations	- Don't consider ranges containing colours
									reserved by registers which are part of an optional group.
			  psColourRangeList		- Ranges of colours to consider.
			  psBaseColour			- On success returns the start of the range.

 RETURNS	: TRUE if a range was available.
*****************************************************************************/
{
	PREGISTER_GROUP	psGroup;
	IMG_UINT32		uOffsetInGroup;
	IMG_UINT32		uAlign;
	HWREG_ALIGNMENT	eNodeAlignment;
	IMG_UINT32		uStartOffset;
	IMG_UINT32		uAlignedLength;

	/*
		Reset the set of available colours.
	*/
	ResetColourStatus(psRegState, psStatus);

	/*
		If the start of the range of hardware register numbers assigned to the group requires
		a particular alignment then renumber the set so bit B represents colour
		'B * alignment'.
	*/
	eNodeAlignment = GetNodeAlignment(psBaseGroup);
	switch (eNodeAlignment)
	{
		case HWREG_ALIGNMENT_NONE:	uStartOffset = 0; uAlign = 0; break; 
		case HWREG_ALIGNMENT_EVEN:	uStartOffset = 0; uAlign = 1; break;
		case HWREG_ALIGNMENT_ODD:	uStartOffset = 1; uAlign = 1; break;
		default: imgabort();
	}

	/*
		Take the intersection of the colours available for all the registers in the group. If the Pth node
		conflicts with colour C then remove bit 'C - P' from the set.
	*/
	for (uOffsetInGroup = uStartOffset, psGroup = psBaseGroup; psGroup != NULL; psGroup = psGroup->psNext, uOffsetInGroup++)
	{
		IMG_UINT32	uGroupNode;

		uGroupNode = RegisterToNode(&psRegState->sRAData, USEASM_REGTYPE_TEMP, psGroup->uRegister);
		BaseGetAvailableColourVector(psRegState, psStatus, uGroupNode, uOffsetInGroup, uAlign, bIncludeReservations);
	}

	/*
		Check if at least one range of colours is available.
	*/
	uAlignedLength = ((uLength - 1) >> uAlign) + 1;
	if (GetAvailableColour(psStatus, 
						   psColourRangeList, 
						   uAlignedLength, 
						   eNodeAlignment, 
						   uLength & 1, 
						   psBaseColour))
	{
		/*
			Transform the bit position back to a register number.
		*/
		psBaseColour->uNum <<= uAlign;
		psBaseColour->uNum += uStartOffset;
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_VOID SetGroupColour(PINTERMEDIATE_STATE	psState,
							   PRAGCOL_STATE		psRegState,
							   PREGISTER_GROUP		psBaseGroup,
							   IMG_UINT32			uNode,
							   IMG_UINT32			uColourIdx,
							   PCOLOUR				psBaseColour,
							   IMG_BOOL				bHonour)
/*****************************************************************************
 FUNCTION	: SetGroupColour

 PURPOSE	: Set the colours assigned to an whole group of intermediate registers.

 PARAMETERS	: psState				- Compiler state.
			  psRegState			- Module state.
			  psBaseGroup			- Start of the group.
			  uNode					- Node from the group which was the trigger
									for colouring the whole group.
			  uColourIdx			- Index of the colour to set.
			  psBaseColour			- Colour assigned to the start of the group.
			  bHonour				- TRUE if reservations were honoured when
									allocating the colour.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_BOOL		bPrevWasOptional;
	IMG_UINT32		uOffsetInGroup;
	PREGISTER_GROUP	psGroup;
	PREGISTER_GROUP	psGroupNext;

	bPrevWasOptional = IMG_TRUE;
	for (psGroup = psBaseGroup, uOffsetInGroup = 0; psGroup != NULL; psGroup = psGroupNext, uOffsetInGroup++)
	{
		COLOUR		sNodeColour;
		IMG_BOOL	bOptionalGroup;
		IMG_UINT32	uGroupNode;

		sNodeColour.eType = psBaseColour->eType;
		sNodeColour.uNum = psBaseColour->uNum + uOffsetInGroup;

		psGroupNext = psGroup->psNext;
		uGroupNode = RegisterToNode(&psRegState->sRAData, USEASM_REGTYPE_TEMP, psGroup->uRegister);
		bOptionalGroup = psGroup->bOptional;

		/* Colour mandatory group members or the current node */
		if (!bOptionalGroup || !bPrevWasOptional || uGroupNode == uNode)
		{
			SetNodeColour(psRegState, uGroupNode, uColourIdx, &sNodeColour);
			if (!bHonour)
				ClearConflictingReservations(psRegState, uGroupNode, &sNodeColour);
			if (bPrevWasOptional && psGroup->psPrev != NULL)
				DropLinkAfterNode(psState, psGroup->psPrev);
			if (bOptionalGroup)
				DropLinkAfterNode(psState, psGroup);
		}
		/* Reserve the colour for optional group members */
		else
		{
			ReserveNodeColour(psRegState, uGroupNode, &sNodeColour);

			/* Disconnect the current node */
			RemoveFromGroup(psState, psGroup);
		}

		bPrevWasOptional = bOptionalGroup;
	}
}

#if defined(DEBUG)
static IMG_BOOL ValidColourForRange(PCOLOUR_RANGE_LIST	psColourRanges,
									PCOLOUR				psColour)
/*****************************************************************************
 FUNCTION	: ValidColourForRange

 PURPOSE	: Check if a colour is in one of a list of ranges.

 PARAMETERS	: psColourRanges	- List of ranges to check.
			  psColour			- Colour to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uRangeIdx;

	/*
		If this node has restrictions on the available colours check the already assigned colour
		satisfies the restrictions.
	*/
	for (uRangeIdx = 0; uRangeIdx < psColourRanges->uRangeCount; uRangeIdx++)
	{
		PCOLOUR_RANGE	psRange = &psColourRanges->asRanges[uRangeIdx];
		COLOUR_TYPE		eRangeType = psRange->eType;
		IMG_UINT32		uRangeStart = psRange->asRanges[0][0].uStart;
		IMG_UINT32		uRangeEnd = uRangeStart + psRange->asRanges[0][0].uLength - 1;

		if (psColour->eType == eRangeType && psColour->uNum >= uRangeStart && psColour->uNum <= uRangeEnd)
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}
#endif /* defined(DEBUG) */

static IMG_BOOL CanNodeUseColour(PRAGCOL_STATE	psRegState,
								 IMG_UINT32		uNode,
								 PCOLOUR		psColour,
								 IMG_BOOL		bHonour,
								 IMG_PBOOL		pbOverWrite)
/*****************************************************************************
 FUNCTION	: CanNodeUseColour

 PURPOSE	: Check if a node can be given a specified colour.

 PARAMETERS	: psRegState		- Module state.
			  uNode				- Node to check.
			  psColour			- Colour to check.
			  bHonour			- Whether to honour colour reservations.
			  pbOverWrite		- Returns TRUE if a colour reserved was overridden.

 RETURNS	: IMG_TRUE if the node could be given the colour; IMG_FALSE otherwise.
*****************************************************************************/
{
	PADJACENCY_LIST			psNodeList = IntfGraphGetVertexAdjacent(psRegState->psIntfGraph, uNode);
	ADJACENCY_LIST_ITERATOR sIter;
	IMG_UINT32				uOtherNode;
	PINTERMEDIATE_STATE		psState = psRegState->sRAData.psState;

	for (uOtherNode = FirstAdjacent(psNodeList, &sIter);
		 !IsLastAdjacent(&sIter);
		 uOtherNode = NextAdjacent(&sIter))
	{
		PNODE_DATA	psOtherNode = &psRegState->asNodes[uOtherNode];
		IMG_UINT32 uColourIdx;

		/*
		  Check if another node has already reserved this colour.
		*/
		if (psOtherNode->sReservedColour.eType == psColour->eType &&
			psOtherNode->sReservedColour.uNum == psColour->uNum)
		{
			if (bHonour)
			{
				/*
				  If honouring reservations then fail.
				*/
				return IMG_FALSE;
			}
			else
			{
				/*
				  Otherwise just note that a reservation was ignored.
				*/
				*pbOverWrite = IMG_TRUE;
			}
		}

		/*
		  Skip nodes we haven't coloured yet.
		*/
		if (IntfGraphIsVertexRemoved(psRegState->psIntfGraph, uOtherNode))
		{
			continue;
		}

		/*
			Fail if another node has already been given the same colour.
		*/
		for (uColourIdx = 0; uColourIdx < psOtherNode->uColourCount; uColourIdx++)
		{
			if (psOtherNode->asColours[uColourIdx].eType == psColour->eType &&
				psOtherNode->asColours[uColourIdx].uNum == psColour->uNum)
			{
				return IMG_FALSE;
			}
		}
	}

	/* Check the register doesn't interfere with already allocated GPIs */
	if (psColour->eType == USEASM_REGTYPE_FPINTERNAL)
	{
		IMG_UINT32	uRegNum, uRegType;
		PINST		psDefineInst, psLastUseInst;

		NodeToRegister(&psRegState->sRAData, uNode, &uRegType, &uRegNum);

		ASSERT(psColour->eType == USEASM_REGTYPE_FPINTERNAL);

		psDefineInst = UseDefGetDefInst(psState, uRegType, uRegNum, NULL);
		psLastUseInst = UseDefGetLastUse(psState, uRegType, uRegNum);

		ASSERT(CanReplaceTempWithAnyGPI(psState, uRegNum, NULL, NULL, IMG_TRUE));
		
		if (!MOEAllowsGPIEverywhere(psState, uRegNum, psColour->uNum))
		{
			return IMG_FALSE;
		}

		/*
			Using i2 or i3 if UsedAsSrcInF16FmtControlInst returns true would require extra
			SMBO instructions, so it's not worth it.
			Also might not be able to use i2 if GPIs are encoded as large temp registers.
		*/

		if (UseDefIsIRegLiveInInternal(psState, psColour->uNum, psDefineInst, psLastUseInst) ||
			(psColour->uNum > 1 && UsedAsSrcInF16FmtControlInst(psState, uRegNum)))
		{
			return IMG_FALSE;
		}
	}

	/*
	   If no nodes this node interferes with were the current colour then
	   we have succeeded.
	*/
	return IMG_TRUE;
}

static IMG_BOOL ColourSingleNode(PRAGCOL_STATE		psRegState, 
								 PCOLOUR_STATUS		psStatus,
								 IMG_UINT32			uNode, 
								 IMG_UINT32			uColourIdx,
								 IMG_BOOL			bHonour, 
								 PCOLOUR_RANGE_LIST	psColourRangeList)
/*****************************************************************************
 FUNCTION	: ColourSingleNode

 PURPOSE	: Colour an intermediate register which isn't part of a group
			  of registers which have to be given consecutive hardware register
			  numbers.

 PARAMETERS	: psRegState		- Module state.
			  psStatus			- Temporary storage for the set of available
								colours.
			  uNode				- Intermediate register to colour.
			  uColourIdx		- Index of the colour to set.
			  bHonour			- TRUE if honour colour reservations when
								allocating.
			  psColourRangeList	- Range of colours to consider.

 RETURNS	: TRUE if the node was coloured.
*****************************************************************************/
{
	COLOUR				sNodeColour;

	/*
		Get the set of colours available for this node.
	*/
	GetAvailableColourVector(psRegState, uNode, psStatus, bHonour);

	/*
		Check if at least one colour is available.
	*/
	if (!GetAvailableColour(psStatus, 
							psColourRangeList, 
							1 /* uLength */, 
							HWREG_ALIGNMENT_NONE, 
							0 /* uLengthAlign */, 
							&sNodeColour))
	{
		return IMG_FALSE;
	}
		
	/*
		Mark the node as having been assigned a colour.
	*/
	SetNodeColour(psRegState, uNode, uColourIdx, &sNodeColour);
	if (!bHonour)
		ClearConflictingReservations(psRegState, uNode, &sNodeColour);

	return IMG_TRUE;
}

static IMG_VOID GetGroupStartAndSize(PRAGCOL_STATE			psRegState,
									 PREGISTER_GROUP		psNode,
									 IMG_PUINT32			puBaseNode,
									 PREGISTER_GROUP*		ppsBaseGroup,
									 IMG_PUINT32			puLength)
/*****************************************************************************
 FUNCTION	: GetGroupStartAndSize

 PURPOSE	: Given a node in a group gets the first node in the group and the
			  size of the group.

 PARAMETERS	: psState			- Compiler state.
			  psRegState		- Module state.
			  psNode			- Node which is in a group.
			  puBaseNode		- Returns the first node in the group.
			  ppsBaseNode		- Returns group information about the first node.
			  puLength			- Returns the size of the group.

 RETURNS	: Nothing
*****************************************************************************/
{
	IMG_UINT32		uLength;
	PREGISTER_GROUP psGroup;

	/* Find the first node in the group which this node is part of. */
	uLength = 1;

	/* Count the nodes preceding the current node in the group */
	for (psGroup = psNode; psGroup->psPrev != NULL; psGroup = psGroup->psPrev, uLength++);

	/* Store node starting the group */
	*puBaseNode = RegisterToNode(&psRegState->sRAData,  USEASM_REGTYPE_TEMP, psGroup->uRegister);
	*ppsBaseGroup = psGroup;

	/* Count the rest of the nodes in the group */
	for (psGroup = psNode; psGroup->psNext != NULL; psGroup = psGroup->psNext, uLength++);

	*puLength = uLength;
}

static IMG_BOOL DisconnectOptionalGroupParts(PREGALLOC_DATA psRAData, IMG_UINT32 uNode)
/*****************************************************************************
 FUNCTION	: DisconnectOptionalGroupParts

 PURPOSE	: Drop some optional parts of a group containing a specified node.

 PARAMETERS	: psRAData		- Module state.
			  uNode			- Index of the node to disconnect.

 RETURNS	: TRUE if some optional parts were dropped.
*****************************************************************************/
{
	PREGISTER_GROUP psNode = GetRegisterGroup(psRAData, uNode);
	PREGISTER_GROUP	psGroupNode;
	IMG_BOOL		bDropped;

	bDropped = IMG_FALSE;

	/*
		Step forward in the group starting at the node.
	*/
	psGroupNode = psNode;
	do
	{
		/*
			Stop once we found an optional link between two nodes.
		*/
		if (psGroupNode->bOptional)
		{
			break;
		}

		psGroupNode = psGroupNode->psNext;
	} while (psGroupNode != NULL);
	
	/*
		If we found an optional link then remove it.
	*/
	if (psGroupNode != NULL)
	{
		DropLinkAfterNode(psRAData->psState, psGroupNode);
		bDropped = IMG_TRUE;
	}

	/*
		Step backwards in the group.
	*/
	psGroupNode = psNode->psPrev;
	while (psGroupNode != NULL)
	{
		/*
			Stop once we found an optional link.
		*/
		if (psGroupNode->bOptional)
		{
			break;
		}

		psGroupNode = psGroupNode->psPrev;
	}

	/*
		Drop the optional link.
	*/
	if (psGroupNode != NULL)
	{
		DropLinkAfterNode(psRAData->psState, psGroupNode);
		bDropped = IMG_TRUE;
	}

	return bDropped;
}

static IMG_BOOL ColourNodeGroup(PRAGCOL_STATE		psRegState, 
								PCOLOUR_STATUS		psStatus,
								IMG_UINT32			uNode,
								PREGISTER_GROUP		psNodeGroup, 
								IMG_UINT32			uColourIdx,
								IMG_BOOL			bIncludeReservations, 
								PCOLOUR_RANGE_LIST	psColourRangeList)
/*****************************************************************************
 FUNCTION	: ColourNodeGroup

 PURPOSE	: Colour a group of registers which have to be given consecutive hardware register
			  numbers.

 PARAMETERS	: psRegState		- Module state.
			  psStatus			- Temporary storage for the set of available
								colours.
			  uNode				- Intermediate register which is part of the group.
			  psNodeGroup		- Information aboutt he group.
			  uColourIdx		- Index of the colour to set.
			  bIncludeReservations
								- TRUE if honour colour reservations when
								allocating.
			  psColourRangeList	- Range of colours to consider.

 RETURNS	: TRUE if the node was coloured.
*****************************************************************************/
{
	COLOUR				sBaseColour;
	PREGISTER_GROUP		psBaseGroup;
	IMG_UINT32			uBaseNode;
	IMG_UINT32			uLength;
	
	#if defined(DEBUG)
	PREGISTER_GROUP		psGroup;
	#endif
	PINTERMEDIATE_STATE	psState = psRegState->sRAData.psState;

	/*
		Get details of the group this node belongs to.
	*/
	GetGroupStartAndSize(psRegState,
						 psNodeGroup,
						 &uBaseNode,
						 &psBaseGroup,
						 &uLength);

	#if defined(DEBUG)
	/* Shouldn't have already coloured any nodes in the group. */
	for (psGroup = psBaseGroup; psGroup != NULL; psGroup = psGroup->psNext)
	{
		IMG_UINT32	uGroupNode;

		uGroupNode = RegisterToNode(&psRegState->sRAData, USEASM_REGTYPE_TEMP, psGroup->uRegister);
		ASSERT(psRegState->asNodes[uGroupNode].asColours[uColourIdx].eType == COLOUR_TYPE_UNDEF);
	}
	#endif /* defined(DEBUG) */

	/*
		Check if there is at least one available range of colours.
	*/
	if (GetColourForGroup(psState,
						  psRegState,
					      psBaseGroup,
						  uLength,
						  psStatus,
						  bIncludeReservations,
						  psColourRangeList,
						  &sBaseColour))
	{
		/*
			Mark all of the nodes in the group as having been assigned a colour.
		*/
		SetGroupColour(psState, psRegState, psBaseGroup, uNode, uColourIdx, &sBaseColour, bIncludeReservations);
		return IMG_TRUE;
	}

	/*
	   Got here, so none of the runs were large enough for the whole group.
	*/
	/*
	   If node is in the mandatory group try disconnecting the group.
	*/
	if (DisconnectOptionalGroupParts(&psRegState->sRAData, uNode))
	{
		/* Go around again */
		return ColourNodeGroup(psRegState, 
							   psStatus, 
							   uNode, 
							   psNodeGroup, 
							   uColourIdx, 
							   bIncludeReservations, 
							   psColourRangeList);
	}
	else
	{
		/* There is no optional group. */
		return IMG_FALSE;
	}
}

static IMG_BOOL ColourNode(PRAGCOL_STATE psRegState,
						   PCOLOUR_STATUS psStatus,
						   IMG_UINT32 uNode,
						   IMG_UINT32 uColourIdx,
						   IMG_BOOL bHonour,
						   PCOLOUR_RANGE_LIST psColourRanges)
/*****************************************************************************
 FUNCTION	: ColourNode

 PURPOSE	: Try to colour a node.

 PARAMETERS	: psRegState		- State of the registers
			  psStatus			- Temporary storage used for the set of available
								colours.
			  uNode				- Node to colour.
			  uColourIdx		- Index of the colour to set.
			  bHonour			- Whether to honour colour reservations.
			  psColourRanges	- Ranges of colours to consider.

 RETURNS	: IMG_TRUE if the node could be coloured; IMG_FALSE otherwise.
*****************************************************************************/
{
	#if defined(DEBUG)
	PINTERMEDIATE_STATE psState = psRegState->sRAData.psState;
	#endif
	
	PREGISTER_GROUP		psNodeGroup = GetRegisterGroup(&psRegState->sRAData, uNode);
	COLOUR				sNodeColour;

	/*
	   Description: Allocate a colour to uNode. A colour is found by testing
	   each colour i against every register j, to see whether uNode and j are
	   live at the same time and register j is coloured i.

	   Colour allocation is prioritised: colours are allocated for
	   individually specified nodes (argument uNode) and for members of
	   mandatory groups, members of optional groups have lower priority and
	   can only reserve colours. Once colours have been reserved for an
	   optional group, the group is dissolved to its individual nodes.

	   Individual nodes are coloured (by calling ColourNode) in an order
	   determined by the nodes degree. When any individual node is coloured,
	   it is first tested to see whether a colour has been reserved for it and
	   if the colour is still available. If so, that colour is allocated to
	   the node otherwise the interference graph is searched for a colour.

	   A mandatory group is coloured when any member is coloured. When a
	   mandatory group is coloured, every member of the group must be
	   coloured (with consecutive colours) for colouring to succeed.

	   When searching for colours, the flag bHonour determines whether colour
	   reservations are honoured. If bHonour is IMG_FALSE, reserverations are
	   ignored and a node may be allocated a colour which had been reserved.

	   Function ColourNode should be run twice, the first time with bHonour ==
	   IMG_TRUE then, if that fails, with bHonour == IMG_FALSE.

	   Data strutures: The n'th flag in psRegstate->auReservedColour indicates
	   whether colour n has been reserved. Field
	   psRegState->auRegGroup[i].uColour stores the colour reserved for node i
	   or ((IMG_UINT32) -1) if no colour is reserved.

	   If no colours can be found for uNode and any mandatory group it is part
	   of, then colouring fails.
	*/

	/* This node may have already been coloured if it was part of a group */
	if (psRegState->asNodes[uNode].asColours[uColourIdx].eType != COLOUR_TYPE_UNDEF)
	{
		#if defined(DEBUG)
		ASSERT(ValidColourForRange(psColourRanges, &psRegState->asNodes[uNode].asColours[uColourIdx]));
		#endif /* defined(DEBUG) */
		return IMG_TRUE;
	}

	/* Test whether a colour has been reserved for this node */
	sNodeColour = psRegState->asNodes[uNode].sReservedColour;

	if (sNodeColour.eType != COLOUR_TYPE_UNDEF)
	{
		#if defined(DEBUG)
		IMG_BOOL	bOverWrite;

		ASSERT(ValidColourForRange(psColourRanges, &sNodeColour));
		ASSERT(CanNodeUseColour(psRegState, uNode, &sNodeColour, bHonour, &bOverWrite));
		#endif /* defined(DEBUG) */

		/* Available, so take it */
		SetNodeColour(psRegState, uNode, uColourIdx, &sNodeColour);
		ClearConflictingReservations(psRegState, uNode, &sNodeColour);
		return IMG_TRUE;
	}

	if (psNodeGroup != NULL)
	{
		/*
			Colour an entire group of registers with consecutive colours.
		*/
		return ColourNodeGroup(psRegState, psStatus, uNode, psNodeGroup, uColourIdx, bHonour, psColourRanges);
	}
	else
	{
		/* uNode was not in a group */
		return ColourSingleNode(psRegState, psStatus, uNode, uColourIdx, bHonour, psColourRanges);
	}
}

static
PREGISTER_GROUP GetFirstMandatoryNodeInGroup(PREGISTER_GROUP psGroup)
/*****************************************************************************
 FUNCTION	: GetFirstMandatoryNodeInGroup

 PURPOSE	: Given a member of a group of registers requiring consecutive register
			  numbers return the member requiring the lowest register number.

 PARAMETERS	: psGroup			- Member of the group.

 RETURNS	: The first member of the group.
*****************************************************************************/
{
	while (psGroup->psPrev != NULL && !psGroup->psPrev->bOptional)
	{
		psGroup = psGroup->psPrev;
	}
	return psGroup;
}

static
IMG_UINT32 FindBestSpillNode(PINTERMEDIATE_STATE psState,
							 PRAGCOL_STATE psRegState,
							 IMG_UINT32 uSpillNode)
/*****************************************************************************
 FUNCTION	: ChooseBestSpillNode

 PURPOSE	: Find a better node to spill than the given node.

 PARAMETERS	: psState			- Compiler state.
			  psRegState		- Register allocation state.
			  uNrSpillNodes		- Number of nodes currently in the spill list.
              uSpillNode        - Current spill node

 RETURNS	: The node to spill
*****************************************************************************/
{
	IMG_UINT32 uNode;

	/*
	  Run through nodes that interfere with current node,
	  pick the least accesesed node as the node to spill.
	*/
	for (uNode = psRegState->sRAData.uTempStart;
		 uNode < psRegState->sRAData.uNrRegisters;
		 uNode++)
	{
		IMG_UINT32		uNodeType, uNodeNum;
		PREGISTER_GROUP	psNodeGroup;

		if (uNode == uSpillNode)
		{
			continue;
		}

		/* Make sure that node is used and is less used than current best choice */
		if (!GetBit(psRegState->asNodes[uNode].auFlags, NODE_FLAG_SPILLABLE))
		{
			continue;
		}

		/* Make sure the node isn't precoloured. */
		if (IsPrecolouredNode(GetRegisterGroup(&psRegState->sRAData, uNode)))
		{
			continue;
		}

		/*
		  Check this isn't the same as something we already tried to
		  spill.
		*/
		if (GetBit(psRegState->asNodes[uNode].auFlags, NODE_FLAG_SPILL))
		{
			continue;
		}

		/* Convert to a register type/register number. */
		NodeToRegister(&psRegState->sRAData, uNode, &uNodeType, &uNodeNum);
		ASSERT(uNodeType == USEASM_REGTYPE_TEMP);

		/* Make sure node isn't already used in spills */
		if (VectorGet(psState, &psRegState->sNodesUsedInSpills, uNodeNum))
		{
			continue;
		}

		/* Make sure node interferes with spill node */
		if (!IntfGraphGet(psRegState->psIntfGraph, uNode, uSpillNode))
		{
			continue;
		}

		/* Found a better node then add it to list and return. */

		/*
			If the better node is part of a group of registers requiring consecutive
			hardware register numbers then spill the entire group.
		*/
		psNodeGroup = GetRegisterGroup(&psRegState->sRAData, uNode);
		if (psNodeGroup != NULL)
		{
			psNodeGroup = GetFirstMandatoryNodeInGroup(psNodeGroup);
			return RegisterToNode(&psRegState->sRAData, USEASM_REGTYPE_TEMP, psNodeGroup->uRegister);
		}
		else
		{
			return uNode;
		}
	}

	/* We didn't find anything better than the original node. */
	return uSpillNode;
}

static
IMG_BOOL AddSecAttrWithMemoryOffset(PINTERMEDIATE_STATE	psState,
									PSPILL_STATE		psSpillState)
/*****************************************************************************
 FUNCTION     : AddSecAttrWithMemoryOffset

 PURPOSE      : Try to reserve a secondary attribute to hold the offset in
				memory of a spill area location.

 PARAMETERS	  : psState			- Compiler state.
                psSpillState	- Information about the spill area.

 RETURNS	  : TRUE if a secondary attribute was available to hold the
			    memory offset.
*****************************************************************************/
{
	PINREGISTER_CONST	psSecAttr;

	/*
		Check for running out of secondary attributes.
	*/
	if (psState->sSAProg.uConstSecAttrCount >= psState->sSAProg.uInRegisterConstantLimit)
	{
		return IMG_FALSE;
	}

	/*
		Add an undefined entry - we'll convert it to an entry asking the driver to load
		a static value once we know the final size of the spill area required.
	*/
	AddSpillAreaOffsetPlaceholder(psState, &psSecAttr);

	/*
		Store the offset of the secondary attribute used.
	*/
	ResizeArray(psState,
				psSpillState->asSpillAreaOffsetsSANums,
				(psSpillState->uSpillAreaOffsetsSACount + 0) * sizeof(psSpillState->asSpillAreaOffsetsSANums[0]),
				(psSpillState->uSpillAreaOffsetsSACount + 1) * sizeof(psSpillState->asSpillAreaOffsetsSANums[0]),
				(IMG_PVOID*)&psSpillState->asSpillAreaOffsetsSANums);
	psSpillState->asSpillAreaOffsetsSANums[psSpillState->uSpillAreaOffsetsSACount].psConst = psSecAttr;
	psSpillState->uSpillAreaOffsetsSACount++;	

	return IMG_TRUE;
}

static
IMG_UINT32 AllocateSpillSpace(PINTERMEDIATE_STATE psState, PRAGCOL_STATE psRegState, IMG_UINT32 uTempRegNum)
/*****************************************************************************
 FUNCTION     : AllocateSpillSpace

 PURPOSE      : Allocate space in the spill area to save the contents of a
				temporary register.

 PARAMETERS	  : psState			- Compiler state.
                psRegState		- Register allocator state.
				uTempRegNum		- Temporary register to allocate space for.

 RETURNS	  : The address (in dwords) of the allocated space.
*****************************************************************************/
{
	IMG_UINT32	uNode;
	IMG_UINT32	uSpillAddress;

	uNode = RegisterToNode(&psRegState->sRAData, USEASM_REGTYPE_TEMP, uTempRegNum);

	/*
		Get some space in the spill memory area for the node.
	*/
	if (psRegState->asNodes[uNode].uAddress > psState->uSpillAreaSize)
	{
		PSPILL_STATE	psSpillState = psRegState->psSpillState;

		if (psState->uSpillAreaSize == 0)
		{
			/*
				Add instructions to the secondary update program to adjust
				the base address for the spill area depending on the pipe
				number.
			*/
			ASSERT(psSpillState->psSpillAreaSizeLoader == NULL);
			if (psState->uCompilerFlags & UF_EXTRACTCONSTANTCALCS)
			{
				psSpillState->psSpillAreaSizeLoader = 
					AdjustBaseAddress(psState,
									  psState->psSAOffsets->uScratchBase);
			}
		}

		/*
			Allocate the next location in the spill area to use for this variable.
		*/
		uSpillAddress = psState->uSpillAreaSize ++;
		psRegState->asNodes[uNode].uAddress = uSpillAddress;

		/*
			Is the spill area too large to be able to use immediate offsets in the memory
			access instructions.
		*/
		if (psSpillState->bSpillUseImmediateOffsets && psState->uSpillAreaSize > EURASIA_USE_LDST_MAX_IMM_LOCAL_STRIDE)
		{
			PUSC_LIST_ENTRY	psListEntry;
			IMG_UINT32		uSpillOffset;

			/*
				Don't use immediate offsets in future.
			*/
			psSpillState->bSpillUseImmediateOffsets = IMG_FALSE;

			/*
				If secondary attributes are available then use them to hold the memory offsets
				for the LD/ST instruction we have already inserted.
			*/
			ASSERT(psSpillState->uSpillAreaOffsetsSACount == 0);
			for (uSpillOffset = 0; uSpillOffset < psState->uSpillAreaSize; uSpillOffset++)
			{
				if (!AddSecAttrWithMemoryOffset(psState, psSpillState))
				{
					break;
				}
			}

			/*
				Update the LD or ST instructions we have already inserted.
			*/
			for (psListEntry = psSpillState->sSpillInstList.psHead;
				 psListEntry != NULL;
				 psListEntry = psListEntry->psNext)
			{
				PINST		psInst = IMG_CONTAINING_RECORD(psListEntry, PINST, sAvailableListEntry);
				IMG_UINT32	uOffset;
				PARG		psOffsetArg;

				ASSERT(psInst->eOpcode == ISPILLREAD || psInst->eOpcode == ISPILLWRITE);
				ASSERT(GetBit(psInst->auFlag, INST_SPILL));

				/*
					Get the offset in the spill area accessed by this instruction.
				*/
				psOffsetArg = &psInst->asArg[MEMSPILL_OFFSET_ARGINDEX];
				ASSERT(psOffsetArg->uType == USEASM_REGTYPE_IMMEDIATE);
				uOffset = psOffsetArg->uNumber;
				ASSERT(uOffset < (psState->uSpillAreaSize - 1));

				/*
					Replace the immediate offset either by a secondary attribute or by a
					temporary register loaded using a LIMM instruction.
				*/
				SetSpillAddress(psState, psRegState, psInst, MEMSPILL_OFFSET_ARGINDEX, uOffset);
			}
		}

		/*
			If a secondary attribute is available use it to hold the memory offset for the LD or
			ST instructions we are going to insert.
		*/
		if (!psSpillState->bSpillUseImmediateOffsets)
		{
			AddSecAttrWithMemoryOffset(psState, psSpillState);
		}
	}
	else
	{
		uSpillAddress = psRegState->asNodes[uNode].uAddress;
	}

	return uSpillAddress;
}

static
IMG_VOID SpillReferencesInInst(PINTERMEDIATE_STATE	psState, 
							   PRAGCOL_STATE		psRegState,
							   PUSEDEF_CHAIN		psSpillTemp, 
							   PUSC_LIST_ENTRY*		ppsListEntry, 
							   PINST				psInst,
							   PINST				psAfterInst,
							   IMG_PUINT32			puModifiedDestMask,
							   IMG_PUINT32			puModifiedSrcMask,
							   PINST*				ppsLoadInst,
							   PINST*				ppsStoreInst)
/*****************************************************************************
 FUNCTION     : SpillReferencesInInst

 PURPOSE      : Replace all references to a temporary register in an instruction
				by new temporary registers which are read or written to memory
				immediately before or after the instruction.

 PARAMETERS	  : psState			- Compiler state.
                psRegState		- Register allocator state.
				psSpillTemp		- Temporary register to spill.
				ppsListEntry	- On input: Points to the start of the references
								  to the temporary register in the instruction (if any).
								  On output: Points to the first reference in a
								  different instruction.
			    psInst			- Instruction to spill for.
				puModifiedDestMask
								- Updated with a mask of the instruction destinations
								which have been replaced.
				puModifiedSrcMask
								- Updated with a mask of the instruction sources which
								have been replaced.
			    psAfterInst		- Instruction to insert store instructions before.
				ppsLoadInst		- If a memory load instruction was inserted then
								returns a pointer to it.
				ppsStoreInst	- If a memory store instruction was inserted then
								returns a pointer to it.

 RETURNS	  : Nothing.
*****************************************************************************/
{
	ARG				sLoadTemp;
	PINST			psLoadInst;
	PINST			psStoreInst;
	PUSC_LIST_ENTRY	psListEntry;

	psListEntry = (*ppsListEntry);
	InitInstArg(&sLoadTemp);
	sLoadTemp.uNumber = USC_UNDEF;
	psLoadInst = psStoreInst = NULL;
	while (psListEntry != NULL)
	{
		PUSEDEF				psGroupUseDef;
		USEDEF_TYPE			eType;
		IMG_UINT32			uArgIdx;
		IMG_BOOL			bLoad;
		PARG				psArg;
		UF_REGFORMAT		eSpillFmt;
		IMG_UINT32			uLiveChansInDest;
		PINST				psInsertBeforeInst;
		PINST*				ppsLoadOrStoreInst;
		IMG_UINT32			uSpillAddress;
		ARG					sStoreTemp;
		PARG				psReplacementTemp;

		psGroupUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

		/*
			Stop once we've processed all the references to this register in the current
			instruction.
		*/
		if (UseDefGetInst(psGroupUseDef) != psInst)
		{
			break;
		}

		psListEntry = psListEntry->psNext;

		/*
			Save the details of the reference.
		*/
		eType = psGroupUseDef->eType;
		uArgIdx = psGroupUseDef->uLocation;

		/*
			Update the masks of sources and destinations which have been modified.
		*/
		if (eType == DEF_TYPE_INST)
		{
			(*puModifiedDestMask) |= (1U << uArgIdx);
		}
		else if (eType == USE_TYPE_SRC)
		{
			(*puModifiedSrcMask) |= (1U << uArgIdx);
		}

		psArg = UseDefGetInstUseLocation(psState, psGroupUseDef, NULL /* ppsIndexUse */);
		eSpillFmt = psArg->eFmt;

		psReplacementTemp = &sLoadTemp;
		if (eType == DEF_TYPE_INST && psInst->apsOldDest[uArgIdx] == NULL)
		{
			/*
				If the destination is completely overwritten then use a different replacement temporary register
				to avoid unnecessary constraints on the register allocator.
			*/
			InitInstArg(&sStoreTemp);
			sStoreTemp.uNumber = USC_UNDEF;

			psReplacementTemp = &sStoreTemp;
		}

		if (psReplacementTemp->uNumber == USC_UNDEF)
		{
			/*
				Allocate a new temporary register which will be used only in the instruction and either loaded from memory
				immediately before or stored to memory immediately after.
			*/
			MakeNewTempArg(psState, eSpillFmt, psReplacementTemp);

			/*
				Mark the new temporary so we don't try to spill it in future.
			*/
			VectorSetRange(psState, &psRegState->sNodesUsedInSpills, psReplacementTemp->uNumber, psReplacementTemp->uNumber, 1);
		}

		/*
			Replace the source or destination by the new temporary.
		*/
		UseDefSubstUse(psState, psSpillTemp, psGroupUseDef, psReplacementTemp);
		psGroupUseDef = NULL;

		/*
			Don't load or store the temporary if it isn't really used.
		*/
		if (eType == DEF_TYPE_INST || eType == USE_TYPE_OLDDEST)
		{
			if (psInst->auDestMask[uArgIdx] == 0)
			{
				continue;
			}
		}
		else
		{
			ASSERT(eType == USE_TYPE_SRC);
			if (GetLiveChansInArg(psState, psInst, uArgIdx) == 0)
			{
				continue;
			}
		}

		if (eType == DEF_TYPE_INST)
		{
			ASSERT(psStoreInst == NULL);

			bLoad = IMG_FALSE;
			uLiveChansInDest = psInst->auLiveChansInDest[uArgIdx];
			psInsertBeforeInst = psAfterInst;
			ppsLoadOrStoreInst = &psStoreInst;
		}
		else
		{
			/*
				Skip if we already loadedd the temporary for this instruction.
			*/
			if (psLoadInst != NULL)
			{
				continue;
			}

			ASSERT(eType == USE_TYPE_SRC || eType == USE_TYPE_OLDDEST);
			bLoad = IMG_TRUE;
			uLiveChansInDest = USC_UNDEF;
			psInsertBeforeInst = psInst;
			ppsLoadOrStoreInst = &psLoadInst;
		}

		/*
			Allocate space in the spill area to hold the contents of the spilled temporary.
		*/
		uSpillAddress = AllocateSpillSpace(psState, psRegState, psSpillTemp->uNumber);

		/*
			Either load the contents of the spilled temporary from memory or store it to
			memory.
		*/
		InsertSpill(psState,
					psRegState,
					psInst->psBlock,
					psInst,
					psInsertBeforeInst,
					psReplacementTemp->uNumber,
					bLoad,
					eSpillFmt,
					uSpillAddress,
					uLiveChansInDest,
					ppsLoadOrStoreInst);
	}

	(*ppsLoadInst) = psLoadInst;
	(*ppsStoreInst) = psStoreInst;
	(*ppsListEntry) = psListEntry;
}

static
PINST GetEarliestInst(PINST psInst1, PINST psInst2)
/*****************************************************************************
 FUNCTION     : GetEarliestInst

 PURPOSE      : 

 PARAMETERS	  : psInst1, psInst2		- Instructions to compare.

 RETURNS	  : The earliest instruction.
*****************************************************************************/
{
	if (psInst1 == NULL)
	{
		return psInst2;
	}
	if (psInst2 == NULL)
	{
		return psInst1;
	}

	if (psInst1->psBlock == psInst2->psBlock)
	{
		if (psInst1->uBlockIndex < psInst2->uBlockIndex)
		{
			return psInst1;
		}
		else
		{
			return psInst2;
		}
	}
	else
	{
		if (psInst1->psBlock->uGlobalIdx < psInst2->psBlock->uGlobalIdx)
		{
			return psInst1;
		}
		else
		{
			return psInst2;
		}
	}
}

typedef struct _REGISTER_TO_SPILL
{
	PUSEDEF_CHAIN	psUseDefChain;
	PUSC_LIST_ENTRY	psListEntry;
} REGISTER_TO_SPILL, *PREGISTER_TO_SPILL;

static
IMG_VOID SpillRegisterGroup(PINTERMEDIATE_STATE	psState,
							PRAGCOL_STATE		psRegState,
							PREGISTER_GROUP		psGroupHead, 
							IMG_UINT32			uGroupHeadTempNum)
/*****************************************************************************
 FUNCTION     : SpillRegisterGroup

 PURPOSE      : Spill an entire group of registers requiring consecutive
			    hardware register numbers.

 PARAMETERS	  : psState			- Compiler state.
				psRegState		- Register allocator state.
				psGroupHead		- First register to spill (might be NULL if
								the register to spill has no restrictions on
								the ordering of the assigned hardware register
								number).
				uGroupHeadTempNum
								- Temporary register number to spill.

 RETURNS	  : None.
*****************************************************************************/
{
	IMG_UINT32			uGroupLength;
	PREGISTER_GROUP		psGroup;
	PREGISTER_TO_SPILL	asRegistersToSpill;
	REGISTER_TO_SPILL	sOneRegisterToSpill;
	IMG_UINT32			uGroupIdx;

	/*
		Get the length of the group of registers to spill.
	*/
	uGroupLength = 0;
	if (psGroupHead != NULL)
	{
		for (psGroup = psGroupHead; psGroup != NULL; psGroup = psGroup->psNext)
		{
			uGroupLength++;
		}
	}
	else
	{
		uGroupLength = 1;
	}

	/*
		Allocate space to store the next reference to spill for each register.
	*/
	if (uGroupLength > 1)
	{
		asRegistersToSpill = UscAlloc(psState, sizeof(asRegistersToSpill[0]) * uGroupLength);
	}
	else
	{
		asRegistersToSpill = &sOneRegisterToSpill;
	}

	/*
		Initialize information about each register to spill.
	*/
	psGroup = psGroupHead;
	for (uGroupIdx = 0; uGroupIdx < uGroupLength; uGroupIdx++)
	{
		IMG_UINT32			uTempNum;
		PREGISTER_TO_SPILL	psSpill = &asRegistersToSpill[uGroupIdx];

		if (uGroupIdx == 0)
		{
			uTempNum = uGroupHeadTempNum;
			ASSERT(psGroupHead == NULL || psGroupHead->uRegister == uTempNum);
		}
		else
		{
			uTempNum = psGroup->uRegister;
		}

		ASSERT(!VectorGet(psRegState->sRAData.psState, &psRegState->sNodesUsedInSpills, uTempNum));

		psSpill->psUseDefChain = UseDefGet(psState, USEASM_REGTYPE_TEMP, uTempNum);
		psSpill->psListEntry = psSpill->psUseDefChain->sList.psHead;

		if (psGroup != NULL)
		{
			psGroup = psGroup->psNext;
		}
	}

	for (;;)
	{
		PINST		psCurrentInst;
		PINST		psFirstLoadInst, psLastLoadInst;
		PINST		psFirstStoreInst, psLastStoreInst;
		PINST		psInstAfterCurrent;
		IMG_UINT32	uModifiedDestMask;
		IMG_UINT32	uModifiedSrcMask;
		IMG_UINT32	uType;

		/*
			Find the earliest (in the sorting order used by lists of uses/defines)
			instruction with a reference to one of the registers to spill.
		*/
		psCurrentInst = NULL;
		for (uGroupIdx = 0; uGroupIdx < uGroupLength; uGroupIdx++)
		{
			PINST				psUseInst;
			PREGISTER_TO_SPILL	psSpill = &asRegistersToSpill[uGroupIdx];
			PUSEDEF				psGroupUseDef;

			if (psSpill->psListEntry == NULL)
			{
				continue;
			}

			psGroupUseDef = IMG_CONTAINING_RECORD(psSpill->psListEntry, PUSEDEF, sListEntry);

			psUseInst = UseDefGetInst(psGroupUseDef);
			ASSERT(psUseInst != NULL);

			psCurrentInst = GetEarliestInst(psUseInst, psCurrentInst);
		}

		/*
			Check for no more references to any of the registers in the group.
		*/
		if (psCurrentInst == NULL)
		{
			break;
		}

		/*
			Spill all references to any of the registers in the current
			instruction.
		*/
		psFirstLoadInst = psLastLoadInst = NULL;
		psFirstStoreInst = psLastStoreInst = NULL;
		psInstAfterCurrent = psCurrentInst->psNext;
		uModifiedDestMask = 0;
		uModifiedSrcMask = 0;
		for (uGroupIdx = 0; uGroupIdx < uGroupLength; uGroupIdx++)
		{
			PINST				psLoadInst;
			PINST				psStoreInst;
			PREGISTER_TO_SPILL	psSpill = &asRegistersToSpill[uGroupIdx];

			SpillReferencesInInst(psState, 
								  psRegState,
								  psSpill->psUseDefChain, 
								  &psSpill->psListEntry, 
								  psCurrentInst, 
								  psInstAfterCurrent,
								  &uModifiedDestMask,
								  &uModifiedSrcMask,
								  &psLoadInst,
								  &psStoreInst);
			
			if (psLoadInst != NULL)
			{
				if (psFirstLoadInst == NULL)
				{
					psFirstLoadInst = psLoadInst;
				}
				psLastLoadInst = psLoadInst;
			}

			if (psStoreInst != NULL)
			{
				if (psFirstStoreInst == NULL)
				{
					psFirstStoreInst = psStoreInst;
				}
				psLastStoreInst = psStoreInst;
			}
		}

		/*
			For any sources or destinations which have been modified give the replacement
			temporary registers the same restrictions on their hardware register numbers as
			the original registers.
		*/
		for (uType = 0; uType < 2; uType++)
		{
			REGISTER_GROUPS_DESC	sGroups;
			IMG_UINT32				uModifiedArgMask;
			PARG					asArg;
			IMG_UINT32				uGroup;

			if (uType == 0)
			{
				GetDestRegisterGroups(psState, psCurrentInst, &sGroups);
				uModifiedArgMask = uModifiedDestMask;
				asArg = psCurrentInst->asDest;
			}
			else
			{		
				GetSourceRegisterGroups(psState, psCurrentInst, &sGroups);
				uModifiedArgMask = uModifiedSrcMask;
				asArg = psCurrentInst->asArg;
			}	

			for (uGroup = 0; uGroup < sGroups.uCount; uGroup++)
			{
				IMG_UINT32				uGroupArgMask;
				PREGISTER_GROUP_DESC	psGroupDesc = &sGroups.asGroups[uGroup];

				uGroupArgMask = ((1U << psGroupDesc->uCount) - 1U) << psGroupDesc->uStart;
				if ((uModifiedArgMask & uGroupArgMask) != 0)
				{
					REGISTER_GROUP_DESC		sSwizzledGroup;

					ASSERT((uModifiedArgMask & uGroupArgMask) == uGroupArgMask);

					#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
					if (uType == 1 && (g_psInstDesc[psCurrentInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) != 0)
					{
						/*
							Check if we could remove some of the restrictions by modifying the instruction's
							swizzle.
						*/
						AdjustRegisterGroupForSwizzle(psState, psCurrentInst, uGroup, &sSwizzledGroup, psGroupDesc);
					}
					else
					#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
					{
						sSwizzledGroup = *psGroupDesc;
					}

					MakeGroup(psState, asArg + sSwizzledGroup.uStart, sSwizzledGroup.uCount, sSwizzledGroup.eAlign);
				}
			}
		}

		/*
			Save the internal registers before the inserted memory load/stores and
			restore them afterwards.
		*/
		if (psFirstLoadInst != NULL)
		{
			SpillInternalRegisters(psState, psRegState, psCurrentInst->psBlock, psFirstLoadInst, psLastLoadInst);
		}
		if (psFirstStoreInst != NULL)
		{
			SpillInternalRegisters(psState, psRegState, psCurrentInst->psBlock, psFirstStoreInst, psLastStoreInst);
		}
	}

	if (uGroupLength > 1)
	{
		UscFree(psState, asRegistersToSpill);
		asRegistersToSpill = NULL;
	}
}

static
IMG_VOID SpillRegistersBP(PINTERMEDIATE_STATE psState,
						  PCODEBLOCK psBlock,
						  IMG_PVOID pvRegState)
/*****************************************************************************
 FUNCTION     : SpillRegistersBP

 PURPOSE      : Insert load/save instructions to spill registers through a program

 PARAMETERS	  : psState			- Compiler state.
                psBlock         - Code block to process
                pvRegState      - PREGISTER_STATE of Register allocator state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PRAGCOL_STATE psRegState = (PRAGCOL_STATE)pvRegState;
	
	/*
	  For each spilled node clear the corresponding entry in the set of registers
	  live out from the block.
	*/
	{
		PUSC_LIST_ENTRY	psListEntry;

		for (psListEntry = psRegState->sSpillList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
		{
			PNODE_DATA	psNode = IMG_CONTAINING_RECORD(psListEntry, PNODE_DATA, sSpillListEntry);
			IMG_UINT32	uNode = (IMG_UINT32)(psNode - psRegState->asNodes);
			IMG_UINT32 uRegType;
			IMG_UINT32 uRegNum;

			NodeToRegister(&psRegState->sRAData, uNode, &uRegType, &uRegNum);

			/* We only spill nodes representing temporaries. */
			ASSERT(uRegType == USEASM_REGTYPE_TEMP);

			SetRegisterLiveMask(psState,
								&psBlock->sRegistersLiveOut,
								USEASM_REGTYPE_TEMP,
								uRegNum,
								0,
								0);
		}
    }
}

static
IMG_VOID AppendToSpillList(PINTERMEDIATE_STATE	psState,
						   PRAGCOL_STATE		psRegState,
						   IMG_UINT32			uSpillNode)
/*****************************************************************************
 FUNCTION	  : AppendToSpillList

 PURPOSE	  : Add a register to the list of registers to spill.

 PARAMETERS	  : psState			- Compiler state.
                psRegState      - Register allocator state
				uSpillNode		- Register to add.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PNODE_DATA	psSpillNode;

	psSpillNode = &psRegState->asNodes[uSpillNode];

	if (GetBit(psSpillNode->auFlags, NODE_FLAG_UNCOLOURED))
	{
		RemoveFromList(&psRegState->sUncolouredList, &psSpillNode->sSpillListEntry);
		SetBit(psSpillNode->auFlags, NODE_FLAG_UNCOLOURED, IMG_FALSE);
	}

	AppendToList(&psRegState->sSpillList, &psSpillNode->sSpillListEntry);
	ASSERT(!GetBit(psSpillNode->auFlags, NODE_FLAG_SPILL));
	SetBit(psSpillNode->auFlags, NODE_FLAG_SPILL, IMG_TRUE);
}

static
IMG_VOID SpillEntireGroup(PINTERMEDIATE_STATE	psState,
						  PRAGCOL_STATE			psRegState,
						  IMG_UINT32			uBaseNode)
/*****************************************************************************
 FUNCTION	  : SpillEntireGroup

 PURPOSE	  : Add an entire group of registers requiring consecutive hardware
				register numbers to the list of registers to spill.

 PARAMETERS	  : psState			- Compiler state.
                psRegState      - Register allocator state
				uBaseNode		- First register in the group. 

 RETURNS	: Nothing.
*****************************************************************************/
{
	PREGISTER_GROUP	psBaseNode = GetRegisterGroup(&psRegState->sRAData, uBaseNode);
	PREGISTER_GROUP	psNode;
	IMG_UINT32		uBaseRegType;
	IMG_UINT32		uBaseRegNum;

	NodeToRegister(&psRegState->sRAData, uBaseNode, &uBaseRegType, &uBaseRegNum);
	ASSERT(uBaseRegType == USEASM_REGTYPE_TEMP);

	SpillRegisterGroup(psState, psRegState, psBaseNode, uBaseRegNum);

	if (psBaseNode == NULL)
	{
		AppendToSpillList(psState, psRegState, uBaseNode);
		return;
	}

	ASSERT(!(psBaseNode->psPrev != NULL && !psBaseNode->psPrev->bOptional));

	psNode = psBaseNode;
	for (;;)
	{
		IMG_UINT32	uNode;

		uNode = RegisterToNode(&psRegState->sRAData, USEASM_REGTYPE_TEMP, psNode->uRegister);
		AppendToSpillList(psState, psRegState, uNode);

		if (psNode->psNext == NULL || psNode->bOptional)
		{
			break;
		}
		psNode = psNode->psNext;
	}
}

static
IMG_BOOL IsGroupSpillable(PRAGCOL_STATE psRegState, IMG_UINT32 uBaseNode)
/*****************************************************************************
 FUNCTION	  : IsGroupSpillable

 PURPOSE	  : Check if any of the registers in a group of registers which require
				consecutive hardware register numbers are spillable.

 PARAMETERS	  : psRegState      - Register allocator state
				uBaseNode		- First register in the group.

 RETURNS	: TRUE if any of the registers are spillable.
*****************************************************************************/
{
	PREGISTER_GROUP	psBaseNode = GetRegisterGroup(&psRegState->sRAData, uBaseNode);
	PREGISTER_GROUP	psGroup;

	if (psBaseNode == NULL)
	{
		return (GetBit(psRegState->asNodes[uBaseNode].auFlags, NODE_FLAG_SPILLABLE)) ? IMG_TRUE : IMG_FALSE;
	}

	for (psGroup = psBaseNode; psGroup != NULL; psGroup = psGroup->psNext)
	{
		IMG_UINT32	uGroupNode = RegisterToNode(&psRegState->sRAData, USEASM_REGTYPE_TEMP, psGroup->uRegister);
		
		if (GetBit(psRegState->asNodes[uGroupNode].auFlags, NODE_FLAG_SPILLABLE))
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static
IMG_VOID SpillPlainNodes(PINTERMEDIATE_STATE psState,
						 PRAGCOL_STATE psRegState)
/*****************************************************************************
 FUNCTION	  : SpillPlainNodes

 PURPOSE	  : Spill the nodes that can't be coloured.

 PARAMETERS	  : psState			- Compiler state.
                psRegState      - Register allocator state

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;

   /* Choose spill nodes */
	InitializeList(&psRegState->sSpillList);
	while ((psListEntry = RemoveListHead(&psRegState->sUncolouredList)) != NULL)
	{
		PNODE_DATA	psNode = IMG_CONTAINING_RECORD(psListEntry, PNODE_DATA, sSpillListEntry);
		IMG_UINT32	uNode = (IMG_UINT32)(psNode - psRegState->asNodes);
		IMG_BOOL	bUse;
		IMG_UINT32	uNodeRegType, uNodeRegNum;

		/*
			Flag this node as no longer in the uncoloured list.
		*/
		SetBit(psNode->auFlags, NODE_FLAG_UNCOLOURED, IMG_FALSE);

		ASSERT(!GetBit(psNode->auFlags, NODE_FLAG_SPILL));

		/* Convert to a register type/register number. */
		NodeToRegister(&psRegState->sRAData, uNode, &uNodeRegType, &uNodeRegNum);

		/* Find a better node to spill */
		bUse = IMG_FALSE;
		if (VectorGet(psState, &psRegState->sNodesUsedInSpills, uNodeRegNum) ||
			!IsGroupSpillable(psRegState, uNode))
		{
			IMG_UINT32	uNewNode;

			uNewNode = FindBestSpillNode(psState, psRegState, uNode);
			if (uNewNode != uNode)
			{
				uNode = uNewNode;
				bUse = IMG_TRUE;
			}
		}
		else
		{
			bUse = IMG_TRUE;
		}

		if (bUse)
		{
			SpillEntireGroup(psState, psRegState, uNode);
		}
	}
	ASSERT(!IsListEmpty(&psRegState->sSpillList));

	/* Insert spill instructions */
	DoOnAllFuncGroupBasicBlocks(psState,
								ANY_ORDER,
								SpillRegistersBP,
								IMG_FALSE,
								psRegState,
								psRegState->sRAData.eFuncGroup);

	TESTONLY_PRINT_INTERMEDIATE("After spilling nodes", "spill", IMG_FALSE);
}

static
IMG_VOID MakeRegisterGroupsFlgsConsistent(PINTERMEDIATE_STATE psState, PREGISTER_GROUP psGroupHead, IMG_PVOID pvRegState)
/*****************************************************************************
 FUNCTION	: MakeRegisterGroupsFlgsConsistent

 PURPOSE	: If any register in a group of registers requiring consecutive
			  hardware register numbers is Referenced or Live accross Alpha 
			  Split or Live accross Sample Rate Split then mark all the
			  registers in the group accordingly.

 PARAMETERS	: psState		 - Compiler state.
			  psGroupHead	 - Group of registers to process.
              pvRegState     - Allocator state

 RETURNS	: Nothing.
*****************************************************************************/
{
	PRAGCOL_STATE psRegState = (PRAGCOL_STATE)pvRegState;
	IMG_BOOL bSmpRateStillSplit = IsSampleRateStillSplit(psState);
	IMG_BOOL bReferenced;
	IMG_BOOL bAlphaLive;
	IMG_BOOL bSplitLive;
	PREGISTER_GROUP psGroup;

	ASSERT(psGroupHead->psPrev == NULL);

	bReferenced = IMG_FALSE;
	bAlphaLive = IMG_FALSE;
	bSplitLive = IMG_FALSE;
	for (psGroup = psGroupHead; psGroup != NULL; psGroup = psGroup->psNext)
	{
		IMG_UINT32	uGroupNode;

		uGroupNode = RegisterToNode(&psRegState->sRAData, USEASM_REGTYPE_TEMP, psGroup->uRegister);
		if (!bReferenced && GetBit(psRegState->asNodes[uGroupNode].auFlags, NODE_FLAG_REFERENCED))
		{
			bReferenced = IMG_TRUE;			
		}
		if (bSmpRateStillSplit && (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL) && !bAlphaLive && GetBit(psRegState->asNodes[uGroupNode].auFlags, NODE_FLAG_ALPHALIVE))
		{
			bAlphaLive = IMG_TRUE;			
		}
		if (!bSplitLive && GetBit(psRegState->asNodes[uGroupNode].auFlags, NODE_FLAG_SPLITLIVE))
		{
			bSplitLive = IMG_TRUE;			
		}
	}

	if (bReferenced || bAlphaLive || bSplitLive)
	{
		for (psGroup = psGroupHead; psGroup != NULL; psGroup = psGroup->psNext)
		{
			IMG_UINT32	uGroupNode;

			uGroupNode = RegisterToNode(&psRegState->sRAData, USEASM_REGTYPE_TEMP, psGroup->uRegister);
			if (bReferenced)
			{
				SetBit(psRegState->asNodes[uGroupNode].auFlags, NODE_FLAG_REFERENCED, 1);
			}
			if (bAlphaLive)
			{
				SetBit(psRegState->asNodes[uGroupNode].auFlags, NODE_FLAG_ALPHALIVE, 1);
			}
			if (bSplitLive)
			{
				SetBit(psRegState->asNodes[uGroupNode].auFlags, NODE_FLAG_SPLITLIVE, 1);
			}
		}
	}
}

static
IMG_VOID PrepRegState(PINTERMEDIATE_STATE psState,
					  PRAGCOL_STATE psRegState,
					  FUNCGROUP eFuncGroup)
/*****************************************************************************
 FUNCTION	: PrepRegState

 PURPOSE	: Prepare the register state for off-line register allocation

 PARAMETERS	: psState		 - Compiler state.
              psRegState     - Allocator state

 RETURNS	: Nothing.

 NOTES      : Forms the interference graph and mandatory register groups.
*****************************************************************************/
{
	INTFGRAPH_CONTEXT	sIntfContext;

	/* Flag registers which are C10 formats. */
	DoOnAllFuncGroupBasicBlocks(psState, ANY_ORDER, FlagC10NodesBP, IMG_FALSE, psRegState, eFuncGroup);

	/*
		Flag C10 secondary attributes (which are treated as modifiable registers in the secondary
		update program).
	*/
	if (eFuncGroup == FUNCGROUP_SECONDARY)
	{
		PUSC_LIST_ENTRY	psListEntry;

		for (psListEntry = psState->sSAProg.sInRegisterConstantList.psHead;
		     psListEntry != NULL;
			 psListEntry = psListEntry->psNext)
		{
			PINREGISTER_CONST	psConst = IMG_CONTAINING_RECORD(psListEntry, PINREGISTER_CONST, sListEntry);

			if (psConst->eFormat == UNIFLEX_CONST_FORMAT_C10)
			{
				IMG_UINT32		uNode;
				PFIXED_REG_DATA	psFixedReg = psConst->psResult->psFixedReg;

				ASSERT(psFixedReg->uConsecutiveRegsCount == 1);
				ASSERT(psFixedReg->uVRegType == USEASM_REGTYPE_TEMP);
				uNode = RegisterToNode(&psRegState->sRAData, USEASM_REGTYPE_TEMP, psFixedReg->auVRegNum[0]);
				SetBit(psRegState->asNodes[uNode].auFlags, NODE_FLAG_C10, IMG_TRUE);
			}
		}
	}

	/*
		Allocate storage for the set of currently live registers.
	*/
	sIntfContext.sLiveSet.psLiveList = SparseSetAlloc(psState, psRegState->sRAData.uNrRegisters);
	sIntfContext.sLiveSet.auLiveChanMask = 
		UscAlloc(psState, sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(psRegState->sRAData.uNrRegisters * CHANS_PER_REGISTER));
	sIntfContext.psRegState = psRegState;

	/* Do per-block initialisation */
	DoOnAllFuncGroupBasicBlocks(psState, ANY_ORDER, ConstructInterferenceGraphBP, IMG_FALSE, &sIntfContext, eFuncGroup);

	/*
		Free storage for the set of currently live registers.
	*/
	SparseSetFree(psState, sIntfContext.sLiveSet.psLiveList);
	sIntfContext.sLiveSet.psLiveList = NULL;
	
	UscFree(psState, sIntfContext.sLiveSet.auLiveChanMask);
	sIntfContext.sLiveSet.auLiveChanMask = NULL;

	/*
		For groups of registers which have to be given consecutive hardware register numbers: ensure if
		any register in the group is referenced that all the registers in the group are marked as referenced.
	*/
	ForAllRegisterGroups(psState, MakeRegisterGroupsFlgsConsistent, psRegState);
}

static
IMG_INT32 NodeCmp(const IMG_VOID* pvElem1, const IMG_VOID* pvElem2)
/*****************************************************************************
 FUNCTION	: NodeCmp

 PURPOSE	: Returns the register allocation order for two virtual registers.

 PARAMETERS	: pvElem1, pvElem2	- The two registers to compare.

 RETURNS    : The order of the two registers.
*****************************************************************************/
{
	const PNODE_DATA* ppsElem1 = (const PNODE_DATA*)pvElem1;
	const PNODE_DATA* ppsElem2 = (const PNODE_DATA*)pvElem2;
	const PNODE_DATA  psElem1  = *ppsElem1;
	const PNODE_DATA  psElem2  = *ppsElem2;

	IMG_UINT32	uElem1IsAlpha	= GetBit(psElem1->auFlags, NODE_FLAG_ALPHALIVE);
	IMG_UINT32	uElem1IsSplit	= GetBit(psElem1->auFlags, NODE_FLAG_SPLITLIVE);
	IMG_UINT32	uElem1IsUSPTemp	= GetBit(psElem1->auFlags, NODE_FLAG_USPTEMP);
	IMG_UINT32	uElem1Degree	= psElem1->uDegree;
	IMG_UINT32	uElem2IsAlpha	= GetBit(psElem2->auFlags, NODE_FLAG_ALPHALIVE);
	IMG_UINT32	uElem2IsSplit	= GetBit(psElem2->auFlags, NODE_FLAG_SPLITLIVE);
	IMG_UINT32	uElem2IsUSPTemp	= GetBit(psElem2->auFlags, NODE_FLAG_USPTEMP);
	IMG_UINT32	uElem2Degree	= psElem2->uDegree;

	/*
		Give registers live at the point of feedback a higher priority.
	*/
	if (uElem1IsAlpha != uElem2IsAlpha)
	{
		return (IMG_INT32)(uElem1IsAlpha - uElem2IsAlpha);
	}
	/*
		Give registers live at the point of split a higher priority.
	*/
	if (uElem1IsSplit != uElem2IsSplit)
	{
		return (IMG_INT32)(uElem1IsSplit - uElem2IsSplit);
	}
	/*
		Give USP temporary registers a lower priority.
	*/
	if (uElem1IsUSPTemp != uElem2IsUSPTemp)
	{
		return (IMG_INT32)(uElem2IsUSPTemp - uElem1IsUSPTemp);
	}

	/*
		Sort registers in decreasing order of degree.
	*/
	if (uElem2Degree != uElem1Degree)
	{
		return (IMG_INT32)(uElem2Degree - uElem1Degree);
	}

	/*
		Otherwise sort by virtual register number.
	*/
	return (IMG_INT32)(psElem1 - psElem2);
}

static
PNODE_DATA* SortRegsByDegree(PRAGCOL_STATE psRegState)
/*****************************************************************************
 FUNCTION	: SortRegsByDegree

 PURPOSE	: Sort registers by degree

 PARAMETERS	: psState		 - Compiler state.
              psRegState     - Allocator state

 OUTPUT     : bRestart       - Whether the allocation must be restarted.

 RETURNS	: Nothing.

 NOTES      : Forms the interference graph and mandatory register groups.
*****************************************************************************/
{
	IMG_UINT32			uNode;
	PINTERMEDIATE_STATE psState = psRegState->sRAData.psState;
	PNODE_DATA*			apsRegistersSortedByDegree;
	IMG_UINT32			uOutIdx;

	psRegState->uNumUsedRegisters = 0;
	for (uNode = 0; uNode < psRegState->sRAData.uNrRegisters; uNode++)
	{
		PNODE_DATA	psNode = &psRegState->asNodes[uNode];

		if (GetBit(psNode->auFlags, NODE_FLAG_REFERENCED))
		{
			psRegState->uNumUsedRegisters++;
		}
	}

	apsRegistersSortedByDegree = UscAlloc(psState, sizeof(apsRegistersSortedByDegree[0]) * psRegState->uNumUsedRegisters);
	uOutIdx = 0;
	for (uNode = 0; uNode < psRegState->sRAData.uNrRegisters; uNode++)
	{
		PNODE_DATA	psNode = &psRegState->asNodes[uNode];

		if (GetBit(psNode->auFlags, NODE_FLAG_REFERENCED))
		{
			psNode->uDegree = IntfGraphGetVertexDegree(psRegState->psIntfGraph, uNode);
			apsRegistersSortedByDegree[uOutIdx++] = psNode;
		}
	}
	ASSERT(uOutIdx == psRegState->uNumUsedRegisters);

	qsort(apsRegistersSortedByDegree, psRegState->uNumUsedRegisters, sizeof(apsRegistersSortedByDegree[0]), NodeCmp);

	return apsRegistersSortedByDegree;
}

#if defined(OUTPUT_USPBIN)
static
IMG_VOID PrecolourFixedRegToDummy(PRAGCOL_STATE		psRegState,
								  PFIXED_REG_DATA	psFixedReg,
								  PARG				psPReg,
								  IMG_UINT32		uColourIdx)
/*****************************************************************************
 FUNCTION	  : PrecolourFixedRegToDummy

 PURPOSE	  : Set the colours for all the virtual registers in a group
			    of registers with fixed colours to the dummy colour.

 PARAMETERS	  : psRegState			- Register allocator state
				psFixedReg			- Group of fixed registers to set the
									colours for.
				psPReg				- Hardware register.
				uColourIdx			- Index of the colour to set to dummy.

 RETURNS	  : Nothing.
*****************************************************************************/
{
	IMG_UINT32	uIdx;
	COLOUR		sDummyColour;

	sDummyColour.eType = COLOUR_TYPE_DUMMY;
	sDummyColour.uNum = USC_UNDEF;

	for (uIdx = 0; uIdx < psFixedReg->uConsecutiveRegsCount; uIdx++)
	{
		IMG_UINT32	uNode;

		uNode = RegisterToNode(&psRegState->sRAData, psFixedReg->uVRegType, psFixedReg->auVRegNum[uIdx]);
		SetNodeColour(psRegState, uNode, uColourIdx, &sDummyColour);
	}

	/*
		No physical register is associated with this fixed register.
	*/
	InitInstArg(psPReg);
	psPReg->uType = USC_REGTYPE_UNUSEDDEST;
}
#endif /* defined(OUTPUT_USPBIN) */

static
IMG_VOID UpdateLiveSetsForShaderEndMOV(PINTERMEDIATE_STATE	psState,
									   PFIXED_REG_DATA		psFixedReg,
									   IMG_UINT32			uIdx,
									   PCODEBLOCK			psDefBlock,
									   IMG_UINT32			uOldRegNum,
									   IMG_UINT32			uNewRegNum)
/*****************************************************************************
 FUNCTION	  : UpdateLiveSetsForShaderEndMOV

 PURPOSE	  : Updated the stored sets of registers live out of each block
				when replacing one register representing a shader result by
				another.

 PARAMETERS	  : psState				- Compiler state
                psFixedReg			- Group of fixed registers.
				uIdx				- Index into the group for the register
									which has been modified.
			    psDefBlock			- Block where the new register is defined.
			    uOldRegNum			- Old register number.
				uNewRegNum			- New register number.

 RETURNS	  : Nothing.
*****************************************************************************/
{
	FEEDBACK_USE_TYPE	eFeedbackUseType;
	PCODEBLOCK			psUseBlock;
	BLOCK_WORK_LIST		sWorkList;
	PCODEBLOCK			psWorkBlock;
	IMG_UINT32			uUsedChanMask = psFixedReg->auMask != NULL ? psFixedReg->auMask[uIdx] : USC_ALL_CHAN_MASK;

	/*
		Which driver epilogs is the shader result used in?
	*/
	if (psFixedReg->aeUsedForFeedback != NULL)
	{
		eFeedbackUseType = psFixedReg->aeUsedForFeedback[uIdx];
	}
	else
	{
		eFeedbackUseType = FEEDBACK_USE_TYPE_NONE;
	}
	if (eFeedbackUseType == FEEDBACK_USE_TYPE_NONE || eFeedbackUseType == FEEDBACK_USE_TYPE_BOTH)
	{
		psUseBlock = psState->psMainProg->sCfg.psExit;
	}
	else
	{
		ASSERT(eFeedbackUseType == FEEDBACK_USE_TYPE_ONLY);
		psUseBlock = psState->psPreFeedbackBlock;
	}

	InitializeBlockWorkList(&sWorkList);
	PrependToBlockWorkList(&sWorkList, psUseBlock);
	while ((psWorkBlock = RemoveBlockWorkListHead(psState, &sWorkList)) != NULL)
	{
		PREGISTER_LIVESET	psLiveSet = &psWorkBlock->sRegistersLiveOut;

		if (GetRegisterLiveMask(psState, psLiveSet, psFixedReg->uVRegType, uNewRegNum, 0 /* uArrayOffset */) != 0)
		{
			continue;
		}

		SetRegisterLiveMask(psState, psLiveSet, psFixedReg->uVRegType, uNewRegNum, 0 /* uArrayOffset */, uUsedChanMask);

		if (psWorkBlock != psDefBlock)
		{
			if (IsCall(psState, psWorkBlock))
			{
				PINST	psCallInst = psWorkBlock->psBody;

				ASSERT(psCallInst->eOpcode == ICALL);
				AppendToBlockWorkList(&sWorkList, psCallInst->u.psCall->psTarget->sCfg.psExit);
			}
			AppendPredecessorsToBlockWorkList(&sWorkList, psWorkBlock);
		}
	}

	SetRegisterLiveMask(psState, 
						&psState->psMainProg->sCfg.psExit->sRegistersLiveOut, 
						psFixedReg->uVRegType, 
						uOldRegNum, 
						0 /* uArrayOffset */, 
						0 /* uMask */);
}

static
IMG_VOID InsertShaderEndMOV(PINTERMEDIATE_STATE	psState,
							PFIXED_REG_DATA		psFixedReg,
							IMG_UINT32			uIdx,
							PREGISTER_GROUP*	ppsAttachPoint,
							IMG_BOOL			bAlwaysAtEnd)
/*****************************************************************************
 FUNCTION	  : InsertShaderEndMOV

 PURPOSE	  : Fix a conflict between two different variables precoloured to the
				same hardware register by replacing one of the variables by a
				newly allocated one and inserting a MOV from the old variable
				to the new one at the end of the shader.

 PARAMETERS	  : psState				- Compiler state
                psFixedReg			- Group of fixed registers to precolour.
				uIdx				- Index into the group for the register
									to insert a MOV for.
				bAlwaysAtEnd		- Even if this shader result is used in the
									pre-feedback driver epilog insert the move
									before the main epilog.

 RETURNS	  : Nothing.
*****************************************************************************/
{
	PINST				psMovInst;
	IMG_UINT32			uNewShaderResultReg;
	PCODEBLOCK			psInsertBlock;
	IMG_UINT32			uVRegNum = psFixedReg->auVRegNum[uIdx];
	PREGISTER_GROUP		psNewResultGroup;
	IMG_UINT32			uPhysicalRegNum;

	/*
		Get the block to insert the MOV into.
	*/
	if (bAlwaysAtEnd)
	{
		psInsertBlock = psState->psMainProg->sCfg.psExit;
	}
	else
	{
		psInsertBlock = GetShaderEndBlock(psState, psFixedReg, uIdx);
	}

	/*
		We can only fix the conflict between nodes using the same fixed colour where
		one is live into the shader and the other is live out.
	*/
	ASSERT(psFixedReg->bLiveAtShaderEnd);
	ASSERT(psFixedReg->bPrimary);

	/*
		Get a new register to replace the one which can't be coloured.
	*/
	uNewShaderResultReg = GetNextRegister(psState);

	/*
		Add a MOV instruction to the end of the shader with the new register as its
		destination and the old register as its source.
	*/
	psMovInst = AllocateInst(psState, NULL);
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0U) && (psFixedReg->sPReg.uType == USEASM_REGTYPE_OUTPUT) && (psState->sHWRegs.uNumOutputRegisters > 1))
	{
		IMG_UINT32	uHWRegNum = psFixedReg->sPReg.uNumber + uIdx;
		SetOpcodeAndDestCount(psState, psMovInst, IVMOV, (uHWRegNum % 2) == 1 ? 2 : 1);
		if ((uHWRegNum % 2) == 1)
		{
			SetDestUnused(psState, psMovInst, 1 - (uHWRegNum % 2) /* uDestIdx */);
			psMovInst->auDestMask[1 - (uHWRegNum % 2)] = 0;	
			psMovInst->u.psVec->bWriteYOnly = IMG_TRUE;
		}
		SetDest(psState, psMovInst, uHWRegNum % 2 /* uDestIdx */, USEASM_REGTYPE_TEMP, uNewShaderResultReg, psFixedReg->aeVRegFmt[uIdx]);
		SetSrcUnused(psState, psMovInst, 0 /* uSrcIdx */);
		SetSrc(psState, psMovInst, 1 /* uSrcIdx */, psFixedReg->uVRegType, uVRegNum, psFixedReg->aeVRegFmt[uIdx]);
		SetSrcUnused(psState, psMovInst, 2 /* uSrcIdx */);
		SetSrcUnused(psState, psMovInst, 3 /* uSrcIdx */);
		SetSrcUnused(psState, psMovInst, 4 /* uSrcIdx */);
		if (psFixedReg->aeVRegFmt[uIdx] == UF_REGFORMAT_F16)
		{
			psMovInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, X, Y);
		}
		else
		{
			psMovInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, X, X, X);
		}
		psMovInst->u.psVec->aeSrcFmt[0] = psFixedReg->aeVRegFmt[uIdx];
	}
	else
	#endif	/* #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		SetOpcode(psState, psMovInst, IMOV);
		SetDest(psState, psMovInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uNewShaderResultReg, UF_REGFORMAT_UNTYPED);
		SetSrc(psState, psMovInst, 0 /* uSrcIdx */, psFixedReg->uVRegType, uVRegNum, UF_REGFORMAT_UNTYPED);
	}

	/*
		Outputs from the shader are never required for
		gradient calculations.
	*/
	SetBit(psMovInst->auFlag, INST_SKIPINV, 1);

	/*
		Add the MOV instruction to the end of the last block in the function.
	*/
	AppendInst(psState, psInsertBlock, psMovInst);

	/*
		Replace the old register by the new register in the set of registers live-out
		of the shader.
	*/
	UpdateLiveSetsForShaderEndMOV(psState, psFixedReg, uIdx, psInsertBlock, uVRegNum, uNewShaderResultReg);

	/*
		Replace the old register in the structure representing a group of
		fixed registers.
	*/
	ASSERT(psFixedReg->uVRegType == USEASM_REGTYPE_TEMP);
	UseDefDropFixedRegUse(psState, psFixedReg, uIdx);
	psFixedReg->auVRegNum[uIdx] = uNewShaderResultReg;
	UseDefAddFixedRegUse(psState, psFixedReg, uIdx);

	/*
		Add register grouping information for the replacement register.
	*/
	psNewResultGroup = AddRegisterGroup(psState, uNewShaderResultReg);
	psNewResultGroup->psFixedReg = psFixedReg;
	psNewResultGroup->uFixedRegOffset = uIdx;

	/*
		Link the replacement register together with the other register precoloured
		to consecutive hardware registers.
	*/
	if ((*ppsAttachPoint) != NULL)
	{
		IMG_BOOL	bRet;

		bRet = AddToGroup(psState,
						  (*ppsAttachPoint)->uRegister,
						  (*ppsAttachPoint),
						  psNewResultGroup->uRegister,
						  psNewResultGroup,
						  IMG_FALSE /* bLinkedByInst */,
						  IMG_FALSE /* bOptional */);
		ASSERT(bRet);
	}
	(*ppsAttachPoint) = psNewResultGroup;

	/*
		Set the required alignment of the replacement register.
	*/
	uPhysicalRegNum = psFixedReg->sPReg.uNumber + uIdx;
	if (uPhysicalRegNum != USC_UNDEF)
	{
		HWREG_ALIGNMENT		eNewResultAlignment;

		if ((uPhysicalRegNum % 2) == 0)
		{
			eNewResultAlignment = HWREG_ALIGNMENT_EVEN;
		}
		else
		{
			eNewResultAlignment = HWREG_ALIGNMENT_ODD;
		}
		SetNodeAlignment(psState, psNewResultGroup, eNewResultAlignment, IMG_FALSE /* bAlignRequiredByInst */);
	}	
}

static
IMG_VOID SpillPrecolouredNode(PINTERMEDIATE_STATE	psState, 
							  IMG_UINT32			uNumber, 
							  IMG_PUINT32			puFirstUnspilledIdx,
							  IMG_BOOL				bAlwaysAtEnd)
/*****************************************************************************
 FUNCTION	  : SpillPrecolouredNode

 PURPOSE	  : Fix cases where a precoloured node can't be given its
			    fixed colour.

 PARAMETERS	  : psState				- Compile state
                uNumber				- Register number which can't be coloured.
				puFirstUnspilledIdx	- Returns the next index in the same
									group of precoloured registers which might
									be colourable.
				bAlwaysAtEnd		- If TRUE always insert the move to copy
									to the new shader output at the end of the
									shader.

 RETURNS	  : Nothing.
*****************************************************************************/
{
	PREGISTER_GROUP		psNodeGroup = FindRegisterGroup(psState, uNumber);
	PREGISTER_GROUP		psPrevNodeGroup;
	PREGISTER_GROUP		psNextNodeGroup;
	PREGISTER_GROUP		psSpillGroup;
	PREGISTER_GROUP		psFirstNodeToSpill;
	PREGISTER_GROUP		psLastNodeToSpill;
	PREGISTER_GROUP		psAttachPoint;

	/*
		Fix the conflict by inserting a MOV at the end of the shader e.g.

			Y <- .....

		->
			Y <- .....
			[...]
			X <- Y

		X is given the fixed colour and Y is allowed any colour.
	*/

	/*
		Step backwards to the first register which is used together with the
		uncolourable register in instructions which require their arguments to
		have consecutive hardware register numbers.
	*/
	psPrevNodeGroup = psNodeGroup;
	while (psPrevNodeGroup->psPrev != NULL && psPrevNodeGroup->psPrev->bLinkedByInst)
	{
		psPrevNodeGroup = psPrevNodeGroup->psPrev;
	}
	psFirstNodeToSpill = psPrevNodeGroup;

	/*
		Step forwards to the last register which is used together with the
		uncolourable register.
	*/
	psNextNodeGroup = psNodeGroup;
	while (psNextNodeGroup->psNext != NULL && psNextNodeGroup->bLinkedByInst)
	{
		psNextNodeGroup = psNextNodeGroup->psNext;
	}
	psLastNodeToSpill = psNextNodeGroup;

	/*
		Remove the link between the first register to spill and any previous register.
	*/
	psAttachPoint = psFirstNodeToSpill->psPrev;
	if (psAttachPoint != NULL)
	{
		DropLinkAfterNode(psState, psAttachPoint);
	}

	/*
		Return the next index in the current group of fixed colour registers which
		might be colourable.
	*/
	if (puFirstUnspilledIdx != NULL)
	{
		if (psLastNodeToSpill->psFixedReg == psNodeGroup->psFixedReg)
		{
			(*puFirstUnspilledIdx) = psLastNodeToSpill->uFixedRegOffset + 1;
		}
		else
		{
			(*puFirstUnspilledIdx) = psNodeGroup->psFixedReg->uConsecutiveRegsCount;
		}
	}

	/*
		For each register which can't be coloured insert a MOV at the end of the shader to copy
		from the old register to a newly allocated register and make the newly allocated register
		precoloured.
	*/
	psSpillGroup = psFirstNodeToSpill;
	for (;;)
	{
		ASSERT(psSpillGroup->psFixedReg != NULL);

		InsertShaderEndMOV(psState, 
						   psSpillGroup->psFixedReg, 
						   psSpillGroup->uFixedRegOffset, 
						   &psAttachPoint,
						   bAlwaysAtEnd);

		/*
			Mark the uncolourable register as not having a fixed colour.
		*/
		psSpillGroup->psFixedReg = NULL;
		psSpillGroup->uFixedRegOffset = 0;

		/*
			If the uncolourable register isn't used in any instructions which require its hardware register
			number to have a particular alignment then clear the alignment restriction.
		*/
		if (!psSpillGroup->bAlignRequiredByInst)
		{
			psSpillGroup->eAlign = HWREG_ALIGNMENT_NONE;
		}

		if (psSpillGroup == psLastNodeToSpill)
		{
			break;
		}
		psSpillGroup = psSpillGroup->psNext;
	} 

	/*
		Link the last replacement register together with the first precoloured node we didn't spill.
	*/
	ASSERT(psAttachPoint->psNext == NULL);
	if (psLastNodeToSpill->psNext != NULL)
	{
		IMG_BOOL			bRet;
		PREGISTER_GROUP		psFirstUnspilled;

		psFirstUnspilled = psLastNodeToSpill->psNext;

		DropLinkAfterNode(psState, psLastNodeToSpill);

		bRet = AddToGroup(psState,
						  psAttachPoint->uRegister,
						  psAttachPoint,
						  psFirstUnspilled->uRegister,
						  psFirstUnspilled,
						  IMG_FALSE /* bLinkedByInst */,
						  IMG_FALSE /* bOptional */);
		ASSERT(bRet);
	}
}

static
IMG_VOID PrecolourNode(PRAGCOL_STATE		psRegState,
					   PFIXED_REG_DATA		psFixedReg,
					   PARG					psPReg,
					   IMG_UINT32			uColourIdx,
					   IMG_PBOOL			pbRestart)
/*****************************************************************************
 FUNCTION	  : PrecolourNode

 PURPOSE	  : Precolours all the nodes in a group of registers which require
				fixed colours.

 PARAMETERS	  : psRegState			- Register allocator state
                psFixedReg			- Group of fixed registers to precolour.
				psPReg				- Hardware register to precolour to.
				uColourIdx			- Which of the register's colour is to be set.
				pbRestart			- Set to TRUE on return if register
									allocation needs to be restarted.

 RETURNS	  : Nothing.
*****************************************************************************/
{
	COLOUR				sBaseFixedColour;
	IMG_UINT32			uIdx;
	IMG_BOOL			bNoValidColour;
	PINTERMEDIATE_STATE	psState = psRegState->sRAData.psState;

	if (psFixedReg->uConsecutiveRegsCount == 0)
	{
		return;
	}

	/*
		Get the fixed colour we want to give this node.
	*/
	sBaseFixedColour = ArgumentToColour(&psRegState->sRAData, psPReg);
	
	bNoValidColour = IMG_FALSE;
	for (uIdx = 0; uIdx < psFixedReg->uConsecutiveRegsCount; )
	{
		IMG_UINT32	uNode = RegisterToNode(&psRegState->sRAData, psFixedReg->uVRegType, psFixedReg->auVRegNum[uIdx]);
		COLOUR		sFixedColour;
		IMG_BOOL	bColouringFailed;

		sFixedColour = sBaseFixedColour;
		sFixedColour.uNum += uIdx;

		/*
			If the same intermediate register is both a shader input and a shader output then it may already
			have been coloured.
		*/
		if (!IntfGraphIsVertexRemoved(psRegState->psIntfGraph, uNode))
		{
			PNODE_DATA	psNode = &psRegState->asNodes[uNode];
			ASSERT(psNode->asColours[uColourIdx].eType == sFixedColour.eType);
			ASSERT(psNode->asColours[uColourIdx].uNum == sFixedColour.uNum);
			uIdx++;
			continue;
		}

		bColouringFailed = IMG_FALSE;
		/*
			Check if bank restrictions on the instructions where this node is used mean it
			can't be given the fixed colour.
		*/
		if (psRegState->asNodes[uNode].uBankFlags == 0)
		{
			bColouringFailed = IMG_TRUE;
		}
		/*
			Check if this node conflicts with another given the same fixed colour.
		*/
		if (!CanNodeUseColour(psRegState, uNode, &sFixedColour, IMG_FALSE /* bHonour */, NULL))
		{
			bColouringFailed = IMG_TRUE;
		}
		/*
			Check if this node is used in the post-feedback phase but the contents of the fixed hardware
			register are lost inbetween the pre- and post-feedback phases.
		*/
		if (
				psFixedReg->sPReg.uType != USEASM_REGTYPE_PRIMATTR && 
				psFixedReg->sPReg.uType != USEASM_REGTYPE_OUTPUT &&
				GetBit(psRegState->asNodes[uNode].auFlags, NODE_FLAG_ALPHALIVE)
			)
		{
			bColouringFailed = IMG_TRUE;
		}

		if (bColouringFailed)
		{
			/*
				Insert extra instructions to fix conflicts between precoloured
				nodes.
			*/
			SpillPrecolouredNode(psState, psFixedReg->auVRegNum[uIdx], &uIdx, IMG_FALSE /* bAlwaysAtEnd */);

			/*
				Restart register allocation.
			*/
			bNoValidColour = IMG_TRUE;
		}
		else
		{
			uIdx++;
		}
	}					

	if (bNoValidColour)
	{
		*pbRestart = IMG_TRUE;
		return;
	}

	/*
		Set all the nodes as having a colour.
	*/
	for (uIdx = 0; uIdx < psFixedReg->uConsecutiveRegsCount; uIdx++)
	{
		IMG_UINT32	uNode;
		COLOUR		sFixedColour;

		sFixedColour = sBaseFixedColour;
		sFixedColour.uNum += uIdx;

		uNode = RegisterToNode(&psRegState->sRAData, psFixedReg->uVRegType, psFixedReg->auVRegNum[uIdx]);
		if (IntfGraphIsVertexRemoved(psRegState->psIntfGraph, uNode))
		{
			SetNodeColour(psRegState, uNode, uColourIdx, &sFixedColour);
		}
	}

	if (
			uColourIdx == 0 &&
			psPReg->uType == USEASM_REGTYPE_TEMP
	   )
	{
		/*
		  Update the count of temporary registers used in the
		  program.
		*/
		if (
			(psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL) && 
			(psState->uFlags2 & USC_FLAGS2_SPLITCALC) && (psState->psPreSplitBlock != NULL) && psFixedReg->bLiveAtShaderEnd
		)
		{
			psRegState->uTemporaryRegisterCountPostSplit =
				max(psRegState->uTemporaryRegisterCountPostSplit, psPReg->uNumber + psFixedReg->uConsecutiveRegsCount);			
		}
		else
		{
			psRegState->uTemporaryRegisterCount =
				max(psRegState->uTemporaryRegisterCount, psPReg->uNumber + psFixedReg->uConsecutiveRegsCount);
		}
	}
}

static
IMG_VOID SpillFixedTypeShaderOutput(PINTERMEDIATE_STATE	psState,
									PRAGCOL_STATE		psRegState,
									IMG_UINT32			uPAInputsToAdd,
									HWREG_ALIGNMENT		ePAAlignment,
									IMG_PBOOL			pbAddedExtraPrimaryAttribute,
									IMG_PBOOL			pbRestart)
/*****************************************************************************
 FUNCTION	  : SpillFixedTypeShaderOutput

 PURPOSE	  : Apply special behaviour when a shader output which must
				be allocated a particular hardware register type can't
				be coloured.

 PARAMETERS	  : psState			- Compiler state.
				uPAInputsToAdd	- PA inputs to add.
				ePAAlignment	- Required aligmemnt of the register hardware
								register numbers assigned to the shader output.

 INPUT/OUTPUT : pbAddedExtraPrimaryAttribute
								- If TRUE then extra primary attributes were
								added to allow a shader output to be coloured.

 OUTPUT		  : pbRestart		- If TRUE repeat register allocation again.

 RETURNS	  : Nothing.
*****************************************************************************/
{
	ASSERT(psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL);

	PVR_UNREFERENCED_PARAMETER(psRegState);
	
	/*
		Add an extra primary attribute so the alignment of the start of the region available for the shader output
		matches the required alignment.
	*/
	if (ePAAlignment != HWREG_ALIGNMENT_NONE)
	{
		if ((psState->sHWRegs.uNumPrimaryAttributes % 2) != g_auRequiredAlignment[ePAAlignment])
		{
			uPAInputsToAdd++;
		}
	}

	#if defined(OUTPUT_USPBIN)
	/*
		Add extra dummy iterations 

		NB: These can potentially be removed by the USP under some
			circumstances, so we flag them differently from other
			'extra' iterations that are added for other reasons.
	*/
	if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
	{
		IMG_UINT32			uRegIdx;		
		
		ASSERT(psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL);
	
		for (uRegIdx = 0; uRegIdx < uPAInputsToAdd; uRegIdx++)
		{
			AddDummyIteration(psState, 
							  PIXELSHADER_INPUT_FLAG_OUTPUT_PADDING, 
							  psState->sHWRegs.uNumPrimaryAttributes + uRegIdx);
		}
	}
	#endif /* defined(OUTPUT_USPBIN) */

	/* Should only need to add primary attributes once only. */
	ASSERT(!(*pbAddedExtraPrimaryAttribute));
	*pbAddedExtraPrimaryAttribute = IMG_TRUE;

	/*
		If we couldn't find a primary attribute then increase the number
		requested. Since the extra primary attribute will never be
		used elsewhere we will always be able to use it for the shader output.
	*/
	psState->sHWRegs.uNumPrimaryAttributes += uPAInputsToAdd;

	/*
		Repeat register allocation again.
	*/
	*pbRestart = IMG_TRUE;
}

#if defined(OUTPUT_USPBIN)
static
IMG_BOOL IsUnusedAlternateOutput(PINTERMEDIATE_STATE psState, PFIXED_REG_DATA psOut, IMG_UINT32 uColourIdx)
/*****************************************************************************
 FUNCTION	  : IsUnusedAlternateOutput

 PURPOSE	  : Check for an alternate shader output which is unused.

 PARAMETERS	  : psState			- Compiler state.
				psOut			- Shader output to check.
				uColour			- Alternate output to check.

 RETURNS	  : TRUE or FALSE
*****************************************************************************/
{
	if ((psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) == 0)
	{
		return IMG_FALSE;
	}
	if (psState->psSAOffsets->eShaderType != USC_SHADERTYPE_PIXEL)
	{
		return IMG_FALSE;
	}
	if (psOut != psState->sShader.psPS->psColFixedReg)
	{
		return IMG_FALSE;
	}
	if ((psState->uFlags & USC_FLAGS_RESULT_REFS_NOT_PATCHABLE) != 0)
	{
		if (uColourIdx == psState->sShader.psPS->uAltTempFixedReg || uColourIdx == psState->sShader.psPS->uAltPAFixedReg)
		{
			return IMG_TRUE;
		}
	}
	if ((psState->uFlags & USC_FLAGS_RESULT_REFS_NOT_PATCHABLE_WITH_PAS) != 0)
	{
		if (uColourIdx == psState->sShader.psPS->uAltPAFixedReg)
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}
#endif /* defined(OUTPUT_USPBIN) */

static
IMG_VOID ColourFixedRegister(PINTERMEDIATE_STATE	psState,
							 PRAGCOL_STATE			psRegState,
							 PCOLOUR_STATUS			psStatus,
							 PFIXED_REG_DATA		psOut,
							 IMG_PBOOL				pbRestart,
							 IMG_PBOOL				pbAddedExtraPrimaryAttribute)
/*****************************************************************************
 FUNCTION	  : ColourFixedRegister

 PURPOSE	  : Set the colours assigned to shader input or outputs.

 PARAMETERS	  : psState			- Compiler state.
                psRegState      - Register allocator state
				psStatus		- Temporary storage.
				psOut			- Shader input or output.

 OUTPUT		  : pbRestart		- If TRUE repeat register allocation again.
				pbAddedExtraPrimaryAttribute
								- If TRUE an extra primary attribute register
								was allocated.

 RETURNS	  : Nothing.
*****************************************************************************/
{
	IMG_UINT32	uColourIdx;
	IMG_UINT32	uCountOfColours;

	uCountOfColours = 1;
	#if defined(OUTPUT_USPBIN)	
	uCountOfColours += psOut->uNumAltPRegs;
	#endif /* defined(OUTPUT_USPBIN) */

	for (uColourIdx = 0; uColourIdx < uCountOfColours; uColourIdx++)
	{
		PARG	psPReg;

		if (uColourIdx == 0)
		{
			psPReg = &psOut->sPReg;
		}
		else
		{
			#if defined(OUTPUT_USPBIN)	
			psPReg = &psOut->asAltPRegs[uColourIdx - 1];
			#else  /* defined(OUTPUT_USPBIN) */
			imgabort();
			#endif /* defined(OUTPUT_USPBIN) */
		}

		if (psPReg->uNumber == USC_UNDEF)
		{
			IMG_BOOL	bNodeColoured;
			IMG_UINT32	uOutputNode;

			ASSERT(psPReg->uType == USEASM_REGTYPE_PRIMATTR || psPReg->uType == USEASM_REGTYPE_SECATTR);

			uOutputNode = RegisterToNode(&psRegState->sRAData, psOut->uVRegType, psOut->auVRegNum[0]);

			/*
				As a special case this node needs to be assigned a primary attribute but
				with any number so do colouring almost as normal.
			*/
			bNodeColoured = ColourNode(psRegState,
									   psStatus,
									   uOutputNode,
									   uColourIdx,
									   IMG_FALSE /* bHonour */,
									   &psRegState->sPAOnlyRange);
			if (!bNodeColoured)
			{
				ASSERT(psRegState->sRAData.eFuncGroup == FUNCGROUP_MAIN);
				
				#if defined(OUTPUT_USPBIN)
				if (
						(psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) != 0 &&
						psOut == psState->sShader.psPS->psColFixedReg &&
						uColourIdx == psState->sShader.psPS->uAltPAFixedReg
					)
				{
					/*
						If these fixed registers are alternate shader outputs then don't try and
						change things so we can colour them. Instead set their colors to a dummy
						value and tell the patcher to use an alternate method to handle changing
						the shader output to the primary attributes.
					*/
					psState->uFlags |= USC_FLAGS_RESULT_REFS_NOT_PATCHABLE_WITH_PAS;
					PrecolourFixedRegToDummy(psRegState, psOut, psPReg, uColourIdx);
				}
				else
				#endif /* defined(OUTPUT_USPBIN) */
				{
					IMG_UINT32		uPAInputsToAdd;
					HWREG_ALIGNMENT	eFixedRegAlignment;

					uPAInputsToAdd = psOut->uConsecutiveRegsCount;
					eFixedRegAlignment = GetNodeAlignment(GetRegisterGroup(&psRegState->sRAData, uOutputNode));

					SpillFixedTypeShaderOutput(psState, 
											   psRegState, 
											   uPAInputsToAdd, 
											   eFixedRegAlignment,
											   pbAddedExtraPrimaryAttribute, 
											   pbRestart);
				}
			}
		}
		else
		{
			#if defined(OUTPUT_USPBIN)
			if (IsUnusedAlternateOutput(psState, psOut, uColourIdx))
			{
				/*
					For alternate shader outputs if we aren't going to be able to make use of them then
					set their colours to dummy values.
				*/
				PrecolourFixedRegToDummy(psRegState, psOut, psPReg, uColourIdx);
			}
			else
			#endif /* defined(OUTPUT_USPBIN) */
			{
				PrecolourNode(psRegState,
							  psOut,
							  psPReg,
							  uColourIdx,
							  pbRestart);
			}
		}
	}
}

static
IMG_VOID AssignColours(PINTERMEDIATE_STATE psState,
					   PRAGCOL_STATE psRegState,
					   IMG_PBOOL pbRestart,
					   IMG_PBOOL pbAddedExtraPrimaryAttribute)
/*****************************************************************************
 FUNCTION	  : AssignColours

 PURPOSE	  : Find and assign registers (colours) to variables (nodes) for the
                offline compiler.

 PARAMETERS	  : psState			- Compiler state.
                psRegState      - Register allocator state

 OUTPUT		  : pbRestart		- If TRUE repeat register allocation again.

 RETURNS	  : Nothing.
*****************************************************************************/
{
	IMG_UINT32		uRegsRemoved;
	IMG_UINT32		i;
	PREGALLOC_DATA	psRAData = &psRegState->sRAData;
	REGSTACK		sRegStack;
	IMG_UINT32		uMaximumRegStackSize;
	PNODE_DATA*		apsRegistersSortedByDegree;
	COLOUR_STATUS	sStatus;

	/*
	 * Graph colouring
	 */

	/*
		Set default return value.
	*/
	*pbRestart = IMG_FALSE;

	/* Sort the registers by degree */
	apsRegistersSortedByDegree = SortRegsByDegree(psRegState);

	/* Remove the registers that hold the results of the shader so we can precolour them. */
	uRegsRemoved = 0;
	if (psRAData->psFixedVRegList != NULL)
	{
		PUSC_LIST_ENTRY	psListEntry;

		for (psListEntry = psRAData->psFixedVRegList->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
		{
			PFIXED_REG_DATA psOut = IMG_CONTAINING_RECORD(psListEntry, PFIXED_REG_DATA, sListEntry);

			if (psOut->uVRegType == USEASM_REGTYPE_TEMP)
			{
				IMG_UINT32	uConsecutiveRegIdx;

				for (uConsecutiveRegIdx = 0; uConsecutiveRegIdx < psOut->uConsecutiveRegsCount; uConsecutiveRegIdx++)
				{
					IMG_UINT32	uOutNode;

					uOutNode = RegisterToNode(&psRegState->sRAData, psOut->uVRegType, psOut->auVRegNum[uConsecutiveRegIdx]);
					if (!IntfGraphIsVertexRemoved(psRegState->psIntfGraph, uOutNode))
					{
						IntfGraphRemove(psState, psRegState->psIntfGraph, uOutNode);

						if (GetBit(psRegState->asNodes[uOutNode].auFlags, NODE_FLAG_REFERENCED))
						{
							uRegsRemoved++;
						}
					}
				}
			}
		}
	}

	/* Remove nodes which represent primary attribute registers. */
	for (i = 0; i < psRegState->sRAData.auAvailRegsPerType[COLOUR_TYPE_PRIMATTR]; i++)
	{
		IMG_UINT32	uNode = REGALLOC_PRIMATTR_START + i;

		IntfGraphRemove(psState, psRegState->psIntfGraph, uNode);
		if (GetBit(psRegState->asNodes[uNode].auFlags, NODE_FLAG_REFERENCED))
		{
			uRegsRemoved++;
		}
	}

	/* Remove nodes which represent output registers. */
	for (i = 0; i < psRegState->sRAData.auAvailRegsPerType[COLOUR_TYPE_OUTPUT]; i++)
	{
		IMG_UINT32	uNode = psRegState->sRAData.uOutputStart + i;

		IntfGraphRemove(psState, psRegState->psIntfGraph, uNode);
		if (GetBit(psRegState->asNodes[uNode].auFlags, NODE_FLAG_REFERENCED))
		{
			uRegsRemoved++;
		}
	}

	/* Find the registers that can be coloured. */
	sRegStack.uSize = 0;
	ASSERT(psRegState->uNumUsedRegisters >= uRegsRemoved);
	uMaximumRegStackSize = psRegState->uNumUsedRegisters - uRegsRemoved;
	sRegStack.auStack = UscAlloc(psState, sizeof(sRegStack.auStack[0]) * uMaximumRegStackSize);
	while (sRegStack.uSize != uMaximumRegStackSize)
	{
		while (SimplifyGraph(psRegState, apsRegistersSortedByDegree, IMG_FALSE, &sRegStack));
		while (SimplifyGraph(psRegState, apsRegistersSortedByDegree, IMG_TRUE, &sRegStack));
	}

	UscFree(psState, apsRegistersSortedByDegree);
	apsRegistersSortedByDegree = NULL;

	/*
	  Precolour nodes which represent primary attribute registers.
	*/
	for (i = 0; i < psRegState->sRAData.auAvailRegsPerType[COLOUR_TYPE_PRIMATTR]; i++)
	{
		COLOUR		sPAColour;
		IMG_UINT32	uPANode;

		uPANode = REGALLOC_PRIMATTR_START + i;

		sPAColour.eType = COLOUR_TYPE_PRIMATTR;
		sPAColour.uNum  = i;

		SetNodeColour(psRegState, uPANode, 0 /* uColourIdx */, &sPAColour);
	}

	/* Precolour nodes which represent output registers. */
	for (i = 0; i < psRegState->sRAData.auAvailRegsPerType[COLOUR_TYPE_OUTPUT]; i++)
	{
		COLOUR		sOutputColour;
		IMG_UINT32	uOutputNode;

		uOutputNode = psRegState->sRAData.uOutputStart + i;

		sOutputColour.eType = COLOUR_TYPE_OUTPUT;
		sOutputColour.uNum  = i;

		SetNodeColour(psRegState, uOutputNode, 0 /* uColourIdx */, &sOutputColour);
	}

	/*
		Allocate temporary storage to hold the vector of available colours while colouring
		intermediate registers.
	*/
	AllocColourStatus(psState, psRegState, &sStatus);

	/*
	   Precolour the registers which hold the results of the shader so
	   the code to pack them into the output buffer will know where to
	   find them.
	*/
	psRegState->uTemporaryRegisterCount = 0;
	psRegState->uTemporaryRegisterCountPostSplit = 0;
	psRegState->uPrimaryAttributeCount = 0;
	if (psRAData->psFixedVRegList != NULL)
	{
		PUSC_LIST_ENTRY	psListEntry;

		for (psListEntry = psRAData->psFixedVRegList->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
		{
			PFIXED_REG_DATA psOut = IMG_CONTAINING_RECORD(psListEntry, PFIXED_REG_DATA, sListEntry);

			if (psOut->uVRegType != USEASM_REGTYPE_TEMP)
			{
				continue;
			}
		
			ColourFixedRegister(psState, psRegState, &sStatus, psOut, pbRestart, pbAddedExtraPrimaryAttribute);	
		}

		/* pre colour the last use of any primary attributes before any other registers
		this means last uses of PA regs will try re-use their register and hopefully not have
		to use up a temp register */
		for (psListEntry = psRAData->psFixedVRegList->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
		{
			IMG_UINT32	uIdx;
			PFIXED_REG_DATA psFixedReg = IMG_CONTAINING_RECORD(psListEntry, PFIXED_REG_DATA, sListEntry);
			PARG	psPReg = &psFixedReg->sPReg;

			if (psFixedReg->uVRegType != USEASM_REGTYPE_TEMP)
			{
				continue;
			}

			if (psPReg->uType != USEASM_REGTYPE_PRIMATTR)
			{
				continue;
			}

			for (uIdx = 0; uIdx < psFixedReg->uConsecutiveRegsCount; uIdx++)
			{
				IMG_UINT32	uNode;
				PUSEDEF_CHAIN	psUseDef;
				PINST psSingleUseInst = IMG_NULL; 
				IMG_UINT32 uDest;
				PUSC_LIST_ENTRY	psListEntryL;

				psUseDef = UseDefGet(psState, psFixedReg->uVRegType, psFixedReg->auVRegNum[uIdx]);
				if (psUseDef == NULL)
				{
					continue;
				}

				for (psListEntryL = psUseDef->sList.psHead; psListEntryL != NULL; psListEntryL = psListEntryL->psNext)
				{
					PUSEDEF		psUse;
					PINST		psUseInst;

					psUse = IMG_CONTAINING_RECORD(psListEntryL, PUSEDEF, sListEntry);
					if (psUse == psUseDef->psDef)
					{
						if (psUse->eType != DEF_TYPE_FIXEDREG)
						{
							break;
						}
						continue;
					}
					if (psUse->eType != USE_TYPE_SRC)
					{
						continue;
					}
					psUseInst = psUse->u.psInst;
					if (psSingleUseInst != NULL)
					{
						break;
					}
					psSingleUseInst = psUseInst;
				}

				if (psListEntryL != NULL)
				{
					/* exited the loop early so for some reason we should skip this register */
					continue;
				}

				if (psSingleUseInst == NULL)
				{
					continue;
				}

				for (uDest = 0; uDest < psSingleUseInst->uDestCount; uDest++)
				{
					if (psSingleUseInst->asDest[uDest].uType != USEASM_REGTYPE_TEMP)
					{
						continue;
					}
					uNode = RegisterToNode(&psRegState->sRAData, psSingleUseInst->asDest[uDest].uType, psSingleUseInst->asDest[uDest].uNumber);
					{
						PREGISTER_GROUP		psNodeGroup = GetRegisterGroup(&psRegState->sRAData, uNode);

						if (IsNodeInGroup(psNodeGroup))
						{
							/* skip primary attributes which are in a group.  this could change the allocation order
							too much to be beneficial */
							continue;
						}
					}

					if(!GetBit(psRegState->asNodes[uNode].auFlags, NODE_FLAG_POSTSPLITPHASE))
					{
						ColourNode(psRegState, &sStatus, uNode, 0 /* uColourIdx */, IMG_TRUE, &psRegState->sGeneralRange);
					}
					else
					{
						ColourNode(psRegState, &sStatus, uNode, 0 /* uColourIdx */, IMG_TRUE, &psRegState->sPostSplitGeneralRange);
					}
				}
			}
		}
	}

	/*
	   Actually colour all the registers and build up an array of those
	   that need to be spilled.
	*/
	InitializeList(&psRegState->sUncolouredList);
	{
		IMG_BOOL bSmpRateStillSplit = IsSampleRateStillSplit(psState);
		IMG_UINT32 uPAInputsToAdd = 0U;
		IMG_BOOL bFeedBackSplitCleared = IMG_FALSE;
		for (i = 0; i < sRegStack.uSize; i++)
		{
			IMG_UINT32 uNode = sRegStack.auStack[sRegStack.uSize - i - 1];
			IMG_BOOL bNodeColoured;
			PCOLOUR_RANGE_LIST	psColourRangeList;

			if (GetBit(psRegState->asNodes[uNode].auFlags, NODE_FLAG_POSTSPLITPHASE))
			{
				psColourRangeList = &psRegState->sPostSplitGeneralRange;
			}
			else
			{
				psColourRangeList = &psRegState->sGeneralRange;
			}

			/* Try to colour the node */
			bNodeColoured = ColourNode(psRegState, &sStatus, uNode, 0 /* uColourIdx */, IMG_TRUE, psColourRangeList);
			if (!bNodeColoured)
			{
				/* Failed to colour the node, try ignoring reservations */
				bNodeColoured = ColourNode(psRegState, &sStatus, uNode, 0 /* uColourIdx */, IMG_FALSE, psColourRangeList);
			}
			if (!bNodeColoured)
			{
				PREGISTER_GROUP	psSpillGroup;

				/* Failed to colour the node, so spill it */
				psSpillGroup = GetRegisterGroup(&psRegState->sRAData, uNode);

				if (psSpillGroup == NULL || psSpillGroup->psPrev == NULL || psSpillGroup->psPrev->bOptional)
				{
					SetBit(psRegState->asNodes[uNode].auFlags, NODE_FLAG_UNCOLOURED, IMG_TRUE);
					AppendToList(&psRegState->sUncolouredList, &psRegState->asNodes[uNode].sSpillListEntry);
				}
			}
			else
			{
				/* Coloured the node */
				COLOUR_TYPE	eColourType;
				
				ASSERT(psRegState->asNodes[uNode].uColourCount == 1);
				eColourType = psRegState->asNodes[uNode].asColours[0].eType;

				if (eColourType == COLOUR_TYPE_TEMP)
				{
					IMG_BOOL bPAInputAlreaydAdded = IMG_FALSE;
					/*
					  If a register live across the point of feedback is
					  assigned a temporary then we can't split because
					  the value of a temporary is lost when restarting
					  after feedback so we need to increase the PAs and
					  restart register allocation.
					  OR
					  If a register live across the point of split is
					  assigned a temporary then we can't split because
					  the value of a temporary is lost when restarting
					  after split so we need to increase the PAs and 
					  restart register allocation.
					*/
					if (GetBit(psRegState->asNodes[uNode].auFlags, NODE_FLAG_ALPHALIVE) && (!bFeedBackSplitCleared))
					{
						if ((!bSmpRateStillSplit) || (psState->psSAOffsets->eShaderType != USC_SHADERTYPE_PIXEL))
						{
							ClearFeedbackSplit(psState);
							bFeedBackSplitCleared = IMG_TRUE;
						}
						else
						{
							uPAInputsToAdd++;
							bPAInputAlreaydAdded = IMG_TRUE;
						}
					}
					if (!bPAInputAlreaydAdded && GetBit(psRegState->asNodes[uNode].auFlags, NODE_FLAG_SPLITLIVE))
					{
						uPAInputsToAdd++;
					}					
				}
			}
		}
		if (uPAInputsToAdd > 0U)
		{
			SpillFixedTypeShaderOutput(	psState, 
										psRegState, 
										uPAInputsToAdd, 
										HWREG_ALIGNMENT_NONE,
										pbAddedExtraPrimaryAttribute, 
										pbRestart);
		}
	}

	FreeColourStatus(psState, &sStatus);

	UscFree(psState, sRegStack.auStack);
	sRegStack.auStack = NULL;
}

static
IMG_BOOL RenameReg(PRAGCOL_STATE psRegState, IMG_PUINT32 puType, IMG_PUINT32 puNum, IMG_BOOL bUpdateCount)
/*****************************************************************************
 FUNCTION	: RenameReg

 PURPOSE	: Rename a register based on the results of register allocation.

 PARAMETERS	: psRegState	- Register allocator state
			  puType		- Register to rename
			  puNum
			  bUpdateCount	- If TRUE update the stored count of temporary
							registers and primary attributes used in the program.

 RETURNS	: Whether a renaming happened.
*****************************************************************************/
{
	if ((*puType) == USEASM_REGTYPE_TEMP)
	{
		IMG_UINT32			uColourType, uColourNum;
		IMG_UINT32			uNode;
		PCOLOUR				psColour;
		PINTERMEDIATE_STATE	psState = psRegState->sRAData.psState;

		uNode = RegisterToNode(&psRegState->sRAData, (*puType), (*puNum));
		ASSERT(psRegState->asNodes[uNode].uColourCount >= 1);
		psColour = &psRegState->asNodes[uNode].asColours[0];

		ColourToRegister(&psRegState->sRAData, psColour, &uColourType, &uColourNum);

		(*puType) = uColourType;
		(*puNum) = uColourNum;

		if (bUpdateCount)
		{
			if (uColourType == USEASM_REGTYPE_TEMP)
			{
				if (!GetBit(psRegState->asNodes[uNode].auFlags, NODE_FLAG_POSTSPLITPHASE))
				{
					psRegState->uTemporaryRegisterCount =
						max(psRegState->uTemporaryRegisterCount, uColourNum + 1);
				}
				else
				{
					psRegState->uTemporaryRegisterCountPostSplit =
						max(psRegState->uTemporaryRegisterCountPostSplit, uColourNum + 1);
				}
			}
			if (uColourType == USEASM_REGTYPE_PRIMATTR)
			{
				psRegState->uPrimaryAttributeCount =
					max(psRegState->uPrimaryAttributeCount, uColourNum + 1);
			}
		}
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static
IMG_BOOL RenameArg(PRAGCOL_STATE psRegState, PARG psArg, IMG_BOOL bUpdateCount)
/*****************************************************************************
 FUNCTION	: RenameReg

 PURPOSE	: Rename a register based on the results of register allocation.

 PARAMETERS	: psRegState	- Register allocator state
			  psArg			- Register to rename
			  bUpdateCount	- If TRUE update the stored count of temporary
							registers and primary attributes used in the program.

 RETURNS	: Whether a renaming happened.
*****************************************************************************/
{
	return RenameReg(psRegState, &psArg->uType, &psArg->uNumber, bUpdateCount);
}

static
IMG_VOID DropInst(PINTERMEDIATE_STATE	psState,
				  PRAGCOL_STATE			psRegState,
				  PCODEBLOCK			psBlock,
				  PINST					psInst)
/*****************************************************************************
 FUNCTION	: DropInst

 PURPOSE	: Remove an instruction from a basic block and free it.

 PARAMETERS	: psState     - Compiler State
              psRegState  - Module state.
			  psBlock     - Basic block the instructions belong too.
			  psInst	  - Instruction to drop.

 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		If this was an instruction we converted from a MOV to a PCKU8U8/PCKC10C10
		then remove it from the list of such instructions.
	*/
	if (GetBit(psInst->auFlag, INST_CHANGEDMOVTOPCK))
	{
		RemoveFromList(psRegState->psMOVToPCKList, &psInst->sAvailableListEntry);
	}

	RemoveInst(psState, psBlock, psInst);
	FreeInst(psState, psInst);
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
PINST ConvertVPCKFLTFLTToMove(PINTERMEDIATE_STATE psState, PRAGCOL_STATE psRegState, PINST psInst, IMG_UINT32 uOtherDest)
/*****************************************************************************
 FUNCTION	: ConvertVPCKFLTFLTToMove

 PURPOSE	: Convert a vector move into a scalar move, if possible.

 PARAMETERS	: psState		- Compiler state.
			  psRegState	- Register allocator state.
              psInst		- Instruction to convert.
			  uOtherDest	- The destination to copy to in the new instruction.

 RETURNS	: The scalar move instruction, if successful. NULL otherwise.
*****************************************************************************/
{
	PINST		psMovInst;
	IMG_UINT32	uOtherDestSrc;
	
	/*
		Get the index of the source argument copied to the other (non-coalesced) destination.
	*/
	uOtherDestSrc = GetVPCKFLTFLT_EXPSourceOffset(psState, psInst, uOtherDest);
	
	/* Only convert UNPCKVEC -> MOV if the source is aligned right. */
	if ((psInst->asArg[uOtherDestSrc].uType == USEASM_REGTYPE_FPCONSTANT) &&
		((psInst->asArg[uOtherDestSrc].uNumber % (1U << QWORD_SIZE_LOG2)) != 0))
	{
		psInst->auLiveChansInDest[1 - uOtherDest] = 0;
		return NULL;
	}

	/*
		Convert the instruction to a simple move.
	*/
	psMovInst = AllocateInst(psState, psInst);
	SetBit(psMovInst->auFlag, INST_SKIPINV, GetBit(psInst->auFlag, INST_SKIPINV));

	if (psInst->asDest[uOtherDest].uType == USEASM_REGTYPE_FPINTERNAL &&
		psInst->asDest[uOtherDest].uNumber != 0 &&
		(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		/* Due to alignment requirements on the dest, need to make a VMOV instead of MOV */
		IMG_UINT32 uChan, uVectorSrc;

		SetOpcode(psState, psMovInst, IVMOV);

		uVectorSrc = uOtherDestSrc/(CHANS_PER_REGISTER+1) * (CHANS_PER_REGISTER+1);

		for (uChan = 0; uChan < CHANS_PER_REGISTER + 1; ++uChan)
		{
			CopySrc(psState, 
					psMovInst, 
					uChan, 
					psInst,
					uVectorSrc + uChan);
		}

		MoveDest(psState, psMovInst, 0 /* uMoveToDestIdx */, psInst, 0);

		psMovInst->auDestMask[0] = (1 << uOtherDest);
		psMovInst->auLiveChansInDest[0] = (1 << uOtherDest);
		psMovInst->u.psVec->aeSrcFmt[0] = psInst->asArg[uOtherDestSrc].eFmt;
		psMovInst->u.psVec->auSwizzle[0] = psInst->u.psVec->auSwizzle[uOtherDestSrc/(CHANS_PER_REGISTER+1)];
	}
	else
	{
		SetOpcode(psState, psMovInst, IMOV);

		/* Don't use MoveSrc here, because the usedef of the source may not have a format, leading to a crash. */
		SetSrc(psState, 
				psMovInst, 
				0 /* uMoveToSrcIdx */, 
				psInst->asArg[uOtherDestSrc].uType,
				psInst->asArg[uOtherDestSrc].uNumber, 
				psInst->asArg[uOtherDestSrc].eFmt);

		MoveDest(psState, psMovInst, 0 /* uMoveToDestIdx */, psInst, uOtherDest);
	}

	if (psInst->uPredCount > 0)
	{
		CopyPredicate(psState, psMovInst, psInst);
		CopyPartiallyWrittenDest(psState, psMovInst, 0, psInst, uOtherDest);
	}
	InsertInstAfter(psState, psInst->psBlock, psMovInst, psInst);
	
	#if defined(OUTPUT_USPBIN)
	/*
		Copy information about which instruction sources/destinations are shader results to
		the new instruction.
	*/
	if (IsInstSrcShaderResultRef(psState, psInst, uOtherDestSrc))
	{
		IMG_UINT32	uHWOperandsUsed = GetHWOperandsUsedForArg(psMovInst, 1 /* uArg */);
		psMovInst->uShaderResultHWOperands |= uHWOperandsUsed;
	}
	if (IsInstDestShaderResult(psState, psInst))
	{
		MarkShaderResultWrite(psState, psMovInst);
	}
	#endif /* defined(OUTPUT_USPBIN) */

	/*
		Drop the VPCK instruction.
	*/
	DropInst(psState, psRegState, psInst->psBlock, psInst);

	return psMovInst;
}

static
IMG_BOOL IsNullVMOV(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: IsNullVMOV

 PURPOSE	: Check for a vector move instruction where the destinations and
			  sources are exactly the same registers.

 PARAMETERS	: psState		- Compiler State
              psInst		- Instruction to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uDest;

	for (uDest = 0; uDest < psInst->uDestCount; uDest++)
	{
		if (psInst->auDestMask[uDest] == 0)
		{
			continue;
		}

		if (psInst->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F32)
		{
			USEASM_SWIZZLE_SEL	eSel;
			IMG_UINT32			uSrc;

			/*
				Get the source data swizzled into this channel of the destination.
			*/
			eSel = GetChan(psInst->u.psVec->auSwizzle[0], uDest);

			/*
				Fail if constant data is swizzled into this channel.
			*/
			if (!(eSel >= USEASM_SWIZZLE_SEL_X && eSel <= USEASM_SWIZZLE_SEL_W))
			{
				return IMG_FALSE;
			}

			/*
				Convert the source channel to the offset of a source argument.
			*/
			uSrc = 1 + eSel - USEASM_SWIZZLE_SEL_X;

			/*
				Check the source argument is exactly the same register as
				the destination.
			*/
			if (!EqualArgs(&psInst->asArg[uSrc], &psInst->asDest[uDest]))
			{
				return IMG_FALSE;
			}
		}
		else
		{
			IMG_UINT32	uChan;
			IMG_UINT32	uSrc;

			ASSERT(psInst->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F16);

			/*
				For each of the two 16-bit source channels corresponding to this 32-bit destination.
			*/
			uSrc = USC_UNDEF;
			for (uChan = 0; uChan < F16_CHANNELS_PER_SCALAR_REGISTER; uChan++)
			{
				IMG_UINT32	uChanByteMask = USC_XY_CHAN_MASK << (uChan * WORD_SIZE);

				if ((psInst->auDestMask[uDest] & uChanByteMask) != 0)
				{
					USEASM_SWIZZLE_SEL	eSel;
					IMG_UINT32			uChanSrc;
					IMG_UINT32			uOffsetInSrc;

					/*
						Get the source channel swizzled into this half of the destination.
					*/
					eSel = GetChan(psInst->u.psVec->auSwizzle[0], uDest * F16_CHANNELS_PER_SCALAR_REGISTER + uChan);

					/*
						Fail if the instruction is swizzling in constant data.
					*/
					if (!(eSel >= USEASM_SWIZZLE_SEL_X && eSel <= USEASM_SWIZZLE_SEL_W))
					{
						return IMG_FALSE;
					}

					/*
						Convert the source channel to a source argument.
					*/
					uChanSrc = 1 + (eSel - USEASM_SWIZZLE_SEL_X) / F16_CHANNELS_PER_SCALAR_REGISTER;

					/*
						Check both halves of the destination are copying data from the same 
						source argument.
					*/
					if (uSrc != USC_UNDEF && uSrc != uChanSrc)
					{
						return IMG_FALSE;
					}

					/*
						Check the 16-bit source channel and the 16-bit destination channel have the
						same alignment within a dword.
					*/
					uOffsetInSrc = (eSel - USEASM_SWIZZLE_SEL_X) % F16_CHANNELS_PER_SCALAR_REGISTER;
					if (uOffsetInSrc != uChan)
					{
						return IMG_FALSE;
					}
					
					/*
						Check the source argument and the destination argument are exactly the
						same register.
					*/
					if (!EqualArgs(&psInst->asArg[uChanSrc], &psInst->asDest[uDest]))
					{
						return IMG_FALSE;
					}

					uSrc = uChanSrc;
				}
			}
		}
	}

	return IMG_TRUE;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

#if defined(OUTPUT_USPBIN)
static
IMG_VOID RenameTempRegSet(PINTERMEDIATE_STATE	psState,
						  PRAGCOL_STATE			psRegState,
						  PARG					psBaseTempReg,
						  IMG_UINT32			uTempCount)
/*****************************************************************************
 FUNCTION	: RenameTempRegSet

 PURPOSE	: Rename the set of registers reserved at an instruction for
			  use by the USP when it expands the instruction.

 PARAMETERS	: psState		- Compiler State
              psRegState	- Register allocator state.
			  psBaseTempReg	- First intermediate register.
			  uTempCount	- Count of intermediate registers.

 RETURNS	: Nothing.
*****************************************************************************/
{
	#if defined(DEBUG)
	/*
		Check all the registers have been given consecutive register numbers.
	*/
	IMG_UINT32	uBaseNode = ArgumentToNode(&psRegState->sRAData, psBaseTempReg);
	PCOLOUR		psBaseColour = &psRegState->asNodes[uBaseNode].asColours[0];
	IMG_UINT32	uTempIdx;					

	for (uTempIdx = 0; uTempIdx < uTempCount; uTempIdx++)
	{
		PCOLOUR	psColour = &psRegState->asNodes[uBaseNode + uTempIdx].asColours[0];
		
		ASSERT(psColour->eType == psBaseColour->eType);
		ASSERT(psColour->uNum == (psBaseColour->uNum + uTempIdx));
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		Check the registers start at an even aligned register number.
	*/
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		ASSERT((psBaseColour->uNum % 2) == 0);
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	#else

	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(uTempCount);

	#endif /* defined(DEBUG) */

	RenameReg(psRegState, &psBaseTempReg->uType, &psBaseTempReg->uNumber, IMG_FALSE /* bUpdateTempCount */);
}
#endif /* defined(OUTPUT_USPBIN) */

static
IMG_VOID RenameRegistersBP(PINTERMEDIATE_STATE psState,
						   PCODEBLOCK psBlock,
						   IMG_PVOID pvRegState)
/*****************************************************************************
 FUNCTION	: RenameRegistersBP

 PURPOSE	: BLOCK_PROC, renames non-array registers in a single block,
		      based on the results of register allocation.

 PARAMETERS	: psState     - Compiler State
              psBlock     - Code block to process
              pvRegState  - PREGISTER_STATE of allocation state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PRAGCOL_STATE psRegState = (PRAGCOL_STATE)pvRegState;
	PINST psInst, psNextInst;

	for (psInst = psBlock->psBody; psInst != NULL; psInst = psNextInst)
	{
		IMG_UINT32	uArgCount = psInst->uArgumentCount;
		IMG_UINT32	uArgIdx;
		IMG_UINT32	uDestIdx;
		IMG_UINT32	uTempIdx;
		IMG_BOOL	bUpdateTempCount;

		/*
			For NOEMIT instructions don't include any temporary registers they use
			in the count reported for the program.
		*/
		bUpdateTempCount = GetBit(psInst->auFlag, INST_NOEMIT) ? IMG_FALSE : IMG_TRUE;

		psNextInst = psInst->psNext;

		/*
		  Rename the destination registers.
		*/
		for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
		{
			PARG		psDest = &psInst->asDest[uDestIdx];
			PARG		psOldDest = psInst->apsOldDest[uDestIdx];
			IMG_UINT32	uOrigDestType = psDest->uType;
			IMG_BOOL	bUpdateTempCountForDest, bRenamed;

			bUpdateTempCountForDest = bUpdateTempCount;
			#if defined(OUTPUT_USPBIN)		
			if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_USPTEXTURESAMPLE)
			{
				if (uDestIdx < UF_MAX_CHUNKS_PER_TEXTURE)
				{
					bUpdateTempCountForDest = IMG_FALSE;
				}
			}
			#endif /* defined(OUTPUT_USPBIN) */

			bRenamed = RenameArg(psRegState, psDest, bUpdateTempCountForDest);

			if (bRenamed && psDest->uType == USEASM_REGTYPE_FPINTERNAL)
			{
				SetDest(psState, psInst, uDestIdx, USEASM_REGTYPE_FPINTERNAL, psDest->uNumber, UF_REGFORMAT_F32);
			}

			if (psOldDest != NULL)
			{
				bRenamed = RenameArg(psRegState, psOldDest, bUpdateTempCount);
				
				if (bRenamed && psOldDest->uType == USEASM_REGTYPE_FPINTERNAL)
				{
					SetPartialDest(psState, psInst, uDestIdx, USEASM_REGTYPE_FPINTERNAL, psDest->uNumber, UF_REGFORMAT_F32);
				}
				ASSERT(EqualArgs(psOldDest, psDest));
			}
			else
			{
				/*
					Register allocation might have assigned the destination of this instruction to the
					same hardware register as another intermediate register live at the same point (providing
					the masks of channels live in each don't overlap). So we might need to preserve channels
					in a partially overwritten destination where we didn't before.
				*/
				if (
						uOrigDestType == USEASM_REGTYPE_TEMP && 
						psInst->asDest[uDestIdx].uType != USEASM_REGTYPE_FPINTERNAL &&
						(
							psInst->auDestMask[uDestIdx] != USC_ALL_CHAN_MASK || 
							!NoPredicate(psState, psInst)
						)
					)
				{
					SetPartiallyWrittenDest(psState, psInst, uDestIdx, psDest);
				}
			}
		}

		/*
		  Rename the source registers.
		*/
		for (uArgIdx = 0; uArgIdx < uArgCount; uArgIdx++)
		{
			PARG		psArg = &psInst->asArg[uArgIdx];
			IMG_BOOL	bUpdateTempCountForSource, bRenamed;

			if (!RegIsNode(&psRegState->sRAData, psArg))
			{
				continue;
			}

			bUpdateTempCountForSource = bUpdateTempCount;
			#if defined(OUTPUT_USPBIN)
			if (psInst->eOpcode == ISMPUNPACK_USP && uArgIdx < UF_MAX_CHUNKS_PER_TEXTURE)
			{
				bUpdateTempCountForSource = IMG_FALSE;
			}
			#endif /* defined(OUTPUT_USPBIN) */

			bRenamed = RenameArg(psRegState, psArg, bUpdateTempCountForSource);
			
			if (bRenamed && psArg->uType == USEASM_REGTYPE_FPINTERNAL)
			{
				SetSrc(psState, psInst, uArgIdx, USEASM_REGTYPE_FPINTERNAL, psArg->uNumber, UF_REGFORMAT_F32);
			}

			/*
			  The sources to a DSX/DSY instruction must be different
			  to the destination.
			*/
			#if defined(DEBUG)
			if (g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_DESTANDSRCOVERLAP)
			{
				IMG_UINT32	uDestIdxDbg;

				for (uDestIdxDbg = 0; uDestIdxDbg < psInst->uDestCount; uDestIdxDbg++)
				{
					PARG	psDest = &psInst->asDest[uDestIdxDbg];

					if (psInst->auDestMask[uDestIdxDbg] != 0)
					{
						ASSERT(!(psDest->uType == psArg->uType && psDest->uNumber == psArg->uNumber));
					}
				}
			}
			#endif /* defined(DEBUG) */
		}

		/*
			Rename reserved temporary registers.
		*/
		for (uTempIdx = 0; uTempIdx < psInst->uTempCount; uTempIdx++)
		{
			PSHORT_REG	psReservedTemp = &psInst->asTemp[uTempIdx];

			RenameReg(psRegState, &psReservedTemp->eType, &psReservedTemp->uNumber, bUpdateTempCount);
		}

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		/*
			Check for cases where an instruction source swizzle needs to be updated to make valid
			a group of instruction arguments which require consecutive hardware register numbers.
		*/
		FixConsecutiveRegisterSwizzlesInst(psState, psInst);
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

		/*
			Check groups of registers that need consecutive numbers.
		*/
		#if defined(DEBUG)
		CheckValidRegisterGroupsInst(psState, psInst);	
		#endif /* defined(DEBUG) */

		#if defined(OUTPUT_USPBIN)
		if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
		{
			/*
			  Rename the registers used for temporary data needed by the USP when generating
			  unpacking code for a texture sample.
			*/
			if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_USPTEXTURESAMPLE)
			{
				IMG_UINT32	uTempCount;

				uTempCount = GetUSPPerSampleTemporaryCount(psState, psInst->u.psSmp->uTextureStage, psInst);
				if (uTempCount > 0)
				{
					PARG		psUSPTempReg = &psInst->u.psSmp->sUSPSample.sTempReg;

					RenameTempRegSet(psState, psRegState, psUSPTempReg, uTempCount);
				}
			}
			/*
			  Rename the registers used for temporary data needed by the USP when generating
			  unpacking code for a texture sample unpack.
			*/
			if (psInst->eOpcode == ISMPUNPACK_USP)
			{
				IMG_UINT32	uTempCount;
				uTempCount = GetUSPPerSampleUnpackTemporaryCount();
				if (uTempCount > 0)
				{
					PARG		psUSPTempReg = &(psInst->u.psSmpUnpack->sTempReg);

					RenameTempRegSet(psState, psRegState, psUSPTempReg, uTempCount);
				}
			}
			/* Rename temporary registers used for texture writes */
			if (psInst->eOpcode == ITEXWRITE)
			{
				IMG_UINT32	uTempCount;
				PARG		psUSPTempReg;

				uTempCount = GetUSPTextureWriteTemporaryCount();
				psUSPTempReg = &(psInst->u.psTexWrite->sTempReg);

				if (uTempCount > 0)
				{
					RenameTempRegSet(psState, psRegState, psUSPTempReg, uTempCount);
				}
			}
		}
		#endif /* defined(OUTPUT_USPBIN) */

		/*
		  For move instructions where the same register was allocated to
		  both the source and destination get rid of the instruction.
		*/
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (
				psInst->eOpcode == IVMOV &&
				psInst->u.psVec->aeSrcFmt[0] == psInst->asDest[0].eFmt &&
				!psInst->u.psVec->asSrcMod[0].bNegate &&
				!psInst->u.psVec->asSrcMod[0].bAbs &&
				IsNullVMOV(psState, psInst)
		   )
		{
			DropInst(psState, psRegState, psBlock, psInst);
			continue;
		}

		if (psInst->eOpcode == IVPCKFLTFLT_EXP)
		{
			IMG_UINT32	uDest;

			for (uDest = 0; uDest < psInst->uDestCount; uDest++)
			{
				if (psInst->auDestMask[uDest] != 0)
				{
					IMG_UINT32	uDestSrc;
	
					uDestSrc = GetVPCKFLTFLT_EXPSourceOffset(psState, psInst, uDest);
					if (EqualArgs(&psInst->asDest[uDest], &psInst->asArg[uDestSrc]))
					{
						PINST psNewInst = ConvertVPCKFLTFLTToMove(psState, psRegState, psInst, 1 - uDest);

						if (psNewInst != NULL)
						{
							psInst = psNewInst;
						}
						break;
					}
				}
			}
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

		if (
				(
					psInst->eOpcode == IMOV ||
					psInst->eOpcode == ISETL
				) &&
				EqualArgs(&psInst->asDest[0], &psInst->asArg[0])
		    )
		{
			DropInst(psState, psRegState, psBlock, psInst);
			continue;
		}
	}
}

static
IMG_BOOL IsCoalescableRegister(PRAGCOL_STATE		psRegState,
							   PARG					psArg)
/*****************************************************************************
 FUNCTION	: IsCoalescableRegister

 PURPOSE	: Check if a register is one we can coalesce.

 PARAMETERS	: psState			- Compiler state.
			  psArg				- Register to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (
			RegIsNode(&psRegState->sRAData, psArg) &&
			psArg->uType != USC_REGTYPE_REGARRAY &&
			psArg->uIndexType == USC_REGTYPE_NOINDEX
		)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static
IMG_VOID CoalesceFixedTypeVariable(PINTERMEDIATE_STATE	psState,
								   PREGISTER_GROUP		psRenameToNode,
								   PREGISTER_GROUP		psRenameFromNode)
/*****************************************************************************
 FUNCTION	: CoalesceFixedTypeVariable

 PURPOSE	: Coalesce a variable which is precoloured to a fixed type but with
			  any register number with another variable which precoloured to a
			  fixed type and number.

 PARAMETERS	: psState			- Compiler state.
			  psRenameToNode	- The variables to coalesce.
			  psRenameFromNode	

 RETURNS	: Nothing.
*****************************************************************************/
{
	PFIXED_REG_DATA	psRenameFromFixedReg;
	IMG_UINT32		uRenameFromFixedRegOffset;
	PFIXED_REG_DATA	psRenameToFixedReg;
	IMG_UINT32		uRenameToFixedRegOffset;
	IMG_UINT32		uPRegNum;

	psRenameFromFixedReg = psRenameFromNode->psFixedReg;
	uRenameFromFixedRegOffset = psRenameFromNode->uFixedRegOffset;
	
	/*
		Get the intermediate register in the renamed-to group corresponding to start of the
		fixed register containing the rename-from intermediate register.
	*/
	ASSERT(IsPrecolouredNode(psRenameToNode));
	while (uRenameFromFixedRegOffset > 0)
	{
		psRenameToNode = psRenameToNode->psPrev;
		uRenameFromFixedRegOffset--;
	}
	
	/*
		Get the physical register for the start of the fixed register,
	*/
	ASSERT(IsPrecolouredNode(psRenameToNode));
	psRenameToFixedReg = psRenameToNode->psFixedReg;
	uRenameToFixedRegOffset = psRenameToNode->uFixedRegOffset;
	ASSERT(psRenameToFixedReg->sPReg.uType == psRenameFromFixedReg->sPReg.uType);
	ASSERT(psRenameToFixedReg->sPReg.uNumber != USC_UNDEF);

	/*
		Copy the coalesced register over the shader output.
	*/
	uPRegNum = psRenameToFixedReg->sPReg.uNumber + uRenameToFixedRegOffset;

	if (psRenameFromFixedReg->sPReg.uNumber == USC_UNDEF)
	{
		psRenameFromFixedReg->sPReg.uType = psRenameToFixedReg->sPReg.uType;
		psRenameFromFixedReg->sPReg.uNumber = uPRegNum;
	}
	else
	{
		ASSERT(psRenameFromFixedReg->sPReg.uType == psRenameToFixedReg->sPReg.uType);
		ASSERT(psRenameFromFixedReg->sPReg.uNumber == uPRegNum);
	}
}

static
IMG_VOID CoalesceGroupElement(PINTERMEDIATE_STATE	psState,
							  PRAGCOL_STATE			psRegState,
							  IMG_UINT32			uRenameToGroupNode,
							  PREGISTER_GROUP		psRenameToGroupNode,
							  IMG_UINT32			uRenameFromGroupNode,
							  PREGISTER_GROUP		psRenameFromGroupNode,
							  IMG_BOOL				bRenameFromIsPrecoloured,
							  IMG_BOOL				bToNodeShaderResult,
							  IMG_BOOL				bFromNodeShaderResult)
/*****************************************************************************
 FUNCTION	: CoalesceGroupElement

 PURPOSE	: Coalesce an element from two groups of registers.

 PARAMETERS	: psState			- Compiler state.
			  psRegState		- Module state.
			  uRenameToNode		- Node in the first group to coalesce.
			  psRenameToGroupNode
			  uRenameFromNode	- Node in the second group to coalesce.
			  psRenameFromGroupNode
			  bRenameFromIsPrecoloured
								- TRUE if the first node is precoloured.
			  bToNodeShaderResult
								- TRUE if the first node is a shader result.
			  bFromNodeShaderResult
								- TRUE if the second node is a shader result.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PNODE_DATA	psRenameTo = &psRegState->asNodes[uRenameToGroupNode];
	PNODE_DATA	psRenameFrom = &psRegState->asNodes[uRenameFromGroupNode];

	/*
		If coalescing the register which containing the shader
		result update the reference to it in the state.
	*/
	if (bRenameFromIsPrecoloured)
	{
		CoalesceFixedTypeVariable(psState, psRenameToGroupNode, psRenameFromGroupNode);
	}

	/*
		If the renamed-from register will have multiple colours then allocate space
		for multiple colours for the renamed-to register.
	*/
	if (psRenameFrom->uColourCount > 1)
	{
		ASSERT(psRenameTo->uColourCount == 1);
		ASSERT(psRenameTo->asColours == &psRenameTo->sSingleColour);

		psRenameTo->uColourCount = psRenameFrom->uColourCount;
		psRenameTo->asColours = psRenameFrom->asColours;

		psRenameFrom->uColourCount = 1;
		psRenameFrom->asColours = &psRenameFrom->sSingleColour;
	}

	/*
		Copy information about the requirements of instructions which have the second node
		as an argument.
	*/
	if (psRenameFromGroupNode != NULL && psRenameFromGroupNode->bLinkedByInst)
	{
		psRenameToGroupNode->bLinkedByInst = IMG_TRUE;
	}

	/*
		Copy information about the coalesced register.
	*/
	if (GetBit(psRenameFrom->auFlags, NODE_FLAG_ALPHALIVE))
	{
		SetBit(psRenameTo->auFlags, NODE_FLAG_ALPHALIVE, IMG_TRUE);
	}
	if (GetBit(psRenameFrom->auFlags, NODE_FLAG_SPLITLIVE))
	{
		SetBit(psRenameTo->auFlags, NODE_FLAG_SPLITLIVE, IMG_TRUE);
	}
	if (GetBit(psRenameFrom->auFlags, NODE_FLAG_REFERENCED))
	{
		SetBit(psRenameTo->auFlags, NODE_FLAG_REFERENCED, IMG_TRUE);
	}
	if (GetBit(psRenameFrom->auFlags, NODE_FLAG_POSTSPLITPHASE))
	{
		SetBit(psRenameTo->auFlags, NODE_FLAG_POSTSPLITPHASE, IMG_TRUE);
	}

	/*
		Merge information about the possible hardware register banks.
	*/
	psRenameTo->uBankFlags &= psRenameFrom->uBankFlags;

	/*
		Coalesce the registers from each group.
	*/
	CoalesceRegister(psState, 
					 psRegState, 
					 uRenameToGroupNode, 
					 uRenameFromGroupNode, 
					 bToNodeShaderResult, 
					 bFromNodeShaderResult);
}

static
IMG_VOID CoalesceGroups(PINTERMEDIATE_STATE	psState,
					    PRAGCOL_STATE		psRegState,
					    IMG_UINT32			uRenameToNode,
						IMG_UINT32			uRenameFromNode,
						IMG_BOOL			bToNodeShaderResult,
						IMG_BOOL			bFromNodeShaderResult)
/*****************************************************************************
 FUNCTION	: CoalesceGroups

 PURPOSE	: Coalesce two groups of registers.

 PARAMETERS	: psState			- Compiler state.
			  psRegState		- Module state.
			  uRenameToNode		- Node in the first group to coalesce.
			  uRenameFromNode	- Node in the second group to coalesce.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PREGISTER_GROUP	psRenameToNode, psRenameFromNode;
	PREGISTER_GROUP	psRenameToFirstNode, psRenameFromFirstNode;
	PREGISTER_GROUP	psRenameFromGroupNode, psRenameToGroupNode;
	HWREG_ALIGNMENT	eRenameFromAlignment;
	IMG_BOOL		bRenameFromIsPrecoloured;
	IMG_BOOL		bRet;

	psRenameToNode = GetRegisterGroup(&psRegState->sRAData, uRenameToNode);
	psRenameFromNode = GetRegisterGroup(&psRegState->sRAData, uRenameFromNode);

	/*
		If the renamed-to node doesn't already have information to track register
		groups then create it.
	*/
	if (psRenameFromNode != NULL && psRenameToNode == NULL)
	{
		psRenameToNode = MakeRegisterGroup(&psRegState->sRAData, uRenameToNode);
	}

	bRenameFromIsPrecoloured = IsPrecolouredNode(psRenameFromNode);

	/*
		Copy the alignment flag from the source node to the
		destination node.
	*/
	eRenameFromAlignment = GetNodeAlignment(psRenameFromNode);
	if (eRenameFromAlignment != HWREG_ALIGNMENT_NONE)
	{
		SetNodeAlignment(psState, psRenameToNode, eRenameFromAlignment, psRenameFromNode->bAlignRequiredByInst);
	}

	/*
		Coalesce the nodes.
	*/
	CoalesceGroupElement(psState, 
						 psRegState, 
						 uRenameToNode, 
						 psRenameToNode, 
						 uRenameFromNode, 
						 psRenameFromNode,
						 bRenameFromIsPrecoloured,
						 bToNodeShaderResult,
						 bFromNodeShaderResult);

	/*
		Exit if the renamed-from node has no restrictions on its hardware register number.
	*/
	if (psRenameFromNode == NULL)
	{
		return;
	}

	/*
		Create the data structure so hardware register number restrictions can be added for
		the renamed-to node.
	*/
	psRenameToNode = MakeRegisterGroup(&psRegState->sRAData, uRenameToNode);

	/*
		Seek backwards in both of the groups we are coalescing until we reach the end of one of
		them.
	*/
	psRenameToGroupNode = psRenameToNode;
	psRenameFromGroupNode = psRenameFromNode;
	do
	{
		psRenameToFirstNode = psRenameToGroupNode;
		psRenameFromFirstNode = psRenameFromGroupNode;

		psRenameToGroupNode = psRenameToGroupNode->psPrev;
		psRenameFromGroupNode = psRenameFromGroupNode->psPrev;
	} while (psRenameToGroupNode != NULL && psRenameFromGroupNode != NULL);

	/*
		Check if the renamed-from group is larger than the renamed-to group.
	*/
	if (psRenameFromGroupNode != NULL)
	{
		IMG_BOOL	bLinkedByInst;

		bLinkedByInst = psRenameFromGroupNode->bLinkedByInst;

		/*
			Drop the link between the uncoalesced portion and the rest.
		*/
		DropLinkAfterNode(psState, psRenameFromGroupNode);

		/*
			Link the uncoalesced prefix with the group we are coalescing with.
		*/
		bRet = AddToGroup(psState,
						  psRenameFromGroupNode->uRegister,
						  psRenameFromGroupNode,
						  psRenameToFirstNode->uRegister,
						  psRenameToFirstNode,
						  bLinkedByInst,
						  IMG_FALSE /* bOptional */);
		ASSERT(bRet);
	}

	psRenameToGroupNode = psRenameToFirstNode;
	psRenameFromGroupNode = psRenameFromFirstNode;
	for (;;)
	{
		PREGISTER_GROUP	psRenameToNextGroupNode;
		PREGISTER_GROUP	psRenameFromNextGroupNode;
		IMG_BOOL		bLinkedByInst;
		IMG_BOOL		bLastCoalesce;
		IMG_BOOL		bOptional;

		psRenameFromNextGroupNode = psRenameFromGroupNode->psNext;
		psRenameToNextGroupNode = psRenameToGroupNode->psNext;

		bLastCoalesce = IMG_FALSE;
		if (psRenameFromNextGroupNode == NULL || psRenameToNextGroupNode == NULL)
		{
			bLastCoalesce = IMG_TRUE;
		}

		/*
			Save information about the linked nodes in the renamed-from group.
		*/
		bLinkedByInst = psRenameFromGroupNode->bLinkedByInst;
		bOptional = psRenameFromGroupNode->bOptional;

		/*
			Drop the coalesced-from node from the group.
		*/
		RemoveFromGroup(psState, psRenameFromGroupNode);

		if (psRenameToNextGroupNode == NULL)
		{
			if (psRenameFromNextGroupNode != NULL)
			{
				/*
					Link any uncoalesced suffix with the group we are coalescing with.
				*/
				bRet = AddToGroup(psState,
								  psRenameToGroupNode->uRegister,
								  psRenameToGroupNode,
								  psRenameFromNextGroupNode->uRegister,
								  psRenameFromNextGroupNode,
								  bLinkedByInst,
								  IMG_FALSE /* bOptional */);
				ASSERT(bRet);
			}
		}
		else
		{
			if (bLinkedByInst)
			{
				psRenameToGroupNode->bLinkedByInst = IMG_TRUE;
			}
			if (!bOptional)
			{
				psRenameToGroupNode->bOptional = IMG_FALSE;
			}
		}

		/*
			Replace the group element in every instruction.
		*/
		if (psRenameFromGroupNode != psRenameFromNode)
		{
			IMG_UINT32	uRenameToGroupNode;
			IMG_UINT32	uRenameFromGroupNode;

			uRenameToGroupNode = 
				RegisterToNode(&psRegState->sRAData, USEASM_REGTYPE_TEMP, psRenameToGroupNode->uRegister);
			uRenameFromGroupNode = 
				RegisterToNode(&psRegState->sRAData, USEASM_REGTYPE_TEMP, psRenameFromGroupNode->uRegister);
	
			CoalesceGroupElement(psState, 
								 psRegState, 
								 uRenameToGroupNode, 
								 psRenameToGroupNode, 
								 uRenameFromGroupNode, 
								 psRenameFromGroupNode,
								 bRenameFromIsPrecoloured,
								 bToNodeShaderResult,
								 bFromNodeShaderResult);
		}

		if (bLastCoalesce)
		{
			break;
		}

		psRenameFromGroupNode = psRenameFromNextGroupNode;
		psRenameToGroupNode = psRenameToNextGroupNode;
	}
}

static
IMG_BOOL AreNodesCoalesceable(PRAGCOL_STATE		psRegState,
						      IMG_UINT32		uSrcNode,
						      IMG_UINT32		uDestNode)
/*****************************************************************************
 FUNCTION	: AreNodesCoalesceable

 PURPOSE	: Check if two intermediate registers can be coalesced.

 PARAMETERS	: psRegState		- Module state.
			  uSrcNode			- The two nodes to check.
			  uDestNode

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	/*
		Check the two nodes don't interfere (aren't simultaneously live).
	*/
	if (IntfGraphGet(psRegState->psIntfGraph, uSrcNode, uDestNode))
	{
		return IMG_FALSE;
	}

	/*
		Check at least one hardware register bank is available for both nodes.
	*/
	if ((psRegState->asNodes[uSrcNode].uBankFlags & psRegState->asNodes[uDestNode].uBankFlags) == 0)
	{
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

static
IMG_BOOL CanCoalesceGroups(PINTERMEDIATE_STATE	psState,
						   PRAGCOL_STATE		psRegState,
						   IMG_UINT32			uSrcNode,
						   IMG_UINT32			uDestNode,
						   IMG_PBOOL			pbRenameSrc)
/*****************************************************************************
 FUNCTION	: CanCoalesceGroups

 PURPOSE	: Check if two groups of registers can be coalesced.

 PARAMETERS	: psState			- Compiler state.
			  psRegState		- Module state.
			  uSrcNode			- The two nodes to check.
			  uDestNode
			  pbRenameSrc		- Return TRUE if the coalesce should replace
								the source register.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{

	DIRECTION		eDir;
	IMG_UINT32		uNewGroupLength;
	PREGISTER_GROUP	psSrcNode, psDestNode;
	IMG_BOOL		bSrcNodePrecoloured, bDestNodePrecoloured;
	IMG_BOOL		bDestNodeIsPrecolouredToTypeOnly;

	psSrcNode = GetRegisterGroup(&psRegState->sRAData, uSrcNode);
	psDestNode = GetRegisterGroup(&psRegState->sRAData, uDestNode);

	bSrcNodePrecoloured = IsPrecolouredNode(psSrcNode);
	bDestNodePrecoloured = IsPrecolouredNode(psDestNode);
	bDestNodeIsPrecolouredToTypeOnly = IMG_FALSE;
	if (bSrcNodePrecoloured && bDestNodePrecoloured)
	{
		IMG_UINT32	uDestNodeColourType, uDestNodeColourNum;

		GetFixedColour(psState, &psRegState->sRAData, uDestNode, &uDestNodeColourType, &uDestNodeColourNum);

		/*
			Check for the destination node having a fixed hardware register type but 
			with any hardware register number.
		*/
		if (uDestNodeColourNum == USC_UNDEF)
		{
			IMG_UINT32	uSrcNodeColourType, uSrcNodeColourNum;

			/*
				As a special case allow this variable to be coalesced
				with any completely fixed register of the same type.
			*/
			GetFixedColour(psState, &psRegState->sRAData, uSrcNode, &uSrcNodeColourType, &uSrcNodeColourNum);
			if (uSrcNodeColourType != uDestNodeColourType || 
				uSrcNodeColourNum == USC_UNDEF ||
				IsNonTemporaryNode(&psRegState->sRAData, uSrcNode))
			{
				return IMG_FALSE;
			}

			bDestNodeIsPrecolouredToTypeOnly = IMG_TRUE;

			/*
				Keep the completely fixed register.
			*/
			*pbRenameSrc = IMG_FALSE;
		}
		else
		{
			/*
				Otherwise don't try to coalesce two precoloured nodes.
			*/
			return IMG_FALSE;
		}
	}
	else
	{
		/*
			Rename the non-precoloured register.
		*/
		*pbRenameSrc = bDestNodePrecoloured;
	}

	/*
		Check if the source has to be given a register number either
		always lower or always higher than the destination.
	*/
	if (IsAfter(psSrcNode, psDestNode) || IsAfter(psDestNode, psSrcNode))
	{
		return IMG_FALSE;
	}

	/*
		If either of the nodes requires an even aligned register number check if that's
		compatible with the other node.
	*/
	if (!CanCoalesceNodeWithQwordAlignedNode(psDestNode, psSrcNode))
	{
		return IMG_FALSE;
	}

	/*
		Check the two nodes don't interfere.
	*/
	if (!AreNodesCoalesceable(psRegState, uSrcNode, uDestNode))
	{
		return IMG_FALSE;
	}

	/*
		Check the groups in two steps: first the part after each node
		then the part before.
	*/
	for (eDir = DIRECTION_FORWARD; eDir <= DIRECTION_BACKWARD; eDir++)
	{
		PREGISTER_GROUP	psSrcGroupNode;
		PREGISTER_GROUP	psDestGroupNode;

		psSrcGroupNode  = GetConsecutiveGroup(psSrcNode,  eDir);
		psDestGroupNode = GetConsecutiveGroup(psDestNode, eDir);

		while (psSrcGroupNode != NULL && psDestGroupNode != NULL)
		{
			IMG_UINT32	uSrcGroupNode, uDestGroupNode;

			uSrcGroupNode = RegisterToNode(&psRegState->sRAData, USEASM_REGTYPE_TEMP, psSrcGroupNode->uRegister);
			uDestGroupNode = RegisterToNode(&psRegState->sRAData, USEASM_REGTYPE_TEMP, psDestGroupNode->uRegister);

			/*
				Check the two nodes don't interfere.
			*/
			if (!AreNodesCoalesceable(psRegState, uSrcGroupNode, uDestGroupNode))
			{
				return IMG_FALSE;
			}

			/*
				Move onto the next node in the group.
			*/
			switch (eDir)
			{
				case DIRECTION_FORWARD:
				{
					psSrcGroupNode = psSrcGroupNode->psNext;
					psDestGroupNode = psDestGroupNode->psNext;
					break;
				}
				case DIRECTION_BACKWARD:
				{
					psSrcGroupNode = psSrcGroupNode->psPrev;
					psDestGroupNode = psDestGroupNode->psPrev;
					break;
				}
			}
		}

		/*
			If either group has a part we aren't coalescing.
		*/
		if (psSrcGroupNode != NULL || psDestGroupNode != NULL)
		{
			/*
				Only coalesce a group of registers with another precoloured group if the groups
				are the same length. Requiring that a non-precoloured register has a register
				number lower or higher than a precoloured register might not be possible if (for example)
				the precoloured register's fixed colour was the first register in a particular
				bank.
			*/
			if (psSrcGroupNode != NULL)
			{
				if (bDestNodePrecoloured && !bDestNodeIsPrecolouredToTypeOnly)
				{
					return IMG_FALSE;
				}
			}
			else
			{
				if (bSrcNodePrecoloured)
				{
					return IMG_FALSE;
				}
			}
		}
	}

	/*
		Calculate the length of the group formed if we did the coalesce.
	*/
	uNewGroupLength = max(GetNextNodeCount(psSrcNode), GetNextNodeCount(psDestNode));
	uNewGroupLength += max(GetPrevNodeCount(psSrcNode), GetPrevNodeCount(psDestNode));
	uNewGroupLength++;
	/*
		Don't coalesce if the new group is too large.
	*/
	if (uNewGroupLength > psRegState->sRAData.uMaximumRangeLength)
	{
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

static
IMG_BOOL IsMOV(PINST	psInst)
/*****************************************************************************
 FUNCTION	: IsMOV

 PURPOSE	: Check for an instruction equivalent to a move.

 PARAMETERS	: psInst	- Instruction to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (psInst->eOpcode == IMOV)
	{
		return IMG_TRUE;
	}
	/*
		Check for a PCKU8U8/PCKC10C10 without a swizzle or mask.
	*/
	if ((psInst->eOpcode == IPCKU8U8 || psInst->eOpcode == IPCKC10C10) &&
		g_abSingleBitSet[psInst->auDestMask[0]] &&
		g_aiSingleComponent[psInst->auDestMask[0]] == (IMG_INT32)psInst->u.psPck->auComponent[0])
	{
		return IMG_TRUE;
	}
	/*
		Check for a SOPWM copying its first source argument.
	*/
	if (IsSOPWMMove(psInst) && psInst->asDest[0].eFmt == psInst->asArg[0].eFmt)
	{
		return IMG_TRUE;
	}
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psInst->eOpcode == IVPCKU8U8)
	{
		if (CompareSwizzles(psInst->u.psVec->auSwizzle[0], USEASM_SWIZZLE(X, Y, Z, W), psInst->auDestMask[0]))
		{
			return IMG_TRUE;
		}
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	return IMG_FALSE;
}

static
IMG_BOOL TryCoalesceArguments(PINTERMEDIATE_STATE	psState,
							  PRAGCOL_STATE			psRegState,
							  #if defined(OUTPUT_USPBIN)
							  IMG_BOOL				bDestIsShaderResult,
							  IMG_BOOL				bSrcIsShaderResult,
							  #endif /* defined(OUTPUT_USPBIN) */
							  PARG					psDest,
							  PARG					psSrc)
/*****************************************************************************
 FUNCTION	: TryCoalesceRegisters

 PURPOSE	: Try to remove MOV instructions by coalescing registers.

 PARAMETERS	: psState			- Compiler state.
              psRegState		- Register allocator state.
			  psInst			- MOV instruction.
			  psDest			- Destination to coalesce.
			  psSrc				- Source to coalesce.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (
			IsCoalescableRegister(psRegState, psDest) &&
			IsCoalescableRegister(psRegState, psSrc)
	   )
	{
		IMG_UINT32	uDestNode = ArgumentToNode(&psRegState->sRAData, psDest);
		IMG_UINT32	uSrcNode = ArgumentToNode(&psRegState->sRAData, psSrc);
		IMG_UINT32	uRenameToNode, uRenameFromNode;
		IMG_BOOL	bToNodeShaderResult = IMG_FALSE;
		IMG_BOOL	bFromNodeShaderResult = IMG_FALSE;
		IMG_BOOL	bRenameSrc;

		/*
			The nodes are already the same so we can just drop the MOV.
		*/
		if (uDestNode == uSrcNode)
		{
			return IMG_TRUE;
		}

		/*
			Check the nodes (or the group they belong to) don't interfere and
			coalescing them won't increase register pressure too much.
		*/
		if (!CanCoalesceGroups(psState, psRegState, uSrcNode, uDestNode, &bRenameSrc))
		{
			return IMG_FALSE;
		}

		/*
			Don't rename a precoloured node to something else.
		*/
		if (bRenameSrc)
		{
			uRenameToNode = uDestNode;
			uRenameFromNode = uSrcNode;

			#if defined(OUTPUT_USPBIN)
			if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
			{
				bToNodeShaderResult = bDestIsShaderResult;
				bFromNodeShaderResult = bSrcIsShaderResult;
			}
			#endif /* defined(OUTPUT_USPBIN) */
		}
		else
		{
			uRenameToNode = uSrcNode;
			uRenameFromNode = uDestNode;

			#if defined(OUTPUT_USPBIN)
			if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
			{
				bToNodeShaderResult = bSrcIsShaderResult;
				bFromNodeShaderResult = bDestIsShaderResult;
			}
			#endif /* defined(OUTPUT_USPBIN) */
		}

		/*
			Do the coalesce.
		*/
		CoalesceGroups(psState, psRegState, uRenameToNode, uRenameFromNode, bToNodeShaderResult, bFromNodeShaderResult);

		return IMG_TRUE;
	}

	return IMG_FALSE;
}

static
IMG_BOOL TryCoalesceRegisters(PINTERMEDIATE_STATE	psState,
							  PRAGCOL_STATE			psRegState,
							  PINST					psInst,
							  IMG_UINT32			uDestIdx,
							  IMG_UINT32			uSrcIdx)
/*****************************************************************************
 FUNCTION	: TryCoalesceRegisters

 PURPOSE	: Try to remove MOV instructions by coalescing registers.

 PARAMETERS	: psState			- Compiler state.
              psRegState		- Register allocator state.
			  psInst			- MOV instruction.
			  uDestIdx			- Destination to coalesce.
			  uSrcIdx			- Source to coalesce.

 RETURNS	: Nothing.
*****************************************************************************/
{
	#if defined(OUTPUT_USPBIN)
	IMG_BOOL	bDestIsShaderResult;
	IMG_BOOL	bSrcIsShaderResult;

	bDestIsShaderResult = IsInstDestShaderResult(psState, psInst);
	bSrcIsShaderResult = IsInstSrcShaderResultRef(psState, psInst, uSrcIdx);
	#endif /* defined(OUTPUT_USPBIN) */

	return TryCoalesceArguments(psState,
								psRegState,
								#if defined(OUTPUT_USPBIN)
								bDestIsShaderResult,
								bSrcIsShaderResult,
								#endif /* defined(OUTPUT_USPBIN) */
								&psInst->asDest[uDestIdx],
								&psInst->asArg[uSrcIdx]);
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_BOOL TryCoalesceVPCKFLTFLT(PINTERMEDIATE_STATE	psState,
							   PRAGCOL_STATE		psRegState,
							   PINST				psInst,
							   PINST*				ppsNextInst)
/*****************************************************************************
 FUNCTION	: TryCoalesceVPCKFLTFLT

 PURPOSE	: For a vector copy between two independent pairs of scalar
			  destinations/sources try to coalesce one of the pairs.

 PARAMETERS	: psState			- Compiler state.
			  psRegState		- Register allocator state.
              psInst			- Instruction.
			  ppsNextInst		- If the instruction was removed then
								returns another instruction which does the
								part of the copy which couldn't be coalesced.

 RETURNS	: TRUE if the instruction was coalesced.
*****************************************************************************/
{
	IMG_UINT32	uDest;

	for (uDest = 0; uDest < psInst->uDestCount; uDest++)
	{
		IMG_UINT32	uDestSrc;

		/*
			Get the index of the source argument which is copied to this destination.
		*/
		uDestSrc = GetVPCKFLTFLT_EXPSourceOffset(psState, psInst, uDest);

		/*
			Can the first destination be coalesced?
		*/
		if (TryCoalesceRegisters(psState, psRegState, psInst, uDest, uDestSrc))
		{
			if (psInst->uDestCount == 2)
			{
				IMG_UINT32	uOtherDest = 1 - uDest;
				PINST		psMovInst;
			
				/*
					Convert the instruction to a simple move.
				*/
				psMovInst = ConvertVPCKFLTFLTToMove(psState, psRegState, psInst, uOtherDest);

				/*
					Consider the generated MOV instruction for coalescing next.
				*/
				if (psMovInst != NULL)
				{
					*ppsNextInst = psMovInst;
				}

				return IMG_TRUE;
			}
		}				
	}

	return IMG_FALSE;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

typedef struct _COALESCING_ARG
{
	PRAGCOL_STATE	psRegState;
	IMG_BOOL		bRemovedInst;
} COALESCING_ARG;

static
IMG_VOID CoalesceRegistersForMOVsBP(PINTERMEDIATE_STATE	psState,
									PCODEBLOCK psBlock,
									IMG_PVOID pvCoalesceArg)
/*****************************************************************************
 FUNCTION	: CoaleseRegistersForMOVs

 PURPOSE	: Try to remove MOV instructions by coalescing registers.

 PARAMETERS	: psState			- Compiler state.
              psBlock           - Block in which to look for MOV instructions.
			  pvRegState		- PREGISTER_STATE to Register allocator state.
								  IF an instruction was removed, this is set to NULL.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PRAGCOL_STATE	psRegState = ((COALESCING_ARG*)pvCoalesceArg)->psRegState;
	PINST			psInst, psNextInst;
	IMG_BOOL		bRemovedInst = IMG_FALSE;

	for (psInst = psBlock->psBody; psInst; psInst = psNextInst)
	{
		psNextInst = psInst->psNext;

		/*
			Try to convert a SOP2 moving different data into the RGB and ALPHA channels into
			a simple move if we can coalesce the two sources.
		*/
		if (IsSOP2DoubleMove(psState, psInst))
		{
			IMG_UINT32	uRGBSrc, uAlphaSrc;

			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			if (psInst->eOpcode == ISOP2_VEC)
			{
				uRGBSrc = 0;
				uAlphaSrc = 2;
			}
			else
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
			{
				uRGBSrc =0;
				uAlphaSrc = 1;
			}
			
			if (
					psInst->asArg[uRGBSrc].eFmt == psInst->asArg[uAlphaSrc].eFmt &&
					psInst->asArg[uRGBSrc].eFmt == psInst->asDest[0].eFmt
			   )
			{
				if (TryCoalesceArguments(psState,
										 psRegState,
										 #if defined(OUTPUT_USPBIN)	
										 IsInstSrcShaderResultRef(psState, psInst, uRGBSrc /* uSrcIdx */),
										 IsInstSrcShaderResultRef(psState, psInst, uAlphaSrc /* uSrcIdx */),
										 #endif /* defined(OUTPUT_USPBIN) */
										 &psInst->asArg[uRGBSrc],
										 &psInst->asArg[uAlphaSrc]))
				{
					SetOpcode(psState, psInst, IMOV);
				}
			}
		}

		/*
			Look for an unpredicated MOV between two registers we can coalesce.
		*/
		if (IsMOV(psInst))
		{
			if (TryCoalesceRegisters(psState, psRegState, psInst, 0 /* uDestIdx */, 0 /* uSrcIdx */))
			{
				DropInst(psState, psRegState, psBlock, psInst);
				bRemovedInst = IMG_TRUE;
				continue;
			}
		}

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		/*
			Try to coalesce for a vector MOV instruction using the identity
			swizzle. 
		*/
		if (
				psInst->eOpcode == IVMOV &&
				NoPredicate(psState, psInst) &&
				!psInst->u.psVec->asSrcMod[0].bNegate &&
				!psInst->u.psVec->asSrcMod[0].bAbs &&
				psInst->asDest[0].eFmt == psInst->u.psVec->aeSrcFmt[0]
		   )
		{
			IMG_UINT32	uDest;
			IMG_UINT32	uFirstDestSrc;
			IMG_UINT32	uFirstDest;
			IMG_BOOL	bInvalidSwizzle;

			uFirstDest = uFirstDestSrc = USC_UNDEF;
			bInvalidSwizzle = IMG_FALSE;
			for (uDest = 0; uDest < psInst->uDestCount; uDest++)
			{
				IMG_UINT32	uDestSrc;

				if (psInst->auDestMask[uDest] != 0)
				{
					uDestSrc = GetVectorCopySourceArgument(psState, psInst, uDest);
					if (uDestSrc == USC_UNDEF)
					{
						bInvalidSwizzle = IMG_TRUE;
						break;
					}

					if (uFirstDest == USC_UNDEF)
					{
						uFirstDest = uDest;
						uFirstDestSrc = uDestSrc;
					}
					else
					{
						if ((uFirstDestSrc + uDest) != uDestSrc)
						{
							bInvalidSwizzle = IMG_TRUE;
							break;
						}
					}
				}
			}

			if (!bInvalidSwizzle)
			{
				if (TryCoalesceRegisters(psState, psRegState, psInst, uFirstDest, uFirstDestSrc))
				{
					DropInst(psState, psRegState, psBlock, psInst);
					bRemovedInst = IMG_TRUE;
					continue;
				}
			}
		}

		if (psInst->eOpcode == IVPCKFLTFLT_EXP && psInst->uDestCount <= 2)
		{
			if (TryCoalesceVPCKFLTFLT(psState, psRegState, psInst, &psNextInst))
			{
				continue;
			}
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		
		#if defined(OUTPUT_USPBIN)
		if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
		{
			/*
				Try to coalesce the destination of a non-dependent USP sample with
				the source representing the packed texture sample. We can't remove
				the instruction but this helps the USP to avoid inserting a MOV
				where no texture unpacking is required since the destination for
				the packed texture sample and the destination for the unpacked result
				will be the same register.
			*/
			if (
					psInst->eOpcode == ISMP_USP_NDR &&
					(psState->uCompilerFlags & UF_MSAATRANS) == 0
			   )
			{
				if (psInst->u.psSmp->sUSPSample.eTexPrecision == UF_REGFORMAT_U8)
				{
					TryCoalesceRegisters(psState, 
										 psRegState, 
										 psInst,  
										 UF_MAX_CHUNKS_PER_TEXTURE /* uDestIdx */, 
										 0 /* uSrcIdx */);
				}
				#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) || defined(SUPPORT_SGX545)
				/*
					Can hardware unpack sample results to higher precision
				*/
				else if (
							(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_TAG_UNPACK_RESULT) != 0 &&
							psInst->asDest[0].eFmt != UF_REGFORMAT_C10 && 
							psInst->asDest[0].eFmt <= psInst->u.psSmp->sUSPSample.eTexPrecision
						)
				{
					TryCoalesceRegisters(psState, 
										 psRegState, 
										 psInst,
										 UF_MAX_CHUNKS_PER_TEXTURE /* uDestIdx */, 
										 0 /* uSrcIdx */);
				}
				#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) || defined(SUPPORT_SGX545) */
			}
			if (psInst->eOpcode == ISMPUNPACK_USP)
			{
				TryCoalesceRegisters(psState, 
									 psRegState, 
									 psInst,
									 0 /* uDestIdx */, 
									 UF_MAX_CHUNKS_PER_TEXTURE /* uSrcIdx */);			
			}
		}
		#endif /* defined(OUTPUT_USPBIN) */
	}

	((COALESCING_ARG*)pvCoalesceArg)->bRemovedInst = bRemovedInst;
}

#if defined(UF_TESTBENCH) && !(defined(PCONVERT_HASH_COMPUTE) && defined(__linux__))
static
IMG_UINT32 PrintColour(PRAGCOL_STATE psRegState, PCOLOUR psColour, IMG_PCHAR pszHwRegName)
/*****************************************************************************
 FUNCTION	  : PrintColour

 PURPOSE	  : Print a string representation of a register colour.

 PARAMETERS	  : psRegState			- Register allocator state.
			    psColour			- Colour to print.
				pszHwRegNum			- String to print to.

 RETURNS	  : The number of characters printed.
*****************************************************************************/
{
	if (psColour->eType == COLOUR_TYPE_UNDEF)
	{
		return sprintf(pszHwRegName, "__");
	}
	else
	{
		USC_REGTYPE uRegType = USC_UNDEF;
		USC_REGTYPE uRegNum = USC_UNDEF;

		ColourToRegister(&psRegState->sRAData, psColour, &uRegType, &uRegNum);

		return sprintf(pszHwRegName, "%s%u", g_pszRegType[uRegType], uRegNum);
	}
}

static
IMG_VOID PrintNodeColoursGC(PINTERMEDIATE_STATE psState,
							PRAGCOL_STATE psRegState)
/*****************************************************************************
 FUNCTION	  : PrintNodeColoursGC

 PURPOSE	  : Print the assignment of colours to nodes for the
                graph colouring algorithm

 PARAMETERS	  : psState			  - Compiler state.
                psMapData         - Register map data
                uNumNodes         - Number of nodes
                apsInterval       - Interval array

 RETURNS	  : Nothing.
*****************************************************************************/
{
    IMG_UINT32 uNode;
	const IMG_UINT32 uInc = 5;
	IMG_CHAR *pszOutput = 0;
	IMG_UINT32 uBufferSize = sizeof(IMG_CHAR) * 256;
	pszOutput = UscAlloc(psState, uBufferSize);

	DBG_PRINTF((DBG_MESSAGE, "Register Mapping"));
	DBG_PRINTF((DBG_MESSAGE, "----------------"));

	for (uNode = psRegState->sRAData.uTempStart; uNode < psRegState->sRAData.uNrRegisters; uNode += uInc)
    {
		IMG_UINT32 uIdx, uCtr;

		memset(pszOutput, 0, uBufferSize);
		uCtr = 0;

		for (uIdx = 0; uIdx < uInc && (uNode + uIdx) < psRegState->sRAData.uNrRegisters; uIdx ++)
		{
			IMG_CHAR	pszHwRegName[64];
			PNODE_DATA	psNode = &psRegState->asNodes[uNode + uIdx];

			if (psNode->uColourCount == 1)
			{
				PrintColour(psRegState, &psNode->asColours[0], pszHwRegName);
			}
			else
			{
				IMG_UINT32	uColourIdx;
				IMG_PCHAR	pszStr;

				pszStr = pszHwRegName;
				pszStr += sprintf(pszStr, "{");
				for (uColourIdx = 0; uColourIdx < psNode->uColourCount; uColourIdx++)
				{
					pszStr += PrintColour(psRegState, &psNode->asColours[uColourIdx], pszHwRegName);
					if (uColourIdx < (psNode->uColourCount - 1))
					{
						pszStr += sprintf(pszStr, ", ");
					}
				}
				pszStr += sprintf(pszStr, "}");
			}

			uCtr += sprintf(&pszOutput[uCtr], "r%u -> %s\t ",
							uNode + uIdx - psRegState->sRAData.uTempStart, pszHwRegName);
		}

		DBG_PRINTF((DBG_MESSAGE, "%s", pszOutput));
    }

	/* Print register arrays */
	if (psState->apsVecArrayReg != NULL)
	{
		IMG_UINT32 uIdx;
		DBG_PRINTF((DBG_MESSAGE, "\nRegister arrays:"));
		DBG_PRINTF((DBG_MESSAGE, "----------------"));

		for (uIdx = 0; uIdx < psState->uNumVecArrayRegs; uIdx ++)
		{
			PUSC_VEC_ARRAY_REG psVecArrayReg = psState->apsVecArrayReg[uIdx];

			if (psVecArrayReg != NULL)
			{
				DBG_PRINTF((DBG_MESSAGE, "R%u -> %s%u", uIdx, g_pszRegType[psVecArrayReg->uRegType], psVecArrayReg->uBaseReg));
			}
		}
	}

	/* Done */
	DBG_PRINTF((DBG_MESSAGE, "------------------------------\n"));

	UscFree(psState, pszOutput);
}
#else /* defined(UF_TESTBENCH) && !(defined(PCONVERT_HASH_COMPUTE) && defined(__linux__)) */
#define PrintNodeColoursGC(a, b)	
#endif /* defined(UF_TESTBENCH) && !(defined(PCONVERT_HASH_COMPUTE) && defined(__linux__)) */

static
IMG_VOID ConvertMOVToPCKBP(PINTERMEDIATE_STATE	psState,
						   PCODEBLOCK			psBlock,
						   IMG_PVOID			pvMOVToPCKList)
/*****************************************************************************
 FUNCTION	: ConvertMOVToPCKBP

 PURPOSE	: Look for MOV instructions which could be converted to PCKU8U8
			  or PCKC10C10.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Basic block.
			  pvMovToPckList	- Head of the list of converted instructions.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST			psInst;
	PUSC_LIST		psMOVToPCKList = (PUSC_LIST)pvMOVToPCKList;

	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		return;
	}

	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		SetBit(psInst->auFlag, INST_CHANGEDMOVTOPCK, 0);
		if (psInst->eOpcode == IMOV && g_abSingleBitSet[psInst->auLiveChansInDest[0]])
		{
			IMG_UINT32	uSourceComponent = (IMG_UINT32)(g_aiSingleComponent[psInst->auLiveChansInDest[0]]);

			psInst->auDestMask[0] = psInst->auLiveChansInDest[0];
			if (psInst->asDest[0].eFmt == UF_REGFORMAT_C10)
			{
				SetOpcode(psState, psInst, IPCKC10C10);
				if (uSourceComponent == USC_ALPHA_CHAN)
				{
					ASSERT(psInst->asDest[0].uType == USEASM_REGTYPE_FPINTERNAL);
					if (psInst->asArg[0].uType == USEASM_REGTYPE_TEMP)
					{
						uSourceComponent = USC_RED_CHAN;
					}
				}
			}
			else
			{
				SetOpcode(psState, psInst, IPCKU8U8);
			}
			SetPCKComponent(psState, psInst, 0 /* uArgIdx */, uSourceComponent);
			InitInstArg(&psInst->asArg[1]);
			psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psInst->asArg[1].uNumber = 0;

			SetBit(psInst->auFlag, INST_CHANGEDMOVTOPCK, 1);

			AppendToList(psMOVToPCKList, &psInst->sAvailableListEntry);
		}
	}
}

static
IMG_VOID ConvertPCKToMOV(PINTERMEDIATE_STATE	psState,
						 PUSC_LIST				psMovToPckList)
/*****************************************************************************
 FUNCTION	: ConvertPCKToMOV

 PURPOSE	: For any instructions which were temporarily converted from MOV
			  to PCKU8U8/PCKC10C10 try and convert them back.

 PARAMETERS	: psState			- Compiler state.
			  psMovToPckList	- Head of the list of converted instructions.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;

	for (psListEntry = psMovToPckList->psHead;
		 psListEntry != NULL;
		 psListEntry = psListEntry->psNext)
	{
		PINST	psInst = IMG_CONTAINING_RECORD(psListEntry, PINST, sAvailableListEntry);

		/*
			Look for a PCK which is equivalent to a move: No swizzling of data and no
			need for a write mask.
		*/
		if (
				(psInst->eOpcode == IPCKC10C10 || psInst->eOpcode == IPCKU8U8) &&
				g_abSingleBitSet[psInst->auLiveChansInDest[0]] &&
				(IMG_UINT32)g_aiSingleComponent[psInst->auLiveChansInDest[0]] == GetPCKComponent(psState, psInst, 0 /* uArgIdx */) &&
				(psInst->auLiveChansInDest[0] & ~psInst->auDestMask[0]) == 0
		   )
		{
			psInst->auDestMask[0] = USC_DESTMASK_FULL;
			SetOpcode(psState, psInst, IMOV);
		}
	}
}

static
IMG_UINT32 RenameSAProgResult(PRAGCOL_STATE		psRegState,
							  PSAPROG_RESULT	psResult)
/*****************************************************************************
 FUNCTION	: RenameSAProgResult

 PURPOSE	: Get the number of the secondary attribute assigned to a
			  secondary update program result.

 PARAMETERS	: psRegState			- Module state.
			  psResult				- Secondary update program result.

 RETURNS	: The number of the secondary attribute.
*****************************************************************************/
{
	IMG_UINT32			uNode;
	PCOLOUR				psColour;
	PINTERMEDIATE_STATE	psState = psRegState->sRAData.psState;
	PFIXED_REG_DATA		psFixedReg = psResult->psFixedReg;

	ASSERT(psFixedReg->uConsecutiveRegsCount == 1);
	ASSERT(psFixedReg->uVRegType == USEASM_REGTYPE_TEMP);
	uNode = RegisterToNode(&psRegState->sRAData, USEASM_REGTYPE_TEMP, psFixedReg->auVRegNum[0]);
	ASSERT(psRegState->asNodes[uNode].uColourCount == 1);
	psColour = &psRegState->asNodes[uNode].asColours[0];
	ASSERT(psColour->eType == COLOUR_TYPE_PRIMATTR);

	return psColour->uNum;
}

static
IMG_VOID RenameSecondaryAttribute(PINTERMEDIATE_STATE	psState,
								  PUSEDEF_CHAIN			psSAUseDef,
								  IMG_UINT32			uAssignedSecAttr,
								  PUSC_LIST				psModifiedInstList)
/*****************************************************************************
 FUNCTION	: RenameSecondaryAttribute

 PURPOSE	: Replace a temporary register by the secondary
			  attribute assigned by the register allocator.

 PARAMETERS	: psState			- Compiler state.
			  psSAUseDef		- List of uses of the temporary register.
			  uAssignedSecAttr	- Secondary attribute register assigned.
			  psModifiedInstList
								- Any instructions whose arguments are
								modified are appended to this list.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psUseListEntry, psNextUseListEntry;

	if (psSAUseDef == NULL)
	{
		return;
	}

	for (psUseListEntry = psSAUseDef->sList.psHead; psUseListEntry != NULL; psUseListEntry = psNextUseListEntry)
	{
		PUSEDEF	psUse;

		psNextUseListEntry = psUseListEntry->psNext;
		psUse = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);

		if (psUse->eType == USE_TYPE_SRC)
		{
			PINST	psUseInst = psUse->u.psInst;

			if (psUseInst->psBlock->psOwner->psFunc != psState->psSecAttrProg)
			{
				PARG		psArg;
				IMG_UINT32	uNewSecAttr;
				IMG_UINT32	uArg;

				ASSERT(psUse->uLocation < psUseInst->uArgumentCount);
				uArg = psUse->uLocation;
				psArg = &psUseInst->asArg[uArg];

				uNewSecAttr = uAssignedSecAttr;
				if (psArg->uType == USC_REGTYPE_REGARRAY)
				{
					uNewSecAttr = uAssignedSecAttr + psArg->uArrayOffset;
				}

				psArg->uType = USEASM_REGTYPE_SECATTR;
				psArg->uNumber = uNewSecAttr;
				psArg->uArrayOffset = 0;
				psArg->psRegister = NULL;

				if (GetBit(psUseInst->auFlag, INST_MODIFIEDINST) == 0)
				{
					AppendToList(psModifiedInstList, &psUseInst->sAvailableListEntry);
					SetBit(psUseInst->auFlag, INST_MODIFIEDINST, 1);
				}
	
				#if defined(DEBUG)
				if (
						psArg->uIndexType == USC_REGTYPE_NOINDEX && 
						GetLiveChansInArg(psState, psUseInst, uArg) != 0 &&
						psSAUseDef->psDef != NULL
				   )
				{
					ASSERT(psArg->uNumber >= psState->psSAOffsets->uInRegisterConstantOffset);
					ASSERT(psArg->uNumber < (psState->psSAOffsets->uInRegisterConstantOffset + psState->psSAOffsets->uInRegisterConstantLimit));
				}
				#endif /* defined(DEBUG) */
			}
		}
	}
}

static
IMG_VOID RenameSecondaryAttributes(PINTERMEDIATE_STATE psState, PRAGCOL_STATE psRegState)
/*****************************************************************************
 FUNCTION	: RenameSecondaryAttributes

 PURPOSE	: Replace temporary registers by the secondary
			  attribute assigned by the register allocator.

 PARAMETERS	: psState			- Compiler state.
			  psRegState		- Register allocator state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	USC_LIST		sModifiedInstList;
	PUSC_LIST_ENTRY	psListEntry;
	PSAPROG_STATE	psSAProg = &psState->sSAProg;
	IMG_UINT32		uArrayIdx;	

	InitializeList(&sModifiedInstList);

	/*
		Rename arrays which are in secondary attributes.
	*/
	for (uArrayIdx = 0; uArrayIdx < psState->uNumVecArrayRegs; uArrayIdx++)
	{
		PUSC_VEC_ARRAY_REG	psVecArrayReg = psState->apsVecArrayReg[uArrayIdx];
		
		if (psVecArrayReg == NULL)
		{
			continue;
		}
		if (psVecArrayReg->eArrayType == ARRAY_TYPE_DIRECT_MAPPED_SECATTR ||
			psVecArrayReg->eArrayType == ARRAY_TYPE_DRIVER_LOADED_SECATTR)
		{
			PUSEDEF_CHAIN	psSAUseDef;

			psSAUseDef = psVecArrayReg->psUseDef;

			ASSERT(psVecArrayReg->uRegType == USEASM_REGTYPE_PRIMATTR);

			RenameSecondaryAttribute(psState, psSAUseDef, psVecArrayReg->uBaseReg, &sModifiedInstList);
		}
	}		

	/*
		Rename intermediate registers which are secondary attributes.
	*/
	for (psListEntry = psSAProg->sResultsList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PSAPROG_RESULT	psResult;
		PFIXED_REG_DATA	psFixedReg;
		IMG_UINT32		uAssignedSecAttr;
		PUSEDEF_CHAIN	psSAUseDef;

		psResult = IMG_CONTAINING_RECORD(psListEntry, PSAPROG_RESULT, sListEntry);
		psFixedReg = psResult->psFixedReg;

		ASSERT(psFixedReg->uConsecutiveRegsCount == 1);
		ASSERT(psFixedReg->uVRegType == USEASM_REGTYPE_TEMP);

		/*
			Skip intermediate registers which are part of an array.
		*/
		if (psFixedReg->uRegArrayIdx != USC_UNDEF)
		{
			continue;
		}
		
		psSAUseDef = UseDefGet(psState, psFixedReg->uVRegType, psFixedReg->auVRegNum[0]);
		ASSERT(psSAUseDef != NULL);

		/*
			Get the secondary attribute number assigned to this intermediate register.
		*/
		uAssignedSecAttr = RenameSAProgResult(psRegState, psResult);

		/*
			Replace all instances of the intermediate register by the secondary attribute.
		*/
		RenameSecondaryAttribute(psState, psSAUseDef, uAssignedSecAttr, &sModifiedInstList);
	}

	/*
		For all instructions whose source arguments were modified.
	*/
	while ((psListEntry = RemoveListHead(&sModifiedInstList)) != NULL)
	{
		PINST	psModifiedInst;

		psModifiedInst = IMG_CONTAINING_RECORD(psListEntry, PINST, sAvailableListEntry);

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		/*
			Check for cases where an instruction source swizzle needs to be updated to make valid
			a group of instruction arguments which require consecutive hardware register numbers.
		*/
		FixConsecutiveRegisterSwizzlesInst(psState, psModifiedInst);
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

		/*
			Check groups of registers that need consecutive numbers.
		*/
		#if defined(DEBUG)
		CheckValidRegisterGroupsInst(psState, psModifiedInst);	
		#endif /* defined(DEBUG) */
	}
}

static
IMG_VOID ExpandDeltaInstruction(PINTERMEDIATE_STATE psState, PINST psDeltaInst)
/*****************************************************************************
 FUNCTION	: ExpandDeltaInstruction

 PURPOSE	: Expand an SSA choice function to moves in the predecessor blocks.

 PARAMETERS	: psState		- Compiler state.
			  psDeltaInst	- Instruction to expand.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PCODEBLOCK	psDeltaBlock;
	IMG_UINT32	uPredIdx;
	PARG		psDeltaDest;
	#if defined(OUTPUT_USPBIN)
	IMG_BOOL	bShaderResult;
	#endif /* defined(OUTPUT_USPBIN) */

	psDeltaBlock = psDeltaInst->psBlock;

	ASSERT(psDeltaInst->uArgumentCount == psDeltaBlock->uNumPreds);

	psDeltaDest = &psDeltaInst->asDest[0];
	ASSERT(psDeltaDest->uType == USEASM_REGTYPE_TEMP || psDeltaDest->uType == USEASM_REGTYPE_PREDICATE);

	#if defined(OUTPUT_USPBIN)
	/*
		Is the delta instruction destination a shader result?
	*/
	bShaderResult = IMG_FALSE;
	if ((psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) != 0)
	{
		if ((psDeltaInst->uShaderResultHWOperands & (1 << 0)) != 0)
		{
			bShaderResult = IMG_TRUE;
			psDeltaInst->uShaderResultHWOperands &= ~(1U << 0);
		}
	}
	#endif /* defined(OUTPUT_USPBIN) */

	for (uPredIdx = 0; uPredIdx < psDeltaBlock->uNumPreds; uPredIdx++)
	{
		PINST		psMOVInst;
		PCODEBLOCK	psPredBlock;
		IMG_UINT32	uPredBlockDestIdx;
		PARG		psDeltaArg = &psDeltaInst->asArg[uPredIdx];

		psPredBlock = psDeltaBlock->asPreds[uPredIdx].psDest;
		uPredBlockDestIdx = psDeltaBlock->asPreds[uPredIdx].uDestIdx;		
		if (psPredBlock->uNumSuccs > 1)
		{
			psPredBlock = AddEdgeViaEmptyBlock(psState, psDeltaBlock, uPredIdx);
		}
 
		ASSERT(psDeltaArg->uIndexType == USC_REGTYPE_NOINDEX);

		SetRegisterLiveMask(psState, 
							&psPredBlock->sRegistersLiveOut,
							psDeltaArg->uType,
							psDeltaArg->uNumber,
							0 /* uArrayOffset */,
							0 /* uMask */);
			
		psMOVInst = AllocateInst(psState, psDeltaInst);
		if (psDeltaDest->uType == USEASM_REGTYPE_PREDICATE)
		{
			SetOpcode(psState, psMOVInst, IMOVPRED);
		}
		else if (psDeltaArg->uType == USEASM_REGTYPE_IMMEDIATE && psDeltaArg->uNumber > EURASIA_USE_MAXIMUM_IMMEDIATE)
		{
			SetOpcode(psState, psMOVInst, ILIMM);
		}
		else
		{
			SetOpcode(psState, psMOVInst, IMOV);
		}
		SetDestFromArg(psState, psMOVInst, 0 /* uDestIdx */, psDeltaDest);
		MoveSrc(psState, psMOVInst, 0 /* uMoveToSrcIdx */, psDeltaInst, uPredIdx);
		AppendInst(psState, psPredBlock, psMOVInst);

		#if defined(OUTPUT_USPBIN)
		/*
			Copy the shader result flag to the new instruction.
		*/	
		if (bShaderResult)
		{
			psMOVInst->uShaderResultHWOperands |= (1 << 0);
		}
		#endif /* defined(OUTPUT_USPBIN) */

		SetRegisterLiveMask(psState, 
							&psPredBlock->sRegistersLiveOut,
							psDeltaDest->uType,
							psDeltaDest->uNumber,
							0 /* uArrayOffset */,
							psDeltaInst->auLiveChansInDest[0]);
	}

	RemoveInst(psState, psDeltaBlock, psDeltaInst);
	FreeInst(psState, psDeltaInst);
}

static
IMG_VOID ExpandDeltaInstructions(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: ExpandDeltaInstructions

 PURPOSE	: Expand DELTA instructions to moves in the predecessor blocks.

 PARAMETERS	: psState			- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ForAllInstructionsOfType(psState, IDELTA, ExpandDeltaInstruction);

	MergeAllBasicBlocks(psState);
}

static
IMG_BOOL ConvertPartialMoveToSOP2(PINTERMEDIATE_STATE psState, PINST psInst, IMG_BOOL bPack)
/*****************************************************************************
 FUNCTION	: ConvertPartialMoveToSOP2

 PURPOSE	: Check for special cases for PCK/SOPWM instructions where just
			  the ALPHA channel or the RGB channels are written.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- PCKC10C10/PCKU8U8/SOPWM instruction.
			  bPack				- TRUE if the instruction is PCKC10C10 or PCKU8U8.
								  FALSE if the instruction is SOPWM.
								

 RETURNS	: TRUE if the instruction no longer needs expansion.
*****************************************************************************/
{
	IMG_BOOL	bConvertToSOP2;
	IMG_BOOL	bReplaceByHwConst = IMG_FALSE;
	IMG_UINT32	uHwConstNum = USC_UNDEF;
	IMG_UINT32	uDest;
	PARG		psOldDest = psInst->apsOldDest[0];

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psInst->eOpcode == ISOPWM_VEC && psInst->asDest[0].eFmt == UF_REGFORMAT_C10)
	{
		/*
			On this core SOP2 doesn't support writing a vec4 to a unified store register destination.
		*/
		return IMG_FALSE;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	ASSERT(psInst->uDestCount == 1);

	/*
		If the source for the unwritten channels is an immediate check it is supported
		by the SOP2 instruction.
	*/
	bReplaceByHwConst = IMG_FALSE;
	if (psOldDest->uType == USEASM_REGTYPE_IMMEDIATE)
	{
		if (!IsImmediateSourceValid(psState, psInst, 0 /* uArgIdx */, 0 /* uComponentSelect */, psOldDest->uNumber))
		{
			if ((uHwConstNum = FindHardwareConstant(psState, psOldDest->uNumber)) != USC_UNDEF)
			{
				bReplaceByHwConst = IMG_TRUE;
			}
			else
			{
				return IMG_FALSE;
			}
		}
	}

	/*
		For a SOPWM instruction: check we are just moving data.
	*/
	if (!bPack)
	{
		if (!IsSOPWMMove(psInst))
		{
			return IMG_FALSE;
		}
		if (psInst->u.psSopWm->bRedChanFromAlpha)
		{
			return IMG_FALSE;
		}
		/*	
			Check the SOPWM isn't converting to or from C10 format.
		*/
		if (psInst->asDest[0].eFmt == UF_REGFORMAT_C10 && psInst->asArg[0].eFmt != UF_REGFORMAT_C10)
		{
			return IMG_FALSE;
		}
		if (psInst->asDest[0].eFmt != UF_REGFORMAT_C10 && psInst->asArg[0].eFmt == UF_REGFORMAT_C10)
		{
			return IMG_FALSE;
		}
	}

	/*
		Check for moving just the ALPHA channel.
	*/
	bConvertToSOP2 = IMG_FALSE;
	if (psInst->auDestMask[0] == USC_W_CHAN_MASK)
	{
		if (bPack)
		{
			PARG	psAlphaSrc = &psInst->asArg[0];

			if (
					!(
						psAlphaSrc->uType == USEASM_REGTYPE_FPCONSTANT && 
						(
							psAlphaSrc->uNumber == EURASIA_USE_SPECIAL_CONSTANT_INT32ONE ||
							psAlphaSrc->uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO
						)
					)
				)
			{
				IMG_UINT32	uComponent;
				
				#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				if (psInst->eOpcode == IVPCKU8U8)
				{
					USEASM_SWIZZLE_SEL	eSel;

					eSel = GetChan(psInst->u.psVec->auSwizzle[0], USC_W_CHAN);
					ASSERT(!g_asSwizzleSel[eSel].bIsConstant);
					uComponent = g_asSwizzleSel[eSel].uSrcChan;
				}
				else
				#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
				{
					uComponent = GetPCKComponent(psState, psInst, 0 /* uArgIdx */);
				}

				if (uComponent != USC_W_CHAN)
				{
					return IMG_FALSE;
				}
			}
		}
		bConvertToSOP2 = IMG_TRUE;
	}
	/*
		Check for moving just the RGB channels.
	*/
	if (!bPack && (psInst->auDestMask[0] & USC_W_CHAN_MASK) == 0)
	{
		IMG_UINT32	uPreservedChanMask;

		uPreservedChanMask = psInst->auLiveChansInDest[0] & ~psInst->auDestMask[0];
		if ((uPreservedChanMask & USC_RGB_CHAN_MASK) != 0)
		{
			return IMG_FALSE;
		}
		bConvertToSOP2 = IMG_TRUE;
	}

	
	if (bConvertToSOP2)
	{
		IMG_UINT32	uPartialDestSrc;
		IMG_UINT32	uSrcCount;

		/*
			Convert

				IPCKU8U8	T.W[=S], A
			->
				SOP2		T, S, A
							(T.RGB = S; T.ALPHA = A)
		*/ 
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
		{
			IOPCODE	eOldOpcode = psInst->eOpcode;
			SetOpcode(psState, psInst, ISOP2_VEC);
			uSrcCount = 2;
			if (eOldOpcode == IVPCKU8U8)
			{
				SetSrcUnused(psState, psInst, 1 /* uSrcIdx */);
			}
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			SetOpcode(psState, psInst, ISOP2);
			uSrcCount = 1;
		}

		/*
			Set the colour calculation to
				
				SRC1 * (1 - 0) + SRC2 * 0
		*/
		psInst->u.psSop2->uCOp = USEASM_INTSRCSEL_ADD;
		psInst->u.psSop2->uCSel1 = USEASM_INTSRCSEL_ZERO;
		psInst->u.psSop2->bComplementCSel1 = IMG_TRUE;
		psInst->u.psSop2->uCSel2 = USEASM_INTSRCSEL_ZERO;
		psInst->u.psSop2->bComplementCSel2 = IMG_FALSE;
		psInst->u.psSop2->bComplementCSrc1 = IMG_FALSE;

		/*
			Set the alpha calculation to
				
				SRC1 * 0 + SRC2 * (1 - 0)
		*/
		psInst->u.psSop2->uAOp = USEASM_INTSRCSEL_ADD;
		psInst->u.psSop2->uASel1 = USEASM_INTSRCSEL_ZERO;
		psInst->u.psSop2->bComplementASel1 = IMG_FALSE;
		psInst->u.psSop2->uASel2 = USEASM_INTSRCSEL_ZERO;
		psInst->u.psSop2->bComplementASel2 = IMG_TRUE;
		psInst->u.psSop2->bComplementASrc1 = IMG_FALSE;

		if (psInst->auDestMask[0] == USC_W_CHAN_MASK)
		{
			IMG_UINT32	uSrc;

			for (uSrc = 0; uSrc < uSrcCount; uSrc++)
			{
				MoveSrc(psState, 
						psInst, 
						1 * uSrcCount + uSrc /* uMoveToSrcIdx */,
						psInst, 
						0 * uSrcCount + uSrc /* uMoveFromSrcIdx */);
			}
			uPartialDestSrc = 0 * uSrcCount;
		}
		else
		{
			uPartialDestSrc = 1 * uSrcCount;
		}

		for (uDest = 0; uDest < psInst->uDestCount; uDest++)
		{
			psOldDest = psInst->apsOldDest[uDest];

			if (bReplaceByHwConst)
			{
				SetSrc(psState, psInst, uPartialDestSrc + uDest, USEASM_REGTYPE_FPCONSTANT, uHwConstNum, psOldDest->eFmt);
			}
			else
			{
				MovePartialDestToSrc(psState, psInst, uPartialDestSrc + uDest, psInst, uDest);
			}
			SetPartiallyWrittenDest(psState, psInst, uDest, NULL /* psPartialDest */);

			psInst->auDestMask[uDest] = USC_ALL_CHAN_MASK;
		}
		for (; uDest < uSrcCount; uDest++)
		{
			SetSrcUnused(psState, psInst, uPartialDestSrc + uDest);
		}

		return IMG_TRUE;
	}
	return IMG_FALSE;
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_BOOL HandleVPCKMask(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uGroupCount)
/*****************************************************************************
 FUNCTION	: HandlePCKMask

 PURPOSE	: Check for special cases for VPCK instructions where the source
			  for partially overwritten data as the packed source.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- VPCKC10C10 instruction.

 RETURNS	: TRUE if the instruction no longer needs expansion.
*****************************************************************************/
{
	IMG_UINT32				uGroup;
	static const IMG_UINT32	auMask[] = {USC_RGB_CHAN_MASK, USC_ALPHA_CHAN_MASK};
	static const IMG_UINT32 aeChanSel[] = {USEASM_SWIZZLE_SEL_X, 
										   USEASM_SWIZZLE_SEL_Y, 
										   USEASM_SWIZZLE_SEL_Z, 
										   USEASM_SWIZZLE_SEL_W};
	IMG_UINT32				uNewSwizzle;
	IMG_UINT32				uChan;

	/*
		Check the source for the non-written channels is the same as the source
		for the written channels.
	*/
	for (uGroup = 0; uGroup < uGroupCount; uGroup++)
	{
		PARG	psOldDest = psInst->apsOldDest[uGroup];

		if ((psInst->auDestMask[0] & auMask[uGroup]) == 0)
		{
			continue;
		}

		if (psOldDest != NULL && !EqualArgs(psOldDest, &psInst->asArg[uGroup]))
		{
			return IMG_FALSE;
		}
	}

	/*
		Replace channel selectors in the swizzle for unwritten channels.
	*/
	uNewSwizzle = psInst->u.psVec->auSwizzle[0];
	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		if ((psInst->auDestMask[0] & (1U << uChan)) == 0)
		{
			uNewSwizzle = SetChan(uNewSwizzle, uChan, aeChanSel[uChan]);
		}
	}

	/*
		If 3 or 4 channels are written then the first and third, and second and fourth
		selectors must match in the swizzle.
	*/
	if (g_auSetBitCount[psInst->auLiveChansInDest[0]] >= 3)
	{
		IMG_UINT32	uHalf;
		
		for (uHalf = 0; uHalf < 2; uHalf++)
		{
			IMG_UINT32	uChan0 = uHalf * 2 + 0;
			IMG_UINT32	uChan1 = uHalf * 2 + 1;
			IMG_UINT32	uSel0, uSel1;

			uSel0 = GetChan(uNewSwizzle, uChan0);
			uSel1 = GetChan(uNewSwizzle, uChan1);

			if ((psInst->auLiveChansInDest[0] & (1U << uChan0)) == 0)
			{
				uNewSwizzle = SetChan(uNewSwizzle, uChan0, uSel1);
			}
			else if ((psInst->auLiveChansInDest[0] & (1U << uChan1)) == 0)
			{
				uNewSwizzle = SetChan(uNewSwizzle, uChan1, uSel0);
			}
			else
			{
				if (uSel0 != uSel1)
				{
					return IMG_FALSE;
				}
			}
		}
	}

	/*
		Clear the sources for the non-written channel.
	*/
	for (uGroup = 0; uGroup < uGroupCount; uGroup++)
	{
		SetPartiallyWrittenDest(psState, psInst, uGroup, NULL /* psPartialDest */);

		/*
			Write all channels from the PCK rather than copying some of them from the 
			another register.
		*/
		psInst->auDestMask[uGroup] |= psInst->auLiveChansInDest[uGroup];
	}

	/*
		The hardware doesn't allow only 3 channels to be written by VPCKC10C10 so increase
		the mask to write all 4 channels.
	*/
	if (g_auSetBitCount[psInst->auDestMask[0]] == 3)
	{
		psInst->auDestMask[0] = USC_ALL_CHAN_MASK;
		psInst->auDestMask[1] |= USC_RED_CHAN_MASK;
	}
	psInst->u.psVec->auSwizzle[0] = uNewSwizzle;

	return IMG_TRUE;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static
IMG_BOOL HandlePCKMask(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: HandlePCKMask

 PURPOSE	: Check for special cases for PCK instructions where the source
			  for partially overwritten data is the same as one of the other
			  sources.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- PCKC10C10/PCKU8U8 instruction.

 RETURNS	: TRUE if the instruction no longer needs expansion.
*****************************************************************************/
{
	IMG_UINT32	uDestChan;
	IMG_UINT32	uNewDestMask;
	ARG			asDestChanSrc[VECTOR_LENGTH];
	IMG_UINT32	auDestChanSrcComp[VECTOR_LENGTH];
	IMG_UINT32	auNewSrc[PCK_SOURCE_COUNT];
	IMG_UINT32	uSrc;
	IMG_BOOL	bConvertToMove;
	PARG		psSrc;


	ASSERT(psInst->eOpcode == IPCKC10C10 || psInst->eOpcode == IPCKU8U8);
	ASSERT(psInst->uDestCount == 1);

	/*
		Get the source argument or overwritten destination copied to each channel in the 
		new destination.
	*/
	uSrc = 0;
	for (uDestChan = 0; uDestChan < VECTOR_LENGTH; uDestChan++)
	{
		if ((psInst->auDestMask[0] & (1U << uDestChan)) != 0)
		{
			asDestChanSrc[uDestChan] = psInst->asArg[uSrc];
			auDestChanSrcComp[uDestChan] = GetPCKComponent(psState, psInst, uSrc);
			uSrc = (uSrc + 1) % PCK_SOURCE_COUNT;
		}
		else
		{
			asDestChanSrc[uDestChan] = *psInst->apsOldDest[0];
			auDestChanSrcComp[uDestChan] = uDestChan;
		}
	}

	/*
		If the partially written destination is the same as the sources then convert to a MOV.
	*/
	bConvertToMove = IMG_TRUE;
	psSrc = NULL;
	for (uDestChan = 0; uDestChan < VECTOR_LENGTH; uDestChan++)
	{
		if ((psInst->auLiveChansInDest[0] & (1U << uDestChan)) != 0)
		{
			if (auDestChanSrcComp[uDestChan] != uDestChan)
			{
				bConvertToMove = IMG_FALSE;
				break;
			}
			if (psSrc != NULL && !EqualArgs(psSrc, &asDestChanSrc[uDestChan]))
			{
				bConvertToMove = IMG_FALSE;
				break;
			}
			psSrc = &asDestChanSrc[uDestChan];
		}
	}
	if (bConvertToMove)
	{
		if (psInst->eOpcode == IPCKC10C10)
		{
			MakeSOPWMMove(psState, psInst);
		}
		else
		{
			SetOpcode(psState, psInst, IMOV);
		}
		psInst->auDestMask[0] = USC_ALL_CHAN_MASK;
		SetPartiallyWrittenDest(psState, psInst, 0 /* uDestIdx */, NULL /* psPartiallyWrittenDest */);
		return IMG_TRUE;
	}

	/*
		If we avoided expanding the instruction what would the new
		destination mask be?
	*/
	uNewDestMask = psInst->auLiveChansInDest[0];
	if (g_auSetBitCount[uNewDestMask] == 3)
	{
		uNewDestMask = USC_ALL_CHAN_MASK;
	}

	for (uSrc = 0; uSrc < PCK_SOURCE_COUNT; uSrc++)
	{
		auNewSrc[uSrc] = USC_UNDEF;
	}

	/*
		Check that where a single source argument in the new instruction
		would be copied to multiple channels in the destination those channels had the
		same value in the old instruction.
	*/
	uSrc = 0;
	for (uDestChan = 0; uDestChan < VECTOR_LENGTH; uDestChan++)
	{
		if ((uNewDestMask & (1U << uDestChan)) != 0)
		{
			if ((psInst->auLiveChansInDest[0] & (1U << uDestChan)) != 0)
			{
				if (auNewSrc[uSrc] == USC_UNDEF)
				{
					auNewSrc[uSrc] = uDestChan;
				}
				else
				{
					IMG_UINT32	uMatchChan = auNewSrc[uSrc];

					if (
							!EqualArgs(&asDestChanSrc[uMatchChan], &asDestChanSrc[uDestChan]) ||
							auDestChanSrcComp[uMatchChan] != auDestChanSrcComp[uDestChan]
					   )
					{
						return IMG_FALSE;
					}
				}
			}
			uSrc = (uSrc + 1) % PCK_SOURCE_COUNT;
		}
	}

	/*
		Clear the new instruction.
	*/
	psInst->auDestMask[0] = uNewDestMask;

	for (uSrc = 0; uSrc < PCK_SOURCE_COUNT; uSrc++)
	{
		if (auNewSrc[uSrc] == USC_UNDEF)
		{
			SetSrc(psState, psInst, uSrc, USEASM_REGTYPE_IMMEDIATE, 0, UF_REGFORMAT_U8);
		}
		else
		{
			SetSrcFromArg(psState, psInst, uSrc, &asDestChanSrc[auNewSrc[uSrc]]);
		}
		SetPCKComponent(psState, psInst, uSrc, auDestChanSrcComp[auNewSrc[uSrc]]);
	}

	/*
		Clearly the reference to a partially overwritten source.
	*/
	SetPartiallyWrittenDest(psState, psInst, 0 /* uDestIdx */, NULL /* psPartiallyWrittenDest */);

	return IMG_TRUE;
}

static
IMG_VOID CreateMoveForMask(PINTERMEDIATE_STATE	psState, 
						   PINST				psSrcLineInst, 
						   PARG					psArgToCopy,
						   IMG_UINT32			uUsedChans,
						   PINST*				ppsMOVInst)
/*****************************************************************************
 FUNCTION	: CreateMoveForMask

 PURPOSE	: Create a move instruction suitable for copying a partially/conditionally
			  overwritten source.

 PARAMETERS	: psState			- Compiler state.
			  psSrcLineInst		- Supplies source information for the move
								instruction.
			  psArgToCopy		- Details of the data to copy.
			  uUsedChans		- Mask of channels to copy.
			  ppsMOVInst		- Returns the created instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psMOVInst;

	psMOVInst = *ppsMOVInst = AllocateInst(psState, psSrcLineInst);

	if (psArgToCopy->uType == USEASM_REGTYPE_PREDICATE || psArgToCopy->uType == USC_REGTYPE_BOOLEAN)
	{
		SetOpcode(psState, psMOVInst, IMOVPRED);
		return;
	}

	if (psArgToCopy->uType == USEASM_REGTYPE_IMMEDIATE && psArgToCopy->uNumber > EURASIA_USE_MAXIMUM_IMMEDIATE)
	{
		SetOpcode(psState, psMOVInst, ILIMM);
		return;
	}

	if (uUsedChans != USC_ALL_CHAN_MASK)
	{
		IMG_BOOL	bUseSOPWM;

		bUseSOPWM = IMG_FALSE;
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
		{
			if (psArgToCopy->eFmt == UF_REGFORMAT_U8)
			{
				bUseSOPWM = IMG_TRUE;
			}
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			if (psArgToCopy->eFmt == UF_REGFORMAT_U8 || psArgToCopy->eFmt == UF_REGFORMAT_C10)
			{
				bUseSOPWM = IMG_TRUE;
			}
		}

		if (bUseSOPWM)
		{
			MakeSOPWMMove(psState, psMOVInst);
			psMOVInst->auDestMask[0] = uUsedChans;
			return;
		}
	}
	
	SetOpcode(psState, psMOVInst, IMOV);
}

typedef enum
{
	INSERT_LOCATION_BEFORE,
	INSERT_LOCATION_AFTER
} INSERT_LOCATION, PINSERT_LOCATION;

static
IMG_VOID UpdateMaskedInstArguments(PINTERMEDIATE_STATE	psState,
								   INSERT_LOCATION		eLocation,
								   PINST				psInst,
								   IMG_UINT32			uInstDestIdx,
								   #if defined(OUTPUT_USPBIN)
								   IMG_BOOL				bInstIsShaderResult,
								   #endif /* defined(OUTPUT_USPBIN) */
								   PINST				psMovInst,
								   IMG_UINT32			uMovDestIdx,
								   IMG_UINT32			uMovSrcIdx,
								   PARG					psMaskTemp)
{
	if (eLocation == INSERT_LOCATION_AFTER)
	{
		if (psMovInst != NULL)
		{
			/*
				Copy the original instruction's destination to the MOV instruction.
			*/
			MoveDest(psState, psMovInst, uMovDestIdx, psInst, uInstDestIdx);
	
			/*
				Set the newly allocated temporary register as the source to the MOV instruction.
			*/
			SetSrcFromArg(psState, psMovInst, uMovSrcIdx, psMaskTemp);

			#if defined(OUTPUT_USPBIN)
			/*
				Copy the shader result flag to the MOV instruction.
			*/
			if (bInstIsShaderResult)
			{	
				psMovInst->uShaderResultHWOperands |= (1U << 0);
			}
			#endif /* defined(OUTPUT_USPBIN) */
		}

		/*
			Replace the existing instruction destination by the new temporary.
		*/
		SetDestFromArg(psState, psInst, uInstDestIdx, psMaskTemp);
	}
	else
	{
		if (psMovInst != NULL)
		{
			/*
				Set the replacement temporary as the destination of the move instruction.
			*/
			SetDestFromArg(psState, psMovInst, uMovDestIdx, psMaskTemp);

			/*
				Set the value for unwritten channels as the source to the move instruction.
			*/
			MovePartialDestToSrc(psState, psMovInst, uMovSrcIdx, psInst, uInstDestIdx);
		}
					
		/*
			Set the newly allocated temporary register as the source for the partially/conditional
			written destination.
		*/
		SetPartiallyWrittenDest(psState, psInst, uInstDestIdx, psMaskTemp);
	}
}

static
IMG_VOID InsertMaskMoveInst(PINTERMEDIATE_STATE psState, PINST psInst, PINST psMovInst, INSERT_LOCATION eLocation)
{
	/*
		Copy the NOEMIT flag from the instruction.
	*/
	SetBit(psMovInst->auFlag, INST_NOEMIT, GetBit(psInst->auFlag, INST_NOEMIT));

	if (eLocation == INSERT_LOCATION_AFTER)
	{
		/*
			Insert the MOV instruction after the existing instruction.
		*/
		InsertInstBefore(psState, psInst->psBlock, psMovInst, psInst->psNext);
	}
	else
	{
		ASSERT(eLocation == INSERT_LOCATION_BEFORE);

		/*
			Insert the MOV instruction before the existing instruction.
		*/
		InsertInstBefore(psState, psInst->psBlock, psMovInst, psInst);
	}
}

static
IMG_VOID CreateMaskCopyInsts(PINTERMEDIATE_STATE	psState,
							 INSERT_LOCATION		eLocation,
							 ARG					asMaskTemp[],
							 PARG					apsArgToCopy[],
							 IMG_UINT32				auChanMasksToCopy[],
							 PINST					psInst,
							 #if defined(OUTPUT_USPBIN)
							 IMG_BOOL				bInstIsShaderResult,
							 #endif /* defined(OUTPUT_USPBIN) */
							 IMG_UINT32				uGroupStart,
							 IMG_UINT32				uGroupCount)
/*****************************************************************************
 FUNCTION	: CreateMaskCopyInsts

 PURPOSE	: Create a move instruction suitable for copying a set of instruction
			  destinations to/from new temporary registers.

 PARAMETERS	: psState			- Compiler state.
			  eLocation			- Where to insert the new instructions.
			  asMaskTemp		- New temporary registers to copy/from.
			  apsArgToCopy		- Destination arguments to copy.
			  auChanMasksToCopy	- Masks of channels to copy in each argument.
			  psInst			- Instruction whose destinations are to be copied.
			  uGroupStart		- Start of the arguments to copy in the instruction's
								destination array.
			  uGroupCount		- Count of arguments to copy.
			  bInstIsShaderResult
								- TRUE to set the 'is shader result' flag on
								the destinations of the move instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uGroupIdx;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_C10VECTORDEST) != 0 && uGroupCount == 2)
		{
			PINST		psSOPWMInst;

			ASSERT(uGroupStart == 0);

			if (apsArgToCopy[0] != NULL && 
				apsArgToCopy[1] != NULL &&
				IsValidArgumentSet(psState, apsArgToCopy, 2 /* uCount */, HWREG_ALIGNMENT_EVEN))
			{
				/*
					Create a SOPWM instruction to copy a 40-bit C10 vector.
				*/

				psSOPWMInst = AllocateInst(psState, psInst);

				SetOpcodeAndDestCount(psState, psSOPWMInst, ISOPWM_VEC, 2 /* uDestCount */);

				/*
					Set the second source to immediate zero.
				*/
				for (uGroupIdx = 0; uGroupIdx < 2; uGroupIdx++)
				{
					SetSrc(psState, psSOPWMInst, 2 + uGroupIdx, USEASM_REGTYPE_IMMEDIATE, 0, UF_REGFORMAT_U8);
				}

				/*
					Set the SOPWM calculation to
	
						SRC1 * (1 - ZER0) + SRC2 * ZERO
				*/
				psSOPWMInst->u.psSopWm->uSel1 = USEASM_INTSRCSEL_ZERO;
				psSOPWMInst->u.psSopWm->bComplementSel1 = IMG_TRUE;
				psSOPWMInst->u.psSopWm->uSel2 = USEASM_INTSRCSEL_ZERO;
				psSOPWMInst->u.psSopWm->bComplementSel2 = IMG_FALSE;
				psSOPWMInst->u.psSopWm->uCop = USEASM_INTSRCSEL_ADD;
				psSOPWMInst->u.psSopWm->uAop = USEASM_INTSRCSEL_ADD;

				for (uGroupIdx = 0; uGroupIdx < 2; uGroupIdx++)
				{
					psSOPWMInst->auDestMask[uGroupIdx] = auChanMasksToCopy[uGroupIdx];
					psSOPWMInst->auLiveChansInDest[uGroupIdx] = auChanMasksToCopy[uGroupIdx];

					UpdateMaskedInstArguments(psState,
											  eLocation,
											  psInst,
											  uGroupIdx,
											  #if defined(OUTPUT_USPBIN)
											  bInstIsShaderResult,
											  #endif /* defined(OUTPUT_USPBIN) */
											  psSOPWMInst /* psMovInst */,
											  uGroupIdx /* uMovDestIdx */,
											  uGroupIdx /* uMovSrcIdx */,
											  &asMaskTemp[uGroupIdx]);
				}

				MakeGroup(psState, asMaskTemp, 2 /* uArgCount */, HWREG_ALIGNMENT_EVEN);

				InsertMaskMoveInst(psState, psInst, psSOPWMInst, eLocation);
	
				return;
			}
		}

		/*
			If possible copy vector data two registers at a time.
		*/
		if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORDEST) != 0 && uGroupCount == 2)
		{
			ASSERT(uGroupStart == 0);

			if (apsArgToCopy[0] != NULL && 
				apsArgToCopy[1] != NULL &&
				apsArgToCopy[0]->eFmt == UF_REGFORMAT_F32 &&
				apsArgToCopy[1]->eFmt == UF_REGFORMAT_F32)
			{
				PINST		psVPCKInst;

				psVPCKInst = AllocateInst(psState, psInst);

				SetOpcodeAndDestCount(psState, psVPCKInst, IVPCKFLTFLT_EXP, 2 /* uDestCount */);

				psVPCKInst->u.psVec->uPackSwizzle = USEASM_SWIZZLE(X, Z, X, X);
				for (uGroupIdx = 0; uGroupIdx < 2; uGroupIdx++)
				{
					IMG_UINT32	uChan;

					SetSrcUnused(psState, psVPCKInst, uGroupIdx * SOURCE_ARGUMENTS_PER_VECTOR);
					for (uChan = 1; uChan < VECTOR_LENGTH; uChan++)
					{
						SetSrcUnused(psState, psVPCKInst, uGroupIdx * SOURCE_ARGUMENTS_PER_VECTOR + 1 + uChan);
					}	

					psVPCKInst->u.psVec->auSwizzle[uGroupIdx] = USEASM_SWIZZLE(X, Y, Z, W);
					psVPCKInst->u.psVec->aeSrcFmt[uGroupIdx] = asMaskTemp[uGroupIdx].eFmt;
				}

				for (uGroupIdx = 0; uGroupIdx < 2; uGroupIdx++)
				{
					psVPCKInst->auDestMask[uGroupIdx] = auChanMasksToCopy[uGroupIdx];
					psVPCKInst->auLiveChansInDest[uGroupIdx] = auChanMasksToCopy[uGroupIdx];

					UpdateMaskedInstArguments(psState,
											  eLocation,
											  psInst,
											  uGroupIdx,
											  #if defined(OUTPUT_USPBIN)
											  bInstIsShaderResult,
											  #endif /* defined(OUTPUT_USPBIN) */
											  psVPCKInst /* psMovInst */,
											  uGroupIdx /* uMovDestIdx */,
											  uGroupIdx * SOURCE_ARGUMENTS_PER_VECTOR + 1 /* uMovSrcIdx */,
											  &asMaskTemp[uGroupIdx]);
				}

				MakeGroup(psState, asMaskTemp, 2 /* uArgCount */, HWREG_ALIGNMENT_EVEN);

				InsertMaskMoveInst(psState, psInst, psVPCKInst, eLocation);

				return;
			}
		}
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	for (uGroupIdx = 0; uGroupIdx < uGroupCount; uGroupIdx++)
	{
		PARG				psArgToCopy = apsArgToCopy[uGroupIdx];
		IMG_UINT32			uChanMaskToCopy = auChanMasksToCopy[uGroupIdx];
		PINST				psMovInst;

		if (psArgToCopy == NULL)
		{
			UpdateMaskedInstArguments(psState,
									  eLocation,
									  psInst,
									  uGroupStart + uGroupIdx,
									  #if defined(OUTPUT_USPBIN)
									  bInstIsShaderResult,
									  #endif /* defined(OUTPUT_USPBIN) */
									  NULL /* psMovInst */,
									  USC_UNDEF /* uMovDestIdx */,
									  USC_UNDEF /* uMovSrcIdx */,
									  &asMaskTemp[uGroupIdx]);
			continue;
		}
	
		/*
			Create an appropriate instruction to copy the instruction's destination.
		*/
		CreateMoveForMask(psState, psInst, psArgToCopy, uChanMaskToCopy, &psMovInst);
		psMovInst->auLiveChansInDest[0] = uChanMaskToCopy;

		/*
			Insert the created instruction before or after the current instruction.
		*/
		InsertMaskMoveInst(psState, psInst, psMovInst, eLocation);
	
		/*
			Move the new or old destinations from the current instruction to the
			move instruction.
		*/
		UpdateMaskedInstArguments(psState,
								  eLocation,
								  psInst,
								  uGroupStart + uGroupIdx,
								  #if defined(OUTPUT_USPBIN)
								  bInstIsShaderResult,
								  #endif /* defined(OUTPUT_USPBIN) */
								  psMovInst,
								  0 /* uMovDestIdx */,
								  0 /* uMovSrcIdx */,
								  &asMaskTemp[uGroupIdx]);
	}
}

typedef struct _PARTIAL_DEST_PARAMETERS
{
	IMG_UINT32		uDestGroupStart;
	IMG_UINT32		uDestGroupCount;
	HWREG_ALIGNMENT	eDestGroupAlign;
	PARG*			apsPartialDests;
	PARG			asNewDests;
	IMG_PUINT32		auPartialDestChansToCopy;
} PARTIAL_DEST_PARAMETERS, *PPARTIAL_DEST_PARAMETERS;

static
IMG_VOID ReplacePartialDestSources(PINTERMEDIATE_STATE		psState,
								   PINST					psInst,
								   IMG_UINT32				uGroupIdx,
								   PREGISTER_GROUP_DESC		psGroupDesc,
								   PPARTIAL_DEST_PARAMETERS	psDestParams)
/*****************************************************************************
 FUNCTION	: ReplacePartialDestSources

 PURPOSE	: Where a group of sources to an instruction are the same as the
			  sources for a group of partially written destinations then replace
			  the sources by the temporary registers containing a copy of their data.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.
			  uGroupIdx			- Details of the group of arguments.
			  psGroupDesc
			  psDestParams		- Information about the partial destinations of
								the instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32					uGroup;
	IMG_UINT32					uMatchStart;
	IMG_UINT32					uMatchRange;
	HWREG_ALIGNMENT				eMatchStartAlign;
	REGISTER_GROUP_DESC			sSwizzledGroupDesc;
	
	PVR_UNREFERENCED_PARAMETER(uGroupIdx);

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) != 0)
	{
		/*
			Check if we could remove some of the restrictions by modifying the instruction's
			swizzle.
		*/
		AdjustRegisterGroupForSwizzle(psState, psInst, uGroupIdx, &sSwizzledGroupDesc, psGroupDesc);
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		sSwizzledGroupDesc = *psGroupDesc;
	}

	/*
		Check the group of sources and destinations have the same restrictions.
	*/
	if (sSwizzledGroupDesc.uCount > psDestParams->uDestGroupCount)
	{
		return;
	}

	/*
		Check the group of sources is the same as the source for the partially
		written destinations.
	*/
	uMatchStart = USC_UNDEF;
	eMatchStartAlign = sSwizzledGroupDesc.eAlign;
	for (uGroup = 0; uGroup < psDestParams->uDestGroupCount; uGroup++)
	{
		if (psDestParams->apsPartialDests[uGroup] == NULL)
		{
			return;
		}
		if (EqualArgs(&psInst->asArg[sSwizzledGroupDesc.uStart], psDestParams->apsPartialDests[uGroup]))
		{
			uMatchStart = uGroup;
			break;
		}
		eMatchStartAlign = g_aeOtherAlignment[eMatchStartAlign];
	}
	if (uMatchStart == USC_UNDEF)
	{
		return;
	}

	if (!AreAlignmentsCompatible(eMatchStartAlign, psDestParams->eDestGroupAlign))
	{
		return;
	}

	uMatchRange = min(sSwizzledGroupDesc.uCount, psDestParams->uDestGroupCount - uMatchStart);
	for (uGroup = 0; uGroup < uMatchRange; uGroup++)
	{
		PARG	psSrc = &psInst->asArg[sSwizzledGroupDesc.uStart + uGroup];

		if (psDestParams->apsPartialDests[uGroup] == NULL)
		{
			return;
		}
		if (!EqualArgs(psSrc, psDestParams->apsPartialDests[uMatchStart + uGroup]))
		{
			return;
		}
	}

	/*
		Replace the sources by the temporaries.
	*/
	for (uGroup = 0; uGroup < uMatchRange; uGroup++)
	{
		IMG_UINT32	uSrc = sSwizzledGroupDesc.uStart + uGroup;
		PARG		psNewDest = &psDestParams->asNewDests[uMatchStart + uGroup];

		SetSrc(psState,		
			   psInst, 
			   uSrc, 
			   psNewDest->uType, 
			   psNewDest->uNumber, 
			   psNewDest->eFmt);
		psDestParams->auPartialDestChansToCopy[uGroup] |= GetLiveChansInArg(psState, psInst, uSrc);
	}

	/*
		Add any restrictions on the source group to the temporaries.
	*/
	MakeGroup(psState,
			  psInst->asArg + sSwizzledGroupDesc.uStart,
			  uMatchRange,
			  sSwizzledGroupDesc.eAlign);
}

static
IMG_VOID ExpandMasksGroupCB(PINTERMEDIATE_STATE	psState,
						    PINST				psInst,
							IMG_BOOL			bDest,
							IMG_UINT32			uGroupStart,
							IMG_UINT32			uGroupCount,
							HWREG_ALIGNMENT		eGroupAlign,
							IMG_PVOID			pvNULL)
/*****************************************************************************
 FUNCTION	: ExpandMasksGroupCB

 PURPOSE	: Expand a group of partially/conditionally updated destinations
			  so the source for the overwritten data is the same as the destination.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.
			  bDest				-
			  uGroupStart		- Details of the group of arguments.
			  uGroupCount
			  eGroupAlign
			  pvNUL				- Ignored.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uGroupIdx;
	IMG_BOOL	bExpandMasks;

	PVR_UNREFERENCED_PARAMETER(pvNULL);

	ASSERT(bDest);

	bExpandMasks = IMG_FALSE;
	for (uGroupIdx = 0; uGroupIdx < uGroupCount; uGroupIdx++)
	{
		IMG_UINT32	uDestIdx = uGroupStart + uGroupIdx;
		PARG		psOldDest = psInst->apsOldDest[uDestIdx];
		PARG		psNewDest = &psInst->asDest[uDestIdx];

		if (psOldDest != NULL && !EqualArgs(psOldDest, psNewDest))
		{
			bExpandMasks = IMG_TRUE;
			break;
		}
	}

	/*
		Expand
	
			NEWDEST = (CONDITION) ? INST_RESULT : OLDDEST
		->
			MOV		TEMP, OLDDEST
			INST	TEMP, ....
			MOV		NEWDEST, TEMP
	*/
	if (bExpandMasks)
	{
		ARG						asMaskTemp[USC_MAXIMUM_CONSECUTIVE_REGISTER_SET_LENGTH];
		IMG_UINT32				aauChansToCopy[2][USC_MAXIMUM_CONSECUTIVE_REGISTER_SET_LENGTH];
		PARG					aapsArgToCopy[2][USC_MAXIMUM_CONSECUTIVE_REGISTER_SET_LENGTH];
		IMG_UINT32				uIdx;
		#if defined(OUTPUT_USPBIN)
		IMG_BOOL				bShaderResult;
		#endif /* defined(OUTPUT_USPBIN) */
		PARTIAL_DEST_PARAMETERS	sPartialDestParams;

		ASSERT(uGroupCount <= USC_MAXIMUM_CONSECUTIVE_REGISTER_SET_LENGTH);
		
		if (NoPredicate(psState, psInst))
		{
			if (psInst->eOpcode == IPCKC10C10 || psInst->eOpcode == IPCKU8U8)
			{
				ASSERT(uGroupStart == 0);
				ASSERT(uGroupCount == 1);

				/*
					If the partial destination for a PCK instruction is the same as one of the
					sources then we can handle the mask more optimally.
				*/
				if (HandlePCKMask(psState, psInst))
				{
					return;
				}
			}
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			if (psInst->eOpcode == IVPCKC10C10)
			{
				ASSERT(uGroupStart == 0);
				ASSERT(uGroupCount == 1 || uGroupCount == 2);
				if (HandleVPCKMask(psState, psInst, uGroupCount))
				{
					return;
				}
			}
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	
			/*
				Check for cases where different data is copied into the RGB and ALPHA channels
				of the result.
			*/
			if (
					#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
					psInst->eOpcode == IVPCKU8U8 ||
					#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
					psInst->eOpcode == IPCKC10C10 || 
					psInst->eOpcode == IPCKU8U8
				)
			{
				if (ConvertPartialMoveToSOP2(psState, psInst, IMG_TRUE /* bPack */))
				{
					return;
				}
			}
			if (
					#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
					psInst->eOpcode == ISOPWM_VEC ||
					#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
					psInst->eOpcode == ISOPWM
				)
			{
				if (ConvertPartialMoveToSOP2(psState, psInst, IMG_FALSE /* bPack */))
				{
					return;
				}
			}
		}

		#if defined(OUTPUT_USPBIN)
		/*
			Is the partially updated destination a shader result?
		*/
		bShaderResult = IMG_FALSE;
		if ((psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) != 0)
		{
			if ((psInst->uShaderResultHWOperands & (1U << 0)) != 0)
			{
				bShaderResult = IMG_TRUE;
				psInst->uShaderResultHWOperands &= ~(1U << 0);
			}
		}
		#endif /* defined(OUTPUT_USPBIN) */

		for (uGroupIdx = 0; uGroupIdx < uGroupCount; uGroupIdx++)
		{
			IMG_UINT32	uDestIdx = uGroupStart + uGroupIdx;
			PARG		psOldDest = psInst->apsOldDest[uDestIdx];
			PARG		psNewDest = &psInst->asDest[uDestIdx];
			PARG		psMaskTemp = &asMaskTemp[uGroupIdx];
			IMG_UINT32	uPreservedChans = GetPreservedChansInPartialDest(psState, psInst, uDestIdx);

			/*
				Allocate a fresh temporary register to replace the instruction's destination.
			*/
			if (psNewDest->uType == USEASM_REGTYPE_PREDICATE)
			{
				ASSERT(psOldDest->uType == USEASM_REGTYPE_PREDICATE || psOldDest->uType == USC_REGTYPE_BOOLEAN);
				MakeNewPredicateArg(psState, psMaskTemp);
			}
			else
			{
				MakeNewTempArg(psState, psNewDest->eFmt, psMaskTemp);
			}

			/*
				If the old destination is also a source then also replace the source by the newly
				allocated temporary register.
			*/
			if (psOldDest != NULL && psOldDest->uType == USEASM_REGTYPE_PREDICATE)
			{
				IMG_UINT32 uPred;
				for (uPred = 0; uPred < psInst->uPredCount; ++uPred)
				{
					if (psInst->apsPredSrc[uPred] != NULL && EqualArgs(psInst->apsPredSrc[uPred], psOldDest))
					{
						SetPredicateAtIndex(psState, psInst, psMaskTemp->uNumber, (GetBit(psInst->auFlag, INST_PRED_NEG)) ? IMG_TRUE : IMG_FALSE, uPred);
					}
				}
			}

			/*
				Get the mask of channels to copy into the original destination.
			*/
			aauChansToCopy[0][uGroupIdx] = psInst->auLiveChansInDest[uDestIdx];
			aapsArgToCopy[0][uGroupIdx] = psNewDest;

			/*
				Get the mask of channels to copy from the source for unwritten
				channels.
			*/
			if (psOldDest != NULL)
			{
				aauChansToCopy[1][uGroupIdx] = uPreservedChans;
			}
			else
			{
				aauChansToCopy[1][uGroupIdx] = 0;
			}
			aapsArgToCopy[1][uGroupIdx] = psOldDest;
		}

		/*
			If the old destination is also a source then also replace the source by the newly
			allocated temporary register.
		*/
		if (aapsArgToCopy[1][0] != NULL &&
			aapsArgToCopy[1][0]->uType == USEASM_REGTYPE_TEMP &&
			(g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_DESTANDSRCOVERLAP) == 0)
		{
			REGISTER_GROUPS_DESC	sDesc;
			IMG_UINT32				uGroup;

			GetSourceRegisterGroups(psState, psInst, &sDesc);

			sPartialDestParams.uDestGroupStart = uGroupStart;
			sPartialDestParams.uDestGroupCount = uGroupCount;
			sPartialDestParams.eDestGroupAlign = eGroupAlign;
			sPartialDestParams.apsPartialDests = aapsArgToCopy[1];
			sPartialDestParams.asNewDests = asMaskTemp;
			sPartialDestParams.auPartialDestChansToCopy = aauChansToCopy[1];

			if (sDesc.uCount > 0)
			{
				for (uGroup = 0; uGroup < sDesc.uCount; uGroup++)
				{
					ReplacePartialDestSources(psState, psInst, uGroup, &sDesc.asGroups[uGroup], &sPartialDestParams);
				}
			}
			else
			{
				for (uGroup = 0; uGroup < psInst->uArgumentCount; uGroup++)
				{
					REGISTER_GROUP_DESC	sOneArgDesc;

					sOneArgDesc.uStart = uGroup;
					sOneArgDesc.uCount = 1;
					sOneArgDesc.eAlign = HWREG_ALIGNMENT_NONE;
					ReplacePartialDestSources(psState, psInst, uGroup, &sOneArgDesc, &sPartialDestParams);
				}
			}
		}

		/*
			Create instructions to copy the data in to/out of the replacement destination.
		*/
		for (uIdx = 0; uIdx < 2; uIdx++)
		{
			INSERT_LOCATION	eLocation;

			eLocation = (uIdx == 0) ? INSERT_LOCATION_AFTER : INSERT_LOCATION_BEFORE;

			CreateMaskCopyInsts(psState,
								eLocation,
								asMaskTemp,
								aapsArgToCopy[uIdx],
								aauChansToCopy[uIdx],
								psInst,
								#if defined(OUTPUT_USPBIN)
								bShaderResult,
								#endif /* defined(OUTPUT_USPBIN) */
								uGroupStart,
								uGroupCount);
		}

		/*
			If the instruction destinations require consecutive hardware register numbers then apply
			those restrictions to the replacement temporaries.
		*/
		if (uGroupCount > 1 || eGroupAlign != HWREG_ALIGNMENT_NONE)
		{
			MakeGroup(psState, 
					  psInst->asDest + uGroupStart,
					  uGroupCount,
					  eGroupAlign);
		}
	}
}

static
IMG_VOID ExpandMasksBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, IMG_PVOID pvNULL)
/*****************************************************************************
 FUNCTION	: ExpandMasksBP

 PURPOSE	: Expand instructions with partially/conditionally updated destinations
			  so the source for the overwritten data is the same as the destination.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to expand masked instructions within.
			  pvNULL			- Ignored.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psInst;

	PVR_UNREFERENCED_PARAMETER(pvNULL);

	for (psInst = psCodeBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		ProcessDestRegisterGroups(psState, psInst, ExpandMasksGroupCB, NULL /* pvContext */);
	}
}

static
IMG_VOID ExpandStoresBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, IMG_PVOID pvNULL)
/*****************************************************************************
 FUNCTION	: ExpandStoresBP

 PURPOSE	: Expand stores which update the address.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to expand store instructions within.
			  pvNULL			- Ignored.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psInst;

	PVR_UNREFERENCED_PARAMETER(pvNULL);

	for (psInst = psCodeBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		/*
			Expand
	
				NEWADDR = MEMST ADDR --/++OFFSET SRC
			->
				MOV		TEMP, ADDR
				MEMST	TEMP --/++OFFSET SRC
				MOV		NEWADDR, TEMP
		*/

		if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_MEMSTORE) &&
			(g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_ABSOLUTELOADSTORE) &&
			psInst->u.psLdSt->bPreIncrement)
		{
			ARG		sTemp;
			PINST	psAfterMovInst;
			PINST	psBeforeMovInst;

			MakeNewTempArg(psState, UF_REGFORMAT_F32, &sTemp);

			/*
				Move from the new temporary to the original destination for the incremented
				base address.
			*/
			psAfterMovInst = AllocateInst (psState, IMG_NULL);
			SetOpcode(psState, psAfterMovInst, IMOV);
			SetDestFromArg(psState, psAfterMovInst, 0 /* uSrcIdx */, &psInst->asDest[1]);
			SetSrcFromArg(psState, psAfterMovInst, 0 /* uDestIdx */, &sTemp);
			InsertInstAfter(psState, psCodeBlock, psAfterMovInst, psInst);
			
			/*
				Move from the original input base address to the new temporary.
			*/
			psBeforeMovInst = AllocateInst (psState, IMG_NULL);
			SetOpcode(psState, psBeforeMovInst, IMOV);
			SetDestFromArg(psState, psBeforeMovInst, 0 /* uDestIdx */, &sTemp);
			SetSrcFromArg(psState, psBeforeMovInst, 0 /* uSrcIdx */, &psInst->asArg[0]);
			InsertInstBefore(psState, psCodeBlock, psBeforeMovInst, psInst);
	
			/*
				Set the new temporary as both the input base address and the destination
				for the incremented base address.
			*/
			SetDestFromArg(psState, psInst, 1 /* uDestIdx */, &sTemp);
			SetSrcFromArg(psState, psInst, 0 /* uSrcIdx */, &sTemp);
		}
	}
}

static 
IMG_VOID UpdateSecondaryAttributeCount(PINTERMEDIATE_STATE psState, IMG_UINT32 uAssignedPrimaryAttribute)
/*****************************************************************************
 FUNCTION	: UpdateSecondaryAttributeCount

 PURPOSE	: Update the count of secondary attributes used by the program
			  for driver loaded secondary attributes/results of the secondary
			  update program.

 PARAMETERS	: psState			- Compiler state.
			  uAssignedPrimaryAttribute
								- Used primary attribute.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uOffsetInRange;
	PSAPROG_STATE	psSAProg = &psState->sSAProg;

	ASSERT(uAssignedPrimaryAttribute >= psState->psSAOffsets->uInRegisterConstantOffset);
	ASSERT(uAssignedPrimaryAttribute < (psState->psSAOffsets->uInRegisterConstantOffset + psState->psSAOffsets->uInRegisterConstantLimit));

	/*
		Get the offset of the secondary attribute assigned within the range of secondary attributes
		given to us by the driver.
	*/
	uOffsetInRange = 
		uAssignedPrimaryAttribute - psState->psSAOffsets->uInRegisterConstantOffset;
			
	/*
		Update the count of secondary attributes used from the range.
	*/
	psSAProg->uConstSecAttrCount = max(psSAProg->uConstSecAttrCount, uOffsetInRange + 1);
}

static
IMG_VOID AssignRegistersOffLineFuncGroup(PINTERMEDIATE_STATE psState, PSPILL_STATE psSpillState, FUNCGROUP eFuncGroup)
/*****************************************************************************
 FUNCTION	: AssignRegistersOffLineFuncGroup

 PURPOSE	: Assign hardware registers for the instructions in a group of functions.

 PARAMETERS	: psState			- Compiler state.
			  psSpilState		- Information about the spill area in memory.
			  eFuncGroup		- Group of functions to assign registers for.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PRAGCOL_STATE psRegState;
	IMG_BOOL bRestart;
	IMG_UINT32 uPass;
	IMG_BOOL bSpilled;
	IMG_BOOL bFormOptRegGroup = IMG_TRUE;
	IMG_BOOL bAddedExtraPrimaryAttribute = IMG_FALSE;
	USC_LIST sMOVToPCKList;

	/*
		Temporarily convert some MOV instructions to PCKU8U8 or
		PCKC10C10.
	*/
	InitializeList(&sMOVToPCKList);
	DoOnAllFuncGroupBasicBlocks(psState,
								ANY_ORDER,
								ConvertMOVToPCKBP,
								IMG_FALSE,
								&sMOVToPCKList,
								eFuncGroup);

	/*
	  Flag that we need to allocate state for information about registers on
	  the first pass.
	*/
	psRegState = NULL;

	/*
	 * Graph colouring
	 */
	uPass = 0;
	do
	{
		/*
		  Reset register allocation state.
		*/
        ResetRAGColState(psState, &psRegState, eFuncGroup);
		psRegState->psMOVToPCKList = &sMOVToPCKList;
		psRegState->psSpillState = psSpillState;

		/* Setup the register allocator state */
		PrepRegState(psState, psRegState, eFuncGroup);
		
		/*
		  Try to coalesce registers to get rid of MOV instructions.
		*/
		if (uPass == 0)
		{
			for (;;)
			{
				COALESCING_ARG sCoalesceArg;
				sCoalesceArg.psRegState = psRegState;

				DoOnAllFuncGroupBasicBlocks(psState,
					ANY_ORDER,
					CoalesceRegistersForMOVsBP,
					IMG_FALSE,
					&sCoalesceArg,
					eFuncGroup);

				TESTONLY_PRINT_INTERMEDIATE("After coalescing", "coalesce", IMG_TRUE);

				if (sCoalesceArg.bRemovedInst)
				{
					/* Reset register allocation state. */
					ResetRAGColState(psState, &psRegState, eFuncGroup);
					PrepRegState(psState, psRegState, eFuncGroup);
				}
				else
				{
					break;
				}
			}
		}

		/*
		 * Construct optional register groups
		 */
		if (bFormOptRegGroup)
		{
			DoOnAllFuncGroupBasicBlocks(psState,
										ANY_ORDER,
										FormOptRegGroupBP,
										IMG_FALSE,
										&psRegState->sRAData,
										eFuncGroup);
		}

		/*
			Print register groups.
		*/
		TESTONLY(PrintRegisterGroups(psState));

		CheckGPIFlags(psState, psRegState, eFuncGroup);

		/*
		 * Assign registers
		 */

		AssignColours(psState,
					  psRegState,
					  &bRestart,
					  &bAddedExtraPrimaryAttribute);

		/*
		  Did graph colouring fail?
		*/
		bSpilled = IMG_FALSE;
		if (!IsListEmpty(&psRegState->sUncolouredList))
		{
			if (uPass == 0)
			{
				/* Don't spill yet: try again without optional register groups. */
				bFormOptRegGroup = IMG_FALSE;
			}
			else
			{
				/* Spill the registers that can't be coloured. */
				SpillPlainNodes(psState, psRegState);
			}
			bSpilled = IMG_TRUE;
		}

		uPass++;
	} while (bSpilled || bRestart);

	/* Rename the register arrays */
	if (psState->apsVecArrayReg != NULL)
	{
		IMG_UINT32	uIdx;

		for (uIdx = 0; uIdx < psState->uNumVecArrayRegs; uIdx ++)
		{
			PUSC_VEC_ARRAY_REG	psVecArrayReg = psState->apsVecArrayReg[uIdx];
			IMG_UINT32			uLastRegNum;
			IMG_BOOL			bUnusedArray;

			if (psVecArrayReg == NULL)
				continue;

			/*
				Skip arrays which aren't assigned by this pass of the register
				allocator (either arrays of secondary attributes when assigned
				for the main program or vice-versa).
			*/
			if (psVecArrayReg->eArrayType == ARRAY_TYPE_DIRECT_MAPPED_SECATTR ||
				psVecArrayReg->eArrayType == ARRAY_TYPE_DRIVER_LOADED_SECATTR)
			{
				if (eFuncGroup == FUNCGROUP_MAIN)
				{
					continue;
				}
			}
			else
			{
				if (eFuncGroup != FUNCGROUP_MAIN)
				{
					continue;
				}
			}

			/*
				Check for an array which is completely unreferenced.
			*/
			bUnusedArray = IMG_FALSE;
			if (psVecArrayReg->uRegs == 0)
			{
				bUnusedArray = IMG_TRUE;
			}
			else
			{
				IMG_UINT32	uBaseNode;

				uBaseNode = RegisterToNode(&psRegState->sRAData, psVecArrayReg->uRegType, psVecArrayReg->uBaseReg);
				if (GetBit(psRegState->asNodes[uBaseNode].auFlags, NODE_FLAG_REFERENCED) == 0)
				{
					bUnusedArray = IMG_TRUE;
				}
			}

			/*
				Set the start of the hardware registers for an unreferenced array to
				dummy values.
			*/
			if (bUnusedArray)
			{
				psVecArrayReg->uRegType = USC_REGTYPE_DUMMY;
				psVecArrayReg->uBaseReg = USC_UNDEF;
				continue;
			}

			/*
				Get the hardware register assigned to the first element in the array.
			*/
			RenameReg(psRegState, &psVecArrayReg->uRegType, &psVecArrayReg->uBaseReg, IMG_TRUE);

			uLastRegNum = psVecArrayReg->uBaseReg + psVecArrayReg->uRegs;
			if (psVecArrayReg->uRegType == USEASM_REGTYPE_TEMP)
			{
				/*
					Set the temporary register count to at least the
					last register in the array.
				*/
				psRegState->uTemporaryRegisterCount =
					max(psRegState->uTemporaryRegisterCount, uLastRegNum);
			}
			else if (psVecArrayReg->uRegType == USEASM_REGTYPE_PRIMATTR)
			{
				psRegState->uPrimaryAttributeCount = 
					max(psRegState->uPrimaryAttributeCount, uLastRegNum);
			}
		}
	}

	/*
		Flag to later stages that we've assigned registers for the
		secondary update program.
	*/
	if (eFuncGroup == FUNCGROUP_SECONDARY)
	{
		psState->uFlags |= USC_FLAGS_ASSIGNEDSECPROGREGISTERS;
	}
	else
	{
		psState->uFlags2 |= USC_FLAGS2_ASSIGNED_PRIMARY_REGNUMS;
	}

	/* Rename from input registers to hardware registers. */
	DoOnAllFuncGroupBasicBlocks(psState,
								ANY_ORDER,
								RenameRegistersBP,
								IMG_FALSE,
								psRegState,
								eFuncGroup);

	if (eFuncGroup == FUNCGROUP_MAIN)
	{
		PUSC_LIST_ENTRY	psListEntry;

		/*
			Update information about the location of the shader outputs.
		*/
		for (psListEntry = psState->sFixedRegList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
		{
			PFIXED_REG_DATA psOut = IMG_CONTAINING_RECORD(psListEntry, PFIXED_REG_DATA, sListEntry);		
			IMG_UINT32		uConsecutiveRegIdx;

			if (psOut->uVRegType != USEASM_REGTYPE_TEMP)
			{
				continue;
			}

			for (uConsecutiveRegIdx = 0; uConsecutiveRegIdx < psOut->uConsecutiveRegsCount; uConsecutiveRegIdx++)
			{
				IMG_UINT32	uNode;
				PCOLOUR		psColour;
				IMG_UINT32	uColourType, uColourNum;

				uNode = RegisterToNode(&psRegState->sRAData, psOut->uVRegType, psOut->auVRegNum[uConsecutiveRegIdx]);
				ASSERT(psRegState->asNodes[uNode].uColourCount >= 1);
				psColour = &psRegState->asNodes[uNode].asColours[0];
				ColourToRegister(&psRegState->sRAData, psColour, &uColourType, &uColourNum);

				ASSERT(uColourType == psOut->sPReg.uType);
				if (psOut->sPReg.uNumber == USC_UNDEF)
				{
					ASSERT(psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL);
					ASSERT(psState->sShader.psPS->psFixedHwPARegReg == psOut);
					ASSERT(uColourNum < psState->sHWRegs.uNumPrimaryAttributes);

					if (uConsecutiveRegIdx == 0)
					{
						psOut->sPReg.uNumber = uColourNum;
					}
				}
				else
				{
					ASSERT(uColourType == USC_REGTYPE_UNUSEDDEST || uColourNum == (psOut->sPReg.uNumber + uConsecutiveRegIdx));
				}
			}

			/*
				Update the virtual registers to the same as the physical registers.
			*/
			psOut->uVRegType = psOut->sPReg.uType;
			for (uConsecutiveRegIdx = 0; uConsecutiveRegIdx < psOut->uConsecutiveRegsCount; uConsecutiveRegIdx++)
			{
				psOut->auVRegNum[uConsecutiveRegIdx] = psOut->sPReg.uNumber + uConsecutiveRegIdx;
			}
		}	
	}
	else
	{
		PREGISTER_LIVESET	psSAOutputs;
		PUSC_LIST_ENTRY		psListEntry;
		PSAPROG_STATE		psSAProg = &psState->sSAProg;

		ASSERT(eFuncGroup == FUNCGROUP_SECONDARY);

		/*
			Replace temporary registers in the main program by the actual secondary attribute
			assigned to hold the result in the secondary update program.
		*/
		RenameSecondaryAttributes(psState, psRegState);

		/*
			Update the count of secondary attributes used to reflect secondary attributes used for
			temporary data in the secondary update program.
		*/
		if (psRegState->uPrimaryAttributeCount >= psState->psSAOffsets->uInRegisterConstantOffset)
		{
			IMG_UINT32	uSAsUsedFromConstRange;

			/*
				Make the count relative to the range of secondary attributes reserved for us by 
				the driver.
			*/
			uSAsUsedFromConstRange = 
				psRegState->uPrimaryAttributeCount - psState->psSAOffsets->uInRegisterConstantOffset;
			uSAsUsedFromConstRange = min(uSAsUsedFromConstRange, psState->psSAOffsets->uInRegisterConstantLimit);

			psSAProg->uConstSecAttrCount = uSAsUsedFromConstRange;
		}
		else
		{
			psSAProg->uConstSecAttrCount = 0;
		}

		/*
			Include all secondary attributes we are asking the driver to load (even if they are otherwise 
			unreferenced).
		*/
		for (psListEntry = psState->sSAProg.sInRegisterConstantList.psHead; 
			 psListEntry != NULL; 
			 psListEntry = psListEntry->psNext)
		{
			PINREGISTER_CONST	psConst = IMG_CONTAINING_RECORD(psListEntry, PINREGISTER_CONST, sListEntry);

			UpdateSecondaryAttributeCount(psState, psConst->psResult->psFixedReg->sPReg.uNumber);
		}

		psSAOutputs = &psState->psSecAttrProg->sCfg.psExit->sRegistersLiveOut;
		for (psListEntry = psSAProg->sResultsList.psHead; 
			 psListEntry != NULL; 
			 psListEntry = psListEntry->psNext)
		{
			PSAPROG_RESULT	psResult = IMG_CONTAINING_RECORD(psListEntry, PSAPROG_RESULT, sListEntry);
			PFIXED_REG_DATA	psFixedReg = psResult->psFixedReg;
			IMG_UINT32		uTempRegNum = psFixedReg->auVRegNum[0];
			IMG_UINT32		uAssignedPrimaryAttribute;
			IMG_UINT32		uOldMask;
			PUSEDEF_CHAIN	psSAUseDef;

			/*
				Get the mask of channels live in this result on output from the secondary update
				program.
			*/
			uOldMask = 
				GetRegisterLiveMask(psState,
									psSAOutputs,
									USEASM_REGTYPE_TEMP,
									uTempRegNum,
									0 /* uArrayOffset */);

			/*
				Clear the live-mask for the temporary register as it has now been mapped to a
				hardware register by the register allocator.
			*/
			SetRegisterLiveMask(psState,
								psSAOutputs,
								USEASM_REGTYPE_TEMP,
								uTempRegNum,
								0 /* uArrayOffset */,
								0 /* uMask */);

			/*
				Get the secondary attribute assigned to this result.
			*/
			uAssignedPrimaryAttribute = RenameSAProgResult(psRegState, psResult);

			psSAUseDef = UseDefGet(psState, psFixedReg->uVRegType, psFixedReg->auVRegNum[0]);
			if (uOldMask != 0 && psSAUseDef->psDef != NULL)
			{
				UpdateSecondaryAttributeCount(psState, uAssignedPrimaryAttribute);
			}

			/*
				Flag this secondary attribute as live on exit from the secondary
				update program.
			*/
			IncreaseRegisterLiveMask(psState,
									 psSAOutputs,
									 USEASM_REGTYPE_PRIMATTR,
									 uAssignedPrimaryAttribute,
									 0 /* uArrayOffset */,
									 uOldMask);
		}
	}

#if defined(UF_TESTBENCH)
	/* Print the register mapping */
	PrintNodeColoursGC(psState, psRegState);
#endif /* defined(UF_TESTBENCH) */

	/*
		Store the number of temporary registers used into the state.
	*/
	if (eFuncGroup == FUNCGROUP_MAIN)
	{
		psState->uTemporaryRegisterCount = psRegState->uTemporaryRegisterCount;
		psState->uTemporaryRegisterCountPostSplit = psRegState->uTemporaryRegisterCountPostSplit;
	}
	else
	{
		psState->uSecTemporaryRegisterCount = psRegState->uTemporaryRegisterCount;
		ASSERT(!((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_UNIFIED_TEMPS_AND_PAS) && psState->uSecTemporaryRegisterCount > 0));
	}

	/*
	  Free state memory.
	*/
	FreeRAGColState(psState, &psRegState, IMG_TRUE);

	{
		PUSC_LIST_ENTRY	psListEntry;
		for (psListEntry = sMOVToPCKList.psHead;
			 psListEntry != NULL;
			 psListEntry = psListEntry->psNext)
		{
			PINST	psInst = IMG_CONTAINING_RECORD(psListEntry, PINST, sAvailableListEntry);

			if (psInst->asDest[0].uType == USC_REGTYPE_UNUSEDDEST)
			{
				RemoveFromList(&sMOVToPCKList, &psInst->sAvailableListEntry);
			}
		}
	}

	/*
		Update information about register liveness.
	*/
	if (eFuncGroup == FUNCGROUP_MAIN)
	{
		DeadCodeElimination(psState, IMG_TRUE);
	}
	else
	{
		DoOnCfgBasicBlocks(psState,
						   &(psState->psSecAttrProg->sCfg),
						   ANY_ORDER,
						   DeadCodeEliminationBP,
						   IMG_FALSE,
						   (IMG_PVOID)(IMG_UINTPTR_T)IMG_TRUE);
	}

	/*
		For any instructions which we temporarily converted from MOV to
		PCKC10C10 or PCKU8U8 reset them.
	*/
	ConvertPCKToMOV(psState, &sMOVToPCKList);
}

static
IMG_VOID AssignRegistersOffLine(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: AssignRegistersOffLine

 PURPOSE	: Assign hardware registers for the instructions in the program.

 PARAMETERS	: psState			- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	SPILL_STATE	sSpillState;

	/*
	  By default no spill data required.
	*/
	psState->uSpillAreaSize = 0;

	/*
		Initialize information about the spill area in memory.
	*/
	sSpillState.psSpillAreaSizeLoader = NULL;
	if (psState->psTargetBugs->ui32Flags & (SGX_BUG_FLAGS_FIX_HW_BRN_24895 | SGX_BUG_FLAGS_FIX_HW_BRN_30795))
	{
		sSpillState.bSpillUseImmediateOffsets = IMG_FALSE;
	}
	else
	{
		sSpillState.bSpillUseImmediateOffsets = IMG_TRUE;
	}
	sSpillState.uSpillAreaOffsetsSACount = 0;
	sSpillState.asSpillAreaOffsetsSANums = NULL;
	InitializeList(&sSpillState.sSpillInstList);

	/*
		Adjust the base address of the indexable temp base.
	*/
	if ((psState->uCompilerFlags & UF_EXTRACTCONSTANTCALCS) != 0 &&
		(psState->uFlags & USC_FLAGS_INDEXABLETEMPS_USED) != 0 &&
		psState->uIndexableTempArraySize > 0)
	{
		PINST	psLimmInst;

		/*
			Generate code to calculate
				INDEXABLE_TEMP_BASE = INDEXABLE_TEMP_BASE + PIPE_NUMBER * STRIDE
		*/
		psLimmInst = 
			AdjustBaseAddress(psState,
						  psState->psSAOffsets->uIndexableTempBase);

		/*
			Set the stride in the above calculation to the size of the indexable temporary
			area required per-instance.
		*/
		ASSERT(psLimmInst->asArg[0].uType == USEASM_REGTYPE_IMMEDIATE);
		psLimmInst->asArg[0].uNumber = EURASIA_MAX_USE_INSTANCES * psState->uIndexableTempArraySize;
	}

	/*
		Assign registers for the main program.
	*/
	AssignRegistersOffLineFuncGroup(psState, &sSpillState, FUNCGROUP_MAIN);

	/*
		Assign registers for the secondary update program.
	*/
	AssignRegistersOffLineFuncGroup(psState, &sSpillState, FUNCGROUP_SECONDARY);

	/* Spill area settings */
	if (psState->uSpillAreaSize > 0)
	{
		SetupSpillArea(psState, &sSpillState);
	}

	/*
		Flag to later stages that hardware registers have been assigned.
	*/
	psState->uFlags2 |= USC_FLAGS2_ASSIGNED_TEMPORARY_REGNUMS;
}
#endif /* defined(REGALLOC_GCOL) */

#if defined(REGALLOC_LSCAN)
/**************************************************************************
 *
 * Linear Scan Allocator
 *
 **************************************************************************/

/***
 * Function definitions
 ***/
#if 0
static
IMG_BOOL NodeIsFixedColour(PINTERMEDIATE_STATE psState,
						   PREGALLOC_DATA psRAData,
						   IMG_UINT32 uNode)
/*****************************************************************************
 FUNCTION	: NodeIsFixedColour

 PURPOSE	: Is a node one which has a fixed colour.

 PARAMETERS	: psState			- Compiler state.
			  psRegState		- Register allocation state.
			  uNode				- The node to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (uNode < psRAData->uTempStart)
	{
		return IMG_TRUE;
	}
	else
	{
		IMG_UINT32	uReg = uNode - psRAData->uTempStart;
		IMG_UINT32	uColOutputCount;
		if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL)
		{
			if (psState->psSAOffsets->uPackDestType == USEASM_REGTYPE_TEMP)
			{
				if (psState->uCompilerFlags & UF_PACKPSOUTPUT)
				{
					uColOutputCount = 1;
				}
				else
				{
					uColOutputCount = psState->uColOutputCount;
				}
				if (uReg < uColOutputCount)
				{
					return IMG_TRUE;
				}
				if (psState->uFlags & USC_FLAGS_OMASKFEEDBACKPRESENT)
				{
					if (uReg < (uColOutputCount + 1))
					{
						return IMG_TRUE;
					}
				}
			}
			if ((psState->uFlags & USC_FLAGS_DEPTHFEEDBACKPRESENT) &&
				uReg == psState->uDepthOutputNum)
			{
				return IMG_TRUE;
			}
		}
		else if(psState->psSAOffsets->eShaderType != USC_SHADERTYPE_GEOMETRY)
		{
			if ((psState->uFlags & USC_FLAGS_VSCLIPPOS_USED) && uReg < 4)
			{
				return IMG_TRUE;
			}
		}
		return IMG_FALSE;
	}
}
#endif

static
IMG_VOID GetNodeActivity(PUSC_REG_INTERVAL psInterval,
						 IMG_UINT32 uUpper,
						 IMG_UINT32 uLower,
						 IMG_PUINT32 puNumerator,
						 IMG_PUINT32 puDenominator)
/*****************************************************************************
 FUNCTION	  : GetNodeActivity

 PURPOSE	  : Calculate activity of a node over its life-time.

 PARAMETERS	  : psInterval     - Node interval
                uUpper         - Upper bound on interval
                uLower         - Lower bound on interval
				puNumerator	   - Returns the numerator of the number
							   representing node activity.
			    puDenominator  - Returns the denominator of the number
							   representing node activity.

 RETURNS	  : Nothing.
*****************************************************************************/
{
	IMG_UINT32 uMax, uMin;

	uMin = max(uLower, psInterval->uEnd);
	uMax = min(uUpper, psInterval->uStart);

	uMin = min(uMin, uMax);
	uMax = max(uMin, uMax);

	*puNumerator = max(uMax - uMin, 1);
	*puDenominator = max(psInterval->uAccessCtr, 1);
}

IMG_INTERNAL
IMG_INT32 CmpNodeActivity(PUSC_REG_INTERVAL	psInterval1,
						  PUSC_REG_INTERVAL	psInterval2,
						  IMG_UINT32 uUpper,
						  IMG_UINT32 uLower)
/*****************************************************************************
 FUNCTION	  : CmpNodeActivity

 PURPOSE	  : Compare the activity of two nodes over their life-time.

 PARAMETERS	  : psInterval1    - Node intervals to compare.
				psInterval2
                uUpper         - Upper bound on intervals
                uLower         - Lower bound on intervals

 RETURNS	  : -1 if interval 1 is less busy than interval 2.
				 0 if both intervals are equally busy.
				 1 if interval 1 is busier than interval 2.
*****************************************************************************/
{
	IMG_UINT32	uNumerator1, uDenominator1;
	IMG_UINT32	uNumerator2, uDenominator2;

	GetNodeActivity(psInterval1, uUpper, uLower, &uNumerator1, &uDenominator1);
	GetNodeActivity(psInterval2, uUpper, uLower, &uNumerator2, &uDenominator2);

	uNumerator1 *= uDenominator2;
	uNumerator2 *= uDenominator1;

	if (uNumerator1 == uNumerator2)
	{
		return 0;
	}
	else if (uNumerator1 < uNumerator2)
	{
		return -1;
	}
	else
	{
		return 1;
	}
}

IMG_INTERNAL
IMG_FLOAT NodeActivity(PUSC_REG_INTERVAL psInterval,
					   IMG_UINT32 uUpper,
					   IMG_UINT32 uLower)
/*****************************************************************************
 FUNCTION	  : NodeActivity

 PURPOSE	  : Calculate activity of a node over its life-time.

 PARAMETERS	  : psRAData     - Allocator state
                psInterval     - Node interval
                uUpper         - Upper bound on interval
                uLower         - Lower bound on interval

 RETURNS	  : Number of lines/accesses of node
*****************************************************************************/
{
	IMG_UINT32	uNumerator, uDenominator;
	IMG_FLOAT	fNumerator, fDenominator;

	GetNodeActivity(psInterval, uUpper, uLower, &uNumerator, &uDenominator);

	fNumerator = (IMG_FLOAT)uNumerator;
	fDenominator = (IMG_FLOAT)uDenominator;

	return (fNumerator / fDenominator);
}

IMG_INTERNAL
IMG_VOID InitRegInterval(PUSC_REG_INTERVAL psInterval,
                         IMG_UINT32 uNode)
/*****************************************************************************
 FUNCTION   : InitRegInterval

 PURPOSE    : Initialise a register interval

 PARAMETERS : psRegState    - Register allocator state
              psInterval    - Interval to intialise
              uNode         - Node to set

 RETURNS    : Nothing.
*****************************************************************************/
{
    memset(psInterval, 0, sizeof(*psInterval));

    psInterval->uStart = USC_OPEN_START;
    psInterval->uEnd = USC_OPEN_END;

    psInterval->uColour = USC_UNDEF_COLOUR;
    psInterval->uNode = uNode;
    psInterval->uBaseNode = USC_UNDEF_COLOUR;
    psInterval->uCount = 1;
}

IMG_INTERNAL
IMG_VOID DeleteRegInterval(PINTERMEDIATE_STATE psState,
						   PUSC_REG_INTERVAL psInterval)
/*****************************************************************************
 FUNCTION   : DeleteRegInterval

 PURPOSE    : Delete a register interval

 PARAMETERS : psState       - Compiler state
              psInterval    - Interval to clear

 RETURNS    : Nothing.
*****************************************************************************/
{
	UscFree(psState, psInterval);
}

IMG_INTERNAL
IMG_VOID FreeRegIntervalList(PINTERMEDIATE_STATE psState,
							 PUSC_REG_INTERVAL psInterval)
/*****************************************************************************
 FUNCTION   : FreeRegIntervalList

 PURPOSE    : Free a list of register intervals connected through
              psRegNext/psRegPrev

 PARAMETERS : psState       - Compiler state
              psInterval    - Interval in the list to free

 RETURNS    : Nothing
*****************************************************************************/
{
	PUSC_REG_INTERVAL psCurr, psNext, psPrev;

	if (psInterval == NULL)
		return;

	psPrev = psInterval->psRegPrev;

	for (psCurr = psInterval;
		 psCurr != NULL;
		 psCurr = psNext)
	{
		psNext = psCurr->psRegNext;
		DeleteRegInterval(psState, psCurr);
	}

	for (psCurr = psPrev;
		 psCurr != NULL;
		 psCurr = psNext)
	{
		psNext = psCurr->psRegPrev;
		DeleteRegInterval(psState, psCurr);
	}
}

IMG_INTERNAL
PUSC_REG_INTERVAL MergeIntervals(PUSC_REG_INTERVAL psInterval1,
                                 PUSC_REG_INTERVAL psInterval2)
/*****************************************************************************
 FUNCTION   : MergeIntervals

 PURPOSE    : Try to merge intervals

 PARAMETERS : psInterval1    - First interval and destination of merge.
              psInterval2    - Second interval.

 RETURNS    : psInterval1 extended to include psInterval2.
              NULL if intervals are not compatible;
*****************************************************************************/
{
    if(psInterval1 == NULL)
        return psInterval1;

    if (psInterval2 == NULL)
        return psInterval2;

    /* Test for incompatibilities */
    if (psInterval1->uColour != USC_UNDEF_COLOUR &&
        psInterval2->uColour != USC_UNDEF_COLOUR &&
        psInterval1->uColour != psInterval2->uColour)
    {
        return NULL;
    }

    if (psInterval1->uBaseNode != USC_UNDEF_COLOUR &&
        psInterval2->uBaseNode != USC_UNDEF_COLOUR &&
        psInterval1->uBaseNode != psInterval2->uBaseNode)
    {
        return NULL;
    }

    /* Merge the two intervals */
    if (EarlierThan(psInterval2->uStart, psInterval1->uStart))
    {
        psInterval1->uStart = psInterval2->uStart;
    }
    if (EarlierThan(psInterval1->uEnd, psInterval2->uEnd))
    {
        psInterval1->uEnd = psInterval2->uEnd;
    }
    if (psInterval1->uColour == USC_UNDEF_COLOUR)
    {
        psInterval1->uColour = psInterval2->uColour;
    }
    if (psInterval1->uBaseNode == USC_UNDEF_COLOUR)
    {
        psInterval1->uBaseNode = psInterval2->uBaseNode;
    }

    return psInterval1;
}

IMG_INTERNAL
IMG_BOOL EarlierThan(IMG_UINT32 uPoint1, IMG_UINT32 uPoint2)
/*****************************************************************************
 FUNCTION	: EarlierThan

 PURPOSE	: Ordering between start-points

 PARAMETERS	: uPoint1, uPoint2    - Start-points to compare

 RETURNS	: IMG_TRUE iff uPoint1 appears earlier than uPoint2 in a program
*****************************************************************************/
{
	if (uPoint1 > uPoint2)
		return IMG_TRUE;

	return IMG_FALSE;
}

IMG_INTERNAL
IMG_INT32 IntervalLive(IMG_UINT32 uStart,
					   IMG_UINT32 uEnd,
					   IMG_BOOL bDest,
					   IMG_UINT32 uIdx)
/*****************************************************************************
 FUNCTION	: IntervalLive

 PURPOSE	: Test whether a destination or source is live in an iterval

 PARAMETERS	: uStart, uEnd    - End-points of the interval
              bDest           - Comparing for a destination
              uIdx            - Index to compare

 RETURNS	: 0 if uIdx is in the interval,
              <0 if uIdx is earlier in the program than the interval
              >0 if uIdx is later in the program than the interval

*****************************************************************************/
{
	if (uStart == uIdx)
	{
		if (bDest)
			return 0;
		else
			return (IMG_INT32)-1;
	}
	if (uEnd == uIdx)
	{
		if (bDest)
			return 1;
		else
			return 0;
	}

	if (EarlierThan(uIdx, uStart))
		return (IMG_INT32)-1;
	if (EarlierThan(uEnd, uIdx))
		return 1;

	/* uStart <= uIdx <= uEnd */
	return 0;
}

IMG_INTERNAL
IMG_INT32 IntervalCmp(IMG_UINT32 uStart, IMG_UINT32 uEnd, IMG_UINT32 uIdx)
/*****************************************************************************
 FUNCTION	: IntervalCmp

 PURPOSE	: Test the position of an index relative to an interval

 PARAMETERS	: uStart, uEnd    - End-points of the interval
              uIdx            - Index to compare

 RETURNS	: 0 if uIdx is in the interval,
              <0 if uIdx is earlier in the program than the interval
              >0 if uIdx is later in the program than the interval

*****************************************************************************/
{
	if (EarlierThan(uIdx, uStart))
		return (IMG_INT32)-1;
	if (EarlierThan(uEnd, uIdx))
		return 1;

	/* uStart <= uIdx <= uEnd */
	return 0;
}

static
IMG_BOOL IntervalLessThan(IMG_UINT32 uStart1, IMG_UINT32 uEnd1,
                          IMG_UINT32 uStart2, IMG_UINT32 uEnd2)
/*****************************************************************************
 FUNCTION   : IntervalLessThan

 PURPOSE    : Ordering between intervals for list insertion

 PARAMETERS : uStart1, uEnd1    - First Interval
              uStart2, uEnd2    - Second interval

 RETURNS    : IMG_TRUE iff first interval comes before second interval
*****************************************************************************/
{
    if (EarlierThan(uStart1, uStart2))
    {
        return IMG_TRUE;
    }
    if (uStart1 == uStart2 &&
        (uEnd1 == uEnd2 || EarlierThan(uEnd1, uEnd2)))
    {
        return IMG_TRUE;
    }
    return IMG_FALSE;
}

IMG_INTERNAL
IMG_UINT32 IntervalLength(IMG_UINT32 uStart, IMG_UINT32 uEnd)
/*****************************************************************************
 FUNCTION   : IntervalLength

 PURPOSE    : Length of an interval

 PARAMETERS : uStart, uEnd    - End-points of the interval

 RETURNS    : Length of the interval
*****************************************************************************/
{
    return (uStart - uEnd);
}

IMG_INTERNAL
PUSC_REG_INTERVAL ProcListFindPos(PUSC_REG_INTERVAL psList,
                                  IMG_UINT32 uStart,
                                  IMG_UINT32 uEnd)
/*****************************************************************************
 FUNCTION     : ProcListFindPos

 PURPOSE      : Find the interval preceding the specifed start and end point.

 PARAMETERS   : psList           - List connected through psProcNext/psProcPrev
                uStart, uEnd     - Start and end to search for

  RETURNS      : First list element earlier than specified interval or NULL if
                 interval is earlier than any in the list.
*****************************************************************************/
{
    PUSC_REG_INTERVAL psCurr, psNext, psLast;
    IMG_BOOL bForward;

    if (psList == NULL)
    {
        return NULL;
    }
    /* Set the direction to search in */
    if (IntervalLessThan(psList->uStart, psList->uEnd, uStart, uEnd))
    {
        bForward = IMG_TRUE;
    }
    else
    {
        bForward = IMG_FALSE;
    }

	psNext = NULL;
    psLast = NULL;
    for (psCurr = psList; psCurr != NULL; psCurr = psNext)
    {
        if (bForward)
        {
            if (IntervalLessThan(psCurr->uStart, psCurr->uEnd, uStart, uEnd))
            {
                /* Start is later than current interval, go forward */
                psNext = psCurr->psProcNext;
            }
            else
            {
                break;
            }
        }
        else /* !bForward */
        {
            if (!IntervalLessThan(psCurr->uStart, psCurr->uEnd, uStart, uEnd))
            {
                /* Start is later than current interval, go backward */
                psNext = psCurr->psProcPrev;
            }
            else
            {
                psLast = psCurr;
                break;
            }
        }
        psLast = psCurr;
    }

    if ((!bForward) && psCurr == NULL)
    {
        /* Going backward and ran out of list, interval is earlier than any in list. */
        psLast = NULL;
    }

    return psLast;
}

IMG_INTERNAL
IMG_BOOL IntervalOverlap(IMG_UINT32 uStart1, IMG_UINT32 uEnd1,
						 IMG_UINT32 uStart2, IMG_UINT32 uEnd2)
/*****************************************************************************
 FUNCTION	: IntervalOverlap

 PURPOSE	: Test whether two intervals overlap

 PARAMETERS	: uStart1, uEnd1    - End-points of the first interval
              uStart2, uEnd2    - End-points of the second interval

 RETURNS	: IMG_TRUE iff the intervals overlap
*****************************************************************************/
{
	/* s1 <= s2 < e1 */
	if ((uStart1 == uStart2 || EarlierThan(uStart1, uStart2)) &&
		EarlierThan(uStart2, uEnd1))
	{
		return IMG_TRUE;
	}

	/* s2 <= s1 < e2 */
	if ((uStart2 == uStart1 || EarlierThan(uStart2, uStart1)) &&
		EarlierThan(uStart1, uEnd2))
	{
		return IMG_TRUE;
	}

	/* s1 < e2 <= e1 */
	if ((uEnd1 == uEnd2 || EarlierThan(uEnd2, uEnd1)) &&
		EarlierThan(uStart1, uEnd2))
	{
		return IMG_TRUE;
	}

	/* s2 < e1 <= e2 */
	if ((uEnd2 == uEnd1 || EarlierThan(uEnd1, uEnd2)) &&
		EarlierThan(uStart2, uEnd1))
	{
		return IMG_TRUE;
	}

	return IMG_FALSE;
}

static
PUSC_REG_INTERVAL RegIntvlFindPos(PUSC_REG_INTERVAL psList,
                                  IMG_UINT32 uStart,
                                  IMG_UINT32 uEnd)
/*****************************************************************************
 FUNCTION     : RegIntvlFindPos

 PURPOSE      : Find the interval preceding the specifed start and end point.

 PARAMETERS   : psList           - List connected through psRegNext/psRegPrev
                uStart, uEnd     - Start and end to search for

  RETURNS     : First list element earlier than specified interval or NULL if
                interval is earlier than any in the list.
*****************************************************************************/
{
    PUSC_REG_INTERVAL psCurr, psNext, psLast;
    IMG_BOOL bForward;

    if (psList == NULL)
    {
        return NULL;
    }
    /* Set the direction to search in */
    if (IntervalLessThan(psList->uStart, psList->uEnd, uStart, uEnd))
    {
        bForward = IMG_TRUE;
    }
    else
    {
        bForward = IMG_FALSE;
    }

    psNext = NULL;
    psLast = NULL;
    for (psCurr = psList; psCurr != NULL; psCurr = psNext)
    {
        if (bForward)
        {
            if (IntervalLessThan(psCurr->uStart, psCurr->uEnd, uStart, uEnd))
            {
                /* Start is later than current interval, go forward */
                psNext = psCurr->psRegNext;
            }
            else
            {
                break;
            }
        }
        else /* !bForward */
        {
			if (IntervalLessThan(uStart, uEnd, psCurr->uStart, psCurr->uEnd))
            {
                /* Start is later than current interval, go backward */
                psNext = psCurr->psRegPrev;
            }
            else
            {
                psLast = psCurr;
                break;
            }
        }
        psLast = psCurr;
    }

    if ((!bForward) && psCurr == NULL)
    {
        /* Going backward and ran out of list, interval is earlier than any in list. */
        psLast = NULL;
    }

    return psLast;
}

static
PUSC_REG_INTERVAL RegListFindStart(PUSC_REG_INTERVAL psList)
/*****************************************************************************
 FUNCTION     : ProcListFindStart

 PURPOSE      : Get the start of a list connected through psProcNext/psProcPrev

 PARAMETERS   : psList            - List

  RETURNS     : Start of the list
*****************************************************************************/
{
    if (psList == NULL)
        return NULL;
    for (; psList->psRegPrev != NULL; psList = psList->psRegPrev)
    {
        /* skip */
    }
    return psList;
}

IMG_INTERNAL
PUSC_REG_INTERVAL RegIntvlFindWith(PUSC_REG_INTERVAL psList,
                                   IMG_UINT32 uIdx,
                                   IMG_BOOL bDest)
/*****************************************************************************
 FUNCTION     : RegIntvlFindWith

 PURPOSE      : Find the interval containing an index

 PARAMETERS   : psList           - List of intervals (connected through psRegNext/psRegPrev)
                uIdx             - Index to look for
                bDest            - Whether index is for a write or a read

 OUTPUT       : ppsRet           - Current interval or NULL if no active interval

 RETURNS      : Index into List connected by psRegNext/psRegPrev of nearest interval
*****************************************************************************/
{
    PUSC_REG_INTERVAL psCurr, psNext, psTmp;
    IMG_INT32 iCmp;
    /* Go to the interval before the required point */
    psTmp = RegIntvlFindPos(psList, uIdx, uIdx);
    if (psTmp == NULL)
		psList = RegListFindStart(psList);
	else
        psList = psTmp;

    /* Search for the interval containing the required point */
    psNext = NULL;
    for (psCurr = psList; psCurr != NULL; psCurr = psNext)
    {
        iCmp = IntervalCmp(psCurr->uStart, psCurr->uEnd, uIdx);

        /* Index is unambiguously in interval */
        if (iCmp == 0)
        {
            if (uIdx != psCurr->uStart && uIdx != psCurr->uEnd)
                return psCurr;
            /* Test for first occurence of a write to a node */
            if (bDest && uIdx == psCurr->uStart)
            {
                return psCurr;
            }

            /* Test for last occurence of a read to a node */
            if ((!bDest) && uIdx == psCurr->uEnd)
            {
                return psCurr;
            }
            psNext = psCurr->psRegNext;
        }
        /* Index is earlier than interval */
        if (iCmp < 0)
            psNext = psCurr->psRegPrev;
        /* Index is later than interval */
        if (iCmp > 0)
            psNext = psCurr->psRegNext;
    }

    return NULL;
}

IMG_INTERNAL
PUSC_REG_INTERVAL ProcListFindStart(PUSC_REG_INTERVAL psList)
/*****************************************************************************
 FUNCTION     : ProcListFindStart

 PURPOSE      : Get the start of a list connected through psProcNext/psProcPrev

 PARAMETERS   : psList            - List

  RETURNS     : Start of the list
*****************************************************************************/
{
    if (psList == NULL)
        return NULL;
    for (; psList->psProcPrev != NULL; psList = psList->psProcPrev)
    {
        /* skip */
    }
    return psList;
}

IMG_INTERNAL
PUSC_REG_INTERVAL ProcListCons(PUSC_REG_INTERVAL psInterval,
							   PUSC_REG_INTERVAL psList)
/*****************************************************************************
 FUNCTION	: ProcListCons

 PURPOSE	: Add a register interval to the front of a  list

 PARAMETERS	: psInterval    - Interval to add
              psList         - List to update

 RETURNS	: The updated list
*****************************************************************************/
{
	psInterval->psProcPrev = NULL;
	psInterval->psProcNext = psList;
	if (psList != NULL)
	{
		psList->psProcPrev = psInterval;
	}

	return psInterval;
}

IMG_INTERNAL
PUSC_REG_INTERVAL RegIntvlDrop(PUSC_REG_INTERVAL psInterval,
							   PUSC_REG_INTERVAL psList)
/*****************************************************************************
 FUNCTION	: RegIntvlDrop

 PURPOSE	: Remove a register interval from a list connected through
              psRegNext, psRegPrev

 PARAMETERS	: psInterval    - Interval to remove
              psList        - List to remove from

 RETURNS	: Update PUSC
*****************************************************************************/
{
	PUSC_REG_INTERVAL psPrev = psInterval->psRegPrev;
	PUSC_REG_INTERVAL psNext = psInterval->psRegNext;

	if (psList == psInterval)
	{
		psList = psNext;
	}

	psInterval->psRegPrev = NULL;
	psInterval->psRegNext = NULL;

	if (psNext != NULL)
	{
		psNext->psRegPrev = psPrev;
	}
	if (psPrev != NULL)
	{
		psPrev->psRegNext = psNext;
	}

	return psList;
}

IMG_INTERNAL
PUSC_REG_INTERVAL RegIntvlCons(PUSC_REG_INTERVAL psInterval,
							   PUSC_REG_INTERVAL psList)
/*****************************************************************************
 FUNCTION   : RegIntvlCons

 PURPOSE    : Add a register interval to the front of a list
              connected by psRegNext/psRegPrev.

 PARAMETERS : psInterval    - Interval to add
              psList        - List to update.

 RETURNS    : The updated list
*****************************************************************************/
{
	if (psList == NULL)
	{
		psInterval->psRegNext = NULL;
		psInterval->psRegPrev = NULL;
	}
	else
	{
	    psInterval->psRegNext = psList;

		psInterval->psRegPrev = psList->psRegPrev;
		psList->psRegPrev = psInterval;

		if (psInterval->psRegPrev != NULL)
		{
			psInterval->psRegPrev->psRegNext = psInterval;
		}

	}

	return psInterval;
}

IMG_INTERNAL
PUSC_REG_INTERVAL ProcListAdd(PUSC_REG_INTERVAL psInterval,
							  PUSC_REG_INTERVAL psList)
/*****************************************************************************
 FUNCTION   : ProcListAdd

 PURPOSE    : Insert a register interval into a list, connected by psProcNext/psProcPrev,
              sorted by start and end points

 PARAMETERS : psInterval    - Interval to insert
              psList        - List to update.

 RETURNS    : The updated list
*****************************************************************************/
{
	PUSC_REG_INTERVAL psPoint;
	PUSC_REG_INTERVAL psRet;

	psPoint = ProcListFindPos(psList, psInterval->uStart, psInterval->uEnd);

	/* ProcListFindPos is the interval that immediately precedes psInterval */
	if (psPoint == NULL)
	{
	    /* psInterval is earlier than any in psList */
	    psList = ProcListFindStart(psList);
		psRet = ProcListCons(psInterval, psList);
	}
	else
	{
	    psInterval->psProcNext = psPoint->psProcNext;
	    psInterval->psProcPrev = psPoint;

	    if (psPoint->psProcNext != NULL)
	        psPoint->psProcNext->psProcPrev = psInterval;
	    psPoint->psProcNext = psInterval;
		psRet = psList;
	}

	return psRet;
}

IMG_INTERNAL
PUSC_REG_INTERVAL ProcListRemove(PUSC_REG_INTERVAL psInterval,
								 PUSC_REG_INTERVAL psList)
/*****************************************************************************
 FUNCTION	: ProcListRemove

 PURPOSE    : Remove a register interval from a list connected through
              psProcNext, psProcPrev

 PARAMETERS	: psInterval    - Interval to remove
              psList        - List to remove from

 RETURNS	: Update list
*****************************************************************************/
{
	PUSC_REG_INTERVAL psPrev = psInterval->psProcPrev;
	PUSC_REG_INTERVAL psNext = psInterval->psProcNext;

	psInterval->psProcPrev = NULL;
	psInterval->psProcNext = NULL;

	if (psNext != NULL)
	{
		psNext->psProcPrev = psPrev;
	}
	if (psPrev != NULL)
	{
		psPrev->psProcNext = psNext;
	}

	if (psList != psInterval)
		return psList;
	if (psNext != NULL)
		return psNext;
	else
		return psPrev;
}

static IMG_VOID FreeRALScanState(PINTERMEDIATE_STATE psState,
								 PRALSCAN_STATE* ppsRegState,
								 IMG_BOOL bFreeState)
/*****************************************************************************
 FUNCTION	: FreeRALScanState

 PURPOSE	: Free register allocation state.

 PARAMETERS	: psState			- Compiler state.
			  ppsRegState		- Points to the register allocation state to free.
			  bFreeState		- If FALSE just free the array that vary in size
								depending on the number of registers.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32 i;
	PRALSCAN_STATE	psRegState = *ppsRegState;
	IMG_UINT32 uNrRegisters;

	ASSERT(psRegState != NULL);
	uNrRegisters = psRegState->sRAData.uNrRegisters;

	ClearRAData(psState, &(psRegState->sRAData));

	if (psRegState->apsRegInterval != NULL)
	{
		for (i = 0; i < uNrRegisters; i ++)
		{
			if(psRegState->apsRegInterval[i] != NULL)
			{
				FreeRegIntervalList(psRegState->sRAData.psState,
									psRegState->apsRegInterval[i]);
				psRegState->apsRegInterval[i] = NULL;
			}
		}
		UscFree(psState, psRegState->apsRegInterval);
	}
	UscFree(psState, psRegState->apsColour);

	UscFree(psState, psRegState->auIsWritten);
	UscFree(psState, psRegState->auIsFixed);

	UscFree(psState, psRegState->auFixedColour);
	UscFree(psState, psRegState->auIsOutput);

	UscFree(psState, psRegState->auNodeHasLoc);
	UscFree(psState, psRegState->auNodeMemLoc);

    if (bFreeState)
    {
        UscFree(psState, *ppsRegState);
    }
}

static
IMG_VOID RALScanCalcRegs(PINTERMEDIATE_STATE psState,
						 FUNCGROUP eFuncGroup,
						 RA_REG_LIMITS* psRegLimits)
/*****************************************************************************
 FUNCTION	: RALScanCalcRegs

 PURPOSE	: Calculate the number of registers available.

 PARAMETERS	: psState			- Compiler state.
			  uNrRegisters		- Number of registers that need to be allocated.
			  ppsRegState		- Initialised with the allocated structure.

 OUTPUTS    : psRegLimits       - Number of registers

 RETURNS	: Nothing
*****************************************************************************/
{
    IMG_UINT32 uNumAvailRegs = 0;
    IMG_UINT32 uNumAvailPARegs = 0;
	IMG_UINT32 uNumAvailOutputRegs = 0;
	IMG_UINT32 uPAMaxIndex = 0;
	IMG_UINT32 uOutputMaxIndex = 0;
	IMG_UINT32 uNrRegisters = 0;

	/* Set up information about the number of registers available. */
	if (eFuncGroup == FUNCGROUP_MAIN)
	{
		if (psState->uCompilerFlags & UF_MSAATRANS)
		{
			uNumAvailPARegs = 0;
		}
		else
		{
			uNumAvailPARegs = psState->uPARegistersUsed;
		}

		/*
			Get maximum ranges for indexed accesses to the primary
			attributes and outputs.
		*/
		if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL)
		{
			uPAMaxIndex = ((UNIFLEX_TEXTURE_COORDINATE_9 + 1) *
						   CHANNELS_PER_INPUT_REGISTER);

			if (psState->psSAOffsets->uPackDestType == USEASM_REGTYPE_OUTPUT)
			{
				uNumAvailOutputRegs = 1;
			}
			else
			{
				uNumAvailOutputRegs = 0;
			}
			uOutputMaxIndex = 0;
		}
		else
		{
			uPAMaxIndex = max(EURASIA_USE_PRIMATTR_BANK_SIZE, uNumAvailPARegs);

			if (!(psState->uCompilerFlags & UF_REDIRECTVSOUTPUTS))
			{
				uNumAvailOutputRegs = psState->uMaximumVSOutput + 1;
				uOutputMaxIndex = uNumAvailOutputRegs;
			}
			else
			{
				uNumAvailOutputRegs = 0;
				uOutputMaxIndex = 0;
			}
		}
	}
	else
	{
		uNumAvailOutputRegs = 0;
		uOutputMaxIndex = 0;

		uNumAvailPARegs = 0;
		uPAMaxIndex = 0;
	}

    /* Set some basic values */
    uNumAvailRegs = REGALLOC_PRIMATTR_START;
	uNumAvailRegs += uNumAvailPARegs;
	uNumAvailRegs += uNumAvailOutputRegs;
	uNumAvailRegs += psState->psSAOffsets->uNumAvailableTemporaries;

	uNrRegisters = REGALLOC_PRIMATTR_START;
	uNrRegisters += uNumAvailPARegs;
	uNrRegisters += uNumAvailOutputRegs;
	uNrRegisters += psState->uNumRegisters;

	/* Record the register numbers */
	psRegLimits->uNumVariables = uNrRegisters;
	psRegLimits->uNumHWRegs = uNumAvailRegs;
	psRegLimits->uNumPARegs = uNumAvailPARegs;
	psRegLimits->uNumOutputRegs = uNumAvailOutputRegs;

	psRegLimits->uOutputStart = (REGALLOC_PRIMATTR_START +
								 uNumAvailPARegs);
	psRegLimits->uTempStart = (REGALLOC_PRIMATTR_START +
							   uNumAvailPARegs +
							   uNumAvailOutputRegs);

	psRegLimits->uMaxPAIndex = uPAMaxIndex;
	psRegLimits->uMaxOutputIndex = uOutputMaxIndex;
}

static IMG_VOID AllocateRALScanState(PINTERMEDIATE_STATE psState,
									 IMG_UINT32 uNrRegisters,
									 PRALSCAN_STATE* ppsRegState)
/*****************************************************************************
 FUNCTION	: AllocateRALScanState

 PURPOSE	: Allocate register allocation data structures.

 PARAMETERS	: psState			- Compiler state.
			  uNrRegisters		- Number of registers that need to be allocated.
			  ppsRegState		- Initialised with the allocated structure.
              eFuncGroup        - Function group being processed

 RETURNS	: Nothing.
*****************************************************************************/
{
	PRALSCAN_STATE psRegState;
	IMG_BOOL bNewState = IMG_FALSE;

	/* Allocate the register state */
	psRegState = *ppsRegState;
	bNewState = IMG_FALSE;
	if (psRegState == NULL)
	{
		/* No existing state */
		bNewState = IMG_TRUE;
	}
	else if (psRegState->sRAData.uNrRegisters != uNrRegisters)
	{
		/* Number of registers has changed */
		bNewState = IMG_TRUE;
	}

	if (bNewState)
	{
		PREGALLOC_DATA psRAData = NULL;

		if (psRegState == NULL)
		{
			psRegState = UscAlloc(psState, sizeof(RALSCAN_STATE));
			*ppsRegState = psRegState;
		}
		else
		{
			FreeRALScanState(psState, ppsRegState, IMG_FALSE);
			psRegState = *ppsRegState;
		}
		psRAData = &(psRegState->sRAData);

		/* Initialise data */
		InitRAData(psState, psRAData, uNrRegisters);

		/* Clear pointers */
		psRegState->apsRegInterval = NULL;
		psRegState->apsColour = NULL;
		psRegState->auIsWritten = NULL;
		psRegState->auIsFixed = NULL;
		psRegState->auFixedColour = NULL;
		psRegState->auIsOutput = NULL;
		psRegState->auNodeHasLoc = NULL;
		psRegState->auNodeMemLoc = NULL;

#ifdef DEBUG
		psRegState->uIteration = 0;
#endif
	}
	else
	{
		psRegState = *ppsRegState;
	}

	psRegState->sRAData.uNrRegisters = uNrRegisters;
}

static IMG_VOID ResetRALScanState(PINTERMEDIATE_STATE psState,
								  IMG_UINT32 uNrRegisters,
								  FUNCGROUP eFuncGroup,
								  PRALSCAN_STATE* ppsRegState)
/*****************************************************************************
 FUNCTION   : ResetRALScanState

 PURPOSE    : Reset register allocation data structures.

 PARAMETERS : psState           - Compiler state.
              uNrRegisters      - Number of registers that need to be allocated.
              ppsRegState       - Initialised with the allocated structure.
              eFuncGroup        - Program type

 RETURNS    : Nothing.
*****************************************************************************/
{
    PRALSCAN_STATE psRegState = *ppsRegState;
	PREGALLOC_DATA psRAData = NULL;
    IMG_UINT32 uNode = 0;
    IMG_UINT32 uIdx = 0;
	IMG_UINT32 uAvailRegs = 0;
	IMG_UINT32 uAvailPARegs = 0;
	IMG_UINT32 uAvailOutputRegs = 0;
	IMG_UINT32 uNumFixedVRegs = 0;
	IMG_BOOL bAllocArrays = IMG_FALSE;
	RA_REG_LIMITS *psRegLimits;

	/* Calculate number of registers */

	psRegLimits = UscAlloc(psState, sizeof(*psRegLimits));
	RALScanCalcRegs(psState, eFuncGroup, psRegLimits);

	uNrRegisters = psRegLimits->uNumVariables;
	uAvailRegs = psRegLimits->uNumHWRegs;
	uAvailPARegs = psRegLimits->uNumPARegs;
	uAvailOutputRegs = psRegLimits->uNumOutputRegs;

    /* Reallocate the state if the number of registers has changed */
	bAllocArrays = IMG_FALSE;
	if ((psRegState == NULL) ||
		(uNrRegisters != psRegState->sRAData.uNrRegisters))
	{
		/* Number of registers has changed */
        AllocateRALScanState(psState, uNrRegisters, ppsRegState);
        psRegState = *ppsRegState;
		bAllocArrays = IMG_TRUE;
	}
	else if (psRegState->apsRegInterval == NULL ||
			 psRegState->apsColour == NULL)
	{
		/*
		  Register data is unallocated (apsRegInterval and apsColour
		  are used as representatives of all allocated arrays).
		*/
		bAllocArrays = IMG_TRUE;
	}
	else
	{
		/* All arrays should have been allocated */
		ASSERT(uNrRegisters == psRegState->sRAData.uNrRegisters);
		ASSERT(psRegState->apsRegInterval != NULL);
		ASSERT(psRegState->apsColour != NULL);
		ASSERT(psRegState->auIsWritten != NULL);
		ASSERT(psRegState->auIsFixed != NULL);
		ASSERT(psRegState->auFixedColour != NULL);
		ASSERT(psRegState->auIsOutput != NULL);
		ASSERT(psRegState->auNodeHasLoc != NULL);
		ASSERT(psRegState->auNodeMemLoc != NULL);
	}
	psRAData = &(psRegState->sRAData);

	/* Store some data in local variables */
	psRAData->eFuncGroup = eFuncGroup;

	psRAData->uNrRegisters = uNrRegisters;
	psRAData->uAvailRegs = uAvailRegs;
	psRAData->uAvailPARegs = uAvailPARegs;
	psRAData->uAvailOutputRegs = uAvailOutputRegs;

	psRAData->uOutputStart = psRegLimits->uOutputStart;
	psRAData->uTempStart = psRegLimits->uTempStart;

	psRAData->uPAIndexMax = psRegLimits->uMaxPAIndex;
	psRAData->uOutputIndexMax = psRegLimits->uMaxOutputIndex;

	/* Number of hw registers = available registers - registers needed for spilling */
	psRegState->uNumHwRegs = uAvailRegs;
	psRegState->uAddrReg = psRegState->uNumHwRegs;

	/* Set start of spill registers */
	psRegState->uNextSpillColour = uAvailPARegs;

	if (bAllocArrays)
	{
		/* Allocate register arrays */

		psRegState->apsRegInterval = UscAlloc(psState, sizeof(PUSC_REG_INTERVAL) * uNrRegisters);
		psRegState->apsColour = UscAlloc(psState, sizeof(PUSC_REG_INTERVAL) * uAvailRegs);

		usc_alloc_bitnum(psState, psRegState->auIsWritten, uNrRegisters);
		usc_alloc_bitnum(psState, psRegState->auIsFixed, uNrRegisters);

		usc_alloc_num(psState, psRegState->auFixedColour, uNrRegisters);
        usc_alloc_bitnum(psState, psRegState->auIsOutput, uNrRegisters);

        usc_alloc_bitnum(psState, psRegState->auNodeHasLoc, uNrRegisters);
		usc_alloc_num(psState, psRegState->auNodeMemLoc, uNrRegisters);

		/* Required initialisation */
		memset(psRegState->apsRegInterval, 0, sizeof(PUSC_REG_INTERVAL) * uNrRegisters);

	}

	/* Initialiase register and group data */
	memset(psRAData->auOptionalGroup,
		   0,
		   UINTS_TO_SPAN_BITS(uNrRegisters) * sizeof(IMG_UINT32));

	for (uNode = 0; uNode < uNrRegisters; uNode++)
	{
		psRAData->asRegGroup[uNode].psPrev = NULL;
		psRAData->asRegGroup[uNode].psNext = NULL;
		psRAData->asRegGroup[uNode].uReservedColour = USC_UNDEF_COLOUR;
	}

	/*
	 * Determine the registers which are shader results.
	 */
	if (eFuncGroup == FUNCGROUP_MAIN)
	{
		psRAData->asFixedVReg = psState->asFixedVReg;
		psRAData->uNumFixedVRegs = psState->uNumFixedRegs;
		FlagPrimaryAttributeResult(psState, psRAData);
	}
	else
	{
		psRAData->asFixedVReg = NULL;
		psRAData->uNumFixedVRegs = 0;
		psRAData->uShaderOutputToPAs = USC_UNDEF;
	}
	uNumFixedVRegs = psRAData->uNumFixedVRegs;

	/*
	 * Set up information about precoloured nodes
	 */

	memset(psRAData->auPreColoured,
		   0,
		   UINTS_TO_SPAN_BITS(uNrRegisters) * sizeof(IMG_UINT32));

	/* All non-temporary registers are precoloured */
	for (uNode = 0; uNode < psRAData->uTempStart; uNode++)
	{
		SetBit(psRAData->auPreColoured, uNode, IMG_TRUE);
	}

	/* Shader results are precoloured */
	for (uIdx = 0; uIdx < uNumFixedVRegs; uIdx++)
	{
		IMG_UINT32					uShaderOutputNode;
		PSFIXED_REG_DATA			psOut = &psRAData->asFixedVReg[uIdx];
		IMG_UINT32					uConsectiveRegIdx;
		ARG							sVReg;
		ARG							sPReg;

		sVReg = psOut->sVReg;
		sPReg = psOut->sPReg;
		for(uConsectiveRegIdx = 0; uConsectiveRegIdx < psOut->uConsectiveRegsCount; uConsectiveRegIdx++)
		{
			sVReg.uNumber = psOut->sVReg.uNumber + uConsectiveRegIdx;
			sPReg.uNumber = psOut->sPReg.uNumber + uConsectiveRegIdx;
			if (RegIsNode(psRAData, &sVReg))
			{
				IMG_UINT32 uColour = USC_UNDEF_COLOUR;
				uShaderOutputNode = ArgumentToNode(psRAData, &sVReg);
				SetBit(psRAData->auPreColoured, uShaderOutputNode, IMG_TRUE);

				SetBit(psRegState->auIsOutput, uShaderOutputNode, IMG_TRUE);
				SetBit(psRegState->auIsFixed, uShaderOutputNode, IMG_TRUE);

				uColour = ArgumentToNode(psRAData, &sPReg);
				psRegState->auFixedColour[uShaderOutputNode] = uColour;
			}
		}
	}

	#if defined(OUTPUT_USPBIN)
	if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
	{
		/* Shadow USP output registers are precoloured */
		if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL)
		{
			IMG_UINT32 uBankIdx;
			IMG_UINT32 uShaderOutputNode;
			for	(uBankIdx = 0;
				 uBankIdx < (psRAData->uAltResultBanksCount);
				 uBankIdx++)
			{
				PSHADEROUTPUT	psOut = &psRAData->asAltBankShaderOutputs[uBankIdx];
				IMG_UINT32		uRegIdx;
				for (uRegIdx = 0;
					 uRegIdx < (psRAData->uAltBankOutputsCount); uRegIdx++)
				{
					SHADEROUTPUT sOut = *psOut;
					sOut.sVReg.uNumber = sOut.sVReg.uNumber + uRegIdx;
					IMG_UINT32 uColour = USC_UNDEF_COLOUR;				
					uShaderOutputNode = ArgumentToNode(psRAData, &(sOut.sVReg));
					SetBit(psRAData->auPreColoured, uShaderOutputNode, IMG_TRUE);

					SetBit(psRegState->auIsOutput, uShaderOutputNode, IMG_TRUE);
					SetBit(psRegState->auIsFixed, uShaderOutputNode, IMG_TRUE);

					sOut.sPReg.uNumber = sOut.sPReg.uNumber + uRegIdx;
					uColour = ArgumentToNode(psRAData, &(sOut.sPReg));
					psRegState->auFixedColour[uShaderOutputNode] = uColour;
				}
			}
		}
	}
	#endif /* defined(OUTPUT_USPBIN) */

#if 0
	for (uNode = 0; uNode < uNrRegisters; uNode++)
	{
		IMG_BOOL bIsFixed = IMG_FALSE;

		psRAData->asRegGroup[uNode].psPrev = NULL;
		psRAData->asRegGroup[uNode].psNext = NULL;
		psRAData->asRegGroup[uNode].uReservedColour = (IMG_UINT32) -1;

		bIsFixed = NodeIsFixedColour(psState, psRAData, uNode);
		SetBit(psRAData->auPreColoured, uNode, bIsFixed);

		if(psRegState->apsRegInterval[uNode] != NULL)
		{
			FreeRegIntervalList(psRAData->psState,
								psRegState->apsRegInterval[uNode]);
			psRegState->apsRegInterval[uNode] = NULL;
		}
	}
#endif
	psRegState->uRegAllocStart = ((psState->uCompilerFlags & UF_MSAATRANS) ?
								  uAvailPARegs :
								  0);

	memset(psRegState->apsRegInterval, 0,
		   sizeof(PUSC_REG_INTERVAL) * uNrRegisters);
    memset(psRegState->apsColour, 0, sizeof(PUSC_REG_INTERVAL) * uAvailRegs);
	memset(psRegState->auFixedColour, 0,
		   sizeof(IMG_UINT32) * uNrRegisters);

    memset(psRegState->auIsWritten, 0,
		   sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(uNrRegisters));
	memset(psRegState->auIsFixed, 0,
		   sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(uNrRegisters));
    memset(psRegState->auIsOutput,
           0,
           sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(uNrRegisters));

	psRegState->psIntervals = NULL;
	psRegState->psFixed = NULL;

	memset(psRegState->auNodeHasLoc, 0,
		   UINTS_TO_SPAN_BITS(uNrRegisters) * sizeof(IMG_UINT32));
	memset(psRegState->auNodeMemLoc, 0, uNrRegisters * sizeof(IMG_UINT32));


	/* Clean up */
	UscFree(psState, psRegLimits);

#ifdef DEBUG
    psRegState->uMaxInstIdx = 0;
#endif /* defined(DEBUG) */
}

static
PUSC_REG_INTERVAL RegIntvlFind(PUSC_REG_INTERVAL psList,
							   IMG_UINT32 uStart,
							   IMG_UINT32 uEnd)
/*****************************************************************************
 FUNCTION     : RegIntvlFind

 PURPOSE      : Find the interval closest to a specifed start and end point.

 PARAMETERS   : psList           - List connected through psRegNext/psRegPrev
                uStart, uEnd     - Start and end to try to match

  RETURNS      : First list element later than specified interval
*****************************************************************************/
{

    PUSC_REG_INTERVAL psCurr, psNext, psLast;
    IMG_BOOL bForward, bFound;

    if (psList == NULL)
    {
        return NULL;
    }
    /* Set the direction to search in */
    if (IntervalLessThan(psList->uStart, psList->uEnd, uStart, uEnd))
    {
        bForward = IMG_TRUE;
    }
    else
    {
        bForward = IMG_FALSE;
    }

    psNext = NULL;
    bFound = IMG_FALSE;
    psLast = psList;
    for (psCurr = psList; psCurr != NULL; psCurr = psNext)
    {
        if (bForward)
        {
            psNext = psCurr->psRegNext;

            if (IntervalLessThan(psCurr->uStart, psCurr->uEnd, uStart, uEnd))
            {
                /* Start is later than current interval, go forward */
                psNext = psCurr->psRegNext;
            }
            else
            {
                bFound = IMG_TRUE;
                break;
            }
        }
        else /* !bForward */
        {
            if (!IntervalLessThan(psCurr->uStart, psCurr->uEnd, uStart, uEnd))
            {
                /* Start is earlier than current interval, go backward */
                psNext = psCurr->psRegPrev;
            }
            else
            {
                bFound = IMG_TRUE;
                psLast = psCurr;
                break;
            }
        }
        psLast = psCurr;
    }

    return psLast;
}

static
PUSC_REG_INTERVAL RegIntvlAdvance(PUSC_REG_INTERVAL psList,
                                  IMG_UINT32 uStart,
                                  IMG_UINT32 uEnd,
                                  PUSC_REG_INTERVAL *ppsRet)
/*****************************************************************************
 FUNCTION     : RegIntvlAdvance

 PURPOSE      : Advance the interval for a base node to the interval nearest to an index.

 PARAMETERS   : psList           - List of intervals
                uStart, uEnd     - Start and end to try to match

 OUTPUT       : ppsRet           - Current interval or NULL if no active interval

 RETURNS      : Index into List connected by psRegNext/psRegPrev of nearest interval
*****************************************************************************/
{
    PUSC_REG_INTERVAL psCurr;
    IMG_UINT32 uCmp;

    psCurr = RegIntvlFind(psList, uStart, uEnd);
    if (psCurr == NULL)
    {
        *ppsRet = NULL;
        return NULL;
    }

    /* May need to more backward */
    uCmp = IntervalCmp(psCurr->uStart, psCurr->uEnd, uStart);

    if (uCmp == 0 ||
        (uCmp < 0 && EarlierThan(psCurr->uStart, uEnd)) ||
        (uCmp > 0 && EarlierThan(uStart, psCurr->uEnd)))
    {
        /* Current interval contains required interval */
        *ppsRet = psCurr;
    }
    else if (uCmp > 0)
    {
        *ppsRet = NULL;
    }
    return psCurr;
}

static
IMG_VOID GetNodeReg(PRALSCAN_STATE psRegState,
					IMG_UINT32 uNode,
					IMG_PUINT32 puRegType,
					IMG_PUINT32 puRegNum)
/*****************************************************************************
 FUNCTION     : GetNodeReg

 PURPOSE      : Get the register type and number from a node

 PARAMETERS   : psRegState    - Register allocator state
                uNode         - Node number

 OUTPUT       : puRegType     - Register type
                puRegNum     - Register number

 RETURNS      : Nothing
*****************************************************************************/
{
	if (uNode >= psRegState->sRAData.uAvailPARegs)
	{
		if (puRegType != NULL)
			*puRegType = USEASM_REGTYPE_TEMP;

		if (puRegNum != NULL)
			*puRegNum = uNode - psRegState->sRAData.uAvailPARegs;
	}
	else
	{
		if (puRegType != NULL)
			*puRegType = USEASM_REGTYPE_PRIMATTR;

		if (puRegNum != NULL)
			*puRegNum = uNode;
	}
}


static
PUSC_REG_INTERVAL ProcListFindWith(PUSC_REG_INTERVAL psList,
                                   IMG_UINT32 uIdx,
                                   IMG_BOOL bDest)
/*****************************************************************************
 FUNCTION     : RegIntvlFindWith

 PURPOSE      : Find the interval containing an index

 PARAMETERS   : psList           - List of intervals (connected through psRegNext/psRegPrev)
                uIdx             - Index to look for
                bDest            - Whether index is for a write or a read

 OUTPUT       : ppsRet           - Current interval or NULL if no active interval

 RETURNS      : Index into List connected by psProcNext/psProcPrev of nearest interval
*****************************************************************************/
{
    PUSC_REG_INTERVAL psCurr, psNext, psTmp;
    IMG_UINT32 uCmp;
    /* Go to the interval before the required point */
    psTmp = ProcListFindPos(psList, uIdx, uIdx);
    if (psTmp == NULL)
		psList = ProcListFindStart(psList);
	else
        psList = psTmp;

    /* Search for the interval containing the required point */
    psNext = NULL;
    for (psCurr = psList; psCurr != NULL; psCurr = psNext)
    {
        uCmp = IntervalCmp(psCurr->uStart, psCurr->uEnd, uIdx);

        /* Index is unambiguously in interval */
        if (uCmp == 0)
        {
            if (uIdx != psCurr->uStart && uIdx != psCurr->uEnd)
                return psCurr;
            /* Test for first occurence of a write to a node */
            if (bDest && uIdx == psCurr->uStart)
            {
                return psCurr;
            }

            /* Test for last occurence of a read to a node */
            if ((!bDest) && uIdx == psCurr->uEnd)
            {
                return psCurr;
            }
            psNext = psCurr->psProcNext;
        }
        /* Index is earlier than interval */
        if (uCmp < 0)
            psNext = psCurr->psProcPrev;
        /* Index is later than interval */
        if (uCmp > 0)
            psNext = psCurr->psProcNext;
    }

    return NULL;
}

static
IMG_BOOL NodeIsTempReg(PRALSCAN_STATE psRegState,
                       IMG_UINT32 uNode)
/*****************************************************************************
 FUNCTION   : NodeIsTempReg

 PURPOSE    : Test whether a node is a temporary register

 PARAMETERS : psRegState       - Register allocator state
              uNode            - The node to test

 RETURNS    : IMG_TRUE iff uNode is a temporary register;
*****************************************************************************/
{
    if (uNode < psRegState->sRAData.uAvailPARegs)
    {
        return IMG_FALSE;
    }
    else
    {
        return IMG_TRUE;
    }
}

static
IMG_BOOL NodeToReg(PREGALLOC_DATA psRAData,
				   IMG_UINT32 uNode,
				   PUSC_REGTYPE puRegType,
				   IMG_PUINT32 puRegNum)
/*****************************************************************************
 FUNCTION   : NodeToReg

 PURPOSE    : Get the register type and number of a node

 PARAMETERS : psRAData      - Register allocator data
              uNode         - The node.

 OUTPUT     : puRegType     - Register type
              puRegNum      - Register number

 RETURNS    : IMG_TRUE if node is a register.
*****************************************************************************/
{
	USC_REGTYPE uRegType = USC_UNDEF;
	USC_REGTYPE uRegNum = USC_UNDEF;

    if (uNode < psRAData->uAvailPARegs)
    {
		uRegType = USEASM_REGTYPE_PRIMATTR;
		uRegNum = uNode;
    }
    else
    {
		uRegType = USEASM_REGTYPE_TEMP;
		uRegNum = uNode - psRAData->uAvailPARegs;
    }

	if (puRegType != NULL)
		(*puRegType) = uRegType;
	if (puRegNum != NULL)
		(*puRegNum) = uRegNum;

	return IMG_TRUE;
}

static
IMG_VOID RegGroupingInst(PINTERMEDIATE_STATE psState,
						 PRALSCAN_STATE psRegState,
						 PINST psInst,
						 IMG_PUINT32 puAddMoves,
						 PINST* ppsNext)
/*****************************************************************************
 FUNCTION	: RegGroupingInst

 PURPOSE	: Form mandatory register groups for an instruction.

 PARAMETERS	: psInst			- The instruction to add relations for.

 OUTPUT		: puAddMoves		- Whether the instruction should be re-procecessed
								  after putting the destination/sources into
								  temporary registers.
								  puAddMoves == 0: No moves,
								  puAddMoves & (1U << 0): Add destination moves.
								  puAddMoves & (1U << 1): Add source moves.

              ppsNext           - Next Instruction to process

 RETURNS	: Nothing.

 NOTES      : puAddMoves handles the degenerate case when instruction registers
			  are to be added to a new group but are already in an existing group.
			  If possible, the new group will be added to the existing group.
			  If the two groups are not compatible or if the combined size of
			  the groups is excessive then the instructions operands must be
			  put into fresh registers. A group size is excessive if registers
			  cannot be allocated for every member of the group without spilling
			  nodes.
*****************************************************************************/
{
	IMG_UINT32 i;
	IMG_UINT32 uArgCount = g_psInstDesc[psInst->eOpcode].uArgumentCount;
	IMG_BOOL bSrcGroupConflict = IMG_FALSE;
	IMG_BOOL bDstGroupConflict = IMG_FALSE;
	IMG_UINT32 auSrcInGroup [UINTS_TO_SPAN_BITS(USC_NUM_SMP_SRCS)];
	IMG_UINT32 auDstInGroup [UINTS_TO_SPAN_BITS(USC_NUM_SMP_DSTS)];
	IMG_BOOL bDstInGroup = IMG_FALSE;
	IMG_UINT32 uNumDstGroup = 0;
	IMG_BOOL bSrcInGroup = IMG_FALSE;
	IMG_UINT32 uNumSrcGroup = 0;

	memset(&auSrcInGroup, 0, sizeof(auSrcInGroup));
	memset(&auDstInGroup, 0, sizeof(auDstInGroup));

	if (ppsNext != NULL)
	{
		(*ppsNext) = psInst->psNext;
	}

	/*
	  Check for a sequence of loads that we want to group into a
	  fetch.
	*/
	FormFetchGroup(psState, &psRegState->sRAData, psInst, ppsNext);

	/*
	   If instruction writes to multiple destinations, the
	   registers must be contiguous.
	*/
	if	((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_TEXTURESAMPLE) ||
		 (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_MULTIPLEDEST))
	{
		if (psInst->uDestCount > 1 &&
			(psInst->asDest[0].uType == USEASM_REGTYPE_TEMP ||
			 psInst->asDest[0].uType == USEASM_REGTYPE_PRIMATTR))
		{
			IMG_UINT32	uFirstDestNode = ArgumentToNode(&psRegState->sRAData, &psInst->asDest[0]);
			IMG_UINT32	uPrevDestNode;

			MakeGroup(&psRegState->sRAData, uFirstDestNode, IMG_FALSE);

			uPrevDestNode = uFirstDestNode;
			for (i = 1; i < psInst->uDestCount; i++)
			{
				IMG_UINT32	uDestNode = ArgumentToNode(&psRegState->sRAData, &psInst->asDest[i]);

				if (IsNodeInGroup(&psRegState->sRAData, uDestNode))
				{
					bDstInGroup = IMG_TRUE;
					SetBit(auDstInGroup, i, 1);
				}

				if (!AddToGroup(&psRegState->sRAData, uPrevDestNode, uDestNode, IMG_FALSE))
				{
					bDstGroupConflict = IMG_TRUE;
				}
				else
				{
					uNumDstGroup += 1;
				}

				uPrevDestNode = uDestNode;
			}
		}
	}

	/*
	   If this is a texture sample then we need the coordinates to be in
	   contiguous registers so note this.
	*/
	if	(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_DEPENDENTTEXTURESAMPLE)
	{
		IMG_UINT32 uPrevNode = UINT_MAX;

		ASSERT(psInst->u.psSmp != NULL);

        /* Add all the coordinates to the group. */
        if (psInst->u.psSmp->uCoordSize > 1 &&
            (psInst->asArg[SMP_COORD_ARG_START].uType == USEASM_REGTYPE_TEMP ||
             psInst->asArg[SMP_COORD_ARG_START].uType == USEASM_REGTYPE_PRIMATTR))
        {
			for (i = 0; i < psInst->u.psSmp->uCoordSize; i++)
			{
				IMG_UINT32 uOffset = SMP_COORD_ARG_START + i;
				IMG_UINT32 uNode = ArgumentToNode(&psRegState->sRAData, &psInst->asArg[uOffset]);

				ASSERT (psInst->asArg[uOffset].uType == USEASM_REGTYPE_TEMP ||
						psInst->asArg[uOffset].uType == USEASM_REGTYPE_PRIMATTR);

				if (IsNodeInGroup(&psRegState->sRAData, uNode) ||
					uNode == uPrevNode)
				{
					bSrcInGroup = IMG_TRUE;
					SetBit(auSrcInGroup, i, 1);
				}

				if (i == 0)
				{
					MakeGroup(&psRegState->sRAData, uNode, IMG_FALSE);
				}
				else
				{
					if (!AddToGroup(&psRegState->sRAData, uPrevNode, uNode, IMG_FALSE))
					{
						bSrcGroupConflict = IMG_TRUE;
					}
					else
					{
						uNumSrcGroup += 1;
					}
				}
				uPrevNode = uNode;
			}
		}

		/* Add all the state to the group. */
		#if defined(OUTPUT_USPBIN)
		if	((psInst->eOpcode >= ISMP) &&
			 (psInst->eOpcode <= ISMPGRAD))
		#endif /* defined(OUTPUT_USPBIN) ISMP/ISMPGRAD??? */
		{
			if (psInst->asArg[SMP_STATE_ARG_START].uType == USEASM_REGTYPE_SECATTR)
			{
				ASSERT(psInst->asArg[SMP_STATE_ARG_START + 1].uType == USEASM_REGTYPE_SECATTR);
				ASSERT(psInst->asArg[SMP_STATE_ARG_START + 2].uType == USEASM_REGTYPE_SECATTR);
				ASSERT(psInst->asArg[SMP_STATE_ARG_START + 1].uNumber ==
					   (psInst->asArg[SMP_STATE_ARG_START].uNumber + 1));
				ASSERT(psInst->asArg[SMP_STATE_ARG_START + 2].uNumber ==
					   (psInst->asArg[SMP_STATE_ARG_START].uNumber + 2));
			}
			else
			{
				for (i = 0; i < psState->uTexStateSize; i++)
				{
					IMG_UINT32 uOffset = SMP_STATE_ARG_START + i;
					IMG_UINT32 uNode = ArgumentToNode(&psRegState->sRAData,
													  &psInst->asArg[uOffset]);

					ASSERT (psInst->asArg[uOffset].uType == USEASM_REGTYPE_TEMP ||
							psInst->asArg[uOffset].uType == USEASM_REGTYPE_PRIMATTR);

					if (IsNodeInGroup(&psRegState->sRAData, uNode) || uNode == uPrevNode)
					{
						bSrcInGroup = IMG_TRUE;
						SetBit(auSrcInGroup, i, 1);
					}

					if (i == 0)
					{
						MakeGroup(&psRegState->sRAData, uNode, IMG_FALSE);
					}
					else
					{
						if (!AddToGroup(&psRegState->sRAData, uPrevNode, uNode, IMG_FALSE))
						{
							bSrcGroupConflict = IMG_TRUE;
						}
						else
						{
							uNumSrcGroup += 1;
						}
					}
					uPrevNode = uNode;
				}
			}
		}

		/* Add all the gradients to the group. */
		if	(psInst->u.psSmp->uGradSize > 0 &&
			 (psInst->asArg[SMP_GRAD_ARG_START].uType == USEASM_REGTYPE_TEMP ||
			  psInst->asArg[SMP_GRAD_ARG_START].uType == USEASM_REGTYPE_PRIMATTR))
		{
			for (i = 0; i < psInst->u.psSmp->uGradSize; i++)
			{
				IMG_UINT32 uOffset = SMP_GRAD_ARG_START + i;
				IMG_UINT32 uNode = ArgumentToNode(&psRegState->sRAData, &psInst->asArg[uOffset]);

				ASSERT (psInst->asArg[uOffset].uType == USEASM_REGTYPE_TEMP ||
						psInst->asArg[uOffset].uType == USEASM_REGTYPE_PRIMATTR);

				if (IsNodeInGroup(&psRegState->sRAData, uNode) || uPrevNode == uNode)
				{
					bSrcInGroup = IMG_TRUE;
					SetBit(auSrcInGroup, i, 1);
				}

				if (i == 0)
				{
					MakeGroup(&psRegState->sRAData, uNode, IMG_FALSE);
				}
				else
				{
					if (!AddToGroup(&psRegState->sRAData, uPrevNode, uNode, IMG_FALSE))
					{
						bSrcGroupConflict = IMG_TRUE;
					}
					else
					{
						uNumSrcGroup += 1;
					}
				}
				uPrevNode = uNode;
			}
		}
	}

	/*
	   If there was a group conflict, remove all registers added to the group.
	   Assumes that nodes are generated in increasing order.
	*/
	if (puAddMoves != NULL &&  // Don't do anything if AddMoves is turned off.
		(bDstInGroup ||
		 bDstGroupConflict ||
		 bSrcInGroup ||
		 bSrcGroupConflict))
	{
		IMG_BOOL bLargeGroup = IMG_FALSE;
		IMG_UINT32 uAddMoves = 0;
		IMG_UINT32 uSize;

		uSize = 1;
		if (bDstInGroup)
		{
			/* Count the size of the destination group. */
			REGISTER_GROUP* psGroup;
			IMG_UINT32 uNode = ArgumentToNode(&psRegState->sRAData, &psInst->asDest[0]);

			for (psGroup = &psRegState->sRAData.asRegGroup[uNode];
				 psGroup->psPrev != NULL;
				 psGroup = psGroup->psPrev, uSize++)  /* skip */ ;
			for (psGroup = &psRegState->sRAData.asRegGroup[uNode];
				 psGroup->psNext != NULL;
				 psGroup = psGroup->psNext, uSize++)  /* skip */ ;

			if (uSize >= psState->psSAOffsets->uNumAvailableTemporaries)
			{
				bLargeGroup = IMG_TRUE;
			}
		}

		if (bSrcInGroup && !bLargeGroup)
		{
			/* Count the size of the sources group. */
			REGISTER_GROUP* psGroup;
			IMG_UINT32 uNode;

			/* find the first source operand in the sources group */
			uNode = ArgumentToNode(&psRegState->sRAData, &psInst->asArg[0]);
			for (i = 0; i < uArgCount; i++)
			{
				if (psInst->asArg[i].uType == USEASM_REGTYPE_TEMP ||
					psInst->asArg[i].uType == USEASM_REGTYPE_PRIMATTR)
				{
					uNode = ArgumentToNode(&psRegState->sRAData, &psInst->asArg[i]);

					if (IsNodeInGroup(&psRegState->sRAData, uNode))
					{
						break;
					}
				}
			}

			/* Count the size of the group of which uNode is a member. */
			for (psGroup = &psRegState->sRAData.asRegGroup[uNode];
				 psGroup->psPrev != NULL;
				 psGroup = psGroup->psPrev, uSize++)  /* skip */ ;
			for (psGroup = &psRegState->sRAData.asRegGroup[uNode];
				 psGroup->psNext != NULL;
				 psGroup = psGroup->psNext, uSize++)  /* skip */ ;

			if (uSize >= psState->psSAOffsets->uNumAvailableTemporaries)
			{
				bLargeGroup = IMG_TRUE;
			}
		}

		if (bDstGroupConflict || bSrcGroupConflict || bLargeGroup)
		{
			IMG_UINT32 uNode;

			/* Remove destination registers. */
			if	((bDstGroupConflict || bLargeGroup) &&
				 (psInst->asDest[0].uType == USEASM_REGTYPE_TEMP ||
				  psInst->asDest[0].uType == USEASM_REGTYPE_PRIMATTR))
			{
				for (i = 0; i < psInst->uDestCount; i++)
				{
					IMG_UINT32	uDestNode;

					uDestNode = ArgumentToNode(&psRegState->sRAData, &psInst->asDest[i]);

					if (
						(GetBit(auDstInGroup, i) != 1) &&
						IsNodeInGroup(&psRegState->sRAData, uDestNode)
						)
					{
						RemoveFromGroup(&psRegState->sRAData, uDestNode);
					}
				}

				/* Add moves for the destination */
				uAddMoves |= (1U << 0);
			}

			/* Remove source registers */
			if	((bSrcGroupConflict || bLargeGroup))
			{
				for (i = 0; i < uArgCount; i++)
				{
					if (psInst->asArg[i].uType != USEASM_REGTYPE_TEMP &&
						psInst->asArg[i].uType != USEASM_REGTYPE_PRIMATTR)
					{
						continue;
					}

					uNode = ArgumentToNode(&psRegState->sRAData, &psInst->asArg[i]);
					if ((GetBit(auSrcInGroup, i) != 1) &&
						IsNodeInGroup(&psRegState->sRAData, uNode))
					{
						RemoveFromGroup(&psRegState->sRAData, uNode);
					}
				}

				/* Add moves for the sources */
				uAddMoves |= (1U << 1);
			}
			/* Need to add the moves before anything else can be done. */
			*puAddMoves = uAddMoves;
			return;
		}
	}
}

static
IMG_VOID RegGroupingBlock(PINTERMEDIATE_STATE psState,
						  PRALSCAN_STATE psRegState,
						  PCODEBLOCK psCodeBlock,
						  IMG_UINT32 uIteration,
						  IMG_PBOOL pbRestart)
/*****************************************************************************
 FUNCTION	: RegGroupingBlock

 PURPOSE	: Form mandatory register groups for instructions in an block.

 PARAMETERS	: psState          - Compiler state
              psRegState       - Register allocator state
              psCodeBlock      - Block to process
              uIteration       - Number of times this block has been processed
                                 (Always < 2)

 OUTPUT		: pbRestart        - Whether a register re-allocation is needed.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST psInst;
    PREGISTER_USEDEF psNodeDef = NULL;
	PARG* apsNodeRepl = NULL;
	IMG_UINT32 uNodeReplSize = 0;
	PINST psNextInst;

	/* Initialise substitution record */
	psNodeDef = UscAlloc(psState, sizeof(REGISTER_USEDEF));
	InitRegUseDef(psNodeDef);

	uNodeReplSize = sizeof(PARG) * psRegState->sRAData.uNrRegisters;
	apsNodeRepl = UscAlloc(psState, uNodeReplSize);
	memset(apsNodeRepl, 0, uNodeReplSize);

	/*
	   Move forwards through the block constructing and mandatory register grouping.
	*/
	psNextInst = NULL;
	for (psInst = psCodeBlock->psBody; psInst; psInst = psNextInst)
	{
		IMG_UINT32 uAddMoves = 0;
		IMG_UINT32 i;

		psNextInst = psInst->psNext;

		RegGroupingInst(psState, psRegState, psInst,
						&uAddMoves, &psNextInst);

		if (uAddMoves != 0)
		{
			PINST psNewInst;
			PINST psMove = NULL;
			IMG_UINT32 uTempReg = 0;

			/* Can only happen on the first iteration */
			ASSERT(uIteration < 2);

			/* Need to add moves to handle a degenerate register grouping. */
			psNewInst = psInst;
			if (uAddMoves & (1U << 1))  // Bad group in the sources.
			{
				/* Move sources into new, temporary registers. */

				IMG_UINT32 uArgCount = g_psInstDesc[psInst->eOpcode].uArgumentCount;
				for (i = 0; i < uArgCount; i ++)
				{
					if (psInst->asArg[i].uType != USEASM_REGTYPE_TEMP &&
                        psInst->asArg[i].uType != USEASM_REGTYPE_PRIMATTR)
					{
						continue;
					}

					uTempReg = psState->uNumRegisters++;

					psMove = FormMove(psState,
									  NULL, &psInst->asArg[i],
									  psInst);
                    psMove->asDest[0].uType = USEASM_REGTYPE_TEMP;
                    psMove->asDest[0].uNumber = uTempReg;

                    psInst->asArg[i].uType = psMove->asDest[0].uType;
                    psInst->asArg[i].uNumber = psMove->asDest[0].uNumber;
					psInst->asArg[i].bKilled = IMG_TRUE;

					InsertInstBefore(psState, psCodeBlock, psMove, psInst);
				}
			}
			if (uAddMoves & (1U << 0))   // Bad group in the sources.
			{
				/* Move destinations  into new, temporary registers. */
				uTempReg = psState->uNumRegisters;
				psState->uNumRegisters += psInst->uDestCount;

				for (i = 0; i < psInst->uDestCount; i ++)
				{
					psMove = FormMove(psState,
									  &psInst->asDest[i], NULL,
									  psInst);
					psMove->asArg[0].uType = USEASM_REGTYPE_TEMP;
					psMove->asArg[0].uNumber = uTempReg + i;
					psInst->asArg[0].bKilled = IMG_TRUE;

					InsertInstBefore(psState, psCodeBlock, psMove, psInst->psNext);

					psNewInst = psMove;

					psInst->asDest[i].uType = USEASM_REGTYPE_TEMP;
					psInst->asDest[i].uNumber = uTempReg + i;

  				}
			}

			/* Go round again with the new set of instructions */
			*pbRestart = IMG_TRUE;
		}
	}

	/* Free data */
	ClearRegUseDef(psState, psNodeDef);
	UscFree(psState, psNodeDef);

	UscFree(psState, apsNodeRepl);
}

static
IMG_VOID InitCallbackData(PRALSCAN_STATE psRegState,
						  LIVE_CALLBACK_FN pfFn,
						  PUSC_LIVE_CALLBACK psData)
/*****************************************************************************
 FUNCTION	: InitCallbackData

 PURPOSE	: Initialise callback data

 PARAMETERS	: psRegState	- Register allocator state
              pfFn          - Callback function
              psData        - Call back structure to initialise

 RETURNS	: Nothing.
*****************************************************************************/
{
    PUSC_CALLBACK_DATA psBundle;

	memset(psData, 0, sizeof(*psData));

	psBundle = (PUSC_CALLBACK_DATA)UscAlloc(psRegState->sRAData.psState,
											sizeof(USC_CALLBACK_DATA));
	memset(psBundle, 0, sizeof(USC_CALLBACK_DATA));
	psBundle->psRegState = psRegState;

	psData->pfFn = pfFn;
	psData->pvFnData = (IMG_PVOID)psBundle;
	psData->eRegType = USC_LCB_TYPE_UNDEF;
}

static
IMG_VOID ClearCallbackData(PINTERMEDIATE_STATE psState,
						   PUSC_LIVE_CALLBACK psCallBack)
/*****************************************************************************
 FUNCTION	: ClearCallbackData

 PURPOSE	: Clear a callback data structure

 PARAMETERS	: psState        - Compiler state
              psCallBack     - Call back structure to clear

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psCallBack != NULL)
	{
	    PUSC_CALLBACK_DATA psBundle = (PUSC_CALLBACK_DATA)psCallBack->pvFnData;
	    UscFree(psState, psBundle);
	}
}

static
IMG_BOOL ProcListIntvlIn(PRALSCAN_STATE psRegState,
						 PUSC_REG_INTERVAL psInterval)
/*****************************************************************************
 FUNCTION	: ProcListIntvlIn

 PURPOSE	: Test whether interval is in a processing list

 PARAMETERS	: psRegState        - Register allocator state
              psInterval        - Interval to test

 RETURNS	: IMG_TRUE iff interval is in a list
*****************************************************************************/
{
    if (psInterval == NULL)
        return IMG_FALSE;

    if (psInterval->psProcNext != NULL ||
		psInterval->psProcPrev != NULL)
	{
		return IMG_TRUE;
	}
	if (psRegState->psFixed == psInterval ||
	    psRegState->psIntervals == psInterval)
	{
	    return IMG_TRUE;
	}

	return IMG_FALSE;
}

static
IMG_VOID ProcListRemoveIntvl(PRALSCAN_STATE psRegState,
                             PUSC_REG_INTERVAL psInterval)
/*****************************************************************************
 FUNCTION	: ProcListRemoveIntvl

 PURPOSE	: Remove interval to the list of intervals to be processed.

 PARAMETERS	: psRegState        - Register allocator state
              psInterval        - Interval to add

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (GetBit(psRegState->auIsFixed, psInterval->uNode))
	{
		psRegState->psFixed = ProcListRemove(psInterval, psRegState->psFixed);
	}
	else
	{
		psRegState->psIntervals = ProcListRemove(psInterval, psRegState->psIntervals);
	}
}

static
IMG_VOID ProcListAddIntvl(PRALSCAN_STATE psRegState,
						  PUSC_REG_INTERVAL psInterval)
/*****************************************************************************
 FUNCTION	: ProcListAddIntvl

 PURPOSE	: Add an interval to the list of intervals to be processed.

 PARAMETERS	: psRegState        - Register allocator state
              psInterval        - Interval to add

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINTERMEDIATE_STATE psState = psRegState->sRAData.psState;
	if (GetBit(psRegState->auIsFixed, psInterval->uNode))
	{
		psRegState->psFixed = ProcListAdd(psInterval, psRegState->psFixed);
		ASSERT(psRegState->psFixed->psProcPrev == NULL);
	}
	else
	{
		psRegState->psIntervals = ProcListAdd(psInterval, psRegState->psIntervals);
		ASSERT(psRegState->psIntervals->psProcPrev == NULL);
	}
}

static
PUSC_REG_INTERVAL RegIntvlPush(PREGALLOC_DATA psRAData,
							   PUSC_REG_INTERVAL *apsRegInterval,
							   IMG_BOOL bCombine,
							   IMG_UINT32 uReg,
							   IMG_UINT32 uStart,
							   IMG_UINT32 uEnd)
/*****************************************************************************
 FUNCTION	: RegIntvlPush

 PURPOSE	: Push a register interval on to the front of a list.

 PARAMETERS	: psRAData       - Reg. Allocator data
              apsRegInterval - Array of interval lists to build up.
              bCombine      - Whether to try to combine interval with existing interval
              bDest         - Whether node is unconditionall written
              uChanMask     - Mask of channels written
              uReg          - Register number
              uStart        - Start of interval or 0
              uEnd          - End of interval or 0

 RETURNS	: The interval created for the liveness range. If the interval is
              new, it is always put at the front of the list of intervals
              for the register (psRegState->apsRegInterval[uReg]).
*****************************************************************************/
{
	PUSC_REG_INTERVAL psInterval = NULL, psNext = NULL;
	PINTERMEDIATE_STATE psState = psRAData->psState;
	IMG_BOOL bRet = IMG_FALSE;

	ASSERT(uReg < psRAData->uNrRegisters);

	if (bCombine)
	{
		PUSC_REG_INTERVAL psCurr;

		bCombine = IMG_FALSE;
		for (psCurr = apsRegInterval[uReg];
			 psCurr != NULL;
			 psCurr = psCurr->psRegNext)
		{
		    if (GetBit(psCurr->auFlags, INTVL_PARAM))
			{
				bCombine = IMG_TRUE;
			}
			else if	(!EarlierThan(psCurr->uStart, uStart) &&
					 GetBit(psCurr->auFlags, INTVL_WRITTEN) &&
					 !GetBit(psCurr->auFlags, INTVL_FUNCTION))
			{
		        /* Interval is written, can't extend it */
		        bCombine = IMG_FALSE;
			}
            else
			{
				bCombine = IMG_TRUE;
			}

			if (bCombine)
			{
				psInterval = psCurr;
				break;
			}
		}
    }

	if (bCombine)
	{
		/* Set the new end-points of the interval */
		if (EarlierThan(uStart, psInterval->uStart))
			psInterval->uStart = uStart;

		if (EarlierThan(psInterval->uEnd, uEnd))
			psInterval->uEnd = uEnd;

		bRet = IMG_TRUE;
	}
	else
	{
		/* Start a new interval */

		psInterval = UscAlloc(psState, sizeof(USC_REG_INTERVAL));
		InitRegInterval(psInterval, uReg);

		psInterval->uStart = uStart;
		psInterval->uEnd = uEnd;

		psNext = apsRegInterval[uReg];
		apsRegInterval[uReg] = psInterval;

		psInterval->psRegNext = psNext;
		if (psNext != NULL)
			psNext->psRegPrev = psInterval;
	}

	return psInterval;
}

static
PUSC_REG_INTERVAL CombineIntervals(PRALSCAN_STATE psRegState,
								   PUSC_REG_INTERVAL psInterval1,
								   PUSC_REG_INTERVAL psInterval2)
/*****************************************************************************
 FUNCTION   : CombineIntervals

 PURPOSE    : Merge intervals, remove them from register state lists

 PARAMETERS : psRegState    - Register allocator state
              psInterval1    - First interval and destination of merge.
              psInterval2    - Second interval.

 NOTES      : Deletes psInterval2

 RETURNS    : psInterval1 extended to include psInterval2.
              NULL if intervals are not compatible;
*****************************************************************************/
{
	psInterval1 = MergeIntervals(psInterval1, psInterval2);

    /* Completely delete psInterval2 */
    if (psRegState != NULL)
    {
        PINTERMEDIATE_STATE psState = psRegState->sRAData.psState;
        IMG_UINT32 uNode = psInterval2->uNode;
        PUSC_REG_INTERVAL psTmp, psList;

        psList = psRegState->apsRegInterval[uNode];
        psTmp = RegIntvlDrop(psInterval2, psList);
        if (psList == psInterval2)
        {
            psRegState->apsRegInterval[uNode] = psTmp;
        }

        ProcListRemoveIntvl(psRegState, psInterval2);

        /* Make sure that the removal took hold */
        ASSERT(psInterval2->psRegNext == NULL && psInterval2->psRegPrev == NULL);
        ASSERT(psInterval2->psProcNext == NULL && psInterval2->psProcPrev == NULL);

		DeleteRegInterval(psState, psInterval2);
    }

    return psInterval1;
}

static
PUSC_REG_INTERVAL MergeIntervalList(PRALSCAN_STATE psRegState,
									PUSC_REG_INTERVAL psFirst)
/*****************************************************************************
 FUNCTION   : MergeIntervalList

 PURPOSE    : Merge a list of intervals, connected by psRegNext/psRegPrev

 PARAMETERS : psRegState    - Register allocator state
              psFirst       - First interval in list and destination of merge.

 NOTES      : Deletes all intervals except psFirst

 RETURNS    : psFirst extended to include subsequent interval
*****************************************************************************/
{
	PUSC_REG_INTERVAL psCurr, psNext;

	if (psFirst == NULL)
		return NULL;

	psNext = NULL;
	for (psCurr = psFirst->psRegNext;
		 psCurr != NULL;
		 psCurr = psNext)
	{
		psNext = psCurr->psRegNext;

		psFirst = CombineIntervals(psRegState, psFirst, psCurr);
	}

	return psFirst;
}

static
PUSC_REG_INTERVAL RegIntvlInsert(PRALSCAN_STATE psRegState,
								 PUSC_REG_INTERVAL psInterval,
								 PUSC_REG_INTERVAL psList,
								 IMG_BOOL bMerge)
/*****************************************************************************
 FUNCTION   : RegIntvlInsert

 PURPOSE    : Insert a register interval into a list, connected by psRegNext/psRegPrev,
              sorted by start and end points

 PARAMETERS : psRegState    - Register allocator state
              psInterval    - Interval to insert
              psList        - List to update.
              bMerge        - Whether to try to merge the interval with
                              an existing, overlapping interval.

 RETURNS    : The updated list
*****************************************************************************/
{
	PUSC_REG_INTERVAL psPoint, psRet;

	if (psInterval == NULL)
	    return psList;
	if (psList == NULL)
	    return psInterval;

	psPoint = RegIntvlFindPos(psList, psInterval->uStart, psInterval->uEnd);

    psRet = NULL;
    if (psPoint == NULL)
	{
        /* Interval is earlier than any other interval */
        if (bMerge &&
            IntervalOverlap(psInterval->uStart, psInterval->uEnd,
                            psList->uStart, psList->uEnd))
        {
            psRet = CombineIntervals(psRegState, psInterval, psList);
        }
        if (psRet == NULL)
	    {
	        psInterval->psRegNext = psList;
	        if (psInterval->psRegNext != NULL)
	        {
	            psInterval->psRegNext->psRegPrev = psInterval;
	        }
	        psRet = psInterval;
	    }
	}
	else
	{
        if (bMerge &&
            IntervalOverlap(psPoint->uStart, psPoint->uEnd,
                            psInterval->uStart, psInterval->uEnd))
        {
            psRet = CombineIntervals(psRegState, psPoint, psInterval);
        }
        if (psRet == NULL)
        {
            psInterval->psRegPrev = psPoint;
            psInterval->psRegNext = psPoint->psRegNext;
            if (psInterval->psRegNext != NULL)
            {
                psInterval->psRegNext->psRegPrev = psInterval;
            }

            psPoint->psRegNext = psInterval;
            psRet = psPoint;
        }
	}
    return psRet;
}

static
IMG_VOID CutTieInterval(PUSC_REG_INTERVAL psInterval)
/*****************************************************************************
 FUNCTION	: CutTieInterval

 PURPOSE	: Disconnect an interval from a tie list

 PARAMETERS	: psInterval    - Interval to disconnect

 RETURNS	: Nothing
*****************************************************************************/
{
	if (psInterval == NULL)
		return;

	if (psInterval->psTiePrev != NULL)
		psInterval->psTiePrev->psTieNext = psInterval->psTieNext;

	if (psInterval->psTieNext != NULL)
		psInterval->psTieNext->psTiePrev = psInterval->psTiePrev;

	psInterval->psTiePrev = NULL;
	psInterval->psTieNext = NULL;
}

IMG_INTERNAL
IMG_BOOL InsertTieInterval(PINTERMEDIATE_STATE psState,
						   PUSC_REG_INTERVAL psInterval,
						   PUSC_REG_INTERVAL psList)
/*****************************************************************************
 FUNCTION	: InsertTie

 PURPOSE	: Insert an interval into a tie list

 PARAMETERS	: psState		- Compiler state.
              psInterval    - Interval to insert
              psList        - List

 NOTES      : List is orderd by node and then pointer

 RETURNS	: True if interval was added to the list, false if the interval
              was already in the list.
*****************************************************************************/
{
	PINTERMEDIATE_STATE psState = psRegState->sRAData.psState;
	PUSC_REG_INTERVAL psPoint;

	if (psInterval == NULL)
		return IMG_FALSE;

	/* Check the interval is disconnected */
	ASSERT(psInterval->psTiePrev == NULL);
	ASSERT(psInterval->psTieNext == NULL);

	/* Check for an empty list */
	if (psList == NULL)
		return IMG_TRUE;

	/* Rewind to first node which is smaller than psInterval */
	for ( /* skip */; psList->psTiePrev != NULL; psList = psList->psTiePrev)
	{
		if (psInterval->uNode == psList->uNode)
		{
			return IMG_FALSE;
		}

		if (psInterval->uNode >= psList->uNode)
		{
			if (((IMG_UINT32)psInterval) > ((IMG_UINT32)psList))
			{
				break;
			}
		}
	}

	/* Move forward to last node which is smaller than psInterval */
	psPoint = psList;
	for ( /* skip */ ; psList != NULL; psList = psList->psTieNext)
	{
		if (psInterval->uNode == psList->uNode)
		{
			return IMG_FALSE;
		}

		psPoint = psList;
		if (psInterval->uNode < psList->uNode)
		{
			if (((IMG_UINT32)psInterval) < ((IMG_UINT32)psList))
			{
				break;
			}
		}
	}

	/* Insert into list */
	if (psList == NULL)
	{
		/*
		  Interval is larger than any in the list,
		  psPoint is the last element in the list.
		*/
		ASSERT(psPoint->psTieNext == NULL);

		psPoint->psTieNext = psInterval;
		psInterval->psTiePrev = psPoint;
	}
	else
	{
		/* psList is the first element larger than psInterval */
		psInterval->psTiePrev = psList->psTiePrev;
		if (psInterval->psTiePrev != NULL)
			psInterval->psTiePrev->psTieNext = psInterval;

		psInterval->psTieNext = psList;
		psList->psTiePrev = psInterval;
	}

	/* Done */
	return IMG_TRUE;
}

static
IMG_VOID MergeTieLists(PRALSCAN_STATE psRegState,
					   PUSC_REG_INTERVAL psLeft,
					   PUSC_REG_INTERVAL psRight)
/*****************************************************************************
 FUNCTION	: MergeTieLists

 PURPOSE	: Merge lists of tied intervals

 PARAMETERS	: psRegState      - Register allocater state
              psLeft, psRight - Lists to merge

 RETURNS	: Nothing
*****************************************************************************/
{
	PUSC_REG_INTERVAL psCurr = NULL;
	PUSC_REG_INTERVAL psNext = NULL;
	PUSC_REG_INTERVAL psPoint = NULL;
	PUSC_REG_INTERVAL psLeftNext = NULL;


	/* Test for nothing to do */
	if (psLeft == NULL)
	{
		return;
	}

	/*
	  Iterate over left hand list, inserting its elements into the
	  right hand list.
	*/

	psPoint = psRight;
	psLeftNext = psLeft->psTieNext;

	/* Go backwards, towards the start */
	psNext = NULL;
	for (psCurr = psLeft->psTiePrev; psCurr != NULL; psCurr = psNext)
	{
		if (psCurr == psRight)
		{
			return;
		}
		psNext = psCurr->psTiePrev;

		CutTieInterval(psCurr);
		if (!InsertTieInterval(psRegState, psCurr, psPoint))
		{
			/* Node is already in the list */
			return;
		}
		psPoint = psCurr;
	}

	/* Go Forwards, towards the end */
	psNext = NULL;
	for (psCurr = psLeft; psCurr != NULL; psCurr = psNext)
	{
		if (psCurr == psRight)
		{
			return;
		}

		psNext = psCurr->psTieNext;
		CutTieInterval(psCurr);
		if (!InsertTieInterval(psRegState, psCurr, psPoint))
		{
			/* Node is already in the list */
			return;
		}
 	}
}

static
IMG_VOID SetTieColour(PRALSCAN_STATE psRegState,
					  PUSC_REG_INTERVAL psReg,
					  IMG_UINT32 uColour)
/*****************************************************************************
 FUNCTION	: SetTieColour

 PURPOSE	: Put the interval with the active colour at the front of the list.

 PARAMETERS	: psReg          - Interval
              uColour        - Colour

 RETURNS	: Nothing
*****************************************************************************/
{
	PINTERMEDIATE_STATE psState = psRegState->sRAData.psState;
	PUSC_REG_INTERVAL psCurr;
	PUSC_REG_INTERVAL psNext;
	IMG_BOOL bIsFixed;

	/*
	  Iterate through the list, setting the colour and disconnecting the
	  intervals which are not compatible
	*/

	psNext = NULL;

	ASSERT (psReg != NULL);

	/* Set current interval */
	if (psReg->uColour < psRegState->sRAData.uAvailRegs)
	{
		/* Test whether tie group has a different colour */
		if (psReg->uColour != uColour)
		{
			CutTieInterval(psReg);
			psReg->uColour = uColour;

			return;
		}
		else
		{
			ASSERT(psReg->uColour == uColour);
			return;
		}
	}
	psReg->uColour = uColour;

	/* Go backwards */
	for (psCurr = psReg->psTiePrev;
		 psCurr != NULL;
		 psCurr = psNext)
	{
		IMG_UINT32 uCurrColour;
		psNext = psCurr->psTiePrev;

		bIsFixed = GetBit(psRegState->auIsFixed, psCurr->uNode);
		if (bIsFixed)
		{
			psCurr->uColour = psRegState->auFixedColour[psCurr->uNode];
		}

		uCurrColour = psCurr->uColour;
		if (uCurrColour < psRegState->sRAData.uAvailRegs)
		{
			if (uCurrColour != uColour)
			{
				/* Disconnect for incompatible colour */
				CutTieInterval(psCurr);
				continue;
			}
		}
		else if (!bIsFixed)
		{
			psCurr->uColour = uColour;
		}
	}

	/* Go Forwards */
	for (psCurr = psReg->psTieNext;
		 psCurr != NULL;
		 psCurr = psNext)
	{
		IMG_UINT32 uCurrColour;
		psNext = psCurr->psTieNext;

		bIsFixed = GetBit(psRegState->auIsFixed, psCurr->uNode);
		if (bIsFixed)
		{
			psCurr->uColour = psRegState->auFixedColour[psCurr->uNode];
		}

		uCurrColour = psCurr->uColour;
		if (uCurrColour < psRegState->sRAData.uAvailRegs)
		{
			if (uCurrColour != uColour)
			{
				/* Disconnect for incompatible colour */
				CutTieInterval(psCurr);
				continue;
			}
		}
		else if (!bIsFixed)
		{
			psCurr->uColour = uColour;
		}
	}

	bIsFixed = GetBit(psRegState->auIsFixed, psReg->uNode);
	ASSERT((bIsFixed && psReg->uColour == psRegState->auFixedColour[psReg->uNode]) ||
		   (psReg->uColour == uColour));
}

static
IMG_BOOL GetTieColour(PRALSCAN_STATE psRegState,
					  PUSC_REG_INTERVAL psReg,
					  IMG_PUINT32 puColour)
/*****************************************************************************
 FUNCTION	: GetTieColour

 PURPOSE	: Get the colour of a tied interval

 PARAMETERS	: psRegState     - Register allocater state
              psReg          - Interval

 OUTPUT     : puColour       - Colour

 RETURNS	: IMG_TRUE iff interval was tied and had a colour
*****************************************************************************/
{
	PINTERMEDIATE_STATE psState = psRegState->sRAData.psState;
	IMG_UINT32 uColour;
	IMG_BOOL bIsFixed;

	uColour = psReg->uColour;
	bIsFixed = GetBit(psRegState->auIsFixed, psReg->uNode);

	if (bIsFixed)
	{
		uColour = psRegState->auFixedColour[psReg->uNode];
		psReg->uColour = uColour;
	}
	else if (uColour < psRegState->sRAData.uAvailRegs)
	{
		ASSERT((!bIsFixed) || (uColour == psRegState->auFixedColour[psReg->uNode]));
	}
	else
	{
		return IMG_FALSE;
	}

	if (puColour != NULL)
		(*puColour) = uColour;
	return IMG_TRUE;
}

static
IMG_BOOL TieRegister(PRALSCAN_STATE psRegState,
					 PINST psInst,
					 PUSC_REG_INTERVAL psNew,
					 IMG_BOOL bDest)
/*****************************************************************************
 FUNCTION	: TieRegister

 PURPOSE	: Try to tie a register to an instruction source

 PARAMETERS	: psRegState     - Register allocater state
              psInst         - Instruction registers meet at
              psNew          - Register to tie
              bDest          - Destination or source

 RETURNS	: IMG_TRUE iff registers were tied
*****************************************************************************/
{
	PINTERMEDIATE_STATE psState = psRegState->sRAData.psState;
	PARG psDst, psSrc;
	PUSC_REG_INTERVAL psReg;
	IMG_UINT32 uDstNode, uSrcNode;

	/* Test for an instruction */
	if (psInst == NULL)
	{
		return IMG_FALSE;
	}

	/* Test for predication */
	if (psInst->uPredSrc != USC_PREDREG_NONE)
	{
		/* No predicated instructions */
		return IMG_FALSE;
	}

	/* Test instruction type */
	if (!(psInst->eOpcode == IMOV))
	{
		return IMG_FALSE;
	}

	psDst = &psInst->asDest[0];
	uDstNode = ArgumentToNode(&psRegState->sRAData, psDst);

	psSrc = &psInst->asArg[0];
	uSrcNode = ArgumentToNode(&psRegState->sRAData, psSrc);

#ifdef DEBUG
	/* Check nodes match */
	if (bDest)
	{
		ASSERT(uDstNode == psNew->uNode);
	}
	else
	{
		ASSERT(uSrcNode == psNew->uNode);
	}
#endif

	/* Check for a register array */
	if (psDst->uType == USC_REGTYPE_REGARRAY ||
		psSrc->uType == USC_REGTYPE_REGARRAY)
	{
		return IMG_FALSE;
	}

	/* Test for an identity operation */
	if (uDstNode == uSrcNode)
	{
		return IMG_FALSE;
	}

	/* Test for src being killed */
	if (!psSrc->bKilled)
	{
		return IMG_FALSE;
	}

	/* Check for fixed nodes */
	if (GetBit(psRegState->auIsFixed, uDstNode) &&
		GetBit(psRegState->auIsFixed, uSrcNode))
	{
		return IMG_FALSE;
	}

	/* Get the interval for the source */
	if (bDest)
	{
		psReg = RegIntvlFindWith(psRegState->apsRegInterval[uSrcNode],
								 psInst->uId,
								 IMG_FALSE);
	}
	else
	{
		psReg = RegIntvlFindWith(psRegState->apsRegInterval[uDstNode],
								 psInst->uId,
								 IMG_TRUE);
	}
	/* Test that there is an interval to tie */
	if (psReg == NULL)
	{
		return IMG_FALSE;
	}
	ASSERT(psReg != NULL);

	/* Tie the registers */
	MergeTieLists(psRegState, psNew, psReg);

	return IMG_TRUE;
}

static
IMG_VOID ProcIntervalDst(PRALSCAN_STATE psRegState,
						 PUSC_CALLBACK_DATA psBundle,
						 IMG_UINT32 uInstIdx,
						 PINST psInst,
						 PARG psArg,
						 IMG_BOOL bDoubleIndex)
/*****************************************************************************
 FUNCTION	: ProcIntervalDst

 PURPOSE	: Compute liveness interval for a destination node

 PARAMETERS	: psRegState    - Register allocater state
              psBundle      - Callback data
              uInstIdx      - Current instruction index
              psInst        - Instruction
              psArg         - Register being written
              bDoubleIdx    - Instruction has two indexes.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINTERMEDIATE_STATE psState = psRegState->sRAData.psState;
	PREGALLOC_DATA psRAData = &psRegState->sRAData;
	IMG_BOOL bConditional;     // Instruction is conditionally executed
	IMG_BOOL bExtend;          // Instruction completely overwrites any earlier write
	PUSC_REG_INTERVAL psWrite;
	PUSC_REG_INTERVAL psCurr;  // The current interval
	IMG_UINT32 uInstWriteMask, uLiveDestChans;
	IMG_UINT32 uWriteIdx = uInstIdx;
	IMG_BOOL bRegArray = IMG_FALSE;
	IMG_UINT32 uNode;

	uNode = ArgumentToNode(psRAData, psArg);

	/* Check for a register array */
	if (psArg->uType == USC_REGTYPE_REGARRAY)
		bRegArray = IMG_TRUE;
	else
		bRegArray = IMG_FALSE;

	/* Record the write */
	SetBit(psRegState->auIsWritten, uNode, IMG_TRUE);

	/*
	   Write to register starts an interval but only if
	   the register is read later in the program.
	*/
	ASSERT(uNode < psRAData->uNrRegisters);
	psCurr = psRegState->apsRegInterval[uNode];

	if (psCurr != NULL)
		bExtend = IMG_TRUE;
	else
		bExtend = IMG_FALSE;

	bConditional = (psBundle->bConditional ||
					psInst->uPredSrc != USC_PREDREG_NONE);

	/* Index of instruction with double index is always
	 * one earlier than the actual instruction location */
	if (bDoubleIndex)
	{
		psBundle->uInstIdx += 1;

		uInstIdx = psBundle->uInstIdx;
		uWriteIdx = uInstIdx;
		psInst->uId = uInstIdx;

#ifdef DEBUG
		psRegState->uMaxInstIdx = max(psRegState->uMaxInstIdx, psInst->uId);
#endif
	}

	/* Determine whether new write is completely removed by later write */
	GetDestMask(psInst, &uInstWriteMask);

	uLiveDestChans = psInst->auLiveChansInDest[0];
	uInstWriteMask &= uLiveDestChans;

	/* Get the next interval to write to the node */
	for (psWrite = psCurr;
		 psWrite != NULL;
		 psWrite = psWrite->psRegNext)
	{
		if (GetBit(psWrite->auFlags, INTVL_WRITTEN) ||
			GetBit(psWrite->auFlags, INTVL_CONDWRITTEN))
		{
			/* Found the next write */
			break;
		}
	}

	if (psCurr != NULL)
		bExtend = IMG_TRUE;
	else
		bExtend = IMG_FALSE;

	if (psWrite != NULL)
	{
		IMG_UINT32 uCommonMask;

		/* Try to decide if register is a function input/output */
		if (psBundle->bInFunction &&
			GetBit(psWrite->auFlags, INTVL_MAYBEPARAM))
		{
			/*
			  Register is written in a fuction and is live out
			  of the function block, so treat it as an output.
			*/
			SetBit(psWrite->auFlags, INTVL_PARAM, IMG_TRUE);
		}


		/*
		  Extend interval if:
		  - Channels have been read but not written
		  - Register array
		  - Parameter
		  - Conditionally written
		*/
		uCommonMask = (psWrite->uWriteMask & uInstWriteMask) & USC_DESTMASK_FULL;

		ASSERT(GetBit(psWrite->auFlags, INTVL_CONDWRITTEN) ||
			   GetBit(psWrite->auFlags, INTVL_WRITTEN));

		if (bRegArray ||
			GetBit(psWrite->auFlags, INTVL_PARAM) ||
			GetBit(psWrite->auFlags, INTVL_CONDWRITTEN) ||
			psWrite->uReadMask != 0)
		{
			PUSC_REG_INTERVAL psPrev;
			/* Current instruction is only partially overwritten by later instruction
			 * Extend existing new interval.
			 */
			bExtend = IMG_TRUE;

			/* Remove earlier intervals */
			psPrev = NULL;
			for (psCurr = psWrite->psRegPrev; psCurr != NULL; psCurr = psPrev)
			{
				PUSC_REG_INTERVAL psList;
				psPrev = psCurr->psRegPrev;

				psWrite->uReadMask |= psCurr->uReadMask;

				if (GetBit(psCurr->auFlags, INTVL_PENDING))
				{
					psBundle->psCurrent = ProcListRemove(psCurr, psBundle->psCurrent);
				}
				else
				{
					ProcListRemoveIntvl(psRegState, psCurr);
				}
				psList = RegIntvlDrop(psCurr,
									  psRegState->apsRegInterval[psCurr->uNode]);
				if (psCurr == psRegState->apsRegInterval[psCurr->uNode])
				{
					psRegState->apsRegInterval[psCurr->uNode] = psList;
				}

				DeleteRegInterval(psState, psCurr);
			}
			psCurr = psWrite;
			ASSERT(psRegState->apsRegInterval[psCurr->uNode] == psCurr);
		}
		else
		{
			/* Current instruction is completely overwritten by later instruction */
			bExtend = IMG_FALSE;
		}
	}

	if (bExtend)
	{
		/* No overlapping write */
		/*
		   If an interval doesn't already exist then this
		   register isn't read after this instruction.

		   If an interval exists but hasn't been written than use the
		   earlier of the two possible start-points.
		*/

		/* Test for interval which may be a function parameter */
		if (psBundle->bInFunction)
		{
			SetBit(psCurr->auFlags, INTVL_FUNCTION, IMG_TRUE);

			/* Function parameter */
			if (GetBit(psCurr->auFlags, INTVL_MAYBEPARAM))
			{
				SetBit(psCurr->auFlags, INTVL_MAYBEPARAM, IMG_FALSE);
				SetBit(psCurr->auFlags, INTVL_PARAM, IMG_TRUE);
			}
		}

		/* Set the start index, closing the interval */
		if (EarlierThan(uWriteIdx, psCurr->uStart))
		{
			psCurr->uStart = uWriteIdx;
#ifdef DEBUG
			psCurr->psStartInst = psInst;
#endif
		}
		if (GetBit(psCurr->auFlags, INTVL_PARAM) &&
			EarlierThan(psCurr->uEnd, uWriteIdx))
		{
			psCurr->uEnd = uWriteIdx;
#ifdef DEBUG
			psCurr->psEndInst = psInst;
#endif
		}

		/* Mark interval as written */
		if (bConditional)
		{
			/*
			   Conditional write: Mark interval as being written,
			   but start of the interval has not been found yet.
			*/
			SetBit(psCurr->auFlags, INTVL_CONDWRITTEN, IMG_TRUE);
		}
		else
		{
			/*
			   Unconditional full write marks the start of the interval.
			   Mark the interval as closed.
			   Add the interval either to the list of fixed colour
			   intervals or to the list of general intervals.
			*/
			if ((psCurr->uWriteMask & USC_DESTMASK_FULL) == USC_DESTMASK_FULL)
			{
				if (GetBit(psCurr->auFlags, INTVL_PENDING))
				{
					psBundle->psCurrent = ProcListRemove(psCurr, psBundle->psCurrent);
					SetBit(psCurr->auFlags, INTVL_PENDING, IMG_FALSE);
				}
			}
			SetBit(psCurr->auFlags, INTVL_WRITTEN, IMG_TRUE);
			SetBit(psCurr->auFlags, INTVL_CONDWRITTEN, IMG_FALSE);

			if (!GetBit(psCurr->auFlags, INTVL_PENDING))
			{
				ProcListRemoveIntvl(psRegState, psCurr);
				ProcListAddIntvl(psRegState, psCurr);
			}
		}
	}
	else
	{
		/* No open interval exists */
		PUSC_REG_INTERVAL psTmp;

		/*
		  Create an interval and push it onto the front of the
		  registers interval list
		*/
		psTmp = RegIntvlPush(psRAData, psRegState->apsRegInterval,
							 IMG_TRUE,
							 uNode,
							 uWriteIdx, uWriteIdx);

		if (bConditional)
			SetBit(psTmp->auFlags, INTVL_CONDWRITTEN, IMG_TRUE);
		else
			SetBit(psTmp->auFlags, INTVL_WRITTEN, IMG_TRUE);


		/* Test for interval which may be a function parameter */
		if (psBundle->bInFunction)
		{
			SetBit(psTmp->auFlags, INTVL_FUNCTION, IMG_TRUE);

			/* Function parameter */
			if (GetBit(psTmp->auFlags, INTVL_MAYBEPARAM))
			{
				SetBit(psTmp->auFlags, INTVL_MAYBEPARAM, IMG_FALSE);
				SetBit(psTmp->auFlags, INTVL_PARAM, IMG_TRUE);
			}
		}


		if (psTmp != psCurr)
		{
			ProcListAddIntvl(psRegState, psTmp);
		}

		psCurr = psTmp;
	}

	/* Record write mask */
	psCurr->uWriteMask |= uInstWriteMask;

	/* Clear the channels written from the read mask */
	psCurr->uReadMask &= ~uInstWriteMask;

	/* Mark last access is not a read */
	SetBit(psCurr->auFlags, INTVL_READ, IMG_FALSE);

	/* Activity count */
	psCurr->uAccessCtr += 1;

	/* Extend end points of global intervals */
	if (GetBit(psCurr->auFlags, INTVL_PARAM))
	{
		/*
		  Extend start-point so that the node gets the same colour
		  colour at each call site (Note that the intervals in functions
		  appear to come later than intervals in the main program because
		  the functions are handled first).
		*/

		/* Merge interval list */
		MergeIntervalList(psRegState, psCurr);
	}
}

static
IMG_VOID ProcIntervalSrc(PRALSCAN_STATE psRegState,
						 PUSC_CALLBACK_DATA psBundle,
						 IMG_UINT32 uInstIdx,
						 PINST psInst,
						 PARG psArg,
						 IMG_UINT32 uReadMask,
						 IMG_BOOL bDoubleIndex)
/*****************************************************************************
 FUNCTION	: ProcIntervalSrc

 PURPOSE	: Compute liveness interval for a source node

 PARAMETERS	: psRegState    - Register allocater state
              psBundle      - Callback data
              uInstIdx      - Current instruction index
              psInst        - Instruction
              psArg         - Register being read
              uReadMask     - Mask of channels read from source.
              bDoubleIdx    - Instruction has two indexes.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINTERMEDIATE_STATE psState = psRegState->sRAData.psState;
	PREGALLOC_DATA psRAData = &psRegState->sRAData;
	PUSC_REG_INTERVAL psNodeInterval;
	IMG_BOOL bNewInterval;         // Whether to create a new interval
	IMG_UINT32 uReadIdx;
	IMG_BOOL bRegArray;
	IMG_UINT32 uNode;

	uNode = ArgumentToNode(psRAData, psArg);
	ASSERT(uNode < psRAData->uNrRegisters);

	/* Check for a register array */
	if (psArg->uType == USC_REGTYPE_REGARRAY)
		bRegArray = IMG_TRUE;
	else
		bRegArray = IMG_FALSE;

	/* Source of instruction with double index is always
	 * one later than the instruction index */
	uReadIdx = uInstIdx;
	if (bDoubleIndex)
	{
		ASSERT(psInst->uId > 1);
		uReadIdx = psInst->uId - 1;
	}

	/* Get most recent interval for the node */
	psNodeInterval = psRegState->apsRegInterval[uNode];

	/* Test for whether to start a new interval */
	bNewInterval = IMG_FALSE;
	if (psNodeInterval == NULL)
	{
		/* No existing interval */
		bNewInterval = IMG_TRUE;
	}
	else if (psArg->bKilled)
	{
		/* Register killed */
		bNewInterval = IMG_TRUE;
	}
	else if ((psNodeInterval->uReadMask & USC_DESTMASK_FULL) == 0)
	{
		/* All channels read have been written */
		bNewInterval = IMG_TRUE;
	}

	/* Test for register arrays */
	if (bRegArray)
	{
		bNewInterval = IMG_FALSE;
	}
	/* Test for function parameters */
	else if (psNodeInterval != NULL &&
			 GetBit(psNodeInterval->auFlags, INTVL_PARAM))
	{
		bNewInterval = IMG_FALSE;
	}

	if (bNewInterval)
	{
		PUSC_REG_INTERVAL psTmp;

		/*
		   Create a new interval, put it at the front of the
		   registers interval list
		*/
		psTmp = RegIntvlPush(psRAData, psRegState->apsRegInterval,
							 IMG_TRUE,  uNode,
							 uReadIdx, uReadIdx);
		psTmp->uStart = uReadIdx + 1;
#ifdef DEBUG
		psTmp->psEndInst = psInst;
#endif
		SetBit(psTmp->auFlags, INTVL_READ, IMG_TRUE);
		if (psTmp != psNodeInterval)
		{
			/*
			   Created a new node interval, put it at the front of
			   the open interval list
			*/
			psBundle->psCurrent = ProcListCons(psRegState->apsRegInterval[uNode],
											   psBundle->psCurrent);
			SetBit(psRegState->apsRegInterval[uNode]->auFlags,
				   INTVL_PENDING,
				   IMG_TRUE);
		}
		psNodeInterval = psTmp;
	}
    else if (!EarlierThan(psNodeInterval->uStart, uReadIdx))
	{
		/*
		  Increase the start to include the current read,
		  use the preceding instruction to start since
		  the start of an interval must be a write.
		*/
		psNodeInterval->uStart = uReadIdx + 1;
	}

	/* Set the read mask */
	psNodeInterval->uReadMask |= uReadMask;

	/* Registers in function bodies get special treatment */
	if (psBundle->bInFunction)
	{
		SetBit(psNodeInterval->auFlags, INTVL_FUNCTION, IMG_TRUE);
	}
	SetBit(psNodeInterval->auFlags, INTVL_READ, IMG_TRUE);
	/* Activity count */
	psNodeInterval->uAccessCtr += 1;

	/* Try to tie register to a destination */
	TieRegister(psRegState, psInst, psNodeInterval, IMG_FALSE);
}

static
IMG_VOID ProcRegIntoBlock(PREGALLOC_DATA psRAData,
						  PUSC_REG_INTERVAL *apsRegInterval,
						  PUSC_CALLBACK_DATA psBundle,
						  PCODEBLOCK psCodeBlock,
						  IMG_UINT32 uRegNode)
/*****************************************************************************
 FUNCTION	: ProcRegIntoBlock

 PURPOSE	: Process registers live into a block

 PARAMETERS	: psRAData       - Allocator data
              apsRegInterval - Array of intervals to build up
              psBundle       - Callback data
              psCodeBlock    - Code block to process
              uRegNode       - Register node to process

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINTERMEDIATE_STATE psState = psRAData->psState;
	IMG_UINT32 uInstIdx = psBundle->uInstIdx;
	PUSC_REG_INTERVAL psInterval = NULL;
	IMG_UINT32 uLiveMask = 0;
	USC_REGTYPE uRegType = USC_REGTYPE_UNDEF;
	IMG_UINT32 uRegNum = USC_UNDEF;

	NodeToReg(psRAData, uRegNode, &uRegType, &uRegNum);

	uLiveMask = GetRegisterLiveMask(psState,
									&psCodeBlock->sRegistersLiveOut,
									uRegType, uRegNum, 0);

	if (uLiveMask != 0)
	{
		PUSC_REG_INTERVAL psCurr = apsRegInterval[uRegNode];

		if (apsRegInterval[uRegNode] == NULL ||
			apsRegInterval[uRegNode]->uStart != USC_OPEN_START)
		{
			IMG_UINT32 uStart = uInstIdx, uEnd = uInstIdx;
			/*
			   Push the temp reg interval onto the front
			   of the reg interval list
			*/
			psInterval = RegIntvlPush(psRAData, apsRegInterval,
									  IMG_TRUE,
									  uRegNode, uStart, uEnd);

			psInterval->uReadMask |= uLiveMask;
			SetBit(psInterval->auFlags, INTVL_READ, IMG_TRUE);

			/*
			 * Registers which live at the end of a
			 * function and which are written in
			 * the function are an output for the
			 * function. Function outputs are
			 * treated as live for the whole program.
			 */
			if (psBundle->bInFunction)
			{
				SetBit(psInterval->auFlags, INTVL_FUNCTION, IMG_TRUE);

				/* Mark interval as a possible input/output */
				if (!GetBit(psInterval->auFlags, INTVL_PARAM))
				{
					SetBit(psInterval->auFlags, INTVL_MAYBEPARAM, IMG_TRUE);
				}
			}

			if (psCurr != psInterval)
			{
				/* Created a new node interval */
				psBundle->psCurrent = ProcListCons(psInterval,
												   psBundle->psCurrent);
				SetBit(psInterval->auFlags, INTVL_PENDING, IMG_TRUE);

				psCurr = psInterval;
			}
		}
		if (psCurr != NULL &&
			IsCall(psState, psCodeBlock))
		{
			SetBit(psCurr->auFlags, INTVL_MAYBEPARAM, IMG_TRUE);
		}
	}
}

static
IMG_VOID LiveIntervalBlock(PUSC_LIVE_CALLBACK psCBData)
/*****************************************************************************
 FUNCTION	: LiveIntervalBlock

 PURPOSE	: Calculate liveness around a code block

 PARAMETERS	: psCBData      - Callback data

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_CALLBACK_DATA psBundle;
	PRALSCAN_STATE psRegState;
	PINTERMEDIATE_STATE psState;
	PINST psInst;
	IMG_UINT32 uInstIdx;
	IMG_BOOL bDoubleIndex;
	PCODEBLOCK psCodeBlock;

	/* Extract data from call back */
	psBundle = (PUSC_CALLBACK_DATA)psCBData->pvFnData;
	psState = NULL;
	ASSERT(psBundle != NULL);

	psRegState = psBundle->psRegState;
	psState = psRegState->sRAData.psState;

	psInst = psCBData->psInst;
    bDoubleIndex = IMG_FALSE;

    uInstIdx = psBundle->uInstIdx;

	psCodeBlock = psCBData->psBlock;

	/*
	 *  Process callback
	 */

	/*
	   If this is the start of a code block, make sure that all
	   registers live on exit have a liveness interval with an open
	   start.
	*/
	if(psCBData->ePos == USC_LCB_ENTRY)
	{
		IMG_UINT32 uReg;

		/* Set the loop depth. */
		psBundle->uLoopDepth = LoopDepth(psCodeBlock);

		/* Test for conditional block */
		if (psBundle->uLoopDepth == 0 &&
			Dominates(psState, psCodeBlock, psCodeBlock->psOwner->psExit))
		{
			psBundle->bConditional = IMG_FALSE;
		}
		else
		{
			psBundle->bConditional = IMG_TRUE;
		}

		/* Test for function */
		if (IsCall(psState, psCodeBlock))
		{
			return;
		}

		/* Test for split feedback */
		if (psCodeBlock == psState->psPreFeedbackBlock)
		{
			psBundle->uSplitFeedBackIdx = uInstIdx;
		}

		/* Test for split */
		if (psCodeBlock == psState->psPreSplitBlock)
		{
			psBundle->uSplitIdx = uInstIdx;
		}

		/* Add the PA and temporary registers */
		for (uReg = 0; uReg < psRegState->sRAData.uNrRegisters; uReg++)
		{
			ProcRegIntoBlock(&(psRegState->sRAData), psRegState->apsRegInterval,
							 psBundle, psCodeBlock, uReg);
		}
	}
	/* End of Codeblock Entry */

	/* Codeblock Exit */
	/* Nothing to do */
	/* End of Codeblock Exit */
	return;
}

static
IMG_VOID LiveIntervalCB(PUSC_LIVE_CALLBACK psCBData)
/*****************************************************************************
 FUNCTION	: LiveIntervalCB

 PURPOSE	: Callback to calculate liveness intervals of temporary registers

 PARAMETERS	: psCBData      - Callback data

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_CALLBACK_DATA psBundle;
	PRALSCAN_STATE psRegState;
	PREGALLOC_DATA psRAData;
	PINTERMEDIATE_STATE psState;
	IMG_UINT32 uInstIdx;
	IMG_BOOL bDoubleIndex;
	ARG sArg;
	IMG_UINT32 uArgNum;
	IMG_BOOL bHasArgNum;
	PINST psInst;

	if (psCBData == NULL)
	{
		return;
	}

	/* Extract data from call back */
	psBundle = (PUSC_CALLBACK_DATA)psCBData->pvFnData;
	psState = NULL;
	ASSERT(psBundle != NULL);

	psRegState = psBundle->psRegState;
	psRAData = &(psRegState->sRAData);
	psState = psRAData->psState;

	psInst = psCBData->psInst;
    bDoubleIndex = IMG_FALSE;

    uInstIdx = psBundle->uInstIdx;

	/*
	 *  Process callback
	 */

	/*
	   If this is the start of a code block, make sure that all
	   registers live on exit have a liveness interval with an open
	   start.
	*/
	if (psCBData->bIsBlock)
	{
		LiveIntervalBlock(psCBData);
		return;
	}

	/* Process an instruction */
	ASSERT(!psCBData->bIsBlock);

	/* Instruction entry */
	if (psCBData->ePos == USC_LCB_ENTRY)
	{
		/* Update instruction counter */
		psBundle->uInstIdx += 1;

		/* Set write mask */
		if (psInst != NULL)
			GetDestMask(psInst, &(psBundle->uInstWriteMask));
		else
			psBundle->uInstWriteMask = 0;

		/* Record iregister use */
		psBundle->uIRegUse |= GetIRegsReadMask(psState, psInst, IMG_FALSE, IMG_FALSE);
		psBundle->uIRegUse |= GetIRegsWrittenMask(psState, psInst, IMG_FALSE);
	}
	if (psCBData->ePos != USC_LCB_PROC)
	{
		/* Ignore end-of-instruction callbacks */
		return;
	}

	/* Reset the instruction index */
	uInstIdx = psBundle->uInstIdx;
	bDoubleIndex = IMG_FALSE;

	if (psInst != NULL)
	{
		psInst->uId = uInstIdx;
#ifdef DEBUG
		psRegState->uMaxInstIdx = max(psRegState->uMaxInstIdx, psInst->uId);
#endif

		/*
		   DSX, DSY instruction need to have destination and source
		   operands in different registers. To do this, their
		   destination and source are given overlapping intervals: the
		   destination is always treated as written one instruction
		   earlier than actual DSX/DSY instruction.

		   This means that DSX/DSY instructions are given two indices
		   and the destination is written to the earlier.
		*/
		if (psInst->eOpcode == IFDSX ||
			psInst->eOpcode == IFDSY)
		{
			bDoubleIndex = IMG_TRUE;
		}
	}

	/* Work out which register is being read or written */
	InitInstArg(&sArg);

	uArgNum = (IMG_UINT32)psCBData->pvRegData;
	bHasArgNum = IMG_TRUE;

	switch (psCBData->eRegType)
	{
		case USC_LCB_DEST:
		{
			ASSERT(psInst != NULL);
			sArg = psInst->asDest[uArgNum];
			break;
		}
		case USC_LCB_SRC:
		{
			ASSERT(psInst != NULL);
			sArg = psInst->asArg[uArgNum];
			break;
		}
		case USC_LCB_PRED:
		{
			sArg.uType = USEASM_REGTYPE_PREDICATE;
			sArg.uNumber = uArgNum;
			break;
		}
		case USC_LCB_IREG:
		{
			sArg.uType = USEASM_REGTYPE_FPINTERNAL;
			sArg.uNumber = uArgNum;
			break;
		}
		case USC_LCB_INDEX:
		{
			sArg.uType = psInst->asArg[uArgNum].uIndexType;
			sArg.uNumber = psInst->asArg[uArgNum].uIndexNumber;
			break;
		}
		case USC_LCB_EFOI0:
		case USC_LCB_EFOI1:
		{
			sArg = *((PARG)psCBData->pvRegData);
			bHasArgNum = IMG_FALSE;
			break;
		}
		case USC_LCB_TYPE_UNDEF:
		{
			bHasArgNum = IMG_FALSE;
			break;
		}
	}

	/* Skip registers which aren't subject to allocation */
	if (!RegIsNode(psRAData, &sArg))
	{
		return;
	}


	/* Record an iregister used to store c10 data */
	if (sArg.uType == USEASM_REGTYPE_FPINTERNAL)
	{
		if (sArg.eFmt == UF_REGFORMAT_C10)
		{
			psRegState->uIRegC10Mask |= (1U << sArg.uNumber);
		}
	}

	/* Process register */
	if (psCBData->bDest)
	{
		/* Destinations */
		ProcIntervalDst(psRegState, psBundle,
						uInstIdx, psInst,
						&sArg,
						bDoubleIndex);
	}
	else
	{
		/* Sources */
		IMG_UINT32 uReadMask = USC_DESTMASK_FULL; // Channels read

		if (bHasArgNum)
		{
			uReadMask = CalcInstSrcChans(psState, psInst, uArgNum);
		}

		ProcIntervalSrc(psRegState, psBundle,
						uInstIdx, psInst,
						&sArg, uReadMask,
						bDoubleIndex);
	}
	psCBData->pvFnData = (IMG_PVOID)psBundle;
}

static
PUSC_REG_INTERVAL ProcOpenIntervals(PRALSCAN_STATE psRegState,
                                    PUSC_CALLBACK_DATA psBundle)
/*****************************************************************************
 FUNCTION	: ProcOpenIntervals

 PURPOSE	: Process the list of intervals which aren't explicitly written

 PARAMETERS	: psRegState       - Register allocator state
              psBundle         - Call back data

 RETURNS	: Nothing.
*****************************************************************************/
{
	PREGALLOC_DATA psRAData;
	PINTERMEDIATE_STATE psState;
	PUSC_REG_INTERVAL psCurr, psNext;
	PUSC_REG_INTERVAL psOpenIntervals;

	psRAData = &(psRegState->sRAData);
	psState = psRAData->psState;
	psOpenIntervals = ProcListFindStart(psBundle->psCurrent);

	psNext = NULL;
	for (psCurr = psOpenIntervals;
		 psCurr != NULL;
		 psCurr = psNext)
	{
		IMG_UINT32 uNode = psCurr->uNode;

        psNext = psCurr->psProcNext;
		psOpenIntervals = ProcListRemove(psCurr, psOpenIntervals);
		SetBit(psCurr->auFlags, INTVL_PENDING, IMG_FALSE);

		/* Eliminate nodes that are never read and never written */
		if (!GetBit(psCurr->auFlags, INTVL_WRITTEN) &&
			!GetBit(psCurr->auFlags, INTVL_CONDWRITTEN) &&
			!GetBit(psCurr->auFlags, INTVL_READ) &&
			!GetBit(psRegState->auIsOutput, uNode))
		{
			PUSC_REG_INTERVAL psList = psRegState->apsRegInterval[uNode];

			psRegState->apsRegInterval[uNode] = RegIntvlDrop(psCurr, psList);
			DeleteRegInterval(psState, psCurr);

			continue;
		}

        /* A node that was read but never unconditionally written is given
         * an open interval (in some circumstances).
		 */
        if ((!GetBit(psCurr->auFlags, INTVL_WRITTEN)) ||
			((psCurr->uWriteMask & USC_DESTMASK_FULL) != USC_DESTMASK_FULL))
        {
			if (GetBit(psCurr->auFlags, INTVL_FUNCTION) &&
				GetBit(psCurr->auFlags, INTVL_READ))
			{
				/*
				  An interval that is read in a function
				  and is open at the start of the function
				  may be a parameter to the function.
				  Make it global.
				*/
				SetBit(psCurr->auFlags, INTVL_PARAM, IMG_TRUE);
				MergeIntervalList(psRegState, psCurr);

				/* Extend start-point to include all possibe call sites */
			}

			/* An interval which has a channel live is given an open start */
			if ((!GetBit(psCurr->auFlags, INTVL_FUNCTION)) &&
				psCurr->uNode < psRAData->uAvailPARegs &&
				(psCurr->uReadMask & USC_DESTMASK_FULL) != 0)
			{
				psCurr->uStart = USC_OPEN_START;
			}

			/* Primary attributes */
			if (uNode < psRAData->uAvailPARegs)
			{
				if (GetBit(psCurr->auFlags, INTVL_READ) ||
					GetBit(psRegState->auIsOutput, uNode) ||
					GetBit(psCurr->auFlags, INTVL_CONDWRITTEN))
				{
					psCurr->uStart = USC_OPEN_START;
				}
			}
			/* Temporary registers */
			else if (uNode >= psRAData->uAvailPARegs)
			{
				if (GetBit(psRegState->auIsOutput, uNode))
				{
					psCurr->uStart = USC_OPEN_START;
				}
			}
        }

		if (GetBit(psRegState->auIsFixed, uNode))
		{
			/* Fixed colours */
			SetBit(psCurr->auFlags, INTVL_WRITTEN, IMG_TRUE);
			if (!ProcListIntvlIn(psRegState, psCurr))
			{
				psRegState->psFixed = ProcListAdd(psCurr, psRegState->psFixed);
			}
		}
		else if (GetBit(psCurr->auFlags, INTVL_CONDWRITTEN))
		{
			ASSERT(!GetBit(psCurr->auFlags, INTVL_WRITTEN) ||
				   GetBit(psCurr->auFlags, INTVL_PARAM));
			if (!ProcListIntvlIn(psRegState, psCurr))
			{
				psRegState->psIntervals = ProcListAdd(psCurr, psRegState->psIntervals);
			}
		}
		else
		{
			/* Not temporary, not fixed */
			psRegState->psIntervals = ProcListAdd(psCurr, psRegState->psIntervals);
		}

	}
	return psOpenIntervals;
}

static
IMG_VOID CalcIntervals(PINTERMEDIATE_STATE psState,
					   PRALSCAN_STATE psRegState)
/*****************************************************************************
 FUNCTION	: CalcIntervals

 PURPOSE	: Calculate liveness intervals

 PARAMETERS	: psState          - Compiler state
              psRegState       - Register allocator state

 RETURNS	: Nothing.
*****************************************************************************/
{
	USC_LIVE_CALLBACK sCallback;
	PUSC_CALLBACK_DATA psBundle;
	IMG_UINT32 uCtr;
	IMG_BOOL bSplitFeedBack;
	IMG_BOOL bSplit;
	PUSC_FUNCTION psFunc;

	psRegState->uIRegC10Mask = 0;
	InitCallbackData(psRegState, LiveIntervalCB, &sCallback);
	psBundle = (PUSC_CALLBACK_DATA)sCallback.pvFnData;

	/* Test for split-feedback */
	if (psState->uFlags & USC_FLAGS_SPLITFEEDBACKCALC)
        bSplitFeedBack = IMG_TRUE;
    else
        bSplitFeedBack = IMG_FALSE;

	/* Test for split */
	if (psState->uFlags2 & USC_FLAGS_SPLITCALC)
        bSplit = IMG_TRUE;
    else
        bSplit = IMG_FALSE;

	/*
	 * Calculate liveness intervals
	 */
    /* Functions */
	for (psFunc = psState->psFnInnermost; psFunc; psFunc = psFunc->psFnNestOuter)
    {
		if (psFunc == psState->psMainProg || psFunc == psState->psSecAttrProg)
			psBundle->bInFunction = IMG_FALSE;
		else
			psBundle->bInFunction = IMG_TRUE;

        RegUseBlock(psState, &sCallback, psFunc);

		/*
		 * Process intervals left open by the functions. Any register used
		 * but not written is assumed to be an input to the function and
		 * given an open start point.
		 */
		psBundle->uInstIdx += 1;
		psBundle->bInFunction = IMG_TRUE;
		psBundle->psCurrent = ProcOpenIntervals(psRegState, psBundle);
		psBundle->bInFunction = IMG_FALSE;
		psBundle->uInstIdx += 1;
		ASSERT(psBundle->psCurrent == NULL);
	}

    /* whereas functions (above) straightforwardly RegUseBlock,
	   in old code, the main program was handled differently;
	   for reference, it used to be like this:
	*/
	//for (psCurrBlock = psState->sMainFunc.psTail;
	//	 psCurrBlock != NULL;
	//	 psCurrBlock = psCurrBlock->psPrev)
	//{
	//	RegUseBlock(psState, &sCallback, psCurrBlock);
	//}

	/* Store the number of internal registers used */
	psRegState->uNumIRegsUsed = 0;
	for (uCtr = 0; uCtr < NUM_INTERNAL_REGISTERS; uCtr++)
	{
		if (psBundle->uIRegUse & (1U << uCtr))
		{
			psRegState->uNumIRegsUsed += 1;
		}
	}
	psRegState->uProgIRegsLive = psBundle->uIRegUse;

	/*
	  If split feed back is used, then temp. reg. intervals can't
	  overlap the split feed back point.
	*/
	if (bSplitFeedBack &&
		(!(psState->psTargetFeatures->ui32Flags &
		   SGX_FEATURE_FLAGS_USE_UNIFIED_TEMPS_AND_PAS)))
	{
		psRegState->uSplitFeedBackIdx = psBundle->uSplitFeedBackIdx;
	}
	else
	{
		psRegState->uSplitFeedBackIdx = USC_OPEN_END;
	}

	if	(bSplit &&
		(!(psState->psTargetFeatures->ui32Flags &
		   SGX_FEATURE_FLAGS_USE_UNIFIED_TEMPS_AND_PAS)))
	{
		psRegState->uSplitIdx = psBundle->uSplitIdx;
	}
	else
	{
		psRegState->uSplitIdx = USC_OPEN_END;
	}

	ClearCallbackData(psState, &sCallback);
}

static
IMG_VOID PrepRegStateJITBlock(PINTERMEDIATE_STATE psState,
							  PCODEBLOCK psBlock,
							  IMG_PVOID pvRegState)
/*****************************************************************************
 FUNCTION	: PrepRegStateJITBlock

 PURPOSE	: Do per-block alloation data initialisation

 PARAMETERS	: psState		 - Compiler state.
              psBlock        - Block to process
              pvRegState     - Register state

 RETURNS	: Nothing.

 NOTES      : Forms mandatory register groups.
              Sets pvRegState->bRestartRegState if allocation
              must be restarted.
*****************************************************************************/
{
	PRALSCAN_STATE psRegState = (PRALSCAN_STATE)pvRegState;

	/* Check for blocks that are never processed */
	if (psBlock->psOwner == psState->psSecAttrProg ||
		psRegState->bRestartRegState)
	{
		return;
	}
	/* Mandatory groups for registers used in specific instructions */
	RegGroupingBlock(psState, psRegState, psBlock,
					 psRegState->uIteration,
					 &psRegState->bRestartRegState);
}

static
IMG_VOID PrepRegStateJIT(PINTERMEDIATE_STATE psState,
						 PRALSCAN_STATE psRegState,
						 FUNCGROUP eFuncGroup,
						 IMG_PBOOL pbRestart)
/*****************************************************************************
 FUNCTION	: PrepRegStateJIT

 PURPOSE	: Prepare the register state for just-in-time register allocation

 PARAMETERS	: psState		 - Compiler state.
              psRegState     - Allocator state
              eFuncGroup     - Function group

 OUTPUT     : pbRestart      - Whether to restore the allocation

 RETURNS	: Nothing.

 NOTES      : Forms mandatory register groups.
*****************************************************************************/
{
	/* Check that this function hasn't been called more than twice */
	ASSERT(psRegState->uIteration < 3);

	/*
	 * Form mandatory register groups
	 */
	/* Mandatory groups for register arrays */
	if (eFuncGroup == FUNCGROUP_MAIN)
	{
		GroupArrayRegs(psState, &psRegState->sRAData);
	}

	/* Mandatory groups for registers used in specific instructions */
	psRegState->bRestartRegState = IMG_FALSE;
	DoOnAllBasicBlocks(psState, ANY_ORDER,
					   PrepRegStateJITBlock,
					   IMG_FALSE,
					   psRegState);

	*pbRestart = psRegState->bRestartRegState;
}

static
IMG_BOOL IsClobbered(PRALSCAN_STATE psRegState,
					 PUSC_REG_INTERVAL psInterval)
/*****************************************************************************
 FUNCTION	  : IsClobbered

 PURPOSE	  : Test whether an interval is clobbered by split-feedback

 PARAMETERS	  : psRegState        - Register allocator state
                psInterval        - Liveness interval specifying start node
                                    and size of register group.

 RETURNS	  : IMG_TRUE if temporary register are clobbered during the interval.
*****************************************************************************/
{
	if (psRegState->uSplitFeedBackIdx == USC_OPEN_END)
	{
		return IMG_FALSE;
	}
	if (psInterval == NULL)
	{
		return IMG_FALSE;
	}

	if (EarlierThan(psInterval->uStart, psRegState->uSplitFeedBackIdx) &&
		!EarlierThan(psInterval->uEnd, psRegState->uSplitFeedBackIdx))
	{
		return IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
}

static
IMG_BOOL TakeColour(PRALSCAN_STATE psRegState,
					IMG_BOOL bCanRetire,
					PUSC_REG_INTERVAL psInterval,
					IMG_UINT32 uColour)
/*****************************************************************************
 FUNCTION	  : TakeColour

 PURPOSE	  : Try to take a colour or an interval, retiring the
                existing interval if necessary.

 PARAMETERS	  : psRegState        - Register allocator state
                bCanRetire        - Whether intervals can be retired.
                psInterval        - Liveness interval specifying start node
                                    and size of register group.
                uColour           - Colour to take

 RETURNS	  : IMG_TRUE if colour can be used for the interval, IMG_FALSE otherwise.
*****************************************************************************/
{
	PUSC_REG_INTERVAL psCurr, psNext, psList;

	psList = ProcListFindPos(psRegState->apsColour[uColour],
	                         psInterval->uStart, psInterval->uEnd);
	if (psList != NULL)
	{
	    psRegState->apsColour[uColour] = psList;
	}
	else
	{
        /* Interval is earlier than any in the list */
	    psRegState->apsColour[uColour] = ProcListFindStart(psRegState->apsColour[uColour]);
	}

	/* Colour has an interval, test for retiring */
	psNext = NULL;
    for (psCurr = psRegState->apsColour[uColour];
		 psCurr != NULL;
		 psCurr = psNext)
    {
        psNext = psCurr->psProcNext;

        if (bCanRetire &&
            (EarlierThan(psCurr->uEnd, psInterval->uStart) ||
			 psCurr->uEnd == psInterval->uStart))
        {
            /* Interval ends before current interval starts. */
			continue;
        }
        else
            if (!IntervalOverlap(psCurr->uStart, psCurr->uEnd,
								 psInterval->uStart, psInterval->uEnd))
			{
				/* Intervals don't overlap */
				return IMG_TRUE;
			}
			else
			{
				/* Colour can't be used */
				return IMG_FALSE;
			}
    }

	return IMG_TRUE;
}

static
IMG_BOOL FindClassRange(PRALSCAN_STATE psRegState,
						IMG_UINT32 uClassBase,
						IMG_UINT32 uClassSize,
						PUSC_REG_INTERVAL psInterval,
						IMG_UINT32 uStart,
						IMG_PUINT32 puColour)
/*****************************************************************************
 FUNCTION	  : FindClassRange

 PURPOSE	  : Search a register class for a range of consecutive colours
                large enough to colour all registers in a group.

 PARAMETERS	  : psRegState        - Register allocator state
                uClassBase        - First node in the register class
                uClassSize        - Number of nodes in the register class
                psInterval        - Liveness interval specifying start node
                                    and size of register group.
                uStart            - Node in class to start looking from

 OUTPUT       : puColour          - First colour of range (if any available).

 NOTES        : uStart is a node in the class,
                uClassBase <= uStart <= (uClassBase + uClassSize)

 RETURNS	  : IMG_TRUE if group could be coloured, IMG_FALSE otherwise.
*****************************************************************************/
{
	IMG_UINT32 uNext;
	IMG_UINT32 uCtr;
	IMG_UINT32 uOffset = 0;
	IMG_UINT32 uColour;
	IMG_UINT32 uNumRegs;
	IMG_BOOL bFound;

	bFound = IMG_FALSE;
	uColour = USC_UNDEF;

	uOffset = uStart - uClassBase;
	if (psInterval->uCount > uClassSize)
	{
		return IMG_FALSE;
	}
	else
	{
		uNumRegs = (uClassSize - psInterval->uCount) + 1;
	}

	uNext = uClassSize;
	for (uCtr = 0; uCtr < uNumRegs; uCtr = uNext)
	{
		IMG_UINT32 uIdx;
		uColour = ((uOffset + uCtr) % uClassSize) + uClassBase;
		uNext = uCtr + 1;

		/* Test the colour usage */
		bFound = IMG_FALSE;
		for (uIdx = 0;
			 uIdx < psInterval->uCount;
			 uIdx ++)
		{
            if (TakeColour(psRegState, IMG_TRUE, psInterval, uColour + uIdx))
			{
				bFound = IMG_TRUE;
			}
			else
			{
				bFound = IMG_FALSE;
				uNext += uIdx;
				break;
			}
		}
		if (bFound)
		{
			goto Exit;
		}
	}

  Exit:
	if (bFound)
	{
		if (puColour != NULL)
			(*puColour) = uColour;
	}

	return bFound;
}

static
IMG_BOOL FindColourRange(PRALSCAN_STATE psRegState,
						 PUSC_REG_INTERVAL psInterval,
						 IMG_PUINT32 puColour)
/*****************************************************************************
 FUNCTION	  : FindColourRange

 PURPOSE	  : Find a range of consecutive colours large enough to
                colour all registers in a group.

 PARAMETERS	  : psRegState        - Register allocator state
                psInterval        - Livenessinterval specifying start node
                                    and size of register group.

 OUTPUT       : puColour          - First colour of range (if any available).

 RETURNS	  : IMG_TRUE if group could be coloured, IMG_FALSE otherwise.
*****************************************************************************/
{
	PINTERMEDIATE_STATE psState = psRegState->sRAData.psState;
	IMG_UINT32 uColour, uNext;
	IMG_UINT32 uLastReg;
	IMG_BOOL bFound = IMG_FALSE;
	IMG_UINT32 uRet = USC_UNDEF_COLOUR;
	IMG_UINT32 uNode;
	IMG_UINT32 uAvailPARegs;
	IMG_BOOL bPASearched;

	/*
	 * Find a large enough group of registers
	 */

	if (psInterval->uCount > psRegState->uNumHwRegs)
	{
	    *puColour = USC_UNDEF_COLOUR;
	    return IMG_FALSE;
	}

	uLastReg = (psRegState->uNumHwRegs - psInterval->uCount);
	uNext = uLastReg;
	uNode = psInterval->uNode;

	/* Test for a fixed colour */
	if (GetBit(psRegState->auIsFixed, uNode))
	{
		uRet = psRegState->auFixedColour[uNode];
		bFound = IMG_TRUE;
		goto Exit;
	}

	/* Test for a fixed colour */
	if (GetBit(psRegState->auIsFixed, uNode))
	{
		GetTieColour(psRegState, psInterval, &uRet);

		ASSERT(uRet == psRegState->auFixedColour[uNode]);
		bFound = IMG_TRUE;
		goto Exit;
	}

	/* If the interval is tied, try the tied colour first */
	if (GetTieColour(psRegState, psInterval, &uColour))
	{
		/* Got a colour */
        IMG_UINT32 uTmp;

        uTmp = uColour;
		/* skip */
	}
	/* Test for a dummy register */
	else  if (psInterval->uStart == psInterval->uEnd)
	{
		uColour = psRegState->sRAData.uAvailPARegs;
	}
	else
	{
        /* Set the first colour to try */
		uColour = psRegState->uRegAllocStart;
	}

	/* Set the available PA registers */
	uAvailPARegs = psRegState->sRAData.uAvailPARegs;
	if (uAvailPARegs >= psState->psSAOffsets->uExtraPARegisters)
		uAvailPARegs -= psState->psSAOffsets->uExtraPARegisters;
	else
		uAvailPARegs = 0;

	/* Search the primary attributes */
	bPASearched = IMG_FALSE;
	if (uColour < (uAvailPARegs - psInterval->uCount))
	{
		bFound = FindClassRange(psRegState, 0, uAvailPARegs,
								psInterval, uColour, &uRet);
		if (bFound)
		{
			goto Exit;
		}
		bPASearched = IMG_TRUE;
	}

	/* Test for the temporary registers being clobbered */
	if (IsClobbered(psRegState, psInterval))
	{
		/* Can't use a tempory that has been clobbered by split feed back */

		/* Try to turn off split feed back */
		if (psState->uFlags & USC_FLAGS_SPLITFEEDBACKCALC)
		{
			psState->uFlags &= ~USC_FLAGS_SPLITFEEDBACKCALC;
			psRegState->uSplitFeedBackIdx = USC_OPEN_END;
		}
		/* Test again */
		if (IsClobbered(psRegState, psInterval))
		{
			return IMG_FALSE;
		}
	}

	/* Search the temporary registers */
	bFound = FindClassRange(psRegState,
							psRegState->sRAData.uAvailPARegs,
							psRegState->uNumHwRegs - psRegState->sRAData.uAvailPARegs,
							psInterval,
							max(uColour, psRegState->sRAData.uAvailPARegs),
							&uRet);
	if (bFound)
	{
		goto Exit;
	}


	/* Search the PA Registers if the initial prefered colour was a temp */
	if (!bPASearched)
	{
		bFound = FindClassRange(psRegState, 0, uAvailPARegs,
								psInterval, uColour, &uRet);
		if (bFound)
		{
			goto Exit;
		}
		bPASearched = IMG_TRUE;
	}

	/* Got here so no colour found */
	return IMG_FALSE;

  Exit:
	ASSERT(uRet < psRegState->sRAData.uAvailRegs);
	*puColour = uRet;
	return IMG_TRUE;
}

static
IMG_BOOL SpillLessThan(PUSC_REG_INTERVAL psInterval1,
                       PUSC_REG_INTERVAL psInterval2)
/*****************************************************************************
 FUNCTION     : SpillLessThan

 PURPOSE      : Order intervals for best-to-spill

 PARAMETERS   : psInterval1, psInterval2 - Intervals to compare

 RETURNS      : IMG_TRUE If psInterval1 is a worse choice than psInterval2
                for spilling (IMG_TRUE implies psInterval2 should be spilled).
****************************************************************************/
{
	IMG_FLOAT fActivity1, fActivity2;

	fActivity1 = NodeActivity(psInterval1, psInterval1->uStart, psInterval1->uEnd);
	fActivity2 = NodeActivity(psInterval2, psInterval2->uStart, psInterval2->uEnd);

	if (fActivity1 < fActivity2)
	{
		return IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
}

static
IMG_BOOL SpillChoice(PRALSCAN_STATE psRegState,
                     IMG_UINT32 uBest,
					 IMG_UINT32 uStart,
					 IMG_UINT32 uEnd,
					 IMG_UINT32 uColour,
					 IMG_PUINT32 puCount)
/*****************************************************************************
 FUNCTION     : SpillChoice

 PURPOSE      : Choose whether to spill the current interval or the
                interval for the colour.

 PARAMETERS   : psRegState    - Register allocator state
                psInteraval   - Current interval being spilled
                uColour       - Colour with candidate to spill

 OUTPUT       : ppsSpill      - Interval to spill
                puCount       - Number of colours in the register group

 RETURNS      : IMG_TRUE If psInterval should be spilled,
                IMG_FALSE if colour interval should be spilled
****************************************************************************/
{
    PUSC_REG_INTERVAL psCurr = NULL;
	PUSC_REG_INTERVAL psBest = NULL;
	IMG_BOOL bRet = IMG_TRUE;
	IMG_BOOL bNewCurr = IMG_FALSE;

    psBest = RegIntvlFindPos(psRegState->apsColour[uBest],
							 uStart, uStart);
	if(psBest == NULL ||
	   EarlierThan(psBest->uEnd, uStart))
	{
		psRegState->apsColour[uBest] = RegListFindStart(psRegState->apsColour[uBest]);
		psBest = NULL;
	}

	psCurr = RegIntvlFindPos(psRegState->apsColour[uColour],
							 uStart, uStart);
	if (psCurr == NULL ||
		EarlierThan(psCurr->uEnd, uStart))
	{
		psRegState->apsColour[uColour] = RegListFindStart(psRegState->apsColour[uColour]);
		psCurr = NULL;
	}

    if (psCurr == NULL)
    {
		if (psBest != NULL)
		{
			/* Use the new colour */
			bRet = IMG_FALSE;
			goto Exit;
		}
		else if (psBest == NULL)
		{
			bRet = IMG_TRUE;
			goto Exit;
		}
		else
		{
			PUSC_REG_INTERVAL psTmp;

			psCurr = (PUSC_REG_INTERVAL)UscAlloc(psRegState->sRAData.psState,
												 sizeof(USC_REG_INTERVAL));
			bNewCurr = IMG_TRUE;

			psCurr->uStart = uStart;

			psTmp = psRegState->apsColour[uColour];
			if (psTmp != NULL)
			{
				psCurr->uAccessCtr = psTmp->uAccessCtr;
				psTmp->uEnd = uEnd;
			}
			else
			{
				psCurr->uAccessCtr = psBest->uAccessCtr;
				psCurr->uEnd = uEnd;
			}
			psBest = psRegState->apsColour[uBest];
		}
	}
	*puCount = psCurr->uCount;

	if (psCurr->uNode < psRegState->sRAData.uAvailPARegs ||
		GetBit(psRegState->auIsOutput, psCurr->uNode))
	{
        /* found interval is a PA or an output register */
		bRet = IMG_TRUE;
		goto Exit;
	}

    if (IsNodeInGroup(&psRegState->sRAData, psCurr->uNode))
	{
		bRet = IMG_FALSE;
		goto Exit;
	}

	if ((psCurr->uBaseNode != USC_UNDEF_COLOUR &&
		 psCurr->uNode != psCurr->uBaseNode) ||
        (psCurr->bSpill))
    {
        /* No interval found. */
		bRet = IMG_TRUE;
		goto Exit;
    }

    if (SpillLessThan(psBest, psCurr))
    {
		bRet = IMG_FALSE;
		goto Exit;
    }

  Exit:
	if (bNewCurr)
	{
		DeleteRegInterval(psRegState->sRAData.psState, psCurr);
	}

    return bRet;
}

static
IMG_BOOL FindSpillRange(PRALSCAN_STATE psRegState,
                        PUSC_REG_INTERVAL psInterval,
                        PUSC_REG_INTERVAL *ppsSpill)
/*****************************************************************************
 FUNCTION     : FindSpillRange

 PURPOSE      : Find a range of consecutive colours to spill for an interval

 PARAMETERS   : psRegState        - Register allocator state
                psInterval        - Liveness interval specifying start node
                                    and size of register group.

 OUTPUT       : ppsSpil           - Interval to spill

 NOTES        : If the interval is coloured, the interval it displaces will be marked
                as spilled.

  RETURNS      : IMG_TRUE iff the interval could be coloured,
                 IMG_FALSE if it will be spilled.
*****************************************************************************/
{
    PINTERMEDIATE_STATE psState = psRegState->sRAData.psState;
    const IMG_UINT32 uRegCount = ((psRegState->sRAData.uAvailRegs - psInterval->uCount) +
								  1 +
								  psRegState->uRegAllocStart);
    PUSC_REG_INTERVAL psBestSpill;    // Best interval to spill
	IMG_UINT32 uBestSpill;
    IMG_UINT32 uColour;
    IMG_UINT32 uNext;
	IMG_UINT32 uStart, uEnd;

    /* Run through the colours trying to find an interval that would
     * be better to spill than the current one.
     * If found, spill it and give its colour to the new interval.
     */

    /* Set the first colour to try */
    uColour = psRegState->uRegAllocStart;

    /* Start with the given interval as the interval to spill */
	psBestSpill = psInterval;

    /* Test for the temporary registers being clobbered */
    if (IsClobbered(psRegState, psInterval))
    {
        /* Can't use a tempory that has been clobbered */
        return IMG_FALSE;
    }

    /* Run through the colours, testing the existing intervals */
	uStart = USC_OPEN_START;
	uEnd = USC_OPEN_END;
    uNext = 1;
	uBestSpill = USC_UNDEF;
    for (uColour = psRegState->sRAData.uAvailPARegs;
		 uColour < uRegCount;
		 uColour += uNext)
    {
        uNext = 1;

        if (!SpillChoice(psRegState,
						 uBestSpill,
						 uStart, uEnd,
						 uColour,
						 &uNext))
        {
			uBestSpill = uColour;
        }
    }

	if (uBestSpill < psRegState->sRAData.uAvailRegs)
	{
		RegIntvlAdvance(psRegState->apsColour[uBestSpill],
						uStart, uStart,
						&psBestSpill);
	}
	else
	{
		psBestSpill = NULL;
	}

	/* If the best spill node is already spilled or a PA register then fail */
	if (psBestSpill != NULL &&
		psBestSpill->uNode < psRegState->sRAData.uAvailPARegs)
	{
		psBestSpill = NULL;
	}

    /* Set the return values */
    *ppsSpill = psBestSpill;

    if (psBestSpill == NULL ||
		psBestSpill == psInterval)
    {
        return IMG_FALSE;
    }

    /* If the spilled interval had a colour, drop it from the colour list */
    {
        ASSERT (psBestSpill->uColour < psRegState->sRAData.uAvailRegs);

        psBestSpill->uColour = USC_UNDEF_COLOUR;
        CutTieInterval(psBestSpill);
    }

	return IMG_TRUE;
}

static
IMG_VOID SpillGroup(PRALSCAN_STATE psRegState,
                    IMG_UINT32 uBaseNode,
                    IMG_UINT32 uStart,
                    IMG_UINT32 uEnd)
/*****************************************************************************
 FUNCTION     : SpillGroup

 PURPOSE      : Spill a group in an interval

 PARAMETERS   : psRegState        - Register allocator state
                uBaseNode         - BaseNode of the group
                uStart, uEnd      - End-points of the interval to spill at.

  RETURNS     : Nothing.
*****************************************************************************/
{
    PREGISTER_GROUP psNodeGroup;
    PUSC_REG_INTERVAL psNodeInterval;

    for (psNodeGroup = &psRegState->sRAData.asRegGroup[uBaseNode];
         psNodeGroup != NULL;
         psNodeGroup = psNodeGroup->psNext)
    {
        /* Get the node to colour */
        IMG_UINT32 uGroupNode, uColour;

        uGroupNode = psNodeGroup - &(psRegState->sRAData.asRegGroup[0]);

        /* Advance to the first interval */
        RegIntvlAdvance(psRegState->apsRegInterval[uGroupNode],
                        uStart, uEnd, &psNodeInterval);

        /* Spill the group node */
        uColour = psNodeInterval->uColour;

        if (!psNodeInterval->bSpill)
        {
            psNodeInterval->bSpill = IMG_TRUE;
        }
    }
}

static
IMG_VOID GetGroupEndPoints(PRALSCAN_STATE psRegState,
                           IMG_UINT32 uNode,
                           IMG_BOOL bOptionalGroup,
                           IMG_PUINT32 puStart,
                           IMG_PUINT32 puEnd)
/*****************************************************************************
 FUNCTION     : GetGroupEndPoints

 PURPOSE      : Update start and end points needed to accommadate
                an interval of a group node.

 PARAMETERS   : psRegState    - Allocator state
                uNode         - Group node to test
                bOptionalGroup- Working on an optional group

 INPUT/OUTPUT : puStart       - Start point
                puEnd         - End point

 RETURNS	: Nothing.
*****************************************************************************/
{
    IMG_UINT32 uStart = *puStart;
    IMG_UINT32 uEnd = *puEnd;
    PUSC_REG_INTERVAL psInterval;

    RegIntvlAdvance(psRegState->apsRegInterval[uNode], uStart, uEnd, &psInterval);
    if (psInterval == NULL)
        return;

    if (bOptionalGroup ||
        IntervalOverlap(psInterval->uStart, psInterval->uEnd,
                        uStart, uEnd))
    {
        if (EarlierThan(psInterval->uStart, uStart))
        {
            uStart= psInterval->uStart;
		}
        if (EarlierThan(uEnd, psInterval->uEnd))
        {
            uEnd = psInterval->uEnd;
            if (psInterval->psRegNext != NULL)
			{
                if (EarlierThan(psInterval->psRegNext->uStart, psInterval->uEnd))
				{
                    psInterval->uEnd = psInterval->psRegNext->uStart;
                }
            }
        }

        *puStart = uStart;
        *puEnd = uEnd;
	}
}

static
PUSC_REG_INTERVAL GetBaseInterval(PRALSCAN_STATE psRegState,
                                  PUSC_REG_INTERVAL psCurrInterval,
                                  IMG_UINT32 auMinInterval[],
                                  IMG_PUINT32 puMinGroupSize,
                                  IMG_PUINT32 puLastReqNode)
/*****************************************************************************
 FUNCTION     : GetBaseInterval

 PURPOSE      : Get the interval for the base node of a register group.

 PARAMETERS   : psRegState       - Allocator state
                psCurrInterval   - Current liveness interval

 OUTPUT       : auMinInterval    - Start and end points for the mandatory group only
                puMinGroupSize   - Size of the mandatory register group
                puLastReqNode    - Last member of mandatory register group

 NOTES        : puMinInterval and puFullInterval must be arrays of size 2.

 RETURNS      : Interval for base of register group.
*****************************************************************************/
{
    IMG_UINT32 uMinGroupSize;          // Number of nodes in the mandatory group
    IMG_UINT32 uLastReqNode;
    IMG_UINT32 uNode;
    PUSC_REG_INTERVAL psTmp = NULL;
	IMG_UINT32 auReqBound[2] = {0, 0}; // Bounds for required group node

    uNode = psCurrInterval->uNode;

    auReqBound[0] = psCurrInterval->uStart;
    auReqBound[1] = psCurrInterval->uEnd;

    uMinGroupSize = 1;
    uLastReqNode = uNode;

    if (!IsNodeInGroup(&psRegState->sRAData, uNode))
    {
        goto Exit;
	}

    /* Node is in a group, find the base of the group */
    if (psCurrInterval->uBaseNode != USC_UNDEF_COLOUR)
    {
        /* Already have the base node */
        RegIntvlAdvance(psRegState->apsRegInterval[psCurrInterval->uBaseNode],
                        psCurrInterval->uStart, psCurrInterval->uEnd,
                        &psTmp);
        if (psTmp != NULL)
        {
            psCurrInterval = psRegState->apsRegInterval[psCurrInterval->uBaseNode];
        }
        auReqBound[0] = psCurrInterval->uStart;
        auReqBound[1] = psCurrInterval->uEnd;
    }
    else if (IsNodeInGroup(&psRegState->sRAData, uNode))
    {
        REGISTER_GROUP *psGroup, *psStart, *psBase;
        IMG_UINT32 uGroupNode = 0;
        IMG_UINT32 uBaseNode;      // Node at start of group
        IMG_UINT32 uLength;
        IMG_BOOL bFindLastReqNode;

        uLength = 1;
        bFindLastReqNode = IMG_TRUE;

        psStart = &psRegState->sRAData.asRegGroup[uNode];
        psBase = psStart;

        for (psGroup = psStart;
             psGroup != NULL;
             psGroup = psGroup->psPrev, uLength++)
        {
            psBase = psGroup;
            uGroupNode = psGroup - psRegState->sRAData.asRegGroup;

			uMinGroupSize += 1;
			if (bFindLastReqNode)
			{
				uLastReqNode = uGroupNode;
				bFindLastReqNode = IMG_FALSE;
			}

            /* Record maximum and minimum of current group interval */
            GetGroupEndPoints(psRegState, uGroupNode, IMG_FALSE,
                              &auReqBound[0],
                              &auReqBound[1]);
        }

        /* Store node starting the group */
        uBaseNode = psBase - psRegState->sRAData.asRegGroup;
        GetGroupEndPoints(psRegState, uGroupNode, IMG_FALSE,
                          &auReqBound[0], &auReqBound[1]);
		/* first node is always required */
		uLastReqNode = uBaseNode;
		bFindLastReqNode = IMG_FALSE;

        /* Count the rest of the nodes in the group */
        for (psGroup = psStart->psNext;
             psGroup != NULL;
             psGroup = psGroup->psNext, uLength++)
        {
            uGroupNode = psGroup - psRegState->sRAData.asRegGroup;

            /* Include mandatory registers in the required size */
			uMinGroupSize += 1;

			if (bFindLastReqNode)
			{
				uLastReqNode = uGroupNode;
				bFindLastReqNode = IMG_FALSE;
			}

            /* Record maximum and minimum of current group interval */
            GetGroupEndPoints(psRegState, uGroupNode, IMG_FALSE,
                              &auReqBound[0],
                              &auReqBound[1]);
		}

        /* Set the head of the group as the node to be coloured */
        if (uBaseNode != uNode)
		{
            /* Get the interval for this node */
            RegIntvlAdvance(psRegState->apsRegInterval[uBaseNode],
                            auReqBound[0], auReqBound[1],
                            &psTmp);
            if (psTmp != NULL)
			{
                psCurrInterval = psTmp;
                uNode = uBaseNode;
			}
		}

        /* Set the length if the interval hasn't already been processed */
        if (psCurrInterval->uColour == USC_UNDEF_COLOUR &&
            !psCurrInterval->bSpill)
		{
            /* Interval hasn't been assigned a colour */
            psCurrInterval->uCount = max(psCurrInterval->uCount, uLength);
        }
    }

  Exit:
    /* Set the return values */
    memcpy(auMinInterval, auReqBound, sizeof(auReqBound));
    *puMinGroupSize = uMinGroupSize;
    *puLastReqNode = uLastReqNode;

    return psCurrInterval;
}

static
IMG_BOOL AssignFixedColours(PINTERMEDIATE_STATE psState,
							PRALSCAN_STATE psRegState,
							IMG_PUINT32 puMaxColour)
/*****************************************************************************
 FUNCTION     : AssignFixedColours

 PURPOSE      : Assign colours to intervals for fixed colour nodes

 PARAMETERS   : psState           - Compiler state.
                psRegState        - Register allocator state

 OUTPUT       : puMaxColour       - Maximum colour used

 RETURNS      : IMG_FALSE iff failed to assign a colour
*****************************************************************************/
{
	PUSC_REG_INTERVAL psCurrInterval;
	PUSC_REG_INTERVAL psNextInterval;
	IMG_UINT32 uNode;
	IMG_UINT32 uColour;
	IMG_UINT32 uMaxColour;

    psNextInterval = NULL;
    psRegState->psFixed = ProcListFindStart(psRegState->psFixed);
	uMaxColour = *puMaxColour;

    for (psCurrInterval = psRegState->psFixed;
		 psCurrInterval != NULL;
		 psCurrInterval = psNextInterval)
    {
        psNextInterval = psCurrInterval->psProcNext;
        psRegState->psFixed = ProcListRemove(psCurrInterval, psRegState->psFixed);

        uNode = psCurrInterval->uNode;
        uColour = psRegState->auFixedColour[uNode];

		SetTieColour(psRegState, psCurrInterval, uColour);
        psRegState->apsColour[uColour] = ProcListAdd(psCurrInterval,
													 psRegState->apsColour[uColour]);

        uMaxColour = max(uMaxColour, uColour);
    }
    ASSERT(psRegState->psFixed == NULL);

    if (puMaxColour != NULL)
        (*puMaxColour) = uMaxColour;

	return IMG_TRUE;
}

static
IMG_BOOL FindGroupColour(PRALSCAN_STATE psRegState,
						 PUSC_REG_INTERVAL psBaseInterval,
						 IMG_PUINT32 puBaseColour)
/*****************************************************************************
 FUNCTION     : FindGroupColour

 PURPOSE      : Find colours for a group of intervals

 PARAMETERS   : psRegState        - Register allocator state
                psBaseInterval    - Interval of the base node of the group

 OUTPUT       : puBaseColour      - Colour for the base node

 RETURNS      : IMG_FALSE iff failed to assign a colour
*****************************************************************************/
{
	PINTERMEDIATE_STATE psState = psRegState->sRAData.psState;
	IMG_UINT32 uColour; // Colour to use
	IMG_BOOL bSpill;
	IMG_BOOL bFindColour;
	IMG_UINT32 uNode;

	uNode = psBaseInterval->uNode;

	uColour = USC_UNDEF_COLOUR;
	bSpill = IMG_FALSE;
	bFindColour = IMG_TRUE;
	while (bFindColour)
	{
		bFindColour = IMG_FALSE;
		if (!FindColourRange(psRegState, psBaseInterval, &uColour))
		{
			/* Can't colour the group so spill an interval */
			PUSC_REG_INTERVAL psSpill = NULL;

			bSpill = IMG_TRUE;
			if (FindSpillRange(psRegState, psBaseInterval, &psSpill))
			{
				/* Mark the node to be spilled */
				if (psSpill != psBaseInterval)
				{
					IMG_UINT32 uSpillBase = psSpill->uNode;
					if (psSpill->uBaseNode < psRegState->sRAData.uAvailRegs)
					{
						uSpillBase = psSpill->uBaseNode;
					}
					SpillGroup(psRegState,
							   uSpillBase,
							   psSpill->uStart, psSpill->uEnd);
					bSpill = IMG_FALSE;
				}

				bFindColour = IMG_TRUE;
			}
			else if (NodeIsTempReg(psRegState, psBaseInterval->uNode))
			{
				bSpill = IMG_TRUE;
			}
			else
			{
				IMG_BOOL bCantSpillRegisters = IMG_FALSE;
				ASSERT(bCantSpillRegisters);
			}
		}
	}

	if (bSpill)
	{
		*puBaseColour = USC_UNDEF;
		return IMG_FALSE;
	}
	else
	{
		*puBaseColour = uColour;
		return IMG_TRUE;
	}
}

static
IMG_VOID SetGroupColour(PRALSCAN_STATE psRegState,
						PUSC_REG_INTERVAL psBaseInterval,
						IMG_UINT32 uBaseColour,
						IMG_BOOL bSpill,
						PUSC_REG_INTERVAL *ppsIntervalList,
						IMG_PUINT32 puMaxColour)
/*****************************************************************************
 FUNCTION     : SetGroupColour

 PURPOSE      : Set the colours of a group of intervals

 PARAMETERS   : psRegState        - Register allocator state
                psBaseInterval    - Interval of the base node of the group
                uBaseColour       - Colour for the base node
                bSpill            - Whether group has to be spilled

 INPUT/OUTPUT : ppsIntervals      - List of intervals being processed

 OUTPUT       : uMaxColour        - Maximum colour used

 RETURNS      : Nothing
*****************************************************************************/
{
	PINTERMEDIATE_STATE psState = psRegState->sRAData.psState;
	IMG_UINT32 uStart, uEnd;
	IMG_UINT32 uColour;
	IMG_UINT32 uMaxColour;
	IMG_UINT32 uBaseNode;
	IMG_UINT32 uGroupNode;
	REGISTER_GROUP* psNodeGroup;
	PUSC_REG_INTERVAL psNodeInterval;
	PUSC_REG_INTERVAL psIntervalList;

	psIntervalList = *ppsIntervalList;

	/*
	   Set colour or spill and interval start-, end-points for
	   interval of each node in the group which fall between start and end.
	*/
	uColour = uBaseColour;
	uMaxColour = uColour;
	uStart = psBaseInterval->uStart;
	uEnd = psBaseInterval->uEnd;
	uBaseNode = psBaseInterval->uNode;

	for (psNodeGroup = &psRegState->sRAData.asRegGroup[uBaseNode];
		 psNodeGroup != NULL;
		 psNodeGroup = psNodeGroup->psNext)
	{
		/* Get the node to colour */
		uGroupNode = psNodeGroup - &(psRegState->sRAData.asRegGroup[0]);

		/* Advance to the first interval */
		RegIntvlAdvance(psRegState->apsRegInterval[uGroupNode],
						uStart, uEnd, &psNodeInterval);

		if (psNodeInterval != NULL)
		{
			/* Remove an existing interval from the list to be processed */
			psIntervalList = ProcListRemove(psNodeInterval, psIntervalList);
		}
		else
		{
			PUSC_REG_INTERVAL psTmp;

			/* Create an interval for this node */
			psNodeInterval = (PUSC_REG_INTERVAL)UscAlloc(psState,
														 sizeof(USC_REG_INTERVAL));
			InitRegInterval(psNodeInterval, uGroupNode);

			psNodeInterval->uStart = uStart;
			psNodeInterval->uEnd = uEnd;
			psNodeInterval->uBaseNode = uBaseNode;

			if (psNodeInterval->psRegNext != NULL)
			{
				if (EarlierThan(psNodeInterval->psRegNext->uStart, psNodeInterval->uEnd))
				{
					psNodeInterval->uEnd = psNodeInterval->psRegNext->uStart;
				}
			}

			psTmp = RegIntvlInsert(psRegState,
								   psNodeInterval,
								   psRegState->apsRegInterval[uGroupNode],
								   IMG_FALSE);
			psRegState->apsRegInterval[uGroupNode] = psTmp;
		}

		/* Spill or colour the node */
		if (bSpill)
		{
			psNodeInterval->bSpill = IMG_TRUE;
		}
		else
		{
			PUSC_REG_INTERVAL psColourList = NULL;

			ASSERT((!GetBit(psRegState->auIsFixed, psNodeInterval->uNode)) ||
				   uColour == psRegState->auFixedColour[uBaseNode]);

			/* Set the interval colour */
			psNodeInterval->bSpill = IMG_FALSE;
			SetTieColour(psRegState, psNodeInterval, uColour);

			/* Extend the start and end points of non-optional nodes */
			psNodeInterval->uStart = psBaseInterval->uStart;
			psNodeInterval->uEnd = psBaseInterval->uEnd;

			/* Record the use of the interval colour */
			ASSERT((GetBit(psRegState->auIsFixed, psNodeInterval->uNode) &&
					uColour == psRegState->auFixedColour[psNodeInterval->uNode]) ||
				   uColour == psNodeInterval->uColour);
			psColourList = ProcListAdd(psNodeInterval,
									   psRegState->apsColour[uColour]);
			psRegState->apsColour[uColour] = psColourList;

			/* Record the maximum colour used so far */
			uMaxColour = max(uMaxColour, psNodeInterval->uColour);

			/* Advance to next colour */
			uColour += 1;
		}
	}

	/* Set return values */
	*puMaxColour = uMaxColour;
	*ppsIntervalList = psIntervalList;
}

static
IMG_VOID ComputeNodeColours(PINTERMEDIATE_STATE psState,
                            PRALSCAN_STATE psRegState,
                            IMG_PUINT32 puMaxColour,
							IMG_PBOOL pbSpilling)
/*****************************************************************************
 FUNCTION     : ComputeNodeColours

 PURPOSE      : Find and assign registers (colours) to each register liveness
                interval. Spill the intervals that can't be coloured.

 PARAMETERS   : psState           - Compiler state.
                psRegState        - Register allocator state

 INPUT/OUTPUT : puMaxColour       - Maximum colour used
                pbSpilling        - Whether any node is spilled

 RETURNS      : Nothing.
*****************************************************************************/
{
	PUSC_REG_INTERVAL psActiveList;  // The list of active intervals
    IMG_UINT32 uMaxSpillRegs;        // Max number of spill registers needed.
    PUSC_REG_INTERVAL psCurrInterval;// Interval being processed
    PUSC_REG_INTERVAL psBaseInterval;// Base interval of register group
    PUSC_REG_INTERVAL psNextInterval;// Next interval to process
    IMG_UINT32 uMaxColour;
	IMG_BOOL bSpillingSetup;         // Whether setup for spiling has been done

    /*
     * Initialisation
     */
    uMaxSpillRegs = 0;
    uMaxColour = *puMaxColour;
    psActiveList = NULL;
	bSpillingSetup = IMG_FALSE;

    /*
     * Allocation
     */

    /*
	   1) Record the intervals for fixed colour node.
	   2) Allocate remaining intervals.
	*/

    /* Fixed colour nodes */
	AssignFixedColours(psState, psRegState, &uMaxColour);

    /* Iterate through intervals, allocating colours */
    psNextInterval = NULL;

    psRegState->psIntervals = ProcListFindStart(psRegState->psIntervals);

    for (psCurrInterval = psRegState->psIntervals;
		 psCurrInterval != NULL;
		 psCurrInterval = psNextInterval)
    {
        IMG_UINT32 auReqInterval[2] = { USC_OPEN_START, USC_OPEN_END};
        IMG_UINT32 uMinGroupSize;
        IMG_UINT32 uLastReqNode;
		IMG_BOOL bSpill;   // Whether to spill the group
		IMG_UINT32 uBaseColour;
		IMG_UINT32 uGroupMaxColour;

		ASSERT(!GetBit(psCurrInterval->auFlags, INTVL_PENDING));

        psNextInterval = psCurrInterval->psProcNext;
        psRegState->psIntervals = ProcListRemove(psCurrInterval, psRegState->psIntervals);

        /*
		 * Colour a group of live intervals
		 */

		/* Find the head of the group the current node is in and the group size */
		uMinGroupSize = 1;
		psBaseInterval = GetBaseInterval(psRegState, psCurrInterval,
										 auReqInterval,
										 &uMinGroupSize,
										 &uLastReqNode);
		/* Extend interval start and end to include interval used */
		psBaseInterval->uStart = auReqInterval[0];
		psBaseInterval->uEnd = auReqInterval[1];

		/* Colour or spill the group node */
		uBaseColour = USC_UNDEF_COLOUR;
		bSpill = IMG_FALSE;

		/* Find colours for the group or decide to spill it */
		if (!FindGroupColour(psRegState, psBaseInterval,
							 &uBaseColour))
		{
			/* Failed to colour the group so spill it */
			bSpill = IMG_TRUE;
		}

		/*
		 * Set the colour (or spill) for each group node
		 */
		SetGroupColour(psRegState, psBaseInterval,
					   uBaseColour, bSpill,
					   &(psRegState->psIntervals),
					   &uGroupMaxColour);

		if (bSpill)
		{
			*pbSpilling = IMG_TRUE;
		}
		else
		{
			uMaxColour = max(uMaxColour, uGroupMaxColour);
		}

		/*
		 * Because psBaseInterval may point into the middle of the
		 * interval list, the next interval has to be
		 * recalculated.
		 */

		psNextInterval = psRegState->psIntervals;
	}

    ASSERT(psRegState->psIntervals == NULL);
    psRegState->psIntervals = NULL;

    /* Set return values */
    *puMaxColour = uMaxColour;
}


static
IMG_UINT32 GetNodeMemLoc(PRALSCAN_STATE psRegState,
						 IMG_UINT32 uNode)
/*****************************************************************************
 FUNCTION	: GetNodeMemLoc

 PURPOSE	: The memory location allocated to a node.

 PARAMETERS	: psRegState	- Register allocator state
			  uNode         - Node

 RETURNS	: Address of memory location
*****************************************************************************/
{
	IMG_UINT32 uLoc = USC_UNDEF;

	if (GetBit(psRegState->auNodeHasLoc, uNode))
	{
		return psRegState->auNodeMemLoc[uNode];
	}
	else
	{
		uLoc = psRegState->sRAData.psState->uSpillAreaSize ++;

		psRegState->auNodeMemLoc[uNode] = uLoc;
		SetBit(psRegState->auNodeHasLoc, uNode, IMG_TRUE);

		return uLoc;
	}
}

static
IMG_VOID SetRegColour(PARG psReg,
					  IMG_UINT32 uNewType,
					  IMG_UINT32 uNewNum)
/*****************************************************************************
 FUNCTION   : SetRegColour

 PURPOSE    : Set a registers new type and number

 PARAMETERS : psReg       - Register to update
              uNewType    - New type
              uNewNum     - New number

 RETURNS    : Nothing
*****************************************************************************/
{
	if (psReg->uType == USEASM_REGTYPE_IMMEDIATE &&
		psReg->uImmTempNum != UINT_MAX)
	{
		psReg->uImmTempType = uNewType;
		psReg->uImmTempNum = uNewNum;
	}
	else if (psReg->uType == USEASM_REGTYPE_TEMP)
	{
		psReg->uType = uNewType;
		psReg->uNumber = uNewNum;
	}
}

static
IMG_VOID LoadStoreIRegs(PRALSCAN_STATE psRegState,
						PCODEBLOCK psBlock,
						PINST psPoint,
						IMG_UINT32 uIRegsLive,
						IMG_BOOL bLoad)
/*****************************************************************************
 FUNCTION   : LoadStoreIRegs

 PURPOSE    : Load or store internal registers

 PARAMETERS : psRegState    - Register allocator state
              psBlock       - Code block containing the instruction
              psPoint       - Insertion point
              bLoad         - Load (IMG_TRUE) or store (IMG_FALSE) iregisters

 RETURNS    : Nothing
*****************************************************************************/
{
	const IMG_UINT32 uAlphaComponent = 4;
	PINTERMEDIATE_STATE psState = psRegState->sRAData.psState;
	IMG_UINT32 uCtr;
	IMG_UINT32 uRGBNode, uAlphaNode, uAlphaChan;
	PINST psInst;

	if (bLoad && (psRegState->uIRegSaveMask == 0))
	{
		/* No iregisters to load */
		return;
	}

	/* Load/store each live register */
	for (uCtr = 0; uCtr < NUM_INTERNAL_REGISTERS; uCtr ++)
	{
		IMG_BOOL bHasAlpha = IMG_FALSE;

		if (!(uIRegsLive & (1U << uCtr)))
			continue;

		if (psRegState->uIRegC10Mask & (1U << uCtr))
			bHasAlpha = IMG_TRUE;

		ASSERT(psRegState->uProgIRegsLive & (1U << uCtr));
		uRGBNode = psRegState->apsIRegStore[uCtr].uRGBNode;
		ASSERT(uRGBNode <= psRegState->sRAData.uAvailRegs);

		/* Load/Store the RGB part */
		psInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psInst, IMOV);

		if (bLoad)
		{
			/* Restore iregister */
			psInst->asDest[0].uType = USEASM_REGTYPE_FPINTERNAL;
			psInst->asDest[0].uNumber = uCtr;

			GetNodeReg(psRegState, uRGBNode,
					   &psInst->asArg[0].uType,
					   &psInst->asArg[0].uNumber);
		}
		else
		{
			/* Save iregister */
			GetNodeReg(psRegState, uRGBNode,
					   &psInst->asDest[0].uType,
					   &psInst->asDest[0].uNumber);

			psInst->asArg[0].uType = USEASM_REGTYPE_FPINTERNAL;
			psInst->asArg[0].uNumber = uCtr;
			psInst->asArg[0].bKilled = IMG_TRUE;
		}

		/* Insert the load/store */
		InsertInstBefore(psState, psBlock, psInst, psPoint);

		/* Load/store the alpha part */
		if (bHasAlpha)
		{
			uAlphaNode = psRegState->apsIRegStore[uCtr].uAlphaNode;
			uAlphaChan = psRegState->apsIRegStore[uCtr].uAlphaChan;

			ASSERT(uAlphaNode <= psRegState->sRAData.uAvailRegs);

			psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, IPCKC10C10);

			if (bLoad)
			{
				/* Restore iregister */
				psInst->asDest[0].uType = USEASM_REGTYPE_FPINTERNAL;
				psInst->asDest[0].uNumber = uCtr;
				psInst->auDestMask[0] = (1U << uAlphaComponent);

				GetNodeReg(psRegState, uAlphaNode,
						   &psInst->asArg[0].uType,
						   &psInst->asArg[0].uNumber);
				SetPCKComponent(psState, psInst, 0 /* uArgIdx */, uAlphaChan);
				psInst->asArg[0].uComponent = uAlphaChan;
			}
			else
			{
				/* Save iregister */
				GetNodeReg(psRegState, uAlphaNode,
						   &psInst->asDest[0].uType,
						   &psInst->asDest[0].uNumber);
				psInst->auDestMask[0] = (1U << uAlphaChan);

				psInst->asArg[0].uType = USEASM_REGTYPE_FPINTERNAL;
				psInst->asArg[0].uNumber = uCtr;
				SetPCKComponent(psState, psInst, 0 /* uArgIdx */, uAlphaComponent);
				psInst->asArg[0].uComponent = uAlphaComponent;
				psInst->asArg[0].bKilled = IMG_TRUE;
			}

			psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psInst->asArg[1].uNumber = 0;

			/* Insert the load/store */
			InsertInstBefore(psState, psBlock, psInst, psPoint);
		}

		if (bLoad)
		{
			psRegState->uIRegLoadMask |= (1U << uCtr);
		}
		else
		{
			psRegState->uIRegSaveMask |= (1U << uCtr);
			psRegState->uIRegWriteMask &= ~(1U << uCtr);
			psRegState->uIRegLoadMask &= ~(1U << uCtr);
		}
	}
}

static
IMG_VOID InsertLdSt(PRALSCAN_STATE psRegState,
					PCODEBLOCK psBlock,
					PINST psInst,
					IMG_UINT32 uMemLoc,
					IMG_UINT32 uRegType,
					IMG_UINT32 uRegNum,
					IMG_BOOL bLoad,
					IMG_BOOL bPreInst)
/*****************************************************************************
 FUNCTION   : InsertLdSt

 PURPOSE    : Load or store a node

 PARAMETERS : psRegState    - Register allocator state
              psBlock       - Code block containing the instruction
              psInst        - Instruction being processed
              psInterval    - Interval to restore
              bPreInst      - Saves are added before the instruction being renamed.

 RETURNS    : Nothing
*****************************************************************************/
{
	PINTERMEDIATE_STATE psState = psRegState->sRAData.psState;
	PINST psSpill = NULL;

	psSpill = AllocateInst(psState, IMG_NULL);
	if (bLoad)
	{
        SetOpcode(psState, psSpill, ISPILLREAD);
        psSpill->asDest[0].uType = uRegType;
        psSpill->asDest[0].uNumber = uRegNum;
    }
    else
    {
        SetOpcode(psState, psSpill, ISPILLWRITE);
        psSpill->asDest[0].uType = USC_REGTYPE_DUMMY;
        psSpill->asDest[0].uNumber = 0;
	}
	psState->uOptimizationHint |= USC_COMPILE_HINT_LOCAL_LOADSTORE_USED;
	
	SetBit(psSpill->auFlag, INST_SPILL, IMG_TRUE);

	psSpill->asArg[0].uType = USEASM_REGTYPE_SECATTR;
	psSpill->asArg[0].uNumber = psState->psSAOffsets->uScratchBase;

	psSpill->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
	psSpill->asArg[1].uNumber = uMemLoc;

	GetNodeReg(psRegState, psRegState->uAddrReg,
			   &psSpill->asArg[1].uImmTempType,
			   &psSpill->asArg[1].uImmTempNum);

    if (bLoad)
    {
        psSpill->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
        psSpill->asArg[2].uNumber = 0;
        psSpill->asArg[3].uType = USEASM_REGTYPE_DRC;
        psSpill->asArg[3].uNumber = 0;
    }
    else
    {
        psSpill->asArg[2].uType = uRegType;
        psSpill->asArg[2].uNumber = uRegNum;
    }

    InsertInstBefore(psState, psBlock, psSpill, psInst);

	psRegState->bSpillToMem = IMG_TRUE;

	if (bPreInst)
		psRegState->bSpillToMemPre = IMG_TRUE;
	else
		psRegState->bSpillToMemPost = IMG_TRUE;
}

static
IMG_VOID InsertMove(PRALSCAN_STATE psRegState,
					PCODEBLOCK psBlock,
					PINST psPoint,
					IMG_UINT32 uDest,
					IMG_UINT32 uSrc)
/*****************************************************************************
 FUNCTION   : InsertMove

 PURPOSE    : Insert a move between a node and a colour

 PARAMETERS : psRegState    - Register allocator state
              psBlock       - Code block containing the instruction
              psPoint       - Insertion point
              uDest         - Destination node
              uSrc          - Source node

 RETURNS    : Nothing
*****************************************************************************/
{
	PINTERMEDIATE_STATE psState = psRegState->sRAData.psState;
	PINST psMove;

	if (uDest == uSrc)
		return;

	psMove = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psMove, IMOV);
	/* Destination */
	GetNodeReg(psRegState,
			   uDest,
			   &psMove->asDest[0].uType,
			   &psMove->asDest[0].uNumber);
	/* Source */
	GetNodeReg(psRegState,
			   uSrc,
			   &psMove->asArg[0].uType,
			   &psMove->asArg[0].uNumber);

	InsertInstBefore(psState, psBlock, psMove, psPoint);
}

static
IMG_VOID SaveReg(PRALSCAN_STATE psRegState,
				 PCODEBLOCK psBlock,
				 PINST psInst,
				 PUSC_REG_INTERVAL psInterval,
				 IMG_BOOL bSave,
				 IMG_BOOL bPreInst)
/*****************************************************************************
 FUNCTION   : SaveReg

 PURPOSE    : Save a register, freeing its colour

 PARAMETERS : psRegState    - Register allocator state
              psBlock       - Code block containing the instruction
              psInst        - Instruction being processed
              psInterval    - Interval to restore
              bPreInst      - Saves are added before the instruction being renamed.

 RETURNS    : Nothing
*****************************************************************************/
{
	IMG_UINT32 uNode;
	IMG_UINT32 uColour;
	IMG_UINT32 uRegType;
	IMG_UINT32 uRegNum;
	IMG_UINT32 uMemLoc;

	if (psInterval == NULL)
		return;
	uNode = psInterval->uNode;
	uColour = psInterval->uColour;

	GetNodeReg(psRegState, uColour, &uRegType, &uRegNum);

	if (bSave)
	{
		uMemLoc = GetNodeMemLoc(psRegState, uNode);
		InsertLdSt(psRegState, psBlock, psInst,
				   uMemLoc, uRegType, uRegNum,
				   IMG_FALSE,
				   bPreInst);
	}

	/* Reset interval data */
	psInterval->psInst = NULL;
	if (psInterval->bSpill)
	{
		psInterval->uColour = USC_UNDEF;
	}
	psRegState->apsHwReg[uColour] = NULL;
	SetBit(psInterval->auFlags, INTVL_SAVE, IMG_FALSE);
}

static
IMG_VOID RestoreReg(PRALSCAN_STATE psRegState,
					PCODEBLOCK psBlock,
					PINST psInst,
					PUSC_REG_INTERVAL psInterval,
					IMG_BOOL bLoad,
					IMG_BOOL bPreInst)
/*****************************************************************************
 FUNCTION   : RestoreReg

 PURPOSE    : Restore a register to its colour

 PARAMETERS : psRegState    - Register allocator state
              psBlock       - Code block containing the instruction
              psInst        - Instruction being processed
              psInterval    - Interval to restore
              bPreInst      - Saves are added before the instruction being renamed.

 RETURNS    : Nothing
*****************************************************************************/
{
	IMG_UINT32 uNode;
	IMG_UINT32 uColour;
	IMG_UINT32 uRegType;
	IMG_UINT32 uRegNum;
	IMG_UINT32 uMemLoc;

	if (psInterval == NULL)
		return;

	uNode = psInterval->uNode;
	uColour = psInterval->uColour;

	GetNodeReg(psRegState, uColour, &uRegType, &uRegNum);

	if (bLoad)
	{
		uMemLoc = GetNodeMemLoc(psRegState, uNode);
		InsertLdSt(psRegState, psBlock, psInst,
				   uMemLoc, uRegType, uRegNum,
				   IMG_TRUE, bPreInst);
	}
	psRegState->apsHwReg[uColour] = psInterval;
	{
		PINTERMEDIATE_STATE psState = psRegState->sRAData.psState;
		ASSERT(psInterval->uColour == uColour);
	}
}

static
IMG_VOID SaveColour(PRALSCAN_STATE psRegState,
					PCODEBLOCK psBlock,
					PINST psInst,
					IMG_UINT32 uInstIdx,
					IMG_UINT32 uColour,
					IMG_BOOL bPreInst)
/*****************************************************************************
 FUNCTION   : SaveColour

 PURPOSE    : Save a colour

 PARAMETERS : psRegState    - Register allocator state
              psBlock       - Code block
              psInst        - Instruction being processed
              uColour       - First colour to save
              bPreInst      - Saves are added before the instruction being renamed.

 RETURNS    : Nothing
*****************************************************************************/
{
	PUSC_REG_INTERVAL psInterval;
	IMG_BOOL bSave = IMG_FALSE;
	IMG_UINT32 uBlockIndex;

	if (psBlock->psBody == NULL)
	{
		return;
	}
	uBlockIndex = psBlock->psBody->uId;
	/* Decide whether to save the interval */
	bSave = IMG_FALSE;

	psInterval = psRegState->apsHwReg[uColour];
	if (psInterval == NULL)
	{
		/*
		  If there is no interval, find the interval that should be in this colour
		*/
		psInterval = ProcListFindWith(psRegState->apsColour[uColour],
									  uBlockIndex,
									  IMG_FALSE);
        if (psInterval == NULL)
        {
            return;
        }
    }

	/* Start of interval is outside the code block */
	if (EarlierThan(psInterval->uStart, uBlockIndex) &&
		EarlierThan(uInstIdx, psInterval->uEnd))
	{
		bSave = IMG_TRUE;
	}

	/* Interval is marked for saving */
	if (GetBit(psInterval->auFlags, INTVL_SAVE))
	{
		if (EarlierThan(uInstIdx, psInterval->uEnd))
		{
			/* Interval is still live */
			bSave = IMG_TRUE;
		}
		else
		{
			SetBit(psInterval->auFlags, INTVL_SAVE, IMG_FALSE);
		}
	}

	/* Spill interval is not in a colour */
	if (psInterval->bSpill && !(psInterval->uColour < psRegState->sRAData.uAvailRegs))
	{
		bSave = IMG_FALSE;
	}

	/* Save the interval */
	if (bSave)
	{
		/* Interval must be saved */
		SaveReg(psRegState, psBlock, psInst, psInterval, IMG_TRUE, bPreInst);
		SetBit(psInterval->auFlags, INTVL_RESTORE, IMG_TRUE);
	}
	else
	{
		/* Only mark interval as saved */
		SaveReg(psRegState, psBlock, psInst, psInterval, IMG_FALSE, bPreInst);
	}
	psRegState->apsHwReg[uColour] = NULL;
	psRegState->apsRegInst[uColour] = NULL;
}

static
IMG_VOID LoadColour(PRALSCAN_STATE psRegState,
					PCODEBLOCK psBlock,
					PINST psInst,
					PUSC_REG_INTERVAL psInterval,
					IMG_UINT32 uNewColour,
					IMG_BOOL bPreInst)
/*****************************************************************************
 FUNCTION   : LoadColour

 PURPOSE    : Save a register into a colour

 PARAMETERS : psRegState    - Register allocator state
              psBlock       - Code block
              psInst        - Instruction being processed
              psInterval    - Interval to load
              uNewColour    - Colour to load interval into
              bPreInst      - Saves are added before the instruction being renamed.

 RETURNS    : Nothing
*****************************************************************************/
{
	IMG_UINT32 uBlockIndex;
	IMG_UINT32 uNode;
	IMG_BOOL bMove, bRestore;

	if (psInterval == NULL)
		return;
	if (psBlock->psBody == NULL)
		return;

	uBlockIndex = psBlock->psBody->uId;
	uNode = psInterval->uNode;

	bMove = IMG_FALSE;
	bRestore = IMG_FALSE;

	/* Test whether interval is in a colour */
	if (psInterval->bSpill &&
		psInterval->uColour < psRegState->sRAData.uAvailRegs)
	{
		if (psRegState->apsHwReg[psInterval->uColour] == psInterval)
		{
			bMove = IMG_TRUE;
		}
	}

	if (GetBit(psInterval->auFlags, INTVL_RESTORE))
	{
		bRestore = IMG_TRUE;
	}
	if (bMove)
	{
		/* Interval is in a colour, move it to the right colour */
		IMG_UINT32 uOldColour = psInterval->uColour;

		if (bRestore)
		{
			InsertMove(psRegState, psBlock, psInst,
					   uNewColour, uOldColour);
		}
		psRegState->apsHwReg[uOldColour] = NULL;
		psRegState->apsHwReg[uNewColour] = psInterval;
		{
			PINTERMEDIATE_STATE psState = psRegState->sRAData.psState;
			ASSERT(psInterval->uColour == uNewColour);
		}
	}
	else
	{
		/* Load interval from memory */
		if (psInterval->bSpill)
		{
			psInterval->uColour = uNewColour;
		}

		RestoreReg(psRegState, psBlock,
				   psInst, psInterval,
				   bRestore, bPreInst);
	}
	psRegState->apsHwReg[uNewColour] = psInterval;

	psRegState->apsRegInst[uNewColour] = psInst;
}


static
IMG_UINT32 WrapSpillColour(PRALSCAN_STATE psRegState,
						   IMG_UINT32 uIdx)
/*****************************************************************************
 FUNCTION   : WrapSpillColour

 PURPOSE    : Get a colour from an index (wraps around the number of registers).

 PARAMETERS : psRegState    - Register allocator state
              uIdx          - Index

 RETURNS    : A colour
*****************************************************************************/
{
	/* Shift left */
	uIdx -= psRegState->uRegAllocStart;
	/* Wrap around */
	uIdx = uIdx % (psRegState->uNumHwRegs - psRegState->uRegAllocStart);
	/* Shift right */
	uIdx += psRegState->uRegAllocStart;

	return uIdx;
}

static
IMG_UINT32 GroupStart(PRALSCAN_STATE psRegState,
					  IMG_UINT32 uNode)
/*****************************************************************************
 FUNCTION   : GroupStart

 PURPOSE    : Get the start of a register group

 PARAMETERS : psRegState    - Register allocator state
              uNode         - A node in the group

 RETURNS    : The first node in a register group
*****************************************************************************/
{
	REGISTER_GROUP *psGroup;
	IMG_UINT32 uBaseNode = USC_UNDEF;

	for (psGroup = &psRegState->sRAData.asRegGroup[uNode];
		 psGroup->psPrev != NULL;
		 psGroup = psGroup->psPrev)
	{
		/* Skip */
	}

	uBaseNode = psGroup - psRegState->sRAData.asRegGroup;

	return uBaseNode;
}

static
IMG_UINT32 ColourIsFree(PRALSCAN_STATE psRegState,
						PINST psInst,
						IMG_UINT32 uColour,
						IMG_UINT32 uLength)
/*****************************************************************************
 FUNCTION   : ColourIsFree

 PURPOSE    : Test whether a colour is currently in use at an instruction

 PARAMETERS : psRegState    - Register allocator state
              psInst        - Instruction being processed
              uColour       - First Colour to test
              uLength       - Length of range

 RETURNS    : IMG_TRUE iff colour is not used the instruction
*****************************************************************************/
{
	IMG_UINT32 uCtr;

	for (uCtr = 0; uCtr < uLength; uCtr ++)
	{
		IMG_UINT32 uIdx = uColour + uCtr;

		if (psRegState->apsRegInst[uIdx] != NULL &&
			psRegState->apsRegInst[uIdx] == psInst)
		{
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

static
IMG_UINT32 FindSpillColour(PRALSCAN_STATE psRegState,
						   PINST psInst,
						   IMG_UINT32 uNode,
						   IMG_BOOL bDest,
						   IMG_PUINT32 puColour,
						   IMG_PUINT32 puLength)
/*****************************************************************************
 FUNCTION   : FindSpillColour

 PURPOSE    : Find a colour for a spill register group

 PARAMETERS : psRegState    - Register allocator state
              psInst        - Instruction being processed
              uNode         - Node to be coloured
              bDest         - Node is a destination

 OUTPUT     : puColour      - Start of colour range for register group
              puLength      - Length of the colour range

 RETURNS    : IMG_FALSE iff no colour range could be found
*****************************************************************************/
{
	IMG_UINT32 uCtr;
	IMG_UINT32 uColour = USC_UNDEF;
	IMG_UINT32 uBest = USC_UNDEF;
	PUSC_REG_INTERVAL psBest = NULL;
	IMG_UINT32 uInstIdx = psInst->uId;
	IMG_UINT32 uBaseNode;
	IMG_UINT32 uLength;
	IMG_UINT32 uColourOffset;

	/* Get the start of the register group */
	{
		REGISTER_GROUP *psGroup;

		uLength = 1;
		for (psGroup = &psRegState->sRAData.asRegGroup[uNode];
			 psGroup->psPrev != NULL;
			 psGroup = psGroup->psPrev, uLength ++)
		{
			/* Skip */
		}

		uBaseNode = psGroup - psRegState->sRAData.asRegGroup;

		for (psGroup = psRegState->sRAData.asRegGroup[uNode].psNext;
			 psGroup != NULL;
			 psGroup = psGroup->psNext, uLength ++)
		{
			/* Skip */
		}
	}

	/* Find the first register to use */
	uColourOffset = psRegState->uNextSpillColour;
	{
		IMG_BOOL bNoFreeColour = IMG_TRUE;

		for (uCtr = psRegState->uRegAllocStart;
			 uCtr < psRegState->uNumHwRegs;
			 uCtr ++)
		{
			uColour = WrapSpillColour(psRegState, uCtr + uColourOffset);

			if (ColourIsFree(psRegState, psInst, uColour, uLength))
			{
				bNoFreeColour = IMG_FALSE;
				break;
			}
		}
		if (bNoFreeColour)
		{
			/* Failed to find a colour */
			return IMG_FALSE;
		}
	}
	uBest = uColour;

	/* If more than one colour is needed, just take the first valid range */
	if (uLength > 1)
	{
		goto Exit;
	}

	psBest = psRegState->apsHwReg[uColour];
	if (psBest == NULL)
	{
		psBest = RegIntvlFindWith(psRegState->apsColour[uColour],
								  uInstIdx, bDest);
		if (psBest == NULL)
		{
			goto Exit;
		}
	}

	uCtr += 1;
	for ( /* skip */;
					uCtr < psRegState->uNumHwRegs;
					uCtr ++)
	{
		PUSC_REG_INTERVAL psCurr;
		uColour = WrapSpillColour(psRegState, uCtr + uColourOffset);

		if(psRegState->apsRegInst[uColour] == psInst)
		{
			continue;
		}

		if(psRegState->apsHwReg[uColour] == NULL)
		{
			uBest = uColour;
			if (uColour < psRegState->sRAData.uAvailPARegs)
			{
				continue;
			}
		}

		psCurr = RegIntvlFindWith(psRegState->apsRegInterval[uColour],
								  uInstIdx, bDest);
		if (psCurr == NULL)
		{
			uBest = uColour;
			break;
		}

		{
			IMG_FLOAT fCurrIdle, fBestIdle;

			fCurrIdle = NodeActivity(psCurr,
									 psInst->uId, psCurr->uEnd);
			fBestIdle = NodeActivity(psBest,
									 psInst->uId, psBest->uEnd);

			if (fBestIdle < fCurrIdle)
			{
				uBest = uColour;
				continue;
			}
		}
	}

  Exit:
	if (uBest < psRegState->sRAData.uAvailRegs)
	{
		psRegState->uNextSpillColour = WrapSpillColour(psRegState, uBest + 1);
	}

	/* Set values */
	if (puColour != NULL)
		*puColour = uBest;
	if (puLength != NULL)
		*puLength = uLength;

	return IMG_TRUE;
}

static
PUSC_REG_INTERVAL RegListEarliest(PUSC_REG_INTERVAL psList,
								  IMG_UINT32 uIndex)
/*****************************************************************************
 FUNCTION     : RegListEarliest

 PURPOSE      : Find the earliest interval after a point.

 PARAMETERS   : psList    - List connected through psRegNext/psRegPrev
                uIndex    - Earliest point of interest

  RETURNS     : First interval beginning after uIndex
*****************************************************************************/
{
	PUSC_REG_INTERVAL psInterval;

	psInterval = RegIntvlFindPos(psList, uIndex, uIndex);
	if (psInterval == NULL)
		psInterval = RegListFindStart(psList);

	return psInterval;
}

static
IMG_VOID MarkRegGroup(PRALSCAN_STATE psRegState,
					  IMG_UINT32 uNode,
					  IMG_UINT32 uInstIdx,
					  PINST psInst,
					  IMG_UINT32 uFirstColour)
/*****************************************************************************
 FUNCTION   : MarkRegGroup

 PURPOSE    : Mark a register group with a colour and instruction

 PARAMETERS : psRegState    - Register allocator state
              uNode         - Any node in the group
              uInstIdx      - Index in the program of the instruction being processed
              psInst        - Instruction using a register in the group
              uFirstColour  - The colour of the first node in the group

 RETURNS    : Nothing
*****************************************************************************/
{
	IMG_UINT32 uBaseNode;
	REGISTER_GROUP* psGroup;
	IMG_UINT32 uColourOffset;

	uBaseNode = GroupStart(psRegState, uNode);
	for (psGroup = &psRegState->sRAData.asRegGroup[uBaseNode], uColourOffset = 0;
		 psGroup != NULL;
		 psGroup = psGroup->psNext, uColourOffset += 1)
	{
		IMG_UINT32 uCurr;
		IMG_UINT32 uNewColour;
		PUSC_REG_INTERVAL psInterval;

		uNewColour = uFirstColour + uColourOffset;

		/* Mark the colour as being in use */
		psRegState->apsRegInst[uNewColour] = psInst;

		/* Set the colour of the current group node */
		uCurr = psGroup - psRegState->sRAData.asRegGroup;
		psInterval = RegListEarliest(psRegState->apsRegInterval[uCurr],
									 uInstIdx);
		if (psInterval == NULL)
			continue;

		psInterval->uColour = uNewColour;
	}
}

static
IMG_BOOL SpillRegJIT(PRALSCAN_STATE psRegState,
					 PCODEBLOCK psBlock,
					 PINST psInst,
					 PARG psReg,
					 IMG_UINT32 uOperand,
					 IMG_BOOL bDest,
					 IMG_PUINT32 auOpRenamed)
/*****************************************************************************
 FUNCTION     : SpillRegJIT

 PURPOSE      : Spill a temoprary register

 PARAMETERS   : psRegState    - Register allocator state
                psBlock       - Code block
                psInst        - Instruction being processed
                psReg         - Register being processed
                uOperand      - Operand number being processed
                bDest         - Register is a destination

 INPUT/OUTPUT : auOpRenamed       - Renamed operands

 RETURNS      : IMG_FALSE iff node couldn't be spilled
*****************************************************************************/
{
	const IMG_UINT32 uDestCount = psInst->uDestCount;
	const IMG_UINT32 uArgCount = g_psInstDesc[psInst->eOpcode].uArgumentCount;
	const IMG_UINT32 uOperandOffset = bDest ? 0 : uDestCount;
	IMG_UINT32 uNode;
	IMG_UINT32 uBaseColour = USC_UNDEF;
	IMG_UINT32 uColour = USC_UNDEF;
	IMG_UINT32 uInstIdx;
	IMG_UINT32 uCtr;
	PINTERMEDIATE_STATE psState = psRegState->sRAData.psState;
	PUSC_REG_INTERVAL psCurrInterval = NULL;
	IMG_UINT32 uBaseNode = USC_UNDEF;
	IMG_BOOL bDestIgnored;
	IMG_BOOL bOverWrite;

	PVR_UNREFERENCED_PARAMETER(psBlock);

	if (!RegIsNode(&psRegState->sRAData, psReg))
	{
		return IMG_TRUE;
	}

	/* Get the instruction index, register node number and current interval */
	uInstIdx = psInst->uId;
	uNode = ArgumentToNode(&psRegState->sRAData, psReg);

	psCurrInterval = RegIntvlFindWith(psRegState->apsRegInterval[uNode],
									  uInstIdx,
									  bDest);
	if (psCurrInterval != NULL)
	{
	    psRegState->apsRegInterval[uNode] = psCurrInterval;
	}
	else
	{
		return IMG_FALSE;
	}
	ASSERT(psCurrInterval != NULL);
	ASSERT(psCurrInterval->bSpill);

	/* Don't restore an interval which will be overwritten */
	bDestIgnored = IMG_FALSE;
	bOverWrite = IMG_FALSE;
	if (bDest)
	{
		IMG_UINT32 uDestMask;

		if (psInst->eOpcode == IEFO &&
			psInst->u.psEfo->bIgnoreDest)
		{
			bDestIgnored = IMG_TRUE;
			bOverWrite = IMG_TRUE;
		}
		else
		{
			GetDestMask(psInst, &uDestMask);
			if (uDestMask == USC_DESTMASK_FULL)
			{
				bOverWrite = IMG_TRUE;
			}
		}
	}

	{
		IMG_BOOL bLoadReg = IMG_FALSE;

		/* Check whether node has a colour */
		uColour = psCurrInterval->uColour;
		if (uColour < psRegState->sRAData.uAvailRegs)
		{
			/* Check whether interval is in its colour */
			if (psRegState->apsHwReg[uColour] != psCurrInterval)
			{
				bLoadReg = IMG_TRUE;
			}
		}
		else /* Find a colour to use */
		{
			IMG_UINT32 uLength = 0;
			/* Node doesn't have a colour, get the base node for the register group */
			uBaseNode = GroupStart(psRegState, uNode);

			/* Find colours for the register group */
			if (FindSpillColour(psRegState, psInst, uBaseNode, bDest,
								&uBaseColour, &uLength))
			{
				MarkRegGroup(psRegState, uBaseNode, psInst->uId,
							 psInst, uBaseColour);

				/* Get the colour assigned to the current node */
				ASSERT(psCurrInterval->uColour <= psRegState->sRAData.uAvailRegs);
				uColour = psCurrInterval->uColour;

				bLoadReg = IMG_TRUE;
			}
			else
			{
				/* Couldn't find colours for all the group */
				return IMG_FALSE;
			}
		}

		ASSERT(uColour <= psRegState->uNumHwRegs);

		if (bLoadReg && !bDestIgnored)
		{
			/* Save the register currently in the colour */
			SaveColour(psRegState, psBlock, psInst, psInst->uId,
					   uColour, IMG_TRUE);

			/* Load the register into the colour */
			LoadColour(psRegState, psBlock, psInst,
					   psCurrInterval, uColour,
					   !bOverWrite);
		}
	}

	/*
	  Rename the register wherever it appears in the operands
	*/
	ASSERT(uColour <= psRegState->sRAData.uAvailRegs);
	{
		IMG_UINT32 uNewType;
		IMG_UINT32 uNewNum;

		/* Calculate the registers new type and number */
		GetNodeReg(psRegState, uColour, &uNewType, &uNewNum);

		/* Rename operands */
		if (bDest)
		{
			PARG psOperand = psReg;

			SetRegColour(psOperand, uNewType, uNewNum);
			SetBit(auOpRenamed, uOperand + uOperandOffset, IMG_TRUE);
		}
		else
		{
			for (uCtr = 0; uCtr < uArgCount; uCtr ++)
			{
				PARG psOperand = &psInst->asArg[uCtr];

				if (GetBit(auOpRenamed, uCtr + uOperandOffset))
					continue;
				if (!RegIsNode(&psRegState->sRAData, psOperand))
					continue;
				if (ArgumentToNode(&psRegState->sRAData, psOperand) != uNode)
					continue;

				SetRegColour(psOperand, uNewType, uNewNum);
				SetBit(auOpRenamed, uCtr + uOperandOffset, IMG_TRUE);
			}
		}
	}

	/* Mark the write */
	if (bDest)
	{
		SetBit(psCurrInterval->auFlags, INTVL_SAVE, IMG_TRUE);
		SetBit(psCurrInterval->auFlags, INTVL_RESTORE, IMG_FALSE);
	}
	/* Mark the use */
	psRegState->apsRegInst[uColour] = psInst;

	/* Tests */
	if (!bDestIgnored)
	{
		ASSERT(psRegState->apsHwReg[psCurrInterval->uColour] == psCurrInterval);
		ASSERT(psRegState->apsRegInst[uColour] == psInst);
	}

	/* Done */
	return IMG_TRUE;
}

static
IMG_BOOL RenameRegJIT(PRALSCAN_STATE psRegState,
					  PCODEBLOCK psBlock,
                      PINST psInst,
                      PARG psReg,
					  IMG_UINT32 uOperand,
                      IMG_BOOL bDest,
					  IMG_PUINT32 auOpRenamed,
                      IMG_PBOOL pbDeadReg)
/*****************************************************************************
 FUNCTION     : RenameRegJIT

 PURPOSE      : Rename a temoprary register with its newly assigned colour.

 PARAMETERS   : psRegState    - Register allocator state
                psBlock       - Code block containing the instruction
                psInst        - Instruction being processed
                psReg         - Register being processed
                uOperand      - Operand number being processed
                bDest         - Register is a destination

 INPUT/OUTPUT : auOpRenamed       - Renamed operands

 OUTPUT       : pbDeadReg     - Register is dead.

 RETURNS      : IMG_FALSE iff register is to be spilled
*****************************************************************************/
{
	const IMG_UINT32 uDestCount = psInst->uDestCount;
	const IMG_UINT32 uArgCount = g_psInstDesc[psInst->eOpcode].uArgumentCount;
	const IMG_UINT32 uOperandOffset = bDest ? 0 : uDestCount;
	IMG_UINT32 uNode;
	IMG_UINT32 uColour = USC_UNDEF;
	IMG_UINT32 uInstIdx;
	IMG_UINT32 uCtr;
	PINTERMEDIATE_STATE psState = psRegState->sRAData.psState;
	PUSC_REG_INTERVAL psCurrInterval = NULL;
	IMG_BOOL bOverWrite;

	PVR_UNREFERENCED_PARAMETER(psBlock);

	if (!RegIsNode(&psRegState->sRAData, psReg))
	{
		return IMG_TRUE;
	}

	/* Get the instruction index, register node number and current interval */
	uInstIdx = psInst->uId;
	uNode = ArgumentToNode(&psRegState->sRAData, psReg);

	psCurrInterval = RegIntvlFindWith(psRegState->apsRegInterval[uNode],
									  uInstIdx,
									  bDest);
	if (psCurrInterval != NULL)
	{
	    psRegState->apsRegInterval[uNode] = psCurrInterval;
	}
	else
	{
		return IMG_TRUE;
	}
	ASSERT(psCurrInterval != NULL);

	/* Test whether the node represents a dead register */
	if (!bDest &&
		!GetBit(psRegState->auIsWritten, uNode))
	{
		/* Node is never written, test for deadness */

	    if (psReg->uType == USEASM_REGTYPE_TEMP &&
			!IsNodeInGroup(&psRegState->sRAData, uNode))
		{
			*pbDeadReg = IMG_TRUE;
			return IMG_TRUE;
		}
	}

	/* Check whether node has a colour or must be spilled */
	if (psCurrInterval->bSpill)
	{
		/*
		  Interval is a spill node,
		  check whether it has a colour assigned
		*/
		if (psCurrInterval->uColour < psRegState->sRAData.uAvailRegs)
		{
			if (psRegState->apsHwReg[psCurrInterval->uColour] == psCurrInterval)
			{
				/* Spill node has a colour assigned, so mark the colour
				   as being used by this instructin */
				psRegState->apsRegInst[psCurrInterval->uColour] = psInst;
			}
		}

	    return IMG_FALSE;
	}
	else
	{
		uColour = psCurrInterval->uColour;
	}

	/* Don't restore an interval which will be overwritten */
	bOverWrite = IMG_FALSE;
	if (bDest)
	{
		IMG_UINT32 uDestMask;

		GetDestMask(psInst, &uDestMask);
		if (uDestMask == USC_DESTMASK_FULL)
		{
			bOverWrite = IMG_TRUE;
		}
	}

	/*
	  Got a colour, test whether it is currently in use.
	*/
	ASSERT(uColour <= psRegState->sRAData.uAvailRegs);
	if (psRegState->apsHwReg[uColour] != psCurrInterval)
	{
		PUSC_REG_INTERVAL psResident = psRegState->apsHwReg[uColour];
		/* Interval not in its colour */
		if (psResident != NULL)
		{
			/* Colour in use, expel the current interval */
			SaveColour(psRegState, psBlock, psInst,
					   psInst->uId,
					   uColour, IMG_TRUE);
		}

		LoadColour(psRegState, psBlock, psInst,
				   psCurrInterval, uColour, !bOverWrite);
	}

	/*
	  Rename the register wherever it appears in the operands
	*/
	ASSERT(uColour < psRegState->sRAData.uAvailRegs);
	{
		IMG_UINT32 uNewType;
		IMG_UINT32 uNewNum;

		/* Calculate the registers new type and number */
		GetNodeReg(psRegState, uColour, &uNewType, &uNewNum);

		/* Rename operands */
		if (bDest)
		{
			PARG psOperand = psReg;

			SetRegColour(psOperand, uNewType, uNewNum);
			SetBit(auOpRenamed, uOperand + uOperandOffset, IMG_TRUE);
		}
		else
		{
			for (uCtr = 0; uCtr < uArgCount; uCtr ++)
			{
				PARG psOperand = &psInst->asArg[uCtr];

				if (GetBit(auOpRenamed, uCtr + uOperandOffset))
					continue;
				if (!RegIsNode(&psRegState->sRAData, psOperand))
					continue;
				if (ArgumentToNode(&psRegState->sRAData, psOperand) != uNode)
					continue;

				SetRegColour(psOperand, uNewType, uNewNum);
				SetBit(auOpRenamed, uCtr + uOperandOffset, IMG_TRUE);
			}
		}
	}

	/* Mark the write */
	if (bDest)
	{
		SetBit(psCurrInterval->auFlags, INTVL_SAVE, IMG_TRUE);
		SetBit(psCurrInterval->auFlags, INTVL_RESTORE, IMG_FALSE);
	}
	/* Mark the use */
	psRegState->apsRegInst[uColour] = psInst;

	/* Set return values */
	*pbDeadReg = IMG_FALSE;

	/* Tests */
	{
		ASSERT(psRegState->apsHwReg[psCurrInterval->uColour] == psCurrInterval);
		ASSERT(psRegState->apsRegInst[uColour] == psInst);
	}

	return IMG_TRUE;
}

static
IMG_VOID RestoreBlockColours(PRALSCAN_STATE psRegState,
							 PCODEBLOCK psBlock)
/*****************************************************************************
 FUNCTION	  : RestoreBlockColours

 PURPOSE	  : Restore registers live  out of a block to their colours

 PARAMETERS	  : psState			  - Compiler state.
                psRegState        - Register allocator state

 RETURNS	  : Nothing.
*****************************************************************************/
{
	PINTERMEDIATE_STATE psState = psRegState->sRAData.psState;
	PINST psFirstInst, psLastInst;
	IMG_UINT32 uFirstIndex = USC_OPEN_START;
	IMG_UINT32 uLastIndex = USC_OPEN_END;
	IMG_UINT32 uColour;
	PREGISTER_LIVESET psLiveOut;

	psFirstInst = psBlock->psBody;
	if (psFirstInst == NULL)
	{
		/* Nothing to do */
		return;
	}

	uFirstIndex = psFirstInst->uId;
	psLastInst = psBlock->psBodyTail;
	uLastIndex = psLastInst->uId;

	psLiveOut = &psBlock->sRegistersLiveOut;

	/*
	  Restore registers at the end of the block:
	  - save spilled registers to memory
	  - restore registers to their colour
	*/
	for (uColour = psRegState->uRegAllocStart;
		 uColour < psRegState->sRAData.uAvailRegs;
		 uColour ++)
	{
		PUSC_REG_INTERVAL psSpillInterval;
		PUSC_REG_INTERVAL psRegInterval;

		psSpillInterval = psRegState->apsHwReg[uColour];
		psRegInterval = RegIntvlFindWith(psRegState->apsColour[uColour],
										 uLastIndex,
										 IMG_FALSE);

		if (psSpillInterval == NULL)
			continue;

		if (!psSpillInterval->bSpill)
		{
			/* Interval is not a spill register */
			ASSERT(psSpillInterval->uColour == uColour);
			continue;
		}

		/* Evict register from colour */
		SaveColour(psRegState, psBlock, NULL, uLastIndex,
				   psSpillInterval->uColour, IMG_FALSE);

		if (psRegInterval != NULL)
		{
			/* Restore original register to colour */
			LoadColour(psRegState, psBlock, NULL, psRegInterval,
					   uColour, IMG_FALSE);
		}
	}
}

static
IMG_VOID InitIRegStore(PRALSCAN_STATE psRegState)
/*****************************************************************************
 FUNCTION	  : InitIRegStore

 PURPOSE	  : Set up the registers used to store iregisters

 PARAMETERS	  : psRegState        - Register allocator state

 RETURNS	  : Nothing.
*****************************************************************************/
{
	const IMG_UINT32 uChansPerReg = 3;
	IMG_UINT32 uCtr;
	IMG_UINT32 uIRegNode;
	IMG_UINT32 uAlphaNode, uAlphaChan;

	if (psRegState->uNumIRegsUsed == 0)
		return;

	uIRegNode = psRegState->uIRegNodeReg;
	uAlphaChan = uChansPerReg;
	uAlphaNode = USC_UNDEF;

	/* RGB Stores */
	for (uCtr = 0; uCtr < NUM_INTERNAL_REGISTERS; uCtr++)
	{
		if ((psRegState->uProgIRegsLive & (1U << uCtr)) == 0)
			continue;

		psRegState->apsIRegStore[uCtr].uRGBNode = uIRegNode;
		uIRegNode += 1;
	}

	/* Alpha Stores */
	for (uCtr = 0; uCtr < NUM_INTERNAL_REGISTERS; uCtr++)
	{
		if ((psRegState->uProgIRegsLive & (1U << uCtr)) == 0)
			continue;

		if (!(uAlphaChan < uChansPerReg))
		{
			uAlphaChan = 0;
			uAlphaNode = uIRegNode;
			uIRegNode += 1;
		}

		psRegState->apsIRegStore[uCtr].uAlphaNode = uAlphaNode;
		psRegState->apsIRegStore[uCtr].uAlphaChan = uAlphaChan;

		uAlphaChan += 1;
	}
}

static
IMG_VOID SaveRestoreIRegs(PRALSCAN_STATE psRegState,
						  PCODEBLOCK psBlock,
						  PINST psFirstInst,
						  PINST psPrevInst,
						  PINST psInst,
						  IMG_UINT32 uPreInstIRegMask,
						  IMG_UINT32 uIRegsLive,
						  IMG_UINT32 uIRegsLiveAfter,
						  IMG_UINT32 uIRegsRead,
						  IMG_UINT32 uIRegsWritten)
/*****************************************************************************
 FUNCTION	  : SaveRestoreIRegs

 PURPOSE	  : Save and restore iregisters for memory accesses

 PARAMETERS	  : psRegState        - Register allocator state
                psBlock           - Codeblock
                psFirstInst       - First instruction in code block
                psPrevInst        - Previous instruction in code block
                psNextInst        - Nest instruction in code block
                psInst            - Instruction
                uPreInstIRegMask  -
                uIRegsLive        - Mask of registers live before the instruction
                uIRegsLiveAfter   - MAsk of registers live after the instruction
                uIRegsRead        - Mask of registers read by the instruction
                uIRegsWritten     - Mask of registers read by the instruction

 RETURNS	  : Nothing.
*****************************************************************************/
{
	IMG_UINT32 uActiveMask = 0;

	if ((g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_DESTMASKABLE) != 0 &&
		psInst->auDestMask[0] != USC_DESTMASK_FULL)
	{
		uIRegsRead |= uIRegsWritten;
	}

	/* Save/restore iregisters before instruction */
	if (psRegState->bSpillToMemPre)
	{
		PINST psPoint;

		/* Save iregisters before memory access */
		if (psPrevInst == NULL)
			psPoint = psFirstInst;
		else
			psPoint = GET_NEXT_INST(psPrevInst);

		uActiveMask = uIRegsLive & psRegState->uIRegWriteMask;
		LoadStoreIRegs(psRegState, psBlock, psPoint,
					   uActiveMask,
					   IMG_FALSE);

		/* Restore iregisters before instruction */
		uActiveMask = uIRegsRead & psRegState->uIRegSaveMask;
		uActiveMask &= ~(uPreInstIRegMask | psRegState->uIRegLoadMask);

		LoadStoreIRegs(psRegState, psBlock, psInst,
					   uActiveMask,
					   IMG_TRUE);
	}

	psRegState->uIRegWriteMask |= (uIRegsWritten & uIRegsLiveAfter);
	/* Save/restore iregisters after instruction */
	if (psRegState->bSpillToMemPost)
	{
		/* Save iregisters after instruction (before memory access) */
		uActiveMask = uIRegsLiveAfter & uIRegsWritten;
		LoadStoreIRegs(psRegState, psBlock, psInst->psNext,
					   uActiveMask,
					   IMG_FALSE);
	}
}

typedef struct
{
	IMG_PUINT32 auSpillOp;
	IMG_PUINT32 auOpRenamed;
	IMG_BOOL bRegAllocFail;
	PRALSCAN_STATE psRegState;
} RENAME_STATE, *PRENAME_STATE;

static
IMG_VOID RenameRegistersJITBlock(PINTERMEDIATE_STATE psState,
								 PCODEBLOCK psBlock,
								 IMG_PVOID pvRenameState)
/*****************************************************************************
 FUNCTION	  : RenameRegistersJITBlock

 PURPOSE	  : Per-block rename temoprary registers with their newly assigned colours.

 PARAMETERS	  : psState			  - Compiler state.
                psBlock           - Block to process
                pvRenameState     - PRENAME_STATE to renaming state (inc reg state)

 RETURNS	  : Nothing.
*****************************************************************************/
{
	PRENAME_STATE psRenameState = (PRENAME_STATE)pvRenameState;
	PRALSCAN_STATE psRegState = psRenameState->psRegState;

	/* Iterate backwards through code block */
	PINST psInst, psNextInst, psPrevInst;
	PINST psFirstInst = psBlock->psBody;
	IMG_UINT32 uIRegsLive;
	IMG_UINT32 uIRegsLiveAfter;
	const IMG_UINT32 uOperandSpan = (sizeof(IMG_UINT32) *
									 UINTS_TO_SPAN_BITS(USC_MAX_DEST_COUNT +
														USC_MAXIMUM_ARG_COUNT));
	const IMG_UINT32 uHwRegSpan = sizeof(PINST) * psRegState->sRAData.uAvailRegs;

	if (psRenameState->bRegAllocFail)
	{
		return;
	}

	memset(psRegState->apsHwReg, 0,
		   sizeof(PUSC_REG_INTERVAL) * psRegState->sRAData.uAvailRegs);

	psRegState->uIRegSaveMask = 0;
	psRegState->uIRegWriteMask = 0;
	psRegState->uIRegC10Mask = 0;

	uIRegsLive = 0;
	uIRegsLiveAfter = 0;

	psPrevInst = NULL;
	for (psInst = psFirstInst; psInst != NULL; psInst = psNextInst)
	{
		IMG_UINT32 j, i;
		IMG_BOOL bDead = IMG_FALSE;
		IMG_BOOL bSpilling;
		IMG_BOOL bRunSpillPass;
		IMG_UINT32 uDestCount = psInst->uDestCount;
		IMG_UINT32 uArgCount = g_psInstDesc[psInst->eOpcode].uArgumentCount;
		IMG_UINT32 uIRegsWritten = 0;
		IMG_UINT32 uIRegsRead = 0;
		IMG_UINT32 uPreInstIRegMask = 0;
		IMG_UINT32 uIRegsKilled = 0;

		psNextInst = psInst->psNext;

		/* Reset per-instruction data */
		memset(psRenameState->auSpillOp, 0, uOperandSpan);
		memset(psRenameState->auOpRenamed, 0, uOperandSpan);

		/* Clear settings */
		memset(psRegState->apsRegInst, 0, uHwRegSpan);

		/* Set iregisters live */
		uIRegsKilled = GetIRegsReadMask(psState, psInst, IMG_FALSE, IMG_TRUE);
		psRegState->bSpillToMem = IMG_FALSE;
		psRegState->bSpillToMemPre = IMG_FALSE;
		psRegState->bSpillToMemPost = IMG_FALSE;

		uIRegsWritten = GetIRegsWrittenMask(psState, psInst, IMG_FALSE);
		uIRegsRead = GetIRegsReadMask(psState, psInst, IMG_FALSE, IMG_FALSE);
		if (psInst->uPredSrc != USC_PREDREG_NONE)
		{
			uIRegsRead |= uIRegsWritten;
		}

		uIRegsLiveAfter = uIRegsLive;
		uIRegsLiveAfter &= ~uIRegsKilled;
		uIRegsLiveAfter |= uIRegsWritten;

		/* Rename source operands */
		bSpilling = IMG_FALSE;
		do
		{
			bRunSpillPass = IMG_FALSE;

			for (i = 0; i < uArgCount && !bDead; i++)
			{
				IMG_BOOL bDeadReg = IMG_FALSE;

				if ((bSpilling && !GetBit(psRenameState->auSpillOp, i)) ||
					GetBit(psRenameState->auOpRenamed, i + uDestCount) ||
					!RegIsNode(&psRegState->sRAData, &psInst->asArg[i]))
				{
					/* Not a renamable register */
					continue;
				}

				if (bSpilling)
				{
					if (!SpillRegJIT(psRegState, psBlock, psInst,
									 &psInst->asArg[i], i,
									 IMG_FALSE,
									 psRenameState->auOpRenamed))
					{
						psRenameState->bRegAllocFail = IMG_TRUE;
						goto Exit;
					}
				}
				else
				{
					if (!RenameRegJIT(psRegState, psBlock, psInst,
									  &psInst->asArg[i], i,
									  IMG_FALSE,
									  psRenameState->auOpRenamed,
									  &bDeadReg))
					{
						SetBit(psRenameState->auSpillOp, i, IMG_TRUE);
						bRunSpillPass = IMG_TRUE;
					}
				}
#ifdef DEBUG
				if (psInst->asArg[i].uType == USEASM_REGTYPE_TEMP)
				{
					if (psInst->eOpcode == IFDSX || psInst->eOpcode == IFDSY)
					{
						ASSERT(psInst->asDest[0].uType != psInst->asArg[i].uType ||
							   psInst->asDest[0].uNumber != psInst->asArg[i].uNumber);
					}
				}
#endif
				if (bDeadReg)
				{
					bDead = IMG_TRUE;
					break;
				}
			}
			bSpilling = IMG_TRUE;
		}
		while(bRunSpillPass && !bDead);


		/* Clear settings */
		memset(psRegState->apsRegInst, 0, uHwRegSpan);

		/* Rename destination operands */
		bSpilling = IMG_FALSE;
		do
		{
			bRunSpillPass = IMG_FALSE;

			if (psInst->asDest[0].uType != USEASM_REGTYPE_TEMP ||
				psInst->uDestCount == 0)
			{
				continue;
			}

			if (psBlock->psOwner == psState->psSecAttrProg)
			{
				ASSERT(psInst->eOpcode == IEFO && psInst->u.psEfo->bIgnoreDest);
				psInst->asDest[0].uNumber = 0;

				continue;
			}


			for (j = 0; j < uDestCount; j++)
			{
				IMG_BOOL bDeadReg = IMG_FALSE;

				/* Record whether internal registers have c10 data */
				if (!bSpilling &&
					psInst->asDest[j].uType == USEASM_REGTYPE_FPINTERNAL &&
					psInst->asDest[j].eFmt == UF_REGFORMAT_C10)
				{
					psRegState->uIRegC10Mask |= (1U << psInst->asDest[j].uNumber);
				}
				if ((bSpilling && !GetBit(psRenameState->auSpillOp, j)) ||
					psInst->uDestCount == 0 ||
					GetBit(psRenameState->auOpRenamed, j) ||
					!RegIsNode(&psRegState->sRAData, &psInst->asDest[j]))
				{
					/* Not a renameable register */
					continue;
				}

				if (bSpilling)
				{
					if (!SpillRegJIT(psRegState, psBlock, psInst,
									 &psInst->asDest[j], j,
									 IMG_TRUE,
									 psRenameState->auOpRenamed))
					{
						psRenameState->bRegAllocFail = IMG_TRUE;
						goto Exit;
					}
					else
					{
						ASSERT(psInst->asDest[j].uType == psInst->asDest[0].uType);
						ASSERT(psInst->asDest[j].uNumber == (psInst->asDest[0].uNumber + j));
					}
				}
				else
				{
					if (!RenameRegJIT(psRegState, psBlock, psInst,
									  &psInst->asDest[j], j,
									  IMG_TRUE,
									  psRenameState->auOpRenamed,
									  &bDeadReg))
					{
						SetBit(psRenameState->auSpillOp, j, IMG_TRUE);
						bRunSpillPass = IMG_TRUE;
					}
					else
					{
						ASSERT(psInst->asDest[j].uType == psInst->asDest[0].uType);
						ASSERT(psInst->asDest[j].uNumber == (psInst->asDest[0].uNumber + j));
					}
				}
				if (bDeadReg)
				{
					bDead = bDeadReg;
					break;
				}
			}

			bSpilling = IMG_TRUE;
		}
		while(bRunSpillPass && !bDead);

		/* Remove instructions with source operands which are never written */
		if (bDead ||
			(psInst->eOpcode == IMOV &&
			 psInst->asDest[0].uType == psInst->asArg[0].uType &&
			 psInst->asDest[0].uNumber == psInst->asArg[0].uNumber &&
			 psInst->asDest[0].uIndex == psInst->asArg[0].uIndex))
		{
			RemoveInst(psState, psBlock, psInst);
			FreeInst(psState, psInst);

			continue;
		}

		/* Restore iregisters before instruction */
		uPreInstIRegMask = (uIRegsRead & psRegState->uIRegSaveMask);
		uPreInstIRegMask &= ~(psRegState->uIRegLoadMask);
		LoadStoreIRegs(psRegState, psBlock, psInst,
					   uPreInstIRegMask,
					   IMG_TRUE);

		/* Save/restore iregisters if there is a memory access */
		if (psRegState->bSpillToMem)
		{
			SaveRestoreIRegs(psRegState, psBlock,
							 psFirstInst, psPrevInst,
							 psInst, uPreInstIRegMask,
							 uIRegsLive, uIRegsLiveAfter,
							 uIRegsRead, uIRegsWritten);
		}
		else
		{
			psRegState->uIRegWriteMask |= (uIRegsWritten & uIRegsLiveAfter);
		}

		uIRegsLive = uIRegsLiveAfter;
		uIRegsLiveAfter = 0;
		psPrevInst = psInst;
	}

	/* Restore nodes that were evicted from their colours by spills */
	RestoreBlockColours(psRegState, psBlock);

  Exit:
	UscFree(psState, psRegState->apsIRegStore);
	//if psRenameState->bRegAllocFail, renaming _BP's will abort, and caller will UscFail.
}

static
IMG_VOID PreColourNodes(PINTERMEDIATE_STATE psState,
						PRALSCAN_STATE psRegState,
						IMG_PUINT32 puColourCount)
/*****************************************************************************
 FUNCTION	  : PreColourNodes

 PURPOSE	  : Determine and set the colours for nodes which must have
                specific colours.

 PARAMETERS	  : psState			    - Compiler state.
                psRegState          - Register allocator state

 INPUT/OUTPUT : puColourCount;      - Maximum colour used.


 RETURNS	  : Nothing.
*****************************************************************************/
{
	ARG sArg;
	IMG_UINT32 uColourCount;
	IMG_UINT32 i;
	IMG_UINT32 uNode;
	IMG_UINT32 uMax;
	PREGALLOC_DATA psRAData;

	uColourCount = *puColourCount;
	psRAData = &(psRegState->sRAData);

	/* Precolour primary attributes */
	sArg.uType = USEASM_REGTYPE_PRIMATTR;
	uMax = psRAData->uAvailPARegs;
	for (i = 0; i < uMax; i++)
	{
		sArg.uNumber = i;
		uNode = ArgumentToNode(psRAData, &sArg);

		SetBit(psRegState->auIsFixed, uNode, IMG_TRUE);
		psRegState->auFixedColour[uNode] = uNode;
	}
	uColourCount = max(uColourCount, psRAData->uAvailPARegs);

	/* Precolour output regs */
	sArg.uType = USEASM_REGTYPE_OUTPUT;
	uMax = psRAData->uAvailOutputRegs;
	for (i = 0; i < uMax; i++)
	{
		sArg.uNumber = i;
		uNode = ArgumentToNode(psRAData, &sArg);

		SetBit(psRegState->auIsFixed, uNode, IMG_TRUE);
		psRegState->auFixedColour[uNode] = uNode;
	}
	uColourCount = max(uColourCount, psRAData->uAvailOutputRegs);

	/* Precolour the shader results */
	uMax = psRAData->uNumShaderOutputs;
	for (i = 0; i < uMax; i++)
	{
		IMG_UINT32 uNode = USC_UNDEF;
		IMG_UINT32 uColour = USC_UNDEF_COLOUR;
		PSHADEROUTPUT psOut = &(psRAData->asShaderOutputs[i]);

		if (psOut->sVReg.uType != USEASM_REGTYPE_TEMP)
		{
			/*
			  Only temp.regs need to be coloured (others have already
			  be precoloured)
			*/
			continue;
		}

		if (RegIsNode(psRAData, &psOut->sVReg))
		{
			uNode = ArgumentToNode(psRAData, &(psOut->sVReg));

			/* Test for the special case where a result must go into a PA */
			if (uNode == psRAData->uShaderOutputToPAs)
			{
				/* Increase the number of primary attributes */
				uColour = psState->uPrimaryAttributeRegisterCount;

				psState->uPARegistersUsed += 1;
				psState->uPrimaryAttributeRegisterCount += 1;
			}
			else
			{
				uColour = ArgumentToNode(psRAData, &(psOut->sPReg));
			}

			SetBit(psRegState->auIsFixed, uNode, IMG_TRUE);
			psRegState->auFixedColour[uNode] = uColour;

			uColourCount = max(uColourCount, uColour);
		}
	}

	*puColourCount = uColourCount;
}

static
IMG_VOID CalcIRegStore(PRALSCAN_STATE psRegState,
					   IMG_PUINT32 puNumIRegsUsed,
					   IMG_PUINT32 puNumIRegStores)
/*****************************************************************************
 FUNCTION	  : CalcIRegStore

 PURPOSE	  : Calculate how many f32 registers are needed to store the iregisters

 PARAMETERS	  : psRegState        - Register allocator state

 OUTPUT       : puNumIRegsUsed    - Number of iregisters used
                puNumIRegStores   - Number of 32-bit registers needed to store ireg data

 RETURNS	  : Nothing
*****************************************************************************/
{
	const IMG_UINT32 uChansPerReg = 3;
	IMG_UINT32 uRGBCtr;
	IMG_UINT32 uAlphaCtr, uAlphaChan;
	IMG_UINT32 uCtr;
	IMG_UINT32 uNumIRegsUsed;

	uRGBCtr = 0;
	uAlphaCtr = 0;
	uAlphaChan = 0;

	uNumIRegsUsed = 0;
	for (uCtr = 0; uCtr < NUM_INTERNAL_REGISTERS; uCtr ++)
	{
		if ((psRegState->uProgIRegsLive & (1U << uCtr)) == 0)
			continue;
		uNumIRegsUsed += 1;

		// RGB part
		uRGBCtr += 1;

		// Alpha part
		if ((psRegState->uIRegC10Mask & (1U << uCtr)) != 0)
		{
			uAlphaChan += 1;

			if (!(uAlphaChan < uChansPerReg))
			{
				uAlphaChan = 0;
				uAlphaCtr += 1;
			}
		}
	}


	if (puNumIRegsUsed != NULL)
		(*puNumIRegsUsed) = uNumIRegsUsed;
	if (puNumIRegStores != NULL)
		(*puNumIRegStores) = uRGBCtr + uAlphaCtr;
}

static
IMG_VOID SetForSpilling(PRALSCAN_STATE psRegState)
/*****************************************************************************
 FUNCTION     : SetForSpilling

 PURPOSE      : Change the register state to spill registers

 PARAMETERS   : psRegState     - Register allocator state

 RETURNS      : Nothing.
*****************************************************************************/
{
	const IMG_UINT32 uMinRegs = 16;
	IMG_UINT32 uNumAddrRegs = 0;
	IMG_UINT32 uNumIRegsUsed= 0;
	IMG_UINT32 uNumIRegStores = 0;
	IMG_UINT32 uNumSpillRegisters = 0;

	/*
	  1/ Calculate the number of temporary registers needed
	  for the address register and the internal registers.
	  These are the spill registers

	  2/ Reduce the number of hw registers available to exclude the spill
	  registers.
	*/

	/* Calculate spill registers for iregisters */
	CalcIRegStore(psRegState,
				  &uNumIRegsUsed,
				  &uNumIRegStores);
	uNumSpillRegisters += uNumIRegStores;

	/* Calculate spill registers for address */
	if (psRegState->sRAData.uAvailRegs < uMinRegs)
	{
		/* Don't have a register to calculate the address offset */
		uNumAddrRegs = 0;
	}
	else
	{
		/* Need a register to calculate the address offset */
		uNumAddrRegs = 1;
	}
	uNumSpillRegisters += uNumAddrRegs;


	/* Reduce hw registers available */
	psRegState->uNumHwRegs = psRegState->sRAData.uAvailRegs - uNumSpillRegisters;

	psRegState->uNumIRegsUsed = uNumIRegsUsed;
	psRegState->uNumIRegStores = uNumIRegStores;
	psRegState->uNumAddrRegisters = uNumAddrRegs;
}

#if defined(UF_TESTBENCH)
/*****************************************************************************
 FUNCTION	  : LScanNodeToReg

 PURPOSE	  : Convert a node to a register

 PARAMETERS	  : psMapData   - Register map data
                uNode       - Node

 OUTPUT       : puRegType   - Register type
                puRegNum    - Register number

 RETURNS	  : IMG_TRUE iff node could be converted to a register
*****************************************************************************/
static
IMG_BOOL LScanNodeToReg(PUSC_REGMAP_DATA psMapData,
                        IMG_UINT32 uNode,
						IMG_PUINT32 puRegType,
						IMG_PUINT32 puRegNum)
{
	IMG_UINT32 uRegType = USC_UNDEF;
	IMG_UINT32 uRegNum = USC_UNDEF;

    if (uNode >= psMapData->uNumPARegs)
    {
		uRegType = USEASM_REGTYPE_TEMP;
        uRegNum = uNode - psMapData->uNumPARegs;
    }
    else
    {
		uRegType = USEASM_REGTYPE_PRIMATTR;
        uRegNum = uNode;
    }

	if (puRegType != NULL)
		(*puRegType) = uRegType;
	if (puRegNum != NULL)
		(*puRegNum) = uRegNum;

	return IMG_TRUE;
}


/*****************************************************************************
 FUNCTION	  : PrintNodeSpill

 PURPOSE	  : Print spill for each node

 PARAMETERS	  : psMapData   - Register map data
                uNode       - Node

 OUTPUT       : puRegType   - Register type
                puRegNum    - Register number

 RETURNS	  : IMG_TRUE iff node could be converted to a register
*****************************************************************************/
static
IMG_VOID PrintNodeSpill(PINTERMEDIATE_STATE psState,
						PRALSCAN_STATE psRegState)
{
	IMG_UINT32 uNode = 0;
	IMG_UINT32 uRegNum = USC_UNDEF;
	IMG_PCHAR pszOutput = NULL;

	pszOutput = UscAlloc(psState, sizeof(IMG_CHAR) * 100);

	for (uNode = 0; uNode < psRegState->sRAData.uNrRegisters; uNode++)
	{
		IMG_PCHAR pszBuffer;
		IMG_UINT32 uCtr = 0;
		IMG_PCHAR pszName = NULL;
		IMG_UINT32 uLoc = USC_UNDEF;

		pszBuffer = pszOutput;
		memset(pszBuffer, 0, sizeof(IMG_CHAR) * 100);

		if (GetBit(psRegState->auNodeHasLoc, uNode))
		{
			if (uNode >= psRegState->sRAData.uAvailPARegs)
			{
				pszName = g_pszRegType[USEASM_REGTYPE_TEMP];
				uRegNum = uNode - psRegState->sRAData.uAvailPARegs;
			}
			else
			{
				pszName = g_pszRegType[USEASM_REGTYPE_PRIMATTR];
				uRegNum = uNode - psRegState->sRAData.uAvailPARegs;
			}
			uLoc = psRegState->auNodeMemLoc[uNode];
			uCtr = sprintf(pszBuffer, "%s%u: %lu", pszName, uRegNum, uLoc);
			DBG_PRINTF((DBG_MESSAGE, pszBuffer));
		}
	}

	UscFree(psState, pszOutput);
}
#endif /* defined(UF_TESTBENCH) */

static
IMG_VOID AssignColoursJIT(PINTERMEDIATE_STATE psState,
						  PRALSCAN_STATE psRegState,
						  IMG_PUINT32 puColourCount,
						  IMG_PBOOL pbSpilling)
/*****************************************************************************
 FUNCTION	  : AssignColoursJIT

 PURPOSE	  : Find and assign registers (colours) to variables (nodes) for the
                just-in-time compiler.

 PARAMETERS	  : psState			  - Compiler state.
                psRegState        - Register allocator state

 INPUT/OUTPUT : puColourCount;    - Maximum colour used.


 RETURNS	  : Nothing.
*****************************************************************************/
{
	IMG_UINT32 uMaxColour = 0, uColourCount = 0;
	IMG_BOOL bSpilling = IMG_FALSE;

	/* Assign fixed colour nodes */
	PreColourNodes(psState, psRegState, puColourCount);
	uColourCount = *puColourCount;

	/* Calculate the liveness intervals */
	CalcIntervals(psState, psRegState);

	/* Find the registers that can be coloured. */
	SetForSpilling(psRegState);
	ComputeNodeColours(psState, psRegState,
					   &uMaxColour,
					   &bSpilling);
	uColourCount = max(uColourCount, uMaxColour + 1);

#if defined(UF_TESTBENCH)
	/* Print the register mapping */
	{
		USC_REGMAP_DATA sMapData;
		InitMapData(&sMapData);

		sMapData.uNumPARegs = psRegState->sRAData.uAvailPARegs;
		sMapData.pfnNodeToReg = LScanNodeToReg;

		PrintNodeColours(psState, &sMapData,
						 psRegState->sRAData.uNrRegisters,
						 psRegState->apsRegInterval);
	}
#endif /* defined(UF_TESTBENCH) */

	/* Set the return values */
	*puColourCount = uColourCount;
	*pbSpilling = bSpilling;
}

static
IMG_VOID RenameRegistersJIT(PINTERMEDIATE_STATE psState,
							PRALSCAN_STATE psRegState)
/*****************************************************************************
 FUNCTION	  : RenameRegistersJIT

 PURPOSE	  : Rename temoprary registers with their newly assigned colours.

 PARAMETERS	  : psState			  - Compiler state.
                psRegState        - Register allocator state

 RETURNS	  : Nothing.
*****************************************************************************/
{
	IMG_BOOL bRegAllocFail = IMG_FALSE;
	PRENAME_STATE psRenameState = NULL;
	IMG_UINT32 uSize;
	const IMG_UINT32 uOperandSpan = (sizeof(IMG_UINT32) *
									 UINTS_TO_SPAN_BITS(USC_MAX_DEST_COUNT +
														USC_MAXIMUM_ARG_COUNT));
	const IMG_UINT32 uHwRegSpan = sizeof(PINST) * psRegState->sRAData.uAvailRegs;

	/* Setup block-processing call-back data */
	psRenameState = UscAlloc(psState, sizeof(RENAME_STATE));
	psRenameState->auSpillOp = UscAlloc(psState, uOperandSpan);
	psRenameState->auOpRenamed = UscAlloc(psState, uOperandSpan);

	psRegState->apsRegInst = UscAlloc(psState, uHwRegSpan);

	psRegState->apsHwReg = UscAlloc(psState,
									sizeof(PUSC_REG_INTERVAL) * psRegState->sRAData.uAvailRegs);

	psRegState->auIRegSpill = UscAlloc(psState,
									   sizeof(IMG_UINT32) * NUM_INTERNAL_REGISTERS);
	psRenameState->psRegState = psRegState;

	/* Set up the iregister spill nodes */
	uSize = NUM_INTERNAL_REGISTERS;
	psRegState->apsIRegStore = UscAlloc(psState,
										sizeof(IREG_STORE) * uSize);
	memset(psRegState->apsIRegStore, 0, sizeof(IREG_STORE) * uSize);
	InitIRegStore(psRegState);

	/* Reset data */
	memset(psRegState->apsHwReg, 0,
		   sizeof(PUSC_REG_INTERVAL) * psRegState->sRAData.uAvailRegs);
	memset(psRegState->apsRegInst, 0, uHwRegSpan);

	psRegState->uNextSpillColour = WrapSpillColour(psRegState,
												   psRegState->sRAData.uAvailPARegs);


	/* Iterate through basic blocks, renaming registers */
	psRenameState->bRegAllocFail = IMG_FALSE;
	DoOnAllBasicBlocks(psState, ANY_ORDER,
					   RenameRegistersJITBlock,
					   IMG_FALSE/*CALLs*/, psRenameState);

	if (psRenameState->bRegAllocFail)
	{
		bRegAllocFail = IMG_TRUE;
		goto Exit;
	}

	/* Rename the register arrays */
	if (psState->apsVecArrayReg != NULL)
	{
		IMG_UINT32 uIdx;
		ARG sRegister;
		InitInstArg(&sRegister);

		for (uIdx = 0; uIdx < psState->uNumVecArrayRegs; uIdx ++)
		{
			PUSC_VEC_ARRAY_REG psVecArrayReg = psState->apsVecArrayReg[uIdx];
			IMG_UINT32 uNode;
			IMG_UINT32 uNewNum = USC_UNDEF;
			PUSC_REG_INTERVAL psInterval;

			if (psVecArrayReg == NULL)
				continue;

			sRegister.uType = USEASM_REGTYPE_TEMP;
			sRegister.uNumber = psVecArrayReg->uBaseReg;

			uNode = ArgumentToNode(&psRegState->sRAData, &sRegister);
			psInterval = RegListFindStart(psRegState->apsRegInterval[uNode]);

			if (psInterval == NULL)
				continue;
			if (psInterval->bSpill)
				continue;

			/* Calculate the registers new type and number */
			ASSERT(psInterval->uColour >= psRegState->sRAData.uAvailPARegs);
			GetNodeReg(psRegState, psInterval->uColour, NULL, &uNewNum);

			psVecArrayReg->uBaseReg = uNewNum;
		}
	}


  Exit:
	/* Clean up */
	UscFree(psState, psRenameState->auSpillOp);
	UscFree(psState, psRenameState->auOpRenamed);
	UscFree(psState, psRegState->apsRegInst);
	UscFree(psState, psRegState->apsHwReg);
	UscFree(psState, psRegState->auIRegSpill);
	UscFree(psState, psRegState->apsIRegStore);
	UscFree(psState, psRenameState);

	if (bRegAllocFail)
	{
		UscFail(psState, UF_ERR_GENERIC, "Can't assign registers");
	}
}

static
IMG_VOID AssignProgRegsJIT(PINTERMEDIATE_STATE psState,
						   FUNCGROUP eFuncGroup)
/*****************************************************************************
 FUNCTION	: AssignProgRegsJIT

 PURPOSE	: Assign hardware registers for the instructions in  program.

 PARAMETERS	: psState			- Compiler state.
              eFuncGroup        - Function group

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32 uColourCount = 0;
	IMG_UINT32 uColOutputCount = USC_UNDEF;
	PRALSCAN_STATE psRegState = NULL;
	IMG_BOOL bSpilling = IMG_FALSE;
	IMG_UINT32 uIdx = USC_UNDEF;
	IMG_UINT32 uInitialNumHwRegs = USC_UNDEF;

	if (eFuncGroup == FUNCGROUP_MAIN)
	{
		/* Get the number of temporary registers used for outputs. */
		if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL)
		{
			if (psState->uCompilerFlags & UF_PACKPSOUTPUT)
			{
				uColOutputCount = 0;
				if (psState->psSAOffsets->uPackDestType == USEASM_REGTYPE_TEMP)
				{
					uColOutputCount = 1;
				}
			}
			else
			{
				uColOutputCount = 0;
				if (!(psState->uFlags & USC_FLAGS_OMASKFEEDBACKPRESENT))
				{
					uColOutputCount = psState->uColOutputCount;
				}
				else
				{
					uColOutputCount = (psState->uColOutputCount + 1);
				}
			}
		}
		else if ((psState->psSAOffsets->eShaderType == USC_SHADERTYPE_VERTEX) || (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_GEOMETRY))
		{
			uColOutputCount = 0;
		}
	}
	else
	{
		uColOutputCount = 0;
	}

	/* Assign dummy registers to the register array elements */
	for (uIdx = 0; uIdx < psState->uNumVecArrayRegs; uIdx ++)
	{
		PUSC_VEC_ARRAY_REG psVecArrayReg = psState->apsVecArrayReg[uIdx];

		if (psVecArrayReg == NULL)
			continue;
		/* Assumes that base register is at its default value */
		ASSERT(psVecArrayReg->uBaseReg == 0);

		/* Get a new base register */
		psVecArrayReg->uBaseReg = psState->uNumRegisters;
		psState->uNumRegisters += psVecArrayReg->uRegs;
	}

    /*
	  Allocate memory for register allocation state.
    */
	if (0)
	{
		AllocateRALScanState(psState,
							 (psState->uPARegistersUsed +
							  psState->uNumRegisters +
							  uColOutputCount),
							 &psRegState);
	}
	/*
	  Setup register allocation state.
	*/
	{
		IMG_BOOL bRestart = IMG_FALSE;

		do
		{
			ResetRALScanState(psState,
							  (psState->uPARegistersUsed +
							   psState->uNumRegisters +
							   uColOutputCount),
							  eFuncGroup,
							  &psRegState);

#ifdef DEBUG
			psRegState->uIteration += 1;
#endif /* DEBUG */
			bRestart = IMG_FALSE;

			/* Setup the register allocator state */
			PrepRegStateJIT(psState, psRegState, eFuncGroup, &bRestart);

#ifdef DEBUG
			/* No more than two iterations should be necessary */
			ASSERT (psRegState->uIteration < 3);
#endif /* DEBUG */
		}
		while(bRestart);
	}

	/*
	 * Assign register colour
	 */
	uColourCount = 0;
	bSpilling = IMG_FALSE;
	uInitialNumHwRegs = psRegState->uNumHwRegs;

	AssignColoursJIT(psState,
					 psRegState,
					 &uColourCount,
					 &bSpilling);

	/* Set the address register */
	if (bSpilling)
	{
		/*
		  Should have reserved enough registers to handle spilling in
		  SetForSpilling
		*/
		if (psRegState->uNumIRegStores > 0)
		{
			psRegState->uIRegNodeReg = uColourCount;
			uColourCount += psRegState->uNumIRegStores;

			ASSERT(((psRegState->uIRegNodeReg + (psRegState->uNumIRegStores - 1))) <
				   psRegState->sRAData.uAvailRegs);

		}

		if (psRegState->uNumAddrRegisters > 0)
		{
			psRegState->uAddrReg = uColourCount;
			uColourCount += psRegState->uNumAddrRegisters;

			ASSERT((psRegState->uAddrReg + (psRegState->uNumAddrRegisters - 1)) <
				   psRegState->sRAData.uAvailRegs);
		}
	}
	else
	{
		psRegState->uAddrReg = USC_UNDEF;
		psRegState->uIRegNodeReg = USC_UNDEF;
	}

	/* Rename registers */
	RenameRegistersJIT(psState, psRegState);

	/* Calculate and record the number of temporaries needed */
	{
		IMG_UINT32 uTempRegCount;

		if (uColourCount >= psRegState->sRAData.uAvailPARegs)
			uTempRegCount = (uColourCount - psRegState->sRAData.uAvailPARegs);
		else
			uTempRegCount = 0;


		/* Store the number of temporary registers used */
		if (eFuncGroup == FUNCGROUP_MAIN)
		{
			psState->uTemporaryRegisterCount = max(uTempRegCount,
												   psState->uTemporaryRegisterCount);

		}
		else
		{
			psState->uSecTemporaryRegisterCount = max(uTempRegCount,
													  psState->uSecTemporaryRegisterCount);
		}
	}

#if defined(UF_TESTBENCH)
	{
		/* Print spill addresses */
		DBG_PRINTF((DBG_MESSAGE, "Spill locations"));
		DBG_PRINTF((DBG_MESSAGE, "---------------"));
		PrintNodeSpill(psState, psRegState);
		DBG_PRINTF((DBG_MESSAGE, "------------------------------"));
	}
#endif /* defined(UF_TESTBENCH) */

	/* Free state memory.*/
	FreeRALScanState(psState, &psRegState, IMG_TRUE);
}

static
IMG_VOID AssignRegistersJIT(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: AssignRegistersJIT

 PURPOSE	: Assign hardware registers for the instructions in the program.

 PARAMETERS	: psState			- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
	  Just-In-Time register allocator
	  ===============================

	  Register allocator based on bin-packing: calculate the
	  liveness-intervals for each variable and allocate hardware
	  registers to each interval.

	  Current implementation does not support spilling.

	  Variables handled are primary attribute (PA) and temporary
	  registers (R).

	  Notation:
	  Program variables to be allocated hardware registers are called
	  'variables'.  Hardware registers allocated to program variables
	  are called 'colours'

	  Major data structures
	  ---------------------

	  USC_REG_INTERVAL: Liveness interval

	  - auFlag: Property flags.
	  - uStart: Start of the interval (first full, unconditional write
	  or USC_OPEN_START).
	  - uEnd: End of the interval (last read or USC_OPEN_END).
	  - uColour: Colour assigned to the interval
	  - bSpill: Whether the interval is spilled.
	  - psRegNext/psRegPrev: Ordered list of liveness intervals for
	  the variable.
	  - psProcNext/psProcPrev: Connectors for lists of intervals to be
	  processed.

	  LSCAN_STATE: Register allocator state

	  - apsRegIntervals: Array of interval lists for each
	  register. Each list is ordered by interval start and then by
	  interval end point.
	  - apsColour: Array of interval lists for each colour. Each
	  interval in the list for colour c is assigned colour c. The
	  intervals are kept in order. Used when computing colours.
      - apsHwReg: Array of intervals currently assigned to colours.
	  Use when renaming registers to keep track of what is being
	  assigned to the colours.

	  Phases
	  ------

	  1) CalcIntervals: Calculate liveness intervals for each variable
	  in the program.

	  2) ComputeNodeColours: Allocate registers to liveness
	  intervals. Decide which variables to spill.

	  3) RenameRegisters: Replace variables with their assigned
	  registers throughout the program. Spill variables.

	  Primary attribute registers and some temporary registers have
	  pre-determined colours. These are called 'fixed'
	  registers. Fixed registers pass through the register allocator
	  as normal except that they are always assigned their fixed
	  colour.


	  CalcIntervals
	  -------------

	  Iterate backwards through the program structure creating,
	  extending or closing intervals for each variable found. An
	  interval is closed if it is unconditionally written; an interval
	  is opened if a variable is read but has no open interval; an
	  interval is extended otherwise.

	  As each instruction is processed, it is assigned an index which
	  is stored in field INST.uId. The interval start and end-points
	  are the indices of instructions.

	  All intervals are added to the apsRegInterval array.

	  Intervals for fixed registers are add to the psFixed list. All
	  other intervals are added to list psCurrent when they are
	  closed. The lists psFixed and psCurrent as passed to
	  ComputeNodeColours as the lists of intervals to be processed.
	  Both psFixed and psCurrent are maintained in interval order
	  (earliest start point then latest end-point).

	  Opened intervals are placed on a Pending list to allow them to
	  be processed if they are still open when the iteration through
	  the program completes. Normally, these are given a
	  USC_OPEN_START start point.

	  Intervals on the pending list are marked INTVL_PENDING.

	  Intervals for registers occurring in functions are marked
	  INTVL_FUNCTION.

      Intervals that represent variables which are live into or out of
	  a function are treated as parameters to the function and marked
	  INTVL_PARAM.

	  In some cases, a temporary (R) register may appear in a program
	  without ever being written. These cause failures, are called
	  'dead' and are removed when the registers are renamed.

	  Note that large intervals tie up colours more than small
	  intervals.

	  ComputeNodeColours
	  ------------------

	  Run through the lists of intervals, assigning colours to the
	  registers. First the list psFixed is processed followed by the
	  list of psCurrent. Intervals in the list psFixed get the colour
	  of the fixed register. Intervals in the list psCurrent are
	  assigned colours that are free for the duration of the
	  interval. If no colour is free for the duration of the interval,
	  the interval is spilled.

	  If an interval is assigned a colour c, the interval is added to
	  the list apsColour[c].  The list apsColour[c] is used to
	  determine when a colour is free. The colour assigned to an
	  interval is recorded in interval field uColour.

	  Simple register grouping is supported to allow mandatory
	  register grouping. A group of n registers starting at register r
	  is treated as the single register r requiring n colours. If
	  there are not enough colours, the whole group is spilled. Each
	  colour is assigned to each member of the group for the full life
	  of the group. This is calculated as the interval from the
	  earliest to the latest point in the group. The assignment of
	  colours to group members is recorded in the normal way (in
	  apsColour and field uColour).

	  RenameRegisters
	  ---------------

	  Iterate through the program replacing registers with their
	  colours. The index of each instruction (taken from field uId) is
	  used to identify the liveness intervals for the source and
	  destination registers. If a register is identified as being
	  dead, the whole instruction is removed.
	*/
	IMG_UINT32 uSecAttrTemp = USC_UNDEF;

	/*
	  By default no spill data required.
	*/
	psState->uSpillAreaSize = 0;

	/* Nothing to do if no temporary registers are used. */
	if (psState->uNumRegisters == 0)
	{
		psState->uTemporaryRegisterCount = 0;
		psState->uSecTemporaryRegisterCount = 0;
		return;
	}

	/* Allocate the registers for each of the programs */
	if (psState->psMainProg != NULL)
	{
		AssignProgRegsJIT(psState, FUNCGROUP_MAIN);
	}
	if (psState->psSecAttrProg &&
		psState->psSecAttrProg->uNumBlocks > 0)
	{
//		AssignProgRegsJIT(psState, FUNCGROUP_SECONDARY);
	}

	/* Spill area settings */
	if (psState->uSpillAreaSize > 0)
	{
		SetupSpillArea(psState, &uSecAttrTemp);
	}

	/*
		Adjust the base address of the indexable temp base.
	*/
	if ((psState->uCompilerFlags & UF_EXTRACTCONSTANTCALCS) &&
		(psState->uFlags & USC_FLAGS_INDEXABLETEMPS_USED) &&
		psState->uIndexableTempArraySize > 0)
	{
		AdjustBaseAddress(psState,
						  psState->psSAOffsets->uIndexableTempBase,
						  psState->uIndexableTempArraySize,
						  &uSecAttrTemp);
	}

	/* Check that the number of registers used is valid */
	ASSERT(psState->uTemporaryRegisterCount <= psState->psSAOffsets->uNumAvailableTemporaries);
}
#endif /* defined(REGALLOC_LSCAN) */

static
IMG_VOID ExpandFeedbackDriverEpilog(PINTERMEDIATE_STATE psState, PINST psEpilogInst)
{
	/*
		For some programs which were split into pre- and post-feedback sections we may have an only partially
		defined version of a shader result as an input to the pre-feedback driver epilog with the fully defined
		result an input to the post-feedback epilog. Now insert instructions so the partially and fully defined
		results are the same register number e.g.

			ALU   TEMP1.W <- ...
			FBDRIVEREPILOG(..., TEMP1, ...)

			ALU   TEMP2.RGB[=TEMP1] <- ...
			DRIVEREPILOG(..., TEMP2, ...)

		->
			ALU   TEMP1.W <- ...
			MOV   TEMP3, TEMP1
			FBDRIVEREPILOG(..., TEMP3, ...)

			ALU   TEMP2.RGB[=TEMP3], ...
			DRIVEREPILOG(..., TEMP3, ...)
	*/
	UF_REGFORMAT	eResultFmt;
	IMG_UINT32		uNewShaderResultTemp;
	PFIXED_REG_DATA	psFixedReg;
	IMG_UINT32		uFixedRegOffset;
	PINST			psMOVInst;

	ASSERT(psEpilogInst->psBlock == psState->psPreFeedbackDriverEpilogBlock);

	/*
		Don't generate any hardware instructions for the epilog.
	*/
	SetBit(psEpilogInst->auFlag, INST_NOEMIT, 1);

	if (!psEpilogInst->u.psFeedbackDriverEpilog->bPartial)
	{
		return;
	}

	psFixedReg = psEpilogInst->u.psFeedbackDriverEpilog->psFixedReg;
	uFixedRegOffset = psEpilogInst->u.psFeedbackDriverEpilog->uFixedRegOffset;

	ASSERT(psFixedReg->uVRegType == USEASM_REGTYPE_TEMP);
	ASSERT(psFixedReg->aeUsedForFeedback[uFixedRegOffset] == FEEDBACK_USE_TYPE_BOTH);

	eResultFmt = psEpilogInst->asArg[0].eFmt;

	/*
		Insert a MOV at the end of the post-feedback program to copy from the old shader output to a
		fresh temporary.
	*/
	SpillPrecolouredNode(psState, 
						 psFixedReg->auVRegNum[uFixedRegOffset], 
						 NULL /* puFirstUnspilledIdx */, 
						 IMG_TRUE /* bAlwaysAtEnd */);

	/*
		Get the temporary which will now be precoloured to the shader output.
	*/
	uNewShaderResultTemp = psFixedReg->auVRegNum[uFixedRegOffset];

	/*
		Replace all uses of the source argument in the post-feedback program by the new temporary.
	*/
	if (psEpilogInst->asArg[0].uType == USEASM_REGTYPE_TEMP)
	{
		PUSEDEF_CHAIN	psUseDef;
		PUSC_LIST_ENTRY	psUseListEntry, psNextUseListEntry;

		psUseDef = UseDefGet(psState, USEASM_REGTYPE_TEMP, psEpilogInst->asArg[0].uNumber);
		for (psUseListEntry = psUseDef->sList.psHead; psUseListEntry != NULL; psUseListEntry = psNextUseListEntry)
		{
			PUSEDEF	psUse;

			psNextUseListEntry = psUseListEntry->psNext;
			psUse = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);
			if (psUse == psUseDef->psDef)
			{
				continue;
			}
			if (psUse->eType == USE_TYPE_SRC || psUse->eType == USE_TYPE_OLDDEST)
			{
				PINST		psUseInst = psUse->u.psInst;
				PCODEBLOCK	psUseBlock = psUseInst->psBlock;

				if (
						psState->psPreFeedbackDriverEpilogBlock != psUseBlock && 
						Dominates(psState, psState->psPreFeedbackDriverEpilogBlock, psUseBlock)
					)
				{
					if (psUse->eType == USE_TYPE_SRC)
					{
						SetSrc(psState, 
							   psUseInst, 
							   psUse->uLocation, 
							   USEASM_REGTYPE_TEMP, 
							   uNewShaderResultTemp, 
							   eResultFmt);
					}
					else
					{
						SetPartialDest(psState, 
									   psUseInst, 
									   psUse->uLocation, 
									   USEASM_REGTYPE_TEMP, 
									   uNewShaderResultTemp, 
									   eResultFmt);
					}
				}
			}
		}
	}

	/*
		Insert a MOV at the end of the pre-feedback program to copy from the source argument to the
		current instruction to the new temporary.
	*/
	psMOVInst = AllocateInst(psState, NULL);
	SetOpcode(psState, psMOVInst, IMOV);
	SetDest(psState, psMOVInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uNewShaderResultTemp, eResultFmt);
	MoveSrc(psState, psMOVInst, 0 /* uMoveToSrcIdx */, psEpilogInst, 0 /* uMoveFromSrcIdx */);
	InsertInstBefore(psState, psState->psPreFeedbackBlock, psMOVInst, NULL /* psInsertBeforeInst */);

	SetSrc(psState, psEpilogInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uNewShaderResultTemp, eResultFmt);
}

static
IMG_VOID PreRegisterAllocationExpansions(PINTERMEDIATE_STATE psState)
{
	/*
		Relax restrictions on defining a register only once.
	*/
	psState->uFlags2 &= ~USC_FLAGS2_SSA_FORM;

	if (TestInstructionUsage(psState, ICALL))
	{
		/*
			Expand call instructions to add explict copies to/from function inputs/outputs.
		*/
		ExpandCallInstructions(psState);
		TESTONLY_PRINT_INTERMEDIATE("After expanding calls", "expandcalls", IMG_FALSE);
	}

	ForAllInstructionsOfType(psState, IFEEDBACKDRIVEREPILOG, ExpandFeedbackDriverEpilog);

	/*
		Expand instructions partially/conditionally updating a destination.
	*/
	DoOnAllBasicBlocks(psState, ANY_ORDER, ExpandMasksBP, IMG_FALSE, NULL /* pvUserData */);
	TESTONLY_PRINT_INTERMEDIATE("After expanding masks", "expandmask", IMG_FALSE);

	if (psState->uCompilerFlags & UF_OPENCL)
	{
		/*
			Expand store instructions which update a base address.
		*/
		DoOnAllBasicBlocks(psState, ANY_ORDER, ExpandStoresBP, IMG_FALSE, NULL /* pvUserData */);
		TESTONLY_PRINT_INTERMEDIATE("After expanding stores", "expandstore", IMG_FALSE);
	}
	
	if (TestInstructionUsage(psState, IDELTA))
	{
		/*
			Expand delta instructions.
		*/
		ExpandDeltaInstructions(psState);
		TESTONLY_PRINT_INTERMEDIATE("After delta instructions", "expanddelta", IMG_FALSE);
	}
}

/*
 * Toplevel register assignment function
 */

IMG_INTERNAL
IMG_VOID AssignRegisters(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: AssignRegisters

 PURPOSE	: Assign hardware registers for the instructions in the program.

 PARAMETERS	: psState			- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PreRegisterAllocationExpansions(psState);

	#if defined (EXECPRED)
	if (((psState->uCompilerFlags2 & UF_FLAGS2_EXECPRED) == 0) || ((psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_INTEGER_CONDITIONALS) == 0)  || (psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_29643))
	#endif /*#if defined (EXECPRED)*/
	{
		if ((psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL) && (psState->uNumDynamicBranches > 0))
		{
			SetupSyncStartInformation(psState);
			TESTONLY_PRINT_INTERMEDIATE("Set up syncstart information", "syncstart", IMG_FALSE);
		}
	}

	if(psState->uOptimizationHint & USC_COMPILE_HINT_SWITCH_USED)
	{
		DoOnAllBasicBlocks(psState, ANY_ORDER, ConvertSwitchBP, IMG_FALSE, NULL);
		MergeAllBasicBlocks(psState);
		TESTONLY_PRINT_INTERMEDIATE("Convert Switches", "convswitch", IMG_FALSE);
	}

	#if defined (EXECPRED)	
	if ((psState->uCompilerFlags2 & UF_FLAGS2_EXECPRED) && (psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_INTEGER_CONDITIONALS)  && !(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_29643))
	{
		{
			DeadCodeElimination(psState, IMG_TRUE /* bFreeBlocks */);
			TESTONLY_PRINT_INTERMEDIATE("Dead Code Elimination (Before Execution Predication)", "dce", IMG_FALSE);
		}
		{
			PRAGCOL_STATE psRegState;
			//IMG_BOOL bRestart;
			IMG_UINT32 uPass;
			//IMG_BOOL bSpilled;
			//IMG_BOOL bFormOptRegGroup = IMG_TRUE;
			//IMG_BOOL bAddedExtraPrimaryAttribute = IMG_FALSE;
			
			/*
			  Flag that we need to allocate state for information about registers on
			  the first pass.
			*/
			psRegState = NULL;

			/*
			 * Graph colouring
			 */
			uPass = 0;			
			{
				/*
				  Reset register allocation state.
				*/
				ResetRAGColState(psState, &psRegState, FUNCGROUP_MAIN);
				psRegState->psMOVToPCKList = NULL;
				psRegState->psSpillState = NULL;

				/* Setup the register allocator state */
				PrepRegState(psState, psRegState, FUNCGROUP_MAIN);
				
				/*
				  Try to coalesce registers to get rid of MOV instructions.
				*/
				if (uPass == 0)
				{
					for (;;)
					{
						COALESCING_ARG sCoalesceArg;
						sCoalesceArg.psRegState = psRegState;

						DoOnAllFuncGroupBasicBlocks(psState,
							ANY_ORDER,
							CoalesceRegistersForMOVsBP,
							IMG_FALSE,
							&sCoalesceArg,
							FUNCGROUP_MAIN);

						TESTONLY_PRINT_INTERMEDIATE("After coalescing", "coalesce", IMG_TRUE);

						if (sCoalesceArg.bRemovedInst)
						{
							/* Reset register allocation state. */
							ResetRAGColState(psState, &psRegState, FUNCGROUP_MAIN);
							PrepRegState(psState, psRegState, FUNCGROUP_MAIN);
						}
						else
						{
							break;
						}
					}
				}
				FreeRAGColState(psState, &psRegState, IMG_TRUE);
			}
		}
		MergeAllBasicBlocks(psState);
		TESTONLY_PRINT_INTERMEDIATE("Register Coalesceing (Before Execution Predication)", "coalesce", IMG_FALSE);
		ChangeProgramStructToExecPred(psState);
		TESTONLY_PRINT_INTERMEDIATE("After changing program structure to Execution Predication", "execpred", IMG_FALSE);
		MergeAllBasicBlocks(psState);
	}
	#endif /* #if defined(EXECPRED) */
	
	DeadCodeElimination(psState, IMG_TRUE /* bFreeBlocks */);
	TESTONLY_PRINT_INTERMEDIATE("Dead Code Elimination (Before Register Allocation)", "dce", IMG_FALSE);

	/*
		Assign hardware registers for predicate registers.
	*/
	AssignPredicateRegisters(psState);
	TESTONLY_PRINT_INTERMEDIATE("Predicate Register Allocation", "pregalloc", IMG_FALSE);

	/*
		Setup the 'skip-invalid' execution-flag for all instructions
	*/
	SetupSkipInvalidFlag(psState);
	TESTONLY_PRINT_INTERMEDIATE("Skip Invalid", "skipinv", IMG_FALSE);

#if defined(UF_TESTBENCH) && defined(REGALLOC_GCOL) && defined(REGALLOC_LSCAN)
	if (g_uOnlineOption == 0)
	{
		/* Offline compiler, choose graph-colouring */
		AssignRegistersOffLine(psState);
		TESTONLY(DBG_PRINTF((DBG_MESSAGE, "------- Register Allocation (graph-colouring) --------\n")));
	}
	else
	{
		/* Online compiler, choose linear-scan */
		TESTONLY(DBG_PRINTF((DBG_MESSAGE, "------- Register Allocation (linear-scan) --------\n")));
		AssignRegistersJIT(psState);
	}
#else
#if defined(REGALLOC_GCOL)
	/*
	 * Off-Line compiler
	 */
	AssignRegistersOffLine(psState);
	TESTONLY(DBG_PRINTF((DBG_MESSAGE, "------- Register Allocation (graph-colouring) --------\n")));
#else
#if defined(REGALLOC_LSCAN)
	/*
	 * Just-In-Time compiler
	 */
	TESTONLY(DBG_PRINTF((DBG_MESSAGE, "------- Register Allocation (linear-scan) --------\n")));
	AssignRegistersJIT(psState);
#endif /* defined(REGALLOC_LSCAN) */
#endif /* defined(REGALLOC_GCOL) */
#endif /* defined(UF_TESTBENCH) && defined(REGALLOC_GCOL) && defined(REGALLOC_LSCAN) */
}

/****************************************
 * FILE END
 ****************************************/

