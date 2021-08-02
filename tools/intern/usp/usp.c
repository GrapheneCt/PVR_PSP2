 /*****************************************************************************
 Name			: USP.C
 
 Title			: USP API functions
 
 C Author 		: Joe Molleson
 
 Created  		: 02/01/2002
 
 Copyright      : 2002-2006 by Imagination Technologies Limited. All rights reserved.
                : No part of this software, either material or conceptual 
                : may be copied or distributed, transmitted, transcribed,
                : stored in a retrieval system or translated into any 
                : human or computer language in any form by any means,
                : electronic, mechanical, manual or other-wise, or 
                : disclosed to third parties without the express written
                : permission of Imagination Technologies Limited, Unit 8, HomePark
                : Industrial Estate, King's Langley, Hertfordshire,
                : WD4 8LZ, U.K.

 Description 	: Top-level USP API functions
   
 Program Type	: 32-bit DLL

 Version	 	: $Revision: 1.38 $

 Modifications	:

 $Log: usp.c $
*****************************************************************************/
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <memory.h>
#include "img_types.h"
#include "sgxdefs.h"
#include "use.h"
#include "usp_typedefs.h"
#include "usp.h"
#include "hwinst.h"
#include "uspshrd.h"
#include "uspshader.h"
#include "finalise.h"

#if defined(SHARED_LIBRARY_FOR_TEST_ONLY)
#define EXPORT_FOR_TEST_ONLY
#else
#define EXPORT_FOR_TEST_ONLY IMG_INTERNAL
#endif

/*****************************************************************************
 Name:		PVRUniPatchCreateContext

 Purpose:	Allocate and initialise a new unpatch execution context, which
			contains all the data that must be retained between API calls.

 Inputs:	pfnAlloc	- Memory allocation routine to be used by he patcher
			pfnFree		- Memory free routine to be used by the patcher
			pfnPrint	- Debug-print output routine to be used by the patcher

 Outputs:	none

 Returns:	none
*****************************************************************************/
EXPORT_FOR_TEST_ONLY IMG_PVOID IMG_CALLCONV PVRUniPatchCreateContext(USP_MEMALLOCFN	pfnAlloc,
															 USP_MEMFREEFN	pfnFree,
															 USP_DBGPRINTFN	pfnPrint)
{
	PUSP_CONTEXT	psContext;

	psContext = (PUSP_CONTEXT)pfnAlloc(sizeof(*psContext));
	if	(!psContext)
	{
		pfnPrint("PVRUniPatchCreateContext: Context alloc failed\n");
		return IMG_NULL;
	}

	memset(psContext, 0, sizeof(*psContext));
	
	psContext->pfnAlloc = pfnAlloc;
	psContext->pfnFree	= pfnFree;
	psContext->pfnPrint	= pfnPrint;

	return psContext;
}

/*****************************************************************************
 Name:		PVRUniPatchCreateShader

 Purpose:	Create a runtime-patchable shader from a precompiled uniflex
			shader-data.

 Inputs:	pvContext	- The current USP execution context
			psPCShader	- Pre-compiled shader data

 Outputs:	The created USP shader

 Returns:	none
*****************************************************************************/
EXPORT_FOR_TEST_ONLY IMG_PVOID IMG_CALLCONV PVRUniPatchCreateShader(IMG_PVOID		pvContext,
															PUSP_PC_SHADER	psPCShader)
{
	PUSP_CONTEXT	psContext;
	PUSP_SHADER		psUSPShader;

	psContext = (PUSP_CONTEXT)pvContext;

	psUSPShader = USPShaderCreate(psContext, psPCShader);
	if	(!psUSPShader)
	{
		USP_DBGPRINT(("PVRUniPatchCreateShader: Error creating USP shader\n"));
	}

	return psUSPShader;
}

/*****************************************************************************
 Name:		PVRUniPatchCreatePCShader

 Purpose:	Create a precompiled uniflex shader-data from a runtime-patchable 
            shader.

 Inputs:	pvContext	- The current USP execution context
			pvShader	- runtime-patchable shader

 Outputs:	none

 Returns:	The created precompiled shader
*****************************************************************************/
EXPORT_FOR_TEST_ONLY IMG_PVOID IMG_CALLCONV PVRUniPatchCreatePCShader(IMG_PVOID		pvContext,
												              IMG_PVOID		pvShader)
{
	PUSP_CONTEXT	psContext;
	PUSP_SHADER		psShader;
	PUSP_PC_SHADER	psPCShader;
	
	psContext   = (PUSP_CONTEXT)pvContext;
	psShader	= (PUSP_SHADER)pvShader;

	psPCShader = PCShaderCreate(psContext, psShader);
	if	(!psPCShader)
	{
		USP_DBGPRINT(("PVRUniPatchCreatePCShader: Error creating PC shader\n"));
	}

	return psPCShader;
}

/*****************************************************************************
 Name:		PVRUniPatchSetTextureFormat

 Purpose:	Allows the texture-format and other information associated with
			a specific sampler to be specified.

 Inputs:	pvContext		- The current USP execution context
			uSamplerIdx		- Index of the sampler being described
			psFormat		- The data-format for each channel within the
							  associated texture
			bAnisoEnabled	- Indicates whether the sampler is to use
							  anisotropic filtering
			bGammaEnabled	- Indicates whether the sampler is to use
							  gamma correction
			
 Outputs:	none

 Returns:	none
*****************************************************************************/
EXPORT_FOR_TEST_ONLY IMG_VOID IMG_CALLCONV PVRUniPatchSetTextureFormat(IMG_PVOID		pvContext,
															   IMG_UINT16		uSamplerIdx,
															   PUSP_TEX_FORMAT	psFormat,
															   IMG_BOOL			bAnisoEnabled,
															   IMG_BOOL			bGammaEnabled)
{
	PUSP_CONTEXT		psContext;
	PUSP_SAMPLER_DESC	psSamplerDesc;

	psContext = (PUSP_CONTEXT)pvContext;

	/*
		Simply record the sampler details for future use
	*/
	if	(uSamplerIdx >= USP_MAX_SAMPLER_IDX)
	{
		psContext->pfnPrint("PVRUniPatchSetTextureFormat: Invalid sampler-index. Must be 0-%d\n", USP_MAX_SAMPLER_IDX);
		return;
	}

	psSamplerDesc = &psContext->asSamplerDesc[uSamplerIdx];

	psSamplerDesc->sTexFormat		= *psFormat;
	psSamplerDesc->bAnisoEnabled	= bAnisoEnabled;
	psSamplerDesc->bGammaEnabled	= bGammaEnabled;

	USP_DBGPRINT(("The texture format of the texture (%d) which we are setting is %d\n", uSamplerIdx, psContext->asSamplerDesc[uSamplerIdx].sTexFormat.eTexFmt));
}

/*****************************************************************************
 Name:		PVRUniPatchSetNormalizedCoords

 Purpose:	Specifies for a given sampler whether the input coordinates will
            will be normalized.

 Inputs:	pvContext		- The current USP execution context
			uSamplerIdx		- Index of the sampler being described
			bNormCoords     - Indicates whether the sampler is to use
							  normalized co-ordinates
			
 Outputs:	none

 Returns:	none
*****************************************************************************/
EXPORT_FOR_TEST_ONLY IMG_VOID IMG_CALLCONV PVRUniPatchSetNormalizedCoords(IMG_PVOID  pvContext,
                                                                          IMG_UINT16 uSamplerIdx,
                                                                          IMG_BOOL   bNormCoords,
                                                                          IMG_BOOL   bLinearFilter,	    
                                                                          IMG_UINT32 uTextureWidth,
                                                                          IMG_UINT32 uTextureHeight)
{
	PUSP_CONTEXT      psContext;
	PUSP_SAMPLER_DESC psSamplerDesc;

	psContext = (PUSP_CONTEXT)pvContext;

	/*
		Simply record the sampler details for future use
	*/
	if	(uSamplerIdx >= USP_MAX_SAMPLER_IDX)
	{
		psContext->pfnPrint("PVRUniPatchSetTextureFormat: Invalid sampler-index. Must be 0-%d\n", USP_MAX_SAMPLER_IDX);
		return;
	}

	psSamplerDesc = &psContext->asSamplerDesc[uSamplerIdx];

	/* Set normalized flag accordingly */
	psSamplerDesc->bNormalizeCoords = !bNormCoords;
	psSamplerDesc->bLinearFilter    = bLinearFilter;
	psSamplerDesc->uTextureWidth    = uTextureWidth;
	psSamplerDesc->uTextureHeight   = uTextureHeight;
}

/*****************************************************************************
 Name:		PVRUniPatchSetOutputLocation

 Purpose:	Allows the register-bank where the final results of a shader must
			be located.

 Inputs:	pvContext		- The current USP execution context
			eRegType		- The required register-type
			
 Outputs:	none

 Returns:	none
*****************************************************************************/
EXPORT_FOR_TEST_ONLY IMG_VOID IMG_CALLCONV PVRUniPatchSetOutputLocation(IMG_PVOID			pvContext,
												   				USP_OUTPUT_REGTYPE	eRegType)
{
	PUSP_CONTEXT psContext;

	/*
		Simply record the required output register-type for future use
	*/
	psContext = (PUSP_CONTEXT)pvContext;

	psContext->eOutputRegType = eRegType;
}

/*****************************************************************************
 Name:		PVRUniPatchSetPreambleInstCount

 Purpose:	Specify the number of instructions to be appended at the beginning
			of the shader entry-point (Default is zero, must be less than 
			USP_MAX_PREAMBLE_INST_COUNT).

 Inputs:	pvContext	- The current USP execution context
			uInstCount	- The number of NOP instruction that the USP will
						  insert at the beginning of the shader.
			
 Outputs:	none

 Returns:	IMG_FALSE on error.
*****************************************************************************/
EXPORT_FOR_TEST_ONLY IMG_BOOL IMG_CALLCONV PVRUniPatchSetPreambleInstCount(
													IMG_PVOID		pvContext,
													IMG_UINT32		uInstCount)
{
	PUSP_CONTEXT psContext;

	psContext = (PUSP_CONTEXT)pvContext;

	/*
		Validate and record the required preamble instruction count
	*/
	if	(uInstCount > USP_MAX_PREAMBLE_INST_COUNT)
	{
		return IMG_FALSE;
	}
	psContext->uPreambleInstCount = uInstCount;

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		PVRUniPatchFinaliseShader

 Purpose:	Generates a HW-executable shader from a patchable USP shader.

 Inputs:	pvContext	- The current USP execution context
			pvShader	- The runtime-patcheable shader for which HW code
						  should be generated
			
 Outputs:	none

 Returns:	Pointer to a structure containing the HW instructions and 
			associated meta-data needed by a driver.
*****************************************************************************/
EXPORT_FOR_TEST_ONLY PUSP_HW_SHADER IMG_CALLCONV PVRUniPatchFinaliseShader(
														IMG_PVOID pvContext,
													  	IMG_PVOID pvShader)
{
	PUSP_CONTEXT	psContext;
	PUSP_SHADER		psShader;
	PUSP_HW_SHADER	psHWShader;
	IMG_UINT32		uSmpIdx;

	psContext	= (PUSP_CONTEXT)pvContext;
	psShader	= (PUSP_SHADER)pvShader;
	psHWShader	= IMG_NULL;

	/*
		Initialize all texture control words as not set
	*/
	for(uSmpIdx=0; uSmpIdx<psShader->uTotalSmpTexCtrWrds; uSmpIdx++)
	{
		IMG_UINT32 uPlnIdx;

		psShader->psTexCtrWrds[uSmpIdx].bUsed = IMG_FALSE;

		for (uPlnIdx=0; uPlnIdx<USP_TEXFORM_MAX_PLANE_COUNT; uPlnIdx++)
		{
			psShader->psTexCtrWrds[uSmpIdx].auWords[uPlnIdx] = (IMG_UINT32)-1;
			
			psShader->psTexCtrWrds[uSmpIdx].auTexWrdIdx[uPlnIdx] = (IMG_UINT16)-1;

			psShader->psTexCtrWrds[uSmpIdx].abExtFmt[uPlnIdx] = IMG_FALSE;

			psShader->psTexCtrWrds[uSmpIdx].abReplicate[uPlnIdx] = IMG_FALSE;

			psShader->psTexCtrWrds[uSmpIdx].aeUnPackFmts[uPlnIdx] = USP_REGFMT_UNKNOWN;
			
			psShader->psTexCtrWrds[uSmpIdx].auSwizzle[uPlnIdx] = (IMG_UINT32)-1;

			psShader->psTexCtrWrds[uSmpIdx].abMinPack[uPlnIdx] = IMG_FALSE;

			psShader->psTexCtrWrds[uSmpIdx].auTagSize[uPlnIdx] = 0;
		}
	}

	/*
		Add texture formats to USP sample blocks
	*/
	if	(!HandleUSPSampleTextureFormat(psContext, psShader))
	{
		USP_DBGPRINT(( "PVRUniPatchFinaliseShader: Error adding sample data to shader for finalisation\n"));
		return IMG_NULL;
	}

	/*
		Insert preamble code as required
	*/
	if	(!FinaliseShaderPreamble(psContext, psShader))
	{
		USP_DBGPRINT(( "PVRUniPatchFinaliseShader: Error finalising shader preamble code\n"));
		return IMG_NULL;
	}

	/*
		Finalise the location of results and temporary data used for samples
	*/
	if	(!FinaliseResultLocations(psContext, psShader))
	{
		USP_DBGPRINT(( "PVRUniPatchFinaliseShader: Error finalising result locations\n"));
		return IMG_NULL;
	}

	/*
		Generate code for samples
	*/
	if	(!FinaliseSamples(psContext, psShader))
	{
		USP_DBGPRINT(( "PVRUniPatchFinaliseShader: Error finalising texture-samples\n"));
		return IMG_NULL;
	}

	/*
	  Generate code for texture writes
	*/
	if  (!FinaliseTextureWriteList(psContext, psShader))
	{
		USP_DBGPRINT(( "PVRUniPatchFinaliseShader: Error finalising texture write list\n"));
		return IMG_NULL;
	}

	/*
		Finalise the overall register counts now that we know how many
		are required by samples
	*/
	if	(!FinaliseRegCounts(psContext, psShader))
	{
		USP_DBGPRINT(( "PVRUniPatchFinaliseShader: Error finalising temp/pa counts\n"));
		return IMG_NULL;
	}

	/*
		Finalise the shader results
	*/
	if	(!FinaliseResults(psContext, psShader))
	{
		USP_DBGPRINT(("PVRUniPatchFinaliseShader: Error finalising results\n"));
		return IMG_NULL;
	}

	/*
		Finalise the instructions within all blocks
	*/
	if	(!FinaliseInstructions(psContext, psShader))
	{
		USP_DBGPRINT(( "PVRUniPatchFinaliseShader: Error finalising instructions\n"));
		return IMG_NULL;
	}

	/*
		Fixup branch destinations now that we have generated all instructions
	*/
	if	(!FinaliseBranches(psContext, psShader))
	{
		USP_DBGPRINT(( "PVRUniPatchFinaliseShader: Error finalising branch destinations\n"));
		return IMG_NULL;
	}
	
	/*
		Remove excess trailing iterations that may have been added by the USC to
		stop the USP overwriting shader results when in PA registers
	*/
	if	(!RemoveDummyIterations(psShader))
	{
		USP_DBGPRINT(( "PVRUniPatchFinaliseShader: Error removing excess PS input data\n" ));
		return IMG_NULL;
	}

	/*
		Generate the HW structure encapsulating the finalised shader code
	*/
	psHWShader = CreateHWShader(psContext, psShader);
	if	(!psHWShader)
	{
		USP_DBGPRINT(( "PVRUniPatchFinaliseShader: Failed to create HW shader\n"));
		return IMG_NULL;
	}

	/*
		Reset any data that is altered during finalisation
	*/
	if	(!ResetShader(psContext, psShader))
	{
		USP_DBGPRINT(( "PVRUniPatchFinaliseShader: Error resetting shader for finalisation\n"));
		psContext->pfnFree(psHWShader);
		return IMG_NULL;
	}

	return psHWShader;
}

/*****************************************************************************
 Name:		PVRUniPatchDestroyHWShader

 Purpose:	Destroys a HW executeable shader, previously created from a
			runtime-patchable shader using PVRUniPatchFinaliseShader.

 Inputs:	pvContext	- The current USP execution context
			psHWShader	- The HW-executeable shader-data to destroy
			
 Outputs:	none

 Returns:	none
*****************************************************************************/
EXPORT_FOR_TEST_ONLY IMG_VOID IMG_CALLCONV PVRUniPatchDestroyHWShader(
													IMG_PVOID		pvContext,
													PUSP_HW_SHADER	psHWShader)
{
	PUSP_CONTEXT psContext;

	psContext = (PUSP_CONTEXT)pvContext;
	if	(psHWShader)
	{		
		psContext->pfnFree(psHWShader);
	}
}

/*****************************************************************************
 Name:		PVRUniPatchDestroyShader

 Purpose:	Destroy a runtime-patchable shader, previously created using
			PVRUniPatchCreateShader

 Inputs:	pvContext	- The current USP execution context
			pvShader	- The patchable shader to be destroyed
			
 Outputs:	none

 Returns:	none
*****************************************************************************/
EXPORT_FOR_TEST_ONLY IMG_VOID IMG_CALLCONV PVRUniPatchDestroyShader(IMG_PVOID pvContext,
															IMG_PVOID pvShader)
{
	PUSP_CONTEXT	psUSPContext;
	PUSP_SHADER		psUSPShader;

	psUSPContext	= (PUSP_CONTEXT)pvContext;
	psUSPShader		= (PUSP_SHADER)pvShader;

	USPShaderDestroy(psUSPContext, psUSPShader);
}

/*****************************************************************************
 Name:		PVRUniPatchDestroyPCShader

 Purpose:	Destroy a precompiled shader, previously created using
			PVRUniPatchCreatePCShader

 Inputs:	pvContext	- The current USP execution context
			psPCShader	- The precompiled shader to be destroyed
			
 Outputs:	none

 Returns:	none
*****************************************************************************/
EXPORT_FOR_TEST_ONLY IMG_VOID IMG_CALLCONV PVRUniPatchDestroyPCShader(
													IMG_PVOID		pvContext,
													PUSP_PC_SHADER	psPCShader)
{
	PUSP_CONTEXT	psContext;
		
	psContext = (PUSP_CONTEXT)pvContext;
	
	psContext->pfnFree(psPCShader);
}

/*****************************************************************************
 Name:		PVRUniPatchDestroyContext

 Purpose:	Destroy a unipatch execution context, previously created using
			PVRUniPatchCreateContext

 Inputs:	pvContext	- The USP context that is no longer required

 Outputs:	none

 Returns:	none
*****************************************************************************/
EXPORT_FOR_TEST_ONLY IMG_VOID IMG_CALLCONV PVRUniPatchDestroyContext(IMG_PVOID pvContext)
{
	PUSP_CONTEXT psContext;

	psContext = (PUSP_CONTEXT)pvContext;

	psContext->pfnFree(psContext);
}

/*****************************************************************************
 End of file (USC.C)
*****************************************************************************/

