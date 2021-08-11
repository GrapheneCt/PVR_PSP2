/******************************************************************************
 * Name         : ogles2_types.h
 * Author       : BCB
 * Created      : 08/12/2005
 *
 * Copyright    : 2005-2006 by Imagination Technologies Limited.
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
 * $Date: 2010/02/01 15:31:28 $ $Revision: 1.19 $
 * $Log: ogles2_types.h $
 *****************************************************************************/
#ifndef _OGLES2_TYPES_
#define _OGLES2_TYPES_

#include "img_defs.h"
#include "img_3dtypes.h"

typedef struct GLES2Context_TAG GLES2Context;
typedef struct GLES2ProgramRec GLES2Program;
typedef struct GLES2USEShaderVariant_TAG GLES2USEShaderVariant;
typedef struct GLES2PDSCodeVariant_TAG GLES2PDSCodeVariant;
typedef struct GLES2ProgramShaderRec GLES2ProgramShader;

typedef enum GLES2_MEMERROR_TAG
{
	GLES2_NO_ERROR	= 0,
	GLES2_TA_BUFFER_ERROR	= 1,
	GLES2_3D_BUFFER_ERROR	= 2,
	GLES2_TA_USECODE_ERROR	= 3,
	GLES2_TA_PDSCODE_ERROR	= 4,
	GLES2_3D_USECODE_ERROR	= 5,
	GLES2_3D_PDSCODE_ERROR	= 6,
	GLES2_GENERAL_MEM_ERROR	= 7,
	GLES2_HOST_MEM_ERROR	= 8
}GLES2_MEMERROR;


/*
** Coordinate structure.  Coordinates contain x, y, z and w.
*/
typedef struct GLES2coordRec
{
    IMG_FLOAT fX, fY, fZ, fW;

} GLES2coord;

typedef union GLES2_FUINT32Rec
{
	IMG_FLOAT fVal;
	IMG_UINT32 ui32Val;

}GLES2_FUINT32;

/*
** Color structure.  Colors are composed of red, green, blue and alpha.
*/
typedef struct GLES2colorRec
{
    IMG_FLOAT fRed, fGreen, fBlue, fAlpha;

} GLES2color;


typedef struct GLESpixelSpanInfoRec
{
    IMG_INT32  i32ReadX, i32ReadY;						/* Reading coords (ReadPixels) */
    IMG_UINT32 ui32Width, ui32Height;					/* Size of image */
    IMG_UINT32 ui32DstSkipPixels, ui32DstSkipLines;		/* Skip some pixels (probably due to window clip) */
    
	IMG_INT32 i32SrcRowIncrement;						/* Add this much to get to the next row */
    IMG_INT32 i32SrcGroupIncrement;						/* Add this much to get to the next group */
	IMG_UINT32 ui32DstRowIncrement;						/* Add this much to get to the next row */
    IMG_UINT32 ui32DstGroupIncrement;					/* Add this much to get to the next group */
	IMG_VOID  *pvInData, *pvOutData;					/* Temp pointers for input and output */

} GLES2PixelSpanInfo;


#define FLOAT_TO_LONG(x)	(* (long *)( & x)) 
#define FLOAT_TO_UINT32(x)	(* (IMG_UINT32 *)( & x)) 
#define LONG_TO_FLOAT(x)	(* (float *)( & x))
#define FIXED_TO_LONG(x)	((long)(x) >> 16)
#define LONG_TO_FIXED(x)	((long)(x) << 16)

#define FIXED_TO_FLOAT(x)	((float)(x) * (1.0f / 65536.0f))

#define FLOAT_TO_FIXED(x)	(long)((x) * 65536.0f)

/*
** Not quite 2^31-1 because of possible floating point errors.  4294965000
** is a much safer number to use.
*/
#define GLES_INT_TO_FLOAT(i)		((((IMG_FLOAT)(i) * 2.0f) + 1.0f) * (1.0f/4294965000.0f))
#define GLES_UB_TO_FLOAT(ub)		((ub) * ((IMG_FLOAT) 1.0f /(IMG_FLOAT) 255.0))
#define GLES_B_TO_FLOAT(b)			((((b)<<1) + 1) * (1.0f/255.0f))
#define GLES_S_TO_FLOAT(s)			((((s)<<1) + 1) * (1.0f/65535.0f))
#define GLES_US_TO_FLOAT(us) 		((IMG_FLOAT)(us) * (1.0f/65535.0f))
#define GLES_UI_TO_FLOAT(ui) 		((IMG_FLOAT)(ui) * (1.0f/4294965000.0f))


#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
#define GLES_UI24_TO_FLOAT(ui) 		((IMG_FLOAT)(ui) * (1.0f/16777216.0f))
#endif /* defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */


#endif /* _OGLES2_TYPES_ */

/******************************************************************************
 End of file (ogles2_types.h)
******************************************************************************/


