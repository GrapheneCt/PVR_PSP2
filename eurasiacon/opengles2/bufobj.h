/**************************************************************************
 * Name         : bufobj.h
 * Author       : 
 * Created      : 
 *
 * Copyright    : 2004-2006 by Imagination Technologies Limited. 
 *                All rights reserved.
 *                No part of this software, either material or conceptual 
 *                may be copied or distributed, transmitted, transcribed,
 *                stored in a retrieval system or translated into any 
 *                human or computer language in any form by any means,
 *                electronic, mechanical, manual or other-wise, or 
 *                disclosed to third parties without the express written
 *                permission of:
 *                        Imagination Technologies Limited, 
 *                        HomePark Industrial Estate,
 *                        Kings Langley, 
 *                        Hertfordshire,
 *                        WD4 8LZ, U.K.
 *
 * Description  : Header file vertex_buffer_object
 *
 * Platform     : ANSI
 *
 * Modifications:-
 * $Log: bufobj.h $
 **************************************************************************/

#ifndef _BUFOBJ_
#define _BUFOBJ_


/* num of buffer object bindings */
#define ARRAY_BUFFER_INDEX			0
#define ELEMENT_ARRAY_BUFFER_INDEX	1
#define GLES2_NUM_BUFOBJ_BINDINGS	2

#define INDEX_BUFFER_OBJECT(gc)	(gc->sBufferObject.psActiveBuffer[ELEMENT_ARRAY_BUFFER_INDEX]!=IMG_NULL)

#define ARRAY_BUFFER_OBJECT(gc)	(gc->sBufferObject.psActiveBuffer[ARRAY_BUFFER_INDEX]!=IMG_NULL)


/* type casting for using pointers as offsets */
#define GLES2_BUFFER_OFFSET(pointer) ((GLintptr)pointer)


typedef struct GLES2BufferObjectRec
{
 	/* This struct must be the first variable */
	GLES2NamedItem  sNamedItem;

    /* The buffer index: ARRAY_BUFFER_INDEX or ELEMENT_ARRAY_BUFFER_INDEX */ 
    GLenum eIndex; 

	/* The buffer object states */
	GLenum eUsage, eAccess;

	IMG_UINT32 ui32BufferSize;

	/* The alignment of the device memory allocation */
	IMG_UINT32 ui32AllocAlign;

	/* Meminfo for any uploaded version of the buffer */
	PVRSRV_CLIENT_MEM_INFO *psMemInfo;

	/* Buffer objects are TA-kick resources */
	KRMResource sResource;

	/* Is the buffer mapped */
	IMG_BOOL bMapped;

#if defined(PDUMP)
	/* Has this object been pdumped since it was last changed. */
	IMG_BOOL bDumped;
#endif

} GLES2BufferObject;


typedef struct GLES2BufferObjectMachineRec 
{
	GLES2BufferObject *psActiveBuffer[GLES2_NUM_BUFOBJ_BINDINGS]; 

} GLES2BufferObjectMachine;


IMG_BOOL CreateBufObjState(GLES2Context *gc);
IMG_VOID FreeBufObjState(GLES2Context *gc);
IMG_VOID SetupBufObjNameArray(GLES2NamesArray *psNamesArray);

IMG_VOID ReclaimBufferObjectMemKRM(IMG_VOID *pvContext, KRMResource *psResource);
IMG_VOID DestroyBufferObjectGhostKRM(IMG_VOID *pvContext, KRMResource *psResource);

#endif /* _BUFOBJ_ */
