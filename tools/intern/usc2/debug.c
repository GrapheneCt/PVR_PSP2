/******************************************************************************
 * Name         : debug.c
 * Title        : Debugging utils, intermediate code printing, etc.
 * Created      : April 2005
 *
 * Copyright    : 2002-2006 by Imagination Technologies Limited.
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
 * Modifications:-
 * $Log: debug.c $
 *****************************************************************************/

#include "uscshrd.h"
#include "bitops.h"
#include "math.h"
#include "usedef.h"
#include "reggroup.h"


#ifdef USER_PERFORMANCE_STATS_ONLY
FILE* uniflexfile = NULL;
#endif


#ifndef USER_PERFORMANCE_STATS_ONLY

#define DBGANDPDUMPOUT(FMT)	\
		DBG_PRINTF((DBG_MESSAGE, "%s", FMT));  \
		if (psState->pfnPDump) \
		{ \
			psState->pfnPDump(psState->pvPDumpFnDrvParam, "%s", FMT); \
		}

#define DBGANDPDUMPOUT1(FMT, A)	\
		DBG_PRINTF((DBG_MESSAGE, FMT, A));  \
		if (psState->pfnPDump) \
		{ \
			psState->pfnPDump(psState->pvPDumpFnDrvParam, FMT, A); \
		}

#define DBGANDPDUMPOUT2(FMT, A, B)	\
		DBG_PRINTF((DBG_MESSAGE, FMT, A, B));  \
		if (psState->pfnPDump) \
		{ \
			psState->pfnPDump(psState->pvPDumpFnDrvParam, FMT, A, B); \
		}

#define DBGANDPDUMPOUT3(FMT, A, B, C)	\
		DBG_PRINTF((DBG_MESSAGE, FMT, A, B, C));  \
		if (psState->pfnPDump) \
		{ \
			psState->pfnPDump(psState->pvPDumpFnDrvParam, FMT, A, B, C); \
		}

#define DBGANDPDUMPOUT4(FMT, A, B, C, D)	\
		DBG_PRINTF((DBG_MESSAGE, FMT, A, B, C, D));  \
		if (psState->pfnPDump) \
		{ \
			psState->pfnPDump(psState->pvPDumpFnDrvParam, FMT, A, B, C, D); \
		}

#define DBGANDPDUMPOUT5(FMT, A, B, C, D, E)	\
		DBG_PRINTF((DBG_MESSAGE, FMT, A, B, C, D, E));  \
		if (psState->pfnPDump) \
		{ \
			psState->pfnPDump(psState->pvPDumpFnDrvParam, FMT, A, B, C, D, E); \
		}

#define DBGANDPDUMPOUT8(FMT, A, B, C, D, E, F, G, H)	\
		DBG_PRINTF((DBG_MESSAGE, FMT, A, B, C, D, E, F, G, H));  \
		if (psState->pfnPDump) \
		{ \
			psState->pfnPDump(psState->pvPDumpFnDrvParam, FMT, A, B, C, D, E, F, G, H); \
		}

#define DBGANDPDUMPOUT13(FMT, A, B, C, D, E, F, G, H, I, J, K, L, M)	\
		DBG_PRINTF((DBG_MESSAGE, FMT, A, B, C, D, E, F, G, H, I, J, K, L, M));  \
		if (psState->pfnPDump) \
		{ \
			psState->pfnPDump(psState->pvPDumpFnDrvParam, FMT, A, B, C, D, E, F, G, H, I, J, K, L, M); \
		}

#define DBGANDPDUMPOUT14(FMT, A, B, C, D, E, F, G, H, I, J, K, L, M, N)	\
		DBG_PRINTF((DBG_MESSAGE, FMT, A, B, C, D, E, F, G, H, I, J, K, L, M, N));  \
		if (psState->pfnPDump) \
		{ \
			psState->pfnPDump(psState->pvPDumpFnDrvParam, FMT, A, B, C, D, E, F, G, H, I, J, K, L, M, N); \
		}


#else
 
#define DBGANDPDUMPOUT(FMT)	\
		if(uniflexfile) { \
			fprintf(uniflexfile,"%s\n",FMT);	\
		}
#define DBGANDPDUMPOUT1(FMT, A) \
		if(uniflexfile) { \
			fprintf(uniflexfile,FMT,A);  \
			fprintf(uniflexfile,"\n");  \
		}
#define DBGANDPDUMPOUT2(FMT, A, B)      \
		if(uniflexfile) { \
			fprintf(uniflexfile,FMT,A,B);        \
			fprintf(uniflexfile,"\n");  \
		}

#define DBGANDPDUMPOUT3(FMT, A, B, C)   \
		if(uniflexfile) { \
			fprintf(uniflexfile,FMT,A,B,C);        \
			fprintf(uniflexfile,"\n");  \
		}

#define DBGANDPDUMPOUT4(FMT, A, B, C, D)        \
		if(uniflexfile) { \
			fprintf(uniflexfile,FMT,A,B,C,D);        \
			fprintf(uniflexfile,"\n");  \
		}

#define DBGANDPDUMPOUT5(FMT, A, B, C, D, E)     \
		if(uniflexfile) { \
			fprintf(uniflexfile,FMT,A,B,C,D,E);        \
			fprintf(uniflexfile,"\n");  \
		}

#define DBGANDPDUMPOUT8(FMT, A, B, C, D, E, F, G, H)    \
		if(uniflexfile) { \
			fprintf(uniflexfile,FMT,A,B,C,D,E,F,G,H);        \
			fprintf(uniflexfile,"\n");  \
		}

#define DBGANDPDUMPOUT13(FMT, A, B, C, D, E, F, G, H, I, J, K, L, M)    \
		if(uniflexfile) { \
			fprintf(uniflexfile,FMT,A,B,C,D,E,F,G,H,I,J,K,L,M);        \
			fprintf(uniflexfile,"\n");  \
		}

#define DBGANDPDUMPOUT14(FMT, A, B, C, D, E, F, G, H, I, J, K, L, M, N) \
		if(uniflexfile) { \
			fprintf(uniflexfile,FMT,A,B,C,D,E,F,G,H,I,J,K,L,M,M);        \
			fprintf(uniflexfile,"\n");  \
		}



#endif	// USER_PERFORMANCE_STATS_ONLY



#if defined(DEBUG)
IMG_INTERNAL
IMG_VOID DebugPrintf(PINTERMEDIATE_STATE psState, DBG_MESSAGE_TYPE iType, const IMG_CHAR *pszFormat, ...)
/*****************************************************************************
 FUNCTION	: DebugPrintf

 PURPOSE	: Print a debug message with given debug level indication, it also
			  Includes source file and line number information.
			  Message can be suppressed according to debug level.

 PARAMETERS	: psState					- Compiler state.
			  iType						- Debug Level.
			  pszFormat					- Messgae format string
			  ...						- Rest of the arguments

 RETURNS	: Nothing.
*****************************************************************************/
{
#define _MESSAGE_SIZE_GRAN	256
	int n, size = _MESSAGE_SIZE_GRAN;
	char *p = IMG_NULL, *np;
	const char *pszType;
	
	va_list args;
	
#if defined(UF_TESTBENCH)
	PVR_UNREFERENCED_PARAMETER(psState);
#endif
	
	if(iType == __DBG_SUPPRESS)
	{
		return;
	}

	switch(iType)
	{
		case __DBG_ERROR:
			pszType = "Error: ";
			break;
		case __DBG_WARNING:
			pszType = "Warning: ";
			break;
		case __DBG_ABORT:
			pszType = "Error: ";
			break;
		default:
			pszType = "";
			break;
	}
	
#if defined(UF_TESTBENCH)
	/* Can't relay on psState in this type of build. */
	if ((np = (char*)malloc(size)) == NULL)
#else
	if ((np = (char*)UscAlloc(psState, size)) == NULL)
#endif
	{
		/* Exiting without message notice */
		return;
	}
	
	do
	{
		IMG_UINT32	uRemainingSize;
		np[0] = '\0';
		
		strcat(np, pszType);

		p = np + strlen(np);
		uRemainingSize = size - (IMG_UINT32)strlen(np);
	
		/* Try to print in the allocated space. */
		va_start(args, pszFormat);
		n = vsnprintf (p, uRemainingSize, pszFormat, args);
		va_end(args);
		/* If that worked, return the string. */
		if (n > -1 && n < size)
		{
			break;
		}
		
		/* else retry with new size */
		size = size + _MESSAGE_SIZE_GRAN;
#if defined(UF_TESTBENCH)
		p = np;
		if ((np = (char*)realloc(p, size)) == NULL)
		{
			free(p);
#else
		UscFree(psState, np);
		if ((np = (char*)UscAlloc(psState, size)) == NULL)
		{
#endif
			return;
		}
	}while(np);
	
#if defined(UF_TESTBENCH)
	{
		if(iType < __DBG_WARNING)
		{
			fputs(np, stdout);
			fputs("\n", stdout);
		}
		else
		{
			fputs(np, stderr);
			fputs("\n", stderr);
		}
	}
#else
	{
		/* DEBUG & !UF_TESTBENCH */
		/* send message to standard error printing function */
		psState->pfnPrint(np);
	}
#endif

#if defined(UF_TESTBENCH)
	free(np);
#else
	UscFree(psState, np);
#endif
#undef _MESSAGE_SIZE_GRAN
}

#if defined(UF_TESTBENCH)
IMG_INTERNAL
IMG_VOID DumpIntermediateDependacyGraph(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvGraphName)
/*****************************************************************************
 FUNCTION	: DumpIntermediateDependacyGraph

 PURPOSE	: Dump instruction dependency graph

 PARAMETERS	: psState	- Compile state
			  psBlock	- Block to dump
			  pvGraphName - Graph Name, will be used as file name

 RETURNS	: Nothing.
*****************************************************************************/
{
	PDGRAPH_STATE	psDepState;
	IMG_CHAR *pszGraphName = (pvGraphName) ? pvGraphName : "Unknown-dep-graph";
	
	psDepState = ComputeBlockDependencyGraph(psState, psBlock, IMG_TRUE);
	
	DotPrintDepGraph(psDepState, pszGraphName, IMG_FALSE);
	
	FreeBlockDGraphState(psState, psBlock);
}
#endif /* defined(UF_TESTBENCH) */
#endif /* defined(DEBUG) */

static IMG_CHAR * const g_pszRegFormat[] = 
{
	"f32",		/* UF_REGFORMAT_F32 */
	"f16",		/* UF_REGFORMAT_F16 */
	"c10",		/* UF_REGFORMAT_C10 */
	"u8",		/* UF_REGFORMAT_U8 */
	"i32",		/* UF_REGFORMAT_I32 */
	"u32",		/* UF_REGFORMAT_U32 */
	"i16",		/* UF_REGFORMAT_I16 */
	"u16",		/* UF_REGFORMAT_U16 */
	"untyped",	/* UF_REGFORMAT_UNTYPED */
	"i8",		/* UF_REGFORMAT_I8_UN */
	"ui8",		/* UF_REGFORMAT_U8_UN */
};

static IMG_VOID PrintValidShaderOutputs(
										PINTERMEDIATE_STATE			psState, 
										PUNIFLEX_PROGRAM_PARAMETERS	psSAOffsets)
/*****************************************************************************
 FUNCTION	: PrintValidShaderOutputs

 PURPOSE	: Prints valid Shader Outputs bit mask.

 PARAMETERS	: psSAOffsets		- Uniflex program parameters              

 RETURNS	: Nothing.
*****************************************************************************/
{
	char	tempString[19 + (USC_SHADER_OUTPUT_MASK_DWORD_COUNT * 9) + 1];
	char*	pcCurLocation = tempString;
	pcCurLocation = (pcCurLocation + sprintf(pcCurLocation, "VALIDSHADEROUTPUTS\t"));
	{
		IMG_UINT32 uDwordIdx;
		for(uDwordIdx = 0; uDwordIdx < USC_SHADER_OUTPUT_MASK_DWORD_COUNT; uDwordIdx++)
		{
			pcCurLocation = (pcCurLocation + sprintf(pcCurLocation, " %08X", psSAOffsets->puValidShaderOutputs[uDwordIdx]));			
		}
	}
	DBGANDPDUMPOUT(tempString);
	return;
}

static IMG_VOID PrintInvariantShaderOutputs(
											PINTERMEDIATE_STATE			psState,
											PUNIFLEX_PROGRAM_PARAMETERS	psSAOffsets)
/*****************************************************************************
 FUNCTION	: PrintInvariantShaderOutputs

 PURPOSE	: Prints invariant Shader Outputs bit mask.

 PARAMETERS	: psSAOffsets		- Uniflex program parameters              

 RETURNS	: Nothing.
*****************************************************************************/
{
	char	tempString[23 + (USC_SHADER_OUTPUT_MASK_DWORD_COUNT * 9) + 1];
	char*	pcCurLocation = tempString;
	pcCurLocation = (pcCurLocation + sprintf(pcCurLocation, "INVARIANTSHADEROUTPUTS\t"));
	{
		IMG_UINT32 uDwordIdx;
		for(uDwordIdx = 0; uDwordIdx < USC_SHADER_OUTPUT_MASK_DWORD_COUNT; uDwordIdx++)
		{
			pcCurLocation = (pcCurLocation + sprintf(pcCurLocation, " %08X", psSAOffsets->puInvariantShaderOutputs[uDwordIdx]));			
		}
	}
	DBGANDPDUMPOUT(tempString);
	return;
}

#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP)
/*****************************************************************************
 FUNCTION	: PrintInputInst

 PURPOSE	: Print an input instruction.

 PARAMETERS	: psState			- The current compiler state
			  uCompileFlags		- Compile flags.
              i32Indent         - Indentation level
			  psInst			- Instruction to print.
			  pdwTextureToDumps	- Variable which is updated which a bitmask
								  of the textures referenced by the instruction.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID PrintInputInst(PINTERMEDIATE_STATE psState,
							   IMG_UINT32 uCompileFlags,
							   IMG_INT32 i32Indent, 
							   PUNIFLEX_INST psInst, 
							   IMG_PUINT32 puTexturesToDump)
{
	char apcTempString[512] = {'0'};

	InternalDecodeInputInst(uCompileFlags, psInst, i32Indent, apcTempString, puTexturesToDump);
	DBGANDPDUMPOUT(apcTempString);
}
 #endif 

static
IMG_PCHAR PrintTextureFilterType(PINTERMEDIATE_STATE			psState,
								 IMG_PCHAR						pszStr, 
								 IMG_PCHAR						pszType, 
								 UNIFLEX_TEXTURE_FILTER_TYPE	eType)
{
	static const IMG_PCHAR apszFilteringType[] = 
	{
		"UNSPECIFIED",	/* UNIFLEX_TEXTURE_FILTER_TYPE_UNSPECIFIED */
		"POINT",		/* UNIFLEX_TEXTURE_FILTER_TYPE_POINT */
		"LINEAR",		/* UNIFLEX_TEXTURE_FILTER_TYPE_LINEAR */
		"ANISO",		/* UNIFLEX_TEXTURE_FILTER_TYPE_ANISOTROPIC */
	};

	if (eType != UNIFLEX_TEXTURE_FILTER_TYPE_UNSPECIFIED)
	{
		ASSERT(eType < (sizeof(apszFilteringType) / sizeof(apszFilteringType[0])));
		pszStr += sprintf(pszStr, "%s=%s", pszType, apszFilteringType[eType]);
	}
	return pszStr;
}

extern IMG_CHAR const g_pszChannels[];	/* From usc_utils.c */

/*****************************************************************************
 FUNCTION	: PrintRangeList

 PURPOSE	: Print a list of ranges from the compiler parameters.

 PARAMETERS	: psState			- Compiler state.
			  pszRangeType		- Mnemonic for the range declaration.
			  psRangeList		- List of ranges to print.

 RETURNS	: Nothing.
*****************************************************************************/
static
IMG_VOID PrintRangeList(PINTERMEDIATE_STATE psState, IMG_CHAR const* pszRangeType, PUNIFLEX_RANGES_LIST psRangeList)
{
	IMG_UINT32 uRangeIdx;
	for (uRangeIdx = 0; uRangeIdx < psRangeList->uRangesCount; uRangeIdx++)
	{
		PUNIFLEX_RANGE	psRange = &psRangeList->psRanges[uRangeIdx];

		DBGANDPDUMPOUT3("%s %u, %u", pszRangeType, psRange->uRangeStart, psRange->uRangeEnd);
	}
}

static
IMG_VOID PrintConstantBufferDescription(PINTERMEDIATE_STATE			psState, 
										PUNIFLEX_PROGRAM_PARAMETERS psProgParams,
										IMG_UINT32					uCompilerFlags, 
										IMG_UINT32					uCBIdx)
/*****************************************************************************
 FUNCTION	: PrintConstantBufferDescription

 PURPOSE	: Print the parameters for a constant buffer.

 PARAMETERS	: psState			- Compiler state.
			  psProgParams		- Compiler parameters.
			  uCompilerFlags
			  uCBIdx			- Index of the constant buffer to print.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_CHAR					cLocType;
	PUNIFLEX_CONSTBUFFERDESC	psDesc = &psProgParams->asConstBuffDesc[uCBIdx];

	switch (psDesc->eConstBuffLocation)
	{
		case UF_CONSTBUFFERLOCATION_DONTCARE:
		{
			cLocType = 'D';
			break;
		}
		case UF_CONSTBUFFERLOCATION_SAS_ONLY:
		{
			cLocType = 'S';
			break;
		}
		default:
		{
			cLocType = 'M';
			break;
		}
	}
	DBGANDPDUMPOUT5("CONSTBUFFDESC %d, %c, %d, %d, %d", 
					uCBIdx, 
					cLocType, 
					psDesc->uStartingSAReg, 
					psDesc->uBaseAddressSAReg,
					psDesc->uSARegCount);	

	if ((uCompilerFlags & UF_CONSTRANGES) != 0)
	{
		IMG_CHAR	acRangeName[32];

		if ((uCompilerFlags & UF_MULTIPLECONSTANTSBUFFERS) == 0)
		{
			strcpy(acRangeName, "CONSTRANGE");
		}
		else
		{
			sprintf(acRangeName, "CONSTRANGE %d,", uCBIdx);
		}

		PrintRangeList(psState, acRangeName, &psDesc->sConstsBuffRanges);
	}

	if ((uCompilerFlags & UF_ENABLERANGECHECK) != 0)
	{
		if ((uCompilerFlags & UF_MULTIPLECONSTANTSBUFFERS) == 0)
		{
			DBGANDPDUMPOUT1("RANGECHECK %u", psDesc->uRelativeConstantMax);
		}
		else
		{
			DBGANDPDUMPOUT2("RANGECHECK %u, %u", uCBIdx, psDesc->uRelativeConstantMax);
		}
	}
}

static
IMG_VOID PrintInputBitArray(PINTERMEDIATE_STATE	psState,
							IMG_PUINT32			puBitArray,
							IMG_UINT32			uBitArrayLength,
							IMG_CHAR const*		pszBitArrayName)
{
	IMG_BOOL	bNonZeroEntries;
	IMG_UINT32	i;

	bNonZeroEntries = IMG_FALSE;
	for (i = 0; i < uBitArrayLength; i++)
	{
		if (puBitArray[i] != 0)
		{
			bNonZeroEntries = IMG_TRUE;
			break;
		}
	}

	if (bNonZeroEntries)
	{
		IMG_PCHAR	pszStr;
		IMG_CHAR	apcTempString[512];

		pszStr = apcTempString;
		pszStr += sprintf(pszStr, "%s", pszBitArrayName);

		for (i = 0; i < uBitArrayLength; i++)
		{
			pszStr += sprintf(pszStr, " %.8X", puBitArray[i]);
		}
		DBGANDPDUMPOUT(apcTempString);
	}
}

IMG_INTERNAL 
IMG_VOID PrintInput(PINTERMEDIATE_STATE			psState,
					IMG_PCHAR					pszBeginLabel,
					IMG_PCHAR					pszEndLabel,
					PUNIFLEX_INST				psProg,
					PUNIFLEX_CONSTDEF			psConstants,
					IMG_UINT32					uCompilerFlags,
					IMG_UINT32					uCompilerFlags2,
					PUNIFLEX_PROGRAM_PARAMETERS psProgramParameters,
					IMG_BOOL					bCompileForUSP
#ifdef USER_PERFORMANCE_STATS_ONLY
					,IMG_CHAR* szfilename
#endif
					)
/*****************************************************************************
 FUNCTION	: PrintInput

 PURPOSE	: Print a list of input instructions and program parameters

 PARAMETERS	: psProg					- Head of the input instruction list.
			  puTextureDimensions		- Dimensions of the textures used in the program.
			  psConstants				- Local constants used in the program.

 RETURNS	: Nothing.
*****************************************************************************/
{


#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) 
	PUNIFLEX_INST psInst;
	IMG_PUINT32 auTexturesToDump;
	IMG_UINT32 i;
	IMG_INT32 indent = 1;
	IMG_CHAR apcTempString[512];
#ifdef USER_PERFORMANCE_STATS_ONLY
	if(szfilename) {
	    uniflexfile = fopen(szfilename,"w");
	    if(!uniflexfile) {
		    printf("Error opening a file: check permissions!\n");
		    return;
	    }
	}
#endif


	psInst = psProg;

	DBGANDPDUMPOUT(pszBeginLabel);
	if (uCompilerFlags & UF_SPLITFEEDBACK)
	{
		DBGANDPDUMPOUT1("SPLITFB %u", psProgramParameters->uFeedbackInstCount);
	}
	if (uCompilerFlags & UF_NOALPHATEST)
	{
		DBGANDPDUMPOUT("NOALPHATEST");
	}
	PrintValidShaderOutputs(psState, psProgramParameters);
	PrintInvariantShaderOutputs(psState, psProgramParameters);
	if (psProgramParameters->eShaderType == USC_SHADERTYPE_VERTEX)
	{
		DBGANDPDUMPOUT("VERTEXSHADER" );
		if (uCompilerFlags & UF_REDIRECTVSOUTPUTS)
		{
			DBGANDPDUMPOUT("REDIRECTVSOUTPUTS");
		}
	}
	if (psProgramParameters->eShaderType == USC_SHADERTYPE_GEOMETRY)
	{
		DBGANDPDUMPOUT2("GEOMETRYSHADER %u, %u", psProgramParameters->uInputVerticesCount, psProgramParameters->uOutPutBuffersCount);
	}
	if((psProgramParameters->eShaderType == USC_SHADERTYPE_VERTEX) || (psProgramParameters->eShaderType == USC_SHADERTYPE_GEOMETRY))
	{
		PrintRangeList(psState, "SHADEROUTPUTRANGE", &psProgramParameters->sShaderOutPutRanges);
	}

	/*
		Print ranges within the shader inputs which are dynamically indexed.
	*/
	PrintRangeList(psState, "SHADERINPUTRANGE", &psProgramParameters->sShaderInputRanges);

	if (uCompilerFlags & UF_MSAATRANS)
	{
		DBGANDPDUMPOUT("MSAATRANS");
	}
	if (uCompilerFlags & UF_GLSL)
	{
		DBGANDPDUMPOUT("GLSL");
	}
	if (uCompilerFlags & UF_OPENCL)
	{
		DBGANDPDUMPOUT("OPENCL");
	}
	if (bCompileForUSP)
	{
		DBGANDPDUMPOUT("COMPILEFORUSP");
	}
	if (uCompilerFlags & UF_NOCONSTREMAP)
	{
		DBGANDPDUMPOUT("NOCONSTREMAP");
	}
	if (uCompilerFlags & UF_DONTRESETMOEAFTERPROGRAM)
	{
		DBGANDPDUMPOUT("NOMOERESET");
	}
	if (psProgramParameters->uPreFeedbackPhasePreambleCount > 0)
	{
		DBGANDPDUMPOUT1("PREAMBLECOUNT %u", psProgramParameters->uPreFeedbackPhasePreambleCount);
	}
	if (psProgramParameters->uPostFeedbackPhasePreambleCount > 0)
	{
		DBGANDPDUMPOUT1("POSTFBPREAMBLECOUNT %u", psProgramParameters->uPostFeedbackPhasePreambleCount);
	}
	if (psProgramParameters->uPostSplitPhasePreambleCount > 0)
	{
		DBGANDPDUMPOUT1("POSTSPLITPREAMBLECOUNT %u", psProgramParameters->uPostSplitPhasePreambleCount);
	}
	if (psProgramParameters->uExtraPARegisters > 0)
	{
		DBGANDPDUMPOUT1("EXTRAITR %u", psProgramParameters->uExtraPARegisters);
	}
	if (psProgramParameters->eShaderType == USC_SHADERTYPE_PIXEL)
	{
		switch (psProgramParameters->uPackDestType)
		{
			case USEASM_REGTYPE_TEMP:
			{
				DBGANDPDUMPOUT("PACKDEST R" );
				break;
			}
			case USEASM_REGTYPE_PRIMATTR:
			{
				DBGANDPDUMPOUT("PACKDEST P");
				break;
			}
			case USEASM_REGTYPE_OUTPUT:
			{
				DBGANDPDUMPOUT("PACKDEST O");
				break;
			}
			default:
			{
				DBGANDPDUMPOUT("PACKDEST ?");
				break;
			}
		}

		PrintInputBitArray(psState, 
						   psProgramParameters->auFlatShadedTextureCoordinates,
						   sizeof(psProgramParameters->auFlatShadedTextureCoordinates) / sizeof(psProgramParameters->auFlatShadedTextureCoordinates[0]),
						   "FLATSHADEDTEXCOORDS");
		PrintInputBitArray(psState,
						   psProgramParameters->auProjectedCoordinateMask,
						   sizeof(psProgramParameters->auProjectedCoordinateMask) / sizeof(psProgramParameters->auProjectedCoordinateMask[0]),
						   "PROJCOORD");

		if (psProgramParameters->uNumPDSPrimaryConstantsReserved > 0)
		{
			DBGANDPDUMPOUT1("RESERVEDPDSCONSTS %u", psProgramParameters->uNumPDSPrimaryConstantsReserved);
		}

		if (psProgramParameters->ePackDestFormat != UF_REGFORMAT_F32)
		{
			ASSERT((IMG_UINT32)(psProgramParameters->ePackDestFormat) < (sizeof(g_pszRegFormat) / sizeof(g_pszRegFormat[0])));
			DBGANDPDUMPOUT1("PACKDESTFORMAT %s", g_pszRegFormat[psProgramParameters->ePackDestFormat]);
		}

		if (psProgramParameters->ePackDestFormat == UF_REGFORMAT_U8)
		{
			DBGANDPDUMPOUT1("PACKPREC %u", psProgramParameters->uPackPrecision);
		}
		else
		{
			DBGANDPDUMPOUT4("OUTPUTSAT %u, %u, %u, %u", 
							psProgramParameters->puOutputSaturate[0], 
							psProgramParameters->puOutputSaturate[1], 
							psProgramParameters->puOutputSaturate[2], 
							psProgramParameters->puOutputSaturate[3]);
		}
	}
	DBGANDPDUMPOUT1("TARGET %s", UseAsmGetCoreDesc(&psProgramParameters->sTarget)->psFeatures->pszCoreName);
	DBGANDPDUMPOUT1("TARGETREV %u", psProgramParameters->sTarget.uiRev);
	if (uCompilerFlags & UF_EXTRACTCONSTANTCALCS)
	{
		DBGANDPDUMPOUT("EXTRACTCONSTANTCALCS");
	}

	if (psProgramParameters->uTextureState == (IMG_UINT32)-1)
	{
		DBGANDPDUMPOUT1("TEXTURESTATECONSTOFFSET %u", psProgramParameters->uTextureStateConstOffset);
	}

	if (uCompilerFlags & UF_MULTIPLECONSTANTSBUFFERS)
	{
		DBGANDPDUMPOUT("MULTIPLECONSTANTSBUFFERS");
	}

	if (uCompilerFlags & UF_SMPIMMCOORDOFFSETS)
	{
		DBGANDPDUMPOUT("SMPIMMCOORDOFFSETS");
	}

	if (uCompilerFlags & UF_CONSTEXPLICTADDRESSMODE)
	{
		DBGANDPDUMPOUT("CONSTEXPLICTADDRESSMODE");
	}

#if defined(TRACK_REDUNDANT_PCONVERSION)
	if (uCompilerFlags & UF_FORCE_C10_TO_F16_PRECISION)
	{
		DBGANDPDUMPOUT("HINTFORCEC10TOF16PRECISION");
	}
	
	if (uCompilerFlags & UF_FORCE_F16_TO_F32_PRECISION)
	{
		DBGANDPDUMPOUT("HINTFORCEF16TOF32PRECISION");
	}
	
	if (uCompilerFlags & UF_FORCE_C10_TO_F32_PRECISION)
	{
		DBGANDPDUMPOUT("HINTFORCEC10TOF32PRECISION");
	}
#endif
	
	if (uCompilerFlags & UF_VSOUTPUTSALWAYSVALID)
	{
		DBGANDPDUMPOUT("VSOUTPUTSALWAYSVALID");
	}
	
	DBGANDPDUMPOUT8("SAOFFSETS %u, %u, %u, %u, %u, %u, %u, %d",
				    psProgramParameters->uBooleanConsts,
				    psProgramParameters->uConstantBase,
				    psProgramParameters->uInRegisterConstantLimit,
					psProgramParameters->uInRegisterConstantOffset,
					psProgramParameters->uIntegerConsts,
					psProgramParameters->uScratchBase,
					0,
					psProgramParameters->uTextureState);

	DBGANDPDUMPOUT1("TEMPARRAYSABASE %u", psProgramParameters->uIndexableTempBase);
	DBGANDPDUMPOUT1("NUMTEMPS %u", psProgramParameters->uNumAvailableTemporaries);

	for (i = 0; i < psProgramParameters->uIndexableTempArrayCount; i++)
	{
		DBGANDPDUMPOUT2("TEMPARRAY %u, %u",
					    psProgramParameters->psIndexableTempArraySizes[i].uTag,
					    psProgramParameters->psIndexableTempArraySizes[i].uSize );
	}

	if ((uCompilerFlags & UF_SPLITFEEDBACK) && (psProgramParameters->eShaderType == USC_SHADERTYPE_VERTEX))
	{
		DBGANDPDUMPOUT1("POINTSIZEOUTPUT %u", psProgramParameters->uVSPointSizeOutputNum);
	}

	if (uCompilerFlags & UF_CLAMPVSCOLOUROUTPUTS)
	{
		if (psProgramParameters->uVSDiffuseOutputNum != (IMG_UINT32)-1)
		{
			DBGANDPDUMPOUT1("DIFFUSEOUTPUT %u", psProgramParameters->uVSDiffuseOutputNum);
		}
		if (psProgramParameters->uVSSpecularOutputNum != (IMG_UINT32)-1)
		{
			DBGANDPDUMPOUT1("SPECULAROUTPUT %u", psProgramParameters->uVSSpecularOutputNum);
		}
	}
	
	if (psProgramParameters->uMaxInstMovement != USC_MAXINSTMOVEMENT_NOLIMIT)
	{
		DBGANDPDUMPOUT1("MAXINSTMOVEMENT %u", psProgramParameters->uMaxInstMovement);
	}

	DBGANDPDUMPOUT1("PREDICATIONLEVEL %u", psProgramParameters->ePredicationLevel);

	if (
			(uCompilerFlags & UF_RESTRICTOPTIMIZATIONS) &&
			(psProgramParameters->uOptimizationLevel != USC_OPTIMIZATIONLEVEL_MAXIMUM)
	   )
	{
		DBGANDPDUMPOUT1("MAXOPTLEVEL %u", psProgramParameters->uOptimizationLevel);
	}

	if ((uCompilerFlags & UF_RUNTIMECORECOUNT) != 0)
	{
		DBGANDPDUMPOUT1("MPCORECOUNT %u", psProgramParameters->uMPCoreCount);
	}

	#if defined(INITIALISE_REGS_ON_FIRST_WRITE)
	if (uCompilerFlags & UF_INITREGSONFIRSTWRITE)
	{
		DBGANDPDUMPOUT("INITREGSONFIRSTWRITE");
	}
	#endif /* defined(INITIALISE_REGS_ON_FIRST_WRITE) */
	#if defined(EXECPRED)
	if (uCompilerFlags2 & UF_FLAGS2_EXECPRED)
	{
		DBGANDPDUMPOUT("EXECPRED");
	}
	#else
	PVR_UNREFERENCED_PARAMETER(uCompilerFlags2);
	#endif /* #if defined(EXECPRED) */

	if (psConstants != NULL)
	{
		for (i = 0; i < (IMG_UINT32)psConstants->uCount; i++)
		{
			if (psConstants->puConstStaticFlags[i / 32] & (1 << (i % 32)))
			{
				DBGANDPDUMPOUT3("CONSTF\tc%d.%c, 0x%X", 
							    i / 4, 
							    g_pszChannels[i % 4],
							    *(IMG_PUINT32)&psConstants->pfConst[i]);
			}
		}
	}

	if ((uCompilerFlags & UF_MULTIPLECONSTANTSBUFFERS) == 0)
	{
		PrintConstantBufferDescription(psState, psProgramParameters, uCompilerFlags, UF_CONSTBUFFERID_LEGACY);
	}
	else
	{
		for (i=0; i < UF_CONSTBUFFERID_COUNT; i++)
		{
			PrintConstantBufferDescription(psState, psProgramParameters, uCompilerFlags, i);
		}
	}

	auTexturesToDump = CallocBitArray(psState, psProgramParameters->uTextureCount);
	while( psInst != NULL )
	{
		if(indent < 0)
		{
			indent = 0;
		}

		/* Decrease the indentation if needed */
		switch(psInst->eOpCode)
		{
		case UFOP_ELSE:
		case UFOP_ENDIF:
		case UFOP_ENDLOOP:
		case UFOP_ENDREP:
		case UFOP_GLSLENDLOOP:
		case UFOP_GLSLLOOPTAIL:
		case UFOP_ENDSWITCH:
			indent--;
			break;
		case UFOP_LABEL:
			indent = 0;
			break;
		default:
			/* do nothing */
			break;
		}

		/* Print the indented instruction */
		PrintInputInst(psState, uCompilerFlags, indent, psInst, auTexturesToDump);	

		/* Update the indentation for the following intructions if needed */
		switch(psInst->eOpCode)
		{
		case UFOP_IF:
		case UFOP_IFNZBIT:
		case UFOP_ELSE:
		case UFOP_LOOP:
		case UFOP_REP:
		case UFOP_IFC:
		case UFOP_LABEL:
		case UFOP_IFP:
		case UFOP_GLSLLOOP:
		case UFOP_GLSLSTATICLOOP:
		case UFOP_GLSLLOOPTAIL:
		case UFOP_SWITCH:
			indent++;
			break;
		default:
			/* do nothing */
			break;
		}

		psInst = psInst->psILink;
	}

	for (i = 0; i < psProgramParameters->uTextureCount; i++)
	{
		static const IMG_PCHAR pszChanFormat[] = {"INV",	/* USC_CHANNELFORM_INVALID */
												  "U8",		/* USC_CHANNELFORM_U8 */
												  "S8", 	/* USC_CHANNELFORM_S8 */
												  "U16", 	/* USC_CHANNELFORM_U16 */
												  "S16", 	/* USC_CHANNELFORM_S16 */
												  "F16", 	/* USC_CHANNELFORM_F16 */
												  "F32",	/* USC_CHANNELFORM_F32 */
												  "ZERO",	/* USC_CHANNELFORM_ZERO */ 
												  "ONE",	/* USC_CHANNELFORM_ONE */ 
												  "U24",	/* USC_CHANNELFORM_U24 */ 
												  "U8UN",	/* USC_CHANNELFORM_U8UN */
												  "C10",	/* USC_CHANNELFORM_C10 */
												  "UINT8",	/* USC_CHANNELFORM_UINT8 */
												  "UINT16",	/* USC_CHANNELFORM_UINT16 */
												  "UINT32",	/* USC_CHANNELFORM_UINT32 */
												  "SINT8",	/* USC_CHANNELFORM_SINT8 */
												  "SINT16",	/* USC_CHANNELFORM_SINT16 */
												  "SINT32",			/* USC_CHANNELFORM_SINT32 */
												  "UINT16UINT10",	/* USC_CHANNELFORM_UINT16_UINT10 */
												  "UINT16UINT2",	/* USC_CHANNELFORM_UINT16_UINT10 */
												  "DF32",	/* USC_CHANNELFORM_DF32 */};
		static const IMG_PCHAR apszTrilinearType[] = 
		{
			"",					/* UNIFLEX_TEXTURE_TRILINEAR_TYPE_UNSPECIFIED */
			", TRILINEAR=ON",	/* UNIFLEX_TEXTURE_TRILINEAR_TYPE_ON */
			", TRILINEAR=OFF",	/* UNIFLEX_TEXTURE_TRILINEAR_TYPE_ON */
		};
		static const IMG_PCHAR apszDim[] = 
		{
			"1",				/* UNIFLEX_DIMENSIONALITY_TYPE_1D */
			"2",				/* UNIFLEX_DIMENSIONALITY_TYPE_2D */
			"3",				/* UNIFLEX_DIMENSIONALITY_TYPE_3D */
			"cube",				/* UNIFLEX_DIMENSIONALITY_TYPE_CUBEMAP */
		};
		IMG_CHAR pszSwiz[5];
		IMG_PCHAR pszStr;
		IMG_UINT32 uChan;
		UNIFLEX_TEXTURE_TRILINEAR_TYPE eTrilinearType;
		PUNIFLEX_TEXTURE_PARAMETERS	psTexParams = &psProgramParameters->asTextureParameters[i];
		UNIFLEX_DIMENSIONALITY_TYPE eDimensionality = psProgramParameters->asTextureDimensionality[i].eType;
		PUNIFLEX_TEXFORM psTexForm = &psTexParams->sFormat;
		IMG_BOOL bIsArray = psProgramParameters->asTextureDimensionality[i].bIsArray;

		if (GetBit(auTexturesToDump, i) == 0)
		{
			continue;
		}

		apcTempString[0] = '\0';
		pszStr = apcTempString;

		pszStr += sprintf(pszStr, "TEX\t%d", i);

		ASSERT(eDimensionality < (sizeof(apszDim) / sizeof(apszDim[0])));
		pszStr += sprintf(pszStr, ", %s", apszDim[eDimensionality]);
		if (bIsArray)
		{
			pszStr += sprintf(pszStr, "[]");
		}

		for (uChan = 0; uChan < 4; uChan++)
		{
			pszStr += sprintf(pszStr, ", %s", pszChanFormat[psTexForm->peChannelForm[uChan]]);
		}

		if (psTexForm->uSwizzle != UFREG_SWIZ_NONE)
		{
			pszStr += 
				sprintf(pszStr, ", %.4s", PVRUniFlexDecodeInputSwizzle(IMG_FALSE, psTexForm->uSwizzle, pszSwiz));
		}

		if (psTexForm->bTwoResult)
		{
			pszStr += sprintf(pszStr, ", TWO");
		}

		if (psTexForm->bMinPack)
		{
			pszStr += sprintf(pszStr, ", MINP");
		}

		if (psTexParams->bGamma)
		{
			pszStr += sprintf(pszStr, ", GAMMA");
		}

		if (psTexForm->bUsePCF)
		{
			pszStr += sprintf(pszStr, ", PCF");
		}

		if (psTexForm->uChunkSize != 4)
		{
			pszStr += sprintf(pszStr, ", C%d", psTexForm->uChunkSize);
		}

		if (psTexForm->uCSCConst != (IMG_UINT32)-1)
		{
			pszStr += sprintf(pszStr, ", YUV%u", psTexForm->uCSCConst);
		}

		if (psTexForm->bTFSwapRedAndBlue)
		{
			pszStr += sprintf(pszStr, ", SWAPF16");
		}
			
		if (psTexParams->bMipMap)
		{
			pszStr += sprintf(pszStr, ", M");
		}

		pszStr = PrintTextureFilterType(psState, pszStr, ", MAGFILTER", psTexParams->eMagFilter);

		pszStr = PrintTextureFilterType(psState, pszStr, ", MINFILTER", psTexParams->eMinFilter);

		eTrilinearType = psTexParams->eTrilinearType;
		ASSERT(eTrilinearType < (sizeof(apszTrilinearType) / sizeof(apszTrilinearType[0])));
		pszStr += sprintf(pszStr, "%s", apszTrilinearType[eTrilinearType]);

		if (psTexParams->ePCFComparisonMode != UFREG_COMPOP_INVALID)
		{
			pszStr += sprintf(pszStr, ", PCFCOMP=");
			InternalDecodeComparisonOperator(psTexParams->ePCFComparisonMode, pszStr);
		}

		DBGANDPDUMPOUT(apcTempString);
	}
	UscFree(psState, auTexturesToDump);

	DBGANDPDUMPOUT(pszEndLabel);
#endif //(UF_TESTBENCH) || (DEBUG) || (PDUMP) 


#ifdef USER_PERFORMANCE_STATS_ONLY
	if(szfilename) {
	    fclose(uniflexfile);
	    uniflexfile = NULL;
	}
#endif

}
#ifdef SRC_DEBUG
/*****************************************************************************
 FUNCTION	: PrintInputInstCost

 PURPOSE	: Print an input instruction.

 PARAMETERS	: psState			- The current compiler state
              i32Indent         - Indentation level
			  psInst			- Instruction to print.
			  pdwTextureToDumps	- Variable which is updated which a bitmask
								  of the textures referenced by the instruction.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID PrintInputInstCost(PINTERMEDIATE_STATE psState,
							   IMG_INT32 i32Indent, 
							   PUNIFLEX_INST psInst, 
							   IMG_PUINT32 puTexturesToDump)
{
	char apcTempString[532] = {'0'};

	InternalDecodeInputInst(psState->uCompilerFlags, psInst, i32Indent, apcTempString, puTexturesToDump);
	sprintf(apcTempString + strlen(apcTempString),"		;C[%u]", psState->puSrcLineCost[psInst->uSrcLine]);
	DBGANDPDUMPOUT(apcTempString);
}


IMG_INTERNAL 
IMG_VOID PrintSourceLineCost(PINTERMEDIATE_STATE		psState,
					PUNIFLEX_INST			psProg)
/*****************************************************************************
 FUNCTION	: PrintSourceLineCost

 PURPOSE	: Print a list of input instructions and program parameters

 PARAMETERS	: psProg					- Head of the input instruction list.
			  puTextureDimensions		- Dimensions of the textures used in the program.
			  psConstants				- Local constants used in the program.

 RETURNS	: Nothing.
*****************************************************************************/
{
#if defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) 
	PUNIFLEX_INST psInst;
	IMG_INT32 indent = 1;
	IMG_UINT32 i;
	IMG_CHAR szTotalCost[64];

	psInst = psProg;
	DBGANDPDUMPOUT(";------ Source Line Cost -------\r\n"); 
	while( psInst != NULL )
	{
		if(indent < 0)
		{
			indent = 0;
		}

		/* Decrease the indentation if needed */
		switch(psInst->eOpCode)
		{
		case UFOP_ELSE:
		case UFOP_ENDIF:
		case UFOP_ENDLOOP:
		case UFOP_ENDREP:
		case UFOP_GLSLENDLOOP:
		case UFOP_GLSLLOOPTAIL:
		case UFOP_ENDSWITCH:
		case UFOP_JUMP:
			indent--;
			break;
		default:
			/* do nothing */
			break;
		}

		/* Print the indented instruction */
		PrintInputInstCost(psState, indent, psInst, NULL);

		/* Update the indentation for the following intructions if needed */
		switch(psInst->eOpCode)
		{
		case UFOP_IF:
		case UFOP_IFNZBIT:
		case UFOP_ELSE:
		case UFOP_LOOP:
		case UFOP_REP:
		case UFOP_IFC:
 		case UFOP_LABEL:
		case UFOP_IFP:
		case UFOP_GLSLLOOP:
		case UFOP_GLSLSTATICLOOP:
		case UFOP_GLSLLOOPTAIL:
		case UFOP_SWITCH:
			indent++;
			break;
		case UFOP_BLOCK:
			{
				if (indent == 0)
					indent++;
				break;
			}
		default:
			/* do nothing */
			break;
		}

		psInst = psInst->psILink;
	}
	psState->uTotalCost = 0;
	for(i=0;i<((psState->uTotalLines)+1);i++)
	{
		if((psState->puSrcLineCost[i])>0)
		(psState->uTotalCost) += (psState->puSrcLineCost[i]);
	}
	sprintf(szTotalCost, ";------ Total Measured Cost = %u -------\r\n", (psState->uTotalCost));
	DBGANDPDUMPOUT(szTotalCost);
	sprintf(szTotalCost, ";------ Total Hardware Instructions = %u -------\r\n", (psState->uMainProgInstCount));
	DBGANDPDUMPOUT(szTotalCost);
	sprintf(szTotalCost, ";------ USC Generated Instructions = %u -------\r\n", psState->puSrcLineCost[(psState->uTotalLines)]);
	DBGANDPDUMPOUT(szTotalCost);
	DBGANDPDUMPOUT(" ");
#endif //defined(UF_TESTBENCH) || defined(DEBUG) || defined(PDUMP) 
}
#endif /* SRC_DEBUG */

#if defined(SUPPORT_ICODE_SERIALIZATION)
/*
	Structures for serializing Intermediate Code
*/
typedef struct _STRUCT_PACKET_
{
	IMG_PVOID	pvOldAddress;
	IMG_PVOID	pvNewAddress;
	IMG_UINT32	uSize;
#if !defined(ICODE_BIN_FORMAT_DUMP)
#define PACKET_REF_NAME_LENGTH		64
	IMG_CHAR	aszRefName[PACKET_REF_NAME_LENGTH];
#endif
	/* + Rest of the data +*/
} STRUCT_PACKET, *PSTRUCT_PACKET;

typedef struct _TRANS_ENTRY_
{
	IMG_PVOID		pvOldAddress;
	IMG_PVOID		pvNewAddress;
	IMG_UINT32		uSize;
	PSTRUCT_PACKET	psEntryPacket;
}TRANS_ENTRY, *PTRANS_ENTRY;

typedef struct _TRANS_STATE_
{
	PINTERMEDIATE_STATE		psState;
	IMG_PVOID				pvData;
	PTRANS_ENTRY			psTransTable;
	IMG_UINT32				uTransEntries;
	IMG_UINT32				uTransEntriesAllocated;
	IMG_BOOL				bStore;
	FILE 					*pFile;
}TRANS_STATE, *PTRANS_STATE;

#if !defined(ICODE_BIN_FORMAT_DUMP)
	#define ICODE_COMMENT(x)					{if(bStore){ fprintf(pFile, "@[%04d] " x "\n", __LINE__ ); }}
	#define ICODE_COMMENT_FMT1(fmt, x)			{if(bStore){ fprintf(pFile, "@" fmt "\n", x); }}
	#define ICODE_COMMENT_FMT2(fmt, x, y)		{if(bStore){ fprintf(pFile, "@" fmt "\n", x, y); }}
#else
	#define ICODE_COMMENT(x)
	#define ICODE_COMMENT_FMT1(fmt, x)			PVR_UNREFERENCED_PARAMETER(x)
	#define ICODE_COMMENT_FMT2(fmt, x, y)		PVR_UNREFERENCED_PARAMETER(x);PVR_UNREFERENCED_PARAMETER(y)
#endif

#define LOAD_STORE_STRUCT(struct_addr, struct_size)									\
	if(bStore)																		\
	{																				\
		ICODE_COMMENT_FMT2("<%p>%s", struct_addr, #struct_addr); 					\
		WriteStructure(psState, psTransState, pFile, struct_addr, struct_size);		\
	}																				\
	else																			\
	{ 																				\
		ASSERT(psTransState);														\
		psPacket = ReadStructure(psState, psTransState, pFile, (IMG_PVOID*)&(struct_addr), struct_size); 		\
		/*struct_addr = psPacket->pvNewAddress;*/ 										\
	}
	
#define TRANS_ASSERT(x, y) 															\
	if(bStore) 																		\
	{ 																				\
		ASSERT(x == y); 															\
	} 																				\
	else 																			\
	{ 																				\
		x = y; 																		\
	}
	
#define TRANSLATE_ADDRESS(x) x = TranslateAddress(psState, psTransState, x, IMG_NULL, IMG_TRUE, IMG_FALSE)

#define TRANSLATE_LIST_ENTRY_ADDRESS(addr_to_trans, container_struct, list_entry_member)		\
		if(addr_to_trans)																		\
		{																						\
			container_struct psTemp = TranslateAddress( psState, psTransState, 					\
					IMG_CONTAINING_RECORD(addr_to_trans, container_struct, list_entry_member), IMG_NULL, IMG_TRUE, IMG_FALSE);	\
			ASSERT(psTemp);																		\
			addr_to_trans = &psTemp->list_entry_member;											\
		}
		
#define LOAD_STORE_EACH_ENTRY_START(list_ptr, struct_type, element)								\
		ICODE_COMMENT(#list_ptr)																\
		{																						\
			PUSC_LIST psList = list_ptr;														\
			PUSC_LIST_ENTRY psListEntry, psPrevListEntry = IMG_NULL;							\
			for (psListEntry = list_ptr->psHead;												\
				psListEntry != NULL;															\
				psListEntry = psListEntry->psNext)												\
			{																					\
				struct_type psElement = IMG_CONTAINING_RECORD(psListEntry, struct_type, element);


#define LOAD_STORE_EACH_ENTRY_END(element)														\
				ASSERT(psElement);																\
				TRANS_ASSERT(psListEntry, &psElement->element);									\
				if(psPrevListEntry)																\
				{																				\
					TRANS_ASSERT(psPrevListEntry->psNext, psListEntry);							\
					TRANS_ASSERT(psListEntry->psPrev, psPrevListEntry);							\
				}																				\
				else																			\
				{																				\
					TRANS_ASSERT(psList->psHead, psListEntry);									\
				}																				\
				psPrevListEntry = psListEntry;													\
			}																					\
			TRANS_ASSERT(psList->psTail, psPrevListEntry);										\
		}
		
#define LOAD_STORE_EACH_ARRAY_ELEMENT_START(array, element_type, count)							\
	{																							\
		IMG_UINT32 i;																			\
		for(i = 0; i < count; i++)																\
		{																						\
			element_type psElement;																\
			psElement = (PVREGISTER)ArrayGet(psState, array, i);
			
#define LOAD_STORE_EACH_ARRAY_ELEMENT_END()														\
		}																						\
	}

static 
PTRANS_STATE NewTranslationState(PINTERMEDIATE_STATE psState, FILE * pFile, IMG_BOOL bStore)
/*****************************************************************************
 FUNCTION	: NewTranslationState

 PURPOSE	: Allocates new translation table state

 PARAMETERS	: psState	    - Shader compiler state.
			  psBloc        - Block to serialize
			  pFile         - File Handle

 RETURNS	: None.
*****************************************************************************/
{
	PTRANS_STATE psTransState = UscAlloc(psState, sizeof(TRANS_STATE));
	memset(psTransState, 0, sizeof(TRANS_STATE));
	psTransState->psState = psState;
	psTransState->pFile = pFile;
	psTransState->bStore = bStore;
	return psTransState;
}

static
IMG_VOID FreeTranslationState(PINTERMEDIATE_STATE psState, PTRANS_STATE *ppsTransState)
/*****************************************************************************
 FUNCTION	: FreeTranslationState

 PURPOSE	: Free up the translation table

 PARAMETERS	: psState	    - Shader compiler state.
			  psBloc        - Block to serialize
			  pFile         - File Handle

 RETURNS	: None.
*****************************************************************************/
{
	PTRANS_STATE psTransState = *ppsTransState;
	if(psTransState->uTransEntriesAllocated)
	{
		UscFree(psState, psTransState->psTransTable);
	}
	UscFree(psState, psTransState);
	UscFree(psState, psTransState);
}

static 
IMG_PVOID TranslateAddress(PINTERMEDIATE_STATE psState, PTRANS_STATE psTransState, IMG_PVOID pvOldAddress, 
						   PTRANS_ENTRY *ppsTransEntry, IMG_BOOL bAbortInvalid, IMG_BOOL bCheckAlreadyTranslated)
/*****************************************************************************
 FUNCTION	: TranslateAddress

 PURPOSE	: Translates an old address to newly allocated equivalent structure
 			   address.

 PARAMETERS	: psState	    - Shader compiler state.
			  psTransState	- Trans State
			  pvOldAddress	- Address read from bin file.

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32 i;
	
	ASSERT(psTransState);
	
	if(pvOldAddress == IMG_NULL)
	{
		return IMG_NULL;
	}
	
	for(i = 0; i < psTransState->uTransEntries; i++)
	{
		IMG_PVOID pvAddressCompare;
		if(bCheckAlreadyTranslated)
		{
			pvAddressCompare = psTransState->psTransTable[i].pvNewAddress;
		}
		else
		{
			pvAddressCompare = psTransState->psTransTable[i].pvOldAddress;
		}
		if(pvAddressCompare == pvOldAddress)
		{
			if(ppsTransEntry)
			{
				*ppsTransEntry = &psTransState->psTransTable[i];
			}
			return psTransState->psTransTable[i].pvNewAddress;
		}
		else
		{
			IMG_UINT32 uBase, uOffset, uLocation, uSize;
			uBase		= (IMG_UINT32)(pvAddressCompare);
			uLocation	= (IMG_UINT32)pvOldAddress;
			uSize		= psTransState->psTransTable[i].uSize;
			
			if(uBase < uLocation && uLocation < (uBase + uSize))
			{
				/* Address lies inside allocation */
				uOffset = uLocation - uBase;
				uBase = (IMG_UINT32)psTransState->psTransTable[i].pvNewAddress;
				if(ppsTransEntry)
				{
					*ppsTransEntry = &psTransState->psTransTable[i];
				}
				return (IMG_PVOID)(uBase + uOffset);
			}
		}
	}
	if(bAbortInvalid && (!bCheckAlreadyTranslated))
	{
		/* Check are we tranlating an already translated adddress, debugging help */
		for(i = 0; i < psTransState->uTransEntries; i++)
		{
			if(psTransState->psTransTable[i].pvNewAddress == pvOldAddress)
			{
				DBG_PRINTF((DBG_WARNING, "Address already translated %p %p", psTransState->psTransTable[i].pvOldAddress, psTransState->psTransTable[i].pvNewAddress));
				imgabort();
			}
		}
		imgabort();
	}
	if(ppsTransEntry)
	{
		*ppsTransEntry = IMG_NULL;
	}
	return IMG_NULL;
}

static
IMG_VOID TransEntryAppend(PINTERMEDIATE_STATE psState, PTRANS_STATE psTransState, IMG_PVOID pvOldAddress, 
						  IMG_PVOID pvNewAddress, PSTRUCT_PACKET psPacket, IMG_UINT32 uSize)
/*****************************************************************************
 FUNCTION	: TranslateAddress

 PURPOSE	: Appends newly allocated structure address to tranlation list

 PARAMETERS	: psState	    - Shader compiler state.
			  psTransState	- Trans State
			  pvOldAddress	- Address read from bin file.
			  pvNewAddress	- Address of equivalent structure reallocated.

 RETURNS	: None.
*****************************************************************************/
{
	ASSERT(pvOldAddress);
	ASSERT(!TranslateAddress(psState, psTransState, pvOldAddress, IMG_NULL, IMG_FALSE, IMG_FALSE));
	if(psTransState->uTransEntriesAllocated == psTransState->uTransEntries)
	{
		// Reallocate
		ResizeTypedArray(psState, psTransState->psTransTable, psTransState->uTransEntriesAllocated, 
						 psTransState->uTransEntriesAllocated + 128);
		psTransState->uTransEntriesAllocated = psTransState->uTransEntriesAllocated + 128;
	}
	psTransState->psTransTable[psTransState->uTransEntries].pvOldAddress = pvOldAddress;
	psTransState->psTransTable[psTransState->uTransEntries].pvNewAddress = pvNewAddress;
	psTransState->psTransTable[psTransState->uTransEntries].uSize = uSize;
	psTransState->psTransTable[psTransState->uTransEntries].psEntryPacket = psPacket;
	psTransState->uTransEntries++;
}

#if defined(DEBUG_ICODE_SERIALIZATION)
static 
IMG_VOID TranslationCheck(PINTERMEDIATE_STATE psState, PTRANS_STATE psTransState)
/*****************************************************************************
 FUNCTION	: TranslationCheck

 PURPOSE	: Fuzzy check try assuming each word as address and see if it is one 
 			  of already loaded structure.

 PARAMETERS	: psState	    - Shader compiler state.
			  psTransState	- Trans State
			  pvOldAddress	- Address read from bin file.
			  pvNewAddress	- Address of equivalent structure reallocated.

 RETURNS	: None.
*****************************************************************************/
{
	PTRANS_ENTRY psEntry;
	IMG_UINT32 i, j;

	for(i = 1; i < psTransState->uTransEntries; i++)
	{
		IMG_UINT32 **ppuRunner = (IMG_UINT32 **)psTransState->psTransTable[i].pvNewAddress;
		IMG_UINT32 uTotalAddr = psTransState->psTransTable[i].uSize / sizeof(IMG_PVOID);
		
		j = 0;
		while(uTotalAddr)
		{
			IMG_PVOID pvRetAddr, pvAddr = *(ppuRunner + j);
			pvRetAddr = TranslateAddress(psState, psTransState, pvAddr, &psEntry, IMG_FALSE, IMG_FALSE);
			if(pvRetAddr)
			{
				DBG_PRINTF((DBG_WARNING, "i@%d pvAddr:%p|%p (%p) of %p (%p) not translated", 
							j * sizeof(IMG_PVOID), 
							pvAddr, 
							pvRetAddr, 
							(ppuRunner + j), 
							psTransState->psTransTable[i].pvOldAddress,
						    psTransState->psTransTable[i].pvNewAddress));
				imgabort();
			}
			uTotalAddr--;
			j++;
		}
	}
}
#endif /* defined(DEBUG_ICODE_SERIALIZATION) */

#if defined(ICODE_BIN_FORMAT_DUMP)
static
IMG_VOID WriteStructure(PINTERMEDIATE_STATE psState, PTRANS_STATE psTransState, FILE *pFile, IMG_PVOID psStruct, IMG_UINT32 uSize)
#else
#define WriteStructure(a, b, c, d, e) __WriteStructure(a, b, c, d, e, #e)
static
IMG_VOID __WriteStructure(PINTERMEDIATE_STATE psState, PTRANS_STATE psTransState, FILE *pFile, IMG_PVOID psStruct, IMG_UINT32 uSize, const IMG_CHAR *pszRefName)
#endif
/*****************************************************************************
 FUNCTION	: WriteStructure

 PURPOSE	: Writes a structure to file with additional size information,
 			  old memeory location.

 PARAMETERS	: psState	    - Shader compiler state.
			  pFile         - File to write into
			  psStruct      - Structure to serialize
			  uSize         - Size of structure

 RETURNS	: None.
*****************************************************************************/
{
	PSTRUCT_PACKET psPacket;
	PTRANS_ENTRY psEntry;
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	ASSERT(psStruct);
	ASSERT(uSize);
	
	TranslateAddress(psState, psTransState, psStruct, &psEntry, IMG_FALSE, IMG_FALSE);
	if(psEntry)
	{
		/* Address already serialized */
		#if !defined(ICODE_BIN_FORMAT_DUMP)
		fprintf(pFile, "@ <REPEAT>\n");
		#endif
		return;
	}
	
	psPacket = UscAlloc(psState, sizeof(STRUCT_PACKET) + uSize);
	memset(psPacket, 0xBD, sizeof(STRUCT_PACKET) + uSize);
	psPacket->pvOldAddress = psStruct;
	psPacket->pvNewAddress = (IMG_PVOID)0xBAD0BAD0;
	psPacket->uSize = uSize;
	
	/*
		Not implemented for non 4-byte aligned structures, (may needs padding) 
	*/
	ASSERT(!(uSize & 0x3));
	
#if defined(ICODE_BIN_FORMAT_DUMP)
	fwrite(psPacket, sizeof(STRUCT_PACKET), 1, pFile);
	fwrite(psStruct, uSize, 1, pFile);
#else
	memset(psPacket->aszRefName, ' ', PACKET_REF_NAME_LENGTH-1);
	
	strncpy(psPacket->aszRefName, pszRefName, 
			((strlen(pszRefName) < (PACKET_REF_NAME_LENGTH))? strlen(pszRefName)+1 : PACKET_REF_NAME_LENGTH) - 1);
	
	psPacket->aszRefName[PACKET_REF_NAME_LENGTH-1] = '\0';
	
	fprintf(pFile, "### %s:\n%08x %08x %08x", psPacket->aszRefName, 
			(unsigned int)psPacket->pvOldAddress, (unsigned int)psPacket->pvNewAddress, psPacket->uSize);
	{
		IMG_UINT32 *puiData = (IMG_UINT32*)psStruct;
		IMG_UINT32 ui32Cnt = 0;
		IMG_UINT32 ui32ByteSize = (uSize & ~3);
		
		while(ui32ByteSize)
		{
			if(ui32Cnt%8 == 0)
				fprintf(pFile, "\n %08x: ", (IMG_UINT32)puiData);
			else
			{
				if(ui32Cnt%4 == 0)
					fprintf(pFile, "  ");
			}
			fprintf(pFile, "%08X ", *puiData);
			puiData++;
			ui32ByteSize-= sizeof(unsigned int);
			ui32Cnt++;
		}
		fprintf(pFile, "\n");
	}
#endif
	if(psTransState)
	{
		/*
			Remember the written entry shouldn't be written again in future
		*/
		TransEntryAppend(psState, psTransState, psStruct, (IMG_PVOID)0xBAD0BAD0, IMG_NULL, uSize);
	}
	UscFree(psState, psPacket);
}

static 
IMG_UINT32 GetInstructionTypeParamSize(PINST psInst)
/*****************************************************************************
 FUNCTION	: GetInstructionTypeParamSize

 PURPOSE	: Returns required size for instruction parameters

 PARAMETERS	: psInst 	- Instruction to work upon.

 RETURNS	: None.
*****************************************************************************/
{
	switch(g_psInstDesc[psInst->eOpcode].eType)
	{
		case INST_TYPE_NONE:		return 0;
		case INST_TYPE_FLOAT:		return (sizeof(FLOAT_PARAMS)); 
		case INST_TYPE_EFO:			return (sizeof(EFO_PARAMETERS)); 
		case INST_TYPE_SOPWM:		return (sizeof(SOPWM_PARAMS)); 
		case INST_TYPE_SOP2:		return (sizeof(SOP2_PARAMS)); 
		case INST_TYPE_SOP3:		return (sizeof(SOP3_PARAMS)); 
		case INST_TYPE_LRP1:		return (sizeof(LRP1_PARAMS)); 
		case INST_TYPE_FPMA:		return (sizeof(FPMA_PARAMS)); 
		case INST_TYPE_IMA32:		return (sizeof(IMA32_PARAMS)); 
		case INST_TYPE_SMP:			return (sizeof(SMP_PARAMS)); 
#if defined(OUTPUT_USPBIN)
		case INST_TYPE_SMPUNPACK:	return (sizeof(SMPUNPACK_PARAMS));
#endif
		case INST_TYPE_DOT34:		return (sizeof(DOT34_PARAMS));
		case INST_TYPE_DPC:			return (sizeof(DPC_PARAMS));
		case INST_TYPE_LDST:		return (sizeof(*psInst->u.psLdSt));
#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
		case INST_TYPE_NRM:			return (sizeof(NRM_PARAMS));
#endif
		case INST_TYPE_LOADCONST:	return (sizeof(LOADCONST_PARAMS));
		case INST_TYPE_LOADMEMCONST:return (sizeof(LOADMEMCONST_PARAMS));
		case INST_TYPE_IMA16:		return (sizeof(*psInst->u.psIma16));
		case INST_TYPE_IMAE:		return (sizeof(IMAE_PARAMS));
		case INST_TYPE_CALL:		return (sizeof(CALL_PARAMS));
#if defined(OUTPUT_USPBIN)
		case INST_TYPE_TEXWRITE:	return (sizeof(TEXWRITE_PARAMS));
#endif
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case INST_TYPE_VEC:			return (sizeof(*psInst->u.psVec));
#endif
#if defined(SUPPORT_SGX545)
		case INST_TYPE_DUAL:		return (sizeof(*psInst->u.psDual));
#endif
		case INST_TYPE_EMIT:		return 0;
		case INST_TYPE_TEST:		return (sizeof(*psInst->u.psTest));
		case INST_TYPE_MOVC:		return (sizeof(*psInst->u.psMovc));
		case INST_TYPE_FARITH16:	return (sizeof(*psInst->u.psArith16));
		case INST_TYPE_MEMSPILL:	return (sizeof(*psInst->u.psMemSpill));
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case INST_TYPE_IDXSC:		return (sizeof(*psInst->u.psIdxsc));
#endif
		case INST_TYPE_CVTFLT2INT:	return (sizeof(*psInst->u.psCvtFlt2Int));
		case INST_TYPE_PCK:			return (sizeof(*psInst->u.psPck));
		case INST_TYPE_LDSTARR:		return (sizeof(*psInst->u.psLdStArray));
		case INST_TYPE_FDOTPRODUCT:	return (sizeof(*psInst->u.psFdp));
		case INST_TYPE_DELTA:		return (sizeof(*psInst->u.psDelta));
		case INST_TYPE_MOVP:		return (sizeof(*psInst->u.psMovp));
		case INST_TYPE_FEEDBACKDRIVEREPILOG:
									return (sizeof(*psInst->u.psFeedbackDriverEpilog));
		case INST_TYPE_BITWISE:
									return (sizeof(*psInst->u.psBitwise));
		case INST_TYPE_UNDEF:
									return 0;
		default:
									return 0;
	}
}

static
PSTRUCT_PACKET ReadStructure(PINTERMEDIATE_STATE psState, PTRANS_STATE psTransState, FILE *pFile, IMG_PVOID *ppvNewAddress, IMG_UINT32 uSize)
/*****************************************************************************
 FUNCTION	: ReadStructure

 PURPOSE	: Reads a already serialized structure from file 

 PARAMETERS	: psState	    - Shader compiler state.
			  pFile         - File to read from
			  uSize         - Size of structure

 RETURNS	: None.
*****************************************************************************/
{
	PSTRUCT_PACKET	psPacket;
	PTRANS_ENTRY	psEntry;
	IMG_PVOID		*pvData;
	IMG_PVOID		pvAllocStruct;
	IMG_UINT32		uDummy;
	IMG_PVOID		pvOldAddress;
	IMG_PVOID		pvNewAddress;
	
	ASSERT(!(uSize & 0x3));	/* NOT Handling 4 byte unaligned structures */
	ASSERT(*ppvNewAddress);
	
	pvOldAddress = *ppvNewAddress;
	pvNewAddress = TranslateAddress(psState, psTransState, pvOldAddress, &psEntry, IMG_FALSE, IMG_FALSE);
	if(psEntry)
	{
		/* Structure already loaded */
		*ppvNewAddress = pvNewAddress;
		return psEntry->psEntryPacket;
	}
	
	pvNewAddress = TranslateAddress(psState, psTransState, pvOldAddress, &psEntry, IMG_FALSE, IMG_TRUE);
	if(psEntry)
	{
		/* Structure is loaded and translated*/
		DBG_PRINTF((DBG_WARNING, "Structures %p alread translated (%p)", psEntry->pvOldAddress, psEntry->pvNewAddress));
		imgabort();
	}
	
	psPacket = UscAlloc(psState, sizeof(STRUCT_PACKET) + uSize);
	memset(psPacket, 0xBD, sizeof(STRUCT_PACKET) + uSize);
	
	/*
		Creating another copy of allocation as UscFree will disfunction when we passed 
		offseted memory to it.
	*/
	pvAllocStruct = UscAlloc(psState, uSize);
	pvData = (IMG_PVOID)(((IMG_INT8*)psPacket) + sizeof(STRUCT_PACKET));
	
#if defined(ICODE_BIN_FORMAT_DUMP)
	fread(psPacket, (sizeof(STRUCT_PACKET) + uSize), 1, pFile);
	
	ASSERT(psPacket->uSize);
	ASSERT(psPacket->uSize == uSize);
#else
	
	memset(psPacket->aszRefName, ' ', PACKET_REF_NAME_LENGTH-1);
	
	/* Skip comment line */
	while(fgetc(pFile) == '@') while(fgetc(pFile) != '\n');
	
	fscanf(pFile, "## %s", psPacket->aszRefName);
	while(fgetc(pFile) != '\n');
	
	fscanf(pFile, "%x %x %x\n", 
			(IMG_PUINT32)&psPacket->pvOldAddress, &uDummy, &psPacket->uSize);
	
	ASSERT(psPacket->uSize);
	ASSERT(psPacket->uSize == uSize);

	{
		IMG_UINT32 *puiData = (IMG_UINT32*)pvData;
		IMG_UINT32 ui32Cnt = 0;
		IMG_UINT32 ui32ByteSize = (uSize & ~3);
		
		while(ui32ByteSize)
		{
			if(ui32Cnt%8 == 0)
			{
				fscanf(pFile, "\n %x: ", puiData);
			}
			else
			{
				if(ui32Cnt%4 == 0)
				{
					fscanf(pFile, "  ");
				}
			}
			fscanf(pFile, "%X ", puiData);
			puiData++;
			ui32ByteSize-= sizeof(unsigned int);
			ui32Cnt++;
		}
		fscanf(pFile, "\n");
	}
#endif
	
	memcpy(pvAllocStruct, pvData, uSize);
	psPacket->pvNewAddress = pvAllocStruct;
	*ppvNewAddress = pvAllocStruct;
	if(psTransState)
	{
		TransEntryAppend(psState, psTransState, psPacket->pvOldAddress, psPacket->pvNewAddress, psPacket, uSize);
	}
	return psPacket;
}

static
IMG_VOID SerializeOrUnSerializeVectorChunks(PINTERMEDIATE_STATE psState, PTRANS_STATE psTransState, FILE *pFile, 
											 USC_PVECTOR psVector, IMG_BOOL bStore)
/*****************************************************************************
 FUNCTION	: SerializeOrUnSerializeVectorChunks

 PURPOSE	: Serialize or Unserialize a USC_VECT structure chunks

 PARAMETERS	: psState	    - Shader compiler state.
			  psTransState	- Trans state
			  pFile         - File Handle

 RETURNS	: None.
*****************************************************************************/
{
	PSTRUCT_PACKET psPacket;
	USC_PCHUNK psCurr, psPrev = IMG_NULL;
	
	if (psVector == NULL)
		return;

	psCurr = psVector->psFirst;
	while(psCurr)
	{
		LOAD_STORE_STRUCT(psCurr, sizeof(USC_CHUNK));
		LOAD_STORE_STRUCT(psCurr->pvArray, (psVector->uChunk * sizeof(IMG_UINT32)));
		
		if(psPrev)
		{
			TRANS_ASSERT(psPrev->psNext, psCurr);
		}
		else
		{
			TRANS_ASSERT(psVector->psFirst, psCurr);
		}
		TRANS_ASSERT(psCurr->psPrev, psPrev);
		psPrev = psCurr;
		
		psCurr = psCurr->psNext;
	}
	
	if(!bStore)
	{
		psVector->sMemo.pvData = NULL;
	}
}

static
IMG_VOID SerializeOrUnSerializeArray(PINTERMEDIATE_STATE psState, PTRANS_STATE psTransState, FILE *pFile, 
									 USC_PARRAY *ppsArray, IMG_BOOL bStore)
/*****************************************************************************
 FUNCTION	: SerializeOrUnSerializeArray

 PURPOSE	: Serialize or Unserialize a USC_ARRAY structure 

 PARAMETERS	: psState	    - Shader compiler state.
			  psTransState	- Trans state
			  pFile         - File Handle

 RETURNS	: None.
*****************************************************************************/
{
	PSTRUCT_PACKET psPacket;
	USC_PCHUNK psCurr, psPrev = IMG_NULL;
	USC_PARRAY psArray = *ppsArray;
	
	if (psArray == NULL)
	{
		return;
	}
	
	LOAD_STORE_STRUCT(psArray, sizeof(USC_ARRAY));
	*ppsArray = psArray;
	psCurr = psArray->psFirst;
	while(psCurr != NULL)
	{
		LOAD_STORE_STRUCT(psCurr, sizeof(USC_CHUNK));
		LOAD_STORE_STRUCT(psCurr->pvArray, psArray->uChunk * psArray->uSize);
		
		if(psPrev)
		{
			TRANS_ASSERT(psPrev->psNext, psCurr);
		}
		else
		{
			TRANS_ASSERT(psArray->psFirst, psCurr);
		}
		TRANS_ASSERT(psCurr->psPrev, psPrev);
		psPrev = psCurr;
		
		psCurr = psCurr->psNext;
	}
	
	if(!bStore)
	{
		psArray->sMemo.pvData = NULL;
	}
}

static
IMG_VOID SerializeOrUnSerializeTreeNodes(IMG_VOID* pvTransState, IMG_PVOID pvElement, IMG_UINT32 uSize)
{
	IMG_PVOID *ppvElement = (IMG_PVOID*)pvElement;
	PSTRUCT_PACKET psPacket;
	PTRANS_STATE psTransState = (PTRANS_STATE)pvTransState;
	PINTERMEDIATE_STATE psState = psTransState->psState;
	IMG_BOOL bStore = psTransState->bStore;
	FILE *pFile = psTransState->pFile;
	
	if((*ppvElement))
	{
		LOAD_STORE_STRUCT((*ppvElement), uSize);
	}
}

static
IMG_VOID TranslateRegGroupNode(IMG_VOID* pvTransState, IMG_VOID *pvElement)
{
	IMG_PVOID *ppvElement = (IMG_PVOID*)pvElement;
	IMG_PVOID pvElementPtr;
	PTRANS_STATE psTransState = (PTRANS_STATE)pvTransState;
	PINTERMEDIATE_STATE psState = psTransState->psState;
	
	if(psTransState->bStore)
		return;
		
	ppvElement++;
	pvElementPtr = (*ppvElement);
	TRANSLATE_ADDRESS(pvElementPtr);
	(*ppvElement) = pvElementPtr;
}

typedef IMG_VOID (*PFN_TRANSLATE_TREE_NODE_KEY)(IMG_VOID *pvTransState, IMG_PVOID pvElement);

static
IMG_VOID SerializeOrUnSerializeTree(PINTERMEDIATE_STATE psState, PTRANS_STATE psTransState, 
									PUSC_TREE *ppsTree, PFN_TRANSLATE_TREE_NODE_KEY pfnTranslateKey)
/*****************************************************************************
 FUNCTION	: SerializeOrUnSerializeTree

 PURPOSE	: Serialize or unserialize argument

 PARAMETERS	: psState	    - Shader compiler state.
			  psBloc        - Block to serialize
			  pFile         - File Handle

 RETURNS	: None.
*****************************************************************************/
{
	if(*ppsTree)
	{
		UscTreePostOrderTraverseRecursive(psState, ppsTree, SerializeOrUnSerializeTreeNodes, pfnTranslateKey, psTransState);
	}
}

static
IMG_VOID LoadOrStoreArg(PINTERMEDIATE_STATE psState, PTRANS_STATE psTransState, PARG *ppArg, 
								IMG_UINT32 uArgCount, IMG_VOID *pvFileHandle, IMG_BOOL bStore)
/*****************************************************************************
 FUNCTION	: LoadOrStoreArg

 PURPOSE	: Serialize or unserialize argument

 PARAMETERS	: psState	    - Shader compiler state.
			  psBloc        - Block to serialize
			  pFile         - File Handle

 RETURNS	: None.
*****************************************************************************/
{
	PSTRUCT_PACKET psPacket;
	FILE *pFile = (FILE*) pvFileHandle;
	
	PARG psArg;
	IMG_UINT32 i;
	
	if(!uArgCount)
		return;
	
	LOAD_STORE_STRUCT((*ppArg), sizeof(ARG) * uArgCount);
	psArg = (*ppArg);
	for(i = 0; i < uArgCount; i++, psArg++)
	{
		ASSERT(psArg);
		ASSERT(psArg->uType < USC_REGTYPE_MAXIMUM);
		
		if(psArg->psRegister && (!bStore))
		{
			TRANSLATE_ADDRESS(psArg->psRegister);
		}
		if(psArg->psIndexRegister && (!bStore))
		{
			TRANSLATE_ADDRESS(psArg->psIndexRegister);
		}
	}
}

static
IMG_VOID LoadStoreUseDefChain(PINTERMEDIATE_STATE psState, PTRANS_STATE psTransState, PUSEDEF_CHAIN *ppsUseDef,
							   IMG_VOID *pvFileHandle, IMG_BOOL bStore)
/*****************************************************************************
 FUNCTION	: LoadStoreUseDefChain

 PURPOSE	: Writes a function with all of its intermediate code block to file

 PARAMETERS	: psState	    - Shader compiler state.
			  psBloc        - Block to serialize
			  pFile         - File Handle

 RETURNS	: None.
*****************************************************************************/
{
	PUSEDEF_CHAIN psUseDef;
	PSTRUCT_PACKET psPacket = IMG_NULL;
	FILE *pFile = (FILE*)pvFileHandle;
	
	ASSERT((*ppsUseDef));
	
	psUseDef = *ppsUseDef;
	LOAD_STORE_STRUCT((*ppsUseDef), sizeof(USEDEF_CHAIN));
	psUseDef = *ppsUseDef;

	LOAD_STORE_EACH_ENTRY_START((&psUseDef->sList), PUSEDEF, sListEntry);
		LOAD_STORE_STRUCT(psElement, sizeof(USEDEF));
	LOAD_STORE_EACH_ENTRY_END(sListEntry);
}

static 
IMG_VOID FixUseDefInfo(PINTERMEDIATE_STATE psState, PTRANS_STATE psTransState, PUSEDEF psUseDef)
/*****************************************************************************
 FUNCTION	: FixUseDefInfo

 PURPOSE	: Writes a intermediate code block to file

 PARAMETERS	: psState	    - Shader compiler state.
			  psBloc        - Block to serialize
			  pFile         - File Handle

 RETURNS	: None.
*****************************************************************************/
{
	TRANSLATE_ADDRESS(psUseDef->psUseDefChain);
	TRANSLATE_ADDRESS(psUseDef->u.pvData);
}

static
IMG_VOID LoadStoreInstruction(PINTERMEDIATE_STATE psState, PTRANS_STATE psTransState, PINST *ppsInst, FILE *pFile, IMG_BOOL bStore)
/*****************************************************************************
 FUNCTION	: LoadStoreInstruction

 PURPOSE	: Writes a intermediate instruction with all of its intermediate code 
			  block to file

 PARAMETERS	: psState	    - Shader compiler state.
			  psBloc        - Block to serialize
			  pFile         - File Handle

 RETURNS	: None.
*****************************************************************************/
{
	PINST psInst;
	IMG_UINT32 i, uInstParamSize;
	PSTRUCT_PACKET psPacket = IMG_NULL;
	
	psInst = *ppsInst;
	
#if defined(UF_TESTBENCH)
	if(bStore)
	{
		IMG_CHAR acBuffer[1024], *pcReader;
		pcReader = acBuffer;
		PrintIntermediateInst(psState, psInst, IMG_NULL, acBuffer);
		acBuffer[1023] = '\0';
		/* Insert comment start for all new lines */
		while(*pcReader != '\0')
		{
			if((*pcReader) == '\n')
			{
				*(++pcReader) = '@';
			}
			pcReader++;
		}
		fprintf(pFile, "@ %s\n", acBuffer);
	}
#endif
	
	LOAD_STORE_STRUCT(psInst, sizeof(INST));
	
	*ppsInst = psInst;
	
	if(psInst->uDestCount)
	{
		ICODE_COMMENT("psInst->uDestCount");
		LoadOrStoreArg(psState, psTransState, &psInst->asDest, psInst->uDestCount, pFile, bStore);
		
		LOAD_STORE_STRUCT(psInst->asDestUseDef, sizeof(ARGUMENT_USEDEF) * psInst->uDestCount);
	
		LOAD_STORE_STRUCT(psInst->auDestMask, sizeof(IMG_UINT32) * psInst->uDestCount);
		LOAD_STORE_STRUCT(psInst->auLiveChansInDest, sizeof(IMG_UINT32) * psInst->uDestCount);
		
		LOAD_STORE_STRUCT(psInst->apsOldDest, sizeof(PARG) * psInst->uDestCount);
		LOAD_STORE_STRUCT(psInst->apsOldDestUseDef, sizeof(PARGUMENT_USEDEF) * psInst->uDestCount);
		for(i = 0; i < psInst->uDestCount; i++)
		{
			if(psInst->apsOldDest[i])
			{
				LoadOrStoreArg(psState, psTransState, &psInst->apsOldDest[i], 1, pFile, bStore);
			}
			if(psInst->apsOldDestUseDef[i])
			{
				LOAD_STORE_STRUCT(psInst->apsOldDestUseDef[i], sizeof(ARGUMENT_USEDEF));
			}
		}
	}
	if(psInst->uArgumentCount)
	{
		ICODE_COMMENT("psInst->uArgumentCount");
		LoadOrStoreArg(psState, psTransState, &psInst->asArg, psInst->uArgumentCount, pFile, bStore);
		LOAD_STORE_STRUCT(psInst->asArgUseDef, sizeof(ARGUMENT_USEDEF) * psInst->uArgumentCount);
	}
	
	if(psInst->uPredCount)
	{
		ICODE_COMMENT("psInst->uPredCount");
		LoadOrStoreArg(psState, psTransState, &psInst->apsPredSrc, psInst->uPredCount, pFile, bStore);
		LOAD_STORE_STRUCT(psInst->apsPredSrcUseDef, sizeof(USEDEF) * psInst->uPredCount);
	}

	if(psInst->uTempCount)
	{
		LOAD_STORE_STRUCT(psInst->asTemp, sizeof(SHORT_REG) * psInst->uDestCount);
	}
	
	uInstParamSize = GetInstructionTypeParamSize(psInst);
	if(uInstParamSize)
	{
		LOAD_STORE_STRUCT(psInst->u.pvNULL, uInstParamSize);
	}
	
	switch(g_psInstDesc[psInst->eOpcode].eType)
	{
		case INST_TYPE_LOADCONST:
			if(psInst->u.psLoadConst->psRange)
			{
				PCONSTANT_RANGE psRange;
				LOAD_STORE_STRUCT(psInst->u.psLoadConst->psRange, sizeof(CONSTANT_RANGE));
				psRange = psInst->u.psLoadConst->psRange;
				
				if(psRange->psData)
				{
					ASSERT(0);
				}
			}
			break;
		default:
			break;
	}
	
	if(psInst->psGroupNext)
	{
		ICODE_COMMENT("psInst->psGroupNext");
		/*
			Load the detached instruction from blocks
		*/
		LoadStoreInstruction(psState, psTransState, &psInst->psGroupNext, pFile, bStore);
	}
}

static
IMG_VOID LoadOrStoreBlockToFile(PINTERMEDIATE_STATE psState, PTRANS_STATE psTransState, PCODEBLOCK psBlock, 
								IMG_VOID *pvFileHandle, IMG_BOOL bStore)
/*****************************************************************************
 FUNCTION	: LoadOrStoreBlockToFile

 PURPOSE	: Writes a intermediate code block to file

 PARAMETERS	: psState	    - Shader compiler state.
			  psBloc        - Block to serialize
			  pFile         - File Handle

 RETURNS	: None.
*****************************************************************************/
{
#if defined(DEBUG_STRUCT_LOAD_STORE)
	IMG_CHAR acTempBuffer[4096];
#endif
	PSTRUCT_PACKET psPacket;
	PINST psInst = IMG_NULL, psPrevInst = IMG_NULL;
	IMG_UINT32 uInstCount, i;
	FILE *pFile = (FILE*) pvFileHandle;
	
	ICODE_COMMENT("CODEBLOCK_START");
	
	if(psBlock->uNumPreds)
	{
		if(psBlock->asPreds)
		{
			LOAD_STORE_STRUCT(psBlock->asPreds, sizeof(CODEBLOCK_EDGE) * psBlock->uNumPreds);
		}
	}
	else
	{
		ASSERT(!psBlock->asPreds);
	}
	
	if(psBlock->uNumSuccs)
	{
		if(psBlock->asSuccs)
		{
			LOAD_STORE_STRUCT(psBlock->asSuccs, sizeof(CODEBLOCK_EDGE) * psBlock->uNumSuccs);
		}
	}
	else
	{
		ASSERT(!psBlock->asSuccs);
	}
	
	psInst = psBlock->psBody;
	for(uInstCount = 0; uInstCount < psBlock->uInstCount; uInstCount++)
	{
		LoadStoreInstruction(psState, psTransState, &psInst, pFile, bStore);
		
		if(psPrevInst)
		{
			TRANS_ASSERT(psPrevInst->psNext, psInst);
			TRANS_ASSERT(psInst->psPrev, psPrevInst);
		}
		else
		{
			TRANS_ASSERT(psBlock->psBody, psInst);
		}
		TRANS_ASSERT(psInst->psBlock, psBlock);
		psPrevInst = psInst;
		
		psInst = psInst->psNext;
	}
	
	for(i = 0; i < MAXIMUM_GPI_SIZE_IN_SCALAR_REGS; i++)
	{
		if(psBlock->apsIRegUseDef[i])
		{
			ICODE_COMMENT("psBlock->apsIRegUseDef[i]");
			LoadStoreUseDefChain(psState, psTransState, &psBlock->apsIRegUseDef[i], pFile, bStore);
		}
	}
	TRANS_ASSERT(psBlock->psBodyTail, psPrevInst);
	{
		ICODE_COMMENT("psBlock->sRegistersLiveOut.sPredicate");
		SerializeOrUnSerializeVectorChunks(psState, psTransState, pFile, &psBlock->sRegistersLiveOut.sPredicate, bStore);
		ICODE_COMMENT("psBlock->sRegistersLiveOut.sPrimAttr");
		SerializeOrUnSerializeVectorChunks(psState, psTransState, pFile, &psBlock->sRegistersLiveOut.sPrimAttr, bStore);
		ICODE_COMMENT("psBlock->sRegistersLiveOut.sTemp");
		SerializeOrUnSerializeVectorChunks(psState, psTransState, pFile, &psBlock->sRegistersLiveOut.sTemp, bStore);
		ICODE_COMMENT("psBlock->sRegistersLiveOut.sOutput");
		SerializeOrUnSerializeVectorChunks(psState, psTransState, pFile, &psBlock->sRegistersLiveOut.sOutput, bStore);
		ICODE_COMMENT("psBlock->sRegistersLiveOut.sFpInternal");
		SerializeOrUnSerializeVectorChunks(psState, psTransState, pFile, &psBlock->sRegistersLiveOut.sFpInternal, bStore);
	}
	if(!bStore)
	{
		TRANSLATE_ADDRESS(psBlock->psOwner);
		psBlock->psSavedDepState = IMG_NULL;
	}
	
	if(psBlock->eType == CBTYPE_SWITCH)
	{
		ASSERT(psBlock->uNumSuccs);
		
		LOAD_STORE_STRUCT(psBlock->u.sSwitch.auCaseValues, (psBlock->uNumSuccs - 1) * sizeof(IMG_UINT32));
	}
	
	ICODE_COMMENT("CODEBLOCK_END");
}

static 
IMG_VOID LoadOrStoreFunction(PINTERMEDIATE_STATE psState, PTRANS_STATE psTransState, PFUNC *ppsFunc, 
							   IMG_VOID *pvFileHandle, IMG_BOOL bStore)
/*****************************************************************************
 FUNCTION	: LoadOrStoreFunction

 PURPOSE	: Writes a function with all of its intermediate code block to file

 PARAMETERS	: psState	    - Shader compiler state.
			  psBloc        - Block to serialize
			  pFile         - File Handle

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32 uBlockIndex;
	PSTRUCT_PACKET psPacket = IMG_NULL;
	
	FILE *pFile = (FILE*)pvFileHandle;
	PFUNC psFunc = *ppsFunc;
	
	ICODE_COMMENT("FUNC_START");
	
	LOAD_STORE_STRUCT(psFunc, sizeof(FUNC));
	*ppsFunc = psFunc;
	
	if(psFunc->pchEntryPointDesc && bStore)
	{
		IMG_CHAR *pcTempBuffer;
		pcTempBuffer = UscAlloc(psState, 128);
		memset(pcTempBuffer, 0, 128);
		strncpy(pcTempBuffer, psFunc->pchEntryPointDesc, 127);
		LOAD_STORE_STRUCT(psFunc->pchEntryPointDesc, 128);
	}
	if(psFunc->pchEntryPointDesc && (!bStore))
	{
		IMG_CHAR *pcTempBuffer = psFunc->pchEntryPointDesc;
		LOAD_STORE_STRUCT(pcTempBuffer, 128);
		pcTempBuffer[127] = '\0';
		psFunc->pchEntryPointDesc = pcTempBuffer;
	}
	
	if(psFunc->uNumBlocks)
	{
		LOAD_STORE_STRUCT(psFunc->apsAllBlocks, sizeof(PCODEBLOCK) * psFunc->uNumBlocks);
		for(uBlockIndex = 0; uBlockIndex < psFunc->uNumBlocks; uBlockIndex++)
		{
			LOAD_STORE_STRUCT(psFunc->apsAllBlocks[uBlockIndex], sizeof(CODEBLOCK));
			LoadOrStoreBlockToFile(psState, psTransState, psFunc->apsAllBlocks[uBlockIndex], pFile, bStore);
		}
	}
	
	if(psFunc->sIn.uCount)
	{
		ICODE_COMMENT("psFunc->sIn");
		LOAD_STORE_STRUCT(psFunc->sIn.asArray, sizeof(FUNC_INOUT) * psFunc->sIn.uCount);
		LOAD_STORE_STRUCT(psFunc->sIn.asArrayUseDef, sizeof(USEDEF) * psFunc->sIn.uCount);
	}
	
	if(psFunc->sOut.uCount)
	{
		ICODE_COMMENT("psFunc->sOut");
		LOAD_STORE_STRUCT(psFunc->sOut.asArray, sizeof(FUNC_INOUT) * psFunc->sOut.uCount);
		LOAD_STORE_STRUCT(psFunc->sOut.asArrayUseDef, sizeof(USEDEF) * psFunc->sOut.uCount);
	}
	
	ICODE_COMMENT("psFunc->sCallStartRegistersLive.sPredicate");
	SerializeOrUnSerializeVectorChunks(psState, psTransState, pFile, &psFunc->sCallStartRegistersLive.sPredicate, bStore);
	ICODE_COMMENT("psFunc->sCallStartRegistersLive.sPrimAttr");
	SerializeOrUnSerializeVectorChunks(psState, psTransState, pFile, &psFunc->sCallStartRegistersLive.sPrimAttr, bStore); 
	ICODE_COMMENT("psFunc->sCallStartRegistersLive.sTemp");
	SerializeOrUnSerializeVectorChunks(psState, psTransState, pFile, &psFunc->sCallStartRegistersLive.sTemp, bStore);
	ICODE_COMMENT("psFunc->sCallStartRegistersLive.sOutput");
	SerializeOrUnSerializeVectorChunks(psState, psTransState, pFile, &psFunc->sCallStartRegistersLive.sOutput, bStore);
	ICODE_COMMENT("psFunc->sCallStartRegistersLive.sFpInternal");
	SerializeOrUnSerializeVectorChunks(psState, psTransState, pFile, &psFunc->sCallStartRegistersLive.sFpInternal, bStore);
	ICODE_COMMENT("FUNC_END");
	
	if(!bStore)
	{
		TRANSLATE_ADDRESS(psFunc->sCfg.psExit);
		TRANSLATE_ADDRESS(psFunc->sCfg.psEntry);
	}
}

static
IMG_VOID FixUseDefChain(PINTERMEDIATE_STATE psState, PTRANS_STATE psTransState, PUSEDEF_CHAIN psUseDef)
/*****************************************************************************
 FUNCTION	: FixUseDefChain

 PURPOSE	: Writes a function with all of its intermediate code block to file

 PARAMETERS	: psState	    - Shader compiler state.
			  psBloc        - Block to serialize
			  pFile         - File Handle

 RETURNS	: None.
*****************************************************************************/
{
	PUSC_LIST_ENTRY psListEntry, psNextListEntry;
	
	TRANSLATE_LIST_ENTRY_ADDRESS(psUseDef->sIndexUseTempListEntry.psNext, PUSEDEF_CHAIN, sIndexUseTempListEntry);
	TRANSLATE_LIST_ENTRY_ADDRESS(psUseDef->sIndexUseTempListEntry.psPrev, PUSEDEF_CHAIN, sIndexUseTempListEntry);
	
	TRANSLATE_LIST_ENTRY_ADDRESS(psUseDef->sDroppedUsesTempListEntry.psNext, PUSEDEF_CHAIN, sDroppedUsesTempListEntry);
	TRANSLATE_LIST_ENTRY_ADDRESS(psUseDef->sDroppedUsesTempListEntry.psPrev, PUSEDEF_CHAIN, sDroppedUsesTempListEntry);
	
	TRANSLATE_LIST_ENTRY_ADDRESS(psUseDef->sC10TempListEntry.psNext, PUSEDEF_CHAIN, sC10TempListEntry);
	TRANSLATE_LIST_ENTRY_ADDRESS(psUseDef->sC10TempListEntry.psPrev, PUSEDEF_CHAIN, sC10TempListEntry);
	
	TRANSLATE_ADDRESS(psUseDef->psBlock);
	TRANSLATE_ADDRESS(psUseDef->psDef);
	
	for (psListEntry = psUseDef->sList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PUSEDEF	psUse;
		psNextListEntry = psListEntry->psNext;
	
		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		if (psUse->eType == DEF_TYPE_UNINITIALIZED)
		{
			FixUseDefInfo(psState, psTransState, psUse);
		}
	}
}

static
IMG_VOID FixOldAddress(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvTransState)
/*****************************************************************************
 FUNCTION	: FixOldAddress

 PURPOSE	: Replace all of the old ref address with new addresses fo equivalent
 			  structures.

 PARAMETERS	: psState	    - Shader compiler state.
 			  psBlock		- Block to work upon
			  psTransState	- Trans State

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32 i;
	PINST psInst;
	PTRANS_STATE psTransState = (PTRANS_STATE)pvTransState;
	
	TRANSLATE_ADDRESS(psBlock->psIDom);
	TRANSLATE_ADDRESS(psBlock->psIPostDom);
	
#if defined(REGALLOC_LSCAN) || defined(IREGALLOC_LSCAN)
	TRANSLATE_ADDRESS(psBlock->psLoopHeader);
#endif
	TRANSLATE_ADDRESS(psBlock->psExtPostDom);
	TRANSLATE_ADDRESS(psBlock->psWorkListNext);
	
	if(psBlock->asPreds)
	{
		for(i = 0; i < psBlock->uNumPreds; i++)
		{
			TRANSLATE_ADDRESS(psBlock->asPreds[i].psDest);
		}
	}
	if(psBlock->asSuccs)
	{
		for(i = 0; i < psBlock->uNumSuccs; i++)
		{
			TRANSLATE_ADDRESS(psBlock->asSuccs[i].psDest);
		}
	}
	
	for(i = 0; i < MAXIMUM_GPI_SIZE_IN_SCALAR_REGS; i++)
	{
		if(psBlock->apsIRegUseDef[i])
		{
			FixUseDefChain(psState, psTransState, psBlock->apsIRegUseDef[i]);
		}
	}
	
	psInst = psBlock->psBody;
	
	while(psInst)
	{
		IMG_UINT32 uPred;

		for(i = 0; i < psInst->uDestCount; i++)
		{
			FixUseDefInfo(psState, psTransState, &psInst->asDestUseDef[i].sUseDef);
			FixUseDefInfo(psState, psTransState, &psInst->asDestUseDef[i].sIndexUseDef);
			
			if(psInst->apsOldDestUseDef[i])
			{
				FixUseDefInfo(psState, psTransState, &psInst->apsOldDestUseDef[i]->sUseDef);
				FixUseDefInfo(psState, psTransState, &psInst->apsOldDestUseDef[i]->sIndexUseDef);
			}
		}
		
		for(i = 0; i < psInst->uArgumentCount; i++)
		{
			FixUseDefInfo(psState, psTransState, &psInst->asArgUseDef[i].sUseDef);
			FixUseDefInfo(psState, psTransState, &psInst->asArgUseDef[i].sIndexUseDef);
		}
		
		for (uPred = 0; uPred < psInst->uPredCount; ++uPred)
		{
			FixUseDefInfo(psState, psTransState, psInst->apsPredSrcUseDef[uPred]);
		}
		
		psInst->sAvailableListEntry.psNext = psInst->sAvailableListEntry.psPrev = IMG_NULL;
		psInst->sTempListEntry.psNext = psInst->sTempListEntry.psPrev = IMG_NULL;
		
		TRANSLATE_LIST_ENTRY_ADDRESS(psInst->sOpcodeListEntry.psPrev, PINST, sOpcodeListEntry);
		TRANSLATE_LIST_ENTRY_ADDRESS(psInst->sOpcodeListEntry.psNext, PINST, sOpcodeListEntry);

		TRANSLATE_ADDRESS(psInst->psCoInst);
		TRANSLATE_ADDRESS(psInst->psGroupParent);
		
		ASSERT(!psInst->psRepeatGroup);
		
		switch(g_psInstDesc[psInst->eOpcode].eType)
		{
#if defined(OUTPUT_USPBIN)
			case INST_TYPE_SMPUNPACK:
				TRANSLATE_ADDRESS(psInst->u.psSmpUnpack->psTextureSample);
				
				ASSERT(psInst->u.psSmpUnpack->psTextureSample);
				
				TRANSLATE_ADDRESS(psInst->u.psSmpUnpack->psTextureSample->u.psSmp->sUSPSample.psSampleUnpack);
				break;
#endif
			case INST_TYPE_LOADCONST:
				TRANSLATE_ADDRESS(psInst->u.psLoadConst->psInst);
				TRANSLATE_LIST_ENTRY_ADDRESS(psInst->u.psLoadConst->sBufferListEntry.psPrev, PLOADCONST_PARAMS, sBufferListEntry);
				TRANSLATE_LIST_ENTRY_ADDRESS(psInst->u.psLoadConst->sBufferListEntry.psNext, PLOADCONST_PARAMS, sBufferListEntry);
				break;
			case INST_TYPE_CALL:
				TRANSLATE_ADDRESS(psInst->u.psCall->psCallSiteNext);
				TRANSLATE_ADDRESS(psInst->u.psCall->psTarget);
				TRANSLATE_ADDRESS(psInst->u.psCall->psBlock);
				break;
			case INST_TYPE_DELTA:
				TRANSLATE_ADDRESS(psInst->u.psDelta->psInst);
				TRANSLATE_LIST_ENTRY_ADDRESS(psInst->u.psDelta->sListEntry.psPrev, PDELTA_PARAMS, sListEntry);
				TRANSLATE_LIST_ENTRY_ADDRESS(psInst->u.psDelta->sListEntry.psNext, PDELTA_PARAMS, sListEntry);
				break;
			default:
				break;
		}
		psInst = psInst->psNext;
	}
	
	switch(psBlock->eType)
	{
	case CBTYPE_UNCOND:
		break;
	case CBTYPE_COND:
		//psBlock.u.sPredSrc
		FixUseDefInfo(psState, psTransState, &psBlock->u.sCond.sPredSrcUse);
		break;
	case CBTYPE_SWITCH:
		FixUseDefInfo(psState, psTransState, &psBlock->u.sSwitch.sArgUse.sUseDef);
		FixUseDefInfo(psState, psTransState, &psBlock->u.sSwitch.sArgUse.sIndexUseDef);
		break;
	case CBTYPE_EXIT:
		break;
	case CBTYPE_UNDEFINED:
		break;
	}
}

static 
IMG_VOID LoadOrStoreFixedRegData(PINTERMEDIATE_STATE psState, PTRANS_STATE psTransState, 
								 PFIXED_REG_DATA *ppFixedReg, IMG_VOID *pvFileHandle, IMG_BOOL bStore)
/*****************************************************************************
 FUNCTION	: LoadOrStoreFixedRegData

 PURPOSE	: 

 PARAMETERS	: psState	    - Shader compiler state.
			  psBloc        - Block to serialize
			  pFile         - File Handle

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT uNumAttributes;
	PFIXED_REG_DATA psFixedReg;
	PSTRUCT_PACKET psPacket = IMG_NULL;
	FILE *pFile = (FILE*)pvFileHandle;
	
	if(!(*ppFixedReg))
	{
		return;
	}
	
	LOAD_STORE_STRUCT((*ppFixedReg), sizeof(FIXED_REG_DATA));
	psFixedReg = (*ppFixedReg);
	
	ASSERT(psFixedReg->uVRegType < USEASM_REGTYPE_MAXIMUM);
	
	if(psFixedReg->auVRegNum)
	{
		LOAD_STORE_STRUCT(psFixedReg->auVRegNum, sizeof(psFixedReg->auVRegNum[0]) * psFixedReg->uConsecutiveRegsCount);
	}
	
	if(psFixedReg->asVRegUseDef)
	{
		LOAD_STORE_STRUCT(psFixedReg->asVRegUseDef, sizeof(USEDEF) * psFixedReg->uConsecutiveRegsCount);
	}
	
	if(psFixedReg->puUsedChans)
	{
		IMG_UINT32 uChansInInput;
		
		uNumAttributes = psFixedReg->uConsecutiveRegsCount;
		uChansInInput = uNumAttributes * CHANS_PER_REGISTER;
		LOAD_STORE_STRUCT(psFixedReg->puUsedChans, UINTS_TO_SPAN_BITS(uChansInInput) * sizeof(IMG_UINT32));
	}
	
	#if defined(OUTPUT_USPBIN)
	LoadOrStoreArg(psState, psTransState, &psFixedReg->asAltPRegs, psFixedReg->uNumAltPRegs, pFile, bStore);
	#endif
	
	if(!bStore)
	{
		TRANSLATE_ADDRESS(psFixedReg->sPReg.psRegister);
		TRANSLATE_ADDRESS(psFixedReg->sPReg.psIndexRegister);
	}
}

static
IMG_VOID LoadOrStoreVertexShaderState(PINTERMEDIATE_STATE psState, PTRANS_STATE psTransState, 
									 PVERTEXSHADER_STATE *ppsVS, IMG_VOID *pvFileHandle, IMG_BOOL bStore)
/*****************************************************************************
 FUNCTION	: LoadOrStoreVertexShaderState

 PURPOSE	: 

 PARAMETERS	: psState	    - Shader compiler state.
			  psBloc        - Block to serialize
			  pFile         - File Handle

 RETURNS	: None.
*****************************************************************************/
{
	FILE *pFile = (FILE*)pvFileHandle;
	PSTRUCT_PACKET psPacket = IMG_NULL;
	PVERTEXSHADER_STATE psVS;
	
	LOAD_STORE_STRUCT((*ppsVS), sizeof(VERTEXSHADER_STATE));
	psVS = (*ppsVS);
	
	if(psVS->auVSInputPARegUsage)
	{
		LOAD_STORE_STRUCT(psVS->auVSInputPARegUsage, UINTS_TO_SPAN_BITS(EURASIA_USE_PRIMATTR_BANK_SIZE) * sizeof(IMG_UINT32));
	}
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	ASSERT(!pVS->auVSOutputToVecReg);
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
}

static
IMG_VOID LoadOrStorePixelShaderState(PINTERMEDIATE_STATE psState, PTRANS_STATE psTransState, 
									 PPIXELSHADER_STATE *ppsPS, IMG_VOID *pvFileHandle, IMG_BOOL bStore)
/*****************************************************************************
 FUNCTION	: LoadOrStorePixelShaderState

 PURPOSE	: 

 PARAMETERS	: psState	    - Shader compiler state.
			  psBloc        - Block to serialize
			  pFile         - File Handle

 RETURNS	: None.
*****************************************************************************/
{
	FILE *pFile = (FILE*)pvFileHandle;
	PSTRUCT_PACKET psPacket = IMG_NULL;
	
	PPIXELSHADER_STATE psPS;
	
	IMG_UINT32 i, uIndex;
	
	LOAD_STORE_STRUCT((*ppsPS), sizeof(PIXELSHADER_STATE));
	psPS = (*ppsPS);
	
	for(i = 0; i < USC_MAXIMUM_TEXTURE_COORDINATES; i++)
	{
#if defined(OUTPUT_USPBIN)
		if(psPS->apsF32TextureCoordinateInputs[i])
		{
			LOAD_STORE_STRUCT(psPS->apsF32TextureCoordinateInputs[i], sizeof(PIXELSHADER_INPUT));
		}
#endif
		if(psPS->asTextureCoordinateArrays)
		{
			LOAD_STORE_STRUCT(psPS->asTextureCoordinateArrays, 
							sizeof(TEXTURE_COORDINATE_ARRAY) * psPS->uTextureCoordinateArrayCount);
			
			for(uIndex = 0; uIndex < psPS->uTextureCoordinateArrayCount; uIndex++)
			{
				if(psPS->asTextureCoordinateArrays[uIndex].psRange)
				{
					IMG_UINT32 uIterationIndex;
					IMG_UINT32 uArraySize;
					
					LOAD_STORE_STRUCT(psPS->asTextureCoordinateArrays[uIndex].psRange, sizeof(UNIFLEX_RANGE));
					
					uArraySize = psPS->asTextureCoordinateArrays[uIndex].psRange->uRangeEnd - psPS->asTextureCoordinateArrays[uIndex].psRange->uRangeStart;
					LOAD_STORE_STRUCT(psPS->asTextureCoordinateArrays[uIndex].apsIterations, 
									sizeof(PPIXELSHADER_INPUT) * uArraySize);
					
					for(uIterationIndex = 0; uIterationIndex < uArraySize; uIterationIndex++)
					{
						LOAD_STORE_STRUCT(psPS->asTextureCoordinateArrays[uIndex].apsIterations[uIterationIndex], 
												sizeof(PIXELSHADER_INPUT));
					}
				}
			}
		}
	}
	
	{
		PUSC_LIST_ENTRY psListEntry, psPrevListEntry = IMG_NULL;
		PPIXELSHADER_INPUT psShaderInput = IMG_NULL;
		
		for (psListEntry = psPS->sPixelShaderInputs.psHead;
				   psListEntry != NULL;
				   psListEntry = psListEntry->psNext)
		{
			psShaderInput = IMG_CONTAINING_RECORD(psListEntry, PPIXELSHADER_INPUT, sListEntry);
			
			LOAD_STORE_STRUCT(psShaderInput, sizeof(PIXELSHADER_INPUT));
			
			if(!bStore)
			{
				psListEntry = &psShaderInput->sListEntry;
			}
			
			if(psPrevListEntry)
			{
				TRANS_ASSERT(psPrevListEntry->psNext, psListEntry);
				TRANS_ASSERT(psListEntry->psPrev, psPrevListEntry);
			}
			else
			{
				TRANS_ASSERT(psPS->sPixelShaderInputs.psHead, &psShaderInput->sListEntry);
			}
			psPrevListEntry = psListEntry;
		}
		if(psShaderInput)
		{
			TRANS_ASSERT(psPS->sPixelShaderInputs.psTail, &psShaderInput->sListEntry);
		}
		else
		{
			TRANS_ASSERT(psPS->sPixelShaderInputs.psTail, IMG_NULL);
		}
	}
}

static 
IMG_VOID LoadOrStoreRegisterGroup(PINTERMEDIATE_STATE psState, PTRANS_STATE psTransState, PREGISTER_GROUP *ppsRegGroup,
									FILE *pFile, IMG_BOOL bStore)
/*****************************************************************************
 FUNCTION	: LoadOrStoreRegisterGroup

 PURPOSE	: 

 PARAMETERS	: psState	    - Shader compiler state.
			  psBloc        - Block to serialize
			  pFile         - File Handle

 RETURNS	: None.
*****************************************************************************/
{
	PSTRUCT_PACKET psPacket = IMG_NULL;
	
	PREGISTER_GROUP psRegGroup = *ppsRegGroup;
	
	LOAD_STORE_STRUCT(psRegGroup, sizeof(REGISTER_GROUP));
	*ppsRegGroup = psRegGroup;
}

static 
IMG_VOID FixRegisterGroup(PINTERMEDIATE_STATE psState, PTRANS_STATE psTransState, PREGISTER_GROUP *ppsRegGroup)
{
	PREGISTER_GROUP psRegGroup = *ppsRegGroup;
	
	if(psRegGroup->psPrev)
	{
		TRANSLATE_ADDRESS(psRegGroup->psPrev);
		TRANSLATE_ADDRESS(psRegGroup->psPrev->psNext);
	}
	TRANSLATE_ADDRESS(psRegGroup->psFixedReg);
	if(psRegGroup->u.pvNULL)
	{
		TRANSLATE_ADDRESS(psRegGroup->u.pvNULL);
	}
}

static
IMG_VOID FixPixelShaderState(PINTERMEDIATE_STATE psState, PTRANS_STATE psTransState, PPIXELSHADER_STATE *ppsPS, 
							 FILE *pFile, IMG_BOOL bStore)
/*****************************************************************************
 FUNCTION	: FixPixelShaderState

 PURPOSE	: Writes a function with all of its intermediate code block to file

 PARAMETERS	: psState	    - Shader compiler state.
			  psBloc        - Block to serialize
			  pFile         - File Handle

 RETURNS	: None.
*****************************************************************************/
{
	if(ppsPS && *ppsPS)
	{
		TRANSLATE_ADDRESS((*ppsPS)->psColFixedReg);
		{
			LOAD_STORE_EACH_ENTRY_START((&((*ppsPS)->sPixelShaderInputs)), PPIXELSHADER_INPUT, sListEntry);
				TRANSLATE_ADDRESS(psElement->psFixedReg);
			LOAD_STORE_EACH_ENTRY_END(sListEntry);
		}
		
		if((*ppsPS)->psFixedHwPARegReg)
		{
			TRANSLATE_ADDRESS((*ppsPS)->psFixedHwPARegReg);
		}
	}
}

static
IMG_VOID FixVertexShaderState(PINTERMEDIATE_STATE psState, PTRANS_STATE psTransState, PVERTEXSHADER_STATE *ppsVS, 
							  FILE *pFile, IMG_BOOL bStore)
/*****************************************************************************
 FUNCTION	: FixVertexShaderState

 PURPOSE	: Writes a function with all of its intermediate code block to file

 PARAMETERS	: psState	    - Shader compiler state.
			  psBloc        - Block to serialize
			  pFile         - File Handle

 RETURNS	: None.
*****************************************************************************/
{
	PVERTEXSHADER_STATE psVS = *ppsVS;
	
	PVR_UNREFERENCED_PARAMETER(bStore);
	PVR_UNREFERENCED_PARAMETER(pFile);
	
	if(psVS->psVertexShaderInputsFixedReg)
	{
		TRANSLATE_ADDRESS(psVS->psVertexShaderInputsFixedReg);
	}
	if(psVS->psVertexShaderOutputsFixedReg)
	{
		TRANSLATE_ADDRESS(psVS->psVertexShaderOutputsFixedReg);
	}
}

static
IMG_VOID LoadStoreIntermediateState(PINTERMEDIATE_STATE psState, PTRANS_STATE psTransState, PINTERMEDIATE_STATE *ppsLoadedState, 
									IMG_VOID *pvFileHandle, IMG_BOOL bStore)
/*****************************************************************************
 FUNCTION	: LoadStoreIntermediateState

 PURPOSE	: Writes a function with all of its intermediate code block to file

 PARAMETERS	: psState	    - Shader compiler state.
			  psBloc        - Block to serialize
			  pFile         - File Handle

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32 i, uNumVecArrayRegs, uNumRegisters, uNumPredicates;
	PINTERMEDIATE_STATE psLoadedState, psCurrentState = IMG_NULL;
	PUSC_VEC_ARRAY_REG *ppvVecArrayReg = IMG_NULL;
	PSTRUCT_PACKET psPacket = IMG_NULL;
	PFUNC psFunc, psFuncStart, psPrevFunc = IMG_NULL;
	PUNIFLEX_PROGRAM_PARAMETERS *ppsSAOffsets = IMG_NULL;
	PPIXELSHADER_STATE *ppsPS = IMG_NULL;
	PVERTEXSHADER_STATE *ppsVS = IMG_NULL;
	PSAPROG_STATE psSAProg = IMG_NULL;
	IMG_PUINT32 *ppuSrcLineCost = IMG_NULL;
	USC_PARRAY *ppsTempVReg = IMG_NULL, *ppsPredVReg = IMG_NULL;
	PREGISTER_GROUP_STATE *ppsGroupState;
	
	PUSC_LIST psFixedRegList = IMG_NULL, psDroppedUsesTempList = IMG_NULL, psIndexUseTempList = IMG_NULL;
	
	FILE *pFile = (FILE*)pvFileHandle;
	
	psLoadedState = *ppsLoadedState;
	
	ICODE_COMMENT("INTERMEDIATE_STATE_START");
	LOAD_STORE_STRUCT(psLoadedState, sizeof(INTERMEDIATE_STATE));
	
	if(bStore)
	{
		/* Assingment to locals */
		psCurrentState	= psState;
		ppvVecArrayReg	= psState->apsVecArrayReg;
		uNumVecArrayRegs = psState->uNumVecArrayRegs;
		psFuncStart		= psState->psFnInnermost;
		ppsSAOffsets	= &psState->psSAOffsets;
		uNumVecArrayRegs = psState->uNumVecArrayRegs;
		psSAProg		= &psState->sSAProg;
		psFixedRegList	= &psState->sFixedRegList;
#if defined(SRC_DEBUG)
		ppuSrcLineCost	= &psState->puSrcLineCost;
#endif
		uNumRegisters	= psState->uNumRegisters;
		ppsTempVReg		= &psState->psTempVReg;
		ppsPredVReg		= &psState->psPredVReg;
		uNumPredicates	= psState->uNumPredicates;
		psDroppedUsesTempList
						= &psState->sDroppedUsesTempList;
		psIndexUseTempList
						= &psState->sIndexUseTempList;
		ppsVS			= &psState->sShader.psVS;
		ppsGroupState	= &psState->psGroupState;
	}
	else
	{
		/* Assingment to globals */
		*ppsLoadedState	= psLoadedState;
		
		/* Assingment to locals */
		psCurrentState	= psLoadedState;
		ppvVecArrayReg	= psLoadedState->apsVecArrayReg;
		uNumVecArrayRegs = psLoadedState->uNumVecArrayRegs;
		psFuncStart		= psLoadedState->psFnInnermost;
		ppsSAOffsets	= &psLoadedState->psSAOffsets;
		uNumVecArrayRegs = psLoadedState->uNumVecArrayRegs;
		psSAProg		= &psLoadedState->sSAProg;
		psFixedRegList	= &psLoadedState->sFixedRegList;
#if defined(SRC_DEBUG)
		ppuSrcLineCost	= &psLoadedState->puSrcLineCost;
#endif
		uNumRegisters	= psLoadedState->uNumRegisters;
		ppsTempVReg		= &psLoadedState->psTempVReg;
		ppsPredVReg		= &psLoadedState->psPredVReg;
		uNumPredicates	= psLoadedState->uNumPredicates;
		psDroppedUsesTempList 
						= &psLoadedState->sDroppedUsesTempList;
		psIndexUseTempList
						= &psLoadedState->sIndexUseTempList;
		ppsVS			= &psLoadedState->sShader.psVS;
		ppsGroupState	= &psLoadedState->psGroupState;
	}
	
	for(i = 0; i < uNumVecArrayRegs; i++)
	{
		ICODE_COMMENT("psLoadedState->apsVecArrayReg");
		LOAD_STORE_STRUCT(ppvVecArrayReg[i], sizeof(USC_VEC_ARRAY_REG));
	}
	
	
	if(*ppsTempVReg)
	{
		ICODE_COMMENT("psState->psTempVReg");
		SerializeOrUnSerializeArray(psState, psTransState, pFile, ppsTempVReg, bStore);
		{
			LOAD_STORE_EACH_ARRAY_ELEMENT_START((*ppsTempVReg), PVREGISTER, uNumRegisters);
				LOAD_STORE_STRUCT(psElement, sizeof(VREGISTER));
				if(!bStore)
				{
					ArraySet(psState, (*ppsTempVReg), i, psElement);
				}
			LOAD_STORE_EACH_ARRAY_ELEMENT_END();
		}
	}

	if(*ppsPredVReg)
	{
		ICODE_COMMENT("psState->psPredVReg");
		SerializeOrUnSerializeArray(psState, psTransState, pFile, ppsPredVReg, bStore);
		{
			LOAD_STORE_EACH_ARRAY_ELEMENT_START((*ppsPredVReg), PVREGISTER, uNumPredicates);
				LOAD_STORE_STRUCT(psElement, sizeof(VREGISTER));
				if(!bStore)
				{
					ArraySet(psState, (*ppsPredVReg), i, psElement);
				}
			LOAD_STORE_EACH_ARRAY_ELEMENT_END();
		}
	}
	
	psFunc = psFuncStart;
	while(psFunc)
	{
		LoadOrStoreFunction(psState, psTransState, &psFunc, pFile, bStore);
		if(psPrevFunc)
		{
			TRANS_ASSERT(psPrevFunc->psFnNestOuter, psFunc);
			TRANS_ASSERT(psFunc->psFnNestInner, psPrevFunc);
		}
		psPrevFunc = psFunc;
		psFunc = psFunc->psFnNestOuter;
	}
		

	for(i = 0; i < uNumVecArrayRegs; i++)
	{
		if(ppvVecArrayReg[i]->psBaseUseDef)
		{
			LoadStoreUseDefChain(psState, psTransState, &ppvVecArrayReg[i]->psBaseUseDef, pFile, bStore);
		}
	}
	
	{
		ICODE_COMMENT("psState->sSAProg");
		ICODE_COMMENT("psState->sSAProg.sInRegisterConstantList");
		
		LOAD_STORE_EACH_ENTRY_START((&psSAProg->sInRegisterConstantList), PINREGISTER_CONST, sListEntry);
		LOAD_STORE_STRUCT(psElement, sizeof(INREGISTER_CONST));
		LOAD_STORE_EACH_ENTRY_END(sListEntry);
		
		ICODE_COMMENT("psState->sSAProg.sConstantRangeList");
		LOAD_STORE_EACH_ENTRY_START((&psSAProg->sConstantRangeList), PINREGISTER_CONST, sListEntry);
		{
			LOAD_STORE_STRUCT(psElement, sizeof(CONSTANT_RANGE));
		}
		LOAD_STORE_EACH_ENTRY_END(sListEntry);

		ICODE_COMMENT("psState->sSAProg.sInRegisterConstantRangeList");
		LOAD_STORE_EACH_ENTRY_START((&psSAProg->sInRegisterConstantRangeList), PCONSTANT_INREGISTER_RANGE, sListEntry);
		LOAD_STORE_STRUCT(psElement, sizeof(CONSTANT_INREGISTER_RANGE));
		LOAD_STORE_EACH_ENTRY_END(sListEntry);
		
		ICODE_COMMENT("psState->sFixedRegList");
		LOAD_STORE_EACH_ENTRY_START(psFixedRegList, PFIXED_REG_DATA, sListEntry);
			LoadOrStoreFixedRegData(psState, psTransState, &psElement, pFile, bStore);
		LOAD_STORE_EACH_ENTRY_END(sListEntry);
		
		ICODE_COMMENT("sSAProg.sFixedRegList");
		LOAD_STORE_EACH_ENTRY_START((&psSAProg->sFixedRegList), PFIXED_REG_DATA, sListEntry);
			LoadOrStoreFixedRegData(psState, psTransState, &psElement, pFile, bStore);
		LOAD_STORE_EACH_ENTRY_END(sListEntry);
		
		ICODE_COMMENT("psState->sSAProg.sResultsList");
		LOAD_STORE_EACH_ENTRY_START((&psSAProg->sResultsList), PSAPROG_RESULT, sListEntry);
			LOAD_STORE_STRUCT(psElement, sizeof(SAPROG_RESULT));
		LOAD_STORE_EACH_ENTRY_END(sListEntry);
		
		ICODE_COMMENT("psState->sSAProg.asAllocRegions");
		LOAD_STORE_STRUCT(psSAProg->asAllocRegions, psSAProg->uAllocRegionCount * sizeof(SA_ALLOC_REGION));
	}

	if(*ppsSAOffsets)
	{
		PUNIFLEX_PROGRAM_PARAMETERS psSAOffsets;
		ICODE_COMMENT("psState->psSAOffsets");
		LOAD_STORE_STRUCT((*ppsSAOffsets), sizeof(UNIFLEX_PROGRAM_PARAMETERS));
		psSAOffsets = (*ppsSAOffsets);
		
		if(psSAOffsets->asTextureParameters && psSAOffsets->uTextureCount)
		{
			ICODE_COMMENT("psSAOffsets->asTextureParameters");
			LOAD_STORE_STRUCT(psSAOffsets->asTextureParameters, psSAOffsets->uTextureCount * sizeof(UNIFLEX_TEXTURE_PARAMETERS));
		}
		
		if(psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL)
		{
			if(bStore)
			{
				ppsPS = &psState->sShader.psPS;
			}
			else
			{
				ppsPS = &psLoadedState->sShader.psPS;
			}
			ICODE_COMMENT("psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL");
			LoadOrStorePixelShaderState(psState, psTransState, ppsPS, pFile, bStore);
		}
		else
		{
			if(psSAOffsets->eShaderType == USC_SHADERTYPE_VERTEX)
			{
				if(bStore)
				{
					ppsVS = &psState->sShader.psVS;
				}
				else
				{
					ppsVS = &psLoadedState->sShader.psVS;
				}
				ICODE_COMMENT("psSAOffsets->eShaderType == USC_SHADERTYPE_VERTEX");
				LoadOrStoreVertexShaderState(psState, psTransState, ppsVS, pFile, bStore);
			}
		}
		if(psSAOffsets->auTextureDimensionality)
		{
			LOAD_STORE_STRUCT(psSAOffsets->auTextureDimensionality, sizeof(IMG_UINT32) * psSAOffsets->uTextureCount);
		}
	}
	
	if(*ppsTempVReg)
	{
		{
			ICODE_COMMENT("psTempVReg USEDEF_CHAIN");
			LOAD_STORE_EACH_ARRAY_ELEMENT_START((*ppsTempVReg), PVREGISTER, uNumRegisters);
				if(psElement->psUseDefChain)
				{
					LoadStoreUseDefChain(psState, psTransState, &psElement->psUseDefChain, pFile, bStore);
				}
				if(psElement->psGroup)
				{
					LoadOrStoreRegisterGroup(psState, psTransState, &psElement->psGroup, pFile, bStore);
				}
			LOAD_STORE_EACH_ARRAY_ELEMENT_END();
		}
	}
	
	if(*ppsPredVReg)
	{
		{
			LOAD_STORE_EACH_ARRAY_ELEMENT_START((*ppsPredVReg), PVREGISTER, uNumPredicates);
				if(psElement->psUseDefChain)
				{
					LoadStoreUseDefChain(psState, psTransState, &psElement->psUseDefChain, pFile, bStore);
				}
				if(psElement->psGroup)
				{
					LoadOrStoreRegisterGroup(psState, psTransState, &psElement->psGroup, pFile, bStore);
				}
			LOAD_STORE_EACH_ARRAY_ELEMENT_END();
		}
	}

	if(*ppsPredVReg)
	{
		{
			LOAD_STORE_EACH_ARRAY_ELEMENT_START((*ppsPredVReg), PVREGISTER, uNumPredicates);
				if(psElement->psGroup)
				{
					TRANSLATE_LIST_ENTRY_ADDRESS(psElement->psGroup->sGroupHeadsListEntry.psNext, PREGISTER_GROUP, sGroupHeadsListEntry);
					TRANSLATE_LIST_ENTRY_ADDRESS(psElement->psGroup->sGroupHeadsListEntry.psPrev, PREGISTER_GROUP, sGroupHeadsListEntry);
				}
			LOAD_STORE_EACH_ARRAY_ELEMENT_END();
		}
	}
	
	if(!bStore)
	{
		TRANSLATE_ADDRESS(psLoadedState->psFnInnermost);
		TRANSLATE_ADDRESS(psLoadedState->psFnOutermost);
		
		psFunc = psLoadedState->psFnInnermost;
		while(psFunc)
		{
			TRANSLATE_ADDRESS(psFunc->psCallSiteHead);
			psFunc = psFunc->psFnNestOuter;
		}
		
		/* Fix rest of psRegGroup ref */
		if(*ppsTempVReg)
		{
			LOAD_STORE_EACH_ARRAY_ELEMENT_START((*ppsTempVReg), PVREGISTER, uNumRegisters);
				if(psElement->psGroup)
				{
					FixRegisterGroup(psState, psTransState, &psElement->psGroup);
				}
			LOAD_STORE_EACH_ARRAY_ELEMENT_END();
		}
		if(*ppsPredVReg)
		{
			LOAD_STORE_EACH_ARRAY_ELEMENT_START((*ppsPredVReg), PVREGISTER, uNumPredicates);
				if(psElement->psGroup)
				{
					FixRegisterGroup(psState, psTransState, &psElement->psGroup);
				}
			LOAD_STORE_EACH_ARRAY_ELEMENT_END();
		}
	}
	
	{
		PUSC_LIST psC10TempList = IMG_NULL;
		
		ICODE_COMMENT("psState->sC10TempList");
		if(!bStore)
		{
			psC10TempList = &psLoadedState->sC10TempList;
		}
		else
		{
			psC10TempList = &psState->sC10TempList;
		}
	}
	
#if defined(SRC_DEBUG)
	if(ppuSrcLineCost)
	{
		LOAD_STORE_STRUCT((*ppuSrcLineCost), sizeof(IMG_UINT32) * (psCurrentState->uTotalLines+1));
	}
#else
	PVR_UNREFERENCED_PARAMETER(ppuSrcLineCost);
#endif
	
	if(*ppsGroupState)
	{
		PREGISTER_GROUP_STATE psGroupState;
		LOAD_STORE_STRUCT((*ppsGroupState), sizeof(REGISTER_GROUP_STATE));
		psGroupState = (*ppsGroupState);
		
		LOAD_STORE_EACH_ENTRY_START((&psGroupState->sGroupHeadsList), PREGISTER_GROUP, sGroupHeadsListEntry);
			LOAD_STORE_STRUCT(psElement, sizeof(REGISTER_GROUP));
		LOAD_STORE_EACH_ENTRY_END(sGroupHeadsListEntry);
		
		SerializeOrUnSerializeTree(psState, psTransState, &psGroupState->psRegisterGroups, TranslateRegGroupNode);
	}
	
	if(psSAProg)
	{
		IMG_UINT32 uInRegIndex;
		LOAD_STORE_EACH_ENTRY_START((&psSAProg->sResultsList), PSAPROG_RESULT, sListEntry);
			if(psElement->eType == SAPROG_RESULT_TYPE_DRIVERLOADED)
			{
				ICODE_COMMENT("PSAPROG_RESULT->eType == SAPROG_RESULT_TYPE_DRIVERLOADED");
				for(uInRegIndex = 0; uInRegIndex < MAX_SCALAR_REGISTERS_PER_VECTOR; uInRegIndex++)
				{
					if(psElement->u1.sDriverConst.apsDriverConst[uInRegIndex])
					{
						LOAD_STORE_STRUCT(psElement->u1.sDriverConst.apsDriverConst[uInRegIndex], sizeof(INREGISTER_CONST));
					}
				}
			}
		LOAD_STORE_EACH_ENTRY_END(sListEntry);
		
		LOAD_STORE_EACH_ENTRY_START((&psSAProg->sDriverLoadedSAList), PSAPROG_RESULT, u1.sDriverConst.sDriverLoadedSAListEntry)
			LOAD_STORE_STRUCT(psElement, sizeof(SAPROG_RESULT));
		LOAD_STORE_EACH_ENTRY_END(u1.sDriverConst.sDriverLoadedSAListEntry);
	}

	ICODE_COMMENT("INTERMEDIATE_STATE_END");
	
	if(!bStore)
	{
		IMG_UINT32 i;
				
		switch(psLoadedState->psSAOffsets->eShaderType)
		{
			case USC_SHADERTYPE_PIXEL:
			{
				FixPixelShaderState(psState, psTransState, ppsPS, pFile, bStore);
				break;
			}
			case USC_SHADERTYPE_VERTEX:
			{
				FixVertexShaderState(psState, psTransState, ppsVS, pFile, bStore);
				break;
			}
			default:
			{
				/* Add code */
				imgabort();
				break;
			}
		}
		
		if(*ppsTempVReg)
		{
			LOAD_STORE_EACH_ARRAY_ELEMENT_START((*ppsTempVReg), PVREGISTER, uNumRegisters);
				if(psElement->psUseDefChain)
				{
					FixUseDefChain(psState, psTransState, psElement->psUseDefChain);
				}
				TRANSLATE_ADDRESS(psElement->psSecAttr);
			LOAD_STORE_EACH_ARRAY_ELEMENT_END();
		}
		
		if(*ppsPredVReg)
		{
			LOAD_STORE_EACH_ARRAY_ELEMENT_START((*ppsPredVReg), PVREGISTER, uNumPredicates);
				if(psElement->psUseDefChain)
				{
					FixUseDefChain(psState, psTransState, psElement->psUseDefChain);
				}
				TRANSLATE_ADDRESS(psElement->psSecAttr);
			LOAD_STORE_EACH_ARRAY_ELEMENT_END();
		}
		
		LOAD_STORE_EACH_ENTRY_START(psFixedRegList, PFIXED_REG_DATA, sListEntry);
			for(i = 0; i < psElement->uConsecutiveRegsCount; i++)
			{
				FixUseDefInfo(psState, psTransState, &psElement->asVRegUseDef[i]);
			}
		LOAD_STORE_EACH_ENTRY_END(sListEntry);
		
		LOAD_STORE_EACH_ENTRY_START((&psSAProg->sFixedRegList), PFIXED_REG_DATA, sListEntry);
			for(i = 0; i < psElement->uConsecutiveRegsCount; i++)
			{
				FixUseDefInfo(psState, psTransState, &psElement->asVRegUseDef[i]);
			}
		LOAD_STORE_EACH_ENTRY_END(sListEntry);
		
		LOAD_STORE_EACH_ENTRY_START((&psSAProg->sInRegisterConstantList), PINREGISTER_CONST, sListEntry);
			TRANSLATE_ADDRESS(psElement->psResult);
		LOAD_STORE_EACH_ENTRY_END(sListEntry);
		
		LOAD_STORE_EACH_ENTRY_START((&psSAProg->sResultsList), PSAPROG_RESULT, sListEntry);
			TRANSLATE_ADDRESS(psElement->psFixedReg);
			TRANSLATE_LIST_ENTRY_ADDRESS(psElement->sRangeListEntry.psNext, PSAPROG_RESULT, sRangeListEntry);
			TRANSLATE_LIST_ENTRY_ADDRESS(psElement->sRangeListEntry.psPrev, PSAPROG_RESULT, sRangeListEntry);
		LOAD_STORE_EACH_ENTRY_END(sListEntry);
	}
}

IMG_INTERNAL
IMG_VOID TranslateSimpleElement(IMG_VOID* pvTransState, IMG_PVOID *ppvElement)
{
	PTRANS_STATE psTransState = (PTRANS_STATE)pvTransState;
	PINTERMEDIATE_STATE psState = psTransState->psState;
	
	if(psTransState->bStore)
		return;
		
	TRANSLATE_ADDRESS((*ppvElement));
}

IMG_INTERNAL
IMG_VOID LoadIntermediateCodeFromFile(PINTERMEDIATE_STATE psState, const IMG_CHAR *pszFileName,
									 const IMG_CHAR *pszDumpInfo, IMG_UINT32 uLineInfo)
/*****************************************************************************
 FUNCTION	: LoadIntermediateCodeFromFile

 PURPOSE	: Loads an already serialized intermediate program in memory.

 PARAMETERS	: psState	    - Shader compiler state.
			  psStruct      - Structure to serialize
			  uSize         - Size of structure

 RETURNS	: None.
*****************************************************************************/
{
	FILE			*pFile;
	IMG_UINT32		i;

	PINTERMEDIATE_STATE psLoadedState = psState;
	PTRANS_STATE	psTransState = IMG_NULL;
	
	PVR_UNREFERENCED_PARAMETER(pszDumpInfo);
	PVR_UNREFERENCED_PARAMETER(uLineInfo);
	
	if(psState->uInsideDoOnAllBasicBlocks)
	{
		DBG_PRINTF((DBG_WARNING, "%s loading ignored, can't override compiler state in middle of progressing opitmization stage.", pszFileName));
		return;
	}
	
	pFile = fopen(pszFileName, "r");
	if(!pFile)
	{
		/* Do nothing */
		return;
	}
	DBG_PRINTF((DBG_WARNING, "Found intermediate dump file %s, now trying load it", pszFileName));
	psTransState = NewTranslationState(psState, pFile, IMG_FALSE);
	
	LoadStoreIntermediateState(psState, psTransState, &psLoadedState, pFile, IMG_FALSE);
	
	fclose(pFile);
	
	if(psState->psSAOffsets)
	{
		if(psState->psSAOffsets->eShaderType != psLoadedState->psSAOffsets->eShaderType)
		{
			DBG_PRINTF((DBG_WARNING, "%s: Can't overide state by icode load when shader types differ", pszFileName));
			return;
		}
	}
	
	/* Now fix the address references */
	DoOnAllBasicBlocks(psLoadedState, ANY_ORDER, FixOldAddress, IMG_TRUE/*CALLs*/, (IMG_PVOID)psTransState);
	
	TRANSLATE_LIST_ENTRY_ADDRESS(psLoadedState->sC10TempList.psHead, PUSEDEF_CHAIN, sC10TempListEntry);
	TRANSLATE_LIST_ENTRY_ADDRESS(psLoadedState->sC10TempList.psTail, PUSEDEF_CHAIN, sC10TempListEntry);
	
	TRANSLATE_LIST_ENTRY_ADDRESS(psLoadedState->sDroppedUsesTempList.psHead, PUSEDEF_CHAIN, sDroppedUsesTempListEntry);
	TRANSLATE_LIST_ENTRY_ADDRESS(psLoadedState->sDroppedUsesTempList.psTail, PUSEDEF_CHAIN, sDroppedUsesTempListEntry);
	
	TRANSLATE_LIST_ENTRY_ADDRESS(psLoadedState->sIndexUseTempList.psHead, PUSEDEF_CHAIN, sIndexUseTempListEntry);
	TRANSLATE_LIST_ENTRY_ADDRESS(psLoadedState->sIndexUseTempList.psTail, PUSEDEF_CHAIN, sIndexUseTempListEntry);
	
	/* Override state */
	psState->psFnInnermost = psLoadedState->psFnInnermost;
	psState->psFnOutermost = psLoadedState->psFnOutermost;
	
	psState->psTempVReg = psLoadedState->psTempVReg;
	psState->psPredVReg = psLoadedState->psPredVReg;
	
	psState->psGroupState = psLoadedState->psGroupState;
	
	for(i = 0; i < psLoadedState->uNumVecArrayRegs; i++)
	{
		TRANSLATE_ADDRESS(psLoadedState->apsVecArrayReg[i]->psUseDef);
		TRANSLATE_ADDRESS(psLoadedState->apsVecArrayReg[i]->psBaseUseDef);
		psState->apsVecArrayReg[i] = psLoadedState->apsVecArrayReg[i];
	}
	for(i = 0; i < IOPCODE_MAX; i++)
	{
		TRANSLATE_LIST_ENTRY_ADDRESS(psLoadedState->asOpcodeLists[i].psHead, PINST, sOpcodeListEntry);
		psState->asOpcodeLists[i].psHead = psLoadedState->asOpcodeLists[i].psHead;
		TRANSLATE_LIST_ENTRY_ADDRESS(psLoadedState->asOpcodeLists[i].psTail, PINST, sOpcodeListEntry);
		psState->asOpcodeLists[i].psTail = psLoadedState->asOpcodeLists[i].psTail;
	}
	
	psState->sDroppedUsesTempList.psHead = psLoadedState->sDroppedUsesTempList.psHead;
	psState->sDroppedUsesTempList.psTail = psLoadedState->sDroppedUsesTempList.psTail;
	
	psState->sIndexUseTempList.psHead = psLoadedState->sIndexUseTempList.psHead;
	psState->sIndexUseTempList.psTail = psLoadedState->sIndexUseTempList.psTail;

	
	psState->sC10TempList.psHead = psLoadedState->sC10TempList.psHead;
	psState->sC10TempList.psTail = psLoadedState->sC10TempList.psTail;
	
	psState->uNumRegisters = psLoadedState->uNumRegisters;
	
	psState->psMainProg = TRANSLATE_ADDRESS(psLoadedState->psMainProg);
	psState->psSecAttrProg = TRANSLATE_ADDRESS(psLoadedState->psSecAttrProg);
	
	psState->sFixedRegList.psHead = psLoadedState->sFixedRegList.psHead;
	psState->sFixedRegList.psTail = psLoadedState->sFixedRegList.psTail;
	
	psState->sSAProg = psLoadedState->sSAProg;
	
	psState->psSAOffsets = psLoadedState->psSAOffsets;
	
	psState->uFlags = psLoadedState->uFlags;
	/* Disable pconversion */
	psState->uCompilerFlags &= ~UF_FORCE_PRECISION_CONVERT_ENABLE;
	
	switch(psLoadedState->psSAOffsets->eShaderType)
	{
		case USC_SHADERTYPE_PIXEL:
		{
			TRANSLATE_ADDRESS(psLoadedState->sShader.psPS->psTexkillOutput);
			psState->sShader.psPS = psLoadedState->sShader.psPS;
			break;
		}
		case USC_SHADERTYPE_VERTEX:
		{
			psState->sShader.psVS = psLoadedState->sShader.psVS;
			break;
		}
		default:
		{
			/* Add code */
			imgabort();
			break;
		}
	}
	{
#ifdef SRC_DEBUG
		psState->uTotalLines = psLoadedState->uTotalLines;
		psState->puSrcLineCost = psLoadedState->puSrcLineCost;
#endif
	}
	
	DBG_PRINTF((DBG_WARNING, "Orveriding with Intermediate dump file %s ", pszFileName));
	
	TESTONLY(PrintIntermediate(psState, "LoadIntermediateCodeFromFile", IMG_TRUE));
	
#if defined(DEBUG_ICODE_SERIALIZATION)
	/* DumpIntermediateToFile(psState, "before_tr_chk", "Translation Check", IMG_FALSE); */
	TranslationCheck(psState, psTransState);
#endif
	FreeTranslationState(psState, &psTransState);
}

IMG_INTERNAL
IMG_VOID DumpIntermediateToFile(PINTERMEDIATE_STATE psState, const IMG_CHAR *pszFileName, 
								const IMG_CHAR *pszComment, const IMG_CHAR *pszDumpInfo, IMG_UINT32 uLineInfo,
								IMG_BOOL bReloadTest)
/*****************************************************************************
 FUNCTION	: DumpIntermediateBlockToFile

 PURPOSE	: Writes a intermediate code block to file with additional size information,
 			  old memeory location.

 PARAMETERS	: psState	    - Shader compiler state.
			  psStruct      - Structure to serialize
			  uSize         - Size of structure

 RETURNS	: None.
*****************************************************************************/
{
	FILE *pFile;
	IMG_BOOL bStore = IMG_TRUE;
	PTRANS_STATE	psTransState;
	
	
	pFile = fopen(pszFileName, "w");
	if(!pFile)
	{
		DBG_PRINTF((DBG_ERROR, "Unable to create file %s", pszFileName));
		return;
	}
	psTransState = NewTranslationState(psState, pFile, IMG_TRUE);
	ICODE_COMMENT_FMT1("Source File Info : %s", pszDumpInfo);
	ICODE_COMMENT_FMT1("Source Line Info : %u", uLineInfo);
	ICODE_COMMENT_FMT1("Comment          : %s", pszComment);

	LoadStoreIntermediateState(psState, psTransState, &psState, pFile, IMG_TRUE);
	
	/* Serialize all USC_ARRAYs */
	fclose(pFile);
	
	FreeTranslationState(psState, &psTransState);
	
	PVR_UNREFERENCED_PARAMETER(bReloadTest);
	if(bReloadTest)
	{
		/* Try loading same file back */
		LoadIntermediateCodeFromFile(psState, pszFileName, pszDumpInfo, uLineInfo);
	}
}

IMG_INTERNAL
IMG_VOID DumpOrLoadIntermediate(PINTERMEDIATE_STATE psState, const IMG_CHAR *pszFileName,
								const IMG_CHAR *pszDumpInfo, IMG_UINT32 uLineInfo,
								const IMG_CHAR *pszComment, IMG_BOOL bStore)
/*****************************************************************************
 FUNCTION	: DumpOrLoadIntermediate
    
 PURPOSE	: 

 PARAMETERS	: psPredSrc		- Predicate to print.
			  bPredNegate	- NOT flag on the predicate source.
			  pszResult		- String to print to.
			  
 RETURNS	: The string.
*****************************************************************************/
{
	static IMG_UINT32 uSerial = 0;
	IMG_CHAR acDumpFileName[256];
	
	/* Orderly name file */
	sprintf(acDumpFileName, "_icode_%03u_%s.txt", uSerial++, pszFileName);
	if(bStore)
	{
		DumpIntermediateToFile(psState, acDumpFileName, pszComment, pszDumpInfo, uLineInfo, IMG_FALSE);
	}
	else
	{
		LoadIntermediateCodeFromFile(psState, acDumpFileName, pszDumpInfo, uLineInfo);
	}
}
#endif /* defined(SUPPORT_ICODE_SERIALIZATION) */

#if defined(UF_TESTBENCH)
static IMG_PCHAR PrintPredicate(PARG psPredSrc, IMG_BOOL bPredNegate, IMG_PCHAR pszResult)
/*****************************************************************************
 FUNCTION	: PrintPredicate
    
 PURPOSE	: Print a text representation of an instruction's source predicate.

 PARAMETERS	: psPredSrc		- Predicate to print.
			  bPredNegate	- NOT flag on the predicate source.
			  pszResult		- String to print to.
			  
 RETURNS	: The string.
*****************************************************************************/
{
	if (psPredSrc == NULL)
	{
		*pszResult = '\0';
	}
	else if (psPredSrc->uNumber == USC_PREDREG_PNMOD4)
	{
		if	(!bPredNegate)
		{
			strcpy(pszResult, "pN");
		}
		else
		{
			strcpy(pszResult, "p?");
		}
	}
	else
	{
		sprintf(pszResult, "%sp%u", bPredNegate ? "!" : "", psPredSrc->uNumber);
	}
	return pszResult;
}

static IMG_PCHAR PrintPredicates(PARG* apsPredSrc, IMG_UINT32 uPredCount, IMG_BOOL bPredNegate, IMG_PCHAR pszResult)
/*****************************************************************************
 FUNCTION	: PrintPredicates
    
 PURPOSE	: Print a text representation of an instruction's array of source predicates.

 PARAMETERS	: apsPredSrc	- Predicates to print.
			  uPredCount	- Number of predicates to print.
			  bPredNegate	- NOT flag on the predicate source.
			  pszResult		- String to print to.
			  
 RETURNS	: The string.
*****************************************************************************/
{
	IMG_UINT32 uPred;

	*pszResult = '\0';

	for (uPred = 0; uPred < uPredCount; ++uPred)
	{
		IMG_CHAR pszPredString[6];
		PrintPredicate(apsPredSrc[uPred], bPredNegate, pszPredString);

		strcat(pszResult, pszPredString);
		if (uPred < uPredCount - 1)
		{
			strcat(pszResult, ",");
		}
	}

	return pszResult;
}

static IMG_BOOL ArgUsedFromSmp(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uArg)
/*****************************************************************************
 FUNCTION	: ArgUsedFromSmp
    
 PURPOSE	: Check if an argument to a texture sample instruction is referenced.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Texture sample instruction.
			  uArg			- Index of the argument to check.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);

	/* All USP sample instructions use 2 or 3 coordinate args */
	#if defined(OUTPUT_USPBIN)
	if(psInst->eOpcode != ISMP_USP_NDR)
	{
	#endif
		if	(
				(uArg >= SMP_COORD_ARG_START) && 
				(uArg < (SMP_COORD_ARG_START + psInst->u.psSmp->uCoordSize))
			)
		{
			return IMG_TRUE;
		}
	#if defined(OUTPUT_USPBIN)
	}
	else
	{	
		if	(
				(uArg >= NON_DEP_SMP_TFDATA_ARG_START) && 
				(uArg < (NON_DEP_SMP_COORD_ARG_START + psInst->u.psSmp->uCoordSize))
			)
		{
			return IMG_TRUE;			
		}
		else			
		{
			return IMG_FALSE;
		}	
	}
	#endif

	/* Is projection enabled for the SMP? */
	if (psInst->u.psSmp->bProjected && uArg == SMP_PROJ_ARG)
	{
		return IMG_TRUE;
	}

	/* Is the texture to sample a texture array? */
	if (psInst->u.psSmp->bTextureArray && uArg == SMP_ARRAYINDEX_ARG)
	{
		return IMG_TRUE;
	}

	/* All USP sample instructions use a DRC register */
	if	(uArg == SMP_DRC_ARG_START)
	{
		return IMG_TRUE;
	}

	#if defined(OUTPUT_USCHW)
	/*
		Texture state is always referenced.
	*/
	if (!(psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN))
	{
		if (
				uArg >= SMP_STATE_ARG_START && 
				uArg < (SMP_STATE_ARG_START + psState->uTexStateSize)
		   )
		{
			return IMG_TRUE;
		}
	}
	#endif /* defined(OUPUT_USCHW) */

	/* LOD source is only used by bias or replace USP sample instructions */
	if	(uArg == SMP_LOD_ARG_START)
	{
		if	(
				 #if defined(OUTPUT_USPBIN)
				 (psInst->eOpcode == ISMPBIAS_USP) || 
				 (psInst->eOpcode == ISMPREPLACE_USP) ||
				 #endif /* defined(OUTPUT_USPBIN) */
				 (psInst->eOpcode == ISMPBIAS) ||
				 (psInst->eOpcode == ISMPREPLACE)
			)
		{
			return IMG_TRUE;
		}
	}

	/* Gradiant sources are only used by appropriate sample instructions */
	if	(
			(uArg >= SMP_GRAD_ARG_START) && 
			(uArg < (SMP_GRAD_ARG_START + psInst->u.psSmp->uGradSize))
		)
	{
		if	(
				 #if defined(OUTPUT_USPBIN)
				 (psInst->eOpcode == ISMPGRAD_USP) ||
				 #endif /* defined(OUTPUT_USPBIN) */
				 (psInst->eOpcode == ISMPGRAD)
			)
		{
			return IMG_TRUE;
		}
	}

	/* Sample Idx sources can be used by all sample instructions but will be set to unused soure type generally */
	if	(
			(uArg >= SMP_SAMPLE_IDX_ARG_START) && 
			(uArg < (SMP_SAMPLE_IDX_ARG_START + 1))
		)
	{
		return IMG_TRUE;		
	}

	/* All other args are unused */
	return IMG_FALSE;
}

#define _ADDSTR(S) sprintf(apcTempString + strlen(apcTempString), S)
#define _ADDSTRP1(S, P1) sprintf(apcTempString + strlen(apcTempString), S, P1)
#define _ADDSTRP2(S, P1, P2) sprintf(apcTempString + strlen(apcTempString), S, P1, P2)
#define _ADDSTRP3(S, P1, P2, P3) sprintf(apcTempString + strlen(apcTempString), S, P1, P2, P3)
#define _ADDSTRP4(S, P1, P2, P3, P4) sprintf(apcTempString + strlen(apcTempString), S, P1, P2, P3, P4)

static IMG_CHAR * const pszIntSrcSel[] = {"zero", 
										  "one", 
										  "srcalphasat", 
										  "rsrcalphasat", 
										  "src0", 
										  "src1", 
										  "src2", 
										  "src0alpha", 
										  "src1alpha", 
										  "src2alpha",
										  "CX1", 
										  "CX2", 
										  "CX4", 
										  "CX8", 
										  "AX1", 
										  "AX2", 
										  "AX4", 
										  "AX8", 
										  "+", 
										  "-", 
										  "neg", 
										  "none", 
										  " min ", 
										  " max "};

static IMG_VOID PrintTestType(PINTERMEDIATE_STATE psState, TEST_TYPE eTestType, IMG_PCHAR apcTempString)
/*****************************************************************************
 FUNCTION	: PrintTestType
    
 PURPOSE	: Print a text representation of an instruction's test type.

 PARAMETERS	: psState		- Compiler state.
			  peTest		- Test type.
			  apcTempString	- The text representation is appended to this 
							string.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	static const IMG_PCHAR pszTestType[TEST_TYPE_MAXIMUM] =
	{
		"invalid",	/* TEST_TYPE_INVALID */
		"true",		/* TEST_TYPE_ALWAYS_TRUE */
		"GTZ",		/* TEST_TYPE_GT_ZERO */
		"GTEQZ",	/* TEST_TYPE_GTE_ZERO */
		"EQZ",		/* TEST_TYPE_EQ_ZERO */
		"LTZ",		/* TEST_TYPE_LT_ZERO */
		"LTEQZ",	/* TEST_TYPE_LTE_ZERO */
		"NEQZ",		/* TEST_TYPE_NEQ_ZERO */
		"unsigned",	/* TEST_TYPE_SIGN_BIT_CLEAR */
		"signed",	/* TEST_TYPE_SIGN_BIT_SET */
	};

	ASSERT(eTestType < (sizeof(pszTestType) / sizeof(pszTestType[0])));
	_ADDSTRP1("_test(%s)", pszTestType[eTestType]);
}

static IMG_VOID PrintTest(PINTERMEDIATE_STATE psState, PTEST_DETAILS psTest, IMG_PCHAR apcTempString)
/*****************************************************************************
 FUNCTION	: PrintTest
    
 PURPOSE	: Print a text representation of an instruction's test type.

 PARAMETERS	: psState		- Compiler state.
			  psTest		- Test type.
			  apcTempString	- The text representation is appended to this 
							string.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	static const IMG_PCHAR pszChanSel[] = {"", ".c1", ".c2", ".c3", ".andall", ".orall", ".and02", ".or02"};

	PrintTestType(psState, psTest->eType, apcTempString);

	ASSERT(psTest->eChanSel < (sizeof(pszChanSel) / sizeof(pszChanSel[0])));
	_ADDSTRP1("%s", pszChanSel[psTest->eChanSel]);
}

static IMG_VOID PrintSOPSrc(IMG_PCHAR	apcTempString,
							IMG_UINT32	uSel,
							IMG_BOOL	bComplementSel)
{
	if (uSel == USEASM_INTSRCSEL_ZERO && bComplementSel)
	{
		_ADDSTR("one");
		return;
	}
	if (bComplementSel)
	{
		_ADDSTR("(1-");
	}
	if	(uSel < (sizeof(pszIntSrcSel) / sizeof(pszIntSrcSel[0])))
	{
		_ADDSTRP1("%s", pszIntSrcSel[uSel]);
	}
	else
	{
		_ADDSTR("Invalid");
	}
	if (bComplementSel)
	{
		_ADDSTR(")");
	}
}

static IMG_VOID PrintSOP(IMG_PCHAR apcTempString,
						 IMG_UINT32 uCOp,
						 IMG_UINT32 uCSel11,
						 IMG_BOOL bComplementCSel11,
						 IMG_UINT32 uCSel12,
						 IMG_BOOL bComplementCSel12,
						 IMG_UINT32 uCSel21,
						 IMG_BOOL bComplementCSel21,
						 IMG_UINT32 uCSel22,
						 IMG_BOOL bComplementCSel22,
						 IMG_UINT32 uAOp,
						 IMG_UINT32 uASel11,
						 IMG_BOOL bComplementASel11,
						 IMG_UINT32 uASel12,
						 IMG_BOOL bComplementASel12,
						 IMG_UINT32 uASel21,
						 IMG_BOOL bComplementASel21,
						 IMG_UINT32 uASel22,
						 IMG_BOOL bComplementASel22)
{
	
	_ADDSTR(", rgb=");
	PrintSOPSrc(apcTempString, uCSel11, bComplementCSel11);
	_ADDSTR("*");
	PrintSOPSrc(apcTempString, uCSel12, bComplementCSel12);

	if	(uCOp < (sizeof(pszIntSrcSel) / sizeof(pszIntSrcSel[0])))
	{
		_ADDSTRP1("%s", pszIntSrcSel[uCOp]);
	}
	else
	{
		_ADDSTR("Invalid");
	}

	PrintSOPSrc(apcTempString, uCSel21, bComplementCSel21);
	_ADDSTR("*");
	PrintSOPSrc(apcTempString, uCSel22, bComplementCSel22);

	_ADDSTR(", a=");
	PrintSOPSrc(apcTempString, uASel11, bComplementASel11);
	_ADDSTR("*");
	PrintSOPSrc(apcTempString, uASel12, bComplementASel12);

	if	(uAOp < (sizeof(pszIntSrcSel) / sizeof(pszIntSrcSel[0])))
	{
		_ADDSTRP1("%s", pszIntSrcSel[uAOp]);
	}
	else
	{
		_ADDSTR("Invalid");
	}

	PrintSOPSrc(apcTempString, uASel21, bComplementASel21);
	_ADDSTR("*");
	PrintSOPSrc(apcTempString, uASel22, bComplementASel22);
}

static IMG_CHAR * const g_pszEfoSrc[] = {"i0", "i1", "a0", "a1", "m0", "m1", "s0", "s1", "s2"};

IMG_INTERNAL IMG_CHAR * const g_pszRegType[] = 
{
	"r",				/* USEASM_REGTYPE_TEMP */
	"o",				/* USEASM_REGTYPE_OUTPUT */
	"pa",				/* USEASM_REGTYPE_PRIMATTR */
	"sa",				/* USEASM_REGTYPE_SECATTR */
	"idx",				/* USEASM_REGTYPE_INDEX */
	"g",				/* USEASM_REGTYPE_GLOBAL */
	"h",				/* USEASM_REGTYPE_FPCONSTANT */
	"i",				/* USEASM_REGTYPE_FPINTERNAL */
	"imm",				/* USEASM_REGTYPE_IMMEDIATE */
	"pclink",			/* USEASM_REGTYPE_LINK */
	"drc",				/* USEASM_REGTYPE_DRC */
	"label",			/* USEASM_REGTYPE_LABEL */
	"P",				/* USEASM_REGTYPE_PREDICATE */
	"clipplane",		/* USEASM_REGTYPE_CLIPPLANE */
	"addressmode",		/* USEASM_REGTYPE_ADDRESSMODE */
	"swizzle",			/* USEASM_REGTYPE_SWIZZLE */
	"intsrcsel",		/* USEASM_REGTYPE_INTSRCSEL */
	"fcs",				/* USEASM_REGTYPE_FILTERCOEFF */
	"label",			/* USEASM_REGTYPE_LABEL_WITH_OFFSET */
	"named",			/* USEASM_REGTYPE_TEMP_NAMED */
	"floatimmediate",	/* USEASM_REGTYPE_FLOATIMMEDIATE */
	"ref",              /* USEASM_REGTYPE_REF */
	"dummy",			/* USC_REGTYPE_DUMMY */
	"dpaccum",			/* USC_REGTYPE_DPACCUM */
	"regarray",			/* USC_REGTYPE_REGARRAY */
	"arraybase",		/* USC_REGTYPE_ARRAYBASE */
	"unusedsource",		/* USC_REGTYPE_UNUSEDSOURCE */
	"unuseddest",		/* USC_REGTYPE_UNUSEDDEST */
	"gsi",				/* USC_REGTYPE_GSINPUT */
	"sac",				/* USC_REGTYPE_CALCSECATTR */
	"noindex",			/* USC_REGTYPE_NOINDEX */
	"boolean",			/* USC_REGTYPE_BOOLEAN */
	"<invalid>",    	/* USC_REGTYPE_MAXIMUM */
};

static IMG_VOID PrintUseasmSwizzle(IMG_PCHAR	apcTempString,
								   IMG_UINT32	uSwizzle)
/*****************************************************************************
 FUNCTION	: PrintUseasmSwizzle
    
 PURPOSE	: Append a text representation of a USEASM swizzle to a string.

 PARAMETERS	: apcTempString			- String to append to.
			  uSwizzle				- Swizzle to print.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	static const IMG_PCHAR pszChan[] = {"x"		/* USEASM_SWIZZLE_SEL_X */, 
										"y"		/* USEASM_SWIZZLE_SEL_Y */, 
										"z"		/* USEASM_SWIZZLE_SEL_Z */, 
										"w"		/* USEASM_SWIZZLE_SEL_W */, 
										"0"		/* USEASM_SWIZZLE_SEL_0 */, 
										"1"		/* USEASM_SWIZZLE_SEL_1 */, 
										"2"		/* USEASM_SWIZZLE_SEL_2 */, 
										"half"	/* USEASM_SWIZZLE_SEL_HALF */};
	_ADDSTRP4("swizzle(%s%s%s%s)", 
			  pszChan[(uSwizzle >> (0 * USEASM_SWIZZLE_FIELD_SIZE)) & USEASM_SWIZZLE_VALUE_MASK], 
			  pszChan[(uSwizzle >> (1 * USEASM_SWIZZLE_FIELD_SIZE)) & USEASM_SWIZZLE_VALUE_MASK],
			  pszChan[(uSwizzle >> (2 * USEASM_SWIZZLE_FIELD_SIZE)) & USEASM_SWIZZLE_VALUE_MASK],
			  pszChan[(uSwizzle >> (3 * USEASM_SWIZZLE_FIELD_SIZE)) & USEASM_SWIZZLE_VALUE_MASK]);
}

static IMG_VOID BasePrintRegister(PINTERMEDIATE_STATE	psState,
								  IMG_PCHAR				apcTempString,
								  PINST					psInst,
								  IMG_UINT32			uType,
								  IMG_UINT32			uNumber,
								  IMG_UINT32			uArrayOffset)
{
	ASSERT(uArrayOffset == 0 || (uType == USC_REGTYPE_ARRAYBASE || uType == USC_REGTYPE_REGARRAY));

	switch (uType)
	{
		case USEASM_REGTYPE_INDEX:
		{
			if (uNumber == USC_INDEXREG_LOW)
			{
				_ADDSTR("idx.l");
			}
			else
			{
				ASSERT(uNumber == USC_INDEXREG_HIGH);
				_ADDSTR("idx.h");
			}
			break;
		}
		case USEASM_REGTYPE_SWIZZLE:
		{
			PrintUseasmSwizzle(apcTempString, uNumber);
			break;
		}
		case USEASM_REGTYPE_IMMEDIATE:
		{
			if (psInst->eOpcode == ISMLSI || psInst->eOpcode == ISMR || psInst->eOpcode == ISETFC || psInst->eOpcode == ISMBO)
			{
				_ADDSTRP1("#%d", (IMG_INT32)uNumber);
			}
			else if (uNumber < 128)
			{
				_ADDSTRP1("#%u", uNumber);
			}
			else
			{
				_ADDSTRP1("#0x%.8X", uNumber);
			}
			break;
		}
		case USC_REGTYPE_ARRAYBASE:
		{
			_ADDSTRP2("&R%u(+%u)", uNumber, uArrayOffset);
			break;
		}	
		case USC_REGTYPE_REGARRAY:
		{
			if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_GEOMETRY &&
				uNumber == psState->sShader.psVS->uVerticesBaseInternArrayIdx)
			{
				_ADDSTRP1("VB(+%u)", uArrayOffset);
			}
			else
			{
				_ADDSTRP2("R%u(+%u)", uNumber, uArrayOffset);
			}
			break;
		}
		case USC_REGTYPE_UNUSEDSOURCE:
		{
			_ADDSTR("unused");
			break;
		}
		case USC_REGTYPE_UNUSEDDEST:
		{
			_ADDSTR("__");
			break;
		}
		case USC_REGTYPE_CALCSECATTR:
		{
			_ADDSTRP1("sac%u", uNumber);
			break;
		}
		case USC_REGTYPE_BOOLEAN:
		{
			if (uNumber)
			{
				_ADDSTR("true");
			}
			else
			{
				_ADDSTR("false");
			}
			break;
		}
		default:
		{
			ASSERT(uType < (sizeof(g_pszRegType) / sizeof(g_pszRegType[0])));
			_ADDSTRP2("%s%u", g_pszRegType[uType], uNumber);	
			break;
		}
	}
}

static IMG_VOID PrintIndex(PINTERMEDIATE_STATE	psState,
						   IMG_PCHAR			apcTempString,
						   PINST				psInst,
						   PARG					psArg)
{
	if (psArg->uIndexType != USC_REGTYPE_NOINDEX)
	{
		_ADDSTR("[");
		BasePrintRegister(psState, apcTempString, psInst, psArg->uIndexType, psArg->uIndexNumber, psArg->uIndexArrayOffset);
		_ADDSTRP1(", stride=%d]", psArg->uIndexStrideInBytes);
	}
	else
	{
		ASSERT(psArg->uIndexNumber == USC_UNDEF);
		ASSERT(psArg->uIndexArrayOffset == USC_UNDEF);
		ASSERT(psArg->uIndexStrideInBytes == USC_UNDEF);
	}
}

static IMG_VOID PrintMask(IMG_PCHAR	apcTempString, IMG_UINT32 uDestMask, IMG_UINT32 uLiveChansInDest)
{
	if	(uDestMask != USC_DESTMASK_FULL)
	{
		_ADDSTRP4("_bytemask%d%d%d%d", (uDestMask & 8) ? 1 : 0, (uDestMask & 4) ? 1 : 0, (uDestMask & 2) ? 1 : 0, (uDestMask & 1) ? 1 : 0);
	}
	if	(uLiveChansInDest != USC_DESTMASK_FULL)
	{
		_ADDSTRP4("(_live%d%d%d%d)", (uLiveChansInDest & 8) ? 1 : 0, (uLiveChansInDest & 4) ? 1 : 0, (uLiveChansInDest & 2) ? 1 : 0, (uLiveChansInDest & 1) ? 1 : 0);
	}
}

static IMG_VOID PrintMaskLetters(IMG_PCHAR	apcTempString, IMG_UINT32 uDestMask, IMG_UINT32 uLiveChansInDest)
{
	if	(uDestMask != USC_DESTMASK_FULL)
	{
		_ADDSTRP4(".%s%s%s%s", (uDestMask & 1) ? "x" : "", (uDestMask & 2) ? "y" : "", (uDestMask & 4) ? "z" : "", (uDestMask & 8) ? "w" : "");
	}
	if	(uLiveChansInDest != USC_DESTMASK_FULL)
	{
		_ADDSTRP4("(live=%s%s%s%s)", (uLiveChansInDest & 1) ? "x" : "", (uLiveChansInDest & 2) ? "y" : "", (uLiveChansInDest & 4) ? "z" : "", (uLiveChansInDest & 8) ? "w" : "");
	}
}

static IMG_VOID PrintRegister(IMG_PCHAR				apcTempString,
							  PINTERMEDIATE_STATE	psState,
							  PINST					psInst,
							  PARG					psArg)
{
	BasePrintRegister(psState, apcTempString, psInst, psArg->uType, psArg->uNumber, psArg->uArrayOffset);
	PrintIndex(psState, apcTempString, psInst, psArg);
	if	(psArg->eFmt != UF_REGFORMAT_F32)
	{
		if(psArg->eFmt == UF_REGFORMAT_INVALID)
		{
			_ADDSTR(".fmt(invalid)");
		}
		else
		{
			_ADDSTRP1(".fmt%s", g_pszRegFormat[psArg->eFmt]);
		}
	} 
}

static IMG_VOID PrintDestRange(PINTERMEDIATE_STATE	psState,
							   IMG_PCHAR			apcTempString,
							   PINST				psInst,
							   IMG_UINT32			uRangeStart,
							   IMG_UINT32			uRangeEnd)
{
	IMG_BOOL	bPlus;
	IMG_UINT32	uDestOffset;

	bPlus = IMG_FALSE;
	for (uDestOffset = uRangeStart; uDestOffset < uRangeEnd; uDestOffset++)
	{
		#if defined(SUPPORT_SGX545)
		/*
			For IDUAL show the secondary destination on the next line
			with the secondary operation.
		*/
		if (
				psInst->eOpcode == IDUAL &&
				(
					uDestOffset == 1 ||
					(psInst->u.psDual->ePrimaryOp != IFDDP && uDestOffset == 2)
				)
		   )
		{
			continue;
		}
		#endif /* defined(SUPPORT_SGX545) */
		if (
				psInst->eOpcode == IEFO &&
				(
					(
						uDestOffset == EFO_US_DEST &&
						psInst->u.psEfo->bIgnoreDest
					) ||
					psInst->asDest[uDestOffset].uType == USC_REGTYPE_UNUSEDDEST
				)
		   )
		{
			continue;
		}
		if (
				psInst->eOpcode == IIMAE &&
				uDestOffset == IMAE_CARRYOUT_DEST &&
				psInst->asDest[uDestOffset].uType == USC_REGTYPE_UNUSEDDEST
			)
		{
			continue;
		}
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if ((psInst->eOpcode == IVTEST) && (psInst->asDest[uDestOffset].uType == USC_REGTYPE_UNUSEDDEST))
		{
			continue;
		}
#endif //defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)

		if	(bPlus)
		{
			_ADDSTR("+");
		}

		#if defined(OUTPUT_USPBIN)
		if	(
				(psInst->eOpcode >= ISMP_USP) &&
				(psInst->eOpcode <= ISMP_USP_NDR) &&
				psInst->auDestMask[uDestOffset] == 0
			)
		{
			_ADDSTR("__");
			continue;
		}
		else
		#endif /* defined(OUTPUT_USPBIN) */
		{
			PrintRegister(apcTempString, psState, psInst, &psInst->asDest[uDestOffset]);
		}

		/*
			Show information about where the data written to each EFO destination
			comes from.
		*/
		if (psInst->eOpcode == IEFO)
		{
			switch (uDestOffset)
			{
				case EFO_US_DEST: _ADDSTRP1("=%s", g_pszEfoSrc[psInst->u.psEfo->eDSrc]); break;
				case EFO_USFROMI0_DEST: _ADDSTR("=i0"); break;
				case EFO_USFROMI1_DEST: _ADDSTR("=i1"); break;
				case EFO_I0_DEST: _ADDSTRP1("=%s", g_pszEfoSrc[psInst->u.psEfo->eI0Src]); break;
				case EFO_I1_DEST: _ADDSTRP1("=%s", g_pszEfoSrc[psInst->u.psEfo->eI1Src]); break;
				default: imgabort();
			}
		}
		
		if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORDEST)
		{
			PrintMaskLetters(apcTempString, psInst->auDestMask[uDestOffset], psInst->auLiveChansInDest[uDestOffset]);
		}
		else
		{
			PrintMask(apcTempString, psInst->auDestMask[uDestOffset], psInst->auLiveChansInDest[uDestOffset]);
		}

		if (psInst->apsOldDest[uDestOffset] != NULL)
		{
			_ADDSTR("[=");
			PrintRegister(apcTempString, psState, psInst, psInst->apsOldDest[uDestOffset]);
			_ADDSTR("]");
		}

		bPlus = IMG_TRUE;
	}
}

IMG_VOID static PrintDest(PINTERMEDIATE_STATE		psState,
						  IMG_PCHAR					apcTempString,
						  PINST						psInst)
{
	if (psInst->uDestCount == 0)
	{
		_ADDSTR("!");
		return;
	}

	PrintDestRange(psState, apcTempString, psInst, 0 /* uRangeStart */, psInst->uDestCount /* uRangeEnd */);
}

IMG_VOID static PrintFloatSourceModifier(PINTERMEDIATE_STATE	psState,
										 IMG_PCHAR				apcTempString,
										 PINST					psInst,
										 IMG_UINT32				uSrcIdx,
										 PFLOAT_SOURCE_MODIFIER	psMod)
/*****************************************************************************
 FUNCTION	: PrintFloatSourceModifier
    
 PURPOSE	: Append a description of the modifiers on a floating point instruction source
			  to a string.

 PARAMETERS	: psState				- Compiler state.
			  apcTempString			- String to append to.
			  psInst				- Instruction whose source modifiers are to
									be printed.
			  uSrcIdx				- Index of the source whose modifiers are to
									be printed.
			  psMod					- Floating point source modifiers to print.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (
			(
				psInst->asArg[uSrcIdx].eFmt == UF_REGFORMAT_F16 &&
				g_psInstDesc[psInst->eOpcode].eType != INST_TYPE_FARITH16
			) ||
			psInst->asArg[uSrcIdx].eFmt == UF_REGFORMAT_C10
	   )
	{
		_ADDSTRP1(".chan%u", psMod->uComponent);
	}
	else
	{
		ASSERT(psMod->uComponent == 0);
	}
	if (psMod->bAbsolute)
	{
		_ADDSTR(".abs");
	}
	if (psMod->bNegate)
	{
		_ADDSTR(".neg");
	}
}

IMG_VOID static PrintSourceModifiersBeforeSource(IMG_PCHAR				apcTempString,
												 PINST					psInst,
												 IMG_UINT32				uSrcIdx)
/*****************************************************************************
 FUNCTION	: PrintSourceModifiersBeforeSource
    
 PURPOSE	: Append a description of the modifiers on an instruction source
			  to a string before the source register is printed.

 PARAMETERS	: apcTempString			- String to append to.
			  psInst				- Instruction whose source modifiers are to
									be printed.
			  uSrcIdx				- Index of the source whose modifiers are to
									be printed.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	switch (g_psInstDesc[psInst->eOpcode].eType)
	{
		case INST_TYPE_LDST:
		{
			if (uSrcIdx == 1)
			{
				if (psInst->u.psLdSt->bPreIncrement)
				{
					if (psInst->u.psLdSt->bPositiveOffset)
					{
						_ADDSTR("++");
					}
					else
					{
						_ADDSTR("--");
					}
				}
				else
				{
					if (!psInst->u.psLdSt->bPositiveOffset)
					{
						_ADDSTR("-");
					}
				}
				break;
			}
		}
		default:
		{
			break;
		}
	}
}

IMG_VOID static PrintSourceModifiers(PINTERMEDIATE_STATE	psState,
									 IMG_PCHAR				apcTempString,
									 PINST					psInst,
									 IMG_UINT32				uSrcIdx)
/*****************************************************************************
 FUNCTION	: PrintSourceModifiers
    
 PURPOSE	: Append a description of the modifiers on an instruction source
			  to a string after the source register is printed.

 PARAMETERS	: psState				- Compiler state.
			  apcTempString			- String to append to.
			  psInst				- Instruction whose source modifiers are to
									be printed.
			  uSrcIdx				- Index of the source whose modifiers are to
									be printed.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	switch (g_psInstDesc[psInst->eOpcode].eType)
	{
		case INST_TYPE_FARITH16:
		{
			static IMG_PCHAR const pszF16Swizzle[] = {"", ".swzll", ".swzhh", ".swzf32"};

			FMAD16_SWIZZLE	eSwiz = psInst->u.psArith16->aeSwizzle[uSrcIdx];
			if (eSwiz != FMAD16_SWIZZLE_LOWHIGH)
			{
				_ADDSTRP1("%s", pszF16Swizzle[eSwiz]);
			}
			PrintFloatSourceModifier(psState, 
									 apcTempString, 
									 psInst, 
									 uSrcIdx, 
									 &psInst->u.psArith16->sFloat.asSrcMod[uSrcIdx]);
			break;
		}
		case INST_TYPE_FLOAT:
		{
			PrintFloatSourceModifier(psState, 
									 apcTempString, 
									 psInst, 
									 uSrcIdx, 
									 &psInst->u.psFloat->asSrcMod[uSrcIdx]);
			break;
		}
		case INST_TYPE_DPC:
		{
			PrintFloatSourceModifier(psState, 
									 apcTempString, 
									 psInst, 
									 uSrcIdx, 
									 &psInst->u.psDpc->sFloat.asSrcMod[uSrcIdx]);
			break;
		}
		case INST_TYPE_EFO:
		{
			if (uSrcIdx < EFO_HARDWARE_SOURCE_COUNT)
			{
				PrintFloatSourceModifier(psState, 
										 apcTempString, 
										 psInst, 
										 uSrcIdx, 
										 &psInst->u.psEfo->sFloat.asSrcMod[uSrcIdx]);
			}
			break;
		}
		#if defined(SUPPORT_SGX545)
		case INST_TYPE_DUAL:
		{
			if (uSrcIdx < DUAL_MAX_SEC_SOURCES)
			{
				if (psInst->u.psDual->abPrimarySourceNegate[uSrcIdx])
				{
					_ADDSTR(".neg");
				}
				if (psInst->asArg[uSrcIdx].eFmt == UF_REGFORMAT_F16)
				{
					_ADDSTRP1(".chan%u", psInst->u.psDual->auSourceComponent[uSrcIdx]);
				}
			}
			break;
		}
		#endif /* defined(SUPPORT_SGX545) */
		case INST_TYPE_FPMA:
		{
			if (uSrcIdx == 0 && psInst->u.psFpma->bNegateSource0)
			{
				_ADDSTR(".neg");
			}
			break;
		}
		case INST_TYPE_IMA16:
		{
			if (uSrcIdx == 2 && psInst->u.psIma16->bNegateSource2)
			{
				_ADDSTR(".neg");
			}
			break;
		}
		case INST_TYPE_IMAE:
		{
			if (uSrcIdx < IMAE_UNIFIED_STORE_SOURCE_COUNT)
			{
				if (psInst->u.psImae->auSrcComponent[uSrcIdx] > 0)
				{
					_ADDSTRP1(".chan%u", psInst->u.psImae->auSrcComponent[uSrcIdx]);
				}
			}
			break;
		}
		case INST_TYPE_PCK:
		{
			_ADDSTRP1(".chan%u", psInst->u.psPck->auComponent[uSrcIdx]);
			break;
		}
		case INST_TYPE_MOVP:
		{
			if (psInst->u.psMovp->bNegate)
			{
				_ADDSTR(".neg");
			}
			break;
		}
		case INST_TYPE_BITWISE:
		{
			if (uSrcIdx == 1 && psInst->u.psBitwise->bInvertSrc2)
			{
				_ADDSTR(".invert");
			}
			break;
		}
		case INST_TYPE_TEST:
		{
			if ((g_psInstDesc[psInst->u.psTest->eAluOpcode].uFlags & DESC_FLAGS_F16F32SELECT) != 0)
			{
				if (psInst->asArg[uSrcIdx].eFmt == UF_REGFORMAT_F16)
				{
					_ADDSTRP1(".chan%u", psInst->u.psTest->auSrcComponent[uSrcIdx]);
				}
			}
			break;
		}
		case INST_TYPE_FDOTPRODUCT:
		{
			IMG_UINT32	uSrcModIdx;

			if (psInst->eOpcode == IFDDP_RPT)
			{
				uSrcModIdx = uSrcIdx % FDDP_PER_ITERATION_ARGUMENT_COUNT;
			}
			else
			{
				uSrcModIdx = uSrcIdx % FDP_RPT_PER_ITERATION_ARGUMENT_COUNT;
			}
			if (psInst->u.psFdp->abAbsolute[uSrcModIdx])
			{
				_ADDSTR(".abs");
			}
			if (psInst->u.psFdp->abNegate[uSrcModIdx])
			{
				_ADDSTR(".neg");
			}
			if (psInst->asArg[uSrcIdx].eFmt == UF_REGFORMAT_F16)
			{
				_ADDSTRP1(".chan%d", psInst->u.psFdp->auComponent[uSrcIdx]);
			}
			break;
		}
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case INST_TYPE_VEC:
		{
			if (psInst->u.psVec->asSrcMod[uSrcIdx].bAbs)
			{
				_ADDSTR(".abs");
			}
			if (psInst->u.psVec->asSrcMod[uSrcIdx].bNegate)
			{
				_ADDSTR(".neg");
			}

			_ADDSTR(".");
			PrintUseasmSwizzle(apcTempString, psInst->u.psVec->auSwizzle[uSrcIdx]);
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		default:
		{
			break;
		}
	}
}

static
IMG_VOID PrintInstModifier(PINTERMEDIATE_STATE psState, PINST psInst, IMG_PCHAR apcTempString)
/*****************************************************************************
 FUNCTION	: PrintInstModifier
    
 PURPOSE	: Prints a text representation of the modifiers on an instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to print modifiers from.
			  apcTempString		- The text representation is appended to this
								string.

 RETURNS	: Nothing.
*****************************************************************************/
{
	switch (psInst->eOpcode)
	{
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case IIMA32:
		{
			if ((psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_IMA32_32X16_PLUS_32) != 0)
			{
				if (psInst->u.psIma32->uStep == 2)
				{
					_ADDSTR(".s2");
				}
				else
				{
					ASSERT(psInst->u.psIma32->uStep == 1);
					_ADDSTR(".s1");
				}
			}
			break;
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

		case ITESTMASK:
		case ITESTPRED:
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case IFPTESTPRED_VEC:
		case IFPTESTMASK_VEC:
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			PrintTest(psState, &psInst->u.psTest->sTest, apcTempString);
			break;
		}

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case IVTEST:
		{
			PVTEST_PARAMS			psTest = &psInst->u.psVec->sTest;
			static const IMG_PCHAR	pszChanSel[] =
			{
				"invalid",
				".selx",			/* VTEST_CHANSEL_XCHAN */
				".perchan",			/* VTEST_CHANSEL_PERCHAN */
				".andall",			/* VTEST_CHANSEL_ANDALL */
				".orall",			/* VTEST_CHANSEL_ORALL */
			};

			PrintTestType(psState, psTest->eType, apcTempString);

			ASSERT(psTest->eChanSel < (sizeof(pszChanSel) / sizeof(pszChanSel[0])));
			_ADDSTRP1("%s", pszChanSel[psTest->eChanSel]);
			break;
		}
		case IVMOVC:
		case IVMOVCU8_FLT:
		{
			PrintTestType(psState, psInst->u.psVec->eMOVCTestType, apcTempString);
			break;
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

		case IMOVC:
		case IMOVC_C10:
		case IMOVC_U8:
		case IMOVC_I32:
		case IMOVC_I16:
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case IVMOVC_I32:
		case IVMOVCU8_I32:
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			PrintTestType(psState, psInst->u.psMovc->eTest, apcTempString);
			break;
		}
		case IFPDOT:
		case IFP16DOT:
		{
			if (psInst->u.psDot34->uVecLength == 3)
			{
				_ADDSTR("3");
			}
			else
			{
				ASSERT(psInst->u.psDot34->uVecLength == 4);
				_ADDSTR("4");
			}
			if (psInst->u.psDot34->bOffset)
			{
				_ADDSTR("OFF");
			}
			if (psInst->u.psDot34->uDot34Scale > 0)
			{
				_ADDSTRP1("_x%d", 1 << psInst->u.psDot34->uDot34Scale);
			}
			break;
		}
		case IEMIT:
		{
			if (psInst->u.sEmit.bCut)
			{
				_ADDSTR(".CUT");
			}
			if((psState->uFlags2 & USC_FLAGS2_TWO_PARTITION_MODE) != 0)
			{
				_ADDSTR(".TWOPARTITION");
			}
			break;
		}
		case IWOP:
		{			
			if((psState->uFlags2 & USC_FLAGS2_TWO_PARTITION_MODE) != 0)
			{
				_ADDSTR(".TWOPARTITION");
			}
			break;
		}

		case ISMP:
		case ISMPBIAS:
		case ISMPREPLACE:
		case ISMPGRAD:
		{
			static const IMG_PCHAR	apszReturnData[] = 
			{
				"",			/* SMP_RETURNDATA_NORMAL */
				".SINF",	/* SMP_RETURNDATA_SAMPLEINFO */
				".RAWSAMP",	/* SMP_RETURNDATA_RAWSAMPLES */
				".BOTH",	/* SMP_RETURNDATA_BOTH */
			};
	
			ASSERT(psInst->u.psSmp->eReturnData < (sizeof(apszReturnData) / sizeof(apszReturnData[0])));
			_ADDSTRP1("%s", apszReturnData[psInst->u.psSmp->eReturnData]);
			break;
		}
		case ILDARRF32:
		case ILDARRF16:
		case ILDARRC10:
		case ILDARRU8:
		case ISTARRF32:
		case ISTARRF16:
		case ISTARRC10:
		case ISTARRU8:
		{
			if (psInst->u.psLdStArray->uLdStMask > 0)
			{
				_ADDSTRP4("_%s%s%s%s",
						  (psInst->u.psLdStArray->uLdStMask & USC_X_CHAN_MASK) ? "x" : "_",
						  (psInst->u.psLdStArray->uLdStMask & USC_Y_CHAN_MASK) ? "y" : "_",
						  (psInst->u.psLdStArray->uLdStMask & USC_Z_CHAN_MASK) ? "z" : "_",
						  (psInst->u.psLdStArray->uLdStMask & USC_W_CHAN_MASK) ? "w" : "_");
			}
			break;
		}
		default:
		{
			break;
		}
	}

}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_VOID PrintVectorSourceSlot(PINTERMEDIATE_STATE psState, PINST psInst, IMG_CHAR* apcTempString, IMG_UINT32 uSlot)
/*****************************************************************************
 FUNCTION	: PrintVectorSourceSlot
    
 PURPOSE	: Print an vector source slot for an instruction along with swizzle 
			  and source modifiers.

 PARAMETERS	: psInst		- Instruction to print.
			  acTempBuffer	- Buffer for output
			  uSlot			- Slot number

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32 uArgStart = uSlot * 5;
	PARG psGPIArg = &psInst->asArg[uArgStart + 0];
	
	ASSERT(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC);
	
	if (psGPIArg->uType != USC_REGTYPE_UNUSEDSOURCE)
	{
		ASSERT(psGPIArg->eFmt == psInst->u.psVec->aeSrcFmt[uSlot]);
		PrintRegister(apcTempString, psState, psInst, psGPIArg);
	}
	else
	{
		IMG_UINT32	uCompIdx;
		IMG_UINT32	uNumComponents;

		if (psInst->u.psVec->aeSrcFmt[uSlot] == UF_REGFORMAT_F32)
		{
			uNumComponents = 4;
		}
		else
		{
			ASSERT(psInst->u.psVec->aeSrcFmt[uSlot] == UF_REGFORMAT_F16 || psInst->u.psVec->aeSrcFmt[uSlot] == UF_REGFORMAT_C10);
			uNumComponents = 2;
		}

		_ADDSTR("(");
		for (uCompIdx = 0; uCompIdx < uNumComponents; uCompIdx++)
		{
			PARG	psUSArg = &psInst->asArg[uArgStart + 1 + uCompIdx];

			ASSERT(psUSArg->uType == USC_REGTYPE_UNUSEDSOURCE || 
				   psUSArg->eFmt == psInst->u.psVec->aeSrcFmt[uSlot] ||
				   psUSArg->eFmt == UF_REGFORMAT_UNTYPED);

			PrintRegister(apcTempString, psState, psInst, psUSArg);
			if (uCompIdx < (uNumComponents - 1))
			{
				_ADDSTR(", ");
			}
		}
		_ADDSTR(")");
	}
	_ADDSTR(".");
	PrintUseasmSwizzle(apcTempString, psInst->u.psVec->auSwizzle[uSlot]);
	if (psInst->u.psVec->asSrcMod[uSlot].bAbs)
	{
		_ADDSTR(".abs");
	}
	if (psInst->u.psVec->asSrcMod[uSlot].bNegate)
	{
		_ADDSTR(".neg");
	}
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

IMG_INTERNAL
IMG_VOID PrintIntermediateInst(PINTERMEDIATE_STATE psState, 
							   PINST psInst, 
							   IMG_CHAR* pszNum,
							   IMG_CHAR* apcTempString)
/*****************************************************************************
 FUNCTION	: PrintIntermediateInst
    
 PURPOSE	: Print an intermediate instructions.

 PARAMETERS	: psInst		- Instruction to print.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32 i;
	IMG_UINT32 uArgCount = 0;
	IMG_UINT32 uArgStart = 0;
	IMG_BOOL bComma = IMG_FALSE;
	IMG_UINT32 uSpaces;
	IMG_CHAR pszPredResult[24];
	IMG_BOOL bInstMoeIncrement;

	/* Check for SGX545 features */
	bInstMoeIncrement = IMG_FALSE;
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_PER_INST_MOE_INCREMENTS)
	{
		bInstMoeIncrement = IMG_TRUE;
	}

	apcTempString[0] = '\0';

	if (pszNum != NULL)
	{
		_ADDSTRP1("%3s ", pszNum);
	}
	else
	{
		_ADDSTR("    ");
	}

	if (GetBit(psInst->auFlag, INST_NOSCHED))
	{
		_ADDSTR("NS ");
	}
	else
	{
		_ADDSTR("   ");
	}
	if (GetBit(psInst->auFlag, INST_SKIPINV))
	{
		_ADDSTR("SI ");
	}
	else
	{
		_ADDSTR("   ");
	}
	
	//fix indent at 4.
	_ADDSTRP1("%-6s    ",  PrintPredicates(psInst->apsPredSrc, 
										   psInst->uPredCount,
										   GetBit(psInst->auFlag, INST_PRED_NEG),
										   pszPredResult));

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psInst->eOpcode == IVDUAL)
	{
		_ADDSTRP4("IVDUAL[%s%s+%s%s]", 
				  (psInst->u.psVec->sDual.bPriUsesGPIDest) ? "*" : "", g_asDualIssueOpDesc[psInst->u.psVec->sDual.ePriOp].pszName, 
				  (psInst->u.psVec->sDual.bPriUsesGPIDest) ? "" : "*", g_asDualIssueOpDesc[psInst->u.psVec->sDual.eSecOp].pszName);
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	#if defined(SUPPORT_SGX545)
	if (psInst->eOpcode == IDUAL)
	{
		_ADDSTRP1("%s", g_psInstDesc[psInst->u.psDual->ePrimaryOp].pszName);

		uArgCount = g_psInstDesc[psInst->u.psDual->ePrimaryOp].uDefaultArgumentCount;

		if (psInst->u.psDual->ePrimaryOp == IFSSQ ||
			psInst->u.psDual->ePrimaryOp == IMOV)
		{
			uArgStart = 1;
			uArgCount++;
		}
	}
	else
	#endif /* defined(SUPPORT_SGX545) */
	if (g_psInstDesc[psInst->eOpcode].eType == INST_TYPE_TEST)
	{
		IOPCODE	eAluOpcode = psInst->u.psTest->eAluOpcode;

		if (psInst->eOpcode == ITESTMASK)
		{
			_ADDSTRP1("TM[%s]", g_psInstDesc[eAluOpcode].pszName);
		}
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		else if (psInst->eOpcode == IVTEST)
		{
			_ADDSTRP1("VT[%s]", g_psInstDesc[eAluOpcode].pszName);
		}
		else if (psInst->eOpcode == IFPTESTPRED_VEC)
		{
			_ADDSTRP1("FPTP_VEC[%s]", g_psInstDesc[eAluOpcode].pszName);
		}
		else if (psInst->eOpcode == IFPTESTMASK_VEC)
		{
			_ADDSTRP1("FPTM_VEC[%s]", g_psInstDesc[eAluOpcode].pszName);
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		else
		{
			ASSERT(psInst->eOpcode == ITESTPRED);
			_ADDSTRP1("TP[%s]", g_psInstDesc[eAluOpcode].pszName);
		}

		uArgCount = g_psInstDesc[eAluOpcode].uDefaultArgumentCount;
	}
	else if (g_psInstDesc[psInst->eOpcode].eType == INST_TYPE_FDOTPRODUCT)
	{
		IMG_UINT32	uRepeatCount;

		_ADDSTRP1("%s", g_psInstDesc[psInst->eOpcode].pszName);

		uRepeatCount = psInst->u.psFdp->uRepeatCount;
		if (psInst->eOpcode == IFDDP_RPT)
		{
			uArgCount = uRepeatCount * FDDP_PER_ITERATION_ARGUMENT_COUNT;
		}
		else
		{
			uArgCount = uRepeatCount * FDP_RPT_PER_ITERATION_ARGUMENT_COUNT;
		}
	}
	else
	{
		_ADDSTRP1("%s", g_psInstDesc[psInst->eOpcode].pszName);
		uArgCount = psInst->uArgumentCount;
	}

	PrintInstModifier(psState, psInst, apcTempString);
	
	if (psInst->uRepeat > 0)
	{
		_ADDSTRP1("_rpt%u", psInst->uRepeat);

		if (bInstMoeIncrement && !GetBit(psInst->auFlag, INST_MOE))
		{
			/* Repeating using source increments */
			_ADDSTRP3("[%d%d%d]", 
					  GetBit(psInst->puSrcInc, 0),
					  GetBit(psInst->puSrcInc, 1),
					  GetBit(psInst->puSrcInc, 2));
		}
	}
	else if (psInst->uMask > 1)
	{
		_ADDSTRP4("_mask%s%s%s%s", (psInst->uMask & 1) ? "x" : "", (psInst->uMask & 2) ? "y" : "", (psInst->uMask & 4) ? "z" : "", (psInst->uMask & 8) ? "w" : "");
	}
	

	uSpaces = 31 - strlen(apcTempString);
	_ADDSTRP2("%-*s", (IMG_INT)uSpaces, "");

	if (!(psInst->uDestCount == 1 && psInst->asDest[0].uType == USC_REGTYPE_DUMMY))
	{
		PrintDest(psState, apcTempString, psInst);
		bComma = IMG_TRUE;
	}

	if (psInst->eOpcode == IEFO)
	{
		if (bComma)
		{
			_ADDSTR(", ");
		}
		if (psInst->u.psEfo->bA0Used)
		{
			_ADDSTRP3("a0=%s%s%s, ", g_pszEfoSrc[psInst->u.psEfo->eA0Src0], psInst->u.psEfo->bA0RightNeg ? "-" : "+", g_pszEfoSrc[psInst->u.psEfo->eA0Src1]);
		}
		if (psInst->u.psEfo->bA1Used)
		{
			_ADDSTRP3("a1=%s%s+%s, ", psInst->u.psEfo->bA1LeftNeg ? "-" : "", g_pszEfoSrc[psInst->u.psEfo->eA1Src0], g_pszEfoSrc[psInst->u.psEfo->eA1Src1]);
		}
		if (psInst->u.psEfo->bM0Used)
		{
			_ADDSTRP2("m0=%s*%s, ", g_pszEfoSrc[psInst->u.psEfo->eM0Src0], g_pszEfoSrc[psInst->u.psEfo->eM0Src1]);
		}
		if (psInst->u.psEfo->bM1Used)
		{
			_ADDSTRP2("m1=%s*%s, ", g_pszEfoSrc[psInst->u.psEfo->eM1Src0], g_pszEfoSrc[psInst->u.psEfo->eM1Src1]);
		}

		bComma = IMG_FALSE;
	}	

	#if defined(OUTPUT_USPBIN)
	if	(psInst->eOpcode == ISMP_USP_NDR)
	{
		if (bComma)
		{
			_ADDSTR(", ");
		}

		if	(psInst->u.psSmp->sUSPSample.bProjected) _ADDSTR("proj(");
		if	(psInst->u.psSmp->sUSPSample.bCentroid) _ADDSTR("cent(");

		_ADDSTRP1("tc%d", (IMG_INT32)psInst->u.psSmp->sUSPSample.uNDRTexCoord);

		if	(psInst->u.psSmp->sUSPSample.bCentroid) _ADDSTR(")");
		if	(psInst->u.psSmp->sUSPSample.bProjected) _ADDSTR(")");

		_ADDSTRP1(", t%d", (IMG_INT32)psInst->u.psSmp->uTextureStage);

		bComma = IMG_TRUE;
	}
	#endif /* #if defined(OUTPUT_USPBIN) */

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC)
	{
		uArgCount = GetSwizzleSlotCount(psState, psInst);
		for (i = uArgStart; i < uArgCount; i++)
		{
			if (bComma)
			{
				_ADDSTR(", ");
			}
			
			if(psInst->eOpcode == IVDUAL)
			{
				if(psInst->u.psVec->sDual.aeSrcUsage[i] == VDUAL_OP_TYPE_PRIMARY)
				{
					_ADDSTR("[P]");
				}
				else
				{
					_ADDSTR("[S]");
				}
			}
			
			PrintVectorSourceSlot(psState, psInst, apcTempString, i);
			
			bComma = IMG_TRUE;
		}
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		for (i = uArgStart; i < uArgCount; i++)
		{
			if	(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_TEXTURESAMPLE)
			{
				#if defined(OUTPUT_USPBIN)
				if	(
						i == SMP_STATE_ARG_START && 
						(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_USPTEXTURESAMPLE) != 0 &&
						psInst->eOpcode != ISMP_USP_NDR
					)
				{
					if (bComma)
					{
						_ADDSTR(", ");
					}
					_ADDSTRP1("t%d", (IMG_INT32)psInst->u.psSmp->uTextureStage);
					bComma = IMG_TRUE;
					continue;
				}
				#endif /* #if defined(OUTPUT_USPBIN) */
	
				if	(!ArgUsedFromSmp(psState, psInst, i))
				{
					continue;
				}
			}
			if (bComma)
			{
				_ADDSTR(", ");
			}
	
			PrintSourceModifiersBeforeSource(apcTempString, psInst, i);
			PrintRegister(apcTempString, psState, psInst, &psInst->asArg[i]);
			PrintSourceModifiers(psState, apcTempString, psInst, i);
			
			bComma = IMG_TRUE;
		}
	
		if (psInst->eOpcode == ISOPWM)
		{
			PrintSOP(apcTempString,
					psInst->u.psSopWm->uCop,
					USEASM_INTSRCSEL_SRC1,
					IMG_FALSE,
					psInst->u.psSopWm->uSel1,
					psInst->u.psSopWm->bComplementSel1,
					USEASM_INTSRCSEL_SRC2,
					IMG_FALSE,
					psInst->u.psSopWm->uSel2,
					psInst->u.psSopWm->bComplementSel2,
					psInst->u.psSopWm->uAop,
					USEASM_INTSRCSEL_SRC1,
					IMG_FALSE,
					psInst->u.psSopWm->uSel1,
					psInst->u.psSopWm->bComplementSel1,
					USEASM_INTSRCSEL_SRC2,
					IMG_FALSE,
					psInst->u.psSopWm->uSel2,
					psInst->u.psSopWm->bComplementSel2);
		}
		else
		if (psInst->eOpcode == ISOP2)
		{
			PrintSOP(apcTempString,
					psInst->u.psSop2->uCOp,
					USEASM_INTSRCSEL_SRC1,
					psInst->u.psSop2->bComplementCSrc1,
					psInst->u.psSop2->uCSel1,
					psInst->u.psSop2->bComplementCSel1,
					USEASM_INTSRCSEL_SRC2,
					IMG_FALSE,
					psInst->u.psSop2->uCSel2,
					psInst->u.psSop2->bComplementCSel2,
					psInst->u.psSop2->uAOp,
					USEASM_INTSRCSEL_SRC1,
					psInst->u.psSop2->bComplementASrc1,
					psInst->u.psSop2->uASel1,
					psInst->u.psSop2->bComplementASel1,
					USEASM_INTSRCSEL_SRC2,
					IMG_FALSE,
					psInst->u.psSop2->uASel2,
					psInst->u.psSop2->bComplementASel2);
		}
		else
		if (psInst->eOpcode == ILRP1)
		{
			PrintSOP(apcTempString,
					USEASM_INTSRCSEL_ADD,
					psInst->u.psLrp1->uCS,
					IMG_TRUE,
					psInst->u.psLrp1->uCSel10,
					psInst->u.psLrp1->bComplementCSel10,
					psInst->u.psLrp1->uCS,
					IMG_FALSE,
					psInst->u.psLrp1->uCSel11,
					psInst->u.psLrp1->bComplementCSel11,
					psInst->u.psLrp1->uAOp,
					USEASM_INTSRCSEL_SRC1,
					IMG_FALSE,
					psInst->u.psLrp1->uASel1,
					psInst->u.psLrp1->bComplementASel1,
					USEASM_INTSRCSEL_SRC2,
					IMG_FALSE,
					psInst->u.psLrp1->uASel2,
					psInst->u.psLrp1->bComplementASel2);
		}
		else
		if (psInst->eOpcode == IFPMA)
		{
			PrintSOP(apcTempString,
					USEASM_INTSRCSEL_ADD,
					psInst->u.psFpma->uCSel0,
					psInst->u.psFpma->bComplementCSel0,
					USEASM_INTSRCSEL_ZERO,
					IMG_TRUE,
					psInst->u.psFpma->uCSel1,
					psInst->u.psFpma->bComplementCSel1,
					psInst->u.psFpma->uCSel2,
					psInst->u.psFpma->bComplementCSel2,
					USEASM_INTSRCSEL_ADD,
					psInst->u.psFpma->uASel0,
					psInst->u.psFpma->bComplementASel0,
					USEASM_INTSRCSEL_ZERO,
					IMG_TRUE,
					USEASM_INTSRCSEL_SRC1,
					psInst->u.psFpma->bComplementASel1,
					USEASM_INTSRCSEL_SRC2,
					psInst->u.psFpma->bComplementASel2);
		}
		else
		if (psInst->eOpcode == ISOP3)
		{
			if (psInst->u.psSop3->uCoissueOp == USEASM_OP_ASOP)
			{
				PrintSOP(apcTempString,
						psInst->u.psSop3->uCOp,
						USEASM_INTSRCSEL_SRC1,
						IMG_FALSE,
						psInst->u.psSop3->uCSel1,
						psInst->u.psSop3->bComplementCSel1,
						USEASM_INTSRCSEL_SRC2,
						IMG_FALSE,
						psInst->u.psSop3->uCSel2,
						psInst->u.psSop3->bComplementCSel2,
						psInst->u.psSop3->uAOp,
						USEASM_INTSRCSEL_SRC1,
						IMG_FALSE,
						psInst->u.psSop3->uASel1,
						psInst->u.psSop3->bComplementASel1,
						USEASM_INTSRCSEL_SRC2,
						IMG_FALSE,
						psInst->u.psSop3->uASel2,
						psInst->u.psSop3->bComplementASel2);
			}
			else
			{
				ASSERT(psInst->u.psSop3->uCoissueOp == USEASM_OP_ALRP);
				PrintSOP(apcTempString,
						psInst->u.psSop3->uCOp,
						USEASM_INTSRCSEL_SRC1,
						IMG_FALSE,
						psInst->u.psSop3->uCSel1,
						psInst->u.psSop3->bComplementCSel1,
						USEASM_INTSRCSEL_SRC2,
						IMG_FALSE,
						psInst->u.psSop3->uCSel2,
						psInst->u.psSop3->bComplementCSel2,
						psInst->u.psSop3->uAOp,
						USEASM_INTSRCSEL_SRC0ALPHA,
						IMG_TRUE,
						psInst->u.psSop3->uASel1,
						psInst->u.psSop3->bComplementASel1,
						USEASM_INTSRCSEL_SRC0ALPHA,
						IMG_FALSE,
						psInst->u.psSop3->uASel2,
						psInst->u.psSop3->bComplementASel2);
			}
		}
	}
	
	#if defined(SUPPORT_SGX545)	
	if (psInst->eOpcode == IDUAL)
	{
		PDUAL_PARAMS psDual = psInst->u.psDual;
		IMG_UINT32 uArg;
		IMG_UINT32 uLastLineLength;

		uLastLineLength = strlen(apcTempString);
		_ADDSTRP2("\n%*s", (IMG_INT)(4 + 15), "");
		_ADDSTRP1("+%s", g_psInstDesc[psDual->eSecondaryOp].pszName);
		uSpaces = 31 - (strlen(apcTempString) - uLastLineLength - 1);
		_ADDSTRP2("%-*s", (IMG_INT)uSpaces, "");	
		ASSERT(psInst->asDest[1].uType == USEASM_REGTYPE_FPINTERNAL);
		_ADDSTRP1("i%d", psInst->asDest[1].uNumber);

		for (uArg = 0; uArg < g_psInstDesc[psDual->eSecondaryOp].uDefaultArgumentCount; uArg++)
		{
			DUAL_SEC_SRC	eSrc = psDual->aeSecondarySrcs[uArg];

			_ADDSTR(", ");
			if (eSrc == DUAL_SEC_SRC_I0 || 
				eSrc == DUAL_SEC_SRC_I1 ||
				eSrc == DUAL_SEC_SRC_I2)
			{
				_ADDSTRP1("i%d", eSrc - DUAL_SEC_SRC_I0);
			}
			else
			{
				IMG_UINT32	uPriSrc = eSrc - DUAL_SEC_SRC_PRI0;

				ASSERT(eSrc == DUAL_SEC_SRC_PRI0 ||
					   eSrc == DUAL_SEC_SRC_PRI1 ||
					   eSrc == DUAL_SEC_SRC_PRI2);
				PrintRegister(apcTempString, psState, psInst, &psInst->asArg[uPriSrc]);
			}
		}
	}
	#endif /* defined(SUPPORT_SGX545) */

	#if defined(OUTPUT_USPBIN)
	if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_USPTEXTURESAMPLE)
	{
		ARG sTempReg = psInst->u.psSmp->sUSPSample.sTempReg;
		if(sTempReg.uType != USC_REGTYPE_DUMMY)
		{
			_ADDSTR(", [");
			PrintRegister(apcTempString, psState, psInst, &sTempReg);
			_ADDSTR("-");
			sTempReg.uNumber += (GetUSPPerSampleTemporaryCount(psState, psInst->u.psSmp->uTextureStage, psInst) - 1);
			PrintRegister(apcTempString, psState, psInst, &sTempReg);
			_ADDSTR("]");
		}
	}
	else if (psInst->eOpcode == ISMPUNPACK_USP)
	{
		ARG sTempReg = psInst->u.psSmpUnpack->sTempReg;
		if(sTempReg.uType != USC_REGTYPE_DUMMY)
		{
			_ADDSTR(", [");
			PrintRegister(apcTempString, psState, psInst, &sTempReg);
			_ADDSTR("-");
			sTempReg.uNumber += (GetUSPPerSampleUnpackTemporaryCount() - 1);
			PrintRegister(apcTempString, psState, psInst, &sTempReg);
			_ADDSTR("]");
		}
	}
	#endif /* defined(OUTPUT_USPBIN) */

	#ifdef SRC_DEBUG
	ASSERT(psInst->uAssociatedSrcLine != UNDEFINED_SOURCE_LINE);
	if (psInst->uAssociatedSrcLine == psState->uTotalLines)
	{
		_ADDSTR("		;L[usc]");
	}
	else
	{
		_ADDSTRP1("		;L[%u]", psInst->uAssociatedSrcLine);
	}
	#endif /* SRC_DEBUG */

	//#undef _ADDSTRP4
}

#ifdef SRC_DEBUG
IMG_UINT32 CountOnBits(IMG_UINT32 uValue)
{
	IMG_UINT32 uCount = 0;
	if(uValue == 0)
	{
		return 0;
	}
	while(uValue&(uValue-1))
	{
		uValue = uValue & (uValue-1);
		uCount++;
	}
	uCount++;
	return uCount;
}
#endif /* SRC_DEBUG */

#ifdef SRC_DEBUG
typedef struct _COUNT_INSTRUCTIONS_BLOCK_DATA
{
	BLOCK_SORT_FUNC pfnSort;
	IMG_BOOL bHandlesCalls;
	IMG_PUINT32 auTotalIntermediateInstrCounts;
} COUNT_INSTRUCTIONS_BLOCK_DATA, *PCOUNT_INSTRUCTIONS_BLOCK_DATA;

static IMG_VOID CountInstructionsBP(PINTERMEDIATE_STATE psState,
					PCODEBLOCK psBlock, IMG_PVOID pvCounts)
/*****************************************************************************
 FUNCTION	: CountInstructionsBP

 DESCRIPTION	: BLOCK_PROC to count number of instructions & repeats

 PARAMETERS	: pvCounts - IMG_PUINT32 to two integers; the first is
	the total (static) number of intermediate instructions, the second is the
	number of instruction cycles after including repeats.

OUTPUT	: the two integers pointed to by pvCounts are updated
*****************************************************************************/
{
	PCOUNT_INSTRUCTIONS_BLOCK_DATA psCountInstructionsBlockData = (PCOUNT_INSTRUCTIONS_BLOCK_DATA)pvCounts;
	IMG_PUINT32 puCounts = psCountInstructionsBlockData->auTotalIntermediateInstrCounts;
	PINST	psInst, psNextInst;
	PVR_UNREFERENCED_PARAMETER(psState);

	for (psInst = psBlock->psBody; psInst; psInst = psNextInst)
	{
		psNextInst = psInst->psNext;
		puCounts[0]++;
		
		if (g_psInstDesc[psInst->eOpcode].eType == INST_TYPE_FDOTPRODUCT)
		{
			puCounts[1] += psInst->u.psFdp->uRepeatCount;
		}
		else if((psInst->uRepeat)>0)
		{
			if(GetBit(psInst->auFlag, INST_FETCH))
			{
				/* Fetch cost only one cycle */
				puCounts[1]++;
			}
			else
			{
				puCounts[1] += (psInst->uRepeat);
			}
		}
		else if(psInst->uMask & 0x0000000f)
		{
			puCounts[1] += CountOnBits(((psInst->uMask) & 0x0000000f));
		}
		else
		{
			puCounts[1]++;
		}
	}
	if(psBlock->uFlags & USC_CODEBLOCK_CFG)
	{		
		DoOnCfgBasicBlocks(psState, psBlock->u.sUncond.psCfg, psCountInstructionsBlockData->pfnSort, CountInstructionsBP, psCountInstructionsBlockData->bHandlesCalls, pvCounts);
	}
	
}

IMG_VOID DumpInstructionCount(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: DumpInstructionCount

 PURPOSE	: Dumps out the intermediate instruction count and total cpu 
			  cycles consumed by program.

 PARAMETERS	: psState	- Compiler state. (carries the source cycle counter 
						  table)

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32 auTotalIntermediateInstrCounts[2] = {0,0};
	IMG_UINT32 uTotalTableCount = 0;
	IMG_UINT32 i;
	char apcTempString[100];
	COUNT_INSTRUCTIONS_BLOCK_DATA sCountInstructionsBlockData;

	sCountInstructionsBlockData.auTotalIntermediateInstrCounts = auTotalIntermediateInstrCounts;
	for(i=0;i<=(psState->uTotalLines);i++)
	{
		uTotalTableCount += (psState->puSrcLineCost[i]);
	}
	
	sCountInstructionsBlockData.pfnSort = ANY_ORDER;
	sCountInstructionsBlockData.bHandlesCalls = IMG_TRUE;
	DoOnAllBasicBlocks(psState, ANY_ORDER, CountInstructionsBP, IMG_TRUE/*CALLs*/, &sCountInstructionsBlockData);
	
	sprintf(apcTempString, ";------ Total Measured Cost = %u -------\r\n", uTotalTableCount);
	DBGANDPDUMPOUT(apcTempString);
	sprintf(apcTempString, ";------ Total Intermediate Instructions = %u -------\r\n", auTotalIntermediateInstrCounts[0]);
	DBGANDPDUMPOUT(apcTempString);
	sprintf(apcTempString, ";------ Total Intermediate Instructions With Repeats = %u -------\r\n", auTotalIntermediateInstrCounts[1]);
	DBGANDPDUMPOUT(apcTempString);
	DBGANDPDUMPOUT(" ");
	
	ASSERT(uTotalTableCount == auTotalIntermediateInstrCounts[1]);
}
#endif /* SRC_DEBUG */

static
IMG_VOID PrintFunctionInputsOutputs(PINTERMEDIATE_STATE psState, PFUNC_INOUT_ARRAY psParams, IMG_PCHAR pszName)
{
	IMG_CHAR	apcTempString[5120];

	if (psParams->uCount > 0)
	{
		IMG_UINT32	uParamIdx;
		IMG_PCHAR	pszStr = apcTempString;

		pszStr += sprintf(pszStr, "\t%s:", pszName);
		for (uParamIdx = 0; uParamIdx < psParams->uCount; uParamIdx++)
		{
			PFUNC_INOUT	psParam = &psParams->asArray[uParamIdx];

			pszStr += sprintf(pszStr, " ");
			BasePrintRegister(psState, 
							  apcTempString, 
							  NULL /* psInst */, 
							  psParam->uType, 
							  psParam->uNumber, 
							  0 /* uArrayOffset */);
			pszStr = apcTempString + strlen(apcTempString);
			if (psParam->uChanMask != USC_ALL_CHAN_MASK)
			{
				pszStr += sprintf(pszStr, ".%s%s%s%s",
					(psParam->uChanMask & USC_X_CHAN_MASK) ? "x" : "",
					(psParam->uChanMask & USC_Y_CHAN_MASK) ? "y" : "",
					(psParam->uChanMask & USC_Z_CHAN_MASK) ? "z" : "",
					(psParam->uChanMask & USC_W_CHAN_MASK) ? "w" : "");
			}
		}
		DBG_PRINTF((DBG_MESSAGE, "%s", apcTempString));
	}
}

typedef struct _PRINT_BLOCK_DATA
{
	BLOCK_SORT_FUNC pfnSort;
	IMG_BOOL bHandlesCalls;
	IMG_CHAR acCfgLevel[1024];
	IMG_PBOOL abShow;
} PRINT_BLOCK_DATA, *PPRINT_BLOCK_DATA;

static
IMG_VOID PrintBlockBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvUserData)
{
	PPRINT_BLOCK_DATA psPrintBlockData = (PPRINT_BLOCK_DATA)(pvUserData);
	IMG_PBOOL abShow = psPrintBlockData->abShow;
	IMG_BOOL bShowNums = abShow[0];
	PINST psInst;
	IMG_UINT32 uInst = 0, uInstCountLog10 = 0;
	IMG_CHAR pszNum[12], apcTempString[5120];

	if (psBlock == psBlock->psOwner->psEntry)
	{
		const PFUNC psFunc = psBlock->psOwner->psFunc;
		
		DBG_PRINTF((DBG_MESSAGE, " "));
		
		if (psFunc->pchEntryPointDesc)
		{
			DBG_PRINTF((DBG_MESSAGE, "%s", psFunc->pchEntryPointDesc));
		}
		else if (psFunc == psState->psSecAttrProg)
		{
			/*
				The SA Prog does not have an EntryPointDesc because it
				isn't required externally (we export it if it's wanted _internally_).
			*/
			DBG_PRINTF((DBG_MESSAGE, "SECONDARY UPDATE PROGRAM:"));
		}
		else
		{
			DBG_PRINTF((DBG_MESSAGE, "FUNCTION %d :", psFunc->uLabel));

			PrintFunctionInputsOutputs(psState, &psFunc->sIn, "Inputs");
		}
	}

	DBG_PRINTF((DBG_MESSAGE, "BLOCK %s%i:", psPrintBlockData->acCfgLevel, psBlock->uIdx));

	if (bShowNums)
	{
		IMG_UINT32 uTemp;
		IMG_UINT32 uInstCount = 0;
		for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
		{
			uInstCount++;
		}
		for (uInstCountLog10 = 0, uTemp = 1; uTemp < uInstCount; uInstCountLog10++, uTemp *= 10); 
	}
	uInst = 0;
	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		ASSERT(psInst->psNext != NULL || psInst == psBlock->psBodyTail);
		ASSERT(psInst->psNext == NULL || psInst->psNext->psPrev == psInst);
		if (bShowNums)
		{
			sprintf(pszNum, "%*d", (IMG_INT)uInstCountLog10, uInst++);
		}

		apcTempString[0] = '\0';
		PrintIntermediateInst(psState, psInst, bShowNums ? pszNum : NULL, apcTempString);
		DBG_PRINTF((DBG_MESSAGE, "%s", apcTempString));
	}

	if (psBlock == psState->psMainProg->sCfg.psExit)
	{
		PUSC_LIST_ENTRY	psListEntry;

		for (psListEntry = psState->sFixedRegList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
		{
			PFIXED_REG_DATA	psFixedReg;

			psFixedReg = IMG_CONTAINING_RECORD(psListEntry, PFIXED_REG_DATA, sListEntry);
			if (psFixedReg->bLiveAtShaderEnd)
			{
				IMG_UINT32	uRegIdx;
				for (uRegIdx = 0; uRegIdx < psFixedReg->uConsecutiveRegsCount; uRegIdx++)
				{
					printf("\t%s%d <- %s%d\n", 
						   g_pszRegType[psFixedReg->sPReg.uType], 
						   psFixedReg->sPReg.uNumber + uRegIdx,
						   g_pszRegType[psFixedReg->uVRegType], 
						   psFixedReg->auVRegNum[uRegIdx]);
				}
			}
		}
	}

	ASSERT (psBlock != psState->psPreFeedbackBlock || psBlock->eType == CBTYPE_UNCOND);

	switch (psBlock->eType)
	{
	case CBTYPE_UNCOND:
		{
			IMG_CHAR const*	pszTransitionType;

			apcTempString[0] = '\0';

			if ((psState->uFlags & USC_FLAGS_SPLITFEEDBACKCALC) != 0 && psBlock == psState->psPreFeedbackBlock)
			{
				pszTransitionType = "SPLIT FEEDBACK, THEN";
			}
			else if ((psState->uFlags & USC_FLAGS_SPLITFEEDBACKCALC) != 0 && psBlock == psState->psPreFeedbackDriverEpilogBlock)
			{
				pszTransitionType = "SPLIT FEEDBACK EPILOG, THEN";
			}
			else if ((psState->uFlags2 & USC_FLAGS2_SPLITCALC) != 0 && psBlock == psState->psPreSplitBlock)
			{
				pszTransitionType = "SPLIT, THEN";
			}
			else
			{
				pszTransitionType = "UNCONDITIONAL";
			}

			_ADDSTRP4("%s --> %s%u(%u)", pszTransitionType, psPrintBlockData->acCfgLevel, psBlock->asSuccs[0].psDest->uIdx, psBlock->asSuccs[0].uDestIdx);
			if (psBlock->u.sUncond.bSyncEnd) _ADDSTR(" syncend");
			DBG_PRINTF((DBG_MESSAGE, "%s", apcTempString));
			break;
		}
	case CBTYPE_COND:
		{
			IMG_UINT32 uSucc;
			apcTempString[0] = '\0';
			_ADDSTR("CONDITIONAL ");
			PrintPredicate(&psBlock->u.sCond.sPredSrc, IMG_FALSE, apcTempString + strlen(apcTempString));
			_ADDSTR(" --> ");
			for (uSucc = 0; uSucc < 2; uSucc++)
			{
				_ADDSTRP3("%s%d(%d)", psPrintBlockData->acCfgLevel, psBlock->asSuccs[uSucc].psDest->uIdx, psBlock->asSuccs[uSucc].uDestIdx);
				if (psBlock->u.sCond.uSyncEndBitMask & (1 << uSucc)) _ADDSTR(" syncend");
				if (uSucc == 0) _ADDSTR(", ");
			}
			DBG_PRINTF((DBG_MESSAGE, "%s", apcTempString));
			break;
		}
	case CBTYPE_SWITCH:
		{
			IMG_CHAR buf[128];
			IMG_UINT32 uSucc, uNumCases = psBlock->uNumSuccs;
			if (psBlock->u.sSwitch.bDefault) uNumCases--;
			buf[0] = '\0';
			PrintRegister(buf, psState, NULL, psBlock->u.sSwitch.psArg);
			DBG_PRINTF((DBG_MESSAGE, "SWITCH %s", buf));
			for (uSucc = 0; uSucc < psBlock->uNumSuccs; uSucc++)
			{
				apcTempString[0] = '\0';
				if (uSucc == psBlock->uNumSuccs-1 && psBlock->u.sSwitch.bDefault)
				{
					_ADDSTR("    default");
				}
				else
				{
					_ADDSTRP1("    case %u", psBlock->u.sSwitch.auCaseValues[uSucc]);
				}
				_ADDSTRP3(" --> %s%d(%d)", psPrintBlockData->acCfgLevel, psBlock->asSuccs[uSucc].psDest->uIdx, psBlock->asSuccs[uSucc].uDestIdx);
				if (psBlock->u.sSwitch.pbSyncEnd[uSucc]) _ADDSTR(" syncend");
				DBG_PRINTF((DBG_MESSAGE, "%s", apcTempString));
			}
			break;
		}
	case CBTYPE_EXIT:
		ASSERT (psBlock == psBlock->psOwner->psExit);
		PrintFunctionInputsOutputs(psState, &psBlock->psOwner->psFunc->sOut, "Outputs");
		break;
	case CBTYPE_CONTINUE:
		DBG_PRINTF((DBG_MESSAGE, "**** LOOP CONTINUE****"));
		break;
	case CBTYPE_UNDEFINED:
		DBG_PRINTF((DBG_MESSAGE, "**** UNDEFINED CBTYPE****"));
		break;
	}
	if(psBlock->uFlags & USC_CODEBLOCK_CFG)
	{
		IMG_UINT32 uPrevStrLen = strlen(psPrintBlockData->acCfgLevel);
		sprintf(&(psPrintBlockData->acCfgLevel[uPrevStrLen]), "%d_", psBlock->uIdx);
		DoOnCfgBasicBlocks(psState, psBlock->u.sUncond.psCfg, psPrintBlockData->pfnSort, PrintBlockBP, psPrintBlockData->bHandlesCalls, pvUserData);
		psPrintBlockData->acCfgLevel[uPrevStrLen] = 0;
	}
	#undef _ADDSTR
	#undef _ADDSTRP1
	#undef _ADDSTRP2
}

#ifdef UF_TESTBENCH
IMG_CHAR apcDotLastFilename[32] = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};
IMG_UINT32 uDotFileNum = 0;
FILE *fDotOut = NULL;
FILE *fDotLast = NULL;
IMG_BOOL bDotSame;

static IMG_VOID DotPrint(IMG_PCHAR pchString)
{
	IMG_UINT32 i;
	for (i = 0; bDotSame && i<strlen(pchString); i++)
		if (getc(fDotLast) != pchString[i]) bDotSame = IMG_FALSE;
	fprintf(fDotOut, "%s", pchString);
}

typedef struct _PRINT_DOT_BLOCK_DATA
{
	BLOCK_SORT_FUNC pfnSort;
	IMG_BOOL bHandlesCalls;
	IMG_CHAR acCfgLevel[1024];
} PRINT_DOT_BLOCK_DATA, *PPRINT_DOT_BLOCK_DATA;

static IMG_VOID PrintDotBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvUserData)
{
#define MAX_NODE_LEN 10240
#define MAX_INSTR_LEN 512
	IMG_CHAR apcTempString[MAX_NODE_LEN];
	IMG_UINT32 uLabel = psBlock->psOwner->psFunc->uLabel;
	PINST psInst;
	PPRINT_DOT_BLOCK_DATA psPrintDotBlockData = ((PPRINT_DOT_BLOCK_DATA) pvUserData);	
	sprintf(apcTempString, "f%d%sn%d [style=%s,label=\"{", uLabel, psPrintDotBlockData->acCfgLevel, psBlock->uIdx, (psBlock == psBlock->psOwner->psExit) ? "rounded" : "solid" );
	if (psBlock->bAddSyncAtStart) sprintf(apcTempString + strlen(apcTempString), "SYNC |");

	sprintf(apcTempString + strlen(apcTempString), "BLOCK ADDR = %x | BLOCK ID = %u |", psBlock, psBlock->uIdx);
	//if (psBlock->psBody == NULL)
	{
	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		IMG_CHAR apcInstTemp[MAX_INSTR_LEN];
		IMG_PCHAR pch;
		ASSERT(psInst->psNext != NULL || psInst == psBlock->psBodyTail);
		ASSERT(psInst->psNext == NULL || psInst->psNext->psPrev == psInst);
		if (psInst->psPrev) sprintf(apcTempString + strlen(apcTempString), " | ");
		
		PrintIntermediateInst(psState, psInst, NULL, apcInstTemp);

		//NOTE 5 is length of terminator after loop below
		if (strlen(apcTempString) + strlen(apcInstTemp) >= MAX_NODE_LEN - 5) 
		{	
			//truncate instruction sequence to avoid a ridiculously big node
			sprintf(apcTempString + strlen(apcTempString), "...");
			break;
		}

		/*
			Fix a couple incompatibilities between normal output format and DOT...
			(a) Replace |'s, for conditions, with /'s
			(b) Remove \n's, resulting from dual-issue instructions;
		*/
		for (pch = apcInstTemp; *pch; pch++)
			if (*pch == '|') *pch = '/'; else if (*pch == '\n') *pch = '|';

		sprintf(apcTempString + strlen(apcTempString), "%s", apcInstTemp);
		#if defined (EXECPRED)
		if ((psState->uCompilerFlags2 & UF_FLAGS2_EXECPRED) && (psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_INTEGER_CONDITIONALS)  && !(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_29643))
		{
			break;
		}
		#endif /* #if defined(EXECPRED) */
	}
	}
	sprintf(apcTempString + strlen(apcTempString),"}\"];\n");
	DotPrint(apcTempString);
	if (psBlock == psBlock->psOwner->psFunc->sCfg.psEntry)
	{
		sprintf(apcTempString, "f%d [shape=plaintext, label=\"f%d\"];\n", uLabel, uLabel);
		DotPrint(apcTempString);
		sprintf(apcTempString, "f%d -> f%d%sn%d;\n", uLabel, uLabel, psPrintDotBlockData->acCfgLevel, psBlock->uIdx);
		DotPrint(apcTempString);
	}
	
	ASSERT (psBlock != psState->psPreFeedbackBlock || psBlock->eType == CBTYPE_UNCOND);
	switch (psBlock->eType)
	{
	case CBTYPE_UNCOND:
		sprintf(apcTempString,
				(psBlock == psState->psPreFeedbackBlock ? "f%d%sn%d -> f%d%sn%d [arrowtail=doublecross,style=%s];\n" : "f%d%sn%d -> f%d%sn%d [style=%s];\n"),
				uLabel, psPrintDotBlockData->acCfgLevel, psBlock->uIdx,
				uLabel, psPrintDotBlockData->acCfgLevel, psBlock->asSuccs[0].psDest->uIdx,
				(psBlock->u.sUncond.bSyncEnd ? "dashed" : "solid"));
		DotPrint(apcTempString);
		break;
	case CBTYPE_COND:
		sprintf(apcTempString, "f%d%sn%d -> f%d%sn%d [arrowtail=dot,style=%s];\n", uLabel, psPrintDotBlockData->acCfgLevel, psBlock->uIdx, uLabel, psPrintDotBlockData->acCfgLevel, psBlock->asSuccs[0].psDest->uIdx, (psBlock->u.sCond.uSyncEndBitMask & 1) ? "dashed" : "solid");
		DotPrint(apcTempString);
		sprintf(apcTempString, "f%d%sn%d -> f%d%sn%d [arrowtail=odot,style=%s];\n", uLabel, psPrintDotBlockData->acCfgLevel, psBlock->uIdx, uLabel, psPrintDotBlockData->acCfgLevel, psBlock->asSuccs[1].psDest->uIdx, (psBlock->u.sCond.uSyncEndBitMask & 2) ? "dashed" : "\"setlinewidth(3)\"");
		DotPrint(apcTempString);
		break;
	case CBTYPE_SWITCH:
		{
			IMG_UINT32 uSucc, uNumCases = psBlock->uNumSuccs;
			if (psBlock->u.sSwitch.bDefault) uNumCases--;
			for (uSucc = 0; uSucc < uNumCases; uSucc++)
			{
				sprintf(apcTempString, "f%d%sn%d -> f%d%sn%d [label=%d,style=%s];\n",
						uLabel, psPrintDotBlockData->acCfgLevel, psBlock->uIdx,
						uLabel, psPrintDotBlockData->acCfgLevel, psBlock->asSuccs[uSucc].psDest->uIdx,
						psBlock->u.sSwitch.auCaseValues[uSucc],
						psBlock->u.sSwitch.pbSyncEnd[uSucc] ? "dashed" : "solid");
				DotPrint(apcTempString);
			}
			if (psBlock->u.sSwitch.bDefault)
			{
				sprintf(apcTempString, "f%d%sn%d -> f%d%sn%d [label=\"-\",style=%s];\n",
						uLabel, psPrintDotBlockData->acCfgLevel, psBlock->uIdx,
						uLabel, psPrintDotBlockData->acCfgLevel, psBlock->asSuccs[uSucc].psDest->uIdx,
						(psBlock->u.sSwitch.pbSyncEnd[uSucc] ? "dashed" : "solid"));
				DotPrint(apcTempString);
			}
			break;
		}
	case CBTYPE_CONTINUE:
	case CBTYPE_EXIT:
		ASSERT (psBlock == psBlock->psOwner->psExit);
		//...in which case, don't print any edges
	case CBTYPE_UNDEFINED:
		break;
	}
	if (psBlock->psIDom)
	{
		sprintf(apcTempString, "f%d%sn%d -> f%d%sn%d [color=grey75,arrowtail=crow];\n", uLabel, psPrintDotBlockData->acCfgLevel, psBlock->psIDom->uIdx, uLabel, psPrintDotBlockData->acCfgLevel, psBlock->uIdx);
		DotPrint(apcTempString);
	}
#if 0 /*defined(IREGALLOC_LSCAN) || defined(REGALLOC_LSCAN)*/
	if (psBlock->psLoopHeader)
	{
		sprintf(apcTempString, "f%dn%d -> f%dn%d [color=grey50,arrowhead=obox];\n", uLabel, psBlock->psLoopHeader->uIdx, uLabel, psBlock->uIdx);
		DotPrint(apcTempString);
	}
#endif
//#define DEBUG_POSTDOMS
#if defined(DEBUG_POSTDOMS)
	if (psBlock->psIPostDom)
	{
		sprintf(apcTempString, "f%d%sn%d -> f%d%sn%d [color=grey50, arrowhead=crow, arrowtail=tee];\n", uLabel, psPrintBlockData->acCfgLevel, psBlock->uIdx, uLabel, psPrintBlockData->acCfgLevel, psBlock->psIPostDom->uIdx);
		DotPrint(apcTempString);
	}
	if (psBlock->psExtPostDom)
	{
		sprintf(apcTempString, "f%d%sn%d -> f%d%sn%d [color=grey50, arrowhead=crow, arrowtail=teetee];\n", uLabel, psPrintBlockData->acCfgLevel, psBlock->uIdx, uLabel, psPrintBlockData->acCfgLevel, psBlock->psExtPostDom->uIdx);
		DotPrint(apcTempString);
	}
#endif
	if(psBlock->uFlags & USC_CODEBLOCK_CFG)
	{
		sprintf(apcTempString, "f%d%sn%d -> f%d%sc%dn0 [style=dashed];\n", uLabel, psPrintDotBlockData->acCfgLevel, psBlock->uIdx, uLabel, psPrintDotBlockData->acCfgLevel, psBlock->uIdx);
		DotPrint(apcTempString);
		{
			IMG_UINT32 uPrevStrLen = strlen(psPrintDotBlockData->acCfgLevel);
			sprintf(&(psPrintDotBlockData->acCfgLevel[uPrevStrLen]), "c%d", psBlock->uIdx);
			DoOnCfgBasicBlocks(psState, psBlock->u.sUncond.psCfg, psPrintDotBlockData->pfnSort, PrintDotBP, psPrintDotBlockData->bHandlesCalls, pvUserData);
			psPrintDotBlockData->acCfgLevel[uPrevStrLen] = 0;
		}
	}
#undef MAX_NODE_LEN
#undef MAX_INSTR_LEN
}
#endif /*UF_TESTBENCH*/

IMG_INTERNAL
IMG_VOID BasePrintIntermediate(PINTERMEDIATE_STATE psState,
							   IMG_PCHAR pchName,
							   IMG_BOOL bShowNums)
/*****************************************************************************
 FUNCTION	: BasePrintIntermediate

 PURPOSE	: Print an intermediate instruction

 PARAMETERS	: psState	    - Compiler state.
			  pchName       -
              bShowNums     - Whether to print line numbers.

 RETURNS	: Nothing
*****************************************************************************/
{
	const BLOCK_SORT_FUNC pfnSort = (psState->uFlags & USC_FLAGS_INTERMEDIATE_CODE_GENERATED) ? DebugPrintSF : ANY_ORDER;
#if defined(DEBUG_TIME)
	double dTimeDiff = USC_CLOCK_ELAPSED(psState->sTimeStart);
#endif
	IMG_BOOL abShow[1];

	abShow[0] = bShowNums;

	DBG_PRINTF((DBG_MESSAGE, __SECTION_TITLE__ "%s" __SECTION_TITLE__, pchName));
	
	if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL)
	{
		PUSC_LIST_ENTRY	psListEntry;

		for (psListEntry = psState->sShader.psPS->sPixelShaderInputs.psHead;
			 psListEntry != NULL;
			 psListEntry = psListEntry->psNext)
		{
			PPIXELSHADER_INPUT	psInput = IMG_CONTAINING_RECORD(psListEntry, PPIXELSHADER_INPUT, sListEntry);
			IMG_UINT32			uRegIdx;
			IMG_CHAR			aszStr[256];
			IMG_PCHAR			pszStr = aszStr;

			pszStr += sprintf(pszStr, "\t");
			for (uRegIdx = 0; uRegIdx < psInput->psFixedReg->uConsecutiveRegsCount; uRegIdx++)
			{
				if (uRegIdx > 0)
				{
					pszStr += sprintf(pszStr, "+");
				}
				pszStr += sprintf(pszStr, "%s%d", 
					g_pszRegType[psInput->psFixedReg->uVRegType], 
					psInput->psFixedReg->auVRegNum[uRegIdx]);
			}
			pszStr += sprintf(pszStr, " <- ");
			pszStr = PVRUniFlexDecodeIteration(&psInput->sLoad, pszStr);
			DBG_PRINTF((DBG_MESSAGE, "%s", aszStr));
		}
	}

	{
		IMG_UINT32		uRegIdx;
		PUSC_LIST_ENTRY	psListEntry;

		for (uRegIdx = 0, psListEntry = psState->sSAProg.sInRegisterConstantList.psHead; 
			 uRegIdx < psState->sSAProg.uInRegisterConstantCount; 
			 uRegIdx++, psListEntry = psListEntry->psNext)
		{
			PINREGISTER_CONST	psConst = IMG_CONTAINING_RECORD(psListEntry, PINREGISTER_CONST, sListEntry);
			IMG_CHAR			aszStr[256];
			IMG_PCHAR			pszStr = aszStr;
			static const IMG_PCHAR		pszFormat[] =
			{
				"f32",	/* UNIFLEX_CONST_FORMAT_F32 */
				"f16",	/* UNIFLEX_CONST_FORMAT_F16 */
				"c10",	/* UNIFLEX_CONST_FORMAT_C10 */
				"u8",	/* UNIFLEX_CONST_FORMAT_U8 */
			};

			pszStr += sprintf(pszStr, "\t");
			
			pszStr += sprintf(pszStr, "r%d", psConst->psResult->psFixedReg->auVRegNum[0]);
			if ((psState->uFlags & USC_FLAGS_ASSIGNED_SECATTR_REGNUMS) != 0)
			{
				pszStr += sprintf(pszStr, "(sa%d)", psConst->psResult->psFixedReg->sPReg.uNumber);
			}
			pszStr += sprintf(pszStr, " <- ");
			switch (psConst->eFormat)
			{
				case UNIFLEX_CONST_FORMAT_F32:
				case UNIFLEX_CONST_FORMAT_F16:
				case UNIFLEX_CONST_FORMAT_C10:
				case UNIFLEX_CONST_FORMAT_U8:
				{
					pszStr += sprintf(pszStr, "%s(c%d_t%d%s)", 
						pszFormat[psConst->eFormat], 
						psConst->uNum & ~REMAPPED_CONSTANT_UNPACKEDC10, 
						psConst->uBuffer,
						(psConst->uNum & REMAPPED_CONSTANT_UNPACKEDC10) != 0 ? "_unpacked" : "");
					break;
				}
				case UNIFLEX_CONST_FORMAT_STATIC:
				{
					pszStr +=
						sprintf(pszStr, "%g/0x%.8X", *(IMG_PFLOAT)&psConst->uNum, psConst->uNum);
					break;
				}
				case UNIFLEX_CONST_FORMAT_CONSTS_BUFFER_BASE:
				{
					pszStr += sprintf(pszStr, "buffer_base%d", psConst->uBuffer);
					break;
				}
				case UNIFLEX_CONST_FORMAT_RTA_IDX:
				{
					pszStr += sprintf(pszStr, "rta_idx");
					break;
				}
				case UNIFLEX_CONST_FORMAT_UNDEF:
				{
					pszStr += sprintf(pszStr, "undef");
					break;
				}
				default: imgabort();
			}
			DBG_PRINTF((DBG_MESSAGE, "%s", aszStr));
		}
	}

	{
		PRINT_BLOCK_DATA sPrintBlockData;		
		sPrintBlockData.pfnSort = pfnSort;
		sPrintBlockData.bHandlesCalls = IMG_TRUE;
		sPrintBlockData.acCfgLevel[0] = 0;
		sPrintBlockData.abShow = abShow;
		DoOnAllBasicBlocks(psState, pfnSort, PrintBlockBP, IMG_TRUE, &sPrintBlockData);		
	}

#ifdef DEBUG
	DBG_PRINTF((DBG_MESSAGE, "Memory use high-water mark: %d\n", psState->uMemoryUsedHWM));
#endif /* DEBUG */

#ifdef DEBUG_TIME
	DBG_PRINTF((DBG_MESSAGE, "CPU time (seconds): %g\n", dTimeDiff));
#endif /* DEBUG_TIME */

#ifdef SRC_DEBUG
	DumpInstructionCount(psState);
#endif /* SRC_DEBUG */

	if (g_bDotOption)
	{
		if (fDotOut == NULL && fDotLast == NULL)
		{
			IMG_CHAR apcFilename[32];
			PRINT_BLOCK_DATA sPrintDotBlockData;
			sprintf(apcFilename, "%d", uDotFileNum);
			sprintf(apcFilename + strlen(apcFilename), "%s.dot", pchName);
			fDotOut = fopen(apcFilename,"w");
			if (strlen(apcDotLastFilename))
			{
				fDotLast = fopen(apcDotLastFilename, "r");
				bDotSame = IMG_TRUE;
			}
			else
			{
				bDotSame = IMG_FALSE;
			}
			DotPrint("digraph PRE {\n");
			DotPrint("    node [shape=record];\n");
			sPrintDotBlockData.pfnSort = ANY_ORDER;
			sPrintDotBlockData.bHandlesCalls = IMG_TRUE;
			sPrintDotBlockData.acCfgLevel[0] = 0;
			DoOnAllBasicBlocks(psState, ANY_ORDER, PrintDotBP, IMG_TRUE, &sPrintDotBlockData);
			DotPrint("};\n");
			fclose(fDotOut); fDotOut = NULL;
			if (fDotLast) fclose(fDotLast); fDotLast = NULL;
			if (bDotSame)
			{
				remove(apcFilename); //check return code?!?!
			}
			else
			{
				sprintf(apcDotLastFilename, "%s", apcFilename);
				uDotFileNum++;
			}
		}
		else DBG_PRINTF((DBG_MESSAGE, "Not producing dot output - in middle of recursive call?!?!"));
	}
}

IMG_INTERNAL
IMG_VOID PrintIntermediate(PINTERMEDIATE_STATE psState,
						   IMG_PCHAR pchName,
						   IMG_BOOL bShowNums)
/*****************************************************************************
 FUNCTION	: PrintIntermediate

 PURPOSE	: Print an intermediate instruction

 PARAMETERS	: psState	    - Compiler state.
			  pchName       -
              bShowNums     - Whether to print line numbers.

 RETURNS	: Nothing
*****************************************************************************/
{
	BasePrintIntermediate(psState, pchName, bShowNums);
	CheckProgram(psState);
}
#endif /* defined(UF_TESTBENCH) */

/******************************************************************************
 End of file (debug.c)
******************************************************************************/
