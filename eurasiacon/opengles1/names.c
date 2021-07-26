/******************************************************************************
 * Name         : names.c
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
 * $Log: names.c $
 *
 *  --- Revision Logs Removed --- 
 *****************************************************************************/

#include "context.h"



/***********************************************************************************
 Function Name      : LOCK_NAMES_ARRAY
 Inputs             : psNamesArray
 Outputs            : -
 Returns            : NULL
 Description        : Lock function that either locks namesarray or does nothing.
************************************************************************************/
static IMG_VOID LOCK_NAMES_ARRAY(GLES1NamesArray *psNamesArray)
{
    if (psNamesArray->bShareable)
	{
	    /* Doesn't matter what the lock ID is  - so use 1 */
		PVRSRVLockMutex(psNamesArray->hSharedLock);
	}
}

/***********************************************************************************
 Function Name      : UNLOCK_NAMES_ARRAY
 Inputs             : psNamesArray
 Outputs            : -
 Returns            : NULL
 Description        : Unlock function that either locks namesarray or does nothing.
************************************************************************************/
static IMG_VOID UNLOCK_NAMES_ARRAY(GLES1NamesArray *psNamesArray)
{
    if (psNamesArray->bShareable)
	{
	    /* Doesn't matter what the lock ID is  - so use 1 */
		PVRSRVUnlockMutex(psNamesArray->hSharedLock);
	}
}

/***********************************************************************************
 Function Name      : BucketForName
 Inputs             : ui32Name
 Outputs            : -
 Returns            : The bucket where an item with the  given name should be stored.
 Description        : See above.
 Notes              : GLES1_DEFAULT_NAMES_ARRAY_SIZE should be a prime number.
                      See Knuth's Art of Computer Programming vol 3 for an explanation.
************************************************************************************/
#define BucketForName(ui32Name) \
	(IMG_UINT32)((ui32Name) % GLES1_DEFAULT_NAMES_ARRAY_SIZE )


/***********************************************************************************
 Function Name      : LookupItemByName
 Inputs             : psNamesArray, ui32Name
 Outputs            : -
 Returns            : The item with the given name or IMG_NULL of no item with that
                      name exists in the names array.
 Description        : The hash table implementation uses a chained list to resolve collisions.
                      The names array must have been locked previously.
************************************************************************************/
static GLES1NamedItem *LookupItemByName(const GLES1NamesArray *psNamesArray, IMG_UINT32 ui32Name)
{
	IMG_UINT32     ui32Position;
	GLES1NamedItem *psNamedItem;

	ui32Position = BucketForName(ui32Name);

	GLES1_ASSERT(ui32Position < GLES1_DEFAULT_NAMES_ARRAY_SIZE);

	psNamedItem = psNamesArray->apsEntry[ui32Position];

	/* Iterate the list until we find the end or the item */
	while(psNamedItem && psNamedItem->ui32Name != ui32Name)
	{
		psNamedItem = psNamedItem->psNext;
	}

	return psNamedItem;
}


/***********************************************************************************
 Function Name      : RemoveItemFromList
 Inputs             : psNamedItem
 Outputs            : -
 Returns            : -
 Description        : Removes a named item from the linked list of the hash table.
                      This function updates the linked list appropriately.
					  This function does NOT free the memory used by the item.
************************************************************************************/
static IMG_VOID RemoveItemFromList(GLES1NamesArray* psNamesArray, GLES1NamedItem* psNamedItem)
{
	IMG_UINT32     ui32Position;
	GLES1NamedItem *psPrev;

	ui32Position = BucketForName(psNamedItem->ui32Name);

	GLES1_ASSERT(ui32Position < GLES1_DEFAULT_NAMES_ARRAY_SIZE);

	psPrev = psNamesArray->apsEntry[ui32Position];
	
	if(psPrev)
	{
		if(psPrev == psNamedItem)
		{
			/* The list starts with the element we are removing */
			psNamesArray->apsEntry[ui32Position] = psNamedItem->psNext;
		}
		else
		{
			/* The list didn't start with the element we are removing */
			while(psPrev && psPrev->psNext != psNamedItem)
			{
				psPrev = psPrev->psNext;
			}

			if(psPrev)
			{
				psPrev->psNext = psNamedItem->psNext;
			}
			else
			{
				/* The item was not in the names array. Maybe its name was removed previously */
				return;
			}
		}
	}
	else
	{
		/* The item was not in the names array. Maybe its name was removed previously */
		return;
	}

	if(!psNamedItem->bGeneratedButUnused)
	{
	    /* The item was succesfully removed from the names array */
	    psNamesArray->ui32NumItems--;
	}
}


/***********************************************************************************
 Function Name      : CreateNamesArray
 Inputs             : gc, eType, hSharedLock
 Outputs            : -
 Returns            : A pointer to a new NamesArray
 Description        : Creates a new names array with a specific type. 
					  Sets up free fn for this type.
************************************************************************************/
IMG_INTERNAL GLES1NamesArray* CreateNamesArray(GLES1Context *gc, GLES1NameType eType, PVRSRV_MUTEX_HANDLE hSharedLock)
{
	GLES1NamesArray *psNamesArray;

	PVR_UNREFERENCED_PARAMETER(gc);

	GLES1_TIME_START(GLES1_TIMER_NAMES_ARRAY);

	psNamesArray = GLES1Calloc(gc, sizeof(GLES1NamesArray));

	if(!psNamesArray)
	{
		GLES1_TIME_STOP(GLES1_TIMER_NAMES_ARRAY);

		return IMG_NULL;
	}

	psNamesArray->eType        = eType;
	psNamesArray->hSharedLock  = hSharedLock;

	switch(eType)
	{
		case GLES1_NAMETYPE_TEXOBJ:
		{
		    psNamesArray->bShareable = IMG_TRUE;
			psNamesArray->bGeneratedOnly = IMG_FALSE;

			SetupTexNameArray(psNamesArray);
			break;
		}
		case GLES1_NAMETYPE_BUFOBJ:
		{
		    psNamesArray->bShareable = IMG_TRUE;
			psNamesArray->bGeneratedOnly = IMG_FALSE;

			SetupBufObjNameArray(psNamesArray);
			break;
		}
		case GLES1_NAMETYPE_RENDERBUFFER:
		{
		    psNamesArray->bShareable = IMG_TRUE;
			psNamesArray->bGeneratedOnly = IMG_FALSE;

			SetupRenderBufferNameArray(psNamesArray);
			break;
		}
		case GLES1_NAMETYPE_FRAMEBUFFER:
		{
		    psNamesArray->bShareable = IMG_TRUE;
			psNamesArray->bGeneratedOnly = IMG_FALSE;

			SetupFrameBufferObjectNameArray(psNamesArray);
			break;
		}
#if defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT)
	    case GLES1_NAMETYPE_VERARROBJ:
		{
		    psNamesArray->bShareable = IMG_FALSE;
			psNamesArray->bGeneratedOnly = IMG_TRUE;

			SetupVertexArrayObjectNameArray(psNamesArray);
			break;
		}
#endif
		default:
		{
			PVR_DPF((PVR_DBG_ERROR,"CreateNamesArray: Invalid name type"));

			GLES1Free(gc, psNamesArray);
			GLES1_TIME_STOP(GLES1_TIMER_NAMES_ARRAY);
			return IMG_NULL;
		}
	}

	/* Any value will do. This one is sensible */
	psNamesArray->ui32LastNameGenerated = 0;

	psNamesArray->ui32NumItems = 0;

	GLES1_TIME_STOP(GLES1_TIMER_NAMES_ARRAY);

	return psNamesArray;
}


/***********************************************************************************
 Function Name      : NamesArrayGenNames
 Inputs             : psNamesArray, ui32Num
 Outputs            : pui32Names
 Returns            : -
 Description        : Generates a list of unique and unused names.
************************************************************************************/
IMG_INTERNAL IMG_BOOL NamesArrayGenNames(GLES1NamesArray *psNamesArray, IMG_UINT32 ui32Num, IMG_UINT32 pui32Names[/*ui32Num*/])
{
	IMG_UINT32     i, ui32CandidateName;
	GLES1NamedItem *psFound = IMG_NULL;

	__GLES1_GET_CONTEXT_RETURN(IMG_FALSE);

	if(!psNamesArray || !pui32Names)
	{
		return IMG_FALSE;
	}

	GLES1_TIME_START(GLES1_TIMER_NAMES_ARRAY);

	LOCK_NAMES_ARRAY(psNamesArray);

	ui32CandidateName = psNamesArray->ui32LastNameGenerated;

	for(i=0; i < ui32Num; ++i)
	{
		/* Generate candidate names using a linear congruential generator. The probability of having
		 * a collision is extremely small, so the following loop should only iterate once in most cases.
		 */
		do
		{
#if defined GLES1_USE_SEQUENTIAL_NAMES
			/* This produces names sequentially starting with 1,2,3,4... making it easier to debug the code */
			ui32CandidateName++;
#else
			/* This code should be used in release mode. It avoids collisions much better than using sequential probing */
			ui32CandidateName = ui32CandidateName * 29943829 + 100271;
			/* Alternative (addition of a small prime): ui32CandidateName += 13567; */
#endif

			/* Name zero is reserved */
			if(ui32CandidateName != 0)
			{
				/* Is the name already used in the table? */
				psFound = LookupItemByName(psNamesArray, ui32CandidateName);
			}
		}
		while(psFound);

		pui32Names[i] = ui32CandidateName;
	}

	psNamesArray->ui32LastNameGenerated = ui32CandidateName;

	UNLOCK_NAMES_ARRAY(psNamesArray);

	if(psNamesArray->bGeneratedOnly)
	{
		GLES1NamedItem *psNewName;

		for(i=0; i < ui32Num; ++i)
		{
			psNewName = GLES1Calloc(gc, sizeof(GLES1NamedItem));
			psNewName->bGeneratedButUnused = IMG_TRUE;
			psNewName->ui32Name = pui32Names[i];

			InsertNamedItem(psNamesArray, psNewName);
		}
	}

	GLES1_TIME_STOP(GLES1_TIMER_NAMES_ARRAY);

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : NamesArrayMapFunction
 Inputs             : gc, psNamesArray, pfnMap, pvFunctionContext
 Outputs            : -
 Returns            : -
 Description        : Calls pfnMap on all the data items currently in the psNamesArray.
                      The function pfnMap must NOT lock the names array, or else a deadlock will occurs.
                      The graphics context gc and the function context are forwarded to each call.
 Limitations        : pfnMap _MUST_NOT_ try to lock the mutex pointed by psNamesArray->hSharedLock.
                      In particular, it must not call LOCK_NAMES_ARRAY.
************************************************************************************/
IMG_INTERNAL IMG_VOID NamesArrayMapFunction(GLES1Context *gc, GLES1NamesArray *psNamesArray,
						IMG_VOID (*pfnMap)(GLES1Context *gc, const IMG_VOID *pvFunctionContext, GLES1NamedItem* psNamedItem),
						const IMG_VOID *pvFunctionContext)
{
	IMG_UINT32     i;
	GLES1NamedItem *psNamedItem;

	GLES1_TIME_START(GLES1_TIMER_NAMES_ARRAY);

	LOCK_NAMES_ARRAY(psNamesArray);

	if(psNamesArray->ui32NumItems)
	{
		for(i=0; i < GLES1_DEFAULT_NAMES_ARRAY_SIZE; ++i)
		{
			psNamedItem = psNamesArray->apsEntry[i];

			while(psNamedItem)
			{
				if(!psNamedItem->bGeneratedButUnused)
				{
				    /* If this call causes a deadlock, someone didn't read the Limitations section of this function */
				    (*pfnMap)(gc, pvFunctionContext, psNamedItem);
				}
				psNamedItem = psNamedItem->psNext;
			}
		}
	}

	UNLOCK_NAMES_ARRAY(psNamesArray);

	GLES1_TIME_STOP(GLES1_TIMER_NAMES_ARRAY);
}


/***********************************************************************************
 Function Name      : DestroyNamesArray
 Inputs             : gc, psNamesArray
 Outputs            : -
 Returns            : -
 Description        : Destroys a namesarray - frees all items in it.
************************************************************************************/
IMG_INTERNAL IMG_VOID DestroyNamesArray(GLES1Context *gc, GLES1NamesArray *psNamesArray)
{
	IMG_UINT32     i;
	GLES1NamedItem *psNamedItem, *psNext;

	GLES1_TIME_START(GLES1_TIMER_NAMES_ARRAY);

	/* If any other thread tries to perform any operation in the array now there is a bug in the caller's code */

	/* Delete the contents at last */
	for(i=0; i < GLES1_DEFAULT_NAMES_ARRAY_SIZE; i++)
	{
		psNamedItem = psNamesArray->apsEntry[i];

		while(psNamedItem)
		{
			psNext = psNamedItem->psNext;
	
			if(psNamedItem->bGeneratedButUnused)
			{
				GLES1Free(gc, psNamedItem);
			}
			else
			{
			    /* IMPORTANT: These calls must be done while the array is UNLOCKED.  */
			    /*            The reason is that pfnFree() may try to lock the array */
			    psNamesArray->pfnFree(gc, psNamedItem, IMG_TRUE);
			}

			psNamedItem = psNext;
		}
	}

	GLES1Free(gc, psNamesArray);

	GLES1_TIME_STOP(GLES1_TIMER_NAMES_ARRAY);
}


/***********************************************************************************
 Function Name      : InsertNamedItem
 Inputs             : psNamesArray, psNamedItem
 Outputs            : -
 Returns            : IMG_TRUE if successfull. IMG_FALSE otherwise.
 Description        : Inserts a new named item in the names array.
                      The item and its name must be unique in the names array.
                      Its refcount is initialized automatically when it is inserted.
************************************************************************************/
IMG_INTERNAL IMG_BOOL InsertNamedItem(GLES1NamesArray *psNamesArray, GLES1NamedItem* psNamedItemToInsert)
{
	IMG_UINT32     ui32Position, ui32Name;
	GLES1NamedItem *psNamedItem, *psPrev = IMG_NULL;
	IMG_BOOL       bResult = IMG_TRUE;

	__GLES1_GET_CONTEXT_RETURN(IMG_FALSE);

	if(!psNamedItemToInsert || !psNamedItemToInsert->ui32Name)
	{
		return IMG_FALSE;
	}

	GLES1_TIME_START(GLES1_TIMER_NAMES_ARRAY);

	psNamedItemToInsert->ui32RefCount = 1;
	psNamedItemToInsert->psNext       = IMG_NULL;

	ui32Name = psNamedItemToInsert->ui32Name;
	ui32Position = BucketForName(ui32Name);

	GLES1_ASSERT(ui32Position < GLES1_DEFAULT_NAMES_ARRAY_SIZE);


	LOCK_NAMES_ARRAY(psNamesArray);
	/*
	 * When we insert an item we make sure that its name is unique.
	 * It's slightly slower than simply inserting it at the front of the list,
	 * but we can sleep better at night.
	 */
	psNamedItem = psNamesArray->apsEntry[ui32Position];

	if(!psNamedItem)
	{
		if(psNamesArray->bGeneratedOnly && !psNamedItemToInsert->bGeneratedButUnused)
		{
			/* There should have been a duplicate bGeneratedButUnused name. This must be a user-supplied name */
			bResult = IMG_FALSE;
		}
		else
		{
		    /* The list is empty. The name is necessarily unique */
		    psNamesArray->apsEntry[ui32Position] = psNamedItemToInsert;
		}
	}
	else
	{
		/* The list is not empty. Check that the name is unique */
		while(psNamedItem && psNamedItem->ui32Name != ui32Name)
		{
			psPrev      = psNamedItem;
			psNamedItem = psNamedItem->psNext;
		}

		/* Did we stop because we found a duplicate name? */
		if(psNamedItem)
		{
			if(psNamedItem->bGeneratedButUnused)
			{
				if(psPrev)
				{
					psPrev->psNext = psNamedItemToInsert;
					psNamedItemToInsert->psNext = psNamedItem->psNext;
				}
				else
				{
					psNamesArray->apsEntry[ui32Position] = psNamedItemToInsert;
				}
				GLES1Free(gc, psNamedItem);
			}
			else
			{
			    /* Yes, there's a duplicate. Do not insert. */
			    bResult = IMG_FALSE;
			}
		}
		else
		{
			if(psNamesArray->bGeneratedOnly && !psNamedItemToInsert->bGeneratedButUnused)
			{
				/* There should have been a duplicate bGeneratedButUnused name. This must be a user-supplied name */
				bResult = IMG_FALSE;
			}
			else
			{
			    /* Nop. There are no duplicates :) Insert as requested */
			    psPrev->psNext = psNamedItemToInsert;
			}
		}
	}

	if(bResult && !psNamedItemToInsert->bGeneratedButUnused)
	{
		/* The item was really inserted */
		psNamesArray->ui32NumItems++;
	}

	/* If the insertion fails, then this named item's RefCount is set back to 0 */
	if (!bResult)
	{
		psNamedItemToInsert->ui32RefCount = 0;
	}


	UNLOCK_NAMES_ARRAY(psNamesArray);

	GLES1_TIME_STOP(GLES1_TIMER_NAMES_ARRAY);

	return bResult;
}


/***********************************************************************************
 Function Name      : NamedItemAddRef
 Inputs             : psNamesArray, ui32Name
 Outputs            : -
 Returns            : The data associated with the given name
 Description        : Increments the reference counter of the given named item
************************************************************************************/
IMG_INTERNAL GLES1NamedItem* NamedItemAddRef(GLES1NamesArray *psNamesArray, IMG_UINT32 ui32Name)
{
	GLES1NamedItem *psNamedItem;

	__GLES1_GET_CONTEXT_RETURN(IMG_NULL);

	GLES1_TIME_START(GLES1_TIMER_NAMES_ARRAY);

	LOCK_NAMES_ARRAY(psNamesArray);

	psNamedItem = LookupItemByName(psNamesArray, ui32Name);

	if(psNamedItem)
	{
		if(psNamedItem->bGeneratedButUnused)
		{
			UNLOCK_NAMES_ARRAY(psNamesArray);
			GLES1_TIME_STOP(GLES1_TIMER_NAMES_ARRAY);
			return IMG_NULL;
		}

		psNamedItem->ui32RefCount++;
	} /* ignore silently items not found */

	UNLOCK_NAMES_ARRAY(psNamesArray);

	GLES1_TIME_STOP(GLES1_TIMER_NAMES_ARRAY);

	return psNamedItem;
}


/***********************************************************************************
 Function Name      : NamedItemDelRef
 Inputs             : gc, psNamesArray, psNamedItem
 Outputs            : -
 Returns            : -
 Description        : Decrements the reference counter of the given named item.
                      Deletes the item if its refcount drops to zero.
************************************************************************************/
IMG_INTERNAL IMG_VOID NamedItemDelRef(GLES1Context *gc, GLES1NamesArray *psNamesArray, GLES1NamedItem* psNamedItem)
{
	GLES1_TIME_START(GLES1_TIMER_NAMES_ARRAY);

	LOCK_NAMES_ARRAY(psNamesArray);

	GLES1_ASSERT(psNamedItem->ui32Name);
	GLES1_ASSERT(psNamedItem->ui32RefCount > 0);

	if(psNamedItem->ui32RefCount == 1)
	{
		/* The item must be deleted. Remove it from the list */
		psNamedItem->ui32RefCount = 0;
		RemoveItemFromList(psNamesArray, psNamedItem);
	}
	else if(psNamedItem->ui32RefCount > 1)
	{
		/* Simply decrement the reference count */
		psNamedItem->ui32RefCount--;
		psNamedItem = IMG_NULL;
	}

	UNLOCK_NAMES_ARRAY(psNamesArray);

	GLES1_TIME_STOP(GLES1_TIMER_NAMES_ARRAY);

	/* Delete the data if its refcount dropped to zero */
	if(psNamedItem)
	{
		psNamesArray->pfnFree(gc, psNamedItem, IMG_FALSE);
	}
}


/***********************************************************************************
 Function Name      : NamedItemDelRefByName
 Inputs             : gc, psNamesArray, ui32Num, ui32Name
 Outputs            : -
 Returns            : -
 Description        : Decreases the refcount of a list of items given their names.
                      The objects are deleted if their refcount drops to zero.
                      The names are removed from the list whether the objects are deleted or not.
                      The array ui32Name must have length ui32Num.
                      Names that do not match an object in the array are ignored silently.
************************************************************************************/
IMG_INTERNAL IMG_VOID NamedItemDelRefByName(GLES1Context *gc, GLES1NamesArray *psNamesArray,
							   IMG_UINT32 ui32Num, const IMG_UINT32 ui32Name[/*ui32Num*/])
{
	IMG_UINT32      i;
	GLES1NamedItem  *psNamedItem, *psNext, *psDeadMan = IMG_NULL;

	GLES1_TIME_START(GLES1_TIMER_NAMES_ARRAY);

	LOCK_NAMES_ARRAY(psNamesArray);

	for(i=0; i < ui32Num; ++i)
	{
		psNamedItem = LookupItemByName(psNamesArray, ui32Name[i]);

		if(psNamedItem)
		{
			GLES1_ASSERT(psNamedItem->ui32RefCount > 0);
			/* Remove the name from the list even if the object is not going to be deleted */
			RemoveItemFromList(psNamesArray, psNamedItem);

			if(psNamedItem->ui32RefCount == 1)
			{
				/* The item must be deleted. Append it to the list of dead items
				 */
				psNamedItem->ui32RefCount = 0;

				psNamedItem->psNext = psDeadMan;
				psDeadMan = psNamedItem;
			}
			else if(psNamedItem->ui32RefCount > 1)
			{
				psNamedItem->ui32RefCount--;
			}
		}
	}

	UNLOCK_NAMES_ARRAY(psNamesArray);

	GLES1_TIME_STOP(GLES1_TIMER_NAMES_ARRAY);

	while(psDeadMan)
	{
		psNext = psDeadMan->psNext;
		if(psDeadMan->bGeneratedButUnused)
		{
			GLES1Free(gc, psDeadMan);
		}
		else
		{
		    psNamesArray->pfnFree(gc, psDeadMan, IMG_FALSE);
		}
		psDeadMan = psNext;
	}
}


/******************************************************************************
 End of file (names.c)
******************************************************************************/
