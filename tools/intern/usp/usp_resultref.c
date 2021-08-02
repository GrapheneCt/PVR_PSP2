/******************************************************************************
 Name           : USP_RESULTREF.C

 Title          : USP result-reference implementation

 C Author       : Joe Molleson

 Created        : 02/01/2002

 Copyright      : 2002-2007 by Imagination Technologies Limited.
                : All rights reserved. No part of this software, either
                : material or conceptual may be copied or distributed,
                : transmitted, transcribed, stored in a retrieval system or
                : translated into any human or computer language in any form
                : by any means, electronic, mechanical, manual or otherwise,
                : or disclosed to third parties without the express written
                : permission of Imagination Technologies Limited,
                : Home Park Estate, Kings Langley, Hertfordshire,
                : WD4 8LZ, U.K.

 Description    : Code to create, destroy and manage result-references for
                  and USP shader.

 Program Type   : 32-bit DLL

 Version        : $Revision: 1.14 $

 Modifications  :
 $Log: usp_resultref.c $
 .#
  --- Revision Logs Removed --- 
******************************************************************************/
#include <memory.h>
#include "img_types.h"
#include "sgxdefs.h"
#include "usp_typedefs.h"
#include "usc.h"
#include "usp.h"
#include "hwinst.h"
#include "uspbin.h"
#include "uspshrd.h"
#include "usp_resultref.h"
#include "usp_instblock.h"

/*****************************************************************************
 Name:		USPResultRefSetup

 Purpose:	Initialise a USP result-reference

 Inputs:	psResultRef	- The result-reference to initialise
			psShader	- The shader that the result-ref is for

 Outputs:	none

 Returns:	IMG_FALSE on failure
*****************************************************************************/
IMG_INTERNAL IMG_BOOL USPResultRefSetup(PUSP_RESULTREF	psResultRef,
										PUSP_SHADER		psShader)
{
	psResultRef->psShader			= psShader;
	psResultRef->bActive			= IMG_FALSE;
	psResultRef->psInst				= IMG_NULL;
	psResultRef->uOrgResultRegNum	= psShader->psProgDesc->uDefaultPSResultRegNum;
	psResultRef->psNext				= IMG_NULL;
	psResultRef->psPrev				= IMG_NULL;

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		USPResultRefCreate

 Purpose:	Create an initialise USP result-reference

 Inputs:	psContext	- The current USP context
			psShader	- The shader that the result-ref is for

 Outputs:	none

 Returns:	The created USP result-reference data
*****************************************************************************/
IMG_INTERNAL PUSP_RESULTREF USPResultRefCreate(PUSP_CONTEXT	psContext,
											   PUSP_SHADER	psShader)
{
	PUSP_RESULTREF	psResultRef;

	/*
		Allocate and initialise a result-reference
	*/
	psResultRef = (PUSP_RESULTREF)psContext->pfnAlloc(sizeof(USP_RESULTREF));
	if	(!psResultRef)
	{
		USP_DBGPRINT(("USPResultRefCreate: Failed to alloc USP result-ref data\n"));
		return IMG_NULL;
	}

	USPResultRefSetup(psResultRef, psShader);

	return psResultRef;
}

/*****************************************************************************
 Name:		USPResultRefSetInst

 Purpose:	Set the instruction associated with a result-reference

 Inputs:	psResultRef	- The result reference to update
			psInst		- A USP instruction that references the 
						  shader's result
			psContext	- The current USP context

 Outputs:	none

 Returns:	IMG_FALSE on failure
*****************************************************************************/
IMG_INTERNAL IMG_BOOL USPResultRefSetInst(PUSP_RESULTREF	psResultRef,
										  PUSP_INST			psInst,
										  PUSP_CONTEXT		psContext)
{
	
	IMG_UINT32	uInstDescFlags;
	PHW_INST	psHWInst;

	#if !defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psContext);
	#endif /* #if !defined(DEBUG)*/

	/*
		Associate the instruciton with this result-reference
	*/
	psHWInst = &psInst->sHWInst;

	psResultRef->psInst			= psInst;
	psResultRef->eOpcode		= psInst->sDesc.eOpcode;
	psResultRef->uInstDescFlags	= psInst->sDesc.uFlags;
	psResultRef->eFmtCtl		= psInst->sDesc.eFmtCtl;
	psResultRef->sOrgHWInst		= *psHWInst;

	/*
		If the instruction currently does not references any result-registers,
		disable the result-reference
	*/
	uInstDescFlags = psResultRef->uInstDescFlags;

	if	(uInstDescFlags & (USP_INSTDESC_FLAG_DEST_USES_RESULT |
						   USP_INSTDESC_FLAG_SRC0_USES_RESULT |
						   USP_INSTDESC_FLAG_SRC1_USES_RESULT |
						   USP_INSTDESC_FLAG_SRC2_USES_RESULT))
	{
		psResultRef->bActive = IMG_TRUE;
	}
	else
	{
		psResultRef->bActive = IMG_FALSE;
	}

	/*
		Decode each of the original operands that reference a result-register
	*/
	if	(psResultRef->bActive)
	{
		PUSP_REG	psOrgOperand;
		USP_FMTCTL	eFmtCtl;

		eFmtCtl	= psResultRef->eFmtCtl;

		if	(uInstDescFlags & USP_INSTDESC_FLAG_DEST_USES_RESULT)
		{
			psOrgOperand = &psResultRef->auOrgHWOperands[USP_OPERANDIDX_DST];

			if	(!HWInstDecodeDestBankAndNum(eFmtCtl,
											 psResultRef->eOpcode,
											 psHWInst,
											 psOrgOperand))
			{
				USP_DBGPRINT(("USPResultRefSetInst: Error decoding org dest operand\n"));
				return IMG_FALSE;
			}
		}

		if	(uInstDescFlags & USP_INSTDESC_FLAG_SRC0_USES_RESULT)
		{
			IMG_BOOL bSrc0ExtBanksOK;

			psOrgOperand	= &psResultRef->auOrgHWOperands[USP_OPERANDIDX_SRC0];
			bSrc0ExtBanksOK	= (IMG_BOOL)(uInstDescFlags & USP_INSTDESC_FLAG_SUPPORTS_SRC0_EXTBANKS);

			if	(!HWInstDecodeSrc0BankAndNum(eFmtCtl,
											 bSrc0ExtBanksOK,
											 psHWInst,
											 psOrgOperand))
			{
				USP_DBGPRINT(("USPResultRefSetInst: Error decoding org src0 operand\n"));
				return IMG_FALSE;
			}
		}

		if	(uInstDescFlags & USP_INSTDESC_FLAG_SRC1_USES_RESULT)
		{
			psOrgOperand = &psResultRef->auOrgHWOperands[USP_OPERANDIDX_SRC1];

			if	(!HWInstDecodeSrc1BankAndNum(eFmtCtl, psHWInst, psOrgOperand))
			{
				USP_DBGPRINT(("USPResultRefSetInst: Error decoding org src1 operand\n"));
				return IMG_FALSE;
			}
		}

		if	(uInstDescFlags & USP_INSTDESC_FLAG_SRC2_USES_RESULT)
		{
			psOrgOperand = &psResultRef->auOrgHWOperands[USP_OPERANDIDX_SRC2];

			if	(!HWInstDecodeSrc2BankAndNum(eFmtCtl, psHWInst, psOrgOperand))
			{
				USP_DBGPRINT(("USPResultRefSetInst: Error decoding org src2 operand\n"));
				return IMG_FALSE;
			}
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		USPResultRefDestroy

 Purpose:	Destroy USP result-reference data previously created using
			USPResultRefCreate

 Inputs:	psResultRef	- The USP result-reference info do destroy
			psContext	- The current USP context

 Outputs:	none

 Returns:	none
*****************************************************************************/
IMG_INTERNAL IMG_VOID USPResultRefDestroy(PUSP_RESULTREF	psResultRef,
										  PUSP_CONTEXT		psContext)
{
	/*
		Only free result-references that were allocated for pre-compiled
		instructions
	*/
	if	(psResultRef->psInst->sDesc.uFlags & USP_INSTDESC_FLAG_PRECOMPILED_INST)
	{
		psContext->pfnFree(psResultRef);
	}
}

/*****************************************************************************
 Name:		USPResultRefReset

 Purpose:	Reset a USP result-reference ready for finalisation

 Inputs:	psResultRef	- The USP result-reference to reset

 Outputs:	none

 Returns:	none
*****************************************************************************/
IMG_INTERNAL IMG_VOID USPResultRefReset(PUSP_RESULTREF psResultRef)
{
	/*
		Restore the original pre-compiled HW-instruction for references to non
		USP-generated insts
	*/
	if	(psResultRef->uInstDescFlags & USP_INSTDESC_FLAG_PRECOMPILED_INST)
	{
		psResultRef->sOrgHWInst = psResultRef->psInst->sHWInst;
	}
}

/*****************************************************************************
 Name:		USPResultRefSupportsRegLocation

 Purpose:	Check that a result-reference can support a specific location for
			the shaders results.

 Inputs:	psResultRef			- The USP result-reference to check
			eNewResultRegType	- The new register bank for the result-regs
			uNewResultRegNum	- The new register index for the result-regs
								  (i.e. the first register used for the
								  shader's results, assumed to be contigous).

 Outputs:	pbRegLocationOK		- Set to IMG_TRUE if the new location can be
								  supported

 Returns:	IMG_TRUE/IMG_FALSE to indicate whether the specified new location
			is supported by the result-ref
*****************************************************************************/
IMG_INTERNAL IMG_BOOL USPResultRefSupportsRegLocation(PUSP_RESULTREF	psResultRef,
													  USP_REGTYPE		eNewResultRegType,
													  IMG_UINT32		uNewResultRegNum)
{
	PUSP_REG	psOrgOperand;
	IMG_UINT32	uOrgResultRegNum;
	IMG_UINT32	uInstDescFlags;
	USP_FMTCTL	eFmtCtl;
	USP_OPCODE	eOpcode;
	PHW_INST	psHWInst;
	USP_REG		sNewOperand;

	/*
		Ignore inactive result-references
	*/
	if	(!psResultRef->bActive)
	{
		return IMG_TRUE;
	}

	/*
		Try changing a sacrificial copy of the associated HW-instruction,
		to confirm that the new result-location is OK
	*/
	uOrgResultRegNum	= psResultRef->uOrgResultRegNum;
	uInstDescFlags		= psResultRef->uInstDescFlags;
	eFmtCtl				= psResultRef->eFmtCtl;
	eOpcode				= psResultRef->eOpcode;
	psHWInst			= &psResultRef->sOrgHWInst;

	if	(uInstDescFlags & USP_INSTDESC_FLAG_DEST_USES_RESULT)
	{
		/*
			Try changing the destination operand to use the proposed
			result-location
		*/
		psOrgOperand = &psResultRef->auOrgHWOperands[USP_OPERANDIDX_DST];

		sNewOperand			= *psOrgOperand;
		sNewOperand.eType	= eNewResultRegType;
		sNewOperand.uNum	= (psOrgOperand->uNum - uOrgResultRegNum) +
							   uNewResultRegNum;

		if	(!HWInstEncodeDestBankAndNum(eFmtCtl,
										 eOpcode,
										 psHWInst,
										 &sNewOperand))
		{
			return IMG_FALSE;
		}		
	}

	if	(uInstDescFlags & USP_INSTDESC_FLAG_SRC0_USES_RESULT)
	{
		IMG_BOOL bSrc0ExtBanksOK;

		/*
			Try changing the source 0 operand to use the proposed
			result-location
		*/
		bSrc0ExtBanksOK	= (IMG_BOOL)(uInstDescFlags & USP_INSTDESC_FLAG_SUPPORTS_SRC0_EXTBANKS);
		psOrgOperand	= &psResultRef->auOrgHWOperands[USP_OPERANDIDX_SRC0];

		sNewOperand			= *psOrgOperand;
		sNewOperand.eType	= eNewResultRegType;
		sNewOperand.uNum	= (psOrgOperand->uNum - uOrgResultRegNum) +
							  uNewResultRegNum;

		if	(!HWInstEncodeSrc0BankAndNum(eFmtCtl,
										 eOpcode,
										 bSrc0ExtBanksOK,
										 psHWInst,
										 &sNewOperand))
		{
			return IMG_FALSE;
		}
	}

	if	(uInstDescFlags & USP_INSTDESC_FLAG_SRC1_USES_RESULT)
	{
		/*
			Try changing the source 1 operand to use the proposed
			result-location
		*/
		psOrgOperand = &psResultRef->auOrgHWOperands[USP_OPERANDIDX_SRC1];

		sNewOperand			= *psOrgOperand;
		sNewOperand.eType	= eNewResultRegType;
		sNewOperand.uNum	= (psOrgOperand->uNum - uOrgResultRegNum) +
							  uNewResultRegNum;

		if	(!HWInstEncodeSrc1BankAndNum(eFmtCtl, eOpcode, psHWInst, &sNewOperand))
		{
			return IMG_FALSE;
		}
	}

	if	(uInstDescFlags & USP_INSTDESC_FLAG_SRC2_USES_RESULT)
	{
		/*
			Try changing the source 2 operand to use the proposed
			result-location
		*/
		psOrgOperand = &psResultRef->auOrgHWOperands[USP_OPERANDIDX_SRC2];

		sNewOperand			= *psOrgOperand;
		sNewOperand.eType	= eNewResultRegType;
		sNewOperand.uNum	= (psOrgOperand->uNum - uOrgResultRegNum) +
							  uNewResultRegNum;

		if	(!HWInstEncodeSrc2BankAndNum(eFmtCtl, eOpcode, psHWInst, &sNewOperand))
		{
			return IMG_FALSE;
		}
	}

	/*
		The requested register bank and index is supported
	*/
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		USPResultRefChangeRegLocation

 Purpose:	Update an instruction that references the results of a shader,
			to access the result from a new location.

 Inputs:	psContext			- The current USP context
			psResultRef			- The USP result-reference to update
			eNewResultRegType	- The new register bank for the result-regs
			uNewResultRegNum	- The new register index for the result-regs
								  (i.e. the first register used for the
								  shader's results, assumed to be contigous).

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL USPResultRefChangeRegLocation(PUSP_CONTEXT	psContext,
													PUSP_RESULTREF	psResultRef,
													USP_REGTYPE		eNewResultRegType,
													IMG_UINT32		uNewResultRegNum)
{
	PUSP_REG	psOrgOperand;
	IMG_UINT32	uOrgResultRegNum;
	PHW_INST	psHWInst;
	IMG_UINT32	uInstDescFlags;
	USP_FMTCTL	eFmtCtl;
	USP_OPCODE	eOpcode;
	USP_REG		sNewOperand;

	#if !defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psContext);
	#endif	/* #if !defined(DEBUG) */

	/*
		Ignore inactive result-references
	*/
	if	(!psResultRef->bActive)
	{
		return IMG_TRUE;
	}

	/*
		Modify the associated HW-instruction, to reference the shader-results
		from their new location
	*/
	uOrgResultRegNum	= psResultRef->uOrgResultRegNum;
	uInstDescFlags		= psResultRef->uInstDescFlags;
	eFmtCtl				= psResultRef->eFmtCtl;
	psHWInst			= &psResultRef->psInst->sHWInst;
	eOpcode				= psResultRef->eOpcode;

	if	(uInstDescFlags & USP_INSTDESC_FLAG_DEST_USES_RESULT)
	{
		/*
			Modify the destination operand to use the new result-location
		*/
		psOrgOperand = &psResultRef->auOrgHWOperands[USP_OPERANDIDX_DST];

		sNewOperand			= *psOrgOperand;
		sNewOperand.eType	= eNewResultRegType;
		sNewOperand.uNum	= (psOrgOperand->uNum - uOrgResultRegNum) +
							   uNewResultRegNum;

		if	(!HWInstEncodeDestBankAndNum(eFmtCtl,
										 eOpcode,
										 psHWInst,
										 &sNewOperand))
		{
			USP_DBGPRINT(("USPResultRefChangeRegLocation: Error encoding new dest operand\n"));
			return IMG_FALSE;
		}

		/*
			If appropriate, update thw destination write mask of the
			instruction to ensure that all components of the result are
			initialised.
		*/		
	}

	if	(uInstDescFlags & USP_INSTDESC_FLAG_SRC0_USES_RESULT)
	{
		IMG_BOOL bSrc0ExtBanksOK;

		/*
			Modify the source 0 operand to use the new result-location
		*/
		bSrc0ExtBanksOK	= (IMG_BOOL)(uInstDescFlags & USP_INSTDESC_FLAG_SUPPORTS_SRC0_EXTBANKS);
		psOrgOperand	= &psResultRef->auOrgHWOperands[USP_OPERANDIDX_SRC0];

		sNewOperand			= *psOrgOperand;
		sNewOperand.eType	= eNewResultRegType;
		sNewOperand.uNum	= (psOrgOperand->uNum - uOrgResultRegNum) +
							   uNewResultRegNum;

		if	(!HWInstEncodeSrc0BankAndNum(eFmtCtl,
										 eOpcode,
										 bSrc0ExtBanksOK,
										 psHWInst,
										 &sNewOperand))
		{
			USP_DBGPRINT(("USPResultRefChangeRegLocation: Error encoding new src0 operand\n"));
			return IMG_FALSE;
		}
	}

	if	(uInstDescFlags & USP_INSTDESC_FLAG_SRC1_USES_RESULT)
	{
		/*
			Modify the source 1 operand to use the new result-location
		*/
		psOrgOperand = &psResultRef->auOrgHWOperands[USP_OPERANDIDX_SRC1];

		sNewOperand			= *psOrgOperand;
		sNewOperand.eType	= eNewResultRegType;
		sNewOperand.uNum	= (psOrgOperand->uNum - uOrgResultRegNum) +
							   uNewResultRegNum;

		if	(!HWInstEncodeSrc1BankAndNum(eFmtCtl, eOpcode, psHWInst, &sNewOperand))
		{
			USP_DBGPRINT(("USPResultRefChangeRegLocation: Error encoding new src1 operand\n"));
			return IMG_FALSE;
		}
	}

	if	(uInstDescFlags & USP_INSTDESC_FLAG_SRC2_USES_RESULT)
	{
		/*
			Modify the source 2 operand to use the new result-location
		*/
		psOrgOperand = &psResultRef->auOrgHWOperands[USP_OPERANDIDX_SRC2];

		sNewOperand			= *psOrgOperand;
		sNewOperand.eType	= eNewResultRegType;
		sNewOperand.uNum	= (psOrgOperand->uNum - uOrgResultRegNum) +
							   uNewResultRegNum;

		if	(!HWInstEncodeSrc2BankAndNum(eFmtCtl, eOpcode, psHWInst, &sNewOperand))
		{
			USP_DBGPRINT(("USPResultRefChangeRegLocation: Error encoding new src2 operand\n"));
			return IMG_FALSE;
		}
	}

	#if !defined(SGX_FEATURE_USE_VEC34)
	/*
		Detect and remove a redundant move created by remapping the 
		result-references

		NB: MOVCs perfoming a test are not removed, neither are those with a
			repat count/mask (the MOE state may make them not-redundant, even
			though the dest and source operands are the same).
	*/
	if	(eOpcode == USP_OPCODE_MOVC)
	{
		USP_MOVC_TESTDTYPE	eTestType;

		/*
			Only remove moves that aren't performing a test
		*/
		if	(!HWInstDecodeMOVInstTestDataType(psHWInst, &eTestType))
		{
			USP_DBGPRINT(("USPResultRefChangeRegLocation: Error decoding MOVC test data-type\n"));
			return IMG_FALSE;
		}

		if	(eTestType == USP_MOVC_TESTDTYPE_NONE)
		{
			USP_REPEAT_MODE	eMode;
			IMG_UINT32		uRepeat;

			/*
				Only remove moves that are not repeated
			*/
			if	(!HWInstDecodeRepeat(psHWInst, eOpcode, &eMode, &uRepeat))
			{
				USP_DBGPRINT(("USPResultRefChangeRegLocation: Error decoding inst repeat\n"));
				return IMG_FALSE;
			}

			if	((eMode == USP_REPEAT_MODE_REPEAT) && (uRepeat == 0))
			{
				USP_REG	sDestReg;
				USP_REG	sSrcReg;

				/*
					This move is redundant if the source and dest operands match...
				*/
				if	(!HWInstDecodeDestBankAndNum(eFmtCtl, eOpcode, psHWInst, &sDestReg))
				{
					USP_DBGPRINT(("USPResultRefChangeRegLocation: Error decoding new dest operand\n"));
					return IMG_FALSE;
				}
				if	(!HWInstDecodeSrc1BankAndNum(eFmtCtl, psHWInst, &sSrcReg))
				{
					USP_DBGPRINT(("USPResultRefChangeRegLocation: Error decoding new dest operand\n"));
					return IMG_FALSE;
				}

				if	(
						(sDestReg.uNum		== sSrcReg.uNum)	&&
						(sDestReg.eType		== sSrcReg.eType)	&&
						(sDestReg.eDynIdx	== sSrcReg.eDynIdx)
					)
				{
					PUSP_INST		psInst;
					PUSP_INSTBLOCK	psInstBlock;

					psInst		= psResultRef->psInst;
					psInstBlock	= psInst->psInstBlock;

					if	(!USPInstBlockRemoveInst(psInstBlock, psInst))
					{
						USP_DBGPRINT(("USPResultRefChangeRegLocation: Error removing redundant MOV from block\n"));
						return IMG_FALSE;
					}
				}
			}
		}
	}
	#endif /* !defined(SGX_FEATURE_USE_VEC34) */

	return IMG_TRUE;
}

/******************************************************************************
 End of file (USP_RESULTREF.C)
******************************************************************************/

