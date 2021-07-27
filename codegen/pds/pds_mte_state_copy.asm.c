/******************************************************************************
 * Name         : pds_mte_state_copy.asm
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
 * $Log: pds_mte_state_copy.asm $
 *****************************************************************************/
 
#include "psp2_pvr_desc.h"
#include "sgxdefs.h"
 
#if !defined(FIX_HW_BRN_25339) && !defined(FIX_HW_BRN_27330)
 
/**********************************************************
						PDS data
***********************************************************/

data dword DMA0;
data dword DMA1;
data dword DMA2;
data dword DMA3;
data dword USE0;
data dword USE1;
data dword USE2;

/**********************************************************
					PDS instructions
***********************************************************/

/* check DMA0, skip it if null...  */
tstz		p0,		DMA0
p0			bra		USE_ISSUE

/* ... else write to DMA interface */
movs		doutd,	DMA0,	DMA1		

/* check DMA2, skip it if null...  */
tstz		p0,		DMA2
p0			bra		USE_ISSUE

/* ... else write to DMA interface */
movs		doutd,	DMA2,	DMA3


/* run MTE update program on USE */
USE_ISSUE:
movs	doutu,		USE0,			USE1,			USE2

/* end of program */
halt

#else /* !defined(FIX_HW_BRN_25339) && !defined(FIX_HW_BRN_27330) */
 
/**********************************************************
						PDS data
***********************************************************/

data dword DMA0;
data dword DMA1;
data dword DMA2;
data dword DMA3;
data dword USE0;
data dword USE1;
data dword USE2;
temp dword temp_USE2 = ds1[48]

/**********************************************************
					PDS instructions
***********************************************************/

/* write to DMA interface */
movs		doutd,	DMA0,	DMA1		

/* check DMA2, skip it if null...  */
tstz		p0,		DMA2
p0			bra		USE_ISSUE

/* ... else write to DMA interface */
movs		doutd,	DMA2,	DMA3


/* run MTE update program on USE */
USE_ISSUE:
mov32	temp_USE2,	USE2
movs	doutu,		USE0,			USE1,			temp_USE2

/* end of program */
halt

#endif /* !defined(FIX_HW_BRN_25339) && !defined(FIX_HW_BRN_27330) */
