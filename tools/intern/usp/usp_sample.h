 /*****************************************************************************
 Name			: USP_SAMPLE.H
 
 Title			: Header for USP_SAMPLE.C
 
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

 Description 	: Definitions and interface for USP_SAMPLE.C
   
 Program Type	: 32-bit DLL

 Version	 	: $Revision: 1.7 $

 Modifications	:

 $Log: usp_sample.h $
*****************************************************************************/
#ifndef _USP_SAMPLE_H_
#define _USP_SAMPLE_H_

/*
	Reset a dependent or non-dependent sample.
*/
IMG_BOOL USPSampleReset(PUSP_SAMPLE psSample, PUSP_CONTEXT psContext);

/*
	Reset a sample unpack.
*/
IMG_BOOL USPSampleUnpackReset(PUSP_SAMPLEUNPACK psSample, PUSP_CONTEXT psContext);


/*
	Adds the pre-sampled data i.e. required to input-data for the shader
*/
IMG_BOOL USPSampleDataAdd(PUSP_SAMPLE	psSample, 
						  PUSP_CONTEXT	psContext, 
						  PUSP_SHADER	psShader);

/*
	Generate instrucitons for a dependent sample
*/
IMG_BOOL USPSampleGenerateCode(PUSP_SAMPLE	psSample,
							   PUSP_SHADER	psShader,
							   PUSP_CONTEXT	psContext);

/*
	Generate instrucitons for a sample unpack
*/
IMG_BOOL USPSampleUnpackGenerateCode(	PUSP_SAMPLEUNPACK	psSample,
										PUSP_SHADER			psShader,
										PUSP_CONTEXT		psContext);

/*
	Create and initialise USP sample info
*/
PUSP_SAMPLE USPSampleCreate(PUSP_CONTEXT	psContext,
							PUSP_SHADER		psShader,
							PUSP_MOESTATE	psMOEState);

/*
	Create and initialise USP sample unpack info
*/
PUSP_SAMPLEUNPACK USPSampleUnpackCreate(	PUSP_CONTEXT	psContext,
											PUSP_SHADER		psShader,
											PUSP_MOESTATE	psMOEState);

/*
	Destroy USP sample data previously created using USPSampleCreate
*/
IMG_VOID USPSampleDestroy(PUSP_SAMPLE psSample, PUSP_CONTEXT psContext);

/*
	Destroy USP sample data previously created using USPSampleCreate
*/
IMG_VOID USPSampleUnpackDestroy(PUSP_SAMPLEUNPACK psSampleUnpack, PUSP_CONTEXT psContext);

/* 
	Decides whether to Sample Non Depedent Samples using Sample 
	Instructions or using Command List Entries.
*/
IMG_BOOL DecideChunksSampling(
							  PUSP_CONTEXT	psContext, 
							  PUSP_SHADER	psShader);

/*  
	Decides final texture formats.
*/
IMG_BOOL DecideFinalTextureFormats(PUSP_CONTEXT	psContext, 
								   PUSP_SHADER	psShader);

#endif	/* #ifndef _USP_SAMPLE_H_ */
/*****************************************************************************
 End of file (USP_SAMPLE.H)
*****************************************************************************/
