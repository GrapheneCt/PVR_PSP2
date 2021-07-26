/*****************************************************************************
 Name			: pds.c
 
 Title			: PDS control
 
 C Author 		: Paul Burgess
 
 Created  		: 24/03/05
 
 Copyright		: 2005-2010 by Imagination Technologies Limited. All rights reserved.
				  No part of this software, either material or conceptual may
				  be copied	or distributed, transmitted, transcribed, stored
				  in a retrieval system or translated into any human or
				  computer language in any form by any means, electronic,
				  mechanical, manual or otherwise, or disclosed to third
				  parties without the express written permission of Imagination Technologies
				  Limited, Home Park Estate, Kings Langley, Herts, WD4 8LZ, UK
				  
 Description	: PDS control.
 
 Program Type	: 32-bit DLL
 
 Version	 	: $Revision: 1.223 $
 
 Modifications	:
 
 $Log: pds.c $
 /
  --- Revision Logs Removed --- 
 
  --- Revision Logs Removed --- 
 ..
  --- Revision Logs Removed --- 
 
*****************************************************************************/

#include <string.h>

#include "img_defs.h"
#include "sgxdefs.h"
#include "sgxpdsdefs.h"

#include "pixelevent.h"

#if (defined(FIX_HW_BRN_31175) || !defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)) && !defined(PDS_BUILD_OPENGLES) && !defined(PDS_BUILD_SERVICES)
	#include "pixelevent_msnods_4x.h"
	#include "pixelevent_dsnoms_4x.h"
	
	#if defined(SGX_FEATURE_MULTISAMPLE_2X)
		#if defined(SGX_FEATURE_MSAA_2X_IN_X)
			#include "pixelevent_msnods_2x_inX.h"
			#include "pixelevent_dsnoms_2x_inX.h"
		#else
			#include "pixelevent_msnods_2x_inY.h"
			#include "pixelevent_dsnoms_2x_inY.h"
		#endif
	#endif
#endif

#include "pixelevent_tilexy.h"

	#if defined(PDS_BUILD_OPENGLES) || defined(PDS_BUILD_D3DM) || defined(PDS_BUILD_SERVICES)
		#include "pvr_debug.h"
		#define PDS_ASSERT(a) PVR_ASSERT((a))
		#define PDS_DPF(a)    PVR_DPF((PVR_DBG_VERBOSE, (a)))
	#else
		#if defined(PDS_BUILD_D3D)
			#pragma message ("Building D3D version of PDS code")
			
			#pragma warning ( disable : 4127 )
			
			#include "d3dctl.h"
			#define PDS_ASSERT(a) ASSERT(a)
			#define PDS_DPF(a)    DPFL6(a)
		#else
			#if defined(PDS_BUILD_D3DVISTA)
				#pragma message ("Building D3DVista version of PDS code")
				
				#include "debug.h"
				#define PDS_ASSERT(a) ASSERT(a)
				#define PDS_DPF(a)	DPFPDSCG(a)
			#else
				#pragma message ("Unknown version of PDS code")
			#endif
		#endif
	#endif

#include "pds.h"

#include "pixelevent_sizeof.h"
#include "pixelevent_tilexy_sizeof.h"

#if defined(FIX_HW_BRN_31175) && !defined(PDS_BUILD_OPENGLES)
#include "pixelevent_dsnoms_2x_inX_sizeof.h"
#include "pixelevent_dsnoms_2x_inY_sizeof.h"
#include "pixelevent_dsnoms_4x_sizeof.h"
#include "pixelevent_msnods_2x_inX_sizeof.h"
#include "pixelevent_msnods_2x_inY_sizeof.h"
#include "pixelevent_msnods_4x_sizeof.h"
#endif

/*
	Pixel event program size
*/
#if defined(FIX_HW_BRN_31175) && !defined(PDS_BUILD_OPENGLES)
#if !defined(MAX)
#define MAX(x, y) (x>y?x:y)
#endif
#define PDS_PIXEVENT_PROG_SIZE			MAX(PDS_PIXELEVENT_SIZE, MAX(PDS_PIXELEVENTDOWNSCALENOMSAA_2X_SIZE, MAX(PDS_PIXELEVENTDOWNSCALENOMSAA_2X_SIZE, MAX(PDS_PIXELEVENTDOWNSCALENOMSAA_4X_SIZE, MAX(PDS_PIXELEVENTMSAANODOWNSCALE_2X_SIZE, MAX(PDS_PIXELEVENTMSAANODOWNSCALE_2X_SIZE, PDS_PIXELEVENTMSAANODOWNSCALE_4X_SIZE))))))
#else
#define PDS_PIXEVENT_PROG_SIZE			PDS_PIXELEVENT_SIZE
#endif

#if !defined(FIX_HW_BRN_25339) && !defined(FIX_HW_BRN_27330)

/*****************************************************************************
 Macro definitions
*****************************************************************************/

#define	PDS_NUM_WORDS_PER_DWORD		2
#define	PDS_NUM_DWORDS_PER_REG		2

#define	PDS_DS0_CONST_BLOCK_BASE	0
#define	PDS_DS0_CONST_BLOCK_SIZE	EURASIA_PDS_DATASTORE_TEMPSTART
#define	PDS_DS0_TEMP_BLOCK_BASE		EURASIA_PDS_DATASTORE_TEMPSTART
#define	PDS_DS1_CONST_BLOCK_BASE	0
#define	PDS_DS1_CONST_BLOCK_SIZE	EURASIA_PDS_DATASTORE_TEMPSTART
#define	PDS_DS1_TEMP_BLOCK_BASE		EURASIA_PDS_DATASTORE_TEMPSTART

#define	PDS_DS0_INDEX				(PDS_DS0_TEMP_BLOCK_BASE + 0)
#define	PDS_DS0_ADDRESS_PART		(PDS_DS0_TEMP_BLOCK_BASE + 1)

#define	PDS_DS1_INDEX_PART			(PDS_DS1_TEMP_BLOCK_BASE + 0)
#define	PDS_DS1_DMA_ADDRESS			(PDS_DS1_TEMP_BLOCK_BASE + 2)
#define	PDS_DS1_DMA_REGISTER		(PDS_DS1_TEMP_BLOCK_BASE + 3)

#define PDS_DS0_TESS_SYNC			(PDS_DS0_TEMP_BLOCK_BASE + 4)
#define PDS_DS0_TESS_COUNTER		(PDS_DS0_TEMP_BLOCK_BASE + 5)

#define PDS_DS1_TEMPEDMDATA			(PDS_DS1_TEMP_BLOCK_BASE + 1)

#define PDS_DS0_LOOPVERTNUM			(PDS_DS0_TEMP_BLOCK_BASE + 0)

#define PDS_DS0_VERTEX_TEMP_BLOCK_BASE 52
#define PDS_DS1_VERTEX_TEMP_BLOCK_BASE 52

#define PDS_DS0_VERTEX_TEMP_BLOCK_END  60
#define PDS_DS1_VERTEX_TEMP_BLOCK_END  60

#if !defined(SGX545)
#if !defined(SGX543) && !defined(SGX544) && !defined(SGX554)
#define EURASIA_DOUTI_TAG_MASK		((~EURASIA_PDS_DOUTI_TEXISSUE_CLRMSK) |		\
									 EURASIA_PDS_DOUTI_TEXCENTROID |			\
									 (~EURASIA_PDS_DOUTI_TEXWRAP_CLRMSK) |		\
									 (~EURASIA_PDS_DOUTI_TEXPROJ_CLRMSK) |		\
									 EURASIA_PDS_DOUTI_TEXLASTISSUE)
#else
#define EURASIA_DOUTI_TAG_MASK		((~EURASIA_PDS_DOUTI_TEXISSUE_CLRMSK) |		\
									 EURASIA_PDS_DOUTI_TEXCENTROID |			\
									 (~EURASIA_PDS_DOUTI_TEXWRAP_CLRMSK) |		\
									 (~EURASIA_PDS_DOUTI_TEXPROJ_CLRMSK) |		\
									 EURASIA_PDS_DOUTI_TEX_POINTSPRITE_FORCED | \
									 EURASIA_PDS_DOUTI_TEXLASTISSUE)
#endif
#else
#define EURASIA_DOUTI0_TAG_MASK		((~EURASIA_PDS_DOUTI_TEXISSUE_CLRMSK) |		\
									 EURASIA_PDS_DOUTI_TEXCENTROID |			\
									 (~EURASIA_PDS_DOUTI_TEXWRAP_CLRMSK) |		\
									 (~EURASIA_PDS_DOUTI_TEXPROJ_CLRMSK) |		\
									 EURASIA_PDS_DOUTI_TEXLASTISSUE)
									 
#define EURASIA_DOUTI1_TAG_MASK		((~EURASIA_PDS_DOUTI1_USESAMPLE_RATE_CLRMSK) |	\
									(~EURASIA_PDS_DOUTI1_TEXSAMPLE_RATE_CLRMSK) |	\
									EURASIA_PDS_DOUTI1_UPOINTSPRITE_FORCE | 		\
									EURASIA_PDS_DOUTI1_TPOINTSPRITE_FORCE | 		\
									EURASIA_PDS_DOUTI1_UDONT_ITERATE)
#endif

/*****************************************************************************
 Structure forward declarations
*****************************************************************************/

/*****************************************************************************
 Structure definitions
*****************************************************************************/

/*****************************************************************************
 Static function prototypes
*****************************************************************************/

static IMG_UINT32 PDSGetNextConstant	  (IMG_UINT32 * pui32NextDS0Constant, IMG_UINT32 * pui32NextDS1Constant);
static IMG_UINT32 PDSGetConstants		  (IMG_UINT32 * pui32NextConstant, IMG_UINT32 ui32NumConstants);
static IMG_UINT32 PDSGetDS0ConstantOffset (IMG_UINT32   ui32Constant);
static IMG_UINT32 PDSGetDS1ConstantOffset (IMG_UINT32   ui32Constant);
static IMG_VOID   PDSSetDS0Constant		  (IMG_UINT32 * pui32Constants, IMG_UINT32 ui32Constant, IMG_UINT32 ui32Value);
static IMG_VOID   PDSSetDS1Constant		  (IMG_UINT32 * pui32Constants, IMG_UINT32 ui32Constant, IMG_UINT32 ui32Value);
static IMG_UINT32 PDSGetNumConstants	  (IMG_UINT32 ui32NextDS0Constant, IMG_UINT32 ui32NextDS1Constant);
static IMG_UINT32 PDSGetTemps			  (IMG_UINT32 * pui32NextTemp, IMG_UINT32 ui32NumTemps);

/*****************************************************************************
 Static variables
*****************************************************************************/

/*****************************************************************************
 Function definitions
*****************************************************************************/

#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
#define PDSSetTaskControl(X, Y) \
		PDS##X##Set##Y##0(pui32Buffer, psProgram->aui32##Y##USETaskControl[0]); \
		PDS##X##Set##Y##1(pui32Buffer, psProgram->aui32##Y##USETaskControl[1]); \
		PDS##X##Set##Y##2(pui32Buffer, psProgram->aui32##Y##USETaskControl[2]);
#else
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
#define PDSSetTaskControl(X, Y) \
		PDS##X##Set##Y##0(pui32Buffer, psProgram->aui32##Y##USETaskControl[0]); \
		PDS##X##Set##Y##1(pui32Buffer, psProgram->aui32##Y##USETaskControl[1]);
#else /* defined(SGX545) */
#define PDSSetTaskControl(X, Y) \
		PDS##X##Set##Y##0(pui32Buffer, psProgram->aui32##Y##USETaskControl[0]); \
		PDS##X##Set##Y##1(pui32Buffer, psProgram->aui32##Y##USETaskControl[1]); \
		PDS##X##Set##Y##2(pui32Buffer, psProgram->aui32##Y##USETaskControl[2]);
#endif /* defined(SGX545) */
#endif /* SGX_FEATURE_PDS_EXTENDED_SOURCES */

#if !defined(__psp2__)
/*****************************************************************************
 Function Name	: PDSGeneratePixelEventProgram
 Inputs			: psProgram		- pointer to the PDS pixel event program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS pixel event program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGeneratePixelEventProgram	(PPDS_PIXEL_EVENT_PROGRAM	psProgram,
														 IMG_UINT32 *				pui32Buffer,
														 IMG_BOOL					bMultisampleWithoutDownscale,
														 IMG_BOOL					bDownscaleWithoutMultisample,
														 IMG_UINT32					ui32MultiSampleQuality)
{
	IMG_UINT32 *pui32Constants;
	
	PDS_DPF("PDSGeneratePixelEventProgram()");
	
	PDS_ASSERT((((IMG_UINTPTR_T)pui32Buffer) & ((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1)) == 0);
	PDS_ASSERT(!(bMultisampleWithoutDownscale && bDownscaleWithoutMultisample));
	
	#if (defined(FIX_HW_BRN_31175) || !defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)) && !defined(PDS_BUILD_OPENGLES) && !defined(PDS_BUILD_SERVICES)
	PDS_ASSERT(sizeof(g_pui32PDSPixelEventMSAANoDownscale_4X) <= ((IMG_UINT32)(psProgram-psProgram) + PDS_PIXEVENT_PROG_SIZE));
	PDS_ASSERT(sizeof(g_pui32PDSPixelEventDownscaleNoMSAA_4X) <= ((IMG_UINT32)(psProgram-psProgram) + PDS_PIXEVENT_PROG_SIZE));
	#if defined(SGX_FEATURE_MULTISAMPLE_2X)
	PDS_ASSERT(sizeof(g_pui32PDSPixelEventMSAANoDownscale_2X) <= ((IMG_UINT32)(psProgram-psProgram) + PDS_PIXEVENT_PROG_SIZE));
	PDS_ASSERT(sizeof(g_pui32PDSPixelEventDownscaleNoMSAA_2X) <= ((IMG_UINT32)(psProgram-psProgram) + PDS_PIXEVENT_PROG_SIZE));
	#else
	PVR_UNREFERENCED_PARAMETER(ui32MultiSampleQuality);
	#endif
	#else
	PVR_UNREFERENCED_PARAMETER(ui32MultiSampleQuality);
	PVR_UNREFERENCED_PARAMETER(bMultisampleWithoutDownscale);
	PVR_UNREFERENCED_PARAMETER(bDownscaleWithoutMultisample);
	#endif
	PDS_ASSERT(PDS_PIXEVENT_PROG_SIZE >= ((IMG_UINT32)(psProgram-psProgram) + sizeof(g_pui32PDSPixelEvent)));
	
	#if (defined(FIX_HW_BRN_31175) || !defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)) && !defined(PDS_BUILD_OPENGLES) && !defined(PDS_BUILD_SERVICES)
	if (bMultisampleWithoutDownscale)
	{
		#if defined(SGX_FEATURE_MULTISAMPLE_2X)
		if(ui32MultiSampleQuality == 2)
		{
			memcpy(pui32Buffer, g_pui32PDSPixelEventMSAANoDownscale_2X, PDS_PIXEVENT_PROG_SIZE);
			psProgram->ui32DataSize	= PDS_PIXELEVENTMSAANODOWNSCALE_2X_DATA_SEGMENT_SIZE;
			
			/*
				Copy the task control words for the various USE programs to the PDS program's
				data segment.
			*/
			#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
			PDSPixelEventMSAANoDownscale_2XSetEOT0(pui32Buffer, psProgram->aui32EOTUSETaskControl[0]);
			PDSPixelEventMSAANoDownscale_2XSetEOT1(pui32Buffer, psProgram->aui32EOTUSETaskControl[1]);
			#else
			PDSSetTaskControl(PixelEventMSAANoDownscale_2X, EOT);
			#endif
			PDSSetTaskControl(PixelEventMSAANoDownscale_2X, PTOFF);
			PDSSetTaskControl(PixelEventMSAANoDownscale_2X, EOR);
		}
		else
		#endif
		{
			memcpy(pui32Buffer, g_pui32PDSPixelEventMSAANoDownscale_4X, PDS_PIXEVENT_PROG_SIZE);
			psProgram->ui32DataSize	= PDS_PIXELEVENTMSAANODOWNSCALE_4X_DATA_SEGMENT_SIZE;
			
			/*
				Copy the task control words for the various USE programs to the PDS program's
				data segment.
			*/
			#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
			PDSPixelEventMSAANoDownscale_4XSetEOT0(pui32Buffer, psProgram->aui32EOTUSETaskControl[0]);
			PDSPixelEventMSAANoDownscale_4XSetEOT1(pui32Buffer, psProgram->aui32EOTUSETaskControl[1]);
			#else
			PDSSetTaskControl(PixelEventMSAANoDownscale_4X, EOT);
			#endif
			PDSSetTaskControl(PixelEventMSAANoDownscale_4X, PTOFF);
			PDSSetTaskControl(PixelEventMSAANoDownscale_4X, EOR);
		}
		#if defined(SGX_FEATURE_WRITEBACK_DCU)
		PDSPixelEventSetEOT_DOUTA1(pui32Buffer, 0);
		#endif
	}
	else if (bDownscaleWithoutMultisample)
	{
		#if defined(SGX_FEATURE_MULTISAMPLE_2X)
		if(ui32MultiSampleQuality == 2)
		{
			memcpy(pui32Buffer, g_pui32PDSPixelEventDownscaleNoMSAA_2X, PDS_PIXEVENT_PROG_SIZE);
			psProgram->ui32DataSize	= PDS_PIXELEVENTDOWNSCALENOMSAA_2X_DATA_SEGMENT_SIZE;
			
			/*
				Copy the task control words for the various USE programs to the PDS program's
				data segment.
			*/
			#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
			PDSPixelEventDownscaleNoMSAA_2XSetEOT0(pui32Buffer, psProgram->aui32EOTUSETaskControl[0]);
			PDSPixelEventDownscaleNoMSAA_2XSetEOT1(pui32Buffer, psProgram->aui32EOTUSETaskControl[1]);
			#else
			PDSSetTaskControl(PixelEventDownscaleNoMSAA_2X, EOT);
			#endif
			PDSSetTaskControl(PixelEventDownscaleNoMSAA_2X, PTOFF);
			PDSSetTaskControl(PixelEventDownscaleNoMSAA_2X, EOR);
		}
		else
		#endif
		{
			memcpy(pui32Buffer, g_pui32PDSPixelEventDownscaleNoMSAA_4X, PDS_PIXEVENT_PROG_SIZE);
			psProgram->ui32DataSize	= PDS_PIXELEVENTDOWNSCALENOMSAA_4X_DATA_SEGMENT_SIZE;
			
			/*
				Copy the task control words for the various USE programs to the PDS program's
				data segment.
			*/
			#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
			PDSPixelEventDownscaleNoMSAA_4XSetEOT0(pui32Buffer, psProgram->aui32EOTUSETaskControl[0]);
			PDSPixelEventDownscaleNoMSAA_4XSetEOT1(pui32Buffer, psProgram->aui32EOTUSETaskControl[1]);
			#else
			PDSSetTaskControl(PixelEventDownscaleNoMSAA_4X, EOT);
			#endif
			PDSSetTaskControl(PixelEventDownscaleNoMSAA_4X, PTOFF);
			PDSSetTaskControl(PixelEventDownscaleNoMSAA_4X, EOR);
		}
		
		#if defined(SGX_FEATURE_WRITEBACK_DCU)
		PDSPixelEventSetEOT_DOUTA1(pui32Buffer, 0);
		#endif
	}
	else
	#endif /* !defined(SGX_FEATURE_PDS_EXTENDED_SOURCES) */
	{
		memcpy(pui32Buffer, g_pui32PDSPixelEvent, PDS_PIXEVENT_PROG_SIZE);
		psProgram->ui32DataSize	= PDS_PIXELEVENT_DATA_SEGMENT_SIZE;
		
		/*
			Copy the task control words for the various USE programs to the PDS program's
			data segment.
		*/
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
		PDSPixelEventSetEOT0(pui32Buffer, psProgram->aui32EOTUSETaskControl[0]);
		PDSPixelEventSetEOT1(pui32Buffer, psProgram->aui32EOTUSETaskControl[1]);
#else
		PDSSetTaskControl(PixelEvent, EOT);
#endif
		PDSSetTaskControl(PixelEvent, PTOFF);
		PDSSetTaskControl(PixelEvent, EOR);
		#if defined(SGX_FEATURE_WRITEBACK_DCU)
		PDSPixelEventSetEOT_DOUTA1(pui32Buffer, 0);
		#endif
	}
	
	/*
		Generate the PDS pixel event data
	*/
	pui32Constants				= (IMG_UINT32 *) (((IMG_UINTPTR_T)pui32Buffer + ((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1));
	psProgram->pui32DataSegment	= pui32Constants;
	
	return pui32Buffer + (PDS_PIXEVENT_PROG_SIZE >> 2);
}
#endif

/*****************************************************************************
 Function Name	: PDSGeneratePixelShaderSAProgram
 Inputs			: psProgram		- pointer to the PDS pixel shader secondary attributes program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS pixel shader seconary attributes program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGeneratePixelShaderSAProgram	(PPDS_PIXEL_SHADER_SA_PROGRAM	psProgram,
															 IMG_UINT32 *					pui32Buffer)
{
	IMG_UINT32 *	pui32Constants;
	IMG_UINT32	ui32NextDS0Constant;
	IMG_UINT32	ui32NextDS1Constant;
	IMG_UINT32	ui32DS0Constant;
	IMG_UINT32	ui32DS1Constant;
	IMG_UINT32	ui32NumConstants;
	IMG_UINT32 *	pui32Instructions;
	IMG_UINT32 *	pui32Instruction;
#if defined(SGX_FEATURE_ALPHATEST_SECONDARY)
	IMG_UINT32 ui32IterateZWord;
#endif


	PDS_DPF("PDSGeneratePixelShaderSAProgram()");
	
	PDS_ASSERT((((IMG_UINTPTR_T)pui32Buffer) & ((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1)) == 0);
	PDS_ASSERT(psProgram->ui32NumDMAKicks <= 3);
	
	/*
		Generate the PDS pixel shader secondary attributes data
	*/
	pui32Constants		= pui32Buffer;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	/*
		Copy the DMA control words to constants
	*/
	if (psProgram->ui32NumDMAKicks >= 1)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32DMAControl[0]);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32DMAControl[1]);
		
		if (psProgram->ui32NumDMAKicks >= 2)
		{
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32DMAControl[2]);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32DMAControl[3]);
			
			if (psProgram->ui32NumDMAKicks == 3)
			{
				ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 2);
				PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32DMAControl[4]);
				PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 1, psProgram->aui32DMAControl[5]);
			}
			
		}
	}
	
	if (psProgram->bWriteTilePosition)
	{
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant, (psProgram->uTilePositionAttrDest << EURASIA_PDS_DOUTA1_AO_SHIFT));
	}
	
#if defined(FIX_HW_BRN_22249)
	if(psProgram->bGenerateTileAddress)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->ui32RenderBaseAddress);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, (0 << EURASIA_PDS_DOUTA1_AO_SHIFT));
		
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, ~EURASIA_PDS_IR0_PDM_TILEY_CLRMSK);
	}
#endif /* defined(FIX_HW_BRN_22249) */

	if (psProgram->bKickUSE || psProgram->bKickUSEDummyProgram)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
		
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32USETaskControl[2]);
	}
	
#if defined(SGX_FEATURE_ALPHATEST_SECONDARY)
	if(psProgram->bIterateZAbs)
	{
		ui32IterateZWord = EURASIA_PDS_DOUTI_TEXISSUE_NONE  |
							EURASIA_PDS_DOUTI_USEISSUE_ZABS  |
							EURASIA_PDS_DOUTI_USELASTISSUE;
							
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, ui32IterateZWord);
	}
#endif

	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant, ui32NextDS1Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_PDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_PDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS pixel shader secondary attributes code
	*/
	pui32Instructions		= pui32Constants + ui32NumConstants;
	pui32Instruction		= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	/*
		DMA the state into the secondary attributes
	*/
	if (psProgram->ui32NumDMAKicks >= 1)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTD,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H);
												 
		if (psProgram->ui32NumDMAKicks >= 2)
		{
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
			
			*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MOVS_DEST_DOUTD,
													 EURASIA_PDS_MOVS_SRC1SEL_DS0,
													 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SRC2SEL_DS1,
													 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 EURASIA_PDS_MOVS_SWIZ_SRC1H,
													 EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 EURASIA_PDS_MOVS_SWIZ_SRC1H);
													 
			if (psProgram->ui32NumDMAKicks == 3)
			{
				ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 2);
				
				*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
														EURASIA_PDS_MOVS_DEST_DOUTD,
														EURASIA_PDS_MOVS_SRC1SEL_DS0,
														EURASIA_PDS_MOVS_SRC1_DS0_BASE + PDS_DS0_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
														EURASIA_PDS_MOVS_SRC2SEL_DS1,
														EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
														EURASIA_PDS_MOVS_SWIZ_SRC2L,
														EURASIA_PDS_MOVS_SWIZ_SRC2H,
														EURASIA_PDS_MOVS_SWIZ_SRC2L,
														EURASIA_PDS_MOVS_SWIZ_SRC2H);
			}
			
		}
	}
	
	if (psProgram->bWriteTilePosition)
	{
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTA,
												 EURASIA_PDS_MOVS_SRC1SEL_REG,
												 EURASIA_PDS_MOVS_SRC1_IR0,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 (ui32DS1Constant % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 (ui32DS1Constant % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
	}
	
#if defined(FIX_HW_BRN_22249)
	if(psProgram->bGenerateTileAddress)
	{
		/* EURASIA_PDS_IR0_PDM_TILEY_CLRMSK */
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		
		/* and ds1[PDS_DS1_TEMP_BLOCK_BASE], ir0, ds1[EURASIA_PDS_IR0_PDM_TILEY_CLRMSK] */
		*pui32Instruction++	= PDSEncodeAND		(EURASIA_PDS_CC_ALWAYS,
												EURASIA_PDS_LOGIC_DESTSEL_DS1,
												PDS_DS1_TEMP_BLOCK_BASE,
												EURASIA_PDS_LOGIC_SRC1SEL_REG,
												EURASIA_PDS_LOGIC_SRC1_IR0,
												EURASIA_PDS_LOGIC_SRC2SEL_DS1,
												ui32DS1Constant);
												
		/* shl ds1[PDS_DS1_TEMP_BLOCK_BASE], ds1[PDS_DS1_TEMP_BLOCK_BASE], log2(1k*8*2bpp) - EURASIA_PDS_IR0_PDM_TILEY_SHIFT */
		*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
													EURASIA_PDS_SHIFT_DESTSEL_DS1,
													EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS1_TEMP_BLOCK_BASE,
													EURASIA_PDS_SHIFT_SRCSEL_DS1,
													EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS1_TEMP_BLOCK_BASE,
													14 - EURASIA_PDS_IR0_PDM_TILEY_SHIFT);
													
		/* BASE_ADDRESS */
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
		
		/*
			add ds0[PDS_DS1_TEMP_BLOCK_BASE], ds0[BASE_ADDRESS], ds1[PDS_DS1_TEMP_BLOCK_BASE]
		*/
		*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
													EURASIA_PDS_ARITH_DESTSEL_DS1,
													EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS1_TEMP_BLOCK_BASE,
													EURASIA_PDS_ARITH_SRC1SEL_DS0,
													EURASIA_PDS_ARITH_SRC1_DS0_BASE + ui32DS0Constant,
													EURASIA_PDS_ARITH_SRC2SEL_DS1,
													EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE);
		/* DOUTA control */
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
		
		/*
			Copy the state into the secondary attributes
		*/
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												EURASIA_PDS_MOVS_DEST_DOUTA,
												EURASIA_PDS_MOVS_SRC1SEL_DS0,
												EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
												EURASIA_PDS_MOVS_SRC2SEL_DS1,
												EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
												EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 (ui32DS0Constant % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 (ui32DS0Constant % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1L : EURASIA_PDS_MOVS_SWIZ_SRC1H,
												EURASIA_PDS_MOVS_SWIZ_SRC2H);
												
	}
#endif /* defined(FIX_HW_BRN_22249) */

	if (psProgram->bKickUSE || psProgram->bKickUSEDummyProgram)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTU,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H,
												 (ui32DS1Constant % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 (ui32DS1Constant % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
	}
	
#if defined(SGX_FEATURE_ALPHATEST_SECONDARY)
	if(psProgram->bIterateZAbs)
	{
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		
		*pui32Instruction++ = PDSEncodeMOVS	(EURASIA_PDS_CC_ALWAYS,
											EURASIA_PDS_MOVS_DEST_DOUTI,
											EURASIA_PDS_MOVS_SRC1SEL_DS0,
											EURASIA_PDS_MOVS_SRC1_DS0_BASE + PDS_DS0_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
											EURASIA_PDS_MOVS_SRC2SEL_DS1,
											EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((ui32DS1Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											EURASIA_PDS_MOVS_SWIZ_SRC1L,
											EURASIA_PDS_MOVS_SWIZ_SRC1H);
	}
#endif

	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}


/*****************************************************************************
 Function Name	: PDSGenerateStaticPixelShaderSAProgram
 Inputs			: psProgram		- pointer to the PDS pixel shader secondary attributes program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS pixel shader seconary attributes program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateStaticPixelShaderSAProgram	(PPDS_PIXEL_SHADER_STATIC_SA_PROGRAM psProgram,
																 IMG_UINT32 *pui32Buffer)
{
	IMG_UINT32 *pui32Instructions;
	IMG_UINT32 *pui32Instruction;
	IMG_UINT32 *pui32Constants;
	IMG_UINT32	ui32NextDS0Constant;
	IMG_UINT32	ui32NextDS1Constant;
	IMG_UINT32	ui32DS0Constant;
	IMG_UINT32	ui32DS1Constant;
	IMG_UINT32	ui32NumConstants;
	IMG_UINT32 i;
	
	PDS_DPF("PDSGenerateStaticPixelShaderSAProgram()");
	
	PDS_ASSERT(psProgram->ui32DAWCount > 0);
	
	#if defined(FIX_HW_BRN_31988)
	// avoid compiler warning..
	ui32DS1Constant = ui32DS0Constant = 0;
	#endif
	
	/*
		Generate the PDS pixel shader secondary attributes data
	*/
	pui32Constants		= pui32Buffer;
	
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	if (psProgram->bKickUSEDummyProgram)
	{
		ui32DS0Constant	= PDSGetConstants(&ui32NextDS0Constant, 2);
		
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
		
		ui32DS1Constant	= PDSGetConstants(&ui32NextDS1Constant, 1);
		
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32USETaskControl[2]);
	}
	
#if defined(SGX_FEATURE_ALPHATEST_SECONDARY)
	if(psProgram->bIterateZAbs)
	{
		IMG_UINT32 ui32IterateZWord;
		
		ui32IterateZWord = 	EURASIA_PDS_DOUTI_TEXISSUE_NONE  |
							EURASIA_PDS_DOUTI_USEISSUE_ZABS  |
							EURASIA_PDS_DOUTI_USELASTISSUE;
							
		ui32DS1Constant	= PDSGetConstants(&ui32NextDS1Constant, 1);
		
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, ui32IterateZWord);
	}
#endif

	for(i=0; i<psProgram->ui32DAWCount; i++)
	{
		if(i%2==0)
		{
			ui32DS0Constant	= PDSGetConstants(&ui32NextDS0Constant, 2);
			
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->pui32DAWData[i]);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, ((psProgram->ui32DAWOffset + i) << EURASIA_PDS_DOUTA1_AO_SHIFT));
		}
		else
		{
			ui32DS1Constant	= PDSGetConstants(&ui32NextDS1Constant, 2);
			
			PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->pui32DAWData[i]);
			PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 1, ((psProgram->ui32DAWOffset + i) << EURASIA_PDS_DOUTA1_AO_SHIFT));
		}
	}
	
	/*
		Get the number of constants
	*/
	ui32NumConstants = PDSGetNumConstants(ui32NextDS0Constant, ui32NextDS1Constant);
	ui32NumConstants = ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_PDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_PDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS pixel shader secondary attributes code
	*/
	pui32Instructions	= pui32Constants + ui32NumConstants;
	pui32Instruction	= pui32Instructions;
	
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	if (psProgram->bKickUSEDummyProgram)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		
		*pui32Instruction++	= PDSEncodeMOVS	(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 EURASIA_PDS_MOVS_SWIZ_SRC1H,
											 (ui32DS1Constant % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 (ui32DS1Constant % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
	}
	
#if defined(SGX_FEATURE_ALPHATEST_SECONDARY)
	if(psProgram->bIterateZAbs)
	{
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		
		*pui32Instruction++ = PDSEncodeMOVS	(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTI,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + PDS_DS0_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((ui32DS1Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 EURASIA_PDS_MOVS_SWIZ_SRC1H);
	}
#endif


	for(i=0; i<psProgram->ui32DAWCount; i++)
	{
		if(i%2==0)
		{
			ui32DS0Constant	= PDSGetConstants(&ui32NextDS0Constant, 2);
			
			*pui32Instruction++	= PDSEncodeMOVS	(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTA,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H,
												 EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 EURASIA_PDS_MOVS_SWIZ_SRC2L);
		}
		else
		{
			ui32DS1Constant	= PDSGetConstants(&ui32NextDS1Constant, 2);
			
			*pui32Instruction++	= PDSEncodeMOVS	(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTA,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + PDS_DS0_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 EURASIA_PDS_MOVS_SWIZ_SRC2H,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L);
		}
	}
	
#if defined(FIX_HW_BRN_31988)
	if(psProgram->ui32DAWCount > 0)
	{
		/* Repest the last DOUTA twice */
		if((i-1)%2==0)
		{
			*pui32Instruction++	= PDSEncodeMOVS	(EURASIA_PDS_CC_ALWAYS,
													EURASIA_PDS_MOVS_DEST_DOUTA,
													EURASIA_PDS_MOVS_SRC1SEL_DS0,
													EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
													EURASIA_PDS_MOVS_SRC2SEL_DS1,
													EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
													EURASIA_PDS_MOVS_SWIZ_SRC1L,
													EURASIA_PDS_MOVS_SWIZ_SRC1H,
													EURASIA_PDS_MOVS_SWIZ_SRC2L,
													EURASIA_PDS_MOVS_SWIZ_SRC2L);

			*pui32Instruction++	= PDSEncodeMOVS	(EURASIA_PDS_CC_ALWAYS,
													EURASIA_PDS_MOVS_DEST_DOUTA,
													EURASIA_PDS_MOVS_SRC1SEL_DS0,
													EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
													EURASIA_PDS_MOVS_SRC2SEL_DS1,
													EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
													EURASIA_PDS_MOVS_SWIZ_SRC1L,
													EURASIA_PDS_MOVS_SWIZ_SRC1H,
													EURASIA_PDS_MOVS_SWIZ_SRC2L,
													EURASIA_PDS_MOVS_SWIZ_SRC2L);
		}
		else
		{
				*pui32Instruction++	= PDSEncodeMOVS	(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MOVS_DEST_DOUTA,
													 EURASIA_PDS_MOVS_SRC1SEL_DS0,
													 EURASIA_PDS_MOVS_SRC1_DS0_BASE + PDS_DS0_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SRC2SEL_DS1,
													 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SWIZ_SRC2L,
													 EURASIA_PDS_MOVS_SWIZ_SRC2H,
													 EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 EURASIA_PDS_MOVS_SWIZ_SRC1L);

				*pui32Instruction++	= PDSEncodeMOVS	(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MOVS_DEST_DOUTA,
													 EURASIA_PDS_MOVS_SRC1SEL_DS0,
													 EURASIA_PDS_MOVS_SRC1_DS0_BASE + PDS_DS0_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SRC2SEL_DS1,
													 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SWIZ_SRC2L,
													 EURASIA_PDS_MOVS_SWIZ_SRC2H,
													 EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 EURASIA_PDS_MOVS_SWIZ_SRC1L);
		}
	}
#endif


	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}


/*****************************************************************************
 Function Name	: PDSEncodeSimpleMOVS
 Inputs			: ui32Dest		- Destination for the movs.
				  ui32DSConst		- Start of the arguments to move to the interface
 Outputs		: none
 Returns		: The encoded instruction
 Description	: Generates a MOVS instruction with a contiguous set of arguments.
*****************************************************************************/
static IMG_UINT32 PDSEncodeSimpleMOVS(IMG_UINT32 ui32Dest, IMG_UINT32 ui32DSConst, IMG_UINT32 ui32ExtraInfo0, IMG_UINT32 ui32ExtraInfo1)
{
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
	if(ui32Dest == EURASIA_PDS_MOVS_DEST_DOUTI)
	{
		if (ui32DSConst & 2)
		{
			if (ui32DSConst & 1)
			{
				return PDSEncodeDOUTI	(EURASIA_PDS_CC_ALWAYS,
										EURASIA_PDS_MOVS_SRC1SEL_DS0,
										EURASIA_PDS_MOVS_SRC1_DS0_BASE + (ui32DSConst >> 2) + 1,
										EURASIA_PDS_MOVS_SRC2SEL_DS1,
										EURASIA_PDS_MOVS_SRC2_DS1_BASE + (ui32DSConst >> 2),
										EURASIA_PDS_MOVS_SWIZ_SRC2H,
										EURASIA_PDS_MOVS_SWIZ_SRC1L,
										ui32ExtraInfo0,
										ui32ExtraInfo1);
			}
			else
			{
				return PDSEncodeDOUTI	(EURASIA_PDS_CC_ALWAYS,
										EURASIA_PDS_MOVS_SRC1SEL_DS0,
										EURASIA_PDS_MOVS_SRC1_DS0_BASE + (ui32DSConst >> 2) + 1,
										EURASIA_PDS_MOVS_SRC2SEL_DS1,
										EURASIA_PDS_MOVS_SRC2_DS1_BASE + (ui32DSConst >> 2),
										EURASIA_PDS_MOVS_SWIZ_SRC2L,
										EURASIA_PDS_MOVS_SWIZ_SRC2H,
										ui32ExtraInfo0,
										ui32ExtraInfo1);
			}
		}
		else
		{
			if (ui32DSConst & 1)
			{
				return PDSEncodeDOUTI	(EURASIA_PDS_CC_ALWAYS,
										EURASIA_PDS_MOVS_SRC1SEL_DS0,
										EURASIA_PDS_MOVS_SRC1_DS0_BASE + (ui32DSConst >> 2),
										EURASIA_PDS_MOVS_SRC2SEL_DS1,
										EURASIA_PDS_MOVS_SRC2_DS1_BASE + (ui32DSConst >> 2),
										EURASIA_PDS_MOVS_SWIZ_SRC1H,
										EURASIA_PDS_MOVS_SWIZ_SRC2L,
										ui32ExtraInfo0,
										ui32ExtraInfo1);
			}
			else
			{
				return PDSEncodeDOUTI	(EURASIA_PDS_CC_ALWAYS,
										EURASIA_PDS_MOVS_SRC1SEL_DS0,
										EURASIA_PDS_MOVS_SRC1_DS0_BASE + (ui32DSConst >> 2),
										EURASIA_PDS_MOVS_SRC2SEL_DS1,
										EURASIA_PDS_MOVS_SRC2_DS1_BASE + (ui32DSConst >> 2),
										EURASIA_PDS_MOVS_SWIZ_SRC1L,
										EURASIA_PDS_MOVS_SWIZ_SRC1H,
										ui32ExtraInfo0,
										ui32ExtraInfo1);
			}
		}
	}
	else if(ui32Dest == EURASIA_PDS_MOVS_DEST_DOUTT)
	{
		if (ui32DSConst & 2)
		{
			return PDSEncodeDOUTT	(EURASIA_PDS_CC_ALWAYS,
									EURASIA_PDS_MOVS_SRC1SEL_DS0,
									EURASIA_PDS_MOVS_SRC1_DS0_BASE + (ui32DSConst >> 2) + 1,
									EURASIA_PDS_MOVS_SRC2SEL_DS1,
									EURASIA_PDS_MOVS_SRC2_DS1_BASE + (ui32DSConst >> 2),
									IMG_TRUE,
									ui32ExtraInfo0,
									ui32ExtraInfo1);
		}
		else
		{
			return PDSEncodeDOUTT	(EURASIA_PDS_CC_ALWAYS,
									EURASIA_PDS_MOVS_SRC1SEL_DS0,
									EURASIA_PDS_MOVS_SRC1_DS0_BASE + (ui32DSConst >> 2),
									EURASIA_PDS_MOVS_SRC2SEL_DS1,
									EURASIA_PDS_MOVS_SRC2_DS1_BASE + (ui32DSConst >> 2),
									IMG_FALSE,
									ui32ExtraInfo0,
									ui32ExtraInfo1);
		}
	}
	else
#endif
	{
		PVR_UNREFERENCED_PARAMETER(ui32ExtraInfo0);
		PVR_UNREFERENCED_PARAMETER(ui32ExtraInfo1);
		
		if (ui32DSConst & 2)
		{
			if (ui32DSConst & 1)
			{
				return PDSEncodeMOVS	(EURASIA_PDS_CC_ALWAYS,
										ui32Dest,
										EURASIA_PDS_MOVS_SRC1SEL_DS0,
										EURASIA_PDS_MOVS_SRC1_DS0_BASE + (ui32DSConst >> 2) + 1,
										EURASIA_PDS_MOVS_SRC2SEL_DS1,
										EURASIA_PDS_MOVS_SRC2_DS1_BASE + (ui32DSConst >> 2),
										EURASIA_PDS_MOVS_SWIZ_SRC2H,
										EURASIA_PDS_MOVS_SWIZ_SRC1L,
										EURASIA_PDS_MOVS_SWIZ_SRC1H,
										EURASIA_PDS_MOVS_SWIZ_SRC1H);
			}
			else
			{
				return PDSEncodeMOVS	(EURASIA_PDS_CC_ALWAYS,
										ui32Dest,
										EURASIA_PDS_MOVS_SRC1SEL_DS0,
										EURASIA_PDS_MOVS_SRC1_DS0_BASE + (ui32DSConst >> 2) + 1,
										EURASIA_PDS_MOVS_SRC2SEL_DS1,
										EURASIA_PDS_MOVS_SRC2_DS1_BASE + (ui32DSConst >> 2),
										EURASIA_PDS_MOVS_SWIZ_SRC2L,
										EURASIA_PDS_MOVS_SWIZ_SRC2H,
										EURASIA_PDS_MOVS_SWIZ_SRC1L,
										EURASIA_PDS_MOVS_SWIZ_SRC1H);
			}
		}
		else
		{
			if (ui32DSConst & 1)
			{
				return PDSEncodeMOVS	(EURASIA_PDS_CC_ALWAYS,
										ui32Dest,
										EURASIA_PDS_MOVS_SRC1SEL_DS0,
										EURASIA_PDS_MOVS_SRC1_DS0_BASE + (ui32DSConst >> 2),
										EURASIA_PDS_MOVS_SRC2SEL_DS1,
										EURASIA_PDS_MOVS_SRC2_DS1_BASE + (ui32DSConst >> 2),
										EURASIA_PDS_MOVS_SWIZ_SRC1H,
										EURASIA_PDS_MOVS_SWIZ_SRC2L,
										EURASIA_PDS_MOVS_SWIZ_SRC2H,
										EURASIA_PDS_MOVS_SWIZ_SRC2H);
			}
			else
			{
				return PDSEncodeMOVS	(EURASIA_PDS_CC_ALWAYS,
										ui32Dest,
										EURASIA_PDS_MOVS_SRC1SEL_DS0,
										EURASIA_PDS_MOVS_SRC1_DS0_BASE + (ui32DSConst >> 2),
										EURASIA_PDS_MOVS_SRC2SEL_DS1,
										EURASIA_PDS_MOVS_SRC2_DS1_BASE + (ui32DSConst >> 2),
										EURASIA_PDS_MOVS_SWIZ_SRC1L,
										EURASIA_PDS_MOVS_SWIZ_SRC1H,
										EURASIA_PDS_MOVS_SWIZ_SRC2L,
										EURASIA_PDS_MOVS_SWIZ_SRC2H);
			}
		}
	}
}

/*****************************************************************************
 Function Name	: PDSGeneratePixelShaderProgram
 Inputs			: psProgram		- pointer to the PDS pixel shader program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS pixel shader program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32* PDSGeneratePixelShaderProgram	(PPDS_TEXTURE_IMAGE_UNIT      psTextureImageUnits,
														 PPDS_PIXEL_SHADER_PROGRAM	  psProgram,
														 IMG_UINT32                  *pui32Buffer)
{
	IMG_UINT32 *pui32Constants;
	IMG_UINT32	ui32NextDS0Constant;
	IMG_UINT32	ui32NextDS1Constant;
	IMG_UINT32	ui32Constant;
	IMG_UINT32	ui32Iterator;
	IMG_UINT32	ui32NumConstants;
	IMG_UINT32 *pui32Instructions;
	IMG_UINT32 *pui32Instruction;
	IMG_UINT32	ui32NextDSConstant;
	#if defined(SGX545)
	IMG_UINT32	ui32Constant0;
	IMG_UINT32	ui32Constant1;
	IMG_UINT32 *pui32LastIterationIssue0;
	IMG_UINT32 *pui32LastIterationIssue1;
	#else
	IMG_UINT32 *pui32LastIterationIssue;
	#endif
	IMG_BOOL	abCombinedWithPreviousIssue[PVRD3D_MAXIMUM_ITERATIONS];
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
	IMG_UINT8	aui8TagSize[PVRD3D_MAXIMUM_ITERATIONS];
#endif
	PDS_DPF("PDSGeneratePixelShaderProgram()");
	
	PDS_ASSERT((((IMG_UINTPTR_T)pui32Buffer) & ((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1)) == 0);
	
	/*
		Generate the PDS pixel shader data
	*/
	pui32Constants		= (IMG_UINT32 *) (((IMG_UINTPTR_T)pui32Buffer + ((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1)) & ~((IMG_UINTPTR_T)(1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= 0;
	ui32NextDS1Constant	= 0;
	
	/*
		Copy the USE task control words to constants
	*/
	
	ui32Constant = PDSGetNextConstant(&ui32NextDS0Constant, &ui32NextDS1Constant);
	pui32Constants[ui32Constant] = psProgram->aui32USETaskControl[0];
	
	ui32Constant = PDSGetNextConstant(&ui32NextDS0Constant, &ui32NextDS1Constant);
	pui32Constants[ui32Constant] = psProgram->aui32USETaskControl[1];
	
	ui32Constant = PDSGetNextConstant(&ui32NextDS0Constant, &ui32NextDS1Constant);
	pui32Constants[ui32Constant] = psProgram->aui32USETaskControl[2];
	
	/*
		Copy the FPU iterator and texture control words to constants
	*/
	#if defined(SGX545)
	pui32LastIterationIssue0 = IMG_NULL;
	pui32LastIterationIssue1 = IMG_NULL;
	#else
	pui32LastIterationIssue = IMG_NULL;
	#endif
	
	for (ui32Iterator = 0; ui32Iterator < psProgram->ui32NumFPUIterators; ui32Iterator++)
	{
		IMG_UINT32 ui32TagIssue	= psProgram->aui32TAGLayers[ui32Iterator];
		
		if (ui32TagIssue != 0xFFFFFFFF)
		{
			PPDS_TEXTURE_IMAGE_UNIT psTextureCtl;
			#if defined(SGX545)
			IMG_UINT32 ui32DOutI0 = psProgram->aui32FPUIterators0[ui32Iterator];
			IMG_UINT32 ui32DOutI1 = psProgram->aui32FPUIterators1[ui32Iterator];
			#else
			IMG_UINT32 ui32DOutI = psProgram->aui32FPUIterators[ui32Iterator];
			#endif
			
			PDS_ASSERT(psTextureImageUnits != IMG_NULL);
 			psTextureCtl = &psTextureImageUnits[ui32TagIssue];

			/*
				Check if we can combine this issue with the previous one.
			*/
			#if defined(SGX545)
			if (pui32LastIterationIssue0 != IMG_NULL && pui32LastIterationIssue1 != IMG_NULL)
			#else
			if (pui32LastIterationIssue != IMG_NULL)
			#endif
			{
				#if defined(SGX545)
				*pui32LastIterationIssue0 &= ~EURASIA_DOUTI0_TAG_MASK;
				*pui32LastIterationIssue0 |= (ui32DOutI0 & EURASIA_DOUTI0_TAG_MASK);
				*pui32LastIterationIssue1 &= ~EURASIA_DOUTI1_TAG_MASK;
				*pui32LastIterationIssue1 |= (ui32DOutI1 & EURASIA_DOUTI1_TAG_MASK);
				#else
				*pui32LastIterationIssue &= ~EURASIA_DOUTI_TAG_MASK;
				*pui32LastIterationIssue |= (ui32DOutI & EURASIA_DOUTI_TAG_MASK);
				#endif
				
				abCombinedWithPreviousIssue[ui32Iterator] = IMG_TRUE;
				
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
				aui8TagSize[ui32Iterator - 1] = psProgram->aui8LayerSize[ui32Iterator];
#endif
			}
			else
			{
				#if defined(SGX545)
				ui32Constant = PDSGetNextConstant(&ui32NextDS0Constant, &ui32NextDS1Constant);
				pui32Constants[ui32Constant] = ui32DOutI0;
				ui32Constant = PDSGetNextConstant(&ui32NextDS0Constant, &ui32NextDS1Constant);
				pui32Constants[ui32Constant] = ui32DOutI1;
				#else
				
				ui32Constant = PDSGetNextConstant(&ui32NextDS0Constant, &ui32NextDS1Constant);
				
				pui32Constants[ui32Constant] = ui32DOutI;
				#endif
				
				abCombinedWithPreviousIssue[ui32Iterator] = IMG_FALSE;
				
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
				aui8TagSize[ui32Iterator] = psProgram->aui8LayerSize[ui32Iterator];
#endif
			}
			#if defined(SGX545)
			pui32LastIterationIssue0 = IMG_NULL;
			pui32LastIterationIssue1 = IMG_NULL;
			#else
			pui32LastIterationIssue = IMG_NULL;
			#endif
			
			ui32Constant = PDSGetNextConstant(&ui32NextDS0Constant, &ui32NextDS1Constant);
			
#if(EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
			/* Make sure to align this 4 dword interface to 2 dwords */
			if(ui32Constant & 0x1)
			{
				ui32Constant = PDSGetNextConstant(&ui32NextDS0Constant, &ui32NextDS1Constant);
			}
#endif

			pui32Constants[ui32Constant] = psTextureCtl->ui32TAGControlWord0;
			
			ui32Constant = PDSGetNextConstant(&ui32NextDS0Constant, &ui32NextDS1Constant);
			pui32Constants[ui32Constant] = psTextureCtl->ui32TAGControlWord1;
			
			ui32Constant = PDSGetNextConstant(&ui32NextDS0Constant, &ui32NextDS1Constant);
			pui32Constants[ui32Constant] = psTextureCtl->ui32TAGControlWord2;
			
#if(EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
			ui32Constant = PDSGetNextConstant(&ui32NextDS0Constant, &ui32NextDS1Constant);
			pui32Constants[ui32Constant] = psTextureCtl->ui32TAGControlWord3;
#endif
		}
		else
		{
			#if defined(SGX545)
			IMG_UINT32 ui32DOutI0 = psProgram->aui32FPUIterators0[ui32Iterator];
			IMG_UINT32 ui32DOutI1 = psProgram->aui32FPUIterators1[ui32Iterator];
			#else
			IMG_UINT32 ui32DOutI = psProgram->aui32FPUIterators[ui32Iterator];
			#endif
			
			#if defined(SGX545)
			ui32Constant0 = PDSGetNextConstant(&ui32NextDS0Constant, &ui32NextDS1Constant);
			pui32Constants[ui32Constant0] = ui32DOutI0;
			ui32Constant1 = PDSGetNextConstant(&ui32NextDS0Constant, &ui32NextDS1Constant);
			pui32Constants[ui32Constant1] = ui32DOutI1;
			
			if ((ui32DOutI0 & ~EURASIA_PDS_DOUTI_TEXISSUE_CLRMSK) == EURASIA_PDS_DOUTI_TEXISSUE_NONE)
			{
				pui32LastIterationIssue0 = pui32Constants + ui32Constant0;
				pui32LastIterationIssue1 = pui32Constants + ui32Constant1;
			}
			#else
			ui32Constant = PDSGetNextConstant(&ui32NextDS0Constant, &ui32NextDS1Constant);
			
			pui32Constants[ui32Constant] = ui32DOutI;
			
			if ((ui32DOutI & ~EURASIA_PDS_DOUTI_TEXISSUE_CLRMSK) == EURASIA_PDS_DOUTI_TEXISSUE_NONE)
			{
				pui32LastIterationIssue = pui32Constants + ui32Constant;
			}
			#endif
			
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
			aui8TagSize[ui32Iterator] = 0;
#endif


		}
	}
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant, ui32NextDS1Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_PDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_PDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS pixel shader code
	*/
	pui32Instructions		= pui32Constants + ui32NumConstants;
	pui32Instruction		= pui32Instructions;
	ui32NextDSConstant	= 0;
	
	/*
		Issue the task to the USE
		
		movs doutu, ds0[constant_use], ds1[constant_use], ds0[constant_use], ds1[constant_use], emit
	*/
	*pui32Instruction++	= PDSEncodeSimpleMOVS(EURASIA_PDS_MOVS_DEST_DOUTU, ui32NextDSConstant, 0, 0);
	ui32NextDSConstant	+= 3;
	
	for (ui32Iterator = 0; ui32Iterator < psProgram->ui32NumFPUIterators; ui32Iterator++)
	{
		IMG_UINT32 ui32TagIssue	= psProgram->aui32TAGLayers[ui32Iterator];
		
		if (ui32TagIssue != 0xFFFFFFFF)
		{
			if (!abCombinedWithPreviousIssue[ui32Iterator])
			{
				/*
					Iterate the components and send them to the tag
				*/
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
				*pui32Instruction++	= PDSEncodeSimpleMOVS(EURASIA_PDS_MOVS_DEST_DOUTI, ui32NextDSConstant, aui8TagSize[ui32Iterator], psProgram->aui8LayerSize[ui32Iterator]);
#else
				*pui32Instruction++	= PDSEncodeSimpleMOVS(EURASIA_PDS_MOVS_DEST_DOUTI, ui32NextDSConstant, 0, 0);
#endif

				ui32NextDSConstant += EURASIA_PDS_DOUTI_STATE_SIZE;
			}
			
			/*
				Setup the TAG layer control words
			*/
#if(EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
			/* Make sure to align this 4 dword interface to 2 dwords */
			if(ui32NextDSConstant & 0x1)
			{
				ui32NextDSConstant++;
			}
#endif

#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
			*pui32Instruction++	= PDSEncodeSimpleMOVS(EURASIA_PDS_MOVS_DEST_DOUTT, ui32NextDSConstant, psProgram->abMinPack[ui32Iterator], psProgram->aui8FormatConv[ui32Iterator]);
#else
			*pui32Instruction++	= PDSEncodeSimpleMOVS(EURASIA_PDS_MOVS_DEST_DOUTT, ui32NextDSConstant, 0, 0);
#endif
			ui32NextDSConstant	+= EURASIA_TAG_TEXTURE_STATE_SIZE;
		}
		else
		{
			/*
				Iterate the component into the USE primary attribute buffer
			*/
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
			*pui32Instruction++	= PDSEncodeSimpleMOVS(EURASIA_PDS_MOVS_DEST_DOUTI, ui32NextDSConstant, aui8TagSize[ui32Iterator], psProgram->aui8LayerSize[ui32Iterator]);
#else
			*pui32Instruction++	= PDSEncodeSimpleMOVS(EURASIA_PDS_MOVS_DEST_DOUTI, ui32NextDSConstant, 0, 0);
#endif
			ui32NextDSConstant += EURASIA_PDS_DOUTI_STATE_SIZE;
		}
	}
	
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}




/*****************************************************************************
 Function Name	: PDSGeneratePixelStateProgram
 Inputs			: psProgram		- pointer to the PDS pixel state program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS state program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateStateCopyProgram	(PPDS_STATE_COPY_PROGRAM	psProgram,
														 IMG_UINT32                *pui32Buffer)
{
	IMG_UINT32 *pui32Constants;
	IMG_UINT32	ui32NextDS0Constant;
	IMG_UINT32	ui32NextDS1Constant;
	IMG_UINT32	ui32DS0Constant;
	IMG_UINT32	ui32DS1Constant;
	IMG_UINT32	ui32NumConstants;
	IMG_UINT32 *pui32Instructions;
	IMG_UINT32 *pui32Instruction;
	
	PDS_DPF("PDSGenerateStateCopyProgram()");
	
	PDS_ASSERT((((IMG_UINTPTR_T)pui32Buffer) & ((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1)) == 0);
	
	/*
		Generate the PDS pixel state data
	*/
	pui32Constants		= (IMG_UINT32 *) (((IMG_UINTPTR_T)pui32Buffer + ((1UL << EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) - 1)) & ~((IMG_UINTPTR_T)(1UL << EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	/*
		Copy the DMA control words to constants
	*/
	if (psProgram->ui32NumDMAKicks >= 1)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32DMAControl[0]);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32DMAControl[1]);
	}
	if (psProgram->ui32NumDMAKicks == 2)
	{
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 2);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32DMAControl[2]);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 1, psProgram->aui32DMAControl[3]);
	}
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
	PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32USETaskControl[2]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant, ui32NextDS1Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS state code
	*/
	pui32Instructions		= pui32Constants + ui32NumConstants;
	pui32Instruction		= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	/*
		DMA the state into the primary attributes
	*/
	if (psProgram->ui32NumDMAKicks >= 1)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTD,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H);
	}
	if (psProgram->ui32NumDMAKicks == 2)
	{
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 2);
		
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTD,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + PDS_DS0_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 EURASIA_PDS_MOVS_SWIZ_SRC2H,
												 EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 EURASIA_PDS_MOVS_SWIZ_SRC2H);
	}
	
	/*
		Kick the USE to copy the state from the primary attributes into the output buffer
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

/*****************************************************************************
 Function Name	: PDSGenerateVertexShaderSAProgram
 Inputs			: psProgram		- pointer to the PDS vertex shader secondary attributes program
				: pui32Buffer	- pointer to the buffer for the program
				: bShadowSAs	- Copy out control stream object into the shadow.
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS vertex shader seconary attributes program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateVertexShaderSAProgram	(PPDS_VERTEX_SHADER_SA_PROGRAM	psProgram,
															 IMG_UINT32                    *pui32Buffer,
															 IMG_BOOL						bShadowSAs)
{
	IMG_UINT32 *	pui32Constants;
	IMG_UINT32	ui32NextDS0Constant;
	IMG_UINT32	ui32NextDS1Constant;
	IMG_UINT32	ui32DS0Constant;
	IMG_UINT32	ui32DS1Constant;
	IMG_UINT32	ui32NumConstants;
	IMG_UINT32 *	pui32Instructions;
	IMG_UINT32 *	pui32Instruction;
	
	PDS_DPF("PDSGenerateVertexShaderSAProgram()");
	
	PDS_ASSERT((((IMG_UINTPTR_T)pui32Buffer) & ((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1)) == 0);
	
	/*
		Generate the PDS vertex shader secondary attributes data
	*/
	pui32Constants		= (IMG_UINT32 *) (((IMG_UINTPTR_T)pui32Buffer + ((1UL << EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) - 1)) & ~((IMG_UINTPTR_T)(1UL << EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	/*
		Copy the DMA control words to constants
	*/
	
	if (psProgram->ui32NumDMAKicks > 0)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32DMAControl[0]);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32DMAControl[1]);
	}
	if (psProgram->ui32NumDMAKicks == 2)
	{
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 2);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32DMAControl[2]);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 1, psProgram->aui32DMAControl[3]);
	}
	
	if(bShadowSAs)
	{
		/*
			Copy the USE task control words to constants
		*/
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32USETaskControl[2]);
	}
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant, ui32NextDS1Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS vertex shader secondary attributes code
	*/
	pui32Instructions		= pui32Constants + ui32NumConstants;
	pui32Instruction		= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	if (psProgram->ui32NumDMAKicks > 0)
	{
		/*
		  DMA the state into the secondary attributes
		*/
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTD,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H);
	}
	if (psProgram->ui32NumDMAKicks == 2)
	{
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 2);
		
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTD,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + PDS_DS0_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 EURASIA_PDS_MOVS_SWIZ_SRC2H,
												 EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 EURASIA_PDS_MOVS_SWIZ_SRC2H);
	}
	
	if(bShadowSAs)
	{
		/*
			Issue the task to the USE...
			
			(if0) movs doutu, ds0[constant_use], ds0[constant_use], ds1[constant_use], ds1[constant_use], emit
		*/
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTU,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
												 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
	}
	
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}


/*****************************************************************************
 Function Name	: PDSGenerateStaticVertexShaderProgram
 Inputs			: psProgram		- pointer to the PDS vertex shader program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the static PDS vertex shader program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateStaticVertexShaderProgram	(PPDS_VERTEX_SHADER_PROGRAM       psProgram,
																 IMG_UINT32                      *pui32Buffer,
																 PPDS_VERTEX_SHADER_PROGRAM_INFO  psPDSVertexShaderProgramInfo)
{
	IMG_UINT32 *			pui32Constants;
	IMG_UINT32				ui32NextDS0Constant;
	IMG_UINT32				ui32NextDS1Constant;
	IMG_UINT32				ui32DS0Constant;
	IMG_UINT32				ui32DS1Constant;
	IMG_UINT32				ui32NumConstants;
	IMG_UINT32				ui32Stream;
	PPDS_VERTEX_STREAM		psStream;
	IMG_UINT32				ui32Element;
	PPDS_VERTEX_ELEMENT		psElement;
	IMG_UINT32 *			pui32Instructions;
	IMG_UINT32 *			pui32Instruction;
	IMG_UINT32              ui32NextDS0PersistentReg;
	IMG_UINT32              ui32DS0PersistentReg;
	
	/*
		To load a typical set of vertex stream elements
		
		For each stream
		{
			Take the 24-bit index or the 24-bit instance
			Multiply by the stream stride
			For each element
			{
				Add the stream base address and the element offset
				DMA the element into the primray attribute buffer
			}
		}
		
		The straight-line code is organised to never re-visit constants, which are pre-computed into the program data
		
		The USE pre-processing program should unpack and pad the elements with 0s and 1s as required
	*/
	
	PDS_DPF("PDSGenerateStaticVertexShaderProgram()");
	
	/*
		Generate the PDS vertex shader data
	*/
	pui32Constants		     = pui32Buffer;
	ui32NextDS0Constant	     = PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	     = PDS_DS1_CONST_BLOCK_BASE;
	
	for (ui32Stream = 0; ui32Stream < psProgram->ui32NumStreams; ui32Stream++)
	{
		psStream = &psProgram->asStreams[ui32Stream];
		
		
		/*
			Copy the vertex stream stride to a constant
		*/
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psStream->ui32Stride);
		
		/*
			Copy the vertex stream element address, size and primary attribute register to constants
		*/
		for (ui32Element = 0; ui32Element < psStream->ui32NumElements; ui32Element++)
		{
			IMG_UINT32 ui32DOutD1;
			IMG_UINT32 ui32DMASize;
			
			psElement = &psStream->asElements[ui32Element];
			
			/* Size is in bytes - round up to nearest 32 bit word */
			ui32DMASize = (psElement->ui32Size + 3) >> 2;
			
			/*
				Set up the DMA transfer control word.
			*/
			PDS_ASSERT(ui32DMASize <= EURASIA_PDS_DOUTD1_BSIZE_MAX);
			
			ui32DOutD1 =	((ui32DMASize - 1) << EURASIA_PDS_DOUTD1_BSIZE_SHIFT) |
							(psElement->ui32Register << EURASIA_PDS_DOUTD1_AO_SHIFT);
							
							
							
			/*
				Write the dma transfer control words into the PDS data section.
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant, ui32DOutD1);
			
			/*
			   Store the offsets if required
			*/
			if (psPDSVertexShaderProgramInfo)
			{
				psPDSVertexShaderProgramInfo->aui32AddressOffsets[ui32Stream][ui32Element] = PDSGetDS0ConstantOffset(ui32DS0Constant + 0);
				psPDSVertexShaderProgramInfo->aui32ElementOffsets[ui32Stream][ui32Element] = psElement->ui32Offset;
			}
		}
		
		/* Store the number of elements for this stream */
		if (psPDSVertexShaderProgramInfo)
		{
			psPDSVertexShaderProgramInfo->aui32NumElements[ui32Stream] = psStream->ui32NumElements;
		}
	}
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
	PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32USETaskControl[2]);
	
	/*
	   Store the offsets if required
	*/
	if (psPDSVertexShaderProgramInfo)
	{
		psPDSVertexShaderProgramInfo->aui32USETaskControlOffsets[0] = PDSGetDS0ConstantOffset(ui32DS0Constant + 0);
		psPDSVertexShaderProgramInfo->aui32USETaskControlOffsets[1] = PDSGetDS0ConstantOffset(ui32DS0Constant + 1);
		psPDSVertexShaderProgramInfo->aui32USETaskControlOffsets[2] = PDSGetDS1ConstantOffset(ui32DS1Constant + 0);
		
		/*
		   Store the number of streams
		*/
		psPDSVertexShaderProgramInfo->ui32NumStreams          = psProgram->ui32NumStreams;
	}
	
	/*
		Get the number of constants
	*/
	ui32NumConstants	= PDSGetNumConstants(ui32NextDS0Constant, ui32NextDS1Constant);
	ui32NumConstants	= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS vertex shader code
	*/
	pui32Instructions	     = pui32Constants + ui32NumConstants;
	pui32Instruction	     = pui32Instructions;
	ui32NextDS0Constant	     = PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	     = PDS_DS1_CONST_BLOCK_BASE;
	ui32NextDS0PersistentReg = PDS_DS0_VERTEX_TEMP_BLOCK_BASE;
	
	for (ui32Stream = 0; ui32Stream < psProgram->ui32NumStreams; ui32Stream++)
	{
		psStream = &psProgram->asStreams[ui32Stream];
		
		
		if (psStream->ui32Shift)
		{
			/*
				shr ds0[PDS_DS0_INDEX], ir0/1, shift
			*/
			*pui32Instruction++	= PDSEncodeSHR		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_SHIFT_DESTSEL_DS0,
														 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_SHIFT_SRCSEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_SHIFT_SRC_IR1 : EURASIA_PDS_SHIFT_SRC_IR0,
														 EURASIA_PDS_SHIFT_SHIFT_ZERO + psStream->ui32Shift);
		}
		
		/*
			Multiply by the vertex stream stride
			
			mul ds1[PDS_DS1_DMA_ADDRESS], ds0[PDS_DS0_INDEX].l, ds1[constant_stride].l
		*/
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		
		if (psStream->ui32Shift)
		{
			*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MUL_DESTSEL_DS1,
													 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_MUL_SRC1SEL_DS0,
													 EURASIA_PDS_MUL_SRC1_DS0_BASE + PDS_DS0_INDEX * PDS_NUM_WORDS_PER_DWORD,
													 EURASIA_PDS_MUL_SRC2SEL_DS1,
													 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD);
		}
		else
		{
			*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MUL_DESTSEL_DS1,
													 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_MUL_SRC1SEL_REG,
													 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1L : EURASIA_PDS_MUL_SRC1_IR0L,
													 EURASIA_PDS_MUL_SRC2SEL_DS1,
													 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD);
		}
		
		if ((!psStream->bInstanceData && psProgram->b32BitIndices) ||
			(psStream->bInstanceData && (psProgram->ui32NumInstances > 0x10000)))
		{
			/*
				mul ds0[PDS_DS0_ADDRESS_PART], ds0[PDS_DS0_INDEX].h, ds1[constant_stride].l
				shl ds0[PDS_DS0_ADDRESS_PART], ds0[PDS_DS0_ADDRESS_PART], 16
				add ds1[PDS_DS1_DMA_ADDRESS], ds0[PDS_DS0_ADDRESS_PART], ds1[PDS_DS1_DMA_ADDRESS]
			*/
			if (psStream->ui32Multiplier || psStream->ui32Shift)
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_DS0,
														 EURASIA_PDS_MUL_SRC1_DS0_BASE + PDS_DS0_INDEX * PDS_NUM_WORDS_PER_DWORD + 1,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD);
			}
			else
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1H : EURASIA_PDS_MUL_SRC1_IR0H,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD);
			}
			
			*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_SHIFT_DESTSEL_DS0,
													 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SRCSEL_DS0,
													 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SHIFT_ZERO + 16);
													 
			*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_ARITH_DESTSEL_DS1,
													 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_ARITH_SRC1SEL_DS0,
													 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_ARITH_SRC2SEL_DS1,
													 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS);
		}
		
		if (psStream->ui32Stride > 0xFFFF)
		{
			/*
				mul ds0[PDS_DS0_ADDRESS_PART], ds0[PDS_DS0_INDEX].l, ds1[constant_stride].h
				shl ds0[PDS_DS0_ADDRESS_PART], ds1[PDS_DS0_ADDRESS_PART], 16
				add ds1[PDS_DS1_DMA_ADDRESS], ds0[PDS_DS0_ADDRESS_PART], ds1[PDS_DS1_DMA_ADDRESS]
			*/
			if (psStream->ui32Multiplier || psStream->ui32Shift)
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_DS0,
														 EURASIA_PDS_MUL_SRC1_DS0_BASE + PDS_DS0_INDEX * PDS_NUM_WORDS_PER_DWORD,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD + 1);
			}
			else
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1L : EURASIA_PDS_MUL_SRC1_IR0L,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD + 1);
			}
			
			*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_SHIFT_DESTSEL_DS0,
													 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SRCSEL_DS0,
													 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SHIFT_ZERO + 16);
													 
			*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_ARITH_DESTSEL_DS1,
													 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_ARITH_SRC1SEL_DS0,
													 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_ARITH_SRC2SEL_DS1,
													 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS);
		}
		
		ui32DS0PersistentReg = PDSGetTemps(&ui32NextDS0PersistentReg, 1);
		
		for (ui32Element = 0; ui32Element < psStream->ui32NumElements; ui32Element++)
		{
			/*
				Calculate address to fetch data from
				add: temp = stream_address_in_persistent_temp + index_multiplied_by_stride
			*/
			*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_ARITH_DESTSEL_DS1,
													 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS1_TEMP_BLOCK_BASE,
													 EURASIA_PDS_ARITH_SRC1SEL_DS0,
													 EURASIA_PDS_ARITH_SRC1_DS0_BASE + ui32DS0PersistentReg,
													 EURASIA_PDS_ARITH_SRC2SEL_DS1,
													 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS);
													 
			/*
				Get the DMA control word
			*/
			
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
			
			/*
				Set DMA interface with address (temp) and doutd1
			*/
			*pui32Instruction++	= PDSEncodeMOVS 	(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MOVS_DEST_DOUTD,
													 EURASIA_PDS_MOVS_SRC1SEL_DS0,
													 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant  / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SRC2SEL_DS1,
													 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
													 ((PDS_DS1_TEMP_BLOCK_BASE) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
													 ((ui32DS0Constant) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
													 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
													 
													 
		}
	}
	
	/*
		Conditionally issue the task to the USE
		
		(if0) movs doutu, ds0[constant_use], ds0[constant_use], ds1[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_IF0,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

#if !defined(SUPPORT_SGX_PDS_INDEX_CHECKING)
/*****************************************************************************
 Function Name	: PDSGenerateVertexShaderProgram
 Inputs			: psProgram		- pointer to the PDS vertex shader program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS vertex shader program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateVertexShaderProgram	(PPDS_VERTEX_SHADER_PROGRAM       psProgram,
															 IMG_UINT32                      *pui32Buffer,
															 PPDS_VERTEX_SHADER_PROGRAM_INFO  psPDSVertexShaderProgramInfo)
{
	IMG_UINT32 *			pui32Constants;
	IMG_UINT32				ui32NextDS0Constant;
	IMG_UINT32				ui32NextDS1Constant;
	IMG_UINT32				ui32DS0Constant;
	IMG_UINT32				ui32DS1Constant;
	IMG_UINT32				ui32NumConstants;
	IMG_UINT32				ui32Stream;
	PPDS_VERTEX_STREAM		psStream;
	IMG_UINT32				ui32Element;
	PPDS_VERTEX_ELEMENT		psElement;
	IMG_UINT32 *			pui32Instructions;
	IMG_UINT32 *			pui32Instruction;
	
	/*
		To load a typical set of vertex stream elements
		
		For each stream
		{
			Take the 24-bit index or the 24-bit instance
			Divide by the frequency if necessary
			Multiply by the stream stride
			For each element
			{
				Add the stream base address and the element offset
				DMA the element into the primray attribute buffer
			}
		}
		
		The straight-line code is organised to never re-visit constants, which are pre-computed into the program data
		
		The USE pre-processing program should unpack and pad the elements with 0s and 1s as required
	*/
	
	PDS_DPF("PDSGenerateVertexShaderProgram()");
	
	/*
		Generate the PDS vertex shader data
	*/
	pui32Constants		= pui32Buffer;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	for (ui32Stream = 0; ui32Stream < psProgram->ui32NumStreams; ui32Stream++)
	{
		psStream = &psProgram->asStreams[ui32Stream];
		
		/*
			Copy the vertex stream frequency multiplier to a constant if necessary
		*/
		if (psStream->ui32Multiplier)
		{
			ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
			PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psStream->ui32Multiplier | 0x01000000);
		}
		
		/*
			Copy the vertex stream stride to a constant
		*/
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psStream->ui32Stride);
		
		/*
			Copy the vertex stream element address, size and primary attribute register to constants
		*/
		for (ui32Element = 0; ui32Element < psStream->ui32NumElements; ui32Element++)
		{
			IMG_UINT32 ui32DOutD1;
			IMG_UINT32 ui32DMASize;
			
			psElement = &psStream->asElements[ui32Element];
			
			/* Size is in bytes - round up to nearest 32 bit word */
			ui32DMASize = (psElement->ui32Size + 3) >> 2;
			
			/*
				Set up the DMA transfer control word.
			*/
			PDS_ASSERT(ui32DMASize <= EURASIA_PDS_DOUTD1_BSIZE_MAX);
			
			ui32DOutD1 =	((ui32DMASize - 1) << EURASIA_PDS_DOUTD1_BSIZE_SHIFT) |
							(psElement->ui32Register << EURASIA_PDS_DOUTD1_AO_SHIFT);
							
			/*
				Write the dma transfer control words into the PDS data section.
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psStream->ui32Address + psElement->ui32Offset);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, ui32DOutD1);
			
			/*
			   Store the offsets if required
			*/
			if (psPDSVertexShaderProgramInfo)
			{
				psPDSVertexShaderProgramInfo->aui32AddressOffsets[ui32Stream][ui32Element] = PDSGetDS0ConstantOffset(ui32DS0Constant + 0);
				psPDSVertexShaderProgramInfo->aui32ElementOffsets[ui32Stream][ui32Element] = psElement->ui32Offset;
			}
		}
		
		/* Store the number of elements for this stream */
		if (psPDSVertexShaderProgramInfo)
		{
			psPDSVertexShaderProgramInfo->aui32NumElements[ui32Stream] = psStream->ui32NumElements;
		}
	}
	
	if (psProgram->bIterateVtxID)
	{
		/* Set up the douta word in the next DS1 constant */
		if (psProgram->ui32VtxIDOffset)
		{
			/* if an additional offset is present it gets its own DS1 constant */
			ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 2);
			PDSSetDS1Constant(pui32Constants, ui32DS1Constant +0,
							  (psProgram->ui32VtxIDRegister << EURASIA_PDS_DOUTA1_AO_SHIFT));
			PDSSetDS1Constant(pui32Constants, ui32DS1Constant +1,
							  psProgram->ui32VtxIDOffset);
		}
		else
		{
			ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
			PDSSetDS1Constant(pui32Constants, ui32DS1Constant +0,
							  (psProgram->ui32VtxIDRegister << EURASIA_PDS_DOUTA1_AO_SHIFT));
		}
	}

	if (psProgram->bIterateInstanceID)
	{
		/* Set up the douta word in the next DS1 constant */
		if (psProgram->ui32InstanceIDOffset)
		{
			/* store an additional offset if present */
			ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 2);
			PDSSetDS1Constant(pui32Constants, ui32DS1Constant +0,
							  (psProgram->ui32InstanceIDRegister << EURASIA_PDS_DOUTA1_AO_SHIFT));
			PDSSetDS1Constant(pui32Constants, ui32DS1Constant +1,
							  psProgram->ui32InstanceIDOffset);
		}
		else
		{
			ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
			PDSSetDS1Constant(pui32Constants, ui32DS1Constant +0,
							  (psProgram->ui32InstanceIDRegister << EURASIA_PDS_DOUTA1_AO_SHIFT));
		}
	}
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
	PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32USETaskControl[2]);
	
	/*
	   Store the offsets if required
	*/
	if (psPDSVertexShaderProgramInfo)
	{
		psPDSVertexShaderProgramInfo->aui32USETaskControlOffsets[0] = PDSGetDS0ConstantOffset(ui32DS0Constant + 0);
		psPDSVertexShaderProgramInfo->aui32USETaskControlOffsets[1] = PDSGetDS0ConstantOffset(ui32DS0Constant + 1);
		psPDSVertexShaderProgramInfo->aui32USETaskControlOffsets[2] = PDSGetDS1ConstantOffset(ui32DS1Constant + 0);
		
		/*
		   Store the number of streams
		*/
		psPDSVertexShaderProgramInfo->ui32NumStreams          = psProgram->ui32NumStreams;
	}
	
	/*
		Get the number of constants
	*/
	ui32NumConstants	= PDSGetNumConstants(ui32NextDS0Constant, ui32NextDS1Constant);
	ui32NumConstants	= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS vertex shader code
	*/
	pui32Instructions	= pui32Constants + ui32NumConstants;
	pui32Instruction	= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	for (ui32Stream = 0; ui32Stream < psProgram->ui32NumStreams; ui32Stream++)
	{
		psStream = &psProgram->asStreams[ui32Stream];
		
		/*
			Divide by the frequency if necessary
			
			How does this work? I'll tell you:
			Iout = (Iin * (Multiplier+2^24)) >> (Shift+24)
			This allows streams to be fetched at a rate of less than one
			
			The way we do this with 16 bit multiplies is as follows
			
			Simple case:
			
			( (i * M.l) + ((i * M.h) << 16) ) >> (shift + 24)
			
			but as this overflows easily, we shift early to get the following
			
			( ((i * M.l) >> 15) + ((i * M.h) << 1) ) >> (shift + 24 - 15)
			
			Where i = Iin, and M.l and M.h are Multiplier low word and high word.
			
			In the case where ui32InstanceCount > 0x10000, we need to use the high
			part of ir1 (i.e. ir1.h)
			
			So we actually do
			
			((i.l*M.l)>>15 + (i.l*M.h)<<1 + (i.h*M.l)<<1 + (i.h*M.h)<<17) >> (shift + 24 - 15)
		*/
		if (psStream->ui32Multiplier)
		{
			/*
				mul ds0[PDS_DS0_INDEX], ir0/1.l, ds1[constant_multiplier].l
				shr ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], 15
				mul ds1[PDS_DS1_INDEX_PART], ir0/1.l, ds1[constant_multiplier].h
				shl ds1[PDS_DS1_INDEX_PART], ds1[PDS_DS1_INDEX_PART], 1
			*/
			ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
			
			*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MUL_DESTSEL_DS0,
													 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_INDEX,
													 EURASIA_PDS_MUL_SRC1SEL_REG,
													 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1L : EURASIA_PDS_MUL_SRC1_IR0L,
													 EURASIA_PDS_MUL_SRC2SEL_DS1,
													 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD);
													 
			*pui32Instruction++	= PDSEncodeSHR		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_SHIFT_DESTSEL_DS0,
													 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_INDEX,
													 EURASIA_PDS_SHIFT_SRCSEL_DS0,
													 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS0_INDEX,
													 EURASIA_PDS_SHIFT_SHIFT_ZERO + 15);
													 
			*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MUL_DESTSEL_DS1,
													 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_INDEX_PART,
													 EURASIA_PDS_MUL_SRC1SEL_REG,
													 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1L : EURASIA_PDS_MUL_SRC1_IR0L,
													 EURASIA_PDS_MUL_SRC2SEL_DS1,
													 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD + 1);
													 
			*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_SHIFT_DESTSEL_DS1,
													 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS1_INDEX_PART,
													 EURASIA_PDS_SHIFT_SRCSEL_DS1,
													 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS1_INDEX_PART,
													 EURASIA_PDS_SHIFT_SHIFT_ZERO + 1);
													 
			if ((!psStream->bInstanceData && !psProgram->b32BitIndices) ||
				(psStream->bInstanceData && (psProgram->ui32NumInstances <= 0x10000)))
			{
				/*
					add ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], ds1[PDS_DS1_INDEX_PART]
				*/
				*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_ARITH_DESTSEL_DS0,
														 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC1SEL_DS0,
														 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC2SEL_DS1,
														 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_INDEX_PART);
			}
			else
			{
				/*
					add ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], ds1[PDS_DS1_INDEX_PART]
					mul ds1[PDS_DS1_INDEX_PART], ir0/1.h, ds1[const_multiplier].l
					shl ds1[PDS_DS1_INDEX_PART], ds1[PDS_DS1_INDEX_PART], 1
					add ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], ds1[PDS_DS1_INDEX_PART]
					mul ds1[PDS_DS1_INDEX_PART], ir0/1.h, ds1[const_multiplier].h
					shl ds1[PDS_DS1_INDEX_PART], ds1[PDS_DS1_INDEX_PART], 17
					add ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], ds1[PDS_DS1_INDEX_PART]
				*/
				*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_ARITH_DESTSEL_DS0,
														 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC1SEL_DS0,
														 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC2SEL_DS1,
														 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_INDEX_PART);
														 
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS1,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_INDEX_PART,
														 EURASIA_PDS_MUL_SRC1SEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1H : EURASIA_PDS_MUL_SRC1_IR0H,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD);
				*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
						EURASIA_PDS_SHIFT_DESTSEL_DS1,
						EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS1_INDEX_PART,
						EURASIA_PDS_SHIFT_SRCSEL_DS1,
						EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS1_INDEX_PART,
						EURASIA_PDS_SHIFT_SHIFT_ZERO + 1);
						
				*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_ARITH_DESTSEL_DS0,
														 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC1SEL_DS0,
														 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC2SEL_DS1,
														 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_INDEX_PART);
														 
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS1,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_INDEX_PART,
														 EURASIA_PDS_MUL_SRC1SEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1H : EURASIA_PDS_MUL_SRC1_IR0H,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD + 1);
														 
				*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_SHIFT_DESTSEL_DS1,
														 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS1_INDEX_PART,
														 EURASIA_PDS_SHIFT_SRCSEL_DS1,
														 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS1_INDEX_PART,
														 EURASIA_PDS_SHIFT_SHIFT_ZERO + 17);
														 
				*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_ARITH_DESTSEL_DS0,
														 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC1SEL_DS0,
														 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC2SEL_DS1,
														 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_INDEX_PART);
			}
			
			if (psStream->ui32Shift)
			{
				/*
					shr ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], shift + (24 - 15)
				*/
				*pui32Instruction++	= PDSEncodeSHR		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_SHIFT_DESTSEL_DS0,
														 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_SHIFT_SRCSEL_DS0,
														 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_SHIFT_SHIFT_ZERO + psStream->ui32Shift + 24 - 15);
			}
		}
		else
		{
			if (psStream->ui32Shift)
			{
				/*
					shr ds0[PDS_DS0_INDEX], ir0/1, shift
				*/
				*pui32Instruction++	= PDSEncodeSHR		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_SHIFT_DESTSEL_DS0,
														 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_SHIFT_SRCSEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_SHIFT_SRC_IR1 : EURASIA_PDS_SHIFT_SRC_IR0,
														 EURASIA_PDS_SHIFT_SHIFT_ZERO + psStream->ui32Shift);
			}
		}
		
		/*
			Multiply by the vertex stream stride
			
			mul ds1[PDS_DS1_DMA_ADDRESS], ds0[PDS_DS0_INDEX].l, ds1[constant_stride].l
		*/
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		
		if (psStream->ui32Multiplier || psStream->ui32Shift)
		{
			*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MUL_DESTSEL_DS1,
													 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_MUL_SRC1SEL_DS0,
													 EURASIA_PDS_MUL_SRC1_DS0_BASE + PDS_DS0_INDEX * PDS_NUM_WORDS_PER_DWORD,
													 EURASIA_PDS_MUL_SRC2SEL_DS1,
													 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD);
		}
		else
		{
			*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MUL_DESTSEL_DS1,
													 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_MUL_SRC1SEL_REG,
													 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1L : EURASIA_PDS_MUL_SRC1_IR0L,
													 EURASIA_PDS_MUL_SRC2SEL_DS1,
													 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD);
		}
		
		if ((!psStream->bInstanceData && psProgram->b32BitIndices) ||
			(psStream->bInstanceData && (psProgram->ui32NumInstances > 0x10000)))
		{
			/*
				mul ds0[PDS_DS0_ADDRESS_PART], ds0[PDS_DS0_INDEX].h, ds1[constant_stride].l
				shl ds0[PDS_DS0_ADDRESS_PART], ds0[PDS_DS0_ADDRESS_PART], 16
				add ds1[PDS_DS1_DMA_ADDRESS], ds0[PDS_DS0_ADDRESS_PART], ds1[PDS_DS1_DMA_ADDRESS]
			*/
			if (psStream->ui32Multiplier || psStream->ui32Shift)
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_DS0,
														 EURASIA_PDS_MUL_SRC1_DS0_BASE + PDS_DS0_INDEX * PDS_NUM_WORDS_PER_DWORD + 1,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD);
			}
			else
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1H : EURASIA_PDS_MUL_SRC1_IR0H,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD);
			}
			
			*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_SHIFT_DESTSEL_DS0,
													 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SRCSEL_DS0,
													 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SHIFT_ZERO + 16);
													 
			*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_ARITH_DESTSEL_DS1,
													 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_ARITH_SRC1SEL_DS0,
													 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_ARITH_SRC2SEL_DS1,
													 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS);
		}
		
		if (psStream->ui32Stride > 0xFFFF)
		{
			/*
				mul ds0[PDS_DS0_ADDRESS_PART], ds0[PDS_DS0_INDEX].l, ds1[constant_stride].h
				shl ds0[PDS_DS0_ADDRESS_PART], ds1[PDS_DS0_ADDRESS_PART], 16
				add ds1[PDS_DS1_DMA_ADDRESS], ds0[PDS_DS0_ADDRESS_PART], ds1[PDS_DS1_DMA_ADDRESS]
			*/
			if (psStream->ui32Multiplier || psStream->ui32Shift)
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_DS0,
														 EURASIA_PDS_MUL_SRC1_DS0_BASE + PDS_DS0_INDEX * PDS_NUM_WORDS_PER_DWORD,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD + 1);
			}
			else
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1L : EURASIA_PDS_MUL_SRC1_IR0L,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD + 1);
			}
			
			*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_SHIFT_DESTSEL_DS0,
													 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SRCSEL_DS0,
													 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SHIFT_ZERO + 16);
													 
			*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_ARITH_DESTSEL_DS1,
													 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_ARITH_SRC1SEL_DS0,
													 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_ARITH_SRC2SEL_DS1,
													 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS);
		}
		
		for (ui32Element = 0; ui32Element < psStream->ui32NumElements; ui32Element++)
		{
			/*
				Get the DMA source address, destination register and size
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
			
			/*
			  Add the vertex stream base address and vertex element offset
			  
			  And DMA the vertex element into the USE primary attribute buffer
			  
			  movsa doutd, ds0[constant_address], ds0[constant_size_and_register], ds1[PDS_DS1_DMA_ADDRESS], ds1[PDS_DS1_DMA_ADDRESS], emit
			*/
			*pui32Instruction++	= PDSEncodeMOVSA	(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MOVS_DEST_DOUTD,
													 EURASIA_PDS_MOVS_SRC1SEL_DS0,
													 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SRC2SEL_DS1,
													 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS / PDS_NUM_DWORDS_PER_REG,
													 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
													 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
		}
	}
	
	/* Set up the vertex ID iteration */
	if (psProgram->bIterateVtxID)
	{
		/* Add the offset if present */
		if (psProgram->ui32VtxIDOffset)
		{
			ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 2);
		
			*pui32Instruction++	= PDSEncodeMOVSA	(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MOVS_DEST_DOUTA,
													 EURASIA_PDS_MOVS_SRC1SEL_REG,
													 EURASIA_PDS_MOVS_SRC1_IR0,
													 EURASIA_PDS_MOVS_SRC2SEL_DS1,
													 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 ((ui32DS1Constant +0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
													 EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 ((ui32DS1Constant +1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
		}
		else
		{
			ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		
			*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MOVS_DEST_DOUTA,
													 EURASIA_PDS_MOVS_SRC1SEL_REG,
													 EURASIA_PDS_MOVS_SRC1_IR0,
													 EURASIA_PDS_MOVS_SRC2SEL_DS1,
													 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 ((ui32DS1Constant +0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
													 EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 ((ui32DS1Constant +0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
		}
	}

	/* and the instance ID */
	if (psProgram->bIterateInstanceID)
	{
		/* Add the offset if present */
		if (psProgram->ui32InstanceIDOffset)
		{
			ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 2);
		
			*pui32Instruction++	= PDSEncodeMOVSA	(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MOVS_DEST_DOUTA,
													 EURASIA_PDS_MOVS_SRC1SEL_REG,
													 EURASIA_PDS_MOVS_SRC1_IR1,
													 EURASIA_PDS_MOVS_SRC2SEL_DS1,
													 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 ((ui32DS1Constant +0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
													 EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 ((ui32DS1Constant +1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
		}
		else
		{
			ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		
			*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MOVS_DEST_DOUTA,
													 EURASIA_PDS_MOVS_SRC1SEL_REG,
													 EURASIA_PDS_MOVS_SRC1_IR1,
													 EURASIA_PDS_MOVS_SRC2SEL_DS1,
													 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 ((ui32DS1Constant +0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
													 EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 ((ui32DS1Constant +0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
		}
	}
	
	/*
		Conditionally issue the task to the USE
		
		(if0) movs doutu, ds0[constant_use], ds0[constant_use], ds1[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_IF0,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

#else /* !defined(SUPPORT_SGX_PDS_INDEX_CHECKING) */

/*****************************************************************************
 Function Name	: PDSGenerateVertexShaderProgram
 Inputs			: psProgram		- pointer to the PDS vertex shader program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS vertex shader program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateVertexShaderProgram	(PPDS_VERTEX_SHADER_PROGRAM       psProgram,
															 IMG_UINT32                      *pui32Buffer,
															 PPDS_VERTEX_SHADER_PROGRAM_INFO  psPDSVertexShaderProgramInfo)
{
	IMG_UINT32 *			pui32Constants;
	IMG_UINT32				ui32NextDS0Constant;
	IMG_UINT32				ui32NextDS1Constant;
	IMG_UINT32				ui32DS0Constant;
	IMG_UINT32				ui32DS1Constant;
	IMG_UINT32				ui32NumConstants;
	IMG_UINT32				ui32Stream;
	PPDS_VERTEX_STREAM		psStream;
	IMG_UINT32				ui32Element;
	PPDS_VERTEX_ELEMENT		psElement;
	IMG_UINT32 *			pui32Instructions;
	IMG_UINT32 *			pui32Instruction;
	IMG_UINT32 				ui32DS1Constant_VertexCount;
	IMG_UINT32 				ui32DS1Constant_DefaultIndex;
	
	/*
		To load a typical set of vertex stream elements
		
		For each stream
		{
			Take the 24-bit index or the 24-bit instance
			Divide by the frequency if necessary
			Multiply by the stream stride
			For each element
			{
				Add the stream base address and the element offset
				DMA the element into the primray attribute buffer
			}
		}
		
		The straight-line code is organised to never re-visit constants, which are pre-computed into the program data
		
		The USE pre-processing program should unpack and pad the elements with 0s and 1s as required
	*/
	
	PDS_DPF("PDSGenerateVertexShaderProgram()");
	
	/*
		Generate the PDS vertex shader data
	*/
	pui32Constants		= pui32Buffer;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	/*
		Vertex count and default index for use in invalid index handling..
	*/
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->ui32VertexCount);
	
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, 0);
	
	for (ui32Stream = 0; ui32Stream < psProgram->ui32NumStreams; ui32Stream++)
	{
		psStream = &psProgram->asStreams[ui32Stream];
		
		/*
			Copy the vertex stream frequency multiplier to a constant if necessary
		*/
		if (psStream->ui32Multiplier)
		{
			ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
			PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psStream->ui32Multiplier | 0x01000000);
		}
		
		/*
			Copy the vertex stream stride to a constant
		*/
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psStream->ui32Stride);
		
		/*
			Copy the vertex stream element address, size and primary attribute register to constants
		*/
		for (ui32Element = 0; ui32Element < psStream->ui32NumElements; ui32Element++)
		{
			IMG_UINT32 ui32DOutD1;
			IMG_UINT32 ui32DMASize;
			
			psElement = &psStream->asElements[ui32Element];
			
			/* Size is in bytes - round up to nearest 32 bit word */
			ui32DMASize = (psElement->ui32Size + 3) >> 2;
			
			/*
				Set up the DMA transfer control word.
			*/
			PDS_ASSERT(ui32DMASize <= EURASIA_PDS_DOUTD1_BSIZE_MAX);
			
			ui32DOutD1 =	((ui32DMASize - 1) << EURASIA_PDS_DOUTD1_BSIZE_SHIFT) |
							(psElement->ui32Register << EURASIA_PDS_DOUTD1_AO_SHIFT);
							
			/*
				Write the dma transfer control words into the PDS data section.
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psStream->ui32Address + psElement->ui32Offset);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, ui32DOutD1);
			
			/*
			   Store the offsets if required
			*/
			if (psPDSVertexShaderProgramInfo)
			{
				psPDSVertexShaderProgramInfo->aui32AddressOffsets[ui32Stream][ui32Element] = PDSGetDS0ConstantOffset(ui32DS0Constant + 0);
				psPDSVertexShaderProgramInfo->aui32ElementOffsets[ui32Stream][ui32Element] = psElement->ui32Offset;
			}
		}
		
		/* Store the number of elements for this stream */
		if (psPDSVertexShaderProgramInfo)
		{
			psPDSVertexShaderProgramInfo->aui32NumElements[ui32Stream] = psStream->ui32NumElements;
		}
	}
	
	if (psProgram->bIterateVtxID)
	{
		/* Set up the douta word in the next DS1 constant */
		if (psProgram->ui32VtxIDOffset)
		{
			/* if an additional offset is present it gets its own DS1 constant */
			ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 2);
			PDSSetDS1Constant(pui32Constants, ui32DS1Constant +0,
							  (psProgram->ui32VtxIDRegister << EURASIA_PDS_DOUTA1_AO_SHIFT));
			PDSSetDS1Constant(pui32Constants, ui32DS1Constant +1,
							  psProgram->ui32VtxIDOffset);
		}
		else
		{
			ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
			PDSSetDS1Constant(pui32Constants, ui32DS1Constant +0,
							  (psProgram->ui32VtxIDRegister << EURASIA_PDS_DOUTA1_AO_SHIFT));
		}
	}

	if (psProgram->bIterateInstanceID)
	{
		/* Set up the douta word in the next DS1 constant */
		if (psProgram->ui32InstanceIDOffset)
		{
			/* store an additional offset if present */
			ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 2);
			PDSSetDS1Constant(pui32Constants, ui32DS1Constant +0,
							  (psProgram->ui32InstanceIDRegister << EURASIA_PDS_DOUTA1_AO_SHIFT));
			PDSSetDS1Constant(pui32Constants, ui32DS1Constant +1,
							  psProgram->ui32InstanceIDOffset);
		}
		else
		{
			ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
			PDSSetDS1Constant(pui32Constants, ui32DS1Constant +0,
							  (psProgram->ui32InstanceIDRegister << EURASIA_PDS_DOUTA1_AO_SHIFT));
		}
	}
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
	PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32USETaskControl[2]);
	
	/*
	   Store the offsets if required
	*/
	if (psPDSVertexShaderProgramInfo)
	{
		psPDSVertexShaderProgramInfo->aui32USETaskControlOffsets[0] = PDSGetDS0ConstantOffset(ui32DS0Constant + 0);
		psPDSVertexShaderProgramInfo->aui32USETaskControlOffsets[1] = PDSGetDS0ConstantOffset(ui32DS0Constant + 1);
		psPDSVertexShaderProgramInfo->aui32USETaskControlOffsets[2] = PDSGetDS1ConstantOffset(ui32DS1Constant + 0);
		
		/*
		   Store the number of streams
		*/
		psPDSVertexShaderProgramInfo->ui32NumStreams          = psProgram->ui32NumStreams;
	}
	
	/*
		Get the number of constants
	*/
	ui32NumConstants	= PDSGetNumConstants(ui32NextDS0Constant, ui32NextDS1Constant);
	ui32NumConstants	= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS vertex shader code
	*/
	pui32Instructions	= pui32Constants + ui32NumConstants;
	pui32Instruction	= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	ui32DS1Constant_VertexCount = ui32DS1Constant;
	
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	ui32DS1Constant_DefaultIndex = ui32DS1Constant;
	
	for (ui32Stream = 0; ui32Stream < psProgram->ui32NumStreams; ui32Stream++)
	{
		psStream = &psProgram->asStreams[ui32Stream];
		
		/*
			Divide by the frequency if necessary
			
			How does this work? I'll tell you:
			Iout = (Iin * (Multiplier+2^24)) >> (Shift+24)
			This allows streams to be fetched at a rate of less than one
			
			The way we do this with 16 bit multiplies is as follows
			
			Simple case:
			
			( (i * M.l) + ((i * M.h) << 16) ) >> (shift + 24)
			
			but as this overflows easily, we shift early to get the following
			
			( ((i * M.l) >> 15) + ((i * M.h) << 1) ) >> (shift + 24 - 15)
			
			Where i = Iin, and M.l and M.h are Multiplier low word and high word.
			
			In the case where ui32InstanceCount > 0x10000, we need to use the high
			part of ir1 (i.e. ir1.h)
			
			So we actually do
			
			((i.l*M.l)>>15 + (i.l*M.h)<<1 + (i.h*M.l)<<1 + (i.h*M.h)<<17) >> (shift + 24 - 15)
		*/
		if (psStream->ui32Multiplier)
		{
			/*
				mul ds0[PDS_DS0_INDEX], ir0/1.l, ds1[constant_multiplier].l
				shr ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], 15
				mul ds1[PDS_DS1_INDEX_PART], ir0/1.l, ds1[constant_multiplier].h
				shl ds1[PDS_DS1_INDEX_PART], ds1[PDS_DS0_INDEX_PART], 16
			*/
			ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
			
			*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MUL_DESTSEL_DS0,
													 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_INDEX,
													 EURASIA_PDS_MUL_SRC1SEL_REG,
													 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1L : EURASIA_PDS_MUL_SRC1_IR0L,
													 EURASIA_PDS_MUL_SRC2SEL_DS1,
													 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD);
													 
			*pui32Instruction++	= PDSEncodeSHR		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_SHIFT_DESTSEL_DS0,
													 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_INDEX,
													 EURASIA_PDS_SHIFT_SRCSEL_DS0,
													 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS0_INDEX,
													 EURASIA_PDS_SHIFT_SHIFT_ZERO + 15);
													 
			*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MUL_DESTSEL_DS1,
													 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_INDEX_PART,
													 EURASIA_PDS_MUL_SRC1SEL_REG,
													 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1L : EURASIA_PDS_MUL_SRC1_IR0L,
													 EURASIA_PDS_MUL_SRC2SEL_DS1,
													 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD + 1);
													 
			*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_SHIFT_DESTSEL_DS1,
													 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS1_INDEX_PART,
													 EURASIA_PDS_SHIFT_SRCSEL_DS1,
													 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS1_INDEX_PART,
													 EURASIA_PDS_SHIFT_SHIFT_ZERO + 1);
													 
			if ((!psStream->bInstanceData && !psProgram->b32BitIndices) ||
				(psStream->bInstanceData && (psProgram->ui32NumInstances <= 0x10000)))
			{
				/*
					add ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], ds1[PDS_DS1_INDEX_PART]
				*/
				*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_ARITH_DESTSEL_DS0,
														 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC1SEL_DS0,
														 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC2SEL_DS1,
														 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_INDEX_PART);
			}
			else
			{
				/*
					add ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], ds1[PDS_DS1_INDEX_PART]
					mul ds1[PDS_DS1_INDEX_PART], ir0/1.h, ds1[const_multiplier].l
					shl ds1[PDS_DS1_INDEX_PART], ds1[PDS_DS1_INDEX_PART], 1
					add ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], ds1[PDS_DS1_INDEX_PART]
					mul ds1[PDS_DS1_INDEX_PART], ir0/1.h, ds1[const_multiplier].h
					shl ds1[PDS_DS1_INDEX_PART], ds1[PDS_DS1_INDEX_PART], 17
					add ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], ds1[PDS_DS1_INDEX_PART]
				*/
				*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_ARITH_DESTSEL_DS0,
														 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC1SEL_DS0,
														 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC2SEL_DS1,
														 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_INDEX_PART);
														 
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS1,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_INDEX_PART,
														 EURASIA_PDS_MUL_SRC1SEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1H : EURASIA_PDS_MUL_SRC1_IR0H,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD);

				*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
														EURASIA_PDS_SHIFT_DESTSEL_DS1,
														EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS1_INDEX_PART,
														EURASIA_PDS_SHIFT_SRCSEL_DS1,
														EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS1_INDEX_PART,
														EURASIA_PDS_SHIFT_SHIFT_ZERO + 1);
														 
				*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_ARITH_DESTSEL_DS0,
														 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC1SEL_DS0,
														 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC2SEL_DS1,
														 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_INDEX_PART);
														 
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS1,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_INDEX_PART,
														 EURASIA_PDS_MUL_SRC1SEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1H : EURASIA_PDS_MUL_SRC1_IR0H,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD + 1);
														 
				*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_SHIFT_DESTSEL_DS1,
														 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS1_INDEX_PART,
														 EURASIA_PDS_SHIFT_SRCSEL_DS1,
														 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS1_INDEX_PART,
														 EURASIA_PDS_SHIFT_SHIFT_ZERO + 17);
														 
				*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_ARITH_DESTSEL_DS0,
														 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC1SEL_DS0,
														 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC2SEL_DS1,
														 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_INDEX_PART);
			}
			
			if (psStream->ui32Shift)
			{
				/*
					shr ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], shift + (24 - 15)
				*/
				*pui32Instruction++	= PDSEncodeSHR		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_SHIFT_DESTSEL_DS0,
														 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_SHIFT_SRCSEL_DS0,
														 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_SHIFT_SHIFT_ZERO + psStream->ui32Shift + 24 - 15);
			}
		}
		else
		{
			if (psStream->ui32Shift)
			{
				/*
					shr ds0[PDS_DS0_INDEX], ir0/1, shift
				*/
				*pui32Instruction++	= PDSEncodeSHR		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_SHIFT_DESTSEL_DS0,
														 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_SHIFT_SRCSEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_SHIFT_SRC_IR1 : EURASIA_PDS_SHIFT_SRC_IR0,
														 EURASIA_PDS_SHIFT_SHIFT_ZERO + psStream->ui32Shift);
			}
			else
			{
				/*
					mov32 ds0[PDS_DS0_INDEX], ir0/1
				*/
				*pui32Instruction++	= PDSEncodeMOV32	(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MOV32_DESTSEL_DS0,
														 EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_MOV32_SRCSEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_MOV32_SRC_IR1 : EURASIA_PDS_MOV32_SRC_IR0);
			}
		}
		
		/*
			Check for invalid indices. This causes the ALU to indicate invalid
			indices via it's ALUN register (true indicates a valid index). Invalid
			indices are then replaced with a safe default value.
			
			alum signed
			sub ds1[PDS_DS1_DMA_ADDRESS], ir0/1, ds1[constant_vertexcount]
			(alun) br valid
			mov32 ds0[PDS_DS0_INDEX], ds1[constant_defaultindex]
		valid:
			alum unsigned
			
			Note that we use the vertex count and not the max vertex because the ALU treats
			zero as a negative number.
		*/
		*pui32Instruction++	= PDSEncodeALUM		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_FLOW_ALUM_MODE_SIGNED);
												 
		*pui32Instruction++	= PDSEncodeSUB		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_ARITH_DESTSEL_DS1,
												 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
												 EURASIA_PDS_ARITH_SRC1SEL_REG,
												 psStream->bInstanceData ? EURASIA_PDS_ARITH_SRC1_IR1 : EURASIA_PDS_ARITH_SRC1_IR0,
												 EURASIA_PDS_ARITH_SRC2SEL_DS1,
												 EURASIA_PDS_ARITH_SRC2_DS1_BASE + ui32DS1Constant_VertexCount);
												 
		*pui32Instruction = PDSEncodeBRA		(EURASIA_PDS_CC_ALUN,
												((IMG_UINT32)(pui32Instruction - pui32Instructions) + 2) << EURASIA_PDS_FLOW_DEST_ALIGNSHIFT);
		pui32Instruction++;
		
		*pui32Instruction++	= PDSEncodeMOV32	(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOV32_DESTSEL_DS0,
												 EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS0_INDEX,
												 EURASIA_PDS_MOV32_SRCSEL_DS1,
												 EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS1Constant_DefaultIndex);
												 
		*pui32Instruction++	= PDSEncodeALUM		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_FLOW_ALUM_MODE_UNSIGNED);
												 
		/*
			Multiply by the vertex stream stride
			
			mul ds1[PDS_DS1_DMA_ADDRESS], ds0[PDS_DS0_INDEX].l, ds1[constant_stride].l
		*/
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		
		if (psStream->ui32Multiplier || psStream->ui32Shift)
		{
			*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MUL_DESTSEL_DS1,
													 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_MUL_SRC1SEL_DS0,
													 EURASIA_PDS_MUL_SRC1_DS0_BASE + PDS_DS0_INDEX * PDS_NUM_WORDS_PER_DWORD,
													 EURASIA_PDS_MUL_SRC2SEL_DS1,
													 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD);
		}
		else
		{
			*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MUL_DESTSEL_DS1,
													 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_MUL_SRC1SEL_REG,
													 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1L : EURASIA_PDS_MUL_SRC1_IR0L,
													 EURASIA_PDS_MUL_SRC2SEL_DS1,
													 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD);
		}
		
		if ((!psStream->bInstanceData && psProgram->b32BitIndices) ||
			(psStream->bInstanceData && (psProgram->ui32NumInstances > 0x10000)))
		{
			/*
				mul ds0[PDS_DS0_ADDRESS_PART], ds0[PDS_DS0_INDEX].h, ds1[constant_stride].l
				shl ds0[PDS_DS0_ADDRESS_PART], ds0[PDS_DS0_ADDRESS_PART], 16
				add ds1[PDS_DS1_DMA_ADDRESS], ds0[PDS_DS0_ADDRESS_PART], ds1[PDS_DS1_DMA_ADDRESS]
			*/
			if (psStream->ui32Multiplier || psStream->ui32Shift)
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_DS0,
														 EURASIA_PDS_MUL_SRC1_DS0_BASE + PDS_DS0_INDEX * PDS_NUM_WORDS_PER_DWORD + 1,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD);
			}
			else
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1H : EURASIA_PDS_MUL_SRC1_IR0H,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD);
			}
			
			*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_SHIFT_DESTSEL_DS0,
													 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SRCSEL_DS0,
													 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SHIFT_ZERO + 16);
													 
			*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_ARITH_DESTSEL_DS1,
													 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_ARITH_SRC1SEL_DS0,
													 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_ARITH_SRC2SEL_DS1,
													 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS);
		}
		
		if (psStream->ui32Stride > 0xFFFF)
		{
			/*
				mul ds0[PDS_DS0_ADDRESS_PART], ds0[PDS_DS0_INDEX].l, ds1[constant_stride].h
				shl ds0[PDS_DS0_ADDRESS_PART], ds1[PDS_DS0_ADDRESS_PART], 16
				add ds1[PDS_DS1_DMA_ADDRESS], ds0[PDS_DS0_ADDRESS_PART], ds1[PDS_DS1_DMA_ADDRESS]
			*/
			if (psStream->ui32Multiplier || psStream->ui32Shift)
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_DS0,
														 EURASIA_PDS_MUL_SRC1_DS0_BASE + PDS_DS0_INDEX * PDS_NUM_WORDS_PER_DWORD,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD + 1);
			}
			else
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1L : EURASIA_PDS_MUL_SRC1_IR0L,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD + 1);
			}
			
			*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_SHIFT_DESTSEL_DS0,
													 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SRCSEL_DS0,
													 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SHIFT_ZERO + 16);
													 
			*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_ARITH_DESTSEL_DS1,
													 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_ARITH_SRC1SEL_DS0,
													 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_ARITH_SRC2SEL_DS1,
													 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS);
		}
		
		for (ui32Element = 0; ui32Element < psStream->ui32NumElements; ui32Element++)
		{
			/*
				Get the DMA source address, destination register and size
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
			
			/*
			  Add the vertex stream base address and vertex element offset
			  
			  And DMA the vertex element into the USE primary attribute buffer
			  
			  movsa doutd, ds0[constant_address], ds0[constant_size_and_register], ds1[PDS_DS1_DMA_ADDRESS], ds1[PDS_DS1_DMA_ADDRESS], emit
			*/
			*pui32Instruction++	= PDSEncodeMOVSA	(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MOVS_DEST_DOUTD,
													 EURASIA_PDS_MOVS_SRC1SEL_DS0,
													 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SRC2SEL_DS1,
													 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS / PDS_NUM_DWORDS_PER_REG,
													 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
													 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
		}
	}
	
	/* Set up the vertex ID iteration */
	if (psProgram->bIterateVtxID)
	{
		/* Add the offset if present */
		if (psProgram->ui32VtxIDOffset)
		{
			ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 2);
		
			*pui32Instruction++	= PDSEncodeMOVSA	(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MOVS_DEST_DOUTA,
													 EURASIA_PDS_MOVS_SRC1SEL_REG,
													 EURASIA_PDS_MOVS_SRC1_IR0,
													 EURASIA_PDS_MOVS_SRC2SEL_DS1,
													 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 ((ui32DS1Constant +0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
													 EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 ((ui32DS1Constant +1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
		}
		else
		{
			ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		
			*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MOVS_DEST_DOUTA,
													 EURASIA_PDS_MOVS_SRC1SEL_REG,
													 EURASIA_PDS_MOVS_SRC1_IR0,
													 EURASIA_PDS_MOVS_SRC2SEL_DS1,
													 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 ((ui32DS1Constant +0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
													 EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 ((ui32DS1Constant +0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
		}
	}

	/* and the instance ID */
	if (psProgram->bIterateInstanceID)
	{
		/* Add the offset if present */
		if (psProgram->ui32InstanceIDOffset)
		{
			ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 2);
		
			*pui32Instruction++	= PDSEncodeMOVSA	(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MOVS_DEST_DOUTA,
													 EURASIA_PDS_MOVS_SRC1SEL_REG,
													 EURASIA_PDS_MOVS_SRC1_IR1,
													 EURASIA_PDS_MOVS_SRC2SEL_DS1,
													 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 ((ui32DS1Constant +0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
													 EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 ((ui32DS1Constant +1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
		}
		else
		{
			ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		
			*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MOVS_DEST_DOUTA,
													 EURASIA_PDS_MOVS_SRC1SEL_REG,
													 EURASIA_PDS_MOVS_SRC1_IR1,
													 EURASIA_PDS_MOVS_SRC2SEL_DS1,
													 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 ((ui32DS1Constant +0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
													 EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 ((ui32DS1Constant +0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
		}
	}
	
	/*
		Conditionally issue the task to the USE
		
		(if0) movs doutu, ds0[constant_use], ds0[constant_use], ds1[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_IF0,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}
#endif /* !defined(SUPPORT_SGX_PDS_INDEX_CHECKING) */

/*****************************************************************************
 Function Name	: PDSPatchVertexShaderProgram
 Inputs			: PDSVertexShaderProgramInfo - pointer to the PDS vertex shader program info
				: pui32Buffer		         - pointer to the buffer containing program
 Outputs		: none
 Returns		: Nothing
 Description	: Patches the addresses and use task controls of a pds vertex program
*****************************************************************************/
PDS_CALLCONV IMG_VOID PDSPatchVertexShaderProgram(PPDS_VERTEX_SHADER_PROGRAM_INFO  psPDSVertexShaderProgramInfo,
												  IMG_UINT32                      *pui32Buffer)
{
	IMG_UINT32  i, j;
	
	if(psPDSVertexShaderProgramInfo->bPatchTaskControl)
	{
		/* Write the use task control words */
		for (i = 0; i < PDS_NUM_USE_TASK_CONTROL_WORDS; i++)
		{
			IMG_UINT32 ui32Offset = psPDSVertexShaderProgramInfo->aui32USETaskControlOffsets[i];
			
			pui32Buffer[ui32Offset] = psPDSVertexShaderProgramInfo->aui32USETaskControl[i];
		}
	}
	
	/* Write the stream addresses */
	for (i = 0; i < psPDSVertexShaderProgramInfo->ui32NumStreams; i++)
	{
		/* Need a copy of the address for each element */
		for (j = 0; j < psPDSVertexShaderProgramInfo->aui32NumElements[i]; j++)
		{
			IMG_UINT32 ui32Offset     = psPDSVertexShaderProgramInfo->aui32AddressOffsets[i][j];
			IMG_UINT32 uElementOffset = psPDSVertexShaderProgramInfo->aui32ElementOffsets[i][j];
			
			pui32Buffer[ui32Offset] = psPDSVertexShaderProgramInfo->aui32StreamAddresses[i] + uElementOffset;
		}
	}
}

/*****************************************************************************
 Function Name	: PDSGenerateConstUploadProgram
 Inputs			: psProgram		- pointer to the PDS const upload program
				: pui32Buffer	- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS const upload program. See the definition
				  of sProgram in the header for more details.
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateConstUploadProgram
						(PPDS_CONST_UPLOAD_PROGRAM       psProgram,
						 IMG_UINT32                      *pui32Buffer)
{
	IMG_UINT32 *			pui32Constants;
	IMG_UINT32				ui32NextDS0Constant;
	IMG_UINT32				ui32NextDS1Constant;
	IMG_UINT32				ui32DS0Constant;
	IMG_UINT32				ui32DS1Constant;
	IMG_UINT32				ui32NumConstants;
	IMG_UINT32				ui32Element;
	PPDS_CONST_ELEMENT		psElement;
	IMG_UINT32 *			pui32Instructions;
	IMG_UINT32 *			pui32Instruction;
	
	/*
		This function generates a PDS program to load constant data from either
		constant buffers or memory directly.
	*/
	
	PDS_DPF("PDSGenerateConstUploadProgram()");
	
	/*
		Generate the PDS vertex shader data
	*/
	pui32Constants		= pui32Buffer;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	PDS_ASSERT(psProgram->ui32NumElements < PDS_NUM_CONST_ELEMENTS);
	
	/*
	 * Copy the elements
	*/
	for (ui32Element = 0;
		 ui32Element < psProgram->ui32NumElements;
		 ui32Element++)
	{
		IMG_UINT32 ui32DOutD1;
		IMG_UINT32 ui32DMASize;
		IMG_UINT32 ui32NumLines;
		
		psElement = &psProgram->asElements[ui32Element];
		
		/* Get size of transfer */
		ui32DMASize = psElement->ui32Size;
		ui32NumLines = psElement->ui32Lines;
		
		/*
			Set up the DMA transfer control word.
		*/
#if (EURASIA_PDS_DOUTD1_MAXBURST != EURASIA_PDS_DOUTD1_BSIZE_MAX)
		PDS_ASSERT(ui32DMASize <= EURASIA_PDS_DOUTD1_BSIZE_MAX);
		PDS_ASSERT(ui32NumLines <= EURASIA_PDS_DOUTD1_BLINES_MAX);
		PDS_ASSERT(ui32NumLines > 0);
		PDS_ASSERT(ui32DMASize > 0);
		
		ui32DOutD1 = ((ui32DMASize - 1) << EURASIA_PDS_DOUTD1_BSIZE_SHIFT) |
					(psElement->ui32Register << EURASIA_PDS_DOUTD1_AO_SHIFT);
		ui32DOutD1 |= ((ui32NumLines - 1) << EURASIA_PDS_DOUTD1_BLINES_SHIFT);
		ui32DOutD1 |= ((ui32DMASize - 1) << EURASIA_PDS_DOUTD1_STRIDE_SHIFT);
#else
		PDS_ASSERT((ui32DMASize * ui32NumLines) <= EURASIA_PDS_DOUTD1_BSIZE_MAX);
		ui32DOutD1 = (((ui32DMASize * ui32NumLines) - 1) << EURASIA_PDS_DOUTD1_BSIZE_SHIFT) |
					(psElement->ui32Register << EURASIA_PDS_DOUTD1_AO_SHIFT);
#endif
		
		/*
			Write the dma transfer control words into the PDS data section.
		*/
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		PDSSetDS0Constant(pui32Constants,
						  ui32DS0Constant + 0,
						  psElement->ui32Address);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, ui32DOutD1);
	}
	
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant	= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant	= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS0Constant(pui32Constants,
					  ui32DS0Constant + 0,
					  psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants,
					  ui32DS0Constant + 1,
					  psProgram->aui32USETaskControl[1]);
	PDSSetDS1Constant(pui32Constants,
					  ui32DS1Constant + 0,
					  psProgram->aui32USETaskControl[2]);
					  
	/*
		Get the number of constants
	*/
	ui32NumConstants =
		PDSGetNumConstants(ui32NextDS0Constant, ui32NextDS1Constant);
	ui32NumConstants =
		((ui32NumConstants * sizeof(IMG_UINT32) +
		  ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) &
		 ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
		 
	/*
		Generate the PDS vertex shader code
	*/
	pui32Instructions	= pui32Constants + ui32NumConstants;
	pui32Instruction	= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	for (ui32Element = 0;
		 ui32Element < psProgram->ui32NumElements;
		 ui32Element++)
	{
		/*
			Get the DMA source address, destination register and size
		*/
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		
		/*
			DMA the data to destination register
			
		  movsa doutd, ds0[constant_address], ds0[constant_size_and_register], ds1[PDS_DS1_DMA_ADDRESS], ds1[PDS_DS1_DMA_ADDRESS], emit
		  doutd, ds0[constant_address], ds0[constant_size_and_register]
		*/
		*pui32Instruction++	=
			PDSEncodeMOVS(EURASIA_PDS_CC_ALWAYS,
						  EURASIA_PDS_MOVS_DEST_DOUTD,
						  EURASIA_PDS_MOVS_SRC1SEL_DS0,
						  EURASIA_PDS_MOVS_SRC1_DS0_BASE +
							 ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
						  EURASIA_PDS_MOVS_SRC2SEL_DS1,
						  EURASIA_PDS_MOVS_SRC2_DS1_BASE +
							 PDS_DS1_DMA_ADDRESS / PDS_NUM_DWORDS_PER_REG,
						  ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ?
							 EURASIA_PDS_MOVS_SWIZ_SRC1H :
							 EURASIA_PDS_MOVS_SWIZ_SRC1L,
						  ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ?
							 EURASIA_PDS_MOVS_SWIZ_SRC1H :
							 EURASIA_PDS_MOVS_SWIZ_SRC1L,
						  (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ?
							 EURASIA_PDS_MOVS_SWIZ_SRC2H :
							 EURASIA_PDS_MOVS_SWIZ_SRC2L,
						  (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ?
							 EURASIA_PDS_MOVS_SWIZ_SRC2H :
							 EURASIA_PDS_MOVS_SWIZ_SRC2L);
	}
	
	/*
		Conditionally issue the task to the USE
		
		(if0) movs doutu, ds0[constant_use], ds0[constant_use], ds1[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

#if defined(PDS_BUILD_OPENGLES) || defined(PDS_BUILD_D3DM)
#if !defined(__psp2__)
/*****************************************************************************
 Function Name	: PDSGenerateTerminateStateProgram
 Inputs			: psProgram		- pointer to the PDS terminate state program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS state program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 *PDSGenerateTerminateStateProgram (PDS_TERMINATE_STATE_PROGRAM *psProgram,
														   IMG_UINT32 *pui32Buffer)
{
	IMG_UINT32 *	pui32Constants;
	IMG_UINT32	ui32NextDS0Constant;
	IMG_UINT32	ui32NextDS1Constant;
	IMG_UINT32	ui32DS0Constant;
	IMG_UINT32	ui32DS1Constant;
	IMG_UINT32	ui32NumConstants;
	IMG_UINT32 *	pui32Instructions;
	IMG_UINT32 *	pui32Instruction;
	
	PDS_DPF("PDSGenerateTerminateStateProgram()");
	
	PDS_ASSERT((((IMG_UINT32)pui32Buffer) & ((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1)) == 0);
	
	pui32Constants		= (IMG_UINT32 *) (((IMG_UINTPTR_T)pui32Buffer + ((1UL << EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) - 1)) & ~((IMG_UINTPTR_T)(1UL << EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	/*
		Copy the terminate words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, EURASIA_TACTLPRES_TERMINATE);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, 0 << EURASIA_PDS_DOUTA1_AO_SHIFT);
	
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->ui32TerminateRegion);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, 1 << EURASIA_PDS_DOUTA1_AO_SHIFT);
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
	PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32USETaskControl[2]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant, ui32NextDS1Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS state code
	*/
	pui32Instructions		= pui32Constants + ui32NumConstants;
	pui32Instruction		= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	
	/*
		Copy the state into the primary attributes
	*/
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTA,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 EURASIA_PDS_MOVS_SWIZ_SRC1H,
											 EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTA,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 EURASIA_PDS_MOVS_SWIZ_SRC1H,
											 EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 EURASIA_PDS_MOVS_SWIZ_SRC2L);
#if defined(FIX_HW_BRN_31988)
	/* Repest the last DOUTA twice */
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTA,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 EURASIA_PDS_MOVS_SWIZ_SRC1H,
											 EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 EURASIA_PDS_MOVS_SWIZ_SRC2L);

	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTA,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 EURASIA_PDS_MOVS_SWIZ_SRC1H,
											 EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 EURASIA_PDS_MOVS_SWIZ_SRC2L);

#endif
											 
	/*
		Kick the USE to copy the state from the primary attributes into the output buffer
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}
#endif

/*****************************************************************************
 Function Name	: PDSPatchTerminateStateProgram
 Inputs			: psProgram		- pointer to the PDS terminate state program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: -
 Description	: Patches the terminate region in the PDS state program
*****************************************************************************/

PDS_CALLCONV IMG_VOID PDSPatchTerminateStateProgram (PDS_TERMINATE_STATE_PROGRAM *psProgram,
														IMG_UINT32 *pui32Buffer)
{
	IMG_UINT32 ui32DS0Constant, ui32NextDS0Constant;
	
	PDS_DPF("PDSPatchTerminateStateProgram()");
	
	PDS_ASSERT((((IMG_UINT32)pui32Buffer) & ((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1)) == 0);
	
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	/*
		Copy the terminate words to constants
	*/
	PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	
	PDSSetDS0Constant(pui32Buffer, ui32DS0Constant + 0, psProgram->ui32TerminateRegion);
}


#endif // #if defined(PDS_BUILD_OPENGLES) || defined(PDS_BUILD_D3DM)

#if !defined(PDS_BUILD_OPENGLES)
/*****************************************************************************
 Function Name	: PDSGenerateLoopbackVertexShaderProgram
 Inputs			: psProgram		- pointer to the PDS vertex shader program
				: pui32Buffer	- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS vertex shader program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateLoopbackVertexShaderProgram	(PPDS_VERTEX_SHADER_PROGRAM	psProgram,
																	 IMG_UINT32 *				pui32Buffer)
{
	IMG_UINT32 *			pui32Constants;
	IMG_UINT32				ui32NextDS0Constant;
	IMG_UINT32				ui32NextDS1Constant;
	IMG_UINT32				ui32DS0Constant;
	IMG_UINT32				ui32DS1Constant;
	IMG_UINT32				ui32NumConstants;
	IMG_UINT32				ui32Stream;
	PPDS_VERTEX_STREAM		psStream;
	IMG_UINT32				ui32Element;
	PPDS_VERTEX_ELEMENT		psElement;
	IMG_UINT32 *			pui32Instructions;
	IMG_UINT32 *			pui32Instruction;
	IMG_UINT32				ui32StreamOffset;
	
	PDS_DPF("PDSGenerateLoopbackVertexShaderProgram()");
	
	/*
		Generate the PDS vertex shader data
	*/
	pui32Constants		= pui32Buffer;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	ui32StreamOffset = 0;
	
	for (ui32Stream = 0; ui32Stream < psProgram->ui32NumStreams; ui32Stream++)
	{
		psStream = &psProgram->asStreams[ui32Stream];
		
		/*
			Copy the vertex element address, size and primary attribute register to constants
			
			This version is for fetching tesselator output.
		*/
		for (ui32Element = 0; ui32Element < psStream->ui32NumElements; ui32Element++)
		{
			#if (EURASIA_PDS_DOUTD1_MAXBURST != EURASIA_PDS_DOUTD1_BSIZE_MAX)
			IMG_UINT32	ui32DOutD1;
			#else
			IMG_UINT32	ui32DOutO0;
			#endif
			
			psElement = &psStream->asElements[ui32Element];
			
			#if (EURASIA_PDS_DOUTD1_MAXBURST != EURASIA_PDS_DOUTD1_BSIZE_MAX)
			/*
				Set up the DMA transfer control word.
			*/
			ui32DOutD1	= (3 << EURASIA_PDS_DOUTD1_BSIZE_SHIFT) | // Always 4 dwords.
						  (psElement->ui32Register << EURASIA_PDS_DOUTD1_AO_SHIFT) |
						  EURASIA_PDS_DOUTD1_STYPE;
						  
			/*
				Write the dma transfer control words into the PDS data section.
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, ui32StreamOffset + ui32Element * 4); // our elements are all 4d vectors now.
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, ui32DOutD1);
			#else
			/*
				Set up the DMA transfer control word.
			*/
			PDS_ASSERT((ui32StreamOffset + ui32Element * 4) <= EURASIA_PDS_DOUTO0_SOFF_MAX);
			ui32DOutO0	= (3 << EURASIA_PDS_DOUTO0_BSIZE_SHIFT) | // Always 4 dwords.
						  ((ui32StreamOffset + ui32Element * 4) << EURASIA_PDS_DOUTO0_SOFF_SHIFT) |
						  (psElement->ui32Register << EURASIA_PDS_DOUTO0_AO_SHIFT);
						  
			/*
				Write the dma transfer control words into the PDS data section.
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, ui32DOutO0);
			#endif
		}
		
		ui32StreamOffset += psStream->ui32NumElements * 4;
	}
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
	PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32USETaskControl[2]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants	= PDSGetNumConstants(ui32NextDS0Constant, ui32NextDS1Constant);
	ui32NumConstants	= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS vertex shader code
	*/
	pui32Instructions	= pui32Constants + ui32NumConstants;
	pui32Instruction	= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_SHIFT_DESTSEL_DS1,
											 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
											 EURASIA_PDS_SHIFT_SRCSEL_REG,
											 EURASIA_PDS_SHIFT_SRC_IR0,
											 0);
											 
	for (ui32Stream = 0; ui32Stream < psProgram->ui32NumStreams; ui32Stream++)
	{
		psStream = &psProgram->asStreams[ui32Stream];
		
		for (ui32Element = 0; ui32Element < psStream->ui32NumElements; ui32Element++)
		{
			#if !defined(SGX543)
			/*
				Get the DMA source address, destination register and size
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
			
			if ((ui32Stream == (psProgram->ui32NumStreams - 1)) && (ui32Element == (psStream->ui32NumElements - 1)))
			{
				/*
					mov32 ds1[PDS_DS1_DMA_REGISTER], ds0[constant_size_and_register]
				*/
				*pui32Instruction++	= PDSEncodeMOV32	(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MOV32_DESTSEL_DS1,
														 EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_DMA_REGISTER,
														 EURASIA_PDS_MOV32_SRCSEL_DS0,
														 EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant + 1);
														 
				/*
					Add the vertex stream base address and vertex element offset
					
					And DMA the vertex element into the USE primary attribute buffer
					
					movsa doutd, ds0[constant_address], ds1[PDS_DS1_DMA_REGISTER], ds1[PDS_DS1_DMA_ADDRESS], ds1[PDS_DS1_DMA_ADDRESS], emit
				*/
				*pui32Instruction++	= PDSEncodeMOVSA	(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MOVS_DEST_DOUTD,
														 EURASIA_PDS_MOVS_SRC1SEL_DS0,
														 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
														 EURASIA_PDS_MOVS_SRC2SEL_DS1,
														 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS / PDS_NUM_DWORDS_PER_REG,
														 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
														 (PDS_DS1_DMA_REGISTER % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
														 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
														 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
			}
			else
			{
				/*
					Add the vertex stream base address and vertex element offset
					
					And DMA the vertex element into the USE primary attribute buffer
					
					movsa doutd, ds0[constant_address], ds0[constant_size_and_register], ds1[PDS_DS1_DMA_ADDRESS], ds1[PDS_DS1_DMA_ADDRESS], emit
				*/
				*pui32Instruction++	= PDSEncodeMOVSA	(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MOVS_DEST_DOUTD,
														 EURASIA_PDS_MOVS_SRC1SEL_DS0,
														 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
														 EURASIA_PDS_MOVS_SRC2SEL_DS1,
														 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS / PDS_NUM_DWORDS_PER_REG,
														 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
														 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
														 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
														 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
			}
			#else /* #if (EURASIA_PDS_DOUTD1_MAXBURST == EURASIA_PDS_DOUTD1_BSIZE_MAX) */
			/*
				Get the DMA source address, destination register and size
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
			
			/*
				Add the vertex stream base address and vertex element offset
				
				And DMA the vertex element into the USE primary attribute buffer
				
				movsa douto, ds0[constant_address], ds1[PDS_DS1_DMA_REGISTER], ds1[PDS_DS1_DMA_ADDRESS], ds1[PDS_DS1_DMA_ADDRESS], emit
			*/
			*pui32Instruction++	= PDSEncodeMOVSA	(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MOVS_DEST_DOUTO,
													 EURASIA_PDS_MOVS_SRC1SEL_DS0,
													 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SRC2SEL_DS1,
													 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS / PDS_NUM_DWORDS_PER_REG,
													 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
													 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
													 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
			#endif /* #if (EURASIA_PDS_DOUTD1_MAXBURST != EURASIA_PDS_DOUTD1_BSIZE_MAX) */
		}
	}
	
	/*
		Conditionally issue the task to the USE
		
		(if0) movs doutu, ds0[constant_use], ds0[constant_use], ds1[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

/*****************************************************************************
 Function Name	: PDSGenerateTessVertexShaderProgram
 Inputs			: psProgram		- pointer to the PDS vertex shader program
				: pui32Buffer	- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS vertex shader program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateTessVertexShaderProgram	(PPDS_VERTEX_SHADER_PROGRAM	psProgram,
																 IMG_UINT32					ui32InputVertexStride,
																 IMG_UINT32 *				pui32Buffer,
																 IMG_UINT32					ui32Address)
{
	IMG_UINT32 *			pui32Constants;
	IMG_UINT32				ui32NextDS0Constant;
	IMG_UINT32				ui32NextDS1Constant;
	IMG_UINT32				ui32DS0Constant;
	IMG_UINT32				ui32DS1Constant;
	IMG_UINT32				ui32NumConstants;
	IMG_UINT32				ui32Stream;
	PPDS_VERTEX_STREAM		psStream;
	IMG_UINT32				ui32Element;
	PPDS_VERTEX_ELEMENT		psElement;
	IMG_UINT32 *			pui32Instructions;
	IMG_UINT32 *			pui32Instruction;
	IMG_UINT32				ui32StreamOffset;
	
	PDS_DPF("PDSGenerateTessVertexShaderProgram()");
	
	/*
		Generate the PDS vertex shader data
	*/
	pui32Constants		= pui32Buffer;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, ui32InputVertexStride);
	
	ui32StreamOffset = ui32Address;
	
	for (ui32Stream = 0; ui32Stream < psProgram->ui32NumStreams; ui32Stream++)
	{
		psStream = &psProgram->asStreams[ui32Stream];
		
		/*
			Copy the vertex element address, size and primary attribute register to constants
			
			This version is for fetching tesselator output.
		*/
		for (ui32Element = 0; ui32Element < psStream->ui32NumElements; ui32Element++)
		{
			IMG_UINT32	ui32DOutD1;
			
			psElement = &psStream->asElements[ui32Element];
			
			/*
				Set up the DMA transfer control word.
			*/
			ui32DOutD1	= (3 << EURASIA_PDS_DOUTD1_BSIZE_SHIFT) | // Always 4 dwords.
						  (psElement->ui32Register << EURASIA_PDS_DOUTD1_AO_SHIFT) |
						  (1UL << EURASIA_PDS_DOUTD1_INSTR_SHIFT); // Bypass the cache
						  
			/*
				Write the dma transfer control words into the PDS data section.
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, ui32StreamOffset + psElement->ui32Offset);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, ui32DOutD1);
		}
	}
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
	PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32USETaskControl[2]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants	= PDSGetNumConstants(ui32NextDS0Constant, ui32NextDS1Constant);
	ui32NumConstants	= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS vertex shader code
	*/
	pui32Instructions	= pui32Constants + ui32NumConstants;
	pui32Instruction	= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	
	
	*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MUL_DESTSEL_DS1,
											 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
											 EURASIA_PDS_MUL_SRC1SEL_REG,
											 EURASIA_PDS_MUL_SRC1_IR0L,
											 EURASIA_PDS_MUL_SRC2SEL_DS1,
											 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD);
											 
	for (ui32Stream = 0; ui32Stream < psProgram->ui32NumStreams; ui32Stream++)
	{
		psStream = &psProgram->asStreams[ui32Stream];
		
		for (ui32Element = 0; ui32Element < psStream->ui32NumElements; ui32Element++)
		{
			/*
				Get the DMA source address, destination register and size
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
			
			/*
				Add the vertex stream base address and vertex element offset
				
				And DMA the vertex element into the USE primary attribute buffer
				
				movsa doutd, ds0[constant_address], ds1[PDS_DS1_DMA_REGISTER], ds1[PDS_DS1_DMA_ADDRESS], ds1[PDS_DS1_DMA_ADDRESS], emit
			*/
			
			*pui32Instruction++	= PDSEncodeMOVSA	(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MOVS_DEST_DOUTD,
													 EURASIA_PDS_MOVS_SRC1SEL_DS0,
													 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SRC2SEL_DS1,
													 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS / PDS_NUM_DWORDS_PER_REG,
													 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
													 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
		}
	}
	
	/*
		Conditionally issue the task to the USE
		
		(if0) movs doutu, ds0[constant_use], ds0[constant_use], ds1[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_IF0,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

/*****************************************************************************
 Function Name	: PDSGenerateTessSAProgram
 Inputs			: psProgram		- pointer to the PDS tesselaotr secondary attributes program
				: pui32Buffer	- pointer to the buffer for the program
				: bShadowSAs	- Copy our control stream object into the shadow.
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS tesselator seconary attributes program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateTessSAProgram	(PPDS_TESSELATOR_SA_PROGRAM	psProgram,
													 IMG_UINT32 *							pui32Buffer,
													 IMG_BOOL	bShadowSAs)
{
	IMG_UINT32 *	pui32Constants;
	IMG_UINT32	ui32NextDS0Constant;
	IMG_UINT32	ui32NextDS1Constant;
	IMG_UINT32	ui32DS0Constant;
	IMG_UINT32	ui32DS1Constant;
	IMG_UINT32	ui32NumConstants;
	IMG_UINT32 *	pui32Instructions;
	IMG_UINT32 *	pui32Instruction;
	
	PDS_DPF("PDSGenerateTessSAProgram()");
	
	/*
		Generate the PDS tesselator secondary attributes data
	*/
	pui32Constants		= (IMG_UINT32 *) (((IMG_UINTPTR_T)pui32Buffer + ((1UL << EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) - 1)) & ~((IMG_UINTPTR_T)(1UL << EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	/*
		Copy the DMA control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32DMAControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32DMAControl[1]);
	
	if (psProgram->ui32NumDMAKicks == 2)
	{
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 2);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32DMAControl[2]);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 1, psProgram->aui32DMAControl[3]);
	}
	
	if(bShadowSAs)
	{
		/*
			Copy the USE task control words to constants
		*/
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32USETaskControl[2]);
	}
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant, ui32NextDS1Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS tesselator secondary attributes code
	*/
	pui32Instructions		= pui32Constants + ui32NumConstants;
	pui32Instruction		= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	/*
		DMA the state into the secondary attributes
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTD,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 EURASIA_PDS_MOVS_SWIZ_SRC1H,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 EURASIA_PDS_MOVS_SWIZ_SRC1H);
											 
	if (psProgram->ui32NumDMAKicks == 2)
	{
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 2);
		
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTD,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + PDS_DS0_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 EURASIA_PDS_MOVS_SWIZ_SRC2H,
												 EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 EURASIA_PDS_MOVS_SWIZ_SRC2H);
	}
	
	if(bShadowSAs)
	{
		/*
			Issue the task to the USE...
			
			(if0) movs doutu, ds0[constant_use], ds0[constant_use], ds1[constant_use], ds1[constant_use], emit
		*/
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTU,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
												 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
	}
	
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

/*****************************************************************************
 Function Name	: PDSGenerateTessIndicesProgram
 Inputs			: psProgram		- pointer to the PDS tesselator program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS program to load indices into the tesselator
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateTessIndicesProgram	(PPDS_TESSELATOR_PROGRAM	psProgram,
														 IMG_UINT32 *						pui32Buffer)
{
	IMG_UINT32 *					pui32Constants;
	IMG_UINT32					ui32NextDS0Constant;
	IMG_UINT32					ui32NextDS1Constant;
	IMG_UINT32					ui32DS0Constant;
	IMG_UINT32					ui32DS1Constant;
	IMG_UINT32					ui32NumConstants;
	IMG_UINT32 *					pui32Instructions;
	IMG_UINT32 *					pui32Instruction;
	
	/*
		This function is only interested in loading indices from the VDM into the USE PA buffer...
	*/
	
	PDS_DPF("PDSGenerateTessIndicesProgram()");
	
	pui32Constants = (IMG_UINT32 *) (((IMG_UINTPTR_T)pui32Buffer + ((1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	#if EURASIA_USE_COMPLX_PADATAOFFSET
	/*
		Set up the register offset...
	*/
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, EURASIA_USE_COMPLX_PADATAOFFSET);
	#endif
	
#if defined(SGX_FEATURE_PDS_EXTENDED_INPUT_REGISTERS)
	if (psProgram->bIteratePrimID)
	{
		/* Set up the next DS0 constant with a value of 4 (increment of the
		 * douta address for the Primitive ID iteration) */
		ui32DS0Constant = PDSGetConstants(&ui32NextDS0Constant, 1);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0,
						  1 << EURASIA_PDS_DOUTA1_AO_SHIFT);
	}
#endif

	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControlDummy[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControlDummy[1]);
	PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32USETaskControlDummy[2]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant, ui32NextDS1Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS tesselator code
	*/
	pui32Instructions	= pui32Constants + ui32NumConstants;
	pui32Instruction		= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	
	/*
		Get the external dependency flag
	*/
	
	#if EURASIA_USE_COMPLX_PADATAOFFSET
	/*
		add ds0[PDS_DS0_INDEX], ir1, (EURASIA_USE_COMPLX_PADATAOFFSET)
	*/
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_ARITH_DESTSEL_DS0,
											 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS0_INDEX,
											 EURASIA_PDS_ARITH_SRC1SEL_REG,
											 EURASIA_PDS_ARITH_SRC1_IR1,
											 EURASIA_PDS_ARITH_SRC2SEL_DS1,
											 EURASIA_PDS_ARITH_SRC2_DS1_BASE + ui32DS1Constant);
											 
	/*
		shl ds1[PDS_DS1_INDEX_PART], ds0[PDS_DS0_INDEX], (EURASIA_PDS_DOUTA1_AO_SHIFT)
	*/
	*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_SHIFT_DESTSEL_DS1,
											 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS1_INDEX_PART,
											 EURASIA_PDS_SHIFT_SRCSEL_DS0,
											 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS0_INDEX,
											 EURASIA_PDS_SHIFT_SHIFT_ZERO + (EURASIA_PDS_DOUTA1_AO_SHIFT));
	#else
	/*
		shl ds1[PDS_DS1_INDEX_PART], ir1, (EURASIA_PDS_DOUTA1_AO_SHIFT)
	*/
	*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_SHIFT_DESTSEL_DS1,
											 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS1_INDEX_PART,
											 EURASIA_PDS_SHIFT_SRCSEL_REG,
											 EURASIA_PDS_SHIFT_SRC_IR1,
											 EURASIA_PDS_SHIFT_SHIFT_ZERO + (EURASIA_PDS_DOUTA1_AO_SHIFT));
	#endif
	
	/*
		Copy the indices into the USE primary attribute buffer
		
		movs douta, ir0, ds1[PDS_DS1_BASE], emit
	*/
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTA,
											 EURASIA_PDS_MOVS_SRC1SEL_REG,
											 EURASIA_PDS_MOVS_SRC1_IR0,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_INDEX_PART / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
#if defined(SGX_FEATURE_PDS_EXTENDED_INPUT_REGISTERS)
	if (psProgram->bIteratePrimID)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
		/*
		 * Add 4 to the address to do the douta
		 * add ds1[PDS_DS1_BASE], ds0[constant], ds1[PDS_DS1_BASE]
		 */
		*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_ARITH_DESTSEL_DS1,
												 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS1_INDEX_PART,
												 EURASIA_PDS_ARITH_SRC1SEL_DS0,
												 EURASIA_PDS_ARITH_SRC1_DS0_BASE + ui32DS0Constant,
												 EURASIA_PDS_ARITH_SRC2SEL_DS1,
												 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_INDEX_PART);
												 
		/*
		 * Copy the result to the primary attribute buffer
		 * movs douta, ir1, ds1[PDS_DS1_BASE], emit
		 */
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTA,
												 EURASIA_PDS_MOVS_SRC1SEL_REG,
												 EURASIA_PDS_MOVS_SRC1_IR2,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_INDEX_PART / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC2L);
	}
#endif

	/*
		Issue the dummy task to the USE
		
		movs doutu, ds0[constant_use], ds1[constant_use], ds0[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	
	#if defined(SGX545) || defined(SGX543) || defined(SGX544) || defined(SGX554)
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_IF1,
	#else
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
	#endif
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
	
	/*
		End the program
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32IndicesDataSegment	= pui32Constants;
	psProgram->ui32DataSize				= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

#if defined(SGX545)
/*****************************************************************************
 Function Name	: PDSGenerateSCWordCopyProgram
 Inputs			: psProgram		- pointer to the PDS program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS program to load indices into the tesselator
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateSCWordCopyProgram	(PPDS_SCWORDCOPY_PROGRAM	psProgram,
														 IMG_UINT32 *						pui32Buffer)
{
	IMG_UINT32 *					pui32Constants;
	IMG_UINT32					ui32NextDS0Constant;
	IMG_UINT32					ui32NextDS1Constant;
	IMG_UINT32					ui32DS0Constant;
	IMG_UINT32					ui32DS1Constant;
	IMG_UINT32					ui32NumConstants;
	IMG_UINT32 *					pui32Instructions;
	IMG_UINT32 *					pui32Instruction;
	
	/*
		This function is only interested in loading the shared comms word from the VDM into the USE PA buffer...
	*/
	
	PDS_DPF("PDSGenerateSCWordCopyProgram()");
	
	pui32Constants = (IMG_UINT32 *) (((IMG_UINTPTR_T)pui32Buffer + ((1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
	PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32USETaskControl[2]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant, ui32NextDS1Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS code
	*/
	pui32Instructions	= pui32Constants + ui32NumConstants;
	pui32Instruction		= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	/*
		shl ds1[PDS_DS1_INDEX_PART], ir1, (EURASIA_PDS_DOUTA1_AO_SHIFT)
	*/
	*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_SHIFT_DESTSEL_DS1,
											 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS1_INDEX_PART,
											 EURASIA_PDS_SHIFT_SRCSEL_REG,
											 EURASIA_PDS_SHIFT_SRC_IR1,
											 EURASIA_PDS_SHIFT_SHIFT_ZERO + (EURASIA_PDS_DOUTA1_AO_SHIFT));
											 
	/*
		Copy the shared comms word into the USE primary attribute buffer
		
		movs douta, ir0, ds1[PDS_DS1_BASE], emit
	*/
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTA,
											 EURASIA_PDS_MOVS_SRC1SEL_REG,
											 EURASIA_PDS_MOVS_SRC1_IR0,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_INDEX_PART / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 EURASIA_PDS_MOVS_SWIZ_SRC2L);
	
	/*
		Issue the dummy task to the USE
		
		movs doutu, ds0[constant_use], ds1[constant_use], ds0[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_IF1,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
	
	/*
		End the program
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

/*****************************************************************************
 Function Name	: PDSGenerateITPProgram
 Inputs			: psProgram		- pointer to the PDS program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS program to perform the ITP state update on all pipes.
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateITPProgram	(PPDS_ITP_PROGRAM	psProgram,
							 IMG_UINT32 *		pui32Buffer)
{
	IMG_UINT32 *					pui32Constants;
	IMG_UINT32					ui32NextDS0Constant;
	IMG_UINT32					ui32NextDS1Constant;
	IMG_UINT32					ui32DS0Constant;
	IMG_UINT32					ui32DS1Constant;
	IMG_UINT32					ui32NumConstants;
	IMG_UINT32 *					pui32Instructions;
	IMG_UINT32 *					pui32Instruction;
	
	PDS_DPF("PDSGenerateITPProgram()");
	
	pui32Constants = (IMG_UINT32 *) (((IMG_UINTPTR_T)pui32Buffer + ((1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
	PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32USETaskControl[2]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant, ui32NextDS1Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS tesselator code
	*/
	pui32Instructions	= pui32Constants + ui32NumConstants;
	pui32Instruction	= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	/*
		Issue the dummy task to the USE
		
		movs doutu, ds0[constant_use], ds1[constant_use], ds0[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	
	
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment		= pui32Constants;
	psProgram->ui32DataSize			= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

/*****************************************************************************
 Function Name	: PDSGenerateVtxBufWritePointerProgram
 Inputs			: psProgram		- pointer to the PDS program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS program to perform the VtxBufWritePointer state update.
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateVtxBufWritePointerProgram	(PPDS_VTXBUFWRITEPOINTER_PROGRAM	psProgram,
							 IMG_UINT32 *		pui32Buffer)
{
	IMG_UINT32 *					pui32Constants;
	IMG_UINT32					ui32NextDS0Constant;
	IMG_UINT32					ui32NextDS1Constant;
	IMG_UINT32					ui32DS0Constant;
	IMG_UINT32					ui32DS1Constant;
	IMG_UINT32					ui32NumConstants;
	IMG_UINT32 *					pui32Instructions;
	IMG_UINT32 *					pui32Instruction;
	
	PDS_DPF("PDSGenerateVtxBufWritePointerProgram()");
	
	pui32Constants = (IMG_UINT32 *) (((IMG_UINTPTR_T)pui32Buffer + ((1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	/*
		Copy the DOUTA control words...
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, (0 << EURASIA_PDS_DOUTA1_AO_SHIFT));
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
	PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32USETaskControl[2]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant, ui32NextDS1Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS tesselator code
	*/
	pui32Instructions	= pui32Constants + ui32NumConstants;
	pui32Instruction	= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	/*
		Copy the vertex write pointer into the USE primary attribute buffer
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
	
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTA,
											 EURASIA_PDS_MOVS_SRC1SEL_REG,
											 EURASIA_PDS_MOVS_SRC1_IR0,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 (ui32DS0Constant % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 (ui32DS0Constant % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		Issue the dummy task to the USE
		
		movs doutu, ds0[constant_use], ds1[constant_use], ds0[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment		= pui32Constants;
	psProgram->ui32DataSize			= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

/*****************************************************************************
 Function Name	: PDSGenerateTessPhase2Program
 Inputs			: psProgram		- pointer to the PDS tesselator program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS tesselator program for phase 2
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateTessPhase2Program	(PPDS_TESSELATOR_PROGRAM	psProgram,
							 IMG_UINT32 *			pui32Buffer)
{
	IMG_UINT32 *					pui32Constants;
	IMG_UINT32					ui32NextDS0Constant;
	IMG_UINT32					ui32NextDS1Constant;
	IMG_UINT32					ui32DS0Constant;
	IMG_UINT32					ui32DS1Constant;
	IMG_UINT32					ui32NumConstants;
	IMG_UINT32 *					pui32Instructions;
	IMG_UINT32 *					pui32Instruction;
	IMG_UINT32						ui32VtxOffset = 0;
	
	PDS_DPF("PDSGenerateTessPhase2Program()");
	
#if defined(SGX_FEATURE_STREAM_OUTPUT)
	if (psProgram->bIterateVtxID)
	{
		ui32VtxOffset = 1;
	}
#endif

	/*
		Generate the PDS tesselator data
	*/
	pui32Constants		= (IMG_UINT32 *) (((IMG_UINTPTR_T)pui32Buffer + ((1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1)) & ~((IMG_UINTPTR_T)(1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
#if defined(SGX_FEATURE_STREAM_OUTPUT)
	if (psProgram->bIterateVtxID)
	{
		/*
		  Copy the VtxID douta words.  This will always go in pa0
		*/
		ui32DS1Constant = PDSGetConstants(&ui32NextDS1Constant, 2);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0,
						  (0 << EURASIA_PDS_DOUTA1_AO_SHIFT));
	}
#endif

	/* We should only ever be allowed up to 2 DMA kicks here */
	PDS_ASSERT(psProgram->ui32NumDMAKicks < 3);

	/*
		Copy the DMA control words to constants..
	*/
	if (psProgram->ui32NumDMAKicks > 0)
	{
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 2);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0,
						  psProgram->aui32DMAControl[1]);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 1,
						  0);
	}
	if (psProgram->ui32NumDMAKicks > 1)
	{
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 2);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0,
						  psProgram->aui32DMAControl[3]);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 1,
						  psProgram->aui32DMAControl[2]);
	}

	/*
		Copy the USE task control words to constants (for first task)
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControlPhase2[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControlPhase2[1]);
	PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32USETaskControlPhase2[2]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant, ui32NextDS1Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS tesselator code
	*/
	pui32Instructions		= pui32Constants + ui32NumConstants;
	pui32Instruction		= pui32Instructions;
	ui32NextDS0Constant		= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant		= PDS_DS1_CONST_BLOCK_BASE;
	
#if defined(SGX_FEATURE_STREAM_OUTPUT)
	if (psProgram->bIterateVtxID)
	{
		/*
		 * Do the VtxID iteration first:
		 * shr DS0[COUNTER], IR0, #4
		 * movs douta, DS0[COUNTER], DS1[constant_vtx], emit
		 */
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 2);
		
		*pui32Instruction++	= PDSEncodeSHR		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_SHIFT_DESTSEL_DS0,
												 EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS0_TESS_COUNTER,
												 EURASIA_PDS_SHIFT_SRCSEL_REG,
												 EURASIA_PDS_SHIFT_SRC_IR0,
												 EURASIA_PDS_SHIFT_SHIFT_ZERO + 4);
												 
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTA,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + PDS_DS0_TESS_COUNTER / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H,
												 EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H,
												 EURASIA_PDS_MOVS_SWIZ_SRC2L);
	}
	
#endif

	/*
		Get the source address from the input register....
		
		mov32 ds0[address], ir1
	*/
	*pui32Instruction++	= PDSEncodeMOV32	(EURASIA_PDS_CC_ALWAYS,
							 EURASIA_PDS_MOV32_DESTSEL_DS0,
							 EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS0_TESS_SYNC,
							 EURASIA_PDS_MOV32_SRCSEL_REG,
							 EURASIA_PDS_MOV32_SRC_IR1);
							 
	if (psProgram->ui32NumDMAKicks > 0)
	{
		/*
		  Copy the vertex from the address specified in IR1 to the primary attributes
		  
		  movsa doutd, ds0[address], ds0[constant_size_and_register], ds1[PDS_DS1_DMA_ADDRESS], ds1[PDS_DS1_DMA_ADDRESS]
		*/
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 2);
		*pui32Instruction++	= PDSEncodeMOVSA	(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTD,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + PDS_DS0_TESS_SYNC / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
												 (PDS_DS0_TESS_SYNC % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 (PDS_DS0_TESS_SYNC % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 ((ui32DS1Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
	}
	if (psProgram->ui32NumDMAKicks > 1)
	{
		/*
		  Copy the vertex from the address specified in IR1 to the primary attributes
		  
		  movsa doutd, ds0[address], ds0[constant_size_and_register], ds1[PDS_DS1_DMA_ADDRESS], ds1[PDS_DS1_DMA_ADDRESS]
		*/
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 2);
		*pui32Instruction++	= PDSEncodeMOVSA	(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTD,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + PDS_DS0_TESS_SYNC / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
												 (PDS_DS0_TESS_SYNC % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 (PDS_DS0_TESS_SYNC % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 ((ui32DS1Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
	}
								
	/*
		Conditionaly issue the task to the USE
		
		(if0) movs doutu, ds0[constant_use], ds1[constant_use], ds0[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_IF0,
							 EURASIA_PDS_MOVS_DEST_DOUTU,
							 EURASIA_PDS_MOVS_SRC1SEL_DS0,
							 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
							 EURASIA_PDS_MOVS_SRC2SEL_DS1,
							 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
							 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
							 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
							 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
							 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
							 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32Phase2DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

/*****************************************************************************
 Function Name	: PDSGenerateTessProgram
 Inputs			: psProgram		- pointer to the PDS tesselator program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS tesselator program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateTessProgram	(PPDS_TESSELATOR_PROGRAM	psProgram,
													 IMG_UINT32 *				pui32Buffer)
{
	IMG_UINT32 *					pui32Constants;
	IMG_UINT32						ui32NextDS0Constant;
	IMG_UINT32						ui32NextDS1Constant;
	IMG_UINT32						ui32DS0Constant;
	IMG_UINT32						ui32DS1Constant;
	IMG_UINT32						ui32NumConstants;
	IMG_UINT32 *					pui32Instructions;
	IMG_UINT32 *					pui32Instruction;
	
	PDS_DPF("PDSGenerateTessProgram()");
	
	/*
		Generate the PDS tesselator data
	*/
	pui32Constants		= (IMG_UINT32 *) (((IMG_UINTPTR_T)pui32Buffer + ((1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1)) & ~((IMG_UINTPTR_T)(1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	/*
		Copy the USE task control words to constants (for first task)
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControlTess[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControlTess[1]);
	PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32USETaskControlTess[2]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant, ui32NextDS1Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS tesselator code
	*/
	pui32Instructions		= pui32Constants + ui32NumConstants;
	pui32Instruction		= pui32Instructions;
	ui32NextDS0Constant		= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant		= PDS_DS1_CONST_BLOCK_BASE;
	
	
	/*
		Issue the task to the USE
		
		movs doutu, ds0[constant_use], ds1[constant_use], ds0[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

#else /* #if defined(SGX545) */

/*****************************************************************************
 Function Name	: PDSGenerateTessProgram
 Inputs			: psProgram		- pointer to the PDS tesselator program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS tesselator program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateTessProgram	(PPDS_TESSELATOR_PROGRAM	psProgram,
													 IMG_UINT32 *							pui32Buffer)
{
	IMG_UINT32 *					pui32Constants;
	IMG_UINT32					ui32NextDS0Constant;
	IMG_UINT32					ui32NextDS1Constant;
	IMG_UINT32					ui32DS0Constant;
	IMG_UINT32					ui32DS1Constant;
	IMG_UINT32					ui32NumConstants;
	IMG_UINT32 *					pui32Instructions;
	IMG_UINT32 *					pui32Instruction;
	
	PDS_DPF("PDSGenerateTessProgram()");
	
	/*
		Generate the PDS tesselator data
	*/
	pui32Constants		= (IMG_UINT32 *) (((IMG_UINTPTR_T)pui32Buffer + ((1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1)) & ~((IMG_UINTPTR_T)(1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	#if defined(SGX541) || defined(SGX543) || defined(SGX544) || defined(SGX554)
	{
		/*
			On this core we pass the output primitive ID to the USSE via PA0..
		*/
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, 0 << EURASIA_PDS_DOUTA1_AO_SHIFT);
		
		#if defined(SGX543) || defined(SGX544) || defined(SGX554)
		/*
			On this core we pass the input primitive ID to the USSE via PA1..
		*/
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, 1 << EURASIA_PDS_DOUTA1_AO_SHIFT);
		#endif
	}
	#endif
	
	#if !defined(SGX543) && !defined(SGX544) && !defined(SGX554)
	/*
		Copy the USE task control words to constants (for first task)
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControlTessFirst[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControlTessFirst[1]);
	PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32USETaskControlTessFirst[2]);
	
	/*
		Copy the flag that will indicate the first task has been sent.
	*/
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, 0xFFFFFFFF);
	#endif
	
	/*
		Copy the USE task control words to constants (for all other tasks)
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControlTessOther[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControlTessOther[1]);
	PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32USETaskControlTessOther[2]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant, ui32NextDS1Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS tesselator code
	*/
	pui32Instructions		= pui32Constants + ui32NumConstants;
	pui32Instruction		= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	#if defined(SGX541) || defined(SGX543) || defined(SGX544) || defined(SGX554)
	/*
		Copy the output primitive ID into the first USE PA register.
		
		movs douta, ir0, ds1[PDS_DS1_BASE], emit
	*/
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTA,
											 EURASIA_PDS_MOVS_SRC1SEL_REG,
											 EURASIA_PDS_MOVS_SRC1_IR0,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
	
	#if defined(SGX543) || defined(SGX544) || defined(SGX554)
	/*
		Copy the input primitive ID into the second USE PA register.
		
		movs douta, ir1, ds1[PDS_DS1_BASE], emit
	*/
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTA,
											 EURASIA_PDS_MOVS_SRC1SEL_REG,
											 EURASIA_PDS_MOVS_SRC1_IR1,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
	#endif
	#endif
	
	#if !defined(SGX543) && !defined(SGX544) && !defined(SGX554)
	/*
		Look for the first task to get here...
	*/
	*pui32Instruction++	= PDSEncodeTSTN		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_TST_DEST_P0,
											 EURASIA_PDS_TST_SRCSEL_SRC1,
											 EURASIA_PDS_TST_SRC1SEL_DS0,
											 PDS_DS0_TESS_SYNC,
											 0);
	/*
		Branch if not first task
	*/
	*pui32Instruction	= (IMG_UINT32)PDSEncodeBRA(EURASIA_PDS_CC_P0,
											 (IMG_UINT32)(((pui32Instruction - pui32Instructions) + 4) << EURASIA_PDS_FLOW_DEST_ALIGNSHIFT));
	pui32Instruction++;
	
	/*
		Issue the task to the USE (first task only)
		
		movs doutu, ds0[constant_use], ds1[constant_use], ds0[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		Set the flag.
		
		mov32 PDS_DS0_TESS_SYNC, #FFFFFFFF
	*/
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	*pui32Instruction++	= PDSEncodeMOV32	(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MOV32_DESTSEL_DS0,
														 EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS0_TESS_SYNC,
														 EURASIA_PDS_MOV32_SRCSEL_DS1,
														 EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS1Constant);
														 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	#endif /* !defined(SGX543) && !defined(SGX544) && !defined(SGX554) */
	
	/*
		Issue the task to the USE (All other tasks)
		
		movs doutu, ds0[constant_use], ds1[constant_use], ds0[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}
#endif /* #if defined(SGX545) */

/*****************************************************************************
 Function Name	: PDSGenerateTessVertexProgram
 Inputs			: psProgram		- pointer to the PDS teselator program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS tess vertex load program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateTessVertexProgram	(PPDS_TESSELATOR_PROGRAM	psProgram,
														 IMG_UINT32 *					pui32Buffer)
{
	IMG_UINT32 *			pui32Constants;
	IMG_UINT32				ui32NextDS0Constant;
	IMG_UINT32				ui32NextDS1Constant;
	IMG_UINT32				ui32DS0Constant;
	IMG_UINT32				ui32DS1Constant;
	IMG_UINT32				ui32NumConstants;
	IMG_UINT32				ui32Stream;
	PPDS_VERTEX_STREAM		psStream;
	IMG_UINT32				ui32Element;
	PPDS_VERTEX_ELEMENT		psElement;
	IMG_UINT32 *			pui32Instructions;
	IMG_UINT32 *			pui32Instruction;
	
	/*
		To load a typical set of vertex stream elements
		
		For each stream
		{
			Take the 24-bit index or the 24-bit instance
			Divide by the frequency if necessary
			Multiply by the stream stride
			For each element
			{
				Add the stream base address and the element offset
				DMA the element into the primray attribute buffer
			}
		}
		
		The straight-line code is organised to never re-visit constants, which are pre-computed into the program data
		
		The USE pre-processing program should unpack and pad the elements with 0s and 1s as required
	*/
	
	PDS_DPF("PDSGenerateTessVertexProgram()");
	
	/*
		Generate the PDS tesselator data
	*/
	pui32Constants		= (IMG_UINT32 *) (((IMG_UINTPTR_T)pui32Buffer + ((1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1)) & ~((IMG_UINTPTR_T)(1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	for (ui32Stream = 0; ui32Stream < psProgram->ui32NumStreams; ui32Stream++)
	{
		psStream = &psProgram->asStreams[ui32Stream];
		
		/*
			Copy the vertex stream frequency multiplier to a constant if necessary
		*/
		if (psStream->ui32Multiplier)
		{
			ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
			PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psStream->ui32Multiplier | 0x01000000);
		}
		
		/*
			Copy the vertex stream stride to a constant
		*/
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psStream->ui32Stride);
		
		/*
			Copy the vertex stream element address, size and primary attribute register to constants
		*/
		for (ui32Element = 0; ui32Element < psStream->ui32NumElements; ui32Element++)
		{
			IMG_UINT32	ui32DOutD1;
			IMG_UINT32 ui32DMASize;
			
			psElement = &psStream->asElements[ui32Element];
			
			/* Size is in bytes - round up to nearest 32 bit word */
			ui32DMASize = (psElement->ui32Size + 3) >> 2;
			
			/*
				Set up the DMA transfer control word.
			*/
			PDS_ASSERT(ui32DMASize <= EURASIA_PDS_DOUTD1_BSIZE_MAX);
			
			ui32DOutD1	= ((ui32DMASize - 1)	<< EURASIA_PDS_DOUTD1_BSIZE_SHIFT) |
						  ((psElement->ui32Register + EURASIA_USE_COMPLX_PADATAOFFSET) << EURASIA_PDS_DOUTD1_AO_SHIFT);
						  
			/*
				Write the dma transfer control words into the PDS data section.
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psStream->ui32Address + psElement->ui32Offset);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, ui32DOutD1);
		}
	}
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControlVertLoad[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControlVertLoad[1]);
	PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32USETaskControlVertLoad[2]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants	= PDSGetNumConstants(ui32NextDS0Constant, ui32NextDS1Constant);
	ui32NumConstants	= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS tesselator code
	*/
	pui32Instructions	= pui32Constants + ui32NumConstants;
	pui32Instruction	= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	for (ui32Stream = 0; ui32Stream < psProgram->ui32NumStreams; ui32Stream++)
	{
		psStream = &psProgram->asStreams[ui32Stream];
		
		/*
			Divide by the frequency if necessary
		*/
		if (psStream->ui32Multiplier)
		{
			/*
				mul ds0[PDS_DS0_INDEX], ir0/1.l, ds1[constant_multiplier].l
				mul ds1[PDS_DS1_INDEX_PART], ir0/1.l, ds1[constant_multiplier].h
				shl ds1[PDS_DS1_INDEX_PART], ds1[PDS_DS1_INDEX_PART], 16
			*/
			ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
			
			*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MUL_DESTSEL_DS0,
													 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_INDEX,
													 EURASIA_PDS_MUL_SRC1SEL_REG,
													 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1L : EURASIA_PDS_MUL_SRC1_IR0L,
													 EURASIA_PDS_MUL_SRC2SEL_DS1,
													 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD);
													 
			*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MUL_DESTSEL_DS1,
													 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_INDEX_PART,
													 EURASIA_PDS_MUL_SRC1SEL_REG,
													 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1L : EURASIA_PDS_MUL_SRC1_IR0L,
													 EURASIA_PDS_MUL_SRC2SEL_DS1,
													 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD + 1);
													 
			*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_SHIFT_DESTSEL_DS1,
													 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS1_INDEX_PART,
													 EURASIA_PDS_SHIFT_SRCSEL_DS1,
													 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS1_INDEX_PART,
													 EURASIA_PDS_SHIFT_SHIFT_ZERO + 16);
													 
			if ((!psStream->bInstanceData && !psProgram->b32BitIndices) ||
				(psStream->bInstanceData && (psProgram->ui32NumInstances <= 0x10000)))
			{
				/*
					add ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], ds1[PDS_DS1_INDEX_PART]
				*/
				*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_ARITH_DESTSEL_DS0,
														 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC1SEL_DS0,
														 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC2SEL_DS1,
														 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_INDEX_PART);
			}
			else
			{
				/*
					add ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], ds1[PDS_DS1_INDEX_PART]
					mul ds1[PDS_DS1_INDEX_PART], ir0/1.h, ds1[const_multiplier].l
					add ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], ds1[PDS_DS1_INDEX_PART]
					mul ds1[PDS_DS1_INDEX_PART], ir0/1.h, ds1[const_multiplier].h
					shl ds1[PDS_DS1_INDEX_PART], ds1[PDS_DS1_INDEX_PART], 16
					add ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], ds1[PDS_DS1_INDEX_PART]
				*/
				*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_ARITH_DESTSEL_DS0,
														 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC1SEL_DS0,
														 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC2SEL_DS1,
														 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_INDEX_PART);
														 
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS1,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_INDEX_PART,
														 EURASIA_PDS_MUL_SRC1SEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1H : EURASIA_PDS_MUL_SRC1_IR0H,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD);
														 
				*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_ARITH_DESTSEL_DS0,
														 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC1SEL_DS0,
														 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC2SEL_DS1,
														 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_INDEX_PART);
														 
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS1,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_INDEX_PART,
														 EURASIA_PDS_MUL_SRC1SEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1H : EURASIA_PDS_MUL_SRC1_IR0H,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD + 1);
														 
				*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_SHIFT_DESTSEL_DS1,
														 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS1_INDEX_PART,
														 EURASIA_PDS_SHIFT_SRCSEL_DS1,
														 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS1_INDEX_PART,
														 EURASIA_PDS_SHIFT_SHIFT_ZERO + 16);
														 
				*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_ARITH_DESTSEL_DS0,
														 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC1SEL_DS0,
														 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC2SEL_DS1,
														 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_INDEX_PART);
			}
			
			if (psStream->ui32Shift)
			{
				/*
					shr ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], shift
				*/
				*pui32Instruction++	= PDSEncodeSHR		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_SHIFT_DESTSEL_DS0,
														 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_SHIFT_SRCSEL_DS0,
														 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_SHIFT_SHIFT_ZERO + psStream->ui32Shift + 24);
			}
		}
		else
		{
			if (psStream->ui32Shift)
			{
				/*
					shr ds0[PDS_DS0_INDEX], ir0/1, shift
				*/
				*pui32Instruction++	= PDSEncodeSHR		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_SHIFT_DESTSEL_DS0,
														 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_SHIFT_SRCSEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_SHIFT_SRC_IR1 : EURASIA_PDS_SHIFT_SRC_IR0,
														 EURASIA_PDS_SHIFT_SHIFT_ZERO + psStream->ui32Shift);
			}
		}
		
		/*
			Multiply by the vertex stream stride
			
			mul ds1[PDS_DS1_DMA_ADDRESS], ds0[PDS_DS0_INDEX].l, ds1[constant_stride].l
		*/
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		
		if (psStream->ui32Multiplier || psStream->ui32Shift)
		{
			*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MUL_DESTSEL_DS1,
													 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_MUL_SRC1SEL_DS0,
													 EURASIA_PDS_MUL_SRC1_DS0_BASE + PDS_DS0_INDEX * PDS_NUM_WORDS_PER_DWORD,
													 EURASIA_PDS_MUL_SRC2SEL_DS1,
													 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD);
		}
		else
		{
			*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MUL_DESTSEL_DS1,
													 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_MUL_SRC1SEL_REG,
													 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1L : EURASIA_PDS_MUL_SRC1_IR0L,
													 EURASIA_PDS_MUL_SRC2SEL_DS1,
													 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD);
		}
		
		if ((!psStream->bInstanceData && psProgram->b32BitIndices) ||
			(psStream->bInstanceData && (psProgram->ui32NumInstances > 0x10000)))
		{
			/*
				mul ds0[PDS_DS0_ADDRESS_PART], ds0[PDS_DS0_INDEX].h, ds1[constant_stride].l
				shl ds0[PDS_DS0_ADDRESS_PART], ds0[PDS_DS0_ADDRESS_PART], 16
				add ds1[PDS_DS1_DMA_ADDRESS], ds0[PDS_DS0_ADDRESS_PART], ds1[PDS_DS1_DMA_ADDRESS]
			*/
			if (psStream->ui32Multiplier || psStream->ui32Shift)
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_DS0,
														 EURASIA_PDS_MUL_SRC1_DS0_BASE + PDS_DS0_INDEX * PDS_NUM_WORDS_PER_DWORD + 1,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD);
			}
			else
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1H : EURASIA_PDS_MUL_SRC1_IR0H,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD);
			}
			
			*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_SHIFT_DESTSEL_DS0,
													 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SRCSEL_DS0,
													 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SHIFT_ZERO + 16);
													 
			*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_ARITH_DESTSEL_DS1,
													 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_ARITH_SRC1SEL_DS0,
													 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_ARITH_SRC2SEL_DS1,
													 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS);
		}
		
		if (psStream->ui32Stride > 0xFFFF)
		{
			/*
				mul ds0[PDS_DS0_ADDRESS_PART], ds0[PDS_DS0_INDEX].l, ds1[constant_stride].h
				shl ds0[PDS_DS0_ADDRESS_PART], ds1[PDS_DS0_ADDRESS_PART], 16
				add ds1[PDS_DS1_DMA_ADDRESS], ds0[PDS_DS0_ADDRESS_PART], ds1[PDS_DS1_DMA_ADDRESS]
			*/
			if (psStream->ui32Multiplier || psStream->ui32Shift)
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_DS0,
														 EURASIA_PDS_MUL_SRC1_DS0_BASE + PDS_DS0_INDEX * PDS_NUM_WORDS_PER_DWORD,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD + 1);
			}
			else
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1L : EURASIA_PDS_MUL_SRC1_IR0L,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + ui32DS1Constant * PDS_NUM_WORDS_PER_DWORD + 1);
			}
			
			*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_SHIFT_DESTSEL_DS0,
													 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SRCSEL_DS0,
													 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SHIFT_ZERO + 16);
													 
			*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_ARITH_DESTSEL_DS1,
													 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_ARITH_SRC1SEL_DS0,
													 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_ARITH_SRC2SEL_DS1,
													 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS);
		}
		
		for (ui32Element = 0; ui32Element < psStream->ui32NumElements; ui32Element++)
		{
			/*
				Get the DMA source address, destination register and size
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
			
			/*
				Add the vertex stream base address and vertex element offset
				
				And DMA the vertex element into the USE primary attribute buffer
				
				movsa doutd, ds0[constant_address], ds0[constant_size_and_register], ds1[PDS_DS1_DMA_ADDRESS], ds1[PDS_DS1_DMA_ADDRESS], emit
			*/
			*pui32Instruction++	= PDSEncodeMOVSA	(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MOVS_DEST_DOUTD,
													 EURASIA_PDS_MOVS_SRC1SEL_DS0,
													 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SRC2SEL_DS1,
													 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS / PDS_NUM_DWORDS_PER_REG,
													 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
													 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
		}
	}
	
	/*
		Conditionaly issue the unpack task to the USE
		
		(if0) movs doutu, ds0[constant_use], ds0[constant_use], ds1[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_IF0,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32VertexDataSegment	= pui32Constants;
	psProgram->ui32DataSize				= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

/*****************************************************************************
 Function Name	: PDSGenerateInterPipeSDProgram
 Inputs			: 	psProgram		- pointer to the PDS tesselator program
					pui32Buffer		- pointer to the buffer for the program
					bClearSyncFlag	- Clears the PDS sync constant
									   for tesselation syncronisation
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS program to run the simulated USE inter-pipe sequential dependency.
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateInterPipeSDProgram	(PPDS_IPSD_PROGRAM	psProgram,
														 IMG_UINT32 *		pui32Buffer,
														 IMG_BOOL			bClearSyncFlag)
{
	IMG_UINT32 *					pui32Constants;
	IMG_UINT32					ui32NextDS0Constant;
	IMG_UINT32					ui32NextDS1Constant;
	IMG_UINT32					ui32DS1Constant;
	IMG_UINT32					ui32DS0Constant;
	IMG_UINT32					ui32NumConstants;
	IMG_UINT32 *					pui32Instructions;
	IMG_UINT32 *					pui32Instruction;
	
	PDS_DPF("PDSGenerateInterPipeSDProgram()");
	
	/*
		Generate the PDS tesselator data
	*/
	pui32Constants		= (IMG_UINT32 *) (((IMG_UINTPTR_T)pui32Buffer + ((1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1)) & ~((IMG_UINTPTR_T)(1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	if(bClearSyncFlag)
	{
		/*
			Copy the flag that will indicate the first task has NOT been sent.
		*/
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, 0);
	}
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
	PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32USETaskControl[2]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant, ui32NextDS1Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS tesselator code
	*/
	pui32Instructions	= pui32Constants + ui32NumConstants;
	pui32Instruction		= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	if(bClearSyncFlag)
	{
		/*
			Clear the flag.
			
			mov32 PDS_DS0_TESS_SYNC, #0
		*/
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
		*pui32Instruction++	= PDSEncodeMOV32	(EURASIA_PDS_CC_ALWAYS,
															 EURASIA_PDS_MOV32_DESTSEL_DS0,
															 EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS0_TESS_SYNC,
															 EURASIA_PDS_MOV32_SRCSEL_DS1,
															 EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS1Constant);
	}
	
	/*
		Issue the dummy task to the USE
		
		movs doutu, ds0[constant_use], ds1[constant_use], ds0[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	
	
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

/*****************************************************************************
 Function Name	: PDSGenerateGenericKickUSEProgram
 Inputs			: 	psProgram		- pointer to the PDS program
					pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS program to run a general USE program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateGenericKickUSEProgram	(PPDS_GENERICKICKUSE_PROGRAM	psProgram,
															 IMG_UINT32						*pui32Buffer)
{
	IMG_UINT32 *				pui32Constants;
	IMG_UINT32					ui32NextDS0Constant;
	IMG_UINT32					ui32NextDS1Constant;
	IMG_UINT32					ui32DS0Constant;
	IMG_UINT32					ui32DS1Constant;
	IMG_UINT32					ui32NumConstants;
	IMG_UINT32 *				pui32Instructions;
	IMG_UINT32 *				pui32Instruction;
	
	PDS_DPF("PDSGenerateGenericKickUSEProgram()");
	
	/*
		Generate the PDS tesselator data
	*/
	pui32Constants		= (IMG_UINT32 *) (((IMG_UINTPTR_T)pui32Buffer + ((1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1)) & ~((IMG_UINTPTR_T)(1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
	PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32USETaskControl[2]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant, ui32NextDS1Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS tesselator code
	*/
	pui32Instructions	= pui32Constants + ui32NumConstants;
	pui32Instruction		= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	/*
		Issue the task to the USE
		
		movs doutu, ds0[constant_use], ds1[constant_use], ds0[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

#if !defined(PDS_BUILD_OPENGLES)

/*****************************************************************************
 Function Name	: PDSGenerateComplexAdvanceFlushProgram
 Inputs			: 	psProgram		- pointer to the PDS tesselator program
					pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS program to run the advance 7 on the use.
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateComplexAdvanceFlushProgram	(PPDS_COMPLEXADVANCEFLUSH_PROGRAM	psProgram,
																 IMG_UINT32 *		pui32Buffer)
{
	IMG_UINT32 *					pui32Constants;
	IMG_UINT32					ui32NextDS0Constant;
	IMG_UINT32					ui32NextDS1Constant;
	IMG_UINT32					ui32DS0Constant;
	IMG_UINT32					ui32DS1Constant;
	IMG_UINT32					ui32NumConstants;
	IMG_UINT32 *					pui32Instructions;
	IMG_UINT32 *					pui32Instruction;
	
	PDS_DPF("PDSGenerateComplexAdvanceFlushProgram()");
	
	/*
		Generate the PDS tesselator data
	*/
	pui32Constants		= (IMG_UINT32 *) (((IMG_UINTPTR_T)pui32Buffer + ((1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1)) & ~((IMG_UINTPTR_T)(1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
	PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32USETaskControl[2]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant, ui32NextDS1Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS tesselator code
	*/
	pui32Instructions	= pui32Constants + ui32NumConstants;
	pui32Instruction		= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	/*
		Issue the task to the USE
		
		movs doutu, ds0[constant_use], ds1[constant_use], ds0[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((ui32DS1Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

/*****************************************************************************
 Function Name	: PDSGenerateVSMemConstUploadProgram
 Inputs			: psProgram		- pointer to the PDS constant upload program
				: pui32Buffer	- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS program to load the constant upload list
				  into USSE primary attribute registers, ready for use by the
				  USSE constant upload program.
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateVSMemConstUploadProgram	(PPDS_VS_MEMCONST_UPLOAD_PROGRAM	psProgram,
																 IMG_UINT32 *						pui32Buffer)
{
	IMG_UINT32 *	pui32Constants;
	IMG_UINT32		ui32NextDS0Constant;
	IMG_UINT32		ui32NextDS1Constant;
	IMG_UINT32		ui32DS0Constant;
	IMG_UINT32		ui32DS1Constant;
	IMG_UINT32		ui32NumConstants;
	IMG_UINT32 *	pui32Instructions;
	IMG_UINT32 *	pui32Instruction;
	
	PDS_DPF("PDSGenerateVSMemConstUploadProgram()");
	
	/*
		Generate the PDS  secondary attributes data
	*/
	pui32Constants		= (IMG_UINT32 *) (((IMG_UINTPTR_T)pui32Buffer + ((1UL << EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) - 1)) & ~((IMG_UINTPTR_T)(1UL << EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	/*
		Copy the DMA control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32DMAControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32DMAControl[1]);
	
	if (psProgram->ui32NumDMAKicks == 2)
	{
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 2);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32DMAControl[2]);
		PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 1, psProgram->aui32DMAControl[3]);
	}
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
	PDSSetDS1Constant(pui32Constants, ui32DS1Constant + 0, psProgram->aui32USETaskControl[2]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant, ui32NextDS1Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS code to load the constant upload list into primary attribute
		registers
	*/
	pui32Instructions	= pui32Constants + ui32NumConstants;
	pui32Instruction	= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS1Constant	= PDS_DS1_CONST_BLOCK_BASE;
	
	/*
		DMA the constant upload list into the primary attributes
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTD,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 EURASIA_PDS_MOVS_SWIZ_SRC1H,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 EURASIA_PDS_MOVS_SWIZ_SRC1H);
											 
	if (psProgram->ui32NumDMAKicks == 2)
	{
		ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 2);
		
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTD,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + PDS_DS0_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 EURASIA_PDS_MOVS_SWIZ_SRC2H,
												 EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 EURASIA_PDS_MOVS_SWIZ_SRC2H);
	}
	
	/*
		Kick the USE to upload the new constant data
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS1Constant		= PDSGetConstants(&ui32NextDS1Constant, 1);
	
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + ui32DS1Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 EURASIA_PDS_MOVS_SWIZ_SRC1H,
											 EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 EURASIA_PDS_MOVS_SWIZ_SRC2H);
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}
#endif /* #if defined(PDS_BUILD_OPENGL) && !defined(PDS_BUILD_OPENGLES)*/

#endif /* #if !defined(PDS_BUILD_OPENGLES) || defined(GLES1_EXTENSION_CARNAV)*/


/*****************************************************************************
 Function Name	: PDSGetNextConstant
 Inputs			: pui32NextDS0Constant	- pointer to the next DS0 constant
				: pui32NextDS1Constant	- pointer to the next DS1 constant
 Outputs		: pui32NextDS0Constant	- pointer to the next DS0 constant
				: pui32NextDS1Constant	- pointer to the next DS1 constant
 Returns		: ui32Constant			- the next constant
 Description	: Gets the next constant
*****************************************************************************/
static IMG_UINT32 PDSGetNextConstant	(IMG_UINT32 *	pui32NextDS0Constant,
								 IMG_UINT32 *	pui32NextDS1Constant)
{
	IMG_UINT32	ui32Constant;
	
	if ((*pui32NextDS0Constant / PDS_NUM_DWORDS_PER_REG) <= (*pui32NextDS1Constant / PDS_NUM_DWORDS_PER_REG))
	{
		ui32Constant = (2 * (*pui32NextDS0Constant / PDS_NUM_DWORDS_PER_ROW)) * PDS_NUM_DWORDS_PER_ROW + (*pui32NextDS0Constant % PDS_NUM_DWORDS_PER_ROW);
		(*pui32NextDS0Constant)++;
	}
	else
	{
		ui32Constant = (2 * (*pui32NextDS1Constant / PDS_NUM_DWORDS_PER_ROW) + 1) * PDS_NUM_DWORDS_PER_ROW + (*pui32NextDS1Constant % PDS_NUM_DWORDS_PER_ROW);
		(*pui32NextDS1Constant)++;
	}
	
	PDS_ASSERT(ui32Constant < 2 * EURASIA_PDS_DATASTORE_TEMPSTART);
	
	return ui32Constant;
}


/*****************************************************************************
 Function Name	: PDSGetTemps
 Inputs			: pui32NextTemp	- pointer to the next temp
				: ui32NumTemps	- the number of temps
 Outputs		: pui32NextTemp	- pointer to the next temp
 Returns		: ui32temp		- the next temp
 Description	: Gets the next temps
*****************************************************************************/
static IMG_UINT32 PDSGetTemps	(IMG_UINT32 *	pui32NextTemp,
								 IMG_UINT32	ui32NumTemps)
{
	IMG_UINT32	ui32Temp;
	
	switch (ui32NumTemps)
	{
		default:
		case 1:
		{
			ui32Temp	= *pui32NextTemp;
			break;
		}
		case 2:
		{
			ui32Temp	= (*pui32NextTemp + 1) & ~1UL;
			break;
		}
	}
	*pui32NextTemp	= ui32Temp + ui32NumTemps;
	PDS_ASSERT((ui32Temp + ui32NumTemps) >= EURASIA_PDS_DATASTORE_TEMPSTART);
	
	return ui32Temp;
}

/*****************************************************************************
 Function Name	: PDSGetConstants
 Inputs			: pui32NextConstant	- pointer to the next constant
				: ui32NumConstants	- the number of constants
 Outputs		: pui32NextConstant	- pointer to the next constant
 Returns		: ui32Constant		- the next constant
 Description	: Gets the next constants
*****************************************************************************/
static IMG_UINT32 PDSGetConstants	(IMG_UINT32 *	pui32NextConstant,
								 IMG_UINT32	ui32NumConstants)
{
	IMG_UINT32	ui32Constant;
	
	switch (ui32NumConstants)
	{
		default:
		case 1:
		{
			ui32Constant	= *pui32NextConstant;
			break;
		}
		case 2:
		{
			ui32Constant	= (*pui32NextConstant + 1) & ~1UL;
			break;
		}
	}
	*pui32NextConstant	= ui32Constant + ui32NumConstants;
	PDS_ASSERT((ui32Constant + ui32NumConstants) <= EURASIA_PDS_DATASTORE_TEMPSTART);
	
	return ui32Constant;
}

/*****************************************************************************
 Function Name	: PDSGetDS0ConstantOffset
 Inputs			: ui32Constant - The constant offset to get
 Outputs		:
 Returns		: The constant offset into the buffer (in DWORDS)
 Description	: Gets the constant offset into the buffer
*****************************************************************************/
static IMG_UINT32 PDSGetDS0ConstantOffset(IMG_UINT32	ui32Constant)
{
	IMG_UINT32	ui32Row;
	IMG_UINT32	ui32Column;
	
	ui32Row		= ui32Constant / PDS_NUM_DWORDS_PER_ROW;
	ui32Column	= ui32Constant % PDS_NUM_DWORDS_PER_ROW;
	
	return (2 * ui32Row) * PDS_NUM_DWORDS_PER_ROW + ui32Column;
	
}

/*****************************************************************************
 Function Name	: PDSSetDS1ConstantOffset
 Inputs			: ui32Constant - The constant offset to get
 Outputs		:
 Returns		: The constant offset into the buffer (in DWORDS)
 Description	: Gets the constant offset into the buffer
*****************************************************************************/
static IMG_UINT32 PDSGetDS1ConstantOffset(IMG_UINT32	ui32Constant)
{
	IMG_UINT32	ui32Row;
	IMG_UINT32	ui32Column;
	
	ui32Row		= ui32Constant / PDS_NUM_DWORDS_PER_ROW;
	ui32Column	= ui32Constant % PDS_NUM_DWORDS_PER_ROW;
	
	return (2 * ui32Row + 1) * PDS_NUM_DWORDS_PER_ROW + ui32Column;
}

/*****************************************************************************
 Function Name	: PDSSetDS0Constant
 Inputs			: ui32NumConstants	- the constant
				: ui32Value			- the value
 Outputs		: pui32Constants		- pointer to the constants
 Returns		: none
 Description	: Sets the constant in the first bank
*****************************************************************************/
static IMG_VOID PDSSetDS0Constant	(IMG_UINT32 *	pui32Constants,
								 IMG_UINT32	ui32Constant,
								 IMG_UINT32	ui32Value)
{
	IMG_UINT32 ui32Offset =  PDSGetDS0ConstantOffset(ui32Constant);
	
	pui32Constants[ui32Offset]	= ui32Value;
}

/*****************************************************************************
 Function Name	: PDSSetDS1Constant
 Inputs			: ui32Constant		- the constant
				: ui32Value			- the value
 Outputs		: pui32Constants		- pointer to the constants
 Returns		: none
 Description	: Sets the constant in the second bank
*****************************************************************************/
static IMG_VOID PDSSetDS1Constant	(IMG_UINT32 *	pui32Constants,
								 IMG_UINT32	ui32Constant,
								 IMG_UINT32	ui32Value)
{
	IMG_UINT32 ui32Offset =  PDSGetDS1ConstantOffset(ui32Constant);
	
	pui32Constants[ui32Offset]	= ui32Value;
}

/*****************************************************************************
 Function Name	: PDSGetNumConstants
 Inputs			: pui32NextDS0Constant	- the next constant in the first bank
				: pui32NextDS1Constant	- the next constant in the second bank
 Outputs		: none
 Returns		: ui32NumConstants	- the number of constants
 Description	: Gets the number of constants
*****************************************************************************/
static IMG_UINT32 PDSGetNumConstants	(IMG_UINT32	ui32NextDS0Constant,
								 IMG_UINT32	ui32NextDS1Constant)
{
	IMG_UINT32	ui32NumConstants;
	IMG_UINT32	ui32Constant;
	IMG_UINT32	ui32Row;
	IMG_UINT32	ui32Column;
	IMG_UINT32	ui32NextConstant;
	
	ui32NumConstants = 0;
	if (ui32NextDS0Constant > 0)
	{
		ui32Constant		= ui32NextDS0Constant - 1;
		ui32Row			= ui32Constant / PDS_NUM_DWORDS_PER_ROW;
		ui32Column		= ui32Constant % PDS_NUM_DWORDS_PER_ROW;
		ui32NextConstant	= (2 * ui32Row) * PDS_NUM_DWORDS_PER_ROW + ui32Column + 1;
		ui32NumConstants = ui32NextConstant;
	}
	if (ui32NextDS1Constant > 0)
	{
		ui32Constant		= ui32NextDS1Constant - 1;
		ui32Row			= ui32Constant / PDS_NUM_DWORDS_PER_ROW;
		ui32Column		= ui32Constant % PDS_NUM_DWORDS_PER_ROW;
		ui32NextConstant	= (2 * ui32Row + 1) * PDS_NUM_DWORDS_PER_ROW + ui32Column + 1;
		if (ui32NextConstant > ui32NumConstants)
		{
			ui32NumConstants = ui32NextConstant;
		}
	}
	
	return ui32NumConstants;
}

#else /* !FIX_HW_BRN_25339 && !(FIX_HW_BRN_27330)*/


/*****************************************************************************
 Macro definitions
*****************************************************************************/

#define	PDS_NUM_WORDS_PER_DWORD		2
#define	PDS_NUM_DWORDS_PER_REG		2

#define	PDS_DS0_CONST_BLOCK_BASE	0
#define	PDS_DS0_CONST_BLOCK_SIZE	EURASIA_PDS_DATASTORE_TEMPSTART
#define	PDS_DS0_TEMP_BLOCK_BASE		EURASIA_PDS_DATASTORE_TEMPSTART
#define	PDS_DS1_CONST_BLOCK_BASE	0
#define	PDS_DS1_CONST_BLOCK_SIZE	EURASIA_PDS_DATASTORE_TEMPSTART
#define	PDS_DS1_TEMP_BLOCK_BASE		EURASIA_PDS_DATASTORE_TEMPSTART

#define PDS_DS1_TEMP0				(PDS_DS1_TEMP_BLOCK_BASE + 0)

#define	PDS_DS0_INDEX				(PDS_DS0_TEMP_BLOCK_BASE + 0)
#define	PDS_DS0_ADDRESS_PART		(PDS_DS0_TEMP_BLOCK_BASE + 1)

#define	PDS_DS1_INDEX_PART			(PDS_DS1_TEMP_BLOCK_BASE + 1)
#define	PDS_DS1_DMA_ADDRESS			(PDS_DS1_TEMP_BLOCK_BASE + 2)
#define	PDS_DS1_DMA_REGISTER		(PDS_DS1_TEMP_BLOCK_BASE + 3)

#define PDS_DS0_TESS_SYNC			(PDS_DS0_TEMP_BLOCK_BASE + 2)


#define PDS_DS0_VERTEX_TEMP_BLOCK_BASE 52
#define PDS_DS1_VERTEX_TEMP_BLOCK_BASE 52

#define PDS_DS0_VERTEX_TEMP_BLOCK_END  60
#define PDS_DS1_VERTEX_TEMP_BLOCK_END  60

#if !defined(SGX545)
#if !defined(SGX543) && !defined(SGX544) && !defined(SGX554)
#define EURASIA_DOUTI_TAG_MASK		((~EURASIA_PDS_DOUTI_TEXISSUE_CLRMSK) |		\
									 EURASIA_PDS_DOUTI_TEXCENTROID |			\
									 (~EURASIA_PDS_DOUTI_TEXWRAP_CLRMSK) |		\
									 (~EURASIA_PDS_DOUTI_TEXPROJ_CLRMSK) |		\
									 EURASIA_PDS_DOUTI_TEXLASTISSUE)
#else
#define EURASIA_DOUTI_TAG_MASK		((~EURASIA_PDS_DOUTI_TEXISSUE_CLRMSK) |		\
									 EURASIA_PDS_DOUTI_TEXCENTROID |			\
									 (~EURASIA_PDS_DOUTI_TEXWRAP_CLRMSK) |		\
									 (~EURASIA_PDS_DOUTI_TEXPROJ_CLRMSK) |		\
									 EURASIA_PDS_DOUTI_TEX_POINTSPRITE_FORCED | \
									 EURASIA_PDS_DOUTI_TEXLASTISSUE)
#endif
#else
#define EURASIA_DOUTI0_TAG_MASK		((~EURASIA_PDS_DOUTI_TEXISSUE_CLRMSK) |		\
									 EURASIA_PDS_DOUTI_TEXCENTROID |			\
									 (~EURASIA_PDS_DOUTI_TEXWRAP_CLRMSK) |		\
									 (~EURASIA_PDS_DOUTI_TEXPROJ_CLRMSK) |		\
									 EURASIA_PDS_DOUTI_TEXLASTISSUE)
									 
#define EURASIA_DOUTI1_TAG_MASK		((~EURASIA_PDS_DOUTI1_USESAMPLE_RATE_CLRMSK) |	\
									(~EURASIA_PDS_DOUTI1_TEXSAMPLE_RATE_CLRMSK) |	\
									EURASIA_PDS_DOUTI1_UPOINTSPRITE_FORCE | 		\
									EURASIA_PDS_DOUTI1_TPOINTSPRITE_FORCE | 		\
									EURASIA_PDS_DOUTI1_UDONT_ITERATE)
#endif
									 
/*****************************************************************************
 Structure forward declarations
*****************************************************************************/

/*****************************************************************************
 Structure definitions
*****************************************************************************/

/*****************************************************************************
 Static function prototypes
*****************************************************************************/

static IMG_UINT32 PDSGetConstants		  (IMG_UINT32 * pui32NextConstant, IMG_UINT32 ui32NumConstants);
static IMG_UINT32 PDSGetDS0ConstantOffset (IMG_UINT32   ui32Constant);
static IMG_VOID   PDSSetDS0Constant		  (IMG_UINT32 * pui32Constants, IMG_UINT32 ui32Constant, IMG_UINT32 ui32Value);
static IMG_UINT32 PDSGetNumConstants	  (IMG_UINT32 ui32NextDS0Constant);
static IMG_UINT32 PDSGetTemps			  (IMG_UINT32 * pui32NextTemp, IMG_UINT32 ui32NumTemps);

/*****************************************************************************
 Static variables
*****************************************************************************/

/*****************************************************************************
 Function definitions
*****************************************************************************/

#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
#define PDSSetTaskControl(X, Y) \
		PDS##X##Set##Y##0(pui32Buffer, psProgram->aui32##Y##USETaskControl[0]); \
		PDS##X##Set##Y##1(pui32Buffer, psProgram->aui32##Y##USETaskControl[1]); \
		PDS##X##Set##Y##2(pui32Buffer, psProgram->aui32##Y##USETaskControl[2]);
#else
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
#define PDSSetTaskControl(X, Y) \
		PDS##X##Set##Y##0(pui32Buffer, psProgram->aui32##Y##USETaskControl[0]); \
		PDS##X##Set##Y##1(pui32Buffer, psProgram->aui32##Y##USETaskControl[1]);
#else /* defined(SGX545) */
#define PDSSetTaskControl(X, Y) \
		PDS##X##Set##Y##0(pui32Buffer, psProgram->aui32##Y##USETaskControl[0]); \
		PDS##X##Set##Y##1(pui32Buffer, psProgram->aui32##Y##USETaskControl[1]); \
		PDS##X##Set##Y##2(pui32Buffer, psProgram->aui32##Y##USETaskControl[2]);
#endif /* defined(SGX545) */
#endif /* SGX_FEATURE_PDS_EXTENDED_SOURCES */
		
/*****************************************************************************
 Function Name	: PDSGeneratePixelEventProgram
 Inputs			: psProgram		- pointer to the PDS pixel event program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS pixel event program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGeneratePixelEventProgram	(PPDS_PIXEL_EVENT_PROGRAM	psProgram,
														 IMG_UINT32 *				pui32Buffer,
														 IMG_BOOL					bMultisampleWithoutDownscale,
														 IMG_BOOL					bDownscaleWithoutMultisample,
														 IMG_UINT32					ui32MultiSampleQuality)
{
	IMG_UINT32 *pui32Constants;
	
	PDS_DPF("PDSGeneratePixelEventProgram()");
	
	PDS_ASSERT((((IMG_UINT32)pui32Buffer) & ((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1)) == 0);
	PDS_ASSERT(!(bMultisampleWithoutDownscale && bDownscaleWithoutMultisample));
	
	#if !defined(SGX_FEATURE_PDS_EXTENDED_SOURCES) && !defined(PDS_BUILD_OPENGLES) && !defined(PDS_BUILD_SERVICES)
	PDS_ASSERT(sizeof(g_pui32PDSPixelEventMSAANoDownscale_4X) == ((IMG_UINT32)(psProgram-psProgram) + sizeof(g_pui32PDSPixelEvent)));
	PDS_ASSERT(sizeof(g_pui32PDSPixelEventDownscaleNoMSAA_4X) == ((IMG_UINT32)(psProgram-psProgram) + sizeof(g_pui32PDSPixelEvent)));
	#if defined(SGX_FEATURE_MULTISAMPLE_2X)
	PDS_ASSERT(sizeof(g_pui32PDSPixelEventMSAANoDownscale_2X) == ((IMG_UINT32)(psProgram-psProgram) + sizeof(g_pui32PDSPixelEvent)));
	PDS_ASSERT(sizeof(g_pui32PDSPixelEventDownscaleNoMSAA_2X) == ((IMG_UINT32)(psProgram-psProgram) + sizeof(g_pui32PDSPixelEvent)));
	#else
	PVR_UNREFERENCED_PARAMETER(ui32MultiSampleQuality);
	#endif
	#else
	PVR_UNREFERENCED_PARAMETER(ui32MultiSampleQuality);
	PVR_UNREFERENCED_PARAMETER(bMultisampleWithoutDownscale);
	PVR_UNREFERENCED_PARAMETER(bDownscaleWithoutMultisample);
	#endif
	PDS_ASSERT(PDS_PIXEVENT_PROG_SIZE == ((IMG_UINT32)(psProgram-psProgram) + sizeof(g_pui32PDSPixelEvent)));
	
	#if !defined(SGX_FEATURE_PDS_EXTENDED_SOURCES) && !defined(PDS_BUILD_OPENGLES) && !defined(PDS_BUILD_SERVICES)
	if (bMultisampleWithoutDownscale)
	{
		#if defined(SGX_FEATURE_MULTISAMPLE_2X)
		if(ui32MultiSampleQuality == 2)
		{
			memcpy(pui32Buffer, g_pui32PDSPixelEventMSAANoDownscale_2X, PDS_PIXEVENT_PROG_SIZE);
			psProgram->ui32DataSize	= PDS_PIXELEVENTMSAANODOWNSCALE_2X_DATA_SEGMENT_SIZE;
			
			/*
				Copy the task control words for the various USE programs to the PDS program's
				data segment.
			*/
			PDSSetTaskControl(PixelEventMSAANoDownscale_2X, EOT);
			PDSSetTaskControl(PixelEventMSAANoDownscale_2X, PTOFF);
			PDSSetTaskControl(PixelEventMSAANoDownscale_2X, EOR);
		}
		else
		#endif
		{
			memcpy(pui32Buffer, g_pui32PDSPixelEventMSAANoDownscale_4X, PDS_PIXEVENT_PROG_SIZE);
			psProgram->ui32DataSize	= PDS_PIXELEVENTMSAANODOWNSCALE_4X_DATA_SEGMENT_SIZE;
			
			/*
				Copy the task control words for the various USE programs to the PDS program's
				data segment.
			*/
			PDSSetTaskControl(PixelEventMSAANoDownscale_4X, EOT);
			PDSSetTaskControl(PixelEventMSAANoDownscale_4X, PTOFF);
			PDSSetTaskControl(PixelEventMSAANoDownscale_4X, EOR);
		}
		#if defined(SGX_FEATURE_WRITEBACK_DCU)
		PDSPixelEventSetEOT_DOUTA1(pui32Buffer, 0);
		#endif
	}
	else if (bDownscaleWithoutMultisample)
	{
		#if defined(SGX_FEATURE_MULTISAMPLE_2X)
		if(ui32MultiSampleQuality == 2)
		{
			memcpy(pui32Buffer, g_pui32PDSPixelEventDownscaleNoMSAA_2X, PDS_PIXEVENT_PROG_SIZE);
			psProgram->ui32DataSize	= PDS_PIXELEVENTDOWNSCALENOMSAA_2X_DATA_SEGMENT_SIZE;
			
			/*
				Copy the task control words for the various USE programs to the PDS program's
				data segment.
			*/
			PDSSetTaskControl(PixelEventDownscaleNoMSAA_2X, EOT);
			PDSSetTaskControl(PixelEventDownscaleNoMSAA_2X, PTOFF);
			PDSSetTaskControl(PixelEventDownscaleNoMSAA_2X, EOR);
		}
		else
		#endif
		{
			memcpy(pui32Buffer, g_pui32PDSPixelEventDownscaleNoMSAA_4X, PDS_PIXEVENT_PROG_SIZE);
			psProgram->ui32DataSize	= PDS_PIXELEVENTDOWNSCALENOMSAA_4X_DATA_SEGMENT_SIZE;
			
			/*
				Copy the task control words for the various USE programs to the PDS program's
				data segment.
			*/
			PDSSetTaskControl(PixelEventDownscaleNoMSAA_4X, EOT);
			PDSSetTaskControl(PixelEventDownscaleNoMSAA_4X, PTOFF);
			PDSSetTaskControl(PixelEventDownscaleNoMSAA_4X, EOR);
		}
		
		#if defined(SGX_FEATURE_WRITEBACK_DCU)
		PDSPixelEventSetEOT_DOUTA1(pui32Buffer, 0);
		#endif
	}
	else
	#endif /* !defined(SGX_FEATURE_PDS_EXTENDED_SOURCES) */
	{
		memcpy(pui32Buffer, g_pui32PDSPixelEvent, PDS_PIXEVENT_PROG_SIZE);
		psProgram->ui32DataSize	= PDS_PIXELEVENT_DATA_SEGMENT_SIZE;
		
		/*
			Copy the task control words for the various USE programs to the PDS program's
			data segment.
		*/
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
		PDSPixelEventSetEOT0(pui32Buffer, psProgram->aui32EOTUSETaskControl[0]);
		PDSPixelEventSetEOT1(pui32Buffer, psProgram->aui32EOTUSETaskControl[1]);
#else
		PDSSetTaskControl(PixelEvent, EOT);
#endif
		PDSSetTaskControl(PixelEvent, PTOFF);
		PDSSetTaskControl(PixelEvent, EOR);
		#if defined(SGX_FEATURE_WRITEBACK_DCU)
		PDSPixelEventSetEOT_DOUTA1(pui32Buffer, 0);
		#endif
	}
	
	/*
		Generate the PDS pixel event data
	*/
	pui32Constants				= (IMG_UINT32 *) (((IMG_UINT32) pui32Buffer + ((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1));
	psProgram->pui32DataSegment	= pui32Constants;
	
	return (IMG_PUINT32)((IMG_PUINT8)pui32Buffer + PDS_PIXEVENT_PROG_SIZE);
}

/*****************************************************************************
 Function Name	: PDSGeneratePixelShaderSAProgram
 Inputs			: psProgram		- pointer to the PDS pixel shader secondary attributes program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS pixel shader seconary attributes program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGeneratePixelShaderSAProgram	(PPDS_PIXEL_SHADER_SA_PROGRAM	psProgram,
															 IMG_UINT32 *					pui32Buffer)
{
	IMG_UINT32 *	pui32Constants;
	IMG_UINT32	ui32NextDS0Constant;
	IMG_UINT32	ui32DS0Constant;
	IMG_UINT32	ui32NumConstants;
	IMG_UINT32 *	pui32Instructions;
	IMG_UINT32 *	pui32Instruction;
	IMG_UINT32 ui32IterateZWord;
	
	PDS_DPF("PDSGeneratePixelShaderSAProgram()");
	
	PDS_ASSERT((((IMG_UINT32)pui32Buffer) & ((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1)) == 0);
	PDS_ASSERT(psProgram->ui32NumDMAKicks <= 3);
	
	/*
		Generate the PDS pixel shader secondary attributes data
	*/
	pui32Constants		= pui32Buffer;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	/*
		Copy the DMA control words to constants
	*/
	if (psProgram->ui32NumDMAKicks >= 1)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32DMAControl[0]);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32DMAControl[1]);
		
		if (psProgram->ui32NumDMAKicks >= 2)
		{
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32DMAControl[2]);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32DMAControl[3]);
			
			if (psProgram->ui32NumDMAKicks == 3)
			{
				ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
				PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32DMAControl[4]);
				PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32DMAControl[5]);
			}
			
		}
	}
	
	if (psProgram->bWriteTilePosition)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2); // request 2 to keep the DOUTU constant alignment ok.
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant, (psProgram->uTilePositionAttrDest << EURASIA_PDS_DOUTA1_AO_SHIFT));
	}
	
	if (psProgram->bKickUSE || psProgram->bKickUSEDummyProgram)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 2, psProgram->aui32USETaskControl[2]);
	}
	
	if(psProgram->bIterateZAbs)
	{
		ui32IterateZWord = EURASIA_PDS_DOUTI_TEXISSUE_NONE  |
							EURASIA_PDS_DOUTI_USEISSUE_ZABS  |
							EURASIA_PDS_DOUTI_USELASTISSUE;
							
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, ui32IterateZWord);
	}
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_PDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_PDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS pixel shader secondary attributes code
	*/
	pui32Instructions		= pui32Constants + ui32NumConstants;
	pui32Instruction		= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	/*
		DMA the state into the secondary attributes
	*/
	if (psProgram->ui32NumDMAKicks >= 1)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTD,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H);
												 
		if (psProgram->ui32NumDMAKicks >= 2)
		{
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
			
			*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MOVS_DEST_DOUTD,
													 EURASIA_PDS_MOVS_SRC1SEL_DS0,
													 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SRC2SEL_DS1,
													 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 EURASIA_PDS_MOVS_SWIZ_SRC1H,
													 EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 EURASIA_PDS_MOVS_SWIZ_SRC1H);
													 
			if (psProgram->ui32NumDMAKicks == 3)
			{
				ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
				
				*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
														EURASIA_PDS_MOVS_DEST_DOUTD,
														EURASIA_PDS_MOVS_SRC1SEL_DS0,
														EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
														EURASIA_PDS_MOVS_SRC2SEL_DS1,
														EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
														EURASIA_PDS_MOVS_SWIZ_SRC1L,
														EURASIA_PDS_MOVS_SWIZ_SRC1H,
														EURASIA_PDS_MOVS_SWIZ_SRC1L,
														EURASIA_PDS_MOVS_SWIZ_SRC1H);
			}
			
		}
	}
	
	if (psProgram->bWriteTilePosition)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		
		*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
											EURASIA_PDS_MOV32_DESTSEL_DS1,
											EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
											EURASIA_PDS_MOV32_SRCSEL_DS0,
											EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant);
											
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTA,
												 EURASIA_PDS_MOVS_SRC1SEL_REG,
												 EURASIA_PDS_MOVS_SRC1_IR0,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 (PDS_DS1_TEMP0 % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 (PDS_DS1_TEMP0 % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
	}
	
	if (psProgram->bKickUSE || psProgram->bKickUSEDummyProgram)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
		
		*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
											EURASIA_PDS_MOV32_DESTSEL_DS1,
											EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
											EURASIA_PDS_MOV32_SRCSEL_DS0,
											EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant + 2);
											
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTU,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H,
												 (PDS_DS1_TEMP0 % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 (PDS_DS1_TEMP0 % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
	}
	
	if(psProgram->bIterateZAbs)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
		
		*pui32Instruction++ = PDSEncodeMOVS	(EURASIA_PDS_CC_ALWAYS,
											EURASIA_PDS_MOVS_DEST_DOUTI,
											EURASIA_PDS_MOVS_SRC1SEL_DS0,
											EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											EURASIA_PDS_MOVS_SRC2SEL_DS1,
											EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											EURASIA_PDS_MOVS_SWIZ_SRC2L,
											EURASIA_PDS_MOVS_SWIZ_SRC2H);
	}
	
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}


/*****************************************************************************
 Function Name	: PDSGenerateStaticPixelShaderSAProgram
 Inputs			: psProgram		- pointer to the PDS pixel shader secondary attributes program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS pixel shader seconary attributes program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateStaticPixelShaderSAProgram	(PPDS_PIXEL_SHADER_STATIC_SA_PROGRAM psProgram,
																 IMG_UINT32 *pui32Buffer)
{
	IMG_UINT32 *pui32Instructions;
	IMG_UINT32 *pui32Instruction;
	IMG_UINT32 *pui32Constants;
	IMG_UINT32	ui32NextDS0Constant;
	IMG_UINT32	ui32DS0Constant;
	IMG_UINT32	ui32NumConstants;
	IMG_UINT32 i;
	
	PDS_DPF("PDSGenerateStaticPixelShaderSAProgram()");
	
	PDS_ASSERT(psProgram->ui32DAWCount > 0);
	
	/*
		Generate the PDS pixel shader secondary attributes data
	*/
	pui32Constants		= pui32Buffer;
	
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	if (psProgram->bKickUSEDummyProgram)
	{
		ui32DS0Constant	= PDSGetConstants(&ui32NextDS0Constant, 3);
		
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 2, psProgram->aui32USETaskControl[2]);
	}
	
	if(psProgram->bIterateZAbs)
	{
		IMG_UINT32 ui32IterateZWord;
		
		ui32IterateZWord = 	EURASIA_PDS_DOUTI_TEXISSUE_NONE  |
							EURASIA_PDS_DOUTI_USEISSUE_ZABS  |
							EURASIA_PDS_DOUTI_USELASTISSUE;
							
		ui32DS0Constant	= PDSGetConstants(&ui32NextDS0Constant, 1);
		
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, ui32IterateZWord);
	}
	
	for(i=0; i<psProgram->ui32DAWCount; i++)
	{
		ui32DS0Constant	= PDSGetConstants(&ui32NextDS0Constant, 2);
		
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->pui32DAWData[i]);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, ((psProgram->ui32DAWOffset + i) << EURASIA_PDS_DOUTA1_AO_SHIFT));
	}
	
	/*
		Get the number of constants
	*/
	ui32NumConstants = PDSGetNumConstants(ui32NextDS0Constant);
	ui32NumConstants = ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_PDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_PDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS pixel shader secondary attributes code
	*/
	pui32Instructions	= pui32Constants + ui32NumConstants;
	pui32Instruction	= pui32Instructions;
	
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	if (psProgram->bKickUSEDummyProgram)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
		
		*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
											EURASIA_PDS_MOV32_DESTSEL_DS1,
											EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
											EURASIA_PDS_MOV32_SRCSEL_DS0,
											EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant + 2);
											
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTU,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H,
												 (PDS_DS1_TEMP0 % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 (PDS_DS1_TEMP0 % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
	}
	
	if(psProgram->bIterateZAbs)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
		
		*pui32Instruction++ = PDSEncodeMOVS	(EURASIA_PDS_CC_ALWAYS,
											EURASIA_PDS_MOVS_DEST_DOUTI,
											EURASIA_PDS_MOVS_SRC1SEL_DS0,
											EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											EURASIA_PDS_MOVS_SRC2SEL_DS1,
											EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											EURASIA_PDS_MOVS_SWIZ_SRC2L,
											EURASIA_PDS_MOVS_SWIZ_SRC2H);
	}
	
	for(i=0; i<psProgram->ui32DAWCount; i++)
	{
		ui32DS0Constant	= PDSGetConstants(&ui32NextDS0Constant, 2);
		
		*pui32Instruction++	= PDSEncodeMOVS	(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTA,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 EURASIA_PDS_MOVS_SWIZ_SRC1H,
											 EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 EURASIA_PDS_MOVS_SWIZ_SRC2L);
	}
	
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}


/*****************************************************************************
 Function Name	: PDSGeneratePixelShaderProgram
 Inputs			: psProgram		- pointer to the PDS pixel shader program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS pixel shader program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32* PDSGeneratePixelShaderProgram	(PPDS_TEXTURE_IMAGE_UNIT      psTextureImageUnits,
														 PPDS_PIXEL_SHADER_PROGRAM	  psProgram,
														 IMG_UINT32                  *pui32Buffer)
{
	IMG_UINT32 *pui32Constants;
	IMG_UINT32	ui32DS0Constant;
	IMG_UINT32	ui32NextDS0Constant;
	IMG_UINT32	ui32Iterator;
	IMG_UINT32	ui32NumConstants;
	IMG_UINT32 *pui32Instructions;
	IMG_UINT32 *pui32Instruction;
	#if defined(SGX545)
	IMG_UINT32 *pui32LastIterationIssue0;
	IMG_UINT32 *pui32LastIterationIssue1;
	#else
	IMG_UINT32 *pui32LastIterationIssue;
	#endif
	IMG_BOOL	abCombinedWithPreviousIssue[PVRD3D_MAXIMUM_ITERATIONS];
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
	IMG_UINT8	aui8TagSize[PVRD3D_MAXIMUM_ITERATIONS];
#endif
	PDS_DPF("PDSGeneratePixelShaderProgram()");
	
	PDS_ASSERT((((IMG_UINT32)pui32Buffer) & ((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1)) == 0);
	
	/*
		Generate the PDS pixel shader data
	*/
	pui32Constants		= (IMG_UINT32 *) (((IMG_UINTPTR_T) pui32Buffer + ((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1)) & ~((IMG_UINTPTR_T)(1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 2, psProgram->aui32USETaskControl[2]);
	
	/*
		Copy the FPU iterator and texture control words to constants
	*/
	#if defined(SGX545)
	pui32LastIterationIssue0 = IMG_NULL;
	pui32LastIterationIssue1 = IMG_NULL;
	#else
	pui32LastIterationIssue = IMG_NULL;
	#endif
	
	for (ui32Iterator = 0; ui32Iterator < psProgram->ui32NumFPUIterators; ui32Iterator++)
	{
		IMG_UINT32 ui32TagIssue	= psProgram->aui32TAGLayers[ui32Iterator];
		
		if (ui32TagIssue != 0xFFFFFFFF)
		{
			PPDS_TEXTURE_IMAGE_UNIT psTextureCtl = &psTextureImageUnits[ui32TagIssue];
			#if defined(SGX545)
			IMG_UINT32 ui32DOutI0 = psProgram->aui32FPUIterators0[ui32Iterator];
			IMG_UINT32 ui32DOutI1 = psProgram->aui32FPUIterators1[ui32Iterator];
			#else
			IMG_UINT32 ui32DOutI = psProgram->aui32FPUIterators[ui32Iterator];
			#endif
			
			/*
				Check if we can combine this issue with the previous one.
			*/
			#if defined(SGX545)
			if (pui32LastIterationIssue0 != IMG_NULL && pui32LastIterationIssue1 != IMG_NULL)
			#else
			if (pui32LastIterationIssue != IMG_NULL)
			#endif
			{
				#if defined(SGX545)
				*pui32LastIterationIssue0 &= ~EURASIA_DOUTI0_TAG_MASK;
				*pui32LastIterationIssue0 |= (ui32DOutI0 & EURASIA_DOUTI0_TAG_MASK);
				*pui32LastIterationIssue1 &= ~EURASIA_DOUTI1_TAG_MASK;
				*pui32LastIterationIssue1 |= (ui32DOutI1 & EURASIA_DOUTI1_TAG_MASK);
				#else
				*pui32LastIterationIssue &= ~EURASIA_DOUTI_TAG_MASK;
				*pui32LastIterationIssue |= (ui32DOutI & EURASIA_DOUTI_TAG_MASK);
				#endif
				
				abCombinedWithPreviousIssue[ui32Iterator] = IMG_TRUE;
				
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
				aui8TagSize[ui32Iterator - 1] = psProgram->aui8LayerSize[ui32Iterator];
#endif
			}
			else
			{
				#if defined(SGX545)
				ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
				PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, ui32DOutI0);
				PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, ui32DOutI1);
				#else
				ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
				PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, ui32DOutI);
				#endif
				
				abCombinedWithPreviousIssue[ui32Iterator] = IMG_FALSE;
				
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
				aui8TagSize[ui32Iterator] = psProgram->aui8LayerSize[ui32Iterator];
#endif
			}
			#if defined(SGX545)
			pui32LastIterationIssue0 = IMG_NULL;
			pui32LastIterationIssue1 = IMG_NULL;
			#else
			pui32LastIterationIssue = IMG_NULL;
			#endif
			
			if(ui32NextDS0Constant & 0x1)
			{
				// ensure that we are starting on an even numbered register...
				ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
			}
			
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, EURASIA_TAG_TEXTURE_STATE_SIZE);
			
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psTextureCtl->ui32TAGControlWord0);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psTextureCtl->ui32TAGControlWord1);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 2, psTextureCtl->ui32TAGControlWord2);
			#if(EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 3, psTextureCtl->ui32TAGControlWord3);
			#endif
		}
		else
		{
			#if defined(SGX545)
			IMG_UINT32 ui32DOutI0 = psProgram->aui32FPUIterators0[ui32Iterator];
			IMG_UINT32 ui32DOutI1 = psProgram->aui32FPUIterators1[ui32Iterator];
			#else
			IMG_UINT32 ui32DOutI = psProgram->aui32FPUIterators[ui32Iterator];
			#endif
			
			#if defined(SGX545)
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, ui32DOutI0);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, ui32DOutI1);
			
			if ((ui32DOutI0 & ~EURASIA_PDS_DOUTI_TEXISSUE_CLRMSK) == EURASIA_PDS_DOUTI_TEXISSUE_NONE)
			{
				pui32LastIterationIssue0 = pui32Constants + PDSGetDS0ConstantOffset(ui32DS0Constant + 0);
				pui32LastIterationIssue1 = pui32Constants + PDSGetDS0ConstantOffset(ui32DS0Constant + 1);
			}
			#else
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, ui32DOutI);
			
			if ((ui32DOutI & ~EURASIA_PDS_DOUTI_TEXISSUE_CLRMSK) == EURASIA_PDS_DOUTI_TEXISSUE_NONE)
			{
				pui32LastIterationIssue = pui32Constants + PDSGetDS0ConstantOffset(ui32DS0Constant);
			}
			#endif
			
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
			aui8TagSize[ui32Iterator] = 0;
#endif
		}
	}
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_PDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_PDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS pixel shader code
	*/
	pui32Instructions		= pui32Constants + ui32NumConstants;
	pui32Instruction		= pui32Instructions;
	ui32NextDS0Constant		= PDS_DS0_CONST_BLOCK_BASE;
	
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
	
	*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
										EURASIA_PDS_MOV32_DESTSEL_DS1,
										EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
										EURASIA_PDS_MOV32_SRCSEL_DS0,
										EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant + 2);
										
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	for (ui32Iterator = 0; ui32Iterator < psProgram->ui32NumFPUIterators; ui32Iterator++)
	{
		IMG_UINT32 ui32TagIssue	= psProgram->aui32TAGLayers[ui32Iterator];
		
		if (ui32TagIssue != 0xFFFFFFFF)
		{
			if (!abCombinedWithPreviousIssue[ui32Iterator])
			{
			
				ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, EURASIA_PDS_DOUTI_STATE_SIZE);
				
				/*
					Iterate the components and send them to the tag
				*/
				*pui32Instruction++ = PDSEncodeMOVS	(EURASIA_PDS_CC_ALWAYS,
													EURASIA_PDS_MOVS_DEST_DOUTI,
													EURASIA_PDS_MOVS_SRC1SEL_DS0,
													EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
													EURASIA_PDS_MOVS_SRC2SEL_DS1,
													EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
													((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
													((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
													((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
													((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
			}
			
			if(ui32NextDS0Constant & 0x1)
			{
				// ensure that we are starting on an even numbered register...
				ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
			}
			
			/*
				Setup the TAG layer control words
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, EURASIA_TAG_TEXTURE_STATE_SIZE);
			
			*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
												EURASIA_PDS_MOV32_DESTSEL_DS1,
												EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
												EURASIA_PDS_MOV32_SRCSEL_DS0,
												EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant + 2);
			
			#if(EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
			*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
												EURASIA_PDS_MOV32_DESTSEL_DS1,
												EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0 + 1,
												EURASIA_PDS_MOV32_SRCSEL_DS0,
												EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant + 3);
			#endif
			
			*pui32Instruction++ = PDSEncodeMOVS	(EURASIA_PDS_CC_ALWAYS,
												EURASIA_PDS_MOVS_DEST_DOUTT,
												EURASIA_PDS_MOVS_SRC1SEL_DS0,
												EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
												EURASIA_PDS_MOVS_SRC2SEL_DS1,
												EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
												((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
												((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
												((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
												((PDS_DS1_TEMP0 + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
		}
		else
		{
			/*
				Iterate the component into the USE primary attribute buffer
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, EURASIA_PDS_DOUTI_STATE_SIZE);
			
			/*
				Iterate the components and send them to the tag
			*/
			*pui32Instruction++ = PDSEncodeMOVS	(EURASIA_PDS_CC_ALWAYS,
												EURASIA_PDS_MOVS_DEST_DOUTI,
												EURASIA_PDS_MOVS_SRC1SEL_DS0,
												EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
												EURASIA_PDS_MOVS_SRC2SEL_DS1,
												EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
												((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
												((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
												((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
												((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
		}
	}
	
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

/*****************************************************************************
 Function Name	: PDSGenerateVertexShaderSAProgram
 Inputs			: psProgram		- pointer to the PDS vertex shader secondary attributes program
				: pui32Buffer	- pointer to the buffer for the program
				: bShadowSAs	- Copy out control stream object into the shadow.
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS vertex shader seconary attributes program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateVertexShaderSAProgram	(PPDS_VERTEX_SHADER_SA_PROGRAM	psProgram,
															 IMG_UINT32                    *pui32Buffer,
															 IMG_BOOL						bShadowSAs)
{
	IMG_UINT32 *	pui32Constants;
	IMG_UINT32	ui32NextDS0Constant;
	IMG_UINT32	ui32DS0Constant;
	IMG_UINT32	ui32NumConstants;
	IMG_UINT32 *	pui32Instructions;
	IMG_UINT32 *	pui32Instruction;
	
	PDS_DPF("PDSGenerateVertexShaderSAProgram()");
	
	PDS_ASSERT((((IMG_UINT32)pui32Buffer) & ((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1)) == 0);
	
	/*
		Generate the PDS vertex shader secondary attributes data
	*/
	pui32Constants		= (IMG_UINT32 *) (((IMG_UINTPTR_T) pui32Buffer + ((1UL << EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) - 1)) & ~((IMG_UINTPTR_T)(1UL << EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	/*
		Copy the DMA control words to constants
	*/
	if (psProgram->ui32NumDMAKicks > 0)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32DMAControl[0]);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32DMAControl[1]);
	}
	if (psProgram->ui32NumDMAKicks == 2)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32DMAControl[2]);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32DMAControl[3]);
	}
	
	if(bShadowSAs)
	{
		/*
			Copy the USE task control words to constants
		*/
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[2]);
	}
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS vertex shader secondary attributes code
	*/
	pui32Instructions		= pui32Constants + ui32NumConstants;
	pui32Instruction		= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	/*
		DMA the state into the secondary attributes
	*/
	if (psProgram->ui32NumDMAKicks > 0)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTD,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H);
	}
	if (psProgram->ui32NumDMAKicks == 2)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTD,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H);
	}
	
	if(bShadowSAs)
	{
		/*
			Issue the task to the USE...
			
			(if0) movs doutu, ds0[constant_use], ds0[constant_use], ds1[constant_use], ds1[constant_use], emit
		*/
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
		
		*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
												EURASIA_PDS_MOV32_DESTSEL_DS1,
												EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
												EURASIA_PDS_MOV32_SRCSEL_DS0,
												EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant + 2);
												
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTU,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
												 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
	}
	
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

/*****************************************************************************
 Function Name	: PDSGeneratePixelStateProgram
 Inputs			: psProgram		- pointer to the PDS pixel state program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS state program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateStateCopyProgram	(PPDS_STATE_COPY_PROGRAM	psProgram,
														 IMG_UINT32                *pui32Buffer)
{
	IMG_UINT32 *pui32Constants;
	IMG_UINT32	ui32NextDS0Constant;
	IMG_UINT32	ui32DS0Constant;
	IMG_UINT32	ui32NumConstants;
	IMG_UINT32 *pui32Instructions;
	IMG_UINT32 *pui32Instruction;
	
	PDS_DPF("PDSGenerateStateCopyProgram()");
	
	PDS_ASSERT((((IMG_UINT32)pui32Buffer) & ((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1)) == 0);
	
	/*
		Generate the PDS pixel state data
	*/
	pui32Constants		= (IMG_UINT32 *) (((IMG_UINTPTR_T) pui32Buffer + ((1UL << EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) - 1)) & ~((IMG_UINTPTR_T)(1UL << EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	/*
		Copy the DMA control words to constants
	*/
	if (psProgram->ui32NumDMAKicks >= 1)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32DMAControl[0]);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32DMAControl[1]);
	}
	if (psProgram->ui32NumDMAKicks == 2)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32DMAControl[2]);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32DMAControl[3]);
	}
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
	
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 2, psProgram->aui32USETaskControl[2]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS state code
	*/
	pui32Instructions		= pui32Constants + ui32NumConstants;
	pui32Instruction		= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	/*
		DMA the state into the primary attributes
	*/
	if (psProgram->ui32NumDMAKicks >= 1)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTD,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H);
	}
	if (psProgram->ui32NumDMAKicks == 2)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTD,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H);
	}
	
	/*
		Kick the USE to copy the state from the primary attributes into the output buffer
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
	
	*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
										EURASIA_PDS_MOV32_DESTSEL_DS1,
										EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
										EURASIA_PDS_MOV32_SRCSEL_DS0,
										EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant + 2);
										
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}


/*****************************************************************************
 Function Name	: PDSGenerateStaticVertexShaderProgram
 Inputs			: psProgram		- pointer to the PDS vertex shader program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the static PDS vertex shader program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateStaticVertexShaderProgram	(PPDS_VERTEX_SHADER_PROGRAM       psProgram,
																 IMG_UINT32                      *pui32Buffer,
																 PPDS_VERTEX_SHADER_PROGRAM_INFO  psPDSVertexShaderProgramInfo)
{
	IMG_UINT32 *			pui32Constants;
	IMG_UINT32				ui32NextDS0Constant;
	IMG_UINT32				ui32DS0Constant;
	IMG_UINT32				ui32NumConstants;
	IMG_UINT32				ui32Stream;
	PPDS_VERTEX_STREAM		psStream;
	IMG_UINT32				ui32Element;
	PPDS_VERTEX_ELEMENT		psElement;
	IMG_UINT32 *			pui32Instructions;
	IMG_UINT32 *			pui32Instruction;
	IMG_UINT32              ui32NextDS0PersistentReg;
	IMG_UINT32              ui32DS0PersistentReg;
	
	/*
		To load a typical set of vertex stream elements
		
		For each stream
		{
			Take the 24-bit index or the 24-bit instance
			Multiply by the stream stride
			For each element
			{
				Add the stream base address and the element offset
				DMA the element into the primray attribute buffer
			}
		}
		
		The straight-line code is organised to never re-visit constants, which are pre-computed into the program data
		
		The USE pre-processing program should unpack and pad the elements with 0s and 1s as required
	*/
	
	PDS_DPF("PDSGenerateStaticVertexShaderProgram()");
	
	/*
		Generate the PDS vertex shader data
	*/
	pui32Constants		     = pui32Buffer;
	ui32NextDS0Constant	     = PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS0PersistentReg = PDS_DS0_VERTEX_TEMP_BLOCK_BASE;
	
	for (ui32Stream = 0; ui32Stream < psProgram->ui32NumStreams; ui32Stream++)
	{
		psStream = &psProgram->asStreams[ui32Stream];
		
		
		/*
			Copy the vertex stream stride to a constant
		*/
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psStream->ui32Stride);
		
		/*
			Copy the vertex stream element address, size and primary attribute register to constants
		*/
		for (ui32Element = 0; ui32Element < psStream->ui32NumElements; ui32Element++)
		{
			IMG_UINT32 ui32DOutD1;
			IMG_UINT32 ui32DMASize;
			
			psElement = &psStream->asElements[ui32Element];
			
			/* Size is in bytes - round up to nearest 32 bit word */
			ui32DMASize = (psElement->ui32Size + 3) >> 2;
			
			/*
				Set up the DMA transfer control word.
			*/
			PDS_ASSERT(ui32DMASize <= EURASIA_PDS_DOUTD1_BSIZE_MAX);
			
			ui32DOutD1 =	((ui32DMASize - 1) << EURASIA_PDS_DOUTD1_BSIZE_SHIFT) |
							(psElement->ui32Register << EURASIA_PDS_DOUTD1_AO_SHIFT);
							
							
							
			/*
				Write the dma transfer control words into the PDS data section.
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant, ui32DOutD1);
			
			/*
			   Store the offsets if required
			*/
			if (psPDSVertexShaderProgramInfo)
			{
				psPDSVertexShaderProgramInfo->aui32AddressOffsets[ui32Stream][ui32Element] = PDSGetDS0ConstantOffset(ui32DS0Constant + 0);
				psPDSVertexShaderProgramInfo->aui32ElementOffsets[ui32Stream][ui32Element] = psElement->ui32Offset;
			}
		}
		
		/* Store the number of elements for this stream */
		if (psPDSVertexShaderProgramInfo)
		{
			psPDSVertexShaderProgramInfo->aui32NumElements[ui32Stream] = psStream->ui32NumElements;
		}
	}
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 2, psProgram->aui32USETaskControl[2]);
	
	/*
	   Store the offsets if required
	*/
	if (psPDSVertexShaderProgramInfo)
	{
		psPDSVertexShaderProgramInfo->aui32USETaskControlOffsets[0] = PDSGetDS0ConstantOffset(ui32DS0Constant + 0);
		psPDSVertexShaderProgramInfo->aui32USETaskControlOffsets[1] = PDSGetDS0ConstantOffset(ui32DS0Constant + 1);
		psPDSVertexShaderProgramInfo->aui32USETaskControlOffsets[2] = PDSGetDS0ConstantOffset(ui32DS0Constant + 2);
		
		/*
		   Store the number of streams
		*/
		psPDSVertexShaderProgramInfo->ui32NumStreams          = psProgram->ui32NumStreams;
	}
	
	/*
		Get the number of constants
	*/
	ui32NumConstants	= PDSGetNumConstants(ui32NextDS0Constant);
	ui32NumConstants	= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS vertex shader code
	*/
	pui32Instructions	     = pui32Constants + ui32NumConstants;
	pui32Instruction	     = pui32Instructions;
	ui32NextDS0Constant	     = PDS_DS0_CONST_BLOCK_BASE;
	ui32NextDS0PersistentReg = PDS_DS0_VERTEX_TEMP_BLOCK_BASE;
	
	for (ui32Stream = 0; ui32Stream < psProgram->ui32NumStreams; ui32Stream++)
	{
		psStream = &psProgram->asStreams[ui32Stream];
		
		
		if (psStream->ui32Shift)
		{
			/*
				shr ds0[PDS_DS0_INDEX], ir0/1, shift
			*/
			*pui32Instruction++	= PDSEncodeSHR		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_SHIFT_DESTSEL_DS0,
														 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_SHIFT_SRCSEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_SHIFT_SRC_IR1 : EURASIA_PDS_SHIFT_SRC_IR0,
														 EURASIA_PDS_SHIFT_SHIFT_ZERO + psStream->ui32Shift);
		}
		
		/*
			Multiply by the vertex stream stride
			
			mul ds1[PDS_DS1_DMA_ADDRESS], ds0[PDS_DS0_INDEX].l, ds1[constant_stride].l
		*/
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
		
		*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
											EURASIA_PDS_MOV32_DESTSEL_DS1,
											EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
											EURASIA_PDS_MOV32_SRCSEL_DS0,
											EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant);
											
		if (psStream->ui32Shift)
		{
			*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MUL_DESTSEL_DS1,
													 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_MUL_SRC1SEL_DS0,
													 EURASIA_PDS_MUL_SRC1_DS0_BASE + PDS_DS0_INDEX * PDS_NUM_WORDS_PER_DWORD,
													 EURASIA_PDS_MUL_SRC2SEL_DS1,
													 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD);
		}
		else
		{
			*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MUL_DESTSEL_DS1,
													 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_MUL_SRC1SEL_REG,
													 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1L : EURASIA_PDS_MUL_SRC1_IR0L,
													 EURASIA_PDS_MUL_SRC2SEL_DS1,
													 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD);
		}
		
		if ((!psStream->bInstanceData && psProgram->b32BitIndices) ||
			(psStream->bInstanceData && (psProgram->ui32NumInstances > 0x10000)))
		{
			/*
				mul ds0[PDS_DS0_ADDRESS_PART], ds0[PDS_DS0_INDEX].h, ds1[constant_stride].l
				shl ds0[PDS_DS0_ADDRESS_PART], ds0[PDS_DS0_ADDRESS_PART], 16
				add ds1[PDS_DS1_DMA_ADDRESS], ds0[PDS_DS0_ADDRESS_PART], ds1[PDS_DS1_DMA_ADDRESS]
			*/
			if (psStream->ui32Multiplier || psStream->ui32Shift)
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_DS0,
														 EURASIA_PDS_MUL_SRC1_DS0_BASE + PDS_DS0_INDEX * PDS_NUM_WORDS_PER_DWORD + 1,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD);
			}
			else
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1H : EURASIA_PDS_MUL_SRC1_IR0H,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD);
			}
			
			*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_SHIFT_DESTSEL_DS0,
													 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SRCSEL_DS0,
													 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SHIFT_ZERO + 16);
													 
			*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_ARITH_DESTSEL_DS1,
													 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_ARITH_SRC1SEL_DS0,
													 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_ARITH_SRC2SEL_DS1,
													 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS);
		}
		
		if (psStream->ui32Stride > 0xFFFF)
		{
			/*
				mul ds0[PDS_DS0_ADDRESS_PART], ds0[PDS_DS0_INDEX].l, ds1[constant_stride].h
				shl ds0[PDS_DS0_ADDRESS_PART], ds1[PDS_DS0_ADDRESS_PART], 16
				add ds1[PDS_DS1_DMA_ADDRESS], ds0[PDS_DS0_ADDRESS_PART], ds1[PDS_DS1_DMA_ADDRESS]
			*/
			if (psStream->ui32Multiplier || psStream->ui32Shift)
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_DS0,
														 EURASIA_PDS_MUL_SRC1_DS0_BASE + PDS_DS0_INDEX * PDS_NUM_WORDS_PER_DWORD,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD + 1);
			}
			else
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1L : EURASIA_PDS_MUL_SRC1_IR0L,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD + 1);
			}
			
			*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_SHIFT_DESTSEL_DS0,
													 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SRCSEL_DS0,
													 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SHIFT_ZERO + 16);
													 
			*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_ARITH_DESTSEL_DS1,
													 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_ARITH_SRC1SEL_DS0,
													 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_ARITH_SRC2SEL_DS1,
													 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS);
		}
		
		ui32DS0PersistentReg = PDSGetTemps(&ui32NextDS0PersistentReg, 1);
		
		for (ui32Element = 0; ui32Element < psStream->ui32NumElements; ui32Element++)
		{
		
			/*
				Calculate address to fetch data from
				add: temp = stream_address_in_persistent_temp + index_multiplied_by_stride
			*/
			*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_ARITH_DESTSEL_DS1,
													 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS1_TEMP_BLOCK_BASE,
													 EURASIA_PDS_ARITH_SRC1SEL_DS0,
													 EURASIA_PDS_ARITH_SRC1_DS0_BASE + ui32DS0PersistentReg,
													 EURASIA_PDS_ARITH_SRC2SEL_DS1,
													 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS);
													 
			/*
				Get the DMA control word
			*/
			
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
			
			/*
				Set DMA interface with address (temp) and doutd1
			*/
			*pui32Instruction++	= PDSEncodeMOVS 	(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MOVS_DEST_DOUTD,
													 EURASIA_PDS_MOVS_SRC1SEL_DS0,
													 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant  / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SRC2SEL_DS1,
													 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
													 ((PDS_DS1_TEMP_BLOCK_BASE) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
													 ((ui32DS0Constant) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
													 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
													 
													 
		}
	}
	
	/*
		Conditionally issue the task to the USE
		
		(if0) movs doutu, ds0[constant_use], ds0[constant_use], ds1[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
	
	*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
										EURASIA_PDS_MOV32_DESTSEL_DS1,
										EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
										EURASIA_PDS_MOV32_SRCSEL_DS0,
										EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant + 2);
										
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_IF0,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}


/*****************************************************************************
 Function Name	: PDSGenerateVertexShaderProgram
 Inputs			: psProgram		- pointer to the PDS vertex shader program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS vertex shader program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateVertexShaderProgram	(PPDS_VERTEX_SHADER_PROGRAM       psProgram,
															 IMG_UINT32                      *pui32Buffer,
															 PPDS_VERTEX_SHADER_PROGRAM_INFO  psPDSVertexShaderProgramInfo)
{
	IMG_UINT32 *			pui32Constants;
	IMG_UINT32				ui32NextDS0Constant;
	IMG_UINT32				ui32DS0Constant;
	IMG_UINT32				ui32NumConstants;
	IMG_UINT32				ui32Stream;
	PPDS_VERTEX_STREAM		psStream;
	IMG_UINT32				ui32Element;
	PPDS_VERTEX_ELEMENT		psElement;
	IMG_UINT32 *			pui32Instructions;
	IMG_UINT32 *			pui32Instruction;
	
	/*
		To load a typical set of vertex stream elements
		
		For each stream
		{
			Take the 24-bit index or the 24-bit instance
			Divide by the frequency if necessary
			Multiply by the stream stride
			For each element
			{
				Add the stream base address and the element offset
				DMA the element into the primray attribute buffer
			}
		}
		
		The straight-line code is organised to never re-visit constants, which are pre-computed into the program data
		
		The USE pre-processing program should unpack and pad the elements with 0s and 1s as required
	*/
	
	PDS_DPF("PDSGenerateVertexShaderProgram()");
	
	/*
		Generate the PDS vertex shader data
	*/
	pui32Constants		= pui32Buffer;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	for (ui32Stream = 0; ui32Stream < psProgram->ui32NumStreams; ui32Stream++)
	{
		psStream = &psProgram->asStreams[ui32Stream];
		
		/*
			Copy the vertex stream frequency multiplier to a constant if necessary
		*/
		if (psStream->ui32Multiplier)
		{
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psStream->ui32Multiplier | 0x01000000);
		}
		
		/*
			Copy the vertex stream stride to a constant
		*/
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psStream->ui32Stride);
		
		/*
			Copy the vertex stream element address, size and primary attribute register to constants
		*/
		for (ui32Element = 0; ui32Element < psStream->ui32NumElements; ui32Element++)
		{
			IMG_UINT32 ui32DOutD1;
			IMG_UINT32 ui32DMASize;
			
			psElement = &psStream->asElements[ui32Element];
			
			/* Size is in bytes - round up to nearest 32 bit word */
			ui32DMASize = (psElement->ui32Size + 3) >> 2;
			
			/*
				Set up the DMA transfer control word.
			*/
			PDS_ASSERT(ui32DMASize <= EURASIA_PDS_DOUTD1_BSIZE_MAX);
			
			ui32DOutD1 =	((ui32DMASize - 1) << EURASIA_PDS_DOUTD1_BSIZE_SHIFT) |
							(psElement->ui32Register << EURASIA_PDS_DOUTD1_AO_SHIFT);
							
			/*
				Write the dma transfer control words into the PDS data section.
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psStream->ui32Address + psElement->ui32Offset);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, ui32DOutD1);
			
			/*
			   Store the offsets if required
			*/
			if (psPDSVertexShaderProgramInfo)
			{
				psPDSVertexShaderProgramInfo->aui32AddressOffsets[ui32Stream][ui32Element] = PDSGetDS0ConstantOffset(ui32DS0Constant + 0);
				psPDSVertexShaderProgramInfo->aui32ElementOffsets[ui32Stream][ui32Element] = psElement->ui32Offset;
			}
		}
		
		/* Store the number of elements for this stream */
		if (psPDSVertexShaderProgramInfo)
		{
			psPDSVertexShaderProgramInfo->aui32NumElements[ui32Stream] = psStream->ui32NumElements;
		}
	}

	if (psProgram->bIterateVtxID)
	{
		/* Set up the douta word in the next DS0 constant */
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant,
						  (psProgram->ui32VtxIDRegister << EURASIA_PDS_DOUTA1_AO_SHIFT));
	}
	
	if (psProgram->bIterateInstanceID)
	{
		/* Set up the douta word in the next DS0 constant */
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant,
						  (psProgram->ui32InstanceIDRegister << EURASIA_PDS_DOUTA1_AO_SHIFT));
	}

	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 2, psProgram->aui32USETaskControl[2]);
	
	/*
	   Store the offsets if required
	*/
	if (psPDSVertexShaderProgramInfo)
	{
		psPDSVertexShaderProgramInfo->aui32USETaskControlOffsets[0] = PDSGetDS0ConstantOffset(ui32DS0Constant + 0);
		psPDSVertexShaderProgramInfo->aui32USETaskControlOffsets[1] = PDSGetDS0ConstantOffset(ui32DS0Constant + 1);
		psPDSVertexShaderProgramInfo->aui32USETaskControlOffsets[2] = PDSGetDS0ConstantOffset(ui32DS0Constant + 2);
		
		/*
		   Store the number of streams
		*/
		psPDSVertexShaderProgramInfo->ui32NumStreams          = psProgram->ui32NumStreams;
	}
	
	/*
		Get the number of constants
	*/
	ui32NumConstants	= PDSGetNumConstants(ui32NextDS0Constant);
	ui32NumConstants	= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS vertex shader code
	*/
	pui32Instructions	= pui32Constants + ui32NumConstants;
	pui32Instruction	= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	for (ui32Stream = 0; ui32Stream < psProgram->ui32NumStreams; ui32Stream++)
	{
		psStream = &psProgram->asStreams[ui32Stream];
		
		/*
			Divide by the frequency if necessary
			
			How does this work? I'll tell you:
			Iout = (Iin * (Multiplier+2^24)) >> (Shift+24)
			This allows streams to be fetched at a rate of less than one
		*/
		if (psStream->ui32Multiplier)
		{
			/*
				mul ds0[PDS_DS0_INDEX], ir0/1.l, ds1[constant_multiplier].l
				mul ds1[PDS_DS1_INDEX_PART], ir0/1.l, ds1[constant_multiplier].h
				shl ds1[PDS_DS1_INDEX_PART], ds1[PDS_DS0_INDEX_PART], 16
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
			
			*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
												EURASIA_PDS_MOV32_DESTSEL_DS1,
												EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
												EURASIA_PDS_MOV32_SRCSEL_DS0,
												EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant);
												
			*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MUL_DESTSEL_DS0,
													 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_INDEX,
													 EURASIA_PDS_MUL_SRC1SEL_REG,
													 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1L : EURASIA_PDS_MUL_SRC1_IR0L,
													 EURASIA_PDS_MUL_SRC2SEL_DS1,
													 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD);
													 
			*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MUL_DESTSEL_DS1,
													 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_INDEX_PART,
													 EURASIA_PDS_MUL_SRC1SEL_REG,
													 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1L : EURASIA_PDS_MUL_SRC1_IR0L,
													 EURASIA_PDS_MUL_SRC2SEL_DS1,
													 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD + 1);
													 
			*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_SHIFT_DESTSEL_DS1,
													 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS1_INDEX_PART,
													 EURASIA_PDS_SHIFT_SRCSEL_DS1,
													 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS1_INDEX_PART,
													 EURASIA_PDS_SHIFT_SHIFT_ZERO + 16);
													 
													 
			if ((!psStream->bInstanceData && !psProgram->b32BitIndices) ||
				(psStream->bInstanceData && (psProgram->ui32NumInstances <= 0x10000)))
			{
				/*
					add ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], ds1[PDS_DS1_INDEX_PART]
				*/
				*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_ARITH_DESTSEL_DS0,
														 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC1SEL_DS0,
														 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC2SEL_DS1,
														 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_INDEX_PART);
			}
			else
			{
				/*
					add ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], ds1[PDS_DS1_INDEX_PART]
					mul ds1[PDS_DS1_INDEX_PART], ir0/1.h, ds1[const_multiplier].l
					add ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], ds1[PDS_DS1_INDEX_PART]
					mul ds1[PDS_DS1_INDEX_PART], ir0/1.h, ds1[const_multiplier].h
					shl ds1[PDS_DS1_INDEX_PART], ds1[PDS_DS1_INDEX_PART], 16
					add ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], ds1[PDS_DS1_INDEX_PART]
				*/
				*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_ARITH_DESTSEL_DS0,
														 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC1SEL_DS0,
														 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC2SEL_DS1,
														 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_INDEX_PART);
														 
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS1,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_INDEX_PART,
														 EURASIA_PDS_MUL_SRC1SEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1H : EURASIA_PDS_MUL_SRC1_IR0H,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD);
														 
				*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_ARITH_DESTSEL_DS0,
														 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC1SEL_DS0,
														 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC2SEL_DS1,
														 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_INDEX_PART);
														 
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS1,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_INDEX_PART,
														 EURASIA_PDS_MUL_SRC1SEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1H : EURASIA_PDS_MUL_SRC1_IR0H,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD + 1);
														 
				*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_SHIFT_DESTSEL_DS1,
														 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS1_INDEX_PART,
														 EURASIA_PDS_SHIFT_SRCSEL_DS1,
														 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS1_INDEX_PART,
														 EURASIA_PDS_SHIFT_SHIFT_ZERO + 16);
														 
				*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_ARITH_DESTSEL_DS0,
														 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC1SEL_DS0,
														 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC2SEL_DS1,
														 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_INDEX_PART);
			}
			
			if (psStream->ui32Shift)
			{
				/*
					shr ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], shift + 24
				*/
				*pui32Instruction++	= PDSEncodeSHR		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_SHIFT_DESTSEL_DS0,
														 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_SHIFT_SRCSEL_DS0,
														 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_SHIFT_SHIFT_ZERO + psStream->ui32Shift + 24);
			}
		}
		else
		{
			if (psStream->ui32Shift)
			{
				/*
					shr ds0[PDS_DS0_INDEX], ir0/1, shift
				*/
				*pui32Instruction++	= PDSEncodeSHR		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_SHIFT_DESTSEL_DS0,
														 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_SHIFT_SRCSEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_SHIFT_SRC_IR1 : EURASIA_PDS_SHIFT_SRC_IR0,
														 EURASIA_PDS_SHIFT_SHIFT_ZERO + psStream->ui32Shift);
			}
		}
		
		/*
			Multiply by the vertex stream stride
			
			mul ds1[PDS_DS1_DMA_ADDRESS], ds0[PDS_DS0_INDEX].l, ds1[constant_stride].l
		*/
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
		
		*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
											EURASIA_PDS_MOV32_DESTSEL_DS1,
											EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
											EURASIA_PDS_MOV32_SRCSEL_DS0,
											EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant);
											
		if (psStream->ui32Multiplier || psStream->ui32Shift)
		{
			*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MUL_DESTSEL_DS1,
													 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_MUL_SRC1SEL_DS0,
													 EURASIA_PDS_MUL_SRC1_DS0_BASE + PDS_DS0_INDEX * PDS_NUM_WORDS_PER_DWORD,
													 EURASIA_PDS_MUL_SRC2SEL_DS1,
													 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD);
		}
		else
		{
			*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MUL_DESTSEL_DS1,
													 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_MUL_SRC1SEL_REG,
													 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1L : EURASIA_PDS_MUL_SRC1_IR0L,
													 EURASIA_PDS_MUL_SRC2SEL_DS1,
													 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD);
		}
		
		if ((!psStream->bInstanceData && psProgram->b32BitIndices) ||
			(psStream->bInstanceData && (psProgram->ui32NumInstances > 0x10000)))
		{
			/*
				mul ds0[PDS_DS0_ADDRESS_PART], ds0[PDS_DS0_INDEX].h, ds1[constant_stride].l
				shl ds0[PDS_DS0_ADDRESS_PART], ds0[PDS_DS0_ADDRESS_PART], 16
				add ds1[PDS_DS1_DMA_ADDRESS], ds0[PDS_DS0_ADDRESS_PART], ds1[PDS_DS1_DMA_ADDRESS]
			*/
			if (psStream->ui32Multiplier || psStream->ui32Shift)
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_DS0,
														 EURASIA_PDS_MUL_SRC1_DS0_BASE + PDS_DS0_INDEX * PDS_NUM_WORDS_PER_DWORD + 1,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD);
			}
			else
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1H : EURASIA_PDS_MUL_SRC1_IR0H,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD);
			}
			
			*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_SHIFT_DESTSEL_DS0,
													 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SRCSEL_DS0,
													 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SHIFT_ZERO + 16);
													 
			*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_ARITH_DESTSEL_DS1,
													 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_ARITH_SRC1SEL_DS0,
													 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_ARITH_SRC2SEL_DS1,
													 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS);
		}
		
		if (psStream->ui32Stride > 0xFFFF)
		{
			/*
				mul ds0[PDS_DS0_ADDRESS_PART], ds0[PDS_DS0_INDEX].l, ds1[constant_stride].h
				shl ds0[PDS_DS0_ADDRESS_PART], ds1[PDS_DS0_ADDRESS_PART], 16
				add ds1[PDS_DS1_DMA_ADDRESS], ds0[PDS_DS0_ADDRESS_PART], ds1[PDS_DS1_DMA_ADDRESS]
			*/
			if (psStream->ui32Multiplier || psStream->ui32Shift)
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_DS0,
														 EURASIA_PDS_MUL_SRC1_DS0_BASE + PDS_DS0_INDEX * PDS_NUM_WORDS_PER_DWORD,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD + 1);
			}
			else
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1L : EURASIA_PDS_MUL_SRC1_IR0L,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD + 1);
			}
			
			*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_SHIFT_DESTSEL_DS0,
													 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SRCSEL_DS0,
													 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SHIFT_ZERO + 16);
													 
			*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_ARITH_DESTSEL_DS1,
													 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_ARITH_SRC1SEL_DS0,
													 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_ARITH_SRC2SEL_DS1,
													 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS);
		}
		
		for (ui32Element = 0; ui32Element < psStream->ui32NumElements; ui32Element++)
		{
			/*
				Get the DMA source address, destination register and size
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
			
			/*
			  Add the vertex stream base address and vertex element offset
			  
			  And DMA the vertex element into the USE primary attribute buffer
			  
			  movsa doutd, ds0[constant_address], ds0[constant_size_and_register], ds1[PDS_DS1_DMA_ADDRESS], ds1[PDS_DS1_DMA_ADDRESS], emit
			*/
			*pui32Instruction++	= PDSEncodeMOVSA	(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MOVS_DEST_DOUTD,
													 EURASIA_PDS_MOVS_SRC1SEL_DS0,
													 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SRC2SEL_DS1,
													 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS / PDS_NUM_DWORDS_PER_REG,
													 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
													 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
		}
	}

	/* Set up the vertex ID iteration */
	if (psProgram->bIterateVtxID)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);

		*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOV32_DESTSEL_DS1,
											 EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
											 EURASIA_PDS_MOV32_SRCSEL_DS0,
											 EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant);

		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTA,
												 EURASIA_PDS_MOVS_SRC1SEL_REG,
												 EURASIA_PDS_MOVS_SRC1_IR0,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 (ui32DS0Constant % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 (ui32DS0Constant % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
	}
	
	/* and the instance ID */
	if (psProgram->bIterateInstanceID)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);

		*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOV32_DESTSEL_DS1,
											 EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0 + ui32DS0Constant,
											 EURASIA_PDS_MOV32_SRCSEL_DS0,
											 EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant);

		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTA,
												 EURASIA_PDS_MOVS_SRC1SEL_REG,
												 EURASIA_PDS_MOVS_SRC1_IR1,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 (ui32DS0Constant % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 (ui32DS0Constant % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
												 
	}
	
	/*
		Conditionally issue the task to the USE
		
		(if0) movs doutu, ds0[constant_use], ds0[constant_use], ds1[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
	
	*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
										EURASIA_PDS_MOV32_DESTSEL_DS1,
										EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
										EURASIA_PDS_MOV32_SRCSEL_DS0,
										EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant + 2);
										
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_IF0,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}



/*****************************************************************************
 Function Name	: PDSPatchVertexShaderProgram
 Inputs			: PDSVertexShaderProgramInfo - pointer to the PDS vertex shader program info
				: pui32Buffer		         - pointer to the buffer containing program
 Outputs		: none
 Returns		: Nothing
 Description	: Patches the addresses and use task controls of a pds vertex program
*****************************************************************************/
PDS_CALLCONV IMG_VOID PDSPatchVertexShaderProgram(PPDS_VERTEX_SHADER_PROGRAM_INFO  psPDSVertexShaderProgramInfo,
												  IMG_UINT32                      *pui32Buffer)
{
	IMG_UINT32  i, j;
	
	if(psPDSVertexShaderProgramInfo->bPatchTaskControl)
	{
		/* Write the use task control words */
		for (i = 0; i < PDS_NUM_USE_TASK_CONTROL_WORDS; i++)
		{
			IMG_UINT32 ui32Offset = psPDSVertexShaderProgramInfo->aui32USETaskControlOffsets[i];
			
			pui32Buffer[ui32Offset] = psPDSVertexShaderProgramInfo->aui32USETaskControl[i];
		}
	}
	
	/* Write the stream addresses */
	for (i = 0; i < psPDSVertexShaderProgramInfo->ui32NumStreams; i++)
	{
		/* Need a copy of the address for each element */
		for (j = 0; j < psPDSVertexShaderProgramInfo->aui32NumElements[i]; j++)
		{
			IMG_UINT32 ui32Offset     = psPDSVertexShaderProgramInfo->aui32AddressOffsets[i][j];
			IMG_UINT32 uElementOffset = psPDSVertexShaderProgramInfo->aui32ElementOffsets[i][j];
			
			pui32Buffer[ui32Offset] = psPDSVertexShaderProgramInfo->aui32StreamAddresses[i] + uElementOffset;
		}
	}
}



/*****************************************************************************
 Function Name	: PDSGenerateTerminateStateProgram
 Inputs			: psProgram		- pointer to the PDS terminate state program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS state program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 *PDSGenerateTerminateStateProgram (PDS_TERMINATE_STATE_PROGRAM *psProgram,
														   IMG_UINT32 *pui32Buffer)
{
	IMG_UINT32 *	pui32Constants;
	IMG_UINT32	ui32NextDS0Constant;
	IMG_UINT32	ui32DS0Constant;
	IMG_UINT32	ui32NumConstants;
	IMG_UINT32 *	pui32Instructions;
	IMG_UINT32 *	pui32Instruction;
	
	PDS_DPF("PDSGenerateTerminateStateProgram()");
	
	PDS_ASSERT((((IMG_UINT32)pui32Buffer) & ((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1)) == 0);
	
	pui32Constants		= (IMG_UINT32 *) (((IMG_UINTPTR_T) pui32Buffer + ((1UL << EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) - 1)) & ~((IMG_UINTPTR_T)(1UL << EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	/*
		Copy the terminate words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, EURASIA_TACTLPRES_TERMINATE);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, 0 << EURASIA_PDS_DOUTA1_AO_SHIFT);
	
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->ui32TerminateRegion);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, 1 << EURASIA_PDS_DOUTA1_AO_SHIFT);
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 2, psProgram->aui32USETaskControl[2]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS state code
	*/
	pui32Instructions		= pui32Constants + ui32NumConstants;
	pui32Instruction		= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	
	/*
		Copy the state into the primary attributes
	*/
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTA,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 EURASIA_PDS_MOVS_SWIZ_SRC1H,
											 EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTA,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 EURASIA_PDS_MOVS_SWIZ_SRC1H,
											 EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		Kick the USE to copy the state from the primary attributes into the output buffer
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
	
	*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
										EURASIA_PDS_MOV32_DESTSEL_DS1,
										EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
										EURASIA_PDS_MOV32_SRCSEL_DS0,
										EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant + 2);
										
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

/*****************************************************************************
 Function Name	: PDSPatchTerminateStateProgram
 Inputs			: psProgram		- pointer to the PDS terminate state program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: -
 Description	: Patches the terminate region in the PDS state program
*****************************************************************************/

PDS_CALLCONV IMG_VOID PDSPatchTerminateStateProgram (PDS_TERMINATE_STATE_PROGRAM *psProgram,
														IMG_UINT32 *pui32Buffer)
{
	IMG_UINT32 ui32DS0Constant, ui32NextDS0Constant;
	
	PDS_DPF("PDSPatchTerminateStateProgram()");
	
	PDS_ASSERT((((IMG_UINT32)pui32Buffer) & ((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1)) == 0);
	
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	/*
		Copy the terminate words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	
	PDSSetDS0Constant(pui32Buffer, ui32DS0Constant + 0, psProgram->ui32TerminateRegion);
}

#if !defined(SGX545)
#if !defined(PDS_BUILD_OPENGLES)
/*****************************************************************************
 Function Name	: PDSGenerateLoopbackVertexShaderProgram
 Inputs			: psProgram		- pointer to the PDS vertex shader program
				: pui32Buffer	- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS vertex shader program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateLoopbackVertexShaderProgram	(PPDS_VERTEX_SHADER_PROGRAM	psProgram,
																	 IMG_UINT32 *				pui32Buffer)
{
	IMG_UINT32 *			pui32Constants;
	IMG_UINT32				ui32NextDS0Constant;
	IMG_UINT32				ui32DS0Constant;
	IMG_UINT32				ui32NumConstants;
	IMG_UINT32				ui32Stream;
	PPDS_VERTEX_STREAM		psStream;
	IMG_UINT32				ui32Element;
	PPDS_VERTEX_ELEMENT		psElement;
	IMG_UINT32 *			pui32Instructions;
	IMG_UINT32 *			pui32Instruction;
	IMG_UINT32				ui32StreamOffset;
	
	PDS_DPF("PDSGenerateLoopbackVertexShaderProgram()");
	
	/*
		Generate the PDS vertex shader data
	*/
	pui32Constants		= pui32Buffer;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	ui32StreamOffset = 0;
	
	for (ui32Stream = 0; ui32Stream < psProgram->ui32NumStreams; ui32Stream++)
	{
		psStream = &psProgram->asStreams[ui32Stream];
		
		/*
			Copy the vertex element address, size and primary attribute register to constants
			
			This version is for fetching tesselator output.
		*/
		for (ui32Element = 0; ui32Element < psStream->ui32NumElements; ui32Element++)
		{
			IMG_UINT32	ui32DOutD1;
			
			psElement = &psStream->asElements[ui32Element];
			
			/*
				Set up the DMA transfer control word.
			*/
			ui32DOutD1	= (3 << EURASIA_PDS_DOUTD1_BSIZE_SHIFT) | // Always 4 dwords.
						  (psElement->ui32Register << EURASIA_PDS_DOUTD1_AO_SHIFT) |
						  EURASIA_PDS_DOUTD1_STYPE;
						  
			/*
				Write the dma transfer control words into the PDS data section.
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, ui32StreamOffset + ui32Element * 4); // our elements are all 4d vectors now.
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, ui32DOutD1);
		}
		
		ui32StreamOffset += psStream->ui32NumElements * 4;
	}
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 2, psProgram->aui32USETaskControl[2]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants	= PDSGetNumConstants(ui32NextDS0Constant);
	ui32NumConstants	= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS vertex shader code
	*/
	pui32Instructions	= pui32Constants + ui32NumConstants;
	pui32Instruction	= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_SHIFT_DESTSEL_DS1,
											 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
											 EURASIA_PDS_SHIFT_SRCSEL_REG,
											 EURASIA_PDS_SHIFT_SRC_IR0,
											 0);
											 
	for (ui32Stream = 0; ui32Stream < psProgram->ui32NumStreams; ui32Stream++)
	{
		psStream = &psProgram->asStreams[ui32Stream];
		
		for (ui32Element = 0; ui32Element < psStream->ui32NumElements; ui32Element++)
		{
			/*
				Get the DMA source address, destination register and size
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
			
			if ((ui32Stream == (psProgram->ui32NumStreams - 1)) && (ui32Element == (psStream->ui32NumElements - 1)))
			{
				/*
					mov32 ds1[PDS_DS1_DMA_REGISTER], ds0[constant_size_and_register]
				*/
				*pui32Instruction++	= PDSEncodeMOV32	(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MOV32_DESTSEL_DS1,
														 EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_DMA_REGISTER,
														 EURASIA_PDS_MOV32_SRCSEL_DS0,
														 EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant + 1);
														 
				/*
					Add the vertex stream base address and vertex element offset
					
					And DMA the vertex element into the USE primary attribute buffer
					
					movsa doutd, ds0[constant_address], ds1[PDS_DS1_DMA_REGISTER], ds1[PDS_DS1_DMA_ADDRESS], ds1[PDS_DS1_DMA_ADDRESS], emit
				*/
				*pui32Instruction++	= PDSEncodeMOVSA	(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MOVS_DEST_DOUTD,
														 EURASIA_PDS_MOVS_SRC1SEL_DS0,
														 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
														 EURASIA_PDS_MOVS_SRC2SEL_DS1,
														 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS / PDS_NUM_DWORDS_PER_REG,
														 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
														 (PDS_DS1_DMA_REGISTER % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
														 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
														 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
			}
			else
			{
				/*
					Add the vertex stream base address and vertex element offset
					
					And DMA the vertex element into the USE primary attribute buffer
					
					movsa doutd, ds0[constant_address], ds0[constant_size_and_register], ds1[PDS_DS1_DMA_ADDRESS], ds1[PDS_DS1_DMA_ADDRESS], emit
				*/
				*pui32Instruction++	= PDSEncodeMOVSA	(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MOVS_DEST_DOUTD,
														 EURASIA_PDS_MOVS_SRC1SEL_DS0,
														 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
														 EURASIA_PDS_MOVS_SRC2SEL_DS1,
														 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS / PDS_NUM_DWORDS_PER_REG,
														 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
														 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
														 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
														 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
			}
		}
	}
	
	/*
		Conditionally issue the task to the USE
		
		(if0) movs doutu, ds0[constant_use], ds0[constant_use], ds1[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
	
	*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
										EURASIA_PDS_MOV32_DESTSEL_DS1,
										EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
										EURASIA_PDS_MOV32_SRCSEL_DS0,
										EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant + 2);
										
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

/*****************************************************************************
 Function Name	: PDSGenerateTessVertexShaderProgram
 Inputs			: psProgram		- pointer to the PDS vertex shader program
				: pui32Buffer	- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS vertex shader program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateTessVertexShaderProgram	(PPDS_VERTEX_SHADER_PROGRAM	psProgram,
																 IMG_UINT32					ui32InputVertexStride,
																 IMG_UINT32 *				pui32Buffer,
																 IMG_UINT32					ui32Address)
{
	IMG_UINT32 *			pui32Constants;
	IMG_UINT32				ui32NextDS0Constant;
	IMG_UINT32				ui32DS0Constant;
	IMG_UINT32				ui32NumConstants;
	IMG_UINT32				ui32Stream;
	PPDS_VERTEX_STREAM		psStream;
	IMG_UINT32				ui32Element;
	PPDS_VERTEX_ELEMENT		psElement;
	IMG_UINT32 *			pui32Instructions;
	IMG_UINT32 *			pui32Instruction;
	IMG_UINT32				ui32StreamOffset;
	
	PDS_DPF("PDSGenerateTessVertexShaderProgram()");
	
	/*
		Generate the PDS vertex shader data
	*/
	pui32Constants		= pui32Buffer;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, ui32InputVertexStride);
	
	ui32StreamOffset = ui32Address;
	
	for (ui32Stream = 0; ui32Stream < psProgram->ui32NumStreams; ui32Stream++)
	{
		psStream = &psProgram->asStreams[ui32Stream];
		
		/*
			Copy the vertex element address, size and primary attribute register to constants
			
			This version is for fetching tesselator output.
		*/
		for (ui32Element = 0; ui32Element < psStream->ui32NumElements; ui32Element++)
		{
			IMG_UINT32	ui32DOutD1;
			
			psElement = &psStream->asElements[ui32Element];
			
			/*
				Set up the DMA transfer control word.
			*/
			ui32DOutD1	= (3 << EURASIA_PDS_DOUTD1_BSIZE_SHIFT) | // Always 4 dwords.
						  (psElement->ui32Register << EURASIA_PDS_DOUTD1_AO_SHIFT) |
						  (1UL << EURASIA_PDS_DOUTD1_INSTR_SHIFT); // Bypass the cache
						  
			/*
				Write the dma transfer control words into the PDS data section.
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, ui32StreamOffset + psElement->ui32Offset);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, ui32DOutD1);
		}
	}
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 2, psProgram->aui32USETaskControl[2]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants	= PDSGetNumConstants(ui32NextDS0Constant);
	ui32NumConstants	= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS vertex shader code
	*/
	pui32Instructions	= pui32Constants + ui32NumConstants;
	pui32Instruction	= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
	
	*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
										EURASIA_PDS_MOV32_DESTSEL_DS1,
										EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
										EURASIA_PDS_MOV32_SRCSEL_DS0,
										EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant);
										
	*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MUL_DESTSEL_DS1,
											 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
											 EURASIA_PDS_MUL_SRC1SEL_REG,
											 EURASIA_PDS_MUL_SRC1_IR0L,
											 EURASIA_PDS_MUL_SRC2SEL_DS1,
											 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD);
											 
	for (ui32Stream = 0; ui32Stream < psProgram->ui32NumStreams; ui32Stream++)
	{
		psStream = &psProgram->asStreams[ui32Stream];
		
		for (ui32Element = 0; ui32Element < psStream->ui32NumElements; ui32Element++)
		{
			/*
				Get the DMA source address, destination register and size
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
			
			/*
				Add the vertex stream base address and vertex element offset
				
				And DMA the vertex element into the USE primary attribute buffer
				
				movsa doutd, ds0[constant_address], ds1[PDS_DS1_DMA_REGISTER], ds1[PDS_DS1_DMA_ADDRESS], ds1[PDS_DS1_DMA_ADDRESS], emit
			*/
			
			*pui32Instruction++	= PDSEncodeMOVSA	(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MOVS_DEST_DOUTD,
													 EURASIA_PDS_MOVS_SRC1SEL_DS0,
													 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SRC2SEL_DS1,
													 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS / PDS_NUM_DWORDS_PER_REG,
													 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
													 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
		}
	}
	
	/*
		Conditionally issue the task to the USE
		
		(if0) movs doutu, ds0[constant_use], ds0[constant_use], ds1[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
	
	*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
										EURASIA_PDS_MOV32_DESTSEL_DS1,
										EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
										EURASIA_PDS_MOV32_SRCSEL_DS0,
										EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant + 2);
										
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_IF0,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

/*****************************************************************************
 Function Name	: PDSGenerateTessSAProgram
 Inputs			: psProgram		- pointer to the PDS tesselaotr secondary attributes program
				: pui32Buffer	- pointer to the buffer for the program
				: bShadowSAs	- Copy our control stream object into the shadow.
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS tesselator seconary attributes program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateTessSAProgram	(PPDS_TESSELATOR_SA_PROGRAM	psProgram,
													 IMG_UINT32 *							pui32Buffer,
													 IMG_BOOL	bShadowSAs)
{
	IMG_UINT32 *	pui32Constants;
	IMG_UINT32	ui32NextDS0Constant;
	IMG_UINT32	ui32DS0Constant;
	IMG_UINT32	ui32NumConstants;
	IMG_UINT32 *	pui32Instructions;
	IMG_UINT32 *	pui32Instruction;
	
	PDS_DPF("PDSGenerateTessSAProgram()");
	
	/*
		Generate the PDS tesselator secondary attributes data
	*/
	pui32Constants		= (IMG_UINT32 *) ((((IMG_UINTPTR_T)pui32Buffer) + ((1UL << EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) - 1)) & ~((IMG_UINTPTR_T)(1UL << EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	/*
		Copy the DMA control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32DMAControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32DMAControl[1]);
	
	if (psProgram->ui32NumDMAKicks == 2)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32DMAControl[2]);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32DMAControl[3]);
	}
	
	if(bShadowSAs)
	{
		/*
			Copy the USE task control words to constants
		*/
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[2]);
	}
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS tesselator secondary attributes code
	*/
	pui32Instructions		= pui32Constants + ui32NumConstants;
	pui32Instruction		= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	/*
		DMA the state into the secondary attributes
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTD,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 EURASIA_PDS_MOVS_SWIZ_SRC1H,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 EURASIA_PDS_MOVS_SWIZ_SRC1H);
											 
	if (psProgram->ui32NumDMAKicks == 2)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTD,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H);
	}
	
	if(bShadowSAs)
	{
		/*
			Issue the task to the USE...
			
			(if0) movs doutu, ds0[constant_use], ds0[constant_use], ds1[constant_use], ds1[constant_use], emit
		*/
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
		
		*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
										EURASIA_PDS_MOV32_DESTSEL_DS1,
										EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
										EURASIA_PDS_MOV32_SRCSEL_DS0,
										EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant + 2);
										
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTU,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
												 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
												 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
	}
	
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

/*****************************************************************************
 Function Name	: PDSGenerateTessIndicesProgram
 Inputs			: psProgram		- pointer to the PDS tesselator program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS program to load indices into the tesselator
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateTessIndicesProgram	(PPDS_TESSELATOR_PROGRAM	psProgram,
														 IMG_UINT32 *				pui32Buffer)
{
	IMG_UINT32 *					pui32Constants;
	IMG_UINT32						ui32NextDS0Constant;
	IMG_UINT32						ui32DS0Constant;
	IMG_UINT32						ui32NumConstants;
	IMG_UINT32 *					pui32Instructions;
	IMG_UINT32 *					pui32Instruction;
	
	/*
		This function is only interested in loading indices from the VDM into the USE PA buffer...
	*/
	
	PDS_DPF("PDSGenerateTessIndicesProgram()");
	
	pui32Constants = (IMG_UINT32 *) (((IMG_UINT32) pui32Buffer + ((1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	#if EURASIA_USE_COMPLX_PADATAOFFSET
	/*
		Set up the register offset...
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, EURASIA_USE_COMPLX_PADATAOFFSET);
	#endif
	
	#if defined(SGX540) || defined(SGX535) || defined(SGX545) || defined(SGX541)
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControlDummy[2]);
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControlDummy[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControlDummy[1]);
	
	#else
	ui32DS0Constant;
	#endif
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS tesselator code
	*/
	pui32Instructions	= pui32Constants + ui32NumConstants;
	pui32Instruction		= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	
	/*
		Get the external dependency flag
	*/
	
	#if EURASIA_USE_COMPLX_PADATAOFFSET
	/*
		add ds0[PDS_DS0_INDEX], ir1, (EURASIA_USE_COMPLX_PADATAOFFSET)
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
	
	*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
											EURASIA_PDS_MOV32_DESTSEL_DS1,
											EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
											EURASIA_PDS_MOV32_SRCSEL_DS0,
											EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant);
											
	*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_ARITH_DESTSEL_DS0,
											 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS0_INDEX,
											 EURASIA_PDS_ARITH_SRC1SEL_REG,
											 EURASIA_PDS_ARITH_SRC1_IR1,
											 EURASIA_PDS_ARITH_SRC2SEL_DS1,
											 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_TEMP0);
											 
	/*
		shl ds1[PDS_DS1_INDEX_PART], ds0[PDS_DS0_INDEX], (EURASIA_PDS_DOUTA1_AO_SHIFT)
	*/
	*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_SHIFT_DESTSEL_DS1,
											 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS1_INDEX_PART,
											 EURASIA_PDS_SHIFT_SRCSEL_DS0,
											 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS0_INDEX,
											 EURASIA_PDS_SHIFT_SHIFT_ZERO + (EURASIA_PDS_DOUTA1_AO_SHIFT));
	#else
	/*
		shl ds1[PDS_DS1_INDEX_PART], ir1, (EURASIA_PDS_DOUTA1_AO_SHIFT)
	*/
	*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_SHIFT_DESTSEL_DS1,
											 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS1_INDEX_PART,
											 EURASIA_PDS_SHIFT_SRCSEL_REG,
											 EURASIA_PDS_SHIFT_SRC_IR1,
											 EURASIA_PDS_SHIFT_SHIFT_ZERO + (EURASIA_PDS_DOUTA1_AO_SHIFT));
	#endif
	
	/*
		Copy the indices into the USE primary attribute buffer
		
		movs douta, ir0, ds1[PDS_DS1_BASE], emit
	*/
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTA,
											 EURASIA_PDS_MOVS_SRC1SEL_REG,
											 EURASIA_PDS_MOVS_SRC1_IR0,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_INDEX_PART / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 EURASIA_PDS_MOVS_SWIZ_SRC2H,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 EURASIA_PDS_MOVS_SWIZ_SRC2H);
											 
	#if defined(SGX540) || defined(SGX535) || defined(SGX545) || defined(SGX541)
	/*
		Issue the dummy task to the USE
		
		movs doutu, ds0[constant_use], ds1[constant_use], ds0[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
	
	*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
											EURASIA_PDS_MOV32_DESTSEL_DS1,
											EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
											EURASIA_PDS_MOV32_SRCSEL_DS0,
											EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant);

	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);

	#if defined(SGX545)
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_IF1,
	#else
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
	#endif
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
	#endif
	
	/*
		End the program
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32IndicesDataSegment	= pui32Constants;
	psProgram->ui32DataSize				= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

/*****************************************************************************
 Function Name	: PDSGenerateTessProgram
 Inputs			: psProgram		- pointer to the PDS tesselator program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS tesselator program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateTessProgram	(PPDS_TESSELATOR_PROGRAM	psProgram,
													 IMG_UINT32 *							pui32Buffer)
{
	IMG_UINT32 *					pui32Constants;
	IMG_UINT32					ui32NextDS0Constant;
	IMG_UINT32					ui32DS0Constant;
	IMG_UINT32					ui32NumConstants;
	IMG_UINT32 *					pui32Instructions;
	IMG_UINT32 *					pui32Instruction;
	
	PDS_DPF("PDSGenerateTessProgram()");
	
	/*
		Generate the PDS tesselator data
	*/
	pui32Constants		= (IMG_UINT32 *) ((((IMG_UINTPTR_T)pui32Buffer) + ((1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1)) & ~((IMG_UINTPTR_T)(1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	/*
		Copy the USE task control words to constants (for first task)
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControlTessFirst[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControlTessFirst[1]);
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControlTessFirst[2]);
	
	/*
		Copy the flag that will indicate the first task has been sent.
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, 0xFFFFFFFF);
	
	/*
		Copy the USE task control words to constants (for all other tasks)
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControlTessOther[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControlTessOther[1]);
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControlTessOther[2]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS tesselator code
	*/
	pui32Instructions		= pui32Constants + ui32NumConstants;
	pui32Instruction		= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	/*
		Look for the first task to get here...
	*/
	*pui32Instruction++	= PDSEncodeTSTN		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_TST_DEST_P0,
											 EURASIA_PDS_TST_SRCSEL_SRC1,
											 EURASIA_PDS_TST_SRC1SEL_DS0,
											 PDS_DS0_TESS_SYNC,
											 0);
	/*
		Branch if not first task
	*/
	*pui32Instruction	= PDSEncodeBRA		(EURASIA_PDS_CC_P0,
											 ((pui32Instruction - pui32Instructions) + 5) << EURASIA_PDS_FLOW_DEST_ALIGNSHIFT);
	pui32Instruction++;
	
	/*
		Issue the task to the USE (first task only)
		
		movs doutu, ds0[constant_use], ds1[constant_use], ds0[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
	
	*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
											EURASIA_PDS_MOV32_DESTSEL_DS1,
											EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
											EURASIA_PDS_MOV32_SRCSEL_DS0,
											EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant + 2);
											
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		Set the flag.
		
		mov32 PDS_DS0_TESS_SYNC, #FFFFFFFF
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
	*pui32Instruction++	= PDSEncodeMOV32	(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MOV32_DESTSEL_DS0,
														 EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS0_TESS_SYNC,
														 EURASIA_PDS_MOV32_SRCSEL_DS0,
														 EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant);
														 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Issue the task to the USE (All other tasks)
		
		movs doutu, ds0[constant_use], ds1[constant_use], ds0[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
	
	*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
											EURASIA_PDS_MOV32_DESTSEL_DS1,
											EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
											EURASIA_PDS_MOV32_SRCSEL_DS0,
											EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant + 2);
											
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

/*****************************************************************************
 Function Name	: PDSGenerateTessVertexProgram
 Inputs			: psProgram		- pointer to the PDS teselator program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS tess vertex load program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateTessVertexProgram	(PPDS_TESSELATOR_PROGRAM	psProgram,
														 IMG_UINT32 *					pui32Buffer)
{
	IMG_UINT32 *			pui32Constants;
	IMG_UINT32				ui32NextDS0Constant;
	IMG_UINT32				ui32DS0Constant;
	IMG_UINT32				ui32NumConstants;
	IMG_UINT32				ui32Stream;
	PPDS_VERTEX_STREAM		psStream;
	IMG_UINT32				ui32Element;
	PPDS_VERTEX_ELEMENT		psElement;
	IMG_UINT32 *			pui32Instructions;
	IMG_UINT32 *			pui32Instruction;
	
	/*
		To load a typical set of vertex stream elements
		
		For each stream
		{
			Take the 24-bit index or the 24-bit instance
			Divide by the frequency if necessary
			Multiply by the stream stride
			For each element
			{
				Add the stream base address and the element offset
				DMA the element into the primray attribute buffer
			}
		}
		
		The straight-line code is organised to never re-visit constants, which are pre-computed into the program data
		
		The USE pre-processing program should unpack and pad the elements with 0s and 1s as required
	*/
	
	PDS_DPF("PDSGenerateTessVertexProgram()");
	
	/*
		Generate the PDS tesselator data
	*/
	pui32Constants		= (IMG_UINT32 *) ((((IMG_UINTPTR_T)pui32Buffer) + ((1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1)) & ~((IMG_UINTPTR_T)(1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	for (ui32Stream = 0; ui32Stream < psProgram->ui32NumStreams; ui32Stream++)
	{
		psStream = &psProgram->asStreams[ui32Stream];
		
		/*
			Copy the vertex stream frequency multiplier to a constant if necessary
		*/
		if (psStream->ui32Multiplier)
		{
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psStream->ui32Multiplier | 0x01000000);
		}
		
		/*
			Copy the vertex stream stride to a constant
		*/
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psStream->ui32Stride);
		
		/*
			Copy the vertex stream element address, size and primary attribute register to constants
		*/
		for (ui32Element = 0; ui32Element < psStream->ui32NumElements; ui32Element++)
		{
			IMG_UINT32	ui32DOutD1;
			IMG_UINT32 ui32DMASize;
			
			psElement = &psStream->asElements[ui32Element];
			
			/* Size is in bytes - round up to nearest 32 bit word */
			ui32DMASize = (psElement->ui32Size + 3) >> 2;
			
			/*
				Set up the DMA transfer control word.
			*/
			PDS_ASSERT(ui32DMASize <= EURASIA_PDS_DOUTD1_BSIZE_MAX);
			
			ui32DOutD1	= ((ui32DMASize - 1)	<< EURASIA_PDS_DOUTD1_BSIZE_SHIFT) |
						  ((psElement->ui32Register + EURASIA_USE_COMPLX_PADATAOFFSET) << EURASIA_PDS_DOUTD1_AO_SHIFT);
						  
			/*
				Write the dma transfer control words into the PDS data section.
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psStream->ui32Address + psElement->ui32Offset);
			PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, ui32DOutD1);
		}
	}
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControlVertLoad[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControlVertLoad[1]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 2, psProgram->aui32USETaskControlVertLoad[2]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants	= PDSGetNumConstants(ui32NextDS0Constant);
	ui32NumConstants	= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS tesselator code
	*/
	pui32Instructions	= pui32Constants + ui32NumConstants;
	pui32Instruction	= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	for (ui32Stream = 0; ui32Stream < psProgram->ui32NumStreams; ui32Stream++)
	{
		psStream = &psProgram->asStreams[ui32Stream];
		
		/*
			Divide by the frequency if necessary
		*/
		if (psStream->ui32Multiplier)
		{
			/*
				mul ds0[PDS_DS0_INDEX], ir0/1.l, ds1[constant_multiplier].l
				mul ds1[PDS_DS1_INDEX_PART], ir0/1.l, ds1[constant_multiplier].h
				shl ds1[PDS_DS1_INDEX_PART], ds1[PDS_DS1_INDEX_PART], 16
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
			
			*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
											EURASIA_PDS_MOV32_DESTSEL_DS1,
											EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
											EURASIA_PDS_MOV32_SRCSEL_DS0,
											EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant);
											
			*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MUL_DESTSEL_DS0,
													 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_INDEX,
													 EURASIA_PDS_MUL_SRC1SEL_REG,
													 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1L : EURASIA_PDS_MUL_SRC1_IR0L,
													 EURASIA_PDS_MUL_SRC2SEL_DS1,
													 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD);
													 
			*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MUL_DESTSEL_DS1,
													 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_INDEX_PART,
													 EURASIA_PDS_MUL_SRC1SEL_REG,
													 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1L : EURASIA_PDS_MUL_SRC1_IR0L,
													 EURASIA_PDS_MUL_SRC2SEL_DS1,
													 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD + 1);
													 
			*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_SHIFT_DESTSEL_DS1,
													 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS1_INDEX_PART,
													 EURASIA_PDS_SHIFT_SRCSEL_DS1,
													 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS1_INDEX_PART,
													 EURASIA_PDS_SHIFT_SHIFT_ZERO + 16);
													 
			if ((!psStream->bInstanceData && !psProgram->b32BitIndices) ||
				(psStream->bInstanceData && (psProgram->ui32NumInstances <= 0x10000)))
			{
				/*
					add ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], ds1[PDS_DS1_INDEX_PART]
				*/
				*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_ARITH_DESTSEL_DS0,
														 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC1SEL_DS0,
														 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC2SEL_DS1,
														 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_INDEX_PART);
			}
			else
			{
				/*
					add ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], ds1[PDS_DS1_INDEX_PART]
					mul ds1[PDS_DS1_INDEX_PART], ir0/1.h, ds1[const_multiplier].l
					add ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], ds1[PDS_DS1_INDEX_PART]
					mul ds1[PDS_DS1_INDEX_PART], ir0/1.h, ds1[const_multiplier].h
					shl ds1[PDS_DS1_INDEX_PART], ds1[PDS_DS1_INDEX_PART], 16
					add ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], ds1[PDS_DS1_INDEX_PART]
				*/
				*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_ARITH_DESTSEL_DS0,
														 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC1SEL_DS0,
														 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC2SEL_DS1,
														 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_INDEX_PART);
														 
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS1,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_INDEX_PART,
														 EURASIA_PDS_MUL_SRC1SEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1H : EURASIA_PDS_MUL_SRC1_IR0H,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD);
														 
				*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_ARITH_DESTSEL_DS0,
														 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC1SEL_DS0,
														 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC2SEL_DS1,
														 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_INDEX_PART);
														 
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS1,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_INDEX_PART,
														 EURASIA_PDS_MUL_SRC1SEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1H : EURASIA_PDS_MUL_SRC1_IR0H,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD + 1);
														 
				*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_SHIFT_DESTSEL_DS1,
														 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS1_INDEX_PART,
														 EURASIA_PDS_SHIFT_SRCSEL_DS1,
														 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS1_INDEX_PART,
														 EURASIA_PDS_SHIFT_SHIFT_ZERO + 16);
														 
				*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_ARITH_DESTSEL_DS0,
														 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC1SEL_DS0,
														 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_ARITH_SRC2SEL_DS1,
														 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_INDEX_PART);
			}
			
			if (psStream->ui32Shift)
			{
				/*
					shr ds0[PDS_DS0_INDEX], ds0[PDS_DS0_INDEX], shift
				*/
				*pui32Instruction++	= PDSEncodeSHR		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_SHIFT_DESTSEL_DS0,
														 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_SHIFT_SRCSEL_DS0,
														 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_SHIFT_SHIFT_ZERO + psStream->ui32Shift + 24);
			}
		}
		else
		{
			if (psStream->ui32Shift)
			{
				/*
					shr ds0[PDS_DS0_INDEX], ir0/1, shift
				*/
				*pui32Instruction++	= PDSEncodeSHR		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_SHIFT_DESTSEL_DS0,
														 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_INDEX,
														 EURASIA_PDS_SHIFT_SRCSEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_SHIFT_SRC_IR1 : EURASIA_PDS_SHIFT_SRC_IR0,
														 EURASIA_PDS_SHIFT_SHIFT_ZERO + psStream->ui32Shift);
			}
		}
		
		/*
			Multiply by the vertex stream stride
			
			mul ds1[PDS_DS1_DMA_ADDRESS], ds0[PDS_DS0_INDEX].l, ds1[constant_stride].l
		*/
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
		
		*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
											EURASIA_PDS_MOV32_DESTSEL_DS1,
											EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
											EURASIA_PDS_MOV32_SRCSEL_DS0,
											EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant);
											
		if (psStream->ui32Multiplier || psStream->ui32Shift)
		{
			*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MUL_DESTSEL_DS1,
													 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_MUL_SRC1SEL_DS0,
													 EURASIA_PDS_MUL_SRC1_DS0_BASE + PDS_DS0_INDEX * PDS_NUM_WORDS_PER_DWORD,
													 EURASIA_PDS_MUL_SRC2SEL_DS1,
													 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD);
		}
		else
		{
			*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MUL_DESTSEL_DS1,
													 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_MUL_SRC1SEL_REG,
													 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1L : EURASIA_PDS_MUL_SRC1_IR0L,
													 EURASIA_PDS_MUL_SRC2SEL_DS1,
													 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD);
		}
		
		if ((!psStream->bInstanceData && psProgram->b32BitIndices) ||
			(psStream->bInstanceData && (psProgram->ui32NumInstances > 0x10000)))
		{
			/*
				mul ds0[PDS_DS0_ADDRESS_PART], ds0[PDS_DS0_INDEX].h, ds1[constant_stride].l
				shl ds0[PDS_DS0_ADDRESS_PART], ds0[PDS_DS0_ADDRESS_PART], 16
				add ds1[PDS_DS1_DMA_ADDRESS], ds0[PDS_DS0_ADDRESS_PART], ds1[PDS_DS1_DMA_ADDRESS]
			*/
			if (psStream->ui32Multiplier || psStream->ui32Shift)
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_DS0,
														 EURASIA_PDS_MUL_SRC1_DS0_BASE + PDS_DS0_INDEX * PDS_NUM_WORDS_PER_DWORD + 1,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD);
			}
			else
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1H : EURASIA_PDS_MUL_SRC1_IR0H,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD);
			}
			
			*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_SHIFT_DESTSEL_DS0,
													 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SRCSEL_DS0,
													 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SHIFT_ZERO + 16);
													 
			*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_ARITH_DESTSEL_DS1,
													 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_ARITH_SRC1SEL_DS0,
													 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_ARITH_SRC2SEL_DS1,
													 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS);
		}
		
		if (psStream->ui32Stride > 0xFFFF)
		{
			/*
				mul ds0[PDS_DS0_ADDRESS_PART], ds0[PDS_DS0_INDEX].l, ds1[constant_stride].h
				shl ds0[PDS_DS0_ADDRESS_PART], ds1[PDS_DS0_ADDRESS_PART], 16
				add ds1[PDS_DS1_DMA_ADDRESS], ds0[PDS_DS0_ADDRESS_PART], ds1[PDS_DS1_DMA_ADDRESS]
			*/
			if (psStream->ui32Multiplier || psStream->ui32Shift)
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_DS0,
														 EURASIA_PDS_MUL_SRC1_DS0_BASE + PDS_DS0_INDEX * PDS_NUM_WORDS_PER_DWORD,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD + 1);
			}
			else
			{
				*pui32Instruction++	= PDSEncodeMUL		(EURASIA_PDS_CC_ALWAYS,
														 EURASIA_PDS_MUL_DESTSEL_DS0,
														 EURASIA_PDS_MUL_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
														 EURASIA_PDS_MUL_SRC1SEL_REG,
														 psStream->bInstanceData ? EURASIA_PDS_MUL_SRC1_IR1L : EURASIA_PDS_MUL_SRC1_IR0L,
														 EURASIA_PDS_MUL_SRC2SEL_DS1,
														 EURASIA_PDS_MUL_SRC2_DS1_BASE + PDS_DS1_TEMP0 * PDS_NUM_WORDS_PER_DWORD + 1);
			}
			
			*pui32Instruction++	= PDSEncodeSHL		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_SHIFT_DESTSEL_DS0,
													 EURASIA_PDS_SHIFT_DEST_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SRCSEL_DS0,
													 EURASIA_PDS_SHIFT_SRC_DS_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_SHIFT_SHIFT_ZERO + 16);
													 
			*pui32Instruction++	= PDSEncodeADD		(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_ARITH_DESTSEL_DS1,
													 EURASIA_PDS_ARITH_DEST_DS_BASE + PDS_DS1_DMA_ADDRESS,
													 EURASIA_PDS_ARITH_SRC1SEL_DS0,
													 EURASIA_PDS_ARITH_SRC1_DS0_BASE + PDS_DS0_ADDRESS_PART,
													 EURASIA_PDS_ARITH_SRC2SEL_DS1,
													 EURASIA_PDS_ARITH_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS);
		}
		
		for (ui32Element = 0; ui32Element < psStream->ui32NumElements; ui32Element++)
		{
			/*
				Get the DMA source address, destination register and size
			*/
			ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
			
			/*
				Add the vertex stream base address and vertex element offset
				
				And DMA the vertex element into the USE primary attribute buffer
				
				movsa doutd, ds0[constant_address], ds0[constant_size_and_register], ds1[PDS_DS1_DMA_ADDRESS], ds1[PDS_DS1_DMA_ADDRESS], emit
			*/
			*pui32Instruction++	= PDSEncodeMOVSA	(EURASIA_PDS_CC_ALWAYS,
													 EURASIA_PDS_MOVS_DEST_DOUTD,
													 EURASIA_PDS_MOVS_SRC1SEL_DS0,
													 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
													 EURASIA_PDS_MOVS_SRC2SEL_DS1,
													 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_DMA_ADDRESS / PDS_NUM_DWORDS_PER_REG,
													 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
													 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
													 (PDS_DS1_DMA_ADDRESS % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
		}
	}
	
	/*
		Conditionaly issue the unpack task to the USE
		
		(if0) movs doutu, ds0[constant_use], ds0[constant_use], ds1[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
	
	*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
											EURASIA_PDS_MOV32_DESTSEL_DS1,
											EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
											EURASIA_PDS_MOV32_SRCSEL_DS0,
											EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant + 2);
											
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_IF0,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32VertexDataSegment	= pui32Constants;
	psProgram->ui32DataSize				= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

/*****************************************************************************
 Function Name	: PDSGenerateInterPipeSDProgram
 Inputs			: 	psProgram		- pointer to the PDS tesselator program
					pui32Buffer		- pointer to the buffer for the program
					bClearSyncFlag	- Clears the PDS sync constant
									   for tesselation syncronisation
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS program to run the simulated USE inter-pipe sequential dependency.
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateInterPipeSDProgram	(PPDS_IPSD_PROGRAM	psProgram,
														 IMG_UINT32 *		pui32Buffer,
														 IMG_BOOL			bClearSyncFlag)
{
	IMG_UINT32 *					pui32Constants;
	IMG_UINT32					ui32NextDS0Constant;
	IMG_UINT32					ui32DS0Constant;
	IMG_UINT32					ui32NumConstants;
	IMG_UINT32 *					pui32Instructions;
	IMG_UINT32 *					pui32Instruction;
	
	PDS_DPF("PDSGenerateInterPipeSDProgram()");
	
	/*
		Generate the PDS tesselator data
	*/
	pui32Constants		= (IMG_UINT32 *) ((((IMG_UINTPTR_T)pui32Buffer) + ((1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1)) & ~((IMG_UINTPTR_T)(1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	if(bClearSyncFlag)
	{
		/*
			Copy the flag that will indicate the first task has NOT been sent.
		*/
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, 0);
	}
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[2]);
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS tesselator code
	*/
	pui32Instructions	= pui32Constants + ui32NumConstants;
	pui32Instruction		= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	if(bClearSyncFlag)
	{
		/*
			Clear the flag.
			
			mov32 PDS_DS0_TESS_SYNC, #0
		*/
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
		*pui32Instruction++	= PDSEncodeMOV32	(EURASIA_PDS_CC_ALWAYS,
															 EURASIA_PDS_MOV32_DESTSEL_DS0,
															 EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS0_TESS_SYNC,
															 EURASIA_PDS_MOV32_SRCSEL_DS0,
															 EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant);
	}
	
	/*
		Issue the dummy task to the USE
		
		movs doutu, ds0[constant_use], ds1[constant_use], ds0[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 1);
	
	*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
											EURASIA_PDS_MOV32_DESTSEL_DS1,
											EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
											EURASIA_PDS_MOV32_SRCSEL_DS0,
											EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant);

	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

/*****************************************************************************
 Function Name	: PDSGenerateGenericKickUSEProgram
 Inputs			: 	psProgram		- pointer to the PDS program
					pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS program to run a general USE program
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateGenericKickUSEProgram(PPDS_GENERICKICKUSE_PROGRAM	psProgram,
														   IMG_UINT32 *		pui32Buffer)
{
	IMG_UINT32 *					pui32Constants;
	IMG_UINT32					ui32NextDS0Constant;
	IMG_UINT32					ui32DS0Constant;
	IMG_UINT32					ui32NumConstants;
	IMG_UINT32 *					pui32Instructions;
	IMG_UINT32 *					pui32Instruction;
	
	PDS_DPF("PDSGenerateGenericKickUSEProgram()");
	
	/*
		Generate the PDS tesselator data
	*/
	pui32Constants		= (IMG_UINT32 *) ((((IMG_UINTPTR_T)pui32Buffer) + ((1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1)) & ~((IMG_UINTPTR_T)(1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 2, psProgram->aui32USETaskControl[2]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS tesselator code
	*/
	pui32Instructions	= pui32Constants + ui32NumConstants;
	pui32Instruction		= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	/*
		Issue the task to the USE
		
		movs doutu, ds0[constant_use], ds1[constant_use], ds0[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
	
	*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
											EURASIA_PDS_MOV32_DESTSEL_DS1,
											EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
											EURASIA_PDS_MOV32_SRCSEL_DS0,
											EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant + 2);
											
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

#if !defined(PDS_BUILD_OPENGL) & !defined(PDS_BUILD_OPENGLES)

/*****************************************************************************
 Function Name	: PDSGenerateComplexAdvanceFlushProgram
 Inputs			: 	psProgram		- pointer to the PDS tesselator program
					pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS program to run the advance 7 on the use.
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateComplexAdvanceFlushProgram	(PPDS_COMPLEXADVANCEFLUSH_PROGRAM	psProgram,
																 IMG_UINT32 *		pui32Buffer)
{
	IMG_UINT32 *					pui32Constants;
	IMG_UINT32					ui32NextDS0Constant;
	IMG_UINT32					ui32DS0Constant;
	IMG_UINT32					ui32NumConstants;
	IMG_UINT32 *					pui32Instructions;
	IMG_UINT32 *					pui32Instruction;
	
	PDS_DPF("PDSGenerateComplexAdvanceFlushProgram()");
	
	/*
		Generate the PDS tesselator data
	*/
	pui32Constants		= (IMG_UINT32 *) ((((IMG_UINTPTR_T)pui32Buffer) + ((1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1)) & ~((IMG_UINTPTR_T)(1UL << EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 2, psProgram->aui32USETaskControl[2]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS tesselator code
	*/
	pui32Instructions	= pui32Constants + ui32NumConstants;
	pui32Instruction		= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	/*
		Issue the task to the USE
		
		movs doutu, ds0[constant_use], ds1[constant_use], ds0[constant_use], ds1[constant_use], emit
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
	
	*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
											EURASIA_PDS_MOV32_DESTSEL_DS1,
											EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
											EURASIA_PDS_MOV32_SRCSEL_DS0,
											EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant + 2);
											
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}

/*****************************************************************************
 Function Name	: PDSGenerateVSMemConstUploadProgram
 Inputs			: psProgram		- pointer to the PDS constant upload program
				: pui32Buffer	- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS program to load the constant upload list
				  into USSE primary attribute registers, ready for use by the
				  USSE constant upload program.
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGenerateVSMemConstUploadProgram	(PPDS_VS_MEMCONST_UPLOAD_PROGRAM	psProgram,
																 IMG_UINT32 *						pui32Buffer)
{
	IMG_UINT32 *	pui32Constants;
	IMG_UINT32		ui32NextDS0Constant;
	IMG_UINT32		ui32DS0Constant;
	IMG_UINT32		ui32NumConstants;
	IMG_UINT32 *	pui32Instructions;
	IMG_UINT32 *	pui32Instruction;
	
	PDS_DPF("PDSGenerateVSMemConstUploadProgram()");
	
	/*
		Generate the PDS  secondary attributes data
	*/
	pui32Constants		= (IMG_UINT32 *) ((((IMG_UINTPTR_T)pui32Buffer) + ((1UL << EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) - 1)) & ~((IMG_UINTPTR_T)(1UL << EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) - 1));
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	/*
		Copy the DMA control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32DMAControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32DMAControl[1]);
	
	if (psProgram->ui32NumDMAKicks == 2)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32DMAControl[2]);
		PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32DMAControl[3]);
	}
	
	/*
		Copy the USE task control words to constants
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 0, psProgram->aui32USETaskControl[0]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 1, psProgram->aui32USETaskControl[1]);
	PDSSetDS0Constant(pui32Constants, ui32DS0Constant + 2, psProgram->aui32USETaskControl[2]);
	
	/*
		Get the number of constants
	*/
	ui32NumConstants		= PDSGetNumConstants(ui32NextDS0Constant);
	ui32NumConstants		= ((ui32NumConstants * sizeof(IMG_UINT32) + ((1UL << EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) - 1)) / sizeof(IMG_UINT32);
	
	/*
		Generate the PDS code to load the constant upload list into primary attribute
		registers
	*/
	pui32Instructions	= pui32Constants + ui32NumConstants;
	pui32Instruction	= pui32Instructions;
	ui32NextDS0Constant	= PDS_DS0_CONST_BLOCK_BASE;
	
	/*
		DMA the constant upload list into the primary attributes
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
	
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTD,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 EURASIA_PDS_MOVS_SWIZ_SRC1H,
											 EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 EURASIA_PDS_MOVS_SWIZ_SRC1H);
											 
	if (psProgram->ui32NumDMAKicks == 2)
	{
		ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 2);
		
		*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
												 EURASIA_PDS_MOVS_DEST_DOUTD,
												 EURASIA_PDS_MOVS_SRC1SEL_DS0,
												 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SRC2SEL_DS1,
												 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP_BLOCK_BASE / PDS_NUM_DWORDS_PER_REG,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H,
												 EURASIA_PDS_MOVS_SWIZ_SRC1L,
												 EURASIA_PDS_MOVS_SWIZ_SRC1H);
	}
	
	/*
		Kick the USE to upload the new constant data
	*/
	ui32DS0Constant		= PDSGetConstants(&ui32NextDS0Constant, 3);
	
	*pui32Instruction++	= PDSEncodeMOV32(EURASIA_PDS_CC_ALWAYS,
											EURASIA_PDS_MOV32_DESTSEL_DS1,
											EURASIA_PDS_MOV32_DEST_DS_BASE + PDS_DS1_TEMP0,
											EURASIA_PDS_MOV32_SRCSEL_DS0,
											EURASIA_PDS_MOV32_SRC_DS_BASE + ui32DS0Constant + 2);
											
	*pui32Instruction++	= PDSEncodeMOVS		(EURASIA_PDS_CC_ALWAYS,
											 EURASIA_PDS_MOVS_DEST_DOUTU,
											 EURASIA_PDS_MOVS_SRC1SEL_DS0,
											 EURASIA_PDS_MOVS_SRC1_DS0_BASE + ui32DS0Constant / PDS_NUM_DWORDS_PER_REG,
											 EURASIA_PDS_MOVS_SRC2SEL_DS1,
											 EURASIA_PDS_MOVS_SRC2_DS1_BASE + PDS_DS1_TEMP0 / PDS_NUM_DWORDS_PER_REG,
											 ((ui32DS0Constant + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((ui32DS0Constant + 1) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC1H : EURASIA_PDS_MOVS_SWIZ_SRC1L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L,
											 ((PDS_DS1_TEMP0 + 0) % PDS_NUM_DWORDS_PER_REG) ? EURASIA_PDS_MOVS_SWIZ_SRC2H : EURASIA_PDS_MOVS_SWIZ_SRC2L);
											 
	/*
		End the program
		
		halt
	*/
	*pui32Instruction++	= PDSEncodeHALT		(EURASIA_PDS_CC_ALWAYS);
	
	/*
		Save the data segment pointer and size
	*/
	psProgram->pui32DataSegment	= pui32Constants;
	psProgram->ui32DataSize		= ui32NumConstants * sizeof(IMG_UINT32);
	
	return pui32Instruction;
}
#endif /* #if defined(PDS_BUILD_OPENGL) && defined(PDS_BUILD_OPENGLES) */

#endif /* #if !defined(PDS_BUILD_OPENGLES) || defined(GLES1_EXTENSION_CARNAV) */

#endif /* #if !defined(SGX545) */


/*****************************************************************************
 Function Name	: PDSGetTemps
 Inputs			: pui32NextTemp	- pointer to the next temp
				: ui32NumTemps	- the number of temps
 Outputs		: pui32NextTemp	- pointer to the next temp
 Returns		: ui32temp		- the next temp
 Description	: Gets the next temps
*****************************************************************************/
static IMG_UINT32 PDSGetTemps	(IMG_UINT32 *	pui32NextTemp,
								 IMG_UINT32	ui32NumTemps)
{
	IMG_UINT32	ui32Temp;
	
	switch (ui32NumTemps)
	{
		default:
		case 1:
		{
			ui32Temp	= *pui32NextTemp;
			break;
		}
		case 2:
		{
			ui32Temp	= (*pui32NextTemp + 1U) & ~1U;
			break;
		}
	}
	*pui32NextTemp	= ui32Temp + ui32NumTemps;
	PDS_ASSERT((ui32Temp + ui32NumTemps) >= EURASIA_PDS_DATASTORE_TEMPSTART);
	
	return ui32Temp;
}

/*****************************************************************************
 Function Name	: PDSGetConstants
 Inputs			: pui32NextConstant	- pointer to the next constant
				: ui32NumConstants	- the number of constants
 Outputs		: pui32NextConstant	- pointer to the next constant
 Returns		: ui32Constant		- the next constant
 Description	: Gets the next constants
*****************************************************************************/
static IMG_UINT32 PDSGetConstants	(IMG_UINT32 *	pui32NextConstant,
								 IMG_UINT32	ui32NumConstants)
{
	IMG_UINT32	ui32Constant;
	
	switch (ui32NumConstants)
	{
		default:
		case 1:
		{
			ui32Constant	= *pui32NextConstant;
			break;
		}
		case 2:
		{
			ui32Constant	= (*pui32NextConstant + 1U) & ~1U;
			break;
		}
	}
	*pui32NextConstant	= ui32Constant + ui32NumConstants;
	PDS_ASSERT((ui32Constant + ui32NumConstants) <= EURASIA_PDS_DATASTORE_TEMPSTART);
	
	return ui32Constant;
}

/*****************************************************************************
 Function Name	: PDSGetDS0ConstantOffset
 Inputs			: ui32Constant - The constant offset to get
 Outputs		:
 Returns		: The constant offset into the buffer (in DWORDS)
 Description	: Gets the constant offset into the buffer
*****************************************************************************/
static IMG_UINT32 PDSGetDS0ConstantOffset(IMG_UINT32	ui32Constant)
{
	IMG_UINT32	ui32Row;
	IMG_UINT32	ui32Column;
	
	ui32Row		= ui32Constant / PDS_NUM_DWORDS_PER_ROW;
	ui32Column	= ui32Constant % PDS_NUM_DWORDS_PER_ROW;
	
	return (2 * ui32Row) * PDS_NUM_DWORDS_PER_ROW + ui32Column;
	
}


/*****************************************************************************
 Function Name	: PDSSetDS0Constant
 Inputs			: ui32NumConstants	- the constant
				: ui32Value			- the value
 Outputs		: pui32Constants		- pointer to the constants
 Returns		: none
 Description	: Sets the constant in the first bank
*****************************************************************************/
static IMG_VOID PDSSetDS0Constant	(IMG_UINT32 *	pui32Constants,
								 IMG_UINT32	ui32Constant,
								 IMG_UINT32	ui32Value)
{
	IMG_UINT32 ui32Offset =  PDSGetDS0ConstantOffset(ui32Constant);
	
	pui32Constants[ui32Offset]	= ui32Value;
}


/*****************************************************************************
 Function Name	: PDSGetNumConstants
 Inputs			: pui32NextDS0Constant	- the next constant in the first bank
				: pui32NextDS1Constant	- the next constant in the second bank
 Outputs		: none
 Returns		: ui32NumConstants	- the number of constants
 Description	: Gets the number of constants
*****************************************************************************/
static IMG_UINT32 PDSGetNumConstants	(IMG_UINT32	ui32NextDS0Constant)
{
	IMG_UINT32	ui32NumConstants;
	IMG_UINT32	ui32Constant;
	IMG_UINT32	ui32Row;
	IMG_UINT32	ui32Column;
	IMG_UINT32	ui32NextConstant;
	
	ui32NumConstants = 0;
	if (ui32NextDS0Constant > 0)
	{
		ui32Constant		= ui32NextDS0Constant - 1;
		ui32Row			= ui32Constant / PDS_NUM_DWORDS_PER_ROW;
		ui32Column		= ui32Constant % PDS_NUM_DWORDS_PER_ROW;
		ui32NextConstant	= (2 * ui32Row) * PDS_NUM_DWORDS_PER_ROW + ui32Column + 1;
		ui32NumConstants = ui32NextConstant;
	}
	
	return ui32NumConstants;
}


#endif /* !FIX_HW_BRN_25339 && !(FIX_HW_BRN_27330) */

/*****************************************************************************
 Function Name	: PDSGeneratePixelEventProgramTileXY
 Inputs			: psProgram		- pointer to the PDS pixel event program
				: pui32Buffer		- pointer to the buffer for the program
 Outputs		: none
 Returns		: pointer to just beyond the buffer for the program
 Description	: Generates the PDS pixel event program that writes tilexy to PAs
				  on EOT.
*****************************************************************************/
PDS_CALLCONV IMG_UINT32 * PDSGeneratePixelEventProgramTileXY(PPDS_PIXEL_EVENT_PROGRAM	psProgram,
														 IMG_UINT32 *				pui32Buffer)
{
	IMG_UINT32 *pui32Constants;
	

	PDS_ASSERT(PDS_PIXEVENT_PROG_SIZE >= ((IMG_UINT32)(psProgram-psProgram) + sizeof(g_pui32PDSPixelEvent)));

	memcpy(pui32Buffer, g_pui32PDSPixelEvent_TileXY, PDS_PIXELEVENT_TILEXY_SIZE);
	psProgram->ui32DataSize	= PDS_PIXELEVENT_TILEXY_DATA_SEGMENT_SIZE;

	/*
	   Copy the task control words for the various USE programs to the PDS program's
	   data segment.
	 */
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
	PDSPixelEvent_TileXYSetEOT0(pui32Buffer, psProgram->aui32EOTUSETaskControl[0]);
	PDSPixelEvent_TileXYSetEOT1(pui32Buffer, psProgram->aui32EOTUSETaskControl[1]);
#else
	PDSSetTaskControl(PixelEvent_TileXY, EOT);
#endif
	PDSSetTaskControl(PixelEvent_TileXY, PTOFF);
	PDSSetTaskControl(PixelEvent_TileXY, EOR);
	PDSPixelEvent_TileXYSetEOT_DOUTA1(pui32Buffer, 0);

	/*
		Generate the PDS pixel event data
	*/
	pui32Constants = (IMG_UINT32 *) (((IMG_UINTPTR_T)pui32Buffer + ((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1)) & ~((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1));
	psProgram->pui32DataSegment	= pui32Constants;
	
	return pui32Buffer + (PDS_PIXELEVENT_TILEXY_SIZE >> 2);
}

PDS_CALLCONV IMG_UINT32 PDSGetPixeventProgSize(IMG_VOID)
{
	return PDS_PIXEVENT_PROG_SIZE;
}

PDS_CALLCONV IMG_UINT32 PDSGetPixeventTileXYProgSize(IMG_VOID)
{
	return PDS_PIXELEVENT_TILEXY_SIZE;
}



/*****************************************************************************
 End of file (pds.c)
*****************************************************************************/
