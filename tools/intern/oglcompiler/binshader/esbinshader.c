/**************************************************************************
 * Name         : esbinshader.c
 * Created      : 6/07/2006
 *
 * Copyright    : 2000-2005 by Imagination Technologies Limited. All rights reserved.
 *              : No part of this software, either material or conceptual 
 *              : may be copied or distributed, transmitted, transcribed,
 *              : stored in a retrieval system or translated into any 
 *              : human or computer language in any form by any means,
 *              : electronic, mechanical, manual or other-wise, or 
 *              : disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited, Unit 8, HomePark
 *              : Industrial Estate, King's Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * Modifications:-
 * $Log: esbinshader.c $
 */


#include "esbinshader.h"
#include "esbinshaderinternal.h"
#include <string.h>
#include "usp.h"
#include "pvrversion.h"

/* Size of the header of the UniPatch Input */
#define SGXBS_UNIPATCH_HEADER_SIZE 12


/*
** Used to keep track of the status of a buffer in the packing functions.
*/
typedef struct SGXBS_BufferTAG
{
	IMG_UINT8                       *pu8Buffer;
	IMG_UINT32                      u32CurrentPosition;
	IMG_UINT32                      u32BufferSizeInBytes;
	IMG_BOOL                        bOverflow;
	IMG_BOOL                        bInvalidArgument;

} SGXBS_Buffer;

static IMG_VOID GetCoreAndRevisionNumber(IMG_UINT16 * u16Core, IMG_UINT16 * u16Revision)
{
	#if defined(SGX520)
		*u16Core = SGXBS_CORE_520;
	#else
		#if defined(SGX530)
			*u16Core = SGXBS_CORE_530;
		#else
			#if defined(SGX531)
						*u16Core = SGXBS_CORE_531;
			#else
				#if defined(SGX535) || defined(SGX535_V1_1)
					*u16Core = SGXBS_CORE_535;
				#else
					#if defined(SGX540)
						*u16Core = SGXBS_CORE_540;
					#else
						#if defined(SGX541)
							*u16Core = SGXBS_CORE_541;
						#else
							#if defined(SGX543)
								*u16Core = SGXBS_CORE_543;
							#else	
								#if defined(SGX544)
									*u16Core = SGXBS_CORE_544;
								#else	
									#if defined(SGX545)
										*u16Core = SGXBS_CORE_545;
									#else
										#if defined(SGX554)
											*u16Core = SGXBS_CORE_554;
										#else
											#error "Unsupported core version. Update binary specification as needed."
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

	#if !defined(SGX_CORE_REV)
	#error "SGX_CORE_REV must be specified."
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

	/* Allows release builds targeting head revision ! */
	#if (SGX_CORE_REV == SGX_CORE_REV_HEAD)
	#pragma message ("-- Binary shader targetting head revision")
	#define SGXBS_CORE_REVISION SGX_CORE_REV_HEAD
	#endif

	#ifndef SGXBS_CORE_REVISION
	#error "Unsupported core revision. Update binary specification as needed."
	#endif

	*u16Revision = SGXBS_CORE_REVISION;
}

static IMG_UINT32 GetNumOfBytesForWritingArrayHeader(IMG_VOID)
{
	/* An array header is just a 16-bit unsigned integer.  See WriteArrayHeader. */
	return 2;
}

static IMG_UINT32 GetNumOfBytesForWritingBindings(const GLSLBindingSymbol * psBindingSymbols, IMG_UINT32 ui32NumSymbols)
{
	IMG_UINT32 i;
	const GLSLBindingSymbol * psSymbols;
	IMG_UINT32 ui32Count = GetNumOfBytesForWritingArrayHeader(); /* array header */

	psSymbols = psBindingSymbols;

	for(i = 0; i < ui32NumSymbols; i++)
	{
		ui32Count += (strlen(psSymbols->pszName) +1); 
		ui32Count += 2; /* ID */
		ui32Count += 1; /* specifier */
		ui32Count += 1; /* qualifier */
		ui32Count += 1; /* precision */
		ui32Count += 1; /* varying flags */
		ui32Count += 2; /* active array size */
		ui32Count += 2; /* declared array size */
		ui32Count += 1; /* reg type */
		ui32Count += 2; /* union */
		ui32Count += 1; /* alloc count */
		ui32Count += 2; /* CompUseMask */
		
		ui32Count += GetNumOfBytesForWritingBindings(psSymbols->psBaseTypeMembers, psSymbols->uNumBaseTypeMembers);

		psSymbols++;
	}

	return ui32Count;
}

static IMG_UINT32 GetNumOfBytesForWritingUserBindings(const GLSLAttribUserBinding* psUserBinding)
{
	IMG_UINT32 ui32Count = GetNumOfBytesForWritingArrayHeader(); /* array header */
	const GLSLAttribUserBinding* psBinding = psUserBinding;

	while(psBinding)
	{
		ui32Count += (strlen(psBinding->pszName) +1); /* name */
		ui32Count += 4; /*index */

		psBinding = psBinding->psNext;
	}

	return ui32Count;
}

static IMG_VOID WriteString(SGXBS_Buffer *psBuffer, IMG_CHAR *pszString)
{
	IMG_UINT32 ui32Length = strlen(pszString) + 1;

	if(psBuffer->u32CurrentPosition + ui32Length <= psBuffer->u32BufferSizeInBytes)
	{
		memcpy(&psBuffer->pu8Buffer[psBuffer->u32CurrentPosition], pszString, ui32Length);
		psBuffer->u32CurrentPosition += ui32Length;
	}
	else
	{
		psBuffer->bOverflow = IMG_TRUE;
	}
}


static IMG_VOID WriteU8(SGXBS_Buffer *psBuffer, IMG_INT32 i32Value)
{
	if(i32Value < 0x100 && i32Value >= 0)
	{
		/* The value is within range */
		if(psBuffer->u32CurrentPosition + 1 <= psBuffer->u32BufferSizeInBytes)
		{
			/* The buffer won't overflow if the data is written */
			psBuffer->pu8Buffer[psBuffer->u32CurrentPosition] = (IMG_UINT8)i32Value;
			psBuffer->u32CurrentPosition += 1;
		}
		else
		{
			psBuffer->bOverflow = IMG_TRUE;
		}
	}
	else
	{
#ifdef DEBUG
		printf("WriteU8: Value out of range: 0x%X cannot be represented with an U8\n", i32Value);
#endif
		psBuffer->bInvalidArgument = IMG_TRUE;
	}
}


static IMG_VOID WriteU16(SGXBS_Buffer *psBuffer, IMG_UINT32 ui32Value)
{
	if(ui32Value < 0x10000)
	{
		/* The value is within range */
		if(psBuffer->u32CurrentPosition + 2 <= psBuffer->u32BufferSizeInBytes)
		{
			/* The buffer won't overflow if the data is written */
			psBuffer->pu8Buffer[psBuffer->u32CurrentPosition+0] = (IMG_UINT8)((ui32Value >> 8) & 0xFF);
			psBuffer->pu8Buffer[psBuffer->u32CurrentPosition+1] = (IMG_UINT8)((ui32Value     ) & 0xFF);
			psBuffer->u32CurrentPosition += 2;
		}
		else
		{
			psBuffer->bOverflow = IMG_TRUE;
		}
	}
	else
	{
#ifdef DEBUG
		printf("WriteU16: Value out of range: 0x%X cannot be represented with an U16\n", ui32Value);
#endif
		psBuffer->bInvalidArgument = IMG_TRUE;
	}
}


static IMG_VOID WriteU32(SGXBS_Buffer *psBuffer, IMG_UINT32 u32Value)
{
	if(psBuffer->u32CurrentPosition + 4 <= psBuffer->u32BufferSizeInBytes)
	{
		psBuffer->pu8Buffer[psBuffer->u32CurrentPosition+0] = (IMG_UINT8)((u32Value >> 24) & 0xFF);
		psBuffer->pu8Buffer[psBuffer->u32CurrentPosition+1] = (IMG_UINT8)((u32Value >> 16) & 0xFF);
		psBuffer->pu8Buffer[psBuffer->u32CurrentPosition+2] = (IMG_UINT8)((u32Value >>  8) & 0xFF);
		psBuffer->pu8Buffer[psBuffer->u32CurrentPosition+3] = (IMG_UINT8)((u32Value      ) & 0xFF);
		psBuffer->u32CurrentPosition += 4;
	}
	else
	{
		psBuffer->bOverflow = IMG_TRUE;
	}
}


static IMG_VOID WriteFloat(SGXBS_Buffer *psBuffer, IMG_FLOAT fValue)
{
	union { IMG_UINT32 u32; IMG_FLOAT f; } u;

	u.f = fValue;

	if(psBuffer->u32CurrentPosition + 4 <= psBuffer->u32BufferSizeInBytes)
	{
		psBuffer->pu8Buffer[psBuffer->u32CurrentPosition+0] = (IMG_UINT8)((u.u32 >> 24) & 0xFF);
		psBuffer->pu8Buffer[psBuffer->u32CurrentPosition+1] = (IMG_UINT8)((u.u32 >> 16) & 0xFF);
		psBuffer->pu8Buffer[psBuffer->u32CurrentPosition+2] = (IMG_UINT8)((u.u32 >>  8) & 0xFF);
		psBuffer->pu8Buffer[psBuffer->u32CurrentPosition+3] = (IMG_UINT8)((u.u32      ) & 0xFF);
		psBuffer->u32CurrentPosition += 4;
	}
	else
	{
		psBuffer->bOverflow = IMG_TRUE;
	}
}

static IMG_VOID WriteArrayHeader(SGXBS_Buffer *psBuffer, IMG_INT32 i32NumElements)
{
	/* An array header is just a 16-bit unsigned integer */
	WriteU16(psBuffer, i32NumElements);
}

static SGXBS_Error PackSymbolBindings(const GLSLBindingSymbol psSymbols[/*u32NumSymbols*/],
	IMG_UINT32 u32NumSymbols, SGXBS_Buffer *psBuffer)
{
	SGXBS_Error         eError;
	IMG_UINT32          i;

	/*
	 * Check arguments
	 */
	if(!psBuffer)
	{
		return SGXBS_INTERNAL_ERROR;
	}

	if(u32NumSymbols && !psSymbols)
	{
		return SGXBS_INVALID_ARGUMENTS_ERROR;
	}

	/* Write the array header */
	WriteArrayHeader(psBuffer, u32NumSymbols);

	/* Write the array data */
	for(i=0; i < u32NumSymbols; ++i)
	{
		/* 01- Name of the symbol */
		WriteString(psBuffer, psSymbols->pszName);

		/* 02- eBIVariableID */
		WriteU16(psBuffer, psSymbols->eBIVariableID);

		/* 03- eTypeSpecifier */
		WriteU8( psBuffer, psSymbols->eTypeSpecifier);

		/* 04- eTypeQualifier */
		WriteU8( psBuffer, psSymbols->eTypeQualifier);

		/* 05- ePrecisionQualifier */
		WriteU8( psBuffer, psSymbols->ePrecisionQualifier);

		/* 06- eVaryingModifierFlags */
		WriteU8( psBuffer, psSymbols->eVaryingModifierFlags);

		/* 07- u16ActiveArraySize */
		WriteU16(psBuffer, psSymbols->iActiveArraySize);

		/* 08- u16DeclaredArraySize */
		WriteU16(psBuffer, psSymbols->iDeclaredArraySize);

		/* 09- eRegType */
		WriteU8( psBuffer, psSymbols->sRegisterInfo.eRegType);

		/* 10- union { u16BaseComp, u16TextureUnit } */
		WriteU16(psBuffer, psSymbols->sRegisterInfo.u.uBaseComp);

		/* 11- u8CompAllocCount */
		WriteU8( psBuffer, psSymbols->sRegisterInfo.uCompAllocCount);

		/* 12- u32CompUseMask */
		WriteU16(psBuffer, psSymbols->sRegisterInfo.ui32CompUseMask);

		/* 13- sBaseTypeMembersArray */
		if(psSymbols->uNumBaseTypeMembers && psSymbols->eTypeSpecifier != GLSLTS_STRUCT)
		{
			return SGXBS_INVALID_ARGUMENTS_ERROR;
		}

		eError = PackSymbolBindings(psSymbols->psBaseTypeMembers, psSymbols->uNumBaseTypeMembers, psBuffer);

		if(eError != SGXBS_NO_ERROR)
		{
			return eError;
		}

		psSymbols++;
	}

	if(psBuffer->bOverflow)
	{
		return SGXBS_OUT_OF_MEMORY_ERROR;
	}

	if(psBuffer->bInvalidArgument)
	{
		return SGXBS_INVALID_ARGUMENTS_ERROR;
	}

	return SGXBS_NO_ERROR;
}


static SGXBS_Error PackUniflexProgram(const GLSLCompiledUniflexProgram *psShader, SGXBS_Buffer* psBuffer)
{
	SGXBS_Error                     eError;
	const GLSLUniFlexCode          *psCode = psShader->psUniFlexCode;
	const struct _USP_PC_SHADER_  *psUniPatchInput = (struct _USP_PC_SHADER_*)psCode->psUniPatchInput;
	const struct _USP_PC_SHADER_  *psUniPatchInputMSAATrans = (struct _USP_PC_SHADER_*)psCode->psUniPatchInputMSAATrans;
	const GLSLBindingSymbolList    *psSymbolList = psShader->psBindingSymbolList;
	IMG_UINT8                       *pu8Data;
	IMG_UINT32                      i, u32UniPatchSize;

	/*
	 * 1- Pack the shader type (vertex/fragment), flags and 32 reserved bits (for future BRNs, etc)
	 */
	WriteU32(psBuffer, psShader->eProgramType);
	WriteU32(psBuffer, psShader->eProgramFlags);
	WriteU32(psBuffer, 0 /* Reserved for future use */ );

	/*
	 * 2- Pack the varying mask.
	 */
	WriteU32(psBuffer, psCode->eActiveVaryingMask);

	/*
	 * 3- Pack the texture coordinates' dimensions
	 */
	for(i = 0; i < SGXBS_NUM_TC_REGISTERS; ++i)
	{
		/* Ocassionaly, the data won't be initialized and an INVALID_ARGUMENTS_ERROR would be raised
		 * if the extra checking code was not in place here.
		 */
		if(psCode->auTexCoordDims[i] > 0xFF)
		{
			WriteU8(psBuffer, 0);
		}
		else
		{
			WriteU8(psBuffer, psCode->auTexCoordDims[i]);
		}
	}

	/*
	 * 4- Pack the texture coordinates' precisions
	 */
	for(i = 0; i < SGXBS_NUM_TC_REGISTERS; ++i)
	{
		/* Ocassionaly, the data won't be initialized and an INVALID_ARGUMENTS_ERROR would be raised
		 * if the extra checking code was not in place here.
		 */
		if((IMG_UINT32)psCode->aeTexCoordPrecisions[i] > 0xFF)
		{
			WriteU8(psBuffer, 0);
		}
		else
		{
			WriteU8(psBuffer, psCode->aeTexCoordPrecisions[i]);
		}
	}

	/*
	 * 5- Write the standard UniPatch input
	 */
	u32UniPatchSize = SGXBS_UNIPATCH_HEADER_SIZE + psUniPatchInput->uSize;

	WriteU32(psBuffer, u32UniPatchSize);

	pu8Data = (IMG_UINT8*)psUniPatchInput;

	for(i = 0; i < u32UniPatchSize; ++i)
	{
		WriteU8(psBuffer, pu8Data[i]);
	}

	/*
	 * 6- Write the UniPatch input used for configs with multisample antialias with translucency (only fragment shaders)
	 */
	if((GLSLPT_FRAGMENT == psShader->eProgramType) && (psUniPatchInputMSAATrans))
	{
		u32UniPatchSize = SGXBS_UNIPATCH_HEADER_SIZE + psUniPatchInputMSAATrans->uSize;

		WriteU32(psBuffer, u32UniPatchSize);

		pu8Data = (IMG_UINT8*)psUniPatchInputMSAATrans;

		for(i = 0; i < u32UniPatchSize; ++i)
		{
			WriteU8(psBuffer, pu8Data[i]);
		}
	}
	else
	{	
		/* write 0 if the MSAA program is null */
		if(GLSLPT_FRAGMENT == psShader->eProgramType)
		{
			WriteU32(psBuffer, 0);
		}
	}

	/*
	 * 7- Pack the constants
	 */
	if(psSymbolList->uNumCompsUsed && !psSymbolList->pfConstantData)
	{
		return SGXBS_INVALID_ARGUMENTS_ERROR;
	}
	
	WriteArrayHeader(psBuffer, psSymbolList->uNumCompsUsed);

	for(i=0; i < psSymbolList->uNumCompsUsed; ++i)
	{
		WriteFloat(psBuffer, psSymbolList->pfConstantData[i]);
	}

	/*
	 * 8- Pack the symbol bindings
	 */
	eError = PackSymbolBindings(psSymbolList->psBindingSymbolEntries, psSymbolList->uNumBindings, psBuffer);

	if(eError != SGXBS_NO_ERROR)
	{
		return eError;
	}

	return SGXBS_NO_ERROR;
}

static SGXBS_Error PackUserBindings(const GLSLAttribUserBinding* psUserBinding, SGXBS_Buffer* psBuffer)
{
	IMG_UINT32 u32Bindings =0, u32ArraysPosition, u32LastPosition;
	const GLSLAttribUserBinding* psBinding = psUserBinding;

	u32ArraysPosition = psBuffer->u32CurrentPosition;
	/* Write bogus array header */
	WriteArrayHeader(psBuffer, 0xBEEF);

	while(psBinding)
	{
		WriteString(psBuffer, psBinding->pszName);
		WriteU32(psBuffer, psBinding->i32Index);

		u32Bindings++;
		psBinding = psBinding->psNext;
	}

	u32LastPosition = psBuffer->u32CurrentPosition;
	psBuffer->u32CurrentPosition = u32ArraysPosition;
	WriteArrayHeader(psBuffer, u32Bindings);
	psBuffer->u32CurrentPosition = u32LastPosition;

	return SGXBS_NO_ERROR;
}

static SGXBS_Error PackRevision(const GLSLCompiledUniflexProgram* psShader, SGXBS_Buffer* psBuffer)
{
	IMG_UINT32   u32MainRevisionBodyPosition, u32MainRevisionSizePosition, u32LastPosition;
	SGXBS_Error  eError;
	IMG_UINT16   u16Core = 0, u16CoreRevision = 0;

	GetCoreAndRevisionNumber(&u16Core, &u16CoreRevision);

	/*
	 * Fill in the main revision header.
	 */

	/* 1- Software version */
	WriteU16(psBuffer, SGXBS_SOFTWARE_VERSION_1);

	/* 2- Major hardware version */
	WriteU16(psBuffer, u16Core);

	/* 3- Hardware core revision */
	WriteU16(psBuffer, u16CoreRevision);

	/* 4- Write a zeroed 16-bit reserved word */
	WriteU16(psBuffer, 0);

	/* 5- DDK version. Not used for offline binary checking, 
	   but potentially used by blobcache
	 */
	WriteU32(psBuffer, PVRVERSION_BUILD);

	/* 6- GLSL compiled Uniflex program interface version */
	WriteU32(psBuffer, GLSL_COMPILED_UNIFLEX_INTERFACE_VER);

	/* 7- USP_PC_SHADER version */
	WriteU32(psBuffer, USP_PC_SHADER_VER);

	/* Write a bogus revision size to be overwritten later */
	u32MainRevisionSizePosition =  psBuffer->u32CurrentPosition;
	WriteU32(psBuffer, 0xDEADBEEF);
	u32MainRevisionBodyPosition = psBuffer->u32CurrentPosition;

	/*
	 * Fill in the main revision body
	 */
	eError = PackUniflexProgram(psShader, psBuffer);

	if(eError != SGXBS_NO_ERROR)
	{
		return eError;
	}

	/*
	 * Rewrite the revision size now that we know it.
	 */
	u32LastPosition = psBuffer->u32CurrentPosition;
	psBuffer->u32CurrentPosition = u32MainRevisionSizePosition;
	WriteU32(psBuffer, u32LastPosition - u32MainRevisionBodyPosition);
	psBuffer->u32CurrentPosition = u32LastPosition;

	return SGXBS_NO_ERROR;
}

static SGXBS_Error PackBinary(const GLSLCompiledUniflexProgram* psShader, SGXBS_Buffer* psBuffer)
{
	SGXBS_Error  eError;
	IMG_UINT32   u32FirstRevisionHeaderPosition, u32HashPosition, u32LastPosition;
	SGXBS_Hash   sHash;

	/*
	 * Check arguments.
	 */
	if(!psShader || !psBuffer)
	{
		return SGXBS_INVALID_ARGUMENTS_ERROR;
	}

	if(!psShader->bSuccessfullyCompiled)
	{
		/* Packing a useless shader? No way! */
		return SGXBS_INVALID_ARGUMENTS_ERROR;
	}

	/*
	 * 1- Write the magic header number
	 */
	WriteU32(psBuffer, SGXBS_HEADER_MAGIC_NUMBER);

	/*
	 * 2- Get the current position. Write a bogus hash that will be overwritten later
	 */
	u32HashPosition = psBuffer->u32CurrentPosition;
	WriteU32(psBuffer, 0xDEADBEEF);

	/* Write the revision data */
	u32FirstRevisionHeaderPosition = psBuffer->u32CurrentPosition;
	eError = PackRevision(psShader, psBuffer);

	if(eError != SGXBS_NO_ERROR)
	{
		return eError;
	}

	/* Compute the hash of all the binary except for the binary header */
	sHash = SGXBS_ComputeHash(&psBuffer->pu8Buffer[u32FirstRevisionHeaderPosition], 
	                          psBuffer->u32CurrentPosition -u32FirstRevisionHeaderPosition);

	/* Rewind to the place where we'll overwrite the bogus hash */
	u32LastPosition = psBuffer->u32CurrentPosition;
	psBuffer->u32CurrentPosition = u32HashPosition;

	WriteU32(psBuffer, sHash.u32Hash);

	/* Go back to the end of the written data */
	psBuffer->u32CurrentPosition = u32LastPosition;

	if(psBuffer->bOverflow)
	{
		return SGXBS_OUT_OF_MEMORY_ERROR;
	}

	if(psBuffer->bInvalidArgument)
	{
		return SGXBS_INVALID_ARGUMENTS_ERROR;
	}

	return SGXBS_NO_ERROR;
}

static SGXBS_Error PackProgramBinary(const GLSLCompiledUniflexProgram* psVertex, const GLSLCompiledUniflexProgram* psFragment, const GLSLAttribUserBinding* psUserBinding, SGXBS_Buffer* psBuffer)
{
	SGXBS_Error  eError = SGXBS_NO_ERROR; 
	SGXBS_Hash   sHash;
	IMG_UINT16 u16Core = 0, u16CoreRevision = 0;
	IMG_UINT32 u32HashPosition, u32FirstRevisionHeaderPosition, u32MainRevisionSizePosition, u32MainRevisionBodyPosition, u32LastPosition;	

	if(!psVertex || !psFragment ||  !psBuffer)
	{
		return SGXBS_INVALID_ARGUMENTS_ERROR;
	}

	GetCoreAndRevisionNumber(&u16Core, &u16CoreRevision);

	/************************************************
		Write the meta data 1 (magic number and hash) 
	*************************************************/

	/* 1- Write the magic header number */
	WriteU32(psBuffer, SGXBS_HEADER_MAGIC_NUMBER);

	/* 2- Get the current position. Write a bogus hash that will be overwritten later */
	u32HashPosition = psBuffer->u32CurrentPosition;
	WriteU32(psBuffer, 0xDEADBEEF);

	/************************************************
		Write the meta data 2 (revision data) 
	*************************************************/
	u32FirstRevisionHeaderPosition = psBuffer->u32CurrentPosition;
	
	/* Fill in the main revision header. */

	/* 3  - Software version */
	WriteU16(psBuffer, SGXBS_SOFTWARE_VERSION_1);

	/* 4- Major hardware version */
	WriteU16(psBuffer, u16Core);

	/* 5- Hardware core revision */
	WriteU16(psBuffer, u16CoreRevision);

	/* 6- Write a zeroed 16-bit reserved word */
	WriteU16(psBuffer, 0);

	/* 7- Used to be a 32-bit hash of the project version string. Not used any more in favour of
	 * items 8 and 9 below
	 */
	WriteU32(psBuffer, 0);

	/* 8- GLSL compiled Uniflex program interface version */
	WriteU32(psBuffer, GLSL_COMPILED_UNIFLEX_INTERFACE_VER);

	/* 9- USP_PC_SHADER version */
	WriteU32(psBuffer, USP_PC_SHADER_VER);

	/* 10- Write a bogus revision size to be overwritten later */
	u32MainRevisionSizePosition =  psBuffer->u32CurrentPosition;
	WriteU32(psBuffer, 0xDEADBEEF);
	u32MainRevisionBodyPosition = psBuffer->u32CurrentPosition;

	/************************************************
		Write the data 
	*************************************************/

	/* 11- pack vertex shader info */
	eError = PackUniflexProgram(psVertex, psBuffer);

	/* 12- pack fragment shader info */
	eError = PackUniflexProgram(psFragment, psBuffer);

	/* 13- pack user bindings info */
	eError = PackUserBindings(psUserBinding, psBuffer);

	/************************************************
		Write revision size and hash 
	*************************************************/

	/* Rewrite the revision size now that we know it. */
	u32LastPosition = psBuffer->u32CurrentPosition;
	psBuffer->u32CurrentPosition = u32MainRevisionSizePosition;
	WriteU32(psBuffer, u32LastPosition - u32MainRevisionBodyPosition);
	psBuffer->u32CurrentPosition = u32LastPosition;
	
	/* Compute the hash of all the binary except for the binary header */
	sHash = SGXBS_ComputeHash(&psBuffer->pu8Buffer[u32FirstRevisionHeaderPosition], 
	                          psBuffer->u32CurrentPosition - u32FirstRevisionHeaderPosition);

	/* Rewind to the place where we'll overwrite the bogus hash */
	u32LastPosition = psBuffer->u32CurrentPosition;
	psBuffer->u32CurrentPosition = u32HashPosition;

	WriteU32(psBuffer, sHash.u32Hash);

	/* Go back to the end of the written data */
	psBuffer->u32CurrentPosition = u32LastPosition;

	if(psBuffer->bOverflow)
	{
		return SGXBS_OUT_OF_MEMORY_ERROR;
	}

	if(psBuffer->bInvalidArgument)
	{
		return SGXBS_INVALID_ARGUMENTS_ERROR;
	}

	return eError;
}


/* ------------------------------------------------------------------------------------------- */
/* --------------------------------------- ENTRY POINTS  ------------------------------------- */
/* ------------------------------------------------------------------------------------------- */



IMG_EXPORT 
SGXBS_Error SGXBS_CreateBinaryShader(const GLSLCompiledUniflexProgram* psShader,
	IMG_VOID* (*pfnMalloc)(IMG_UINT32), IMG_VOID (*pfnFree)(IMG_VOID*),
	IMG_VOID** ppvBinaryShader, IMG_UINT32* pu32BinaryShaderLengthInBytes)
{
	SGXBS_Buffer          sBuffer;
	SGXBS_Error           eError = SGXBS_NO_ERROR;
	IMG_BOOL              bDone;

	if(!psShader || !pfnMalloc || !ppvBinaryShader || !pu32BinaryShaderLengthInBytes)
	{
		/* Null arguments */
		return SGXBS_INVALID_ARGUMENTS_ERROR;
	}

	/*
	 * Whether this function will return true or false is determined at compile-time.
	 * However, the cost of calling it is totally negligible so we'll keep it for the safety it provides.
	 */
	if(!SGXBS_TestBinaryShaderInterface())
	{
		return SGXBS_INTERNAL_ERROR;
	}

	/* We initially allocate a fixed amount of memory for the output buffer. We then try to pack the
	 * compiled shader in that buffer. If it is too small, we will receive a SGXBS_OUT_OF_MEMORY_ERROR.
	 * If that happens (unlikely but possible), we try to allocate a bigger buffer and try packing again.
	 * That is repeated until either the buffer memory allocation fails or the packing succeeds.
	 */

	/* "16KB ought to be enough for anyone" --Anonymous */
	*pu32BinaryShaderLengthInBytes = 16*1024;
	bDone= IMG_FALSE;

	while(!bDone)
	{
		*ppvBinaryShader = (*pfnMalloc)(*pu32BinaryShaderLengthInBytes);
		if(!*ppvBinaryShader)
		{
			/* There is nothing to do. We are out of memory */
			return SGXBS_OUT_OF_MEMORY_ERROR;
		}

		/* Fill the buffer with zeroes to minimize the damage caused by potential bugs */
		memset(*ppvBinaryShader, 0, *pu32BinaryShaderLengthInBytes);

		sBuffer.pu8Buffer            = *ppvBinaryShader;
		sBuffer.u32CurrentPosition   = 0;
		sBuffer.u32BufferSizeInBytes = *pu32BinaryShaderLengthInBytes;
		sBuffer.bOverflow            = IMG_FALSE;
		sBuffer.bInvalidArgument     = IMG_FALSE;

		eError = PackBinary(psShader, &sBuffer);

		if(eError != SGXBS_NO_ERROR)
		{
			(*pfnFree)(*ppvBinaryShader);
		}

		if(eError == SGXBS_OUT_OF_MEMORY_ERROR)
		{
			/* Duplicate the buffer size and try again */
			*pu32BinaryShaderLengthInBytes *= 2;
		}
		else
		{
			bDone = IMG_TRUE;
			*pu32BinaryShaderLengthInBytes = sBuffer.u32CurrentPosition;
		}
	}

	return eError;
}

IMG_EXPORT 
SGXBS_Error SGXBS_CreateBinaryProgram(const GLSLCompiledUniflexProgram* psVertex, const GLSLCompiledUniflexProgram* psFragment, const GLSLAttribUserBinding* psUserBinding,
										IMG_UINT32 u32BinarySizeInBytes, IMG_UINT32 * ui32Length, IMG_VOID* pvBinaryShader, IMG_BOOL bCreateBinary)
{
	SGXBS_Buffer          sBuffer;
	SGXBS_Error           eError = SGXBS_NO_ERROR;

	if(!psVertex || !psFragment || !pvBinaryShader || (u32BinarySizeInBytes <= 0))
	{
		/* Null arguments */
		return SGXBS_INVALID_ARGUMENTS_ERROR;
	}

	/*
	 * Whether this function will return true or false is determined at compile-time.
	 * However, the cost of calling it is totally negligible so we'll keep it for the safety it provides.
	 */
	if(!SGXBS_TestBinaryShaderInterface())
	{
		return SGXBS_INTERNAL_ERROR;
	}

	/* create the binary if boolean is true... */
	if(bCreateBinary)
	{
		/* Fill the buffer with zeroes to minimize the damage caused by potential bugs */
		memset(pvBinaryShader, 0, u32BinarySizeInBytes);

		sBuffer.pu8Buffer            = pvBinaryShader;
		sBuffer.u32CurrentPosition   = 0;
		sBuffer.u32BufferSizeInBytes = u32BinarySizeInBytes;
		sBuffer.bOverflow            = IMG_FALSE;
		sBuffer.bInvalidArgument     = IMG_FALSE;

		eError = PackProgramBinary(psVertex, psFragment, psUserBinding, &sBuffer);

		if(eError == SGXBS_NO_ERROR)
		{
			if(ui32Length)
			{
				*ui32Length = sBuffer.u32CurrentPosition;
			}
		}
	}
	else /* ... otherwise just return what would be the binary size */
	{	
		IMG_UINT32 ui32MetaData1, ui32MetaData2, ui32DataVertex, ui32DataFragment, ui32DataUserBindings;
		struct _USP_PC_SHADER_  *psUniPatchInput, *psUniPatchInputMSAATrans;
		GLSLBindingSymbolList    *psSymbolList; 

		/* FIXME! use asserts and defines */
		ui32MetaData1 = SGXBS_METADATA_1*sizeof(IMG_UINT32);
		ui32MetaData2 = SGXBS_METADATA_2*sizeof(IMG_UINT32);

		psUniPatchInput = psVertex->psUniFlexCode->psUniPatchInput; 
		psSymbolList	= psVertex->psBindingSymbolList;

		/* vertex data size */
		ui32DataVertex = 3 *sizeof(IMG_UINT32)/* type/flag and reserved */
						+ 1 *sizeof(IMG_UINT32)/* varying mask */
						+ (2*SGXBS_NUM_TC_REGISTERS) /* tex dimensions and precisions */;

		ui32DataVertex += 1*sizeof(IMG_UINT32) + SGXBS_UNIPATCH_HEADER_SIZE + psUniPatchInput->uSize;
		ui32DataVertex += 2/*array header*/ + (psSymbolList->uNumCompsUsed)*sizeof(IMG_FLOAT);
		ui32DataVertex += GetNumOfBytesForWritingBindings(psSymbolList->psBindingSymbolEntries, psSymbolList->uNumBindings);
		
		/* fragment data size */
		psUniPatchInput				= psFragment->psUniFlexCode->psUniPatchInput;
		psUniPatchInputMSAATrans	= psFragment->psUniFlexCode->psUniPatchInputMSAATrans;
		psSymbolList				= psFragment->psBindingSymbolList;

		ui32DataFragment = 3 *sizeof(IMG_UINT32)/* type/flag and reserved */
						+ 1 *sizeof(IMG_UINT32)/* varying mask */
						+ (2*SGXBS_NUM_TC_REGISTERS) /* tex dimensions and precisions */
						+ 1*sizeof(IMG_UINT32) + SGXBS_UNIPATCH_HEADER_SIZE + psUniPatchInput->uSize;
		

		ui32DataFragment += 2/*array header*/ + (psSymbolList->uNumCompsUsed)*sizeof(IMG_FLOAT);
		ui32DataFragment += GetNumOfBytesForWritingBindings(psSymbolList->psBindingSymbolEntries, psSymbolList->uNumBindings);

		if(psUniPatchInputMSAATrans)
		{
			ui32DataFragment += 1*sizeof(IMG_UINT32) + SGXBS_UNIPATCH_HEADER_SIZE + psUniPatchInputMSAATrans->uSize;
		}
		else
		{
			ui32DataFragment += 1*sizeof(IMG_UINT32);
		}

		ui32DataUserBindings = GetNumOfBytesForWritingUserBindings(psUserBinding);

		*ui32Length = ui32MetaData1 + ui32MetaData2 + ui32DataVertex + ui32DataFragment + ui32DataUserBindings; 
	}

	return eError;
}
