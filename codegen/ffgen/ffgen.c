/******************************************************************************
 * Name         : ffgen.c
 * Author       : James McCarthy
 * Created      : 07/11/2005
 *
 * Copyright    : 2000-2008 by Imagination Technologies Limited.
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
 * Modifications:-
 * $Log: ffgen.c $
 *****************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "apidefs.h"
#include "codegen.h"
#if defined(OGLES1_MODULE) && defined(FFGEN_UNIFLEX)
#include "inst.h"
#endif /* defined(OGLES1_MODULE) && defined(FFGEN_UNIFLEX) */
#if !defined(OGLES1_MODULE)
#include "fftousc.h"
#endif /* !defined(OGLES1_MODULE) */
#include "reg.h"
#include "source.h"
#if defined(OGL_LINESTIPPLE)
#include "geogen.h"
#endif
#if STANDALONE
IMG_BOOL gbUseDescriptions = IMG_FALSE;
#endif

#if (STANDALONE || defined(DEBUG))

/******************************************************************************
 * Function Name: FFGenDumpDisassembly
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : 
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL
IMG_VOID IMG_CALLCONV FFGenDumpDisassembly(FFGenCode	*psFFGenCode,
										   IMG_BOOL		bDumpSource,
										   IMG_CHAR		bNewFile,
										   IMG_CHAR		*pszFileName,
										   IMG_CHAR		*pszComment)
{
	IMG_UINT32  uInstCount     = psFFGenCode->uNumHWInstructions;
	IMG_UINT32 *puInstructions = psFFGenCode->puHWCode;
	IMG_UINT32  i;
	IMG_UINT32  uInstCountDigits;
	SGX_CORE_INFO sTarget;
	PCSGX_CORE_DESC psTargetDesc;
	FILE *psAsmFile;

	if (!pszFileName)
	{
		return;
	}

	if (bNewFile)
	{
		psAsmFile = fopen(pszFileName,"w+t");
	}
	else
	{
		psAsmFile = fopen(pszFileName,"a+t");
	}

	if (!psAsmFile)
	{
		psFFGenCode->psFFGenContext->pfnPrint("DumpDisassembly: Failed to open file\n");
		return;
	}

	if (pszComment)
	{
		fprintf(psAsmFile, "%s", pszComment);
	}

	if (bDumpSource)
	{
		/* Dump the source code */
		DumpSource(psFFGenCode, psAsmFile);

		fprintf(psAsmFile, "\n#if 0\n\n/* Dissasembled source code */\n\n");
	}

	for (i = 10, uInstCountDigits = 1; i < uInstCount; i *= 10, uInstCountDigits++);

	sTarget.eID = SGX_CORE_ID;
#if defined (SGX_CORE_REV)
	sTarget.uiRev = SGX_CORE_REV;
#else
	sTarget.uiRev = 0;		/* use head revision */
#endif

	psTargetDesc = UseAsmGetCoreDesc(&sTarget);
	for (i = 0; i < uInstCount; i++, puInstructions+=2)
	{
		IMG_UINT32 uInst0 = puInstructions[0];
		IMG_UINT32 uInst1 = puInstructions[1];
		IMG_CHAR pszInst[256];

		fprintf(psAsmFile,"%*u: 0x%.8X%.8X\t", uInstCountDigits, i, uInst1, uInst0);
		UseDisassembleInstruction(psTargetDesc, uInst0, uInst1, pszInst);
		fprintf(psAsmFile,"%s\n", pszInst);
	}

	if (bDumpSource)
	{
		fprintf(psAsmFile, "\n#endif\n");
	}

	fclose(psAsmFile);

}


#endif


/* Stolen from statehash.c */
#define HASH_VALUE(a, b) a += b; a += (a << 10); a ^= (a >> 6)
#define END_HASH(a) a += (a << 3); a ^= (a >> 11); a += (a << 15)


IMG_INTERNAL const IMG_UINT32 auLightSourceMemberSizes[FFTNL_NUM_MEMBERS_LIGHTSOURCE]             = {4, 4, 4, 4, 4, 4, 3, 1, 1, 1, 1, 1, 1};
IMG_INTERNAL const IMG_UINT32 auPointParamsMemberSizes[FFTNL_NUM_MEMBERS_POINTPARAMS]             = {1, 1, 1, 1, 1, 1, 1};
IMG_INTERNAL const IMG_UINT32 auMaterialMemberSizes[FFTNL_NUM_MEMBERS_MATERIAL]                   = {4, 4, 4, 4, 1};
IMG_INTERNAL const IMG_UINT32 auLightProductMemberSizes[FFTNL_NUM_MEMBERS_LIGHTPRODUCT]           = {4, 4, 4};
IMG_INTERNAL const IMG_UINT32 auLightModelMemberSizes[FFTNL_NUM_MEMBERS_LIGHTMODEL]               = {4};
IMG_INTERNAL const IMG_UINT32 auLightModelProductMemberSizes[FFTNL_NUM_MEMBERS_LIGHTMODELPRODUCT] = {4};


#if !defined(OGLES1_MODULE)
/******************************************************************************
 * Function Name: FFTNLHashProgramDesc
 * Inputs       : psFFTNLGenDesc
 * Outputs      : -
 * Returns      : IMG_UINT32
 * Globals Used : 
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_UINT32 FFTNLHashProgramDesc(FFTNLGenDesc *psFFTNLGenDesc)
{
	IMG_UINT32 uHashValue = 0, i;
	IMG_UINT32 *puHashData = (IMG_UINT32 *)psFFTNLGenDesc;
	IMG_UINT32 uSizeInDWords = sizeof(FFTNLGenDesc) / sizeof(IMG_UINT32);

	for (i = 0; i < uSizeInDWords; i++)
	{
		HASH_VALUE(uHashValue, puHashData[i]);
	}

	END_HASH(uHashValue); 

	return uHashValue;
}

#else /* !defined(OGLES1_MODULE) */


#if !defined(FFGEN_UNIFLEX)

/***********************************************************************************
 Function Name      : ConvertFFGENProgram
 Inputs             : psVPGenCode
 Outputs            : -
 Returns            : psFFGENProgramDetails
 Description        : 
************************************************************************************/
static FFGEN_PROGRAM_DETAILS *ConvertFFGENProgram(FFGenCode *psFFGenCode)
{
	FFGEN_PROGRAM_DETAILS 	   *psFFGENProgramDetails;
	IMG_UINT32			        ui32NumSecAttribs, ui32NumMemConsts;
	FFGenRegList               *psRegList;

	psFFGENProgramDetails = FFGENCalloc(psFFGenCode->psFFGenContext, sizeof(FFGEN_PROGRAM_DETAILS));

	if(!psFFGENProgramDetails)
	{
		return IMG_NULL;
	}

	/* Setup compiled code */
	psFFGENProgramDetails->pui32Instructions            = psFFGenCode->puHWCode;
	psFFGENProgramDetails->ui32InstructionCount         = psFFGenCode->uNumHWInstructions;
	psFFGENProgramDetails->ui32PrimaryAttributeCount    = psFFGenCode->uInputSize;
	psFFGENProgramDetails->ui32TemporaryRegisterCount   = psFFGenCode->uTempSize;
	psFFGENProgramDetails->iSAAddressAdjust				= psFFGenCode->iSAConstBaseAddressAdjust;


	if (psFFGenCode->eCodeGenFlags & FFGENCGF_DYNAMIC_FLOW_CONTROL)
	{
		psFFGENProgramDetails->bUSEPerInstanceMode = IMG_TRUE;
	}

	ui32NumSecAttribs = 0;
	ui32NumMemConsts = 0;

	psRegList = psFFGenCode->psConstantsList;

	/* Calculate num of sec attribs and num of mem constants, - this needs to optimise */
	while (psRegList)
	{
		FFGenReg *psReg = psRegList->psReg;
		
		if (psReg->eType == USEASM_REGTYPE_SECATTR)
		{
			ui32NumSecAttribs += psReg->uSizeInDWords;
		}
		/* Is a constant in memory */
		else
		{	
			ui32NumMemConsts += psReg->uSizeInDWords;
		}
		
		psRegList = psRegList->psNext;
	}

	psFFGENProgramDetails->ui32SecondaryAttributeCount = ui32NumSecAttribs;
	psFFGENProgramDetails->ui32MemoryConstantCount     = ui32NumMemConsts;

	return psFFGENProgramDetails;
}

#endif /* !defined(FFGEN_UNIFLEX) */

#endif /* !defined(OGLES1_MODULE) */
											  

#if defined(OGLES1_MODULE) && defined(FFGEN_UNIFLEX)

/******************************************************************************
 * Function Name: FFGenGenerateTNLProgram
 * Inputs       : pvFFGenContext, psFFTNLGenDesc
 * Outputs      : -
 * Returns      : psFFGenProgram
 * Globals Used : -
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL 
FFGenProgram* IMG_CALLCONV FFGenGenerateTNLProgram(IMG_VOID		*pvFFGenContext,
												   FFTNLGenDesc	*psFFTNLGenDesc)
{
	FFGenContext *psFFGenContext = (FFGenContext *)pvFFGenContext;
	FFGEN_PROGRAM_DETAILS *psFFGENProgramDetails = IMG_NULL;
	FFGenProgram *psFFGenProgram;
	FFGenRegList *psRegList;
	FFGenCode    *psCode;
	IMG_UINT32 i;

	/* Generate the code */
	psCode = FFTNLGenerateCode(psFFGenContext, psFFTNLGenDesc, IMG_FALSE);

	psFFGenProgram = FFGENMalloc(psFFGenContext, sizeof(FFGenProgram));

	if(!psFFGenProgram)
	{
		psCode->psFFGenContext->pfnPrint("FFGenGenerateTNLProgram: Failed to allocate memory for FFGenProgram\n");

		goto failed;
	}

	psFFGenProgram->psUniFlexInstructions = psCode->psUniFlexInstructions;


	psFFGENProgramDetails = FFGENCalloc(psCode->psFFGenContext, sizeof(FFGEN_PROGRAM_DETAILS));

	if(!psFFGENProgramDetails)
	{
		psCode->psFFGenContext->pfnPrint("FFGenGenerateTNLProgram: Failed to allocate memory for FFGENProgramDetails\n");

		goto failed;
	}

#if defined(OGL_LINESTIPPLE)
	psFFGenProgram->psGeoExtra = IMG_NULL;
#endif

	/* Copy required information to program */ 
	psFFGenProgram->psConstantsList	    = psCode->psConstantsList;
	psFFGenProgram->psInputsList		= psCode->psInputsList;
	psFFGenProgram->psOutputsList	    = psCode->psOutputsList;

	psFFGenProgram->uNumTexCoordUnits	= psCode->uNumTexCoordUnits;

	for(i=0; i<psFFGenProgram->uNumTexCoordUnits; i++)
	{
		psFFGenProgram->auOutputTexDimensions[i] = psCode->auOutputTexDimensions[i];
	}

	psFFGenProgram->uMemoryConstantsSize	= psCode->uMemoryConstantsSize;
	psFFGenProgram->uMemConstBaseAddrSAReg	= 0;
	psFFGenProgram->uSecAttribStart			= psCode->uSecAttribStart;
	psFFGenProgram->uSecAttribSize			= psCode->uSecAttribSize;

	psFFGenProgram->psUniFlexInst = psCode->psUniFlexInstructions;

	psRegList = psFFGenProgram->psConstantsList;

	while (psRegList)
	{
		FFGenReg *psReg = psRegList->psReg;

		psReg->pui32SrcOffset = FFGENMalloc(psCode->psFFGenContext, sizeof(IMG_UINT32)*psReg->uSizeInDWords);

		if(!psReg->pui32SrcOffset)
		{
			PVR_DPF((PVR_DBG_ERROR,"FFGenGenerateTNLProgram: Malloc failed"));

			return IMG_NULL;
		}

		psReg->pui32DstOffset = FFGENMalloc(psCode->psFFGenContext, sizeof(IMG_UINT32)*psReg->uSizeInDWords);

		if(!psReg->pui32DstOffset)
		{
			PVR_DPF((PVR_DBG_ERROR,"FFGenGenerateTNLProgram: Malloc failed"));

			return IMG_NULL;
		}

		psRegList = psRegList->psNext;
	}

	psFFGenProgram->psFFGENProgramDetails = psFFGENProgramDetails;

	/* Destroy the code */
	FFGENDestroyCode(psCode);

#if defined(DEBUG)
	psFFGenContext->uProgramCount++;
#endif

	/* Return program */
	return psFFGenProgram;

failed:

	if(psFFGENProgramDetails)
	{
		FFGENFree(psCode->psFFGenContext, psFFGENProgramDetails);
	}

	if(psFFGenProgram)
	{
		FFGENFree(psCode->psFFGenContext, psFFGenProgram);
	}

	psRegList = psCode->psConstantsList;

	while (psRegList)
	{
		FFGenReg *psReg = psRegList->psReg;

		if(psReg->pui32SrcOffset)
		{
			FFGENFree(psCode->psFFGenContext, psReg->pui32SrcOffset);
		}

		if(psReg->pui32DstOffset)
		{
			FFGENFree(psCode->psFFGenContext, psReg->pui32DstOffset);
		}

		psRegList = psRegList->psNext;
	}

	/* Destroy the code */
	FFGENDestroyCode(psCode);

	return IMG_NULL;
}	


#else /* defined(OGLES1_MODULE) && defined(FFGEN_UNIFLEX) */

	
/******************************************************************************
 * Function Name: FFGenGenerateTNLProgram
 * Inputs       : pvFFGenContext, psFFTNLGenDesc
 * Outputs      : -
 * Returns      : psFFGenProgram
 * Globals Used : -
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL
FFGenProgram* IMG_CALLCONV FFGenGenerateTNLProgram(IMG_VOID		*pvFFGenContext,
												   FFTNLGenDesc	*psFFTNLGenDesc)
{
	/* Supply values for global vars in non standalone builds */
#ifndef STANDALONE
	IMG_BOOL     gbUseDescriptions = IMG_FALSE;
#endif
	FFGenProgram *psFFGenProgram	= IMG_NULL;
	FFGenCode    *psCode    = IMG_NULL;
	FFGenContext *psFFGenContext = (FFGenContext *)pvFFGenContext;
	IMG_UINT32 i;

#if !defined(OGLES1_MODULE)
	/* Create a hash value for this description */
	IMG_UINT32 uHashValue =  FFTNLHashProgramDesc(psFFTNLGenDesc);
#endif /* !defined(OGLES1_MODULE) */

	/* Generate the code */
	psCode  = FFTNLGenerateCode(psFFGenContext, psFFTNLGenDesc, gbUseDescriptions);

	/* Create a new entry */
	psFFGenProgram = FFGENMalloc(psFFGenContext, sizeof(FFGenProgram));

#ifdef FFGEN_UNIFLEX
	/* Send the linked list of UniFlex instructions to the FFGen program from the FFTNL code. */
	psFFGenProgram->psUniFlexInstructions = psCode->psUniFlexInstructions;
#endif	
#if defined(OGLES1_MODULE)
	psFFGenProgram->psFFGENProgramDetails = ConvertFFGENProgram(psCode);
#else

#ifdef FFGEN_UNIFLEX
	/* No need to convert to hw code if we generating uniflex input */
	psFFGenProgram->psUniFlexHW = IMG_NULL;
	if( !psCode->psUniFlexInstructions )
#endif
	/* Convert to uniflex HW */
	psFFGenProgram->psUniFlexHW = ConvertFFTNLToUniFlexHW(psCode);

#endif /* defined(OGLES1_MODULE) */

#if defined(OGL_LINESTIPPLE)
	psFFGenProgram->psGeoExtra = IMG_NULL;
#endif

	/* Copy required information to program */ 
	psFFGenProgram->psConstantsList	    = psCode->psConstantsList;
	psFFGenProgram->psInputsList		= psCode->psInputsList;
	psFFGenProgram->psOutputsList	    = psCode->psOutputsList;

	psFFGenProgram->uNumTexCoordUnits	= psCode->uNumTexCoordUnits;

	for(i = 0; i < psFFGenProgram->uNumTexCoordUnits; i++)
	{
		psFFGenProgram->auOutputTexDimensions[i] = psCode->auOutputTexDimensions[i];
	}

	psFFGenProgram->uMemoryConstantsSize	= psCode->uMemoryConstantsSize;
	psFFGenProgram->uMemConstBaseAddrSAReg	= psFFTNLGenDesc->uSecAttrConstBaseAddressReg;
	psFFGenProgram->uSecAttribStart			= psCode->uSecAttribStart;
	psFFGenProgram->uSecAttribSize			= psCode->uSecAttribSize;

#if !defined(OGLES1_MODULE)
	/* Setup entry */
	psFFGenProgram->uHashValue	= uHashValue;
	psFFGenProgram->uRefCount	= 1;
	psFFGenProgram->uID			= 0;
#endif /* !defined(OGLES1_MODULE) */

	#if (STANDALONE || defined(DEBUG))
	if (psFFGenContext->pszDisassemblyFileName)
	{
		IMG_CHAR acComment[200];

		/* Dump the desciption */
		DumpFFTNLDescription(pvFFGenContext, psFFTNLGenDesc, IMG_FALSE, psFFGenContext->pszDisassemblyFileName);

		//sprintf(acComment, "/* Hash value = %08lX (Program number %lu)*/\n", uHashValue, uNumSearches);

		acComment[0] = 0;

		/* Dump the source */
		FFGenDumpDisassembly(psCode, IMG_TRUE, IMG_FALSE, psFFGenContext->pszDisassemblyFileName, acComment);
	}
	#endif /* STANDALONE || DEBUG */

	/* Destroy the code */
	FFGENDestroyCode(psCode);

#if defined(DEBUG)
	psFFGenContext->uProgramCount++;
#endif

	/* Return program */
	return psFFGenProgram;
}

#endif /* defined(OGLES1_MODULE) && defined(FFGEN_UNIFLEX) */


#if !defined(OGLES1_MODULE)
/******************************************************************************
 * Function Name: FFGenGetTNLProgram
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL
FFGenProgram* IMG_CALLCONV FFGenGetTNLProgram(IMG_VOID			*pvFFGenContext,
											  FFTNLGenDesc		*psFFTNLGenDesc,
											  FFGenProgDesc		*peProgDesc)
{
	FFGenContext	*psFFGenContext = (FFGenContext *)pvFFGenContext;

	FFGenProgram *psProgramList  = psFFGenContext->psFFTNLProgramList;
	FFGenProgram *psProgramEntry = IMG_NULL;
	IMG_UINT32    uNumSearches   = 0;
	FFGenProgDesc eProgDesc      = FFGENPD_NEW;

	/* Create a hash value for this description */
	IMG_UINT32 uHashValue =  FFTNLHashProgramDesc(psFFTNLGenDesc);

	/* Search through existing programs for one that matches this value */
	while (psProgramList && !psProgramEntry)
	{
		if (uHashValue == psProgramList->uHashValue)
		{
			/* Remove this entry */
			if (psProgramList->psNext)
			{
				psProgramList->psNext->psPrev =  psProgramList->psPrev;
			}
			if (psProgramList->psPrev)
			{
				psProgramList->psPrev->psNext = psProgramList->psNext;
			}
			if (psProgramList == psFFGenContext->psFFTNLProgramList)
			{
				psFFGenContext->psFFTNLProgramList = psProgramList->psNext;
			}

			/* Was it the first entry */
			if (psProgramList == psFFGenContext->psFFTNLProgramList)
			{
				eProgDesc = FFGENPD_CURRENT;
			}
			/* No, must have been existing entry */
			else
			{
				eProgDesc = FFGENPD_EXISTING;
			}

			/* return this program */
			psProgramEntry = psProgramList;

			/* Increment the reference count of the program */
			psProgramEntry->uRefCount++;
		}

		psProgramList = psProgramList->psNext;

		uNumSearches++;
	}

	if (!psProgramEntry)
	{
		/* Generate program */
		psProgramEntry = FFGenGenerateTNLProgram(pvFFGenContext, psFFTNLGenDesc);

		/* Setup entry */
		psProgramEntry->uHashValue          = uHashValue;
		psProgramEntry->uRefCount           = 1;
		psProgramEntry->uID                 = uNumSearches;

		//psProgramEntry->psFFGenDesc			= psCode->psFFTNLGenDesc;

		/* Indicate it was a new entry */
		eProgDesc = FFGENPD_NEW;
	}

	/* Add to top of list */
	if (psFFGenContext->psFFTNLProgramList)
	{
		psFFGenContext->psFFTNLProgramList->psPrev = psProgramEntry;
	}

	psProgramEntry->psNext               = psFFGenContext->psFFTNLProgramList;
	psProgramEntry->psPrev               = IMG_NULL;
	psFFGenContext->psFFTNLProgramList   = psProgramEntry;

	if (peProgDesc)
	{
		*peProgDesc = eProgDesc;
	}
	
	/* Return program */
	return psProgramEntry;
}
#endif /* !defined(OGLES1_MODULE) */

#if defined(OGL_LINESTIPPLE)

/******************************************************************************
 * Function Name: FFGenGenerateGEOProgram
 * Inputs       : pvFFGenContext, psFFGEOGenDesc
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL
FFGenProgram* IMG_CALLCONV FFGenGenerateGEOProgram(IMG_VOID		*pvFFGenContext,
												   FFGEOGenDesc	*psFFGEOGenDesc)
{
	/* Supply values for global vars in non standalone builds */
#ifndef STANDALONE
	IMG_BOOL		gbUseDescriptions	= IMG_FALSE;
#endif

	FFGenProgram	*psFFGenProgram		= IMG_NULL;
	FFGenCode		*psCode				= IMG_NULL;

	FFGenContext	*psFFGenContext		= (FFGenContext *)pvFFGenContext;

	/* Generate the code */
	psCode = FFGEOGenerateCode(psFFGenContext, psFFGEOGenDesc, gbUseDescriptions);

	/* Create a new entry */
	psFFGenProgram = FFGENMalloc(psFFGenContext, sizeof(FFGenProgram));

#if !defined(OGLES1_MODULE)
	/* Convert to uniflex HW */
	psFFGenProgram->psUniFlexHW = IMG_NULL; 
#else
	psFFGenProgram->psFFGENProgramDetails = IMG_NULL;
#endif

	/* Copy required information to program */ 
	psFFGenProgram->psConstantsList	    = psCode->psConstantsList;
	psFFGenProgram->psInputsList		= psCode->psInputsList;
	psFFGenProgram->psOutputsList	    = psCode->psOutputsList;

	psFFGenProgram->uMemoryConstantsSize	= psCode->uMemoryConstantsSize;
	psFFGenProgram->uMemConstBaseAddrSAReg  = psFFGEOGenDesc->uSecAttrConstBaseAddressReg;
	psFFGenProgram->uSecAttribSize			= psCode->uSecAttribSize;
	psFFGenProgram->uSecAttribStart			= psCode->uSecAttribStart;

#ifdef FFGEN_UNIFLEX
	/* Geometry shaders do not have Uniflex instructions */
	psFFGenProgram->psUniFlexInstructions = IMG_NULL;
#endif

	psFFGenProgram->psGeoExtra = FFGENMalloc(psFFGenContext, sizeof(FFGenGeoExtra));

	if(psFFGenProgram->psGeoExtra == IMG_NULL)
	{
		return IMG_NULL;
	}

	psFFGenProgram->psGeoExtra->iSAAddressAdjust			= psCode->iSAConstBaseAddressAdjust;
	psFFGenProgram->psGeoExtra->uOutVertexStride			= psCode->uOutVertexStride;
	psFFGenProgram->psGeoExtra->uNumPartitionsRequired		= psCode->uNumPartitionsRequired;

	psFFGenProgram->psGeoExtra->uProgramStart				= psCode->uProgramStart;
	psFFGenProgram->psGeoExtra->uTotalNumInstructions		= psCode->uNumHWInstructions;
	psFFGenProgram->psGeoExtra->uNumTemporaryRegisters		= psCode->uTempSize;
	psFFGenProgram->psGeoExtra->puUSECode					= psCode->puHWCode;

	/* Destroy the code */
	FFGENDestroyCode(psCode);

#if defined(DEBUG)
	psFFGenContext->uProgramCount++;
#endif
	/* Return program */
	return psFFGenProgram;
}
#endif

/******************************************************************************
 * Function Name: FFGenFreeProgram
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL
IMG_VOID IMG_CALLCONV FFGenFreeProgram(IMG_VOID		*pvFFGenContext,
									   FFGenProgram	*psFFGenProgram)
{
	FFGenContext *psFFGenContext = (FFGenContext *)pvFFGenContext;

	/* Free register lists */
	DestroyRegList(psFFGenContext, psFFGenProgram->psConstantsList, IMG_TRUE);
	DestroyRegList(psFFGenContext, psFFGenProgram->psInputsList,    IMG_TRUE);
	DestroyRegList(psFFGenContext, psFFGenProgram->psOutputsList,   IMG_TRUE);

#if !defined(OGLES1_MODULE)
	
	#if defined(FFGEN_UNIFLEX)
	if(psFFGenProgram->psUniFlexInstructions)
	{
		UNIFLEX_INST* psUniInst = psFFGenProgram->psUniFlexInstructions;
		
		/* Destroy the uniflex instructions.. */
		while(psUniInst)
		{
			UNIFLEX_INST* psNext = psUniInst->psILink;
			
			FFGENFree(psFFGenContext, psUniInst);
			
			psUniInst = psNext;
		}
	}
	#endif

	/* Free UniFlex HW */
#if defined(OGL_LINESTIPPLE)
	if(psFFGenProgram->psGeoExtra)
	{
		FFGENFree(psFFGenContext, psFFGenProgram->psGeoExtra->puUSECode);

		FFGENFree(psFFGenContext, psFFGenProgram->psGeoExtra);
	}
	else
#endif
	{
		if(psFFGenProgram->psUniFlexHW)
		{
			FreeFFTNLUniFlexHW(psFFGenContext, psFFGenProgram->psUniFlexHW);
		}
	}

#else /* !defined(OGLES1_MODULE) */


	if(psFFGenProgram->psFFGENProgramDetails)
	{
		if(psFFGenProgram->psFFGENProgramDetails->pui32Instructions)
		{
			FFGENFree(psFFGenContext, psFFGenProgram->psFFGENProgramDetails->pui32Instructions);
		}

		FFGENFree(psFFGenContext, psFFGenProgram->psFFGENProgramDetails);
	}

#endif /* !defined(OGLES1_MODULE) */


	/* Finally free the program structure */
	FFGENFree(psFFGenContext, psFFGenProgram);
}



/******************************************************************************
 * Function Name: FFTNLDestroyEntry
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 * Description  : 
 *****************************************************************************/
static IMG_VOID FFTNLDestroyEntry(FFGenContext *psFFGenContext, FFGenProgram *psProgramEntry)
{
	/* Remove from linked list */
	if (psProgramEntry->psNext)
	{
		psProgramEntry->psNext->psPrev = psProgramEntry->psPrev;
	}

	if (psProgramEntry->psPrev)
	{
		psProgramEntry->psPrev->psNext = psProgramEntry->psNext;
	}

	if (psFFGenContext->psFFTNLProgramList == psProgramEntry)
	{
		psFFGenContext->psFFTNLProgramList = psProgramEntry->psNext;
	}

	/* Free the actual program */
	FFGenFreeProgram(psFFGenContext, psProgramEntry);
}


/******************************************************************************
 * Function Name: FFGenCreateContext
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL
IMG_VOID* IMG_CALLCONV FFGenCreateContext(IMG_HANDLE		hClientHandle,
										  const IMG_CHAR	*pszDumpFileName,
										  FFGEN_MALLOCFN	pfnMalloc,
										  FFGEN_CALLOCFN	pfnCalloc,
										  FFGEN_REALLOCFN	pfnRealloc,
										  FFGEN_FREEFN		pfnFree,
										  FFGEN_PRINTFN		pfnPrint)
{
	/* Alloc mem and set evrything to 0 */
	FFGenContext *psFFGenContext = pfnCalloc(hClientHandle, sizeof(FFGenContext));

	if (psFFGenContext)
	{
		if (pszDumpFileName)
		{
			/* Reset the file */
			FILE *psAsmFile = fopen(pszDumpFileName,"w+");

			if (!psAsmFile)
			{
				pfnPrint("DumpDisassembly: Failed to open file\n");

				return 0;
			}

			fprintf(psAsmFile, "/* Runtime code log */\n");

			fclose(psAsmFile);

			/* Alloc mem for the file name */
			psFFGenContext->pszDisassemblyFileName = pfnMalloc(hClientHandle, ((IMG_UINT32)(strlen(pszDumpFileName) + 1)));

			/* Copy the name */
			strcpy(psFFGenContext->pszDisassemblyFileName, pszDumpFileName);
		}
		else
		{
			psFFGenContext->pszDisassemblyFileName = NULL;
		}

		psFFGenContext->hClientHandle	= hClientHandle;
		psFFGenContext->pfnMalloc		= pfnMalloc;
		psFFGenContext->pfnCalloc		= pfnCalloc;
		psFFGenContext->pfnRealloc		= pfnRealloc;
		psFFGenContext->pfnFree			= pfnFree;
		psFFGenContext->pfnPrint		= pfnPrint;
	}

	return (IMG_VOID *) psFFGenContext;	/* NULL on allocation failure */
}


/******************************************************************************
 * Function Name: 
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_VOID IMG_CALLCONV FFGenDestroyContext(IMG_VOID *pvFFGenContext)
{
	FFGenContext *psFFGenContext = (FFGenContext *)pvFFGenContext;

	FFGenProgram *psProgramEntry = psFFGenContext->psFFTNLProgramList;

	/* Go through list freeing all programs */
	while (psProgramEntry)
	{
		FFGenProgram *psNext = psProgramEntry->psNext;

		FFTNLDestroyEntry(psFFGenContext, psProgramEntry);

		psProgramEntry = psNext;
	}
	
	if (psFFGenContext->pszDisassemblyFileName)
	{
		FFGENFree(psFFGenContext,psFFGenContext->pszDisassemblyFileName); 
	}

	FFGENFree(psFFGenContext, psFFGenContext);
}


#if STANDALONE 

IMG_VOID* IMG_CALLCONV TMalloc(IMG_HANDLE hClientHandle, IMG_UINT32 uSize)
{
	hClientHandle;
	return DebugMemAlloc(uSize); 
}

IMG_VOID* IMG_CALLCONV TCalloc(IMG_HANDLE hClientHandle, IMG_UINT32 uSize)
{
	hClientHandle;
	return DebugMemCalloc(uSize);
}

IMG_VOID* IMG_CALLCONV TRealloc(IMG_HANDLE hClientHandle, IMG_VOID *pvData, IMG_UINT32 uSize)
{
	hClientHandle;
	return DebugMemRealloc(pvData, uSize);
}

IMG_VOID IMG_CALLCONV TFree(IMG_HANDLE hClientHandle, IMG_VOID *pvData)
{
	hClientHandle;
	DebugMemFree(pvData);
}

/******************************************************************************
 * Function Name: CreateProgram
 * Inputs       : psFFTNLGenDesc, pszAsmFileName
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 * Description  : 
 *****************************************************************************/
static IMG_VOID CreateProgram(FFTNLGenDesc *psFFTNLGenDesc, const IMG_CHAR *pszAsmFileName)
{
	IMG_VOID *pvContext;
	IMG_HANDLE hClientHandle = (IMG_HANDLE)1;

	FFGenProgram *psFFGenProgram;

	FFGenProgDesc eProgDesc;

	/* Create the context */
	pvContext = FFGenCreateContext(hClientHandle,
								   pszAsmFileName,
								   TMalloc,
								   TCalloc,
								   TRealloc,
								   TFree,
								   DebugToolsMessage);

	/* Generate the code */ 
	psFFGenProgram = FFGenGetTNLProgram(pvContext,
										psFFTNLGenDesc,
										&eProgDesc);

	FFGenDestroyContext(pvContext);

}

#define USE_FFTNL_GENDESC_DUMP 1

/******************************************************************************
 * Function Name: main
 * Inputs       : -
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 * Description  : 
 *****************************************************************************/
IMG_VOID main()
{
#if USE_FFTNL_GENDESC_DUMP
	FFTNLGenDesc sFFTNLGenDesc;

	/* This data copy from ffshaders.txt */
	IMG_UINT32 auDescData[] = {
		0x00000C08, 0x00000004, 0x00000000, 0x00000002, 0x00000080, 0x00000001, 0x00000001, 0x00000002,
		0x00000000, 0x00000000, 0x00000010, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	};

	memcpy(&sFFTNLGenDesc, auDescData, sizeof(FFTNLGenDesc));
#else

	FFTNLGenDesc  sFFTNLGenDesc;
	memset(&sFFTNLGenDesc, 0, sizeof(FFTNLGenDesc));

	sFFTNLGenDesc.eFFTNLEnables1 |= (FFTNL_ENABLES1_MATRIXPALETTEBLENDING |
									FFTNL_ENABLES1_TRANSFORMNORMALS       |
									FFTNL_ENABLES1_NORMALISENORMALS       |
									FFTNL_ENABLES1_EYEPOSITION            |
									FFTNL_ENABLES1_VERTEXTOEYEVECTOR      |
									FFTNL_ENABLES1_CLIPPING               |
									FFTNL_ENABLES1_SECONDARYCOLOUR        |
									FFTNL_ENABLES1_POINTATTEN             |
									FFTNL_ENABLES1_FOGCOORDEYEPOS         |
									FFTNL_ENABLES1_FOGEXP2                |
									FFTNL_ENABLES1_TEXTURINGENABLED       |
									FFTNL_ENABLES1_TEXGENSPHEREMAP        |
									FFTNL_ENABLES1_REFLECTIONMAP          |
									FFTNL_ENABLES1_LIGHTINGENABLED        |
									FFTNL_ENABLES1_LIGHTINGTWOSIDED       |
									FFTNL_ENABLES1_LOCALVIEWER            |
									FFTNL_ENABLES1_VERTCOLOURDIFFUSE);

	sFFTNLGenDesc.eFFTNLEnables2  = FFTNL_ENABLES2_POINTS | FFTNL_ENABLES2_LINEATTENUATION;

	sFFTNLGenDesc.uEnabledClipPlanes           = 0x5;
	sFFTNLGenDesc.uNumBlendUnits               = 4;
	sFFTNLGenDesc.uEnabledPassthroughCoords    = 0x5;
	sFFTNLGenDesc.aubPassthroughCoordMask[0]   = 0x3;
	sFFTNLGenDesc.aubPassthroughCoordMask[2]   = 0xD;
	sFFTNLGenDesc.uEnabledEyeLinearCoords      = 0xC;
	sFFTNLGenDesc.aubEyeLinearCoordMask[2]     = 0x2;
	sFFTNLGenDesc.aubEyeLinearCoordMask[3]     = 0xF;
	sFFTNLGenDesc.uEnabledObjLinearCoords      = 0x10;
	sFFTNLGenDesc.aubObjLinearCoordMask[4]     = 0x6;
	sFFTNLGenDesc.uEnabledSphereMapCoords      = 0x10;
	sFFTNLGenDesc.aubSphereMapCoordMask[4]     = 0x9;
	sFFTNLGenDesc.uEnabledNormalMapCoords      = 0x20;
	sFFTNLGenDesc.aubNormalMapCoordMask[5]     = 0x7;
	sFFTNLGenDesc.uEnabledReflectionMapCoords  = 0x40;
	sFFTNLGenDesc.aubReflectionMapCoordMask[6] = 0x3;
	sFFTNLGenDesc.uEnabledTextureMatrices      = 0x5;
	sFFTNLGenDesc.uEnabledSpotLocalLights      = 0x1;
	sFFTNLGenDesc.uEnabledSpotInfiniteLights   = 0x2;
	sFFTNLGenDesc.uEnabledPointLocalLights     = 0x4;
	sFFTNLGenDesc.uEnabledPointInfiniteLights  = 0x10;
	sFFTNLGenDesc.uLightHasSpecular            = 0x0;
	sFFTNLGenDesc.uNumBlendUnits               = 4;
	sFFTNLGenDesc.uNumMatrixPaletteEntries     = 16;

	sFFTNLGenDesc.uSecAttrConstBaseAddressReg = 0;
	sFFTNLGenDesc.uSecAttrStart               = 2;
	sFFTNLGenDesc.uSecAttrEnd                 = 128;
	sFFTNLGenDesc.eCodeGenMethod              = FFCGM_TWO_PASS;

	sFFTNLGenDesc.eCodeGenFlags               = FFGENCGF_REDIRECT_OUTPUT_TO_INPUT | FFGENCGF_INPUT_REG_SIZE_4;

#endif
	gbUseDescriptions = IMG_FALSE;
	CreateProgram(&sFFTNLGenDesc, "asmcode.asm");
	gbUseDescriptions = IMG_TRUE;
	CreateProgram(&sFFTNLGenDesc, "asmcodevb.asm");

	DisplayUnfreedMem();
}



#endif /* #if STADALONE */

/******************************************************************************
 End of file (ffgen.c)
******************************************************************************/
