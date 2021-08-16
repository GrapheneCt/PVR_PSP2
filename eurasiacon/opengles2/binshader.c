/******************************************************************************
 * Name         : binshader.c
 * Created      : 6/07/2006
 *
 * Copyright    : 2006-2010 by Imagination Technologies Limited.
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
 * $Log: binshader.c $
 *****************************************************************************/
#if defined(SUPPORT_BINARY_SHADER) || defined(GLES2_EXTENSION_GET_PROGRAM_BINARY)

#include "context.h"
#include "binshader.h"
#include <string.h>
#include "pvrversion.h"

#include "esbinshaderinternal.h"
#include "usp.h"
#include "glsl.h"

#define SGXBS_DEFAULT_ALLOC_BUFFER_SIZE		64

#define SGXBS_VER_SEPERATOR					((IMG_UINT16) 0xFFFFU)

/*
 * This data structure has two functions:
 * 1- Keep track of the status of a read-only buffer in the unpacking functions.
 * 2- Keep track of memory allocations made by the unpacking functions.
 */
typedef struct SGXBS_BufferTAG
{
	const IMG_UINT8*   pu8Buffer;
	IMG_UINT32          u32CurrentPosition;
	IMG_UINT32          u32BufferSizeInBytes;
	IMG_BOOL            bOverflow;
	GLES2Context        *gc;

	/* Variables to keep track of allocated memory (used for freeing mem in case of error) */
	/* These should only be accessed by SGXBS_Calloc and SGXBS_FreeAllocatedMemory */

	IMG_VOID     **apvAllocatedMemory;
	IMG_UINT32   u32NumMemoryAllocations;
	IMG_UINT32   u32MaxMemoryAllocations;

} SGXBS_Buffer;

static IMG_VOID GetCoreAndRevisionNumber(IMG_UINT32 * u32ExpectedCore, IMG_UINT32 * u32ExpectedCoreRevision)
{
	/* Look at $(EURASIAROOT)/hwdefs/sgxerrata.h to see the which cores have been released */
	#if defined(SGX520)
		*u32ExpectedCore = SGXBS_CORE_520;
	#else
		#if defined(SGX530)
			*u32ExpectedCore = SGXBS_CORE_530;
		#else
			#if defined(SGX531)
					*u32ExpectedCore = SGXBS_CORE_531;
			#else
				#if defined(SGX535) || defined(SGX535_V1_1)
					*u32ExpectedCore = SGXBS_CORE_535;
				#else							
					#if defined(SGX540)
						*u32ExpectedCore = SGXBS_CORE_540;
					#else
						#if defined(SGX541)
							*u32ExpectedCore = SGXBS_CORE_541;
						#else
							#if defined(SGX543)
								*u32ExpectedCore = SGXBS_CORE_543;
							#else
								#if defined(SGX544)
									*u32ExpectedCore = SGXBS_CORE_544;
								#else
									#if defined(SGX545)
										*u32ExpectedCore = SGXBS_CORE_545;
									#else
										#if defined(SGX554)
											*u32ExpectedCore = SGXBS_CORE_554;
										#else
											#error "Unknown major SGX core version"
										#endif
									#endif
								#endif
							#endif
						#endif
					#endif
				#endif
			#endif
		#endif
	#endif

	#undef SGXBS_CORE_REVISION
	#if SGX_CORE_REV == 100
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_100
	#endif
	#if SGX_CORE_REV == 101
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_101
	#endif
	#if SGX_CORE_REV == 102
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_102
	#endif
	#if SGX_CORE_REV == 103
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_103
	#endif
	#if SGX_CORE_REV == 104
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_104
	#endif
	#if SGX_CORE_REV == 105
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_105
	#endif
	#if SGX_CORE_REV == 105
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_105
	#endif
	#if SGX_CORE_REV == 106
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_106
	#endif
	#if SGX_CORE_REV == 107
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_107
	#endif
	#if SGX_CORE_REV == 108
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_108
	#endif
	#if SGX_CORE_REV == 109
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_109
	#endif
	#if SGX_CORE_REV == 1012
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_1012
	#endif
	#if SGX_CORE_REV == 1013
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_1013
	#endif
	#if SGX_CORE_REV == 10131
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_10131
	#endif
	#if SGX_CORE_REV == 1014
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_1014
	#endif
	#if SGX_CORE_REV == 110
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_110
	#endif
	#if SGX_CORE_REV == 111
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_111
	#endif
	#if SGX_CORE_REV == 1111
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_1111
	#endif
	#if SGX_CORE_REV == 112
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_112
	#endif
	#if SGX_CORE_REV == 113
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_113
	#endif
	#if SGX_CORE_REV == 114
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_114
	#endif
	#if SGX_CORE_REV == 115
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_115
	#endif
	#if SGX_CORE_REV == 116
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_116
	#endif
	#if SGX_CORE_REV == 117
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_117
	#endif
	#if SGX_CORE_REV == 120
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_120
	#endif
	#if SGX_CORE_REV == 121
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_121
	#endif
	#if SGX_CORE_REV == 122
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_122
	#endif
	#if SGX_CORE_REV == 1221
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_1221
	#endif
	#if SGX_CORE_REV == 123
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_123
	#endif
	#if SGX_CORE_REV == 124
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_124
	#endif
	#if SGX_CORE_REV == 125
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_125
	#endif
	#if SGX_CORE_REV == 1251
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_1251
	#endif
	#if SGX_CORE_REV == 126
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_126
	#endif
	#if SGX_CORE_REV == 130
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_130
	#endif
	#if SGX_CORE_REV == 140
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_140
	#endif
	#if SGX_CORE_REV == 1401
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_1401
	#endif
	#if SGX_CORE_REV == 141
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_141
	#endif
	#if SGX_CORE_REV == 142
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_142
	#endif
	#if SGX_CORE_REV == 211
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_211
	#endif
	#if SGX_CORE_REV == 2111
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_2111
	#endif
	#if SGX_CORE_REV == 213
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_213
	#endif
	#if SGX_CORE_REV == 216
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_216
	#endif
	#if SGX_CORE_REV == 302
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_302
	#endif
	#if SGX_CORE_REV == 303
	#define SGXBS_CORE_REVISION SGXBS_CORE_REVISION_303
	#endif

	#if SGX_CORE_REV == SGX_CORE_REV_HEAD
	#define SGXBS_CORE_REVISION SGX_CORE_REV_HEAD
	#endif

	#ifndef SGXBS_CORE_REVISION
	#error "Unknown core revision. Update me."
	#endif

	*u32ExpectedCoreRevision = SGXBS_CORE_REVISION;
}

/***********************************************************************************
 Function Name      : SGXBS_ValidateHWRevision
 Inputs             : u32Core - SGX core version being checked
                      u32CoreRevision - SGX core revision being checked
					  u32ExpectedCore - expected SGX core version to refer
					  u32ExpectedCoreRevision - expected SGX core revision to refer
 Outputs            : -
 Returns            : IMG_TRUE if the (u32Core, u32CoreRevision) is compatible with (u32ExpectedCore, u32ExpectedCoreRevision), otherwise, IMG_FALSE
 Description        : validate if the hardware component revision of a BS revision is compatible with in-use hardware
					  
					  SGX_CORE_REV_HEAD is NOT checked in this function.
************************************************************************************/
static IMG_BOOL SGXBS_ValidateHWRevision(IMG_UINT32 u32Core, IMG_UINT32 u32CoreRevision, 
										 IMG_UINT32 u32ExpectedCore, IMG_UINT32 u32ExpectedCoreRevision)
{
	/* BS HW revision table	where: 
	   Every CORE version is ended up with a SGXBS_VER_SEPERATOR, which is 0xFFFF.
       Every CORE version has no less than zero number of group of different revisions.  Every revision group is ended with SGXBS_VER_SEPERATOR.
       The revisions inside the same revision group must have the same hardware struture in terms of binary shaders.

	   refer to eurasia/hwdefs/sgxerrata.h
	 */

	IMG_UINT16 SGXBS_HW_VerTable_V0[] = {	SGXBS_CORE_520,		/* 520 */
												SGXBS_CORE_REVISION_100, SGXBS_VER_SEPERATOR, 
											SGXBS_VER_SEPERATOR,
											SGXBS_CORE_530,		/* 530 */
												SGXBS_CORE_REVISION_110, SGXBS_VER_SEPERATOR, 
												SGXBS_CORE_REVISION_111, SGXBS_VER_SEPERATOR, 
												SGXBS_CORE_REVISION_1111, SGXBS_VER_SEPERATOR, 
												SGXBS_CORE_REVISION_120, SGXBS_CORE_REVISION_121, SGXBS_CORE_REVISION_125, SGXBS_VER_SEPERATOR, 
											SGXBS_VER_SEPERATOR,
											SGXBS_CORE_531,		/* 531 */
												SGXBS_CORE_REVISION_101, SGXBS_VER_SEPERATOR, 
											SGXBS_VER_SEPERATOR,
											SGXBS_CORE_535,		/* 535 */
												SGXBS_CORE_REVISION_111, SGXBS_VER_SEPERATOR, 
												SGXBS_CORE_REVISION_1111, SGXBS_VER_SEPERATOR, 
												SGXBS_CORE_REVISION_112, SGXBS_VER_SEPERATOR, 
												SGXBS_CORE_REVISION_113, SGXBS_VER_SEPERATOR, 
												SGXBS_CORE_REVISION_121, SGXBS_VER_SEPERATOR, 
												SGXBS_CORE_REVISION_126, SGXBS_VER_SEPERATOR,
											SGXBS_VER_SEPERATOR,
											SGXBS_CORE_540,		/* 540 */
												SGXBS_CORE_REVISION_101, SGXBS_VER_SEPERATOR, 
												SGXBS_CORE_REVISION_110, SGXBS_VER_SEPERATOR, 
												SGXBS_CORE_REVISION_120, SGXBS_VER_SEPERATOR,
												SGXBS_CORE_REVISION_121, SGXBS_VER_SEPERATOR,
												SGXBS_CORE_REVISION_125, SGXBS_VER_SEPERATOR, 
											SGXBS_VER_SEPERATOR,
											SGXBS_CORE_543,		/* 543 */
												SGXBS_CORE_REVISION_113, SGXBS_CORE_REVISION_117, SGXBS_VER_SEPERATOR, 
												SGXBS_CORE_REVISION_140, SGXBS_CORE_REVISION_1401, SGXBS_VER_SEPERATOR, 
												SGXBS_CORE_REVISION_141, SGXBS_VER_SEPERATOR, 
											SGXBS_VER_SEPERATOR,
											SGXBS_CORE_544,		/* 544 */
												SGXBS_CORE_REVISION_102, SGXBS_VER_SEPERATOR, 
												SGXBS_CORE_REVISION_105, SGXBS_VER_SEPERATOR, 
											SGXBS_VER_SEPERATOR,
											SGXBS_CORE_545,		/* 545 */
												SGXBS_CORE_REVISION_109, SGXBS_VER_SEPERATOR, 
												SGXBS_CORE_REVISION_1012, SGXBS_VER_SEPERATOR, 
												SGXBS_CORE_REVISION_1013, SGXBS_VER_SEPERATOR, 
											SGXBS_VER_SEPERATOR
										};

	
	IMG_INT32 i = 0;
	IMG_INT32 size =  sizeof(SGXBS_HW_VerTable_V0)/sizeof(IMG_UINT16) - 1;
	IMG_INT32 groupIndex, expectedGroupIndex, currentGroupIndex;
	IMG_BOOL foundGroupIndex, foundExpectedGroupIndex;

	IMG_UINT16 u16Core, u16CoreRevision, u16ExpectedCoreRevision;

	u16Core					= (IMG_UINT16)u32Core;
	u16CoreRevision			= (IMG_UINT16)u32CoreRevision;
	u16ExpectedCoreRevision	= (IMG_UINT16)u32ExpectedCoreRevision;

	/* the core version must be the expected core version */
	if (u32Core != u32ExpectedCore)
	{
		PVR_DPF((PVR_DBG_WARNING,"UnpackRevision: The SGX core flag is 0x%X but we expected 0x%X.", 
			u32Core, u32ExpectedCore));
		return IMG_FALSE;
	}

	/* if two revsion numbers are the same, they are just a match */
	if (u32CoreRevision == u32ExpectedCoreRevision)
	{
		return IMG_TRUE;
	}

	/* try to find if the revisions come from the same revision group under the same core version 
	   by iterating the version table SGXBS_HW_VerTable_V0
	 */
	while (i < size)
	{
		if (SGXBS_HW_VerTable_V0[i] == u16Core)
		{
			/* If the core version number is founded, search through its revision groups */
			i += 1;
			if (SGXBS_HW_VerTable_V0[i] == SGXBS_VER_SEPERATOR)
			{
				PVR_DPF((PVR_DBG_ERROR,"UnpackRevision: Corrupt binary shader version table (SGXBS_HW_VerTable_V0).  No revision flag in version 0x%X.", u32Core));
				return IMG_FALSE;
			}
			else
			{
				groupIndex = expectedGroupIndex = -1;
				currentGroupIndex = 0;
				foundGroupIndex = foundExpectedGroupIndex = IMG_FALSE;
				while (i < size)
				{
					if (SGXBS_HW_VerTable_V0[i] == u16CoreRevision)
					{
						if (!foundGroupIndex)
						{
							groupIndex = currentGroupIndex;
							foundGroupIndex = IMG_TRUE;
						}
						else
						{
							PVR_DPF((PVR_DBG_ERROR,"UnpackRevision: Corrupt binary shader version table (SGXBS_HW_VerTable_V0).  Revision 0x%X in version 0x%X from groups %d and %d.", 
								u32CoreRevision, u32Core, groupIndex, currentGroupIndex));
							return IMG_FALSE;
						}
					}
					else if (SGXBS_HW_VerTable_V0[i] == u16ExpectedCoreRevision)
					{
						if (!foundExpectedGroupIndex)
						{
							expectedGroupIndex = currentGroupIndex;
							foundExpectedGroupIndex = IMG_TRUE;
						}
						else
						{
							PVR_DPF((PVR_DBG_ERROR,"UnpackRevision: Corrupt binary shader version table (SGXBS_HW_VerTable_V0).  Revision 0x%X in version 0x%X from groups %d and %d.", 
								u32ExpectedCoreRevision, u32Core, groupIndex, currentGroupIndex));
							return IMG_FALSE;
						}
					}
					else if (SGXBS_HW_VerTable_V0[i] == SGXBS_VER_SEPERATOR)
					{
						currentGroupIndex += 1;
					}
					
					if (foundGroupIndex && foundExpectedGroupIndex)
					{
						if (groupIndex == expectedGroupIndex)
						{
							/* a pair of matched revisions is found */
							return IMG_TRUE;
						}
						else
						{
							/* cannot find the compatible BS hardware revisions */
							PVR_DPF((PVR_DBG_ERROR,"UnpackRevision: The SGX core revision flag is 0x%X and we expected 0x%X, which are not compatible revisions.", 
								u32CoreRevision, u32ExpectedCoreRevision));
							return IMG_FALSE;
						}
					}
					else
					{
						if (SGXBS_HW_VerTable_V0[i] == SGXBS_VER_SEPERATOR && SGXBS_HW_VerTable_V0[i+1] == SGXBS_VER_SEPERATOR)
						{
							/* end of the revisions under the core version */
							PVR_DPF((PVR_DBG_ERROR, "UnpackRevision: At least one of the SGX core revision flags 0x%X and 0x%X in version 0x%X cannot be found in the binary shader version table (SGXBS_HW_VerTable_V0) \n", 
								u32CoreRevision, u32ExpectedCoreRevision, u32Core));
							return IMG_FALSE;
						}

						i += 1;
					}
				}

				/* end of the reveision groups in this core version is reached,
				   cannot find the compatible BS hardware revisions
				 */
				if (!foundGroupIndex || !foundExpectedGroupIndex)
				{
					PVR_DPF((PVR_DBG_ERROR,"UnpackRevision: Corrupt binary shader version table (SGXBS_HW_VerTable_V0).  Cannot find the SGX core revision flag 0x%X in version 0x%X.", 
						foundGroupIndex ? u32ExpectedCoreRevision : u32CoreRevision, u32Core));
				}
				else
				{
					PVR_DPF((PVR_DBG_ERROR,"UnpackRevision: The SGX core revision flag is 0x%X and we expected 0x%X, which are not compatible revisions.", 
						u32CoreRevision, u32ExpectedCoreRevision));
				}

				return IMG_FALSE;
			}
		}
		else
		{
			/* If the core version number cannot be founded, locate to the next core version number */
			i += 1;
			if (SGXBS_HW_VerTable_V0[i] == SGXBS_VER_SEPERATOR)
			{
				i += 1;
			}
			else
			{
				while (i < size)
				{
					if (SGXBS_HW_VerTable_V0[i] == SGXBS_VER_SEPERATOR && SGXBS_HW_VerTable_V0[i + 1] == SGXBS_VER_SEPERATOR)
					{
						i += 2;
						break;
					}
					else
					{
						i += 1;
					}
				}
				if (i == size)
				{
					PVR_DPF((PVR_DBG_ERROR,"UnpackRevision: Corrupt binary shader version table (SGXBS_HW_VerTable_V0).  Forget a SGXBS_VER_SEPERATOR between core versions?"));
					return IMG_FALSE;
				}
			}
		}
	}

	/* cannot find the compatible BS hardware revisions */
	PVR_DPF((PVR_DBG_ERROR,"UnpackRevision: Corrupt binary shader version table (SGXBS_HW_VerTable_V0). \
						   The SGX core version flag is 0x%X which is not listed in binary shader revision table(SGXBS_HW_VerTable_V0), \
						   or the table lacks a SGXBS_VER_SEPERATOR before this version if it is presented in the table.\n", 
						   u32CoreRevision));
	return IMG_FALSE;
}

/***********************************************************************************
 Function Name      : SGXBS_Calloc
 Inputs             : u32NumBytes - Number of bytes to allocate
                      psBuffer    - Buffer that remembers the allocated blocks
 Outputs            : -
 Returns            : A block of zeroed memory of u32NumBytes size or IMG_NULL if the allocation failed.
 Description        : UTILITY: Allocates some zeroed memory for use in the binary shader decoder.
************************************************************************************/
static IMG_VOID* SGXBS_Calloc(IMG_UINT32 u32NumBytes, SGXBS_Buffer* psBuffer)
{
	IMG_VOID*  pvNewAlloc;
	IMG_UINT32 u32NewMaxMemoryAllocations;
	IMG_VOID   **apvNewBuffer;

	if(!u32NumBytes)
	{
		return IMG_NULL;
	}

	pvNewAlloc = GLES2Calloc(psBuffer->gc, u32NumBytes);

	if (!pvNewAlloc)
	{
		PVR_DPF((PVR_DBG_ERROR,"SGXBS_Calloc: Calloc failed. Returning NULL"));
		return IMG_NULL;
	}

	if(psBuffer->u32NumMemoryAllocations == psBuffer->u32MaxMemoryAllocations)
	{
		/* Enlarge the buffer */
		u32NewMaxMemoryAllocations = psBuffer->u32MaxMemoryAllocations << 1;
		apvNewBuffer = GLES2Realloc(psBuffer->gc, psBuffer->apvAllocatedMemory, u32NewMaxMemoryAllocations*sizeof(IMG_VOID*));

		if(apvNewBuffer)
		{
			psBuffer->apvAllocatedMemory      = apvNewBuffer;
			psBuffer->u32MaxMemoryAllocations = u32NewMaxMemoryAllocations;

			psBuffer->apvAllocatedMemory[psBuffer->u32NumMemoryAllocations++] = pvNewAlloc;
		}
		else
		{
			/* Realloc failed. Free the block that was just allocated and return NULL */
			PVR_DPF((PVR_DBG_ERROR,"SGXBS_Calloc: Realloc failed. Returning NULL"));
			GLES2Free(psBuffer->gc, pvNewAlloc);
			pvNewAlloc = IMG_NULL;
		}
	}
	else
	{
		psBuffer->apvAllocatedMemory[psBuffer->u32NumMemoryAllocations++] = pvNewAlloc;
	}

	return pvNewAlloc;
}


/***********************************************************************************
 Function Name      : SGXBS_FreeAllocatedMemory
 Inputs             : psBuffer    - Buffer that remembers the allocated blocks
 Outputs            : -
 Returns            : Frees all blocks that were allocated in psBuffer.
 Description        : UTILITY: Frees all memory allocated via psBuffer.
************************************************************************************/
static void SGXBS_FreeAllocatedMemory(SGXBS_Buffer* psBuffer)
{
	while(psBuffer->u32NumMemoryAllocations)
	{
		GLES2Free(psBuffer->gc, psBuffer->apvAllocatedMemory[--psBuffer->u32NumMemoryAllocations]);
	}
}


/***********************************************************************************
 Function Name      : ReadString
 Inputs             : psBuffer    - Buffer that stores the data.
 Outputs            : ppszString  - Pointer to a null-terminated string where the result is saved.
 Returns            : SGXBS_NO_ERROR if successfull. SGXBS_OUT_OF_MEMORY_ERROR otherwise.
 Description        : Reads a string from the buffer, allocates memory for ppszString and copies the string into it.
                      Increments the current buffer position appropriately.
************************************************************************************/
static SGXBS_Error ReadString(SGXBS_Buffer *psBuffer, IMG_CHAR **ppszString)
{
	IMG_UINT32 ui32Length = 0;
	IMG_CHAR   cLastChar = 1;

	/* Determine the length of the string */
	while(cLastChar && psBuffer->u32CurrentPosition + ui32Length < psBuffer->u32BufferSizeInBytes)
	{
		cLastChar = (IMG_CHAR)(psBuffer->pu8Buffer[psBuffer->u32CurrentPosition + ui32Length++]);
	}

	if(cLastChar)
	{
		/* We went out of the loop because we reached the end of the buffer */
		if(!psBuffer->bOverflow)
		{
			PVR_DPF((PVR_DBG_ERROR,"ReadString: Buffer overflow"));
		}
		psBuffer->bOverflow = IMG_TRUE;
	}
	else
	{
		/* We found the end of the string */
		*ppszString = SGXBS_Calloc(ui32Length, psBuffer);

		if(!*ppszString)
		{
			return SGXBS_OUT_OF_MEMORY_ERROR;
		}

		memcpy(*ppszString, &psBuffer->pu8Buffer[psBuffer->u32CurrentPosition], ui32Length);
		psBuffer->u32CurrentPosition += ui32Length;
	}

	return SGXBS_NO_ERROR;
}


/***********************************************************************************
 Function Name      : ReadU8
 Inputs             : psBuffer    - Buffer that stores the data.
 Outputs            : -
 Returns            : The 8-bit unsigned int that was read from the buffer.
 Description        : Reads a 8-bit unsigned integer from the buffer.
                      Increments the current buffer position appropriately.
************************************************************************************/
static IMG_UINT8 ReadU8(SGXBS_Buffer *psBuffer)
{
	IMG_UINT8 ui8Result = 0;

	if(psBuffer->u32CurrentPosition + 1 <= psBuffer->u32BufferSizeInBytes)
	{
		/* The buffer won't overflow if the data is read */
		ui8Result = psBuffer->pu8Buffer[psBuffer->u32CurrentPosition];
		psBuffer->u32CurrentPosition += 1;
	}
	else
	{
		if(!psBuffer->bOverflow)
		{
			PVR_DPF((PVR_DBG_ERROR,"ReadU8: Buffer overflow"));
		}
		psBuffer->bOverflow = IMG_TRUE;
	}

	return ui8Result;
}


/***********************************************************************************
 Function Name      : ReadU16
 Inputs             : psBuffer    - Buffer that stores the data.
 Outputs            : -
 Returns            : The 16-bit unsigned int that was read from the buffer.
 Description        : Reads a 16-bit unsigned integer from the buffer.
                      Increments the current buffer position appropriately.
************************************************************************************/
static IMG_UINT16 ReadU16(SGXBS_Buffer *psBuffer)
{
	IMG_UINT16 u16Result = 0;

	if(psBuffer->u32CurrentPosition + 2 <= psBuffer->u32BufferSizeInBytes)
	{
		/* The buffer won't overflow if the data is read */
		u16Result = ((IMG_UINT16)psBuffer->pu8Buffer[psBuffer->u32CurrentPosition+0] << 8) |
		            ((IMG_UINT16)psBuffer->pu8Buffer[psBuffer->u32CurrentPosition+1]     );
		psBuffer->u32CurrentPosition += 2;
	}
	else
	{
		if(!psBuffer->bOverflow)
		{
			PVR_DPF((PVR_DBG_ERROR,"ReadU16: Buffer overflow"));
		}
		psBuffer->bOverflow = IMG_TRUE;
	}

	return u16Result;
}


/***********************************************************************************
 Function Name      : ReadU32
 Inputs             : psBuffer    - Buffer that stores the data.
 Outputs            : -
 Returns            : The 32-bit unsigned int that was read from the buffer.
 Description        : Reads a 32-bit unsigned integer from the buffer.
                      Increments the current buffer position appropriately.
************************************************************************************/
static IMG_UINT32 ReadU32(SGXBS_Buffer *psBuffer)
{
	IMG_UINT32 u32Result = 0;

	if(psBuffer->u32CurrentPosition + 4 <= psBuffer->u32BufferSizeInBytes)
	{
		/* The buffer won't overflow if the data is read */
		u32Result = ((IMG_UINT32)psBuffer->pu8Buffer[psBuffer->u32CurrentPosition+0] << 24) |
		            ((IMG_UINT32)psBuffer->pu8Buffer[psBuffer->u32CurrentPosition+1] << 16) |
		            ((IMG_UINT32)psBuffer->pu8Buffer[psBuffer->u32CurrentPosition+2] <<  8) |
		            ((IMG_UINT32)psBuffer->pu8Buffer[psBuffer->u32CurrentPosition+3]      );
		psBuffer->u32CurrentPosition += 4;
	}
	else
	{
		if(!psBuffer->bOverflow)
		{
			PVR_DPF((PVR_DBG_ERROR,"ReadU32: Buffer overflow"));
		}
		psBuffer->bOverflow = IMG_TRUE;
	}

	return u32Result;
}


/***********************************************************************************
 Function Name      : ReadFloat
 Inputs             : psBuffer    - Buffer that stores the data.
 Outputs            : -
 Returns            : The 32-bit float that was read from the buffer.
 Description        : Reads a 32-bit float from the buffer.
                      Increments the current buffer position appropriately.
************************************************************************************/
static IMG_FLOAT ReadFloat(SGXBS_Buffer *psBuffer)
{
	union { IMG_UINT32 u32; IMG_FLOAT f; } u;

	u.f = 0;

	if(psBuffer->u32CurrentPosition + 4 <= psBuffer->u32BufferSizeInBytes)
	{
		/* The buffer won't overflow if the data is read */
		u.u32 = ((IMG_UINT32)psBuffer->pu8Buffer[psBuffer->u32CurrentPosition+0] << 24) |
		        ((IMG_UINT32)psBuffer->pu8Buffer[psBuffer->u32CurrentPosition+1] << 16) |
		        ((IMG_UINT32)psBuffer->pu8Buffer[psBuffer->u32CurrentPosition+2] <<  8) |
		        ((IMG_UINT32)psBuffer->pu8Buffer[psBuffer->u32CurrentPosition+3]      );
		psBuffer->u32CurrentPosition += 4;
	}
	else
	{
		if(!psBuffer->bOverflow)
		{
			PVR_DPF((PVR_DBG_ERROR,"ReadFloat: Buffer overflow"));
		}
		psBuffer->bOverflow = IMG_TRUE;
	}

	return u.f;
}


/***********************************************************************************
 Function Name      : ReadArrayHeader
 Inputs             : psBuffer    - Buffer that stores the data.
 Outputs            : -
 Returns            : The number of elements in the array.
 Description        : Reads the current position of the buffer and interprets it as an array header.
                      Increments the current buffer position appropriately.
************************************************************************************/
static IMG_UINT16 ReadArrayHeader(SGXBS_Buffer *psBuffer)
{
	/* An array header is just a 16-bit unsigned integer */
	return ReadU16(psBuffer);
}


/***********************************************************************************
 Function Name      : UnpackSymbolBindings
 Inputs             : psBuffer    - Buffer that stores the data.
 Outputs            : ppsSymbols  - Array of symbols that have been read.
                      pu32NumSymbols - Number of elements in ppsSymbols.
 Returns            : SGXBS_NO_ERROR if successfull. Some other value otherwise.
 Description        : Reads the tree of symbol bindings from the buffer recursively.
                      Increments the current buffer position appropriately.
************************************************************************************/
static SGXBS_Error UnpackSymbolBindings(GLSLBindingSymbol *ppsSymbols[/* *pu32NumSymbols */],
	IMG_UINT32 *pu32NumSymbols, SGXBS_Buffer *psBuffer)
{
	SGXBS_Error       eError;
	GLSLBindingSymbol *psSymbols;
	IMG_UINT32        u32NumSymbols, i;

	/* Read the array header */
	u32NumSymbols = ReadArrayHeader(psBuffer);

	/* Alloc space for the symbols */
	psSymbols = SGXBS_Calloc(sizeof(GLSLBindingSymbol)*u32NumSymbols, psBuffer);

	if(u32NumSymbols && !psSymbols)
	{
		return SGXBS_OUT_OF_MEMORY_ERROR;
	}

	*pu32NumSymbols = u32NumSymbols;
	*ppsSymbols = psSymbols;

	/* Read the array data */
	for(i=0; i < u32NumSymbols; ++i)
	{
		/* 01- Name of the symbol */
		eError = ReadString(psBuffer, &psSymbols->pszName);

		if(eError != SGXBS_NO_ERROR)
		{
			return eError;
		}

		/* 02- eBIVariableID */
		psSymbols->eBIVariableID = (GLSLBuiltInVariableID) ReadU16(psBuffer);	/* PRQA S 1482 */ /* Override enum warning. */

		/* 03- eTypeSpecifier */
		psSymbols->eTypeSpecifier =(GLSLTypeSpecifier)  ReadU8(psBuffer);	/* PRQA S 1482 */ /* Override enum warning. */

		/* 04- eTypeQualifier */
		psSymbols->eTypeQualifier = (GLSLTypeQualifier) ReadU8(psBuffer);	/* PRQA S 1482 */ /* Override enum warning. */
	
		/* 05- ePrecisionQualifier */
		psSymbols->ePrecisionQualifier =(GLSLPrecisionQualifier)  ReadU8(psBuffer);	/* PRQA S 1482 */ /* Override enum warning. */

		/* 06- eVaryingModifierFlags */
		psSymbols->eVaryingModifierFlags = (GLSLVaryingModifierFlags) ReadU8(psBuffer);	/* PRQA S 1482 */ /* Override enum warning. */

		/* 07- u16ActiveArraySize */
		psSymbols->iActiveArraySize = (IMG_INT32)ReadU16(psBuffer);	/* PRQA S 1482 */ /* Override enum warning. */

		/* 08- u16DeclaredArraySize */
		psSymbols->iDeclaredArraySize = (IMG_INT32)ReadU16(psBuffer);	/* PRQA S 1482 */ /* Override enum warning. */

		/* 09- eRegType */
		psSymbols->sRegisterInfo.eRegType = (GLSLHWRegType) ReadU8(psBuffer);	/* PRQA S 1482 */ /* Override enum warning. */

		/* 10- union { u16BaseComp, u16TextureUnit } */
		psSymbols->sRegisterInfo.u.uBaseComp = ReadU16(psBuffer);

		/* 11- u8CompAllocCount */
		psSymbols->sRegisterInfo.uCompAllocCount = ReadU8(psBuffer);

		/* 12- u32CompUseMask */
		psSymbols->sRegisterInfo.ui32CompUseMask = ReadU16(psBuffer);

		/* 13- sBaseTypeMembersArray */
		eError = UnpackSymbolBindings(&psSymbols->psBaseTypeMembers, &psSymbols->uNumBaseTypeMembers, psBuffer);	/* PRQA S 3670 */ /* Override QAC suggestion and use recursive call. */

		if(eError != SGXBS_NO_ERROR)
		{
			return eError;
		}

		psSymbols++;
	}

	return SGXBS_NO_ERROR;
}

/***********************************************************************************
 Function Name      : UnpackUserBindings
 Inputs             : psBuffer          - Buffer that stores the data.
                      pvUniPatchContext - Context for UniPatch.
 Outputs            : ppsUserBinding    - GLSLAttribUserBinding output.
 Returns            : SGXBS_NO_ERROR if successfull. Some other value otherwise.
 Description        : read and create attrib user binding list - this is part
						of the program object stete
************************************************************************************/
static SGXBS_Error UnpackUserBindings(GLSLAttribUserBinding **ppsUserBinding, IMG_VOID *pvUniPatchContext, SGXBS_Buffer* psBuffer)
{
	SGXBS_Error eError;
	IMG_UINT32 i, u32NumBindings;
	GLSLAttribUserBinding * psUserBinding, * psLastUserBinding = IMG_NULL;

	PVR_UNREFERENCED_PARAMETER(pvUniPatchContext);

	/* Read the array header */
	u32NumBindings = ReadArrayHeader(psBuffer);

	for(i = 0; i < u32NumBindings; i++)
	{
		psUserBinding = SGXBS_Calloc(sizeof(GLSLAttribUserBinding), psBuffer);	
		if (!psUserBinding)
		{
			return SGXBS_OUT_OF_MEMORY_ERROR;
		}

		/* name */
		eError = ReadString(psBuffer, &psUserBinding->pszName);

		if(eError != SGXBS_NO_ERROR)
		{
			return eError;
		}

		/* index */
		psUserBinding->i32Index = (IMG_INT32)ReadU32(psBuffer);

		psUserBinding->psNext = psLastUserBinding;

		psLastUserBinding = psUserBinding;
	}

	*ppsUserBinding = psLastUserBinding;

	return SGXBS_NO_ERROR;
}

/***********************************************************************************
 Function Name      : UnpackUniPatchInput
 Inputs             : psBuffer          - Buffer that stores the data.
                      pvUniPatchContext - Context for UniPatch.
 Outputs            : ppvUniPatchShader - UniPatch output.
 Returns            : SGXBS_NO_ERROR if successfull. Some other value otherwise.
 Description        : Reads some UniPatch input from the current position of the buffer and tries to create
                      a UniPatch shader out of it.
************************************************************************************/
static SGXBS_Error UnpackUniPatchInput(IMG_VOID **ppvUniPatchShader, IMG_VOID *pvUniPatchContext, SGXBS_Buffer* psBuffer)
{
	IMG_UINT32       u32UniPatchSize;
	USP_PC_SHADER    *psUniPatchInput;

	u32UniPatchSize = ReadU32(psBuffer);

	/*
		A binary program created for oes_get_program_binary will have unipatch size null
		in case of missing fragment shader for MSAA; in contrast, binary shaders will
		have a unipatch structure anyway, even if the MSAA shader is not present  
	*/
	if(u32UniPatchSize)
	{
		if(psBuffer->u32CurrentPosition + u32UniPatchSize >= psBuffer->u32BufferSizeInBytes)
		{
			/* There is not enough space in the buffer for the unipatch header */
			PVR_DPF((PVR_DBG_ERROR,"UnpackUniPatchInput: The UniPatch input data is too long. Corrupt binary!"));
			return SGXBS_CORRUPT_BINARY_ERROR;
		}

		psUniPatchInput = (USP_PC_SHADER*)((IMG_UINTPTR_T)(&psBuffer->pu8Buffer[psBuffer->u32CurrentPosition]));

		/* Skip the opaque UniPatch input data */
		psBuffer->u32CurrentPosition += u32UniPatchSize;

		GLES_ASSERT(psBuffer->u32CurrentPosition < psBuffer->u32BufferSizeInBytes);

		/* Feed it into UniPatch. If it dislikes it, return an error */
		*ppvUniPatchShader = PVRUniPatchCreateShader(pvUniPatchContext, psUniPatchInput);

		if(!*ppvUniPatchShader)
		{
			/* UniPatch didn't like the input. If it happens, there's a bug */
			PVR_DPF((PVR_DBG_ERROR,"UnpackUniPatchInput: UniPatch failed to create a shader"));
			return SGXBS_INTERNAL_ERROR;
		}
	}

	return SGXBS_NO_ERROR;
}


/***********************************************************************************
 Function Name      : UnpackSharedShaderState
 Inputs             : psBuffer    - Buffer that stores the data.
                      bExpectingVertexShader - IMG_TRUE if the GL shader object we are reading to is a vertex shader.
                      pvUniPatchContext - Context for UniPatch.
 Outputs            : ppsSharedState  - Shared shader state.
 Returns            : SGXBS_NO_ERROR if successfull. Some other value otherwise.
 Description        : Reads the buffer and creates a shared shader state from the data.
                      Increments the current buffer position appropriately.
************************************************************************************/
static SGXBS_Error UnpackSharedShaderState(GLES2SharedShaderState **ppsSharedState, IMG_BOOL bExpectingVertexShader,
											IMG_VOID *pvUniPatchContext, SGXBS_Buffer* psBuffer)
{
	SGXBS_Error                 eError;
	GLES2SharedShaderState      *psSharedState;
	GLSLBindingSymbolList       *psSymbolList;
	IMG_UINT32                  eProgramType;
	IMG_UINT32                  i;

	/*
	 * Allocate memory
	 */
	psSharedState = SGXBS_Calloc(sizeof(GLES2SharedShaderState), psBuffer);

	if(!psSharedState)
	{
		return SGXBS_OUT_OF_MEMORY_ERROR;
	}

	*ppsSharedState = psSharedState;

	psSharedState->ui32RefCount = 1;
	psSymbolList = &psSharedState->sBindingSymbolList;

	/*
	 * 1- Check that the shader type (vertex/fragment) matches. Unpack the program flags and 32 reserved bits.
	 */
	eProgramType = ReadU32(psBuffer);

	if(((eProgramType == GLSLPT_FRAGMENT) && bExpectingVertexShader) ||
	   ((eProgramType == GLSLPT_VERTEX) && !bExpectingVertexShader))
	{
		return SGXBS_INVALID_ARGUMENTS_ERROR;
	}

	psSharedState->eProgramFlags = (GLSLProgramFlags) ReadU32(psBuffer);	/* PRQA S 1482 */ /* Override enum warning. */

	/* Reserved for future BRNs, etc */
	ReadU32(psBuffer);

	/*
	 * 2- Unpack the varying mask.
	 */
	psSharedState->eActiveVaryingMask = (GLSLVaryingMask) ReadU32(psBuffer);	/* PRQA S 1482 */ /* Override enum warning. */

	/*
	 * 3- Unpack the texture coordinates' dimensions
	 */
	for(i = 0; i < SGXBS_NUM_TC_REGISTERS; ++i)
	{
		/* Ocassionaly, the data won't be initialized and an INVALID_ARGUMENTS_ERROR would be raised
		 * if the extra checking code was not in place here.
		 */
		psSharedState->aui32TexCoordDims[i] = ReadU8(psBuffer);
	}

	/*
	 * 4- Unpack the texture coordinates' precisions
	 */
	for(i = 0; i < SGXBS_NUM_TC_REGISTERS; ++i)
	{
		/* Ocassionaly, the data won't be initialized and an INVALID_ARGUMENTS_ERROR would be raised
		 * if the extra checking code was not in place here.
		 */
		psSharedState->aeTexCoordPrecisions[i] = (GLSLPrecisionQualifier) ReadU8(psBuffer);	/* PRQA S 1482 */ /* Override enum warning. */
	}

	/*
	 * 5- Read the standard UniPatch input
	 */
	eError = UnpackUniPatchInput(&psSharedState->pvUniPatchShader, pvUniPatchContext, psBuffer);

	if(eError != SGXBS_NO_ERROR || !psSharedState->pvUniPatchShader)
	{
		PVR_DPF((PVR_DBG_MESSAGE,"UnpackSharedShaderState: re-throwing error raised by UnpackUniPatchInput when reading standard UniPatch input"));
		return eError;
	}

	/*
	 * 6- Read the UniPatch input for multisample antialias with translucency (only fragment shaders)
	 */
	if(!bExpectingVertexShader)
	{
		eError = UnpackUniPatchInput(&psSharedState->pvUniPatchShaderMSAATrans, pvUniPatchContext, psBuffer);

		if(eError != SGXBS_NO_ERROR)
		{
			PVR_DPF((PVR_DBG_MESSAGE,"UnpackSharedShaderState: re-throwing error raised by UnpackUniPatchInput when reading MSAATrans UniPatch input"));
			return eError;
		}
	}

	/*
	 * 7- Unpack the constants
	 */
	psSymbolList->uNumCompsUsed = ReadArrayHeader(psBuffer);

	psSymbolList->pfConstantData = SGXBS_Calloc(sizeof(IMG_FLOAT)*psSymbolList->uNumCompsUsed, psBuffer);

	if(psSymbolList->uNumCompsUsed && !psSymbolList->pfConstantData)
	{
		return SGXBS_OUT_OF_MEMORY_ERROR;
	}

	for(i = 0; i < psSymbolList->uNumCompsUsed; ++i)
	{
		psSymbolList->pfConstantData[i] = ReadFloat(psBuffer);
	}

	/*
	 * 8- Unpack the symbol bindings
	 */
	eError = UnpackSymbolBindings(&psSymbolList->psBindingSymbolEntries, &psSymbolList->uNumBindings, psBuffer);

	if(eError != SGXBS_NO_ERROR)
	{
		return eError;
	}

	return SGXBS_NO_ERROR;
}


/***********************************************************************************
 Function Name      : UnpackRevision
 Inputs             : psBuffer    - Buffer that stores the data.
                      bExpectingVertexShader - IMG_TRUE if the GL shader object we are reading to is a vertex shader.
                      pvUniPatchContext - Context for UniPatch.
 Outputs            : ppsSharedState  - Shared shader state.
 Returns            : SGXBS_NO_ERROR if successfull. Some other value otherwise.
 Description        : Reads the buffer until it finds a revision that matches the current hardware/software combination.
                      If found, it creates a shared shader state with the data.
                      Increments the current buffer position appropriately.
************************************************************************************/
static SGXBS_Error UnpackRevision(GLES2SharedShaderState **ppsSharedState, IMG_BOOL bExpectingVertexShader,
								  IMG_BOOL bCheckDDKVersion, IMG_VOID *pvUniPatchContext, SGXBS_Buffer* psBuffer)
{
	IMG_UINT32  u32SoftwareVersion, u32Core, u32CoreRevision, u32CompiledGLSLVersion, u32USPPCShaderVersion, u32ExpectedCore, u32ExpectedCoreRevision,
	            u32RevisionSize;
	
	IMG_UINT32  u32Reserved;
	SGXBS_Error eError;
	IMG_BOOL    bFoundRevision;

	GetCoreAndRevisionNumber(&u32ExpectedCore, &u32ExpectedCoreRevision);

	/*
	 * Try to find a Revision that matches this software and hardware version.
	 */
	do
	{
		bFoundRevision = IMG_TRUE;

		/*
		 * Read the Revision header.
		 */
		u32SoftwareVersion = ReadU16(psBuffer);
		u32Core = ReadU16(psBuffer);
		u32CoreRevision = ReadU16(psBuffer);

		/* Make sure the reserved word is zeroed */
		u32Reserved = ReadU16(psBuffer);

		if(u32Reserved)
		{
			PVR_DPF((PVR_DBG_ERROR,"UnpackRevision: The u16ReservedA flag is 0x%X but we expected 0x0.", u32Reserved));
			bFoundRevision = IMG_FALSE;
		}

		if(bCheckDDKVersion)
		{
			IMG_UINT32 ui32DDKVersion = ReadU32(psBuffer);

			/* Debug messages to make life easier */
			if(ui32DDKVersion != PVRVERSION_BUILD)
			{
				PVR_DPF((PVR_DBG_WARNING,"UnpackRevision: The DDK version is 0x%X but we expected 0x%X.",
					ui32DDKVersion, PVRVERSION_BUILD));
				bFoundRevision = IMG_FALSE;
			}
		}
		else
		{
			/* Jump over one UINT32 storing project version hash which is not used here */
			ReadU32(psBuffer);
		}

		u32CompiledGLSLVersion = ReadU32(psBuffer);
		u32USPPCShaderVersion = ReadU32(psBuffer);

		u32RevisionSize = ReadU32(psBuffer);

		/* Debug messages to make life easier */
		if(SGXBS_SOFTWARE_VERSION_1 != u32SoftwareVersion)
		{
			PVR_DPF((PVR_DBG_WARNING,"UnpackRevision: The software version flag is 0x%X but we expected 0x%X.",
				u32SoftwareVersion, SGXBS_SOFTWARE_VERSION_1));
			bFoundRevision = IMG_FALSE;
		}

		/* SGXBS_ValidateHWRevision is used to relax the hardware revision restriction */
		if (!SGXBS_ValidateHWRevision(u32Core, u32CoreRevision, u32ExpectedCore, u32ExpectedCoreRevision))
		{
			bFoundRevision = IMG_FALSE;
		}

		if (u32CompiledGLSLVersion != GLSL_COMPILED_UNIFLEX_INTERFACE_VER)
		{
			PVR_DPF((PVR_DBG_WARNING,"UnpackRevision: The GLSL compiled Uniflex interface revision flag is 0x%X but we expected 0x%X.",
				u32CompiledGLSLVersion, GLSL_COMPILED_UNIFLEX_INTERFACE_VER));
			bFoundRevision = IMG_FALSE;
		}

		if (u32USPPCShaderVersion != USP_PC_SHADER_VER)
		{
			PVR_DPF((PVR_DBG_WARNING,"UnpackRevision: The Unipatch input revision flag is 0x%X but we expected 0x%X.",
				u32USPPCShaderVersion, USP_PC_SHADER_VER));
			bFoundRevision = IMG_FALSE;
		}

		if(!bFoundRevision)
		{
			/* Skip the whole revision */
			PVR_DPF((PVR_DBG_MESSAGE,"UnpackRevision: Skipping revision."));
			psBuffer->u32CurrentPosition += u32RevisionSize;
		}
	}
	while(!bFoundRevision && psBuffer->u32CurrentPosition < psBuffer->u32BufferSizeInBytes);

	if(bFoundRevision)
	{
		/* Check that the Revision is not longer than the actual binary */
		if(psBuffer->u32CurrentPosition + u32RevisionSize <= psBuffer->u32BufferSizeInBytes)
		{
			/* Reset the end of the buffer to be safe from corrupt binaries and read the revision body. */
			psBuffer->u32BufferSizeInBytes = psBuffer->u32CurrentPosition + u32RevisionSize;
			eError = UnpackSharedShaderState(ppsSharedState, bExpectingVertexShader, pvUniPatchContext, psBuffer);
		}
		else
		{
			PVR_DPF((PVR_DBG_WARNING,"UnpackRevision: This Revision believes it's longer than the actual Binary. Corrupt binary!"));
			eError = SGXBS_CORRUPT_BINARY_ERROR;
		}
	}
	else
	{
		/* We reached the end of the Binary before finding a suitable Revision */
		PVR_DPF((PVR_DBG_WARNING,"UnpackRevision: Couldn't find a revision that matches the current hardware and driver."));
		eError = SGXBS_MISSING_REVISION_ERROR;
	}

	return eError;
}


/***********************************************************************************
 Function Name      : UnpackBinary
 Inputs             : psBuffer    - Buffer that stores the data.
                      bExpectingVertexShader - IMG_TRUE if the GL shader object we are reading to is a vertex shader.
                      pvUniPatchContext - Context for UniPatch.
 Outputs            : ppsSharedState  - Shared shader state.
 Returns            : SGXBS_NO_ERROR if successfull. Some other value otherwise.
 Description        : Reads the buffer and performs a global sanity check with a hash.
                      If the hash seems OK, then reads the revisions calling UnpackRevision.
                      Increments the current buffer position appropriately.
************************************************************************************/
static SGXBS_Error UnpackBinary(GLES2SharedShaderState **ppsSharedState, IMG_BOOL bExpectingVertexShader,
								IMG_BOOL bCheckDDKVersion, IMG_VOID *pvUniPatchContext, SGXBS_Buffer* psBuffer)
{
	SGXBS_Error   eError;
	SGXBS_Hash    sHeaderHash, sComputedHash;
	IMG_UINT32    u32BinaryHeaderMagic;

	/*
	 * Test that the binary is not corrupt.
	 */
	u32BinaryHeaderMagic = ReadU32(psBuffer);

	if(u32BinaryHeaderMagic != SGXBS_HEADER_MAGIC_NUMBER)
	{
		PVR_DPF((PVR_DBG_ERROR,"UnpackBinary: The magic number in the header should be 0x%X but it is 0x%X. Corrupt binary!",
			SGXBS_HEADER_MAGIC_NUMBER, u32BinaryHeaderMagic));
		return SGXBS_CORRUPT_BINARY_ERROR;
	}

	sHeaderHash.u32Hash = ReadU32(psBuffer);

	sComputedHash = SGXBS_ComputeHash(&psBuffer->pu8Buffer[psBuffer->u32CurrentPosition],
						psBuffer->u32BufferSizeInBytes - psBuffer->u32CurrentPosition);

	if(sHeaderHash.u32Hash != sComputedHash.u32Hash)
	{
		PVR_DPF((PVR_DBG_WARNING,"UnpackBinary: The hash in the binary header is wrong. Corrupt binary!"));
		return SGXBS_CORRUPT_BINARY_ERROR;
	}

	/*
	 * Try to find a suitable revision for this software and hardware version.
	 */ 
	eError = UnpackRevision(ppsSharedState, bExpectingVertexShader, bCheckDDKVersion, pvUniPatchContext, psBuffer);

	if(eError != SGXBS_NO_ERROR)
	{
		return eError;
	}

	if(psBuffer->bOverflow)
	{
		return SGXBS_CORRUPT_BINARY_ERROR;
	}

	return SGXBS_NO_ERROR;
}

static SGXBS_Error UnpackProgramBinary(GLES2SharedShaderState **ppsVertexState, GLES2SharedShaderState **ppsFragmentState, GLSLAttribUserBinding **ppsUserBinding,
									   IMG_VOID *pvUniPatchContext, SGXBS_Buffer* psBuffer)
{
	SGXBS_Error   eError;
	SGXBS_Hash    sHeaderHash, sComputedHash;
	IMG_UINT32    u32BinaryHeaderMagic;
	IMG_UINT32	  u32SoftwareVersion, u32Core, u32CoreRevision, u32CompiledGLSLVersion, u32USPPCShaderVersion, u32ExpectedCore, u32ExpectedCoreRevision,
	            u32RevisionSize;
	IMG_UINT32  u32Reserved;
	IMG_BOOL    bFoundRevision;

	/* Meta data 1: test magic number and hash for binary corruption */
	u32BinaryHeaderMagic = ReadU32(psBuffer);

	if(u32BinaryHeaderMagic != SGXBS_HEADER_MAGIC_NUMBER)
	{
		PVR_DPF((PVR_DBG_WARNING,"UnpackProgramBinary: The magic number in the header should be 0x%X but it is 0x%X. Corrupt binary!",
			SGXBS_HEADER_MAGIC_NUMBER, u32BinaryHeaderMagic));
		return SGXBS_CORRUPT_BINARY_ERROR;
	}

	sHeaderHash.u32Hash = ReadU32(psBuffer);

	sComputedHash = SGXBS_ComputeHash(&psBuffer->pu8Buffer[psBuffer->u32CurrentPosition],
						psBuffer->u32BufferSizeInBytes - psBuffer->u32CurrentPosition);

	if(sHeaderHash.u32Hash != sComputedHash.u32Hash)
	{
		PVR_DPF((PVR_DBG_WARNING,"UnpackProgramBinary: The hash in the binary header is wrong. Corrupt binary!"));
		return SGXBS_CORRUPT_BINARY_ERROR;
	}

	/* Meta data 2: try to find a suitable revision for this software and hardware version. */ 

	GetCoreAndRevisionNumber(&u32ExpectedCore, &u32ExpectedCoreRevision);

	/* Try to find a Revision that matches this software and hardware version. */
	do
	{
		bFoundRevision = IMG_TRUE;

		/*
		 * Read the Revision header.
		 */
		u32SoftwareVersion = ReadU16(psBuffer);
		u32Core = ReadU16(psBuffer);
		u32CoreRevision = ReadU16(psBuffer);

		/* Make sure the reserved word is zeroed */
		u32Reserved = ReadU16(psBuffer);

		if(u32Reserved)
		{
			PVR_DPF((PVR_DBG_WARNING,"UnpackProgramBinary: The u16ReservedA flag is 0x%X but we expected 0x0.", u32Reserved));
			bFoundRevision = IMG_FALSE;
		}

		/* Jump over one UINT32 storing project version hash which is not used here */
		ReadU32(psBuffer);

		u32CompiledGLSLVersion = ReadU32(psBuffer);
		u32USPPCShaderVersion = ReadU32(psBuffer);

		u32RevisionSize = ReadU32(psBuffer);

		/* Debug messages to make life easier */
		if(SGXBS_SOFTWARE_VERSION_1 != u32SoftwareVersion)
		{
			PVR_DPF((PVR_DBG_WARNING,"UnpackProgramBinary: The software version flag is 0x%X but we expected 0x%X.",
				u32SoftwareVersion, SGXBS_SOFTWARE_VERSION_1));
			bFoundRevision = IMG_FALSE;
		}

		/* SGXBS_ValidateHWRevision is used to relax the hardware revision restriction */
		if (!SGXBS_ValidateHWRevision(u32Core, u32CoreRevision, u32ExpectedCore, u32ExpectedCoreRevision))
		{
			bFoundRevision = IMG_FALSE;
		}

		if (u32CompiledGLSLVersion != GLSL_COMPILED_UNIFLEX_INTERFACE_VER)
		{
			PVR_DPF((PVR_DBG_WARNING,"UnpackRevision: The GLSL compiled Uniflex interface revision flag is 0x%X but we expected 0x%X.",
				u32CompiledGLSLVersion, GLSL_COMPILED_UNIFLEX_INTERFACE_VER));
			bFoundRevision = IMG_FALSE;
		}

		if (u32USPPCShaderVersion != USP_PC_SHADER_VER)
		{
			PVR_DPF((PVR_DBG_WARNING,"UnpackRevision: The Unipatch input revision flag is 0x%X but we expected 0x%X.",
				u32USPPCShaderVersion, USP_PC_SHADER_VER));
			bFoundRevision = IMG_FALSE;
		}

		if(!bFoundRevision)
		{
			/* Skip the whole revision */
			PVR_DPF((PVR_DBG_MESSAGE,"UnpackProgramBinary: Skipping revision."));
			psBuffer->u32CurrentPosition += u32RevisionSize;
		}
	}
	while(!bFoundRevision && psBuffer->u32CurrentPosition < psBuffer->u32BufferSizeInBytes);

	if(bFoundRevision)
	{
		/* Check that the Revision is not longer than the actual binary */
		if(psBuffer->u32CurrentPosition + u32RevisionSize <= psBuffer->u32BufferSizeInBytes)
		{
			/* Reset the end of the buffer to be safe from corrupt binaries and read the revision body. */
			psBuffer->u32BufferSizeInBytes = psBuffer->u32CurrentPosition + u32RevisionSize;
			eError = UnpackSharedShaderState(ppsVertexState, IMG_TRUE, pvUniPatchContext, psBuffer);

			if(eError == SGXBS_NO_ERROR)
			{
				eError = UnpackSharedShaderState(ppsFragmentState, IMG_FALSE, pvUniPatchContext, psBuffer);
			
				if(eError == SGXBS_NO_ERROR)
				{
					eError = UnpackUserBindings(ppsUserBinding, pvUniPatchContext, psBuffer);
				}
			}
		}
		else
		{
			PVR_DPF((PVR_DBG_WARNING,"UnpackProgramBinary: This Revision believes it's longer than the actual Binary. Corrupt binary!"));
			eError = SGXBS_CORRUPT_BINARY_ERROR;
		}
	}
	else
	{
		/* We reached the end of the Binary before finding a suitable Revision */
		PVR_DPF((PVR_DBG_WARNING,"UnpackProgramBinary: Couldn't find a revision that matches the current hardware and driver."));
		eError = SGXBS_MISSING_REVISION_ERROR;
	}


	if(eError != SGXBS_NO_ERROR)
	{
		return eError;
	}

	if(psBuffer->bOverflow)
	{
		return SGXBS_CORRUPT_BINARY_ERROR;
	}

	return SGXBS_NO_ERROR;
}

/* ------------------------------------------------------------------------------------------- */
/* --------------------------------------- ENTRY POINTS  ------------------------------------- */
/* ------------------------------------------------------------------------------------------- */


IMG_INTERNAL SGXBS_Error SGXBS_CreateSharedShaderState(	GLES2Context *gc, 
														const IMG_VOID* pvBinaryShader,
														IMG_INT32 i32BinaryShaderLengthInBytes, 
														IMG_BOOL bExpectingVertexShader,
														IMG_BOOL bCheckDDKVersion,
														IMG_VOID *pvUniPatchContext, 
														GLES2SharedShaderState **ppsSharedState)
{
	SGXBS_Buffer          sBuffer;
	SGXBS_Error           eError;

	if(!SGXBS_TestBinaryShaderInterface())
	{
		return SGXBS_INTERNAL_ERROR;
	}

	if(!pvBinaryShader || !ppsSharedState)
	{
		/* Null arguments */
		return SGXBS_INVALID_ARGUMENTS_ERROR;
	}

	if(i32BinaryShaderLengthInBytes < SGXBS_MINIMUM_VALID_BINARY_SIZE)
	{
		PVR_DPF((PVR_DBG_ERROR,"SGXBS_CreateSharedShaderState: The size of the shader is invalid. Corrupt binary!"));
		return SGXBS_CORRUPT_BINARY_ERROR;
	}

	/* Initialize the buffer */
	sBuffer.pu8Buffer               = pvBinaryShader;
	sBuffer.u32BufferSizeInBytes    = (IMG_UINT32)i32BinaryShaderLengthInBytes;
	sBuffer.u32CurrentPosition      = 0;
	sBuffer.bOverflow               = IMG_FALSE;
	sBuffer.gc                      = gc;
	sBuffer.u32NumMemoryAllocations = 0;
	sBuffer.u32MaxMemoryAllocations = SGXBS_DEFAULT_ALLOC_BUFFER_SIZE;
	sBuffer.apvAllocatedMemory      = GLES2Malloc(gc, sBuffer.u32MaxMemoryAllocations*sizeof(IMG_VOID*));

	if(!sBuffer.apvAllocatedMemory)
	{
		return SGXBS_OUT_OF_MEMORY_ERROR;
	}

	/* Unpack the shader and return it */
	eError = UnpackBinary(ppsSharedState, bExpectingVertexShader, bCheckDDKVersion, pvUniPatchContext, &sBuffer);

	if(eError != SGXBS_NO_ERROR)
	{
		/* Release the allocated memory before returning */
		if(*ppsSharedState)
		{
			if((*ppsSharedState)->pvUniPatchShader)
			{
				PVRUniPatchDestroyShader(pvUniPatchContext, (*ppsSharedState)->pvUniPatchShader);
			}

			if((*ppsSharedState)->pvUniPatchShaderMSAATrans)
			{
				PVRUniPatchDestroyShader(pvUniPatchContext, (*ppsSharedState)->pvUniPatchShaderMSAATrans);
			}
		}
		SGXBS_FreeAllocatedMemory(&sBuffer);
	}

	/* Free the memory used by the buffer */
	GLES2Free(IMG_NULL, sBuffer.apvAllocatedMemory);
	
	return eError;
}

IMG_INTERNAL SGXBS_Error SGXBS_CreateProgramState(GLES2Context *gc, const IMG_VOID* pvBinaryProgram,
													IMG_INT32 i32BinaryShaderLengthInBytes, IMG_VOID *pvUniPatchContext, 
													GLES2SharedShaderState **ppsVertexState, GLES2SharedShaderState **ppsFragmentState, GLSLAttribUserBinding **ppsUserBinding)
{
	SGXBS_Buffer          sBuffer;
	SGXBS_Error           eError;

	if(!SGXBS_TestBinaryShaderInterface())
	{
		return SGXBS_INTERNAL_ERROR;
	}

	if(!pvBinaryProgram || !ppsVertexState || !ppsFragmentState)
	{
		/* Null arguments */
		return SGXBS_INVALID_ARGUMENTS_ERROR;
	}

	if(i32BinaryShaderLengthInBytes < SGXBS_MINIMUM_VALID_BINARY_SIZE)
	{
		PVR_DPF((PVR_DBG_ERROR,"SGXBS_CreateSharedShaderState: The size of the shader is invalid. Corrupt binary!"));
		return SGXBS_CORRUPT_BINARY_ERROR;
	}

	/* Initialize the buffer */
	sBuffer.pu8Buffer               = pvBinaryProgram;
	sBuffer.u32BufferSizeInBytes    = (IMG_UINT32)i32BinaryShaderLengthInBytes;
	sBuffer.u32CurrentPosition      = 0;
	sBuffer.bOverflow               = IMG_FALSE;
	sBuffer.gc                      = gc;
	sBuffer.u32NumMemoryAllocations = 0;
	sBuffer.u32MaxMemoryAllocations = SGXBS_DEFAULT_ALLOC_BUFFER_SIZE;
	sBuffer.apvAllocatedMemory      = GLES2Malloc(gc, sBuffer.u32MaxMemoryAllocations*sizeof(IMG_VOID*));

	if(!sBuffer.apvAllocatedMemory)
	{
		return SGXBS_OUT_OF_MEMORY_ERROR;
	}

	/* Unpack the shader and return it */
	eError = UnpackProgramBinary(ppsVertexState, ppsFragmentState, ppsUserBinding, pvUniPatchContext, &sBuffer);

	if(eError != SGXBS_NO_ERROR)
	{
		/* Release the allocated memory before returning */
		if(*ppsVertexState)
		{
			if((*ppsVertexState)->pvUniPatchShader)
			{
				PVRUniPatchDestroyShader(pvUniPatchContext, (*ppsVertexState)->pvUniPatchShader);
			}

			if((*ppsVertexState)->pvUniPatchShaderMSAATrans)
			{
				PVRUniPatchDestroyShader(pvUniPatchContext, (*ppsVertexState)->pvUniPatchShaderMSAATrans);
			}
		}

		if(*ppsFragmentState)
		{
			if((*ppsFragmentState)->pvUniPatchShader)
			{
				PVRUniPatchDestroyShader(pvUniPatchContext, (*ppsFragmentState)->pvUniPatchShader);
			}

			if((*ppsFragmentState)->pvUniPatchShaderMSAATrans)
			{
				PVRUniPatchDestroyShader(pvUniPatchContext, (*ppsFragmentState)->pvUniPatchShaderMSAATrans);
			}
		}

		SGXBS_FreeAllocatedMemory(&sBuffer);
	}

	/* Free the memory used by the buffer */
	GLES2Free(IMG_NULL, sBuffer.apvAllocatedMemory);
	
	return eError;
}

#endif /* defined(SUPPORT_SHADER_BINARY) */
/******************************************************************************
 End of file (binshader.c)
******************************************************************************/

