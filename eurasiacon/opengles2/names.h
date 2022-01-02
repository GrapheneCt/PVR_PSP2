/******************************************************************************
 * Name         : names.h
 * Author       : BCB
 * Created      : 29/05/2003
 *
 * Copyright    : 2003-2006 by Imagination Technologies Limited.
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
 * $Date: 2010/02/19 16:20:18 $ $Revision: 1.24 $
 * $Log: names.h $
 *****************************************************************************/
#ifndef _NAMES_
#define _NAMES_


#if defined(GLES2_EXTENSION_VERTEX_ARRAY_OBJECT)
#define GLES2_MAX_NAMETYPE               6
#define GLES2_MAX_SHAREABLE_NAMETYPE     5
#define GLES2_MAX_UNSHAREABLE_NAMETYPE   1
#else
#define GLES2_MAX_NAMETYPE               5
#define GLES2_MAX_SHAREABLE_NAMETYPE     5
#define GLES2_MAX_UNSHAREABLE_NAMETYPE   0
#endif /* defined(GLES2_EXTENSION_VERTEX_ARRAY_OBJECT) */


typedef enum GLES2NameTypeTAG
{
    GLES2_NAMETYPE_TEXOBJ       = 0x00000000, /* Shareable */
	GLES2_NAMETYPE_PROGRAM      = 0x00000001, /* Shareable */
	GLES2_NAMETYPE_BUFOBJ       = 0x00000002, /* Shareable */
	GLES2_NAMETYPE_RENDERBUFFER = 0x00000003, /* Shareable */
	GLES2_NAMETYPE_FRAMEBUFFER  = 0x00000004  /* Shareable */
#if defined(GLES2_EXTENSION_VERTEX_ARRAY_OBJECT)
    ,GLES2_NAMETYPE_VERARROBJ   = 0x00000005  /* Unshareable */
#endif
/* PRQA S 3332 1 */ /* Override QAC suggestion and use this macro. */

} GLES2NameType;

/* It must be a prime number. Some alternatives are: 71, 83, 101, 127, 163, 199, 251, 307, 359, 439 */
//#define GLES2_DEFAULT_NAMES_ARRAY_SIZE	127
#define GLES2_DEFAULT_NAMES_ARRAY_SIZE	439


/* This structure must be the first variable of all objects we put in a names array */
typedef struct GLES2NamedItemTAG
{
	/* Name of the item. Name zero is reserved.
	 */
	IMG_UINT32           ui32Name;

	/* Reference count. Starts with value one. The named item is
	 * deleted when the refcount reaches zero.
	 * IMPORTANT: Must be initialized and modified _only_ by GLES2NamesArray.
	 */
	IMG_UINT32           ui32RefCount;

	IMG_BOOL			 bGeneratedButUnused;

	/*  Pointer to the next element in the chain. Used Internally.
	 */
	struct GLES2NamedItemTAG *psNext;

} GLES2NamedItem;


/* Names array: a dictionary that associates 32-bit names with arbitrary objects */
typedef struct GLES2NamesArrayTAG
{
	/* Type of the elements in this name array */
	GLES2NameType        eType;
    
    /* Whether shareable */
    IMG_BOOL             bShareable;

	/* Only allow generated names */
    IMG_BOOL             bGeneratedOnly;

	/* Mutex (used internally) */
	PVRSRV_MUTEX_HANDLE  hSharedLock;

	/* Callback for freeing named items. It's alright if it locks the name array. */
	IMG_VOID             (*pfnFree)(GLES2Context *gc, GLES2NamedItem *psItem, IMG_BOOL bIsShutdown);

	/* Last unique name that was generated. Used to generate new names */
	IMG_UINT32           ui32LastNameGenerated;

	/* Number of items currently in the array. Used to optimize NamesArrayMapFunction() on empty arrays */
	IMG_UINT32           ui32NumItems;

	/* The hash table we use for the lookups.
	 * It uses chaining to resolve collisions.
	 */
	GLES2NamedItem      *apsEntry[GLES2_DEFAULT_NAMES_ARRAY_SIZE];

} GLES2NamesArray;


/* Returns a new empty instance of GLES2NamesArray */
GLES2NamesArray * CreateNamesArray(GLES2Context *gc, GLES2NameType eType, PVRSRV_MUTEX_HANDLE hSharedLock);

/* Generate Num unique names and store them in the array Names */
IMG_BOOL NamesArrayGenNames(GLES2NamesArray *psNamesArray, IMG_UINT32 ui32Num, IMG_UINT32 pui32Names[/*ui32Num*/]);

/* Call a function on all the items currently in the names array. The function must NOT try to lock the array  */
IMG_VOID NamesArrayMapFunction(GLES2Context *gc, GLES2NamesArray *psNamesArray,
							   IMG_VOID (*pfnMap)(GLES2Context *gc, const IMG_VOID *pvFunctionContext, GLES2NamedItem* psNamedItem),
							   const IMG_VOID *pvFunctionContext);

					   
/* Deletes the names array and all the elements in it */
IMG_VOID DestroyNamesArray(GLES2Context *gc, GLES2NamesArray *psNamesArray);


/* Inserts a new named item in the names array.
 * The item and its name must be unique in the names array.
 * Its refcount is initialized automatically when it is inserted.
 * NOTE: this function (with differing semantics) was named NewNamedItem
 */
IMG_BOOL InsertNamedItem(GLES2NamesArray *psNamesArray, GLES2NamedItem* psNamedItem);


/* Increases the refcount of the named item. Returns a pointer to its data */
/* If no object with that name is in the array, IMG_NULL is returned */
/* NOTE: this function was previously named LockNamedItem */
GLES2NamedItem* NamedItemAddRef(GLES2NamesArray *psNamesArray, IMG_UINT32 ui32Name);


/* Decreases the refcount of an item. The object is deleted when this function is 
 * called one more time than NamedItemAddRef had been called before.
 */
/* NOTE: this function was previously named UnlockDataItem */
IMG_VOID NamedItemDelRef(GLES2Context *gc, GLES2NamesArray *psNamesArray, GLES2NamedItem *psNamedItem);


/*
 * Decreases the refcount of a list of items given their names. The objects are deleted if their refcount drops to zero.
 * The array ui32Name must have length ui32Num. Names that do not match an object in the array are ignored silently.
 * After this function returns, the names that had been passed to it become available again even if the objects
 * they identified have not been deleted. This complies with the ES 2.0 spec.
 */
IMG_VOID NamedItemDelRefByName(GLES2Context *gc, GLES2NamesArray *psNamesArray,
							   IMG_UINT32 ui32Num, const IMG_UINT32 ui32Name[/*ui32Num*/]);


#endif /* _NAMES_ */

/******************************************************************************
 End of file (names.h)
******************************************************************************/

