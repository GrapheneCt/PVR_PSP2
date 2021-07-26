/******************************************************************************
 * Name         : source.h
 * Author       : James McCarthy
 * Created      : 05/12/2005
 *
 * Copyright    : 200o-2006 by Imagination Technologies Limited.
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
 * Modifications:-
 * $Log: source.h $
 *****************************************************************************/

#ifndef __gl_source_h_
#define __gl_source_h_


#if defined(STANDALONE) || defined(DEBUG)

IMG_INTERNAL IMG_VOID PrintInstruction(FFGenCode     *psFFGenCode,
						  FFGenInstruction *psInstruction,
						  IMG_UINT32        uLineNumber,
						  IMG_UINT32		uInstrNumber);

IMG_INTERNAL IMG_VOID DumpSource(FFGenCode *psFFGenCode, FILE *psAsmFile);

IMG_INTERNAL IMG_VOID DumpFFTNLDescription(FFGenContext	*psFFGenContext,
										   FFTNLGenDesc *psFFTNLGenDesc,
										   IMG_CHAR      bNewFile,
										   IMG_CHAR     *pszFileName);

#if defined(OGL_LINESTIPPLE)

IMG_INTERNAL IMG_VOID DumpFFGEODescription(FFGenContext	*psFFGenContext,
										   FFGEOGenDesc *psFFTNLGenDesc,
										   IMG_CHAR     *pszFileName);
#endif /* OGL_CARNAV_EXTS */

#else

#define PrintInstruction(a, b, c, d)
#define PrintSMLSI(a, b, c, d, e)
#define DumpSource(a, b)
#define DumpFFTNLDescription(a,b,c,d)

#if defined(OGL_LINESTIPPLE)
#define DumpFFGEODescription(a,b,c)
#endif /* OGL_CARNAV_EXTS */

#endif

#endif // __gl_source_h_

/******************************************************************************
 End of file (source.h)
******************************************************************************/

