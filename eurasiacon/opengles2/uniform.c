/******************************************************************************
 * Name         : uniform.c
 *
 * Copyright    : 2005-2007 by Imagination Technologies Limited.
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
 * $Log: uniform.c $
 * ./
 *  --- Revision Logs Removed --- 
 *
 *****************************************************************************/

#include "context.h"
#include "codeheap.h"

#include <string.h>

/* 
** Convert GLSL type to GL type
*/
static const GLenum asGLSLTypeSpecifierToGLType[GLSLTS_STRUCT + 1] =
{
	0,							/* GLSLTS_INVALID, */
	0,							/* GLSLTS_VOID, */
   	GL_FLOAT,					/* GLSLTS_FLOAT, */
	GL_FLOAT_VEC2,				/* GLSLTS_VEC2, */
	GL_FLOAT_VEC3,				/* GLSLTS_VEC3,*/
	GL_FLOAT_VEC4,				/* GLSLTS_VEC4,*/
	GL_INT,						/* GLSLTS_INT,*/
	GL_INT_VEC2,				/* GLSLTS_IVEC2,*/
	GL_INT_VEC3,				/* GLSLTS_IVEC3,*/
	GL_INT_VEC4,				/* GLSLTS_IVEC4,*/
	GL_BOOL,					/* GLSLTS_BOOL,*/
	GL_BOOL_VEC2,				/* GLSLTS_BVEC2,*/
	GL_BOOL_VEC3,				/* GLSLTS_BVEC3,*/
	GL_BOOL_VEC4,				/* GLSLTS_BVEC4,*/
	GL_FLOAT_MAT2,				/* GLSLTS_MAT2X2,*/
	0,							/* GLSLTS_MAT2X3 */
	0,							/* GLSLTS_MAT2X4 */
	0,							/* GLSLTS_MAT3X2 */
	GL_FLOAT_MAT3,				/* GLSLTS_MAT3X3,*/
	0,							/* GLSLTS_MAT3X4, */
	0,							/* GLSLTS_MAT4X2,*/
	0,							/* GLSLTS_MAT4X3 */
	GL_FLOAT_MAT4,				/* GLSLTS_MAT4X4 */ 
	0,							/* GLSLTS_SAMPLER1D, */
	GL_SAMPLER_2D,				/* GLSLTS_SAMPLER2D,*/
	0,							/* GLSLTS_SAMPLER3D,*/
	GL_SAMPLER_CUBE,			/* GLSLTS_SAMPLERCUBE,*/
	0,							/* GLSLTS_SAMPLER1DSHADOW,*/
	0,							/* GLSLTS_SAMPLER2DSHADOW,*/
	GL_SAMPLER_STREAM_IMG,		/* GLSLTS_SAMPLERSTREAM,*/
	GL_SAMPLER_EXTERNAL_OES,	/* GLSLTS_SAMPLEREXTERNAL,*/
	0,							/* GLSLTS_STRUCT,*/
};

/* 
** Convert GLSL type to GL type
*/
static const GLenum asTypeSpecifierNumComponents[GLSLTS_STRUCT + 1] =
{
	0,			/* GLSLTS_INVALID, */
	0,			/* GLSLTS_VOID, */
   	1,			/* GLSLTS_FLOAT, */
	2,			/* GLSLTS_VEC2, */
	3,			/* GLSLTS_VEC3,*/
	4,			/* GLSLTS_VEC4,*/
	1,			/* GLSLTS_INT,*/
	2,			/* GLSLTS_IVEC2,*/
	3,			/* GLSLTS_IVEC3,*/
	4,			/* GLSLTS_IVEC4,*/
	1,			/* GLSLTS_BOOL,*/
	2,			/* GLSLTS_BVEC2,*/
	3,			/* GLSLTS_BVEC3,*/
	4,			/* GLSLTS_BVEC4,*/
	4,			/* GLSLTS_MAT2X2,*/
	6,			/* GLSLTS_MAT2X3, */
	8,			/* GLSLTS_MAT2X4 */
	6,			/* GLSLTS_MAT3X2 */
	9,			/* GLSLTS_MAT3X3,*/
	12,			/* GLSLTS_MAT3X4, */
	8,			/* GLSLTS_MAT4X2 */
	12,			/* GLSLTS_MAT4X3 */
	16,			/* GLSLTS_MAT4X4,*/
	1,			/* GLSLTS_SAMPLER1D, */
	1,			/* GLSLTS_SAMPLER2D,*/
	1,			/* GLSLTS_SAMPLER3D,*/
	1,			/* GLSLTS_SAMPLERCUBE,*/
	1,			/* GLSLTS_SAMPLER1DSHADOW,*/
	1,			/* GLSLTS_SAMPLER2DSHADOW,*/
	1,			/* GLSLTS_SAMPLERSTREAM,*/
	1,			/* GLSLTS_SAMPLEREXTERNAL,*/
	0,			/* GLSLTS_STRUCT,*/
};



IMG_INTERNAL GLenum ConvertGLSLtoGLType(GLSLTypeSpecifier eGLSLType)
{
	return asGLSLTypeSpecifierToGLType[eGLSLType];
}

#define GLSLTYPE_TO_GLTYPE(typespecifier) asGLSLTypeSpecifierToGLType[typespecifier];

#define COPY_FLOAT(pfConstant, fSrc, sReginfo)						\
{																	\
	IMG_UINT32 s;													\
	for(s = 0; s < sReginfo.uCompAllocCount; s++)					\
	{																\
		if(sReginfo.ui32CompUseMask & (1U << s))					\
		{															\
			(*pfConstant) = fSrc;									\
			break;													\
		}															\
		pfConstant++;												\
	}																\
}

#define COPY_COORD4(pfDst, sCoord, sReginfo)						\
{																	\
	IMG_UINT8 ui8Component = 0;										\
	IMG_UINT32 s;													\
	for(s = 0; s < sReginfo.uCompAllocCount; s++)					\
	{																\
		if(sReginfo.ui32CompUseMask & (1U << s))					\
		{															\
			switch(ui8Component)									\
			{														\
				case 0:												\
					pfDst[ui8Component] = sCoord.fX;				\
					break;											\
				case 1:												\
					pfDst[ui8Component] = sCoord.fY;				\
					break;											\
				case 2:												\
					pfDst[ui8Component] = sCoord.fZ;				\
					break;											\
				case 3:												\
					pfDst[ui8Component] = sCoord.fW;				\
					break;											\
			}														\
																	\
			ui8Component++;											\
		}															\
		if(ui8Component == 4)										\
			break;													\
	}																\
																	\
	GLES_ASSERT(ui8Component == 4);									\
}


/***********************************************************************************
 Function Name      : GetConstantDataPtr
 Inputs             : psSymbolList, psSymbol, psUniform, ui32Location,
 Outputs            : data
 Returns            : None
 Description        :                
************************************************************************************/
static IMG_FLOAT *GetConstantDataPtr(IMG_FLOAT				*pfConstantData,
									 GLSLBindingSymbol		*psSymbol,
									 GLES2Uniform			*psUniform,
									 IMG_INT32				i32Location)
{
	IMG_FLOAT *pfData;

	if(psSymbol->sRegisterInfo.eRegType != HWREG_FLOAT)
	{
		PVR_DPF((PVR_DBG_ERROR, "GetConstantDataPtr: unrecognised reg type \n"));

		return IMG_NULL;
	}

	pfData = pfConstantData;

	if (psUniform)
	{
		pfData += psSymbol->sRegisterInfo.u.uBaseComp;

		pfData += (IMG_UINT32)(i32Location - psUniform->i32Location) * psSymbol->sRegisterInfo.uCompAllocCount;
	}
	
	return pfData;
}



/***********************************************************************************
 Function Name      : SetupBuiltInUniforms
 Inputs             : gc, ui32ProgramType
 Outputs            : 
 Returns            : -
 Description        : Sets up built in or special uniforms
************************************************************************************/
IMG_INTERNAL IMG_VOID SetupBuiltInUniforms(GLES2Context *gc, IMG_UINT32 ui32ProgramType)
{
	IMG_UINT32 i, j;
	GLES2ProgramShader *psShader;
	GLES2Program *psProgram;
	IMG_FLOAT *pfConstant, *pfConstantBase;

	psProgram = gc->sProgram.psCurrentProgram;

	if (ui32ProgramType == GLES2_SHADERTYPE_VERTEX)
	{
		psShader = &psProgram->sVertex;
	}
	else
	{
		psShader = &psProgram->sFragment;
	}

	for (i = 0; i < psProgram->ui32NumBuiltInUniforms; i++) 
	{
		GLES2BuiltInUniform *psBuiltInUniform = &(psProgram->psBuiltInUniforms[i]);

		GLSLBindingSymbol *psSymbol = (ui32ProgramType == GLES2_SHADERTYPE_VERTEX) ?
			psBuiltInUniform->psSymbolVP : psBuiltInUniform->psSymbolFP;

		if(psSymbol)
		{
			pfConstantBase = GetConstantDataPtr(psShader->pfConstantData, psSymbol, IMG_NULL, 0);
			pfConstant = pfConstantBase + psSymbol->sRegisterInfo.u.uBaseComp;

			switch(psSymbol->eBIVariableID)
			{
				case GLSLBV_DEPTHRANGE:
				{
					IMG_FLOAT afValue[3];
					/*
					// Depth range in window coordinates
					//
					struct gl_DepthRangeParameters {
					float near; // n
					float far; // f
					float diff; // f - n
					};
					uniform gl_DepthRangeParameters gl_DepthRange;
					*/
			
					GLES_ASSERT(psSymbol->uNumBaseTypeMembers == 3);

					afValue[0] = gc->sState.sViewport.fZNear;
					afValue[1] = gc->sState.sViewport.fZFar;
					afValue[2] = afValue[1] - afValue[0];

					for(j = 0; j < psSymbol->uNumBaseTypeMembers; j++)
					{
						pfConstant = pfConstantBase + psSymbol->psBaseTypeMembers[j].sRegisterInfo.u.uBaseComp;

						COPY_FLOAT(pfConstant, afValue[j], psSymbol->psBaseTypeMembers[j].sRegisterInfo);
					}

					break;
				}
				case GLSLBV_PMXSWAPFRONTFACE:
				{
					/*
					uniform bool gl_PMXTwoSideEnable; 
					*/
					IMG_BOOL bFrontFaceCW = (gc->sState.sPolygon.eFrontFaceDirection == GL_CW) ? IMG_TRUE : IMG_FALSE;
					IMG_BOOL bYInverted = 	(gc->psDrawParams->eRotationAngle == PVRSRV_FLIP_Y) ? IMG_TRUE : IMG_FALSE;
					IMG_FLOAT fSwap;

					if((bFrontFaceCW && bYInverted) || (!bFrontFaceCW && !bYInverted))
					{
						fSwap = 0.0;
					}
					else
					{
						fSwap = 1.0;
					}

					COPY_FLOAT(pfConstant, fSwap, psSymbol->sRegisterInfo);

					break;
				}
				case GLSLBV_PMXPOSADJUST:
				{
					GLES2coord sPosAdjust = {0};

					if(gc->psDrawParams->eRotationAngle != PVRSRV_FLIP_Y)
					{
						sPosAdjust.fY = (IMG_FLOAT)gc->psDrawParams->ui32Height;
					}
	
					COPY_COORD4(pfConstant, sPosAdjust, psSymbol->sRegisterInfo);

					break;
				}
				case GLSLBV_PMXINVERTDFDY:
				{
					IMG_FLOAT fInvertdFdY = -1.0f;

					if(gc->psDrawParams->eRotationAngle == PVRSRV_FLIP_Y)
					{
						fInvertdFdY = 1.0;
					}
	
					COPY_FLOAT(pfConstant, fInvertdFdY, psSymbol->sRegisterInfo);

					break;
				}
				default:
				{
					PVR_DPF((PVR_DBG_ERROR, "CopyBuiltinUniformDataFromState: not a built-in uniform \n"));

					break;
				}
			}
		}
	}
}


/***********************************************************************************
 Function Name      : WriteUSEShaderMemConsts
 Inputs             : gc, ui32ProgramType
 Outputs            : 
 Returns            : Mem Error
 Description        : Writes constants into memory buffer for USE program
************************************************************************************/
IMG_INTERNAL GLES2_MEMERROR WriteUSEShaderMemConsts(GLES2Context *gc, IMG_UINT32 ui32ProgramType)
{
	IMG_UINT32 ui32ConstantBufferType;
	IMG_FLOAT *pfBuffer; 
	IMG_UINT32 *pui32Buffer;
	IMG_UINT32 i;
	GLES2Program *psProgram;
	GLES2ProgramShader *psShader;
	IMG_UINT32 ui32SizeOfConstantsInDWords;
	USP_HW_SHADER *psPatchedShader;
	IMG_UINT32 (* pui32TexControlWords)[EURASIA_TAG_TEXTURE_STATE_SIZE];
	GLES2_MEMERROR eError;

	psProgram = gc->sProgram.psCurrentProgram;

	if (ui32ProgramType == GLES2_SHADERTYPE_VERTEX)
	{
		psShader = &psProgram->sVertex;
		psPatchedShader = gc->sProgram.psCurrentVertexVariant->psPatchedShader;
		pui32TexControlWords = &gc->sPrim.sFragmentTextureState.aui32TAGControlWord[0];
		
		ui32ConstantBufferType = CBUF_TYPE_PDS_VERT_BUFFER;
		eError = GLES2_TA_BUFFER_ERROR;
	}
	else
	{
		psShader = &psProgram->sFragment;
		psPatchedShader = gc->sProgram.psCurrentFragmentVariant->psPatchedShader;
		pui32TexControlWords = &gc->sPrim.sVertexTextureState.aui32TAGControlWord[0];
		
		ui32ConstantBufferType = CBUF_TYPE_PDS_FRAG_BUFFER;
		eError = GLES2_3D_BUFFER_ERROR;
	}

	ui32SizeOfConstantsInDWords = psPatchedShader->uMemConstCount + (psPatchedShader->uMemTexStateCount * 3);

	/*
		Get buffer space for all the memory constants/texture control words
	*/
	pui32Buffer = CBUF_GetBufferSpace(gc->apsBuffers,
								ui32SizeOfConstantsInDWords,
								ui32ConstantBufferType, IMG_FALSE);

	if(!pui32Buffer)
	{
		return eError;
	}

	pfBuffer = (IMG_FLOAT *)pui32Buffer;

	for (i = 0; i < psPatchedShader->uMemConstCount; i++)
	{
		USP_HW_CONST_LOAD *psConstLoad = &psPatchedShader->psMemConstLoads[i];

		if(psConstLoad->eFormat == USP_HW_CONST_FMT_F32)
		{
			pfBuffer[psConstLoad->uDestIdx] = psShader->pfConstantData[psConstLoad->uSrcIdx];
		}
		else
		{
			IMG_FLOAT fValue = psShader->pfConstantData[psConstLoad->uSrcIdx];

			if(psConstLoad->eFormat == USP_HW_CONST_FMT_F16)
			{
				/* F16 */
				IMG_UINT16 uF16Value = GLES2ConvertFloatToF16(fValue);
			
				uF16Value >>= psConstLoad->uSrcShift;

				pui32Buffer[psConstLoad->uDestIdx] &= CLEARMASK(psConstLoad->uDestShift, (16 - psConstLoad->uSrcShift));

				pui32Buffer[psConstLoad->uDestIdx] |= ((IMG_UINT32)uF16Value << psConstLoad->uDestShift);
			}
			else
			{
				/* C10 */
				IMG_UINT16 uC10Value = GLES2ConvertFloatToC10(fValue);
			
				uC10Value = (uC10Value & 0x3FF) >> psConstLoad->uSrcShift;
				
				pui32Buffer[psConstLoad->uDestIdx] &= CLEARMASK(psConstLoad->uDestShift, (10 - psConstLoad->uSrcShift));

				pui32Buffer[psConstLoad->uDestIdx] |= ((IMG_UINT32)uC10Value << psConstLoad->uDestShift);
			}		
		}
	}

	/* Load texture control words */
	for(i = 0; i < psPatchedShader->uMemTexStateCount; i++)
	{
		USP_HW_TEXSTATE_LOAD *psInMemoryTexMap;
		USP_TEX_CTR_WORDS *psInMemoryTex;
		IMG_UINT32 ui32Offset, ui32Chunk;
		IMG_UINT32 ui32Count;
		
		psInMemoryTexMap = &psPatchedShader->psMemTexStateLoads[i];
		psInMemoryTex = &psPatchedShader->psTextCtrWords[psInMemoryTexMap->uTexCtrWrdIdx];
		
		ui32Offset =  psInMemoryTexMap->uDestIdx;
		ui32Chunk = psInMemoryTex->uChunkIdx + (psInMemoryTex->uTextureIdx * PDS_NUM_TEXTURE_IMAGE_CHUNKS);

		for(ui32Count = 0; ui32Count < EURASIA_TAG_TEXTURE_STATE_SIZE; ui32Count++)
		{
			pui32Buffer[ui32Offset + ui32Count] = 
				(pui32TexControlWords[ui32Chunk][ui32Count] & psInMemoryTex->auMask[ui32Count]) | psInMemoryTex->auWord[ui32Count];
		}
	}
	
	/* Update buffer position */
	CBUF_UpdateBufferPos(gc->apsBuffers, ui32SizeOfConstantsInDWords, ui32ConstantBufferType);

	if(ui32ProgramType == GLES2_SHADERTYPE_VERTEX)
	{
		GLES2_INC_COUNT(GLES2_TIMER_PDS_VERT_DATA_COUNT, ui32SizeOfConstantsInDWords);		
	}
	else
	{
		GLES2_INC_COUNT(GLES2_TIMER_PDS_FRAG_DATA_COUNT, ui32SizeOfConstantsInDWords);
	}

	/*
		Record the device address of the memory constants and their size
	*/
	psShader->uUSEConstsDataBaseAddress  = CBUF_GetBufferDeviceAddress(gc->apsBuffers, pui32Buffer, ui32ConstantBufferType); 
	psShader->ui32USEConstsDataSizeinDWords = ui32SizeOfConstantsInDWords;

	return GLES2_NO_ERROR;
}


/***********************************************************************************
 Function Name      : UpdateConstantRange
 Inputs             : psSymbol, psUniformCopyRange, ui32Start, ui32End
 Outputs            : data
 Returns            : None
 Description        : Updates the relevant constant range for this symbol               
************************************************************************************/
static IMG_VOID UpdateConstantRange(GLSLBindingSymbol *psSymbol, GLES2UniformCopyRange *psUniformCopyRange,
									IMG_UINT32 ui32Start, IMG_UINT32 ui32End)
{
	GLES2CopyRange *psCopyRange;

	switch (psSymbol->sRegisterInfo.eRegType)
	{
		case HWREG_FLOAT:
		{
			psCopyRange = &psUniformCopyRange->sFloatCopyRange;

			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR,"Invalid constant type"));

			return;
		}
	}
	
	if (ui32Start < psCopyRange->ui32Start)
	{
		psCopyRange->ui32Start = ui32Start;
	}

	if (ui32End > psCopyRange->ui32End)
	{
		psCopyRange->ui32End = ui32End;
	}
}


/***********************************************************************************
 Function Name      : SaveUniformDataFloat
 Inputs             : gc, psProgram, psUniform, ui32Location, ui32Numcomponents, ui32Count, pfSrcData
 Outputs            : 
 Returns            : None
 Description        : Save float uniform data in the program, in HW layout                
************************************************************************************/
static IMG_VOID SaveUniformDataFloat(GLES2Context *gc, GLES2Program *psProgram, GLES2Uniform *psUniform, 
									   IMG_INT32 i32Location, IMG_INT32 i32NumComponents,
									   IMG_INT32 i32Count, const GLfloat *pfSrcData)
{
	IMG_FLOAT *pfData, *pfDst;
	const GLfloat *pfSrc;
	IMG_INT32 i;
	IMG_UINT32 j;
	GLSLBindingSymbol *psSymbol;
	IMG_INT32 i32Loadcount;
	IMG_UINT32 ui32Compstart, ui32Compcount;
	IMG_INT32 i32Component;
	IMG_BOOL bIsBool = (IMG_BOOL)(
					   (psUniform->eTypeSpecifier == GLSLTS_BOOL)  || (psUniform->eTypeSpecifier == GLSLTS_BVEC2) ||
	                   (psUniform->eTypeSpecifier == GLSLTS_BVEC3) || (psUniform->eTypeSpecifier == GLSLTS_BVEC4));
	
	/* If uniform is active in vertex program */
	if(psUniform->psSymbolVP)
	{
		psSymbol = psUniform->psSymbolVP;

		/* Modify count if trying to load inactive elements */
		i32Loadcount = i32Count;

		if((i32Location + i32Loadcount) > (psUniform->i32Location + psSymbol->iActiveArraySize))
		{
			i32Loadcount = (psUniform->i32Location + psSymbol->iActiveArraySize) - i32Location;
		}

		pfData = GetConstantDataPtr(psProgram->sVertex.pfConstantData, psSymbol, psUniform, i32Location);
		
		/* Update the data */
		for(i = 0; i < i32Loadcount; i++)
		{
			pfDst = pfData + (psSymbol->sRegisterInfo.uCompAllocCount * (IMG_UINT32)i);
			pfSrc = pfSrcData + i * i32NumComponents;

			i32Component = 0;

			for(j = 0; j < psSymbol->sRegisterInfo.uCompAllocCount; j++)
			{
				if(psSymbol->sRegisterInfo.ui32CompUseMask & (1U << j))
				{
					/* Bools are handled as floats by the compiler.
					   Transform them to 0.0 or 1.0 here.
					*/
					if(bIsBool)
					{
						*pfDst = (*pfSrc)? 1.0f : 0.0f;
					}
					else
					{
						*pfDst = *pfSrc;
					}

					pfSrc++;

					i32Component++;

					if(i32Component == i32NumComponents)
					{
						break;
					}
				}

				pfDst++;
			}
		}

		ui32Compstart = psSymbol->sRegisterInfo.u.uBaseComp;
		ui32Compcount = psSymbol->sRegisterInfo.uCompAllocCount * (IMG_UINT32)i32Loadcount;

		/* Update start and end points */
		UpdateConstantRange(psSymbol, &psProgram->sVertex.sUniformCopyRange, ui32Compstart, ui32Compstart +  ui32Compcount);

		gc->ui32DirtyState |= GLES2_DIRTYFLAG_VERTPROG_CONSTANTS;
	}

	/* If uniform is active in fragment program */
	if(psUniform->psSymbolFP)
	{
		psSymbol = psUniform->psSymbolFP;

		/* Modify count if trying to load inactive elements */
		i32Loadcount = i32Count;

		if((i32Location + i32Loadcount) > (psUniform->i32Location + psSymbol->iActiveArraySize))
		{
			i32Loadcount = (psUniform->i32Location + psSymbol->iActiveArraySize) - i32Location;
		}

		pfData = GetConstantDataPtr(psProgram->sFragment.pfConstantData, psSymbol, psUniform, i32Location);

		/* Update the data */
		for(i = 0; i < i32Loadcount; i++)
		{
			pfDst = pfData + (psSymbol->sRegisterInfo.uCompAllocCount * (IMG_UINT32)i);
			pfSrc = pfSrcData + i * i32NumComponents;

			i32Component = 0;

			for(j = 0; j < psSymbol->sRegisterInfo.uCompAllocCount; j++)
			{
				if(psSymbol->sRegisterInfo.ui32CompUseMask & (1U << j))
				{
					/* Bools are handled as floats by the compiler.
					   Transform them to 0.0 or 1.0 here.
					*/
					if(bIsBool)
					{
						*pfDst = (*pfSrc)? 1.0f : 0.0f;
					}
					else
					{
						*pfDst = *pfSrc;
					}

					pfSrc++;
					i32Component++;

					if(i32Component == i32NumComponents)
					{
						break;
					}
				}

				pfDst++;
			}
		}

		ui32Compstart = psSymbol->sRegisterInfo.u.uBaseComp;
		ui32Compcount = psSymbol->sRegisterInfo.uCompAllocCount * (IMG_UINT32)i32Loadcount;

		/* Update start and end points */
		UpdateConstantRange(psSymbol, &psProgram->sFragment.sUniformCopyRange, ui32Compstart, ui32Compstart +  ui32Compcount);

		gc->ui32DirtyState |= GLES2_DIRTYFLAG_FRAGPROG_CONSTANTS;
	}
}


/***********************************************************************************
 Function Name      : SaveUniformDataInteger
 Inputs             : gc, psProgram, psUniform, ui32Location, ui32Numcomponents, ui32Count, pui32SrcData
 Outputs            : 
 Returns            : None
 Description        : Save integer uniform data in the program, in HW layout                
************************************************************************************/
static IMG_VOID SaveUniformDataInteger(GLES2Context *gc, GLES2Program *psProgram, GLES2Uniform *psUniform, 
									   IMG_INT32 i32Location, IMG_INT32 i32NumComponents,
									   IMG_INT32 i32Count, const GLint *pi32SrcData)
{
	IMG_FLOAT *pfData, *pfDst;
	const GLint *pi32Src;
	IMG_INT32 i;
	IMG_UINT32 j;
	GLSLBindingSymbol *psSymbol;
	IMG_INT32 i32Loadcount;
	IMG_UINT32 ui32Compstart, ui32Compcount;
	IMG_INT32 i32Component;
	IMG_BOOL bIsBool = (IMG_BOOL)(
					   (psUniform->eTypeSpecifier == GLSLTS_BOOL)  || (psUniform->eTypeSpecifier == GLSLTS_BVEC2) ||
	                   (psUniform->eTypeSpecifier == GLSLTS_BVEC3) || (psUniform->eTypeSpecifier == GLSLTS_BVEC4));

	/* If uniform is active in vertex program */
	if(psUniform->psSymbolVP)
	{
		psSymbol = psUniform->psSymbolVP;

		/* Modify count if trying to load inactive elements */
		i32Loadcount = i32Count;

		if((i32Location + i32Loadcount) > (psUniform->i32Location + psSymbol->iActiveArraySize))
		{
			i32Loadcount = (psUniform->i32Location + psSymbol->iActiveArraySize) - i32Location;
		}

		if(GLES2_IS_SAMPLER(psSymbol->eTypeSpecifier))
		{
			/* Get a pointer to the start of the texture sampler info */
			GLES2TextureSampler *psTextureSampler = &psProgram->sVertex.asTextureSamplers[psUniform->ui32VPSamplerIndex];
			IMG_BOOL bDirty = IMG_FALSE;
			
			/* Save the unit this sampler is being bound to */
			for(i = 0; i < i32Loadcount; i++)
			{
				if(psTextureSampler[i32Location - psUniform->i32Location + i].ui8ImageUnit != (IMG_UINT8)pi32SrcData[i])
				{
					psTextureSampler[i32Location - psUniform->i32Location + i].ui8ImageUnit = (IMG_UINT8)pi32SrcData[i];
					bDirty = IMG_TRUE;
				}
			}

			if(bDirty)
			{
				gc->ui32DirtyState |= GLES2_DIRTYFLAG_TEXTURE_STATE;
			}
		}
		else
		{
			pfData = GetConstantDataPtr(psProgram->sVertex.pfConstantData, psSymbol, psUniform, i32Location);

			/* Update the data */
			for(i = 0; i < i32Loadcount; i++)
			{
				pfDst = pfData + (psSymbol->sRegisterInfo.uCompAllocCount * (IMG_UINT32)i);
				pi32Src = pi32SrcData + i * i32NumComponents;

				i32Component = 0;

				for(j = 0; j < psSymbol->sRegisterInfo.uCompAllocCount; j++)
				{
					if(psSymbol->sRegisterInfo.ui32CompUseMask & (1U << j))
					{
						/* Bools are handled as floats by the compiler.
						   Transform them to 0.0 or 1.0 here.
						*/
						if(bIsBool)
						{
							*pfDst = (*pi32Src)? 1.0f : 0.0f;
						}
						else
						{
							*pfDst = (GLfloat)(*pi32Src);
						}

						pi32Src++;
						i32Component++;

						if(i32Component == i32NumComponents)
						{
							break;
						}
					}

					pfDst++;
				}
			}

			ui32Compstart = psSymbol->sRegisterInfo.u.uBaseComp;
			ui32Compcount = psSymbol->sRegisterInfo.uCompAllocCount * (IMG_UINT32)i32Loadcount;

			/* Update start and end points */
			UpdateConstantRange(psSymbol, &psProgram->sVertex.sUniformCopyRange,
								ui32Compstart, ui32Compstart +  ui32Compcount);
		}

		gc->ui32DirtyState |= GLES2_DIRTYFLAG_VERTPROG_CONSTANTS;
	}

	/* If uniform is active in fragment program */
	if(psUniform->psSymbolFP)
	{
		psSymbol = psUniform->psSymbolFP;

		/* Modify count if trying to load inactive elements */
		i32Loadcount = i32Count;

		if((i32Location + i32Loadcount) > (psUniform->i32Location + psSymbol->iActiveArraySize))
		{
			i32Loadcount = (psUniform->i32Location + psSymbol->iActiveArraySize) - i32Location;
		}

		if(GLES2_IS_SAMPLER(psSymbol->eTypeSpecifier))
		{
			/* Get a pointer to the start of the texture sampler info */
			GLES2TextureSampler *psTextureSampler = 
				&(psProgram->sFragment.asTextureSamplers[psUniform->ui32FPSamplerIndex]);			
			IMG_BOOL bDirty = IMG_FALSE;
			
			/* Save the unit this sampler is being bound to */
			for(i = 0; i < i32Loadcount; i++)
			{
				if(psTextureSampler[i32Location - psUniform->i32Location + i].ui8ImageUnit != (IMG_UINT8)pi32SrcData[i])
				{
					psTextureSampler[i32Location - psUniform->i32Location + i].ui8ImageUnit = (IMG_UINT8)pi32SrcData[i];
					bDirty = IMG_TRUE;
				}
			}

			if(bDirty)
			{
				gc->ui32DirtyState |= GLES2_DIRTYFLAG_TEXTURE_STATE;
			}
		}
		else
		{
			pfData = GetConstantDataPtr(psProgram->sFragment.pfConstantData, psSymbol, psUniform, i32Location);

			/* Update the data */
			for(i = 0; i < i32Loadcount; i++)
			{
				pfDst = pfData + (psSymbol->sRegisterInfo.uCompAllocCount * (IMG_UINT32)i);
				pi32Src = pi32SrcData + i * i32NumComponents;

				i32Component = 0;

				for(j = 0; j < psSymbol->sRegisterInfo.uCompAllocCount; j++)
				{
					if(psSymbol->sRegisterInfo.ui32CompUseMask & (1U << j))
					{
						/* Bools are handled as floats by the compiler.
						   Transform them to 0.0 or 1.0 here.
						*/
						if(bIsBool)
						{
							*pfDst = (*pi32Src)? 1.0f : 0.0f;
						}
						else
						{
							*pfDst = (GLfloat)(*pi32Src);
						}

						pi32Src++;
						i32Component++;

						if(i32Component == i32NumComponents)
						{
							break;
						}

					}
					pfDst++;
				}
			}

			ui32Compstart = psSymbol->sRegisterInfo.u.uBaseComp;
			ui32Compcount = psSymbol->sRegisterInfo.uCompAllocCount * (IMG_UINT32)i32Loadcount;

			/* Update start and end points */
			UpdateConstantRange(psSymbol, &psProgram->sFragment.sUniformCopyRange, 
				ui32Compstart, ui32Compstart +  ui32Compcount);

			gc->ui32DirtyState |= GLES2_DIRTYFLAG_FRAGPROG_CONSTANTS;
		}
	}
}


/***********************************************************************************
 Function Name      : GetUniformData
 Inputs             : gc, psProgram, psUniform, ui32Location
 Outputs            : numFloats, dstData
 Returns            : None
 Description        : Get the uniform data from HW layout to user defined layout.               
************************************************************************************/
IMG_INTERNAL IMG_VOID GetUniformData(GLES2Context *gc, GLES2Program *psProgram, GLES2Uniform *psUniform, 
									 IMG_INT32 i32Location, IMG_UINT32 *pui32NumFloats, IMG_FLOAT *pfDstData)
{
	GLSLBindingSymbol *psSymbol;
	IMG_FLOAT *pfData;
	IMG_BOOL bFragment = IMG_FALSE;
	IMG_UINT32 i;
	GLES2ProgramShader *psShader;

	PVR_UNREFERENCED_PARAMETER(gc);

	if(psUniform->psSymbolFP) 
	{
		psSymbol = psUniform->psSymbolFP;
		psShader = &psProgram->sFragment;
		bFragment = IMG_TRUE;
	}
	else
	{
		GLES_ASSERT(psUniform->psSymbolVP);

		psSymbol = psUniform->psSymbolVP;
		psShader = &psProgram->sVertex;
	}

	GLES_ASSERT(psSymbol);

	if(GLES2_IS_SAMPLER(psSymbol->eTypeSpecifier))
	{
		IMG_UINT32 ui32SamplerIndex;

		*pui32NumFloats = 1; 

		if(bFragment)
		{
			ui32SamplerIndex = psUniform->ui32FPSamplerIndex + (IMG_UINT32)(i32Location - psUniform->i32Location);

			pfDstData[0] = (GLfloat) psProgram->sFragment.asTextureSamplers[ui32SamplerIndex].ui8ImageUnit;
		}
		else
		{
			ui32SamplerIndex = psUniform->ui32VPSamplerIndex + (IMG_UINT32)(i32Location - psUniform->i32Location);

			pfDstData[0] = (GLfloat) psProgram->sVertex.asTextureSamplers[ui32SamplerIndex].ui8ImageUnit;
		}
	}
	else
	{
		IMG_UINT32 ui32NumComponents = 0;

		pfData = GetConstantDataPtr(psShader->pfConstantData, psSymbol, psUniform, i32Location);

		*pui32NumFloats = asTypeSpecifierNumComponents[psUniform->eTypeSpecifier];

		for(i = 0; i < psSymbol->sRegisterInfo.uCompAllocCount; i++)
		{
			if(psSymbol->sRegisterInfo.ui32CompUseMask & (1U << i))
			{
				pfDstData[ui32NumComponents] = pfData[i];
				ui32NumComponents++;

				if(ui32NumComponents == *pui32NumFloats) 
					break;
			}
		}

		GLES_ASSERT(ui32NumComponents == *pui32NumFloats);
	}	
}


/***********************************************************************************
 Function Name      : FindUniformFromLocation
 Inputs             : gc, psProgram, i32Location
 Outputs            : 
 Returns            : Uniform
 Description        : Find uniform from its location
************************************************************************************/
IMG_INTERNAL GLES2Uniform *FindUniformFromLocation(GLES2Context *gc, GLES2Program *psProgram, IMG_INT32 i32Location)
{
	IMG_UINT32 i;
	GLES2Uniform *psUniform;

	PVR_UNREFERENCED_PARAMETER(gc);

	/* search through the uniform array and find the one with specific location */
	for(i = 0; i < psProgram->ui32NumActiveUniforms; i++)
	{
		psUniform = &psProgram->psActiveUniforms[i];

		if(psUniform->i32Location == -1)
		{
			continue;
		}

		if(i32Location >= psUniform->i32Location && 
			i32Location < (psUniform->i32Location + (IMG_INT32)psUniform->ui32ActiveArraySize ) )
		{
			return psUniform;
		}
	}

	return IMG_NULL;
}


/***********************************************************************************
 Function Name      : glGetActiveUniform
 Inputs             : program, index, bufsize, 
 Outputs            : length, size, type, name
 Returns            : None
 Description        : ENTRTPOINT: Gets a list of size/types of active uniforms in a program

************************************************************************************/

GL_APICALL void GL_APIENTRY glGetActiveUniform(GLuint program, GLuint index, GLsizei bufsize, 
										   GLsizei *length, GLint *size, GLenum *type, char *name)
{
	GLES2Program *psProgram;
	GLES2Uniform *psUniform;

	char *uniformName;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetActiveUniform"));

	GLES2_TIME_START(GLES2_TIMES_glGetActiveUniform);

	psProgram = GetNamedProgram(gc, program);

	if(psProgram == IMG_NULL)
	{
		goto StopTimeAndExit;
	}

	if((!psProgram->bAttemptedLink) || (index >= psProgram->ui32NumActiveUserUniforms) || (bufsize < 0))
	{
		SetError(gc, GL_INVALID_VALUE);
		goto StopTimeAndExit;
	}

	/* If these assertions fail the app is broken */
	GLES_ASSERT(size != IMG_NULL);
	GLES_ASSERT(type != IMG_NULL);

	psUniform = psProgram->ppsActiveUserUniforms[index];

	if(bufsize > 0)
	{
		/* From the ES spec:
		 * If the active uniform is an array, the uniform name returned in name will always
		 * be the name of the uniform array appended with "[0]". 
		 */
	    IMG_CHAR* pszUniformName[2];
		IMG_UINT32 i;
		
		pszUniformName[0] = psUniform->pszName;
		pszUniformName[1] = "[0]";	

		uniformName = name;

		for(i = 0; i < (IMG_UINT32)(psUniform->ui32DeclaredArraySize ? 2 : 1); i++)
		{
			GLsizei len = (GLsizei)strlen(pszUniformName[i]);
			if(bufsize <= len)
			{
				if(bufsize)
				{
					GLES2MemCopy(name, pszUniformName[i], (IMG_UINT32)(bufsize-1));
					name[bufsize-1] = 0;
				}

				break;
			}
			else
			{
				strcpy(name, pszUniformName[i]);

				name += len;
				bufsize -= len;
			}
		}

		if(length)
		{
			*length = (GLsizei)strlen(uniformName);
			
		}
	}
	else
	{
		if(length)
		{
			*length = 0;
		}
	}

	*size = (GLint)psUniform->ui32ActiveArraySize;

	*type = GLSLTYPE_TO_GLTYPE(psUniform->eTypeSpecifier);

StopTimeAndExit:
	GLES2_TIME_STOP(GLES2_TIMES_glGetActiveUniform);
}


/***********************************************************************************
 Function Name      : glGetUniformLocation
 Inputs             : program, name
 Outputs            : -
 Returns            : Uniform Location
 Description        : Gets the location of a named uniform in a program

************************************************************************************/
GL_APICALL int  GL_APIENTRY glGetUniformLocation(GLuint program, const char *name)
{
	GLES2Uniform *psUniform;
	GLES2Program *psProgram;
	IMG_BOOL bArrayElement = IMG_FALSE;
	IMG_UINT32 i, ui32Length;
	IMG_INT32 i32Index = 0;
	IMG_UINT32 ui32LeftBracket = 0;

	__GLES2_GET_CONTEXT_RETURN(-1);

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetUniformLocation"));

	GLES2_TIME_START(GLES2_TIMES_glGetUniformLocation);

	psProgram = GetNamedProgram(gc, program);

	if(psProgram == IMG_NULL)
	{
		goto StopTimeAndReturnMinusOne;
	}

	if(!psProgram->bSuccessfulLink)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndReturnMinusOne;
	}

	/* gl prefix reserved */
	if(strlen(name) >= 3)
	{
		if ((name[0] == 'g') &&
			(name[1] == 'l') &&
			(name[2] == '_'))
		{
			goto StopTimeAndReturnMinusOne;
		}
	}

	ui32Length = strlen(name);

	if(name[ui32Length-1] == ']')
	{
		ui32LeftBracket = ui32Length-3;

		while(name[ui32LeftBracket] != '[' && (ui32LeftBracket > 0))
		{
			ui32LeftBracket--;
		}

		bArrayElement = IMG_TRUE;

		i32Index = atoi(name + ui32LeftBracket + 1U);
	}
	
	for(i = 0; i < psProgram->ui32NumActiveUserUniforms; i++)
	{
		psUniform = psProgram->ppsActiveUserUniforms[i];

		if(bArrayElement)
		{
			if(!memcmp(name, psUniform->pszName, ui32LeftBracket))
			{
				if((IMG_UINT32)i32Index <= psUniform->ui32ActiveArraySize)
				{
					GLES2_TIME_STOP(GLES2_TIMES_glGetUniformLocation);
					return psUniform->i32Location + i32Index;
				}
				else
				{
					goto StopTimeAndReturnMinusOne;
				}
			}
		}
		else
		{
			if(!strcmp(name, psUniform->pszName))
			{
				GLES2_TIME_STOP(GLES2_TIMES_glGetUniformLocation);
				return psUniform->i32Location;
			}
		}
	}

StopTimeAndReturnMinusOne:

	/* Couldn't find name */
	GLES2_TIME_STOP(GLES2_TIMES_glGetUniformLocation);

	return -1;
}


/***********************************************************************************
 Function Name      : glUniform1i
 Inputs             : location, x
 Outputs            : -
 Returns            : None
 Description        : ENTRTPOINT: Sets an active uniform in a program

************************************************************************************/
GL_APICALL void GL_APIENTRY glUniform1i(GLint location, GLint x)
{
	GLES2Program *psProgram;
	GLES2Uniform *psUniform;
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glUniform1i"));

	GLES2_TIME_START(GLES2_TIMES_glUniform1i);

	/* If location is -1, the command will silently ignore the data passed in */
	if(location == -1) 
	{
		goto StopTimeAndExit;
	}

	/* Get the current program */
	psProgram = gc->sProgram.psCurrentProgram;

	if(!psProgram)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	/* Get the uniform to be loaded */
	psUniform = FindUniformFromLocation(gc, psProgram, location);

	if(psUniform == NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	/* This function can be used for loading sampler uniforms as well */
	if(psUniform->eTypeSpecifier != GLSLTS_INT && 
		psUniform->eTypeSpecifier != GLSLTS_BOOL && 
		!GLES2_IS_SAMPLER(psUniform->eTypeSpecifier) )
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	/* Is it setting up a sampler out of range? */
	if(GLES2_IS_SAMPLER(psUniform->eTypeSpecifier))
	{
		if(x < 0 || x >= GLES2_MAX_TEXTURE_UNITS)
		{
			SetError(gc, GL_INVALID_VALUE);
			goto StopTimeAndExit;
		}
	}

	SaveUniformDataInteger(gc, psProgram, psUniform, location, 1, 1, &x);

StopTimeAndExit:
	GLES2_TIME_STOP(GLES2_TIMES_glUniform1i);
}


/***********************************************************************************
 Function Name      : glUniform2i
 Inputs             : location, x, y
 Outputs            : -
 Returns            : None
 Description        : ENTRTPOINT: Sets an active uniform in a program

************************************************************************************/
GL_APICALL void GL_APIENTRY glUniform2i(GLint location, GLint x, GLint y)
{
	GLES2Program *psProgram;
	GLES2Uniform *psUniform;
	GLint value[2];

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glUniform2i"));

	GLES2_TIME_START(GLES2_TIMES_glUniform2i);

	/* If location is -1, the command will silently ignore the data passed in */
	if(location == -1) 
	{
		goto StopTimeAndExit;
	}

	/* Get the current program */
	psProgram = gc->sProgram.psCurrentProgram;

	if(!psProgram)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	/* Get the uniform to be loaded */
	psUniform = FindUniformFromLocation(gc, psProgram, location);

	if(psUniform == NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	if(psUniform->eTypeSpecifier != GLSLTS_IVEC2 && 
		psUniform->eTypeSpecifier != GLSLTS_BVEC2)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	value[0] = x;
	value[1] = y;

	SaveUniformDataInteger(gc, psProgram, psUniform, location, 2, 1, value);

StopTimeAndExit:
	GLES2_TIME_STOP(GLES2_TIMES_glUniform2i);
}


/***********************************************************************************
 Function Name      : glUniform3i
 Inputs             : location, x, y, z
 Outputs            : -
 Returns            : None
 Description        : ENTRTPOINT: Sets an active uniform in a program

************************************************************************************/
GL_APICALL void GL_APIENTRY glUniform3i(GLint location, GLint x, GLint y, GLint z)
{
	GLES2Program *psProgram;
	GLES2Uniform *psUniform;
	GLint value[3];

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glUniform3i"));

	GLES2_TIME_START(GLES2_TIMES_glUniform3i);

	/* If location is -1, the command will silently ignore the data passed in */
	if(location == -1) 
	{
		goto StopTimeAndExit;
	}

	/* Get the current program */
	psProgram = gc->sProgram.psCurrentProgram;

	if(!psProgram)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	/* Get the uniform to be loaded */
	psUniform = FindUniformFromLocation(gc, psProgram, location);

	if(psUniform == NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	if(psUniform->eTypeSpecifier != GLSLTS_IVEC3 && 
		psUniform->eTypeSpecifier != GLSLTS_BVEC3)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	value[0] = x;
	value[1] = y;
	value[2] = z;

	SaveUniformDataInteger(gc, psProgram, psUniform, location, 3, 1, value);

StopTimeAndExit:
	GLES2_TIME_STOP(GLES2_TIMES_glUniform3i);
}


/***********************************************************************************
 Function Name      : glUniform4i
 Inputs             : location, x, y, z, w
 Outputs            : -
 Returns            : None
 Description        : ENTRTPOINT: Sets an active uniform in a program

************************************************************************************/
GL_APICALL void GL_APIENTRY glUniform4i(GLint location, GLint x, GLint y, GLint z, GLint w)
{
	GLES2Program *psProgram;
	GLES2Uniform *psUniform;
	GLint value[4];

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glUniform4i"));

	GLES2_TIME_START(GLES2_TIMES_glUniform4i);

	/* If location is -1, the command will silently ignore the data passed in */
	if(location == -1) 
	{
		goto StopTimeAndExit;
	}

	/* Get the current program */
	psProgram = gc->sProgram.psCurrentProgram;

	if(!psProgram)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	/* Get the uniform to be loaded */
	psUniform = FindUniformFromLocation(gc, psProgram, location);

	if(psUniform == NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	if(psUniform->eTypeSpecifier != GLSLTS_IVEC4 && 
		psUniform->eTypeSpecifier != GLSLTS_BVEC4)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	value[0] = x;
	value[1] = y;
	value[2] = z;
	value[3] = w;

	SaveUniformDataInteger(gc, psProgram, psUniform, location, 4, 1, value);

StopTimeAndExit:
	GLES2_TIME_STOP(GLES2_TIMES_glUniform4i);
}


/***********************************************************************************
 Function Name      : glUniform1f
 Inputs             : location, x
 Outputs            : -
 Returns            : None
 Description        : ENTRTPOINT: Sets an active uniform in a program
                      
************************************************************************************/
GL_APICALL void GL_APIENTRY glUniform1f(GLint location, GLfloat x)
{
	GLES2Program *psProgram;
	GLES2Uniform *psUniform;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glUniform1f"));

	GLES2_TIME_START(GLES2_TIMES_glUniform1f);

	/* If location is -1, the command will silently ignore the data passed in */
	if(location == -1) 
	{
		goto StopTimeAndExit;
	}

	/* Get the current program */
	psProgram = gc->sProgram.psCurrentProgram;

	if(!psProgram)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	/* Get the uniform to be loaded */
	psUniform = FindUniformFromLocation(gc, psProgram, location);

	if(psUniform == NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	if(psUniform->eTypeSpecifier != GLSLTS_FLOAT && 
		psUniform->eTypeSpecifier != GLSLTS_BOOL)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	SaveUniformDataFloat(gc, psProgram, psUniform, location, 1, 1, &x);

StopTimeAndExit:
	GLES2_TIME_STOP(GLES2_TIMES_glUniform1f);
}


/***********************************************************************************
 Function Name      : glUniform2f
 Inputs             : location, x, y
 Outputs            : -
 Returns            : None
 Description        : ENTRTPOINT: Sets an active uniform in a program

************************************************************************************/
GL_APICALL void GL_APIENTRY glUniform2f(GLint location, GLfloat x, GLfloat y)
{
	GLES2Program *psProgram;
	GLES2Uniform *psUniform;
	GLfloat afValue[2];

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glUniform2f"));

	GLES2_TIME_START(GLES2_TIMES_glUniform2f);

	/* If location is -1, the command will silently ignore the data passed in */
	if(location == -1) 
	{
		goto StopTimeAndExit;
	}

	/* Get the current program */
	psProgram = gc->sProgram.psCurrentProgram;

	if(!psProgram)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	/* Get the uniform to be loaded */
	psUniform = FindUniformFromLocation(gc, psProgram, location);

	if(psUniform == NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	if(psUniform->eTypeSpecifier != GLSLTS_VEC2 && 
		psUniform->eTypeSpecifier != GLSLTS_BVEC2)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	afValue[0] = x;
	afValue[1] = y;

	SaveUniformDataFloat(gc, psProgram, psUniform, location, 2, 1, afValue);

StopTimeAndExit:
	GLES2_TIME_STOP(GLES2_TIMES_glUniform2f);
}


/***********************************************************************************
 Function Name      : glUniform3f
 Inputs             : location, x, y, z
 Outputs            : -
 Returns            : None
 Description        : ENTRTPOINT: Sets an active uniform in a program
                      
************************************************************************************/
GL_APICALL void GL_APIENTRY glUniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z)
{
	GLES2Program *psProgram;
	GLES2Uniform *psUniform;
	GLfloat afValue[3];

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glUniform3f"));

	GLES2_TIME_START(GLES2_TIMES_glUniform3f);

	/* If location is -1, the command will silently ignore the data passed in */
	if(location == -1) 
	{
		goto StopTimeAndExit;
	}

	/* Get the current program */
	psProgram = gc->sProgram.psCurrentProgram;

	if(!psProgram)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	/* Get the uniform to be loaded */
	psUniform = FindUniformFromLocation(gc, psProgram, location);

	if(psUniform == NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	if(psUniform->eTypeSpecifier != GLSLTS_VEC3 && 
		psUniform->eTypeSpecifier != GLSLTS_BVEC3)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	afValue[0] = x;
	afValue[1] = y;
	afValue[2] = z;

	SaveUniformDataFloat(gc, psProgram, psUniform, location, 3, 1, afValue);

StopTimeAndExit:
	GLES2_TIME_STOP(GLES2_TIMES_glUniform3f);
}


/***********************************************************************************
 Function Name      : glUniform4f
 Inputs             : location, x, y, z, w
 Outputs            : -
 Returns            : None
 Description        : ENTRTPOINT: Sets an active uniform in a program
                      
************************************************************************************/
GL_APICALL void GL_APIENTRY glUniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	GLES2Program *psProgram;
	GLES2Uniform *psUniform;
	GLfloat afValue[4];

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glUniform4f"));

	GLES2_TIME_START(GLES2_TIMES_glUniform4f);

	/* If location is -1, the command will silently ignore the data passed in */
	if(location == -1) 
	{
		goto StopTimeAndExit;
	}

	/* Get the current program */
	psProgram = gc->sProgram.psCurrentProgram;

	if(!psProgram)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	/* Get the uniform to be loaded */
	psUniform = FindUniformFromLocation(gc, psProgram, location);

	if(psUniform == NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	if(psUniform->eTypeSpecifier != GLSLTS_VEC4 && 
		psUniform->eTypeSpecifier != GLSLTS_BVEC4)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	afValue[0] = x;
	afValue[1] = y;
	afValue[2] = z;
	afValue[3] = w;

	SaveUniformDataFloat(gc, psProgram, psUniform, location, 4, 1, afValue);

StopTimeAndExit:
	GLES2_TIME_STOP(GLES2_TIMES_glUniform4f);
}


/***********************************************************************************
 Function Name      : glUniform1iv
 Inputs             : location, v
 Outputs            : -
 Returns            : None
 Description        : ENTRTPOINT: Sets an active uniform in a program
                      
************************************************************************************/
GL_APICALL void GL_APIENTRY glUniform1iv(GLint location, GLsizei count, const GLint *v)
{
	GLES2Program *psProgram;
	GLES2Uniform *psUniform;
	IMG_UINT32    i;
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glUniform1iv"));

	GLES2_TIME_START(GLES2_TIMES_glUniform1iv);

	/* If location is -1, the command will silently ignore the data passed in */
	if(location == -1) 
	{
		goto StopTimeAndExit;
	}

	if(count < 0)
	{
		SetError(gc, GL_INVALID_VALUE);
		goto StopTimeAndExit;
	}

	/* Get the current program */
	psProgram = gc->sProgram.psCurrentProgram;

	if(!psProgram)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	/* Get the uniform to be loaded */
	psUniform = FindUniformFromLocation(gc, psProgram, location);

	if(psUniform == NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	/* This function can be used for loading sampler uniforms as well */
	if(psUniform->eTypeSpecifier != GLSLTS_INT && 
		psUniform->eTypeSpecifier != GLSLTS_BOOL && 
		!GLES2_IS_SAMPLER(psUniform->eTypeSpecifier) )
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	/* Is it setting up a sampler out of range? */
	if(GLES2_IS_SAMPLER(psUniform->eTypeSpecifier))
	{
		for(i=0; i < (IMG_UINT32) count; ++i)
		{
			if(v[i] < 0 || v[i] >= GLES2_MAX_TEXTURE_UNITS)
			{
				SetError(gc, GL_INVALID_VALUE);
				goto StopTimeAndExit;
			}
		}
	}

	if((psUniform->ui32DeclaredArraySize == 0) && count > 1)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	SaveUniformDataInteger(gc, psProgram, psUniform, location, 1, count, v);

StopTimeAndExit:
	GLES2_TIME_STOP(GLES2_TIMES_glUniform1iv);
}


/***********************************************************************************
 Function Name      : glUniform2iv
 Inputs             : location, v
 Outputs            : -
 Returns            : None
 Description        : ENTRTPOINT: Sets an active uniform in a program
                      
************************************************************************************/
GL_APICALL void GL_APIENTRY glUniform2iv(GLint location, GLsizei count, const GLint *v)
{
	GLES2Program *psProgram;
	GLES2Uniform *psUniform;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glUniform2iv"));

	GLES2_TIME_START(GLES2_TIMES_glUniform2iv);

	/* If location is -1, the command will silently ignore the data passed in */
	if(location == -1) 
	{
		goto StopTimeAndExit;
	}

	if(count < 0)
	{
		SetError(gc, GL_INVALID_VALUE);
		goto StopTimeAndExit;
	}

	/* Get the current program */
	psProgram = gc->sProgram.psCurrentProgram;

	if(!psProgram)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	/* Get the uniform to be loaded */
	psUniform = FindUniformFromLocation(gc, psProgram, location);

	if(psUniform == NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	if(psUniform->eTypeSpecifier != GLSLTS_IVEC2 && 
		psUniform->eTypeSpecifier != GLSLTS_BVEC2)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}
	
	if((psUniform->ui32DeclaredArraySize == 0) && count > 1)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	SaveUniformDataInteger(gc, psProgram, psUniform, location, 2, count, v);

StopTimeAndExit:
	GLES2_TIME_STOP(GLES2_TIMES_glUniform2iv);
}


/***********************************************************************************
 Function Name      : glUniform3iv
 Inputs             : location, v
 Outputs            : -
 Returns            : None
 Description        : ENTRTPOINT: Sets an active uniform in a program
                      
************************************************************************************/
GL_APICALL void GL_APIENTRY glUniform3iv(GLint location, GLsizei count, const GLint *v)
{
	GLES2Program *psProgram;
	GLES2Uniform *psUniform;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glUniform3iv"));

	GLES2_TIME_START(GLES2_TIMES_glUniform3iv);

	/* If location is -1, the command will silently ignore the data passed in */
	if(location == -1) 
	{
		goto StopTimeAndExit;
	}

	if(count < 0)
	{
		SetError(gc, GL_INVALID_VALUE);
		goto StopTimeAndExit;
	}

	/* Get the current program */
	psProgram = gc->sProgram.psCurrentProgram;

	if(!psProgram)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	/* Get the uniform to be loaded */
	psUniform = FindUniformFromLocation(gc, psProgram, location);

	if(psUniform == NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	if(psUniform->eTypeSpecifier != GLSLTS_IVEC3 && 
		psUniform->eTypeSpecifier != GLSLTS_BVEC3)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}
	
	if((psUniform->ui32DeclaredArraySize == 0) && count > 1)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	SaveUniformDataInteger(gc, psProgram, psUniform, location, 3, count, v);

StopTimeAndExit:
	GLES2_TIME_STOP(GLES2_TIMES_glUniform3iv);
}


/***********************************************************************************
 Function Name      : glUniform4iv
 Inputs             : location, v
 Outputs            : -
 Returns            : None
 Description        : ENTRTPOINT: Sets an active uniform in a program
                      
************************************************************************************/
GL_APICALL void GL_APIENTRY glUniform4iv(GLint location, GLsizei count, const GLint *v)
{
	GLES2Program *psProgram;
	GLES2Uniform *psUniform;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glUniform4iv"));

	GLES2_TIME_START(GLES2_TIMES_glUniform4iv);

	/* If location is -1, the command will silently ignore the data passed in */
	if(location == -1) 
	{
		goto StopTimeAndExit;
	}

	if(count < 0)
	{
		SetError(gc, GL_INVALID_VALUE);
		goto StopTimeAndExit;
	}

	/* Get the current program */
	psProgram = gc->sProgram.psCurrentProgram;

	if(!psProgram)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	/* Get the uniform to be loaded */
	psUniform = FindUniformFromLocation(gc, psProgram, location);

	if(psUniform == NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	if(psUniform->eTypeSpecifier != GLSLTS_IVEC4 && 
		psUniform->eTypeSpecifier != GLSLTS_BVEC4)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}
	
	if((psUniform->ui32DeclaredArraySize == 0) && count > 1)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	SaveUniformDataInteger(gc, psProgram, psUniform, location, 4, count, v);

StopTimeAndExit:
	GLES2_TIME_STOP(GLES2_TIMES_glUniform4iv);
}


/***********************************************************************************
 Function Name      : glUniform1fv
 Inputs             : location, v
 Outputs            : -
 Returns            : None
 Description        : ENTRTPOINT: Sets an active uniform in a program
                      
************************************************************************************/
GL_APICALL void GL_APIENTRY glUniform1fv(GLint location, GLsizei count, const GLfloat *v)
{
	GLES2Program *psProgram;
	GLES2Uniform *psUniform;
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glUniform1fv"));

	GLES2_TIME_START(GLES2_TIMES_glUniform1fv);

	/* If location is -1, the command will silently ignore the data passed in */
	if(location == -1) 
	{
		goto StopTimeAndExit;
	}

	if(count < 0)
	{
		SetError(gc, GL_INVALID_VALUE);
		goto StopTimeAndExit;
	}

	/* Get the current program */
	psProgram = gc->sProgram.psCurrentProgram;

	if(!psProgram)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	/* Get the uniform to be loaded */
	psUniform = FindUniformFromLocation(gc, psProgram, location);

	if(psUniform == NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	/* This function can be used for loading sampler uniforms as well */
	if(psUniform->eTypeSpecifier != GLSLTS_FLOAT && 
		psUniform->eTypeSpecifier != GLSLTS_BOOL)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	if((psUniform->ui32DeclaredArraySize == 0) && count > 1)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	SaveUniformDataFloat(gc, psProgram, psUniform, location, 1, count, v);

StopTimeAndExit:
	GLES2_TIME_STOP(GLES2_TIMES_glUniform1fv);
}


/***********************************************************************************
 Function Name      : glUniform2fv
 Inputs             : location, v
 Outputs            : -
 Returns            : None
 Description        : ENTRTPOINT: Sets an active uniform in a program

************************************************************************************/
GL_APICALL void GL_APIENTRY glUniform2fv(GLint location, GLsizei count, const GLfloat *v)
{
	GLES2Program *psProgram;
	GLES2Uniform *psUniform;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glUniform2fv"));

	GLES2_TIME_START(GLES2_TIMES_glUniform2fv);

	/* If location is -1, the command will silently ignore the data passed in */
	if(location == -1) 
	{
		goto StopTimeAndExit;
	}

	if(count < 0)
	{
		SetError(gc, GL_INVALID_VALUE);
		goto StopTimeAndExit;
	}

	/* Get the current program */
	psProgram = gc->sProgram.psCurrentProgram;

	if(!psProgram)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	/* Get the uniform to be loaded */
	psUniform = FindUniformFromLocation(gc, psProgram, location);

	if(psUniform == NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	if(psUniform->eTypeSpecifier != GLSLTS_VEC2 && 
		psUniform->eTypeSpecifier != GLSLTS_BVEC2)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	if((psUniform->ui32DeclaredArraySize == 0) && count > 1)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	SaveUniformDataFloat(gc, psProgram, psUniform, location, 2, count, v);

StopTimeAndExit:
	GLES2_TIME_STOP(GLES2_TIMES_glUniform2fv);
}


/***********************************************************************************
 Function Name      : glUniform3fv
 Inputs             : location, v
 Outputs            : -
 Returns            : None
 Description        : ENTRTPOINT: Sets an active uniform in a program

************************************************************************************/
GL_APICALL void GL_APIENTRY glUniform3fv(GLint location, GLsizei count, const GLfloat *v)
{
	GLES2Program *psProgram;
	GLES2Uniform *psUniform;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glUniform3fv"));

	GLES2_TIME_START(GLES2_TIMES_glUniform3fv);

	/* If location is -1, the command will silently ignore the data passed in */
	if(location == -1) 
	{
		goto StopTimeAndExit;
	}

	if(count < 0)
	{
		SetError(gc, GL_INVALID_VALUE);
		goto StopTimeAndExit;
	}

	/* Get the current program */
	psProgram = gc->sProgram.psCurrentProgram;

	if(!psProgram)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	/* Get the uniform to be loaded */
	psUniform = FindUniformFromLocation(gc, psProgram, location);

	if(psUniform == NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	if(psUniform->eTypeSpecifier != GLSLTS_VEC3 && 
		psUniform->eTypeSpecifier != GLSLTS_BVEC3)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}
	
	if((psUniform->ui32DeclaredArraySize == 0) && count > 1)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	SaveUniformDataFloat(gc, psProgram, psUniform, location, 3, count, v);

StopTimeAndExit:
	GLES2_TIME_STOP(GLES2_TIMES_glUniform3fv);
}


/***********************************************************************************
 Function Name      : glUniform4fv
 Inputs             : location, v
 Outputs            : -
 Returns            : None
 Description        : ENTRTPOINT: Sets an active uniform in a program

************************************************************************************/
GL_APICALL void GL_APIENTRY glUniform4fv(GLint location, GLsizei count, const GLfloat *v)
{
	GLES2Program *psProgram;
	GLES2Uniform *psUniform;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glUniform4fv"));

	GLES2_TIME_START(GLES2_TIMES_glUniform4fv);

	/* If location is -1, the command will silently ignore the data passed in */
	if(location == -1) 
	{
		goto StopTimeAndExit;
	}

	if(count < 0)
	{
		SetError(gc, GL_INVALID_VALUE);
		goto StopTimeAndExit;
	}

	/* Get the current program */
	psProgram = gc->sProgram.psCurrentProgram;

	if(!psProgram)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	/* Get the uniform to be loaded */
	psUniform = FindUniformFromLocation(gc, psProgram, location);

	if(psUniform == NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	if(psUniform->eTypeSpecifier != GLSLTS_VEC4 && 
		psUniform->eTypeSpecifier != GLSLTS_BVEC4)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	if((psUniform->ui32DeclaredArraySize == 0) && count > 1)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	SaveUniformDataFloat(gc, psProgram, psUniform, location, 4, count, v);

StopTimeAndExit:
	GLES2_TIME_STOP(GLES2_TIMES_glUniform4fv);
}


/***********************************************************************************
 Function Name      : glUniformMatrix2fv
 Inputs             : location, count, transpose, value
 Outputs            : -
 Returns            : None
 Description        : ENTRTPOINT: Sets an active uniform (matrix) in a program

************************************************************************************/
GL_APICALL void GL_APIENTRY glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
	GLES2Program *psProgram;
	GLES2Uniform *psUniform;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glUniformMatrix2fv"));

	GLES2_TIME_START(GLES2_TIMES_glUniformMatrix2fv);
	
	/* If location is -1, the command will silently ignore the data passed in */
	if(location == -1) 
	{
		goto StopTimeAndExit;
	}

	if(count < 0)
	{
		SetError(gc, GL_INVALID_VALUE);
		goto StopTimeAndExit;
	}

	/* Get the current program */
	psProgram = gc->sProgram.psCurrentProgram;

	if(!psProgram)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	/* Get the uniform to be loaded */
	psUniform = FindUniformFromLocation(gc, psProgram, location);

	if(psUniform == NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	if(psUniform->eTypeSpecifier != GLSLTS_MAT2X2)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	if((psUniform->ui32DeclaredArraySize == 0) && count > 1)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	if(transpose)
	{
		SetError(gc, GL_INVALID_VALUE);
	}
	else
	{
		SaveUniformDataFloat(gc, psProgram, psUniform, location, 4, count, value);
	}
	
StopTimeAndExit:
	GLES2_TIME_STOP(GLES2_TIMES_glUniformMatrix2fv);
}


/***********************************************************************************
 Function Name      : glUniformMatrix3fv
 Inputs             : location, count, transpose, value
 Outputs            : -
 Returns            : None
 Description        : ENTRTPOINT: Sets an active uniform (matrix) in a program

************************************************************************************/
GL_APICALL void GL_APIENTRY glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
	GLES2Program *psProgram;
	GLES2Uniform *psUniform;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glUniformMatrix3fv"));

	GLES2_TIME_START(GLES2_TIMES_glUniformMatrix3fv);
	
	/* If location is -1, the command will silently ignore the data passed in */
	if(location == -1) 
	{
		goto StopTimeAndExit;
	}

	if(count < 0)
	{
		SetError(gc, GL_INVALID_VALUE);
		goto StopTimeAndExit;
	}

	/* Get the current program */
	psProgram = gc->sProgram.psCurrentProgram;

	if(!psProgram)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	/* Get the uniform to be loaded */
	psUniform = FindUniformFromLocation(gc, psProgram, location);

	if(psUniform == NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	if(psUniform->eTypeSpecifier != GLSLTS_MAT3X3)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	if((psUniform->ui32DeclaredArraySize == 0) && count > 1)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	if(transpose)
	{
		SetError(gc, GL_INVALID_VALUE);
	}
	else
	{
		SaveUniformDataFloat(gc, psProgram, psUniform, location, 9, count, value);
	}

StopTimeAndExit:
	GLES2_TIME_STOP(GLES2_TIMES_glUniformMatrix3fv);
}


/***********************************************************************************
 Function Name      : glUniformMatrix4fv
 Inputs             : location, count, transpose, value
 Outputs            : -
 Returns            : None
 Description        : ENTRTPOINT: Sets an active uniform (matrix) in a program

************************************************************************************/
GL_APICALL void GL_APIENTRY glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
	GLES2Program *psProgram;
	GLES2Uniform *psUniform;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glUniformMatrix4fv"));

	GLES2_TIME_START(GLES2_TIMES_glUniformMatrix4fv);
	
	/* If location is -1, the command will silently ignore the data passed in */
	if(location == -1) 
	{
		goto StopTimeAndExit;
	}

	if(count < 0)
	{
		SetError(gc, GL_INVALID_VALUE);
		goto StopTimeAndExit;
	}

	/* Get the current program */
	psProgram = gc->sProgram.psCurrentProgram;

	if(!psProgram)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	/* Get the uniform to be loaded */
	psUniform = FindUniformFromLocation(gc, psProgram, location);
	if(psUniform == NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	if(psUniform->eTypeSpecifier != GLSLTS_MAT4X4)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	if((psUniform->ui32DeclaredArraySize == 0) && count > 1)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndExit;
	}

	if(transpose)
	{
		SetError(gc, GL_INVALID_VALUE);
	}
	else
	{
		SaveUniformDataFloat(gc, psProgram, psUniform, location, 16, count, value);
	}
	
StopTimeAndExit:
	GLES2_TIME_STOP(GLES2_TIMES_glUniformMatrix4fv);
}

/******************************************************************************
 End of file (uniform.c)
******************************************************************************/
