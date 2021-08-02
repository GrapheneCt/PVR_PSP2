/******************************************************************************
 Name           : USPSHADER.C

 Title          : Internal USP shader creation

 C Author       : Joe Molleson

 Created        : 02/01/2002

 Copyright      : 2002-2009 by Imagination Technologies Limited.
                : All rights reserved. No part of this software, either
                : material or conceptual may be copied or distributed,
                : transmitted, transcribed, stored in a retrieval system or
                : translated into any human or computer language in any form
                : by any means, electronic, mechanical, manual or otherwise,
                : or disclosed to third parties without the express written
                : permission of Imagination Technologies Limited,
                : Home Park Estate, Kings Langley, Hertfordshire,
                : WD4 8LZ, U.K.

 Description    : Management of USP internal shaders created from precompiled
                  shader data.

 Program Type   : 32-bit DLL

 Version        : $Revision: 1.98 $

 Modifications  :
 $Log: uspshader.c $
******************************************************************************/
#include <string.h>
#include <memory.h>
#include "img_types.h"
#include "sgxdefs.h"
#include "usp_typedefs.h"
#include "usc.h"
#include "usp.h"
#include "hwinst.h"
#include "uspbin.h"
#include "uspshrd.h"
#include "usp_instblock.h"
#include "usp_resultref.h"
#include "usp_sample.h"
#include "usp_inputdata.h"
#include "uspshader.h"

static const USP_MOESTATE g_sDefaultUSPMOEState =
{
	{ IMG_FALSE, IMG_FALSE, IMG_FALSE , IMG_FALSE },	/* abUseSwiz	*/
	{ 1, 1, 1, 1 },										/* aiInc		*/
	{ 0, 0, 0, 0 },										/* auSwiz		*/
	{ 0, 0, 0, 0 },										/* auBaseOffset	*/
	IMG_FALSE,											/* bEFOFmtCtl	*/
	IMG_FALSE,											/* bColFmtCtl	*/
};

/*
	Collected functions for reading precompiled shader-data of various
	types
*/
typedef IMG_VOID (*PFN_PC_DATA_READER)(IMG_PVOID* pvPCData, IMG_PVOID pvData);

struct _PC_DATA_READERS_
{
	PFN_PC_DATA_READER	pfnReadUINT32;	
	PFN_PC_DATA_READER	pfnReadUINT16;	
};

/*
	Collected functions for writing data of various types to 
	pre-compiled shader
*/
typedef IMG_VOID (*PFN_PC_DATA_WRITER_UINT32)(IMG_PVOID*        pvPCData, 
											  const IMG_UINT32  uValue);
typedef IMG_VOID (*PFN_PC_DATA_WRITER_UINT16)(IMG_PVOID*        pvPCData, 
											  const IMG_UINT16  uValue);

struct _PC_DATA_WRITERS_
{
	PFN_PC_DATA_WRITER_UINT32			 pfnWriteUINT32;	
	PFN_PC_DATA_WRITER_UINT16			 pfnWriteUINT16;
};

typedef struct _PC_DATA_READERS_ PC_DATA_READERS;
typedef struct _PC_DATA_READERS_ const *PPC_DATA_READERS;

typedef struct _PC_DATA_WRITERS_ PC_DATA_WRITERS;
typedef struct _PC_DATA_WRITERS_ const *PPC_DATA_WRITERS;

/*
	Individual routines for reading PC shader data
*/
static IMG_VOID ReadUINT32Straight(IMG_PVOID* pvPCData, IMG_PVOID pvData);
static IMG_VOID ReadUINT32Swapped(IMG_PVOID* pvPCData, IMG_PVOID pvData);
static IMG_VOID ReadUINT16Straight(IMG_PVOID* pvPCData, IMG_PVOID pvData);
static IMG_VOID ReadUINT16Swapped(IMG_PVOID* pvPCData, IMG_PVOID pvData);

/*
	Individual routines for writing data to PC shader
*/
static IMG_VOID WriteUINT32Straight(IMG_PVOID* pvPCData, const IMG_UINT32 uValue);
static IMG_VOID WriteUINT16Straight(IMG_PVOID* pvPCData, const IMG_UINT16 uValue);

/*
	Individual routines to calulate data size for later writing.
*/
static IMG_VOID WriteAddSizeUINT32(IMG_PVOID* ppvPCData, const IMG_UINT32 uValue);
static IMG_VOID WriteAddSizeUINT16(IMG_PVOID* ppvPCData, const IMG_UINT16 uValue);

/*
	Routine for converting between precompiled and internal usp flags.
 */
static IMG_UINT32 InitUSPInstDescFlags(IMG_UINT32 uPCInstDescFlags);

/*
	The set of data-readers for unswapped reading
*/
static const PC_DATA_READERS g_sStraightDataReaders = 
{
	ReadUINT32Straight,
	ReadUINT16Straight,
};

/*
	The set of data-writers for unswapped writing
*/
static const PC_DATA_WRITERS g_sStraightDataWriters = 
{
	WriteUINT32Straight,
	WriteUINT16Straight,
};

/*
	The set of data size calculators.
*/
static const PC_DATA_WRITERS g_sDataSizeCounters = 
{
	WriteAddSizeUINT32,
	WriteAddSizeUINT16,
};

/*
	The set of data-readers for byte-swapped reading
*/
static const PC_DATA_READERS g_sSwappedDataReaders =
{
	ReadUINT32Swapped,
	ReadUINT16Swapped,
};

/*****************************************************************************
 Name:		USPProgDescCreate

 Purpose:	Create and initialise an USP program-description

 Inputs:	psContext			- The current USP context
			uBRNCount			- The number of BRNs used within the pre-
								  compiled code
			uPSInputCount		- The number of iteration or pre-sampled
								  textures
			uMemConstCount		- The number of remapped memory constants
			uRegConstCount		- The number of remapped register constants
			uRegTexStateCount	- The number of sets of texture-state words
			uSAUpdateInstCount	- The number of instructions for the 
								  pre-compiled SA update program
			uShaderOutputsCount	- Count of Shader Outputs from 
								  Compiler.

 Outputs:	none

 Returns:	The created USP program-descriptor
*****************************************************************************/
static PUSP_PROGDESC USPProgDescCreate(PUSP_CONTEXT	psContext,
									   IMG_UINT32	uBRNCount,
									   IMG_UINT32	uPSInputCount,
									   IMG_UINT32	uMemConstCount,
									   IMG_UINT32	uRegConstCount,
									   IMG_UINT32	uRegTexStateCount,
									   IMG_UINT32	uSAUpdateInstCount, 
									   IMG_UINT32	uShaderOutputsCount)
{
	PUSP_PROGDESC			psProgDesc;
	IMG_PUINT32				puBRNList;
	PUSP_PC_PSINPUT_LOAD	psPSInputLoads;
	PUSP_PC_CONST_LOAD		psMemConstLoads;
	PUSP_PC_CONST_LOAD		psRegConstLoads;
	PUSP_PC_TEXSTATE_LOAD	psRegTexStateLoads;
	PHW_INST				psSAUpdateInsts;
	IMG_UINT32*				puValidShaderOutputs;
	IMG_UINT32				uAllocSize;
	IMG_BOOL				bSuccess;

	bSuccess			= IMG_FALSE;
	psProgDesc			= IMG_NULL;
	puBRNList			= IMG_NULL;
	psPSInputLoads		= IMG_NULL;
	psMemConstLoads		= IMG_NULL;
	psRegConstLoads		= IMG_NULL;
	psRegTexStateLoads	= IMG_NULL;
	psSAUpdateInsts		= IMG_NULL;
	puValidShaderOutputs
						= IMG_NULL;

	/*
		Allocate space for the list of BRNs used within the pre-compiled code
	*/
	if	(uBRNCount > 0)
	{
		uAllocSize = uBRNCount * sizeof(IMG_UINT32);
		puBRNList = (IMG_PUINT32)psContext->pfnAlloc(uAllocSize);
		if	(!puBRNList)
		{
			USP_DBGPRINT(( "USPProgDescCreate: Failed to alloc BRN list\n"));
			goto USPProgDescCreateExit;
		}
	}

	/*
		Allocate space for the list of non-dependent samples and iterations
	*/
	if	(uPSInputCount > 0)
	{
		uAllocSize = uPSInputCount * sizeof(USP_PC_PSINPUT_LOAD);
		psPSInputLoads = (PUSP_PC_PSINPUT_LOAD)psContext->pfnAlloc(uAllocSize);
		if	(!psPSInputLoads)
		{
			USP_DBGPRINT(( "USPProgDescCreate: Failed to alloc iteration/pre-sampled texture list\n"));
			goto USPProgDescCreateExit;
		}
	}

	/*
		Allocate space for the list of remapped constants in memory
	*/
	if	(uMemConstCount > 0)
	{
		uAllocSize = uMemConstCount * sizeof(USP_PC_CONST_LOAD);
		psMemConstLoads = (PUSP_PC_CONST_LOAD)psContext->pfnAlloc(uAllocSize);
		if	(!psMemConstLoads)
		{
			USP_DBGPRINT(( "USPProgDescCreate: Failed to alloc mem-const load list\n"));
			goto USPProgDescCreateExit;
		}
	}

	/*
		Allocate space for the list of remapped constants in registers
	*/
	if	(uRegConstCount > 0)
	{
		uAllocSize = uRegConstCount * sizeof(USP_PC_CONST_LOAD);
		psRegConstLoads = (PUSP_PC_CONST_LOAD)psContext->pfnAlloc(uAllocSize);
		if	(!psRegConstLoads)
		{
			USP_DBGPRINT(( "USPProgDescCreate: Failed to alloc reg-const load list\n"));
			goto USPProgDescCreateExit;
		}
	}

	/*
		Allocate space for the list of texture-state words in registers
	*/
	if	(uRegTexStateCount > 0)
	{
		uAllocSize = uRegTexStateCount * sizeof(USP_PC_TEXSTATE_LOAD);
		psRegTexStateLoads = (PUSP_PC_TEXSTATE_LOAD)psContext->pfnAlloc(uAllocSize);
		if	(!psRegTexStateLoads)
		{
			USP_DBGPRINT(("USPProgDescCreate: Failed to alloc reg tex-state load list\n"));
			goto USPProgDescCreateExit;
		}
	}

	/*
		Allocate space for the SA update program
	*/
	if	(uSAUpdateInstCount > 0)
	{
		uAllocSize = uSAUpdateInstCount * sizeof(HW_INST);
		psSAUpdateInsts = (PHW_INST)psContext->pfnAlloc(uAllocSize);
		if	(!psSAUpdateInsts)
		{
			USP_DBGPRINT(("USPProgDescCreate: Failed to alloc SA-update inst list\n"));
			goto USPProgDescCreateExit;
		}
	}

	if(uShaderOutputsCount > 0)
	{
		uAllocSize = (uShaderOutputsCount+((sizeof(IMG_UINT32)*8)-1))/(sizeof(IMG_UINT32)*8);
		uAllocSize = uAllocSize * sizeof(IMG_UINT32);
		puValidShaderOutputs = (IMG_UINT32*)psContext->pfnAlloc(uAllocSize);
		if	(!puValidShaderOutputs)
		{
			USP_DBGPRINT(("USPProgDescCreate: Failed to alloc memory for Valid Shader Outputs Mask\n"));
			goto USPProgDescCreateExit;
		}
	}

	/*
		Allocate and initialise the top-level program descriptor
	*/
	psProgDesc = (PUSP_PROGDESC)psContext->pfnAlloc(sizeof(USP_PROGDESC));
	if	(!psProgDesc)
	{
		USP_DBGPRINT(( "USPProgDescCreate: Failed to alloc USP program descriptor\n"));
		goto USPProgDescCreateExit;
	}

	memset(psProgDesc, 0, sizeof(USP_PROGDESC));

	psProgDesc->puBRNList				= puBRNList;
	psProgDesc->psPSInputLoads			= psPSInputLoads;
	psProgDesc->psMemConstLoads			= psMemConstLoads;
	psProgDesc->psRegConstLoads			= psRegConstLoads;
	psProgDesc->psRegTexStateLoads		= psRegTexStateLoads;
	psProgDesc->psSAUpdateInsts			= psSAUpdateInsts;
	psProgDesc->puValidShaderOutputs	= puValidShaderOutputs;

	/*
		program-descriptor created OK
	*/
	bSuccess = IMG_TRUE;

	USPProgDescCreateExit:

	/*
		Cleanup on error
	*/
	if	(!bSuccess)
	{
		if	(puBRNList)
		{
			psContext->pfnFree(puBRNList);
		}
		if	(psPSInputLoads)
		{
			psContext->pfnFree(psPSInputLoads);
		}
		if	(psMemConstLoads)
		{
			psContext->pfnFree(psMemConstLoads);
		}
		if	(psRegConstLoads)
		{
			psContext->pfnFree(psRegConstLoads);
		}
		if	(psRegTexStateLoads)
		{
			psContext->pfnFree(psRegTexStateLoads);
		}
		if	(psSAUpdateInsts)
		{
			psContext->pfnFree(psSAUpdateInsts);
		}
		if	(puValidShaderOutputs)
		{
			psContext->pfnFree(puValidShaderOutputs);
		}
		if	(psProgDesc)
		{
			psContext->pfnFree(psProgDesc);
			psProgDesc = IMG_NULL;
		}
	}

	return psProgDesc;
}

/*****************************************************************************
 Name:		USPProgDescDestroy

 Purpose:	Destroy USP program description data previously created using
			USPProgDescCreate

 Inputs:	psProgDesc	- The USP program descriuption data to destroy
			psContext	- The current USP context

 Outputs:	none

 Returns:	The created USP program-descriptor
*****************************************************************************/
static IMG_VOID USPProgDescDestroy(PUSP_PROGDESC	psProgDesc,
								   PUSP_CONTEXT		psContext)
{
	if	(psProgDesc->puBRNList)
	{
		psContext->pfnFree(psProgDesc->puBRNList);
	}
	if	(psProgDesc->psPSInputLoads)
	{
		psContext->pfnFree(psProgDesc->psPSInputLoads);
	}
	if	(psProgDesc->psMemConstLoads)
	{
		psContext->pfnFree(psProgDesc->psMemConstLoads);
	}
	if	(psProgDesc->psRegConstLoads)
	{
		psContext->pfnFree(psProgDesc->psRegConstLoads);
	}
	if	(psProgDesc->psRegTexStateLoads)
	{
		psContext->pfnFree(psProgDesc->psRegTexStateLoads);
	}
	if	(psProgDesc->psSAUpdateInsts)
	{
		psContext->pfnFree(psProgDesc->psSAUpdateInsts);
	}
	if	(psProgDesc->puValidShaderOutputs)
	{
		psContext->pfnFree(psProgDesc->puValidShaderOutputs);
	}
	psContext->pfnFree(psProgDesc);	
}

/*****************************************************************************
 Name:		USPBranchCreate

 Purpose:	Create and initialise USP branch info

 Inputs:	psContext	- The current USP context
			uLabelID	- ID of the label this branch will jump to
			psShader	- The shader that the branch is for

 Outputs:	none

 Returns:	The created USP branch data
*****************************************************************************/
static PUSP_BRANCH USPBranchCreate(PUSP_CONTEXT	psContext,
								   IMG_UINT32	uLabelID,
								   PUSP_SHADER	psShader)
{
	PUSP_BRANCH		psBranch;
	PUSP_INSTBLOCK	psInstBlock;
	IMG_BOOL		bFailed;

	/*
		Branch not created yet
	*/
	bFailed		= IMG_TRUE;
	psBranch	= IMG_NULL;
	psInstBlock	= IMG_NULL;

	/*
		Allocate a block of instructions to contain the HW branch instruction
	*/
	psInstBlock = USPInstBlockCreate(psContext, 
									 psShader, 
									 1, 
									 1, 
									 IMG_FALSE,
									 IMG_NULL);
	if	(!psInstBlock)
	{
		USP_DBGPRINT(("USPBranchCreate: Failed to alloc insts\n"));
		goto USPBranchCreateExit;
	}

	/*
		Allocate and initialise the branch data
	*/
	psBranch = (PUSP_BRANCH)psContext->pfnAlloc(sizeof(USP_BRANCH));
	if	(!psBranch)
	{
		USP_DBGPRINT(("USPBranchCreate: Failed to alloc USP branch data\n"));
		goto USPBranchCreateExit;
	}
	memset(psBranch, 0, sizeof(USP_BRANCH));

	psBranch->psInstBlock	= psInstBlock;
	psBranch->uLabelID		= uLabelID;

	/*
		Branch created OK
	*/
	bFailed = IMG_FALSE;

	USPBranchCreateExit:

	/*
		Cleanup on failure
	*/
	if	(bFailed)
	{
		if	(psInstBlock)
		{
			USPInstBlockDestroy(psInstBlock, psContext);
		}
		if	(psBranch)
		{
			psContext->pfnFree(psBranch);
			psBranch = IMG_NULL;
		}
	}

	return psBranch;
}

/*****************************************************************************
 Name:		USPBranchDestroy

 Purpose:	Destroy USP branch data previously created using USPBranchCreate

 Inputs:	psBranch	- The USP branch data to be destroyed
			psContext	- The current USP context

 Outputs:	none

 Returns:	none
*****************************************************************************/
static IMG_VOID USPBranchDestroy(PUSP_BRANCH	psBranch,
								 PUSP_CONTEXT	psContext)
{
	/*
		We only destroy the branch info, not the associated instruction block.
		This is destroyed along with all the others for the shader.
	*/
	psContext->pfnFree(psBranch);
}

/*****************************************************************************
 Name:		USPLabelCreate

 Purpose:	Create and initialise USP label info

 Inputs:	psContext	- The current USP context
			uLabelID	- The unique ID for the new label

 Outputs:	none

 Returns:	The created USP label data
*****************************************************************************/
static PUSP_LABEL USPLabelCreate(PUSP_CONTEXT psContext, IMG_UINT32 uLabelID)
{
	PUSP_LABEL	psLabel;

	/*
		Allocate and initialise the label data
	*/
	psLabel = (PUSP_LABEL)psContext->pfnAlloc(sizeof(USP_LABEL));
	if	(!psLabel)
	{
		USP_DBGPRINT(( "USPLabelCreate: Failed to alloc USP label data\n"));
		return IMG_NULL;
	}
	memset(psLabel, 0, sizeof(USP_LABEL));

	psLabel->uID = uLabelID;

	return psLabel;
}

/*****************************************************************************
 Name:		USPLabelDestroy

 Purpose:	Destroy USP label data previously created using USPLabelCreate

 Inputs:	psLabel		- The USP label data to be destroyed
			psContext	- The current USP context

 Outputs:	none

 Returns:	none
*****************************************************************************/
static IMG_VOID USPLabelDestroy(PUSP_LABEL psLabel, PUSP_CONTEXT psContext)
{
	/*
		We don't destroy the instructions associated with the label. These
		are cleaned-up along with all the other instructions blocks that
		form the shader.
	*/
	psContext->pfnFree(psLabel);
}

/*****************************************************************************
 FUNCTION	: GetUSPHWRegFormat

 PURPOSE	: Returns the corresponding USP HW reg format for a given
 			  USP PC reg format

 PARAMETERS	: ePCFmt 	- The USP PC register format

 RETURNS	: The USP HW register format
*****************************************************************************/
static USP_REGFMT GetUSPHWRegFormat(USP_PC_SAMPLE_DEST_REGFMT ePCFmt)
{
	switch(ePCFmt)
	{
		case 	USP_PC_SAMPLE_DEST_REGFMT_F32: return USP_REGFMT_F32;
		case	USP_PC_SAMPLE_DEST_REGFMT_F16: return USP_REGFMT_F16;
		case 	USP_PC_SAMPLE_DEST_REGFMT_C10: return USP_REGFMT_C10;
		case	USP_PC_SAMPLE_DEST_REGFMT_U8:  return USP_REGFMT_U8;
		case	USP_PC_SAMPLE_DEST_REGFMT_U16: return USP_REGFMT_U16;
		case	USP_PC_SAMPLE_DEST_REGFMT_I16: return USP_REGFMT_I16;
		case	USP_PC_SAMPLE_DEST_REGFMT_U32: return USP_REGFMT_U32;
		case	USP_PC_SAMPLE_DEST_REGFMT_I32: return USP_REGFMT_I32;
		default:							   return USP_REGFMT_UNKNOWN;
	}
}

/*****************************************************************************
 FUNCTION	: GetUSPHWRegType

 PURPOSE	: Returns the corresponding USP HW reg type for a given
 			  USP PC reg type

 PARAMETERS	: ePCType 	- The USP PC register type

 RETURNS	: The USP HW register type
*****************************************************************************/
static USP_REGTYPE GetUSPHWRegType(PUSP_CONTEXT					psContext,
								   USP_PC_TEXTURE_WRITE_REGTYPE ePCType)
{
	PVR_UNREFERENCED_PARAMETER(psContext);

	switch(ePCType)
	{
	    case 	USP_PC_TEXTURE_WRITE_REGTYPE_TEMP:	return USP_REGTYPE_TEMP;
	    case	USP_PC_TEXTURE_WRITE_REGTYPE_PA:	return USP_REGTYPE_PA;
	    case 	USP_PC_TEXTURE_WRITE_REGTYPE_SA:	return USP_REGTYPE_SA;
	    case 	USP_PC_TEXTURE_WRITE_REGTYPE_SPECIAL:	return USP_REGTYPE_SPECIAL;
	    case    USP_PC_TEXTURE_WRITE_REGTYPE_INTERNAL:  return USP_REGTYPE_INTERNAL;
	    default: USP_DBGPRINT(("Invalid register type for image write")); return -1;
	}
}

/*****************************************************************************
 Name:		USPTextureWriteCreate

 Purpose:	Create and initialise USP texture write info

 Inputs:	psContext	- The current USP context
			psPCTextureWrite - The pre-compiled texture write block

 Outputs:	none

 Returns:	The created USP texture write data
*****************************************************************************/
static PUSP_TEXTURE_WRITE USPTextureWriteCreate(PUSP_CONTEXT 				psContext,
												PUSP_PC_BLOCK_TEXTURE_WRITE psPCTextureWrite,
												PUSP_SHADER  				psShader)
{
	PUSP_TEXTURE_WRITE 	psTextureWrite;
	PUSP_INSTBLOCK 		psInstBlock;
	USP_REGFMT 			eChannelRegFmt;

	/*
	    Allocate instruction block
	 */
	psInstBlock = USPInstBlockCreate(psContext,
									 psShader,
									 USP_MAX_TEXTURE_WRITE_INSTS,
									 1,
									 IMG_FALSE,
									 IMG_NULL);

	if (!psInstBlock)
	{
		USP_DBGPRINT(("USPTextureWriteCreate: Failed to alloc insts\n"));
		return IMG_NULL;
	}

	/*
		Allocate and initialise the invoke data
	*/
	psTextureWrite = (PUSP_TEXTURE_WRITE)psContext->pfnAlloc(sizeof(USP_TEXTURE_WRITE));
	
	if	(!psTextureWrite)
	{
		USP_DBGPRINT(( "USPTextureWriteCreate: Failed to alloc USP texture write data\n"));
		return IMG_NULL;
	}
	
	memset(psTextureWrite, 0, sizeof(USP_TEXTURE_WRITE));

	psTextureWrite->uWriteID 			= psPCTextureWrite->uWriteID;
	psTextureWrite->uTempRegStartNum 	= psPCTextureWrite->uTempRegStartNum;
	psTextureWrite->uMaxNumTemps		= psPCTextureWrite->uMaxNumTemps;
	psTextureWrite->uInstDescFlags      = InitUSPInstDescFlags((IMG_UINT32)psPCTextureWrite->sInstDesc.uFlags);

	/* Create the base address register */
	psTextureWrite->sBaseAddr.uNum  = psPCTextureWrite->uBaseAddrRegNum;
	psTextureWrite->sBaseAddr.eType = GetUSPHWRegType(psContext, psPCTextureWrite->uBaseAddrRegType);
	psTextureWrite->sBaseAddr.eFmt	= USP_REGFMT_U32;

	/* Create the stride register */
	psTextureWrite->sStride.uNum 	= psPCTextureWrite->uStrideRegNum;
	psTextureWrite->sStride.eType 	= GetUSPHWRegType(psContext, psPCTextureWrite->uStrideRegType);
	psTextureWrite->sStride.eFmt	= USP_REGFMT_U16;

	/* Create the co-ordinate registers */
	psTextureWrite->asCoords[0].uNum 	= psPCTextureWrite->uCoordXRegNum;
	psTextureWrite->asCoords[0].eType 	= GetUSPHWRegType(psContext, psPCTextureWrite->uCoordXRegType);
	psTextureWrite->asCoords[0].eFmt	= USP_REGFMT_U16;

	psTextureWrite->asCoords[1].uNum 	= psPCTextureWrite->uCoordYRegNum;
	psTextureWrite->asCoords[1].eType 	= GetUSPHWRegType(psContext, psPCTextureWrite->uCoordYRegType);
	psTextureWrite->asCoords[1].eFmt	= USP_REGFMT_U16;

	/* Create the channel registers */
	eChannelRegFmt = GetUSPHWRegFormat(psPCTextureWrite->uChannelRegFmt);

	psTextureWrite->asChannel[0].uNum 	= psPCTextureWrite->uChannelXRegNum;
	psTextureWrite->asChannel[0].eType 	= GetUSPHWRegType(psContext, psPCTextureWrite->uChannelXRegType);
	psTextureWrite->asChannel[0].eFmt	= eChannelRegFmt;

	psTextureWrite->asChannel[1].uNum 	= psPCTextureWrite->uChannelYRegNum;
	psTextureWrite->asChannel[1].eType 	= GetUSPHWRegType(psContext, psPCTextureWrite->uChannelYRegType);
	psTextureWrite->asChannel[1].eFmt	= eChannelRegFmt;

	psTextureWrite->asChannel[2].uNum 	= psPCTextureWrite->uChannelZRegNum;
	psTextureWrite->asChannel[2].eType 	= GetUSPHWRegType(psContext, psPCTextureWrite->uChannelZRegType);
	psTextureWrite->asChannel[2].eFmt	= eChannelRegFmt;

	psTextureWrite->asChannel[3].uNum 	= psPCTextureWrite->uChannelWRegNum;
	psTextureWrite->asChannel[3].eType 	= GetUSPHWRegType(psContext, psPCTextureWrite->uChannelWRegType);
	psTextureWrite->asChannel[3].eFmt	= eChannelRegFmt;

	psTextureWrite->psInstBlock		= psInstBlock;
	psTextureWrite->uInstCount		= 0;
	psTextureWrite->uTempRegCount 	= 0;

	return psTextureWrite;
}

/*****************************************************************************
 Name:		USPTextureWriteDestroy

 Purpose:	Destroy USP texture write data previously created using 
 			USPTextureWriteCreate

 Inputs:	psTextureWrite - The USP texture write data to be destroyed
			psContext	 - The current USP context

 Outputs:	none

 Returns:	none
*****************************************************************************/
static IMG_VOID USPTextureWriteDestroy(PUSP_TEXTURE_WRITE psTextureWrite, PUSP_CONTEXT psContext)
{
	psContext->pfnFree(psTextureWrite);
}

/*****************************************************************************
 Name:		USPShaderAddInstBlock

 Purpose:	Add an instruciton block to a USP shader

 Inputs:	psShader	- The USP shader to update
			psInstBlock	- The new instruction-block for the shader

 Outputs:	none

 Returns:	none
*****************************************************************************/
static IMG_VOID USPShaderAddInstBlock(PUSP_SHADER		psShader,
									  PUSP_INSTBLOCK	psInstBlock)
{
	PUSP_LABEL	psLabel;

	/*
		Add the new block of instructions to the end of
		the overall list blocks forming the shader
	*/
	psInstBlock->psNext = IMG_NULL;
	psInstBlock->psPrev = psShader->psInstBlocksEnd;

	if	(!psShader->psInstBlocks)
	{
		psShader->psInstBlocks = psInstBlock;	
	}
	else
	{
		psShader->psInstBlocksEnd->psNext = psInstBlock;
	}
	psShader->psInstBlocksEnd = psInstBlock;

	/*
		Update labels added immediately before this instruction block to point
		to it.
	*/
	psLabel = psShader->psLabelsEnd;
	while (psLabel && !psLabel->psInstBlock)
	{
		IMG_BOOL bAlignBlock;

		psLabel->psInstBlock = psInstBlock;

		/*
			Flag whether the start of the block needs to be aligned to an
			instruciton pair: on either the start of an execution phase, or
			the destination for a branch with SyncEnd set.
		*/
		bAlignBlock = IMG_FALSE;
		if	(
				(psLabel == psShader->psProgStartLabel) ||
				(psLabel == psShader->psPTPhase1StartLabel) ||
				(psLabel == psShader->psPTSplitPhase1StartLabel) 
			)
		{
			bAlignBlock = IMG_TRUE;
		}
		#if !defined(SGX_FEATURE_USE_NO_INSTRUCTION_PAIRING)
		else
		{
			PUSP_BRANCH	psBranch;

			psBranch = psShader->psBranchesEnd;
			while (psBranch)
			{
				if	(psBranch->psLabel == psLabel)
				{
					PUSP_INST	psBranchInst;

					psBranchInst = psBranch->psInstBlock->psFirstInst;
					if	(psBranchInst->sDesc.uFlags & USP_INSTDESC_FLAG_SYNCEND_SET)
					{
						bAlignBlock = IMG_TRUE;
						break;
					}
				}
				psBranch = psBranch->psPrev;
			}
		}
		#endif /* defined(SGX_FEATURE_USE_NO_INSTRUCTION_PAIRING) */

		psInstBlock->bAlignStartToPair = bAlignBlock;

		/*
			Flag that the first instruction in this block will be a branch
			destination
		*/
		psInstBlock->bIsBranchDest = IMG_TRUE;

		psLabel = psLabel->psPrev;
	}
}

/*****************************************************************************
 Name:		USPShaderAddSample

 Purpose:	Add a sample to a USP shader

 Inputs:	psShader	- The USP shader to update
			psSample	- The new sample for the shader

 Outputs:	none

 Returns:	none
*****************************************************************************/
static IMG_VOID USPShaderAddSample(PUSP_SHADER	psShader,
								   PUSP_SAMPLE	psSample)
{
	PUSP_SAMPLE*	ppsSampleList;
	PUSP_SAMPLE*	ppsSampleListEnd;

	/*
		Are we adding a dependent or non-dependent sample?
	*/
	if	(psSample->bOrgNonDependent)
	{
		ppsSampleList		= &psShader->psNonDepSamples;
		ppsSampleListEnd	= &psShader->psNonDepSamplesEnd;

		psShader->uNonDepSampleCount++;
	}
	else
	{
		ppsSampleList		= &psShader->psDepSamples;
		ppsSampleListEnd	= &psShader->psDepSamplesEnd;

		psShader->uDepSampleCount++;
	}

	/*
		Append the sample to the list of all those within the program
	*/
	psSample->psNext	= IMG_NULL;
	psSample->psPrev	= *ppsSampleListEnd;

	if	(!*ppsSampleList)
	{
		*ppsSampleList = psSample;	
	}
	else
	{
		(*ppsSampleListEnd)->psNext = psSample;
	}
	*ppsSampleListEnd = psSample;
}

/*****************************************************************************
 Name:		USPShaderAddSampleUnpack

 Purpose:	Add a sample unpack to a USP shader

 Inputs:	psShader	- The USP shader to update
			psSampleUnpack
						- The new sample unpack for the shader

 Outputs:	none

 Returns:	none
*****************************************************************************/
static IMG_VOID USPShaderAddSampleUnpack(	PUSP_SHADER			psShader,
											PUSP_SAMPLEUNPACK	psSampleUnpack)
{
	PUSP_SAMPLEUNPACK*	ppsSampleUnpackList;
	PUSP_SAMPLEUNPACK*	ppsSampleUnpackListEnd;
	
	ppsSampleUnpackList		= &psShader->psSampleUnpacks;
	ppsSampleUnpackListEnd	= &psShader->psSampleUnpacksEnd;

	psShader->uSampleUnpackCount++;	
	/*
		Append the sample to the list of all those within the program
	*/
	psSampleUnpack->psNext	= IMG_NULL;
	psSampleUnpack->psPrev	= *ppsSampleUnpackListEnd;

	if	(!*ppsSampleUnpackList)
	{
		*ppsSampleUnpackList = psSampleUnpack;
	}
	else
	{
		(*ppsSampleUnpackListEnd)->psNext = psSampleUnpack;
	}
	*ppsSampleUnpackListEnd = psSampleUnpack;
}

/*****************************************************************************
 Name:		USPShaderAddBranch

 Purpose:	Add a branch to a USP shader

 Inputs:	psShader	- The USP shader to update
			psBranch	- The new branch for the shader

 Outputs:	none

 Returns:	none
*****************************************************************************/
static IMG_VOID USPShaderAddBranch(PUSP_SHADER	psShader,
								   PUSP_BRANCH	psBranch)
{
	PUSP_LABEL	psLabel;

	/*
		Append the branch to the list of all those within the program
	*/
	psBranch->psNext	= IMG_NULL;
	psBranch->psPrev	= psShader->psBranchesEnd;

	if	(!psShader->psBranches)
	{
		psShader->psBranches = psBranch;
	}
	else
	{
		psShader->psBranchesEnd->psNext = psBranch;
	}
	psShader->psBranchesEnd = psBranch;

	/*
		Lookup the label referenced by this branch

		NB: There should only ever be one label with each ID
	*/
	if	(psBranch->uLabelID != (IMG_UINT32)-1)
	{
		psLabel = psShader->psLabelsEnd;
		while (psLabel)
		{
			if	(psLabel->uID == psBranch->uLabelID)
			{
				psBranch->psLabel = psLabel;

				#if !defined(SGX_FEATURE_USE_NO_INSTRUCTION_PAIRING)
				/*
					Flag that the destination instruction block (if present)
					must be aligned for branches with SyncEnd set
				*/
				if	(psLabel->psInstBlock)
				{
					PUSP_INST	psBranchInst;

					psBranchInst = psBranch->psInstBlock->psFirstInst;
					if	(psBranchInst->sDesc.uFlags & USP_INSTDESC_FLAG_SYNCEND_SET)
					{
						psLabel->psInstBlock->bAlignStartToPair = IMG_TRUE;
					}
				}
				#endif /* !defined(SGX_FEATURE_USE_NO_INSTRUCTION_PAIRING) */

				break;
			}
			psLabel = psLabel->psPrev;
		}
	}
}

/*****************************************************************************
 Name:		USPShaderAddTextureWrite

 Purpose:	Add an texture write to a USP shader

 Inputs:	psShader	- The USP shader to update
			psLabel		- The new texture write for the shader
			psContext	- The USP execution context

 Outputs:	none

 Returns:	IMG_TRUE iff texture write was added to USP internal list
*****************************************************************************/
static IMG_BOOL USPShaderAddTextureWrite(PUSP_SHADER		psShader,
									   PUSP_TEXTURE_WRITE	psTextureWrite,
									   PUSP_CONTEXT		psContext)
{
	PVR_UNREFERENCED_PARAMETER( psContext );
	
	/*
	  Append the label to the list of all those within the program
	*/
	psTextureWrite->psNext	= IMG_NULL;
	psTextureWrite->psPrev	= psShader->psTextureWritesEnd;

	if	(!psShader->psTextureWrites)
	{
		psShader->psTextureWrites = psTextureWrite;	
	}
	else
	{
		psShader->psTextureWritesEnd->psNext = psTextureWrite;
	}
	psShader->psTextureWritesEnd = psTextureWrite;

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		USPShaderAddLabel

 Purpose:	Add a label to a USP shader

 Inputs:	psShader	- The USP shader to update
			psLabel		- The new label for the shader
			psContext	- The USP execution context

 Outputs:	none

 Returns:	IMG_FALSE if multiple labels with the same ID are detected.
			IMG_TRUE otherwise.
*****************************************************************************/
static IMG_BOOL USPShaderAddLabel(PUSP_SHADER	psShader,
								  PUSP_LABEL	psLabel,
								  PUSP_CONTEXT	psContext)
{
	PUSP_PROGDESC	psProgDesc;
	PUSP_LABEL		psOtherLabel;
	PUSP_BRANCH		psBranch;

	/*
		Make sure that there is only one label with this ID
	*/
	psOtherLabel = psShader->psLabelsEnd;
	while (psOtherLabel)
	{
		if	(psOtherLabel->uID == psLabel->uID)
		{
			return IMG_FALSE;
		}
		psOtherLabel = psOtherLabel->psPrev;
	}

	/*
		Append the label to the list of all those within the program
	*/
	psLabel->psNext	= IMG_NULL;
	psLabel->psPrev	= psShader->psLabelsEnd;

	if	(!psShader->psLabels)
	{
		psShader->psLabels = psLabel;	
	}
	else
	{
		psShader->psLabelsEnd->psNext = psLabel;
	}
	psShader->psLabelsEnd = psLabel;

	/*
		Setup branches that referenced this label
	*/
	psBranch = psShader->psBranchesEnd;
	while (psBranch)
	{
		if	(psBranch->uLabelID == psLabel->uID)
		{
			psBranch->psLabel = psLabel;
		}
		psBranch = psBranch->psPrev;
	}

	/*
		Make a note of special labels within the program
	*/
	psProgDesc = psShader->psProgDesc;

	if	(psLabel->uID == psProgDesc->uProgStartLabelID)
	{
		psShader->psProgStartLabel = psLabel;
	}
	if	(psLabel->uID == psProgDesc->uPTPhase0EndLabelID)
	{
		psShader->psPTPhase0EndLabel = psLabel;
	}
	if	(psLabel->uID == psProgDesc->uPTPhase1StartLabelID)
	{
		psShader->psPTPhase1StartLabel = psLabel;
	}
	if	(psLabel->uID == psProgDesc->uPTSplitPhase1StartLabelID)
	{
		psShader->psPTSplitPhase1StartLabel = psLabel;
	}

	/*
		If this label marks the beginning of the program, add an extra block
		to contain any preable instructions that may be needed.
	*/
	if	(psLabel->uID == psProgDesc->uProgStartLabelID)
	{
		PUSP_INSTBLOCK	psPreableBlock;

		/*
			Add an empty instruction block to the end of the shader for
			any additional instruciton we may need to append.

			NB: Currently, this could be instruction to reset the MOE state, and a 
				MOV for result-register remapping.
		*/
		psPreableBlock = USPInstBlockCreate(psContext, 
											psShader, 
											USP_MAX_PREAMBLE_INST_COUNT, 
											0, 
											IMG_TRUE,
											IMG_NULL);
		if	(!psPreableBlock)
		{
			USP_DBGPRINT(( "USPShaderAddLabel: Failed to create shader-preable USP inst block\n"));
			return IMG_FALSE;
		}

		/*
			Add the preamble instruction block to the shader
		*/
		USPShaderAddInstBlock(psShader, psPreableBlock);

		psShader->psPreableBlock = psPreableBlock;
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		USPShaderAddResultRef

 Purpose:	Append a result-reference to the list of those for a shader

 Inputs:	psShader	- The USP shader to update
			psResultRef	- The result-ref to add to the shader

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL USPShaderAddResultRef(PUSP_SHADER		psShader,
											PUSP_RESULTREF	psResultRef)
{
	/*
		Append the new result-reference to the list of all those within
		the shader
	*/
	psResultRef->psNext = IMG_NULL;
	psResultRef->psPrev = psShader->psResultRefsEnd;

	if	(!psShader->psResultRefs)
	{
		psShader->psResultRefs = psResultRef;	
	}
	else
	{
		psShader->psResultRefsEnd->psNext = psResultRef;
	}
	psShader->psResultRefsEnd = psResultRef;

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		USPShaderRemoveResultRef

 Purpose:	Remove a result-reference from the list of those for a shader

 Inputs:	psShader	- The USP shader to update
			psResultRef	- The result-ref to remove from the shader

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL USPShaderRemoveResultRef(PUSP_SHADER		psShader,
											   PUSP_RESULTREF	psResultRef)
{
	PUSP_RESULTREF	psNext, psPrev;

	psNext = psResultRef->psNext;
	psPrev = psResultRef->psPrev;

	if	(psPrev)
	{
		psPrev->psNext = psNext;
	}
	if	(psNext)
	{
		psNext->psPrev = psPrev;
	}
	if	(psShader->psResultRefs == psResultRef)
	{
		psShader->psResultRefs = psNext;
	}
	if	(psShader->psResultRefsEnd == psResultRef)
	{
		psShader->psResultRefsEnd = psPrev;
	}

	psResultRef->psNext = IMG_NULL;
	psResultRef->psPrev = IMG_NULL;

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		ReadUINT32Straight

 Purpose:	Read a UINT32 from the precompiled binary shader data, without
			swizzling the bytes

 Inputs:	ppvPCData	- Pointer to the precompiled shader data
			pvData		- Where to write the data

 Outputs:	ppvPCData	- Updated to point to the next data to be read

 Returns:	nothing
*****************************************************************************/
static IMG_VOID ReadUINT32Straight(IMG_PVOID*	ppvPCData, 
								   IMG_PVOID	pvData)
{
	IMG_PUINT8	puData, puPCData;

	puData		= (IMG_PUINT8)pvData;
	puPCData	= (IMG_PUINT8)(*ppvPCData);

	puData[0] = puPCData[0];
	puData[1] = puPCData[1];
	puData[2] = puPCData[2];
	puData[3] = puPCData[3];

	*ppvPCData	= puPCData + sizeof(IMG_UINT32);
}

/*****************************************************************************
 Name:		ReadUINT32Swapped

 Purpose:	Read a UINT32 from the precompiled binary shader data, swapping
			byte-order

 Inputs:	ppvPCData	- Pointer to the precompiled shader data
			pvData		- Where to write the data

 Outputs:	ppvPCData	- Updated to point to the next data to be read

 Returns:	nothing
*****************************************************************************/
static IMG_VOID ReadUINT32Swapped(IMG_PVOID*	ppvPCData, 
								  IMG_PVOID		pvData)
{
	IMG_PUINT8	puData, puPCData;

	puData		= (IMG_PUINT8)pvData;
	puPCData	= (IMG_PUINT8)(*ppvPCData);

	puData[0] = puPCData[3];
	puData[1] = puPCData[2];
	puData[2] = puPCData[1];
	puData[3] = puPCData[0];

	*ppvPCData	= puPCData + sizeof(IMG_UINT32);
}

/*****************************************************************************
 Name:		ReadUINT16Straight

 Purpose:	Read a UINT16 from the precompiled binary shader data, without
			swizzling the bytes

 Inputs:	ppvPCData	- Pointer to the precompiled shader data
			pvData		- Where to write the data

 Outputs:	ppvPCData	- Updated to point to the next data to be read

 Returns:	nothing
*****************************************************************************/
static IMG_VOID ReadUINT16Straight(IMG_PVOID*	ppvPCData,
								   IMG_PVOID	pvData)
{
	IMG_PUINT8	puData, puPCData;

	puData		= (IMG_PUINT8)pvData;
	puPCData	= (IMG_PUINT8)(*ppvPCData);

	puData[0] = puPCData[0];
	puData[1] = puPCData[1];

	*ppvPCData	= puPCData + sizeof(IMG_UINT16);
}

/*****************************************************************************
 Name:		ReadUINT16Swapped

 Purpose:	Read a UINT16 from the precompiled binary shader data, swapping
			byte-order

 Inputs:	ppvPCData	- Pointer to the precompiled shader data
			pvData		- Where to write the data

 Outputs:	ppvPCData	- Updated to point to the next data to be read

 Returns:	nothing
*****************************************************************************/
static IMG_VOID ReadUINT16Swapped(IMG_PVOID*	ppvPCData,
								  IMG_PVOID		pvData)
{
	IMG_PUINT8	puData, puPCData;

	puData		= (IMG_PUINT8)pvData;
	puPCData	= (IMG_PUINT8)(*ppvPCData);

	puData[0] = puPCData[1];
	puData[1] = puPCData[0];

	*ppvPCData	= puPCData + sizeof(IMG_UINT16);
}


/*
	Set of data writing routines.
	For writing to pre-compiled shader always write straight data.
*/
/*****************************************************************************
 Name:		WriteUINT32Straight

 Purpose:	Write a IMG_UINT32 to the precompiled binary shader data, without
			swizzling the bytes

 Inputs:	ppvPCData	- Pointer to the precompiled shader data
			uValue		- Data to write

 Outputs:	ppvPCData	- Updated to point to the next data to be written

 Returns:	nothing
*****************************************************************************/
static IMG_VOID WriteUINT32Straight(IMG_PVOID*	      ppvPCData, 
									const IMG_UINT32  uValue)
{
	IMG_PUINT8	puData, puPCData;

	puData		= (IMG_PUINT8)(&uValue);
	puPCData	= (IMG_PUINT8)(*ppvPCData);

	puPCData[0] = puData[0];
	puPCData[1] = puData[1];
	puPCData[2] = puData[2];
	puPCData[3] = puData[3];

	*ppvPCData	= puPCData + sizeof(IMG_UINT32);
}

/*****************************************************************************
 Name:		WriteUINT16

 Purpose:	Write a IMG_UINT16 to the precompiled binary shader data, without
			swizzling the bytes

 Inputs:	ppvPCData	- Pointer to the precompiled shader data
			uValue		- Data to write

 Outputs:	ppvPCData	- Updated to point to the next data to be written

 Returns:	nothing
*****************************************************************************/
static IMG_VOID WriteUINT16Straight(IMG_PVOID*	      ppvPCData, 
									const IMG_UINT16  uValue)
{
	IMG_PUINT8	puData, puPCData;

	puData		= (IMG_PUINT8)(&uValue);
	puPCData	= (IMG_PUINT8)(*ppvPCData);

	puPCData[0] = puData[0];
	puPCData[1] = puData[1];

	*ppvPCData	= puPCData + sizeof(IMG_UINT16);
}

static IMG_VOID WriteAddSizeUINT32(IMG_PVOID*        ppvPCData, 
								   const IMG_UINT32  uValue)
{
	USP_UNREFERENCED_PARAMETER(uValue);
	*ppvPCData	= (IMG_PUINT8)(*ppvPCData) + sizeof(IMG_UINT32);
}

static IMG_VOID WriteAddSizeUINT16(IMG_PVOID*        ppvPCData, 
								   const IMG_UINT16  uValue)
{
	USP_UNREFERENCED_PARAMETER(uValue);
	*ppvPCData	= (IMG_PUINT8)(*ppvPCData) + sizeof(IMG_UINT16);
}

/*****************************************************************************
 Name:		ReadPCBlockHdr

 Purpose:	Read the header for a block of PC shader data

 Inputs:	psContext		- The current USP context
			psDataReaders	- The set of data-reading functions to use
							  (unused here)
			ppvPCData		- Pointer to the precompiled shader data

 Outputs:	ppvPCData		- Updated to point to the next data to be read
			psHdr			- Where to write the PC block header

 Returns:	TRUE/FALSE on success/failure
*****************************************************************************/
static IMG_BOOL ReadPCBlockHdr(PUSP_CONTEXT			psContext,
							   PPC_DATA_READERS		psDataReaders,
							   IMG_PVOID*			ppvPCData, 
							   PUSP_PC_BLOCK_HDR	psHdr)
{	
	USP_UNREFERENCED_PARAMETER(psContext);

	/*
		Read and validate the header data
	*/
	psDataReaders->pfnReadUINT32(ppvPCData, &psHdr->uType);

	switch (psHdr->uType)
	{
		case USP_PC_BLOCK_TYPE_PROGDESC:
		case USP_PC_BLOCK_TYPE_HWCODE:
		case USP_PC_BLOCK_TYPE_BRANCH:
		case USP_PC_BLOCK_TYPE_LABEL:
		case USP_PC_BLOCK_TYPE_SAMPLE:
		case USP_PC_BLOCK_TYPE_SAMPLEUNPACK:
		case USP_PC_BLOCK_TYPE_TEXTURE_WRITE:
		case USP_PC_BLOCK_TYPE_END:
		{
			break;
		}

		default:
		{
			USP_DBGPRINT(( "ReadPCBlockHdr: Unrecognised PC block type %d\n", psHdr->uType));
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		WritePCBlockHdr

 Purpose:	Write the header to PC shader data

 Inputs:	psContext		- The current USP context
			psDataWriters	- The set of data-writing functions to use
			ppvPCData		- Pointer to the precompiled shader data

 Outputs:	ppvPCData		- Updated to point to the next data to be written
			psHdr			- Where to read the PC block header

 Returns:	TRUE/FALSE on success/failure
*****************************************************************************/
static IMG_BOOL WritePCBlockHdr(PUSP_CONTEXT		psContext,
								PPC_DATA_WRITERS	psDataWriters,
								IMG_PVOID*			ppvPCData, 
								PUSP_PC_BLOCK_HDR	psHdr)
{	
	USP_UNREFERENCED_PARAMETER(psContext);

	/*
		Validate an write the header data
	*/

	switch (psHdr->uType)
	{
		case USP_PC_BLOCK_TYPE_PROGDESC:
		case USP_PC_BLOCK_TYPE_HWCODE:
		case USP_PC_BLOCK_TYPE_BRANCH:
		case USP_PC_BLOCK_TYPE_LABEL:
		case USP_PC_BLOCK_TYPE_SAMPLE:
		case USP_PC_BLOCK_TYPE_SAMPLEUNPACK:
		case USP_PC_BLOCK_TYPE_END:
		{
			psDataWriters->pfnWriteUINT32(ppvPCData, psHdr->uType);
			break;
		}

		default:
		{
			USP_DBGPRINT(( "WritePCBlockHdr: Unrecognised PC block type\n"));
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		InitUSPInstDescFlags

 Purpose:	Setup USP instruction description flags form their pre-compiled
			counterparts

 Inputs:	uPCInstDescFlags	- The PC inst-desc flags to decode

 Outputs:	none

 Returns:	The initialise USP instruction description flags
*****************************************************************************/
static IMG_UINT32 InitUSPInstDescFlags(IMG_UINT32 uPCInstDescFlags)
{
	IMG_UINT32	uInstDescFlags;

	uInstDescFlags = 0;

	if	(uPCInstDescFlags & USP_PC_INSTDESC_FLAG_IREGS_LIVE_BEFORE)
	{
		uInstDescFlags |= USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE;
	}	
	if	(uPCInstDescFlags & USP_PC_INSTDESC_FLAG_DONT_SKIPINV)
	{
		uInstDescFlags |= USP_INSTDESC_FLAG_DONT_SKIPINV;
	}

	if	(uPCInstDescFlags & USP_PC_INSTDESC_FLAG_DEST_USES_RESULT)
	{
		uInstDescFlags |= USP_INSTDESC_FLAG_DEST_USES_RESULT;
	}
	if	(uPCInstDescFlags & USP_PC_INSTDESC_FLAG_SRC0_USES_RESULT)
	{
		uInstDescFlags |= USP_INSTDESC_FLAG_SRC0_USES_RESULT;
	}
	if	(uPCInstDescFlags & USP_PC_INSTDESC_FLAG_SRC1_USES_RESULT)
	{
		uInstDescFlags |= USP_INSTDESC_FLAG_SRC1_USES_RESULT;
	}
	if	(uPCInstDescFlags & USP_PC_INSTDESC_FLAG_SRC2_USES_RESULT)
	{
		uInstDescFlags |= USP_INSTDESC_FLAG_SRC2_USES_RESULT;
	}

	return uInstDescFlags;
}

/*****************************************************************************
 Name:		USPShaderAddPCInstBlock

 Purpose:	Setup and add an instruciton to the USP shader, from a list of
			PC instructions and descriptors

 Inputs:	psContext		- The current USP context
			psShader		- The USP Shader being created
			psMOEState		- The MOE state at the start of the new block
			uPCInstCount	- The number of pre-compiled instructions for
							  the block
			psPCInstDescs	- A list of PC instruction descriptors for the
							  block's instructions
			puPCInsts		- The PC instructions for the block

 Outputs:	none

 Returns:	TRUE/FALSE on success/failure
*****************************************************************************/
static IMG_BOOL USPShaderAddPCInstBlock(PUSP_CONTEXT		psContext,
										PUSP_SHADER			psShader,
										PUSP_MOESTATE		psMOEState,
										IMG_UINT32			uPCInstCount,
										PUSP_PC_INSTDESC	psPCInstDescs,
										IMG_PUINT32			puPCInsts)
{
	PUSP_INSTBLOCK	psInstBlock;
	IMG_BOOL		bFailed;
	IMG_UINT32		i;

	/*
		No block created yet
	*/
	bFailed		= IMG_TRUE;
	psInstBlock	= IMG_NULL;

	/*
		Create and initialise a block of instructions for the USP shader
	*/
	psInstBlock = USPInstBlockCreate(psContext,
									 psShader,
									 uPCInstCount,
									 uPCInstCount,
									 IMG_TRUE,
									 psMOEState);
	if	(!psInstBlock)
	{
		USP_DBGPRINT(( "USPShaderAddPCInstBlock: Failed to create USP inst block\n"));
		goto USPShaderAddPCInstBlockExit;
	}

	/*
		Add the pre-compiled instructions to the new block
	*/
	for	(i = 0; i < uPCInstCount; i++)
	{
		PUSP_PC_INSTDESC	psPCInstDesc;
		PHW_INST			psPCInst;
		IMG_UINT32			uInstDescFlags;
		PUSP_INST			psInst;

		psPCInstDesc	= psPCInstDescs + i;
		psPCInst		= (PHW_INST)puPCInsts + i;
		psInst			= IMG_NULL;

		/*
			Add this PC instruciton to the block
		*/
		uInstDescFlags = InitUSPInstDescFlags(psPCInstDesc->uFlags);

		if	(!USPInstBlockAddPCInst(psInstBlock,
									psPCInst,
									uInstDescFlags,
									psContext,
									&psInst))
		{
			USP_DBGPRINT(("USPShaderAddPCInstBlock: Error adding PC inst to block\n"));
			goto USPShaderAddPCInstBlockExit;
		}
	}

	/*
		Add the new instruction block to the shader
	*/
	USPShaderAddInstBlock(psShader, psInstBlock);

	/*
		Instruction block created OK.
	*/
	bFailed = IMG_FALSE;

	USPShaderAddPCInstBlockExit:

	/*
		Cleanup on error
	*/
	if	(bFailed)
	{
		if	(psInstBlock)
		{
			USPInstBlockDestroy(psInstBlock, psContext);
		}
	}

	return (IMG_BOOL)!bFailed;
}

/*****************************************************************************
 Name:		ReadPCMOEState

 Purpose:	Read pre-compiled MOE state

 Inputs:	psDataReaders	- The set of data-reading functions to use
							  (unused here)
			ppvPCData		- Pointer to the precompiled shader data

 Outputs:	psPCMOEState	- Filled-in with the MOE state read
			ppvPCData		- Updated to point to the next data to be read

 Returns:	none
*****************************************************************************/
static IMG_VOID ReadPCMOEState(PPC_DATA_READERS	psDataReaders,
							   IMG_PVOID*		ppvPCData,
							   PUSP_PC_MOESTATE	psPCMOEState)
{
	IMG_UINT32	i;

	psDataReaders->pfnReadUINT16(ppvPCData, &psPCMOEState->uMOEFmtCtlFlags);

	for	(i = 0; i < USP_PC_MOE_OPERAND_COUNT; i++)
	{
		psDataReaders->pfnReadUINT16(ppvPCData, &psPCMOEState->auMOEIncOrSwiz[i]);
	}
	for	(i = 0; i < USP_PC_MOE_OPERAND_COUNT; i++)
	{
		psDataReaders->pfnReadUINT16(ppvPCData, &psPCMOEState->auMOEBaseOffset[i]);
	}
}

/*****************************************************************************
 Name:		DecodePCMOEState

 Purpose:	Decode pre-compiled MOE state into USP-internal MOE state

 Inputs:	psPCMOEState	- The PC MOE state to decode/expand

 Outputs:	psUSPMOEState	- USP-internal MOE state corresponding to the PC
							  MOE state data

 Returns:	IMG_FALSE on error.
*****************************************************************************/
static IMG_BOOL DecodePCMOEState(PUSP_PC_MOESTATE	psPCMOEState,
								 PUSP_MOESTATE		psUSPMOEState)
{
	IMG_UINT32	i;

	if	(psPCMOEState->uMOEFmtCtlFlags & USP_PC_MOE_FMTCTLFLAG_EFOFMTCTL)
	{
		psUSPMOEState->bEFOFmtCtl = IMG_TRUE;
	}
	else
	{
		psUSPMOEState->bEFOFmtCtl = IMG_FALSE;
	}

	if	(psPCMOEState->uMOEFmtCtlFlags & USP_PC_MOE_FMTCTLFLAG_COLFMTCTL)
	{
		psUSPMOEState->bColFmtCtl = IMG_TRUE;
	}
	else
	{
		psUSPMOEState->bColFmtCtl = IMG_FALSE;
	}

	for	(i = 0; i < USP_PC_MOE_OPERAND_COUNT; i++)
	{
		IMG_UINT16	uIncOrSwiz;

		uIncOrSwiz = psPCMOEState->auMOEIncOrSwiz[i];

		if	(uIncOrSwiz & USP_PC_MOE_INCORSWIZ_MODE_INC)
		{
			IMG_INT8	iInc;

			iInc = (IMG_INT8)((uIncOrSwiz & ~USP_PC_MOE_INCORSWIZ_INC_CLRMSK) >>
							  USP_PC_MOE_INCORSWIZ_INC_SHIFT);

			psUSPMOEState->abUseSwiz[i]	= IMG_FALSE;
			psUSPMOEState->aiInc[i]		= iInc;
		}
		else
		{
			IMG_UINT8	uSwiz;

			uSwiz = (IMG_UINT8)((uIncOrSwiz & ~USP_PC_MOE_INCORSWIZ_SWIZ_CLRMSK) >>
								USP_PC_MOE_INCORSWIZ_SWIZ_SHIFT);

			psUSPMOEState->abUseSwiz[i]	= IMG_TRUE;
			psUSPMOEState->auSwiz[i]	= uSwiz;
		}

		psUSPMOEState->auBaseOffset[i] = psPCMOEState->auMOEBaseOffset[i];
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		ReadPCHWCodeBlock

 Purpose:	Read the data for a block of precompiled HW code

 Inputs:	psContext		- The current USP context
			psShader		- The USP Shader being created
			psDataReaders	- The set of data-reading functions to use
							  (unused here)
			ppvPCData		- Pointer to the precompiled shader data

 Outputs:	ppvPCData		- Updated to point to the next data to be read

 Returns:	TRUE/FALSE on success/failure
*****************************************************************************/
static IMG_BOOL ReadPCHWCodeBlock(PUSP_CONTEXT		psContext,
								  PUSP_SHADER		psShader,
								  PPC_DATA_READERS	psDataReaders,
								  IMG_PVOID*		ppvPCData)
{
	USP_PC_BLOCK_HWCODE	sPCHWCode;
	PUSP_PC_INSTDESC	psPCInstDescs;
	IMG_PUINT32			puPCInsts;
	USP_MOESTATE		sMOEState;
	IMG_UINT32			uDataSize;
	IMG_UINT32			i;
	IMG_UINT32			uBlockFirstInst;
	IMG_BOOL			bPrevInstWasMOE;
	IMG_BOOL			bFailed;

	bFailed			= IMG_TRUE;
	puPCInsts		= IMG_NULL;
	psPCInstDescs	= IMG_NULL;

	/*
		Read the data preceeding the instructions
	*/
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCHWCode.uInstCount);

	ReadPCMOEState(psDataReaders, ppvPCData, &sPCHWCode.sMOEState);

	/*
		Ignore zero-sized instruciton blocks
	*/
	if	(sPCHWCode.uInstCount == 0)
	{
		bFailed = IMG_FALSE;
		goto ReadPCHWCodeBlockExit;
	}

	/*
		Read the list of pre-compiled instruction descriptors
	*/
	uDataSize = sPCHWCode.uInstCount * sizeof(USP_PC_INSTDESC);

	psPCInstDescs = (PUSP_PC_INSTDESC)psContext->pfnAlloc(uDataSize);
	if	(!psPCInstDescs)
	{
		USP_DBGPRINT(( "ReadPCHWCodeBlock: Failed to alloc space for PC inst descs\n"));
		goto ReadPCHWCodeBlockExit;
	}

	for	(i = 0; i < sPCHWCode.uInstCount; i++)
	{
		PUSP_PC_INSTDESC psPCInstDesc;

		psPCInstDesc = psPCInstDescs + i;
		psDataReaders->pfnReadUINT16(ppvPCData, &psPCInstDesc->uFlags);
	}

	/*
		Read the list of pre-compiled instruction
	*/
	uDataSize = sPCHWCode.uInstCount * EURASIA_USE_INSTRUCTION_SIZE;

	puPCInsts = (IMG_PUINT32)psContext->pfnAlloc(uDataSize);
	if	(!puPCInsts)
	{
		USP_DBGPRINT(( "ReadPCHWCodeBlock: Failed to alloc space for PC insts\n"));
		goto ReadPCHWCodeBlockExit;
	}

	for	(i = 0; i < sPCHWCode.uInstCount; i++)
	{
		PHW_INST psPCInst;

		psPCInst = (PHW_INST)puPCInsts + i;
		psDataReaders->pfnReadUINT32(ppvPCData, &psPCInst->uWord0);
		psDataReaders->pfnReadUINT32(ppvPCData, &psPCInst->uWord1);
	}

	/*
		Decode the initial MOE state for the block
	*/
	if	(!DecodePCMOEState(&sPCHWCode.sMOEState, &sMOEState))
	{
		USP_DBGPRINT(("ReadPCHWCodeBlock: Error decoding PC MOE state\n"));
		goto ReadPCHWCodeBlockExit;
	}

	/*
		Create one or more USP instruciton blocks for the PC
		instructions, splitting the blocks on MOE state changes.
	*/
	uBlockFirstInst	= 0;
	bPrevInstWasMOE	= IMG_TRUE;

	for	(i = 0; i < sPCHWCode.uInstCount; i++)
	{
		USP_OPCODE	eOpcode;
		IMG_BOOL	bIsMOEInst;
		PHW_INST	psPCInst;

		/*
			Decode the Opcode for the instruction
		*/
		psPCInst = (PHW_INST)puPCInsts + i;

		if	(!HWInstGetOpcode(psPCInst, &eOpcode))
		{
			USP_DBGPRINT(( "ReadPCHWCodeBlock: Invalid instruction\n"));
			goto ReadPCHWCodeBlockExit;
		}

		/*
			Is this a MOE instruction?
		*/
		bIsMOEInst = HWInstIsMOEControlInst(eOpcode);
		if	(bIsMOEInst)
		{
			/*
				Create an instruction block for the preceeding instructions
			*/
			if	(!bPrevInstWasMOE)
			{
				IMG_UINT32			uBlockPCInstCount;
				PUSP_PC_INSTDESC	psFirstPCInstDesc;
				IMG_PUINT32			puFirstPCInst;

				uBlockPCInstCount	= i - uBlockFirstInst;
				psFirstPCInstDesc	= psPCInstDescs + uBlockFirstInst;
				puFirstPCInst		= (IMG_PUINT32)((PHW_INST)puPCInsts + uBlockFirstInst);

				if	(!USPShaderAddPCInstBlock(psContext,
											  psShader,
											  &sMOEState,
											  uBlockPCInstCount,
											  psFirstPCInstDesc,
											  puFirstPCInst))
				{
					USP_DBGPRINT(( "ReadPCHWCodeBlock: Failed to create USP instblock from PC insts\n"));
					goto ReadPCHWCodeBlockExit;
				}

				uBlockFirstInst	= i;
			}

			/*
				Update the MOE state to reflect the changes made by this MOE
				instruction
			*/
			if	(!HWInstUpdateMOEState(eOpcode, psPCInst, &sMOEState))
			{
				USP_DBGPRINT(( "ReadPCHWCodeBlock: Invalid MOE instruction\n"));
				goto ReadPCHWCodeBlockExit;
			}
		}

		bPrevInstWasMOE = bIsMOEInst;
	}

	/*
		Create an instruction block for the last instructions
	*/
	if	(uBlockFirstInst < i)
	{
		IMG_UINT32			uBlockPCInstCount;
		PUSP_PC_INSTDESC	psFirstPCInstDesc;
		IMG_PUINT32			puFirstPCInst;

		uBlockPCInstCount	= i - uBlockFirstInst;
		psFirstPCInstDesc	= psPCInstDescs + uBlockFirstInst;
		puFirstPCInst		= (IMG_PUINT32)((PHW_INST)puPCInsts + uBlockFirstInst);

		if	(!USPShaderAddPCInstBlock(psContext,
									  psShader,
									  &sMOEState,
									  uBlockPCInstCount,
									  psFirstPCInstDesc,
									  puFirstPCInst))
		{
			USP_DBGPRINT(( "ReadPCHWCodeBlock: Failed to create USP instblock from PC insts\n"));
			goto ReadPCHWCodeBlockExit;
		}
	}

	/*
		Instruciton block read OK.	
	*/
	bFailed = IMG_FALSE;

	ReadPCHWCodeBlockExit:

	/*
		Free te list of pre-compiled instructions and descriptors
	*/
	if	(psPCInstDescs)
	{
		psContext->pfnFree(psPCInstDescs);
	}
	if	(puPCInsts)
	{
		psContext->pfnFree(puPCInsts);
	}

	return (IMG_BOOL)!bFailed;
}

/*****************************************************************************
 Name:		WritePCProgDescBlock

 Purpose:	Write the meta-data describing a precompiled shader

 Inputs:	psContext		- The current USP context
			psShader		- The USP shader being read
			psDataWriters	- The set of data-writing functions to use
			ppvPCData		- Pointer to the precompiled shader data

 Outputs:	ppvPCData		- Updated to point to the next data to be written

 Returns:	TRUE/FALSE on success/failure
*****************************************************************************/
static IMG_BOOL WritePCProgDescBlock(PUSP_CONTEXT	   psContext,
									 PUSP_SHADER	   psShader,
									 PPC_DATA_WRITERS  psDataWriters,
									 IMG_PVOID*		   ppvPCData)
{
	IMG_BOOL	   bSuccess = IMG_FALSE;
	PUSP_PROGDESC  psUSPProgDesc;
	IMG_UINT32     i;

	#if !defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psContext);
	#endif	/* #if !defined(DEBUG) */

	psUSPProgDesc = psShader->psProgDesc;

	/*
		Writes members of USP_PC_BLOCK_PROGDESC
	*/
	psDataWriters->pfnWriteUINT32(ppvPCData, (psUSPProgDesc->uShaderType));
	psDataWriters->pfnWriteUINT32(ppvPCData, (psUSPProgDesc->uFlags));
	psDataWriters->pfnWriteUINT32(ppvPCData, (psUSPProgDesc->uUFFlags));
	
	i = (psUSPProgDesc->uHWFlags) & (~UNIFLEX_HW_FLAGS_LABEL_AT_END);
	psDataWriters->pfnWriteUINT32(ppvPCData, i);

	psDataWriters->pfnWriteUINT32(ppvPCData, (psUSPProgDesc->sTargetDev.eID));
	psDataWriters->pfnWriteUINT32(ppvPCData, (psUSPProgDesc->sTargetDev.uiRev));
	psDataWriters->pfnWriteUINT32(ppvPCData, (psUSPProgDesc->uBRNCount));
	psDataWriters->pfnWriteUINT32(ppvPCData, (psUSPProgDesc->uSAUsageFlags));
	psDataWriters->pfnWriteUINT16(ppvPCData, (psUSPProgDesc->uMemConstBaseAddrSAReg));
	psDataWriters->pfnWriteUINT16(ppvPCData, (psUSPProgDesc->uMemConst2BaseAddrSAReg));
	psDataWriters->pfnWriteUINT16(ppvPCData, (psUSPProgDesc->uScratchMemBaseAddrSAReg));
	psDataWriters->pfnWriteUINT16(ppvPCData, (psUSPProgDesc->uTexStateFirstSAReg));
	psDataWriters->pfnWriteUINT32(ppvPCData, (psUSPProgDesc->uTexStateMemOffset));
	psDataWriters->pfnWriteUINT16(ppvPCData, (psUSPProgDesc->uRegConstsFirstSAReg));
	psDataWriters->pfnWriteUINT16(ppvPCData, (psUSPProgDesc->uRegConstsMaxSARegCount));
	psDataWriters->pfnWriteUINT16(ppvPCData, (psUSPProgDesc->uIndexedTempBaseAddrSAReg));
	psDataWriters->pfnWriteUINT16(ppvPCData, (psUSPProgDesc->uExtraPARegs));
	psDataWriters->pfnWriteUINT16(ppvPCData, (psUSPProgDesc->uPARegCount));
	psDataWriters->pfnWriteUINT16(ppvPCData, (psUSPProgDesc->uTempRegCount));
	psDataWriters->pfnWriteUINT16(ppvPCData, (psUSPProgDesc->uSecTempRegCount));
	psDataWriters->pfnWriteUINT32(ppvPCData, (psUSPProgDesc->uIndexedTempTotalSize));
	psDataWriters->pfnWriteUINT16(ppvPCData, (psUSPProgDesc->uPSInputCount));
	psDataWriters->pfnWriteUINT16(ppvPCData, (psUSPProgDesc->uRegConstCount));
	psDataWriters->pfnWriteUINT32(ppvPCData, (psUSPProgDesc->uMemConstCount));
	psDataWriters->pfnWriteUINT16(ppvPCData, (psUSPProgDesc->uRegTexStateCount));
	for (i = 0; i < (sizeof(psUSPProgDesc->auNonAnisoTexStages) / sizeof(psUSPProgDesc->auNonAnisoTexStages[0])); i++)
	{
		psDataWriters->pfnWriteUINT32(ppvPCData, (psUSPProgDesc->auNonAnisoTexStages[i]));
	}
	for (i = 0; i < (sizeof(psUSPProgDesc->auAnisoTexStages) / sizeof(psUSPProgDesc->auAnisoTexStages[0])); i++)
	{
		psDataWriters->pfnWriteUINT32(ppvPCData, (psUSPProgDesc->auAnisoTexStages[i]));
	}
	psDataWriters->pfnWriteUINT32(ppvPCData, (psUSPProgDesc->uScratchAreaSize));
	psDataWriters->pfnWriteUINT16(ppvPCData, (psUSPProgDesc->iSAAddressAdjust));

	{
		IMG_UINT16 uPSResultRegType;

		/* For pixel shaders do not ignore this data */
		if(psUSPProgDesc->uShaderType == USP_PC_SHADERTYPE_PIXEL)
		{
			switch(psUSPProgDesc->eDefaultPSResultRegType)
			{
				case USP_REGTYPE_TEMP:
				{
					uPSResultRegType = USP_PC_PSRESULT_REGTYPE_TEMP;
					break;
				}
				case USP_REGTYPE_OUTPUT:
				{
					uPSResultRegType = USP_PC_PSRESULT_REGTYPE_OUTPUT;
					break;
				}
				case USP_REGTYPE_PA:
				{
					uPSResultRegType = USP_PC_PSRESULT_REGTYPE_PA;
					break;
				}
				default:
				{
					USP_DBGPRINT(( "WritePCProgDescBlock: Unknown USP reg type\n"));
					goto WritePCProgDescBlockFinish;
				}
			}

			psDataWriters->pfnWriteUINT16(ppvPCData, uPSResultRegType);
		}
		else
		{
			psDataWriters->pfnWriteUINT16(ppvPCData, 0);
		}
	}

	/* For pixel shaders do not ignore this data */
	if(psUSPProgDesc->uShaderType == USP_PC_SHADERTYPE_PIXEL)
	{
		psDataWriters->pfnWriteUINT16(ppvPCData, psUSPProgDesc->uDefaultPSResultRegNum);
	}
	else
	{
		psDataWriters->pfnWriteUINT16(ppvPCData, 0);
	}

	{
		IMG_UINT16 uPSResultRegFmt;

		/* For pixel shaders do not ignore this data */
		if(psUSPProgDesc->uShaderType == USP_PC_SHADERTYPE_PIXEL)
		{
			switch (psUSPProgDesc->eDefaultPSResultRegFmt)
			{
				case USP_REGFMT_F32:
				{
					uPSResultRegFmt = USP_PC_PSRESULT_REGFMT_F32;
					break;
				}
				case USP_REGFMT_F16:
				{
					uPSResultRegFmt = USP_PC_PSRESULT_REGFMT_F16;
					break;
				}
				case USP_REGFMT_C10:
				{
					uPSResultRegFmt = USP_PC_PSRESULT_REGFMT_C10;
					break;
				}
				case USP_REGFMT_U8:
				{
					uPSResultRegFmt = USP_PC_PSRESULT_REGFMT_U8;
					break;
				}
				default:
				{
					USP_DBGPRINT(( "WritePCProgDescBlock: Unknown USP reg fmt\n"));
					goto WritePCProgDescBlockFinish;
				}
			}

			psDataWriters->pfnWriteUINT16(ppvPCData, uPSResultRegFmt);
		}
		else		
		{
			psDataWriters->pfnWriteUINT16(ppvPCData, 0);
		}
	}

	/* For pixel shaders do not ignore this data */
	if(psUSPProgDesc->uShaderType == USP_PC_SHADERTYPE_PIXEL)
	{
		psDataWriters->pfnWriteUINT16(ppvPCData, psUSPProgDesc->uDefaultPSResultRegCount);
	}
	else	
	{
		psDataWriters->pfnWriteUINT16(ppvPCData, 0);
	}
	
	/* For pixel shaders do not ignore this data */
	if(psUSPProgDesc->uShaderType == USP_PC_SHADERTYPE_PIXEL)
	{
		psDataWriters->pfnWriteUINT16(ppvPCData, psUSPProgDesc->uPSResultFirstTempReg);
	}
	else
	{
		psDataWriters->pfnWriteUINT16(ppvPCData, 0);
	}
	
	/* For pixel shaders do not ignore this data */
	if(psUSPProgDesc->uShaderType == USP_PC_SHADERTYPE_PIXEL)
	{
		psDataWriters->pfnWriteUINT16(ppvPCData, 
			psUSPProgDesc->uPSResultFirstPAReg);
	}
	else
	{
		psDataWriters->pfnWriteUINT16(ppvPCData, 0);
	}

	/* For pixel shaders do not ignore this data */
	if(psUSPProgDesc->uShaderType == USP_PC_SHADERTYPE_PIXEL)
	{
		psDataWriters->pfnWriteUINT16(ppvPCData, 
			psUSPProgDesc->uPSResultFirstOutputReg);
	}
	else
	{
		psDataWriters->pfnWriteUINT16(ppvPCData, 0);
	}

	psDataWriters->pfnWriteUINT16(ppvPCData, (psUSPProgDesc->uProgStartLabelID));
	psDataWriters->pfnWriteUINT16(ppvPCData, (psUSPProgDesc->uPTPhase0EndLabelID));
	psDataWriters->pfnWriteUINT16(ppvPCData, (psUSPProgDesc->uPTPhase1StartLabelID));
	psDataWriters->pfnWriteUINT16(ppvPCData, (psUSPProgDesc->uPTSplitPhase1StartLabelID));
	psDataWriters->pfnWriteUINT16(ppvPCData, (psUSPProgDesc->uSAUpdateInstCount));
	psDataWriters->pfnWriteUINT16(ppvPCData, (psUSPProgDesc->uRegConstsSARegCount));
	psDataWriters->pfnWriteUINT16(ppvPCData, 
		(IMG_UINT16)(psUSPProgDesc->uVSInputPARegCount));
	psDataWriters->pfnWriteUINT16(ppvPCData, 
		(IMG_UINT16)(psUSPProgDesc->uShaderOutputsCount));

	/*
		Write the list of BRNs used when the code was compiled offline
	*/
	if	((psUSPProgDesc->uBRNCount) > 0)
	{
		IMG_PUINT32	puBRNList;

		puBRNList = psUSPProgDesc->puBRNList;
		for	(i = 0; i < (psUSPProgDesc->uBRNCount); i++)
		{
			psDataWriters->pfnWriteUINT32(ppvPCData, *puBRNList);
			puBRNList++;
		}
	}

	/*
		Write the list of pre-sampled textures and iterated data for pixel-shaders
	*/
	if	((psUSPProgDesc->uPSInputCount) > 0)
	{
		PUSP_PC_PSINPUT_LOAD psInputLoad;

		/*
			Write each of the pre-sampled textures/iterated values
		*/
		psInputLoad = psUSPProgDesc->psPSInputLoads;

		for	(i = 0; i < (psUSPProgDesc->uPSInputCount); i++)
		{
			psDataWriters->pfnWriteUINT16(ppvPCData, psInputLoad->uFlags);
			psDataWriters->pfnWriteUINT16(ppvPCData, psInputLoad->uTexture);
			psDataWriters->pfnWriteUINT16(ppvPCData, psInputLoad->uCoord);
			psDataWriters->pfnWriteUINT16(ppvPCData, psInputLoad->uCoordDim);
			psDataWriters->pfnWriteUINT16(ppvPCData, psInputLoad->uFormat);
			psDataWriters->pfnWriteUINT16(ppvPCData, psInputLoad->uDataSize);

			psInputLoad++;
		}
	}


	/*
		Write the list of constant mappings for those in memory
	*/
	if	((psUSPProgDesc->uMemConstCount) > 0)
	{
		PUSP_PC_CONST_LOAD psConstLoad;

		/*
			Validate and store each of the constants to be loaded
		*/
		psConstLoad = psUSPProgDesc->psMemConstLoads;

		for	(i = 0; i < (psUSPProgDesc->uMemConstCount); i++)
		{
			psDataWriters->pfnWriteUINT16(ppvPCData, psConstLoad->uFormat);

			if	(psConstLoad->uFormat == USP_PC_CONST_FMT_STATIC)
			{
				psDataWriters->pfnWriteUINT32(ppvPCData, psConstLoad->sSrc.uStaticVal);
			}
			else
			{
				psDataWriters->pfnWriteUINT16(ppvPCData, psConstLoad->sSrc.sConst.uSrcIdx);
				if (psConstLoad->uFormat == USP_PC_CONST_FMT_C10)
				{
					psDataWriters->pfnWriteUINT16(ppvPCData, psConstLoad->sSrc.sConst.uSrcShift);
				}
			}


			switch (psConstLoad->uFormat)
			{
				case USP_PC_CONST_FMT_F32:
				case USP_PC_CONST_FMT_STATIC:
				{
					if	(psConstLoad->uDestShift != 0)
					{
						USP_DBGPRINT(( "WritePCProgDescBlock: Invalid dest-shift for F32/static const-load\n"));
						goto WritePCProgDescBlockFinish;
					}
					break;
				}

				case USP_PC_CONST_FMT_F16:
				{
					if	(
							(psConstLoad->uDestShift > 16) ||
							((psConstLoad->uDestShift % 16) != 0)
						)
					{
						USP_DBGPRINT(( "WritePCProgDescBlock: Invalid dest-shift for F16 const-load\n"));
						goto WritePCProgDescBlockFinish;
					}
					break;
				}

				case USP_PC_CONST_FMT_C10:
				{
					if	(
							(psConstLoad->uDestShift > 30) ||
							((psConstLoad->uDestShift % 10) != 0)
						)
					{
						USP_DBGPRINT(( "WritePCProgDescBlock: Invalid dest-shift for C10 const-load\n"));
						goto WritePCProgDescBlockFinish;
					}
					break;
				}
				
				case USP_PC_CONST_FMT_U8:
				{
					if	(
							(psConstLoad->uDestShift > 24) ||
							((psConstLoad->uDestShift % 8) != 0)
						)
					{
						USP_DBGPRINT(( "WritePCProgDescBlock: Invalid dest-shift for U8 const-load\n"));
						goto WritePCProgDescBlockFinish;
					}
					break;
				}

				default:
				{
					USP_DBGPRINT(( "WritePCProgDescBlock: Invalid format for const-load\n"));
					goto WritePCProgDescBlockFinish;
				}
			}

			psDataWriters->pfnWriteUINT16(ppvPCData, psConstLoad->uDestIdx);
			psDataWriters->pfnWriteUINT16(ppvPCData, psConstLoad->uDestShift);

			psConstLoad++;
		}
	}

	/*
		Write the list of constant mappings for those in SA registers
	*/
	if	((psUSPProgDesc->uRegConstCount) > 0)
	{
		PUSP_PC_CONST_LOAD psConstLoad;

		/*
			Validate and store each of the constants to be loaded
		*/
		psConstLoad = psUSPProgDesc->psRegConstLoads;

		for	(i = 0; i < (psUSPProgDesc->uRegConstCount); i++)
		{
			psDataWriters->pfnWriteUINT16(ppvPCData, psConstLoad->uFormat);

			if	(psConstLoad->uFormat == USP_PC_CONST_FMT_STATIC)
			{
				psDataWriters->pfnWriteUINT32(ppvPCData, psConstLoad->sSrc.uStaticVal);
			}
			else
			{
				psDataWriters->pfnWriteUINT16(ppvPCData, psConstLoad->sSrc.sConst.uSrcIdx);
				if (psConstLoad->uFormat == USP_PC_CONST_FMT_C10)
				{
					psDataWriters->pfnWriteUINT16(ppvPCData, psConstLoad->sSrc.sConst.uSrcShift);
				}
			}

			switch (psConstLoad->uFormat)
			{
				case USP_PC_CONST_FMT_F32:
				case USP_PC_CONST_FMT_STATIC:
				{
					if	(psConstLoad->uDestShift != 0)
					{
						USP_DBGPRINT(( "WritePCProgDescBlock: Invalid dest-shift for F32/static const-load\n"));
						goto WritePCProgDescBlockFinish;
					}
					break;
				}

				case USP_PC_CONST_FMT_F16:
				{
					if	(
							(psConstLoad->uDestShift > 16) ||
							((psConstLoad->uDestShift % 16) != 0)
						)
					{
						USP_DBGPRINT(( "WritePCProgDescBlock: Invalid dest-shift for F16 const-load\n"));
						goto WritePCProgDescBlockFinish;
					}
					break;
				}

				case USP_PC_CONST_FMT_C10:
				{
					if	(
							(psConstLoad->uDestShift > 30) ||
							((psConstLoad->uDestShift % 10) != 0)
						)
					{
						USP_DBGPRINT(( "WritePCProgDescBlock: Invalid dest-shift for C10 const-load\n"));
						goto WritePCProgDescBlockFinish;
					}
					break;
				}

				case USP_PC_CONST_FMT_U8:
				{
					if	(
							(psConstLoad->uDestShift > 24) ||
							((psConstLoad->uDestShift % 8) != 0)
						)
					{
						USP_DBGPRINT(( "WritePCProgDescBlock: Invalid dest-shift for U8 const-load\n"));
						goto WritePCProgDescBlockFinish;
					}
					break;
				}

				default:
				{
					USP_DBGPRINT(( "WritePCProgDescBlock: Invalid format for const-load\n"));
					goto WritePCProgDescBlockFinish;
				}
			}

			psDataWriters->pfnWriteUINT16(ppvPCData, psConstLoad->uDestIdx);
			psDataWriters->pfnWriteUINT16(ppvPCData, psConstLoad->uDestShift);

			psConstLoad++;
		}
	}


	/*
		Write the list of texture-state to be loaded into SA registers
	*/
	if	((psUSPProgDesc->uRegTexStateCount) > 0)
	{
		PUSP_PC_TEXSTATE_LOAD psTexStateLoad;

		/*
			Validate and store each of the sets of texture-state to be loaded
		*/
		psTexStateLoad = psUSPProgDesc->psRegTexStateLoads;

		for	(i = 0; i < (psUSPProgDesc->uRegTexStateCount); i++)
		{
			if	(psTexStateLoad->uTextureIdx > USP_MAX_SAMPLER_IDX)
			{
				USP_DBGPRINT(( "WritePCProgDescBlock: Invalid texture index in tex-state load\n"));
				goto WritePCProgDescBlockFinish;
			}

			if	(psTexStateLoad->uChunkIdx > USP_MAX_TEXTURE_CHUNKS)
			{
				USP_DBGPRINT(( "WritePCProgDescBlock: Invalid chunk index in tex-state load\n"));
				goto WritePCProgDescBlockFinish;
			}

			psDataWriters->pfnWriteUINT16(ppvPCData, psTexStateLoad->uTextureIdx);
			psDataWriters->pfnWriteUINT16(ppvPCData, psTexStateLoad->uChunkIdx);
			psDataWriters->pfnWriteUINT16(ppvPCData, psTexStateLoad->uDestIdx);

			psTexStateLoad++;
		}
	}

	/*
		Write the list of pre-compiled instructions for the SA update program
	*/
	if	((psUSPProgDesc->uSAUpdateInstCount) > 0)
	{
		PHW_INST psHWInst;

		/*
			Write the list of HW instructions
		*/
		psHWInst = psUSPProgDesc->psSAUpdateInsts;

		for	(i = 0; i < (psUSPProgDesc->uSAUpdateInstCount); i++)
		{
			psDataWriters->pfnWriteUINT32(ppvPCData, psHWInst->uWord0);
			psDataWriters->pfnWriteUINT32(ppvPCData, psHWInst->uWord1);

			psHWInst++;
		}
	}

	/*
		Write the VS input PA-register usage into (only present for vertex-shaders)
	*/
	if	((psUSPProgDesc->uShaderType == USP_PC_SHADERTYPE_VERTEX) && 
		((psUSPProgDesc->uVSInputPARegCount) > 0))
	{
		for	(i = 0; i < ((IMG_UINT32)(psUSPProgDesc->uVSInputPARegCount)+0x1F)>>5; i++)
		{
			psDataWriters->pfnWriteUINT32(ppvPCData, psUSPProgDesc->auVSInputsUsed[i]);	
		}
	}

	/* 
		Write shader outputs that are valid after finalising the shader.
	*/
	{
		{
			IMG_UINT32 uOutputsCountInDwords;
			IMG_UINT32 uBitsInLastDword;
			IMG_UINT32 uDwordIdx;
			uOutputsCountInDwords = ((IMG_UINT32)(psUSPProgDesc->uShaderOutputsCount)) / 32;
			for(uDwordIdx = 0; uDwordIdx < uOutputsCountInDwords; uDwordIdx++)
			{
				psDataWriters->pfnWriteUINT32(ppvPCData, psUSPProgDesc->puValidShaderOutputs[uDwordIdx]);
			}
			uBitsInLastDword =((IMG_UINT32)(psUSPProgDesc->uShaderOutputsCount)) % 32;
			if(uBitsInLastDword)
			{
				IMG_UINT32	uBitMask;
				uBitMask = ((IMG_UINT32)(-1));
				uBitMask = (uBitMask << uBitsInLastDword);
				uBitMask = ~uBitMask;
				psDataWriters->pfnWriteUINT32(ppvPCData, (psUSPProgDesc->puValidShaderOutputs[uOutputsCountInDwords] & uBitMask));
			}
		}
	}

	bSuccess = IMG_TRUE;
	
	WritePCProgDescBlockFinish:

	return bSuccess;
}

/*****************************************************************************
 Name:		ReadPCProgDescBlock

 Purpose:	Read the meta-data describing a precompiled shader

 Inputs:	psContext		- The current USP context
			psShader		- The USP shader being created
			psDataReaders	- The set of data-reading functions to use
							  (unused here)
			ppvPCData		- Pointer to the precompiled shader data

 Outputs:	ppvPCData		- Updated to point to the next data to be read

 Returns:	TRUE/FALSE on success/failure
*****************************************************************************/
static IMG_BOOL ReadPCProgDescBlock(PUSP_CONTEXT		psContext,
									PUSP_SHADER			psShader,
									PPC_DATA_READERS	psDataReaders,
									IMG_PVOID*			ppvPCData)
{
	USP_PC_BLOCK_PROGDESC	sPCProgDesc;
	PUSP_PROGDESC			psUSPProgDesc;
	IMG_BOOL				bSuccess;
	IMG_UINT32				i;

	/*
		Program description not created yet
	*/
	psUSPProgDesc	= IMG_NULL;
	bSuccess		= IMG_FALSE;

	/*
		Read members of USP_PC_BLOCK_PROGDESC
	*/
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCProgDesc.uShaderType);	
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCProgDesc.uFlags);
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCProgDesc.uUFFlags);
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCProgDesc.uHWFlags);
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCProgDesc.uTargetDev);
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCProgDesc.uTargetRev);
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCProgDesc.uBRNCount);
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCProgDesc.uSAUsageFlags);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uMemConstBaseAddrSAReg);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uMemConst2BaseAddrSAReg);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uScratchMemBaseAddrSAReg);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uTexStateFirstSAReg);
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCProgDesc.uTexStateMemOffset);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uRegConstsFirstSAReg);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uRegConstsMaxSARegCount);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uIndexedTempBaseAddrSAReg);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uExtraPARegs);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uPARegCount);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uTempRegCount);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uSecTempRegCount);
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCProgDesc.uIndexedTempTotalSize);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uPSInputCount);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uRegConstCount);
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCProgDesc.uMemConstCount);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uRegTexStateCount);
	for (i = 0; i < (sizeof(sPCProgDesc.auNonAnisoTexStages) / sizeof(sPCProgDesc.auNonAnisoTexStages[0])); i++)
	{
		psDataReaders->pfnReadUINT32(ppvPCData, &sPCProgDesc.auNonAnisoTexStages[i]);
	}
	for (i = 0; i < (sizeof(sPCProgDesc.auAnisoTexStages) / sizeof(sPCProgDesc.auAnisoTexStages[0])); i++)
	{
		psDataReaders->pfnReadUINT32(ppvPCData, &sPCProgDesc.auAnisoTexStages[i]);
	}
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCProgDesc.uScratchAreaSize);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.iSAAddressAdjust);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uPSResultRegType);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uPSResultRegNum);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uPSResultRegFmt);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uPSResultRegCount);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uPSResultTempReg);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uPSResultPAReg);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uPSResultOutputReg);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uProgStartLabelID);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uPTPhase0EndLabelID);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uPTPhase1StartLabelID);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uPTSplitPhase1StartLabelID);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uSAUpdateInstCount);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uRegConstsSARegCount);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uVSInputPARegCount);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCProgDesc.uShaderOutputsCount);

	/*
		Create the USP program description data
	*/
	psUSPProgDesc = USPProgDescCreate(psContext,
									  sPCProgDesc.uBRNCount,
									  sPCProgDesc.uPSInputCount,
									  sPCProgDesc.uMemConstCount,
									  sPCProgDesc.uRegConstCount,
									  sPCProgDesc.uRegTexStateCount,
									  sPCProgDesc.uSAUpdateInstCount, 
									  sPCProgDesc.uShaderOutputsCount);

	if	(!psUSPProgDesc)
	{
		USP_DBGPRINT(( "ReadPCProgDescBlock: Failed to create USP prog-desc\n"));
		goto ReadPCProgDescBlockFinish;
	}

	/*
		Copy members from the PC program description into the USP version
	*/
	psUSPProgDesc->uShaderType	= sPCProgDesc.uShaderType;
	psUSPProgDesc->uFlags	= sPCProgDesc.uFlags;
	psUSPProgDesc->uUFFlags	= sPCProgDesc.uUFFlags;
	psUSPProgDesc->uHWFlags	= sPCProgDesc.uHWFlags;

	switch (sPCProgDesc.uTargetDev)
	{
		case SGX_CORE_ID_520:
		case SGX_CORE_ID_530:
		case SGX_CORE_ID_531:
		case SGX_CORE_ID_535:
		case SGX_CORE_ID_540:
		case SGX_CORE_ID_541:
		case SGX_CORE_ID_543:
		case SGX_CORE_ID_544:
	    case SGX_CORE_ID_545:
	    case SGX_CORE_ID_554:
		{
			psUSPProgDesc->sTargetDev.eID = (SGX_CORE_ID_TYPE)sPCProgDesc.uTargetDev;
			psUSPProgDesc->sTargetDev.uiRev = sPCProgDesc.uTargetRev;
			break;
		}
		default:
		{
			USP_DBGPRINT(( "ReadPCProgDescBlock: Unknown target device\n"));
			goto ReadPCProgDescBlockFinish;
		}
	}
	psUSPProgDesc->uBRNCount = sPCProgDesc.uBRNCount;

	psUSPProgDesc->uSAUsageFlags				= sPCProgDesc.uSAUsageFlags;
	psUSPProgDesc->uMemConstBaseAddrSAReg		= sPCProgDesc.uMemConstBaseAddrSAReg;
	psUSPProgDesc->uMemConst2BaseAddrSAReg		= sPCProgDesc.uMemConst2BaseAddrSAReg;
	psUSPProgDesc->uScratchMemBaseAddrSAReg		= sPCProgDesc.uScratchMemBaseAddrSAReg;
	psUSPProgDesc->uTexStateFirstSAReg			= sPCProgDesc.uTexStateFirstSAReg;
	psUSPProgDesc->uTexStateMemOffset			= sPCProgDesc.uTexStateMemOffset;
	psUSPProgDesc->uRegConstsFirstSAReg			= sPCProgDesc.uRegConstsFirstSAReg;
	psUSPProgDesc->uRegConstsSARegCount			= sPCProgDesc.uRegConstsSARegCount;
	psUSPProgDesc->uRegConstsMaxSARegCount		= sPCProgDesc.uRegConstsMaxSARegCount;
	psUSPProgDesc->uIndexedTempBaseAddrSAReg	= sPCProgDesc.uIndexedTempBaseAddrSAReg;
	psUSPProgDesc->uIndexedTempTotalSize		= sPCProgDesc.uIndexedTempTotalSize;

	psUSPProgDesc->uExtraPARegs			= sPCProgDesc.uExtraPARegs;
	psUSPProgDesc->uPARegCount			= sPCProgDesc.uPARegCount;
	psUSPProgDesc->uTempRegCount		= sPCProgDesc.uTempRegCount;
	psUSPProgDesc->uSecTempRegCount		= sPCProgDesc.uSecTempRegCount;
	psUSPProgDesc->uPSInputCount		= sPCProgDesc.uPSInputCount;
	psUSPProgDesc->uMemConstCount		= sPCProgDesc.uMemConstCount;
	psUSPProgDesc->uRegConstCount		= sPCProgDesc.uRegConstCount;
	psUSPProgDesc->uRegTexStateCount	= sPCProgDesc.uRegTexStateCount;

	memcpy(psUSPProgDesc->auNonAnisoTexStages, sPCProgDesc.auNonAnisoTexStages, sizeof(psUSPProgDesc->auNonAnisoTexStages));
	memcpy(psUSPProgDesc->auAnisoTexStages, sPCProgDesc.auAnisoTexStages, sizeof(psUSPProgDesc->auAnisoTexStages));
	psUSPProgDesc->uScratchAreaSize		= sPCProgDesc.uScratchAreaSize;
	psUSPProgDesc->iSAAddressAdjust		= sPCProgDesc.iSAAddressAdjust;

	psUSPProgDesc->uProgStartLabelID		= sPCProgDesc.uProgStartLabelID;
	psUSPProgDesc->uPTPhase0EndLabelID		= sPCProgDesc.uPTPhase0EndLabelID;
	psUSPProgDesc->uPTPhase1StartLabelID	= sPCProgDesc.uPTPhase1StartLabelID;
	psUSPProgDesc->uPTSplitPhase1StartLabelID 
											= sPCProgDesc.uPTSplitPhase1StartLabelID;
	psUSPProgDesc->uSAUpdateInstCount		= sPCProgDesc.uSAUpdateInstCount;
	psUSPProgDesc->uShaderOutputsCount		= sPCProgDesc.uShaderOutputsCount;

	/*
		If this is a pixel-shader, record where the PS result has
		been stored
	*/
	if	(psUSPProgDesc->uShaderType == USP_PC_SHADERTYPE_PIXEL)
	{
		psUSPProgDesc->uDefaultPSResultRegNum	= sPCProgDesc.uPSResultRegNum;
		psUSPProgDesc->uDefaultPSResultRegCount	= sPCProgDesc.uPSResultRegCount;

		switch (sPCProgDesc.uPSResultRegType)
		{
			case USP_PC_PSRESULT_REGTYPE_TEMP:
			{
				psUSPProgDesc->eDefaultPSResultRegType = USP_REGTYPE_TEMP;
				break;
			}
			case USP_PC_PSRESULT_REGTYPE_OUTPUT:
			{
				psUSPProgDesc->eDefaultPSResultRegType = USP_REGTYPE_OUTPUT;
				break;
			}
			case USP_PC_PSRESULT_REGTYPE_PA:
			{
				psUSPProgDesc->eDefaultPSResultRegType = USP_REGTYPE_PA;
				break;
			}
			default:
			{
				USP_DBGPRINT(( "ReadPCProgDescBlock: Unknown PS result type\n"));
				goto ReadPCProgDescBlockFinish;
			}
		}

		switch (sPCProgDesc.uPSResultRegFmt)
		{
			case USP_PC_PSRESULT_REGFMT_F32:
			{
				psUSPProgDesc->eDefaultPSResultRegFmt = USP_REGFMT_F32;
				break;
			}
			case USP_PC_PSRESULT_REGFMT_F16:
			{
				psUSPProgDesc->eDefaultPSResultRegFmt = USP_REGFMT_F16;
				break;
			}
			case USP_PC_PSRESULT_REGFMT_C10:
			{
				psUSPProgDesc->eDefaultPSResultRegFmt = USP_REGFMT_C10;
				break;
			}
			case USP_PC_PSRESULT_REGFMT_U8:
			{
				psUSPProgDesc->eDefaultPSResultRegFmt = USP_REGFMT_U8;
				break;
			}
			default:
			{
				USP_DBGPRINT(( "ReadPCProgDescBlock: Unknown PS result fmt\n"));
				goto ReadPCProgDescBlockFinish;
			}
		}

		psUSPProgDesc->uPSResultFirstTempReg	= sPCProgDesc.uPSResultTempReg;
		psUSPProgDesc->uPSResultFirstPAReg		= sPCProgDesc.uPSResultPAReg;
		psUSPProgDesc->uPSResultFirstOutputReg	= sPCProgDesc.uPSResultOutputReg;
	}

	/*
		Read the list of BRNs used when the code was compiled offline
	*/
	if	(sPCProgDesc.uBRNCount > 0)
	{
		IMG_PUINT32	puBRNList;

		puBRNList = psUSPProgDesc->puBRNList;
		for	(i = 0; i < sPCProgDesc.uBRNCount; i++)
		{
			psDataReaders->pfnReadUINT32(ppvPCData, puBRNList);
			puBRNList++;
		}
	}

	/*
		Read the list of pre-sampled textures and iterated data for pixel-shaders
	*/
	if	(sPCProgDesc.uPSInputCount > 0)
	{
		PUSP_PC_PSINPUT_LOAD psInputLoad;

		/*
			Load each of the pre-sampled textures/iterated values
		*/
		psInputLoad = psUSPProgDesc->psPSInputLoads;

		for	(i = 0; i < sPCProgDesc.uPSInputCount; i++)
		{
			psDataReaders->pfnReadUINT16(ppvPCData, &psInputLoad->uFlags);
			psDataReaders->pfnReadUINT16(ppvPCData, &psInputLoad->uTexture);
			psDataReaders->pfnReadUINT16(ppvPCData, &psInputLoad->uCoord);
			psDataReaders->pfnReadUINT16(ppvPCData, &psInputLoad->uCoordDim);
			psDataReaders->pfnReadUINT16(ppvPCData, &psInputLoad->uFormat);
			psDataReaders->pfnReadUINT16(ppvPCData, &psInputLoad->uDataSize);

			psInputLoad++;
		}
	}

	/*
		Read the list of constant mappings for those in memory
	*/
	if	(sPCProgDesc.uMemConstCount > 0)
	{
		PUSP_PC_CONST_LOAD psConstLoad;

		/*
			Load and validate each of the constants to be loaded
		*/
		psConstLoad = psUSPProgDesc->psMemConstLoads;

		for	(i = 0; i < sPCProgDesc.uMemConstCount; i++)
		{
			psDataReaders->pfnReadUINT16(ppvPCData, &psConstLoad->uFormat);

			if	(psConstLoad->uFormat == USP_PC_CONST_FMT_STATIC)
			{
				psDataReaders->pfnReadUINT32(ppvPCData, &psConstLoad->sSrc.uStaticVal);
			}
			else
			{
				psDataReaders->pfnReadUINT16(ppvPCData, &psConstLoad->sSrc.sConst.uSrcIdx);
				if (psConstLoad->uFormat == USP_PC_CONST_FMT_C10)
				{
					psDataReaders->pfnReadUINT16(ppvPCData, &psConstLoad->sSrc.sConst.uSrcShift);
				}
				else
				{
					psConstLoad->sSrc.sConst.uSrcShift = 0;
				}
			}

			psDataReaders->pfnReadUINT16(ppvPCData, &psConstLoad->uDestIdx);
			psDataReaders->pfnReadUINT16(ppvPCData, &psConstLoad->uDestShift);

			switch (psConstLoad->uFormat)
			{
				case USP_PC_CONST_FMT_F32:
				case USP_PC_CONST_FMT_STATIC:
				{
					if	(psConstLoad->uDestShift != 0)
					{
						USP_DBGPRINT(( "ReadPCProgDescBlock: Invalid dest-shift for F32/static const-load\n"));
						goto ReadPCProgDescBlockFinish;
					}
					break;
				}

				case USP_PC_CONST_FMT_F16:
				{
					if	(
							(psConstLoad->uDestShift > 16) ||
							((psConstLoad->uDestShift % 16) != 0)
						)
					{
						USP_DBGPRINT(( "ReadPCProgDescBlock: Invalid dest-shift for F16 const-load\n"));
						goto ReadPCProgDescBlockFinish;
					}
					break;
				}

				case USP_PC_CONST_FMT_C10:
				{
					if	(
							(psConstLoad->uDestShift > 30) ||
							((psConstLoad->uDestShift % 10) != 0)
						)
					{
						USP_DBGPRINT(( "ReadPCProgDescBlock: Invalid dest-shift for C10 const-load\n"));
						goto ReadPCProgDescBlockFinish;
					}
					break;
				}
				
				case USP_PC_CONST_FMT_U8:
				{
					if	(
							(psConstLoad->uDestShift > 24) ||
							((psConstLoad->uDestShift % 8) != 0)
						)
					{
						USP_DBGPRINT(( "ReadPCProgDescBlock: Invalid dest-shift for U8 const-load\n"));
						goto ReadPCProgDescBlockFinish;
					}
					break;
				}

				default:
				{
					USP_DBGPRINT(( "ReadPCProgDescBlock: Invalid format for const-load\n"));
					goto ReadPCProgDescBlockFinish;
				}
			}

			psConstLoad++;
		}
	}

	/*
		Read the list of constant mappings for those in SA registers
	*/
	if	(sPCProgDesc.uRegConstCount > 0)
	{
		PUSP_PC_CONST_LOAD psConstLoad;

		/*
			Load and validate each of the constants to be loaded
		*/
		psConstLoad = psUSPProgDesc->psRegConstLoads;

		for	(i = 0; i < sPCProgDesc.uRegConstCount; i++)
		{
			psDataReaders->pfnReadUINT16(ppvPCData, &psConstLoad->uFormat);

			if	(psConstLoad->uFormat == USP_PC_CONST_FMT_STATIC)
			{
				psDataReaders->pfnReadUINT32(ppvPCData, &psConstLoad->sSrc.uStaticVal);
			}
			else
			{
				psDataReaders->pfnReadUINT16(ppvPCData, &psConstLoad->sSrc.sConst.uSrcIdx);
				if (psConstLoad->uFormat == USP_PC_CONST_FMT_C10)
				{
					psDataReaders->pfnReadUINT16(ppvPCData, &psConstLoad->sSrc.sConst.uSrcShift);
				}
				else
				{
					psConstLoad->sSrc.sConst.uSrcShift = 0;
				}
			}

			psDataReaders->pfnReadUINT16(ppvPCData, &psConstLoad->uDestIdx);
			psDataReaders->pfnReadUINT16(ppvPCData, &psConstLoad->uDestShift);

			switch (psConstLoad->uFormat)
			{
				case USP_PC_CONST_FMT_F32:
				case USP_PC_CONST_FMT_STATIC:
				{
					if	(psConstLoad->uDestShift != 0)
					{
						USP_DBGPRINT(( "ReadPCProgDescBlock: Invalid dest-shift for F32/static const-load\n"));
						goto ReadPCProgDescBlockFinish;
					}
					break;
				}

				case USP_PC_CONST_FMT_F16:
				{
					if	(
							(psConstLoad->uDestShift > 16) ||
							((psConstLoad->uDestShift % 16) != 0)
						)
					{
						USP_DBGPRINT(( "ReadPCProgDescBlock: Invalid dest-shift for F16 const-load\n"));
						goto ReadPCProgDescBlockFinish;
					}
					break;
				}

				case USP_PC_CONST_FMT_C10:
				{
					if	(
							(psConstLoad->uDestShift > 30) ||
							((psConstLoad->uDestShift % 10) != 0)
						)
					{
						USP_DBGPRINT(( "ReadPCProgDescBlock: Invalid dest-shift for C10 const-load\n"));
						goto ReadPCProgDescBlockFinish;
					}
					break;
				}

				case USP_PC_CONST_FMT_U8:
				{
					if	(
							(psConstLoad->uDestShift > 24) ||
							((psConstLoad->uDestShift % 8) != 0)
						)
					{
						USP_DBGPRINT(( "ReadPCProgDescBlock: Invalid dest-shift for U8 const-load\n"));
						goto ReadPCProgDescBlockFinish;
					}
					break;
				}

				default:
				{
					USP_DBGPRINT(( "ReadPCProgDescBlock: Invalid format for const-load\n"));
					goto ReadPCProgDescBlockFinish;
				}
			}

			psConstLoad++;
		}
	}

	/*
		Read the list of texture-state to be loaded into SA registers
	*/
	if	(sPCProgDesc.uRegTexStateCount > 0)
	{
		PUSP_PC_TEXSTATE_LOAD psTexStateLoad;

		/*
			Load and validate each of the sets of texture-state to be loaded
		*/
		psTexStateLoad = psUSPProgDesc->psRegTexStateLoads;

		for	(i = 0; i < sPCProgDesc.uRegTexStateCount; i++)
		{
			psDataReaders->pfnReadUINT16(ppvPCData, &psTexStateLoad->uTextureIdx);
			psDataReaders->pfnReadUINT16(ppvPCData, &psTexStateLoad->uChunkIdx);
			psDataReaders->pfnReadUINT16(ppvPCData, &psTexStateLoad->uDestIdx);

			if	(psTexStateLoad->uTextureIdx > USP_MAX_SAMPLER_IDX)
			{
				USP_DBGPRINT(( "ReadPCProgDescBlock: Invalid texture index in tex-state load\n"));
				goto ReadPCProgDescBlockFinish;
			}

			if	(psTexStateLoad->uChunkIdx > USP_MAX_TEXTURE_CHUNKS)
			{
				USP_DBGPRINT(( "ReadPCProgDescBlock: Invalid chunk index in tex-state load\n"));
				goto ReadPCProgDescBlockFinish;
			}

			psTexStateLoad++;
		}
	}

	/*
		Read the list of pre-compiled instructions for the SA update program
	*/
	if	(sPCProgDesc.uSAUpdateInstCount > 0)
	{
		PHW_INST psHWInst;

		/*
			Load the inst of HW instructions
		*/
		psHWInst = psUSPProgDesc->psSAUpdateInsts;

		for	(i = 0; i < sPCProgDesc.uSAUpdateInstCount; i++)
		{
			psDataReaders->pfnReadUINT32(ppvPCData, &psHWInst->uWord0);
			psDataReaders->pfnReadUINT32(ppvPCData, &psHWInst->uWord1);

			psHWInst++;
		}
	}

	/*
		Read the VS input PA-register usage into (only prsent for vertex-shaders)
	*/
	if	(psUSPProgDesc->uShaderType == USP_PC_SHADERTYPE_VERTEX)
	{
		psUSPProgDesc->uVSInputPARegCount = sPCProgDesc.uVSInputPARegCount;

		for	(i = 0; i < ((IMG_UINT32)sPCProgDesc.uVSInputPARegCount+0x1F)>>5; i++)
		{
			psDataReaders->pfnReadUINT32(ppvPCData, &psUSPProgDesc->auVSInputsUsed[i]);	
		}
	}
	else
	{
		psUSPProgDesc->uVSInputPARegCount = 0;
	}

	/* 
		Read Shader Outputs that are Valid After Compilation.
	*/
	{
		{
			IMG_UINT32 uOutputsCountInDwords;
			IMG_UINT32 uBitsInLastDword;
			IMG_UINT32 uDwordIdx;
			uOutputsCountInDwords = ((IMG_UINT32)sPCProgDesc.uShaderOutputsCount) / 32;
			for(uDwordIdx = 0; uDwordIdx < uOutputsCountInDwords; uDwordIdx++)
			{
				psDataReaders->pfnReadUINT32(ppvPCData, &psUSPProgDesc->puValidShaderOutputs[uDwordIdx]);
			}
			uBitsInLastDword =((IMG_UINT32)sPCProgDesc.uShaderOutputsCount) % 32;
			if(uBitsInLastDword)
			{
				IMG_UINT32	uBitMask;				
				uBitMask = ((IMG_UINT32)(-1));
				uBitMask = (uBitMask << uBitsInLastDword);
				uBitMask = ~uBitMask;				
				psDataReaders->pfnReadUINT32(ppvPCData, &(psUSPProgDesc->puValidShaderOutputs[uOutputsCountInDwords]));
				psUSPProgDesc->puValidShaderOutputs[uOutputsCountInDwords] = ((psUSPProgDesc->puValidShaderOutputs[uOutputsCountInDwords]) & uBitMask);
			}
		}
	}

	/*
		All data read OK.	
	*/
	bSuccess = IMG_TRUE;

	/*
		Save the created USP program-description in the shader on success,
		or cleanup
	*/
	ReadPCProgDescBlockFinish:

	if	(bSuccess)
	{
		psShader->psProgDesc = psUSPProgDesc;
	}
	else
	{
		if	(psUSPProgDesc)
		{
			USPProgDescDestroy(psUSPProgDesc, psContext);
		}
	}

	return bSuccess;
}

/*****************************************************************************
 Name:		AddNewTextureFormat

 Purpose:	Records sample formats for each sample instruction and groups 
            them on basis of texture id. It helps to decide the final format
			for texture.

 Inputs:	psContext	- The current USP execution context
            psList      - List to record formats into
			psSample	- The new sample instruction

 Outputs:	None.

 Returns:	None.
*****************************************************************************/
static IMG_VOID AddNewTextureFormat(PUSP_CONTEXT		psContext, 
							        TEX_SAMPLES_LIST	**psList, 
							        PUSP_SAMPLE			psSample)
{
	TEX_SAMPLES_LIST *psFirst = *psList;
	TEX_SAMPLES_LIST *psNode, *psPrev = IMG_NULL;

	if(psFirst)
	{
		psNode = psFirst;
		while(psNode)
		{
			if(psNode->uTexIdx == psSample->uTextureIdx)
			{
				break;
			}
			psPrev = psNode;
			psNode = psNode->psNext;
		}
		if(psNode)
		{
			/* Texture is already used. Record the new format. */
			SAMPLES_LIST* psTexNode;
			psTexNode = psNode->psList;
			while(psTexNode->psNext)
			{
				psTexNode = psTexNode->psNext;
			}
			psTexNode->psNext = (SAMPLES_LIST*)psContext->pfnAlloc(sizeof(SAMPLES_LIST));
			psTexNode->psNext->psNext = IMG_NULL;
			psTexNode->psNext->psSample = psSample;
			psNode->uCount++;
		}
		else
		{
			/* This texture is used first time. Record the format. */
			psNode = (TEX_SAMPLES_LIST*)psContext->pfnAlloc(sizeof(TEX_SAMPLES_LIST));
			psNode->psNext = IMG_NULL;
			psNode->uCount = 1;
			psNode->uTexIdx = psSample->uTextureIdx;
			psNode->psList = (SAMPLES_LIST*)psContext->pfnAlloc(sizeof(SAMPLES_LIST));
			psNode->psList->psNext = IMG_NULL;
			psNode->psList->psSample = psSample;
			psPrev->psNext =  psNode;
		}
	}
	else
	{
		/* First texture.  Record the format. */
		psFirst = (TEX_SAMPLES_LIST*)psContext->pfnAlloc(sizeof(TEX_SAMPLES_LIST));
		psFirst->psNext = IMG_NULL;
		psFirst->uCount = 1;
		psFirst->uTexIdx = psSample->uTextureIdx;
		psFirst->psList = (SAMPLES_LIST*)psContext->pfnAlloc(sizeof(SAMPLES_LIST));
		psFirst->psList->psNext = IMG_NULL;
		psFirst->psList->psSample = psSample;
		*psList = psFirst;
	}
}

/*****************************************************************************
 Name:		ReadPCSampleBlock

 Purpose:	Read precompiled data describing a texture-sample

 Inputs:	psContext		- The current USP context
			psShader		- The USP shader being created
			psDataReaders	- The set of data-reading functions to use
							  (unused here)
			ppvPCData		- Pointer to the precompiled shader data

 Outputs:	ppvPCData		- Updated to point to the next data to be read

 Returns:	TRUE/FALSE on success/failure
*****************************************************************************/
static IMG_BOOL ReadPCSampleBlock(PUSP_CONTEXT		psContext,
								  PUSP_SHADER		psShader,
								  PPC_DATA_READERS	psDataReaders,
								  IMG_PVOID*		ppvPCData)
{
	USP_PC_BLOCK_SAMPLE			sPCSample;
	USP_PC_DR_SAMPLE_DATA		sPCDrSampleData;
	USP_PC_NONDR_SAMPLE_DATA	sPCNonDrSampleData;
	PUSP_SAMPLE					psUSPSample;
	USP_MOESTATE				sMOEState;
	IMG_BOOL					bSuccess;	

	/*
		No USP sampler created yet
	*/
	psUSPSample = IMG_NULL;
	bSuccess	= IMG_FALSE;

	/*
		Read the common precompiled sample data
	*/
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCSample.uFlags);

	ReadPCMOEState(psDataReaders, ppvPCData, &sPCSample.sMOEState);

	psDataReaders->pfnReadUINT32(ppvPCData, &sPCSample.uSmpID);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCSample.sInstDesc.uFlags);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCSample.uTextureIdx);	
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCSample.uSwizzle);	

	psDataReaders->pfnReadUINT16(ppvPCData, &sPCSample.uBaseSampleDestRegType);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCSample.uBaseSampleDestRegNum);

	psDataReaders->pfnReadUINT16(ppvPCData, &sPCSample.uDirectDestRegType);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCSample.uDirectDestRegNum);

	psDataReaders->pfnReadUINT16(ppvPCData, &sPCSample.uSampleTempRegType);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCSample.uSampleTempRegNum);

	psDataReaders->pfnReadUINT16(ppvPCData, &sPCSample.uIRegsLiveBefore);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCSample.uC10IRegsLiveBefore);

	psDataReaders->pfnReadUINT16(ppvPCData, &sPCSample.uTexPrecision);

	/*
		Read the data specific for dependent or non-dependent samples
	*/
	if	(sPCSample.uFlags & USP_PC_BLOCK_SAMPLE_FLAG_NONDR)
	{
		psDataReaders->pfnReadUINT16(ppvPCData, &sPCNonDrSampleData.uCoord);
		psDataReaders->pfnReadUINT16(ppvPCData, &sPCNonDrSampleData.uTexDim);

		psDataReaders->pfnReadUINT16(ppvPCData, &sPCNonDrSampleData.uSmpNdpDirectSrcRegType);
		psDataReaders->pfnReadUINT16(ppvPCData, &sPCNonDrSampleData.uSmpNdpDirectSrcRegNum);

		/* avoid warnings about sPCDrSampleData being potentially uninitialised */
		sPCDrSampleData.auBaseSmpInst[0] = (EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT);
		//TODO: Remove following code
		//sPCDrSampleData.auBaseSmpInst[1] = 0;
		sPCDrSampleData.auBaseSmpInst[1] = 
			(EURASIA_USE1_OP_SMP << EURASIA_USE1_OP_SHIFT) | 
			(EURASIA_USE1_EPRED_ALWAYS << EURASIA_USE1_EPRED_SHIFT) | 
			/*EURASIA_USE1_SKIPINV | TODO: need to verify*/
			EURASIA_USE1_SMP_NOSCHED | 
			(EURASIA_USE1_SMP_LODM_NONE << EURASIA_USE1_SMP_LODM_SHIFT) | 
			EURASIA_USE1_S2BEXT | 
			((1 << EURASIA_USE1_RMSKCNT_SHIFT))/* | EURASIA_USE1_RCNTSEL) TODO: need to verify*/;
	}
	else
	{
		psDataReaders->pfnReadUINT32(ppvPCData, &sPCDrSampleData.auBaseSmpInst[0]);
		psDataReaders->pfnReadUINT32(ppvPCData, &sPCDrSampleData.auBaseSmpInst[1]);

		/* avoid warnings about sPCNonDrSampleData being potentially uninitialised ! */
		sPCNonDrSampleData.uCoord	= 0;
		sPCNonDrSampleData.uTexDim	= 0;
		sPCNonDrSampleData.uSmpNdpDirectSrcRegType	= 0;
		sPCNonDrSampleData.uSmpNdpDirectSrcRegNum	= 0;
	}

	/*
		Allocate a corresponding USP sample data and fill-it using the PC
		sample data
	*/
	if	(!DecodePCMOEState(&sPCSample.sMOEState, &sMOEState))
	{
		USP_DBGPRINT(( "ReadPCSampleBlock: Error decoding PC MOE state\n"));
		goto ReadPCSampleBlockExit;
	}

	psUSPSample = USPSampleCreate(psContext, psShader, &sMOEState);
	if	(!psUSPSample)
	{
		USP_DBGPRINT(( "ReadPCSampleBlock: Failed to create USP sample data\n"));
		goto ReadPCSampleBlockExit;
	}

	psUSPSample->uInstDescFlags = InitUSPInstDescFlags(sPCSample.sInstDesc.uFlags);

	psUSPSample->uIRegsLiveBefore = sPCSample.uIRegsLiveBefore;
	psUSPSample->uC10IRegsLiveBefore = sPCSample.uC10IRegsLiveBefore;

	psUSPSample->uTexPrecision = (IMG_UINT16)GetUSPHWRegFormat(sPCSample.uTexPrecision);

	if	(sPCSample.uSmpID == USP_MAX_SAMPLE_ID)
	{
		USP_DBGPRINT(( "ReadPCSampleBlock: Invalid sample ID (must be 0 to %u)\n", USP_MAX_SAMPLE_ID-1));
		goto ReadPCSampleBlockExit;
	}
	psUSPSample->uSmpID = sPCSample.uSmpID;

	if	(sPCSample.uTextureIdx >= USP_MAX_SAMPLER_IDX)
	{
		USP_DBGPRINT(( "ReadPCSampleBlock: Invalid texture index (must be 0 to %d)\n", USP_MAX_SAMPLER_IDX-1));
		goto ReadPCSampleBlockExit;
	}
	psUSPSample->uTextureIdx = sPCSample.uTextureIdx;

	if	(sPCSample.uSwizzle & ~((1 << (USP_MAX_SAMPLE_CHANS*3)) - 1))
	{
		USP_DBGPRINT(( "ReadPCSampleBlock: Invalid sample swizzle\n"));
		goto ReadPCSampleBlockExit;
	}
	psUSPSample->uOrgSwizzle = sPCSample.uSwizzle;
	psUSPSample->uSwizzle = sPCSample.uSwizzle;

	switch (sPCSample.uBaseSampleDestRegType)
	{
		case USP_PC_SAMPLE_TEMP_REGTYPE_TEMP:
		{
			psUSPSample->eBaseSampleDestRegType = USP_REGTYPE_TEMP;
			break;
		}

		case USP_PC_SAMPLE_TEMP_REGTYPE_PA:
		{
			psUSPSample->eBaseSampleDestRegType = USP_REGTYPE_PA;
			break;
		}
		default:
		{
			USP_DBGPRINT(( "ReadPCSampleBlock: Invalid dest. reg-type\n"));
			goto ReadPCSampleBlockExit;
		}
	}

	psUSPSample->uBaseSampleDestRegNum = sPCSample.uBaseSampleDestRegNum;
	
	switch (sPCSample.uDirectDestRegType)
	{
		case USP_PC_SAMPLE_DEST_REGTYPE_TEMP:
		{
			psUSPSample->eDirectDestRegType = USP_REGTYPE_TEMP;
			break;
		}
		case USP_PC_SAMPLE_DEST_REGTYPE_OUTPUT:
		{
			psUSPSample->eDirectDestRegType = USP_REGTYPE_OUTPUT;
			break;
		}
		case USP_PC_SAMPLE_DEST_REGTYPE_PA:
		{
			psUSPSample->eDirectDestRegType = USP_REGTYPE_PA;
			break;
		}
		case USP_PC_SAMPLE_DEST_REGTYPE_INTERNAL:
		{
			psUSPSample->eDirectDestRegType = USP_REGTYPE_INTERNAL;
			break;
		}
		default:
		{
			USP_DBGPRINT(( "ReadPCSampleBlock: Invalid direct dest reg-type\n"));
			goto ReadPCSampleBlockExit;
		}
	}

	psUSPSample->uDirectDestRegNum = sPCSample.uDirectDestRegNum;

	switch (sPCSample.uSampleTempRegType)
	{
		case USP_PC_SAMPLE_TEMP_REGTYPE_TEMP:
		{
			psUSPSample->eSampleTempRegType = USP_REGTYPE_TEMP;
			break;
		}

		case USP_PC_SAMPLE_TEMP_REGTYPE_PA:
		{
			psUSPSample->eSampleTempRegType = USP_REGTYPE_PA;
			break;
		}

		default:
		{
			USP_DBGPRINT(( "ReadPCSampleBlock: Invalid temp reg-type for use during sampling\n"));
			goto ReadPCSampleBlockExit;
		}
	}

	psUSPSample->uSampleTempRegNum = sPCSample.uSampleTempRegNum;	

	if	(sPCSample.uFlags & USP_PC_BLOCK_SAMPLE_FLAG_NONDR)
	{
		psUSPSample->bOrgNonDependent = IMG_TRUE;
		psUSPSample->bNonDependent = IMG_TRUE;

		switch (sPCNonDrSampleData.uCoord)
		{
			case USP_PC_SAMPLE_COORD_TC0:
			{
				psUSPSample->eCoord = USP_ITERATED_DATA_TYPE_TC0;
				break;
			}
			case USP_PC_SAMPLE_COORD_TC1:
			{
				psUSPSample->eCoord = USP_ITERATED_DATA_TYPE_TC1;
				break;
			}
			case USP_PC_SAMPLE_COORD_TC2:
			{
				psUSPSample->eCoord = USP_ITERATED_DATA_TYPE_TC2;
				break;
			}
			case USP_PC_SAMPLE_COORD_TC3:
			{
				psUSPSample->eCoord = USP_ITERATED_DATA_TYPE_TC3;
				break;
			}
			case USP_PC_SAMPLE_COORD_TC4:
			{
				psUSPSample->eCoord = USP_ITERATED_DATA_TYPE_TC4;
				break;
			}
			case USP_PC_SAMPLE_COORD_TC5:
			{
				psUSPSample->eCoord = USP_ITERATED_DATA_TYPE_TC5;
				break;
			}
			case USP_PC_SAMPLE_COORD_TC6:
			{
				psUSPSample->eCoord = USP_ITERATED_DATA_TYPE_TC6;
				break;
			}
			case USP_PC_SAMPLE_COORD_TC7:
			{
				psUSPSample->eCoord = USP_ITERATED_DATA_TYPE_TC7;
				break;
			}
			case USP_PC_SAMPLE_COORD_TC8:
			{
				psUSPSample->eCoord = USP_ITERATED_DATA_TYPE_TC8;
				break;
			}
			case USP_PC_SAMPLE_COORD_TC9:
			{
				psUSPSample->eCoord = USP_ITERATED_DATA_TYPE_TC9;
				break;
			}
			default:
			{
				USP_DBGPRINT(( "ReadPCSampleBlock: Invalid non-DR tex-coord\n"));
				goto ReadPCSampleBlockExit;
			}
		}

		if	(sPCNonDrSampleData.uTexDim > USP_MAX_TEX_DIM)
		{
			USP_DBGPRINT(( "ReadPCSampleBlock: Invalid non-DR tex-dimension\n"));
			goto ReadPCSampleBlockExit;
		}
		psUSPSample->uTexDim = sPCNonDrSampleData.uTexDim;

		if	(sPCSample.uFlags & USP_PC_BLOCK_SAMPLE_FLAG_PROJECTED)
		{
			psUSPSample->bProjected = IMG_TRUE;
		}
		else
		{
			psUSPSample->bProjected = IMG_FALSE;
		}

		if	(sPCSample.uFlags & USP_PC_BLOCK_SAMPLE_FLAG_CENTROID)
		{
			psUSPSample->bCentroid = IMG_TRUE;
		}
		else
		{
			psUSPSample->bCentroid = IMG_FALSE;
		}

		switch (sPCNonDrSampleData.uSmpNdpDirectSrcRegType)
		{
			case USP_PC_SAMPLE_DEST_REGTYPE_TEMP:
			{
				psUSPSample->eSmpNdpDirectSrcRegType = USP_REGTYPE_TEMP;
				break;
			}
			case USP_PC_SAMPLE_DEST_REGTYPE_OUTPUT:
			{
				psUSPSample->eSmpNdpDirectSrcRegType = USP_REGTYPE_OUTPUT;
				break;
			}
			case USP_PC_SAMPLE_DEST_REGTYPE_PA:
			{
				psUSPSample->eSmpNdpDirectSrcRegType = USP_REGTYPE_PA;
				break;
			}
			case USP_PC_SAMPLE_DEST_REGTYPE_INTERNAL:
			{
				psUSPSample->eSmpNdpDirectSrcRegType = USP_REGTYPE_INTERNAL;
				break;
			}
			default:
			{
				USP_DBGPRINT(( "ReadPCSampleBlock: Invalid direct Src reg-type for Non-Dependent Sample\n"));
				goto ReadPCSampleBlockExit;
			}
		}

		psUSPSample->uSmpNdpDirectSrcRegNum = sPCNonDrSampleData.uSmpNdpDirectSrcRegNum;
	}
	//TODO: Remove following code
	else
	{

		/*
			Decode information about the supplied base SMP
			instruction
		*/
		psUSPSample->bOrgNonDependent   = IMG_FALSE;
		psUSPSample->bNonDependent		= IMG_FALSE;
		psUSPSample->bProjected			= IMG_FALSE;
		psUSPSample->bCentroid			= IMG_FALSE;
		psUSPSample->eSmpNdpDirectSrcRegType	= USP_REGTYPE_TEMP;
		psUSPSample->uSmpNdpDirectSrcRegNum		= 0;
	}
	{
		USP_OPCODE	eBaseInstOpcode;
		psUSPSample->sBaseInst.uWord0	= sPCDrSampleData.auBaseSmpInst[0];
		psUSPSample->sBaseInst.uWord1	= sPCDrSampleData.auBaseSmpInst[1];

		if	(!HWInstGetOpcode(&psUSPSample->sBaseInst, &eBaseInstOpcode))
		{
			USP_DBGPRINT(("ReadPCSampleBlock: Error decoding opcode of base SMP inst\n"));
			goto ReadPCSampleBlockExit;
		}

		switch (eBaseInstOpcode)
		{
			case USP_OPCODE_SMP:
			case USP_OPCODE_SMPBIAS:
			case USP_OPCODE_SMPREPLACE:
			case USP_OPCODE_SMPGRAD:
			{
				psUSPSample->eBaseInstOpcode = eBaseInstOpcode;
				break;
			}

			default:
			{
				USP_DBGPRINT(("ReadPCSampleBlock: Invalid base SMP inst\n"));
				goto ReadPCSampleBlockExit;
			}
		}

		psUSPSample->uBaseInstDRCNum = HWInstDecodeSMPInstDRCNum(&psUSPSample->sBaseInst);

		if	(!HWInstGetOperandsUsed(eBaseInstOpcode,
									&psUSPSample->sBaseInst,
									&psUSPSample->uBaseInstOperandsUsed))
		{
			USP_DBGPRINT(("ReadPCSampleBlock: Error getting operands used by smp-inst\n"));
			return IMG_FALSE;
		}
	}

	/*
		Add the new sample to the list of all those within the shader
	*/
	USPShaderAddSample(psShader, psUSPSample);

	/*
		Record all texture samples for final texture format selection
	*/
	AddNewTextureFormat(psContext, &(psShader->psUsedTexFormats), 
		psUSPSample);

	/*
		Add the instruction-block containing the sample instructions to
		the shader
	*/
	USPShaderAddInstBlock(psShader, psUSPSample->psInstBlock);

	/*
		USP Shader created OK
	*/
	bSuccess = IMG_TRUE;

	ReadPCSampleBlockExit:

	/*
		Cleanup the created USP sample block on failure
	*/
	if	(!bSuccess)
	{
		if	(psUSPSample)
		{
			USPSampleDestroy(psUSPSample, psContext);
		}
	}

	return bSuccess;
}

/*****************************************************************************
 Name:		ReadPCSampleUnpackBlock

 Purpose:	Read precompiled data describing a texture-sample unpack

 Inputs:	psContext		- The current USP context
			psShader		- The USP shader being created
			psDataReaders	- The set of data-reading functions to use
							  (unused here)
			ppvPCData		- Pointer to the precompiled shader data

 Outputs:	ppvPCData		- Updated to point to the next data to be read

 Returns:	TRUE/FALSE on success/failure
*****************************************************************************/
static IMG_BOOL ReadPCSampleUnpackBlock(	PUSP_CONTEXT		psContext,
											PUSP_SHADER			psShader,
											PPC_DATA_READERS	psDataReaders,
											IMG_PVOID*			ppvPCData)
{
	USP_PC_BLOCK_SAMPLEUNPACK	sPCSampleUnpack;		
	PUSP_SAMPLEUNPACK			psUSPSampleUnpack;
	USP_MOESTATE				sMOEState;
	IMG_BOOL					bSuccess;
	IMG_UINT32					i;

	/*
		No USP sampler unpack created yet
	*/
	psUSPSampleUnpack = IMG_NULL;
	bSuccess	= IMG_FALSE;

	ReadPCMOEState(psDataReaders, ppvPCData, &sPCSampleUnpack.sMOEState);

	psDataReaders->pfnReadUINT32(ppvPCData, &sPCSampleUnpack.uSmpID);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCSampleUnpack.sInstDesc.uFlags);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCSampleUnpack.uMask);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCSampleUnpack.uLive);	

	for	(i = 0; i < USP_MAX_TEX_CHANNELS; i++)
	{
		psDataReaders->pfnReadUINT16(ppvPCData, &sPCSampleUnpack.auDestRegType[i]);
	}
	for	(i = 0; i < USP_MAX_TEX_CHANNELS; i++)
	{
		psDataReaders->pfnReadUINT16(ppvPCData, &sPCSampleUnpack.auDestRegNum[i]);
	}
	for	(i = 0; i < USP_MAX_TEX_CHANNELS; i++)
	{
		psDataReaders->pfnReadUINT16(ppvPCData, &sPCSampleUnpack.auDestRegFmt[i]);
	}
	for	(i = 0; i < USP_MAX_TEX_CHANNELS; i++)
	{
		psDataReaders->pfnReadUINT16(ppvPCData, &sPCSampleUnpack.auDestRegComp[i]);
	}
	
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCSampleUnpack.uDirectSrcRegType);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCSampleUnpack.uDirectSrcRegNum);

	psDataReaders->pfnReadUINT16(ppvPCData, &sPCSampleUnpack.uBaseSampleUnpackSrcRegType);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCSampleUnpack.uBaseSampleUnpackSrcRegNum);

	psDataReaders->pfnReadUINT16(ppvPCData, &sPCSampleUnpack.uSampleUnpackTempRegType);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCSampleUnpack.uSampleUnpackTempRegNum);
	
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCSampleUnpack.uIRegsLiveBefore);

	/*
		Allocate a corresponding USP sample data and fill-it using the PC
		sample data
	*/
	if	(!DecodePCMOEState(&sPCSampleUnpack.sMOEState, &sMOEState))
	{
		USP_DBGPRINT(( "ReadPCSampleUnpackBlock: Error decoding PC MOE state\n"));
		goto ReadPCSampleUnpackBlockExit;
	}

	psUSPSampleUnpack = USPSampleUnpackCreate(psContext, psShader, &sMOEState);
	if	(!psUSPSampleUnpack)
	{
		USP_DBGPRINT(( "ReadPCSampleUnpackBlock: Failed to create USP sample unpack data\n"));
		goto ReadPCSampleUnpackBlockExit;
	}

	psUSPSampleUnpack->uInstDescFlags = InitUSPInstDescFlags(sPCSampleUnpack.sInstDesc.uFlags);
	
	psUSPSampleUnpack->uIRegsLiveBefore = sPCSampleUnpack.uIRegsLiveBefore;

	if	(sPCSampleUnpack.uSmpID == USP_MAX_SAMPLE_ID)
	{
		USP_DBGPRINT(( "ReadPCSampleUnpackBlock: Invalid sample ID (must be 0 to %u)\n", USP_MAX_SAMPLE_ID-1));
		goto ReadPCSampleUnpackBlockExit;
	}
	psUSPSampleUnpack->uSmpID = sPCSampleUnpack.uSmpID;

	if	(sPCSampleUnpack.uLive & ~((1 << USP_MAX_SAMPLE_CHANS) - 1))
	{
		USP_DBGPRINT(( "ReadPCSampleUnpackBlock: Invalid sample unpack dest liveness\n"));
		goto ReadPCSampleUnpackBlockExit;
	}
	psUSPSampleUnpack->uLive = sPCSampleUnpack.uLive;

	if	(sPCSampleUnpack.uMask & ~sPCSampleUnpack.uLive)
	{
		USP_DBGPRINT(( "ReadPCSampleUnpackBlock: Invalid sample unpack dest mask\n"));
		goto ReadPCSampleUnpackBlockExit;
	}	
	psUSPSampleUnpack->uMask = sPCSampleUnpack.uMask;
	

	for	(i = 0; i < USP_MAX_TEX_CHANNELS; i++)
	{
		PUSP_REG psDest;

		if	(!(sPCSampleUnpack.uMask & (1 << i)))
		{
			continue;
		}
		
		psDest = &psUSPSampleUnpack->asDest[i];

		switch (sPCSampleUnpack.auDestRegType[i])
		{
			case USP_PC_SAMPLE_DEST_REGTYPE_TEMP:
			{
				psDest->eType = USP_REGTYPE_TEMP;
				break;
			}
			case USP_PC_SAMPLE_DEST_REGTYPE_OUTPUT:
			{
				psDest->eType = USP_REGTYPE_OUTPUT;
				break;
			}
			case USP_PC_SAMPLE_DEST_REGTYPE_PA:
			{
				psDest->eType = USP_REGTYPE_PA;
				break;
			}
			case USP_PC_SAMPLE_DEST_REGTYPE_INTERNAL:
			{
				psDest->eType = USP_REGTYPE_INTERNAL;
				break;
			}
			default:
			{
				USP_DBGPRINT(( "ReadPCSampleUnpackBlock: Invalid dest reg-type\n"));
				goto ReadPCSampleUnpackBlockExit;
			}
		}

		switch (sPCSampleUnpack.auDestRegFmt[i])
		{
			case USP_PC_SAMPLE_DEST_REGFMT_F32:
			{
				if	(sPCSampleUnpack.auDestRegComp[i] != 0)
				{
					USP_DBGPRINT(( "ReadPCSampleBlock: Invalid F32 dest component index\n"));
					goto ReadPCSampleUnpackBlockExit;
				}
				psDest->eFmt = USP_REGFMT_F32;
				break;
			}

			case USP_PC_SAMPLE_DEST_REGFMT_F16:
			{
				if	(
						(sPCSampleUnpack.auDestRegComp[i] != 0) &&
						(sPCSampleUnpack.auDestRegComp[i] != 2)
					)
				{
					USP_DBGPRINT(( "ReadPCSampleUnpackBlock: Invalid F16 dest component index\n"));
					goto ReadPCSampleUnpackBlockExit;
				}
				psDest->eFmt = USP_REGFMT_F16;
				break;
			}

			case USP_PC_SAMPLE_DEST_REGFMT_C10:
			{
				IMG_UINT16	uRegMaxComp;

				#if !defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
				if	(psDest->eType != USP_REGTYPE_INTERNAL)
				{
					uRegMaxComp = 2;
				}
				else
				#endif /* !defined(SGX_FEATURE_UNIFIED_STORE_64BITS) */
				{
					uRegMaxComp = 3;
				}
				if	(sPCSampleUnpack.auDestRegComp[i] > uRegMaxComp)
				{
					USP_DBGPRINT(( "ReadPCSampleUnpackBlock: Invalid C10 dest component index\n"));
					goto ReadPCSampleUnpackBlockExit;
				}
				psDest->eFmt = USP_REGFMT_C10;
				break;
			}

			case USP_PC_SAMPLE_DEST_REGFMT_U8:
			{
				if	(sPCSampleUnpack.auDestRegComp[i] > 3)
				{
					USP_DBGPRINT(( "ReadPCSampleUnpackBlock: Invalid U8 dest component index\n"));
					goto ReadPCSampleUnpackBlockExit;
				}
				psDest->eFmt = USP_REGFMT_U8;
				break;
			}

			default:
			{
				USP_DBGPRINT(( "ReadPCSampleUnpackBlock: Invalid dest format\n"));
				goto ReadPCSampleUnpackBlockExit;
			}
		}

		psDest->uNum	= sPCSampleUnpack.auDestRegNum[i];
		psDest->uComp	= sPCSampleUnpack.auDestRegComp[i];
		psDest->eDynIdx	= USP_DYNIDX_NONE;
	}

	switch (sPCSampleUnpack.uDirectSrcRegType)
	{
		case USP_PC_SAMPLE_DEST_REGTYPE_TEMP:
		{
			psUSPSampleUnpack->eDirectSrcRegType = USP_REGTYPE_TEMP;
			break;
		}
		case USP_PC_SAMPLE_DEST_REGTYPE_OUTPUT:
		{
			psUSPSampleUnpack->eDirectSrcRegType = USP_REGTYPE_OUTPUT;
			break;
		}
		case USP_PC_SAMPLE_DEST_REGTYPE_PA:
		{
			psUSPSampleUnpack->eDirectSrcRegType = USP_REGTYPE_PA;
			break;
		}
		case USP_PC_SAMPLE_DEST_REGTYPE_INTERNAL:
		{
			psUSPSampleUnpack->eDirectSrcRegType = USP_REGTYPE_INTERNAL;
			break;
		}
		default:
		{
			USP_DBGPRINT(( "ReadPCSampleUnpackBlock: Invalid direct src reg-type\n"));
			goto ReadPCSampleUnpackBlockExit;
		}
	}

	psUSPSampleUnpack->uDirectSrcRegNum = sPCSampleUnpack.uDirectSrcRegNum;

	switch (sPCSampleUnpack.uBaseSampleUnpackSrcRegType)
	{
		case USP_PC_SAMPLE_TEMP_REGTYPE_TEMP:
		{
			psUSPSampleUnpack->eBaseSampleUnpackSrcRegType = USP_REGTYPE_TEMP;
			break;
		}

		case USP_PC_SAMPLE_TEMP_REGTYPE_PA:
		{
			psUSPSampleUnpack->eBaseSampleUnpackSrcRegType = USP_REGTYPE_PA;
			break;
		}
		default:
		{
			USP_DBGPRINT(( "ReadPCSampleUnpackBlock: Invalid temp reg-type for base sample source\n"));
			goto ReadPCSampleUnpackBlockExit;
		}
	}

	psUSPSampleUnpack->uBaseSampleUnpackSrcRegNum = sPCSampleUnpack.uBaseSampleUnpackSrcRegNum;	

	switch (sPCSampleUnpack.uSampleUnpackTempRegType)
	{
		case USP_PC_SAMPLE_TEMP_REGTYPE_TEMP:
		{
			psUSPSampleUnpack->eSampleUnpackTempRegType = USP_REGTYPE_TEMP;
			break;
		}

		case USP_PC_SAMPLE_TEMP_REGTYPE_PA:
		{
			psUSPSampleUnpack->eSampleUnpackTempRegType = USP_REGTYPE_PA;
			break;
		}
		default:
		{
			USP_DBGPRINT(( "ReadPCSampleUnpackBlock: Invalid temp reg-type\n"));
			goto ReadPCSampleUnpackBlockExit;
		}
	}

	psUSPSampleUnpack->uSampleUnpackTempRegNum = sPCSampleUnpack.uSampleUnpackTempRegNum;

	
	/*
		Add the new sample unpack to the list of all those within the shader
	*/
	USPShaderAddSampleUnpack(psShader, psUSPSampleUnpack);	

	/*
		Add the instruction-block containing the sample instructions to
		the shader
	*/
	USPShaderAddInstBlock(psShader, psUSPSampleUnpack->psInstBlock);

	/*
		USP Shader created OK
	*/
	bSuccess = IMG_TRUE;

	ReadPCSampleUnpackBlockExit:

	/*
		Cleanup the created USP sample block on failure
	*/
	if	(!bSuccess)
	{
		if	(psUSPSampleUnpack)
		{
			USPSampleUnpackDestroy(psUSPSampleUnpack, psContext);
		}
	}

	return bSuccess;
}

/*****************************************************************************
 Name:		ReadPCBranchBlock

 Purpose:	Read precompiled data describing a branch/call

 Inputs:	psContext		- The current USP context
			psShader		- The USP shader being created
			psDataReaders	- The set of data-reading functions to use
							  (unused here)
			ppvPCData		- Pointer to the precompiled shader data

 Outputs:	ppvPCData		- Updated to point to the next data to be read

 Returns:	TRUE/FALSE on success/failure
*****************************************************************************/
static IMG_BOOL ReadPCBranchBlock(PUSP_CONTEXT		psContext,
								  PUSP_SHADER		psShader,
								  PPC_DATA_READERS	psDataReaders,
								  IMG_PVOID*		ppvPCData)
{
	USP_PC_BLOCK_BRANCH	sPCBranch;
	PUSP_BRANCH			psUSPBranch;
	IMG_BOOL			bFailed;

	/*
		Branch not created yet
	*/
	bFailed		= IMG_TRUE;
	psUSPBranch	= IMG_NULL;

	/*
		Read the precompiled branch data

		NB: We cannot verify the supplied label ID until the entire
			shader has been read.
	*/
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCBranch.auBaseBranchInst[0]);
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCBranch.auBaseBranchInst[1]);
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCBranch.uDestLabelID);

	/*
		Create and initialise corresponding USP branch info
	*/
	psUSPBranch = USPBranchCreate(psContext, 
								  sPCBranch.uDestLabelID,
								  psShader);
	if	(!psUSPBranch)
	{
		USP_DBGPRINT(("ReadPCBranchBlock: Failed to create USP branch info\n"));
		goto ReadPCBranchBlockExit;
	}

	/*
		Copy the base HW branch instruction into the associated instruction
		block.
	*/
	if	(!USPInstBlockAddPCInst(psUSPBranch->psInstBlock,
								(PHW_INST)sPCBranch.auBaseBranchInst,
								0,
								psContext,
								IMG_NULL))
	{
		USP_DBGPRINT(("ReadPCBranchBlock: Error adding PC inst to block\n"));
		goto ReadPCBranchBlockExit;
	}

	/*
		Add the created branch to the list of all those within the shader
	*/
	USPShaderAddBranch(psShader, psUSPBranch);

	/*
		Add the instruction block for the branch to the shader too
	*/
	USPShaderAddInstBlock(psShader, psUSPBranch->psInstBlock);

	/*
		Branch-block setup OK.
	*/
	bFailed = IMG_FALSE;

	ReadPCBranchBlockExit:

	/*
		Cleanup on error
	*/
	if	(bFailed)
	{
		if	(psUSPBranch)
		{
			USPBranchDestroy(psUSPBranch, psContext);
			psUSPBranch = IMG_NULL;
		}
	}

	return (IMG_BOOL)!bFailed;
}

/*****************************************************************************
 Name:		WriteUSPMOEState

 Purpose:	Decodes USP shader MOE state and writes to pre-compiled shader.

 Inputs:	psContext		- The current USP context
                              (unused here)
			psDataWriters	- The set of data-writing functions to use
			ppvPCData		- Pointer to the precompiled shader data
			psUSPMOEState   - USP shader MOE state.

 Outputs:	ppvPCData		- Updated to point to the next data to be written

 Returns:	TRUE/FALSE on success/failure
*****************************************************************************/
static IMG_BOOL WriteUSPMOEState(PUSP_CONTEXT	   psContext,
						         PPC_DATA_WRITERS  psDataWriters,
						         IMG_PVOID*		   ppvPCData,
						         PUSP_MOESTATE	   psUSPMOEState)
{
	IMG_UINT16	uMOEFmtCtlFlags = 0;
	IMG_UINT32  i;
	IMG_UINT32	uIncOrSwiz;

	USP_UNREFERENCED_PARAMETER(psContext);

	if(psUSPMOEState->bEFOFmtCtl)
	{
		uMOEFmtCtlFlags |= USP_PC_MOE_FMTCTLFLAG_EFOFMTCTL;
	}
	if(psUSPMOEState->bColFmtCtl)
	{
		uMOEFmtCtlFlags |= USP_PC_MOE_FMTCTLFLAG_COLFMTCTL;
	}
	psDataWriters->pfnWriteUINT16(ppvPCData, uMOEFmtCtlFlags);

	for	(i = 0; i < USP_PC_MOE_OPERAND_COUNT; i++)
	{
		uIncOrSwiz = 0;
		if(psUSPMOEState->abUseSwiz[i])
		{
			uIncOrSwiz = (psUSPMOEState->auSwiz[i]) << USP_PC_MOE_INCORSWIZ_SWIZ_SHIFT;
			uIncOrSwiz |= USP_PC_MOE_INCORSWIZ_MODE_SWIZ;
		}
		else
		{
			uIncOrSwiz = (psUSPMOEState->aiInc[i]) << USP_PC_MOE_INCORSWIZ_INC_SHIFT;
			uIncOrSwiz &= 0x00FF;
			uIncOrSwiz |= USP_PC_MOE_INCORSWIZ_MODE_INC;
		}
		psDataWriters->pfnWriteUINT16(ppvPCData, (IMG_UINT16)uIncOrSwiz);
	}
	for	(i = 0; i < USP_PC_MOE_OPERAND_COUNT; i++)
	{
		psDataWriters->pfnWriteUINT16(ppvPCData, ((IMG_UINT16)(psUSPMOEState->auBaseOffset[i])));
	}
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		InitPCInstDescFlags

 Purpose:	Computes the pre-compiled shader instruction flags from given USP
			shader instrcution flags.

 Inputs:	uInstDescFlags  - The USP shader instruction falgs.

 Returns:	Pre-compiled shader instruction flags.
*****************************************************************************/
static IMG_UINT32 InitPCInstDescFlags(IMG_UINT32  uInstDescFlags)
{
	/* Reset instruction description flags */
	IMG_UINT32 uPCInstDescFlags = 0;

	if(uInstDescFlags & USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE)
	{
		 uPCInstDescFlags |= USP_PC_INSTDESC_FLAG_IREGS_LIVE_BEFORE;
	}	
	if(uInstDescFlags & USP_INSTDESC_FLAG_DONT_SKIPINV)
	{
		uPCInstDescFlags |= USP_PC_INSTDESC_FLAG_DONT_SKIPINV;
	}
	if(uInstDescFlags & USP_INSTDESC_FLAG_DEST_USES_RESULT)
	{
		uPCInstDescFlags |= USP_PC_INSTDESC_FLAG_DEST_USES_RESULT;
	}
	if(uInstDescFlags & USP_INSTDESC_FLAG_SRC0_USES_RESULT)
	{
		uPCInstDescFlags |= USP_PC_INSTDESC_FLAG_SRC0_USES_RESULT;
	}
	if(uInstDescFlags & USP_INSTDESC_FLAG_SRC1_USES_RESULT)
	{
		uPCInstDescFlags |= USP_PC_INSTDESC_FLAG_SRC1_USES_RESULT;
	}
	if(uInstDescFlags & USP_INSTDESC_FLAG_SRC2_USES_RESULT)
	{
		uPCInstDescFlags |= USP_PC_INSTDESC_FLAG_SRC2_USES_RESULT;
	}

	return uPCInstDescFlags;
}

/*****************************************************************************
 Name:		WriteUSPSampleBlock

 Purpose:	Write USP sample block to pre-compiled data.

 Inputs:	psContext		- The current USP context
			psShader		- The USP shader being read
			                  (unused here)
			psDataWriters	- The set of data-writing functions to use
			ppvPCData		- Pointer to the precompiled shader data
			psUSPSample     - USP Sample block to write

 Outputs:	ppvPCData		- Updated to point to the next data to be written

 Returns:	TRUE/FALSE on success/failure
*****************************************************************************/
static IMG_BOOL WriteUSPSampleBlock( PUSP_CONTEXT	   psContext, 
									 PUSP_SHADER	   psShader,
									 PPC_DATA_WRITERS  psDataWriters, 
									 IMG_PVOID*		   ppvPCData,
									 PUSP_SAMPLE	   psUSPSample)
{
	IMG_BOOL    bSuccess = IMG_FALSE;
	IMG_UINT16  uFlags = 0;
	IMG_UINT16	uRegType = 0;	

	USP_UNREFERENCED_PARAMETER(psShader);

	if(psUSPSample->bOrgNonDependent)
	{
		uFlags |= USP_PC_BLOCK_SAMPLE_FLAG_NONDR;
	}
	if(psUSPSample->bProjected)
	{
		uFlags |= USP_PC_BLOCK_SAMPLE_FLAG_PROJECTED;
	}
	if(psUSPSample->bCentroid)
	{
		uFlags |= USP_PC_BLOCK_SAMPLE_FLAG_CENTROID;
	}

	/*
		Write the common precompiled sample data
	*/
	psDataWriters->pfnWriteUINT16(ppvPCData, uFlags);

	/* Decode and write MOE state */
	if(!WriteUSPMOEState(psContext, psDataWriters, ppvPCData, &(psUSPSample->sMOEState)))
	{
		goto WriteUSPSampleBlockExit;
	}

	psDataWriters->pfnWriteUINT32(ppvPCData, psUSPSample->uSmpID);
	uFlags = (IMG_UINT16)InitPCInstDescFlags(psUSPSample->uInstDescFlags);
	psDataWriters->pfnWriteUINT16(ppvPCData, uFlags);

	psDataWriters->pfnWriteUINT16(ppvPCData, (IMG_UINT16)(psUSPSample->uTextureIdx));
	psDataWriters->pfnWriteUINT16(ppvPCData, (psUSPSample->uOrgSwizzle));

	switch (psUSPSample->eBaseSampleDestRegType)
	{
		case USP_REGTYPE_TEMP:
		{
			uRegType = USP_PC_SAMPLE_TEMP_REGTYPE_TEMP;
			break;
		}

		case USP_REGTYPE_PA:
		{
			uRegType = USP_PC_SAMPLE_TEMP_REGTYPE_PA;
			break;
		}
		default:
		{
			USP_DBGPRINT(( "WriteUSPSampleBlock: Invalid dest reg-type\n"));
			goto WriteUSPSampleBlockExit;
		}
	}
	psDataWriters->pfnWriteUINT16(ppvPCData, uRegType);
	psDataWriters->pfnWriteUINT16(ppvPCData, ((IMG_UINT16)(psUSPSample->uBaseSampleDestRegNum)));
	
	switch (psUSPSample->eDirectDestRegType)
	{
		case USP_REGTYPE_TEMP:
		{
			uRegType = USP_PC_SAMPLE_TEMP_REGTYPE_TEMP;
			break;
		}

		case USP_REGTYPE_OUTPUT:
		{
			uRegType = USP_PC_SAMPLE_DEST_REGTYPE_OUTPUT;
		}

		case USP_REGTYPE_PA:
		{
			uRegType = USP_PC_SAMPLE_TEMP_REGTYPE_PA;
			break;
		}
		case USP_REGTYPE_INTERNAL:
		{
			uRegType = USP_PC_SAMPLE_DEST_REGTYPE_INTERNAL;
		}

		default:
		{
			USP_DBGPRINT(( "WriteUSPSampleBlock: Invalid direct dest reg-type\n"));
			goto WriteUSPSampleBlockExit;
		}
	}
	psDataWriters->pfnWriteUINT16(ppvPCData, uRegType);
	psDataWriters->pfnWriteUINT16(ppvPCData, ((IMG_UINT16)(psUSPSample->uDirectDestRegNum)));

	switch (psUSPSample->eSampleTempRegType)
	{
		case USP_REGTYPE_TEMP:
		{
			uRegType = USP_PC_SAMPLE_TEMP_REGTYPE_TEMP;
			break;
		}

		case USP_REGTYPE_PA:
		{
			uRegType = USP_PC_SAMPLE_TEMP_REGTYPE_PA;
			break;
		}
		default:
		{
			USP_DBGPRINT(( "WriteUSPSampleBlock: Invalid temp reg-type\n"));
			goto WriteUSPSampleBlockExit;
		}
	}
	psDataWriters->pfnWriteUINT16(ppvPCData, uRegType);
	psDataWriters->pfnWriteUINT16(ppvPCData, ((IMG_UINT16)(psUSPSample->uSampleTempRegNum)));

	psDataWriters->pfnWriteUINT16(ppvPCData, (psUSPSample->uIRegsLiveBefore));
	psDataWriters->pfnWriteUINT16(ppvPCData, (psUSPSample->uC10IRegsLiveBefore));
	psDataWriters->pfnWriteUINT16(ppvPCData, (psUSPSample->uTexPrecision));

	/* Write tex-coord */
	if(psUSPSample->bOrgNonDependent)
	{
		IMG_UINT32 uCoord;
		switch (psUSPSample->eCoord)
		{
			case USP_ITERATED_DATA_TYPE_TC0:
			{
				uCoord = USP_PC_SAMPLE_COORD_TC0;
				break;
			}
			case USP_ITERATED_DATA_TYPE_TC1:
			{
				uCoord = USP_PC_SAMPLE_COORD_TC1;
				break;
			}
			case USP_ITERATED_DATA_TYPE_TC2:
			{
				uCoord = USP_PC_SAMPLE_COORD_TC2;
				break;
			}
			case USP_ITERATED_DATA_TYPE_TC3:
			{
				uCoord = USP_PC_SAMPLE_COORD_TC3;
				break;
			}
			case USP_ITERATED_DATA_TYPE_TC4:
			{
				uCoord = USP_PC_SAMPLE_COORD_TC4;
				break;
			}
			case USP_ITERATED_DATA_TYPE_TC5:
			{
				uCoord = USP_PC_SAMPLE_COORD_TC5;
				break;
			}
			case USP_ITERATED_DATA_TYPE_TC6:
			{
				uCoord = USP_PC_SAMPLE_COORD_TC6;
				break;
			}
			case USP_ITERATED_DATA_TYPE_TC7:
			{
				uCoord = USP_PC_SAMPLE_COORD_TC7;
				break;
			}
			case USP_ITERATED_DATA_TYPE_TC8:
			{
				uCoord = USP_PC_SAMPLE_COORD_TC8;
				break;
			}
			case USP_ITERATED_DATA_TYPE_TC9:
			{
				uCoord = USP_PC_SAMPLE_COORD_TC9;
				break;
			}
			default:
			{
				USP_DBGPRINT(( "WriteUSPSampleBlock: Invalid non-DR tex-coord\n"));
				goto WriteUSPSampleBlockExit;
			}
		}
		psDataWriters->pfnWriteUINT16(ppvPCData, (IMG_UINT16)uCoord);

		/* write tex-dimension */
		if	((psUSPSample->uTexDim) > USP_MAX_TEX_DIM)
		{
			USP_DBGPRINT(( "WriteUSPSampleBlock: Invalid non-DR tex-dimension\n"));
			goto WriteUSPSampleBlockExit;
		}
		psDataWriters->pfnWriteUINT16(ppvPCData, ((IMG_UINT16)(psUSPSample->uTexDim)));

		switch (psUSPSample->eSmpNdpDirectSrcRegType)
		{
			case USP_REGTYPE_TEMP:
			{
				uRegType = USP_PC_SAMPLE_TEMP_REGTYPE_TEMP;
				break;
			}

			case USP_REGTYPE_OUTPUT:
			{
				uRegType = USP_PC_SAMPLE_DEST_REGTYPE_OUTPUT;
			}

			case USP_REGTYPE_PA:
			{
				uRegType = USP_PC_SAMPLE_TEMP_REGTYPE_PA;
				break;
			}
			case USP_REGTYPE_INTERNAL:
			{
				uRegType = USP_PC_SAMPLE_DEST_REGTYPE_INTERNAL;
			}

			default:
			{
				USP_DBGPRINT(( "WriteUSPSampleBlock: Invalid direct Src reg-type for Non-Dependent Sample\n"));
				goto WriteUSPSampleBlockExit;
			}
		}
		psDataWriters->pfnWriteUINT16(ppvPCData, uRegType);
		psDataWriters->pfnWriteUINT16(ppvPCData, ((IMG_UINT16)(psUSPSample->uSmpNdpDirectSrcRegNum)));

	}
	else
	{
		/* Write instruction */
		psDataWriters->pfnWriteUINT32(ppvPCData, (psUSPSample->sBaseInst.uWord0));
		psDataWriters->pfnWriteUINT32(ppvPCData, (psUSPSample->sBaseInst.uWord1));
	}

	bSuccess = IMG_TRUE;

	WriteUSPSampleBlockExit:
	return bSuccess;
}

/*****************************************************************************
 Name:		WriteUSPSampleUnpackBlock

 Purpose:	Write USP sample block to pre-compiled data.

 Inputs:	psContext		- The current USP context
			psShader		- The USP shader being read
			                  (unused here)
			psDataWriters	- The set of data-writing functions to use
			ppvPCData		- Pointer to the precompiled shader data
			psUSPSampleUnpack
							- USP Sample Unpack block to write

 Outputs:	ppvPCData		- Updated to point to the next data to be written

 Returns:	TRUE/FALSE on success/failure
*****************************************************************************/
static IMG_BOOL WriteUSPSampleUnpackBlock(	PUSP_CONTEXT		psContext, 
											PUSP_SHADER			psShader,
											PPC_DATA_WRITERS	psDataWriters, 
											IMG_PVOID*			ppvPCData,
											PUSP_SAMPLEUNPACK	psUSPSampleUnpack)
{
	IMG_BOOL    bSuccess = IMG_FALSE;	
	IMG_UINT16	uRegType = 0;
	IMG_UINT16  uRegFmt = 0;
	IMG_UINT32  i;	

	USP_UNREFERENCED_PARAMETER(psShader);	

	/* Decode and write MOE state */
	if(!WriteUSPMOEState(psContext, psDataWriters, ppvPCData, &(psUSPSampleUnpack->sMOEState)))
	{
		goto WriteUSPSampleUnpackBlockExit;
	}

	psDataWriters->pfnWriteUINT32(ppvPCData, psUSPSampleUnpack->uSmpID);
	psDataWriters->pfnWriteUINT16(ppvPCData,(IMG_UINT16)(psUSPSampleUnpack->uInstDescFlags));
	psDataWriters->pfnWriteUINT16(ppvPCData, (IMG_UINT16)(psUSPSampleUnpack->uMask));
	psDataWriters->pfnWriteUINT16(ppvPCData, (IMG_UINT16)(psUSPSampleUnpack->uLive));	

	/* Write destination register */
	for	(i = 0; i < USP_MAX_TEX_CHANNELS; i++)
	{
		/* Ignore missing components */
		if(!((psUSPSampleUnpack->uMask) & (1 << i)))
		{
			uRegType = 0;
			psDataWriters->pfnWriteUINT16(ppvPCData, uRegType);
		}
		else
		{
			switch (psUSPSampleUnpack->asDest[i].eType)
			{
				case USP_REGTYPE_TEMP:
				{
					uRegType = USP_PC_SAMPLE_DEST_REGTYPE_TEMP;
					break;
				}
				case USP_REGTYPE_OUTPUT:
				{
					uRegType = USP_PC_SAMPLE_DEST_REGTYPE_OUTPUT;
					break;
				}
				case USP_REGTYPE_PA:
				{
					uRegType = USP_PC_SAMPLE_DEST_REGTYPE_PA;
					break;
				}
				case USP_REGTYPE_INTERNAL:
				{
					uRegType = USP_PC_SAMPLE_DEST_REGTYPE_INTERNAL;
					break;
				}
				default:
				{
					USP_DBGPRINT(( "WriteUSPSampleUnpackBlock: Invalid dest reg-type\n"));
					goto WriteUSPSampleUnpackBlockExit;
				}
			}
			
			psDataWriters->pfnWriteUINT16(ppvPCData, uRegType);
		}
	}

	for	(i = 0; i < USP_MAX_TEX_CHANNELS; i++)
	{
		/* Ignore missing components */
		if	(!((psUSPSampleUnpack->uMask) & (1 << i)))
		{
			psDataWriters->pfnWriteUINT16(ppvPCData, 0);
		}
		else
		{
			psDataWriters->pfnWriteUINT16(ppvPCData, 
				(IMG_UINT16)(psUSPSampleUnpack->asDest[i].uNum));
		}
	}

	for	(i = 0; i < USP_MAX_TEX_CHANNELS; i++)
	{
		/* Ignore missing components */
		if	(!((psUSPSampleUnpack->uMask) & (1 << i)))
		{
			psDataWriters->pfnWriteUINT16(ppvPCData, 0);
		}
		else
		{
			switch(psUSPSampleUnpack->asDest[i].eFmt)
			{
				case USP_REGFMT_F32:
				{
					uRegFmt = USP_PC_SAMPLE_DEST_REGFMT_F32;
				}
				break;
				case USP_REGFMT_F16:
				{
					uRegFmt = USP_PC_SAMPLE_DEST_REGFMT_F16;
				}
				break;
				case USP_REGFMT_C10:
				{
					uRegFmt = USP_PC_SAMPLE_DEST_REGFMT_C10;
				}
				break;
				case USP_REGFMT_U8:
				{
					uRegFmt = USP_PC_SAMPLE_DEST_REGFMT_U8;
				}
				break;
				default:
				{
					USP_DBGPRINT(( "WriteUSPSampleUnpackBlock: Invalid dest format\n"));
					goto WriteUSPSampleUnpackBlockExit;
				}
			}

			psDataWriters->pfnWriteUINT16(ppvPCData, uRegFmt);
		}
	}

	/* Write destination */
	for	(i = 0; i < USP_MAX_TEX_CHANNELS; i++)
	{
		/* Ignore missing components */
		if	(!((psUSPSampleUnpack->uMask) & (1 << i)))
		{
			psDataWriters->pfnWriteUINT16(ppvPCData, 0);
		}
		else
		{
			psDataWriters->pfnWriteUINT16(ppvPCData, 
				(IMG_UINT16)(psUSPSampleUnpack->asDest[i].uComp));
		}
	}
	
	switch (psUSPSampleUnpack->eDirectSrcRegType)
	{
		case USP_REGTYPE_TEMP:
		{
			uRegType = USP_PC_SAMPLE_DEST_REGTYPE_TEMP;
			break;
		}

		case USP_REGTYPE_OUTPUT:
		{
			uRegType = USP_PC_SAMPLE_DEST_REGTYPE_OUTPUT;
		}

		case USP_REGTYPE_PA:
		{
			uRegType = USP_PC_SAMPLE_DEST_REGTYPE_PA;
			break;
		}
		case USP_REGTYPE_INTERNAL:
		{
			uRegType = USP_PC_SAMPLE_DEST_REGTYPE_INTERNAL;
		}

		default:
		{
			USP_DBGPRINT(( "WriteUSPSampleUnpackBlock: Invalid direct src reg-type\n"));
			goto WriteUSPSampleUnpackBlockExit;
		}
	}
	psDataWriters->pfnWriteUINT16(ppvPCData, uRegType);

	psDataWriters->pfnWriteUINT16(ppvPCData, ((IMG_UINT16)(psUSPSampleUnpack->uDirectSrcRegNum)));

	switch (psUSPSampleUnpack->eBaseSampleUnpackSrcRegType)
	{
		case USP_REGTYPE_TEMP:
		{
			uRegType = USP_PC_SAMPLE_TEMP_REGTYPE_TEMP;
			break;
		}

		case USP_REGTYPE_PA:
		{
			uRegType = USP_PC_SAMPLE_TEMP_REGTYPE_PA;
			break;
		}
		default:
		{
			USP_DBGPRINT(( "WriteUSPSampleUnpackBlock: Invalid temp reg-type\n"));
			goto WriteUSPSampleUnpackBlockExit;
		}
	}
	psDataWriters->pfnWriteUINT16(ppvPCData, uRegType);
	psDataWriters->pfnWriteUINT16(ppvPCData, ((IMG_UINT16)(psUSPSampleUnpack->uBaseSampleUnpackSrcRegNum)));	

	switch (psUSPSampleUnpack->eSampleUnpackTempRegType)
	{
		case USP_REGTYPE_TEMP:
		{
			uRegType = USP_PC_SAMPLE_TEMP_REGTYPE_TEMP;
			break;
		}

		case USP_REGTYPE_PA:
		{
			uRegType = USP_PC_SAMPLE_TEMP_REGTYPE_PA;
			break;
		}
		default:
		{
			USP_DBGPRINT(( "WriteUSPSampleUnpackBlock: Invalid temp reg-type\n"));
			goto WriteUSPSampleUnpackBlockExit;
		}
	}
	psDataWriters->pfnWriteUINT16(ppvPCData, uRegType);
	psDataWriters->pfnWriteUINT16(ppvPCData, ((IMG_UINT16)(psUSPSampleUnpack->uSampleUnpackTempRegNum)));
	
	psDataWriters->pfnWriteUINT16(ppvPCData, ((IMG_UINT16)(psUSPSampleUnpack->uIRegsLiveBefore)));

	bSuccess = IMG_TRUE;

	WriteUSPSampleUnpackBlockExit:
	return bSuccess;
}

/*****************************************************************************
 Name:		WriteUSPHWCodeBlock

 Purpose:	Write USP hardware code block to pre-compiled data.

 Inputs:	psContext		- The current USP context
			psShader		- The USP shader being read
			                  (unused here)
			psDataWriters	- The set of data-writing functions to use
			ppvPCData		- Pointer to the precompiled shader data
			psUSPInstBlock  - Instruction block to be written

 Outputs:	ppvPCData		- Updated to point to the next data to be written

 Returns:	TRUE/FALSE on success/failure
*****************************************************************************/
static IMG_BOOL WriteUSPHWCodeBlock( PUSP_CONTEXT	   psContext, 
									 PUSP_SHADER	   psShader,
									 PPC_DATA_WRITERS  psDataWriters, 
									 IMG_PVOID*		   ppvPCData,
									 PUSP_INSTBLOCK	   psUSPInstBlock)
{
	IMG_UINT16  uInstCount = 0;
	IMG_UINT32  i;
	IMG_UINT16  uPCInstDesFlags; 

	USP_UNREFERENCED_PARAMETER(psShader);

	/* Write instruction count */
	uInstCount = (IMG_UINT16)(psUSPInstBlock->uOrgInstCount);
	
	psDataWriters->pfnWriteUINT16(ppvPCData, uInstCount);

	/* Decode and write MOE state */
	if(!WriteUSPMOEState(psContext, psDataWriters, ppvPCData, &(psUSPInstBlock->sInitialMOEState)))
	{
		USP_DBGPRINT(( "WriteUSPHWCodeBlock: Invalid USP shader MOE state\n"));
		return IMG_FALSE;
	}
	
	/* Write instruction desriptions */
	for	(i = 0; i < uInstCount; i++)
	{
		uPCInstDesFlags = (IMG_UINT16)InitPCInstDescFlags((psUSPInstBlock->psInsts[i].sDesc.uFlags));
		psDataWriters->pfnWriteUINT16(ppvPCData, uPCInstDesFlags);
	}

	/* Write hardware instructions */
	for	(i = 0; i < uInstCount; i++)
	{
		psDataWriters->pfnWriteUINT32(ppvPCData, (psUSPInstBlock->psInsts[i].sHWInst.uWord0));
		psDataWriters->pfnWriteUINT32(ppvPCData, (psUSPInstBlock->psInsts[i].sHWInst.uWord1));
	}

	return IMG_TRUE; 
}

/*****************************************************************************
 Name:		WriteUSPBranchBlock

 Purpose:	Write USP branch block to pre-compiled data.

 Inputs:	psContext		- The current USP context
                              (unused here)
			psShader		- The USP shader being read
			                  (unused here)
			psDataWriters	- The set of data-writing functions to use
			ppvPCData		- Pointer to the precompiled shader data
			psUSPBranch     - Branch data to be written

 Outputs:	ppvPCData		- Updated to point to the next data to be written

 Returns:	TRUE/FALSE on success/failure
*****************************************************************************/
static IMG_BOOL WriteUSPBranchBlock(PUSP_CONTEXT	  psContext,
								    PUSP_SHADER		  psShader,
								    PPC_DATA_WRITERS  psDataWriters,
								    IMG_PVOID*		  ppvPCData,
								    PUSP_BRANCH       psUSPBranch)
{
	IMG_BOOL bSuccess = IMG_FALSE;

	USP_UNREFERENCED_PARAMETER(psShader);
	USP_UNREFERENCED_PARAMETER(psContext);

	if((psUSPBranch->psInstBlock->uInstCount) == 1)
	{
		psDataWriters->pfnWriteUINT32(ppvPCData, (psUSPBranch->psInstBlock->psInsts[0].sHWInst.uWord0));
		psDataWriters->pfnWriteUINT32(ppvPCData, (psUSPBranch->psInstBlock->psInsts[0].sHWInst.uWord1));
		psDataWriters->pfnWriteUINT16(ppvPCData, (IMG_UINT16)(psUSPBranch->uLabelID));
		bSuccess = IMG_TRUE;
	}
	return bSuccess;
}

/*****************************************************************************
 Name:		WriteUSPBlocks

 Purpose:	Write USP blocks to pre-compiled shader

 Inputs:	psContext		- The current USP context
			psShader		- The USP shader being read
			psDataWriters	- The set of data-writing functions to use
			ppvPCData		- Pointer to the precompiled shader data

 Outputs:	ppvPCData		- Updated to point to the next data to be written

 Returns:	TRUE/FALSE on success/failure
*****************************************************************************/
static IMG_BOOL WriteUSPBlocks(PUSP_CONTEXT		 psContext, 
							   PUSP_SHADER		 psShader,
							   PPC_DATA_WRITERS	 psDataWriters, 
							   IMG_PVOID*		 ppvPCData)
{
	PUSP_LABEL			psLabels = (psShader->psLabels);
	IMG_BOOL			bSuccess = IMG_FALSE;
	USP_PC_BLOCK_HDR	sPCBlockHdr;
	PUSP_SAMPLE			psNonDepSamples = (psShader->psNonDepSamples);
	PUSP_SAMPLE			psDepSamples = (psShader->psDepSamples);
	PUSP_SAMPLEUNPACK	psSampleUnpacks = (psShader->psSampleUnpacks);
	PUSP_INSTBLOCK		psInstBlocks = (psShader->psInstBlocks);
	PUSP_BRANCH			psBranches = (psShader->psBranches);

	while(psInstBlocks)
	{
		if(psInstBlocks == (psShader->psInstBlocksEnd))
		{
			/* There must be a labels at the end */
			if((psShader->psProgDesc->uHWFlags) & UNIFLEX_HW_FLAGS_LABEL_AT_END)
			{
				/* Write all the labels at end */
				while(psLabels)
				{
					sPCBlockHdr.uType = USP_PC_BLOCK_TYPE_LABEL;
					WritePCBlockHdr(psContext, psDataWriters, ppvPCData, &sPCBlockHdr);
					psDataWriters->pfnWriteUINT16(ppvPCData, (IMG_UINT16)(psLabels->uID));
					psLabels = psLabels->psNext;
				}
			}
			break;
		}
		
		/* Is this block in labels */
		else if((psLabels) && (psInstBlocks == (psLabels->psInstBlock)))
		{
			/* Write all the labels following this block */
			while((psLabels) && (psInstBlocks == (psLabels->psInstBlock)))
			{
				sPCBlockHdr.uType = USP_PC_BLOCK_TYPE_LABEL;
				WritePCBlockHdr(psContext, psDataWriters, ppvPCData, &sPCBlockHdr);
				psDataWriters->pfnWriteUINT16(ppvPCData, (IMG_UINT16)(psLabels->uID));
				psLabels = psLabels->psNext;
			}

			/* Is this block handled by non-dependant samples */
			if((psNonDepSamples) && (psInstBlocks == (psNonDepSamples->psInstBlock)))
			{
				continue;
			}

			/* Is this block handled by dependant samples */
			else if((psDepSamples) && (psInstBlocks == (psDepSamples->psInstBlock)))
			{
				continue;
			}

			/* Is this block handled by dependant branches */
			else if((psBranches) && (psInstBlocks == (psBranches->psInstBlock)))
			{
				continue;
			}

			/* This is a hardware code block */
			else if((psInstBlocks->uInstCount) > 0)
			{
				/* Write hardware code block */
				sPCBlockHdr.uType = USP_PC_BLOCK_TYPE_HWCODE;
				WritePCBlockHdr(psContext, psDataWriters, ppvPCData, &sPCBlockHdr);
				if(!WriteUSPHWCodeBlock(psContext, 
					                    psShader, 
									    psDataWriters, 
									    ppvPCData, 
									    psInstBlocks))
				{
					USP_DBGPRINT(( "WriteUSPBlocks: Error writing USP hardware code block\n"));
					goto WriteUSPBlocksExit;
				}
			}
		}

		/* Is this block in branches */
		else if((psBranches) && (psInstBlocks == (psBranches->psInstBlock)))
		{
			while((psBranches) && (psInstBlocks == (psBranches->psInstBlock)))
			{
				/* Write a branch block */
				sPCBlockHdr.uType = USP_PC_BLOCK_TYPE_BRANCH;
				WritePCBlockHdr(psContext, psDataWriters, ppvPCData, &sPCBlockHdr);
				if(!WriteUSPBranchBlock(psContext, 
					                    psShader, 
										psDataWriters, 
										ppvPCData, 
										psBranches))
				{
					USP_DBGPRINT(( "WriteUSPBlocks: Error writing USP branch block\n"));
					goto WriteUSPBlocksExit;
				}
				psBranches = psBranches->psNext;
			}
		}

		/* Is this block in non-dependent samples */
		else if((psNonDepSamples) && (psInstBlocks == (psNonDepSamples->psInstBlock)))
		{
			while((psNonDepSamples) && (psInstBlocks == (psNonDepSamples->psInstBlock)))
			{
				/* Write a sample block */
				sPCBlockHdr.uType = USP_PC_BLOCK_TYPE_SAMPLE;
				WritePCBlockHdr(psContext, psDataWriters, ppvPCData, &sPCBlockHdr);
				if(!WriteUSPSampleBlock(psContext,
					psShader,
					psDataWriters,
					ppvPCData,
					psNonDepSamples))
				{
					USP_DBGPRINT(( "WriteUSPBlocks: Error writing USP sample block\n"));
					goto WriteUSPBlocksExit;
				}
				psNonDepSamples = psNonDepSamples->psNext;
			}
		}

		/* Is this block in dependent samples */
		else if((psDepSamples) && (psInstBlocks == (psDepSamples->psInstBlock)))
		{
			while((psDepSamples) && (psInstBlocks == (psDepSamples->psInstBlock)))
			{
				/* Write a sample block */
				sPCBlockHdr.uType = USP_PC_BLOCK_TYPE_SAMPLE;
				WritePCBlockHdr(psContext, psDataWriters, ppvPCData, &sPCBlockHdr);
				if(!WriteUSPSampleBlock(psContext,
					psShader,
					psDataWriters,
					ppvPCData,
					psDepSamples))
				{
					USP_DBGPRINT(( "WriteUSPBlocks: Error writing USP sample block\n"));
					goto WriteUSPBlocksExit;
				}
				psDepSamples = psDepSamples->psNext;
			}
		}

		/* Is this block in sample unpackes */
		else if((psSampleUnpacks) && (psInstBlocks == (psSampleUnpacks->psInstBlock)))
		{
			while((psSampleUnpacks) && (psInstBlocks == (psSampleUnpacks->psInstBlock)))
			{
				/* Write a sample block */
				sPCBlockHdr.uType = USP_PC_BLOCK_TYPE_SAMPLEUNPACK;
				WritePCBlockHdr(psContext, psDataWriters, ppvPCData, &sPCBlockHdr);
				if(!WriteUSPSampleUnpackBlock(psContext,
					psShader,
					psDataWriters,
					ppvPCData,
					psSampleUnpacks))
				{
					USP_DBGPRINT(( "WriteUSPBlocks: Error writing USP sample block\n"));
					goto WriteUSPBlocksExit;
				}
				psSampleUnpacks = psSampleUnpacks->psNext;
			}
		}

		/* This is hardware code block */
		else
		{
			/* Write a hardware code block */
			sPCBlockHdr.uType = USP_PC_BLOCK_TYPE_HWCODE;
			WritePCBlockHdr(psContext, psDataWriters, ppvPCData, &sPCBlockHdr);
			if(!WriteUSPHWCodeBlock(psContext, 
				                    psShader, 
									psDataWriters, 
									ppvPCData, 
									psInstBlocks))
			{
				USP_DBGPRINT(( "WriteUSPBlocks: Error writing USP hardware code block\n"));
				goto WriteUSPBlocksExit;
			}
		}
		psInstBlocks = psInstBlocks->psNext;
	}

	/* Write the end marker block */
	sPCBlockHdr.uType = USP_PC_BLOCK_TYPE_END;
	WritePCBlockHdr(psContext, psDataWriters, ppvPCData, &sPCBlockHdr);

	bSuccess = IMG_TRUE;

	WriteUSPBlocksExit:
	return bSuccess;
}

/*****************************************************************************
 Name:		ReadPCLabelBlock

 Purpose:	Read precompiled data describing a branch/call destination

 Inputs:	psContext		- The current USP context
			psShader		- The USP shader being created
			psDataReaders	- The set of data-reading functions to use
							  (unused here)
			ppvPCData		- Pointer to the precompiled shader data

 Outputs:	ppvPCData		- Updated to point to the next data to be read

 Returns:	TRUE/FALSE on success/failure
*****************************************************************************/
static IMG_BOOL ReadPCLabelBlock(PUSP_CONTEXT		psContext,
								 PUSP_SHADER		psShader,
								 PPC_DATA_READERS	psDataReaders,
								 IMG_PVOID*			ppvPCData)
{
	USP_PC_BLOCK_LABEL	sPCLabel;
	PUSP_LABEL			psUSPLabel;
	IMG_BOOL			bFailed;

	/*
		Label not created yet
	*/
	bFailed		= IMG_TRUE;
	psUSPLabel	= IMG_NULL;

	/*
		Read the precompiled label data (currently just the ID)
	*/
	psDataReaders->pfnReadUINT16(ppvPCData, &sPCLabel.uLabelID);

	/*
		Create and initialise corresponding USP branch info
	*/
	psUSPLabel = USPLabelCreate(psContext, sPCLabel.uLabelID);
	if	(!psUSPLabel)
	{
		USP_DBGPRINT(("ReadPCLabelBlock: Failed to create USP label info\n"));
		goto ReadPCLabelBlockExit;
	}

	/*
		Add the new label to the list of all those within the shader
	*/
	if	(!USPShaderAddLabel(psShader, psUSPLabel, psContext))
	{
		USP_DBGPRINT(("ReadPCLabelBlock: Multiple labels with ID %d encountered\n", psUSPLabel->uID));
		goto ReadPCLabelBlockExit;
	}

	/*
		Label created and added to shader
	*/
	bFailed = IMG_FALSE;

	ReadPCLabelBlockExit:

	/*
		Cleanup on error
	*/
	if	(bFailed)
	{
		if	(psUSPLabel)
		{
			USPLabelDestroy(psUSPLabel, psContext);
			psUSPLabel = IMG_NULL;
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		ReadPCTextureWriteBlock

 Purpose:	Read precompiled data describing a texture write

 Inputs:	psContext		- The current USP context
			psShader		- The USP shader being created
			psDataReaders	- The set of data-reading functions to use
							  (unused here)
			ppvPCData		- Pointer to the precompiled shader data

 Outputs:	ppvPCData		- Updated to point to the next data to be read

 Returns:	TRUE/FALSE on success/failure
*****************************************************************************/
static IMG_BOOL ReadPCTextureWriteBlock(PUSP_CONTEXT		psContext,
										PUSP_SHADER			psShader,
										PPC_DATA_READERS	psDataReaders,
										IMG_PVOID*			ppvPCData)
{
	USP_PC_BLOCK_TEXTURE_WRITE	sPCTextureWrite;
	PUSP_TEXTURE_WRITE			psUSPTextureWrite;
	IMG_BOOL 					bFailed	= IMG_TRUE;

	/*
	  Read the precompiled invocation data
	*/
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCTextureWrite.uWriteID);

	psDataReaders->pfnReadUINT32(ppvPCData, &sPCTextureWrite.uBaseAddrRegNum);
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCTextureWrite.uBaseAddrRegType);
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCTextureWrite.uStrideRegNum);
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCTextureWrite.uStrideRegType);
	
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCTextureWrite.uCoordXRegNum);
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCTextureWrite.uCoordXRegType);
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCTextureWrite.uCoordYRegNum);
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCTextureWrite.uCoordYRegType);

	psDataReaders->pfnReadUINT32(ppvPCData, &sPCTextureWrite.uChannelXRegNum);
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCTextureWrite.uChannelXRegType);
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCTextureWrite.uChannelYRegNum);
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCTextureWrite.uChannelYRegType);
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCTextureWrite.uChannelZRegNum);
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCTextureWrite.uChannelZRegType);
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCTextureWrite.uChannelWRegNum);
	psDataReaders->pfnReadUINT32(ppvPCData, &sPCTextureWrite.uChannelWRegType);

	psDataReaders->pfnReadUINT32(ppvPCData, &sPCTextureWrite.uTempRegStartNum);

	psDataReaders->pfnReadUINT32(ppvPCData, &sPCTextureWrite.uChannelRegFmt);

	psDataReaders->pfnReadUINT32(ppvPCData, &sPCTextureWrite.uMaxNumTemps);

	psDataReaders->pfnReadUINT16(ppvPCData, &sPCTextureWrite.sInstDesc.uFlags);

	/*
	  Create and initialise corresponding USP texture write info
	*/
	psUSPTextureWrite = USPTextureWriteCreate(psContext, &sPCTextureWrite, psShader);
	if	(!psUSPTextureWrite)
	{
		USP_DBGPRINT(("ReadPCTextureWriteBlock: Failed to create USP texture write info\n"));
		goto ReadPCTextureWriteBlockExit;
	}

	/*
	  Add the new texture write to the list of all those within the shader
	*/
	if	(!USPShaderAddTextureWrite(psShader, psUSPTextureWrite, psContext))
	{
		USP_DBGPRINT(("ReadPCTextureWriteBlock: Failed to add texture write to USP shader\n"));
		goto ReadPCTextureWriteBlockExit;
	}

	/* 
	  Add the instruction block for this texture write to the list of all those within the shader
	*/
	USPShaderAddInstBlock(psShader, psUSPTextureWrite->psInstBlock);

	bFailed = IMG_FALSE;

ReadPCTextureWriteBlockExit:

	/*
	  Cleanup on error
	*/
	if	(bFailed)
	{
		if	(psUSPTextureWrite)
		{
			USPTextureWriteDestroy(psUSPTextureWrite, psContext);
			psUSPTextureWrite = IMG_NULL;
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		SetupInputData

 Purpose:	Create and initialise the input-data information for a USP shader

 Inputs:	psShader	- The shader to setup the input-data for
			psContext	- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on error. IMG_TRUE otherwise.
*****************************************************************************/
static IMG_BOOL SetupInputData(PUSP_SHADER	psShader,
							   PUSP_CONTEXT	psContext)
{
	PUSP_INPUT_DATA			psInputData;
	PUSP_PROGDESC			psProgDesc;
	PUSP_PC_PSINPUT_LOAD	psPCInputLoad;
	PUSP_PC_PSINPUT_LOAD	psPCInputLoadEnd;
	IMG_UINT32				uMaxIteratedData;
	IMG_UINT32				uMaxPreSampledData;
	IMG_UINT32				uMaxTexStateData;
	IMG_BOOL				bFailed;

	/*
		No input data created yet!		
	*/
	bFailed		= IMG_TRUE;
	psInputData	= IMG_NULL;
	psProgDesc	= psShader->psProgDesc;

	/*
		Count the iterated data present in the pre-compiled input-data list
	*/
	uMaxIteratedData	= 0;
	psPCInputLoad		= psProgDesc->psPSInputLoads;
	psPCInputLoadEnd	= psPCInputLoad + psProgDesc->uPSInputCount;

	while (psPCInputLoad < psPCInputLoadEnd)
	{
		uMaxIteratedData++;
		psPCInputLoad++;
	}

	/*
		Lookup the maximum number of pre-sampled data entries required

		NB: At most 4 chunks of pre-sampled data per non-dependent sample,
			assuming each uses a unique texture
	*/
	uMaxPreSampledData = psShader->uNonDepSampleCount * USP_MAX_TEXTURE_CHUNKS 
	#if defined(SGX543) || defined(SGX544) || defined(SGX545) || defined(SGX554)
		/*
			It may be required if a non dependent chunk require more than 1 
			registers 
		*/
		+ psShader->uNonDepSampleCount
	#endif /* defined(SGX543) || defined(SGX545) */
	
	#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
		/*
			It may be required if a non dependent (unpacked) chunk requires
			the register to be 64-bit aligned. The 32-bit aligned entry is 
			converted into dummy iteration so an extra space is required.
		*/
		+ psShader->uNonDepSampleCount
	#endif /* defined(SGX_FEATURE_UNIFIED_STORE_64BITS) */
		;

	/*
		Lookup the maximum number of texture-state data entries required

		NB: At most 4 sets of texture-state words (one per chunk) for each
			dependent sample, assuming each is uses a unique texture.
	*/
	uMaxTexStateData = (psShader->uDepSampleCount * USP_MAX_TEXTURE_CHUNKS) + (psShader->uNonDepSampleCount * USP_MAX_TEXTURE_CHUNKS);

	/*
		Try to create the input-data for the shader
	*/
	psInputData = USPInputDataCreate(psContext);
	if	(!psInputData)
	{
		USP_DBGPRINT(("SetupInputData: Failed to create input-data\n"));
		goto SetupInputDataExit;
	}

	/*
		Create the lists of iterated, pre-sampled and texture-state input data
	*/
	if	(uMaxIteratedData > 0)
	{
		if	(!USPInputDataCreateIteratedDataList(psInputData,
												 uMaxIteratedData,
												 psContext))
		{
			USP_DBGPRINT(("SetupInputData: Failed to create iterated data list\n"));
			goto SetupInputDataExit;
		}
	}

	if	(uMaxPreSampledData > 0)
	{
		if	(!USPInputDataCreatePreSampledDataList(psInputData,
												   uMaxPreSampledData,
												   psContext))
		{
			USP_DBGPRINT(("SetupInputData: Failed to created pre-sampled data list\n"));
			goto SetupInputDataExit;
		}
	}

	if	(uMaxTexStateData > 0)
	{
		if	(!USPInputDataCreateTexStateDataList(psInputData,
												 uMaxTexStateData,
												 psContext))
		{
			USP_DBGPRINT(("SetupInputData: Failed to created texture-state data list\n"));
			goto SetupInputDataExit;
		}
	}

	/*
		Add the pre-compiled iterations and pre-sampled textures to the input-data
		for this shader

		NB: The final list of pre-sampled texture-data and texture-state will be
			setup during finalisation, when the texture-formats are known. The list
			entries added represent the iterated and pre-sampled data known by the
			USC.
	*/
	if	(!USPInputDataAddPCInputData(psInputData,
									 psProgDesc->uPSInputCount,
									 psProgDesc->psPSInputLoads,
									 psContext))
	{
		USP_DBGPRINT(("SetupInputData: Error adding iterated-data to input-data\n"));
		goto SetupInputDataExit;
	}

	/*
		Input data created and initialised OK
	*/
	psShader->psInputData = psInputData;
	bFailed = IMG_FALSE;

	SetupInputDataExit:

	/*
		Cleanup on error
	*/
	if	(bFailed)
	{
		if	(psInputData)
		{
			USPInputDataDestroy(psInputData, psContext);
		}
	}

	return (IMG_BOOL)!bFailed;
}

static IMG_VOID USPTexSampDestroy(TEX_SAMPLES_LIST	**psList,
								  PUSP_CONTEXT		psContext)
/*****************************************************************************
 Name:		USPTexSampDestroy

 Purpose:	Frees memory allocated for used samples for each texture.

 Inputs:	psList      - List to recorded texture samples for each texture
            psContext	- The current USP execution context
            

 Outputs:	None.

 Returns:	None.
*****************************************************************************/
{
	TEX_SAMPLES_LIST *psNode, *psPrev;
	SAMPLES_LIST *psInNode, *psInPrev;

	psNode = *psList;
	while(psNode)
	{
		psPrev = psNode;
		psNode = psNode->psNext;

		psInNode = psPrev->psList;
		while(psInNode)
		{
			psInPrev = psInNode;
			psInNode = psInNode->psNext;
			psContext->pfnFree(psInPrev);
		}
		psContext->pfnFree(psPrev);
	}
	*psList = IMG_NULL;
}

/*****************************************************************************
 Name:		USPShaderDestroy

 Purpose:	Destroy a USP internal shader previously created using USPShaderCreate

 Inputs:	psPCShader		Pointer to precompiled shader data

 Outputs:	psUSPShader		Pointer to the created USP internal shader

 Returns:	none
*****************************************************************************/
IMG_INTERNAL IMG_VOID USPShaderDestroy(PUSP_CONTEXT	psContext,
									   PUSP_SHADER	psUSPShader)
{
	if	(psUSPShader)
	{
		PUSP_LABEL			psLabel;
		PUSP_BRANCH			psBranch;
		PUSP_SAMPLE			psSample;
		PUSP_SAMPLEUNPACK	psSampleUnpack;
		PUSP_RESULTREF		psResultRef;
		PUSP_INSTBLOCK		psInstBlock;
		PUSP_TEXTURE_WRITE	psTextureWrite;

		/*
			Destroy the shader's input-data
		*/
		if	(psUSPShader->psInputData)
		{
			USPInputDataDestroy(psUSPShader->psInputData, psContext);
		}

		/*
			Destroy all the labels in the shader
		*/
		for	(psLabel = psUSPShader->psLabels; psLabel; )
		{
			PUSP_LABEL	psNextLabel;

			psNextLabel = psLabel->psNext;
			USPLabelDestroy(psLabel, psContext);
			psLabel = psNextLabel;
		}

		/*
			Destroy all the branches in the shader
		*/
		for	(psBranch = psUSPShader->psBranches; psBranch; )
		{
			PUSP_BRANCH	psNextBranch;

			psNextBranch = psBranch->psNext;
			USPBranchDestroy(psBranch, psContext);
			psBranch = psNextBranch;
		}

		/*
			Destroy all the dependent samples in the shader
		*/
		for	(psSample = psUSPShader->psDepSamples; psSample; )
		{
			PUSP_SAMPLE	psNextSample;

			psNextSample = psSample->psNext;
			USPSampleDestroy(psSample, psContext);
			psSample = psNextSample;
		}

		/*
			Destroy all the non-dependent samples in the shader
		*/
		for	(psSample = psUSPShader->psNonDepSamples; psSample; )
		{
			PUSP_SAMPLE	psNextSample;

			psNextSample = psSample->psNext;
			USPSampleDestroy(psSample, psContext);
			psSample = psNextSample;
		}

		/*
			Destroy all the sample unpacks in the shader
		*/
		for	(psSampleUnpack = psUSPShader->psSampleUnpacks; psSampleUnpack; )
		{
			PUSP_SAMPLEUNPACK	psNextSampleUnpack;

			psNextSampleUnpack = psSampleUnpack->psNext;
			USPSampleUnpackDestroy(psSampleUnpack, psContext);
			psSampleUnpack = psNextSampleUnpack;
		}

		/*
			Destroy all the result-references in the shader 
		*/
		for	(psResultRef = psUSPShader->psResultRefs; psResultRef; )
		{
			PUSP_RESULTREF psNextResultRef;

			psNextResultRef = psResultRef->psNext;
			USPResultRefDestroy(psResultRef, psContext);
			psResultRef = psNextResultRef;
		}

		/*
			Destroy all the instruciton blocks in the shader
		*/
		for	(psInstBlock = psUSPShader->psInstBlocks; psInstBlock; )
		{
			PUSP_INSTBLOCK	psNextInstBlock;

			psNextInstBlock = psInstBlock->psNext;
			USPInstBlockDestroy(psInstBlock, psContext);
			psInstBlock = psNextInstBlock;
		}

		/*
		    Destroy all the texture writes in the shader
		 */
		for  (psTextureWrite = psUSPShader->psTextureWrites; psTextureWrite; )
		{
			PUSP_TEXTURE_WRITE psNextTextureWrite;

			psNextTextureWrite = psTextureWrite->psNext;
			USPTextureWriteDestroy(psTextureWrite, psContext);
			psTextureWrite = psNextTextureWrite;
		}

		/*
			Destroy the program description block
		*/
		if	(psUSPShader->psProgDesc)
		{
			USPProgDescDestroy(psUSPShader->psProgDesc, psContext);
		}

		/*
		    Destroy the list of texture samples used for each texture
		*/
		if (psUSPShader->psUsedTexFormats)
		{
			USPTexSampDestroy(&(psUSPShader->psUsedTexFormats), psContext);
		}

		/*
			Destroy the space allocated for texture control words
		*/
		if (psUSPShader->psTexCtrWrds)
		{
			psContext->pfnFree(psUSPShader->psTexCtrWrds);
		}

		/*
			finally destroy the shader structure	
		*/
		psContext->pfnFree(psUSPShader);
	}
}

/*****************************************************************************
 Name:		PCShaderCreate

 Purpose:	Packs a USP shader to a precompiled shader. 

 Inputs:	psContext	- The current USP execution context
			psUSPShader	- Pointer to the USP shader

 Outputs:	none

 Returns:	The created precompiled shader
*****************************************************************************/
IMG_INTERNAL PUSP_PC_SHADER PCShaderCreate(PUSP_CONTEXT	psContext,
										   PUSP_SHADER	psUSPShader)
{
	PUSP_PC_SHADER    psPCShader = IMG_NULL;
	IMG_BOOL	      bSuccess = IMG_FALSE;
	IMG_PVOID	      pvPCShader = IMG_NULL;
	PPC_DATA_WRITERS  psDataWriters = IMG_NULL;
	USP_PC_BLOCK_HDR  sPCBlockHdr;
	IMG_UINT32        uTemp, i;
	IMG_UINT32        uCalSize = 0;

	/*
		First iteration calculates size i.e. required to create 
		the pre-compiled shader.
		Second iteration creates the required pre-compiled shader.
	*/
	for(i = 0; i<2; i++)
	{
		if(i == 0)
		{
			/* Point to data size calculation routines */
			psDataWriters = &g_sDataSizeCounters;
		}
		else
		{
			/* Save the size calculated by last iteration */
			uCalSize = (IMG_UINT32)pvPCShader;
			
			if(uCalSize == 0)
			{
				USP_DBGPRINT(( "PCShaderCreate: Failed to calculate size \n"));
				goto PCShaderCreateFinish;
			}

			psPCShader = (PUSP_PC_SHADER)psContext->pfnAlloc(uCalSize);

			if(!psPCShader)
			{
				USP_DBGPRINT(( "PCShaderCreate: Failed to allocate new PC shader \n"));
				goto PCShaderCreateFinish;
			}
			
			pvPCShader = psPCShader;

			/* Point to data writing routines */
			psDataWriters = &g_sStraightDataWriters;

		}

		psDataWriters->pfnWriteUINT32(&pvPCShader , USP_PC_SHADER_ID);

		psDataWriters->pfnWriteUINT32(&pvPCShader , USP_PC_SHADER_VER);

		if(i == 0)
		{
			/* size is not calculated yet so set to zero */
			uTemp = 0;
		}
		else
		{
			uTemp = uCalSize - sizeof(USP_PC_SHADER);
		}
		psDataWriters->pfnWriteUINT32(&pvPCShader, uTemp);

		if(psUSPShader->psProgDesc)
		{
			/*
				The first block immediately follows the overall PC shader
				header must be a program-description block.
			*/
			sPCBlockHdr.uType = USP_PC_BLOCK_TYPE_PROGDESC;
			WritePCBlockHdr(psContext, psDataWriters, &pvPCShader, &sPCBlockHdr);
		}
		else
		{
			USP_DBGPRINT(( "PCShaderCreate: Failed to write PC prog desc block\n"));
			goto PCShaderCreateFinish;
		}

		/*
			Write the overall program description
		*/
		if	(!WritePCProgDescBlock(psContext,
								   psUSPShader,
								   psDataWriters,
								   &pvPCShader))
		{
			USP_DBGPRINT(( "PCShaderCreate: Failed to write PC prog desc block\n"));
			goto PCShaderCreateFinish;
		}

		/*
			Write all the USP blocks.
		*/
		if	(!WriteUSPBlocks(psContext,
							 psUSPShader,
							 psDataWriters,
							 &pvPCShader))
		{
			USP_DBGPRINT(( "PCShaderCreate: Failed to write USP blocks\n"));
			goto PCShaderCreateFinish;
		}
	}

	if(uCalSize !=   ((IMG_UINT32)pvPCShader - (IMG_UINT32)psPCShader))
	{
		USP_DBGPRINT(( "PCShaderCreate: Failed size mismatch in calculated an written data\n"));
		goto PCShaderCreateFinish;
	}

	if((uCalSize - sizeof(USP_PC_SHADER)) != psPCShader->uSize)
	{
		USP_DBGPRINT(( "PCShaderCreate: Failed incorrect pre-compiled shader size\n"));
		goto PCShaderCreateFinish;
	}

	/*
		Precompiled shader created OK
	*/
	bSuccess = IMG_TRUE;

	PCShaderCreateFinish:
	
	/* On failure delete the partially created pre-compiled shader */
	if(!bSuccess)
	{
		if(psPCShader)
		{
			psContext->pfnFree(psPCShader);
			psPCShader = IMG_NULL;
		}
	}
	return psPCShader;
}

/*****************************************************************************
 Name:		USPShaderCreate

 Purpose:	Unpack precompiled shader data to form an USP internal shader

 Inputs:	psContext	- The current USP execution context
			psPCShader	- Pointer to the precompiled shader data

 Outputs:	none

 Returns:	The created patchable (USP) shader
*****************************************************************************/
IMG_INTERNAL PUSP_SHADER USPShaderCreate(PUSP_CONTEXT	psContext,
										 IMG_PVOID		pvPCShader)
{
	PPC_DATA_READERS	psDataReaders;
	PUSP_SHADER			psUSPShader;
	PUSP_INSTBLOCK		psInstBlock;
	PUSP_MOESTATE		psFinalMOEState;
	USP_PC_BLOCK_HDR	sPCBlockHdr;
	USP_PC_BLOCK_TYPE	eLastPCBlockType;
	IMG_PVOID			pvPCData;
	IMG_UINT32			uId;
	IMG_UINT32			uVersion;
	IMG_UINT32			uSize;
	IMG_UINT32			uSizeRead;
	IMG_BOOL			bSuccess;

	/*
		No shader created yet
	*/
	psUSPShader	= IMG_NULL;
	bSuccess	= IMG_FALSE;

	/*
		Check the ID at the start of the precompiled data, so see
		whether we need to swizzle the data as we read it.
	*/
	pvPCData = pvPCShader;

	ReadUINT32Straight(&pvPCData, &uId);
	if	(uId == USP_PC_SHADER_ID)
	{
		/*
			Saved data format is the way we want it
		*/
		psDataReaders = &g_sStraightDataReaders;
	}
	else if	(uId == (((USP_PC_SHADER_ID >> 24) & 0x000F) |
					 ((USP_PC_SHADER_ID >> 8)  & 0x00F0) |
					 ((USP_PC_SHADER_ID << 8)  & 0x0F00) |
					 ((USP_PC_SHADER_ID << 24) & 0xF000)))
	{
		/*
			Saved data format is the other way to the way we want it
		*/
		psDataReaders = &g_sSwappedDataReaders;
	}
	else
	{
		USP_DBGPRINT(( "USPShaderCreate: Invalid binary data ID read (%c%c%c%c)", (uId >> 24) & 0xf, (uId >> 16) & 0xf, (uId >> 8) & 0xf, (uId >> 0) & 0xf));
		goto USPShaderCreateFinish;
	}

	/*
		Check that the data-format is one that we can support
	*/
	psDataReaders->pfnReadUINT32(&pvPCData, &uVersion);
	if	(uVersion != USP_PC_SHADER_VER)
	{
		USP_DBGPRINT(( "USPShaderCreate: Unsupported binary shader version %d, need %d", uVersion, USP_PC_SHADER_VER));
		goto USPShaderCreateFinish;
	}

	/*
		Read the overall data size
	*/
	psDataReaders->pfnReadUINT32(&pvPCData, &uSize);

	/*
		The first block immediately follows the overall PC shader
		header. It must be a program-description block.
	*/
	if	(!ReadPCBlockHdr(psContext,
						 psDataReaders,
						 &pvPCData,
						 &sPCBlockHdr))
	{
		USP_DBGPRINT(( "USPShaderCreate: Error reading first PC block-hdr"));
		goto USPShaderCreateFinish;
	}

	if	(sPCBlockHdr.uType != USP_PC_BLOCK_TYPE_PROGDESC)
	{
		USP_DBGPRINT(( "USPShaderCreate: First PC block must be PROGDESC\n"));
		goto USPShaderCreateFinish;
	}

	/*
		Allocate and initialise a new USP shader
	*/
	psUSPShader = (PUSP_SHADER)psContext->pfnAlloc(sizeof(USP_SHADER));
	if	(!psUSPShader)
	{
		USP_DBGPRINT(( "USPShaderCreate: Failed to allocate new USP shader\n"));
		goto USPShaderCreateFinish;
	}
	memset(psUSPShader, 0, sizeof(*psUSPShader));

	/*
		Read all the data blocks until we reach the special END block
	*/
	eLastPCBlockType = (USP_PC_BLOCK_TYPE)sPCBlockHdr.uType;
	do
	{
		switch (sPCBlockHdr.uType)
		{
			case USP_PC_BLOCK_TYPE_PROGDESC:
			{
				/*
					Only one program-description block allowed at present
				*/
				if	(psUSPShader->psProgDesc)
				{
					USP_DBGPRINT(( "USPShaderCreate: Multiple PC program description blocks present\n"));
					goto USPShaderCreateFinish;
				}

				/*
					Read the overall program description
				*/
				if	(!ReadPCProgDescBlock(psContext,
										  psUSPShader,
										  psDataReaders,
										  &pvPCData))
				{
					USP_DBGPRINT(( "USPShaderCreate: Failed to read PC prog desc block\n"));
					goto USPShaderCreateFinish;
				}

				break;
			}

			case USP_PC_BLOCK_TYPE_HWCODE:
			{
				/*
					Create a USP instruciton-block for the precompiled code
				*/
				if	(!ReadPCHWCodeBlock(psContext,
										psUSPShader,
										psDataReaders,
										&pvPCData))
				{
					USP_DBGPRINT(( "USPShaderCreate: Failed to read PC inst block\n"));
					goto USPShaderCreateFinish;
				}

				break;
			}

			case USP_PC_BLOCK_TYPE_BRANCH:
			{
				/*
					Create USP data for the precompiled branch info
				*/
				if	(!ReadPCBranchBlock(psContext,
										psUSPShader,
										psDataReaders,
										&pvPCData))
				{
					USP_DBGPRINT(( "USPShaderCreate: Failed to read PC branch block\n"));
					goto USPShaderCreateFinish;
				}

				break;
			}

			case USP_PC_BLOCK_TYPE_LABEL:
			{
				/*
					Create USP data for the precompiled branch info
				*/
				if	(!ReadPCLabelBlock(psContext,
									   psUSPShader,
									   psDataReaders,
									   &pvPCData))
				{
					USP_DBGPRINT(( "USPShaderCreate: Failed to read PC label block\n"));
					goto USPShaderCreateFinish;
				}

				break;
			}

			case USP_PC_BLOCK_TYPE_TEXTURE_WRITE:
			{
				/*
					Read the overall program description
				 */
				if (!ReadPCTextureWriteBlock(psContext,
										   psUSPShader,
										   psDataReaders,
										   &pvPCData))
				{
					USP_DBGPRINT(( "USPShaderCreate: Failed to read PC write texture block\n"));
					goto USPShaderCreateFinish;
				}
				
				break;
			}

			case USP_PC_BLOCK_TYPE_SAMPLE:
			{
				/*
					Read the overall program description
				*/
				if	(!ReadPCSampleBlock(psContext,
										psUSPShader,
										psDataReaders,
										&pvPCData))
				{
					USP_DBGPRINT(( "USPShaderCreate: Failed to read PC sample block\n"));
					goto USPShaderCreateFinish;
				}

				break;
			}

			case USP_PC_BLOCK_TYPE_SAMPLEUNPACK:
			{
				/*
					Read the overall program description
				*/
				if	(!ReadPCSampleUnpackBlock(psContext,
										psUSPShader,
										psDataReaders,
										&pvPCData))
				{
					USP_DBGPRINT(( "USPShaderCreate: Failed to read PC sample block\n"));
					goto USPShaderCreateFinish;
				}

				break;
			}

			default:
			{
				USP_DBGPRINT(( "USPShaderCreate: Unrecognised PC block type encountered\n"));
				goto USPShaderCreateFinish;
			}
		}

		/*
			Read the header of the next PC data-block
		*/
		eLastPCBlockType = (USP_PC_BLOCK_TYPE)sPCBlockHdr.uType;

		if	(!ReadPCBlockHdr(psContext,
							 psDataReaders,
							 &pvPCData,
							 &sPCBlockHdr))
		{
			USP_DBGPRINT(( "USPShaderCreate: Error reading next PC block-hdr"));
			goto USPShaderCreateFinish;
		}

	} while (sPCBlockHdr.uType != USP_PC_BLOCK_TYPE_END);

	/*
		Verify that we have read the expected amount of data	
	*/
	uSizeRead = (IMG_PUINT8)pvPCData - (IMG_PUINT8)pvPCShader;
	uSizeRead -= sizeof(USP_PC_SHADER);

	if	(uSizeRead != uSize)
	{
		USP_DBGPRINT(("USPShaderCreate: Unexpected shader size"));
		goto USPShaderCreateFinish;
	}

	/*
		Setup the input-data for the shader
	*/
	if	(!SetupInputData(psUSPShader, psContext))
	{
		USP_DBGPRINT(( "USPShaderCreate: Failed to create shader input-data\n"));
		goto USPShaderCreateFinish;
	}

	/*
		Record whether there was a label at the end of the shader
	*/
	if	(eLastPCBlockType == USP_PC_BLOCK_TYPE_LABEL)
	{
		psUSPShader->psProgDesc->uHWFlags |= UNIFLEX_HW_FLAGS_LABEL_AT_END;
	}

	/*
		Add an empty instruction block to the end of the shader for
		any additional instruciton we may need to append.

		NB: Currently, this could be instruction to reset the MOE state, and a 
			MOV for result-register remapping.
	*/
	if	(psUSPShader->psInstBlocksEnd)
	{
		psFinalMOEState = &psUSPShader->psInstBlocksEnd->sFinalMOEState;
	}
	else
	{
		psFinalMOEState = (PUSP_MOESTATE)(&g_sDefaultUSPMOEState);
	}

	psInstBlock = USPInstBlockCreate(psContext, 
									 psUSPShader, 
									 3, 
									 0, 
									 IMG_TRUE,
									 psFinalMOEState);
	if	(!psInstBlock)
	{
		USP_DBGPRINT(( "USPShaderCreate: Failed to create USP inst block\n"));
		goto USPShaderCreateFinish;
	}

	/*
		Add the final instruction block to the shader
	*/
	USPShaderAddInstBlock(psUSPShader, psInstBlock);

	/*
		Allocate space for texture control word list
	*/
	{
		IMG_UINT32 uSampleCnt = 0;
		TEX_SAMPLES_LIST *psTexFormats;
		
		for(psTexFormats = (psUSPShader->psUsedTexFormats);
			psTexFormats;
			psTexFormats = psTexFormats->psNext)
		{
			uSampleCnt += (psTexFormats->uCount);
		}
		
		psUSPShader->uTotalSmpTexCtrWrds = uSampleCnt;
		
		if	(uSampleCnt)
		{
			psUSPShader->psTexCtrWrds = 
				(PUSP_SHDR_TEX_CTR_WORDS)
				psContext->pfnAlloc(
				uSampleCnt*sizeof(USP_SHDR_TEX_CTR_WORDS));

			if	(!psUSPShader->psTexCtrWrds)
			{
				USP_DBGPRINT(( "USPShaderCreate: Failed to allocate space for texture control words\n"));
				goto USPShaderCreateFinish;
			}
		}

		memset(psUSPShader->psTexCtrWrds, 0, uSampleCnt*sizeof(USP_SHDR_TEX_CTR_WORDS));

	}

	/*
		Shader created OK
	*/
	bSuccess = IMG_TRUE;

	USPShaderCreateFinish:

	/*
		Cleanup the partialiiy completed USP shader on failure
	*/
	if	(!bSuccess)
	{
		if	(psUSPShader)
		{
			USPShaderDestroy(psContext, psUSPShader);
			psUSPShader = IMG_NULL;
		}
	}

	return psUSPShader;
}

/*****************************************************************************
 End of file (USPSHADER.C)
*****************************************************************************/

