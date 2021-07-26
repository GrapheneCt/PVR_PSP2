/******************************************************************************
 * Name         : fftnlgles.h
 * Author       : jml
 * Created      : 2/09/2006
 *
 * Copyright    : 2006 by Imagination Technologies Limited.
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
 * $Log: fftnlgles.h $
 *****************************************************************************/

#ifndef _FFTNLGLES_
#define _FFTNLGLES_


#define GLES1_FFTNL_SHADERS_FILENAME "fftnlshader.txt"


IMG_BOOL InitFFTNLState(GLES1Context *gc);
IMG_VOID FreeFFTNLState(GLES1Context *gc);

GLES1_MEMERROR SetupFFTNLShader(GLES1Context *gc);

IMG_VOID SetupBuildFFTNLShaderConstants(GLES1Context *gc);

IMG_BOOL SetupFFTNLShaderOutputs(GLES1Context *gc);

IMG_VOID DestroyFFTNLCode(GLES1Context *gc, IMG_UINT32 ui32Item);

#define FFTNL_COMMENT_SIZE 200

#define GLES1_ONE_OVER_LN_TWO			(1.0f/0.693147180559945309417232121458177f)
#define GLES1_ONE_OVER_SQRT_LN_TWO		(1.0f/0.832554611157697756353164644895201f)

#define COPY_FLOAT(pfConstant, fSrc)			\
{							\
	(*pfConstant) = fSrc;				\
}

#define COPY_COORD4(pfDst, sCoord)			\
{							\
	pfDst[0] = sCoord.fX;				\
	pfDst[1] = sCoord.fY;				\
	pfDst[2] = sCoord.fZ;				\
	pfDst[3] = sCoord.fW;				\
}

#define COPY_COORD3(pfDst, sCoord)			\
{							\
	pfDst[0] = sCoord.fX;				\
	pfDst[1] = sCoord.fY;				\
	pfDst[2] = sCoord.fZ;				\
}

#define COPY_COLOR4(pfDst, sColor)			\
{							\
	pfDst[0] = sColor.fRed;				\
	pfDst[1] = sColor.fGreen;			\
	pfDst[2] = sColor.fBlue;			\
	pfDst[3] = sColor.fAlpha;			\
}

#define COPY_MATRIX(pfDest, pfSrc, ui32Count)		\
{							\
	IMG_UINT16 j;					\
	for(j = 0; j < ui32Count; j++)			\
	{						\
		pfDest[j] = pfSrc[j];			\
	}						\
}	

#endif /* _FFTNLGLES_ */

/******************************************************************************
 End of file (fftnlgles.h)
******************************************************************************/

