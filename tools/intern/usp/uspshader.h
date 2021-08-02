 /*****************************************************************************
 Name			: USPSHADER.H
 
 Title			: Definitions and interface for USPSHADER.C
 
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

 Description 	: Interface for the Uniflex Shader Patcher (UniPatch)
   
 Program Type	: 32-bit DLL

 Version	 	: $Revision: 1.6 $

 Modifications	:

 $Log: uspshader.h $
*****************************************************************************/
#ifndef _USPSHADER_H_
#define _USPSHADER_H_

/*
	Create and add a result-reference to a USP shader
*/
IMG_BOOL USPShaderAddResultRef(PUSP_SHADER		psShader,
							   PUSP_RESULTREF	psResultRef);

/*
	Remove a result-reference from the list of those for a shader
*/
IMG_BOOL USPShaderRemoveResultRef(PUSP_SHADER		psShader,
								  PUSP_RESULTREF	psResultRef);

/*
	Unpack precompiled shader data to form an USP internal shader
*/
PUSP_SHADER USPShaderCreate(PUSP_CONTEXT psContext, IMG_PVOID psPCShader);

/*
    Pack USP shader to precompiled shader data
*/
PUSP_PC_SHADER PCShaderCreate(PUSP_CONTEXT psContext, PUSP_SHADER psUSPShader);

/*
	Destroy a USP internal shader previously created using USPShaderCreate
*/
IMG_VOID USPShaderDestroy(PUSP_CONTEXT psContext, PUSP_SHADER psUSPShader);

#endif	/* #ifndef _USPSHADER_H_ */
/*****************************************************************************
 End of file (USPSHADER.H)
*****************************************************************************/
