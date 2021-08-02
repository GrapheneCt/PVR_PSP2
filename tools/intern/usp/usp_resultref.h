 /*****************************************************************************
 Name			: USP_RESULTREF.H
 
 Title			: Interface for usp_resultref.c
 
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

 Description 	: Definitions and prototypes for usp_resultef.c
   
 Program Type	: 32-bit DLL

 Version	 	: $Revision: 1.2 $

 Modifications	:

 $Log: usp_resultref.h $
*****************************************************************************/
#ifndef _USP_RESULTREF_H_
#define _USP_RESULTREF_H_

/*
	Initialise a USP result-reference
*/
IMG_BOOL USPResultRefSetup(PUSP_RESULTREF	psResultRef,
						   PUSP_SHADER		psShader);

/*
	Create an initialise USP result-reference
*/
PUSP_RESULTREF USPResultRefCreate(PUSP_CONTEXT	psContext, 
								  PUSP_SHADER	psShader);

/*
	Set the instruction associated with a result-reference
*/
IMG_BOOL USPResultRefSetInst(PUSP_RESULTREF		psResultRef,
							 PUSP_INST			psInst,
							 PUSP_CONTEXT		psContext);

/*
	Destroy USP result-reference data previously created using 
	USPResultRefCreate
*/
IMG_VOID USPResultRefDestroy(PUSP_RESULTREF	psResultRef,
							 PUSP_CONTEXT	psContext); 

/*
	Reset a USP result-reference ready for finalisation
*/
IMG_VOID USPResultRefReset(PUSP_RESULTREF psResultRef);

/*
	Check that a result-reference can support a specific location for the
	shaders results.
*/
IMG_BOOL USPResultRefSupportsRegLocation(PUSP_RESULTREF	psResultRef,
										 USP_REGTYPE	eNewResultRegType,
										 IMG_UINT32		uNewResultRegNum);

/*
	Update an instruction that references the results of a shader, to access
	the result from a new location
*/
IMG_BOOL USPResultRefChangeRegLocation(PUSP_CONTEXT		psContext,
									   PUSP_RESULTREF	psResultRef,
									   USP_REGTYPE		eNewResultRegType,
									   IMG_UINT32		uNewResultRegNum);

#endif	/* #ifndef _USP_RESULTREF_H_ */
/*****************************************************************************
 End of file (USP_RESULTREF.H)
*****************************************************************************/
