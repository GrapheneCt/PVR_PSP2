/******************************************************************************
 * Name         : oGLES1_types.h
 * Author       : BCB
 * Created      : 02/05/2003
 *
 * Copyright    : 2003-2005 by Imagination Technologies Limited.
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
 * $Log: ogles_types.h $
 *****************************************************************************/
#ifndef _OGLES1_TYPES_
#define _OGLES1_TYPES_

#include "img_defs.h"
#include "img_3dtypes.h"

typedef struct GLES1Context_TAG GLES1Context;
typedef struct GLESTextureRec GLESTexture;
typedef struct GLESProcsRec GLESProcs;
typedef struct GLESMatrixRec GLESMatrix;
typedef struct GLESLightSourceMachineRec GLESLightSourceMachine;
typedef struct GLES1USEShaderVariant_TAG GLES1ShaderVariant;
typedef struct GLES1PDSCodeVariant_TAG GLES1PDSCodeVariant;

typedef enum GLES1_MEMERROR_TAG
{
	GLES1_NO_ERROR	= 0,
	GLES1_TA_BUFFER_ERROR	= 1,
	GLES1_3D_BUFFER_ERROR	= 2,
	GLES1_TA_USECODE_ERROR	= 3,
	GLES1_TA_PDSCODE_ERROR	= 4,
	GLES1_3D_USECODE_ERROR	= 5,
	GLES1_3D_PDSCODE_ERROR	= 6,
	GLES1_GENERAL_MEM_ERROR	= 7,
	GLES1_HOST_MEM_ERROR	= 8
}GLES1_MEMERROR;

/*
** Coordinate structure.  Coordinates contain x, y, z and w.
*/
typedef struct GLEScoordRec
{
    IMG_FLOAT fX, fY, fZ, fW;

} GLEScoord;

typedef union GLES1_FUINT32Rec
{
	IMG_FLOAT  fVal;
	IMG_UINT32 ui32Val;

} GLES1_FUINT32;

/*
** Color structure.  Colors are composed of red, green, blue and alpha.
*/
typedef struct GLEScolorRec
{
    IMG_FLOAT fRed, fGreen, fBlue, fAlpha;

} GLEScolor;

typedef struct GLESPixelSpanInfoRec
{
    IMG_INT32  i32ReadX, i32ReadY;						/* Reading coords (ReadPixels) */
    IMG_UINT32 ui32Width, ui32Height;					/* Size of image */
    IMG_UINT32 ui32DstSkipPixels, ui32DstSkipLines;		/* Skip some pixels (probably due to window clip) */
    
	IMG_INT32 i32SrcRowIncrement;						/* Add this much to get to the next row */
    IMG_INT32 i32SrcGroupIncrement;						/* Add this much to get to the next group */
	IMG_UINT32 ui32DstRowIncrement;						/* Add this much to get to the next row */
    IMG_UINT32 ui32DstGroupIncrement;					/* Add this much to get to the next group */
	IMG_VOID  *pvInData, *pvOutData;					/* Temp pointers for input and output */

} GLESPixelSpanInfo;

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
#define GLES1_INT_TO_FLOAT(i)		((((IMG_FLOAT)(i) * 2.0f) + 1.0f) * (1.0f/4294965000.0f))
#define GLES1_UB_TO_FLOAT(ub)		((IMG_FLOAT)(ub) * (1.0f /255.0f))
#define GLES1_B_TO_FLOAT(b)			((((b)<<1) + 1) * (1.0f/255.0f))
#define GLES1_S_TO_FLOAT(s)			((((s)<<1) + 1) * (1.0f/65535.0f))

#endif /* _OGLES1_TYPES_ */

/******************************************************************************
 End of file (oGLES1_types.h)
******************************************************************************/


