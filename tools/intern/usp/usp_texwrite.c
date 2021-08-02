 /*****************************************************************************
 Name           : usp_imgwrite.c

 Title          : Texture write handling

 C Author       :

 Created        : 13/07/2010

 Copyright      : 2010 by Imagination Technologies Limited.
                : All rights reserved. No part of this software, either
                : material or conceptual may be copied or distributed,
                : transmitted, transcribed, stored in a retrieval system or
                : translated into any human or computer language in any form
                : by any means, electronic, mechanical, manual or otherwise,
                : or disclosed to third parties without the express written
                : permission of Imagination Technologies Limited,
                : Home Park Estate, Kings Langley, Hertfordshire,
                : WD4 8LZ, U.K.

 Description    : Routines to handle fixing-up of texture writes upon finalising
 				  a shader.

 Program Type   : 32-bit DLL

 Version        : $Revision: 1.21 $

 Modifications :
 $Log: usp_texwrite.c $
*****************************************************************************/

#include "img_types.h"
#include "sgxdefs.h"
#include "usp_typedefs.h"
#include "usp_texwrite.h"
#include "usp.h"
#include "hwinst.h"
#include "uspshrd.h"
#include "usp_instblock.h"

static IMG_BOOL AddTextureWriteCode(PUSP_TEXTURE_WRITE psTextureWrite, PUSP_CONTEXT psContext);

static IMG_BOOL CalculateTextureOffset(PUSP_TEXTURE_WRITE psTextureWrite, 
									   PUSP_CONTEXT 	  psContext,
									   PUSP_REG			  psDest,
									   PUSP_REG			  psTemp0,
									   PUSP_REG			  psTemp1);

#if !defined(SGX_FEATURE_USE_32BIT_INT_MAD)
static IMG_BOOL USPGenerateU16Multiply(PUSP_TEXTURE_WRITE 	psTextureWrite,
									   PUSP_CONTEXT			psContext,
									   PUSP_REG 			psDest,
									   PUSP_REG				psSrc0,
									   PUSP_REG				psSrc1);

static IMG_BOOL USPGenerateIMAE(PUSP_TEXTURE_WRITE 	psTextureWrite,
								PUSP_CONTEXT		psContext,
								PUSP_REG 			psDest,
								PUSP_REG			psSrc0,
								PUSP_REG			psSrc1,
								PUSP_REG			psSrc2);
#endif /* defined(SGX_FEATURE_USE_32BIT_INT_MAD) */

static IMG_BOOL USPTextureWritePackFromU16(PUSP_TEXTURE_WRITE 	psTextureWrite, 
										   PUSP_CONTEXT			psContext,
										   PUSP_REG				psAddr,
										   PUSP_REG				asTemps,
										   IMG_UINT32			uNumTemps,
										   IMG_UINT32*			puTempsUsed);

static IMG_BOOL USPTextureWritePackFromI16(PUSP_TEXTURE_WRITE 	psTextureWrite, 
										   PUSP_CONTEXT			psContext,
										   PUSP_REG				psAddr,
										   PUSP_REG				asTemps,
										   IMG_UINT32			uNumTemps,
										   IMG_UINT32*			puTempsUsed);

static IMG_BOOL USPTextureWritePackFromU32(PUSP_TEXTURE_WRITE 	psTextureWrite, 
										   PUSP_CONTEXT			psContext,
										   PUSP_REG				psAddr,
										   PUSP_REG				asTemps,
										   IMG_UINT32			uNumTemps,
										   IMG_UINT32*			puTempsUsed);

static IMG_BOOL USPTextureWritePackFromI32(PUSP_TEXTURE_WRITE 	psTextureWrite, 
										   PUSP_CONTEXT			psContext,
										   PUSP_REG				psAddr,
										   PUSP_REG				asTemps,
										   IMG_UINT32			uNumTemps,
										   IMG_UINT32*			puTempsUsed);

static IMG_BOOL USPTextureWritePackFromF32(PUSP_TEXTURE_WRITE 	psTextureWrite, 
										   PUSP_CONTEXT			psContext,
										   PUSP_REG				psAddr,
										   PUSP_REG				asTemps,
										   IMG_UINT32			uNumTemps,
										   IMG_UINT32*			puTempsUsed);

static IMG_UINT32 GetTextureWriteSize(USP_TEXTURE_FORMAT eFmt);


static IMG_VOID SwizzleChannels(PUSP_REG *ppsSwizzled, PUSP_REG asChannel, PUSP_CHAN_SWIZZLE aeChanSwizzle);

/*****************************************************************************
 Name:		USPTextureWriteGenerateCode

 Purpose:	Generate instructions for a texture write

 Inputs:	psSample	- The texture write to generate code for,
			psShader	- The USP shader containing the texture write
			psContext	- The current USP execution context

 Outputs:	none

 Returns:	TRUE/FALSE to indicate success or failure
*****************************************************************************/
IMG_INTERNAL IMG_BOOL USPTextureWriteGenerateCode(PUSP_TEXTURE_WRITE psTextureWrite,
												  PUSP_SHADER		 psShader,
												  PUSP_CONTEXT		 psContext)
{
	IMG_UINT32 	uFirstUnusedTemp = psTextureWrite->uTempRegStartNum;
	IMG_UINT32  i 				 = 0;
	PUSP_REG	psTempRegs		 = IMG_NULL;

	IMG_UINT32  uTempsUsed		 = 0;

	USP_UNREFERENCED_PARAMETER(psShader);

	psTempRegs = psContext->pfnAlloc(sizeof(USP_REG) * psTextureWrite->uMaxNumTemps);

	if  (!psTempRegs)
	{
		USP_DBGPRINT(("USPTextureWriteGenerataeCode: Error allocating memory for temporary registers\n"));
		return IMG_FALSE;
	}

	/* Encode the temporary registers */
	for(i = 0; i < psTextureWrite->uMaxNumTemps; i++)
	{
		psTempRegs[i].eType   = USP_REGTYPE_TEMP;
		psTempRegs[i].uNum    = uFirstUnusedTemp + i;
		psTempRegs[i].eFmt    = USP_REGFMT_UNKNOWN;
		psTempRegs[i].uComp   = 0;
		psTempRegs[i].eDynIdx = USP_DYNIDX_NONE;
	}

	/*
		Add instructions to calculate the offset into the texture
	*/
	if	(!CalculateTextureOffset(psTextureWrite, 
								 psContext,
								 &psTempRegs[0],
								 &psTempRegs[1],
								 &psTempRegs[2]))
	{
		USP_DBGPRINT(("USPTextureWriteGenerateCode: Error calculating texture offset\n"));
		return IMG_FALSE;
	}

	psTextureWrite->uTempRegCount += 3;

	/*
	    Add instruction to pack from the format we are given to the image format
	*/
	switch(psTextureWrite->asChannel[0].eFmt)
	{
		case USP_REGFMT_U16:		USPTextureWritePackFromU16(psTextureWrite,
															   psContext,
															   &psTempRegs[0], /* The base address */
															   &psTempRegs[1], /* Starting temp register */
															   psTextureWrite->uMaxNumTemps-1, /* Number of temps available */
															   &uTempsUsed);
									break;

		case USP_REGFMT_I16:		USPTextureWritePackFromI16(psTextureWrite,
															   psContext,
															   &psTempRegs[0], /* The base address */
															   &psTempRegs[1], /* Starting temp register */
															   psTextureWrite->uMaxNumTemps-1, /* Number of temps available */
															   &uTempsUsed);
									break;

		case USP_REGFMT_U32:		USPTextureWritePackFromU32(psTextureWrite,
															   psContext,
															   &psTempRegs[0], /* The base address */
															   &psTempRegs[1], /* Starting temp register */
															   psTextureWrite->uMaxNumTemps-1, /* Number of temps available */
															   &uTempsUsed);
									break;

		case USP_REGFMT_I32:		USPTextureWritePackFromI32(psTextureWrite,
															   psContext,
															   &psTempRegs[0], /* The base address */
															   &psTempRegs[1], /* Starting temp register */
															   psTextureWrite->uMaxNumTemps-1, /* Number of temps available */
															   &uTempsUsed);
									break;

		case USP_REGFMT_F32:		USPTextureWritePackFromF32(psTextureWrite,
															   psContext,
															   &psTempRegs[0], /* The base address */
															   &psTempRegs[1], /* Starting temp register */
															   psTextureWrite->uMaxNumTemps-1, /* Number of temps available */
															   &uTempsUsed);
									break;
		default:
		{
			USP_DBGPRINT(("USPTextureWriteGenerateCode : Invalid source channel format\n"));
			return IMG_FALSE;
		}
	}

#if 0
	/* Insert an IDF after the store */
	HWInstEncodeIDFInst(&psTextureWrite->asInsts[psTextureWrite->uInstCount]);
	psTextureWrite->aeOpcodeForInsts[psTextureWrite->uInstCount] = USP_OPCODE_IDF;
	psTextureWrite->auInstDescFlagsForInsts[psTextureWrite->uInstCount] = psTextureWrite->uInstDescFlags;
	psTextureWrite->uInstCount++;

	/* Insert a WDF after the IDF */
	HWInstEncodeWDFInst(&psTextureWrite->asInsts[psTextureWrite->uInstCount], 0);
	psTextureWrite->aeOpcodeForInsts[psTextureWrite->uInstCount] = USP_OPCODE_WDF;
	psTextureWrite->auInstDescFlagsForInsts[psTextureWrite->uInstCount] = psTextureWrite->uInstDescFlags;
	psTextureWrite->uInstCount++;
#endif

	/*
	   If the number of temps used to convert and store the code + 1 (because
	   the address to store to is in a temp), update the temp counter
	*/
	if((uTempsUsed + 1) > psTextureWrite->uTempRegCount)
	{
		psTextureWrite->uTempRegCount = uTempsUsed + 1;
	}

	/*
		Add the generated HW-instructions to the associated USP instruction
		block
	*/
	if	(!AddTextureWriteCode(psTextureWrite, psContext))
	{
		USP_DBGPRINT(("USPTextureWriteGenerateCode: Error adding HW-insts to block\n"));
		return IMG_FALSE;
	}

	psContext->pfnFree(psTempRegs);

	return IMG_TRUE;
}

/******************************************************************************
 Name:		AddTextureWriteCode

 Purpose:	Add the HW instructions generated for a texture write to the associated
			code block

 Inputs:	psTextureWrite	- The texture write to generate code for
			psContext		- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL AddTextureWriteCode(PUSP_TEXTURE_WRITE psTextureWrite, PUSP_CONTEXT psContext)
{
	IMG_UINT32	i;

	/*
		Add the instructions to perform the texture write
	*/
	for	(i = 0; i < psTextureWrite->uInstCount; i++)
	{
		if	(!USPInstBlockInsertHWInst(psTextureWrite->psInstBlock,
									   IMG_NULL,
									   psTextureWrite->aeOpcodeForInsts[i],
									   &psTextureWrite->asInsts[i],
									   psTextureWrite->auInstDescFlagsForInsts[i],
									   psContext,
									   IMG_NULL))
		{
			USP_DBGPRINT(("AddTextureWriteCode: Error adding HW-inst to block\n"));
			return IMG_FALSE;
		}
	}	

	return IMG_TRUE;
}

/******************************************************************************
 Name:		CalculateTextureOffset

 Purpose:	Emit HW instructions to calculate the image offset

 Inputs:	psTextureWrite	- The texture write to generate code for
			psContext		- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL CalculateTextureOffset(PUSP_TEXTURE_WRITE psTextureWrite, 
									   PUSP_CONTEXT		  psContext,
									   PUSP_REG			  psDest,
									   PUSP_REG			  psTemp0,
									   PUSP_REG			  psTemp1)
{
	IMG_UINT32  uWriteID		= psTextureWrite->uWriteID;
	IMG_UINT32 	uHWInstCount 	= psTextureWrite->uInstCount;
	IMG_UINT32 	uInstDescFlags 	= psTextureWrite->uInstDescFlags;
	IMG_UINT32  uElementSize	= 0;
	
	USP_REG     sElementSize	= {0};
	USP_REG     sOne			= {0};
	USP_REG     sZero			= {0};

	PHW_INST 	psHWInst 		= IMG_NULL;

	PVR_UNREFERENCED_PARAMETER(psHWInst);
	PVR_UNREFERENCED_PARAMETER(uHWInstCount);
	PVR_UNREFERENCED_PARAMETER(uInstDescFlags);

	uElementSize = GetTextureWriteSize(psContext->asSamplerDesc[uWriteID].sTexFormat.eTexFmt);

	if( !uElementSize )
	{
		USP_DBGPRINT(("Invalid texture format (%u)\n",psContext->asSamplerDesc[uWriteID].sTexFormat.eTexFmt));
		return IMG_FALSE;
	}
	
	sElementSize.eType 	 = USP_REGTYPE_IMM;
	sElementSize.uNum  	 = uElementSize;
	sElementSize.eFmt  	 = USP_REGFMT_U16;
	sElementSize.uComp 	 = 0;
	sElementSize.eDynIdx = USP_DYNIDX_NONE;

	sOne.eType 	 	= USP_REGTYPE_IMM;
	sOne.uNum  	 	= 1;
	sOne.eFmt  	 	= USP_REGFMT_U16;
	sOne.uComp 	 	= 0;
	sOne.eDynIdx 	= USP_DYNIDX_NONE;

	sZero.eType 	= USP_REGTYPE_IMM;
	sZero.uNum  	= 0;
	sZero.eFmt    	= USP_REGFMT_U16;
	sZero.uComp   	= 0;
	sZero.eDynIdx 	= USP_DYNIDX_NONE;

#if defined(SGX_FEATURE_USE_32BIT_INT_MAD)
	/* Get HW Instruction */
	psHWInst = &psTextureWrite->asInsts[uHWInstCount];

	/* temp0 = element_size * x */
	if(!HWInstEncodeIMA32Inst(psHWInst,
							  IMG_TRUE, /* bSkipInv */
							  psTemp0,
							  &psTextureWrite->asCoords[0],
							  &sElementSize,
							  &sZero))
	{
		USP_DBGPRINT(("Unable to encode HW IMA32 instruction\n"));
		return IMG_FALSE;
	}

	/* set description */
	psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_IMA32;
	psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

	uHWInstCount++;

	psHWInst = &psTextureWrite->asInsts[uHWInstCount];

	/* temp1 = (stride * y) + base-addr */
	if(!HWInstEncodeIMA32Inst(psHWInst,
							  IMG_TRUE, /* bSkipInv */
							  psTemp1,
							  &psTextureWrite->sStride,
							  &psTextureWrite->asCoords[1],
						 	  &psTextureWrite->sBaseAddr))
	{
		USP_DBGPRINT(("Unable to encode HW IMA32 instruction\n"));
		return IMG_FALSE;
	}

	/* set description */
	psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_IMA32;
	psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

	uHWInstCount++;

	psHWInst = &psTextureWrite->asInsts[uHWInstCount];

	/* dest = (temp0 * 1) + temp1 */
	if(!HWInstEncodeIMA32Inst(psHWInst,
							  IMG_TRUE, /* bSkipInv */
							  psDest,
							  psTemp0,
							  &sOne,
						 	  psTemp1))
	{
		USP_DBGPRINT(("Unable to encode HW IMA32 instruction\n"));
		return IMG_FALSE;
	}

	/* set description */
	psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_IMA32;
	psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

	uHWInstCount++;

	psTextureWrite->uInstCount = uHWInstCount;
#else
	USPGenerateU16Multiply(psTextureWrite,
						   psContext,
						   psTemp0,
						   &psTextureWrite->asCoords[0],
						   &sElementSize);

	/* temp1 = (stride * y) + base-addr */
	USPGenerateIMAE(psTextureWrite,
					psContext,
					psTemp1,
					&psTextureWrite->sStride,
					&psTextureWrite->asCoords[1],
					&psTextureWrite->sBaseAddr);

	/* dest = (temp0 * 1) + temp1 */
	USPGenerateIMAE(psTextureWrite,
					psContext,
					psDest,
					psTemp0,
					&sOne,
					psTemp1);
#endif /* defined(SGX_FEATURE_USE_VEC34) */

	return IMG_TRUE;
}

#if !defined(SGX_FEATURE_USE_32BIT_INT_MAD)
/******************************************************************************
 Name:		USPGenerateU16Multiply

 Purpose:	Emit HW instructions to calculate an multiply of
 			two unsigned 16-bit integers.

 Inputs:	psTextureWrite	- The texture write to generate code for
 			psDest			- The dest register
			psSrc0			- The first source register
			psSrc1			- The second source register

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL USPGenerateU16Multiply(PUSP_TEXTURE_WRITE 	psTextureWrite,
									   PUSP_CONTEXT			psContext,
									   PUSP_REG 			psDest,
									   PUSP_REG				psSrc0,
									   PUSP_REG				psSrc1)
{
	IMG_UINT32 uHWInstCount   = psTextureWrite->uInstCount;
	IMG_UINT32 uInstDescFlags = psTextureWrite->uInstDescFlags;
	USP_REG	   sZero		  = {0};
	PHW_INST   psHWInst;

	PUSP_REG	psU16MulSrc0	= IMG_NULL;

	PVR_UNREFERENCED_PARAMETER(psContext);

	sZero.eType 	= USP_REGTYPE_IMM;
	sZero.uNum  	= 0;
	sZero.eFmt  	= USP_REGFMT_U16;
	sZero.uComp 	= 0;
	sZero.eDynIdx 	= USP_DYNIDX_NONE;

	/* Get HW Instruction */
	psHWInst = &psTextureWrite->asInsts[uHWInstCount];

	/*
	   If the source 0 for the U16 multiply is an SA
	   move it into a temp first
	*/
	if(psSrc0->eType == USP_REGTYPE_SA ||
	   psSrc0->eType == USP_REGTYPE_SPECIAL)
	{
#if !defined(SGX_FEATURE_USE_VEC34)
		if(!HWInstEncodeMOVInst(psHWInst,
								1, /* uRepeatCount */
								IMG_TRUE, /* bSkipInv */
								psDest,
								psSrc0))
		{
			USP_DBGPRINT(("Unable to encode HW MOV instruction\n"));
			return IMG_FALSE;
		}

		psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_MOVC;
		psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
#else
	{
		USP_REG sImdVal;
		sImdVal.eType	= USP_REGTYPE_IMM;
		sImdVal.uNum	= 0xFFFFFFFF;
		sImdVal.eFmt	= psSrc0->eFmt;
		sImdVal.uComp	= psSrc0->uComp;
		sImdVal.eDynIdx = USP_DYNIDX_NONE;
		if	(!HWInstEncodeANDInst(psHWInst, 
								  1,
								  IMG_FALSE, /* bUsePred */
								  IMG_FALSE, /* bNegatePred */
								  0, /* uPredNum */
								  IMG_TRUE, /* bSkipInv */
								  psDest, 
								  psSrc0, 
								  &sImdVal))
		{
			USP_DBGPRINT(("UnpackTextureDataToF32: Error encoding data movement (AND) HW-inst\n"));
			return IMG_FALSE;
		}

		psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_AND;
	}
#endif

		/* Get the next instruction */
		uHWInstCount++;

		psHWInst = &psTextureWrite->asInsts[uHWInstCount];

		psU16MulSrc0 = psDest;
	}
	else
	{
		psU16MulSrc0 = psSrc0;
	}

	if(!HWInstEncodeIMA16Inst(psHWInst,
						  1, /* uRepeatCount */
						  psDest,
						  psU16MulSrc0,
						  psSrc1,
						  &sZero))
	{
		USP_DBGPRINT(("Unable to encode HW IMA16 instruction\n"));
		return IMG_FALSE;
	}

	/* set description */
	psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_IMA16;
	psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

	/* Increment HW instruction count */
	psTextureWrite->uInstCount = ++uHWInstCount;

	return IMG_TRUE;
}

/******************************************************************************
 Name:		USPGenerateIMAE

 Purpose:	Emit HW instructions to calculate an IMAE

 Inputs:	psTextureWrite	- The texture write to generate code for
 			psDest			- The dest register
			psSrc0			- The first source register
			psSrc1			- The second source register
			psSrc1			- The third source register

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL USPGenerateIMAE(PUSP_TEXTURE_WRITE 	psTextureWrite,
								PUSP_CONTEXT		psContext,
								PUSP_REG 			psDest,
								PUSP_REG			psSrc0,
								PUSP_REG			psSrc1,
								PUSP_REG			psSrc2)
{
	IMG_UINT32 uHWInstCount   = psTextureWrite->uInstCount;
	IMG_UINT32 uInstDescFlags = psTextureWrite->uInstDescFlags;
	PHW_INST   psHWInst;

	PUSP_REG	psIMAESrc0		= IMG_NULL;

	PVR_UNREFERENCED_PARAMETER(psContext);

	/* Get HW Instruction */
	psHWInst = &psTextureWrite->asInsts[uHWInstCount];

	/*
	   If the source 0 for the U16 multiply is an SA
	   move it into a temp first
	*/
	if(psSrc0->eType == USP_REGTYPE_SA)
	{
#if !defined(SGX_FEATURE_USE_VEC34)
		if(!HWInstEncodeMOVInst(psHWInst,
							1, /* uRepeatCount */
							IMG_TRUE, /* bSkipInv */
							psDest,
							psSrc0))
		{
			USP_DBGPRINT(("Unable to encode HW MOV instruction\n"));
			return IMG_FALSE;
		}

		psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_MOVC;
		psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

		/* Get the next instruction */
		uHWInstCount++;

		psHWInst = &psTextureWrite->asInsts[uHWInstCount];

		psIMAESrc0 = psDest;
#else
	{
		USP_REG sImdVal;
		sImdVal.eType	= USP_REGTYPE_IMM;
		sImdVal.uNum	= 0xFFFFFFFF;
		sImdVal.eFmt	= psSrc0->eFmt;
		sImdVal.uComp	= psSrc0->uComp;
		sImdVal.eDynIdx = USP_DYNIDX_NONE;
		if	(!HWInstEncodeANDInst(psHWInst, 
								  1,
								  IMG_FALSE, /* bUsePred */
								  IMG_FALSE, /* bNegatePred */
								  0, /* uPredNum */
								  IMG_TRUE, /* bSkipInv */
								  psDest, 
								  psSrc0, 
								  &sImdVal))
		{
			USP_DBGPRINT(("UnpackTextureDataToF32: Error encoding data movement (AND) HW-inst\n"));
			return IMG_FALSE;
		}

		psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_AND;
	}
#endif
	}
	else
	{
		psIMAESrc0 = psSrc0;
	}

	if(!HWInstEncodeIMAEInst(psHWInst,
						  1, /* uRepeatCount */
						  psDest,
						  psIMAESrc0,
						  psSrc1,
						  psSrc2))
	{
		USP_DBGPRINT(("Unable to encode HW IMAE instruction\n"));
		return IMG_FALSE;
	}
	
	/* set description */
	psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_IMAE;
	psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

	/* Increment HW instruction count */
	psTextureWrite->uInstCount = ++uHWInstCount;

	return IMG_TRUE;
}
#endif /* defined(SGX_FEATURE_USE_32BIT_INT_MAD) */

/******************************************************************************
 Name:		USPTextureWritePackFromU16

 Purpose:	Emit HW instructions to pack to the right image format
 			and store to the given address.

 Inputs:	psTextureWrite	- The texture write to generate code for
			psContext		- The current USP execution context
			psAddr			- The address the store the data to
			asTemps			- An array of temps
			uNumTemps		- The number of temps in the array
			puTempsUsed		- Used to return the number of temps used

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL USPTextureWritePackFromU16(PUSP_TEXTURE_WRITE 	psTextureWrite,
										   PUSP_CONTEXT			psContext,
										   PUSP_REG				psAddr,
										   PUSP_REG				asTemps,
										   IMG_UINT32			uNumTemps,
										   IMG_UINT32*			puTempsUsed)
{
	IMG_UINT32  uHWInstCount	= psTextureWrite->uInstCount;
	IMG_UINT32  uInstDescFlags  = psTextureWrite->uInstDescFlags;
	PHW_INST	psHWInst;

	USP_REG	sZero                               = {0};
	USP_REG	sOne                                = {0};
	USP_REG	sShiftSize                          = {0};
	PUSP_REG ppsChannel[USP_TEXFORM_CHAN_COUNT] = {IMG_NULL};

	sZero.eType 	= USP_REGTYPE_IMM;
	sZero.uNum  	= 0;
	sZero.eFmt  	= USP_REGFMT_U32;
	sZero.uComp 	= 0;
	sZero.eDynIdx 	= USP_DYNIDX_NONE;

	sOne.eType 		= USP_REGTYPE_IMM;
	sOne.uNum  		= 1;
	sOne.eFmt  		= USP_REGFMT_U32;
	sOne.uComp 		= 0;
	sOne.eDynIdx 	= USP_DYNIDX_NONE;
	
	sShiftSize.eType 	= USP_REGTYPE_IMM;
	sShiftSize.uNum  	= 0;
	sShiftSize.eFmt  	= USP_REGFMT_U32;
	sShiftSize.uComp 	= 0;
	sShiftSize.eDynIdx 	= USP_DYNIDX_NONE;

	/* Get the HW Instruction */
	psHWInst = &psTextureWrite->asInsts[uHWInstCount];

	/* Swizzle the source registers appropriately */
	SwizzleChannels(ppsChannel,
	                psTextureWrite->asChannel,
	                psContext->asSamplerDesc[psTextureWrite->uWriteID].sTexFormat.aeChanSwizzle);

	switch(psContext->asSamplerDesc[psTextureWrite->uWriteID].sTexFormat.eTexFmt)
	{
		case USP_TEXTURE_FORMAT_R8G8B8A8:
	    case USP_TEXTURE_FORMAT_RGBA_U8:
		{
			IMG_UINT32 	i				= 0;

			/* Set the right-shift size */
			sShiftSize.uNum = 8;

#if defined(SGX_FEATURE_USE_VEC34)
			/* 
			   We need one temp for the result, one for the saturated value and
			   one for the channel data, so retrun false if we don't have enough.
			*/
			if(uNumTemps < 3)
			{
				USP_DBGPRINT(("Not enough temps to pack from U16 to U8. Have %d but need 3\n", uNumTemps));
				return IMG_FALSE;
			}

			/* Start by loading the saturation value into a temp */
			
			/* temp[2] = 0xff */
			if(!HWInstEncodeLIMMInst(psHWInst,
									 1, /* uRepeatCount */
									 &asTemps[2],
									 0xff))
			{
				USP_DBGPRINT(("Unable to encode HW LIMM instruction\n"));
				return IMG_FALSE;
			}

			/* Set the opcode */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_LIMM;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/*
			   On vector cores we do the pack inside the for-loop because we 
			   need 4 packs instead of the two we need on non-vector cores 
			*/
			for(i = 0; i < 4; i++)
			{
				/*
				  shr temp[1], channel[i], 8
				*/
				if(!HWInstEncodeSHRInst(psHWInst,
										1, /* uRepeatCount */
										IMG_TRUE, /* bSkipInv */
										&asTemps[1],
										ppsChannel[i],
										&sShiftSize))
				{
					USP_DBGPRINT(("Unable to encode HW SHR instruction\n"));
					return IMG_FALSE;
				}

				/* Set the opcode */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_SHR;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/* temp1 = temp1 : 0xff ? channel[i] */
				if(!HWInstEncodeMOVCInst(psHWInst,
										 1, /* uRepeatCount */
										 EURASIA_USE1_EPRED_ALWAYS, /* uPred */
										 IMG_TRUE, /* bTestDst */
										 IMG_TRUE, /* bSkipInv */
										 &asTemps[1],
										 &asTemps[1],
				                         ppsChannel[i], /* saturation max */
				                         &asTemps[2]))
				{
					USP_DBGPRINT(("Unable to encode HW MOVC instruction\n"));
					return IMG_FALSE;
				}

				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_VMOV;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				if(!HWInstEncodePCKUNPCKInst(psHWInst,
											 IMG_TRUE, /* bSkipInv */
								  			 USP_PCKUNPCK_FMT_U8,
								  			 USP_PCKUNPCK_FMT_U8,
								  			 IMG_FALSE, /* bScaleEnable */
											 1 << i, /* Write the current component */
							  				 &asTemps[0],
											 0,
											 1,
											 &asTemps[1],
											 &asTemps[1]))
				{
					USP_DBGPRINT(("Unable to encode HW PCKU8U8 instruction\n"));
					return IMG_FALSE;
				}

				/* set description */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];
			}

			if(!HWInstEncodeSTInst(psHWInst,
								   IMG_TRUE, /* bSkipInv */
								   1, /* uRepeatCount */
								   psAddr,
								   &sZero,
								   &asTemps[0]))
			{
				USP_DBGPRINT(("Unable to encode HW ST instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_ST;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Increment HW instruction count */
			psTextureWrite->uInstCount = ++uHWInstCount;

			/* This path used 5 temps (one for each channel and one to pack the result into) */
			*puTempsUsed = 3;
#else
			/* 
			   We need one temp per channel and one for the packed
			   result, so retrun false if we don't have enough.
			*/
			if(uNumTemps < 5)
			{
				USP_DBGPRINT(("Not enough temps to pack from U16 to U8. Have %d but need 5\n", uNumTemps));
				return IMG_FALSE;
			}

			/* Start by loading the saturation value into a temp */
			
			/* temp[4] = 0xff */
			if(!HWInstEncodeLIMMInst(psHWInst,
									 1, /* uRepeatCount */
									 &asTemps[4],
									 0xff))
			{
				USP_DBGPRINT(("Unable to encode HW LIMM instruction\n"));
				return IMG_FALSE;
			}

			/* Set the opcode */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_LIMM;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/*
			   For each channel we pack from U16 to U8 and
			   store it in a temp
			*/
			for(i = 0; i < 4; i++)
			{
				/*
				  shr temp[i], channel[i], 8
				*/
				if(!HWInstEncodeSHRInst(psHWInst,
										1, /* uRepeatCount */
										IMG_TRUE, /* bSkipInv */
										&asTemps[i],
										ppsChannel[i],
										&sShiftSize))
				{
					USP_DBGPRINT(("Unable to encode HW SHR instruction\n"));
					return IMG_FALSE;
				}

				/* Set the opcode */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_SHR;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/* temp1 = temp1 : 0xff ? channel[i] */
				if(!HWInstEncodeMOVCInst(psHWInst,
										 1, /* uRepeatCount */
										 EURASIA_USE1_EPRED_ALWAYS, /* uPred */
										 IMG_TRUE, /* bTestDst */
										 IMG_TRUE, /* bSkipInv */
										 &asTemps[i],
										 &asTemps[i],
										 &asTemps[4], /* saturation max */
										 ppsChannel[i]))
				{
					USP_DBGPRINT(("Unable to encode HW MOVC instruction\n"));
					return IMG_FALSE;
				}

				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_MOVC;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];
			}
		

			/*
			   So at this point the packed data for this channel is in
			   temp0 to temp3 and we need to pack it into the register 
			   we are going to store.
			*/
			if(!HWInstEncodePCKUNPCKInst(psHWInst,
										 IMG_TRUE, /* bSkipInv */
							  			 USP_PCKUNPCK_FMT_U8,
							  			 USP_PCKUNPCK_FMT_U8,
							  			 IMG_FALSE, /* bScaleEnable */
										 0xC, /* Write the top half */
							  			 &asTemps[4],
										 0,
										 1,
										 &asTemps[2],
										 &asTemps[3]))
			{
				USP_DBGPRINT(("Unable to encode HW PCKU8U8 instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/* We have written the top half so now write the bottom half */
			if(!HWInstEncodePCKUNPCKInst(psHWInst,
										 IMG_TRUE, /* bSkipInv */
							  			 USP_PCKUNPCK_FMT_U8,
							  			 USP_PCKUNPCK_FMT_U8,
							  			 IMG_FALSE, /* bScaleEnable */
										 0x3, /* Write the bottom half */
							  			 &asTemps[4],
										 0,
										 1,
										 &asTemps[0],
										 &asTemps[1]))
			{
				USP_DBGPRINT(("Unable to encode HW PCKU8U8 instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			if(!HWInstEncodeSTInst(psHWInst,
								   IMG_TRUE, /* bSkipInv */
								   1, /* uRepeatCount */
								   psAddr,
								   &sZero,
								   &asTemps[4]))
			{
				USP_DBGPRINT(("Unable to encode HW ST instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_ST;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Increment HW instruction count */
			psTextureWrite->uInstCount = ++uHWInstCount;

			/* This path used 5 temps (one for each channel and one to pack the result into) */
			*puTempsUsed = 5;
#endif /* defined(SGX_FEATURE_USE_VEC34) */
			
			break;
		}

		case USP_TEXTURE_FORMAT_RGBA_U16:
		{
			IMG_UINT32	i			= 0;
			USP_REG		sOffset		= {0};

			sOffset.eType 	= USP_REGTYPE_IMM;
			sOffset.uNum  	= 0;
			sOffset.eFmt  	= USP_REGFMT_U32;
			sOffset.uComp 	= 0;
			sOffset.eDynIdx = USP_DYNIDX_NONE;

			if(uNumTemps < 1)
			{
				USP_DBGPRINT(("Not enough temps to pack from U16 to U8. Have %d but need 1\n", uNumTemps));
				return IMG_FALSE;
			}

			for(i = 0; i < 2; i++)
			{
				/*
				   So at this point the packed data for this channel is in
				   temp0 to temp3 and we need to pack it into the register 
				   we are going to store.
				   */
				if(!HWInstEncodePCKUNPCKInst(psHWInst,
							IMG_TRUE, /* bSkipInv */
							USP_PCKUNPCK_FMT_U16,
							USP_PCKUNPCK_FMT_U16,
							IMG_FALSE, /* bScaleEnable */
							0xF,
							&asTemps[0],
							0,
							1,
							ppsChannel[(2*i) + 0],
							ppsChannel[(2*i) + 1]))
				{
					USP_DBGPRINT(("Unable to encode HW PCKU16U16 instruction\n"));
					return IMG_FALSE;
				}

				/* set description */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				sOffset.uNum = i >> 1;

				if(!HWInstEncodeSTInst(psHWInst,
									   IMG_TRUE, /* bSkipInv */
									   1, /* uRepeatCount */
									   psAddr,
									   &sOffset,
									   &asTemps[0]))
				{
					USP_DBGPRINT(("Unable to encode HW ST instruction\n"));
					return IMG_FALSE;
				}

				/* set description */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_ST;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];
			}

			/* Update the instruction count */
			psTextureWrite->uInstCount = uHWInstCount;

			*puTempsUsed = 1;
			break;
		}

		default: USP_DBGPRINT(("Unsupported texture write format %d for write ID %d\n",
							 psContext->asSamplerDesc[psTextureWrite->uWriteID].sTexFormat.eTexFmt,
							 psTextureWrite->uWriteID));
				 break;
	}

	return IMG_TRUE;
}

/******************************************************************************
 Name:		USPTextureWritePackFromI16

 Purpose:	Emit HW instructions to pack to the right image format
 			and store to the given address.

 Inputs:	psTextureWrite	- The texture write to generate code for
			psContext		- The current USP execution context
			psAddr			- The address the store the data to
			asTemps			- An array of temps
			uNumTemps		- The number of temps in the array
			puTempsUsed		- Used to return the number of temps used

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL USPTextureWritePackFromI16(PUSP_TEXTURE_WRITE 	psTextureWrite,
										   PUSP_CONTEXT			psContext,
										   PUSP_REG				psAddr,
										   PUSP_REG				asTemps,
										   IMG_UINT32			uNumTemps,
										   IMG_UINT32*			puTempsUsed)
{
	IMG_UINT32 uHWInstCount   = psTextureWrite->uInstCount;
	IMG_UINT32 uInstDescFlags = psTextureWrite->uInstDescFlags;
	PHW_INST   psHWInst;

	USP_REG	sZero                               = {0};
	USP_REG	sOne                                = {0};
	USP_REG	sShiftSize                          = {0};
	PUSP_REG ppsChannel[USP_TEXFORM_CHAN_COUNT] = {IMG_NULL};

	sZero.eType 	= USP_REGTYPE_IMM;
	sZero.uNum  	= 0;
	sZero.eFmt  	= USP_REGFMT_U32;
	sZero.uComp 	= 0;
	sZero.eDynIdx 	= USP_DYNIDX_NONE;

	sOne.eType 		= USP_REGTYPE_IMM;
	sOne.uNum  		= 1;
	sOne.eFmt  		= USP_REGFMT_U32;
	sOne.uComp 		= 0;
	sOne.eDynIdx 	= USP_DYNIDX_NONE;
	
	sShiftSize.eType 	= USP_REGTYPE_IMM;
	sShiftSize.uNum  	= 0;
	sShiftSize.eFmt  	= USP_REGFMT_U32;
	sShiftSize.uComp 	= 0;
	sShiftSize.eDynIdx 	= USP_DYNIDX_NONE;

	/* Get the HW Instruction */
	psHWInst = &psTextureWrite->asInsts[uHWInstCount];

	/* Swizzle the source registers appropriately */
	SwizzleChannels(ppsChannel,
	                psTextureWrite->asChannel,
	                psContext->asSamplerDesc[psTextureWrite->uWriteID].sTexFormat.aeChanSwizzle);
	
	switch(psContext->asSamplerDesc[psTextureWrite->uWriteID].sTexFormat.eTexFmt)
	{
		case USP_TEXTURE_FORMAT_R8G8B8A8:
	    case USP_TEXTURE_FORMAT_RGBA_S8:
		{
			USP_REG		sPosMax			= {0};
			USP_REG		sPosMaxMask		= {0};
			USP_REG		sNegMaxMask		= {0};

			IMG_UINT32 	i				= 0;

			sPosMax.eType 	= USP_REGTYPE_IMM;
			sPosMax.uNum  	= 0x7f;
			sPosMax.eFmt  	= USP_REGFMT_U32;
			sPosMax.uComp 	= 0;
			sPosMax.eDynIdx = USP_DYNIDX_NONE;

			sPosMaxMask.eType 	= USP_REGTYPE_IMM;
			sPosMaxMask.uNum  	= 0x7f80;
			sPosMaxMask.eFmt  	= USP_REGFMT_U32;
			sPosMaxMask.uComp 	= 0;
			sPosMaxMask.eDynIdx = USP_DYNIDX_NONE;

			sNegMaxMask.eType 	= USP_REGTYPE_IMM;
			sNegMaxMask.uNum  	= 0xff80;
			sNegMaxMask.eFmt  	= USP_REGFMT_U32;
			sNegMaxMask.uComp 	= 0;
			sNegMaxMask.eDynIdx = USP_DYNIDX_NONE;

			/* Set the right-shift size */
			sShiftSize.uNum = 8;

#if defined(SGX_FEATURE_USE_VEC34)
			/* 
			   We need one temp for the result, one for the saturated value and
			   one for the channel data, so retrun false if we don't have enough.
			*/
			if(uNumTemps < 5)
			{
				USP_DBGPRINT(("Not enough temps to pack from S32 to S8. Have %d but need 5\n", uNumTemps));
				return IMG_FALSE;
			}

			/* Start by loading the saturation value into a temp */
			
			/* temp[2] = 0x80 */
			if(!HWInstEncodeLIMMInst(psHWInst,
									 1, /* uRepeatCount */
									 &asTemps[2],
									 0x80))
			{
				USP_DBGPRINT(("Unable to encode HW LIMM instruction\n"));
				return IMG_FALSE;
			}

			/* Set the opcode */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_LIMM;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/* temp[3] =  sign bit*/
			if(!HWInstEncodeLIMMInst(psHWInst,
									 1, /* uRepeatCount */
									 &asTemps[3],
									 0x00008000))
			{
				USP_DBGPRINT(("Unable to encode HW LIMM instruction\n"));
				return IMG_FALSE;
			}

			/* Set the opcode */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_LIMM;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/* temp[4] = 0x7f */
			if(!HWInstEncodeLIMMInst(psHWInst,
									 1, /* uRepeatCount */
									 &asTemps[4],
									 0x7f))
			{
				USP_DBGPRINT(("Unable to encode HW LIMM instruction\n"));
				return IMG_FALSE;
			}

			/* Set the opcode */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_LIMM;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/*
			   For each channel we pack from S16 to S8.
			*/
			for(i = 0; i < 4; i++)
			{
				/* Test if the number is too negative and we have to saturate it */

				/* Start by seeing if it is negative */

				/*
				  and p0, temp[1], channel[i], 0x00008000
				*/
				if(!HWInstEncodeTESTInst(psHWInst,
										 0, /* uRepeatCount */
										 EURASIA_USE1_TEST_CRCOMB_AND,
										 &asTemps[1],
										 ppsChannel[i],
										 &asTemps[3],
										 0, /* predicate destination */
										 EURASIA_USE1_TEST_STST_NONE,
										 EURASIA_USE1_TEST_ZTST_NOTZERO,
										 EURASIA_USE1_TEST_CHANCC_SELECT0,
										 EURASIA_USE0_TEST_ALUSEL_BITWISE,
										 EURASIA_USE0_TEST_ALUOP_BITWISE_AND))
				{
					USP_DBGPRINT(("Unable to encode HW TEST instruction\n"));
					return IMG_FALSE;
				}

				/* Set the opcode */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_TEST;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/* If it is negative, check it isn't too negative */
				
				/*
				   p0 : and temp[1], channel[i], 0xffffff80
				*/
				if(!HWInstEncodeANDInst(psHWInst,
										1, /* uRepeatCount */
										IMG_TRUE, /* bUsePred */
										IMG_FALSE, /* bNegatePred */
										0, /* uPredNum */
										IMG_TRUE, /* bSkipInv */
										&asTemps[1],
										ppsChannel[i],
										&sNegMaxMask))
				{
					USP_DBGPRINT(("Unable to encode HW AND instruction\n"));
					return IMG_FALSE;
				}

				/* Set the opcode */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_AND;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/*
				   p0 : xor temp[1], temp[i], 0xffffff80
				*/
				if(!HWInstEncodeXORInst(psHWInst,
										1, /* uRepeatCount */
										EURASIA_USE1_EPRED_P0, /* uPred */
										IMG_TRUE, /* bSkipInv */
										&asTemps[1],
										&asTemps[1],
										&sNegMaxMask))
				{
					USP_DBGPRINT(("Unable to encode HW XOR instruction\n"));
					return IMG_FALSE;
				}

				/* Set the opcode */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_XOR;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/* p0 : temp[1] = temp[1] ?  0x80 : channel[i] */
				if(!HWInstEncodeMOVCInst(psHWInst,
										 1, /* uRepeatCount */
										 EURASIA_USE1_EPRED_P0, /* uPred */
										 IMG_TRUE, /* bTestDst */
										 IMG_TRUE, /* bSkipInv */
										 &asTemps[1],
										 &asTemps[1],
				                         ppsChannel[i],
				                         &asTemps[2]/* special case value */))
				{
					USP_DBGPRINT(("Unable to encode HW MOVC instruction\n"));
					return IMG_FALSE;
				}

				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_VMOV;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/* Test if the number is too positive and we have to saturate it */

				/*
				  !p0 and temp[1], channel[i], 0x7fffff80
				*/
				if(!HWInstEncodeANDInst(psHWInst,
										1, /* uRepeatCount */
										IMG_TRUE, /* bUsePred */
										IMG_TRUE, /* bNegatePred */
										0, /* uPredNum */
										IMG_TRUE, /* bSkipInv */
										&asTemps[1],
										ppsChannel[i],
										&sPosMaxMask))
				{
					USP_DBGPRINT(("Unable to encode HW AND instruction\n"));
					return IMG_FALSE;
				}

				/* Set the opcode */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_XOR;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/* temp[1] = temp[1] : channel[i] ? 0xffffff7f */
				if(!HWInstEncodeMOVCInst(psHWInst,
										 1, /* uRepeatCount */
										 EURASIA_USE1_EPRED_NOTP0, /* uPred */
										 IMG_TRUE, /* bTestDst */
										 IMG_TRUE, /* bSkipInv */
										 &asTemps[1],
										 &asTemps[1],
				                         ppsChannel[i],
										 &asTemps[4] /* special case value */))
				{
					USP_DBGPRINT(("Unable to encode HW MOVC instruction\n"));
					return IMG_FALSE;
				}

				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_VMOV;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				if(!HWInstEncodePCKUNPCKInst(psHWInst,
											 IMG_TRUE, /* bSkipInv */
								  			 USP_PCKUNPCK_FMT_U8,
								  			 USP_PCKUNPCK_FMT_U8,
								  			 IMG_FALSE, /* bScaleEnable */
											 1 << i, /* Write the current component */
							  				 &asTemps[0],
											 0,
											 1,
											 &asTemps[1],
											 &asTemps[1]))
				{
					USP_DBGPRINT(("Unable to encode HW PCKU8U8 instruction\n"));
					return IMG_FALSE;
				}

				/* set description */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];
			}

			if(!HWInstEncodeSTInst(psHWInst,
								   IMG_TRUE, /* bSkipInv */
								   1, /* uRepeatCount */
								   psAddr,
								   &sZero,
								   &asTemps[0]))
			{
				USP_DBGPRINT(("Unable to encode HW ST instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_ST;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Increment HW instruction count */
			psTextureWrite->uInstCount = ++uHWInstCount;

			/* This path used 5 temps */
			*puTempsUsed = 5;
#else
			/* 
			   We need one temp per channel and one for the packed
			   result, so retrun false if we don't have enough.
			*/
			if(uNumTemps < 6)
			{
				USP_DBGPRINT(("Not enough temps to pack from S32 to S8. Have %d but need 6\n", uNumTemps));
				return IMG_FALSE;
			}

			/* Start by loading the saturation value into a temp */
			
			/* temp[4] = 0xff */
			if(!HWInstEncodeLIMMInst(psHWInst,
									 1, /* uRepeatCount */
									 &asTemps[4],
									 0x80))
			{
				USP_DBGPRINT(("Unable to encode HW LIMM instruction\n"));
				return IMG_FALSE;
			}

			/* Set the opcode */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_LIMM;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/* temp[5] =  sign bit*/
			if(!HWInstEncodeLIMMInst(psHWInst,
									 1, /* uRepeatCount */
									 &asTemps[5],
									 0x80000000))
			{
				USP_DBGPRINT(("Unable to encode HW LIMM instruction\n"));
				return IMG_FALSE;
			}

			/* Set the opcode */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_LIMM;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/*
			   For each channel we pack from U32 to U8 and
			   store it in a temp
			*/
			/*
			   For each channel we pack from S32 to S8.
			*/
			for(i = 0; i < 4; i++)
			{
				/* Test if the number is too negative and we have to saturate it */

				/* Start by seeing if it is negative */

				/*
				  and p0, temp[i], channel[i], 0x8000000
				*/
				if(!HWInstEncodeTESTInst(psHWInst,
										 1, /* uRepeatCount */
										 EURASIA_USE1_TEST_CRCOMB_AND,
										 &asTemps[i],
										 ppsChannel[i],
										 &asTemps[5],
										 0, /* predicate destination */
										 EURASIA_USE1_TEST_STST_NONE,
										 EURASIA_USE1_TEST_ZTST_NOTZERO,
										 EURASIA_USE1_TEST_CHANCC_SELECT0,
										 EURASIA_USE0_TEST_ALUSEL_BITWISE,
										 EURASIA_USE0_TEST_ALUOP_BITWISE_AND))
				{
					USP_DBGPRINT(("Unable to encode HW TEST instruction\n"));
					return IMG_FALSE;
				}

				/* Set the opcode */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_TEST;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/* If it is negative, check it isn't too negative */
				
				/*
				   p0 : and temp[i], channel[i], 0xffffff80
				*/
				if(!HWInstEncodeANDInst(psHWInst,
										1, /* uRepeatCount */
										IMG_TRUE, /* bUsePred */
										IMG_FALSE, /* bNegatePred */
										0, /* uPredNum */
										IMG_TRUE, /* bSkipInv */
										&asTemps[i],
										ppsChannel[i],
										&sNegMaxMask))
				{
					USP_DBGPRINT(("Unable to encode HW AND instruction\n"));
					return IMG_FALSE;
				}

				/* Set the opcode */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_AND;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/*
				   p0 : xor temp[i], temp[i], 0xffffff80
				*/
				if(!HWInstEncodeXORInst(psHWInst,
										1, /* uRepeatCount */
										EURASIA_USE1_EPRED_P0, /* uPred */
										IMG_TRUE, /* bSkipInv */
										&asTemps[i],
										&asTemps[i],
										&sNegMaxMask))
				{
					USP_DBGPRINT(("Unable to encode HW XOR instruction\n"));
					return IMG_FALSE;
				}

				/* Set the opcode */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_XOR;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/* p0 : temp[i] = temp[i] ?  0x80 : channel[i] */
				if(!HWInstEncodeMOVCInst(psHWInst,
										 1, /* uRepeatCount */
										 EURASIA_USE1_EPRED_P0, /* uPred */
										 IMG_TRUE, /* bTestDst */
										 IMG_TRUE, /* bSkipInv */
										 &asTemps[i],
										 &asTemps[i],
										 &asTemps[4], /* special case value */
										 ppsChannel[i]))
				{
					USP_DBGPRINT(("Unable to encode HW MOVC instruction\n"));
					return IMG_FALSE;
				}

				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_MOVC;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/* Test if the number is too positive and we have to saturate it */

				/*
				  !p0 and temp[i], channel[i], 0x7fffff80
				*/
				if(!HWInstEncodeANDInst(psHWInst,
										1, /* uRepeatCount */
										IMG_TRUE, /* bUsePred */
										IMG_TRUE, /* bNegatePred */
										0, /* uPredNum */
										IMG_TRUE, /* bSkipInv */
										&asTemps[i],
										ppsChannel[i],
										&sPosMaxMask))
				{
					USP_DBGPRINT(("Unable to encode HW AND instruction\n"));
					return IMG_FALSE;
				}

				/* Set the opcode */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_XOR;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/* temp[i] = temp[i] : channel[i] ? 0xffffff7f */
				if(!HWInstEncodeMOVCInst(psHWInst,
										 1, /* uRepeatCount */
										 EURASIA_USE1_EPRED_NOTP0, /* uPred */
										 IMG_TRUE, /* bTestDst */
										 IMG_TRUE, /* bSkipInv */
										 &asTemps[i],
										 &asTemps[i],
										 &sPosMax, /* special case value */
										 ppsChannel[i]))
				{
					USP_DBGPRINT(("Unable to encode HW MOVC instruction\n"));
					return IMG_FALSE;
				}

				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_MOVC;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];
			}

			/*
			   So at this point the packed data for this channel is in
			   temp0 to temp3 and we need to pack it into the register 
			   we are going to store.
			*/
			if(!HWInstEncodePCKUNPCKInst(psHWInst,
										 IMG_TRUE, /* bSkipInv */
							  			 USP_PCKUNPCK_FMT_U8,
							  			 USP_PCKUNPCK_FMT_U8,
							  			 IMG_FALSE, /* bScaleEnable */
										 0xC, /* Write the top half */
							  			 &asTemps[4],
										 0,
										 1,
										 &asTemps[2],
										 &asTemps[3]))
			{
				USP_DBGPRINT(("Unable to encode HW PCKU8U8 instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/* We have written the top half so now write the bottom half */
			if(!HWInstEncodePCKUNPCKInst(psHWInst,
										 IMG_TRUE, /* bSkipInv */
							  			 USP_PCKUNPCK_FMT_U8,
							  			 USP_PCKUNPCK_FMT_U8,
							  			 IMG_FALSE, /* bScaleEnable */
										 0x3, /* Write the bottom half */
							  			 &asTemps[4],
										 0,
										 1,
										 &asTemps[0],
										 &asTemps[1]))
			{
				USP_DBGPRINT(("Unable to encode HW PCKU8U8 instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
			
			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			if(!HWInstEncodeSTInst(psHWInst,
								   IMG_TRUE, /* bSkipInv */
								   1, /* uRepeatCount */
								   psAddr,
								   &sZero,
								   &asTemps[4]))
			{
				USP_DBGPRINT(("Unable to encode HW ST instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_ST;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Increment HW instruction count */
			psTextureWrite->uInstCount = ++uHWInstCount;

			/* This path used 6 temps */
			*puTempsUsed = 6;
#endif
			break;
		}

		case USP_TEXTURE_FORMAT_RGBA_S16:
		{
			IMG_UINT32	i			= 0;
			USP_REG		sOffset		= {0};

			sOffset.eType 	= USP_REGTYPE_IMM;
			sOffset.uNum  	= 0;
			sOffset.eFmt  	= USP_REGFMT_U32;
			sOffset.uComp 	= 0;
			sOffset.eDynIdx = USP_DYNIDX_NONE;

			if(uNumTemps < 1)
			{
				USP_DBGPRINT(("Not enough temps to pack from S16 to S8. Have %d but need 1\n", uNumTemps));
				return IMG_FALSE;
			}

			for(i = 0; i < 2; i++)
			{
				if(!HWInstEncodePCKUNPCKInst(psHWInst,
							IMG_TRUE, /* bSkipInv */
							USP_PCKUNPCK_FMT_S16,
							USP_PCKUNPCK_FMT_S16,
							IMG_FALSE, /* bScaleEnable */
							0xF,
							&asTemps[0],
							0,
							1,
							ppsChannel[(2*i) + 0],
							ppsChannel[(2*i) + 1]))
				{
					USP_DBGPRINT(("Unable to encode HW PCKU16U16 instruction\n"));
					return IMG_FALSE;
				}

				/* set description */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				sOffset.uNum = i >> 1;

				if(!HWInstEncodeSTInst(psHWInst,
									   IMG_TRUE, /* bSkipInv */
									   1, /* uRepeatCount */
									   psAddr,
									   &sOffset,
									   &asTemps[0]))
				{
					USP_DBGPRINT(("Unable to encode HW ST instruction\n"));
					return IMG_FALSE;
				}

				/* set description */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_ST;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];
			}

			/* Update the instruction count */
			psTextureWrite->uInstCount = uHWInstCount;

			*puTempsUsed = 1;
			break;
		}

		default: USP_DBGPRINT(("Unsupported texture write format %d for write ID %d\n",
							 psContext->asSamplerDesc[psTextureWrite->uWriteID].sTexFormat.eTexFmt,
							 psTextureWrite->uWriteID));
				 break;
	}

	return IMG_TRUE;
}

/******************************************************************************
 Name:		USPTextureWritePackFromU32

 Purpose:	Emit HW instructions to pack to the right image format
 			and store to the given address.

 Inputs:	psTextureWrite	- The texture write to generate code for
			psContext		- The current USP execution context
			psAddr			- The address the store the data to
			asTemps			- An array of temps
			uNumTemps		- The number of temps in the array
			puTempsUsed		- Used to return the number of temps used

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL USPTextureWritePackFromU32(PUSP_TEXTURE_WRITE 	psTextureWrite,
										   PUSP_CONTEXT			psContext,
										   PUSP_REG				psAddr,
										   PUSP_REG				asTemps,
										   IMG_UINT32			uNumTemps,
										   IMG_UINT32*			puTempsUsed)
{
	IMG_UINT32  uHWInstCount	= psTextureWrite->uInstCount;
	IMG_UINT32  uInstDescFlags  = psTextureWrite->uInstDescFlags;
	PHW_INST	psHWInst;

	USP_REG	sZero                               = {0};
	USP_REG	sOne                                = {0};
	USP_REG	sShiftSize                          = {0};
	PUSP_REG ppsChannel[USP_TEXFORM_CHAN_COUNT] = {IMG_NULL};

	sZero.eType 	= USP_REGTYPE_IMM;
	sZero.uNum  	= 0;
	sZero.eFmt  	= USP_REGFMT_U32;
	sZero.uComp 	= 0;
	sZero.eDynIdx 	= USP_DYNIDX_NONE;

	sOne.eType 		= USP_REGTYPE_IMM;
	sOne.uNum  		= 1;
	sOne.eFmt  		= USP_REGFMT_U32;
	sOne.uComp 		= 0;
	sOne.eDynIdx 	= USP_DYNIDX_NONE;
	
	sShiftSize.eType 	= USP_REGTYPE_IMM;
	sShiftSize.uNum  	= 0;
	sShiftSize.eFmt  	= USP_REGFMT_U32;
	sShiftSize.uComp 	= 0;
	sShiftSize.eDynIdx 	= USP_DYNIDX_NONE;

	/* Get the HW Instruction */
	psHWInst = &psTextureWrite->asInsts[uHWInstCount];

	/* Swizzle the source registers appropriately */
	SwizzleChannels(ppsChannel,
	                psTextureWrite->asChannel,
	                psContext->asSamplerDesc[psTextureWrite->uWriteID].sTexFormat.aeChanSwizzle);

	switch(psContext->asSamplerDesc[psTextureWrite->uWriteID].sTexFormat.eTexFmt)
	{
		case USP_TEXTURE_FORMAT_R8G8B8A8:
	    case USP_TEXTURE_FORMAT_RGBA_U8:
		{
			IMG_UINT32 	i				= 0;

			/* Set the right-shift size */
			sShiftSize.uNum = 8;

#if defined(SGX_FEATURE_USE_VEC34)
			/* 
			   We need one temp for the result, one for the saturated value and
			   one for the channel data, so retrun false if we don't have enough.
			*/
			if(uNumTemps < 3)
			{
				USP_DBGPRINT(("Not enough temps to pack from U32 to U8. Have %d but need 3\n", uNumTemps));
				return IMG_FALSE;
			}

			/* Start by loading the saturation value into a temp */
			
			/* temp[2] = 0xff */
			if(!HWInstEncodeLIMMInst(psHWInst,
									 1, /* uRepeatCount */
									 &asTemps[2],
									 0xff))
			{
				USP_DBGPRINT(("Unable to encode HW LIMM instruction\n"));
				return IMG_FALSE;
			}

			/* Set the opcode */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_LIMM;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/*
			   On vector cores we do the pack inside the for-loop because we 
			   need 4 packs instead of the two we need on non-vector cores 
			*/
			for(i = 0; i < 4; i++)
			{
				/*
				  shr temp[1], channel[i], 8
				*/
				if(!HWInstEncodeSHRInst(psHWInst,
										1, /* uRepeatCount */
										IMG_TRUE, /* bSkipInv */
										&asTemps[1],
										ppsChannel[i],
										&sShiftSize))
				{
					USP_DBGPRINT(("Unable to encode HW SHR instruction\n"));
					return IMG_FALSE;
				}

				/* Set the opcode */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_SHR;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/* temp1 = temp1 : 0xff ? channel[i] */
				if(!HWInstEncodeMOVCInst(psHWInst,
										 1, /* uRepeatCount */
										 EURASIA_USE1_EPRED_ALWAYS, /* uPred */
										 IMG_TRUE, /* bTestDst */
										 IMG_TRUE, /* bSkipInv */
										 &asTemps[1],
										 &asTemps[1],
				                         ppsChannel[i], /* saturation max */
				                         &asTemps[2]))
				{
					USP_DBGPRINT(("Unable to encode HW MOVC instruction\n"));
					return IMG_FALSE;
				}

				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_VMOV;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				if(!HWInstEncodePCKUNPCKInst(psHWInst,
											 IMG_TRUE, /* bSkipInv */
								  			 USP_PCKUNPCK_FMT_U8,
								  			 USP_PCKUNPCK_FMT_U8,
								  			 IMG_FALSE, /* bScaleEnable */
											 1 << i, /* Write the current component */
							  				 &asTemps[0],
											 0,
											 1,
											 &asTemps[1],
											 &asTemps[1]))
				{
					USP_DBGPRINT(("Unable to encode HW PCKU8U8 instruction\n"));
					return IMG_FALSE;
				}

				/* set description */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];
			}

			if(!HWInstEncodeSTInst(psHWInst,
								   IMG_TRUE, /* bSkipInv */
								   1, /* uRepeatCount */
								   psAddr,
								   &sZero,
								   &asTemps[0]))
			{
				USP_DBGPRINT(("Unable to encode HW ST instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_ST;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Increment HW instruction count */
			psTextureWrite->uInstCount = ++uHWInstCount;

			/* This path used 5 temps (one for each channel and one to pack the result into) */
			*puTempsUsed = 3;
#else
			/* 
			   We need one temp per channel and one for the packed
			   result, so retrun false if we don't have enough.
			*/
			if(uNumTemps < 5)
			{
				USP_DBGPRINT(("Not enough temps to pack from U32 to U8. Have %d but need 5\n", uNumTemps));
				return IMG_FALSE;
			}

			/* Start by loading the saturation value into a temp */
			
			/* temp[4] = 0xff */
			if(!HWInstEncodeLIMMInst(psHWInst,
									 1, /* uRepeatCount */
									 &asTemps[4],
									 0xff))
			{
				USP_DBGPRINT(("Unable to encode HW LIMM instruction\n"));
				return IMG_FALSE;
			}

			/* Set the opcode */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_LIMM;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/*
			   For each channel we pack from U32 to U8 and
			   store it in a temp
			*/
			for(i = 0; i < 4; i++)
			{
				/*
				  shr temp[i], channel[i], 8
				*/
				if(!HWInstEncodeSHRInst(psHWInst,
										1, /* uRepeatCount */
										IMG_TRUE, /* bSkipInv */
										&asTemps[i],
										ppsChannel[i],
										&sShiftSize))
				{
					USP_DBGPRINT(("Unable to encode HW SHR instruction\n"));
					return IMG_FALSE;
				}

				/* Set the opcode */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_SHR;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/* temp1 = temp1 : 0xff ? channel[i] */
				if(!HWInstEncodeMOVCInst(psHWInst,
										 1, /* uRepeatCount */
										 EURASIA_USE1_EPRED_ALWAYS, /* uPred */
										 IMG_TRUE, /* bTestDst */
										 IMG_TRUE, /* bSkipInv */
										 &asTemps[i],
										 &asTemps[i],
										 &asTemps[4], /* saturation max */
										 ppsChannel[i]))
				{
					USP_DBGPRINT(("Unable to encode HW MOVC instruction\n"));
					return IMG_FALSE;
				}

				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_MOVC;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];
			}
		

			/*
			   So at this point the packed data for this channel is in
			   temp0 to temp3 and we need to pack it into the register 
			   we are going to store.
			*/
			if(!HWInstEncodePCKUNPCKInst(psHWInst,
										 IMG_TRUE, /* bSkipInv */
							  			 USP_PCKUNPCK_FMT_U8,
							  			 USP_PCKUNPCK_FMT_U8,
							  			 IMG_FALSE, /* bScaleEnable */
										 0xC, /* Write the top half */
							  			 &asTemps[4],
										 0,
										 1,
										 &asTemps[2],
										 &asTemps[3]))
			{
				USP_DBGPRINT(("Unable to encode HW PCKU8U8 instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/* We have written the top half so now write the bottom half */
			if(!HWInstEncodePCKUNPCKInst(psHWInst,
										 IMG_TRUE, /* bSkipInv */
							  			 USP_PCKUNPCK_FMT_U8,
							  			 USP_PCKUNPCK_FMT_U8,
							  			 IMG_FALSE, /* bScaleEnable */
										 0x3, /* Write the bottom half */
							  			 &asTemps[4],
										 0,
										 1,
										 &asTemps[0],
										 &asTemps[1]))
			{
				USP_DBGPRINT(("Unable to encode HW PCKU8U8 instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			if(!HWInstEncodeSTInst(psHWInst,
								   IMG_TRUE, /* bSkipInv */
								   1, /* uRepeatCount */
								   psAddr,
								   &sZero,
								   &asTemps[4]))
			{
				USP_DBGPRINT(("Unable to encode HW ST instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_ST;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Increment HW instruction count */
			psTextureWrite->uInstCount = ++uHWInstCount;

			/* This path used 5 temps (one for each channel and one to pack the result into) */
			*puTempsUsed = 5;
#endif /* defined(SGX_FEATURE_USE_VEC34) */
			
			break;
		}

		case USP_TEXTURE_FORMAT_RGBA_U16:
		{
			IMG_UINT32 i = 0;
			IMG_UINT32 j = 0;

			PUSP_REG psOffset = IMG_NULL;

			/* Set the right-shift size */
			sShiftSize.uNum = 16;

#if defined(SGX_FEATURE_USE_VEC34)
			/* 
			   We need one temp for the result, one for the saturated value and
			   one for the channel data, so retrun false if we don't have enough.
			*/
			if(uNumTemps < 3)
			{
				USP_DBGPRINT(("Not enough temps to pack from U32 to U16. Have %d but need 3\n", uNumTemps));
				return IMG_FALSE;
			}

			/* Start by loading the saturation value into a temp */
			
			/* temp2 = 0xffff */
			if(!HWInstEncodeLIMMInst(psHWInst,
									 1, /* uRepeatCount */
									 &asTemps[2],
									 0xffff))
			{
				USP_DBGPRINT(("Unable to encode HW LIMM instruction\n"));
				return IMG_FALSE;
			}

			/* Set the opcode */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_LIMM;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			for(i = 0; i < 2; i++)
			{
				for(j = 0; j < 2; j++)
				{
					/*
					  shr temp[1], channel[i], 16
					*/
					if(!HWInstEncodeSHRInst(psHWInst,
											1, /* uRepeatCount */
											IMG_TRUE, /* bSkipInv */
											&asTemps[1],
											ppsChannel[(i * 2) + j],
											&sShiftSize))
					{
						USP_DBGPRINT(("Unable to encode HW SHR instruction\n"));
						return IMG_FALSE;
					}

					/* Set the opcode */
					psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_SHR;
					psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
					/* Get the next HW Instruction */
					psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

					/* temp[1] = temp[1] : 0xffff ? channel[i] */
					if(!HWInstEncodeMOVCInst(psHWInst,
											 1, /* uRepeatCount */
											 EURASIA_USE1_EPRED_ALWAYS, /* uPred */
											 IMG_TRUE, /* bTestDst */
											 IMG_TRUE, /* bSkipInv */
											 &asTemps[1],
											 &asTemps[1],
					                         ppsChannel[(i * 2) + j],
											 &asTemps[2] /* saturation max */))
					{
						USP_DBGPRINT(("Unable to encode HW MOVC instruction\n"));
						return IMG_FALSE;
					}

					psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_VMOV;
					psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

					/* Get the next HW Instruction */
					psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

					if(!HWInstEncodePCKUNPCKInst(psHWInst,
												 IMG_TRUE, /* bSkipInv */
									  			 USP_PCKUNPCK_FMT_U16,
									  			 USP_PCKUNPCK_FMT_U16,
									  			 IMG_FALSE, /* bScaleEnable */
					                             0x1 << j,
									  			 &asTemps[0],
												 0,
												 1,
												 &asTemps[1],
												 &asTemps[1]))
					{
						USP_DBGPRINT(("Unable to encode HW PCKU16U16 instruction\n"));
						return IMG_FALSE;
					}

					/* set description */
					psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
					psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

					/* Get the next HW Instruction */
					psHWInst = &psTextureWrite->asInsts[++uHWInstCount];
				}

				/* Set the offset for the store instruction */
				if(i == 0)
					psOffset = &sZero;
				else
					psOffset = &sOne;

				if(!HWInstEncodeSTInst(psHWInst,
									   IMG_TRUE, /* bSkipInv */
									   1, /* uRepeatCount */
									   psAddr,
									   psOffset,
									   &asTemps[0]))
				{
					USP_DBGPRINT(("Unable to encode HW ST instruction\n"));
					return IMG_FALSE;
				}

				/* set description */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_ST;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];
			}

			psTextureWrite->uInstCount = uHWInstCount;
		
			/* 
			   This path used 3 temps. One for channel channel data, one for the 
			   max value and for the data we store 
			*/
			*puTempsUsed = 3;
#else
			PVR_UNREFERENCED_PARAMETER(j);
			PVR_UNREFERENCED_PARAMETER(psOffset);

			/* 
			   We need one temp per channel and two for the packed
			   results, so retrun false if we don't have enough.
			*/
			if(uNumTemps < 6)
			{
				USP_DBGPRINT(("Not enough temps to pack from U32 to U16. Have %d but need 6\n", uNumTemps));
				return IMG_FALSE;
			}

			/* Start by loading the saturation value into a temp */
			
			/* temp4 = 0xffff */
			if(!HWInstEncodeLIMMInst(psHWInst,
									 1, /* uRepeatCount */
									 &asTemps[4],
									 0xffff))
			{
				USP_DBGPRINT(("Unable to encode HW LIMM instruction\n"));
				return IMG_FALSE;
			}

			/* Set the opcode */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_LIMM;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/*
			   For each channel we pack from U32 to U16.
			*/
			for(i = 0; i < 4; i++)
			{
				/*
				  shr temp[i], channel[i], 16
				*/
				if(!HWInstEncodeSHRInst(psHWInst,
										1, /* uRepeatCount */
										IMG_TRUE, /* bSkipInv */
										&asTemps[i],
										ppsChannel[i],
										&sShiftSize))
				{
					USP_DBGPRINT(("Unable to encode HW SHR instruction\n"));
					return IMG_FALSE;
				}

				/* Set the opcode */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_SHR;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/* temp[i] = temp[i] : 0xffff ? channel[i] */
				if(!HWInstEncodeMOVCInst(psHWInst,
										 1, /* uRepeatCount */
										 EURASIA_USE1_EPRED_ALWAYS, /* uPred */
										 IMG_TRUE, /* bTestDst */
										 IMG_TRUE, /* bSkipInv */
										 &asTemps[i],
										 &asTemps[i],
										 &asTemps[4], /* saturation max */
										 ppsChannel[i]))
				{
					USP_DBGPRINT(("Unable to encode HW MOVC instruction\n"));
					return IMG_FALSE;
				}

				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_MOVC;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];
			}
		
			/*
			   So at this point the packed data for this channel is in
			   temp0 to temp3 and we need to pack it into the two
			   registers we are going to store.
			*/
			if(!HWInstEncodePCKUNPCKInst(psHWInst,
										 IMG_TRUE, /* bSkipInv */
							  			 USP_PCKUNPCK_FMT_U16,
							  			 USP_PCKUNPCK_FMT_U16,
							  			 IMG_FALSE, /* bScaleEnable */
										 0xF,
							  			 &asTemps[4],
										 0,
										 1,
										 &asTemps[0],
										 &asTemps[1]))
			{
				USP_DBGPRINT(("Unable to encode HW PCKU16U16 instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			if(!HWInstEncodePCKUNPCKInst(psHWInst,
										 IMG_TRUE, /* bSkipInv */
							  			 USP_PCKUNPCK_FMT_U16,
							  			 USP_PCKUNPCK_FMT_U16,
							  			 IMG_FALSE, /* bScaleEnable */
										 0xF,
							  			 &asTemps[5],
										 0,
										 1,
										 &asTemps[2],
										 &asTemps[3]))
			{
				USP_DBGPRINT(("Unable to encode HW PCKU16U16 instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/*
			   FIXME: We should be able to use a fetch/repeat here
			*/
			if(!HWInstEncodeSTInst(psHWInst,
								   IMG_TRUE, /* bSkipInv */
								   1, /* uRepeatCount */
								   psAddr,
								   &sZero,
								   &asTemps[4]))
			{
				USP_DBGPRINT(("Unable to encode HW ST instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_ST;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];
			
			if(!HWInstEncodeSTInst(psHWInst,
								   IMG_TRUE, /* bSkipInv */
								   1, /* uRepeatCount */
								   psAddr,
								   &sOne,
								   &asTemps[5]))
			{
				USP_DBGPRINT(("Unable to encode HW ST instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_ST;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Increment HW instruction count */
			psTextureWrite->uInstCount = ++uHWInstCount;

			/* This path used 6 temps. One for each channel and two for the data we store */
			*puTempsUsed = 6;
#endif /* defined(SGX_FEATURE_USE_VEC34) */

			break;
		}

		case USP_TEXTURE_FORMAT_RGBA_U32:
		{
			IMG_UINT32	i			= 0;
			USP_REG		sOffset		= {0};

			sOffset.eType 	= USP_REGTYPE_IMM;
			sOffset.uNum  	= 0;
			sOffset.eFmt  	= USP_REGFMT_U32;
			sOffset.uComp 	= 0;
			sOffset.eDynIdx = USP_DYNIDX_NONE;

			for(i = 0; i < 4; i++)
			{
				sOffset.uNum = i;

				if(!HWInstEncodeSTInst(psHWInst,
									   IMG_TRUE, /* bSkipInv */
									   1, /* uRepeatCount */
									   psAddr,
									   &sOffset,
									   ppsChannel[i]))
				{
					USP_DBGPRINT(("Unable to encode HW ST instruction\n"));
					return IMG_FALSE;
				}

				/* set description */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_ST;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];
			}

			/* Update the instruction count */
			psTextureWrite->uInstCount = uHWInstCount;

			*puTempsUsed = 0;
			break;
		}

		default: USP_DBGPRINT(("Unsupported texture write format %d for write ID %d\n",
							 psContext->asSamplerDesc[psTextureWrite->uWriteID].sTexFormat.eTexFmt,
							 psTextureWrite->uWriteID));
				 break;
	}

	return IMG_TRUE;
}

/******************************************************************************
 Name:		USPTextureWritePackFromI32

 Purpose:	Emit HW instructions to pack to the right image format
 			and store to the given address.

 Inputs:	psTextureWrite	- The texture write to generate code for
			psContext		- The current USP execution context
			psAddr			- The address the store the data to
			asTemps			- An array of temps
			uNumTemps		- The number of temps in the array
			puTempsUsed		- Used to return the number of temps used

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL USPTextureWritePackFromI32(PUSP_TEXTURE_WRITE 	psTextureWrite,
										   PUSP_CONTEXT			psContext,
										   PUSP_REG				psAddr,
										   PUSP_REG				asTemps,
										   IMG_UINT32			uNumTemps,
										   IMG_UINT32*			puTempsUsed)
{
	IMG_UINT32 uHWInstCount   = psTextureWrite->uInstCount;
	IMG_UINT32 uInstDescFlags = psTextureWrite->uInstDescFlags;
	PHW_INST   psHWInst;

	USP_REG	sZero                               = {0};
	USP_REG	sOne                                = {0};
	USP_REG	sShiftSize                          = {0};
	PUSP_REG ppsChannel[USP_TEXFORM_CHAN_COUNT] = {IMG_NULL};

	sZero.eType 	= USP_REGTYPE_IMM;
	sZero.uNum  	= 0;
	sZero.eFmt  	= USP_REGFMT_U32;
	sZero.uComp 	= 0;
	sZero.eDynIdx 	= USP_DYNIDX_NONE;

	sOne.eType 		= USP_REGTYPE_IMM;
	sOne.uNum  		= 1;
	sOne.eFmt  		= USP_REGFMT_U32;
	sOne.uComp 		= 0;
	sOne.eDynIdx 	= USP_DYNIDX_NONE;
	
	sShiftSize.eType 	= USP_REGTYPE_IMM;
	sShiftSize.uNum  	= 0;
	sShiftSize.eFmt  	= USP_REGFMT_U32;
	sShiftSize.uComp 	= 0;
	sShiftSize.eDynIdx 	= USP_DYNIDX_NONE;

	/* Get the HW Instruction */
	psHWInst = &psTextureWrite->asInsts[uHWInstCount];

	/* Swizzle the source registers appropriately */
	SwizzleChannels(ppsChannel,
	                psTextureWrite->asChannel,
	                psContext->asSamplerDesc[psTextureWrite->uWriteID].sTexFormat.aeChanSwizzle);
	
	switch(psContext->asSamplerDesc[psTextureWrite->uWriteID].sTexFormat.eTexFmt)
	{
		case USP_TEXTURE_FORMAT_R8G8B8A8:
	    case USP_TEXTURE_FORMAT_RGBA_S8:
		{
			USP_REG		sPosMax			= {0};
			USP_REG		sPosMaxMask		= {0};
			USP_REG		sNegMaxMask		= {0};

			IMG_UINT32 	i				= 0;

			sPosMax.eType 	= USP_REGTYPE_IMM;
			sPosMax.uNum  	= 0x7f;
			sPosMax.eFmt  	= USP_REGFMT_U32;
			sPosMax.uComp 	= 0;
			sPosMax.eDynIdx = USP_DYNIDX_NONE;

			sPosMaxMask.eType 	= USP_REGTYPE_IMM;
			sPosMaxMask.uNum  	= 0x7fffff80;
			sPosMaxMask.eFmt  	= USP_REGFMT_U32;
			sPosMaxMask.uComp 	= 0;
			sPosMaxMask.eDynIdx = USP_DYNIDX_NONE;

			sNegMaxMask.eType 	= USP_REGTYPE_IMM;
			sNegMaxMask.uNum  	= 0xffffff80;
			sNegMaxMask.eFmt  	= USP_REGFMT_U32;
			sNegMaxMask.uComp 	= 0;
			sNegMaxMask.eDynIdx = USP_DYNIDX_NONE;

			/* Set the right-shift size */
			sShiftSize.uNum = 8;

#if defined(SGX_FEATURE_USE_VEC34)
			/* 
			   We need one temp for the result, one for the saturated value and
			   one for the channel data, so retrun false if we don't have enough.
			*/
			if(uNumTemps < 5)
			{
				USP_DBGPRINT(("Not enough temps to pack from S32 to S8. Have %d but need 5\n", uNumTemps));
				return IMG_FALSE;
			}

			/* Start by loading the saturation value into a temp */
			
			/* temp[2] = 0x80 */
			if(!HWInstEncodeLIMMInst(psHWInst,
									 1, /* uRepeatCount */
									 &asTemps[2],
									 0x80))
			{
				USP_DBGPRINT(("Unable to encode HW LIMM instruction\n"));
				return IMG_FALSE;
			}

			/* Set the opcode */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_LIMM;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/* temp[3] =  sign bit*/
			if(!HWInstEncodeLIMMInst(psHWInst,
									 1, /* uRepeatCount */
									 &asTemps[3],
									 0x80000000))
			{
				USP_DBGPRINT(("Unable to encode HW LIMM instruction\n"));
				return IMG_FALSE;
			}

			/* Set the opcode */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_LIMM;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/* temp[4] = 0x7f */
			if(!HWInstEncodeLIMMInst(psHWInst,
									 1, /* uRepeatCount */
									 &asTemps[4],
									 0x7f))
			{
				USP_DBGPRINT(("Unable to encode HW LIMM instruction\n"));
				return IMG_FALSE;
			}

			/* Set the opcode */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_LIMM;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/*
			   For each channel we pack from S32 to S8.
			*/
			for(i = 0; i < 4; i++)
			{
				/* Test if the number is too negative and we have to saturate it */

				/* Start by seeing if it is negative */

				/*
				  and p0, temp[1], channel[i], 0x8000000
				*/
				if(!HWInstEncodeTESTInst(psHWInst,
										 0, /* uRepeatCount */
										 EURASIA_USE1_TEST_CRCOMB_AND,
										 &asTemps[1],
										 ppsChannel[i],
										 &asTemps[3],
										 0, /* predicate destination */
										 EURASIA_USE1_TEST_STST_NONE,
										 EURASIA_USE1_TEST_ZTST_NOTZERO,
										 EURASIA_USE1_TEST_CHANCC_SELECT0,
										 EURASIA_USE0_TEST_ALUSEL_BITWISE,
										 EURASIA_USE0_TEST_ALUOP_BITWISE_AND))
				{
					USP_DBGPRINT(("Unable to encode HW TEST instruction\n"));
					return IMG_FALSE;
				}

				/* Set the opcode */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_TEST;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/* If it is negative, check it isn't too negative */
				
				/*
				   p0 : and temp[1], channel[i], 0xffffff80
				*/
				if(!HWInstEncodeANDInst(psHWInst,
										1, /* uRepeatCount */
										IMG_TRUE, /* bUsePred */
										IMG_FALSE, /* bNegatePred */
										0, /* uPredNum */
										IMG_TRUE, /* bSkipInv */
										&asTemps[1],
										ppsChannel[i],
										&sNegMaxMask))
				{
					USP_DBGPRINT(("Unable to encode HW AND instruction\n"));
					return IMG_FALSE;
				}

				/* Set the opcode */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_AND;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/*
				   p0 : xor temp[1], temp[i], 0xffffff80
				*/
				if(!HWInstEncodeXORInst(psHWInst,
										1, /* uRepeatCount */
										EURASIA_USE1_EPRED_P0, /* uPred */
										IMG_TRUE, /* bSkipInv */
										&asTemps[1],
										&asTemps[1],
										&sNegMaxMask))
				{
					USP_DBGPRINT(("Unable to encode HW XOR instruction\n"));
					return IMG_FALSE;
				}

				/* Set the opcode */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_XOR;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/* p0 : temp[1] = temp[1] ?  0x80 : channel[i] */
				if(!HWInstEncodeMOVCInst(psHWInst,
										 1, /* uRepeatCount */
										 EURASIA_USE1_EPRED_P0, /* uPred */
										 IMG_TRUE, /* bTestDst */
										 IMG_TRUE, /* bSkipInv */
										 &asTemps[1],
										 &asTemps[1],
				                         ppsChannel[i],
				                         &asTemps[2]/* special case value */))
				{
					USP_DBGPRINT(("Unable to encode HW MOVC instruction\n"));
					return IMG_FALSE;
				}

				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_VMOV;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/* Test if the number is too positive and we have to saturate it */

				/*
				  !p0 and temp[1], channel[i], 0x7fffff80
				*/
				if(!HWInstEncodeANDInst(psHWInst,
										1, /* uRepeatCount */
										IMG_TRUE, /* bUsePred */
										IMG_TRUE, /* bNegatePred */
										0, /* uPredNum */
										IMG_TRUE, /* bSkipInv */
										&asTemps[1],
										ppsChannel[i],
										&sPosMaxMask))
				{
					USP_DBGPRINT(("Unable to encode HW AND instruction\n"));
					return IMG_FALSE;
				}

				/* Set the opcode */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_XOR;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/* temp[1] = temp[1] : channel[i] ? 0xffffff7f */
				if(!HWInstEncodeMOVCInst(psHWInst,
										 1, /* uRepeatCount */
										 EURASIA_USE1_EPRED_NOTP0, /* uPred */
										 IMG_TRUE, /* bTestDst */
										 IMG_TRUE, /* bSkipInv */
										 &asTemps[1],
										 &asTemps[1],
				                         ppsChannel[i],
										 &asTemps[4] /* special case value */))
				{
					USP_DBGPRINT(("Unable to encode HW MOVC instruction\n"));
					return IMG_FALSE;
				}

				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_VMOV;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				if(!HWInstEncodePCKUNPCKInst(psHWInst,
											 IMG_TRUE, /* bSkipInv */
								  			 USP_PCKUNPCK_FMT_U8,
								  			 USP_PCKUNPCK_FMT_U8,
								  			 IMG_FALSE, /* bScaleEnable */
											 1 << i, /* Write the current component */
							  				 &asTemps[0],
											 0,
											 1,
											 &asTemps[1],
											 &asTemps[1]))
				{
					USP_DBGPRINT(("Unable to encode HW PCKU8U8 instruction\n"));
					return IMG_FALSE;
				}

				/* set description */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];
			}

			if(!HWInstEncodeSTInst(psHWInst,
								   IMG_TRUE, /* bSkipInv */
								   1, /* uRepeatCount */
								   psAddr,
								   &sZero,
								   &asTemps[0]))
			{
				USP_DBGPRINT(("Unable to encode HW ST instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_ST;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Increment HW instruction count */
			psTextureWrite->uInstCount = ++uHWInstCount;

			/* This path used 5 temps */
			*puTempsUsed = 5;
#else
			/* 
			   We need one temp per channel and one for the packed
			   result, so retrun false if we don't have enough.
			*/
			if(uNumTemps < 6)
			{
				USP_DBGPRINT(("Not enough temps to pack from S32 to S8. Have %d but need 6\n", uNumTemps));
				return IMG_FALSE;
			}

			/* Start by loading the saturation value into a temp */
			
			/* temp[4] = 0xff */
			if(!HWInstEncodeLIMMInst(psHWInst,
									 1, /* uRepeatCount */
									 &asTemps[4],
									 0x80))
			{
				USP_DBGPRINT(("Unable to encode HW LIMM instruction\n"));
				return IMG_FALSE;
			}

			/* Set the opcode */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_LIMM;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/* temp[5] =  sign bit*/
			if(!HWInstEncodeLIMMInst(psHWInst,
									 1, /* uRepeatCount */
									 &asTemps[5],
									 0x80000000))
			{
				USP_DBGPRINT(("Unable to encode HW LIMM instruction\n"));
				return IMG_FALSE;
			}

			/* Set the opcode */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_LIMM;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/*
			   For each channel we pack from U32 to U8 and
			   store it in a temp
			*/
			/*
			   For each channel we pack from S32 to S8.
			*/
			for(i = 0; i < 4; i++)
			{
				/* Test if the number is too negative and we have to saturate it */

				/* Start by seeing if it is negative */

				/*
				  and p0, temp[i], channel[i], 0x8000000
				*/
				if(!HWInstEncodeTESTInst(psHWInst,
										 1, /* uRepeatCount */
										 EURASIA_USE1_TEST_CRCOMB_AND,
										 &asTemps[i],
										 ppsChannel[i],
										 &asTemps[5],
										 0, /* predicate destination */
										 EURASIA_USE1_TEST_STST_NONE,
										 EURASIA_USE1_TEST_ZTST_NOTZERO,
										 EURASIA_USE1_TEST_CHANCC_SELECT0,
										 EURASIA_USE0_TEST_ALUSEL_BITWISE,
										 EURASIA_USE0_TEST_ALUOP_BITWISE_AND))
				{
					USP_DBGPRINT(("Unable to encode HW TEST instruction\n"));
					return IMG_FALSE;
				}

				/* Set the opcode */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_TEST;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/* If it is negative, check it isn't too negative */
				
				/*
				   p0 : and temp[i], channel[i], 0xffffff80
				*/
				if(!HWInstEncodeANDInst(psHWInst,
										1, /* uRepeatCount */
										IMG_TRUE, /* bUsePred */
										IMG_FALSE, /* bNegatePred */
										0, /* uPredNum */
										IMG_TRUE, /* bSkipInv */
										&asTemps[i],
										ppsChannel[i],
										&sNegMaxMask))
				{
					USP_DBGPRINT(("Unable to encode HW AND instruction\n"));
					return IMG_FALSE;
				}

				/* Set the opcode */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_AND;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/*
				   p0 : xor temp[i], temp[i], 0xffffff80
				*/
				if(!HWInstEncodeXORInst(psHWInst,
										1, /* uRepeatCount */
										EURASIA_USE1_EPRED_P0, /* uPred */
										IMG_TRUE, /* bSkipInv */
										&asTemps[i],
										&asTemps[i],
										&sNegMaxMask))
				{
					USP_DBGPRINT(("Unable to encode HW XOR instruction\n"));
					return IMG_FALSE;
				}

				/* Set the opcode */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_XOR;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/* p0 : temp[i] = temp[i] ?  0x80 : channel[i] */
				if(!HWInstEncodeMOVCInst(psHWInst,
										 1, /* uRepeatCount */
										 EURASIA_USE1_EPRED_P0, /* uPred */
										 IMG_TRUE, /* bTestDst */
										 IMG_TRUE, /* bSkipInv */
										 &asTemps[i],
										 &asTemps[i],
										 &asTemps[4], /* special case value */
										 ppsChannel[i]))
				{
					USP_DBGPRINT(("Unable to encode HW MOVC instruction\n"));
					return IMG_FALSE;
				}

				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_MOVC;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/* Test if the number is too positive and we have to saturate it */

				/*
				  !p0 and temp[i], channel[i], 0x7fffff80
				*/
				if(!HWInstEncodeANDInst(psHWInst,
										1, /* uRepeatCount */
										IMG_TRUE, /* bUsePred */
										IMG_TRUE, /* bNegatePred */
										0, /* uPredNum */
										IMG_TRUE, /* bSkipInv */
										&asTemps[i],
										ppsChannel[i],
										&sPosMaxMask))
				{
					USP_DBGPRINT(("Unable to encode HW AND instruction\n"));
					return IMG_FALSE;
				}

				/* Set the opcode */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_XOR;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/* temp[i] = temp[i] : channel[i] ? 0xffffff7f */
				if(!HWInstEncodeMOVCInst(psHWInst,
										 1, /* uRepeatCount */
										 EURASIA_USE1_EPRED_NOTP0, /* uPred */
										 IMG_TRUE, /* bTestDst */
										 IMG_TRUE, /* bSkipInv */
										 &asTemps[i],
										 &asTemps[i],
										 &sPosMax, /* special case value */
										 ppsChannel[i]))
				{
					USP_DBGPRINT(("Unable to encode HW MOVC instruction\n"));
					return IMG_FALSE;
				}

				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_MOVC;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];
			}

			/*
			   So at this point the packed data for this channel is in
			   temp0 to temp3 and we need to pack it into the register 
			   we are going to store.
			*/
			if(!HWInstEncodePCKUNPCKInst(psHWInst,
										 IMG_TRUE, /* bSkipInv */
							  			 USP_PCKUNPCK_FMT_U8,
							  			 USP_PCKUNPCK_FMT_U8,
							  			 IMG_FALSE, /* bScaleEnable */
										 0xC, /* Write the top half */
							  			 &asTemps[4],
										 0,
										 1,
										 &asTemps[2],
										 &asTemps[3]))
			{
				USP_DBGPRINT(("Unable to encode HW PCKU8U8 instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/* We have written the top half so now write the bottom half */
			if(!HWInstEncodePCKUNPCKInst(psHWInst,
										 IMG_TRUE, /* bSkipInv */
							  			 USP_PCKUNPCK_FMT_U8,
							  			 USP_PCKUNPCK_FMT_U8,
							  			 IMG_FALSE, /* bScaleEnable */
										 0x3, /* Write the bottom half */
							  			 &asTemps[4],
										 0,
										 1,
										 &asTemps[0],
										 &asTemps[1]))
			{
				USP_DBGPRINT(("Unable to encode HW PCKU8U8 instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
			
			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			if(!HWInstEncodeSTInst(psHWInst,
								   IMG_TRUE, /* bSkipInv */
								   1, /* uRepeatCount */
								   psAddr,
								   &sZero,
								   &asTemps[4]))
			{
				USP_DBGPRINT(("Unable to encode HW ST instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_ST;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Increment HW instruction count */
			psTextureWrite->uInstCount = ++uHWInstCount;

			/* This path used 6 temps */
			*puTempsUsed = 6;
#endif
			break;
		}

		case USP_TEXTURE_FORMAT_RGBA_S16:
		{
			IMG_UINT32 	i				= 0;
			IMG_UINT32 	j				= 0;

			USP_REG		sPosMaxMask		= {0};
			USP_REG		sNegMaxMask		= {0};
			PUSP_REG	psOffset		= IMG_NULL;

			sPosMaxMask.eType 	= USP_REGTYPE_IMM;
			sPosMaxMask.uNum  	= 0x7fff8000;
			sPosMaxMask.eFmt  	= USP_REGFMT_U32;
			sPosMaxMask.uComp 	= 0;
			sPosMaxMask.eDynIdx = USP_DYNIDX_NONE;

			sNegMaxMask.eType 	= USP_REGTYPE_IMM;
			sNegMaxMask.uNum  	= 0xffff8000;
			sNegMaxMask.eFmt  	= USP_REGFMT_U32;
			sNegMaxMask.uComp 	= 0;
			sNegMaxMask.eDynIdx = USP_DYNIDX_NONE;
		
			/* Set the right-shift size */
			sShiftSize.uNum = 16;
#if defined(SGX_FEATURE_USE_VEC34)

			if(uNumTemps < 5)
			{
				USP_DBGPRINT(("Not enough temps to pack from I32 to I16. Have %d but need 5\n", uNumTemps));
				return IMG_FALSE;
			}

			/* temp2 = 0x8000 */
			if(!HWInstEncodeLIMMInst(psHWInst,
									 1, /* uRepeatCount */
									 &asTemps[2],
									 0x8000))
			{
				USP_DBGPRINT(("Unable to encode HW LIMM instruction\n"));
				return IMG_FALSE;
			}

			/* Set the opcode */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_LIMM;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/* temp3 = 0x7fff */
			if(!HWInstEncodeLIMMInst(psHWInst,
									 1, /* uRepeatCount */
									 &asTemps[3],
									 0x7fff))
			{
				USP_DBGPRINT(("Unable to encode HW LIMM instruction\n"));
				return IMG_FALSE;
			}

			/* Set the opcode */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_LIMM;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/* temp4 =  sign bit*/
			if(!HWInstEncodeLIMMInst(psHWInst,
									 1, /* uRepeatCount */
									 &asTemps[4],
									 0x80000000))
			{
				USP_DBGPRINT(("Unable to encode HW LIMM instruction\n"));
				return IMG_FALSE;
			}

			/* Set the opcode */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_LIMM;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/*
			   For each channel we pack from S32 to S16.
			*/
			for(i = 0; i < 2; i++)
			{
				for(j = 0; j < 2; j++)
				{
					/* Test if the number is too negative and we have to saturate it */

					/* Start by seeing if it is negative */

					/*
					  and p0, temp[1], channel[i], 0x8000000
					*/
					if(!HWInstEncodeTESTInst(psHWInst,
											 0, /* uRepeatCount */
											 EURASIA_USE1_TEST_CRCOMB_AND,
											 &asTemps[1],
											 ppsChannel[(i * 2) + j],
											 &asTemps[4],
											 0, /* predicate destination */
											 EURASIA_USE1_TEST_STST_NONE,
											 EURASIA_USE1_TEST_ZTST_NOTZERO,
											 EURASIA_USE1_TEST_CHANCC_SELECT0,
											 EURASIA_USE0_TEST_ALUSEL_BITWISE,
											 EURASIA_USE0_TEST_ALUOP_BITWISE_AND))
					{
						USP_DBGPRINT(("Unable to encode HW TEST instruction\n"));
						return IMG_FALSE;
					}

					/* Set the opcode */
					psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_TEST;
					psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

					/* Get the next HW Instruction */
					psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

					/* If it is negative, check it isn't too negative */
				
					/*
					   p0 : and temp[1], channel[i], 0xffff8000
					*/
					if(!HWInstEncodeANDInst(psHWInst,
											1, /* uRepeatCount */
											IMG_TRUE, /* bUsePred */
											IMG_FALSE, /* bNegatePred */
											0, /* uPredNum */
											IMG_TRUE, /* bSkipInv */
											&asTemps[1],
											ppsChannel[(i * 2) + j],
											&sNegMaxMask))
					{
						USP_DBGPRINT(("Unable to encode HW AND instruction\n"));
						return IMG_FALSE;
					}

					/* Set the opcode */
					psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_AND;
					psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
					/* Get the next HW Instruction */
					psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

					/*
					   p0 : xor temp[1], temp[1], 0xffff8000
					*/
					if(!HWInstEncodeXORInst(psHWInst,
											1, /* uRepeatCount */
											EURASIA_USE1_EPRED_P0, /* uPred */
											IMG_TRUE, /* bSkipInv */
											&asTemps[1],
											&asTemps[1],
											&sNegMaxMask))
					{
						USP_DBGPRINT(("Unable to encode HW AND instruction\n"));
						return IMG_FALSE;
					}

					/* Set the opcode */
					psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_XOR;
					psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
					/* Get the next HW Instruction */
					psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

					/* p0 : temp[1] = temp[1] ?  0x8000 : channel[i] */
					if(!HWInstEncodeMOVCInst(psHWInst,
											 1, /* uRepeatCount */
											 EURASIA_USE1_EPRED_P0, /* uPred */
											 IMG_TRUE, /* bTestDst */
											 IMG_TRUE, /* bSkipInv */
											 &asTemps[1],
											 &asTemps[1],
					                         ppsChannel[(i * 2) + j],
					                         &asTemps[2] /* special case value */))
					{
						USP_DBGPRINT(("Unable to encode HW MOVC instruction\n"));
						return IMG_FALSE;
					}

					psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_VMOV;
					psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

					/* Get the next HW Instruction */
					psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

					/* Test if the number is too positive and we have to saturate it */

					/*
					  !p0 and temp[1], channel[i], 0x7fff8000
					*/
					if(!HWInstEncodeANDInst(psHWInst,
											1, /* uRepeatCount */
											IMG_TRUE, /* bUsePred */
											IMG_TRUE, /* bNegatePred */
											0, /* uPredNum */
											IMG_TRUE, /* bSkipInv */
											&asTemps[1],
											ppsChannel[(i * 2) + j],
											&sPosMaxMask))
					{
						USP_DBGPRINT(("Unable to encode HW AND instruction\n"));
						return IMG_FALSE;
					}

					/* Set the opcode */
					psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_XOR;
					psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
	
					/* Get the next HW Instruction */
					psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

					/* temp[1] = temp[1] : channel[i] ? 0x7fff */
					if(!HWInstEncodeMOVCInst(psHWInst,
											 1, /* uRepeatCount */
											 EURASIA_USE1_EPRED_NOTP0, /* uPred */
											 IMG_TRUE, /* bTestDst */
											 IMG_TRUE, /* bSkipInv */
											 &asTemps[1],
											 &asTemps[1],
					                         ppsChannel[(i * 2) + j],
					                         &asTemps[3] /* special case value */))
					{
						USP_DBGPRINT(("Unable to encode HW MOVC instruction\n"));
						return IMG_FALSE;
					}

					psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_VMOV;
					psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

					/* Get the next HW Instruction */
					psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

					if(!HWInstEncodePCKUNPCKInst(psHWInst,
												 IMG_TRUE, /* bSkipInv */
									  			 USP_PCKUNPCK_FMT_S16,
									  			 USP_PCKUNPCK_FMT_S16,
									  			 IMG_FALSE, /* bScaleEnable */
					                             1 << j,
									  			 &asTemps[0],
												 0,
												 1,
												 &asTemps[1],
												 &asTemps[1]))
					{
						USP_DBGPRINT(("Unable to encode HW PCKS16S16 instruction\n"));
						return IMG_FALSE;
					}

					/* set description */
					psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
					psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

					/* Get the next HW Instruction */
					psHWInst = &psTextureWrite->asInsts[++uHWInstCount];
				}

				/* Set the offset for the store instruction */
				if(i == 0)
					psOffset = &sZero;
				else
					psOffset = &sOne;

				if(!HWInstEncodeSTInst(psHWInst,
									   IMG_TRUE, /* bSkipInv */
									   1, /* uRepeatCount */
									   psAddr,
									   psOffset,
									   &asTemps[0]))
				{
					USP_DBGPRINT(("Unable to encode HW ST instruction\n"));
					return IMG_FALSE;
				}

				/* set description */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_ST;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];
			}

			psTextureWrite->uInstCount = uHWInstCount;

			/* This path used 5 temps. */
			*puTempsUsed = 5;

#else /* defined(SGX_FEATURE_USE_VEC34) */

			PVR_UNREFERENCED_PARAMETER(j);
			PVR_UNREFERENCED_PARAMETER(psOffset);

			/* 
			   We need one temp per channel and two for the packed
			   results, so retrun false if we don't have enough.
			*/
			if(uNumTemps < 7)
			{
				USP_DBGPRINT(("Not enough temps to pack from I32 to I16. Have %d but need 7\n", uNumTemps));
				return IMG_FALSE;
			}

			/* temp4 = 0x8000 */
			if(!HWInstEncodeLIMMInst(psHWInst,
									 1, /* uRepeatCount */
									 &asTemps[4],
									 0x8000))
			{
				USP_DBGPRINT(("Unable to encode HW LIMM instruction\n"));
				return IMG_FALSE;
			}

			/* Set the opcode */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_LIMM;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/* temp5 = 0x7fff */
			if(!HWInstEncodeLIMMInst(psHWInst,
									 1, /* uRepeatCount */
									 &asTemps[5],
									 0x7fff))
			{
				USP_DBGPRINT(("Unable to encode HW LIMM instruction\n"));
				return IMG_FALSE;
			}

			/* Set the opcode */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_LIMM;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/* temp6 =  sign bit*/
			if(!HWInstEncodeLIMMInst(psHWInst,
									 1, /* uRepeatCount */
									 &asTemps[6],
									 0x80000000))
			{
				USP_DBGPRINT(("Unable to encode HW LIMM instruction\n"));
				return IMG_FALSE;
			}

			/* Set the opcode */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_LIMM;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/*
			   For each channel we pack from S32 to S16.
			*/
			for(i = 0; i < 4; i++)
			{
				/* Test if the number is too negative and we have to saturate it */

				/* Start by seeing if it is negative */

				/*
				  and p0, temp[i], channel[i], 0x8000000
				*/
				if(!HWInstEncodeTESTInst(psHWInst,
										 1, /* uRepeatCount */
										 EURASIA_USE1_TEST_CRCOMB_AND,
										 &asTemps[i],
										 ppsChannel[i],
										 &asTemps[6],
										 0, /* predicate destination */
										 EURASIA_USE1_TEST_STST_NONE,
										 EURASIA_USE1_TEST_ZTST_NOTZERO,
										 EURASIA_USE1_TEST_CHANCC_SELECT0,
										 EURASIA_USE0_TEST_ALUSEL_BITWISE,
										 EURASIA_USE0_TEST_ALUOP_BITWISE_AND))
				{
					USP_DBGPRINT(("Unable to encode HW TEST instruction\n"));
					return IMG_FALSE;
				}

				/* Set the opcode */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_TEST;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/* If it is negative, check it isn't too negative */
				
				/*
				   p0 : and temp[i], channel[i], 0xffff8000
				*/
				if(!HWInstEncodeANDInst(psHWInst,
										1, /* uRepeatCount */
										IMG_TRUE, /* bUsePred */
										IMG_FALSE, /* bNegatePred */
										0, /* uPredNum */
										IMG_TRUE, /* bSkipInv */
										&asTemps[i],
										ppsChannel[i],
										&sNegMaxMask))
				{
					USP_DBGPRINT(("Unable to encode HW AND instruction\n"));
					return IMG_FALSE;
				}

				/* Set the opcode */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_AND;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/*
				   p0 : xor temp[i], temp[i], 0xffff8000
				*/
				if(!HWInstEncodeXORInst(psHWInst,
										1, /* uRepeatCount */
										EURASIA_USE1_EPRED_P0, /* uPred */
										IMG_TRUE, /* bSkipInv */
										&asTemps[i],
										&asTemps[i],
										&sNegMaxMask))
				{
					USP_DBGPRINT(("Unable to encode HW AND instruction\n"));
					return IMG_FALSE;
				}

				/* Set the opcode */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_XOR;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/* p0 : temp[i] = temp[i] ?  0x8000 : channel[i] */
				if(!HWInstEncodeMOVCInst(psHWInst,
										 1, /* uRepeatCount */
										 EURASIA_USE1_EPRED_P0, /* uPred */
										 IMG_TRUE, /* bTestDst */
										 IMG_TRUE, /* bSkipInv */
										 &asTemps[i],
										 &asTemps[i],
										 &asTemps[4], /* special case value */
										 ppsChannel[i]))
				{
					USP_DBGPRINT(("Unable to encode HW MOVC instruction\n"));
					return IMG_FALSE;
				}

				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_MOVC;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/* Test if the number is too positive and we have to saturate it */

				/*
				  !p0 and temp[i], channel[i], 0x7fff8000
				*/
				if(!HWInstEncodeANDInst(psHWInst,
										1, /* uRepeatCount */
										IMG_TRUE, /* bUsePred */
										IMG_TRUE, /* bNegatePred */
										0, /* uPredNum */
										IMG_TRUE, /* bSkipInv */
										&asTemps[i],
										ppsChannel[i],
										&sPosMaxMask))
				{
					USP_DBGPRINT(("Unable to encode HW AND instruction\n"));
					return IMG_FALSE;
				}

				/* Set the opcode */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_XOR;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
				
				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

				/* temp[i] = temp[i] : channel[i] ? 0x7fff */
				if(!HWInstEncodeMOVCInst(psHWInst,
										 1, /* uRepeatCount */
										 EURASIA_USE1_EPRED_NOTP0, /* uPred */
										 IMG_TRUE, /* bTestDst */
										 IMG_TRUE, /* bSkipInv */
										 &asTemps[i],
										 &asTemps[i],
										 &asTemps[5], /* special case value */
										 ppsChannel[i]))
				{
					USP_DBGPRINT(("Unable to encode HW MOVC instruction\n"));
					return IMG_FALSE;
				}

				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_MOVC;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];
			}

			/*
			   So at this point the packed data for this channel is in
			   temp0 to temp3 and we need to pack it into the two
			   registers we are going to store.
			*/
			if(!HWInstEncodePCKUNPCKInst(psHWInst,
										 IMG_TRUE, /* bSkipInv */
							  			 USP_PCKUNPCK_FMT_S16,
							  			 USP_PCKUNPCK_FMT_S16,
							  			 IMG_FALSE, /* bScaleEnable */
										 0xF,
							  			 &asTemps[4],
										 0,
										 1,
										 &asTemps[0],
										 &asTemps[1]))
			{
				USP_DBGPRINT(("Unable to encode HW PCKS16S16 instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			if(!HWInstEncodePCKUNPCKInst(psHWInst,
										 IMG_TRUE, /* bSkipInv */
							  			 USP_PCKUNPCK_FMT_S16,
							  			 USP_PCKUNPCK_FMT_S16,
							  			 IMG_FALSE, /* bScaleEnable */
										 0xF,
							  			 &asTemps[5],
										 0,
										 1,
										 &asTemps[2],
										 &asTemps[3]))
			{
				USP_DBGPRINT(("Unable to encode HW PCKS16S16 instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/*
			   FIXME: We should be able to use a fetch/repeat here
			*/
			if(!HWInstEncodeSTInst(psHWInst,
								   IMG_TRUE, /* bSkipInv */
								   1, /* uRepeatCount */
								   psAddr,
								   &sZero,
								   &asTemps[4]))
			{
				USP_DBGPRINT(("Unable to encode HW ST instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_ST;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];
			
			if(!HWInstEncodeSTInst(psHWInst,
								   IMG_TRUE, /* bSkipInv */
								   1, /* uRepeatCount */
								   psAddr,
								   &sOne,
								   &asTemps[5]))
			{
				USP_DBGPRINT(("Unable to encode HW ST instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_ST;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Increment HW instruction count */
			psTextureWrite->uInstCount = ++uHWInstCount;

			/* This path used 7 temps. */
			*puTempsUsed = 7;
#endif /* defined(SGX_FEATURE_USE_VEC34) */

			break;
		}

		case USP_TEXTURE_FORMAT_RGBA_S32:
		{
			IMG_UINT32	i			= 0;
			USP_REG		sOffset		= {0};

			sOffset.eType 	= USP_REGTYPE_IMM;
			sOffset.uNum  	= 0;
			sOffset.eFmt  	= USP_REGFMT_U32;
			sOffset.uComp 	= 0;
			sOffset.eDynIdx = USP_DYNIDX_NONE;

			for(i = 0; i < 4; i++)
			{
				sOffset.uNum = i;

				if(!HWInstEncodeSTInst(psHWInst,
									   IMG_TRUE, /* bSkipInv */
									   1, /* uRepeatCount */
									   psAddr,
									   &sOffset,
									   ppsChannel[i]))
				{
					USP_DBGPRINT(("Unable to encode HW ST instruction\n"));
					return IMG_FALSE;
				}

				/* set description */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_ST;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];
			}

			/* Update the instruction count */
			psTextureWrite->uInstCount = uHWInstCount;

			*puTempsUsed = 0;
			break;
		}

		default: USP_DBGPRINT(("Unsupported texture write format %d for write ID %d\n",
							 psContext->asSamplerDesc[psTextureWrite->uWriteID].sTexFormat.eTexFmt,
							 psTextureWrite->uWriteID));
				 break;
	}

	return IMG_TRUE;
}

/******************************************************************************
 Name:		USPTextureWritePackFromF32

 Purpose:	Emit HW instructions to pack to the right image format
 			and store to the given address.

 Inputs:	psTextureWrite	- The texture write to generate code for
			psContext		- The current USP execution context
			psAddr			- The address the store the data to
			asTemps			- An array of temps
			uNumTemps		- The number of temps in the array
			puTempsUsed		- Used to return the number of temps used

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL USPTextureWritePackFromF32(PUSP_TEXTURE_WRITE 	psTextureWrite,
										   PUSP_CONTEXT			psContext,
										   PUSP_REG				psAddr,
										   PUSP_REG				asTemps,
										   IMG_UINT32			uNumTemps,
										   IMG_UINT32*			puTempsUsed)
{
	IMG_UINT32 uHWInstCount   = psTextureWrite->uInstCount;
	IMG_UINT32 uInstDescFlags = psTextureWrite->uInstDescFlags;
	PHW_INST   psHWInst;

	USP_REG	sZero                               = {0};
	USP_REG	sOne                                = {0};
	PUSP_REG ppsChannel[USP_TEXFORM_CHAN_COUNT] = {IMG_NULL};

	sZero.eType 	= USP_REGTYPE_IMM;
	sZero.uNum  	= 0;
	sZero.eFmt  	= USP_REGFMT_U32;
	sZero.uComp 	= 0;
	sZero.eDynIdx 	= USP_DYNIDX_NONE;

	sOne.eType 		= USP_REGTYPE_IMM;
	sOne.uNum  		= 1;
	sOne.eFmt  		= USP_REGFMT_U32;
	sOne.uComp 		= 0;
	sOne.eDynIdx 	= USP_DYNIDX_NONE;
	
	/* Get the HW Instruction */
	psHWInst = &psTextureWrite->asInsts[uHWInstCount];

	/* Swizzle the source registers appropriately */
	SwizzleChannels(ppsChannel,
	                psTextureWrite->asChannel,
	                psContext->asSamplerDesc[psTextureWrite->uWriteID].sTexFormat.aeChanSwizzle);
	
	switch(psContext->asSamplerDesc[psTextureWrite->uWriteID].sTexFormat.eTexFmt)
	{
		/* This is UNORM_INT8 */
		case USP_TEXTURE_FORMAT_RGBA_UNORM8:
		{
			/* 
			   We need one temp for the packed result,
			   so retrun false if we don't have enough.
			*/
			if(uNumTemps < 1)
			{
				USP_DBGPRINT(("Not enough temps to pack from F32 to U8. Have %d but need 1\n", uNumTemps));
				return IMG_FALSE;
			}
		
			if(!HWInstEncodePCKUNPCKInst(psHWInst,
										 IMG_TRUE, /* bSkipInv */
							  			 USP_PCKUNPCK_FMT_U8,
							  			 USP_PCKUNPCK_FMT_F32,
							  			 IMG_TRUE, /* bScaleEnable */
										 0xC, /* Write the top half */
							  			 &asTemps[0],
										 2,
										 3,
										 ppsChannel[2],
										 ppsChannel[3]))
			{
				USP_DBGPRINT(("Unable to encode HW PCKU8F32 instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/* We have written the top half so now write the bottom half */
			if(!HWInstEncodePCKUNPCKInst(psHWInst,
										 IMG_TRUE, /* bSkipInv */
							  			 USP_PCKUNPCK_FMT_U8,
							  			 USP_PCKUNPCK_FMT_F32,
							  			 IMG_TRUE, /* bScaleEnable */
										 0x3, /* Write the bottom half */
							  			 &asTemps[0],
										 0,
										 1,
										 ppsChannel[0],
										 ppsChannel[1]))
			{
				USP_DBGPRINT(("Unable to encode HW PCKU8F32 instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;
			
			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			if(!HWInstEncodeSTInst(psHWInst,
								   IMG_TRUE, /* bSkipInv */
								   1, /* uRepeatCount */
								   psAddr,
								   &sZero,
								   &asTemps[0]))
			{
				USP_DBGPRINT(("Unable to encode HW ST instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_ST;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Increment HW instruction count */
			psTextureWrite->uInstCount = ++uHWInstCount;

			/* This path used 1 temp */
			*puTempsUsed = 1;
			break;
		}

		/* This is UNORM_INT16 */
		case USP_TEXTURE_FORMAT_RGBA_UNORM16:
		{
			/* 
			   We need two temps for the packed results,
			   so retrun false if we don't have enough.
			*/
			if(uNumTemps < 2)
			{
				USP_DBGPRINT(("Not enough temps to pack from F32 to U16. Have %d but need 2\n", uNumTemps));
				return IMG_FALSE;
			}

			if(!HWInstEncodePCKUNPCKInst(psHWInst,
										 IMG_TRUE, /* bSkipInv */
							  			 USP_PCKUNPCK_FMT_U16,
							  			 USP_PCKUNPCK_FMT_F32,
							  			 IMG_TRUE, /* bScaleEnable */
										 0xF,
							  			 &asTemps[0],
										 0,
										 1,
										 ppsChannel[2],
										 ppsChannel[3]))
			{
				USP_DBGPRINT(("Unable to encode HW PCKU16F32 instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/* We have written the top half so now write the bottom half */
			if(!HWInstEncodePCKUNPCKInst(psHWInst,
										 IMG_TRUE, /* bSkipInv */
							  			 USP_PCKUNPCK_FMT_U16,
							  			 USP_PCKUNPCK_FMT_F32,
							  			 IMG_TRUE, /* bScaleEnable */
										 0xF,
							  			 &asTemps[1],
										 0,
										 1,
										 ppsChannel[0],
										 ppsChannel[1]))
			{
				USP_DBGPRINT(("Unable to encode HW PCKU16F32 instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/*
			   FIXME: We should be able to use a fetch/repeat here
			*/
			if(!HWInstEncodeSTInst(psHWInst,
								   IMG_TRUE, /* bSkipInv */
								   1, /* uRepeatCount */
								   psAddr,
								   &sZero,
								   &asTemps[1]))
			{
				USP_DBGPRINT(("Unable to encode HW ST instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_ST;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];
			
			if(!HWInstEncodeSTInst(psHWInst,
								   IMG_TRUE, /* bSkipInv */
								   1, /* uRepeatCount */
								   psAddr,
								   &sOne,
								   &asTemps[0]))
			{
				USP_DBGPRINT(("Unable to encode HW ST instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_ST;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Increment HW instruction count */
			psTextureWrite->uInstCount = ++uHWInstCount;

			/* This path used 2 temps for the data we store */
			*puTempsUsed = 2;
			break;
		}

		case USP_TEXTURE_FORMAT_RGBA_F16:
		{
			/* 
			   We need two temps for the packed results,
			   so retrun false if we don't have enough.
			*/
			if(uNumTemps < 2)
			{
				USP_DBGPRINT(("Not enough temps to pack from F32 to F16. Have %d but need 2\n", uNumTemps));
				return IMG_FALSE;
			}

			if(!HWInstEncodePCKUNPCKInst(psHWInst,
										 IMG_TRUE, /* bSkipInv */
							  			 USP_PCKUNPCK_FMT_F16,
							  			 USP_PCKUNPCK_FMT_F32,
							  			 IMG_TRUE, /* bScaleEnable */
#if defined(SGX_FEATURE_USE_VEC34)
										 0x3,
#else
										 0xF,
#endif/*defined(SGX_FEATURE_USE_VEC34)*/
							  			 &asTemps[0],
										 0,
										 1,
										 ppsChannel[0],
										 ppsChannel[1]))
			{
				USP_DBGPRINT(("Unable to encode HW PCKF16F32 instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];

			/* We have written the top half so now write the bottom half */
			if(!HWInstEncodePCKUNPCKInst(psHWInst,
										 IMG_TRUE, /* bSkipInv */
							  			 USP_PCKUNPCK_FMT_F16,
							  			 USP_PCKUNPCK_FMT_F32,
							  			 IMG_TRUE, /* bScaleEnable */
#if defined(SGX_FEATURE_USE_VEC34)
										 0x3,
#else
										 0xF,
#endif/*defined(SGX_FEATURE_USE_VEC34)*/
							  			 &asTemps[1],
										 0,
										 1,
										 ppsChannel[2],
										 ppsChannel[3]))
			{
				USP_DBGPRINT(("Unable to encode HW PCKF16F32 instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_PCKUNPCK;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];
			
			/*
			   FIXME: We should be able to use a fetch/repeat here
			*/
			if(!HWInstEncodeSTInst(psHWInst,
								   IMG_TRUE, /* bSkipInv */
								   1, /* uRepeatCount */
								   psAddr,
								   &sZero,
								   &asTemps[0]))
			{
				USP_DBGPRINT(("Unable to encode HW ST instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_ST;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Get the next HW Instruction */
			psHWInst = &psTextureWrite->asInsts[++uHWInstCount];
			
			if(!HWInstEncodeSTInst(psHWInst,
								   IMG_TRUE, /* bSkipInv */
								   1, /* uRepeatCount */
								   psAddr,
								   &sOne,
								   &asTemps[1]))
			{
				USP_DBGPRINT(("Unable to encode HW ST instruction\n"));
				return IMG_FALSE;
			}

			/* set description */
			psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_ST;
			psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

			/* Increment HW instruction count */
			psTextureWrite->uInstCount = ++uHWInstCount;

			/* This path used 2 temps for the data we store */
			*puTempsUsed = 2;
			break;
		}

		case USP_TEXTURE_FORMAT_RGBA_F32:
		{
			IMG_UINT32	i			= 0;
			USP_REG		sOffset		= {0};

			sOffset.eType 	= USP_REGTYPE_IMM;
			sOffset.uNum  	= 0;
			sOffset.eFmt  	= USP_REGFMT_U32;
			sOffset.uComp 	= 0;
			sOffset.eDynIdx = USP_DYNIDX_NONE;

			for(i = 0; i < 4; i++)
			{
				sOffset.uNum = i;

				if(!HWInstEncodeSTInst(psHWInst,
									   IMG_TRUE, /* bSkipInv */
									   1, /* uRepeatCount */
									   psAddr,
									   &sOffset,
									   ppsChannel[i]))
				{
					USP_DBGPRINT(("Unable to encode HW ST instruction\n"));
					return IMG_FALSE;
				}

				/* set description */
				psTextureWrite->aeOpcodeForInsts[uHWInstCount] = USP_OPCODE_ST;
				psTextureWrite->auInstDescFlagsForInsts[uHWInstCount] = uInstDescFlags;

				/* Get the next HW Instruction */
				psHWInst = &psTextureWrite->asInsts[++uHWInstCount];
			}

			/* Update the instruction count */
			psTextureWrite->uInstCount = uHWInstCount;

			*puTempsUsed = 0;
			break;
		}

		default: USP_DBGPRINT(("Unsupported texture write format %d for write ID %d\n",
							 psContext->asSamplerDesc[psTextureWrite->uWriteID].sTexFormat.eTexFmt,
							 psTextureWrite->uWriteID));
				 break;
	}

	return IMG_TRUE;
}

/******************************************************************************
 Name:		GetTextureWriteSize

 Purpose:	Returns the size (in bytes) of a pixel in a texture
 			of the given register format

 Inputs:	eFmt	- The texture format	

 Outputs:	none

 Returns:	(As above)
*****************************************************************************/
static IMG_UINT32 GetTextureWriteSize(USP_TEXTURE_FORMAT eFmt)
{
	switch(eFmt)
	{
		/* 8 bits per channel */
	    case USP_TEXTURE_FORMAT_RGBA_S8     :
	    case USP_TEXTURE_FORMAT_RGBA_U8     :
	    case USP_TEXTURE_FORMAT_RGBA_UNORM8 : return 4;

		/* 16 bits per channel */
	    case USP_TEXTURE_FORMAT_RGBA_S16     :
		case USP_TEXTURE_FORMAT_RGBA_U16     :
	    case USP_TEXTURE_FORMAT_RGBA_F16     :
	    case USP_TEXTURE_FORMAT_RGBA_UNORM16 : return 8;

		/* 32 bits per channel */
	    case USP_TEXTURE_FORMAT_RGBA_F32 :
	    case USP_TEXTURE_FORMAT_RGBA_S32 :
		case USP_TEXTURE_FORMAT_RGBA_U32 : return 16;

		default : return 0;
	}
}

/******************************************************************************
 Name:		SwizzleChannels

 Purpose:	Swizzles the channels.
			Assumption: At least USP_TEXFORM_CHAN_COUNT entries

 Inputs:	ppsSwizzled   - destination for swizzled pointer array
			asChannel     - array of registers
			aeChanSwizzle - channel swizzle

 Outputs:	ppsSwizzled - destination for swizzled pointer array

 Returns:	A swizzled array according fo aeChanSwizzle.
*****************************************************************************/
static IMG_VOID SwizzleChannels(PUSP_REG *ppsSwizzled, PUSP_REG asChannel, PUSP_CHAN_SWIZZLE aeChanSwizzle)
{
	IMG_UINT32 i;
		
	for(i=0; i < USP_TEXFORM_CHAN_COUNT; i++)
	{
		ppsSwizzled[aeChanSwizzle[i]] = &asChannel[i];
	}
	
	return;
}

/******************************************************************************
 End of file (usp_imgwrite.c)
******************************************************************************/
