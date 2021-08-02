 /*****************************************************************************
 Name			: FINALISE.H
 
 Title			: Definitions and interface for FINALISE.C
 
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

 Description 	: Function prototypes for finalise.c
   
 Program Type	: 32-bit DLL

 Version	 	: $Revision: 1.18 $

 Modifications	:

 $Log: finalise.h $
*****************************************************************************/
#ifndef _FINALISE_H_
#define _FINALISE_H_

#include "uspshrd.h"

/*
	Add texture formats to the USP shader sample blocks.
*/
IMG_BOOL HandleUSPSampleTextureFormat(PUSP_CONTEXT psContext, 
									  PUSP_SHADER psShader);

/*
	Initialises a shader prior to finalisation
*/
IMG_BOOL ResetShader(PUSP_CONTEXT psContext, PUSP_SHADER psShader);

/*
	Finalise the location of results used by samples
*/
IMG_BOOL FinaliseResultLocations(PUSP_CONTEXT	psContext,
								 PUSP_SHADER	psShader);

/*
	Finalise the overall temp and PA register couts required by the shader
*/
IMG_BOOL FinaliseRegCounts(PUSP_CONTEXT	psContext, PUSP_SHADER psShader);

/*
	Generate code for dependent and non-dependent texture-samples
*/
IMG_BOOL FinaliseSamples(PUSP_CONTEXT psContext, PUSP_SHADER psShader);

/*
	Finalise the PS result location
*/
IMG_BOOL FinaliseResults(PUSP_CONTEXT psContext, PUSP_SHADER psShader);

/*
	Remove dummy iterations added to avoid overwriting outputs when in PA regs
*/
IMG_BOOL RemoveDummyIterations(PUSP_SHADER psShader);

/*
	Finalise the instructions for all instruction blocks
*/
IMG_BOOL FinaliseInstructions(PUSP_CONTEXT psContext, PUSP_SHADER psShader);

/*
	Insert additional instructions at the start of the shader if required
*/
IMG_BOOL FinaliseShaderPreamble(PUSP_CONTEXT psContext, PUSP_SHADER	psShader);

/*
	Finalise branch destinations within a USP shader
*/
IMG_BOOL FinaliseBranches(PUSP_CONTEXT psContext, PUSP_SHADER psShader);

/*
     Finalise texture writes within a USP shader
 */
IMG_INTERNAL IMG_BOOL FinaliseTextureWriteList(PUSP_CONTEXT psContext,
											 PUSP_SHADER  psShader);

/*
	Create a HW-shader from a finalised USP shader
*/
PUSP_HW_SHADER CreateHWShader(PUSP_CONTEXT psContext, PUSP_SHADER psShader);

#endif	/* #ifndef _FINALISE_H_ */
/*****************************************************************************
 End of file (FINALISE.H)
*****************************************************************************/
