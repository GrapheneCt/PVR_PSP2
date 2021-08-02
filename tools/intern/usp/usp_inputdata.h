 /*****************************************************************************
 Name			: USP_INPUTDATA.H
 
 Title			: Header for USP_INPUTDATA.C
 
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

 Description 	: Definitions and interface for USP_INPUTDATA.C
   
 Program Type	: 32-bit DLL

 Version	 	: $Revision: 1.8 $

 Modifications	:

 $Log: usp_inputdata.h $
*****************************************************************************/
#ifndef _USP_INPUTDATA_H_
#define _USP_INPUTDATA_H_

/*
	Lookup details about pre-sampled data for a texture chunk
*/
PUSP_PRESAMPLED_DATA USPInputDataGetPreSampledData(
											PUSP_INPUT_DATA			psInputData,
											IMG_UINT32				uTextureIdx,
											IMG_UINT32				uChunkIdx,
											IMG_UINT16				uTexCtrWrdIdx,
											USP_ITERATED_DATA_TYPE	eCoord,
											IMG_BOOL				bProjected,
											USP_REGFMT				eFmtConvert);

/*
	Lookup details about an iterated input value
*/
PUSP_ITERATED_DATA USPInputDataGetIteratedData(PUSP_INPUT_DATA	psInputData,
											   IMG_UINT32		uOrgRegNum);

/*
	Lookup details about the state for a chunk of texture data
*/
PUSP_TEXSTATE_DATA USPInputDataGetTexStateData(PUSP_INPUT_DATA	psInputData,
											   IMG_UINT32		uTextureIdx,
											   IMG_UINT32		uChunkIdx,
											   IMG_UINT32		uTexCtrWrdIdx);

/*
	Calculate PA register indexes for the iterated and pre-sampled input data
*/
IMG_BOOL USPInputDataCalcPARegUse(PUSP_INPUT_DATA	psInputData,
								  PUSP_SHADER		psShader,
								  IMG_PUINT32		puPARegCount);

/*									
	Add a texture-state data to the input-data for a shader
*/
IMG_BOOL USPInputDataAddTexStateData(PUSP_INPUT_DATA	psInputData,
									 PUSP_SAMPLE		psSample,
									 IMG_UINT32			uTextureIdx,
									 IMG_UINT32			uTexChunkMask,
									 IMG_UINT32			uTexNonDepChunkMask,
									 PUSP_SHADER		psShader,
									 PUSP_CONTEXT		psContext);

/*
	Add a iterated data to the input-data for a shader
*/
IMG_BOOL USPInputDataAddIteratedData(PUSP_INPUT_DATA	psInputData, 
									 PUSP_SAMPLE		psSample, 
									 PUSP_CONTEXT		psContext);

/* 
	Lookup details about an iterated value for a non dependent sample
*/
PUSP_ITERATED_DATA USPInputDataGetIteratedDataForNonDepSample(PUSP_INPUT_DATA	psInputData, 
															  PUSP_SAMPLE		psSample, 
															  PUSP_CONTEXT		psContext);

/*
	Add a pre-sampled texture to the input-data for a shader
*/
IMG_BOOL USPInputDataAddPreSampledData(PUSP_SHADER		psShader,
									   PUSP_INPUT_DATA	psInputData,
									   PUSP_SAMPLE		psSample,
									   PUSP_CONTEXT		psContext);

/*
	Record the pre-compiled iterated and pre-sampled data for a shader
*/
IMG_BOOL USPInputDataAddPCInputData(PUSP_INPUT_DATA			psInputData,
									IMG_UINT32				uPCInputLoadCount,
									PUSP_PC_PSINPUT_LOAD	psPCInputLoads,
									PUSP_CONTEXT			psContext);

/*
	Alloc and initialise data describing the input for a shader
*/
PUSP_INPUT_DATA USPInputDataCreate(PUSP_CONTEXT psContext);

/*
	Allocate a list for the iterated input data to a shader
*/
IMG_BOOL USPInputDataCreateIteratedDataList(PUSP_INPUT_DATA	psInputData,
											IMG_UINT32		uMaxIteratedData,
											PUSP_CONTEXT	psContext);

/*
	Allocate a list for the pre-sampled input data to a shader
*/
IMG_BOOL USPInputDataCreatePreSampledDataList(PUSP_INPUT_DATA	psInputData,
											  IMG_UINT32		uMaxPreSampledData,
											  PUSP_CONTEXT		psContext);

/*
	Allocate a list for the texture-state input data to a shader
*/
IMG_BOOL USPInputDataCreateTexStateDataList(PUSP_INPUT_DATA	psInputData,
											IMG_UINT32		uMaxTexStateData,
											PUSP_CONTEXT	psContext);

/*
	Cleanup data describing the input data for a shader
*/
IMG_VOID USPInputDataDestroy(PUSP_INPUT_DATA	psInputData,
							 PUSP_CONTEXT		psContext);

/*
	Reset the input-data for a shader prior to finalisation
*/
IMG_BOOL USPInputDataReset(PUSP_INPUT_DATA psInputData);

/*
	For any pre-sampled texture samples whose results aren't used set the
	entries to dummy values.
*/
IMG_VOID USPInputDataFixUnusedPreSampledData(PUSP_SHADER		psShader, 
											 PUSP_INPUT_DATA	psInputData);

#endif	/* #ifndef _USP_INPUTDATA_H_ */
/*****************************************************************************
 End of file (USP_INPUTDATA.H)
*****************************************************************************/

