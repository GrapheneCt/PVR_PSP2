/* Auto-generated file - don't edit. */

#define PDS_PROGRAM_DATA_SEGMENT_SIZE	(48UL)
static const IMG_UINT32 g_pui32PDSProgram[20] = {
0x00000000, 0x00000000, 0x00000000, 0x00000000,
0x00000000, 0x00000000, 0x00000000, 0x00000000,
0x00000000, 0x00000000, 0x00000000, 0x00000000,
0x87000000, 0x90000006, 0x07000103, 0x87040000,
0x90000006, 0x07040103, 0x07080185, 0xAF000000,
};



#ifdef INLINE_IS_PRAGMA
#pragma inline(PDSProgramSetDMA0)
#endif
FORCE_INLINE IMG_VOID PDSProgramSetDMA0 (IMG_PUINT32 pui32Program, IMG_UINT32 ui32Value)
{
	pui32Program[0] = ui32Value;
}
#define PDS_PROGRAM_DMA0_LOCATIONS	{0}

#ifdef INLINE_IS_PRAGMA
#pragma inline(PDSProgramSetDMA1)
#endif
FORCE_INLINE IMG_VOID PDSProgramSetDMA1 (IMG_PUINT32 pui32Program, IMG_UINT32 ui32Value)
{
	pui32Program[1] = ui32Value;
}
#define PDS_PROGRAM_DMA1_LOCATIONS	{4}

#ifdef INLINE_IS_PRAGMA
#pragma inline(PDSProgramSetDMA2)
#endif
FORCE_INLINE IMG_VOID PDSProgramSetDMA2 (IMG_PUINT32 pui32Program, IMG_UINT32 ui32Value)
{
	pui32Program[4] = ui32Value;
}
#define PDS_PROGRAM_DMA2_LOCATIONS	{16}

#ifdef INLINE_IS_PRAGMA
#pragma inline(PDSProgramSetDMA3)
#endif
FORCE_INLINE IMG_VOID PDSProgramSetDMA3 (IMG_PUINT32 pui32Program, IMG_UINT32 ui32Value)
{
	pui32Program[5] = ui32Value;
}
#define PDS_PROGRAM_DMA3_LOCATIONS	{20}

#ifdef INLINE_IS_PRAGMA
#pragma inline(PDSProgramSetUSE0)
#endif
FORCE_INLINE IMG_VOID PDSProgramSetUSE0 (IMG_PUINT32 pui32Program, IMG_UINT32 ui32Value)
{
	pui32Program[8] = ui32Value;
}
#define PDS_PROGRAM_USE0_LOCATIONS	{32}

#ifdef INLINE_IS_PRAGMA
#pragma inline(PDSProgramSetUSE1)
#endif
FORCE_INLINE IMG_VOID PDSProgramSetUSE1 (IMG_PUINT32 pui32Program, IMG_UINT32 ui32Value)
{
	pui32Program[9] = ui32Value;
}
#define PDS_PROGRAM_USE1_LOCATIONS	{36}

#ifdef INLINE_IS_PRAGMA
#pragma inline(PDSProgramSetUSE2)
#endif
FORCE_INLINE IMG_VOID PDSProgramSetUSE2 (IMG_PUINT32 pui32Program, IMG_UINT32 ui32Value)
{
	pui32Program[2] = ui32Value;
}
#define PDS_PROGRAM_USE2_LOCATIONS	{8}

