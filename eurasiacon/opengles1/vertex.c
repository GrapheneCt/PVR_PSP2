/******************************************************************************
 * Name         : vertex.c
 *
 * Copyright    : 2003-2008 by Imagination Technologies Limited.
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
 *****************************************************************************/

#include "context.h"


static IMG_VOID Copy1Byte(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT32 i;
	const IMG_UINT8 *pui8Src = (const IMG_UINT8 *)pvSrc;
	IMG_UINT8 * pui8Dst = (IMG_UINT8 *)pvDst;

	for (i=0; i<ui32Count; i++)
	{
		pui8Dst[0] = pui8Src[0];

		pui8Dst += 1;

		pui8Src += ui32SrcStride;
	}
}


static IMG_VOID Copy3Bytes(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT32 i;
	const IMG_UINT8 *pui8Src = (const IMG_UINT8 *)pvSrc;
	IMG_UINT8 * pui8Dst = (IMG_UINT8 *)pvDst;

	for (i=0; i<ui32Count; i++)
	{
		pui8Dst[0] = pui8Src[0];
		pui8Dst[1] = pui8Src[1];
		pui8Dst[2] = pui8Src[2];

		pui8Dst += 3;

		pui8Src += ui32SrcStride;
	}
}


static IMG_VOID Copy1Short(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	const IMG_UINT16 *pui16Src;
	IMG_UINT16 *pui16Dst;
	IMG_UINT32 i;
    
	pui16Src = (const IMG_UINT16 *)pvSrc;
	pui16Dst = (IMG_UINT16 *)pvDst;

	for (i=0; i<ui32Count; i++)
	{
		pui16Dst[0] = pui16Src[0];

		pui16Dst += 1;

		pui16Src = (const IMG_UINT16 *)((IMG_UINTPTR_T)pui16Src + ui32SrcStride);
	}
}


static IMG_VOID Copy3Shorts(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	const IMG_UINT16 *pui16Src;
	IMG_UINT16 *pui16Dst;
	IMG_UINT32 i;

	pui16Src = (const IMG_UINT16 *)pvSrc;
	pui16Dst = (IMG_UINT16 *)pvDst;

	for (i=0; i<ui32Count; i++)
	{
		pui16Dst[0] = pui16Src[0];
		pui16Dst[1] = pui16Src[1];
		pui16Dst[2] = pui16Src[2];

		pui16Dst += 3;

		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)pui16Src + ui32SrcStride);
	}
}


static IMG_VOID Copy1Long(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	const IMG_UINT32 *pui32Src;
	IMG_UINT32 *pui32Dst;
	IMG_UINT32 i;

	pui32Src = (const IMG_UINT32 *)pvSrc;
	pui32Dst = (IMG_UINT32 *)pvDst;

	for (i=0; i<ui32Count; i++)
	{
		pui32Dst[0] = pui32Src[0];

		pui32Dst += 1;

		pui32Src = (const IMG_UINT32 *)((IMG_UINTPTR_T)pui32Src + ui32SrcStride);
	}
}


static IMG_VOID Copy2Longs(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	const IMG_UINT32 *pui32Src;
	IMG_UINT32 *pui32Dst;
	IMG_UINT32 i;

	pui32Src = (const IMG_UINT32 *)pvSrc;
	pui32Dst = (IMG_UINT32 *)pvDst;

	for (i=0; i<ui32Count; i++)
	{
		pui32Dst[0] = pui32Src[0];
		pui32Dst[1] = pui32Src[1];

		pui32Dst += 2;

		pui32Src = (const IMG_UINT32 *)((IMG_UINTPTR_T)pui32Src + ui32SrcStride);
	}
}


static IMG_VOID Copy3Longs(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	const IMG_UINT32 *pui32Src;
	IMG_UINT32 *pui32Dst;
	IMG_UINT32 i;

	pui32Src = (const IMG_UINT32 *)pvSrc;
	pui32Dst = (IMG_UINT32 *)pvDst;

	for (i=0; i<ui32Count; i++)
	{
		pui32Dst[0] = pui32Src[0];
		pui32Dst[1] = pui32Src[1];
		pui32Dst[2] = pui32Src[2];

		pui32Dst += 3;

		pui32Src = (const IMG_UINT32 *)((IMG_UINTPTR_T)pui32Src + ui32SrcStride);
	}
}


static IMG_VOID Copy4Longs(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	const IMG_UINT32 *pui32Src;
	IMG_UINT32 *pui32Dst;
	IMG_UINT32 i;

	pui32Src = (const IMG_UINT32 *)pvSrc;
	pui32Dst = (IMG_UINT32 *)pvDst;

	for (i=0; i<ui32Count; i++)
	{
		pui32Dst[0] = pui32Src[0];
		pui32Dst[1] = pui32Src[1];
		pui32Dst[2] = pui32Src[2];
		pui32Dst[3] = pui32Src[3];

		pui32Dst += 4;

		pui32Src = (const IMG_UINT32 *)((IMG_UINTPTR_T)pui32Src + ui32SrcStride);
	}
}


static IMG_VOID MemCopy1Byte(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	PVR_UNREFERENCED_PARAMETER(ui32SrcStride);

	GLES1MemCopy(pvDst, pvSrc, ui32Count);
}

static IMG_VOID MemCopy3Bytes(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	PVR_UNREFERENCED_PARAMETER(ui32SrcStride);

	GLES1MemCopy(pvDst, pvSrc, ui32Count*3);
}

static IMG_VOID MemCopy1Short(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	PVR_UNREFERENCED_PARAMETER(ui32SrcStride);

	GLES1MemCopy(pvDst, pvSrc, ui32Count*sizeof(IMG_UINT16));
}

static IMG_VOID MemCopy3Shorts(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	PVR_UNREFERENCED_PARAMETER(ui32SrcStride);

	GLES1MemCopy(pvDst, pvSrc, ui32Count*(3*sizeof(IMG_UINT16)));
}

static IMG_VOID MemCopy1Long(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	PVR_UNREFERENCED_PARAMETER(ui32SrcStride);

	GLES1MemCopy(pvDst, pvSrc, ui32Count*sizeof(IMG_UINT32));
}

static IMG_VOID MemCopy2Longs(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	PVR_UNREFERENCED_PARAMETER(ui32SrcStride);

	GLES1MemCopy(pvDst, pvSrc, ui32Count*(2*sizeof(IMG_UINT32)));
}

static IMG_VOID MemCopy3Longs(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	PVR_UNREFERENCED_PARAMETER(ui32SrcStride);

	GLES1MemCopy(pvDst, pvSrc, ui32Count*(3*sizeof(IMG_UINT32)));
}

static IMG_VOID MemCopy4Longs(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	PVR_UNREFERENCED_PARAMETER(ui32SrcStride);

	GLES1MemCopy(pvDst, pvSrc, ui32Count*(4*sizeof(IMG_UINT32)));
}


IMG_INTERNAL IMG_VOID (* const CopyData[4][GLES1_STREAMTYPE_MAX])(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count) = 
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

IMG_INTERNAL IMG_VOID (* const MemCopyData[4][GLES1_STREAMTYPE_MAX])(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count) = 
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
static IMG_VOID Copy2Bytes(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT32 i;
	const IMG_UINT8 *pui8Src = (const IMG_UINT8 *)pvSrc;
	IMG_UINT8 * pui8Dst = (IMG_UINT8 *)pvDst;

	for (i=0; i<ui32Count; i++)
	{
		pui8Dst[0] = pui8Src[0];
		pui8Dst[1] = pui8Src[1];

		pui8Dst += 2;

		pui8Src += ui32SrcStride;
	}
}


static IMG_VOID Copy4Bytes(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT32 i;
	const IMG_UINT8 *pui8Src = (const IMG_UINT8 *)pvSrc;
	IMG_UINT8 * pui8Dst = (IMG_UINT8 *)pvDst;

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


static IMG_VOID Copy6Bytes(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT32 i, j;
	const IMG_UINT8 *pui8Src = (const IMG_UINT8 *)pvSrc;
	IMG_UINT8 * pui8Dst = (IMG_UINT8 *)pvDst;

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


static IMG_VOID Copy8Bytes(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT32 i, j;
	const IMG_UINT8 *pui8Src = (const IMG_UINT8 *)pvSrc;
	IMG_UINT8 * pui8Dst = (IMG_UINT8 *)pvDst;

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


static IMG_VOID Copy12Bytes(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT32 i, j;
	const IMG_UINT8 *pui8Src = (const IMG_UINT8 *)pvSrc;
	IMG_UINT8 * pui8Dst = (IMG_UINT8 *)pvDst;

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


static IMG_VOID Copy16Bytes(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	IMG_UINT32 i, j;
	const IMG_UINT8 *pui8Src = (const IMG_UINT8 *)pvSrc;
	IMG_UINT8 * pui8Dst = (IMG_UINT8 *)pvDst;

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


static IMG_VOID Copy2Shorts(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	const IMG_UINT16 *pui16Src;
	IMG_UINT16 *pui16Dst;
	IMG_UINT32 i;

	pui16Src = (const IMG_UINT16 *)pvSrc;
	pui16Dst = (IMG_UINT16 *)pvDst;

	for (i=0; i<ui32Count; i++)
	{
		pui16Dst[0] = pui16Src[0];
		pui16Dst[1] = pui16Src[1];

		pui16Dst += 2;
		pui16Src = (IMG_UINT16 *)((IMG_UINT8 *)pui16Src + ui32SrcStride);
	}
}


static IMG_VOID Copy4Shorts(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	const IMG_UINT16 *pui16Src;
	IMG_UINT16 *pui16Dst;
	IMG_UINT32 i;

	pui16Src = (const IMG_UINT16 *)pvSrc;
	pui16Dst = (IMG_UINT16 *)pvDst;

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


static IMG_VOID Copy6Shorts(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	const IMG_UINT16 *pui16Src;
	IMG_UINT16 *pui16Dst;
	IMG_UINT32 i, j;

	pui16Src = (const IMG_UINT16 *)pvSrc;
	pui16Dst = (IMG_UINT16 *)pvDst;

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


static IMG_VOID Copy8Shorts(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count)
{
	const IMG_UINT16 *pui16Src;
	IMG_UINT16 *pui16Dst;
	IMG_UINT32 i, j;

	pui16Src = (const IMG_UINT16 *)pvSrc;
	pui16Dst = (IMG_UINT16 *)pvDst;

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


IMG_VOID (* const CopyDataShortAligned[4][GLES1_STREAMTYPE_MAX])(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count) = 
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

IMG_VOID (* const CopyDataByteAligned[4][GLES1_STREAMTYPE_MAX])(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count) = 
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
 Function Name      : glClientActiveTexture
 Inputs             : texture
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets current texture coordinate array unit.
************************************************************************************/
GL_API void GL_APIENTRY glClientActiveTexture(GLenum texture)
{
	IMG_UINT32 ui32Unit;
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glClientActiveTexture"));
	    
	GLES1_TIME_START(GLES1_TIMES_glClientActiveTexture);

	ui32Unit = texture - GL_TEXTURE0;

	if (ui32Unit >= GLES1_MAX_TEXTURE_UNITS)
	{
		SetError(gc, GL_INVALID_ENUM);
		return;
	}

	gc->sState.ui32ClientActiveTexture = ui32Unit;

	GLES1_TIME_STOP(GLES1_TIMES_glClientActiveTexture);
}


/***********************************************************************************
 Function Name      : glEnableClientState
 Inputs             : array
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Enables coordinate array specified by array
************************************************************************************/
GL_API void GL_APIENTRY glEnableClientState(GLenum array)
{

    GLES1VertexArrayObject *psVAO;

    IMG_UINT32 ui32NewArrayEnables = 0;

	__GLES1_GET_CONTEXT();

	/* Setup and assert VAO */
	psVAO = gc->sVAOMachine.psActiveVAO;
	GLES1_ASSERT(VAO(gc));	


	PVR_DPF((PVR_DBG_CALLTRACE,"glEnableClientState"));

	GLES1_TIME_START(GLES1_TIMES_glEnableClientState);


	switch(array)
	{
		case GL_VERTEX_ARRAY:
		{
			ui32NewArrayEnables |= VARRAY_VERT_ENABLE;

			break;
		}
		case GL_NORMAL_ARRAY:
		{
			ui32NewArrayEnables |= VARRAY_NORMAL_ENABLE;

			break;
		}
		case GL_COLOR_ARRAY:
		{
			ui32NewArrayEnables |= VARRAY_COLOR_ENABLE;

			break;
		}
		case GL_TEXTURE_COORD_ARRAY:
		{
			ui32NewArrayEnables |= (VARRAY_TEXCOORD0_ENABLE<<gc->sState.ui32ClientActiveTexture);

			break;
		}
		case GL_POINT_SIZE_ARRAY_OES:
		{
			ui32NewArrayEnables |= VARRAY_POINTSIZE_ENABLE;

			break;
		}
#if defined(GLES1_EXTENSION_MATRIX_PALETTE)
		case GL_MATRIX_INDEX_ARRAY_OES:
		{
			ui32NewArrayEnables |= VARRAY_MATRIXINDEX_ENABLE;

			break;
		}
		case GL_WEIGHT_ARRAY_OES:
		{
			ui32NewArrayEnables |= VARRAY_WEIGHTARRAY_ENABLE;

			break;
		}
#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			return;
		}
	}

	/* Return if this attribute is enabled before */
	if (psVAO->ui32CurrentArrayEnables & ui32NewArrayEnables)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glEnableClientState);
		return;
	}
	
	/* Setup ui32ArrayEnables for attribute array machine */
	psVAO->ui32CurrentArrayEnables |= ui32NewArrayEnables;


	/* Set dirty flag */
	psVAO->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_CLIENT_STATE;


	GLES1_TIME_STOP(GLES1_TIMES_glEnableClientState);

}


/***********************************************************************************
 Function Name      : glDisableClientState
 Inputs             : array
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Disables coordinate array specified by array.
************************************************************************************/
GL_API void GL_APIENTRY glDisableClientState(GLenum array)
{

   GLES1VertexArrayObject *psVAO;

    IMG_UINT32 ui32NewArrayEnables;


	__GLES1_GET_CONTEXT();

	/* Setup and assert VAO */
	psVAO = gc->sVAOMachine.psActiveVAO;
	GLES1_ASSERT(VAO(gc));


	PVR_DPF((PVR_DBG_CALLTRACE,"glDisableClientState"));

	GLES1_TIME_START(GLES1_TIMES_glDisableClientState);


	switch(array)
	{
		case GL_VERTEX_ARRAY:
		{
			ui32NewArrayEnables = VARRAY_VERT_ENABLE;

			break;
		}
		case GL_NORMAL_ARRAY:
		{
			ui32NewArrayEnables = VARRAY_NORMAL_ENABLE;

			break;
		}
		case GL_COLOR_ARRAY:
		{
			ui32NewArrayEnables = VARRAY_COLOR_ENABLE;

			break;
		}
		case GL_TEXTURE_COORD_ARRAY:
		{
		    ui32NewArrayEnables = (VARRAY_TEXCOORD0_ENABLE<<gc->sState.ui32ClientActiveTexture);

			break;
		}
		case GL_POINT_SIZE_ARRAY_OES:
		{
			ui32NewArrayEnables = VARRAY_POINTSIZE_ENABLE;

			break;
		}
#if defined(GLES1_EXTENSION_MATRIX_PALETTE)
		case GL_MATRIX_INDEX_ARRAY_OES:
		{
			ui32NewArrayEnables = VARRAY_MATRIXINDEX_ENABLE;

			break;
		}
		case GL_WEIGHT_ARRAY_OES:
		{
			ui32NewArrayEnables = VARRAY_WEIGHTARRAY_ENABLE;

			break;
		}
#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
			return;
		}
	}

	/* Return if this attribute is not enabled before */
	if ((psVAO->ui32CurrentArrayEnables & ui32NewArrayEnables) == 0)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glDisableClientState);
		return;
	}

	/* Setup ui32ArrayEnables for attribute array machine */
	psVAO->ui32CurrentArrayEnables &= ~ui32NewArrayEnables;

	/* Set VAO's dirty flag */
	psVAO->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_CLIENT_STATE;


	GLES1_TIME_STOP(GLES1_TIMES_glDisableClientState);

}


/***********************************************************************************
 Function Name      : glColor4f
 Inputs             : red, green, blue, alpha
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets current color
************************************************************************************/
#if defined(PROFILE_COMMON)
GL_API void GL_APIENTRY glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glColor4f"));

	GLES1_TIME_START(GLES1_TIMES_glColor4f);

	gc->sState.sCurrent.asAttrib[AP_COLOR].fX = red;
	gc->sState.sCurrent.asAttrib[AP_COLOR].fY = green;
	gc->sState.sCurrent.asAttrib[AP_COLOR].fZ = blue;
	gc->sState.sCurrent.asAttrib[AP_COLOR].fW = alpha;

	/* Update material is color_material is enabled */
	if(gc->ui32TnLEnables & GLES1_TL_COLORMAT_ENABLE)
	{
		Materialfv(gc, GL_FRONT_AND_BACK, gc->sState.sLight.eColorMaterialParam, &gc->sState.sCurrent.asAttrib[AP_COLOR].fX);
	}

	GLES1_TIME_STOP(GLES1_TIMES_glColor4f);
}
#endif

/***********************************************************************************
 Function Name      : glColor4x
 Inputs             : red, green, blue, alpha
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets current color
************************************************************************************/
GL_API void GL_APIENTRY glColor4x(GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glColor4x"));

	GLES1_TIME_START(GLES1_TIMES_glColor4x);

	gc->sState.sCurrent.asAttrib[AP_COLOR].fX = FIXED_TO_FLOAT(red);
	gc->sState.sCurrent.asAttrib[AP_COLOR].fY = FIXED_TO_FLOAT(green);
	gc->sState.sCurrent.asAttrib[AP_COLOR].fZ = FIXED_TO_FLOAT(blue);
	gc->sState.sCurrent.asAttrib[AP_COLOR].fW = FIXED_TO_FLOAT(alpha);

	/* Update material is color_material is enabled */
	if(gc->ui32TnLEnables & GLES1_TL_COLORMAT_ENABLE)
	{
		Materialfv(gc, GL_FRONT_AND_BACK, gc->sState.sLight.eColorMaterialParam, &gc->sState.sCurrent.asAttrib[AP_COLOR].fX);
	}

	GLES1_TIME_STOP(GLES1_TIMES_glColor4x);
}

/***********************************************************************************
 Function Name      : glColor4ub
 Inputs             : red, green, blue, alpha
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets current color
************************************************************************************/
GL_API void GL_APIENTRY glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glColor4ub"));

	GLES1_TIME_START(GLES1_TIMES_glColor4ub);

	gc->sState.sCurrent.asAttrib[AP_COLOR].fX = GLES1_UB_TO_FLOAT(red);
	gc->sState.sCurrent.asAttrib[AP_COLOR].fY = GLES1_UB_TO_FLOAT(green);
	gc->sState.sCurrent.asAttrib[AP_COLOR].fZ = GLES1_UB_TO_FLOAT(blue);
	gc->sState.sCurrent.asAttrib[AP_COLOR].fW = GLES1_UB_TO_FLOAT(alpha);

	/* Update material is color_material is enabled */
	if(gc->ui32TnLEnables & GLES1_TL_COLORMAT_ENABLE)
	{
		Materialfv(gc, GL_FRONT_AND_BACK, gc->sState.sLight.eColorMaterialParam, &gc->sState.sCurrent.asAttrib[AP_COLOR].fX);
	}

	GLES1_TIME_STOP(GLES1_TIMES_glColor4ub);
}

/***********************************************************************************
 Function Name      : glMultiTexCoord4f
 Inputs             : target, s, t, r, q
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets current texture coordinate on unit specified
					  by target.
************************************************************************************/
#if defined(PROFILE_COMMON)
GL_API void GL_APIENTRY glMultiTexCoord4f(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
	IMG_UINT32 ui32Unit = target - GL_TEXTURE0;
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glMultiTexCoord4f"));

	GLES1_TIME_START(GLES1_TIMES_glMultiTexCoord4f);

	if (ui32Unit >= GLES1_MAX_TEXTURE_UNITS)
	{
		SetError(gc, GL_INVALID_ENUM);
		return;
	}

	gc->sState.sCurrent.asAttrib[AP_TEXCOORD0 + ui32Unit].fX = s;
	gc->sState.sCurrent.asAttrib[AP_TEXCOORD0 + ui32Unit].fY = t;
	gc->sState.sCurrent.asAttrib[AP_TEXCOORD0 + ui32Unit].fZ = r;
	gc->sState.sCurrent.asAttrib[AP_TEXCOORD0 + ui32Unit].fW = q;

	GLES1_TIME_STOP(GLES1_TIMES_glMultiTexCoord4f);
}
#endif

/***********************************************************************************
 Function Name      : glMultiTexCoord4x
 Inputs             : target, s, t, r, q
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets current texture coordinate on unit specified
					  by target.
************************************************************************************/
GL_API void GL_APIENTRY glMultiTexCoord4x(GLenum target, GLfixed s, GLfixed t, GLfixed r, GLfixed q)
{
	IMG_UINT32 ui32Unit = target - GL_TEXTURE0;
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glMultiTexCoord4x"));

	GLES1_TIME_START(GLES1_TIMES_glMultiTexCoord4x);

	if (ui32Unit >= GLES1_MAX_TEXTURE_UNITS)
	{
		SetError(gc, GL_INVALID_ENUM);
		return;
	}

	gc->sState.sCurrent.asAttrib[AP_TEXCOORD0 + ui32Unit].fX = FIXED_TO_FLOAT(s);
	gc->sState.sCurrent.asAttrib[AP_TEXCOORD0 + ui32Unit].fY = FIXED_TO_FLOAT(t);
	gc->sState.sCurrent.asAttrib[AP_TEXCOORD0 + ui32Unit].fZ = FIXED_TO_FLOAT(r);
	gc->sState.sCurrent.asAttrib[AP_TEXCOORD0 + ui32Unit].fW = FIXED_TO_FLOAT(q);

	GLES1_TIME_STOP(GLES1_TIMES_glMultiTexCoord4x);
}


/***********************************************************************************
 Function Name      : glNormal3f
 Inputs             : nx, ny, nz
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets current normal.
************************************************************************************/
#if defined(PROFILE_COMMON)
GL_API void GL_APIENTRY glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glNormal3f"));

	GLES1_TIME_START(GLES1_TIMES_glNormal3f);

	gc->sState.sCurrent.asAttrib[AP_NORMAL].fX = nx;
	gc->sState.sCurrent.asAttrib[AP_NORMAL].fY = ny;
	gc->sState.sCurrent.asAttrib[AP_NORMAL].fZ = nz;

	GLES1_TIME_STOP(GLES1_TIMES_glNormal3f);
}
#endif

/***********************************************************************************
 Function Name      : glNormal3x
 Inputs             : nx, ny, nz
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets current normal.
************************************************************************************/
GL_API void GL_APIENTRY glNormal3x(GLfixed nx, GLfixed ny, GLfixed nz)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glNormal3x"));

	GLES1_TIME_START(GLES1_TIMES_glNormal3x);

	gc->sState.sCurrent.asAttrib[AP_NORMAL].fX = FIXED_TO_FLOAT(nx);
	gc->sState.sCurrent.asAttrib[AP_NORMAL].fY = FIXED_TO_FLOAT(ny);
	gc->sState.sCurrent.asAttrib[AP_NORMAL].fZ = FIXED_TO_FLOAT(nz);

	GLES1_TIME_STOP(GLES1_TIMES_glNormal3x);
}




/***********************************************************************************
 Function Name      : glColorPointer
 Inputs             : size, type, stride, pointer
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets array pointer/size/type  for color data
************************************************************************************/
GL_API void GL_APIENTRY glColorPointer	(GLint			size,
										 GLenum			type,
										 GLsizei		stride,
										 const GLvoid	*pointer)
{
	GLES1VertexArrayObject       *psVAO;
	GLES1AttribArrayPointerState *psVAOAPState;

	IMG_UINT32					 ui32StreamTypeSize;
	GLESBufferObject			*psCurrentBufObj, *psNewBufObj;

	GLES1NamesArray *psNamesArray;	

	__GLES1_GET_CONTEXT();

	/* Setup and assert VAO */
	psVAO = gc->sVAOMachine.psActiveVAO;
	GLES1_ASSERT(VAO(gc));


	PVR_DPF((PVR_DBG_CALLTRACE,"glColorPointer"));

	GLES1_TIME_START(GLES1_TIMES_glColorPointer);

	if ((stride < 0) || (size != 4))
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES1_TIME_STOP(GLES1_TIMES_glColorPointer);

		return;
	}

	switch (type)
	{
		case GL_UNSIGNED_BYTE:
		{
			/* Colors are normalized in the range 0..1 */
			ui32StreamTypeSize = GLES1_STREAMTYPE_UBYTE | GLES1_STREAMNORM_BIT;

			break;
		}
#if defined(PROFILE_COMMON)
		case GL_FLOAT:
		{
			ui32StreamTypeSize = GLES1_STREAMTYPE_FLOAT;

			break;
		}
#endif
		case GL_FIXED:
		{
			ui32StreamTypeSize = GLES1_STREAMTYPE_FIXED;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES1_TIME_STOP(GLES1_TIMES_glColorPointer);

			return;
		}
	}

#if defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT)

	/* For any non-zero VAO, return error if VBO is NULL and pointer is not NULL */
	if (!VAO_IS_ZERO(gc))
	{
	    if ((!ARRAY_BUFFER_OBJECT(gc)) && (pointer))
		{
			SetError(gc, GL_INVALID_OPERATION);

			GLES1_TIME_STOP(GLES1_TIMES_glColorPointer);

			return;
		}
	}
#endif /* defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT) */


	ui32StreamTypeSize |= ((IMG_UINT32)size << GLES1_STREAMSIZE_SHIFT);

	psVAOAPState = &(psVAO->asVAOState[AP_COLOR]);


	/* Setup new stride, type, size for VAO's attribute */
	if ((psVAOAPState->ui32UserStride	!= (IMG_UINT32)stride) ||
		(psVAOAPState->ui32StreamTypeSize	!= ui32StreamTypeSize))
	{
		psVAOAPState->ui32UserStride		= (IMG_UINT32)stride;
		psVAOAPState->ui32StreamTypeSize	= ui32StreamTypeSize;

		psVAO->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ATTRIB_STREAM;
	}

	if (psVAOAPState->pui8Pointer	!= (const IMG_UINT8 *) pointer)
	{
		psVAOAPState->pui8Pointer	= (const IMG_UINT8 *) pointer;

		psVAO->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ATTRIB_POINTER;
	}

	/* Setup bufobj names array */
	GLES1_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_BUFOBJ]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_BUFOBJ];


	/* Setup the bound VBO for VAO's attribute */
	psCurrentBufObj = psVAOAPState->psBufObj;

	psNewBufObj = gc->sBufferObject.psActiveBuffer[ARRAY_BUFFER_INDEX];

	if(psCurrentBufObj != psNewBufObj)
	{
	    /* Decrease bufobj's RefCount by 1 when unbinding it from the VAO */
	    if (psCurrentBufObj && (psCurrentBufObj->sNamedItem.ui32Name != 0))
		{
			NamedItemDelRef(gc, psNamesArray, (GLES1NamedItem*)psCurrentBufObj);
		}	

		/* Add bufobj's RefCount by 1 when binding it to the VAO */
		if (psNewBufObj && (psNewBufObj->sNamedItem.ui32Name != 0))
		{
			NamedItemAddRef(psNamesArray, psNewBufObj->sNamedItem.ui32Name);
		}

		psVAOAPState->psBufObj = psNewBufObj;

		psVAO->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ATTRIB_STREAM;
	}

	GLES1_TIME_STOP(GLES1_TIMES_glColorPointer);

}


/***********************************************************************************
 Function Name      : glNormalPointer
 Inputs             : type, stride, pointer
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets array pointer/type  for normal data
************************************************************************************/
GL_API void GL_APIENTRY glNormalPointer	(GLenum			type,
									 GLsizei		stride,
									 const GLvoid	*pointer)
{
	GLES1VertexArrayObject       *psVAO;
	GLES1AttribArrayPointerState *psVAOAPState;

	IMG_UINT32					ui32StreamTypeSize;
	GLESBufferObject			*psCurrentBufObj, *psNewBufObj;

	GLES1NamesArray *psNamesArray;	


	__GLES1_GET_CONTEXT();

	/* Setup and assert VAO */
	psVAO = gc->sVAOMachine.psActiveVAO;
	GLES1_ASSERT(VAO(gc));


	PVR_DPF((PVR_DBG_CALLTRACE,"glNormalPointer"));

	GLES1_TIME_START(GLES1_TIMES_glNormalPointer);

	if (stride < 0)
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES1_TIME_STOP(GLES1_TIMES_glNormalPointer);

		return;
	}

	switch (type)
	{
		case GL_BYTE:
		{
			/* Normals will be normalized in the range -1..1 in the VGP*/
			ui32StreamTypeSize = GLES1_STREAMTYPE_BYTE | GLES1_STREAMNORM_BIT;

			break;
		}
		case GL_SHORT:
		{
			/* Normals will be normalized in the range -1..1 in the VGP*/
			ui32StreamTypeSize = GLES1_STREAMTYPE_SHORT | GLES1_STREAMNORM_BIT;

			break;
		}
#if defined(PROFILE_COMMON)
		case GL_FLOAT:
		{
			ui32StreamTypeSize = GLES1_STREAMTYPE_FLOAT;

			break;
		}
#endif
		case GL_FIXED:
		{
			ui32StreamTypeSize = GLES1_STREAMTYPE_FIXED;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES1_TIME_STOP(GLES1_TIMES_glNormalPointer);

			return;
		}
	}

#if defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT)

	/* For any non-zero VAO, return error if VBO is NULL and pointer is not NULL */
	if (!VAO_IS_ZERO(gc))
	{
	    if ((!ARRAY_BUFFER_OBJECT(gc)) && (pointer))
		{
			SetError(gc, GL_INVALID_OPERATION);

			GLES1_TIME_STOP(GLES1_TIMES_glNormalPointer);

			return;
		}
	}
#endif /* defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT) */


	ui32StreamTypeSize |= (3 << GLES1_STREAMSIZE_SHIFT);

	psVAOAPState = &(psVAO->asVAOState[AP_NORMAL]);


	/* Setup new stride, type, size for VAO's attribute */
	if ((psVAOAPState->ui32UserStride	!= (IMG_UINT32)stride) ||
		(psVAOAPState->ui32StreamTypeSize	!= ui32StreamTypeSize))
	{
		psVAOAPState->ui32UserStride		= (IMG_UINT32)stride;
		psVAOAPState->ui32StreamTypeSize	= ui32StreamTypeSize;

		psVAO->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ATTRIB_STREAM;
	}

	if (psVAOAPState->pui8Pointer	!= (const IMG_UINT8 *) pointer)
	{
		psVAOAPState->pui8Pointer	= (const IMG_UINT8 *) pointer;

		psVAO->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ATTRIB_POINTER;
	}

	/* Setup bufobj names array */
	GLES1_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_BUFOBJ]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_BUFOBJ];


	/* Setup the bound VBO for VAO's attribute */
	psCurrentBufObj = psVAOAPState->psBufObj;

	psNewBufObj = gc->sBufferObject.psActiveBuffer[ARRAY_BUFFER_INDEX];

	if(psCurrentBufObj != psNewBufObj)
	{
	    /* Decrease bufobj's RefCount by 1 when unbinding it from the VAO */
	    if (psCurrentBufObj && (psCurrentBufObj->sNamedItem.ui32Name != 0))
		{
			NamedItemDelRef(gc, psNamesArray, (GLES1NamedItem*)psCurrentBufObj);
		}	

		/* Add bufobj's RefCount by 1 when binding it to the VAO */
		if (psNewBufObj && (psNewBufObj->sNamedItem.ui32Name != 0))
		{
			NamedItemAddRef(psNamesArray, psNewBufObj->sNamedItem.ui32Name);
		}

		psVAOAPState->psBufObj = psNewBufObj;

		psVAO->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ATTRIB_STREAM;
	}

	GLES1_TIME_STOP(GLES1_TIMES_glNormalPointer);

}


/***********************************************************************************
 Function Name      : glTexCoordPointer
 Inputs             : size, type, stride, pointer
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets array pointer/size/type  for texcoord data.
					  Applies to active texture coordinate array unit.
************************************************************************************/
GL_API void GL_APIENTRY glTexCoordPointer	(GLint			size,
										 GLenum			type,
										 GLsizei		stride,
										 const GLvoid	*pointer)
{
	GLES1VertexArrayObject       *psVAO;
	GLES1AttribArrayPointerState *psVAOAPState;

	IMG_UINT32					 ui32StreamTypeSize, ui32AttribNumber;
	GLESBufferObject			 *psCurrentBufObj, *psNewBufObj;

	GLES1NamesArray *psNamesArray;	


	__GLES1_GET_CONTEXT();

	/* Setup and assert VAO */
	psVAO = gc->sVAOMachine.psActiveVAO;
	GLES1_ASSERT(VAO(gc));


	PVR_DPF((PVR_DBG_CALLTRACE,"glTexCoordPointer"));

	GLES1_TIME_START(GLES1_TIMES_glTexCoordPointer);

	if ((stride < 0) || (size < 2) || (size > 4))
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES1_TIME_STOP(GLES1_TIMES_glTexCoordPointer);

		return;
	}

	switch (type)
	{
		case GL_BYTE:
		{
			ui32StreamTypeSize = GLES1_STREAMTYPE_BYTE;

			break;
		}
		case GL_SHORT:
		{

			ui32StreamTypeSize = GLES1_STREAMTYPE_SHORT;

			break;
		}
#if defined(PROFILE_COMMON)
		case GL_FLOAT:
		{
			ui32StreamTypeSize = GLES1_STREAMTYPE_FLOAT;

			break;
		}
#endif
		case GL_FIXED:
		{
			ui32StreamTypeSize = GLES1_STREAMTYPE_FIXED;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES1_TIME_STOP(GLES1_TIMES_glTexCoordPointer);

			return;
		}
	}

#if defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT)

	/* For any non-zero VAO, return error if VBO is NULL and pointer is not NULL */
	if (!VAO_IS_ZERO(gc))
	{
	    if ((!ARRAY_BUFFER_OBJECT(gc)) && (pointer))
		{
			SetError(gc, GL_INVALID_OPERATION);

			GLES1_TIME_STOP(GLES1_TIMES_glTexCoordPointer);

			return;
		}
	}
#endif /* defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT) */


	ui32StreamTypeSize |= ((IMG_UINT32)size << GLES1_STREAMSIZE_SHIFT);

	ui32AttribNumber = (AP_TEXCOORD0 + gc->sState.ui32ClientActiveTexture);

	psVAOAPState = &(psVAO->asVAOState[ui32AttribNumber]);


	/* Setup new stride, type, size for VAO's attribute */
	if ((psVAOAPState->ui32UserStride	!= (IMG_UINT32)stride) ||
		(psVAOAPState->ui32StreamTypeSize	!= ui32StreamTypeSize))
	{
		psVAOAPState->ui32UserStride		= (IMG_UINT32)stride;
		psVAOAPState->ui32StreamTypeSize	= ui32StreamTypeSize;

		psVAO->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ATTRIB_STREAM;

		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;
	}

	if (psVAOAPState->pui8Pointer	!= (const IMG_UINT8 *) pointer)
	{
		psVAOAPState->pui8Pointer	= (const IMG_UINT8 *) pointer;

		psVAO->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ATTRIB_POINTER;
	}

	/* Setup bufobj names array */
	GLES1_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_BUFOBJ]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_BUFOBJ];


	/* Setup the bound VBO for VAO's attribute */
	psCurrentBufObj = psVAOAPState->psBufObj;

	psNewBufObj = gc->sBufferObject.psActiveBuffer[ARRAY_BUFFER_INDEX];

	if(psCurrentBufObj != psNewBufObj)
	{
	    /* Decrease bufobj's RefCount by 1 when unbinding it from the VAO */
	    if (psCurrentBufObj && (psCurrentBufObj->sNamedItem.ui32Name != 0))
		{
			NamedItemDelRef(gc, psNamesArray, (GLES1NamedItem*)psCurrentBufObj);
		}	

		/* Add bufobj's RefCount by 1 when binding it to the VAO */
		if (psNewBufObj && (psNewBufObj->sNamedItem.ui32Name != 0))
		{
			NamedItemAddRef(psNamesArray, psNewBufObj->sNamedItem.ui32Name);
		}

		psVAOAPState->psBufObj = psNewBufObj;

		psVAO->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ATTRIB_STREAM;
	}

	GLES1_TIME_STOP(GLES1_TIMES_glTexCoordPointer);

}


/***********************************************************************************
 Function Name      : glVertexPointer
 Inputs             : size, type, stride, pointer
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets array pointer/size/type  for vertex data
************************************************************************************/
GL_API void GL_APIENTRY glVertexPointer	(GLint			size,
									 GLenum			type,
									 GLsizei		stride,
									 const GLvoid	*pointer)
{
	GLES1VertexArrayObject       *psVAO;
	GLES1AttribArrayPointerState *psVAOAPState;

	IMG_UINT32					ui32StreamTypeSize;
	GLESBufferObject			*psCurrentBufObj, *psNewBufObj;

	GLES1NamesArray *psNamesArray;	


	__GLES1_GET_CONTEXT();

	/* Setup and assert VAO */
	psVAO = gc->sVAOMachine.psActiveVAO;
	GLES1_ASSERT(VAO(gc));


	PVR_DPF((PVR_DBG_CALLTRACE,"glVertexPointer"));

	GLES1_TIME_START(GLES1_TIMES_glVertexPointer);

	if ((stride < 0) || (size < 2) || (size > 4))
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES1_TIME_STOP(GLES1_TIMES_glVertexPointer);

		return;
	}

	switch (type)
	{
		case GL_BYTE:
		{
			ui32StreamTypeSize = GLES1_STREAMTYPE_BYTE;

			break;
		}
		case GL_SHORT:
		{

			ui32StreamTypeSize = GLES1_STREAMTYPE_SHORT;

			break;
		}
#if defined(PROFILE_COMMON)
		case GL_FLOAT:
		{
			ui32StreamTypeSize = GLES1_STREAMTYPE_FLOAT;

			break;
		}
#endif
		case GL_FIXED:
		{
			ui32StreamTypeSize = GLES1_STREAMTYPE_FIXED;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES1_TIME_STOP(GLES1_TIMES_glVertexPointer);

			return;
		}
	}

#if defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT)

	/* For any non-zero VAO, return error if VBO is NULL and pointer is not NULL */
	if (!VAO_IS_ZERO(gc))
	{
	    if ((!ARRAY_BUFFER_OBJECT(gc)) && (pointer))
		{
			SetError(gc, GL_INVALID_OPERATION);

			GLES1_TIME_STOP(GLES1_TIMES_glVertexPointer);

			return;
		}
	}
#endif /* defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT) */


	ui32StreamTypeSize |= ((IMG_UINT32)size << GLES1_STREAMSIZE_SHIFT);

	psVAOAPState = &(psVAO->asVAOState[AP_VERTEX]);


	/* Setup new stride, type, size for VAO's attribute */
	if ((psVAOAPState->ui32UserStride	!= (IMG_UINT32)stride) ||
		(psVAOAPState->ui32StreamTypeSize	!= ui32StreamTypeSize))
	{
		psVAOAPState->ui32UserStride		= (IMG_UINT32)stride;
		psVAOAPState->ui32StreamTypeSize	= ui32StreamTypeSize;

		psVAO->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ATTRIB_STREAM;
	}

	if (psVAOAPState->pui8Pointer	!= (const IMG_UINT8 *) pointer)
	{
		psVAOAPState->pui8Pointer	= (const IMG_UINT8 *) pointer;

		psVAO->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ATTRIB_POINTER;
	}

	/* Setup bufobj names array */
	GLES1_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_BUFOBJ]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_BUFOBJ];


	/* Setup the bound VBO for VAO's attribute */
	psCurrentBufObj = psVAOAPState->psBufObj;

	psNewBufObj = gc->sBufferObject.psActiveBuffer[ARRAY_BUFFER_INDEX];

	if(psCurrentBufObj != psNewBufObj)
	{
	    /* Decrease bufobj's RefCount by 1 when unbinding it from the VAO */
	    if (psCurrentBufObj && (psCurrentBufObj->sNamedItem.ui32Name != 0))
		{
			NamedItemDelRef(gc, psNamesArray, (GLES1NamedItem*)psCurrentBufObj);
		}	

		/* Add bufobj's RefCount by 1 when binding it to the VAO */
		if (psNewBufObj && (psNewBufObj->sNamedItem.ui32Name != 0))
		{
			NamedItemAddRef(psNamesArray, psNewBufObj->sNamedItem.ui32Name);
		}

		psVAOAPState->psBufObj = psNewBufObj;

		psVAO->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ATTRIB_STREAM;
	}

	GLES1_TIME_STOP(GLES1_TIMES_glVertexPointer);

}


/***********************************************************************************
 Function Name      : glPointSizePointerOES
 Inputs             : type, stride, pointer
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets array pointer/type  for point size data
************************************************************************************/
GL_API void GL_APIENTRY glPointSizePointerOES	(GLenum			type,
											 GLsizei		stride,
											 const GLvoid	*pointer)
{
	GLES1VertexArrayObject       *psVAO;
	GLES1AttribArrayPointerState *psVAOAPState;

	IMG_UINT32					ui32StreamTypeSize;
	GLESBufferObject			*psCurrentBufObj, *psNewBufObj;

	GLES1NamesArray *psNamesArray;	


	__GLES1_GET_CONTEXT();

	/* Setup and assert VAO */
	psVAO = gc->sVAOMachine.psActiveVAO;
	GLES1_ASSERT(VAO(gc));


	PVR_DPF((PVR_DBG_CALLTRACE,"glPointSizePointerOES"));

	GLES1_TIME_START(GLES1_TIMES_glPointSizePointerOES);

	if (stride < 0) 
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES1_TIME_STOP(GLES1_TIMES_glPointSizePointerOES);

		return;
	}

	switch (type)
	{
#if defined(PROFILE_COMMON)
		case GL_FLOAT:
		{
			ui32StreamTypeSize = GLES1_STREAMTYPE_FLOAT;

			break;
		}
#endif
		case GL_FIXED:
		{
			ui32StreamTypeSize = GLES1_STREAMTYPE_FIXED;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES1_TIME_STOP(GLES1_TIMES_glPointSizePointerOES);

			return;
		}
	}

#if defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT)

	/* For any non-zero VAO, return error if VBO is NULL and pointer is not NULL */
	if (!VAO_IS_ZERO(gc))
	{
	    if ((!ARRAY_BUFFER_OBJECT(gc)) && (pointer))
		{
			SetError(gc, GL_INVALID_OPERATION);

			GLES1_TIME_STOP(GLES1_TIMES_glPointSizePointerOES);

			return;
		}
	}
#endif /* defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT) */


	ui32StreamTypeSize |= (1 << GLES1_STREAMSIZE_SHIFT);

	psVAOAPState = &(psVAO->asVAOState[AP_POINTSIZE]);


	/* Setup new stride, type, size for VAO's attribute */
	if ((psVAOAPState->ui32UserStride	!= (IMG_UINT32)stride) ||
		(psVAOAPState->ui32StreamTypeSize	!= ui32StreamTypeSize))
	{
		psVAOAPState->ui32UserStride		= (IMG_UINT32)stride;
		psVAOAPState->ui32StreamTypeSize	= ui32StreamTypeSize;

		psVAO->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ATTRIB_STREAM;
	}

	if (psVAOAPState->pui8Pointer	!= (const IMG_UINT8 *) pointer)
	{
		psVAOAPState->pui8Pointer	= (const IMG_UINT8 *) pointer;

		psVAO->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ATTRIB_POINTER;
	}

	/* Setup bufobj names array */
	GLES1_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_BUFOBJ]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_BUFOBJ];

	/* Setup the bound VBO for VAO's attribute */
	psCurrentBufObj = psVAOAPState->psBufObj;

	psNewBufObj = gc->sBufferObject.psActiveBuffer[ARRAY_BUFFER_INDEX];

	if(psCurrentBufObj != psNewBufObj)
	{
	    /* Decrease bufobj's RefCount by 1 when unbinding it from the VAO */
	    if (psCurrentBufObj && (psCurrentBufObj->sNamedItem.ui32Name != 0))
		{
			NamedItemDelRef(gc, psNamesArray, (GLES1NamedItem*)psCurrentBufObj);
		}	

		/* Add bufobj's RefCount by 1 when binding it to the VAO */
		if (psNewBufObj && (psNewBufObj->sNamedItem.ui32Name != 0))
		{
			NamedItemAddRef(psNamesArray, psNewBufObj->sNamedItem.ui32Name);
		}

		psVAOAPState->psBufObj = psNewBufObj;

		psVAO->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ATTRIB_STREAM;
	}

	GLES1_TIME_STOP(GLES1_TIMES_glPointSizePointerOES);

}



#if defined(GLES1_EXTENSION_MATRIX_PALETTE)

/***********************************************************************************
 Function Name      : glMatrixIndexPointerOES
 Inputs             : size, type, stride, pointer
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets array pointer/size/type  for matrix index data
************************************************************************************/
GL_API_EXT void GL_APIENTRY glMatrixIndexPointerOES(GLint			size,
													GLenum			type,
													GLsizei			stride,
													const GLvoid	*pointer)
{
	GLES1VertexArrayObject       *psVAO;
	GLES1AttribArrayPointerState *psVAOAPState;

	IMG_UINT32					ui32StreamTypeSize;
	GLESBufferObject			*psCurrentBufObj, *psNewBufObj;

	GLES1NamesArray *psNamesArray;	


	__GLES1_GET_CONTEXT();

	/* Setup and assert VAO */
	psVAO = gc->sVAOMachine.psActiveVAO;
	GLES1_ASSERT(VAO(gc));


	PVR_DPF((PVR_DBG_CALLTRACE,"glMatrixIndexPointerOES"));

	GLES1_TIME_START(GLES1_TIMES_glMatrixIndexPointerOES);

	if ((stride < 0) || (size <= 0) || (size > GLES1_MAX_VERTEX_UNITS))
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES1_TIME_STOP(GLES1_TIMES_glMatrixIndexPointerOES);

		return;
	}

	switch (type)
	{
		case GL_UNSIGNED_BYTE:
		{
			ui32StreamTypeSize = GLES1_STREAMTYPE_UBYTE;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES1_TIME_STOP(GLES1_TIMES_glMatrixIndexPointerOES);

			return;
		}
	}

#if defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT)

	/* For any non-zero VAO, return error if VBO is NULL and pointer is not NULL */
	if (!VAO_IS_ZERO(gc))
	{
	    if ((!ARRAY_BUFFER_OBJECT(gc)) && (pointer))
		{
			SetError(gc, GL_INVALID_OPERATION);

			GLES1_TIME_STOP(GLES1_TIMES_glMatrixIndexPointerOES);

			return;
		}
	}
#endif /* defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT) */


	ui32StreamTypeSize |= ((IMG_UINT32)size << GLES1_STREAMSIZE_SHIFT);

	psVAOAPState = &(psVAO->asVAOState[AP_MATRIXINDEX]);


	/* Setup new stride, type, size for VAO's attribute */
	if ((psVAOAPState->ui32UserStride	!= (IMG_UINT32)stride) ||
		(psVAOAPState->ui32StreamTypeSize	!= ui32StreamTypeSize))
	{
		psVAOAPState->ui32UserStride		= (IMG_UINT32)stride;
		psVAOAPState->ui32StreamTypeSize	= ui32StreamTypeSize;

		psVAO->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ATTRIB_STREAM;
	}

	if (psVAOAPState->pui8Pointer	!= (const IMG_UINT8 *) pointer)
	{
		psVAOAPState->pui8Pointer	= (const IMG_UINT8 *) pointer;

		psVAO->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ATTRIB_POINTER;
	}

	/* Setup bufobj names array */
	GLES1_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_BUFOBJ]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_BUFOBJ];


	/* Setup the bound VBO for VAO's attribute */
	psCurrentBufObj = psVAOAPState->psBufObj;

	psNewBufObj = gc->sBufferObject.psActiveBuffer[ARRAY_BUFFER_INDEX];

	if(psCurrentBufObj != psNewBufObj)
	{
	    /* Decrease bufobj's RefCount by 1 when unbinding it from the VAO */
	    if (psCurrentBufObj && (psCurrentBufObj->sNamedItem.ui32Name != 0))
		{
			NamedItemDelRef(gc, psNamesArray, (GLES1NamedItem*)psCurrentBufObj);
		}	

		/* Add bufobj's RefCount by 1 when binding it to the VAO */
		if (psNewBufObj && (psNewBufObj->sNamedItem.ui32Name != 0))
		{
			NamedItemAddRef(psNamesArray, psNewBufObj->sNamedItem.ui32Name);
		}

		psVAOAPState->psBufObj = psNewBufObj;

		psVAO->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ATTRIB_STREAM;
	}

	GLES1_TIME_STOP(GLES1_TIMES_glMatrixIndexPointerOES);
}


/***********************************************************************************
 Function Name      : glWeightPointerOES
 Inputs             : size, type, stride, pointer
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets array pointer/size/type for weight data
************************************************************************************/
GL_API_EXT void GL_APIENTRY glWeightPointerOES(GLint		size,
											   GLenum		type,
											   GLsizei		stride,
											   const GLvoid	*pointer)
{
	GLES1VertexArrayObject       *psVAO;
	GLES1AttribArrayPointerState *psVAOAPState;

	IMG_UINT32					ui32StreamTypeSize;
	GLESBufferObject			*psCurrentBufObj, *psNewBufObj;

	GLES1NamesArray *psNamesArray;	


	__GLES1_GET_CONTEXT();

	/* Setup and assert VAO */
	psVAO = gc->sVAOMachine.psActiveVAO;
	GLES1_ASSERT(VAO(gc));


	PVR_DPF((PVR_DBG_CALLTRACE,"glWeightPointerOES"));
	    
	GLES1_TIME_START(GLES1_TIMES_glWeightPointerOES);

	if ((stride < 0) || (size <= 0) || (size > GLES1_MAX_VERTEX_UNITS))
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES1_TIME_STOP(GLES1_TIMES_glWeightPointerOES);

		return;
	}

	switch (type)
	{
#if defined(PROFILE_COMMON)
		case GL_FLOAT:
		{
			ui32StreamTypeSize = GLES1_STREAMTYPE_FLOAT;

			break;
		}
#endif
		case GL_FIXED:
		{
			ui32StreamTypeSize = GLES1_STREAMTYPE_FIXED;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES1_TIME_STOP(GLES1_TIMES_glWeightPointerOES);

			return;
		}
	}

#if defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT)

	/* For any non-zero VAO, return error if VBO is NULL and pointer is not NULL */
	if (!VAO_IS_ZERO(gc))
	{
	    if ((!ARRAY_BUFFER_OBJECT(gc)) && (pointer))
		{
			SetError(gc, GL_INVALID_OPERATION);

			GLES1_TIME_STOP(GLES1_TIMES_glWeightPointerOES);

			return;
		}
	}
#endif /* defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT) */


	ui32StreamTypeSize |= ((IMG_UINT32)size << GLES1_STREAMSIZE_SHIFT);

	psVAOAPState = &(psVAO->asVAOState[AP_WEIGHTARRAY]);


	/* Setup new stride, type, size for VAO's attribute */
	if ((psVAOAPState->ui32UserStride	!= (IMG_UINT32)stride) ||
		(psVAOAPState->ui32StreamTypeSize	!= ui32StreamTypeSize))
	{
		psVAOAPState->ui32UserStride		= (IMG_UINT32)stride;
		psVAOAPState->ui32StreamTypeSize	= ui32StreamTypeSize;

		psVAO->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ATTRIB_STREAM;
	}

	if (psVAOAPState->pui8Pointer	!= (const IMG_UINT8 *) pointer)
	{
		psVAOAPState->pui8Pointer	= (const IMG_UINT8 *) pointer;

		psVAO->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ATTRIB_POINTER;
	}

	/* Setup bufobj names array */
	GLES1_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_BUFOBJ]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_BUFOBJ];


	/* Setup the bound VBO for VAO's attribute */
	psCurrentBufObj = psVAOAPState->psBufObj;

	psNewBufObj = gc->sBufferObject.psActiveBuffer[ARRAY_BUFFER_INDEX];

	if(psCurrentBufObj != psNewBufObj)
	{
	    /* Decrease bufobj's RefCount by 1 when unbinding it from the VAO */
	    if (psCurrentBufObj && (psCurrentBufObj->sNamedItem.ui32Name != 0))
		{
			NamedItemDelRef(gc, psNamesArray, (GLES1NamedItem*)psCurrentBufObj);
		}	

		/* Add bufobj's RefCount by 1 when binding it to the VAO */
		if (psNewBufObj && (psNewBufObj->sNamedItem.ui32Name != 0))
		{
			NamedItemAddRef(psNamesArray, psNewBufObj->sNamedItem.ui32Name);
		}

		psVAOAPState->psBufObj = psNewBufObj;

		psVAO->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ATTRIB_STREAM;
	}

	GLES1_TIME_STOP(GLES1_TIMES_glWeightPointerOES);

}

#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */

/***********************************************************************************
 Function Name      : InitVertexArrayState
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Initialises internal vertex array state to default values
************************************************************************************/
IMG_INTERNAL IMG_VOID InitVertexArrayState(GLES1Context *gc)
{
	IMG_UINT32 i;

	gc->sVAOMachine.asAttribPointer[AP_VERTEX].psState                     = &(gc->sVAOMachine.sDefaultVAO.asVAOState[AP_VERTEX]);
	gc->sVAOMachine.asAttribPointer[AP_VERTEX].psState->ui32StreamTypeSize = GLES1_STREAMTYPE_FLOAT | (4 << GLES1_STREAMSIZE_SHIFT); 
	gc->sVAOMachine.asAttribPointer[AP_VERTEX].ui32CopyStreamTypeSize      = GLES1_STREAMTYPE_FLOAT | (4 << GLES1_STREAMSIZE_SHIFT); 
	gc->sVAOMachine.asAttribPointer[AP_VERTEX].ui32Stride = 16;
	gc->sVAOMachine.asAttribPointer[AP_VERTEX].ui32Size   = 16;

	gc->sVAOMachine.asAttribPointer[AP_NORMAL].psState                     = &(gc->sVAOMachine.sDefaultVAO.asVAOState[AP_NORMAL]);
	gc->sVAOMachine.asAttribPointer[AP_NORMAL].psState->ui32StreamTypeSize = GLES1_STREAMTYPE_FLOAT | (3 << GLES1_STREAMSIZE_SHIFT); 
	gc->sVAOMachine.asAttribPointer[AP_NORMAL].ui32CopyStreamTypeSize      = GLES1_STREAMTYPE_FLOAT | (3 << GLES1_STREAMSIZE_SHIFT); 
	gc->sVAOMachine.asAttribPointer[AP_NORMAL].ui32Stride = 12;
	gc->sVAOMachine.asAttribPointer[AP_NORMAL].ui32Size   = 12;

	gc->sVAOMachine.asAttribPointer[AP_COLOR].psState                     = &(gc->sVAOMachine.sDefaultVAO.asVAOState[AP_COLOR]);
	gc->sVAOMachine.asAttribPointer[AP_COLOR].psState->ui32StreamTypeSize = GLES1_STREAMTYPE_FLOAT | (4 << GLES1_STREAMSIZE_SHIFT);  
	gc->sVAOMachine.asAttribPointer[AP_COLOR].ui32CopyStreamTypeSize      = GLES1_STREAMTYPE_FLOAT | (4 << GLES1_STREAMSIZE_SHIFT);  
	gc->sVAOMachine.asAttribPointer[AP_COLOR].ui32Stride = 16; 
	gc->sVAOMachine.asAttribPointer[AP_COLOR].ui32Size   = 16;

	for (i=0; i<GLES1_MAX_TEXTURE_UNITS; i++) 
	{
	    gc->sVAOMachine.asAttribPointer[AP_TEXCOORD0 + i].psState                     = &(gc->sVAOMachine.sDefaultVAO.asVAOState[AP_TEXCOORD0 + i]);
	    gc->sVAOMachine.asAttribPointer[AP_TEXCOORD0 + i].psState->ui32StreamTypeSize = GLES1_STREAMTYPE_FLOAT | (4 << GLES1_STREAMSIZE_SHIFT); 
	    gc->sVAOMachine.asAttribPointer[AP_TEXCOORD0 + i].ui32CopyStreamTypeSize      = GLES1_STREAMTYPE_FLOAT | (4 << GLES1_STREAMSIZE_SHIFT); 
		gc->sVAOMachine.asAttribPointer[AP_TEXCOORD0 + i].ui32Stride = 16;
		gc->sVAOMachine.asAttribPointer[AP_TEXCOORD0 + i].ui32Size   = 16;
	}

    gc->sVAOMachine.asAttribPointer[AP_POINTSIZE].psState                     = &(gc->sVAOMachine.sDefaultVAO.asVAOState[AP_POINTSIZE]);
	gc->sVAOMachine.asAttribPointer[AP_POINTSIZE].psState->ui32StreamTypeSize = GLES1_STREAMTYPE_FLOAT | (1 << GLES1_STREAMSIZE_SHIFT);
	gc->sVAOMachine.asAttribPointer[AP_POINTSIZE].ui32CopyStreamTypeSize      = GLES1_STREAMTYPE_FLOAT | (1 << GLES1_STREAMSIZE_SHIFT);
	gc->sVAOMachine.asAttribPointer[AP_POINTSIZE].ui32Stride = 4;
	gc->sVAOMachine.asAttribPointer[AP_POINTSIZE].ui32Size   = 4;


#if defined(GLES1_EXTENSION_MATRIX_PALETTE)

	gc->sVAOMachine.asAttribPointer[AP_WEIGHTARRAY].psState                     = &(gc->sVAOMachine.sDefaultVAO.asVAOState[AP_WEIGHTARRAY]);
	gc->sVAOMachine.asAttribPointer[AP_WEIGHTARRAY].psState->ui32StreamTypeSize = GLES1_STREAMTYPE_FLOAT | (0 << GLES1_STREAMSIZE_SHIFT);
	gc->sVAOMachine.asAttribPointer[AP_WEIGHTARRAY].ui32CopyStreamTypeSize      = GLES1_STREAMTYPE_FLOAT | (0 << GLES1_STREAMSIZE_SHIFT);
	gc->sVAOMachine.asAttribPointer[AP_WEIGHTARRAY].ui32Stride = 0;
	gc->sVAOMachine.asAttribPointer[AP_WEIGHTARRAY].ui32Size   = 0;

	gc->sVAOMachine.asAttribPointer[AP_MATRIXINDEX].psState                     = &(gc->sVAOMachine.sDefaultVAO.asVAOState[AP_MATRIXINDEX]);
	gc->sVAOMachine.asAttribPointer[AP_MATRIXINDEX].psState->ui32StreamTypeSize = GLES1_STREAMTYPE_UBYTE | (0 << GLES1_STREAMSIZE_SHIFT);
	gc->sVAOMachine.asAttribPointer[AP_MATRIXINDEX].ui32CopyStreamTypeSize      = GLES1_STREAMTYPE_UBYTE | (0 << GLES1_STREAMSIZE_SHIFT);
	gc->sVAOMachine.asAttribPointer[AP_MATRIXINDEX].ui32Stride = 0;
	gc->sVAOMachine.asAttribPointer[AP_MATRIXINDEX].ui32Size   = 0;

#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */

}

/******************************************************************************
 End of file (vertex.c)
******************************************************************************/
