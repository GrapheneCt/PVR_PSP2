/* 
 * Name         : qsort_s.c
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
 * Description  : Naive implementation of libc qsort with an arbitrary
 *                threaded state.
 * 
 * Platform     : ALL
 *
 * $Date: 2009/11/04 16:11:33 $ $Revision: 1.3 $
 * $Log: qsort_s.c $
 *
 *  --- Revision Logs Removed --- 
 */

#include "img_defs.h"
#include "qsort.h"

/*
   <function>
   FUNCTION   : _swap
   PURPOSE    : Exchange to arbitrary elements within an array of element.
   PARAMETERS : In:  pva - pointer to first element
                In:  pvb - pointer to second element
                In:  size - size of each element in bytes.
   RETURNS    :
   </function>
 */
static IMG_VOID _swap(IMG_VOID *pva, IMG_VOID *pvb, IMG_INT size)
{
	IMG_UINT8 *a = pva;
	IMG_UINT8 *b = pvb;

	if (a!=b)
	{
		int i;

		for (i=0; i<size; i++)
		{
			IMG_UINT8 t;

			t = a[i];

			a[i] = b[i];

			b[i] = t;
		}
	}
}

/*
   <function>
   FUNCTION   : _partition
   PURPOSE    : Partition a sub-section of an array of elements such that
                all elements to the left of an arbitrary pivot point compare
                less than all of the elements to the right of the pivot point.
   PARAMETERS : In:  l - pointer to left most element in partition.
                In:  r - pointer to right most element in partition.
                In:  size - size of each element in bytes.
                In:  compare - element comparison function.
                In:  state
   RETURNS    : Pointer to the pivot.
   </function>
 */
static IMG_INT *_partition(IMG_VOID *l, IMG_VOID *r, IMG_INT size,
					   IMG_INT (*compare)(IMG_VOID *a, IMG_VOID *b, IMG_VOID *s),
					   IMG_VOID *state)
{
	IMG_VOID *p = l;

	while (l<r)
	{
		while (compare (l, p, state) <= 0 && l < r)
		{
			l = (IMG_UINT8 *)l + size;
		}

		while (compare (r, p, state) > 0)
		{
			r = (IMG_UINT8 *)r - size;
		}

		if (l<r)
		{
			_swap (l, r, size);
		}
	}

	_swap (p, r, size);

	return r;
}

/*
   <function>
   FUNCTION   : _sort
   PURPOSE    : Quick sort work horse. Sort the elements within a
                sub parition of an array of elements.
   PARAMETERS : In:  l - pointer to left most element in partition.
                In:  r - pointer to right most element in partition.
                In:  size - size of each element in bytes.
                In:  compare - element comparison function.
                In:  state -
   RETURNS    : None
   </function>
 */
static IMG_VOID _sort(IMG_VOID *l, IMG_VOID *r, IMG_INT size,
			      IMG_INT (*compare)(IMG_VOID *a, IMG_VOID *b, IMG_VOID *s),
			      IMG_VOID *state)
{
	if (l<r)
	{
		IMG_INT *m;

		m = _partition(l, r, size, compare, state);

		_sort(l, (IMG_UINT8 *)m - size, size, compare, state);

		_sort((IMG_UINT8 *)m + size, r, size, compare, state);
	}
}

/*
   <function>
   FUNCTION   : PVR_qsort_s
   PURPOSE    : Quick sort in place.
   PARAMETERS : In:  base - array of elements to sort
                In:  n - number of elements in the array
                In:  size - size in bytes of each element
                In:  compare - function to compare two elements should
                     return 0 => a == b
                           <0 => a <  b
                           >0 => a >  b
                In:  state -
   RETURNS    : None
   </function>
 */
IMG_INTERNAL IMG_VOID PVR_qsort_s(IMG_VOID *base, IMG_INT n, IMG_INT size,
						      IMG_INT (*compare)(IMG_VOID *a, IMG_VOID *b, IMG_VOID *state),
						      IMG_VOID *state)
{
	  _sort(base, (IMG_UINT8 *)base + size * (n-1), size, compare, state);
}

