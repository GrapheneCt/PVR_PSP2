/******************************************************************************
* Name         : sgxpixfmts.c
* Title        : Pixel format definitions
* Author(s)    : Imagination Technologies
* Created      : 14th May 2009
*
* Copyright    : 2009-2010 by Imagination Technologies Limited.
*                All rights reserved. No part of this software, either material
*                or conceptual may be copied or distributed, transmitted,
*                transcribed, stored in a retrieval system or translated into
*                any human or computer language in any form by any means,
*                electronic, mechanical, manual or otherwise, or disclosed
*                to third parties without the express written permission of
*                Imagination Technologies Limited, Home Park Estate,
*                Kings Langley, Hertfordshire, WD4 8LZ, U.K.
*
* Description  : For specified pixel formats, describe useful information
*
* Platform     : Generic
*
* Modifications:-
* $Log: sgxpixfmts.c $
******************************************************************************/

#ifdef __psp2__

#include "psp2_pvr_desc.h"
#include "psp2_pvr_defs.h"

#endif

#include "img_defs.h"
#include "sgxdefs.h"
#include "servicesext.h"
#include "img_3dtypes.h"
#include "sgxpixfmts_types.h"

/*
	Include the auto-generated texture format table...
*/
#if defined(SGX545)
	#include "sgx545pixfmts.h"
#else
	#if defined(SGX543) || defined(SGX544) || defined(SGX554)
		#include "sgx543pixfmts.h"
	#else
		#if defined(SGX520) || defined(SGX530) || defined(SGX531) || defined(SGX535) || defined(SGX540) || defined(SGX541)
			#include "sgx535pixfmts.h"
		#endif
	#endif
#endif

/*****************************************************************************
 End of file (sgxpixfmts.c)
*****************************************************************************/
