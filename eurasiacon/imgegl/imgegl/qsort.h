/*
 * Name         : qsort.h
 * Title        : Quick Sort
 * Author       : Marcus Shawcroft
 * Created      : 4 Nov 2003
 *
 * Copyright    : 2003-2005 by Imagination Technologies Limited.
 *                All rights reserved.  No part of this software, either
 *                material or conceptual may be copied or distributed,
 *                transmitted, transcribed, stored in a retrieval system
 *                or translated into any human or computer language in any
 *                form by any means, electronic, mechanical, manual or
 *                other-wise, or disclosed to third parties without the
 *                express written permission of Imagination Technologies
 *                Limited, Unit 8, HomePark Industrial Estate,
 *                King's Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * Description  : Naive implementation of libc qsort API.
 * 
 * Platform     : ALL
 *
 * $Log: qsort.h $
 *
 */

#ifndef _qsort_h_
#define _qsort_h_

IMG_VOID PVR_qsort_s (IMG_VOID *x,
			      IMG_INT n,
			      IMG_INT size,
			      IMG_INT (*compare)(IMG_VOID *a, IMG_VOID *b, IMG_VOID *state),
			      IMG_VOID *state);

#endif
