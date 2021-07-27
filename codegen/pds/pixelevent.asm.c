/******************************************************************************
 * Name         : pixelevent.asm
 *
 * Copyright    : 2006-2009 by Imagination Technologies Limited.
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
 * $Log: pixelevent.asm $
 *****************************************************************************/


#include "psp2_pvr_desc.h"
#include "sgxdefs.h"


#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)


temp dword param0, temp_EOT2;
data dword EOT0, EOT1, EOT2;
data dword EOR0, EOR1, EOR2;
data dword PTOFF0, PTOFF1, PTOFF2;

!if0 bra	not_eot

#if defined(MSAA_NO_DOWNSCALE) && defined(FIX_HW_BRN_31175)
#if (TILE_EXTRA_SHIFTX != 0) || (TILE_EXTRA_SHIFTY != 0)
/*
	NOTE: Add DMSAA_NO_DOWNSCALE to your makefiles!
	
	The incomming tileXY for multisample without downscale is incorrect when
	this BRN is present. We need to scale them to the correct values.
*/

// Grab everything but the tile coords..
and	temp_EOT2, (EURASIA_PDS_DOUTU2_TILEX_CLRMSK & EURASIA_PDS_DOUTU2_TILEY_CLRMSK), ir0

// Scale the tile X
and	param0, ir0, ~EURASIA_PDS_DOUTU2_TILEX_CLRMSK
shl	param0, param0, TILE_EXTRA_SHIFTX
or	temp_EOT2, param0, temp_EOT2

// Scale the tile Y
and	param0, ir0, ~EURASIA_PDS_DOUTU2_TILEY_CLRMSK
shl	param0, param0, TILE_EXTRA_SHIFTY
or	temp_EOT2, param0, temp_EOT2

/*
	Send the end of tile program to the USE and halt.
*/
movs	doutu, EOT0, EOT1, temp_EOT2
#endif /* (TILE_EXTRA_SHIFTX != 0) || (TILE_EXTRA_SHIFTY != 0) */
#else /* FIX_HW_BRN_31175 */
/*
	Send the end of tile program to the USE and halt.
*/
movs	doutu, EOT0, EOT1, ir0
#endif /* FIX_HW_BRN_31175 */

halt

not_eot:

!if1 bra	not_eor

/*
	Send the end of render program to the USE and halt.
	If ZEROCNT send TILEX,TILEY and PBEEnable and not EOR
	Else send PBEnable only
*/
and		param0, EURASIA_PDS_IR1_PIX_EVENT_ZEROCNT, ir1
tstz	p0, param0
and	param0, ir0, ~EURASIA_PDS_IR0_PDM_EOR
or	param0, param0, EURASIA_PDS_IR0_PDM_EOR_EOT
p0	mov32	param0, EOR2

movs	doutu, EOR0, EOR1, param0

halt

not_eor:

/*
	Send the PTOFF program to the USE and halt.
*/

movs	doutu, PTOFF0, PTOFF1, PTOFF2

halt

#else /* defined(SGX_FEATURE_PDS_EXTENDED_SOURCES) */

/* Unlimited phases implies DOUTU reordering */
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)

#if !defined(FIX_HW_BRN_25339) && !defined(FIX_HW_BRN_27330)
temp dword param0, temp_EOT1;
data dword PTOFF0, PTOFF1;
data dword EOT0, EOT1;
data dword EOR0, EOR1;

#if defined(SGX_FEATURE_WRITEBACK_DCU)
data dword EOT_DOUTA1;
#endif

/*
	Check if this is an end of pass event
*/
and	param0, ir1, EURASIA_PDS_IR1_PIX_EVENT_SENDPTOFF
tstz	p0, param0
p0 bra	not_ptoff

/*
	Send the PTOFF program to the USE and halt.
*/
movs	doutu, PTOFF0, PTOFF1
halt

not_ptoff:

/*
	Check if this is an end of tile event.
*/
and	param0, ir1, EURASIA_PDS_IR1_PIX_EVENT_ENDTILE
tstz	p0, param0
p0 bra	not_eot


not	param0, ir1
and	param0, EURASIA_PDS_IR1_PIX_EVENT_ZEROCNT, param0
tstz	p0, param0
p0	bra dummy_eot

#if defined(SGX_FEATURE_WRITEBACK_DCU)
/*
	Move the value of ir1 into the primary attributes so that CFI can be added on last tile in 
	render
*/
movs	douta, ir1, EOT_DOUTA1
#endif

/*
	Move the tile X coordinate from ir0 to the DOUTU parameters.
*/
shl	param0, ir0, (EURASIA_PDS_DOUTU1_TILEX_SHIFT - EURASIA_PDS_IR0_PDM_TILEX_SHIFT) + TILE_EXTRA_SHIFTX
and	param0, ~EURASIA_PDS_DOUTU1_TILEX_CLRMSK, param0
or	temp_EOT1, EOT1, param0

/*
	Move the tile Y coordinate from ir0 to the DOUTU parameters.
*/
shl	param0, ir0, (EURASIA_PDS_DOUTU1_TILEY_SHIFT - EURASIA_PDS_IR0_PDM_TILEY_SHIFT) + TILE_EXTRA_SHIFTY
and	param0, ~EURASIA_PDS_DOUTU1_TILEY_CLRMSK, param0 
or	temp_EOT1, temp_EOT1, param0

/*
	Move the end of render flag from ir1 to the DOUTU parameters.
*/
shl	param0, ir1, EURASIA_PDS_DOUTU1_ENDOFRENDER_SHIFT - EURASIA_PDS_IR1_PIX_EVENT_ENDRENDER_BIT
and	param0, ~EURASIA_PDS_DOUTU1_ENDOFRENDER_CLRMSK, param0
or	temp_EOT1, temp_EOT1, param0

/*
	Send the end of tile program to the USE and halt.
*/
movs	doutu, EOT0, temp_EOT1

halt

not_eot:

/*
	Check for end of render without end of tile - this is a special context switch event.
*/
and	param0, ir1, EURASIA_PDS_IR1_PIX_EVENT_ENDRENDER
tstz	p0, param0
p0 bra	not_eor

dummy_eot:

/*
	Send the end of render program to the USE and halt.
*/
movs	doutu, EOR0, EOR1

not_eor:
halt
#else /* !FIX_HW_BRN_25339 && !(FIX_HW_BRN_27330)*/

data dword PTOFF0, PTOFF1;
data dword EOT0, EOT1;
data dword EOR0, EOR1;
#if defined(SGX_FEATURE_WRITEBACK_DCU)
data dword EOT_DOUTA1;
#endif
temp dword param0 = ds1[48];
temp dword temp_EOT1 = ds0[48];
temp dword temp_ds1 = ds1[49];
temp dword temp_ds0 = ds0[49];

/*
	Check if this is an end of pass event
*/
mov32	temp_ds1, EURASIA_PDS_IR1_PIX_EVENT_SENDPTOFF
and	param0, ir1, temp_ds1
tstz	p0, param0
p0 bra	not_ptoff

/*
	Send the PTOFF program to the USE and halt.
*/
movs	doutu, PTOFF0, PTOFF1
halt

not_ptoff:

/*
	Check if this is an end of tile event.
*/
mov32	temp_ds1, EURASIA_PDS_IR1_PIX_EVENT_ENDTILE
and	param0, ir1, temp_ds1
tstz	p0, param0
p0 bra	not_eot

not	param0, ir1
and	param0, EURASIA_PDS_IR1_PIX_EVENT_ZEROCNT, param0
tstz	p0, param0
p0	bra dummy_eot

#if defined(SGX_FEATURE_WRITEBACK_DCU)
/*
	Move the value of ir1 into the primary attributes so that CFI can be added on last tile in 
	render
*/
mov32	temp_ds1, EOT_DOUTA1
movs	douta, ir1, temp_ds1
#endif

/*
	Move the tile X coordinate from ir0 to the DOUTU parameters.
*/
shl	param0, ir0, (EURASIA_PDS_DOUTU1_TILEX_SHIFT - EURASIA_PDS_IR0_PDM_TILEX_SHIFT) + TILE_EXTRA_SHIFTX
and	param0, ~EURASIA_PDS_DOUTU1_TILEX_CLRMSK, param0
or	temp_EOT1, EOT1, param0

/*
	Move the tile Y coordinate from ir0 to the DOUTU parameters.
*/
shl	param0, ir0, (EURASIA_PDS_DOUTU1_TILEY_SHIFT - EURASIA_PDS_IR0_PDM_TILEY_SHIFT) + TILE_EXTRA_SHIFTY
and	param0, ~EURASIA_PDS_DOUTU1_TILEY_CLRMSK, param0 
or	temp_EOT1, temp_EOT1, param0

/*
	Move the end of render flag from ir1 to the DOUTU parameters.
*/
shl	param0, ir1, EURASIA_PDS_DOUTU1_ENDOFRENDER_SHIFT - EURASIA_PDS_IR1_PIX_EVENT_ENDRENDER_BIT
and	param0, ~EURASIA_PDS_DOUTU1_ENDOFRENDER_CLRMSK, param0
or	temp_EOT1, temp_EOT1, param0

/*
	Send the end of tile program to the USE and halt.
*/
mov32	temp_ds1, EOT0
movs	doutu, temp_ds1, temp_EOT1

halt

not_eot:

/*
	Check for end of render without end of tile - this is a special context switch event.
*/
mov32	temp_ds1, EURASIA_PDS_IR1_PIX_EVENT_ENDRENDER
and	param0, ir1, temp_ds1
tstz	p0, param0
p0 bra	not_eor

dummy_eot:

/*
	Send the end of render program to the USE and halt.
*/
mov32	temp_ds0, EOR0
mov32	temp_ds1, EOR1
movs	doutu, temp_ds0, temp_ds1

not_eor:
halt

#endif /* !FIX_HW_BRN_25339 && !(FIX_HW_BRN_27330)*/

#else /* SGX_FEATURE_USE_UNLIMITED_PHASES */

#if !defined(FIX_HW_BRN_25339) && !defined(FIX_HW_BRN_27330)

temp dword param0, temp_EOT2;
data dword PTOFF0, PTOFF1, PTOFF2;
data dword EOT0, EOT1, EOT2;
data dword EOR0, EOR1, EOR2;

/*
	Check if this is an end of pass event
*/
and	param0, ir1, EURASIA_PDS_IR1_PIX_EVENT_SENDPTOFF
tstz	p0, param0
p0 bra	not_PTOFF

/*
	Send the PTOFF program to the USE and halt.
*/
movs	doutu, PTOFF0, PTOFF1, PTOFF2
halt

not_PTOFF:

/*
	Check if this is an end of tile event.
*/
and	param0, ir1, EURASIA_PDS_IR1_PIX_EVENT_ENDTILE
tstz	p0, param0
p0 bra	not_EOT

/*
	Move the tile X coordinate from ir0 to the DOUTU parameters.
*/
shl	param0, ir0, (EURASIA_PDS_DOUTU2_TILEX_SHIFT - EURASIA_PDS_IR0_PDM_TILEX_SHIFT) + TILE_EXTRA_SHIFTX
and	param0, ~EURASIA_PDS_DOUTU2_TILEX_CLRMSK, param0
or	temp_EOT2, EOT2, param0

/*
	Move the tile Y coordinate from ir0 to the DOUTU parameters.
*/
shl	param0, ir0, (EURASIA_PDS_DOUTU2_TILEY_SHIFT - EURASIA_PDS_IR0_PDM_TILEY_SHIFT) + TILE_EXTRA_SHIFTY
and	param0, ~EURASIA_PDS_DOUTU2_TILEY_CLRMSK, param0 
or	temp_EOT2, temp_EOT2, param0

/*
	Move the end of render flag from ir0 to the DOUTU parameters.
*/
shl	param0, ir1, EURASIA_PDS_DOUTU2_ENDOFRENDER_SHIFT - EURASIA_PDS_IR1_PIX_EVENT_ENDRENDER_BIT
and	param0, ~EURASIA_PDS_DOUTU2_ENDOFRENDER_CLRMSK, param0
or	temp_EOT2, temp_EOT2, param0

/*
	Send the end of tile program to the USE and halt.
*/
movs	doutu, EOT0, EOT1, temp_EOT2

halt

not_EOT:

/*
	Check for end of render without end of tile.
*/
and	param0, ir1, EURASIA_PDS_IR1_PIX_EVENT_ENDRENDER
tstz	p0, param0
p0 bra	not_eor

/*
	Send the end of render program to the USE and halt.
*/
movs	doutu, EOR0, EOR1, EOR2

not_eor:
halt


#else /* !FIX_HW_BRN_25339 && !(FIX_HW_BRN_27330)*/


data dword PTOFF0, PTOFF1, PTOFF2;
data dword EOT0, EOT1, EOT2;
data dword EOR0, EOR1, EOR2;
temp dword temp_ds1 = ds1[48];
temp dword param0 = ds0[48];
temp dword temp_EOT2 = ds1[49];

/*
	Check if this is an end of pass event
*/
mov32	temp_ds1, EURASIA_PDS_IR1_PIX_EVENT_SENDPTOFF
and	param0, ir1, temp_ds1
tstz	p0, param0
p0 bra	not_PTOFF

/*
	Send the PTOFF program to the USE and halt.
*/
mov32	temp_ds1, PTOFF2
movs	doutu, PTOFF0, PTOFF1, temp_ds1
halt

not_PTOFF:

/*
	Check if this is an end of tile event.
*/
mov32	temp_ds1, EURASIA_PDS_IR1_PIX_EVENT_ENDTILE
and	param0, ir1, temp_ds1
tstz	p0, param0
p0 bra	not_EOT

/*
	Move the tile X coordinate from ir0 to the DOUTU parameters.
*/
shl	param0, ir0, (EURASIA_PDS_DOUTU2_TILEX_SHIFT - EURASIA_PDS_IR0_PDM_TILEX_SHIFT) + TILE_EXTRA_SHIFTX
mov32	temp_ds1, ~EURASIA_PDS_DOUTU2_TILEX_CLRMSK
and	param0, param0, temp_ds1
mov32	temp_ds1, EOT2
or	temp_EOT2, param0, temp_ds1

/*
	Move the tile Y coordinate from ir0 to the DOUTU parameters.
*/
shl	param0, ir0, (EURASIA_PDS_DOUTU2_TILEY_SHIFT - EURASIA_PDS_IR0_PDM_TILEY_SHIFT) + TILE_EXTRA_SHIFTY
mov32	temp_ds1, ~EURASIA_PDS_DOUTU2_TILEY_CLRMSK
and	param0, param0, temp_ds1
or	temp_EOT2, param0, temp_EOT2 

/*
	Move the end of render flag from ir1 to the DOUTU parameters.
*/
shl	param0, ir1, EURASIA_PDS_DOUTU2_ENDOFRENDER_SHIFT - EURASIA_PDS_IR1_PIX_EVENT_ENDRENDER_BIT
mov32	temp_ds1, ~EURASIA_PDS_DOUTU2_ENDOFRENDER_CLRMSK
and	param0, param0, temp_ds1
or	temp_EOT2, param0, temp_EOT2

/*
	Send the end of tile program to the USE and halt.
*/
movs	doutu, EOT0, EOT1, temp_EOT2

halt

not_EOT:

/*
	Check for end of render without end of tile.
*/
mov32	temp_ds1, EURASIA_PDS_IR1_PIX_EVENT_ENDRENDER
and	param0, ir1, temp_ds1
tstz	p0, param0
p0 bra	not_eor

/*
	Send the end of render program to the USE and halt.
*/
mov32	temp_ds1, EOR2
movs	doutu, EOR0, EOR1, temp_ds1

not_eor:

halt

#endif /* !FIX_HW_BRN_25339 && !(FIX_HW_BRN_27330)*/

#endif /* SGX_FEATURE_USE_UNLIMITED_PHASES */

#endif /* SGX_FEATURE_PDS_EXTENDED_SOURCES */
