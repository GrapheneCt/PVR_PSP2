/******************************************************************************
 * Name         : attrib.h
 * Author       : BCB
 * Created      : 19/12/2005
 *
 * Copyright    : 2005-2006 by Imagination Technologies Limited.
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
 * $Date: 2010/02/12 15:41:11 $ $Revision: 1.29 $
 * $Log: attrib.h $
 **************************************************************************/

#ifndef _ATTRIB_
#define _ATTRIB_


#define AP_VERTEX_ATTRIB0				0

#define AP_VERTEX_ATTRIB_MAX			GLES2_MAX_VERTEX_ATTRIBS

/* Bitfield assumes enables are packed into 32 bits starting from here */
#define VARRAY_ATTRIB0_ENABLE			(1U << AP_VERTEX_ATTRIB0)


#define GLES2_STREAMTYPE_BYTE			0x00000000
#define GLES2_STREAMTYPE_UBYTE			0x00000001
#define GLES2_STREAMTYPE_SHORT			0x00000002
#define GLES2_STREAMTYPE_USHORT			0x00000003
#define GLES2_STREAMTYPE_FLOAT			0x00000004
#define GLES2_STREAMTYPE_HALFFLOAT		0x00000005
#define GLES2_STREAMTYPE_FIXED			0x00000006

#define GLES2_STREAMTYPE_MAX			0x00000007

#define GLES2_STREAMTYPE_MASK			0x00000007
#define GLES2_STREAMNORM_BIT			0x00000008

#define GLES2_STREAMSIZE_SHIFT			4
#define GLES2_STREAMSIZE_MASK			0x000000F0


typedef struct GLES2AttribArrayPointerStateRec
{
	IMG_UINT8	*pui8Pointer;

	IMG_UINT32	ui32StreamTypeSize;

	IMG_UINT32	ui32UserStride;

	GLES2BufferObject *psBufObj;

} GLES2AttribArrayPointerState;


extern IMG_VOID (* const CopyData[4][GLES2_STREAMTYPE_MAX])(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count);
extern IMG_VOID (* const MemCopyData[4][GLES2_STREAMTYPE_MAX])(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count);

#if defined(NO_UNALIGNED_ACCESS)
extern IMG_VOID (* const CopyDataShortAligned[4][GLES2_STREAMTYPE_MAX])(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count);
extern IMG_VOID (* const CopyDataByteAligned[4][GLES2_STREAMTYPE_MAX])(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count);
#endif

typedef struct GLES2AttribArrayPointerMachineRec
{
	IMG_UINT32	ui32Stride;

	IMG_UINT32	ui32Size;

	IMG_BOOL	bIsCurrentState;

	IMG_UINT8	*pui8CopyPointer;
	IMG_UINT32	ui32CopyStreamTypeSize;
	IMG_UINT32	ui32CopyStride;
	IMG_UINT8	*pui8SrcPointer;
	IMG_UINT8	*pui8DstPointer;
	IMG_UINT32	ui32DstSize;

#if defined(NO_UNALIGNED_ACCESS)
	IMG_VOID	(*pfnCopyData[4])(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count);
#else
	IMG_VOID	(*pfnCopyData)(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count);
#endif

	IMG_VOID	*pvPDSSrcAddress;
	GLES2AttribArrayPointerState *psState;

} GLES2AttribArrayPointerMachine;


/* Attrib array control word flags. */
#define ATTRIBARRAY_SOURCE_BUFOBJ		0x00000001 /* At least one attrib is a buffer object */
#define ATTRIBARRAY_SOURCE_VARRAY		0x00000002 /* At least one attrib is vertex array */
#define ATTRIBARRAY_SOURCE_CURRENT		0x00000004 /* At least one attrib is current state */
#define ATTRIBARRAY_MAP_BUFOBJ			0x00000008 /* At least one buffer object is mapped currently */
#define ATTRIBARRAY_BAD_BUFOBJ			0x00000010 /* At least one attrib is a buffer object with no meminfo */

typedef struct GLES2AttribArrayMachineRec
{
	GLES2AttribArrayPointerMachine asAttribPointer[GLES2_MAX_VERTEX_ATTRIBS];

	GLES2AttribArrayPointerMachine *apsPackedAttrib[GLES2_MAX_VERTEX_ATTRIBS];

	IMG_UINT32 ui32NumItemsPerVertex;

	IMG_UINT32 ui32ArrayEnables;

	IMG_UINT32 ui32ControlWord;

} GLES2AttribArrayMachine;


#endif /* _ATTRIB_ */

/******************************************************************************
 End of file (attrib.h)
******************************************************************************/

