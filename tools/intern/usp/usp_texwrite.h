 /*****************************************************************************
 Name			: usp_imgwrite.c
 
 Title			: Header for usp_imgwrite.c
 
 C Author 		: 
 
 Created  		: 13/07/2010
 
 Copyright      : 2010 by Imagination Technologies Limited. All rights reserved.
                : No part of this software, either material or conceptual 
                : may be copied or distributed, transmitted, transcribed,
                : stored in a retrieval system or translated into any 
                : human or computer language in any form by any means,
                : electronic, mechanical, manual or other-wise, or 
                : disclosed to third parties without the express written
                : permission of Imagination Technologies Limited, Unit 8, HomePark
                : Industrial Estate, King's Langley, Hertfordshire,
                : WD4 8LZ, U.K.

 Description 	: Definitions and interface for usp_imgwrite.c
   
 Program Type	: 32-bit DLL

 Version	 	: $Revision: 1.3 $

 Modifications	:

 $Log: usp_texwrite.h $
*****************************************************************************/

#ifndef _USP_IMGWRITE_H_
#define _USP_IMGWRITE_H_

IMG_INTERNAL IMG_BOOL USPTextureWriteGenerateCode(PUSP_TEXTURE_WRITE psTextureWrite,
												PUSP_SHADER		 psShader,
												PUSP_CONTEXT	 psContext);

#endif /* #ifndef _USP_IMGWRITE_H_ */

/*****************************************************************************
 End of file (usp_imgwrite.h)
*****************************************************************************/
