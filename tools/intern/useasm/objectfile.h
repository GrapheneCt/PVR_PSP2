/*************************************************************************
 * Name         : objectfile.h
 * Title        : Pixel Shader Compiler
 * Author       : John Russell
 * Created      : Jan 2002
 *
 * Copyright    : 2002-2006 by Imagination Technologies Limited. All rights reserved.
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
 * $Log: objectfile.h $
 **************************************************************************/

#define USEASM_OBJFILE_ID	"USEOBJ"

#define HW_REGS_ALLOC_ENABLED

typedef struct
{
	/* Identifies the file as USEASM output. */
	IMG_CHAR		pszFileId[6];
	/* Version of the file format. */
	IMG_UINT32		uVersion;
	/* Target processor the module was assembled for. */
	SGX_CORE_INFO	sTarget;
	/* Alignment requirement (in instructions) for the start of the module. */
	IMG_UINT32		uModuleAlignment;
	/* Count of instructions in the module. */
	IMG_UINT32		uInstructionCount;
	/* Offset in bytes of the instructions within the file. */
	IMG_UINT32		uInstructionOffset;
	/* Count of import structures (USEASM_IMPORTDATA) in the module. */
	IMG_UINT32		uImportDataCount;
	/* Offset in bytes of the imports within the file. */
	IMG_UINT32		uImportDataOffset;
	/* Count of export structures (USEASM_EXPORTDATA) in the module. */
	IMG_UINT32		uExportDataCount;
	/* Offset in bytes of the exports within the file. */
	IMG_UINT32		uExportDataOffset;
	/* Size in bytes of the strings within the file. */
	IMG_UINT32		uStringDataSize;
	/* Offset in bytes of the strings within the file. */
	IMG_UINT32		uStringDataOffset;
	/* Count of LADDR relocation structures in the module. */
	IMG_UINT32		uLADDRRelocationCount;
	/* Offset in bytes of the LADDR relocation data in the module. */
	IMG_UINT32		uLADDRRelocationOffset;
#ifdef HW_REGS_ALLOC_ENABLED
	/* Hardware register allocation range start for named registers. */
	IMG_UINT32		uHwRegsAllocRangeStart;
	/* Hardware register allocation range end for named registers. */
	IMG_UINT32		uHwRegsAllocRangeEnd;
#endif
} USEASM_OBJFILE_HEADER, *PUSEASM_OBJFILE_HEADER;

typedef struct
{
	/* Offset within the string segment of the name of the imported label. */
	IMG_UINT32		uNameOffset;
	/* Type of reference to the import. */
	IMG_UINT32		uFixupOp;
	/* Offset (in instructions) within the module of the reference to the label. */
	IMG_UINT32		uFixupAddress;
	/* Line number of the label reference. */
	IMG_UINT32		uFixupSourceFileNameOffset;
	/* Offset within the string segment of the name of source file for the label reference. */
	IMG_UINT32		uFixupSourceLineNumber;
	/* TRUE if the label reference is a branch with the syncend flag (NOT CURRENTLY USED). */
	IMG_BOOL		bFixupSyncEnd;
	/* Offset to be added onto the resolved label address (NOT CURRENTLY USED). */
	IMG_UINT32		uFixupOffset;
} USEASM_IMPORTDATA, *PUSEASM_IMPORTDATA;

typedef struct
{
	/* Offset within the string segment of the name of the exported label. */
	IMG_UINT32		uNameOffset;
	/* Offset (in instructions) within the module of the label. */
	IMG_UINT32		uLabelAddress;
} USEASM_EXPORTDATA, *PUSEASM_EXPORTDATA;

typedef struct
{
	/* Offset (in instructions) of the LIMM instruction within the module. */
	IMG_UINT32		uAddress;
} USEASM_LADDRRELOCATION, *PUSEASM_LADDRRELOCATION;

/* EOF */

