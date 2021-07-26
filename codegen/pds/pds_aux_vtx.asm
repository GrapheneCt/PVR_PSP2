/******************************************************************************
 * Name         : pds_aux_vtx.asm
 *
 * Copyright    : 2006-2008 by Imagination Technologies Limited.
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
 * $Log: pds_aux_vtx.asm $
 *****************************************************************************/

#include "sgxdefs.h"

#if !defined(FIX_HW_BRN_25339) && !defined(FIX_HW_BRN_27330)
 
/**********************************************************
						PDS data
***********************************************************/
 
/* Constants */ 
data dword STREAM0;
data dword STREAM1;
data dword STREAM2;
data dword STREAM3;
data dword STREAM4;
data dword STREAM5;
data dword STREAM6;
data dword STREAM7;
data dword USE0;
data dword USE1;
data dword USE2;
 
/* Persistant temps */
temp dword vtx_stream0   = ds0[52];
temp dword vtx_stream1   = ds0[53];
temp dword vtx_stream2   = ds0[54];
temp dword vtx_stream3   = ds0[55];
temp dword vtx_stream4   = ds0[56];
temp dword vtx_stream5   = ds0[57];
temp dword vtx_stream6   = ds0[58];
temp dword vtx_stream7   = ds0[59];

/**********************************************************
					PDS instructions
***********************************************************/

/* update persistant temps with current stream addresses */
mov32	 vtx_stream0, STREAM0
mov32	 vtx_stream1, STREAM1
mov32	vtx_stream2, STREAM2
mov32	vtx_stream3, STREAM3
mov32	 vtx_stream4, STREAM4
mov32	 vtx_stream5, STREAM5
mov32	 vtx_stream6, STREAM6
mov32	vtx_stream7, STREAM7

/* run a dummy USE program */
movs	doutu,		USE0,			USE1,			USE2

/* end of program */
halt

#else /* !defined(FIX_HW_BRN_25339) && !defined(FIX_HW_BRN_27330) */

/**********************************************************
						PDS data
***********************************************************/
 
/* Constants */ 
data dword STREAM0;
data dword STREAM1;
data dword STREAM2;
data dword STREAM3;
data dword STREAM4;
data dword STREAM5;
data dword STREAM6;
data dword STREAM7;
data dword USE0;
data dword USE1;
data dword USE2;
temp dword temp_USE2 = ds1[48];
 
/* Persistant temps */
temp dword vtx_stream0   = ds0[52];
temp dword vtx_stream1   = ds0[53];
temp dword vtx_stream2   = ds0[54];
temp dword vtx_stream3   = ds0[55];
temp dword vtx_stream4   = ds0[56];
temp dword vtx_stream5   = ds0[57];
temp dword vtx_stream6   = ds0[58];
temp dword vtx_stream7   = ds0[59];

/**********************************************************
					PDS instructions
***********************************************************/

/* update persistant temps with current stream addresses */
mov32	vtx_stream0, STREAM0
mov32	vtx_stream1, STREAM1
mov32	vtx_stream2, STREAM2
mov32	vtx_stream3, STREAM3
mov32	vtx_stream4, STREAM4
mov32	vtx_stream5, STREAM5
mov32	vtx_stream6, STREAM6
mov32	vtx_stream7, STREAM7

/* run a dummy USE program */
mov32	temp_USE2,	USE2
movs	doutu,		USE0,			USE1,			temp_USE2

/* end of program */
halt

#endif /* !defined(FIX_HW_BRN_25339) && !defined(FIX_HW_BRN_27330) */
