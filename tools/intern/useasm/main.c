/*************************************************************************
 * Name         : main.c
 * Title        : Pixel Shader Compiler
 * Author       : John Russell
 * Created      : Jan 2002
 *
 * Copyright    : 2002-2010 by Imagination Technologies Limited. All rights reserved.
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
 *
 * Modifications:-
 * $Log: main.c $
 **************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#if !defined(__psp2__)
#include <fcntl.h>
#if !defined(LINUX)
#include <io.h>
#else
#include <errno.h>
#define max(a,b) a>b?a:b
#define stricmp strcasecmp
#endif
#else
#define max(a,b) a>b?a:b
#define stricmp strcasecmp
#endif
#include <assert.h>

#include "sgxsupport.h"

#include "sgxdefs.h"
#include "use.h"
#include "useasm.h"
#include "ctree.h"
#include "objectfile.h"
#include "usetab.h"
#include "osglue.h"
#if defined(ENABLE_USEOPT)
#include "useopt.h"
#endif /* defined(ENABLE_USEOPT) */

static IMG_CHAR const* g_pszOptions = 
"-offset=OFFSET Set the start of the program within the instruction page.\n"
"-label_header=FILENAME\n"
"               Generate a header containing C defines for the offset of\n"
"               each label.\n"
"-label_prefix=PREFIX\n"
"               Prefix for the define names in the generated label header.\n"
"-quiet         Don't print a disassembly of the assembled hardware\n"
"               instructions.\n"
#if defined(ENABLE_USEOPT)
"-opt           Enable the optimizer.\n"
#endif /* defined(ENABLE_USEOPT) */
"-l             Prefix the disassembly of each hardware instruction with\n"
"               the source file and line.\n"
"-listing\n"
"-target=TARGETNAME\n"
"               Sets the target core to assemble for.\n"
"-targetrev=NUM Sets the revision of the target core to assemble for.\n"
"-fixpairs      Automatically fix invalid instruction pairs by inserting\n"
"               extra NOP instructions.\n"
"-obj           Generate an object file for input to uselink rather than\n"
"               a raw binary.\n"
"-cpp           Enable compatibility with the output of a C preprocessor.\n"
"               Specifically this stops the assembler treating a newline\n"
"               character as delimiting the end of an instruction.\n"
"-enablescopes  Enables named register support.\n"
"-r=START,END   Sets the range of hardware register numbers available for\n"
"               named registers.\n"
"-rf\n";

static IMG_VOID usage(IMG_VOID)
{
	puts("Usage: useasm [OPTION] ... <infile> <outfile>\n");
	puts(g_pszOptions);
}

/* Input file name. */
IMG_PCHAR g_pszInFileName;

/* Was an error encountered during assembly. */
static IMG_BOOL g_bAssemblerError;

/* Was an error encountered in the c parsing module. */
IMG_BOOL g_bCCodeError = IMG_FALSE;

/* Starting offset to assemble at. */
IMG_UINT32 g_uCodeOffset;

/* Should pairing errors be automatically fixed? */
IMG_BOOL g_bFixInvalidPairs = IMG_FALSE;

/* Target core. */
SGX_CORE_INFO g_sTargetCoreInfo = {SGX_CORE_ID_INVALID, 0};

/* Information about the features/bugs of the target core. */
PCSGX_CORE_DESC	g_psTargetCoreDesc = NULL;

/* Have the old EFO options been used before a target core was used. */
IMG_BOOL g_bUsedOldEfoOptions = IMG_FALSE;

/* Is compatibility with the output from a C preprocessor enabled? */
IMG_BOOL g_bCPreprocessor = IMG_FALSE;

/* Links to declaration of yyin in the output of flex. */
extern FILE* yyin;

typedef struct
{
	IMG_PCHAR		pszName;
	IMG_UINT32		uAddress;
	const IMG_CHAR*	pszSourceFile;
	IMG_UINT32		uSourceLine;
	IMG_BOOL		bDefined;
	IMG_BOOL		bExported;
	IMG_BOOL		bImported;
	const IMG_CHAR*	pszExportSourceFile;
	IMG_UINT32		uExportSourceLine;
	const IMG_CHAR*	pszImportSourceFile;
	IMG_UINT32		uImportSourceLine;
} BRANCH;

/* Database of labels in the assembler program. */
static IMG_UINT32 g_uLabelCount;
static BRANCH* g_psLabels;

/* Count of the number of times we have entered a nosched region. */
IMG_UINT32 g_uSchedOffCount;
/* Count of the number of times we have entered a skipinv region. */
IMG_UINT32 g_uSkipInvOnCount;
/* First instruction in the nosched region. */
PUSE_INST g_psFirstNoSchedInst;
/* Last instruction in the nosched region. */
PUSE_INST g_psLastNoSchedInst;
/* Offset of the first instruction in the nosched region. */
IMG_UINT32 g_uFirstNoSchedInstOffset;
/* Offset of the last instruction in the nosched region. */
IMG_UINT32 g_uLastNoSchedInstOffset;
/* Stack of repeat count directives. */
IMG_UINT32 g_puRepeatStack[REPEAT_STACK_LENGTH];
/* Top of the stack of repeat count directives. */
IMG_UINT32 g_uRepeatStackTop;
/* Current offset in the output program. */
IMG_UINT32 g_uInstOffset;
/* Is this the start of nosched region. */
IMG_BOOL g_bStartedNoSchedRegion;
 
/* Address and size of the import data segment. */
IMG_PVOID	g_pvFixupDataSegment = NULL;
IMG_UINT32	g_uFixupDataSegmentSize = 0;
/* Address and size of the string segment. */
IMG_PVOID	g_pvStringSegment = NULL;
IMG_UINT32	g_uStringSegmentSize = 0;
/* Address and size of the export segment. */
IMG_PVOID	g_pvExportDataSegment;
IMG_UINT32	g_uLabelExportCount;
/* Address and size of the LADDR relocation segment. */
IMG_PVOID	g_pvLADDRRelocationSegment = NULL;
IMG_UINT32	g_uLADDRRelocationCount = 0;

/* TRUE if we are outputting an object file. */
IMG_BOOL	g_bWriteObjectFile;

/* Required alignment for the start of the module. */
IMG_UINT32	g_uModuleAlignment = 2;

/*****************************************************************************
 FUNCTION	: UseAssemblerLADDRNotify
    
 PURPOSE	: Records a reference to a label in a LIMM instruction.

 PARAMETERS	: uAddress		- The address of the LIMM instruction.
			  
 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID IMG_CALLCONV UseAssemblerLADDRNotify(IMG_UINT32 uAddress)
{
	PUSEASM_LADDRRELOCATION	psReloc;

	g_pvLADDRRelocationSegment = 
		realloc(g_pvLADDRRelocationSegment, (g_uLADDRRelocationCount + 1) * sizeof(USEASM_LADDRRELOCATION));

	psReloc = (PUSEASM_LADDRRELOCATION)g_pvLADDRRelocationSegment + g_uLADDRRelocationCount;
	psReloc->uAddress = uAddress;

	g_uLADDRRelocationCount++;
}

/*****************************************************************************
 FUNCTION	: UseasmRealloc
    
 PURPOSE	: 

 PARAMETERS	:
			  
 RETURNS	: 
*****************************************************************************/
static IMG_PVOID IMG_CALLCONV UseasmRealloc(IMG_PVOID pvContext, IMG_PVOID pvOldBuf, IMG_UINT32 uNewSize, IMG_UINT32 uOldSize)
{
	PVR_UNREFERENCED_PARAMETER(pvContext);
	PVR_UNREFERENCED_PARAMETER(uOldSize);
	return realloc(pvOldBuf, uNewSize);
}

#if defined(ENABLE_USEOPT)
/*****************************************************************************
 FUNCTION	: UseasmAlloc
    
 PURPOSE	: Allocate memory

 PARAMETERS	: pvContext    - Assembler context
              uSize        - Size in bytes of required memory
			  
 RETURNS	: Allocated memory or NULL on failure.
*****************************************************************************/
static IMG_PVOID IMG_CALLCONV UseasmAlloc(IMG_PVOID pvContext, IMG_UINT32 uSize)
{
	PVR_UNREFERENCED_PARAMETER(pvContext);
	return UsmAsm_Malloc(uSize);
}

/*****************************************************************************
 FUNCTION	: UseasmFree
    
 PURPOSE	: Free memory

 PARAMETERS	: pvContext    - Assembler context
              pvChunk      - Memory to free
			  
 RETURNS	: Nothing
*****************************************************************************/
static IMG_VOID IMG_CALLCONV UseasmFree(IMG_PVOID pvContext, IMG_PVOID pvChunk)
{
	PVR_UNREFERENCED_PARAMETER(pvContext);
	UseAsm_Free(pvChunk);
}
#endif /* defined(ENABLE_USEOPT) */


/*****************************************************************************
 FUNCTION	: AddStringToStringSegment
    
 PURPOSE	: Adds a string to the string segment.

 PARAMETERS	: pszString			- The string to add.
			  
 RETURNS	: The offset in bytes of the string within the string segment.
*****************************************************************************/
static IMG_UINT32 AddStringToStringSegment(IMG_PCHAR	pszString)
{
	IMG_UINT32	uOffset;

	/*
		Check if the string is already in the segment.
	*/
	uOffset = 0;
	while (uOffset < g_uStringSegmentSize)
	{
		IMG_PCHAR	pszExistingString;

		pszExistingString = (IMG_PCHAR)g_pvStringSegment + uOffset;
		if (strcmp(pszExistingString, pszString) == 0)
		{
			return uOffset;
		}

		uOffset += strlen(pszExistingString) + 1;
	}

	/*
		Otherwise grow the string segment and add the string to the end.
	*/
	g_pvStringSegment = realloc(g_pvStringSegment, g_uStringSegmentSize + strlen(pszString) + 1);
	strcpy((IMG_PCHAR)g_pvStringSegment + g_uStringSegmentSize, pszString);
	g_uStringSegmentSize += strlen(pszString) + 1;

	return uOffset;
}

/*****************************************************************************
 FUNCTION	: PrepareExportSegment
    
 PURPOSE	: Create the export segment in memory.

 PARAMETERS	: None.
			  
 RETURNS	: None.
*****************************************************************************/
static IMG_BOOL PrepareExportSegment(IMG_VOID)
{
	IMG_UINT32	uLabel;

	g_uLabelExportCount = 0;
	g_pvExportDataSegment = NULL;
	for (uLabel = 0; uLabel < g_uLabelCount; uLabel++)
	{
		if (g_psLabels[uLabel].bExported)
		{
			PUSEASM_EXPORTDATA	psExport;

			if (g_psLabels[uLabel].uAddress ==  USE_UNDEF)
			{
#if defined(__psp2__)
				sceClibPrintf("%s(%d): error: Label '%s' is exported but never defined.\n",
						g_psLabels[uLabel].pszExportSourceFile,
						g_psLabels[uLabel].uExportSourceLine,
						g_psLabels[uLabel].pszName);
#else
				fprintf(stderr, "%s(%d): error: Label '%s' is exported but never defined.\n",
						g_psLabels[uLabel].pszExportSourceFile, 
						g_psLabels[uLabel].uExportSourceLine, 
						g_psLabels[uLabel].pszName);
#endif
				return IMG_FALSE;
			}

			g_pvExportDataSegment = realloc(g_pvExportDataSegment, (g_uLabelExportCount + 1) * sizeof(USEASM_EXPORTDATA));

			psExport = (PUSEASM_EXPORTDATA)g_pvExportDataSegment + g_uLabelExportCount;

			psExport->uNameOffset = AddStringToStringSegment(g_psLabels[uLabel].pszName);
			psExport->uLabelAddress = g_psLabels[uLabel].uAddress;

			g_uLabelExportCount++;
		}
	}
	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: WriteObjectFile
    
 PURPOSE	: Creates a USEASM object file.

 PARAMETERS	: uInstCount		- Count of instructions in the module.
			  puInsts			- Pointer to the instructions for the module.
			  fOutFile			- Handle to the output file.
			  
 RETURNS	: TRUE if the file was written successfully.
*****************************************************************************/
static IMG_BOOL WriteObjectFile(IMG_UINT32				uInstCount,
								IMG_PUINT32				puInsts,
								FILE*					fOutFile)
{
	USEASM_OBJFILE_HEADER		sHeader;
	IMG_UINT32					uOutputOffset;

	strncpy(sHeader.pszFileId, USEASM_OBJFILE_ID, sizeof(sHeader.pszFileId));
	sHeader.uVersion = 0;
	sHeader.sTarget = g_sTargetCoreInfo;
	uOutputOffset = sizeof(sHeader);

	sHeader.uModuleAlignment = g_uModuleAlignment;

	sHeader.uInstructionOffset = uOutputOffset;
	sHeader.uInstructionCount = uInstCount;
	uOutputOffset += uInstCount * EURASIA_USE_INSTRUCTION_SIZE;

	sHeader.uExportDataCount = g_uLabelExportCount;
	sHeader.uExportDataOffset = uOutputOffset;
	uOutputOffset += g_uLabelExportCount * sizeof(USEASM_EXPORTDATA);

	sHeader.uImportDataOffset = uOutputOffset;
	sHeader.uImportDataCount = g_uFixupDataSegmentSize / sizeof(USEASM_IMPORTDATA);
	uOutputOffset += g_uFixupDataSegmentSize;

	sHeader.uStringDataOffset = uOutputOffset;
	sHeader.uStringDataSize = g_uStringSegmentSize;
	uOutputOffset += g_uStringSegmentSize;

	sHeader.uLADDRRelocationCount = g_uLADDRRelocationCount;
	sHeader.uLADDRRelocationOffset = uOutputOffset;
	uOutputOffset +=  g_uLADDRRelocationCount * sizeof(USEASM_LADDRRELOCATION);

#ifdef HW_REGS_ALLOC_ENABLED
	sHeader.uHwRegsAllocRangeStart = GetHwRegsAllocRangeStartForBinary();
	sHeader.uHwRegsAllocRangeEnd = GetHwRegsAllocRangeEndForBinary();
#endif

	if (fwrite(&sHeader, sizeof(sHeader), 1, fOutFile) != 1)
	{
		goto write_failed;
	}
	if (fwrite(puInsts, EURASIA_USE_INSTRUCTION_SIZE, uInstCount, fOutFile) != uInstCount)
	{
		goto write_failed;
	}
	if (fwrite(g_pvExportDataSegment, sizeof(USEASM_EXPORTDATA), g_uLabelExportCount, fOutFile) != g_uLabelExportCount)
	{
		goto write_failed;
	}
	if (fwrite(g_pvFixupDataSegment, 1, g_uFixupDataSegmentSize, fOutFile) != g_uFixupDataSegmentSize)
	{
		goto write_failed;
	}
	if (fwrite(g_pvStringSegment, 1, g_uStringSegmentSize, fOutFile) != g_uStringSegmentSize)
	{
		goto write_failed;
	}
	if (fwrite(g_pvLADDRRelocationSegment, sizeof(USEASM_LADDRRELOCATION), g_uLADDRRelocationCount, fOutFile) != g_uLADDRRelocationCount)
	{
		goto write_failed;
	}

	return IMG_TRUE;

write_failed:
#if defined(__psp2__)
	sceClibPrintf("Couldn't write output\n");
#else
	fprintf(stderr, "Couldn't write output: %s.\n", strerror(errno));
#endif
	return IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SetLabelExportImportState
    
 PURPOSE	: Sets whether a label is exported or imported.

 PARAMETERS	: uLabelId			- Label to set.
			  pszSourceFile		- Location of the import/export statement.
			  uSourceLine
			  bImported			- If TRUE flag the label as imported.
			  bExported			- If TRUE flag the label as exported.
			  
 RETURNS	: Nothing.
*****************************************************************************/
IMG_VOID SetLabelExportImportState(IMG_UINT32	uLabelId, 
								   const IMG_CHAR*	pszSourceFile, 
								   IMG_UINT32	uSourceLine, 
								   IMG_BOOL		bImported,
								   IMG_BOOL		bExported)
{
	assert(uLabelId < g_uLabelCount);
	assert(!(bImported && bExported));
	if (
			(bExported && g_psLabels[uLabelId].bImported) ||
			(bImported && g_psLabels[uLabelId].bExported)
	   )
	{
#if defined(__psp2__)
		sceClibPrintf("%s(%d): error: Label '%s' is both imported and exported.\n",
			pszSourceFile, uSourceLine, g_psLabels[uLabelId].pszName);
#else
		fprintf(stderr, "%s(%d): error: Label '%s' is both imported and exported.\n",
			pszSourceFile, uSourceLine, g_psLabels[uLabelId].pszName);
#endif
		g_bAssemblerError = TRUE;
	}
	if (bExported)
	{
		g_psLabels[uLabelId].bExported = TRUE;
		g_psLabels[uLabelId].pszExportSourceFile = pszSourceFile;
		g_psLabels[uLabelId].uExportSourceLine = uSourceLine;
	}
	if (bImported)
	{
		if (g_psLabels[uLabelId].bDefined)
		{
#if defined(__psp2__)
			sceClibPrintf("%s(%d): error: Label '%s' is both imported and defined.\n",
				pszSourceFile, uSourceLine, g_psLabels[uLabelId].pszName);
			sceClibPrintf("%s(%d): error: This is the location of the definition.\n",
				g_psLabels[uLabelId].pszSourceFile, g_psLabels[uLabelId].uSourceLine);
#else
			fprintf(stderr, "%s(%d): error: Label '%s' is both imported and defined.\n",
				pszSourceFile, uSourceLine, g_psLabels[uLabelId].pszName);
			fprintf(stderr, "%s(%d): error: This is the location of the definition.\n",
				g_psLabels[uLabelId].pszSourceFile, g_psLabels[uLabelId].uSourceLine);
#endif
			g_bAssemblerError = TRUE;
		}
		g_psLabels[uLabelId].bImported = TRUE;
		g_psLabels[uLabelId].pszImportSourceFile = pszSourceFile;
		g_psLabels[uLabelId].uImportSourceLine = uSourceLine;
	}
}

/*****************************************************************************
 FUNCTION	: UseAssemblerAddImportedLabel

 PURPOSE	: Adds an imported label to the import segment.

 PARAMETERS	: uLabelId			- The ID of the label.
			  uFixOp			- The type of reference to the label.
			  uFixupAddress		- The address of the reference to the label.
			  pszFixupSourceFileName	- The location in the input of the
			  uFixupSourceLineNumber      reference to the label.

 RETURNS	: TRUE if the label was flagged as imported.
*****************************************************************************/
static IMG_BOOL UseAssemblerAddImportedLabel(IMG_UINT32	uLabelId,
											  IMG_UINT32	uFixupOp,
											  IMG_UINT32	uFixupAddress,
											  IMG_PCHAR		pszFixupSourceFileName,
											  IMG_UINT32	uFixupSourceLineNumber,
											  IMG_BOOL		bFixupSyncEnd)
{
	PUSEASM_IMPORTDATA	psData;
	
	if (!g_psLabels[uLabelId].bImported)
	{
		return IMG_FALSE;
	}
	if (bFixupSyncEnd)
	{
#if defined(__psp2__)
		sceClibPrintf("%s(%d): error: Imported labels can't be used in branch instructions with the syncend flag.\n",
				pszFixupSourceFileName,
				uFixupSourceLineNumber);
#else
		fprintf(stderr, "%s(%d): error: Imported labels can't be used in branch instructions with the syncend flag.\n",
				pszFixupSourceFileName,
				uFixupSourceLineNumber);
#endif
		g_bAssemblerError = TRUE;
	}

	g_pvFixupDataSegment = 
		realloc(g_pvFixupDataSegment, g_uFixupDataSegmentSize + sizeof(USEASM_IMPORTDATA));
	psData = (PUSEASM_IMPORTDATA)((IMG_PBYTE)g_pvFixupDataSegment + g_uFixupDataSegmentSize);
	psData->uNameOffset = AddStringToStringSegment(g_psLabels[uLabelId].pszName);
	psData->uFixupOp = uFixupOp;
	psData->uFixupAddress = uFixupAddress;
	psData->bFixupSyncEnd = IMG_FALSE;
	psData->uFixupOffset = 0;
	psData->uFixupSourceFileNameOffset = AddStringToStringSegment(pszFixupSourceFileName);
	psData->uFixupSourceLineNumber = uFixupSourceLineNumber;

	g_uFixupDataSegmentSize += sizeof(USEASM_IMPORTDATA);

	return IMG_TRUE;
}

static IMG_VOID ImportUndefinedLabels(PUSEASM_CONTEXT psContext, IMG_PUINT32 puBaseInst)
/*****************************************************************************
 FUNCTION	: ImportUndefinedLabels

 PURPOSE	: Check for undefined labels in the program.

 PARAMETERS	:

 RETURNS	: Nothing.
*****************************************************************************/
{
	LABEL_CONTEXT* psLabelContext;
	IMG_UINT32 i;

	psLabelContext = (LABEL_CONTEXT*)psContext->pvLabelState;
	if (psLabelContext != NULL)
	{
		for (i = 0; i < psLabelContext->uLabelReferenceCount; i++)
		{
			IMG_UINT32	uOffset;

			uOffset = (psLabelContext->psLabelReferences[i].puOffset - puBaseInst) / 2;

			if (!UseAssemblerAddImportedLabel(psLabelContext->psLabelReferences[i].uLabel,
											  psLabelContext->psLabelReferences[i].uOp,
											  uOffset,
											  psLabelContext->psLabelReferences[i].psInst->pszSourceFile,
											  psLabelContext->psLabelReferences[i].psInst->uSourceLine,
											  psLabelContext->psLabelReferences[i].bSyncEnd))
			{
				IMG_PCHAR	pcLabelName;

				pcLabelName = psContext->pfnGetLabelName(psContext->pvContext, psLabelContext->psLabelReferences[i].uLabel);

				AssemblerError(psContext->pvContext,
							   psLabelContext->psLabelReferences[i].psInst, 
							   "Label '%s' is not defined", 
							   pcLabelName);
			}
		}
	}
}

IMG_UINT32 FindOrCreateLabel(const IMG_CHAR* pszNameIn, IMG_BOOL bCreateOnly, const IMG_CHAR* pszSourceFile, IMG_UINT32 uSourceLine)
/*****************************************************************************
 FUNCTION	: FindOrCreateLabel
    
 PURPOSE	: Find or create a label for the next instruction.

 PARAMETERS	: pszName		- The name of the label
			  
 RETURNS	: The number of the label.
*****************************************************************************/
{
	IMG_UINT32 i;
	IMG_PCHAR pszColon;
	IMG_PCHAR pszName = strdup(pszNameIn);
	if ((pszColon = strrchr(pszName, ':')) != NULL)
	{
		*pszColon = 0;
	}
	for (i = 0; i < g_uLabelCount; i++)
	{
		if (strcmp(g_psLabels[i].pszName, pszName) == 0)
		{
			UseAsm_Free(pszName);
			if (bCreateOnly)
			{
				if (g_psLabels[i].bImported)
				{
#if defined(__psp2__)
					sceClibPrintf("%s(%d): error: Label '%s' is both imported and defined.\n",
						g_psLabels[i].pszImportSourceFile, g_psLabels[i].uImportSourceLine, g_psLabels[i].pszName);
					sceClibPrintf("%s(%d): error: This is the location of the definition.\n",
						pszSourceFile, uSourceLine);
#else
					fprintf(stderr, "%s(%d): error: Label '%s' is both imported and defined.\n",
						g_psLabels[i].pszImportSourceFile, g_psLabels[i].uImportSourceLine, g_psLabels[i].pszName);
					fprintf(stderr, "%s(%d): error: This is the location of the definition.\n",
						pszSourceFile, uSourceLine);
#endif
					g_bAssemblerError = TRUE;
				}
				if (g_psLabels[i].bDefined)
				{
#if defined(__psp2__)
					sceClibPrintf("%s(%d): error: Label '%s' already defined\n"
							"%s(%d): error: this is the location of the previous definition\n", pszSourceFile, uSourceLine,
							g_psLabels[i].pszName, g_psLabels[i].pszSourceFile, g_psLabels[i].uSourceLine);
#else
					fprintf(stderr, "%s(%d): error: Label '%s' already defined\n"
							"%s(%d): error: this is the location of the previous definition\n", pszSourceFile, uSourceLine,
							g_psLabels[i].pszName, g_psLabels[i].pszSourceFile, g_psLabels[i].uSourceLine);
#endif
					g_bAssemblerError = TRUE;
				}
				else
				{
					g_psLabels[i].pszSourceFile = strdup(pszSourceFile);
					g_psLabels[i].uSourceLine = uSourceLine;
					g_psLabels[i].bDefined = TRUE;
				}
			}
			return i;
		}
	}
	g_psLabels = realloc(g_psLabels, (g_uLabelCount + 1) * sizeof(g_psLabels[0]));
	g_psLabels[g_uLabelCount].pszName = pszName;
	g_psLabels[g_uLabelCount].uAddress =  USE_UNDEF;
	if (bCreateOnly)
	{
		g_psLabels[g_uLabelCount].pszSourceFile = strdup(pszSourceFile);
		g_psLabels[g_uLabelCount].uSourceLine = uSourceLine;
		g_psLabels[g_uLabelCount].bDefined = TRUE;
	}
	else
	{
		g_psLabels[g_uLabelCount].bDefined = FALSE;
	}
	g_psLabels[g_uLabelCount].bImported = FALSE;
	g_psLabels[g_uLabelCount].bExported = FALSE;
	g_uLabelCount++;
	return g_uLabelCount - 1;
}

static IMG_UINT32 IMG_CALLCONV UseAssemblerGetLabelAddress(IMG_PVOID pvContext, IMG_UINT32 uLabel)
/*****************************************************************************
 FUNCTION	: UseAssemblerGetLabelAddress
    
 PURPOSE	: Get the address of a label.

 PARAMETERS	: pvContext			- Context
			  uLabel			- Number of the label to get the address for.
			  
 RETURNS	: Either the address of the label if it has been defined or zero.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(pvContext);
	return g_psLabels[uLabel].uAddress;
}

static IMG_VOID IMG_CALLCONV UseAssemblerSetLabelAddress(IMG_PVOID pvContext, IMG_UINT32 uLabel, IMG_UINT32 uAddress)
/*****************************************************************************
 FUNCTION	: UseAssemblerSetLabelAddress
    
 PURPOSE	: Set the address of a label.

 PARAMETERS	: pvContext			- Context.
			  dwLabel			- Number of the label to set the address for.
			  dwAddress			- The address to set.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(pvContext);
	g_psLabels[uLabel].uAddress = uAddress;
}

static IMG_PCHAR IMG_CALLCONV UseAssemblerGetLabelName(IMG_PVOID pvContext, IMG_UINT32 uLabel)
/*****************************************************************************
 FUNCTION	: UseAssemblerGetLabelName
    
 PURPOSE	: Get the name of a label.

 PARAMETERS	: pvContext			- Context.
			  uLabel			- Number of the label to get the name for.
			  
 RETURNS	: The name of the label.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(pvContext);
	return g_psLabels[uLabel].pszName;
}

IMG_VOID AssemblerError(IMG_PVOID pvContext, PUSE_INST psInst, IMG_PCHAR pszFmt, ...)
/*****************************************************************************
 FUNCTION	: AssemblerError
    
 PURPOSE	: Report an error during assembly to the user.

 PARAMETERS	: pvContext		- Caller defined context pointer (unused here)
			  psInst		- Instruction that caused the error.
			  pszFmt, ...	- Printf format string and arguments.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	va_list ap;

	PVR_UNREFERENCED_PARAMETER(pvContext);

#if defined(__psp2__)
	sceClibPrintf("%s(%d): AssemblerError", psInst->pszSourceFile, psInst->uSourceLine);
#else
	va_start(ap, pszFmt);
	fprintf(stderr, "%s(%d): error: ", psInst->pszSourceFile, psInst->uSourceLine);
	vfprintf(stderr, pszFmt, ap);
	fprintf(stderr, ".\n");
	va_end(ap);
#endif

	g_bAssemblerError = TRUE;
}

IMG_VOID AssemblerWarning(PUSE_INST psInst, IMG_PCHAR pszFmt, ...)
/*****************************************************************************
 FUNCTION	: AssemblerWarning
    
 PURPOSE	: Report a warning during assembly to the user.

 PARAMETERS	: psInst			- Instruction that caused the error.
			  pszFmt, ...		- Printf format string and arguments.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	va_list ap;

#if defined(__psp2__)
	sceClibPrintf("%s(%d): AssemblerWarning", psInst->pszSourceFile, psInst->uSourceLine);
#else
	va_start(ap, pszFmt);
	fprintf(stderr, "%s(%d): warning: ", psInst->pszSourceFile, psInst->uSourceLine);
	vfprintf(stderr, pszFmt, ap);
	fprintf(stderr, ".\n");
	va_end(ap);
#endif
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static IMG_UINT32 FloatToVec34Constant(IMG_FLOAT fF)
/*****************************************************************************
 FUNCTION	: FloatToVec34Constant

 PURPOSE	: Look for a hardware constant which matches a floating point value.

 PARAMETERS	: fF				- Floating point value.

 RETURNS	: The index of the matching constant or -1 if none matched.
*****************************************************************************/
{
	IMG_UINT32	uCstIdx;
	IMG_UINT32	uF = *(IMG_PUINT32)&fF;

	for (uCstIdx = 0; uCstIdx < SGXVEC_USE_SPECIAL_NUM_FLOAT_CONSTANTS; uCstIdx++)
	{
		PCUSETAB_SPECIAL_CONST	psConst = &g_asVecHardwareConstants[uCstIdx];

		if (!psConst->bReserved && psConst->auValue[0] == uF)
		{
			return uCstIdx;
		}
	}
	return USE_UNDEF;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static IMG_VOID ReplaceFloatImmediates(PUSE_INST psInst)
/*****************************************************************************
 FUNCTION	: ReplaceFloatImmediates
    
 PURPOSE	: Replace floating point immediates by hardware constants.

 PARAMETERS	: psInst			- Instruction.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uArgIdx;
	IMG_UINT32	uArgCount;

	uArgCount = OpcodeArgumentCount(psInst->uOpcode);
	if (psInst->uFlags1 & USEASM_OPFLAGS1_TESTENABLE)
	{
		/*
			Add in the extra argument for the predicate destination.
		*/
		uArgCount++;
	}

	for (uArgIdx = 0; uArgIdx < uArgCount; uArgIdx++)
	{
		PUSE_REGISTER	psRegister = &psInst->asArg[uArgIdx];

		if (psRegister->uType == USEASM_REGTYPE_FLOATIMMEDIATE)
		{
			IMG_UINT32	uFPConstNum;
			IMG_FLOAT	fFloatImm;

			/*
				Get the value of the immediate in floating point
				format.
			*/
			fFloatImm = *(IMG_PFLOAT)&psRegister->uNumber;
	
			/*
				Check if the immediate has the same value as a floating
				point constant.
			*/
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			if (SupportsVEC34(g_psTargetCoreDesc))
			{
				uFPConstNum = FloatToVec34Constant(fFloatImm);
			}
			else
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
			{
				uFPConstNum = FloatToCstIndex(fFloatImm);
			}
			if (uFPConstNum == USE_UNDEF)
			{
				/*
					Map to an untyped immediate.
				*/
				psRegister->uType = USEASM_REGTYPE_IMMEDIATE;
			}
			else
			{
				/*
					Replace the immediate by the equivalent constant.
				*/
				psRegister->uType = USEASM_REGTYPE_FPCONSTANT;
				psRegister->uNumber = uFPConstNum;
			}
		}
	}
}

static IMG_BOOL CanSetSkipinvFlag(PCSGX_CORE_DESC psTarget, PUSE_INST psInst)
/*****************************************************************************
 FUNCTION	: CanSetSkipinvFlag
    
 PURPOSE	: Checks for an instruction which supports the SKIPINVALID flag.

 PARAMETERS	: psTarget			- Target core.
			  psInst			- Instruction.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	#if defined(SUPPORT_SGX_FEATURE_USE_IDF_SUPPORTS_SKIPINVALID)
	if (psInst->uOpcode == USEASM_OP_IDF && SupportSkipInvalidOnIDFInstruction(psTarget))
	{
		return IMG_TRUE;
	}
	#else
	PVR_UNREFERENCED_PARAMETER(psTarget);
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_IDF_SUPPORTS_SKIPINVALID) */
	if (!OpcodeAcceptsSkipInv(psInst->uOpcode))
	{
		return FALSE;
	}
	if (psInst->uOpcode == USEASM_OP_MOV && psInst->asArg[0].uType == USEASM_REGTYPE_LINK)
	{
		return FALSE;
	}
	if (psInst->uOpcode == USEASM_OP_MOV && psInst->asArg[1].uType == USEASM_REGTYPE_LINK)
	{
		return FALSE;
	}
	return TRUE;
}

#if !defined(__psp2__)
#if !defined(LINUX)
int __cdecl main (int argc, char* argv[])
#else
int main(int argc, char* argv[])
#endif
{
	FILE* fInFile, *fOutFile;
	static IMG_CHAR pszOutPrefix[] = ".bin";
	IMG_PUINT32 puOutput;
	IMG_UINT32 uInstCount;
	IMG_BOOL bQuiet = FALSE;
	IMG_BOOL bListing = FALSE;
	IMG_PCHAR pszOutFileName;
	IMG_PUINT32 puInst;
	PUSE_INST psInst;
	struct
	{
		IMG_PCHAR pszSourceFile;
		IMG_UINT32 uSourceLine;
	} *psSourceOffsets;
	IMG_PCHAR pszLabelHeaderFileName;
	IMG_PCHAR pszLabelPrefix;
	static IMG_CHAR pszLabelPostfix[] = "_OFFSET";
	USEASM_CONTEXT sContext;
#if defined(ENABLE_USEOPT)
	IMG_BOOL bOptimise = FALSE;
	USEOPT_DATA sUseoptData = USEOPT_DATA_DEFAULT;
#endif /* defined(ENABLE_USEOPT) */
	IMG_BOOL bTargetCoreSetOnCommandLine = IMG_FALSE;
	IMG_BOOL bTargetRevisionSetOnCommandLine = IMG_FALSE;

	/*
		Initialize the support for parsing C types.
	*/
	CTreeInitialize();
	
	/*
		Intialize h/w regs allocation range	
	*/
	InitHwRegsAllocRange();

	/*
		Get the command line arguments.
	*/
	g_uCodeOffset = 0;
	pszLabelHeaderFileName = NULL;
	pszLabelPrefix = "";
	while (argc > 1 && argv[1][0] == '-' && argv[1][1] != '\0')
	{
		if (strncmp(argv[1], "-offset=", strlen("-offset=")) == 0)
		{
			g_uCodeOffset = strtoul(argv[1] + strlen("-offset="), NULL, 0);
			if ((g_uCodeOffset % 8) != 0)
			{
				fprintf(stderr, "The code offset should be aligned to the length of an instruction (8 bytes)");
				return 1;
			}
			g_uCodeOffset >>= 3;
			memmove(&argv[1], &argv[2], (argc - 2) * sizeof(argv[1]));
			argc--;
		}
		else if (strncmp(argv[1], "-label_prefix=", strlen("-label_prefix=")) == 0)
		{
			pszLabelPrefix = strdup(argv[1] + strlen("-label_prefix="));
			memmove(&argv[1], &argv[2], (argc - 2) * sizeof(argv[1]));
			argc--;
		}
		else if (strncmp(argv[1], "-label_header=", strlen("-label_header=")) == 0)
		{
			pszLabelHeaderFileName = strdup(argv[1] + strlen("-label_header="));
			memmove(&argv[1], &argv[2], (argc - 2) * sizeof(argv[1]));
			argc--;
		}
		else if (strncmp(argv[1], "-quiet", strlen("-quiet")) == 0)
		{
			bQuiet = TRUE;
			memmove(&argv[1], &argv[2], (argc - 2) * sizeof(argv[1]));
			argc--;
		}
#if defined(ENABLE_USEOPT)
		else if (strncmp(argv[1], "-opt", strlen("-opt")) == 0)
		{
			bOptimise = TRUE;
			memmove(&argv[1], &argv[2], (argc - 2) * sizeof(argv[1]));
			argc--;
		}
#endif /* defined(ENABLE_USEOPT) */
		else if (strncmp(argv[1], "-listing", strlen("-listing")) == 0 ||
				 strncmp(argv[1], "-l", strlen("-l")) == 0)
		{
			bListing = TRUE;
			memmove(&argv[1], &argv[2], (argc - 2) * sizeof(argv[1]));
			argc--;
		}
		else if (strncmp(argv[1], "-target=", strlen("-target=")) == 0)
		{
			IMG_PCHAR pszTarget = argv[1] + strlen("-target=");

			if ((g_sTargetCoreInfo.eID = ParseCoreSpecifier(pszTarget)) == SGX_CORE_ID_INVALID)
			{	
				fprintf(stderr, "%s: Unknown target %s.\n", argv[0], pszTarget);
				return 1;
			}

			if (!CheckCore(g_sTargetCoreInfo.eID))
			{
				fprintf(stderr, "This target processor isn't supported by this build of useasm.\n");
				return 1;
			}

			bTargetCoreSetOnCommandLine = IMG_TRUE;

			memmove(&argv[1], &argv[2], (argc - 2) * sizeof(argv[1]));
			argc--;
		}
		else if (strncmp(argv[1], "-targetrev=", strlen("-targetrev=")) == 0)
		{
			IMG_PCHAR pszTarget = argv[1] + strlen("-targetrev=");

			if ((g_sTargetCoreInfo.uiRev = ParseRevisionSpecifier(pszTarget)) == 0)
			{	
				fprintf(stderr, "%s: Invalid target revision %s.\n", argv[0], pszTarget);
				return 1;
			}

			bTargetRevisionSetOnCommandLine = IMG_TRUE;

			memmove(&argv[1], &argv[2], (argc - 2) * sizeof(argv[1]));
			argc--;
		}
		else if (strncmp(argv[1], "-fixpairs", strlen("-fixpairs")) == 0)
		{
			g_bFixInvalidPairs = IMG_TRUE;

			memmove(&argv[1], &argv[2], (argc - 2) * sizeof(argv[1]));
			argc--;
		}
		else if (strncmp(argv[1], "-obj", strlen("-obj")) == 0)
		{
			g_bWriteObjectFile = IMG_TRUE;

			memmove(&argv[1], &argv[2], (argc - 2) * sizeof(argv[1]));
			argc--;
		}
		else if (strncmp(argv[1], "-cpp", strlen("-cpp")) == 0)
		{
			g_bCPreprocessor = IMG_TRUE;

			memmove(&argv[1], &argv[2], (argc - 2) * sizeof(argv[1]));
			argc--;
		}
		else if (strncmp(argv[1], "-enablescopes", strlen("-enablescopes")) == 
			0)
		{
			EnableScopeManagement();
			memmove(&argv[1], &argv[2], (argc - 2) * sizeof(argv[1]));
			argc--;
		}
		else if (strncmp(argv[1], "-r=", strlen("-r=")) == 0)
		{
			if(IsHwRegsAllocRangeSet() == IMG_FALSE)
			{
				IMG_PCHAR pszInterArgument = strdup(argv[1]);
				IMG_PCHAR pszSecondNumber = NULL;
				IMG_PCHAR pszFirstNumber = pszInterArgument + strlen("-r=");
				IMG_UINT32 uRangeStart = 0;
				IMG_UINT32 uRangeEnd = 0;
				if((pszSecondNumber = strrchr(pszFirstNumber, '-')) != NULL)
				{
					*pszSecondNumber = '\0';
					if(IsAllDigitsString(pszFirstNumber) == IMG_TRUE)
					{
						uRangeStart = (int) atoi(pszFirstNumber);
					}
					else
					{
						fprintf(stderr, "%s: Invalid first number syntax '%s' in range value.\n", 
							argv[0], pszFirstNumber);
						UseAsm_Free(pszInterArgument);
						return 1;
					}
					pszSecondNumber++;
					if(IsAllDigitsString(pszSecondNumber) == IMG_TRUE)
					{
						uRangeEnd = (int) atoi(pszSecondNumber);
					}
					else
					{
						fprintf(stderr, "%s: Invalid second number syntax '%s' in range value.\n", 
							argv[0], pszSecondNumber);
						UseAsm_Free(pszInterArgument);
						return 1;
					}
				}
				else
				{
					fprintf(stderr, "%s: Invalid range value syntax '%s'.\n", 
						argv[0], pszFirstNumber);
					UseAsm_Free(pszInterArgument);
					return 1;
				}
				if(uRangeStart > uRangeEnd)
				{
					fprintf(stderr, "%s: Range start '%u' can not be larger than range end '%u'.\n", 
						argv[0], uRangeStart, uRangeEnd);
					UseAsm_Free(pszInterArgument);
					return 1;
				}
				SetHwRegsAllocRange(uRangeStart, uRangeEnd, IMG_FALSE);	
			}
			else
			{
				fprintf(stderr, "%s: Can not set h/w regs alloc range more than once from command line '%s'.\n", 
						argv[0], argv[1]);				
				return 1;
			}
			memmove(&argv[1], &argv[2], (argc - 2) * sizeof(argv[1]));
			argc--;
		}
		else if (strncmp(argv[1], "-rf=", strlen("-rf=")) == 0)
		{			
			if(IsHwRegsAllocRangeSet() == IMG_FALSE)
			{
				IMG_PCHAR pszInterArgument = strdup(argv[1]);
				IMG_PCHAR pszSecondNumber = NULL;
				IMG_PCHAR pszFirstNumber = pszInterArgument + strlen("-rf=");
				IMG_UINT32 uRangeStart = 0;
				IMG_UINT32 uRangeEnd = 0;
				if((pszSecondNumber = strrchr(pszFirstNumber, '-')) != NULL)
				{
					*pszSecondNumber = '\0';
					if(IsAllDigitsString(pszFirstNumber) == IMG_TRUE)
					{
						uRangeStart = (int) atoi(pszFirstNumber);
					}
					else
					{
						fprintf(stderr, "%s: Invalid first number syntax '%s' in range value.\n", 
							argv[0], pszFirstNumber);
						UseAsm_Free(pszInterArgument);
						return 1;
					}
					pszSecondNumber++;
					if(IsAllDigitsString(pszSecondNumber) == IMG_TRUE)
					{
						uRangeEnd = (int) atoi(pszSecondNumber);
					}
					else
					{
						fprintf(stderr, "%s: Invalid second number syntax '%s' in range value.\n", 
							argv[0], pszSecondNumber);
						UseAsm_Free(pszInterArgument);
						return 1;
					}
				}
				else
				{
					fprintf(stderr, "%s: Invalid range value syntax '%s'.\n", argv[0], 
						pszFirstNumber);
					UseAsm_Free(pszInterArgument);
					return 1;
				}
				if(uRangeStart > uRangeEnd)
				{
					fprintf(stderr, "%s: Range start '%u' can not be larger than range end '%u'.\n", 
						argv[0], uRangeStart, uRangeEnd);
					UseAsm_Free(pszInterArgument);
					return 1;				
				}
				SetHwRegsAllocRange(uRangeStart, uRangeEnd, IMG_TRUE);
			}
			else
			{
				fprintf(stderr, "%s: Can not set h/w regs alloc range more than once from command line '%s'.\n", 
					argv[0], argv[1]);				
				return 1;
			}
			memmove(&argv[1], &argv[2], (argc - 2) * sizeof(argv[1]));
			argc--;
		}
		else
		{
			usage();
			return 1;
		}
	}

	if (argc == 1)
	{
		usage();
		return 1;
	}

	if (argv[1][0] == '-' && argv[1][1] == '\0')
	{
		g_pszInFileName = strdup("stdin");
		yyin = fInFile = stdin;
#if !defined(LINUX)
		_setmode(_fileno(stdin), _O_BINARY);
#endif /* defined(LINUX) */
	}
	else
	{
		g_pszInFileName = strdup(argv[1]);
		yyin = fInFile = fopen(argv[1], "rb");
		if (fInFile == NULL)
		{
			fprintf(stderr, "%s: Couldn't open input file %s: %s.\n", argv[0], argv[1], strerror(errno));
			return 1;
		}
	}

	if (argc == 2)
	{
		if (argv[1][0] == '-' && argv[1][1] == '\0')
		{
			pszOutFileName = strdup("out.bin");
		}
		else
		{
			char *pszDot;

			if ((pszDot = strrchr(argv[1], '.')) == NULL)
			{
				pszOutFileName = UseAsm_Malloc(strlen(argv[1]) + strlen(pszOutPrefix) + 1);
				strcpy(pszOutFileName, argv[1]);
				strcat(pszOutFileName, pszOutPrefix);
			}
			else
			{
				pszOutFileName = UseAsm_Malloc((pszDot - argv[1]) + strlen(pszOutPrefix) + 1);
				UseAsm_MemCopy(pszOutFileName, argv[1], pszDot - argv[1]);
				UseAsm_MemCopy(pszOutFileName + (pszDot - argv[1]), pszOutPrefix, strlen(pszOutPrefix) + 1);
			}
		}

		
	}
	else
	{
		pszOutFileName = strdup(argv[2]);
	}

	if (bTargetCoreSetOnCommandLine)
	{
		/*
			Default to the head revision if only the core is specified.
		*/
		if (!bTargetRevisionSetOnCommandLine)
		{
			g_sTargetCoreInfo.uiRev = 0;
		}

		/*
			Check this core/revision is one we have information about.
		*/
		g_psTargetCoreDesc = UseAsmGetCoreDesc(&g_sTargetCoreInfo);
		if (g_psTargetCoreDesc == NULL)
		{
			fprintf(stderr, "Invalid Core revision specified.\n");
			return 1;
		}
	}
	else if (bTargetRevisionSetOnCommandLine)
	{
		fprintf(stderr, "%s: Set target revision without setting target core.\n", argv[0]);
		return 1;
	}

	UseAsm_MemSet(&g_sUseasmParserOptState, 0, sizeof(g_sUseasmParserOptState));

	g_uInstCount = 0;
	g_psInstListHead = NULL;
	g_uSourceLine = 1;

	g_uLabelCount = 0;
	g_psLabels = NULL;

	g_uSchedOffCount = 0;
	g_psLastNoSchedInst = NULL;
	g_uSkipInvOnCount = 0;
	g_uRepeatStackTop = 0;
	
	InitScopeManagementDataStructs();

	g_bStartedNoSchedRegion = FALSE;

	g_uInstOffset = g_uCodeOffset;

	g_bAssemblerError = FALSE;

	yydebug = 0;
	g_uParserError = 0;
	if (yyparse() != 0)
	{
		fclose(fInFile);
		UseAsm_Free(pszOutFileName);
		return 1;
	}
	VerifyThatAllScopesAreEnded();
	if (g_uInstCount == 0)
	{
		fprintf(stderr, "No instructions defined.\n");
		fclose(fInFile);
		UseAsm_Free(pszOutFileName);
		return 1;
	}
#if defined(ENABLE_USEOPT)
	if (g_sUseasmParserOptState.psOutputRegs != NULL)
	{
		/* Process output registers data for the optimiser */
		USEASM_PARSER_REG_LIST *psCurr, *psNext;
		IMG_UINT32 uLen;
		
		psNext = NULL;
		uLen = 0;
		for (psCurr = g_sUseasmParserOptState.psOutputRegs;
			 psCurr != NULL;
			 psCurr = psNext)
		{
			psNext = psCurr->psNext;

			if (bOptimise)
			{
				uLen += 1;
			}
			else
			{
				UseAsm_Free(psCurr);
			}
		}
		if (bOptimise)
		{
			IMG_UINT32 uIdx;

			sUseoptData.uNumOutRegs = uLen;
			sUseoptData.asOutRegs = (USE_REGISTER*)UseAsm_Malloc(sizeof(USE_REGISTER) * uLen);

			uIdx = 0;
			psNext = NULL;
			for (psCurr = g_sUseasmParserOptState.psOutputRegs;
				 psCurr != NULL;
				 psCurr = psNext)
			{
				psNext = psCurr->psNext;
				
				UseAsmInitReg(&sUseoptData.asOutRegs[uIdx]);

				sUseoptData.asOutRegs[uIdx].uType = psCurr->uType;
				sUseoptData.asOutRegs[uIdx].uNumber = psCurr->uNumber;

				uIdx += 1;
				UseAsm_Free(psCurr);
			}
		}
		else
		{
			sUseoptData.uNumOutRegs = 0;
			sUseoptData.asOutRegs = NULL;
		}
	}
#endif /* defined(ENABLE_USEOPT) */

	if (g_uSchedOffCount > 0)
	{
		SetNoSchedFlag(g_psFirstNoSchedInst,
					   g_uFirstNoSchedInstOffset,
					   g_psLastNoSchedInst,
					   g_uLastNoSchedInstOffset);
	}
	if (g_sTargetCoreInfo.eID == SGX_CORE_ID_INVALID)
	{
		g_sTargetCoreInfo.eID = GetDefaultCore();
		if (g_sTargetCoreInfo.eID == SGX_CORE_ID_INVALID)
		{
			fprintf(stderr, "No default target for this build of useasm.\n");
			return 1;
		}
		g_psTargetCoreDesc = UseAsmGetCoreDesc(&g_sTargetCoreInfo);
	}

	/*
		Replace references to immediate floating point values by a hardware constant with the
		same value where possible.
	*/
	for (psInst = g_psInstListHead; psInst != NULL; psInst = psInst->psNext)
	{
		ReplaceFloatImmediates(psInst);
	}

	/*
		For instructions inside regions delimited by .SKIPINVON and .SKIPINVOFF
		set the SKIPINVALID flag if the instruction supports it.
	*/
	for (psInst = g_psInstListHead; psInst != NULL; psInst = psInst->psNext)
	{
		if (psInst->uFlags3 & USEASM_OPFLAGS3_SKIPINVALIDON)
		{
			if (CanSetSkipinvFlag(g_psTargetCoreDesc, psInst))
			{
				psInst->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
			}

			psInst->uFlags3 &= ~USEASM_OPFLAGS3_SKIPINVALIDON;
		}
	}

	/*
		Map MOV instructions to LIMM.
	*/
	for (psInst = g_psInstListHead; psInst != NULL; psInst = psInst->psNext)
	{
		if (
				psInst->uOpcode == USEASM_OP_MOV &&
				psInst->asArg[0].uType != USEASM_REGTYPE_LINK &&
				(
					psInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE ||
					psInst->asArg[1].uType == USEASM_REGTYPE_LABEL ||
					psInst->asArg[1].uType == USEASM_REGTYPE_LABEL_WITH_OFFSET ||
					psInst->asArg[1].uType == USEASM_REGTYPE_REF
				) &&
				(psInst->uFlags1 & USEASM_OPFLAGS1_SYNCSTART) == 0 &&
				(psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) <= 1
		   )
		{
			psInst->uOpcode = USEASM_OP_LIMM;
		}
	}

	puOutput = UseAsm_Malloc(g_uInstCount * EURASIA_USE_INSTRUCTION_SIZE);
	psSourceOffsets = UseAsm_Malloc(g_uInstCount * sizeof(*psSourceOffsets));
	puInst = puOutput;
	sContext.pvContext = NULL;
	sContext.pvLabelState = NULL;
	sContext.pfnRealloc = UseasmRealloc;
	sContext.pfnGetLabelAddress = UseAssemblerGetLabelAddress;
	sContext.pfnSetLabelAddress = UseAssemblerSetLabelAddress;
	sContext.pfnGetLabelName = UseAssemblerGetLabelName;
	sContext.pfnAssemblerError = AssemblerError;
	sContext.pfnLADDRNotify = UseAssemblerLADDRNotify;
	AllocHwRegsToNamdTempRegsAndSetLabelRefs(g_psInstListHead);
	psInst = NULL;

#if defined(ENABLE_USEOPT)
	/* Assemble the program */
	if (bOptimise)
	{
		sUseoptData.pfnAlloc = UseasmAlloc;
		sUseoptData.pfnFree = UseasmFree;

		sUseoptData.psProgram = g_psInstListHead;
        sUseoptData.psStart = g_psInstListHead;

		sUseoptData.uNumTempRegs = g_sUseasmParserOptState.uMaxTemp + 1;
		sUseoptData.auKeepTempReg = NULL;

		sUseoptData.uNumPARegs = g_sUseasmParserOptState.uMaxPrimAttr + 1;
		sUseoptData.auKeepPAReg = NULL;

		sUseoptData.uNumOutputRegs = g_sUseasmParserOptState.uMaxOutput + 1;
		sUseoptData.auKeepOutputReg = NULL;

        UseoptProgram(&g_sTargetProcessorInfo,
                      &sContext,
                      &sUseoptData);

        g_psInstListHead = sUseoptData.psProgram;

        /* Clean up optimiser data */
        UseAsm_Free(sUseoptData.auKeepTempReg);
        UseAsm_Free(sUseoptData.auKeepPAReg);
        UseAsm_Free(sUseoptData.auKeepOutputReg);
        UseAsm_Free(sUseoptData.asOutRegs);
        sUseoptData.asOutRegs = NULL;
	}
#endif /* defined(ENABLE_USEOPT) */

	for (psInst = g_psInstListHead; psInst != NULL; psInst = psInst->psNext)
	{
		IMG_UINT32 uInstSpace;
		uInstSpace = UseAssembleInstruction(g_psTargetCoreDesc, 
											psInst,
											puOutput, 
											puInst,
											g_uCodeOffset,
											&sContext);
		if (uInstSpace == USE_UNDEF)
		{
			fprintf(stderr, "%s: Out of memory.\n", argv[0]);
			UseAsm_Free(puOutput);
			fclose(fInFile);
			UseAsm_Free(pszOutFileName);
			return 1;
		}
		if (uInstSpace > 0)
		{
			psSourceOffsets[(puInst - puOutput) / 2].pszSourceFile = psInst->pszSourceFile;
			psSourceOffsets[(puInst - puOutput) / 2].uSourceLine = psInst->uSourceLine;
		}
		puInst += uInstSpace;
	}

	if (g_bWriteObjectFile)
	{
		ImportUndefinedLabels(&sContext, puOutput);
	}
	else
	{
		CheckUndefinedLabels(&sContext);
	}
	uInstCount = (puInst - puOutput) / 2;
	if (g_bAssemblerError || g_uParserError || g_bCCodeError)
	{
		UseAsm_Free(puOutput);
		fclose(fInFile);
		UseAsm_Free(pszOutFileName);
		return 1;
	}

	if (g_bWriteObjectFile)
	{
		if (!PrepareExportSegment())
		{
			return 1;
		}
	}

	fOutFile = fopen(pszOutFileName, "wb");
	if (fOutFile == NULL)
	{
		fprintf(stderr, "%s: Couldn't open output file %s: %s.\n", argv[0], pszOutFileName, strerror(errno));
		UseAsm_Free(pszOutFileName);
		fclose(fInFile);
		return 1;
	}

	if (g_bWriteObjectFile)
	{
		if (!WriteObjectFile(uInstCount, puOutput, fOutFile))
		{
			return 1;
		}
	}
	else
	{
		if (fwrite(puOutput, EURASIA_USE_INSTRUCTION_SIZE, uInstCount, fOutFile) != uInstCount)
		{
			fprintf(stderr, "Couldn't write output: %s.\n", strerror(errno));
			UseAsm_Free(puOutput);
			fclose(fInFile);
			fclose(fOutFile);
			UseAsm_Free(pszOutFileName);
			return 1;
		}
		
	}

	fclose(fInFile);
	fclose(fOutFile);

	if (!bQuiet)
	{
		IMG_UINT32 i;
		IMG_UINT32 uInstCountDigits;
		IMG_PUINT32 puInstructions = puOutput;

		for (i = 10, uInstCountDigits = 1; i < uInstCount; i *= 10, uInstCountDigits++);
	
		for (i = 0; i < uInstCount; i++, puInstructions+=2)
		{
			IMG_UINT32 uInst0 = puInstructions[0];
			IMG_UINT32 uInst1 = puInstructions[1];
			IMG_CHAR pszInst[256];
	
			if (bListing &&
				(i == 0 || 
				 strcmp(psSourceOffsets[i].pszSourceFile, psSourceOffsets[i - 1].pszSourceFile) != 0 ||
				 psSourceOffsets[i].uSourceLine != psSourceOffsets[i - 1].uSourceLine))
			{
				printf("%s(%d):\n", psSourceOffsets[i].pszSourceFile, psSourceOffsets[i].uSourceLine);
			}
			printf("%*u: 0x%.8X%.8X\t", uInstCountDigits, i, uInst1, uInst0);

			UseDisassembleInstruction(g_psTargetCoreDesc, uInst0, uInst1, pszInst);
			printf("%s\n", pszInst);
		}
	}

	UseAsm_Free(psSourceOffsets);
	UseAsm_Free(puOutput);
	UseAsm_Free(pszOutFileName);

	if (pszLabelHeaderFileName != NULL)
	{	
		FILE*		fLabelHeaderFile;
		IMG_UINT32	i;
		IMG_UINT32	uMaxLabelNameLength;

		fLabelHeaderFile = fopen(pszLabelHeaderFileName, "wb");
		if (fLabelHeaderFile == NULL)
		{
			fprintf(stderr, "%s: Couldn't open label header file %s: %s.\n", argv[0], pszLabelHeaderFileName, strerror(errno)); 
			UseAsm_Free(pszLabelHeaderFileName);
			return 1;
		}

		uMaxLabelNameLength = 0;
		for (i = 0; i < g_uLabelCount; i++)
		{
			uMaxLabelNameLength = max(uMaxLabelNameLength, strlen(g_psLabels[i].pszName));
		}
		uMaxLabelNameLength += strlen(pszLabelPrefix);
		uMaxLabelNameLength += strlen(pszLabelPostfix);

		if (fprintf(fLabelHeaderFile, "/* Auto-generated file - don't edit */\n\n") < 0)
		{
			fprintf(stderr, "Couldn't write label header file: %s.\n", strerror(errno));
			fclose(fLabelHeaderFile);
			UseAsm_Free(pszLabelHeaderFileName);
			return 1;
		}

		for (i = 0; i < g_uLabelCount; i++)
		{
			IMG_PCHAR	pszDefineName;
			IMG_UINT32	j;

			pszDefineName = UseAsm_Malloc(strlen(pszLabelPrefix) + strlen(g_psLabels[i].pszName) + strlen(pszLabelPostfix) + 1);

			strcpy(pszDefineName, pszLabelPrefix);
			strcat(pszDefineName, g_psLabels[i].pszName);
			strcat(pszDefineName, pszLabelPostfix);

			for (j = 0; j < strlen(pszDefineName); j++)
			{
				pszDefineName[j] = (IMG_CHAR)toupper(pszDefineName[j]);
			}

			if (fprintf(fLabelHeaderFile, "#define %-*s\t0x%.8X\n\n", (int)uMaxLabelNameLength, pszDefineName, g_psLabels[i].uAddress << 3) < 0)
			{
				fprintf(stderr, "Couldn't write label header file: %s.\n", strerror(errno));
				fclose(fLabelHeaderFile);
				UseAsm_Free(pszLabelHeaderFileName);
				UseAsm_Free(pszDefineName);
				return 1;
			}

			UseAsm_Free(pszDefineName);
		}

		fclose(fLabelHeaderFile);

		UseAsm_Free(pszLabelHeaderFileName);
	}

	return 0;
}
#endif

/******************************************************************************
 End of file (main.c)
******************************************************************************/
