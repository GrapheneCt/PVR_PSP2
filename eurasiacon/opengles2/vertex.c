/******************************************************************************
 * Name         : vertex.c
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
 * $Log: vertex.c $
 *
 **************************************************************************/

#include "context.h"


static IMG_VOID Copy1Byte(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT32 i;

	for (i=0; i<ui32Count; i++)
	{
		pui8Dst[0] = pui8Src[0];

		pui8Dst += 1;

		pui8Src += ui32SrcStride;
	}
}

static IMG_VOID Copy3Bytes(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT32 i;

	for (i=0; i<ui32Count; i++)
	{
		pui8Dst[0] = pui8Src[0];
		pui8Dst[1] = pui8Src[1];
		pui8Dst[2] = pui8Src[2];

		pui8Dst += 3;

		pui8Src += ui32SrcStride;
	}
}


static IMG_VOID Copy1Short(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT16 *pui16Src, *pui16Dst;
	IMG_UINT32 i;

	pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)pui8Src);
	pui16Dst = (IMG_UINT16 *)((IMG_UINTPTR_T)pui8Dst);

	for (i=0; i<ui32Count; i++)
	{
		pui16Dst[0] = pui16Src[0];

		pui16Dst += 1;

		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)pui16Src + ui32SrcStride);
	}
}

static IMG_VOID Copy3Shorts(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT16 *pui16Src, *pui16Dst;
	IMG_UINT32 i;

	pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)pui8Src);
	pui16Dst = (IMG_UINT16 *)((IMG_UINTPTR_T)pui8Dst);

	for (i=0; i<ui32Count; i++)
	{
		pui16Dst[0] = pui16Src[0];
		pui16Dst[1] = pui16Src[1];
		pui16Dst[2] = pui16Src[2];

		pui16Dst += 3;

		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)pui16Src + ui32SrcStride);
	}
}


static IMG_VOID Copy1Long(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT32 *pui32Src, *pui32Dst;
	IMG_UINT32 i;

	pui32Src = (IMG_UINT32 *)((IMG_UINTPTR_T)pui8Src);
	pui32Dst = (IMG_UINT32 *)((IMG_UINTPTR_T)pui8Dst);

	for (i=0; i<ui32Count; i++)
	{
		pui32Dst[0] = pui32Src[0];

		pui32Dst += 1;

		pui32Src = (IMG_UINT32 *)((IMG_UINTPTR_T)pui32Src + ui32SrcStride);
	}
}


static IMG_VOID Copy2Longs(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT32 *pui32Src, *pui32Dst;
	IMG_UINT32 i;

	pui32Src = (IMG_UINT32 *)((IMG_UINTPTR_T)pui8Src);
	pui32Dst = (IMG_UINT32 *)((IMG_UINTPTR_T)pui8Dst);

	for (i=0; i<ui32Count; i++)
	{
		pui32Dst[0] = pui32Src[0];
		pui32Dst[1] = pui32Src[1];

		pui32Dst += 2;

		pui32Src = (IMG_UINT32 *)((IMG_UINTPTR_T)pui32Src + ui32SrcStride);
	}
}


static IMG_VOID Copy3Longs(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT32 *pui32Src, *pui32Dst;
	IMG_UINT32 i;

	pui32Src = (IMG_UINT32 *)((IMG_UINTPTR_T)pui8Src);
	pui32Dst = (IMG_UINT32 *)((IMG_UINTPTR_T)pui8Dst);

	for (i=0; i<ui32Count; i++)
	{
		pui32Dst[0] = pui32Src[0];
		pui32Dst[1] = pui32Src[1];
		pui32Dst[2] = pui32Src[2];

		pui32Dst += 3;

		pui32Src = (IMG_UINT32 *)((IMG_UINTPTR_T)pui32Src + ui32SrcStride);
	}
}


static IMG_VOID Copy4Longs(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT32 *pui32Src, *pui32Dst;
	IMG_UINT32 i;

	pui32Src = (IMG_UINT32 *)((IMG_UINTPTR_T)pui8Src);
	pui32Dst = (IMG_UINT32 *)((IMG_UINTPTR_T)pui8Dst);

	for (i=0; i<ui32Count; i++)
	{
		pui32Dst[0] = pui32Src[0];
		pui32Dst[1] = pui32Src[1];
		pui32Dst[2] = pui32Src[2];
		pui32Dst[3] = pui32Src[3];

		pui32Dst += 4;

		pui32Src = (IMG_UINT32 *)((IMG_UINTPTR_T)pui32Src + ui32SrcStride);
	}
}


static IMG_VOID MemCopy1Byte(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	PVR_UNREFERENCED_PARAMETER(ui32SrcStride);

	GLES2MemCopy(pui8Dst, pui8Src, ui32Count);
}

static IMG_VOID MemCopy3Bytes(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	PVR_UNREFERENCED_PARAMETER(ui32SrcStride);

	GLES2MemCopy(pui8Dst, pui8Src, ui32Count*3);
}

static IMG_VOID MemCopy1Short(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	PVR_UNREFERENCED_PARAMETER(ui32SrcStride);

	GLES2MemCopy(pui8Dst, pui8Src, ui32Count*sizeof(IMG_UINT16));
}

static IMG_VOID MemCopy3Shorts(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	PVR_UNREFERENCED_PARAMETER(ui32SrcStride);

	GLES2MemCopy(pui8Dst, pui8Src, ui32Count*(3*sizeof(IMG_UINT16)));
}

static IMG_VOID MemCopy1Long(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	PVR_UNREFERENCED_PARAMETER(ui32SrcStride);

	GLES2MemCopy(pui8Dst, pui8Src, ui32Count*sizeof(IMG_UINT32));
}

static IMG_VOID MemCopy2Longs(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	PVR_UNREFERENCED_PARAMETER(ui32SrcStride);

	GLES2MemCopy(pui8Dst, pui8Src, ui32Count*(2*sizeof(IMG_UINT32)));
}

static IMG_VOID MemCopy3Longs(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	PVR_UNREFERENCED_PARAMETER(ui32SrcStride);

	GLES2MemCopy(pui8Dst, pui8Src, ui32Count*(3*sizeof(IMG_UINT32)));
}

static IMG_VOID MemCopy4Longs(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	PVR_UNREFERENCED_PARAMETER(ui32SrcStride);

	GLES2MemCopy(pui8Dst, pui8Src, ui32Count*(4*sizeof(IMG_UINT32)));
}


IMG_INTERNAL IMG_VOID (* const CopyData[4][GLES2_STREAMTYPE_MAX])(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count) = 
{
	{
		Copy1Byte,
		Copy1Byte,
		Copy1Short,
		Copy1Short,
		Copy1Long,
		Copy1Short,
		Copy1Long 
	},
	{
		Copy1Short,
		Copy1Short,
		Copy1Long,
		Copy1Long,
		Copy2Longs,
		Copy1Long,
		Copy2Longs 
	},
	{
		Copy3Bytes,
		Copy3Bytes,
		Copy3Shorts,
		Copy3Shorts,
		Copy3Longs,
		Copy3Shorts,
		Copy3Longs 
	},
	{
		Copy1Long,
		Copy1Long,
		Copy2Longs,
		Copy2Longs,
		Copy4Longs,
		Copy2Longs,
		Copy4Longs 
	},
};

IMG_INTERNAL IMG_VOID (* const MemCopyData[4][GLES2_STREAMTYPE_MAX])(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count) = 
{
	{
		MemCopy1Byte,
		MemCopy1Byte,
		MemCopy1Short,
		MemCopy1Short,
		MemCopy1Long,
		MemCopy1Short,
		MemCopy1Long 
	},
	{
		MemCopy1Short,
		MemCopy1Short,
		MemCopy1Long,
		MemCopy1Long,
		MemCopy2Longs,
		MemCopy1Long,
		MemCopy2Longs 
	},
	{
		MemCopy3Bytes,
		MemCopy3Bytes,
		MemCopy3Shorts,
		MemCopy3Shorts,
		MemCopy3Longs,
		MemCopy3Shorts,
		MemCopy3Longs 
	},
	{
		MemCopy1Long,
		MemCopy1Long,
		MemCopy2Longs,
		MemCopy2Longs,
		MemCopy4Longs,
		MemCopy2Longs,
		MemCopy4Longs 
	},
};

#if defined(NO_UNALIGNED_ACCESS)
static IMG_VOID Copy2Bytes(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT32 i;

	for (i=0; i<ui32Count; i++)
	{
		pui8Dst[0] = pui8Src[0];
		pui8Dst[1] = pui8Src[1];

		pui8Dst += 2;

		pui8Src += ui32SrcStride;
	}
}


static IMG_VOID Copy4Bytes(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT32 i;

	for (i=0; i<ui32Count; i++)
	{
		pui8Dst[0] = pui8Src[0];
		pui8Dst[1] = pui8Src[1];
		pui8Dst[2] = pui8Src[2];
		pui8Dst[3] = pui8Src[3];

		pui8Dst += 4;

		pui8Src += ui32SrcStride;
	}
}


static IMG_VOID Copy6Bytes(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT32 i, j;

	for (i=0; i<ui32Count; i++)
	{
		for (j=0 ; j<6 ; j++)
		{
			pui8Dst[j] = pui8Src[j];
		}

		pui8Dst += 6;

		pui8Src += ui32SrcStride;
	}
}


static IMG_VOID Copy8Bytes(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT32 i, j;

	for (i=0; i<ui32Count; i++)
	{
		for (j=0 ; j<8 ; j++)
		{
			pui8Dst[j] = pui8Src[j];
		}

		pui8Dst += 8;

		pui8Src += ui32SrcStride;
	}
}


static IMG_VOID Copy12Bytes(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT32 i, j;

	for (i=0; i<ui32Count; i++)
	{
		for (j=0 ; j<12 ; j++)
		{
			pui8Dst[j] = pui8Src[j];
		}

		pui8Dst += 12;

		pui8Src += ui32SrcStride;
	}
}


static IMG_VOID Copy16Bytes(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT32 i, j;

	for (i=0; i<ui32Count; i++)
	{
		for (j=0 ; j<16 ; j++)
		{
			pui8Dst[j] = pui8Src[j];
		}

		pui8Dst += 16;

		pui8Src += ui32SrcStride;
	}
}


static IMG_VOID Copy2Shorts(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT16 *pui16Src, *pui16Dst;
	IMG_UINT32 i;

	pui16Src = (IMG_UINT16 *)pui8Src;
	pui16Dst = (IMG_UINT16 *)pui8Dst;

	for (i=0; i<ui32Count; i++)
	{
		pui16Dst[0] = pui16Src[0];
		pui16Dst[1] = pui16Src[1];

		pui16Dst += 2;
		pui16Src = (IMG_UINT16 *)((IMG_UINT8 *)pui16Src + ui32SrcStride);
	}
}


static IMG_VOID Copy4Shorts(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT16 *pui16Src, *pui16Dst;
	IMG_UINT32 i;

	pui16Src = (IMG_UINT16 *)pui8Src;
	pui16Dst = (IMG_UINT16 *)pui8Dst;

	for (i=0; i<ui32Count; i++)
	{
		pui16Dst[0] = pui16Src[0];
		pui16Dst[1] = pui16Src[1];
		pui16Dst[2] = pui16Src[2];
		pui16Dst[3] = pui16Src[3];

		pui16Dst += 4;
		pui16Src = (IMG_UINT16 *)((IMG_UINT8 *)pui16Src + ui32SrcStride);
	}
}


static IMG_VOID Copy6Shorts(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT16 *pui16Src, *pui16Dst;
	IMG_UINT32 i, j;

	pui16Src = (IMG_UINT16 *)pui8Src;
	pui16Dst = (IMG_UINT16 *)pui8Dst;

	for (i=0; i<ui32Count; i++)
	{
		for (j=0 ; j<6 ; j++)
		{
			pui16Dst[j] = pui16Src[j];
		}

		pui16Dst += 6;
		pui16Src = (IMG_UINT16 *)((IMG_UINT8 *)pui16Src + ui32SrcStride);
	}
}


static IMG_VOID Copy8Shorts(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT16 *pui16Src, *pui16Dst;
	IMG_UINT32 i, j;

	pui16Src = (IMG_UINT16 *)pui8Src;
	pui16Dst = (IMG_UINT16 *)pui8Dst;

	for (i=0; i<ui32Count; i++)
	{
		for (j=0 ; j<8 ; j++)
		{
			pui16Dst[j] = pui16Src[j];
		}

		pui16Dst += 8;
		pui16Src = (IMG_UINT16 *)((IMG_UINT8 *)pui16Src + ui32SrcStride);
	}
}


IMG_VOID (* const CopyDataShortAligned[4][GLES2_STREAMTYPE_MAX])(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count) = 
{
	{
		Copy1Byte,
		Copy1Byte,
		Copy1Short,
		Copy1Short,
		Copy2Shorts,
		Copy1Short,
		Copy2Shorts 
	},
	{
		Copy1Short,
		Copy1Short,
		Copy2Shorts,
		Copy2Shorts,
		Copy4Shorts,
		Copy2Shorts,
		Copy4Shorts 
	},
	{
		Copy3Bytes,
		Copy3Bytes,
		Copy3Shorts,
		Copy3Shorts,
		Copy6Shorts,
		Copy3Shorts,
		Copy6Shorts 
	},
	{
		Copy2Shorts,
		Copy2Shorts,
		Copy4Shorts,
		Copy4Shorts,
		Copy8Shorts,
		Copy4Shorts,
		Copy8Shorts 
	},
};

IMG_VOID (* const CopyDataByteAligned[4][GLES2_STREAMTYPE_MAX])(const IMG_UINT8 *pui8Src, IMG_UINT8 * IMG_RESTRICT pui8Dst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count) = 
{
	{
		Copy1Byte,
		Copy1Byte,
		Copy2Bytes,
		Copy2Bytes,
		Copy4Bytes,
		Copy2Bytes,
		Copy4Bytes 
	},
	{
		Copy2Bytes,
		Copy2Bytes,
		Copy4Bytes,
		Copy4Bytes,
		Copy8Bytes,
		Copy4Bytes,
		Copy8Bytes 
	},
	{
		Copy3Bytes,
		Copy3Bytes,
		Copy6Bytes,
		Copy6Bytes,
		Copy12Bytes,
		Copy6Bytes,
		Copy12Bytes 
	},
	{
		Copy4Bytes,
		Copy4Bytes,
		Copy8Bytes,
		Copy8Bytes,
		Copy16Bytes,
		Copy8Bytes,
		Copy16Bytes 
	},
};
#endif



/***********************************************************************************
 Function Name      : glEnableVertexAttribArray
 Inputs             : index
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Enables attrib array specified by index
************************************************************************************/
GL_APICALL void GL_APIENTRY glEnableVertexAttribArray(GLuint index)
{
    GLES2VertexArrayObject *psVAO;

	__GLES2_GET_CONTEXT();

	/* Setup and assert VAO */
	psVAO = gc->sVAOMachine.psActiveVAO;
	GLES_ASSERT(VAO(gc));	

	PVR_DPF((PVR_DBG_CALLTRACE,"glEnableVertexAttribArray"));

	GLES2_TIME_START(GLES2_TIMES_glEnableVertexAttribArray);

	if(index >= GLES2_MAX_VERTEX_ATTRIBS)
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES2_TIME_STOP(GLES2_TIMES_glEnableVertexAttribArray);

		return;
	}

	/* Return if this attribute is enabled before */
	if (psVAO->ui32CurrentArrayEnables & (VARRAY_ATTRIB0_ENABLE << index))
	{
		GLES2_TIME_STOP(GLES2_TIMES_glEnableVertexAttribArray);
		return;
	}

	/* Setup ui32ArrayEnables for attribute array machine */
	psVAO->ui32CurrentArrayEnables |= (VARRAY_ATTRIB0_ENABLE << index);

	/* Set dirty flag */
	psVAO->ui32DirtyState |= GLES2_DIRTYFLAG_VAO_CLIENT_STATE;


	GLES2_TIME_STOP(GLES2_TIMES_glEnableVertexAttribArray);

}


/***********************************************************************************
 Function Name      : glDisableVertexAttribArray
 Inputs             : index
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Disables attrib array specified by index
************************************************************************************/
GL_APICALL void GL_APIENTRY glDisableVertexAttribArray(GLuint index)
{
    GLES2VertexArrayObject *psVAO;

	__GLES2_GET_CONTEXT();

	/* Setup and assert VAO */
	psVAO = gc->sVAOMachine.psActiveVAO;
	GLES_ASSERT(VAO(gc));	

	PVR_DPF((PVR_DBG_CALLTRACE,"glDisableVertexAttribArray"));

	GLES2_TIME_START(GLES2_TIMES_glDisableVertexAttribArray);

	if(index >= GLES2_MAX_VERTEX_ATTRIBS)
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES2_TIME_STOP(GLES2_TIMES_glDisableVertexAttribArray);

		return;
	}

	/* Return if this attribute is not enabled before */
	if ((psVAO->ui32CurrentArrayEnables & (VARRAY_ATTRIB0_ENABLE << index)) == 0)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glDisableVertexAttribArray);
		return;
	}

	/* Setup ui32ArrayEnables for attribute array machine */
	psVAO->ui32CurrentArrayEnables &= ~(VARRAY_ATTRIB0_ENABLE << index);

	/* Set VAO's dirty flag */
	psVAO->ui32DirtyState |= GLES2_DIRTYFLAG_VAO_CLIENT_STATE;


	GLES2_TIME_STOP(GLES2_TIMES_glDisableVertexAttribArray);

}


/***********************************************************************************
 Function Name      : glVertexAttrib1f
 Inputs             : index, x
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Specifies current attrib values specified by index
************************************************************************************/
GL_APICALL void GL_APIENTRY glVertexAttrib1f(GLuint index, GLfloat x)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glVertexAttrib1f"));

	GLES2_TIME_START(GLES2_TIMES_glVertexAttrib1f);

	if(index >= GLES2_MAX_VERTEX_ATTRIBS)
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES2_TIME_STOP(GLES2_TIMES_glVertexAttrib1f);

		return;
	}

	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fX = x;
	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fY = GLES2_Zero;
	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fZ = GLES2_Zero;
	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fW = GLES2_One;

	GLES2_TIME_STOP(GLES2_TIMES_glVertexAttrib1f);
}


/***********************************************************************************
 Function Name      : glVertexAttrib2f
 Inputs             : index, x, y
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Specifies current attrib values specified by index
************************************************************************************/
GL_APICALL void GL_APIENTRY glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glVertexAttrib2f"));

	GLES2_TIME_START(GLES2_TIMES_glVertexAttrib2f);

	if(index >= GLES2_MAX_VERTEX_ATTRIBS)
	{
		SetError(gc, GL_INVALID_VALUE);
		
		GLES2_TIME_STOP(GLES2_TIMES_glVertexAttrib2f);
		
		return;
	}

	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fX = x;
	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fY = y;
	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fZ = GLES2_Zero;
	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fW = GLES2_One;

	GLES2_TIME_STOP(GLES2_TIMES_glVertexAttrib2f);
}


/***********************************************************************************
 Function Name      : glVertexAttrib3f
 Inputs             : index, x, y, z
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Specifies current attrib values specified by index
************************************************************************************/
GL_APICALL void GL_APIENTRY glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glVertexAttrib3f"));

	GLES2_TIME_START(GLES2_TIMES_glVertexAttrib3f);

	if(index >= GLES2_MAX_VERTEX_ATTRIBS)
	{
		SetError(gc, GL_INVALID_VALUE);
		
		GLES2_TIME_STOP(GLES2_TIMES_glVertexAttrib3f);
		
		return;
	}

	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fX = x;
	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fY = y;
	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fZ = z;
	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fW = GLES2_One;

	GLES2_TIME_STOP(GLES2_TIMES_glVertexAttrib3f);
}


/***********************************************************************************
 Function Name      : glVertexAttrib4f
 Inputs             : index, x, y, z, w
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Specifies current attrib values specified by index
************************************************************************************/
GL_APICALL void GL_APIENTRY glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glVertexAttrib4f"));

	GLES2_TIME_START(GLES2_TIMES_glVertexAttrib4f);
	
	if(index >= GLES2_MAX_VERTEX_ATTRIBS)
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES2_TIME_STOP(GLES2_TIMES_glVertexAttrib4f);

		return;
	}

	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fX = x;
	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fY = y;
	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fZ = z;
	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fW = w;

	GLES2_TIME_STOP(GLES2_TIMES_glVertexAttrib4f);
}


/***********************************************************************************
 Function Name      : glVertexAttrib1fv
 Inputs             : index, values
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Specifies current attrib values specified by index
************************************************************************************/
GL_APICALL void GL_APIENTRY glVertexAttrib1fv(GLuint index, const GLfloat *values)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glVertexAttrib1fv"));

	GLES2_TIME_START(GLES2_TIMES_glVertexAttrib1fv);
	
	if(index >= GLES2_MAX_VERTEX_ATTRIBS)
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES2_TIME_STOP(GLES2_TIMES_glVertexAttrib1fv);

		return;
	}

	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fX = values[0];
	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fY = GLES2_Zero;
	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fZ = GLES2_Zero;
	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fW = GLES2_One;

	GLES2_TIME_STOP(GLES2_TIMES_glVertexAttrib1fv);
}


/***********************************************************************************
 Function Name      : glVertexAttrib2fv
 Inputs             : index, values
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Specifies current attrib values specified by index
************************************************************************************/
GL_APICALL void GL_APIENTRY glVertexAttrib2fv(GLuint index, const GLfloat *values)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glVertexAttrib2fv"));

	GLES2_TIME_START(GLES2_TIMES_glVertexAttrib2fv);

	if(index >= GLES2_MAX_VERTEX_ATTRIBS)
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES2_TIME_STOP(GLES2_TIMES_glVertexAttrib2fv);

		return;
	}

	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fX = values[0];
	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fY = values[1];
	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fZ = GLES2_Zero;
	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fW = GLES2_One;

	GLES2_TIME_STOP(GLES2_TIMES_glVertexAttrib2fv);
}


/***********************************************************************************
 Function Name      : glVertexAttrib3fv
 Inputs             : index, values
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Specifies current attrib values specified by index
************************************************************************************/
GL_APICALL void GL_APIENTRY glVertexAttrib3fv(GLuint index, const GLfloat *values)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glVertexAttrib3fv"));

	GLES2_TIME_START(GLES2_TIMES_glVertexAttrib3fv);

	if(index >= GLES2_MAX_VERTEX_ATTRIBS)
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES2_TIME_STOP(GLES2_TIMES_glVertexAttrib3fv);

		return;
	}

	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fX = values[0];
	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fY = values[1];
	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fZ = values[2];
	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fW = GLES2_One;

	GLES2_TIME_STOP(GLES2_TIMES_glVertexAttrib3fv);
}


/***********************************************************************************
 Function Name      : glVertexAttrib4fv
 Inputs             : index, values
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Specifies current attrib values specified by index
************************************************************************************/

GL_APICALL void GL_APIENTRY glVertexAttrib4fv(GLuint index, const GLfloat *values)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glVertexAttrib4fv"));

	GLES2_TIME_START(GLES2_TIMES_glVertexAttrib4fv);

	if(index >= GLES2_MAX_VERTEX_ATTRIBS)
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES2_TIME_STOP(GLES2_TIMES_glVertexAttrib4fv);

		return;
	}

	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fX = values[0];
	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fY = values[1];
	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fZ = values[2];
	gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fW = values[3];

	GLES2_TIME_STOP(GLES2_TIMES_glVertexAttrib4fv);
}


/***********************************************************************************
 Function Name      : glVertexAttribPointer
 Inputs             : index, size, type, normalized, stride, pointer
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets array pointer/size/type  for attrib data 
					  specified by index
************************************************************************************/

GL_APICALL void GL_APIENTRY glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, 
											  GLsizei stride, const void *pointer)
{
	IMG_UINT32 ui32StreamTypeSize;
	GLES2VertexArrayObject *psVAO;
	GLES2AttribArrayPointerState *psVAOAPState;
	GLES2BufferObject *psCurrentBufObj;
	GLES2BufferObject *psNewBufObj;
	GLES2NamesArray *psNamesArray;	

	__GLES2_GET_CONTEXT();

	/* Setup and assert VAO */
	psVAO = gc->sVAOMachine.psActiveVAO;
	GLES_ASSERT(VAO(gc));


	PVR_DPF((PVR_DBG_CALLTRACE,"glVertexAttribPointer"));

	GLES2_TIME_START(GLES2_TIMES_glVertexAttribPointer);

	if ((stride < 0) || (size < 1) || (size > 4) || (index >= GLES2_MAX_VERTEX_ATTRIBS))
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES2_TIME_STOP(GLES2_TIMES_glVertexAttribPointer);

		return;
	}

	switch (type)
	{
		case GL_BYTE:
		{
			ui32StreamTypeSize = GLES2_STREAMTYPE_BYTE;

			break;
		}
		case GL_SHORT:
		{
			ui32StreamTypeSize = GLES2_STREAMTYPE_SHORT;

			break;
		}
		case GL_UNSIGNED_BYTE:
		{
			ui32StreamTypeSize = GLES2_STREAMTYPE_UBYTE;

			break;
		}
		case GL_UNSIGNED_SHORT:
		{
			ui32StreamTypeSize = GLES2_STREAMTYPE_USHORT;

			break;
		}
		case GL_FLOAT:
		{
			ui32StreamTypeSize = GLES2_STREAMTYPE_FLOAT;

			break;
		}
		case GL_HALF_FLOAT_OES:
		{
			ui32StreamTypeSize = GLES2_STREAMTYPE_HALFFLOAT;

			break;
		}
		case GL_FIXED:
		{

			ui32StreamTypeSize = GLES2_STREAMTYPE_FIXED;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES2_TIME_STOP(GLES2_TIMES_glVertexAttribPointer);

			return;
		}
	}

#if defined(GLES2_EXTENSION_VERTEX_ARRAY_OBJECT)

	/* For any non-zero VAO, return error if VBO is NULL and pointer is not NULL */
	if (!VAO_IS_ZERO(gc))
	{
	    if ((!ARRAY_BUFFER_OBJECT(gc)) && (pointer))
		{
			SetError(gc, GL_INVALID_OPERATION);

			GLES2_TIME_STOP(GLES2_TIMES_glVertexAttribPointer);

			return;
		}
	}
#endif /* defined(GLES2_EXTENSION_VERTEX_ARRAY_OBJECT) */


	if(normalized)
	{
		ui32StreamTypeSize |= GLES2_STREAMNORM_BIT;
	}

	ui32StreamTypeSize |= ((IMG_UINT32)size << GLES2_STREAMSIZE_SHIFT);


	psVAOAPState = &(psVAO->asVAOState[AP_VERTEX_ATTRIB0 + index]);

	
	/* Setup new stride, type, size for VAO's attribute */
	if ((psVAOAPState->ui32UserStride	!= (IMG_UINT32)stride) ||
		(psVAOAPState->ui32StreamTypeSize	!= ui32StreamTypeSize))
	{
		psVAOAPState->ui32UserStride		= (IMG_UINT32)stride;
		psVAOAPState->ui32StreamTypeSize	= ui32StreamTypeSize;

		psVAO->ui32DirtyState |= GLES2_DIRTYFLAG_VAO_ATTRIB_STREAM;
	}

	/* Setup new pointer for VAO's attribute */
	if (psVAOAPState->pui8Pointer != (const IMG_UINT8 *)pointer)
	{
		psVAOAPState->pui8Pointer  = (IMG_UINT8 *)((IMG_UINTPTR_T)pointer);

		psVAO->ui32DirtyState |= GLES2_DIRTYFLAG_VAO_ATTRIB_POINTER;
	}

	/* Setup bufobj names array */
	GLES_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_BUFOBJ]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_BUFOBJ];


	/* Setup the bound VBO for VAO's attribute */

	psCurrentBufObj = psVAOAPState->psBufObj;

	psNewBufObj = gc->sBufferObject.psActiveBuffer[ARRAY_BUFFER_INDEX];

	if(psCurrentBufObj != psNewBufObj)
	{
	    /* Decrease bufobj's RefCount by 1 when unbinding it from the VAO */
	    if (psCurrentBufObj && (psCurrentBufObj->sNamedItem.ui32Name != 0))
		{
			NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)psCurrentBufObj);
		}	

		/* Add bufobj's RefCount by 1 when binding it to the VAO */
		if (psNewBufObj && (psNewBufObj->sNamedItem.ui32Name != 0))
		{
			NamedItemAddRef(psNamesArray, psNewBufObj->sNamedItem.ui32Name);
		}

		psVAOAPState->psBufObj = psNewBufObj;

		psVAO->ui32DirtyState |= GLES2_DIRTYFLAG_VAO_ATTRIB_STREAM;
	}

	GLES2_TIME_STOP(GLES2_TIMES_glVertexAttribPointer);

}

/******************************************************************************
 End of file (vertex.c)
******************************************************************************/

